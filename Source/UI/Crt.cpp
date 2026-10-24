#include "Crt.h"

namespace ui
{
juce::Rectangle<float> CrtScreen::screenArea() const
{
    auto r = getLocalBounds().toFloat().reduced (round ? 16.0f : 13.0f);
    if (round)
    {
        const float d = std::min (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (d, d);
    }
    return r;
}

juce::Path CrtScreen::tubePath() const
{
    juce::Path tube;
    if (round)
        tube.addEllipse (screenArea());
    else
        tube.addRoundedRectangle (screenArea(), 14.0f);
    return tube;
}

void CrtScreen::paintBase (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const auto s = screenArea();

    // bezel: black stove enamel with screws
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (b.translated (0.0f, 2.0f).reduced (0.0f, 1.0f), 10.0f);
    juce::ColourGradient bez (juce::Colour (0xff2e3331), b.getX(), b.getY(), juce::Colour (0xff131615), b.getX(), b.getBottom(), false);
    g.setGradientFill (bez);
    g.fillRoundedRectangle (b.withTrimmedBottom (2.0f), 10.0f);
    paintTexture (g, b.toNearestInt(), 0.12f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (b.withTrimmedBottom (2.0f).reduced (1.0f), 9.0f, 1.0f);
    if (! round)
        for (auto p : { b.getTopLeft() + juce::Point<float> (7, 7), b.getTopRight() + juce::Point<float> (-7, 7),
                        b.getBottomLeft() + juce::Point<float> (7, -9), b.getBottomRight() + juce::Point<float> (-7, -9) })
            paintScrew (g, p, 3.0f, 0.5f);

    const auto tube = tubePath();
    g.setColour (juce::Colours::black);
    g.strokePath (tube, juce::PathStrokeType (5.0f));
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (tube);
        juce::ColourGradient face (juce::Colour (0xff0a2416), s.getCentreX(), s.getCentreY(), juce::Colour (0xff010604), s.getX(), s.getY(), true);
        g.setGradientFill (face);
        g.fillRect (s);
        paintStatic (g, s);
    }

    if (bezelLabel.isNotEmpty())
        engraved (g, bezelLabel, juce::Rectangle<float> (b.getX(), b.getBottom() - 15.0f, b.getWidth(), 12.0f),
                  label (9.0f, 0.18f), juce::Justification::centred, col::cream.withAlpha (0.55f));
}

void CrtScreen::paintOverlay (juce::Graphics& g)
{
    const auto s = screenArea();
    const auto tube = tubePath();
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (tube);
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        for (float y = s.getY(); y < s.getBottom(); y += 3.0f)
            g.fillRect (s.getX(), y, s.getWidth(), 1.0f);
        juce::ColourGradient vig (juce::Colours::transparentBlack, s.getCentreX(), s.getCentreY(),
                                  juce::Colours::black.withAlpha (0.55f), s.getX() - s.getWidth() * 0.08f, s.getY() - s.getHeight() * 0.08f, true);
        g.setGradientFill (vig);
        g.fillRect (s);
        juce::ColourGradient glass (juce::Colours::white.withAlpha (0.07f), s.getX(), s.getY(),
                                    juce::Colours::white.withAlpha (0.0f), s.getX() + s.getWidth() * 0.35f, s.getY() + s.getHeight() * 0.45f, false);
        g.setGradientFill (glass);
        g.fillRect (s);
    }
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.strokePath (tube, juce::PathStrokeType (1.0f), juce::AffineTransform::translation (0.0f, 1.5f));
}

void CrtScreen::paint (juce::Graphics& g)
{
    base.draw (g, getLocalBounds(), [this] (juce::Graphics& cg) { paintBase (cg); });
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (tubePath());
        paintScreen (g, screenArea());
    }
    overlay.draw (g, getLocalBounds(), [this] (juce::Graphics& cg) { paintOverlay (cg); });
}

//==============================================================================
SpectrumAnalyser::SpectrumAnalyser()
    : window ((size_t) kSize), work ((size_t) kSize * 2), smooth ((size_t) kSize / 2 + 1, -160.0f)
{
    for (int i = 0; i < kSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) kSize);
}

void SpectrumAnalyser::clear()
{
    std::fill (smooth.begin(), smooth.end(), -160.0f);
    lastWritten = 0;
}

void SpectrumAnalyser::analyse()
{
    for (int i = 0; i < kSize; ++i)
        work[(size_t) i] *= window[(size_t) i];
    std::fill (work.begin() + kSize, work.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (work.data());

    const float norm = 4.0f / (float) kSize;
    for (int k = 1; k <= kSize / 2; ++k)
    {
        const float freq = (float) k * (float) fs / (float) kSize;
        const float db = 20.0f * std::log10 (work[(size_t) k] * norm + 1.0e-9f) + 4.5f * std::log2 (freq / 1000.0f);
        auto& s = smooth[(size_t) k];
        s = db > s ? 0.45f * s + 0.55f * db : 0.86f * s + 0.14f * db;
    }
}

float SpectrumAnalyser::dbAt (float freq) const
{
    const float bin = freq * (float) kSize / (float) fs;
    const int b0 = juce::jlimit (1, kSize / 2 - 1, (int) bin);
    const float t = juce::jlimit (0.0f, 1.0f, bin - (float) b0);
    return smooth[(size_t) b0] * (1.0f - t) + smooth[(size_t) b0 + 1] * t;
}

juce::Path SpectrumAnalyser::path (juce::Rectangle<float> a, float fMin, float fMax, float dbTop, float dbBottom, bool closed) const
{
    juce::Path p;
    const float step = 2.0f;
    const float logRange = std::log (fMax / fMin);
    bool started = false;
    for (float x = a.getX(); x <= a.getRight() + 0.01f; x += step)
    {
        const float f0 = fMin * std::exp (logRange * (x - a.getX()) / a.getWidth());
        const float f1 = fMin * std::exp (logRange * (x + step - a.getX()) / a.getWidth());
        if (f0 >= (float) fs * 0.5f)
            break;
        const float b0 = f0 * (float) kSize / (float) fs, b1 = f1 * (float) kSize / (float) fs;
        float db;
        if (b1 - b0 < 1.0f)
            db = dbAt (f0);
        else
        {
            db = -160.0f;
            for (int b = std::max (1, (int) b0); b <= std::min (kSize / 2, (int) b1); ++b)
                db = std::max (db, smooth[(size_t) b]);
        }
        const float y = juce::jlimit (a.getY(), a.getBottom(), juce::jmap (db, dbTop, dbBottom, a.getY(), a.getBottom()));
        if (! started)
        {
            if (closed)
            {
                p.startNewSubPath (x, a.getBottom());
                p.lineTo (x, y);
            }
            else
                p.startNewSubPath (x, y);
            started = true;
        }
        else
            p.lineTo (x, y);
        if (closed && x + step > a.getRight() + 0.01f)
            p.lineTo (x, a.getBottom());
    }
    if (closed && started)
    {
        p.lineTo (p.getCurrentPosition().x, a.getBottom());
        p.closeSubPath();
    }
    return p;
}

//==============================================================================
juce::String shortFreq (float f)
{
    if (f >= 1000.0f)
    {
        const float k = f / 1000.0f;
        return juce::String (k, k >= 10.0f ? 1 : 2) + "k";
    }
    return juce::String (juce::roundToInt (f));
}

void FreqAxis::paintGrid (juce::Graphics& g, juce::Rectangle<float> a, bool labels) const
{
    const float majors[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    for (float decade = 10.0f; decade <= 10000.0f; decade *= 10.0f)
        for (int m = 1; m <= 9; ++m)
        {
            const float f = decade * (float) m;
            if (f < fMin || f > fMax)
                continue;
            const float xx = x (f, a);
            g.setColour (col::grid.withAlpha (0.55f));
            g.drawVerticalLine (juce::roundToInt (xx), a.getY(), a.getBottom());
        }
    g.setFont (mono (11.0f));
    for (float f : majors)
    {
        if (f < fMin || f > fMax)
            continue;
        const float xx = x (f, a);
        g.setColour (col::grid.brighter (0.5f));
        g.drawVerticalLine (juce::roundToInt (xx), a.getY(), a.getBottom());
        if (labels)
        {
            g.setColour (col::phosphorDim.withAlpha (0.9f));
            g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                        juce::Rectangle<float> (xx - 20.0f, a.getBottom() - 16.0f, 40.0f, 14.0f), juce::Justification::centred, false);
        }
    }
}
} // namespace ui
