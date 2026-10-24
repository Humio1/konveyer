#pragma once
// Parameter IDs, ranges and the mapping from the host-visible parameters to the engine.

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/Suppressor.h"

namespace params
{
inline const juce::String depth = "depth";
inline const juce::String sharpness = "sharpness";
inline const juce::String selectivity = "selectivity";
inline const juce::String attack = "attack";
inline const juce::String release = "release";
inline const juce::String low = "range_low";
inline const juce::String high = "range_high";
inline const juce::String emphasis = "emphasis";
inline const juce::String mix = "mix";
inline const juce::String output = "output";
inline const juce::String mode = "mode";
inline const juce::String quality = "quality";
inline const juce::String soft = "soft_knee";
inline const juce::String delta = "delta";

const juce::StringArray& modeNames();
const juce::StringArray& qualityNames();

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
juce::String formatFreq (float hz);

/** Raw parameter pointers, read on the audio thread. */
class Cache
{
public:
    void init (juce::AudioProcessorValueTreeState& state);
    void fill (kv::SuppressorParams& p) const;
    int quality() const { return juce::roundToInt (q->load()); }

private:
    std::atomic<float>* get (juce::AudioProcessorValueTreeState& s, const juce::String& id);
    std::atomic<float>* v[13] {};
    std::atomic<float>* q = nullptr;
};
} // namespace params
