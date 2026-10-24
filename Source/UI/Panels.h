#pragma once
// The five station panels, the conveyor strip and the output column.

#include "../PluginProcessor.h"
#include "Widgets.h"
#include "Crt.h"

namespace ui
{
/** Everything the meters need for one repaint, collected once per timer tick. */
struct MeterFrame
{
    float dt = 1.0f / 30.0f;
    float inL = -120.0f, inR = -120.0f, outL = -120.0f, outR = -120.0f;
    float compGr = 0.0f, compLevel = -100.0f;
    std::array<float, 4> mbcGr {};
    float limGr = 0.0f;
    float correlation = 1.0f;
    float lufsM = -100.0f, lufsS = -100.0f, lufsI = -100.0f, tpMax = -100.0f;
    std::array<float, kv::kEqBands> eqDyn {};
};

/** Common base for the station panels. */
class StationPanel : public juce::Component
{
public:
    explicit StationPanel (KonveyerProcessor& p) : proc (p) {}
    virtual void refresh (const MeterFrame&) = 0;
    virtual juce::String hint() const { return {}; }
    void paint (juce::Graphics&) override;

protected:
    /** Paints a recessed sub-panel with a small engraved title in its top-left corner. */
    void paintSection (juce::Graphics&, juce::Rectangle<float> r, const juce::String& en, const juce::String& ru) const;
    KonveyerProcessor& proc;
};

//==============================================================================
class EqScreen : public CrtScreen
{
public:
    explicit EqScreen (KonveyerProcessor&);

    void refresh (const MeterFrame&);
    void setSelected (int band);
    int getSelected() const { return selected; }
    std::function<void (int)> onSelectionChanged;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    int analyserMode = 2;  // 0 off, 1 pre, 2 post, 3 pre + post
    float range = 12.0f;

private:
    struct BandView
    {
        bool on = false;
        kv::EqType type = kv::EqType::Bell;
        float freq = 1000.0f, gain = 0.0f, q = 0.71f, thr = -30.0f, dynRange = -6.0f;
        int slope = 1;
        kv::Placement place = kv::Placement::Stereo;
        bool dyn = false;
    };

    void paintStatic (juce::Graphics&, juce::Rectangle<float>) override;
    void paintScreen (juce::Graphics&, juce::Rectangle<float>) override;
    BandView band (int b) const;
    float raw (int b, const char* field) const;
    void setValue (int b, const char* field, float v);
    void setComplete (int b, const char* field, float v);
    void gesture (int b, const char* field, bool begin);
    float xOf (float f) const;
    float fOf (float x) const;
    float yOf (float db) const;
    float dbOf (float y) const;
    juce::Point<float> nodePos (int b) const;
    int hit (juce::Point<float>) const;
    void showMenu (int b);
    void addBandAt (juce::Point<float>);

    KonveyerProcessor& proc;
    SpectrumAnalyser pre, post;
    FreqAxis axis;
    std::array<float, kv::kEqBands> dyn {};
    int selected = 0, hover = -1, dragging = -1;
    float dragF = 0.0f, dragG = 0.0f, dragQ = 0.0f;
    juce::Point<float> dragStart;
    juce::Rectangle<float> analyserButton, rangeButton;
};

class EqPanel : public StationPanel
{
public:
    explicit EqPanel (KonveyerProcessor&);
    void refresh (const MeterFrame&) override;
    juce::String hint() const override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void selectBand (int b);
    int selectedBand() const { return band; }
    EqScreen screen;

private:
    void updateEnablement();

    std::array<std::unique_ptr<PushButton>, kv::kEqBands> bandButtons;
    Lever bandOn { "BAND ON", juce::CharPointer_UTF8 ("\xd0\x92\xd0\x9a\xd0\x9b") };
    PianoKeys typeKeys;
    Knob freq, gain, q;
    RotarySwitch slope, place;
    Lever dynOn;
    Knob thr, range, atk, rel, out;
    int band = 0;
    juce::Rectangle<float> secBand, secFilter, secShape, secDyn;
};

//==============================================================================
class TransferScreen : public CrtScreen
{
public:
    explicit TransferScreen (KonveyerProcessor& p) : proc (p) { bezelLabel = juce::CharPointer_UTF8 ("\xd0\xa5\xd0\x90\xd0\xa0\xd0\x90\xd0\x9a\xd0\xa2\xd0\x95\xd0\xa0\xd0\x98\xd0\xa1\xd0\xa2\xd0\x98\xd0\x9a\xd0\x90"); }
    void refresh (const MeterFrame&);

private:
    void paintStatic (juce::Graphics&, juce::Rectangle<float>) override;
    void paintScreen (juce::Graphics&, juce::Rectangle<float>) override;
    KonveyerProcessor& proc;
    float level = -100.0f, gr = 0.0f;
};

class CompPanel : public StationPanel
{
public:
    explicit CompPanel (KonveyerProcessor&);
    void refresh (const MeterFrame&) override;
    juce::String hint() const override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    TransferScreen transfer;
    NeedleMeter meter;
    Lever autoRel, rms;
    Knob thr, ratio, atk, rel, knee, makeup, mix, hpf;
    juce::Rectangle<float> secSwitches, secKnobs;
};

//==============================================================================
class MbcScreen : public CrtScreen
{
public:
    explicit MbcScreen (KonveyerProcessor&);
    void refresh (const MeterFrame&);
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void paintStatic (juce::Graphics&, juce::Rectangle<float>) override;
    void paintScreen (juce::Graphics&, juce::Rectangle<float>) override;
    int hitXover (float x) const;
    float xover (int k) const;
    KonveyerProcessor& proc;
    SpectrumAnalyser spec;
    FreqAxis axis;
    std::array<float, 4> gr {};
    int dragging = -1, hover = -1;
};

class MbcPanel : public StationPanel
{
public:
    explicit MbcPanel (KonveyerProcessor&);
    void refresh (const MeterFrame&) override;
    juce::String hint() const override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Column
    {
        std::unique_ptr<NeedleMeter> meter;
        std::unique_ptr<Knob> thr, ratio, atk, rel, gain;
        std::unique_ptr<PushButton> solo, bypass;
        juce::Rectangle<float> area;
    };
    MbcScreen screen;
    std::array<Column, 4> cols;
};

//==============================================================================
class Vectorscope : public CrtScreen
{
public:
    explicit Vectorscope (KonveyerProcessor&);
    void refresh (const MeterFrame&);

private:
    void paintStatic (juce::Graphics&, juce::Rectangle<float>) override;
    void paintScreen (juce::Graphics&, juce::Rectangle<float>) override;
    KonveyerProcessor& proc;
    std::vector<float> pts;
    juce::Image trail;
    uint64_t lastWritten = 0;
};

class WidthScreen : public CrtScreen
{
public:
    explicit WidthScreen (KonveyerProcessor&);
    void refresh (const MeterFrame&) { repaint(); }
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void paintStatic (juce::Graphics&, juce::Rectangle<float>) override;
    void paintScreen (juce::Graphics&, juce::Rectangle<float>) override;
    int hitTarget (juce::Point<float>) const;  // 0..1 crossovers, 2..4 band widths
    float value (const juce::String& id) const;
    KonveyerProcessor& proc;
    FreqAxis axis;
    int dragging = -1, hover = -1;
};

class ImagerPanel : public StationPanel
{
public:
    explicit ImagerPanel (KonveyerProcessor&);
    void refresh (const MeterFrame&) override;
    juce::String hint() const override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Vectorscope scope;
    NeedleMeter corr;
    WidthScreen widths;
    Knob wLow, wMid, wHigh, xLow, xHigh;
    Lever mono;
    juce::Rectangle<float> secKnobs;
};

//==============================================================================
class HistoryScreen : public CrtScreen
{
public:
    explicit HistoryScreen (KonveyerProcessor&);
    void refresh (const MeterFrame&) { repaint(); }

private:
    void paintStatic (juce::Graphics&, juce::Rectangle<float>) override;
    void paintScreen (juce::Graphics&, juce::Rectangle<float>) override;
    KonveyerProcessor& proc;
    std::vector<float> in, out, gr;
};

class LimiterPanel : public StationPanel
{
public:
    explicit LimiterPanel (KonveyerProcessor&);
    void refresh (const MeterFrame&) override;
    juce::String hint() const override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    HistoryScreen history;
    NeedleMeter meter;
    Knob drive, ceiling, rel, look;
    Lever truePeak, autoRel;
    juce::Rectangle<float> secKnobs, secSwitches;
};

//==============================================================================
/** The belt with the five stations. Drag a station to change the order, click to open it. */
class Conveyor : public juce::Component
{
public:
    explicit Conveyor (KonveyerProcessor&);

    void refresh (const MeterFrame&);
    void setSelected (int module);
    std::function<void (int)> onSelect;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> beltArea() const;
    float slotX (int slot) const;
    float plateWidth() const;
    juce::Rectangle<float> plateRect (int module) const;
    int moduleAt (juce::Point<float>) const;
    void placeLevers();
    juce::String status (int module) const;

    KonveyerProcessor& proc;
    std::array<std::unique_ptr<Lever>, kv::kNumModules> levers;
    kv::Order order = kv::defaultOrder();
    std::array<float, kv::kNumModules> x {};
    int selected = 0, dragModule = -1;
    float dragOffset = 0.0f, dragX = 0.0f;
    bool dragMoved = false;
    float beltPhase = 0.0f;
    MeterFrame last;
    float signal = 0.0f;
    std::array<juce::String, kv::kNumModules> lastStatus;
};

//==============================================================================
class OutputPanel : public juce::Component
{
public:
    explicit OutputPanel (KonveyerProcessor&);
    void refresh (const MeterFrame&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    KonveyerProcessor& proc;
    NeedleMeter left, right;
    NixieReadout integrated, shortTerm, momentary, truePeak;
    PushButton reset { "RESET", col::amber, false };
    RotarySwitch dither;
};
} // namespace ui
