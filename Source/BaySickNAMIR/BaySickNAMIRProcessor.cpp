#include "BaySickNAMIRProcessor.h"
#include "SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)
#include "SafeNamModel.h"   // .nam dimension / prewarm gate (QA-Cleanup)
#include "SafeAudioReader.h"   // channel/frame sanity gate (QA-Cleanup)
#include "../AppPaths.h"
#include "BaySickNAMIREditor.h"
#include "../MissingFileReport.h"   // QA-Export Task 5
#include "../ProjectFileResolver.h"

// NAM core (C++20 internal; consumer translation unit is C++17 - only the
// header pulls in Eigen / nlohmann, both of which are C++17-clean).
#include <NAM/get_dsp.h>
#include <NAM/dsp.h>

#include <cmath>
#include <filesystem>
#include <stdexcept>
#include "SafeAudioFormats.h"   // MP3 decode via vendored LAME (QA-Cleanup)

// 2026-05-05 (Bug C diagnostics): one-off file logger for the get/set state
// trace.  Writes lines to Documents/BaySickDAW/namir_state_log.txt,
// append-mode.
//
// Debug build only (Jeff, 2026-08-07).  It fires on every project save AND
// load with no cap or rotation, so a shipping build would grow a file the user
// never asked for and cannot clear from inside the app.  The Release stub
// keeps the call sites compiling unchanged.
namespace
{
   #if JUCE_DEBUG
    void namirLog (const juce::String& line)
    {
        const auto logFile = AppPaths::appRoot()
                                 .getChildFile ("namir_state_log.txt");
        logFile.getParentDirectory().createDirectory();
        const auto stamped = juce::Time::getCurrentTime().toString (false, true, true, true)
                              + "  " + line + juce::newLine;
        logFile.appendText (stamped);
    }
   #else
    void namirLog (const juce::String&) {}
   #endif
}

// ─────────────────────────────────────────────────────────────────────────────
// BaySickNAMIRProcessor - Phase G-1.3 implementation.
// ─────────────────────────────────────────────────────────────────────────────

BaySickNAMIRProcessor::BaySickNAMIRProcessor (juce::UndoManager* undoMgr)
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, undoMgr, "BaySickNAMIR", createLayout())
{
    // Explicit atomic init - see the comment on mNamSwapPending in the header.
    for (auto& a : mNamSwapPending) a.store (false);
    for (auto& a : mNamLoaded)      a.store (false);
    for (auto& a : mNamIsFullRig)   a.store (false);
    for (auto& a : mIrLoaded)       a.store (false);

    // Listen for OS factor changes so we can re-Reset the NAM models on the
    // message thread (prepareToPlay won't re-fire when only a param changes).
    apvts.addParameterListener ("oversampling", this);

    // H-6d: per-slot A/B snapshot listener.  Fires on the writing thread
    // whenever ab_slot changes (UI click OR automation replay).  parameterChanged
    // restores the incoming slot's snapshot, and on a USER flip first captures
    // the outgoing slot's tone state -- an automation replay switches without
    // capturing, and a state restore in flight (mRestoringState) does neither.
    // Knobs auto-update via APVTS attachment.
    apvts.addParameterListener ("ab_slot", this);
    mLastSlot = getActiveSlot();
}

BaySickNAMIRProcessor::~BaySickNAMIRProcessor()
{
    // Dropped FIRST: any parameterChanged post already sitting in the message
    // queue holds a weak copy of this token and bails out once it expires.
    mLife.reset();

    apvts.removeParameterListener ("oversampling", this);
    apvts.removeParameterListener ("ab_slot", this);
}

juce::AudioProcessorEditor* BaySickNAMIRProcessor::createEditor()
{
    return new BaySickNAMIREditor (*this);
}

juce::AudioProcessorValueTreeState::ParameterLayout
BaySickNAMIRProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using B = juce::AudioParameterBool;
    using C = juce::AudioParameterChoice;
    using PID = juce::ParameterID;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ── Gain stages ──────────────────────────────────────────────────────────
    layout.add (std::make_unique<F> (PID ("input_gain", 1), "Input Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<F> (PID ("output", 1), "Master Output",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    // ── Noise gate ───────────────────────────────────────────────────────────
    layout.add (std::make_unique<F> (PID ("gate_threshold", 1), "Gate Threshold",
        juce::NormalisableRange<float> (-60.0f, 0.0f, 0.5f), -50.0f));
    layout.add (std::make_unique<F> (PID ("gate_release", 1), "Gate Release",
        juce::NormalisableRange<float> (5.0f, 500.0f, 1.0f), 100.0f));

    // ── Bypass toggles ───────────────────────────────────────────────────────
    layout.add (std::make_unique<B> (PID ("nam_bypass", 1), "NAM Bypass", false));
    layout.add (std::make_unique<B> (PID ("cab_bypass", 1), "Cab Bypass", false));

    // ── Pre-convolution filters ──────────────────────────────────────────────
    layout.add (std::make_unique<F> (PID ("low_cut", 1), "Low Cut",
        juce::NormalisableRange<float> (20.0f, 500.0f, 1.0f), 20.0f));
    layout.add (std::make_unique<F> (PID ("high_cut", 1), "High Cut",
        juce::NormalisableRange<float> (3000.0f, 20000.0f, 10.0f), 20000.0f));

    // ── Cabinet mix ──────────────────────────────────────────────────────────
    layout.add (std::make_unique<F> (PID ("cab_mix", 1), "Cabinet Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f));

    // ── Oversampling ─────────────────────────────────────────────────────────
    layout.add (std::make_unique<C> (PID ("oversampling", 1), "Oversampling",
        juce::StringArray { "1x", "2x", "4x" }, 0));

    // ── A/B compare slot ─────────────────────────────────────────────────────
    layout.add (std::make_unique<C> (PID ("ab_slot", 1), "A/B Slot",
        juce::StringArray { "A", "B" }, 0));

    // ── H-6d Mic Sim ─────────────────────────────────────────────────────────
    // TWO choices, not three (Jeff, 2026-08-11).  "None" was the mic being off,
    // which is now what the Mic A Active switch says -- and on Mic B it said it
    // twice, once here and once on that mic's own switch.  Index 0 is Built-in,
    // so the DSP mode is index + 1 (see processBlock); MicSimDSP::Mode::None is
    // now unreachable from the UI and is reached by the switch instead.
    layout.add (std::make_unique<B> (PID ("nam_mica_active", 1), "Mic A Active", false));
    layout.add (std::make_unique<C> (PID ("nam_micsim_mode", 1), "Mic Sim Mode",
        juce::StringArray { "Built-in", "User IR" }, 0));
    layout.add (std::make_unique<C> (PID ("nam_micsim_model", 1), "Mic Sim Model",
        MicSimDSP::modelDisplayNames(), 0));
    layout.add (std::make_unique<F> (PID ("nam_micsim_mix", 1), "Mic Sim Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f));

    // ── H-6d Mic Placement ───────────────────────────────────────────────────
    layout.add (std::make_unique<F> (PID ("nam_placement_distance_cm", 1), "Mic Distance",
        juce::NormalisableRange<float> (1.0f, 150.0f, 0.5f), 30.0f));
    layout.add (std::make_unique<F> (PID ("nam_placement_angle_deg", 1), "Mic Angle",
        juce::NormalisableRange<float> (-90.0f, 90.0f, 0.5f), 0.0f));
    // Vertical offset from the centre of the cone (Jeff, 2026-08-11).  The
    // engine still takes ONE off-axis angle; height is combined with Angle in
    // processBlock, which is why MicPlacementDSP needed no change.  0 is dead
    // centre, so a project saved before this loads sounding identical.
    layout.add (std::make_unique<F> (PID ("nam_placement_height_cm", 1), "Mic Height",
        juce::NormalisableRange<float> (-30.0f, 30.0f, 0.5f), 0.0f));
    layout.add (std::make_unique<C> (PID ("nam_placement_polar", 1), "Polar Pattern",
        juce::StringArray { "Omni", "Cardioid", "Supercardioid",
                              "Hypercardioid", "Figure-8" }, 1));
    layout.add (std::make_unique<F> (PID ("nam_placement_mix", 1), "Mic Placement Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f));

    // ── QA-Fc dual-mic (Mic B - parallel path, summed) ───────────────────────
    layout.add (std::make_unique<B> (PID ("nam_micb_active", 1), "Mic B Active", false));
    layout.add (std::make_unique<C> (PID ("nam_micsim_b_mode", 1), "Mic B Sim Mode",
        juce::StringArray { "Built-in", "User IR" }, 0));
    layout.add (std::make_unique<C> (PID ("nam_micsim_b_model", 1), "Mic B Sim Model",
        MicSimDSP::modelDisplayNames(), 0));
    layout.add (std::make_unique<F> (PID ("nam_micsim_b_mix", 1), "Mic B Sim Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f));
    layout.add (std::make_unique<F> (PID ("nam_placement_b_distance_cm", 1), "Mic B Distance",
        juce::NormalisableRange<float> (1.0f, 150.0f, 0.5f), 30.0f));
    layout.add (std::make_unique<F> (PID ("nam_placement_b_height_cm", 1), "Mic B Height",
        juce::NormalisableRange<float> (-30.0f, 30.0f, 0.5f), 0.0f));
    layout.add (std::make_unique<F> (PID ("nam_placement_b_angle_deg", 1), "Mic B Angle",
        juce::NormalisableRange<float> (-90.0f, 90.0f, 0.5f), 0.0f));
    layout.add (std::make_unique<C> (PID ("nam_placement_b_polar", 1), "Mic B Polar Pattern",
        juce::StringArray { "Omni", "Cardioid", "Supercardioid",
                              "Hypercardioid", "Figure-8" }, 1));
    layout.add (std::make_unique<F> (PID ("nam_placement_b_mix", 1), "Mic B Placement Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f));

    return layout;
}

// ─────────────────────────────────────────────────────────────────────────────
// Slot helpers.
// ─────────────────────────────────────────────────────────────────────────────
int BaySickNAMIRProcessor::getActiveSlot() const
{
    if (auto* p = apvts.getRawParameterValue ("ab_slot"))
        return juce::jlimit (0, 1, (int) p->load());
    return 0;
}

int BaySickNAMIRProcessor::resolveSlot (int slot) const
{
    if (slot < 0) slot = getActiveSlot();
    return juce::jlimit (0, 1, slot);
}

bool         BaySickNAMIRProcessor::hasNamModel        (int slot) const { return mNamLoaded   [(size_t) resolveSlot (slot)].load(); }
bool         BaySickNAMIRProcessor::hasIr              (int slot) const { return mIrLoaded    [(size_t) resolveSlot (slot)].load(); }
bool         BaySickNAMIRProcessor::isFullRig          (int slot) const { return mNamIsFullRig[(size_t) resolveSlot (slot)].load(); }
juce::String BaySickNAMIRProcessor::getNamFilePath     (int slot) const { return mNamPaths    [(size_t) resolveSlot (slot)]; }
juce::String BaySickNAMIRProcessor::getIrFilePath      (int slot) const { return mIrPaths     [(size_t) resolveSlot (slot)]; }

// ─────────────────────────────────────────────────────────────────────────────
// prepareToPlay - size scratch + warm filters / convolution / NAM / OS / gate.
// ─────────────────────────────────────────────────────────────────────────────
void BaySickNAMIRProcessor::prepareToPlay (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;
    mPrepared   = true;

    juce::dsp::ProcessSpec stereoSpec { sampleRate, (juce::uint32) maxBlockSize, 2 };

    for (auto& ir : mIr)   { ir.reset(); ir.prepare (stereoSpec); }

    mLowCut .prepare (stereoSpec);
    mHighCut.prepare (stereoSpec);
    updateLowCutCoeffs  (20.0f);
    updateHighCutCoeffs (20000.0f);

    rebuildOversampling();   // (re)allocate 2x + 4x stage chains for current block

    // H-6d post-IR stages: prepare with stereo, host block size.
    mMicSim       .prepare (sampleRate, maxBlockSize, 2);
    mMicPlacement .prepare (sampleRate, maxBlockSize, 2);
    mMicSimB      .prepare (sampleRate, maxBlockSize, 2);
    mMicPlacementB.prepare (sampleRate, maxBlockSize, 2);

    // Scratch - sized to the LARGEST block we might see, including 4x oversampling.
    const int maxBlockOS = maxBlockSize * 4;
    mNamMonoIn .assign ((size_t) maxBlockOS, 0.0);
    mNamMonoOut.assign ((size_t) maxBlockOS, 0.0);
    mMonoFloatBuf.setSize (1, maxBlockSize, false, false, true);
    mDryBuf      .setSize (2, maxBlockSize, false, false, true);
    mPreMicScratch.setSize (2, maxBlockSize, false, false, true);
    mMicBScratch  .setSize (2, maxBlockSize, false, false, true);

    // Re-Reset whichever NAM models are loaded, at the active OS rate.
    int osFactor = 0;
    if (auto* p = apvts.getRawParameterValue ("oversampling"))
        osFactor = juce::jlimit (0, 2, (int) p->load());
    const double osRate     = mSampleRate * (double) (1 << osFactor);
    const int    osMaxBlock = mMaxBlock   * (1 << osFactor);
    for (auto& nam : mNamActive)
    {
        if (nam)
        {
            try { nam->ResetAndPrewarm (osRate, osMaxBlock); }
            catch (...) { /* model rejects rate - leave loaded but it'll glitch */ }
        }
    }

    // Gate state - invalidate cache so first block recomputes.
    mGateEnv           = 0.0f;
    mLastGateThreshDb  = std::numeric_limits<float>::quiet_NaN();
    mLastGateReleaseMs = std::numeric_limits<float>::quiet_NaN();

    setLatencySamples (oversamplingLatencySamples (osFactor));
}

void BaySickNAMIRProcessor::rebuildOversampling()
{
    using OS = juce::dsp::Oversampling<float>;
    // Mono - we oversample post-mono-sum.  Polyphase IIR is the standard cheap
    // anti-imaging filter; isMaximumQuality=true leans toward fewer artifacts.
    mOversampling[0] = std::make_unique<OS> ((size_t) 1, (size_t) 1,
                                             OS::filterHalfBandPolyphaseIIR, true);
    mOversampling[1] = std::make_unique<OS> ((size_t) 1, (size_t) 2,
                                             OS::filterHalfBandPolyphaseIIR, true);
    for (auto& os : mOversampling)
        os->initProcessing ((size_t) mMaxBlock);
}

int BaySickNAMIRProcessor::oversamplingLatencySamples (int factor) const
{
    if (factor <= 0) return 0;
    const auto& os = mOversampling[(size_t) (factor - 1)];
    if (! os) return 0;
    return (int) std::round (os->getLatencyInSamples());
}

// ─────────────────────────────────────────────────────────────────────────────
// processBlock - full Phase G-1.3 chain.
// ─────────────────────────────────────────────────────────────────────────────
namespace
{
    // HEIGHT -> the one off-axis angle MicPlacementDSP takes.
    //
    // The engine is rotationally symmetric about the speaker axis
    // (polarResponse is cos(angle); the off-axis shelf uses abs(angle)), so it
    // wants the TRUE off-axis angle rather than one component of it.  With a
    // horizontal angle h and a height y at horizontal distance d:
    //
    //     v   = atan(y / d)            the vertical angle the height implies
    //     eff = acos(cos h * cos v)    the actual angle off the axis
    //     r   = sqrt(d^2 + y^2)        the actual straight-line distance
    //
    // Feeding those two in means proximity, air absorption, off-axis darkening
    // and polar gain are all correct for a mic that is off-centre vertically,
    // with nothing changed inside the DSP itself.  Height 0 reduces to exactly
    // the old behaviour, which is what makes existing projects load unchanged.
    struct PlacementGeometry { float distanceCm; float angleDeg; };

    PlacementGeometry combinePlacement (float distCm, float angleDeg, float heightCm) noexcept
    {
        distCm = juce::jmax (1.0f, distCm);

        const float h = juce::degreesToRadians (angleDeg);
        const float v = std::atan2 (heightCm, distCm);

        const float cosEff = juce::jlimit (-1.0f, 1.0f, std::cos (h) * std::cos (v));
        const float effDeg = juce::radiansToDegrees (std::acos (cosEff))
                               * (angleDeg < 0.0f ? -1.0f : 1.0f);

        return { std::sqrt (distCm * distCm + heightCm * heightCm),
                 juce::jlimit (-90.0f, 90.0f, effDeg) };
    }
}

void BaySickNAMIRProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // ── 1. Adopt any pending NAM swaps (per slot; wait-free; no dealloc here)
    for (size_t s = 0; s < 2; ++s)
    {
        if (mNamSwapPending[s].load (std::memory_order_acquire))
        {
            std::swap (mNamActive[s], mNamPending[s]);
            mNamSwapPending[s].store (false, std::memory_order_release);
        }
    }

    const int numCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (numCh == 0 || numSamples == 0)
        return;

    // 2026-05-06 DSP gate: when neither slot has a NAM model AND neither has
    // an IR loaded, the only DSP work left is mic placement on a dry signal -
    // a barely-audible coloration.  Skip the whole pipeline (input gain,
    // model, cab, mic placement, output gain) and let the input pass through
    // unchanged.  Effect on user audio: passthrough = same as before this
    // gate, since with nothing loaded the chain was approximately a unity
    // pipeline anyway.  Saves the per-block cost of every IIR filter +
    // gain stage on Inst tabs that haven't loaded a NAM yet.
    // Audio-thread danger zone: gate on mIrLoaded, NEVER on getIrFilePath.
    // mIrPaths is a bare juce::String array that loadImpulseResponse reassigns
    // from the message thread with audio running; returning one by value here
    // would copy-construct a refcounted String -- a plain pointer read followed
    // by a retain, which can decrement a StringHolder the writer already freed.
    // mIrLoaded is release-stored one line BEFORE the path write and is what
    // the per-slot runIr gate already reads, so the two decisions now agree.
    const bool noModelOrIr =
           ! hasNamModel (0) && ! hasNamModel (1)
        && ! mIrLoaded[0].load (std::memory_order_acquire)
        && ! mIrLoaded[1].load (std::memory_order_acquire);
    if (noModelOrIr)
        return;

    // C.4 Phase 2.2: refresh engine-level SC RMS for any internal mod source.
    mScHelper.updateLevel (numSamples);

    // ── 2. Snapshot APVTS params once per block ──────────────────────────────
    auto get = [&] (const char* id) -> float
    {
        if (auto* p = apvts.getRawParameterValue (id))
            return p->load();
        return 0.0f;
    };
    const float inGainDb     = get ("input_gain");
    const float outGainDb    = get ("output");
    const bool  namBypassed  = get ("nam_bypass") > 0.5f;
    const bool  cabBypassed  = get ("cab_bypass") > 0.5f;
    const float lowCutHz     = get ("low_cut");
    const float highCutHz    = get ("high_cut");
    const float cabMix01     = juce::jlimit (0.0f, 1.0f, get ("cab_mix") * 0.01f);
    const float gateThreshDb = get ("gate_threshold");
    const float gateReleaseMs= get ("gate_release");
    const int   osFactor     = juce::jlimit (0, 2, (int) get ("oversampling"));
    const int   slot         = juce::jlimit (0, 1, (int) get ("ab_slot"));

    if (! juce::approximatelyEqual (lowCutHz,  mLastLowCutHz))  updateLowCutCoeffs  (lowCutHz);
    if (! juce::approximatelyEqual (highCutHz, mLastHighCutHz)) updateHighCutCoeffs (highCutHz);
    if (! juce::approximatelyEqual (gateThreshDb,  mLastGateThreshDb)
     || ! juce::approximatelyEqual (gateReleaseMs, mLastGateReleaseMs))
        updateGateCoeffs (gateThreshDb, gateReleaseMs);

    // ── 3. Input gain (in-place stereo) ──────────────────────────────────────
    const float inLin  = juce::Decibels::decibelsToGain (inGainDb);
    const float outLin = juce::Decibels::decibelsToGain (outGainDb);
    if (! juce::approximatelyEqual (inLin, 1.0f))
        buffer.applyGain (inLin);

    // ── 4. Noise gate (BEFORE NAM - gate the dry guitar, not the saturation)
    {
        float* L = buffer.getWritePointer (0);
        float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
        for (int i = 0; i < numSamples; ++i)
        {
            const float peak = std::max (std::abs (L[i]),
                                         R ? std::abs (R[i]) : 0.0f);
            const float target = (peak >= mGateThresholdLin) ? 1.0f : 0.0f;
            const float coef   = (target > mGateEnv) ? mGateAttackCoef
                                                     : mGateReleaseCoef;
            mGateEnv = target + (mGateEnv - target) * coef;
            L[i] *= mGateEnv;
            if (R) R[i] *= mGateEnv;
        }
    }

    // ── 5. NAM inference - mono in/out; broadcast to both channels ───────────
    //    OS factor 0 = direct path; 1/2 = oversample → NAM → downsample.
    auto& nam = mNamActive[(size_t) slot];
    if (nam && ! namBypassed)
    {
        // Mono-sum stereo into mMonoFloatBuf (channel 0).
        if (mMonoFloatBuf.getNumSamples() < numSamples)
            mMonoFloatBuf.setSize (1, numSamples, false, false, true);
        float* monoF = mMonoFloatBuf.getWritePointer (0);
        const float* L = buffer.getReadPointer (0);
        const float* R = numCh > 1 ? buffer.getReadPointer (1) : L;
        for (int i = 0; i < numSamples; ++i)
            monoF[i] = 0.5f * (L[i] + R[i]);

        if (osFactor == 0)
        {
            // Direct path - host-rate NAM.
            if ((int) mNamMonoIn .size() < numSamples) mNamMonoIn .resize ((size_t) numSamples);
            if ((int) mNamMonoOut.size() < numSamples) mNamMonoOut.resize ((size_t) numSamples);
            for (int i = 0; i < numSamples; ++i)
                mNamMonoIn[(size_t) i] = (double) monoF[i];

            double* inP[1]  = { mNamMonoIn .data() };
            double* outP[1] = { mNamMonoOut.data() };
            try { nam->process (inP, outP, numSamples); }
            catch (...)
            {
                std::fill (mNamMonoOut.begin(), mNamMonoOut.begin() + numSamples, 0.0);
            }
            for (int i = 0; i < numSamples; ++i)
                monoF[i] = (float) mNamMonoOut[(size_t) i];
        }
        else if (auto* os = mOversampling[(size_t) (osFactor - 1)].get())
        {
            // Upsample → NAM → downsample.  AudioBlock is a mutable view; we
            // write OS output into the same view that processSamplesDown reads
            // back from before writing the host-rate result into our block.
            juce::dsp::AudioBlock<float> block (mMonoFloatBuf);
            auto upBlock = os->processSamplesUp (block);
            const int upN = (int) upBlock.getNumSamples();

            if ((int) mNamMonoIn .size() < upN) mNamMonoIn .resize ((size_t) upN);
            if ((int) mNamMonoOut.size() < upN) mNamMonoOut.resize ((size_t) upN);
            float* upPtr = upBlock.getChannelPointer (0);
            for (int i = 0; i < upN; ++i)
                mNamMonoIn[(size_t) i] = (double) upPtr[i];

            double* inP[1]  = { mNamMonoIn .data() };
            double* outP[1] = { mNamMonoOut.data() };
            try { nam->process (inP, outP, upN); }
            catch (...)
            {
                std::fill (mNamMonoOut.begin(), mNamMonoOut.begin() + upN, 0.0);
            }
            for (int i = 0; i < upN; ++i)
                upPtr[i] = (float) mNamMonoOut[(size_t) i];

            os->processSamplesDown (block);
        }

        // Broadcast mono → stereo.
        float* dL = buffer.getWritePointer (0);
        float* dR = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
        for (int i = 0; i < numSamples; ++i)
        {
            const float v = monoF[i];
            dL[i] = v;
            if (dR) dR[i] = v;
        }
    }

    // ── 6. Pre-cab filters (HPF then LPF) ────────────────────────────────────
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        mLowCut .process (ctx);
        mHighCut.process (ctx);
    }

    // ── 7. Cabinet (IR) - wet/dry mix on cab_mix, bypass-aware ───────────────
    const bool runIr = mIrLoaded[(size_t) slot].load (std::memory_order_acquire) && ! cabBypassed;
    if (runIr)
    {
        if (mDryBuf.getNumSamples() < numSamples)
            mDryBuf.setSize (2, numSamples, false, false, true);
        for (int ch = 0; ch < juce::jmin (2, numCh); ++ch)
            mDryBuf.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        mIr[(size_t) slot].process (ctx);

        const float wet = cabMix01;
        const float dry = 1.0f - cabMix01;
        for (int ch = 0; ch < juce::jmin (2, numCh); ++ch)
        {
            float* w = buffer.getWritePointer (ch);
            const float* d = mDryBuf.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                w[i] = w[i] * wet + d[i] * dry;
        }
    }

    // ── 7b-pre. QA-Fc dual-mic tap ───────────────────────────────────────────
    // Mic B mirrors Mic A over the SAME post-cab signal, but Mic A processes
    // in-place next -- stash the feed first.  micBRun stays true through the
    // ramp-out so toggling off glides to silence instead of stepping; once
    // settled at 0 the whole B path costs one param read + two compares.
    //
    // BOTH mics are switched now (Jeff, 2026-08-11), and the pre-mic tap serves
    // both: it is B's INPUT and it is also A's dry side, because A processes the
    // main buffer in place and switching it off has to fade back to something.
    const bool micAActive = get ("nam_mica_active") > 0.5f;
    const bool micBActive = get ("nam_micb_active") > 0.5f;
    const bool micARun    = micAActive || mMicAGain > 0.0f;
    const bool micBRun    = micBActive || mMicBGain > 0.0f;

    if (micARun || micBRun)
    {
        if (mPreMicScratch.getNumSamples() < numSamples)
            mPreMicScratch.setSize (2, numSamples, false, false, true);
        for (int ch = 0; ch < juce::jmin (2, numCh); ++ch)
            mPreMicScratch.copyFrom (ch, 0, buffer, ch, 0, numSamples);
    }

    // Rising edge after idle: the convolution FIFO + filter state still hold
    // audio from the last active period -- flush so the ramp-in starts from
    // silence, not a replay of stale signal.
    if (micARun && ! mMicAWasRunning) { mMicSim .reset(); mMicPlacement .reset(); }
    if (micBRun && ! mMicBWasRunning) { mMicSimB.reset(); mMicPlacementB.reset(); }
    mMicAWasRunning = micARun;
    mMicBWasRunning = micBRun;

    // ── 7b/7c. H-6d Mic Sim + Mic Placement (post-IR, in place) ─────────────
    if (micARun)
    {
        {
            const int   modeI  = 1 + juce::jlimit (0, 1, (int) get ("nam_micsim_mode"));
            const int   modelI = juce::jlimit (0, (int) MicSimDSP::Model::kNumModels - 1,
                                                  (int) get ("nam_micsim_model"));
            const float mix01  = juce::jlimit (0.0f, 1.0f, get ("nam_micsim_mix") * 0.01f);
            mMicSim.setMode  ((MicSimDSP::Mode) modeI);
            mMicSim.setModel (modelI);
            mMicSim.setMix   (mix01);
            mMicSim.process  (buffer);
        }
        {
            const float distCm = get ("nam_placement_distance_cm");
            const float angle  = get ("nam_placement_angle_deg");
            const float height = get ("nam_placement_height_cm");
            const int   polar  = juce::jlimit (0, 4, (int) get ("nam_placement_polar"));
            const float mix01  = juce::jlimit (0.0f, 1.0f, get ("nam_placement_mix") * 0.01f);
            const auto  geo    = combinePlacement (distCm, angle, height);
            mMicPlacement.setDistanceCm (geo.distanceCm);
            mMicPlacement.setAngleDeg   (geo.angleDeg);
            mMicPlacement.setPolar      (polar);
            mMicPlacement.setMix        (mix01);
            mMicPlacement.process       (buffer);
        }

        // Same 15 ms ramp the B path uses, but crossfading toward the DRY tap
        // rather than toward silence: switching Mic A off means no mic model on
        // the cab, not no cab.
        const float target = micAActive ? 1.0f : 0.0f;
        const float step   = 1.0f / juce::jmax (1.0f, 0.015f * (float) mSampleRate);
        float gEnd = mMicAGain;
        for (int ch = 0; ch < juce::jmin (2, numCh); ++ch)
        {
            float g = mMicAGain;   // both channels trace the same trajectory
            float* w       = buffer.getWritePointer (ch);
            const float* d = mPreMicScratch.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                g = (target > g) ? juce::jmin (target, g + step)
                                 : juce::jmax (target, g - step);
                w[i] = d[i] + (w[i] - d[i]) * g;
            }
            gEnd = g;
        }
        mMicAGain = gEnd;
    }

    // ── 7d. QA-Fc Mic B (parallel Sim + Placement over the pre-mic tap) ─────
    // The processed B branch SUMS into the main buffer -- parallel mics add
    // (+6 dB on identical settings), they don't crossfade.
    if (micBRun)
    {
        if (mMicBScratch.getNumSamples() < numSamples)
            mMicBScratch.setSize (2, numSamples, false, false, true);
        for (int ch = 0; ch < juce::jmin (2, numCh); ++ch)
            mMicBScratch.copyFrom (ch, 0, mPreMicScratch, ch, 0, numSamples);

        {
            const int   modeI  = 1 + juce::jlimit (0, 1, (int) get ("nam_micsim_b_mode"));
            const int   modelI = juce::jlimit (0, (int) MicSimDSP::Model::kNumModels - 1,
                                                  (int) get ("nam_micsim_b_model"));
            const float mix01  = juce::jlimit (0.0f, 1.0f, get ("nam_micsim_b_mix") * 0.01f);
            mMicSimB.setMode  ((MicSimDSP::Mode) modeI);
            mMicSimB.setModel (modelI);
            mMicSimB.setMix   (mix01);
            mMicSimB.process  (mMicBScratch);
        }
        {
            const float distCm = get ("nam_placement_b_distance_cm");
            const float angle  = get ("nam_placement_b_angle_deg");
            const float height = get ("nam_placement_b_height_cm");
            const auto  geoB   = combinePlacement (distCm, angle, height);
            const int   polar  = juce::jlimit (0, 4, (int) get ("nam_placement_b_polar"));
            const float mix01  = juce::jlimit (0.0f, 1.0f, get ("nam_placement_b_mix") * 0.01f);
            mMicPlacementB.setDistanceCm (geoB.distanceCm);
            mMicPlacementB.setAngleDeg   (geoB.angleDeg);
            mMicPlacementB.setPolar      (polar);
            mMicPlacementB.setMix        (mix01);
            mMicPlacementB.process       (mMicBScratch);
        }

        // 15 ms activation ramp: fast enough to feel instant, slow enough
        // that the correlated +6 dB step can't click under automation.
        const float target = micBActive ? 1.0f : 0.0f;
        const float step   = 1.0f / juce::jmax (1.0f, 0.015f * (float) mSampleRate);
        float gEnd = mMicBGain;
        for (int ch = 0; ch < juce::jmin (2, numCh); ++ch)
        {
            float g = mMicBGain;   // both channels trace the same trajectory
            float* w = buffer.getWritePointer (ch);
            const float* b = mMicBScratch.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                g = (target > g) ? juce::jmin (target, g + step)
                                 : juce::jmax (target, g - step);
                w[i] += b[i] * g;
            }
            gEnd = g;
        }
        mMicBGain = gEnd;
    }

    // ── 8. Master output ─────────────────────────────────────────────────────
    if (! juce::approximatelyEqual (outLin, 1.0f))
        buffer.applyGain (outLin);
}

// ─────────────────────────────────────────────────────────────────────────────
// Coefficient updates.
// ─────────────────────────────────────────────────────────────────────────────
void BaySickNAMIRProcessor::updateLowCutCoeffs (float hz)
{
    mLastLowCutHz = hz;
    *mLowCut.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (
                          mSampleRate, juce::jlimit (10.0f, 1000.0f, hz));
}

void BaySickNAMIRProcessor::updateHighCutCoeffs (float hz)
{
    mLastHighCutHz = hz;
    *mHighCut.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass (
                          mSampleRate, juce::jlimit (1000.0f, 20000.0f, hz));
}

void BaySickNAMIRProcessor::updateGateCoeffs (float thresholdDb, float releaseMs)
{
    mLastGateThreshDb   = thresholdDb;
    mLastGateReleaseMs  = releaseMs;
    mGateThresholdLin   = juce::Decibels::decibelsToGain (thresholdDb);

    // Single-pole exp coefficient: y[n] = target + (y[n-1] - target) * coef.
    // Attack is fixed-fast (1 ms) so percussive transients don't re-trigger
    // gate ramps audibly.  Release is the user knob.
    const float sr        = (float) mSampleRate;
    const float attackSec = 0.001f;                  // 1 ms attack
    const float releaseSec = juce::jmax (0.001f, releaseMs * 0.001f);
    mGateAttackCoef  = std::exp (-1.0f / (attackSec  * sr));
    mGateReleaseCoef = std::exp (-1.0f / (releaseSec * sr));
}

// ─────────────────────────────────────────────────────────────────────────────
// loadNamModel / loadImpulseResponse - message-thread file I/O + swap pattern.
// ─────────────────────────────────────────────────────────────────────────────
bool BaySickNAMIRProcessor::loadNamModel (const juce::String& filePath, juce::String& outErr, int slot)
{
    slot = resolveSlot (slot);

    if (filePath.isEmpty())
    {
        outErr = "No file path provided.";
        return false;
    }
    // filePath is the string that gets PERSISTED (see the assignment at the
    // bottom), so it stays exactly as the caller wrote it: a bundled project
    // stores "Samples/<name>.nam" and only the resolver knows what that means.
    const juce::File resolved = ProjectFileResolver::resolve (filePath);
    if (! resolved.existsAsFile())
    {
        outErr = "File not found: " + filePath;
        return false;
    }

    // SECURITY (QA-Cleanup 2026-08-10): gate BEFORE nam::get_dsp.  The catch
    // blocks below cannot help here - the vendored loader's failure modes are an
    // unchecked over-read (no exception; on Windows an access violation is a
    // Structured Exception, which /EHsc does not route to catch (...)) and a
    // prewarm loop driven by a file-supplied length (throws nothing, simply
    // never returns).  See SafeNamModel.h.
    if (const auto why = SafeNamModel::rejectReason (resolved); why.isNotEmpty())
    {
        outErr = "This NAM capture was refused: " + why + ".";
        return false;
    }

    std::unique_ptr<nam::DSP> newModel;
    bool isFullRigFlag = false;
    try
    {
        nam::dspData data;
        std::filesystem::path p (resolved.getFullPathName().toStdString());
        newModel = nam::get_dsp (p, data);
        if (! newModel)
        {
            outErr = "NAM core returned a null model.";
            return false;
        }

        // Full-rig auto-detect: peek metadata.gear_type ("amp_cab" /
        // "amp_pedal_cab" mean the capture already includes a cabinet so the
        // editor will suggest auto-bypassing the IR convolution).
        if (data.metadata.contains ("gear_type") && data.metadata["gear_type"].is_string())
        {
            const std::string g = data.metadata["gear_type"].get<std::string>();
            isFullRigFlag = (g == "amp_cab" || g == "amp_pedal_cab");
        }

        if (mPrepared)
        {
            // Reset at the active oversampling rate so this slot is ready to
            // run as soon as the audio thread swaps it in.
            int osFactor = 0;
            if (auto* pp = apvts.getRawParameterValue ("oversampling"))
                osFactor = juce::jlimit (0, 2, (int) pp->load());
            const double osRate     = mSampleRate * (double) (1 << osFactor);
            const int    osMaxBlock = mMaxBlock   * (1 << osFactor);
            newModel->ResetAndPrewarm (osRate, osMaxBlock);
        }
    }
    catch (const std::exception& ex)
    {
        outErr = juce::String ("NAM load failed: ") + ex.what();
        return false;
    }
    catch (...)
    {
        outErr = "NAM load failed: unknown error.";
        return false;
    }

    {
        const juce::ScopedLock lk (mLoadLock);
        for (int spin = 0; spin < 1000 && mNamSwapPending[(size_t) slot].load (std::memory_order_acquire); ++spin)
            juce::Thread::sleep (1);

        std::unique_ptr<nam::DSP> oldPending = std::move (mNamPending[(size_t) slot]);
        juce::ignoreUnused (oldPending);

        mNamPending  [(size_t) slot] = std::move (newModel);
        mNamIsFullRig[(size_t) slot].store (isFullRigFlag, std::memory_order_release);
        mNamLoaded   [(size_t) slot].store (true,          std::memory_order_release);
        mNamSwapPending[(size_t) slot].store (true,        std::memory_order_release);
    }

    mNamPaths[(size_t) slot] = filePath;
    return true;
}

bool BaySickNAMIRProcessor::loadImpulseResponse (const juce::String& filePath, juce::String& outErr, int slot)
{
    slot = resolveSlot (slot);

    if (filePath.isEmpty())
    {
        outErr = "No file path provided.";
        return false;
    }
    // Same contract as loadNamModel: resolve for the read, persist what came in.
    const juce::File f = ProjectFileResolver::resolve (filePath);
    if (! f.existsAsFile())
    {
        outErr = "File not found: " + filePath;
        return false;
    }

    // Framework quirk: juce::dsp::Convolution::loadImpulseResponse only QUEUES
    // the file to its own background thread, so it cannot throw and never
    // reports a decode failure back here.  A file it cannot read becomes an
    // empty buffer, which fixNumChannels turns into a 1-sample unit impulse --
    // a pass-through convolution indistinguishable from a loaded cab.  Decode
    // it ourselves first so an unreadable file fails loudly instead.
    {
        juce::AudioFormatManager fm;
        SafeAudioFormats::registerAll (fm);
        auto reader = SafeAudioReader::guard (
            std::unique_ptr<juce::AudioFormatReader> (fm.createReaderFor (f)));
        if (reader == nullptr || reader->lengthInSamples <= 0)
        {
            outErr = "That file is not readable audio: " + filePath;
            return false;
        }
    }

    mIr[(size_t) slot].loadImpulseResponse (
        f,
        juce::dsp::Convolution::Stereo   ::yes,
        juce::dsp::Convolution::Trim     ::yes,
        0,
        juce::dsp::Convolution::Normalise::yes);

    mIrLoaded[(size_t) slot].store (true, std::memory_order_release);
    mIrPaths[(size_t) slot] = filePath;
    return true;
}

bool BaySickNAMIRProcessor::loadUserMicIr (const juce::File& f, juce::String& outErr)
{
    const juce::ScopedLock lock (mLoadLock);
    // Loads into the currently-active slot (matches the NAM/Cab IR pattern
    // where loadNamModel / loadImpulseResponse default to the active slot).
    const bool ok = mMicSim.loadUserIr (f, outErr, getActiveSlot());
    return ok;
}

void BaySickNAMIRProcessor::clearUserMicIr()
{
    mMicSim.clearUserIr (getActiveSlot());
}

bool BaySickNAMIRProcessor::loadUserMicIrB (const juce::File& f, juce::String& outErr)
{
    const juce::ScopedLock lock (mLoadLock);
    const bool ok = mMicSimB.loadUserIr (f, outErr, getActiveSlot());
    return ok;
}

void BaySickNAMIRProcessor::clearUserMicIrB()
{
    mMicSimB.clearUserIr (getActiveSlot());
}

// ─────────────────────────────────────────────────────────────────────────────
// APVTS listener - oversampling factor change re-Resets loaded NAM models at
// the new effective rate, ab_slot swaps the per-slot tone snapshots.  Fires
// synchronously on whatever thread wrote the parameter.
//
// LIVE: the work is posted to the message thread because Reset allocates and
// the snapshot swap walks the whole parameter set - neither may run on the
// realtime callback.  The post carries a weak lifetime token because this is a
// rig-owned tab engine the message thread can free at any time, and a queued
// callAsync cannot be retracted.
//
// OFFLINE: the writer already IS the render thread (automation replay), which
// has no realtime deadline, so the work runs inline - a message hop would land
// after the block it belongs to, and during a freeze render the message thread
// is blocked inside the render loop entirely.
// ─────────────────────────────────────────────────────────────────────────────
void BaySickNAMIRProcessor::parameterChanged (const juce::String& paramID, float newValue)
{
    const bool offline = isNonRealtime();
    std::weak_ptr<char> life (mLife);

    if (paramID == "oversampling")
    {
        const int newFactor = juce::jlimit (0, 2, (int) newValue);

        if (offline)
        {
            reResetNamForOversampling (newFactor);
            return;
        }

        juce::MessageManager::callAsync ([this, life, newFactor]()
        {
            if (life.expired()) return;
            this->reResetNamForOversampling (newFactor);
        });
    }
    else if (paramID == "ab_slot")
    {
        // replaceState fires this listener while a project/page preset loads.
        // The restore seats both snapshots, mLastSlot and the Mic Sim active
        // slot itself, from the saved SlotA / SlotB children; letting a swap
        // run here would capture the freshly restored values over the OTHER
        // slot's stored tone and destroy it.  (The oversampling branch above is
        // deliberately NOT guarded: it only reconfigures DSP from the new value,
        // and the restore's own model loads reset at the same factor anyway.)
        if (isRestoringState()) return;

        const int newSlot = juce::jlimit (0, 1, (int) newValue);
        if (newSlot == mLastSlot) return;
        // Mic Sim's per-slot user IR is resident; just point at the new
        // slot's IR (audio-thread-safe atomic store, no reload).
        mMicSim .setActiveSlot (newSlot);
        mMicSimB.setActiveSlot (newSlot);

        const int outgoingSlot = mLastSlot;

        // A replay is not a user edit -- the same ruling that keeps a replay
        // from marking engine content dirty or staling a freeze.  The two banks
        // hold tones the user dialled by hand; capturing over the outgoing one
        // on every lane transition would bank whatever the OTHER lanes have
        // driven the chain to at that instant, destroying both tones over the
        // length of a pass.  The slot still SWITCHES on replay -- only the
        // capture is suppressed.
        //
        // The phase flag is thread_local and this listener runs synchronously
        // on the writer's thread, so it must be read here and carried into the
        // async hop below, which lands after the writer's scope has closed.
        const bool userGesture = ! juce::AudioProcessorValueTreeState::programmaticWritePhase;

        if (offline)
        {
            if (userGesture) captureSnapshotFromCurrent (outgoingSlot);
            applySnapshotToCurrent (newSlot);
            mLastSlot = newSlot;
            return;
        }

        juce::MessageManager::callAsync ([this, life, outgoingSlot, newSlot, userGesture]()
        {
            if (life.expired()) return;
            if (userGesture) captureSnapshotFromCurrent (outgoingSlot);
            applySnapshotToCurrent (newSlot);
            mLastSlot = newSlot;
        });
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SlotSnapshot serialization (used by getStateInformation / setStateInformation).
// ─────────────────────────────────────────────────────────────────────────────
juce::ValueTree BaySickNAMIRProcessor::SlotSnapshot::toValueTree (const juce::Identifier& root) const
{
    juce::ValueTree v (root);
    v.setProperty ("inputGain",         inputGain,         nullptr);
    v.setProperty ("output",            output,            nullptr);
    v.setProperty ("gateThresh",        gateThresh,        nullptr);
    v.setProperty ("gateRelease",       gateRelease,       nullptr);
    v.setProperty ("lowCut",            lowCut,            nullptr);
    v.setProperty ("highCut",           highCut,           nullptr);
    v.setProperty ("cabMix",            cabMix,            nullptr);
    v.setProperty ("namBypass",         namBypass,         nullptr);
    v.setProperty ("cabBypass",         cabBypass,         nullptr);
    v.setProperty ("micaActive",        micaActive,        nullptr);
    v.setProperty ("micSimMode",        micSimMode,        nullptr);
    v.setProperty ("micSimModel",       micSimModel,       nullptr);
    v.setProperty ("micSimMix",         micSimMix,         nullptr);
    v.setProperty ("micUserIrPath",     micUserIrPath,     nullptr);
    v.setProperty ("placementDistance", placementDistance, nullptr);
    v.setProperty ("placementAngle",    placementAngle,    nullptr);
    v.setProperty ("placementHeight",   placementHeight,   nullptr);
    v.setProperty ("placementPolar",    placementPolar,    nullptr);
    v.setProperty ("placementMix",      placementMix,      nullptr);
    v.setProperty ("micbActive",         micbActive,         nullptr);
    v.setProperty ("micSimBMode",        micSimBMode,        nullptr);
    v.setProperty ("micSimBModel",       micSimBModel,       nullptr);
    v.setProperty ("micSimBMix",         micSimBMix,         nullptr);
    v.setProperty ("micbUserIrPath",     micbUserIrPath,     nullptr);
    v.setProperty ("placementBDistance", placementBDistance, nullptr);
    v.setProperty ("placementBAngle",    placementBAngle,    nullptr);
    v.setProperty ("placementBHeight",   placementBHeight,   nullptr);
    v.setProperty ("placementBPolar",    placementBPolar,    nullptr);
    v.setProperty ("placementBMix",      placementBMix,      nullptr);
    return v;
}

void BaySickNAMIRProcessor::SlotSnapshot::fromValueTree (const juce::ValueTree& v)
{
    inputGain         = (float) v.getProperty ("inputGain",         inputGain);
    output            = (float) v.getProperty ("output",            output);
    gateThresh        = (float) v.getProperty ("gateThresh",        gateThresh);
    gateRelease       = (float) v.getProperty ("gateRelease",       gateRelease);
    lowCut            = (float) v.getProperty ("lowCut",            lowCut);
    highCut           = (float) v.getProperty ("highCut",           highCut);
    cabMix            = (float) v.getProperty ("cabMix",            cabMix);
    namBypass         = (bool)  v.getProperty ("namBypass",         namBypass);
    cabBypass         = (bool)  v.getProperty ("cabBypass",         cabBypass);
    micaActive        = (bool)  v.getProperty ("micaActive",        micaActive);
    micSimMode        = (int)   v.getProperty ("micSimMode",        micSimMode);
    micSimModel       = (int)   v.getProperty ("micSimModel",       micSimModel);
    micSimMix         = (float) v.getProperty ("micSimMix",         micSimMix);
    micUserIrPath     =         v.getProperty ("micUserIrPath",     micUserIrPath).toString();
    placementDistance = (float) v.getProperty ("placementDistance", placementDistance);
    placementAngle    = (float) v.getProperty ("placementAngle",    placementAngle);
    placementHeight   = (float) v.getProperty ("placementHeight",   placementHeight);
    placementPolar    = (int)   v.getProperty ("placementPolar",    placementPolar);
    placementMix      = (float) v.getProperty ("placementMix",      placementMix);
    micbActive         = (bool)  v.getProperty ("micbActive",         micbActive);
    micSimBMode        = (int)   v.getProperty ("micSimBMode",        micSimBMode);
    micSimBModel       = (int)   v.getProperty ("micSimBModel",       micSimBModel);
    micSimBMix         = (float) v.getProperty ("micSimBMix",         micSimBMix);
    micbUserIrPath     =         v.getProperty ("micbUserIrPath",     micbUserIrPath).toString();
    placementBDistance = (float) v.getProperty ("placementBDistance", placementBDistance);
    placementBAngle    = (float) v.getProperty ("placementBAngle",    placementBAngle);
    placementBHeight   = (float) v.getProperty ("placementBHeight",   placementBHeight);
    placementBPolar    = (int)   v.getProperty ("placementBPolar",    placementBPolar);
    placementBMix      = (float) v.getProperty ("placementBMix",      placementBMix);
}

// ─────────────────────────────────────────────────────────────────────────────
// Snapshot capture / restore.  Called from parameterChanged on ab_slot change.
// ─────────────────────────────────────────────────────────────────────────────
void BaySickNAMIRProcessor::captureSnapshotFromCurrent (int slot)
{
    if (slot < 0 || slot > 1) return;
    auto& s = mSnapshots[(size_t) slot];

    auto getF = [&] (const char* id, float fallback) -> float
    {
        if (auto* p = apvts.getRawParameterValue (id)) return p->load();
        return fallback;
    };

    s.inputGain         = getF ("input_gain",                s.inputGain);
    s.output            = getF ("output",                    s.output);
    s.gateThresh        = getF ("gate_threshold",            s.gateThresh);
    s.gateRelease       = getF ("gate_release",              s.gateRelease);
    s.lowCut            = getF ("low_cut",                   s.lowCut);
    s.highCut           = getF ("high_cut",                  s.highCut);
    s.cabMix            = getF ("cab_mix",                   s.cabMix);
    s.namBypass         = getF ("nam_bypass", 0.0f) > 0.5f;
    s.cabBypass         = getF ("cab_bypass", 0.0f) > 0.5f;
    s.micaActive        = getF ("nam_mica_active", 0.0f) > 0.5f;
    s.micSimMode        = (int) getF ("nam_micsim_mode",     (float) s.micSimMode);
    s.micSimModel       = (int) getF ("nam_micsim_model",    (float) s.micSimModel);
    s.micSimMix         = getF ("nam_micsim_mix",            s.micSimMix);
    s.micUserIrPath     = mMicSim.getUserIrPath (slot);
    s.placementDistance = getF ("nam_placement_distance_cm", s.placementDistance);
    s.placementAngle    = getF ("nam_placement_angle_deg",   s.placementAngle);
    s.placementHeight   = getF ("nam_placement_height_cm",   s.placementHeight);
    s.placementPolar    = (int) getF ("nam_placement_polar", (float) s.placementPolar);
    s.placementMix      = getF ("nam_placement_mix",         s.placementMix);
    s.micbActive         = getF ("nam_micb_active", 0.0f) > 0.5f;
    s.micSimBMode        = (int) getF ("nam_micsim_b_mode",     (float) s.micSimBMode);
    s.micSimBModel       = (int) getF ("nam_micsim_b_model",    (float) s.micSimBModel);
    s.micSimBMix         = getF ("nam_micsim_b_mix",            s.micSimBMix);
    s.micbUserIrPath     = mMicSimB.getUserIrPath (slot);
    s.placementBDistance = getF ("nam_placement_b_distance_cm", s.placementBDistance);
    s.placementBAngle    = getF ("nam_placement_b_angle_deg",   s.placementBAngle);
    s.placementBHeight   = getF ("nam_placement_b_height_cm",   s.placementBHeight);
    s.placementBPolar    = (int) getF ("nam_placement_b_polar", (float) s.placementBPolar);
    s.placementBMix      = getF ("nam_placement_b_mix",         s.placementBMix);
}

void BaySickNAMIRProcessor::applySnapshotToCurrent (int slot)
{
    if (slot < 0 || slot > 1) return;
    // QA-UndoCoverage Task 6: snapshot restores are programmatic.  Undo of an
    // A/B flip stays correct WITHOUT these values in history -- undoing the
    // ab_slot param re-fires parameterChanged, which re-applies the other
    // slot's snapshot (the handler is self-healing).
    juce::AudioProcessorValueTreeState::ScopedProgrammaticParamWrites spw;
    const auto& s = mSnapshots[(size_t) slot];

    auto setF = [&] (const char* id, float value)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (value));
    };
    auto setB = [&] (const char* id, bool value)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (id)))
            *p = value;
    };

    setF ("input_gain",                s.inputGain);
    setF ("output",                    s.output);
    setF ("gate_threshold",            s.gateThresh);
    setF ("gate_release",              s.gateRelease);
    setF ("low_cut",                   s.lowCut);
    setF ("high_cut",                  s.highCut);
    setF ("cab_mix",                   s.cabMix);
    setB ("nam_bypass",                s.namBypass);
    setB ("cab_bypass",                s.cabBypass);
    setB ("nam_mica_active",           s.micaActive);
    setF ("nam_micsim_mode",           (float) s.micSimMode);
    setF ("nam_micsim_model",          (float) s.micSimModel);
    setF ("nam_micsim_mix",            s.micSimMix);
    setF ("nam_placement_distance_cm", s.placementDistance);
    setF ("nam_placement_angle_deg",   s.placementAngle);
    setF ("nam_placement_height_cm",   s.placementHeight);
    setF ("nam_placement_polar",       (float) s.placementPolar);
    setF ("nam_placement_mix",         s.placementMix);
    setB ("nam_micb_active",           s.micbActive);
    setF ("nam_micsim_b_mode",         (float) s.micSimBMode);
    setF ("nam_micsim_b_model",        (float) s.micSimBModel);
    setF ("nam_micsim_b_mix",          s.micSimBMix);
    setF ("nam_placement_b_distance_cm", s.placementBDistance);
    setF ("nam_placement_b_angle_deg",   s.placementBAngle);
    setF ("nam_placement_b_height_cm",   s.placementBHeight);
    setF ("nam_placement_b_polar",       (float) s.placementBPolar);
    setF ("nam_placement_b_mix",         s.placementBMix);

    // Mic Sim user IR -- per-slot DSP buffer, both resident.  Slot switch
    // is instant; we still ensure the per-slot path matches what's
    // currently loaded for that slot.  Mismatch can only happen on first
    // setStateInformation when the per-slot IR hasn't been loaded yet --
    // in that case load it now (one-time cost, not on every slot switch).
    //
    // The "already loaded" test compares RESOLVED FILES on BOTH sides, because
    // neither side is guaranteed to be an absolute path: MicSimDSP now records a
    // stable REFERENCE ("mysamples:<rel>" / "library:<rel>", or an absolute for
    // anything outside those roots), and a snapshot restored from a bundled
    // project holds "Samples/<name>".  Comparing a resolved file against a raw
    // ref never matches, and would reload the IR on every slot switch for the
    // life of the project.
    const juce::String currentRef = mMicSim.getUserIrPath (slot);
    if (s.micUserIrPath.isEmpty())
    {
        if (currentRef.isNotEmpty()) mMicSim.clearUserIr (slot);
    }
    else if (const juce::File f = ProjectFileResolver::resolve (s.micUserIrPath);
             f != ProjectFileResolver::resolve (currentRef))
    {
        // QA-Export Task 5: report instead of skipping quietly (see the project
        // -load path above for the same pattern).
        juce::String err;
        if (! f.existsAsFile())
            MissingFileReport::add ("Mic A user IR", s.micUserIrPath);
        else if (! mMicSim.loadUserIr (f, err, slot))
            MissingFileReport::add ("Mic A user IR (failed to load)", s.micUserIrPath);
    }

    // Same resolve-both-sides rule as Mic A above.
    const juce::String currentRefB = mMicSimB.getUserIrPath (slot);
    if (s.micbUserIrPath.isEmpty())
    {
        if (currentRefB.isNotEmpty()) mMicSimB.clearUserIr (slot);
    }
    else if (const juce::File f = ProjectFileResolver::resolve (s.micbUserIrPath);
             f != ProjectFileResolver::resolve (currentRefB))
    {
        juce::String err;
        if (! f.existsAsFile())
            MissingFileReport::add ("Mic B user IR", s.micbUserIrPath);
        else if (! mMicSimB.loadUserIr (f, err, slot))
            MissingFileReport::add ("Mic B user IR (failed to load)", s.micbUserIrPath);
    }
}

void BaySickNAMIRProcessor::reResetNamForOversampling (int factor)
{
    if (! mPrepared) return;

    const juce::ScopedLock lk (mLoadLock);

    const double osRate     = mSampleRate * (double) (1 << factor);
    const int    osMaxBlock = mMaxBlock   * (1 << factor);

    for (size_t s = 0; s < 2; ++s)
    {
        for (int spin = 0; spin < 1000 && mNamSwapPending[s].load (std::memory_order_acquire); ++spin)
            juce::Thread::sleep (1);

        if (mNamActive[s])
        {
            try { mNamActive[s]->ResetAndPrewarm (osRate, osMaxBlock); }
            catch (...) { /* swallow - model is left in whatever state Reset reached */ }
        }
    }

    setLatencySamples (oversamplingLatencySamples (factor));
}

// ─────────────────────────────────────────────────────────────────────────────
// State (un)serialization - paths persist as ValueTree custom string props.
// Property keys stay backward-compatible with G-1.1/G-1.2 single-slot layouts.
// ─────────────────────────────────────────────────────────────────────────────
void BaySickNAMIRProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("nam_filepath",   mNamPaths[0], nullptr);
    state.setProperty ("ir_filepath",    mIrPaths [0], nullptr);
    state.setProperty ("nam_filepath_b", mNamPaths[1], nullptr);
    state.setProperty ("ir_filepath_b",  mIrPaths [1], nullptr);
    state.setProperty ("mic_user_ir_path", mMicSim.getUserIrPath(), nullptr);

    // 2026-05-05 (Bug C diagnostics): log what's being saved.
    namirLog ("getStateInformation:");
    namirLog ("  nam_filepath  ='" + mNamPaths[0] + "'");
    namirLog ("  ir_filepath   ='" + mIrPaths [0] + "'");
    namirLog ("  nam_filepath_b='" + mNamPaths[1] + "'");
    namirLog ("  ir_filepath_b ='" + mIrPaths [1] + "'");

    // H-6d: capture the currently-active slot's APVTS values into its
    // snapshot before serializing.  The other slot's snapshot already holds
    // its captured state from the last slot switch (or defaults).  Remove
    // ONLY prior SlotA/SlotB children -- APVTS PARAM children must stay.
    captureSnapshotFromCurrent (getActiveSlot());
    auto removePriorSnapshot = [&] (const juce::Identifier& name)
    {
        while (true)
        {
            auto child = state.getChildWithName (name);
            if (! child.isValid()) break;
            state.removeChild (child, nullptr);
        }
    };
    removePriorSnapshot ("SlotA");
    removePriorSnapshot ("SlotB");
    state.appendChild (mSnapshots[0].toValueTree ("SlotA"), nullptr);
    state.appendChild (mSnapshots[1].toValueTree ("SlotB"), nullptr);

    if (auto xml = std::unique_ptr<juce::XmlElement> (state.createXml()))
        copyXmlToBinary (*xml, destData);
}

void BaySickNAMIRProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = SafeXml::parseBinaryBlob (data, sizeInBytes))
    {
        // 2026-05-05 (Bug C diagnostics): Debug-build trace of the restore
        // path.  Output goes to Documents/BaySickDAW/namir_state_log.txt
        // (open in Notepad).
        namirLog ("setStateInformation: rootTag=" + xml->getTagName()
                  + " expected=" + apvts.state.getType().toString()
                  + " size=" + juce::String (sizeInBytes));

        if (xml->hasTagName (apvts.state.getType()))
        {
            // Raise before replaceState: a saved ab_slot that differs from the
            // live one fires the A/B listener mid-restore (see the guard in
            // parameterChanged).  The queued clear at the bottom runs AFTER
            // every hop the restore posted.
            mRestoringState->store (true, std::memory_order_release);

            apvts.replaceStateKeepingUndoHistory (juce::ValueTree::fromXml (*xml));
            mNamPaths[0] = apvts.state.getProperty ("nam_filepath",   {}).toString();
            mIrPaths [0] = apvts.state.getProperty ("ir_filepath",    {}).toString();
            mNamPaths[1] = apvts.state.getProperty ("nam_filepath_b", {}).toString();
            mIrPaths [1] = apvts.state.getProperty ("ir_filepath_b",  {}).toString();

            namirLog ("  nam_filepath  ='" + mNamPaths[0] + "'");
            namirLog ("  ir_filepath   ='" + mIrPaths [0] + "'");
            namirLog ("  nam_filepath_b='" + mNamPaths[1] + "'");
            namirLog ("  ir_filepath_b ='" + mIrPaths [1] + "'");

            juce::String err;
            if (mNamPaths[0].isNotEmpty())
            {
                const bool exists = ProjectFileResolver::resolve (mNamPaths[0]).existsAsFile();
                const bool ok = exists && loadNamModel (mNamPaths[0], err, 0);
                if (! exists)
                    MissingFileReport::add ("NAM model", mNamPaths[0]);
                else if (! ok)
                    MissingFileReport::add ("NAM model (failed to load)", mNamPaths[0]);
                namirLog ("  loadNamModel slot=0 ok=" + juce::String (ok ? 1 : 0)
                          + " err='" + err + "'");
                err.clear();
            }
            if (mNamPaths[1].isNotEmpty())
            {
                const bool exists = ProjectFileResolver::resolve (mNamPaths[1]).existsAsFile();
                const bool ok = exists && loadNamModel (mNamPaths[1], err, 1);
                if (! exists)
                    MissingFileReport::add ("NAM model", mNamPaths[1]);
                else if (! ok)
                    MissingFileReport::add ("NAM model (failed to load)", mNamPaths[1]);
                namirLog ("  loadNamModel slot=1 ok=" + juce::String (ok ? 1 : 0)
                          + " err='" + err + "'");
                err.clear();
            }
            if (mIrPaths [0].isNotEmpty())
            {
                const bool exists = ProjectFileResolver::resolve (mIrPaths[0]).existsAsFile();
                const bool ok = exists && loadImpulseResponse (mIrPaths[0], err, 0);
                if (! exists)
                    MissingFileReport::add ("Amp IR", mIrPaths[0]);
                else if (! ok)
                    MissingFileReport::add ("Amp IR (failed to load)", mIrPaths[0]);
                namirLog ("  loadImpulseResponse slot=0 ok=" + juce::String (ok ? 1 : 0)
                          + " err='" + err + "'");
                err.clear();
            }
            if (mIrPaths [1].isNotEmpty())
            {
                const bool exists = ProjectFileResolver::resolve (mIrPaths[1]).existsAsFile();
                const bool ok = exists && loadImpulseResponse (mIrPaths[1], err, 1);
                if (! exists)
                    MissingFileReport::add ("Amp IR", mIrPaths[1]);
                else if (! ok)
                    MissingFileReport::add ("Amp IR (failed to load)", mIrPaths[1]);
                namirLog ("  loadImpulseResponse slot=1 ok=" + juce::String (ok ? 1 : 0)
                          + " err='" + err + "'");
                err.clear();
            }

            // H-6d: restore both per-slot snapshots if present.  Snapshots
            // hold per-slot tone state (knobs + mic sim/placement params +
            // per-slot user-IR path).  After restore, push the active
            // slot's snapshot back into APVTS so knobs reflect the loaded
            // state.  Pre-H-6d projects have no snapshot tags; in that
            // case the snapshots stay at default-constructed values (which
            // matches the loaded APVTS state, so applying them is a no-op).
            auto slotA = apvts.state.getChildWithName ("SlotA");
            auto slotB = apvts.state.getChildWithName ("SlotB");
            if (slotA.isValid()) mSnapshots[0].fromValueTree (slotA);
            if (slotB.isValid()) mSnapshots[1].fromValueTree (slotB);
            mLastSlot = getActiveSlot();

            // Load EACH slot's Mic IR into its dedicated DSP buffer (per-slot
            // resident, instant switching at runtime).  This is the one-time
            // cost on project load; runtime slot switches don't reload.
            // QA-Fc: Mic B's per-slot user IR gets the identical treatment.
            for (int s = 0; s < 2; ++s)
            {
                // QA-Export Task 5: a missing or unreadable IR used to be skipped
                // in silence here -- the project-load path -- so the mic sim came
                // back reporting a user IR it never loaded.  `err` was captured
                // and discarded even when an existing file failed to load.
                const auto& path = mSnapshots[(size_t) s].micUserIrPath;
                if (path.isNotEmpty())
                {
                    const juce::File irFile = ProjectFileResolver::resolve (path);
                    if (! irFile.existsAsFile())
                        MissingFileReport::add ("Mic A user IR", path);
                    else if (! mMicSim.loadUserIr (irFile, err, s))
                        MissingFileReport::add ("Mic A user IR (failed to load)", path);
                }
                const auto& pathB = mSnapshots[(size_t) s].micbUserIrPath;
                if (pathB.isNotEmpty())
                {
                    const juce::File irFileB = ProjectFileResolver::resolve (pathB);
                    if (! irFileB.existsAsFile())
                        MissingFileReport::add ("Mic B user IR", pathB);
                    else if (! mMicSimB.loadUserIr (irFileB, err, s))
                        MissingFileReport::add ("Mic B user IR (failed to load)", pathB);
                }
            }
            // Sync the Mic Sim active-slot pointers to match ab_slot.
            mMicSim .setActiveSlot (mLastSlot);
            mMicSimB.setActiveSlot (mLastSlot);

            // Apply the active slot AFTER APVTS restore so knobs reflect
            // the active slot's saved tone (not whatever the last
            // captureSnapshot wrote).
            if (slotA.isValid() || slotB.isValid())
                applySnapshotToCurrent (mLastSlot);

            // H-6d: GLOBAL mic-IR fallback for pre-A/B-snapshot projects;
            // per-slot paths now live in the snapshots and are loaded by
            // applySnapshotToCurrent above.
            if (! slotA.isValid() && ! slotB.isValid())
            {
                const juce::String micIrPath
                    = apvts.state.getProperty ("mic_user_ir_path", {}).toString();
                if (micIrPath.isNotEmpty())
                {
                    const juce::File irFile = ProjectFileResolver::resolve (micIrPath);
                    if (! irFile.existsAsFile())
                        MissingFileReport::add ("Mic user IR", micIrPath);
                    else if (! loadUserMicIr (irFile, err))
                        MissingFileReport::add ("Mic user IR (failed to load)", micIrPath);
                }
            }

            // 2026-05-05 (Bug C fix): notify the editor so file-name labels
            // refresh.  setStateInformation reloaded the NAM/IR correctly
            // (audio works) but the editor's labels are only updated by
            // explicit browse/load calls - without this hook the user sees
            // "(no model loaded)" even though the model is active.  Posted
            // async so the audio thread (if it pumped this load via
            // project-load) doesn't run editor mutation directly.
            if (onStateRestored)
                juce::MessageManager::callAsync (
                    [cb = onStateRestored] { if (cb) cb(); });

            // Queued AFTER every hop the restore posted (message-queue order);
            // the lambda co-owns the flag so a torn-down processor is safe.
            juce::MessageManager::callAsync (
                [tok = mRestoringState] { tok->store (false, std::memory_order_release); });
        }
    }
}
