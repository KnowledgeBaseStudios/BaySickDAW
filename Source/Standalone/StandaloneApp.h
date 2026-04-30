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
    void   setBPM(double bpm)          { mBPM.store(bpm); }
    double getCurrentBeat()      const { return mPPQPos.load(); }
    void   seekTo(double beat);

    // Set by PluginProcessor each block so the playhead wraps at the pattern loop point.
    // 0 = no wrap (unlimited advance).
    void   setLoopBeats(double beats)  { mLoopBeats.store(juce::jmax(0.0, beats)); }

    // Loop start point (for time-selection looping in both pattern & song mode).
    // When loopStart > 0 the playhead wraps to loopStart rather than 0.
    void   setLoopStart(double beat)   { mLoopStart.store(juce::jmax(0.0, beat)); }

    juce::Optional<PositionInfo> getPosition() const override;

private:
    std::atomic<bool>   mPlaying    { false };
    std::atomic<bool>   mRecording  { false };
    std::atomic<double> mBPM        { 120.0 };
    std::atomic<double> mPPQPos     { 0.0 };
    std::atomic<double> mLoopBeats  { 0.0 };
    std::atomic<double> mLoopStart  { 0.0 };
    double              mSampleRate { 44100.0 };
};

// ── Standalone JUCE Application ───────────────────────────────────────────────
class VibesynthStandaloneApp : public juce::JUCEApplication,
                               public juce::ChangeListener
{
public:
    const juce::String getApplicationName()    override { return "BaySickDAW"; }
    const juce::String getApplicationVersion() override { return "1.2.0"; }
    bool moreThanOneInstanceAllowed()          override { return false; }

    void initialise(const juce::String&) override;
    void shutdown()  override;
    void systemRequestedQuit() override { quit(); }

    // ChangeListener — saves audio device state whenever the user changes it
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

private:
    std::unique_ptr<VibeSynthProcessor>          mProcessor;
    std::unique_ptr<StandalonePlayHead>          mPlayHead;
    std::unique_ptr<juce::AudioDeviceManager>    mDeviceManager;
    std::unique_ptr<juce::AudioProcessorPlayer>  mPlayer;
    std::unique_ptr<juce::AudioIODeviceCallback> mAdvancer; // PlayHeadAdvancer — defined in StandaloneApp.cpp
    std::unique_ptr<juce::DocumentWindow>        mWindow;

    void saveAudioSettings();

public:
    // 2026-04-25: made public so AudioSettingsDialog (in StandaloneEditor.cpp)
    // can write the pending file as a SIBLING of the live settings file.
    // Single source of truth for the settings path resolution.
    static juce::File getAudioSettingsFile();
};
