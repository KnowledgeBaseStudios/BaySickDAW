# Freeze and Export

**Purpose** - Both halves of this document are the same machine: the app rendering
audio faster than real time with the sound card suspended. **Export** turns your
song into a finished file. **Freeze** does the same thing to one instrument and
plays the result back in place of the live instrument, so a heavy synth stops
costing CPU while its effects, EQ and fader stay live and editable. There is one
render loop in the code and all three consumers - export, the loudness Measure
button, and freeze - go through it.

## How it operates

**The one render core.** `BuilderPage::runOfflineLoop`
(`Source/Standalone/BuilderPage.cpp`) owns the span/scope maths, the offline
drive, the tempo-aware beat clock, per-block automation replay, and tail handling.
Its three consumers differ only in what they do with each rendered block:
`renderToFile` writes files, `measureRender` feeds loudness meters, and
`renderFreezeFile` taps one strip. It is never copied.

**Rendering is the live processor rendering itself** - there is no second copy of
the project. `VibeSynthProcessor::beginOfflineRender` suspends the audio device
(so the render loop becomes the only caller of `processBlock`), sweeps every
engine into non-realtime mode, resets the graph, and re-prepares everything at the
render's sample rate. `endOfflineRender` reverses all of it and clears the
render's own tails so they cannot bleed into live playback. Only one render can
run at a time.

Several things check "am I rendering offline?" and stay quiet: the metronome, the
recorders, the DSP load meter, and - importantly - the content-changed signals
that invalidate freezes, so an automation replay during an export does not cascade
into re-rendering every frozen track.

**Freeze storage.** Files live in `<project>\Freeze\`, named
`tab_<kind>_<index>_song.wav` or `tab_<kind>_<index>_pat<N>.wav`. A frozen tab
owns a *family* of files: one song render plus one render per pattern it actually
has content in. Rusty Drums is the exception - it is one action over thirteen
strip files, written by a single freeze.

The freeze tap reads the instrument's **pre-rack** output, which is why the
strip's effects, EQ, pan and fader stay live on a frozen track.

**Substitution** happens in one place, `Source/Engine/FrozenSourceRead.h`, used by
all five task types that can substitute. In song mode it serves the song render at
the playhead's sample position; in pattern mode it serves that pattern's render at
a pattern-local position, wrapping the loop seam internally. When the transport is
**stopped** a frozen track serves silence, because the playhead does not advance
and reading would replay one block forever as a buzz. Anything the file cannot
answer - no file, wrong pattern, past the end - falls back to the live engine.

**Staleness** has two independent axes, and they add rather than replace each
other:

- `markEngineContentChanged(tab)` - the player axis. Its notes, its own
  parameters, an engine swap, swing. Invalidates every cached pattern for that
  one tab.
- `markPatternContentChanged(pattern)` - the pattern axis. Invalidates that one
  pattern across every frozen tab.
- `markAllFreezesStale()` - tempo and tempo-map changes, which move every
  engine's output in time.

Rack, EQ, fader and send changes do **not** make a freeze stale: they are
downstream of the tap and are still live.

Every one of those calls retracts the frozen source pointers first, so a stale
track plays **live** from that block onward rather than continuing to play the old
render until a new one lands.

**Validity across sessions.** Each freeze records a content stamp (a hash of the
engine's whole state, the notes in scope, the tempo and - for song scope - the
arrangement) and the span it was rendered over. On project load, a file whose
stamp still matches is reused rather than re-rendered; per-pattern stamps are
checked individually, so editing pattern 3 leaves patterns 1, 2 and 4 valid.

## User-facing behavior

### Freeze

Freeze runs **automatically** by default and most people never need to know the
word. When the app has been over your CPU threshold continuously for about three
seconds it *arms*, and then freezes one track the next time you **stop** the
transport and stop editing - never during playback, because the render blocks the
app for several seconds. One track per trip.

Two settings in **Options > File Settings...** control it:

| Control | Range / default | What it does |
|---|---|---|
| Auto-freeze above | Slider 0-100 %, plus "Off" one step past the top. Default 80 %. | The CPU level that arms automatic freezing. 0 always freezes. Drag past 100 to switch it off entirely. |
| Enable Instrument Level Freeze | Checkbox, off by default | Adds the manual **Freeze** control to every player's Menu. Automatic freezing works either way; this only shows the manual control. |

**The manual control** lives on each player window's **Menu**, directly after
FX Rack. It is always visible - grayed out with an explanation rather than hidden -
and it changes label and color with state:

| State | Label | Color | Tooltip |
|---|---|---|---|
| Not frozen | Freeze | white | "Freeze - render this player to a file so its engine stops costing CPU. Its effects, EQ and fader stay live." |
| Frozen | Frozen | cyan | "Frozen - this player's audio is a rendered file, so its engine costs no CPU. Click to unfreeze and edit it again." |
| Frozen but stale | Frozen | orange | "Frozen, but its content changed - it plays live until the new freeze finishes rendering (at Stop). Click to unfreeze." |

Grayed reasons you may see:

- "Freeze is locked. Turn on "Enable Instrument Level Freeze" in File Settings,
  beside the auto-freeze CPU threshold, to freeze players by hand."
- "This player cannot be frozen yet."
- "Save the project first - the freeze file lives beside it."

On a **vocal** the tooltip adds a warning that cannot be turned off: freezing a
vocal prints the whole chain - pitch, alignment, gate, de-reverb, de-esser,
compressor, saturation, limiter and amp - and none of them can be adjusted while
frozen. Use it to get CPU back once a sound is settled, not while you are still
dialing one in.

Freezing by hand shows a progress overlay with a step label ("Freezing -
arrangement", "Freezing - pattern 2 of 4") and a Cancel button. Automatic freezes
show a notice too, so a multi-second stall is never unexplained.

**Which tabs can freeze:** Layers, Bass, Drums, Plugins, Vox, Inst, Clips, and the
Rusty Drums kit. Vox, Inst and Clips freeze their *grid playback* - a recorded take
on that strip replayed through that strip's chain.

**Unfreezing** is the same click. An explicit unfreeze also takes that track out of
automatic freezing's reach for the rest of the session; a project reload puts it
back in play.

**Restore.** A freeze you made **by hand** comes back frozen on any machine,
including one with auto-freeze switched off. An **automatic** freeze is an
adaptation to the machine that made it, so it only restores where auto-freeze is
armed. Freeze files a project no longer needs are swept when a project's tabs
finish restoring.

### Export Audio

**File > Export Audio...**, or **Ctrl+R**. If the project has never been saved you
are asked to save it first, because exports land in the project's `Exports`
folder.

| Control | Options | Default | Notes |
|---|---|---|---|
| Selection | Full Arrangement / Selected Section | Full Arrangement | "Selected Section" is disabled unless you have a selection on the Builder ruler. |
| Tail | Included / Cut | Included | "Included" keeps rendering past the last note until the sound has actually decayed, so a long reverb is not chopped. "Cut" stops dead at the end. |
| Format | WAV / OGG / MP3 | WAV | |
| Quality | WAV: 16-bit / 24-bit / 32-bit float. OGG: Low / Medium / High / Highest. MP3: 128 / 192 / 256 / 320 kbps | 24-bit for WAV, High for OGG, 256 kbps for MP3 | One control, read three ways. |
| Sample rate | 44100 Hz / 48000 Hz | 44100 Hz | |
| Dither | Off / Flat (TPDF) / Noise-Shaped | Off | Applies to 16-bit WAV exports. Flat is the safe default; Noise-Shaped moves the same noise out of the ear's most sensitive range. |
| Normalize to `___` LUFS | checkbox + typed value | off, -14.0 | Measures the render first, then re-renders with a gain applied so it lands on your number. Works in both directions, and any boost is capped so the true peak stays under -1 dBTP. |
| Export stems (one file per mixer strip) | checkbox + a checklist | off | Each checked strip gets its own file beside the main one, tapped during the **same** render pass - so sends stay separate and sidechain behavior (a bass compressor keyed by the kick) is in the stem by construction. |
| Check against | the loudness standards list | remembered between sessions | Which standard the Measure button judges against. Picking "Custom" reveals a "Custom ref (LUFS)" box. |
| Measure | button | | Runs the same offline pass with meters instead of writers and writes no files. |

**Measure** reports two lines: `Integrated -13.2 LUFS   LRA 6.4 LU   True peak
-0.8 dBTP`, then a verdict against the chosen standard - "in spec", or how far off
it is ("loudness 1.2 LU off target, true peak 0.3 dB over"), plus a count of
flagged spans. The result also opens the master analyzer window and draws the
loudness curve, because the number is worth more as a shape than as two lines of
text.

Pressing **Export** asks where to save (defaulting to the project's `Exports`
folder, filename derived from the project name with spaces replaced by
underscores), then renders with a progress bar and a percentage. Every option
control is disabled while a render runs. Stem files land beside the main file as
`<name> - <strip name>.<ext>`. If the render fails you get "Export failed" with
the reason; canceling is silent.

### Other render surfaces

The same machinery is reachable in two more places on the Builder page, both of
which write to the project's `Exports` folder by default and both of which render
24-bit 44100 Hz WAV:

- **Right-click a pattern in the browser > Render** asks *Per Track* (one file per
  track that has notes), *Full Mix* (one file, the same tracks summed - never the
  master output), or *Select Tracks...* which lets you tick the tracks and then
  choose one file per track or one mixed file.
- **Right-click an audio track's row header > Render Track to WAV...** consolidates
  that one track through its own chain. Offered on audio rows only, and shown
  disabled elsewhere: a pattern row is not a channel, and an automation row has no
  audio.

## Parameters and persistence

**Per machine, in `ui_prefs.xml`:**

| Key | Meaning |
|---|---|
| `fsAutoFreezeCpu` | The auto-freeze threshold. 0 = always, 1-100 = a percentage, above 100 = Off. Default 80. |
| `fsInstrumentFreeze` | Whether the manual Freeze control is shown. Default false. |
| `exSpecId` / `exSpecCustom` | The loudness standard and Custom reference. Shared by Measure, export and the automatic take verdicts. |

**Saved with the project**, on each tab's record in `<UIState>`:

| Attribute | Meaning |
|---|---|
| `frozen` | Whether the tab was frozen. |
| `frozenBy` | `manual` restores everywhere; `auto` is re-evaluated per machine. |
| `freezeStale` | Saved while stale means re-render on restore even if the stamp matches - the stamp cannot see every invalidator, which is how it went stale. |
| `freezeScope` / `freezeBeats` / `freezeBpm` / `freezeStamp` | The span and content stamp the render was made against. The stamp is stored as a string because it does not survive XML's signed integer attributes. |
| one `<FreezePattern index stamp>` per cached pattern | Validated individually, so one edited pattern does not invalidate the rest. |

**Not saved anywhere:** the freeze audio itself. `<project>\Freeze\` is a cache -
excluded from project bundles, excluded from the Builder's file browser, and
rebuilt on demand. Export dialog choices other than the loudness spec are not
remembered between openings.

## Lifetime and teardown

- **Publish last.** A freeze clears the audio thread's pointers, waits a block if
  the tab was already frozen, installs the new streams, and only then publishes
  the pointer. Unfreezing does the reverse: null the pointer first, wait one
  block, then release the streams. Releasing first would leave a block in flight
  reading freed memory.
- A per-pattern render that fails is **not** fatal: the song render already
  succeeded, so the tab stays frozen and simply plays live in that one pattern.
- A failed pattern fill is parked until the next content edit, so a broken folder
  cannot flash the overlay on every idle tick.
- The re-render queue only drains when the transport is stopped **and** two
  seconds have passed since your last content edit, one job per tick, because each
  render blocks the app.
- Automatic freezes and stale refreshes render song scope only; per-pattern files
  fill in one short render at a time on later idle ticks. A manual freeze renders
  its whole set at once under the stepped progress overlay.
- Deleting a tab deletes that tab's whole freeze family. Switching project or
  quitting does **not** - those files are the cache the content stamp reuses on
  the next load.
- The orphan sweep runs after a project's tabs are restored. It builds the live
  set from the engine registry rather than parsing filenames back into tabs, so a
  name it cannot parse is left alone rather than deleted on a guess.
- One render at a time, enforced by a compare-exchange. The automatic freeze poll
  stays quiet while an export is running.

## Cross-references

- *Projects and Saving* - the `Freeze\`, `Exports\` and `Reports\` folders, and
  the save-first interlock.
- *Project Bundles* - why the freeze cache is excluded from bundles.
- *Automation* - offline lane replay, which is what makes an export match what
  you heard.
- *Undo History* - freezing is not an undoable edit.

## Differs from Carry-Forward

Nothing to record. The frozen Carry-Forward snapshot predates both freeze and the
offline export path.
