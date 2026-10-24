#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Presets.h"

class TishinaProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    TishinaProcessor();
    ~TishinaProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TISHINA"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return (int) factoryPresets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override { loadPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    kv::Suppressor& engine() { return eng; }
    double currentSampleRate() const { return sampleRateUsed.load(); }
    void loadPreset (int index);

    /** Applies a pending quality change now (the host normally gets it via the message thread). */
    void applyQualityChange();

    kv::AtomicMax inPeak, outPeak;

    /** Editor settings that travel with the session (window scale). */
    juce::ValueTree uiState() { return apvts.state.getOrCreateChildWithName ("UI", nullptr); }

private:
    void handleAsyncUpdate() override { applyQualityChange(); }
    void rebuild (double sampleRate, int quality);

    kv::Suppressor eng;
    params::Cache cache;
    kv::SuppressorParams sp;
    std::atomic<double> sampleRateUsed { 48000.0 };
    std::atomic<int> preparedQuality { -1 };
    bool prepared = false;
    int currentProgram = 0;

    juce::AudioBuffer<float> bypassDelay;
    int bypassPos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TishinaProcessor)
};
