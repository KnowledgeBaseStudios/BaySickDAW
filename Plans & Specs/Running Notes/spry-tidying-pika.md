# Running Notes - QA-Cleanup (spry-tidying-pika)

> Append-only working log for QA-Cleanup, the last batch of G4 and the last
> coding batch of the bulk run. A new dated entry lands at EVERY checkpoint:
> a commit landing, a sub-task verifying, a finding surfacing, a spec call
> resolving, a scope pivot. Never batched up and written at the end. At batch
> close this file is the source the Implemented Work Log entry is compiled
> from, so anything not written here is lost.

**Pair file:** `Plans & Specs/Batch Plans/spry-tidying-pika.md`
**Convention:** Main Plan section 0, "Batch Plans + Running Notes layout"
(locked 2026-05-11). Reference exemplar:
`Plans & Specs/Batch Plans/federated-bouncing-cupcake.md` (QA-D).

---

## 2026-08-10 - Batch open - Phase 6 collapsed to one batch

Jeff's call, verbatim: *"Ok let's do the cleanup batch then, write the plan
which will be the last batch in G4. We are going to wipe G5 completely out with
the cleanup work, and will move the build test into the test campaign. Then we
have G6 which will now be G5 and while I start working on the campaign I will
have you start working on the new G5."* Follow-up ruling on the security agent:
pick **(b)**, build `/audit-security` IN the cleanup batch.

Doc reconciliation done before the plan was written, as instructed:

- **`Main Plan.md` section 6** - the seven-batch Phase 6 arrow replaced with a
  single `QA-Cleanup` line plus a SUPERSEDED note giving the per-batch
  rationale for the collapse. Former shape preserved in the note.
- **`Main Plan.md` section 5** - new `QA-Cleanup: Phase 6 in one batch` entry
  inserted at the head of the Phase 6 entries. SUPERSEDED banners added to
  `QA-Cleanup-1`, `QA-PlayerRename`, `QA-Cleanup-2`, `QA-Cleanup-3`,
  `QA-Cleanup-4` and `QA-Audit`; DISSOLVED banner added to `QA-RC`. All seven
  entries RETAINED for their item detail, which the cleanup batch executes.
- **`Main Plan.md`** - Phase 6 section header gained a collapse note; the
  Phase 7 arrow no longer says "runs ONLY after QA-RC"; the QA-RC row in the
  batch-index table struck through with its dissolution reason; "G1-G6"
  corrected to "G1-G5".
- **`v1-master-test-plan.md`** - section G soak note no longer schedules
  against a group that does not exist, and **test G-5, clean-slate build**,
  added per Jeff's "move the build test into the test campaign".
- **`swift-stampeding-caribou.md`** - G5 marked DISSOLVED with per-batch
  rationale; G6 renumbered to G5; run-success criteria repointed at the
  campaign's clean-slate build; `/audit-security` re-routed to this batch;
  G4 composition note gained QA-Cleanup as batch 10.
- **`grand-inverting-mammoth.md:1541`** and **`loud-bouncing-walrus.md:189`** -
  stray G5/G6 references corrected.

### Findings from the pre-plan source sweep

Three things came out of checking the fold-ins rather than inheriting them:

1. **The `mPianoRoll` fold-in is 5x its Main Plan estimate.** The entry offers
   "(i) delete the setTabName writeback lines" or "(ii) full member drop".
   Option (i) is no longer on the table: `buildPianoRollTab()` has no caller in
   `LayersPage` / `BassPage` / `DrumPage`, so `mPianoRoll` is null for the
   entire life of all three pages, `getPianoRoll()` has zero callers app-wide,
   and every `if (mPianoRoll)` guard is a permanent no-op. That is ~25 dead
   lines per page, not 5. Raised as SSC-2.
2. **The `ProjectBrowserWindow` fold-in is already done.** The unreachable
   block after the early `return;` in
   `StandaloneEditor::doFileSetDefaultTemplate()` is gone; the method now ends
   at the FileChooser launch. Consistent with C4702 reading 0 in the current
   `build_log.txt`. Removed from the task list, recorded here as evidence.
   (`ProjectBrowserWindow` itself is still live at
   `StandaloneEditor.cpp:14506` and stays.)
3. **The `Kind::Audio` deadness confirmed by construction-site search, not by
   the plan entry.** Zero `BrowserItem` construction sites use `Kind::Audio`;
   the five surviving references at `BuilderPage.cpp` 178 / 1760 / 1808 / 1833
   / 1918 are all reads. The pre-delete guard on the SHARED `renameAudioAt`
   still holds and is carried into the task.

### Open sub-spec calls at plan close

- **SSC-1** - rename scope. First version was wrong and Jeff caught it: it
  listed every string containing "Vibe" without asking whether each was
  actually confusing. Rewritten around the real test - does the name CONTRADICT
  something that has a different name - into four groups. Two facts were wrong
  in the first pass and are corrected in the plan:
  - `VibeSynth` / `VibeVoice` / `VibeSynthSound` were listed as app-wide. They
    live inside `Source/VibePlayer/VibePlayerDSP.h` (`:647`, `:145`) and are the
    SAMPLE PLAYER's Synthesiser. That makes them worse than stale, not better:
    a reader looking for BaySickSynth lands on the player. They come along with
    the player rename either way.
  - `VibeSynthProcessor` (376) is the MAIN app processor
    (`PluginProcessor.h:128`), not an engine. It is the single most misleading
    name in the tree, because `BaySickSynthProcessor` also exists and the two
    read as siblings when one is the whole app.
  - `VibeSlider` / `VibeGraph` / `VibeLAF` / `VibeThreadPool` / `VibeTooltip`
    have no counterpart name anywhere. Jeff's read is right on these: stale
    prefix, no confusion, cosmetic.
  - The Tape effect's **Vibe** control (`SaturationDSP.cpp` `tape_vibe`) is a
    real user-facing control name and an ordinary English word. Not in scope.

- **SSC-1 side finding - the real persistence exposure.** The Main Plan entry
  names a `vp_*` parameter prefix as the saved-project risk. That prefix does
  not exist (player params are `tk_<trackId>_bsp_`), so the player rename is
  clean. But `"VibeRackStates"` (`PluginProcessor.cpp` 6848 / 6849 / 6897 /
  7073 / 7074 / 7322, `StandaloneEditor.cpp:14419`) is a **ValueTree node name
  written into saved project XML**. Renaming that string loses rack state on
  every existing project. Pre-v1 rule says no migration shims, so that outcome
  is acceptable, but it is a call and it belongs to whichever option includes
  Group C.
- **SSC-2** - `mPianoRoll` full drop, per the finding above.
- **SSC-3** - what happens to a HIGH security finding from Task 7.

## 2026-08-10 - Spec calls resolved - rename widened to the whole tree

Jeff ruled on all three open sub-spec calls.

- **SSC-1 -> (c)**, locked as SC-8: *"do C then for rename just so things are as
  clean for the user as possible."* Every `Vibe*` / `Vibesynth*` identifier
  renames EXCEPT the Tape effect's `Vibe` control. Measured scope after the
  ruling: **151 files, ~1,548 occurrences, 13 file/folder renames**
  (`Source/VibePlayer/` folder + its 7 files, `VibeGraph.h/.cpp`,
  `Engine/VibeThreadPool.h/.cpp`, `VibesynthConstants.h`). Full identifier map
  written into Task 1.
- **SSC-2 -> (a)**, locked as SC-10: *"yes do the mpianoroll drop."* Full member
  drop across LayersPage / BassPage / DrumPage.
- **SSC-3 -> (a)**: security findings get triaged with Jeff before any fix is
  written.

**SC-9 recorded as a consequence, not a new call.** `"VibeRackStates"` renames
with everything else, so projects saved before this batch lose their rack state
(mixer, routing, racks, EQ). The consequence was stated in the option Jeff
picked. No migration shim, per `feedback_no_backward_compat_pre_v1`.

**One trap written into the task.** All 7 bare `Vibe` occurrences in the tree
are the Tape effect's knob (`EffectEditorPanels.cpp` 3241 / 3251 / 3380 / 3385 /
3413 / 3418, plus `tape_vibe` / `vibe` param ids in `SaturationDSP.cpp` and
`EffectParamMap.cpp:315`). That is a real user-facing control name and an
ordinary English word. Every substitution in Task 1 is whole-identifier anchored
(`\bVibeGraph\b`, never bare `Vibe`) specifically so a sweep cannot rename a
knob and silently break its saved parameter values.

**SSC-1a -> (a)**, locked as SC-11. `VibeSynthProcessor` (376 occurrences, the
main app processor at `PluginProcessor.h:128`) becomes **`BaySickDAWProcessor`**.
It was the one name the mechanical `Vibe` -> `BaySick` substitution could not
handle, because `BaySickSynthProcessor` is already the BaySickSynth engine's
processor and that collision is the reason the name was worth fixing at all.
All spec calls for this batch are now closed.

## 2026-08-10 - Task 0 - Pending QA-Soundness commit landed

Commit `485499ae`, 43 files, tree clean afterwards. Contents: the 17-ruling fix
pass, engine gain staging (Synth/Bass -12 dB, Harmless -6 dB, sqrt-N unison
normalization), the NAMIR IR path-resolution fixes, the Phase 6 doc collapse,
and this batch's plan + notes pair. Staged by name, not `git add -A`.

One mechanical note for next time: `Assets/big_rusty_drums.svg` was already
staged as a deletion, and `git add` on a deleted path fails with
"pathspec did not match any files" because the file is gone from the working
tree. Already-staged deletions need no re-add.

## 2026-08-10 - Task 1 - Full `Vibe*` rename executed

**13 file/folder moves, all via `git mv` so history follows:**
`Source/VibePlayer/` -> `Source/BaySickPlayer/` plus its 7 files,
`VibeGraph.h/.cpp` -> `BaySickGraph.h/.cpp`,
`Engine/VibeThreadPool.h/.cpp` -> `Engine/BaySickThreadPool.h/.cpp`,
`VibesynthConstants.h` -> `BaySickConstants.h`.

**Substitution: 169 files changed, 1,711 identifiers replaced.** Higher than
the ~1,548 pre-flight estimate because the estimate counted `Source/` +
`CMakeLists.txt` only; the 20 System Reference docs carried the rest.

**Method - whole-identifier anchoring, and why it mattered.** Every one of the
24 identifiers was replaced through a `\b<name>\b` regex, never a bare `Vibe`
substring. Two reasons, both load-bearing:

1. **The Tape knob.** All 7 bare `Vibe` occurrences in the tree are the Tape
   effect's user-facing **Vibe** control. A substring sweep would have renamed
   the knob AND left `mTapeVibe` / `tape_vibe` / `"vibe"` half-renamed, breaking
   its saved parameter values silently.
2. **Prefix collisions.** `\bVibePlayer\b` correctly does NOT match inside
   `VibePlayerProcessor`, and `\bVibeGraph\b` does not match inside
   `VibeGraphInsertKindBridge`, so the map needed no ordering discipline.

**Verified post-substitution:**
- `grep -rnE "\bVibe[A-Za-z]*\b|\bVibesynth[A-Za-z]*\b" Source CMakeLists.txt`
  returns exactly the 7 Tape-knob hits and nothing else.
- `find Source -iname "*vibe*"` returns nothing.
- The System Reference residual is 2 hits, both legitimate references to the
  Tape Vibe knob (`Effect Modules.md:298`, `MANUAL-1 Screenshot List.md:1682`).
- No `BaySickDAWProcessor` / `BaySickSynthProcessor` collision: the app
  processor is `PluginProcessor.h:128`, the engine is
  `BaySickSynthProcessor.h:16`, and 20 forward declarations resolved to the
  correct one.
- `"BaySickRackStates"` landed at all 7 sites (SC-9), and the three XML-shape
  comments that describe the node were updated with it.

**Historical docs untouched**, per SC-2: Implemented Work Log, Previously
Implemented, Running Notes for closed batches, and closed Batch Plans all keep
the names they were written with.

**Anchoring proof, unplanned but useful.** 20 System Reference files matched the
`Vibe*` grep; only 18 changed. The two that did not
(`Effect Modules.md`, `MANUAL-1 Screenshot List.md`) contain ONLY the Tape
effect's `Vibe` knob. That is the anchoring working exactly as intended, with no
manual exclusion list needed.

**Build gate: GREEN.** Six exit codes at 0, four correct link lines
(Release + Debug `BaySickDAW.exe`, `BaySickPluginHost64.exe`,
`BaySickPluginHost32.exe`), zero `error C|LNK|MSB`.

### FINDING - the "warnings are already at zero" claim was wrong

The batch plan's Context table originally said the QA-Cleanup-1 warning sweep
"already reads 0 for C4702 / C4189 / C4505." That reading came from an
**incremental** `build_log.txt` in which those translation units were never
recompiled, so their warnings simply were not in the log.

The rename touched effectively every header, which forced a genuine full
rebuild, and four real sites appeared:

| Warning | Site |
|---|---|
| C4702 unreachable code | `Source/BaySickPlayer/BaySickPlayerEditor.cpp:623` |
| C4702 unreachable code | `Source/SampleLibrary.cpp:106` |
| C4189 unused local `cx` | `Source/BaySickSynth/BaySickVisualizerScreen.cpp:144` |
| C4189 unused local `hw` | `Source/BaySickSynth/BaySickVisualizerScreen.cpp:146` |

C4505 is genuinely 0. None of the four is caused by the rename - a
whole-identifier substitution cannot create unreachable code or orphan a local.
They were always there and the incremental log was hiding them.

Plan corrected in place (Context table now states the incremental-log error) and
the four sites added as **Task 4b**. Lesson worth keeping: a warning count from
an incremental build is not evidence of absence, only of not-recompiled.

### State at open

41 dirty paths in the working tree, all QA-Soundness follow-up work (the
17-ruling fix pass, engine gain staging, NAMIR IR path fixes) plus today's doc
reconciliation. Task 0 lands that commit first so this batch's diff is its own.
Last build gate green: six exit codes 0, four link lines, zero errors.
