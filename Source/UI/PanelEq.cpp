#include "Panels.h"

namespace ui
{
void StationPanel::paint (juce::Graphics&) {}

void StationPanel::paintSection (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& en, const juce::String& ru) const
{
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawLine (r.getX() + 5.0f, r.getBottom() + 1.0f, r.getRight() - 5.0f, r.getBottom() + 1.0f, 1.0f);

    auto t = r.reduced (8.0f, 4.0f).removeFromTop (14.0f);
    g.setFont (label (11.0f, 0.14f));
    juce::GlyphArrangement ga;
    ga.addLineOfText (label (11.0f, 0.14f), en, 0.0f, 0.0f);
    const float w = ga.getBoundingBox (0, -1, true).getWidth();
    engraved (g, en, t, label (11.0f, 0.14f), juce::Justification::centredLeft, col::cream.withAlpha (0.85f));
    engraved (g, ru, t.withTrimmedLeft (w + 8.0f), light (10.0f, 0.12f), juce::Justification::centredLeft, col::cream.withAlpha (0.55f));
}

//==============================================================================
EqScreen::EqScreen (KonveyerProcessor& p) : CrtScreen (false), proc (p)
{
    bezelLabel = cyr ("ЭКВАЛАЙЗЕР  ·  12 ПОЛОС");
    analyserMode = (int) proc.uiState().getProperty ("eqAnalyser", 2);
    range = (float) proc.uiState().getProperty ("eqRange", 12.0f);
}

float EqScreen::raw (int b, const char* field) const
{
    if (! juce::isPositiveAndBelow (b, kv::kEqBands))
        return 0.0f;
    return proc.apvts.getRawParameterValue (params::eq (b, field))->load();
}

EqScreen::BandView EqScreen::band (int b) const
{
    BandView v;
    v.on = raw (b, "on") > 0.5f;
    v.type = (kv::EqType) juce::jlimit (0, kv::kEqTypeCount - 1, (int) raw (b, "type"));
    v.freq = raw (b, "freq");
    v.gain = raw (b, "gain");
    v.q = raw (b, "q");
    v.slope = (int) raw (b, "slope");
    v.place = (kv::Placement) juce::jlimit (0, kv::kPlacementCount - 1, (int) raw (b, "place"));
    v.dyn = raw (b, "dyn") > 0.5f;
    v.thr = raw (b, "thr");
    v.dynRange = raw (b, "range");
    return v;
}

void EqScreen::setValue (int b, const char* field, float v)
{
    if (auto* p = proc.apvts.getParameter (params::eq (b, field)))
        p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void EqScreen::setComplete (int b, const char* field, float v)
{
    if (auto* p = proc.apvts.getParameter (params::eq (b, field)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    }
}

void EqScreen::gesture (int b, const char* field, bool begin)
{
    if (auto* p = proc.apvts.getParameter (params::eq (b, field)))
    {
        if (begin)
            p->beginChangeGesture();
        else
            p->endChangeGesture();
    }
}

float EqScreen::xOf (float f) const { return axis.x (f, screenArea()); }
float EqScreen::fOf (float x) const { return axis.f (x, screenArea()); }

float EqScreen::yOf (float db) const
{
    const auto s = screenArea();
    return s.getCentreY() - db / range * (s.getHeight() * 0.5f - 16.0f);
}

float EqScreen::dbOf (float y) const
{
    const auto s = screenArea();
    return (s.getCentreY() - y) / (s.getHeight() * 0.5f - 16.0f) * range;
}

juce::Point<float> EqScreen::nodePos (int b) const
{
    const auto v = band (b);
    const float x = xOf (juce::jlimit (axis.fMin, axis.fMax, v.freq));
    const float db = kv::typeHasGain (v.type) ? juce::jlimit (-range, range, v.gain) : 0.0f;
    return { x, yOf (db) };
}

int EqScreen::hit (juce::Point<float> p) const
{
    int best = -1;
    float bestD = 12.0f;
    for (int b = 0; b < kv::kEqBands; ++b)
    {
        if (raw (b, "on") < 0.5f)
            continue;
        const float d = nodePos (b).getDistanceFrom (p);
        if (d < bestD || (b == selected && d < 12.0f && d <= bestD + 2.0f))
        {
            bestD = d;
            best = b;
        }
    }
    return best;
}

void EqScreen::setSelected (int b)
{
    if (b == selected)
        return;
    selected = b;
    repaint();
    if (onSelectionChanged)
        onSelectionChanged (b);
}

void EqScreen::refresh (const MeterFrame& f)
{
    const double sr = proc.currentSampleRate();
    if (analyserMode == 1 || analyserMode == 3)
        pre.pull (proc.engine().tapEqPre, sr);
    if (analyserMode >= 2)
        post.pull (proc.engine().tapEqPost, sr);
    dyn = f.eqDyn;
    repaint();
}

void EqScreen::paintStatic (juce::Graphics& g, juce::Rectangle<float> s)
{
    axis.paintGrid (g, s, true);

    // dB grid
    const float step = range <= 6.0f ? 2.0f : (range <= 12.0f ? 3.0f : (range <= 18.0f ? 6.0f : 10.0f));
    g.setFont (mono (10.5f));
    for (float d = -range; d <= range + 0.01f; d += step)
    {
        const float y = yOf (d);
        g.setColour (std::abs (d) < 0.01f ? col::grid.brighter (1.0f) : col::grid.withAlpha (0.8f));
        g.drawHorizontalLine (juce::roundToInt (y), s.getX(), s.getRight());
        if (std::abs (d) > 0.01f && std::abs (std::abs (d) - range) > 0.01f)
        {
            g.setColour (col::phosphorDim.withAlpha (0.85f));
            g.drawText ((d > 0 ? "+" : "") + juce::String ((int) d), juce::Rectangle<float> (s.getX() + 6.0f, y - 8.0f, 30.0f, 16.0f), juce::Justification::centredLeft, false);
        }
    }
}

void EqScreen::paintScreen (juce::Graphics& g, juce::Rectangle<float> s)
{

    // analyser (tilted spectrum: 0 dB at the top, -84 dB at the bottom)
    const float aTop = 6.0f, aBottom = -84.0f;
    if (analyserMode == 1 || analyserMode == 3)
    {
        g.setColour (col::phosphorDim.withAlpha (0.55f));
        g.strokePath (pre.path (s, axis.fMin, axis.fMax, aTop, aBottom, false), juce::PathStrokeType (1.0f));
    }
    if (analyserMode >= 2)
    {
        auto filled = post.path (s, axis.fMin, axis.fMax, aTop, aBottom, true);
        juce::ColourGradient fillGrad (col::phosphor.withAlpha (0.22f), s.getX(), s.getY() + s.getHeight() * 0.3f,
                                       col::phosphor.withAlpha (0.03f), s.getX(), s.getBottom(), false);
        g.setGradientFill (fillGrad);
        g.fillPath (filled);
        g.setColour (col::phosphor.withAlpha (0.35f));
        g.strokePath (post.path (s, axis.fMin, axis.fMax, aTop, aBottom, false), juce::PathStrokeType (1.0f));
    }

    // curves
    const double fs = proc.currentSampleRate();
    std::array<BandView, kv::kEqBands> views;
    std::array<kv::BandDesign, kv::kEqBands> designs;
    bool anyAlt = false, anyOn = false;
    for (int b = 0; b < kv::kEqBands; ++b)
    {
        views[(size_t) b] = band (b);
        const auto& v = views[(size_t) b];
        if (! v.on)
            continue;
        anyOn = true;
        const float live = v.gain + ((v.dyn && kv::typeSupportsDynamics (v.type)) ? dyn[(size_t) b] : 0.0f);
        designs[(size_t) b] = kv::designBand (v.type, v.freq, live, v.q, v.slope, fs);
        anyAlt = anyAlt || v.place == kv::Placement::Side || v.place == kv::Placement::Right;
    }

    juce::Path main, alt, own;
    const float stepX = 2.0f;
    bool first = true;
    const auto& selView = views[(size_t) juce::jlimit (0, kv::kEqBands - 1, selected)];
    for (float x = s.getX(); x <= s.getRight() + 0.01f; x += stepX)
    {
        const double f = std::min ((double) fOf (x), fs * 0.4999);
        const double w = kv::kTwoPi * f / fs;
        double dbMain = 0.0, dbAlt = 0.0, dbOwn = 0.0;
        for (int b = 0; b < kv::kEqBands; ++b)
        {
            const auto& v = views[(size_t) b];
            if (! v.on)
                continue;
            const double m = 20.0 * std::log10 (std::max (1.0e-9, kv::bandMagnitude (designs[(size_t) b], w)));
            if (v.place == kv::Placement::Stereo || v.place == kv::Placement::Mid || v.place == kv::Placement::Left)
                dbMain += m;
            if (v.place == kv::Placement::Stereo || v.place == kv::Placement::Side || v.place == kv::Placement::Right)
                dbAlt += m;
            if (b == selected)
                dbOwn = m;
        }
        const float yM = juce::jlimit (s.getY() - 4.0f, s.getBottom() + 4.0f, yOf ((float) dbMain));
        const float yA = juce::jlimit (s.getY() - 4.0f, s.getBottom() + 4.0f, yOf ((float) dbAlt));
        const float yO = juce::jlimit (s.getY() - 4.0f, s.getBottom() + 4.0f, yOf ((float) dbOwn));
        if (first)
        {
            main.startNewSubPath (x, yM);
            alt.startNewSubPath (x, yA);
            own.startNewSubPath (x, yOf (0.0f));
            own.lineTo (x, yO);
            first = false;
        }
        else
        {
            main.lineTo (x, yM);
            alt.lineTo (x, yA);
            own.lineTo (x, yO);
        }
    }
    own.lineTo (s.getRight(), yOf (0.0f));
    own.closeSubPath();

    if (selView.on)
    {
        const auto c = selView.place == kv::Placement::Stereo || selView.place == kv::Placement::Mid || selView.place == kv::Placement::Left ? col::phosphor : col::amber;
        g.setColour (c.withAlpha (0.13f));
        g.fillPath (own);
    }
    if (anyAlt)
    {
        paintGlowPath (g, alt, col::amber, 1.1f, 0.8f);
        g.setFont (mono (10.5f));
        g.setColour (col::amber.withAlpha (0.8f));
        g.drawText ("S/R", juce::Rectangle<float> (s.getX() + 40.0f, s.getY() + 6.0f, 40.0f, 14.0f), juce::Justification::centredLeft);
        g.setColour (col::phosphor.withAlpha (0.8f));
        g.drawText ("M/L", juce::Rectangle<float> (s.getX() + 8.0f, s.getY() + 6.0f, 40.0f, 14.0f), juce::Justification::centredLeft);
    }
    paintGlowPath (g, main, col::phosphor, 1.4f, 1.0f);

    // nodes
    for (int b = 0; b < kv::kEqBands; ++b)
    {
        const auto& v = views[(size_t) b];
        if (! v.on)
            continue;
        const auto p = nodePos (b);
        const bool sel = b == selected, hot = b == hover || b == dragging;
        const bool alternate = v.place == kv::Placement::Side || v.place == kv::Placement::Right;
        const auto c = v.place == kv::Placement::Stereo ? col::phosphor : (alternate ? col::amber : juce::Colour (0xffb8ffd0));

        if (v.dyn && kv::typeSupportsDynamics (v.type))
        {
            const float y0 = yOf (juce::jlimit (-range, range, v.gain));
            const float y1 = yOf (juce::jlimit (-range, range, v.gain + v.dynRange));
            const float yc = yOf (juce::jlimit (-range, range, v.gain + dyn[(size_t) b]));
            g.setColour (c.withAlpha (0.35f));
            g.drawLine (p.x, y0, p.x, y1, 3.0f);
            g.setColour (c.withAlpha (0.25f));
            g.drawLine (p.x - 6.0f, y1, p.x + 6.0f, y1, 1.0f);
            g.setColour (c);
            g.drawEllipse (p.x - 4.0f, yc - 4.0f, 8.0f, 8.0f, 1.4f);
        }

        const float r = sel ? 9.5f : 7.5f;
        if (sel || hot)
        {
            g.setColour (c.withAlpha (0.18f));
            g.fillEllipse (p.x - r * 2.0f, p.y - r * 2.0f, r * 4.0f, r * 4.0f);
        }
        g.setColour (juce::Colour (0xff031008));
        g.fillEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f);
        g.setColour (c.withAlpha (sel ? 1.0f : 0.8f));
        g.drawEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f, sel ? 2.0f : 1.3f);
        g.setFont (mono (sel ? 11.0f : 10.0f));
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (p.x - r, p.y - r, r * 2.0f, r * 2.0f), juce::Justification::centred, false);
        if (v.place != kv::Placement::Stereo)
        {
            const char* tag[] = { "", "M", "S", "L", "R" };
            g.setFont (mono (10.0f));
            g.drawText (tag[(int) v.place], juce::Rectangle<float> (p.x + r - 1.0f, p.y - r - 11.0f, 12.0f, 12.0f), juce::Justification::centred, false);
        }
    }

    // readout of the band under the mouse
    const int info = dragging >= 0 ? dragging : hover;
    if (info >= 0 && views[(size_t) info].on)
    {
        const auto& v = views[(size_t) info];
        const auto p = nodePos (info);
        juce::String t = shortFreq (v.freq) + "Hz";
        if (kv::typeHasGain (v.type))
            t << "   " << (v.gain > 0 ? "+" : "") << juce::String (v.gain, 1) << " dB";
        if (kv::typeIsCut (v.type))
            t << "   " << kv::slopeDbPerOct (v.slope) << " dB/oct";
        if (v.type != kv::EqType::TiltShelf)
            t << "   Q " << juce::String (v.q, 2);
        g.setFont (mono (11.5f));
        const float tw = (float) t.length() * 7.0f + 14.0f;
        auto box = juce::Rectangle<float> (tw, 18.0f).withCentre ({ p.x, p.y - 26.0f });
        box = box.constrainedWithin (s.reduced (4.0f));
        g.setColour (juce::Colour (0xe0031008));
        g.fillRoundedRectangle (box, 3.0f);
        g.setColour (col::phosphor.withAlpha (0.6f));
        g.drawRoundedRectangle (box, 3.0f, 0.8f);
        g.setColour (col::phosphor);
        g.drawText (t, box, juce::Justification::centred, false);
    }

    if (! anyOn)
    {
        g.setFont (label (15.0f, 0.2f));
        g.setColour (col::phosphorDim);
        g.drawText ("DOUBLE-CLICK TO ADD A BAND", s.withTrimmedBottom (40.0f), juce::Justification::centred, false);
        g.setFont (light (12.0f, 0.2f));
        g.drawText (cyr ("ДВОЙНОЙ ЩЕЛЧОК — НОВАЯ ПОЛОСА"), s.withTrimmedTop (10.0f), juce::Justification::centred, false);
    }

    // analyser / range switches
    rangeButton = juce::Rectangle<float> (s.getRight() - 74.0f, s.getY() + 8.0f, 64.0f, 17.0f);
    analyserButton = rangeButton.translated (-104.0f, 0.0f).withWidth (98.0f);
    const char* modes[] = { "SPEC  OFF", "SPEC  PRE", "SPEC  POST", "SPEC  BOTH" };
    g.setFont (mono (10.5f));
    for (auto [box, text] : { std::pair<juce::Rectangle<float>, juce::String> { analyserButton, modes[analyserMode] },
                              { rangeButton, juce::String::fromUTF8 ("\xc2\xb1") + juce::String ((int) range) + " dB" } })
    {
        g.setColour (juce::Colour (0xc0031008));
        g.fillRoundedRectangle (box, 2.0f);
        g.setColour (col::phosphorDim);
        g.drawRoundedRectangle (box, 2.0f, 0.8f);
        g.setColour (col::phosphor.withAlpha (0.85f));
        g.drawText (text, box, juce::Justification::centred, false);
    }
}

void EqScreen::mouseMove (const juce::MouseEvent& e)
{
    const int h = hit (e.position);
    if (h != hover)
    {
        hover = h;
        repaint();
    }
    setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::CrosshairCursor);
}

void EqScreen::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

void EqScreen::mouseDown (const juce::MouseEvent& e)
{
    if (analyserButton.contains (e.position))
    {
        analyserMode = (analyserMode + 1) % 4;
        proc.uiState().setProperty ("eqAnalyser", analyserMode, nullptr);
        pre.clear();
        post.clear();
        repaint();
        return;
    }
    if (rangeButton.contains (e.position))
    {
        const float ranges[] = { 6.0f, 12.0f, 18.0f, 30.0f };
        int i = 0;
        while (i < 4 && ranges[i] <= range + 0.01f)
            ++i;
        range = ranges[i % 4];
        proc.uiState().setProperty ("eqRange", range, nullptr);
        invalidateStatic();
        return;
    }

    const int b = hit (e.position);
    if (e.mods.isPopupMenu())
    {
        if (b >= 0)
            setSelected (b);
        showMenu (b);
        return;
    }
    if (b < 0)
        return;
    if (e.mods.isAltDown())
    {
        setComplete (b, "on", 0.0f);
        return;
    }
    setSelected (b);
    const auto v = band (b);
    dragging = b;
    dragStart = e.position;
    dragF = v.freq;
    dragG = v.gain;
    dragQ = v.q;
    gesture (b, "freq", true);
    gesture (b, kv::typeHasGain (v.type) ? "gain" : "q", true);
}

void EqScreen::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;
    const auto v = band (dragging);
    const float fine = e.mods.isShiftDown() ? 0.2f : 1.0f;
    const auto d = (e.position - dragStart) * fine;
    setValue (dragging, "freq", juce::jlimit (10.0f, 30000.0f, fOf (xOf (dragF) + d.x)));
    if (kv::typeHasGain (v.type))
        setValue (dragging, "gain", juce::jlimit (-30.0f, 30.0f, dbOf (yOf (juce::jlimit (-range, range, dragG)) + d.y)));
    else if (v.type != kv::EqType::TiltShelf)
        setValue (dragging, "q", juce::jlimit (0.1f, 40.0f, dragQ * std::pow (2.0f, -d.y / 40.0f)));
    repaint();
}

void EqScreen::mouseUp (const juce::MouseEvent&)
{
    if (dragging < 0)
        return;
    const auto v = band (dragging);
    gesture (dragging, "freq", false);
    gesture (dragging, kv::typeHasGain (v.type) ? "gain" : "q", false);
    dragging = -1;
}

void EqScreen::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (analyserButton.contains (e.position) || rangeButton.contains (e.position))
        return;
    const int b = hit (e.position);
    if (b >= 0)
        setComplete (b, "on", 0.0f);
    else if (screenArea().contains (e.position))
        addBandAt (e.position);
}

void EqScreen::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int b = hit (e.position);  // only over a band, so scrolling the host window never changes Q
    if (b < 0)
        return;
    const auto v = band (b);
    if (v.type == kv::EqType::TiltShelf)
        return;
    setComplete (b, "q", juce::jlimit (0.025f, 40.0f, v.q * std::pow (2.0f, w.deltaY * 1.5f)));
    repaint();
}

void EqScreen::addBandAt (juce::Point<float> p)
{
    int slot = -1;
    if (raw (selected, "on") < 0.5f)
        slot = selected;
    for (int b = 0; b < kv::kEqBands && slot < 0; ++b)
        if (raw (b, "on") < 0.5f)
            slot = b;
    if (slot < 0)
        return;

    const float f = juce::jlimit (20.0f, 20000.0f, fOf (p.x));
    const auto type = f < 30.0f ? kv::EqType::LowCut : (f > 15000.0f ? kv::EqType::HighCut : kv::EqType::Bell);
    const float gain = type == kv::EqType::Bell ? std::round (juce::jlimit (-range, range, dbOf (p.y)) * 10.0f) / 10.0f : 0.0f;
    setComplete (slot, "type", (float) type);
    setComplete (slot, "freq", f);
    setComplete (slot, "gain", gain);
    setComplete (slot, "q", type == kv::EqType::Bell ? 1.0f : 0.71f);
    setComplete (slot, "slope", 1.0f);
    setComplete (slot, "place", 0.0f);
    setComplete (slot, "dyn", 0.0f);
    setComplete (slot, "on", 1.0f);
    selected = -1;
    setSelected (slot);
}

void EqScreen::showMenu (int b)
{
    juce::PopupMenu m;
    if (b >= 0)
    {
        const auto v = band (b);
        juce::PopupMenu types, slopes, places;
        for (int t = 0; t < kv::kEqTypeCount; ++t)
            types.addItem (100 + t, params::eqTypeNames()[t], true, (int) v.type == t);
        for (int s = 0; s < kv::kSlopeCount; ++s)
            slopes.addItem (200 + s, params::slopeNames()[s], true, v.slope == s);
        for (int pl = 0; pl < kv::kPlacementCount; ++pl)
            places.addItem (300 + pl, params::placementNames()[pl], true, (int) v.place == pl);
        m.addSectionHeader ("BAND " + juce::String (b + 1));
        m.addSubMenu ("Type", types);
        if (kv::typeIsCut (v.type))
            m.addSubMenu ("Slope", slopes);
        m.addSubMenu ("Channel", places);
        m.addItem (400, "Dynamic", kv::typeSupportsDynamics (v.type), v.dyn);
        m.addItem (500, "Remove band");
        m.addSeparator();
    }
    juce::PopupMenu an, rg;
    const char* modes[] = { "Off", "Pre EQ", "Post EQ", "Pre and post" };
    for (int i = 0; i < 4; ++i)
        an.addItem (600 + i, modes[i], true, analyserMode == i);
    const float ranges[] = { 6.0f, 12.0f, 18.0f, 30.0f };
    for (int i = 0; i < 4; ++i)
        rg.addItem (700 + i, juce::String::fromUTF8 ("\xc2\xb1") + juce::String ((int) ranges[i]) + " dB", true, std::abs (range - ranges[i]) < 0.01f);
    m.addSubMenu ("Analyser", an);
    m.addSubMenu ("Display range", rg);

    juce::Component::SafePointer<EqScreen> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [safe, b] (int r)
    {
        if (safe == nullptr || r == 0)
            return;
        auto& s = *safe;
        if (r >= 100 && r < 100 + kv::kEqTypeCount) s.setComplete (b, "type", (float) (r - 100));
        else if (r >= 200 && r < 200 + kv::kSlopeCount) s.setComplete (b, "slope", (float) (r - 200));
        else if (r >= 300 && r < 300 + kv::kPlacementCount) s.setComplete (b, "place", (float) (r - 300));
        else if (r == 400) s.setComplete (b, "dyn", s.raw (b, "dyn") > 0.5f ? 0.0f : 1.0f);
        else if (r == 500) s.setComplete (b, "on", 0.0f);
        else if (r >= 600 && r < 604)
        {
            s.analyserMode = r - 600;
            s.proc.uiState().setProperty ("eqAnalyser", s.analyserMode, nullptr);
        }
        else if (r >= 700 && r < 704)
        {
            const float ranges2[] = { 6.0f, 12.0f, 18.0f, 30.0f };
            s.range = ranges2[r - 700];
            s.proc.uiState().setProperty ("eqRange", s.range, nullptr);
            s.invalidateStatic();
        }
        s.repaint();
    });
}

//==============================================================================
EqPanel::EqPanel (KonveyerProcessor& p)
    : StationPanel (p), screen (p),
      typeKeys (kv::kEqTypeCount, [] (juce::Graphics& g, int i, juce::Rectangle<float> face, juce::Colour ink)
      {
          g.setColour (ink);
          g.strokePath (filterIcon (i, face), juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
      }),
      freq ("FREQUENCY", cyr ("ЧАСТОТА"), Knob::Size::Large),
      gain ("GAIN", cyr ("УСИЛЕНИЕ"), Knob::Size::Large, true),
      q ("Q", cyr ("ДОБРОТНОСТЬ"), Knob::Size::Medium),
      slope ("SLOPE", cyr ("КРУТИЗНА"), { "6", "12", "18", "24", "36", "48" }, 250.0f),
      place ("CHANNEL", cyr ("КАНАЛ"), { "ST", "MID", "SIDE", "L", "R" }, 220.0f),
      dynOn ("DYNAMIC", cyr ("ДИНАМ."), col::amber, true),
      thr ("THRESHOLD", cyr ("ПОРОГ"), Knob::Size::Small),
      range ("RANGE", cyr ("ДИАПАЗОН"), Knob::Size::Small, true),
      atk ("ATTACK", cyr ("АТАКА"), Knob::Size::Small),
      rel ("RELEASE", cyr ("ВОССТ."), Knob::Size::Small),
      out ("EQ OUT", cyr ("ВЫХОД"), Knob::Size::Small, true)
{
    addAndMakeVisible (screen);
    screen.onSelectionChanged = [this] (int b) { selectBand (b); };

    for (int b = 0; b < kv::kEqBands; ++b)
    {
        auto btn = std::make_unique<PushButton> (juce::String (b + 1), col::amber, false);
        btn->onClick = [this, b] { selectBand (b); };
        addAndMakeVisible (*btn);
        bandButtons[(size_t) b] = std::move (btn);
    }
    typeKeys.tooltips = params::eqTypeNames();
    for (juce::Component* c : std::initializer_list<juce::Component*> { &bandOn, &typeKeys, &freq, &gain, &q, &slope, &place, &dynOn, &thr, &range, &atk, &rel, &out })
        addAndMakeVisible (c);

    atk.attach (proc.apvts, params::eqDynAtk);
    rel.attach (proc.apvts, params::eqDynRel);
    out.attach (proc.apvts, params::eqOut);

    selectBand (juce::jlimit (0, kv::kEqBands - 1, (int) proc.uiState().getProperty ("eqBand", 0)));
}

void EqPanel::selectBand (int b)
{
    band = juce::jlimit (0, kv::kEqBands - 1, b);
    proc.uiState().setProperty ("eqBand", band, nullptr);
    screen.setSelected (band);
    bandOn.attach (proc.apvts, params::eq (band, "on"));
    typeKeys.attach (proc.apvts, params::eq (band, "type"));
    freq.attach (proc.apvts, params::eq (band, "freq"));
    gain.attach (proc.apvts, params::eq (band, "gain"));
    q.attach (proc.apvts, params::eq (band, "q"));
    slope.attach (proc.apvts, params::eq (band, "slope"));
    place.attach (proc.apvts, params::eq (band, "place"));
    dynOn.attach (proc.apvts, params::eq (band, "dyn"));
    thr.attach (proc.apvts, params::eq (band, "thr"));
    range.attach (proc.apvts, params::eq (band, "range"));
    for (int i = 0; i < kv::kEqBands; ++i)
        bandButtons[(size_t) i]->setToggleState (i == band, juce::dontSendNotification);
    updateEnablement();
    repaint();
}

void EqPanel::updateEnablement()
{
    const auto type = (kv::EqType) (int) proc.apvts.getRawParameterValue (params::eq (band, "type"))->load();
    const bool dynamic = proc.apvts.getRawParameterValue (params::eq (band, "dyn"))->load() > 0.5f;
    auto enable = [] (juce::Component& c, bool on)
    {
        if (c.isEnabled() != on)
        {
            c.setEnabled (on);
            c.setAlpha (on ? 1.0f : 0.45f);
        }
    };
    enable (gain, kv::typeHasGain (type));
    enable (q, type != kv::EqType::TiltShelf);
    enable (slope, kv::typeIsCut (type));
    enable (dynOn, kv::typeSupportsDynamics (type));
    enable (thr, dynamic && kv::typeSupportsDynamics (type));
    enable (range, dynamic && kv::typeSupportsDynamics (type));
    for (auto* k : { &gain, &q, &thr, &range })
        k->slider.setEnabled (k->isEnabled());
}

void EqPanel::refresh (const MeterFrame& f)
{
    screen.refresh (f);
    for (int b = 0; b < kv::kEqBands; ++b)
    {
        const bool on = proc.apvts.getRawParameterValue (params::eq (b, "on"))->load() > 0.5f;
        auto& btn = *bandButtons[(size_t) b];
        if (btn.lampOn != on)
        {
            btn.lampOn = on;
            btn.repaint();
        }
    }
    updateEnablement();
    repaint (juce::Rectangle<float> (secFilter.getX(), secFilter.getBottom() - 38.0f, secFilter.getWidth(), 32.0f).toNearestInt());
}

juce::String EqPanel::hint() const
{
    return "Double-click the screen to add a band" + dot() + "drag to move" + dot() + "wheel for Q" + dot()
         + "right-click for type, channel and dynamics" + dot() + "double-click a band to remove it";
}

void EqPanel::resized()
{
    auto r = getLocalBounds().toFloat().reduced (12.0f, 10.0f);
    screen.setBounds (r.removeFromTop (298.0f).toNearestInt());
    r.removeFromTop (10.0f);

    secBand = r.removeFromLeft (182.0f);
    r.removeFromLeft (8.0f);
    secFilter = r.removeFromLeft (252.0f);
    r.removeFromLeft (8.0f);
    secShape = r.removeFromLeft (236.0f);
    r.removeFromLeft (8.0f);
    secDyn = r;

    // band selector and type keys
    {
        auto a = secBand.reduced (8.0f, 6.0f);
        a.removeFromTop (16.0f);
        for (int row = 0; row < 2; ++row)
        {
            auto line = a.removeFromTop (25.0f);
            const float w = line.getWidth() / 6.0f;
            for (int c = 0; c < 6; ++c)
                bandButtons[(size_t) (row * 6 + c)]->setBounds (line.withWidth (w).translated (w * (float) c, 0.0f).reduced (1.5f, 1.0f).toNearestInt());
        }
        a.removeFromTop (8.0f);
        typeKeys.setBounds (a.removeFromTop (42.0f).toNearestInt());
        a.removeFromTop (4.0f);
        bandOn.setBounds (a.toNearestInt());
    }
    // filter knobs, summary plate below
    {
        auto a = secFilter.reduced (4.0f, 6.0f);
        a.removeFromTop (14.0f);
        auto row = a.removeFromTop (136.0f);
        const float w = row.getWidth() / 3.0f;
        freq.setBounds (row.removeFromLeft (w).toNearestInt());
        gain.setBounds (row.removeFromLeft (w).toNearestInt());
        q.setBounds (row.reduced (2.0f, 8.0f).toNearestInt());
    }
    // slope / channel rotary switches side by side
    {
        auto a = secShape.reduced (4.0f, 6.0f);
        a.removeFromTop (14.0f);
        slope.setBounds (a.removeFromLeft (a.getWidth() * 0.5f).toNearestInt());
        place.setBounds (a.toNearestInt());
    }
    // dynamics
    {
        auto a = secDyn.reduced (4.0f, 6.0f);
        a.removeFromTop (14.0f);
        auto top = a.removeFromTop (a.getHeight() * 0.52f);
        const float w = top.getWidth() / 3.0f;
        dynOn.setBounds (top.removeFromLeft (w).toNearestInt());
        thr.setBounds (top.removeFromLeft (w).toNearestInt());
        range.setBounds (top.toNearestInt());
        const float w2 = a.getWidth() / 3.0f;
        atk.setBounds (a.removeFromLeft (w2).toNearestInt());
        rel.setBounds (a.removeFromLeft (w2).toNearestInt());
        out.setBounds (a.toNearestInt());
    }
}

void EqPanel::paint (juce::Graphics& g)
{
    paintSection (g, secBand, "BAND", cyr ("ПОЛОСА"));
    paintSection (g, secFilter, "FILTER", cyr ("ФИЛЬТР"));
    paintSection (g, secShape, "SHAPE", cyr ("ФОРМА"));
    paintSection (g, secDyn, "DYNAMIC EQ", cyr ("ДИНАМИЧЕСКИЙ"));

    // Dymo-style summary plate for the selected band
    const auto type = (int) proc.apvts.getRawParameterValue (params::eq (band, "type"))->load();
    const auto pl = (int) proc.apvts.getRawParameterValue (params::eq (band, "place"))->load();
    const bool on = proc.apvts.getRawParameterValue (params::eq (band, "on"))->load() > 0.5f;
    auto plate = juce::Rectangle<float> (secFilter.getX() + 10.0f, secFilter.getBottom() - 34.0f, secFilter.getWidth() - 20.0f, 24.0f);
    paintPlate (g, plate, col::cream, true);
    juce::String text = "BAND " + juce::String (band + 1) + dot() + params::eqTypeNames()[type].toUpperCase()
                      + dot() + params::placementNames()[pl].toUpperCase();
    if (! on)
        text = "BAND " + juce::String (band + 1) + dot() + "OFF";
    inked (g, text, plate, label (12.5f, 0.1f), juce::Justification::centred, on ? col::ink : col::red);
}
} // namespace ui
