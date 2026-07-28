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
        teardownEngine (*mTabs[i], /*settleAfterUnregister=*/ true);
        mTabs.erase (mTabs.begin() + (long) i);
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

    registerWithProcessor (*tab);
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

    switch (tab.kind)
    {
        case TabKind::Layers: mProc.registerLayerEngine (tab.pageIndex, eng); break;
        case TabKind::Bass:   mProc.registerBassEngine  (tab.pageIndex, eng); break;
        case TabKind::Drums:  mProc.registerDrumEngine  (tab.pageIndex, eng); break;

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
    }
}

void EngineRig::teardownEngine (EngineTab& tab, bool settleAfterUnregister)
{
    if (tab.engine == nullptr) return;

    if (onEngineDestroying) onEngineDestroying (tab);
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
