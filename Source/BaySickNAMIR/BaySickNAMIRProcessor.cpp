#include "BaySickNAMIRProcessor.h"
#include "BaySickNAMIREditor.h"

// NAM core (C++20 internal; consumer translation unit is C++17 — only the
// header pulls in Eigen / nlohmann, both of which are C++17-clean).
#include <NAM/get_dsp.h>
#include <NAM/dsp.h>

#include <cmath>
#include <filesystem>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
// BaySickNAMIRProcessor — Phase G-1.3 implementation.
// ─────────────────────────────────────────────────────────────────────────────

BaySickNAMIRProcessor::BaySickNAMIRProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BaySickNAMIR", createLayout())
{
    // Explicit atomic init — see the comment on mNamSwapPending in the header.
    for (auto& a : mNamSwapPending) a.store (false);
    for (auto& a : mNamLoaded)      a.store (false);
    for (auto& a : mNamIsFullRig)   a.store (false);
    for (auto& a : mIrLoaded)       a.store (false);

    // Listen for OS factor changes so we can re-Reset the NAM models on the
    // message thread (prepareToPlay won't re-fire when only a param changes).
    apvts.addParameterListener ("oversampling", this);
}

BaySickNAMIRProcessor::~BaySickNAMIRProcessor()
{
    apvts.removeParameterListener ("oversampling", this);
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
bool         BaySickNAMIRProcessor::hasImpulseResponse (int slot) const { return mIrLoaded    [(size_t) resolveSlot (slot)].load(); }
bool         BaySickNAMIRProcessor::isFullRig          (int slot) const { return mNamIsFullRig[(size_t) resolveSlot (slot)].load(); }
juce::String BaySickNAMIRProcessor::getNamFilePath     (int slot) const { return mNamPaths    [(size_t) resolveSlot (slot)]; }
juce::String BaySickNAMIRProcessor::getIrFilePath      (int slot) const { return mIrPaths     [(size_t) resolveSlot (slot)]; }
void         BaySickNAMIRProcessor::setNamFilePath     (const juce::String& p, int slot) { mNamPaths[(size_t) resolveSlot (slot)] = p; }
void         BaySickNAMIRProcessor::setIrFilePath      (const juce::String& p, int slot) { mIrPaths [(size_t) resolveSlot (slot)] = p; }

// ─────────────────────────────────────────────────────────────────────────────
// prepareToPlay — size scratch + warm filters / convolution / NAM / OS / gate.
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

    // Scratch — sized to the LARGEST block we might see, including 4x oversampling.
    const int maxBlockOS = maxBlockSize * 4;
    mNamMonoIn .assign ((size_t) maxBlockOS, 0.0);
    mNamMonoOut.assign ((size_t) maxBlockOS, 0.0);
    mMonoFloatBuf.setSize (1, maxBlockSize, false, false, true);
    mDryBuf      .setSize (2, maxBlockSize, false, false, true);

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
            catch (...) { /* model rejects rate — leave loaded but it'll glitch */ }
        }
    }

    // Gate state — invalidate cache so first block recomputes.
    mGateEnv           = 0.0f;
    mLastGateThreshDb  = std::numeric_limits<float>::quiet_NaN();
    mLastGateReleaseMs = std::numeric_limits<float>::quiet_NaN();

    setLatencySamples (oversamplingLatencySamples (osFactor));
}

void BaySickNAMIRProcessor::rebuildOversampling()
{
    using OS = juce::dsp::Oversampling<float>;
    // Mono — we oversample post-mono-sum.  Polyphase IIR is the standard cheap
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
// processBlock — full Phase G-1.3 chain.
// ─────────────────────────────────────────────────────────────────────────────
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

    // ── 4. Noise gate (BEFORE NAM — gate the dry guitar, not the saturation)
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

    // ── 5. NAM inference — mono in/out; broadcast to both channels ───────────
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
            // Direct path — host-rate NAM.
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

    // ── 7. Cabinet (IR) — wet/dry mix on cab_mix, bypass-aware ───────────────
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
// loadNamModel / loadImpulseResponse — message-thread file I/O + swap pattern.
// ─────────────────────────────────────────────────────────────────────────────
bool BaySickNAMIRProcessor::loadNamModel (const juce::String& filePath, juce::String& outErr, int slot)
{
    slot = resolveSlot (slot);

    if (filePath.isEmpty())
    {
        outErr = "No file path provided.";
        return false;
    }
    if (! juce::File (filePath).existsAsFile())
    {
        outErr = "File not found: " + filePath;
        return false;
    }

    std::unique_ptr<nam::DSP> newModel;
    bool isFullRigFlag = false;
    try
    {
        nam::dspData data;
        std::filesystem::path p (filePath.toStdString());
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
        mLastNamError = outErr;
        return false;
    }
    catch (...)
    {
        outErr = "NAM load failed: unknown error.";
        mLastNamError = outErr;
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
    mLastNamError = {};
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
    juce::File f (filePath);
    if (! f.existsAsFile())
    {
        outErr = "File not found: " + filePath;
        return false;
    }

    if (! mPrepared)
    {
        mIrPaths[(size_t) slot] = filePath;
        outErr  = "Audio device not yet prepared; IR will load on next prepare.";
        mLastIrError = outErr;
        return false;
    }

    try
    {
        mIr[(size_t) slot].loadImpulseResponse (
            f,
            juce::dsp::Convolution::Stereo   ::yes,
            juce::dsp::Convolution::Trim     ::yes,
            0,
            juce::dsp::Convolution::Normalise::yes);
    }
    catch (const std::exception& ex)
    {
        outErr = juce::String ("IR load failed: ") + ex.what();
        mLastIrError = outErr;
        return false;
    }
    catch (...)
    {
        outErr = "IR load failed: unknown error.";
        mLastIrError = outErr;
        return false;
    }

    mIrLoaded[(size_t) slot].store (true, std::memory_order_release);
    mIrPaths[(size_t) slot] = filePath;
    mLastIrError = {};
    return true;
}

void BaySickNAMIRProcessor::clearNamModel (int slot)
{
    slot = resolveSlot (slot);
    const juce::ScopedLock lk (mLoadLock);
    for (int spin = 0; spin < 1000 && mNamSwapPending[(size_t) slot].load (std::memory_order_acquire); ++spin)
        juce::Thread::sleep (1);

    std::unique_ptr<nam::DSP> oldPending = std::move (mNamPending[(size_t) slot]);
    juce::ignoreUnused (oldPending);
    mNamPending  [(size_t) slot].reset();
    mNamLoaded   [(size_t) slot].store (false, std::memory_order_release);
    mNamIsFullRig[(size_t) slot].store (false, std::memory_order_release);
    mNamSwapPending[(size_t) slot].store (true, std::memory_order_release);
    mNamPaths[(size_t) slot] = {};
}

void BaySickNAMIRProcessor::clearImpulseResponse (int slot)
{
    slot = resolveSlot (slot);
    mIrLoaded[(size_t) slot].store (false, std::memory_order_release);
    mIrPaths[(size_t) slot] = {};
}

// ─────────────────────────────────────────────────────────────────────────────
// APVTS listener — oversampling factor change re-Resets loaded NAM models at
// the new effective rate.  Fires from whatever thread APVTS hands us; the
// actual work runs via callAsync on the message thread because Reset can
// allocate.
// ─────────────────────────────────────────────────────────────────────────────
void BaySickNAMIRProcessor::parameterChanged (const juce::String& paramID, float newValue)
{
    if (paramID == "oversampling")
    {
        const int newFactor = juce::jlimit (0, 2, (int) newValue);
        juce::MessageManager::callAsync ([this, newFactor]()
        {
            this->reResetNamForOversampling (newFactor);
        });
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
            catch (...) { /* swallow — model is left in whatever state Reset reached */ }
        }
    }

    setLatencySamples (oversamplingLatencySamples (factor));
}

// ─────────────────────────────────────────────────────────────────────────────
// State (un)serialization — paths persist as ValueTree custom string props.
// Property keys stay backward-compatible with G-1.1/G-1.2 single-slot layouts.
// ─────────────────────────────────────────────────────────────────────────────
void BaySickNAMIRProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("nam_filepath",   mNamPaths[0], nullptr);
    state.setProperty ("ir_filepath",    mIrPaths [0], nullptr);
    state.setProperty ("nam_filepath_b", mNamPaths[1], nullptr);
    state.setProperty ("ir_filepath_b",  mIrPaths [1], nullptr);
    if (auto xml = std::unique_ptr<juce::XmlElement> (state.createXml()))
        copyXmlToBinary (*xml, destData);
}

void BaySickNAMIRProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            mNamPaths[0] = apvts.state.getProperty ("nam_filepath",   {}).toString();
            mIrPaths [0] = apvts.state.getProperty ("ir_filepath",    {}).toString();
            mNamPaths[1] = apvts.state.getProperty ("nam_filepath_b", {}).toString();
            mIrPaths [1] = apvts.state.getProperty ("ir_filepath_b",  {}).toString();

            juce::String err;
            if (mNamPaths[0].isNotEmpty()) loadNamModel        (mNamPaths[0], err, 0);
            if (mNamPaths[1].isNotEmpty()) loadNamModel        (mNamPaths[1], err, 1);
            if (mIrPaths [0].isNotEmpty()) loadImpulseResponse (mIrPaths [0], err, 0);
            if (mIrPaths [1].isNotEmpty()) loadImpulseResponse (mIrPaths [1], err, 1);
            juce::ignoreUnused (err);
        }
    }
}
