#pragma once
// Shared helpers for the KONVEYER engine. Plain C++17, no JUCE, so the DSP can be tested offline.

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace kv
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

/** The engine processes at most this many samples per call; the plugin splits larger host blocks. */
constexpr int kMaxChunk = 512;

inline double dbToGain (double db) { return std::exp (db * 0.11512925464970229); }
inline double gainToDb (double g, double floorDb = -200.0)
{
    return g > 1.0e-10 ? std::max (floorDb, 20.0 * std::log10 (g)) : floorDb;
}

template <typename T> inline T sq (T x) { return x * x; }
template <typename T> inline T clampv (T x, T lo, T hi) { return std::min (hi, std::max (lo, x)); }

/** Coefficient of a one-pole smoother for a time constant in seconds. */
inline double onePoleCoeff (double seconds, double sampleRate)
{
    if (seconds <= 0.0 || sampleRate <= 0.0)
        return 0.0;
    return std::exp (-1.0 / (seconds * sampleRate));
}

/** Sample-accurate linear ramp, used for gains and crossfades. */
struct LinearRamp
{
    double current = 1.0, target = 1.0, step = 0.0;
    int remaining = 0;

    void reset (double v) noexcept { current = target = v; step = 0.0; remaining = 0; }

    void setTarget (double t, int samples) noexcept
    {
        if (! (t < target) && ! (t > target))
            return;
        target = t;
        if (samples <= 0) { current = t; remaining = 0; step = 0.0; return; }
        remaining = samples;
        step = (target - current) / samples;
    }

    inline double next() noexcept
    {
        if (remaining > 0)
        {
            current += step;
            if (--remaining == 0)
                current = target;
        }
        return current;
    }

    bool isSmoothing() const noexcept { return remaining > 0; }
};

/** Max-hold that the audio thread raises and the UI collects (and resets) when it repaints. */
struct AtomicMax
{
    std::atomic<float> v { -1000.0f };

    void update (float x) noexcept
    {
        float cur = v.load (std::memory_order_relaxed);
        while (x > cur && ! v.compare_exchange_weak (cur, x, std::memory_order_relaxed)) {}
    }

    float take (float resetTo = -1000.0f) noexcept { return v.exchange (resetTo, std::memory_order_relaxed); }
    float peek() const noexcept { return v.load (std::memory_order_relaxed); }
};

/** Lock-free display tap: the audio thread writes samples, the UI copies out the most recent ones. */
template <int Size>
class TapRing
{
public:
    void push (float x) noexcept
    {
        const auto w = write.load (std::memory_order_relaxed);
        data[(size_t) (w & (uint64_t) (Size - 1))].store (x, std::memory_order_relaxed);
        write.store (w + 1, std::memory_order_release);
    }

    /** Copies the n most recent samples into dest, oldest first. alignTo rounds the end down (2 for L/R pairs). */
    uint64_t readLatest (float* dest, int n, int alignTo = 1) const noexcept
    {
        auto w = write.load (std::memory_order_acquire);
        if (alignTo > 1)
            w -= w % (uint64_t) alignTo;
        for (int i = 0; i < n; ++i)
            dest[i] = data[(size_t) ((w - (uint64_t) n + (uint64_t) i) & (uint64_t) (Size - 1))].load (std::memory_order_relaxed);
        return w;
    }

    uint64_t written() const noexcept { return write.load (std::memory_order_acquire); }

private:
    static_assert ((Size & (Size - 1)) == 0, "TapRing size must be a power of two");
    std::array<std::atomic<float>, (size_t) Size> data {};
    std::atomic<uint64_t> write { 0 };
};
} // namespace kv
