# Running Notes — QA-ClipDrop (fancy-kindling-dongarra)

> Append-only running log for the QA-ClipDrop batch.  A new dated `## YYYY-MM-DD
> — Task N — <name>` entry is appended at every checkpoint (commit landed /
> sub-task verified / finding captured / spec call resolved / scope pivot) per
> `feedback_draft_doc_running_notes_every_checkpoint.md`.  At batch close,
> `/draft-doc batch-close` consumes this file as the primary input for the single
> Implemented Work Log entry.  Never edit prior entries — new findings get a new
> dated entry.

**Pair file:** `Plans & Specs/Batch Plans/fancy-kindling-dongarra.md` (the plan).
**Conventions:** Main Plan §0 Document Formatting Conventions + the Batch
Plans / Running Notes required-sections rule (locked 2026-05-11; exemplar
`federated-bouncing-cupcake.md`).

---

## 2026-06-02 — Task 0 — Batch open

- QA-ClipDrop opened as a **long-running diagnostic batch** (Jeff's call,
  2026-06-02).  The audio-clip drop bug is **intermittent / session-state-dependent**
  — it cleared on a fresh app restart and would not reproduce on demand, so per
  `feedback_diagnose_before_fixing.md` we **instrument-and-catch** rather than ship
  a blind fix.
- Diagnosis (collaborative, 2026-06-02): drag-from-Explorer symptoms did NOT
  reproduce (3 targeted scenarios + the original pattern-and-clip test rig all
  passed); the reliable repro was the Clips ribbon `+ Add New Clip` picker silently
  no-op'ing (no page / strip / browser entry).  Source trace: `+ Add New Clip`
  (StandaloneEditor.cpp:3704) and drag-drop both converge on
  `ArrangementGrid::importAudioFile` (BuilderPage.cpp:3350); the missing browser
  entry proves the drop bails BEFORE `addAudioToLibrary` (:3415) — at the file-missing
  (:3365) or copy-on-drop-empty (:3402) early-return.  `importSample`
  (ProjectManager.cpp:484) only returns empty (project open) on `copyFileTo` failure
  → the project `Samples/` folder state goes bad mid-session; a restart rebuilds
  `mCurrentFolder`.
- Reframe: very likely ONE intermittent session-state failure (copy-on-drop poisoned
  by mid-session project lifecycle churn during the QA-Ed test-rig setup), not the 6
  separate deterministic symptoms the §5 entry first listed.
- Framing correction (SC-B): the §5 "pre-existing" wording was wrong — corrected this
  task to "regression that recurred / intermittent / session-state-dependent."
- Spec calls locked this open: SC-A slot (after QA-Ed, before QA-Ee) · SC-B framing ·
  SC-C instrument-and-catch · SC-D diag form = AlertWindow popup + append-to-log in
  `Documents/BaySickDAW/`, Debug + Release · SC-E coverage = full drop cascade ·
  SC-F lifecycle = held open until evidence or close-as-not-reproduced by end of QA.
- Deferred sub-spec calls: DS-1 (§5.5 Domain Coverage backfill placement — QA-ClipDrop
  + QA-Ed + QA-TempoMap missing) · DS-2 (probe strip/keep if not reproduced) · DS-3
  (fix scope at evidence-time) · DS-4 (end-of-QA trigger).
- Reuse found for Task 1: `namirLog()` / `pedalsLog()` one-off file-logger convention
  (append to `Documents/BaySickDAW/*.txt`, Debug + Release); `ProjectManager::getSettingsFile()`
  canonical Documents/BaySickDAW resolver.
- **Task 0 committed:** `122ff3f` (docs only — Main Plan §5 reframe + Plan-file pointer +
  STATUS/Risk/Effort fill; plan file + this Running Notes seed).  Working tree clean.

## 2026-06-02 — Task 1 — Diagnostic trap implemented (awaiting build + arm-verify)

- Added `Source/ClipDropDiag.h` — shared helper: `log()` (append to
  `Documents/BaySickDAW/clipdrop_diag_log.txt`, timestamped, Debug + Release) +
  `alert()` (log + `AlertWindow::showMessageBoxAsync` popup).  Mirrors
  `namirLog()` / `pedalsLog()`.  `#include <JuceHeader.h>` (project umbrella).
- Full-cascade probes (SC-E) across the convergence path:
  - `importSample` (ProjectManager.cpp) — logs every return reason incl. the
    `copyFileTo` bail with samplesDir/target/dir-exists.  **The key WHY probe.**
  - `importAudioFile` (BuilderPage.cpp) — enter; `alert()` on the file-missing +
    copy-on-drop-empty bails; library-add; onAudioClipAdded-fired/DONE.
  - `filesDropped` (BuilderPage.cpp) — drag entry + duplicate-vs-import branch.
  - `onAddTabRequest` Clip branch (StandaloneEditor.cpp) — `+ Add New Clip` pick;
    `alert()` when it produces no NEW library entry (libCount unchanged).
  - `onAudioClipAdded` + `spawnClipsTabIfMissing` (StandaloneEditor.cpp) —
    page/strip-spawn outcome (bail / dedup / row-taken / create).
- Popup-wiring verifies on demand benignly: `+ Add New Clip` -> pick a file already
  in the library -> dedup -> "no new library entry" popup fires (log shows it was a
  dedup, NOT a bail).  No throwaway forced-alert line / extra build cycle needed.
- 4 source files touched (1 new + 3 instrumented), no behavior change (append-only
  logging + popup-on-anomaly).  Awaiting Jeff's Debug + Release arm-verify before
  the Task 1 source commit.

## 2026-06-02 — Task 1 verify + diagnosis: trap caught the bug; SC-G..SC-J locked

**Trap armed + verified** (Jeff, 2026-06-02): popups fired + the log wrote in Debug. Arm-verify
effectively passed via live capture.

**What the trap caught (log `clipdrop_diag_log.txt`):**
- The "+ Add New Clip does nothing" failure THIS session = **no project open** (log line 49:
  `importAudioFile BAIL: copy-on-drop returned empty` with **no `importSample` line before it** →
  `onImportSampleRequest` returned empty from its `hasProject()==false` branch). The New-Project
  prompt fires + the retry succeeds (lines 51-56).
- Jeff surfaced a **deterministic regression cluster**: "+ Add New Clip" routes through
  `importAudioFile(f, row 0)` → drops a grid block + names the strip after the Builder row
  ("Track 1") + (post-QA-E) pins routing to that row → move-breaks-playback + stray "Track 1"
  strips. In TESTIES (row 0 page already existed) → `spawnClips ROW-TAKEN`: no new page but a
  stray block + "Track 1" strip (log lines 61-68).

**Root causes (git-traced):**
- Row-coupling (move-breaks-playback) = **QA-E Task 5 `6b044aa`** retag (`blk.routeChannel =
  audioInsert(row)`), built on **Task 4 `1d928fc`** (routeChannel param). The commit's
  "functionally a no-op for playback" assumption is the bug — only true while the block never moves.
- "+ Add New Clip" → grid routing = G-6 mega-commit `16037a4` (2026-04-30); strip naming "Track N"
  = `mRowNames` default label (BuilderPage.cpp:1275) fed to `onAudioClipAdded`. Both trace into the
  **2026-04-28 git re-baseline** (`d595ee3`, 203 commits, joke-named early commits) — the exact
  pre-re-baseline flip is not bisectable; mechanism identified regardless. (Own-the-codebase: cause
  found in current code, not deflected to the re-baseline.)

**Spec calls locked (Jeff, 2026-06-02):**
- **SC-G = (a):** "+ Add New Clip" → browser listing + Clips page + strip, **no grid block**.
- **SC-H = (c):** clip strip name follows the **Clips page/tab name** (synced like Layers/Bass/Drums).
- **SC-I = (a), emphatic:** **NOTHING auto-attaches to any Builder grid row**; clip audio routes by
  its own Clips page, never pinned to a row; applies to BOTH drag-drop and "+ Add New Clip".
- **SC-J = (a):** keep the New-Project prompt for the no-project case.

**Lifecycle (Jeff confirmed):** fix the deterministic bugs (SC-G..SC-J) in-batch as **Task 2**, AND
keep the batch **held open + trap in** to catch the SECOND case — the original failure was in a
**SAVED project** (Jeff, 2026-06-02), so there's a copy-fail-WITH-project mode the trap hasn't caught
yet (only the no-project skip seen). The shared `importSample` `copyFileTo FAILED` probe will catch
it. Close when the 2nd case is caught+fixed OR end-of-QA (not-reproduced).
- **Vox/Inst confirmed UNAFFECTED** (Jeff's scoping Q, 2026-06-02): Vox/Inst WAVs route by
  `routeChannel` (Vox/Inst range) via `renderFilePlayPlayer` (PluginProcessor.cpp:623),
  independent of the grid row — they already do what we're fixing clips to do. The fix touches
  the clips path ONLY. Also verified: clips carrying an Audio-range `routeChannel` (400-449)
  won't collide with the Vox/Inst FilePlay path (disjoint ranges 400/600/700).
- **Plan expanded with the deterministic fix (Tasks 2/3, code in plan)** per Jeff's "proceed"
  (2026-06-02): SC-G..J + render-by-owner (PluginProcessor.cpp:405) + load migration
  (PatternManager.cpp:1452) + no-block "+ Add New Clip" + strip naming/rename. Held open for the
  2nd case (Task 4). Next: commit the trap, then implement Task 2.
- **Provenance (honest):** the grid-row coupling is present in the earliest visible commit (the
  2026-04-28 re-baseline `d595ee3`); the trackRow filter line was authored at `cc011e0` (MT-engine,
  2026-05-06) but only re-expressed pre-existing coupling. The independent-clip era Jeff remembers
  predates the re-baseline (squashed) — un-bisectable, stated plainly, not used to deflect.

## 2026-06-02 — Task 2 — SC-I routing decouple (code landed, awaiting build + verify)

- **Render by owner, not trackRow** (`PluginProcessor.cpp` `renderAudioClipsForRow`): removed
  `if (player.trackRow != row) continue;`; after the Vox/Inst skip, compute `ownerRow` from
  `routeChannel` (audioInsert range 400-449 → owner page row; legacy/0 → `trackRow` fallback) and
  `if (ownerRow != row) continue;`. A clip now renders into its owning Clips-page strip regardless
  of grid row → moving a block never breaks playback. `row` is now the owner row, so the existing
  mute checks (`audioRowMute[row]` / `isRowAudible(row)`) key on the clip's own strip automatically;
  unmoved clips (the common case) are byte-identical.
- **Owner stamped at creation — NO code change:** the post-Task-5 retag (`onAudioClipAdded`) already
  stamps `blk.routeChannel = audioInsert(row)`; `placeAudioLibraryEntry` stamps `routeChannel = owner`.
  These become functional (were cosmetic). Block moves leave `routeChannel` untouched → owner preserved.
- **Old-project load migration** (`PluginProcessor.cpp` `rebuildAudioClipPlayers`): for Audio clips
  with `routeChannel==0` (pre-retag saves), stamp `blk.routeChannel = audioInsert(trackRow)` ONCE
  (`auto& blk`, non-const `getBlock`, message-thread mutate; idempotent — once non-zero it never
  re-derives, so a later move preserves the owner). Placed here, not in `PatternManager` deserialize,
  to avoid a data-model→VibeGraph include (`MixerChannelIds` is already in scope in PluginProcessor.cpp).
- **Vox/Inst untouched** — they route via `routeChannel` (600/700) through `renderFilePlayPlayer`,
  excluded from this path; the Audio (400) / Vox (600) / Inst (700) ranges are disjoint.
- **Trap refinement (responding to Jeff's no-project-popup finding, 2026-06-02):** Jeff dragged a
  clip with NO project open and got the copy-empty popup again (log 22:01:31: copy-empty bail with
  no `importSample` line = `hasProject()` false; the New-Project prompt + retry at 22:01:40 then
  succeeded — SC-J working). The popup on the EXPECTED no-project case is noise for the long
  held-open watch, so the popup is now scoped to the genuine 2nd case ONLY: `importSample`
  `copyFileTo` FAILED while a project is open (`log()`→`alert()`). Demoted to `log()` (no popup):
  the importAudioFile copy-empty bail, the file-missing bail, the "+ Add New Clip no-new-entry"
  anomaly. Added an explicit "NO PROJECT OPEN" `log()` in the `onImportSampleRequest` lambda so the
  no-project case is unambiguous in the log AND flags the case-(b) tell (if it fires while a project
  IS open, that's the 2nd case = project handle lost mid-session). Net: quiet on normal no-project
  drags; pops only when the real saved-project copy-failure recurs.
- Files: `Source/PluginProcessor.cpp` (Task 2 routing) + `Source/ProjectManager.cpp` +
  `Source/Standalone/BuilderPage.cpp` + `Source/Standalone/StandaloneEditor.cpp` (trap refinement).
  Awaiting Jeff's Debug+Release verify (drag / move / move-to-unused-row / old-project / Vox-Inst,
  **with a project OPEN**) before the Task 2 commit. **HIGH-risk hot path.**
- **VERIFIED PASS (Jeff, Debug + Release, 2026-06-02):** drag + move a clip across grid rows (incl.
  a never-used row) keeps playing; old project (TESTIES) clips play + strips unchanged; Vox/Inst
  unaffected; the trap is quiet on no-project drags (New-Project prompt only, no popup). Ready to
  commit Task 2 + the trap refinement.

## Diagnostic Instrumentation Catalog

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| `Source/ClipDropDiag.h` (NEW file) | `[QA-ClipDrop DIAG]` | Diagnostic helper: `log()` append-to-file + `alert()` popup+log (Debug + Release) | Remove at batch close (or Keep per DS-2) |
| `ProjectManager.cpp` `importSample` (all return paths) | `[QA-ClipDrop DIAG]` | Log every return path; **`alert()` popup on `copyFileTo` FAILED while a project is open** (the genuine 2nd-case catch) | Remove at batch close (or Keep per DS-2) |
| `StandaloneEditor.cpp` `onImportSampleRequest` lambda | `[QA-ClipDrop DIAG]` | Explicit "NO PROJECT OPEN" log (copy skipped, New-Project prompt follows); flags the case-(b) tell if it fires with a project open | Remove at batch close (or Keep per DS-2) |
| `BuilderPage.cpp` `importAudioFile` (enter / 2 `alert()` bails / library-add / callback) | `[QA-ClipDrop DIAG]` | Trace the drop convergence point; popup on file-missing + copy-empty bails | Remove at batch close (or Keep per DS-2) |
| `BuilderPage.cpp` `filesDropped` (enter / dup-vs-import branch) | `[QA-ClipDrop DIAG]` | Trace the drag-drop entry + branch | Remove at batch close (or Keep per DS-2) |
| `StandaloneEditor.cpp` `onAddTabRequest` Clip branch (picked / anomaly `alert()` / OK) | `[QA-ClipDrop DIAG]` | `+ Add New Clip` entry; popup when it produces no new library entry | Remove at batch close (or Keep per DS-2) |
| `StandaloneEditor.cpp` `onAudioClipAdded` (enter) | `[QA-ClipDrop DIAG]` | Confirm the strip/page cascade fired | Remove at batch close (or Keep per DS-2) |
| `StandaloneEditor.cpp` `spawnClipsTabIfMissing` (bail / dedup / row-taken / create) | `[QA-ClipDrop DIAG]` | Which page-spawn outcome (or no-op reason) | Remove at batch close (or Keep per DS-2) |
