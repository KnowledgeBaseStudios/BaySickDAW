#include "EngineRig.h"
#include "PluginProcessor.h"
#include "Harmless/HarmlessProcessor.h"
#include "BaySickPlayer/BaySickPlayerProcessor.h"
#include "BaySickSynth/BaySickSynthProcessor.h"
#include "BaySickBass/BaySickBassProcessor.h"
#include "BaySickVocal/BaySickVocalProcessor.h"
#include "BaySickPedals/BaySickPedalsProcessor.h"
#include "BaySickNAMIR/BaySickNAMIRProcessor.h"
#include "Standalone/EngineChainProcessor.h"
#include "Hosting/HostedPlugin.h"   // QA-ModelShell TS6: hosted VST3 instrument
// Complete types for the content stamp's processor-owned engine hashing --
// PluginProcessor.h only forward-declares these three.
#include "BaySickGuitars/BaySickGuitarsProcessor.h"
#include "BaySickBasses/BaySickBassesProcessor.h"
#include "BaySickRustyDrums/BaySickRustyDrumsProcessor.h"

EngineRig::EngineRig (BaySickDAWProcessor& proc, juce::UndoManager& undoMgr)
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
    if (auto* p = dynamic_cast<BaySickPlayerProcessor*>    (eng)) return &p->apvts;
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

EngineTab* EngineRig::addTab (TabKind k, int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= capacityOf (k)) return nullptr;
    if (auto* existing = findTab (k, pageIndex)) return existing;

    auto tab = std::make_unique<EngineTab>();
    tab->kind      = k;
    tab->pageIndex = pageIndex;
    mTabs.push_back (std::move (tab));
    return mTabs.back().get();
}

void EngineRig::removeTab (TabKind k, int pageIndex, bool deleteFreezeFiles)
{
    for (size_t i = 0; i < mTabs.size(); ++i)
    {
        if (mTabs[i]->kind != k || mTabs[i]->pageIndex != pageIndex) continue;

        // ORDER IS LOAD-BEARING (the Clips-close use-after-free): a Clips
        // strip's Composite task SURVIVES this teardown -- audio inserts
        // persist for the project lifetime -- so its frozen-source pointers
        // must be nulled BEFORE the streamers die, and the settle inside
        // teardownEngine is what lets an in-flight block drain between the
        // null and the erase.  Harmless for the kinds whose task dies with
        // the unregister.
        const bool wasFrozen = mTabs[i]->frozen;
        if (wasFrozen)
            mProc.retractFrozenSources (k, pageIndex);

        teardownEngine (*mTabs[i], /*settleAfterUnregister=*/ true);

        // teardownEngine early-returns on an engine-less tab, so its settle
        // cannot be what covers the erase below -- and the erase destroys the
        // freeze streamers a still-live insert task reads through the pointers
        // retracted above.  Keyed on what is actually being freed rather than
        // on whether an engine happens to exist, so removeTab is safe on its
        // own instead of relying on the caller's statement order.
        if (wasFrozen)
            mProc.settleAudioThread();

        // The stash is keyed on page index alone, so it has to die with the tab
        // identity that index stands for: a Plugins tab at index N in the next
        // project is not this one, and the factory would not know the difference.
        if (k == TabKind::Plugins)
            mPluginRestoreDescs.erase (pageIndex);

        mTabs.erase (mTabs.begin() + (long) i);

        // File delete AFTER teardown: Windows refuses to delete a file the
        // streamer still holds open.  And only on a USER delete -- a project
        // switch or app quit keeps the files, which are the regenerable cache
        // the content stamp reuses on the next load (§6.8).
        if (wasFrozen && deleteFreezeFiles && onFreezeFileObsolete)
            onFreezeFileObsolete (k, pageIndex);
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

    // NOR DURING A REVIVAL.  retryDeadPluginTab rebuilds the SAME plugin from the
    // SAME description and pushes the SAME state blob back in, so nothing this
    // tab produces changed -- while the rebuild routes through setEngineType,
    // whose mark is written for a genuine engine SWAP.  Letting it through
    // discards the freeze in the one situation freeze is most load-bearing: a
    // frozen tab whose plugin is dead keeps playing its good render, and any
    // unrelated edit to the added-plugin list would otherwise throw that away.
    if (k == TabKind::Plugins && pageIndex == mRevivingPluginPage) return;

    auto* t = findTab (k, pageIndex);
    if (t == nullptr || ! t->frozen) return;

    const bool transition = ! t->freezeStale;
    t->freezeStale = true;

    // §6.8 AXIS ONE -- the PLAYER changed, so EVERY cached pattern for this tab
    // is wrong, not just the one being viewed.  Populated even when the tab was
    // ALREADY stale: markAllFreezesStale sets the flag without walking this
    // map, so an early-out on the flag left tempo-staled caches looking valid
    // to the republish gate.
    for (const auto& kv : t->freezePatternStreams)
        t->stalePatterns.insert (kv.first);

    // §6.6: stale plays LIVE from this block, not from whenever the re-render
    // lands.  Atomic nulls only -- the streams stay owned by the tab, so
    // nothing here frees memory an in-flight block could be reading.
    mProc.retractFrozenSources (k, pageIndex);
    republishPatternSources (mProc.freezePatternIndexNow());

    if (transition && onFreezeStateChanged) onFreezeStateChanged (k, pageIndex);
}

// §6.8 AXIS TWO -- this PATTERN's notes changed, so its cached render is wrong
// for every tab that has one, while those tabs' OTHER patterns stay valid.  The
// song render is invalidated too: the arrangement contains this pattern.
void EngineRig::markPatternContentChanged (int patternIndex)
{
    if (mProc.isNonRealtime() || patternIndex < 0) return;

    for (auto& t : mTabs)
    {
        if (! t->frozen) continue;

        const bool hasPattern = t->freezePatternStreams.count (patternIndex) > 0;
        if (hasPattern) t->stalePatterns.insert (patternIndex);

        const bool transition = ! t->freezeStale;
        t->freezeStale = true;
        // §6.6: the song render contains this pattern, so it must stop playing
        // NOW -- retraction is what makes stale-plays-live true during
        // playback rather than a promise the next re-render keeps.
        mProc.retractFrozenSources (t->kind, t->pageIndex);
        if (transition && onFreezeStateChanged)
            onFreezeStateChanged (t->kind, t->pageIndex);
    }

    // Re-point every tab whose CURRENT pattern render is still valid -- the
    // retraction above nulled those too, and the editor's poll only
    // republishes on a pattern CHANGE, so without this they would sit on the
    // live engine until the user switched away and back.
    republishPatternSources (mProc.freezePatternIndexNow());
}

// Point every frozen tab's task at the render for the pattern now looping, or at
// nothing when there is none or it is stale -- which falls back to the live
// engine.  Message thread; the audio thread only ever reads the published
// pointer, so nothing here frees anything a block could be holding.
void EngineRig::republishPatternSources (int patternIndex)
{
    for (auto& t : mTabs)
    {
        if (! t->frozen) continue;

        AudioClipStreamer* s = nullptr;

        if (patternIndex >= 0 && t->stalePatterns.count (patternIndex) == 0)
        {
            auto it = t->freezePatternStreams.find (patternIndex);
            if (it != t->freezePatternStreams.end() && ! it->second.empty())
                s = it->second.front().get();
        }

        // The kit is THIRTEEN tasks, not one -- renderTaskForTab cannot express
        // it, which is the same reason its freeze has its own render path.
        if (t->kind == TabKind::Rusty)
        {
            const std::vector<std::unique_ptr<AudioClipStreamer>>* v = nullptr;
            if (patternIndex >= 0 && t->stalePatterns.count (patternIndex) == 0)
            {
                auto it = t->freezePatternStreams.find (patternIndex);
                if (it != t->freezePatternStreams.end() && ! it->second.empty())
                    v = &it->second;
            }
            mProc.setRustyFrozenPatternSources (v, patternIndex);
            continue;
        }

        if (auto* task = mProc.renderTaskForTab (t->kind, t->pageIndex))
            task->setFrozenPatternSource (s, patternIndex);
    }
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
        if (! t->frozen) continue;

        // Every cached pattern too, not just the flag: the republish gate
        // reads stalePatterns, and leaving it empty here is exactly how a
        // tempo change kept pattern mode serving pre-change renders.
        for (const auto& kv : t->freezePatternStreams)
            t->stalePatterns.insert (kv.first);

        const bool transition = ! t->freezeStale;
        t->freezeStale = true;
        mProc.retractFrozenSources (t->kind, t->pageIndex);
        if (transition && onFreezeStateChanged) onFreezeStateChanged (t->kind, t->pageIndex);
    }
}

bool EngineRig::isFrozen (TabKind k, int pageIndex) const
{
    const auto* t = findTab (k, pageIndex);
    return t != nullptr && t->frozen;
}

// Message thread only: markEngineContentChanged walks vectors and fires view
// callbacks, which is exactly what the listener callbacks must not do from
// whatever thread notified them.
void EngineRig::drainEngineChangeStamps()
{
    for (auto& t : mTabs)
    {
        if (t->freezeProcListener == nullptr) continue;
        const auto s = t->freezeProcListener->stamp.load (std::memory_order_acquire);
        if (s == t->freezeProcStampSeen) continue;
        t->freezeProcStampSeen = s;
        if (t->frozen) markEngineContentChanged (t->kind, t->pageIndex);
    }
}

bool EngineRig::isFreezeStale (TabKind k, int pageIndex) const
{
    const auto* t = findTab (k, pageIndex);
    return t != nullptr && t->frozen && t->freezeStale;
}

// ── Engine lifecycle ─────────────────────────────────────────────────────────

juce::AudioProcessor* EngineRig::setEngineType (TabKind k, int pageIndex,
                                                const juce::String& engineType)
{
    auto* tab = findTab (k, pageIndex);
    if (tab == nullptr || engineType.isEmpty()) return nullptr;

    if (tab->engineType == engineType && tab->engine != nullptr)
        return tab->engine.get();

    // The swap takes the same drain as a full teardown.  Nothing else covers it:
    // EngineInsertTask caches its engine in the constructor and nothing ever nulls
    // it, so a strip task cannot early-out on a dead engine, and the destroy below
    // otherwise lands under whatever block is in flight -- for a hosted plugin that
    // means unloading the module or unmapping the bridge's shared block while the
    // audio thread is inside it.
    if (tab->engine != nullptr)
        teardownEngine (*tab, /*settleAfterUnregister=*/ true);

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
    else if (tab->engine != nullptr)
    {
        // No APVTS to watch (hosted plugin / chain wrapper): listen to the
        // processors themselves -- the wrapper AND its owned stages, so a
        // pedal or amp knob still invalidates an Inst freeze.  Bump-and-drain,
        // never a direct mark (no thread guarantee on the notifications).
        tab->freezeProcListener = std::make_unique<EngineTab::FreezeProcListener>();
        tab->freezeListenedProcs.clear();
        auto attach = [&tab] (juce::AudioProcessor* p)
        {
            if (p == nullptr) return;
            p->addListener (tab->freezeProcListener.get());
            tab->freezeListenedProcs.push_back (p);
        };
        attach (tab->engine.get());
        for (const auto& stage : tab->ownedStages)
            attach (stage.get());
    }

    if (onEngineCreated) onEngineCreated (*tab);
    return tab->engine.get();
}

juce::AudioProcessor* EngineRig::recreateEngine (TabKind k, int pageIndex)
{
    auto* tab = findTab (k, pageIndex);
    if (tab == nullptr || tab->engine == nullptr || tab->engineType.isEmpty())
        return nullptr;

    // Copied out first: teardownEngine clears the tab's engineType.
    const juce::String type = tab->engineType;

    teardownEngine (*tab, /*settleAfterUnregister=*/ true);

    // setEngineType now meets an engine-less tab, so it runs its whole build
    // path -- createEngineFor, registerWithProcessor, the freeze watcher and
    // onEngineCreated -- instead of the early-return this function exists to
    // get past.  Calling it rather than repeating those steps is the point:
    // a parallel path is one missed step away from a leak or a dead lane.
    return setEngineType (k, pageIndex, type);
}

bool EngineRig::retryDeadPluginTab (int pageIndex)
{
    auto* tab = findTab (TabKind::Plugins, pageIndex);
    if (tab == nullptr) return false;

    auto* dead = dynamic_cast<Hosting::HostedPluginInstance*> (tab->engine.get());
    if (dead == nullptr || dead->isAlive()) return false;

    // Nothing could be built anyway, so bail BEFORE the teardown -- trading a
    // dead instance for no instance at all would take the tab's plugin identity
    // with it and leave nothing left to retry.
    if (Hosting::PluginManager::getInstance() == nullptr) return false;

    // Both read while the outgoing instance still exists.
    auto desc = std::make_unique<juce::PluginDescription> (dead->getDescription());
    juce::MemoryBlock state;
    dead->getStateInformation (state);

    // The factory's added-list lookup is precisely what fails in this case, so
    // it is handed the description the dead instance was built from.
    stashPluginRestoreDescription (pageIndex, std::move (desc));

    // Scoped to THIS page so anything else that marks during the rebuild still
    // lands -- see the guard in markEngineContentChanged for why a revival is
    // not a content change.
    const int prevReviving = mRevivingPluginPage;
    mRevivingPluginPage = pageIndex;

    // THREAD SAFETY -- the twin of EffectsPage::retryDeadPluginSlot's shield, and
    // it must not diverge from it.  registerWithProcessor publishes the rebuilt
    // engine to the dispatcher, and the state push that follows reaches straight
    // into the live plugin: HostedPluginInstance::setStateInformation hands the
    // blob to mInner->setStateInformation, which almost no VST3 tolerates
    // concurrently with its own processBlock, and can additionally re-instantiate
    // -- freeing mInner outright -- when the blob's bridge mode differs from the
    // mode the fresh instance came up in.  Shield raised so processBlock bails to
    // silence, settle so the in-flight block has returned, and only then is
    // anything published or pushed.  Nest-aware save/restore; recreateEngine's own
    // teardown shield nests inside this one and so pays no second settle.
    //
    // The re-instantiate branch specifically cannot fire from a Plugins TAB as the
    // surface stands, and the reason is worth writing down because it is not
    // structural: setBridgePreference has exactly one caller outside
    // HostedPluginInstance -- the RACK slot window's "Run bridged" item -- so a
    // 64-bit tab instance is pinned at mBridgePreferred == false, and a 32-bit one
    // is bridged from birth, leaving mInner and mSandbox both null on the one tab
    // path that does re-instantiate (a bridge start that failed twice).  That is
    // an accident of which window grew which menu.  The concurrent-setState hazard
    // above is live here regardless, so the shield is unconditional.
    const bool shieldWasUp = mProc.isProjectLoadInProgress();
    mProc.setProjectLoadInProgress (true);
    if (! shieldWasUp)
        mProc.settleAudioThread();

    auto* revived = recreateEngine (TabKind::Plugins, pageIndex);

    if (revived != nullptr && state.getSize() > 0)
        revived->setStateInformation (state.getData(), (int) state.getSize());

    mProc.setProjectLoadInProgress (shieldWasUp);
    mRevivingPluginPage = prevReviving;

    // Belt and braces.  The factory consumes the stash unconditionally, but
    // recreateEngine has early returns that never reach it (no tab, no engine,
    // no engineType), and the stash above must not outlive this call either way.
    mPluginRestoreDescs.erase (pageIndex);

    if (revived == nullptr)
    {
        // No engine at all now, and the tab's engineType went with it -- there is
        // nothing left for a cached render to be a render OF, so this one really
        // is a content change and the suppression above must not cover it.
        markEngineContentChanged (TabKind::Plugins, pageIndex);
        return false;
    }

    // The rebuild installed a FRESH FreezeProcListener whose stamp starts at zero
    // while freezeProcStampSeen still holds the outgoing listener's count, and the
    // state push above bumps the new one besides.  Either mismatch makes the next
    // drainEngineChangeStamps mark this tab stale after the fact and undo the
    // suppression.
    if (tab->freezeProcListener != nullptr)
        tab->freezeProcStampSeen = tab->freezeProcListener->stamp.load (std::memory_order_acquire);

    // The teardown destroyed this tab's RenderTask and the rebuild made a new one,
    // which starts with no frozen source at all -- so a preserved freeze has to be
    // re-published here or the tab silently drops back to playing live.
    if (tab->frozen)
    {
        if (! tab->freezeStale && ! tab->freezeStreams.empty())
            if (auto* task = mProc.renderTaskForTab (TabKind::Plugins, pageIndex))
                task->setFrozenSource (tab->freezeStreams.front().get());

        republishPatternSources (mProc.freezePatternIndexNow());
    }

    auto* inst = dynamic_cast<Hosting::HostedPluginInstance*> (revived);
    return inst != nullptr && inst->isAlive();
}

juce::AudioProcessor* EngineRig::createEngineFor (EngineTab& tab, const juce::String& engineType)
{
    const double srOr44100 = mProc.getSampleRate() > 0.0 ? mProc.getSampleRate() : 44100.0;
    const juce::String trackId = trackIdFor (tab.kind, tab.pageIndex);

    // QA-UndoCoverage redo-across-resurrection fix (2026-08-06): every rig
    // engine's APVTS gets a STABLE identity tag -- (kind, pageIndex), the same
    // key the whole resurrection spine uses -- so undo entries re-resolve the
    // live instance at apply time and survive engine re-creation.
    const juce::String rigTag = "rig:" + juce::String ((int) tab.kind)
                              + ":" + juce::String (tab.pageIndex);

    switch (tab.kind)
    {
        case TabKind::Layers:
        case TabKind::Bass:
        case TabKind::Drums:
        case TabKind::Clips:
        {
            // A PLACEHOLDER prep, like every other kind's below: registration is
            // what prepares the engine at the live device config, and the
            // processor's own prepareToPlay sweep is what carries later device
            // rate/block CHANGES.  (Until 2026-08-09 this comment claimed the
            // device path was the only prepare these ever needed, and that is
            // what hid the missing re-prepare in registerWithProcessor.)
            std::unique_ptr<juce::AudioProcessor> eng;
            if      (engineType == "Harmless")      { auto e = std::make_unique<HarmlessProcessor>     (trackId, &mUndoManager); e->apvts.undoOwnerTag = rigTag; eng = std::move (e); }
            else if (engineType == "BaySickPlayer") { auto e = std::make_unique<BaySickPlayerProcessor>   (trackId, &mUndoManager); e->apvts.undoOwnerTag = rigTag; eng = std::move (e); }
            else if (engineType == "BaySickSynth")  { auto e = std::make_unique<BaySickSynthProcessor> (trackId, &mUndoManager); e->apvts.undoOwnerTag = rigTag; eng = std::move (e); }
            else if (engineType == "BaySickBass")   { auto e = std::make_unique<BaySickBassProcessor>  (trackId, &mUndoManager); e->apvts.undoOwnerTag = rigTag; eng = std::move (e); }
            if (eng == nullptr) return nullptr;

            eng->prepareToPlay (srOr44100, 512);
            tab.engine = std::move (eng);
            return tab.engine.get();
        }

        case TabKind::Vox:
        {
            if (engineType != "BaySickVocal") return nullptr;
            auto vp = std::make_unique<BaySickVocalProcessor> (&mUndoManager);
            vp->apvts.undoOwnerTag = rigTag;
            vp->getNamIrProcessor().apvts.undoOwnerTag = rigTag + ".namir";
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

            // Fall back to the description the project's own blob carried
            // (stashed by the restore walker).  NARROWED 2026-08-10: the
            // stashed description is itself added-list checked now, so this no
            // longer resurrects an un-added plugin.  It covers the case where
            // findAdded misses by IDENTIFIER while the same file path is still
            // added - a rescan that changed a uid.
            //
            // SINGLE USE, which is why the erase is unconditional rather than
            // part of the fallback.  A stash is only ever written immediately
            // before the one select it was derived for -- both writers do that,
            // the restore walkers from the project blob and retryDeadPluginTab
            // from the dead instance -- so once this select has run it describes
            // nothing.  Leaving it behind whenever findAdded HIT (the common
            // case) kept a description keyed on a bare page index alive past the
            // tab's deletion and into the next project, where a later select for
            // a DIFFERENT plugin that missed the added list would silently build
            // this one while setEngineType still stamped the tab with the
            // identifier that was actually asked for -- type and instance
            // diverging with no signal, and the page's own "not in your added
            // list" warning suppressed because the type read back correct.
            auto stashed = mPluginRestoreDescs.find (tab.pageIndex);
            if (stashed != mPluginRestoreDescs.end())
            {
                if (desc == nullptr)
                    desc = std::move (stashed->second);
                mPluginRestoreDescs.erase (stashed);
            }
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
            pedals->apvts.undoOwnerTag = rigTag + ".pedals";
            pedals->prepareToPlay (44100.0, 512);
            auto nam = std::make_unique<BaySickNAMIRProcessor> (&mUndoManager);
            nam->apvts.undoOwnerTag = rigTag + ".namir";
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

void EngineRig::liveDeviceConfig (double& sampleRate, int& blockSize) const noexcept
{
    sampleRate = mProc.getSampleRate();
    blockSize  = mProc.getBlockSize();

    // Only reachable before the audio device has ever prepared the processor
    // (project restore can run first).  The values are arbitrary because nothing
    // is rendering yet -- BaySickDAWProcessor::prepareToPlay sweeps every
    // registered engine the moment a real device arrives, so these are never the
    // config an engine actually renders at.
    if (sampleRate <= 0.0) sampleRate = 44100.0;
    if (blockSize  <= 0)   blockSize  = 512;
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
        // Prepared at the LIVE device config before registration, exactly like
        // every kind below.  These three were the omission (fixed 2026-08-09):
        // they registered straight off createEngineFor's placeholder prep, so an
        // engine created while the device ran a LARGER block than that
        // placeholder received blocks bigger than it had sized its buffers for --
        // adding a tab, or loading any project, at a 1024-sample buffer put every
        // Layers/Bass/Drums engine in that state until some unrelated device
        // change happened to re-prepare it.
        case TabKind::Layers:
        case TabKind::Bass:
        case TabKind::Drums:
        {
            double sr; int bs;
            liveDeviceConfig (sr, bs);
            eng->prepareToPlay (sr, bs);
            if      (tab.kind == TabKind::Layers) mProc.registerLayerEngine (tab.pageIndex, eng);
            else if (tab.kind == TabKind::Bass)   mProc.registerBassEngine  (tab.pageIndex, eng);
            else                                  mProc.registerDrumEngine  (tab.pageIndex, eng);
            break;
        }

        // QA-ModelShell TS6: a hosted plugin additionally needs the details set,
        // not just a prepare -- JUCE's VST3 wrapper reads them back off the
        // AudioProcessor.  Our own engines deliberately do NOT get that call: the
        // processor's device sweep only re-prepares, so details set here would go
        // stale on the next device change rather than track it.
        case TabKind::Plugins:
        {
            double sr; int bs;
            liveDeviceConfig (sr, bs);
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
            double sr; int bs;
            liveDeviceConfig (sr, bs);
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

    // Same rule for the processor listener: detach from EVERYTHING before any
    // of it is destroyed below.
    if (tab.freezeProcListener != nullptr)
    {
        for (auto* p : tab.freezeListenedProcs)
            if (p != nullptr) p->removeListener (tab.freezeProcListener.get());
        tab.freezeListenedProcs.clear();
        tab.freezeProcListener.reset();
    }

    // THREAD SAFETY -- ordering invariant, and it is the whole point of this block.
    // unregisterFromProcessor erases from the dispatcher's task list and frees the
    // RenderTask; the resets below free the engine that task holds.  The audio
    // thread walks that list and runs those tasks every block, so the shield goes
    // up FIRST and the settle drains the in-flight block BEFORE anything is mutated
    // or freed.  A settle placed AFTER the unregister fences nothing -- do not move
    // it back.  shieldWasUp is the nest-aware save/restore idiom used by the Rusty
    // kit load (safe because the shield is only ever raised from the message
    // thread), so an outer project load or closeAllDynamicTabs keeps its own
    // shield.  The brief mute this costs on a teardown gesture is the accepted
    // trade.
    const bool shieldWasUp = mProc.isProjectLoadInProgress();
    mProc.setProjectLoadInProgress (true);
    if (! shieldWasUp)
        mProc.settleAudioThread();

    unregisterFromProcessor (tab);
    if (settleAfterUnregister)
        mProc.settleAudioThread();   // outlast one audio block (page-dtor contract)

    // The chain wrapper holds raw pointers into ownedStages -- it must be
    // destroyed before them.
    tab.engine.reset();
    tab.pedals = nullptr;
    tab.namIr  = nullptr;
    tab.ownedStages.clear();
    tab.engineType.clear();

    mProc.setProjectLoadInProgress (shieldWasUp);
}

void EngineRig::stashPluginRestoreDescription (int pageIndex,
                                               std::unique_ptr<juce::PluginDescription> desc)
{
    // A null desc CLEARS rather than skipping.  The caller resolved no
    // description for this page, and skipping leaves whatever the page's
    // PREVIOUS occupant stashed in place for the select that is about to run --
    // the factory cannot tell that entry apart from one meant for this select.
    if (desc == nullptr)
        mPluginRestoreDescs.erase (pageIndex);
    else
        mPluginRestoreDescs[pageIndex] = std::move (desc);
}

namespace
{
    // FNV-1a.  Chosen over juce::String::hashCode because the inputs are mostly
    // BINARY (an engine's state blob, note structs) rather than text, and this
    // folds bytes in one pass with no allocation.  Collision risk is the usual
    // 32-bit birthday bound -- a false MATCH would reuse a stale render, so the
    // stamp only ever gates REUSE, never correctness of a fresh render.
    inline juce::uint32 fnvByte (juce::uint32 h, juce::uint8 b) noexcept
    { return (h ^ b) * 16777619u; }

    inline juce::uint32 fnvBytes (juce::uint32 h, const void* p, size_t n) noexcept
    {
        const auto* b = static_cast<const juce::uint8*> (p);
        for (size_t i = 0; i < n; ++i) h = fnvByte (h, b[i]);
        return h;
    }

    inline juce::uint32 fnvDouble (juce::uint32 h, double d) noexcept
    {
        // Quantised before hashing: a double that round-trips through XML text
        // can come back a few ULPs off, and an exact byte hash would then call
        // every restored freeze stale -- the precise failure the span's own
        // tolerant compare exists to avoid.
        const juce::int64 q = (juce::int64) std::llround (d * 1000.0);
        return fnvBytes (h, &q, sizeof (q));
    }

    inline juce::uint32 fnvNotes (juce::uint32 h, const std::vector<PianoNote>& notes) noexcept
    {
        h = fnvBytes (h, "n", 1);
        for (const auto& n : notes)
        {
            h = fnvBytes  (h, &n.midiNote, sizeof (n.midiNote));
            h = fnvDouble (h, n.startBeat);
            h = fnvDouble (h, n.durationBeats);
            h = fnvDouble (h, (double) n.velocity);
            h = fnvDouble (h, (double) n.finePitch);
            h = fnvByte   (h, (juce::uint8) (n.muted ? 1 : 0));
            h = fnvByte   (h, (juce::uint8) n.type);
        }
        return h;
    }
}

// The notes THIS tab plays in one pattern.  Mirrors the roll selection in
// BaySickDAWProcessor::patternsWithContentFor -- kept as its own switch rather
// than shared because that one answers "is there content", this one needs the
// content itself.
static const std::vector<PianoNote>* rollNotesFor (const Pattern& pat,
                                                   TabKind kind, int pageIndex)
{
    auto pick = [&] (const auto& arr) -> const std::vector<PianoNote>*
    {
        return (pageIndex >= 0 && pageIndex < (int) arr.size())
             ? &arr[(size_t) pageIndex].notes : nullptr;
    };

    switch (kind)
    {
        case TabKind::Layers:  return pick (pat.layerRoll);
        case TabKind::Bass:    return pick (pat.bassRoll);
        case TabKind::Drums:   return pick (pat.drumRolls);
        case TabKind::Clips:   return pick (pat.clipRoll);
        case TabKind::Vox:     return pick (pat.voxRoll);
        case TabKind::Inst:    return pick (pat.instRoll);
        case TabKind::Plugins: return pick (pat.pluginRoll);
        case TabKind::Rusty:   return &pat.baySickRustyDrumsRoll.notes;
    }
    return nullptr;
}

juce::uint32 EngineRig::freezeContentStamp (TabKind k, int pageIndex, int patternIndex) const
{
    auto* pm = mProc.getPatternManager();
    if (pm == nullptr) return 0;

    const auto* t = findTab (k, pageIndex);

    juce::uint32 h = 2166136261u;   // FNV offset basis

    // 1. WHICH ENGINE, and its entire parameter state.  This is the player axis:
    //    a preset change, a knob move or an engine swap all land here, which is
    //    what makes the stamp catch things the coarse flags only guessed at.
    auto hashProcessorState = [&h] (juce::AudioProcessor* p)
    {
        if (p == nullptr) return;
        juce::MemoryBlock mb;
        p->getStateInformation (mb);
        h = fnvBytes (h, mb.getData(), mb.getSize());
    };

    if (t != nullptr)
    {
        h = fnvBytes (h, t->engineType.toRawUTF8(), (size_t) t->engineType.getNumBytesAsUTF8());
        hashProcessorState (t->engine.get());

        // Inst is a CHAIN: the wrapper's own blob says nothing about the pedal
        // board or amp/IR baked into its freeze -- every owned stage counts.
        for (const auto& stage : t->ownedStages)
            hashProcessorState (stage.get());
    }

    // Processor-owned engines the rig's blob cannot see: the Inst tab's sfizz
    // engine (whichever source mode owns one) and the kit.  Without these the
    // player axis was INVISIBLE to the stamp for exactly those kinds -- a
    // save/reload after any kit or amp change reused the old render.
    if (k == TabKind::Inst)
    {
        hashProcessorState (mProc.getBaySickGuitars (pageIndex));
        hashProcessorState (mProc.getBaySickBasses  (pageIndex));
    }
    if (k == TabKind::Rusty)
        hashProcessorState (mProc.getBaySickRustyDrums());

    // Swing shapes this tab's note timing and lives in the MAIN APVTS, which
    // the per-engine watcher and the engine blob both miss.
    {
        juce::String sp;
        switch (k)
        {
            case TabKind::Layers:  sp = "swing_layer_"  + juce::String (pageIndex); break;
            case TabKind::Bass:    sp = "swing_bass_"   + juce::String (pageIndex); break;
            case TabKind::Drums:   sp = "swing_drum_"   + juce::String (pageIndex); break;
            case TabKind::Inst:    sp = "swing_inst_"   + juce::String (pageIndex); break;
            case TabKind::Plugins: sp = "swing_plugin_" + juce::String (pageIndex); break;
            case TabKind::Rusty:   sp = "swing_rusty";                              break;
            case TabKind::Clips:   break;   // no swing params exist for these two
            case TabKind::Vox:     break;
        }
        if (sp.isNotEmpty())
        {
            if (auto* v = mProc.apvts.getRawParameterValue (sp + "_mix"))
                h = fnvDouble (h, (double) v->load());
            if (auto* v = mProc.apvts.getRawParameterValue (sp + "_trunc"))
                h = fnvDouble (h, (double) v->load());
        }
    }

    // 2. Tempo -- moves every rendered sample in time.  The tempo MAP and the
    //    time-signature map move samples exactly the same way; the stamp saw
    //    neither, so a marker edit reused stale renders on reload.
    h = fnvDouble (h, pm->getGlobalTempo());
    for (int i = 0; i < pm->getNumTempoChanges(); ++i)
    {
        const auto& tc = pm->getTempoChange (i);
        h = fnvBytes  (h, &tc.bar, sizeof (tc.bar));
        h = fnvDouble (h, tc.bpm);
    }
    for (int i = 0; i < pm->getNumTimeSigChanges(); ++i)
    {
        const auto& ts = pm->getTimeSigChange (i);
        h = fnvBytes (h, &ts.bar, sizeof (ts.bar));
        h = fnvBytes (h, &ts.num, sizeof (ts.num));
        h = fnvBytes (h, &ts.den, sizeof (ts.den));
    }

    if (patternIndex >= 0)
    {
        // PATTERN SCOPE: this pattern's notes for this tab, and its length.
        if (patternIndex < pm->getNumPatterns())
        {
            const auto& pat = pm->getPattern (patternIndex);
            if (const auto* notes = rollNotesFor (pat, k, pageIndex))
                h = fnvNotes (h, *notes);
            h = fnvDouble (h, pm->getPatternContentBeats (patternIndex));
        }
        return h;
    }

    // SONG SCOPE: the arrangement decides what plays and when, plus every
    // pattern's notes for this tab (any of them can appear in the arrangement).
    h = fnvDouble (h, pm->getSongEndBeats());

    for (int i = 0; i < pm->getNumBlocks(); ++i)
    {
        const auto& b = const_cast<PatternManager*> (pm)->getBlock (i);
        h = fnvBytes  (h, &b.trackRow,     sizeof (b.trackRow));
        h = fnvBytes  (h, &b.patternIndex, sizeof (b.patternIndex));
        h = fnvDouble (h, effectiveStartBeats  (b));
        h = fnvDouble (h, effectiveLengthBeats (b));
        // A muted block is silence in the render; toggling it must not reuse.
        h = fnvByte   (h, (juce::uint8) (b.muted ? 1 : 0));
    }

    for (int p = 0; p < pm->getNumPatterns(); ++p)
        if (const auto* notes = rollNotesFor (pm->getPattern (p), k, pageIndex))
            h = fnvNotes (h, *notes);

    return h;
}

EngineTab::FreezeSpan EngineRig::currentSongFreezeSpan() const
{
    // Read from the same two values renderFreezeFile renders against -- the
    // arrangement's end and the project tempo -- so a later change to either is
    // detectable rather than the file silently standing in for a length it never
    // covered.
    EngineTab::FreezeSpan s;
    s.patternIndex = EngineTab::kSongScope;

    if (auto* pm = mProc.getPatternManager())
    {
        s.lengthBeats = pm->getSongEndBeats();
        s.bpm         = pm->getGlobalTempo();
    }
    return s;
}

EngineTab::FreezeSpan EngineRig::songFreezeSpanFor (TabKind k, int pageIndex) const
{
    auto s = currentSongFreezeSpan();
    s.contentStamp = freezeContentStamp (k, pageIndex, EngineTab::kSongScope);
    return s;
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
        mProc.settleAudioThread();   // one settle for the whole rig

    for (auto& t : mTabs)
    {
        t->engine.reset();          // chain before stages, same as teardownEngine
        t->pedals = nullptr;
        t->namIr  = nullptr;
        t->ownedStages.clear();
    }
    mTabs.clear();

    // Page indices address nothing once every tab is gone.
    mPluginRestoreDescs.clear();
}
