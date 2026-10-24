#pragma once
// Parameter IDs, ranges and the mapping from the host-visible parameters to the engine.

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/Engine.h"

namespace params
{
inline const juce::String inGain = "in_gain";
inline const juce::String outGain = "out_gain";
inline const juce::String dither = "dither";

juce::String moduleOn (int module);
juce::String eq (int band, const char* field);  // fields: on type freq gain q slope place dyn thr range
inline const juce::String eqOut = "eq_out";
inline const juce::String eqDynAtk = "eq_dyn_atk";
inline const juce::String eqDynRel = "eq_dyn_rel";

inline const juce::String compThr = "comp_thr";
inline const juce::String compRatio = "comp_ratio";
inline const juce::String compAtk = "comp_atk";
inline const juce::String compRel = "comp_rel";
inline const juce::String compKnee = "comp_knee";
inline const juce::String compMakeup = "comp_makeup";
inline const juce::String compMix = "comp_mix";
inline const juce::String compHpf = "comp_hpf";
inline const juce::String compAuto = "comp_auto";
inline const juce::String compRms = "comp_rms";

juce::String mbcX (int k);                     // k = 0..2
juce::String mbc (int band, const char* field); // fields: thr ratio atk rel gain solo byp

juce::String imgX (int k);   // k = 0..1
juce::String imgW (int band); // band = 0..2
inline const juce::String imgMono = "img_mono";

inline const juce::String limDrive = "lim_drive";
inline const juce::String limCeil = "lim_ceil";
inline const juce::String limRel = "lim_rel";
inline const juce::String limLook = "lim_look";
inline const juce::String limAuto = "lim_auto";
inline const juce::String limTp = "lim_tp";

const juce::StringArray& eqTypeNames();
const juce::StringArray& slopeNames();
const juce::StringArray& placementNames();
const juce::StringArray& moduleNames();       // "Equalizer", ...
const juce::StringArray& moduleShortIds();     // "eq", "comp", ...

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

/** Default frequency for an EQ band slot (spread across the spectrum). */
double defaultBandFreq (int band);

/** Raw parameter pointers, read on the audio thread. */
class Cache
{
public:
    void init (juce::AudioProcessorValueTreeState& state);
    void fill (kv::EngineParams& p) const;

private:
    struct Band { std::atomic<float>* f[10] {}; };
    std::atomic<float>* get (juce::AudioProcessorValueTreeState& s, const juce::String& id);

    std::atomic<float>* in = nullptr; std::atomic<float>* out = nullptr; std::atomic<float>* dith = nullptr;
    std::array<std::atomic<float>*, kv::kNumModules> on {};
    std::array<Band, kv::kEqBands> bands {};
    std::atomic<float>* eqOutP = nullptr; std::atomic<float>* eqAtk = nullptr; std::atomic<float>* eqRel = nullptr;
    std::atomic<float>* c[10] {};
    std::array<std::atomic<float>*, 3> mx {};
    std::array<std::array<std::atomic<float>*, 7>, 4> mb {};
    std::array<std::atomic<float>*, 2> ix {};
    std::array<std::atomic<float>*, 3> iw {};
    std::atomic<float>* imono = nullptr;
    std::atomic<float>* l[6] {};
};
} // namespace params
