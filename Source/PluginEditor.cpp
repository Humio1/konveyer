#include "PluginEditor.h"
#include "UI/Widgets.h"
#include "UI/Crt.h"

using namespace ui;

namespace
{
float rawValue (TishinaProcessor& p, const juce::String& id) { return p.apvts.getRawParameterValue (id)->load(); }

void paintSection (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& en, const juce::String& ru)
{
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawLine (r.getX() + 5.0f, r.getBottom() + 1.0f, r.getRight() - 5.0f, r.getBottom() + 1.0f, 1.0f);

    auto t = r.reduced (8.0f, 4.0f).removeFromTop (14.0f);
    juce::GlyphArrangement ga;
    ga.addLineOfText (label (11.0f, 0.14f), en, 0.0f, 0.0f);
    const float w = ga.getBoundingBox (0, -1, true).getWidth();
    engraved (g, en, t, label (11.0f, 0.14f), juce::Justification::centredLeft, col::cream.withAlpha (0.85f));
    engraved (g, ru, t.withTrimmedLeft (w + 8.0f), light (10.0f, 0.12f), juce::Justification::centredLeft, col::cream.withAlpha (0.55f));
}

//==============================================================================
/** Preset window: black slot with the program name, arrows either side, menu on click. */
class ProgramBox : public juce::Component
{
public:
    explicit ProgramBox (TishinaProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        caption (g, r.removeFromTop (26.0f), "PROGRAM", cyr ("ПРОГРАММА"), col::cream, 12.0f);
        auto slot = r.reduced (0.0f, 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (slot.translated (0.0f, 1.5f), 4.0f);
        g.setColour (juce::Colour (0xff141210));
        g.fillRoundedRectangle (slot, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (slot, 4.0f, 1.0f);

        prevArea = slot.removeFromLeft (30.0f);
        nextArea = slot.removeFromRight (30.0f);
        for (auto [a, dir] : { std::pair<juce::Rectangle<float>, float> { prevArea, -1.0f }, { nextArea, 1.0f } })
        {
            juce::Path tri;
            const auto c = a.getCentre();
            tri.addTriangle (c.x - 5.0f * dir, c.y - 6.0f, c.x - 5.0f * dir, c.y + 6.0f, c.x + 5.0f * dir, c.y);
            g.setColour (col::cream.withAlpha (0.8f));
            g.fillPath (tri);
        }
        const int idx = proc.getCurrentProgram();
        g.setFont (mono (15.0f));
        g.setColour (col::amber);
        g.drawText (juce::String (idx + 1).paddedLeft ('0', 2) + "  " + proc.getProgramName (idx).toUpperCase(), slot, juce::Justification::centred, true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int n = proc.getNumPrograms(), cur = proc.getCurrentProgram();
        if (prevArea.contains (e.position)) { proc.loadPreset ((cur + n - 1) % n); repaint(); return; }
        if (nextArea.contains (e.position)) { proc.loadPreset ((cur + 1) % n); repaint(); return; }
        juce::PopupMenu m;
        m.setLookAndFeel (&getLookAndFeel());
        for (int i = 0; i < n; ++i)
            m.addItem (i + 1, juce::String (i + 1).paddedLeft ('0', 2) + "   " + proc.getProgramName (i), true, i == cur);
        juce::Component::SafePointer<ProgramBox> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()), [safe] (int r)
        {
            if (safe != nullptr && r > 0)
            {
                safe->proc.loadPreset (r - 1);
                safe->repaint();
            }
        });
    }

private:
    TishinaProcessor& proc;
    juce::Rectangle<float> prevArea, nextArea;
};

//==============================================================================
/** Everything static behind the controls, rendered once into a cached image. */
class Backdrop : public juce::Component
{
public:
    Backdrop()
    {
        setBufferedToImage (true);
        setInterceptsMouseClicks (false, false);
    }

    juce::Rectangle<float> header, screenArea, sideArea, controlArea, footer;
    std::vector<std::tuple<juce::Rectangle<float>, juce::String, juce::String>> sections;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::room);

        juce::ColourGradient hg (col::frameLight, 0.0f, header.getY(), col::frame, 0.0f, header.getBottom(), false);
        g.setGradientFill (hg);
        g.fillRect (header);
        paintTexture (g, header.toNearestInt(), 0.25f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawHorizontalLine ((int) header.getBottom() - 1, header.getX(), header.getRight());

        // logo plate
        auto logoPlate = juce::Rectangle<float> (16.0f, 11.0f, 262.0f, 54.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (logoPlate.translated (0.0f, 2.0f), 3.0f);
        juce::ColourGradient red (col::red.brighter (0.15f), logoPlate.getX(), logoPlate.getY(), col::red.darker (0.35f), logoPlate.getX(), logoPlate.getBottom(), false);
        g.setGradientFill (red);
        g.fillRoundedRectangle (logoPlate, 3.0f);
        paintTexture (g, logoPlate.toNearestInt(), 0.2f);
        g.setColour (col::cream.withAlpha (0.85f));
        g.drawRoundedRectangle (logoPlate.reduced (3.5f), 2.0f, 1.0f);
        juce::Path wedge;
        wedge.addTriangle (logoPlate.getRight() - 64.0f, logoPlate.getBottom() - 3.5f, logoPlate.getRight() - 3.5f, logoPlate.getY() + 3.5f,
                           logoPlate.getRight() - 3.5f, logoPlate.getBottom() - 3.5f);
        g.setColour (col::ink.withAlpha (0.85f));
        g.fillPath (wedge);
        g.setFont (logo (34.0f));
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawText ("TISHINA", logoPlate.withTrimmedLeft (16.0f).translated (1.5f, 2.0f), juce::Justification::centredLeft, false);
        g.setColour (col::cream);
        g.drawText ("TISHINA", logoPlate.withTrimmedLeft (16.0f), juce::Justification::centredLeft, false);

        auto sub = juce::Rectangle<float> (logoPlate.getRight() + 14.0f, 18.0f, 240.0f, 40.0f);
        engraved (g, "RESONANCE SUPPRESSOR", sub.withHeight (20.0f), label (16.0f, 0.18f), juce::Justification::centredLeft);
        engraved (g, cyr ("ТИШИНА  ·  ПОДАВИТЕЛЬ РЕЗОНАНСОВ"), sub.withTrimmedTop (21.0f), light (11.5f, 0.12f), juce::Justification::centredLeft, col::cream.withAlpha (0.7f));

        auto tp = juce::Rectangle<float> (header.getRight() - 236.0f, 13.0f, 220.0f, 50.0f);
        paintPlate (g, tp, juce::Colour (0xffbfc4c0), true);
        inked (g, cyr ("ИЗДЕЛИЕ  №  212"), tp.withHeight (25.0f).translated (0.0f, 4.0f), label (13.0f, 0.16f));
        inked (g, "MODEL TS-1" + dot() + "SER. 0001" + dot() + "2026", tp.withTrimmedTop (25.0f).translated (0.0f, -4.0f), light (10.5f, 0.1f), juce::Justification::centred, col::inkSoft);

        for (auto a : { screenArea, sideArea, controlArea })
        {
            paintEnamel (g, a, a == sideArea ? col::enamel.darker (0.08f) : col::enamel, 6.0f);
            for (auto p : { a.getTopLeft() + juce::Point<float> (9, 9), a.getTopRight() + juce::Point<float> (-9, 9),
                            a.getBottomLeft() + juce::Point<float> (9, -9), a.getBottomRight() + juce::Point<float> (-9, -9) })
                paintScrew (g, p, 4.0f, p.y * 0.05f + p.x * 0.01f);
        }
        for (auto& [r, en, ru] : sections)
            paintSection (g, r, en, ru);

        g.setColour (col::frame);
        g.fillRect (footer);
        paintTexture (g, footer.toNearestInt(), 0.2f);
    }
};

/** Status line: a hint, sample rate, FFT size and latency. */
class Footer : public juce::Component
{
public:
    explicit Footer (TishinaProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }

    void paint (juce::Graphics& g) override
    {
        auto f = getLocalBounds().toFloat().reduced (16.0f, 0.0f);
        const double sr = proc.currentSampleRate();
        const juce::String right = "TS-1 v" JucePlugin_VersionString + dot() + juce::String (sr / 1000.0, 1) + " kHz" + dot()
                                 + "FFT " + juce::String (proc.engine().getFftSize()) + dot()
                                 + "LATENCY " + juce::String (proc.getLatencySamples() * 1000.0 / sr, 1) + " ms";
        engraved (g, right, f.removeFromRight (380.0f), mono (11.5f), juce::Justification::centredRight, col::cream.withAlpha (0.6f));
        engraved (g, "Drag LOW and HIGH on the screen to set the working range" + dot() + "DELTA plays only what is taken out",
                  f, regular (12.0f, 0.03f), juce::Justification::centredLeft, col::cream.withAlpha (0.75f));
    }

private:
    TishinaProcessor& proc;
};

//==============================================================================
/** The main tube: input spectrum in green phosphor, the cut hanging from the top in amber,
    and the two range handles. */
class ResonanceScreen : public CrtScreen
{
public:
    explicit ResonanceScreen (TishinaProcessor& p) : CrtScreen (false), proc (p)
    {
        bezelLabel = cyr ("СПЕКТР  ·  ПОДАВЛЕНИЕ");
        axis.fMin = 20.0f;
        axis.fMax = 20000.0f;
        level.fill (-120.0f);
        cut.fill (0.0f);
    }

    void refresh()
    {
        auto& e = proc.engine();
        for (int i = 0; i < kv::Suppressor::kDisplayPoints; ++i)
        {
            level[(size_t) i] = e.displayLevel[(size_t) i].load (std::memory_order_relaxed);
            cut[(size_t) i] = e.displayCut[(size_t) i].load (std::memory_order_relaxed);
        }
        repaint();
    }

    static constexpr float kTopDb = 0.0f, kBottomDb = -96.0f, kCutMax = 24.0f;

    float levelY (float db, juce::Rectangle<float> s) const { return juce::jmap (db, kTopDb, kBottomDb, s.getY() + 4.0f, s.getBottom()); }
    float cutY (float db, juce::Rectangle<float> s) const { return s.getY() + 6.0f + (s.getHeight() * 0.46f) * juce::jlimit (0.0f, 1.0f, db / kCutMax); }

    void paintStatic (juce::Graphics& g, juce::Rectangle<float> s) override
    {
        axis.paintGrid (g, s, true);
        g.setFont (mono (10.0f));
        for (float db : { 6.0f, 12.0f, 18.0f, 24.0f })
        {
            const float y = cutY (db, s);
            g.setColour (col::amber.withAlpha (0.12f));
            g.drawHorizontalLine ((int) y, s.getX() + 4.0f, s.getRight() - 4.0f);
            g.setColour (col::amber.withAlpha (0.5f));
            g.drawText ("-" + juce::String ((int) db), juce::Rectangle<float> (s.getRight() - 34.0f, y - 12.0f, 28.0f, 11.0f), juce::Justification::centredRight);
        }
    }

    void paintScreen (juce::Graphics& g, juce::Rectangle<float> s) override
    {
        const int n = kv::Suppressor::kDisplayPoints;
        // input spectrum, tilted +4.5 dB/oct around 1 kHz so pink noise reads flat
        juce::Path spec;
        for (int i = 0; i < n; ++i)
        {
            const float f = (float) kv::Suppressor::displayHz (i);
            const float db = level[(size_t) i] + 4.5f * std::log2 (f / 1000.0f);
            const juce::Point<float> pt (axis.x (f, s), juce::jlimit (s.getY(), s.getBottom(), levelY (db, s)));
            if (i == 0) spec.startNewSubPath (pt); else spec.lineTo (pt);
        }
        juce::Path fill (spec);
        fill.lineTo (s.getRight(), s.getBottom());
        fill.lineTo (s.getX(), s.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (col::phosphor.withAlpha (0.22f), 0.0f, s.getY(), col::phosphor.withAlpha (0.03f), 0.0f, s.getBottom(), false));
        g.fillPath (fill);
        paintGlowPath (g, spec, col::phosphor, 1.4f, 0.8f);

        // reduction, hanging from the top
        juce::Path red;
        red.startNewSubPath (s.getX(), cutY (0.0f, s));
        for (int i = 0; i < n; ++i)
        {
            const float f = (float) kv::Suppressor::displayHz (i);
            red.lineTo (axis.x (f, s), cutY (cut[(size_t) i], s));
        }
        red.lineTo (s.getRight(), cutY (0.0f, s));
        juce::Path redFill (red);
        redFill.lineTo (s.getRight(), s.getY());
        redFill.lineTo (s.getX(), s.getY());
        redFill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (col::amber.withAlpha (0.08f), 0.0f, s.getY(), col::amberGlow.withAlpha (0.35f), 0.0f, cutY (kCutMax, s), false));
        g.fillPath (redFill);
        paintGlowPath (g, red, col::amber, 1.6f, 1.0f);

        // range: dim what is outside, draw the handles
        const float xl = axis.x (rawValue (proc, params::low), s), xh = axis.x (rawValue (proc, params::high), s);
        const float a = std::min (xl, xh), b = std::max (xl, xh);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        if (a > s.getX())
            g.fillRect (juce::Rectangle<float> (s.getX(), s.getY(), a - s.getX(), s.getHeight()));
        if (b < s.getRight())
            g.fillRect (juce::Rectangle<float> (b, s.getY(), s.getRight() - b, s.getHeight()));
        paintHandle (g, s, xl, "LO", rawValue (proc, params::low), hover == 0 || drag == 0);
        paintHandle (g, s, xh, "HI", rawValue (proc, params::high), hover == 1 || drag == 1);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int h = hit (e.position.x);
        if (h != hover)
        {
            hover = h;
            setMouseCursor (h >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hover = -1;
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        drag = hit (e.position.x);
        if (drag >= 0)
            if (auto* p = param (drag))
                p->beginChangeGesture();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag < 0)
            return;
        const auto s = screenArea();
        float f = axis.f (juce::jlimit (s.getX(), s.getRight(), e.position.x), s);
        // keep at least a third of an octave between the two handles
        if (drag == 0)
            f = std::min (f, rawValue (proc, params::high) / 1.26f);
        else
            f = std::max (f, rawValue (proc, params::low) * 1.26f);
        if (auto* p = param (drag))
            p->setValueNotifyingHost (p->convertTo0to1 (f));
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (drag >= 0)
            if (auto* p = param (drag))
                p->endChangeGesture();
        drag = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int h = hit (e.position.x);
        if (auto* p = h >= 0 ? param (h) : nullptr)
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getDefaultValue());
            p->endChangeGesture();
        }
    }

private:
    juce::RangedAudioParameter* param (int which) { return proc.apvts.getParameter (which == 0 ? params::low : params::high); }

    int hit (float x) const
    {
        const auto s = screenArea();
        const float xl = axis.x (rawValue (proc, params::low), s), xh = axis.x (rawValue (proc, params::high), s);
        const float dl = std::abs (x - xl), dh = std::abs (x - xh);
        if (std::min (dl, dh) > 9.0f)
            return -1;
        return dl <= dh ? 0 : 1;
    }

    void paintHandle (juce::Graphics& g, juce::Rectangle<float> s, float x, const char* tag, float hz, bool active) const
    {
        const auto c = active ? col::cream : col::cream.withAlpha (0.7f);
        juce::Path line;
        line.startNewSubPath (x, s.getY() + 4.0f);
        line.lineTo (x, s.getBottom() - 4.0f);
        juce::Path dashed;
        const float dash[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, line, dash, 2);
        g.setColour (c.withAlpha (active ? 0.8f : 0.45f));
        g.fillPath (dashed);

        auto tab = juce::Rectangle<float> (0.0f, 0.0f, 62.0f, 22.0f).withCentre ({ x, s.getBottom() - 38.0f });
        tab = tab.withX (juce::jlimit (s.getX() + 2.0f, s.getRight() - tab.getWidth() - 2.0f, tab.getX()));
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillRoundedRectangle (tab, 3.0f);
        g.setColour (c);
        g.drawRoundedRectangle (tab, 3.0f, active ? 1.4f : 1.0f);
        g.setFont (mono (10.0f));
        g.drawText (juce::String (tag) + " " + shortFreq (hz), tab, juce::Justification::centred, false);
    }

    TishinaProcessor& proc;
    FreqAxis axis;
    std::array<float, kv::Suppressor::kDisplayPoints> level {}, cut {};
    int hover = -1, drag = -1;
};

NeedleMeter::Scale reductionScale()
{
    NeedleMeter::Scale s;
    s.minValue = 0.0f;
    s.maxValue = 24.0f;
    s.position = [] (float v) { return 1.0f - std::log1p (v) / std::log1p (24.0f); };
    s.marks = { { 0.0f, "0" }, { 1.0f, "1" }, { 2.0f, "2" }, { 3.0f, "3" }, { 6.0f, "6" }, { 12.0f, "12" }, { 24.0f, "24" } };
    for (float m : { 0.5f, 1.5f, 2.5f, 4.0f, 5.0f, 8.0f, 10.0f, 16.0f, 20.0f })
        s.minorMarks.push_back (m);
    s.redFrom = 12.0f;
    s.redBelow = true;
    s.title = "CUT";
    s.attackMs = 5.0f;
    s.releaseMs = 300.0f;
    return s;
}
} // namespace

//==============================================================================
class TishinaEditor::Content : public juce::Component
{
public:
    explicit Content (TishinaProcessor& p)
        : proc (p), footerBar (p), program (p), screen (p), meter (reductionScale()),
          peakCut ("PEAK CUT", cyr ("МАКС. ПОДАВЛЕНИЕ"), "dB", 3, false),
          outPeak ("OUTPUT PEAK", cyr ("ПИК ВЫХОДА"), "dBFS", 3, true),
          depth ("DEPTH", cyr ("ГЛУБИНА"), Knob::Size::Large),
          sharpness ("SHARPNESS", cyr ("ОСТРОТА"), Knob::Size::Large),
          selectivity ("SELECTIVITY", cyr ("ИЗБИРАТЕЛЬНОСТЬ"), Knob::Size::Large),
          attack ("ATTACK", cyr ("АТАКА")),
          release ("RELEASE", cyr ("ВОССТАНОВЛЕНИЕ")),
          mix ("MIX", cyr ("СМЕСЬ")),
          output ("OUTPUT", cyr ("ВЫХОД"), Knob::Size::Medium, true),
          low ("RANGE LOW", cyr ("НИЖНЯЯ ГРАНИЦА")),
          high ("RANGE HIGH", cyr ("ВЕРХНЯЯ ГРАНИЦА")),
          emphasis ("EMPHASIS", cyr ("АКЦЕНТ"), Knob::Size::Medium, true),
          mode ("MODE", cyr ("РЕЖИМ"), { "LINK", "L/R", "M/S" }, 120.0f),
          quality ("QUALITY", cyr ("КАЧЕСТВО"), { "FAST", "NORM", "FINE" }, 120.0f),
          soft ("SOFT KNEE", cyr ("МЯГКОЕ КОЛЕНО"), col::amber),
          delta ("DELTA", cyr ("РАЗНОСТЬ"), col::red)
    {
        addAndMakeVisible (backdrop);
        addAndMakeVisible (footerBar);
        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 &program, &screen, &meter, &peakCut, &outPeak, &depth, &sharpness, &selectivity, &attack, &release, &mix, &output,
                 &low, &high, &emphasis, &mode, &quality, &soft, &delta })
            addAndMakeVisible (c);

        auto& s = p.apvts;
        depth.attach (s, params::depth);
        sharpness.attach (s, params::sharpness);
        selectivity.attach (s, params::selectivity);
        attack.attach (s, params::attack);
        release.attach (s, params::release);
        mix.attach (s, params::mix);
        output.attach (s, params::output);
        low.attach (s, params::low);
        high.attach (s, params::high);
        emphasis.attach (s, params::emphasis);
        mode.attach (s, params::mode);
        quality.attach (s, params::quality);
        soft.attach (s, params::soft);
        delta.attach (s, params::delta);

        depth.slider.setTooltip ("How hard resonances are pulled down");
        sharpness.slider.setTooltip ("High: only narrow peaks count as resonances. Low: broader build-ups are smoothed too");
        selectivity.slider.setTooltip ("How far a peak must stand out before it is touched. High leaves more alone");
        emphasis.slider.setTooltip ("Work harder on the highs (+) or on the lows (-)");
        mode.setTooltip ("LINK: same cut on both channels. L/R: each channel on its own. M/S: mid and side on their own");
        quality.setTooltip ("Frequency resolution. FAST has the lowest latency, FINE resolves the lows best");
        delta.setTooltip ("Listen only to what is being removed");
    }

    void tick (float dt)
    {
        screen.refresh();
        auto& e = proc.engine();
        const float c = std::max (0.0f, e.maxCut.take (0.0f));
        meter.setTarget (c);
        meter.tick (dt);
        holdCut = std::max (c, holdCut - dt * 4.0f);
        peakCut.setValue (holdCut, true);
        const float o = proc.outPeak.take (-120.0f);
        holdOut = std::max (o, holdOut - dt * 12.0f);
        outPeak.setValue (holdOut, holdOut > -70.0f);
        if (++slowCounter % 15 == 0)
        {
            program.repaint();
            footerBar.repaint();
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().toFloat();
        const auto header = r.removeFromTop (76.0f);
        const auto footer = r.removeFromBottom (30.0f);
        r = r.reduced (8.0f, 6.0f);
        auto top = r.removeFromTop (352.0f);
        r.removeFromTop (8.0f);
        const auto controls = r;
        auto side = top.removeFromRight (290.0f);
        top.removeFromRight (8.0f);

        backdrop.header = header;
        backdrop.footer = footer;
        backdrop.screenArea = top;
        backdrop.sideArea = side;
        backdrop.controlArea = controls;

        program.setBounds (juce::Rectangle<float> (header.getCentreX() - 10.0f, header.getY() + 8.0f, 300.0f, 60.0f).toNearestInt());
        screen.setBounds (top.reduced (12.0f, 12.0f).toNearestInt());

        auto sd = side.reduced (14.0f, 16.0f);
        meter.setBounds (sd.removeFromTop (170.0f).toNearestInt());
        sd.removeFromTop (8.0f);
        peakCut.setBounds (sd.removeFromTop (78.0f).toNearestInt());
        sd.removeFromTop (6.0f);
        outPeak.setBounds (sd.removeFromTop (78.0f).toNearestInt());

        // control strip: detector | shaping | switches
        auto c = controls.reduced (14.0f, 12.0f);
        auto secDet = c.removeFromLeft (396.0f);
        c.removeFromLeft (10.0f);
        auto secSw = c.removeFromRight (250.0f);
        c.removeFromRight (10.0f);
        auto secShape = c;
        backdrop.sections = { { secDet, "DETECTOR", cyr ("ДЕТЕКТОР") },
                              { secShape, "TIME & RANGE", cyr ("ВРЕМЯ И ДИАПАЗОН") },
                              { secSw, "OPERATION", cyr ("РАБОТА") } };
        backdrop.setBounds (getLocalBounds());
        backdrop.repaint();
        footerBar.setBounds (footer.toNearestInt());

        auto d = secDet.reduced (8.0f, 24.0f).withTrimmedBottom (-12.0f);
        const float dw = d.getWidth() / 3.0f;
        for (auto* k : { &depth, &sharpness, &selectivity })
            k->setBounds (d.removeFromLeft (dw).withSizeKeepingCentre (dw, std::min (d.getHeight(), 196.0f)).toNearestInt());

        auto sh = secShape.reduced (8.0f, 4.0f).withTrimmedTop (18.0f);
        auto row1 = sh.removeFromTop (sh.getHeight() * 0.5f);
        auto row2 = sh;
        const float sw = row1.getWidth() / 4.0f;
        for (auto* k : { &attack, &release, &mix, &output })
            k->setBounds (row1.removeFromLeft (sw).reduced (2.0f, 0.0f).toNearestInt());
        for (auto* k : { &low, &high, &emphasis })
            k->setBounds (row2.removeFromLeft (sw).reduced (2.0f, 0.0f).toNearestInt());
        soft.setBounds (row2.reduced (6.0f, 10.0f).toNearestInt());

        auto sws = secSw.reduced (8.0f, 4.0f).withTrimmedTop (18.0f);
        auto srow = sws.removeFromTop (sws.getHeight() * 0.56f);
        mode.setBounds (srow.removeFromLeft (srow.getWidth() * 0.5f).toNearestInt());
        quality.setBounds (srow.toNearestInt());
        delta.setBounds (sws.withSizeKeepingCentre (130.0f, sws.getHeight()).toNearestInt());
    }

    TishinaProcessor& proc;
    Backdrop backdrop;
    Footer footerBar;
    ProgramBox program;
    ResonanceScreen screen;
    NeedleMeter meter;
    NixieReadout peakCut, outPeak;
    Knob depth, sharpness, selectivity, attack, release, mix, output, low, high, emphasis;
    RotarySwitch mode, quality;
    Lever soft, delta;
    float holdCut = 0.0f, holdOut = -120.0f;
    int slowCounter = 0;
};

//==============================================================================
TishinaEditor::TishinaEditor (TishinaProcessor& p) : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    content = std::make_unique<Content> (p);
    addAndMakeVisible (*content);

    const double scale = juce::jlimit (0.55, 1.6, (double) p.uiState().getProperty ("scale", 0.9));
    setResizable (true, true);
    setResizeLimits ((int) (kBaseW * 0.55), (int) (kBaseH * 0.55), (int) (kBaseW * 1.6), (int) (kBaseH * 1.6));
    getConstrainer()->setFixedAspectRatio ((double) kBaseW / (double) kBaseH);
    setSize (juce::roundToInt (kBaseW * scale), juce::roundToInt (kBaseH * scale));
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

TishinaEditor::~TishinaEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TishinaEditor::paint (juce::Graphics& g) { g.fillAll (col::room); }

void TishinaEditor::resized()
{
    const float scale = (float) getWidth() / (float) kBaseW;
    content->setTransform (juce::AffineTransform::scale (scale));
    content->setBounds (0, 0, kBaseW, kBaseH);
    proc.uiState().setProperty ("scale", scale, nullptr);
}

void TishinaEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.005, 0.2, (now - lastTick) * 0.001);
    lastTick = now;
    content->tick (dt);
}
