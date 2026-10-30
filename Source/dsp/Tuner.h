// MIMIC - pitch tracking (YIN on a decimated signal) + TD-PSOLA pitch correction
// with independent formant shift. Fixed latency, reported to the host.
#pragma once

#include "Filters.h"

namespace mimic
{
//==============================================================================
class PitchTracker
{
public:
    struct Estimate
    {
        int64_t time = 0;      // full-rate sample index of the analysis centre
        float period = 0.0f;   // in full-rate samples
        bool voiced = false;
    };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        D = fs > 64000.0 ? 4 : 2;
        fsd = fs / D;
        maxLag = (int) std::ceil (fsd / kMinHz);
        minLag = std::max (2, (int) std::floor (fsd / kMaxHz));
        W = std::max (maxLag + 8, (int) (fsd * 0.016));
        N = W + maxLag + 2;
        hop = std::max (32, (int) (fsd * 0.004));

        lp1.lowpass (fs, 1800.0); lp2.lowpass (fs, 1800.0);
        lp1.reset(); lp2.reset();

        ring.prepare (N * 2 + 16);
        frame.assign ((size_t) N, 0.0f);
        diff.assign ((size_t) maxLag + 2, 0.0f);
        decCount = 0; decWrite = 0; sinceHop = 0; fullIndex = 0;
        hist.assign (kHist, Estimate{});
        histHead = 0; histCount = 0;
        lastPeriodDec = 0.0f;
    }

    /** Max period (full rate) the tracker can report. */
    int maxPeriod() const noexcept       { return maxLag * D + 2; }
    /** How far behind "now" the newest estimate centre sits (full-rate samples, approx). */
    int analysisDelay() const noexcept   { return (N - (W + maxLag) / 2) * D + hop * D; }

    inline void push (float x) noexcept
    {
        const float y = lp2.process (lp1.process (x));
        ++fullIndex;
        if (++decCount < D) return;
        decCount = 0;
        ring.at (decWrite++) = y;
        if (++sinceHop >= hop && decWrite >= N)
        {
            sinceHop = 0;
            analyse();
        }
    }

    /** Estimate closest to (at or before) time t, linearly interpolated with the next newer one. */
    Estimate lookup (int64_t t) const noexcept
    {
        if (histCount == 0) return {};
        // newest first
        for (int k = 0; k < histCount; ++k)
        {
            const Estimate& e = hist[(size_t) ((histHead - 1 - k + kHist) % kHist)];
            if (e.time <= t)
            {
                if (k > 0)
                {
                    const Estimate& n = hist[(size_t) ((histHead - k + kHist) % kHist)];
                    if (e.voiced && n.voiced && n.time > e.time
                        && std::fabs (n.period / e.period - 1.0f) < 0.06f)
                    {
                        Estimate r = e;
                        const float a = (float) (t - e.time) / (float) (n.time - e.time);
                        r.period = e.period + (n.period - e.period) * a;
                        return r;
                    }
                }
                return e;
            }
        }
        return hist[(size_t) ((histHead - histCount + kHist) % kHist)]; // oldest
    }

    Estimate latest() const noexcept
    {
        if (histCount == 0) return {};
        return hist[(size_t) ((histHead - 1 + kHist) % kHist)];
    }

private:
    static constexpr double kMinHz = 75.0, kMaxHz = 1100.0;
    static constexpr int kHist = 256;

    void analyse() noexcept
    {
        // copy last N decimated samples into a linear frame
        const int64_t start = decWrite - N;
        double energy = 0.0;
        for (int i = 0; i < N; ++i)
        {
            const float v = ring.get (start + i);
            frame[(size_t) i] = v;
            energy += (double) v * v;
        }
        const float rms = (float) std::sqrt (energy / N);

        Estimate est;
        est.voiced = false;
        est.period = 0.0f;

        if (rms > 0.0025f)
        {
            // YIN difference function
            const float* x = frame.data();
            for (int tau = 1; tau <= maxLag; ++tau)
            {
                float s = 0.0f;
                const float* y = x + tau;
                for (int j = 0; j < W; ++j)
                {
                    const float d = x[j] - y[j];
                    s += d * d;
                }
                diff[(size_t) tau] = s;
            }
            // cumulative mean normalised difference
            diff[0] = 1.0f;
            float running = 0.0f;
            for (int tau = 1; tau <= maxLag; ++tau)
            {
                running += diff[(size_t) tau];
                diff[(size_t) tau] = running > 0.0f ? diff[(size_t) tau] * (float) tau / running : 1.0f;
            }

            int best = -1;
            for (int tau = minLag; tau <= maxLag; ++tau)
            {
                if (diff[(size_t) tau] < 0.15f)
                {
                    while (tau + 1 <= maxLag && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                    best = tau;
                    break;
                }
            }
            if (best < 0)
            {
                float m = 1.0e9f; int mi = -1;
                for (int tau = minLag; tau <= maxLag; ++tau)
                    if (diff[(size_t) tau] < m) { m = diff[(size_t) tau]; mi = tau; }
                if (mi > 0 && m < 0.3f) best = mi;
            }

            if (best > 0)
            {
                float t = (float) best;
                if (best > minLag && best < maxLag)
                {
                    const float a = diff[(size_t) best - 1], b = diff[(size_t) best], c = diff[(size_t) best + 1];
                    const float den = a - 2.0f * b + c;
                    if (std::fabs (den) > 1.0e-9f) t += std::clamp (0.5f * (a - c) / den, -0.5f, 0.5f);
                }
                // light smoothing when the estimate is stable
                if (lastPeriodDec > 0.0f && std::fabs (t / lastPeriodDec - 1.0f) < 0.05f)
                    t = 0.6f * t + 0.4f * lastPeriodDec;
                lastPeriodDec = t;
                est.voiced = true;
                est.period = t * (float) D;
            }
            else lastPeriodDec = 0.0f;
        }
        else lastPeriodDec = 0.0f;

        // centre of the analysed region in full-rate samples
        const float tauDec = est.voiced ? est.period / (float) D : (float) maxLag * 0.5f;
        est.time = fullIndex - (int64_t) ((N - (W + tauDec) * 0.5f) * (float) D);

        hist[(size_t) histHead] = est;
        histHead = (histHead + 1) % kHist;
        histCount = std::min (histCount + 1, kHist);
    }

    double fs = 44100.0, fsd = 22050.0;
    int D = 2, maxLag = 300, minLag = 20, W = 400, N = 700, hop = 100;
    Biquad lp1, lp2;
    Ring ring;
    std::vector<float> frame, diff;
    int decCount = 0, sinceHop = 0;
    int64_t decWrite = 0, fullIndex = 0;
    std::vector<Estimate> hist;
    int histHead = 0, histCount = 0;
    float lastPeriodDec = 0.0f;
};

//==============================================================================
/** Real-time TD-PSOLA. Pitch marks advance by the tracked period, grains are re-spaced
    for pitch correction and time-scaled inside for formant shift. */
class Tuner
{
public:
    struct Params
    {
        float mix = 0.0f;         // 0..1 amount of correction applied
        float retuneMs = 20.0f;   // 0 = hard tune
        float formantSemis = 0.0f;
        int key = 0;              // 0 = C
        bool scale[12] = { true, true, true, true, true, true, true, true, true, true, true, true };
    };

    static constexpr float kMaxFormantSemis = 5.0f;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        tracker.prepare (fs);
        Pmax = (double) tracker.maxPeriod();
        const double fMin = std::pow (2.0, -kMaxFormantSemis / 12.0);
        HmaxOut = Pmax / fMin + 2.0;
        L = (int) std::ceil (std::max (HmaxOut + 1.5 * Pmax + 8.0, (double) tracker.analysisDelay() * 0.85));
        Punv = fs * 0.0055;

        const int ringSize = (int) (L + HmaxOut * 2 + Pmax * 4 + 4096);
        inBuf.prepare (ringSize);
        outBuf.prepare (ringSize);
        n = 0;
        nextSynth = (double) L;
        anaMark = 0.0;
        corr = 0.0f;
        lastNote = -1000;
        formant = 1.0f;
        blend.prepare (fs, 15.0f, 0.0f);
        detectedMidi = -1.0f; targetMidi = -1.0f;
    }

    int getLatency() const noexcept { return L; }
    void setParams (const Params& p) noexcept { params = p; }

    inline float process (float x) noexcept
    {
        inBuf.at (n) = x;
        tracker.push (x);

        while (nextSynth <= (double) n + HmaxOut)
            placeGrain();

        float& o = outBuf.at (n);
        const float shifted = o;
        o = 0.0f;

        const bool active = params.mix > 0.001f || std::fabs (params.formantSemis) > 0.01f;
        blend.set (active ? 1.0f : 0.0f);
        const float b = blend.next();
        const float dry = inBuf.get (n - L);
        ++n;
        return dry + (shifted - dry) * b;
    }

    // Telemetry (audio thread writes, read via atomics by the wrapper)
    float detectedMidi = -1.0f, targetMidi = -1.0f, outputMidi = -1.0f;

private:
    bool allowed (int note) const noexcept
    {
        const int pc = ((note - params.key) % 12 + 12) % 12;
        return params.scale[pc];
    }

    int snap (float midi) noexcept
    {
        int best = (int) std::lround (midi);
        float bestD = 1.0e9f;
        for (int d = -7; d <= 7; ++d)
        {
            const int c = (int) std::lround (midi) + d;
            if (! allowed (c)) continue;
            const float dist = std::fabs ((float) c - midi);
            if (dist < bestD) { bestD = dist; best = c; }
        }
        // hysteresis: stay on the previous note unless clearly closer to another
        if (lastNote > -1000 && allowed (lastNote) && std::fabs ((float) lastNote - midi) < bestD + 0.18f)
            best = lastNote;
        lastNote = best;
        return best;
    }

    void placeGrain() noexcept
    {
        const double t = nextSynth;
        const double aTarget = t - (double) L;
        const auto est = tracker.lookup ((int64_t) aTarget);

        // formant (smoothed per grain)
        const float fTarget = std::pow (2.0f, std::clamp (params.formantSemis, -kMaxFormantSemis, kMaxFormantSemis) / 12.0f);
        formant += (fTarget - formant) * 0.15f;
        const double f = formant;

        double P, spacing;
        if (est.voiced && est.period > 8.0f)
        {
            P = std::min ((double) est.period, Pmax);
            const float midi = 69.0f + 12.0f * std::log2 ((float) (fs / P) / 440.0f);
            const int note = snap (midi);
            const float target = (float) note - midi;
            const float tauS = std::max (params.retuneMs, 0.0f) * 0.001f * (float) fs;
            const float a = tauS < 1.0f ? 1.0f : 1.0f - std::exp (-(float) P / tauS);
            corr += (target - corr) * a;
            const float ratio = std::pow (2.0f, corr * params.mix / 12.0f);
            spacing = P / ratio;
            detectedMidi = midi;
            targetMidi = params.mix > 0.001f ? (float) note : midi;
            outputMidi = midi + corr * params.mix; // pitch actually produced
        }
        else
        {
            P = Punv;
            corr *= 0.5f;
            lastNote = -1000;
            spacing = P / f; // constant-overlap for noise
            detectedMidi = -1.0f; targetMidi = -1.0f; outputMidi = -1.0f;
        }

        if (anaMark < aTarget - 2.0 * P || anaMark > aTarget + 2.0 * P)
            anaMark = aTarget;
        while (anaMark + 0.5 * P < aTarget)
            anaMark += P;

        const double Hout = P / f;
        const auto k0 = (int64_t) std::ceil (t - Hout);
        const auto k1 = (int64_t) std::floor (t + Hout);
        const double invH = 1.0 / Hout;
        const float levelComp = std::sqrt ((float) f); // keep loudness steady when grains shrink/grow
        for (int64_t k = k0; k <= k1; ++k)
        {
            const double u = ((double) k - t) * invH;
            const float w = (0.5f + 0.5f * (float) std::cos (kPiD * u)) * levelComp;
            const double src = anaMark + ((double) k - t) * f;
            outBuf.at (k) += w * inBuf.read (src);
        }
        nextSynth += std::max (spacing, 8.0);
    }

    double fs = 44100.0, Pmax = 600.0, HmaxOut = 800.0, Punv = 250.0;
    int L = 1700;
    PitchTracker tracker;
    Ring inBuf, outBuf;
    int64_t n = 0;
    double nextSynth = 0.0, anaMark = 0.0;
    float corr = 0.0f, formant = 1.0f;
    int lastNote = -1000;
    Smoothed blend;
    Params params;
};
} // namespace mimic
