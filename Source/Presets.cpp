#include "Presets.h"
#include "Params.h"

const std::vector<Preset>& factoryPresets()
{
    using namespace params;
    static const std::vector<Preset> list {
        { "Init", {} },
        { "Pad Gentle", { { depth, 35 }, { sharpness, 40 }, { selectivity, 60 }, { attack, 10 }, { release, 160 }, { low, 120 }, { high, 12000 } } },
        { "Pad Smoother", { { depth, 60 }, { sharpness, 55 }, { selectivity, 45 }, { attack, 8 }, { release, 130 }, { low, 150 }, { high, 16000 } } },
        { "Pad Glassy Highs", { { depth, 55 }, { sharpness, 65 }, { selectivity, 40 }, { attack, 4 }, { release, 90 }, { low, 2000 }, { high, 18000 }, { emphasis, 40 } } },
        { "Pad Warm Low-Mids", { { depth, 55 }, { sharpness, 45 }, { selectivity, 45 }, { attack, 12 }, { release, 220 }, { low, 150 }, { high, 1200 }, { emphasis, -40 } } },
        { "Pad Wide Sides", { { depth, 50 }, { sharpness, 55 }, { selectivity, 45 }, { attack, 8 }, { release, 140 }, { low, 300 }, { high, 16000 }, { mode, 2 } } },
        { "Harsh Synth Lead", { { depth, 65 }, { sharpness, 70 }, { selectivity, 35 }, { attack, 2 }, { release, 60 }, { low, 1500 }, { high, 12000 } } },
        { "Vocal De-Ring", { { depth, 50 }, { sharpness, 60 }, { selectivity, 50 }, { attack, 3 }, { release, 80 }, { low, 300 }, { high, 9000 } } },
        { "Room Resonances", { { depth, 70 }, { sharpness, 80 }, { selectivity, 40 }, { attack, 20 }, { release, 300 }, { low, 40 }, { high, 500 }, { quality, 2 } } },
        { "Mix Bus Polish", { { depth, 25 }, { sharpness, 50 }, { selectivity, 60 }, { attack, 10 }, { release, 120 }, { mix, 70 } } },
        { "Heavy Tame", { { depth, 90 }, { sharpness, 50 }, { selectivity, 25 }, { attack, 3 }, { release, 100 } } },
        { "Low Latency", { { depth, 45 }, { sharpness, 50 }, { selectivity, 50 }, { quality, 0 } } },
    };
    return list;
}
