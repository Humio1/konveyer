#include "Panels.h"

namespace ui
{
namespace
{
NeedleMeter::Scale grScale (float maxDb, const juce::String& title)
{
    NeedleMeter::Scale s;
    s.minValue = 0.0f;
    s.maxValue = maxDb;
    // reduction grows to the left, like a classic gain-reduction meter
    s.position = [maxDb] (float v) { return 1.0f - std::log1p (v) / std::log1p (maxDb); };
    if (maxDb >= 20.0f)
        s.marks = { { 0.0f, "0" }, { 1.0f, "1" }, { 2.0f, "2" }, { 3.0f, "3" }, { 5.0f, "5" }, { 7.0f, "7" }, { 10.0f, "10" }, { 15.0f, "15" }, { 20.0f, "20" } };
    else
        s.marks = { { 0.0f, "0" }, { 1.0f, "1" }, { 2.0f, "2" }, { 4.0f, "4" }, { 6.0f, "6" }, { 9.0f, "9" }, { 12.0f, "12" } };
    for (float m = 0.5f; m < maxDb; m += m < 3.0f ? 0.5f : 1.0f)
        s.minorMarks.push_back (m);
    s.redFrom = maxDb >= 20.0f ? 10.0f : 6.0f;
    s.redBelow = true;
    s.title = title;
    s.attackMs = 6.0f;
    s.releaseMs = 260.0f;
    return s;
}

float rawValue (KonveyerProcessor& p, const juce::String& id) { return p.apvts.getRawParameterValue (id)->load(); }

void setParam (KonveyerProcessor& p, const juce::String& id, float v, int gesture)
{
    if (auto* prm = p.apvts.getParameter (id))
    {
        if (gesture == 1) prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        if (gesture == 2) prm->endChangeGesture();
    }
}
} // namespace

//==============================================================================
void TransferScreen::refresh (const MeterFrame& f)
{
    level = f.compLevel;
    gr = f.compGr;
    repaint();
}

void TransferScreen::paintStatic (juce::Graphics& g, juce::Rectangle<float> s)
{
    const float lo = -60.0f, hi = 0.0f;
    const auto a = s.reduced (14.0f);
    auto px = [&] (float db) { return juce::jmap (db, lo, hi, a.getX(), a.getRight()); };
    auto py = [&] (float db) { return juce::jmap (db, lo, hi, a.getBottom(), a.getY()); };

    g.setFont (mono (10.0f));
    for (float d = lo; d <= hi + 0.1f; d += 12.0f)
    {
        g.setColour (col::grid.withAlpha (d > hi - 0.1f ? 1.0f : 0.8f));
        g.drawVerticalLine (juce::roundToInt (px (d)), a.getY(), a.getBottom());
        g.drawHorizontalLine (juce::roundToInt (py (d)), a.getX(), a.getRight());
        g.setColour (col::phosphorDim);
        if (d > lo)
            g.drawText (juce::String ((int) d), juce::Rectangle<float> (px (d) - 16.0f, a.getBottom() - 13.0f, 32.0f, 12.0f), juce::Justification::centred, false);
    }
    juce::Path unity;
    unity.startNewSubPath (px (lo), py (lo));
    unity.lineTo (px (hi), py (hi));
    juce::Path dashed;
    const float dashes[] = { 4.0f, 4.0f };
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, unity, dashes, 2);
    g.setColour (col::phosphorDim.withAlpha (0.6f));
    g.fillPath (dashed);
    g.setFont (mono (10.0f));
    g.setColour (col::phosphorDim);
    g.drawText ("OUT", a.withHeight (14.0f).translated (4.0f, 2.0f), juce::Justification::centredLeft, false);
    g.drawText ("IN", a.withTop (a.getBottom() - 28.0f).withHeight (14.0f), juce::Justification::centredRight, false);
}

void TransferScreen::paintScreen (juce::Graphics& g, juce::Rectangle<float> s)
{
    const float lo = -60.0f, hi = 0.0f;
    const auto a = s.reduced (14.0f);
    auto px = [&] (float db) { return juce::jmap (db, lo, hi, a.getX(), a.getRight()); };
    auto py = [&] (float db) { return juce::jmap (db, lo, hi, a.getBottom(), a.getY()); };

    const float T = rawValue (proc, params::compThr), R = rawValue (proc, params::compRatio);
    const float W = rawValue (proc, params::compKnee), M = rawValue (proc, params::compMakeup);

    g.setColour (col::amber.withAlpha (0.45f));
    g.drawVerticalLine (juce::roundToInt (px (T)), a.getY(), a.getBottom());

    juce::Path curve;
    for (int i = 0; i <= 120; ++i)
    {
        const float x = lo + (hi - lo) * (float) i / 120.0f;
        const float y = x + (float) kv::gainComputer (x, T, R, W) + M;
        const auto p = juce::Point<float> (px (x), juce::jlimit (a.getY() - 2.0f, a.getBottom(), py (y)));
        if (i == 0) curve.startNewSubPath (p); else curve.lineTo (p);
    }
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (a.toNearestInt());
        paintGlowPath (g, curve, col::phosphor, 1.4f);
    }

    if (level > lo)
    {
        const float lx = juce::jlimit (lo, hi, level);
        const float ly = juce::jlimit (lo, hi, lx - gr + M);
        const auto p = juce::Point<float> (px (lx), py (ly));
        g.setColour (col::amber.withAlpha (0.25f));
        g.fillEllipse (p.x - 9.0f, p.y - 9.0f, 18.0f, 18.0f);
        g.setColour (col::amber);
        g.fillEllipse (p.x - 3.5f, p.y - 3.5f, 7.0f, 7.0f);
    }
}

//==============================================================================
CompPanel::CompPanel (KonveyerProcessor& p)
    : StationPanel (p), transfer (p), meter (grScale (20.0f, "GR")),
      autoRel ("AUTO RELEASE", cyr ("АВТО ВОССТ."), col::amber, true),
      rms ("RMS DETECT", cyr ("СКЗ ДЕТЕКТОР"), col::amber, true),
      thr ("THRESHOLD", cyr ("ПОРОГ"), Knob::Size::Large),
      ratio ("RATIO", cyr ("СТЕПЕНЬ"), Knob::Size::Large),
      atk ("ATTACK", cyr ("АТАКА"), Knob::Size::Large),
      rel ("RELEASE", cyr ("ВОССТАНОВЛЕНИЕ"), Knob::Size::Large),
      knee ("KNEE", cyr ("ИЗЛОМ"), Knob::Size::Large),
      makeup ("MAKEUP", cyr ("КОМПЕНСАЦИЯ"), Knob::Size::Large, true),
      mix ("MIX", cyr ("СМЕСЬ"), Knob::Size::Medium),
      hpf ("SIDECHAIN HPF", cyr ("ФВЧ ДЕТЕКТОРА"), Knob::Size::Medium)
{
    for (juce::Component* c : std::initializer_list<juce::Component*> { &transfer, &meter, &autoRel, &rms, &thr, &ratio, &atk, &rel, &knee, &makeup, &mix, &hpf })
        addAndMakeVisible (c);
    thr.attach (proc.apvts, params::compThr);
    ratio.attach (proc.apvts, params::compRatio);
    atk.attach (proc.apvts, params::compAtk);
    rel.attach (proc.apvts, params::compRel);
    knee.attach (proc.apvts, params::compKnee);
    makeup.attach (proc.apvts, params::compMakeup);
    mix.attach (proc.apvts, params::compMix);
    hpf.attach (proc.apvts, params::compHpf);
    autoRel.attach (proc.apvts, params::compAuto);
    rms.attach (proc.apvts, params::compRms);
}

void CompPanel::refresh (const MeterFrame& f)
{
    meter.setTarget (f.compGr);
    meter.tick (f.dt);
    transfer.refresh (f);
}

juce::String CompPanel::hint() const
{
    return "Bus compressor for glue" + dot() + "the dot on the curve shows the level right now" + dot() + "sidechain HPF keeps the bass from pumping";
}

void CompPanel::resized()
{
    auto r = getLocalBounds().toFloat().reduced (12.0f, 10.0f);
    auto top = r.removeFromTop (268.0f);
    transfer.setBounds (top.removeFromLeft (268.0f).toNearestInt());
    top.removeFromLeft (14.0f);
    secSwitches = top.removeFromRight (250.0f);
    top.removeFromRight (14.0f);
    meter.setBounds (top.reduced (0.0f, 8.0f).toNearestInt());

    auto s = secSwitches.reduced (8.0f, 6.0f);
    s.removeFromTop (14.0f);
    auto levers = s.removeFromTop (96.0f);
    autoRel.setBounds (levers.removeFromLeft (levers.getWidth() * 0.5f).toNearestInt());
    rms.setBounds (levers.toNearestInt());
    s.removeFromTop (6.0f);
    mix.setBounds (s.removeFromLeft (s.getWidth() * 0.5f).toNearestInt());
    hpf.setBounds (s.toNearestInt());

    r.removeFromTop (10.0f);
    secKnobs = r;
    auto k = secKnobs.reduced (8.0f, 6.0f);
    k.removeFromTop (16.0f);
    const float w = k.getWidth() / 6.0f;
    for (auto* knob : { &thr, &ratio, &atk, &rel, &knee, &makeup })
        knob->setBounds (k.removeFromLeft (w).reduced (6.0f, 0.0f).toNearestInt());
}

void CompPanel::paint (juce::Graphics& g)
{
    paintSection (g, secSwitches, "DETECTOR", cyr ("ДЕТЕКТОР"));
    paintSection (g, secKnobs, "BUS COMPRESSOR", cyr ("КОМПРЕССОР ШИНЫ"));
}

//==============================================================================
MbcScreen::MbcScreen (KonveyerProcessor& p) : CrtScreen (false), proc (p)
{
    bezelLabel = cyr ("МНОГОПОЛОСНЫЙ  ·  4 ПОЛОСЫ");
}

float MbcScreen::xover (int k) const { return rawValue (proc, params::mbcX (k)); }

int MbcScreen::hitXover (float x) const
{
    const auto s = screenArea();
    for (int k = 0; k < 3; ++k)
        if (std::abs (axis.x (xover (k), s) - x) < 7.0f)
            return k;
    return -1;
}

void MbcScreen::refresh (const MeterFrame& f)
{
    spec.pull (proc.engine().tapMbc, proc.currentSampleRate());
    for (size_t b = 0; b < 4; ++b)
        gr[b] = f.mbcGr[b];
    repaint();
}

void MbcScreen::paintStatic (juce::Graphics& g, juce::Rectangle<float> s) { axis.paintGrid (g, s, true); }

void MbcScreen::paintScreen (juce::Graphics& g, juce::Rectangle<float> s)
{

    auto filled = spec.path (s, axis.fMin, axis.fMax, 6.0f, -84.0f, true);
    juce::ColourGradient fillGrad (col::phosphor.withAlpha (0.20f), s.getX(), s.getY() + s.getHeight() * 0.3f,
                                   col::phosphor.withAlpha (0.03f), s.getX(), s.getBottom(), false);
    g.setGradientFill (fillGrad);
    g.fillPath (filled);
    g.setColour (col::phosphor.withAlpha (0.4f));
    g.strokePath (spec.path (s, axis.fMin, axis.fMax, 6.0f, -84.0f, false), juce::PathStrokeType (1.0f));

    const float edges[5] = { s.getX(), axis.x (xover (0), s), axis.x (xover (1), s), axis.x (xover (2), s), s.getRight() };
    const char* names[4] = { "LOW", "LOW MID", "HIGH MID", "HIGH" };
    bool anySolo = false;
    for (int b = 0; b < 4; ++b)
        anySolo = anySolo || rawValue (proc, params::mbc (b, "solo")) > 0.5f;

    for (int b = 0; b < 4; ++b)
    {
        const auto band = juce::Rectangle<float> (edges[b], s.getY(), edges[b + 1] - edges[b], s.getHeight());
        const bool solo = rawValue (proc, params::mbc (b, "solo")) > 0.5f;
        const bool byp = rawValue (proc, params::mbc (b, "byp")) > 0.5f;
        if ((anySolo && ! solo) || byp)
        {
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRect (band);
        }
        // gain reduction hanging from the top: 1 dB = 7 px
        const float h = std::min (band.getHeight() * 0.8f, gr[(size_t) b] * 7.0f);
        if (h > 0.5f)
        {
            auto bar = band.reduced (band.getWidth() * 0.18f, 0.0f).withHeight (h).translated (0.0f, 24.0f);
            juce::ColourGradient gg (col::amber.withAlpha (0.55f), bar.getX(), bar.getY(), col::amber.withAlpha (0.15f), bar.getX(), bar.getBottom(), false);
            g.setGradientFill (gg);
            g.fillRect (bar);
            g.setColour (col::amber);
            g.drawHorizontalLine (juce::roundToInt (bar.getBottom()), bar.getX(), bar.getRight());
        }
        g.setFont (label (11.0f, 0.15f));
        g.setColour (col::phosphor.withAlpha (solo ? 1.0f : 0.7f));
        g.drawText (juce::String (names[b]) + (solo ? "  [S]" : byp ? "  [BYP]" : ""), band.withHeight (22.0f).translated (0.0f, 4.0f), juce::Justification::centred, false);
    }

    for (int k = 0; k < 3; ++k)
    {
        const float x = axis.x (xover (k), s);
        const bool hot = k == dragging || k == hover;
        juce::Path line;
        line.startNewSubPath (x, s.getY());
        line.lineTo (x, s.getBottom());
        paintGlowPath (g, line, hot ? col::amber : col::phosphor, hot ? 1.3f : 0.9f, hot ? 1.0f : 0.5f);
        auto tag = juce::Rectangle<float> (64.0f, 16.0f).withCentre ({ x, s.getBottom() - 30.0f });
        g.setColour (juce::Colour (0xd0031008));
        g.fillRoundedRectangle (tag, 2.0f);
        g.setColour (hot ? col::amber : col::phosphor.withAlpha (0.85f));
        g.setFont (mono (10.5f));
        g.drawText (shortFreq (xover (k)) + "Hz", tag, juce::Justification::centred, false);
    }
}

void MbcScreen::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitXover (e.position.x);
    if (h != hover)
    {
        hover = h;
        repaint();
    }
    setMouseCursor (h >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void MbcScreen::mouseDown (const juce::MouseEvent& e)
{
    dragging = hitXover (e.position.x);
    if (dragging >= 0)
        setParam (proc, params::mbcX (dragging), xover (dragging), 1);
}

void MbcScreen::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;
    float f = axis.f (e.position.x, screenArea());
    if (dragging > 0) f = std::max (f, xover (dragging - 1) * 1.3f);
    if (dragging < 2) f = std::min (f, xover (dragging + 1) / 1.3f);
    setParam (proc, params::mbcX (dragging), f, 0);
    repaint();
}

void MbcScreen::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        setParam (proc, params::mbcX (dragging), xover (dragging), 2);
    dragging = -1;
}

//==============================================================================
MbcPanel::MbcPanel (KonveyerProcessor& p) : StationPanel (p), screen (p)
{
    addAndMakeVisible (screen);
    for (int b = 0; b < 4; ++b)
    {
        auto& c = cols[(size_t) b];
        c.meter = std::make_unique<NeedleMeter> (grScale (12.0f, "GR"));
        c.thr = std::make_unique<Knob> ("THRESHOLD", cyr ("ПОРОГ"), Knob::Size::Medium);
        c.ratio = std::make_unique<Knob> ("RATIO", cyr ("СТЕПЕНЬ"), Knob::Size::Medium);
        c.atk = std::make_unique<Knob> ("ATTACK", cyr ("АТАКА"), Knob::Size::Small);
        c.rel = std::make_unique<Knob> ("RELEASE", cyr ("ВОССТ."), Knob::Size::Small);
        c.gain = std::make_unique<Knob> ("GAIN", cyr ("УСИЛ."), Knob::Size::Small, true);
        c.solo = std::make_unique<PushButton> ("SOLO", col::amber, true);
        c.bypass = std::make_unique<PushButton> ("BYPASS", juce::Colour (0xffff5a3c), true);
        for (juce::Component* comp : std::initializer_list<juce::Component*> { c.meter.get(), c.thr.get(), c.ratio.get(), c.atk.get(), c.rel.get(), c.gain.get(), c.solo.get(), c.bypass.get() })
            addAndMakeVisible (comp);
        c.thr->attach (proc.apvts, params::mbc (b, "thr"));
        c.ratio->attach (proc.apvts, params::mbc (b, "ratio"));
        c.atk->attach (proc.apvts, params::mbc (b, "atk"));
        c.rel->attach (proc.apvts, params::mbc (b, "rel"));
        c.gain->attach (proc.apvts, params::mbc (b, "gain"));
        c.solo->attach (proc.apvts, params::mbc (b, "solo"));
        c.bypass->attach (proc.apvts, params::mbc (b, "byp"));
    }
}

void MbcPanel::refresh (const MeterFrame& f)
{
    screen.refresh (f);
    for (size_t b = 0; b < 4; ++b)
    {
        cols[b].meter->setTarget (f.mbcGr[b]);
        cols[b].meter->tick (f.dt);
    }
    repaint();
}

juce::String MbcPanel::hint() const
{
    return "Drag the crossover lines on the screen" + dot() + "the amber bars show each band's gain reduction" + dot() + "solo a band to hear it alone";
}

void MbcPanel::resized()
{
    auto r = getLocalBounds().toFloat().reduced (12.0f, 10.0f);
    screen.setBounds (r.removeFromTop (212.0f).toNearestInt());
    r.removeFromTop (10.0f);
    const float w = (r.getWidth() - 3.0f * 8.0f) / 4.0f;
    for (size_t b = 0; b < 4; ++b)
    {
        auto& c = cols[b];
        c.area = r.withWidth (w).translated ((w + 8.0f) * (float) b, 0.0f);
        auto a = c.area.reduced (8.0f, 6.0f);
        auto head = a.removeFromTop (24.0f);
        c.bypass->setBounds (head.removeFromRight (62.0f).toNearestInt());
        head.removeFromRight (4.0f);
        c.solo->setBounds (head.removeFromRight (50.0f).toNearestInt());
        a.removeFromTop (6.0f);
        c.meter->setBounds (a.removeFromTop (74.0f).reduced (8.0f, 0.0f).toNearestInt());
        a.removeFromTop (4.0f);
        auto row1 = a.removeFromTop (a.getHeight() * 0.54f);
        c.thr->setBounds (row1.removeFromLeft (row1.getWidth() * 0.5f).toNearestInt());
        c.ratio->setBounds (row1.toNearestInt());
        const float w3 = a.getWidth() / 3.0f;
        c.atk->setBounds (a.removeFromLeft (w3).toNearestInt());
        c.rel->setBounds (a.removeFromLeft (w3).toNearestInt());
        c.gain->setBounds (a.toNearestInt());
    }
}

void MbcPanel::paint (juce::Graphics& g)
{
    const char* en[4] = { "LOW", "LOW MID", "HIGH MID", "HIGH" };
    const char* ru[4] = { "НЧ", "НСЧ", "ВСЧ", "ВЧ" };
    for (size_t b = 0; b < 4; ++b)
    {
        const auto& c = cols[b];
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillRoundedRectangle (c.area, 5.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (c.area, 5.0f, 1.0f);
        const float lo = b == 0 ? 20.0f : rawValue (proc, params::mbcX ((int) b - 1));
        const float hi = b == 3 ? 20000.0f : rawValue (proc, params::mbcX ((int) b));
        auto head = c.area.reduced (10.0f, 6.0f).withHeight (24.0f);
        engraved (g, en[b], head.withHeight (14.0f), label (12.5f, 0.12f), juce::Justification::centredLeft);
        engraved (g, cyr (ru[b]) + "  " + shortFreq (lo) + "-" + shortFreq (hi) + " Hz", head.withTrimmedTop (13.0f), light (10.0f, 0.08f),
                  juce::Justification::centredLeft, col::cream.withAlpha (0.65f));
    }
}
} // namespace ui
