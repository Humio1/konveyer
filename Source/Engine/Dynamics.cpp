#include "Dynamics.h"

namespace kv
{
void Compressor::prepare (double sampleRate)
{
    fs = sampleRate;
    reset();
}

void Compressor::reset()
{
    for (auto& s : hp)
        s.reset();
    hpFreq = -1.0;
    rmsState = 0.0;
    g = gSlow = gFast = 0.0;
    makeup.reset (dbToGain (params.makeupDb));
    mix.reset (clampv (params.mix, 0.0, 1.0));
}

void Compressor::process (double* L, double* R, int n)
{
    const auto& p = params;
    const bool hpOn = p.scHpfHz > 21.0;
    if (hpOn && std::abs (p.scHpfHz - hpFreq) > 1.0e-6)
    {
        hpf = design::highpass2BLT (p.scHpfHz, 0.70710678118654752, fs);
        hpFreq = p.scHpfHz;
    }

    const double T = p.thresholdDb, Rt = std::max (1.0, p.ratio), W = std::max (0.0, p.kneeDb);
    const double atkS = std::max (0.05, p.attackMs) * 0.001, relS = std::max (5.0, p.releaseMs) * 0.001;
    const double aA = onePoleCoeff (atkS, fs);
    const double aR = onePoleCoeff (relS, fs);
    const double aRms = onePoleCoeff (0.010, fs);
    // Program-dependent release: a slow stage carries sustained reduction, a fast stage the peaks on top.
    const double aAslow = onePoleCoeff (std::max (0.02, atkS * 6.0), fs);
    const double aRslow = onePoleCoeff (std::max (0.3, relS * 4.0), fs);
    const double aRfast = onePoleCoeff (std::max (0.02, relS * 0.3), fs);

    makeup.setTarget (dbToGain (p.makeupDb), (int) (0.02 * fs));
    mix.setTarget (clampv (p.mix, 0.0, 1.0), (int) (0.02 * fs));

    double maxGr = 0.0, maxLvl = -200.0;
    for (int i = 0; i < n; ++i)
    {
        const double xl = L[i], xr = R[i];
        double dl = xl, dr = xr;
        if (hpOn)
        {
            dl = hp[0].process (hpf, dl);
            dr = hp[1].process (hpf, dr);
        }

        double lvl;
        if (p.rms)
        {
            rmsState = aRms * rmsState + (1.0 - aRms) * 0.5 * (dl * dl + dr * dr);
            lvl = std::sqrt (2.0 * rmsState);  // a sine reads its peak level
        }
        else
            lvl = std::max (std::abs (dl), std::abs (dr));

        const double xdb = gainToDb (lvl);
        maxLvl = std::max (maxLvl, xdb);
        const double gc = gainComputer (xdb, T, Rt, W);

        double gr;
        if (p.autoRelease)
        {
            gSlow = gc < gSlow ? aAslow * gSlow + (1.0 - aAslow) * gc : aRslow * gSlow + (1.0 - aRslow) * gc;
            const double ft = std::min (0.0, gc - gSlow);
            gFast = ft < gFast ? aA * gFast + (1.0 - aA) * ft : aRfast * gFast + (1.0 - aRfast) * ft;
            gr = gSlow + gFast;
            g = gr;
        }
        else
        {
            g = gc < g ? aA * g + (1.0 - aA) * gc : aR * g + (1.0 - aR) * gc;
            gr = g;
            gSlow = g;
            gFast = 0.0;
        }

        maxGr = std::max (maxGr, -gr);
        const double wet = dbToGain (gr) * makeup.next();
        const double m = mix.next();
        const double gain = 1.0 - m + m * wet;
        L[i] = xl * gain;
        R[i] = xr * gain;
    }

    grMeter.update ((float) maxGr);
    levelDb.store ((float) maxLvl, std::memory_order_relaxed);
}

//==============================================================================
void MultibandCompressor::prepare (double sampleRate)
{
    fs = sampleRate;
    smooth = 1.0 - std::exp (-kSub / (0.03 * fs));
    reset();
}

void MultibandCompressor::reset()
{
    for (auto& s : split)
        s.reset();
    g.fill (0.0);
    primed = false;
    updateCrossovers (true);
    const bool anySolo = std::any_of (params.bands.begin(), params.bands.end(), [] (const MbcBandParams& b) { return b.solo; });
    for (size_t k = 0; k < 4; ++k)
    {
        const auto& b = params.bands[k];
        outGain[k].reset ((anySolo && ! b.solo) ? 0.0 : dbToGain (b.bypass ? 0.0 : b.makeupDb));
    }
}

void MultibandCompressor::updateCrossovers (bool snap)
{
    const double f0 = clampv (params.xover[0], 20.0, 0.30 * fs);
    const double f1 = clampv (params.xover[1], f0 * 1.3, 0.38 * fs);
    const double f2 = clampv (params.xover[2], f1 * 1.3, 0.45 * fs);
    const double target[3] = { std::log (f0), std::log (f1), std::log (f2) };
    for (size_t k = 0; k < 3; ++k)
    {
        const double before = xLog[k];
        if (snap || ! primed)
            xLog[k] = target[k];
        else
        {
            xLog[k] += (target[k] - xLog[k]) * smooth;
            if (std::abs (target[k] - xLog[k]) < 1.0e-5)
                xLog[k] = target[k];
        }
        if (snap || ! primed || std::abs (xLog[k] - before) > 0.0)
            xc[k].design (std::exp (xLog[k]), fs);
    }
    primed = true;
}

void MultibandCompressor::process (double* L, double* R, int n)
{
    std::array<double, 4> aA {}, aR {}, T {}, Rt {};
    for (size_t k = 0; k < 4; ++k)
    {
        const auto& b = params.bands[k];
        aA[k] = onePoleCoeff (std::max (0.05, b.attackMs) * 0.001, fs);
        aR[k] = onePoleCoeff (std::max (5.0, b.releaseMs) * 0.001, fs);
        T[k] = b.thresholdDb;
        Rt[k] = std::max (1.0, b.ratio);
    }

    const bool anySolo = std::any_of (params.bands.begin(), params.bands.end(), [] (const MbcBandParams& b) { return b.solo; });
    for (size_t k = 0; k < 4; ++k)
    {
        const auto& b = params.bands[k];
        outGain[k].setTarget ((anySolo && ! b.solo) ? 0.0 : dbToGain (b.bypass ? 0.0 : b.makeupDb), (int) (0.02 * fs));
    }

    std::array<double, 4> maxGr {};
    constexpr double knee = 6.0;
    for (int start = 0; start < n; start += kSub)
    {
        const int len = std::min (kSub, n - start);
        updateCrossovers (false);
        for (int i = start; i < start + len; ++i)
        {
            double bl[4], br[4];
            split[0].split (xc, L[i], bl);
            split[1].split (xc, R[i], br);
            double yl = 0.0, yr = 0.0;
            for (size_t k = 0; k < 4; ++k)
            {
                if (params.bands[k].bypass)
                    g[k] *= aR[k];  // let any reduction fade out
                else
                {
                    const double xdb = gainToDb (std::max (std::abs (bl[k]), std::abs (br[k])));
                    const double gc = gainComputer (xdb, T[k], Rt[k], knee);
                    g[k] = gc < g[k] ? aA[k] * g[k] + (1.0 - aA[k]) * gc : aR[k] * g[k] + (1.0 - aR[k]) * gc;
                }
                maxGr[k] = std::max (maxGr[k], -g[k]);
                const double gain = dbToGain (g[k]) * outGain[k].next();
                yl += bl[k] * gain;
                yr += br[k] * gain;
            }
            L[i] = yl;
            R[i] = yr;
        }
    }

    for (size_t k = 0; k < 4; ++k)
        grMeter[k].update ((float) maxGr[k]);
}
} // namespace kv
