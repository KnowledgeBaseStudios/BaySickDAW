#pragma once
#include <JuceHeader.h>
#include "../Standalone/ApvtsDirtyTracker.h"
#include <atomic>
#include <map>
#include <memory>

namespace sfz { class Sfizz; }

// ── BaySickBassesProcessor ──────────────────────────────────────────────────
// Phase L BaySickBasses engine.  AudioProcessor wrapper around a single sfizz
// Sfizz instance, driven by piano-roll MIDI on the Inst page that owns it.
// Per-instance: every BaySickBasses Inst tab spawns its own processor with a
// unique APVTS prefix `bbb_<instIdx>_*`, so up to 20 instances coexist (the
// 20-cap is shared with BaySickBasses + classic live-input Inst pages).
//
// Single stereo out — basses are melodic single-source instruments, no
// per-piece multi-out routing (that pattern lives in BaySickRustyDrums).
//
// APVTS prefix: bbb_<instIdx>_  (`bbb_<instIdx>_outVol` + 128 `bbb_<instIdx>_cc<N>`)
// ─────────────────────────────────────────────────────────────────────────────

class BaySickBassesProcessor : public juce::AudioProcessor,
                                public juce::AudioProcessorValueTreeState::Listener
{
private:
    // Declared FIRST so they initialize before mUndoManager + apvts below.
    // C++ initializes members in declaration order regardless of init-list
    // order, and `createLayout(mPrefix)` is invoked while constructing apvts —
    // so mPrefix must already be set by that point.
    const int          mInstIdx;
    const juce::String mPrefix;     // "bbb_<instIdx>_"
    const juce::String mCcParamRoot; // "<prefix>cc"

public:
    explicit BaySickBassesProcessor (int instIdx);
    ~BaySickBassesProcessor() override;

    // ── AudioProcessor interface ──────────────────────────────────────────────
    void prepareToPlay    (double sampleRate, int maxBlockSize) override;
    void releaseResources () override {}
    void processBlock     (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; } // L-5
    bool hasEditor() const override { return false; }

    const juce::String getName()        const override { return "BaySickBasses"; }
    bool   acceptsMidi()                const override { return true; }
    bool   producesMidi()               const override { return false; }
    bool   isMidiEffect()               const override { return false; }
    double getTailLengthSeconds()       const override { return 1.0; }

    int  getNumPrograms()                                   override { return 1; }
    int  getCurrentProgram()                                override { return 0; }
    void setCurrentProgram  (int)                           override {}
    const juce::String getProgramName   (int)               override { return "Default"; }
    void changeProgramName  (int, const juce::String&)      override {}

    void getStateInformation (juce::MemoryBlock& dest) override;
    void setStateInformation (const void* data, int sz) override;

    bool isBusesLayoutSupported (const BusesLayout&) const override;

    // ── Public interface ──────────────────────────────────────────────────────
    // UndoManager declared BEFORE apvts so its address is valid by the time the
    // apvts constructor stores `&mUndoManager` (matches BaySickRustyDrums).
    juce::UndoManager                  mUndoManager;
    juce::AudioProcessorValueTreeState apvts;

    // L-1 instance bookkeeping.  `mInstIdx` baked into APVTS prefix at
    // construction; queried by save/load + page wiring.  `mPrefix` =
    // "bbb_<instIdx>_" — used everywhere a param ID is built.
    int          getInstIdx()    const noexcept { return mInstIdx; }
    juce::String getApvtsPrefix() const         { return mPrefix; }

    // Loads `sfzPath` (an SFZ program file) into the sfizz engine, then walks
    // the kit's `#include` chain (depth 4) collecting `set_cc<N>=<int>` defaults
    // into mCcKitDefault and `label_cc<N>=<text>` strings into mCcLabel.  The
    // collected defaults are pushed through APVTS so the parameter listener
    // forwards each value to sfizz at the kit author's intended starting point.
    bool loadKit (const juce::File& sfzPath);

    juce::File getCurrentKitPath() const { return mCurrentKitPath; }

    // Thread-safe note audition (UI thread → audio thread via atomic).
    // Velocity packed into the upper byte; default 100 matches Rusty.
    void auditionNote (int midiNote, int velocity = 100)
    {
        const int v = juce::jlimit (1, 127, velocity);
        const int n = juce::jlimit (0, 127, midiNote);
        mAuditionNote.store ((v << 8) | n);
    }

    // ARIA control surface CC dispatch.  Writes the value (0..127) through
    // APVTS so the change is undoable + automatable + serialized with project
    // state.  The parameterChanged listener forwards to sfizz.
    // 2026-05-05: kCcCount lifted to 512 so kit-author "extended CCs" >= 128
    // get APVTS-bound (no kit Basses currently uses any, but matches Rusty).
    static constexpr int kCcCount = 512;

    void sendCc (int cc, int value);
    int  getCcValue (int cc) const;
    int  getKitDefaultCc (int cc) const;   // read-only snapshot of kit's set_cc<N> values
    juce::String getCcLabel (int cc) const; // kit's `label_cc<N>=<text>` (empty if none)

    // APVTS listener — forwards every <prefix>cc<N> change to sfizz.
    void parameterChanged (const juce::String& paramId, float newValue) override;

    // L-5 fix #5 (2026-05-05): processing gate.  Set to false BEFORE loadKit
    // mutates sfizz hash maps; true AFTER the SFZ is fully parsed.  When false,
    // processBlock early-exits with a cleared buffer instead of calling
    // mSfizz->renderBlock against half-parsed state (which crashes inside
    // sfizz's ControllerSource hash-map insert).
    void setProcessingEnabled (bool b) noexcept { mProcessingEnabled.store (b, std::memory_order_release); }
    bool isProcessingEnabled() const noexcept   { return mProcessingEnabled.load (std::memory_order_acquire); }

    // 2026-05-06 (Option A idle suspend): see BaySickGuitars for rationale.
    int getNumActiveVoices() const noexcept;

    // Project-level undo — editor wires Ctrl+Z to undo()/redo() so panel
    // edits, automation captures, and CC type-in entries are all reversible.
    juce::UndoManager& getUndoManager() noexcept { return mUndoManager; }

    // 2026-05-05 dirty-flag wiring: every edit to this engine's apvts.state
    // (knob drag, automation, kit-load CC defaults push, replaceState) fires
    // onAny.  StandaloneEditor wires it to ProjectManager::markDirty so the
    // title-bar "*" reflects engine-internal state changes.
    void setOnAnyStateChange (std::function<void()> fn) { mDirtyTracker.onAny = std::move (fn); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout (const juce::String& prefix);
    void updateFromApvts();

    std::unique_ptr<sfz::Sfizz> mSfizz;
    juce::File                  mCurrentKitPath;
    std::atomic<int>            mAuditionNote { -1 };
    // L-5 fix #5: processing-enabled gate (default true so first construction
    // works; PluginProcessor's loadBaySickBassesKit wrapper flips false→load→true).
    std::atomic<bool>           mProcessingEnabled { true };
    double                      mSampleRate   { 48000.0 };
    int                         mMaxBlockSize { 1024 };

    // Single stereo render scratch — sfizz writes 2 channels per block.
    juce::AudioBuffer<float>    mRenderScratch;
    std::vector<float*>         mRenderPtrs;   // sized 2; refreshed per block

    // CC mirror state.  Live values stored in APVTS (one <prefix>cc<N> Int
    // param per MIDI CC); mCcKitDefault is a read-only snapshot of the kit's
    // `set_cc<N>=<int>` directives, used for double-click reset on the panel.
    mutable juce::SpinLock      mCcKitDefaultLock;
    std::map<int, int>          mCcKitDefault;
    mutable juce::SpinLock      mCcLabelLock;
    std::map<int, juce::String> mCcLabel;       // kit's `label_cc<N>=<text>` lookup

    // ── CPU guard cache ───────────────────────────────────────────────────────
    struct ParamCache
    {
        float outVol { -1.f };
    } mCache;

    // 2026-05-05 dirty-flag wiring.  Declared LAST so apvts is fully
    // constructed by the time the tracker installs its ValueTree listener.
    ApvtsDirtyTracker mDirtyTracker { apvts };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickBassesProcessor)
};
