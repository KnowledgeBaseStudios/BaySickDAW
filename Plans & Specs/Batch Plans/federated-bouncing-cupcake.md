# QA-D — Project State Reset — Plan (federated-bouncing-cupcake)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/federated-bouncing-cupcake.md`
> Paired running notes: `Plans & Specs/Running Notes/federated-bouncing-cupcake.md`

> **For execution:** use `superpowers:executing-plans` inline. Steps use `- [ ]` checkbox syntax for tracking. Build commands run by Jeff (`do_build.bat`) — never by Claude. Verify in Debug exe FIRST, then Release (per CLAUDE.md Build System standing rule).

## Context

QA-D is the Phase 2 batch following QA-C, addressing four project-lifecycle bugs that surface during project-open / new-project / template-load flows. Three are explicitly scoped in Main Plan §5 (STATE-01/02/04); the fourth was folded in 2026-05-07 from QA-0a/QA-0 finding #8 (MenuBarModel listener-dangle).

**STATE-02 reframed (mid-planning):** original §5 wording "Guitar/Bass counters don't reset on new project" turned out to be the tip of a broader bug — **every** dynamic-tab type's name-numbering currently restarts at the first-free index when a tab is deleted and a new one of the same type is added. Layers/Bass/Drums tabs additionally don't carry a number suffix at all in the ribbon (Vox/Inst/Clips do, inconsistently). The fix is a unified monotonic-counter scheme across all tab-name sites, with all counters resetting on new project / `closeAllDynamicTabs`.

**Dependencies:** none. Phase 2 is gated on QA-C close (already landed at 2ba626b).

**Risk:** medium. Project-load critical path. STATE-02 refactor touches 8 tab-naming sites in StandaloneEditor.

**Effort estimate:** ~5-7 hours. (Original §5 estimate was 4-6h plus 1-2h for folded MenuBarModel; the STATE-02 reframe adds a small amount.)

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| S1 | MenuBarModel: defensive `setModel(nullptr); reset()` in destructors of containers (PianoRoll, BuilderPage, DrumKitGrid). RAII declaration-order swap also applied as the root-cause fix. | Belt-and-suspenders. Carry-Forward §6 mShuttingDown gate philosophy. Bug was suppressed in vendored JUCE but real. |
| S2 | Unified monotonic-counter scheme across all 8 tab-naming sites; counters reset on `closeAllDynamicTabs`. | Per Jeff's reframe: actual user-visible bug is delete+re-add reuses freed slot number; consistency fix lifts all 8 sites onto the same scheme. |
| S3 | Wrap markDirty callback at the StandaloneEditor wiring site with `if (! projectManager.isLoadingProject()) projectManager.markDirty()`. New `ProjectManager::isLoadingProject()` accessor returns existing `mIgnoreDirty`. | Reuses existing flag; one source of truth; ApvtsDirtyTracker stays simple; matches existing G-7 ClipsPage/InstPage suppress philosophy. |
| S4 | `StandalonePlayHead::stop()` invoked from top of `ProjectManager::openProject()` via callback hook set by StandaloneEditor at startup. | Lowest-coupling anchor — openProject is THE load entry point. Callback hook avoids dragging StandalonePlayHead into ProjectManager's includes. |
| S5 | `resetProjectState()` lives as a member function of `StandaloneEditor`, called from inside `closeAllDynamicTabs` after the existing teardown loop. | closeAllDynamicTabs is already a StandaloneEditor member. Counters live on StandaloneEditor. ProjectManager doesn't have UI-state visibility (intentional). |
| S6 | One commit per item: 4 source commits (Tasks 1-4) + 1 close commit. | Locked at pre-batch. |
| S7 | MT-awareness verification = reasoned static analysis paragraph in close entry + Jeff's normal Debug+Release smoke. | The barrier is a single atomic; `playHead.stop()` + `setProjectLoadInProgress(true)` are independent gates. |
| S8 | Plan-file silly-name = `federated-bouncing-cupcake` (assigned by plan-mode runtime; running-notes file matches). | Locked at plan-mode entry. |

---

## Sub-spec calls surfaced for ExitPlanMode (recommendations Jeff can override)

| ID | Question | Recommendation | Reasoning |
|----|----------|----------------|-----------|
| Sub-A | Tab-name singular vs plural form for Layers/Bass/Drums in the ribbon? | **Singular**: "Layer 1" / "Bass 1" / "Drum 1". | Matches existing `mTabName` style in LayersPage.cpp:113 / BassPage.cpp:104 / DrumPage.cpp:275 ("Layer "/"Bass "/"Drum "). Vox is "Vox" (singular), Inst is "Inst" (singular). |
| Sub-B | Numbering 0-based or 1-based? | **1-based** ("Layer 1", "Bass 1", etc.) | Matches existing Vox/Inst/Clips behavior. More user-friendly. Industry-standard for DAW UIs. |
| Sub-C | When loading a saved project, do tab-name counters resume from the highest tab number found in the project, or restart from 1? | **Restart from 1, then advance to (highest existing tab number + 1).** | Project-load happens through closeAllDynamicTabs (counters reset to 1), then deserialize re-creates tabs with their saved names. Counter must be advanced past the highest restored number so a subsequent +Add doesn't collide. Concrete rule: after deserialize, scan restored tabs of each type, set counter to max(found-name-numbers) + 1. |

---

## Files to modify

### Task 1 — STATE-04 (playhead-stop on project-open)
- [Source/ProjectManager.h](Source/ProjectManager.h) — add `void setPlayHeadStopCallback(std::function<void()>)` + member `std::function<void()> mPlayHeadStopFn`.
- [Source/ProjectManager.cpp](Source/ProjectManager.cpp) — invoke `mPlayHeadStopFn` at top of `openProject()` (line ~282, before file-existence check).
- [Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp) — wire the callback during ProjectManager initialization (search for where `mProjectManager` is constructed/wired; pass a lambda that calls `mPlayHead.stop()` and updates `mTransport->setPlayState(false, true)`).

### Task 2 — STATE-02 (unified monotonic counters + reset)
- [Source/Standalone/StandaloneEditor.h](Source/Standalone/StandaloneEditor.h) — add 8 counter members (`int mNextLayerNameNum { 1 }`, `mNextBassNameNum`, `mNextDrumNameNum`, `mNextVoxNameNum`, `mNextInstNameNum`, `mNextClipNameNum`, `mNextGuitarNameNum`, `mNextBassesNameNum`). Add `void resetProjectState()` member declaration.
- [Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp):
  - **Layers ribbon name** at `:1499` (default ctor) and `:3248` (onAddTabRequest): change from `"Layers"` to `"Layer " + juce::String (mNextLayerNameNum++)`.
  - **Bass ribbon name** at `:1544` and `:3253`: change from `"Bass"` to `"Bass " + juce::String (mNextBassNameNum++)`.
  - **Drums ribbon name** at `:1588` and `:3258`: change from `"Drums"` to `"Drum " + juce::String (mNextDrumNameNum++)`.
  - **Vox ribbon name** at `:7249`: change from `"Vox " + juce::String (voxIdx + 1)` to `"Vox " + juce::String (mNextVoxNameNum++)`.
  - **Inst (LiveInput) ribbon name** at `:7346`: similar replacement using `mNextInstNameNum++`.
  - **Inst (BaySickGuitars) ribbon name** at `:6491`: replace scan-and-count with `mNextGuitarNameNum++`.
  - **Inst (BaySickBasses) ribbon name** at `:6571`: replace scan-and-count with `mNextBassesNameNum++`.
  - **Clips fallback ribbon name** at `:6870`: replace `"Clip " + juce::String (audioRow + 1)` fallback with `mNextClipNameNum++` (filename branch unchanged).
  - **resetProjectState() body**: zero all 8 counters back to 1.
  - **Call site**: invoke `resetProjectState()` from inside `closeAllDynamicTabs()` (line ~8767), after the existing teardown loop, before `setProjectLoadInProgress(false)`.
  - **Post-deserialize counter advance** (per Sub-C above): after `deserializeUIState` runs the tab-restore walker (line ~8814+), scan restored tabs of each type, parse their name suffixes, and advance each counter to `max(found-numbers) + 1`. Helper: `void advanceCountersFromRestoredTabs()` private member.
- [Source/Standalone/LayersPage.cpp:113](Source/Standalone/LayersPage.cpp:113), [BassPage.cpp:104](Source/Standalone/BassPage.cpp:104), [DrumPage.cpp:275](Source/Standalone/DrumPage.cpp:275) — internal `mTabName` defaults: keep as-is OR sync to use the same `(pageIndex)`-based form. **Note:** internal `mTabName` is currently used for piano-roll context label; with the ribbon now showing the monotonic-counter name, the piano-roll context label should match. Cleanest path: when StandaloneEditor creates each page, push the ribbon-tab name into `setTabName()` so the page uses the same string. Avoids divergence.

### Task 3 — STATE-01 (dirty-flag suppression during project load)
- [Source/ProjectManager.h](Source/ProjectManager.h) — add `bool isLoadingProject() const noexcept { return mIgnoreDirty; }` public accessor.
- [Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp:7091) — wrap the dirty-hook lambda (around line 7091) with the suppression check:
  ```cpp
  auto hook = [safeThis] {
      if (safeThis && safeThis->mProjectManager
          && ! safeThis->mProjectManager->isLoadingProject())
          safeThis->mProjectManager->markDirty();
  };
  ```
  Same wrap-pattern at every other `mProjectManager->markDirty()` call site (lines 453, 460, 468, 762, 1141, 3651, 7091, 7952, 7981, 7999, 9624, 9656). 12 wrap sites total.
- Verify ProjectManager::openProject already holds `mIgnoreDirty = true` across `deserializeProject()` (confirmed at lines 294-296). No change needed there.

### Task 4 — Folded MenuBarModel listener-dangle fix
- [Source/Standalone/PianoRoll.h:645-646](Source/Standalone/PianoRoll.h:645) — swap declaration order (model FIRST, component SECOND) so model is destroyed LAST.
- [Source/Standalone/BuilderPage.h:750-751](Source/Standalone/BuilderPage.h:750) — same swap.
- [Source/Standalone/DrumKitGrid.h:494-495](Source/Standalone/DrumKitGrid.h:494) — same swap.
- [Source/Standalone/PianoRoll.cpp](Source/Standalone/PianoRoll.cpp), [BuilderPage.cpp](Source/Standalone/BuilderPage.cpp), [DrumKitGrid.cpp](Source/Standalone/DrumKitGrid.cpp) — at the top of each container's destructor, defensive teardown:
  ```cpp
  if (mMenuBar) { mMenuBar->setModel (nullptr); mMenuBar.reset(); }
  ```
  Belt-and-suspenders against future re-ordering or any not-yet-reproduced cascade-ordering edge case.
- StandaloneEditor + EventEditorContent inherit MenuBarModel directly (no separate member); no change needed for those — JUCE handles the self-referential case.

---

## Tasks

### Task 0 — Open commit
- [ ] Mirror `~/.claude/plans/federated-bouncing-cupcake.md` → `Plans & Specs/Batch Plans/federated-bouncing-cupcake.md` (Write tool); delete the home-dir copy.
- [ ] Update Main Plan §5 QA-D entry header with `**Plan file:** Plans & Specs/Batch Plans/federated-bouncing-cupcake.md` line.
- [ ] Seed `Plans & Specs/Running Notes/federated-bouncing-cupcake.md` with header + initial "Task 0: open" entry.
- [ ] Surface full git status. Dispatch `/draft-commit`. Surface drafted message + git status to Jeff for approval. Commit on approval.
- [ ] Mark Task 0 done; update todos.

### Task 1 — STATE-04: stop playhead at top of project-open
- [ ] Read existing `ProjectManager` constructor to confirm wiring entry point.
- [ ] Add `void setPlayHeadStopCallback(std::function<void()>)` + private `std::function<void()> mPlayHeadStopFn` to ProjectManager.h.
- [ ] At top of `ProjectManager::openProject()` (line ~282), before file-existence check: `if (mPlayHeadStopFn) mPlayHeadStopFn();`.
- [ ] In StandaloneEditor (where ProjectManager is constructed/wired — search for `mProjectManager = ` or `make_unique<ProjectManager>`), call `setPlayHeadStopCallback([this] { mPlayHead.stop(); if (mTransport) mTransport->setPlayState(false, true); })`.
- [ ] Tell Jeff: "Run `do_build.bat`. Then in Debug: load a project while playback is active. Verify the transport stops and the playhead returns to position 0 before the project loads."
- [ ] Wait for Jeff's verify result.
- [ ] If verify passes: dispatch `/draft-commit`, surface drafted message + git status, commit on approval.
- [ ] Dispatch `/draft-doc running-notes` and apply to running-notes file.

### Task 2 — STATE-02: unified monotonic counters + ribbon naming consistency
- [ ] Add 8 counter members to StandaloneEditor.h (initial value 1). Add `resetProjectState()` declaration. Add `advanceCountersFromRestoredTabs()` private helper declaration.
- [ ] Update all 8 ribbon-name composition sites in StandaloneEditor.cpp per the file table above.
- [ ] Implement `resetProjectState()`: zero all 8 counters back to 1.
- [ ] Implement `advanceCountersFromRestoredTabs()`: scan `mPages` after deserializeUIState, parse trailing-number suffixes from each tab's display name (per type), set each counter to `max(found) + 1` (or stay at 1 if none found).
- [ ] Call `resetProjectState()` from inside `closeAllDynamicTabs()` after the existing teardown loop (before barrier release).
- [ ] Call `advanceCountersFromRestoredTabs()` at the end of `deserializeUIState`.
- [ ] Sync internal `mTabName` on Layers/Bass/Drum pages to match the ribbon name (push via `setTabName()` from StandaloneEditor at page-creation time, replacing the old hard-coded `"Layer " + pageIndex` form).
- [ ] Tell Jeff: "Run `do_build.bat`. Test sequence in Debug:
  - **(1)** New project. Add 3 Layer tabs → ribbon shows "Layer 1", "Layer 2", "Layer 3". Delete "Layer 2". Add new Layer → ribbon shows "Layer 4" (NOT "Layer 2 again").
  - **(2)** Same for Bass / Drums / Vox / Inst (LiveInput) / Inst BaySickGuitars / Inst BaySickBasses / Clips.
  - **(3)** With several numbered tabs open, File → New. Add a Layer → ribbon shows "Layer 1" (counter reset).
  - **(4)** Save a project with "Layer 1", "Layer 2", "Layer 3", "Layer 5" (delete Layer 4 first). Close + reopen the project. Add a new Layer → ribbon shows "Layer 6" (counter advanced past max-restored)."
- [ ] Wait for Jeff's verify result.
- [ ] On pass: `/draft-commit`, surface, commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 3 — STATE-01: dirty-flag suppression during project load
- [ ] Add `bool isLoadingProject() const noexcept { return mIgnoreDirty; }` to ProjectManager.h public section.
- [ ] Audit every `mProjectManager->markDirty()` call site in StandaloneEditor.cpp. Wrap each with `if (! mProjectManager->isLoadingProject()) ...`. 12 sites per pre-batch grep.
- [ ] Tell Jeff: "Run `do_build.bat`. Test in Debug:
  - **(1)** Load a saved project. Verify the title-bar `*` (dirty indicator) stays clean immediately after load (does not flicker on, doesn't stick).
  - **(2)** After load, tweak a knob. Verify `*` appears (dirty trigger still works for real edits).
  - **(3)** File → New. Verify `*` is clean.
  - **(4)** Edit a Harmless / BaySickSynth / Guitars / Basses knob immediately after a load (don't wait). Verify `*` appears (per-engine ApvtsDirtyTrackers gated correctly)."
- [ ] Wait for verify result.
- [ ] On pass: `/draft-commit`, surface, commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 4 — MenuBarModel listener-dangle fix
- [ ] Swap declaration order in PianoRoll.h:645-646, BuilderPage.h:750-751, DrumKitGrid.h:494-495.
- [ ] Add defensive teardown at top of each container's destructor (`if (mMenuBar) { mMenuBar->setModel(nullptr); mMenuBar.reset(); }`).
- [ ] Tell Jeff: "Run `do_build.bat`. In Debug: load a project, switch tabs around (open piano roll, open builder, open drum kit), close tabs, close project. Verify no `removeListener` jassert dialog fires. Repeat with Restore Backup + New Project flows."
- [ ] Wait for verify result. (This is a hard-to-trigger bug — assertion was suppressed in vendored JUCE; if Debug doesn't fire it now either, we're shipping the fix as a defensive measure with reasoned-static-analysis as the verification.)
- [ ] On pass: `/draft-commit`, surface, commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 5 — Close sequence
- [ ] Dispatch `/draft-doc batch-close` with a synthesis of the running-notes file.
- [ ] Apply the close entry to `Plans & Specs/Implemented Work Log.md` via Edit.
- [ ] Dispatch `/review-batch QA-D`.
- [ ] Address BLOCKERs / NEEDS-FIX in-batch. Defer NITs into close-entry routing table.
- [ ] Surface full git status.
- [ ] Dispatch `/draft-commit` for close commit. Surface message + status. Commit on approval.

---

## MT-awareness static-analysis (per S7)

The audio-thread barrier (`mProjectLoadInProgress.load(memory_order_acquire)` at PluginProcessor.cpp:945) is independent of playhead state. Adding `playHead.stop()` BEFORE the existing `setProjectLoadInProgress(true)` does not change the barrier semantics — playhead `mPlaying` flag is checked separately by transport-driven scheduling, while the audio-thread barrier short-circuits processBlock entirely and clears the buffer. Both still engage in the new ordering. No runtime test needed beyond Jeff's standard Debug+Release smoke.

---

## Verification (end-to-end smoke)

After Task 4 commit lands:
1. **Build clean.** `do_build.bat` Release + Debug both green.
2. **Load + Play stop.** Open a project while transport is playing. Transport stops, playhead resets to 0, project loads.
3. **Counter monotonicity.** Add 5 tabs of any type → "Type 1..5". Delete "Type 3". Add new → "Type 6".
4. **Counter reset on new project.** With "Type 5" present, File → New. Add → "Type 1".
5. **Counter advance from saved project.** Save project at "Type 5". Close + reopen. Add → "Type 6".
6. **Dirty flag clean post-load.** Load → no flicker. Tweak knob → dirty appears.
7. **No menubar assertions.** Project load → cycle tabs → close → no jassert dialogs.

---

## Routing notes (Rule 3 application during execution)

- Findings about other STATE bugs surfaced during execution → fold here if scoped to project-load lifecycle; route to §9 + new §5 batch otherwise.
- Findings about engine-specific dirty-tracker gaps (engines that don't have ApvtsDirtyTracker yet) → log in routing table; route to QA-Audit as a manifest item.
- Findings about tab counter persistence across saves (e.g. "I want max-restored + 1" but Jeff prefers "highest-name + 1 even with deletions") → revisit Sub-C decision before commit.
