#include "BaySickGuitarsProcessor.h"
#include "sfizz.hpp"
#include <functional>
#include <map>

namespace
{
inline juce::String makePrefix (int instIdx)
{
    return juce::String ("bgg_") + juce::String (instIdx) + "_";
}
}

BaySickGuitarsProcessor::BaySickGuitarsProcessor (int instIdx)
    : juce::AudioProcessor (BusesProperties()
                                .withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      mInstIdx     (instIdx),
      mPrefix      (makePrefix (instIdx)),
      mCcParamRoot (mPrefix + "cc"),
      apvts (*this, &mUndoManager, "BaySickGuitarsState", createLayout (mPrefix))
{
    mSfizz = std::make_unique<sfz::Sfizz>();
    mSfizz->setSampleRate     (static_cast<float>(mSampleRate));
    mSfizz->setSamplesPerBlock (mMaxBlockSize);
    // 2026-05-06 memory: lower preload from sfizz default (8192) to 4096
    // samples per region.  Rest streams from disk on-demand via the file
    // pool's background thread.  Modest memory win; safe on SSD, fine on
    // most HDDs at typical buffer sizes.
    mSfizz->setPreloadSize (4096);

    // Listen on every <prefix>cc<N> APVTS param so widget edits, automation,
    // and project state restore all funnel into a single sfizz CC dispatch
    // path.  Listener fires on the message thread; sfizz's cc() is documented
    // as message-thread safe when not concurrently rendering.
    for (int cc = 0; cc < kCcCount; ++cc)
        apvts.addParameterListener (mCcParamRoot + juce::String (cc), this);
}

BaySickGuitarsProcessor::~BaySickGuitarsProcessor()
{
    for (int cc = 0; cc < kCcCount; ++cc)
        apvts.removeParameterListener (mCcParamRoot + juce::String (cc), this);
}

void BaySickGuitarsProcessor::parameterChanged (const juce::String& paramId, float newValue)
{
    // <prefix>cc<N> → sfizz CC.  Pure dispatch.  Param IDs follow the form
    // `bgg_<instIdx>_cc<N>`, so `mCcParamRoot` is the concrete prefix to strip.
    if (! paramId.startsWith (mCcParamRoot)) return;
    const int cc = paramId.substring (mCcParamRoot.length()).getIntValue();
    if (cc < 0 || cc >= kCcCount) return;
    const int v  = juce::jlimit (0, 127, (int) std::round (newValue));
    if (mSfizz) mSfizz->cc (0, cc, v);
}

void BaySickGuitarsProcessor::sendCc (int cc, int value)
{
    if (cc < 0 || cc >= kCcCount) return;
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
            apvts.getParameter (mCcParamRoot + juce::String (cc))))
    {
        p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 ((float) value));
    }
}

int BaySickGuitarsProcessor::getNumActiveVoices() const noexcept
{
    if (! mProcessingEnabled.load (std::memory_order_acquire)) return 0;
    return mSfizz ? mSfizz->getNumActiveVoices() : 0;
}

// QA-Sfizz Task 2B (2026-05-27): iterates sfizz's public keyswitch-label
// vector (exposed via the BaySickDAW local-patch accessor on sfz::Sfizz
// from Task 2A) and returns the matching label by midiNote; empty if no match.
juce::String BaySickGuitarsProcessor::getKeyswitchLabel (int midiNote) const noexcept
{
    if (! mSfizz) return {};
    for (const auto& pair : mSfizz->getKeyswitchLabels())
        if (static_cast<int> (pair.first) == midiNote)
            return juce::String::fromUTF8 (pair.second.c_str());
    return {};
}

int BaySickGuitarsProcessor::getCcValue (int cc) const
{
    // Invalid (out-of-range) CC index -> 0.  Valid-but-unset CCs also default
    // to 0 (QA-Sfizz-Followup reverted Sub-E's blanket 64), but that is enforced
    // by the APVTS layout + getKitDefaultCc, not here.
    if (cc < 0 || cc >= kCcCount) return 0;
    if (auto* raw = apvts.getRawParameterValue (mCcParamRoot + juce::String (cc)))
        return juce::jlimit (0, 127, (int) std::round (raw->load()));
    return 0;
}

int BaySickGuitarsProcessor::getKitDefaultCc (int cc) const
{
    const juce::SpinLock::ScopedLockType lk (mCcKitDefaultLock);
    if (auto it = mCcKitDefault.find (cc); it != mCcKitDefault.end())
        return it->second;
    // QA-Sfizz-Followup (2026-06-01): unset CC -> 0 (off), reverting Sub-E's
    // blanket 64.  Double-click reset returns here for CCs the kit author did
    // not set_cc; 0 matches sfizz's natural default + the createLayout baseline.
    return 0;
}

juce::String BaySickGuitarsProcessor::getCcLabel (int cc) const
{
    const juce::SpinLock::ScopedLockType lk (mCcLabelLock);
    if (auto it = mCcLabel.find (cc); it != mCcLabel.end())
        return it->second;
    return {};
}

juce::AudioProcessorValueTreeState::ParameterLayout
BaySickGuitarsProcessor::createLayout (const juce::String& prefix)
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

    return { params.begin(), params.end() };
}

void BaySickGuitarsProcessor::prepareToPlay (double sr, int maxBlockSize)
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
}

bool BaySickGuitarsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        || out == juce::AudioChannelSet::mono();
}

void BaySickGuitarsProcessor::updateFromApvts()
{
    if (auto* p = apvts.getRawParameterValue (mPrefix + "outVol"))
    {
        const float v = p->load();
        if (v != mCache.outVol)
            mCache.outVol = v;
    }
}

void BaySickGuitarsProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numFrames   = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // K-5 fix #5: processing-enabled gate.  When false, the kit is mid-load;
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
        for (const auto meta : midi)
        {
            const auto& msg = meta.getMessage();
            const int delay = juce::jlimit (0, numFrames - 1, (int) meta.samplePosition);
            if      (msg.isNoteOn())             mSfizz->noteOn (delay, msg.getNoteNumber(), msg.getVelocity());
            else if (msg.isNoteOff())            mSfizz->noteOff (delay, msg.getNoteNumber(), msg.getVelocity());
            else if (msg.isController())         mSfizz->cc (delay, msg.getControllerNumber(), msg.getControllerValue());
            else if (msg.isPitchWheel())         mSfizz->pitchWheel (delay, msg.getPitchWheelValue());
            else if (msg.isChannelPressure())    mSfizz->channelAftertouch (delay, msg.getChannelPressureValue());
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                for (int n = 0; n < 128; ++n) mSfizz->noteOff (delay, n, 0);
            }
        }
    }
    midi.clear();

    buffer.clear();
    if (! mSfizz || numChannels < 1)
        return;

    // 2026-05-06 DSP gate: skip the renderBlock entirely when sfizz has no
    // active voices (no notes playing, no release tail, no sustain-held
    // voices).  MIDI was already dispatched above, so any fresh note-on this
    // block bumps the active-voice count above zero - we won't skip in that
    // case.  Saves ~all the per-block sfizz overhead on Inst tabs that aren't
    // currently producing notes.  Mono fallback path also benefits: buffer
    // was just cleared, leaving channel 0 silent matches the mono mix of
    // two silent channels.
    if (mSfizz->getNumActiveVoices() == 0)
        return;

    // Lazily resize the render scratch to the current block size.
    if (mRenderScratch.getNumSamples() < numFrames)
    {
        mRenderScratch.setSize (2, juce::jmax (numFrames, mMaxBlockSize),
                                /*keepContent=*/false, /*clearExtra=*/true,
                                /*avoidReallocating=*/false);
    }
    mRenderScratch.clear (0, numFrames);
    mRenderPtrs[0] = mRenderScratch.getWritePointer (0);
    mRenderPtrs[1] = mRenderScratch.getWritePointer (1);

    // Single stereo render path.  numOutputs=1 means one stereo pair (sfizz
    // writes channels 0+1 of mRenderPtrs).
    mSfizz->renderBlock (mRenderPtrs.data(),
                         static_cast<size_t> (numFrames),
                         /*numOutputs=*/1);

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

bool BaySickGuitarsProcessor::loadKit (const juce::File& sfzPath)
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
    // kit - e.g. user adjusts CC100 on Green to 90, switches to Black (which
    // never set_cc100); without the reset CC100 would stay at 90.  The reset
    // reaches sfizz via setValueNotifyingHost -> parameterChanged.
    for (int cc = 0; cc < kCcCount; ++cc)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                apvts.getParameter (mCcParamRoot + juce::String (cc))))
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (0.0f));
    }

    // Push kit defaults through APVTS - drives the parameterChanged listener
    // which forwards each value to sfizz.  Overrides the 0 reset above for
    // CCs the kit author explicitly set.
    for (auto& [cc, val] : kitDefaults)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                apvts.getParameter (mCcParamRoot + juce::String (cc))))
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 ((float) val));
    }

    return true;
}

void BaySickGuitarsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("BaySickGuitarsState");
    root.appendChild (apvts.copyState(), nullptr);

    if (mCurrentKitPath != juce::File())
    {
        juce::ValueTree kitNode ("KitPath");
        kitNode.setProperty ("path", mCurrentKitPath.getFullPathName(), nullptr);
        root.appendChild (kitNode, nullptr);
    }

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, dest);
}

void BaySickGuitarsProcessor::setStateInformation (const void* data, int sz)
{
    auto xml = getXmlFromBinary (data, sz);
    if (! xml || ! xml->hasTagName ("BaySickGuitarsState"))
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
        const juce::File kit (kitNode.getProperty ("path").toString());
        if (kit.existsAsFile())
            loadKit (kit);
    }

    if (auto apvtsState = root.getChildWithName (apvts.state.getType()); apvtsState.isValid())
        apvts.replaceState (apvtsState);
}
