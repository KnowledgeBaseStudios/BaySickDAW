# Clips Page

**Purpose** - A Clips tab is one audio file turned into a playable instrument. Drop a WAV (or MP3, AIFF, FLAC, OGG) into the project and BaySickDAW makes a tab for it with a sample player already loaded, its own mixer channel, its own effects rack and its own piano roll - so you can retrigger the sound, chop it, pitch it and mix it like any other instrument. A Clips tab is born from a file, never from picking an engine.

---

## How it operates

**One tab, one player.** `Source/Clips/ClipsPage.h/.cpp` is the view. The engine is always a `VibePlayerProcessor` (user-facing name: BaySickPlayer), owned by `EngineRig` under `TabKind::Clips` and keyed by the page index. `ClipsPage::selectEngine` is the activation point; passing `EngineType::None` is a no-op, and `EngineType::BaySickPlayer` asks the rig to build the engine, then builds the editor. The page holds a non-owning pointer to the engine, so closing the window leaves the clip playing.

**The page index IS the audio row.** A Clips page's index is its mixer audio-insert row, capped by `kMaxClipPages = 100` (`Source/VibesynthConstants.h`). That gives the page:

| Thing | Value |
|---|---|
| Mixer channel id | `MixerChannelIds::audioInsert(row)` = 400 + row |
| Strip APVTS prefix | `mixer_audio_<row>` |
| Default output | Clips Bus (`kClipsBus`, id 6) |
| Engine APVTS prefix | `tk_clip_<row>__bsp_` (the rig's track id for Clips carries a trailing underscore, so the doubled `__` is expected) |
| Piano roll | `EngineKind::Clip`, index = row |

**Creation cascade.** Every route ends at `StandaloneEditor::createClipStripAndPage(row, path)`, which in strict order registers the audio row channel, registers the audio InsertNode + its APVTS parameters, adds the mixer strip, rebuilds the Effects page channel list, and then calls `spawnClipsTabIfMissing`. The order matters: the strip's parameter attachments silently fail if the insert's parameters do not exist yet.

`spawnClipsTabIfMissing` is idempotent by file path and by row - the same file dropped on several Builder rows makes **one** Clips page, bound to the first drop's row - unless the caller is the Duplicate flow, which passes `allowDuplicate`.

**Path handling.** The engine is always loaded from an absolute path resolved through `VibeSynthProcessor::resolveProjectFile`, while the audio-library entry is tagged with the **stored/relative** path (`Samples/<file>`) so it dedupes against every other library entry. The library entry carries `pageOwnerChannelId = audioInsert(row)`, which is how the Builder browser groups files under the Clips page that owns them.

**Threading.** Page construction, menus, preset IO and file copying are message-thread. The audio thread reads the registered engine through the processor's clip-engine registry.

---

## User-facing behavior

### Making a Clips tab

Three ways, all of which start with an audio file:

1. **Drag an audio file onto the Builder grid.** The clip lands as a block on the arrangement AND gets its own Clips tab.
2. **Ribbon "+" > BaySickPlayer > Audio Clips.** Opens a file picker (defaulting to `Documents/BaySickDAW/My Samples`), then creates the tab and the mixer strip with **no** block on the Builder grid.
3. **Clips ribbon slot > "+ Add BaySickPlayer..."** - the same file picker and the same result.

There is no engine choice to make. Clips is a sample player and nothing else.

**If you have no project open**, the app stops and asks you to name one first ("To save your audio, give this project a name"), creates and saves it, then adds the clip. That is deliberate: audio imported into a project that was never written to disk would be lost on close.

The picked file is **copied into your project's `Samples` folder**, so the project stays self-contained. The tab and the mixer strip are both named after the file (a file called `vocal chop 3.wav` gives you a tab called `vocal chop 3`); if the name comes out empty you get `Clip 1`, `Clip 2` and so on.

If all 100 Clips pages are in use you get "No free Clips page - close one before adding another".

### The Clips ribbon slot

Gold-colored, appears once you have at least one clip, and vanishes when you delete the last one. Its top row shows the current clip's name; its bottom row carries the clip count badge, a frozen dot if any clip is frozen, and the dropdown arrow.

The dropdown lists every clip (tick on the current one), then a **Pages:** section with **Player**, **Piano Roll**, **Pre EQ** and **Post EQ**, then **Rename...**, **Delete**, and **"+ Add BaySickPlayer..."**.

### The Clips window

The window shows the sample player's own editor. Along the window's top strip: the **Menu** button, the player's name and its preset button, and an **FX Rack** shortcut. Clips tabs deliberately have **no Swing Mix knob** - a clip is audio, not a note grid, so there is no swing to bend.

A filename label exists on the page showing the audio file this tab is bound to ("(no clip)" when empty).

### The Menu (page actions)

| Item | What it does |
|---|---|
| **Player** | Shows the sample player (already the view you are on). |
| **Piano Roll** | Jumps to the Piano Roll tab with this clip selected, so you can place retriggers. |
| **Lock** | Tick-toggle. A locked clip cannot be deleted; the ribbon shows `[L] ` in front of the name. |
| **Rename...** | Text box; renames the ribbon tab. |
| **Duplicate Clip (new tab)** | Makes a second Clips tab on the same audio at a new row, with the player settings copied. |
| **Choke Group** | None (default) or Group 1-16. Clips in the same group cut each other off, and the groups are shared with Layers, Bass and Drums. |
| **Save Page Preset As...** | Saves the whole channel - player settings, mixer strip, effects rack, EQs - **and copies the attached audio into `My Samples`** so the preset works in other projects. |
| **Load Page Preset** | Cascading folder menu of saved clip page presets (factory folders and your own). |
| **FX Rack** | Jumps to this clip's effects rack. |
| **Freeze / Frozen** | Renders the clip's player output to a file so its engine stops costing CPU; its effects, EQ and fader stay live. |
| **Delete Clip** | See below. Grayed out while the tab is locked. |

**Deleting.** The warning is explicit: "Deleting this clip removes its Player, Mixer Strip, Effects Rack, Piano Roll, and the audio library entry for the attached file. The audio file in your project's Samples folder stays on disk." If you have changed the player's settings you get three buttons - **Save Page Preset & Delete**, **Delete**, **Cancel** - otherwise just **Delete / Cancel**. Deleting also removes the Builder blocks that were routed to this page (blocks routed elsewhere survive).

### If the audio is missing

Loading a clip whose file cannot be found (moved, renamed, project copied without its Samples folder) reports through the shared missing-file mechanism. During a project load every miss is collected and shown once, as a single list, rather than one box per clip. On a hand gesture - a drop, an add, a preset apply - you get an immediate "Nothing playable could be loaded from: `<path>`".

---

## Parameters and persistence

**Engine parameters** live in the player's own APVTS under `tk_clip_<row>__bsp_*`. The sample reference itself is stamped onto the engine's state tree (`bsp_loadKind`, `bsp_loadPath`, `bsp_loadNormalize`) so a project reload replays the same load.

**Channel parameters** live in the main APVTS under `mixer_audio_<row>`:

| Parameter | Range / default |
|---|---|
| `mixer_audio_<row>_sendTo` | 0-999, default Clips Bus |
| `mixer_audio_<row>_send0..3_to` / `_amount` / `_prepost` | -1..999 / -60..+6 dB / bool |
| `mixer_audio_<row>_sc_recv0..3_from` | -1..999, default -1 |
| `mixer_audio_<row>_chokeGroup` | 0-16, default 0 |
| `mixer_audio_<row>_preeq_*`, `_mid_eq*`, `_side_eq*` | EQ banks |

There is **no** `swing_clip_*` parameter set - Clips is the one player family with no per-tab swing.

**Saved with the project**: one `<Tab type="Clips">` record per page carrying `pageIndex`, `name`, `engine` (the `EngineType` ordinal - `0` None, `1` BaySickPlayer; never reorder), `engineData` (base64 player state), `clipPath` (a stable reference produced by `SampleLibrary::refForPersist`), `locked`, and freeze state (`frozen` + `frozenBy`) when frozen. The audio library entry and any Builder blocks are project data held by `PatternManager`.

**Saved with a page preset** (`Documents/BaySickDAW/Presets/Clip Page/`, `My Presets/` for yours): player state, every `mixer_audio_<row>` parameter, the insert rack and the EQs, plus a copy of the attached audio placed in `Documents/BaySickDAW/My Samples` and referenced by name. Collisions auto-suffix rather than overwrite, and the copy only happens once the preset write actually succeeds.

**Saved with a clip preset** (`ClipsPage::savePatchAs`, same Clip Page folder): the `ClipPageState` XML, player only.

**Per-machine**: nothing specific to Clips.

**Not saved**: the resolved absolute path (recomputed on load from the stored reference) and the dirty flag behind the delete prompt.

---

## Lifetime and teardown

A Clips page is constructed by `spawnClipsTabIfMissing` after its mixer strip and audio insert already exist. Its callbacks (duplicate, rename, delete, lock, choke-group read/write) are wired **before** `selectEngine(BaySickPlayer)` is called, so the engine-created hook fires with everything in place.

The rig owns the engine, the page owns the editor, and page destruction on window close is off - the page dies only with the tab.

`StandaloneEditor::onTabClosed` unregisters the clip engine and the piano-roll connection, removes the mixer strip, then walks the audio library for entries owned by `audioInsert(row)`, removes the Builder blocks that match both path and route channel, and removes the library entries. Block removal runs under one hoisted audio shield so the whole cascade costs a single settle rather than one per block. Files on disk are never deleted.

---

## Cross-references

- **Engine Tabs (Layers, Bass, Drums).md** - the note-driven tab families; same ribbon, page-preset and freeze shape.
- **Inst Page.md**, **Plugins Page.md** - the other two "+"-menu tab families.
- **BaySickPlayer.md** - the sample player every Clips tab runs.
- **Builder Page.md** (the arrangement grid and its browser), **Sample Library.md** (where the audio comes from and how its path is stored), **Mixer.md**, **Effect Racks.md**.

---

## Differs from Carry-Forward

- **Engine ownership.** Carry-Forward describes the page owning its player. The player is now owned by `EngineRig` under `TabKind::Clips`; the page is a non-owning view.
- **Add route.** Carry-Forward's cascade is drop-driven only. There are now two ribbon routes ("+" > BaySickPlayer > Audio Clips, and the Clips dropdown's "+ Add BaySickPlayer...") that open a file picker and create the page and strip with **no** Builder block.
- **Strip naming.** The strip is named from the sample file, not from the Builder grid row label.
- **NAM/IR on Clips.** Removed - Clips is a sample player; amp and cabinet simulation lives on the Inst page.
- **Per-page EQ tab.** Gone; a clip's pre-rack and post-rack EQ are edited from the Effects page or the window's Pre EQ / Post EQ rows.
- **Tab close cascade.** Carry-Forward's "no file delete" contract stands for files on disk, but closing a Clips tab now also removes the page's audio-library entries and its Builder blocks.
