#pragma once
#include <JuceHeader.h>
#include <vector>
#include "VibesynthConstants.h"

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

enum class TabKind { Layers, Bass, Drums, Clips, Vox, Inst };

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
