# Engine Tabs (Layers, Bass, Drums)

**Purpose** - These are the three families of instrument tab you create yourself and play with the piano roll. A Layers tab is a general melodic instrument, a Bass tab is a bass instrument, and a Drums tab is one single drum sound. Each tab owns exactly one sound-making engine, one mixer channel, and one piano roll, so "a tab" and "a track" are the same thing here. Drums are special in one way: the drum tabs are gathered into two independent kits of sixteen, and each kit has its own drums bus in the mixer.

---

## How it operates

**The engine lives in the model, not in the window.** `Source/EngineRig.h/.cpp` owns every dynamic tab's engine, keyed by `(TabKind, pageIndex)`. `TabKind::Layers`, `TabKind::Bass` and `TabKind::Drums` are three of the eight kinds. The page component (`Source/Standalone/LayersPage.cpp`, `BassPage.cpp`, `DrumPage.cpp`) is a **view**: it holds a non-owning `juce::AudioProcessor*` into the rig plus the engine editor it built, and asks the rig for engines through `EngineRig::setEngineType`. Closing a window does not stop the sound.

Per-kind capacity comes from `Source/BaySickConstants.h` and is the single statement of each cap:

| Family | Constant | Value |
|---|---|---|
| Layers | `kMaxLayerPages` | 20 |
| Bass | `kMaxBassPages` | 10 |
| Drums | `kMaxDrumPages` | 32 |

(Header comments inside `LayersPage.h` and `BassPage.h` still say "8" and "4"; those comments are stale. The constants above are what the code enforces.)

**Engine construction.** `EngineRig::createEngineFor` builds the engine by name string:

| Name string | Class |
|---|---|
| `"Harmless"` | `HarmlessProcessor` |
| `"BaySickPlayer"` | `BaySickPlayerProcessor` |
| `"BaySickSynth"` | `BaySickSynthProcessor` |
| `"BaySickBass"` | `BaySickBassProcessor` |

Every one gets its own `AudioProcessorValueTreeState` whose parameter ids are prefixed per tab: the rig hands out a track id (`lay_<n>` / `bas_<n>` / `drm_<n>` from `EngineRig::trackIdFor`) and each engine builds `tk_<trackId>_<tag>_` from it, where the tag is `harm_`, `bsp_`, `bss_` or `bsb_`. So Layer 3's Harmless cutoff lives at `tk_lay_2_harm_...`. Each engine's APVTS also carries an `undoOwnerTag` of `rig:<kind>:<pageIndex>` so undo entries can find the engine again after it has been destroyed and rebuilt.

**Audio path.** The registered engine is fed by `EngineInsertTask` and lands on its own mixer insert channel: `MixerChannelIds::layerInsert(i)` (200+i), `bassInsert(i)` (300+i), `drumInsert(i)` (500+i). The APVTS prefix for that strip is `mixer_layer_<i>` / `mixer_bass_<i>` / `mixer_drum_<i>`. Layer inserts default to the Layers bus, bass inserts to the Bass bus, and drum inserts to the drums bus their **kit** implies (see below).

**Threading.** All rig mutation, page building, menus and preset IO run on the message thread. The audio thread reads the processor's spinlock-guarded pointer arrays that the rig keeps in sync. Teardown order is fixed in `EngineRig::removeTab`: retract frozen sources, unregister from audio dispatch, settle one audio block, then destroy.

**The two drum kits.** `MixerChannelIds::kDrumPagesPerBank = 16` (in `Source/BaySickGraph.h`) splits the 32 drum pages into two kits. `drumBankForPage(pageIdx)` answers 0 for pages 0-15 and 1 for pages 16-31; `drumBusForPage(pageIdx)` maps those to `kDrumsBus` (id 3) and `kDrumsBus2` (id 18). **The slot number is the only record of which kit a drum belongs to** - nothing stores a bank field, and a drum never moves between kits. Which kit is on screen is editor state (`StandaloneEditor::mActiveDrumBank`), because add / save kit / load kit all read it and it has to survive a page rebuild.

---

## User-facing behavior

### Making a tab

Every tab is born from the **"+" slot at the right end of the ribbon** (the tab strip across the top). Click it and you get a menu of instrument names, not page names - **the instrument you pick decides which kind of tab you get.**

| You pick | You get |
|---|---|
| **Harmless > Layers** | a Layers tab running Harmless |
| **Harmless > Bass** | a Bass tab running Harmless |
| **BaySickSynth** | a Layers tab running BaySickSynth |
| **BaySickPlayer > Layers** | a Layers tab running BaySickPlayer |
| **BaySickPlayer > Bass** | a Bass tab running BaySickPlayer |
| **BaySickBass** | a Bass tab running BaySickBass |
| **BaySickDrums > BaySickPlayer** | a Drums tab whose sound is a sample player |
| **BaySickDrums > BaySickSynth** | a Drums tab whose sound is synthesized |

(The same "+" menu also creates Clips, Vox, Inst and Plugins tabs - those have their own documents.)

Once a family has at least one tab, its own colored slot appears in the ribbon (Layers orange, Bass green, Drums red) and its dropdown arrow carries the same add rows scoped to that family: "+ Add Harmless", "+ Add BaySickPlayer", "+ Add BaySickSynth" on Layers; "+ Add Harmless", "+ Add BaySickPlayer", "+ Add BaySickBass" on Bass; "+ Add BaySickPlayer", "+ Add BaySickSynth" on Drums. **A family's slot vanishes from the ribbon when its last tab is deleted** and comes back through "+".

New tabs are named `Layer 1`, `Bass 1`, `Drum 1` and count upward. The counters never reuse a number you deleted, so deleting Layer 3 and adding another gives you Layer 4.

### Getting around the ribbon

Each family slot is two rows. The top row is the name of the tab you are currently on in that family. The bottom row carries a small round **badge with the number of tabs** in that family, a **cyan dot** if any of them is frozen (solid = playing its rendered file, hollow ring = frozen but out of date and playing live while it re-renders), and a **small down-arrow on the right**.

- **Click the slot body** - jump to the tab in that family you were last on.
- **Click the down-arrow** - open the family dropdown. It lists every tab in the family with a tick on the current one, then a **Pages:** section listing the windows that belong to the current tab, then **Rename...** and **Delete**, then the add rows.

The "Pages:" rows for these three families are:

| Family | Rows |
|---|---|
| Layers | Player, Piano Roll, Pre EQ, Post EQ |
| Bass | Player, Piano Roll, Pre EQ, Post EQ |
| Drums | Drum Kit, Player, Piano Roll, Pre EQ, Post EQ |

Picking a row jumps you to that surface. "Piano Roll" and "Drum Kit" take you to the Piano Roll tab with the right engine already selected.

### The tab's own window

Each tab opens as its own window inside the main frame. Along the top of that window sits a strip with a **Menu** button (the page menu), the engine's name in the middle, the engine's own preset button, an **FX Rack** shortcut, and a **Swing Mix** knob.

**Swing Mix** (0.00 to 1.00, default 1.00) is how much of the project's Global Swing this one player follows. At 1.00 the player swings fully with the global setting; at 0.00 it stays perfectly straight while everything else swings. Right-click the knob for **Truncate Swing Notes** (off by default), which shortens notes rather than letting them run into the swung beat. Drums, Layers and Bass each have one per tab (`swing_layer_<n>_mix`, `swing_bass_<n>_mix`, `swing_drum_<n>_mix`, plus the matching `_trunc`).

### The Menu (page actions)

Open it from the window's **Menu** button. The top of the menu is the window/view list described above, then:

| Item | What it does |
|---|---|
| **Lock Layer / Lock Bass / Lock Drum** | Tick-toggle. A locked tab cannot be deleted (the Delete item grays out and the ribbon refuses with a "Cannot Delete" box), and a locked drum cannot have its sound swapped from the kit. The ribbon shows `[L] ` in front of the tab name. |
| **Polyphony: Polyphonic / Monophonic** | One click switches the engine between playing chords and playing one note at a time. Reads "(n/a)" and grays out on Harmless, which is always polyphonic. |
| **Rename...** | Opens a text box; typing a new name and clicking OK renames the ribbon tab. An empty name is ignored. |
| **Duplicate Layer / Bass / Drum (new tab)** | Makes a second tab with the same engine and the same settings. Grayed out until the tab has an engine. A duplicated drum lands in the **same kit** as the drum it came from, not the kit you happen to be looking at. |
| **Choke Group** | None (default) or Group 1-16. Two tabs in the same group cut each other off - the classic open-hat/closed-hat trick, and it works across engine types. |
| **Save Current Patch As...** | Saves just the engine's sound (not the effects or the fader) under a name you type. Grayed out until there is an engine to save. |
| **Load Preset** | **Layers and Bass only.** Cascading folder menu of factory and user patches for the engine this tab is running. Loading one also renames the tab to the preset's name. Drums do not have this row - a drum's sound is chosen from the kit pad's sound picker instead (below). |
| **MIDI Note** / **MIDI Learn** | **Drums only, and only when the menu was opened from a kit pad.** MIDI Note picks the note this drum sounds at, in octave submenus, with the current assignment shown at the top ("Assigned: C5"); default C5 (60). Changing it re-pitches this drum's existing hits that sat on the old note, and leaves hits you deliberately placed elsewhere alone. MIDI Learn binds a pad on a hardware controller to this drum - a box appears saying "Hit a pad or key to assign it to this drum. Waiting 30 seconds..." with a Cancel button, and it gives up on its own after 30 seconds. The item's label shows the current binding, and a **MIDI Forget** row appears once there is one. |
| **Save Page Preset As...** | Saves the **whole channel**: engine sound + mixer strip settings + the insert effects rack + both EQs. |
| **Load Page Preset** | Loads one of those back. |
| **FX Rack** | Jumps to this tab's effects rack. |
| **Freeze / Frozen** | Renders this player to a file so its engine stops costing CPU. Its effects, EQ and fader stay live and editable. Click again to unfreeze. |
| **Delete Layer / Bass / Drum** | See below. |

**Deleting.** If the sound has been changed since it was loaded, you get three buttons: **Save Page Preset & Delete** (writes the whole channel to disk first), **Delete**, **Cancel**. If nothing has been changed you get a simpler **Delete / Cancel**. The warning text tells you plainly that deleting the tab removes its player, mixer strip, effects rack and piano roll.

### Drums: the Drum Kit view and the two kits

Drum tabs also show up together on the **Drum Kit** grid (reached from the Piano Roll tab, or from any Drums tab's "Drum Kit" page row). The grid is 16 rows. Down the left side each row shows:

- a **drag handle** (three small bars) - drag a row up or down to reorder your drums,
- a **picker button** showing the drum's sound name (with `[L] ` in front when that drum is locked), or `Pick a sound  v` when the slot is empty,
- **M** and **S** LEDs - mute (red) and solo (yellow) for that drum's mixer strip, bound to `mixer_drum_<n>_mute` and `_solo`; grayed out until the slot has a sound,
- a **white key** at the right of the row - press and hold to audition the drum.

The drum you are currently editing gets a colored border around its picker.

**What a picker click does depends on whether the slot already has a sound.**

An **empty** slot opens the **sound menu**:

- **Sample >** - "Browse sample folder...", "Load SFZ file...", then your Core Library drum packs and the factory sample-based drum patches. Picking any of these puts a sample player in the slot.
- **Synth Patch >** - "+ New Patch (Blank)", the installed synth drum patches, and "Save Current Patch As...". Picking one puts the drum synth in the slot.
- **None (clear)** - only present once the slot has something in it.

You never choose an "engine" here; picking a sample or a patch chooses it for you.

A slot that **already has a sound** opens that drum's own menu instead - Lock Drum, Polyphony, Rename..., Duplicate, Choke Group, MIDI Note, MIDI Learn, Save Current Patch As..., Delete Drum. This is the deliberate pick-once behavior: the picker becomes the drum's control once it is loaded, so a kit cannot be re-pointed by a stray click. The consequence to know is that **the sound menu (and its "None (clear)" row) is not reachable from the picker again** once a sound is loaded; to change that drum you load a different patch from the player's own preset button on the Drums tab window, load a Page Preset, or delete the drum and add a new one.

Above the pickers sits **Lock/Unlock 1-16** (or **Lock/Unlock 17-32**). Clicking it asks "This will lock or unlock every drum in kit 1-16. The other kit is not affected." and then flips them all. If any drum in that kit is unlocked, the click locks the whole kit; if they are all locked, it unlocks them. **The two kits lock independently.** There is a "Don't show again" tick on the prompt, remembered per kit.

Top right of the grid are three buttons: **1-16**, **17-32** and **Kit  v**.

- **1-16 / 17-32** switch which kit the grid is showing. These are **two separate kits**, not two views of one - kit 1 is drums 1 to 16 feeding the Drums Bus, kit 2 is drums 17 to 32 feeding Drums Bus 2. Adding a drum lands in the kit you are looking at; if that kit is full you get a "Drum Kit Full" box telling you to delete one or switch to the other kit, and nothing spills over.
- **Kit  v** opens Save / Load Kit.
  - **Save Kit** asks for a name and saves *only the sixteen drums of the kit you are on*, sounds only - no EQ, no effects rack, no fader (those live on Page Presets). Saving an empty kit is refused with "Nothing to save".
  - **Load Kit** replaces *only the kit you are on*. It warns first: "This will replace the drums in 1-16 with the new kit's drums, do you wish to proceed? (Kit 17-32 is not affected.)", plus a line saying BaySickRustyDrums is untouched if a Rusty tab exists. A kit file is written with slots numbered 0-15 regardless of which half it was saved from, so **any kit file loads into either half**, and every factory kit works in kit 2.

Clicking an empty row past the end of the list adds a new drum tab and opens its sound picker straight away.

The kit grid itself is a drum sequencer with the same tools, snapping, zoom, undo and control lane (velocity and pan) as the piano roll.

### Where a tab shows up elsewhere

- **Mixer** - a strip appears the moment the tab gets its engine (not when the tab is created). It is named after the tab and follows the tab's renames. Layer strips default to the Layers bus, Bass strips to the Bass bus, drum strips to the drums bus for their kit.
- **Piano Roll** - the tab appears in the Piano Roll page's engine dropdown under its tab name. Selecting it also makes it the live MIDI target, so a connected keyboard plays it.
- **Effects page** - the tab's strip appears in the channel dropdown, grouped under whichever bus it currently feeds.

---

## Parameters and persistence

**Engine parameters** live in the engine's own APVTS, not the main one: `tk_lay_<n>_harm_*`, `tk_lay_<n>_bsp_*`, `tk_lay_<n>_bss_*`, `tk_bas_<n>_bsb_*`, `tk_drm_<n>_bsp_*`, and so on.

**Channel parameters** live in the main APVTS under the strip prefix:

| Parameter | Range / default | Notes |
|---|---|---|
| `mixer_<kind>_<n>_sendTo` | 0-999, default = the natural parent bus | Where the strip's output goes |
| `mixer_<kind>_<n>_send0..3_to` / `_amount` / `_prepost` | -1..999 / -60..+6 dB / bool | Extra sends |
| `mixer_<kind>_<n>_sc_recv0..3_from` | -1..999, default -1 | Sidechain receive lines |
| `mixer_<kind>_<n>_chokeGroup` | 0-16, default 0 | 0 = none |
| `mixer_drum_<n>_playNote` | 0-127, default 60 | Drums only: the note the drum sounds at |
| `mixer_<kind>_<n>_preeq_*` / `_mid_eq*` / `_side_eq*` | - | Pre-rack and strip EQ banks |
| `swing_layer_<n>_mix` / `swing_bass_<n>_mix` / `swing_drum_<n>_mix` | 0.0-1.0, default 1.0 | Per-player swing follow |
| `swing_*_trunc` | bool, default false | Truncate swung notes |

**Saved with the project** (`<UIState><Tabs>`, written by `StandaloneEditor::serializeTabsInto`): one `<Tab>` per page carrying `type` (`Layers` / `Bass` / `Drum`), `pageIndex`, `name`, `engine` (the engine name string), `engineData` (base64 of the engine's full state), `locked`, and - for a frozen tab - `frozen` plus `frozenBy` (`manual` or `auto`). Piano-roll notes are project data held by `PatternManager`, not by the tab record. `drumKitBank` (which kit is on screen) is saved in the same `<UIState>`.

**Saved with a page preset** (`Documents/BaySickDAW/Presets/<Layer|Bass|Drum> Page/My Presets/*.xml`, root tag `<BaySickPagePreset>`, version 2): the engine's state, every mixer-strip parameter under the tab's prefix, the insert effects rack, and both EQs. **Piano-roll notes are deliberately excluded** - a page preset is a sound, not a part.

**Saved with a patch preset** (`Save Current Patch As...`): the engine only, as a `BaySickEnginePreset` wrapper carrying base64 engine state. Sample-based patches also carry a `<Sample kind path>` reference.

**Saved with a kit file** (`Documents/BaySickDAW/Kits/My Kits/*.xml`, root `<BaySickKit>` version 1): one `<Drum slot="0..15">` per non-empty slot of one bank, each holding that drum's `exportDrumState()` blob (engine type + engine state + sound name + lock flag). Slots are normalized to 0-15. **No EQ, rack or strip settings.**

**Also saved with the project, outside the tab records:** the per-drum hardware trigger bindings from MIDI Learn, written into the processor's state as a `<DrumTriggers>` child. They are part of the kit setup, like each drum's play note.

**Not saved at all:** the ribbon's `kitMissing` display marker (Inst tabs only, see the Inst document) and `EngineTab::userUnfroze` (an explicit unfreeze keeps auto-freeze off that tab for the session only).

**Per-machine, not per-project:** the "Don't show again" answers for the kit-replace prompt and the per-kit lock prompt live on `ProjectManager` preferences.

---

## Lifetime and teardown

A page is created by `StandaloneEditor::onAddTabRequest` (or the restore / duplicate / kit-load equivalents), which builds the page component, adds the ribbon tab, wires the callbacks, and hosts the page in a `WorkspaceWindow`. The **engine** is created later, on first pick, by the page calling `EngineRig::setEngineType` - which is also when the mixer strip appears.

The rig owns the engine; the page owns only the editor. **Page destruction on window close is off**, so a page outlives every window it opens and dies only with the tab.

Delete order in `StandaloneEditor::onTabClosed` matters: the page's index slot is freed and its piano-roll registration dropped *before* the page is destroyed (the roll connection's closures capture the page pointer), the mixer strip is removed, then `EngineRig::removeTab` runs its own ordered teardown - retract frozen sources, unregister from audio dispatch, settle, destroy. Freeze files are deleted only when a **user** deletes the tab; closing a project or quitting keeps them as reusable cache.

A kit load tears down only the target bank's drum tabs, by tab id rather than by type, so the other kit's tabs and any BaySickRustyDrums tab survive.

---

## Cross-references

- **Clips Page.md** - the audio-clip tab family, same ribbon and page-preset shape.
- **Inst Page.md** - live-input and sfizz guitar/bass tabs.
- **Plugins Page.md** - hosted third-party instrument tabs.
- **BaySickRustyDrums.md** - the separate single-instance drum engine that also appears under the Drums ribbon slot. A kit load never touches it.
- The engines these tabs can run: **Harmless.md**, **BaySickSynth.md**, **BaySickBass.md**, **BaySickPlayer.md**.
- **Mixer.md**, **Effect Racks.md**, **Piano Roll.md**, **Builder Page.md**, **Freeze and Export.md** - the systems every tab plugs into.

---

## Differs from Carry-Forward

- **Page caps.** Carry-Forward predates the raise; the shipping caps are Layers 20, Bass 10, Drums 32 (Clips 100, Vox 10, Inst 30, Plugins 20).
- **Engine ownership.** Carry-Forward describes pages that own their engines. Engines are now model-owned by `EngineRig`, keyed `(TabKind, pageIndex)`; pages are non-owning views and engines keep playing with every window closed.
- **Two drum kits.** Carry-Forward has one Drums Bus. Drum pages 16-31 now form a second, independent kit on its own `kDrumsBus2`, with its own add window, its own kit file and its own lock toggle.
- **No minimum tab count.** Carry-Forward's era guaranteed at least one Layers / Bass / Drums tab. Every family can now reach zero; its ribbon slot disappears and returns through "+".
- **Engine picker location.** The per-page engine picker combo described in Carry-Forward is gone. The engine is chosen in the "+" menu (or, for drums, implied by the sound you pick) before the page exists.
