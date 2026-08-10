#include "NAMPedalStyleDSP.h"
#include "../MissingFileReport.h"   // QA-Export Task 5
#include "../ProjectFileResolver.h"
#include <NAM/get_dsp.h>
#include <NAM/dsp.h>
#include <filesystem>

NAMPedalStyleDSP::NAMPedalStyleDSP() = default;
NAMPedalStyleDSP::~NAMPedalStyleDSP() = default;

void NAMPedalStyleDSP::prepare (double sampleRate, int maxBlockSize)
{
    // Not the audio thread: every caller either holds the rack's slot lock
    // (which shuts audio out for the duration) or is still building an
    // unpublished DSP.  mLoadLock is what stops a concurrent loadModel from
    // reassigning mNamPending out from under the prewarm at the bottom of
    // this function.
    const juce::ScopedLock lk (mLoadLock);

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

    // Whatever currently sits in pending leaves through this local so it
    // destructs on the message thread, outside every lock.  A nam::DSP
    // teardown frees its weight tensors -- far too much work to run under a
    // lock the audio thread try-locks every block.
    std::unique_ptr<nam::DSP> leaving;

    {
        const juce::ScopedLock lk (mLoadLock);

        {
            // mSwapLock is the only exclusion between this write and the audio
            // thread's swap of the same pointer.  Holding it means pending is
            // either a model audio never swapped in, or the old active audio
            // demoted on its last drain -- in both cases unreachable from the
            // audio thread, so taking ownership of it here is safe.
            const juce::SpinLock::ScopedLockType lkAudio (mSwapLock);
            leaving     = std::move (mNamPending);
            mNamPending = std::move (newModel);
            mSwapPending.store (true, std::memory_order_release);
        }

        mModelPath = file.getFullPathName();
        // The marker has to clear on EVERY successful load, not just the one
        // inside setStateInformation: a user who re-picks a working capture on
        // a pedal flagged missing would otherwise keep reading "(missing)" over
        // a capture that is loaded and audible.
        mModelMissing = false;
    }
    // `leaving` destructs here -- outside every lock.

    return true;
}

void NAMPedalStyleDSP::unloadModel()
{
    // Same publish discipline as loadModel with a null model: the audio thread
    // takes it on its next drain and process() falls through to the dry path
    // (Input/Drive, EQ, blend and Output trim still apply -- they are the
    // user's settings, not the capture's).  The demoted model parks in
    // mNamPending exactly as it would between two real loads, and leaves
    // through the next load's `leaving`.
    std::unique_ptr<nam::DSP> leaving;

    {
        const juce::ScopedLock lk (mLoadLock);

        {
            const juce::SpinLock::ScopedLockType lkAudio (mSwapLock);
            leaving = std::move (mNamPending);
            mSwapPending.store (true, std::memory_order_release);
        }

        mModelPath.clear();
        mModelMissing = false;
    }
    // `leaving` destructs here -- outside every lock.
}

void NAMPedalStyleDSP::drainPendingSwap()
{
    // AUDIO THREAD.  The flag pre-check keeps the common case (no swap
    // outstanding) free of any lock traffic whatsoever.
    if (! mSwapPending.load (std::memory_order_acquire)) return;

    // Try-lock, never block.  loadModel holds mSwapLock for two pointer moves
    // and nothing else, so losing the race just means the drain is skipped for
    // one block and the new model goes live on the next one -- inaudible, and
    // strictly better than the render thread waiting on the message thread.
    const juce::SpinLock::ScopedTryLockType lk (mSwapLock);
    if (! lk.isLocked()) return;

    if (mSwapPending.load (std::memory_order_acquire))
    {
        std::swap (mNamActive, mNamPending);
        mSwapPending.store (false, std::memory_order_release);
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
    if (path.isEmpty())
    {
        // A state that names no capture has to LAND.  Skipping the branch left
        // the previously-loaded model running under the previous name, so a
        // model-less slot state restored as whatever the slot happened to hold.
        unloadModel();
    }
    else
    {
        const juce::File f = ProjectFileResolver::resolve (path);
        if (f.existsAsFile())
        {
            juce::String err;
            if (loadModel (f, err))
            {
                // loadModel records the absolute file it opened; the reference
                // that was SAVED is what has to survive the next save, or a
                // bundled project rewrites itself back to this machine's paths.
                mModelPath    = path;
                mModelMissing = false;
            }
            else
            {
                // The capture named by this state did not install, so the one
                // already running has to go with it: leaving it published means
                // the pedal renders the PREVIOUS capture under the new name,
                // which is a sound the user never picked and a mismatch the
                // panel cannot show.  unloadModel clears the path it drops, so
                // the remembered reference is stamped back afterwards.
                unloadModel();
                mModelPath    = path;
                mModelMissing = true;
                MissingFileReport::add ("NAM capture (failed to load)", path);
            }
        }
        else
        {
            // QA-Export Task 5: the path is remembered so the user can see WHICH
            // capture went missing, but nothing is installed behind it -- the
            // name is flagged missing (getModelName appends a marker) and
            // reported.  unloadModel is what makes "nothing installed" true:
            // the branch used to leave whatever the slot was already running
            // audible under the missing name.
            unloadModel();
            mModelPath    = path;
            mModelMissing = true;
            MissingFileReport::add ("NAM capture", path);
        }
    }
}
