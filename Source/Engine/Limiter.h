#pragma once
// Look-ahead brickwall limiter with true-peak detection, plus the loudness meter (ITU-R BS.1770-4 / EBU R128).

#include "Filters.h"

namespace kv
{
/** 4x oversampled peak estimate (windowed-sinc interpolation, 16 taps per phase). */
class TruePeakDetector
{
public:
    static constexpr int kDelay = 7;  // the returned peak belongs to the sample pushed kDelay calls earlier

    TruePeakDetector() { reset(); }
    void reset() { hist.fill (0.0); }

    /** Pushes one sample; returns the peak of the interval (x[c-1], x[c]] with c = n - kDelay. */
    inline double push (double x, bool interpolate) noexcept
    {
        for (int j = 0; j < 15; ++j)
            hist[(size_t) j] = hist[(size_t) j + 1];
        hist[15] = x;
        double pk = std::abs (hist[8]);
        if (interpolate)
        {
            const auto& k = kernel();
            for (size_t f = 0; f < 3; ++f)
            {
                double v = 0.0;
                for (size_t j = 0; j < 16; ++j)
                    v += k[f][j] * hist[j];
                pk = std::max (pk, std::abs (v));
            }
        }
        return pk;
    }

    static const std::array<std::array<double, 16>, 3>& kernel();

private:
    std::array<double, 16> hist {};
};

//==============================================================================
struct LimiterParams
{
    double driveDb = 0.0, ceilingDb = -1.0, releaseMs = 120.0, lookaheadMs = 1.5;
    bool autoRelease = true, truePeak = true;
};

class Limiter
{
public:
    void prepare (double sampleRate);
    void reset();
    void setParams (const LimiterParams& p) { params = p; }

    /** When 'on' is false the signal still runs through the delay, so latency never changes. */
    void process (double* L, double* R, int n, bool on);
    int latency() const { return latencySamples; }

    AtomicMax grMeter;
    TapRing<2048> histIn, histOut, histGr;  // 10 ms points for the history display (dB)
    int64_t safetyClips = 0;                // samples the last-resort clip had to touch (diagnostics)

private:
    void configureWindow (int k);

    struct SlidingMin
    {
        std::vector<double> val;
        std::vector<int64_t> idx;
        int head = 0, size = 0, cap = 0;
        void init (int capacity) { cap = capacity; val.assign ((size_t) cap, 0.0); idx.assign ((size_t) cap, 0); head = size = 0; }
        void clear() { head = size = 0; }
        double push (double v, int64_t i, int window) noexcept
        {
            while (size > 0 && val[(size_t) ((head + size - 1) % cap)] >= v)
                --size;
            const int pos = (head + size) % cap;
            val[(size_t) pos] = v;
            idx[(size_t) pos] = i;
            ++size;
            while (idx[(size_t) head] <= i - window)
            {
                head = (head + 1) % cap;
                --size;
            }
            return val[(size_t) head];
        }
    };

    struct Box
    {
        std::vector<double> buf;
        int len = 1, pos = 0;
        double sum = 0.0;
        void init (int capacity) { buf.assign ((size_t) std::max (1, capacity), 0.0); }
        void setLength (int l, double fill)
        {
            len = std::max (1, l);
            for (int i = 0; i < len; ++i)
                buf[(size_t) i] = fill;
            pos = 0;
            sum = fill * len;
        }
        inline double process (double v) noexcept
        {
            sum += v - buf[(size_t) pos];
            buf[(size_t) pos] = v;
            if (++pos == len)
                pos = 0;
            return sum / len;
        }
    };

    double fs = 48000.0;
    LimiterParams params;
    int kMax = 1, k = 1, latencySamples = 0, mask = 0, gainMask = 0;
    std::vector<double> dlyL, dlyR, dryL, dryR, gainDly;
    int64_t n64 = 0;
    TruePeakDetector tpL, tpR;
    SlidingMin minWin;
    Box box1, box2;
    double env = 0.0, avgGr = 0.0;
    LinearRamp drive, onMix;
    int histHop = 480, histCount = 0;
    double histInPk = 0.0, histOutPk = 0.0, histGrMax = 0.0;
};

//==============================================================================
class LoudnessMeter
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (const double* L, const double* R, int n);

    std::atomic<float> momentary { -100.0f }, shortTerm { -100.0f }, integrated { -100.0f }, truePeakMax { -100.0f };
    AtomicMax tpRecent;                 // dBTP since the UI last looked
    std::atomic<bool> resetRequest { false };

    static void kWeighting (double fs, Coeffs& shelf, Coeffs& highpass);

private:
    void clearHistory();

    double fs = 48000.0;
    Coeffs k1, k2;
    std::array<BiquadState, 2> s1 {}, s2 {};
    double acc = 0.0;
    int accCount = 0, subLen = 4800;
    std::array<double, 30> sub {};
    int subPos = 0, subFilled = 0;
    static constexpr int kBins = 800;  // -70 .. +10 LUFS in 0.1 LU steps
    std::array<double, kBins> binE {};
    std::array<uint32_t, kBins> binN {};
    TruePeakDetector tpL, tpR;
    double tpMax = 0.0;
};
} // namespace kv
