# KBS DAW Fork - Implementation Plan (program level)

> **For execution:** one batch per milestone, run under this repo's batch system
> (Main Plan §0: plan file + running notes per batch, `- [ ]` punch-list, a build
> gate at the end of every task, one commit per task, Jeff's smoke at the batch
> boundary).  The first batch is written to task granularity in
> [`Batch Plans/lithe-lopping-lynx.md`](Batch Plans/lithe-lopping-lynx.md); each
> later batch gets its detailed plan at its own start, from this document, the
> spec, and the code map.

**Goal:** turn a pruned copy of BaySickDAW's shell into KBS DAW - a closed-source,
GPL-free, VST3-only DAW with an FL-style mixer, an Instruments window, and a
five-icon ribbon - built in `Documents\BaySickDAW\KBS DAW\` as its own repository.

**Architecture:** the same processor / model / views shape as BaySickDAW with the
engine layer replaced by hosting; a growable insert bank where every insert
sums its inputs and routes anywhere with a level; instruments as a list keyed
by stable id (spec sections 3-6).

**Tech stack:** JUCE 8.0.12 (free tier), VST3 hosting via JUCE, the two
out-of-process helper exes, MSVC via `do_build.bat`, CMake, NSIS, LAME as a DLL.

**Spec:** [`KBS DAW Fork - Design.md`](KBS DAW Fork - Design.md).
**Code map:** [`Research Reports/lite-shell-map-2026-09-02.md`](Research Reports/lite-shell-map-2026-09-02.md).

---

## How every batch runs

- Work happens in `KBS DAW/` (its own repo).  BaySickDAW's tree is not touched
  except for the one `.gitignore` line that hides the fork folder.
- Build: `cmd.exe /c "C:\Users\jeffm\Documents\BaySickDAW\KBS DAW\do_build.bat"`
  in the background; judge `KBS DAW\build_log.txt` by the six exit codes and
  four link lines exactly as here.
- Headless smoke after every carve task: `"KBS DAW\build\...\Release\KBS DAW.exe" --shot "Builder"`
  (the harness constructs the processor, pages and menus with no device); a
  crash or a `FAILED` line is a task failure.
- Commit per task, brief one-liner (Rule 9).  Jeff's Debug-then-Release smoke
  walks each batch's sheet at the batch boundary.
- Docs: the fork carries its own `Plans & Specs/` seeded with the spec, this
  plan, and per-batch plan + running-notes files; its own `CLAUDE.md`.

## Batch 1 - KBS-Seed (M0 + M1): seed, prune, carve

Plan: [`Batch Plans/lithe-lopping-lynx.md`](Batch Plans/lithe-lopping-lynx.md).

**Scope.** Create the fork repository from the prune list; prove the untouched
copy builds (M0); remove every built-in engine, effect, pedal, the strip EQ,
the engine pages and tab kinds, the Core Library fetcher, the factory-content
loaders, in an order where every task ends green; collapse the five tab enums
to one and the effect-type enum to `None` + `VST3Plugin`; hunt the dead code
the compiler cannot see; give the fork its own `CLAUDE.md`, `README`,
`LICENSE` placeholder (M1).

**Exit:** the fork builds green; launches; Builder, Mixer, Effects, Piano Roll
open; a hosted VST3 instrument added through the legacy Plugins tab plays
through Master; save and reload keep it.  Nothing new exists yet.

**Sizing:** ~120 files touched, ~114k lines deleted, 9 build gates.

## Batch 2 - KBS-Core (M2): instruments as a list

**Scope.** The `Instrument` record with stable ids replaces `(TabKind::Plugins,
pageIndex)`; `EngineRig` becomes the instrument model; `InstrumentsWindow` (5B)
replaces `PluginsPage` + the per-type ribbon slot; the ribbon becomes five
icon buttons (5A); pattern note data and automation lanes target instrument
ids; page presets become instrument presets (7.4); the project root tag becomes
`KBSDAWProject` and a `BaySickDAWProject` file is refused (7.3); the piano-roll
context dropdown lists instruments; `<PianoRollSelection>` stores an
instrument id.

**Files (from the map):** `Source/EngineRig.h/.cpp` (`EngineTab` -> `Instrument`;
`createEngineFor` one arm; `trackIdFor` -> `inst_<id>_`); `Source/Standalone/RibbonTabBar.*`
(five fixed slots, icon paint, no instance dropdowns); new
`Source/Standalone/InstrumentsWindow.h/.cpp`; `Source/Standalone/PluginsPage.*`
(folded into the window or kept as the per-instrument editor host);
`Source/Standalone/StandaloneEditor.cpp` (`serializeTabsInto` /
restore `:15050-15278`, `:18202-18400+` -> instrument records; piano-roll
dropdown `:2320-2376`; live-MIDI target `:2378-2396`; automation lane names
`:4158-4230`; freeze kind table `PluginProcessor.cpp:4372-4452`);
`Source/PatternManager.*` (target by id); `Source/ProjectManager.cpp:250, 333`
(root tag); `Source/Standalone/PagePresetIO.*` (one kind); `Source/Standalone/PianoRollPage.h:38-68`
(`EngineId` -> instrument id).

**Exit (golden test):** empty project; `+ Add instrument`; pick a VST3; its row
appears; its editor opens; notes in the piano roll play it through Master; save,
close, reopen: identical.  A BaySickDAW project file is refused with a message.

## Batch 3 - KBS-Mixer (M3): the FL mixer

**Scope, in dependency order:**
1. **Insert identity** - `insert_<slot>_` params replace `mixer_<family>_<i>`;
   `MixerChannelIds` becomes slot-based; the bank grows by `+ Add insert`;
   Master = slot 0; `MixerTrackStrip` gets one generic type with the utility
   row on every strip; meters keyed by slot; undo owner tags by slot; the
   legacy `MixerState` snapshot deleted.
2. **Every insert sums** - a shared predecessor-sum prologue in
   `Engine/Tasks/*` (today only `PassiveStripTask` / `MasterTask` sum;
   `EngineInsertTask.cpp:71, 80`, `CompositeAudioInsertTask.cpp:66`,
   `DirectFileTask.cpp:27` clear); `RoutingGraph` edges carry a level
   (`BaySickGraph.cpp:2748` unity today); the family whitelist
   `MixerPage.cpp:482-587` and `isValidBusSendTarget` (`BaySickGraph.h:219-222`)
   go; the 17 hard-coded bus ids (`BaySickGraph.h:172-182, 1008-1107`) become a
   role; bus solo axis becomes per-slot.
3. **Routes UI** - the strip "+" menu offers every other insert with a level
   knob and a sidechain toggle (4.3); the cable overlay reads routes.
4. **Instrument insert selector** (4.6) on the Instruments row and window;
   per-output for multi-out.
5. **Live input per insert** - ungate `addLiveInputParams`
   (`PluginProcessor.cpp:9318, 9330`), `_arm` (`:8979-8983`), the strip rows
   (`MixerTrackStrip.cpp:361-450`); capture moves from `VoxStripTask` /
   `InstStripTask` into the generic insert task; **input gain** added
   (`PluginProcessor.cpp:8916-9038`, `MixerTrackStrip`).
6. **Multi-out** - bus layouts on `HostedPlugin` (`HostedPlugin.cpp:41-46,
   615-621`), one render target per active output bus, the bridged helper
   prepared with the plugin's real layout (`PluginHostMain.cpp:324-347`).
7. **Latency compensation** - hosted-instrument latency in
   `updateBusLatencies` (`BaySickGraph.cpp:1589, 1681-1686`); bridged latency
   change message added to the protocol (`PluginBridgeProtocol.h:69-95`).
8. **Rack to 12 slots** - `EffectRack.h` slot count; `+ Add Pre EQ` /
   `+ Add Post EQ` load buttons; the rack window resized.

**Exit:** route an instrument to insert 3; route inserts 3 and 4 into insert 5
at -6 dB each; hear the sum; flag a route sidechain and key a hosted
compressor from it; arm insert 6 with a live input and record a take onto the
grid; a 16-out drum VST lands on 16 inserts; a plugin with 2048 samples of
latency stays in time with one that has none.

## Batch 4 - KBS-Host (M4): the remaining hosting gaps

**Scope:** sidechain into a hosted instrument (`ISidechainEngine` on
`HostedPlugin`, `EngineInsertTask.cpp:81-88`); MIDI out of a plugin +
plugin-to-plugin MIDI + MIDI-effect plugins (collect `producesMidi` output per
block, a "MIDI source" selector per instrument, helper->host MIDI in the
bridge protocol); per-instrument MIDI input device + channel and multiple live
targets (replace the single collector `PluginProcessor.h:640-657`, `.cpp:3579-3595`,
open devices individually `StandaloneApp.cpp:1184-1199`); `.vstpreset` and
program-list browsing (`setCurrentProgram` has no caller today); MIDI Learn
onto hosted-plugin params (`MidiLearnRegistry.cpp:154, 183`); bridge-on-demand
for instruments (`EngineRig.cpp:477-484`); host-resizable plugin windows for
plugins that declare support (`PluginsPage.cpp:270`, `EffectWindows.cpp:145`);
the 32-bit bridge exercised with a Win32 build of a Steinberg SDK example
plugin.

**Exit:** one scenario per gap on the walk sheet, all passing, including the
32-bit plugin loading, playing and surviving a forced crash.

## Batch 5 - KBS-Ship (M5): identity, notices, installer, manual

**Scope:** every renamed identifier from the map's section 9 - user-data root
(`AppPaths.h:10-14`), six settings files, project layout, window / product /
splash / About strings, CMake `project()` + `PRODUCT_NAME`, helper exe names +
`SandboxedPluginClient.cpp:102-112`, bridge handshake id
(`PluginBridgeProtocol.h:62`), installer names / paths / registry keys, undo
tags and lane prefixes already changed in M2 / M3; LAME switched from static
lib to a DLL loaded at runtime (+ its notice); `NOTICES.txt` staged beside the
exe; About lists every component; VST / ASIO trademark lines; fontaudio OFL +
CC BY texts; `LICENSE` = the KBS EULA; `THIRD_PARTY_LICENSES.md` rewritten;
NSIS installer renamed and repackaged; the manual: shell chapters + control
tables, figures re-shot with the fork's harness, In View + In Depth only,
`Riff Machine` -> `Tune Generator` throughout.

**Exit:** the installer output installs on a clean Windows user account, the
app launches with an empty `Documents\KBS DAW\`, F1 opens a manual with zero
engine content, About and `NOTICES.txt` list every third-party component.

## Cross-batch rules

- The brand docket's rulings apply to the fork at M5 and to BaySySickDAW at
  QA-Solstice Task 5, separately (10b).
- Nothing from the fork is ported back to BaySickDAW inside these batches;
  findings that also apply here are routed through BaySickDAW's own Rule 3 at
  each batch close.
- Jeff moves the folder when he says so; until then it stays at
  `Documents\BaySickDAW\KBS DAW\`, ignored by this repo.
