# Transport and Playback

**Purpose** - The transport is the strip of controls across the top of the app
that starts, stops and records the music, sets the tempo, and tells you where you
are in the song. It also owns the two playback modes - looping one pattern, or
playing the whole arrangement - and the metronome click.

---

## How it operates

Three pieces:

* **`GlobalTransportBar`** (`Source/Standalone/GlobalTransportBar.cpp/.h`) - the
  visible bar. It owns no state of its own beyond button visuals; every button
  calls a callback that `StandaloneEditor` wired up. It also hosts the
  performance readout on the far right. It runs a 10 Hz timer that pushes the
  loop length and the current time signature down to the playhead and syncs
  button visuals back from the processor's real state (so a project load cannot
  leave a button lying).
* **`StandalonePlayHead`** (`Source/Standalone/StandaloneApp.h`,
  `StandaloneApp.cpp`) - the clock. It implements JUCE's `AudioPlayHead`, so the
  whole engine asks it where the song is. Its source of truth is a **64-bit
  absolute sample counter**; musical beats are *derived* from that counter
  through a sample-indexed tempo timeline (`TempoMap`), which is why tempo
  changes and loops never accumulate drift. `advanceBlock` is the sole
  audio-thread writer.
* **`TransportPositionReadout`** (in `GlobalTransportBar.h`, positioned by
  `StandaloneEditor`) - the amber clock display. Read-only; runs its own 30 Hz
  timer and repaints only when the formatted text actually changes.

**Play.** `StandaloneEditor::startPlayback` either starts the playhead
immediately, or - when record is armed *and* the precount toggle is on - starts
a one-bar count-in and defers `mPlayHead.start()` to a timer sized to that bar.

**Looping.** Every 10 Hz tick, `onGetLoopBeats` recomputes where the playhead
should wrap and pushes it to `setLoopBeats` / `setLoopStart`. The playhead wraps
by exact integer sample arithmetic and **preserves the overshoot** into the next
pass, so looped material stays sample-locked to absolute time.

**Stop at the end of a song.** In Song mode with looping off, the audio thread
sets a `mRequestStop` flag when the playhead passes the last block's end. The UI
picks that flag up on its next tick and runs the same code path the Stop button
does, including finalizing any recording in progress.

**The metronome and count-in** are synthesized in the processor
(`BaySickDAWProcessor::applyPostMixRecordAndMetro`, `Source/PluginProcessor.cpp`)
and added to the master buffer **after** the recorder taps, so a click never ends
up inside a recorded file or an export. Clicks are deferred by the project's
total plugin latency so they land on the music rather than ahead of it. Nothing
in that function runs during an offline render.

---

## User-facing behavior

### The transport bar, left to right

| Control | What it does |
|---|---|
| **Play** (triangle) | Starts playback from wherever the playhead is. The button body turns green while playing. Pressing Space does the same, and pressing it again pauses. |
| **Pause** (two bars) | Stops the clock but leaves the playhead where it is, quantized to the nearest 16th note so it sits on a grid line. Pause **always** ends a recording in progress - to record again you must arm again. Held notes are released so nothing rings on. The button tints yellow while paused. |
| **Stop** (square) | Stops, releases every held note, disarms record, and sends the playhead back to the start - or to the start of your highlighted loop region if you have one, so stopping keeps you in the section you are working on. |
| **Record** (dot) | Arms recording. It does **not** start anything: arm, then press Play. The button body turns red while armed. Pressing it again while armed disarms and finishes the take. |
| **Record chevron** (small arrow on the right of the Record button) | Opens the record options: **ASIO** (records audio from your interface) or **MIDI (piano roll tabs only)** (captures played notes into whichever piano roll you were last editing), plus a **Global Record-Quantize** submenu that snaps captured note starts to a chosen grid when the take is committed. The list is the same one the Builder and Piano Roll use - Off, Line, Bar, Beat, 1/2 Beat, 1/3 Beat, Step, 1/2 Step, 1/3 Step, 1/4 Step, 1/6 Step - and defaults to Off, which keeps your raw timing. ("Line" is present for consistency and does nothing here; there is no zoom grid at record time.) |
| **BPM** | The tempo. Type a number and press Enter, or click away. Values are clamped to the musical range rather than snapped back to a default, so typing 500 gives you 300, not 120. Right-click the field for **Automate tempo**, which opens an automation lane for tempo. Range 20-300, default 120. |
| **TAP** | Tap it in time and it works out the tempo. Two taps is the minimum; it averages the last eight and rounds to a whole number. Leaving a gap of more than 2 seconds starts a fresh count. |
| **PATTERN / SONG** | The playback mode toggle - see below. Default PATTERN. |
| **Loop toggle** (arrow-and-bar / circular arrow) | Only meaningful in Song mode. Arrow-into-a-bar = play to the end of the song and stop. Circular arrow = loop back to the start. Default is loop. |
| **Metronome** (metronome icon) | Click on or off. The button lights amber when on. Default off. |
| **Metronome chevron** | Opens the metronome settings panel - see below. |
| **Piano-keys button** | Turns the typing keyboard on and off, letting you play the current tab's instrument from your computer keys. Lights amber when on. Same as Ctrl+T. |
| **Swing knob** | Pushes every second 16th step late, for a shuffle feel. 0% is dead straight. Double-click resets to 0. Each player has its own Swing Mix knob that scales this global amount. Range 0-100%, default 0. |

### The rest of the top row

Immediately right of the transport controls sit the **pattern button** (shows the
current pattern's name, with its time signature appended when it is not 4/4;
click for the pattern list), the **position readout**, and then the **ribbon
tabs**. Those are documented with the pattern and workspace systems.

### The position readout

The amber display between the pattern button and the tabs. It has two formats and
**clicking it switches between them**:

* **Bars:beats:ticks** (the default) - e.g. `5:3:48`. Bar 5, beat 3, 48 ticks
  into that beat. There are 96 ticks per beat. Beats are counted in the
  denominator of the current time signature, so 7/8 counts 1 through 7. Both
  numbers start at 1.
* **Minutes:seconds.milliseconds** - e.g. `1:23.450`. This is real elapsed time
  and stays exact across tempo changes, because it is computed from the sample
  counter.

In Song mode the bar numbering follows the time-signature markers on the Builder
ruler. In Pattern mode it follows the current pattern's own signature.

Your choice of format is remembered across sessions on this computer.

### Pattern mode vs Song mode

* **PATTERN** - plays the pattern currently selected in the pattern dropdown, over
  and over. The loop length is the pattern's own content length. This is the mode
  for writing a part.
* **SONG** - plays the arrangement you built on the Builder page, from bar 1
  through the end of the last clip. This is the mode for hearing the whole piece.

The keyboard toggle is **L**.

**Loop regions.** In either mode you can Ctrl-drag on the ruler to highlight a
span of time; playback then loops just that span, and Stop returns you to the
start of it instead of the start of the project. In Song mode the highlight comes
from the Builder ruler; in Pattern mode it comes from the piano roll you are
looking at. A highlighted span always wins over the whole-pattern or whole-song
loop.

With no arrangement blocks at all, Song mode loops a 1-bar audition (loop on) or
runs freely without stopping (loop off).

### The metronome

Click the chevron next to the metronome button for the settings panel:

| Control | Options / range | Default |
|---|---|---|
| **Sound** | Sine, Click, Wood, Bell | Sine |
| **Volume** | 0-200% | 70% |
| **Precount** | "1-bar lead-in when recording (Ctrl+P)" on/off | off |

The click accents the first beat of each bar. In Song mode it follows the time
signature markers on the Builder ruler; in Pattern mode it follows the current
pattern's signature, so a 7/8 pattern gets seven eighth-note clicks with the
accent on beat 1.

The metronome button itself is on/off, keyboard **Ctrl+M**. It is off by default
and only clicks while the transport is running.

### Count-in (precount)

Turn **Precount** on in the metronome panel (or press **Ctrl+P**). Then, whenever
you arm Record and press Play, you get **exactly one bar** of click before the
transport starts. The bar's length, its click subdivision and its accent all
follow the time signature at the point you are recording from, so a 7/8 count-in
is seven eighth-note clicks.

Two things are worth knowing:

* The count-in is *not* silence. The recorded audio file contains the count-in
  bar as pre-roll, so a note you play a hair early is captured. What lands on the
  arrangement grid is trimmed to start at the downbeat.
* The click never reaches the recording, an export, or a freeze render.

Stopping cancels a count-in in progress.

### What is recorded, and where it lands

Arming and pressing Play in **ASIO** mode records every armed Vox and Inst strip
to its own WAV inside the project's `Samples` folder, and drops the resulting
clips onto the Builder arrangement. If no project has been saved yet, the app
asks for a name first, saves the project, then re-arms and starts - you do not
have to press the buttons again.

In **MIDI** mode the played notes are captured into whichever piano roll you were
last editing. If you have never opened one this session, the app says so and
disarms rather than recording into nowhere.

Stop, Pause, disarming, and the automatic stop at the end of a song all finalize
the take identically.

### The performance readout (far right of the bar)

Six live numbers in three small rows. Hover for the full values plus a legend.

| Token | Meaning |
|---|---|
| **SYS** | Whole-computer CPU percentage - every app, not just this one. Green / yellow as it climbs. |
| **DSP** | This app's audio engine load, as a percentage of the time available per audio block. Green under 50%, yellow above, flashing red above 95%. |
| **MEM** | This app's memory use in megabytes. |
| **LAT** | Total reported plugin latency in samples, summed across every effect that adds delay. |
| **UND** | Audio-clip streaming underruns during continuous playback - reads that came back silent because the disk could not keep up. Should stay 0. |
| **PF** | The slowest disk read this session, in milliseconds. |

SYS is colored on its own; the other rows follow the DSP load, so a busy
background app cannot make an idle project look overloaded.

---

## Parameters and persistence

Most transport state is **not** APVTS - it lives in plain atomics on the
processor and plain members on the editor, and is written into the project file's
`<UIState>` element.

| State | Where it lives | Saved with |
|---|---|---|
| Tempo (base BPM) | `PatternManager` global tempo; also the playhead's base | project |
| Tempo markers | `PatternManager` tempo changes (Builder ruler) | project |
| Song / Pattern mode | `BaySickDAWProcessor::mSongMode` (default Pattern) | **not saved**; every launch and every project open starts in Pattern mode |
| Song loop on/off | `BaySickDAWProcessor::mSongLoopMode` (default on) | project, `<SongLoop on>` |
| Metronome on/off | `mMetro.enabled` (default off) | project, `<Metronome enabled>` |
| Metronome volume | `mMetro.volume` (default 0.7) | project, `<Metronome volume>` |
| Metronome sound | `mMetro.soundType` (0 Sine, 1 Click, 2 Wood, 3 Bell; default Sine) | project, `<Metronome soundType>` |
| Precount on/off | `StandaloneEditor::mPrecountEnabled` (default off) | project, `<Metronome precountEnabled>` |
| Position readout format | - | `settings.xml`, `<TransportDisplay showTime>` - **per machine**, not per project |
| Record mode (ASIO / MIDI) | `GlobalTransportBar::mRecordMode` | **not saved**; resets to ASIO each launch |
| Record-arm state | `StandaloneEditor::mRecordArmed` | **not saved** |
| Playhead position | `StandalonePlayHead::mSamplePos` | **not saved**; every session starts at bar 1 |

Two APVTS parameters do belong to the transport:

| Parameter | Type | Range | Default |
|---|---|---|---|
| `globalSwing` | Float | 0 to 1 (shown 0-100%) | 0 |
| `Unified_RecordQuantizeDiv` | Int | 0-10 (shared 11-label snap scheme; 0 = Off) | 0 |

Both are in the main APVTS and therefore save with the project and are
automatable.

---

## Lifetime and teardown

`GlobalTransportBar` and `TransportPositionReadout` are created in the
`StandaloneEditor` constructor and live for the whole session; there is exactly
one of each and neither can be closed. The bar's 10 Hz timer starts in its
constructor and stops in its destructor.

`StandalonePlayHead` is created by the application object
(`BaySickDAWStandaloneApp::initialise`) **before** the processor and the editor,
and it outlives them - the audio device callback advances it directly. Ordering
that matters:

* The playhead is registered with the processor before the first audio block, or
  the engine would have no clock.
* The count-in timer must be stopped and `countInActive` cleared before the
  playhead stops, or the click keeps running past a Stop.
* Recording is finalized *before* playback halts, so the last in-flight audio
  block lands in the file.
* On Stop and Pause, an all-notes-off broadcast is flushed to every engine.
  Without it, long-tailed sounds keep ringing.

---

## Cross-references

* **Keyboard Shortcuts.md** - Space, Shift+Space, R, L, Home, Ctrl+M, Ctrl+P,
  Ctrl+T and the bar-navigation keys.
* **Mixer.md** - the Arm and Listen LEDs on Vox and Inst strips are what decide
  *which* channels an armed recording captures; the performance readout's DSP
  figure is the same engine load the Mixer meters reflect.
* **Workspace and Windows.md** - the transport bar is part of the fixed frame
  chrome, above the workspace, and is never a contained window.
* The Builder page owns the arrangement, the ruler, tempo markers, time-signature
  markers and the loop-region highlight; the piano roll owns the pattern-mode
  loop region. Both are documented separately.

---

## Differs from Carry-Forward

Carry-Forward (2026-05-07) contains no transport section, so there is nothing to
reconcile. Its recording-lifecycle notes in section 3 remain broadly accurate; the one
change worth flagging is that the song-end auto-stop now runs the full
stop-and-finalize path rather than stopping playback alone.
