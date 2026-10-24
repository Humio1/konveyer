#pragma once
// Small iterative radix-2 complex FFT (no allocation after setSize).

#include "Common.h"
#include <complex>

namespace kv
{
class Fft
{
public:
    void setSize (int n)
    {
        size = n;
        twiddle.resize ((size_t) n / 2);
        for (int k = 0; k < n / 2; ++k)
            twiddle[(size_t) k] = std::polar (1.0, -kTwoPi * k / n);
        rev.resize ((size_t) n);
        int bits = 0;
        while ((1 << bits) < n)
            ++bits;
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                if (i & (1 << b))
                    r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }
    }

    int getSize() const { return size; }

    /** In-place transform. inverse = true computes the unscaled inverse. */
    void transform (std::complex<double>* x, bool inverse) const noexcept
    {
        for (int i = 0; i < size; ++i)
            if (i < rev[(size_t) i])
                std::swap (x[i], x[rev[(size_t) i]]);
        for (int len = 2; len <= size; len <<= 1)
        {
            const int half = len / 2, step = size / len;
            for (int i = 0; i < size; i += len)
                for (int j = 0; j < half; ++j)
                {
                    auto w = twiddle[(size_t) (j * step)];
                    if (inverse)
                        w = std::conj (w);
                    const auto t = w * x[i + j + half];
                    x[i + j + half] = x[i + j] - t;
                    x[i + j] += t;
                }
        }
    }

private:
    int size = 0;
    std::vector<std::complex<double>> twiddle;
    std::vector<int> rev;
};
} // namespace kv
