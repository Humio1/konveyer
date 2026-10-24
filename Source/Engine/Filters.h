#pragma once
// Biquad sections and filter designs.
//
// The EQ shapes (bell, shelves, tilt, high cut, notch) use "matched" designs: the poles come from the
// analog prototype by impulse invariance and the zeros are solved so the digital magnitude equals the
// analog one at DC, at Nyquist and at the corner frequency (after M. Vicanek, "Matched Second Order
// Digital Filters", 2016). Unlike the bilinear transform they do not squeeze bells and shelves near
// Nyquist, so a boost at 16 kHz looks and sounds like the analog curve at 44.1 kHz too.
// Low cuts and crossovers use the bilinear transform, which keeps the exact sum identities
// Linkwitz-Riley crossovers rely on.

#include "Common.h"
#include <complex>

namespace kv
{
struct Coeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
};

/** Transposed direct form II, double precision. */
struct BiquadState
{
    double z1 = 0.0, z2 = 0.0;

    inline double process (const Coeffs& c, double x) noexcept
    {
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }

    void reset() noexcept { z1 = z2 = 0.0; }
};

std::complex<double> response (const Coeffs& c, double w);  // w in radians per sample
double magnitude (const Coeffs& c, double w);

namespace design
{
// Matched designs. Frequencies are limited to 0.45 * fs.
Coeffs peak (double freq, double gainDb, double q, double fs);
Coeffs lowShelf (double freq, double gainDb, double q, double fs);
Coeffs highShelf (double freq, double gainDb, double q, double fs);
Coeffs tilt (double freq, double gainDb, double fs);  // first order: +gain above, -gain below the pivot
Coeffs lowpass2 (double freq, double q, double fs);
Coeffs lowpass1 (double freq, double fs);
Coeffs notch (double freq, double q, double fs);

// Bilinear (RBJ cookbook) designs.
Coeffs lowpass2BLT (double freq, double q, double fs);
Coeffs highpass2BLT (double freq, double q, double fs);
Coeffs highpass1BLT (double freq, double fs);
Coeffs allpass2BLT (double freq, double q, double fs);
Coeffs bandpassBLT (double freq, double q, double fs);

/** Counts how often the matched solver had to fall back (used by the offline tests). */
int fallbackCount();
} // namespace design

// Analog prototypes (squared magnitude at frequency ratio x = f / f0), used by the tests.
namespace analog
{
double peak2 (double x, double gainDb, double q);
double lowShelf2 (double x, double gainDb, double q);
double highShelf2 (double x, double gainDb, double q);
} // namespace analog
} // namespace kv
