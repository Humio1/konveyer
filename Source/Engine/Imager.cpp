#include "Imager.h"

namespace kv
{
void Imager::prepare (double sampleRate)
{
    fs = sampleRate;
    smooth = 1.0 - std::exp (-kSub / (0.03 * fs));
    corrCoeff = onePoleCoeff (0.3, fs);
    reset();
}

void Imager::reset()
{
    splitM.reset();
    splitS.reset();
    primed = false;
    updateCrossovers (true);
    for (size_t k = 0; k < 3; ++k)
        width[k].reset ((k == 0 && params.monoBass) ? 0.0 : clampv (params.width[k], 0.0, 2.0));
    sLR = sLL = sRR = 0.0;
    correlation.store (1.0f);
}

void Imager::updateCrossovers (bool snap)
{
    const double f0 = clampv (params.xover[0], 20.0, 0.30 * fs);
    const double f1 = clampv (params.xover[1], f0 * 1.3, 0.45 * fs);
    const double target[2] = { std::log (f0), std::log (f1) };
    for (size_t k = 0; k < 2; ++k)
    {
        const double before = xLog[k];
        if (snap || ! primed)
            xLog[k] = target[k];
        else
        {
            xLog[k] += (target[k] - xLog[k]) * smooth;
            if (std::abs (target[k] - xLog[k]) < 1.0e-5)
                xLog[k] = target[k];
        }
        if (snap || ! primed || std::abs (xLog[k] - before) > 0.0)
            xc[k].design (std::exp (xLog[k]), fs);
    }
    primed = true;
}

void Imager::process (double* L, double* R, int n)
{
    const int ramp = (int) (0.03 * fs);
    for (size_t k = 0; k < 3; ++k)
        width[k].setTarget ((k == 0 && params.monoBass) ? 0.0 : clampv (params.width[k], 0.0, 2.0), ramp);

    const double a = corrCoeff;
    for (int start = 0; start < n; start += kSub)
    {
        const int len = std::min (kSub, n - start);
        updateCrossovers (false);
        for (int i = start; i < start + len; ++i)
        {
            const double m = 0.5 * (L[i] + R[i]);
            const double s = 0.5 * (L[i] - R[i]);
            double mb[3], sb[3];
            splitM.split (xc, m, mb);
            splitS.split (xc, s, sb);
            const double mo = mb[0] + mb[1] + mb[2];
            const double so = sb[0] * width[0].next() + sb[1] * width[1].next() + sb[2] * width[2].next();
            const double l = mo + so, r = mo - so;
            L[i] = l;
            R[i] = r;
            sLR = a * sLR + (1.0 - a) * l * r;
            sLL = a * sLL + (1.0 - a) * l * l;
            sRR = a * sRR + (1.0 - a) * r * r;
        }
    }

    const double denom = std::sqrt (sLL * sRR);
    correlation.store (denom > 1.0e-12 ? (float) clampv (sLR / denom, -1.0, 1.0) : 1.0f, std::memory_order_relaxed);
}
} // namespace kv
