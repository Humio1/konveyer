#include "Equalizer.h"

namespace kv
{
int slopeOrder (int s)
{
    static const int orders[kSlopeCount] = { 1, 2, 3, 4, 6, 8 };
    return orders[clampv (s, 0, kSlopeCount - 1)];
}

int slopeDbPerOct (int s) { return 6 * slopeOrder (s); }

BandDesign designBand (EqType type, double freq, double gainDb, double q, int slope, double fs)
{
    BandDesign d;
    switch (type)
    {
        case EqType::Bell:      d.sec[0] = design::peak (freq, gainDb, clampv (q, 0.025, 40.0), fs); d.count = 1; break;
        case EqType::LowShelf:  d.sec[0] = design::lowShelf (freq, gainDb, clampv (q, 0.1, 2.0), fs); d.count = 1; break;
        case EqType::HighShelf: d.sec[0] = design::highShelf (freq, gainDb, clampv (q, 0.1, 2.0), fs); d.count = 1; break;
        case EqType::TiltShelf: d.sec[0] = design::tilt (freq, gainDb, fs); d.count = 1; break;
        case EqType::Notch:     d.sec[0] = design::notch (freq, clampv (q, 0.025, 40.0), fs); d.count = 1; break;
        case EqType::LowCut:
        case EqType::HighCut:
        {
            // Butterworth cascade; the Q control adds resonance to the sharpest section.
            const int order = slopeOrder (slope);
            const int pairs = order / 2;
            const bool odd = (order % 2) != 0;
            const double resonance = clampv (q, 0.1, 10.0) / 0.70710678118654752;
            const bool low = type == EqType::LowCut;
            int n = 0;
            if (odd)
                d.sec[(size_t) n++] = low ? design::highpass1BLT (freq, fs) : design::lowpass1 (freq, fs);
            for (int k = 1; k <= pairs; ++k)
            {
                const double psi = odd ? k * kPi / order : (2.0 * k - 1.0) * kPi / (2.0 * order);
                double qk = 1.0 / (2.0 * std::cos (psi));
                if (k == pairs)
                    qk *= resonance;
                d.sec[(size_t) n++] = low ? design::highpass2BLT (freq, qk, fs) : design::lowpass2 (freq, qk, fs);
            }
            d.count = n;
            break;
        }
    }
    return d;
}

double bandMagnitude (const BandDesign& d, double w)
{
    double m = 1.0;
    for (int k = 0; k < d.count; ++k)
        m *= magnitude (d.sec[(size_t) k], w);
    return m;
}

//==============================================================================
void Equalizer::prepare (double sampleRate)
{
    fs = sampleRate;
    smooth = 1.0 - std::exp (-kSub / (0.03 * fs));
    mixStep = kSub / (0.015 * fs);
    reset();
}

void Equalizer::reset()
{
    for (auto& b : bands)
    {
        b.resetStates();
        b.primed = false;
        b.mix = b.mixStart = 0.0;
        b.env = 0.0;
        b.dynDb = 0.0;
        b.dFreq = -1.0;
        b.detFreq = -1.0;
        b.active = false;
    }
    out.reset (dbToGain (params.outputDb));
    for (auto& d : dynDisplay)
        d.store (0.0f);
}

void Equalizer::updateBand (Band& b, const EqBandParams& p, const double* l, const double* r, int len)
{
    bool structural = p.type != b.type || p.place != b.place || (typeIsCut (p.type) && p.slope != b.slope);

    if (structural && b.mix <= 0.0)
    {
        b.type = p.type;
        b.slope = p.slope;
        b.place = p.place;
        b.resetStates();
        b.primed = false;
        b.dFreq = -1.0;
        b.detFreq = -1.0;
        b.env = 0.0;
        b.dynDb = 0.0;
        structural = false;
    }

    const double target = (p.on && ! structural) ? 1.0 : 0.0;
    b.mixStart = b.mix;
    b.mix = b.mix < target ? std::min (target, b.mix + mixStep) : std::max (target, b.mix - mixStep);
    b.active = b.mixStart > 0.0 || b.mix > 0.0;

    if (! b.active)
    {
        b.primed = false;
        b.env = 0.0;
        b.dynDb = 0.0;
        return;
    }

    // Glide frequency, Q (log domain) and gain towards the targets.
    const double tf = std::log (clampv (p.freq, 5.0, 0.45 * fs));
    const double tq = std::log (clampv (p.q, 0.025, 40.0));
    const double tg = clampv (p.gainDb, -30.0, 30.0);
    if (! b.primed)
    {
        b.freqLog = tf;
        b.qLog = tq;
        b.gainDb = tg;
        b.primed = true;
    }
    else
    {
        b.freqLog += (tf - b.freqLog) * smooth;
        b.qLog += (tq - b.qLog) * smooth;
        b.gainDb += (tg - b.gainDb) * smooth;
        if (std::abs (tf - b.freqLog) < 1.0e-5) b.freqLog = tf;
        if (std::abs (tq - b.qLog) < 1.0e-5) b.qLog = tq;
        if (std::abs (tg - b.gainDb) < 1.0e-4) b.gainDb = tg;
    }

    const double f = std::exp (b.freqLog), q = std::exp (b.qLog);

    // Dynamic section: the band listens to its own region of the EQ input.
    const bool dynamic = p.dyn && typeSupportsDynamics (b.type);
    if (dynamic)
    {
        const int kind = b.type == EqType::Bell ? 0 : (b.type == EqType::LowShelf ? 1 : 2);
        if (kind != b.detKind || std::abs (f - b.detFreq) > 1.0e-6 * f || std::abs (q - b.detQ) > 1.0e-6)
        {
            b.det = kind == 0 ? design::bandpassBLT (f, std::max (0.3, q), fs)
                  : kind == 1 ? design::lowpass2BLT (f, 0.707, fs)
                              : design::highpass2BLT (f, 0.707, fs);
            b.detKind = kind;
            b.detFreq = f;
            b.detQ = q;
        }

        double env = b.env;
        for (int i = 0; i < len; ++i)
        {
            double x;
            switch (b.place)
            {
                case Placement::Stereo: x = std::max (std::abs (b.detSt[0].process (b.det, l[i])), std::abs (b.detSt[1].process (b.det, r[i]))); break;
                case Placement::Mid:    x = std::abs (b.detSt[0].process (b.det, 0.5 * (l[i] + r[i]))); break;
                case Placement::Side:   x = std::abs (b.detSt[0].process (b.det, 0.5 * (l[i] - r[i]))); break;
                case Placement::Left:   x = std::abs (b.detSt[0].process (b.det, l[i])); break;
                case Placement::Right:
                default:                x = std::abs (b.detSt[1].process (b.det, r[i])); break;
            }
            env = x > env ? atk * env + (1.0 - atk) * x : rel * env + (1.0 - rel) * x;
        }
        b.env = env;

        const double over = gainToDb (env) - p.dynThresholdDb;
        constexpr double knee = 6.0;
        double amount = 0.0;
        if (over >= 0.5 * knee)
            amount = over;
        else if (over > -0.5 * knee)
            amount = sq (over + 0.5 * knee) / (2.0 * knee);
        const double range = clampv (p.dynRangeDb, -24.0, 24.0);
        b.dynDb = range < 0.0 ? -std::min (-range, amount) : std::min (range, amount);
    }
    else
    {
        b.env = 0.0;
        b.dynDb = 0.0;
    }

    const double g = typeHasGain (b.type) ? b.gainDb + b.dynDb : 0.0;
    if (std::abs (f - b.dFreq) > 0.0 || std::abs (q - b.dQ) > 0.0 || std::abs (g - b.dGain) > 0.0)
    {
        b.design = designBand (b.type, f, g, q, b.slope, fs);
        b.dFreq = f;
        b.dQ = q;
        b.dGain = g;
    }
}

void Equalizer::runBand (Band& b, double* x, int ch, int len)
{
    auto& st = b.st[(size_t) ch];
    const int count = b.design.count;
    const bool full = b.mixStart >= 1.0 && b.mix >= 1.0;
    for (int i = 0; i < len; ++i)
    {
        const double in = x[i];
        double y = in;
        for (int k = 0; k < count; ++k)
            y = st[(size_t) k].process (b.design.sec[(size_t) k], y);
        if (full)
            x[i] = y;
        else
        {
            const double m = b.mixStart + (b.mix - b.mixStart) * (i + 1) / len;
            x[i] = in + m * (y - in);
        }
    }
}

void Equalizer::process (double* L, double* R, int n)
{
    atk = onePoleCoeff (std::max (0.1, params.dynAttackMs) * 0.001, fs);
    rel = onePoleCoeff (std::max (1.0, params.dynReleaseMs) * 0.001, fs);

    for (int start = 0; start < n; start += kSub)
    {
        const int len = std::min (kSub, n - start);
        double* l = L + start;
        double* r = R + start;

        bool anyMS = false;
        for (int i = 0; i < kEqBands; ++i)
        {
            updateBand (bands[(size_t) i], params.bands[(size_t) i], l, r, len);
            const auto& b = bands[(size_t) i];
            anyMS = anyMS || (b.active && (b.place == Placement::Mid || b.place == Placement::Side));
        }

        // Left/right domain bands.
        for (auto& b : bands)
        {
            if (! b.active || b.place == Placement::Mid || b.place == Placement::Side)
                continue;
            if (b.place != Placement::Right) runBand (b, l, 0, len);
            if (b.place != Placement::Left)  runBand (b, r, 1, len);
        }

        // Mid/side domain bands.
        if (anyMS)
        {
            for (int i = 0; i < len; ++i)
            {
                mBuf[(size_t) i] = 0.5 * (l[i] + r[i]);
                sBuf[(size_t) i] = 0.5 * (l[i] - r[i]);
            }
            for (auto& b : bands)
            {
                if (! b.active)
                    continue;
                if (b.place == Placement::Mid)  runBand (b, mBuf.data(), 0, len);
                if (b.place == Placement::Side) runBand (b, sBuf.data(), 1, len);
            }
            for (int i = 0; i < len; ++i)
            {
                l[i] = mBuf[(size_t) i] + sBuf[(size_t) i];
                r[i] = mBuf[(size_t) i] - sBuf[(size_t) i];
            }
        }
    }

    for (int i = 0; i < kEqBands; ++i)
        dynDisplay[(size_t) i].store ((float) bands[(size_t) i].dynDb, std::memory_order_relaxed);

    out.setTarget (dbToGain (params.outputDb), (int) (0.02 * fs));
    if (out.isSmoothing() || std::abs (out.current - 1.0) > 1.0e-12)
        for (int i = 0; i < n; ++i)
        {
            const double g = out.next();
            L[i] *= g;
            R[i] *= g;
        }
}
} // namespace kv
