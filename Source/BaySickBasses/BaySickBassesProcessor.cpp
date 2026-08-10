#include "BaySickBassesProcessor.h"
#include "../MissingFileReport.h"   // QA-Export Task 5
#include "../SampleLibrary.h"
#include "sfizz.hpp"
#include <functional>
#include <map>
#include <cmath>

namespace
{
inline juce::String makePrefix (int instIdx)
{
    return juce::String ("bbb_") + juce::String (instIdx) + "_";
}
}

BaySickBassesProcessor::BaySickBassesProcessor (int instIdx, juce::UndoManager& undoMgr)
    : juce::AudioProcessor (BusesProperties()
                                .withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      mInstIdx     (instIdx),
      mPrefix      (makePrefix (instIdx)),
      mCcParamRoot (mPrefix + "cc"),
      mUndoManager (undoMgr),
      apvts (*this, &mUndoManager, "BaySickBassesState", createLayout (mPrefix))
{
    // QA-UndoCoverage: stable undo identity (see BaySickGuitars).
    apvts.undoOwnerTag = "sfizz:" + mPrefix;
    mSfizz = std::make_unique<sfz::Sfizz>();
    mSfizz->setSampleRate     (static_cast<float>(mSampleRate));
    mSfizz->setSamplesPerBlock (mMaxBlockSize);
    // 2026-05-06 memory: see BaySickGuitars for rationale.
    mSfizz->setPreloadSize (4096);

    // Listen on every <prefix>cc<N> APVTS param so widget edits, automation,
    // and project state restore all funnel into a single sfizz CC dispatch
    // path.  The listener runs on the writer's thread and renderBlock may be
    // live at that moment, so it never touches sfizz itself -- it marks
    // mCcDirty and processBlock owns the actual mSfizz->cc() push.
    for (int cc = 0; cc < kCcCount; ++cc)
        apvts.addParameterListener (mCcParamRoot + juce::String (cc), this);

    // Task 12: cache the cc + cut-self param atomics once (layout is static,
    // pointers live as long as apvts) so audio-thread reads are lock-free.
    for (int cc = 0; cc < kCcCount; ++cc)
        mCcRaw[(size_t) cc] = apvts.getRawParameterValue (mCcParamRoot + juce::String (cc));
    mCutSelfRaw     = apvts.getRawParameterValue (mPrefix + "cutSelf");
    mCutSelfModeRaw = apvts.getRawParameterValue (mPrefix + "cutSelfMode");

    // The SlideSampler's block-rate CC reads ride the same atomics -- the kit
    // panel + automation drive the slide's _oncc modulation (and the cc105
    // Mono choke) for free.
    mSlideSampler.setCcProvider ([this] (int cc) -> float
    {
        if (cc < 0 || cc >= kCcCount) return 0.0f;
        auto* raw = mCcRaw[(size_t) cc];
        return raw != nullptr ? raw->load() : 0.0f;
    });
}

BaySickBassesProcessor::~BaySickBassesProcessor()
{
    for (int cc = 0; cc < kCcCount; ++cc)
        apvts.removeParameterListener (mCcParamRoot + juce::String (cc), this);
}

void BaySickBassesProcessor::parameterChanged (const juce::String& paramId, float newValue)
{
    // <prefix>cc<N> → sfizz CC, QUEUED: the mark is drained by processBlock so
    // sfizz's cc() only ever runs on the audio thread.  Param IDs follow the
    // form `bbb_<instIdx>_cc<N>`, so `mCcParamRoot` is the prefix to strip.
    if (! paramId.startsWith (mCcParamRoot)) return;
    const int cc = paramId.substring (mCcParamRoot.length()).getIntValue();
    if (cc < 0 || cc >= kCcCount) return;
    juce::ignoreUnused (newValue);
    mCcDirty[(size_t) (cc >> 6)].fetch_or (1ull << (cc & 63), std::memory_order_release);
}

int BaySickBassesProcessor::getNumActiveVoices() const noexcept
{
    if (! mProcessingEnabled.load (std::memory_order_acquire)) return 0;
    return mSfizz ? mSfizz->getNumActiveVoices() : 0;
}

// QA-Sfizz Task 2B (2026-05-27): iterates sfizz's public keyswitch-label
// vector (exposed via the BaySickDAW local-patch accessor on sfz::Sfizz
// from Task 2A) and returns the matching label by midiNote; empty if no match.
juce::String BaySickBassesProcessor::getKeyswitchLabel (int midiNote) const noexcept
{
    if (! mSfizz) return {};
    for (const auto& pair : mSfizz->getKeyswitchLabels())
        if (static_cast<int> (pair.first) == midiNote)
            return juce::String::fromUTF8 (pair.second.c_str());
    return {};
}

int BaySickBassesProcessor::getCcValue (int cc) const
{
    // Invalid (out-of-range) CC index -> 0.  Valid-but-unset CCs also default
    // to 0 (QA-Sfizz-Followup reverted Sub-E's blanket 64), but that is enforced
    // by the APVTS layout + getKitDefaultCc, not here.
    if (cc < 0 || cc >= kCcCount) return 0;
    if (auto* raw = apvts.getRawParameterValue (mCcParamRoot + juce::String (cc)))
        return juce::jlimit (0, 127, (int) std::round (raw->load()));
    return 0;
}

int BaySickBassesProcessor::getKitDefaultCc (int cc) const
{
    const juce::SpinLock::ScopedLockType lk (mCcKitDefaultLock);
    if (auto it = mCcKitDefault.find (cc); it != mCcKitDefault.end())
        return it->second;
    // QA-Sfizz-Followup (2026-06-01): unset CC -> 0 (off), reverting Sub-E's
    // blanket 64.  Double-click reset returns here for CCs the kit author did
    // not set_cc; 0 matches sfizz's natural default + the createLayout baseline.
    return 0;
}

juce::String BaySickBassesProcessor::getCcLabel (int cc) const
{
    const juce::SpinLock::ScopedLockType lk (mCcLabelLock);
    if (auto it = mCcLabel.find (cc); it != mCcLabel.end())
        return it->second;
    return {};
}

juce::AudioProcessorValueTreeState::ParameterLayout
BaySickBassesProcessor::createLayout (const juce::String& prefix)
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        prefix + "outVol", "Output Volume", 0.0f, 1.0f, 0.8f));

    // QA-Sfizz-Followup (2026-06-01): default 0, NOT 64 - reverts QA-Sfizz
    // Sub-E's blanket CC=64 default.  Sub-E forced every unset CC to 64 on the
    // theory that Karoryfer kits expect the Aria host's CC=64 default; in
    // practice that turns "amount" controls (feedback / muting / unison /
    // vibrato) half-on by default and sounds wrong.  A CC the kit leaves unset
    // means OFF (sfizz's natural 0); explicit non-zero defaults come via the
    // kit's `set_cc<N>` directives, which loadKit applies on top of this 0
    // baseline (including macro-defined ones like Rusty's set_cc4=$ht_lo_hi_init).
    // 2026-05-05 audit: range is kCcCount=512 so kit "extended CCs" >= 128 get
    // APVTS-bound the same way.
    const juce::String ccRoot = prefix + "cc";
    for (int cc = 0; cc < kCcCount; ++cc)
        params.push_back (std::make_unique<juce::AudioParameterInt> (
            ccRoot + juce::String (cc),
            "CC " + juce::String (cc),
            0, 127, 0));

    // Task 12 (G-12): cut-self.  Mode false = Same Pitch, true = Cut All.
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        prefix + "cutSelf", "Cut Self", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        prefix + "cutSelfMode", "Cut Self Mode", false));

    return { params.begin(), params.end() };
}

void BaySickBassesProcessor::prepareToPlay (double sr, int maxBlockSize)
{
    mSampleRate   = sr;
    mMaxBlockSize = maxBlockSize;
    if (mSfizz)
    {
        mSfizz->setSampleRate     (static_cast<float>(sr));
        mSfizz->setSamplesPerBlock (maxBlockSize);
    }
    mRenderScratch.setSize (2, maxBlockSize, false, true, false);
    mRenderPtrs.assign (2, nullptr);
    mSlideSampler.prepare (sr, maxBlockSize);
}

// QA-SlideSampler Task 3: RP Slide arm.  Suppress the sfizz anchor + set up the
// anchor->target pitch ramp; the SlideSampler takes over as a takeover (no
// re-pluck) crossfading in under the anchor's release.
void BaySickBassesProcessor::armSlide (int anchor, int target, int timeMs, int velocity, int delaySample)
{
    if (anchor < 0 || target < 0 || ! mSlideSampler.hasProgram())
        return;

    const double glideSamples = juce::jmax (1.0, timeMs * 0.001 * mSampleRate);
    const bool restruck = anchor < 128 && mNoteOnThisBlock[(size_t) anchor];

    // Chain continuation (base->A->B): every segment reports the same base anchor.
    // Retarget from the CURRENT pitch so the glide stays continuous + the band is
    // held (SL-4); do NOT re-trigger or re-suppress.  #7: a same-block anchor
    // noteOn means this is a COPIED slide, never a chain segment -- chains hold
    // one anchor noteOn across all segments -- so fall through to a fresh gesture.
    if (mSlideActive && anchor == mSlideAnchor && ! restruck)
    {
        mSlideTarget    = (double) target;
        mSlidePitchStep = (mSlideTarget - mSlidePitch) / glideSamples;
        return;
    }

    // G-13 tail policy: cut-self ON hard-cuts the previous gesture's voices;
    // OFF lets them ring through their AHDSR release under the new gesture.
    if (mCutSelfRaw != nullptr && mCutSelfRaw->load() >= 0.5f)
        mSlideSampler.stopAllNow();
    else if (mSlideActive)
        mSlideSampler.release();

    if (mSfizz) mSfizz->noteOff (delaySample, anchor, 0);   // suppress the sfizz anchor voice

    mSlideActive    = true;
    mSlideAnchor    = anchor;
    mSlidePitch     = (double) anchor;
    mSlideTarget    = (double) target;
    mSlidePitchStep = (mSlideTarget - mSlidePitch) / glideSamples;
    mSlideArmSample = delaySample;

    // #3: loudness anchors to the anchor NOTE's recorded velocity; the CC86
    // transport value only covers a gesture with no prior anchor noteOn.
    const int anchorVel = (anchor < 128 && mLastNoteVel[(size_t) anchor] > 0)
                            ? mLastNoteVel[(size_t) anchor] : velocity;

    // A restruck (copied) slide re-plucks: its sfizz anchor noteOn was
    // suppressed same-sample above, so the sampler supplies the attack.
    mSlideSampler.startSlide ((float) mSlidePitch, anchorVel, /*reAttack=*/restruck);
}

// QA-SlideSampler Task 4: arm the native pitch-wheel bend.  Scales the requested
// semitones to a wheel value using the patch's real bend range (cents), captured
// at load.  Bass reads sfizz default +/-2, so both directions work.
void BaySickBassesProcessor::armBend (int note, int semis, int shape, int timeMs)
{
    const int upCents = (mSlideRegions.bendUpCents == SlideRegionMap::kBendUnset)
                          ? 200 : mSlideRegions.bendUpCents;
    int downCents;
    if (mSlideRegions.bendDownCents == SlideRegionMap::kBendUnset) downCents = 200;
    else if (mSlideRegions.bendDownCents < 0) downCents = -mSlideRegions.bendDownCents;
    else                                      downCents = 0;   // positive bend_down = up-only

    int target = 0;
    if (semis > 0 && upCents > 0)
        target =  (int) juce::jlimit (0.0, 8191.0, (double) semis    * 100.0 / upCents   * 8192.0);
    else if (semis < 0 && downCents > 0)
        target = -(int) juce::jlimit (0.0, 8192.0, (double) (-semis) * 100.0 / downCents * 8192.0);

    mBendActive       = true;
    mBendNote         = note;
    mBendTargetWheel  = target;
    mBendShape        = shape;
    mBendSamplesTotal = juce::jmax (1, (int) (timeMs * 0.001 * mSampleRate));
    mBendSamplesDone  = 0;
}

bool BaySickBassesProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        || out == juce::AudioChannelSet::mono();
}

void BaySickBassesProcessor::updateFromApvts()
{
    if (auto* p = apvts.getRawParameterValue (mPrefix + "outVol"))
    {
        const float v = p->load();
        if (v != mCache.outVol)
            mCache.outVol = v;
    }
}

void BaySickBassesProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numFrames   = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // L-5 fix #5: processing-enabled gate.  When false, the kit is mid-load;
    // sfizz's internal hash maps are being mutated and renderBlock would crash
    // inside ControllerSource::generate.  Clear the buffer + drop incoming MIDI
    // and bail; PluginProcessor's wrapper flips this true once loadKit returns.
    if (! mProcessingEnabled.load (std::memory_order_acquire))
    {
        buffer.clear();
        midi.clear();
        return;
    }

    updateFromApvts();

    // Drain the message-thread CC marks ahead of MIDI dispatch + render.
    // Below the processing gate is load-bearing: marks made mid-loadKit flush
    // on the first block after the gate reopens, never into a half-parsed
    // engine.  Acquire pairs with the listener's release.
    if (mSfizz)
    {
        for (int w = 0; w < kCcDirtyWords; ++w)
        {
            auto bits = mCcDirty[(size_t) w].exchange (0, std::memory_order_acquire);
            for (int b = 0; bits != 0; ++b, bits >>= 1)
            {
                if ((bits & 1ull) == 0) continue;
                const int cc = (w << 6) | b;
                if (auto* raw = mCcRaw[(size_t) cc])
                    mSfizz->cc (0, cc, juce::jlimit (0, 127, (int) std::round (raw->load())));
            }
        }
    }

    // Audition exchange (UI → audio thread).  Velocity packed into the upper
    // byte; legacy callers (velocity=0 → unpacked 0) get bumped back to 100.
    const int packed = mAuditionNote.exchange (-1);
    if (packed >= 0 && mSfizz)
    {
        const int note     = packed & 0x7F;
        const int velRaw   = (packed >> 8) & 0x7F;
        const int velocity = velRaw > 0 ? velRaw : 100;
        for (int n = 0; n < 128; ++n)
            mSfizz->noteOff (0, n, 0);
        mSfizz->noteOn (0, note, velocity);
    }

    if (mSfizz)
    {
        // QA-SlideSampler Task 3: intercept the RP Slide transport (CC84 anchor,
        // CC5+CC37 14-bit glide ms, CC86 loudness, CC85 target) and drive the
        // blended SlideSampler instead of sfizz (which ignores those CCs).  CC85
        // arms; the anchor's later noteOff ends the gesture (SS-Q4(a) ring-out).
        // The transport CCs share one sample position in emit order, so the CC85
        // arm sees the anchor/time/vel already gathered.  Notes + all other CCs
        // (expression 10/71/72/74, pitch wheel, patch CCs) pass through to sfizz.
        int slAnchor = -1, slHi = -1, slLo = -1, slVel = 100, slBendAmt = -999, slBendShape = 0;
        mNoteOnThisBlock.reset();
        const bool cutSelfOn  = mCutSelfRaw     != nullptr && mCutSelfRaw->load()     >= 0.5f;
        const bool cutAllMode = mCutSelfModeRaw != nullptr && mCutSelfModeRaw->load() >= 0.5f;
        for (const auto meta : midi)
        {
            const auto& msg = meta.getMessage();
            const int delay = juce::jlimit (0, numFrames - 1, (int) meta.samplePosition);
            if (msg.isController())
            {
                const int cc = msg.getControllerNumber();
                const int cv = msg.getControllerValue();
                switch (cc)
                {
                    case 84: slAnchor = cv; continue;
                    case 5:  slHi = cv;     continue;
                    case 37: slLo = cv;     continue;
                    case 86: slVel = cv;         continue;
                    case 87: slBendAmt = cv - 64; continue;   // Task 4 bend amount (signed semitones)
                    case 88: slBendShape = cv;    continue;   // Task 4 bend shape
                    case 85:
                    {
                        const int timeMs = (slHi >= 0 && slLo >= 0) ? ((slHi << 7) | slLo) : 60;
                        armSlide (slAnchor, cv, timeMs, slVel, delay);
                        continue;
                    }
                    default: mSfizz->cc (delay, cc, cv); break;
                }
            }
            else if (msg.isNoteOn())
            {
                const int n = msg.getNoteNumber();
                mLastNoteVel[(size_t) n] = msg.getVelocity();   // #3 anchor loudness
                mNoteOnThisBlock[(size_t) n] = true;            // #7 copied-slide discriminator

                // #2 timbre half: a keyswitch noteOn swaps the sampler's
                // articulation tables for the NEXT gesture; the note still
                // passes to sfizz (its own keyswitch handling).  Keyswitches
                // are exempt from cut-self -- they're mode presses, not notes.
                const bool isKeyswitch = mSlideSampler.trySelectArticulation (n);

                // G-12 cut-self: new note cuts what's already sounding.  sfizz
                // voices exit via noteOff (their ampeg_release); slide voices
                // hard-stop through the declick ramp.
                if (cutSelfOn && ! isKeyswitch)
                {
                    if (cutAllMode)
                    {
                        for (int k = 0; k < 128; ++k) mSfizz->noteOff (delay, k, 0);
                        mSlideSampler.stopAllNow();
                    }
                    else
                    {
                        mSfizz->noteOff (delay, n, 0);
                        if (mSlideSampler.isActive() && mSlideSampler.currentNote() == n)
                            mSlideSampler.stopAllNow();
                    }
                }

                if (slBendAmt != -999)   // Task 4: this noteOn is a Bend note
                {
                    armBend (n, slBendAmt, slBendShape,
                             (slHi >= 0 && slLo >= 0) ? ((slHi << 7) | slLo) : 200);
                    slBendAmt = -999;
                }
                mSfizz->noteOn (delay, n, msg.getVelocity());
            }
            else if (msg.isNoteOff())
            {
                const int n = msg.getNoteNumber();
                if (mSlideActive && n == mSlideAnchor)   // slide-end: hand to the ring-out
                {
                    mSlideSampler.release();
                    mSlideActive = false;
                }
                if (mBendActive && n == mBendNote)       // bend-end: wheel back to center
                {
                    mSfizz->pitchWheel (delay, 0);
                    mBendActive = false;
                }
                mSfizz->noteOff (delay, n, msg.getVelocity());
            }
            // sfizz pitchWheel takes CENTERED -8192..+8192 (sfizz.hpp:550; normalizeBend
            // clamps +/-8191), NOT JUCE's raw 0..16383 -- raw 8192 reads as full bend up.
            else if (msg.isPitchWheel())      mSfizz->pitchWheel (delay, juce::jlimit (-8191, 8191, msg.getPitchWheelValue() - 8192));
            else if (msg.isChannelPressure()) mSfizz->channelAftertouch (delay, msg.getChannelPressureValue());
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                for (int n = 0; n < 128; ++n) mSfizz->noteOff (delay, n, 0);
                // #5: panic is a hard stop -- gesture AND ring-out tails die
                // through the declick ramp, not a lingering release.
                mSlideSampler.stopAllNow();
                mSlideActive = false;
                if (mBendActive)  { mSfizz->pitchWheel (delay, 0); mBendActive = false; }
            }
        }
    }
    midi.clear();

    // QA-SlideSampler Task 4: advance the Bend pitch-wheel ramp (per block; sfizz's
    // bend_smooth interpolates between updates) in the note's chosen shape.
    if (mBendActive && mSfizz)
    {
        mBendSamplesDone += numFrames;
        const double phase = juce::jlimit (0.0, 1.0, (double) mBendSamplesDone / (double) mBendSamplesTotal);
        // SS-Q5 TUNE: Ramp+Hold rise time - fixed ~120 ms, capped at the note, so a
        // long bend still rises quickly then holds (vs rising over 1/4 of a whole note).
        const double riseSamples = juce::jmin ((double) mBendSamplesTotal, 120.0 * 0.001 * mSampleRate);
        double w;
        switch (mBendShape)
        {
            case 1:  w = phase; break;                                            // Ramp (whole)
            case 2:  w = phase < 0.5 ? phase / 0.5 : (1.0 - phase) / 0.5; break;  // Up + Back
            case 3:  w = 1.0; break;                                              // Instant
            default: w = juce::jmin (1.0, (double) mBendSamplesDone / riseSamples); break;   // Ramp + Hold
        }
        const int wheel = juce::jlimit (-8191, 8191, (int) std::lround (w * mBendTargetWheel));
        mSfizz->pitchWheel (0, wheel);
    }

    buffer.clear();
    if (numChannels < 1)
        return;

    // 2026-05-06 DSP gate: skip rendering when neither sfizz nor the blended
    // slide is producing anything.  See BaySickGuitarsProcessor for the reasoning.
    const bool sfizzActive = mSfizz && mSfizz->getNumActiveVoices() > 0;
    const bool slideActive = mSlideActive || mSlideSampler.isActive();
    if (! sfizzActive && ! slideActive)
        return;

    // Lazily resize the render scratch to the current block size.
    if (mRenderScratch.getNumSamples() < numFrames)
    {
        mRenderScratch.setSize (2, juce::jmax (numFrames, mMaxBlockSize),
                                /*keepContent=*/false, /*clearExtra=*/true,
                                /*avoidReallocating=*/false);
    }
    mRenderScratch.clear (0, numFrames);

    if (sfizzActive)
    {
        mRenderPtrs[0] = mRenderScratch.getWritePointer (0);
        mRenderPtrs[1] = mRenderScratch.getWritePointer (1);
        // Single stereo render path.  numOutputs=1 means one stereo pair (sfizz
        // writes channels 0+1 of mRenderPtrs).
        mSfizz->renderBlock (mRenderPtrs.data(),
                             static_cast<size_t> (numFrames),
                             /*numOutputs=*/1);
    }

    // QA-SlideSampler Task 3: mix the blended slide on top of sfizz.  The
    // anchor->target pitch ramp advances per sub-block (kSlideChunk) so the glide
    // + zone crossfades stay smooth; the arming block starts at the CC85 sample.
    if (slideActive)
    {
        constexpr int kSlideChunk = 64;
        int pos = mSlideArmSample;
        while (pos < numFrames)
        {
            const int chunk = juce::jmin (kSlideChunk, numFrames - pos);
            if (mSlideActive)
            {
                mSlidePitch += mSlidePitchStep * (double) chunk;
                if ((mSlidePitchStep > 0.0 && mSlidePitch > mSlideTarget)
                    || (mSlidePitchStep < 0.0 && mSlidePitch < mSlideTarget))
                    mSlidePitch = mSlideTarget;
                mSlideSampler.moveTo ((float) mSlidePitch);
            }
            mSlideSampler.renderNextBlock (mRenderScratch, pos, chunk);
            pos += chunk;
        }
        mSlideArmSample = 0;
    }

    if (mCache.outVol >= 0.0f)
        mRenderScratch.applyGain (0, numFrames, mCache.outVol);

    if (numChannels >= 2)
    {
        buffer.copyFrom (0, 0, mRenderScratch, 0, 0, numFrames);
        buffer.copyFrom (1, 0, mRenderScratch, 1, 0, numFrames);
    }
    else
    {
        // Mono fallback: 0.5×L + 0.5×R into channel 0 via two addFrom calls
        // (copyFrom has no 7-arg gain overload - only addFrom does).
        buffer.addFrom (0, 0, mRenderScratch, 0, 0, numFrames, 0.5f);
        buffer.addFrom (0, 0, mRenderScratch, 1, 0, numFrames, 0.5f);
    }
}

bool BaySickBassesProcessor::loadKit (const juce::File& sfzPath)
{
    if (! sfzPath.existsAsFile() || ! mSfizz)
        return false;

    if (! mSfizz->loadSfzFile (sfzPath.getFullPathName().toStdString()))
        return false;

    mCurrentKitPath = sfzPath;

    // Seed CC defaults from the kit's `set_cc<N>=<int>` directives so the
    // ARIA panel + sfizz both start at the kit author's intended positions.
    // Walks #include chains depth-first up to depth 4 (covers
    // `program → default/<file> → mappings/<file>` nesting) and collects every
    // `set_cc<N>=<int>` it encounters.  Each is stamped into mCcKitDefault
    // (read-only snapshot for double-click reset) AND written through APVTS
    // (so the parameter listener pushes the value to sfizz and the panel knobs
    // paint at the right initial position).  Direct port of Rusty's scan.
    std::map<int, int>          kitDefaults;
    std::map<int, juce::String> kitLabels;
    std::map<juce::String, int> defines;   // SFZ `#define $name value` macros
    {
        std::function<void (const juce::File&, int)> scan;
        scan = [&] (const juce::File& f, int depth)
        {
            if (depth > 4 || ! f.existsAsFile()) return;
            const auto txt = f.loadFileAsString();
            juce::StringArray ls;
            ls.addLines (txt);
            for (const auto& raw : ls)
            {
                const auto t = raw.trim();
                if (t.startsWithIgnoreCase ("#define"))
                {
                    // SFZ macro `#define $name value`.  Captured so set_cc lines
                    // that reference $name resolve to the kit author's value
                    // (e.g. Big Rusty Drums' set_cc4=$ht_lo_hi_init).  Commented
                    // `//#define` lines are skipped - they do not start with '#'.
                    const auto rest = t.substring (7).trim();
                    const int  sp   = rest.indexOfChar (' ');
                    if (sp > 0)
                    {
                        const auto name = rest.substring (0, sp).trim();
                        if (name.startsWith ("$"))
                            defines[name] = rest.substring (sp + 1).trim().getIntValue();
                    }
                }
                else if (t.startsWithIgnoreCase ("set_cc"))
                {
                    const int eq = t.indexOfChar ('=');
                    if (eq > 6)
                    {
                        const int cc  = t.substring (6, eq).getIntValue();
                        const auto rhs = t.substring (eq + 1).trim();
                        // Resolve an SFZ `#define $macro` reference; a literal int
                        // passes straight through.  Unknown macro -> 0.
                        int resolved;
                        if (rhs.startsWith ("$"))
                        {
                            const auto it = defines.find (rhs);
                            resolved = (it != defines.end()) ? it->second : 0;
                        }
                        else
                            resolved = rhs.getIntValue();
                        const int val = juce::jlimit (0, 127, resolved);
                        if (cc >= 0 && cc < kCcCount) kitDefaults[cc] = val;
                    }
                }
                else if (t.startsWithIgnoreCase ("label_cc"))
                {
                    // Format: `label_cc<N>=<text>` - text runs to end-of-line,
                    // unquoted.  Used by ARIA hosts as the parameter's display
                    // name; we mirror that in tooltip + automation menu labels.
                    const int eq = t.indexOfChar ('=');
                    if (eq > 8)
                    {
                        const int cc = t.substring (8, eq).getIntValue();
                        auto label   = t.substring (eq + 1).trim();
                        if (cc >= 0 && cc < kCcCount && label.isNotEmpty())
                            kitLabels[cc] = label;
                    }
                }
                else if (t.startsWithIgnoreCase ("#include"))
                {
                    const int q1 = t.indexOfChar ('"');
                    const int q2 = (q1 >= 0) ? t.indexOfChar (q1 + 1, '"') : -1;
                    if (q1 >= 0 && q2 > q1)
                    {
                        const auto rel = t.substring (q1 + 1, q2);
                        scan (f.getParentDirectory().getChildFile (rel), depth + 1);
                    }
                }
            }
        };
        scan (sfzPath, 0);
    }
    {
        const juce::SpinLock::ScopedLockType lk (mCcKitDefaultLock);
        mCcKitDefault = kitDefaults;
    }
    {
        const juce::SpinLock::ScopedLockType lk (mCcLabelLock);
        mCcLabel = kitLabels;
    }
    // QA-Sfizz-Followup (2026-06-01): reset every CC to 0 (off) before applying
    // the kit's set_cc overrides - reverts Sub-E's reset-to-64.  Baseline 0 +
    // kit set_cc overrides = the kit author's intended state: unset CCs OFF
    // (sfizz's natural default), set_cc CCs at their declared value.  Sub-E's
    // blanket 64 turned "amount" controls (feedback / muting / unison / vibrato)
    // half-on by default and sounded wrong.  Reset is still required (vs leaving
    // prior values) because switching programs leaks values from the previous
    // kit - a CC the user moved on one program would persist into the next
    // program that never set_cc's it.  The reset reaches sfizz via
    // setValueNotifyingHost -> parameterChanged.
    // QA-UndoCoverage Task 6: kit-default pushes are programmatic (the kit
    // load itself becomes ONE structural transaction in Task 7).
    juce::AudioProcessorValueTreeState::ScopedProgrammaticParamWrites spw;
    for (int cc = 0; cc < kCcCount; ++cc)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                apvts.getParameter (mCcParamRoot + juce::String (cc))))
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (0.0f));
    }

    // Push kit defaults through APVTS - drives the parameterChanged listener,
    // which queues each value for processBlock's drain.  Overrides the 0 reset
    // above for CCs the kit author explicitly set.
    for (auto& [cc, val] : kitDefaults)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                apvts.getParameter (mCcParamRoot + juce::String (cc))))
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 ((float) val));
    }

    // QA-SlideSampler Task 1: build the blended-slide note->sample table for the
    // center-voice default-keyswitch sustain articulation of the loaded program.
    // Independent of the sfizz load above; consumed by the SlideSampler (Task 2)
    // and a no-op for normal playback.
    mSlideRegions = extractSlideRegions (sfzPath);
    mSlideSampler.setProgram (mSlideRegions);   // rebuild zone tables (processing gate off here)

    return true;
}

void BaySickBassesProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("BaySickBassesState");
    root.appendChild (apvts.copyState(), nullptr);

    if (mCurrentKitPath != juce::File())
    {
        juce::ValueTree kitNode ("KitPath");
        // Persist as a stable-root ref when the kit lives under Core Library
        // (every shipped kit does), so the saved path stops embedding the
        // Windows user name and resolves under any account.  Falls back to
        // absolute for anything outside.
        kitNode.setProperty ("path", SampleLibrary::refForPersist (mCurrentKitPath), nullptr);
        root.appendChild (kitNode, nullptr);
    }

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, dest);
}

void BaySickBassesProcessor::setStateInformation (const void* data, int sz)
{
    auto xml = getXmlFromBinary (data, sz);
    if (! xml || ! xml->hasTagName ("BaySickBassesState"))
        return;

    auto root = juce::ValueTree::fromXml (*xml);

    // Order matters when CC params live in APVTS.  loadKit walks the program
    // SFZ and stamps `set_cc<N>=<int>` defaults into every <prefix>cc<N> param,
    // overwriting whatever was there.  We need the user's saved CC values to
    // win, so:
    //   1) load the kit first (kit defaults overwrite anything in apvts)
    //   2) replaceState second (the project's saved CCs overlay the kit defaults)
    if (auto kitNode = root.getChildWithName ("KitPath"); kitNode.isValid())
    {
        // resolvePersistedRef passes a plain absolute path straight through, so
        // states written before the stable-ref switch keep loading.
        const juce::File kit = SampleLibrary::resolvePersistedRef (kitNode.getProperty ("path").toString());
        if (kit.existsAsFile())
            loadKit (kit);
        else
            // QA-Export Task 5: see the Guitars equivalent -- silent skip meant
            // the tab restored looking fine and produced nothing.
            MissingFileReport::add ("Bass kit", kit.getFullPathName());
    }

    if (auto apvtsState = root.getChildWithName (apvts.state.getType()); apvtsState.isValid())
        apvts.replaceStateKeepingUndoHistory (apvtsState);
}
