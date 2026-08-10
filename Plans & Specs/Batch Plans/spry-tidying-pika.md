# QA-Cleanup - Phase 6 in one batch: player rename, dead-code fold-ins, verified-clean sweeps, security agent - Plan (spry-tidying-pika)

**Canonical path:** `Plans & Specs/Batch Plans/spry-tidying-pika.md`
**Paired running notes:** `Plans & Specs/Running Notes/spry-tidying-pika.md`

**For execution.** This is the punch-list, not a discussion document. Every
`- [ ]` is an action. Every "Tell Jeff:" is a hard stop where execution waits.
Read Main Plan sections 0 + 5 (the `QA-Cleanup` entry) + Carry-Forward + the
Implemented Work Log before starting, per Rule 1.

---

## Context

**What this batch is.** The last batch of G4, and the last coding batch of the
bulk run. It is the whole of Phase 6, collapsed from seven batches to one on
2026-08-10 by Jeff's call: *"we are going to wipe G5 completely out with the
cleanup work, and will move the build test into the test campaign."*

**Why one batch is honest and not a shortcut.** Each dissolved batch was
checked individually before the collapse, not waved off:

| Former batch | What was actually found |
|---|---|
| QA-Audit | Its source half already ran, as QA-Soundness: seven category sweeps over the whole tree, eight adversarial re-sweep rounds, 9,160 dead-code sites examined, ten dead files deleted. The findings ledger in `keen-combing-heron.md` IS the manifest this batch existed to produce. |
| QA-Cleanup-1 | Reduced to a handful of mechanical fold-ins. The full-build warning sweep it called "likely the bulk of the effort" already reads 0 for C4702 / C4189 / C4505 in `build_log.txt`. |
| QA-Cleanup-2 | Nothing to remove. All ten vendored libraries are live. `lunasvg` was the one dead folder and it went at QA-Soundness. |
| QA-Cleanup-3 | Nothing to remove, and a filename grep here is actively dangerous (see Task 5). |
| QA-Cleanup-4 | Already done. `.gitignore:8` covers `Files For Claude` and none of its 738 MB was ever tracked. |
| QA-PlayerRename | Folds in here. Its stated saved-project risk rests on a `vp_*` parameter prefix that does not exist. |
| QA-RC / QA-RC-lite | Dissolved into the Master Test Plan campaign. |

**What is left is therefore real work, not a formality:** a rename across 151
files (widened from the player-only scope by Jeff's SC-8 call), two genuine
dead-code strips, two sweeps that need their evidence written down so nobody
re-runs an unsafe grep in six months, and one net-new audit that has never run
against this app in any form.

**Risk.** Moderate, concentrated in Task 1. The rename is mechanical but reaches
151 files and moves 13, and it carries one deliberate breaking change: the
`"VibeRackStates"` saved-state node is renamed with everything else, so projects
saved before this batch lose their rack state (SC-9, Jeff's call, no shim
pre-v1). It also has one trap - the Tape effect's `Vibe` knob must survive the
sweep untouched, which is why every substitution is whole-identifier anchored.
The dead-code strips are provably unreachable. The security audit is read-only;
whatever it finds gets triaged with Jeff before any fix lands.

**Effort.** Medium-large, driven by the rename. Roughly: rename ~4h (151 files,
~1,548 occurrences, 13 file moves - mechanical but wide), fold-ins ~1.5h,
sweeps ~1h, agent build + Tier-1 run ~2h, plus a build gate per task.

**Dependencies.** QA-Soundness landed. The pending 41-file working tree (Task 0)
must be committed first so this batch's diff is its own.

---

## Spec calls already locked

| ID | Decision | Reasoning |
|---|---|---|
| SC-1 | Phase 6 collapses to ONE batch, run as the last batch of G4 | Jeff 2026-08-10. Six of the seven former batches verified out to nothing-to-do or already-done; keeping seven batch shells for one batch of work is bookkeeping theatre. |
| SC-2 | The player rename ships. Source renames; System Reference docs follow; historical docs (Implemented Work Log, Previously Implemented, Running Notes, closed Batch Plans) stay exactly as written | Jeff 2026-08-10. The app is open source and meant to show people how it works; a class called `VibePlayer` in a product with no "Vibe" anywhere in it teaches the reader a name that does not exist. Historical docs are a record of what happened and get falsified by retroactive editing. |
| SC-3 | `Files For Claude` needs no work | Verified: `.gitignore:8` already covers it and it was never tracked. Recorded as evidence, not executed as a task. |
| SC-4 | The clean-slate build test moves to the Master Test Plan campaign as test **G-5** | Jeff 2026-08-10. It is a build test, not a feature test, and it belongs where Jeff is already working. |
| SC-5 | Cleanup-2 and Cleanup-3 produce written evidence, not deletions | The sweeps ran and found nothing removable. The evidence is the deliverable, because the next person to look will otherwise re-run the same grep and reach the opposite conclusion. |
| SC-6 | Build `/audit-security` IN this batch, then run it | Jeff 2026-08-10, pick (b). Building it here means its Tier-1 sweep runs BEFORE the test campaign rather than after, so anything it finds is fixable while the coding run is still open. |
| SC-7 | No group-boundary smoke in this batch | Jeff, standing for the bulk run. All functional verification is the campaign's. |
| SC-8 | Rename scope = **(c)**: every `Vibe*` / `Vibesynth*` identifier in the tree, EXCEPT the Tape effect's `Vibe` control | Jeff 2026-08-10, resolving SSC-1: *"do C then for rename just so things are as clean for the user as possible."* 151 files, ~1,548 occurrences, 13 file/folder renames. |
| SC-9 | `"VibeRackStates"` renames with everything else, and existing saved projects lose their rack state | Follows from SC-8 - the consequence was stated in the option Jeff picked. Pre-v1 rule (`feedback_no_backward_compat_pre_v1`) forbids a migration shim, so the state is dropped rather than migrated. |
| SC-10 | `mPianoRoll` gets the full drop, not the symptom fix | Jeff 2026-08-10, resolving SSC-2: *"yes do the mpianoroll drop."* The symptom fix no longer exists as an option (see SSC-2). |
| SC-11 | `VibeSynthProcessor` -> **`BaySickDAWProcessor`** | Jeff 2026-08-10, resolving SSC-1a (pick a). Matches `BaySickDAWStandaloneApp` and the exe name, and cannot collide with the existing `BaySickSynthProcessor`. |

---

## Sub-spec calls surfaced for ExitPlanMode

**SSC-1 - How far does the rename reach?**

`VibePlayer` is not the only surviving old name in `Source/`. Full inventory of
`Vibe*` identifiers, by occurrence count:

**Corrected 2026-08-10** after Jeff pushed back on the first pass, which listed
every string containing "Vibe" without asking whether each one was actually
confusing. The test is NOT "does the user see it" (none of these are user-facing;
they are all internal names). The test is **does the name contradict something
that has a different name.** Sorted by that test:

**Group A - the name contradicts a real name, so a reader is actively misled:**
- `VibePlayerProcessor` 109, `VibePlayerEditor` 55, `VibePlayer` 34,
  `VibePlayerDSP` 8, `VibePlayerLAF` 6 - the app calls this engine
  **BaySickPlayer**.
- `VibeSynth` 60, `VibeVoice` 77, `VibeSynthSound` 5 - these live INSIDE
  `Source/VibePlayer/VibePlayerDSP.h` (verified: `VibePlayerDSP.h:647`, `:145`).
  They are the sample player's Synthesiser, but they read as the synth engine.
  A reader looking for BaySickSynth lands on the sample player. These come along
  with the player rename automatically.
- `VibeSynthProcessor` 376 - **this is the main app processor**
  (`PluginProcessor.h:128`), and it is the worst one in the tree. A reader
  looking for BaySickDAW's processor finds a class named for a dead project, and
  `BaySickSynthProcessor` is a real separate class, so the two read as siblings
  when one is the whole app and the other is one engine.
- `VibeSampleManager` 38, `VibeRegion` 18, `VibeForwardMemoryAudioSource` 6 -
  player internals, same file.

**Group B - stale prefix on machinery with no counterpart name.** Nothing to
contradict; the user never sees these named anything at all. Cosmetic only:
`VibeGraph` 504, `VibeSlider` 156, `VibeLAF` 64, `VibeThreadPool` 43,
`VibeTooltip` 7, `VibeGraphInsertKindBridge` 2.

**Group C - the OLD PRODUCT NAME on app-level classes:**
`VibesynthStandaloneApp` 22 (`StandaloneApp.h:141`), `VibesynthConstants` 17
(a header included by 7+ files), `VibesynthEditor` 8 (`PluginEditor.h:10`),
`VibeSynthWindow` 7, `VibeDAW` 3 (comment only).

**Group D - NOT the old name, leave alone:** the Tape effect's **Vibe** control
(`SaturationDSP.cpp` `tape_vibe` / `vibe`, `EffectEditorPanels.cpp:3251`). That
is a real user-facing control name and an ordinary English word.

**Persistence exposure, found here and NOT what the Main Plan entry claims.**
The entry names a `vp_*` parameter prefix as the saved-project risk; no such
prefix exists (player params are `tk_<trackId>_bsp_`), so the player rename is
clean. But `"VibeRackStates"` at `PluginProcessor.cpp` 6848 / 6849 / 6897 /
7073 / 7074 / 7322, `StandaloneEditor.cpp:14419` is a **ValueTree node name
written into saved project XML**. Renaming that string breaks rack state on
every existing project. Pre-v1 rule says no migration shims, so the accepted
outcome would be "old projects lose their rack state" - but that is a call, not
a detail. It sits in Group C.

Options:

- **(a)** Group A only, which is the Main Plan entry's scope: the seven
  `Source/VibePlayer/*` files plus their internal classes. ~223 occurrences over
  33 files. Leaves `VibeSynthProcessor` (the main processor) named as it is.
- **(b)** Groups A and C: the player plus the old product name on app-level
  classes, including `VibeSynthProcessor`. ~440 occurrences. Includes the
  `"VibeRackStates"` saved-state string, so existing projects lose rack state.
- **(c)** Groups A, C and B: everything except the Tape Vibe control.
  ~1,150 occurrences, adds a `Source/Engine/VibeThreadPool.*` file rename.
  Mechanical but touches nearly every file, landing the same week the campaign
  opens.
- **(d)** Group A now, the rest recorded as a Future State entry for after v1.

**RESOLVED 2026-08-10: (c)**, locked as SC-8. Measured scope after the ruling:
**151 files, ~1,548 occurrences, 13 file/folder renames.** Full identifier map
in Task 1.

**SSC-1a - what does `VibeSynthProcessor` become?** Every other name in the map
is a mechanical `Vibe` -> `BaySick` substitution. This one cannot be, because
`BaySickSynthProcessor` is already a real class (the BaySickSynth engine's
processor) and that collision is the whole reason this name is worth fixing.
The class is the app's single `juce::AudioProcessor` (`PluginProcessor.h:128`).

**RESOLVED 2026-08-10: (a) `BaySickDAWProcessor`**, locked as SC-11.

- **(a)** `BaySickDAWProcessor` - names it after the product, matches
  `BaySickDAWStandaloneApp` and the exe name.
- **(b)** `BaySickProcessor` - shorter; reads as "the BaySick processor" with no
  DAW-inside-DAW repetition.
- **(c)** `MainProcessor` - drops the brand entirely and says what it is. Inside
  a project called BaySickDAW, a `BaySickDAW` prefix on the one central class
  carries no information.

**SSC-2 - The `mPianoRoll` fold-in is bigger than its Main Plan entry says.**

The entry offers two shapes: "(i) minimal symptom-fix, delete the `setTabName`
writeback lines" or "(ii) full per-page `mPianoRoll` drop". Source check says
(i) is not on the table any more: `buildPianoRollTab()` has NO caller in
`LayersPage` / `BassPage` / `DrumPage`, so `mPianoRoll` is null for the entire
life of all three pages, `getPianoRoll()` has zero callers app-wide, and every
`if (mPianoRoll)` guard is a permanent no-op. That is ~25 dead lines per page,
not 5. Option (ii) is the only one that leaves the file honest. Flagging it
because the effort is 5x the entry's estimate, not because the call is unclear.
(`BaySickRustyDrumsPage::buildPianoRollTab()` IS called and stays.)

**SSC-3 - What happens to security findings.**

Task 7 runs the new agent. If it returns a HIGH finding, per
`feedback_qa_batches_fix_bugs_dont_defer` the default is to fix it in-batch, but
a genuine security fix can be large. The triage step surfaces findings to Jeff
with severity and effort before anything is written.

---

## Files to modify

### Task 0 - pending commit (no edits; commit only)
41 dirty paths, listed in full at the commit step.

### Task 1 - full `Vibe*` rename (SC-8, scope c)

**13 file / folder renames:**

| From | To |
|---|---|
| `Source/VibePlayer/` (folder) | `Source/BaySickPlayer/` |
| `Source/VibePlayer/VibePlayerDSP.h` / `.cpp` | `Source/BaySickPlayer/BaySickPlayerDSP.h` / `.cpp` |
| `Source/VibePlayer/VibePlayerEditor.h` / `.cpp` | `Source/BaySickPlayer/BaySickPlayerEditor.h` / `.cpp` |
| `Source/VibePlayer/VibePlayerProcessor.h` / `.cpp` | `Source/BaySickPlayer/BaySickPlayerProcessor.h` / `.cpp` |
| `Source/VibePlayer/VibePlayerLAF.h` | `Source/BaySickPlayer/BaySickPlayerLAF.h` |
| `Source/VibeGraph.h` / `.cpp` | `Source/BaySickGraph.h` / `.cpp` |
| `Source/Engine/VibeThreadPool.h` / `.cpp` | `Source/Engine/BaySickThreadPool.h` / `.cpp` |
| `Source/VibesynthConstants.h` | `Source/BaySickConstants.h` |

**Identifier map.** Counts are current-tree occurrence counts.

*Group A - player (moves with the folder):*

| From | To | n |
|---|---|---|
| `VibePlayerProcessor` | `BaySickPlayerProcessor` | 110 |
| `VibeVoice` | `BaySickPlayerVoice` | 77 |
| `VibeSynth` | `BaySickPlayerSynth` | 60 |
| `VibePlayerEditor` | `BaySickPlayerEditor` | 56 |
| `VibePlayer` (bare) | `BaySickPlayer` | 41 |
| `VibeSampleManager` | `BaySickSampleManager` | 38 |
| `VibeRegion` | `BaySickPlayerRegion` | 18 |
| `VibePlayerDSP` | `BaySickPlayerDSP` | 9 |
| `VibePlayerLAF` | `BaySickPlayerLAF` | 6 |
| `VibeForwardMemoryAudioSource` | `BaySickForwardMemoryAudioSource` | 6 |
| `VibeSynthSound` | `BaySickPlayerSound` | 5 |

`VibeSynth` and `VibeSynthSound` are the SAMPLE PLAYER's Synthesiser
(`VibePlayerDSP.h:647` and `:145`), not the synth engine. Renaming them to
`BaySickPlayerSynth` / `BaySickPlayerSound` is the single highest-value line in
this table: today they read as BaySickSynth and are not.

*Group C - old product name on app-level classes:*

| From | To | n |
|---|---|---|
| `VibeSynthProcessor` | `BaySickDAWProcessor` (SC-11) | 376 |
| `VibesynthStandaloneApp` | `BaySickDAWStandaloneApp` | 22 |
| `VibesynthConstants` | `BaySickConstants` | 17 |
| `VibeRackStates` | `BaySickRackStates` | 11 |
| `VibesynthEditor` | `BaySickDAWPluginEditor` | 8 |
| `VibeSynthWindow` | `BaySickDAWWindow` | 7 |
| `VibeDAW` | `BaySickDAW` | 3 |

`VibesynthEditor` (`PluginEditor.h:10`) is the LEGACY plugin-target editor, not
the shipped one (`StandaloneEditor`). `BaySickDAWPluginEditor` says so.

*Group B - machinery:*

| From | To | n |
|---|---|---|
| `VibeGraph` | `BaySickGraph` | 505 |
| `VibeSlider` | `BaySickSlider` | 156 |
| `VibeLAF` | `BaySickLAF` | 64 |
| `VibeThreadPool` | `BaySickThreadPool` | 44 |
| `VibeTooltip` | `BaySickTooltip` | 7 |
| `VibeGraphInsertKindBridge` | `BaySickGraphInsertKindBridge` | 2 |

*Group D - DO NOT TOUCH:* the Tape effect's `Vibe` control. 7 sites, all in
`Source/Standalone/EffectEditorPanels.cpp` (3241, 3251, 3380, 3385, 3413, 3418)
plus the `tape_vibe` / `vibe` parameter ids in `Source/DSP/SaturationDSP.cpp`
(736, 773, 843) and `Source/DSP/EffectParamMap.cpp:315`. This is a real
user-facing knob name and an ordinary English word. A blind `Vibe` -> `BaySick`
sweep renames a knob on the Tape effect and silently breaks its saved parameter
values.

*Other files:* `CMakeLists.txt` (lines 136, 159-161, 177, 451, 639-640, 648),
`Source/Standalone/STANDALONE_UI_CHANGES.md`, and the 6
`Plans & Specs/System Reference/*.md` files that reference these names.

### Task 2 - dead `mPianoRoll` drop
| Path | Lines |
|---|---|
| `Source/Standalone/LayersPage.cpp` | 31-36 (the dead-code comment), 66-67, 81-110 (`buildPianoRollTab`), 162-190, 247-258, 289, 303-306 |
| `Source/Standalone/LayersPage.h` | 70 (`getPianoRoll`), 158 (member), 166 (`buildPianoRollTab` decl), 168 (comment) |
| `Source/Standalone/BassPage.cpp` | 31, 60-61, 75-106, 158-186, 238-248, 279, 293-296 |
| `Source/Standalone/BassPage.h` | 56, 142, 149 |
| `Source/Standalone/DrumPage.cpp` | 127, 167-168, 252-285, 338-360, 404-411, 542, 553-556 |
| `Source/Standalone/DrumPage.h` | 109, 263, 270 |

### Task 3 - dead `BrowserItem::Kind::Audio`
| Path | Lines |
|---|---|
| `Source/Standalone/BuilderPage.cpp` | 178 (label ternary arm), 1760 (rename switch case), 1808, 1833, 1918 |
| `Source/Standalone/BuilderPage.h` | the `Kind` enumerator itself, once the last use is gone |

### Task 4 - MtDiagnostic retirement
| Path | Lines |
|---|---|
| `Source/Engine/RenderEngineFlags.h` | 105-130 (namespace `MtDiagnostic` + `gCaptureOn` + 10 counters + `Snapshot` + `reset()` + `snapshot()`) |
| `Source/Engine/RenderGraphDispatcher.cpp` | 317-318, 349, 354 |
| `Source/Engine/BaySickThreadPool.cpp` (renamed in Task 1) | 138-139, 160, 166, 174, 224, 236-237, 256-257, 267 |
| `Source/Standalone/StandaloneEditor.cpp` | the "Run MT Diagnostic (2s capture)" Mixer hamburger item + its OkCancel handler |

### Task 5 - evidence write-up (docs only)
`Plans & Specs/Running Notes/spry-tidying-pika.md`, plus the batch-close entry.

### Task 6 - security agent (new files)
`.claude/agents/security-auditor.md`, `.claude/commands/audit-security.md`,
`CLAUDE.md` (agent table row).

---

### Task 0 - Land the pending QA-Soundness follow-up commit

The working tree carries 41 dirty paths that belong to QA-Soundness, not to
this batch: the 17-ruling fix pass, the engine gain staging, the NAMIR IR path
fixes, and today's doc reconciliation. Committing them first is what makes this
batch's diff reviewable.

- [ ] `git status --porcelain` and read every line. Confirm nothing unexpected.
- [ ] Stage BY NAME, never `git add -A`.
- [ ] Surface the one-line message + the FULL `git status` to Jeff and WAIT.
- [ ] Commit only on explicit approval.

**No build gate.** The tree is already green from the last QA-Soundness gate;
nothing new is being edited here.

- [ ] `/draft-doc running-notes` -> apply to `Running Notes/spry-tidying-pika.md`.

---

### Task 1 - Full `Vibe*` rename (SC-8, scope c)

151 files, ~1,548 occurrences, 13 file/folder renames. Mechanical, but wide
enough that order matters and one identifier is a trap.

**Do the Tape guard FIRST, before any substitution runs.**

- [ ] Pin the Group D sites so no sweep can reach them. Confirm the exact set
      before starting:

```bash
grep -rnE "\bVibe\b" Source          # expect exactly 7, all EffectEditorPanels.cpp
grep -rn "tape_vibe\|\"vibe\"" Source # SaturationDSP.cpp + EffectParamMap.cpp
```

      Every substitution in this task must be **whole-identifier** anchored
      (`\bVibeGraph\b`, not `Vibe`). A bare `Vibe` -> `BaySick` sweep renames
      the Tape knob and breaks its saved parameter values silently.

**File moves (git mv, so history follows):**

- [ ] `git mv Source/VibePlayer Source/BaySickPlayer`, then `git mv` each of
      the 7 files inside it.
- [ ] `git mv Source/VibeGraph.h Source/BaySickGraph.h` and the `.cpp`.
- [ ] `git mv Source/Engine/VibeThreadPool.h Source/Engine/BaySickThreadPool.h`
      and the `.cpp`.
- [ ] `git mv Source/VibesynthConstants.h Source/BaySickConstants.h`.

**Identifier substitution, in this order** (each step leaves the tree in a
state where the next failure is loud rather than silent):

- [ ] 1. Group A (player), all 11 identifiers per the map. The two that matter
      most:

```cpp
// Source/BaySickPlayer/BaySickPlayerDSP.h
class  BaySickPlayerSynth            // was VibeSynth   -- the PLAYER's Synthesiser
struct BaySickPlayerSound            // was VibeSynthSound
class  BaySickPlayerVoice            // was VibeVoice
struct BaySickPlayerRegion           // was VibeRegion
class  BaySickSampleManager          // was VibeSampleManager
class  BaySickForwardMemoryAudioSource
```

- [ ] 2. Group B (machinery): `VibeGraph`, `VibeSlider`, `VibeLAF`,
      `VibeThreadPool`, `VibeTooltip`, `VibeGraphInsertKindBridge`.
- [ ] 3. Group C (product name). `VibeSynthProcessor` -> `BaySickDAWProcessor`
      per SC-11. Do this one with the whole-identifier anchor and check the
      result does NOT read `BaySickSynthProcessor` anywhere - that class already
      exists and is a different thing.
- [ ] 4. `#include` paths for all 13 moved files, tree-wide.
- [ ] 5. Header banner comments in the moved files, so each names the file it
      is actually in.

**The saved-state string (SC-9):**

- [ ] `"VibeRackStates"` -> `"BaySickRackStates"` at all 7 sites
      (`PluginProcessor.cpp` 6848, 6849, 6897, 7073, 7074, 7322;
      `StandaloneEditor.cpp:14419`). This is a `juce::ValueTree` node name
      written into saved project XML, so existing projects lose rack state
      (mixer, routing, racks, EQ). Locked by SC-9. **No migration shim** per
      `feedback_no_backward_compat_pre_v1`.
- [ ] Also update the two comment references at `PluginProcessor.h:178` / `:224`
      and `StandaloneEditor.cpp:8746` so the XML-shape comments stay true.

**Build files + docs:**

- [ ] `CMakeLists.txt`: source-list entries at 136, 159-161, 177, 639-640, and
      the `target_include_directories` paths at 451 and 648.
- [ ] `Source/Standalone/STANDALONE_UI_CHANGES.md`: references follow.
- [ ] The 6 `Plans & Specs/System Reference/*.md` files: references follow (SC-2).
- [ ] **Do NOT touch** `Implemented Work Log.md`, `Previously Implemented.md`,
      `Running Notes/*`, or any closed batch plan. SC-2 is explicit.

**Verification before the build:**

- [ ] `grep -rnE "\bVibe[A-Za-z]*|\bVibesynth[A-Za-z]*" Source CMakeLists.txt`
      returns exactly the 7 Tape-knob hits and nothing else.
- [ ] `find Source -iname "*vibe*"` returns nothing.
- [ ] **Re-check the parameter-ID claim rather than inheriting it.** The Main
      Plan entry names a `vp_*` prefix as the saved-project risk; no such prefix
      exists (player params are `tk_<trackId>_bsp_`). Run
      `grep -rn "\"vp_" Source` (must be zero) and read the player's parameter-ID
      construction directly before declaring the params clean.
- [ ] **Build gate.** `cmd.exe /c "C:\Users\jeffm\Documents\BaySickDAW\do_build.bat"`
      via PowerShell, `run_in_background: true`, that exact single-statement
      string. Judge ONLY by `build_log.txt`: six exit codes at 0
      (`RELEASE_` / `DEBUG_` / `HELPER64_` / `HELPER32_CONFIG_` / `HELPER32_` /
      `ARTEFACTS_`), four `vcxproj -> ....exe` link lines, zero `error C|LNK|MSB`.

**Tell Jeff:** the rename is source-internal, so almost everything should be
invisible. One thing WILL change and it is expected, not a bug.
1. **Expected loss:** open a project saved before this batch. Its mixer levels,
   routing, effect racks and EQ will come back at defaults, because the saved
   node they live in was renamed and there is no migration shim (your SC-9
   call). Everything else about the project loads normally. Projects saved from
   here on are unaffected.
2. Open a project with a BaySickPlayer instrument on it (Layers, Bass, or a
   Drums tab). The samples, the engine, and the notes should all come back.
3. Load an SFZ into a Drums tab and hit a pad. It should play at its recorded
   pitch, not transposed. (Root-note normalization lives in the renamed sample
   manager.)
4. Play a busy arrangement. `VibeGraph` is the audio routing graph and
   `VibeThreadPool` is the multi-threaded render pool, so if a rename went
   wrong anywhere it shows up as silence or a crash here, not as a subtle bug.
5. Open the Tape effect, switch to Advanced, and confirm the **Vibe** knob is
   still there, still named Vibe, and still at whatever value you left it.

- [ ] Brief one-liner (Rule 9) -> surface message + full `git status` -> WAIT
      for approval -> commit.
- [ ] `/draft-doc running-notes` -> apply.

---

### Task 2 - Drop the dead `mPianoRoll` state from the three engine pages

Locked by SC-10 (Jeff 2026-08-10). Full drop; see SSC-2 for why the minimal
option no longer exists.

The unified `PianoRollPage` has been what the user sees since 2026-04-26. In
`LayersPage`, `BassPage` and `DrumPage` the per-page roll was left as dead code
with a comment saying so. `buildPianoRollTab()` was never wired to a caller, so
`mPianoRoll` is null for the whole life of every one of those pages.

- [ ] Confirm the deadness at execution time rather than trusting this plan:

```bash
grep -rn "buildPianoRollTab" Source          # only RustyDrums has a call site
grep -rn "getPianoRoll()" Source             # zero callers
```

- [ ] Per page (LayersPage, BassPage, DrumPage), delete in this order so each
      step leaves the file compiling:
      1. the `if (mPianoRoll) ...` guards and the blocks they wrap
      2. `buildPianoRollTab()` definition and declaration
      3. `getPianoRoll()` accessor
      4. the `std::unique_ptr<PianoRollContainer> mPianoRoll` member
      5. the now-stale comments that explain why the dead code was kept
- [ ] Drop the `#include` for `PianoRollContainer` in any of the three that no
      longer reference it.
- [ ] **Leave `BaySickRustyDrumsPage` alone.** Its `buildPianoRollTab()` is
      called at `BaySickRustyDrumsPage.cpp:77` and its roll is live.
- [ ] Watch the sub-tab index arithmetic. `LayersPage` gates on
      `mActiveTab == 1` and `DrumPage` on `mActiveTab == 2`; removing the
      guarded blocks must not renumber the remaining pills. The menu-bar Piano
      Roll pill redirects to `PianoRollPage` through the editor's
      `showPageForTab` handler and that path is untouched.
- [ ] **Build gate** (same pinned invocation + same six-code check).

**Tell Jeff:** this only removes code that never ran, but the pages it removes
it from are three you use constantly. Check:
1. Open a Layers tab, a Bass tab and a Drums tab. Each should show the same
   sub-tabs as before, in the same order, with the same one selected by default.
2. On each, click the Piano Roll pill on the menu bar. It should jump to the
   unified Piano Roll page showing that tab's notes, exactly as it does today.
3. Play a pattern with notes on all three. Nothing about playback should change.

- [ ] Brief one-liner -> surface + full `git status` -> WAIT -> commit.
- [ ] `/draft-doc running-notes` -> apply.

---

### Task 3 - Delete the unreachable `BrowserItem::Kind::Audio` paths

Since QA-E Task 4 moved the browser to the library-driven model, no
`BrowserItem` is ever constructed with `Kind::Audio`. Verified: zero
construction sites. The live audio path is the tree
(`showAudioTreeContextMenu`), whose choke value lives on `AudioLibraryEntry`
and is untouched by this.

- [ ] Re-verify before deleting:

```bash
grep -n "Kind::Audio" Source/Standalone/BuilderPage.cpp
# expect: 178 (label), 1760 (rename switch), 1808, 1833, 1918 -- all reads,
# no construction site anywhere
```

- [ ] **Pre-delete guard, source-verified and still true:** `renameAudioAt`
      (`BuilderPage.cpp:1117`) is SHARED. It is called from the LIVE tree path
      (`BuilderPage.cpp:411`) as well as from the dead flat-list case at 1760.
      Delete the call site at 1760, **keep `renameAudioAt` itself.**
- [ ] Delete the dead arms at 178, 1760, 1808, 1833, 1918.
- [ ] Only once every use is gone, remove the `Audio` enumerator from
      `BrowserItem::Kind`. If any use remains, leave the enumerator and say so
      in the running notes rather than forcing it.
- [ ] **Build gate.**

**Tell Jeff:** the browser's audio right-click menu is the thing to poke.
1. Right-click an audio file in the browser tree. The menu should have every
   item it has today, including the choke-group entry.
2. Rename an audio file from that menu. The rename should stick and the tree
   should refresh.
3. Drag an audio file to the arrangement. It should create a clip as usual.

- [ ] Brief one-liner -> surface + full `git status` -> WAIT -> commit.
- [ ] `/draft-doc running-notes` -> apply.

---

### Task 4 - Retire the MT Diagnostic instrumentation

QA-Md closed 2026-05-09 with a no-bug-found result: the MT engine works
correctly in Debug. The diagnostic counters and their menu item are leftover
instrumentation with an expired purpose, and `gCaptureOn` is a relaxed atomic
load on the per-block audio hot path plus one per task in the thread pool.

- [ ] Delete the Mixer hamburger item "Run MT Diagnostic (2s capture)" and its
      OkCancel-prompt handler in `Source/Standalone/StandaloneEditor.cpp`.
- [ ] Delete every `gCaptureOn`-gated `fetch_add` site:

```cpp
// Source/Engine/RenderGraphDispatcher.cpp:317-318, 349, 354
// Source/Engine/VibeThreadPool.cpp:138-139, 160, 166, 174, 224,
//                                 236-237, 256-257, 267
// Pattern to remove, e.g.:
if (RenderEngine::MtDiagnostic::gCaptureOn.load (std::memory_order_relaxed))
    RenderEngine::MtDiagnostic::gBlockCount.fetch_add (1, std::memory_order_relaxed);
```

- [ ] Where a local `const bool capture = ...gCaptureOn.load(...)` exists
      (`RenderGraphDispatcher.cpp:349`, `VibeThreadPool.cpp:160` and `:224`),
      remove the local too, and check whether removing it leaves an empty `if`
      or an unused variable.
- [ ] Delete the whole `RenderEngine::MtDiagnostic` namespace from
      `Source/Engine/RenderEngineFlags.h` (`gCaptureOn`, the 10 counters,
      `Snapshot`, `reset()`, `snapshot()`) and the comment block above it.
- [ ] Confirm nothing survives: `grep -rn "MtDiagnostic\|gCaptureOn" Source`
      returns zero.
- [ ] Task 1 already renamed this file to `Source/Engine/BaySickThreadPool.cpp`
      (SC-8). Work in the renamed file; do not reintroduce the old path.
- [ ] **Build gate.**

**Tell Jeff:** this strips diagnostic plumbing out of the multi-threaded render
path, which is the audio engine's hot path, so it is worth a real listen.
1. Play a busy arrangement (several Layers/Bass/Drums tabs plus audio clips).
   It should sound identical, with no dropouts or clicks.
2. Watch the DSP% readout while it plays. It should be the same or slightly
   lower than before, never higher.
3. Open the Mixer hamburger menu. Everything except "Run MT Diagnostic"
   should still be there.

- [ ] Brief one-liner -> surface + full `git status` -> WAIT -> commit.
- [ ] `/draft-doc running-notes` -> apply.

---

### Task 5 - Write down the Cleanup-2 / Cleanup-3 / Cleanup-4 evidence

No deletions (SC-5). The deliverable is the record of WHY, because every one of
these reads as removable to a casual grep and is not.

- [ ] Re-run the vendored-library sweep and record, per library, the thing that
      keeps it. The three that read as unreferenced and are all load-bearing:
      - `asiosdk` - no `#include` names it; CMake auto-detects the folder and
        sets `JUCE_ASIO=1`. Deleting it silently drops ASIO support.
      - `NeuralAmpModelerCore` - reached through an include-path include
        (`#include <NAM/get_dsp.h>`), so a grep for the folder name misses it.
      - `signalsmith-linear` - not included by our code at all; it is a
        FetchContent dependency of `signalsmith-stretch`.
- [ ] Record Jeff's standing rule alongside it: remove whole unused folders,
      never prune inside a library we use. Upstream updates fight the
      deletions (the sfizz precedent).
- [ ] Re-run the asset/preset sweep and record the two traps:
      - **Constructed filenames.** `loadCassetteIR` builds
        `"cassette tape_" + String (i + 1) + ".wav"` at runtime. All 20 tape
        files plus the acoustic IRs are invisible to a filename grep, and
        deleting them kills Tape mode SILENTLY through an `existsAsFile()`
        fall-through. Same shape for anything else path-composed.
      - **`Presets/BaySickDrums/`** looks orphaned because the monolithic
        `BaySickDrums` engine class was deleted at Phase D. It is the live
        preset home for the Drums pages (`DrumPage.cpp:100-101`).
- [ ] Record the factory-preset caveat: the preset XML is generated by
      `gen_factory_presets.py`, so there is no hand-maintained list to diff a
      preset folder against. Any future preset audit has to read the generator.
- [ ] Record Cleanup-4 as already done: `.gitignore:8` covers
      `Files For Claude`; `git log --all -- "Files For Claude"` is empty, so
      none of its 738 MB was ever tracked and there is no history to purge.
- [ ] Apply all of the above to the running notes as a single dated evidence
      block, so the batch-close entry can quote it.

**No build gate.** No source touched.

**No "Tell Jeff" verify.** Nothing changed in the app.

- [ ] `/draft-doc running-notes` -> apply.

---

### Task 6 - Build the `/audit-security` agent

Locked by SC-6. Net-new capability. QA-Soundness audited correctness: does the
code do what it means to do. Nothing has ever audited what happens when the
input is hostile or malformed, and this app opens files it did not write.

- [ ] Write `.claude/agents/security-auditor.md`, following the house shape
      (see `.claude/agents/performance-auditor.md` for frontmatter + section
      structure):

```markdown
---
name: security-auditor
description: Audits BaySickDAW's handling of untrusted input - project files, sample / SFZ / IR / NAM files, hosted third-party plugin binaries, and the Core Library fetcher's network path. Read-only. Context-aware: distinguishes a hostile-input surface from ordinary internal code. Run pre-release or when a new input surface lands.
tools: Read, Grep, Glob, Bash
---
```

- [ ] Give it these audit categories, in this order (they are ranked by what
      this app actually exposes):
      1. **Project / preset / template XML** - the parser trusts a file the
         user may have received from someone else. Unbounded counts, index
         values used without range checks, sizes read from the file and used
         to allocate.
      2. **Media file loading** - WAV / SFZ / IR / NAM. Malformed headers,
         sample counts that do not match the data chunk, SFZ include paths
         that escape the sample folder.
      3. **Path handling** - anything that builds a `juce::File` from text
         that came out of a file. Directory traversal through `../`, absolute
         paths in a "relative" ref, the `library:` / `mysamples:` stable-ref
         resolvers.
      4. **Hosted plugin binaries** - third-party VST3 loaded in-process and
         through the sandbox helpers. What crosses the bridge, and what a
         wedged or hostile helper can do to the host.
      5. **Network** - the Core Library fetcher. HTTPS enforcement, redirect
         handling, what gets written where after a download, and whether a
         served archive can write outside the intended folder.
      6. **Secrets and logging** - anything written to `build_log.txt`,
         settings XML, or a crash path that should not be there.
- [ ] Give it the context rules that keep it useful instead of noisy. Without
      these it will report every `strcpy`-shaped thing in vendored code and
      bury the real findings:
      - Vendored libraries under `libs/` are reported SEPARATELY from our code
        and never mixed into the main findings list.
      - A missing bounds check on data the app itself just wrote is not a
        finding. The question is always "could this have come from a file
        someone else made".
      - Real-time-safety issues are the performance auditor's job, not this
        agent's.
      - Findings are ranked HIGH / MEDIUM / LOW with an explicit exploit path
        for anything HIGH. "Could theoretically overflow" is not a finding
        without the path.
      - Tier 1 = our source under `Source/`. Tier 2 = vendored libraries.
        Tier 1 is what this batch runs.
- [ ] Drafter pattern, matching every other agent here: the agent RETURNS the
      report text; the parent session writes it to
      `Plans & Specs/Research Reports/security-audit-2026-08-10.md`. It never
      edits source and never edits `Plans & Specs/` itself.
- [ ] Write `.claude/commands/audit-security.md` in the house shape, including
      the "Distinct from" block that separates it from `/perf-audit`,
      `/audit-licenses` and `/review-batch`.
- [ ] Add the row to the CLAUDE.md agent table:

```markdown
| `/audit-security` | `security-auditor` | Audit handling of untrusted input (project files, media files, hosted plugin binaries, the Core Library fetcher). Read-only. Pre-release, or when a new input surface lands. |
```

- [ ] Add it to the Main Plan section 0 Agent Orchestration Rules with its
      cadence (pre-release, or when a new input surface lands).

**No build gate.** No source touched.

- [ ] `/draft-doc running-notes` -> apply.

---

### Task 7 - Run the Tier-1 security audit and triage

- [ ] Dispatch `/audit-security` at Tier 1 (our source only). ONE agent, not
      several in parallel.
- [ ] Write the returned report to
      `Plans & Specs/Research Reports/security-audit-2026-08-10.md`.
- [ ] **Verify each finding's premise before relaying it.** An agent finding is
      a lead, not a fact. For every HIGH, read the cited code and confirm the
      exploit path is real. Report only what survives that check, and say how
      many did not.
- [ ] **Tell Jeff:** surface the surviving findings as numbered prose with
      severity, what an attacker would have to do, and a rough fix effort for
      each. Then WAIT. Per SSC-3 the default is fix-in-batch, but the call on
      anything large is his.
- [ ] Execute whatever he rules in. Build gate after any source change.
- [ ] `/draft-doc running-notes` -> apply.

---

## Verification (end-to-end smoke)

**There is no end-to-end smoke in this batch.** SC-7: all functional
verification belongs to the Master Test Plan campaign, which Jeff runs next and
which now also carries the clean-slate build as test G-5.

What this batch owes the campaign instead:

- [ ] Every task's build gate green, per the six-exit-code rule.
- [ ] The per-task "Tell Jeff" checks above, run at Jeff's convenience rather
      than as a gate.
- [ ] Confirm the campaign's affected sections are marked for a re-run given
      what this batch touched: the browser audio right-click path (Task 3), the
      three engine pages' sub-tabs (Task 2), the MT render path (Task 4), and
      any BaySickPlayer-backed tab (Task 1).

---

## Routing notes (Rule 3 application during execution)

- **Findings inside this batch's own scope** get fixed here, per
  `feedback_qa_batches_fix_bugs_dont_defer`. This is the last coding batch;
  there is no later batch to route a bug to.
- **Dead code this batch itself creates** gets cleaned here, not routed.
- **Security findings** route per SSC-3: triaged with Jeff, then fixed here
  unless he rules otherwise.
- **Anything genuinely post-v1** goes to `Future State.md` as a new entry with
  a real ID, surfaced to Jeff before it is written.
- **A finding that belongs to a closed batch** gets fixed here with a Main Plan
  section 9 Forks back-reference, per
  `feedback_closed_batch_carryforward_via_forks`. Closed commits are never
  reopened.

---

## Carry-Forward Reference touch points

| Task | Read before starting |
|---|---|
| Task 1 | Carry-Forward: the engine-ownership section (EngineRig owns dynamic-tab engines; pages are non-owning views) and the parameter-ID conventions. CLAUDE.md "Drum sample root note" - the normalization lives in the class being renamed. |
| Task 2 | Carry-Forward: the page/window architecture section. CLAUDE.md "Contained-window shell" for why page members are unguarded on close. |
| Task 3 | Carry-Forward: the browser/library model section (QA-E's library-driven rewrite is what made these paths dead). |
| Task 4 | Carry-Forward: the render-engine section, MT dispatch. CLAUDE.md audio-thread fast-path notes. |
| Task 5 | No Carry-Forward dependency. Read `.gitignore` and the vendored `CMakeLists.txt` files directly. |
| Task 6 | Carry-Forward: the project-persistence section and the hosting/sandbox section, so the agent's categories match what the app actually exposes. |
| Task 7 | Whatever the findings point at. |
