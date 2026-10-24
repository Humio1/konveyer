#include "LookAndFeel.h"

using namespace ui;

KonveyerLookAndFeel::KonveyerLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, col::bakelite);
    setColour (juce::PopupMenu::textColourId, col::cream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::red);
    setColour (juce::PopupMenu::highlightedTextColourId, col::cream);
    setColour (juce::TooltipWindow::backgroundColourId, col::cream);
    setColour (juce::TooltipWindow::textColourId, col::ink);
    setColour (juce::TooltipWindow::outlineColourId, col::ink);
}

void KonveyerLookAndFeel::paintKnob (juce::Graphics& g, juce::Point<float> c, float R, float angle, bool chickenHead)
{
    const juce::Point<float> dir (std::sin (angle), -std::cos (angle));

    // drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (c.x - R + 1.5f, c.y - R + 3.0f, R * 2.0f, R * 2.0f);

    if (chickenHead)
    {
        // round bakelite base
        juce::ColourGradient base (juce::Colour (0xff3a332e), c.x - R, c.y - R, col::bakelite, c.x + R * 0.6f, c.y + R, false);
        g.setGradientFill (base);
        g.fillEllipse (c.x - R * 0.78f, c.y - R * 0.78f, R * 1.56f, R * 1.56f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (c.x - R * 0.78f, c.y - R * 0.78f, R * 1.56f, R * 1.56f, 1.0f);

        // tapered pointer ("chicken head") with a short tail
        juce::Path beak;
        beak.startNewSubPath (-R * 0.34f, R * 0.62f);
        beak.lineTo (-R * 0.13f, -R * 1.02f);
        beak.quadraticTo (0.0f, -R * 1.22f, R * 0.13f, -R * 1.02f);
        beak.lineTo (R * 0.34f, R * 0.62f);
        beak.quadraticTo (0.0f, R * 0.86f, -R * 0.34f, R * 0.62f);
        beak.closeSubPath();
        const auto tr = juce::AffineTransform::rotation (angle).translated (c.x, c.y);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillPath (beak, tr.translated (1.5f, 2.5f));
        juce::ColourGradient body (juce::Colour (0xff57504a), c.x - R * 0.8f, c.y - R * 0.8f, juce::Colour (0xff141110), c.x + R * 0.6f, c.y + R * 0.8f, false);
        g.setGradientFill (body);
        g.fillPath (beak, tr);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.strokePath (beak, juce::PathStrokeType (0.9f), tr);
        // cream index line along the pointer
        const auto p0 = c + dir * (R * 0.05f), p1 = c + dir * (R * 0.98f);
        g.setColour (col::cream);
        g.drawLine (p0.x, p0.y, p1.x, p1.y, 2.0f);
        return;
    }

    // knurled bakelite skirt
    juce::ColourGradient skirt (juce::Colour (0xff3d3530), c.x - R, c.y - R, col::bakelite, c.x + R * 0.5f, c.y + R, false);
    g.setGradientFill (skirt);
    g.fillEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    const int teeth = 36;
    for (int i = 0; i < teeth; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / (float) teeth;
        const juce::Point<float> d (std::sin (a), -std::cos (a));
        g.drawLine (c.x + d.x * R * 0.86f, c.y + d.y * R * 0.86f, c.x + d.x * R * 0.99f, c.y + d.y * R * 0.99f, 1.0f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawEllipse (c.x - R + 0.5f, c.y - R + 0.5f, R * 2.0f - 1.0f, R * 2.0f - 1.0f, 1.0f);

    // brushed aluminium cap
    const float cap = R * 0.66f;
    juce::ColourGradient metal (juce::Colour (0xffeceeea), c.x - cap * 0.55f, c.y - cap * 0.7f, juce::Colour (0xff6a716e), c.x + cap * 0.8f, c.y + cap, true);
    metal.addColour (0.35, juce::Colour (0xffb4b9b5));
    g.setGradientFill (metal);
    g.fillEllipse (c.x - cap, c.y - cap, cap * 2.0f, cap * 2.0f);
    for (float rr = cap * 0.2f; rr < cap; rr += 1.4f)
    {
        g.setColour ((((int) (rr * 10.0f)) % 3 == 0 ? juce::Colours::black : juce::Colours::white).withAlpha (0.07f));
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f, 0.6f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.drawEllipse (c.x - cap + 1.5f, c.y - cap + 1.5f, cap * 2.0f - 3.0f, cap * 2.0f - 3.0f, 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - cap, c.y - cap, cap * 2.0f, cap * 2.0f, 0.8f);

    // red index line
    const auto p0 = c + dir * (cap * 0.15f), p1 = c + dir * (R * 0.94f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawLine (p0.x + 0.6f, p0.y + 0.8f, p1.x + 0.6f, p1.y + 0.8f, 2.6f);
    g.setColour (col::red);
    g.drawLine (p0.x, p0.y, p1.x, p1.y, 2.4f);
}

void KonveyerLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                            float start, float end, juce::Slider& s)
{
    const auto b = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float R = std::min (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float knobR = R * 0.74f;

    // silk-screened scale on the enamel
    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (end - start) * (float) i / 10.0f;
        const juce::Point<float> d (std::sin (a), -std::cos (a));
        const bool major = i == 0 || i == 10 || i == 5;
        const float r0 = R * 0.84f, r1 = major ? R * 1.0f : R * 0.94f;
        g.setColour (col::cream.withAlpha (major ? 0.95f : 0.7f));
        g.drawLine (c.x + d.x * r0, c.y + d.y * r0, c.x + d.x * r1, c.y + d.y * r1, major ? 1.6f : 1.1f);
    }
    // value arc
    {
        const float angle = start + pos * (end - start);
        const float from = bipolar ? (start + end) * 0.5f : start;
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, R * 0.80f, R * 0.80f, 0.0f, std::min (from, angle), std::max (from, angle), true);
        g.setColour (col::amber.withAlpha (s.isEnabled() ? 0.85f : 0.25f));
        g.strokePath (arc, juce::PathStrokeType (1.6f));
    }

    paintKnob (g, c, knobR, start + pos * (end - start));
    if (! s.isEnabled())
    {
        g.setColour (col::enamel.withAlpha (0.55f));
        g.fillEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
    }
}

juce::Font KonveyerLookAndFeel::getPopupMenuFont() { return regular (15.0f, 0.04f); }

void KonveyerLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (col::bakelite);
    g.setColour (col::cream.withAlpha (0.25f));
    g.drawRect (0, 0, w, h, 1);
}

void KonveyerLookAndFeel::getIdealPopupMenuItemSize (const juce::String& text, bool sep, int, int& w, int& h)
{
    if (sep)
    {
        w = 50;
        h = 8;
        return;
    }
    juce::GlyphArrangement ga;
    ga.addLineOfText (getPopupMenuFont(), text, 0.0f, 0.0f);
    w = (int) ga.getBoundingBox (0, -1, true).getWidth() + 56;
    h = 24;
}

void KonveyerLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                             bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                                             const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour (col::cream.withAlpha (0.2f));
        g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    auto r = area.toFloat();
    if (isHighlighted && isActive)
    {
        g.setColour (col::red);
        g.fillRect (r);
    }
    g.setColour (isActive ? col::cream : col::cream.withAlpha (0.35f));
    g.setFont (getPopupMenuFont());
    g.drawText (text, r.withTrimmedLeft (26.0f).withTrimmedRight (20.0f), juce::Justification::centredLeft, true);
    if (isTicked)
        paintLamp (g, { r.getX() + 13.0f, r.getCentreY() }, 3.5f, col::amber, 1.0f);
    if (hasSubMenu)
    {
        juce::Path tri;
        const float cx = r.getRight() - 12.0f, cy = r.getCentreY();
        tri.addTriangle (cx - 3.0f, cy - 4.0f, cx - 3.0f, cy + 4.0f, cx + 3.0f, cy);
        g.fillPath (tri);
    }
}

juce::Rectangle<int> KonveyerLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (regular (14.0f), tipText, 0.0f, 0.0f);
    const int w = (int) ga.getBoundingBox (0, -1, true).getWidth() + 18, h = 24;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin (parentArea);
}

void KonveyerLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    paintPlate (g, { 0.0f, 0.0f, (float) w, (float) h - 2.0f }, col::cream, false);
    inked (g, text, { 0.0f, 0.0f, (float) w, (float) h - 2.0f }, regular (14.0f), juce::Justification::centred);
}
