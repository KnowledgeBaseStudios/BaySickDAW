#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

// ── VibeRegion ────────────────────────────────────────────────────────────────
// One zone in the sample map.  Pre-loaded audio lives in audioData to avoid
// disk I/O on the audio thread during note-on.
struct VibeRegion
{
    // Zone mapping
    int    rootNote          { 60 };   // MIDI pitch-accurate root
    int    loNote            { 0 };
    int    hiNote            { 127 };
    int    loVel             { 0 };
    int    hiVel             { 127 };
    int    roundRobinIndex   { 0 };    // 0 = any; 1-N = position in RR sequence
    int    roundRobinTotal   { 0 };    // 0 = no RR; N = sequence length
    int    articulationGroup { 0 };    // 0-3 → A/B/C/D

    // Tuning & level offsets
    float  tuneOffset        { 0.0f }; // cents (can be fractional)
    float  volumeOffset      { 0.0f }; // dB

    // SFZ keyswitching (sw_*).  Default -1 = not set; an unset field doesn't
    // filter (region is candidate-eligible regardless of that condition).
    int    swLokey           { -1 };   // sw_lokey: keyswitch range low (MIDI note)
    int    swHikey           { -1 };   // sw_hikey: keyswitch range high (MIDI note)
    int    swLast            { -1 };   // sw_last: region active when active keyswitch == swLast
    int    swDown            { -1 };   // sw_down: region active while swDown note is held
    int    swUp              { -1 };   // sw_up: region active while swUp note is NOT held
    int    swDefault         { -1 };   // sw_default: initial keyswitch (priority init in parseSFZ)
    juce::String swLabel;              // sw_label: human-readable name for this region's keyswitch

    // Pre-loaded audio (shared_ptr so voices can safely outlive a reload)
    std::shared_ptr<juce::AudioBuffer<float>> audioData;
    double fileSampleRate    { 44100.0 };
    juce::File sampleFile;
};

// ── VibeSampleManager ─────────────────────────────────────────────────────────
// Loads regions from a folder or SFZ file (message thread only).
// findRegion() is called from the audio thread - reads only, safe once loaded.
//
// Disk I/O:  loadFolder / loadSFZ  (message thread, may block briefly)
// Audio use: findRegion             (audio thread, lock-free reads)
class VibeSampleManager
{
public:
    VibeSampleManager();

    // Clear and reload from a folder of WAV/AIFF files.
    // Root note detected from filename heuristics.
    void loadFolder (const juce::File& folder);

    // Clear and reload from an SFZ instrument file.
    void loadSFZ    (const juce::File& sfzFile);

    // Load a single audio file as a one-shot instrument (rootNote always 60).
    void loadSingleFile (const juce::File& file);

    // Remove all regions and free audio data.
    void clear();

    // Force all regions to the same root note (use for drum pads where pitch shifting is unwanted).
    void normalizeRootNotes (int midiNote) { for (auto& r : mRegions) r.rootNote = midiNote; }

    // Find the best matching region (audio thread safe).
    // Round-robin state is updated here (per note-per-artic).
    // Returns nullptr if nothing matches.
    const VibeRegion* findRegion (int midiNote, int velocity, int articulationGroup);

    // For UI / state display
    juce::File                      getLoadedFolder()  const { return mLoadedFolder; }
    const std::vector<VibeRegion>&  getRegions()       const { return mRegions; }
    bool                            hasAnyRegions()    const { return !mRegions.empty(); }

    // AudioFormatManager (shared with voices for creating readers)
    juce::AudioFormatManager& getFormatManager() { return mFormatManager; }

    // ── SFZ keyswitching (Sub-N: keyswitch state lives here) ──────────────────
    // Engine queries isKeyswitchNote() on every incoming MIDI note in
    // VibePlayerProcessor::processBlock; if true, the event is stripped from
    // the MIDI buffer + routed to handleKeyswitchNoteOn/Off instead of being
    // dispatched to the synth.  Single-threaded (audio thread only).
    bool isKeyswitchNote        (int midiNote) const noexcept;
    void handleKeyswitchNoteOn  (int midiNote)       noexcept;
    void handleKeyswitchNoteOff (int midiNote)       noexcept;

    // Sub-Q: human-readable label for a keyswitch note (from the SFZ file's
    // sw_label opcode).  Returns empty string for non-keyswitch notes or
    // unlabeled keyswitches.  Used by the piano keyboard to render highlight
    // + label on the keyswitch keys (BaySickRustyDrumsKitGraphic.cpp:665
    // precedent for visual style).
    juce::String getKeyswitchLabel (int midiNote) const noexcept;

private:
    // Load one file into an AudioBuffer and return its sample rate.
    static std::shared_ptr<juce::AudioBuffer<float>>
        loadFile (const juce::File& f, juce::AudioFormatManager& fmt, double& sampleRateOut);

    // Heuristic root-note detection from a filename string.
    static int  detectRootNote  (const juce::String& filename);
    static int  noteNameToMidi  (const juce::String& name);   // "C4", "Bb3", "F#5"

    // SFZ parsing helpers
    void parseSFZ (const juce::File& sfzFile);
    static juce::String sfzOpcode (const juce::String& line, const char* key,
                                    bool readToEOL = false);
    static int          sfzNote   (const juce::String& val);  // accepts "C4" or "60"

    juce::AudioFormatManager  mFormatManager;
    std::vector<VibeRegion>   mRegions;
    juce::File                mLoadedFolder;

    // Per-[note][artic] round-robin counter (audio thread only)
    int mRRCounters[128][4] {};

    // ── SFZ keyswitching state (Sub-N) ────────────────────────────────────────
    // mIsKeyswitch is populated at SFZ load time from the union of sw_lokey..
    // sw_hikey ranges across all loaded regions.  mActiveSwLast tracks the most
    // recent keyswitch note pressed (for sw_last filtering in findRegion).
    // mSwDownHeld tracks per-note "is this keyswitch currently held" state for
    // sw_down / sw_up filtering.  All single-threaded (audio thread only).
    int                          mActiveSwLast     { -1 };
    std::array<bool, 128>        mSwDownHeld       {};
    std::array<bool, 128>        mIsKeyswitch      {};
    std::array<juce::String, 128> mKeyswitchLabels {};

    void resetKeyswitchState() noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VibeSampleManager)
};

// ── VibeSynthSound ────────────────────────────────────────────────────────────
// Trivial sound - all notes accepted; region lookup is in the voice.
struct VibeSynthSound : public juce::SynthesiserSound
{
    bool appliesToNote    (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

// ── VibeForwardMemoryAudioSource ──────────────────────────────────────────────
// Fat forward reader of a shared AudioBuffer (QA-VoicePool Task 2: replaces the
// per-note-on `new juce::MemoryAudioSource` allocation).  Owned permanently by
// VibeVoice; startNote re-points at the new region's buffer via setBuffer().
// Null buffer is the resting state (between notes) - getNextAudioBlock clears
// the destination region and returns when mBuf is null.
class VibeForwardMemoryAudioSource : public juce::PositionableAudioSource
{
public:
    VibeForwardMemoryAudioSource() noexcept = default;

    void setBuffer (const juce::AudioBuffer<float>& buf) noexcept
    {
        mBuf = &buf;
        mPos = 0;
    }

    void prepareToPlay (int, double) override {}
    void releaseResources ()          override {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        if (mBuf == nullptr)
        {
            info.clearActiveBufferRegion();
            return;
        }

        const int total   = mBuf->getNumSamples();
        const int srcChs  = mBuf->getNumChannels();
        const int dstChs  = info.buffer->getNumChannels();
        if (srcChs <= 0) return;

        for (int i = 0; i < info.numSamples; ++i)
        {
            const juce::int64 readIdx = mPos + i;
            if (readIdx >= (juce::int64) total)
            {
                for (int c = 0; c < dstChs; ++c)
                    info.buffer->setSample (c, info.startSample + i, 0.f);
            }
            else
            {
                for (int c = 0; c < dstChs; ++c)
                {
                    const int srcChan = c % srcChs;   // wrap mono -> L+R (matches juce::MemoryAudioSource)
                    info.buffer->setSample (c, info.startSample + i,
                                            mBuf->getSample (srcChan, (int) readIdx));
                }
            }
        }
        mPos = juce::jmin ((juce::int64) total, mPos + info.numSamples);
    }

    juce::int64 getNextReadPosition () const override                 { return mPos; }
    void        setNextReadPosition (juce::int64 p)      override
    {
        mPos = (mBuf != nullptr)
            ? juce::jlimit ((juce::int64) 0, (juce::int64) mBuf->getNumSamples(), p)
            : 0;
    }
    juce::int64 getTotalLength      () const override                 { return mBuf != nullptr ? mBuf->getNumSamples() : 0; }
    bool        isLooping           () const override                 { return false; }

private:
    const juce::AudioBuffer<float>* mBuf { nullptr };
    juce::int64 mPos { 0 };
};

// ── ReversedMemoryAudioSource ─────────────────────────────────────────────────
// Fat reverse reader of a shared AudioBuffer (QA-VoicePool Task 2 moved this
// from VibePlayerDSP.cpp anon-namespace to enable direct VibeVoice membership;
// original implementation S1 Incr3 2026-04-21).  Position semantics match the
// forward source (0 = start of playback = last sample of buffer), so
// sample-start seek works without special-casing the caller.
class ReversedMemoryAudioSource : public juce::PositionableAudioSource
{
public:
    ReversedMemoryAudioSource() noexcept = default;

    explicit ReversedMemoryAudioSource (const juce::AudioBuffer<float>& buf) noexcept
        : mBuf (&buf), mPosFwd (0) {}

    void setBuffer (const juce::AudioBuffer<float>& buf) noexcept
    {
        mBuf = &buf;
        mPosFwd = 0;
    }

    void prepareToPlay (int, double) override {}
    void releaseResources ()          override {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        if (mBuf == nullptr)
        {
            info.clearActiveBufferRegion();
            return;
        }

        const int total   = mBuf->getNumSamples();
        const int srcChs  = mBuf->getNumChannels();
        const int dstChs  = info.buffer->getNumChannels();
        if (srcChs <= 0) return;

        for (int i = 0; i < info.numSamples; ++i)
        {
            const juce::int64 fwdPos  = mPosFwd + i;
            const juce::int64 readIdx = (juce::int64) total - 1 - fwdPos;
            if (readIdx < 0 || fwdPos >= (juce::int64) total)
            {
                for (int c = 0; c < dstChs; ++c)
                    info.buffer->setSample (c, info.startSample + i, 0.f);
            }
            else
            {
                for (int c = 0; c < dstChs; ++c)
                {
                    const int srcChan = c % srcChs;   // wrap mono -> L+R
                    info.buffer->setSample (c, info.startSample + i,
                                            mBuf->getSample (srcChan, (int) readIdx));
                }
            }
        }
        mPosFwd = juce::jmin ((juce::int64) total, mPosFwd + info.numSamples);
    }

    juce::int64 getNextReadPosition () const override                 { return mPosFwd; }
    void        setNextReadPosition (juce::int64 p)      override
    {
        mPosFwd = (mBuf != nullptr)
            ? juce::jlimit ((juce::int64) 0, (juce::int64) mBuf->getNumSamples(), p)
            : 0;
    }
    juce::int64 getTotalLength      () const override                 { return mBuf != nullptr ? mBuf->getNumSamples() : 0; }
    bool        isLooping           () const override                 { return false; }

private:
    const juce::AudioBuffer<float>* mBuf { nullptr };
    juce::int64 mPosFwd { 0 };   // forward-time position (0 = first sample emitted)
};

// ── VibeVoice ─────────────────────────────────────────────────────────────────
// One polyphonic voice.
//
//   startNote   → find region → re-point fat sources → reset ADSR / filter / LFO
//   renderNextBlock → get audio → ADSR → drive → SVF filter → mix into output
//   stopNote    → ADSR note-off (tail-off) or immediate clear
//
// CPU safeguarding: parameter setters check cached values before updating DSP.
class VibeVoice : public juce::SynthesiserVoice
{
public:
    explicit VibeVoice (VibeSampleManager& manager);
    ~VibeVoice() override;

    // ── SynthesiserVoice interface ─────────────────────────────────────────────
    void setCurrentPlaybackSampleRate (double newRate) override;
    bool canPlaySound    (juce::SynthesiserSound*)                     override;
    void startNote       (int midiNote, float velocity,
                          juce::SynthesiserSound*,
                          int pitchWheelPos)                           override;
    void stopNote        (float velocity, bool allowTailOff)           override;
    void pitchWheelMoved (int newValue)                                override {}
    // Batch E #2 (2026-05-01): CC74 = per-note filter cutoff offset
    // (-2..+2 octaves multiplied into effCutoff in renderNextBlock).
    void controllerMoved (int controllerNumber, int newValue)          override
    {
        if (controllerNumber == 74)
        {
            const float norm = (float) newValue / 127.0f;
            mPerNoteCutoffOctaves = (norm - 0.5f) * 4.0f;
        }
    }
    void renderNextBlock (juce::AudioBuffer<float>& buf,
                          int startSample, int numSamples)             override;

    // ── Lifecycle helper called by VibeSynth::prepare ─────────────────────────
    void prepareForPlayback (int blockSize);

    // ── Parameter setters (CPU guarded - call from audio thread only) ──────────
    void setFilterParams   (float cutoffHz, float q)            noexcept;
    void setDrive          (float drive)                        noexcept;
    void setReduct         (float reduct)                       noexcept;
    void setVolume         (float vol)                          noexcept;
    void setPan            (float pan)                          noexcept;
    void setAdsr           (float a, float d, float s, float r) noexcept;
    void setLfoAmt         (float amt)                          noexcept;
    void setStretch        (float stretch)                      noexcept;
    void setMuffle         (float muffle)                       noexcept;
    void setVelToMuffle    (float amt)                          noexcept;
    void setHardness       (float hardness)                     noexcept;
    void setVelToHardness  (float amt)                          noexcept;
    void setSensitivity    (float sens)                         noexcept;
    void setArticulationGroup (int group)                       noexcept { mArticGroup = group; }
    // S1 2026-04-21:
    void setTune           (float semitones)                    noexcept;
    void setVelToVolume    (float amt)                          noexcept;
    void setSampleStart    (float norm)                         noexcept;
    void setLfoRate        (float hz)                           noexcept;
    void setReverse        (bool rev)                           noexcept { mReverse = rev; }
    // Per-note unison tag applied on the next startNote (set by VibeSynth during fan-out):
    void setNextUnisonCents (float cents)                       noexcept { mNextUnisonCents = cents; }
    // Age tracking for voiceCap-based stealing
    juce::uint32 getNoteStartCounter() const noexcept           { return mNoteStartCounter; }

    // ── QA-VoicePool Task 3 — lock-free occupancy + L7(b) hybrid stealing ─────
    // mIsActive is the supplemental atomic flag scanned by VibeSynth::findStealCandidate
    // to avoid dynamic_cast loops on the hot audio path (Sub-A=(a)).  Set true at
    // startNote, cleared at ADSR-end or hard stopNote.  mInRelease tracks whether the
    // voice's amp envelope is in its release phase (used by L7(b) hybrid: prefer
    // release-phase oldest as steal victim, fallback to overall-oldest).
    bool isActive()        const noexcept { return mIsActive.load (std::memory_order_acquire); }
    bool isInRelease()     const noexcept { return mInRelease; }
    // QA-VoicePool Task 3 look-ahead fix (2026-05-25): VibeSynth::renderNextBlock
    // pre-scans the MIDI buffer for noteOffs that WILL be delivered this block
    // (i.e., not stripped by a same-pitch later noteOn) and flips this flag on
    // voices playing those pitches.  findStealCandidate's L7(b) 3-tier classifier
    // demotes flagged voices to Tier 1 so a looping chord pattern's about-to-
    // release voices don't get treated as "still key-down" alongside a held lead -
    // which would let the lead become the oldest Tier 2 victim.  Cleared at the
    // top of every renderNextBlock pre-scan (transient per-block flag).
    bool isNoteOffQueued() const noexcept { return mNoteOffQueued; }
    void setNoteOffQueued (bool v) noexcept { mNoteOffQueued = v; }
    // initiateSteal: override the amp ADSR's release to ~1.5 ms (64 samples @ 44.1 kHz)
    // and save the user's original params so the next startNote can restore them.
    // Caller (VibeSynth voiceCap branch) follows with stopNote(0.f, true) so the
    // voice enters its quick-release phase naturally inside renderNextBlock.
    // Idempotent: re-stealing an already-overridden voice is a no-op.
    void initiateSteal() noexcept;

private:
    void releaseResources();

    VibeSampleManager& mManager;

    // ── Playback sources (QA-VoicePool Task 2: fat voices owned permanently)
    // shared_ptr to the region's AudioBuffer keeps the sample alive while playing.
    std::shared_ptr<juce::AudioBuffer<float>>  mSampleBuffer;

    // Forward + reverse source readers owned as direct members.  startNote
    // re-points the chosen one at the new region's buffer via setBuffer() -
    // no per-note heap allocation.
    //
    // IMPORTANT (QA-VoicePool Task 7 NIT 3 fix-up): mForwardSrc + mReverseSrc
    // MUST be declared BEFORE mForwardResamp + mReverseResamp.  The resampler
    // member-init expressions below take `&mForwardSrc` / `&mReverseSrc` at
    // construction time; C++ requires the referenced member to be fully
    // constructed before the reference is taken, which means strict
    // declaration order matters.  Do NOT reorder.  A drive-by alphabetize or
    // accident-of-refactor reorder will silently produce a use-of-uninitialized-
    // member at VibeVoice construction and segfault on the first note-on.
    VibeForwardMemoryAudioSource mForwardSrc;
    ReversedMemoryAudioSource    mReverseSrc;

    // Dual permanent resamplers - one per direction.  juce::ResamplingAudioSource
    // has no setSource() so its input pointer is fixed at construction; the
    // fat-voice design ties one resampler to each direction's source and lets
    // startNote pick which to feed via mActiveResamp.
    juce::ResamplingAudioSource  mForwardResamp { &mForwardSrc, false, 2 };
    juce::ResamplingAudioSource  mReverseResamp { &mReverseSrc, false, 2 };
    juce::PositionableAudioSource* mActiveSrc    { nullptr };
    juce::ResamplingAudioSource*   mActiveResamp { nullptr };

    bool   mIsPlaying   { false };
    double mSampleRate  { 44100.0 };
    int    mBlockSize   { 512 };
    int    mArticGroup  { 0 };

    // ── DSP ───────────────────────────────────────────────────────────────────
    juce::ADSR                                 mAdsr;
    juce::dsp::StateVariableTPTFilter<float>   mFilter;

    float  mDrive         { 1.0f };
    float  mReduct        { 0.0f };   // 0 = off, 1 = max lo-fi
    float  mVolume        { 1.0f };   // set by VibeSynth (APVTS)
    float  mVelocityScale { 1.0f };   // per-note: velocity * region volumeOffset
    float  mPanL          { 1.0f };
    float  mPanR          { 1.0f };
    float  mLfoAmt        { 0.0f };   // vibrato / shimmer depth 0-1

    // LFO for vibrato modulation
    double mLfoPhase    { 0.0 };
    float  mLfoRate     { 5.5f };  // Hz

    // New per-voice params
    float  mStretch       { 1.0f };   // playback speed multiplier 0.5-2.0
    float  mMuffle        { 0.0f };   // cutoff reduction amount 0-1
    float  mVelToMuffle   { 0.0f };   // velocity→muffle routing 0-1
    float  mHardness      { 0.0f };   // resonance character 0-1
    float  mVelToHardness { 0.0f };   // velocity→hardness routing 0-1
    float  mSensitivity   { 0.5f };   // velocity curve exponent 0-1

    // S1 2026-04-21 additions
    float  mTune          { 0.0f };   // global pitch offset (semitones)
    float  mVelToVolume   { 1.0f };   // velocity→volume amount 0-1 (1=v1 behaviour)
    float  mSampleStart   { 0.0f };   // skip-into offset 0-1 (fraction of sample length)
    bool   mReverse       { false };  // reverse playback direction
    float  mNextUnisonCents { 0.0f }; // applied on the next startNote then consumed (reset to 0)
    juce::uint32 mNoteStartCounter { 0 }; // monotonic age tag (stamped from VibeSynth on startNote)

    // Per-note computed filter offsets (set in startNote from velocity + params)
    float  mBaseCutoff    { 20000.f };
    float  mBaseRes       { 0.5f };
    float  mNoteCutoffBias{ 0.0f };
    float  mNoteResBias   { 0.0f };
    // Batch E #2 (2026-05-01): per-note cutoff offset from CC74 (Brightness).
    // -2..+2 octaves multiplied into effCutoff in renderNextBlock.
    float  mPerNoteCutoffOctaves { 0.0f };

    // Resampling hold counter (for sample-rate reduction)
    int    mReductHold  { 0 };
    int    mReductStep  { 0 };

    // Temp buffer used inside renderNextBlock
    juce::AudioBuffer<float> mTmpBuffer;

    // ── QA-VoicePool Task 3 — lock-free occupancy + steal-aware ADSR override ──
    // Sub-A=(a) explicit atomic isActive: supplements juce::SynthesiserVoice::isVoiceActive()
    // so VibeSynth::findStealCandidate can scan voices without dynamic_cast per voice.
    std::atomic<bool> mIsActive { false };
    // L7(b) hybrid stealing predicate: true between stopNote(allowTailOff=true) and
    // ADSR-end; cleared on startNote / hard stopNote / releaseResources.
    bool              mInRelease { false };
    // ADSR override state for steal quick-release.  initiateSteal saves the user's
    // ADSR params here and overrides mAdsr's release to ~1.5 ms.  startNote restores
    // the saved params on the next note allocation.  setAdsr re-routes user-driven
    // param changes during an active override into mPreStealAdsrParams so the new
    // user setting takes effect on the next note (rather than disturbing the
    // in-flight quick-release).
    juce::ADSR::Parameters mPreStealAdsrParams {};
    bool                   mAdsrOverridden     { false };
    // QA-VoicePool Task 3 look-ahead fix: set by VibeSynth::renderNextBlock's
    // pre-scan when a noteOff for this voice's pitch is queued for delivery this
    // block (and not stripped by a same-pitch later noteOn).  Transient - cleared
    // at the top of the pre-scan every block.  Audio-thread-only; plain bool is
    // sufficient (no cross-thread access).
    bool                   mNoteOffQueued      { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VibeVoice)
};

// ── VibeSynth ─────────────────────────────────────────────────────────────────
// Polyphonic sample player: up to 24 physical voices, articulation group switching.
// Wraps juce::Synthesiser + VibeSampleManager.
//
// QA-VoicePool Task 3: physical pool over-provisioned to 24 voices.  The user-facing
// polyphony limit stays at 16 (enforced by the voiceCap APVTS param + the steal-on-
// cap-exceeded branch in renderNextBlock).  The extra 8 reserve voices give the
// stealing path a safe landing zone: when a steal is required, the victim voice is
// flipped into a ~1.5 ms quick-release via initiateSteal() + stopNote(0.f, true);
// it continues fading inside JUCE's normal renderNextBlock for ~64 samples while
// the new note's mSynth.noteOn() allocates to one of the 8 reserve voices.  No
// custom fade-buffer rendering, no synchronous writes past the audio block boundary.
class VibeSynth
{
public:
    static constexpr int kMaxVoices  = 24;
    // QA-VoicePool Task 3: user-facing polyphony default.  Stealing fires when
    // active voices >= kLogicalCap (or the APVTS voiceCap override), NOT when
    // active >= kMaxVoices; the (kMaxVoices - kLogicalCap) = 8 reserve voices
    // are the safe landing zone for the new note while the stolen voice fades.
    static constexpr int kLogicalCap = 16;

    VibeSynth();

    // ── Audio lifecycle ───────────────────────────────────────────────────────
    void prepare         (double sampleRate, int maxBlockSize);
    void renderNextBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);
    void allNotesOff     ();

    // ── Sample loading ────────────────────────────────────────────────────────
    VibeSampleManager& getManager() { return mManager; }

    // ── Parameter setters (CPU guarded, call from processBlock) ───────────────
    // Each guard checks whether the value actually changed before pushing to voices.
    void setFilterParams      (float cutoffHz, float resonance)      noexcept;
    void setDrive             (float drive)                          noexcept;
    void setReduct            (float reduct)                         noexcept;
    void setVolume            (float vol)                            noexcept;
    void setPan               (float pan)                            noexcept;
    void setAdsr              (float a, float d, float s, float r)   noexcept;
    void setLfoAmt            (float amt)                            noexcept;
    void setStereo            (float width)                          noexcept;
    void setTreble            (float treble)                         noexcept;
    void setStretch           (float stretch)                        noexcept;
    void setMuffle            (float muffle)                         noexcept;
    void setVelToMuffle       (float amt)                            noexcept;
    void setHardness          (float hardness)                       noexcept;
    void setVelToHardness     (float amt)                            noexcept;
    void setSensitivity       (float sens)                           noexcept;
    void setArticulationGroup (int   group)                          noexcept;
    // S1 2026-04-21:
    void setTune              (float semitones)                      noexcept;
    void setDetune            (float cents)                          noexcept;
    void setDetuneMode        (int   mode)                           noexcept;
    void setVelToVolume       (float amt)                            noexcept;
    void setSampleStart       (float norm)                           noexcept;
    void setLfoRate           (float hz)                             noexcept;
    void setVoiceCap          (int   cap)                            noexcept;
    void setCutSelf           (bool  on)                             noexcept;
    void setReverse           (bool  rev)                            noexcept;
    void setUnisonVoices      (int   n)                              noexcept;
    void setUnisonSpread      (float cents)                          noexcept;

private:
    VibeSampleManager  mManager;
    juce::Synthesiser  mSynth;
    double             mSampleRate { 44100.0 };

    // CPU guard cache
    float mLastCutoff      { -1.f };
    float mLastRes         { -1.f };
    float mLastDrive       { -1.f };
    float mLastReduct      { -1.f };
    float mLastVol         { -1.f };
    float mLastPan         { -1.f };
    float mLastAmpA        { -1.f };
    float mLastAmpD        { -1.f };
    float mLastAmpS        { -1.f };
    float mLastAmpR        { -1.f };
    float mLastLfoAmt      { -1.f };
    float mLastStereo      { 9999.f };   // sentinel outside the bipolar -1..1 range
    float mLastTreble      { -1.f };
    float mLastStretch     { -1.f };
    float mLastMuffle      { -1.f };
    float mLastVelToMuffle { -1.f };
    float mLastHardness    { -1.f };
    float mLastVelToHard   { -1.f };
    float mLastSensitivity { -1.f };
    int   mLastArtic       { -1 };
    // S1 2026-04-21 cache
    float mLastTune        { 9999.f };
    float mLastDetune      { 9999.f };
    int   mLastDetuneMode  { -1 };
    float mLastVelToVolume { -1.f };
    float mLastSampleStart { -1.f };
    float mLastLfoRate     { -1.f };
    int   mLastVoiceCap    { -1 };
    // S1 Incr3 state (audio-thread only)
    bool  mCutSelf         { false };
    bool  mReverse         { false };
    bool  mLastReverse     { false };
    int   mUnisonVoices    { 1 };
    float mUnisonSpread    { 0.f };
    int   mLastUnisonVoices{ -1 };
    float mLastUnisonSpread{ -1.f };
    // RNG for detune-random mode (audio thread only; deterministic-enough)
    juce::Random mRng;

    // Stereo width state (applied in renderNextBlock)
    float mStereoWidth { 1.0f };

    // Treble shelf state (one-pole HP per channel, applied in renderNextBlock)
    float mTrebleGain  { 0.0f };   // shelf gain multiplier for high component (-1..+1)
    float mTrebleLp[2] { 0.f, 0.f }; // one-pole LP state per channel

    template <typename Fn>
    void forEachVoice (Fn&& fn)
    {
        // QA-VoicePool Task 3: iterate the cached mVoices[] pointer array
        // instead of dynamic_cast'ing mSynth.getVoice(i) per call - forEachVoice
        // is invoked from the voiceCap-stealing branch on every unison fan-out
        // iteration of every note-on, so the dynamic_cast cost matters.
        for (int i = 0; i < kMaxVoices; ++i)
            if (mVoices[i] != nullptr)
                fn (*mVoices[i]);
    }

    // ── QA-VoicePool Task 3 — direct VibeVoice pointer cache + steal candidate scan ──
    // mVoices is populated in the ctor at addVoice time so the audio-thread voiceCap
    // branch can iterate the pool without dynamic_cast per voice (Sub-A=(a) rationale).
    std::array<VibeVoice*, kMaxVoices> mVoices {};

    // findStealCandidate implements L7(b) hybrid: scan mVoices for active candidates,
    // prefer the oldest-by-mNoteStartCounter that's also in release phase; fall back
    // to overall oldest if no release-phase voice is found.  Returns nullptr only
    // if no active voices exist (shouldn't happen at steal time since activeCount
    // >= cap is the precondition).  newPitch is reserved for future "don't steal a
    // voice playing the same pitch" logic; unused for now.
    VibeVoice* findStealCandidate (int newPitch) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VibeSynth)
};
