#pragma once
#include <JuceHeader.h>
#include "StandaloneApp.h"

// ── TransportPositionReadout ──────────────────────────────────────────────────
// QA-TransportDisplay (2026-07-08): live playback-position readout.  Click
// toggles bars:beats:ticks (96 PPQ) <-> M:SS.mmm.  Owned by StandaloneEditor
// as an overlay child (like the pattern button + ribbon) and positioned
// between them; defined here so it shares the transport bar's LCD styling.
// Read-only clock consumer - all values arrive via callbacks on the message
// thread (the seqlock retry lives inside StandalonePlayHead::deriveBeat).
// Own 30 Hz timer: the bar's shared 10 Hz tick is too coarse for the .mmm
// digits; a repaint fires only when the formatted string actually changes.
class TransportPositionReadout : public juce::Component,
                                 public juce::SettableTooltipClient,
                                 private juce::Timer
{
public:
    TransportPositionReadout();

    std::function<double()> onGetBeat;          // musical position (pattern mode arrives pre-wrapped)
    std::function<double()> onGetTimeSeconds;   // wall-clock position (sample-derived)
    std::function<bool()>   onGetSongMode;
    // QA-G Task 6: pattern-mode signature (num + den -- beats display in
    // DENOMINATOR units, matching the metronome).  Song mode reads the
    // marker map (TsMap) directly.
    std::function<void(int&, int&)> onGetPatternTs;

    std::function<void(bool)> onDisplayModeChanged;   // user click-toggle -> persistence hook (true = time)

    void setShowTime (bool showTime);   // programmatic (pref restore) - does NOT fire the callback
    bool isShowingTime() const noexcept { return mShowTime; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::String buildText() const;

    bool mShowTime { false };           // default = bars:beats:ticks
    juce::String mText { "1:1:00" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportPositionReadout)
};

// ── GlobalTransportBar ────────────────────────────────────────────────────────
// Sits between the title bar and the ribbon tabs.
// Play (Space), Pause (Space toggle), Stop (Shift+Space),
// Pattern/Song mode selector, Metronome toggle, Tempo text box.
// ─────────────────────────────────────────────────────────────────────────────
class GlobalTransportBar : public juce::Component,
                           public juce::Timer
{
public:
    GlobalTransportBar(StandalonePlayHead& ph);
    ~GlobalTransportBar() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    // ── Transport actions (invoked by the application command manager) ──────
    // Phase A keymap migration (2026-04-26): keyPressed handler removed; the
    // command system calls these directly so the same actions can be remapped
    // through the Help > Key Binds editor.
    void togglePlayPause();   // Play if stopped, Pause if playing
    void stopAndDisarm();     // Hard stop + clear record-arm (Shift+Space default)
    void toggleRecord();      // Flip record-arm state (R default)
    // Phase B-3 (2026-04-26): public toggles called from BSCommands::perform.
    void toggleMetronome();   // Ctrl+M default
    void toggleSongMode();    // L default

    // Callbacks set by StandaloneEditor
    std::function<void()>       onPlay;
    std::function<void()>       onPause;
    std::function<void()>       onStop;
    std::function<void(bool)>   onRecord;           // true = start, false = stop

    // R5c (2026-04-24): record mode selector - ASIO (audio) or MIDI (into the
    // active piano-roll tab).  Dropdown chevron on the Record button opens the
    // picker; setter writes back to GlobalTransportBar's internal state + fires
    // onRecordModeChanged.
    enum class RecordMode { Audio, Midi };
    RecordMode getRecordMode() const noexcept { return mRecordMode; }
    void       setRecordMode(RecordMode m);
    std::function<void(RecordMode)> onRecordModeChanged;

    // QA-Ea Task 0c (FL pre-roll record) + QA-Ee Stage 4: global record-quantize
    // division.  Surfaced as a "Global Record-Quantize" submenu in the Record-
    // button dropdown alongside ASIO / MIDI mode.  Owner of the underlying state
    // (APVTS param `Unified_RecordQuantizeDiv`) lives in StandaloneEditor /
    // VibeSynthProcessor; this bar just reads the current value (for the menu
    // tick) and writes new picks via the setter callback.  Values are the shared
    // 11-label snap scheme (0=Off, 1=Line, 2=Bar ... 10=1/6 Step).
    std::function<int()>      onGetRecordQuantizeDiv;
    std::function<void(int)>  onRecordQuantizeDivChanged;
    std::function<void(bool)>   onMetronomeToggle;  // true = on
    std::function<void(double)> onTempoChanged;     // new BPM
    std::function<void(bool)>   onSongModeChanged;  // true = SONG, false = PATTERN
    // Song playback behavior: false (→) = play-through then auto-stop,
    // true (🔁) = loop back to start of song at song end.
    std::function<void(bool)>   onSongLoopModeChanged;
    std::function<void()>       onMetroSettings;    // ▾ button → show settings panel
    // 2026-04-24: right-click the BPM field -> create a tempo automation clip.
    std::function<void()>       onAutomateTempo;
    // Returns effective pattern loop length in beats (queried each timer tick to keep
    // the standalone playhead wrap point in sync with the current pattern content).
    std::function<double()>     onGetLoopBeats;
    // C.5: returns the (num,den) time signature effective at the current beat
    // (queried each timer tick; pushed onto the standalone playhead so JUCE
    // PositionInfo reports the right TS to all consumers).
    std::function<void(int& outNum, int& outDen)>  onGetTimeSig;

    // 1M: DSP load readout - set by StandaloneEditor, polled every timer tick
    std::function<float()>      onGetDspLoad;  // returns 0..1 (smoothed fraction of buffer window)
    std::function<bool()>       onGetDsp95;    // returns true when load >95% (triggers red flash)
    // 12f follow-on: total host PDC latency in samples (sum of every effect that
    // overrides getLatencySamples() - EQ8 anti-cramping, oversampled saturation,
    // look-ahead compression, etc). Empty callback -> "--" displayed.
    std::function<int()>        onGetLatencySamples;
    std::function<double()>     onGetSampleRate;   // for sample -> ms conversion

    // 2026-04-30: state-source callbacks so the timer can re-sync the
    // SongLoop + Metronome button visuals from the processor's atomics
    // after a project load.  Without these, project XML restored the
    // processor state but the buttons stayed at their default toggle
    // values until the user clicked them.
    std::function<bool()>       onGetSongLoopMode;
    std::function<bool()>       onGetMetronomeEnabled;

    // Public state accessors
    double getBPM()         const;
    bool   isSongMode()     const { return mSongModeBtn->getToggleState(); }
    bool   isMetronomOn()   const { return mMetronomeBtn->getToggleState(); }
    // Song loop behavior (only meaningful when isSongMode()==true).
    bool   isSongLoopMode() const { return mSongLoopBtn && mSongLoopBtn->getToggleState(); }
    // Programmatic set (used by StandaloneEditor when auto-switching modes on navigation).
    void   setSongMode(bool song);

    // Returns screen bounds of the metro arrow button (for CallOutBox placement)
    juce::Rectangle<int> getMetroArrowScreenBounds() const
    {
        if (mMetroArrowBtn) return mMetroArrowBtn->getScreenBounds();
        return getScreenBounds();
    }

    // D-4 typing-keyboard MIDI (QA-TransportDisplay 2026-07-08): the toggle
    // button occupies the slot that was reserved next to the metronome.
    // The button only REPORTS clicks; StandaloneEditor owns the mode state
    // (it also flips via Ctrl+T) and pushes the visual back through the
    // setter so the two entry points can't drift.
    std::function<void()> onTypingKeyboardToggle;
    // QA-G3Smoke SW-1: global Swing knob accessors (editor wires to the main
    // APVTS `globalSwing`; the bar has no processor handle).
    std::function<float()>     onGetSwing;
    std::function<void(float)> onSetSwing;
    // Editor calls after wiring so the knob shows the restored value.
    void refreshSwingKnob();
    void setTypingKeyboardOn (bool on);

    // Called by StandaloneEditor to keep display in sync
    void setPlayState(bool playing, bool paused);
    // R5b (2026-04-23): editor calls this on Stop / disarm so the Record
    // button's red body clears even though no Record click happened.
    void setRecordArmed(bool armed);

    // Width (in pixels) occupied by transport controls on the left side of the bar.
    // StandaloneEditor uses this to start the pattern selector right after.
    // 2026-04-26: bumped 500→520 to give METRO (52→64) + ▾ arrow (16→24) full
    // label visibility without squishing the BPM / Tap / Song / Loop cluster.
    static constexpr int kControlsWidth = 520;  // incl. metro arrow + song-loop toggle

private:
    StandalonePlayHead& mPlayHead;

    // Buttons.  R5a (2026-04-23): Play + Record are custom-painted
    // (PlayButton / RecordButton in the .cpp) so the type is the more general
    // juce::Button.  Pause + Stop remain TextButton.
    std::unique_ptr<juce::Button>     mPlayBtn;
    std::unique_ptr<juce::TextButton> mPauseBtn;
    std::unique_ptr<juce::TextButton> mStopBtn;
    std::unique_ptr<juce::Button>     mRecBtn;        // momentary: armed state tracked separately
    std::unique_ptr<juce::TextButton> mMetronomeBtn;    // toggle
    std::unique_ptr<juce::TextButton> mMetroArrowBtn;  // ▾ → settings popup
    std::unique_ptr<juce::TextButton> mSongModeBtn;    // toggle: Pattern / Song
    std::unique_ptr<juce::TextButton> mSongLoopBtn;    // toggle: play-through (→) / loop (🔁)
    std::unique_ptr<juce::Button>     mKeybMidiBtn;    // D-4: typing-keyboard MIDI toggle (custom-painted)
    std::unique_ptr<juce::Slider>     mSwingKnob;      // QA-G3Smoke SW-1: global Swing (0-100%, dbl-click 0)

    // BPM
    std::unique_ptr<juce::Label>      mBpmLabel;
    std::unique_ptr<juce::TextEditor> mBpmField;
    // 2026-04-24: BPM right-click watcher.  Owns `*this` by reference; lifetime
    // tied to the containing GlobalTransportBar so no dangling ref risk.
    struct BpmMouseWatcher : public juce::MouseListener {
        GlobalTransportBar& owner;
        explicit BpmMouseWatcher (GlobalTransportBar& o) : owner (o) {}
        void mouseDown (const juce::MouseEvent& e) override;
    };
    BpmMouseWatcher mBpmMouseWatcher { *this };

    bool mIsPlaying   { false };
    bool mIsPaused    { false };
    bool mIsRecording { false };
    RecordMode mRecordMode { RecordMode::Audio };   // R5c

    // Tap Tempo
    std::unique_ptr<juce::TextButton> mTapBtn;
    juce::Array<double> mTapTimes;   // last few tap timestamps (ms)

    void doTap();   // records tap, recomputes BPM after ≥2 taps

    // CPU / memory readout (far right)
    std::unique_ptr<juce::Label> mPerfLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlobalTransportBar)
};
