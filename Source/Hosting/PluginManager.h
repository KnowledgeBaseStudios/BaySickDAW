#pragma once
#include <JuceHeader.h>

// ── Hosting::PluginManager ───────────────────────────────────────────────────
// QA-ModelShell TS6 (BLU-298 + BLU-299): the model-side owner of everything
// about hosted plugins that is not a plugin INSTANCE -- which folders to scan,
// what the last scan found, and the "added list" the user curated out of it.
//
// Model-side rather than editor-side because the added list has consumers on
// both sides: the FX rack picker and the Plugins "+" menu read it to build
// their menus, and project load reads it while re-instantiating saved plugin
// slots -- which happens with no editor guaranteed to exist.  Same reasoning
// that put EngineRig on the processor at TS1.
//
// FORMAT SCOPE IS VST3 ONLY, deliberately (Jeff 2026-07-29, after a licensing
// review; full finding in Future State CL-303).  JUCE's VST2 host path needs
// Steinberg SDK headers that JUCE does not ship and that are absent from this
// tree, and the VST2 SDK has been unlicensable since October 2018.  The format
// list below is therefore hard-coded rather than addDefaultFormats().
// ─────────────────────────────────────────────────────────────────────────────

namespace Hosting
{

// The architecture a plugin binary was built for.
//
// Determined WITHOUT loading the binary, and that is the whole point: a 64-bit
// process physically cannot LoadLibrary a 32-bit image, so probing by loading
// would report every 32-bit plugin as "broken" when the truth is "needs the
// bridge".  Those two have to stay distinguishable -- one is a defect, the
// other is a supported configuration waiting on BLU-302.
enum class PluginArch
{
    Unknown,
    X86,
    X64
};

// A candidate the scan found but did NOT put on the addable results list.
//
// Jeff's spec 2026-07-29: a scan NEVER silently drops a plugin.  "My plugin
// isn't in the list" has to be answerable, and under VST3-only scope a user's
// old VST2 freeware is precisely what turns up and cannot load -- so
// "Skipped: VST2 is not supported" is the difference between an explanation
// and an apparent bug.
struct SkippedPlugin
{
    juce::String path;
    juce::String reason;
};

class PluginManager  : public  juce::ChangeBroadcaster,
                       private juce::Thread,
                       private juce::AsyncUpdater
{
public:
    PluginManager();
    ~PluginManager() override;

    // ── Section 1 — scan folders ────────────────────────────────────────────
    juce::FileSearchPath getScanFolders() const;
    bool addScanFolder    (const juce::File&);
    void removeScanFolder (int index);
    void resetScanFoldersToDefaults();

    // The standard locations VST3s install to.  Comes straight from JUCE's own
    // VST3PluginFormat rather than a hand-kept list, so it cannot drift.
    static juce::FileSearchPath defaultScanFolders();

    // ── Section 2 — the added list ──────────────────────────────────────────
    // The ONE list every consuming surface reads.  Both filtered getters sort
    // alphabetically here so callers cannot each get the ordering slightly
    // different -- the picker group and the "+" dropdown are both specced
    // alphabetical (Jeff 2026-07-29).
    juce::Array<juce::PluginDescription> getAddedPlugins()     const;
    juce::Array<juce::PluginDescription> getAddedEffects()     const;
    juce::Array<juce::PluginDescription> getAddedInstruments() const;

    void addToAddedList      (const juce::Array<juce::PluginDescription>&);
    void removeFromAddedList (const juce::PluginDescription&);

    // v3: a 32-bit plugin's stored description is a filename-only guess (this
    // 64-bit process cannot open the file), so the first successful bridged
    // load reports what it actually is and this persists the correction.
    // Message thread.
    void refineDescription   (const juce::PluginDescription&);

    // Resolves a saved slot back to its description.  Keyed on
    // PluginDescription::createIdentifierString(), which is stable across
    // sessions and machines -- the stable-id discipline the binding research
    // called for, with REAPER's positional addressing as the anti-pattern.
    std::unique_ptr<juce::PluginDescription> findAdded (const juce::String& identifier) const;

    // ── Section 3 — scan results ────────────────────────────────────────────
    void  startScan();
    void  cancelScan();
    bool  isScanning()      const noexcept;
    bool  hasScanned()      const noexcept;
    float getScanProgress() const noexcept;
    juce::String getScanStatusText() const;

    // Found by the last scan and NOT already on the added list.
    juce::Array<juce::PluginDescription> getScanResults() const;
    juce::Array<SkippedPlugin>           getSkipped()     const;

    // Fired on the MESSAGE THREAD (marshalled through AsyncUpdater) -- the scan
    // itself runs on this object's own thread and never touches a Component.
    std::function<void()> onScanProgress;
    std::function<void()> onScanFinished;
    std::function<void()> onAddedListChanged;

    // The added list also broadcasts through juce::ChangeBroadcaster, because
    // onAddedListChanged has exactly ONE owner (the plugin manager window claims
    // it and clears it on close) and the added list has a second consumer: a
    // rack slot whose plugin was missing re-attempts its load when the user puts
    // the plugin back.  ChangeBroadcaster delivers on the MESSAGE THREAD, which
    // is what keeps that re-instantiation off the audio thread.

    juce::AudioPluginFormatManager& formats() noexcept { return mFormats; }

    // The app-wide instance.  Same scope justification as
    // DSPBase::sTransportPlaying: there is one BaySickDAWProcessor per app, and
    // EffectRack's factory is a plain EffectType switch with no route back to
    // the processor -- so a VST3 rack slot has no other way to reach the format
    // manager.  Set and cleared by this class's own ctor/dtor.
    static PluginManager* getInstance() noexcept;

    // Reads the PE machine field (or, for a VST3 bundle folder, the
    // Contents/<arch>-win layout).  Accepts either shape because JUCE's
    // searchPathsForPlugins returns whichever the plugin ships as.
    static PluginArch architectureOf (const juce::File& pluginPath);

private:
    void run() override;                 // juce::Thread — the scan
    void handleAsyncUpdate() override;   // juce::AsyncUpdater — UI notify

    void loadFromDisk();
    void saveToDisk() const;

    static juce::File dataFile();
    static juce::File deadMansPedalFile();

    juce::AudioPluginFormat* vst3Format() const noexcept;

    // Guards every member below it.  Written by the scan thread, read by the
    // message thread; all public getters copy out under it.
    mutable juce::CriticalSection        mLock;
    juce::FileSearchPath                 mScanFolders;
    juce::KnownPluginList                mAdded;
    juce::Array<juce::PluginDescription> mScanResults;
    juce::Array<SkippedPlugin>           mSkipped;
    juce::String                         mStatusText;

    juce::AudioPluginFormatManager mFormats;

    std::atomic<float> mProgress      { 0.0f };
    std::atomic<bool>  mHasScanned    { false };
    std::atomic<bool>  mPendingFinish { false };

    // One-shot gate so a persistent write failure cannot stack an alert per
    // add/remove gesture; atomic + mutable because saveToDisk() is const.
    mutable std::atomic<bool> mWarnedSaveFailure { false };

    inline static std::atomic<PluginManager*> sInstance { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginManager)
};

} // namespace Hosting
