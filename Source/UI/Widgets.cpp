#include "Widgets.h"
#include "LookAndFeel.h"

namespace ui
{
//==============================================================================
Knob::Knob (juce::String english, juce::String russian, Size s, bool bipolar)
    : en (std::move (english)), ru (std::move (russian)), size (s)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (size == Size::Large ? 320 : 260);
    slider.getProperties().set ("bipolar", bipolar);
    slider.onValueChange = [this] { repaint (valueArea.toNearestInt().expanded (2)); };
    addAndMakeVisible (slider);
}

void Knob::attach (juce::AudioProcessorValueTreeState& state, const juce::String& paramId)
{
    attachment.reset();
    if (auto* p = state.getParameter (paramId))
    {
        attachment = std::make_unique<juce::SliderParameterAttachment> (*p, slider, nullptr);
        slider.setTooltip (p->getName (64));
    }
    repaint();
}

void Knob::detach() { attachment.reset(); }

void Knob::resized()
{
    auto r = getLocalBounds().toFloat();
    const float capH = size == Size::Small ? 24.0f : 28.0f;
    r.removeFromTop (capH);
    valueArea = r.removeFromBottom (17.0f).reduced (std::max (4.0f, r.getWidth() * 0.12f), 0.0f);
    r.removeFromBottom (2.0f);
    const float d = std::min (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (d, d).toNearestInt());
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    caption (g, r.removeFromTop (size == Size::Small ? 24.0f : 28.0f), en, ru, col::cream, size == Size::Small ? 11.5f : 13.0f);

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (valueArea, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawLine (valueArea.getX() + 2.0f, valueArea.getBottom() + 0.5f, valueArea.getRight() - 2.0f, valueArea.getBottom() + 0.5f, 1.0f);
    g.setColour (slider.isEnabled() ? col::cream : col::cream.withAlpha (0.35f));
    g.setFont (mono (12.5f));
    g.drawText (attachment != nullptr ? slider.getTextFromValue (slider.getValue()) : juce::String ("--"), valueArea, juce::Justification::centred, false);
}

//==============================================================================
Lever::Lever (juce::String english, juce::String russian, juce::Colour lampCol, bool showLamp)
    : juce::Button (english), en (std::move (english)), ru (std::move (russian)), lampColour (lampCol), lamp (showLamp)
{
    setClickingTogglesState (true);
}

void Lever::attach (juce::AudioProcessorValueTreeState& state, const juce::String& paramId)
{
    attachment.reset();
    if (auto* p = state.getParameter (paramId))
    {
        attachment = std::make_unique<juce::ButtonParameterAttachment> (*p, *this, nullptr);
        setTooltip (p->getName (64));
    }
}

void Lever::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat();
    if (en.isNotEmpty())
        caption (g, r.removeFromTop (28.0f), en, ru, col::cream, 12.0f);

    const bool on = getToggleState();
    const float lampSpace = lamp ? 22.0f : 0.0f;
    auto body = r.withTrimmedRight (lampSpace);
    const juce::Point<float> c (body.getCentreX() + (lamp ? 6.0f : 0.0f), body.getCentreY());

    // ON / OFF marks (left out on the compact station levers)
    if (en.isNotEmpty())
    {
    g.setFont (label (10.0f, 0.1f));
    g.setColour (col::cream.withAlpha (on ? 0.95f : 0.5f));
    g.drawText ("ON", juce::Rectangle<float> (c.x - 34.0f, c.y - 26.0f, 22.0f, 12.0f), juce::Justification::centredRight);
    g.setColour (col::cream.withAlpha (on ? 0.5f : 0.95f));
    g.drawText ("OFF", juce::Rectangle<float> (c.x - 34.0f, c.y + 14.0f, 22.0f, 12.0f), juce::Justification::centredRight);
    }

    // mounting nut
    const float nr = 9.5f;
    juce::Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const float a = juce::MathConstants<float>::pi / 3.0f * (float) i + 0.26f;
        const juce::Point<float> p (c.x + std::cos (a) * nr, c.y + std::sin (a) * nr);
        if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
    }
    hex.closeSubPath();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillPath (hex, juce::AffineTransform::translation (1.0f, 2.0f));
    juce::ColourGradient nut (col::chrome, c.x - nr, c.y - nr, col::chromeDark, c.x + nr, c.y + nr, false);
    g.setGradientFill (nut);
    g.fillPath (hex);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.strokePath (hex, juce::PathStrokeType (0.8f));
    g.setColour (juce::Colour (0xff222624));
    g.fillEllipse (c.x - 4.5f, c.y - 4.5f, 9.0f, 9.0f);

    // lever
    const float len = std::min (24.0f, body.getHeight() * 0.42f);
    const float dir = on ? -1.0f : 1.0f;
    const juce::Point<float> tip (c.x, c.y + dir * len);
    juce::Path shaft;
    shaft.startNewSubPath (c.x - 2.6f, c.y);
    shaft.lineTo (tip.x - 1.6f, tip.y);
    shaft.lineTo (tip.x + 1.6f, tip.y);
    shaft.lineTo (c.x + 2.6f, c.y);
    shaft.closeSubPath();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillPath (shaft, juce::AffineTransform::translation (2.0f, 2.0f));
    juce::ColourGradient metal (col::chrome, c.x - 3.0f, c.y, col::chromeDark, c.x + 3.0f, c.y, false);
    metal.addColour (0.35, juce::Colours::white);
    g.setGradientFill (metal);
    g.fillPath (shaft);
    const float br = 4.4f;
    juce::ColourGradient ball (juce::Colours::white, tip.x - br * 0.5f, tip.y - br * 0.5f, col::chromeDark, tip.x + br, tip.y + br, true);
    g.setGradientFill (ball);
    g.fillEllipse (tip.x - br, tip.y - br, br * 2.0f, br * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (tip.x - br, tip.y - br, br * 2.0f, br * 2.0f, 0.7f);

    if (lamp)
        paintLamp (g, { r.getRight() - 10.0f, c.y }, 5.5f, lampColour, on ? 1.0f : 0.0f);

    if (highlighted)
    {
        g.setColour (col::cream.withAlpha (0.06f));
        g.fillRoundedRectangle (r, 4.0f);
    }
}

//==============================================================================
PushButton::PushButton (juce::String buttonText, juce::Colour lit, bool latching) : juce::Button (buttonText), litColour (lit)
{
    setClickingTogglesState (latching);
}

void PushButton::attach (juce::AudioProcessorValueTreeState& state, const juce::String& paramId)
{
    attachment.reset();
    if (auto* p = state.getParameter (paramId))
    {
        attachment = std::make_unique<juce::ButtonParameterAttachment> (*p, *this, nullptr);
        setTooltip (p->getName (64));
    }
}

void PushButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    const bool on = lampOn || (getClickingTogglesState() && getToggleState());
    const float press = (down || (getClickingTogglesState() && getToggleState())) ? 1.5f : 0.0f;

    // housing
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 3.0f);
    g.setColour (juce::Colour (0xff1f2422));
    g.fillRoundedRectangle (r, 3.0f);

    auto face = r.reduced (2.5f).translated (0.0f, press);
    const auto base = on ? litColour.interpolatedWith (col::cream, 0.35f) : juce::Colour (0xffcfc6ae);
    juce::ColourGradient grad (base.brighter (0.15f), face.getX(), face.getY(), base.darker (0.25f), face.getX(), face.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (face, 2.0f);
    if (on)
    {
        juce::ColourGradient glow (litColour.withAlpha (0.55f), face.getCentreX(), face.getCentreY(), litColour.withAlpha (0.0f), face.getRight(), face.getBottom(), true);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (face, 2.0f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.drawLine (face.getX() + 2.0f, face.getY() + 0.8f, face.getRight() - 2.0f, face.getY() + 0.8f, 0.8f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawRoundedRectangle (face, 2.0f, 0.8f);

    g.setFont (label (std::min (13.0f, face.getHeight() * 0.55f), 0.06f));
    g.setColour (on ? col::ink : col::inkSoft);
    g.drawText (getButtonText(), face, juce::Justification::centred, false);

    // radio-style buttons (band selector): red frame marks the selected one
    if (! getClickingTogglesState() && getToggleState())
    {
        g.setColour (col::red);
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 2.0f);
    }

    if (highlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (face, 2.0f);
    }
}

//==============================================================================
PianoKeys::PianoKeys (int n, IconPainter p) : count (n), painter (std::move (p)) {}

void PianoKeys::attach (juce::AudioProcessorValueTreeState& state, const juce::String& paramId)
{
    attachment.reset();
    if (auto* p = state.getParameter (paramId))
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v)
        {
            selected = juce::jlimit (0, count - 1, juce::roundToInt (v));
            repaint();
        }, nullptr);
        attachment->sendInitialUpdate();
    }
}

void PianoKeys::detach() { attachment.reset(); }

int PianoKeys::keyAt (juce::Point<float> p) const
{
    const float w = (float) getWidth() / (float) count;
    return juce::jlimit (0, count - 1, (int) (p.x / w));
}

void PianoKeys::mouseDown (const juce::MouseEvent& e)
{
    const int k = keyAt (e.position);
    if (attachment != nullptr)
        attachment->setValueAsCompleteGesture ((float) k);
    else
    {
        selected = k;
        repaint();
    }
    if (onChange)
        onChange (k);
}

void PianoKeys::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff15191a));
    g.fillRoundedRectangle (r, 3.0f);
    const float w = r.getWidth() / (float) count;
    for (int i = 0; i < count; ++i)
    {
        auto key = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight()).reduced (1.5f, 2.0f);
        const bool on = i == selected;
        const float depth = on ? 3.0f : 7.0f;
        auto top = key.withTrimmedBottom (depth).translated (0.0f, on ? 3.0f : 0.0f);
        auto front = juce::Rectangle<float> (key.getX(), top.getBottom(), key.getWidth(), key.getBottom() - top.getBottom());

        g.setColour (juce::Colour (0xff8f8771));
        g.fillRect (front);
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillRect (front.removeFromBottom (1.0f));

        const auto base = on ? juce::Colour (0xffd8ccad) : col::cream;
        juce::ColourGradient grad (base.brighter (0.1f), top.getX(), top.getY(), base.darker (on ? 0.25f : 0.08f), top.getX(), top.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRect (top);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRect (top, 0.8f);
        if (on)
        {
            g.setColour (col::red);
            g.fillRect (top.withHeight (3.0f).reduced (2.0f, 0.0f).translated (0.0f, 2.0f));
        }
        if (painter)
            painter (g, i, top.reduced (4.0f, 6.0f).translated (0.0f, 2.0f), on ? col::ink : col::inkSoft);
    }
}

//==============================================================================
RotarySwitch::RotarySwitch (juce::String english, juce::String russian, juce::StringArray l, float sweepDegrees)
    : en (std::move (english)), ru (std::move (russian)), labels (std::move (l)), sweep (juce::degreesToRadians (sweepDegrees))
{
}

void RotarySwitch::attach (juce::AudioProcessorValueTreeState& state, const juce::String& paramId)
{
    attachment.reset();
    if (auto* p = state.getParameter (paramId))
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v)
        {
            index = juce::jlimit (0, labels.size() - 1, juce::roundToInt (v));
            repaint();
        }, nullptr);
        attachment->sendInitialUpdate();
        setTooltip (p->getName (64));
    }
}

void RotarySwitch::detach() { attachment.reset(); }

float RotarySwitch::angleFor (int i) const
{
    if (labels.size() < 2)
        return 0.0f;
    return -sweep * 0.5f + sweep * (float) i / (float) (labels.size() - 1);
}

juce::Point<float> RotarySwitch::knobCentre() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromTop (en.isNotEmpty() ? 28.0f : 0.0f);
    return { r.getCentreX(), r.getCentreY() + r.getHeight() * 0.06f };
}

float RotarySwitch::knobRadius() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromTop (en.isNotEmpty() ? 28.0f : 0.0f);
    return std::min (r.getWidth(), r.getHeight()) * 0.2f;
}

void RotarySwitch::setIndex (int i, bool notify)
{
    i = juce::jlimit (0, labels.size() - 1, i);
    if (attachment != nullptr)
        attachment->setValueAsCompleteGesture ((float) i);
    else
    {
        index = i;
        repaint();
    }
    if (notify && onChange)
        onChange (i);
}

void RotarySwitch::mouseDown (const juce::MouseEvent& e)
{
    dragStartIndex = index;
    const auto c = knobCentre();
    const float rl = knobRadius() * 2.05f;
    for (int i = 0; i < labels.size(); ++i)
    {
        const float a = angleFor (i);
        const juce::Point<float> p (c.x + std::sin (a) * rl, c.y - std::cos (a) * rl);
        if (p.getDistanceFrom (e.position) < 13.0f)
        {
            setIndex (i, true);
            return;
        }
    }
}

void RotarySwitch::mouseDrag (const juce::MouseEvent& e)
{
    const int steps = juce::roundToInt ((e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY()) / 22.0f);
    if (dragStartIndex + steps != index)
        setIndex (dragStartIndex + steps, true);
}

void RotarySwitch::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float d = std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : -w.deltaX;
    if (std::abs (d) > 0.0f)
        setIndex (index + (d > 0.0f ? 1 : -1), true);
}

void RotarySwitch::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (en.isNotEmpty())
        caption (g, r.removeFromTop (28.0f), en, ru, col::cream, 12.0f);

    const auto c = knobCentre();
    const float kr = knobRadius();
    const float rl = kr * 2.05f;
    g.setFont (label (std::max (9.5f, std::min (11.5f, kr * 0.62f)), 0.04f));
    for (int i = 0; i < labels.size(); ++i)
    {
        const float a = angleFor (i);
        const juce::Point<float> d (std::sin (a), -std::cos (a));
        const bool on = i == index;
        g.setColour (col::cream.withAlpha (on ? 0.95f : 0.55f));
        g.drawLine (c.x + d.x * kr * 1.12f, c.y + d.y * kr * 1.12f, c.x + d.x * kr * 1.45f, c.y + d.y * kr * 1.45f, on ? 1.6f : 1.0f);
        const auto p = c + d * rl;
        const auto box = juce::Rectangle<float> (40.0f, 13.0f).withCentre (p);
        if (on)
            engraved (g, labels[i], box, label (std::max (9.5f, std::min (11.5f, kr * 0.62f)), 0.04f), juce::Justification::centred, col::amber);
        else
            engraved (g, labels[i], box, label (std::max (9.5f, std::min (11.5f, kr * 0.62f)), 0.04f), juce::Justification::centred, col::cream.withAlpha (0.65f));
    }
    // skirt plate under the pointer
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (c.x - kr * 1.08f, c.y - kr * 1.08f, kr * 2.16f, kr * 2.16f);
    KonveyerLookAndFeel::paintKnob (g, c, kr, angleFor (index), true);
}

//==============================================================================
NeedleMeter::NeedleMeter (Scale s) : scale (std::move (s))
{
    value = target = hold = scale.minValue;
    setInterceptsMouseClicks (false, false);
}

float NeedleMeter::posOf (float v) const
{
    v = juce::jlimit (scale.minValue, scale.maxValue, v);
    if (scale.position)
        return juce::jlimit (0.0f, 1.0f, scale.position (v));
    return (v - scale.minValue) / (scale.maxValue - scale.minValue);
}

void NeedleMeter::tick (float dt)
{
    const float tc = target > value ? scale.attackMs : scale.releaseMs;
    const float a = 1.0f - std::exp (-dt / std::max (0.001f, tc * 0.001f));
    value += (target - value) * a;
    if (scale.peakHold)
    {
        if (target >= hold)
        {
            hold = target;
            holdTimer = 1.5f;
        }
        else if ((holdTimer -= dt) <= 0.0f)
            hold = std::max (scale.minValue, hold - 12.0f * dt);
    }
    // repaint only when the needle has moved by a visible amount
    if (std::abs (posOf (value) - posOf (painted)) > 0.0015f || std::abs (posOf (hold) - posOf (paintedHold)) > 0.0015f)
    {
        painted = value;
        paintedHold = hold;
        repaint();
    }
}

namespace
{
struct MeterGeometry
{
    juce::Rectangle<float> bounds, face;
    juce::Point<float> pivot;
    float radius, a0, a1, fontH;

    explicit MeterGeometry (juce::Rectangle<float> r) : bounds (r)
    {
        face = r.reduced (std::max (12.0f, r.getWidth() * 0.06f), std::max (9.0f, r.getHeight() * 0.07f));
        fontH = juce::jlimit (9.0f, 13.0f, face.getHeight() * 0.13f);
        const float sweep = juce::degreesToRadians (46.0f);
        radius = std::min (face.getHeight() * 1.08f, (face.getWidth() * 0.5f - fontH * 1.7f) / std::sin (sweep));
        pivot = { face.getCentreX(), face.getY() + fontH * 1.9f + 8.0f + radius };
        a0 = -sweep;
        a1 = sweep;
    }
    juce::Point<float> pt (float a, float rad) const { return { pivot.x + std::sin (a) * rad, pivot.y - std::cos (a) * rad }; }
};
} // namespace

void NeedleMeter::resized()
{
    faceCache.invalidate();
    glassCache.invalidate();
}

void NeedleMeter::paint (juce::Graphics& g)
{
    const MeterGeometry m (getLocalBounds().toFloat());
    auto angleOf = [&] (float v) { return m.a0 + (m.a1 - m.a0) * posOf (v); };

    faceCache.draw (g, getLocalBounds(), [&] (juce::Graphics& cg)
    {
        const auto r = m.bounds;
        cg.setColour (juce::Colours::black.withAlpha (0.5f));
        cg.fillRoundedRectangle (r.translated (0.0f, 2.0f).withTrimmedBottom (2.0f), 6.0f);
        juce::ColourGradient bez (juce::Colour (0xff35302b), r.getX(), r.getY(), col::bakelite, r.getX(), r.getBottom(), false);
        cg.setGradientFill (bez);
        cg.fillRoundedRectangle (r.withTrimmedBottom (2.0f), 6.0f);
        for (auto p : { r.getTopLeft() + juce::Point<float> (7, 7), r.getTopRight() + juce::Point<float> (-7, 7),
                        r.getBottomLeft() + juce::Point<float> (7, -9), r.getBottomRight() + juce::Point<float> (-7, -9) })
            paintScrew (cg, p, 3.2f, 0.9f);

        const auto face = m.face;
        juce::Graphics::ScopedSaveState s (cg);
        juce::Path clip;
        clip.addRoundedRectangle (face, 3.0f);
        cg.reduceClipRegion (clip);
        juce::ColourGradient paper (juce::Colour (0xfff1e8d0), face.getCentreX(), face.getY() + face.getHeight() * 0.3f,
                                    juce::Colour (0xffcdbf9c), face.getX(), face.getBottom(), true);
        cg.setGradientFill (paper);
        cg.fillRect (face);
        paintTexture (cg, face.toNearestInt(), 0.12f);

        if (scale.redFrom < 1.0e8f)
        {
            juce::Path red;
            const float from = angleOf (scale.redFrom), to = scale.redBelow ? m.a0 : m.a1;
            red.addCentredArc (m.pivot.x, m.pivot.y, m.radius + 3.0f, m.radius + 3.0f, 0.0f, std::min (from, to), std::max (from, to), true);
            cg.setColour (col::red.withAlpha (0.9f));
            cg.strokePath (red, juce::PathStrokeType (5.0f));
        }
        juce::Path arc;
        arc.addCentredArc (m.pivot.x, m.pivot.y, m.radius, m.radius, 0.0f, m.a0, m.a1, true);
        cg.setColour (col::ink);
        cg.strokePath (arc, juce::PathStrokeType (1.2f));
        for (float v : scale.minorMarks)
        {
            const float a = angleOf (v);
            const auto p0 = m.pt (a, m.radius), p1 = m.pt (a, m.radius + 4.0f);
            cg.drawLine (p0.x, p0.y, p1.x, p1.y, 0.9f);
        }
        cg.setFont (label (m.fontH, 0.0f));
        for (const auto& [v, text] : scale.marks)
        {
            const float a = angleOf (v);
            const auto p0 = m.pt (a, m.radius), p1 = m.pt (a, m.radius + 8.0f);
            cg.setColour (col::ink);
            cg.drawLine (p0.x, p0.y, p1.x, p1.y, 1.4f);
            const auto tp = m.pt (a, m.radius + 8.0f + m.fontH * 0.75f);
            cg.setColour (v >= scale.redFrom && ! scale.redBelow ? col::red : col::ink);
            cg.drawText (text, juce::Rectangle<float> (40.0f, m.fontH + 2.0f).withCentre (tp), juce::Justification::centred, false);
        }
        cg.setFont (light (m.fontH * 1.05f, 0.05f));
        cg.setColour (col::inkSoft);
        cg.drawText (scale.unit, face.withTrimmedLeft (8.0f).withTrimmedBottom (4.0f), juce::Justification::bottomLeft, false);
        cg.setFont (label (m.fontH * 1.05f, 0.1f));
        cg.setColour (col::ink);
        cg.drawText (scale.title, face.withTrimmedRight (8.0f).withTrimmedBottom (4.0f), juce::Justification::bottomRight, false);
    });

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (m.face.toNearestInt());
        if (scale.peakHold)
        {
            const float a = angleOf (hold);
            const auto p0 = m.pt (a, m.radius - 2.0f), p1 = m.pt (a, m.radius + 5.0f);
            g.setColour (col::red);
            g.drawLine (p0.x, p0.y, p1.x, p1.y, 2.0f);
        }
        const float a = angleOf (value);
        const auto tip = m.pt (a, m.radius + 6.0f);
        const auto base = m.pt (a, std::max (m.radius * 0.2f, m.pivot.y - (m.face.getBottom() - m.face.getHeight() * 0.14f)));
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.drawLine (base.x + 2.0f, base.y + 2.0f, tip.x + 2.0f, tip.y + 2.0f, 1.6f);
        g.setColour (col::ink);
        g.drawLine (base.x, base.y, tip.x, tip.y, 1.5f);
        const auto tip2 = m.pt (a, m.radius - 10.0f);
        g.setColour (col::red);
        g.drawLine (tip2.x, tip2.y, tip.x, tip.y, 1.8f);
    }

    glassCache.draw (g, getLocalBounds(), [&] (juce::Graphics& cg)
    {
        const auto face = m.face;
        {
            juce::Graphics::ScopedSaveState s (cg);
            cg.reduceClipRegion (face.toNearestInt());
            const float hw = face.getWidth() * 0.26f;
            juce::Path house;
            house.addRoundedRectangle (face.getCentreX() - hw, face.getBottom() - face.getHeight() * 0.16f, hw * 2.0f, face.getHeight() * 0.3f, 4.0f);
            cg.setColour (juce::Colour (0xff23201c));
            cg.fillPath (house);
            juce::ColourGradient glare (juce::Colours::white.withAlpha (0.20f), face.getX(), face.getY(),
                                        juce::Colours::white.withAlpha (0.0f), face.getX() + face.getWidth() * 0.5f, face.getY() + face.getHeight() * 0.7f, false);
            cg.setGradientFill (glare);
            cg.fillRect (face);
        }
        cg.setColour (juce::Colours::black.withAlpha (0.7f));
        cg.drawRoundedRectangle (face, 3.0f, 1.2f);
    });
}

//==============================================================================
NixieReadout::NixieReadout (juce::String t, juce::String russian, juce::String u, int d, bool showSign)
    : title (std::move (t)), ru (std::move (russian)), unit (std::move (u)), digits (d), sign (showSign)
{
    setInterceptsMouseClicks (false, false);
}

void NixieReadout::setValue (float v, bool isValid)
{
    const float rounded = std::round (v * 10.0f) / 10.0f;
    if (std::abs (rounded - value) > 1.0e-4f || isValid != valid)
    {
        value = rounded;
        valid = isValid;
        repaint();
    }
}

void NixieReadout::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    caption (g, r.removeFromTop (title.length() > 0 ? 26.0f : 0.0f), title, ru, titleColour, 11.5f);

    const int tubes = digits + (sign ? 1 : 0);
    auto unitArea = r.removeFromRight (unit.isNotEmpty() ? 34.0f : 0.0f);
    const float tw = std::min (r.getWidth() / (float) tubes, r.getHeight() * 0.6f);
    const float th = std::min (r.getHeight(), tw * 1.68f);
    auto row = juce::Rectangle<float> (tw * (float) tubes, th).withCentre (r.getCentre());

    // socket strip
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (row.expanded (4.0f, 3.0f), 4.0f);

    juce::String text;
    const float mag = std::abs (value);
    const bool fits = valid && mag < std::pow (10.0f, (float) (digits - 1));
    if (fits)
    {
        const int tenths = juce::roundToInt (mag * 10.0f);
        text = juce::String (tenths).paddedLeft ('0', 2);
        text = text.paddedLeft (' ', digits);
    }

    for (int i = 0; i < tubes; ++i)
    {
        auto t = row.withWidth (tw).translated (tw * (float) i, 0.0f).reduced (1.5f, 0.0f);
        paintNixieTube (g, t);
        if (! fits)
            continue;
        if (sign && i == 0)
        {
            if (value < -0.05f)
                paintNixieDigit (g, t, '-', true);
            else if (value > 0.05f)
                paintNixieDigit (g, t, '+', true);
            continue;
        }
        const int di = i - (sign ? 1 : 0);
        const auto ch = text[di];
        paintNixieDigit (g, t, ch, ch != ' ');
        if (di == digits - 2)
            paintNixieDigit (g, t, '.', true);
    }

    if (unit.isNotEmpty())
        engraved (g, unit, unitArea, label (11.0f, 0.05f), juce::Justification::centred, titleColour.withAlpha (0.8f));
}
} // namespace ui
