// Renders the plugin window to PNGs without a host, one per station, for visual checks.
// Usage: EditorSnapshot <out-prefix> [preset-index] [scale]

#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <random>
#include <thread>

namespace
{
/** A short house loop (kick, bass, offbeat hats, minor chord pad) so meters and screens have something to show. */
struct Loop
{
    double fs = 48000.0, t = 0.0;
    std::mt19937 rng { 1 };
    std::normal_distribution<float> noise { 0.0f, 1.0f };
    float hpState = 0.0f, hpPrev = 0.0f, padPhase[6] {}, bassPhase = 0.0f;

    void render (float* L, float* R, int n)
    {
        const double beat = 60.0 / 124.0;
        for (int i = 0; i < n; ++i, t += 1.0 / fs)
        {
            const double inBeat = std::fmod (t, beat);
            const double inHalf = std::fmod (t + beat * 0.5, beat);
            const float kick = (float) (std::exp (-inBeat * 18.0) * std::sin (2.0 * juce::MathConstants<double>::pi * (48.0 * inBeat + 160.0 * (1.0 - std::exp (-inBeat * 40.0)) / 40.0)));
            const float n0 = noise (rng);
            hpState = 0.6f * (hpState + n0 - hpPrev);
            hpPrev = n0;
            const float hat = (float) std::exp (-inHalf * 60.0) * hpState * 0.35f;
            bassPhase += (float) (55.0 / fs);
            bassPhase -= std::floor (bassPhase);
            const float bass = (2.0f * bassPhase - 1.0f) * 0.18f * (float) (0.4 + 0.6 * std::exp (-std::fmod (t, beat * 0.5) * 6.0));
            const float freqs[6] = { 220.0f, 261.63f, 329.63f, 221.1f, 262.9f, 331.2f };
            float padL = 0.0f, padR = 0.0f;
            for (int v = 0; v < 6; ++v)
            {
                padPhase[v] += freqs[v] / (float) fs;
                padPhase[v] -= std::floor (padPhase[v]);
                const float s = (2.0f * padPhase[v] - 1.0f) * 0.035f;
                (v < 3 ? padL : padR) += s;
            }
            L[i] = 0.55f * kick + bass + hat * 0.8f + padL + 0.3f * padR;
            R[i] = 0.55f * kick + bass + hat * 1.2f + padR + 0.3f * padL;
        }
    }
};
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String prefix = argc > 1 ? argv[1] : "snap";
    const int preset = argc > 2 ? juce::String (argv[2]).getIntValue() : 2;
    const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 1.0f;

    KonveyerProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    proc.loadPreset (preset);
    if (preset == 2)
    {
        // show a couple of extra bands, one of them on the side channel
        auto set = [&] (const juce::String& id, float v) { if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        set (params::eq (4, "on"), 1.0f); set (params::eq (4, "type"), 0.0f); set (params::eq (4, "freq"), 6800.0f);
        set (params::eq (4, "gain"), -2.5f); set (params::eq (4, "q"), 2.5f); set (params::eq (4, "place"), 2.0f);
    }

    Loop loop;
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    auto run = [&] (int blocks)
    {
        for (int b = 0; b < blocks; ++b)
        {
            loop.render (buf.getWritePointer (0), buf.getWritePointer (1), 512);
            proc.processBlock (buf, midi);
        }
    };
    run (48000 * 6 / 512);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* editor = dynamic_cast<KonveyerEditor*> (ed.get());
    editor->setSize (juce::roundToInt (KonveyerEditor::kBaseW * scale), juce::roundToInt (KonveyerEditor::kBaseH * scale));

    for (int m = 0; m < kv::kNumModules; ++m)
    {
        editor->selectModule (m);
        for (int i = 0; i < 40; ++i)
        {
            run (3);
            editor->tick();
        }
        auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (prefix + "_" + juce::String (m) + ".png");
        file.deleteFile();
        juce::FileOutputStream os (file);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("wrote %s\n", file.getFullPathName().toRawUTF8());
    }
    editor->selectModule (0);
    ed.reset();
    return 0;
}
