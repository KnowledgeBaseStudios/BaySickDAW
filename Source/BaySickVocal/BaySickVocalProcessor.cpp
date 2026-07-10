#include "BaySickVocalProcessor.h"
#include "../AudioFileRecorder.h"   // I-16 G-9: wet recorder tap inside processBlock
#include "../TempoMapRead.h"        // QA-Fa recovery: legacy-save origin-sample derivation
#include "BaySickVocalEditor.h"
#include <algorithm>
#include "../DSP/DeEsserDSP.h"
#include "../DSP/CompressorDSP.h"
#include "../DSP/SaturationDSP.h"
#include "../DSP/LimiterDSP.h"

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
    addF ("mix",      "Mix",            0.0f, 1.0f, 1.0f);   // global wet/dry
    addB ("bypass",   "Bypass",         false);              // entire processor
    addI ("ab_slot",  "A/B Slot",       0,    1,   0);       // A/B compare snapshot

    // ── Realtime pitch correction stage (BaySickVocals tab bottom half) ─────
    // Default OFF per spec - user opts in via the bypass toggle.
    addB ("pitch_realtime_bypass",  "Pitch Realtime Bypass",  true);

    // H-5 (2026-05-01) -- Pitch correction knobs.
    // Key: 0..11 (C, C#, D, D#, E, F, F#, G, G#, A, A#, B)
    // Scale: 0=Chromatic, 1=Major, 2=Minor, 3=HarmonicMinor, 4=Dorian,
    //        5=Mixolydian, 6=Phrygian, 7=Lydian, 8=Locrian, 9=Custom
    addI ("pitch_key",          "Pitch Key",      0,    11,   0);
    addI ("pitch_scale",        "Pitch Scale",    0,    9,    0);
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
    // + harmonic-routing mode).  Existing presets default to Tube + body off
    // + Normal harmonics -> audio identical to pre-H-7.
    addI ("sat_type",          "Saturation Type",          0, 1, 0);   // 0=Tube, 1=Console
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
    addAI ("align_mode",      "Align Mode",          0, 2, 1);    // 0=Loose, 1=Close, 2=Tight
    addAF ("align_fineTune",  "Align Fine Tune ms",  -50.0f, 50.0f, 0.0f);  // bipolar around Mode base
    addAB ("pitch_on",        "Align Pitch On",      false);
    addAF ("pitch_range",     "Align Pitch Range",   0.0f, 100.0f, 50.0f); // % of leader contour
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
    addPI ("preset",       "Pitch Preset",       0, 3, 0);   // Loose/Close/Tight/User
    addPB ("preset_dirty", "Pitch Preset Dirty", false);
    addPI ("mode",         "Pitch Edit Mode",    0, 1, 1);   // 0=Slice, 1=Edit
    addPB ("on",           "Pitch Chain On",     true);      // QA-Fa recovery: chain switch

    return { params.begin(), params.end() };
}

// ─── Constructor ──────────────────────────────────────────────────────────────
BaySickVocalProcessor::BaySickVocalProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    , apvts (*this, nullptr, "BaySickVocalState", createLayout())
{
    // H-6d (2026-05-02): per-Vox-strip BaySickNAM/IR processor lives on
    // the Vox processor so its state is captured by getStateInformation
    // (page preset save/load picks it up automatically).
    mNamIrProc = std::make_unique<BaySickNAMIRProcessor>();

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
}

// ─── Destructor ───────────────────────────────────────────────────────────────
// 2026-05-06 (Batch 9c N1): audio-thread shutdown safety net.  Owners should
// ideally pre-flag teardown via setShuttingDown(true) followed by ~30 ms of
// sleep (mirrors VibeSynthProcessor's mProjectLoadInProgress barrier in
// StandaloneEditor::closeAllDynamicTabs) so the audio thread sees the flag
// before any member starts dying.  Setting it here as well so we still bail
// on subsequent processBlock entries even if the owner forgot.  Without this,
// the audio thread's mNamIrProc->processBlock(...) dispatches through a
// vtable already zeroed by ~BaySickNAMIRProcessor and crashes.
BaySickVocalProcessor::~BaySickVocalProcessor()
{
    mShuttingDown.store (true, std::memory_order_release);
    // QA-Fa recovery: owners raise the shutdown gate (and the clip snapshot
    // rebuild drops the channel's players) before teardown, so the decode
    // layer cannot be inside a block holding this pointer here.
    delete mAlignPlayActive.exchange (nullptr);
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

    // H-6c -- pre-load the vocal chain rack with 4 locked slots in order.
    // loadEffect is idempotent on type match; second prepareToPlay calls
    // (e.g. sample-rate change) just re-prepare the existing slots.
    if (mVocalChainRack.getSlotType (0) != EffectType::DeEsser)
        mVocalChainRack.loadEffect (0, EffectType::DeEsser);
    if (mVocalChainRack.getSlotType (1) != EffectType::Compressor)
        mVocalChainRack.loadEffect (1, EffectType::Compressor);
    if (mVocalChainRack.getSlotType (2) != EffectType::Saturation)
        mVocalChainRack.loadEffect (2, EffectType::Saturation);
    if (mVocalChainRack.getSlotType (3) != EffectType::Limiter)
        mVocalChainRack.loadEffect (3, EffectType::Limiter);
    mVocalChainRack.prepare (sampleRate, maxBlockSize);

    mDryScratch.setSize (2, maxBlockSize, false, false, true);
    mDryScratch.clear();
    mWetMonoScratch.setSize (1, maxBlockSize, false, false, true);
    mWetMonoScratch.clear();

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
    mPitch.setChainOn   (rdb ("bsp_on"));

    // ── Rack stage Bypass flags (forward to slots) ────────────────────────
    mVocalChainRack.setSlotBypassed (0, rdb ("bsv_deesser_bypass"));
    mVocalChainRack.setSlotBypassed (1, rdb ("bsv_comp_bypass"));
    mVocalChainRack.setSlotBypassed (2, rdb ("bsv_sat_bypass"));
    mVocalChainRack.setSlotBypassed (3, rdb ("bsv_limiter_bypass"));

    // ── De-esser (slot 0) ──────────────────────────────────────────────────
    // QA-F chain-wiring fix (2026-07-10): modeBlend replaces the removed
    // legacy Int mode; the new engine/quality params ride the same push.
    if (auto* de = dynamic_cast<DeEsserDSP*> (mVocalChainRack.getSlotEffect (0)))
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

    // ── Compressor (slot 1) ────────────────────────────────────────────────
    if (auto* cp = dynamic_cast<CompressorDSP*> (mVocalChainRack.getSlotEffect (1)))
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

    // ── Saturation (slot 2) ────────────────────────────────────────────────
    // H-7 (2026-05-01): Type umbrella + Vocal Body.  Other knobs (drive,
    // tube type, etc.) still run at defaults until a polish pass exposes
    // them in the BaySickVocal UI; the existing rack panel reads them too.
    if (auto* sat = dynamic_cast<SaturationDSP*> (mVocalChainRack.getSlotEffect (2)))
    {
        sat->setSatType        (rdi ("bsv_sat_type"));
        sat->setVocalBody      (rdb ("bsv_sat_vocalBody"));
        sat->setHarmonicsMode  (rdi ("bsv_sat_harmonicsMode"));
    }

    // ── Limiter (slot 3) ───────────────────────────────────────────────────
    // QA-F chain-wiring fix (2026-07-10): full knob push (was "polish pass"
    // deferred -- the panel's edits neither persisted nor had params).
    if (auto* lim = dynamic_cast<LimiterDSP*> (mVocalChainRack.getSlotEffect (3)))
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
    // Mirrors VibeSynthProcessor::mProjectLoadInProgress check at the top
    // of its processBlock.
    if (mShuttingDown.load (std::memory_order_acquire))
    {
        buffer.clear();
        return;
    }

    // Master bypass - entire processor passes input straight to output unchanged.
    const bool masterBypass = apvts.getRawParameterValue ("bsv_bypass")->load() > 0.5f;
    mScHelper.updateLevel (numSamples);
    if (masterBypass)
    {
        // QA-Fb: bypass = raw passthrough, so the monitor merge applies raw --
        // prior takes stay audible exactly like pure playback under bypass
        // (finalizeFilePlayStrip's engine call passes them through unchanged).
        if (monMuteLive)
            buffer.clear();
        if (monTakes != nullptr)
        {
            const int nc = juce::jmin (numChannels, monTakes->getNumChannels());
            for (int ch = 0; ch < nc; ++ch)
                buffer.addFrom (ch, 0, *monTakes, ch, 0, numSamples);
        }
        return;
    }

    // H-6 (2026-05-01) -- push APVTS to DSP stages once per block.
    pushApvtsToDsp();

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
    }

    // ── Run the locked vocal chain in order ──────────────────────────────
    // input -> pitch correction -> [WET TAP] -> [rack: deess->comp->sat->lim]
    //       -> NAM/IR -> output
    // I-16 G-9 (2026-05-03): pitch correction skipped when the source mux is
    // FilePlay (force-bypass) so a wet file with realtime pitch already
    // committed at record time doesn't get corrected twice.  NAM/IR routing
    // also added in G-9.
    const bool filePlaySrc = mForcePitchBypass.load (std::memory_order_acquire);
    if (! filePlaySrc)
        mPitchCorrector.process (buffer);
    else
        // QA-Fa Mode C applicator: FilePlay only (mutually exclusive with the
        // realtime corrector) -- note-level edits ride normal playback with
        // no bake step.  One atomic load when the channel has no edits.
        mPitch.processFilePlay (buffer,
            mFilePlayTimelineSample.load (std::memory_order_relaxed));

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
        if (auto* wetRec = mWetRecorder.load (std::memory_order_acquire))
        {
            // Sum stereo to mono for the recorder (dry source was a mono ASIO
            // channel duplicated to L=R upstream; pitch correction may have
            // diverged the channels slightly).
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
            float* monoPtrs[1] = { dst };
            juce::AudioBuffer<float> monoView (monoPtrs, 1, numSamples);
            wetRec->writeBlock (monoView);
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
            mPitch.processFilePlayMonitor (*monTakes, monTimeline);
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
    int v = -1;
    if (auto* p = apvts.getRawParameterValue ("bsa_follower_channel"))
        v = (int) p->load();
    return (v >= 0) ? v : mOwnChannelId;
}

double BaySickVocalProcessor::alignToleranceSec() const
{
    int mode = 1;
    float fine = 0.0f;
    if (auto* p = apvts.getRawParameterValue ("bsa_align_mode"))     mode = (int) p->load();
    if (auto* p = apvts.getRawParameterValue ("bsa_align_fineTune")) fine = p->load();
    // Section 13e: Loose base 150 ms / Close 100 / Tight 50, +/-50 Fine Tune.
    const float base = (mode == 0) ? 150.0f : (mode == 2) ? 50.0f : 100.0f;
    return (double) juce::jmax (5.0f, base + fine) / 1000.0;
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

    bool  pitchOn = false;
    float range01 = 0.0f;
    if (auto* p = apvts.getRawParameterValue ("bsa_pitch_on"))    pitchOn = p->load() > 0.5f;
    if (auto* p = apvts.getRawParameterValue ("bsa_pitch_range")) range01 = p->load() / 100.0f;

    auto map = BaySickAlignDSP::analyzeOffline (
        guide.getReadPointer (0), guide.getNumSamples(),
        dub  .getReadPointer (0), dub.getNumSamples(),
        gSr, 1.0f, alignToleranceSec(),
        mAlignState.syncPoints, mAlignState.protectedAreas,
        pitchOn ? juce::jlimit (0.0f, 1.0f, range01) : 0.0f);

    if (! map.isValid())
        { errorOut = "Not enough matching transients to align - try a looser Mode."; return false; }

    mAlignState.map                 = map;
    mAlignState.analyzedLeaderSig   = onChannelClipSignature (leader);
    mAlignState.analyzedFollowerSig = onChannelClipSignature (follower);
    mAlignState.commonStartBeat     = commonBeat;
    mAlignState.commonStartSample   = commonSamp;
    mAlignState.leaderPadSamples    = gPad;
    mAlignState.followerPadSamples  = dPad;
    mAlignState.analysisSampleRate  = gSr;
    mAlignState.analyzed            = true;
    mAlign.setWarpMap (map);

    // QA-Fa recovery: Analyze IS Apply -- version the committed state and
    // publish it to the decode layer so playback picks it up this block.
    appendAlignVersion();
    publishAlignPlayback();
    if (auto& fn = mDirtyTracker.onAny) fn();
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
    publishAlignPlayback();
    if (auto& fn = mDirtyTracker.onAny) fn();
    return true;
}

bool BaySickVocalProcessor::revertPitchToVersion (int index)
{
    if (index < 0 || index >= (int) mPitchVersions.size()) return false;
    mPitch.stateFromValueTree (mPitchVersions[(size_t) index].state);
    mPitchAnalyzedSig = mPitchVersions[(size_t) index].sigA;
    if (auto& fn = mDirtyTracker.onAny) fn();
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
            snap->guideSec  .push_back (a.guideTimeSec);
            snap->dubSec    .push_back (d);
            snap->pitchSemis.push_back (a.pitchSemis);
            if (std::abs (a.pitchSemis) > 0.01f) snap->anyPitch = true;
            lastDub = d;
        }
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

juce::AudioBuffer<float> BaySickVocalProcessor::buildWarpedFollower (juce::String& errorOut)
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
    WarpMap effMap = mAlignState.map;
    if (! alignOn)
        for (auto& a : effMap.anchors)
            a.guideTimeSec = a.dubTimeSec;
    if (! pitchOn)
        for (auto& a : effMap.anchors)
            a.pitchSemis = 0.0f;

    auto warped = BaySickAlignDSP::applyWarp (
        dub.getReadPointer (0), dub.getNumSamples(),
        mAlignState.analysisSampleRate, effMap, algo,
        pitchOn ? (float) transpose : 0.0f);

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

juce::File BaySickVocalProcessor::renderAlignedTake (juce::String& errorOut)
{
    errorOut.clear();
    if (! onGetProjectFolder)
        { errorOut = "Align services not connected."; return {}; }

    auto warped = buildWarpedFollower (errorOut);
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
    std::unique_ptr<juce::AudioFormatWriter> writer (fmt.createWriterFor (
        os.release(), mAlignState.analysisSampleRate, 1, 24, {}, 0));
    if (writer == nullptr) { errorOut = "Could not create the WAV writer."; return {}; }
    writer->writeFromAudioSampleBuffer (warped, 0, warped.getNumSamples());
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
    writer->writeFromAudioSampleBuffer (rendered, 0, rendered.getNumSamples());
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

// ─── Editor ───────────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* BaySickVocalProcessor::createEditor()
{
    // H-6 (2026-05-01) -- real 6-sub-tab editor lands.
    return new BaySickVocalEditor (*this);
}

// ─── State save / load ────────────────────────────────────────────────────────
//
// H-6d (2026-05-02): captures the entire 6-sub-tab Vox page state, so the
// hamburger menu's Save Page Preset / Load Page Preset round-trips
// faithfully.  Captured state per sub-tab:
//
//   BaySickVocals      -> APVTS (bsv_pitch_*, bsv_mix, bsv_bypass, etc.)
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
}

void BaySickVocalProcessor::getStateInformation (juce::MemoryBlock& dest)
{
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
        for (int i = 0; i < 4; ++i)
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

        apvts.replaceState (newState);

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
            for (int i = 0; i < 4; ++i)
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
    }
}
