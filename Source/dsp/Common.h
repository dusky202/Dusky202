// MIMIC - Artist vocal chain
// Shared helpers. Header-only, no JUCE dependency so the DSP can be tested standalone.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace mimic
{
constexpr double kPiD = 3.14159265358979323846;
constexpr float  kPi  = 3.14159265358979323846f;

inline float dbToGain (float db) noexcept   { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept    { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }
inline float clamp01 (float x) noexcept     { return std::clamp (x, 0.0f, 1.0f); }
/** Exact change detection for cached coefficients (avoids -Wfloat-equal noise). */
inline bool differs (float a, float b) noexcept { return std::memcmp (&a, &b, sizeof (float)) != 0; }

inline int nextPow2 (int v) noexcept
{
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}

/** One-pole parameter smoother (per-sample). */
struct Smoothed
{
    float current = 0.0f, target = 0.0f, coef = 0.0f;

    void prepare (double fs, float timeMs, float initial)
    {
        coef = (float) std::exp (-1.0 / (std::max (0.1, (double) timeMs) * 0.001 * fs));
        current = target = initial;
    }
    void set (float t) noexcept           { target = t; }
    void snap (float t) noexcept          { current = target = t; }
    inline float next() noexcept          { current = target + (current - target) * coef; return current; }
};

/** Mono ring buffer with fractional (cubic Hermite) reads. */
struct Ring
{
    std::vector<float> data;
    int mask = 0;

    void prepare (int minSize)
    {
        const int n = nextPow2 (std::max (minSize, 16));
        data.assign ((size_t) n, 0.0f);
        mask = n - 1;
    }
    void clear()                                        { std::fill (data.begin(), data.end(), 0.0f); }
    inline float& at (int64_t i) noexcept               { return data[(size_t) (i & mask)]; }
    inline float  get (int64_t i) const noexcept        { return data[(size_t) (i & mask)]; }

    /** Cubic Hermite read at absolute (fractional) sample position. */
    inline float read (double pos) const noexcept
    {
        const double fl = std::floor (pos);
        const auto i = (int64_t) fl;
        const float t = (float) (pos - fl);
        const float xm1 = get (i - 1), x0 = get (i), x1 = get (i + 1), x2 = get (i + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }
};

/** Simple delay line addressed relative to the write head. */
struct DelayLine
{
    Ring ring;
    int64_t w = 0;

    void prepare (int maxSamples)          { ring.prepare (maxSamples + 4); w = 0; }
    void clear()                           { ring.clear(); }
    inline void push (float x) noexcept    { ring.at (w++) = x; }
    /** delay in samples (>= 1). Call before push for the classic read-then-write order. */
    inline float read (double delay) const noexcept { return ring.read ((double) w - delay); }
    inline float readInt (int delay) const noexcept { return ring.get (w - delay); }
};

/** Attack/release envelope follower working on |x|. */
struct EnvFollower
{
    float env = 0.0f, a = 0.0f, r = 0.0f;
    void prepare (double fs, float attackMs, float releaseMs)
    {
        a = (float) std::exp (-1.0 / (attackMs * 0.001 * fs));
        r = (float) std::exp (-1.0 / (releaseMs * 0.001 * fs));
        env = 0.0f;
    }
    inline float process (float x) noexcept
    {
        const float v = std::fabs (x);
        env = v > env ? v + (env - v) * a : v + (env - v) * r;
        return env;
    }
};

/** Smooth random LFO in [-1, 1] (raised-cosine interpolation between random points). */
struct DriftLFO
{
    float from = 0.0f, to = 0.0f, phase = 0.0f, inc = 0.0f;
    uint32_t seed = 12345u;

    void prepare (double fs, float rateHz, uint32_t s)
    {
        seed = s;
        inc = (float) (rateHz / fs);
        from = rnd(); to = rnd(); phase = 0.0f;
    }
    inline float rnd() noexcept
    {
        seed = seed * 1664525u + 1013904223u;
        return ((float) (seed >> 8) / 8388608.0f) - 1.0f;
    }
    inline float next() noexcept
    {
        phase += inc;
        if (phase >= 1.0f) { phase -= 1.0f; from = to; to = rnd(); }
        const float s = 0.5f - 0.5f * std::cos (kPi * phase);
        return from + (to - from) * s;
    }
};
} // namespace mimic
