// Plugin-level checks that pluginval does not cover: station order in the saved state, presets,
// host bypass keeps the latency, and how long the editor takes to paint.

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

static float value (KonveyerProcessor& p, const juce::String& id) { return p.apvts.getParameter (id)->convertFrom0to1 (p.apvts.getParameter (id)->getValue()); }
static void set (KonveyerProcessor& p, const juce::String& id, float v) { auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); }

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    std::printf ("\n[State]\n");
    {
        KonveyerProcessor a;
        a.prepareToPlay (44100.0, 256);
        set (a, params::eq (3, "on"), 1.0f);
        set (a, params::eq (3, "freq"), 3150.0f);
        set (a, params::eq (3, "place"), 2.0f);
        set (a, params::compThr, -21.5f);
        set (a, params::limCeil, -0.7f);
        a.setOrder ({ kv::kModLimiter, kv::kModImager, kv::kModEq, kv::kModMbc, kv::kModComp });
        a.uiState().setProperty ("module", 3, nullptr);
        juce::MemoryBlock mb;
        a.getStateInformation (mb);

        KonveyerProcessor b;
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        CHECK (std::abs (value (b, params::eq (3, "freq")) - 3150.0f) < 1.0f && value (b, params::eq (3, "place")) == 2.0f && value (b, params::eq (3, "on")) > 0.5f,
               "EQ band settings survive save/load");
        CHECK (std::abs (value (b, params::compThr) + 21.5f) < 0.01f && std::abs (value (b, params::limCeil) + 0.7f) < 0.01f, "compressor and limiter settings survive save/load");
        const auto o = b.getOrder();
        CHECK (o == kv::Order ({ kv::kModLimiter, kv::kModImager, kv::kModEq, kv::kModMbc, kv::kModComp }), "station order survives save/load (%d %d %d %d %d)", o[0], o[1], o[2], o[3], o[4]);
        CHECK ((int) b.uiState().getProperty ("module", -1) == 3, "editor state (open station) survives save/load");

        KonveyerProcessor c;
        const char junk[] = "not a state";
        c.setStateInformation (junk, (int) sizeof (junk));
        CHECK (c.getOrder() == kv::defaultOrder(), "garbage state is ignored");
    }

    std::printf ("\n[Presets]\n");
    {
        KonveyerProcessor p;
        p.prepareToPlay (48000.0, 512);
        CHECK (p.getNumPrograms() >= 10, "%d factory programs", p.getNumPrograms());
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            bool finite = true;
            for (int b = 0; b < 200; ++b)
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int s = 0; s < 512; ++s)
                        buf.setSample (ch, s, 0.3f * std::sin ((float) (b * 512 + s) * 0.05f * (float) (ch + 1)));
                p.processBlock (buf, midi);
                for (int ch = 0; ch < 2; ++ch)
                    for (int s = 0; s < 512; ++s)
                        finite = finite && std::isfinite (buf.getSample (ch, s));
            }
            CHECK (finite && p.getCurrentProgram() == i, "program %2d \"%s\" loads and processes cleanly", i + 1, p.getProgramName (i).toRawUTF8());
        }
        p.setCurrentProgram (10);
        CHECK (p.getOrder()[0] == kv::kModImager, "the imager-first program also sets the station order");
        p.setCurrentProgram (0);
        CHECK (p.getOrder() == kv::defaultOrder() && std::abs (value (p, params::compThr)) < 0.01f, "Init resets order and parameters");
    }

    std::printf ("\n[Latency and bypass]\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        // The multiband and imager crossovers add phase shift (not delay), so they are switched off
        // here to measure the pure latency.
        KonveyerProcessor p;
        set (p, params::moduleOn (kv::kModMbc), 0.0f);
        set (p, params::moduleOn (kv::kModImager), 0.0f);
        p.prepareToPlay (sr, 512);
        const int lat = p.getLatencySamples();
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        int activeAt = -1, bypassAt = -1;
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int b = 0; b < 8; ++b)
            {
                buf.clear();
                if (b == 1)
                    buf.setSample (0, 100, 0.25f), buf.setSample (1, 100, 0.25f);
                if (pass == 0) p.processBlock (buf, midi); else p.processBlockBypassed (buf, midi);
                for (int s = 0; s < 512; ++s)
                    if (std::abs (buf.getSample (0, s)) > 0.1f)
                        (pass == 0 ? activeAt : bypassAt) = b * 512 + s - (512 + 100);
            }
        }
        CHECK (activeAt == lat && bypassAt == lat, "%.1f kHz: latency %d samples (%.2f ms); processed impulse at %d, bypassed at %d",
               sr / 1000.0, lat, lat * 1000.0 / sr, activeAt, bypassAt);
    }

    std::printf ("\n[Editor paint time]\n");
    {
        KonveyerProcessor p;
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
        auto* e = dynamic_cast<KonveyerEditor*> (ed.get());
        e->setSize (KonveyerEditor::kBaseW, KonveyerEditor::kBaseH);
        for (int m = 0; m < kv::kNumModules; ++m)
        {
            e->selectModule (m);
            e->tick();
            juce::Image img (juce::Image::ARGB, e->getWidth(), e->getHeight(), true);
            const auto t0 = std::chrono::steady_clock::now();
            const int runs = 10;
            for (int i = 0; i < runs; ++i)
            {
                e->tick();
                juce::Graphics g (img);
                e->paintEntireComponent (g, true);
            }
            const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count() / runs;
            std::printf ("  info: station %d: full window repaint %.1f ms\n", m + 1, ms);

            // what actually moves each frame: the station's screen, the belt strip and the output meters
            const juce::Rectangle<int> regions[] = { { 20, 238, 876, 300 }, { 130, 180, 940, 40 }, { 920, 268, 268, 116 } };
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
            std::printf ("  info: station %d: typical frame (moving parts only) %.1f ms\n", m + 1, frame);
        }
        e->selectModule (0);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
