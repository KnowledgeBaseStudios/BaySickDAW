#pragma once
#include <JuceHeader.h>
#include <vector>
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
    void             removeTab (TabKind k, int pageIndex);
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

    // Decode a base64 engine blob into the tab's live engine
    // (setStateInformation).  No-op on empty/undecodable data -- matches the
    // project-load walker's applyEngineState contract.
    void restoreEngineFromBlob (TabKind k, int pageIndex, const juce::String& base64);

    juce::AudioProcessor* engineFor (TabKind k, int pageIndex) const;

    // Processor-shutdown path: one settle for the whole rig, then bulk
    // destroy.  Individual removeTab keeps the per-teardown settle instead.
    void teardownAll();

    // Walk every live engine + owned support stage (message/render thread).
    // TS2's offline sweeps (setNonRealtime) consume this; the vocal's
    // embedded NAM/IR is the caller's job (not a rig-owned stage).
    void forEachEngine (const std::function<void (juce::AudioProcessor&)>& fn);

    // ── Model events ──────────────────────────────────────────────────────
    // StandaloneEditor subscribes ONCE at startup.  Automation registration
    // and dirty-hook wiring key off these -- model events, never view builds.
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineRig)
};
