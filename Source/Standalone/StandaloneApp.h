#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../TempoMapRead.h"   // QA-TempoMap: the published stepped tempo timeline
#include <vector>
#include <utility>

// ── Fake AudioPlayHead for standalone transport ───────────────────────────────
class StandalonePlayHead : public juce::AudioPlayHead
{
public:
    void advanceBlock(int numSamples, double sampleRate);
    // G1 review fix: start() no longer takes/edits tempo.  The pre-map
    // start(bpm) re-anchored at the BPM-field value, which was benign when
    // field == base; now the field displays the EFFECTIVE tempo, so routing
    // it back through a base edit on every Play/resume silently rewrote the
    // base to whatever marker/automation tempo was showing (spec E breach).
    // The timeline already holds tempo truth - Play just plays.
    void start();
    void stop();
    void reset();

    bool   isPlaying()           const { return mPlaying.load(); }
    // 2026-04-30: real recording flag for playhead's PositionInfo (the
    // standalone editor sets this when arming + clears on stop/disarm).
    // Was hardcoded to false in getPosition() so any host-style PositionInfo
    // consumer wouldn't see the recording state.
    bool   isRecording()         const { return mRecording.load(); }
    void   setRecording(bool r)        { mRecording.store(r); }
    // QA-TempoMap: getBPM() reports the EFFECTIVE tempo at the playhead (the
    // BPM field displays it live per Jeff's E pick); the BASE tempo (bar 1
    // until the first marker - the project-persisted value) is separate.
    double getBPM()              const
    {
        return TempoMap::isActive() ? TempoMap::bpmAtSample (mSamplePos.load())
                                    : mBPM.load();
    }
    double getBaseTempo()        const { return mBPM.load(); }
    void   setBPM(double bpm);                          // edits the BASE tempo (message thread)
    // QA-TempoMap: live tempo override (the tempo-automation applicator).
    // Truncate-and-appends a tail segment at the playhead; markers ahead
    // remain in the timeline and re-assert at their samples (last-writer-wins).
    void   setLiveTempo(double bpm);
    // QA-TempoMap: ruler tempo flags, pushed by StandaloneEditor whenever the
    // marker set changes.  {beat, bpm}, any order; stored sorted.
    void   setTempoMarkers(std::vector<std::pair<double,double>> markers);
    double getCurrentBeat()      const { return deriveBeat(mSamplePos.load()); }
    // QA-TransportDisplay: wall-clock position from the sample counter - the
    // sample domain is the transport truth, so this stays exact across tempo
    // changes (and survives QA-TempoMap's anchor->map rework unchanged).
    double getCurrentTimeSeconds() const
    {
        return (double) mSamplePos.load() / juce::jmax (1.0, mSampleRate.load());
    }
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

    // QA-TempoMap (2026-07-08): beat is derived piecewise-linearly through the
    // stepped sample-indexed timeline published in TempoMap:: (TempoMapRead.h)
    // - replaces the QA-Ed single re-basing anchor (whose seqlock protocol the
    // timeline publication inherits).  mBPM stores the BASE tempo; mMarkers is
    // the message-thread copy of the ruler tempo flags.  Single writer stays
    // the message thread: every mutation (base edit / marker CRUD / live
    // automation write) rebuilds + republishes.  While PLAYING, rebuilds
    // truncate-and-append at the playhead so already-heard mapping never
    // re-maps and the sample clock never jumps; while STOPPED, rebuilds are
    // pure and the playhead relocates to keep its BEAT (bar) position.  A
    // loop-wrap needs NO timeline write: the map is a pure sample->beat
    // function, so wrapped samples re-derive their beats exactly at any
    // number of intervening tempo changes.
    std::vector<std::pair<double,double>> mMarkers;   // {beat, bpm} sorted; message thread only
    // G1 review fix: the previous live-automation pivot's sample.  Consecutive
    // live writes COALESCE onto one tail segment instead of appending ~30
    // pivots/s until kMaxSegs saturates and history mapping erodes ("base +
    // markers + one live tail" is the published invariant).  Message thread
    // only; -1 = no live tail; cleared whenever the rule set rebuilds.
    int64_t mLastLivePivot { -1 };

    void   rebuildTimeline (double forwardTempoOverride = -1.0,
                            bool   rebaseWhilePlaying   = false);
    double markerRuleTempoAtBeat (double beat) const;   // base + markers rule (message thread)
    double deriveBeat (int64_t sample) const;
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
    // QA-L-Fix D-11: kit-trigger velocity source (From controller / Fixed).
    static void       loadMidiTriggerVelocityPref();
    static void       saveMidiTriggerVelocityPref();
};
