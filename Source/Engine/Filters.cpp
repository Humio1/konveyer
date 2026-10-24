#include "Filters.h"

namespace kv
{
std::complex<double> response (const Coeffs& c, double w)
{
    const std::complex<double> z1 = std::polar (1.0, -w);
    const std::complex<double> z2 = z1 * z1;
    return (c.b0 + c.b1 * z1 + c.b2 * z2) / (1.0 + c.a1 * z1 + c.a2 * z2);
}

double magnitude (const Coeffs& c, double w) { return std::abs (response (c, w)); }

namespace analog
{
double peak2 (double x, double gainDb, double q)
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double x2 = x * x, d = sq (1.0 - x2);
    return (d + sq (A * x / q)) / (d + sq (x / (A * q)));
}

double lowShelf2 (double x, double gainDb, double q)
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double x2 = x * x, k = A / (q * q);
    return A * A * (sq (A - x2) + k * x2) / (sq (1.0 - A * x2) + k * x2);
}

double highShelf2 (double x, double gainDb, double q)
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double x2 = x * x, k = A / (q * q);
    return A * A * (sq (1.0 - A * x2) + k * x2) / (sq (A - x2) + k * x2);
}
} // namespace analog

namespace design
{
namespace
{
std::atomic<int> fallbacks { 0 };

double limitFreq (double f, double fs) { return clampv (f, 1.0, 0.45 * fs); }

/** Impulse-invariant poles of s^2 + 2 zeta w0 s + w0^2 (w0 in radians per sample). */
void matchedPoles (double w0, double zeta, double& a1, double& a2)
{
    const double r = std::exp (-zeta * w0);
    a2 = r * r;
    if (zeta < 1.0)
        a1 = -2.0 * r * std::cos (w0 * std::sqrt (1.0 - zeta * zeta));
    else
        a1 = -2.0 * r * std::cosh (w0 * std::sqrt (zeta * zeta - 1.0));
}

/** |H|^2 = (B0 phi0 + B1 phi1 + B2 phi2) / (A0 phi0 + A1 phi1 + A2 phi2) -> minimum-phase numerator. */
bool numeratorFrom (double B0, double B1, double B2, Coeffs& c)
{
    if (B0 < 0.0 || B1 < 0.0)
        return false;
    const double r0 = std::sqrt (B0), r1 = std::sqrt (B1);
    const double W = 0.5 * (r0 + r1);
    const double disc = W * W + B2;
    if (disc < 0.0 || W <= 0.0)
        return false;
    c.b0 = 0.5 * (W + std::sqrt (disc));
    c.b1 = 0.5 * (r0 - r1);
    c.b2 = -B2 / (4.0 * c.b0);
    return true;
}

struct PoleTerms
{
    double A0, A1, A2, p, p0, p2, D, Dp;
    PoleTerms (double a1, double a2, double wm)
    {
        A0 = sq (1.0 + a1 + a2);
        A1 = sq (1.0 - a1 + a2);
        A2 = -4.0 * a2;
        const double s = std::sin (0.5 * wm);
        p = s * s;
        p0 = 1.0 - p;
        p2 = 4.0 * p0 * p;
        D = A0 * p0 + A1 * p + A2 * p2;
        Dp = -A0 + A1 + 4.0 * (p0 - p) * A2;
    }
};

/** Matches |H|^2 at DC, at wm and at Nyquist. */
bool solveNyquist (double a1, double a2, double H0sq, double wm, double HMsq, double HNsq, Coeffs& c)
{
    const PoleTerms t (a1, a2, wm);
    const double B0 = t.A0 * H0sq, B1 = t.A1 * HNsq;
    const double B2 = (HMsq * t.D - B0 * t.p0 - B1 * t.p) / t.p2;
    c.a1 = a1;
    c.a2 = a2;
    return numeratorFrom (B0, B1, B2, c);
}

/** Matches |H|^2 at DC, plus value and slope (d|H|^2/dphi) at wm. */
bool solveSlope (double a1, double a2, double H0sq, double wm, double HMsq, double slopePhi, Coeffs& c)
{
    const PoleTerms t (a1, a2, wm);
    const double R1 = HMsq * t.D, R2 = HMsq * t.Dp + t.D * slopePhi;
    const double B0 = t.A0 * H0sq;
    const double B2 = (R1 - R2 * t.p - B0) / (4.0 * t.p * t.p);
    const double B1 = R2 + B0 + 4.0 * (t.p - t.p0) * B2;
    c.a1 = a1;
    c.a2 = a2;
    return numeratorFrom (B0, B1, B2, c);
}

/** Two-point fallback (DC and Nyquist only, single zero). Always realisable. */
void solveTwoPoint (double a1, double a2, double H0, double HN, Coeffs& c)
{
    const double r0 = (1.0 + a1 + a2) * H0, r1 = (1.0 - a1 + a2) * HN;
    c.a1 = a1;
    c.a2 = a2;
    c.b0 = 0.5 * (r0 + r1);
    c.b1 = 0.5 * (r0 - r1);
    c.b2 = 0.0;
}

/** Slope of an analog |H|^2(x) at x = 1, expressed per phi = sin^2(w/2) at w0. */
template <typename Fn>
double slopePerPhi (Fn&& T, double w0)
{
    const double h = 1.0e-5;
    const double dx = (T (1.0 + h) - T (1.0 - h)) / (2.0 * h);
    return (dx / w0) / (0.5 * std::sin (w0));
}

/** 1/H. Valid because the matched numerators are minimum phase. */
Coeffs invert (const Coeffs& c)
{
    Coeffs r;
    r.b0 = 1.0 / c.b0;
    r.b1 = c.a1 / c.b0;
    r.b2 = c.a2 / c.b0;
    r.a1 = c.b1 / c.b0;
    r.a2 = c.b2 / c.b0;
    return r;
}

/** Second-order shelf/bell where the poles sit below the corner: Nyquist match first, slope as backup. */
template <typename Fn>
Coeffs matchedSection (double w0p, double zeta, double H0sq, double w0, Fn&& T, bool slopeFirst)
{
    double a1, a2;
    matchedPoles (w0p, zeta, a1, a2);
    const double HMsq = T (1.0), HNsq = T (kPi / w0);
    Coeffs c;
    bool ok;
    if (slopeFirst)
        ok = solveSlope (a1, a2, H0sq, w0, HMsq, slopePerPhi (T, w0), c) || solveNyquist (a1, a2, H0sq, w0, HMsq, HNsq, c);
    else
        ok = solveNyquist (a1, a2, H0sq, w0, HMsq, HNsq, c) || solveSlope (a1, a2, H0sq, w0, HMsq, slopePerPhi (T, w0), c);
    if (! ok)
    {
        fallbacks.fetch_add (1, std::memory_order_relaxed);
        solveTwoPoint (a1, a2, std::sqrt (H0sq), std::sqrt (HNsq), c);
    }
    return c;
}
} // namespace

int fallbackCount() { return fallbacks.load(); }

// Boost and cut are reciprocal for these prototypes (bell(+g) * bell(-g) = 1, same for shelves and tilt).
// Each shape is designed on the side whose poles lie at or below the corner, so no pole is ever pushed
// past Nyquist, and the other side is the exact inverse.

Coeffs peak (double freq, double gainDb, double q, double fs)
{
    if (std::abs (gainDb) < 1.0e-6)
        return {};
    freq = limitFreq (freq, fs);
    const double w0 = kTwoPi * freq / fs;
    const double g = std::abs (gainDb);
    const double A = std::pow (10.0, g / 40.0);
    const auto c = matchedSection (w0, 1.0 / (2.0 * A * q), 1.0, w0,
                                   [&] (double x) { return analog::peak2 (x, g, q); }, true);
    return gainDb < 0.0 ? invert (c) : c;
}

Coeffs lowShelf (double freq, double gainDb, double q, double fs)
{
    if (std::abs (gainDb) < 1.0e-6)
        return {};
    freq = limitFreq (freq, fs);
    const double w0 = kTwoPi * freq / fs;
    const double g = std::abs (gainDb);  // boost: poles at w0 / sqrt(A) < w0
    const double A = std::pow (10.0, g / 40.0);
    const auto c = matchedSection (w0 / std::sqrt (A), 1.0 / (2.0 * q), A * A * A * A, w0,
                                   [&] (double x) { return analog::lowShelf2 (x, g, q); }, false);
    return gainDb < 0.0 ? invert (c) : c;
}

Coeffs highShelf (double freq, double gainDb, double q, double fs)
{
    if (std::abs (gainDb) < 1.0e-6)
        return {};
    freq = limitFreq (freq, fs);
    const double w0 = kTwoPi * freq / fs;
    const double g = -std::abs (gainDb);  // cut: poles at w0 * sqrt(A) < w0
    const double A = std::pow (10.0, g / 40.0);
    const auto c = matchedSection (w0 * std::sqrt (A), 1.0 / (2.0 * q), 1.0, w0,
                                   [&] (double x) { return analog::highShelf2 (x, g, q); }, false);
    return gainDb > 0.0 ? invert (c) : c;
}

Coeffs tilt (double freq, double gainDb, double fs)
{
    if (std::abs (gainDb) < 1.0e-6)
        return {};
    freq = limitFreq (freq, fs);
    const double w0 = kTwoPi * freq / fs;
    // Designed as the downward tilt (pole below the pivot), inverted for an upward tilt.
    const double g = std::pow (10.0, -std::abs (gainDb) / 10.0);  // high/low ratio, < 1
    const double sg = std::sqrt (g);
    // T(s) = (g s + w0 sqrt g) / (s + w0 sqrt g) / sqrt g ; |T(jx)|^2 = (g x^2 + 1) / (x^2 + g)
    const double p = std::exp (-w0 * sg);
    const double xN = kPi / w0;
    const double T0 = 1.0 / sg;
    const double TN = std::sqrt ((g * xN * xN + 1.0) / (xN * xN + g));
    Coeffs c;
    c.a1 = -p;
    c.a2 = 0.0;
    c.b0 = 0.5 * ((1.0 - p) * T0 + (1.0 + p) * TN);
    c.b1 = 0.5 * ((1.0 - p) * T0 - (1.0 + p) * TN);
    c.b2 = 0.0;
    return gainDb > 0.0 ? invert (c) : c;
}

Coeffs lowpass2 (double freq, double q, double fs)
{
    freq = limitFreq (freq, fs);
    const double w0 = kTwoPi * freq / fs;
    return matchedSection (w0, 1.0 / (2.0 * q), 1.0, w0,
                           [&] (double x) { return 1.0 / (sq (1.0 - x * x) + sq (x / q)); }, false);
}

Coeffs lowpass1 (double freq, double fs)
{
    freq = limitFreq (freq, fs);
    const double w0 = kTwoPi * freq / fs;
    const double p = std::exp (-w0);
    const double hn = 1.0 / std::sqrt (1.0 + sq (kPi / w0));
    Coeffs c;
    c.a1 = -p;
    c.a2 = 0.0;
    c.b0 = 0.5 * ((1.0 - p) + (1.0 + p) * hn);
    c.b1 = 0.5 * ((1.0 - p) - (1.0 + p) * hn);
    c.b2 = 0.0;
    return c;
}

Coeffs notch (double freq, double q, double fs)
{
    freq = limitFreq (freq, fs);
    const double w0 = kTwoPi * freq / fs;
    double a1, a2;
    matchedPoles (w0, 1.0 / (2.0 * q), a1, a2);
    const double cw = std::cos (w0);
    const double k = (1.0 + a1 + a2) / (2.0 - 2.0 * cw);
    Coeffs c;
    c.a1 = a1;
    c.a2 = a2;
    c.b0 = k;
    c.b1 = -2.0 * cw * k;
    c.b2 = k;
    return c;
}

namespace
{
struct Rbj
{
    double cw, alpha, a0;
    Rbj (double freq, double q, double fs)
    {
        const double w0 = kTwoPi * clampv (freq, 1.0, 0.49 * fs) / fs;
        cw = std::cos (w0);
        alpha = std::sin (w0) / (2.0 * q);
        a0 = 1.0 + alpha;
    }
    void poles (Coeffs& c) const { c.a1 = -2.0 * cw / a0; c.a2 = (1.0 - alpha) / a0; }
};
} // namespace

Coeffs lowpass2BLT (double freq, double q, double fs)
{
    const Rbj r (freq, q, fs);
    Coeffs c;
    c.b0 = 0.5 * (1.0 - r.cw) / r.a0;
    c.b1 = (1.0 - r.cw) / r.a0;
    c.b2 = c.b0;
    r.poles (c);
    return c;
}

Coeffs highpass2BLT (double freq, double q, double fs)
{
    const Rbj r (freq, q, fs);
    Coeffs c;
    c.b0 = 0.5 * (1.0 + r.cw) / r.a0;
    c.b1 = -(1.0 + r.cw) / r.a0;
    c.b2 = c.b0;
    r.poles (c);
    return c;
}

Coeffs allpass2BLT (double freq, double q, double fs)
{
    const Rbj r (freq, q, fs);
    Coeffs c;
    c.b0 = (1.0 - r.alpha) / r.a0;
    c.b1 = -2.0 * r.cw / r.a0;
    c.b2 = 1.0;
    r.poles (c);
    return c;
}

Coeffs bandpassBLT (double freq, double q, double fs)
{
    const Rbj r (freq, q, fs);
    Coeffs c;
    c.b0 = r.alpha / r.a0;
    c.b1 = 0.0;
    c.b2 = -r.alpha / r.a0;
    r.poles (c);
    return c;
}

Coeffs highpass1BLT (double freq, double fs)
{
    const double K = std::tan (kPi * clampv (freq, 1.0, 0.49 * fs) / fs);
    Coeffs c;
    c.b0 = 1.0 / (1.0 + K);
    c.b1 = -c.b0;
    c.b2 = 0.0;
    c.a1 = (K - 1.0) / (K + 1.0);
    c.a2 = 0.0;
    return c;
}
} // namespace design
} // namespace kv
