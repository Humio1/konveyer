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

juce::NormalisableRange<float> linRange (float lo, float hi, float step = 0.0f) { return { lo, hi, step }; }

juce::String fmtFreq (float v)
{
    if (v >= 1000.0f)
        return juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz";
    return juce::String (v, v < 100.0f ? 1 : 0) + " Hz";
}
juce::String fmtDb (float v)       { return juce::String (v, 1) + " dB"; }
juce::String fmtDbSigned (float v) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; }
juce::String fmtMs (float v)       { return juce::String (v, v < 10.0f ? 1 : 0) + " ms"; }
juce::String fmtRatio (float v)    { return juce::String (v, v < 10.0f ? 1 : 0) + ":1"; }
juce::String fmtPercent (float v)  { return juce::String (juce::roundToInt (v)) + " %"; }
juce::String fmtQ (float v)        { return juce::String (v, v < 10.0f ? 2 : 1); }

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

void addChoice (Layout& l, const juce::String& id, const juce::String& name, const juce::StringArray& items, int def)
{
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def));
}

void addBool (Layout& l, const juce::String& id, const juce::String& name, bool def)
{
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
}
} // namespace

const juce::StringArray& eqTypeNames()
{
    static const juce::StringArray s { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch", "Tilt Shelf" };
    return s;
}

const juce::StringArray& slopeNames()
{
    static const juce::StringArray s { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" };
    return s;
}

const juce::StringArray& placementNames()
{
    static const juce::StringArray s { "Stereo", "Mid", "Side", "Left", "Right" };
    return s;
}

const juce::StringArray& moduleNames()
{
    static const juce::StringArray s { "Equalizer", "Compressor", "Multiband", "Stereo", "Limiter" };
    return s;
}

const juce::StringArray& moduleShortIds()
{
    static const juce::StringArray s { "eq", "comp", "mbc", "img", "lim" };
    return s;
}

juce::String moduleOn (int m) { return "on_" + moduleShortIds()[m]; }
juce::String eq (int band, const char* field) { return "eq" + juce::String (band + 1) + "_" + field; }
juce::String mbcX (int k) { return "mbc_x" + juce::String (k + 1); }
juce::String mbc (int band, const char* field) { return "mbc" + juce::String (band + 1) + "_" + field; }
juce::String imgX (int k) { return "img_x" + juce::String (k + 1); }
juce::String imgW (int band) { return "img_w" + juce::String (band + 1); }

double defaultBandFreq (int band) { return 40.0 * std::pow (16000.0 / 40.0, band / (double) (kv::kEqBands - 1)); }

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    Layout l;

    addFloat (l, inGain, "Input Gain", linRange (-24.0f, 24.0f), 0.0f, fmtDbSigned);
    addFloat (l, outGain, "Output Gain", linRange (-24.0f, 24.0f), 0.0f, fmtDbSigned);
    addChoice (l, dither, "Dither", { "Off", "24 bit", "16 bit" }, 0);
    for (int m = 0; m < kv::kNumModules; ++m)
        addBool (l, moduleOn (m), moduleNames()[m] + " On", true);

    // Equalizer
    for (int b = 0; b < kv::kEqBands; ++b)
    {
        const juce::String n = "EQ " + juce::String (b + 1) + " ";
        addBool (l, eq (b, "on"), n + "On", false);
        addChoice (l, eq (b, "type"), n + "Type", eqTypeNames(), 0);
        addFloat (l, eq (b, "freq"), n + "Freq", logRange (10.0f, 30000.0f), (float) defaultBandFreq (b), fmtFreq);
        addFloat (l, eq (b, "gain"), n + "Gain", linRange (-30.0f, 30.0f), 0.0f, fmtDbSigned);
        addFloat (l, eq (b, "q"), n + "Q", logRange (0.025f, 40.0f), 0.71f, fmtQ);
        addChoice (l, eq (b, "slope"), n + "Slope", slopeNames(), 1);
        addChoice (l, eq (b, "place"), n + "Placement", placementNames(), 0);
        addBool (l, eq (b, "dyn"), n + "Dynamic", false);
        addFloat (l, eq (b, "thr"), n + "Dyn Threshold", linRange (-60.0f, 0.0f), -30.0f, fmtDb);
        addFloat (l, eq (b, "range"), n + "Dyn Range", linRange (-24.0f, 24.0f), -6.0f, fmtDbSigned);
    }
    addFloat (l, eqOut, "EQ Output", linRange (-24.0f, 24.0f), 0.0f, fmtDbSigned);
    addFloat (l, eqDynAtk, "EQ Dyn Attack", logRange (0.5f, 200.0f), 5.0f, fmtMs);
    addFloat (l, eqDynRel, "EQ Dyn Release", logRange (10.0f, 2000.0f), 120.0f, fmtMs);

    // Compressor
    addFloat (l, compThr, "Comp Threshold", linRange (-60.0f, 0.0f), 0.0f, fmtDb);
    addFloat (l, compRatio, "Comp Ratio", logRange (1.0f, 20.0f), 2.0f, fmtRatio);
    addFloat (l, compAtk, "Comp Attack", logRange (0.1f, 200.0f), 30.0f, fmtMs);
    addFloat (l, compRel, "Comp Release", logRange (10.0f, 3000.0f), 200.0f, fmtMs);
    addFloat (l, compKnee, "Comp Knee", linRange (0.0f, 24.0f), 6.0f, fmtDb);
    addFloat (l, compMakeup, "Comp Makeup", linRange (-12.0f, 24.0f), 0.0f, fmtDbSigned);
    addFloat (l, compMix, "Comp Mix", linRange (0.0f, 100.0f), 100.0f, fmtPercent);
    addFloat (l, compHpf, "Comp Sidechain HPF", logRange (20.0f, 500.0f), 20.0f,
              [] (float v) { return v <= 20.5f ? juce::String ("Off") : fmtFreq (v); });
    addBool (l, compAuto, "Comp Auto Release", false);
    addBool (l, compRms, "Comp RMS Detection", false);

    // Multiband
    const float xLo[3] = { 20.0f, 100.0f, 1000.0f }, xHi[3] = { 1000.0f, 8000.0f, 18000.0f }, xDef[3] = { 120.0f, 1000.0f, 6000.0f };
    for (int k = 0; k < 3; ++k)
        addFloat (l, mbcX (k), "MB Crossover " + juce::String (k + 1), logRange (xLo[k], xHi[k]), xDef[k], fmtFreq);
    const float atk[4] = { 30.0f, 20.0f, 10.0f, 5.0f }, rel[4] = { 200.0f, 150.0f, 100.0f, 80.0f };
    for (int b = 0; b < 4; ++b)
    {
        const juce::String n = "MB " + juce::String (b + 1) + " ";
        addFloat (l, mbc (b, "thr"), n + "Threshold", linRange (-60.0f, 0.0f), 0.0f, fmtDb);
        addFloat (l, mbc (b, "ratio"), n + "Ratio", logRange (1.0f, 20.0f), 2.0f, fmtRatio);
        addFloat (l, mbc (b, "atk"), n + "Attack", logRange (0.1f, 200.0f), atk[b], fmtMs);
        addFloat (l, mbc (b, "rel"), n + "Release", logRange (10.0f, 2000.0f), rel[b], fmtMs);
        addFloat (l, mbc (b, "gain"), n + "Gain", linRange (-12.0f, 12.0f), 0.0f, fmtDbSigned);
        addBool (l, mbc (b, "solo"), n + "Solo", false);
        addBool (l, mbc (b, "byp"), n + "Bypass", false);
    }

    // Stereo imager
    addFloat (l, imgX (0), "Stereo Crossover Low", logRange (20.0f, 1000.0f), 150.0f, fmtFreq);
    addFloat (l, imgX (1), "Stereo Crossover High", logRange (1000.0f, 16000.0f), 5000.0f, fmtFreq);
    const char* bandNames[3] = { "Low", "Mid", "High" };
    for (int b = 0; b < 3; ++b)
        addFloat (l, imgW (b), juce::String ("Stereo Width ") + bandNames[b], linRange (0.0f, 200.0f), 100.0f, fmtPercent);
    addBool (l, imgMono, "Stereo Mono Bass", false);

    // Limiter
    addFloat (l, limDrive, "Limiter Gain", linRange (0.0f, 24.0f), 0.0f, fmtDbSigned);
    addFloat (l, limCeil, "Limiter Ceiling", linRange (-12.0f, 0.0f), -1.0f, fmtDb);
    addFloat (l, limRel, "Limiter Release", logRange (1.0f, 1000.0f), 120.0f, fmtMs);
    addFloat (l, limLook, "Limiter Lookahead", linRange (0.1f, 5.0f), 1.5f, fmtMs);
    addBool (l, limAuto, "Limiter Auto Release", true);
    addBool (l, limTp, "Limiter True Peak", true);

    return l;
}

//==============================================================================
std::atomic<float>* Cache::get (juce::AudioProcessorValueTreeState& s, const juce::String& id)
{
    auto* p = s.getRawParameterValue (id);
    jassert (p != nullptr);
    return p;
}

void Cache::init (juce::AudioProcessorValueTreeState& s)
{
    in = get (s, inGain);
    out = get (s, outGain);
    dith = get (s, dither);
    for (int m = 0; m < kv::kNumModules; ++m)
        on[(size_t) m] = get (s, moduleOn (m));
    const char* fields[10] = { "on", "type", "freq", "gain", "q", "slope", "place", "dyn", "thr", "range" };
    for (int b = 0; b < kv::kEqBands; ++b)
        for (int f = 0; f < 10; ++f)
            bands[(size_t) b].f[f] = get (s, eq (b, fields[f]));
    eqOutP = get (s, eqOut);
    eqAtk = get (s, eqDynAtk);
    eqRel = get (s, eqDynRel);
    const juce::String cids[10] = { compThr, compRatio, compAtk, compRel, compKnee, compMakeup, compMix, compHpf, compAuto, compRms };
    for (int i = 0; i < 10; ++i)
        c[i] = get (s, cids[i]);
    for (int k = 0; k < 3; ++k)
        mx[(size_t) k] = get (s, mbcX (k));
    const char* mf[7] = { "thr", "ratio", "atk", "rel", "gain", "solo", "byp" };
    for (int b = 0; b < 4; ++b)
        for (int f = 0; f < 7; ++f)
            mb[(size_t) b][(size_t) f] = get (s, mbc (b, mf[f]));
    for (int k = 0; k < 2; ++k)
        ix[(size_t) k] = get (s, imgX (k));
    for (int b = 0; b < 3; ++b)
        iw[(size_t) b] = get (s, imgW (b));
    imono = get (s, imgMono);
    const juce::String lids[6] = { limDrive, limCeil, limRel, limLook, limAuto, limTp };
    for (int i = 0; i < 6; ++i)
        l[i] = get (s, lids[i]);
}

void Cache::fill (kv::EngineParams& p) const
{
    p.inputDb = in->load();
    p.outputDb = out->load();
    const int d = (int) dith->load();
    p.ditherBits = d == 1 ? 24 : (d == 2 ? 16 : 0);
    for (size_t m = 0; m < (size_t) kv::kNumModules; ++m)
        p.on[m] = on[m]->load() > 0.5f;

    for (size_t b = 0; b < (size_t) kv::kEqBands; ++b)
    {
        auto& bp = p.eq.bands[b];
        const auto& f = bands[b].f;
        bp.on = f[0]->load() > 0.5f;
        bp.type = (kv::EqType) juce::jlimit (0, kv::kEqTypeCount - 1, (int) f[1]->load());
        bp.freq = f[2]->load();
        bp.gainDb = f[3]->load();
        bp.q = f[4]->load();
        bp.slope = juce::jlimit (0, kv::kSlopeCount - 1, (int) f[5]->load());
        bp.place = (kv::Placement) juce::jlimit (0, kv::kPlacementCount - 1, (int) f[6]->load());
        bp.dyn = f[7]->load() > 0.5f;
        bp.dynThresholdDb = f[8]->load();
        bp.dynRangeDb = f[9]->load();
    }
    p.eq.outputDb = eqOutP->load();
    p.eq.dynAttackMs = eqAtk->load();
    p.eq.dynReleaseMs = eqRel->load();

    p.comp.thresholdDb = c[0]->load();
    p.comp.ratio = c[1]->load();
    p.comp.attackMs = c[2]->load();
    p.comp.releaseMs = c[3]->load();
    p.comp.kneeDb = c[4]->load();
    p.comp.makeupDb = c[5]->load();
    p.comp.mix = c[6]->load() * 0.01;
    p.comp.scHpfHz = c[7]->load();
    p.comp.autoRelease = c[8]->load() > 0.5f;
    p.comp.rms = c[9]->load() > 0.5f;

    for (size_t k = 0; k < 3; ++k)
        p.mbc.xover[k] = mx[k]->load();
    for (size_t b = 0; b < 4; ++b)
    {
        auto& bp = p.mbc.bands[b];
        bp.thresholdDb = mb[b][0]->load();
        bp.ratio = mb[b][1]->load();
        bp.attackMs = mb[b][2]->load();
        bp.releaseMs = mb[b][3]->load();
        bp.makeupDb = mb[b][4]->load();
        bp.solo = mb[b][5]->load() > 0.5f;
        bp.bypass = mb[b][6]->load() > 0.5f;
    }

    for (size_t k = 0; k < 2; ++k)
        p.imager.xover[k] = ix[k]->load();
    for (size_t b = 0; b < 3; ++b)
        p.imager.width[b] = iw[b]->load() * 0.01;
    p.imager.monoBass = imono->load() > 0.5f;

    p.limiter.driveDb = l[0]->load();
    p.limiter.ceilingDb = l[1]->load();
    p.limiter.releaseMs = l[2]->load();
    p.limiter.lookaheadMs = l[3]->load();
    p.limiter.autoRelease = l[4]->load() > 0.5f;
    p.limiter.truePeak = l[5]->load() > 0.5f;
}
} // namespace params
