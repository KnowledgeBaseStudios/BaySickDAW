#include "EffectParamMap.h"
#include "DSPBase.h"
#include "CompressorDSP.h"
#include "ReverbDSP.h"
#include "ChorusDSP.h"
#include "DelayDSP.h"
#include "SaturationDSP.h"
#include "FlangerDSP.h"
#include "OverdriveDSP.h"
#include "PhaserDSP.h"
#include "TransientShaperDSP.h"
#include "LimiterDSP.h"
#include "DeEsserDSP.h"
#include "GateDSP.h"
#include "DeReverbDSP.h"
#include "BluesDriveStyleDSP.h"
#include "DistortionStyleDSP.h"
#include "FuzzStyleDSP.h"
#include "HighGainStyleDSP.h"
#include "NoiseGateStyleDSP.h"
#include "BassCompressorStyleDSP.h"
#include "BassDriverStyleDSP.h"
#include "BassOverdriveStyleDSP.h"
#include "OctaveStyleDSP.h"
#include "SynthStyleDSP.h"
#include "WahStyleDSP.h"
#include "AcousticPreampStyleDSP.h"
#include "AcousticSimulatorStyleDSP.h"
#include "NAMPedalStyleDSP.h"
#include <cmath>

namespace EffectParamMap
{
namespace
{
    // ── Entry shorthands ─────────────────────────────────────────────────────
    // Most knobs are honest pass-throughs: the slider already works in the
    // DSP's own units, so there is no conversion to get wrong and the table
    // entry calls the SAME setter the panel does.  These two macros keep those
    // to one line each so the genuinely non-trivial mappings below stand out
    // instead of drowning in boilerplate.
    //   SET_FLD - setter + public field for the read-back
    //   SET_GET - setter + const getter for the read-back
    #define SET_FLD(SUF, LO, HI, T, SETTER, FIELD)                             \
        { SUF, LO, HI,                                                         \
          [] (DSPBase* d, float v) { static_cast<T*> (d)->SETTER (v); },       \
          [] (const DSPBase* d) -> float                                       \
              { return (float) static_cast<const T*> (d)->FIELD; } }

    #define SET_GET(SUF, LO, HI, T, SETTER, GETTER)                            \
        { SUF, LO, HI,                                                         \
          [] (DSPBase* d, float v) { static_cast<T*> (d)->SETTER (v); },       \
          [] (const DSPBase* d) -> float                                       \
              { return (float) static_cast<const T*> (d)->GETTER(); } }

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
          [] (const DSPBase* d) -> float { return comp (d)->lookaheadMs; },
          nullptr, /*affectsLatency*/ true },
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

    // ── Reverb (FX-rack panel: 2 knob rows + ducking + ER + freeze) ──────────
    const ParamDef kReverb[] =
    {
        SET_GET ("room",     0.f,     2.f,     ReverbDSP, setRoomSize,      getRoomSize),
        SET_GET ("decay",    0.1f,   20.f,     ReverbDSP, setDecay,         getDecay),
        SET_GET ("hfratio",  0.3f,    2.f,     ReverbDSP, setHFRatio,       getHFRatio),
        SET_GET ("diffuse",  0.f,     1.f,     ReverbDSP, setDiffusion,     getDiffusion),
        SET_GET ("predly",   0.f,   200.f,     ReverbDSP, setPreDelay,      getPreDelayMs),
        SET_GET ("wettone", -12.f,   12.f,     ReverbDSP, setWetTone,       getWetTiltDb),
        SET_GET ("wet",      0.f,     1.f,     ReverbDSP, setWet,           getWet),
        SET_GET ("dry",      0.f,     1.f,     ReverbDSP, setDry,           getDry),
        SET_GET ("locut",   20.f,   800.f,     ReverbDSP, setLowCut,        getLowCutHz),
        SET_GET ("hicut",  1000.f, 20000.f,    ReverbDSP, setHighCut,       getHighCutHz),
        SET_GET ("bassmlt",  0.5f,    3.f,     ReverbDSP, setBassMult,      getBassMult),
        SET_GET ("bassx",   20.f,   800.f,     ReverbDSP, setBassCrossover, getBassCrossHz),
        SET_GET ("taildep",  0.f,     1.5f,    ReverbDSP, setTailModDepth,  getTailModDepthMs),
        SET_GET ("tailrt",   0.05f,   2.f,     ReverbDSP, setTailModRate,   getTailModRateHz),
        SET_GET ("stereo",   0.f,   200.f,     ReverbDSP, setStereoSep,     getStereoSep),
        SET_GET ("hidamp",  500.f, 20000.f,    ReverbDSP, setHighDamp,      getHighDampHz),
        SET_GET ("er",     -60.f,    12.f,     ReverbDSP, setER,            getER),
        SET_GET ("duck",     0.f,   100.f,     ReverbDSP, setDuckAmount,      getDuckAmount),
        SET_GET ("dkthr",  -60.f,     0.f,     ReverbDSP, setDuckThresholdDb, getDuckThresholdDb),
        SET_GET ("dkatt",    1.f,   200.f,     ReverbDSP, setDuckAttackMs,    getDuckAttackMs),
        SET_GET ("dkrel",   10.f,  1000.f,     ReverbDSP, setDuckReleaseMs,   getDuckReleaseMs),
        // The one automatable TOGGLE (EditorPanelBase::addAutomatableToggle).
        // 0..1 with a half-way threshold, matching the button applicator it
        // replaces.
        { "freeze", 0.f, 1.f,
          [] (DSPBase* d, float v) { static_cast<ReverbDSP*> (d)->setFreeze (v >= 0.5f); },
          [] (const DSPBase* d) -> float
          { return static_cast<const ReverbDSP*> (d)->getFreeze() ? 1.0f : 0.0f; } },
    };

    // ── Chorus (FX-rack panel: 3 LFO rate knobs + the shape row) ─────────────
    const ParamDef kChorus[] =
    {
        { "lfo1", 0.01f, 10.f,
          [] (DSPBase* d, float v) { static_cast<ChorusDSP*> (d)->setLFOFreq (0, v); },
          [] (const DSPBase* d) -> float { return static_cast<const ChorusDSP*> (d)->getLFOFreq (0); } },
        { "lfo2", 0.01f, 10.f,
          [] (DSPBase* d, float v) { static_cast<ChorusDSP*> (d)->setLFOFreq (1, v); },
          [] (const DSPBase* d) -> float { return static_cast<const ChorusDSP*> (d)->getLFOFreq (1); } },
        { "lfo3", 0.01f, 10.f,
          [] (DSPBase* d, float v) { static_cast<ChorusDSP*> (d)->setLFOFreq (2, v); },
          [] (const DSPBase* d) -> float { return static_cast<const ChorusDSP*> (d)->getLFOFreq (2); } },
        SET_FLD ("delay",    0.5f,    30.f, ChorusDSP, setDelay,       delayMs),
        SET_FLD ("depth",    0.f,     20.f, ChorusDSP, setDepth,       depth),
        SET_FLD ("stereo",   0.f,    360.f, ChorusDSP, setStereo,      stereo),
        SET_FLD ("crosshz", 20.f,  10000.f, ChorusDSP, setCrossCutoff, crossCutoff),
        SET_FLD ("wet",      0.f,      1.f, ChorusDSP, setWet,         wet),
    };

    // ── Delay / Echo (FX-rack panel: 2 knob rows + ducking) ─────────────────
    const ParamDef kDelayEcho[] =
    {
        SET_FLD ("time",       1.f,  2000.f, DelayDSP, setDelayMs,           delayMs),
        SET_GET ("feed",       0.f,     1.2f, DelayDSP, setFeedbackLevel,     getFeedbackLevel),
        SET_GET ("lofisr",   100.f, 48000.f, DelayDSP, setLoFiSampleRate,    getLoFiSampleRate),
        SET_GET ("wetin",      0.f,     1.f, DelayDSP, setWetIn,             getWetIn),
        SET_GET ("wet",        0.f,     1.f, DelayDSP, setWetOut,            getWetOut),
        SET_GET ("dry",        0.f,     1.f, DelayDSP, setDryOut,            getDryOut),
        SET_GET ("fbcut",     20.f, 18000.f, DelayDSP, setFeedbackCutoff,    getFBCutoff),
        SET_GET ("fbreso",     0.1f,   20.f, DelayDSP, setFeedbackResonance, getFBResonance),
        // The Tone knob is drawn left = low-pass / right = high-pass, which is
        // the INVERSE of the DSP's own sign convention.  One negation, one home.
        { "tone", -1.f, 1.f,
          [] (DSPBase* d, float v) { static_cast<DelayDSP*> (d)->setTone (-v); },
          [] (const DSPBase* d) -> float { return -static_cast<const DelayDSP*> (d)->getTone(); } },
        SET_GET ("modhz",      0.f,    20.f, DelayDSP, setModRate,         getModRate),
        SET_GET ("modtime",    0.f,     1.f, DelayDSP, setModTimeMod,      getModTimeMod),
        SET_GET ("modfb",      0.f,     1.f, DelayDSP, setModCutoffMod,    getModCutoffMod),
        SET_GET ("diff",       0.f,     1.f, DelayDSP, setDiffusionLevel,  getDiffLevel),
        SET_GET ("diffsprd",   0.f,     1.f, DelayDSP, setDiffusionSpread, getDiffSpread),
        SET_GET ("lobit",      1.f,    24.f, DelayDSP, setLoFiBits,        getLoFiBits),
        SET_GET ("fbdst",      0.f,    10.f, DelayDSP, setFBDistLevel,     getFBDistLevel),
        SET_GET ("fbknee",     0.f,     1.f, DelayDSP, setFBDistKnee,      getFBDistKnee),
        SET_GET ("fbsym",      0.f,     1.f, DelayDSP, setFBDistSymmetry,  getFBDistSymmetry),
        SET_GET ("spread",     0.f,     1.f, DelayDSP, setStereoSpread,    getStereoSpread),
        SET_GET ("pan",       -1.f,     1.f, DelayDSP, setOffsetPan,       getOffsetPan),
        SET_GET ("smooth",     0.f,     1.f, DelayDSP, setSmoothing,       getSmoothing),
        SET_GET ("duck",       0.f,   100.f, DelayDSP, setDuckAmount,      getDuckAmount),
        SET_GET ("dkthr",    -60.f,     0.f, DelayDSP, setDuckThresholdDb, getDuckThresholdDb),
        SET_GET ("dkatt",      1.f,   200.f, DelayDSP, setDuckAttackMs,    getDuckAttackMs),
        SET_GET ("dkrel",     10.f,  1000.f, DelayDSP, setDuckReleaseMs,   getDuckReleaseMs),
    };

    // ── Delay / VocalDoubler (minimal dual-tap panel) ───────────────────────
    // Same DSP, completely different face: "mix" here is a 0..100 percentage
    // onto setWetOut, where the Echo panel's "wet" is a 0..1 level on the same
    // setter -- and the pedal panel's "mix" is 0..1 onto setWetIn.  Three
    // meanings, one label.
    const ParamDef kDelayVocalDoubler[] =
    {
        SET_GET ("time_l",   5.f,   50.f, DelayDSP, setDoubleTimeLMs, getDoubleTimeLMs),
        SET_GET ("time_r",   5.f,   50.f, DelayDSP, setDoubleTimeRMs, getDoubleTimeRMs),
        SET_GET ("detune",   0.f,  100.f, DelayDSP, setDoubleDetune,  getDoubleDetune),
        SET_GET ("width",    0.f,  100.f, DelayDSP, setDoubleWidth,   getDoubleWidth),
        SET_GET ("rate",     0.1f,   2.f, DelayDSP, setDoubleRate,    getDoubleRate),
        { "mix", 0.f, 100.f,
          [] (DSPBase* d, float v) { static_cast<DelayDSP*> (d)->setWetOut (v * 0.01f); },
          [] (const DSPBase* d) -> float
          { return static_cast<const DelayDSP*> (d)->getWetOut() * 100.0f; } },
        SET_GET ("duck",     0.f,  100.f, DelayDSP, setDuckAmount,      getDuckAmount),
        SET_GET ("dkthr",  -60.f,    0.f, DelayDSP, setDuckThresholdDb, getDuckThresholdDb),
        SET_GET ("dkatt",    1.f,  200.f, DelayDSP, setDuckAttackMs,    getDuckAttackMs),
        SET_GET ("dkrel",   10.f, 1000.f, DelayDSP, setDuckReleaseMs,   getDuckReleaseMs),
    };

    // ── Saturation / Tube (the full 8-knob reference layout) ────────────────
    const ParamDef kSatTube[] =
    {
        SET_FLD ("flowers",   0.f,  10.f,  SaturationDSP, setFlowers,     mFlowers),
        SET_FLD ("dabs",      0.f,  10.f,  SaturationDSP, setDabs,        mDabs),
        SET_FLD ("input",   -12.f,  12.f,  SaturationDSP, setSensitivity, mSensitivity),
        SET_FLD ("bassrlf",   0.f, 100.f,  SaturationDSP, setBassRelief,  mBassRelief),
        SET_FLD ("tonepre",  -9.f,   9.f,  SaturationDSP, setTonePre,     mTonePre),
        SET_FLD ("tonepost", -9.f,   9.f,  SaturationDSP, setTonePost,    mTonePost),
        SET_FLD ("wet",       0.f, 100.f,  SaturationDSP, setWet,         mWet),
        SET_FLD ("out",     -18.f,  18.f,  SaturationDSP, setOut,         mOut),
    };

    // ── Saturation / Console (minimalist 4-knob face) ───────────────────────
    const ParamDef kSatConsole[] =
    {
        SET_FLD ("drive",   0.f,  10.f, SaturationDSP, setFlowers, mFlowers),
        SET_FLD ("color",   0.f,  10.f, SaturationDSP, setDabs,    mDabs),
        SET_FLD ("mix",     0.f, 100.f, SaturationDSP, setWet,     mWet),
        SET_FLD ("output",-18.f,  18.f, SaturationDSP, setOut,     mOut),
    };

    // ── Saturation / Tape (== EffectType::Tape; TapeSatPanel) ───────────────
    // "Drive" and "Hiss" are dB on the face and LINEAR GAIN in the DSP.  That
    // conversion is exactly the kind of load-bearing math that must not exist
    // twice, and it is also why this panel's "drive" cannot share a table with
    // the pedal board's "Drive" (0..10 straight into setFlowers).
    const ParamDef kSatTape[] =
    {
        SET_FLD ("vibe",     0.f,     1.f,  SaturationDSP, setTapeVibe,       mTapeVibe),
        SET_FLD ("hyst",     0.f,     2.f,  SaturationDSP, setTapeHystAmount, mTapeHystAmount),
        SET_FLD ("bias",     0.f,    10.f,  SaturationDSP, setTapeBias,       mTapeBias),
        { "drive", -24.f, 24.f,
          [] (DSPBase* d, float v)
          { static_cast<SaturationDSP*> (d)->setTapeInputGain (juce::Decibels::decibelsToGain (v)); },
          [] (const DSPBase* d) -> float
          { return juce::Decibels::gainToDecibels (static_cast<const SaturationDSP*> (d)->mTapeInputGain, -24.0f); } },
        { "hiss", -80.f, 0.f,
          [] (DSPBase* d, float v)
          { static_cast<SaturationDSP*> (d)->setTapeHiss (juce::Decibels::decibelsToGain (v)); },
          [] (const DSPBase* d) -> float
          { return juce::Decibels::gainToDecibels (static_cast<const SaturationDSP*> (d)->mTapeHiss, -80.0f); } },
        SET_FLD ("wowhz",    0.1f,    5.f,  SaturationDSP, setTapeWowRate,      mTapeWowRate),
        SET_FLD ("wowdp",    0.f,     1.f,  SaturationDSP, setTapeWowDepth,     mTapeWowDepth),
        SET_FLD ("fluthz",   1.f,    15.f,  SaturationDSP, setTapeFlutterRate,  mTapeFlutterRate),
        SET_FLD ("flutdp",   0.f,     1.f,  SaturationDSP, setTapeFlutterDepth, mTapeFlutterDepth),
        SET_FLD ("preshf", -12.f,    12.f,  SaturationDSP, setTapePreShelfDb,   mTapePreShelfDb),
        SET_FLD ("deshf",  -12.f,    12.f,  SaturationDSP, setTapeDeShelfDb,    mTapeDeShelfDb),
        SET_FLD ("lopass", 5000.f, 22000.f, SaturationDSP, setTapeLpHz,         mTapeLpHz),
    };

    // ── Flanger (FX-rack panel) ─────────────────────────────────────────────
    const ParamDef kFlanger[] =
    {
        SET_FLD ("rate",    0.05f,   5.f, FlangerDSP, setRate,   mRate),
        SET_FLD ("depth",   0.f,    10.f, FlangerDSP, setDepth,  mDepth),
        SET_FLD ("delay",   0.f,    20.f, FlangerDSP, setDelay,  mDelay),
        // Feedback is a PERCENTAGE on the face and a 0..1 coefficient inside.
        { "feed", 0.f, 100.f,
          [] (DSPBase* d, float v) { static_cast<FlangerDSP*> (d)->setFeed (v); },
          [] (const DSPBase* d) -> float
          { return static_cast<const FlangerDSP*> (d)->mFeedback * 100.0f; } },
        SET_FLD ("phase",   0.f,   360.f, FlangerDSP, setPhase,  mStereoPhase),
        SET_FLD ("shape",   0.f,     1.f, FlangerDSP, setShape,  mShape),
        // Damp is a 0..1 face control over a LOGARITHMIC 20 kHz -> 200 Hz
        // cutoff sweep.  Both directions live here so the knob's read-back and
        // an automation lane cannot disagree about what 0.5 means.
        { "damp", 0.f, 1.f,
          [] (DSPBase* d, float v)
          { static_cast<FlangerDSP*> (d)->setDampHz (20000.0f * std::pow (200.0f / 20000.0f, v)); },
          [] (const DSPBase* d) -> float
          {
              const float hz = juce::jmax (1.0f, static_cast<const FlangerDSP*> (d)->mDampHz);
              return juce::jlimit (0.0f, 1.0f,
                         std::log (hz / 20000.0f) / std::log (200.0f / 20000.0f));
          } },
        SET_FLD ("wet",     0.f,     1.f, FlangerDSP, setWet,        mWet),
        SET_FLD ("cross", -96.f,     0.f, FlangerDSP, setCrossLevel, mCrossLevelDb),
    };

    // ── Overdrive / Rack ────────────────────────────────────────────────────
    const ParamDef kOverdriveRack[] =
    {
        SET_FLD ("drive",    0.f,    10.f, OverdriveDSP, setPreAmp,     mPreAmp),
        SET_FLD ("color",  200.f,  8000.f, OverdriveDSP, setColor,      mColor),
        SET_FLD ("band",     0.f,     1.f, OverdriveDSP, setPreBand,    mPreBand),
        SET_FLD ("bias",    -1.f,     1.f, OverdriveDSP, setBias,       mBias),
        SET_FLD ("filter", 500.f, 18000.f, OverdriveDSP, setPostFilter, mPostFilter),
        SET_FLD ("out",    -18.f,     0.f, OverdriveDSP, setPostGain,   mPostGain),
        SET_FLD ("wet",      0.f,     1.f, OverdriveDSP, setWet,        mWet),
    };

    // ── Overdrive / Pedal (OD Style stomp face) ─────────────────────────────
    const ParamDef kOverdrivePedal[] =
    {
        SET_FLD ("drive",   0.f,   10.f, OverdriveDSP, setPreAmp,     mPreAmp),
        SET_FLD ("tone",  200.f, 8000.f, OverdriveDSP, setPostFilter, mPostFilter),
        SET_FLD ("level", -18.f,   18.f, OverdriveDSP, setPostGain,   mPostGain),
    };

    // ── Phaser (FX-rack panel) ──────────────────────────────────────────────
    // "Rate" is the one param in the whole map whose RANGE moves at runtime:
    // the Slow/Fast Range switch retunes the upper bound between 2 Hz and
    // 10 Hz, and the applicator this replaced read the live slider bounds on
    // every tick.  rangeOf preserves that.  Note also that the rack's Rate
    // drives setSweepFreq while the pedal face's Rate drives setRate -- a
    // different setter behind the same label.
    const ParamDef kPhaser[] =
    {
        { "rate", 0.05f, 10.f,
          [] (DSPBase* d, float v) { static_cast<PhaserDSP*> (d)->setSweepFreq (v); },
          [] (const DSPBase* d) -> float { return static_cast<const PhaserDSP*> (d)->mSweepHz; },
          [] (const DSPBase* d, float& lo, float& hi)
          { lo = 0.05f; hi = static_cast<const PhaserDSP*> (d)->getRateMaxHz(); } },
        SET_FLD ("minhz",   20.f, 2000.f, PhaserDSP, setMinDepth, mMinDepthHz),
        SET_FLD ("maxhz",  200.f, 8000.f, PhaserDSP, setMaxDepth, mMaxDepthHz),
        SET_FLD ("feed",     0.f,    1.2f, PhaserDSP, setFeedback, mFeedback),
        SET_FLD ("stereo",   0.f,  360.f, PhaserDSP, setStereo,   mStereoPhase),
        SET_FLD ("wet",      0.f,    1.f, PhaserDSP, setWet,      mWet),
        SET_FLD ("cross",    0.f,    1.f, PhaserDSP, setCrossFB,  mCrossFB),
        SET_FLD ("gain",   -18.f,   18.f, PhaserDSP, setOutGain,  mOutGainDb),
    };

    // ── Transient Shaper ────────────────────────────────────────────────────
    // Attack / Release are -100..+100 on the face and -1..+1 inside.
    const ParamDef kTransientShaper[] =
    {
        { "attack", -100.f, 100.f,
          [] (DSPBase* d, float v) { static_cast<TransientShaperDSP*> (d)->setAttack (v); },
          [] (const DSPBase* d) -> float
          { return static_cast<const TransientShaperDSP*> (d)->mAttack * 100.0f; } },
        { "release", -100.f, 100.f,
          [] (DSPBase* d, float v) { static_cast<TransientShaperDSP*> (d)->setRelease (v); },
          [] (const DSPBase* d) -> float
          { return static_cast<const TransientShaperDSP*> (d)->mRelease * 100.0f; } },
        SET_FLD ("sens",       0.f,    1.f, TransientShaperDSP, setSensitivity, mSensitivity),
        SET_FLD ("split",      5.f, 1000.f, TransientShaperDSP, setSplitFreq,   mSplitFreq),
        SET_FLD ("balance", -100.f,  100.f, TransientShaperDSP, setSplitBalance, mSplitBalance),
        SET_FLD ("drive",      0.f,   10.f, TransientShaperDSP, setDrive,       mDrive),
        SET_FLD ("gain",     -18.f,   18.f, TransientShaperDSP, setGain,        mOutGainDb),
        SET_FLD ("wet",        0.f,    1.f, TransientShaperDSP, setWet,         mWet),
        SET_FLD ("fastrel",    1.f,   50.f, TransientShaperDSP, setFastRelMs,   mFastRelMs),
        SET_FLD ("slowatt",    1.f,   50.f, TransientShaperDSP, setSlowAttMs,   mSlowAttMs),
    };

    // ── Limiter (FX-rack panel) ─────────────────────────────────────────────
    const ParamDef kLimiter[] =
    {
        SET_GET ("ingain", -12.f,   24.f, LimiterDSP, setInputGainDb,  getInputGainDb),
        SET_GET ("ceil",   -24.f,   12.f, LimiterDSP, setCeilingDb,    getCeilingDb),
        SET_GET ("satth",    0.f,    1.f, LimiterDSP, setSatThresh,    getSatThresh),
        SET_GET ("satcv",    0.f,    1.f, LimiterDSP, setSatCurve,     getSatCurve),
        SET_GET ("schpf",   20.f, 2000.f, LimiterDSP, setSidechainHPF, getSidechainHPF),
        SET_GET ("atk",      0.1f,  20.f, LimiterDSP, setAttackMs,     getAttackMs),
        SET_GET ("rel",     10.f, 1000.f, LimiterDSP, setReleaseMs,    getReleaseMs),
        { "ahead", 0.f, 10.f,
          [] (DSPBase* d, float v) { static_cast<LimiterDSP*> (d)->setAheadMs (v); },
          [] (const DSPBase* d) -> float { return static_cast<const LimiterDSP*> (d)->getAheadMs(); },
          nullptr, /*affectsLatency*/ true },
        SET_GET ("relcv",    0.f,    1.f, LimiterDSP, setReleaseCurve, getReleaseCurve),
        SET_GET ("sustain",  0.f, 1000.f, LimiterDSP, setSustainMs,    getSustainMs),
    };

    // ── Limiter, MAXIMIZER mode (TS7) ───────────────────────────────────────
    // The reproduction's set (kLimiter above) PLUS the maximizer's own two
    // knobs.  Jeff's ruling 2026-07-29: Limiter mode is the FL reproduction and
    // carries none of the TS7 additions, so they live only here.
    //
    // THE KEY IS THE KNOB'S LABEL, LOWERCASED.  EditorPanelBase::setSlotContext
    // derives every paramId from `label.getText().toLowerCase()`, so these two
    // MUST read "lufs" / "dbtp" to match the VKnob labels in LimiterPanel.  A
    // mismatch here is silent: the Automate menu still offers the lane and still
    // draws it, and nothing ever applies it -- the same defect class as the sfizz
    // kit-CC lanes TS3 had to fix.
    //
    // Mode and character are absent on purpose.  Mode is the variant itself, and
    // the character is a chickenhead selector -- the same scope boundary TS3
    // held, where only stamped knobs get a table entry.  The two automatic-mode
    // toggles are DualLabelToggles that do not go through addAutomatableToggle,
    // matching the limiter's three existing toggles.
    const ParamDef kLimiterMaximizer[] =
    {
        SET_GET ("ingain", -12.f,   24.f, LimiterDSP, setInputGainDb,  getInputGainDb),
        SET_GET ("ceil",   -24.f,   12.f, LimiterDSP, setCeilingDb,    getCeilingDb),
        SET_GET ("satth",    0.f,    1.f, LimiterDSP, setSatThresh,    getSatThresh),
        SET_GET ("satcv",    0.f,    1.f, LimiterDSP, setSatCurve,     getSatCurve),
        SET_GET ("schpf",   20.f, 2000.f, LimiterDSP, setSidechainHPF, getSidechainHPF),
        SET_GET ("atk",      0.1f,  20.f, LimiterDSP, setAttackMs,     getAttackMs),
        SET_GET ("rel",     10.f, 1000.f, LimiterDSP, setReleaseMs,    getReleaseMs),
        { "ahead", 0.f, 10.f,
          [] (DSPBase* d, float v) { static_cast<LimiterDSP*> (d)->setAheadMs (v); },
          [] (const DSPBase* d) -> float { return static_cast<const LimiterDSP*> (d)->getAheadMs(); },
          nullptr, /*affectsLatency*/ true },
        SET_GET ("relcv",    0.f,    1.f, LimiterDSP, setReleaseCurve, getReleaseCurve),
        SET_GET ("sustain",  0.f, 1000.f, LimiterDSP, setSustainMs,    getSustainMs),
        SET_GET ("lufs",   -30.f,    0.f, LimiterDSP, setLoudnessTargetLufs, getLoudnessTargetLufs),
        SET_GET ("dbtp",    -6.f,    0.f, LimiterDSP, setTruePeakTargetDb,   getTruePeakTargetDb),
    };

    // ── De-Esser ────────────────────────────────────────────────────────────
    // "Detect" is a MACRO knob: the reference exposes only an abstract 0..100
    // width, and the panel derives both the detector frequency and Q from it.
    // The panel additionally nudges its own Freq/Q knobs, which is display
    // work; the DSP half is what belongs here.
    const ParamDef kDeEsser[] =
    {
        { "detect", 0.f, 100.f,
          [] (DSPBase* d, float v)
          {
              auto* de = static_cast<DeEsserDSP*> (d);
              de->setFrequencyHz (7500.0f - (v / 100.0f) * 3500.0f);
              de->setQ           (2.2f    - (v / 100.0f) * 1.4f);
          },
          [] (const DSPBase* d) -> float
          { return juce::jlimit (0.0f, 100.0f,
                     (7500.0f - static_cast<const DeEsserDSP*> (d)->mFreqHz) / 35.0f); } },
        SET_FLD ("thresh", -80.f,     0.f, DeEsserDSP, setThresholdDb, mThresholdDb),
        SET_FLD ("range",  -48.f,     0.f, DeEsserDSP, setRangeDb,     mRangeDb),
        SET_FLD ("mode",     0.f,   100.f, DeEsserDSP, setModeBlend,   mModeBlend),
        SET_FLD ("freq",  4000.f, 12000.f, DeEsserDSP, setFrequencyHz, mFreqHz),
        SET_FLD ("q",        0.5f,    4.f, DeEsserDSP, setQ,           mQ),
        SET_FLD ("atk",      0.1f,   30.f, DeEsserDSP, setAttackMs,    mAttackMs),
        SET_FLD ("rel",     10.f,   500.f, DeEsserDSP, setReleaseMs,   mReleaseMs),
        { "look", 0.f, DeEsserDSP::kMaxLookaheadMs,
          [] (DSPBase* d, float v) { static_cast<DeEsserDSP*> (d)->setLookaheadMs (v); },
          [] (const DSPBase* d) -> float { return static_cast<const DeEsserDSP*> (d)->mLookaheadMs; },
          nullptr, /*affectsLatency*/ true },
        SET_FLD ("mix",      0.f,     1.f, DeEsserDSP, setMix,         mMix),
    };

    // ── Gate / De-Reverb (locked vocal-chain stages) ────────────────────────
    const ParamDef kGate[] =
    {
        SET_FLD ("thresh",  -80.f,    0.f, GateDSP, setThresholdDb, thresholdDb),
        SET_FLD ("range",   -80.f,    0.f, GateDSP, setRangeDb,     rangeDb),
        SET_FLD ("attack",    0.1f, 100.f, GateDSP, setAttackMs,    attackMs),
        SET_FLD ("hold",      0.f,  500.f, GateDSP, setHoldMs,      holdMs),
        SET_FLD ("release",   5.f, 2000.f, GateDSP, setReleaseMs,   releaseMs),
    };

    const ParamDef kDeReverb[] =
    {
        SET_FLD ("reduce",   0.f,  100.f, DeReverbDSP, setReductionPct, reductionPct),
        SET_FLD ("tail",   100.f, 1000.f, DeReverbDSP, setTailMs,       tailMs),
        SET_FLD ("mix",      0.f,  100.f, DeReverbDSP, setMixPct,       mixPct),
    };

    // ── BaySickPedals native pedals (EffectType 100+) ───────────────────────
    const ParamDef kBluesDrive[] =
    {
        SET_FLD ("drive",  0.f,   1.f, BluesDriveStyleDSP, setDrive, mDrive),
        SET_FLD ("tone",   0.f,   1.f, BluesDriveStyleDSP, setTone,  mTone),
        SET_FLD ("level",-24.f,  12.f, BluesDriveStyleDSP, setLevel, mLevel),
    };

    const ParamDef kDistortion[] =
    {
        SET_FLD ("dist",   0.f,   1.f, DistortionStyleDSP, setDist,  mDist),
        SET_FLD ("tone",   0.f,   1.f, DistortionStyleDSP, setTone,  mTone),
        SET_FLD ("level",-24.f,  12.f, DistortionStyleDSP, setLevel, mLevel),
    };

    const ParamDef kFuzz[] =
    {
        SET_FLD ("fuzz",   0.f,   1.f, FuzzStyleDSP, setFuzz,  mFuzz),
        SET_FLD ("boost",  0.f,  20.f, FuzzStyleDSP, setBoost, mBoost),
        SET_FLD ("level",-24.f,  12.f, FuzzStyleDSP, setLevel, mLevel),
    };

    const ParamDef kHighGain[] =
    {
        SET_FLD ("dist",    0.f,     1.f, HighGainStyleDSP, setDist,   mDist),
        SET_FLD ("low",   -15.f,    15.f, HighGainStyleDSP, setLowDb,  mLowDb),
        SET_FLD ("mid_hz", 200.f, 5000.f, HighGainStyleDSP, setMidHz,  mMidHz),
        SET_FLD ("mid_db", -15.f,   15.f, HighGainStyleDSP, setMidDb,  mMidDb),
        SET_FLD ("high",   -15.f,   15.f, HighGainStyleDSP, setHighDb, mHighDb),
        SET_FLD ("level",  -24.f,   12.f, HighGainStyleDSP, setLevel,  mLevel),
    };

    const ParamDef kBassDriver[] =
    {
        SET_FLD ("drive",  0.f,   1.f, BassDriverStyleDSP, setDrive, mDrive),
        SET_FLD ("blend",  0.f,   1.f, BassDriverStyleDSP, setBlend, mBlend),
        SET_FLD ("low",    0.f,   1.f, BassDriverStyleDSP, setLow,   mLow),
        SET_FLD ("high",   0.f,   1.f, BassDriverStyleDSP, setHigh,  mHigh),
        SET_FLD ("level",-24.f,  12.f, BassDriverStyleDSP, setLevel, mLevel),
    };

    const ParamDef kBassOverdrive[] =
    {
        SET_FLD ("gain",     0.f,   1.f, BassOverdriveStyleDSP, setGain,    mGain),
        SET_FLD ("balance",  0.f,   1.f, BassOverdriveStyleDSP, setBalance, mBalance),
        SET_FLD ("low",    -15.f,  15.f, BassOverdriveStyleDSP, setEqLow,   mEqLow),
        SET_FLD ("high",   -15.f,  15.f, BassOverdriveStyleDSP, setEqHigh,  mEqHigh),
        SET_FLD ("level",  -24.f,  12.f, BassOverdriveStyleDSP, setLevel,   mLevel),
    };

    // Octave's face labels are the interval names, so the automation suffixes
    // are literally "+1" / "-1" / "-2" (lowercasing leaves them unchanged).
    const ParamDef kOctave[] =
    {
        SET_FLD ("direct", 0.f, 1.f, OctaveStyleDSP, setDirectLevel, mDirectLevel),
        SET_FLD ("+1",     0.f, 1.f, OctaveStyleDSP, setOct1Up,      mOct1Up),
        SET_FLD ("-1",     0.f, 1.f, OctaveStyleDSP, setOct1Down,    mOct1Down),
        SET_FLD ("-2",     0.f, 1.f, OctaveStyleDSP, setOct2Down,    mOct2Down),
        SET_FLD ("range",  0.f, 1.f, OctaveStyleDSP, setRange,       mRange),
    };

    const ParamDef kNoiseGate[] =
    {
        SET_FLD ("threshold", -60.f,    0.f, NoiseGateStyleDSP, setThresholdDb, mThresholdDb),
        SET_FLD ("decay",       1.f, 1000.f, NoiseGateStyleDSP, setDecayMs,     mDecayMs),
    };

    const ParamDef kBassCompressor[] =
    {
        SET_FLD ("thresh",  -48.f,   0.f, BassCompressorStyleDSP, setThresholdDb, mThresholdDb),
        SET_FLD ("ratio",     1.f,  10.f, BassCompressorStyleDSP, setRatio,       mRatio),
        SET_FLD ("release",  50.f, 500.f, BassCompressorStyleDSP, setReleaseMs,   mReleaseMs),
        SET_FLD ("level",   -24.f,  12.f, BassCompressorStyleDSP, setLevel,       mLevel),
    };

    const ParamDef kSynthStyle[] =
    {
        // Variation is an integer position; round rather than truncate so a
        // lane sweep lands on each variation symmetrically.
        { "var", 1.f, 11.f,
          [] (DSPBase* d, float v)
          { static_cast<SynthStyleDSP*> (d)->setVariation ((int) std::lround (v)); },
          [] (const DSPBase* d) -> float
          { return (float) static_cast<const SynthStyleDSP*> (d)->mVariation; } },
        SET_FLD ("tone",   0.f, 1.f, SynthStyleDSP, setTone,        mTone),
        SET_FLD ("rate",   0.f, 1.f, SynthStyleDSP, setRate,        mRate),
        SET_FLD ("depth",  0.f, 1.f, SynthStyleDSP, setDepth,       mDepth),
        SET_FLD ("effect", 0.f, 1.f, SynthStyleDSP, setEffectLevel, mEffectLevel),
        SET_FLD ("direct", 0.f, 1.f, SynthStyleDSP, setDirectLevel, mDirectLevel),
    };

    const ParamDef kWah[] =
    {
        SET_FLD ("pedal", 0.f, 1.f, WahStyleDSP, setPedalPosition, mPedalPosition),
    };

    // The Notch knob's bottom 5 Hz is an OFF zone rather than a frequency --
    // the branch is the control's actual behaviour, so it lives here too.
    const ParamDef kAcousticPreamp[] =
    {
        SET_FLD ("resonance", 0.f,   1.f, AcousticPreampStyleDSP, setResonance, mResonance01),
        SET_FLD ("ambience",  0.f,   1.f, AcousticPreampStyleDSP, setAmbience,  mAmbience01),
        { "notch", 45.f, 1000.f,
          [] (DSPBase* d, float v)
          {
              auto* ap = static_cast<AcousticPreampStyleDSP*> (d);
              if (v < 50.0f) { ap->setNotchOn (false); }
              else           { ap->setNotchOn (true); ap->setNotchHz (v); }
          },
          [] (const DSPBase* d) -> float
          {
              const auto* ap = static_cast<const AcousticPreampStyleDSP*> (d);
              return ap->getNotchOn() ? ap->mNotchHz : 45.0f;
          } },
        SET_FLD ("level", -24.f, 12.f, AcousticPreampStyleDSP, setLevelDb, mLevelDb),
    };

    const ParamDef kAcousticSimulator[] =
    {
        SET_FLD ("top",    -15.f, 15.f, AcousticSimulatorStyleDSP, setTopDb,   mTopDb),
        SET_FLD ("body",   -15.f, 15.f, AcousticSimulatorStyleDSP, setBodyDb,  mBodyDb),
        SET_FLD ("reverb",   0.f,  1.f, AcousticSimulatorStyleDSP, setReverb,  mReverb01),
        SET_FLD ("level",  -24.f, 12.f, AcousticSimulatorStyleDSP, setLevelDb, mLevelDb),
    };

    const ParamDef kNamPedal[] =
    {
        SET_FLD ("input_drive", -24.f, 24.f, NAMPedalStyleDSP, setInputDb,  mInputDb),
        SET_FLD ("low",         -15.f, 15.f, NAMPedalStyleDSP, setLowDb,    mLowDb),
        SET_FLD ("mid",         -15.f, 15.f, NAMPedalStyleDSP, setMidDb,    mMidDb),
        SET_FLD ("high",        -15.f, 15.f, NAMPedalStyleDSP, setHighDb,   mHighDb),
        SET_FLD ("blend",         0.f,  1.f, NAMPedalStyleDSP, setBlend,    mBlend01),
        SET_FLD ("output",      -24.f, 12.f, NAMPedalStyleDSP, setOutputDb, mOutputDb),
    };

    // ── Pedal-board faces of the 7 dual-panel rack effects ──────────────────
    // Reached only when the slot lives on a BaySickPedals board
    // (PanelContext::Pedal).  Every one of these shares at least one suffix
    // with its rack twin, and three of them mean something different there.
    const ParamDef kLimiterPedal[] =
    {
        SET_GET ("ceiling", -24.f,    0.f, LimiterDSP, setCeilingDb, getCeilingDb),
        SET_GET ("release",  10.f, 1000.f, LimiterDSP, setReleaseMs, getReleaseMs),
    };

    // "Drive" here is 0..10 into setFlowers whatever the DSP's satType is --
    // including satType == Tape, where the RACK panel's "Drive" is -24..+24 dB
    // into setTapeInputGain.  This is the collision that forced PanelContext.
    const ParamDef kSaturationPedal[] =
    {
        SET_FLD ("drive", 0.f,  10.f, SaturationDSP, setFlowers, mFlowers),
        SET_FLD ("mix",   0.f, 100.f, SaturationDSP, setWet,     mWet),
    };

    const ParamDef kChorusPedal[] =
    {
        { "rate", 0.05f, 5.f,
          [] (DSPBase* d, float v) { static_cast<ChorusDSP*> (d)->setLFOFreq (0, v); },
          [] (const DSPBase* d) -> float { return static_cast<const ChorusDSP*> (d)->getLFOFreq (0); } },
        SET_FLD ("depth", 0.f, 20.f, ChorusDSP, setDepth, depth),
        SET_FLD ("mix",   0.f,  1.f, ChorusDSP, setWet,   wet),
    };

    const ParamDef kFlangerPedal[] =
    {
        SET_FLD ("rate",     0.05f,  5.f, FlangerDSP, setRate,  mRate),
        SET_FLD ("depth",    0.f,   10.f, FlangerDSP, setDepth, mDepth),
        SET_FLD ("feedback", -1.f,   1.f, FlangerDSP, setFeedback, mFeedback),
        SET_FLD ("mix",      0.f,    1.f, FlangerDSP, setWet,   mWet),
    };

    // The pedal face's Rate drives setRate (the DSP's own LFO rate); the rack
    // face's Rate drives setSweepFreq.  Same label, different setter.
    const ParamDef kPhaserPedal[] =
    {
        SET_FLD ("rate",     0.05f, 10.f, PhaserDSP, setRate,     mRate),
        SET_FLD ("depth",    0.f,    1.f, PhaserDSP, setDepth,    mDepth),
        SET_FLD ("feedback", -1.2f,  1.2f, PhaserDSP, setFeedback, mFeedback),
        SET_FLD ("mix",      0.f,    1.f, PhaserDSP, setWet,      mWet),
    };

    const ParamDef kDelayPedal[] =
    {
        SET_FLD ("time",       1.f, 2000.f, DelayDSP, setDelayMs,       delayMs),
        SET_GET ("feedback",   0.f,    1.2f, DelayDSP, setFeedbackLevel, getFeedbackLevel),
        SET_GET ("mix",        0.f,    1.f, DelayDSP, setWetIn,         getWetIn),
    };

    const ParamDef kReverbPedal[] =
    {
        SET_GET ("decay", 0.1f,     10.f, ReverbDSP, setDecay,     getDecay),
        SET_GET ("damp",  500.f, 20000.f, ReverbDSP, setHighDamp,  getHighDampHz),
        SET_GET ("mix",   0.f,       1.f, ReverbDSP, setWet,       getWet),
    };

    #undef SET_FLD
    #undef SET_GET

    template <size_t N>
    juce::Span<const ParamDef> span (const ParamDef (&t)[N])
    {
        return { t, N };
    }

    // Does this EffectType build a different panel on a pedals board?
    bool hasPedalFace (EffectType type) noexcept
    {
        switch (type)
        {
            case EffectType::Limiter:
            case EffectType::Saturation:
            case EffectType::Tape:        // alias of SaturationDSP + Type::Tape
            case EffectType::Chorus:
            case EffectType::Flanger:
            case EffectType::Phaser:
            case EffectType::Delay:
            case EffectType::Reverb:      return true;
            default:                      return false;
        }
    }

    juce::Span<const ParamDef> pedalDefsFor (EffectType type)
    {
        switch (type)
        {
            case EffectType::Limiter:    return span (kLimiterPedal);
            case EffectType::Saturation:
            case EffectType::Tape:       return span (kSaturationPedal);
            case EffectType::Chorus:     return span (kChorusPedal);
            case EffectType::Flanger:    return span (kFlangerPedal);
            case EffectType::Phaser:     return span (kPhaserPedal);
            case EffectType::Delay:      return span (kDelayPedal);
            case EffectType::Reverb:     return span (kReverbPedal);
            default:                     return {};
        }
    }
}

int variantOf (EffectType type, const DSPBase* dsp, PanelContext ctx)
{
    // Context first: on a pedals board the panel (and therefore the knob
    // vocabulary) is decided by the surface, not by the DSP's character mode.
    if (ctx == PanelContext::Pedal && hasPedalFace (type))
        return kPedalVariant;

    if (dsp == nullptr) return 0;
    switch (type)
    {
        case EffectType::Compressor: return (int) comp (dsp)->mType;
        case EffectType::Saturation: return (int) static_cast<const SaturationDSP*> (dsp)->mSatType;
        case EffectType::Delay:      return       static_cast<const DelayDSP*>      (dsp)->getType();
        case EffectType::Overdrive:  return (int) static_cast<const OverdriveDSP*>  (dsp)->mType;
        // TS7: Limiter / Maximizer.  Mode is a real variant because the two modes
        // expose DIFFERENT control sets -- Limiter stays the FL reproduction and
        // carries none of the maximizer additions.  The character voicing is NOT
        // a variant: it leaves the parameter set identical, and 2 modes x 8
        // characters would be 16 tables describing one set.
        case EffectType::Limiter:
            return (int) static_cast<const LimiterDSP*> (dsp)->getMode();
        default:                     return 0;
    }
}

juce::Span<const ParamDef> defsFor (EffectType type, int variant)
{
    if (variant == kPedalVariant)
        return pedalDefsFor (type);

    switch (type)
    {
        case EffectType::Compressor:
            switch ((CompressorDSP::Type) variant)
            {
                case CompressorDSP::Type::FET:    return span (kCompFet);
                case CompressorDSP::Type::Opto:   return span (kCompOpto);
                case CompressorDSP::Type::CS:     return span (kCompCs);
                case CompressorDSP::Type::Modern:
                default:                          return span (kCompModern);
            }

        case EffectType::Saturation:
            switch ((SaturationDSP::Type) variant)
            {
                case SaturationDSP::Type::Console: return span (kSatConsole);
                case SaturationDSP::Type::Tape:    return span (kSatTape);
                case SaturationDSP::Type::Tube:
                default:                           return span (kSatTube);
            }

        // EffectType::Tape is an alias for SaturationDSP + Type::Tape, so its
        // slot always holds a SaturationDSP and always builds TapeSatPanel.
        case EffectType::Tape:       return span (kSatTape);

        case EffectType::Delay:
            return variant == (int) DelayDSP::Type::VocalDoubler
                     ? span (kDelayVocalDoubler)
                     : span (kDelayEcho);

        case EffectType::Overdrive:
            return variant == (int) OverdriveDSP::Type::Pedal
                     ? span (kOverdrivePedal)
                     : span (kOverdriveRack);

        case EffectType::Reverb:                 return span (kReverb);
        case EffectType::Chorus:                 return span (kChorus);
        case EffectType::Flanger:                return span (kFlanger);
        case EffectType::Phaser:                 return span (kPhaser);
        case EffectType::TransientShaper:        return span (kTransientShaper);
        // TS7: variant 1 is Maximizer, which adds the loudness-target and
        // true-peak-target knobs on top of the reproduction's set.
        case EffectType::Limiter:
            return variant == (int) LimiterDSP::Mode::Maximizer
                     ? span (kLimiterMaximizer)
                     : span (kLimiter);
        case EffectType::DeEsser:                return span (kDeEsser);
        case EffectType::Gate:                   return span (kGate);
        case EffectType::DeReverb:               return span (kDeReverb);

        case EffectType::BluesDriveStyle:        return span (kBluesDrive);
        case EffectType::DistortionStyle:        return span (kDistortion);
        case EffectType::FuzzStyle:              return span (kFuzz);
        case EffectType::HighGainStyle:          return span (kHighGain);
        case EffectType::BassDriverStyle:        return span (kBassDriver);
        case EffectType::BassOverdriveStyle:     return span (kBassOverdrive);
        case EffectType::OctaveStyle:            return span (kOctave);
        case EffectType::NoiseGateStyle:         return span (kNoiseGate);
        case EffectType::BassCompressorStyle:    return span (kBassCompressor);
        case EffectType::SynthStyle:             return span (kSynthStyle);
        case EffectType::WahStyle:               return span (kWah);
        case EffectType::AcousticPreampStyle:    return span (kAcousticPreamp);
        case EffectType::AcousticSimulatorStyle: return span (kAcousticSimulator);
        case EffectType::NAMPedalStyle:          return span (kNamPedal);

        // The three EQ pedals and the Tuner drive plain sliders / knob vectors
        // that setSlotContext never stamps, so they have no automation lanes to
        // resolve.  Giving them tables here would invent lanes the UI has no
        // way to create.
        default:                                 return {};
    }
}

const ParamDef* find (EffectType type, int variant, const juce::String& suffix)
{
    for (const auto& d : defsFor (type, variant))
        if (suffix == d.suffix) return &d;
    return nullptr;
}

namespace
{
    void rangeFor (const ParamDef& d, const DSPBase* dsp, float& lo, float& hi)
    {
        lo = d.lo;
        hi = d.hi;
        if (d.rangeOf != nullptr && dsp != nullptr)
            d.rangeOf (dsp, lo, hi);
    }
}

bool applyNatural (EffectType type, int variant, DSPBase* dsp,
                   const juce::String& suffix, float natural)
{
    const auto* d = find (type, variant, suffix);
    if (d == nullptr || dsp == nullptr) return false;
    float lo = 0.0f, hi = 0.0f;
    rangeFor (*d, dsp, lo, hi);
    d->apply (dsp, juce::jlimit (lo, hi, natural));
    return true;
}

bool applyNorm (EffectType type, int variant, DSPBase* dsp,
                const juce::String& suffix, float v01)
{
    const auto* d = find (type, variant, suffix);
    if (d == nullptr || dsp == nullptr) return false;
    // Same range the slider uses, so a lane at 50% and a knob at 50% land on the
    // same DSP value by construction rather than by two implementations agreeing.
    float lo = 0.0f, hi = 0.0f;
    rangeFor (*d, dsp, lo, hi);
    d->apply (dsp, lo + juce::jlimit (0.0f, 1.0f, v01) * (hi - lo));
    return true;
}

float readNorm (EffectType type, int variant, const DSPBase* dsp,
                const juce::String& suffix, float fallback)
{
    const auto* d = find (type, variant, suffix);
    if (d == nullptr || dsp == nullptr) return fallback;
    float lo = 0.0f, hi = 0.0f;
    rangeFor (*d, dsp, lo, hi);
    const float range = hi - lo;
    if (range <= 0.0f) return fallback;
    return juce::jlimit (0.0f, 1.0f, (d->read (dsp) - lo) / range);
}

float readNatural (EffectType type, int variant, const DSPBase* dsp,
                   const juce::String& suffix, float fallback)
{
    const auto* d = find (type, variant, suffix);
    if (d == nullptr || dsp == nullptr) return fallback;
    float lo = 0.0f, hi = 0.0f;
    rangeFor (*d, dsp, lo, hi);
    return juce::jlimit (lo, hi, d->read (dsp));
}
}
