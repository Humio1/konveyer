#pragma once
// 12-band parametric EQ: bell, shelves, tilt, cuts (6-48 dB/oct), notch.
// Each band can sit on Stereo, Mid, Side, Left or Right and can work dynamically (gain follows the
// band's own level above a threshold, within a range), like a dynamic EQ.

#include "Filters.h"

namespace kv
{
constexpr int kEqBands = 12;

enum class EqType : int { Bell = 0, LowShelf, HighShelf, LowCut, HighCut, Notch, TiltShelf };
constexpr int kEqTypeCount = 7;

enum class Placement : int { Stereo = 0, Mid, Side, Left, Right };
constexpr int kPlacementCount = 5;

constexpr int kSlopeCount = 6;  // 6, 12, 18, 24, 36, 48 dB/oct
int slopeOrder (int slopeIndex);
int slopeDbPerOct (int slopeIndex);

inline bool typeIsCut (EqType t) { return t == EqType::LowCut || t == EqType::HighCut; }
inline bool typeHasGain (EqType t) { return t == EqType::Bell || t == EqType::LowShelf || t == EqType::HighShelf || t == EqType::TiltShelf; }
inline bool typeSupportsDynamics (EqType t) { return t == EqType::Bell || t == EqType::LowShelf || t == EqType::HighShelf; }

struct EqBandParams
{
    bool on = false;
    EqType type = EqType::Bell;
    double freq = 1000.0, gainDb = 0.0, q = 0.71;
    int slope = 1;
    Placement place = Placement::Stereo;
    bool dyn = false;
    double dynThresholdDb = -30.0, dynRangeDb = -6.0;
};

struct EqParams
{
    std::array<EqBandParams, kEqBands> bands;
    double dynAttackMs = 5.0, dynReleaseMs = 120.0;
    double outputDb = 0.0;
};

struct BandDesign
{
    std::array<Coeffs, 4> sec;
    int count = 0;
};

/** Static design of one band. Used by the audio engine and by the curve display. */
BandDesign designBand (EqType type, double freq, double gainDb, double q, int slope, double fs);
double bandMagnitude (const BandDesign& d, double w);

class Equalizer
{
public:
    void prepare (double sampleRate);
    void reset();
    void setParams (const EqParams& p) { params = p; }
    void process (double* L, double* R, int n);

    /** Current dynamic gain offset of a band in dB (for the display ring). */
    float dynamicGainDb (int band) const { return dynDisplay[(size_t) band].load (std::memory_order_relaxed); }

private:
    static constexpr int kSub = 16;

    struct Band
    {
        EqType type = EqType::Bell;
        int slope = 1;
        Placement place = Placement::Stereo;
        double freqLog = 0.0, qLog = 0.0, gainDb = 0.0;
        bool primed = false;
        double mix = 0.0, mixStart = 0.0;
        double env = 0.0, dynDb = 0.0;
        BandDesign design;
        double dFreq = -1.0, dQ = -1.0, dGain = 1.0e9;
        std::array<std::array<BiquadState, 4>, 2> st {};
        Coeffs det;
        double detFreq = -1.0, detQ = -1.0;
        int detKind = -1;
        std::array<BiquadState, 2> detSt {};
        bool active = false;

        void resetStates()
        {
            for (auto& ch : st)
                for (auto& s : ch)
                    s.reset();
            for (auto& s : detSt)
                s.reset();
        }
    };

    void updateBand (Band& b, const EqBandParams& p, const double* l, const double* r, int len);
    static void runBand (Band& b, double* x, int ch, int len);

    double fs = 48000.0;
    EqParams params;
    std::array<Band, kEqBands> bands;
    double smooth = 0.0, mixStep = 0.0, atk = 0.0, rel = 0.0;
    LinearRamp out;
    std::array<double, kSub> mBuf {}, sBuf {};
    std::array<std::atomic<float>, kEqBands> dynDisplay {};
};
} // namespace kv
