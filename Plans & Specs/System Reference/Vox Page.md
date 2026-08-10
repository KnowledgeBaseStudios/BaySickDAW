# Vox Page

**Purpose** - A Vox tab is a vocal track. It is where you plug in a microphone,
record a take, and shape it. Every Vox tab owns one complete vocal channel: a
recording input, a processing chain, an amp/cab stage, and two offline editors
for fixing pitch and timing. A Vox tab also plays back audio you already have -
drag a WAV onto its row in the Builder grid and the whole vocal chain processes
it exactly as if you had just sung it.

## How it operates

`VoxPage` (`Source/Vox/VoxPage.cpp` / `.h`) is a thin host component. The real
engine is a `BaySickVocalProcessor`, created and owned by the model
(`EngineRig`, keyed `TabKind::Vox` + page index); the page only holds a
non-owning pointer to it and owns the `BaySickVocalEditor` that draws it. The
page never picks an engine type - `VoxPage::selectEngine` coerces any request to
`BaySickVocal`.

- **Channel identity.** Page index N owns mixer channel
  `MixerChannelIds::voxInsert(N)` (600 + N), mixer parameter prefix
  `mixer_vox_<N>_`, and routes by default to the Vox bus (`kVoxBus`, 7); a
  second Vox bus (`kVoxBus2`, 9) exists and page presets fall back to bus 1 when
  the saved bus is not active in the current project. Capacity is
  `kMaxVoxPages = 10`.
- **Audio.** `VoxStripTask` (`Source/Engine/Tasks/VoxStripTask.*`) renders the
  strip on the audio thread. Per block it reads the strip's arm / listen /
  input-channel / monitor-mode parameters. When the strip is not live it decodes
  every routed audio clip into one sum and runs the engine once on that sum;
  when the strip is live it copies the live input in, and if prior takes overlap
  the same moment it hands them to the engine as a block-scoped "monitor merge"
  so the chain processes the live voice and the earlier takes together. The task
  then runs `BaySickGraph::processInsert` for the Vox insert.
- **Recording.** The processor's wet tap (`setWetRecorder`) captures the signal
  after realtime pitch correction and before the vocal chain; the dry tap
  captures the raw input. See "User-facing behavior" for what actually lands on
  the grid.
- **Background analysis.** The page runs a 4 Hz timer
  (`VoxPage::timerCallback`). When an existing Pitch or Align analysis has gone
  stale, the clip layout has been stable for about a second, and the transport
  is stopped, it re-runs that analysis automatically. It never retries the same
  failed state twice, and it reports nothing - the manual buttons in the editors
  are the reporting path.
- **Threads.** Everything on the page is message thread. The engine's realtime
  correction, chain, and NAM/IR run on the audio thread; the Pitch and Align
  analyses and renders are message thread only.

## User-facing behavior

### Getting a Vox tab

- **Ribbon Vox dropdown -> "+ Add BaySickVocal"** creates the tab. Adding
  always goes through the mixer: the strip is created first and the tab is
  spawned from it, so a Vox tab and its mixer strip always exist together.
- **Ribbon Vox dropdown -> "+ Add New Vox From Export"** lists the render files
  the Pitch and Align editors have written into this project. Picking one
  creates a new Vox tab, places that file on the grid at the position the render
  came from, then asks two questions in order: "Clone the source tab's vocal
  chain settings?" and "Mute the original Vox strip?" Cloning copies the chain
  tone only - the source tab's pitch and align analyses, version history, and
  render lists are deliberately cleared on the new tab, because they describe
  the old tab's audio.

### The page itself

The page shows one panel, **BaySickVocals** - the realtime pitch board and the
page-wide Mix and A/B controls. Those controls are documented in
`Vocal Chain.md`.

The four other surfaces open as their own windows from the title strip's
**Menu** button:

| Menu entry | Opens | Documented in |
|---|---|---|
| Vocal Chain | The six-stage vocal processing rack | `Vocal Chain.md` |
| BaySickPitch | Note-by-note pitch and timing editor | `Pitch Editor.md` |
| BaySickAlign | Two-track timing alignment editor | `Align Editor.md` |
| NAM/IR | Amp capture, cabinet, and mic simulation | `NAM Amp and Cab.md` |

Each window stays open independently of the page; closing the Vox window does
not destroy the panels or lose an analysis.

The same **Menu** button also carries the page actions:

| Item | What it does |
|---|---|
| Lock | Marks the tab locked. A locked tab cannot be deleted (Delete Vox is grayed). |
| Rename... | Renames the tab. |
| Duplicate Vox (new tab) | Makes a second Vox tab with a copy of this one's whole engine state, and switches to it. |
| Save Page Preset As... | Saves the engine plus the mixer strip, insert rack and post-EQ into a named XML preset. |
| Load Page Preset | Submenu of everything saved under the Vox preset folder, including subfolders. |
| Delete Vox | Deletes the tab. Warns first: the player, mixer strip, effects rack, piano roll and the audio-library entries for every recording made on this tab all go; the audio files in the project's Samples folder stay on disk. If the page has unsaved changes the prompt offers **Save Page Preset & Delete** as well as **Delete** and **Cancel**. |

### The mixer strip (where recording is set up)

The Vox tab's mixer strip carries the usual fader, pan, mute and solo, plus two
LEDs that only Vox and live-input Inst strips have:

- **Arm LED** - **left-click** arms and disarms the strip. **Right-click** opens
  the input picker, which lists the audio interface's input channels (stereo
  pairs are grouped into one entry where the device reports a pair). Picking a
  channel only assigns it; it does **not** arm the strip, which is how you set up
  monitoring before you commit to recording. "Disarm" appears at the bottom of
  the picker while the strip is armed.
- **Builder Grid Default** - a second section in that same right-click picker,
  Vox only. It
  chooses which of the four recorded take types lands on the arrangement grid:
  **Dry**, **Dry Cleaned**, **Wet**, **Wet Cleaned**. With nothing ticked the app
  picks automatically - the highest-order type you asked for in File Settings,
  in the order Dry < Dry Cleaned < Wet < Wet Cleaned. This pick is a session
  setting and is not saved with the project. It only takes effect on the first
  six Vox tabs; on tabs 7-10 the menu still draws but the pick does not stick
  and the automatic rule applies.
- **Listen LED** - turns audible monitoring on. With Listen off the strip still
  processes and still records; it just does not reach the speakers, which is what
  stops feedback when you arm a mic next to monitors. **Right-click** the Listen
  LED for the monitor mode:

| Monitor mode | What you hear while singing | Default |
|---|---|---|
| True Dry | The bare microphone. No correction, no chain, no amp. | |
| Bypass Pitch Corrector | Your uncorrected voice through the chain. | |
| With Effect | The full processed sound including realtime pitch correction. | Yes |

Monitor mode changes what you hear, never what is recorded: the recorded wet
take is corrected in all three modes. Switching modes crossfades over about
10 ms so it does not click.

### What a take produces

Stopping the transport after recording writes up to four files per armed Vox
strip, named `<project> - Vox N - <timestamp> - DRY.wav`, `... - DRY CLEANED.wav`,
`... - WET.wav`, `... - WET CLEANED.wav`.

- **Dry** is the raw microphone. **Wet** is the signal after realtime pitch
  correction (it only exists if realtime correction was on when the take
  started). **Cleaned** variants are the same take with background noise
  removed.
- Which of the four get written is set in the **File Settings** dialog: four
  checkboxes (Dry / Dry Cleaned / Wet / Wet Cleaned, defaults Dry and Wet on)
  plus a **De-noise strength** of Light or Strong (default Strong). At least one
  box always stays checked, and your Builder Grid Default pick is always written
  even if its box is unchecked.
- Exactly one take goes onto the arrangement grid - the Builder Grid Default (or
  the automatic pick). The rest go into the Audio Browser only, filed under this
  Vox tab. Take types that were written but not selected are deleted.
- If a de-noise pass fails, the uncleaned take is used instead and the failure is
  named in the single problem dialog that appears after recording alongside any
  other capture problems.
- Noise removal learns the room while a mic is assigned to the strip. Assigning a
  different input restarts the learning, because it is a different room. A take
  recorded before the learner had warmed up learns its profile from its own file
  instead. The live learners run on the first six Vox tabs.

Both the Pitch and the Align editors analyze **what is on the grid**, so the take
you choose as the grid default is the one those editors work on.

## Parameters and persistence

**Strip parameters** (created per Vox strip, prefix `mixer_vox_<N>_`) - saved
with the project:

| Parameter | Range / values | Default |
|---|---|---|
| `mixer_vox_<N>_inputChannelIdx` | -1..127 (-1 = none) | -1 |
| `mixer_vox_<N>_inputChannelStereo` | on / off | off |
| `mixer_vox_<N>_listen` | on / off | off |
| `mixer_vox_<N>_monitorMode` | 0 True Dry / 1 Bypass Pitch Corrector / 2 With Effect | 2 |
| `mixer_vox_<N>_arm` and the standard level / pan / mute / solo / send set | see the mixer document | |

The friendly input-channel name is stored as a property on the processor state
(`mixer_vox_<N>_inputChannelName`), not as a parameter.

**Engine parameters** live on the `BaySickVocalProcessor`'s own APVTS under three
prefixes - `bsv_` (chain and realtime board), `bsa_` (Align), `bsp_` (Pitch).
They are listed in `Vocal Chain.md`, `Align Editor.md` and `Pitch Editor.md`.

**Saved with the project:** everything above, plus the engine's full state blob
(chain DSP state, A/B slots, pitch edits, align edits, the embedded NAM/IR)
written into the project's tab record.

**Saved with a page preset** (`PagePresetIO`, page kind Vox, written to the Vox
"My Presets" folder): the engine blob plus the mixer strip parameters, insert
rack and post-EQ. Loading substitutes Vox bus 1 when a saved routing points at a
Vox bus that is not active.

**Session only, not saved:** the Builder Grid Default pick per strip, and the
de-noise learner state.

**Per machine, not per project:** the File Settings checkboxes, de-noise
strength, auto-freeze threshold and capture-retention settings (all in the app's
UI preferences file).

**Not saved at all:** the live monitor crossfade state, the analysis staleness
counters (re-baselined on load), and any in-flight preview playback.

## Lifetime and teardown

- The page is spawned by its mixer strip: `MixerPage::addVoxChannelAtIndex`
  creates the strip and its cascade spawns the tab. Every route in - the ribbon,
  Duplicate, Add From Export, a project load - goes through that one call, and it
  is idempotent on page index so a project load cannot double-spawn.
- The constructor immediately asks the rig for the engine
  (`rig.addTab(TabKind::Vox, N)` then `setEngineType(..., "BaySickVocal")`), then
  builds the editor and stamps it with this page's automation prefix
  (`vox<N>_`). `setProcessor` follows later and injects the timeline services the
  Align and Pitch editors need (composite rendering, clip signatures, channel
  list, project folder, recording state) and starts the 4 Hz analysis poller.
- Automation lanes for this page are registered model-side off the rig's
  engine-created event, not by the editor, so lanes survive closing the window.
- On tab close: the rig removes the Vox tab (which destroys the engine), then the
  mixer strip is removed. The strip's APVTS parameters are deliberately kept, so
  re-adding a Vox tab at the same index restores the previous fader, pan and
  sends. Audio-library entries for this tab's recordings are removed; the WAV
  files stay on disk.
- The engine has an explicit shutdown gate (`setShuttingDown`) that owners raise
  before destroying it so an in-flight audio block bails out instead of touching
  half-destroyed members.

## Cross-references

- `Vocal Chain.md` - the BaySickVocals panel, the six-stage chain, A/B compare.
- `Pitch Editor.md` - BaySickPitch, the note-level editor.
- `Align Editor.md` - BaySickAlign, the two-track timing editor.
- `NAM Amp and Cab.md` - the amp capture, cabinet IR and mic simulation stage
  embedded in every Vox tab.

## Differs from Carry-Forward

- **MIX-01 is closed.** Carry-Forward records "Vox branch calls
  `unregisterVoxEngine()` but NOT `removeVoxChannel`" as confirmed open. The
  close path now calls `MixerPage::removeVoxChannel`, so the orphan strip is
  gone.
- **Recording produces up to four take types, not two.** Carry-Forward describes
  a dry file and a wet file, with the wet going on the grid and the dry going to
  the library. The current rule adds de-noise-cleaned variants of both, a File
  Settings dialog that decides which files are written at all, and a per-strip
  Builder Grid Default that decides which one lands on the grid. The old "wet if
  it exists, else dry" rule no longer applies.
- **Live monitoring has a mode.** Carry-Forward has no monitor-mode concept; the
  Listen LED now carries a three-way right-click choice.
- **The sub-tabs are windows.** Carry-Forward predates the contained-window
  shell; Vocal Chain / BaySickPitch / BaySickAlign / NAM-IR are no longer tabs
  inside the page.
