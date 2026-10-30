// MIMIC - RBJ biquads (transposed direct form II) and one-pole helpers.
#pragma once

#include "Common.h"

namespace mimic
{
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void reset() noexcept { z1 = z2 = 0; }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setRaw (double B0, double B1, double B2, double A0, double A1, double A2) noexcept
    {
        const double inv = 1.0 / A0;
        b0 = (float) (B0 * inv); b1 = (float) (B1 * inv); b2 = (float) (B2 * inv);
        a1 = (float) (A1 * inv); a2 = (float) (A2 * inv);
    }

    static double clampFreq (double fs, double f) { return std::clamp (f, 10.0, fs * 0.49); }

    void lowpass (double fs, double f, double q = 0.7071)
    {
        const double w = 2.0 * kPiD * clampFreq (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        setRaw ((1 - c) * 0.5, 1 - c, (1 - c) * 0.5, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double fs, double f, double q = 0.7071)
    {
        const double w = 2.0 * kPiD * clampFreq (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        setRaw ((1 + c) * 0.5, -(1 + c), (1 + c) * 0.5, 1 + al, -2 * c, 1 - al);
    }
    void peak (double fs, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0);
        const double w = 2.0 * kPiD * clampFreq (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        setRaw (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (double fs, double f, double db, double q = 0.7071)
    {
        const double A = std::pow (10.0, db / 40.0);
        const double w = 2.0 * kPiD * clampFreq (fs, f) / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / (2.0 * q), sq = 2.0 * std::sqrt (A) * al;
        setRaw (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
                (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    void highShelf (double fs, double f, double db, double q = 0.7071)
    {
        const double A = std::pow (10.0, db / 40.0);
        const double w = 2.0 * kPiD * clampFreq (fs, f) / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / (2.0 * q), sq = 2.0 * std::sqrt (A) * al;
        setRaw (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
                (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
};

/** One-pole lowpass / highpass (6 dB/oct), used inside feedback loops. */
struct OnePole
{
    float a = 1.0f, z = 0.0f;
    void setLowpass (double fs, double f)  { a = (float) (1.0 - std::exp (-2.0 * kPiD * std::clamp (f, 10.0, fs * 0.49) / fs)); }
    void reset() noexcept                  { z = 0.0f; }
    inline float lp (float x) noexcept     { z += a * (x - z); return z; }
    inline float hp (float x) noexcept     { z += a * (x - z); return x - z; }
};
} // namespace mimic
