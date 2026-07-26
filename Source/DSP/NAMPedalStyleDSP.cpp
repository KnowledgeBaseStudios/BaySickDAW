#include "NAMPedalStyleDSP.h"
#include "../MissingFileReport.h"   // QA-Export Task 5
#include <NAM/get_dsp.h>
#include <NAM/dsp.h>
#include <filesystem>

NAMPedalStyleDSP::NAMPedalStyleDSP() = default;
NAMPedalStyleDSP::~NAMPedalStyleDSP() = default;

void NAMPedalStyleDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;
    mPrepared   = true;

    juce::dsp::ProcessSpec spec { sampleRate,
                                   (juce::uint32) juce::jmax (1, maxBlockSize),
                                   2 };

    mLowShelf .prepare (spec);
    mMidPeak  .prepare (spec);
    mHighShelf.prepare (spec);
    rebuildEQ();

    mMonoIn .resize ((size_t) juce::jmax (1, maxBlockSize), 0.0);
    mMonoOut.resize ((size_t) juce::jmax (1, maxBlockSize), 0.0);

    mDryScratch.setSize (2, juce::jmax (1, maxBlockSize), false, true, true);

    if (mNamActive) mNamActive->ResetAndPrewarm (sampleRate, maxBlockSize);
    if (mNamPending) mNamPending->ResetAndPrewarm (sampleRate, maxBlockSize);
}

void NAMPedalStyleDSP::reset()
{
    mLowShelf.reset();
    mMidPeak.reset();
    mHighShelf.reset();
    if (mNamActive && mPrepared) mNamActive->ResetAndPrewarm (mSampleRate, mMaxBlock);
}

void NAMPedalStyleDSP::rebuildEQ()
{
    auto low  = juce::dsp::IIR::Coefficients<float>::makeLowShelf
                    (mSampleRate, 100.0f,  0.7f,
                     juce::Decibels::decibelsToGain (mLowDb));
    auto mid  = juce::dsp::IIR::Coefficients<float>::makePeakFilter
                    (mSampleRate, 1000.0f, 0.7f,
                     juce::Decibels::decibelsToGain (mMidDb));
    auto high = juce::dsp::IIR::Coefficients<float>::makeHighShelf
                    (mSampleRate, 5000.0f, 0.7f,
                     juce::Decibels::decibelsToGain (mHighDb));
    *mLowShelf .state = *low;
    *mMidPeak  .state = *mid;
    *mHighShelf.state = *high;
}

void NAMPedalStyleDSP::setInputDb  (float db) { mInputDb  = juce::jlimit (-24.0f, 24.0f, db); }
void NAMPedalStyleDSP::setLowDb    (float db)
{
    db = juce::jlimit (-15.0f, 15.0f, db);
    if (! juce::approximatelyEqual (mLowDb, db)) { mLowDb = db; rebuildEQ(); }
}
void NAMPedalStyleDSP::setMidDb    (float db)
{
    db = juce::jlimit (-15.0f, 15.0f, db);
    if (! juce::approximatelyEqual (mMidDb, db)) { mMidDb = db; rebuildEQ(); }
}
void NAMPedalStyleDSP::setHighDb   (float db)
{
    db = juce::jlimit (-15.0f, 15.0f, db);
    if (! juce::approximatelyEqual (mHighDb, db)) { mHighDb = db; rebuildEQ(); }
}
void NAMPedalStyleDSP::setBlend    (float v)  { mBlend01  = juce::jlimit (0.0f, 1.0f, v); }
void NAMPedalStyleDSP::setOutputDb (float db) { mOutputDb = juce::jlimit (-24.0f, 12.0f, db); }

juce::String NAMPedalStyleDSP::getModelName() const
{
    if (mModelPath.isEmpty()) return {};
    // QA-Export Task 5: never present a name we did not actually load -- that
    // reads as "loaded" while the pedal does no amp modeling at all.
    if (mModelMissing)
        return juce::File (mModelPath).getFileNameWithoutExtension() + " (missing)";
    return juce::File (mModelPath).getFileNameWithoutExtension();
}

bool NAMPedalStyleDSP::loadModel (const juce::File& file, juce::String& outErr)
{
    if (! file.existsAsFile())
    {
        outErr = "File not found: " + file.getFullPathName();
        return false;
    }

    std::unique_ptr<nam::DSP> newModel;
    try
    {
        nam::dspData data;
        std::filesystem::path p (file.getFullPathName().toStdString());
        newModel = nam::get_dsp (p, data);
        if (! newModel)
        {
            outErr = "NAM core returned a null model.";
            return false;
        }

        if (mPrepared)
            newModel->ResetAndPrewarm (mSampleRate, mMaxBlock);
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
        // Drain any prior unswapped pending so we don't leak the unique_ptr.
        if (mSwapPending.load (std::memory_order_acquire))
        {
            // No way to safely drain on the message thread without coordinating
            // with audio.  But since the audio thread is the only one that
            // un-flags swapPending, if we're racing here, the prior pending
            // simply gets replaced by ours -- old pending unique_ptr destroys.
        }
        mNamPending = std::move (newModel);
        mModelPath  = file.getFullPathName();
        mSwapPending.store (true, std::memory_order_release);
    }

    return true;
}

void NAMPedalStyleDSP::drainPendingSwap()
{
    if (mSwapPending.load (std::memory_order_acquire))
    {
        std::swap (mNamActive, mNamPending);
        mSwapPending.store (false, std::memory_order_release);
        // mNamPending now holds the OLD active (or nullptr); will be released
        // next time loadModel runs.
    }
}

void NAMPedalStyleDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    drainPendingSwap();

    // Snapshot dry input for blend.
    if (mDryScratch.getNumChannels() < numCh || mDryScratch.getNumSamples() < n)
        mDryScratch.setSize (numCh, n, false, false, true);
    for (int ch = 0; ch < numCh; ++ch)
        mDryScratch.copyFrom (ch, 0, buffer, ch, 0, n);

    // ── Pre-model Input/Drive ───────────────────────────────────────────────
    if (! juce::approximatelyEqual (mInputDb, 0.0f))
    {
        const float g = juce::Decibels::decibelsToGain (mInputDb, -60.0f);
        buffer.applyGain (g);
    }

    // ── Mono-sum + NAM inference (only if a model is loaded) ────────────────
    if (mNamActive)
    {
        if ((int) mMonoIn .size() < n) mMonoIn .resize ((size_t) n);
        if ((int) mMonoOut.size() < n) mMonoOut.resize ((size_t) n);

        // Mono-sum input (average channels).
        const float scale = (numCh > 1) ? (1.0f / (float) numCh) : 1.0f;
        for (int i = 0; i < n; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                s += buffer.getReadPointer (ch)[i];
            mMonoIn[(size_t) i] = (double) (s * scale);
        }

        double* inP[1]  = { mMonoIn .data() };
        double* outP[1] = { mMonoOut.data() };
        try { mNamActive->process (inP, outP, n); }
        catch (...)
        {
            std::fill (mMonoOut.begin(), mMonoOut.begin() + n, 0.0);
        }

        // Spread NAM mono output across both channels (dual-mono).
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* dst = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
                dst[i] = (float) mMonoOut[(size_t) i];
        }
    }
    // else: no model loaded -- buffer carries the (input-gained) signal through

    // ── Post-model 3-band EQ ────────────────────────────────────────────────
    if (! juce::approximatelyEqual (mLowDb, 0.0f)
        || ! juce::approximatelyEqual (mMidDb, 0.0f)
        || ! juce::approximatelyEqual (mHighDb, 0.0f))
    {
        juce::dsp::AudioBlock<float> blk (buffer);
        auto sub = blk.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        mLowShelf .process (ctx);
        mMidPeak  .process (ctx);
        mHighShelf.process (ctx);
    }

    // ── Blend (dry/wet mix) ─────────────────────────────────────────────────
    if (! juce::approximatelyEqual (mBlend01, 1.0f))
    {
        const float wet = mBlend01;
        const float dry = 1.0f - mBlend01;
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* w = buffer.getWritePointer (ch);
            const float* d = mDryScratch.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
                w[i] = w[i] * wet + d[i] * dry;
        }
    }

    // ── Output trim ─────────────────────────────────────────────────────────
    if (! juce::approximatelyEqual (mOutputDb, 0.0f))
    {
        const float g = juce::Decibels::decibelsToGain (mOutputDb, -60.0f);
        buffer.applyGain (g);
    }
}

void NAMPedalStyleDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("NAMPedalStyleDSP");
    state.setProperty ("input",    mInputDb,        nullptr);
    state.setProperty ("low",      mLowDb,          nullptr);
    state.setProperty ("mid",      mMidDb,          nullptr);
    state.setProperty ("high",     mHighDb,         nullptr);
    state.setProperty ("blend",    mBlend01,        nullptr);
    state.setProperty ("output",   mOutputDb,       nullptr);
    state.setProperty ("modelPath", mModelPath,     nullptr);
    state.setProperty ("bypassed", (int) bypassed,  nullptr);
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void NAMPedalStyleDSP::setStateInformation (const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sz);
    if (! xml || ! xml->hasTagName ("NAMPedalStyleDSP")) return;
    auto state = juce::ValueTree::fromXml (*xml);

    setInputDb  ((float)(double) state.getProperty ("input",  0.0));
    setLowDb    ((float)(double) state.getProperty ("low",    0.0));
    setMidDb    ((float)(double) state.getProperty ("mid",    0.0));
    setHighDb   ((float)(double) state.getProperty ("high",   0.0));
    setBlend    ((float)(double) state.getProperty ("blend",  1.0));
    setOutputDb ((float)(double) state.getProperty ("output", 0.0));
    bypassed = ((int) state.getProperty ("bypassed", 0)) != 0;

    const auto path = state.getProperty ("modelPath", juce::String()).toString();
    if (path.isNotEmpty())
    {
        juce::File f (path);
        if (f.existsAsFile())
        {
            mModelMissing = false;
            juce::String err;
            loadModel (f, err);
        }
        else
        {
            // QA-Export Task 5: the path is remembered so the user can see WHICH
            // capture went missing, but the model is NOT loaded -- so the name is
            // now flagged missing (getModelName appends a marker) and reported.
            // Previously this displayed the remembered name with nothing behind
            // it, so the pedal looked loaded while producing no amp modeling.
            mModelPath    = path;
            mModelMissing = true;
            MissingFileReport::add ("NAM capture", path);
        }
    }
}
