#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Presets.h"

class KonveyerProcessor : public juce::AudioProcessor
{
public:
    KonveyerProcessor();
    ~KonveyerProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "KONVEYER"; }
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

    kv::Engine& engine() { return eng; }
    double currentSampleRate() const { return sampleRateUsed.load(); }

    kv::Order getOrder() const { return kv::unpackOrder (orderPacked.load()); }
    void setOrder (const kv::Order& o);

    void loadPreset (int index);

    /** Editor settings that travel with the session (selected station, analyser mode, window scale). */
    juce::ValueTree uiState() { return apvts.state.getOrCreateChildWithName ("UI", nullptr); }

private:
    static juce::String orderToString (const kv::Order&);
    static kv::Order orderFromString (const juce::String&);

    kv::Engine eng;
    params::Cache cache;
    kv::EngineParams ep;
    std::atomic<int> orderPacked { kv::packOrder (kv::defaultOrder()) };
    std::atomic<double> sampleRateUsed { 48000.0 };
    int currentProgram = 0;

    juce::AudioBuffer<float> bypassDelay;
    int bypassPos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KonveyerProcessor)
};
