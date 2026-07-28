#include "EffectParamMap.h"
#include "DSPBase.h"
#include "CompressorDSP.h"

namespace EffectParamMap
{
namespace
{
    CompressorDSP*       comp (DSPBase* d)       { return static_cast<CompressorDSP*> (d); }
    const CompressorDSP* comp (const DSPBase* d) { return static_cast<const CompressorDSP*> (d); }

    // ── Compressor / Modern (CompressorDSP::Type::Modern) ────────────────────
    // Full SSL-ish layout.  Straight pass-through setters: the slider works in
    // the DSP's own units, so there is no conversion to get wrong.
    const ParamDef kCompModern[] =
    {
        { "thresh", -60.f,   0.f,
          [] (DSPBase* d, float v) { comp (d)->setThreshold (v); },
          [] (const DSPBase* d) -> float { return comp (d)->threshold; } },
        { "ratio",    0.4f, 30.f,
          [] (DSPBase* d, float v) { comp (d)->setRatio (v); },
          [] (const DSPBase* d) -> float { return comp (d)->ratio; } },
        { "kneew",    0.f,  18.f,
          [] (DSPBase* d, float v) { comp (d)->setKnee (v); },
          [] (const DSPBase* d) -> float { return comp (d)->kneeDb; } },
        { "gain",   -30.f,  30.f,
          [] (DSPBase* d, float v) { comp (d)->setGain (v); },
          [] (const DSPBase* d) -> float { return comp (d)->makeupDb; } },
        { "attack",   0.f, 400.f,
          [] (DSPBase* d, float v) { comp (d)->setAttack (v); },
          [] (const DSPBase* d) -> float { return comp (d)->attackMs; } },
        { "release",  1.f, 4000.f,
          [] (DSPBase* d, float v) { comp (d)->setRelease (v); },
          [] (const DSPBase* d) -> float { return comp (d)->releaseMs; } },
        { "mix",      0.f,   1.f,
          [] (DSPBase* d, float v) { comp (d)->setMix (v); },
          [] (const DSPBase* d) -> float { return comp (d)->mix; } },
        { "looka",    0.f,   5.f,
          [] (DSPBase* d, float v) { comp (d)->setLookaheadMs (v); },
          [] (const DSPBase* d) -> float { return comp (d)->lookaheadMs; } },
        { "det",      1.f, 100.f,
          [] (DSPBase* d, float v) { comp (d)->setDetectionMs (v); },
          [] (const DSPBase* d) -> float { return comp (d)->detectionMs; } },
        { "schpf",   20.f, 2000.f,
          [] (DSPBase* d, float v) { comp (d)->setSidechainHPF (v); },
          [] (const DSPBase* d) -> float { return comp (d)->sidechainHPF; } },
    };

    // ── Compressor / FET (1176-style face plate) ─────────────────────────────
    // Two mappings that a transcription would have broken:
    //  * "Input" is a DRIVE control.  There is no threshold knob on the face
    //    plate -- turning Input UP drives harder into a fixed threshold, so more
    //    input must mean MORE compression.  [-60..0] maps INVERTED onto
    //    threshold [0..-42].
    //  * Attack / Release are switch POSITIONS 0..7 (0 = OFF) into a ms table,
    //    where position 1 is the SLOWEST and 7 the fastest.
    // Note these share the suffixes "attack"/"release" with Modern while meaning
    // something completely different -- the reason variant is part of the key.
    constexpr float kAttackMs[8]  = { 0.f, 0.8f, 0.5f, 0.3f, 0.15f, 0.075f, 0.035f, 0.020f };
    constexpr float kReleaseMs[8] = { 0.f, 1100.f, 700.f, 450.f, 250.f, 150.f, 90.f, 50.f };

    int nearestPos (const float (&table)[8], float ms)
    {
        int best = 1;   // 0 is OFF and is never the nearest match for a live time
        for (int i = 1; i < 8; ++i)
            if (std::abs (ms - table[i]) < std::abs (ms - table[best])) best = i;
        return best;
    }

    const ParamDef kCompFet[] =
    {
        { "input", -60.f, 0.f,
          [] (DSPBase* d, float v) { comp (d)->setThreshold (-(v + 60.0f) * 0.70f); },
          [] (const DSPBase* d) -> float
          { return juce::jlimit (-60.f, 0.f, -comp (d)->threshold / 0.70f - 60.0f); } },
        { "output", -30.f, 30.f,
          [] (DSPBase* d, float v) { comp (d)->setGain (v); },
          [] (const DSPBase* d) -> float { return comp (d)->makeupDb; } },
        { "attack", 0.f, 7.f,
          [] (DSPBase* d, float v)
          { comp (d)->setAttack (kAttackMs[juce::jlimit (0, 7, (int) std::round (v))]); },
          [] (const DSPBase* d) -> float
          { return (float) nearestPos (kAttackMs, comp (d)->attackMs); } },
        { "release", 0.f, 7.f,
          [] (DSPBase* d, float v)
          { comp (d)->setRelease (kReleaseMs[juce::jlimit (0, 7, (int) std::round (v))]); },
          [] (const DSPBase* d) -> float
          { return (float) nearestPos (kReleaseMs, comp (d)->releaseMs); } },
    };

    // ── Compressor / Opto (LA-2A-style face plate) ───────────────────────────
    // Face-plate scale is 0..100 on BOTH knobs, mapped onto dB internally --
    // "Gain" here is 0..100, where Modern's "gain" is -30..+30 dB.  Same suffix,
    // incompatible units: the second reason variant is part of the key.
    const ParamDef kCompOpto[] =
    {
        { "peak_reduction", 0.f, 100.f,
          [] (DSPBase* d, float v)
          { comp (d)->setThreshold (juce::jmap (v * 0.01f, 0.0f, 1.0f, 0.0f, -40.0f)); },
          [] (const DSPBase* d) -> float
          { return juce::jlimit (0.0f, 1.0f,
                     juce::jmap (comp (d)->threshold, 0.0f, -40.0f, 0.0f, 1.0f)) * 100.0f; } },
        { "gain", 0.f, 100.f,
          [] (DSPBase* d, float v)
          { comp (d)->setGain (juce::jmap (v * 0.01f, 0.0f, 1.0f, -30.0f, 30.0f)); },
          [] (const DSPBase* d) -> float
          { return juce::jlimit (0.0f, 1.0f,
                     juce::jmap (comp (d)->makeupDb, -30.0f, 30.0f, 0.0f, 1.0f)) * 100.0f; } },
    };

    // ── Compressor / CS Style (sustain pedal) ────────────────────────────────
    // Its own setters entirely; "Attack" here IS milliseconds, but on a 1..50
    // range rather than Modern's 0..400.
    const ParamDef kCompCs[] =
    {
        { "level", -12.f, 12.f,
          [] (DSPBase* d, float v) { comp (d)->setCsLevel (v); },
          [] (const DSPBase* d) -> float { return comp (d)->csLevelDb; } },
        { "tone", 0.f, 1.f,
          [] (DSPBase* d, float v) { comp (d)->setCsTone (v); },
          [] (const DSPBase* d) -> float { return comp (d)->csTone01; } },
        { "attack", 1.f, 50.f,
          [] (DSPBase* d, float v) { comp (d)->setAttack (v); },
          [] (const DSPBase* d) -> float { return comp (d)->attackMs; } },
        { "sustain", 0.f, 1.f,
          [] (DSPBase* d, float v) { comp (d)->setCsSustain (v); },
          [] (const DSPBase* d) -> float { return comp (d)->csSustain01; } },
    };
}

int variantOf (EffectType type, const DSPBase* dsp)
{
    if (dsp == nullptr) return 0;
    switch (type)
    {
        case EffectType::Compressor: return (int) comp (dsp)->mType;
        default:                     return 0;
    }
}

juce::Span<const ParamDef> defsFor (EffectType type, int variant)
{
    switch (type)
    {
        case EffectType::Compressor:
            switch ((CompressorDSP::Type) variant)
            {
                case CompressorDSP::Type::FET:
                    return { kCompFet,    (size_t) juce::numElementsInArray (kCompFet) };
                case CompressorDSP::Type::Opto:
                    return { kCompOpto,   (size_t) juce::numElementsInArray (kCompOpto) };
                case CompressorDSP::Type::CS:
                    return { kCompCs,     (size_t) juce::numElementsInArray (kCompCs) };
                case CompressorDSP::Type::Modern:
                default:
                    return { kCompModern, (size_t) juce::numElementsInArray (kCompModern) };
            }
        default:
            return {};
    }
}

const ParamDef* find (EffectType type, int variant, const juce::String& suffix)
{
    for (const auto& d : defsFor (type, variant))
        if (suffix == d.suffix) return &d;
    return nullptr;
}

bool applyNatural (EffectType type, int variant, DSPBase* dsp,
                   const juce::String& suffix, float natural)
{
    const auto* d = find (type, variant, suffix);
    if (d == nullptr || dsp == nullptr) return false;
    d->apply (dsp, juce::jlimit (d->lo, d->hi, natural));
    return true;
}

bool applyNorm (EffectType type, int variant, DSPBase* dsp,
                const juce::String& suffix, float v01)
{
    const auto* d = find (type, variant, suffix);
    if (d == nullptr || dsp == nullptr) return false;
    // Same range the slider uses, so a lane at 50% and a knob at 50% land on the
    // same DSP value by construction rather than by two implementations agreeing.
    d->apply (dsp, d->lo + juce::jlimit (0.0f, 1.0f, v01) * (d->hi - d->lo));
    return true;
}

float readNorm (EffectType type, int variant, const DSPBase* dsp,
                const juce::String& suffix, float fallback)
{
    const auto* d = find (type, variant, suffix);
    if (d == nullptr || dsp == nullptr) return fallback;
    const float range = d->hi - d->lo;
    if (range <= 0.0f) return fallback;
    return juce::jlimit (0.0f, 1.0f, (d->read (dsp) - d->lo) / range);
}
}
