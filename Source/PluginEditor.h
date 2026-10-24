#pragma once

#include "PluginProcessor.h"
#include "UI/LookAndFeel.h"

class TishinaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TishinaEditor (TishinaProcessor&);
    ~TishinaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Runs one display update without waiting for the timer (snapshot tool and tests). */
    void tick() { timerCallback(); }

    static constexpr int kBaseW = 1160, kBaseH = 760;

private:
    class Content;
    void timerCallback() override;

    TishinaProcessor& proc;
    KonveyerLookAndFeel lnf;
    std::unique_ptr<Content> content;
    juce::TooltipWindow tooltips { this, 700 };
    double lastTick = 0.0;
};
