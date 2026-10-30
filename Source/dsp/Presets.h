// MIMIC - artist chain recipes and macro resolution.
// Each artist is a complete vocal chain. Macros (0..1, 0.5 = artist default) bend it.
#pragma once

#include "Common.h"

namespace mimic
{
enum class DelayDiv { Slap = 0, D16, D8, D8Dot, D4, D4Dot, D2 };

struct ArtistPreset
{
    const char* name;
    const char* category;   // "Rap", "Türkçe Rap", "Pop / R&B"
    const char* accent;     // UI accent colour
    const char* tagline;    // short Turkish description for the UI

    // tune
    float tuneMix, retuneMs, formant;
    // eq
    float hpf, lowHz, lowDb, mudHz, mudDb, presHz, presDb, airHz, airDb;
    // dynamics
    float compThr, compRatio, compAtk, compRel, deessHz, deessDb;
    // colour
    float driveDb, satMix, bias;
    // doubler
    float dblLevel, dblMs, dblDepth;
    // delay
    DelayDiv delayDiv; float delayFb, delayMix, delayHp, delayLp; bool pingPong; float delayDuck;
    // reverb
    float revMix, revDecay, revPre, revDamp, revSize, revDuck;
    // image / output
    float width, outDb;
};

// Engineering notes are approximations of each artist's well-known vocal sound,
// tuned on the chain above; they are starting points, not a copy of any session.
inline const ArtistPreset kArtists[] =
{
    // ----------------------------------------------------------------- Rap
    { "Juice WRLD", "Rap", "#7c5cff", "Sıcak, melodik, hafif tune ve geniş plate",
      0.85f, 15.0f, 0.0f,
      90, 200, 1.0f, 350, -3.0f, 3000, 3.0f, 11000, 4.0f,
      -22, 4.0f, 5, 60, 6500, 6.0f,
      6.0f, 0.35f, 0.10f,
      0.18f, 18, 1.0f,
      DelayDiv::D8, 0.32f, 0.18f, 300, 5000, true, 0.6f,
      0.18f, 2.2f, 25, 7000, 0.70f, 0.4f,
      0.90f, 0.0f },

    { "Playboi Carti", "Rap", "#ff2d55", "Bebek sesi formant, sert tune, kirli ve kuru",
      0.90f, 0.0f, 3.5f,
      120, 200, -2.0f, 400, -2.0f, 2500, 4.0f, 9000, 3.0f,
      -26, 6.0f, 2, 40, 7000, 5.0f,
      18.0f, 0.70f, 0.25f,
      0.25f, 12, 0.8f,
      DelayDiv::Slap, 0.10f, 0.08f, 400, 6000, false, 0.3f,
      0.06f, 0.9f, 5, 6000, 0.30f, 0.2f,
      0.70f, -1.0f },

    { "Lil Uzi Vert", "Rap", "#00e5ff", "Çok parlak, hızlı tune, katlı ve geniş",
      1.00f, 5.0f, 1.0f,
      100, 200, -1.0f, 300, -3.0f, 3500, 4.0f, 12000, 6.0f,
      -24, 5.0f, 3, 50, 7500, 8.0f,
      8.0f, 0.30f, 0.05f,
      0.35f, 22, 1.2f,
      DelayDiv::D8Dot, 0.35f, 0.20f, 350, 6500, true, 0.5f,
      0.15f, 1.8f, 20, 8000, 0.60f, 0.3f,
      1.20f, 0.0f },

    { "Travis Scott", "Rap", "#ff8a00", "Karanlık, uzun delay ve devasa reverb",
      0.90f, 8.0f, -0.5f,
      100, 200, -1.0f, 350, -3.0f, 2800, 2.0f, 10000, 2.0f,
      -24, 5.0f, 4, 60, 6500, 6.0f,
      9.0f, 0.40f, 0.15f,
      0.25f, 20, 1.0f,
      DelayDiv::D4, 0.50f, 0.30f, 400, 3500, true, 0.5f,
      0.32f, 4.5f, 40, 4500, 0.95f, 0.3f,
      1.20f, 0.0f },

    { "Don Toliver", "Rap", "#ff5ec4", "İpeksi tepe frekans, akıcı tune",
      0.95f, 10.0f, 0.5f,
      90, 200, 0.0f, 300, -3.0f, 3200, 3.0f, 12000, 5.0f,
      -22, 4.0f, 6, 70, 7000, 7.0f,
      4.0f, 0.25f, 0.05f,
      0.20f, 20, 1.0f,
      DelayDiv::D8Dot, 0.35f, 0.20f, 300, 6000, true, 0.5f,
      0.25f, 2.8f, 30, 8000, 0.80f, 0.3f,
      1.10f, 0.0f },

    { "Ken Carson", "Rap", "#d0ff00", "Rage: aşırı distortion ve ezik kompres",
      0.95f, 3.0f, 1.0f,
      140, 220, -3.0f, 500, -1.0f, 2200, 5.0f, 8000, 1.0f,
      -30, 8.0f, 1, 30, 6000, 4.0f,
      24.0f, 0.85f, 0.30f,
      0.30f, 10, 0.7f,
      DelayDiv::D16, 0.25f, 0.10f, 500, 5000, false, 0.4f,
      0.10f, 1.2f, 10, 6000, 0.40f, 0.2f,
      0.90f, -1.5f },

    { "Future", "Rap", "#36d399", "Kalın, robotik hard-tune, koyu delay",
      1.00f, 0.0f, -0.8f,
      90, 200, 0.0f, 350, -2.0f, 2600, 3.0f, 10000, 3.0f,
      -24, 5.0f, 3, 50, 6500, 6.0f,
      10.0f, 0.45f, 0.20f,
      0.15f, 18, 0.8f,
      DelayDiv::D4, 0.40f, 0.22f, 350, 4000, false, 0.5f,
      0.20f, 2.4f, 25, 6000, 0.70f, 0.3f,
      1.00f, 0.0f },

    // ---------------------------------------------------------- Türkçe Rap
    { "Ezhel", "Türkçe Rap", "#ffd166", "Temiz, punchy, ölçülü tune",
      0.50f, 30.0f, 0.0f,
      90, 200, 0.0f, 300, -2.0f, 3000, 3.0f, 11000, 3.0f,
      -22, 4.0f, 5, 60, 6500, 6.0f,
      5.0f, 0.25f, 0.05f,
      0.12f, 18, 0.8f,
      DelayDiv::D8, 0.30f, 0.15f, 300, 5500, false, 0.5f,
      0.14f, 1.8f, 20, 7000, 0.60f, 0.3f,
      1.00f, 0.0f },

    { "Uzi", "Türkçe Rap", "#ef476f", "Agresif drill/trap, sert tune, öne çıkan vokal",
      0.95f, 4.0f, 0.0f,
      110, 200, -1.0f, 400, -2.0f, 2800, 4.0f, 10000, 3.0f,
      -26, 6.0f, 2, 45, 6500, 6.0f,
      12.0f, 0.50f, 0.20f,
      0.30f, 14, 0.9f,
      DelayDiv::D8, 0.25f, 0.12f, 350, 5000, false, 0.5f,
      0.10f, 1.3f, 15, 6500, 0.50f, 0.3f,
      1.00f, -0.5f },

    { "Lvbel C5", "Türkçe Rap", "#06d6a0", "Yoğun autotune, tiz ve enerjik",
      1.00f, 0.0f, 1.5f,
      110, 200, -1.0f, 350, -3.0f, 3200, 4.0f, 12000, 5.0f,
      -26, 6.0f, 2, 40, 7500, 7.0f,
      9.0f, 0.40f, 0.10f,
      0.35f, 16, 1.1f,
      DelayDiv::D8Dot, 0.30f, 0.16f, 350, 6500, true, 0.5f,
      0.14f, 1.6f, 15, 8000, 0.55f, 0.3f,
      1.20f, 0.0f },

    { "Blok3", "Türkçe Rap", "#118ab2", "Arabesk-trap: duygusal, bol delay ve reverb (Hicaz dene)",
      1.00f, 2.0f, 0.0f,
      90, 200, 0.0f, 350, -2.0f, 3000, 3.0f, 11000, 4.0f,
      -24, 5.0f, 3, 60, 6500, 6.0f,
      6.0f, 0.30f, 0.05f,
      0.25f, 20, 1.0f,
      DelayDiv::D4, 0.45f, 0.25f, 300, 5000, true, 0.5f,
      0.28f, 3.2f, 30, 6000, 0.85f, 0.3f,
      1.15f, 0.0f },

    // ----------------------------------------------------------- Pop / R&B
    { "Billie Eilish", "Pop / R&B", "#9be564", "Yakın, fısıltılı, yumuşak ve doğal",
      0.20f, 80.0f, 0.0f,
      70, 180, 3.0f, 450, -2.0f, 4000, 1.0f, 12000, 3.0f,
      -28, 3.0f, 10, 120, 6000, 7.0f,
      3.0f, 0.20f, 0.0f,
      0.08f, 22, 0.6f,
      DelayDiv::D4, 0.20f, 0.06f, 250, 3000, false, 0.6f,
      0.10f, 1.4f, 10, 5000, 0.50f, 0.4f,
      0.60f, 0.0f },

    { "The Weeknd", "Pop / R&B", "#e63946", "80'ler: parlak, katlı, büyük reverb",
      0.70f, 20.0f, 0.0f,
      100, 200, -1.0f, 300, -3.0f, 3500, 2.0f, 12000, 5.0f,
      -22, 4.0f, 8, 90, 7000, 7.0f,
      4.0f, 0.20f, 0.05f,
      0.30f, 24, 1.2f,
      DelayDiv::D4Dot, 0.40f, 0.22f, 300, 5000, true, 0.5f,
      0.30f, 3.8f, 35, 9000, 0.90f, 0.3f,
      1.30f, 0.0f },

    { "Frank Ocean", "Pop / R&B", "#c9a227", "Bant sıcaklığı, doğal tune, koyu oda",
      0.35f, 45.0f, 0.0f,
      80, 200, 1.5f, 450, -2.0f, 3000, 1.0f, 10000, 2.0f,
      -24, 3.5f, 10, 120, 6000, 6.0f,
      5.0f, 0.30f, 0.10f,
      0.10f, 20, 0.7f,
      DelayDiv::D4, 0.25f, 0.10f, 250, 3000, false, 0.5f,
      0.20f, 2.2f, 20, 5000, 0.70f, 0.3f,
      0.90f, 0.0f },

    { "SZA", "Pop / R&B", "#f4a261", "Havadar, yumuşak tune, geniş ve parlak",
      0.60f, 25.0f, 0.3f,
      90, 200, 0.0f, 350, -2.5f, 4000, 2.0f, 13000, 5.0f,
      -24, 4.0f, 6, 80, 7500, 7.0f,
      3.0f, 0.20f, 0.0f,
      0.22f, 20, 1.0f,
      DelayDiv::D8Dot, 0.30f, 0.14f, 300, 6500, true, 0.5f,
      0.22f, 2.6f, 25, 9000, 0.80f, 0.3f,
      1.15f, 0.0f },
};

constexpr int kNumArtists = (int) (sizeof (kArtists) / sizeof (kArtists[0]));

inline const char* const kScaleNames[] = { "Kromatik", "Majör", "Minör", "Harmonik Minör", "Hicaz", "Minör Pentatonik" };
constexpr int kNumScales = 6;

inline void scaleMask (int scaleIndex, bool out[12])
{
    static const int sets[kNumScales][12] =
    {
        { 1,1,1,1,1,1,1,1,1,1,1,1 }, // chromatic
        { 1,0,1,0,1,1,0,1,0,1,0,1 }, // major
        { 1,0,1,1,0,1,0,1,1,0,1,0 }, // natural minor
        { 1,0,1,1,0,1,0,1,1,0,0,1 }, // harmonic minor
        { 1,1,0,0,1,1,0,1,1,0,1,0 }, // Hicaz (phrygian dominant)
        { 1,0,0,1,0,1,0,1,0,0,1,0 }, // minor pentatonic
    };
    const int s = std::clamp (scaleIndex, 0, kNumScales - 1);
    for (int i = 0; i < 12; ++i) out[i] = sets[s][i] != 0;
}

//==============================================================================
struct Macros
{
    float amount = 1.0f;   // global dry/wet
    float tune = 0.5f, air = 0.5f, grit = 0.5f, space = 0.5f, width = 0.5f;
    float formantOffset = 0.0f; // semitones, added to the artist's formant
    float outputDb = 0.0f;
    int key = 0, scale = 0;
};

/** Fully resolved chain settings (what the DSP actually runs). */
struct ChainSettings
{
    float tuneMix, retuneMs, formant; int key; bool scale[12];
    float hpf, lowHz, lowDb, mudHz, mudDb, presHz, presDb, airHz, airDb;
    float compThr, compRatio, compAtk, compRel, deessHz, deessDb;
    float driveDb, satMix, bias;
    float dblLevel, dblMs, dblDepth;
    DelayDiv delayDiv; float delayFb, delayMix, delayHp, delayLp; bool pingPong; float delayDuck;
    float revMix, revDecay, revPre, revDamp, revSize, revDuck;
    float width, outDb, amount;
};

inline float delayBeats (DelayDiv d)
{
    switch (d)
    {
        case DelayDiv::D16:   return 0.25f;
        case DelayDiv::D8:    return 0.5f;
        case DelayDiv::D8Dot: return 0.75f;
        case DelayDiv::D4:    return 1.0f;
        case DelayDiv::D4Dot: return 1.5f;
        case DelayDiv::D2:    return 2.0f;
        case DelayDiv::Slap:  default: return 0.0f;
    }
}

/** Macro mapping. Every macro at 0.5 reproduces the artist recipe exactly. */
inline ChainSettings resolve (const ArtistPreset& p, const Macros& m)
{
    ChainSettings s {};
    auto bip = [] (float v) { return std::clamp ((v - 0.5f) * 2.0f, -1.0f, 1.0f); }; // -1..1

    // --- Tune: 0 = off, 0.5 = artist, 1 = hard (instant) tune
    if (m.tune <= 0.5f)
    {
        s.tuneMix = p.tuneMix * (m.tune / 0.5f);
        s.retuneMs = p.retuneMs;
    }
    else
    {
        const float t = (m.tune - 0.5f) / 0.5f;
        s.tuneMix = lerp (p.tuneMix, 1.0f, t);
        const float startMs = p.tuneMix < 0.3f ? 80.0f : p.retuneMs;
        s.retuneMs = lerp (startMs, 0.0f, t);
    }
    s.formant = std::clamp (p.formant + m.formantOffset, -5.0f, 5.0f);
    s.key = m.key;
    scaleMask (m.scale, s.scale);

    // --- EQ / Air
    const float air = bip (m.air);
    s.hpf = p.hpf;
    s.lowHz = p.lowHz; s.lowDb = p.lowDb - std::max (0.0f, air) * 1.0f;
    s.mudHz = p.mudHz; s.mudDb = p.mudDb;
    s.presHz = p.presHz; s.presDb = p.presDb + air * 2.5f;
    s.airHz = p.airHz;  s.airDb = p.airDb + air * 6.0f;

    // --- Dynamics (more air -> slightly more de-essing)
    s.compThr = p.compThr; s.compRatio = p.compRatio; s.compAtk = p.compAtk; s.compRel = p.compRel;
    s.deessHz = p.deessHz; s.deessDb = std::max (0.0f, p.deessDb + air * 3.0f);

    // --- Grit
    const float grit = bip (m.grit);
    s.driveDb = std::clamp (p.driveDb + grit * 14.0f, 0.0f, 36.0f);
    s.satMix  = grit >= 0.0f ? lerp (p.satMix, 1.0f, grit * 0.8f) : p.satMix * (1.0f + grit);
    s.bias    = p.bias;
    // heavier grit also squashes a little more
    s.compRatio = p.compRatio + std::max (0.0f, grit) * 3.0f;

    // --- Width (doubler + stereo spread)
    const float w = m.width;
    s.dblLevel = std::clamp (p.dblLevel * (w * 2.0f), 0.0f, 0.7f);
    s.dblMs = p.dblMs; s.dblDepth = p.dblDepth;
    s.width = w <= 0.5f ? lerp (0.0f, p.width, w / 0.5f) : lerp (p.width, 1.8f, (w - 0.5f) / 0.5f);

    // --- Space (delay + reverb sends, longer tails)
    const float sp = m.space;
    const float sendScale = sp * 2.0f;
    s.delayDiv = p.delayDiv; s.delayFb = std::clamp (p.delayFb * (0.7f + 0.6f * sp), 0.0f, 0.85f);
    s.delayMix = std::clamp (p.delayMix * sendScale, 0.0f, 0.8f);
    s.delayHp = p.delayHp; s.delayLp = p.delayLp; s.pingPong = p.pingPong; s.delayDuck = p.delayDuck;
    s.revMix = std::clamp (p.revMix * sendScale, 0.0f, 0.8f);
    s.revDecay = p.revDecay * (0.6f + 0.8f * sp);
    s.revPre = p.revPre; s.revDamp = p.revDamp; s.revSize = std::clamp (p.revSize + (sp - 0.5f) * 0.3f, 0.0f, 1.0f);
    s.revDuck = p.revDuck;

    s.outDb = p.outDb + m.outputDb;
    s.amount = clamp01 (m.amount);
    return s;
}
} // namespace mimic
