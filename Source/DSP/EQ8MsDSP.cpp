#include "EQ8MsDSP.h"

// 5F-9 sec.12 Phase 2 (12h): seed per-band channel defaults so today's behaviour
// is preserved exactly (mMid bands route to channel=Mid, mSide bands to
// channel=Side). Users override per-band via the widget / APVTS _channel param.
EQ8MsDSP::EQ8MsDSP()
{
    for (int i = 0; i < EQ8DSP::kNumBands; ++i)
    {
        mMid .setBandChannel(i, EQ8DSP::Channel::Mid);
        mSide.setBandChannel(i, EQ8DSP::Channel::Side);
    }
    // 2026-04-19: seed spare bank to match the main's per-band channel routing
    // so A/B compare starts both banks on Mid (mid tab) / Side (side tab) -
    // not the raw Band default of Stereo. Without this, swapping to B on a
    // mid tab silently re-routes all 8 bands to Stereo, which sounds nothing
    // like A even though "B is empty" - confusing on first compare flip.
    mMid .saveToSpare();
    mSide.saveToSpare();
}

void EQ8MsDSP::prepare(double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;
    mMid.prepare(sampleRate, maxBlockSize);
    mSide.prepare(sampleRate, maxBlockSize);
    // 12i: pre-allocate mono-mixdown scratch for the feed taps.
    mFeedScratch.setSize(1, maxBlockSize, false, true, true);
}

void EQ8MsDSP::reset()
{
    mMid.reset();
    mSide.reset();
}

void EQ8MsDSP::process(juce::AudioBuffer<float>& buffer)
{
    // 2026-04-22 §P4.3: identity short-circuit moved INSIDE process() so the
    // spectrum analyser feeds (preFeed / postFeed) keep getting populated even
    // when no IIR work runs.  Without this, isIdentity-skipped EQs showed
    // empty waveform displays in the UI.  Identity behaves like bypass - same
    // input on both feeds, no filter compute.
    if (bypassed || isIdentity())
    {
        // Keep the spectrum analyser alive even while the EQ bypasses audio /
        // is identity - both pre and post see the same signal.
        const int numSamples = buffer.getNumSamples();
        const int numCh      = buffer.getNumChannels();
        if (numCh > 0 && numSamples > 0 && mFeedScratch.getNumSamples() >= numSamples)
        {
            const float* L = buffer.getReadPointer(0);
            const float* R = numCh > 1 ? buffer.getReadPointer(1) : L;
            float* M = mFeedScratch.getWritePointer(0);
            for (int n = 0; n < numSamples; ++n)
                M[n] = 0.5f * (L[n] + R[n]);
            preFeed .push(M, numSamples);
            postFeed.push(M, numSamples);
        }
        return;
    }

    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();
    if (numCh < 2) return;   // M/S routing is only meaningful for stereo

    // 12i: pre-EQ spectrum tap. Mono-mixdown of the stereo input before processing.
    if (mFeedScratch.getNumSamples() >= numSamples)
    {
        const float* L = buffer.getReadPointer(0);
        const float* R = buffer.getReadPointer(1);
        float* M = mFeedScratch.getWritePointer(0);
        for (int n = 0; n < numSamples; ++n)
            M[n] = 0.5f * (L[n] + R[n]);
        preFeed.push(M, numSamples);
    }

    // 12h: process both inner EQ8DSPs in place on the FULL stereo buffer.
    // Per-band channel dispatch (Stereo / Mid / Side / LOnly / ROnly) now
    // happens inside each EQ8DSP::process(). No outer M/S encode/decode.
    // Biquad chains are LTI so running mMid then mSide (vs interleaved) yields
    // the same output given each band owns its own filter state.
    mMid .process(buffer);
    mSide.process(buffer);

    // 12i: post-EQ spectrum tap.
    if (mFeedScratch.getNumSamples() >= numSamples)
    {
        const float* L = buffer.getReadPointer(0);
        const float* R = buffer.getReadPointer(1);
        float* M = mFeedScratch.getWritePointer(0);
        for (int n = 0; n < numSamples; ++n)
            M[n] = 0.5f * (L[n] + R[n]);
        postFeed.push(M, numSamples);
    }
}

// ── Serialisation ─────────────────────────────────────────────────────────────

void EQ8MsDSP::getStateInformation(juce::MemoryBlock& dest)
{
    juce::ValueTree state("EQ8MsDSP");

    auto appendEQ = [&](const char* tag, EQ8DSP& eq)
    {
        juce::MemoryBlock mb;
        eq.getStateInformation(mb);
        juce::String b64 = juce::Base64::toBase64(mb.getData(), mb.getSize());
        state.setProperty(tag, b64, nullptr);
    };

    appendEQ("mid",  mMid);
    appendEQ("side", mSide);

    auto xml = state.createXml();
    if (xml) juce::AudioProcessor::copyXmlToBinary(*xml, dest);
}

void EQ8MsDSP::setStateInformation(const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary(data, sz);
    if (!xml || xml->getTagName() != "EQ8MsDSP") return;

    auto loadEQ = [&](const char* tag, EQ8DSP& eq)
    {
        juce::String b64 = xml->getStringAttribute(tag);
        if (b64.isEmpty()) return;
        juce::MemoryBlock mb;
        juce::MemoryOutputStream mos(mb, false);
        if (juce::Base64::convertFromBase64(mos, b64))
            eq.setStateInformation(mb.getData(), (int)mb.getSize());
    };

    loadEQ("mid",  mMid);
    loadEQ("side", mSide);
}
