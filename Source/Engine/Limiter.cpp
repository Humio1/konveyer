#include "Limiter.h"

namespace kv
{
const std::array<std::array<double, 16>, 3>& TruePeakDetector::kernel()
{
    static const auto k = []
    {
        std::array<std::array<double, 16>, 3> h {};
        for (size_t f = 0; f < 3; ++f)
        {
            const double frac = (double) (f + 1) * 0.25;
            double sum = 0.0;
            for (size_t j = 0; j < 16; ++j)
            {
                const double t = 7.0 + frac - (double) j;
                const double sinc = std::abs (t) < 1.0e-12 ? 1.0 : std::sin (kPi * t) / (kPi * t);
                const double w = 0.42 + 0.5 * std::cos (kPi * t / 8.0) + 0.08 * std::cos (2.0 * kPi * t / 8.0);
                h[f][j] = sinc * w;
                sum += h[f][j];
            }
            for (auto& v : h[f])
                v /= sum;
        }
        return h;
    }();
    return k;
}

//==============================================================================
namespace
{
int nextPow2 (int v)
{
    int p = 1;
    while (p < v)
        p <<= 1;
    return p;
}
} // namespace

void Limiter::prepare (double sampleRate)
{
    fs = sampleRate;
    kMax = std::max (1, (int) std::ceil (0.005 * fs));
    latencySamples = TruePeakDetector::kDelay + kMax - 1;

    const int size = nextPow2 (latencySamples + 2);
    mask = size - 1;
    dlyL.assign ((size_t) size, 0.0);
    dlyR.assign ((size_t) size, 0.0);
    dryL.assign ((size_t) size, 0.0);
    dryR.assign ((size_t) size, 0.0);

    const int gsize = nextPow2 (kMax + 2);
    gainMask = gsize - 1;
    gainDly.assign ((size_t) gsize, 0.0);

    minWin.init (kMax + 2);
    box1.init (kMax + 1);
    box2.init (kMax + 1);
    histHop = std::max (1, (int) std::lround (0.01 * fs));
    reset();
}

void Limiter::reset()
{
    std::fill (dlyL.begin(), dlyL.end(), 0.0);
    std::fill (dlyR.begin(), dlyR.end(), 0.0);
    std::fill (dryL.begin(), dryL.end(), 0.0);
    std::fill (dryR.begin(), dryR.end(), 0.0);
    std::fill (gainDly.begin(), gainDly.end(), 0.0);
    n64 = 0;
    tpL.reset();
    tpR.reset();
    env = 0.0;
    avgGr = 0.0;
    k = -1;
    configureWindow (clampv ((int) std::lround (params.lookaheadMs * 0.001 * fs), 1, kMax));
    drive.reset (dbToGain (clampv (params.driveDb, -12.0, 30.0)));
    onMix.reset (1.0);
    histCount = 0;
    histInPk = histOutPk = histGrMax = 0.0;
}

void Limiter::configureWindow (int newK)
{
    k = newK;
    minWin.clear();
    const int l1 = (k + 1) / 2;
    box1.setLength (l1, env);
    box2.setLength (k - l1 + 1, env);
}

void Limiter::process (double* L, double* R, int n, bool on)
{
    const int newK = clampv ((int) std::lround (params.lookaheadMs * 0.001 * fs), 1, kMax);
    if (newK != k)
        configureWindow (newK);

    const double ceilDb = clampv (params.ceilingDb, -30.0, 0.0);
    const double ceilLin = dbToGain (ceilDb);
    const bool tp = params.truePeak;
    drive.setTarget (dbToGain (clampv (params.driveDb, -12.0, 30.0)), (int) (0.02 * fs));
    onMix.setTarget (on ? 1.0 : 0.0, (int) (0.02 * fs));

    const double relS = std::max (0.001, params.releaseMs * 0.001);
    const double fixedRel = onePoleCoeff (relS, fs);
    const double aAvg = onePoleCoeff (0.4, fs);
    const int gainOffset = kMax - k;
    double maxGr = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const double d = drive.next();
        const double xl = L[i], xr = R[i];
        const double dl = xl * d, dr = xr * d;

        // Gain the sample needs (dB, <= 0), from the true or sample peak.
        const double pk = std::max (tpL.push (dl, tp), tpR.push (dr, tp));
        const double req = pk > ceilLin ? ceilDb - gainToDb (pk) : 0.0;

        // Release: exponential recovery in dB. Auto release is fast for isolated peaks and
        // slows down the deeper and longer the limiter works.
        double rc = fixedRel;
        if (params.autoRelease)
        {
            avgGr = aAvg * avgGr + (1.0 - aAvg) * env;
            const double depth = clampv (-avgGr / 8.0, 0.0, 1.0);
            rc = onePoleCoeff (relS * (0.08 + 0.92 * depth), fs);
        }
        env = std::min (req, rc * env);

        // Hold the minimum over the look-ahead window, then smooth with two boxcars whose combined
        // length equals the window: the gain is guaranteed to be down when the peak arrives.
        const double h = minWin.push (env, n64, k);
        const double a = box2.process (box1.process (h));
        gainDly[(size_t) (n64 & gainMask)] = a;
        const double gd = gainDly[(size_t) ((n64 - gainOffset) & gainMask)];

        const size_t w = (size_t) (n64 & mask);
        dlyL[w] = dl;
        dlyR[w] = dr;
        dryL[w] = xl;
        dryR[w] = xr;
        const size_t rp = (size_t) ((n64 - latencySamples) & mask);
        const double ol = dlyL[rp], orr = dlyR[rp];
        const double gl = dbToGain (gd);
        const double m = onMix.next();
        double yl = dryL[rp] + m * (ol * gl - dryL[rp]);
        double yr = dryR[rp] + m * (orr * gl - dryR[rp]);
        if (m >= 1.0)
        {
            // Safety net. The look-ahead already keeps the output at or below the ceiling.
            const double limit = ceilLin * (1.0 + 1.0e-9);
            if (std::abs (yl) > limit || std::abs (yr) > limit)
                ++safetyClips;
            yl = clampv (yl, -ceilLin, ceilLin);
            yr = clampv (yr, -ceilLin, ceilLin);
        }
        L[i] = yl;
        R[i] = yr;

        const double gr = -gd * m;
        maxGr = std::max (maxGr, gr);
        histInPk = std::max (histInPk, std::max (std::abs (ol), std::abs (orr)));
        histOutPk = std::max (histOutPk, std::max (std::abs (yl), std::abs (yr)));
        histGrMax = std::max (histGrMax, gr);
        if (++histCount >= histHop)
        {
            histIn.push ((float) gainToDb (histInPk, -120.0));
            histOut.push ((float) gainToDb (histOutPk, -120.0));
            histGr.push ((float) histGrMax);
            histCount = 0;
            histInPk = histOutPk = histGrMax = 0.0;
        }
        ++n64;
    }

    grMeter.update ((float) maxGr);
}

//==============================================================================
void LoudnessMeter::kWeighting (double sr, Coeffs& shelf, Coeffs& hp)
{
    // Analog parameters of the BS.1770 pre-filter, valid at any sample rate.
    double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
    double K = std::tan (kPi * f0 / sr);
    const double Vh = std::pow (10.0, G / 20.0);
    const double Vb = std::pow (Vh, 0.4996667741545416);
    const double a0 = 1.0 + K / Q + K * K;
    shelf.b0 = (Vh + Vb * K / Q + K * K) / a0;
    shelf.b1 = 2.0 * (K * K - Vh) / a0;
    shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
    shelf.a1 = 2.0 * (K * K - 1.0) / a0;
    shelf.a2 = (1.0 - K / Q + K * K) / a0;

    f0 = 38.13547087602444;
    Q = 0.5003270373238773;
    K = std::tan (kPi * f0 / sr);
    const double d = 1.0 + K / Q + K * K;
    hp.b0 = 1.0;
    hp.b1 = -2.0;
    hp.b2 = 1.0;
    hp.a1 = 2.0 * (K * K - 1.0) / d;
    hp.a2 = (1.0 - K / Q + K * K) / d;
}

void LoudnessMeter::prepare (double sampleRate)
{
    fs = sampleRate;
    kWeighting (fs, k1, k2);
    subLen = std::max (1, (int) std::lround (0.1 * fs));
    reset();
}

void LoudnessMeter::clearHistory()
{
    binE.fill (0.0);
    binN.fill (0);
    tpMax = 0.0;
    integrated.store (-100.0f);
    truePeakMax.store (-100.0f);
}

void LoudnessMeter::reset()
{
    for (auto& s : s1) s.reset();
    for (auto& s : s2) s.reset();
    tpL.reset();
    tpR.reset();
    acc = 0.0;
    accCount = 0;
    sub.fill (0.0);
    subPos = subFilled = 0;
    momentary.store (-100.0f);
    shortTerm.store (-100.0f);
    clearHistory();
}

void LoudnessMeter::process (const double* L, const double* R, int n)
{
    if (resetRequest.exchange (false))
        clearHistory();

    double blockTp = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double yl = s2[0].process (k2, s1[0].process (k1, L[i]));
        const double yr = s2[1].process (k2, s1[1].process (k1, R[i]));
        acc += yl * yl + yr * yr;
        blockTp = std::max (blockTp, std::max (tpL.push (L[i], true), tpR.push (R[i], true)));

        if (++accCount < subLen)
            continue;

        sub[(size_t) subPos] = acc / subLen;
        subPos = (subPos + 1) % 30;
        subFilled = std::min (30, subFilled + 1);
        acc = 0.0;
        accCount = 0;

        if (subFilled < 4)
            continue;

        auto meanOf = [this] (int count)
        {
            double s = 0.0;
            for (int j = 1; j <= count; ++j)
                s += sub[(size_t) ((subPos - j + 30) % 30)];
            return s / count;
        };

        const double zM = meanOf (4);
        const double lM = zM > 0.0 ? -0.691 + 10.0 * std::log10 (zM) : -200.0;
        momentary.store ((float) std::max (-100.0, lM), std::memory_order_relaxed);
        const double zS = meanOf (subFilled);
        shortTerm.store ((float) std::max (-100.0, zS > 0.0 ? -0.691 + 10.0 * std::log10 (zS) : -200.0), std::memory_order_relaxed);

        // Gating block (400 ms, 75 % overlap) into the histogram for the integrated value.
        if (lM > -70.0)
        {
            const int b = clampv ((int) ((lM + 70.0) * 10.0), 0, kBins - 1);
            binE[(size_t) b] += zM;
            binN[(size_t) b] += 1;
        }

        double sumE = 0.0;
        uint64_t cnt = 0;
        for (int b = 0; b < kBins; ++b)
        {
            sumE += binE[(size_t) b];
            cnt += binN[(size_t) b];
        }
        if (cnt > 0)
        {
            const double relGate = -0.691 + 10.0 * std::log10 (sumE / (double) cnt) - 10.0;
            const int first = clampv ((int) std::ceil ((relGate + 70.0) * 10.0), 0, kBins);
            double e2 = 0.0;
            uint64_t c2 = 0;
            for (int b = first; b < kBins; ++b)
            {
                e2 += binE[(size_t) b];
                c2 += binN[(size_t) b];
            }
            if (c2 > 0)
                integrated.store ((float) (-0.691 + 10.0 * std::log10 (e2 / (double) c2)), std::memory_order_relaxed);
        }
    }

    tpMax = std::max (tpMax, blockTp);
    truePeakMax.store ((float) gainToDb (tpMax, -100.0), std::memory_order_relaxed);
    tpRecent.update ((float) gainToDb (blockTp, -100.0));
}
} // namespace kv
