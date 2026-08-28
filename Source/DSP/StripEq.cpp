#include "StripEq.h"
#include "../SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)

void StripEq::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mMaxBlock   = maxBlockSize;
    mEq.prepare (sampleRate, maxBlockSize);
}

void StripEq::reset()
{
    mEq.reset();
}

void StripEq::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();
    if (numCh < 2 || numSamples <= 0) return;

    float* l = buffer.getWritePointer (0);
    float* r = buffer.getWritePointer (1);

    // Identity behaves like bypass but the spectrum feeds stay alive - a
    // skipped EQ with an empty analyser reads as broken (the EQ8MsDSP
    // convention, kept).
    if (bypassed || isIdentity())
    {
        preFeed .push (l, r, numSamples);
        postFeed.push (l, r, numSamples);
        return;
    }

    preFeed.push (l, r, numSamples);
    mEq.process (l, r, numSamples);
    postFeed.push (l, r, numSamples);
}

void StripEq::pushBand (int i, const kbs::EqBandParams& p)
{
    if (i < 0 || i >= kBands) return;
    auto& c = mCached[(size_t) i];

    const bool same =
        c.on == p.on && c.type == p.type && c.freqHz == p.freqHz
        && c.gainDb == p.gainDb && c.q == p.q && c.slope == p.slope
        && c.channel == p.channel && c.placement == p.placement
        && c.muted == p.muted && c.isolated == p.isolated
        && c.dynamic == p.dynamic && c.thresholdDb == p.thresholdDb
        && c.ratio == p.ratio && c.attackMs == p.attackMs
        && c.releaseMs == p.releaseMs && c.autoRelease == p.autoRelease
        && c.phaseMix == p.phaseMix
        && c.thresholdBDb == p.thresholdBDb && c.ratioB == p.ratioB
        && c.rangeBDb == p.rangeBDb && c.onsetMix == p.onsetMix
        && c.spectral == p.spectral && c.density == p.density
        && c.satAmt == p.satAmt
        && c.rangeDb == p.rangeDb && c.scExternal == p.scExternal
        && c.scSource == p.scSource;
    if (same) return;

    c = p;
    mEq.setBand (i, p);
}

kbs::EqBandParams StripEq::getBand (int i) const
{
    if (i < 0 || i >= kBands) return {};
    return mCached[(size_t) i];
}

void StripEq::pushGlobals (bool propQ, bool autoGain, float agAmount01,
                           float outGainDb, bool polarity,
                           int charMode, float charAmt)
{
    mEq.setProportionalQ (propQ);
    mEq.setAutoGain (autoGain, agAmount01);
    mEq.setOutputGainDb (outGainDb);
    mEq.setPolarityFlip (polarity);
    mEq.setCharacter ((kbs::EqCharMode) juce::jlimit (0, 3, charMode), charAmt);
    mAutoGain  = autoGain;
    mAgAmount  = agAmount01;
    mOutGainDb = outGainDb;
    mPolarity  = polarity;
}


void StripEq::setMode (kbs::EqMode m)
{
    mEq.setMode (m);
}

void StripEq::setOversampling (bool on)
{
    mEq.setOversampling (on);
}

void StripEq::resetToDefaults()
{
    for (int i = 0; i < kBands; ++i)
    {
        mCached[(size_t) i] = {};
        mEq.setBand (i, {});
        // B ships like A: an untouched spare is the default bank, not an empty
        // one, so swapping to B lands on the same 8 flat bands A opened with.
        mSpare[(size_t) i] = kbs::eqDefaultBand (i);
    }
    mViewingSpare = false;
    mSpareLocked  = false;
    mEq.setMode (kbs::EqMode::zeroLatency);
    mEq.setOversampling (false);
    pushGlobals (true, false, 1.0f, 0.0f, false, 0, 0.5f);
    mEq.setListenBand (-1);
    mEq.reset();
}

void StripEq::saveToSpare()
{
    if (mSpareLocked) return;
    mSpare = mCached;
}

void StripEq::swapWithSpare()
{
    // Params-only exchange: the caller pushes the swapped bank to the APVTS
    // right after, and the audio-thread sweep materializes it in the engine.
    std::swap (mSpare, mCached);
    for (int i = 0; i < kBands; ++i)
        mEq.setBand (i, mCached[(size_t) i]);
    mViewingSpare = ! mViewingSpare;
}

kbs::EqBandParams StripEq::getSpareBand (int i) const
{
    if (i < 0 || i >= kBands) return {};
    return mSpare[(size_t) i];
}

bool StripEq::isIdentity() const noexcept
{
    if (getLatencySamples() > 0) return false;   // B2: the delay must flow
    if (mEq.getListenBand() >= 0) return false;
    if (mAutoGain || mPolarity || mOutGainDb != 0.0f) return false;

    for (const auto& b : mCached)
    {
        if (! b.on || b.muted) continue;
        if (b.isolated) return false;            // isolate mutes the others
        if (b.dynamic) return false;
        if (! kbs::eqTypeHasGain (b.type)) return false;   // filters always shape
        if (b.gainDb != 0.0f) return false;
    }
    return true;
}

void StripEq::setSidechainBuffers (juce::AudioBuffer<float>* const* bufs, int count) noexcept
{
    DSPBase::setSidechainBuffers (bufs, count);

    // Forward each receive line into the engine's slot copies (SC-4).  The
    // engine invalidates the slots after every process(), so this runs per
    // block from the strip's pre-process push.  The analyser's SC overlay
    // watches one line: the picked slot, or the first connected one.
    const int wantSlot = scFeedSlot.load (std::memory_order_relaxed);
    bool fedSc = false;
    for (int s = 0; s < 4; ++s)
    {
        const auto* buf = (bufs != nullptr && s < count) ? bufs[s] : nullptr;
        if (buf == nullptr || buf->getNumSamples() <= 0)
        {
            mEq.setSidechainSlot (s, nullptr, nullptr, 0);
            continue;
        }
        const float* l = buf->getReadPointer (0);
        const float* r = buf->getNumChannels() > 1 ? buf->getReadPointer (1) : l;
        mEq.setSidechainSlot (s, l, r, buf->getNumSamples());
        if (! fedSc && (wantSlot == s || wantSlot < 0))
        {
            scFeed.push (l, r, buf->getNumSamples());
            fedSc = true;
        }
    }
    scFeedAlive.store (fedSc, std::memory_order_relaxed);
}

// ── serialization ────────────────────────────────────────────────────────────

juce::ValueTree StripEq::bandToTree (int index, const kbs::EqBandParams& p)
{
    juce::ValueTree t ("Band");
    t.setProperty ("index",    index,               nullptr);
    t.setProperty ("on",       p.on,                nullptr);
    t.setProperty ("type",     (int) p.type,        nullptr);
    t.setProperty ("freq",     p.freqHz,            nullptr);
    t.setProperty ("gain",     p.gainDb,            nullptr);
    t.setProperty ("q",        p.q,                 nullptr);
    t.setProperty ("slope",    p.slope,             nullptr);
    t.setProperty ("channel",  (int) p.channel,     nullptr);
    t.setProperty ("place",    p.placement,         nullptr);
    t.setProperty ("muted",    p.muted,             nullptr);
    t.setProperty ("isolated", p.isolated,          nullptr);
    t.setProperty ("dynamic",  p.dynamic,           nullptr);
    t.setProperty ("thr",      p.thresholdDb,       nullptr);
    t.setProperty ("ratio",    p.ratio,             nullptr);
    t.setProperty ("atk",      p.attackMs,          nullptr);
    t.setProperty ("rel",      p.releaseMs,         nullptr);
    t.setProperty ("relAuto",  p.autoRelease,       nullptr);
    t.setProperty ("range",    p.rangeDb,           nullptr);
    t.setProperty ("scSource", p.scSource,          nullptr);
    t.setProperty ("phase",    p.phaseMix,          nullptr);
    t.setProperty ("thrB",     p.thresholdBDb,      nullptr);
    t.setProperty ("ratioB",   p.ratioB,            nullptr);
    t.setProperty ("rangeB",   p.rangeBDb,          nullptr);
    t.setProperty ("onset",    p.onsetMix,          nullptr);
    t.setProperty ("spectral", p.spectral,          nullptr);
    t.setProperty ("density",  p.density,           nullptr);
    t.setProperty ("sat",      p.satAmt,            nullptr);
    return t;
}

void StripEq::bandFromTree (const juce::XmlElement& e, kbs::EqBandParams& p)
{
    p.on          = e.getBoolAttribute   ("on",       p.on);
    p.type        = (kbs::EqType) juce::jlimit (0, 8, e.getIntAttribute ("type", (int) p.type));
    p.freqHz      = (float) e.getDoubleAttribute ("freq",  p.freqHz);
    p.gainDb      = (float) e.getDoubleAttribute ("gain",  p.gainDb);
    p.q           = (float) e.getDoubleAttribute ("q",     p.q);
    p.slope       = juce::jlimit (1.0f, kbs::kEqSlopeBrickwallDb,
                                  (float) e.getDoubleAttribute ("slope", p.slope));
    p.channel     = (kbs::EqChannel) juce::jlimit (0, 4, e.getIntAttribute ("channel", (int) p.channel));
    p.placement   = juce::jlimit (-1.0f, 1.0f, (float) e.getDoubleAttribute ("place", p.placement));
    p.muted       = e.getBoolAttribute   ("muted",    p.muted);
    p.isolated    = e.getBoolAttribute   ("isolated", p.isolated);
    p.dynamic     = e.getBoolAttribute   ("dynamic",  p.dynamic);
    p.thresholdDb = (float) e.getDoubleAttribute ("thr",   p.thresholdDb);
    p.ratio       = (float) e.getDoubleAttribute ("ratio", p.ratio);
    p.attackMs    = (float) e.getDoubleAttribute ("atk",   p.attackMs);
    p.phaseMix    = juce::jlimit (0.0f, 1.0f, (float) e.getDoubleAttribute ("phase", p.phaseMix));
    p.thresholdBDb = juce::jlimit (-60.0f, 0.0f, (float) e.getDoubleAttribute ("thrB", p.thresholdBDb));
    p.ratioB      = juce::jlimit (1.0f, 20.0f, (float) e.getDoubleAttribute ("ratioB", p.ratioB));
    p.rangeBDb    = juce::jlimit (-30.0f, 30.0f, (float) e.getDoubleAttribute ("rangeB", p.rangeBDb));
    p.onsetMix    = juce::jlimit (0.0f, 1.0f, (float) e.getDoubleAttribute ("onset", p.onsetMix));
    p.spectral    = e.getBoolAttribute   ("spectral", p.spectral);
    p.density     = juce::jlimit (0.0f, 1.0f, (float) e.getDoubleAttribute ("density", p.density));
    p.satAmt      = juce::jlimit (0.0f, 1.0f, (float) e.getDoubleAttribute ("sat", p.satAmt));
    p.releaseMs   = (float) e.getDoubleAttribute ("rel",   p.releaseMs);
    p.autoRelease = e.getBoolAttribute   ("relAuto",  p.autoRelease);
    p.rangeDb     = (float) e.getDoubleAttribute ("range", p.rangeDb);
    p.scSource    = juce::jlimit (-1, 3, e.getIntAttribute ("scSource", p.scSource));
}

void StripEq::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("StripEq");
    for (int i = 0; i < kBands; ++i)
        state.appendChild (bandToTree (i, mCached[(size_t) i]), nullptr);

    // Spare bank nested under ONE child so the load loop cannot mistake it
    // for the live bank (the EQ8DSP lesson, kept).
    juce::ValueTree spare ("Spare");
    for (int i = 0; i < kBands; ++i)
        spare.appendChild (bandToTree (i, mSpare[(size_t) i]), nullptr);
    state.appendChild (spare, nullptr);

    state.setProperty ("viewingSpare", mViewingSpare,          nullptr);
    state.appendChild (mViewTree.createCopy(), nullptr);
    state.setProperty ("mode",         (int) mEq.getMode(),    nullptr);
    state.setProperty ("charMode",     (int) mEq.getCharMode(), nullptr);
    state.setProperty ("charAmt",      mEq.getCharAmount(),     nullptr);
    state.setProperty ("os",           mEq.getOversampling(),  nullptr);
    state.setProperty ("propQ",        mEq.getProportionalQ(), nullptr);
    state.setProperty ("autoGain",     mAutoGain,              nullptr);
    state.setProperty ("agAmt",        mAgAmount,              nullptr);
    state.setProperty ("outGain",      mOutGainDb,             nullptr);
    state.setProperty ("polarity",     mPolarity,              nullptr);

    auto xml = state.createXml();
    if (xml) juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void StripEq::setStateInformation (const void* data, int sz)
{
    auto xml = SafeXml::parseBinaryBlob (data, sz);
    if (! xml || xml->getTagName() != "StripEq") return;   // old blobs reset (SC-14)

    resetToDefaults();

    for (auto* child : xml->getChildIterator())
    {
        if (child->getTagName() == "Band")
        {
            const int idx = child->getIntAttribute ("index", -1);
            if (idx < 0 || idx >= kBands) continue;
            kbs::EqBandParams p;
            bandFromTree (*child, p);
            mCached[(size_t) idx] = p;
            mEq.setBand (idx, p);
        }
        else if (child->getTagName() == "View")
        {
            mViewTree.copyPropertiesAndChildrenFrom (juce::ValueTree::fromXml (*child),
                                                     nullptr);
        }
        else if (child->getTagName() == "Spare")
        {
            for (auto* sb : child->getChildIterator())
            {
                const int idx = sb->getIntAttribute ("index", -1);
                if (idx < 0 || idx >= kBands) continue;
                kbs::EqBandParams p;
                bandFromTree (*sb, p);
                mSpare[(size_t) idx] = p;
            }
        }
    }

    mViewingSpare = xml->getBoolAttribute ("viewingSpare", false);
    mEq.setMode ((kbs::EqMode) juce::jlimit (0, 7, xml->getIntAttribute ("mode", 0)));
    mEq.setCharacter ((kbs::EqCharMode) juce::jlimit (0, 3, xml->getIntAttribute ("charMode", 0)),
                      (float) xml->getDoubleAttribute ("charAmt", 0.5));
    mEq.setOversampling (xml->getBoolAttribute ("os", false));
    pushGlobals (xml->getBoolAttribute ("propQ", true),
                 xml->getBoolAttribute ("autoGain", false),
                 (float) xml->getDoubleAttribute ("agAmt", 1.0),
                 (float) xml->getDoubleAttribute ("outGain", 0.0),
                 xml->getBoolAttribute ("polarity", false),
                 juce::jlimit (0, 3, xml->getIntAttribute ("charMode", 0)),
                 (float) xml->getDoubleAttribute ("charAmt", 0.5));
}
