#pragma once
#include <JuceHeader.h>
#include <vector>
#include <map>
#include <set>
#include "VibesynthConstants.h"

// TS7 §6.3: a frozen tab streams its cached file instead of rendering its engine.
// Included rather than forward-declared: EngineTab holds a unique_ptr to it, and
// destroying that needs the complete type in every translation unit that
// destroys an EngineTab -- which is more than one.
#include "DSP/AudioClipStreamer.h"

class VibeSynthProcessor;
class BaySickPedalsProcessor;
class BaySickNAMIRProcessor;

// ─────────────────────────────────────────────────────────────────────────────
// EngineRig - QA-ModelShell TS1 (2026-07-27)
// ─────────────────────────────────────────────────────────────────────────────
// The model-side owner of every dynamic tab's identity and engine instance.
// Inverts the page-owned-engine architecture: pages used to construct engines
// and hand raw pointers to the processor, so a processor without pages had no
// instruments (the silent-export root cause) and engine lifetime was chained
// to view lifetime (fatal under destroy-on-close windows).  Pages/windows are
// now disposable views that request engines from here and never create them.
//
// Scope: the six page-owned families (Layers / Bass / Drums / Clips / Vox /
// Inst).  The sfizz engines (Guitars / Basses / RustyDrums) were already
// processor-owned with their own race-safe load paths and stay on them.
//
// Ownership / threading: ALL rig mutation happens on the message thread.  The
// audio thread never touches this class -- it keeps reading the processor's
// SpinLock-guarded raw-pointer arrays, which the register*/unregister* calls
// made from here keep in sync.  Teardown keeps the page-era discipline:
// unregister from audio dispatch FIRST, settle one audio block, then destroy.
// ─────────────────────────────────────────────────────────────────────────────

// QA-ModelShell TS6 (BLU-447): Plugins is the hosted-VST3-instrument tab.  It
// is the "one more factory case" TS1 task 7 designed the generic slot for -- a
// tab's engine is already a base-class unique_ptr, so nothing outside the
// factory and apvtsOf knows what kind of processor it holds.
// APPEND-ONLY.  Freeze filenames spell the kind as a NAME rather than this
// ordinal (PluginProcessor::freezeFileFor), so appending is safe -- but an
// INSERTION would still re-point anything that ever persists the raw value.
// Rusty appended 2026-07-30 so the kit can carry freeze state like any other
// player; its EngineTab holds no engine (the kit engine is processor-owned) and
// exists purely as identity + freeze state.
enum class TabKind { Layers, Bass, Drums, Clips, Vox, Inst, Plugins, Rusty };

struct EngineTab
{
    TabKind kind;
    int pageIndex = -1;
    juce::String name;
    juce::String engineType;   // empty until an engine is picked

    // The processor registered for audio dispatch.  For Layers/Bass/Drums/
    // Clips this is the instrument itself; for Vox the vocal processor; for
    // Inst the EngineChainProcessor wrapper.
    std::unique_ptr<juce::AudioProcessor> engine;

    // Support processors the registered engine references but does not own
    // (Inst: [pedals, namIr] -- the chain wrapper holds raw pointers into
    // these).  Teardown order is enforced in EngineRig::teardownEngine, not
    // by member order: the chain must die before its stages.
    std::vector<std::unique_ptr<juce::AudioProcessor>> ownedStages;

    // Typed views into ownedStages (Inst tabs only; null otherwise).
    BaySickPedalsProcessor* pedals = nullptr;
    BaySickNAMIRProcessor*  namIr  = nullptr;

    // ── TS7 §6: freeze state ──────────────────────────────────────────────────
    // Model-side, because the freeze survives every view: pages are rebuilt and
    // windows come and go, and a frozen tab must stay frozen through all of it.
    //
    // The ENGINE IS NEVER DESTROYED by a freeze -- it is left in place and simply
    // not called (EngineInsertTask::setFrozenSource), so unfreeze is a store of
    // nullptr and no state was ever lost.  That is what "state retained for
    // unfreeze" costs: nothing.
    bool frozen = false;
    // §6.8 provenance.  A hand freeze restores frozen on ANY machine; an AUTO
    // freeze is re-evaluated against the opening machine's threshold, because one
    // computer's performance adaptation means nothing on another.
    bool frozenByUser = false;
    // Jeff's ruling (2026-07-31): an explicit unfreeze takes the tab OUT of
    // auto-freeze's reach.  Session-scoped on purpose -- deliberately never
    // serialized, so a project reload puts the tab back in play.
    bool userUnfroze = false;

    // ── §6.8 THE SPAN ────────────────────────────────────────────────────────
    // What the freeze file actually COVERS.  §6.8 called for this from the start
    // ("each freeze's span, saved and restored with the project") and it was not
    // built; the item was closed anyway, and the omission is not bookkeeping.
    //
    // A freeze file is the SONG arrangement rendered from bar 1.  Nothing
    // recorded that, so the substitution had no way to know what it was reading:
    // in pattern mode the transport wraps inside the pattern loop, so reading the
    // file at the playhead played the SONG's opening bars on loop instead of the
    // pattern -- wrong audio WITH the CPU saving, which is worse than none, and
    // nothing on screen would explain it.  Gating substitution to song mode
    // suppressed the symptom and left pattern mode with no freeze at all.
    //
    // patternIndex == kSongScope means "the whole arrangement"; anything else is
    // that pattern's own render.  lengthBeats + bpm are what the file was
    // rendered AT, so a change to either is detectable rather than silently
    // playing the wrong length.
    static constexpr int kSongScope = -1;
    struct FreezeSpan
    {
        int    patternIndex = kSongScope;
        double lengthBeats  = 0.0;
        double bpm          = 0.0;

        // ── THE CONTENT STAMP ────────────────────────────────────────────────
        // A hash of everything that determines what this file SOUNDS like: the
        // engine's type and its whole parameter state, the notes in scope, the
        // tempo, and (song scope) the arrangement.  0 means "unknown", which is
        // always treated as a mismatch.
        //
        // Without it a freeze file is audio of unknown provenance, and every
        // consumer had to be conservative in a way that cost something real:
        // project restore RE-RENDERED every frozen tab because it could not tell
        // a still-valid file from a stale one, and per-pattern staleness had to
        // invalidate on any edit because it could not tell WHICH pattern's audio
        // had actually changed.  Reusing files without this would have been the
        // opposite error -- silently playing notes the user had deleted.
        juce::uint32 contentStamp = 0;

        bool isSong() const noexcept { return patternIndex == kSongScope; }
        bool stampMatches (juce::uint32 now) const noexcept
        { return contentStamp != 0 && contentStamp == now; }
        // Deliberately tolerant on the doubles: these round-trip through XML as
        // decimal text, and an exact == would call every restored freeze stale.
        bool matches (double beats, double tempo) const noexcept
        {
            return std::abs (lengthBeats - beats) < 1.0e-4
                && std::abs (bpm         - tempo) < 1.0e-4;
        }
    };
    FreezeSpan freezeSpan;
    // Set when this tab's ENGINE-SCOPE content changes (§6.5): its notes, its own
    // parameters, an engine swap, its swing, tempo.  NOT the rack / EQ / fader /
    // sends -- those are downstream of the pre-rack tap and stay live on a frozen
    // tab, which is the whole point of Source Only.  The project-wide dirty flag
    // is useless here: it would invalidate every freeze whenever a master EQ band
    // moved.
    bool freezeStale = false;
    // Streams the cached file during playback.  Owned here so it outlives any
    // block that could be reading it: the task's pointer is cleared FIRST and the
    // streamer released only after.
    // TS7 §6.9: a VECTOR because the Rusty kit freezes as one action across its
    // 13 strips -- one streamer each.  Every other tab kind holds exactly one.
    std::vector<std::unique_ptr<AudioClipStreamer>> freezeStreams;

    // §6.8 PER-PATTERN CACHE, keyed by pattern index.  Song mode reads
    // freezeStreams; pattern mode reads the entry for the pattern that is
    // looping, and falls back to the LIVE engine when there is none rather than
    // playing the song render at a pattern-local position -- which is exactly
    // the wrong-audio bug that made freeze song-mode-only in the first place.
    //
    // The inner vector mirrors freezeStreams' shape: one entry per strip, so the
    // Rusty kit's thirteen strips work here identically to every other tab's one.
    // Pattern renders are a few bars each, which is what makes caching many of
    // them cheap in a way a song render never is.
    std::map<int, std::vector<std::unique_ptr<AudioClipStreamer>>> freezePatternStreams;

    // Patterns whose cached render is known stale (that pattern's CONTENT
    // changed).  Separate from freezeStale, which is the PLAYER axis and
    // invalidates every cached pattern at once -- §6.8's two axes are additive,
    // and collapsing them into one flag is what made per-pattern staleness
    // worthless the first time it was specified.
    std::set<int> stalePatterns;

    // The stamp each cached PATTERN render was made from, so a per-pattern file
    // can be validated independently of the song render and of every other
    // pattern.  Keyed the same as freezePatternStreams.
    std::map<int, juce::uint32> patternStamps;

    // TS7 §6.5: watches THIS engine's own APVTS so a parameter move marks only
    // THIS tab's freeze stale.  Deliberately not the project-wide dirty flag,
    // which would invalidate every freeze whenever a master EQ band moved.
    // Owned per tab so it dies with the engine it watches.
    struct FreezeParamWatcher : juce::ValueTree::Listener
    {
        std::function<void()> onChanged;
        void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override
        { if (onChanged) onChanged(); }
    };
    std::unique_ptr<FreezeParamWatcher> freezeWatcher;

    // §6.5 for engines with NO resolvable APVTS -- hosted plugins, the Inst
    // chain wrapper and its owned stages, the processor-owned kit.  Those
    // players were entirely invisible to staleness: no watcher ever attached,
    // so a knob move on a frozen one never marked it stale.  Listener
    // callbacks carry NO thread guarantee, so they only bump an atomic;
    // EngineRig::drainEngineChangeStamps walks tabs on the message thread.
    struct FreezeProcListener : juce::AudioProcessorListener
    {
        std::atomic<juce::uint32> stamp { 0 };
        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override
        { stamp.fetch_add (1, std::memory_order_release); }
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override
        { stamp.fetch_add (1, std::memory_order_release); }
    };
    std::unique_ptr<FreezeProcListener> freezeProcListener;
    // Everything the listener is attached to, so teardown detaches BEFORE any
    // of it is destroyed (the dangling-listener class of bug).
    std::vector<juce::AudioProcessor*>  freezeListenedProcs;
    juce::uint32                        freezeProcStampSeen = 0;
};

class EngineRig
{
public:
    // The UndoManager reference is DORMANT plumbing (QA-ModelShell conflict
    // call 2=b): threaded into every engine APVTS ctor so QA-UndoCoverage can
    // flip undo semantics on without another ctor sweep.  Nothing consumes it
    // yet -- StandaloneEditor's manager stays authoritative until then.
    EngineRig (VibeSynthProcessor& proc, juce::UndoManager& undoMgr);
    ~EngineRig();

    static int          capacityOf (TabKind k) noexcept;
    static juce::String trackIdFor (TabKind k, int pageIndex);

    // Resolve an engine's APVTS regardless of concrete type (the 7 page-family
    // engine classes).  Null for the chain wrapper and unknown types.  Lives
    // model-side so UI-free consumers (TS2's offline lane replay) share it.
    static juce::AudioProcessorValueTreeState* apvtsOf (juce::AudioProcessor* eng) noexcept;

    // ── Tab identity (message thread) ─────────────────────────────────────
    EngineTab*       addTab  (TabKind k, int pageIndex, const juce::String& name);
    // deleteFreezeFiles: §6.8 lifetime rule.  A USER deleting the tab orphans
    // its freeze files (delete them); a project switch or app quit does NOT --
    // the files are the regenerable cache the content stamp reuses on the next
    // load, and deleting them on every teardown made "an unchanged project
    // renders nothing on load" structurally impossible.
    void             removeTab (TabKind k, int pageIndex, bool deleteFreezeFiles = true);
    // Tear down the tab's engine but KEEP the tab entry (DrumPage clearSound:
    // back to "no sound picked" without losing tab identity).  No settle --
    // matches the page-era swap/clear paths.
    void             clearEngine (TabKind k, int pageIndex);
    EngineTab*       findTab (TabKind k, int pageIndex);
    const EngineTab* findTab (TabKind k, int pageIndex) const;

    // ── TS7 §6: freeze ────────────────────────────────────────────────────────
    // Marks a tab's ENGINE-SCOPE content changed, which is what makes its freeze
    // stale (§6.5).  Called for notes, the engine's own params, an engine swap,
    // swing and tempo -- NOT for the rack / EQ / fader / sends, which are
    // downstream of the pre-rack tap and stay live on a frozen tab.
    void markEngineContentChanged (TabKind k, int pageIndex);
    // Every frozen tab's freeze is invalidated (tempo / tempo-map change, which
    // moves every engine's output in time).
    void markAllFreezesStale();
    // Message thread (the editor's 5 Hz poll): drain the per-tab
    // FreezeProcListener stamps into staleness marks.
    void drainEngineChangeStamps();

    bool isFrozen      (TabKind k, int pageIndex) const;
    bool isFreezeStale (TabKind k, int pageIndex) const;

    // Fires when a tab's freeze state or staleness changes, so the view can
    // repaint its indicator and the freeze driver can queue a re-render.
    // Model-side event, per the batch's registration rule -- never a view hook.
    std::function<void (TabKind, int pageIndex)> onFreezeStateChanged;
    // §6.7: a frozen tab was removed, so its freeze file is now an orphan.  A
    // hook rather than a direct call because the rig does not know where a
    // project lives -- the processor owns the Freeze folder.
    std::function<void (TabKind, int pageIndex)> onFreezeFileObsolete;
    std::vector<EngineTab*> tabsOf (TabKind k);
    int              allocateFreeIndex (TabKind k) const;   // first unused slot, -1 when full
    void             renameTab (TabKind k, int pageIndex, const juce::String& newName);

    // ── Engine lifecycle (message thread) ─────────────────────────────────
    // Construct-or-swap the tab's engine and register it for audio dispatch.
    // Same-type + engine-already-live is a no-op returning the live engine
    // (the DrumPage swap guard, generalized).  Kind-specific engine menus:
    //   Layers: Harmless / BaySickPlayer / BaySickSynth
    //   Bass:   Harmless / BaySickPlayer / BaySickBass
    //   Drums:  BaySickPlayer / BaySickSynth
    //   Clips:  BaySickPlayer      Vox: BaySickVocal      Inst: Chain
    juce::AudioProcessor* setEngineType (TabKind k, int pageIndex,
                                         const juce::String& engineType);

    // Restore principle (HostedPlugin.h): the state blob carries the FULL
    // PluginDescription so a project keeps loading its plugins after the user
    // removed them from the added list.  The walker stashes it here BEFORE the
    // select; the Plugins factory consumes it when findAdded comes up empty.
    void stashPluginRestoreDescription (int pageIndex,
                                        std::unique_ptr<juce::PluginDescription>);

    juce::AudioProcessor* engineFor (TabKind k, int pageIndex) const;

    // §6.8: the span a SONG-scope freeze render covers, sampled at the moment
    // the file lands.  Lives here rather than on the processor because
    // EngineTab is ours -- PluginProcessor.h deliberately forward-declares
    // TabKind instead of including this header (see the note at its top).
    // Hash of everything that determines what a freeze file at this scope would
    // SOUND like -- engine type + full engine state, tempo, the notes in scope,
    // and (song scope) the arrangement.  patternIndex < 0 is song scope.
    // Message thread: reads engine state and the pattern model.
    juce::uint32 freezeContentStamp (TabKind k, int pageIndex, int patternIndex) const;

    EngineTab::FreezeSpan currentSongFreezeSpan() const;
    // currentSongFreezeSpan plus this tab's content stamp -- what a freeze
    // records when its file lands.
    EngineTab::FreezeSpan songFreezeSpanFor (TabKind k, int pageIndex) const;

    // §6.8's two staleness axes.  markEngineContentChanged above is the PLAYER
    // axis (invalidates every cached pattern for one tab); this is the PATTERN
    // axis (invalidates one pattern across every frozen tab).  Additive, never
    // one instead of the other.
    void markPatternContentChanged (int patternIndex);

    // Publishes the render for the pattern now looping to every frozen tab's
    // task.  Called when the current pattern changes.
    void republishPatternSources (int patternIndex);

    // Processor-shutdown path: one settle for the whole rig, then bulk
    // destroy.  Individual removeTab keeps the per-teardown settle instead.
    void teardownAll();

    // Walk every live engine + owned support stage (message/render thread).
    // TS2's offline sweeps (setNonRealtime) consume this; the vocal's
    // embedded NAM/IR is the caller's job (not a rig-owned stage).
    void forEachEngine (const std::function<void (juce::AudioProcessor&)>& fn);

    // ── Model events ──────────────────────────────────────────────────────
    // StandaloneEditor subscribes onEngineCreated ONCE at startup; automation
    // registration keys off it -- a model event, never a view build.
    // onEngineDestroying currently has NO subscriber, and dirty-hook wiring is
    // still view-driven -- both are the seam a future consumer plugs into,
    // not wiring that exists today.
    std::function<void (EngineTab&)> onEngineCreated;      // after registration
    std::function<void (EngineTab&)> onEngineDestroying;   // before unregistration

private:
    juce::AudioProcessor* createEngineFor (EngineTab& tab, const juce::String& engineType);
    void registerWithProcessor   (EngineTab& tab);
    void unregisterFromProcessor (EngineTab& tab);
    void teardownEngine (EngineTab& tab, bool settleAfterUnregister);

    VibeSynthProcessor& mProc;
    juce::UndoManager&  mUndoManager;
    std::vector<std::unique_ptr<EngineTab>> mTabs;
    // Per-page fallback descriptions for the Plugins factory -- see
    // stashPluginRestoreDescription.  Consumed (erased) on use.
    std::map<int, std::unique_ptr<juce::PluginDescription>> mPluginRestoreDescs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineRig)
};
