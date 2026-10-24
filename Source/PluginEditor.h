#pragma once

#include "PluginProcessor.h"
#include "UI/LookAndFeel.h"
#include "UI/Panels.h"

class KonveyerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit KonveyerEditor (KonveyerProcessor&);
    ~KonveyerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Opens a station's panel (also used by the snapshot tool). */
    void selectModule (int module);
    /** Runs one meter update without waiting for the timer (snapshot tool). */
    void tick() { timerCallback(); }

    static constexpr int kBaseW = 1200, kBaseH = 800;

private:
    class Content;
    void timerCallback() override;

    KonveyerProcessor& proc;
    KonveyerLookAndFeel lnf;
    std::unique_ptr<Content> content;
    juce::TooltipWindow tooltips { this, 700 };
    double lastTick = 0.0;
};
