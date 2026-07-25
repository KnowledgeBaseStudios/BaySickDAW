#pragma once
#include <JuceHeader.h>
#include "../Standalone/ApvtsDirtyTracker.h"
#include "../SlideSampler/SlideRegionMap.h"
#include "../SlideSampler/SlideSampler.h"
#include <array>
#include <atomic>
#include <bitset>
#include <map>
#include <memory>

namespace sfz { class Sfizz; }

// ── BaySickGuitarsProcessor ──────────────────────────────────────────────────
// Phase K BaySickGuitars engine.  AudioProcessor wrapper around a single sfizz
// Sfizz instance, driven by piano-roll MIDI on the Inst page that owns it.
// Per-instance: every BaySickGuitars Inst tab spawns its own processor with a
// unique APVTS prefix `bgg_<instIdx>_*`, so up to 20 instances coexist (the
// 20-cap is shared with BaySickBasses + classic live-input Inst pages).
//
// Single stereo out - guitars are melodic single-source instruments, no
// per-piece multi-out routing (that pattern lives in BaySickRustyDrums).
//
// APVTS prefix: bgg_<instIdx>_  (`bgg_<instIdx>_outVol` + 128 `bgg_<instIdx>_cc<N>`)
// ─────────────────────────────────────────────────────────────────────────────

class BaySickGuitarsProcessor : public juce::AudioProcessor,
                                public juce::AudioProcessorValueTreeState::Listener
{
private:
    // Declared FIRST so they initialize before mUndoManager + apvts below.
    // C++ initializes members in declaration order regardless of init-list
    // order, and `createLayout(mPrefix)` is invoked while constructing apvts -
    // so mPrefix must already be set by that point.
    const int          mInstIdx;
    const juce::String mPrefix;     // "bgg_<instIdx>_"
    const juce::String mCcParamRoot; // "<prefix>cc"

public:
    explicit BaySickGuitarsProcessor (int instIdx);
    ~BaySickGuitarsProcessor() override;

    // ── AudioProcessor interface ──────────────────────────────────────────────
    void prepareToPlay    (double sampleRate, int maxBlockSize) override;
    void releaseResources () override {}
    void processBlock     (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; } // K-5
    bool hasEditor() const override { return false; }

    const juce::String getName()        const override { return "BaySickGuitars"; }
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

    // K-1 instance bookkeeping.  `mInstIdx` baked into APVTS prefix at
    // construction; queried by save/load + page wiring.  `mPrefix` =
    // "bgg_<instIdx>_" - used everywhere a param ID is built.
    int          getInstIdx()    const noexcept { return mInstIdx; }
    juce::String getApvtsPrefix() const         { return mPrefix; }

    // Loads `sfzPath` (an SFZ program file) into the sfizz engine, then walks
    // the kit's `#include` chain (depth 4) collecting `set_cc<N>=<int>` defaults
    // into mCcKitDefault and `label_cc<N>=<text>` strings into mCcLabel.  The
    // collected defaults are pushed through APVTS so the parameter listener
    // forwards each value to sfizz at the kit author's intended starting point.
    bool loadKit (const juce::File& sfzPath);

    juce::File getCurrentKitPath() const { return mCurrentKitPath; }

    // QA-SlideSampler: the extracted blended-slide sustain table for the loaded
    // program (empty if the program has no default-keyswitch sustain).  Built in
    // loadKit; the SlideSampler (Task 2) reads it for the RP Slide gesture.
    const SlideRegionMap& getSlideRegions() const noexcept { return mSlideRegions; }

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
    // get APVTS-bound (no kit Guitars currently uses any, but matches Rusty).
    static constexpr int kCcCount = 512;

    void sendCc (int cc, int value);
    int  getCcValue (int cc) const;
    int  getKitDefaultCc (int cc) const;   // read-only snapshot of kit's set_cc<N> values
    juce::String getCcLabel (int cc) const; // kit's `label_cc<N>=<text>` (empty if none)

    // APVTS listener - forwards every <prefix>cc<N> change to sfizz.
    void parameterChanged (const juce::String& paramId, float newValue) override;

    // K-5 fix #5 (2026-05-05): processing gate.  Set to false BEFORE loadKit
    // mutates sfizz hash maps; true AFTER the SFZ is fully parsed.  When false,
    // processBlock early-exits with a cleared buffer instead of calling
    // mSfizz->renderBlock against half-parsed state (which crashes inside
    // sfizz's ControllerSource hash-map insert).
    void setProcessingEnabled (bool b) noexcept { mProcessingEnabled.store (b, std::memory_order_release); }
    bool isProcessingEnabled() const noexcept   { return mProcessingEnabled.load (std::memory_order_acquire); }

    // 2026-05-06 (Option A idle suspend): expose sfizz active-voice count so
    // PluginProcessor's per-tab dispatch can decide whether to skip the
    // entire chain for this block.  Returns 0 when sfizz isn't ready or
    // processing is disabled - safe default for the suspend gate.
    int getNumActiveVoices() const noexcept;

    // QA-C DSP-10 (2026-05-10): audition-pending peek so the idle-suspend
    // predicate wakes the chain when an audition note is queued via
    // auditionNote(int).  Without this, the dispatcher skips processBlock
    // and the audition is silently swallowed.
    bool isAuditionPending() const noexcept
    {
        return mAuditionNote.load (std::memory_order_acquire) != -1;
    }

    // QA-G3Smoke Task 12: slide-tail peek for the same idle-suspend predicate.
    // SlideSampler voices (gesture + ring-out tails + release zones) don't
    // count in sfizz's getNumActiveVoices, so without this term the dispatcher
    // suspends the chain mid-tail and the ring-out freezes.
    bool isSlideActive() const noexcept { return mSlideSampler.isActive(); }

    // Project-level undo - editor wires Ctrl+Z to undo()/redo() so panel
    // edits, automation captures, and CC type-in entries are all reversible.
    juce::UndoManager& getUndoManager() noexcept { return mUndoManager; }

    // 2026-05-05 dirty-flag wiring: every edit to this engine's apvts.state
    // (knob drag, automation, kit-load CC defaults push, replaceState) fires
    // onAny.  StandaloneEditor wires it to ProjectManager::markDirty so the
    // title-bar "*" reflects engine-internal state changes.
    void setOnAnyStateChange (std::function<void()> fn) { mDirtyTracker.onAny = std::move (fn); }

    // QA-Sfizz Task 2B: keyswitch label accessor for piano-roll discoverability.
    // Iterates sfizz's parsed sw_label storage (populated at SFZ load time from
    // <region>/<group> sw_label opcodes); returns the label for the given MIDI
    // note if it's a keyswitch in the currently loaded kit, else empty.
    juce::String           getKeyswitchLabel (int midiNote) const noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout (const juce::String& prefix);
    void updateFromApvts();

    // QA-SlideSampler Task 3: arm the blended slide from the RP Slide transport
    // (CC84 anchor / CC5+37 glide ms / CC86 loudness / CC85 target) - suppresses
    // the sfizz anchor voice and hands the gesture to mSlideSampler.
    void armSlide (int anchor, int target, int timeMs, int velocity, int delaySample);

    // QA-SlideSampler Task 4: native "Bend" note.  A noteOn + CC87/88/5/37 arms a
    // pitch-wheel ramp scaled to the patch's real range; mBendNote's noteOff ends it.
    void armBend (int note, int semis, int shape, int timeMs);

    // RP Slide gesture state (audio thread).  The anchor->target pitch ramp is
    // advanced per sub-block in processBlock; mSlideAnchor's noteOff ends it.
    bool   mSlideActive    = false;
    int    mSlideAnchor    = -1;
    double mSlidePitch     = 0.0;
    double mSlideTarget    = 0.0;
    double mSlidePitchStep = 0.0;
    int    mSlideArmSample = 0;

    // Bend gesture state (audio thread).  Wheel ramp advanced per block.
    bool   mBendActive       = false;
    int    mBendNote         = -1;
    int    mBendTargetWheel  = 0;    // -8192..+8191 (0 = center)
    int    mBendShape        = 0;
    int    mBendSamplesTotal = 1;
    int    mBendSamplesDone  = 0;

    // Task 12 (#3): last noteOn velocity per note (audio thread) -- armSlide
    // anchors the slide loudness to the ANCHOR NOTE's velocity, with the CC86
    // transport value as the no-prior-noteOn fallback.
    std::array<int, 128> mLastNoteVel {};

    // Task 12 (#7): notes struck THIS block.  A CC85 arm whose anchor was
    // noteOn'd in the same block is a COPIED slide (fresh re-plucked gesture),
    // never a chain continuation -- chains hold ONE anchor noteOn across all
    // segments.  Cleared at the top of every MIDI pass.
    std::bitset<128> mNoteOnThisBlock;

    // Task 12 (G-12): cut-self param atomics, cached at construction for
    // lock-free audio-thread reads.  cutSelfMode false = Same Pitch, true =
    // Cut All (mirrors the QA-CutSelfReview semantics on the synth engines).
    std::atomic<float>* mCutSelfRaw     = nullptr;
    std::atomic<float>* mCutSelfModeRaw = nullptr;

    // Task 12: per-CC APVTS atomics, cached once at construction so the
    // SlideSampler's block-rate CC provider reads lock-free (no string-keyed
    // map lookups on the audio thread).
    std::array<std::atomic<float>*, kCcCount> mCcRaw {};

    std::unique_ptr<sfz::Sfizz> mSfizz;
    juce::File                  mCurrentKitPath;
    SlideRegionMap              mSlideRegions;   // QA-SlideSampler: rebuilt each loadKit
    SlideSampler                mSlideSampler;   // QA-SlideSampler: blended-slide DSP
    std::atomic<int>            mAuditionNote { -1 };
    // K-5 fix #5: processing-enabled gate (default true so first construction
    // works; PluginProcessor's loadBaySickGuitarsKit wrapper flips false→load→true).
    std::atomic<bool>           mProcessingEnabled { true };
    double                      mSampleRate   { 48000.0 };
    int                         mMaxBlockSize { 1024 };

    // Single stereo render scratch - sfizz writes 2 channels per block.
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickGuitarsProcessor)
};
