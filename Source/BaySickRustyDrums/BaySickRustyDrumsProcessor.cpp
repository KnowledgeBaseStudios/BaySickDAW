#include "BaySickRustyDrumsProcessor.h"
#include "../Standalone/UndoBracket.h"
#include "../MissingFileReport.h"   // QA-Export Task 5
#include "../SampleLibrary.h"       // QA-ProjectSave Task 5: stable-root kit refs
#include "sfizz.hpp"
#include <set>
#include <map>

namespace
{
constexpr const char* kPrefix = "brd_";

inline juce::String pid (const char* name) { return juce::String (kPrefix) + name; }
}

BaySickRustyDrumsProcessor::BaySickRustyDrumsProcessor (juce::UndoManager& undoMgr)
    : juce::AudioProcessor (BusesProperties()
                                .withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      mUndoManager (undoMgr),
      apvts (*this, &mUndoManager, "BaySickRustyDrumsState", createLayout())
{
    // QA-UndoCoverage: stable undo identity -- the Rusty singleton is rebuilt
    // on every program switch/kit load; the tag lets its undo entries land on
    // the NEW instance (redo-across-resurrection fix).
    apvts.undoOwnerTag = "rusty";
    mSfizz = std::make_unique<sfz::Sfizz>();
    mSfizz->setSampleRate     (static_cast<float>(mSampleRate));
    mSfizz->setSamplesPerBlock (mMaxBlockSize);
    // 2026-05-06 memory: see BaySickGuitars for rationale.  Drum kits hit
    // their preload less than melodic kits (samples are short - full file
    // often within preload, retriggers don't extend tail), so this also has
    // less risk on Rusty than on long-sustained guitar/bass content.
    mSfizz->setPreloadSize (4096);

    // J-8 stage 2 (2026-05-04): listen on every brd_cc<N> APVTS param so widget
    // edits, automation, and project state restore all funnel into a single
    // sfizz CC dispatch path.  Listener fires on the message thread; sfizz's
    // cc() is documented as message-thread safe when not concurrently rendering.
    for (int cc = 0; cc < kCcCount; ++cc)
        apvts.addParameterListener ("brd_cc" + juce::String (cc), this);
}

BaySickRustyDrumsProcessor::~BaySickRustyDrumsProcessor()
{
    for (int cc = 0; cc < kCcCount; ++cc)
        apvts.removeParameterListener ("brd_cc" + juce::String (cc), this);
}

void BaySickRustyDrumsProcessor::parameterChanged (const juce::String& paramId, float newValue)
{
    // brd_cc<N> -> sfizz CC.  Pure dispatch; mCcState mirrors APVTS for project
    // serialization + the panel's getCcValue queries.
    if (! paramId.startsWith ("brd_cc")) return;
    const int cc = paramId.substring (6).getIntValue();
    if (cc < 0 || cc >= kCcCount) return;
    const int v  = juce::jlimit (0, 127, (int) std::round (newValue));
    {
        const juce::SpinLock::ScopedLockType lk (mCcStateLock);
        mCcState[cc] = v;
    }
    if (mSfizz) mSfizz->cc (0, cc, v);
}

void BaySickRustyDrumsProcessor::setHiHatPedalClosed (bool closed)
{
    mHiHatPedalClosed.store (closed);
    // CC4 = hi-hat pedal.  Route through APVTS so the change is undoable and
    // serializes with project state; the parameter listener forwards to sfizz.
    sendCc (4, closed ? 0 : 127);
}

// J-8 stage 2 (2026-05-04): ARIA control panel CC dispatch.  Writes through
// APVTS so the change is undoable + automatable + serialized with project state.
// The parameterChanged listener forwards the new value to the sfizz instance.
void BaySickRustyDrumsProcessor::sendCc (int cc, int value)
{
    if (cc < 0 || cc >= kCcCount) return;
    beginParamUndoGesture (&mUndoManager, "brd_cc" + juce::String (cc)); // Task 6 (12-iv)
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
            apvts.getParameter ("brd_cc" + juce::String (cc))))
    {
        p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 ((float) value));
    }
}

int BaySickRustyDrumsProcessor::getNumActiveVoices() const noexcept
{
    return mSfizz ? mSfizz->getNumActiveVoices() : 0;
}

// QA-Sfizz Task 2A (2026-05-27): iterates sfizz's public keyswitch-label
// vector (exposed via the BaySickDAW local-patch accessor on sfz::Sfizz)
// and returns the matching label by midiNote; empty if no match.
juce::String BaySickRustyDrumsProcessor::getKeyswitchLabel (int midiNote) const noexcept
{
    if (! mSfizz) return {};
    for (const auto& pair : mSfizz->getKeyswitchLabels())
        if (static_cast<int> (pair.first) == midiNote)
            return juce::String::fromUTF8 (pair.second.c_str());
    return {};
}

int BaySickRustyDrumsProcessor::getCcValue (int cc) const
{
    // Invalid (out-of-range) CC index -> 0.  Valid-but-unset CCs also default
    // to 0 (QA-Sfizz-Followup reverted Sub-E's blanket 64), but that is enforced
    // by the APVTS layout + getKitDefaultCc, not here.
    if (cc < 0 || cc >= kCcCount) return 0;
    if (auto* raw = apvts.getRawParameterValue ("brd_cc" + juce::String (cc)))
        return juce::jlimit (0, 127, (int) std::round (raw->load()));
    return 0;
}

int BaySickRustyDrumsProcessor::getKitDefaultCc (int cc) const
{
    const juce::SpinLock::ScopedLockType lk (mCcKitDefaultLock);
    if (auto it = mCcKitDefault.find (cc); it != mCcKitDefault.end())
        return it->second;
    // QA-Sfizz-Followup (2026-06-01): unset CC -> 0 (off), reverting Sub-E's
    // blanket 64.  Double-click reset returns here for CCs the kit author did
    // not set_cc; 0 matches sfizz's natural default + the createLayout baseline.
    return 0;
}

juce::String BaySickRustyDrumsProcessor::getCcLabel (int cc) const
{
    const juce::SpinLock::ScopedLockType lk (mCcLabelLock);
    if (auto it = mCcLabel.find (cc); it != mCcLabel.end())
        return it->second;
    return {};
}

juce::AudioProcessorValueTreeState::ParameterLayout
BaySickRustyDrumsProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        pid ("outVol"), "Output Volume", 0.0f, 1.0f, 0.8f));

    // J-8 stage 2 (2026-05-04): register one Int param per MIDI CC the ARIA
    // surface might address.
    // QA-Sfizz-Followup (2026-06-01): default 0, NOT 64 - reverts QA-Sfizz
    // Sub-E's blanket CC=64 default.  Sub-E forced every unset CC to 64 on the
    // theory that Karoryfer kits expect the Aria host's CC=64 default; in
    // practice that turns "amount" controls (feedback / muting / unison /
    // vibrato) half-on by default and sounds wrong.  A CC the kit leaves unset
    // means OFF (sfizz's natural 0); explicit non-zero defaults come via the
    // kit's `set_cc<N>` directives, which loadKit applies on top of this 0
    // baseline (including macro-defined ones like Big Rusty Drums'
    // set_cc4=$ht_lo_hi_init for the hi-hat pedal position).
    // 2026-05-05 audit: range is kCcCount=512 so kit "extended CCs" >= 128
    // (e.g. Big Rusty Drums CC400/401) get APVTS-bound the same way.
    // IDs `brd_cc0..(kCcCount-1)`.
    for (int cc = 0; cc < kCcCount; ++cc)
        params.push_back (std::make_unique<juce::AudioParameterInt> (
            "brd_cc" + juce::String (cc),
            "CC " + juce::String (cc),
            0, 127, 0));

    return { params.begin(), params.end() };
}

void BaySickRustyDrumsProcessor::prepareToPlay (double sr, int blockSize)
{
    mSampleRate   = sr;
    mMaxBlockSize = blockSize;
    if (mSfizz)
    {
        mSfizz->setSampleRate     (static_cast<float>(sr));
        mSfizz->setSamplesPerBlock (blockSize);
    }
    if (! mChannels.empty())
    {
        const int neededCh = 2 * (int) mChannels.size();
        mMultiOutScratch.setSize (neededCh, blockSize, false, true, false);
        mMultiOutPtrs.assign ((size_t) neededCh, nullptr);
    }
}

bool BaySickRustyDrumsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        || out == juce::AudioChannelSet::mono();
}

void BaySickRustyDrumsProcessor::updateFromApvts()
{
    if (auto* p = apvts.getRawParameterValue ("brd_outVol"))
    {
        const float v = p->load();
        if (v != mCache.outVol)
            mCache.outVol = v;
    }
}

void BaySickRustyDrumsProcessor::processStrips (int numFrames, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    updateFromApvts();

    // Audition exchange (UI → audio thread).  J-8b: velocity packed into
    // the upper byte of the atomic; legacy callers (velocity=0 → unpacked
    // 0) get bumped back to 100 to match the pre-J-8b default.
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
            // J-8 stage 2 (2026-05-04): forward CC and pitch wheel events from
            // the piano-roll dispatch path so per-note panning (CC10), filter
            // cutoff (CC74), and finePitch (PitchWheel) actually reach sfizz.
            else if (msg.isController())         mSfizz->cc (delay, msg.getControllerNumber(), msg.getControllerValue());
            // sfizz pitchWheel takes CENTERED -8192..+8192 (sfizz.hpp:550; normalizeBend
            // clamps +/-8191), NOT JUCE's raw 0..16383 -- raw 8192 reads as full bend up.
            else if (msg.isPitchWheel())         mSfizz->pitchWheel (delay, juce::jlimit (-8191, 8191, msg.getPitchWheelValue() - 8192));
            else if (msg.isChannelPressure())    mSfizz->channelAftertouch (delay, msg.getChannelPressureValue());
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                for (int n = 0; n < 128; ++n) mSfizz->noteOff (delay, n, 0);
            }
        }
    }
    midi.clear();

    const int stripCount = (int) mChannels.size();
    if (stripCount <= 0 || ! mSfizz)
    {
        if (mMultiOutScratch.getNumChannels() > 0)
            mMultiOutScratch.clear();
        return;
    }

    // Resize scratch lazily to current strip count + numFrames.  We always
    // allocate `2*stripCount` channels so the per-strip view accessor still
    // works; only strip 0 receives audio in this single-output mode.
    const int neededCh = 2 * stripCount;
    if (mMultiOutScratch.getNumChannels() < neededCh
        || mMultiOutScratch.getNumSamples() < numFrames)
    {
        mMultiOutScratch.setSize (neededCh, juce::jmax (numFrames, mMaxBlockSize),
                                  /*keepContent=*/false, /*clearExtra=*/true,
                                  /*avoidReallocating=*/false);
        mMultiOutPtrs.resize ((size_t) neededCh);
    }
    mMultiOutScratch.clear (0, numFrames);
    // Bumped HERE, not at the end: past this line the scratch is guaranteed
    // fresh for this block, including the zero-voice early-out below (which
    // returns silence, and silence that was just written is still fresh).
    mStripRenderSeq.fetch_add (1, std::memory_order_release);
    for (int i = 0; i < neededCh; ++i)
        mMultiOutPtrs[(size_t) i] = mMultiOutScratch.getWritePointer (i);

    // 2026-05-06 DSP gate: skip the multi-out renderBlock when sfizz has no
    // active voices.  Buffer was just cleared so per-strip views read
    // silence - same effective output, but skipping the per-piece SFZ
    // routing + voice scan is a sizable win for Rusty (13 strips).
    if (mSfizz->getNumActiveVoices() == 0)
        return;

    // J-7b: multi-output render.  Each piece's regions route to output=N
    // via wrapper SFZ injection.  sfizz writes 2*stripCount channels in
    // (L0,R0,L1,R1,...) order into mMultiOutScratch; PluginProcessor
    // fans each pair through its dedicated Rusty InsertNode.
    mSfizz->renderBlock (mMultiOutPtrs.data(),
                         static_cast<size_t> (numFrames),
                         stripCount);

    if (mCache.outVol >= 0.0f)
        mMultiOutScratch.applyGain (0, numFrames, mCache.outVol);
}

juce::AudioBuffer<float>
BaySickRustyDrumsProcessor::getStripBuffer (int stripIdx, int numFrames)
{
    juce::AudioBuffer<float> view;
    if (stripIdx < 0 || stripIdx >= (int) mChannels.size())
        return view;
    const int chBase = 2 * stripIdx;
    if (mMultiOutScratch.getNumChannels() < chBase + 2
        || mMultiOutScratch.getNumSamples() < numFrames)
        return view;
    float* chans[2] = { mMultiOutScratch.getWritePointer (chBase + 0),
                        mMultiOutScratch.getWritePointer (chBase + 1) };
    view.setDataToReferTo (chans, 2, numFrames);
    return view;
}

void BaySickRustyDrumsProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numFrames   = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Run the multi-out render path; we'll sum strips into the standard
    // stereo output for the standalone case (PluginProcessor uses the
    // processStrips + getStripBuffer path directly and never calls this).
    processStrips (numFrames, midi);
    buffer.clear();

    if (numChannels < 1 || mChannels.empty())
        return;

    const int stripCount = (int) mChannels.size();
    for (int s = 0; s < stripCount; ++s)
    {
        const int chBase = 2 * s;
        if (mMultiOutScratch.getNumChannels() <= chBase + 1) break;
        if (numChannels >= 2)
        {
            buffer.addFrom (0, 0, mMultiOutScratch, chBase + 0, 0, numFrames);
            buffer.addFrom (1, 0, mMultiOutScratch, chBase + 1, 0, numFrames);
        }
        else
        {
            buffer.addFrom (0, 0, mMultiOutScratch, chBase + 0, 0, numFrames, 0.5f);
            buffer.addFrom (0, 0, mMultiOutScratch, chBase + 1, 0, numFrames, 0.5f);
        }
    }
}

// J-3 channel discovery.  Walks `<kitRoot>/Programs/mappings/` for every
// `<piece>_map.sfz` file that does NOT have an articulation suffix
// (`<piece>_map_<articulation>.sfz` is excluded).  Each master file is
// matched against a sound-type rule table; multiple masters that map to
// the same sound type collapse into one channel (e.g. `kick_24_map.sfz`
// + `kick_24_nodamp_map.sfz` both collapse to "Kick").
//
// Output is sorted in drummer-conventional order:
//   Kick, Snare, Tom 22, Tom 18, Tom 15, Tom 14, Hi-hat, Ride 22,
//   Ride Sizzle 19, Crash 17, Crash Sizzle 17, China 18, Stack
//
// Detection is purely lexical on filenames - does NOT parse SFZ contents.
// Cheap, deterministic, and easy to verify against the on-disk listing.
namespace
{
struct SoundTypeRule
{
    const char* stemPrefix;   // matches stripped filename (no `_map.sfz`) prefix
    const char* displayName;  // mixer-strip + piano-roll label
    int         drummerOrder; // sort key - lower = lower in pitch / earlier in row order
};

// Order of evaluation: more-specific prefixes BEFORE more-general (e.g.
// "ride_sizzle_19" before "ride_22" so the sizzle ride doesn't get caught
// by a "ride" general rule).  These are the 13 sound types Big Rusty Drums
// produces; the rule table generalizes to any ARIA drum kit that follows
// the `<piece>_<size>_map.sfz` naming convention.
constexpr SoundTypeRule kSoundTypeRules[] = {
    { "kick",            "Kick",            0 },  // collapses kick_24 + kick_24_nodamp
    { "snare",           "Snare",           1 },  // collapses all snare articulations
    { "tom_22",          "Tom 22",          2 },
    { "tom_18",          "Tom 18",          3 },
    { "tom_15",          "Tom 15",          4 },
    { "tom_14",          "Tom 14",          5 },
    { "hihat",           "Hi-hat",          6 },  // collapses all hi-hat articulations
    { "ride_sizzle_19",  "Ride Sizzle 19",  8 },  // BEFORE ride_22 (more specific prefix)
    { "ride_22",         "Ride 22",         7 },
    { "crash_sizzle_17", "Crash Sizzle 17", 10 }, // BEFORE crash_17
    { "crash_17",        "Crash 17",        9 },
    { "china_18",        "China 18",        11 },
    { "stack",           "Stack",           12 }, // matches stack_3_layer
};

const SoundTypeRule* findRuleForStem (const juce::String& stem)
{
    for (const auto& rule : kSoundTypeRules)
        if (stem.startsWith (rule.stemPrefix))
            return &rule;
    return nullptr;
}
}

std::vector<KitChannel>
BaySickRustyDrumsProcessor::discoverChannels (const juce::File& kitRoot,
                                              const juce::File& programSfzPath)
{
    juce::File mappingsDir = kitRoot.getChildFile ("Programs").getChildFile ("mappings");
    if (! mappingsDir.isDirectory())
        mappingsDir = kitRoot;

    juce::Array<juce::File> mapFiles;
    mappingsDir.findChildFiles (mapFiles, juce::File::findFiles, /*recursive=*/false, "*_map.sfz");

    // Group masters by drummerOrder so duplicates (kick + kick_nodamp) collapse.
    // Map key = drummerOrder, value = (rule, list of file paths matched).
    struct Bucket { const SoundTypeRule* rule; std::vector<juce::String> files; };
    std::map<int, Bucket> buckets;

    for (const auto& f : mapFiles)
    {
        const auto stem = f.getFileNameWithoutExtension(); // e.g. "kick_24_map"

        // Exclude articulation variants: stem matches `<piece>_map_<articulation>`.
        if (stem.indexOf ("_map_") >= 0)
            continue;

        // Must end in `_map` (the master pattern).
        if (! stem.endsWith ("_map"))
            continue;

        if (auto* rule = findRuleForStem (stem))
        {
            auto& bucket = buckets[rule->drummerOrder];
            bucket.rule = rule;
            bucket.files.push_back (f.getFullPathName());
        }
        // Unmatched masters are silently dropped - they're outside the rule
        // table's coverage.  J-7 may revisit this if exotic kits arrive.
    }

    // J-8 (2026-05-04): program-include filter.  When a programSfzPath is given,
    // parse it for `#include "mappings/..."` lines and keep only the buckets
    // whose drummerOrder is referenced.  This is what makes Basic spawn 8 mixer
    // strips instead of all 13 - Basic only `#include`s a subset of the
    // mappings (e.g. `snare_14_map_basic.sfz`, `tom_14_map_basic.sfz`).
    // Variants (`<piece>_map_<articulation>.sfz`) classify against the same
    // stemPrefix rules as the master, so they correctly indicate the piece.
    std::set<int> presentDrummerOrders;
    bool filterActive = false;
    if (programSfzPath.existsAsFile())
    {
        filterActive = true;
        const auto progText = programSfzPath.loadFileAsString();
        juce::StringArray lines;
        lines.addLines (progText);
        for (const auto& raw : lines)
        {
            const auto t = raw.trim();
            if (! t.startsWithIgnoreCase ("#include")) continue;
            const int q1 = t.indexOfChar ('"');
            const int q2 = (q1 >= 0) ? t.indexOfChar (q1 + 1, '"') : -1;
            if (q1 < 0 || q2 <= q1) continue;
            juce::String inc = t.substring (q1 + 1, q2);

            // Strip a leading "mappings/" or "mappings\\" path prefix.
            if (inc.startsWithIgnoreCase ("mappings/"))   inc = inc.substring (9);
            if (inc.startsWithIgnoreCase ("mappings\\"))  inc = inc.substring (9);
            // Drop subfolder prefix (e.g. "hihat_14/hmuteg_cl.sfz" → use "hihat_14").
            const int slash = inc.indexOfChar ('/');
            const juce::String topToken = (slash > 0) ? inc.substring (0, slash) : inc;

            // Match the stem (filename without `.sfz`) OR the subfolder name
            // against the rule table.  Subfolder match catches things like
            // `hihat_14/hmuteg_cl.sfz` where the parent folder name carries
            // the piece identity.
            juce::String stem = inc;
            if (stem.endsWithIgnoreCase (".sfz"))
                stem = stem.substring (0, stem.length() - 4);
            const juce::String slashTopToken = topToken.endsWithIgnoreCase (".sfz")
                ? topToken.substring (0, topToken.length() - 4) : topToken;

            for (const auto& rule : kSoundTypeRules)
            {
                if (stem.startsWithIgnoreCase (rule.stemPrefix)
                 || slashTopToken.startsWithIgnoreCase (rule.stemPrefix))
                {
                    presentDrummerOrders.insert (rule.drummerOrder);
                }
            }
        }
    }

    std::vector<KitChannel> out;
    out.reserve (buckets.size());
    for (const auto& [order, bucket] : buckets) // std::map ordering = drummer order
    {
        if (filterActive && presentDrummerOrders.count (order) == 0)
            continue;   // Piece not referenced by the loaded program.

        KitChannel ch;
        ch.name        = bucket.rule->displayName;
        // Use the FIRST file in the bucket as the canonical mapFilePath
        // (e.g. kick_24_map.sfz wins over kick_24_nodamp_map.sfz).  All
        // files in the bucket are still loaded by sfizz when the parent
        // SFZ #include's them - the path here is informational, not a
        // load-driver.
        ch.mapFilePath = bucket.files.empty() ? juce::String() : bucket.files.front();
        ch.midiNote    = -1; // J-3 does not parse SFZ for trigger notes
        out.push_back (std::move (ch));
    }

    return out;
}

bool BaySickRustyDrumsProcessor::loadKit (const juce::File& sfzPath)
{
    if (! sfzPath.existsAsFile() || ! mSfizz)
        return false;

    // The "kit root" is two levels up from the SFZ file when the kit follows
    // the ARIA convention (root/Programs/<file>.sfz).  Falls back to the
    // SFZ's own parent when the kit is a flat folder.
    juce::File programsDir = sfzPath.getParentDirectory();
    juce::File kitRoot     = programsDir.getFileName().equalsIgnoreCase ("Programs")
                                ? programsDir.getParentDirectory()
                                : programsDir;

    // Discover channels FIRST so the wrapper builder knows the strip count
    // and drummer-order indices.  J-8 (2026-05-04): pass the program SFZ so
    // discoverChannels filters to only the pieces this program references -
    // Basic spawns ~8 strips, Full spawns 13.
    mChannels = discoverChannels (kitRoot, sfzPath);

    // J-7b: synthesize an output-routed wrapper SFZ + load it via
    // loadSfzString.  The wrapper inlines every piece's `_map*.sfz` content
    // with `output=N` injected into every `<master>` and `<group>` line,
    // so each region in each piece routes to its own sfizz output bus.
    // Always dump for inspection.
    juce::String wrapper = buildOutputRoutedSfzWrapper (sfzPath, kitRoot);
    if (wrapper.isNotEmpty())
        kitRoot.getChildFile ("_wrapper_dump.sfz").replaceWithText (wrapper);

    if (wrapper.isEmpty()
        || ! mSfizz->loadSfzString (sfzPath.getFullPathName().toStdString(),
                                    wrapper.toStdString()))
    {
        // Fall back to plain file load if synthesis or string-load failed
        // (kit will play summed to a single output, no per-strip routing).
        if (! mSfizz->loadSfzFile (sfzPath.getFullPathName().toStdString()))
            return false;
    }

    mPianoRollKeymap = discoverPianoRollKeymap (kitRoot);
    mCurrentKitPath  = sfzPath;

    // J-8 stage 2 (2026-05-04): seed CC defaults from the kit's `set_cc<N>=<int>`
    // directives so the ARIA panel + sfizz both start at the kit author's
    // intended positions.  Walks #include chains depth-first up to depth 4
    // (covers `program → default/<file> → mappings/<file>` nesting) and
    // collects every `set_cc<N>=<int>` it encounters.  Each is stamped into
    // mCcKitDefault (read-only snapshot for double-click reset) AND written
    // through APVTS (so the parameter listener pushes the value to sfizz and
    // the panel knobs paint at the right initial position).
    std::map<int, int> kitDefaults;
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
    // blanket 64 turned "amount" controls half-on by default and sounded wrong.
    // Reset is still required (vs leaving prior values) because switching
    // programs (e.g. Full -> Basic) would otherwise leak the previous program's
    // user-tweaked CC values into the new program.  The reset reaches sfizz via
    // setValueNotifyingHost -> parameterChanged.
    // QA-UndoCoverage Task 6: kit-default pushes are programmatic (the kit
    // load itself becomes ONE structural transaction in Task 7).
    juce::AudioProcessorValueTreeState::ScopedProgrammaticParamWrites spw;
    for (int cc = 0; cc < kCcCount; ++cc)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                apvts.getParameter ("brd_cc" + juce::String (cc))))
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (0.0f));
    }

    // Push kit defaults through APVTS - this drives the parameterChanged
    // listener which forwards each value to sfizz.  `gestureSetValue` would
    // be wrong here (would clobber project-restore state); use the ranged
    // setter which respects the host's parameter pipeline.
    for (auto& [cc, val] : kitDefaults)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                apvts.getParameter ("brd_cc" + juce::String (cc))))
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 ((float) val));
    }

    // Pre-size the multi-out scratch for the new strip count.
    if (! mChannels.empty())
    {
        const int neededCh = 2 * (int) mChannels.size();
        mMultiOutScratch.setSize (neededCh, mMaxBlockSize, false, true, false);
        mMultiOutPtrs.assign ((size_t) neededCh, nullptr);
    }
    return true;
}

// J-7b SFZ wrapper builder (text-injection approach).
//
// Why this approach: SFZ scope rules say a new `<master>` closes any open
// `<group>` and replaces previous `<master>` content.  The kit's piece
// `_map.sfz` files all start with internal `<master>` blocks that override
// any outer `<master>output=N` or `<group>output=N` we'd wrap them in.
// Multiple `<global>output=N` blocks have parsing issues in sfizz.
//
// Solution: inline each piece file's text into the wrapper and APPEND
// `output=N` to every `<master>` and `<group>` line in that text.  Every
// region in the file inherits from its containing `<master>`/`<group>`,
// so output=N propagates correctly regardless of how many inner scope
// blocks the piece file uses.  Top-level `<master>` blocks in 01-full.sfz
// (snare-stir CC crossfades, hihat mute groups) get the most recently
// seen piece's output via a sticky `currentPieceOutput` tracker.
juce::String
BaySickRustyDrumsProcessor::buildOutputRoutedSfzWrapper (const juce::File& sfzPath,
                                                        const juce::File& kitRoot)
{
    if (mChannels.empty()) return {};
    const juce::String orig = sfzPath.loadFileAsString();
    if (orig.isEmpty()) return {};

    auto classifyIncludePath = [] (const juce::String& path) -> int
    {
        juce::String stem = path;
        if (stem.startsWithIgnoreCase ("mappings/"))   stem = stem.substring (9);
        if (stem.startsWithIgnoreCase ("mappings\\"))  stem = stem.substring (9);
        if (stem.endsWithIgnoreCase (".sfz"))          stem = stem.substring (0, stem.length() - 4);
        const int slash = stem.indexOfChar ('/');
        if (slash > 0) stem = stem.substring (0, slash);

        int bestIdx = -1;
        for (const auto& rule : kSoundTypeRules)
            if (stem.startsWith (rule.stemPrefix))
                if (bestIdx < 0 || juce::String (rule.stemPrefix).length()
                                    > juce::String (kSoundTypeRules[bestIdx].stemPrefix).length())
                    bestIdx = (int) (&rule - kSoundTypeRules);
        if (bestIdx < 0) return -1;
        return kSoundTypeRules[bestIdx].drummerOrder;
    };

    auto stripIdxForDrummerOrder = [this] (int drummerOrder) -> int
    {
        const SoundTypeRule* rule = nullptr;
        for (const auto& r : kSoundTypeRules)
            if (r.drummerOrder == drummerOrder) { rule = &r; break; }
        if (! rule) return -1;
        for (size_t i = 0; i < mChannels.size(); ++i)
            if (mChannels[i].name == rule->displayName) return (int) i;
        return -1;
    };

    auto annotateMastersWithOutput = [] (const juce::String& fileText, int outputN) -> juce::String
    {
        // Walk every line; for any `<master>` or `<group>` tag, append
        // `output=N` so the regions inheriting from it route correctly.
        juce::StringArray fl;
        fl.addLines (fileText);
        for (auto& l : fl)
        {
            const auto t = l.trim();
            if (t.startsWithIgnoreCase ("<master>") || t.startsWithIgnoreCase ("<group>"))
                l = l.trimEnd() + " output=" + juce::String (outputN);
        }
        return fl.joinIntoString ("\n");
    };

    juce::StringArray lines;
    lines.addLines (orig);

    juce::String out;
    int currentPieceOutput = -1;   // -1 = pre-piece-context (control/global)

    for (auto& raw : lines)
    {
        const auto trimmed = raw.trim();

        // Top-level <master>/<group> tags (e.g. kit's snare-stir CC crossfade
        // blocks, hi-hat mute group block).  Inject the current piece's
        // output= so regions in those blocks route correctly.
        if (trimmed.startsWithIgnoreCase ("<master>")
         || trimmed.startsWithIgnoreCase ("<group>"))
        {
            if (currentPieceOutput >= 0)
                out += raw.trimEnd() + " output=" + juce::String (currentPieceOutput) + "\n";
            else
                out += raw + "\n";
            continue;
        }

        if (trimmed.startsWithIgnoreCase ("#include"))
        {
            const int q1 = trimmed.indexOfChar ('"');
            const int q2 = (q1 >= 0) ? trimmed.indexOfChar (q1 + 1, '"') : -1;
            juce::String path = (q1 >= 0 && q2 > q1)
                                    ? trimmed.substring (q1 + 1, q2) : juce::String();

            const int drumOrder = classifyIncludePath (path);
            const int strip     = (drumOrder >= 0) ? stripIdxForDrummerOrder (drumOrder) : -1;

            if (strip >= 0)
            {
                currentPieceOutput = strip;

                // Resolve the included file relative to `<kitRoot>/Programs/`,
                // matching sfizz's #include resolution semantics.  Read its
                // text, annotate every `<master>`/`<group>` line with
                // output=N, and inline the result.
                juce::File pieceFile = kitRoot.getChildFile ("Programs").getChildFile (path);
                juce::String pieceText = pieceFile.loadFileAsString();
                if (pieceText.isNotEmpty())
                {
                    out += annotateMastersWithOutput (pieceText, strip);
                    if (! out.endsWithChar ('\n')) out += "\n";
                }
                else
                {
                    // Fallback: keep the original include directive (output= won't propagate).
                    out += raw + "\n";
                }
                continue;
            }

            // Non-piece include (keymap, hihat_cc_ranges, curves) - keep as-is.
            out += raw + "\n";
            continue;
        }

        // Everything else (control content, global defaults, comments,
        // `<effect>` blocks, opcodes, blank lines) passes through unchanged.
        out += raw + "\n";
    }

    return out;
}

// J-7b (2026-05-03, refactored): per-define $name → (sound, articulation)
// label table.  Curated from Big Rusty Drums' keymap.sfz; covers all 51
// kit-native defines.  When a $name isn't in the table, the parser falls
// back to a title-cased version of the stripped name.
namespace
{
struct DefineLabel { const char* defineName; const char* sound; const char* articulation; };
constexpr DefineLabel kDefineLabels[] = {
    // Kick
    { "kickkey",      "Kick",            "" },
    { "kickndkey",    "Kick",            "No Damp" },
    { "kphkey",       "Kick",            "Pedal Hit" },
    { "kprkey",       "Kick",            "Pedal Release" },
    { "ksckey",       "Kick",            "Stick Click" },
    { "krckey",       "Kick",            "Release Click" },
    { "ktckey",       "Kick",            "Top Click" },
    // Snare
    { "sncenterkey",  "Snare",           "Center" },
    { "snedgekey",    "Snare",           "Edge" },
    { "snrimskey",    "Snare",           "Rimshot" },
    { "snsskey",      "Snare",           "Sidestick" },
    { "snrelkey",     "Snare",           "Release" },
    { "sndigkey",     "Snare",           "Brush Dig" },
    { "snstlkey",     "Snare",           "Stir Long" },
    { "snstskey",     "Snare",           "Stir Short" },
    { "snflkey",      "Snare",           "Flam" },
    { "stmkey",       "Snare",           "Stir Mute" },
    { "snrckey",      "Snare",           "Release Click" },
    { "snsckey",      "Snare",           "Stick Click" },
    // Hi-hat
    { "htclstkey",    "Hi-hat",          "Tight Closed Tip" },
    { "htclsskey",    "Hi-hat",          "Tight Closed Shaft" },
    { "htvartkey",    "Hi-hat",          "Half Open Tip" },
    { "htvarskey",    "Hi-hat",          "Half Open Shaft" },
    { "htchkkey",     "Hi-hat",          "Foot Chick" },
    { "htsplkey",     "Hi-hat",          "Splash" },
    { "htretkey",     "Hi-hat",          "Retake" },
    { "htphkey",      "Hi-hat",          "Pedal Hit" },
    { "htprkey",      "Hi-hat",          "Pedal Release" },
    { "htperckey",    "Hi-hat",          "Perc Click" },
    // Toms
    { "t22key",       "Tom 22",          "" },
    { "t18key",       "Tom 18",          "" },
    { "t15key",       "Tom 15",          "" },
    { "t14key",       "Tom 14",          "" },
    { "t18relkey",    "Tom 18",          "Release" },
    { "t18digkey",    "Tom 18",          "Brush Dig" },
    { "t18stlkey",    "Tom 18",          "Stir Long" },
    { "t18stskey",    "Tom 18",          "Stir Short" },
    { "t18flkey",     "Tom 18",          "Flam" },
    { "t22rckey",     "Tom 22",          "Release Click" },
    { "t22sckey",     "Tom 22",          "Stick Click" },
    { "t18rckey",     "Tom 18",          "Release Click" },
    { "t18sckey",     "Tom 18",          "Stick Click" },
    { "t15rckey",     "Tom 15",          "Release Click" },
    { "t15sckey",     "Tom 15",          "Stick Click" },
    { "t14rckey",     "Tom 14",          "Release Click" },
    { "t14sckey",     "Tom 14",          "Stick Click" },
    // Ride 22
    { "rdrdkey",      "Ride 22",         "Ride" },
    { "rdcrkey",      "Ride 22",         "Crash" },
    { "rdblkey",      "Ride 22",         "Bell" },
    { "rdchkey",      "Ride 22",         "Choke" },
    // Ride Sizzle 19
    { "rdsrdkey",     "Ride Sizzle 19",  "Ride" },
    { "rdscrkey",     "Ride Sizzle 19",  "Crash" },
    { "rdsblkey",     "Ride Sizzle 19",  "Bell" },
    { "rdschkey",     "Ride Sizzle 19",  "Choke" },
    // Crash 17
    { "crcrkey",      "Crash 17",        "Crash" },
    { "crchkey",      "Crash 17",        "Choke" },
    // Crash Sizzle 17
    { "crsrdkey",     "Crash Sizzle 17", "Ride" },
    { "crscrkey",     "Crash Sizzle 17", "Crash" },
    { "crsblkey",     "Crash Sizzle 17", "Bell" },
    { "crschkey",     "Crash Sizzle 17", "Choke" },
    // China 18
    { "cncnkey",      "China 18",        "Center" },
    { "cnchkey",      "China 18",        "Choke" },
    // Stack
    { "stmidkey",     "Stack",           "Mid" },
    { "stedkey",      "Stack",           "Edge" },
};

void labelFor (const juce::String& defineName,
               juce::String& outSound,
               juce::String& outArticulation)
{
    for (const auto& d : kDefineLabels)
    {
        if (defineName == d.defineName)
        {
            outSound        = d.sound;
            outArticulation = d.articulation;
            return;
        }
    }
    // Fallback: title-case the raw name minus trailing "key".
    juce::String s = defineName;
    if (s.endsWith ("key")) s = s.substring (0, s.length() - 3);
    if (s.isEmpty()) { outSound = "Unknown"; outArticulation = ""; return; }
    juce::String pretty;
    pretty += juce::String::charToString (juce::CharacterFunctions::toUpperCase (s[0]));
    if (s.length() > 1) pretty += s.substring (1);
    outSound        = pretty;
    outArticulation = "";
}
}

std::vector<PianoRollKey>
BaySickRustyDrumsProcessor::discoverPianoRollKeymap (const juce::File& kitRoot)
{
    // Parse `Programs/keymap/keymap.sfz` for `#define $name <midi>` entries.
    // Each entry becomes one PianoRollKey, sorted ascending by MIDI note.
    const juce::File keymapFile = kitRoot.getChildFile ("Programs")
                                         .getChildFile ("keymap")
                                         .getChildFile ("keymap.sfz");
    std::vector<PianoRollKey> out;
    if (! keymapFile.existsAsFile())
        return out;

    const juce::String keymapText = keymapFile.loadFileAsString();
    juce::StringArray lines;
    lines.addLines (keymapText);
    for (auto& line : lines)
    {
        const auto trimmed = line.trim();
        if (! trimmed.startsWith ("#define $")) continue;
        auto rest = trimmed.fromFirstOccurrenceOf ("#define $", false, false);
        const int spIdx = rest.indexOfChar (' ');
        const int tIdx  = rest.indexOfChar ('\t');
        int sepIdx = -1;
        if      (spIdx >= 0 && tIdx >= 0) sepIdx = juce::jmin (spIdx, tIdx);
        else if (spIdx >= 0)              sepIdx = spIdx;
        else                              sepIdx = tIdx;
        if (sepIdx <= 0) continue;
        const juce::String name = rest.substring (0, sepIdx).trim();
        juce::String rhs        = rest.substring (sepIdx).trim();
        const int commentIdx    = rhs.indexOf ("//");
        if (commentIdx >= 0) rhs = rhs.substring (0, commentIdx).trim();
        const int midi = rhs.getIntValue();
        if (midi < 0 || midi > 127) continue;

        PianoRollKey k;
        k.midiNote   = midi;
        k.defineName = name;
        labelFor (name, k.soundName, k.articulation);
        out.push_back (std::move (k));
    }

    std::sort (out.begin(), out.end(),
               [] (const PianoRollKey& a, const PianoRollKey& b) { return a.midiNote < b.midiNote; });
    return out;
}

void BaySickRustyDrumsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("BaySickRustyDrumsState");
    root.appendChild (apvts.copyState(), nullptr);

    if (mCurrentKitPath != juce::File())
    {
        juce::ValueTree kitNode ("KitPath");
        // QA-ProjectSave Task 5 (2026-07-26): persist as a stable-root ref when
        // the kit lives under Core Library (every shipped kit does), so the
        // saved path stops embedding the Windows user name and resolves under
        // any account.  Falls back to absolute for anything outside.
        kitNode.setProperty ("path", SampleLibrary::refForPersist (mCurrentKitPath), nullptr);
        root.appendChild (kitNode, nullptr);
    }

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, dest);
}

void BaySickRustyDrumsProcessor::setStateInformation (const void* data, int sz)
{
    auto xml = getXmlFromBinary (data, sz);
    if (! xml || ! xml->hasTagName ("BaySickRustyDrumsState"))
        return;

    auto root = juce::ValueTree::fromXml (*xml);

    // J-8 stage 2 (2026-05-04): order matters when CC params live in APVTS.
    // loadKit walks the program SFZ and stamps `set_cc<N>=<int>` defaults into
    // every brd_cc<N> param, overwriting whatever was there.  We need the
    // user's saved CC values to win, so:
    //   1) load the kit first (kit defaults overwrite anything in apvts)
    //   2) replaceState second (the project's saved CCs overlay the kit defaults)
    if (auto kitNode = root.getChildWithName ("KitPath"); kitNode.isValid())
    {
        const juce::File kit = SampleLibrary::resolvePersistedRef (
                                   kitNode.getProperty ("path").toString());
        if (kit.existsAsFile())
            loadKit (kit);
        else
            // QA-Export Task 5: see the Guitars equivalent -- silent skip meant
            // the tab restored looking fine and produced nothing.
            MissingFileReport::add ("Drum kit", kit.getFullPathName());
    }

    if (auto apvtsState = root.getChildWithName (apvts.state.getType()); apvtsState.isValid())
        apvts.replaceStateKeepingUndoHistory (apvtsState);
}
