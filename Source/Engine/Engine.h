#pragma once
// The KONVEYER line: five stations (EQ, compressor, multiband, imager, limiter) in any order,
// with input/output gain, dither and loudness metering at the end of the line.

#include "Equalizer.h"
#include "Dynamics.h"
#include "Imager.h"
#include "Limiter.h"

namespace kv
{
enum Module : int { kModEq = 0, kModComp, kModMbc, kModImager, kModLimiter, kNumModules };

using Order = std::array<int, kNumModules>;

/** True when 'o' contains every module exactly once. */
bool isValidOrder (const Order& o);
int packOrder (const Order& o);
Order unpackOrder (int packed);
inline Order defaultOrder() { return { kModEq, kModComp, kModMbc, kModImager, kModLimiter }; }

struct EngineParams
{
    double inputDb = 0.0, outputDb = 0.0;
    int ditherBits = 0;  // 0 = off, 16 or 24
    std::array<bool, kNumModules> on { true, true, true, true, true };
    Order order = defaultOrder();
    EqParams eq;
    CompParams comp;
    MbcParams mbc;
    ImagerParams imager;
    LimiterParams limiter;
};

struct Meters
{
    AtomicMax inL, inR, outL, outR;  // peak dBFS since the UI last looked
};

class Engine
{
public:
    Engine();

    void prepare (double sampleRate);
    void reset();
    void setParams (const EngineParams& p) { params = p; }

    /** Processes up to kMaxChunk samples in place. */
    void process (float* L, float* R, int n);

    int latencySamples() const { return limiter.latency(); }
    void resetLoudness() { loudness.resetRequest.store (true); }
    const Order& currentOrder() const { return order; }

    // Read-only access for meters and displays.
    const Equalizer& getEq() const { return eq; }
    Compressor& getComp() { return comp; }
    MultibandCompressor& getMbc() { return mbc; }
    Imager& getImager() { return imager; }
    Limiter& getLimiter() { return limiter; }
    LoudnessMeter& getLoudness() { return loudness; }

    Meters meters;
    TapRing<16384> tapEqPre, tapEqPost, tapMbc;  // mono, for the analysers
    TapRing<16384> tapScope;                     // interleaved L/R at the imager output

private:
    void runModule (int module, int n);

    double fs = 48000.0;
    EngineParams params;
    Equalizer eq;
    Compressor comp;
    MultibandCompressor mbc;
    Imager imager;
    Limiter limiter;
    LoudnessMeter loudness;

    std::array<double, kMaxChunk> bufL {}, bufR {}, dryL {}, dryR {};
    std::array<LinearRamp, kNumModules> onRamp {};
    LinearRamp inGain, outGain, orderGain;
    Order order = defaultOrder(), pendingOrder = defaultOrder();
    bool fadingForOrder = false;
    uint32_t ditherState = 0x9e3779b9u;
};
} // namespace kv
