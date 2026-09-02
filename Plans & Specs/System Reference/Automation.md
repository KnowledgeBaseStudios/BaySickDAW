# Automation

**Purpose** - Automation is how a control moves by itself while the song plays: a
filter opening across eight bars, a reverb swelling into a chorus, the tempo
easing up. You draw the movement as a clip on the arrangement, on its own track
row, and it drives the control it is bound to for as long as the clip lasts.

## How it operates

**A lane is a clip.** An automation clip is an ordinary arrangement block whose
type is Automation and which carries an `AutomationLane`
(`Source/PatternManager.h`). The lane holds a stable `paramId` (never shown to
you), a list of control points with time, value, curve type and tension, and an
optional LFO mode with shape, rate, minimum and maximum. It also holds two names:
a `userDisplayName` you set by renaming the lane, and a `lastKnownName` captured
when the lane was created, so a lane whose target has since been deleted still
says which effect it drove.

**One evaluator.** `evalAutomationLaneAt` in `Source/PatternManager.h` is the
single function that turns a lane plus a position into a value. Every replay path
calls it - the editor's UI tick, the audio thread's pass, and both offline
passes - so the curve you draw is the curve that plays and the curve that exports.
Its points are assumed sorted, because it runs on the audio thread and cannot
allocate; every mutation site sorts.

**Two dispatch surfaces.** `StandaloneEditor` holds two maps keyed by `paramId`:
`mAutomationApplicators` (given a 0..1 value, drive the target) and
`mAutomationValueReaders` (read the target's current value, used to seed a new
lane). Registration is **model-side**: it happens when an engine is created, when
a mixer strip's parameters are materialized, when a rack or pedal slot changes,
when a sampler engine becomes ready, and when a hosted plugin reports its
parameter list. It never happens from a widget - windows are destroyed when you
close them, so a widget-scoped registration would be a guaranteed dead lane.

Every applicator **re-resolves its target through the model at apply time**,
never through a captured pointer. That is what lets a lane survive an engine
swap, a kit load, a tab being closed and reopened, or an undo that resurrects a
tab.

**Replay paths.**

- **Playing, in song mode:** parameters that live in the main parameter tree are
  written by the audio thread inside `processBlock`; everything else (engine
  parameters, `global_tempo`) is driven from the editor's 30 Hz tick.
- **Stopped, in song mode:** the editor tick applies everything when the playhead
  *moves* - seek, scrub, or click the ruler - so every automated control snaps to
  its value at that point. It deliberately does nothing while stopped and static,
  so a value you nudge by hand is not immediately overwritten.
- **Offline (export, measure, freeze):** `BuilderPage::applyOfflineLaneValue`
  mirrors the live applicator map. This is a standing rule with teeth: every lane
  class needs a branch there or it plays live and silently vanishes from exports.

**Song mode only.** Automation clips live on the arrangement, so they drive
nothing in pattern mode. Both live paths gate on song mode, and the processor
snapshots the pre-automation value of every automated parameter when you enter
song mode and restores it when you leave, so pattern mode is not left holding
whatever the last automation tick wrote.

Row mute and solo apply to automation rows exactly as they do to any other row,
in every replay path - so the session and the export agree.

## User-facing behavior

### Making an automation clip

**Right-click almost any control** and pick the first item, **Automate: `<name>`**.
That creates an automation clip bound to that control and opens the Event Editor
on it. If a clip for that control already exists, you are taken to it rather than
getting a second one.

The right-click menu on a control carries:

| Item | What it does |
|---|---|
| Automate: `<name>` | Create (or jump to) the automation clip for this control. |
| Type in value... | Type an exact value instead of dragging. |
| Modulate envelope... | Only on engines with a modulation matrix (BaySickSolstice). Opens that control in the engine's own mod editor. |
| MIDI Learn / MIDI Forget / Save MIDI mappings as global default | See *MIDI Learn*. |

Two more creation surfaces:

- **Edit > New Automation Clip** (also on the Builder's Clips menu) lists every
  automatable parameter and lets you pick from the list.
- **A hosted plugin's window Menu > Automate** offers "Last Touched: `<name>`" -
  move a control inside the plugin's own editor, then pick this - plus the
  plugin's full parameter list, chunked into groups of 30 when it is long.

A new clip is placed on the next free track row. If you have a time selection on
the Builder ruler it spans that; otherwise it spans the whole arrangement so far,
or one bar if there is nothing yet. It starts as a flat line at the control's
**current value**, so creating a lane never changes your sound until you draw on
it.

### Editing a clip

Double-click an automation clip on the Builder grid (or use the block right-click
menu's "Open in Event Editor") to open the **Event Editor**. Its menu bar:

| Menu | Items that work |
|---|---|
| File | Close |
| Edit | Undo (Ctrl+Z), Redo (Ctrl+Alt+Z), Select All (Ctrl+A), Erase to Range Start, Convert to Clip (Douglas-Peucker)... |
| Tools | Draw (P), Paint (B), Erase (D), Interpolate (I), Select (E), Zoom (Z) |
| View | Snap submenu (Bar / Beat / 1/8 / 1/16 / 1/32 / None), Toggle LFO Mode |
| Import MIDI | Import MIDI CC Data... (Ctrl+M) |

The tool letters also work as bare keypresses. Undo and redo here are the app's
one history, not a separate editor history.

A **New Automation** button on the toolbar lists every automatable parameter and
creates a lane for the one you pick, switching the editor to it.

**LFO mode** replaces the drawn points with a repeating shape, with its own Rate,
Min and Max knobs. Useful for a steady wobble you do not want to draw by hand.
Two caveats, both recorded in `Event Editor.md`: the lane can store Sine,
Triangle, Saw or Square and playback honors all four, but **no control writes
that field, so an LFO lane is always Sine**; and the Rate knob's units differ
between the drawn preview and playback, so the shape you see is not the shape you
hear.

Some menu entries in the Event Editor are present but do nothing:
File > New Event Editor, File > Save Automation Data, Edit > Copy, Edit > Paste,
View > Sync Zoom with Piano Roll, and the whole Target Control menu (Add Target,
Remove Target, Edit Targets, Animate Target, Locate in Mixer).

### What can be automated

Almost everything with a value:

- Every mixer strip control - level, pan, width, mute, solo, polarity, rack
  bypass, send amounts and send pre/post.
- Every band of both EQ banks on every strip.
- Every knob and toggle in every effect, in the rack and on the pedal board.
- Every engine parameter on every instrument tab, including per-tab parameters
  that only exist once that tab is created.
- Hosted VST3 plugin parameters, both instruments and effects.
- BaySickSolstice's per-target modulation depth and length.
- **Tempo**, via right-clicking the BPM field in the transport bar. The tempo
  lane is a live override - it drives playback without rewriting the project's
  stored base tempo.

Lanes are named for you: mixer-strip things are prefixed **Mx**, instrument-page
things **Pg**, so it is clear at a glance where a lane points. You can rename a
lane to anything you like, and revert to the automatic name later.

If a lane's target genuinely no longer exists (you deleted the effect it drove),
the lane is **kept, not deleted** - it simply drives nothing, and its row still
says what it used to drive. Put an effect back or reopen the tab and it re-binds.

### Where automation clips live

On the Builder grid, on their own track rows, alongside pattern clips and audio
clips. They can be moved, resized, muted and deleted like any other block, and the
browser lists them so you can find them again. Muting an automation clip, or
muting its row, stops it driving anything.

## Parameters and persistence

Automation is **project data**, saved inside `<PatternManager>` in `project.xml`:

| Stored | Meaning |
|---|---|
| `paramId` | The stable key. Never displayed. |
| `userDisplayName` | Your rename, when you have set one. Takes precedence everywhere in the UI. |
| `lastKnownName` | The auto-resolved label captured at creation, while the target still existed. What a deleted-target lane falls back to. |
| `isLFO`, `lfoShape`, `lfoRate`, `lfoMin`, `lfoMax` | LFO mode settings. `lfoShape` is stored and honored on playback but nothing writes it. `lfoRate`'s units diverge between drawing and playback - see `Event Editor.md`. |
| `<Point time value curve tension>` | One per control point. |

Reusable lanes saved for later live in `<AutomationTemplates>` in the same
document.

The clip's own placement - track row, start, length, muted - is stored on the
arrangement block that carries the lane, like any other clip.

Lane ids for rack and pedal effects are built from the channel prefix plus the
**slot's UUID** plus the control's suffix, not the slot index. That is why moving
an effect up or down in a rack does not break its automation, and why swapping the
effect in a slot does (a swap retires the old UUID and mints a new one).

Nothing about automation is per-machine, and nothing is saved with a preset or a
template.

## Lifetime and teardown

- Applicators are registered from model events and are valid for as long as the
  app runs. They are removed in exactly two cases: when an engine tab is torn
  down, and when a rack or pedal slot's UUID retires. Without the second, an
  effect swap would leave a dead "Automate" target behind on every swap for the
  rest of the session.
- Slot-UUID de-registration destroys closures, so it is message-thread only and
  must never run under a lock the audio thread wants.
- Loading an FX rack preset replaces every slot with the preset's own UUIDs, so
  the page re-registers automation for whatever actually landed.
- A hosted plugin's parameter list can arrive asynchronously after the plugin
  loads, so registration is re-run (harmlessly) when you open its Automate menu -
  otherwise a lane created there could have no applicator behind it.
- Entering song mode snapshots the current value of every automated parameter;
  leaving it restores them. That restore is a programmatic write and never enters
  the undo history.
- Creating an automation clip is one undo step covering both the new block and
  the template registration that goes with it.
- A lane whose target cannot be resolved is logged once per parameter per session
  rather than repeatedly - the tick runs 30 times a second.

## Cross-references

- *Freeze and Export* - offline lane replay, which is what makes an export sound
  like the session.
- *Undo History* - automation edits and clip creation as undo steps.
- *MIDI Learn* - the other way to drive a control from outside, sharing the same
  right-click menu.
- *Presets* - what happens to lanes when a preset replaces the effect they point
  at.
- *Projects and Saving* - where lane data is stored.

## Differs from Carry-Forward

- Carry-Forward section 3 records two open items here. **UI-01** (a right-click on the
  open popup activating the item) is JUCE's own default popup behavior and is
  noted there as needing a wrapper. **UI-02** describes automation lanes bound to
  a stale slot UUID at creation. The registration model has since moved wholesale
  to the model side, with applicators re-resolving their target at apply time and
  a de-registration path fired with the retiring UUID; the per-widget registration
  helpers that item was written against no longer exist.
