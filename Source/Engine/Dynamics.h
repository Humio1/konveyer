#pragma once
// Bus compressor, 4-band compressor and the Linkwitz-Riley band splitters they share with the imager.

#include "Filters.h"

namespace kv
{
/** Static curve: gain change in dB (<= 0) for an input level, soft knee of width W. */
inline double gainComputer (double xDb, double T, double R, double W)
{
    const double over = xDb - T;
    if (2.0 * over < -W)
        return 0.0;
    if (W > 0.0 && 2.0 * std::abs (over) <= W)
        return (1.0 / R - 1.0) * sq (over + 0.5 * W) / (2.0 * W);
    return (1.0 / R - 1.0) * over;
}

//==============================================================================
/** Coefficients of one 4th-order Linkwitz-Riley crossover point (two Butterworth sections each way). */
struct XoverCoeffs
{
    Coeffs lp, hp, ap;
    void design (double freq, double fs)
    {
        lp = design::lowpass2BLT (freq, 0.70710678118654752, fs);
        hp = design::highpass2BLT (freq, 0.70710678118654752, fs);
        ap = design::allpass2BLT (freq, 0.70710678118654752, fs);
    }
};

/** Splits one channel into N bands whose sum is an allpass (flat magnitude). N = 3 or 4. */
template <int N>
struct LRSplitter
{
    static_assert (N == 3 || N == 4, "3 or 4 bands");
    static constexpr size_t kPoints = (size_t) (N - 1);
    std::array<std::array<BiquadState, 2>, kPoints> lp {}, hp {};
    std::array<BiquadState, kPoints> apLow {};   // allpasses for the lowest band (xovers 1..N-2)
    std::array<BiquadState, kPoints> apMid {};   // allpass for band 1 (xover 2), only used when N == 4

    void reset()
    {
        for (auto& a : lp) for (auto& s : a) s.reset();
        for (auto& a : hp) for (auto& s : a) s.reset();
        for (auto& s : apLow) s.reset();
        for (auto& s : apMid) s.reset();
    }

    inline void split (const std::array<XoverCoeffs, kPoints>& c, double x, double* out) noexcept
    {
        double rest = x;
        for (int k = 0; k < N - 1; ++k)
        {
            const auto& ck = c[(size_t) k];
            out[(size_t) k] = lp[(size_t) k][1].process (ck.lp, lp[(size_t) k][0].process (ck.lp, rest));
            rest = hp[(size_t) k][1].process (ck.hp, hp[(size_t) k][0].process (ck.hp, rest));
        }
        out[(size_t) (N - 1)] = rest;
        // Phase alignment: band i passes through the allpasses of all higher crossovers.
        for (int k = 1; k < N - 1; ++k)
            out[0] = apLow[(size_t) k].process (c[(size_t) k].ap, out[0]);
        if constexpr (N == 4)
            out[1] = apMid[2].process (c[2].ap, out[1]);
    }
};

//==============================================================================
struct CompParams
{
    double thresholdDb = 0.0, ratio = 2.0, attackMs = 30.0, releaseMs = 200.0, kneeDb = 6.0;
    double makeupDb = 0.0, mix = 1.0, scHpfHz = 20.0;
    bool autoRelease = false, rms = false;
};

class Compressor
{
public:
    void prepare (double sampleRate);
    void reset();
    void setParams (const CompParams& p) { params = p; }
    void process (double* L, double* R, int n);

    AtomicMax grMeter;                      // gain reduction in dB (positive)
    std::atomic<float> levelDb { -100.0f }; // detector level, for the transfer display

private:
    double fs = 48000.0;
    CompParams params;
    Coeffs hpf;
    std::array<BiquadState, 2> hp {};
    double hpFreq = -1.0;
    double rmsState = 0.0, g = 0.0, gSlow = 0.0, gFast = 0.0;
    LinearRamp makeup, mix;
};

//==============================================================================
struct MbcBandParams
{
    double thresholdDb = 0.0, ratio = 2.0, attackMs = 20.0, releaseMs = 150.0, makeupDb = 0.0;
    bool solo = false, bypass = false;
};

struct MbcParams
{
    std::array<double, 3> xover { 120.0, 1000.0, 6000.0 };
    std::array<MbcBandParams, 4> bands;
};

class MultibandCompressor
{
public:
    void prepare (double sampleRate);
    void reset();
    void setParams (const MbcParams& p) { params = p; }
    void process (double* L, double* R, int n);

    std::array<AtomicMax, 4> grMeter;

private:
    static constexpr int kSub = 16;
    void updateCrossovers (bool snap);

    double fs = 48000.0;
    MbcParams params;
    std::array<double, 3> xLog {};
    std::array<XoverCoeffs, 3> xc {};
    std::array<LRSplitter<4>, 2> split {};
    std::array<double, 4> g {};
    std::array<LinearRamp, 4> outGain {};   // makeup, solo and bypass
    double smooth = 0.0;
    bool primed = false;
};
} // namespace kv
