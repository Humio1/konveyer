#include "Panels.h"

namespace ui
{
namespace
{
const char* kRussian[kv::kNumModules] = { "ЭКВАЛАЙЗЕР", "КОМПРЕССОР", "МНОГОПОЛОСНЫЙ", "СТЕРЕОБАЗА", "ЛИМИТЕР" };
const char* kTitles[kv::kNumModules] = { "EQUALIZER", "COMPRESSOR", "MULTIBAND", "STEREO", "LIMITER" };
} // namespace

Conveyor::Conveyor (KonveyerProcessor& p) : proc (p)
{
    for (int m = 0; m < kv::kNumModules; ++m)
    {
        auto lever = std::make_unique<Lever> ("", "", juce::Colour (0xff9dff6e), true);
        lever->attach (proc.apvts, params::moduleOn (m));
        addAndMakeVisible (*lever);
        levers[(size_t) m] = std::move (lever);
    }
    order = proc.getOrder();
}

juce::Rectangle<float> Conveyor::beltArea() const
{
    auto r = getLocalBounds().toFloat();
    return { r.getX() + 18.0f, r.getBottom() - 30.0f, r.getWidth() - 36.0f, 20.0f };
}

float Conveyor::plateWidth() const
{
    const auto b = beltArea();
    return (b.getWidth() - 28.0f - 4.0f * 30.0f) / 5.0f;
}

float Conveyor::slotX (int slot) const
{
    const auto b = beltArea();
    return b.getX() + 14.0f + (float) slot * (plateWidth() + 30.0f);
}

juce::Rectangle<float> Conveyor::plateRect (int module) const
{
    return { x[(size_t) module], 6.0f, plateWidth(), beltArea().getY() - 4.0f };
}

int Conveyor::moduleAt (juce::Point<float> p) const
{
    for (int m = 0; m < kv::kNumModules; ++m)
        if (plateRect (m).contains (p))
            return m;
    return -1;
}

void Conveyor::setSelected (int module)
{
    selected = module;
    repaint();
}

void Conveyor::resized()
{
    order = proc.getOrder();
    for (int slot = 0; slot < kv::kNumModules; ++slot)
        x[(size_t) order[(size_t) slot]] = slotX (slot);
    placeLevers();
}

void Conveyor::placeLevers()
{
    for (int m = 0; m < kv::kNumModules; ++m)
    {
        const auto r = plateRect (m);
        levers[(size_t) m]->setBounds (juce::Rectangle<float> (r.getRight() - 44.0f, r.getBottom() - 58.0f, 40.0f, 54.0f).toNearestInt());
    }
}

juce::String Conveyor::status (int m) const
{
    switch (m)
    {
        case kv::kModEq:
        {
            int n = 0;
            for (int b = 0; b < kv::kEqBands; ++b)
                n += proc.apvts.getRawParameterValue (params::eq (b, "on"))->load() > 0.5f ? 1 : 0;
            return n == 0 ? juce::String ("FLAT") : juce::String (n) + (n == 1 ? " BAND" : " BANDS");
        }
        case kv::kModComp:    return last.compGr > 0.05f ? "GR " + juce::String (last.compGr, 1) + " dB" : juce::String ("GR 0.0 dB");
        case kv::kModMbc:     return "GR " + juce::String (*std::max_element (last.mbcGr.begin(), last.mbcGr.end()), 1) + " dB";
        case kv::kModImager:  return "CORR " + juce::String (last.correlation, 2);
        case kv::kModLimiter: return "GR " + juce::String (last.limGr, 1) + " dB";
        default: return {};
    }
}

void Conveyor::refresh (const MeterFrame& f)
{
    last = f;
    const float level = std::max (f.outL, f.outR);
    signal += ((level > -60.0f ? 1.0f : 0.0f) - signal) * 0.08f;
    beltPhase = std::fmod (beltPhase + f.dt * 26.0f * signal, 40.0f);

    // live order (may have changed from a preset or the host)
    if (dragModule < 0)
        order = proc.getOrder();

    std::array<int, kv::kNumModules> slotOf {};
    for (int s = 0; s < kv::kNumModules; ++s)
        slotOf[(size_t) order[(size_t) s]] = s;
    bool moving = dragModule >= 0 && dragMoved;
    for (int m = 0; m < kv::kNumModules; ++m)
    {
        if (m == dragModule && dragMoved)
            x[(size_t) m] = dragX;
        else
        {
            const float target = slotX (slotOf[(size_t) m]);
            const float before = x[(size_t) m];
            x[(size_t) m] += (target - x[(size_t) m]) * 0.35f;
            if (std::abs (target - x[(size_t) m]) < 0.2f)
                x[(size_t) m] = target;
            moving = moving || std::abs (x[(size_t) m] - before) > 0.01f;
        }
    }
    placeLevers();

    // Repaint only what changed: everything while stations move, otherwise the belt and the status strips.
    if (moving)
    {
        repaint();
        return;
    }
    if (signal > 0.01f)
        repaint (beltArea().expanded (20.0f, 18.0f).toNearestInt());
    for (int m = 0; m < kv::kNumModules; ++m)
    {
        const bool on = proc.apvts.getRawParameterValue (params::moduleOn (m))->load() > 0.5f;
        const auto text = (on ? "1" : "0") + status (m);
        if (text != lastStatus[(size_t) m])
        {
            lastStatus[(size_t) m] = text;
            repaint (plateRect (m).expanded (12.0f).toNearestInt());
        }
    }
}

void Conveyor::paint (juce::Graphics& g)
{
    const auto belt = beltArea();

    // frame rails and rollers
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (belt.expanded (14.0f, 6.0f).translated (0.0f, 2.0f), 14.0f);
    for (float cx : { belt.getX(), belt.getRight() })
    {
        const float r = 15.0f;
        juce::ColourGradient roll (col::chrome, cx - r, belt.getY() - 5.0f, col::chromeDark, cx + r, belt.getBottom() + 5.0f, false);
        g.setGradientFill (roll);
        g.fillEllipse (cx - r, belt.getCentreY() - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (cx - r, belt.getCentreY() - r, r * 2.0f, r * 2.0f, 1.0f);
        paintScrew (g, { cx, belt.getCentreY() }, 4.0f, beltPhase * 0.3f);
    }
    // rubber belt with moving chevrons
    juce::ColourGradient rubber (juce::Colour (0xff2b2a28), belt.getX(), belt.getY(), juce::Colour (0xff121110), belt.getX(), belt.getBottom(), false);
    g.setGradientFill (rubber);
    g.fillRect (belt);
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (belt.toNearestInt());
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        for (float xx = belt.getX() - 40.0f + beltPhase; xx < belt.getRight() + 40.0f; xx += 40.0f)
        {
            juce::Path chev;
            chev.startNewSubPath (xx, belt.getY() + 3.0f);
            chev.lineTo (xx + 8.0f, belt.getCentreY());
            chev.lineTo (xx, belt.getBottom() - 3.0f);
            g.strokePath (chev, juce::PathStrokeType (3.0f));
        }
    }
    g.setColour (juce::Colours::black);
    g.drawRect (belt, 1.0f);

    // IN / OUT markers on the rails
    engraved (g, "IN", juce::Rectangle<float> (belt.getX() - 16.0f, belt.getY() - 26.0f, 32.0f, 14.0f), label (11.0f, 0.15f));
    engraved (g, "OUT", juce::Rectangle<float> (belt.getRight() - 16.0f, belt.getY() - 26.0f, 32.0f, 14.0f), label (11.0f, 0.15f));

    // stations: draw the dragged one last so it floats above the others
    std::array<int, kv::kNumModules> drawOrder = order;
    if (dragModule >= 0)
    {
        auto it = std::find (drawOrder.begin(), drawOrder.end(), dragModule);
        std::rotate (it, it + 1, drawOrder.end());
    }
    std::array<int, kv::kNumModules> slotOf {};
    for (int s = 0; s < kv::kNumModules; ++s)
        slotOf[(size_t) order[(size_t) s]] = s;

    const auto clip = g.getClipBounds().toFloat();
    for (int m : drawOrder)
    {
        auto r = plateRect (m);
        if (! clip.intersects (r.expanded (14.0f)))
            continue;
        const bool sel = m == selected;
        const bool lifted = m == dragModule && dragMoved;
        const bool on = proc.apvts.getRawParameterValue (params::moduleOn (m))->load() > 0.5f;
        if (lifted)
            r = r.translated (0.0f, -4.0f);

        if (sel)
        {
            juce::ColourGradient glow (col::amber.withAlpha (0.5f), r.getCentreX(), r.getCentreY(), col::amber.withAlpha (0.0f), r.getX() - 14.0f, r.getCentreY(), true);
            g.setGradientFill (glow);
            g.fillRoundedRectangle (r.expanded (10.0f), 10.0f);
        }
        if (lifted)
        {
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.fillRoundedRectangle (r.translated (4.0f, 8.0f), 4.0f);
        }
        paintPlate (g, r, sel ? juce::Colour (0xfff1e8cf) : col::cream, true);

        const auto pad = r.reduced (10.0f, 7.0f);
        g.setFont (stencil (30.0f));
        g.setColour (col::red);
        g.drawText (juce::String (slotOf[(size_t) m] + 1).paddedLeft ('0', 2), pad.withHeight (30.0f), juce::Justification::topLeft, false);

        auto text = pad.withTrimmedLeft (40.0f);
        inked (g, kTitles[m], text.withHeight (18.0f).translated (0.0f, 2.0f), label (14.0f, 0.05f), juce::Justification::centredLeft);
        inked (g, cyr (kRussian[m]), text.withHeight (14.0f).translated (0.0f, 19.0f), light (10.5f, 0.05f), juce::Justification::centredLeft, col::inkSoft);

        auto strip = juce::Rectangle<float> (pad.getX(), pad.getBottom() - 19.0f, pad.getWidth() - 44.0f, 18.0f);
        g.setColour (juce::Colour (0xff1b1a17));
        g.fillRoundedRectangle (strip, 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.drawLine (strip.getX() + 2.0f, strip.getBottom() + 0.5f, strip.getRight() - 2.0f, strip.getBottom() + 0.5f, 0.8f);
        g.setFont (mono (11.5f));
        g.setColour (on ? col::amber : col::amber.withAlpha (0.3f));
        g.drawText (on ? status (m) : juce::String ("BYPASSED"), strip, juce::Justification::centred, false);

        if (! on)
        {
            juce::Graphics::ScopedSaveState s (g);
            g.addTransform (juce::AffineTransform::rotation (-0.16f, r.getCentreX(), r.getCentreY()));
            auto stamp = juce::Rectangle<float> (92.0f, 28.0f).withCentre (r.getCentre().translated (-12.0f, -4.0f));
            g.setColour (col::red.withAlpha (0.72f));
            g.drawRoundedRectangle (stamp, 3.0f, 2.2f);
            g.setFont (label (18.0f, 0.18f));
            g.drawText (cyr ("ВЫКЛ"), stamp, juce::Justification::centred, false);
        }
    }

    // flow arrows between stations
    for (int s = 0; s < kv::kNumModules - 1; ++s)
    {
        const float ax = slotX (s) + plateWidth() + 15.0f, ay = 6.0f + (beltArea().getY() - 10.0f) * 0.5f;
        juce::Path arrow;
        arrow.addTriangle (ax - 5.0f, ay - 7.0f, ax - 5.0f, ay + 7.0f, ax + 6.0f, ay);
        g.setColour (col::cream.withAlpha (0.75f));
        g.fillPath (arrow);
    }
}

void Conveyor::mouseDown (const juce::MouseEvent& e)
{
    dragModule = moduleAt (e.position);
    dragMoved = false;
    if (dragModule >= 0)
    {
        dragOffset = e.position.x - x[(size_t) dragModule];
        dragX = x[(size_t) dragModule];
        if (onSelect)
            onSelect (dragModule);
    }
}

void Conveyor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragModule < 0)
        return;
    if (! dragMoved && std::abs (e.getDistanceFromDragStartX()) < 6)
        return;
    dragMoved = true;
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    dragX = juce::jlimit (slotX (0), slotX (kv::kNumModules - 1), e.position.x - dragOffset);

    // preview: move the dragged station to the slot under the mouse
    const int target = juce::jlimit (0, kv::kNumModules - 1, juce::roundToInt ((dragX - slotX (0)) / (plateWidth() + 30.0f)));
    kv::Order next {};
    int i = 0;
    for (int m : order)
        if (m != dragModule)
            next[(size_t) i++] = m;
    // insert
    kv::Order result {};
    int src = 0;
    for (int s = 0; s < kv::kNumModules; ++s)
        result[(size_t) s] = (s == target) ? dragModule : next[(size_t) src++];
    order = result;
    x[(size_t) dragModule] = dragX;
    placeLevers();
    repaint();
}

void Conveyor::mouseUp (const juce::MouseEvent&)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
    if (dragModule >= 0 && dragMoved && order != proc.getOrder())
        proc.setOrder (order);
    dragModule = -1;
    dragMoved = false;
}

//==============================================================================
namespace
{
NeedleMeter::Scale outputScale (const juce::String& title)
{
    NeedleMeter::Scale s;
    s.minValue = -40.0f;
    s.maxValue = 3.0f;
    s.position = [] (float v) { return std::pow ((v + 40.0f) / 43.0f, 1.6f); };
    s.marks = { { -20.0f, "20" }, { -10.0f, "10" }, { -6.0f, "6" }, { -3.0f, "3" }, { 0.0f, "0" }, { 3.0f, "+3" } };
    for (float m : { -30.0f, -25.0f, -15.0f, -8.0f, -5.0f, -4.0f, -2.0f, -1.0f, 1.0f, 2.0f })
        s.minorMarks.push_back (m);
    s.redFrom = 0.0f;
    s.title = title;
    s.attackMs = 4.0f;
    s.releaseMs = 650.0f;
    s.peakHold = true;
    return s;
}
} // namespace

OutputPanel::OutputPanel (KonveyerProcessor& p)
    : proc (p), left (outputScale ("L")), right (outputScale ("R")),
      integrated ("LOUDNESS INTEGRATED", cyr ("ГРОМКОСТЬ ИНТЕГР."), "LUFS", 3, true),
      shortTerm ("SHORT TERM", cyr ("КРАТКОВР."), "", 3, true),
      momentary ("MOMENTARY", cyr ("МГНОВЕН."), "", 3, true),
      truePeak ("TRUE PEAK MAX", cyr ("ИСТИННЫЙ ПИК"), "dBTP", 3, true),
      dither ("DITHER", cyr ("ДИЗЕРИНГ"), { "OFF", "24", "16" }, 120.0f)
{
    for (juce::Component* c : std::initializer_list<juce::Component*> { &left, &right, &integrated, &shortTerm, &momentary, &truePeak, &reset, &dither })
        addAndMakeVisible (c);
    dither.attach (proc.apvts, params::dither);
    reset.setTooltip ("Reset integrated loudness and true-peak max");
    reset.onClick = [this] { proc.engine().resetLoudness(); };
}

void OutputPanel::refresh (const MeterFrame& f)
{
    left.setTarget (f.outL);
    right.setTarget (f.outR);
    left.tick (f.dt);
    right.tick (f.dt);
    integrated.setValue (f.lufsI, f.lufsI > -70.0f);
    shortTerm.setValue (f.lufsS, f.lufsS > -70.0f);
    momentary.setValue (f.lufsM, f.lufsM > -70.0f);
    truePeak.setValue (f.tpMax, f.tpMax > -70.0f);
    const auto c = f.tpMax > proc.apvts.getRawParameterValue (params::limCeil)->load() + 0.05f ? juce::Colour (0xffff7a5a) : col::cream;
    if (truePeak.titleColour != c)
    {
        truePeak.titleColour = c;
        truePeak.repaint();
    }
}

void OutputPanel::resized()
{
    auto r = getLocalBounds().toFloat().reduced (12.0f, 8.0f);
    r.removeFromTop (34.0f);
    auto meters = r.removeFromTop (112.0f);
    left.setBounds (meters.removeFromLeft (meters.getWidth() * 0.5f).reduced (3.0f, 0.0f).toNearestInt());
    right.setBounds (meters.reduced (3.0f, 0.0f).toNearestInt());
    r.removeFromTop (12.0f);
    integrated.setBounds (r.removeFromTop (100.0f).toNearestInt());
    r.removeFromTop (8.0f);
    auto row = r.removeFromTop (82.0f);
    shortTerm.setBounds (row.removeFromLeft (row.getWidth() * 0.5f).reduced (2.0f, 0.0f).toNearestInt());
    momentary.setBounds (row.reduced (2.0f, 0.0f).toNearestInt());
    r.removeFromTop (8.0f);
    auto row2 = r.removeFromTop (82.0f);
    truePeak.setBounds (row2.removeFromLeft (row2.getWidth() * 0.62f).toNearestInt());
    reset.setBounds (row2.withSizeKeepingCentre (row2.getWidth() - 14.0f, 34.0f).translated (0.0f, 10.0f).toNearestInt());
    r.removeFromTop (4.0f);
    dither.setBounds (r.toNearestInt());
}

void OutputPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (12.0f, 8.0f);
    auto t = r.removeFromTop (30.0f);
    engraved (g, "OUTPUT", t.withHeight (17.0f), label (15.0f, 0.2f));
    engraved (g, cyr ("КОНТРОЛЬ ВЫХОДА"), t.withTrimmedTop (17.0f), light (10.5f, 0.15f), juce::Justification::centred, col::cream.withAlpha (0.7f));
}
} // namespace ui
