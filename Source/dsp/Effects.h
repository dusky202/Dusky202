// MIMIC - vocal chain building blocks: compressor, de-esser, saturator, doubler,
// tempo delay and FDN reverb. All zero-latency.
#pragma once

#include "Filters.h"

namespace mimic
{
//==============================================================================
/** Feed-forward compressor, log-domain smoothing (Giannoulis/Massberg/Reiss), soft knee. */
class Compressor
{
public:
    void prepare (double sampleRate) { fs = sampleRate; yL = 0.0f; set (-20.0f, 4.0f, 5.0f, 60.0f); }

    void set (float thresholdDb, float ratioIn, float attackMs, float releaseMs)
    {
        thr = thresholdDb; ratio = std::max (1.0f, ratioIn);
        aA = (float) std::exp (-1.0 / (std::max (0.05f, attackMs) * 0.001 * fs));
        aR = (float) std::exp (-1.0 / (std::max (1.0f, releaseMs) * 0.001 * fs));
        makeup = -thr * (1.0f - 1.0f / ratio) * 0.4f;
    }

    inline float process (float x) noexcept
    {
        const float xg = gainToDb (std::fabs (x));
        float yg;
        const float over = xg - thr;
        if (2.0f * over < -knee)                yg = xg;
        else if (2.0f * std::fabs (over) <= knee) yg = xg + (1.0f / ratio - 1.0f) * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
        else                                    yg = thr + over / ratio;
        const float xl = xg - yg; // >= 0
        yL = xl > yL ? aA * yL + (1.0f - aA) * xl : aR * yL + (1.0f - aR) * xl;
        grDb = yL;
        return x * dbToGain (makeup - yL);
    }

    float grDb = 0.0f;

private:
    double fs = 44100.0;
    float thr = -20.0f, ratio = 4.0f, aA = 0.0f, aR = 0.0f, makeup = 0.0f, knee = 6.0f, yL = 0.0f;
};

//==============================================================================
/** Split-band de-esser: reduces only the HF band when it dominates the full band. */
class DeEsser
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        envHi.prepare (fs, 0.5f, 60.0f);
        envAll.prepare (fs, 0.5f, 60.0f);
        grSm.prepare (fs, 2.0f, 0.0f);
        set (6500.0f, 6.0f);
        hp.reset();
    }

    void set (float freqHz, float maxReductionDb)
    {
        if (differs (freqHz, freq)) { hp.highpass (fs, freqHz, 0.6); freq = freqHz; }
        maxDb = std::max (0.0f, maxReductionDb);
    }

    inline float process (float x) noexcept
    {
        const float hi = hp.process (x);
        const float hDb = gainToDb (envHi.process (hi));
        const float aDb = gainToDb (envAll.process (x));
        float gr = 0.0f;
        if (aDb > -55.0f)
            gr = std::clamp ((hDb - (aDb - 7.0f)) * 0.8f, 0.0f, maxDb);
        grSm.set (gr);
        grDb = grSm.next();
        const float g = dbToGain (-grDb);
        return x + (g - 1.0f) * hi;
    }

    float grDb = 0.0f;

private:
    double fs = 44100.0;
    float freq = -1.0f, maxDb = 6.0f;
    Biquad hp;
    EnvFollower envHi, envAll;
    Smoothed grSm;
};

//==============================================================================
/** Anti-aliased (1st-order ADAA) tanh saturation with bias (even harmonics),
    DC blocking and automatic loudness matching so Grit changes tone, not level. */
class Saturator
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        uPrev = 0.0f;
        dc.setLowpass (fs, 8.0); dc.reset();
        msIn = msOut = 1.0e-6f;
        rmsCoef = (float) std::exp (-1.0 / (0.25 * fs));
        drive.prepare (fs, 30.0f, 1.0f);
        mix.prepare (fs, 30.0f, 0.0f);
        comp = 1.0f;
    }

    void set (float driveDb, float mixAmount, float biasIn)
    {
        drive.set (dbToGain (driveDb));
        mix.set (clamp01 (mixAmount));
        bias = biasIn;
    }

    inline float process (float x) noexcept
    {
        const float d = drive.next();
        const float m = mix.next();
        const float u = d * x + bias;
        float y;
        const float du = u - uPrev;
        if (std::fabs (du) < 1.0e-4f) y = std::tanh (0.5f * (u + uPrev));
        else                          y = (F1 (u) - F1 (uPrev)) / du;
        uPrev = u;
        y = dc.hp (y);

        msIn  = rmsCoef * msIn  + (1.0f - rmsCoef) * x * x;
        msOut = rmsCoef * msOut + (1.0f - rmsCoef) * y * y;
        const float target = std::clamp (std::sqrt ((msIn + 1.0e-9f) / (msOut + 1.0e-9f)), 0.05f, 2.0f);
        comp += (target - comp) * 0.0005f;

        return x + (y * comp - x) * m;
    }

private:
    static inline float F1 (float x) noexcept
    {
        // log(cosh(x)), numerically stable
        const float ax = std::fabs (x);
        return ax + std::log1p (std::exp (-2.0f * ax)) - 0.69314718f;
    }

    double fs = 44100.0;
    float uPrev = 0.0f, bias = 0.0f, msIn = 0.0f, msOut = 0.0f, rmsCoef = 0.0f, comp = 1.0f;
    OnePole dc;
    Smoothed drive, mix;
};

//==============================================================================
/** Two modulated delay voices panned L/R: classic ADT-style double. */
class Doubler
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        lineL.prepare ((int) (fs * 0.08)); lineR.prepare ((int) (fs * 0.08));
        lfoL.prepare (fs, 0.9f, 0x1234u); lfoR.prepare (fs, 0.7f, 0x9876u);
        hpL.highpass (fs, 160.0); hpR.highpass (fs, 160.0);
        lpL.lowpass (fs, 9000.0); lpR.lowpass (fs, 9000.0);
        level.prepare (fs, 40.0f, 0.0f);
        set (0.0f, 20.0f, 1.0f);
    }

    void set (float lvl, float baseMs, float depthMs)
    {
        level.set (lvl);
        base = std::clamp (baseMs, 5.0f, 40.0f) * 0.001f * (float) fs;
        depth = std::clamp (depthMs, 0.0f, 4.0f) * 0.001f * (float) fs;
    }

    inline void process (float x, float& l, float& r) noexcept
    {
        const float g = level.next();
        const float dl = base + depth * lfoL.next();
        const float dr = base * 1.37f + depth * lfoR.next();
        const float yl = lineL.read (std::max (2.0f, dl));
        const float yr = lineR.read (std::max (2.0f, dr));
        lineL.push (x); lineR.push (x);
        l = lpL.process (hpL.process (yl)) * g;
        r = lpR.process (hpR.process (yr)) * g;
    }

private:
    double fs = 44100.0;
    DelayLine lineL, lineR;
    DriftLFO lfoL, lfoR;
    Biquad hpL, hpR, lpL, lpR;
    Smoothed level;
    float base = 800.0f, depth = 40.0f;
};

//==============================================================================
/** Stereo tempo delay with filtered feedback and optional ping-pong. */
class TempoDelay
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        lineL.prepare ((int) (fs * 3.0)); lineR.prepare ((int) (fs * 3.0));
        timeS.prepare (fs, 120.0f, (float) (fs * 0.25));
        hpL.reset(); hpR.reset(); lpL.reset(); lpR.reset();
        set (0.25f, 0.3f, 300.0f, 6000.0f, false);
    }

    void set (float seconds, float feedbackIn, float hpHz, float lpHz, bool pingPongIn)
    {
        timeS.set (std::clamp (seconds, 0.01f, 2.5f) * (float) fs);
        fb = std::clamp (feedbackIn, 0.0f, 0.92f);
        hpL.setLowpass (fs, hpHz); hpR.setLowpass (fs, hpHz);
        lpL.setLowpass (fs, lpHz); lpR.setLowpass (fs, lpHz);
        pingPong = pingPongIn;
    }

    inline void process (float x, float& l, float& r) noexcept
    {
        const float t = timeS.next();
        float dl = lineL.read (t);
        float dr = lineR.read (pingPong ? t : t * 1.013f);
        dl = lpL.lp (hpL.hp (dl));
        dr = lpR.lp (hpR.hp (dr));
        if (pingPong) { lineL.push (x + fb * dr); lineR.push (fb * dl); }
        else          { lineL.push (x + fb * dl); lineR.push (x + fb * dr); }
        l = dl; r = dr;
    }

private:
    double fs = 44100.0;
    DelayLine lineL, lineR;
    Smoothed timeS;
    OnePole hpL, hpR, lpL, lpR;
    float fb = 0.3f;
    bool pingPong = false;
};

//==============================================================================
/** 8-line feedback delay network reverb with input diffusion, damping and modulation. */
class Reverb
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        pre.prepare ((int) (fs * 0.25));
        for (int i = 0; i < 4; ++i) ap[i].prepare ((int) (fs * 0.02));
        for (int i = 0; i < 8; ++i) { lines[i].prepare ((int) (fs * 0.2)); damp[i].reset(); }
        inHp.highpass (fs, 220.0); inHp.reset();
        modL.prepare (fs, 0.37f, 0xabcdu); modR.prepare (fs, 0.29f, 0x4321u);
        configured = false;
        set (2.0f, 20.0f, 7000.0f, 0.7f);
    }

    void set (float decaySeconds, float predelayMs, float dampHz, float size)
    {
        if (configured && ! differs (decaySeconds, cDecay) && ! differs (predelayMs, cPre) && ! differs (dampHz, cDamp) && ! differs (size, cSize))
            return;
        configured = true; cDecay = decaySeconds; cPre = predelayMs; cDamp = dampHz; cSize = size;

        static const float baseMs[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.3f, 73.1f };
        const float scale = 0.45f + std::clamp (size, 0.0f, 1.0f) * 1.1f;
        const float t60 = std::clamp (decaySeconds, 0.2f, 12.0f);
        for (int i = 0; i < 8; ++i)
        {
            len[i] = baseMs[i] * scale * 0.001f * (float) fs;
            gain[i] = std::pow (10.0f, -3.0f * len[i] / (t60 * (float) fs));
            damp[i].setLowpass (fs, dampHz);
        }
        static const float apMs[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
        for (int i = 0; i < 4; ++i) apLen[i] = std::max (8, (int) (apMs[i] * scale * 0.001f * (float) fs));
        preS = std::max (1.0f, std::clamp (predelayMs, 0.0f, 200.0f) * 0.001f * (float) fs);
        modDepth = 0.0012f * (float) fs * 0.25f;
    }

    inline void process (float x, float& l, float& r) noexcept
    {
        float in = pre.read (preS);
        pre.push (inHp.process (x));

        // input diffusion (series allpasses)
        for (int i = 0; i < 4; ++i)
        {
            const float d = ap[i].readInt (apLen[i]);
            const float v = in + 0.6f * d;
            ap[i].push (v);
            in = d - 0.6f * v;
        }

        float d[8];
        const float mL = modL.next() * modDepth, mR = modR.next() * modDepth;
        for (int i = 0; i < 8; ++i)
        {
            float delay = len[i];
            if (i == 1) delay += mL;
            if (i == 6) delay += mR;
            d[i] = lines[i].read (delay);
        }

        float v[8], sum = 0.0f;
        for (int i = 0; i < 8; ++i) { v[i] = damp[i].lp (d[i]) * gain[i]; sum += v[i]; }
        const float hh = sum * 0.25f; // Householder: v - 2/N * sum
        static const float sgn[8] = { 1, -1, 1, -1, 1, -1, 1, -1 };
        for (int i = 0; i < 8; ++i)
            lines[i].push (v[i] - hh + in * sgn[i] * 0.5f);

        l = (d[0] - d[2] + d[4] - d[6]) * 0.45f;
        r = (d[1] - d[3] + d[5] - d[7]) * 0.45f;
    }

private:
    double fs = 44100.0;
    DelayLine pre, ap[4], lines[8];
    OnePole damp[8];
    Biquad inHp;
    DriftLFO modL, modR;
    float len[8] {}, gain[8] {}, preS = 1.0f, modDepth = 0.0f;
    int apLen[4] { 100, 100, 100, 100 };
    bool configured = false;
    float cDecay = 0, cPre = 0, cDamp = 0, cSize = 0;
};
} // namespace mimic
