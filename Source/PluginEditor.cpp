#include "PluginEditor.h"

using namespace ui;

namespace
{
/** Preset window: black slot with the program name, arrows either side, menu on click. */
class ProgramBox : public juce::Component
{
public:
    explicit ProgramBox (KonveyerProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

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
    KonveyerProcessor& proc;
    juce::Rectangle<float> prevArea, nextArea;
};

/** Everything static behind the controls, rendered once into a cached image. */
class Backdrop : public juce::Component
{
public:
    Backdrop()
    {
        setBufferedToImage (true);
        setInterceptsMouseClicks (false, false);
    }

    juce::Rectangle<float> header, convArea, mainArea, sideArea, footer;

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
        auto logoPlate = juce::Rectangle<float> (16.0f, 11.0f, 318.0f, 54.0f);
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
        g.drawText ("KONVEYER", logoPlate.withTrimmedLeft (16.0f).translated (1.5f, 2.0f), juce::Justification::centredLeft, false);
        g.setColour (col::cream);
        g.drawText ("KONVEYER", logoPlate.withTrimmedLeft (16.0f), juce::Justification::centredLeft, false);

        auto sub = juce::Rectangle<float> (logoPlate.getRight() + 14.0f, 18.0f, 220.0f, 40.0f);
        engraved (g, "MASTERING LINE", sub.withHeight (20.0f), label (16.0f, 0.18f), juce::Justification::centredLeft);
        engraved (g, cyr ("ЛИНИЯ МАСТЕРИНГА  ·  5 СТАНЦИЙ"), sub.withTrimmedTop (21.0f), light (11.5f, 0.12f), juce::Justification::centredLeft, col::cream.withAlpha (0.7f));

        auto tp = juce::Rectangle<float> (header.getRight() - 236.0f, 13.0f, 220.0f, 50.0f);
        paintPlate (g, tp, juce::Colour (0xffbfc4c0), true);
        inked (g, cyr ("ИЗДЕЛИЕ  №  147"), tp.withHeight (25.0f).translated (0.0f, 4.0f), label (13.0f, 0.16f));
        inked (g, "MODEL KV-5" + dot() + "SER. 0001" + dot() + "2026", tp.withTrimmedTop (25.0f).translated (0.0f, -4.0f), light (10.5f, 0.1f), juce::Justification::centred, col::inkSoft);

        paintEnamel (g, convArea.reduced (4.0f, 4.0f), col::enamelDark, 6.0f);
        for (auto p : { convArea.getTopLeft() + juce::Point<float> (14, 14), convArea.getBottomLeft() + juce::Point<float> (14, -14),
                        convArea.getTopRight() + juce::Point<float> (-14, 14), convArea.getBottomRight() + juce::Point<float> (-14, -14) })
            paintScrew (g, p, 4.5f, p.x * 0.03f);

        paintEnamel (g, mainArea, col::enamel, 6.0f);
        paintEnamel (g, sideArea, col::enamel.darker (0.08f), 6.0f);
        for (auto a : { mainArea, sideArea })
            for (auto p : { a.getTopLeft() + juce::Point<float> (9, 9), a.getTopRight() + juce::Point<float> (-9, 9),
                            a.getBottomLeft() + juce::Point<float> (9, -9), a.getBottomRight() + juce::Point<float> (-9, -9) })
                paintScrew (g, p, 4.0f, p.y * 0.05f);

        g.setColour (col::frame);
        g.fillRect (footer);
        paintTexture (g, footer.toNearestInt(), 0.2f);
    }
};

/** Status line: hint for the open station, sample rate and latency. */
class Footer : public juce::Component
{
public:
    explicit Footer (KonveyerProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }
    juce::String hint;

    void paint (juce::Graphics& g) override
    {
        auto f = getLocalBounds().toFloat().reduced (16.0f, 0.0f);
        const double sr = proc.currentSampleRate();
        const juce::String right = "KV-5 v" JucePlugin_VersionString + dot() + juce::String (sr / 1000.0, 1) + " kHz" + dot()
                                 + "LATENCY " + juce::String (proc.getLatencySamples() * 1000.0 / sr, 1) + " ms";
        engraved (g, right, f.removeFromRight (300.0f), mono (11.5f), juce::Justification::centredRight, col::cream.withAlpha (0.6f));
        engraved (g, hint, f, regular (12.0f, 0.03f), juce::Justification::centredLeft, col::cream.withAlpha (0.75f));
    }

private:
    KonveyerProcessor& proc;
};
} // namespace

//==============================================================================
class KonveyerEditor::Content : public juce::Component
{
public:
    explicit Content (KonveyerProcessor& p)
        : proc (p), footerBar (p), conveyor (p), program (p), output (p),
          inGain ("INPUT", cyr ("ВХОД"), Knob::Size::Medium, true),
          outGain ("OUTPUT", cyr ("ВЫХОД"), Knob::Size::Medium, true)
    {
        addAndMakeVisible (backdrop);
        addAndMakeVisible (footerBar);
        panels[kv::kModEq] = std::make_unique<EqPanel> (p);
        panels[kv::kModComp] = std::make_unique<CompPanel> (p);
        panels[kv::kModMbc] = std::make_unique<MbcPanel> (p);
        panels[kv::kModImager] = std::make_unique<ImagerPanel> (p);
        panels[kv::kModLimiter] = std::make_unique<LimiterPanel> (p);
        for (auto& panel : panels)
            addChildComponent (*panel);
        for (juce::Component* c : std::initializer_list<juce::Component*> { &conveyor, &program, &output, &inGain, &outGain })
            addAndMakeVisible (c);
        inGain.attach (p.apvts, params::inGain);
        outGain.attach (p.apvts, params::outGain);
        conveyor.onSelect = [this] (int m) { select (m); };
        select (juce::jlimit (0, kv::kNumModules - 1, (int) p.uiState().getProperty ("module", 0)));
    }

    void select (int m)
    {
        module = juce::jlimit (0, kv::kNumModules - 1, m);
        proc.uiState().setProperty ("module", module, nullptr);
        for (int i = 0; i < kv::kNumModules; ++i)
            panels[(size_t) i]->setVisible (i == module);
        conveyor.setSelected (module);
        footerBar.hint = panels[(size_t) module]->hint();
        footerBar.repaint();
    }

    void tick (const MeterFrame& f)
    {
        conveyor.refresh (f);
        panels[(size_t) module]->refresh (f);
        output.refresh (f);
        if (++slowCounter % 15 == 0)
        {
            program.repaint();
            footerBar.repaint();
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().toFloat();
        header = r.removeFromTop (76.0f);
        convArea = r.removeFromTop (146.0f);
        footer = r.removeFromBottom (30.0f);
        mainArea = r.reduced (8.0f, 6.0f);
        sideArea = mainArea.removeFromRight (276.0f);
        mainArea.removeFromRight (8.0f);

        backdrop.header = header;
        backdrop.convArea = convArea;
        backdrop.mainArea = mainArea;
        backdrop.sideArea = sideArea;
        backdrop.footer = footer;
        backdrop.setBounds (getLocalBounds());
        footerBar.setBounds (footer.toNearestInt());

        program.setBounds (juce::Rectangle<float> (header.getCentreX() - 40.0f, header.getY() + 8.0f, 330.0f, 60.0f).toNearestInt());
        auto c = convArea.reduced (8.0f, 8.0f);
        inGain.setBounds (c.removeFromLeft (108.0f).reduced (4.0f, 6.0f).toNearestInt());
        outGain.setBounds (c.removeFromRight (108.0f).reduced (4.0f, 6.0f).toNearestInt());
        conveyor.setBounds (c.reduced (6.0f, 0.0f).toNearestInt());
        for (auto& panel : panels)
            panel->setBounds (mainArea.toNearestInt());
        output.setBounds (sideArea.toNearestInt());
    }

    KonveyerProcessor& proc;
    Backdrop backdrop;
    Footer footerBar;
    Conveyor conveyor;
    ProgramBox program;
    OutputPanel output;
    Knob inGain, outGain;
    std::array<std::unique_ptr<StationPanel>, kv::kNumModules> panels;
    int module = 0;
    int slowCounter = 0;
    juce::Rectangle<float> header, convArea, mainArea, sideArea, footer;
};

//==============================================================================
KonveyerEditor::KonveyerEditor (KonveyerProcessor& p) : juce::AudioProcessorEditor (p), proc (p)
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

KonveyerEditor::~KonveyerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void KonveyerEditor::paint (juce::Graphics& g) { g.fillAll (col::room); }

void KonveyerEditor::resized()
{
    const float scale = (float) getWidth() / (float) kBaseW;
    content->setTransform (juce::AffineTransform::scale (scale));
    content->setBounds (0, 0, kBaseW, kBaseH);
    proc.uiState().setProperty ("scale", scale, nullptr);
}

void KonveyerEditor::selectModule (int module) { content->select (module); }

void KonveyerEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    MeterFrame f;
    f.dt = (float) juce::jlimit (0.005, 0.2, (now - lastTick) * 0.001);
    lastTick = now;

    auto& e = proc.engine();
    f.inL = std::max (-120.0f, e.meters.inL.take());
    f.inR = std::max (-120.0f, e.meters.inR.take());
    f.outL = std::max (-120.0f, e.meters.outL.take());
    f.outR = std::max (-120.0f, e.meters.outR.take());
    f.compGr = std::max (0.0f, e.getComp().grMeter.take (0.0f));
    f.compLevel = e.getComp().levelDb.load();
    for (size_t b = 0; b < 4; ++b)
        f.mbcGr[b] = std::max (0.0f, e.getMbc().grMeter[b].take (0.0f));
    f.limGr = std::max (0.0f, e.getLimiter().grMeter.take (0.0f));
    f.correlation = e.getImager().correlation.load();
    auto& l = e.getLoudness();
    f.lufsM = l.momentary.load();
    f.lufsS = l.shortTerm.load();
    f.lufsI = l.integrated.load();
    f.tpMax = l.truePeakMax.load();
    for (int b = 0; b < kv::kEqBands; ++b)
        f.eqDyn[(size_t) b] = e.getEq().dynamicGainDb (b);

    content->tick (f);
}
