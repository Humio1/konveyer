#pragma once
// TISHINA resonance suppressor.
//
// Short-time Fourier transform with Hann windows and 4x overlap. For every frame the level of each bin is
// compared with a smoothed envelope of the spectrum on a log-frequency scale. Whatever sticks out of that
// envelope by more than the selectivity threshold is treated as a resonance and cut, bin by bin, with
// attack and release. The broad tonal balance is left alone; only narrow peaks are tamed.
//
// Both channels go through one complex FFT (left in the real part, right in the imaginary part). Mid/side
// is done in the spectral domain, so switching modes never mixes buffers of different kinds.

#include "Fft.h"

namespace kv
{
enum class StereoMode : int { Linked = 0, Stereo, MidSide };

struct SuppressorParams
{
    double depth = 50.0;        // %: how strongly peaks are pulled down (ratio and maximum cut)
    double sharpness = 50.0;    // %: high = only narrow peaks count as resonances
    double selectivity = 50.0;  // %: how far above the envelope a peak must rise before it is touched
    double attackMs = 5.0, releaseMs = 80.0;
    double lowHz = 60.0, highHz = 18000.0;  // processing range
    double emphasis = 0.0;      // -100 .. 100: work harder on lows (-) or highs (+)
    double mix = 100.0;         // %
    double outputDb = 0.0;
    StereoMode mode = StereoMode::Linked;
    bool soft = true;           // soft knee
    bool delta = false;         // listen to what is removed
};

class Suppressor
{
public:
    static constexpr int kMaxFft = 16384;
    static constexpr int kDisplayPoints = 192;
    static constexpr double kDisplayMinHz = 20.0, kDisplayMaxHz = 20000.0;

    Suppressor() = default;

    /** quality 0..2 = fast, normal, fine. Allocates; call off the audio thread. */
    void prepare (double sampleRate, int quality);
    void reset();
    void setParams (const SuppressorParams& p) { params = p; }
    void process (float* L, float* R, int n) noexcept;

    int latencySamples() const { return fftSize; }
    int getFftSize() const { return fftSize; }
    static int fftSizeFor (double sampleRate, int quality);

    /** Frequency of display point i (log spaced). */
    static double displayHz (int i)
    {
        return kDisplayMinHz * std::pow (kDisplayMaxHz / kDisplayMinHz, (double) i / (kDisplayPoints - 1));
    }

    // Display data, written by the audio thread once per frame.
    std::array<std::atomic<float>, kDisplayPoints> displayLevel {};  // input level per point, dB (sine = dBFS)
    std::array<std::atomic<float>, kDisplayPoints> displayCut {};    // reduction per point, dB (positive)
    AtomicMax maxCut;      // deepest cut since the UI last looked, dB
    std::atomic<uint32_t> frames { 0 };

private:
    void processFrame() noexcept;
    void updateWeights() noexcept;
    void envelope (const double* lev, double* env) noexcept;
    void computeCut (const double* lev, double* cut) noexcept;
    void smoothGain (const double* target, double* gain) noexcept;

    double fs = 48000.0;
    int fftSize = 2048, hop = 512, bins = 1025, pos = 0;
    double levelOffset = 0.0;  // makes a full-scale sine read 0 dB
    Fft fft;

    std::vector<double> window, inL, inR, outL, outR;
    std::vector<std::complex<double>> z, specA, specB;
    std::vector<double> powA, powB, levA, levB, env, tmp, prefixV, prefixW, invK, cutA, cutB, gainA, gainB, weight;
    std::vector<int> dispLo, dispHi;
    std::vector<double> dispFrac;  // >= 0: point sits between bins dispLo and dispHi at this fraction
    std::array<float, kDisplayPoints> dispSmooth {};

    SuppressorParams params;
    double wLow = -1, wHigh = -1, wEmph = -999;
    StereoMode lastMode = StereoMode::Linked;
    LinearRamp outGain;
};
} // namespace kv
