#include "BaySickVocalProcessor.h"
#include "../AudioFileRecorder.h"   // I-16 G-9: wet recorder tap inside processBlock
#include "../TempoMapRead.h"        // QA-Fa recovery: legacy-save origin-sample derivation
#include "BaySickVocalEditor.h"
#include <algorithm>
#include "../DSP/DeEsserDSP.h"
#include "../DSP/CompressorDSP.h"
#include "../DSP/SaturationDSP.h"
#include "../DSP/LimiterDSP.h"
#include "../DSP/GateDSP.h"      // QA-Fe2 slot 0
#include "../DSP/DeReverbDSP.h"  // QA-Fe2 slot 1

// ─────────────────────────────────────────────────────────────────────────────
// BaySickVocalProcessor - Phase H-1 skeleton (2026-05-01)
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    // 2026-05-01: APVTS param ID helper.  All BaySickVocal params live under
    // the bsv_ prefix so they round-trip cleanly through the host parameter
    // tree without colliding with any other engine.
    juce::ParameterID vid (const char* suffix, int versionHint = 1)
    {
        return juce::ParameterID (juce::String ("bsv_") + suffix, versionHint);
    }

    // QA-F Task 3: BaySickAlign params live under bsa_ (still on this same
    // APVTS -- the prefix keeps the two control families greppable apart).
    juce::ParameterID aid (const char* suffix, int versionHint = 1)
    {
        return juce::ParameterID (juce::String ("bsa_") + suffix, versionHint);
    }
}

// ─── APVTS layout ─────────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout
BaySickVocalProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto addF = [&](const char* suffix, const juce::String& name,
                    float lo, float hi, float def)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            vid (suffix), name,
            juce::NormalisableRange<float> (lo, hi),
            def));
    };
    auto addB = [&](const char* suffix, const juce::String& name, bool def)
    {
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            vid (suffix), name, def));
    };
    auto addI = [&](const char* suffix, const juce::String& name,
                    int lo, int hi, int def)
    {
        params.push_back (std::make_unique<juce::AudioParameterInt> (
            vid (suffix), name, lo, hi, def));
    };

    // ── Page-wide controls (BaySickVocals tab top half) ─────────────────────
    // QA-Fd 3a/12b: the page-master Bypass param is RETIRED -- pitch/align
    // edits are never silenced by a page switch (no other player has one;
    // the realtime section has its own toggle).  Old saves carrying
    // bsv_bypass load fine (unknown params are ignored by replaceState).
    addF ("mix",      "Mix",            0.0f, 1.0f, 1.0f);   // global wet/dry
    addI ("ab_slot",  "A/B Slot",       0,    1,   0);       // A/B compare snapshot

    // ── Realtime pitch correction stage (BaySickVocals tab bottom half) ─────
    // Default OFF per spec - user opts in via the bypass toggle.
    addB ("pitch_realtime_bypass",  "Pitch Realtime Bypass",  true);

    // H-5 (2026-05-01) -- Pitch correction knobs.
    // Root: 0..11 (C, C#, D, D#, E, F, F#, G, G#, A, A#, B)
    // Scale (QA-Fd 17b): 0..12 = the piano roll's 13 scales in piano-roll
    // order (PianoRoll.cpp kScaleDefs / PitchCorrectorDSP::Scale).  The old
    // 0..9 order (with a dead Custom slot) is retired; pre-QA-Fd saves with
    // a non-default scale load shifted (accepted -- the corrector was
    // inaudibly broken until 703f06e4 anyway).
    addI ("pitch_key",          "Pitch Root",     0,    11,   0);
    addI ("pitch_scale",        "Pitch Scale",    0,    12,   0);
    // QA-F Task 5 (Call 2a): defaults re-tuned so correction is musical out
    // of the box -- 60 ms glide + 80% pull leaves natural variation instead
    // of the hard-snapped robotic sound at 50 ms / 100%.  Saved projects
    // keep their stored values; only fresh instances see these.
    addF ("pitch_retuneSpeed",  "Retune Speed ms", 0.0f, 100.0f, 60.0f);
    addF ("pitch_strength",     "Pitch Strength",  0.0f, 1.0f,   0.8f);
    addB ("pitch_formantPreserve", "Formant Preserve", false);
    addF ("pitch_humanize",     "Humanize cents",  0.0f, 20.0f,  0.0f);
    addF ("pitch_throatShift",  "Throat Shift semis", -12.0f, 12.0f, 0.0f);

    // ── Vocal Chain stage Bypass placeholders (Vocal Chain tab) ─────────────
    addB ("deesser_bypass",  "De-esser Bypass",  false);
    addB ("comp_bypass",     "Compressor Bypass",false);
    addB ("sat_bypass",      "Saturation Bypass",false);
    addB ("limiter_bypass",  "Limiter Bypass",   false);

    // QA-Fe2 (2026-07-16) -- Gate (slot 0) + De-reverb (slot 1) stages.
    // Gate defaults are fully transparent (threshold at the floor).
    addB ("gate_bypass",       "Gate Bypass",        false);
    addF ("gate_threshold",    "Gate Threshold dB",  -80.f,   0.f,  -80.f);
    addF ("gate_range",        "Gate Range dB",      -80.f,   0.f,  -60.f);
    addF ("gate_attack",       "Gate Attack ms",       0.1f, 100.f,   1.f);
    addF ("gate_hold",         "Gate Hold ms",         0.f,  500.f,  50.f);
    addF ("gate_release",      "Gate Release ms",      5.f, 2000.f, 100.f);
    addB ("dereverb_bypass",   "De-reverb Bypass",   false);
    addF ("dereverb_reduction","De-reverb Reduction",  0.f,  100.f,  50.f);
    addF ("dereverb_tail",     "De-reverb Tail ms",  100.f, 1000.f, 400.f);
    addF ("dereverb_mix",      "De-reverb Mix",        0.f,  100.f, 100.f);

    // H-2 (2026-05-01) -- Compressor stage params.  The compressor is a
    // single CompressorDSP instance with the new Type dropdown selecting
    // between Modern (default), FET (1176-style), and Opto (LA-2A-style).
    // Knob set exposed in H-6's editor adapts to the active Type.
    addI ("comp_type",       "Compressor Type",  0,    2,    0);   // 0=Modern, 1=FET, 2=Opto
    addF ("comp_threshold",  "Compressor Threshold dB", -60.f, 0.f, -12.f);
    // QA-F chain-wiring fix (2026-07-10): ratio + attack widened to the
    // CompressorPanel knob ranges (attachment range-sync; defaults kept).
    addF ("comp_ratio",      "Compressor Ratio",        0.4f,  30.f, 4.f);
    addF ("comp_attack",     "Compressor Attack ms",    0.f,   400.f, 10.f);
    addF ("comp_release",    "Compressor Release ms",   1.f,   4000.f, 100.f);
    addF ("comp_gain",       "Compressor Makeup dB",   -30.f,  30.f, 0.f);
    addF ("comp_knee",       "Compressor Knee dB",      0.f,   18.f, 6.f);
    addF ("comp_mix",        "Compressor Mix",          0.f,   1.f,  1.f);
    addF ("comp_lookahead",  "Compressor Lookahead ms", 0.f,   5.f,  0.f);
    addB ("comp_stereoLink", "Compressor Stereo Link",  true);
    addB ("comp_autoMakeup", "Compressor Auto Makeup",  false);

    // H-7 (2026-05-01) -- Saturation stage params (Type umbrella + Vocal Body
    // + harmonic-routing mode).
    // QA-Layout T4 (L22): range 0..1 clamped Tape=2 back to Console on every
    // per-block APVTS push, so Tape could never stick on the vocal chain.
    // Widened to 0..2; default Console (Jeff's ruling).
    addI ("sat_type",          "Saturation Type",          0, 2, 1);   // 0=Tube, 1=Console, 2=Tape
    addB ("sat_vocalBody",     "Saturation Vocal Body",    false);
    addI ("sat_harmonicsMode", "Saturation Harmonics Mode", 0, 2, 1);  // 0=Lo, 1=Normal, 2=Hi

    // H-3 (2026-05-01) -- De-esser stage params.  QA-F chain-wiring fix
    // (2026-07-10): ranges re-synced to the QA-EffectsReview panel/DSP
    // clamps (a SliderAttachment resets the knob's range from the param, so
    // a narrower param would shrink the knob's reach); defaults unchanged.
    // The legacy Int deesser_mode (0/1) is REMOVED -- it never had a UI and
    // every save carries 0; the continuous modeBlend replaces it.
    addI ("deesser_msMode",      "De-esser M/S",       0,    2,    0);   // 0=Stereo, 1=Mid, 2=Side
    addF ("deesser_freq",        "De-esser Frequency", 4000.f, 12000.f, 6500.f);
    addF ("deesser_q",           "De-esser Q",         0.5f,  4.0f,    1.4f);
    addF ("deesser_threshold",   "De-esser Threshold dB", -80.f, 0.f, -24.f);
    addF ("deesser_range",       "De-esser Range dB", -48.f, 0.f, -12.f);
    addF ("deesser_attack",      "De-esser Attack ms",  0.1f,  30.f,  1.f);
    addF ("deesser_release",     "De-esser Release ms", 10.f, 500.f, 80.f);
    addF ("deesser_lookahead",   "De-esser Lookahead ms", 0.f, 12.f,  0.f);
    addF ("deesser_mix",         "De-esser Mix",        0.f,   1.f,   1.f);
    addB ("deesser_listen",      "De-esser SC Listen",  false);
    addF ("deesser_modeBlend",   "De-esser Mode Blend", 0.f, 100.f,   0.f);  // 0=Wide, 100=Split@4k
    addB ("deesser_spectral",    "De-esser Spectral Engine",      false);
    addB ("deesser_lowlat",      "De-esser Spectral Low Latency", false);

    // QA-F chain-wiring fix (2026-07-10) -- params for the chain panel
    // controls that had none (the panel edits were stomped by the per-block
    // push or, for the limiter, never persisted).  Ranges/defaults mirror
    // the panels exactly.
    addF ("comp_detection",  "Compressor Detection ms",   1.f,  100.f,  10.f);
    addF ("comp_scHpf",      "Compressor SC HPF Hz",     20.f, 2000.f,  20.f);
    addI ("comp_kneeType",   "Compressor Knee Type",      0,    7,      1);
    addB ("comp_peakDet",    "Compressor Peak Detection", false);

    addF ("limiter_inGain",      "Limiter Input Gain dB", -12.f,  24.f,   0.f);
    addF ("limiter_ceiling",     "Limiter Ceiling dB",    -24.f,  12.f,  -0.3f);
    addF ("limiter_satThresh",   "Limiter Sat Threshold",   0.f,   1.f,   1.f);
    addF ("limiter_satCurve",    "Limiter Sat Curve",       0.f,   1.f,   0.5f);
    addF ("limiter_scHpf",       "Limiter SC HPF Hz",      20.f, 2000.f, 20.f);
    addF ("limiter_attack",      "Limiter Attack ms",       0.1f, 20.f,   1.f);
    addF ("limiter_release",     "Limiter Release ms",     10.f, 1000.f, 100.f);
    addF ("limiter_ahead",       "Limiter Lookahead ms",    0.f,  10.f,   2.f);
    addF ("limiter_relCurve",    "Limiter Release Curve",   0.f,   1.f,   0.5f);
    addF ("limiter_sustain",     "Limiter Sustain ms",      0.f, 1000.f,  0.f);
    addB ("limiter_autoRelease", "Limiter Auto Release",  false);
    addB ("limiter_autoMakeup",  "Limiter Auto Makeup",   false);
    addB ("limiter_stereoLink",  "Limiter Stereo Link",   true);
    // QA-ModelShell TS7 maximizer suite.  Every default reproduces the pre-TS7
    // behaviour (character 0 is the old fixed ballistics, both automatic modes
    // off), so a project saved before these existed loads unchanged.
    addF ("limiter_mode",            "Limiter Mode",             0.f,   1.f,  0.f);
    addF ("limiter_character",       "Limiter Character",        0.f,   7.f,  0.f);
    addB ("limiter_loudTargetOn",    "Limiter Loudness Target",  false);
    addF ("limiter_loudTargetLufs",  "Limiter Loudness LUFS",  -30.f,   0.f, -14.f);
    addB ("limiter_autoCeiling",     "Limiter Auto Ceiling",     false);
    addF ("limiter_truePeakTargetDb","Limiter True Peak dBTP",   -6.f,   0.f,  -1.f);

    // ── QA-F Task 3: BaySickAlign (bsa_ prefix; sections 13a/13e) ───────────
    // Offline-only params -- read at action time on the message thread,
    // never pushed per-block.  Defaults = the Close-Align factory preset
    // (mode Close, pitch off).
    auto addAF = [&](const char* suffix, const juce::String& name,
                     float lo, float hi, float def)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            aid (suffix), name, juce::NormalisableRange<float> (lo, hi), def));
    };
    auto addAB = [&](const char* suffix, const juce::String& name, bool def)
    {
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            aid (suffix), name, def));
    };
    auto addAI = [&](const char* suffix, const juce::String& name,
                     int lo, int hi, int def)
    {
        params.push_back (std::make_unique<juce::AudioParameterInt> (
            aid (suffix), name, lo, hi, def));
    };

    addAB ("align_on",        "Align On",            true);
    // QA-Fd 9a/10a: Mode/Fine = residual tightness (base Tight 0 / Close 50
    // / Loose 100 ms, fine +/-50 clamped >= 0); matching window is internal.
    addAI ("align_mode",      "Align Mode",          0, 2, 1);    // 0=Loose, 1=Close, 2=Tight
    // Owner respec 2026-07-11: Fine sweeps the mode's ABSOLUTE residual
    // window (Tight 0-50 / Close 50-150 / Loose 100-200 ms; 0 = window
    // center).  Stored as the normalized bipolar offset.
    addAF ("align_fineTune",  "Align Fine Tune ms",  -50.0f, 50.0f, 0.0f);
    // QA-Fd 15a: per-word Max Shift cap.  (Flexibility picker removed
    // 2026-07-11 -- owner call; DSP runs a fixed Normal 2:1 slope bound.)
    // Owner review 2026-07-11 vs the reference aligner (VocAlign): cap range
    // 10-150 ms; > 150 = No Limit (its usual default).  Old 10-400/def-400
    // saves clamp to 160 = No Limit -- same effective behavior.
    addAF ("align_maxShift",  "Align Max Shift ms",  10.0f, 160.0f, 160.0f);
    addAB ("pitch_on",        "Align Pitch On",      false);
    // QA-Fd 12a: existing knob renamed at display level ("Pitch Blend" --
    // percent toward the leader contour, free 0-100 travel; mode presets the
    // value).  Applied at publish, live, no re-analysis.
    addAF ("pitch_range",     "Align Pitch Blend",   0.0f, 100.0f, 50.0f);
    // QA-Fd 12a: tuning-variation cap in semitones -- deviation inside the
    // cap is untouched, only the excess is pulled (then Blend-scaled).
    addAF ("pitch_variation", "Align Pitch Variation st", 0.0f, 6.0f, 0.0f);
    // QA-Fd 13a: per-side detection bands (kAlignPitchBands order).
    addAI ("pitch_typeGuide",  "Align Pitch Type Leader",   0, 4, 0);
    addAI ("pitch_typeDub",    "Align Pitch Type Follower", 0, 4, 0);
    addAI ("pitch_algo",      "Align Pitch Algo",    0, 2, 0);    // 0=PSOLA, 1=Granular, 2=Phase Vocoder
    addAI ("pitch_transpose", "Align Transpose st",  -12, 12, 0);
    addAB ("formant_on",      "Align Formant On",    false);
    addAF ("formant_shift",   "Align Formant Shift st", -12.0f, 12.0f, 0.0f);
    addAI ("preset",          "Align Preset",        0, 6, 2);    // 6 factory (13a order) + 6=User
    addAB ("preset_dirty",    "Align Preset Dirty",  false);
    addAI ("leader_channel",   "Align Leader Channel",   -1, 999, -1);
    addAI ("follower_channel", "Align Follower Channel", -1, 999, -1);

    // ── QA-Fa: BaySickPitch (bsp_ prefix; sections 14b/14f) ─────────────────
    // Focus DEFAULTS 0 so an analyzed-but-untouched channel plays untouched.
    auto addPF = [&](const char* suffix, const juce::String& name,
                     float lo, float hi, float def)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (juce::String ("bsp_") + suffix, 1), name,
            juce::NormalisableRange<float> (lo, hi), def));
    };
    auto addPB = [&](const char* suffix, const juce::String& name, bool def)
    {
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID (juce::String ("bsp_") + suffix, 1), name, def));
    };
    auto addPI = [&](const char* suffix, const juce::String& name,
                     int lo, int hi, int def)
    {
        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID (juce::String ("bsp_") + suffix, 1), name, lo, hi, def));
    };
    addPF ("focus",        "Pitch Focus",        0.0f, 100.0f, 0.0f);
    addPF ("mod",          "Pitch Mod",          0.0f, 100.0f, 50.0f);
    addPF ("speed",        "Pitch Speed",        0.0f, 100.0f, 50.0f);
    addPF ("throat",       "Pitch Throat",       0.0f, 100.0f, 50.0f);   // QA-Fe Task 6: 50 = neutral
    // QA-Fd 7a: the Loose/Close/Tight preset combo is RETIRED (no reference
    // counterpart; name collision with Align's modes) -- bsp_preset +
    // bsp_preset_dirty removed; the knobs stand alone.
    // QA-Fd 8/14a-c: editor Root/Scale/Snap (13-scale shared table; Snap ON
    // = Focus targets the scale + vertical drags land on in-scale lanes).
    // Intentionally independent from the realtime board's Root/Scale.
    addPI ("root",         "Pitch Edit Root",    0, 11, 0);
    addPI ("scale",        "Pitch Edit Scale",   0, 12, 0);
    addPB ("snap",         "Pitch Edit Snap",    false);
    addPI ("mode",         "Pitch Edit Mode",    0, 1, 1);   // 0=Slice, 1=Edit
    addPB ("on",           "Pitch Chain On",     true);      // QA-Fa recovery: chain switch
    addPI ("engine",       "Pitch Engine",       0, 2, 0);   // QA-Fe: 0=Rubber Band, 1=Signalsmith, 2=WORLD

    return { params.begin(), params.end() };
}

// ─── A/B compare snapshot parameter set ───────────────────────────────────────
// The two `bsv_ab_slot` slots each remember these values; everything else in
// the tree is shared between them.  Lives next to createLayout deliberately --
// a param added above without a line here is a param the A/B flip forgets.
//
// Deliberately absent: `bsv_ab_slot` (the selector cannot live inside its own
// snapshot), `bsv_deesser_listen` (the de-esser's sidechain-audition monitor --
// solo-class state, not tone), and the bsa_/bsp_ families, which are the
// offline Align/Pitch editors' per-channel edit settings.
namespace
{
    static const char* const kAbSnapshotParams[] =
    {
        "bsv_mix",

        "bsv_pitch_realtime_bypass",
        "bsv_pitch_key",
        "bsv_pitch_scale",
        "bsv_pitch_retuneSpeed",
        "bsv_pitch_strength",
        "bsv_pitch_formantPreserve",
        "bsv_pitch_humanize",
        "bsv_pitch_throatShift",

        "bsv_gate_bypass",
        "bsv_gate_threshold",
        "bsv_gate_range",
        "bsv_gate_attack",
        "bsv_gate_hold",
        "bsv_gate_release",

        "bsv_dereverb_bypass",
        "bsv_dereverb_reduction",
        "bsv_dereverb_tail",
        "bsv_dereverb_mix",

        "bsv_deesser_bypass",
        "bsv_deesser_msMode",
        "bsv_deesser_freq",
        "bsv_deesser_q",
        "bsv_deesser_threshold",
        "bsv_deesser_range",
        "bsv_deesser_attack",
        "bsv_deesser_release",
        "bsv_deesser_lookahead",
        "bsv_deesser_mix",
        "bsv_deesser_modeBlend",
        "bsv_deesser_spectral",
        "bsv_deesser_lowlat",

        "bsv_comp_bypass",
        "bsv_comp_type",
        "bsv_comp_threshold",
        "bsv_comp_ratio",
        "bsv_comp_attack",
        "bsv_comp_release",
        "bsv_comp_gain",
        "bsv_comp_knee",
        "bsv_comp_mix",
        "bsv_comp_lookahead",
        "bsv_comp_stereoLink",
        "bsv_comp_autoMakeup",
        "bsv_comp_detection",
        "bsv_comp_scHpf",
        "bsv_comp_kneeType",
        "bsv_comp_peakDet",

        "bsv_sat_bypass",
        "bsv_sat_type",
        "bsv_sat_vocalBody",
        "bsv_sat_harmonicsMode",

        "bsv_limiter_bypass",
        "bsv_limiter_inGain",
        "bsv_limiter_ceiling",
        "bsv_limiter_satThresh",
        "bsv_limiter_satCurve",
        "bsv_limiter_scHpf",
        "bsv_limiter_attack",
        "bsv_limiter_release",
        "bsv_limiter_ahead",
        "bsv_limiter_relCurve",
        "bsv_limiter_sustain",
        "bsv_limiter_autoRelease",
        "bsv_limiter_autoMakeup",
        "bsv_limiter_stereoLink",
        "bsv_limiter_mode",
        "bsv_limiter_character",
        "bsv_limiter_loudTargetOn",
        "bsv_limiter_loudTargetLufs",
        "bsv_limiter_autoCeiling",
        "bsv_limiter_truePeakTargetDb",
    };

    constexpr int kNumAbSnapshotParams
        = (int) (sizeof (kAbSnapshotParams) / sizeof (kAbSnapshotParams[0]));
}

// ─── Constructor ──────────────────────────────────────────────────────────────
BaySickVocalProcessor::BaySickVocalProcessor (juce::UndoManager* undoMgr)
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    , apvts (*this, undoMgr, "BaySickVocalState", createLayout())
{
    // H-6d (2026-05-02): per-Vox-strip BaySickNAM/IR processor lives on
    // the Vox processor so its state is captured by getStateInformation
    // (page preset save/load picks it up automatically).
    mNamIrProc = std::make_unique<BaySickNAMIRProcessor> (undoMgr);

    // 2026-05-05 dirty-flag wiring: the vocal-chain rack's slot lifecycle
    // (load/clear/move/bypass) doesn't write to apvts, so chain it manually
    // into the engine's dirty hook the same path apvts edits use.
    mVocalChainRack.onSlotsChanged = [this]
    {
        // mDirtyTracker.onAny is the editor-installed markDirty closure;
        // safe to call even when null (handled inside the lambda we set).
        if (auto& fn = mDirtyTracker.onAny) fn();
    };

    // QA-Fa recovery: the decode layer reads the align chain switch + Pitch
    // box gates per block; cache the raw-param atomics once (APVTS-stable).
    mAlignOnRaw        = apvts.getRawParameterValue ("bsa_align_on");
    mAlignPitchOnRaw   = apvts.getRawParameterValue ("bsa_pitch_on");
    mAlignTransposeRaw = apvts.getRawParameterValue ("bsa_pitch_transpose");

    // A/B compare: parameterChanged captures the outgoing slot then restores
    // the incoming one.  Both slots start holding the current values, so the
    // first flip is inaudible until one side has been edited.
    apvts.addParameterListener ("bsv_ab_slot", this);
    mLastSlot = activeAbSlot();
    captureSnapshotFromCurrent (0);
    captureSnapshotFromCurrent (1);
}

// ─── Destructor ───────────────────────────────────────────────────────────────
// 2026-05-06 (Batch 9c N1): audio-thread shutdown safety net.  Owners should
// ideally pre-flag teardown via setShuttingDown(true) and then wait out the
// in-flight block via BaySickDAWProcessor::settleAudioThread() (which waits for
// two acknowledged block boundaries -- a fixed sleep cannot be correct at every
// buffer size) so the audio thread sees the flag before any member starts
// dying.  Setting it here as well so we still bail
// on subsequent processBlock entries even if the owner forgot.  Without this,
// the audio thread's mNamIrProc->processBlock(...) dispatches through a
// vtable already zeroed by ~BaySickNAMIRProcessor and crashes.
BaySickVocalProcessor::~BaySickVocalProcessor()
{
    mShuttingDown.store (true, std::memory_order_release);
    // Dropped before the listener is removed: an A/B swap already sitting in
    // the message queue holds a weak copy of this token and bails once it
    // expires (callAsync cannot be retracted).
    mLife.reset();
    apvts.removeParameterListener ("bsv_ab_slot", this);
    // QA-Fa recovery: owners raise the shutdown gate (and the clip snapshot
    // rebuild drops the channel's players) before teardown, so the decode
    // layer cannot be inside a block holding this pointer here.
    delete mAlignPlayActive.exchange (nullptr);
    delete mPitchPreview.exchange (nullptr);
}

// ─── Audio lifecycle ──────────────────────────────────────────────────────────
void BaySickVocalProcessor::prepareToPlay (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;
    mPrepared   = true;

    // H-6 (2026-05-01) -- prepare pitch corrector + vocal chain rack.
    mPitchCorrector.prepare (sampleRate, maxBlockSize);

    // QA-F Task 3: align engine (offline; prepare just records the rate).
    mAlign.prepare (sampleRate, maxBlockSize);

    // QA-Fa: pitch engine (offline analysis + the FilePlay applicator).
    mPitch.prepare (sampleRate, maxBlockSize);

    // H-6c / QA-Fe2 -- pre-load the vocal chain rack, now 6 locked slots:
    // [0] Gate [1] De-reverb [2] De-esser [3] Compressor [4] Saturation
    // [5] Limiter (de-reverb ahead of compression so the tail can't be
    // pumped back up).  loadEffect is idempotent on type match; second
    // prepareToPlay calls (e.g. sample-rate change) just re-prepare.
    if (mVocalChainRack.getSlotType (0) != EffectType::Gate)
        mVocalChainRack.loadEffect (0, EffectType::Gate);
    if (mVocalChainRack.getSlotType (1) != EffectType::DeReverb)
        mVocalChainRack.loadEffect (1, EffectType::DeReverb);
    if (mVocalChainRack.getSlotType (2) != EffectType::DeEsser)
        mVocalChainRack.loadEffect (2, EffectType::DeEsser);
    if (mVocalChainRack.getSlotType (3) != EffectType::Compressor)
        mVocalChainRack.loadEffect (3, EffectType::Compressor);
    if (mVocalChainRack.getSlotType (4) != EffectType::Saturation)
        mVocalChainRack.loadEffect (4, EffectType::Saturation);
    if (mVocalChainRack.getSlotType (5) != EffectType::Limiter)
        mVocalChainRack.loadEffect (5, EffectType::Limiter);
    mVocalChainRack.prepare (sampleRate, maxBlockSize);

    mDryScratch.setSize (2, maxBlockSize, false, false, true);
    mDryScratch.clear();
    mWetMonoScratch.setSize (1, maxBlockSize, false, false, true);
    mWetMonoScratch.clear();

    // QA-Fe2: De-noise learners (raw + corrector-domain).  prepareToPlay is
    // never concurrent with processBlock, so the ring realloc is safe here.
    mDenoiseRawLearner.prepare (sampleRate);
    mDenoiseWetLearner.prepare (sampleRate);
    mDenoiseMonoScratch.assign ((size_t) juce::jmax (16, maxBlockSize), 0.0f);

    // QA-Fe2 docket 2a: low-latency corrected monitor path.
    mMonitorShifter.prepare (sampleRate, maxBlockSize);
    mMonShiftScratch.assign ((size_t) juce::jmax (16, maxBlockSize), 0.0f);
    mMonShiftFadeStep = 1.0f / juce::jmax (1.0f, (float) (0.04 * sampleRate));
    mMonShiftFade      = 0.0f;
    mMonShiftWasActive = false;
    mDenoisePump.onTick = [this]
    {
        mDenoiseRawLearner.processPending();
        mDenoiseWetLearner.processPending();
    };

    mMonXfadeLen    = juce::jmax (1, (int) (sampleRate * 0.010));   // ~10 ms de-click
    mMonXfadeRemain = 0;
    mMonLastMode    = -1;

    // H-6d: prepare the embedded NAM/IR processor so its DSP is ready when
    // G-9 routes audio through it.
    if (mNamIrProc)
        mNamIrProc->prepareToPlay (sampleRate, maxBlockSize);
}

// H-6c (2026-05-01) -- pull APVTS values once per block + push to each DSP
// stage's setters.  DSPs are now owned by mVocalChainRack; we dynamic_cast
// each slot's effect pointer to the concrete type to call its setters.
void BaySickVocalProcessor::pushApvtsToDsp() noexcept
{
    auto rd = [this](const char* id) -> float {
        if (auto* p = apvts.getRawParameterValue (id)) return p->load();
        return 0.0f;
    };
    auto rdb = [this](const char* id) -> bool {
        if (auto* p = apvts.getRawParameterValue (id)) return p->load() > 0.5f;
        return false;
    };
    auto rdi = [this](const char* id) -> int {
        if (auto* p = apvts.getRawParameterValue (id)) return (int) p->load();
        return 0;
    };

    // ── Pitch correction (outside the rack) ────────────────────────────────
    mPitchCorrector.bypassed = rdb ("bsv_pitch_realtime_bypass");
    mPitchCorrector.setKey               (rdi ("bsv_pitch_key"));
    mPitchCorrector.setScale             (rdi ("bsv_pitch_scale"));
    mPitchCorrector.setRetuneSpeedMs     (rd  ("bsv_pitch_retuneSpeed"));
    mPitchCorrector.setStrength          (rd  ("bsv_pitch_strength"));
    mPitchCorrector.setFormantPreserve   (rdb ("bsv_pitch_formantPreserve"));
    mPitchCorrector.setHumanizeCents     (rd  ("bsv_pitch_humanize"));
    mPitchCorrector.setThroatShiftSemis  (rd  ("bsv_pitch_throatShift"));

    // ── QA-Fa: BaySickPitch applicator knobs (CPU-guarded setters) ─────────
    // Speed knob is "higher = faster": 0 -> 300 ms glide, 100 -> 5 ms.
    mPitch.setFocus01   (rd ("bsp_focus") / 100.0f);
    mPitch.setModAmount (rd ("bsp_mod")   / 50.0f);
    mPitch.setSpeedMs   (300.0f - rd ("bsp_speed") * 2.95f);
    mPitch.setThroat    ((rd ("bsp_throat") - 50.0f) * (12.0f / 50.0f));   // QA-Fe Task 6: 0..100 -> -12..+12 semis
    mPitch.setChainOn   (rdb ("bsp_on"));
    // QA-Fd 14b: the Focus snap target set (editor Root/Scale/Snap).
    mPitch.setRoot      (rdi ("bsp_root"));
    mPitch.setScaleIdx  (rdi ("bsp_scale"));
    mPitch.setSnapOn    (rdb ("bsp_snap"));
    mPitch.setEngine    ((PitchEngine) juce::jlimit (0, 2, rdi ("bsp_engine")));

    // ── Rack stage Bypass flags (forward to slots; QA-Fe2 order 0..5) ─────
    mVocalChainRack.setSlotBypassed (0, rdb ("bsv_gate_bypass"));
    mVocalChainRack.setSlotBypassed (1, rdb ("bsv_dereverb_bypass"));
    mVocalChainRack.setSlotBypassed (2, rdb ("bsv_deesser_bypass"));
    mVocalChainRack.setSlotBypassed (3, rdb ("bsv_comp_bypass"));
    mVocalChainRack.setSlotBypassed (4, rdb ("bsv_sat_bypass"));
    mVocalChainRack.setSlotBypassed (5, rdb ("bsv_limiter_bypass"));

    // ── Gate (slot 0, QA-Fe2) ─────────────────────────────────────────────
    if (auto* gt = dynamic_cast<GateDSP*> (mVocalChainRack.getSlotEffect (0)))
    {
        gt->setThresholdDb (rd ("bsv_gate_threshold"));
        gt->setRangeDb     (rd ("bsv_gate_range"));
        gt->setAttackMs    (rd ("bsv_gate_attack"));
        gt->setHoldMs      (rd ("bsv_gate_hold"));
        gt->setReleaseMs   (rd ("bsv_gate_release"));
    }

    // ── De-reverb (slot 1, QA-Fe2) ────────────────────────────────────────
    if (auto* dr = dynamic_cast<DeReverbDSP*> (mVocalChainRack.getSlotEffect (1)))
    {
        dr->setReductionPct (rd ("bsv_dereverb_reduction"));
        dr->setTailMs       (rd ("bsv_dereverb_tail"));
        dr->setMixPct       (rd ("bsv_dereverb_mix"));
    }

    // ── De-esser (slot 2) ──────────────────────────────────────────────────
    // QA-F chain-wiring fix (2026-07-10): modeBlend replaces the removed
    // legacy Int mode; the new engine/quality params ride the same push.
    if (auto* de = dynamic_cast<DeEsserDSP*> (mVocalChainRack.getSlotEffect (2)))
    {
        de->setModeBlend    (rd  ("bsv_deesser_modeBlend"));
        de->setMsMode       (rdi ("bsv_deesser_msMode"));
        de->setFrequencyHz  (rd  ("bsv_deesser_freq"));
        de->setQ            (rd  ("bsv_deesser_q"));
        de->setThresholdDb  (rd  ("bsv_deesser_threshold"));
        de->setRangeDb      (rd  ("bsv_deesser_range"));
        de->setAttackMs     (rd  ("bsv_deesser_attack"));
        de->setReleaseMs    (rd  ("bsv_deesser_release"));
        de->setLookaheadMs  (rd  ("bsv_deesser_lookahead"));
        de->setMix          (rd  ("bsv_deesser_mix"));
        de->setListen       (rdb ("bsv_deesser_listen"));
        de->setEngine          (rdb ("bsv_deesser_spectral") ? 1 : 0);
        de->setSpectralQuality (rdb ("bsv_deesser_lowlat")   ? 1 : 0);
    }

    // ── Compressor (slot 3) ────────────────────────────────────────────────
    if (auto* cp = dynamic_cast<CompressorDSP*> (mVocalChainRack.getSlotEffect (3)))
    {
        cp->setType        (rdi ("bsv_comp_type"));
        cp->setThreshold   (rd  ("bsv_comp_threshold"));
        cp->setRatio       (rd  ("bsv_comp_ratio"));
        cp->setAttack      (rd  ("bsv_comp_attack"));
        cp->setRelease     (rd  ("bsv_comp_release"));
        cp->setGain        (rd  ("bsv_comp_gain"));
        cp->setKnee        (rd  ("bsv_comp_knee"));
        cp->setMix         (rd  ("bsv_comp_mix"));
        cp->setLookaheadMs (rd  ("bsv_comp_lookahead"));
        cp->setStereoLink  (rdb ("bsv_comp_stereoLink"));
        cp->setAutoMakeup  (rdb ("bsv_comp_autoMakeup"));
        // QA-F chain-wiring fix (2026-07-10): the panel controls that had no
        // params behind them.
        cp->setDetectionMs   (rd  ("bsv_comp_detection"));
        cp->setSidechainHPF  (rd  ("bsv_comp_scHpf"));
        cp->setKneeType      (rdi ("bsv_comp_kneeType"));
        cp->setPeakDetection (rdb ("bsv_comp_peakDet"));
    }

    // ── Saturation (slot 4) ────────────────────────────────────────────────
    // H-7 (2026-05-01): Type umbrella + Vocal Body.  Other knobs (drive,
    // tube type, etc.) still run at defaults until a polish pass exposes
    // them in the BaySickVocal UI; the existing rack panel reads them too.
    if (auto* sat = dynamic_cast<SaturationDSP*> (mVocalChainRack.getSlotEffect (4)))
    {
        sat->setSatType        (rdi ("bsv_sat_type"));
        sat->setVocalBody      (rdb ("bsv_sat_vocalBody"));
        sat->setHarmonicsMode  (rdi ("bsv_sat_harmonicsMode"));
    }

    // ── Limiter (slot 5) ───────────────────────────────────────────────────
    // QA-F chain-wiring fix (2026-07-10): full knob push (was "polish pass"
    // deferred -- the panel's edits neither persisted nor had params).
    if (auto* lim = dynamic_cast<LimiterDSP*> (mVocalChainRack.getSlotEffect (5)))
    {
        lim->setInputGainDb  (rd  ("bsv_limiter_inGain"));
        lim->setCeilingDb    (rd  ("bsv_limiter_ceiling"));
        lim->setSatThresh    (rd  ("bsv_limiter_satThresh"));
        lim->setSatCurve     (rd  ("bsv_limiter_satCurve"));
        lim->setSidechainHPF (rd  ("bsv_limiter_scHpf"));
        lim->setAttackMs     (rd  ("bsv_limiter_attack"));
        lim->setReleaseMs    (rd  ("bsv_limiter_release"));
        lim->setAheadMs      (rd  ("bsv_limiter_ahead"));
        lim->setReleaseCurve (rd  ("bsv_limiter_relCurve"));
        lim->setSustainMs    (rd  ("bsv_limiter_sustain"));
        lim->setAutoRelease  (rdb ("bsv_limiter_autoRelease"));
        lim->setAutoMakeup   (rdb ("bsv_limiter_autoMakeup"));
        lim->setStereoLink   (rdb ("bsv_limiter_stereoLink"));
        // TS7 maximizer suite.  Each setter is change-guarded, so re-applying the
        // same value every block costs a compare (standing CPU-safeguard rule).
        lim->setMode                (((int) std::lround (rd ("bsv_limiter_mode")) == 1)
                                       ? LimiterDSP::Mode::Maximizer
                                       : LimiterDSP::Mode::Limiter);
        lim->setCharacterIndex      ((int) std::lround (rd ("bsv_limiter_character")));
        lim->setLoudnessTargetOn    (rdb ("bsv_limiter_loudTargetOn"));
        lim->setLoudnessTargetLufs  (rd  ("bsv_limiter_loudTargetLufs"));
        lim->setAutoCeiling         (rdb ("bsv_limiter_autoCeiling"));
        lim->setTruePeakTargetDb    (rd  ("bsv_limiter_truePeakTargetDb"));
    }
}

void BaySickVocalProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // QA-Fb Option A: consume the block-scoped monitor-merge params FIRST so
    // an early-out path (empty block / shutdown / master bypass) can never
    // leave a stale task-scratch pointer armed for a later block.
    juce::AudioBuffer<float>* monTakes    = mMonitorMerge;
    const juce::int64         monTimeline = mMonitorMergeTimeline;
    const bool                monMuteLive = mMonitorMuteLive;
    mMonitorMerge    = nullptr;
    mMonitorMuteLive = false;

    if (numSamples <= 0 || numChannels <= 0) return;

    // 2026-05-06 (Batch 9c N1): audio-thread shutdown gate.  Bail BEFORE
    // touching any members (apvts, mScHelper, mNamIrProc, mVocalChainRack)
    // so a teardown-in-progress doesn't race a half-destroyed member access.
    // Mirrors BaySickDAWProcessor::mProjectLoadInProgress check at the top
    // of its processBlock.
    if (mShuttingDown.load (std::memory_order_acquire))
    {
        buffer.clear();
        return;
    }

    // QA-Fd 3a/12b: the master-bypass early return is deleted with the
    // bsv_bypass param -- the chain always runs, so pitch/align edits can
    // never be silently eaten by a page switch.
    mScHelper.updateLevel (numSamples);

    // H-6 (2026-05-01) -- push APVTS to DSP stages once per block.
    pushApvtsToDsp();

    // QA-Fe Task 5: source mux + live-monitor mode, needed before the dry stash.
    const bool filePlaySrc = mForcePitchBypass.load (std::memory_order_acquire);
    const int  monMode     = mMonitorMode.load (std::memory_order_acquire);   // 0 TrueDry / 1 BypassCorr / 2 WithEffect

    // Monitor-swap de-click: detect a mode change and arm the ~10 ms crossfade
    // (live monitoring only).  monFrom + xRem drive the blend at BOTH monitor
    // application points below; mMonXfadeRemain advances once, at block end.
    int monFrom = monMode, xRem = 0;
    if (! filePlaySrc)
    {
        if (mMonLastMode >= 0 && monMode != mMonLastMode)
        { mMonXfadeFrom = mMonLastMode; mMonXfadeRemain = mMonXfadeLen; }
        mMonLastMode = monMode;
        monFrom = mMonXfadeFrom;
        xRem    = mMonXfadeRemain;
    }
    else { mMonLastMode = -1; mMonXfadeRemain = 0; }

    // Stash dry copy for the global Mix wet/dry crossfade.
    const float mix = juce::jlimit (0.0f, 1.0f,
        apvts.getRawParameterValue ("bsv_mix")->load());
    const bool needDry = (mix < 0.999f);
    if (needDry)
    {
        if (mDryScratch.getNumChannels() < numChannels
            || mDryScratch.getNumSamples() < numSamples)
            mDryScratch.setSize (numChannels, numSamples, false, false, true);
        for (int ch = 0; ch < numChannels; ++ch)
            mDryScratch.copyFrom (ch, 0, buffer, ch, 0, numSamples);
        // True Dry re-adds the raw live voice AFTER the Mix, so it must not also
        // ride the wet/dry crossfade here (that would double the live signal).
        if (! filePlaySrc && monMode == 0)
            mDryScratch.clear();
    }

    // QA-Fe Task 5: stash the raw live input before the corrector.  True Dry (0)
    // and Bypass Pitch Corrector (1) route this raw voice to the monitor; the
    // corrector still runs below so the WET tap captures the corrected stream.
    // Always stash for live so the de-click crossfade can read the raw voice even
    // on a swap INTO With-Effect (mode 2, which by itself would not need it).
    const bool needMonitorLive = (! filePlaySrc);
    if (needMonitorLive)
    {
        if (mMonitorLiveDry.getNumChannels() < numChannels
            || mMonitorLiveDry.getNumSamples() < numSamples)
            mMonitorLiveDry.setSize (numChannels, numSamples, false, false, true);
        for (int ch = 0; ch < numChannels; ++ch)
            mMonitorLiveDry.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        // QA-Fe2 raw-domain De-noise learner: fold the pre-corrector live
        // voice to mono and feed the ring (wait-free; FFT work runs on the
        // pump timer).  Digital-silence frames are skipped learner-side, so
        // an assigned-but-idle strip never poisons the room profile.
        if (mDenoiseLearnEnabled.load (std::memory_order_acquire)
            && numSamples <= (int) mDenoiseMonoScratch.size())
        {
            const float invCh = (numChannels > 1) ? (1.0f / (float) numChannels) : 1.0f;
            for (int i = 0; i < numSamples; ++i)
            {
                float s = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                    s += buffer.getReadPointer (ch)[i];
                mDenoiseMonoScratch[(size_t) i] = s * invCh;
            }
            mDenoiseRawLearner.pushBlock (mDenoiseMonoScratch.data(), numSamples);
        }
    }

    // ── Run the locked vocal chain in order ──────────────────────────────
    // input -> pitch correction -> [WET TAP] -> [monitor split] -> [rack:
    //       deess->comp->sat->lim] -> NAM/IR -> output
    // I-16 G-9 (2026-05-03): pitch correction skipped when the source mux is
    // FilePlay (force-bypass) so a wet file with realtime pitch already
    // committed at record time doesn't get corrected twice.  NAM/IR routing
    // also added in G-9.
    if (! filePlaySrc)
        mPitchCorrector.process (buffer);
    else
        // QA-Fa Mode C applicator: FilePlay only (mutually exclusive with the
        // realtime corrector) -- note-level edits ride normal playback with
        // no bake step.  One atomic load when the channel has no edits.
        // QA-Fd: the source stamp resolves pills where the (align/time-map)
        // warped audio actually came from.
        mPitch.processFilePlay (buffer,
            mFilePlayTimelineSample.load (std::memory_order_relaxed),
            mFilePlaySrcX0.load (std::memory_order_relaxed),
            mFilePlaySrcRate.load (std::memory_order_relaxed),
            mFilePlaySrcValid.load (std::memory_order_relaxed));

    // ── Wet recording tap (post-realtime pitch / pre-vocal-chain) ────────
    // This is the user-locked tap point per Option C of G-9.1: the file
    // captures realtime-pitch-applied audio so playback bypasses realtime
    // (single pass) while everything from the vocal chain rack onwards
    // stays dynamic across sessions.
    // QA-Fb bleed gate (G2-3b): capture-eligible = live-source blocks only.
    // The force-bypass flag IS the live/FilePlay source-mux discriminator
    // (its only writers: VoxStripTask live path = false,
    // finalizeFilePlayStrip = true).  FilePlay decode is prior-take playback
    // and must never land in a take's WET file.
    if (! filePlaySrc)
    {
        // QA-Fe2 corrector-domain De-noise learner: the WET take's noise
        // floor is corrector-transformed, so it gets its own profile learned
        // at this same tap point -- during monitoring too, not just capture.
        if (mDenoiseLearnEnabled.load (std::memory_order_acquire)
            && numSamples <= (int) mDenoiseMonoScratch.size())
        {
            const float invCh = (numChannels > 1) ? (1.0f / (float) numChannels) : 1.0f;
            for (int i = 0; i < numSamples; ++i)
            {
                float s = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                    s += buffer.getReadPointer (ch)[i];
                mDenoiseMonoScratch[(size_t) i] = s * invCh;
            }
            mDenoiseWetLearner.pushBlock (mDenoiseMonoScratch.data(), numSamples);
        }

        // The DRY tap is gated against offline blocks in tapDryRecorder, so the
        // WET write has to be too: freezing a Vox tab runs this strip through
        // the render, and a WET take that includes the render's blocks while
        // its DRY partner excludes them drifts by the render's length -- which
        // breaks the aligned pair the Vox page is built on.
        //
        // The gate wraps the ARM-EDGE detector as well as the write.  Nulling
        // wetRec for the duration instead would read as a disarm on the way
        // into the render and a re-arm on the way out, re-priming
        // mWetLatencySkip mid-take and shifting the rest of the take by the
        // corrector's latency.
        if (! isNonRealtime())
        {
        auto* wetRec = mWetRecorder.load (std::memory_order_acquire);
        // QA-Fe Task 4: on the arm edge, prime the latency skip so the recorded
        // WET take aligns to the record-arm moment (the LiveShifter delays its
        // output by getLatencySamples()).
        if (wetRec != mPrevWetRecorder)
        {
            mWetLatencySkip  = (wetRec != nullptr) ? mPitchCorrector.getLatencySamples() : 0;
            mPrevWetRecorder = wetRec;
        }
        if (wetRec != nullptr)
        {
            // Sum to mono for the recorder (the source is a mono ASIO channel
            // duplicated to L=R, and realtime correction is mono -> the channels
            // are identical; the sum is the recorder's mono format).
            // QA-Fb: member scratch + non-owning view -- the old per-block
            // AudioBuffer construction allocated on the audio thread every
            // block while recording.  Lazy-grow guarded like mDryScratch.
            const float invCh = (numChannels > 1) ? (1.0f / (float) numChannels) : 1.0f;
            if (mWetMonoScratch.getNumChannels() < 1
                || mWetMonoScratch.getNumSamples() < numSamples)
                mWetMonoScratch.setSize (1, numSamples, false, false, true);
            float* dst = mWetMonoScratch.getWritePointer (0);
            for (int i = 0; i < numSamples; ++i)
            {
                float s = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                    s += buffer.getReadPointer (ch)[i];
                dst[i] = s * invCh;
            }
            // Drop the leading latency samples (pre-roll / primed silence) so the
            // written take starts at the record-arm moment, matching the DRY take.
            int off = 0;
            if (mWetLatencySkip > 0)
            {
                off = juce::jmin (mWetLatencySkip, numSamples);
                mWetLatencySkip -= off;
            }
            const int toWrite = numSamples - off;
            if (toWrite > 0)
            {
                float* monoPtrs[1] = { dst + off };
                juce::AudioBuffer<float> monoView (monoPtrs, 1, toWrite);
                wetRec->writeBlock (monoView);
            }
        }
        }
    }

    // ── QA-Fe2 docket 2a: low-latency corrected monitor stage ─────────────────
    // While realtime correction is live, the With-Effect monitor/mix target
    // below swaps the R3 stream (~48 ms) for a dual-tap time-domain shift of
    // the RAW voice at the same smoothed correction ratio (~12 ms).  The R3
    // stream already fed the WET recorder above, so recordings keep R3
    // quality; only what the performer (and the live mix) hears changes.
    // ~40 ms fade on the correction-toggle edges; the shifter cold-restarts
    // on engage (ring primes silent under the fade).
    const float* tdMon = nullptr;
    if (! filePlaySrc)
    {
        const bool rtActive = ! mPitchCorrector.bypassed;
        if (rtActive && ! mMonShiftWasActive)
            mMonitorShifter.reset();
        mMonShiftWasActive = rtActive;

        if ((rtActive || mMonShiftFade > 0.0f)
            && (int) mMonShiftScratch.size() >= numSamples
            && mMonitorLiveDry.getNumChannels() > 0)
        {
            // The live source is a mono mic duplicated L=R (recorder-contract
            // comment above) -- shift channel 0 only.
            const float* raw0 = mMonitorLiveDry.getReadPointer (0);
            std::copy (raw0, raw0 + numSamples, mMonShiftScratch.data());
            const float ratio = std::pow (2.0f,
                mPitchCorrector.getCurrentShiftCents() / 1200.0f);
            mMonitorShifter.process (mMonShiftScratch.data(), numSamples, ratio);
            tdMon = mMonShiftScratch.data();
        }
    }

    // ── QA-Fe Task 5: live-monitor split (after the WET tap, so recording stays
    // corrected regardless of what the monitor hears) ────────────────────────
    //   1 Bypass Pitch Corrector -> monitor the RAW live voice through the chain
    //   0 True Dry              -> pull the live voice out of the chain here; it
    //                             is re-added raw AFTER the Mix (below)
    //   2 With Effect          -> corrected voice in the chain (R3 stream, or
    //                             the low-latency monitor shift when live)
    if (! filePlaySrc)
    {
        // De-click crossfade of the monitor SOURCE.  target(mode): 2 = corrected
        // (the incoming buffer), 1 = raw live, 0 = silence (True Dry re-adds the
        // raw voice post-Mix below).  xRem == 0 => w == 1 => steady new mode.
        const float fadeStart = mMonShiftFade;
        const float fadeDir   = mMonShiftWasActive ? 1.0f : -1.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float*       b   = buffer.getWritePointer (ch);
            const float* raw = mMonitorLiveDry.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                float corrected = b[i];
                if (tdMon != nullptr)
                {
                    // Substitution fade is a pure function of i so every
                    // channel blends identically; the member advances once
                    // below.
                    const float f = juce::jlimit (0.0f, 1.0f,
                        fadeStart + fadeDir * mMonShiftFadeStep * (float) i);
                    corrected += f * (tdMon[i] - corrected);
                }
                auto tgt = [&] (int m) noexcept { return m == 2 ? corrected : (m == 1 ? raw[i] : 0.0f); };
                const int   r = juce::jmax (0, xRem - i);
                const float w = 1.0f - (float) r / (float) mMonXfadeLen;
                b[i] = tgt (monFrom) * (1.0f - w) + tgt (monMode) * w;
            }
        }
        mMonShiftFade = juce::jlimit (0.0f, 1.0f,
            fadeStart + fadeDir * mMonShiftFadeStep * (float) numSamples);
    }

    // ── QA-Fd preview play (scrub-audition / sub-editor Play) ─────────────
    // Streams the message-thread-rendered span into the chain pre-rack so
    // it is voiced exactly like playback.  One atomic load when idle.
    if (auto* pv = mPitchPreview.load (std::memory_order_acquire))
    {
        const int len = pv->buf.getNumSamples();
        int cur = pv->cursor.load (std::memory_order_relaxed);
        if (len > 0 && (pv->loop || cur < len))
        {
            int written = 0;
            while (written < numSamples && (pv->loop || cur < len))
            {
                if (cur >= len) cur = 0;   // loop wrap
                const int n = juce::jmin (numSamples - written, len - cur);
                for (int ch = 0; ch < numChannels; ++ch)
                    buffer.addFrom (ch, written, pv->buf, 0, cur, n);
                cur     += n;
                written += n;
                if (! pv->loop && cur >= len) break;
            }
            pv->cursor.store (cur, std::memory_order_relaxed);
        }
    }

    // ── QA-Fb Option A monitor merge (locked 2026-07-10) ─────────────────
    // Prior takes join the chain AFTER the corrector + WET tap (capture =
    // live only, bleed impossible by construction) and BEFORE the rack, so
    // deesser/comp/sat/limiter + NAM process the summed vocal stack exactly
    // like post-stop playback (finalizeFilePlayStrip runs the same sum
    // through this same single chain).  A1: the monitor-stream applicator
    // applies the takes' note edits first -- the realtime corrector occupies
    // the main pitch stage for the live stream.
    if (monTakes != nullptr || monMuteLive)
    {
        if (monMuteLive)
        {
            buffer.clear();                     // captured, not monitored
            if (needDry)
                mDryScratch.clear();            // live must not re-enter via Mix
        }
        if (monTakes != nullptr)
        {
            const int nc = juce::jmin (numChannels, monTakes->getNumChannels());
            // Dry reference gains the RAW takes (pre-applicator) -- matches
            // pure playback, where the dry stash is captured before the
            // pitch stage.
            if (needDry)
                for (int ch = 0; ch < nc; ++ch)
                    mDryScratch.addFrom (ch, 0, *monTakes, ch, 0, numSamples);
            // QA-Fd: the monitor takes decode through the same composed law,
            // so the same block's source stamp applies.
            mPitch.processFilePlayMonitor (*monTakes, monTimeline,
                mFilePlaySrcX0.load (std::memory_order_relaxed),
                mFilePlaySrcRate.load (std::memory_order_relaxed),
                mFilePlaySrcValid.load (std::memory_order_relaxed));
            for (int ch = 0; ch < nc; ++ch)
                buffer.addFrom (ch, 0, *monTakes, ch, 0, numSamples);
        }
    }

    mVocalChainRack .process (buffer);
    if (mNamIrProc != nullptr)
    {
        juce::MidiBuffer dummyMidi;
        mNamIrProc->processBlock (buffer, dummyMidi);
    }

    // ── Global Mix (wet/dry crossfade) ──────────────────────────────────
    if (needDry)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* wet = buffer.getWritePointer (ch);
            const float* dry = mDryScratch.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                wet[i] = dry[i] + mix * (wet[i] - dry[i]);
        }
    }

    // ── QA-Fe Task 5: True Dry -- add the raw live voice at the very end, so it
    // reaches the monitor with zero effect (bypassing corrector + chain + Mix)
    // while any prior takes above stay fully processed.  Skipped when the live
    // is muted (armed && !listen), matching the monitor gate. ─────────────────
    if (! filePlaySrc && ! monMuteLive && (monFrom == 0 || monMode == 0))
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float*       b   = buffer.getWritePointer (ch);
            const float* raw = mMonitorLiveDry.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                const int   r    = juce::jmax (0, xRem - i);
                const float w    = 1.0f - (float) r / (float) mMonXfadeLen;
                const float addW = (monFrom == 0 ? (1.0f - w) : 0.0f) + (monMode == 0 ? w : 0.0f);
                b[i] += raw[i] * addW;
            }
        }

    // Advance the monitor de-click crossfade once for the block.
    if (! filePlaySrc) mMonXfadeRemain = juce::jmax (0, mMonXfadeRemain - numSamples);
}

// ─── QA-F Task 3: BaySickAlign actions (message thread only) ─────────────────

int BaySickVocalProcessor::resolveLeaderChannel() const
{
    if (auto* p = apvts.getRawParameterValue ("bsa_leader_channel"))
        return (int) p->load();
    return -1;
}

int BaySickVocalProcessor::resolveFollowerChannel() const
{
    // Owner call 2026-07-11: the follower is ALWAYS the page you're on.  The
    // override picker is removed; bsa_follower_channel is vestigial (kept so
    // old saves load without a param-missing gap, but never read).
    return mOwnChannelId;
}

double BaySickVocalProcessor::alignResidualCapSec() const
{
    int mode = 1;
    float fine = 0.0f;
    if (auto* p = apvts.getRawParameterValue ("bsa_align_mode"))     mode = (int) p->load();
    if (auto* p = apvts.getRawParameterValue ("bsa_align_fineTune")) fine = p->load();
    // Owner respec 2026-07-11: Fine sweeps the mode's absolute residual
    // window -- Tight 0-50 / Close 50-150 / Loose 100-200 ms, knob center =
    // window center (twin display math in the align editor's
    // fineTuneCenterForMode/fineTuneHalfWidthForMode).
    const float center = (mode == 0) ? 150.0f : (mode == 2) ? 25.0f : 100.0f;
    const float half   = (mode == 2) ? 25.0f : 50.0f;
    return (double) juce::jmax (0.0f, center + fine * half / 50.0f) / 1000.0;
}

double BaySickVocalProcessor::alignMaxShiftSec() const
{
    // > 150 ms = No Limit (owner review 2026-07-11 vs the reference
    // aligner's 10-150 + No Limit model).
    if (auto* p = apvts.getRawParameterValue ("bsa_align_maxShift"))
    {
        const float v = p->load();
        return v > 150.5f ? 1.0e9 : (double) v / 1000.0;
    }
    return 1.0e9;
}

double BaySickVocalProcessor::alignFlexRatio() const
{
    // Owner call 2026-07-11: the Flexibility picker is removed (only Normal
    // felt right; the other rungs dropped valid pairings or smeared).  The
    // DSP stays parameterized on flexRatio so the control can return -- it's
    // just fed Normal's 2:1 slope bound unconditionally now.
    return 2.0;
}

juce::int64 BaySickVocalProcessor::followerPitchMapHash() const
{
    const int follower = resolveFollowerChannel();
    if (follower < 0) return 0;
    if (follower == mOwnChannelId)
        return mPitch.timeMapHash();
    if (onPitchDspForChannel)
        if (auto* dsp = onPitchDspForChannel (follower))
            return dsp->timeMapHash();
    return 0;
}

float BaySickVocalProcessor::effectiveAlignPitchSemis (float rawSemis) const
{
    float blend = 0.5f, cap = 0.0f;
    if (auto* p = apvts.getRawParameterValue ("bsa_pitch_range"))     blend = p->load() / 100.0f;
    if (auto* p = apvts.getRawParameterValue ("bsa_pitch_variation")) cap   = p->load();
    const float excess = std::abs (rawSemis) - juce::jmax (0.0f, cap);
    if (excess <= 0.0f) return 0.0f;
    return (rawSemis >= 0.0f ? 1.0f : -1.0f) * excess
         * juce::jlimit (0.0f, 1.0f, blend);
}

bool BaySickVocalProcessor::analyzeAlign (juce::String& errorOut)
{
    errorOut.clear();
    if (! onRenderComposite || ! onChannelClipSignature)
        { errorOut = "Align services not connected."; return false; }

    const int leader   = resolveLeaderChannel();
    const int follower = resolveFollowerChannel();
    if (leader < 0)         { errorOut = "Pick a Leader channel first.";   return false; }
    if (follower < 0)       { errorOut = "No Follower channel available."; return false; }
    if (leader == follower) { errorOut = "Leader and Follower must be different channels."; return false; }

    double gBeat = 0.0, dBeat = 0.0, gSr = 44100.0, dSr = 44100.0;
    juce::int64 gSamp = 0, dSamp = 0;
    auto guide = onRenderComposite (leader,   gBeat, gSamp, gSr);
    auto dub   = onRenderComposite (follower, dBeat, dSamp, dSr);
    if (guide.getNumSamples() <= 0) { errorOut = "The Leader channel has no audio clips.";   return false; }
    if (dub  .getNumSamples() <= 0) { errorOut = "The Follower channel has no audio clips."; return false; }

    // Common-origin pad: anchor times must share t=0 or pairing compares
    // apples to oranges (composites start at their own first clip).
    const juce::int64 commonSamp = juce::jmin (gSamp, dSamp);
    const double      commonBeat = (gSamp <= dSamp) ? gBeat : dBeat;
    const juce::int64 gPad = gSamp - commonSamp;
    const juce::int64 dPad = dSamp - commonSamp;
    if ((juce::int64) guide.getNumSamples() + gPad > ((juce::int64) 1 << 28)
        || (juce::int64) dub.getNumSamples() + dPad > ((juce::int64) 1 << 28))
        { errorOut = "Leader and Follower are too far apart on the timeline."; return false; }

    auto padFront = [] (juce::AudioBuffer<float>& b, juce::int64 padN)
    {
        if (padN <= 0) return;
        juce::AudioBuffer<float> padded (1, b.getNumSamples() + (int) padN);
        padded.clear();
        padded.copyFrom (0, (int) padN, b, 0, 0, b.getNumSamples());
        b = std::move (padded);
    };
    padFront (guide, gPad);
    padFront (dub,   dPad);

    AlignBuildParams bp;
    bp.residualCapSec = alignResidualCapSec();
    bp.maxShiftSec    = alignMaxShiftSec();
    bp.flexRatio      = alignFlexRatio();
    int bandG = 0, bandD = 0;
    if (auto* p = apvts.getRawParameterValue ("bsa_pitch_typeGuide")) bandG = juce::jlimit (0, 4, (int) p->load());
    if (auto* p = apvts.getRawParameterValue ("bsa_pitch_typeDub"))   bandD = juce::jlimit (0, 4, (int) p->load());
    bp.guideMinHz = kAlignPitchBands[bandG].minHz;
    bp.guideMaxHz = kAlignPitchBands[bandG].maxHz;
    bp.dubMinHz   = kAlignPitchBands[bandD].minHz;
    bp.dubMaxHz   = kAlignPitchBands[bandD].maxHz;

    // QA-Fd (locked 5/13a): align consumes the EDITED performance -- the
    // follower's pitch-tab time edits transform its raw onset times before
    // pairing.  Composite times here are common-origin padded, the pitch
    // map's domain is the follower's own composite -- convert through dPad.
    juce::int64 pmHash = 0;
    {
        BaySickPitchDSP* fDsp = nullptr;
        if (follower == mOwnChannelId)             fDsp = &mPitch;
        else if (onPitchDspForChannel)             fDsp = onPitchDspForChannel (follower);
        if (fDsp != nullptr && fDsp->timeMapHash() != 0)
        {
            pmHash = fDsp->timeMapHash();
            const double padSec = (double) dPad / gSr;
            bp.dubEditTransform = [fDsp, padSec] (double tCommon)
            {
                return fDsp->srcToEditedSec (tCommon - padSec) + padSec;
            };
        }
    }

    AlignAnalyzeDiag diag;
    auto map = BaySickAlignDSP::analyzeOffline (
        guide.getReadPointer (0), guide.getNumSamples(),
        dub  .getReadPointer (0), dub.getNumSamples(),
        gSr, bp,
        mAlignState.syncPoints, mAlignState.protectedAreas,
        &diag);

    if (! map.isValid())
    {
        const int winMs = (int) std::lround (diag.toleranceSec * 1000.0);
        if (diag.guideOnsets < 2 || diag.dubOnsets < 2)
            errorOut = "Could not detect enough word starts (Leader: "
                     + juce::String (diag.guideOnsets) + ", Follower: "
                     + juce::String (diag.dubOnsets)
                     + ").  Check both channels have vocal clips with clear words.  "
                       "The previous alignment (if any) is still applied.";
        else
            errorOut = "Found " + juce::String (diag.guideOnsets) + " Leader / "
                     + juce::String (diag.dubOnsets) + " Follower word starts, but "
                     + (diag.pairs == 0 ? juce::String ("none")
                                        : "only " + juce::String (diag.pairs))
                     + " lined up within the +/-" + juce::String (winMs)
                     + " ms matching window.  Check both takes sit at the same "
                       "spot on the grid.  "
                       "The previous alignment (if any) is still applied.";
        return false;
    }

    mAlignState.map                 = map;
    mAlignState.analyzedLeaderSig   = onChannelClipSignature (leader);
    mAlignState.analyzedFollowerSig = onChannelClipSignature (follower);
    mAlignState.commonStartBeat     = commonBeat;
    mAlignState.commonStartSample   = commonSamp;
    mAlignState.leaderPadSamples    = gPad;
    mAlignState.followerPadSamples  = dPad;
    mAlignState.analysisSampleRate  = gSr;
    mAlignState.analyzed            = true;
    mAlignState.analyzedSettingsGen = alignSettingsGeneration();
    mAlignState.analyzedPitchMapHash = pmHash;
    mAlign.setWarpMap (map);

    // QA-Fa recovery: Analyze IS Apply -- version the committed state and
    // publish it to the decode layer so playback picks it up this block.
    appendAlignVersion();
    publishAlignPlayback();
    if (auto& fn = mDirtyTracker.onAny) fn();
    if (onPitchAlignEditsChanged) onPitchAlignEditsChanged();   // §6.5 freeze
    return true;
}

// ─── QA-Fa recovery: applied-frame (de)serialization + versions + publish ────

juce::ValueTree BaySickVocalProcessor::alignAppliedToTree() const
{
    juce::ValueTree v ("AlignApplied");
    v.setProperty ("lSig",      mAlignState.analyzedLeaderSig,   nullptr);
    v.setProperty ("fSig",      mAlignState.analyzedFollowerSig, nullptr);
    v.setProperty ("startBeat", mAlignState.commonStartBeat,     nullptr);
    v.setProperty ("cSamp",     mAlignState.commonStartSample,   nullptr);
    v.setProperty ("lPad",      mAlignState.leaderPadSamples,    nullptr);
    v.setProperty ("fPad",      mAlignState.followerPadSamples,  nullptr);
    v.setProperty ("sr",        mAlignState.analysisSampleRate,  nullptr);
    v.appendChild (mAlignState.map.toValueTree(), nullptr);
    return v;
}

void BaySickVocalProcessor::alignAppliedFromTree (const juce::ValueTree& v)
{
    if (! v.hasType ("AlignApplied")) return;
    mAlignState.analyzedLeaderSig   = (juce::int64) v.getProperty ("lSig", 0);
    mAlignState.analyzedFollowerSig = (juce::int64) v.getProperty ("fSig", 0);
    mAlignState.commonStartBeat     = (double) v.getProperty ("startBeat", 0.0);
    mAlignState.commonStartSample   = (juce::int64) v.getProperty ("cSamp", 0);
    mAlignState.leaderPadSamples    = (juce::int64) v.getProperty ("lPad", 0);
    mAlignState.followerPadSamples  = (juce::int64) v.getProperty ("fPad", 0);
    mAlignState.analysisSampleRate  = (double) v.getProperty ("sr", 44100.0);
    auto mapTree = v.getChildWithName ("WarpMap");
    if (mapTree.isValid())
    {
        mAlignState.map.fromValueTree (mapTree);
        mAlignState.analyzed = mAlignState.map.isValid();
    }
    if (mAlignState.analyzed)
        mAlign.setWarpMap (mAlignState.map);
}

void BaySickVocalProcessor::appendAlignVersion()
{
    if (! mAlignState.analyzed) return;
    EditorVersionEntry e;
    e.state   = alignAppliedToTree();
    e.dateIso = juce::Time::getCurrentTime().toISO8601 (true);
    e.sigA    = mAlignState.analyzedLeaderSig;
    e.sigB    = mAlignState.analyzedFollowerSig;
    mAlignVersions.push_back (std::move (e));
    while ((int) mAlignVersions.size() > kMaxEditorVersions)
        mAlignVersions.erase (mAlignVersions.begin());
}

void BaySickVocalProcessor::appendPitchVersion()
{
    if (! mPitch.isAnalyzed()) return;
    EditorVersionEntry e;
    e.state   = mPitch.stateToValueTree();
    e.dateIso = juce::Time::getCurrentTime().toISO8601 (true);
    e.sigA    = mPitchAnalyzedSig;
    mPitchVersions.push_back (std::move (e));
    while ((int) mPitchVersions.size() > kMaxEditorVersions)
        mPitchVersions.erase (mPitchVersions.begin());
}

bool BaySickVocalProcessor::revertAlignToVersion (int index)
{
    if (index < 0 || index >= (int) mAlignVersions.size()) return false;
    alignAppliedFromTree (mAlignVersions[(size_t) index].state);
    // Revert is explicit user intent to run THAT map -- current knob state
    // is accepted as its baseline (knob moves after the revert re-arm the
    // stale flag normally).
    mAlignState.analyzedSettingsGen  = alignSettingsGeneration();
    mAlignState.analyzedPitchMapHash = followerPitchMapHash();
    publishAlignPlayback();
    if (auto& fn = mDirtyTracker.onAny) fn();
    if (onPitchAlignEditsChanged) onPitchAlignEditsChanged();   // §6.5 freeze
    return true;
}

bool BaySickVocalProcessor::revertPitchToVersion (int index)
{
    if (index < 0 || index >= (int) mPitchVersions.size()) return false;
    mPitch.stateFromValueTree (mPitchVersions[(size_t) index].state);
    mPitchAnalyzedSig = mPitchVersions[(size_t) index].sigA;
    if (auto& fn = mDirtyTracker.onAny) fn();
    if (onPitchAlignEditsChanged) onPitchAlignEditsChanged();   // §6.5 freeze
    return true;
}

void BaySickVocalProcessor::publishAlignPlayback()
{
    std::unique_ptr<AlignPlaySnapshot> snap;
    if (mAlignState.analyzed && mAlignState.map.isValid())
    {
        snap = std::make_unique<AlignPlaySnapshot>();
        snap->followerChannelId  = resolveFollowerChannel();
        snap->commonStartBeat    = mAlignState.commonStartBeat;
        snap->commonStartSample  = mAlignState.commonStartSample;
        snap->analysisSampleRate = mAlignState.analysisSampleRate;

        // Guide-axis SoA, sorted + monotone-clamped so the decode lookup is
        // well-defined even if a pathological pairing crossed anchors.
        auto anchors = mAlignState.map.anchors;
        std::sort (anchors.begin(), anchors.end(),
                   [] (const WarpAnchor& a, const WarpAnchor& b)
                   { return a.guideTimeSec < b.guideTimeSec; });
        double lastDub = -1.0e18;
        for (const auto& a : anchors)
        {
            const double d = juce::jmax (lastDub, a.dubTimeSec);
            // QA-Fd 12a: map stores RAW deltas; Blend/Variation apply here so
            // the pitch knobs are live -- the editor republishes on any
            // pitch-knob change (time map identical, mid-play legal).
            const float eff = effectiveAlignPitchSemis (a.pitchSemis);
            snap->guideSec  .push_back (a.guideTimeSec);
            snap->dubSec    .push_back (d);
            snap->pitchSemis.push_back (eff);
            if (std::abs (eff) > 0.01f) snap->anyPitch = true;
            lastDub = d;
        }
        // Collapse near-equal guide entries (a post-clamp duplicate would
        // poison the cubic tangents with a degenerate segment); keep the
        // later dub -- the clamp's own resolution.
        {
            size_t w = 0;
            for (size_t i = 0; i < snap->guideSec.size(); ++i)
            {
                if (w > 0 && snap->guideSec[i] - snap->guideSec[w - 1] < 1.0e-4)
                {
                    snap->dubSec[w - 1]     = snap->dubSec[i];
                    snap->pitchSemis[w - 1] = snap->pitchSemis[i];
                    continue;
                }
                snap->guideSec[w]   = snap->guideSec[i];
                snap->dubSec[w]     = snap->dubSec[i];
                snap->pitchSemis[w] = snap->pitchSemis[i];
                ++w;
            }
            snap->guideSec.resize (w);
            snap->dubSec.resize (w);
            snap->pitchSemis.resize (w);
        }
        snap->computeTangents();
        if (! snap->isUsable())
            snap.reset();
    }

    auto* old = mAlignPlayActive.exchange (snap.release(),
                                           std::memory_order_acq_rel);
    if (old != nullptr)
        mAlignPlayRetired.push_back (std::unique_ptr<AlignPlaySnapshot> (old));
    while (mAlignPlayRetired.size() > 8)
        mAlignPlayRetired.erase (mAlignPlayRetired.begin());
}

juce::AudioBuffer<float> BaySickVocalProcessor::buildWarpedFollower (juce::String& errorOut,
                                                                     bool highRes)
{
    errorOut.clear();
    juce::AudioBuffer<float> empty;
    if (! mAlignState.analyzed || ! mAlignState.map.isValid())
        { errorOut = "Analyze first."; return empty; }
    if (! onRenderComposite)
        { errorOut = "Align services not connected."; return empty; }

    const int follower = resolveFollowerChannel();
    if (follower < 0) { errorOut = "No Follower channel available."; return empty; }

    double b = 0.0, sr = mAlignState.analysisSampleRate;
    juce::int64 s = 0;
    auto dub = onRenderComposite (follower, b, s, sr);
    if (dub.getNumSamples() <= 0)
        { errorOut = "The Follower channel has no audio clips."; return empty; }

    if (mAlignState.followerPadSamples > 0)
    {
        juce::AudioBuffer<float> padded (1,
            dub.getNumSamples() + (int) mAlignState.followerPadSamples);
        padded.clear();
        padded.copyFrom (0, (int) mAlignState.followerPadSamples, dub, 0, 0,
                         dub.getNumSamples());
        dub = std::move (padded);
    }

    bool  alignOn = true, pitchOn = false, formantOn = false;
    int   algo = 0, transpose = 0;
    float formantShift = 0.0f;
    if (auto* p = apvts.getRawParameterValue ("bsa_align_on"))       alignOn = p->load() > 0.5f;
    if (auto* p = apvts.getRawParameterValue ("bsa_pitch_on"))       pitchOn = p->load() > 0.5f;
    if (auto* p = apvts.getRawParameterValue ("bsa_pitch_algo"))     algo = (int) p->load();
    if (auto* p = apvts.getRawParameterValue ("bsa_pitch_transpose")) transpose = (int) p->load();
    if (auto* p = apvts.getRawParameterValue ("bsa_formant_on"))     formantOn = p->load() > 0.5f;
    if (auto* p = apvts.getRawParameterValue ("bsa_formant_shift"))  formantShift = p->load();

    // Align master OFF renders the follower unwarped (an identity map keeps
    // the pitch pass available -- the Pitch box is independently toggleable).
    // QA-Fd 12a: anchors carry RAW deltas -- run them through the same
    // Variation/Blend transform the live publish uses so render == playback.
    WarpMap effMap = mAlignState.map;
    if (! alignOn)
        for (auto& a : effMap.anchors)
            a.guideTimeSec = a.dubTimeSec;
    for (auto& a : effMap.anchors)
        a.pitchSemis = pitchOn ? effectiveAlignPitchSemis (a.pitchSemis) : 0.0f;

    // ── QA-Fd Task 8: compose the follower's pitch TIME map (locked 5/13a:
    // the decode layer plays rawLaw(pitchMap(alignMap(t))) -- renders must
    // match).  Sample a dense guide set (align anchor guides + the pitch
    // anchors' dst positions pulled back through the align map) and rebuild
    // the anchors as guide -> SOURCE.
    {
        BaySickPitchDSP* fDsp = nullptr;
        if (follower == mOwnChannelId)  fDsp = &mPitch;
        else if (onPitchDspForChannel)  fDsp = onPitchDspForChannel (follower);
        const AlignPlaySnapshot* tm =
            (fDsp != nullptr) ? fDsp->loadTimeMapSnapshot() : nullptr;

        if (tm != nullptr && tm->guideSec.size() >= 2 && effMap.anchors.size() >= 2)
        {
            const double padSec = (double) mAlignState.followerPadSamples / sr;

            // Guide-axis lookup over the (possibly identity) align anchors.
            AlignPlaySnapshot alook;
            {
                auto sorted = effMap.anchors;
                std::sort (sorted.begin(), sorted.end(),
                           [] (const WarpAnchor& l, const WarpAnchor& r)
                           { return l.guideTimeSec < r.guideTimeSec; });
                double lastDub = -1.0e18;
                for (const auto& a : sorted)
                {
                    const double d = juce::jmax (lastDub, a.dubTimeSec);
                    alook.guideSec  .push_back (a.guideTimeSec);
                    alook.dubSec    .push_back (d);
                    alook.pitchSemis.push_back (a.pitchSemis);
                    lastDub = d;
                }
                alook.computeTangents();
            }
            auto alignDubAt = [&alook] (double g)
            {
                double d = g, dp = 1.0; float sm = 0.0f;
                alook.lookupAtGuideSec (g, d, dp, sm);
                return d;
            };
            auto alignSemisAt = [&alook] (double g)
            {
                double d = g, dp = 1.0; float sm = 0.0f;
                alook.lookupAtGuideSec (g, d, dp, sm);
                return sm;
            };
            // Inverse of the (monotone) align dub lookup by bisection.
            const double gLo = alook.guideSec.front() - 5.0;
            const double gHi = alook.guideSec.back() + 5.0;
            auto guideForDub = [&] (double dWant)
            {
                double lo = gLo, hi = gHi;
                for (int it = 0; it < 48; ++it)
                {
                    const double mid = (lo + hi) * 0.5;
                    if (alignDubAt (mid) < dWant) lo = mid; else hi = mid;
                }
                return (lo + hi) * 0.5;
            };

            std::vector<double> guides;
            for (const auto& a : effMap.anchors)
                guides.push_back (a.guideTimeSec);
            for (double dst : tm->guideSec)
                guides.push_back (guideForDub (dst + padSec));
            std::sort (guides.begin(), guides.end());
            guides.erase (std::unique (guides.begin(), guides.end(),
                              [] (double a, double b2)
                              { return std::abs (a - b2) < 1.0e-4; }),
                          guides.end());

            auto pitchSrcAt = [&tm] (double te)
            {
                double u = te, dp = 1.0; float sm = 0.0f;
                tm->lookupAtGuideSec (te, u, dp, sm);
                return u;
            };

            WarpMap composed;
            composed.analysisSampleRate = effMap.analysisSampleRate;
            composed.dubDurationSec     = effMap.dubDurationSec;
            composed.guideDurationSec   = effMap.guideDurationSec;
            for (double g : guides)
            {
                WarpAnchor a;
                a.guideTimeSec = g;
                const double dEdited = alignDubAt (g);
                a.dubTimeSec   = pitchSrcAt (dEdited - padSec) + padSec;
                a.pitchSemis   = alignSemisAt (g);
                a.weight       = 1.0f;
                composed.anchors.push_back (a);
            }
            effMap = std::move (composed);
        }
    }

    // QA-Fd 16a: High-Res = the warp phase oversampled to ~384 kHz-class.
    const int os = highRes
        ? juce::jlimit (2, 8, juce::roundToInt (384000.0 / juce::jmax (1.0, sr)))
        : 1;

    auto warped = BaySickAlignDSP::applyWarp (
        dub.getReadPointer (0), dub.getNumSamples(),
        mAlignState.analysisSampleRate, effMap, algo,
        pitchOn ? (float) transpose : 0.0f, os);

    if (pitchOn && formantOn && std::abs (formantShift) > 0.01f
        && warped.getNumSamples() > 0)
        CepstralFormantEngine::formantShiftMono (warped.getWritePointer (0),
                                                 warped.getNumSamples(),
                                                 mAlignState.analysisSampleRate,
                                                 formantShift);
    return warped;
}

juce::AudioBuffer<float> BaySickVocalProcessor::renderAlignedPreview()
{
    juce::String ignored;
    return buildWarpedFollower (ignored);
}

juce::File BaySickVocalProcessor::renderAlignedTake (juce::String& errorOut, bool highRes)
{
    errorOut.clear();
    if (! onGetProjectFolder)
        { errorOut = "Align services not connected."; return {}; }

    auto warped = buildWarpedFollower (errorOut, highRes);
    if (warped.getNumSamples() <= 0)
        return {};

    const juce::File projDir = onGetProjectFolder();
    if (projDir == juce::File() || ! projDir.isDirectory())
        { errorOut = "Save the project first - renders live in the project folder."; return {}; }

    auto alignedDir = projDir.getChildFile ("Aligned");
    alignedDir.createDirectory();

    juce::String base = "Follower";
    const int follower = resolveFollowerChannel();
    if (onListCandidateChannels)
        for (const auto& c : onListCandidateChannels())
            if (c.first == follower) { base = c.second.replaceCharacter (' ', '_'); break; }

    int version = (int) mAlignState.renders.size() + 1;
    auto file = alignedDir.getChildFile (base + "_align_v" + juce::String (version) + ".wav");
    while (file.existsAsFile())
        file = alignedDir.getChildFile (base + "_align_v" + juce::String (++version) + ".wav");

    juce::WavAudioFormat fmt;
    auto os = file.createOutputStream();
    if (os == nullptr) { errorOut = "Could not write " + file.getFullPathName(); return {}; }
    // createWriterFor takes the stream only on success (it releases the pointer
    // back to the caller when it fails), so hand it over after the null test or
    // a failed create leaks the open file handle.
    std::unique_ptr<juce::AudioFormatWriter> writer (fmt.createWriterFor (
        os.get(), mAlignState.analysisSampleRate, 1, 24, {}, 0));
    if (writer == nullptr) { errorOut = "Could not create the WAV writer."; return {}; }
    os.release();

    if (! writer->writeFromAudioSampleBuffer (warped, 0, warped.getNumSamples()))
    {
        // reset() first so the header flush happens before the file goes away.
        writer.reset();
        file.deleteFile();
        errorOut = "Could not finish writing " + file.getFileName() + " (disk full?).";
        return {};
    }
    writer.reset();

    AlignRenderEntry e;
    e.file      = "Aligned/" + file.getFileName();
    e.dateIso   = juce::Time::getCurrentTime().toISO8601 (true);
    e.version   = version;
    e.startBeat = mAlignState.commonStartBeat;
    mAlignState.renders.push_back (e);

    // QA-Fa recovery: Render is EXPORT ONLY -- no grid placement, nothing
    // audible changes (playback already applies the map live).  Re-import
    // goes through the Vox ribbon's "+ Add New Vox From Export" flow.
    if (auto& fn = mDirtyTracker.onAny) fn();
    return file;
}

bool BaySickVocalProcessor::isAlignStale() const
{
    if (! mAlignState.analyzed || ! onChannelClipSignature) return false;
    // QA-Fd reconciliation rule: a time-map knob moved since analyze -> the
    // applied map no longer reflects the settings -> stale (swaps at stop).
    if (alignSettingsGeneration() != mAlignState.analyzedSettingsGen) return true;
    // QA-Fd (locked 5/13a): the follower's pitch time edits redefine the
    // performance align matched -> stale on any time-map change.
    if (followerPitchMapHash() != mAlignState.analyzedPitchMapHash) return true;
    const int leader   = resolveLeaderChannel();
    const int follower = resolveFollowerChannel();
    if (leader < 0 || follower < 0) return false;
    return onChannelClipSignature (leader)   != mAlignState.analyzedLeaderSig
        || onChannelClipSignature (follower) != mAlignState.analyzedFollowerSig;
}

// ─── QA-Fa: BaySickPitch actions (message thread only) ────────────────────────

bool BaySickVocalProcessor::analyzePitch (juce::String& errorOut)
{
    errorOut.clear();
    if (! onRenderComposite || ! onChannelClipSignature)
        { errorOut = "Pitch services not connected."; return false; }
    if (mOwnChannelId < 0)
        { errorOut = "This Vox page has no channel yet."; return false; }

    double beat = 0.0, sr = 44100.0;
    juce::int64 samp = 0;
    auto comp = onRenderComposite (mOwnChannelId, beat, samp, sr);
    if (comp.getNumSamples() <= 0)
        { errorOut = "This channel has no audio clips to analyze."; return false; }

    mPitch.analyzeComposite (comp.getReadPointer (0), comp.getNumSamples(),
                             sr, beat, samp);
    mPitchAnalyzedSig = onChannelClipSignature (mOwnChannelId);
    // QA-Fa recovery: every analyze is a restore point (edits stay instant
    // between analyses -- versions are the safety net, not an edit gate).
    appendPitchVersion();
    if (auto& fn = mDirtyTracker.onAny) fn();
    if (onPitchAlignEditsChanged) onPitchAlignEditsChanged();   // §6.5 freeze
    return true;
}

juce::File BaySickVocalProcessor::renderPitchedTake (juce::String& errorOut)
{
    errorOut.clear();
    if (! onRenderComposite || ! onGetProjectFolder)
        { errorOut = "Pitch services not connected."; return {}; }
    if (! mPitch.isAnalyzed())
        { errorOut = "Nothing analyzed yet - open the editor with clips on the channel."; return {}; }

    double beat = 0.0, sr = mPitch.analysisSampleRate();
    juce::int64 samp = 0;
    auto comp = onRenderComposite (mOwnChannelId, beat, samp, sr);
    if (comp.getNumSamples() <= 0)
        { errorOut = "This channel has no audio clips."; return {}; }

    // renderOffline WARPS-THEN-BAKES (the time map is applied inside bakeSpan,
    // exactly like playback), so the export already honors the channel's TIME
    // edits.  The old post-hoc applyWarp here ran on the ALREADY-pitched buffer
    // (pitch-then-warp) and destroyed phase coherence -- engine-independent
    // garble on any time move.  Removed.  (QA-Fe2 item 2: the highRes flag is
    // retired with the dialog choice -- engine-native warping made the PV
    // oversampling it drove obsolete.)
    auto rendered = mPitch.renderOffline (comp.getReadPointer (0),
                                          comp.getNumSamples(), sr);
    if (rendered.getNumSamples() <= 0)
        { errorOut = "Render produced no audio."; return {}; }

    const juce::File projDir = onGetProjectFolder();
    if (projDir == juce::File() || ! projDir.isDirectory())
        { errorOut = "Save the project first - renders live in the project folder."; return {}; }

    auto pitchedDir = projDir.getChildFile ("Pitched");
    pitchedDir.createDirectory();

    juce::String base = "Vox";
    if (onListCandidateChannels)
        for (const auto& c : onListCandidateChannels())
            if (c.first == mOwnChannelId)
                { base = c.second.replaceCharacter (' ', '_'); break; }

    int version = (int) mPitchRenders.size() + 1;
    auto file = pitchedDir.getChildFile (base + "_pitch_v" + juce::String (version) + ".wav");
    while (file.existsAsFile())
        file = pitchedDir.getChildFile (base + "_pitch_v" + juce::String (++version) + ".wav");

    juce::WavAudioFormat fmt;
    auto os = file.createOutputStream();
    if (os == nullptr) { errorOut = "Could not write " + file.getFullPathName(); return {}; }
    std::unique_ptr<juce::AudioFormatWriter> writer (fmt.createWriterFor (
        os.release(), sr, 1, 24, {}, 0));
    if (writer == nullptr) { errorOut = "Could not create the WAV writer."; return {}; }

    if (! writer->writeFromAudioSampleBuffer (rendered, 0, rendered.getNumSamples()))
    {
        // reset() first so the header flush happens before the file goes away.
        writer.reset();
        file.deleteFile();
        errorOut = "Could not finish writing " + file.getFileName() + " (disk full?).";
        return {};
    }
    writer.reset();

    AlignRenderEntry e;
    e.file      = "Pitched/" + file.getFileName();
    e.dateIso   = juce::Time::getCurrentTime().toISO8601 (true);
    e.version   = version;
    e.startBeat = mPitch.startBeat();
    mPitchRenders.push_back (e);

    // QA-Fa recovery: Render is EXPORT ONLY (same contract as the Align
    // render above).
    if (auto& fn = mDirtyTracker.onAny) fn();
    return file;
}

bool BaySickVocalProcessor::isPitchStale() const
{
    if (! mPitch.isAnalyzed() || ! onChannelClipSignature || mOwnChannelId < 0)
        return false;
    return onChannelClipSignature (mOwnChannelId) != mPitchAnalyzedSig;
}

// ─── QA-Fd preview play (message thread) ──────────────────────────────────────
void BaySickVocalProcessor::startPitchPreview (const juce::AudioBuffer<float>& composite,
                                               double sr, double srcStartSec, double srcEndSec,
                                               double editedStartSec, double editedEndSec,
                                               bool loop)
{
    if (composite.getNumSamples() <= 0 || sr <= 0.0 || srcEndSec <= srcStartSec)
        { stopPitchPreview(); return; }

    // Dry-fallback span = the raw note at its SOURCE position (correct for an
    // unedited note AND a time-only edit, which has no cache).
    const juce::int64 s0 = juce::jlimit ((juce::int64) 0,
        (juce::int64) composite.getNumSamples(),
        (juce::int64) std::llround (srcStartSec * sr));
    const int len = (int) juce::jlimit ((juce::int64) 0,
        (juce::int64) composite.getNumSamples() - s0,
        (juce::int64) std::llround ((srcEndSec - srcStartSec) * sr));
    if (len <= 0) { stopPitchPreview(); return; }

    auto pv = std::make_unique<PitchPreview>();
    // QA-Fe threading fix: read the background-baked cache (lock-free load + span
    // copy) instead of running the heavy engine bake HERE.  The old renderOffline
    // call ran WORLD/RubberBand/Signalsmith SYNCHRONOUSLY on the message thread on
    // every scrub during a drag, freezing the UI (WORLD worst).  No cache yet
    // (unedited/neutral note) -> audition the dry source span.
    pv->buf = mPitch.copyCacheSpan (editedStartSec, editedEndSec);
    if (pv->buf.getNumSamples() <= 0)
    {
        pv->buf.setSize (1, len);
        pv->buf.copyFrom (0, 0, composite.getReadPointer (0) + s0, len);
    }
    pv->loop = loop;
    if (pv->buf.getNumSamples() <= 0) { stopPitchPreview(); return; }

    // ~3 ms edge fades de-click the span boundaries.
    const int fade = juce::jmin (pv->buf.getNumSamples() / 2,
                                 (int) std::lround (sr * 0.003));
    if (fade > 0)
    {
        pv->buf.applyGainRamp (0, 0, fade, 0.0f, 1.0f);
        pv->buf.applyGainRamp (0, pv->buf.getNumSamples() - fade, fade, 1.0f, 0.0f);
    }

    auto* old = mPitchPreview.exchange (pv.release(), std::memory_order_acq_rel);
    if (old != nullptr)
        mPreviewRetired.push_back (std::unique_ptr<PitchPreview> (old));
    while (mPreviewRetired.size() > 4)
        mPreviewRetired.erase (mPreviewRetired.begin());
}

void BaySickVocalProcessor::stopPitchPreview()
{
    auto* old = mPitchPreview.exchange (nullptr, std::memory_order_acq_rel);
    if (old != nullptr)
        mPreviewRetired.push_back (std::unique_ptr<PitchPreview> (old));
    while (mPreviewRetired.size() > 4)
        mPreviewRetired.erase (mPreviewRetired.begin());
}

// ─── Editor ───────────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* BaySickVocalProcessor::createEditor()
{
    // H-6 (2026-05-01) -- real 6-sub-tab editor lands.
    return new BaySickVocalEditor (*this);
}

// ─── A/B compare ──────────────────────────────────────────────────────────────
// APVTS listener on `bsv_ab_slot`, fired synchronously on whatever thread wrote
// the parameter.
//
// LIVE: the work is posted to the message thread because the swap walks the
// whole snapshot parameter set -- that may not run on the realtime callback.
// The post carries a weak lifetime token because this is a rig-owned tab engine
// the message thread can free at any time, and a queued callAsync cannot be
// retracted.
//
// OFFLINE: the writer already IS the render thread (automation replay), which
// has no realtime deadline, so the work runs inline -- a message hop would land
// after the block it belongs to, and during a freeze render the message thread
// is blocked inside the render loop entirely.
// ─────────────────────────────────────────────────────────────────────────────
void BaySickVocalProcessor::parameterChanged (const juce::String& paramID, float newValue)
{
    if (paramID != "bsv_ab_slot") return;

    // replaceState fires this listener while a project/page preset loads.  The
    // restore seats both snapshots and mLastSlot itself, from the saved SlotA /
    // SlotB children; letting a swap run here would capture the freshly
    // restored values over the OTHER slot's stored tone and destroy it.
    if (isRestoringState()) return;

    const int newSlot = juce::jlimit (0, 1, (int) newValue);
    if (newSlot == mLastSlot) return;

    const int outgoingSlot = mLastSlot;

    // A replay is not a user edit -- the same ruling that keeps a replay from
    // marking engine content dirty or staling a freeze.  The two banks hold
    // tones the user dialled by hand; capturing over the outgoing one on every
    // lane transition would bank whatever the OTHER lanes have driven the chain
    // to at that instant, destroying both tones over the length of a pass.  The
    // slot still SWITCHES on replay -- only the capture is suppressed.
    //
    // The phase flag is thread_local and this listener runs synchronously on
    // the writer's thread, so it must be read here and carried into the async
    // hop below, which lands after the writer's scope has closed.
    const bool userGesture = ! juce::AudioProcessorValueTreeState::programmaticWritePhase;

    if (isNonRealtime())
    {
        if (userGesture) captureSnapshotFromCurrent (outgoingSlot);
        applySnapshotToCurrent (newSlot);
        mLastSlot = newSlot;
        return;
    }

    std::weak_ptr<char> life (mLife);
    juce::MessageManager::callAsync ([this, life, outgoingSlot, newSlot, userGesture]()
    {
        if (life.expired()) return;
        if (userGesture) captureSnapshotFromCurrent (outgoingSlot);
        applySnapshotToCurrent (newSlot);
        mLastSlot = newSlot;
    });
}

juce::ValueTree BaySickVocalProcessor::SlotSnapshot::toValueTree (const juce::Identifier& root) const
{
    juce::ValueTree v (root);
    for (int i = 0; i < kNumAbSnapshotParams && i < (int) values.size(); ++i)
        v.setProperty (juce::Identifier (kAbSnapshotParams[i]),
                        values[(size_t) i], nullptr);
    return v;
}

void BaySickVocalProcessor::SlotSnapshot::fromValueTree (const juce::ValueTree& v)
{
    values.resize ((size_t) kNumAbSnapshotParams, 0.0f);

    // Keyed by param id, and absent ids are LEFT ALONE: the caller seeds the
    // snapshot from the live parameters first, so an id the save predates
    // restores to the value the project actually loaded with.
    for (int i = 0; i < kNumAbSnapshotParams; ++i)
    {
        const juce::Identifier id (kAbSnapshotParams[i]);
        if (v.hasProperty (id))
            values[(size_t) i] = (float) v.getProperty (id);
    }
}

int BaySickVocalProcessor::activeAbSlot() const noexcept
{
    if (auto* p = apvts.getRawParameterValue ("bsv_ab_slot"))
        return juce::jlimit (0, 1, (int) p->load());
    return 0;
}

void BaySickVocalProcessor::captureSnapshotFromCurrent (int slot)
{
    if (slot < 0 || slot > 1) return;

    auto& s = mSnapshots[(size_t) slot];
    s.values.resize ((size_t) kNumAbSnapshotParams, 0.0f);

    for (int i = 0; i < kNumAbSnapshotParams; ++i)
        if (auto* p = apvts.getRawParameterValue (kAbSnapshotParams[i]))
            s.values[(size_t) i] = p->load();
}

void BaySickVocalProcessor::applySnapshotToCurrent (int slot)
{
    if (slot < 0 || slot > 1) return;

    const auto& s = mSnapshots[(size_t) slot];
    if ((int) s.values.size() != kNumAbSnapshotParams) return;

    // Snapshot restores are programmatic.  Undo of an A/B flip stays correct
    // WITHOUT these values in history -- undoing the bsv_ab_slot param re-fires
    // parameterChanged, which re-applies the other slot's snapshot (the handler
    // is self-healing).
    juce::AudioProcessorValueTreeState::ScopedProgrammaticParamWrites spw;

    for (int i = 0; i < kNumAbSnapshotParams; ++i)
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                          apvts.getParameter (kAbSnapshotParams[i])))
            p->setValueNotifyingHost (
                p->getNormalisableRange().convertTo0to1 (s.values[(size_t) i]));
}

// ─── State save / load ────────────────────────────────────────────────────────
//
// H-6d (2026-05-02): captures the entire 6-sub-tab Vox page state, so the
// hamburger menu's Save Page Preset / Load Page Preset round-trips
// faithfully.  Captured state per sub-tab:
//
//   BaySickVocals      -> APVTS (bsv_pitch_*, bsv_mix, bsv_ab_slot, etc.)
//                          plus <SlotA> / <SlotB>, the two A/B compare
//                          snapshots of the chain params (see SlotSnapshot)
//   Vocal Chain        -> APVTS (bsv_deesser_*, bsv_comp_*, bsv_sat_*,
//                                bsv_limiter_*); rack topology is fixed
//   BaySickPitch       -> <PitchEdits> ValueTree child (note regions +
//                          edits + analysis frame + render history +
//                          version history)
//   BaySickAlign       -> <AlignEdits> ValueTree child (warp map + sync
//                          points + protected areas + analysis frame +
//                          render history + version history)
//   BaySickNAM/IR      -> <NamIrState> child holding the embedded
//                          BaySickNAMIRProcessor's getStateInformation
//                          binary (per-slot A/B snapshots + NAM models +
//                          cab IRs + Mic Sim per-slot user IRs + Mic
//                          Placement, all included transitively)
//   Pre Rack EQ        -> handled by PagePresetIO (strip's Pre Rack EQ8
//                          M/S is captured at the page-preset wrapper
//                          level alongside mixer + insert rack + post-EQ)
//
// PagePresetIO calls THIS getStateInformation as part of its capture, so
// everything below rolls into the page preset XML automatically.
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    constexpr const char* kPitchEditsTag = "PitchEdits";
    constexpr const char* kAlignEditsTag = "AlignEdits";
    constexpr const char* kNamIrStateTag = "NamIrState";
    // QA-F chain-wiring fix (2026-07-10): per-slot chain-DSP state blobs.
    // Covers the panel-only controls with no bsv_ params behind them (the
    // saturation tube/tape set etc.) -- bound controls persist via APVTS and
    // the per-block push re-imposes them over the blob on load (params stay
    // the source of truth for everything bound).
    constexpr const char* kVocalChainStateTag = "VocalChainState";
    // Per-slot A/B snapshots.  Absence means "both slots start as the values
    // the project loaded with", so a save made before A/B existed loads with
    // exactly the tone it was saved with.
    constexpr const char* kSlotATag = "SlotA";
    constexpr const char* kSlotBTag = "SlotB";
}

void BaySickVocalProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    // The active slot's live parameter values are what the user is looking at;
    // the other slot's snapshot already holds what it had at the last flip.
    // Without this the in-flight slot's edits since that flip are lost on save.
    captureSnapshotFromCurrent (activeAbSlot());

    auto state = apvts.copyState();

    // Strip any prior children we own so saves are idempotent.
    auto removeChild = [&] (const juce::Identifier& name)
    {
        while (true)
        {
            auto child = state.getChildWithName (name);
            if (! child.isValid()) break;
            state.removeChild (child, nullptr);
        }
    };
    removeChild (kPitchEditsTag);
    removeChild (kAlignEditsTag);
    removeChild (kNamIrStateTag);
    removeChild (kVocalChainStateTag);
    removeChild (kSlotATag);
    removeChild (kSlotBTag);

    state.appendChild (mSnapshots[0].toValueTree (kSlotATag), nullptr);
    state.appendChild (mSnapshots[1].toValueTree (kSlotBTag), nullptr);

    // QA-Fa: <PitchEdits> carries the BaySickPitch channel state -- analyzed
    // note regions + per-note edits, the analysis frame, the Pitched/ render
    // history, the analyze-time clip signature, and (QA-Fa recovery) the
    // version history.
    {
        juce::ValueTree pitch (kPitchEditsTag);
        pitch.setProperty ("pSig", mPitchAnalyzedSig, nullptr);
        pitch.appendChild (mPitch.stateToValueTree(), nullptr);
        for (const auto& r : mPitchRenders)
        {
            juce::ValueTree e ("R");
            e.setProperty ("f", r.file,      nullptr);
            e.setProperty ("d", r.dateIso,   nullptr);
            e.setProperty ("v", r.version,   nullptr);
            e.setProperty ("b", r.startBeat, nullptr);
            pitch.appendChild (e, nullptr);
        }
        for (const auto& ver : mPitchVersions)
        {
            juce::ValueTree e ("Ver");
            e.setProperty ("ts", ver.dateIso, nullptr);
            e.setProperty ("sa", ver.sigA,    nullptr);
            e.appendChild (ver.state.createCopy(), nullptr);
            pitch.appendChild (e, nullptr);
        }
        state.appendChild (pitch, nullptr);
    }

    // QA-F Task 3: <AlignEdits> carries the full channel-pair align state --
    // WarpMap (with per-anchor pitch deltas), sync points, protected areas,
    // render history, and the analysis frame.
    {
        juce::ValueTree align (kAlignEditsTag);
        align.setProperty ("analyzed",   mAlignState.analyzed ? 1 : 0,      nullptr);
        align.setProperty ("lSig",       mAlignState.analyzedLeaderSig,     nullptr);
        align.setProperty ("fSig",       mAlignState.analyzedFollowerSig,   nullptr);
        align.setProperty ("startBeat",  mAlignState.commonStartBeat,       nullptr);
        align.setProperty ("cSamp",      mAlignState.commonStartSample,     nullptr);
        align.setProperty ("lPad",       mAlignState.leaderPadSamples,      nullptr);
        align.setProperty ("fPad",       mAlignState.followerPadSamples,    nullptr);
        align.setProperty ("sr",         mAlignState.analysisSampleRate,    nullptr);
        if (mAlignState.analyzed)
            align.appendChild (mAlignState.map.toValueTree(), nullptr);
        for (const auto& p : mAlignState.syncPoints)
            align.appendChild (p.toValueTree(), nullptr);
        for (const auto& a : mAlignState.protectedAreas)
            align.appendChild (a.toValueTree(), nullptr);
        for (const auto& r : mAlignState.renders)
        {
            juce::ValueTree e ("R");
            e.setProperty ("f", r.file,      nullptr);
            e.setProperty ("d", r.dateIso,   nullptr);
            e.setProperty ("v", r.version,   nullptr);
            e.setProperty ("b", r.startBeat, nullptr);
            align.appendChild (e, nullptr);
        }
        for (const auto& ver : mAlignVersions)
        {
            juce::ValueTree e ("Ver");
            e.setProperty ("ts", ver.dateIso, nullptr);
            e.setProperty ("sa", ver.sigA,    nullptr);
            e.setProperty ("sb", ver.sigB,    nullptr);
            e.appendChild (ver.state.createCopy(), nullptr);
            align.appendChild (e, nullptr);
        }
        state.appendChild (align, nullptr);
    }

    // QA-F chain-wiring fix (2026-07-10): per-slot chain-DSP blobs (see the
    // kVocalChainStateTag note above).
    {
        juce::ValueTree chain (kVocalChainStateTag);
        for (int i = 0; i < 6; ++i)   // QA-Fe2: 6 locked slots
        {
            if (auto* eff = mVocalChainRack.getSlotEffect (i))
            {
                juce::MemoryBlock blob;
                eff->getStateInformation (blob);
                if (blob.getSize() > 0)
                    chain.setProperty ("s" + juce::String (i),
                                       blob.toBase64Encoding(), nullptr);
            }
        }
        state.appendChild (chain, nullptr);
    }

    // H-6d: embed the BaySickNAMIRProcessor's full state as a base64
    // string property under <NamIrState>.  ValueTree doesn't take MemoryBlock
    // values directly, so we serialize to base64.
    if (mNamIrProc)
    {
        juce::MemoryBlock namIrBlob;
        mNamIrProc->getStateInformation (namIrBlob);
        juce::ValueTree namIrChild (kNamIrStateTag);
        namIrChild.setProperty ("blob",
                                 namIrBlob.toBase64Encoding(),
                                 nullptr);
        state.appendChild (namIrChild, nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void BaySickVocalProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        if (! xml->hasTagName (apvts.state.getType()))
            return;

        // QA-Fd review fix: replaceState fires the align editor's param
        // hooks; the mode hook's queued Blend-preset seat would stomp the
        // restored Blend value.  Raise the restoring flag now; the queued
        // clear below runs AFTER every hook the restore posted.
        mRestoringState->store (true, std::memory_order_release);

        auto newState = juce::ValueTree::fromXml (*xml);

        // Pull NamIrState OUT of the tree before handing it to APVTS so
        // replaceState doesn't carry the transient blob into APVTS internals.
        auto namIrChild = newState.getChildWithName (kNamIrStateTag);
        if (namIrChild.isValid())
            newState.removeChild (namIrChild, nullptr);

        // QA-F Task 3: same for <AlignEdits> -- parsed into mAlignState below.
        auto alignChild = newState.getChildWithName (kAlignEditsTag);
        if (alignChild.isValid())
            newState.removeChild (alignChild, nullptr);

        // QA-F chain-wiring fix (2026-07-10): same for <VocalChainState>.
        auto chainChild = newState.getChildWithName (kVocalChainStateTag);
        if (chainChild.isValid())
            newState.removeChild (chainChild, nullptr);

        // QA-Fa: same for <PitchEdits>.
        auto pitchChild = newState.getChildWithName (kPitchEditsTag);
        if (pitchChild.isValid())
            newState.removeChild (pitchChild, nullptr);

        // Same for the A/B snapshots -- they are read back into mSnapshots
        // below and must not land inside the live APVTS tree, which the freeze
        // watcher listens to.
        auto slotAChild = newState.getChildWithName (kSlotATag);
        if (slotAChild.isValid())
            newState.removeChild (slotAChild, nullptr);
        auto slotBChild = newState.getChildWithName (kSlotBTag);
        if (slotBChild.isValid())
            newState.removeChild (slotBChild, nullptr);

        // QA-Fe migration: bsp_engine is net-new, so its absence marks a
        // pre-QA-Fe project whose bsa_pitch_algo still meant PSOLA/Granular/PV
        // (those engines are retired).  Default such projects to Rubber Band (0)
        // so a stale Granular/PV pick doesn't silently land on a different engine.
        const bool preQaFePitch = ! newState.getChildWithProperty (
            "id", juce::String ("bsp_engine")).isValid();

        apvts.replaceStateKeepingUndoHistory (newState);

        if (preQaFePitch)
        {
            // QA-UndoCoverage Task 6: load-path migration write, not history.
            juce::AudioProcessorValueTreeState::ScopedProgrammaticParamWrites spw;
            if (auto* p = apvts.getParameter ("bsa_pitch_algo"))
                p->setValueNotifyingHost (p->convertTo0to1 (0.0f));
        }

        // A/B: seed BOTH slots from the parameters just restored, then overlay
        // whatever each stored slot actually carries.  A project with no SlotA /
        // SlotB children (or a stored slot missing an id added since) therefore
        // loads with the tone it was saved with on both sides -- no migration.
        // No snapshot is applied back into APVTS here: the save captured the
        // active slot FROM these same parameters, so the two already agree.
        captureSnapshotFromCurrent (0);
        captureSnapshotFromCurrent (1);
        if (slotAChild.isValid()) mSnapshots[0].fromValueTree (slotAChild);
        if (slotBChild.isValid()) mSnapshots[1].fromValueTree (slotBChild);
        mLastSlot = activeAbSlot();

        mPitchRenders.clear();
        mPitchVersions.clear();
        mPitchAnalyzedSig = 0;
        if (pitchChild.isValid())
        {
            mPitchAnalyzedSig = (juce::int64) pitchChild.getProperty ("pSig", 0);
            mPitch.stateFromValueTree (pitchChild.getChildWithName ("PitchState"));
            for (int i = 0; i < pitchChild.getNumChildren(); ++i)
            {
                auto c = pitchChild.getChild (i);
                if (c.hasType ("R"))
                {
                    AlignRenderEntry r;
                    r.file      = c.getProperty ("f", juce::String()).toString();
                    r.dateIso   = c.getProperty ("d", juce::String()).toString();
                    r.version   = (int) c.getProperty ("v", 1);
                    r.startBeat = (double) c.getProperty ("b", 0.0);
                    mPitchRenders.push_back (r);
                }
                else if (c.hasType ("Ver"))
                {
                    EditorVersionEntry ver;
                    ver.dateIso = c.getProperty ("ts", juce::String()).toString();
                    ver.sigA    = (juce::int64) c.getProperty ("sa", 0);
                    ver.state   = c.getChildWithName ("PitchState").createCopy();
                    if (ver.state.isValid())
                        mPitchVersions.push_back (std::move (ver));
                }
            }
        }
        else
            mPitch.stateFromValueTree ({});

        // Restore the per-slot chain-DSP blobs IN PLACE on the existing DSP
        // instances (no slot reload -- the Vocal Chain panel holds raw DSP
        // pointers).  Bound controls get re-imposed from the params by the
        // per-block push; the blob supplies everything unbound.  Pre-fix
        // saves have no child -> chain DSPs keep their defaults, unchanged.
        if (chainChild.isValid())
        {
            for (int i = 0; i < 6; ++i)   // QA-Fe2: 6 locked slots
            {
                const juce::String b64 = chainChild.getProperty (
                    "s" + juce::String (i), juce::String()).toString();
                if (b64.isEmpty()) continue;
                juce::MemoryBlock blob;
                if (! blob.fromBase64Encoding (b64)) continue;
                if (auto* eff = mVocalChainRack.getSlotEffect (i))
                    eff->setStateInformation (blob.getData(), (int) blob.getSize());
            }
        }

        mAlignState = AlignState();
        mAlignVersions.clear();
        if (alignChild.isValid())
        {
            mAlignState.analyzed            = ((int) alignChild.getProperty ("analyzed", 0)) != 0;
            mAlignState.analyzedLeaderSig   = (juce::int64) alignChild.getProperty ("lSig", 0);
            mAlignState.analyzedFollowerSig = (juce::int64) alignChild.getProperty ("fSig", 0);
            mAlignState.commonStartBeat     = (double) alignChild.getProperty ("startBeat", 0.0);
            mAlignState.commonStartSample   = (juce::int64) alignChild.getProperty ("cSamp", 0);
            mAlignState.leaderPadSamples    = (juce::int64) alignChild.getProperty ("lPad", 0);
            mAlignState.followerPadSamples  = (juce::int64) alignChild.getProperty ("fPad", 0);
            mAlignState.analysisSampleRate  = (double) alignChild.getProperty ("sr", 44100.0);
            for (int i = 0; i < alignChild.getNumChildren(); ++i)
            {
                auto c = alignChild.getChild (i);
                if (c.hasType ("WarpMap"))
                    mAlignState.map.fromValueTree (c);
                else if (c.hasType ("SP"))
                    mAlignState.syncPoints.push_back (AlignSyncPoint::fromValueTree (c));
                else if (c.hasType ("PA"))
                    mAlignState.protectedAreas.push_back (AlignProtectedArea::fromValueTree (c));
                else if (c.hasType ("R"))
                {
                    AlignRenderEntry r;
                    r.file      = c.getProperty ("f", juce::String()).toString();
                    r.dateIso   = c.getProperty ("d", juce::String()).toString();
                    r.version   = (int) c.getProperty ("v", 1);
                    r.startBeat = (double) c.getProperty ("b", 0.0);
                    mAlignState.renders.push_back (r);
                }
                else if (c.hasType ("Ver"))
                {
                    EditorVersionEntry ver;
                    ver.dateIso = c.getProperty ("ts", juce::String()).toString();
                    ver.sigA    = (juce::int64) c.getProperty ("sa", 0);
                    ver.sigB    = (juce::int64) c.getProperty ("sb", 0);
                    ver.state   = c.getChildWithName ("AlignApplied").createCopy();
                    if (ver.state.isValid())
                        mAlignVersions.push_back (std::move (ver));
                }
            }
            if (mAlignState.analyzed && mAlignState.map.isValid())
                mAlign.setWarpMap (mAlignState.map);
            else
                mAlign.clearWarpMap();
        }
        else
            mAlign.clearWarpMap();

        // QA-Fa recovery: a restored applied map goes live immediately.
        // Saves predating cSamp (the d8cc9494 checkpoint era) stored only
        // the origin beat -- derive the sample through the tempo timeline
        // (message thread; the standalone always publishes one).
        if (mAlignState.analyzed && mAlignState.commonStartSample == 0
            && mAlignState.commonStartBeat > 0.0 && TempoMap::isActive())
            mAlignState.commonStartSample =
                TempoMap::sampleAtBeat (mAlignState.commonStartBeat);
        // QA-Fd: replaceState above fired the editor's knob hooks (attachment
        // sync), bumping the settings generation -- re-baseline so a freshly
        // restored project doesn't read as knob-stale.  Same for the pitch
        // time-map hash (the restored map was analyzed against the restored
        // edits by construction).
        mAlignState.analyzedSettingsGen  = alignSettingsGeneration();
        mAlignState.analyzedPitchMapHash = followerPitchMapHash();
        publishAlignPlayback();

        // H-6d: restore the embedded NAM/IR state.  Pre-H-6d projects have
        // no <NamIrState> child; the per-Vox NAM/IR processor stays at its
        // construction defaults in that case.
        if (namIrChild.isValid() && mNamIrProc)
        {
            const juce::String b64 = namIrChild.getProperty ("blob",
                                                              juce::String()).toString();
            if (b64.isNotEmpty())
            {
                juce::MemoryBlock blob;
                if (blob.fromBase64Encoding (b64))
                    mNamIrProc->setStateInformation (blob.getData(), (int) blob.getSize());
            }
        }

        // QA-F chain-wiring fix (2026-07-10): let the Vocal Chain sub-tab
        // re-mount its slot editors against the restored DSP state.
        if (onChainStateRestored)
            onChainStateRestored();

        // Queued AFTER every hook the restore posted (message-queue order);
        // the lambda co-owns the flag so a torn-down processor is safe.
        juce::MessageManager::callAsync (
            [tok = mRestoringState] { tok->store (false, std::memory_order_release); });
    }
}
