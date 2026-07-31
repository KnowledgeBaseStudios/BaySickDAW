#include "EngineRig.h"
#include "PluginProcessor.h"
#include "Harmless/HarmlessProcessor.h"
#include "VibePlayer/VibePlayerProcessor.h"
#include "BaySickSynth/BaySickSynthProcessor.h"
#include "BaySickBass/BaySickBassProcessor.h"
#include "BaySickVocal/BaySickVocalProcessor.h"
#include "BaySickPedals/BaySickPedalsProcessor.h"
#include "BaySickNAMIR/BaySickNAMIRProcessor.h"
#include "Standalone/EngineChainProcessor.h"
#include "Hosting/HostedPlugin.h"   // QA-ModelShell TS6: hosted VST3 instrument

EngineRig::EngineRig (VibeSynthProcessor& proc, juce::UndoManager& undoMgr)
    : mProc (proc), mUndoManager (undoMgr)
{
}

EngineRig::~EngineRig()
{
    teardownAll();
}

int EngineRig::capacityOf (TabKind k) noexcept
{
    switch (k)
    {
        case TabKind::Layers: return kMaxLayerPages;
        case TabKind::Bass:   return kMaxBassPages;
        case TabKind::Drums:  return kMaxDrumPages;
        case TabKind::Clips:  return kMaxClipPages;
        case TabKind::Vox:    return kMaxVoxPages;
        case TabKind::Inst:   return kMaxInstPages;
        case TabKind::Plugins: return kMaxPluginPages;
        // TS7 §6.9: the kit is a SINGLETON -- one instance per project, always
        // pageIndex 0.  Explicit rather than left to the fallback below, which
        // returns 0 and would have silently made a Rusty tab impossible to
        // create.  Same silent-default trap as insertKindForTab.
        case TabKind::Rusty:   return 1;
    }
    return 0;
}

juce::AudioProcessorValueTreeState* EngineRig::apvtsOf (juce::AudioProcessor* eng) noexcept
{
    if (eng == nullptr) return nullptr;
    if (auto* p = dynamic_cast<HarmlessProcessor*>      (eng)) return &p->apvts;
    if (auto* p = dynamic_cast<VibePlayerProcessor*>    (eng)) return &p->apvts;
    if (auto* p = dynamic_cast<BaySickSynthProcessor*>  (eng)) return &p->apvts;
    if (auto* p = dynamic_cast<BaySickBassProcessor*>   (eng)) return &p->apvts;
    if (auto* p = dynamic_cast<BaySickVocalProcessor*>  (eng)) return &p->apvts;
    if (auto* p = dynamic_cast<BaySickPedalsProcessor*> (eng)) return &p->apvts;
    if (auto* p = dynamic_cast<BaySickNAMIRProcessor*>  (eng)) return &p->apvts;
    // A hosted plugin deliberately returns null: its parameters live in the
    // plugin, not in an APVTS of ours, and their automation lanes are keyed on
    // the plugin's own stable parameter ids instead.
    return nullptr;
}

juce::String EngineRig::trackIdFor (TabKind k, int pageIndex)
{
    // Prefix vocabulary is load-bearing: engine APVTS param ids and the
    // serialized blobs embed it, so it must match the page-era spellings
    // exactly (incl. Clips' trailing underscore).
    switch (k)
    {
        case TabKind::Layers: return "lay_"  + juce::String (pageIndex);
        case TabKind::Bass:   return "bas_"  + juce::String (pageIndex);
        case TabKind::Drums:  return "drm_"  + juce::String (pageIndex);
        case TabKind::Clips:  return "clip_" + juce::String (pageIndex) + "_";
        case TabKind::Vox:    return {};
        case TabKind::Inst:   return {};
        // A hosted plugin's parameters belong to the plugin, not to our APVTS
        // vocabulary, so there is no per-tab prefix to hand out.
        case TabKind::Plugins: return {};
        // The kit engine is processor-owned with its own "brd_" vocabulary, so
        // there is no per-tab prefix to hand out.  Explicit for the same reason
        // as the capacity case above.
        case TabKind::Rusty:   return {};
    }
    return {};
}

// ── Tab identity ─────────────────────────────────────────────────────────────

EngineTab* EngineRig::addTab (TabKind k, int pageIndex, const juce::String& name)
{
    if (pageIndex < 0 || pageIndex >= capacityOf (k)) return nullptr;
    if (auto* existing = findTab (k, pageIndex)) return existing;

    auto tab = std::make_unique<EngineTab>();
    tab->kind      = k;
    tab->pageIndex = pageIndex;
    tab->name      = name;
    mTabs.push_back (std::move (tab));
    return mTabs.back().get();
}

void EngineRig::removeTab (TabKind k, int pageIndex)
{
    for (size_t i = 0; i < mTabs.size(); ++i)
    {
        if (mTabs[i]->kind != k || mTabs[i]->pageIndex != pageIndex) continue;

        // §6.7: deleting a frozen track deletes its freeze file.  Done BEFORE
        // teardown so the streamer holding the file open is destroyed first --
        // Windows refuses to delete a file with a live handle, which is exactly
        // how this would have failed silently if it ran after.
        const bool wasFrozen = mTabs[i]->frozen;
        teardownEngine (*mTabs[i], /*settleAfterUnregister=*/ true);
        mTabs.erase (mTabs.begin() + (long) i);
        if (wasFrozen && onFreezeFileObsolete) onFreezeFileObsolete (k, pageIndex);
        return;
    }
}

void EngineRig::clearEngine (TabKind k, int pageIndex)
{
    if (auto* t = findTab (k, pageIndex))
        teardownEngine (*t, /*settleAfterUnregister=*/ false);
}

EngineTab* EngineRig::findTab (TabKind k, int pageIndex)
{
    for (auto& t : mTabs)
        if (t->kind == k && t->pageIndex == pageIndex) return t.get();
    return nullptr;
}

const EngineTab* EngineRig::findTab (TabKind k, int pageIndex) const
{
    for (auto& t : mTabs)
        if (t->kind == k && t->pageIndex == pageIndex) return t.get();
    return nullptr;
}

// ── TS7 §6.5: engine-scope staleness ─────────────────────────────────────────
// A freeze is only invalidated by things that change what the ENGINE PRODUCES.
// Everything from preEq downstream -- rack, EQ, fader, pan, width, sends,
// sidechain, the master chain -- stays live on a frozen tab and must NOT mark it
// stale; that is precisely what Jeff's pre-rack Source Only ruling buys.
//
// Marking is idempotent and only fires the event on a TRANSITION, so a knob
// dragged across a hundred values queues one re-render rather than a hundred.
void EngineRig::markEngineContentChanged (TabKind k, int pageIndex)
{
    // NOT DURING AN OFFLINE RENDER (2026-07-30) -- this was a self-sustaining
    // re-render loop, and Jeff's log caught it before I understood it.
    //
    // The render replays automation by calling setValueNotifyingHost on the
    // ENGINE APVTS (applyOfflineLaneValue), which is the exact tree the freeze
    // watcher listens to.  So freezing tab B wrote tab A's engine params, marked
    // A stale, queued A for re-render -- and A's render did the same back to B.
    // Two frozen tabs ping-ponged forever: twenty renders in three minutes in
    // Jeff's timing log, which I first mis-attributed to grid editing.
    //
    // The distinction that makes this correct rather than a papering-over: those
    // writes are a REPLAY of automation that already exists, not a user edit.
    // They do not change what the tab produces -- they ARE what it produces, and
    // the render is in the middle of capturing exactly that.
    if (mProc.isNonRealtime()) return;

    auto* t = findTab (k, pageIndex);
    if (t == nullptr || ! t->frozen || t->freezeStale) return;
    t->freezeStale = true;
    if (onFreezeStateChanged) onFreezeStateChanged (k, pageIndex);
}

// Tempo and the tempo map move every engine's output in time, so they invalidate
// every freeze rather than any one tab's.
void EngineRig::markAllFreezesStale()
{
    // Same offline guard as markEngineContentChanged, and it matters more here:
    // the render restores song mode, loop bounds and the current pattern on exit,
    // all of which route through content-change signals -- so without this, every
    // render would invalidate every freeze in the project on its way out.
    if (mProc.isNonRealtime()) return;

    for (auto& t : mTabs)
    {
        if (! t->frozen || t->freezeStale) continue;
        t->freezeStale = true;
        if (onFreezeStateChanged) onFreezeStateChanged (t->kind, t->pageIndex);
    }
}

bool EngineRig::isFrozen (TabKind k, int pageIndex) const
{
    const auto* t = findTab (k, pageIndex);
    return t != nullptr && t->frozen;
}

bool EngineRig::isFreezeStale (TabKind k, int pageIndex) const
{
    const auto* t = findTab (k, pageIndex);
    return t != nullptr && t->frozen && t->freezeStale;
}

std::vector<EngineTab*> EngineRig::tabsOf (TabKind k)
{
    std::vector<EngineTab*> out;
    for (auto& t : mTabs)
        if (t->kind == k) out.push_back (t.get());
    return out;
}

int EngineRig::allocateFreeIndex (TabKind k) const
{
    const int cap = capacityOf (k);
    for (int i = 0; i < cap; ++i)
        if (findTab (k, i) == nullptr) return i;
    return -1;
}

void EngineRig::renameTab (TabKind k, int pageIndex, const juce::String& newName)
{
    if (auto* t = findTab (k, pageIndex)) t->name = newName;
}

// ── Engine lifecycle ─────────────────────────────────────────────────────────

juce::AudioProcessor* EngineRig::setEngineType (TabKind k, int pageIndex,
                                                const juce::String& engineType)
{
    auto* tab = findTab (k, pageIndex);
    if (tab == nullptr || engineType.isEmpty()) return nullptr;

    if (tab->engineType == engineType && tab->engine != nullptr)
        return tab->engine.get();

    // Swap path keeps the page-era semantics: unregister + destroy with NO
    // settle sleep (only full teardown settles) -- the dispatcher's
    // unregisterTask already fences the in-flight block.
    if (tab->engine != nullptr)
        teardownEngine (*tab, /*settleAfterUnregister=*/ false);

    tab->engineType = engineType;
    if (createEngineFor (*tab, engineType) == nullptr)
    {
        tab->engineType.clear();
        return nullptr;
    }

    // TS7 §6.5: a different engine produces different audio, so this tab's freeze
    // is stale.  Per-tab and precise here, unlike the roll hook which has to mark
    // broadly -- the swap knows exactly which tab it changed.
    markEngineContentChanged (k, pageIndex);

    registerWithProcessor (*tab);

    // TS7 §6.5: watch this engine's OWN parameters so a knob move on a frozen tab
    // marks it stale.  Attached after creation and re-made per engine, so a swap
    // cannot leave a listener on a destroyed APVTS.
    if (auto* apvts = apvtsOf (tab->engine.get()))
    {
        tab->freezeWatcher = std::make_unique<EngineTab::FreezeParamWatcher>();
        tab->freezeWatcher->onChanged = [this, k, pageIndex]
        { markEngineContentChanged (k, pageIndex); };
        apvts->state.addListener (tab->freezeWatcher.get());
    }

    if (onEngineCreated) onEngineCreated (*tab);
    return tab->engine.get();
}

juce::AudioProcessor* EngineRig::createEngineFor (EngineTab& tab, const juce::String& engineType)
{
    const double srOr44100 = mProc.getSampleRate() > 0.0 ? mProc.getSampleRate() : 44100.0;
    const juce::String trackId = trackIdFor (tab.kind, tab.pageIndex);

    switch (tab.kind)
    {
        case TabKind::Layers:
        case TabKind::Bass:
        case TabKind::Drums:
        case TabKind::Clips:
        {
            // Page-era creation prep kept verbatim (rate-or-44100, fixed 512):
            // these are creation defaults; the device path re-prepares live
            // engines on rate/block changes.
            std::unique_ptr<juce::AudioProcessor> eng;
            if      (engineType == "Harmless")      eng = std::make_unique<HarmlessProcessor>     (trackId, &mUndoManager);
            else if (engineType == "BaySickPlayer") eng = std::make_unique<VibePlayerProcessor>   (trackId, &mUndoManager);
            else if (engineType == "BaySickSynth")  eng = std::make_unique<BaySickSynthProcessor> (trackId, &mUndoManager);
            else if (engineType == "BaySickBass")   eng = std::make_unique<BaySickBassProcessor>  (trackId, &mUndoManager);
            if (eng == nullptr) return nullptr;

            eng->prepareToPlay (srOr44100, 512);
            tab.engine = std::move (eng);
            return tab.engine.get();
        }

        case TabKind::Vox:
        {
            if (engineType != "BaySickVocal") return nullptr;
            auto vp = std::make_unique<BaySickVocalProcessor> (&mUndoManager);
            vp->prepareToPlay (44100.0, 512);   // VoxPage-era creation prep
            tab.engine = std::move (vp);
            return tab.engine.get();
        }

        // QA-ModelShell TS6 (BLU-447): engineType IS the plugin's stable
        // identifier string, which is already what the tab record persists --
        // so a hosted instrument saves and restores through the existing tab
        // serialization with no new format.  The description is resolved from
        // the added list; a plugin the user has since removed yields a tab with
        // no engine rather than a failed load.
        case TabKind::Plugins:
        {
            auto* pm = Hosting::PluginManager::getInstance();
            if (pm == nullptr) return nullptr;

            auto desc = pm->findAdded (engineType);
            if (desc == nullptr) return nullptr;

            auto hosted = std::make_unique<Hosting::HostedPluginInstance> (*pm, *desc);
            hosted->prepareToPlay (srOr44100, 512);
            tab.engine = std::move (hosted);
            return tab.engine.get();
        }

        case TabKind::Inst:
        {
            if (engineType != "Chain") return nullptr;
            // InstPage-era shape: both stages permanent, chain wrapper is the
            // registered engine and fans Pedals -> NAM/IR.
            auto pedals = std::make_unique<BaySickPedalsProcessor> (&mUndoManager);
            pedals->prepareToPlay (44100.0, 512);
            auto nam = std::make_unique<BaySickNAMIRProcessor> (&mUndoManager);
            nam->prepareToPlay (44100.0, 512);

            auto chain = std::make_unique<EngineChainProcessor>();
            chain->setChain ({ pedals.get(), nam.get() });

            tab.pedals = pedals.get();
            tab.namIr  = nam.get();
            tab.ownedStages.clear();
            tab.ownedStages.push_back (std::move (pedals));
            tab.ownedStages.push_back (std::move (nam));
            tab.engine = std::move (chain);
            return tab.engine.get();
        }
    }
    return nullptr;
}

void EngineRig::registerWithProcessor (EngineTab& tab)
{
    if (tab.engine == nullptr) return;
    auto* eng = tab.engine.get();

    // 2026-07-31: hand the new engine the transport.  The processor's per-block
    // propagation is gated on the playhead POINTER changing, and it changes once
    // at the first audio block -- long before this engine existed -- so without
    // this an engine added later never receives one.  For a hosted VST3 that is
    // the whole of its transport: JUCE derives the entire VST3 ProcessContext
    // from AudioPlayHead::getPosition(), so no playhead means a zeroed context
    // and nothing in the plugin syncs to anything.
    eng->setPlayHead (mProc.enginePlayHead());

    switch (tab.kind)
    {
        case TabKind::Layers: mProc.registerLayerEngine (tab.pageIndex, eng); break;
        case TabKind::Bass:   mProc.registerBassEngine  (tab.pageIndex, eng); break;
        case TabKind::Drums:  mProc.registerDrumEngine  (tab.pageIndex, eng); break;
        // QA-ModelShell TS6: prepared at the LIVE device config first -- a
        // hosted plugin is created with a placeholder rate like the Clips/Vox/
        // Inst group below, and a plugin that never sees a real prepare would
        // run its DSP at the wrong rate.
        case TabKind::Plugins:
        {
            const double sr = mProc.getSampleRate() > 0.0 ? mProc.getSampleRate() : 44100.0;
            const int    bs = mProc.getBlockSize()  > 0   ? mProc.getBlockSize()  : 512;
            eng->setRateAndBufferSizeDetails (sr, bs);
            eng->prepareToPlay (sr, bs);
            mProc.registerPluginEngine (tab.pageIndex, eng);
            break;
        }

        case TabKind::Clips:
        case TabKind::Vox:
        case TabKind::Inst:
        {
            // Spawn-path parity: these engines were re-prepared with the LIVE
            // device config immediately before registration (the old
            // onEngineChanged handler flow); their creation prep was a
            // placeholder.
            const double sr = mProc.getSampleRate() > 0.0 ? mProc.getSampleRate() : 44100.0;
            const int    bs = mProc.getBlockSize()  > 0   ? mProc.getBlockSize()  : 512;
            eng->prepareToPlay (sr, bs);
            if      (tab.kind == TabKind::Clips) mProc.registerClipEngine (tab.pageIndex, eng);
            else if (tab.kind == TabKind::Vox)   mProc.registerVoxEngine  (tab.pageIndex, eng);
            else                                 mProc.registerInstEngine (tab.pageIndex, eng);

            // TS7 §6.5: pitch + align edits are BAKED INTO a Vox freeze (the
            // capture point is below both), and they never touch the APVTS tree
            // the freeze watcher listens to -- so without this hook, editing a
            // note on a frozen vocal would keep playing the old take silently.
            // Wired HERE, at registration, so it survives every engine swap and
            // project reload rather than depending on a page existing.
            if (tab.kind == TabKind::Vox)
                if (auto* vp = dynamic_cast<BaySickVocalProcessor*> (eng))
                {
                    const int pageIdx = tab.pageIndex;
                    vp->onPitchAlignEditsChanged = [this, pageIdx]
                    {
                        markEngineContentChanged (TabKind::Vox, pageIdx);
                    };
                }
            break;
        }
    }
}

void EngineRig::unregisterFromProcessor (EngineTab& tab)
{
    switch (tab.kind)
    {
        case TabKind::Layers: mProc.unregisterLayerEngine (tab.pageIndex); break;
        case TabKind::Bass:   mProc.unregisterBassEngine  (tab.pageIndex); break;
        case TabKind::Drums:  mProc.unregisterDrumEngine  (tab.pageIndex); break;
        case TabKind::Clips:  mProc.unregisterClipEngine  (tab.pageIndex); break;
        case TabKind::Vox:    mProc.unregisterVoxEngine   (tab.pageIndex); break;
        case TabKind::Inst:   mProc.unregisterInstEngine  (tab.pageIndex); break;
        case TabKind::Plugins: mProc.unregisterPluginEngine (tab.pageIndex); break;
    }
}

void EngineRig::teardownEngine (EngineTab& tab, bool settleAfterUnregister)
{
    if (tab.engine == nullptr) return;

    if (onEngineDestroying) onEngineDestroying (tab);

    // TS7 §6.5: detach the freeze param watcher BEFORE the engine (and its
    // APVTS) is destroyed.  EngineTab's members destruct in REVERSE declaration
    // order, so freezeWatcher would otherwise die while the tree it is attached
    // to is still alive -- a dangling listener that the next property change
    // would walk into.  Removed here so the order is explicit rather than a
    // property of member layout that a later tidy-up could silently flip.
    if (tab.freezeWatcher != nullptr)
    {
        if (auto* apvts = apvtsOf (tab.engine.get()))
            apvts->state.removeListener (tab.freezeWatcher.get());
        tab.freezeWatcher.reset();
    }

    unregisterFromProcessor (tab);
    if (settleAfterUnregister)
        juce::Thread::sleep (20);   // outlast one audio block (page-dtor contract)

    // The chain wrapper holds raw pointers into ownedStages -- it must be
    // destroyed before them.
    tab.engine.reset();
    tab.pedals = nullptr;
    tab.namIr  = nullptr;
    tab.ownedStages.clear();
    tab.engineType.clear();
}

void EngineRig::restoreEngineFromBlob (TabKind k, int pageIndex, const juce::String& base64)
{
    auto* tab = findTab (k, pageIndex);
    if (tab == nullptr || tab->engine == nullptr || base64.isEmpty()) return;

    juce::MemoryBlock mb;
    if (! mb.fromBase64Encoding (base64)) return;
    if (mb.getSize() == 0) return;
    tab->engine->setStateInformation (mb.getData(), (int) mb.getSize());
}

juce::AudioProcessor* EngineRig::engineFor (TabKind k, int pageIndex) const
{
    if (auto* t = findTab (k, pageIndex)) return t->engine.get();
    return nullptr;
}

void EngineRig::forEachEngine (const std::function<void (juce::AudioProcessor&)>& fn)
{
    if (! fn) return;
    for (auto& t : mTabs)
    {
        if (t->engine) fn (*t->engine);
        for (auto& s : t->ownedStages)
            if (s) fn (*s);
    }
}

void EngineRig::teardownAll()
{
    // Shutdown path: the editor (and its subscriptions) is already gone, so
    // events must not fire into dead views.
    onEngineCreated    = nullptr;
    onEngineDestroying = nullptr;

    bool anyEngine = false;
    for (auto& t : mTabs)
    {
        if (t->engine == nullptr) continue;
        unregisterFromProcessor (*t);
        anyEngine = true;
    }
    if (anyEngine)
        juce::Thread::sleep (20);   // one settle for the whole rig

    for (auto& t : mTabs)
    {
        t->engine.reset();          // chain before stages, same as teardownEngine
        t->pedals = nullptr;
        t->namIr  = nullptr;
        t->ownedStages.clear();
    }
    mTabs.clear();
}
