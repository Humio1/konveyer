#pragma once
// Green phosphor oscilloscope tube with bezel, plus the FFT analyser that feeds the screens.

#include <juce_dsp/juce_dsp.h>
#include "Style.h"
#include "../Engine/Common.h"

namespace ui
{
class CrtScreen : public juce::Component
{
public:
    explicit CrtScreen (bool roundTube = false) : round (roundTube) {}
    void paint (juce::Graphics&) override;
    void resized() override { base.invalidate(); overlay.invalidate(); }

    /** The visible phosphor area in component coordinates. */
    juce::Rectangle<float> screenArea() const;

protected:
    /** Static parts drawn behind the trace (grids, fixed labels). Cached; call invalidateStatic() when they change. */
    virtual void paintStatic (juce::Graphics&, juce::Rectangle<float>) {}
    /** Moving parts, drawn every frame. */
    virtual void paintScreen (juce::Graphics&, juce::Rectangle<float> area) = 0;
    void invalidateStatic() { base.invalidate(); repaint(); }

    juce::String bezelLabel;
    bool round;

private:
    juce::Path tubePath() const;
    void paintBase (juce::Graphics&);
    void paintOverlay (juce::Graphics&);
    LayerCache base, overlay;
};

/** Smoothed, tilt-compensated (+4.5 dB/oct around 1 kHz) spectrum of a display tap. */
class SpectrumAnalyser
{
public:
    SpectrumAnalyser();

    template <int N>
    void pull (const kv::TapRing<N>& tap, double sampleRate)
    {
        fs = sampleRate;
        const auto w = tap.written();
        if (w == lastWritten || w < (uint64_t) kSize)
        {
            for (auto& s : smooth)
                s = std::max (-160.0f, s - 2.0f);
            return;
        }
        lastWritten = w;
        tap.readLatest (work.data(), kSize);
        analyse();
    }

    juce::Path path (juce::Rectangle<float> area, float fMin, float fMax, float dbTop, float dbBottom, bool closed) const;
    float dbAt (float freq) const;
    void clear();

private:
    void analyse();
    static constexpr int kOrder = 12, kSize = 1 << kOrder;
    juce::dsp::FFT fft { kOrder };
    std::vector<float> window, work, smooth;
    double fs = 48000.0;
    uint64_t lastWritten = 0;
};

/** Log-frequency helpers shared by the screens. */
struct FreqAxis
{
    float fMin = 15.0f, fMax = 22000.0f;
    float x (float f, juce::Rectangle<float> a) const { return a.getX() + a.getWidth() * std::log (f / fMin) / std::log (fMax / fMin); }
    float f (float x, juce::Rectangle<float> a) const { return fMin * std::pow (fMax / fMin, (x - a.getX()) / a.getWidth()); }
    void paintGrid (juce::Graphics&, juce::Rectangle<float> a, bool labels) const;
};

juce::String shortFreq (float f);
} // namespace ui
