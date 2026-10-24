#include "Engine.h"

namespace kv
{
bool isValidOrder (const Order& o)
{
    std::array<bool, kNumModules> seen {};
    for (int m : o)
    {
        if (m < 0 || m >= kNumModules || seen[(size_t) m])
            return false;
        seen[(size_t) m] = true;
    }
    return true;
}

int packOrder (const Order& o)
{
    int p = 0;
    for (int i = 0; i < kNumModules; ++i)
        p |= (o[(size_t) i] & 7) << (3 * i);
    return p;
}

Order unpackOrder (int packed)
{
    Order o {};
    for (int i = 0; i < kNumModules; ++i)
        o[(size_t) i] = (packed >> (3 * i)) & 7;
    return isValidOrder (o) ? o : defaultOrder();
}

Engine::Engine() = default;

void Engine::prepare (double sampleRate)
{
    fs = sampleRate;
    eq.prepare (fs);
    comp.prepare (fs);
    mbc.prepare (fs);
    imager.prepare (fs);
    limiter.prepare (fs);
    loudness.prepare (fs);
    reset();
}

void Engine::reset()
{
    eq.setParams (params.eq);
    comp.setParams (params.comp);
    mbc.setParams (params.mbc);
    imager.setParams (params.imager);
    limiter.setParams (params.limiter);
    eq.reset();
    comp.reset();
    mbc.reset();
    imager.reset();
    limiter.reset();
    loudness.reset();
    for (size_t m = 0; m < (size_t) kNumModules; ++m)
        onRamp[m].reset (params.on[m] ? 1.0 : 0.0);
    inGain.reset (dbToGain (params.inputDb));
    outGain.reset (dbToGain (params.outputDb));
    orderGain.reset (1.0);
    order = pendingOrder = isValidOrder (params.order) ? params.order : defaultOrder();
    fadingForOrder = false;
}

void Engine::runModule (int m, int n)
{
    double* l = bufL.data();
    double* r = bufR.data();

    if (m == kModEq)
        for (int i = 0; i < n; ++i)
            tapEqPre.push ((float) (0.5 * (l[i] + r[i])));
    else if (m == kModMbc)
        for (int i = 0; i < n; ++i)
            tapMbc.push ((float) (0.5 * (l[i] + r[i])));

    if (m == kModLimiter)
    {
        // The limiter always runs so the plugin's latency stays the same when it is switched off.
        limiter.process (l, r, n, params.on[(size_t) m]);
        return;
    }

    auto& ramp = onRamp[(size_t) m];
    ramp.setTarget (params.on[(size_t) m] ? 1.0 : 0.0, (int) (0.02 * fs));
    const bool idle = ramp.current <= 0.0 && ramp.target <= 0.0 && ! ramp.isSmoothing();

    if (! idle)
    {
        const bool full = ramp.current >= 1.0 && ! ramp.isSmoothing();
        if (! full)
        {
            std::copy (l, l + n, dryL.begin());
            std::copy (r, r + n, dryR.begin());
        }

        switch (m)
        {
            case kModEq:     eq.process (l, r, n); break;
            case kModComp:   comp.process (l, r, n); break;
            case kModMbc:    mbc.process (l, r, n); break;
            case kModImager: imager.process (l, r, n); break;
            default: break;
        }

        if (! full)
        {
            for (int i = 0; i < n; ++i)
            {
                const double g = ramp.next();
                l[i] = dryL[(size_t) i] + g * (l[i] - dryL[(size_t) i]);
                r[i] = dryR[(size_t) i] + g * (r[i] - dryR[(size_t) i]);
            }
            if (ramp.current <= 0.0)
            {
                // Fully off now: start from a clean state next time it is switched on.
                switch (m)
                {
                    case kModEq:     eq.reset(); break;
                    case kModComp:   comp.reset(); break;
                    case kModMbc:    mbc.reset(); break;
                    case kModImager: imager.reset(); break;
                    default: break;
                }
            }
        }
    }

    if (m == kModEq)
        for (int i = 0; i < n; ++i)
            tapEqPost.push ((float) (0.5 * (l[i] + r[i])));
    else if (m == kModImager)
        for (int i = 0; i < n; ++i)
        {
            tapScope.push ((float) l[i]);
            tapScope.push ((float) r[i]);
        }
}

void Engine::process (float* L, float* R, int n)
{
    n = std::min (n, kMaxChunk);
    const int ramp20 = (int) (0.02 * fs);
    inGain.setTarget (dbToGain (params.inputDb), ramp20);
    outGain.setTarget (dbToGain (params.outputDb), ramp20);

    // A new station order: dip the output for a few milliseconds, swap, come back up.
    const Order wanted = isValidOrder (params.order) ? params.order : order;
    if (wanted != order)
    {
        pendingOrder = wanted;
        if (! fadingForOrder)
        {
            fadingForOrder = true;
            orderGain.setTarget (0.0, (int) (0.006 * fs));
        }
    }
    else if (fadingForOrder)
    {
        fadingForOrder = false;  // changed back before the swap happened
        orderGain.setTarget (1.0, (int) (0.006 * fs));
    }

    double pkL = 0.0, pkR = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double g = inGain.next();
        bufL[(size_t) i] = L[i] * g;
        bufR[(size_t) i] = R[i] * g;
        pkL = std::max (pkL, std::abs (bufL[(size_t) i]));
        pkR = std::max (pkR, std::abs (bufR[(size_t) i]));
    }
    meters.inL.update ((float) gainToDb (pkL, -120.0));
    meters.inR.update ((float) gainToDb (pkR, -120.0));

    eq.setParams (params.eq);
    comp.setParams (params.comp);
    mbc.setParams (params.mbc);
    imager.setParams (params.imager);
    limiter.setParams (params.limiter);

    for (int slot = 0; slot < kNumModules; ++slot)
        runModule (order[(size_t) slot], n);

    // Output gain, dither (TPDF, quantised to the chosen word length), meters.
    const double lsb = params.ditherBits > 0 ? std::ldexp (1.0, -(params.ditherBits - 1)) : 0.0;
    auto rnd = [this]
    {
        ditherState ^= ditherState << 13;
        ditherState ^= ditherState >> 17;
        ditherState ^= ditherState << 5;
        return ditherState * (1.0 / 4294967296.0);
    };

    pkL = pkR = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double g = outGain.next() * orderGain.next();
        double yl = bufL[(size_t) i] * g, yr = bufR[(size_t) i] * g;
        if (lsb > 0.0)
        {
            yl = std::floor (yl / lsb + (rnd() - rnd()) + 0.5) * lsb;
            yr = std::floor (yr / lsb + (rnd() - rnd()) + 0.5) * lsb;
        }
        bufL[(size_t) i] = yl;
        bufR[(size_t) i] = yr;
        pkL = std::max (pkL, std::abs (yl));
        pkR = std::max (pkR, std::abs (yr));
    }

    loudness.process (bufL.data(), bufR.data(), n);

    for (int i = 0; i < n; ++i)
    {
        L[i] = (float) bufL[(size_t) i];
        R[i] = (float) bufR[(size_t) i];
    }
    meters.outL.update ((float) gainToDb (pkL, -120.0));
    meters.outR.update ((float) gainToDb (pkR, -120.0));

    if (fadingForOrder && orderGain.current <= 0.0 && ! orderGain.isSmoothing())
    {
        order = pendingOrder;
        fadingForOrder = false;
        orderGain.setTarget (1.0, (int) (0.006 * fs));
    }
}
} // namespace kv
