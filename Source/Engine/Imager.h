#pragma once
// 3-band stereo imager: mid and side are split with identical Linkwitz-Riley filters, the side bands
// are scaled (0 % = mono, 100 % = unchanged, 200 % = twice as wide) and the low band can be forced mono.

#include "Dynamics.h"

namespace kv
{
struct ImagerParams
{
    std::array<double, 2> xover { 150.0, 5000.0 };
    std::array<double, 3> width { 1.0, 1.0, 1.0 };
    bool monoBass = false;
};

class Imager
{
public:
    void prepare (double sampleRate);
    void reset();
    void setParams (const ImagerParams& p) { params = p; }
    void process (double* L, double* R, int n);

    /** Running correlation of the output, -1 .. +1. */
    std::atomic<float> correlation { 1.0f };

private:
    static constexpr int kSub = 16;
    void updateCrossovers (bool snap);

    double fs = 48000.0;
    ImagerParams params;
    std::array<double, 2> xLog {};
    std::array<XoverCoeffs, 2> xc {};
    LRSplitter<3> splitM {}, splitS {};
    std::array<LinearRamp, 3> width {};
    double smooth = 0.0, corrCoeff = 0.0;
    double sLR = 0.0, sLL = 0.0, sRR = 0.0;
    bool primed = false;
};
} // namespace kv
