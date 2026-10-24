#pragma once
// Panel hardware: knobs, levers, push buttons, piano keys, rotary switches, needle meters, nixie readouts.

#include <juce_audio_processors/juce_audio_processors.h>
#include "Style.h"

namespace ui
{
/** Bakelite knob with an engraved caption above and a small readout below. */
class Knob : public juce::Component
{
public:
    enum class Size { Large, Medium, Small };

    Knob (juce::String english, juce::String russian, Size size = Size::Medium, bool bipolar = false);

    void attach (juce::AudioProcessorValueTreeState& state, const juce::String& paramId);
    void detach();

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider;

private:
    juce::String en, ru;
    Size size;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
    juce::Rectangle<float> valueArea;
};

//==============================================================================
/** Bat-handle toggle switch with an optional indicator lamp. */
class Lever : public juce::Button
{
public:
    Lever (juce::String english, juce::String russian, juce::Colour lamp = col::amber, bool showLamp = true);

    void attach (juce::AudioProcessorValueTreeState& state, const juce::String& paramId);
    void detach() { attachment.reset(); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::String en, ru;
    juce::Colour lampColour;
    bool lamp;
    std::unique_ptr<juce::ButtonParameterAttachment> attachment;
};

//==============================================================================
/** Square push button with a lit lens; latching or momentary. */
class PushButton : public juce::Button
{
public:
    PushButton (juce::String buttonText, juce::Colour lit = col::amber, bool latching = true);

    void attach (juce::AudioProcessorValueTreeState& state, const juce::String& paramId);
    void detach() { attachment.reset(); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    /** For radio-style use without a parameter (e.g. band selection). */
    bool lampOn = false;

private:
    juce::Colour litColour;
    std::unique_ptr<juce::ButtonParameterAttachment> attachment;
};

//==============================================================================
/** Row of tape-recorder style piano keys, one per choice of a choice parameter. */
class PianoKeys : public juce::Component, public juce::SettableTooltipClient
{
public:
    using IconPainter = std::function<void (juce::Graphics&, int index, juce::Rectangle<float> face, juce::Colour ink)>;

    PianoKeys (int count, IconPainter painter);

    void attach (juce::AudioProcessorValueTreeState& state, const juce::String& paramId);
    void detach();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    int selected = 0;
    std::function<void (int)> onChange;
    juce::StringArray tooltips;

private:
    int keyAt (juce::Point<float>) const;
    int count;
    IconPainter painter;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

//==============================================================================
/** Chicken-head rotary switch with its positions printed around it. */
class RotarySwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    RotarySwitch (juce::String english, juce::String russian, juce::StringArray labels, float sweepDegrees = 240.0f);

    void attach (juce::AudioProcessorValueTreeState& state, const juce::String& paramId);
    void detach();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    int index = 0;
    std::function<void (int)> onChange;

private:
    void setIndex (int i, bool notify);
    float angleFor (int i) const;
    juce::Point<float> knobCentre() const;
    float knobRadius() const;

    juce::String en, ru;
    juce::StringArray labels;
    float sweep;
    int dragStartIndex = 0;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

//==============================================================================
/** Panel meter: cream face, black needle, red zone. Ballistics live here. */
class NeedleMeter : public juce::Component
{
public:
    struct Scale
    {
        float minValue = -20.0f, maxValue = 0.0f;
        std::vector<std::pair<float, juce::String>> marks;
        std::vector<float> minorMarks;
        float redFrom = 1.0e9f;      // values beyond this are in the red arc
        bool redBelow = false;       // red arc below redFrom instead of above
        juce::String unit = juce::CharPointer_UTF8 ("\xd0\xb4\xd0\x91"); // "дБ"
        juce::String title;
        std::function<float (float)> position;  // value -> 0..1 along the arc (default: linear)
        float attackMs = 10.0f, releaseMs = 300.0f;
        bool peakHold = false;
    };

    explicit NeedleMeter (Scale s);
    void setTarget (float v) { target = v; }
    void tick (float dtSeconds);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    float posOf (float v) const;
    Scale scale;
    float value, target, hold, holdTimer = 0.0f;
    float painted = -1.0e9f, paintedHold = -1.0e9f;
    LayerCache faceCache, glassCache;
};

//==============================================================================
/** Row of nixie tubes showing a number with one decimal. */
class NixieReadout : public juce::Component
{
public:
    NixieReadout (juce::String title, juce::String russian, juce::String unit, int digits = 3, bool showSign = true);
    void setValue (float v, bool valid = true);
    void paint (juce::Graphics&) override;
    juce::Colour titleColour = col::cream;

private:
    juce::String title, ru, unit;
    int digits;
    bool sign;
    float value = 0.0f;
    bool valid = false;
};
} // namespace ui
