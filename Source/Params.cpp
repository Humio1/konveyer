#include "Params.h"

namespace params
{
namespace
{
using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
using Fmt = std::function<juce::String (float)>;

juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
             [] (float s, float e, float t) { return s * std::pow (e / s, t); },
             [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
             [] (float s, float e, float v) { return juce::jlimit (s, e, v); } };
}

juce::String fmtDbSigned (float v) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; }
juce::String fmtMs (float v)       { return juce::String (v, v < 10.0f ? 1 : 0) + " ms"; }
juce::String fmtPercent (float v)  { return juce::String (juce::roundToInt (v)) + " %"; }
juce::String fmtSigned (float v)   { const int i = juce::roundToInt (v); return (i > 0 ? "+" : "") + juce::String (i); }

float parseNumber (const juce::String& text)
{
    const auto t = text.trim().toLowerCase();
    float v = t.getFloatValue();
    if (t.containsChar ('k'))
        v *= 1000.0f;
    return v;
}

void addFloat (Layout& l, const juce::String& id, const juce::String& name, juce::NormalisableRange<float> r, float def, Fmt fmt)
{
    l.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, r, def,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([fmt] (float v, int) { return fmt (v); })
            .withValueFromStringFunction ([] (const juce::String& s) { return parseNumber (s); })));
}
} // namespace

juce::String formatFreq (float v)
{
    if (v >= 1000.0f)
        return juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz";
    return juce::String (v, v < 100.0f ? 1 : 0) + " Hz";
}

const juce::StringArray& modeNames()
{
    static const juce::StringArray s { "Linked", "Stereo", "Mid/Side" };
    return s;
}

const juce::StringArray& qualityNames()
{
    static const juce::StringArray s { "Fast", "Normal", "Fine" };
    return s;
}

Layout createLayout()
{
    Layout l;
    const juce::NormalisableRange<float> pct (0.0f, 100.0f);
    addFloat (l, depth, "Depth", pct, 50.0f, fmtPercent);
    addFloat (l, sharpness, "Sharpness", pct, 50.0f, fmtPercent);
    addFloat (l, selectivity, "Selectivity", pct, 50.0f, fmtPercent);
    addFloat (l, attack, "Attack", logRange (0.5f, 100.0f), 5.0f, fmtMs);
    addFloat (l, release, "Release", logRange (10.0f, 1000.0f), 100.0f, fmtMs);
    addFloat (l, low, "Range Low", logRange (20.0f, 20000.0f), 60.0f, formatFreq);
    addFloat (l, high, "Range High", logRange (20.0f, 20000.0f), 18000.0f, formatFreq);
    addFloat (l, emphasis, "Emphasis", { -100.0f, 100.0f }, 0.0f, fmtSigned);
    addFloat (l, mix, "Mix", pct, 100.0f, fmtPercent);
    addFloat (l, output, "Output", { -24.0f, 12.0f }, 0.0f, fmtDbSigned);
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { mode, 1 }, "Mode", modeNames(), 0));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { quality, 1 }, "Quality", qualityNames(), 1));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { soft, 1 }, "Soft Knee", true));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { delta, 1 }, "Delta", false));
    return l;
}

std::atomic<float>* Cache::get (juce::AudioProcessorValueTreeState& s, const juce::String& id)
{
    auto* p = s.getRawParameterValue (id);
    jassert (p != nullptr);
    return p;
}

void Cache::init (juce::AudioProcessorValueTreeState& s)
{
    const juce::String ids[] = { depth, sharpness, selectivity, attack, release, low, high, emphasis, mix, output, mode, soft, delta };
    for (int i = 0; i < 13; ++i)
        v[i] = get (s, ids[i]);
    q = get (s, params::quality);
}

void Cache::fill (kv::SuppressorParams& p) const
{
    p.depth = v[0]->load();
    p.sharpness = v[1]->load();
    p.selectivity = v[2]->load();
    p.attackMs = v[3]->load();
    p.releaseMs = v[4]->load();
    p.lowHz = v[5]->load();
    p.highHz = v[6]->load();
    p.emphasis = v[7]->load();
    p.mix = v[8]->load();
    p.outputDb = v[9]->load();
    p.mode = (kv::StereoMode) juce::jlimit (0, 2, juce::roundToInt (v[10]->load()));
    p.soft = v[11]->load() > 0.5f;
    p.delta = v[12]->load() > 0.5f;
}
} // namespace params
