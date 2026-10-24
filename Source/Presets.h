#pragma once

#include <juce_core/juce_core.h>
#include <vector>

struct Preset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;  // parameter id -> real value; the rest stay default
};

const std::vector<Preset>& factoryPresets();
