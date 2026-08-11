# Inst Page

**Purpose** - An Inst tab is a guitar / bass rig: a pedalboard and an amp-and-cabinet simulator in series. What feeds that rig is the tab's **source**, and there are three - a live instrument plugged into your audio interface, a sampled electric guitar, or a sampled electric bass. All three share one tab type, one mixer strip family and one preset folder, so a plugged-in guitar and a sampled one are set up and mixed exactly the same way.

---

## How it operates

**The chain is the engine.** `Source/Inst/InstPage.h/.cpp` is the view. The processor registered for audio is an `EngineChainProcessor` wrapper owned by `EngineRig` under `TabKind::Inst`; its `processBlock` fans the buffer through the stages in order. The pedalboard (`BaySickPedalsProcessor`) and the amp/cab unit (`BaySickNAMIRProcessor`) are **rig-owned support stages** held in the tab's `ownedStages`, with raw convenience pointers on the tab (`tab.pedals`, `tab.namIr`). The page holds non-owning views of all three.

`InstPage::rebuildEngineChain` sets the stage list from the source:

| Source | Chain |
|---|---|
| `LiveInput` | Pedals -> NAM/IR (buffer filled by the armed audio input) |
| `BaySickGuitars` | Guitars -> Pedals -> NAM/IR |
| `BaySickBasses` | Basses -> Pedals -> NAM/IR |

The sfizz front-ends are **not** rig-owned - `BaySickGuitars` and `BaySickBasses` stay processor-owned on their own race-safe kit-load paths, and the chain queries the pointer from `BaySickDAWProcessor` every time it rebuilds. If the kit has not loaded yet the chain degrades to Pedals -> NAM/IR and the tab is silent until a follow-up rebuild splices the engine in.

**Teardown order is load-bearing.** `EngineRig::removeTab` must run before `destroyBaySickGuitars` / `destroyBaySickBasses`, because the rig-owned chain holds the spliced sfizz pointer; one audio block in the wrong order is a use-after-free.

**Identity and routing.** Cap is `kMaxInstPages = 30` and it is **shared** - live-input tabs, Guitars tabs and Basses tabs all consume Inst slots.

| Thing | Value |
|---|---|
| Mixer channel id | `MixerChannelIds::instInsert(i)` = 700 + i |
| Strip APVTS prefix | `mixer_inst_<i>` |
| Default output | Inst Bus (`kInstBus`, id 8) |
| Page accent | navy `0xff1c3a8a` (matches the Inst bus strip) |
| Piano roll | `EngineKind::BaySickGuitars` / `BaySickBasses`, index = page index. A live-input tab has **no** roll entry. |

**Spawned by its mixer strip.** Every entry point - the Mixer's Add menu, the ribbon "+", a project load - goes through `MixerPage::addInstChannelAtIndex`, which synchronously fires `onInstStripAdded` and that spawns the page. The two sfizz add paths then load a default kit and flip the source.

**Threading.** Page construction, source flips, kit loads, the program picker and preset IO are all message thread. Kit loads run under a busy overlay because sfizz parsing can take seconds. The audio thread reads the registered chain wrapper.

---

## User-facing behavior

### Making an Inst tab

From the ribbon **"+"** slot:

| You pick | You get |
|---|---|
| **BaySickLiveInst** | a live-input tab: whatever you plug into your interface, through pedals and an amp |
| **BaySickGuitars** | a sampled electric guitar, already loaded with a default program, through the same pedals and amp |
| **BaySickBasses** | a sampled electric bass, same deal |

Or from the **Inst ribbon slot's dropdown**: "+ Add BaySickLiveInst", "+ Add BaySickGuitars", "+ Add BaySickBasses". All three gray out together once you have 30 Inst tabs of any kind.

New tabs are named `LiveInst 1`, `Guitar 1`, `Basses 1` by family, counting upward and never reusing a number you deleted. A sfizz tab is then immediately renamed to the loaded program's friendly name (a file called `01-green_keyswitch.sfz` becomes `Green Keyswitch`), so in practice a fresh guitar tab is named after its sound.

### Live-input tabs

A live-input tab has **no page window of its own** - the **Pedals** window is its player. Clicking the tab opens or fronts that window.

Its mixer strip carries the two live-input LEDs:

- **Arm** - record-enables the strip, and its tooltip names the audio-interface input channel it is listening to (or "no input" when nothing is assigned).
- **Listen** - monitor through the chain.

Recording an armed Inst strip produces one dry WAV named `<project> - Inst N - <timestamp> - DRY.wav`, which is added to the audio library and dropped onto the arrangement. (Inst has no wet capture; there is no realtime stage to bake in.)

### Sampled guitar / bass tabs

These get a real page window showing the sampled-instrument player. The window's top strip shows `BaySickGuitars` or `BaySickBasses` in navy, a **program name label**, and a **Load Guitar** / **Load Bass** button. On the player itself, top-left, sit two toggles:

- **CUT SELF** (off by default) - when on, a new note stops what is already ringing, the way a real string does when you replay it.
- **SAME PITCH / CUT ALL** - how much a new note stops. **SAME PITCH** stops only a note already sounding at that same pitch; **CUT ALL** stops everything that is ringing. Keyswitch notes (the articulation-selecting keys built into the kit) are exempt from both - they are mode presses, not notes.

**Load Guitar / Load Bass** opens a list of every program in the loaded kit's `Programs` folder, with a tick on the current one and names cleaned up for reading (leading track numbers stripped, underscores turned into spaces, words capitalized). Picking one swaps the program. **Each program remembers its own tweaks for the session**, so switching away and back restores what you had set; that cache is saved with the project, so it survives a reload too. A program change is one undo step. If the pick cannot be loaded - a damaged SFZ, or one the kit safety gate refuses - a warning box titled "Load Program" reads "Could not load program:" and the full path, and the tab keeps playing what it was playing. The box gives no reason; a refusal reason, if there is one, is banked in the missing-files store and only surfaces later inside some unrelated **Missing files** dialog.

Their mixer strip has **no** Arm / Listen LEDs - the sampled instrument is the source, so there is nothing to arm.

These tabs appear in the Piano Roll's engine dropdown under their tab name and can be played from a connected keyboard when selected there.

### The Menu (page actions)

Reached from the window's **Menu** button (on a live-input tab, from the Pedals window's Menu). The top of the menu is the window list, then the actions.

Window list for a **live-input** tab: **Pedals**, **NAM/IR**, **Pre EQ**, **Post EQ**.
Window list for a **sampled** tab: **Player**, **Pedals**, **NAM/IR**, **Piano Roll**, **Pre EQ**, **Post EQ**.

| Action | What it does |
|---|---|
| **Lock** | Tick-toggle. A locked tab cannot be deleted; the ribbon shows `[L] ` in front of the name. One undo step. |
| **Rename...** | Text box; renames the ribbon tab. |
| **Duplicate Inst (new tab)** | Makes a second Inst tab with the same source, the same pedals, the same amp and the same program. |
| **Save Page Preset As...** | Saves the whole rig - pedals, amp/cab, the sampled program if there is one, the mixer strip, the effects rack and both EQs. Enabled as soon as the tab has anything in its chain. |
| **Load Page Preset** | Always enabled, even on a fresh tab, so you can drop a saved rig onto a blank one. Every Inst preset lives in one folder no matter which source it was saved from; the preset carries its own source and switches the tab to it before applying anything. |
| **FX Rack** | Jumps to this tab's effects rack. |
| **Freeze / Frozen** | Renders the tab to a file so its engine stops costing CPU; effects, EQ and fader stay live. |
| **Delete Inst** | Grayed out while locked. |

**Deleting** warns: "Deleting this inst tab removes its BaySickPedals, BaySickNAM/IR, Mixer Strip, Effects Rack, and the audio library entries for every recording made on this tab. The audio files in your project's Samples folder stay on disk." If the chain has been changed you get **Save Page Preset & Delete / Delete / Cancel**; otherwise **Delete / Cancel**.

### The Pedals window

Opened from the Menu's **Pedals** row. It has its own **View** menu with two layouts:

- **Standard** - the full pedalboard.
- **Compact** - one pedal at a time in a smaller, effects-sized window.

The choice belongs to the tab, not to the window, so it survives closing the window and is saved with the project. On a live-input tab this window is titled with the tab's own name (it is the player); on a sampled tab it is titled `<tab name> - Pedals`.

### The NAM/IR window

Opened from the Menu's **NAM/IR** row. It hosts the amp-capture and cabinet simulator for this tab. Note that Vox tabs have their **own** separate amp/cab unit built into the vocal engine - changing this one does not affect that one.

### Swing

Sampled Inst tabs carry a **Swing Mix** knob on their window strip (0.00-1.00, default 1.00) with a right-click **Truncate Swing Notes** toggle, exactly like the Layers / Bass / Drums tabs. Parameters: `swing_inst_<n>_mix` and `swing_inst_<n>_trunc`.

### "(missing)" on an Inst tab

If a project is opened on a machine where the saved guitar or bass kit is not installed, the app substitutes a default kit and marks the tab **" (missing)"** in the ribbon and on the mixer strip. That marker means: *the tab is labeled one instrument but is playing another.* It is display only - it is never written into the tab's saved name, and it disappears by itself once the kit is installed and the project reopened.

---

## Parameters and persistence

**Pedals and amp parameters** live in their own APVTS instances, one pair per tab, tagged `rig:<kind>:<pageIndex>.pedals` and `.namir` for undo resolution. **Sampled-instrument parameters** live in the sfizz engine's own APVTS with a per-page prefix (`bgg_<n>_` for Guitars; the Basses engine uses its own equivalent). `EngineRig::trackIdFor` hands out **no** prefix for Inst - the stages own their own vocabularies.

**Channel parameters** live in the main APVTS under `mixer_inst_<n>`: `_sendTo` (default Inst Bus), `_send0..3_to`/`_amount`/`_prepost`, `_sc_recv0..3_from`, `_chokeGroup` (0-16), `_preeq_*` and the strip EQ banks. Live-input tabs additionally use the strip's arm parameter; that is what the recorder scans.

**Saved with the project** - one `<Tab type="Inst">` record carrying:

| Attribute | Meaning |
|---|---|
| `pageIndex`, `name`, `locked` | Tab identity |
| `engine`, `engineData` | Legacy back-compat fields; the chain wrapper's own state is empty |
| `instChainState` | The real payload: the pedalboard's and the amp/cab's serialized state |
| `source` | `BaySickGuitars` / `BaySickBasses` (absent for live input) |
| `kitPath` | Stable reference to the loaded kit |
| `sfizzEngineData` | The sampled engine's APVTS plus its current kit path |
| `<ProgramStateCache>` child | Every program's tweaked values, so switching back restores them |
| `frozen`, `frozenBy` | Freeze state, project saves only (a template skips it) |

The pedals **Standard / Compact** view mode is saved too, read from the page rather than from a window so it works with no window open.

**Saved with a page preset** (`Documents/BaySickDAW/Presets/Inst Page/My Presets/*.xml`, root `<BaySickPagePreset>` version 2): a `sourceMode` label so the loader can switch source before applying anything, then one record per engine slot - `Pedals`, `NamIr` and (when present) `Sfizz` - plus every `mixer_inst_<n>` parameter, the insert rack and the EQs. The sfizz slot carries a kit path that is loaded through the race-safe kit loader **before** any parameter state is applied; sfizz crashes if a kit is reloaded mid-render any other way.

**Not saved**: the " (missing)" kit marker (deliberately absent from every capture - a persisted marker would outlive reinstalling the kit).

**Not consulted today**: `InstPage::setBusActiveQuery` is registered but read by nothing, because `loadPagePreset` goes through the config-based path which has no such field. Do not build Inst routing behavior on it without threading it through first.

---

## Lifetime and teardown

The page's Pedals + NAM/IR + chain trio is created in the page constructor by way of the rig (which is why the page takes a processor reference at construction). The mixer strip exists first - the strip's creation is what spawns the page.

The rig owns the chain and both stages; the page owns only their editors, which must be destroyed before the page because their control attachments reference the stages' APVTS. Page destruction on window close is off, so a page outlives every window it opens.

On close, `StandaloneEditor::onTabClosed` drops the piano-roll registration, removes the mixer strip, cascades the audio-library cleanup for recordings made on the tab, and then `EngineRig::removeTab` runs the ordered teardown. For a sampled tab the rig teardown must precede destroying the sfizz engine.

The two sfizz add paths run an eight-step sequence that is order-sensitive: create the strip (which spawns a live-input page), find the page, load the default kit **before** flipping the source (so the engine exists when the chain rebuilds), wire the dirty hook, flip the source, hide the strip's arm/listen LEDs, register the piano roll, then rename. Default kits are `Black&Green Guitars/Programs/01-green_keyswitch.sfz` and `Black&Blue Basses/Programs/01-darkblack_keysw.sfz` in the Core Library.

---

## Cross-references

- **Engine Tabs (Layers, Bass, Drums).md** - the note-driven tab families.
- **Clips Page.md**, **Plugins Page.md** - the other "+"-menu tab families.
- **Vox Page.md** - the vocal counterpart. It has its **own** embedded amp/cab unit; it is not this one.
- **Pedalboard.md** and **NAM Amp and Cab.md** - the two stages that make up an Inst tab's rig.
- **BaySickGuitars.md** and **BaySickBasses.md** - the two sampled sources and their kit loaders.
- **Mixer.md**, **Effect Racks.md**.

---

## Differs from Carry-Forward

- **Engine ownership.** Carry-Forward has the page owning its stages. The chain wrapper and both stages are now owned by `EngineRig` under `TabKind::Inst`; the page holds non-owning views. The sfizz front-ends remain processor-owned.
- **Inst cap.** Raised to `kMaxInstPages = 30`, shared across live-input, Guitars and Basses tabs.
- **Sub-tabs.** The page's Pedals / NAM/IR / EQ sub-tabs are gone. Those are contained windows now, launched from the window's Menu dropdown, and a live-input tab has no page window at all - its Pedals window is its player.
- **Pedals view modes.** Standard / Compact is new, lives on the page, and persists with the project.
- **Per-page EQ.** Removed; pre-rack and post-rack EQ are edited from the Effects page or the window's Pre EQ / Post EQ rows.
- **Substituted-kit marker.** The " (missing)" display marker on the tab and strip did not exist in the Carry-Forward snapshot.
