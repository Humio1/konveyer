#include "Panels.h"
#include <cstring>

namespace ui
{
namespace
{
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
Vectorscope::Vectorscope (KonveyerProcessor& p) : CrtScreen (true), proc (p)
{
    bezelLabel = cyr ("ГОНИОМЕТР");
    pts.resize (4096);
}

void Vectorscope::refresh (const MeterFrame&)
{
    const auto s = screenArea();
    const int w = juce::jmax (8, (int) s.getWidth()), h = juce::jmax (8, (int) s.getHeight());
    if (trail.getWidth() != w || trail.getHeight() != h)
        trail = juce::Image (juce::Image::ARGB, w, h, true);

    trail.multiplyAllAlphas (0.72f);  // the old trace fades like phosphor persistence
    juce::Graphics g (trail);

    const auto written = proc.engine().tapScope.written();
    if (written != lastWritten && written > (uint64_t) pts.size())
    {
        const int n = juce::jmin ((int) pts.size(), (int) (written - lastWritten));
        lastWritten = written;
        proc.engine().tapScope.readLatest (pts.data(), n - (n % 2), 2);
        const float cx = (float) w * 0.5f, cy = (float) h * 0.5f, R = (float) std::min (w, h) * 0.46f;
        g.setColour (col::phosphor.withAlpha (0.30f));
        for (int i = 0; i + 1 < n - (n % 2); i += 2)
        {
            const float l = pts[(size_t) i], r = pts[(size_t) i + 1];
            float side = (l - r) * 0.70710678f, mid = (l + r) * 0.70710678f;
            const float mag = std::sqrt (side * side + mid * mid);
            if (mag > 1.0e-6f)
            {
                const float k = std::log10 (1.0f + 9.0f * std::min (mag, 1.5f)) / mag;  // log-polar: quiet material stays visible
                side *= k;
                mid *= k;
            }
            g.fillRect (cx + side * R - 0.75f, cy - mid * R - 0.75f, 1.5f, 1.5f);
        }
    }
    repaint();
}

void Vectorscope::paintScreen (juce::Graphics& g, juce::Rectangle<float> s)
{
    if (trail.isValid())
        g.drawImage (trail, s);
}

void Vectorscope::paintStatic (juce::Graphics& g, juce::Rectangle<float> s)
{
    const auto c = s.getCentre();
    const float R = std::min (s.getWidth(), s.getHeight()) * 0.46f;
    g.setColour (col::grid);
    for (int i = 1; i <= 4; ++i)
    {
        const float f = 0.25f * (float) i;
        g.drawEllipse (c.x - R * f, c.y - R * f, R * f * 2.0f, R * f * 2.0f, i == 4 ? 1.2f : 0.8f);
    }
    g.drawLine (c.x, c.y - R, c.x, c.y + R, 1.0f);
    g.drawLine (c.x - R, c.y, c.x + R, c.y, 1.0f);
    const float d = R * 0.7071f;
    g.setColour (col::grid.withAlpha (0.6f));
    g.drawLine (c.x - d, c.y - d, c.x + d, c.y + d, 0.8f);
    g.drawLine (c.x + d, c.y - d, c.x - d, c.y + d, 0.8f);

    g.setFont (mono (11.0f));
    g.setColour (col::phosphorDim);
    g.drawText ("M", juce::Rectangle<float> (c.x - 10.0f, c.y - R - 2.0f, 20.0f, 14.0f), juce::Justification::centred);
    g.drawText ("L", juce::Rectangle<float> (c.x - d - 16.0f, c.y - d - 16.0f, 14.0f, 14.0f), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (c.x + d + 2.0f, c.y - d - 16.0f, 14.0f, 14.0f), juce::Justification::centred);
    g.drawText ("+S", juce::Rectangle<float> (c.x - R - 2.0f, c.y - 16.0f, 22.0f, 14.0f), juce::Justification::centred);
    g.drawText ("-S", juce::Rectangle<float> (c.x + R - 20.0f, c.y - 16.0f, 22.0f, 14.0f), juce::Justification::centred);
}

//==============================================================================
WidthScreen::WidthScreen (KonveyerProcessor& p) : CrtScreen (false), proc (p)
{
    bezelLabel = cyr ("ШИРИНА СТЕРЕОБАЗЫ");
}

float WidthScreen::value (const juce::String& id) const { return rawValue (proc, id); }

int WidthScreen::hitTarget (juce::Point<float> p) const
{
    const auto s = screenArea();
    for (int k = 0; k < 2; ++k)
        if (std::abs (axis.x (value (params::imgX (k)), s) - p.x) < 7.0f)
            return k;
    if (! s.contains (p))
        return -1;
    const float x1 = axis.x (value (params::imgX (0)), s), x2 = axis.x (value (params::imgX (1)), s);
    return p.x < x1 ? 2 : (p.x < x2 ? 3 : 4);
}

void WidthScreen::paintStatic (juce::Graphics& g, juce::Rectangle<float> s)
{
    axis.paintGrid (g, s, true);
    g.setColour (col::grid.brighter (0.8f));
    g.drawHorizontalLine (juce::roundToInt (s.getCentreY()), s.getX(), s.getRight());
}

void WidthScreen::paintScreen (juce::Graphics& g, juce::Rectangle<float> s)
{
    const float cy = s.getCentreY(), half = s.getHeight() * 0.5f - 22.0f;

    const float edges[4] = { s.getX(), axis.x (value (params::imgX (0)), s), axis.x (value (params::imgX (1)), s), s.getRight() };
    const bool monoBass = value (params::imgMono) > 0.5f;
    const char* names[3] = { "LOW", "MID", "HIGH" };
    for (int b = 0; b < 3; ++b)
    {
        const float wPct = (b == 0 && monoBass) ? 0.0f : value (params::imgW (b));
        const float h = half * wPct / 200.0f;
        auto band = juce::Rectangle<float> (edges[b] + 6.0f, cy - h, edges[b + 1] - edges[b] - 12.0f, h * 2.0f);
        const bool hot = hover == b + 2 || dragging == b + 2;
        juce::ColourGradient gg (col::phosphor.withAlpha (hot ? 0.40f : 0.28f), band.getCentreX(), cy,
                                 col::phosphor.withAlpha (0.05f), band.getCentreX(), cy - half, false);
        g.setGradientFill (gg);
        g.fillRect (band);
        juce::Path outline;
        outline.addRectangle (band);
        paintGlowPath (g, outline, col::phosphor, 1.0f, hot ? 1.0f : 0.5f);
        // 100 % reference marks
        g.setColour (col::phosphorDim);
        const float h100 = half * 0.5f;
        g.drawHorizontalLine (juce::roundToInt (cy - h100), edges[b] + 2.0f, edges[b] + 10.0f);
        g.drawHorizontalLine (juce::roundToInt (cy + h100), edges[b] + 2.0f, edges[b] + 10.0f);

        g.setFont (label (11.0f, 0.12f));
        g.setColour (col::phosphor.withAlpha (0.8f));
        const juce::String t = juce::String (names[b]) + "  " + ((b == 0 && monoBass) ? juce::String ("MONO") : juce::String (juce::roundToInt (wPct)) + " %");
        g.drawText (t, juce::Rectangle<float> (edges[b], s.getY() + 6.0f, edges[b + 1] - edges[b], 16.0f), juce::Justification::centred, false);
    }
    for (int k = 0; k < 2; ++k)
    {
        const float x = edges[k + 1];
        const bool hot = hover == k || dragging == k;
        juce::Path line;
        line.startNewSubPath (x, s.getY());
        line.lineTo (x, s.getBottom());
        paintGlowPath (g, line, hot ? col::amber : col::phosphor, hot ? 1.3f : 0.9f, hot ? 1.0f : 0.5f);
        auto tag = juce::Rectangle<float> (64.0f, 16.0f).withCentre ({ x, s.getBottom() - 30.0f });
        g.setColour (juce::Colour (0xd0031008));
        g.fillRoundedRectangle (tag, 2.0f);
        g.setColour (hot ? col::amber : col::phosphor.withAlpha (0.85f));
        g.setFont (mono (10.5f));
        g.drawText (shortFreq (value (params::imgX (k))) + "Hz", tag, juce::Justification::centred, false);
    }
}

void WidthScreen::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitTarget (e.position);
    if (h != hover)
    {
        hover = h;
        repaint();
    }
    setMouseCursor (h >= 0 && h < 2 ? juce::MouseCursor::LeftRightResizeCursor : (h >= 2 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor));
}

void WidthScreen::mouseDown (const juce::MouseEvent& e)
{
    dragging = hitTarget (e.position);
    if (dragging >= 0 && dragging < 2)
        setParam (proc, params::imgX (dragging), value (params::imgX (dragging)), 1);
    else if (dragging >= 2)
        setParam (proc, params::imgW (dragging - 2), value (params::imgW (dragging - 2)), 1);
}

void WidthScreen::mouseDrag (const juce::MouseEvent& e)
{
    const auto s = screenArea();
    if (dragging >= 0 && dragging < 2)
    {
        float f = axis.f (e.position.x, s);
        if (dragging == 0) f = std::min (f, value (params::imgX (1)) / 1.3f);
        else f = std::max (f, value (params::imgX (0)) * 1.3f);
        setParam (proc, params::imgX (dragging), f, 0);
    }
    else if (dragging >= 2)
    {
        const float half = s.getHeight() * 0.5f - 22.0f;
        const float w = juce::jlimit (0.0f, 200.0f, std::abs (e.position.y - s.getCentreY()) / half * 200.0f);
        setParam (proc, params::imgW (dragging - 2), w, 0);
    }
    repaint();
}

void WidthScreen::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0 && dragging < 2)
        setParam (proc, params::imgX (dragging), value (params::imgX (dragging)), 2);
    else if (dragging >= 2)
        setParam (proc, params::imgW (dragging - 2), value (params::imgW (dragging - 2)), 2);
    dragging = -1;
}

//==============================================================================
namespace
{
NeedleMeter::Scale correlationScale()
{
    NeedleMeter::Scale s;
    s.minValue = -1.0f;
    s.maxValue = 1.0f;
    s.marks = { { -1.0f, "-1" }, { -0.5f, "-.5" }, { 0.0f, "0" }, { 0.5f, "+.5" }, { 1.0f, "+1" } };
    for (float m = -0.9f; m < 0.95f; m += 0.1f)
        s.minorMarks.push_back (m);
    s.redFrom = 0.0f;
    s.redBelow = true;
    s.unit = cyr ("КОРР.");
    s.title = "CORR";
    s.attackMs = 120.0f;
    s.releaseMs = 120.0f;
    return s;
}
} // namespace

ImagerPanel::ImagerPanel (KonveyerProcessor& p)
    : StationPanel (p), scope (p), corr (correlationScale()), widths (p),
      wLow ("LOW WIDTH", cyr ("ШИРИНА НЧ"), Knob::Size::Large),
      wMid ("MID WIDTH", cyr ("ШИРИНА СЧ"), Knob::Size::Large),
      wHigh ("HIGH WIDTH", cyr ("ШИРИНА ВЧ"), Knob::Size::Large),
      xLow ("CROSSOVER LOW", cyr ("РАЗДЕЛ НЧ"), Knob::Size::Medium),
      xHigh ("CROSSOVER HIGH", cyr ("РАЗДЕЛ ВЧ"), Knob::Size::Medium),
      mono ("MONO BASS", cyr ("МОНО НЧ"), col::amber, true)
{
    for (juce::Component* c : std::initializer_list<juce::Component*> { &scope, &corr, &widths, &wLow, &wMid, &wHigh, &xLow, &xHigh, &mono })
        addAndMakeVisible (c);
    wLow.attach (proc.apvts, params::imgW (0));
    wMid.attach (proc.apvts, params::imgW (1));
    wHigh.attach (proc.apvts, params::imgW (2));
    xLow.attach (proc.apvts, params::imgX (0));
    xHigh.attach (proc.apvts, params::imgX (1));
    mono.attach (proc.apvts, params::imgMono);
}

void ImagerPanel::refresh (const MeterFrame& f)
{
    scope.refresh (f);
    widths.refresh (f);
    corr.setTarget (f.correlation);
    corr.tick (f.dt);
    const bool m = rawValue (proc, params::imgMono) > 0.5f;
    if (wLow.isEnabled() == m)
    {
        wLow.setEnabled (! m);
        wLow.setAlpha (m ? 0.45f : 1.0f);
    }
}

juce::String ImagerPanel::hint() const
{
    return "0 % = mono, 100 % = untouched, 200 % = twice as wide" + dot() + "drag a band up or down on the screen" + dot() + "mono bass makes everything below the low crossover mono";
}

void ImagerPanel::resized()
{
    auto r = getLocalBounds().toFloat().reduced (12.0f, 10.0f);
    auto left = r.removeFromLeft (304.0f);
    scope.setBounds (left.removeFromTop (304.0f).toNearestInt());
    left.removeFromTop (14.0f);
    corr.setBounds (left.withSizeKeepingCentre (left.getWidth() - 20.0f, std::min (left.getHeight(), 150.0f)).toNearestInt());
    r.removeFromLeft (14.0f);
    widths.setBounds (r.removeFromTop (236.0f).toNearestInt());
    r.removeFromTop (10.0f);
    secKnobs = r;
    auto k = r.reduced (8.0f, 6.0f);
    k.removeFromTop (16.0f);
    auto row1 = k.removeFromTop (k.getHeight() * 0.56f);
    const float w = row1.getWidth() / 4.0f;
    wLow.setBounds (row1.removeFromLeft (w).toNearestInt());
    wMid.setBounds (row1.removeFromLeft (w).toNearestInt());
    wHigh.setBounds (row1.removeFromLeft (w).toNearestInt());
    mono.setBounds (row1.reduced (8.0f, 14.0f).toNearestInt());
    xLow.setBounds (k.removeFromLeft (w).toNearestInt());
    xHigh.setBounds (k.removeFromLeft (w).toNearestInt());
}

void ImagerPanel::paint (juce::Graphics& g)
{
    paintSection (g, secKnobs, "STEREO WIDTH", cyr ("СТЕРЕОБАЗА"));
    // explanatory plate in the free corner
    auto plate = juce::Rectangle<float> (secKnobs.getX() + secKnobs.getWidth() * 0.5f + 8.0f, secKnobs.getBottom() - 70.0f,
                                         secKnobs.getWidth() * 0.5f - 22.0f, 54.0f);
    paintPlate (g, plate, col::cream, true);
    inked (g, "0 % MONO   100 % ORIGINAL   200 % WIDE", plate.withTrimmedBottom (24.0f).translated (0.0f, 4.0f), label (11.0f, 0.06f));
    inked (g, cyr ("0 % МОНО   100 % ИСХОДНО   200 % ШИРОКО"), plate.withTrimmedTop (26.0f).translated (0.0f, -4.0f), light (10.0f, 0.06f), juce::Justification::centred, col::inkSoft);
}

//==============================================================================
HistoryScreen::HistoryScreen (KonveyerProcessor& p) : CrtScreen (false), proc (p)
{
    bezelLabel = cyr ("ИСТОРИЯ  ·  ВХОД / ВЫХОД / ПОДАВЛЕНИЕ");
    in.resize (1024);
    out.resize (1024);
    gr.resize (1024);
}

void HistoryScreen::paintStatic (juce::Graphics& g, juce::Rectangle<float> s)
{
    const float top = 0.0f, bottom = -36.0f;
    auto y = [&] (float db) { return juce::jmap (juce::jlimit (bottom, top, db), top, bottom, s.getY() + 6.0f, s.getBottom() - 4.0f); };
    g.setFont (mono (10.5f));
    for (float d = 0.0f; d >= bottom; d -= 6.0f)
    {
        g.setColour (col::grid.withAlpha (0.8f));
        g.drawHorizontalLine (juce::roundToInt (y (d)), s.getX(), s.getRight());
        g.setColour (col::phosphorDim);
        g.drawText (juce::String ((int) d), juce::Rectangle<float> (s.getRight() - 34.0f, y (d) - 7.0f, 28.0f, 14.0f), juce::Justification::centredRight, false);
    }
    for (float x = s.getRight(); x > s.getX(); x -= 100.0f)
    {
        g.setColour (col::grid.withAlpha (0.6f));
        g.drawVerticalLine (juce::roundToInt (x), s.getY(), s.getBottom());
    }
}

void HistoryScreen::paintScreen (juce::Graphics& g, juce::Rectangle<float> s)
{
    const float top = 0.0f, bottom = -36.0f;
    auto y = [&] (float db) { return juce::jmap (juce::jlimit (bottom, top, db), top, bottom, s.getY() + 6.0f, s.getBottom() - 4.0f); };

    auto& lim = proc.engine().getLimiter();
    const int n = std::min ((int) in.size(), (int) (s.getWidth() / 1.5f));
    const auto written = lim.histGr.written();
    if (written < (uint64_t) 2)
        return;
    const int avail = (int) std::min<uint64_t> ((uint64_t) n, written);
    lim.histIn.readLatest (in.data(), avail);
    lim.histOut.readLatest (out.data(), avail);
    lim.histGr.readLatest (gr.data(), avail);

    const float stepX = 1.5f;
    juce::Path inPath, outPath, grPath;
    const float x0 = s.getRight() - (float) avail * stepX;
    inPath.startNewSubPath (x0, s.getBottom());
    grPath.startNewSubPath (x0, s.getY());
    for (int i = 0; i < avail; ++i)
    {
        const float x = x0 + (float) i * stepX;
        inPath.lineTo (x, y (in[(size_t) i]));
        if (i == 0) outPath.startNewSubPath (x, y (out[(size_t) i])); else outPath.lineTo (x, y (out[(size_t) i]));
        grPath.lineTo (x, juce::jmap (std::min (gr[(size_t) i], 36.0f), 0.0f, 36.0f, s.getY(), s.getBottom()));
    }
    inPath.lineTo (s.getRight(), s.getBottom());
    inPath.closeSubPath();
    grPath.lineTo (s.getRight(), s.getY());
    grPath.closeSubPath();

    juce::ColourGradient inFill (col::phosphor.withAlpha (0.16f), s.getX(), s.getY(), col::phosphor.withAlpha (0.04f), s.getX(), s.getBottom(), false);
    g.setGradientFill (inFill);
    g.fillPath (inPath);
    paintGlowPath (g, outPath, col::phosphor, 1.0f, 0.8f);
    g.setColour (col::amber.withAlpha (0.35f));
    g.fillPath (grPath);
    g.setColour (col::amber.withAlpha (0.9f));
    g.strokePath (grPath, juce::PathStrokeType (1.0f));

    const float ceil = rawValue (proc, params::limCeil);
    juce::Path ceilLine;
    ceilLine.startNewSubPath (s.getX(), y (ceil));
    ceilLine.lineTo (s.getRight(), y (ceil));
    juce::Path dashed;
    const float dashes[] = { 6.0f, 4.0f };
    juce::PathStrokeType (1.2f).createDashedStroke (dashed, ceilLine, dashes, 2);
    g.setColour (juce::Colour (0xffff5a3c));
    g.fillPath (dashed);

    // legend
    g.setFont (mono (10.5f));
    float lx = s.getX() + 10.0f;
    const float ly = s.getBottom() - 19.0f;
    for (auto [text, c] : { std::pair<const char*, juce::Colour> { "IN", col::phosphorDim }, { "OUT", col::phosphor },
                            { "GAIN REDUCTION", col::amber }, { "CEILING", juce::Colour (0xffff5a3c) } })
    {
        g.setColour (c);
        g.fillRect (lx, ly + 5.0f, 10.0f, 3.0f);
        g.drawText (text, juce::Rectangle<float> (lx + 14.0f, ly, 120.0f, 14.0f), juce::Justification::centredLeft, false);
        lx += 24.0f + 7.0f * (float) std::strlen (text);
    }
}

//==============================================================================
namespace
{
NeedleMeter::Scale limiterScale()
{
    NeedleMeter::Scale s;
    s.minValue = 0.0f;
    s.maxValue = 12.0f;
    s.position = [] (float v) { return 1.0f - std::log1p (v) / std::log1p (12.0f); };
    s.marks = { { 0.0f, "0" }, { 1.0f, "1" }, { 2.0f, "2" }, { 3.0f, "3" }, { 4.0f, "4" }, { 6.0f, "6" }, { 9.0f, "9" }, { 12.0f, "12" } };
    for (float m = 0.5f; m < 12.0f; m += m < 4.0f ? 0.5f : 1.0f)
        s.minorMarks.push_back (m);
    s.redFrom = 6.0f;
    s.redBelow = true;
    s.title = "GR";
    s.attackMs = 3.0f;
    s.releaseMs = 220.0f;
    return s;
}
} // namespace

LimiterPanel::LimiterPanel (KonveyerProcessor& p)
    : StationPanel (p), history (p), meter (limiterScale()),
      drive ("GAIN", cyr ("УСИЛЕНИЕ"), Knob::Size::Large),
      ceiling ("CEILING", cyr ("ПОТОЛОК"), Knob::Size::Large),
      rel ("RELEASE", cyr ("ВОССТАНОВЛЕНИЕ"), Knob::Size::Medium),
      look ("LOOKAHEAD", cyr ("ОПЕРЕЖЕНИЕ"), Knob::Size::Medium),
      truePeak ("TRUE PEAK", cyr ("ИСТ. ПИК"), col::amber, true),
      autoRel ("AUTO RELEASE", cyr ("АВТО ВОССТ."), col::amber, true)
{
    for (juce::Component* c : std::initializer_list<juce::Component*> { &history, &meter, &drive, &ceiling, &rel, &look, &truePeak, &autoRel })
        addAndMakeVisible (c);
    drive.attach (proc.apvts, params::limDrive);
    ceiling.attach (proc.apvts, params::limCeil);
    rel.attach (proc.apvts, params::limRel);
    look.attach (proc.apvts, params::limLook);
    truePeak.attach (proc.apvts, params::limTp);
    autoRel.attach (proc.apvts, params::limAuto);
}

void LimiterPanel::refresh (const MeterFrame& f)
{
    history.refresh (f);
    meter.setTarget (f.limGr);
    meter.tick (f.dt);
}

juce::String LimiterPanel::hint() const
{
    return "Gain pushes into the ceiling" + dot() + "true peak also catches overs between samples" + dot() + "the red line on the screen is the ceiling";
}

void LimiterPanel::resized()
{
    auto r = getLocalBounds().toFloat().reduced (12.0f, 10.0f);
    history.setBounds (r.removeFromTop (246.0f).toNearestInt());
    r.removeFromTop (10.0f);
    auto right = r.removeFromRight (300.0f);
    meter.setBounds (right.reduced (4.0f, 16.0f).toNearestInt());
    r.removeFromRight (12.0f);
    secKnobs = r;
    auto k = r.reduced (8.0f, 6.0f);
    k.removeFromTop (16.0f);
    auto row = k.removeFromTop (150.0f);
    const float w = row.getWidth() / 4.0f;
    drive.setBounds (row.removeFromLeft (w).toNearestInt());
    ceiling.setBounds (row.removeFromLeft (w).toNearestInt());
    rel.setBounds (row.removeFromLeft (w).reduced (4.0f, 8.0f).toNearestInt());
    look.setBounds (row.reduced (4.0f, 8.0f).toNearestInt());
    k.removeFromTop (4.0f);
    truePeak.setBounds (k.removeFromLeft (w * 1.1f).toNearestInt());
    autoRel.setBounds (k.removeFromLeft (w * 1.1f).toNearestInt());
    secSwitches = k;
}

void LimiterPanel::paint (juce::Graphics& g)
{
    paintSection (g, secKnobs, "BRICKWALL LIMITER", cyr ("ОГРАНИЧИТЕЛЬ"));
    auto plate = secSwitches.reduced (10.0f, 14.0f).withHeight (46.0f);
    paintPlate (g, plate, col::cream, true);
    const double ms = proc.getLatencySamples() * 1000.0 / proc.currentSampleRate();
    inked (g, "LATENCY " + juce::String (ms, 2) + " MS", plate.withTrimmedBottom (20.0f).translated (0.0f, 4.0f), label (11.5f, 0.08f));
    inked (g, cyr ("ЗАДЕРЖКА ПОСТОЯННА"), plate.withTrimmedTop (24.0f).translated (0.0f, -4.0f), light (10.0f, 0.06f), juce::Justification::centred, col::inkSoft);
}
} // namespace ui
