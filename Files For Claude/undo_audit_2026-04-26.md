# BaySickDAW Undo/Redo Coverage Audit — 2026-04-26

> Initial audit done by Explore agent.  This file is the working reference for
> the undo-coverage sweep that follows.  Refine + verify each row as the fixes
> ship.

## Executive summary

BaySickDAW has **partial but inconsistent** undo/redo coverage.  Core
infrastructure (`UndoManager` + `UndoContext` + custom `UndoableAction`
subclasses) is well-designed but unevenly applied.

**Well-covered areas:** Builder (ArrangementEditAction), Piano Roll
(PianoRollEditAction), Automation lanes (AutomationLaneEditAction), Effect
Rack (EffectRackAction).

**Completely unwrapped areas:** Mixer, Drum Kit Grid, Pattern Manager,
Effect editor parameters, ParametricEQDisplay band edits, Layer/Bass/Drum
tab metadata.

**Critical access gap:** Only Builder, Effects, and PianoRoll pages have
Ctrl+Z handlers.  Undo is inaccessible when the user is on MixerPage,
LayersPage Player tab, BassPage Player tab, or DrumPage Player/Drum-Kit
tabs.

**Infrastructure issue:** Player editors use APVTS Attachments (which
support gesture-based undo via `beginChangeGesture`/`endChangeGesture`),
but the processor's APVTS likely does NOT call `setUndoManager()`, so
parameter drags don't reach `StandaloneEditor`'s `UndoManager`.

---

## Per-area findings

### 1. MixerPage / MixerTrackStrip

| Action | Undoable? | Mechanism | Gaps |
|---|---|---|---|
| Fader drag | No | direct APVTS write | no transaction, no Ctrl+Z route |
| Pan drag | No | direct APVTS write | same |
| Mute / Solo toggle | No | direct APVTS write | same |
| Width drag | No | direct APVTS write | same |
| Polarity toggle | No | direct APVTS write | same |
| Arm toggle | No | direct APVTS write | same |
| FX Bypass toggle | No | direct APVTS write | same (also wired in Effects page) |
| Send level (cable amount) | No | direct APVTS write | same |
| Send target route (cable drag) | No | RoutingGraph rebuild | not undoable |
| Aux strip add | No | append to mAuxInserts | not undoable |
| Aux strip delete | No | erase from mAuxInserts | not undoable |
| Channel rename | No | direct mutation | not undoable |

**Notes:** A `MixerStateAction` class exists in `UndoActions.h/.cpp` but is
**never instantiated** anywhere in the codebase.  No Ctrl+Z handler on
`MixerPage`.

### 2. EffectsPage rack (slot-level operations)

| Action | Undoable? | Mechanism | Gaps |
|---|---|---|---|
| Load effect into slot | Yes | `EffectRackAction` | OK |
| Remove effect from slot | Yes | `EffectRackAction` | OK |
| Move slot up/down | Yes | `EffectRackAction` | OK |
| Slot bypass toggle | Partial | direct APVTS write to `_bypass` param | no transaction grouping; APVTS undo not wired |
| Channel dropdown selection | n/a | not state — view selector | (intentionally not undoable) |
| FX master bypass toggle | Partial | APVTS attachment | same as slot bypass |

**Notes:** `EffectsPage::keyPressed` has Ctrl+Z handler — works.

### 3. Effect editor panels (per-DSP)

`Source/Standalone/EffectEditorPanels.cpp` covers all 12 effect panels
(Compressor, Reverb, Saturation, Chorus, Delay, Flanger, Overdrive,
Phaser, Transient Shaper, Tape, Limiter, EQ8).

| Action | Undoable? | Mechanism | Gaps |
|---|---|---|---|
| Any knob drag (threshold / ratio / room size / damping / etc.) | No | APVTS attachment direct write | parameter changes never reach UndoManager |
| Any toggle (sync / wet only / cross / etc.) | No | APVTS attachment direct write | same |
| Combo box (waveform / mode / etc.) | No | APVTS attachment direct write | same |
| Output gain knob | No | APVTS attachment direct write | same |

**Root cause:** APVTS not wired to UndoManager.  Even if it were, JUCE's
attachment classes call `beginChangeGesture` / `endChangeGesture`
correctly — undo would Just Work the moment APVTS is bound to the manager.

### 4. ParametricEQDisplay (Effects Post-EQ + player Pre-EQ)

| Action | Undoable? | Mechanism | Gaps |
|---|---|---|---|
| Drag a band freq/gain on the curve | No | direct mutation of `mBands` + APVTS write | needs band-snapshot UndoableAction |
| Right-click band → change Q | No | direct APVTS write | same |
| Right-click band → change type (Bell/LP/HP/etc) | No | direct APVTS write | same |
| Right-click band → mute/solo | No | direct APVTS write | same |
| Add band (click empty) | No | direct mutation | same |
| Delete band | No | direct mutation | same |
| Reset All button | No | bulk APVTS write | needs full-snapshot UndoableAction |
| Mid/Side toggle | n/a | view selector — not state | (intentionally not undoable) |

**Notes:** All EQ band edits are interactive, frequent, and currently
**zero-undo**.  Needs a snapshot-based action that captures all 8 mid +
8 side band states (freq / gain / Q / type / on) before each edit.

### 5. BuilderPage / ArrangementGrid

| Action | Undoable? | Mechanism | Gaps |
|---|---|---|---|
| Place block | Yes | `ArrangementEditAction` | OK |
| Move block | Yes | `ArrangementEditAction` | OK |
| Resize block | Yes | `ArrangementEditAction` | OK |
| Delete block | Yes | `ArrangementEditAction` | OK |
| Cut / Copy / Paste / Duplicate selection | Yes | `ArrangementEditAction` | OK |
| Marquee selection | n/a | view state — not undoable | (intentionally) |
| Track row rename | No | direct mutation of `mRowNames` | gap — minor but worth fixing |
| Pattern color / metadata change | n/a | (TODO — verify) | check |
| Block clip-type change (Pattern → Audio etc.) | No | direct mutation | rare op but unwrapped |

**Notes:** Ctrl+Z works.  Coverage is broad.

### 6. Layer / Bass / Drum tabs (per-tab metadata + actions)

| Action | Undoable? | Mechanism | Gaps |
|---|---|---|---|
| Engine pick (first time, locks combo) | No | direct create | not undoable |
| Lock toggle | No | direct mutation of `mLocked` | not undoable |
| Polyphony toggle (engine-aware) | No | direct APVTS write | not undoable |
| Sound load (drum picker) | No | direct loadSampleFile/Folder/SFZ + APVTS replace | major action, not undoable |
| Patch load (preset picker) | No | direct apvts.replaceState | major action, not undoable |
| Save Patch As | n/a | disk write — not stateful in app | (intentionally) |
| Delete drum/layer/bass tab | No | mPages.remove + ribbon close | major action, not undoable; would need full tab snapshot to restore |
| Copy / Paste / Duplicate drum/layer/bass | No | XML clipboard | not undoable |
| Tab rename | No | direct ribbon string + mixer/context-label mutation | not undoable |

**Notes:** Player tabs do NOT have a Ctrl+Z handler.  Undo is unreachable
from the Player sub-tab, the Drum Kit tab, or the EQ tab on these pages.

### 7. Player editors (Harmless / BaySickSynth / BaySickBass / VibePlayer)

All four editors share the same pattern: knobs / switches / combos via
APVTS attachments.

| Action | Undoable? | Mechanism | Gaps |
|---|---|---|---|
| Any knob drag (cutoff / resonance / env / etc.) | No | APVTS attachment | APVTS not wired to UndoManager |
| Any switch toggle | No | APVTS attachment | same |
| Any combo (waveform / filter type / mode) | No | APVTS attachment | same |
| Waveform clicks (Harmless waveform button) | No | direct mutation | same |
| XY pad drag (BaySick filter pad, Harmless mod pad) | No | APVTS attachments | same |
| Routing matrix slider drag (Harmless) | No | APVTS attachment | same |

**Critical:** all of these become undoable for free (or very nearly free)
the moment we wire APVTS to UndoManager.  Attachments already call
`beginChangeGesture` / `endChangeGesture` correctly.

### 8. Piano Roll (Layers / Bass / per-drum)

`PianoRoll.cpp` — covers `PianoRollGrid` and `PianoRollContainer`.

| Action | Undoable? | Mechanism | Gaps |
|---|---|---|---|
| Add note (Draw / Paint tool) | Yes | `PianoRollEditAction` | OK |
| Delete note (Delete tool / Right-click / Backspace) | Yes | `PianoRollEditAction` | OK |
| Move note | Yes | `PianoRollEditAction` | OK |
| Resize note | Yes | `PianoRollEditAction` | OK |
| Slice notes | Yes | `PianoRollEditAction` | OK |
| Mute / Unmute notes | Yes | `PianoRollEditAction` | OK |
| Group / Ungroup | Yes | `PianoRollEditAction` | OK |
| Cut / Copy / Paste / Duplicate | Yes | `PianoRollEditAction` | OK |
| Tool dialogs (Quantize / Strum / Arpeggio / Chop / etc.) | Yes | `PianoRollEditAction` (snapshot) | OK |
| Note type cycle (S key) | Yes | `PianoRollEditAction` | OK |
| Time selection on ruler | n/a | view state — not undoable | (intentionally) |

**Notes:** Ctrl+Z handler at line 798 — works.  Coverage is excellent.

### 9. Drum Kit grid (DrumPage's Drum Kit sub-tab)

| Action | Undoable? | Mechanism | Gaps |
|---|---|---|---|
| Drum row mute toggle | No | direct mutation | not undoable |
| Drum row solo toggle | No | direct mutation | not undoable |
| Lock toggle | No | direct mutation | not undoable |
| Reorder drum rows (drag handle) | No | mPages reorder | not undoable |
| Add new drum (+ button) | No | spawnDrumTab | not undoable |
| Note placement on grid (Draw) | Yes | `PianoRollEditAction`-equivalent | OK |
| Note delete on grid | Yes | `PianoRollEditAction` | OK |
| Note move/resize | Yes | `PianoRollEditAction` | OK |

**Notes:** Note ops are tracked (shared with PianoRoll's edit machinery).
Row metadata ops are NOT.  No Ctrl+Z handler on `DrumKitContainer`.

### 10. EventEditor (automation lane editor)

`EventEditor.cpp` — opens as a floating window.

| Action | Undoable? | Mechanism | Gaps |
|---|---|---|---|
| Add point | Yes | `AutomationLaneEditAction` | OK |
| Move point | Yes | `AutomationLaneEditAction` | OK |
| Delete point | Yes | `AutomationLaneEditAction` | OK |
| Adjust value | Yes | `AutomationLaneEditAction` | OK |
| Interpolate / smooth | Yes | `AutomationLaneEditAction` | OK |
| Cut / Copy / Paste lane | Yes | `AutomationLaneEditAction` | OK |
| Tool button (Draw/Paint/Erase/etc.) | n/a | view state | (intentionally) |

**Notes:** Has its own keyPressed handler (line 1727) handling Delete +
Backspace.  Window-scope undo via UndoContext.  Coverage excellent.

### 11. Pattern manager (transport-bar dropdown)

| Action | Undoable? | Mechanism | Gaps |
|---|---|---|---|
| Create new pattern | No | direct PatternManager call | not undoable |
| Rename pattern | No | direct mutation | not undoable |
| Delete pattern | No | direct mutation | not undoable; destructive — high-priority undo target |
| Clone pattern | No | direct create | not undoable |
| Color change | No | direct mutation | not undoable |
| Switch active pattern | n/a | view state — not undoable | (intentionally) |

---

## Global Ctrl+Z routing

**Current path** (per-page, NOT global):

1. User presses Ctrl+Z while focused inside one of: Builder grid, Effects
   page, Piano roll grid, or EventEditor window.
2. That component's `keyPressed` override fires, calls
   `mUndoCtx.undo()`.
3. `UndoContext::undo` invokes `StandaloneEditor::globalUndo`.
4. `globalUndo` calls `mUndoManager.undo()` and refreshes the history
   window if open.

**Pages with Ctrl+Z handlers:**
- `BuilderPage::ArrangementGrid::keyPressed` — line 2799
- `EffectsPage::keyPressed` — line 815
- `PianoRollGrid::keyPressed` — line 798
- `EventEditorContent::keyPressed` — line 1727 (separate window)

**Pages WITHOUT Ctrl+Z handlers — undo unreachable from these contexts:**
- `MixerPage`
- `LayersPage` (Player tab + Pre-EQ tab)
- `BassPage` (Player tab + Pre-EQ tab)
- `DrumPage` (Drum Kit tab + Player tab + Pre-EQ tab — only the Piano
  Roll sub-tab works because that's `PianoRollGrid`)
- Any embedded player editor (Harmless / BaySickSynth / BaySickBass /
  VibePlayer) when its window has focus

**Edit menu fallback:** items 201 / 202 invoke `globalUndo` / `globalRedo`
directly — works from anywhere if the user goes through the menu.  But
keypress-driven undo is broken on the pages above.

---

## Priority fix ordering (proposal — not yet executed)

1. **Migrate Ctrl+Z / Ctrl+Alt+Z to BSCommands as global commands.**  This
   alone makes undo reachable from every page (they fall through to the
   command manager).  Drop the per-page keyPressed handlers afterward.

2. **Wire APVTS to UndoManager** on each player processor (Harmless /
   BaySickSynth / BaySickBass / VibePlayer) and on `VibeSynthProcessor`
   itself (mixer + effect-panel params).  Single change per processor;
   immediately enables undo for hundreds of params.

3. **MixerStateAction** — instantiate the existing class for fader / pan /
   mute / solo / width / polarity / arm changes.

4. **EQBandSnapshotAction** — band drags + right-click param changes +
   Reset All on `ParametricEQDisplay`.

5. **DrumKitStateAction** — mute / solo / lock / reorder on Drum Kit grid.

6. **InstrumentTabAction** — engine pick / lock / polyphony / sound load /
   patch load / delete / rename on Layer/Bass/Drum tabs.

7. **PatternAction** — create / rename / delete / clone / color on
   transport-bar dropdown.

8. **ArrangementRowRenameAction** — Builder row name (small gap).

9. **EffectRackOutputGainAction** — slot output gain not currently in
   `EffectRackAction`.

10. **Per-strip routing edge changes** — cable drag on mixer (low priority,
    rare op).
