// MIMIC - the full vocal chain.
// in -> HPF -> Tune/Formant -> Low/Mud/Presence EQ -> Comp -> De-ess -> Grit -> Air
//    -> Doubler -> (+ Delay, + Reverb sends, ducked by the vocal) -> Width -> Out -> Amount -> Ceiling
#pragma once

#include "Effects.h"
#include "Presets.h"
#include "Tuner.h"

namespace mimic
{
class Chain
{
public:
    struct Telemetry
    {
        float inDb = -100, outDb = -100, compGr = 0, deessGr = 0, detectedMidi = -1, targetMidi = -1, outputMidi = -1;
    };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        tuner.prepare (fs);
        latency = tuner.getLatency();
        dryL.prepare (latency + 8); dryR.prepare (latency + 8);
        comp.prepare (fs); deess.prepare (fs); sat.prepare (fs);
        dbl.prepare (fs); delay.prepare (fs); verb.prepare (fs);
        duckEnv.prepare (fs, 5.0f, 250.0f);
        for (auto* b : { &hpf1, &hpf2, &low, &mud, &pres, &air }) b->reset();
        amount.prepare (fs, 30.0f, 1.0f);
        delayMix.prepare (fs, 40.0f, 0.0f);
        revMix.prepare (fs, 40.0f, 0.0f);
        width.prepare (fs, 40.0f, 1.0f);
        outGain.prepare (fs, 30.0f, 1.0f);
        haveSettings = false;
        inEnv = outEnv = 0.0f;
        meterCoef = (float) std::exp (-1.0 / (0.3 * fs));
    }

    int getLatency() const noexcept { return latency; }
    void setTempo (double bpmIn) noexcept { if (bpmIn > 20.0 && bpmIn < 400.0) bpm = bpmIn; }

    void setSettings (const ChainSettings& s)
    {
        const bool first = ! haveSettings;
        if (first || differs (s.hpf, cur.hpf))
        {
            hpf1.highpass (fs, s.hpf, 0.54); hpf2.highpass (fs, s.hpf, 1.31); // 4th-order Butterworth
        }
        if (first || differs (s.lowHz, cur.lowHz) || differs (s.lowDb, cur.lowDb))    low.lowShelf (fs, s.lowHz, s.lowDb);
        if (first || differs (s.mudHz, cur.mudHz) || differs (s.mudDb, cur.mudDb))    mud.peak (fs, s.mudHz, 1.1, s.mudDb);
        if (first || differs (s.presHz, cur.presHz) || differs (s.presDb, cur.presDb)) pres.peak (fs, s.presHz, 0.9, s.presDb);
        if (first || differs (s.airHz, cur.airHz) || differs (s.airDb, cur.airDb))    air.highShelf (fs, s.airHz, s.airDb, 0.6);

        Tuner::Params tp;
        tp.mix = s.tuneMix; tp.retuneMs = s.retuneMs; tp.formantSemis = s.formant; tp.key = s.key;
        for (int i = 0; i < 12; ++i) tp.scale[i] = s.scale[i];
        tuner.setParams (tp);

        comp.set (s.compThr, s.compRatio, s.compAtk, s.compRel);
        deess.set (s.deessHz, s.deessDb);
        sat.set (s.driveDb, s.satMix, s.bias);
        dbl.set (s.dblLevel, s.dblMs, s.dblDepth);

        const float beats = delayBeats (s.delayDiv);
        const float seconds = beats <= 0.0f ? 0.09f : (float) (beats * 60.0 / bpm);
        delay.set (seconds, s.delayFb, s.delayHp, s.delayLp, s.pingPong);
        verb.set (s.revDecay, s.revPre, s.revDamp, s.revSize);

        amount.set (s.amount);
        delayMix.set (s.delayMix);
        revMix.set (s.revMix);
        width.set (s.width);
        outGain.set (dbToGain (s.outDb));

        if (first)
        {
            amount.snap (s.amount); delayMix.snap (s.delayMix); revMix.snap (s.revMix);
            width.snap (s.width); outGain.snap (dbToGain (s.outDb));
        }
        cur = s;
        haveSettings = true;
    }

    /** inR may equal inL for mono input. Output buffers may alias the inputs. */
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept
    {
        float compGrMax = 0.0f, deessGrMax = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const float xl = inL[i], xr = inR[i];
            const float mono = 0.5f * (xl + xr);
            inEnv = std::max (std::fabs (mono), inEnv * meterCoef);

            // --- vocal path (mono)
            float v = hpf2.process (hpf1.process (mono));
            v = tuner.process (v);
            v = pres.process (mud.process (low.process (v)));
            v = comp.process (v);
            v = deess.process (v);
            v = sat.process (v);
            v = air.process (v);
            compGrMax = std::max (compGrMax, comp.grDb);
            deessGrMax = std::max (deessGrMax, deess.grDb);

            // --- doubles
            float dl, dr;
            dbl.process (v, dl, dr);
            float l = v + dl, r = v + dr;

            // --- ducked sends
            const float envDb = gainToDb (duckEnv.process (v));
            const float loud = std::clamp ((envDb + 40.0f) / 30.0f, 0.0f, 1.0f);
            float el, er, rl, rr;
            delay.process (v, el, er);
            verb.process (v, rl, rr);
            const float dMix = delayMix.next() * dbToGain (-cur.delayDuck * 12.0f * loud);
            const float rMix = revMix.next() * dbToGain (-cur.revDuck * 12.0f * loud);
            l += el * dMix + rl * rMix;
            r += er * dMix + rr * rMix;

            // --- width (M/S)
            const float w = width.next();
            const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * w;
            const float g = outGain.next();
            l = (mid + side) * g;
            r = (mid - side) * g;

            // --- global amount vs latency-aligned dry
            const float a = amount.next();
            const float dly = dryL.readInt (latency), dry = dryR.readInt (latency);
            dryL.push (xl); dryR.push (xr);
            l = dly + (l - dly) * a;
            r = dry + (r - dry) * a;

            l = ceiling (l); r = ceiling (r);
            outEnv = std::max (std::max (std::fabs (l), std::fabs (r)), outEnv * meterCoef);
            outL[i] = l; outR[i] = r;
        }

        telemetry.inDb = gainToDb (inEnv);
        telemetry.outDb = gainToDb (outEnv);
        telemetry.compGr = compGrMax;
        telemetry.deessGr = deessGrMax;
        telemetry.detectedMidi = tuner.detectedMidi;
        telemetry.targetMidi = tuner.targetMidi;
        telemetry.outputMidi = tuner.outputMidi;
    }

    Telemetry telemetry;

private:
    static inline float ceiling (float x) noexcept
    {
        constexpr float t = 0.89f; // ~ -1 dBFS knee, soft ceiling at 0 dBFS
        const float ax = std::fabs (x);
        if (ax <= t) return x;
        const float y = t + (1.0f - t) * std::tanh ((ax - t) / (1.0f - t));
        return x < 0.0f ? -y : y;
    }

    double fs = 44100.0, bpm = 120.0;
    int latency = 0;
    Tuner tuner;
    DelayLine dryL, dryR;
    Biquad hpf1, hpf2, low, mud, pres, air;
    Compressor comp;
    DeEsser deess;
    Saturator sat;
    Doubler dbl;
    TempoDelay delay;
    Reverb verb;
    EnvFollower duckEnv;
    Smoothed amount, delayMix, revMix, width, outGain;
    ChainSettings cur {};
    bool haveSettings = false;
    float inEnv = 0.0f, outEnv = 0.0f, meterCoef = 0.99f;
};
} // namespace mimic
