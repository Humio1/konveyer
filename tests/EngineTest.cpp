// Offline checks for the KONVEYER engine. Build: see CMakeLists (target EngineTest). Exit code 0 = all passed.

#include "Engine/Engine.h"
#include <chrono>
#include <complex>
#include <cstdio>
#include <random>
#include <vector>

using namespace kv;

static int failures = 0;
#define CHECK(cond, ...)                                         \
    do {                                                         \
        if (! (cond)) { ++failures; std::printf ("  FAIL: "); }  \
        else std::printf ("  ok:   ");                           \
        std::printf (__VA_ARGS__);                               \
        std::printf ("\n");                                      \
    } while (0)

static void section (const char* name) { std::printf ("\n[%s]\n", name); }

// Runs a mono/stereo signal through a processing lambda in kMaxChunk pieces.
template <typename Fn>
static void runChunks (std::vector<double>& l, std::vector<double>& r, Fn&& fn)
{
    for (size_t s = 0; s < l.size(); s += kMaxChunk)
    {
        const int n = (int) std::min<size_t> (kMaxChunk, l.size() - s);
        fn (l.data() + s, r.data() + s, n);
    }
}

static double peakDb (const std::vector<double>& x, size_t from)
{
    double p = 0.0;
    for (size_t i = from; i < x.size(); ++i)
        p = std::max (p, std::abs (x[i]));
    return gainToDb (p);
}

static double dftMagDb (const std::vector<double>& x, double f, double fs)
{
    std::complex<double> s = 0.0;
    for (size_t i = 0; i < x.size(); ++i)
        s += x[i] * std::polar (1.0, -kTwoPi * f * (double) i / fs);
    return 20.0 * std::log10 (std::abs (s) + 1e-300);
}

static std::vector<double> sine (double f, double db, double fs, double seconds)
{
    std::vector<double> v ((size_t) (fs * seconds));
    const double a = dbToGain (db);
    for (size_t i = 0; i < v.size(); ++i)
        v[i] = a * std::sin (kTwoPi * f * (double) i / fs);
    return v;
}

int main()
{
    const double fs = 48000.0;

    //==========================================================================
    section ("EQ shapes vs analog prototypes");
    {
        struct Case { int type; double f, g, q; const char* name; };
        const Case cases[] = { { 2, 10000, 2, 0.7, "air shelf +2 dB @ 10 kHz" }, { 2, 16000, 4, 0.5, "air shelf +4 dB @ 16 kHz" },
                               { 0, 3000, -2, 1.5, "bell -2 dB @ 3 kHz" },     { 0, 12000, 1.5, 0.8, "bell +1.5 dB @ 12 kHz" },
                               { 1, 60, 2, 0.7, "low shelf +2 dB @ 60 Hz" },    { 0, 18000, -6, 4.0, "bell -6 dB @ 18 kHz Q4" } };
        for (double rate : { 44100.0, 48000.0 })
            for (const auto& c : cases)
            {
                const auto d = c.type == 0 ? design::peak (c.f, c.g, c.q, rate) : c.type == 1 ? design::lowShelf (c.f, c.g, c.q, rate) : design::highShelf (c.f, c.g, c.q, rate);
                double err = 0.0;
                for (double ft = 20; ft < 20000; ft *= 1.01)
                {
                    const double x = ft / c.f;
                    const double a = c.type == 0 ? analog::peak2 (x, c.g, c.q) : c.type == 1 ? analog::lowShelf2 (x, c.g, c.q) : analog::highShelf2 (x, c.g, c.q);
                    err = std::max (err, std::abs (10.0 * std::log10 (a) - 20.0 * std::log10 (magnitude (d, kTwoPi * ft / rate))));
                }
                CHECK (err < 0.5, "%-26s at %.1f kHz: max deviation %.3f dB", c.name, rate / 1000.0, err);
            }

        int unstable = 0, total = 0;
        for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
            for (double f = 10; f <= 30000; f *= 1.15)
                for (double g = -30; g <= 30; g += 2.5)
                    for (double q : { 0.025, 0.1, 0.5, 0.71, 2.0, 10.0, 40.0 })
                        for (int t = 0; t < kEqTypeCount; ++t)
                            for (int s = 0; s < kSlopeCount; ++s)
                            {
                                if (s > 0 && ! typeIsCut ((EqType) t))
                                    continue;
                                const auto d = designBand ((EqType) t, f, g, q, s, rate);
                                for (int k = 0; k < d.count; ++k)
                                {
                                    const auto& c = d.sec[(size_t) k];
                                    ++total;
                                    if (! (std::abs (c.a2) < 1.0 && std::abs (c.a1) < 1.0 + c.a2) || ! std::isfinite (c.b0 + c.b1 + c.b2))
                                        ++unstable;
                                }
                            }
        CHECK (unstable == 0, "%d of %d designed sections unstable or invalid", unstable, total);

        const auto lc = designBand (EqType::LowCut, 100.0, 0.0, 0.7071, 1, fs);
        CHECK (std::abs (20.0 * std::log10 (bandMagnitude (lc, kTwoPi * 100.0 / fs)) + 3.01) < 0.05, "12 dB/oct low cut is -3 dB at the corner (%.2f dB)", 20.0 * std::log10 (bandMagnitude (lc, kTwoPi * 100.0 / fs)));
        const auto hc = designBand (EqType::HighCut, 1000.0, 0.0, 0.7071, 5, fs);
        const double oct = 20.0 * std::log10 (bandMagnitude (hc, kTwoPi * 4000.0 / fs) / bandMagnitude (hc, kTwoPi * 8000.0 / fs));
        CHECK (std::abs (oct - 48.0) < 1.5, "48 dB/oct high cut falls %.1f dB per octave", oct);
    }

    //==========================================================================
    section ("EQ processing");
    {
        Equalizer eq;
        EqParams p;
        p.bands[0].on = true;
        p.bands[0].type = EqType::Bell;
        p.bands[0].freq = 1000.0;
        p.bands[0].gainDb = 6.0;
        p.bands[0].q = 1.0;
        eq.setParams (p);
        eq.prepare (fs);
        auto l = sine (1000.0, -20.0, fs, 1.0), r = l;
        runChunks (l, r, [&] (double* a, double* b, int n) { eq.process (a, b, n); });
        CHECK (std::abs (peakDb (l, 24000) - (-14.0)) < 0.05, "bell +6 dB @ 1 kHz: sine comes out at %.2f dBFS (expect -14.00)", peakDb (l, 24000));

        // Mid-only band: left-only input
        Equalizer ms;
        p.bands[0].place = Placement::Mid;
        ms.setParams (p);
        ms.prepare (fs);
        l = sine (1000.0, -20.0, fs, 1.0);
        r.assign (l.size(), 0.0);
        runChunks (l, r, [&] (double* a, double* b, int n) { ms.process (a, b, n); });
        const double gm = dbToGain (6.0) * 0.5;
        CHECK (std::abs (peakDb (l, 24000) - gainToDb (dbToGain (-20.0) * (gm + 0.5))) < 0.05 && std::abs (peakDb (r, 24000) - gainToDb (dbToGain (-20.0) * (gm - 0.5))) < 0.05,
               "mid band on a left-only sine: L %.2f dB, R %.2f dB", peakDb (l, 24000), peakDb (r, 24000));

        // Dynamic band
        Equalizer dyn;
        EqParams d;
        d.bands[3].on = true;
        d.bands[3].type = EqType::Bell;
        d.bands[3].freq = 1000.0;
        d.bands[3].gainDb = 0.0;
        d.bands[3].q = 1.0;
        d.bands[3].dyn = true;
        d.bands[3].dynThresholdDb = -30.0;
        d.bands[3].dynRangeDb = -6.0;
        dyn.setParams (d);
        dyn.prepare (fs);
        l = sine (1000.0, -10.0, fs, 1.0);
        r = l;
        runChunks (l, r, [&] (double* a, double* b, int n) { dyn.process (a, b, n); });
        CHECK (std::abs (dyn.dynamicGainDb (3) + 6.0) < 0.1 && std::abs (peakDb (l, 24000) + 16.0) < 0.3,
               "dynamic bell (range -6 dB) on a loud sine: offset %.2f dB, output %.2f dBFS", dyn.dynamicGainDb (3), peakDb (l, 24000));
        std::vector<double> quiet = sine (1000.0, -50.0, fs, 0.5), q2 = quiet;
        runChunks (quiet, q2, [&] (double* a, double* b, int n) { dyn.process (a, b, n); });
        CHECK (std::abs (dyn.dynamicGainDb (3)) < 0.05, "dynamic bell releases below threshold (offset %.3f dB)", dyn.dynamicGainDb (3));
    }

    //==========================================================================
    section ("Crossovers sum flat");
    {
        MultibandCompressor mbc;
        mbc.prepare (fs);
        std::vector<double> l (16384, 0.0), r (16384, 0.0);
        l[0] = r[0] = 0.01;  // far below every threshold
        runChunks (l, r, [&] (double* a, double* b, int n) { mbc.process (a, b, n); });
        double worst = 0.0;
        for (double f = 20; f < 20000; f *= 1.2)
            worst = std::max (worst, std::abs (dftMagDb (l, f, fs) - 20.0 * std::log10 (0.01)));
        CHECK (worst < 0.01, "4-band split recombines within %.4f dB", worst);

        Imager im;
        im.prepare (fs);
        std::fill (l.begin(), l.end(), 0.0);
        std::fill (r.begin(), r.end(), 0.0);
        l[0] = 0.5;
        runChunks (l, r, [&] (double* a, double* b, int n) { im.process (a, b, n); });
        worst = 0.0;
        for (double f = 20; f < 20000; f *= 1.2)
            worst = std::max (worst, std::abs (dftMagDb (l, f, fs) - 20.0 * std::log10 (0.5)));
        CHECK (worst < 0.01, "imager at 100 %% width is flat within %.4f dB", worst);

        ImagerParams ip;
        ip.width = { 0.0, 0.0, 0.0 };
        Imager mono;
        mono.setParams (ip);
        mono.prepare (fs);
        std::mt19937 rng (7);
        std::normal_distribution<double> nd (0.0, 0.1);
        std::vector<double> a (48000), b (48000);
        for (size_t i = 0; i < a.size(); ++i) { a[i] = nd (rng); b[i] = nd (rng); }
        runChunks (a, b, [&] (double* x, double* y, int n) { mono.process (x, y, n); });
        double diff = 0.0;
        for (size_t i = 2000; i < a.size(); ++i)
            diff = std::max (diff, std::abs (a[i] - b[i]));
        CHECK (diff < 1e-9, "imager at 0 %% width outputs mono (max L-R %.2e), correlation %.3f", diff, mono.correlation.load());
    }

    //==========================================================================
    section ("Compressor");
    {
        Compressor c;
        CompParams p;
        p.thresholdDb = -20.0;
        p.ratio = 4.0;
        p.kneeDb = 0.0;
        p.attackMs = 1.0;
        p.releaseMs = 300.0;
        c.setParams (p);
        c.prepare (fs);
        auto l = sine (1000.0, -10.0, fs, 2.0), r = l;
        runChunks (l, r, [&] (double* a, double* b, int n) { c.process (a, b, n); });
        CHECK (std::abs (peakDb (l, 48000) + 17.5) < 0.6, "-10 dBFS sine, -20 dB threshold, 4:1: output %.2f dBFS (expect about -17.5)", peakDb (l, 48000));
        const float gr = c.grMeter.take (0.0f);
        CHECK (gr > 6.5f && gr < 8.5f, "gain reduction meter reads %.2f dB", gr);

        CompParams pa = p;
        pa.autoRelease = true;
        Compressor ca;
        ca.setParams (pa);
        ca.prepare (fs);
        l = sine (1000.0, -10.0, fs, 2.0);
        r = l;
        runChunks (l, r, [&] (double* a, double* b, int n) { ca.process (a, b, n); });
        CHECK (std::abs (peakDb (l, 72000) + 17.5) < 0.8, "auto release settles to the same static curve (%.2f dBFS)", peakDb (l, 72000));
    }

    //==========================================================================
    section ("Limiter");
    {
        Limiter lim;
        LimiterParams p;
        p.driveDb = 0.0;
        p.ceilingDb = -1.0;
        lim.setParams (p);
        lim.prepare (fs);
        std::vector<double> l (4096, 0.0), r (4096, 0.0);
        l[100] = r[100] = 0.25;
        runChunks (l, r, [&] (double* a, double* b, int n) { lim.process (a, b, n, true); });
        size_t at = 0;
        for (size_t i = 0; i < l.size(); ++i)
            if (std::abs (l[i]) > 0.1) at = i;
        CHECK ((int) at - 100 == lim.latency(), "latency %d samples (%.2f ms), impulse arrives after %d", lim.latency(), lim.latency() * 1000.0 / fs, (int) at - 100);

        for (bool tp : { false, true })
        {
            Limiter lm;
            LimiterParams q;
            q.driveDb = 12.0;
            q.ceilingDb = -1.0;
            q.truePeak = tp;
            q.lookaheadMs = 1.5;
            lm.setParams (q);
            lm.prepare (fs);
            std::mt19937 rng (3);
            std::normal_distribution<double> nd (0.0, 0.25);
            std::vector<double> a (48000 * 5), b (48000 * 5);
            double lp = 0.0;
            for (size_t i = 0; i < a.size(); ++i) { lp = 0.7 * lp + 0.3 * nd (rng); a[i] = lp + 0.3 * std::sin (kTwoPi * 55.0 * (double) i / fs); b[i] = 0.8 * a[i] + 0.2 * nd (rng); }
            runChunks (a, b, [&] (double* x, double* y, int n) { lm.process (x, y, n, true); });
            const double ceil = dbToGain (-1.0);
            double pk = 0.0;
            TruePeakDetector d1, d2;
            double tpk = 0.0;
            for (size_t i = 0; i < a.size(); ++i)
            {
                pk = std::max (pk, std::max (std::abs (a[i]), std::abs (b[i])));
                tpk = std::max (tpk, std::max (d1.push (a[i], true), d2.push (b[i], true)));
            }
            CHECK (pk <= ceil * (1.0 + 1e-9), "%s mode, +12 dB drive: sample peak %.3f dBFS (ceiling -1.0)", tp ? "true-peak" : "sample-peak", gainToDb (pk));
            CHECK (lm.safetyClips == 0, "the look-ahead alone held the ceiling (safety clip used %lld times)", (long long) lm.safetyClips);
            if (tp)
                CHECK (gainToDb (tpk) < -0.8, "true peak of the output %.2f dBTP", gainToDb (tpk));
        }
    }

    //==========================================================================
    section ("Loudness (BS.1770-4)");
    {
        Coeffs k1, k2;
        LoudnessMeter::kWeighting (48000.0, k1, k2);
        const double e1 = std::abs (k1.b0 - 1.53512485958697) + std::abs (k1.b1 + 2.69169618940638) + std::abs (k1.b2 - 1.19839281085285)
                        + std::abs (k1.a1 + 1.69065929318241) + std::abs (k1.a2 - 0.73248077421585);
        const double e2 = std::abs (k2.a1 + 1.99004745483398) + std::abs (k2.a2 - 0.99007225036621);
        CHECK (e1 < 1e-8 && e2 < 1e-8, "K-weighting coefficients at 48 kHz match the standard (err %.1e / %.1e)", e1, e2);

        for (double rate : { 44100.0, 48000.0, 96000.0 })
        {
            LoudnessMeter m;
            m.prepare (rate);
            auto l = sine (997.0, -20.0, rate, 10.0), r = l;
            runChunks (l, r, [&] (double* a, double* b, int n) { m.process (a, b, n); });
            CHECK (std::abs (m.integrated.load() + 20.0f) < 0.05f && std::abs (m.momentary.load() + 20.0f) < 0.05f && std::abs (m.truePeakMax.load() + 20.0f) < 0.05f,
                   "%.1f kHz: -20 dBFS stereo sine reads I %.2f, M %.2f, S %.2f LUFS, TP %.2f dBTP", rate / 1000.0,
                   m.integrated.load(), m.momentary.load(), m.shortTerm.load(), m.truePeakMax.load());
        }
        LoudnessMeter m;
        m.prepare (fs);
        auto l = sine (1000.0, 0.0, fs, 5.0);
        std::vector<double> r (l.size(), 0.0);
        runChunks (l, r, [&] (double* a, double* b, int n) { m.process (a, b, n); });
        CHECK (std::abs (m.integrated.load() + 3.01f) < 0.05f, "0 dBFS 1 kHz sine in one channel reads %.2f LUFS (standard: -3.01)", m.integrated.load());

        // Gating: 5 s at -20 then 5 s of near silence: the silent part must be gated out.
        LoudnessMeter g;
        g.prepare (fs);
        auto a = sine (997.0, -20.0, fs, 5.0), b = a;
        runChunks (a, b, [&] (double* x, double* y, int n) { g.process (x, y, n); });
        auto s1 = sine (997.0, -80.0, fs, 5.0), s2 = s1;
        runChunks (s1, s2, [&] (double* x, double* y, int n) { g.process (x, y, n); });
        // 47 blocks at -20 LUFS plus the three blocks that straddle the cut (75/50/25 % loud) -> -20.13 LUFS
        CHECK (std::abs (g.integrated.load() + 20.13f) < 0.03f, "gated integrated loudness ignores the -80 dB tail (%.2f LUFS, expect -20.13)", g.integrated.load());
    }

    //==========================================================================
    section ("Whole line");
    {
        Engine e;
        EngineParams p;
        e.setParams (p);
        e.prepare (fs);
        std::vector<float> l (16384, 0.0f), r (16384, 0.0f);
        l[0] = r[0] = 0.1f;
        for (size_t s = 0; s < l.size(); s += kMaxChunk)
            e.process (l.data() + s, r.data() + s, kMaxChunk);
        std::vector<double> ld (l.begin(), l.end());
        double worst = 0.0;
        for (double f = 20; f < 20000; f *= 1.25)
            worst = std::max (worst, std::abs (dftMagDb (ld, f, fs) - 20.0 * std::log10 (0.1)));
        CHECK (worst < 0.02, "default settings: frequency response flat within %.4f dB", worst);

        CHECK (packOrder (unpackOrder (packOrder ({ 4, 2, 0, 3, 1 }))) == packOrder ({ 4, 2, 0, 3, 1 }) && ! isValidOrder ({ 0, 0, 1, 2, 3 }), "station order packs and validates");

        // Busy settings, order changes every 100 ms, random audio: must stay finite and below the ceiling.
        EngineParams b;
        for (int i = 0; i < kEqBands; ++i)
        {
            auto& band = b.eq.bands[(size_t) i];
            band.on = true;
            band.type = (EqType) (i % kEqTypeCount);
            band.freq = 40.0 * std::pow (1.6, i);
            band.gainDb = (i % 2 ? 3.0 : -2.0);
            band.q = 0.8;
            band.slope = i % kSlopeCount;
            band.place = (Placement) (i % kPlacementCount);
            band.dyn = (i % 3 == 0);
        }
        b.comp.thresholdDb = -24.0;
        b.comp.ratio = 3.0;
        b.comp.autoRelease = true;
        for (auto& band : b.mbc.bands) { band.thresholdDb = -30.0; band.ratio = 2.5; }
        b.imager.width = { 0.0, 1.2, 1.5 };
        b.limiter.driveDb = 8.0;
        b.limiter.ceilingDb = -0.3;
        b.ditherBits = 16;
        Engine busy;
        busy.setParams (b);
        busy.prepare (fs);
        std::mt19937 rng (11);
        std::normal_distribution<double> nd (0.0, 0.3);
        std::vector<float> a (kMaxChunk), c (kMaxChunk);
        bool finite = true;
        double outPk = 0.0;
        bool quantised = true;
        const double lsb = std::ldexp (1.0, -15);
        const auto t0 = std::chrono::steady_clock::now();
        const int blocks = (int) (60.0 * fs / kMaxChunk);
        for (int blk = 0; blk < blocks; ++blk)
        {
            if (blk % 10 == 0)
            {
                Order o = defaultOrder();
                std::shuffle (o.begin(), o.end(), rng);
                b.order = o;
                b.on[(size_t) (blk / 10 % kNumModules)] = (blk / 10) % 3 != 0;
                b.on[kModLimiter] = true;
                busy.setParams (b);
            }
            for (int i = 0; i < kMaxChunk; ++i)
            {
                a[(size_t) i] = (float) nd (rng);
                c[(size_t) i] = (float) (0.6 * a[(size_t) i] + 0.4 * nd (rng));
            }
            busy.process (a.data(), c.data(), kMaxChunk);
            const bool limiterLast = busy.currentOrder()[kNumModules - 1] == kModLimiter && busy.currentOrder() == b.order;
            for (int i = 0; i < kMaxChunk; ++i)
            {
                finite = finite && std::isfinite (a[(size_t) i]) && std::isfinite (c[(size_t) i]);
                if (limiterLast)
                    outPk = std::max (outPk, (double) std::max (std::abs (a[(size_t) i]), std::abs (c[(size_t) i])));
                const double qa = a[(size_t) i] / lsb;
                quantised = quantised && std::abs (qa - std::round (qa)) < 1e-3;
            }
        }
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        CHECK (finite, "60 s of busy processing with order changes stays finite");
        CHECK (outPk <= dbToGain (-0.3) + 2 * lsb, "with the limiter last, the output never exceeds the -0.3 dB ceiling (peak %.3f dBFS)", gainToDb (outPk));
        CHECK (quantised, "16-bit dither output sits on the 16-bit grid");
        CHECK (secs < 6.0, "CPU: 60 s of audio with 12 bands (4 dynamic) and all stations took %.2f s (%.1f %% of real time)", secs, secs / 60.0 * 100.0);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
