// Offline checks of the TISHINA engine: transparency, latency, that resonances are cut where they are
// and nowhere else, stereo modes, and CPU cost.

#include "../Source/Engine/Suppressor.h"
#include <chrono>
#include <cstdio>
#include <memory>
#include <random>

using namespace kv;

static int failures = 0;
#define CHECK(cond, ...)                                         \
    do {                                                         \
        if (! (cond)) { ++failures; std::printf ("  FAIL: "); }  \
        else std::printf ("  ok:   ");                           \
        std::printf (__VA_ARGS__);                               \
        std::printf ("\n");                                      \
    } while (0)

struct Stereo
{
    std::vector<float> l, r;
    explicit Stereo (size_t n = 0) : l (n, 0.0f), r (n, 0.0f) {}
};

static void run (Suppressor& s, Stereo& x, int block = 480)
{
    for (size_t i = 0; i < x.l.size(); i += (size_t) block)
        s.process (x.l.data() + i, x.r.data() + i, (int) std::min ((size_t) block, x.l.size() - i));
}

static Stereo noise (size_t n, double amp, unsigned seed)
{
    Stereo x (n);
    std::mt19937 rng (seed);
    std::normal_distribution<float> d (0.0f, (float) amp);
    for (size_t i = 0; i < n; ++i)
    {
        x.l[i] = d (rng);
        x.r[i] = d (rng);
    }
    return x;
}

static void addSine (std::vector<float>& v, double hz, double amp, double fs)
{
    for (size_t i = 0; i < v.size(); ++i)
        v[i] += (float) (amp * std::sin (kTwoPi * hz * (double) i / fs));
}

/** Averaged power spectrum (Welch, Hann, 8192 points) of v[from .. from+len), in dB per bin. */
static std::vector<double> psd (const std::vector<float>& v, size_t from, size_t len)
{
    const int N = 8192;
    Fft f;
    f.setSize (N);
    std::vector<std::complex<double>> b ((size_t) N);
    std::vector<double> acc ((size_t) N / 2 + 1, 0.0);
    int count = 0;
    for (size_t start = from; start + (size_t) N <= from + len; start += (size_t) N / 2, ++count)
    {
        for (int i = 0; i < N; ++i)
            b[(size_t) i] = { v[start + (size_t) i] * (0.5 - 0.5 * std::cos (kTwoPi * i / N)), 0.0 };
        f.transform (b.data(), false);
        for (int k = 0; k <= N / 2; ++k)
            acc[(size_t) k] += std::norm (b[(size_t) k]);
    }
    for (auto& a : acc)
        a = 10.0 * std::log10 (a / std::max (1, count) + 1.0e-30);
    return acc;
}

static double bandDb (const std::vector<double>& p, double fs, double hz, int halfBins = 2)
{
    const int k = (int) std::lround (hz * 8192.0 / fs);
    double m = -300.0;
    for (int i = k - halfBins; i <= k + halfBins; ++i)
        m = std::max (m, p[(size_t) i]);
    return m;
}

/** Median of out-in (dB) across bins between lo and hi Hz, skipping +-skipOct around skipHz. */
static double medianChange (const std::vector<double>& in, const std::vector<double>& out, double fs, double lo, double hi,
                            double skipHz = 0.0, double skipOct = 0.0)
{
    std::vector<double> d;
    for (size_t k = 1; k < in.size(); ++k)
    {
        const double f = (double) k * fs / 8192.0;
        if (f < lo || f > hi)
            continue;
        if (skipHz > 0.0 && std::abs (std::log2 (f / skipHz)) < skipOct)
            continue;
        d.push_back (out[k] - in[k]);
    }
    std::nth_element (d.begin(), d.begin() + (long) d.size() / 2, d.end());
    return d[d.size() / 2];
}

int main()
{
    std::printf ("\n[Transparency and latency]\n");
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int q = 0; q < 3; ++q)
        {
            auto s = std::make_unique<Suppressor>();
            SuppressorParams p;
            p.depth = 0.0;
            s->setParams (p);
            s->prepare (fs, q);
            const int lat = s->latencySamples();
            auto x = noise ((size_t) (fs * 0.5), 0.25, 1);
            const auto dry = x;
            run (*s, x);
            double err = 0.0;
            for (size_t i = (size_t) lat; i < x.l.size(); ++i)
                err = std::max (err, (double) std::max (std::abs (x.l[i] - dry.l[i - (size_t) lat]), std::abs (x.r[i] - dry.r[i - (size_t) lat])));
            CHECK (err < 1.0e-5, "%.1f kHz, quality %d: FFT %d, latency %d (%.1f ms), depth 0 nulls against the delayed input (%.1f dB)",
                   fs / 1000.0, q, s->getFftSize(), lat, lat * 1000.0 / fs, gainToDb (err));
        }

    {
        Suppressor s;
        SuppressorParams p;
        p.depth = 80.0;
        s.setParams (p);
        s.prepare (48000.0, 1);
        Stereo x (20000);
        x.l[1000] = x.r[1000] = 0.5f;
        run (s, x, 64);
        size_t peak = 0;
        for (size_t i = 0; i < x.l.size(); ++i)
            if (std::abs (x.l[i]) > std::abs (x.l[peak]))
                peak = i;
        CHECK ((int) peak - 1000 == s.latencySamples(), "impulse arrives exactly %d samples late with processing on (got %d)", s.latencySamples(), (int) peak - 1000);
    }

    std::printf ("\n[Resonance on noise]\n");
    const double fs = 48000.0;
    const size_t len = (size_t) (fs * 8.0), skip = (size_t) fs;
    for (double hz : { 180.0, 1200.0, 4700.0 })
    {
        Suppressor s;
        SuppressorParams p;
        p.depth = 70.0;
        s.setParams (p);
        s.prepare (fs, 1);
        auto x = noise (len, 0.05, 7);
        addSine (x.l, hz, 0.1, fs);
        addSine (x.r, hz, 0.1, fs);
        const auto dry = x;
        run (s, x);
        const int lat = s.latencySamples();
        const auto pin = psd (dry.l, skip, len - skip - (size_t) lat - 1);
        const auto pout = psd (x.l, skip + (size_t) lat, len - skip - (size_t) lat - 1);
        const double cut = bandDb (pin, fs, hz) - bandDb (pout, fs, hz);
        const double rest = medianChange (pin, pout, fs, 100.0, 16000.0, hz, 0.5);
        CHECK (cut > 8.0 && std::abs (rest) < 1.0, "%5.0f Hz whistle cut by %.1f dB, rest of the spectrum moves %.2f dB (median)", hz, cut, rest);
    }

    {
        // A pad: a dense harmonic series (three detuned voices) with one ringing partial.
        Suppressor s;
        SuppressorParams p;
        p.depth = 60.0;
        s.setParams (p);
        s.prepare (fs, 1);
        auto x = noise (len, 0.002, 4);
        for (double det : { 1.0, 1.003, 0.997 })
            for (int n = 1; n <= 60; ++n)
            {
                const double amp = (n == 12 ? 6.0 : 1.0) * 0.03 / n;
                addSine (x.l, 110.0 * det * n, amp, fs);
                addSine (x.r, 110.0 * det * n, amp, fs);
            }
        const auto dry = x;
        run (s, x);
        const int lat = s.latencySamples();
        const auto pin = psd (dry.l, skip, len - skip - (size_t) lat - 1);
        const auto pout = psd (x.l, skip + (size_t) lat, len - skip - (size_t) lat - 1);
        const double ring = bandDb (pin, fs, 1320.0) - bandDb (pout, fs, 1320.0);
        double others = 0.0;
        int count = 0;
        for (int n : { 8, 9, 10, 11, 13, 14, 15, 16 })
        {
            others += bandDb (pin, fs, 110.0 * n) - bandDb (pout, fs, 110.0 * n);
            ++count;
        }
        others /= count;
        CHECK (ring > others + 5.0 && others < 3.0, "pad: ringing partial cut %.1f dB, its healthy neighbours %.1f dB on average", ring, others);
    }

    {
        // Outside the range nothing happens.
        Suppressor s;
        SuppressorParams p;
        p.depth = 100.0;
        p.lowHz = 300.0;
        p.highHz = 3000.0;
        s.setParams (p);
        s.prepare (fs, 1);
        auto x = noise (len, 0.05, 9);
        addSine (x.l, 8000.0, 0.1, fs);
        addSine (x.r, 8000.0, 0.1, fs);
        const auto dry = x;
        run (s, x);
        const int lat = s.latencySamples();
        const auto pin = psd (dry.l, skip, len - skip - (size_t) lat - 1);
        const auto pout = psd (x.l, skip + (size_t) lat, len - skip - (size_t) lat - 1);
        const double cut = bandDb (pin, fs, 8000.0) - bandDb (pout, fs, 8000.0);
        CHECK (std::abs (cut) < 0.5, "whistle outside the range (8 kHz, range 300 Hz - 3 kHz) untouched: %.2f dB", cut);
    }

    {
        // Depth and selectivity behave in the right direction.
        auto measure = [&] (double depth, double sel) {
            Suppressor s;
            SuppressorParams p;
            p.depth = depth;
            p.selectivity = sel;
            s.setParams (p);
            s.prepare (fs, 1);
            auto x = noise (len / 2, 0.05, 11);
            addSine (x.l, 2500.0, 0.05, fs);
            addSine (x.r, 2500.0, 0.05, fs);
            const auto dry = x;
            run (s, x);
            const int lat = s.latencySamples();
            const auto pin = psd (dry.l, skip, len / 2 - skip - (size_t) lat - 1);
            const auto pout = psd (x.l, skip + (size_t) lat, len / 2 - skip - (size_t) lat - 1);
            return bandDb (pin, fs, 2500.0) - bandDb (pout, fs, 2500.0);
        };
        const double d30 = measure (30.0, 50.0), d90 = measure (90.0, 50.0);
        CHECK (d90 > d30 + 2.0, "more depth cuts more (30 %%: %.1f dB, 90 %%: %.1f dB)", d30, d90);
        const double s20 = measure (70.0, 20.0), s90 = measure (70.0, 90.0);
        CHECK (s20 > s90 + 1.0, "higher selectivity leaves more alone (20 %%: %.1f dB, 90 %%: %.1f dB)", s20, s90);
    }

    std::printf ("\n[Mix, delta, stereo modes]\n");
    {
        auto make = [&] {
            auto x = noise ((size_t) fs * 2, 0.05, 3);
            addSine (x.l, 1500.0, 0.1, fs);
            addSine (x.r, 1500.0, 0.1, fs);
            return x;
        };
        SuppressorParams p;
        p.depth = 100.0;
        Suppressor a, b, c;
        a.setParams (p);
        a.prepare (fs, 1);
        auto wet = make();
        run (a, wet);
        p.delta = true;
        b.setParams (p);
        b.prepare (fs, 1);
        auto del = make();
        run (b, del);
        const auto dry = make();
        const int lat = a.latencySamples();
        double err = 0.0, removed = 0.0;
        for (size_t i = (size_t) lat; i < dry.l.size(); ++i)
        {
            err = std::max (err, (double) std::abs (wet.l[i] + del.l[i] - dry.l[i - (size_t) lat]));
            removed = std::max (removed, (double) std::abs (del.l[i]));
        }
        CHECK (err < 1.0e-5 && removed > 0.01, "processed + delta = dry (%.1f dB error); delta carries %.1f dBFS peak", gainToDb (err), gainToDb (removed));

        p.delta = false;
        p.mix = 0.0;
        c.setParams (p);
        c.prepare (fs, 1);
        auto x = make();
        run (c, x);
        err = 0.0;
        for (size_t i = (size_t) lat; i < dry.l.size(); ++i)
            err = std::max (err, (double) std::abs (x.l[i] - dry.l[i - (size_t) lat]));
        CHECK (err < 1.0e-5, "mix 0 %% at full depth is the dry signal (%.1f dB)", gainToDb (err));
    }
    {
        // A whistle only on the left: Stereo mode cuts left only, Linked cuts both.
        auto test = [&] (StereoMode mode, double& cutL, double& cutR) {
            Suppressor s;
            SuppressorParams p;
            p.depth = 80.0;
            p.mode = mode;
            s.setParams (p);
            s.prepare (fs, 1);
            auto x = noise (len / 2, 0.05, 5);
            addSine (x.l, 3000.0, 0.1, fs);
            addSine (x.r, 900.0, 0.1, fs);
            const auto dry = x;
            run (s, x);
            const int lat = s.latencySamples();
            const size_t n = len / 2 - skip - (size_t) lat - 1;
            const auto il = psd (dry.l, skip, n), ol = psd (x.l, skip + (size_t) lat, n);
            const auto ir = psd (dry.r, skip, n), orr = psd (x.r, skip + (size_t) lat, n);
            cutL = bandDb (il, fs, 3000.0) - bandDb (ol, fs, 3000.0);   // left's own whistle
            cutR = bandDb (ir, fs, 3000.0) - bandDb (orr, fs, 3000.0);  // right at the left whistle frequency
        };
        double l, r;
        test (StereoMode::Stereo, l, r);
        CHECK (l > 8.0 && std::abs (r) < 1.5, "stereo mode: left whistle cut %.1f dB, right channel at that frequency %.2f dB", l, r);
        test (StereoMode::Linked, l, r);
        CHECK (l > 6.0 && r > 2.0, "linked mode: same cut on both sides (left %.1f dB, right %.1f dB)", l, r);

        // Mid/side: a whistle only in the side signal is found there.
        Suppressor s;
        SuppressorParams p;
        p.depth = 80.0;
        p.mode = StereoMode::MidSide;
        s.setParams (p);
        s.prepare (fs, 1);
        auto x = noise (len / 2, 0.05, 6);
        for (size_t i = 0; i < x.l.size(); ++i)
        {
            const float w = (float) (0.1 * std::sin (kTwoPi * 2000.0 * (double) i / fs));
            x.l[i] += w;
            x.r[i] -= w;
        }
        const auto dry = x;
        run (s, x);
        const int lat = s.latencySamples();
        std::vector<float> sIn (x.l.size()), sOut (x.l.size());
        for (size_t i = 0; i < x.l.size(); ++i)
        {
            sIn[i] = 0.5f * (dry.l[i] - dry.r[i]);
            sOut[i] = 0.5f * (x.l[i] - x.r[i]);
        }
        const size_t n = len / 2 - skip - (size_t) lat - 1;
        const double cut = bandDb (psd (sIn, skip, n), fs, 2000.0) - bandDb (psd (sOut, skip + (size_t) lat, n), fs, 2000.0);
        CHECK (cut > 8.0, "mid/side mode: whistle in the side signal cut by %.1f dB", cut);

        // switching modes while running stays clean
        Suppressor m;
        SuppressorParams q;
        q.depth = 60.0;
        m.setParams (q);
        m.prepare (fs, 1);
        auto y = noise ((size_t) fs, 0.1, 8);
        bool finite = true;
        float peak = 0.0f;
        for (size_t i = 0; i < y.l.size(); i += 512)
        {
            q.mode = (StereoMode) ((i / 4096) % 3);
            m.setParams (q);
            m.process (y.l.data() + i, y.r.data() + i, (int) std::min ((size_t) 512, y.l.size() - i));
        }
        for (size_t i = 0; i < y.l.size(); ++i)
        {
            finite = finite && std::isfinite (y.l[i]) && std::isfinite (y.r[i]);
            peak = std::max (peak, std::abs (y.l[i]));
        }
        CHECK (finite && peak < 0.8f, "switching modes every 85 ms: clean output (peak %.2f)", peak);
    }

    std::printf ("\n[CPU]\n");
    for (int q = 0; q < 3; ++q)
    {
        Suppressor s;
        SuppressorParams p;
        p.depth = 60.0;
        p.mode = StereoMode::Stereo;
        s.setParams (p);
        s.prepare (48000.0, q);
        auto x = noise ((size_t) 48000 * 10, 0.1, 2);
        const auto t0 = std::chrono::steady_clock::now();
        run (s, x, 256);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        CHECK (sec < 1.0, "quality %d, stereo mode: 10 s of audio in %.0f ms (%.1f %% of real time)", q, sec * 1000.0, sec * 10.0);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
