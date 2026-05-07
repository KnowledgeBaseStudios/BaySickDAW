#include "BaySickVocalProcessor.h"
#include "../AudioFileRecorder.h"   // I-16 G-9: wet recorder tap inside processBlock
#include "BaySickVocalEditor.h"
#include "../DSP/DeEsserDSP.h"
#include "../DSP/CompressorDSP.h"
#include "../DSP/SaturationDSP.h"
#include "../DSP/LimiterDSP.h"

// ─────────────────────────────────────────────────────────────────────────────
// BaySickVocalProcessor — Phase H-1 skeleton (2026-05-01)
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
    // Default OFF per spec — user opts in via the bypass toggle.
    addB ("pitch_realtime_bypass",  "Pitch Realtime Bypass",  true);

    // H-5 (2026-05-01) -- Pitch correction knobs.
    // Key: 0..11 (C, C#, D, D#, E, F, F#, G, G#, A, A#, B)
    // Scale: 0=Chromatic, 1=Major, 2=Minor, 3=HarmonicMinor, 4=Dorian,
    //        5=Mixolydian, 6=Phrygian, 7=Lydian, 8=Locrian, 9=Custom
    addI ("pitch_key",          "Pitch Key",      0,    11,   0);
    addI ("pitch_scale",        "Pitch Scale",    0,    9,    0);
    addF ("pitch_retuneSpeed",  "Retune Speed ms", 0.0f, 100.0f, 50.0f);
    addF ("pitch_strength",     "Pitch Strength",  0.0f, 1.0f,   1.0f);
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
    addF ("comp_ratio",      "Compressor Ratio",        1.0f,  20.f, 4.f);
    addF ("comp_attack",     "Compressor Attack ms",    0.02f, 400.f, 10.f);
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

    // H-3 (2026-05-01) -- De-esser stage params.  New DeEsserDSP module
    // (split-band sidechain HPF + dynamic notch + envelope + range cap +
    // M/S optional + lookahead + listen mode + GR meter).
    addI ("deesser_mode",        "De-esser Mode",      0,    1,    0);   // 0=Wide, 1=Split
    addI ("deesser_msMode",      "De-esser M/S",       0,    2,    0);   // 0=Stereo, 1=Mid, 2=Side
    addF ("deesser_freq",        "De-esser Frequency", 4000.f, 12000.f, 6500.f);
    addF ("deesser_q",           "De-esser Q",         0.5f,  4.0f,    1.4f);
    addF ("deesser_threshold",   "De-esser Threshold dB", -40.f, 0.f, -24.f);
    addF ("deesser_range",       "De-esser Range dB", -20.f, 0.f, -12.f);
    addF ("deesser_attack",      "De-esser Attack ms",  0.1f,  30.f,  1.f);
    addF ("deesser_release",     "De-esser Release ms", 10.f, 500.f, 80.f);
    addF ("deesser_lookahead",   "De-esser Lookahead ms", 0.f, 5.f,   0.f);
    addF ("deesser_mix",         "De-esser Mix",        0.f,   1.f,   1.f);
    addB ("deesser_listen",      "De-esser SC Listen",  false);

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
}

// ─── Audio lifecycle ──────────────────────────────────────────────────────────
void BaySickVocalProcessor::prepareToPlay (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;
    mPrepared   = true;

    // H-6 (2026-05-01) -- prepare pitch corrector + vocal chain rack.
    mPitchCorrector.prepare (sampleRate, maxBlockSize);

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

    // ── Rack stage Bypass flags (forward to slots) ────────────────────────
    mVocalChainRack.setSlotBypassed (0, rdb ("bsv_deesser_bypass"));
    mVocalChainRack.setSlotBypassed (1, rdb ("bsv_comp_bypass"));
    mVocalChainRack.setSlotBypassed (2, rdb ("bsv_sat_bypass"));
    mVocalChainRack.setSlotBypassed (3, rdb ("bsv_limiter_bypass"));

    // ── De-esser (slot 0) ──────────────────────────────────────────────────
    if (auto* de = dynamic_cast<DeEsserDSP*> (mVocalChainRack.getSlotEffect (0)))
    {
        de->setMode         (rdi ("bsv_deesser_mode"));
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

    // Limiter (slot 3) -- knob wiring lands in a polish pass.  Bypass flag
    // above is already pushed; runs at default settings.
}

void BaySickVocalProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
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

    // Master bypass — entire processor passes input straight to output unchanged.
    const bool masterBypass = apvts.getRawParameterValue ("bsv_bypass")->load() > 0.5f;
    mScHelper.updateLevel (numSamples);
    if (masterBypass) return;

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
    if (! mForcePitchBypass.load (std::memory_order_acquire))
        mPitchCorrector.process (buffer);

    // ── Wet recording tap (post-realtime pitch / pre-vocal-chain) ────────
    // This is the user-locked tap point per Option C of G-9.1: the file
    // captures realtime-pitch-applied audio so playback bypasses realtime
    // (single pass) while everything from the vocal chain rack onwards
    // stays dynamic across sessions.
    if (auto* wetRec = mWetRecorder.load (std::memory_order_acquire))
    {
        // Sum stereo to mono for the recorder (dry source was a mono ASIO
        // channel duplicated to L=R upstream; pitch correction may have
        // diverged the channels slightly).
        const float invCh = (numChannels > 1) ? (1.0f / (float) numChannels) : 1.0f;
        juce::AudioBuffer<float> monoView (1, numSamples);
        float* dst = monoView.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                s += buffer.getReadPointer (ch)[i];
            dst[i] = s * invCh;
        }
        wetRec->writeBlock (monoView);
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
//   BaySickPitch       -> <PitchEdits> ValueTree child (empty until G-9
//                          ships the offline editor's data model; slot
//                          reserved here so future G-9 storage round-
//                          trips through this same save path)
//   BaySickAlign       -> <AlignEdits> ValueTree child (same pattern)
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

    // H-6b/c slots reserved for future G-9 edit-data persistence.  Empty
    // ValueTrees today; G-9 populates them when the offline editors gain
    // an edit data model.
    state.appendChild (juce::ValueTree (kPitchEditsTag), nullptr);
    state.appendChild (juce::ValueTree (kAlignEditsTag), nullptr);

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

        apvts.replaceState (newState);

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

        // <PitchEdits> + <AlignEdits> are placeholders today.  G-9 will
        // populate them via offline-editor data models that live on this
        // processor; setStateInformation will then read those children
        // back into the data models here.
    }
}
