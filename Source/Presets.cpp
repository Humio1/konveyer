#include "Presets.h"

namespace
{
using V = std::vector<std::pair<juce::String, float>>;
using namespace params;

enum T { Bell = 0, LowShelf, HighShelf, LowCut, HighCut, Notch, Tilt };

/** One EQ band: slot, type, freq, gain, Q, slope index (cuts), dynamic threshold/range (0 = static). */
void band (V& v, int b, int type, float f, float g, float q, int slope = 1, float dynThr = 0.0f, float dynRange = 0.0f)
{
    v.push_back ({ eq (b, "on"), 1.0f });
    v.push_back ({ eq (b, "type"), (float) type });
    v.push_back ({ eq (b, "freq"), f });
    v.push_back ({ eq (b, "gain"), g });
    v.push_back ({ eq (b, "q"), q });
    v.push_back ({ eq (b, "slope"), (float) slope });
    if (std::abs (dynRange) > 0.0f)
    {
        v.push_back ({ eq (b, "dyn"), 1.0f });
        v.push_back ({ eq (b, "thr"), dynThr });
        v.push_back ({ eq (b, "range"), dynRange });
    }
}

void comp (V& v, float thr, float ratio, float atk, float rel, bool autoRel, float knee, float makeup, float hpf)
{
    v.insert (v.end(), { { compThr, thr }, { compRatio, ratio }, { compAtk, atk }, { compRel, rel },
                         { compAuto, autoRel ? 1.0f : 0.0f }, { compKnee, knee }, { compMakeup, makeup }, { compHpf, hpf } });
}

void mband (V& v, int b, float thr, float ratio, float atk, float rel, float gain = 0.0f)
{
    v.insert (v.end(), { { mbc (b, "thr"), thr }, { mbc (b, "ratio"), ratio }, { mbc (b, "atk"), atk }, { mbc (b, "rel"), rel }, { mbc (b, "gain"), gain } });
}

void xovers (V& v, float a, float b, float c) { v.insert (v.end(), { { mbcX (0), a }, { mbcX (1), b }, { mbcX (2), c } }); }

void imager (V& v, bool monoBass, float x1, float x2, float w1, float w2, float w3)
{
    v.insert (v.end(), { { imgMono, monoBass ? 1.0f : 0.0f }, { imgX (0), x1 }, { imgX (1), x2 }, { imgW (0), w1 }, { imgW (1), w2 }, { imgW (2), w3 } });
}

void limiter (V& v, float drive, float ceil, float rel, float look = 1.5f, bool autoRel = true)
{
    v.insert (v.end(), { { limDrive, drive }, { limCeil, ceil }, { limRel, rel }, { limLook, look }, { limAuto, autoRel ? 1.0f : 0.0f }, { limTp, 1.0f } });
}
} // namespace

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> list = []
    {
        std::vector<Preset> p;

        p.push_back ({ "Init", {} });

        {
            V v;
            comp (v, -18.0f, 1.5f, 30.0f, 200.0f, true, 8.0f, 1.5f, 80.0f);
            limiter (v, 2.0f, -1.0f, 150.0f);
            p.push_back ({ "Gentle Glue", v });
        }
        {
            V v;
            band (v, 0, LowCut, 25.0f, 0.0f, 0.71f, 3);
            band (v, 1, Bell, 230.0f, -1.0f, 0.9f);
            band (v, 2, Bell, 3200.0f, 0.0f, 1.2f, 1, -28.0f, -3.0f);
            band (v, 3, HighShelf, 11000.0f, 1.5f, 0.6f);
            comp (v, -16.0f, 2.0f, 30.0f, 150.0f, true, 6.0f, 1.0f, 90.0f);
            xovers (v, 120.0f, 900.0f, 5500.0f);
            mband (v, 0, -22.0f, 2.0f, 40.0f, 200.0f);
            mband (v, 1, -24.0f, 1.6f, 25.0f, 150.0f);
            mband (v, 2, -26.0f, 1.8f, 10.0f, 100.0f);
            mband (v, 3, -28.0f, 2.0f, 5.0f, 80.0f);
            imager (v, true, 120.0f, 5000.0f, 100.0f, 100.0f, 115.0f);
            limiter (v, 6.0f, -1.0f, 100.0f);
            p.push_back ({ "House Master", v });
        }
        {
            V v;
            band (v, 0, LowCut, 28.0f, 0.0f, 0.71f, 3);
            band (v, 1, LowShelf, 55.0f, 1.0f, 0.71f);
            band (v, 2, Bell, 380.0f, -1.5f, 1.0f);
            band (v, 3, HighShelf, 9000.0f, 1.0f, 0.71f);
            comp (v, -14.0f, 3.0f, 45.0f, 90.0f, false, 4.0f, 2.0f, 100.0f);
            xovers (v, 140.0f, 1000.0f, 6000.0f);
            mband (v, 0, -20.0f, 2.5f, 35.0f, 150.0f);
            mband (v, 2, -26.0f, 1.8f, 10.0f, 90.0f);
            imager (v, true, 140.0f, 5000.0f, 100.0f, 100.0f, 120.0f);
            limiter (v, 7.0f, -0.8f, 80.0f);
            p.push_back ({ "Techno Punch", v });
        }
        {
            V v;
            comp (v, -16.0f, 1.8f, 30.0f, 200.0f, true, 6.0f, 1.0f, 60.0f);
            limiter (v, 3.0f, -1.0f, 150.0f);
            p.push_back ({ "Streaming -14 LUFS", v });
        }
        {
            V v;
            band (v, 0, LowCut, 24.0f, 0.0f, 0.71f, 3);
            comp (v, -18.0f, 2.5f, 20.0f, 120.0f, true, 6.0f, 2.0f, 90.0f);
            xovers (v, 120.0f, 1000.0f, 6000.0f);
            mband (v, 0, -20.0f, 3.0f, 30.0f, 150.0f);
            mband (v, 1, -22.0f, 2.0f, 20.0f, 120.0f);
            mband (v, 2, -24.0f, 2.0f, 8.0f, 90.0f);
            mband (v, 3, -26.0f, 2.5f, 3.0f, 60.0f);
            imager (v, true, 120.0f, 5000.0f, 100.0f, 100.0f, 110.0f);
            limiter (v, 10.0f, -0.3f, 50.0f, 1.0f);
            p.push_back ({ "Club Loud", v });
        }
        {
            V v;
            band (v, 0, LowCut, 20.0f, 0.0f, 0.71f, 3);
            band (v, 1, Bell, 6500.0f, 0.0f, 2.0f, 1, -32.0f, -5.0f);
            band (v, 2, HighCut, 18000.0f, 0.0f, 0.71f, 1);
            comp (v, -18.0f, 1.5f, 30.0f, 250.0f, true, 8.0f, 0.0f, 60.0f);
            imager (v, true, 150.0f, 5000.0f, 0.0f, 100.0f, 100.0f);
            v.push_back ({ moduleOn (kv::kModLimiter), 0.0f });
            v.push_back ({ outGain, -1.0f });
            p.push_back ({ "Vinyl Pre-Master", v });
        }
        {
            V v;
            band (v, 0, HighShelf, 12000.0f, 2.0f, 0.5f);
            imager (v, true, 100.0f, 4000.0f, 90.0f, 110.0f, 135.0f);
            limiter (v, 2.0f, -1.0f, 120.0f);
            p.push_back ({ "Wide & Airy", v });
        }
        {
            V v;
            band (v, 0, Bell, 3200.0f, 0.0f, 1.3f, 1, -30.0f, -4.0f);
            band (v, 1, Bell, 7000.0f, 0.0f, 2.0f, 1, -34.0f, -5.0f);
            p.push_back ({ "De-Harsh", v });
        }
        {
            V v;
            band (v, 0, LowShelf, 90.0f, 0.0f, 0.71f, 1, -18.0f, -4.0f);
            xovers (v, 150.0f, 1000.0f, 6000.0f);
            mband (v, 0, -20.0f, 3.0f, 30.0f, 150.0f);
            imager (v, true, 150.0f, 5000.0f, 100.0f, 100.0f, 100.0f);
            limiter (v, 2.0f, -1.0f, 120.0f);
            p.push_back ({ "Bass Control", v });
        }
        {
            V v;
            imager (v, true, 120.0f, 5000.0f, 100.0f, 115.0f, 125.0f);
            comp (v, -18.0f, 2.0f, 30.0f, 150.0f, true, 6.0f, 1.0f, 80.0f);
            limiter (v, 4.0f, -1.0f, 120.0f);
            p.push_back ({ "Wide Bus (Imager First)", v,
                           { kv::kModImager, kv::kModEq, kv::kModComp, kv::kModMbc, kv::kModLimiter } });
        }
        return p;
    }();
    return list;
}
