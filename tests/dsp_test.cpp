// Offline test for the MIMIC DSP chain (no JUCE needed):
//   g++ -O2 -std=c++17 -I../Source tests/dsp_test.cpp -o dsp_test && ./dsp_test
// Synthesises an off-pitch "vocal" (harmonic source through formants + sibilant noise),
// runs every artist preset and checks stability, level, latency and pitch correction.
#include "dsp/Chain.h"
#include <chrono>
#include <cstdio>
#include <random>
#include <xmmintrin.h>

using namespace mimic;

static std::vector<float> makeVocal (double fs, double seconds, double f0Base, double detuneSemis)
{
    const int n = (int) (fs * seconds);
    std::vector<float> src ((size_t) n, 0.0f);
    std::mt19937 rng (7);
    std::normal_distribution<float> nd (0.0f, 1.0f);
    double phase = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs;
        // slow 5 Hz vibrato (+-20 cents) and a fixed detune from the semitone grid
        const double semis = detuneSemis + 0.2 * std::sin (2.0 * kPiD * 5.0 * t);
        const double f0 = f0Base * std::pow (2.0, semis / 12.0);
        phase += f0 / fs;
        phase -= std::floor (phase);
        float s = 0.0f;
        for (int h = 1; h * f0 < fs * 0.45 && h <= 40; ++h)
            s += (float) (std::sin (2.0 * kPiD * h * phase) / std::pow (h, 1.1));
        // phrase envelope: sung notes with gaps, plus sibilant bursts in the gaps
        const double ph = std::fmod (t, 1.0);
        const float env = ph < 0.75 ? (float) std::min (1.0, std::min (ph * 30.0, (0.75 - ph) * 30.0)) : 0.0f;
        const float sib = (ph > 0.8 && ph < 0.9) ? 0.15f * nd (rng) : 0.0f;
        src[(size_t) i] = 0.18f * s * env + sib;
    }
    // formants
    Biquad f1, f2, f3, hs;
    f1.peak (fs, 700, 3, 12); f2.peak (fs, 1200, 4, 9); f3.peak (fs, 2600, 5, 8); hs.highpass (fs, 9000);
    for (auto& x : src) x = 0.35f * f3.process (f2.process (f1.process (x)));
    return src;
}

// independent pitch estimate (plain YIN, full rate) at a given sample position
static float measureF0 (const std::vector<float>& x, double fs, int centre)
{
    const int W = (int) (fs * 0.03), maxLag = (int) (fs / 70.0), minLag = (int) (fs / 1000.0);
    const int start = centre - (W + maxLag) / 2;
    if (start < 0 || start + W + maxLag >= (int) x.size()) return -1.0f;
    std::vector<float> d ((size_t) maxLag + 1, 0.0f);
    for (int tau = 1; tau <= maxLag; ++tau)
    {
        double s = 0.0;
        for (int j = 0; j < W; ++j) { const double v = x[(size_t) (start + j)] - x[(size_t) (start + j + tau)]; s += v * v; }
        d[(size_t) tau] = (float) s;
    }
    double run = 0.0; d[0] = 1.0f;
    for (int tau = 1; tau <= maxLag; ++tau) { run += d[(size_t) tau]; d[(size_t) tau] = run > 0 ? (float) (d[(size_t) tau] * tau / run) : 1.0f; }
    int best = -1;
    for (int tau = minLag; tau <= maxLag; ++tau)
        if (d[(size_t) tau] < 0.12f) { while (tau + 1 <= maxLag && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau; best = tau; break; }
    if (best < 1) return -1.0f;
    float t = (float) best;
    const float a = d[(size_t) best - 1], b = d[(size_t) best], c = d[(size_t) best + 1], den = a - 2 * b + c;
    if (std::fabs (den) > 1e-9f) t += 0.5f * (a - c) / den;
    return (float) (fs / t);
}

static void writeWav (const char* path, const std::vector<float>& l, const std::vector<float>& r, int fs)
{
    FILE* f = std::fopen (path, "wb");
    if (! f) return;
    const int n = (int) l.size(), bytes = n * 4;
    auto w32 = [&] (int v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (short v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + bytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (2); w32 (fs); w32 (fs * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (bytes);
    for (int i = 0; i < n; ++i)
    {
        w16 ((short) std::lround (std::clamp (l[(size_t) i], -1.0f, 1.0f) * 32767.0f));
        w16 ((short) std::lround (std::clamp (r[(size_t) i], -1.0f, 1.0f) * 32767.0f));
    }
    std::fclose (f);
}

int main (int argc, char** argv)
{
    _mm_setcsr (_mm_getcsr() | 0x8040); // FTZ/DAZ like a host would
    const char* wavDir = argc > 1 ? argv[1] : nullptr;
    int failures = 0;

    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        const double seconds = 4.0;
        // A3 = 220 Hz, sung 38 cents sharp -> hard tune must pull it to 220
        auto vocal = makeVocal (fs, seconds, 220.0, 0.38);
        std::printf ("\n=== fs = %.0f ===\n", fs);

        for (int a = 0; a < kNumArtists; ++a)
        {
            Chain chain;
            chain.prepare (fs);
            Macros m; // all macros at artist default
            chain.setSettings (resolve (kArtists[a], m));

            std::vector<float> l (vocal.size()), r (vocal.size());
            const int block = 256;
            const auto t0 = std::chrono::steady_clock::now();
            for (size_t i = 0; i < vocal.size(); i += block)
            {
                const int nb = (int) std::min ((size_t) block, vocal.size() - i);
                chain.process (vocal.data() + i, vocal.data() + i, l.data() + i, r.data() + i, nb);
            }
            const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();

            bool finite = true; float peak = 0.0f; double ms = 0.0;
            for (size_t i = 0; i < l.size(); ++i)
            {
                if (! std::isfinite (l[i]) || ! std::isfinite (r[i])) finite = false;
                peak = std::max (peak, std::max (std::fabs (l[i]), std::fabs (r[i])));
                ms += 0.5 * ((double) l[i] * l[i] + (double) r[i] * r[i]);
            }
            const float rmsDb = gainToDb ((float) std::sqrt (ms / l.size()));

            // pitch error vs nearest semitone, measured on the mid signal in sung regions (dry tail only)
            std::vector<float> mid (l.size());
            for (size_t i = 0; i < l.size(); ++i) mid[i] = 0.5f * (l[i] + r[i]);
            // measure on the pitch-corrected vocal only: rerun with sends/doubler muted
            Chain dryChain; dryChain.prepare (fs);
            auto s = resolve (kArtists[a], m);
            s.delayMix = s.revMix = s.dblLevel = 0.0f; s.driveDb = 0.0f; s.satMix = 0.0f;
            dryChain.setSettings (s);
            std::vector<float> tl (vocal.size()), tr (vocal.size());
            for (size_t i = 0; i < vocal.size(); i += block)
            {
                const int nb = (int) std::min ((size_t) block, vocal.size() - i);
                dryChain.process (vocal.data() + i, vocal.data() + i, tl.data() + i, tr.data() + i, nb);
            }
            const int lat = dryChain.getLatency();
            double errSum = 0.0; int cnt = 0;
            for (double t = 1.2; t < seconds - 0.3; t += 0.05)
            {
                const double ph = std::fmod (t, 1.0);
                if (ph < 0.15 || ph > 0.6) continue;
                const int c = (int) (t * fs) + lat;
                const float f = measureF0 (tl, fs, c);
                if (f <= 0) continue;
                const float midi = 69.0f + 12.0f * std::log2 (f / 440.0f);
                errSum += std::fabs (midi - std::round (midi)) * 100.0;
                ++cnt;
            }
            const double centsErr = cnt ? errSum / cnt : -1.0;
            const double rt = sec / seconds;

            const bool ok = finite && peak <= 1.0f && rmsDb > -40.0f
                            && (kArtists[a].tuneMix < 0.9f || (centsErr >= 0 && centsErr < 12.0));
            if (! ok) ++failures;
            std::printf ("%-15s lat=%5d (%4.1f ms) peak=%5.2f rms=%6.1f dB  pitchErr=%5.1f c (n=%d)  cpu=%5.2f%%  %s\n",
                         kArtists[a].name, lat, 1000.0 * lat / fs, peak, rmsDb, centsErr, cnt, rt * 100.0, ok ? "OK" : "FAIL");

            if (wavDir && fs == 44100.0)
            {
                char path[512];
                std::snprintf (path, sizeof (path), "%s/%02d_%s.wav", wavDir, a, kArtists[a].name);
                writeWav (path, l, r, (int) fs);
            }
        }

        // uncorrected reference: input pitch error
        double errSum = 0; int cnt = 0;
        for (double t = 1.2; t < 3.7; t += 0.05)
        {
            const double ph = std::fmod (t, 1.0);
            if (ph < 0.15 || ph > 0.6) continue;
            const float f = measureF0 (vocal, fs, (int) (t * fs));
            if (f <= 0) continue;
            const float midi = 69.0f + 12.0f * std::log2 (f / 440.0f);
            errSum += std::fabs (midi - std::round (midi)) * 100.0; ++cnt;
        }
        std::printf ("(input, untuned)      pitchErr=%5.1f c\n", cnt ? errSum / cnt : -1.0);
    }

    // Stress: macros swept to extremes and randomly automated every block, all artists, loud input
    {
        const double fs = 48000.0;
        auto vocal = makeVocal (fs, 3.0, 110.0, -0.3);
        for (auto& v : vocal) v *= 3.5f; // hot input
        std::mt19937 rng (42);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        bool ok = true; float peak = 0.0f;
        for (int a = 0; a < kNumArtists; ++a)
        {
            Chain chain; chain.prepare (fs);
            std::vector<float> l (vocal.size()), r (vocal.size());
            for (size_t i = 0; i < vocal.size(); i += 128)
            {
                Macros m;
                m.tune = u (rng); m.air = u (rng); m.grit = u (rng); m.space = u (rng); m.width = u (rng);
                m.amount = u (rng); m.formantOffset = (u (rng) - 0.5f) * 8.0f; m.outputDb = (u (rng) - 0.5f) * 24.0f;
                m.key = (int) (u (rng) * 11.99f); m.scale = (int) (u (rng) * 5.99f);
                chain.setTempo (60.0 + u (rng) * 140.0);
                chain.setSettings (resolve (kArtists[a], m));
                const int nb = (int) std::min ((size_t) 128, vocal.size() - i);
                chain.process (vocal.data() + i, vocal.data() + i, l.data() + i, r.data() + i, nb);
            }
            for (size_t i = 0; i < l.size(); ++i)
            {
                if (! std::isfinite (l[i]) || ! std::isfinite (r[i])) ok = false;
                peak = std::max (peak, std::max (std::fabs (l[i]), std::fabs (r[i])));
            }
        }
        if (peak > 1.0f) ok = false;
        std::printf ("\nstress (random automation, hot input): peak=%.3f %s\n", peak, ok ? "OK" : "FAIL");
        if (! ok) ++failures;
    }

    std::printf ("\n%s (%d failures)\n", failures ? "FAILED" : "ALL PASSED", failures);
    return failures ? 1 : 0;
}
