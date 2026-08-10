# Align Editor (BaySickAlign)

**Purpose** - BaySickAlign makes one vocal take line up with another. Pick a
Leader (the take whose timing is right) and the Follower is this Vox tab; press
Analyze/Apply and every word of the Follower is nudged into time with the
Leader's. It can optionally pull the Follower's tuning toward the Leader's too.
The result plays back live - the Render button is only for exporting the aligned
take as a file.

## How it operates

`BaySickAlignEditor` (`Source/BaySickVocal/BaySickAlignEditor.cpp` / `.h`) is the
UI; `BaySickAlignDSP` and the align state on the tab's `BaySickVocalProcessor`
hold the analysis and the map.

- **Analysis** (`BaySickVocalProcessor::analyzeAlign`, message thread only)
  renders both channels' composites through the page-injected
  `onRenderComposite` hook, front-pads them both to whichever starts earlier so
  the two share a t=0, detects word starts on each, pairs them, and builds a warp
  map of anchors. Each anchor carries a Follower time, a Leader time, a weight and
  a pitch delta.
- **Analyze IS Apply.** A successful analysis immediately appends a version
  restore point and publishes the map to the playback decode layer, so the very
  next audio block plays aligned. There is no separate apply step and no bake.
- **Playback** reads an immutable `AlignPlaySnapshot` through one atomic load per
  block; the clip decode layer warps the Follower's read position live. Retired
  snapshots are held on the message thread in a ring of eight.
- **The Follower's own pitch-editor time edits are consumed first.** BaySickPitch
  is upstream: the align analysis transforms the Follower's raw onset times
  through that channel's published time map before pairing, and a change to it
  marks the alignment stale.
- **Staleness.** The analysis goes stale if either channel's clip layout changes,
  if a time-map knob (Mode, Fine Tune, Max Shift, or either Pitch Type) moves, or
  if the Follower's pitch time map changes. The toolbar shows `RE-ANALYZE`, or
  `RE-ANALYZE ON STOP` while the transport runs; the Vox page's 4 Hz poller
  re-runs it at the next stop once the layout has been stable about a second.
- **Maps only change while stopped.** Re-analysis, Versions revert, Undo and Redo
  are all disabled during playback. The one carve-out is the *first* analysis of a
  never-analyzed pair, which runs even during playback - there is nothing applied
  to swap away from, and the engagement glide absorbs it.
- **Blend and Variation are live.** Those two knobs re-publish the snapshot
  immediately without touching the time map, so they can be moved mid-play.

## User-facing behavior

Open it from the Vox tab's **Menu -> BaySickAlign**. It opens in its own window
(1047 x 723 minimum).

### Layout

A toolbar across the top, a shared time ruler, then three stacked lanes with two
thin strips between them, a view-mode bar along the bottom, and a settings panel
down the right side.

| Lane | Color | What it is |
|---|---|---|
| **LEADER** | green | The take you are matching to. Its header has the only channel picker - pick any channel in the project that has audio clips. |
| **FOLLOWER** | teal | Always this Vox tab. The header says "(this page)" - there is no picker. |
| **OUTPUT** | red | A preview of the warped Follower, rendered after each analysis. |

Between Leader and Follower is the **SYNC POINTS** strip; between Follower and
Output is the **PROTECTED** strip.

### Toolbar

| Control | What it does |
|---|---|
| Preset dropdown | Six factory presets - **Loose-Align**, **Loose-Align+Pitch**, **Close-Align**, **Close-Align+Pitch**, **Tight-Align**, **Tight-Align+Pitch** - plus **(User)**. Each sets Mode and the Pitch switch together, and the +Pitch variants also seat Blend at that mode's value (Loose 0 %, Close 50 %, Tight 100 %). |
| Green dot | Lights next to the preset dropdown when the current settings differ from the selected preset. |
| **Save** / **Load** | Save or recall the whole Align + Pitch settings block as a named user preset. Saving switches the preset dropdown to (User). |
| **Analyze/Apply** | Pairs the Follower's word starts to the Leader's, builds the warp, and applies it to playback. Grayed during playback once an alignment exists - stop first. A short `ANALYZING...` badge appears while it works. |
| **Versions** | Reverts to an earlier applied state, newest first. Every Analyze/Apply creates one. Entries whose grid has changed since are marked "(grid changed)"; reverting to one is allowed and lights the stale badge. Grayed during playback. |
| **Render** | Exports the aligned Follower to `Aligned/{name}_align_v{N}.wav` in the project folder. Offers **Standard** or **High Resolution (slower)** - the high-resolution pass runs the warp oversampled for finer transients. File only; playback is already live. |
| **Undo** / **Redo** | A local history covering sync points, protected areas and analyses. Grayed during playback. |

### Sync points - forcing a pairing

Sync points are hard anchors you place when the automatic pairing gets a phrase
wrong. In the SYNC POINTS strip:

- **Click empty space** to drop a new sync point and drag it immediately.
- **Drag** the top half of an existing marker to move its Leader time, the bottom
  half to move its Follower time. A line joins the pair.
- **Right-click** a marker for **Delete Sync Point**.
- **Right-click** anywhere for **Automatic Sync Points**, which clears the list
  and seeds a sparse set from the current analysis pairing as a starting point to
  adjust. It requires an existing analysis and says so if there is none.

### Protected areas - "leave this bit alone"

In the PROTECTED strip, **drag** to create a region (drags under about 20 ms are
ignored as accidental clicks). **Right-click** a region for **Protect Timing**,
**Protect Pitch** (both are independent tick items) and **Delete Protected
Area**. The region is drawn with a `T` and/or `P` tag showing which protections
are on.

Sync-point and protected-area edits mark the analysis stale - they take effect at
the next analysis.

### View mode bar

**Wave** / **Pitch** / **Energy** switch what all three lanes draw - the waveform,
the detected pitch contour, or the RMS energy. **+** and **-** zoom time.

### The right-hand settings panel

**ALIGN box**

| Control | What it does | Range | Default |
|---|---|---|---|
| **ON** | The alignment switch. Off returns the Follower to its natural timing. | on / off | ON |
| **Mode** | How tightly timing is corrected. Tight locks every word to the Leader; Close and Loose leave more natural timing in place. Changing Mode also presets the Blend knob. | Loose / Close / Tight | Close |
| **Fine Tune** | How much natural timing each word may keep. Words already within this many milliseconds of the Leader are left alone; words further out are pulled to the edge. The knob sweeps the Mode's own window - Tight 0-50 ms, Close 50-150 ms, Loose 100-200 ms, with 12 o'clock in the middle. The readout shows the resulting milliseconds. | -50 to +50 (displayed in ms) | 0 (center) |
| **Max Shift** | The furthest any single word may be moved. Fully clockwise reads **No Limit**, which is the default and works for most takes; turn down to stop a far-out word being dragged across the bar. | 10-160 ms; above 150 shows "No Limit" | No Limit |

**PITCH box** - optional, off by default.

| Control | What it does | Range | Default |
|---|---|---|---|
| **ON** | Turns pitch matching on. | on / off | off |
| Engine dropdown | **Rubber Band - Balanced**, **Signalsmith - Lightest (Low CPU)**, **WORLD - Highest Quality (High CPU)**. | | Rubber Band |
| **Leader Type** / **Follower Type** | The pitch detection band for each side. Pick the one that matches the material so detection locks onto the right octave. Takes effect at the next analysis. | Normal (60-1200 Hz), High Vocal (160-1400), Low Vocal (50-500), High Instrument (200-2400), Low Instrument (30-350) | Normal |
| **Blend** | Percent of the pitch difference (beyond the Variation cap) pulled toward the Leader's contour. 0 is off, 100 is a full pull. Live - no re-analysis needed. | 0-100 % | 50 % |
| **Variation** | How much natural tuning variation the Follower keeps, in semitones. Differences inside the cap are untouched; only the excess is pulled, and then scaled by Blend. Live. | 0-6 semitones | 0 |
| **Transpose** | A flat semitone shift applied on top of the pitch matching. | -12 to +12 | 0 |
| **Formant** (toggle) + **Formant Shift** (knob) | Shifts the vocal character without re-pitching - up for thinner, down for fuller. | off / on; -12 to +12 semitones | off; 0 |

All knobs use the standard feel: a detent at the default value, Ctrl for fine
movement, and a value box you can type into.

### When an analysis fails

The analysis reports in plain language and leaves any previous alignment applied.
Typical messages:

- "Pick a Leader channel first."
- "Leader and Follower must be different channels."
- "The Leader channel has no audio clips." / "...Follower channel has no audio
  clips."
- "Leader and Follower are too far apart on the timeline."
- "Could not detect enough word starts (Leader: N, Follower: M). Check both
  channels have vocal clips with clear words. The previous alignment (if any) is
  still applied."
- "Found N Leader / M Follower word starts, but only K lined up within the +/-X ms
  matching window. Check both takes sit at the same spot on the grid. The
  previous alignment (if any) is still applied."

Render failures are reported the same way - most commonly "Save the project first
- renders live in the project folder."

## Parameters and persistence

Parameters live on the Vox tab's APVTS under `bsa_`. They are offline settings -
read at action time on the message thread, never pushed per block, except the
three the playback layer reads directly (`bsa_align_on`, `bsa_pitch_on`,
`bsa_pitch_transpose`).

| Parameter | Range | Default |
|---|---|---|
| `bsa_align_on` | bool | true |
| `bsa_align_mode` | 0 Loose / 1 Close / 2 Tight | 1 |
| `bsa_align_fineTune` | -50 to +50 | 0 |
| `bsa_align_maxShift` | 10-160 ms (>150 = No Limit) | 160 (No Limit) |
| `bsa_pitch_on` | bool | false |
| `bsa_pitch_range` (displayed as **Blend**) | 0-100 % | 50 |
| `bsa_pitch_variation` | 0-6 semitones | 0 |
| `bsa_pitch_typeGuide` / `bsa_pitch_typeDub` | 0-4 (the five detection bands) | 0 (Normal) |
| `bsa_pitch_algo` | 0 Rubber Band / 1 Signalsmith / 2 WORLD | 0 |
| `bsa_pitch_transpose` | -12 to +12 semitones | 0 |
| `bsa_formant_on` | bool | false |
| `bsa_formant_shift` | -12 to +12 semitones | 0 |
| `bsa_preset` | 0-6 (six factory + User) | 2 (Close-Align) |
| `bsa_preset_dirty` | bool | false |
| `bsa_leader_channel` | -1..999 (-1 = none picked) | -1 |
| `bsa_follower_channel` | -1..999 (-1 = this page's channel) | -1 |

A Flexibility picker used to sit alongside Max Shift; it was removed and the
engine now always runs the Normal 2:1 stretch bound.

**Saved with the project**, inside the Vox engine's state as an `<AlignEdits>`
child: the analyzed flag, both channels' clip signatures captured at analysis
time, the common-origin frame (start beat, start sample, both pad lengths,
analysis sample rate), the warp map with its per-anchor pitch deltas, every sync
point, every protected area with its Protect Timing / Protect Pitch flags, the
`Aligned/` render history (file, date, version, origin beat) and up to 20 applied
version snapshots.

A restored map goes live immediately. Projects saved before the origin sample was
stored derive it from the stored beat through the tempo timeline.

**Saved as a user preset file** (`Presets/BaySickAlign/My Presets/<name>.xml`):
all thirteen `bsa_` settings listed as preset parameters above (everything except
`bsa_preset`, `bsa_preset_dirty` and the two channel picks).

**Not saved:** the horizontal view state and zoom, the view mode, the local
undo/redo stack, and the two staleness generation counters, which are
re-baselined on load.

## Lifetime and teardown

- The editor is built with the Vox tab's `BaySickVocalEditor` and lives as long
  as the tab, whether or not its window is open - an analysis must survive
  closing the window.
- Its right-hand panel registers an APVTS listener on `bsa_align_mode` and
  removes it in its destructor.
- The published playback snapshot is owned by the processor; the processor's
  destructor deletes it after the owner has settled the audio thread.
- A restore in progress raises a flag on the processor so the Mode hook's queued
  Blend preset cannot overwrite the Blend value the project just loaded.

## Cross-references

- `Vox Page.md` - the tab, the composites this editor analyzes, the auto
  re-analysis poller, and the "+ Add New Vox From Export" route that brings a
  rendered aligned take back into the project.
- `Pitch Editor.md` - upstream of this editor; its time edits redefine the
  performance alignment matches.
- `Vocal Chain.md` - the chain the aligned playback is voiced through.

## Differs from Carry-Forward

Carry-Forward (2026-05-07) lists BaySickAlign only as an open defect line
("DSP-05 / QA-F / BaySickAlign review"). Everything above - the three-lane
layout, the sync-point and protected-area strips, the live applied map, the
version history, and the export-only Render - postdates that snapshot. In
particular, alignment is applied to live playback rather than requiring a bake,
and the render is an export that never places itself on the grid.
