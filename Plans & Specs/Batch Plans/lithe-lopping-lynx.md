# KBS-Seed - fork seed, prune and carve (M0 + M1) - Plan (lithe-lopping-lynx)

> **Canonical path:** `Plans & Specs/Batch Plans/lithe-lopping-lynx.md`
> Paired running notes: `Plans & Specs/Running Notes/lithe-lopping-lynx.md`
> Program plan: [`../KBS DAW Fork - Plan.md`](../KBS DAW Fork - Plan.md).  Spec: [`../KBS DAW Fork - Design.md`](../KBS DAW Fork - Design.md).

> **For execution:** inline, `- [ ]` checkboxes as the punch-list.  Every task
> ends with the fork's build gate (its own `do_build.bat`, judged by the six exit
> codes + four link lines in `KBS DAW\build_log.txt`) and, from Task 3 on, the
> headless launch smoke.  Commit per task in the FORK's repository - no approval
> asked per task (group-run rule, Jeff 2026-09-02); the only BaySickDAW commit is
> Task 1's `.gitignore` line.  The one stop is the END of the batch: Jeff's smoke
> on the walk sheet below, then KBS-Core starts on his word.

## Context

First batch of the KBS DAW fork.  Everything is deletion and collapse; nothing
new is built.  The carve is driven by the code map's coupling ledger, not by
the compiler alone: each task names its sites, the build confirms.  Order is
chosen so every task ends green: effects first (self-contained), then the strip
EQ, then the vocal / NAMIR / pedals group with the Vox and Inst tab kinds, then
the sfizz trio, then the four synth engines with the Layers / Bass / Drums /
Clips tab kinds (the biggest), then the content loaders, then the dead-code
hunt.  The legacy Plugins tab survives this batch on purpose - it is the proof
at the end that hosting still works.

**Risk:** medium.  Large mechanical diffs in the four biggest files; the
compiler catches type errors, the ledger + dead-code hunt catch the rest.
**Effort:** 2-3 days.  **Dependencies:** none; BaySickDAW's queue waits (10b).

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| S1 | Clean-start repository (`git init`, first commit = pruned shell), not a clone. | This repo's `.git` is 1.16 GB with GPL libs and 830 presets in history; Jeff's caveat "just the files you need".  Flip to a clone by saying so before Task 1. |
| S2 | Prune list = spec section 10. | Jeff 2026-09-02. |
| S3 | Internal class names stay (`BaySickDAWProcessor`, `BaySickGraph`, `EngineRig`); identity is strings / paths / tags (M5). | Renaming classes is churn with no user-facing effect. |
| S4 | The legacy Plugins tab + `PluginsPage` survive this batch; they are replaced in KBS-Core. | M1's exit proof needs a working hosted-instrument path. |
| S5 | LAME stays statically linked in this batch. | Legal at ship time (M5), not at build time; keeps M0 = "builds untouched". |
| S6 | Batch ID `KBS-Seed`, plan file `lithe-lopping-lynx`. | Naming is Claude's. |

## Sub-spec calls surfaced for ExitPlanMode

None open.

---

## Files to modify

**BaySickDAW (one line):** `.gitignore` gains `KBS DAW/`.

**Junctions for M0:** the untouched copy still includes the engine sources, which include the pruned libraries' headers unconditionally, and the CMake `fontaudio` module.  Task 2 builds against temporary `mklink /J` junctions to BaySickDAW's `libs/{rubberband,world,signalsmith-stretch,signalsmith-linear,sfizz,NeuralAmpModelerCore,fontaudio}`; each junction is removed in the task that deletes its last consumer (fontaudio at Task 4, NAM/pitch libs at Task 5, sfizz at Task 6).

**Fork (created by Task 1):** everything below is a path inside `KBS DAW/`.

- Task 3 (effects): delete `Source/DSP/*` except the survivors (`AudioClipStreamer.*`, `PhaseVocoder.*`, `Mp3Writer.*`, `DSPBase.h`, `EffectVisualFeed.h`, `SpectrumFeed.h`, `EngineSidechainHelper.h`, `PanLaw.h`, `LufsMeterDSP.*`, `TruePeakMeter.*`, `LoudnessSpec.h`, `BpmDetect.h`, `DenoiseDSP.*` if `cleanFile` is still reached - check), `Source/DSP/EffectParamMap.*`; edit `Source/EffectRack.h:19-80` (enum), `EffectRack.cpp`, `Source/Standalone/SlotComponent.cpp` (89 sites), `EffectPresetIO.cpp` (90), `EffectEditorPanels.cpp/.h` (43; keep `HostedPluginEditor`), `EffectWindows.cpp` (10), `EffectsPage.cpp`, `FxRackPresetIO.cpp`, `EffectVisual.h`; `CMakeLists.txt` source list.
- Task 4 (strip EQ): delete `Source/DSP/StripEq.*`, `Source/DSP/Kbs/`, `Source/Standalone/EqWindowUI/`, `Tools/EqTests`, `run_eq_tests.bat`, the `BaySickEqTests` target (`CMakeLists.txt:913-923`); edit `BaySickGraph.h/.cpp` and `PluginProcessor.cpp` EQ hooks, `StandaloneEditor.cpp` Pre EQ / Post EQ rows (`:6567-6679`), `EffectsPage.cpp`.
- Task 5 (vocal / NAMIR / pedals + Vox / Inst): delete `Source/BaySickVocal/`, `Source/BaySickNAMIR/`, `Source/BaySickPedals/`, `Source/Vox/`, `Source/Inst/`, `Source/Standalone/EngineChainProcessor.*`, `Source/DSP/{BaySickAlignDSP,BaySickPitchDSP,PitchCorrectorDSP,LibraryPitchShifters,PitchShifters,MonitorPitchShifter,PitchTrackerYIN,PolyPitchTracker,MicSimDSP,MicPlacementDSP,PolyphaseOversampler}*`, `Source/SafeNamModel.h`, `libs/{rubberband,world,signalsmith-stretch,signalsmith-linear,NeuralAmpModelerCore}` (already pruned; remove their CMake blocks `:242-455`, gates `:726-769`); edit `EngineRig.cpp` (`:562-657` Vox / Inst arms, `:673-772`), `Engine/Tasks/VoxStripTask.*`, `InstStripTask.*` (delete; live capture returns in M3), `PluginProcessor.cpp` (`:6664-6689`, `:7978-7986`, `:8813-8870`, `:9318, 9330`), `MixerPage.cpp:2597`, `StandaloneEditor.cpp` Vox / Inst arms, `ProjectBundler.cpp` NAMIR site.
- Task 6 (sfizz trio): delete `Source/BaySickGuitars/`, `Source/BaySickBasses/`, `Source/BaySickRustyDrums/`, `Source/SlideSampler/`, `Source/Standalone/{BaySickRustyDrumsPage,RustyDrumsMapWindow,AriaControlPanel}*`, `Source/SafeSfzKit.h`, `Engine/Tasks/{RustyInsertTask,RustyDrumsProducerTask}*`, `Tools/rusty_kit_hitboxes.txt`, `Assets/big_rusty_drums.png`; the sfizz CMake block; edit `BaySickGraph.h:96` (`kMaxRustyStrips`), `PluginProcessor.h` sfizz arrays, `PagePresetIO.*` (Guitars / Basses / Rusty strings), `RibbonTabBar.cpp` rows.
- Task 7 (synth engines + their tabs): delete `Source/BaySickSolstice/`, `Source/BaySickSynth/`, `Source/BaySickBass/`, `Source/BaySickPlayer/`, `Source/Clips/`, `Source/Standalone/{LayersPage,BassPage,DrumPage,DrumKitGrid}*`, `Source/{WavetableOscillator,SynthFilter,AdsrEnvelope,LFO,SynthSound,BroadcastSynthesiser}*`, `Source/MidiLearn/DrumTriggerMap.*`, `Engine/Tasks/CompositeAudioInsertTask.*` (the `mClipPlayer` path; audio clips on the grid are `DirectFileTask` - verify before deleting); edit `EngineRig.h/.cpp` (collapse to `TabKind::Plugins`), `StandaloneEditor.cpp` (the ladders: `:1740-1758`, `:2320-2376`, `:2893-2925`, `:5858-5885`, `:6567-6679`, `:11053-11080`, `:15050-15278`, `:17480-17498`, `:18202-18400+`, `:15704-15790`), `RibbonTabBar.cpp:561-652, 845-939`, `PluginProcessor.cpp` (`:977-1085`, `:3308-3404` family schedule, `:4372-4452` freeze kinds, `:8146-9362` creation table), `PatternManager` `<Mixer>` node, `PagePresetIO.h:29`.
- Task 8 (content loaders): delete `Source/CoreLibraryInstaller.h`, `Source/SampleLibrary.*` if only Core Library reached it (check `library:` token use in `ProjectManager.h:39-43` - keep the resolver for `mysamples:`), `Tools/gen_factory_presets.py`; edit `ProjectManager.cpp:451-466` (first-launch housekeeping), `StandaloneEditor.cpp:816, 12031-12034` (Options 505), `CMakeLists.txt:238-240` (`juce_cryptography` only if nothing else uses it).
- Task 9 (dead-code hunt): `EngineRig.h:49` / `RibbonTabBar.h:24` / `PianoRollPage.h:38` / `MixerPage.h:76` / `BaySickGraph.h:631` (five enums -> the one survivor each), `MixerPage.h:605-608` + `PatternManager.cpp:1254` (`MixerState`), `MixerPage.cpp:482-587` (whitelist stays until M3 - note only), `ProjectManager.cpp:729-734` (dead settings keys), `StandaloneEditor.cpp:20399-20417` (File Settings take-type rows), `StandaloneApp.cpp:526-558` (`DrumTriggerVelocity` pref), `ShotHarness.cpp` figure groups (18 of 43 die), `Manuals/assets/marker-coords.py` + `generate-manual.py` engine entries.
- Task 10 (fork docs): new `CLAUDE.md`, `README.md`, `LICENSE`, `Plans & Specs/` seed.

---

## Tasks

### Task 1 - Seed the fork repository

- [ ] BaySickDAW: append `KBS DAW/` to `.gitignore`; commit here: `Fork: ignore the KBS DAW folder (.gitignore)`.
- [ ] Create the tree from the prune list.  The source list is `git ls-files`, so only TRACKED files are candidates (nothing untracked in this tree - `My Templates`, `My Kits`, `My Presets`, `build/` - can travel).  Write `Tools/seed_fork.py` in BaySickDAW (scratch tool, not committed) and run it:

```python
# Tools/seed_fork.py - copy BaySickDAW's tracked shell into KBS DAW/, minus the prune list.
import io, os, shutil, subprocess
ROOT = r"C:\Users\jeffm\Documents\BaySickDAW"
DST  = os.path.join(ROOT, "KBS DAW")
DROP = ("Presets/", "Kits/", "Templates/", "Resources/Acoustic IRs/", "Resources/Tape/",
        "Assets/big_rusty_drums.png",
        "libs/rubberband/", "libs/world/", "libs/signalsmith-stretch/", "libs/signalsmith-linear/",
        "libs/sfizz/", "libs/NeuralAmpModelerCore/", "libs/fontaudio/",
        "Plans & Specs/", "Files For Claude/", ".claude/",
        "Tools/gen_factory_presets.py", "Tools/EqTests/", "Tools/rusty_kit_hitboxes.txt", "run_eq_tests.bat",
        "Manuals/src-m3/", "Manuals/src-m2/instrument/", "Manuals/src-m2/mixing-effects/",
        "LICENSE")                       # replaced by the fork's own LICENSE
tracked = subprocess.check_output(["git", "-C", ROOT, "ls-files"], text=True, encoding="utf-8").split("\n")
copied = skipped = 0
for p in tracked:
    p = p.strip()
    if not p: continue
    if p.startswith(DROP) or p == "LICENSE":
        skipped += 1; continue
    d = os.path.join(DST, p)
    os.makedirs(os.path.dirname(d), exist_ok=True)
    shutil.copy2(os.path.join(ROOT, p), d)
    copied += 1
print("copied", copied, "| pruned", skipped)
```

Run: `python Tools/seed_fork.py` -> expect roughly `copied 13200 | pruned 1500` (juce 4377 + kept libs + Source 413 + the rest; the exact numbers are recorded in the running notes).

- [ ] Verify the prune: `dir "KBS DAW\Presets"` etc. must not exist; `dir "KBS DAW\libs"` shows exactly `asiosdk concurrentqueue lame webview2`; `dir "KBS DAW\Source"` shows every folder (engines are removed by the carve, not the prune, so M0 builds untouched).
- [ ] Write `KBS DAW\LICENSE`:

```
KBS DAW - Copyright (c) 2026 KnowledgeBase Studios. All rights reserved.
Proprietary. Licensed under the KnowledgeBase Studios End User License Agreement
(to be supplied from the KBS repository's Legal/EULA.md before first release).
This tree was seeded from BaySickDAW commit <hash> on 2026-09-02; the author
holds the copyright on all first-party code and relicenses it here.
Third-party components and their licences: see THIRD_PARTY_LICENSES.md.
```

- [ ] `cd "KBS DAW" && git init && git add -A && git commit -m "KBS DAW: seed from BaySickDAW <hash>, pruned per the fork spec section 10"` (the `-A` is correct HERE: the whole tree is the deliverable and nothing untracked exists yet).  Record `<hash>` = `git -C .. rev-parse --short HEAD`.
- [ ] Verify isolation: in BaySickDAW `git status` shows only the `.gitignore` change (committed) and nothing under `KBS DAW/`; in the fork `git remote -v` prints nothing (no origin - it must never push to the BaySickDAW GitHub repo).

### Task 2 - M0: the untouched copy builds

- [ ] Check `do_build.bat` for absolute paths: `findstr /i "Documents\\BaySickDAW" "KBS DAW\do_build.bat"`.  If it hardcodes the source path, change those lines to `%~dp0` (the bat's own folder) so the copy builds in place and after the move.  Same check on `do_configure.bat` and `make_installer.bat`.
- [ ] Build: `cmd.exe /c "C:\Users\jeffm\Documents\BaySickDAW\KBS DAW\do_build.bat"` (background).  Expect: six exit codes 0, four link lines, output under `KBS DAW\build\` and `build32\`.  If sfizz / NAM / rubberband / world / signalsmith are missing: their CMake blocks are `if(EXISTS ...)` presence-gated (`CMakeLists.txt:242-455`) - the gates fall through, BUT the engine headers include those libraries unconditionally, so **M0 needs the pruned libraries present or the carve first.**  Resolution locked here: Task 2 builds against temporary junctions to BaySickDAW's `libs/` for the six pruned libraries incl. fontaudio (`mklink /J`), each removed in the task that deletes its last consumer (see Files to modify).
- [ ] Verify: `KBS DAW\build\BaySickDAWStandalone_artefacts\Release\BaySickDAW.exe` launches (it is still BaySickDAW by name - expected) and closes clean.
- [ ] Commit: `KBS-Seed Task 2: build scripts location-independent; M0 builds`.

### Task 3 - Carve: built-in effects and the rack collapse

- [ ] `CMakeLists.txt`: remove every `Source/DSP/*DSP.cpp`, `*StyleDSP.cpp`, `SibilanceSpectralProcessor.cpp`, `EffectParamMap.cpp` from the standalone source list (`:512-681`), and `Source/Standalone/EffectEditorPanels.cpp` panels' dependencies stay (the file stays).  Delete the files.
- [ ] `Source/EffectRack.h:19-80`: the enum becomes

```cpp
enum class EffectType : int { None = 0, VST3Plugin = 121 };   // ordinal pinned: persisted in project XML
```

- [ ] `EffectRack.cpp`: `createEffect` keeps only the `VST3Plugin` arm; `SlotComponent.cpp`, `EffectPresetIO.cpp`, `EffectEditorPanels.cpp`, `EffectWindows.cpp`, `EffectsPage.cpp`, `FxRackPresetIO.cpp`: every `case EffectType::<builtin>` / `EffectType::<builtin>` reference deleted; the effect picker (`FXPICK`) lists the plugin allowlist only.  `EffectVisual.h` and the visual strip: delete if only built-in panels used `hasVisual()` (hosted plugins never do).
- [ ] Build gate; headless smoke `--shot "Effects Panel List"` (the picker figure must render with plugin rows only).
- [ ] Commit: `KBS-Seed Task 3: built-in effects + EffectParamMap removed; EffectType = None + VST3Plugin; rack VST3-only (Source, CMakeLists)`.

### Task 4 - Carve: the strip EQ

- [ ] Delete `Source/DSP/StripEq.*`, `Source/DSP/Kbs/`, `Source/Standalone/EqWindowUI/`, `Tools/EqTests/`, `run_eq_tests.bat`; remove `BaySickEqTests` (`CMakeLists.txt:913-923`); remove the `fontaudio` module + link (`CMakeLists.txt:62, :718`) and any `fontaudio::` reference left (`grep -rn fontaudio Source/` - the EQ rail was the only consumer per the brand review; `ShotHarness.cpp` hard-fails without the module today, so its EQ figure group goes here rather than in Task 9).
- [ ] `BaySickGraph.h/.cpp`, `PluginProcessor.cpp`: remove the per-strip EQ node, `eqChannelId`, the EQ latency term, the spectrum feeds; `StandaloneEditor.cpp:6567-6679`: the `Pre EQ` / `Post EQ` page rows go (they return as rack slots in M3).
- [ ] Build gate; headless smoke `--shot "Mixer"`.
- [ ] Commit: `KBS-Seed Task 4: strip EQ removed (StripEq, Kbs core, EqWindowUI, EqTests) (Source, Tools, CMakeLists)`.

### Task 5 - Carve: vocal, NAM/IR, pedals, and the Vox / Inst tab kinds

- [ ] Delete the folders and DSP files listed under Task 5 in Files to modify; remove the five library CMake blocks and gates; remove the junctions from Task 2 for those libraries.
- [ ] `EngineRig`: delete the Vox / Inst arms (`createEngineFor :575-634`, `registerWithProcessor`, `unregisterFromProcessor`); `TabKind::Vox` / `Inst` stay as enumerators until Task 9 (persisted ints).
- [ ] `PluginProcessor.cpp`: delete `VoxStripTask` / `InstStripTask` creation (`:8813-8870`), the WET recorder Vocal cast (`:6664-6689`), the `sweepNonRealtime` Vocal cast (`:7978`), `addLiveInputParams` call sites (`:9318, 9330` - the function itself stays, M3 calls it per insert); `MixerPage.cpp:2597` `setInstStripNoLiveInput`; `StandaloneEditor.cpp` Vox / Inst arms in every ladder; `ProjectBundler.cpp` NAMIR site; `RibbonTabBar.cpp` Vox / Inst rows.
- [ ] Build gate; headless smoke `--shot "Builder"`.
- [ ] Commit: `KBS-Seed Task 5: BaySickVocal, NAMIR, Pedals, Vox/Inst pages + tasks + pitch/mic DSP removed; rubberband/world/signalsmith/NAM blocks gone (Source, CMakeLists)`.

### Task 6 - Carve: the sfizz trio and SlideSampler

- [ ] Delete per Files to modify; remove the sfizz CMake block + junction.
- [ ] `BaySickGraph.h:96` drop `kMaxRustyStrips`; `PluginProcessor.h` sfizz arrays; `PagePresetIO` Guitars / Basses / Rusty strings; `RibbonTabBar.cpp` rows; `EngineRig` arms; `StandaloneEditor.cpp` Rusty / Guitars / Basses sites (strings: Rusty 16, Guitars 11, Basses 9).
- [ ] Build gate; headless smoke `--shot "Builder"`.
- [ ] Commit: `KBS-Seed Task 6: BaySickGuitars/Basses/RustyDrums, SlideSampler, Aria panel, sfizz removed (Source, Assets, Tools, CMakeLists)`.

### Task 7 - Carve: the four synth engines and the Layers / Bass / Drums / Clips tab kinds

- [ ] Delete per Files to modify.  Before deleting `CompositeAudioInsertTask`: confirm grid audio clips render through `DirectFileTask` (`PluginProcessor.cpp:1113-1124` routes `ArrangementBlock::routeChannel`); if the composite task carries the audio-row path, keep it and delete only its `mClipPlayer` member (`.h:82`, `.cpp:29, 156`).
- [ ] `EngineRig.h/.cpp`: `createEngineFor` keeps the Plugins arm only; `apvtsOf` returns the hosted plugin's APVTS or `nullptr`; `capacityOf` / `trackIdFor` one arm each.
- [ ] `StandaloneEditor.cpp`: each ladder listed under Task 7 collapses to its Plugins branch; `registerBaySickSolsticeModAutomation` (`:15704-15790`) deleted with its header decl (`.h:1405-1413`); `floorSizeFor` engine titles (`:15913-15936`) keep the plugin case; F7 `isPlayerTabType` (`:5889-5909`) = Plugins.
- [ ] `PluginProcessor.cpp`: `readClipCtl` (`:977-1085`) deleted; the family schedule (`:3308-3404`) keeps the Plugins family; the creation table (`:8146-9362`) keeps `registerPluginEngine` + the bus / master / aux entries; freeze kinds (`:4372-4452`) keep Plugins.
- [ ] `RibbonTabBar.cpp:561-652, 845-939`: the add menu keeps `VSTPlugin >`; the instance dropdown keeps the Plugins type.
- [ ] `PatternManager.cpp:1254` `<Mixer>` node: keep for now (Task 9 decides with `MixerState`).
- [ ] Build gate; headless smoke `--shot "Builder" "Ribbon + Menu" "Mixer"`.
- [ ] Commit: `KBS-Seed Task 7: Solstice/Synth/Bass/Player engines, Layers/Bass/Drums/Clips pages, DrumKitGrid, synth primitives removed; EngineRig + editor ladders collapsed to Plugins (Source, CMakeLists)`.

### Task 8 - Carve: content loaders and first-launch housekeeping

- [ ] Delete `CoreLibraryInstaller.h`, `gen_factory_presets.py`; `ProjectManager.cpp:451-466` `runFirstLaunchHousekeeping` loses the shortcut + the `offerCoreContentDownload` tail; Options menu item 505 (`StandaloneEditor.cpp:12031-12034`, menu `:11827-11860`) removed; `SampleLibrary`: keep (the `mysamples:` token and project-relative resolution serve audio clips); drop `juce_cryptography` from `CMakeLists.txt:238-240` if the SHA-256 was its only use (grep `SHA256`).
- [ ] Build gate; headless smoke `--shot "File Settings"`.
- [ ] Commit: `KBS-Seed Task 8: Core Library fetcher, preset generator, first-launch content prompts removed (Source, Tools, CMakeLists)`.

### Task 9 - Dead-code hunt (the compiler cannot see these)

- [ ] Collapse the five enums to their survivor (`TabKind::Plugins`, `TabType::Plugins` + the four required slots, `EngineKind::Plugin`, `StripKind` Plugin / Bus / Master / Aux, `InsertKind` likewise); delete the five hand-written maps (`StandaloneEditor.cpp:1896-1906, 7777-7788, 15228-15236, 18175-18182`; `PluginProcessor.cpp:4405-4422`); keep persisted ordinals stable (append-only rule in `EngineRig.h:43-48`, `RibbonTabBar.h:22-23`).
- [ ] Delete the legacy `MixerState` snapshot (`MixerPage.h:605-608`, `applyMixerSnapshot` / `syncApvtsFromMixerState`) and the `<Mixer>` node writer (`PatternManager.cpp:1254`); `UndoActions.h` entries that snapshot it.
- [ ] Settings: drop `shortcutCreated`, `skipGlobalLockPromptBank0/1`, `skipKitReplacePrompt`, `skipCoreContentPrompt` (`ProjectManager.cpp:729-734`), `<MidiTriggerVelocity>` + `DrumTriggerVelocity` (`StandaloneApp.cpp:526-558`, `.h:218-220`, the Audio Settings row), `ui_prefs` keys `fsWriteDry/DryCleaned/Wet/WetCleaned`, `fsDenoiseStrength`, the File Settings take-type toggles + de-noise combo (`StandaloneEditor.cpp:20399-20417`), `pitchMultiResetNoPrompt` / `pitchWorldOfflineNoPrompt`, `<RecentNAMFiles>` / `<RecentIRFiles>`.
- [ ] Keybindings: remove the DrumKit (36 rows) and VocalEditors (24) reference groups (`KeyBindings.cpp:578-770`); the command catalog's engine-bound commands.
- [ ] Shot harness: remove the 18 engine figure groups from `kFigures[]` (`ShotHarness.cpp:1972-1990` + their functions); `marker-coords.py` / `generate-manual.py` / Callout Registry: drop the Instrument and Mixing & Effects entries (the manual is rebuilt at M5; the pipeline must still run: `python Manuals/assets/generate-manual.py` -> `topics placed: N of N`).
- [ ] `grep -rn "Solstice\|BaySickSynth\|BaySickBass\|BaySickPlayer\|BaySickVocal\|NAMIR\|Pedals\|Guitars\|Basses\|Rusty\|sfizz\|Harmless\|kit\b" Source/` reviewed line by line; anything left is a comment to delete or a name to fix.
- [ ] Build gate; headless smoke `--shot` (every remaining figure); `generate-manual.py` runs.
- [ ] Commit: `KBS-Seed Task 9: dead code after the carve - five tab enums collapsed, MixerState snapshot, dead settings + prefs + dialog rows, engine keybinding groups, engine figure groups (Source, Manuals)`.

### Task 10 - The fork's own docs

- [ ] `CLAUDE.md` (fork): build command (its own `do_build.bat`, same six-code / four-line gate), the headless smoke, the batch system, the standing rules (6-9), the spec + plan pointers, the "never add a remote pointing at BaySickDAW" rule.
- [ ] `README.md`: what KBS DAW is, seed commit, how to build.
- [ ] `Plans & Specs/` in the fork: copy the spec, the program plan, this batch plan + its running notes; `THIRD_PARTY_LICENSES.md` marked "rewritten at KBS-Ship".
- [ ] Commit: `KBS-Seed Task 10: fork CLAUDE.md, README, Plans & Specs seed`.

---

## Verification (M1 walk sheet - Jeff, Debug then Release, fork exes)

| # | Do | Expect |
|---|---|---|
| 1 | Launch `KBS DAW\build\...\Debug\BaySickDAW.exe` | Main frame, transport, ribbon with Builder / Mixer / Effects / Piano Roll / `+`; no jassert |
| 2 | `+` | Only `VSTPlugin >` (instruments from the allowlist) |
| 3 | Add a VST3 instrument | A Plugins tab appears, its editor opens in a contained window, a strip appears in the Mixer |
| 4 | Piano roll: draw notes, play | You hear the instrument through Master |
| 5 | Effects page: Master rack `+` | Only allowlisted VST3 effects; load one; it processes |
| 6 | Save, close, reopen | Instrument, notes, effect restored |
| 7 | Options menu | No "Get Sound Content"; File Settings shows freeze / capture rows only |
| 8 | Help > About | Still says BaySickDAW (expected until M5) |
| 9 | `--shot` in the fork | Every remaining figure renders, `0 failed` |

## Routing notes (Rule 3)

Findings that also apply to BaySickDAW (e.g. a dead setting key discovered in
Task 9) are noted in the fork's running notes AND routed into BaySickDAW's §9
at this batch's close - not fixed here.

## Carry-Forward Reference touch points

None - the fork does not carry BaySickDAW's Carry-Forward; the code map is its
reference.

---

## Carry-Over

(Rule 2 block, written at every stopping point.)
