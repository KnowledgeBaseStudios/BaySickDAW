# QA-Solstice - Harmless to BaySickSolstice rename + shipped-name brand review - Plan (solar-scrubbing-sparrow)

> **Canonical path:** `Plans & Specs/Batch Plans/solar-scrubbing-sparrow.md`
> Paired running notes: `Plans & Specs/Running Notes/solar-scrubbing-sparrow.md`

> **For execution:** inline, `- [ ]` checkboxes as the punch-list.  Builds run by
> Claude via the pinned `do_build.bat` invocation (CLAUDE.md Build System), judged
> from `build_log.txt` (six exit codes, four link lines, zero `error C|LNK|MSB`).
> Jeff runs the Debug-then-Release smoke and every ear check.

## Context

Jeff, 2026-09-02: "Harmless" is the name of Image-Line's additive synth - same
name, same product category, shipping inside a DAW.  That is the exact case the
no-brand-names rule exists for, and it was missed because the rule's own memory
file listed `Harmless` under "brand-safe substitutes" (written 2026-06-07), so
every audit since skipped it by design.  The trademark exposure lives in what
ships: the exe (display name, menus, tooltips, RTTI symbols), the installer's
factory content (`Presets/Harmless/`, templates naming the engine), and the
manual (prose, figures, anchors).

Two deliverables:

1. **Rename the engine in full** to `BaySickSolstice` (Jeff's casing) - display
   name, engine-type id, APVTS state tag, param prefix, classes, files, source
   dir, factory presets + templates, manual figure codes, and every doc
   including history.
2. **A brand-safety review list** of everything else that ships under a real
   mark - preset / kit / template names and user-facing strings - produced by a
   semantic read (not a keyword grep), handed to Jeff for per-item calls.
   Renames from that list are NOT applied in this batch unless Jeff rules them
   in; the list is the deliverable.

**Blast radius:** 410 tracked files mention the name (26 source files in the
engine folder, ~75 source refs outside it, 152 preset XMLs, 24 templates, the
manual pipeline, ~90 Plans & Specs docs, CLAUDE.md, memory).  Mechanically low
risk: the substitution is a proper noun with capital H; the English adjective
"harmless" (~25 comments) is lowercase and untouched.  Two hand-caught sites:
the lowercase variable `harmless` at `StandaloneEditor.cpp:15708/15729`, and the
on-screen knob label `"HARM"` (`HarmlessEditor.cpp:787`, Blur-section harmonics)
which collides with the old figure-code root and STAYS.

**Consequences accepted (Jeff, 2026-09-02, item 1 "This is fine"):** every saved
project with a Harmless tab and every user patch under the old folder stops
loading.  No migration (pre-v1 rule).  Verified graceful: an unknown engine-type
string in project XML makes `EngineRig::setEngineType` return `nullptr`
(`EngineRig.cpp:566`) and the tab restores engineless - no crash path.  Jeff's
disk has no `My Presets` under the old folder (checked), so no user patches are
at risk; `Templates/My Templates/Test Kit.xml` is a user file that references
the old engine and is left alone.

**Dependencies:** none.  QA-ManualPress stays open with its close held; this is
its own batch (Jeff, item 4c).

**Risk:** medium - large diff, one build, one manual regeneration.
**Effort:** ~1 day.

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| S1 | New name is `BaySickSolstice`, exactly that casing. | Jeff, 2026-09-02. Matches the `BaySick*` engine family. |
| S2 | Rename **in full**: display name, engine-type id `"Harmless"` -> `"BaySickSolstice"`, APVTS tag `HarmlessState` -> `BaySickSolsticeState`, param prefix `harm` -> `bso`, all `Harmless*` classes / files, `Source/Harmless/` -> `Source/BaySickSolstice/`, `Presets/Harmless/` -> `Presets/BaySickSolstice/`, figure codes `BSHARM`/`BSHARMM` -> `BSSOL`/`BSSOLM`. | Jeff: "rename Harmless in full". Prefix `bso` is free and follows `bss`/`bsb`/`bsp`. Figure-code rename has precedent (the 2026-08-13 map did `HARM` -> `BSHARM`). |
| S3 | `AdditiveVoice`, `HarmonicEngine`, `SpectralModules`, `VisualizerScreen` keep their names. | Descriptive, not brand-derived. Stated 2026-09-02, no objection. |
| S4 | Saved projects + user patches break; no migration. | Jeff item 1. Pre-v1 no-backward-compat rule. Graceful degrade verified at `EngineRig.cpp:566`. |
| S5 | On-disk sync is Claude's: copy the regenerated `Presets/BaySickSolstice/` + `Templates/Factory/` into `Documents/BaySickDAW/`, delete `Documents/BaySickDAW/Presets/Harmless/`. | Jeff item 2a. The generator already writes to Documents, so this is the natural flow. |
| S6 | Docs scrub covers EVERYTHING - history included (Work Log, Running Notes, Batch Plans, Previously Implemented, Research Reports, Carry-Forward, Test Plans, Main Plan), plus CLAUDE.md, `STANDALONE_UI_CHANGES.md`, README, `.claude/agents`, memory (both dirs). | Jeff item 3b. Carry-Forward's "frozen" and the Work Log's "append-only" rules yield to a global rename - the entries keep their meaning, only the noun changes. Recorded in §9. |
| S7 | Own batch; Main Plan slot = **new Phase 8, Legal / Brand Safety**, which also becomes T4's home. | Jeff items 4c and 3c (2026-09-02). |
| S8 | Four tasks: T1 source + factory content + disk; T2 manual; T3 docs + memory; T4 brand review. Source-side figure-anchor strings (`kDotAnchor` values) move with T1 so T2 is manual-assets-only. | Jeff item 1a. |
| S9 | **Commit per task, no approval gate** - write the brief one-liner and commit. | Jeff item 2a: "no approval just go". Standing for this batch only. |
| S10 | T4 = one semantic-read agent over every shipped string and content name; the list comes to Jeff for per-item calls. No renames pre-applied. | Jeff item 4a. The rule's own audit method (keyword grep missed UREI). |
| S11 | Batch ID `QA-Solstice`, plan file `solar-scrubbing-sparrow`. | Naming is Claude's call. |

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open.  Everything above was posed in chat and answered before
this file was written (Rule 5).

---

## Files to modify

### Task 1 - engine rename in source + factory content

**Renamed (git mv), `Source/Harmless/` -> `Source/BaySickSolstice/`:**
`HarmlessEditor.h/.cpp`, `HarmlessFilterRow.h/.cpp`, `HarmlessLAF.h`,
`HarmlessModEditor.h/.cpp`, `HarmlessModRegistry.h/.cpp`,
`HarmlessProcessor.h/.cpp`, `HarmlessRoutingMatrix.h/.cpp`,
`HarmlessSynth.h/.cpp`, `HarmlessWaveformButton.h`, `HarmlessXYZPad.h/.cpp`
-> `BaySickSolstice*` (18 files).  `AdditiveVoice`, `HarmonicEngine`,
`SpectralModules`, `VisualizerScreen` move with the directory, names unchanged.

**Content, engine folder:** every `Harmless` token (classes, includes, the
`"HarmlessState"` tag at `HarmlessProcessor.cpp:10`, `getName()` at
`HarmlessProcessor.h:40`, `getEngineTitle()` at `HarmlessEditor.h:46`, the
`"HarmlessRebuild"` thread name at `HarmlessSynth.h:284`, `presetsDir()` at
`HarmlessEditor.cpp:1332`); the prefix at `HarmlessProcessor.cpp:11,13`
(`"_harm_"` -> `"_bso_"`) and its comments (`HarmlessProcessor.h:11,61,69`,
`HarmlessFilterRow.h:16`, `HarmlessSynth.cpp:486`); the `kDotAnchor` strings
`"BSHARM-n"` in `HarmlessEditor.cpp` (20) + `HarmlessModEditor.cpp` (6).
`"HARM"` at `HarmlessEditor.cpp:787` stays.

**Content, outside the engine folder** (file:line from the 2026-09-02 grep):
- [CMakeLists.txt:179-191](CMakeLists.txt:179) source list; `:489`, `:703` include dirs.
- [Source/EngineRig.cpp:3,51,562](Source/EngineRig.cpp:562) include, apvts cast, the engine-type factory string.  [EngineRig.h:277-278](Source/EngineRig.h:277) comment.
- [Source/Standalone/StandaloneEditor.cpp:40,4381,4671,8290-8347,11060,15608-15786,15936](Source/Standalone/StandaloneEditor.cpp:4381) - include; **`if (tag == "harm")` -> `"bso"`**; audition casts; dirty hook; `registerHarmlessModAutomation` (rename + the lowercase `harmless` variable at 15708/15729); the window-floor `title.contains ("Harmless")`.  [StandaloneEditor.h:715,1405-1413](Source/Standalone/StandaloneEditor.h:1413).
- [Source/Standalone/LayersPage.cpp:9-10,125,158-174,247,256,391,531,994-1012](Source/Standalone/LayersPage.cpp:391) + `.h:23`; [BassPage.cpp:9-10,125,154-170,243,253,376,510,918,1112-1119](Source/Standalone/BassPage.cpp:376) + `.h:23`; [DrumPage.cpp:1165,1691](Source/Standalone/DrumPage.cpp:1165) + `.h:41`.
- [Source/Standalone/RibbonTabBar.cpp:620-622,847,853](Source/Standalone/RibbonTabBar.cpp:620) (menu literals incl. `"+ Add Harmless"`) + `.h:52`.
- [Source/Standalone/BuilderPage.cpp:22,2172,10484-10522](Source/Standalone/BuilderPage.cpp:10506) (include, cast, `HarmlessModLength`).
- [Source/Standalone/ShotHarness.cpp:53,601,662-682,1774,1787,1979](Source/Standalone/ShotHarness.cpp:662) - `shootHarmless`, figure names `"Harmless"` and `"BaySickSynth-BaySickPlayer-Harmless Menu"`, window name `"shot:harmless"`, group key `"harmless"`.
- [Source/Standalone/SharedUI.cpp:1767](Source/Standalone/SharedUI.cpp:1767) (`BSHARM-27` in the shared preset-button anchor) + `SharedUI.h:933`.
- Comment-only: `PluginProcessor.h:243,699,700`, `PluginProcessor.cpp:2780,7557`, `ProjectManager.cpp:434`, `ApvtsDirtyTracker.h:9`, `PagePresetIO.h:67`, `PianoRoll.h:618`, `PianoRollPage.h:64`, `BaySickTitleBar.cpp:48,135`, `EngineSidechainHelper.h:8,23`, `BaySickBass*`, `BaySickSynth*`, `BaySickPlayer*`, `BaySickNAMIREditor.*`.
- [Source/Standalone/STANDALONE_UI_CHANGES.md](Source/Standalone/STANDALONE_UI_CHANGES.md) (15 refs) - **T3**, not here.

**Factory content:**
- [Tools/gen_factory_presets.py:38,58,139-140,272-274,499-503,1851-1859,2976-2991,3749-3765](Tools/gen_factory_presets.py:58) - `HARMLESS_DIR`, `ENGINE_HARM = ("harm", "HarmlessState")`, the `<HarmlessState>` writer, the `TEMPLATES` engine/presetPath pairs.
- `Presets/Harmless/` (152 XML, git rm) -> `Presets/BaySickSolstice/` (regenerated).
- `Templates/Factory/*.xml` (24 of 29 reference the engine) - regenerated.
- Jeff's disk: `Documents/BaySickDAW/Presets/Harmless/` (delete), `Presets/BaySickSolstice/` + `Templates/Factory/` (copy in).

### Task 2 - manual

- `Manuals/src-m2/instrument/BSHARM.html` -> `BSSOL.html`, `BSHARMM.html` -> `BSSOLM.html` (git mv + content).
- Prose: `src-m2/instrument/BSSBM.html:36-37`, `src-m2/shell/EVT.html:36`, `PRMMNU.html:16-17`, `TABUTN.html:19`, plus the two renamed files.
- In The Weeds quotes: `src-m3/IMP-53,57,62,63,67,81,82,83,84,85,87.html` - same substitutions as source so the verbatim quotes stay verbatim; `IMP-87` codehead path.
- [Manuals/assets/marker-coords.py:42-45,293-294,304,321,400-414](Manuals/assets/marker-coords.py:293) - codes + the two figure file names.
- [Manuals/assets/generate-manual.py:428](Manuals/assets/generate-manual.py:428) IMP map; `control-blurbs.py:92` heading.
- `Manuals/assets/code-rename-map-full-2026-08-13.json` (`"HARM": "BSHARM"` becomes the cumulative `"HARM": "BSSOL"`), `manual-2-interim.html`, `bsd-docs.json` (regenerated).
- `Plans & Specs/System Reference/Callout Registry.md` (58 code refs), `MANUAL-1 Screenshot List.md` (69).
- Figures: `Manuals/figures/Harmless.png` and `BaySickSynth-BaySickPlayer-Harmless Menu.png` replaced by the re-shot set; `Plans & Specs/System Reference/Pictures/Harmless.png` + `Added Images/...Harmless Menu.png` renamed.
- Generated outputs: `Manuals/manual.html`; `manual-1/2/3.html` + `index.html` if the generator still emits them (checked at task start), else sed.

### Task 3 - docs + memory

Every remaining tracked file: `git grep -l Harmless -- . ":!libs" ":!juce"` after T1+T2.  Known: 90 `Plans & Specs` files (rename `System Reference/Harmless.md` -> `BaySickSolstice.md`; INDEX link), `CLAUDE.md` (6), `README.md` (2), `.claude/agents/*.md` (5).  Memory: `~/.claude/projects/C--Users-jeffm-Documents-BaySickDAW/memory/` (8 files incl. the brand rule + `MEMORY.md`) and `...-Vibesynth/memory/` (10).

### Task 4 - brand review

No files modified.  Output lands in this plan under Task 4 as the review list.

---

## Tasks

### Task 1 - Engine rename in source + factory content

- [ ] `git mv Source/Harmless Source/BaySickSolstice`; `git mv` the 18 `Harmless*` files to `BaySickSolstice*`.
- [ ] Substitution pass over `Source/` + `CMakeLists.txt` + `Tools/gen_factory_presets.py`: `Harmless` -> `BaySickSolstice` (case-sensitive; lowercase adjective untouched), `_harm_` -> `_bso_`, `"harm"` -> `"bso"`, `tk_{trackId}_harm_` comments.  Then by hand: `harmless` variable (`StandaloneEditor.cpp:15708/15729`) -> `solstice`; `"shot:harmless"` -> `"shot:baysicksolstice"`; group key `"harmless"` -> `"baysicksolstice"`.
- [ ] Figure-anchor strings: `BSHARMM-` -> `BSSOLM-` first, then `BSHARM-` -> `BSSOL-` (order matters) in `BaySickSolsticeEditor.cpp`, `BaySickSolsticeModEditor.cpp`, `SharedUI.cpp:1767`.  Confirm `"HARM"` at the Blur knob survived.
- [ ] Verify: `git grep -n "Harmless\|harm_\|\"harm\"\|BSHARM" -- Source CMakeLists.txt Tools` returns nothing; `git grep -n "harmless" -- Source` returns only the adjective.
- [ ] Generator: run `Tools/gen_factory_presets.py` with `USERPROFILE` pointed at a scratch dir; diff scratch `Presets/*` (minus the engine folder) and `Templates/Factory/*` against the repo - expect zero drift outside the rename.  Copy scratch `Presets/BaySickSolstice/` and `Templates/Factory/*` into the repo; `git rm -r Presets/Harmless`.
- [ ] Disk (S5): copy repo `Presets/BaySickSolstice/` -> `Documents/BaySickDAW/Presets/BaySickSolstice/`; copy `Templates/Factory/*` -> `Documents/BaySickDAW/Templates/Factory/`; delete `Documents/BaySickDAW/Presets/Harmless/`.
- [ ] **Build gate:** pinned `do_build.bat` invocation; six exit codes 0, four link lines, zero errors.
- [ ] Commit (S9): `QA-Solstice Task 1: Harmless -> BaySickSolstice engine rename (dir, 18 files, classes, engine id, state tag, prefix harm->bso, figure anchors); factory presets + templates regenerated; Documents copy synced (Source, CMakeLists, Tools, Presets, Templates)`.
- [ ] `/draft-doc running-notes` -> apply.

**Tell Jeff (smoke, Debug first then Release):**
(1) Layers ribbon `+ Add` shows `BaySickSolstice`; adding one opens a panel titled BaySickSolstice, accent unchanged.
(2) Its `Load Preset` menu walks `BaySickSolstice/<category>/...` - all 20 categories present, a patch loads and sounds.
(3) `Save Preset` lands in `Documents/BaySickDAW/Presets/BaySickSolstice/My Presets/`.
(4) New project from template `Bass Music`: 5 layers + 3 basses populate as BaySickSolstice with their patches (the template's `presetPath` values changed).
(5) Automate any BaySickSolstice knob: the Builder lane label reads `Pg Layer 1 - BaySickSolstice - ...`.
(6) Open a pre-rename project that had a Harmless tab: the tab restores with no engine and the app does not crash.  (Expected loss per S4.)
(7) The Blur section still shows its `HARM` knob label.

### Task 2 - Manual: figure codes, prose, figures, regeneration

- [ ] `git mv` the two src-m2 chapter files; substitution pass over `Manuals/` (excluding `figures/`): `BSHARMM` -> `BSSOLM`, then `BSHARM` -> `BSSOL`, `Harmless` -> `BaySickSolstice`, `_harm_` -> `_bso_`, `"harm"` -> `"bso"`, `harmless->` -> `solstice->`; the two figure file names inside `marker-coords.py`.  Rename the two `System Reference/Pictures` masters.
- [ ] Callout Registry + Screenshot List: same substitutions (they are the manual's data, so they move with T2 even though they live under Plans & Specs).
- [ ] Re-shoot: `BaySickDAW.exe --shot --docs` (Release exe from T1's build) into `Manuals/shots-staging/`; diff-sheet against `Manuals/figures/`; copy staging over figures; `git rm` the two old-named PNGs.
- [ ] `python Manuals/assets/generate-manual.py` - expect `topics placed: 89 of 89`, no `DOT MISMATCH` regressions vs the QA-ManualPress baseline, `BSSOL` dots generated.
- [ ] Verify: `git grep -n "Harmless\|BSHARM" -- Manuals "Plans & Specs/System Reference/Callout Registry.md" "Plans & Specs/System Reference/MANUAL-1 Screenshot List.md"` returns nothing.
- [ ] Commit (S9): `QA-Solstice Task 2: manual follows the rename - figure codes BSHARM/BSHARMM -> BSSOL/BSSOLM, prose + In The Weeds quotes, figures re-shot, registry + screenshot list, manual regenerated (Manuals + System Reference)`.
- [ ] `/draft-doc running-notes` -> apply.

**Tell Jeff:** (1) F1 manual: sidebar shows BaySickSolstice under the instruments, the chapter's figure is the new panel with dots on it, In Depth control table present; (2) search "Harmless" returns nothing, search "Solstice" finds the chapter; (3) eyeball `Main frame.png` and `Hosted Plugin.png` (the two hand captures) for any visible "Harmless" - if either shows it, that is a re-capture for you.

### Task 3 - Docs + memory scrub (everything, history included)

- [ ] `git mv "Plans & Specs/System Reference/Harmless.md" "Plans & Specs/System Reference/BaySickSolstice.md"`.
- [ ] Substitution pass over every remaining tracked file from `git grep -l Harmless -- . ":!libs" ":!juce"`: `Harmless` -> `BaySickSolstice`, `_harm_`/`tk_..._harm_` -> `_bso_`, `HarmlessState` etc. follow.  Do NOT touch the lowercase adjective or `HARM` knob mentions.  Includes: Plans & Specs (all subfolders), CLAUDE.md (engine names line, the `### Harmless-specific` heading, `HarmlessLAF` / `HarmlessEditor` / `HarmlessCurvePoint`), README.md, `.claude/agents/*.md`, `Files For Claude/` if any.
- [ ] Memory, both dirs: same substitution; **rewrite `feedback_no_brand_names_in_user_facing_strings.md`** - remove `Harmless` from "brand-safe substitutes", add the 2026-09-02 miss as the second recorded incident and the lesson (our own engine names must be checked against competitor product catalogs, not assumed safe because we coined them); update `MEMORY.md` hooks; `feedback_match_jeff_text_casing.md` example.
- [ ] Verify: `git grep -n "Harmless" -- . ":!libs" ":!juce"` returns nothing; `grep -rn Harmless ~/.claude/projects/*BaySickDAW*/memory ~/.claude/projects/*Vibesynth*/memory` returns nothing except deliberate historical mentions inside the brand rule's incident record.
- [ ] Build gate: no source touched in this task; T1's gate stands (recorded in running notes).
- [ ] Commit (S9): `QA-Solstice Task 3: docs + memory scrub, history included - Plans & Specs (System Reference/Harmless.md renamed), CLAUDE.md, STANDALONE_UI_CHANGES, README, .claude/agents (Plans & Specs + CLAUDE.md + Source/Standalone/STANDALONE_UI_CHANGES.md + README + .claude)`.
- [ ] `/draft-doc running-notes` -> apply.

### Task 4 - Brand-safety review list

- [ ] Dispatch ONE agent (general-purpose, read-only) with this brief: read every USER-FACING string in `Source/` (tooltips, `setBodyTooltip`, visible labels, `ChickenHeadSelector` option labels + marks + hover text, `PopupMenu` items, button text, `AlertWindow` / message-box text, drawn `Label` / `drawText` literals, knob names, `buildKnobs` tooltip slots, `setupNamed` args, window titles); every shipped content NAME (file and folder names under `Presets/`, `Kits/`, `Templates/`); the manual prose (`Manuals/src-m2/**`); the installer's visible strings (`Installer/*.nsi`).  Flag every real company / product / model / artist name.  For each: file:line (or path), the exact string, the mark and who owns it, and a class - (A) a real mark used AS our name for a feature / preset / kit; (B) a nominative reference ("if you come from X", "in the style of X") in shipped text; (C) a generic-use term that is also someone's mark (808, hoover, supersaw, reese).  Do not fix anything.  Seed the agent with the 2026-09-02 grep findings so it extends them rather than re-finding them.
- [ ] Merge the agent's list with the grep findings; land the full list in this section as a table; present to Jeff for per-item calls (rename / keep / defer).  **STOP** - the per-item calls are spec calls.
- [ ] Renames Jeff rules in: applied in a follow-up task added here (presets regenerate through the generator; UI strings by hand), one build gate, one commit.
- [ ] `/draft-doc running-notes` -> apply.

**Grep findings to seed the agent (2026-09-02):** kit folders `TR-808` / `TR-909` (Roland, 6 kit files); `Presets/BaySickDrums/Yamaha Group/` (16 files); Roland `Fat Juno Sub`, `Juno Poly`, `Juno Warm Pad`, `Jupiter Brass`, `Jupiter Brass Pad`, `JP-8000 Supersaw Pad`; Yamaha `DX7 Glass/Metal/Woodblock`, `DX Style Tubulum`, `CS-80 Bell`, `CS-80 Brass Lead`, `RX-11 Kick/Snare`; Moog x8 (`Moog Sub`, `70s Moog Bass`, `Moog Minimoog Bass`, `Analog Moog Style`, `Moog Lead`, `Moog Lead Woop`, `Moog Style Lead`, `Moog Hz Interval`); Rhodes x6, Wurlitzer/Wurli x4, Hammond x3 + `B3` x2, `Mellotron Pad`, `Clavinet` / `Synth Clav`, `Farfisa Organ`, `Solina Strings`, `OB-8 Brass` / `OB-8 String Pad` (Oberheim), `Prophet Sync` (Sequential), `ARP Lead` (Korg); Nintendo `Gameboy Pulse` x2; `Skrillex Reese` (a living person); tooltips `"Raise for Juno/Prophet/CS-80 analog warmth"` (`BaySickBassEditor.cpp:311`, `BaySickSynthEditor.cpp:313`) and `"(Van Halen 'Jump', Final Countdown brass, Moog/ARP)"` (`BaySickSynthEditor.cpp:104`).  Class C candidates: bare `808`/`909`/`303`, `Reese`, `Supersaw`, `Acid Hoover`, `VHS Keys`, `SID Chip`, every arpeggio "Arp".

---

## Verification (end-to-end smoke)

After T1-T3: the seven T1 scenarios plus the three T2 checks, in Debug then
Release.  Plus: (8) `git grep -c Harmless -- . ":!libs" ":!juce"` is empty; (9)
`Documents/BaySickDAW/Presets/` has `BaySickSolstice/` and no `Harmless/`; (10) a
fresh installer build (`Installer/BaySickDAW-Tester.nsi`) stages
`Presets/BaySickSolstice` - run only if Jeff wants the installer re-cut now.

## Routing notes (Rule 3 application during execution)

- Anything found in T4 that is NOT a name (a real bug, a wrong tooltip) goes to
  §9 at close and to the owning batch's surface, per Rule 3.
- QA-ManualPress's own open items (mode-variant screenshots, the 31 hand-coord
  dots) stay in QA-ManualPress; T2 does not absorb them.

## Carry-Forward Reference touch points

None - the Carry-Forward is itself a rename target in T3 (S6) and holds no
architecture this batch depends on.

---

## Carry-Over

(Rule 2 block, written at every stopping point.)
