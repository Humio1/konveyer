#include "Suppressor.h"

namespace kv
{
namespace
{
constexpr double kOlaScale = 2.0 / 3.0;  // Hann analysis x Hann synthesis at 4x overlap sums to 1.5
constexpr double kFloorDb = -140.0;
constexpr double kGateDb = -96.0;        // bins quieter than this (re full-scale sine) are never cut
constexpr double kDetectSmoothSec = 0.025;
constexpr double kEnvQ = 0.3;  // generalised mean: 0 would be the dB average, 1 the power average
} // namespace

int Suppressor::fftSizeFor (double sampleRate, int quality)
{
    static constexpr int base[] = { 1024, 2048, 4096 };
    const int q = clampv (quality, 0, 2);
    const int mult = sampleRate > 150000.0 ? 4 : sampleRate > 70000.0 ? 2 : 1;
    return std::min (kMaxFft, base[q] * mult);
}

void Suppressor::prepare (double sampleRate, int quality)
{
    fs = sampleRate;
    fftSize = fftSizeFor (sampleRate, quality);
    hop = fftSize / 4;
    bins = fftSize / 2 + 1;
    fft.setSize (fftSize);

    window.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5 - 0.5 * std::cos (kTwoPi * i / fftSize);  // periodic Hann
    // A sine of amplitude A shows up as A * N / 4 in its bin.
    levelOffset = -20.0 * std::log10 (fftSize / 4.0);

    for (auto* v : { &inL, &inR, &outL, &outR })
        v->assign ((size_t) fftSize, 0.0);
    z.assign ((size_t) fftSize, {});
    specA.assign ((size_t) bins, {});
    specB.assign ((size_t) bins, {});
    for (auto* v : { &powA, &powB, &levA, &levB, &env, &tmp, &cutA, &cutB, &gainA, &gainB, &weight, &invK })
        v->assign ((size_t) bins, 0.0);
    prefixV.assign ((size_t) bins + 1, 0.0);
    prefixW.assign ((size_t) bins + 1, 0.0);
    for (int k = 1; k < bins; ++k)
        invK[(size_t) k] = 1.0 / k;

    dispLo.resize (kDisplayPoints);
    dispHi.resize (kDisplayPoints);
    dispFrac.assign (kDisplayPoints, -1.0);
    const double binHz = fs / fftSize;
    const double stepRatio = std::pow (kDisplayMaxHz / kDisplayMinHz, 1.0 / (kDisplayPoints - 1));
    for (int i = 0; i < kDisplayPoints; ++i)
    {
        const double f = displayHz (i);
        const double fLo = f / std::sqrt (stepRatio), fHi = f * std::sqrt (stepRatio);
        int lo = (int) std::ceil (fLo / binHz), hi = (int) std::floor (fHi / binHz);
        dispFrac[(size_t) i] = -1.0;
        if (hi < lo)
        {
            // narrower than a bin: interpolate between the two neighbours instead of drawing steps
            const double pos = f / binHz;
            lo = (int) std::floor (pos);
            hi = lo + 1;
            dispFrac[(size_t) i] = pos - lo;
        }
        dispLo[(size_t) i] = clampv (lo, 1, bins - 1);
        dispHi[(size_t) i] = clampv (hi, 1, bins - 1);
    }

    wLow = wHigh = -1.0;
    wEmph = -999.0;
    reset();
}

void Suppressor::reset()
{
    for (auto* v : { &inL, &inR, &outL, &outR, &powA, &powB, &gainA, &gainB })
        std::fill (v->begin(), v->end(), 0.0);
    pos = 0;
    dispSmooth.fill (-120.0f);
    for (int i = 0; i < kDisplayPoints; ++i)
    {
        displayLevel[(size_t) i].store (-120.0f, std::memory_order_relaxed);
        displayCut[(size_t) i].store (0.0f, std::memory_order_relaxed);
    }
    outGain.reset (dbToGain (params.outputDb));
    lastMode = params.mode;
}

//==============================================================================
void Suppressor::process (float* L, float* R, int n) noexcept
{
    outGain.setTarget (dbToGain (params.outputDb), 1024);
    const int fill = fftSize - hop;
    for (int i = 0; i < n; ++i)
    {
        const double g = outGain.next();
        const double yl = outL[(size_t) pos], yr = outR[(size_t) pos];
        inL[(size_t) (fill + pos)] = L[i];
        inR[(size_t) (fill + pos)] = R[i];
        L[i] = (float) (yl * g);
        R[i] = (float) (yr * g);
        if (++pos == hop)
        {
            pos = 0;
            processFrame();
        }
    }
}

void Suppressor::updateWeights() noexcept
{
    auto same = [] (double a, double b) { return std::abs (a - b) < 1.0e-9; };
    if (same (params.lowHz, wLow) && same (params.highHz, wHigh) && same (params.emphasis, wEmph))
        return;
    wLow = params.lowHz;
    wHigh = params.highHz;
    wEmph = params.emphasis;
    const double lo = std::min (wLow, wHigh), hi = std::max (wLow, wHigh);
    const double e = clampv (wEmph, -100.0, 100.0) / 100.0;
    for (int k = 0; k < bins; ++k)
    {
        const double f = std::max (1.0, k * fs / fftSize);
        // half-octave raised-cosine fades outside the range
        double w = 1.0;
        if (f < lo)
            w = std::log2 (lo / f) >= 0.5 ? 0.0 : 0.5 + 0.5 * std::cos (kPi * std::log2 (lo / f) / 0.5);
        else if (f > hi)
            w = std::log2 (f / hi) >= 0.5 ? 0.0 : 0.5 + 0.5 * std::cos (kPi * std::log2 (f / hi) / 0.5);
        // emphasis: tilt around 1 kHz, up to about x1.7 / x0.6 at the ends
        w *= clampv (std::pow (2.0, e * std::log2 (f / 1000.0) / 5.0), 0.0, 2.0);
        weight[(size_t) k] = k == 0 ? 0.0 : w;
    }
}

void Suppressor::envelope (const double* lev, double* out) noexcept
{
    // Log-frequency moving average: a generalised mean of the power (see kEnvQ), each bin weighted by
    // 1/k so every octave counts the same. Done twice: the second pass clips the peaks found by the
    // first, so a resonance does not lift its own reference. Leaning towards the power average keeps
    // the dense, healthy harmonics of a pad close to the envelope instead of letting the valleys
    // between them drag it down.
    const double s = clampv (params.sharpness, 0.0, 100.0) / 100.0;
    const double widthOct = 2.0 * std::pow (0.125, s);  // 2 octaves .. 1/4 octave
    const double h = std::pow (2.0, widthOct * 0.5);
    for (int k = 0; k < bins; ++k)
        tmp[(size_t) k] = std::pow (10.0, kEnvQ * 0.1 * lev[k]);
    for (int pass = 0; pass < 2; ++pass)
    {
        prefixV[0] = prefixW[0] = 0.0;
        prefixV[1] = prefixW[1] = 0.0;  // bin 0 (DC) is left out
        for (int k = 1; k < bins; ++k)
        {
            prefixV[(size_t) k + 1] = prefixV[(size_t) k] + tmp[(size_t) k] * invK[(size_t) k];
            prefixW[(size_t) k + 1] = prefixW[(size_t) k] + invK[(size_t) k];
        }
        for (int k = 1; k < bins; ++k)
        {
            int lo = (int) std::floor (k / h), hi = (int) std::ceil (k * h);
            if (hi - lo < 10)  // at least a few bins, or the lowest bins would only see themselves
            {
                lo = k - 5;
                hi = k + 5;
            }
            lo = std::max (1, lo);
            hi = std::min (bins - 1, hi);
            const double mean = (prefixV[(size_t) hi + 1] - prefixV[(size_t) lo]) / (prefixW[(size_t) hi + 1] - prefixW[(size_t) lo]);
            out[k] = 10.0 / kEnvQ * std::log10 (mean + 1.0e-30);
        }
        out[0] = out[1];
        if (pass == 0)
            for (int k = 0; k < bins; ++k)
                tmp[(size_t) k] = std::pow (10.0, kEnvQ * 0.1 * std::min (lev[k], out[k] + 3.0));
    }
}

void Suppressor::computeCut (const double* lev, double* cut) noexcept
{
    envelope (lev, env.data());
    const double depth = clampv (params.depth, 0.0, 100.0) / 100.0;
    const double ratio = depth;                       // fraction of the excess that is removed
    const double cutLimit = 3.0 + 21.0 * depth;         // dB
    const double thr = 0.5 + 9.5 * clampv (params.selectivity, 0.0, 100.0) / 100.0;
    const double knee = params.soft ? 6.0 : 0.0;
    for (int k = 0; k < bins; ++k)
    {
        const double w = weight[(size_t) k];
        double c = 0.0;
        if (w > 0.0 && lev[k] > kGateDb && depth > 0.0)
        {
            const double x = lev[k] - env[(size_t) k] - thr;
            double over = 0.0;
            if (knee > 0.0)
                over = x <= -0.5 * knee ? 0.0 : x >= 0.5 * knee ? x : sq (x + 0.5 * knee) / (2.0 * knee);
            else
                over = std::max (0.0, x);
            c = std::min (cutLimit, ratio * w * over);
        }
        cut[k] = c;
    }
    // light smoothing across frequency: no lonely notches, fewer artefacts
    for (int pass = 0; pass < 2; ++pass)
    {
        double prev = cut[0];
        for (int k = 1; k < bins - 1; ++k)
        {
            const double cur = cut[k];
            cut[k] = std::max (cur * 0.6, 0.25 * prev + 0.5 * cur + 0.25 * cut[k + 1]);
            prev = cur;
        }
    }
}

void Suppressor::smoothGain (const double* target, double* gain) noexcept
{
    const double hopSec = hop / fs;
    const double a = onePoleCoeff (std::max (0.1, params.attackMs) * 0.001, 1.0 / hopSec);
    const double r = onePoleCoeff (std::max (1.0, params.releaseMs) * 0.001, 1.0 / hopSec);
    for (int k = 0; k < bins; ++k)
    {
        const double c = target[k] > gain[k] ? a : r;
        gain[k] = target[k] + c * (gain[k] - target[k]);
    }
}

void Suppressor::processFrame() noexcept
{
    const int N = fftSize;
    for (int i = 0; i < N; ++i)
        z[(size_t) i] = { inL[(size_t) i] * window[(size_t) i], inR[(size_t) i] * window[(size_t) i] };
    fft.transform (z.data(), false);

    // Split the packed transform into the two channels (left = real input, right = imaginary input).
    const bool ms = params.mode == StereoMode::MidSide;
    for (int k = 0; k < bins; ++k)
    {
        const auto zk = z[(size_t) k];
        const auto zn = std::conj (z[(size_t) ((N - k) % N)]);
        auto xl = 0.5 * (zk + zn);
        auto xr = std::complex<double> (0.0, -0.5) * (zk - zn);
        if (ms)
        {
            const auto m = 0.5 * (xl + xr), s = 0.5 * (xl - xr);
            xl = m;
            xr = s;
        }
        specA[(size_t) k] = xl;
        specB[(size_t) k] = xr;
    }

    if (params.mode != lastMode)
    {
        // a different signal is being analysed now: start the detectors fresh
        std::fill (powA.begin(), powA.end(), 0.0);
        std::fill (powB.begin(), powB.end(), 0.0);
        std::fill (gainA.begin(), gainA.end(), 0.0);
        std::fill (gainB.begin(), gainB.end(), 0.0);
        lastMode = params.mode;
    }

    updateWeights();
    const double ds = onePoleCoeff (kDetectSmoothSec, fs / hop);
    const bool linked = params.mode == StereoMode::Linked;
    for (int k = 0; k < bins; ++k)
    {
        const double pa = std::norm (specA[(size_t) k]), pb = std::norm (specB[(size_t) k]);
        if (linked)
        {
            powA[(size_t) k] = 0.5 * (pa + pb) + ds * (powA[(size_t) k] - 0.5 * (pa + pb));
            levA[(size_t) k] = std::max (kFloorDb, 10.0 * std::log10 (powA[(size_t) k] + 1.0e-30) + levelOffset);
        }
        else
        {
            powA[(size_t) k] = pa + ds * (powA[(size_t) k] - pa);
            powB[(size_t) k] = pb + ds * (powB[(size_t) k] - pb);
            levA[(size_t) k] = std::max (kFloorDb, 10.0 * std::log10 (powA[(size_t) k] + 1.0e-30) + levelOffset);
            levB[(size_t) k] = std::max (kFloorDb, 10.0 * std::log10 (powB[(size_t) k] + 1.0e-30) + levelOffset);
        }
    }

    computeCut (levA.data(), cutA.data());
    smoothGain (cutA.data(), gainA.data());
    if (linked)
        std::copy (gainA.begin(), gainA.end(), gainB.begin());
    else
    {
        computeCut (levB.data(), cutB.data());
        smoothGain (cutB.data(), gainB.data());
    }

    // Apply. Mix and delta are done here so dry and wet stay perfectly aligned.
    const double mix = clampv (params.mix, 0.0, 100.0) / 100.0;
    const bool delta = params.delta;
    float deepest = 0.0f;
    for (int k = 0; k < bins; ++k)
    {
        double ga = 1.0 - mix * (1.0 - dbToGain (-gainA[(size_t) k]));
        double gb = 1.0 - mix * (1.0 - dbToGain (-gainB[(size_t) k]));
        if (delta)
        {
            ga = 1.0 - ga;
            gb = 1.0 - gb;
        }
        auto ya = specA[(size_t) k] * ga, yb = specB[(size_t) k] * gb;
        if (ms)
        {
            const auto l = ya + yb, r = ya - yb;
            ya = l;
            yb = r;
        }
        specA[(size_t) k] = ya;
        specB[(size_t) k] = yb;
        deepest = std::max (deepest, (float) std::max (gainA[(size_t) k], gainB[(size_t) k]));
    }

    // Rebuild the packed spectrum Y = L + iR with Hermitian halves, then one inverse transform.
    const std::complex<double> I (0.0, 1.0);
    for (int k = 0; k < bins; ++k)
        z[(size_t) k] = specA[(size_t) k] + I * specB[(size_t) k];
    for (int k = bins; k < N; ++k)
        z[(size_t) k] = std::conj (specA[(size_t) (N - k)]) + I * std::conj (specB[(size_t) (N - k)]);
    fft.transform (z.data(), true);

    // Overlap-add. outL/outR hold the not yet played part; shift by one hop first.
    std::copy (outL.begin() + hop, outL.end(), outL.begin());
    std::copy (outR.begin() + hop, outR.end(), outR.begin());
    std::fill (outL.end() - hop, outL.end(), 0.0);
    std::fill (outR.end() - hop, outR.end(), 0.0);
    const double scale = kOlaScale / N;
    for (int i = 0; i < N; ++i)
    {
        const double w = window[(size_t) i] * scale;
        outL[(size_t) i] += z[(size_t) i].real() * w;
        outR[(size_t) i] += z[(size_t) i].imag() * w;
    }
    std::copy (inL.begin() + hop, inL.end(), inL.begin());
    std::copy (inR.begin() + hop, inR.end(), inR.begin());

    // Display: linked input level and the deeper of the two cuts, per log-spaced point.
    for (int p = 0; p < kDisplayPoints; ++p)
    {
        double lev = kFloorDb, cut = 0.0;
        auto binLevel = [&] (int k) {
            const double pk = linked ? powA[(size_t) k] : 0.5 * (powA[(size_t) k] + powB[(size_t) k]);
            return 10.0 * std::log10 (pk + 1.0e-30) + levelOffset;
        };
        auto binCut = [&] (int k) { return std::max (gainA[(size_t) k], gainB[(size_t) k]); };
        const int lo = dispLo[(size_t) p], hi = dispHi[(size_t) p];
        if (const double t = dispFrac[(size_t) p]; t >= 0.0)
        {
            lev = binLevel (lo) + t * (binLevel (hi) - binLevel (lo));
            cut = binCut (lo) + t * (binCut (hi) - binCut (lo));
        }
        else
            for (int k = lo; k <= hi; ++k)
            {
                lev = std::max (lev, binLevel (k));
                cut = std::max (cut, binCut (k));
            }
        auto& s = dispSmooth[(size_t) p];
        s = (float) lev > s ? (float) lev : s + 0.25f * ((float) lev - s);
        displayLevel[(size_t) p].store (s, std::memory_order_relaxed);
        displayCut[(size_t) p].store ((float) (cut * mix), std::memory_order_relaxed);
    }
    maxCut.update (deepest * (float) mix);
    frames.fetch_add (1, std::memory_order_relaxed);
}
} // namespace kv
