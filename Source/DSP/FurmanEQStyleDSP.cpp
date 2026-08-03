#include "FurmanEQStyleDSP.h"

constexpr float FurmanEQStyleDSP::kFreqMin[FurmanEQStyleDSP::kNumBands];
constexpr float FurmanEQStyleDSP::kFreqMax[FurmanEQStyleDSP::kNumBands];
constexpr float FurmanEQStyleDSP::kFreqDef[FurmanEQStyleDSP::kNumBands];

FurmanEQStyleDSP::FurmanEQStyleDSP() = default;

void FurmanEQStyleDSP::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec { sampleRate,
                                   (juce::uint32) juce::jmax (1, maxBlockSize),
                                   2 };

    for (int i = 0; i < kNumBands; ++i)
    {
        mBands[i].prepare (spec);
        rebuildBand (i);
    }
}

void FurmanEQStyleDSP::reset()
{
    for (auto& b : mBands) b.reset();
}

void FurmanEQStyleDSP::rebuildBand (int idx)
{
    if (idx < 0 || idx >= kNumBands) return;

    // -60 dB acts as -inf (silent kill); above that use linear gain.
    const float gain = (mBoost[idx] <= -59.99f)
                          ? 0.0001f
                          : juce::Decibels::decibelsToGain (mBoost[idx]);

    auto coefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter
                    (mSampleRate, mFreq[idx], mQ[idx], gain);
    *mBands[idx].state = *coefs;
}

void FurmanEQStyleDSP::setFreq (int idx, float hz)
{
    if (idx < 0 || idx >= kNumBands) return;
    // jlimit passes NaN (every comparison is false), and a NaN here bakes into
    // the biquad coefficients and the saved project.  Debug-asserts so the
    // WRITER is on the stack (same guard as the two graphic EQs sharing this
    // slot).
    if (! std::isfinite (hz)) { jassertfalse; return; }
    hz = juce::jlimit (kFreqMin[idx], kFreqMax[idx], hz);
    if (! juce::approximatelyEqual (mFreq[idx], hz))
    {
        mFreq[idx] = hz;
        rebuildBand (idx);
    }
}

void FurmanEQStyleDSP::setBoostDb (int idx, float db)
{
    if (idx < 0 || idx >= kNumBands) return;
    if (! std::isfinite (db)) { jassertfalse; return; }
    db = juce::jlimit (-60.0f, 20.0f, db);
    if (! juce::approximatelyEqual (mBoost[idx], db))
    {
        mBoost[idx] = db;
        rebuildBand (idx);
    }
}

void FurmanEQStyleDSP::setQ (int idx, float q)
{
    if (idx < 0 || idx >= kNumBands) return;
    if (! std::isfinite (q)) { jassertfalse; return; }
    q = juce::jlimit (0.2f, 3.8f, q);   // QA-EffectsReview Task 3: PQ-3 Q range
    if (! juce::approximatelyEqual (mQ[idx], q))
    {
        mQ[idx] = q;
        rebuildBand (idx);
    }
}

float FurmanEQStyleDSP::getFreq (int idx) const
{
    return (idx >= 0 && idx < kNumBands) ? mFreq[idx] : 0.0f;
}

float FurmanEQStyleDSP::getBoostDb (int idx) const
{
    return (idx >= 0 && idx < kNumBands) ? mBoost[idx] : 0.0f;
}

float FurmanEQStyleDSP::getQ (int idx) const
{
    return (idx >= 0 && idx < kNumBands) ? mQ[idx] : 0.7f;
}

void FurmanEQStyleDSP::setInputVolDb (float db)
{
    if (! std::isfinite (db)) { jassertfalse; return; }
    // QA-EffectsReview Task 3: preamp capped at +26 dB (was +86); the full PQ-3
    // range emerges from preamp + boost bands + Hi switch.
    mInputVolDb = juce::jlimit (0.0f, kInputMaxDb, db);
}

void FurmanEQStyleDSP::setEqBypassed (bool yes)
{
    mEqBypassed = yes;
}

void FurmanEQStyleDSP::setGainRange (GainRange r)
{
    mGainRange = r;   // QA-EffectsReview Task 3: Hi/Lo output gain-range switch
}

void FurmanEQStyleDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    // ── Preamp: master input volume + sigmoid soft-clip ─────────────────────
    // QA-EffectsReview Task 3: the +86 dB total range emerges from this preamp
    // (now capped at +26 dB) PLUS the boost bands + the Hi switch, not the
    // preamp alone.  We track the pre-tanh peak for the overload LED.
    if (mInputVolDb > 0.001f)
    {
        const float preGain = juce::Decibels::decibelsToGain (mInputVolDb);
        float blockPeak = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* p = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
            {
                const float driven = p[i] * preGain;                 // pre-tanh drive
                blockPeak = juce::jmax (blockPeak, std::abs (driven));
                // tanh smoothly limits the boosted signal.  At low gain
                // tanh(x) ~= x so this is transparent below ~0 dBFS.
                p[i] = std::tanh (driven);
            }
        }
        mClipLevel.store (blockPeak, std::memory_order_relaxed);
    }
    else
    {
        mClipLevel.store (0.0f, std::memory_order_relaxed);          // no preamp -> no overload
    }

    // ── 3 parametric bands (only if EQ-bypass switch is OFF) ────────────────
    if (! mEqBypassed)
    {
        juce::dsp::AudioBlock<float> blk (buffer);
        auto sub = blk.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> ctx (sub);

        for (auto& b : mBands) b.process (ctx);
    }

    // ── Hi/Lo output gain-range switch (+20 dB in Hi) ───────────────────────
    // QA-EffectsReview Task 3.  Applied to the whole output regardless of the
    // EQ-bypass switch (it is an output gain stage, not part of the band block).
    if (mGainRange == GainRange::Hi)
        buffer.applyGain (juce::Decibels::decibelsToGain (20.0f));
}

void FurmanEQStyleDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("FurmanEQStyleDSP");
    for (int i = 0; i < kNumBands; ++i)
    {
        state.setProperty ("freq"  + juce::String (i), mFreq[i],  nullptr);
        state.setProperty ("boost" + juce::String (i), mBoost[i], nullptr);
        state.setProperty ("q"     + juce::String (i), mQ[i],     nullptr);
    }
    state.setProperty ("input",     mInputVolDb,   nullptr);
    state.setProperty ("eqBypass",  (int) mEqBypassed, nullptr);
    state.setProperty ("gainRange", (int) mGainRange,  nullptr);   // QA-EffectsReview Task 3
    state.setProperty ("bypassed",  (int) bypassed,    nullptr);
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void FurmanEQStyleDSP::setStateInformation (const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sz);
    if (! xml || ! xml->hasTagName ("FurmanEQStyleDSP")) return;
    auto state = juce::ValueTree::fromXml (*xml);

    for (int i = 0; i < kNumBands; ++i)
    {
        setFreq    (i, (float)(double) state.getProperty ("freq"  + juce::String (i), kFreqDef[i]));
        setBoostDb (i, (float)(double) state.getProperty ("boost" + juce::String (i), 0.0));
        setQ       (i, (float)(double) state.getProperty ("q"     + juce::String (i), 0.7));
    }
    setInputVolDb ((float)(double) state.getProperty ("input", 0.0));
    setEqBypassed (((int) state.getProperty ("eqBypass", 0)) != 0);
    setGainRange  (((int) state.getProperty ("gainRange", 0)) != 0 ? GainRange::Hi : GainRange::Lo);  // Task 3
    bypassed = ((int) state.getProperty ("bypassed", 0)) != 0;
}
