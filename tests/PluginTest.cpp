// Plugin-level checks that pluginval does not cover: saved state, presets, quality changes with the
// latency they bring, host bypass keeping the latency, and how long the editor takes to paint.

#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <chrono>

static int failures = 0;
#define CHECK(cond, ...)                                         \
    do {                                                         \
        if (! (cond)) { ++failures; std::printf ("  FAIL: "); }  \
        else std::printf ("  ok:   ");                           \
        std::printf (__VA_ARGS__);                               \
        std::printf ("\n");                                      \
    } while (0)

static float value (TishinaProcessor& p, const juce::String& id) { return p.apvts.getParameter (id)->convertFrom0to1 (p.apvts.getParameter (id)->getValue()); }
static void set (TishinaProcessor& p, const juce::String& id, float v) { auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); }

/** Sends an impulse through and returns how late it comes out (-1 if it never does). */
static int impulseDelay (TishinaProcessor& p, bool bypassed)
{
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    int at = -1;
    for (int b = 0; b < 80; ++b)
    {
        buf.clear();
        if (b == 1)
            buf.setSample (0, 100, 0.25f), buf.setSample (1, 100, 0.25f);
        if (bypassed) p.processBlockBypassed (buf, midi); else p.processBlock (buf, midi);
        for (int s = 0; s < 512; ++s)
            if (at < 0 && std::abs (buf.getSample (0, s)) > 0.1f)
                at = b * 512 + s - (512 + 100);
    }
    return at;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    std::printf ("\n[State]\n");
    {
        TishinaProcessor a;
        a.prepareToPlay (44100.0, 256);
        set (a, params::depth, 72.0f);
        set (a, params::low, 240.0f);
        set (a, params::high, 7200.0f);
        set (a, params::mode, 2.0f);
        set (a, params::delta, 1.0f);
        a.uiState().setProperty ("scale", 1.2, nullptr);
        juce::MemoryBlock mb;
        a.getStateInformation (mb);

        TishinaProcessor b;
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        CHECK (std::abs (value (b, params::depth) - 72.0f) < 0.05f && std::abs (value (b, params::low) - 240.0f) < 0.5f
                   && std::abs (value (b, params::high) - 7200.0f) < 5.0f,
               "depth and range survive save/load");
        CHECK (value (b, params::mode) == 2.0f && value (b, params::delta) > 0.5f, "mode and delta survive save/load");
        CHECK (std::abs ((double) b.uiState().getProperty ("scale", 0.0) - 1.2) < 0.001, "window scale survives save/load");

        auto* sw = b.apvts.getParameter (params::soft);
        sw->setValueNotifyingHost (0.3f);
        juce::MemoryBlock mb2;
        b.getStateInformation (mb2);
        sw->setValueNotifyingHost (0.86f);
        b.setStateInformation (mb2.getData(), (int) mb2.getSize());
        CHECK (std::abs (sw->getValue() - 0.0f) < 0.01f, "switch parameter restored exactly (%.3f)", sw->getValue());

        TishinaProcessor c;
        const char junk[] = "not a state";
        c.setStateInformation (junk, (int) sizeof (junk));
        CHECK (std::abs (value (c, params::depth) - 50.0f) < 0.01f, "garbage state is ignored");
    }

    std::printf ("\n[Presets]\n");
    {
        TishinaProcessor p;
        p.prepareToPlay (48000.0, 512);
        CHECK (p.getNumPrograms() >= 10, "%d factory programs", p.getNumPrograms());
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            p.applyQualityChange();
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            bool finite = true;
            float peak = 0.0f;
            for (int b = 0; b < 200; ++b)
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int s = 0; s < 512; ++s)
                        buf.setSample (ch, s, 0.3f * std::sin ((float) (b * 512 + s) * 0.05f * (float) (ch + 1)));
                p.processBlock (buf, midi);
                for (int ch = 0; ch < 2; ++ch)
                    for (int s = 0; s < 512; ++s)
                    {
                        finite = finite && std::isfinite (buf.getSample (ch, s));
                        peak = std::max (peak, std::abs (buf.getSample (ch, s)));
                    }
            }
            CHECK (finite && peak < 1.0f && p.getCurrentProgram() == i, "program %2d \"%s\" loads and processes cleanly (peak %.2f)",
                   i + 1, p.getProgramName (i).toRawUTF8(), peak);
        }
        p.setCurrentProgram (0);
        CHECK (std::abs (value (p, params::depth) - 50.0f) < 0.01f && value (p, params::quality) == 1.0f, "Init resets every parameter");
    }

    std::printf ("\n[Latency, quality and bypass]\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        TishinaProcessor p;
        p.prepareToPlay (sr, 512);
        const int lat = p.getLatencySamples();
        const int active = impulseDelay (p, false), bypassed = impulseDelay (p, true);
        CHECK (active == lat && bypassed == lat, "%.1f kHz: latency %d samples (%.1f ms); processed impulse at %d, bypassed at %d",
               sr / 1000.0, lat, lat * 1000.0 / sr, active, bypassed);
    }
    {
        TishinaProcessor p;
        p.prepareToPlay (48000.0, 512);
        for (int q : { 0, 2, 1 })
        {
            set (p, params::quality, (float) q);
            p.applyQualityChange();
            const int lat = p.getLatencySamples();
            const int active = impulseDelay (p, false), bypassed = impulseDelay (p, true);
            CHECK (lat == kv::Suppressor::fftSizeFor (48000.0, q) && active == lat && bypassed == lat,
                   "quality %s: reported latency %d, processed impulse at %d, bypassed at %d",
                   params::qualityNames()[q].toRawUTF8(), lat, active, bypassed);
        }
    }

    std::printf ("\n[Editor paint time]\n");
    {
        TishinaProcessor p;
        p.prepareToPlay (48000.0, 512);
        p.setCurrentProgram (2);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        for (int b = 0; b < 400; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int s = 0; s < 512; ++s)
                    buf.setSample (ch, s, 0.3f * std::sin ((float) (b * 512 + s) * 0.031f * (float) (ch + 1)));
            p.processBlock (buf, midi);
        }
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto* e = dynamic_cast<TishinaEditor*> (ed.get());
        e->setSize (TishinaEditor::kBaseW, TishinaEditor::kBaseH);
        e->tick();
        juce::Image img (juce::Image::ARGB, e->getWidth(), e->getHeight(), true);
        const int runs = 10;
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < runs; ++i)
        {
            e->tick();
            juce::Graphics g (img);
            e->paintEntireComponent (g, true);
        }
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count() / runs;
        std::printf ("  info: full window repaint %.1f ms\n", ms);

        // what moves each frame: the screen and the meter column
        const juce::Rectangle<int> regions[] = { { 20, 94, 832, 348 }, { 864, 94, 290, 372 } };
        const auto t1 = std::chrono::steady_clock::now();
        for (int i = 0; i < runs; ++i)
        {
            e->tick();
            for (auto rr : regions)
            {
                juce::Graphics g (img);
                g.reduceClipRegion (rr);
                e->paintEntireComponent (g, true);
            }
        }
        const double frame = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t1).count() / runs;
        std::printf ("  info: typical frame (moving parts only) %.1f ms\n", frame);
        CHECK (frame < 25.0, "a display frame fits comfortably in the 33 ms timer budget (%.1f ms)", frame);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
