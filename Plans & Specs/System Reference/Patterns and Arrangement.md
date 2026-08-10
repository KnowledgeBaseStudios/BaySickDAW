# Patterns and Arrangement

**Purpose** - A pattern is a short piece of music you write once and use many times: a drum
groove, a bass line, a chorus. Every instrument in the project has its own set of notes inside
each pattern, so one pattern holds the whole band playing that section. The arrangement is
the map of where those patterns happen, built by dropping pattern blocks onto the Builder
timeline.

---

## How it operates

`PatternManager` (`Source/PatternManager.cpp/.h`) owns both halves:

- **`std::vector<Pattern> mPatterns`** - the pattern list, plus a single "current pattern"
  index that every Piano Roll follows.
- **`std::vector<ArrangementBlock> mArrangement`** - the flat list of clips on the Builder
  grid. A block is a pattern block, an audio clip or an automation clip, distinguished by its
  `clipType`.

A `Pattern` contains one `PianoRollData` per instrument slot: an array for Layers pages, one
for Bass, one for Drums, one for Clips, Vox, Inst and Plugins pages, and a single roll for the
big drum-kit engine. It also carries a name, a color, and a time signature.

Timing is stored in ticks at 96 per beat. A block's start (`startBeats`) is a single beats
value and can be negative when slip-editing exposes audio recorded before the clip began; its
length is exact ticks with a whole-bar fallback.

Nothing on the audio thread reads these vectors directly. `PatternManager` publishes an
immutable snapshot of the roll table and the audio thread takes one acquire-load per block.
Structural edits - adding, removing, moving, reordering patterns - run inside a
`ScopedAudioShield`, which quiets the audio thread and waits for it to acknowledge two block
boundaries before the list is touched, because a half-remapped arrangement read mid-edit would
play the wrong pattern.

Patterns are referenced **by index**: arrangement blocks, linked time-signature markers and
the current selection each store one. So `insertPattern`, `movePattern` and `removePattern`
each re-point every stored reference through their own index map rather than just editing the
list.

---

## User-facing behavior

### Pattern mode and Song mode

The transport has two modes, toggled by the SONG button or the `L` key.

- **Pattern mode** loops the pattern you have selected, over and over. This is where you
  write.
- **Song mode** plays the Builder arrangement from the playhead - every pattern block, audio
  clip and automation clip in the order you laid them out.

Automation clips only drive controls in Song mode.

### Selecting a pattern

The pattern button sits on the main toolbar just right of the transport controls. It shows the
current pattern's name (with a signature suffix like "Synths 7/8" when the pattern is not in
4/4) and a small arrow. Clicking it opens:

| Item | What it does |
|---|---|
| The pattern list | Every pattern, with the current one ticked. Click to switch. |
| New Pattern | Add an empty pattern at the end and select it. |
| Rename... | Rename the current pattern. |
| Change Color... | Pick the color its blocks are drawn in. |
| Set Time Signature... | Type a signature for the current pattern. The label says whether it is user-set or following. |
| Current Time Signature (new patterns) | Which signature flag brand-new patterns should adopt. Grayed out until at least two flags exist. |
| Delete | Remove the current pattern. Grayed out when only one is left. |

The **Patterns** menu on the main menu bar carries the same operations as keyboard commands.
All seven items:

| Item | Key | What it does |
|---|---|---|
| Rename / Color | **F2** | One dialog with both the name and the color. OK applies both, Cancel applies neither, and it is one undo step. |
| Find Next Empty Pattern | **F3** | Jump to the first pattern after this one that has no notes, wrapping around. Does nothing if every pattern has notes. |
| Insert One | **Shift+Ctrl+Insert** | Add a new empty pattern directly after the current one and select it. Patterns after it shift down a slot, and everything pointing at them follows. |
| Clone | **Alt+C** | Full copy of the current pattern - every note comes with it - selected on creation. The copy is named "<name> (copy)". |
| Delete | (none) | Remove the current pattern, after a confirmation prompt naming it. Disabled when only one remains. There is deliberately no key: bare Delete belongs to whichever editing surface has focus. |
| Move Up | **Shift+Ctrl+Up** | Swap with the pattern above. Disabled when already first. |
| Move Down | **Shift+Ctrl+Down** | Swap with the pattern below. Disabled when already last. |

Two more keys sit outside that menu: **F4** creates a new empty pattern and selects it, and
**+** / **-** cycle to the next and previous pattern, wrapping at the ends.

The Builder's browser also has a **Patterns** tab: one draggable box per pattern with **+ Add**
and **Delete** buttons, and a right-click menu offering Rename, Duplicate, Change Color, Render
to WAV, Split by Player Engine and Delete. The browser's Delete removes the pattern
immediately with no confirmation prompt; the Patterns menu's Delete asks first.

### How long a pattern is

You never set a pattern's length by hand. It is worked out from the notes in it: the furthest
note end in *any* of its instrument rolls, rounded up to the next bar line of that pattern's
own signature, with a floor of one bar. Write a note that runs into bar 5 and the pattern
becomes 5 bars long; delete it and the pattern shrinks back.

That length is what the pattern loops at in Pattern mode, and it is the length a pattern block
gets when you place it on the timeline.

### Putting a pattern on the timeline

Three ways, all on the Builder page:

1. **Drag** the pattern's box from the browser onto a row. A translucent ghost shows where it
   will land, already sized to the pattern's content.
2. **Click** the pattern in the browser to arm it, then use the **Draw** tool and click an
   empty spot on a row.
3. **Paint** to lay a run of copies end to end along a row.

Blocks snap to the current Builder snap setting. Hold Alt to place freely.

**A pattern block plays its pattern once, not on a loop.** It is a window onto the pattern's
content: if you drag a block longer than the pattern's own length, the extra is silent rather
than repeating. To hear the pattern four times, place four blocks - which is exactly what the
Paint tool does in one drag.

Clicking an existing block primes the "brush", so the next empty-space Draw click places a
faithful copy of it - same pattern, same length, same content offset.

**Slicing** a pattern block with the Slice tool (`C`) cuts it into two blocks. The right-hand
piece keeps playing what it would have played uncut: it stores an offset into the pattern's
content rather than restarting from the top.

### Mute, solo and what actually sounds

Three independent switches decide whether a block is heard:

- The **block's own mute** (right-click a clip, or the Mute tool, or Right-Alt + click).
- The **row's mute LED** in the track header.
- The **row's solo LED** - solo anywhere silences every row that is not soloed.

Muting a block does not shorten the song. A single muted two-bar block still makes a two-bar
silent song, because the end of the song is the furthest block end regardless of mute.

### Where the song ends

The song runs to the furthest end of any block of any type, using each block's exact length
rather than a rounded bar count. In Song mode with looping on, playback wraps there; with
looping off, the transport stops there. Offline export uses the same number, so what you
export ends where playback ended.

If you have a time selection on the Builder ruler, that overrides the whole thing: playback
loops between its ends instead.

### Time signatures

A pattern's signature can come from two places, and the pattern button's menu tells you
which:

- **User-set** - you typed it into "Set Time Signature...". It stays what you set, and when
  you place a block of that pattern on the timeline, a matching signature flag is
  automatically added to the Builder ruler at that bar (unless there is already one there).
- **Following** - the pattern adopts a signature instead of owning one. A placed pattern
  follows the ruler flag in force at its earliest block; an unplaced pattern follows the flag
  it was bound to when it was created, or the project's current-signature selection, or 4/4.

"Reset to Default" clears the user-set lock and drops the pattern back into following.

Signature flags are added from the Builder ruler's right-click menu. They govern playback, the
position readout and the pattern signatures above; they do **not** re-space the Builder's own
bar lines, which stay a uniform four beats wide.

### Split by Player Engine

Right-click a pattern in the browser and choose **Split by Player Engine...**. Any pattern
that has several instruments playing in it becomes several patterns, one per instrument, each
named "<original> - <tab name>". The original is destroyed, and every block that referenced it
is replaced in place by its siblings stacked on the rows directly below. It is one undo step.

Use it when a single pattern has grown into a whole arrangement and you want to move the drums
and the bass around independently.

### The three clip types on one grid

| Type | What it is | Length | What is stored |
|---|---|---|---|
| **Pattern** | A window onto one pattern's notes | Auto-set from the pattern's content; extending it adds silence | Which pattern, and how far into it to start |
| **Audio** | One audio file | The file's own length in beats at its detected tempo; freely resized | The file path, pitch shift, original BPM, stretch mode, route, and how far into the file to start |
| **Automation** | One control moving over time | Auto-set to the ruler time selection, or to the current song length | The target control and its curve points |

---

## Parameters and persistence

Patterns and the arrangement register no automatable parameters of their own. Everything is
saved with the **project**, inside `<PatternManager>`:

| Element | Contents |
|---|---|
| Root properties | `version`, `currentPattern`, `globalTempo` |
| `<Patterns>` | One `<Pattern name bars stepsPerBar color tsNum tsDen tsLocked tsBoundUid>` each, with a `<Rolls>` child holding one tagged roll per instrument page |
| `<Arrangement>` | One `<Block>` per clip: `trackRow`, `patternIndex`, `startTicks`, `lengthBars` + `lengthTicks`, `clipType`, `audioFilePath`, `displayAlias`, `pitchSemitones`, `originalBPM`, `stretchMode`, `muted`, `routeChannel`, `alignBake`, `isOverride`, `contentStartSamples`, `contentOffsetTicks`, plus an `<AutomationLane>` child on automation clips |
| `<RowState>` | Row mute and solo as 500-character strings, plus a sparse child per row that has been renamed or grouped |
| `<TimeMarkers>` | Named ruler flags |
| `<TimeSigChanges currentUid nextUid>` | Signature flags, each with a stable id and the pattern that spawned it if it was auto-added |
| `<TempoChanges>` | Bar-and-BPM pairs |
| `<AudioLibrary>` | One entry per audio file, the source of truth for that file's pitch, BPM, stretch mode, choke group and owning page |
| `<AudioGroups>` | User-created browser groups |
| `<AutomationTemplates>` | Reusable automation lanes |

`clipType` and the automation `curve` type are stored as raw numbers. Their values are pinned
and may only be appended to, because reordering them would silently retype every saved block
or point.

Two deliberate asymmetries on load: a block's `isOverride` defaults to **true** when the
attribute is absent, so older projects keep their own saved pitch and tempo instead of
suddenly following a library entry; and `routeChannel` defaults to 0, meaning "not routed
through a Vox or Inst page".

`Pattern.bars` is written and read but is **not** what determines pattern length - length is
computed from the notes, as described above.

**Not saved**: which browser tab was open, the arrangement clipboard, and the pattern
"brush" memory.

---

## Lifetime and teardown

`PatternManager` is owned by the processor and lives for the whole session; a project load
clears and refills it rather than replacing it.

- Loading a project runs the whole read inside a `ScopedAudioShield`, because both the pattern
  list and the arrangement are emptied and rebuilt entry by entry while the audio thread would
  otherwise be walking them.
- `File > New` runs a separately shielded reset that clears patterns, arrangement, markers,
  signature and tempo lists, the audio library, browser groups, mixer snapshot, row state and
  automation templates, and leaves one empty default pattern.
- Neither path marks the project dirty, and both republish the roll snapshots directly rather
  than going through the normal content-changed notification.
- There is always at least one pattern. Delete is refused when only one remains.
- Roll snapshots retire through a deferred-destruction queue with its own drain thread, so
  freeing a replaced snapshot never lands on the audio thread.

---

## Cross-references

- `Piano Roll.md` - writing the notes that make up a pattern.
- `Builder Page.md` - the timeline the blocks sit on, the ruler flags, and the row controls.
- `Event Editor.md` - editing an automation clip's curve.

---

## Differs from Carry-Forward

Carry-Forward has no pattern or arrangement section of its own; the only overlap is its
Builder grid file index, and the deltas there are recorded in `Builder Page.md`.
