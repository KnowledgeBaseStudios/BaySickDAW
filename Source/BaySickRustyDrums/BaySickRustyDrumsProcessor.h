#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <map>
#include <memory>

namespace sfz { class Sfizz; }

// ── BaySickRustyDrumsProcessor ───────────────────────────────────────────────
// Phase J BaySickRustyDrums engine.  AudioProcessor wrapper around a single
// sfizz Sfizz instance.  J-2 was the audio shell.  J-3 adds the kit loader:
// `loadKit(juce::File)` calls sfizz's loader and walks the kit's mappings/
// directory to discover the per-sound master `_map.sfz` files.  Each
// discovered map becomes one entry in `mChannels` and (in J-4) one mixer
// strip.  Without a kit loaded, the processor produces silence.
//
// APVTS prefix: brd_  (J-2 has only `brd_outVol`; J-3+ adds kit-driven params)
// ─────────────────────────────────────────────────────────────────────────────
// Channel discovery rule (locked 2026-05-03 J-3):
//   walk `<kitRoot>/Programs/mappings/*_map.sfz` masters; exclude variants
//   matching `*_map_<articulation>.sfz` (basic / brush / mallet / perc / etc).
//   For Big Rusty Drums this resolves to 14 channels (13 drum pieces +
//   `kick_24_nodamp_map.sfz` as the alternate kick variant).
struct KitChannel
{
    juce::String name;        // e.g. "Kick", "Kick (No Damp)", "Snare", "Tom 14", "Hi-hat"
    juce::String mapFilePath; // absolute path to the `_map.sfz` master
    int          midiNote { -1 };  // primary trigger note (-1 if not single-note)
    // J-7b multi-instance: every `_map*.sfz` (master + variants) that
    // belongs to this piece, kit-relative paths (e.g. `mappings/kick_24_map.sfz`).
    // Used to build per-piece SFZ strings at loadKit time.
    std::vector<juce::String> kitRelIncludes;
};

// J-7b (2026-05-03, refactored): one entry per `#define $name <midi>` in the
// kit's `keymap.sfz`.  Notes are reported at their NATIVE MIDI value — the
// piano roll speaks raw kit MIDI (no anchoring, no remap layer).  Variant
// masters (brush/mallet/perc/sidestick/stir/release) all light up here
// because sfizz has them loaded via the parent SFZ's #include chain.
struct PianoRollKey
{
    int          midiNote;        // raw kit-native MIDI from keymap.sfz (24..96)
    juce::String soundName;       // sound type bucket (e.g. "Kick", "Snare", "Tom 14")
    juce::String articulation;    // e.g. "Center", "Sidestick", "Tight Closed", "" if single-note piece
    juce::String defineName;      // raw $name in keymap.sfz (e.g. "kickkey", "sncenterkey")
};
class BaySickRustyDrumsProcessor : public juce::AudioProcessor,
                                    public juce::AudioProcessorValueTreeState::Listener
{
public:
    BaySickRustyDrumsProcessor();
    ~BaySickRustyDrumsProcessor() override;

    // ── AudioProcessor interface ──────────────────────────────────────────────
    void prepareToPlay    (double sampleRate, int maxBlockSize) override;
    void releaseResources () override {}
    void processBlock     (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; } // J-8
    bool hasEditor() const override { return false; }

    const juce::String getName()        const override { return "BaySickRustyDrums"; }
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
    // J-8 stage 2 (2026-05-04): UndoManager declared BEFORE apvts so its
    // address is valid by the time the apvts constructor stores `&mUndoManager`.
    // Without this ordering, `apvts (..., &mUndoManager, ...)` takes the
    // address of an uninitialized member (technically valid, but fragile).
    juce::UndoManager                  mUndoManager;
    juce::AudioProcessorValueTreeState apvts;

    // J-3: loads `sfzPath` (an SFZ file like `01-full.sfz`) into the sfizz
    // engine, then walks the parent kit's `Programs/mappings/` folder to
    // discover per-channel master `_map.sfz` files.  Returns true on success.
    // The discovered channels are exposed via `getChannels()`.
    bool loadKit (const juce::File& sfzPath);

    juce::File                       getCurrentKitPath() const { return mCurrentKitPath; }
    int                              getChannelCount()   const { return (int) mChannels.size(); }
    const std::vector<KitChannel>&   getChannels()       const { return mChannels; }
    const std::vector<PianoRollKey>& getPianoRollKeymap() const { return mPianoRollKeymap; }

    // J-7b: standalone keymap parser exposed for the Help → Rusty Drums Map
    // window so it can populate the map view even when no engine instance
    // has been spawned yet.  Pure file-system + string parsing; no engine
    // state touched.  Pass the kit ROOT (folder containing `Programs/`).
    static std::vector<PianoRollKey> parsePianoRollKeymap (const juce::File& kitRoot)
    { return discoverPianoRollKeymap (kitRoot); }

    // Thread-safe note audition (UI thread → audio thread via atomic).
    // J-8b (2026-05-04): velocity packed into the upper byte of the atomic
    // so the kit-graphic click-position-velocity feature can pass through
    // without a wider message-thread → audio-thread channel.  Default 100
    // matches the previous behavior for legacy callers.
    void auditionNote (int midiNote, int velocity = 100)
    {
        const int v = juce::jlimit (1, 127, velocity);
        const int n = juce::jlimit (0, 127, midiNote);
        mAuditionNote.store ((v << 8) | n);
    }

    // J-8b (2026-05-04): hi-hat pedal state.  Drives CC4 on the sfizz
    // engine (the kit's pedal-position controller — 0 = closed pedal,
    // 127 = open pedal per Big Rusty Drums' cc_ranges).  Callable from
    // the UI thread; sfizz's cc() is documented as message-thread safe
    // when not concurrently rendering.
    void setHiHatPedalClosed (bool closed);
    bool isHiHatPedalClosed() const noexcept { return mHiHatPedalClosed.load(); }

    // J-7b per-strip routing: the engine renders one stereo pair per kit
    // channel into mMultiOutScratch via sfizz's multi-output SFZ routing.
    // PluginProcessor calls processStrips() to fan each strip through its
    // dedicated InsertNode.  After that call, getStripBuffer(i) returns a
    // 2-channel view into the i-th stereo pair (read-only — copy-out).
    // The standard processBlock() path is the standalone fallback (sums
    // every strip into the single stereo `buffer` argument).
    void                    processStrips      (int numFrames, juce::MidiBuffer& midi);
    int                     getStripCount      () const noexcept { return (int) mChannels.size(); }
    juce::AudioBuffer<float> getStripBuffer    (int stripIdx, int numFrames);

    // J-8 stage 2 (2026-05-04): ARIA control surface CC dispatch.  Writes the
    // value (0..127) for `cc` through APVTS so the change is undoable, projectable,
    // and automatable.  The parameterChanged listener forwards to sfizz.
    void sendCc (int cc, int value);
    int  getCcValue (int cc) const;
    int  getKitDefaultCc (int cc) const;   // read-only snapshot of the kit's set_cc<N> values
    juce::String getCcLabel (int cc) const;   // kit's `label_cc<N>=<text>` (empty if none)

    // APVTS listener — forwards every brd_cc<N> change to sfizz.
    void parameterChanged (const juce::String& paramId, float newValue) override;

    // Project-level undo — the editor wires Ctrl+Z to undo()/redo() so panel
    // edits, automation captures, and CC type-in entries are all reversible.
    juce::UndoManager& getUndoManager() noexcept { return mUndoManager; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateFromApvts();

    // J-7b: single sfizz instance with `output=N`-routed wrapper SFZ.
    // sfizz natively supports multi-output by routing regions to numbered
    // output buses based on their `output=` opcode; we synthesize a wrapper
    // SFZ that injects `output=N` into every `<master>` and `<group>` line
    // of every piece's `_map*.sfz` file.  Single instance keeps native
    // SFZ choke groups (off_by) working across pieces.
    std::unique_ptr<sfz::Sfizz> mSfizz;
    juce::File                 mCurrentKitPath;
    std::vector<KitChannel>    mChannels;
    std::vector<PianoRollKey>  mPianoRollKeymap;
    std::atomic<int>           mAuditionNote   { -1 };
    std::atomic<bool>          mHiHatPedalClosed { false };   // J-8b
    double                     mSampleRate     { 48000.0 };
    int                        mMaxBlockSize   { 1024 };

    // Multi-output scratch — sfizz writes 2*stripCount channels per block.
    juce::AudioBuffer<float>   mMultiOutScratch;
    std::vector<float*>        mMultiOutPtrs;   // sfizz expects float**, sized 2*N

    // J-7b synth wrapper builder.  Reads original 01-full.sfz, classifies its
    // mappings/* #include lines by piece (using kSoundTypeRules from the .cpp),
    // and synthesizes a wrapper SFZ that wraps each piece's includes in a
    // <global>output=N block so sfizz routes each piece to its own output.
    juce::String buildOutputRoutedSfzWrapper (const juce::File& sfzPath,
                                              const juce::File& kitRoot);

    // J-3: walks the kit's mappings/ folder.  Returns the discovered list of
    // channels.  Pure file-system scan — does not touch sfizz state.
    // J-8 (2026-05-04): when programSfzPath is non-empty, the program file is
    // parsed and the result is filtered to only include channels whose master
    // `_map.sfz` is referenced (directly or via `_map_<articulation>` variant)
    // by an `#include "mappings/..."` line in that program.  This is what
    // makes Basic spawn fewer mixer strips than Full.
    static std::vector<KitChannel> discoverChannels (const juce::File& kitRoot,
                                                     const juce::File& programSfzPath = {});

    // J-7b (2026-05-03, refactored): build the piano-roll keymap from kitRoot.
    // Parses ONLY `Programs/keymap/keymap.sfz` and emits one PianoRollKey per
    // `#define $name <midi>` entry, sorted ascending by MIDI.  Sound-name +
    // articulation labels come from a $name → (sound, articulation) lookup
    // table.  No anchoring, no remap — piano roll speaks raw kit MIDI.
    static std::vector<PianoRollKey> discoverPianoRollKeymap (const juce::File& kitRoot);

    // J-8 stage 2 (2026-05-04): the live CC values are now stored in APVTS
    // (one brd_cc<N> Int param per MIDI CC).  mCcState mirrors APVTS for
    // legacy callers; mCcKitDefault is a read-only snapshot of the kit's
    // `set_cc<N>=<int>` directives, used for double-click reset on the panel.
    mutable juce::SpinLock     mCcStateLock;
    std::map<int, int>         mCcState;
    mutable juce::SpinLock     mCcKitDefaultLock;
    std::map<int, int>         mCcKitDefault;
    mutable juce::SpinLock     mCcLabelLock;
    std::map<int, juce::String> mCcLabel;        // kit's `label_cc<N>=<text>` lookup

    // ── CPU guard cache ───────────────────────────────────────────────────────
    struct ParamCache
    {
        float outVol { -1.f };
    } mCache;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickRustyDrumsProcessor)
};
