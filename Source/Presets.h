#pragma once
// Factory programs. Values are plain parameter values (dB, Hz, ms, %, choice index, 0/1).

#include "Params.h"

struct Preset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;
    kv::Order order = kv::defaultOrder();
};

const std::vector<Preset>& factoryPresets();
