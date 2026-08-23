#pragma once
#include <JuceHeader.h>
#include "SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include "BaySickConstants.h"
// 2026-04-25: DrumSynth.h + DrumVoice + BaySickDrumsProcessor + DrumsPage
// + BaySickDrumsEditor + BaySickDrumsLAF all deleted.  Drums now use the
// dynamic per-tab DrumPage model with BaySickPlayer / BaySickSynth engines.
#include "PatternManager.h"
#include "DSP/EQ8MsDSP.h"
#include "BaySickGraph.h"
#include "MidiRecorder.h"
#include "AudioFileRecorder.h"
#include "DSP/AudioClipStreamer.h"
#include "DSP/PhaseVocoder.h"
#include "DSP/BaySickAlignDSP.h"   // QA-Fa recovery: AlignPlaySnapshot (decode-layer live warp)
#include "DSP/DenoiseDSP.h"        // QA-Fe2: DenoiseProfile (project-persisted per recording)
#include "MidiLearn/MidiLearnRegistry.h"
#include "MidiLearn/DrumTriggerMap.h"
// Multi-threaded render engine (Phase 1 scaffolding, 2026-05-06).  These
// members live for the lifetime of the processor; per-strip RenderTask
// wrappers ship in Batches 3-8.
#include "Engine/RenderEngineFlags.h"
#include "Engine/ChannelBufferArena.h"
#include "Engine/BaySickThreadPool.h"
#include "Engine/RenderGraphDispatcher.h"
#include "Engine/Tasks/EngineInsertTask.h"   // Batch 3: Layer/Bass/Drum task wrappers
#include "Engine/Tasks/DirectFileTask.h"     // QA-TrueLevel SC-10: Direct to Master strips
#include "Engine/Tasks/VoxStripTask.h"       // Batch 4: Vox live-input strip wrapper
#include "Engine/Tasks/InstStripTask.h"      // Batch 4: Inst source-mode-aware wrapper
#include "Engine/Tasks/CompositeAudioInsertTask.h"  // QA-0 (2026-05-07): per-row composite (replaces ClipPageTask + AudioInsertTask)
#include "Engine/Tasks/RustyDrumsProducerTask.h"   // Batch 6: drives processStrips
#include "Engine/Tasks/RustyInsertTask.h"          // Batch 6: per-strip insert wrapper
#include "Engine/Tasks/PassiveStripTask.h"         // Batch 7: aux + bus accumulator strips
#include "Engine/Tasks/MasterTask.h"               // Batch 8: terminal task (graph sink)
#include "Engine/RetirementQueue.h"                // Batch 9c B1: deferred-destruction GC

// ── APVTS parameter helper ────────────────────────────────────────────────────
// Creates a ParameterID with version 1
#define VID(id) juce::ParameterID{(id), 1}

class EngineRig;   // QA-ModelShell TS1: model-side engine owner (member at class end)
// TS7 §6: opaque enum declaration rather than including EngineRig.h.  A scoped
// enum has a fixed underlying type, so it can be forward-declared -- which keeps
// EngineRig.h (and the AudioClipStreamer it now pulls in) out of this header,
// preserving the deliberate forward-declaration above.
enum class TabKind;
namespace Hosting { class PluginManager; }   // QA-ModelShell TS6

// ── CL-281 decode-once clip cache ────────────────────────────────────────────
// The decoded PCM of ONE resolved audio file.  Decoded on the message thread at
// clip-rebuild time and SHARED, unchanged, by every clip that resolves to that
// file -- the same WAV placed four times decodes once and the four clips read
// one buffer.
//
// THREAD SAFETY / OWNERSHIP: nothing mutates a DecodedClipAudio after its
// shared_ptr leaves the decoder, so the audio thread reads it with no
// synchronization beyond the release-store that publishes the AudioClipSnapshot
// holding the pointer.  A clip cannot mutate data another clip is reading: the
// alias is shared_ptr<CONST>, and every per-clip read is a copy out.  The last
// reference to drop frees the buffer -- for a retired snapshot that drop happens
// on the RetirementQueue drainer thread, never on the audio thread.
struct DecodedClipAudio
{
    juce::AudioBuffer<float> samples;            // numChannels x lengthInSamples, at fileSampleRate
    double      fileSampleRate  { 44100.0 };
    int         numChannels     { 2 };
    juce::int64 lengthInSamples { 0 };
};
using DecodedClipAudioPtr = std::shared_ptr<const DecodedClipAudio>;

// ── ClipSource ───────────────────────────────────────────────────────────────
// One arrangement clip's read head.  Two backings behind one API:
//   CACHED   - a shared DecodedClipAudio.  The whole file is resident, so there
//              is no ring, no prefetch and no seek state to keep.
//   STREAMED - an owned AudioClipStreamer, for files past
//              AudioClipStreamer::kRamThresholdBytes.
// The cached branch reproduces the streamer's own RAM-mode reads (which run
// against a ring whose capacity IS the file length, so its modulo is identity):
// same 1:1 snap, same Catmull-Rom kernel, same EOF behavior.
//
// AUDIO THREAD: readAndMix / readRaw allocate nothing and take no lock on
// either backing.
class ClipSource
{
public:
    explicit ClipSource (DecodedClipAudioPtr cached) noexcept
        : mCached (std::move (cached)), mData (mCached.get()) {}
    explicit ClipSource (std::unique_ptr<AudioClipStreamer> streamed) noexcept
        : mStreamer (std::move (streamed)) {}

    float readAndMix (juce::AudioBuffer<float>& dest,
                      int    destOffset,
                      int    numOutputSamples,
                      double fileStartPos,
                      double readRatio,
                      int    numDestChannels,
                      float  gain);

    bool readRaw (juce::AudioBuffer<float>& dest,
                  int destOffset,
                  int numSamples,
                  juce::int64 filePos);

    // Audio-thread-safe seek request.  No-op on the cached backing.
    void requestSeek (juce::int64 filePos);

    juce::int64 getTotalLength() const noexcept
        { return mData != nullptr ? mData->lengthInSamples : mStreamer->getTotalLength(); }
    int getNumChannels() const noexcept
        { return mData != nullptr ? mData->numChannels : mStreamer->getNumChannels(); }
    // Consumed by the reverse-playback gate: backward reads need the whole file
    // resident, which a cached clip is by definition.
    bool isRamLoaded() const noexcept
        { return mData != nullptr ? true : mStreamer->isRamLoaded(); }

private:
    DecodedClipAudioPtr                mCached;             // null when streaming
    std::unique_ptr<AudioClipStreamer> mStreamer;           // null when cached
    // mCached.get(), hoisted so the audio-thread reads never touch the control
    // block; also the branch selector (null == streaming).
    const DecodedClipAudio*            mData { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipSource)
};

class BaySickDAWProcessor : public juce::AudioProcessor,
                           private juce::ValueTree::Listener   // §P4.3 perf: dirty-flag EQ sync
{
    // Multi-threaded render engine task wrappers (Batches 4+) read internal
    // per-tab state (FilePlay flags, sfizz-active atomics, idle counters,
    // kIdleSuspendBlocks) directly. Friend access avoids cluttering the
    // public API with accessors used only by the engine layer.
    friend class VoxStripTask;
    friend class InstStripTask;
    friend class CompositeAudioInsertTask; // QA-0 (2026-05-07): replaces old ClipPageTask + AudioInsertTask friends
    friend class RustyDrumsProducerTask;  // Batch 6
    friend class RustyInsertTask;         // Batch 6
    friend class PassiveStripTask;        // Batch 7
    friend class MasterTask;              // Batch 8
public:
    BaySickDAWProcessor();
    ~BaySickDAWProcessor() override;

    // ── AudioProcessor interface ──────────────────────────────────────────
    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock   (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BaySickDAW"; }
    bool   acceptsMidi()  const override { return true; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int  getNumPrograms()   override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ── Project persistence (P1, 2026-04-23) ─────────────────────────────────
    // Writes the full project state (APVTS + rack states + PatternManager tree)
    // as children of `root`.  Used by ProjectManager to produce project.xml.
    //   root must start empty; this fills it with <Processor>, <PatternManager>
    //   nodes + a "version" attribute.
    // Non-const because apvts.copyState() + BaySickGraph::saveRackStates both
    // require non-const access; conceptually a read-only snapshot.
    void serializeProject  (juce::XmlElement& root);
    // QA-ProjectSave Task 2 (2026-07-26, docket 15=B): the <Processor> child
    // alone (APVTS + BaySickRackStates), shared with template save.  Carries every
    // mixer strip's fader/pan/width/routing + each insert's rack and post-rack
    // EQ -- none of which is in <UIState>'s per-tab engineData.
    void writeProcessorState (juce::XmlElement& root);
    // The <Processor> apply half, shared with template load.  Caller owns the
    // project-load shield and must have run clearAllAuxInserts() first; per-insert
    // rack states are stashed for applyPendingRackStates once tabs + strips exist.
    void applyProcessorState (const juce::XmlElement& root);
    // One-shot warning for external files engines could not find during a
    // restore.  Call after every engine has finished loading.  A thin forwarder
    // to MissingFileReport::reportIfAny, kept because the restore paths already
    // hold the processor -- the store itself needs nothing from it.
    //
    // The store is process-wide, so EVERY gesture that can reach
    // MissingFileReport::add must drain -- an undrained entry does not just go
    // unreported, it surfaces later under whatever operation happens to drain
    // next, naming a file that has nothing to do with what the user is looking
    // at.  Prefer a MissingFileReport::ScopedGesture at the TOP of the gesture:
    // it cannot be forgotten on an early return, and it nests.  A bare call to
    // this belongs only where the caller owns the whole load and opens no scope
    // (project / template restore); made from INSIDE someone else's open scope
    // it steals that scope's entries and headlines them with this noun.
    // sourceNoun is what the headline calls the thing that referred to the file
    // ("project", "preset", ...).
    void reportMissingFilesIfAny (const juce::String& sourceNoun = "project");
    // Public wrapper over the protected AudioProcessor decode helper so the
    // restore walker can VALIDATE an internal engine's copyXmlToBinary blob
    // (corrupt-data detection) before handing it to setStateInformation --
    // in-attribute corruption usually still base64-decodes, so the XML magic
    // check is what actually catches it.
    static std::unique_ptr<juce::XmlElement> decodeEngineBlob (const juce::MemoryBlock& mb)
    {
        return SafeXml::parseBinaryBlob (mb.getData(), (int) mb.getSize());
    }

    // Restores everything serializeProject wrote.  Caller is responsible for
    // syncing UI afterwards.  No-op if root is missing the expected children.
    void deserializeProject (const juce::XmlElement& root);

    // File > New reset (2026-04-24): restore every APVTS param to its
    // default value, clear BaySickGraph rack states, and reset PatternManager.
    // Caller is responsible for tearing down + rebuilding dynamic tabs - this
    // method deals only with processor-owned state.
    void resetToBlankState();

    // 2026-04-24: deferred rack state replay.  deserializeProject stashes
    // BaySickRackStates into mPendingProjectRackState and skips the immediate
    // apply, because per-insert racks (Layer / Bass / Drum / Audio / Aux /
    // Vox / Inst InsertNodes) don't exist yet - the editor creates them when
    // rebuilding tabs + audio strips.  The editor calls this at the END of
    // its load flow (after restoreAudioStripsFromArrangement), when every
    // InsertNode exists, so effect + EQ state actually reaches its target.
    void applyPendingRackStates();

    // P1+P2 persistence (2026-04-24): StandaloneEditor owns tab + engine state
    // (Harmless / BaySickSynth / BaySickPlayer / BaySickBass / BaySickDrums are
    // all per-page engine processors with their OWN apvts, not the main
    // BaySickDAWProcessor::apvts).  These callbacks let serializeProject /
    // deserializeProject delegate that chunk of persistence up to the editor.
    //   onSerializeUIState  fired inside serializeProject - editor adds its
    //                       <UIState> child to the passed root element.
    //   onDeserializeUIState fired inside deserializeProject after the main
    //                       state has been restored - editor reads <UIState>
    //                       and rebuilds tabs + engines.
    std::function<void(juce::XmlElement&)>       onSerializeUIState;
    // QA-TrueLevel SC-12: fired once the load shield is down, so the editor
    // can run the interactive missing-file sweep on a finished project.
    std::function<void()>                        onProjectLoaded;
    std::function<void(const juce::XmlElement&)> onDeserializeUIState;

    // Load-progress hook: deserializeProject reports phase labels so the
    // editor's HeavyOperationOverlay can repaint mid-load.  Message thread
    // only (fired from inside the synchronous load path).
    std::function<void(const juce::String&)>     onLoadProgress;

    // P4: current project folder on disk, used to resolve relative
    // audioFilePath values like "Samples/<name>.wav" for audio-clip playback.
    // Set from ProjectManager after newProject / openProject / saveProjectAs.
    void setCurrentProjectFolder (const juce::File& folder);
    juce::File getCurrentProjectFolder() const;
    // Resolves a block's `audioFilePath`.  Absolute paths pass through (legacy
    // pre-P4 projects stored full paths).  Relative paths like "Samples/x.wav"
    // are resolved against the current project folder.
    juce::File resolveProjectFile (const juce::String& storedPath) const;

    // PatternManager is owned by StandaloneEditor today - the processor holds
    // a pointer so audio-thread scheduling can read it (see mPatternManager).
    // serializeProject/deserializeProject need it too; this setter is already
    // called by StandaloneEditor on startup via setPatternManager (P1 reuses).
    PatternManager* getPatternManager() const { return mPatternManager; }

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // ── Public accessors ──────────────────────────────────────────────────
    // The app's ONE global undo history.  Declared BEFORE apvts because apvts
    // binds it at construction (member init order = declaration order);
    // EngineRig's factory threads the same manager into every engine APVTS and
    // stamps each with an undoOwnerTag, so a parameter transaction can still be
    // resolved after the engine that made it was destroyed and re-created.
    // StandaloneEditor::mUndoManager is a REFERENCE to this one -- there is no
    // second manager anywhere in the app.
    juce::UndoManager mUndoManager;

    // QA-UndoCoverage Task 8 (the absorbed QA-DirtyFlag spec, Jeff 2026-05-23):
    // transaction-pointer dirty tracking.  current walks with the undo cursor;
    // saved pins the last save point; dirty = (current != saved).  Branch-kill:
    // a new transaction while current < saved destroys the saved future ->
    // saved = -1 (unreachable) until the next save.  Fed by the editor's
    // UndoManager ChangeListener (every transaction source -- choke point AND
    // attachment gestures -- broadcasts); message thread only.
    struct TransactionTracker
    {
        int current { 0 };
        int saved   { 0 };
        std::function<void (bool nowDirty)> onDirtyChanged;   // edge-fired

        bool isDirty() const noexcept { return current != saved; }

        void onNewTransactions (int count)
        {
            const bool was = isDirty();
            if (current < saved) saved = -1;
            current += count;
            edge (was);
        }
        void onUndo (int count) { const bool was = isDirty(); current -= count; edge (was); }
        void onRedo (int count) { const bool was = isDirty(); current += count; edge (was); }
        void onSave()           { const bool was = isDirty(); saved = current;  edge (was); }
        void onLoadReset()      { const bool was = isDirty(); current = 0; saved = 0; edge (was); }

    private:
        void edge (bool was)
        {
            if (onDirtyChanged && was != isDirty()) onDirtyChanged (isDirty());
        }
    };
    TransactionTracker mTxTracker;

    juce::AudioProcessorValueTreeState apvts;

    // QA-ModelShell TS1: the model-side owner of every dynamic tab's engine
    // (Layers/Bass/Drums/Clips/Vox/Inst).  Pages are views and never
    // construct engines.
    EngineRig& engineRig() noexcept { return *mEngineRig; }

    // QA-ModelShell TS6: hosted-plugin scan folders, the added list every
    // picker reads, and the VST3 format manager instances are created through.
    Hosting::PluginManager& pluginManager() noexcept { return *mPluginManager; }

    // ── QA-ModelShell TS2: offline render drive ──────────────────────────
    // The model renders ITSELF offline -- no replica processor.  begin:
    // suspend device processing (the standalone player checks isSuspended,
    // so our render loop becomes processBlock's only caller), capture the
    // restore set, sweep setNonRealtime(true) across self + every engine,
    // reset the graph (wet-tail hygiene), full re-prepare at the render
    // config (rate independent of the device).  end reverses all of it and
    // clears the render's own tails so they never bleed into live playback.
    // Message/render thread only.
    bool beginOfflineRender (double renderSampleRate, int renderBlockSize);
    void endOfflineRender();
    // True between begin/endOfflineRender, any thread.  The auto-freeze poll
    // reads it to stay quiet while an export runs.
    bool isOfflineRenderActive() const noexcept
        { return mOfflineRenderActive.load (std::memory_order_acquire); }

    // QA-ModelShell TS2 (stems): a strip channel's post-chain output for the
    // block that just rendered -- the same arena slot its render task wrote.
    // OFFLINE USE ONLY: valid on the render thread between a processBlock
    // return and the next call, while device processing is suspended.
    juce::AudioBuffer<float>* getStripOutputForTap (int channelId) noexcept
    {
        return mRenderArena.getStripBuffer (channelId);
    }

    // QA-ModelShell TS2: `<project>\Exports\` -- created on demand; every
    // export surface defaults its save dialog here (locked destination
    // spec).  Falls back to the user's Music folder when no project folder
    // exists yet (the dialog-UX chunk adds the save-first interlock).
    juce::File getProjectExportsDir()
    {
        const juce::File proj = getCurrentProjectFolder();
        if (proj == juce::File() || ! proj.isDirectory())
            return juce::File::getSpecialLocation (juce::File::userMusicDirectory);
        const juce::File exports = proj.getChildFile ("Exports");
        exports.createDirectory();
        return exports;
    }

    // QA-ModelShell TS7 (CL-227): `<project>\Reports\` -- the loudness
    // conformance reports.  Deliberately a SIBLING of Exports rather than a
    // subfolder of it: the Builder's Files browser lists the two as separate
    // sections, and reports are not audio the user might drag into the grid.
    // Same Music-folder fallback as Exports for an unsaved session.
    juce::File getProjectReportsDir()
    {
        const juce::File proj = getCurrentProjectFolder();
        if (proj == juce::File() || ! proj.isDirectory())
            return juce::File::getSpecialLocation (juce::File::userMusicDirectory);
        const juce::File reports = proj.getChildFile ("Reports");
        reports.createDirectory();
        return reports;
    }

    // QA-ModelShell TS7 (§6.7): `<project>\Freeze\` -- the frozen-track audio
    // cache.  A tab owns a FAMILY of files, one per scope: `_song` plus one
    // `_patN` for every pattern it plays in, and Rusty spreads over its 13
    // strip indices.  Each scope's file is overwritten in place on re-render,
    // so a delete or orphan sweep must match the name PREFIX, never a single
    // filename.  Excluded from project bundles and from the Files browser:
    // regenerable cache, not something the user made.
    juce::File getProjectFreezeDir()
    {
        const juce::File proj = getCurrentProjectFolder();
        if (proj == juce::File() || ! proj.isDirectory()) return juce::File();
        const juce::File fz = proj.getChildFile ("Freeze");
        fz.createDirectory();
        return fz;
    }

    // Request the audio thread to flush every engine's active voices at the
    // start of the next processBlock (sends CC 123 All-Notes-Off + explicit
    // per-note offs for anything in mPRPendingOffs). Thread-safe trigger:
    // set the flag from the UI, clear on the audio thread after handling.
    std::atomic<bool> mFlushAllNotes { false };

    // End-of-song beat (set by UI when song mode is active + no time sel).
    // Audio thread checks crossing and requests transport stop via
    // mRequestStop. 0 = no auto-stop.
    std::atomic<double> mSongEndBeats { 0.0 };
    std::atomic<bool>   mRequestStop  { false };
    // True = loop back to 0 at mSongEndBeats; false = auto-stop at song end.
    // 2026-04-26: default flipped to true so empty Song mode loops the 1-bar
    // audition instead of immediately auto-stopping.  Matches the matching
    // default in `GlobalTransportBar::mSongLoopBtn`.
    std::atomic<bool>   mSongLoopMode { true };

    // QA-Ed: loop-start beat (set by onGetLoopBeats alongside
    // mCachedPatternLoopBeats = loop-end) so the scheduler can build the
    // sample-accurate loop-seam windows.  0 except for time-selection loops.
    std::atomic<double> mLoopStartBeats { 0.0 };
    // QA-Ed: the playhead's backward-seek discontinuity flag, wired at startup
    // via setSeekDiscontinuityFlag().  Consumed once per block by the scheduler
    // to flush pending note-offs on a backward seek (cut held notes on a scrub).
    std::atomic<bool>*  mSeekDiscontinuity { nullptr };
    void setSeekDiscontinuityFlag (std::atomic<bool>* f) noexcept { mSeekDiscontinuity = f; }
    // QA-Ee Task 1c: the playhead's one-shot loop-wrap flag, wired at startup via
    // setLoopWrappedFlag().  Consumed once per block by the scheduler to flush a
    // note-off stranded on the loop point when the wrap landed on a block boundary.
    std::atomic<bool>*  mLoopWrapped { nullptr };
    void setLoopWrappedFlag (std::atomic<bool>* f) noexcept { mLoopWrapped = f; }

    // QA-RustyMeter Task 3 (2026-05-30): transport-edge tracking for the master
    // LUFS Integrated reset.  Audio-thread-only (processBlock), so plain members.
    // Reset the gated Integrated accumulation on stopped->playing and on a
    // backward ppq jump (play-from-top / loop-start).  M/S keep tracking.
    bool   mLufsWasPlaying { false };
    double mLufsLastPpq    { 0.0 };

    // 2026-07-30: last playhead propagated to the child engines.  Written on the
    // audio thread; a plain pointer compare so the N stores happen only when it
    // changes (startup, and the offline render's swap in and back out).
    //
    // CHANGE-GATED PROPAGATION ALONE IS NOT ENOUGH (found 2026-07-31).  The
    // pointer changes once, at the first audio block -- which runs BEFORE any
    // engine exists (the audio callback is installed before the editor builds
    // its tabs).  Every engine created afterwards -- project load, add tab, swap
    // engine -- was therefore never handed a playhead at all, so its tempo-synced
    // DSP had nothing to follow.  Invisible for our own engines, which mostly
    // read bpm by other means; fatal for a hosted VST3, whose ENTIRE transport
    // comes from this pointer.  enginePlayHead() below is the other half: every
    // creation path sets it on the new engine.
    juce::AudioPlayHead* mLastEnginePlayHead { nullptr };

    // TS7 §3.2: version-capture transport edges, published from the SAME test
    // above.  Counters rather than flags so a UI tick that lands between two
    // edges still sees both happened.
    std::atomic<juce::uint32> mPlayStartEdges { 0 };
    std::atomic<juce::uint32> mLoopWrapEdges  { 0 };

    // Mixer state -- read by processBlock from PatternManager when available
    // Also settable directly for VST mode (no PatternManager)
    struct MixLevels {
        float master { 1.0f };
        float layers { 1.0f };
        float bass   { 1.0f };
        float drums  { 1.0f };
        bool  layersMute { false };
        bool  bassMute   { false };
        bool  drumsMute  { false };
        bool  layersSolo { false };
        bool  bassSolo   { false };
        bool  drumsSolo  { false };
    };
    MixLevels mixLevels;

    // Standalone: inject PatternManager reference so processBlock can
    // read mixer state and sequence data directly.
    void setPatternManager (PatternManager* pm)
    {
        mPatternManager = pm;
        // THREAD SAFETY: the roll retirement queue's consumer is our audio
        // thread, which reaches it only through this pointer, so its idle
        // assertion has to be seeded from the device state at the moment the
        // pointer is published -- a device opened before the editor existed has
        // already run prepareToPlay, which could not see us then.  Message
        // thread only, and every device start is kicked off by a blocking
        // message-thread call, so this cannot race one.
        if (pm != nullptr)
            pm->setRollConsumerIdle (! mAudioDevicePrepared.load (std::memory_order_acquire));
    }

    // Rebuild AudioFormatReaders for all audio clips in the arrangement.
    // Call from the message thread after importing audio or changing the arrangement.
    void rebuildAudioClipPlayers();

    // ── 5F-4a: Audio-row mixer strip registration ────────────────────────────
    // Called when an audio clip is first placed on a new arrangement row.
    // Creates the `mixer_audio_{row}` APVTS params (if missing) and the Audio
    // InsertNode in BaySickGraph. Safe to call repeatedly.
    void ensureAudioInsert(int row, const juce::String& displayName);

    // ── 5F-4b B2: Aux/Group strip registration ──────────────────────────────
    // Creates an Aux strip at the given idx (0..15). No audio source - aux is
    // receive-only; its input is populated by sends from other strips.
    // Default main-out = FX Bus. Safe to call repeatedly for the same idx.
    void ensureAuxInsert(int idx, const juce::String& displayName);

    // QA-Ef #4 (2026-05-22): tear down EVERY registered aux insert -- unregister
    // each PassiveStripTask from the render dispatcher, reset the task slot, and
    // clear the BaySickGraph aux InsertNodes.  Called from the three load-entry
    // points BEFORE restoreAuxStripsFromState rebuilds from the loaded project,
    // so auxes from the prior session don't leak across loads:
    //   - BaySickDAWProcessor::deserializeProject (project open)
    //   - BaySickDAWProcessor::setStateInformation (VST3 host load)
    //   - StandaloneEditor::doFileNew (File > New)
    //   - StandaloneEditor::loadTemplate (apply template)
    // Each caller raises mProjectLoadInProgress + settles BEFORE calling this
    // so the audio thread is bailing while we mutate the render-task list
    // (processBlock bails to silence while the shield is up).  Mirrors how
    // onTabClosed tears down per-tab engines; aux strips have no per-tab
    // teardown because they aren't tabs.
    void clearAllAuxInserts();

    // R1 (2026-04-23): Vox / Inst strip registration for live-input recording.
    // Up to 6 of each; default main-out = their respective bus (VoxBus / InstBus).
    // R1 registers params + InsertNode only; R2+R3 wire the ASIO input source.
    void ensureVoxInsert  (int idx, const juce::String& displayName);
    void ensureInstInsert (int idx, const juce::String& displayName);

    // J-5 (2026-05-03): BaySickRustyDrums strip registration.  One strip per
    // sound type (kick / snare / tom / hi-hat / ride / crash / china / stack -
    // 13 total for Big Rusty Drums).  Default main-out = kRustyDrumsBus.
    void ensureRustyInsert (int idx, const juce::String& displayName);
    void removeRustyInsert (int idx);

    // J-5: BaySickRustyDrums singleton lifecycle.  PluginProcessor owns the
    // singleton sfizz-driven engine; only one instance allowed per project.
    bool                          hasBaySickRustyDrums() const noexcept;
    class BaySickRustyDrumsProcessor* getBaySickRustyDrums() noexcept;
    // Creates the singleton if absent + loads the kit at sfzPath.  On success:
    // discovers 13 channels, registers 13 strips at kRustyBase..kRustyBase+12,
    // returns true.  Idempotent - calling twice with same path no-ops cleanly.
    bool loadBaySickRustyDrumsKit (const juce::File& sfzPath);
    // Tear-down: free engine + remove all 13 InsertNodes.  APVTS params
    // persist as zombies (existing pattern - JUCE doesn't allow unregister).
    void destroyBaySickRustyDrums();
    // J-8 Part C (2026-05-04): reset every `mixer_rusty_*` + `mixer_rustybus_*`
    // APVTS param to its registered default value.  Called by the page when
    // the user switches programs - wipes mixer state so the new kit's
    // freshly-spawned strips start clean (no stale level/pan/mute/EQ from
    // the previous program).
    void resetBaySickRustyDrumsMixerState();

    // K-2 (2026-05-05): BaySickGuitars per-instance lifecycle.  Each Inst page
    // whose source = BaySickGuitars owns one slot in the per-instance array.
    // PluginProcessor owns the engines so it can drive them on the audio thread
    // alongside the existing Inst chain dispatch.  The `mGuitarsActive[idx]`
    // atomic gates audio-thread access during kit-load (sfizz hash maps are
    // mutated by loadSfzFile and not safe to read concurrently with renderBlock).
    class BaySickGuitarsProcessor* getBaySickGuitars (int instIdx) noexcept;
    // Creates the engine at slot `instIdx` if absent + loads the kit at sfzPath.
    // Active flag dance: false → drain audio thread → loadKit → true.  Returns
    // true on success.  Idempotent - calling with same path on a loaded slot
    // no-ops cleanly.
    bool loadBaySickGuitarsKit (int instIdx, const juce::File& sfzPath);
    // Tear-down: drop the engine at slot `instIdx` (clears the atomic first).
    // APVTS params persist as zombies (JUCE doesn't allow unregister) - they
    // simply have no listener once the engine is gone.
    void destroyBaySickGuitars (int instIdx);

    // L-2 (2026-05-05): BaySickBasses per-instance lifecycle.  Mirrors the
    // Guitars pattern above - separate per-slot arrays so a single Inst tab
    // can be in only one source mode at a time + the engines coexist when
    // multiple tabs of different source modes are open.
    class BaySickBassesProcessor* getBaySickBasses (int instIdx) noexcept;
    bool loadBaySickBassesKit (int instIdx, const juce::File& sfzPath);
    void destroyBaySickBasses (int instIdx);

    // ── sfizz automation wiring (QA-ModelShell TS3 fix, 2026-07-28) ─────────
    // These three engines are processor-owned rather than rig-owned, so they
    // were missed when every other engine family got model-side lane
    // registration -- and the miss was not merely "no automation": the Aria
    // control panel has always offered "Automate: ..." on every kit CC, so
    // right-clicking one CREATED a lane that then applied to nothing.  Silent
    // dead lanes, reported by the owner 2026-07-28.
    //
    // Fired after a kit load completes (outside the per-slot SpinLock -- the
    // audio thread try-locks it, and registration is a map insert per param).
    // Registration overwrites by key, so firing on every successful load is
    // both harmless and what makes a destroy/recreate cycle re-register.
    enum class SfizzEngineKind { Guitars, Basses, RustyDrums };
    std::function<void (SfizzEngineKind kind, int instIdx)> onSfizzEngineReady;

    // Walk every live sfizz engine's APVTS.  The offline lane replay needs this
    // because EngineRig::forEachEngine covers only rig-owned engines, so a
    // sfizz lane would resolve live and be missing from every export.
    void forEachSfizzApvts (const std::function<void (juce::AudioProcessorValueTreeState&)>& fn);

    // R2 (2026-04-23): live-input ASIO channel persistence.  Index lives in
    // APVTS as `<prefix>_inputChannelIdx` (Int -1..127, default -1).  The
    // channel NAME (e.g. "Mic 1") is stored as a non-APVTS attribute on
    // apvts.state via setInputChannelName/getInputChannelName so on device
    // reconnect we can re-resolve by name first, fall back to index.
    void         setInputChannelName (const juce::String& stripPrefix,
                                       const juce::String& name);
    juce::String getInputChannelName (const juce::String& stripPrefix) const;

    // 5F-4b B7 / QA-Ef #4 (2026-05-22): scan a SAVED-FILE state tree for
    // mixer_aux_N_level params and re-register their InsertNodes.  The caller
    // MUST pass a deep copy of the saved tree taken BEFORE apvts.replaceState,
    // not apvts.state itself -- replaceState's rebind appends stale empty
    // <PARAM> nodes for any params that were registered in a prior session but
    // are missing from this file (e.g. open AT1 with 3 auxes, then open AT2
    // with 1 -- apvts.state ends up with mixer_aux_1/2_level nodes too because
    // those adapters are still registered).  Scanning apvts.state would
    // recreate phantom auxes; scanning the pre-rebind file copy only finds
    // auxes that were ACTUALLY saved.
    void restoreAuxStripsFromState (const juce::ValueTree& sourceState);

    // ── C.3 (2026-04-30): Hardware MIDI input ────────────────────────────────
    // The MIDI input thread (StandaloneApp::handleIncomingMidiMessage) pushes
    // every message to mLiveMidiCollector.  processBlock drains the collector
    // each block and routes its messages into the engine page-buffer named by
    // mLiveMidiTargetKind/mLiveMidiTargetIndex (the Piano Roll page's currently
    // focused engine, pushed via setLiveMidiTarget when focus changes).
    //
    // Target encoding (matches PianoRollPage::EngineKind ordering):
    //   1 = Layer   (mLiveMidiTargetIndex = 0..kMaxLayerPages-1)
    //   2 = Bass    (mLiveMidiTargetIndex = 0..kMaxBassPages-1)
    //   3 = Drum    (mLiveMidiTargetIndex = 0..kMaxDrumPages-1)
    //   7 = Guitars (mLiveMidiTargetIndex = 0..kMaxInstPages-1, sfizz Inst)
    //   8 = Basses  (mLiveMidiTargetIndex = 0..kMaxInstPages-1, sfizz Inst)
    //   9 = Rusty Drums (singleton; index ignored)
    //   anything else (0 DrumKit grid / 4 Clip / 5 Vox / 6 live-input Inst /
    //   -1 unset) = drop (no MIDI-driven engine on those pages).
    juce::MidiMessageCollector& getLiveMidiCollector() noexcept { return mLiveMidiCollector; }
    // FALSE until prepareToPlay has reset the collector for the current sample
    // rate.  juce::MidiMessageCollector::addMessageToQueue asserts on
    // hasCalledReset, and MIDI input opens BEFORE the audio device prepares --
    // so a controller that sends anything during startup (a hardware handshake,
    // a knob nudged, an active-sensing burst) hit that assert and stopped the
    // Debug build from opening at all.  The race is timing-dependent, which is
    // why it can lie dormant for months.  Callers must check this first.
    bool isLiveMidiReady() const noexcept
    { return mLiveMidiReady.load (std::memory_order_acquire); }
    void setLiveMidiTarget (int engineKind, int index) noexcept
    {
        mLiveMidiTargetKind .store (engineKind, std::memory_order_relaxed);
        mLiveMidiTargetIndex.store (index,      std::memory_order_relaxed);
    }

    // Live-note monitor: snapshot of the hardware-MIDI notes currently held
    // down, as a 128-bit mask (note n -> lo>>n for n<64, hi>>(n-64) for n>=64).
    // The live-MIDI drain in processBlock sets/clears bits (audio thread); the
    // Piano Roll's 30 Hz timer reads this to light the on-screen keyboard for
    // whatever the user is physically playing (a MIDI-mapping diagnostic).
    void getLiveHeldNotes (uint64_t& lo, uint64_t& hi) const noexcept
    {
        lo = mLiveHeldNotesLo.load (std::memory_order_relaxed);
        hi = mLiveHeldNotesHi.load (std::memory_order_relaxed);
    }

    // ── I-3b (2026-05-02): MIDI Learn registry ───────────────────────────────
    // App-wide MIDI Learn mapping table.  Audio thread reads it during
    // processBlock to dispatch incoming hardware CC / pitch-bend / aftertouch
    // events to APVTS params.  Message thread (UI from I-3c) mutates via the
    // registry's set/remove/learn methods.  Persisted in project XML as a
    // <MidiCCMappings> child of getStateInformation; also overlay-loaded at
    // app startup from Documents/BaySickDAW/MidiMappings.xml (global defaults).
    MidiLearnRegistry&  getMidiLearnRegistry()  noexcept { return mMidiLearn; }
    DrumTriggerMap&     getDrumTriggerMap()     noexcept { return mDrumTriggers; }
    MidiLearnEventQueue& getMidiLearnEventQueue() noexcept { return mMidiLearnQueue; }
    // Resolve a drum's play pitch (published atomic pointer; falls back to C5
    // when the strip's param is not registered yet).  Lock-free const read --
    // safe from the audio thread (kit dispatch) AND the message thread (#32
    // recording demux stamps recorded hits at the drum's play note).
    int  drumPlayNoteRT (int drumIdx) const noexcept;

    // ── Latency ───────────────────────────────────────────────────────────
    // Device output latency (set by StandaloneApp when device initialises or changes).
    void setDeviceOutputLatency (int samples) { mDeviceOutputLatency.store (samples); }
    // 2026-05-02: read-only accessor for the meter latency-compensation toggle
    // (Mixer hamburger menu) -- returns the most recent driver latency in samples.
    int  getDeviceOutputLatency() const noexcept { return mDeviceOutputLatency.load(); }
    // Total latency = PDC (getLatencySamples) + audio device output latency.
    int  getTotalOutputLatency() const
         { return getLatencySamples() + mDeviceOutputLatency.load(); }
    double getSampleRate() const { return mSampleRate; }

    // ── Lazy APVTS registration (Phase 2+) ───────────────────────────────────
    // Called when a Layers/Bass/Drums page is created or destroyed.
    // trackId format: "tk_{index}_{engine}" e.g. "tk_0_Harmless", "tk_2_BaySickBass"
    // Engine types: "Harmless", "BaySickPlayer", "BaySickSynth", "BaySickBass", "BaySickDrums"
    // QA-ApvtsAutomation (2026-07-25): registerParamsForTrack /
    // unregisterParamsForTrack / isTrackRegistered removed -- they registered
    // only dead param families (see PluginProcessor.cpp).

    // ── Engine processor registration (audio thread rendering) ───────────────
    // Called from LayersPage / BassPage on the message thread.
    // The processor must remain alive until unregister is called.
    void registerLayerEngine  (int pageIdx, juce::AudioProcessor* eng);
    void unregisterLayerEngine(int pageIdx);
    // QA-ModelShell TS6 (BLU-447): hosted VST3 instrument tabs.  Mirrors the
    // Layer pair exactly -- these are ENGINE-DRIVEN strips (no live input), so
    // Layer/Bass/Drum is the right reference and Vox/Inst is not.
    void registerPluginEngine  (int pageIdx, juce::AudioProcessor* eng);
    void unregisterPluginEngine(int pageIdx);

    // ── QA-TrueLevel SC-10 (Jeff, 2026-08-22): Direct to Master strips ───────
    // A file playing straight into the master from its own strip: no page, no
    // engine, no audio-library entry.  The processor owns the model (name +
    // stored path) and the render task; the editor owns the Mixer strip and
    // the browser row and rebuilds both from onDirectStripsChanged.  Message
    // thread only.  storedPath is project-relative when the file sits inside
    // the project folder, so the strip survives the project being moved.
    struct DirectStrip
    {
        juce::String name;
        juce::String storedPath;
        bool         missing { false };   // file absent at last open/relink
    };
    int        addDirectStrip     (const juce::File& file, const juce::String& name);
    void       removeDirectStrip  (int idx);
    bool       relinkDirectStrip  (int idx, const juce::File& file);
    void       renameDirectStrip  (int idx, const juce::String& name);
    void       clearDirectStrips  ();
    const DirectStrip* getDirectStrip (int idx) const;
    juce::File resolveDirectStripFile (int idx) const;
    // The ONE writer of the stored form every reader resolves through
    // resolveProjectFile: project-relative inside the project folder, absolute
    // outside it.
    juce::String storedProjectPathFor (const juce::File& file) const;
    std::vector<int> getDirectStripIndices() const;
    std::function<void()> onDirectStripsChanged;
    void registerBassEngine   (int pageIdx, juce::AudioProcessor* eng);
    void unregisterBassEngine (int pageIdx);
    // 2026-04-25: registerDrumsEngine / unregisterDrumsEngine removed
    // (legacy 16-slot processor deleted).
    // D1.2: per-drum-page engine registration (dynamic-drum model).  Mirrors
    // registerLayerEngine - also creates the Drum InsertNode + mixer strip
    // params for that drum's pageIdx.
    void registerDrumEngine  (int pageIdx, juce::AudioProcessor* eng);
    void unregisterDrumEngine(int pageIdx);

    // G-3 (2026-04-28): per-clip-page engine registration.  pageIdx here is
    // the audio-row index for the bound clip (1:1 mapping - each Clips tab
    // claims one of the 50 mixer_audio_<row> inserts on spawn).  Engine
    // output is mixed into THAT row's audio insert during processBlock so
    // arrangement-playback audio + piano-roll-triggered audio share the
    // same EffectRack / EQ / fader chain.  No new mixer strip is created.
    void registerClipEngine  (int pageIdx, juce::AudioProcessor* eng);
    void unregisterClipEngine(int pageIdx);

    // G-4 (2026-04-28): per-Vox / per-Inst page engine registration.  pageIdx
    // is the Vox / Inst insert index (1:1 with mixer_vox_<idx> / mixer_inst_
    // <idx>).  Same shape as registerClipEngine - engine output mixes into
    // the existing Vox / Inst InsertNode (created when the user clicks "Add
    // Vox/Inst Strip" on the Mixer page).
    void registerVoxEngine   (int pageIdx, juce::AudioProcessor* eng);
    void unregisterVoxEngine (int pageIdx);
    void registerInstEngine  (int pageIdx, juce::AudioProcessor* eng);
    void unregisterInstEngine(int pageIdx);

    // THREAD SAFETY: a BaySickPlayer rebuilds its sample-region vector on every
    // load gesture while the audio thread indexes that same vector, and the
    // shield + settle that makes the rebuild safe lives on THIS processor -- the
    // engine has no other route to it.  Called from the four register*Engine
    // paths that can be handed a BaySickPlayer (Layers / Bass / Drums / Clips);
    // the rig registers every engine at creation, before a page exists to issue
    // a load, so no load can find the binding missing.  No unbind: this
    // processor outlives every engine, so the pointer cannot dangle, and
    // clearing it on unregister would silently unprotect a surviving engine.
    void bindSampleLoadShield (juce::AudioProcessor* eng) noexcept;

    // §P4.3 B7 (2026-04-22): per-page pre-rack EQ register/unregister APIs +
    // mDrumsEQDSP / mLayerPageEQs / mBassPageEQs / mDrumsPageEQ members all
    // deleted.  Pre-rack EQs now live on InsertNode / BusNode preEq members
    // inside BaySickGraph; pages bind their EQ display to those via
    // BaySickGraph::getInsertPreEQ() / getXxxBusPreEQ() + the mixer strip APVTS
    // prefix (mixer_{kind}_<N>_preeq_*).

    // ── EQ spectrum feed type (defined in DSP/SpectrumFeed.h, alias kept for compat) ──
    using EQSpectrumFeed = BaySickGraph::SpectrumFeed;

    // ── Level meter feeds (audio thread writes, UI timer reads) ───────────────
    // Peak dB for each mix section - used by MixerPage strip meters.
    std::atomic<float> mMasterPeakDbL       { -60.0f };
    std::atomic<float> mMasterPeakDbR       { -60.0f };
    std::atomic<float> mLayersPeakDbL       { -60.0f };
    std::atomic<float> mLayersPeakDbR       { -60.0f };
    std::atomic<float> mBassPeakDbL         { -60.0f };
    std::atomic<float> mBassPeakDbR         { -60.0f };
    std::atomic<float> mDrumsPeakDbL        { -60.0f };
    std::atomic<float> mDrumsPeakDbR        { -60.0f };
    std::atomic<float> mAudioClipsBusPeakDbL{ -60.0f };
    std::atomic<float> mAudioClipsBusPeakDbR{ -60.0f };
    // R3.5 (2026-04-23): Vox + Inst bus peaks (UI mixer-strip meters).
    std::atomic<float> mVoxBusPeakDbL       { -60.0f };
    std::atomic<float> mVoxBusPeakDbR       { -60.0f };
    std::atomic<float> mInstBusPeakDbL      { -60.0f };
    std::atomic<float> mInstBusPeakDbR      { -60.0f };
    // C.1 (2026-04-30): FX Bus peak - written by BaySickGraph::processBus(kFxBus)
    // each block (mirrors the FX node's internal atomics so MixerPage
    // can read alongside its peers without reaching into BaySickGraph internals).
    std::atomic<float> mFxBusPeakDbL        { -60.0f };
    std::atomic<float> mFxBusPeakDbR        { -60.0f };
    // G-6 (2026-04-29): secondary bus peak meters.
    std::atomic<float> mVoxBus2PeakDbL      { -60.0f };
    std::atomic<float> mVoxBus2PeakDbR      { -60.0f };
    std::atomic<float> mInstBus2PeakDbL     { -60.0f };
    std::atomic<float> mInstBus2PeakDbR     { -60.0f };
    std::atomic<float> mInstBus3PeakDbL     { -60.0f };
    std::atomic<float> mInstBus3PeakDbR     { -60.0f };
    // J-7b (2026-05-04): RustyDrums Bus peak meter - written by the bus
    // pipeline post-fader/pan, drained at end of block alongside every
    // other bus meter so the UI strip sees signal activity.  Run variants
    // declared below alongside the other *Run atomics.
    std::atomic<float> mRustyDrumsBusPeakDbL    { -60.0f };
    std::atomic<float> mRustyDrumsBusPeakDbR    { -60.0f };
    // QA-ModelShell TS6 (BLU-447): hosted VST3 instrument bus.
    std::atomic<float> mPluginsBusPeakDbL       { -60.0f };
    std::atomic<float> mPluginsBusPeakDbR       { -60.0f };
    // QA-Layout T10: secondary group buses.
    std::atomic<float> mLayersBus2PeakDbL       { -60.0f };
    std::atomic<float> mLayersBus2PeakDbR       { -60.0f };
    std::atomic<float> mBassBus2PeakDbL         { -60.0f };
    std::atomic<float> mBassBus2PeakDbR         { -60.0f };
    std::atomic<float> mClipsBus2PeakDbL        { -60.0f };
    std::atomic<float> mClipsBus2PeakDbR        { -60.0f };
    std::atomic<float> mPluginsBus2PeakDbL      { -60.0f };
    std::atomic<float> mPluginsBus2PeakDbR      { -60.0f };
    // QA-SOUNDNESS (2026-08-07): second drum kit's bus.
    std::atomic<float> mDrumsBus2PeakDbL        { -60.0f };
    std::atomic<float> mDrumsBus2PeakDbR        { -60.0f };

    // QA-AudioMeters fix-up (2026-05-24): kPeakAtomicNegInf constant deleted.
    // It was introduced in QA-Eg but never referenced -- every drainAndMerge /
    // exchange-store / casMax site defines its own local kNI / kPeakNegInf /
    // kBusNegInf constexpr.  The Task 4 cleanup-pass comment wrongly claimed
    // "drainAndMerge's skip-on-INF clause uses this sentinel"; in reality
    // drainAndMerge captures its own sentinel via lambda.  Removed to avoid
    // future readers wondering why the constant exists.

    // ── 1M: Audio DSP load monitoring (audio thread writes, UI timer reads) ──
    // mAudioDspLoad : smoothed fraction of buffer window used by processBlock (0..1)
    // mDspOverload95: true when current smoothed load >95% (UI red flash)
    // Nothing here protects against overload; both flags are UI signals only.
    std::atomic<float> mAudioDspLoad  { 0.0f };
    std::atomic<bool>  mDspOverload95 { false };
    // Cached one-pole coefficient for the load meter, keyed on the block
    // DURATION so the meter's response time in seconds is the same everywhere
    // on the rate x block matrix.  Audio-thread-only (measureDspLoadAndOverload
    // is the sole reader and writer) - no atomics needed.
    double mDspLoadAlphaBufDur { -1.0 };
    float  mDspLoadAlpha       { 0.0f };

    // ── Audio clip playback (song mode) ──────────────────────────────────────
    juce::AudioFormatManager  mAudioFormatManager;
    juce::TimeSliceThread     mAudioFileThread { "AudioClipBG" };

    // ── CL-281 decode-once clip cache ────────────────────────────────────────
    // MESSAGE THREAD ONLY.  The audio thread never touches this container -- it
    // only reads the immutable buffers the published AudioClipSnapshot holds
    // aliases to, which is why no lock, RCU snapshot or retirement queue is
    // needed for the map itself.  rebuildAudioClipPlayers is the sole mutator
    // and it is the sole reader.
    //
    // KEY: clipAudioCacheKey() -- the RESOLVED absolute path plus the file's
    // size and modification time.  Resolution is what makes "library:kick.wav",
    // "mysamples:kick.wav" and the absolute form one entry; size + mod time are
    // what make a file REPLACED on disk decode again instead of playing stale
    // PCM.  Nothing else changes the decoded result: trim, stretch, pitch,
    // reverse and the Player chain all act downstream of the decode.
    //
    // EVICTION: an entry lives exactly as long as the published arrangement
    // references its file.  Every rebuild collects the keys it used and drops
    // every other entry, so a clip deleted from the timeline stops pinning its
    // PCM at the next rebuild (which a delete itself triggers).  The bytes free
    // when the LAST alias dies -- the map's, plus one per clip -- so a still-
    // retired snapshot keeps its own audio alive until the GC drainer destroys
    // it, and that free never lands on the audio thread.
    std::map<juce::String, DecodedClipAudioPtr> mDecodedClipCache;

    // Identity + change stamp for the cache key.  Empty when the file does not
    // exist (an uncacheable reference must never share an entry with another).
    static juce::String clipAudioCacheKey (const juce::File& resolvedFile);

    // Decodes the whole file to float PCM when it is at or under
    // AudioClipStreamer::kRamThresholdBytes (the SAME test and the same channel
    // fold the streamer's own RAM path applies, read from that class rather
    // than restated); returns null for anything bigger, which is the signal to
    // keep streaming it.  Message thread -- allocates and does disk IO.
    static DecodedClipAudioPtr decodeClipAudioIfCacheable (juce::AudioFormatReader& reader);

    struct AudioClipPlayer {
        double clipStartBeat  = 0.0;
        double clipEndBeat    = 0.0;
        double fileSampleRate = 44100.0;
        int    trackRow       = 0;
        float  originalBPM    = 120.f;
        bool   stretchMode    = true;
        // I-16 G-9 (2026-05-03): when non-zero AND clipType==Audio, the player
        // routes its samples through the Vox/Inst page identified by this
        // mixer channel id (e.g., MixerChannelIds::voxInsert(0)) instead of
        // through a new Audio row.  Copied from ArrangementBlock at
        // rebuildAudioClipPlayers() time.
        int    routeChannel   = 0;
        // D3: choke group (0 = none, 1..16).  Copied from the source clip's
        // AudioLibraryEntry at rebuildAudioClipPlayers() time so the audio
        // thread can read it without a library lookup.
        int    chokeGroup     = 0;
        // D3: true once a peer in the same choke group fires during this clip's
        // current playback.  Reset to false each block when the playhead is
        // outside the clip range (so a fresh playthrough starts un-choked).
        bool   mutedByChoke   = false;
        // QA-Ea Task 0c (FL pre-roll record + non-destructive clip trim):
        // file-position offset added to every file-position read in the
        // audio-clip render loop (direct-read filePos, phase-vocoder
        // reference pvRefPos, file-EOF guard fileTotalSamples) so the clip
        // plays from sample N of the file rather than 0.  Copied from
        // ArrangementBlock::contentStartSamples at rebuildAudioClipPlayers
        // time.  Default 0 = play from file sample 0 (every pre-Task-0c
        // clip is unaffected; backwards-compatible).
        juce::int64 contentStartSamples { 0 };
        // The clip's audio: a shared decoded buffer when the file is at or under
        // AudioClipStreamer::kRamThresholdBytes (CL-281), otherwise an owned
        // disk streamer whose background thread pre-fetches for the audio thread.
        std::unique_ptr<ClipSource> source;
        // Phase vocoder for BPM-aware time stretch.  ALWAYS created (QA-ClipPlayback
        // Task 3) so length-preserving pitch and reverse work on any clip; the
        // render bypasses it when the clip is forward + unstretched + unpitched.
        std::unique_ptr<PhaseVocoder> vocoder;
        // Pre-allocated scratch buffers used by the audio thread (no heap alloc in processBlock).
        juce::AudioBuffer<float> pvInBuf;   // raw file samples fed into vocoder
        juce::AudioBuffer<float> pvOutBuf;  // stretched output from vocoder (file SR)
        // Expected next file position - used to detect seeks and reset vocoder.
        int64 expectedFilePos { 0 };
        // QA-Ec G1-boundary fix: fractional vocoder-OUTPUT read position -
        // carries the interp phase across blocks (peekOutput/advanceOutput
        // pattern); reset alongside vocoder->reset() on seeks.
        double pvOutFrac { 0.0 };
        // Mute/choke gates skip this player's whole render body, freezing
        // expectedFilePos while the transport advances -- a sub-2-second gap
        // slips under the PV seek tolerance and playback resumes offset (and
        // stays offset).  Set by the gates, consumed by the first rendered
        // block after them to force the re-sync.
        bool unmuteResync { false };
        // QA-Fa recovery: align live-warp per-clip state.  The warp branch
        // in decodeFilePlayClip owns these; all zero-cost when the clip's
        // channel has no applied map.
        //   alignEngaged        - clip is currently decoding through the
        //                         PV-warp path (vs the untouched original
        //                         paths when the chain is OFF and settled).
        //   alignPosCorr        - file-domain position correction leftover
        //                         from the last law change (toggle / Apply /
        //                         revert); drains to 0 over ~50 ms as a
        //                         consumption-rate glide (rule 5 no-click).
        //   alignLastLawEnd     - file-domain end position of the previous
        //                         block's law; a mismatch vs this block's
        //                         law start IS the law-change detector.
        //   alignLastEndTimeline- previous block's end timeline sample;
        //                         discontinuity = transport seek -> hard
        //                         resync instead of glide.
        //   alignRho            - smoothed pitch ratio (anchor semis +
        //                         transpose), ~50 ms approach.
        //   alignInFrac         - fractional file-consumption carry so the
        //                         PV input feed matches the effective
        //                         (hop-quantized) stretch long-run exactly.
        bool   alignEngaged        { false };
        double alignPosCorr        { 0.0 };
        double alignLastLawEnd     { -1.0 };
        int64  alignLastEndTimeline { -1 };
        double alignRho            { 1.0 };
        double alignInFrac         { 0.0 };
        // QA-ClipPlayback Task 2: per-clip DSP state for the ClipsPage BaySickPlayer
        // control chain applied to timeline-WAV (Flow B) playback so a WAV clip
        // matches the sampler.  Prepared + reset at rebuildAudioClipPlayers time
        // (like vocoder/pvInBuf).  Per-clip so filter / vibrato state never bleeds
        // between clips sharing a row.
        juce::dsp::StateVariableTPTFilter<float> clipFilter;
        float  clipTrebleLp[2] { 0.f, 0.f };   // one-pole treble-shelf state per channel
        double clipLfoPhase    { 0.0 };        // pitch-vibrato LFO phase, output-sample domain
        // Vibrato reads at a modulated rate while the render is handed an UNmodulated
        // block-start file position every block, so the integrated position deviation
        // has to survive the boundary - dropping it would re-splice the read at every
        // buffer edge.  File-domain samples; bounded and zero-mean by construction.
        double clipVibOffset   { 0.0 };
        int    clipReductStep  { 0 };          // sample-rate-reduction hold counter
    };
    // 2026-05-06 (Batch 9c B1): deferred-destruction GC pattern.
    //
    // Audio thread reads the active snapshot lock-free at the top of
    // processBlock (mActiveAudioClips.load -> mCurrentBlockClipSnapshot)
    // and uses the same pointer for the entire block (every iteration site
    // + every RenderTask reads mCurrentBlockClipSnapshot->players).
    //
    // Mutator (rebuildAudioClipPlayers, message thread) builds a fresh
    // AudioClipSnapshot, atomic-exchanges it into mActiveAudioClips, and
    // retires the old one to mClipRetirement with retiredBeforeGen set to
    // the new snapshot's generation.  The drainer thread (owned by
    // RetirementQueue) destroys the old snapshot once the audio thread has
    // demonstrably moved past the retire point (the in-use gen the audio
    // thread publishes via mClipRetirement.setInUseGeneration >=
    // retiredBeforeGen) -- guaranteeing that neither the slow
    // ~AudioClipStreamer (file close + TimeSliceClient unregister) nor the last
    // DecodedClipAudio release (a multi-MB free) EVER runs on the audio thread.
    //
    // Replaces the pre-9c per-site try-lock pattern (audio bailed silent
    // for a block under MT contention) and the reverted a19c6e3 "lock for
    // whole block" snapshot (which deadlocked with audio->Win32 GUI calls
    // -- closed by the B2 fix in 3b2c85a).
    struct AudioClipSnapshot
    {
        std::vector<AudioClipPlayer> players;
        std::uint64_t                generation { 0 };
    };

    // Atomically published from the message thread; read once per audio
    // block via load-acquire.  The pointer's target is owned by whoever
    // last took it OUT of the atomic (initial bootstrap in the ctor;
    // mutator's exchange returns the previous value to the retirement
    // queue; ~BaySickDAWProcessor explicitly deletes the final value).
    std::atomic<AudioClipSnapshot*> mActiveAudioClips  { nullptr };

    // Mutator's monotonic generation counter.  Each rebuild assigns
    // gen = mNextClipGen.fetch_add(1) + 1, so the first published snapshot
    // beyond the bootstrap (gen 0) gets gen 1.
    std::atomic<std::uint64_t>      mNextClipGen       { 0 };

    // GC queue for retired AudioClipSnapshot instances.  Owns its own
    // dedicated drainer thread (see Engine/RetirementQueue.h).
    RetirementQueue<AudioClipSnapshot> mClipRetirement;

    // Audio-thread-only.  Captured at the top of processBlock (under the
    // load-acquire of mActiveAudioClips) and consumed by every iteration
    // site for the rest of that block: FilePlay pre-scan, Pass 2
    // song-mode loop, applyChokeGroupDispatch (both sub-loops), the
    // shared renderAudioClipsForRow / decodeFilePlayClip / finalizeFilePlayStrip
    // helpers, plus the AudioInsertTask / VoxStripTask / InstStripTask MT workers.
    // Visibility to MT workers is established by the dispatcher's
    // notify/wait release-acquire pair (workers wake AFTER the audio
    // thread has written this).
    AudioClipSnapshot* mCurrentBlockClipSnapshot { nullptr };

    // QA-Fa recovery: per-block align live-warp cache, captured right after
    // mCurrentBlockClipSnapshot (audio thread; MT workers see it through the
    // same dispatcher release-acquire pair).  One entry per Vox page: the
    // engine's published AlignPlaySnapshot (nullptr when none / unusable)
    // and the bsa_ chain-switch + Pitch-box gate reads for this block.
    // decodeFilePlayClip matches a clip's routeChannel against the cached
    // snapshot's followerChannelId -- constant per-block cost, zero per-clip
    // casts, and pointer liveness is the publisher's retire-ring contract.
    struct AlignBlockEntry
    {
        const AlignPlaySnapshot* snap        { nullptr };
        bool                     chainOn     { true };
        bool                     pitchOn     { false };
        float                    transpose   { 0.0f };
        // Source-position stamp: decodeFilePlayClip writes it when the
        // composed law is engaged (timeline-equivalent samples at the
        // device rate + per-sample rate); finalizeFilePlayStrip forwards
        // it to the engine so the pitch applicator resolves pills in the
        // SOURCE domain.  Written and read on the strip's own render
        // thread within one block (no cross-thread hazard).
        double srcX0    { 0.0 };
        double srcRate  { 1.0 };
        bool   srcSet   { false };
    };
    std::array<AlignBlockEntry, kMaxVoxPages> mBlockAlignEntries {};

    // ── Batch 5 (2026-05-06): shared audio-clip render path ──────────────────
    // Per-block context bundle passed into renderAudioClipsForRow.  Keeps the
    // helper signature short while preserving access to all the per-block
    // values the render needs.
    struct AudioClipBlockContext
    {
        double      bpm              = 120.0;
        bool        anySolo          = false;
        double      secPerBeat       = 0.5;
        juce::int64 projectStart     = 0;
        juce::int64 projectEnd       = 0;
        int         numSamples       = 0;
        int         numOut           = 2;
        // No master gain here (QA-TrueLevel SC-1): the master fader belongs to
        // the terminal chain only.  The MT strip tasks used to hand the fader x
        // the hidden "masterGain" param down to the decode, so every clip took
        // both TWICE (decode + master chain) while synth engines took them once.
        const MixerState* mxState    = nullptr;   // PatternManager.h top-level struct
        juce::AudioBuffer<float>* clipScratch = nullptr;   // shared decode buffer
        // QA-ClipPlayback Task 2: the row's ClipsPage BaySickPlayer (null when the
        // clip engine isn't a BaySickPlayer) - the timeline-WAV decode reads its
        // Player controls live and applies them per-clip before the raw sum.
        BaySickPlayerProcessor* clipPlayer = nullptr;
    };

    // Decode all non-FilePlay audio clips on `row` and sum their RAW output into
    // `mtDest` (the row InsertNode's pull-model output buffer; always non-null).
    // Called per audio row by CompositeAudioInsertTask::run.  QA-MultiBlockHazard:
    // the insert chain is NOT applied here -- the caller runs processInsert ONCE
    // per block on the summed sources.  Returns true if >=1 clip contributed.
    // FilePlay clips (clip routed to a Vox/Inst page) are skipped - handled by
    // the FilePlay pass in processBlock.
    bool renderAudioClipsForRow (int row,
                                 const AudioClipBlockContext& ctx,
                                 juce::AudioBuffer<float>* mtDest);

    // QA-MultiBlockHazard (Task 2): the Vox/Inst FilePlay path is split into a
    // per-clip decode + a once-per-block finalize so a stateful engine + rack
    // advance once per block instead of once per clip (was renderFilePlayPlayer,
    // Batch 9b Item 9).  Caller MUST be iterating mCurrentBlockClipSnapshot->
    // players (snapshot captured at processBlock top, alive for the block via the
    // RetirementQueue ack) and MUST have filtered the player by FilePlay status
    // (player.routeChannel is a Vox or Inst id).
    //
    // decodeFilePlayClip: decode the in-block portion of ONE clip (phase vocoder
    // OR direct-path SR-interp) into ctx.clipScratch and ADD it (raw) into
    // sumDest.  Does NOT run the engine or insert chain.  Caller sizes + clears
    // sumDest before the per-clip loop.  Returns true if the clip contributed
    // (false = out of range / muted / choke / EOF).
    bool decodeFilePlayClip (AudioClipPlayer&             player,
                             const AudioClipBlockContext& ctx,
                             juce::AudioBuffer<float>&    sumDest);

    // finalizeFilePlayStrip: run the Vox/Inst engine + insert chain ONCE on the
    // summed clips (engineSum), then addFrom into mtDest (the strip's pull-model
    // output buffer, non-null).  routeCh = the strip's channel id (all summed
    // clips share it); engineMidi = the page's per-block MIDI buffer.  Caller
    // invokes this once per block iff >=1 clip contributed.
    void finalizeFilePlayStrip (int                          routeCh,
                                const AudioClipBlockContext& ctx,
                                juce::MidiBuffer&            engineMidi,
                                juce::AudioBuffer<float>*    mtDest,
                                juce::AudioBuffer<float>&    engineSum);

    // QA-F Task 1: offline channel-composite renderer -- the shared analysis
    // foundation for BaySickAlign / BaySickPitch.  MESSAGE THREAD ONLY: opens
    // its own readers from the arrangement blocks and never touches the
    // audio-thread clip snapshot, so it is safe during live playback.  Sums
    // every un-muted clip routed to channelId (FilePlay takes included) at
    // its grid position, mono, at the device sample rate.  outStartBeat /
    // outStartSample = timeline beat + timeline sample of composite sample 0
    // (the sample form lets two composites be common-origin padded without
    // re-deriving the beat->sample mapping).  Empty buffer when the channel
    // has no decodable clips.
    juce::AudioBuffer<float> renderChannelComposite (int channelId, double& outStartBeat,
                                                     juce::int64& outStartSample);

    // QA-F Task 3: order-independent signature of a channel's clip layout
    // (path/start/length/trim/stretch per routed block).  BaySickAlign
    // compares analyze-time vs current to flag a stale WarpMap.  Message
    // thread only.
    juce::int64 channelClipSignature (int channelId) const;

    // QA-F Task 3: channels that currently have audio clips routed to them
    // (candidates for the BaySickAlign Leader/Follower pickers).  Generic
    // kind+index labels; tab renames live at the editor layer.
    std::vector<std::pair<int, juce::String>> listAudioClipChannels() const;

    // QA-AudioMeters (2026-05-24): per-kind insert peak snapshot mirrors -- the
    // UI poll target for all 8 InsertKinds.  Audio thread writes via
    // drainMeterAtomicsForUI's 8-per-kind drainAndMerge loop (which drains
    // mVibeGraph.<kind>InsertPeakDb*[index] -> these mirrors); UI vblank
    // exchange-and-resets to start a fresh max-since-last-frame window.
    // mAudioRowPeakDb* keeps its existing name (Audio kind) for Builder grid
    // backward compat; the other 7 kinds use the m<Kind>InsertPeakDb* naming.
    // QA-AudioMeters fix-up (2026-05-24): mono m<Kind>InsertPeakDb mirrors
    // (one per kind) deleted as dead writes -- no UI consumer ever read them;
    // drainInsertPeakDbStereo only returns L/R.  Bus mono mirrors above are
    // also dead (pre-existing pre-batch); left for a separate cleanup batch.
    static constexpr int kMaxAudioRows = 100;   // QA-Layout T11: 50 -> 100 with the Clips cap
    std::atomic<float> mAudioRowPeakDbL[kMaxAudioRows];
    std::atomic<float> mAudioRowPeakDbR[kMaxAudioRows];

    std::atomic<float> mLayerInsertPeakDbL[kMaxLayerPages];
    std::atomic<float> mLayerInsertPeakDbR[kMaxLayerPages];
    std::atomic<float> mBassInsertPeakDbL [kMaxBassPages];
    std::atomic<float> mBassInsertPeakDbR [kMaxBassPages];
    std::atomic<float> mDrumInsertPeakDbL [kMaxDrumPages];
    std::atomic<float> mDrumInsertPeakDbR [kMaxDrumPages];
    std::atomic<float> mAuxInsertPeakDbL  [MixerChannelIds::kMaxAuxStrips];
    std::atomic<float> mAuxInsertPeakDbR  [MixerChannelIds::kMaxAuxStrips];
    std::atomic<float> mVoxInsertPeakDbL  [MixerChannelIds::kMaxVoxStrips];
    std::atomic<float> mVoxInsertPeakDbR  [MixerChannelIds::kMaxVoxStrips];
    std::atomic<float> mInstInsertPeakDbL [MixerChannelIds::kMaxInstStrips];
    std::atomic<float> mInstInsertPeakDbR [MixerChannelIds::kMaxInstStrips];
    std::atomic<float> mRustyInsertPeakDbL[MixerChannelIds::kMaxRustyStrips];
    std::atomic<float> mRustyInsertPeakDbR[MixerChannelIds::kMaxRustyStrips];
    std::atomic<float> mPluginInsertPeakDbL[MixerChannelIds::kMaxPluginStrips];
    std::atomic<float> mPluginInsertPeakDbR[MixerChannelIds::kMaxPluginStrips];
    std::atomic<float> mDirectInsertPeakDbL[MixerChannelIds::kMaxDirectStrips];   // QA-TrueLevel SC-10
    std::atomic<float> mDirectInsertPeakDbR[MixerChannelIds::kMaxDirectStrips];

    // QA-AudioMeters: UI-side exchange-and-reset drain for any insert kind.
    // Returns the running max-since-last-call for the (kind, index) pair from
    // the appropriate m<Kind>InsertPeakDb*L/R mirror; resets to -inf so the
    // next vblank window starts fresh.  Wait-free, noexcept, relaxed memory
    // ordering.  **Single-consumer (UI vblank); concurrent callers race on the
    // exchange-reset** -- a second concurrent caller would receive -inf as the
    // first call's exchange already cleared the mirror.  Currently the only
    // consumer is MixerPage::onVBlank.
    std::pair<float, float> drainInsertPeakDbStereo (BaySickGraph::InsertKind kind, int index) noexcept;
    // QA-RustyMeter (2026-05-30): RMS sibling for the split meter; thin passthrough
    // to mVibeGraph.drainInsertNodeRms (no mirror -- RMS is read off the node).
    std::pair<float, float> drainInsertRmsDbStereo (BaySickGraph::InsertKind kind, int index) noexcept;
    // QA-RustyMeter part 2 (2026-05-30): bus RMS sibling; thin passthrough to
    // mVibeGraph.drainBusRms.  busChId is a MixerChannelIds bus id; kMaster (and
    // any unknown id) returns {-inf,-inf} (Master keeps a full peak bar, no RMS).
    std::pair<float, float> drainBusRmsDbStereo (int busChId) noexcept;
    // QA-RustyMeter Task 3 (2026-05-30): master LUFS readout for the LufsReadoutBox.
    // mode 0=Momentary / 1=Short-Term / 2=Integrated.  Passthrough to mVibeGraph.
    float getMasterLufs (int mode) const noexcept;

    // CL-044 (QA-ModelShell TS7): master-out spectrum tap for the floating
    // analyzer window.  Tapped post fader/pan/width, same point as the LUFS
    // meter.  The active flag is what keeps a closed window free.
    void setMasterSpectrumActive (bool on) noexcept;
    bool pollMasterSpectrum (float* dest, int& outCount) noexcept;

    // ── TS7 §6: the freeze driver ─────────────────────────────────────────────
    // Lives here because the processor owns BOTH halves it has to join: the rig
    // (freeze state) and the render tasks (the audio-path switch).  The RENDER
    // itself lives on BuilderPage, so it arrives as a hook rather than a
    // dependency -- the processor must not reach into a view.
    //
    // Returns false + fills outErr if the render failed; the tab is left
    // unfrozen in that case rather than frozen against a file that does not
    // exist.
    // reuseValid skips any file whose CONTENT STAMP still matches -- project
    // restore passes it, so reopening a project reuses renders that are still
    // correct instead of re-rendering every frozen tab.  Off by default: an
    // explicit re-freeze should always produce fresh audio.
    // songScopeOnly (Jeff's ruling 2-b, 2026-07-31): AUTOMATIC freezes render
    // the song scope only and leave pattern coverage to the staggered filler
    // below, so an uninvited render never stalls the app for a whole
    // multi-pattern set.  Manual freezes render their full set.
    bool freezeTab   (TabKind kind, int pageIndex, juce::String& outErr,
                      bool byUser = true, bool reuseValid = false,
                      bool songScopeOnly = false);
    void unfreezeTab (TabKind kind, int pageIndex);
    // Re-renders a frozen tab whose content changed (§6.6).  Playback keeps
    // falling back to the live engine until the new file is in place, so this is
    // never audible as a gap.
    bool refreshFreeze (TabKind kind, int pageIndex, juce::String& outErr,
                        bool songScopeOnly = false);

    // Ruling 2-b's staggered pattern coverage: the editor's 5 Hz poll asks for
    // ONE missing per-pattern render at a time (stopped + quiet only) and
    // shows the render notice around it.  find is a const scan; render does
    // one pattern (reusing a stamp-matched file when one exists) and
    // republishes.  Skips stale tabs -- the refresh queue owns those.
    bool findPendingPatternFreeze (TabKind& outKind, int& outPage, int& outPattern) const;
    bool renderPatternFreeze (TabKind kind, int pageIndex, int patternIndex,
                              juce::String& outErr);

    // §6.6: a stale freeze plays LIVE, immediately -- the rig's staleness marks
    // call this to null every frozen-source pointer a tab has published (song +
    // pattern; all 13 strips + the producer flag for Rusty).  ATOMIC NULLS ONLY:
    // ownership and destruction stay with the tab, so no settle is needed here.
    void retractFrozenSources (TabKind kind, int pageIndex);
    // Current pattern for the rig's republish calls (-1 when no manager).
    int  freezePatternIndexNow() const noexcept;

    // §6.7 file rules.  freezeFileFor is the ONE name builder -- it spells the
    // kind as a name rather than the TabKind ordinal, so inserting an enumerator
    // cannot silently re-point the freeze files a saved project refers to.
    // Not const: getProjectFreezeDir creates the folder on demand.
    // §6.8: patternIndex < 0 is the SONG-scope file; anything else is that
    // pattern's own render.  The scope is part of the name because the two are
    // different audio of different lengths.
    juce::File freezeFileFor      (TabKind kind, int pageIndex, int patternIndex = -1);
    // Patterns this tab has notes in -- the set a per-instrument freeze renders.
    std::vector<int> patternsWithContentFor (TabKind kind, int pageIndex) const;
    // `tab_<kind>_<index>_` -- the family every scope of one tab's freeze shares.
    juce::String     freezeFilePrefixFor (TabKind kind, int pageIndex) const;
    void       deleteFreezeFileFor (TabKind kind, int pageIndex);
    // Removes freeze files whose tab no longer exists.  Call after a project's
    // tabs are restored; without it the folder only ever grows.
    void       sweepOrphanFreezeFiles();

    // Supplied by StandaloneEditor, calling BuilderPage::renderFreezeFile.
    // The RenderTask rides along purely so the render can prune the graph to it
    // (see setFreezePrune) -- the tap itself is still addressed by InsertKind.
    // §6.8: patternIndex < 0 renders SONG scope; anything else renders just that
    // pattern, which is what makes freeze work in pattern mode at all.
    std::function<bool (BaySickGraph::InsertKind, int index, RenderTask*, int patternIndex,
                        const juce::File&, juce::String&)> onRenderFreezeFile;

    // §6.8 stepped progress: a per-instrument freeze is 1 + N renders, so a
    // single anonymous bar would sit at 0 for the whole run.  "Freezing Drums -
    // pattern 3 of 7".
    std::function<void (int done, int total, const juce::String& label)> onFreezeStep;

    // TS7 §6.9: the kit's thirteen strips in ONE render pass.  A separate hook
    // rather than N calls to the one above, because thirteen separate renders
    // would pay the offline setup cost -- and the silence -- thirteen times over
    // for audio one sfizz pass already produces together.
    // patternIndex < 0 renders SONG scope; otherwise just that pattern -- the
    // kit is ONE instrument, so it gets per-pattern renders like every other.
    std::function<bool (const std::vector<juce::File>&, RenderTask*, int patternIndex,
                        juce::String&)> onRenderKitFreezeFiles;

    // The kit publishes THIRTEEN pattern sources, one per strip, so it cannot go
    // through renderTaskForTab the way every other kind does.  Null clears them.
    void setRustyFrozenPatternSources (
        const std::vector<std::unique_ptr<AudioClipStreamer>>* streams, int patternIndex);
private:
    bool setRustyFrozenPatternSourcesImpl (
        const std::vector<std::unique_ptr<AudioClipStreamer>>* streams, int patternIndex);
public:

    // TS7 §6.9: forwards to the dispatcher.  Exposed here because the renderer
    // lives on BuilderPage and must not reach into the private render graph.
    //
    // Thread safety: setFreezePrune walks every task's mPredecessors, which the
    // audio thread clears and refills in rebuildLinks on EVERY block -- so the
    // walk has to happen while the audio thread is not in the graph.  Shield
    // (audio clears to silence) -> settle the in-flight block -> walk -> restore.
    // Nest-aware: a caller that already raised the shield pays no second settle.
    // The nullptr branch only stores a pointer and needs no barrier.
    void setFreezePrune (RenderTask* target)
    {
        if (target == nullptr) { mRenderDispatcher.setFreezePrune (nullptr); return; }

        const bool shieldWasUp = isProjectLoadInProgress();
        setProjectLoadInProgress (true);
        if (! shieldWasUp) settleAudioThread();
        mRenderDispatcher.setFreezePrune (target);
        setProjectLoadInProgress (shieldWasUp);
    }

    bool freezeRustyKit (juce::String& outErr, bool byUser, bool reuseValid = false,
                         bool songScopeOnly = false);

    // The task carrying a tab's audio, or null.  The freeze switch and any
    // future per-tab audio routing both need it.
    // RenderTask, not EngineInsertTask: Vox / Inst strips are plain RenderTasks
    // and the narrower type is what shut freeze out of them (TS7 §6.9).
    RenderTask* renderTaskForTab (TabKind kind, int pageIndex) noexcept;

    // The playhead child engines are currently pointed at.  EVERY engine
    // creation path must hand this to the new engine -- see mLastEnginePlayHead
    // for why the per-block change-gate cannot cover engines created later.
    // Null before the first audio block, which is harmless: that block's change
    // test then propagates to everything at once.
    juce::AudioPlayHead* enginePlayHead() const noexcept { return mLastEnginePlayHead; }
    float getMasterTruePeakDb() const noexcept;
    float getMasterTruePeakDbChannel (int ch) const noexcept    { return mVibeGraph.getMasterTruePeakDbChannel (ch); }
    float getMasterTruePeakMaxDbChannel (int ch) const noexcept { return mVibeGraph.getMasterTruePeakMaxDbChannel (ch); }
    float getMasterCorrelation() const noexcept                 { return mVibeGraph.getMasterCorrelation(); }
    std::pair<float, float> drainMasterAnalyzerPeakDb() noexcept { return mVibeGraph.drainMasterAnalyzerPeakDb(); }

    // ── TS7 §3: version capture support ──────────────────────────────────────
    // Edge counters (§3.2) read by the editor's timer; the audio thread never
    // calls into capture.
    juce::uint32 getPlayStartEdges() const noexcept
        { return mPlayStartEdges.load (std::memory_order_relaxed); }
    juce::uint32 getLoopWrapEdges() const noexcept
        { return mLoopWrapEdges.load (std::memory_order_relaxed); }
    // The master tap kept alive for capture's analysis half (§3.1), independent
    // of whether the analyzer window is open.
    void  setMasterAnalysisActive (bool on) noexcept;
    float getMasterTruePeakMaxDb() const noexcept;
    void  resetMasterTruePeakMax() noexcept;

    // §3.5 audio half.  Reuses AudioFileRecorder (the existing writer) at the
    // existing pre-metronome master tap; independent of the user's record path
    // so the two can run at once.  Returns false if the file could not be
    // opened -- capture then runs analysis-only rather than claiming audio it
    // does not have.
    bool startMasterCapture (const juce::File& target);
    juce::File stopMasterCapture();
    bool isMasterCapturing() const { return mCaptureRecorder.isRecording(); }

    // ── Graph infrastructure (Phase 1A) ───────────────────────────────────────
    BaySickGraph mVibeGraph;

    // ── Recording (1G MIDI / 1H Audio) ────────────────────────────────────────
    // R5d (2026-04-24): rewritten to support mode-aware capture.
    //   - Audio mode: if any Vox / Inst strip is armed, each one's pre-rack
    //     input is tapped into its own WAV; otherwise the master output is
    //     captured (fallback).
    //   - MIDI mode: only mMidiRecorder runs; no WAVs.
    // StandaloneEditor owns all target-commit logic (drop WAV on arrangement
    // row, drop notes into the last-accessed piano roll); this class only
    // opens / closes writers and returns a result struct.
    MidiRecorder      mMidiRecorder;

    enum class RecordMode { Audio, Midi };

    struct StripRecorder
    {
        int                                channelId;       // MixerChannelIds
        juce::String                       displayName;     // e.g. "Vox 1"
        juce::File                         file;            // dry file (RAW pre-chain ASIO)
        std::unique_ptr<AudioFileRecorder> recorder;        // dry recorder

        // I-16 G-9 (2026-05-03): Vox-only second writer for the WET tap.
        // BaySickVocalProcessor pushes post-realtime / pre-vocal-chain audio
        // here.  Inst strips leave wetRecorder null (no realtime stage to
        // bake into a wet capture).
        juce::File                         wetFile;
        std::unique_ptr<AudioFileRecorder> wetRecorder;
    };

    struct RecordResult
    {
        juce::File                                masterFile;     // only when no strips armed
        std::vector<std::pair<int, juce::File>>   stripFiles;     // per-strip dry captures
        std::vector<std::pair<int, juce::File>>   stripWetFiles;  // I-16 G-9: per-strip wet (Vox only)
        std::vector<PianoNote>                    midiNotes;      // from MidiRecorder
        double                                    startBeat { 0.0 };
        // QA-Ea Task 0c (FL pre-roll record + non-destructive clip trim):
        // count-in samples captured before transport-start (the FL pre-roll
        // period).  commitRecordingResult uses this to set every resulting
        // Audio block's contentStartSamples + shorten its lengthBeats so the
        // visible clip starts at the song downbeat (not file sample 0) while
        // the WAV still holds the full pre-roll bar.  MIDI commit shifts
        // captured note startBeats by the beat-equivalent and applies the
        // Noodling-discard + Early-Strike-clamp + input-quantize rules.
        // Zero when no count-in fired (existing behavior unchanged).
        juce::int64                               preRollSamples { 0 };
        // Armed strips whose capture produced no file (writer failed to open
        // at arm, or nothing landed on disk at stop).  The commit dialog names
        // them -- an armed strip must never just vanish from the results.
        std::vector<std::pair<int, juce::String>> failedStrips;    // channelId, displayName
        // Captures that DID produce a file but with a hole in it: the disk
        // writer refused whole blocks (see AudioFileRecorder::
        // getDroppedBlockCount), so the WAV is spliced, not merely short.  Same
        // contract as failedStrips -- a damaged take must reach the commit
        // dialog rather than be handed back looking complete.
        struct DroppedTake
        {
            int          channelId;      // MixerChannelIds; 0 for the master capture
            juce::String displayName;
            int          droppedBlocks;
        };
        std::vector<DroppedTake> droppedTakes;
    };

    // Called from StandaloneEditor onPlay when Record is armed.  Allocates
    // writers per mode + per armed strip; safe no-op if already recording.
    //   projectName / samplesFolder come from ProjectManager (empty project
    //   name is handled by caller - it blocks with a save-first prompt).
    void startRecording (RecordMode mode,
                         double startBeat,
                         const juce::String& projectName,
                         const juce::File& samplesFolder);

    // Called on Pause or Stop.  Closes all writers + returns the captured
    // files + MIDI notes + the startBeat for arrangement-row placement.
    RecordResult stopRecording();

    // 2026-05-06 (Batch 9b Item 8): dry-recorder tap helper.  Locates the
    // StripRecorder whose channelId matches and writes one block of mono
    // input (typically a single channel of mLiveInputSnapshot).  Called from
    // VoxStripTask / InstStripTask::run on the audio thread when those strips
    // are armed.
    //
    // Thread safety: mStripRecorders is a plain vector the message thread
    // clears and repopulates, so the audio thread must never be inside it
    // while that happens.  mStripTapsLive is the gate -- start/stopRecording
    // store false BEFORE touching the container and (start only) true after
    // the last push_back, with a settle on the stop side so any block that
    // already passed the gate has finished.  The strip _arm param is
    // PERSISTENT and is not cleared by transport stop, so the audio thread is
    // genuinely still calling in at the instant the container is mutated.
    //
    // Also a no-op under isNonRealtime(): setFreezePrune keeps the target tab's
    // own strip task in a render's keep-set, so freezing a Vox or Inst tab runs
    // that strip task on every offline block, and without the gate the render's
    // blocks land in an armed take's WAV (see the gate at the top of the body).
    //
    // monoSource: read-only mono input pointer.  Typical usage:
    //   mProcessor->tapDryRecorder (channelId,
    //                               ctx->liveInputSnapshot->getReadPointer (chIdx),
    //                               n)
    void tapDryRecorder (int channelId, const float* monoSource, int numSamples);

    // 2026-05-07 (Batch 9c follow-up): end-of-block UI-meter atomic drain.
    // Promotes per-node / per-bus / per-row peak atomics into the UI-visible
    // mirror atomics that the editor's timer reads.  Called once per block
    // from processBlock (after dispatchBlock returns) so dBFS / VU / per-
    // effect meters get fed from the worker-thread peak writes.
    void drainMeterAtomicsForUI();

    // 2026-05-07 (Batch 10): DSP-load measurement.
    // Called once per block from processBlock after dispatchBlock returns,
    // so mAudioDspLoad reflects the actual cost of running worker tasks via
    // runUntilOrTimeout.  Caller passes the t0 tick captured at the very top
    // of processBlock.
    void measureDspLoadAndOverload (juce::int64 t0Ticks, int numSamples);

    // 2026-05-18 (QA-Ea Task 0b): post-mix recorders + metronome/count-in.
    // Called once per block from processBlock after dispatchBlock returns;
    // feeds master + MIDI recorders and runs the metronome / count-in.
    // Whole-function no-op under isNonRealtime() -- every writer in it is
    // live-only, and it runs outside the task graph so setFreezePrune cannot
    // shield it from an offline render (see the gate at the top of the body).
    // Pre-QA-Ef the 3 hardware-MIDI / master-rec / metronome paths lived
    // only in the deleted serial tail; the extraction landed during QA-Ea
    // Task 0b so the single render path inherits them (Forks #25 close-out).
    // Order preserves the D-5 invariant: MIDI rec -> master rec (the
    // pre-metronome buffer) -> metronome / count-in.  bpm derives from the
    // passed playhead position.
    void applyPostMixRecordAndMetro (juce::AudioBuffer<float>& buffer,
                                     const juce::MidiBuffer& allMidi,
                                     const juce::AudioPlayHead::PositionInfo& pos,
                                     int numSamples);

    bool isRecording() const
    {
        return mMidiRecorder.isRecording() || mMasterRecorder.isRecording()
            || ! mStripRecorders.empty();
    }

    // Message-thread only (start/stopRecording mutate the vector there; the
    // documented audio-thread race is tapDryRecorder's, not this one).
    bool isStripRecording (int channelId) const
    {
        for (const auto& r : mStripRecorders)
            if (r.channelId == channelId) return true;
        return false;
    }

    // QA-Fd: message-thread peek at a Vox page's engine (VoxPage resolves a
    // FOLLOWER channel's pitch DSP through it so align can consume the
    // edited performance).  Page lifecycle is message-thread-owned.
    juce::AudioProcessor* voxEngineAt (int pageIndex) const noexcept
    {
        return (pageIndex >= 0 && pageIndex < kMaxVoxPages)
                 ? mVoxEngines[(size_t) pageIndex] : nullptr;
    }

    // QA-Fe2 De-noise: per-recording noise profiles (raw + corrector domain),
    // keyed by recording base name (file name minus the " - DRY.wav"-style
    // tag).  Project-persisted (<DenoiseProfiles>) so Regenerate De-noise
    // works across sessions.  Message thread only.
    void storeDenoiseProfiles (const juce::String& baseName,
                               const DenoiseProfile& raw, const DenoiseProfile& wet)
    {
        mDenoiseProfiles[baseName] = { raw, wet };
    }
    const std::pair<DenoiseProfile, DenoiseProfile>*
        findDenoiseProfiles (const juce::String& baseName) const
    {
        auto it = mDenoiseProfiles.find (baseName);
        return it != mDenoiseProfiles.end() ? &it->second : nullptr;
    }
    void renameDenoiseProfiles (const juce::String& oldBase, const juce::String& newBase)
    {
        auto it = mDenoiseProfiles.find (oldBase);
        if (it == mDenoiseProfiles.end()) return;
        mDenoiseProfiles[newBase] = it->second;
        mDenoiseProfiles.erase (oldBase);
    }

private:
    AudioFileRecorder              mMasterRecorder;        // master-output fallback
    AudioFileRecorder              mCaptureRecorder;       // TS7 §3.5 version capture
    std::vector<StripRecorder>     mStripRecorders;        // per-armed-strip WAVs
    // Audio-thread entry gate for mStripRecorders (see tapDryRecorder above).
    std::atomic<bool>              mStripTapsLive { false };
    // THREAD SAFETY: the same entry gate for the master tap in
    // applyPostMixRecordAndMetro.  AudioFileRecorder::stopRecording destroys the
    // ThreadedWriter immediately after clearing its own flag, so isRecording()
    // alone leaves a window where a block already past that test writes into a
    // freed writer.  stopRecording lowers this first and settles, which no
    // recorder-internal flag can do for us.
    std::atomic<bool>              mMasterTapLive { false };
    // THREAD SAFETY: the same entry gate for the version-capture tap, which has
    // the identical destroy-under-an-in-flight-block hazard.  Its own flag
    // rather than mMasterTapLive: capture and a user recording run at the same
    // time, so one stop must not close the other's tap.
    std::atomic<bool>              mCaptureTapLive { false };
    // Message-thread only (written in startRecording, drained by stopRecording
    // into RecordResult::failedStrips): armed strips whose writer failed to
    // open, so they never entered mStripRecorders.
    std::vector<std::pair<int, juce::String>> mFailedStripArms;
    // Message-thread dedupe for the unreadable-clip report: the player rebuild
    // runs on every arrangement edit, and one broken clip must report once per
    // session, not once per edit.
    juce::StringArray mReportedUnreadableClips;
    std::atomic<RecordMode>        mRecordMode { RecordMode::Audio };
    double                         mRecordStartBeat { 0.0 };
    // QA-Ea Task 0c (FL pre-roll record): count-in samples accumulated
    // during the current Record session.  applyPostMixRecordAndMetro
    // fetch_adds numSamples while live && isRecording() && countInActive --
    // the live term matters because the WAV writer is gated the same way, and
    // commitRecordingResult subtracts this counter from the file length, so a
    // counter that grew during a render the file never saw discards the take;
    // startRecording zeros it; stopRecording exchanges it into
    // RecordResult::preRollSamples.  One global counter applies to master
    // AND every strip block created by the session (Task 0c strip-recorder
    // scope, plan spec line 120).
    std::atomic<juce::int64>       mPreRollSamples { 0 };
public:

private:
    // ── APVTS layout builder ──────────────────────────────────────────────
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ── Per-page engine processors (message thread sets, audio thread reads) ──
    juce::SpinLock                                         mLayerEngineLock;
    std::array<juce::AudioProcessor*, kMaxLayerPages>      mLayerEngines {};
    juce::SpinLock                                         mPluginEngineLock;
    std::array<juce::AudioProcessor*, kMaxPluginPages>     mPluginEngines {};
    juce::SpinLock                                         mBassEngineLock;
    std::array<juce::AudioProcessor*, kMaxBassPages>       mBassEngines {};

    // D1.2 (2026-04-24): per-drum-page engine processors (dynamic-drum model).
    // Each entry is a NON-OWNING pointer to one rig-owned engine instance
    // (EngineTab::engine); the DrumPage is a view that caches the same pointer,
    // so closing the page must never destroy what this array points at.
    // 2026-04-25: legacy mDrumsEngine + mDrumsEngineLock + mDrumsEngineBuf +
    // mDrumSlotBufs removed (BaySickDrumsProcessor deleted).
    juce::SpinLock                                         mDrumEngineLock;
    std::array<juce::AudioProcessor*, kMaxDrumPages>       mDrumEngines {};
    // Per-drum play pitch: pre-resolved pointer to the lazily-registered
    // mixer_drum_{N}_playNote raw value -- the note a kit trigger fires this
    // drum at.  Published with a release store on the message thread at
    // registerDrumEngine (the param exists by then); audio thread
    // acquire-loads per trigger.  null = strip not registered yet.
    std::array<std::atomic<std::atomic<float>*>, kMaxDrumPages> mDrumPlayNotePtr {};
    // Fast-path bypass - true only when at least one DrumPage tab has registered
    // an engine.  Audio thread checks this before doing any D1.2 work.
    std::atomic<bool>                                      mAnyDrumPageActive { false };

    // G-3 (2026-04-28): per-clip-page engine processors (dynamic-clips model).
    // Index = audio-row index (1:1 with mixer_audio_<row>).  Engine output is
    // mixed into that row's audio insert during processBlock so it shares the
    // same EffectRack / EQ / fader as the arrangement-playback audio.
    juce::SpinLock                                         mClipEngineLock;
    std::array<juce::AudioProcessor*, kMaxClipPages>       mClipEngines {};
    // Fast-path bypass - set true the moment ANY Clips tab registers an engine,
    // false when none remain.  Avoids the per-block iteration cost on projects
    // that don't use clips.  Same pattern as mAnyDrumPageActive.
    std::atomic<bool>                                      mAnyClipPageActive { false };

    // G-4 (2026-04-28): per-Vox / per-Inst-page engine processors.  Same
    // pattern as Clips - engine output routes through the existing Vox / Inst
    // InsertNode (created by the Mixer page's "Add Vox/Inst Strip" flow).
    juce::SpinLock                                         mVoxEngineLock;
    std::array<juce::AudioProcessor*, kMaxVoxPages>        mVoxEngines {};
    std::atomic<bool>                                      mAnyVoxPageActive { false };
    juce::SpinLock                                         mInstEngineLock;
    std::array<juce::AudioProcessor*, kMaxInstPages>       mInstEngines {};
    std::atomic<bool>                                      mAnyInstPageActive { false };

    // ── QA-G3Smoke Swing (SW-1..SW-6): cached raw param atomics ─────────────
    // Registered eagerly at startup (ensureSwingParams); the scheduler reads
    // through these pointers -- no per-block APVTS hash lookups.  Message
    // thread writes them once at registration; audio thread loads relaxed.
    // Vox is excluded (no vox MIDI); clip rolls ride the global at full mix
    // (no per-page params) -- both Jeff 2026-07-23.
    std::atomic<float>* mSwingGlobal { nullptr };
    std::atomic<float>* mSwingMixLayer[kMaxLayerPages] {};
    std::atomic<float>* mSwingTruncLayer[kMaxLayerPages] {};
    std::atomic<float>* mSwingMixBass[kMaxBassPages] {};
    std::atomic<float>* mSwingTruncBass[kMaxBassPages] {};
    std::atomic<float>* mSwingMixDrum[kMaxDrumPages] {};
    std::atomic<float>* mSwingTruncDrum[kMaxDrumPages] {};
    std::atomic<float>* mSwingMixInst[kMaxInstPages] {};
    std::atomic<float>* mSwingTruncInst[kMaxInstPages] {};
    // QA-ModelShell TS6 (BLU-447): per-plugin-tab swing, same shape as Inst.
    std::atomic<float>* mSwingMixPlugin[kMaxPluginPages] {};
    std::atomic<float>* mSwingTruncPlugin[kMaxPluginPages] {};
    std::atomic<float>* mSwingMixRusty { nullptr };
    std::atomic<float>* mSwingTruncRusty { nullptr };


    // J-5 (2026-05-03): BaySickRustyDrums singleton engine.  Only one
    // instance per project.  Owned here so PluginProcessor can orchestrate
    // strip lifecycle on kit load/unload.  The class is forward-declared so
    // PluginProcessor.h doesn't pull sfizz headers into every TU.
    // QA-DispatcherAffinity Task 4 (2026-05-29): the mRustyDrumsEngineLock
    // SpinLock that previously guarded engine pointer access was removed
    // as part of the Sub-K Serial Fallback retirement.  Lifecycle safety
    // is now provided exclusively by the mProjectLoadInProgress shield
    // raised at destroyBaySickRustyDrums + loadBaySickRustyDrumsKit (the
    // shield bails the audio thread at processBlock top for the whole
    // mutation window, so engine pointer reads on the audio path are
    // guaranteed stable for the block).
    std::unique_ptr<class BaySickRustyDrumsProcessor>      mRustyDrumsEngine;
    std::atomic<bool>                                      mRustyDrumsActive { false };
    juce::MidiBuffer                                       mRustyDrumsMidi;    // J-7a: per-block MIDI feed

    // Audio-thread only.  Per-block MIDI feeds, promoted out of processBlock's
    // stack so the render callback stops heap-allocating: MidiBuffer::clear()
    // is a clearQuick that RETAINS the backing store, whereas the old stack
    // objects threw their capacity away at scope exit and re-malloc'd on the
    // next block (222 of them, one malloc+free each for every buffer that took
    // an event).  Cleared at the top of processBlock, never by the consumers.
    //
    // Thread-safety invariant: this is only sound because processBlock is never
    // re-entered concurrently -- beginOfflineRender wins the one-render
    // compare-exchange and suspends + settles before the offline loop becomes
    // the sole caller.  mRustyDrumsMidi above already relies on the same
    // invariant.
    juce::MidiBuffer                                       mAllMidi;
    std::array<juce::MidiBuffer, kMaxLayerPages>           mLayerPageMidi;
    std::array<juce::MidiBuffer, kMaxBassPages>            mBassPageMidi;
    std::array<juce::MidiBuffer, kMaxDrumPages>            mDrumPageMidi;
    std::array<juce::MidiBuffer, kMaxClipPages>            mClipPageMidi;
    std::array<juce::MidiBuffer, kMaxVoxPages>             mVoxPageMidi;
    std::array<juce::MidiBuffer, kMaxInstPages>            mInstPageMidi;
    std::array<juce::MidiBuffer, kMaxPluginPages>          mPluginPageMidi;

    // K-2 (2026-05-05): BaySickGuitars per-instance engines.  Up to kMaxInstPages
    // total instances; one engine per Inst page whose source = BaySickGuitars.
    // Lock array (one SpinLock per slot) avoids cross-instance contention when
    // one tab's kit-load doesn't need to stall another tab's audio thread.
    std::array<juce::SpinLock, kMaxInstPages>                                       mGuitarsEngineLock;
    std::array<std::unique_ptr<class BaySickGuitarsProcessor>, kMaxInstPages>       mGuitarsEngine;
    std::array<std::atomic<bool>, kMaxInstPages>                                    mGuitarsActive {}; // value-init → all false

    // L-2 (2026-05-05): BaySickBasses per-instance engines.  Same pattern as
    // Guitars above; the two source modes share the kMaxInstPages cap (one
    // Inst slot is in exactly one mode at a time, but different tabs can
    // hold different modes).
    std::array<juce::SpinLock, kMaxInstPages>                                       mBassesEngineLock;
    std::array<std::unique_ptr<class BaySickBassesProcessor>, kMaxInstPages>        mBassesEngine;
    std::array<std::atomic<bool>, kMaxInstPages>                                    mBassesActive {};

    // I-16 G-9 (2026-05-03): per-page FilePlay flag.  Pre-scan in processBlock
    // sets these for pages whose linked recorded clip overlaps the current
    // playhead window.  Engine loop skips flagged pages (the audio-clip
    // rendering loop will drive their engine via the FilePlay branch instead).
    std::array<bool, kMaxVoxPages>                         mVoxFilePlayActive {};
    std::array<bool, kMaxInstPages>                        mInstFilePlayActive {};

    // 2026-05-06 Option A: per-tab idle-block counter for sfizz Inst tabs +
    // Rusty.  Increments each block where the tab has no MIDI activity AND
    // sfizz reports 0 active voices AND no audition pending.  When the
    // counter reaches kIdleSuspendBlocks, the tab's entire chain (sfizz +
    // Pedals + NAMIR + insert rack + EQ) is skipped on this block.  Wakes
    // immediately on the next block where any of those gates fail (MIDI,
    // voice activity, audition).  The counters themselves are audio-thread-only
    // state - no atomics.
    //
    // The hold is a real TIME, not a block count.  The old fixed 9 blocks was
    // 26 ms at 128/44.1k and 836 ms at 4096 - a 32x spread across the supported
    // range, so the same session suspended at wildly different points depending
    // only on the buffer setting, and the lower the latency the sooner it fired.
    // kIdleSuspendSeconds reproduces the pre-fix 512/44.1k calibration (9 blocks
    // = 104 ms) at EVERY rate x block pair; kIdleSuspendBlocks is recomputed
    // from it in prepareToPlay against the live sample rate and nominal block
    // size.  Deliberately behavior-neutral at the reference config.  The suspend
    // itself no longer guillotines a ringing tail: the clear sites fade in and
    // out over a real duration (IdleSuspendFade, used by InstStripTask and
    // RustyDrumsProducerTask), which is why this hold only has to decide WHEN
    // the chain goes quiet, not how abruptly.
    static constexpr double kIdleSuspendSeconds = 0.1;
    // THREAD SAFETY / OWNERSHIP: written only by prepareToPlay (device stopped,
    // or offline render suspended + settled), read on the audio thread by
    // InstStripTask and RustyDrumsProducerTask.  Static because those two name
    // it class-qualified and the app runs exactly one processor instance; the
    // implicit atomic->int conversion is what keeps their
    // `counter >= kIdleSuspendBlocks` tests compiling unchanged.  Seeded so
    // that nothing can suspend before the first prepare establishes a rate.
    static inline std::atomic<int> kIdleSuspendBlocks
        { std::numeric_limits<int>::max() };
    std::array<int, kMaxInstPages> mInstIdleBlocks {};
    int mRustyIdleBlocks { 0 };
    // TS7 §6.9: every loaded kit strip is frozen, so the producer can skip the
    // whole sfizz render.  Set on the message thread by freezeTab/unfreezeTab,
    // read by the producer every block.
    std::atomic<bool> mRustyKitFrozen { false };
    // §6.8: which pattern's kit renders are currently published to the 13
    // strips (-1 = none).  The producer reads it so the frozen-kit skip works
    // in PATTERN mode too -- gating the skip on songMode alone left pattern
    // playback paying full sfizz synthesis PLUS 13 streamer reads.  Written on
    // the message thread by setRustyFrozenPatternSourcesImpl / retraction.
    std::atomic<int> mRustyFrozenPatternIndex { -1 };
    // 2026-05-06: bus-level idle gate REVERTED - the per-block magnitude
    // check added overhead during active playback (where buses are not
    // silent) without offsetting savings, net-negative on busy sessions.
    // Per-tab Option A above remains the meaningful DSP win.

    // R3 (2026-04-23): Live-input audio capture for Vox / Inst strips.
    // mLiveInputSnapshot - non-cleared copy of the input channels before
    // buffer.clear(), populated each block when numInputs > 0.
    // mLiveInputSlotBuf - reused stereo scratch for routing one channel of
    // the snapshot through a single Vox / Inst InsertNode at a time.
    juce::AudioBuffer<float> mLiveInputSnapshot;
    juce::AudioBuffer<float> mLiveInputSlotBuf;

    // 2026-04-24 deferred rack-state replay (see applyPendingRackStates doc above).
    juce::ValueTree mPendingProjectRackState;

    // §P4.3 B7 (2026-04-22): per-page pre-rack EQ pointer arrays + SpinLocks
    // deleted.  Pre-rack EQs now live on BaySickGraph InsertNode / BusNode
    // preEq members - no more page-owned DSPs held here as non-owning ptrs.

    // ── State ─────────────────────────────────────────────────────────────
    std::atomic<int> mDeviceOutputLatency { 0 };
    double          mSampleRate  { 44100.0 };
    int             mBlockSize   { 512 };
    PatternManager* mPatternManager { nullptr };

public:
    // Effective loop length - written by the message thread (10 Hz GlobalTransportBar timer),
    // read on the audio thread in processBlock. Avoids a data race on PatternManager vectors.
    std::atomic<double> mCachedPatternLoopBeats { 4.0 };   // 1 bar default; extended by content

    // Song mode - written by UI thread, read on audio thread.
    // true = play arrangement blocks linearly; false = loop current pattern.
    // Smoke round 3 (Jeff): the setter is now the automation-baseline hook --
    // song ENTRY snapshots every automation-targeted param's current value,
    // EXIT restores them, so flipping back to pattern mode returns driven
    // knobs to their pre-automation positions.  Message thread only.
    std::atomic<bool>   mSongMode { false };
    void setSongMode (bool b);
    bool isSongMode() const  { return mSongMode.load(std::memory_order_relaxed); }

private:
    // Smoke round 3: {paramId, normalized value} captured at song entry.
    std::vector<std::pair<juce::String, float>> mAutomationBaseline;

public:

    // 2026-05-06: project-load barrier.  Set true at the start of project
    // open / restoreBackup / closeAllDynamicTabs; the audio thread's
    // processBlock checks this at entry and bails (clears buffer + returns)
    // when set, so teardown of engines + InstPages is safe from concurrent
    // audio-thread access.  Without this, project re-open while audio is
    // running can crash inside a half-destroyed sub-engine (e.g. NAMIR /
    // MicPlacement IIR filter dereferences after the processor was freed).
    //
    // QA-Ef (2026-05-22): the shield now spans the WHOLE load -- teardown AND
    // the tab/engine REBUILD -- via deserializeProject raising it for its full
    // body and closeAllDynamicTabs being nest-aware (leaves it raised when an
    // outer load owns it).  Previously only the teardown half was shielded, so
    // registerTask during rebuild raced the audio thread's render-task-list
    // iteration (use-after-free in RenderGraphDispatcher::dispatchBlock on a
    // save-file load).  Pairs with the dispatcher pre-sizing its task lists.
    //
    // THREAD SAFETY: the shield is raised ONLY from the message thread.  The
    // callers nest it with a plain `const bool shieldWasUp = ...` save/restore
    // rather than a refcount, which is correct only because of that: two
    // threads raising it concurrently would let the inner restore lower it
    // while the outer caller still needs it up.
    std::atomic<bool> mProjectLoadInProgress { false };
    void setProjectLoadInProgress (bool b) noexcept
        { mProjectLoadInProgress.store (b, std::memory_order_release); }
    bool isProjectLoadInProgress() const noexcept
        { return mProjectLoadInProgress.load (std::memory_order_acquire); }

    // THREAD SAFETY: monotonic block count published by the audio thread with
    // release semantics at the very top of processBlock -- before the shield's
    // early-out, so a muted block still acknowledges.  settleAudioThread reads
    // it with acquire; two advances prove the block that may have been in
    // flight when the shield went up has returned.
    alignas(64) std::atomic<std::uint64_t> mAudioBlockCounter { 0 };

    // THREAD SAFETY: true only while a device is actually calling processBlock.
    // Raised in prepareToPlay, and ONLY when the caller is not the offline
    // reconfigure thread (see mOfflineReconfigureThread) -- a render's own
    // re-prepares must not claim a device that was never opened.  Cleared in
    // releaseResources and nowhere else: endOfflineRender READS this flag to
    // re-derive the retirement-consumer idle assertion, it never writes it.
    // Without the CLEAR half, a device that ran and then stopped (user picks
    // "no device", driver drop) still looks live, so every settle waits out its
    // whole timeout for an acknowledgement that can never arrive.
    std::atomic<bool> mAudioDevicePrepared { false };

    // Message thread only.  Blocks until the audio thread acknowledges (see
    // mAudioBlockCounter), so a teardown may free what processBlock reads.
    // Bounded by a timeout derived from the LIVE device buffer: one block is
    // 23 ms at 1024 samples / 44.1 kHz and 46 ms at 2048, so no single fixed
    // duration is right at every buffer size.
    void settleAudioThread() noexcept;

    // P4: current project folder (for resolving relative audioFilePath strings).
    // Guarded by mProjectFolderLock since both message + audio threads read it.
    juce::CriticalSection mProjectFolderLock;
    juce::File            mCurrentProjectFolder;

    // QA-Fe2: De-noise profile store (see accessors above).  Message thread.
    std::map<juce::String, std::pair<DenoiseProfile, DenoiseProfile>> mDenoiseProfiles;

    // ── Metronome DSP ─────────────────────────────────────────────────────
    struct MetroDSP {
        enum SoundType { Sine = 0, Click, Wood, Bell };

        std::atomic<bool>  enabled      { false };
        std::atomic<float> volume       { 0.7f };
        std::atomic<int>   soundType    { (int)Sine };
        // Count-in (independent of transport):
        std::atomic<bool>   countInActive { false };
        std::atomic<double> countInBpm    { 120.0 };
        // QA-G Task 6: signature at the record position (song = marker map,
        // pattern = pattern's effective TS), captured by the record path so
        // count-in clicks run in DENOMINATOR units with the right accent.
        std::atomic<int>    countInNum    { 4 };
        std::atomic<int>    countInDen    { 4 };
        // Audio-thread-only state (no atomic needed):
        double lastBeatFloor    { -99999.0 };
        int    clickSampLeft    { 0 };
        bool   clickIsAccent    { false };
        float  clickPhase       { 0.f };
        // Count-in audio-thread state:
        double countInPhase     { 0.0 };   // beats elapsed since count-in started
        bool   countInWasActive { false }; // edge-detect to reset phase on start
        int    countInBeatsFired{ 0 };     // 2026-04-26 (D-5 fix): tracks how many
                                           // count-in clicks have fired so the loop
                                           // doesn't double-fire beat 1 after its
                                           // initial trigger (fires in-loop once the
                                           // QA-Fe2 PDC deferral elapses).
        // QA-Fe2 PDC (2026-07-16): the compensated mix reaches this buffer
        // totalLatencySamples late, so clicks defer by the same amount or
        // they LEAD the music (default-audible now that De-reverb ships
        // active).  countInDelaySamp defers the whole count-in once at its
        // rising edge; transportWasPlaying edge-inits the transport click
        // grid so the deferral can't fire a stale catch-up click at play.
        int    countInDelaySamp { 0 };
        bool   transportWasPlaying { false };
    } mMetro;

private:

    // Step-change tracking for basic sequence triggering (standalone)
    int mLastDrumStep { -1 };

    // ── Piano roll note scheduling (standalone) ───────────────────────────
    struct PRPendingOff {
        double beatOff;   // absolute beat when note-off should fire
        int    midiNote;  // note to turn off
        int    target;    // 0..7 = layer page; kBassPRTarget+0..3 (8..11) = bass page index
    };
    std::vector<PRPendingOff> mPRPendingOffs;
    // Audio-thread scratch for the per-block pending-off filter.  The filter
    // used to build a fresh local vector and move-assign it over
    // mPRPendingOffs, which freed the old buffer and reset capacity to zero --
    // so every block re-grew from scratch inside the render callback.  clear()
    // + swap() never deallocate, so both vectors keep their capacity for the
    // session.  Reserved in prepareToPlay.
    std::vector<PRPendingOff> mPRKeepScratch;
    // Audio-thread only.  True when the PREVIOUS block's scheduler ran the
    // straddle path (loop seam handled sample-exactly inside that block).
    // The QA-Ee loop-wrap flush exists for the wrap-on-a-block-boundary case
    // the straddle test misses; after a straddle block the flush is redundant
    // -- every pre-wrap off at/past loopEnd was already emitted at the wrap
    // sample, so any off it would now find at loopEnd belongs to the seam-
    // RESTARTED note (a note spanning the full pattern) and flushing it kills
    // that note one block into the new pass (staccato-on-every-repeat).
    bool mPRLastBlockStraddled { false };
    // QA-Ed: the float `mPRLastBeatEnd` jump-heuristic + the kWrapSlop / jumped
    // / windowStart band-aid are removed; the int-sample clock + the exact
    // loop-seam windows (mLoopStartBeats + mSeekDiscontinuity above) replace it.

    // ── Internal helpers ──────────────────────────────────────────────────
    void updateDrumMixLevels();

    // One fire = one note-on / clip-start whose source has a choke group set.
    // Hoisted out of applyChokeGroupDispatch so the list can live as a
    // reserved member: the old local `std::vector` + reserve(8) was an
    // unconditional malloc/free pair inside every render callback, paid even
    // on projects with zero choke groups configured.
    struct ChokeFire
    {
        // Distinguishes self when iterating peers.
        enum class Src { Synth, Audio };
        Src                   src   { Src::Synth };
        BaySickGraph::InsertKind kind  { BaySickGraph::InsertKind::Layer };   // synth only
        int                   index { -1 };            // synth: insert idx; audio: clip idx
        int                   group { 0 };
        int                   sample { 0 };
    };
    // Audio-thread only; cleared (never resized) per block, reserved in
    // prepareToPlay.  Deliberately NOT a fixed-size array: a dense block can
    // exceed any cap, and a cap would silently drop a choke.
    std::vector<ChokeFire> mChokeFireScratch;

    // ── D3: choke-group dispatch (audio thread) ───────────────────────────
    // Scan synth note-ons + audio clip starts; if a fire's source has
    // chokeGroup > 0, inject allNotesOff into every other synth insert in the
    // same group AND set mutedByChoke=true on every other audio clip in the
    // same group.  Wait-free; reads atomic param pointers cached on each
    // InsertNode.  Audio clip mutedByChoke is also reset to false here for
    // clips not currently in playback range so a fresh playthrough starts
    // un-choked.
    // 2026-04-30 (audit B.6): Vox + Inst page MIDI buffers added so choke
    // groups also dispatch to / from those engines.  Was Layer/Bass/Drum
    // only - Audio/Aux/Vox/Inst chokeGroup APVTS params were registered
    // but the dispatch loop never scanned or injected them.
    void applyChokeGroupDispatch(
        std::array<juce::MidiBuffer, kMaxLayerPages>& layerMidi,
        std::array<juce::MidiBuffer, kMaxBassPages>&  bassMidi,
        std::array<juce::MidiBuffer, kMaxDrumPages>&  drumMidi,
        std::array<juce::MidiBuffer, kMaxVoxPages>&   voxMidi,
        std::array<juce::MidiBuffer, kMaxInstPages>&  instMidi,
        juce::int64 projectStartSamp,
        int         numSamples,
        double      secPerBeat);
    // §P4.3 B7 (2026-04-22): updateDrumsEQ + updateLayerPageEQsFromApvts +
    // updateBassPageEQsFromApvts removed along with their DSP instances.
    // Pre-rack EQs now sync via updateAllPreRackEQsFromApvts below (unified
    // mixer-strip iteration).
    // Session B: generic update helper for any EQ8MsDSP bound to one strip slot's
    // param bank.  Reads all 17 band params x 8 bands x 2 sides and applies via
    // the standard setBand* setters (CPU-guarded internally).  Used for every EQ
    // (bus + insert, pre- and post-rack) whose params were lazily registered via
    // addParamsForTrackEQ / addParamsForTrackPreEQ.
    void updateEQFromCache (EQ8MsDSP* eq, int stripSlot, int bank);
    // Iterates every registered mixer strip (18 buses + the insert families) and
    // calls updateEQFromCache for each InsertNode/BusNode's EQ. No-op when a
    // given slot has no node (index out of range or not yet registered).
    void updateAllPostRackEQsFromApvts();
    void updateAllPreRackEQsFromApvts();   // §P4.3

    // File > New: re-seed the half of every EQ that no parameter backs (main
    // level, phase mode, linear precision, anti-cramping, proportional Q, the
    // A/B spare and its lock).  MESSAGE THREAD, and the caller must already
    // hold the project-load shield -- see the definition.
    void resetEqStatesToDefaults();

    // ── EQ param-pointer cache (QA-SOUNDNESS, 2026-08-07) ────────────────────
    // THREAD SAFETY: the EQ sweep below runs on the AUDIO thread.  It used to
    // reach every band value through apvts.getParameter (prefix + suffix) --
    // a juce::String built per band (a heap allocation inside the render
    // callback) feeding an O(log n) string-compare walk of APVTS's adapterTable,
    // which the message thread concurrently emplaces into every time a mixer
    // strip is lazily registered.  Resolving each band's std::atomic<float>*
    // ONCE at registration removes the allocation and the map walk together:
    // the sweep -- by far the heaviest reader, thousands of lookups per pass --
    // no longer touches adapterTable at all.  (The automation-clip loop in
    // processBlock still resolves its lane by id, so this does not make the map
    // reader-free; it removes the only bulk reader.)
    //
    // OWNERSHIP / LIFETIME: APVTS owns every adapter through a std::unique_ptr
    // held in a std::map, and nothing in this tree ever removes a parameter.
    // std::map has node stability, so a later insert cannot move an existing
    // adapter -- a cached pointer stays valid for the APVTS's whole lifetime.
    //
    // PUBLICATION: the message thread fills a band's slots and RELEASE-stores
    // the Freq slot last; the audio thread ACQUIRE-loads Freq and skips the band
    // while it is still null.  So a reader sees either no band at all or all
    // seventeen pointers -- never a half-published one.  The table is a fixed
    // size allocated once, never grown, so no reader can observe a moved array.
    enum EqBandParamSlot
    {
        eqSlotFreq = 0, eqSlotGain, eqSlotQ, eqSlotType, eqSlotOn, eqSlotSlope,
        eqSlotMute, eqSlotSolo, eqSlotChannel,
        eqSlotDynamic, eqSlotThreshold, eqSlotRatio, eqSlotAttack,
        eqSlotRelease, eqSlotRange, eqSlotUpward, eqSlotScSource,
        eqNumBandParamSlots
    };
    struct EqBandParamPtrs
    {
        std::atomic<std::atomic<float>*> p[eqNumBandParamSlots] {};
    };

    static constexpr int kEqBands         = 8;
    static constexpr int kEqSidesPerBank  = 2;    // mid, side
    static constexpr int kEqBanksPerStrip = 2;    // post-rack, pre-rack
    static constexpr int kEqBankPost      = 0;
    static constexpr int kEqBankPre       = 1;
    // Strip-slot space: the buses first, in kEqBuses order, then the insert
    // families in kEqInsertFamilies order.  Both tables live in the .cpp and
    // static_assert against these counts.
    static constexpr int kEqNumBusSlots    = 18;   // kNumBatch7Buses + Master
    static constexpr int kEqNumInsertSlots =
        kMaxLayerPages + kMaxBassPages + kMaxDrumPages + kMaxAudioRows
        + MixerChannelIds::kMaxAuxStrips   + MixerChannelIds::kMaxVoxStrips
        + MixerChannelIds::kMaxInstStrips  + MixerChannelIds::kMaxRustyStrips
        + MixerChannelIds::kMaxPluginStrips + MixerChannelIds::kMaxDirectStrips;
    static constexpr int kEqNumStripSlots = kEqNumBusSlots + kEqNumInsertSlots;
    static constexpr int kEqCacheSize =
        kEqNumStripSlots * kEqBanksPerStrip * kEqSidesPerBank * kEqBands;

    static int eqStripSlotForPrefix (const juce::String& prefix) noexcept;
    static constexpr int eqCacheIndex (int stripSlot, int bank, int side, int band) noexcept
    {
        return ((stripSlot * kEqBanksPerStrip + bank) * kEqSidesPerBank + side)
                   * kEqBands + band;
    }
    // Message thread only (calls into APVTS's map).  Idempotent.
    void cacheEqParamPointers (const juce::String& prefix);

    std::unique_ptr<EqBandParamPtrs[]> mEqParamCache
        { std::make_unique<EqBandParamPtrs[]> ((size_t) kEqCacheSize) };

    // §P4.3 perf: ValueTree::Listener override.  Marks the EQ-sync dirty flag
    // when an EQ APVTS state property changes.  The next processBlock will run
    // updateAllPost+PreRackEQsFromApvts; subsequent blocks skip until the next
    // change.  Catches param edits from UI (message thread), automation (audio
    // thread), and host-driven setValue calls - all uniformly route through
    // ValueTree::setProperty under the hood.
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier&) override
    {
        const auto pid = tree.getProperty ("id").toString();

        // The sweep the flag arms walks every band of every registered strip,
        // pre and post.  Arming it for ANY param meant a fader drag re-ran that
        // whole sweep at the APVTS flush rate.  Every EQ id carries "_mid_eq" /
        // "_side_eq" (optionally behind "_preeq_"), so this filter misses no EQ
        // edit.  (The per-band juce::String rebuild that made an unfiltered
        // sweep genuinely dangerous on the audio thread is gone -- see the EQ
        // param-pointer cache above -- but the sweep is still pure waste when
        // no EQ moved.)
        if (pid.contains ("_eq"))
            mEQsDirty.store(true, std::memory_order_relaxed);
        if (onAnyStateChange) onAnyStateChange();   // P5: project dirty tracking

        // TS7 §6.5: SWING was a missing freeze invalidator.  It lives in the MAIN
        // APVTS ("swing_*"), not the per-tab engine APVTS the freeze watcher
        // listens to, so moving it changed what an engine played while every
        // freeze still read as current.
        //
        // A STAMP, not a direct markEngineContentChanged call: this listener also
        // fires on the AUDIO thread (automation writes route through
        // ValueTree::setProperty too, per the comment above), and the rig walks a
        // vector and fires view callbacks.  The editor's existing 5 Hz poll reads
        // the stamp on the message thread and acts there.
        // BOTH spellings.  The per-player knobs are "swing_<kind>_<n>_mix" /
        // "_trunc", but the GLOBAL swing knob on the transport bar is
        // "globalSwing" -- it does not share the prefix, so a prefix test alone
        // silently missed the one control that re-times every player at once
        // (found by Jeff asking which of the two this covered).
        if (pid.startsWith ("swing_") || pid == "globalSwing")
            mSwingChangeStamp.fetch_add (1, std::memory_order_relaxed);
    }
    // Default true: first processBlock always syncs (catches initial state load
    // + factory defaults that may differ from DSP construction defaults).
    std::atomic<bool> mEQsDirty { true };
    std::atomic<juce::uint32> mSwingChangeStamp { 0 };   // TS7 §6.5

public:
    juce::uint32 getSwingChangeStamp() const noexcept
        { return mSwingChangeStamp.load (std::memory_order_relaxed); }

    // P5: wired by StandaloneEditor to ProjectManager::markDirty.  Fires on
    // any APVTS change (piggybacks on the existing valueTreePropertyChanged
    // subscription).
    std::function<void()> onAnyStateChange;
private:
    void syncMixerFromPatternManager();

    // Owner-side drive for the deferred-destruction drainers' idle assertion
    // (Engine/RetirementQueue.h, CONSUMER-IDLE CONTRACT).  Message thread only.
    void setRetirementConsumersIdle (bool consumerIsIdle);

    // Lazy registration helpers (called from ensureMixerStripParams)
    // QA-ApvtsAutomation (2026-07-25): the four engine mirror helpers removed
    // with registerParamsForTrack; 2026-04-25: addParamsForBaySickDrums before them.
    void addParamsForTrackEQ     (const juce::String& prefix);
    void addParamsForTrackPreEQ  (const juce::String& prefix);   // §P4.3 pre-rack EQ block
    void addLiveInputParams      (const juce::String& prefix);   // R2: Vox/Inst _inputChannelIdx
    mutable juce::CriticalSection mInputChannelNamesLock;        // R2
    void addParamsForEQBank      (const juce::String& prefix, const juce::String& subPrefix);

    // ── 5F-4a: Mixer-strip lazy APVTS registration ───────────────────────────
    // Classifies which mixer-strip param family to register; the per-kind
    // membership and the reasons for each omission live with the branches in
    // addParamsForMixerStrip.
public:
    enum class MixerStripKind { Master, Bus, Insert };
private:
    void addParamsForMixerStrip(const juce::String& prefix, MixerStripKind kind,
                                 int defaultSendTo);

public:
    // Ensure mixer-strip params exist for the given prefix. Safe to call repeatedly.
    // Returns true if any new params were registered on this call.
    // Thread: message thread only (APVTS::createAndAddParameter is not RT-safe).
    // defaultSendTo is the main-out destination channel id (see MixerChannelIds).
    bool ensureMixerStripParams(const juce::String& prefix, MixerStripKind kind,
                                 int defaultSendTo);

    // QA-ModelShell TS3 (2026-07-27): fired with the strip prefix the FIRST time
    // its params are created.  StandaloneEditor uses it to register the strip's
    // automation lanes at materialization, so they exist whether or not the
    // mixer window (or the EQ display) has ever been built -- which
    // destroy-on-close makes the normal case.  Message thread, like the
    // registration it triggers.
    std::function<void (const juce::String& prefix)> onMixerStripParamsCreated;

    // Bulk-register master + every fixed bus strip's params (idempotent).
    // Must run before BaySickGraph::rebindBusApvts, which caches raw pointers to
    // the params created here.
    void ensureMixerBusAndMasterParams();
    // QA-G3Smoke Swing (SW-6): eager global + per-player swing param
    // registration + raw-atomic caching (see the .cpp note for family scope).
    void ensureSwingParams();
    // QA-G3Smoke SW-3: bundle of UI accessors for one player's swing params
    // (mix + truncate) -- pages hand these to the engine editors' title-bar
    // knobs, which have no main-APVTS handle of their own.
    struct SwingKnobBinding
    {
        std::function<float()>     getMix;
        std::function<void(float)> setMix;
        std::function<bool()>      getTrunc;
        std::function<void(bool)>  setTrunc;
    };
    SwingKnobBinding makeSwingKnobBinding (const juce::String& mixId,
                                           const juce::String& truncId);

private:

    // trackId → list of registered param IDs (for cleanup tracking)
    std::map<juce::String, juce::StringArray> mRegisteredTrackParams;

    // Track which mixer-strip prefixes have been registered (prevents double-register)
    std::set<juce::String> mRegisteredMixerStrips;

    // C.3 (2026-04-30): hardware MIDI input bridge.  See public getter +
    // setLiveMidiTarget for the contract.
    juce::MidiMessageCollector mLiveMidiCollector;
    std::atomic<bool>          mLiveMidiReady { false };   // see isLiveMidiReady
    std::atomic<int>           mLiveMidiTargetKind  { -1 };   // -1 = unset
    std::atomic<int>           mLiveMidiTargetIndex { 0  };

    // Live-note monitor mask (see getLiveHeldNotes).  Audio thread sets/clears
    // bits as hardware note-on/offs drain; UI reads the public snapshot.
    std::atomic<uint64_t>      mLiveHeldNotesLo { 0 };
    std::atomic<uint64_t>      mLiveHeldNotesHi { 0 };
    void updateLiveHeldNote (int note, bool on) noexcept
    {
        if (note < 0 || note > 127) return;
        auto& word = (note < 64) ? mLiveHeldNotesLo : mLiveHeldNotesHi;
        const uint64_t bit = 1ull << (note & 63);
        if (on) word.fetch_or  (bit,   std::memory_order_relaxed);
        else    word.fetch_and (~bit,  std::memory_order_relaxed);
    }
    void clearLiveHeldNotes() noexcept
    {
        mLiveHeldNotesLo.store (0, std::memory_order_relaxed);
        mLiveHeldNotesHi.store (0, std::memory_order_relaxed);
    }

    // I-3b (2026-05-02): MIDI Learn registry + per-device event queue.
    // The queue is a MIDI-thread -> audio-thread bridge that preserves source
    // device names (the live MIDI collector loses them).  StandaloneApp
    // pushes into it from MidiInputCallback::handleIncomingMidiMessage; the
    // audio thread drains it inside processBlock and feeds events through the
    // registry's dispatch.  Mutators on mMidiLearn run on the message thread.
    MidiLearnRegistry   mMidiLearn;
    MidiLearnEventQueue mMidiLearnQueue;

    // QA-L-Fix (2026-07-19): per-drum kit trigger bindings.  Dispatched from the
    // LIVE-MIDI loop (not the learn queue) because that path preserves
    // intra-block sample position -- drum hits are the most timing-sensitive
    // events in the app, and the learn queue applies at block rate by design.
    DrumTriggerMap mDrumTriggers;

    // Audio-thread only.  CC triggers have no note-off: a momentary pad may send
    // value 0 on release, but plenty of gear sends nothing at all.  Each fired CC
    // trigger arms a countdown here; the drum is released on CC-0 or when the
    // countdown expires, whichever lands first, so a non-releasing controller can
    // never strand a voice (owner call 2026-07-19: "safest").
    struct CcTriggerHold
    {
        bool    active         { false };
        int     note           { -1 };
        int64_t samplesLeft    { 0 };
    };
    std::array<CcTriggerHold, kMaxDrumPages> mCcTriggerHolds {};
    // Audio-thread only.  Fast-path bypass for the per-block hold tick; set
    // when a hold is armed, recomputed from the survivors on each pass.
    bool mAnyCcHoldActive { false };

    // Audio-thread only.  Pitch a NOTE trigger fired at, per drum; -1 = not
    // held.  Release uses this rather than the live play-note param so
    // re-assigning a drum's play note mid-hold can't strand the old voice.
    // No timeout needed here -- the note-off comes from the hardware.
    std::array<int, kMaxDrumPages> mNoteTriggerHeld;

    // Audio thread.  Fire/release any drum whose trigger binding matches `msg`.
    // `liveTargetKind` is the focused-engine kind (0 = Drum Kit) -- note
    // triggers are gated on it per D-8, CC triggers fire regardless.
    void dispatchDrumTriggers (const juce::MidiMessage& msg,
                               int samplePosition,
                               int liveTargetKind,
                               std::array<juce::MidiBuffer, kMaxDrumPages>& drumPageMidi) noexcept;
    void tickCcTriggerHolds   (int numSamples,
                               std::array<juce::MidiBuffer, kMaxDrumPages>& drumPageMidi) noexcept;
    // 1 s: percussion gates are far shorter than this (a drum's tail lives in its
    // release/sample, not the gate), so it never truncates a real hit, while a
    // stranded voice self-clears fast enough not to drone.
    static constexpr double kCcTriggerMaxHoldSeconds = 1.0;

    // ── Multi-threaded render engine (Phase 1 scaffolding, 2026-05-06) ───────
    // Lifetime = plugin lifetime. Pool spawns its workers in its constructor
    // and joins them in its destructor; prepareToPlay only resizes the arena
    // and clears queues, never touches the threads themselves.
    //
    // Declaration order is critical: dispatcher takes refs to pool + arena,
    // and C++ guarantees member init order matches declaration order.
    static int computeRenderWorkerCount() noexcept;

    BaySickThreadPool        mRenderPool       { computeRenderWorkerCount() };
    ChannelBufferArena    mRenderArena;
    RenderGraphDispatcher mRenderDispatcher { mRenderPool, mRenderArena };

    // Batch 3 (2026-05-06): one EngineInsertTask per active Layer/Bass/Drum.
    // Created in lockstep with the engine via registerXxxEngine; destroyed
    // in unregisterXxxEngine. The dispatcher holds non-owning pointers; we
    // own the storage so destruction is well-defined.
    std::array<std::unique_ptr<EngineInsertTask>, kMaxLayerPages> mLayerRenderTasks;
    std::array<std::unique_ptr<EngineInsertTask>, kMaxPluginPages> mPluginRenderTasks;   // TS6
    // QA-TrueLevel SC-10: Direct to Master model + tasks, indexed by strip slot.
    std::array<std::unique_ptr<DirectStrip>,    MixerChannelIds::kMaxDirectStrips> mDirectStrips;
    std::array<std::unique_ptr<DirectFileTask>, MixerChannelIds::kMaxDirectStrips> mDirectTasks;
    bool openDirectStripTask  (int idx, const juce::File& file);
    void closeDirectStripTask (int idx);
    void ensureDirectStripInfra (int idx);
    void tearDownDirectStrip (int idx);
    void serializeDirectStrips   (juce::XmlElement& root) const;
    void deserializeDirectStrips (const juce::XmlElement& root);
    std::array<std::unique_ptr<EngineInsertTask>, kMaxBassPages>  mBassRenderTasks;
    std::array<std::unique_ptr<EngineInsertTask>, kMaxDrumPages>  mDrumRenderTasks;

    // Batch 4 (2026-05-06): Vox + Inst live-input strip task wrappers.
    std::array<std::unique_ptr<VoxStripTask>,  kMaxVoxPages>  mVoxRenderTasks;
    std::array<std::unique_ptr<InstStripTask>, kMaxInstPages> mInstRenderTasks;

    // Batch 5 (2026-05-06): Clip page + Audio insert task wrappers.
    // QA-0 (2026-05-07): per-row composite owns BOTH arrangement-clip
    // decode + clip-engine MIDI trigger.  mClipRenderTasks is gone --
    // registerClipEngine sets the composite's mClipEngine field on
    // mAudioRenderTasks[pageIdx] instead of registering a separate task.
    std::array<std::unique_ptr<CompositeAudioInsertTask>, kMaxAudioRows> mAudioRenderTasks;

    // Batch 6 (2026-05-06): BaySickRustyDrums producer + per-strip inserts.
    // Producer owned per-engine (only one engine instance ever exists).
    // Inserts are 1:1 with the engine's discovered strip count (up to 13);
    // arrays sized to the max + indexed by stripIndex.
    std::unique_ptr<RustyDrumsProducerTask>                                       mRustyProducerTask;
    std::array<std::unique_ptr<RustyInsertTask>, MixerChannelIds::kMaxRustyStrips> mRustyRenderTasks;

    // Batch 7 (2026-05-06): passive accumulator strip tasks.
    // Aux: created lazily via ensureAuxInsert; up to kMaxAuxStrips (18).
    // Bus: always-on buses registered idempotently in prepareToPlay.
    static constexpr int kNumBatch7Buses = 17;   // QA-SOUNDNESS: +Drums Bus 2
    std::array<std::unique_ptr<PassiveStripTask>, MixerChannelIds::kMaxAuxStrips> mAuxRenderTasks;
    std::array<std::unique_ptr<PassiveStripTask>, kNumBatch7Buses>                mBusRenderTasks;

    // Batch 8 (2026-05-06): terminal MasterTask.  Always-on, registered
    // idempotently in prepareToPlay alongside the bus tasks.
    std::unique_ptr<MasterTask> mMasterRenderTask;

    // QA-ModelShell TS2: offline-render restore set (valid only between
    // begin/endOfflineRender).
    double               mOfflinePrevSr   { 0.0 };
    int                  mOfflinePrevBlk  { 0 };
    bool                 mOfflinePrevSong { true };
    juce::AudioPlayHead* mOfflinePrevHead { nullptr };
    bool                 mOfflinePrevShield { false };
    // The TempoMap rate the LIVE session was published at, restored by
    // endOfflineRender (Jeff's 44.1-export-on-a-48k-device bug, 2026-08-22).
    double               mOfflinePrevMapSr { 0.0 };
    // ONE render at a time: export/measure drive begin/end from a background
    // thread while freeze renders drive it on the message thread -- two
    // interleaved suspend/restore sequences corrupt both.  Owned by
    // beginOfflineRender (compare-exchange) / endOfflineRender (clear).
    std::atomic<bool>    mOfflineRenderActive { false };
    std::atomic<float>*  mPanLawParam { nullptr };   // master_pan_law, published per block (DSP/PanLaw.h)
    // Set to the calling thread around the two prepareToPlay calls the offline
    // path makes itself, so prepareToPlay can tell a device open from a render
    // reconfigure and only the former moves mAudioDevicePrepared.  Without that
    // the flag stops being a statement about the device: getSampleRate() keeps
    // reporting the last negotiated rate after releaseResources, so a render
    // started with no device open restores its config and re-raises the flag,
    // and every later settle then waits out its full timeout for an
    // acknowledgement nothing can send.  A thread id rather than a bool because
    // a real device open arriving mid-render runs on the device thread and must
    // still be able to raise the flag.
    std::atomic<juce::Thread::ThreadID> mOfflineReconfigureThread { nullptr };

    // QA-ModelShell TS6: declared immediately BEFORE the rig so it is destroyed
    // AFTER it -- hosted plugin instances live in the rig (instruments) and in
    // the racks (effects), and they must not outlive the format manager that
    // created them.
    std::unique_ptr<Hosting::PluginManager> mPluginManager;

    // QA-ModelShell TS1: declared LAST so it is destroyed FIRST -- the rig's
    // teardown unregisters engines through the dispatcher + task arrays
    // above, which must still be alive when that runs.
    std::unique_ptr<EngineRig> mEngineRig;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BaySickDAWProcessor)
};
