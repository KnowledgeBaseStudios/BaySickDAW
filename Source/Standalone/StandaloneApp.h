#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"

// ── Fake AudioPlayHead for standalone transport ───────────────────────────────
class StandalonePlayHead : public juce::AudioPlayHead
{
public:
    void advanceBlock(int numSamples, double sampleRate);
    void start(double bpm);
    void stop();
    void reset();

    bool   isPlaying()           const { return mPlaying.load(); }
    // 2026-04-30: real recording flag for playhead's PositionInfo (the
    // standalone editor sets this when arming + clears on stop/disarm).
    // Was hardcoded to false in getPosition() so any host-style PositionInfo
    // consumer wouldn't see the recording state.
    bool   isRecording()         const { return mRecording.load(); }
    void   setRecording(bool r)        { mRecording.store(r); }
    double getBPM()              const { return mBPM.load(); }
    void   setBPM(double bpm);                          // re-anchors the tempo anchor (message thread)
    double getCurrentBeat()      const { return deriveBeat(mSamplePos.load()); }
    void   seekTo(double beat);

    // QA-Ed: backward-seek discontinuity signal.  Set by seekTo() on a
    // backward seek; the scheduler consumes it once per block to flush
    // pending note-offs (cut held notes on a backward scrub, as before).
    std::atomic<bool>* getSeekDiscontinuityFlag() noexcept { return &mSeekDiscontinuity; }

    // QA-Ee Task 1c: one-shot "just looped" signal.  Set by advanceBlock() the
    // block the playhead wraps; the scheduler consumes it once to flush a
    // note-off stranded exactly on the loop point when the wrap landed on a
    // block boundary (so the prior block was not flagged a straddle).
    std::atomic<bool>* getLoopWrappedFlag() noexcept { return &mLoopWrapped; }

    // Set by PluginProcessor each block so the playhead wraps at the pattern loop point.
    // 0 = no wrap (unlimited advance).
    void   setLoopBeats(double beats)  { mLoopBeats.store(juce::jmax(0.0, beats)); }

    // Loop start point (for time-selection looping in both pattern & song mode).
    // When loopStart > 0 the playhead wraps to loopStart rather than 0.
    void   setLoopStart(double beat)   { mLoopStart.store(juce::jmax(0.0, beat)); }

    // C.5: per-block-updated time signature reported via getPosition().
    // PluginProcessor::processBlock pushes the TS effective at the current
    // beat (looked up via PatternManager::getEffectiveTimeSigAtBar).
    void   setTimeSignature(int num, int den)
    {
        mTsNum.store (juce::jlimit (1, 32, num));
        mTsDen.store (juce::jlimit (1, 32, den));
    }

    juce::Optional<PositionInfo> getPosition() const override;

private:
    // QA-Ed: int64 ABSOLUTE sample counter is the transport source-of-truth.
    // Sole audio-thread writer = advanceBlock().  Beat position is DERIVED
    // from it via the tempo anchor below, so the float beat-accumulation drift
    // that caused Issue 3 is gone while the public API stays in beats.
    std::atomic<bool>    mPlaying    { false };
    std::atomic<bool>    mRecording  { false };
    std::atomic<double>  mBPM        { 120.0 };
    std::atomic<int64_t> mSamplePos  { 0 };
    std::atomic<double>  mLoopBeats  { 0.0 };
    std::atomic<double>  mLoopStart  { 0.0 };
    std::atomic<int>     mTsNum      { 4 };
    std::atomic<int>     mTsDen      { 4 };
    std::atomic<double>  mSampleRate { 44100.0 };
    std::atomic<bool>    mSeekDiscontinuity { false };
    std::atomic<bool>    mLoopWrapped       { false };   // QA-Ee Task 1c: set on loop wrap, consumed by the scheduler

    // Tempo anchor: beat = mAnchorBeat + (sample - mAnchorSample) * bpm/(60*sr).
    // Re-based on every BPM change / seek so derived beats stay continuous
    // across tempo automation.  Because the formula is linear in the sample
    // count, a constant-tempo loop-wrap of mSamplePos derives the exact
    // wrapped beat with NO anchor write on the audio thread (loopStartSamp
    // maps to loopStartBeat by construction).  The three fields are published
    // as a group via a SEQLOCK (mAnchorSeq): single writer = the message
    // thread (setBPM/seekTo/start/reset); readers (audio getPosition +
    // message-thread UI cursor) retry on a torn read.
    std::atomic<uint32_t> mAnchorSeq    { 0 };
    std::atomic<double>   mAnchorBeat   { 0.0 };
    std::atomic<int64_t>  mAnchorSample { 0 };
    std::atomic<double>   mAnchorBpm    { 120.0 };

    double deriveBeat (int64_t sample) const;
    void   publishAnchor (double beat, int64_t sample, double bpm);
};

// J-A2 (2026-05-04): master-output routing globals.  Atomics live here so the
// Mixer hamburger menu (in StandaloneEditor.cpp) can flip them and the audio
// callback (PlayHeadAdvancer in StandaloneApp.cpp) can read them lock-free.
namespace MasterOutputRouting
{
    extern std::atomic<int>  gFirstOutputChannel;   // 0-based device output channel index
    extern std::atomic<bool> gMasterIsMono;         // true = sum L+R into gFirstOutputChannel only
}

// ── Standalone JUCE Application ───────────────────────────────────────────────
class VibesynthStandaloneApp : public juce::JUCEApplication,
                               public juce::ChangeListener,
                               public juce::MidiInputCallback   // C.3 (2026-04-30)
{
public:
    const juce::String getApplicationName()    override { return "BaySickDAW"; }
    const juce::String getApplicationVersion() override { return "1.2.0"; }
    bool moreThanOneInstanceAllowed()          override { return false; }

    void initialise(const juce::String&) override;
    void shutdown()  override;
    void systemRequestedQuit() override { quit(); }

    // ChangeListener - saves audio device state whenever the user changes it
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

    // C.3 (2026-04-30): hardware MIDI input bridge.  Called from the JUCE MIDI
    // input thread for every enabled device.  Pushes the message to the
    // processor's MidiMessageCollector; the audio thread drains the collector
    // each block and routes by Piano-Roll-page focus (set via
    // PluginProcessor::setLiveMidiTarget).
    void handleIncomingMidiMessage (juce::MidiInput*,
                                     const juce::MidiMessage& message) override;

private:
    std::unique_ptr<VibeSynthProcessor>          mProcessor;
    std::unique_ptr<StandalonePlayHead>          mPlayHead;
    std::unique_ptr<juce::AudioDeviceManager>    mDeviceManager;
    std::unique_ptr<juce::AudioProcessorPlayer>  mPlayer;
    std::unique_ptr<juce::AudioIODeviceCallback> mAdvancer; // PlayHeadAdvancer - defined in StandaloneApp.cpp
    std::unique_ptr<juce::DocumentWindow>        mWindow;

    void saveAudioSettings();

public:
    // 2026-04-25: made public so AudioSettingsDialog (in StandaloneEditor.cpp)
    // can write the pending file as a SIBLING of the live settings file.
    // Single source of truth for the settings path resolution.
    static juce::File getAudioSettingsFile();

    // J-A2 (2026-05-04): master output channel routing persistence.  Lives
    // alongside audio_settings.xml as `master_output.xml` (machine-scoped,
    // not project-scoped - different rigs have different audio interfaces).
    static juce::File getMasterOutputFile();
    static void       loadMasterOutputRouting();   // call once at startup before mDeviceManager->initialise
    static void       saveMasterOutputRouting();   // call when the mixer hamburger writes a new selection

    // 2026-05-07 (Batch 10 Phase 3): persistence for the Mixer hamburger menu's
    // "Multi-core Rendering" toggle.  Stored as <MultiCoreRendering on="0|1"/>
    // child of the existing <BaySickDAWSettings> root in
    // Documents/BaySickDAW/settings.xml -- the same file PatternColorPicker
    // and BaySickNAMIREditor use for their own children.  Defaults to true
    // when the key is missing (first launch / never toggled).  Load runs
    // once at startup BEFORE mDeviceManager->initialise so the audio thread
    // sees the correct value from block 1.  Save runs immediately after
    // every toggle so the preference survives even an abrupt shutdown.
    static void       loadMultiCoreRenderingPref();
    static void       saveMultiCoreRenderingPref();
};
