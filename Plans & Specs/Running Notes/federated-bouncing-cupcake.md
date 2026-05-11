# Running Notes — QA-D (federated-bouncing-cupcake)

> **Purpose.** Append-only mid-batch log of what was done, what was found, what
> was decided, and what was deferred during QA-D execution.  Compiled from
> `/draft-doc running-notes` dispatches at every significant checkpoint
> (commit landed / sub-task verified / finding captured / decision made /
> scope pivot / spec call resolved).
>
> At batch close, `/draft-doc batch-close` reads this file (plus git log,
> memory entries, the per-batch plan, and conversation context) and produces
> the single Implemented Work Log entry that goes into `Plans & Specs/
> Implemented Work Log.md`.  This file is the source-of-truth intermediate
> artifact during the batch; the close entry is the durable summary.
>
> **Pair file:** `Plans & Specs/Batch Plans/federated-bouncing-cupcake.md`
> (the per-batch plan).
>
> **Convention:** see `Plans & Specs/Main Plan.md` §0 (folder-scope rule +
> Agent Orchestration Rules' mid-batch checkpoint trigger).  Running-notes
> subfolder convention established 2026-05-09 mid-QA-A.

---

## 2026-05-10 — Task 0 (open) — pre-batch setup

### Done

- Pre-batch reads complete: `/standup`, Main Plan §5 QA-D entry + §6
  sequencing + §0 Agent Orchestration Rules + recent §9 Forks entries
  (7-10), Carry-Forward Reference §2/§4/§5/§6/§8 (project-load lifecycle
  primitives), recent Implemented Work Log entries (top 5 close entries
  via persisted reader).
- Plan-mode draft authored at `~/.claude/plans/federated-bouncing-cupcake.md`,
  approved by Jeff at ExitPlanMode.
- Plan mirrored to canonical `Plans & Specs/Batch Plans/federated-bouncing-cupcake.md`.
- Home-dir copy at `~/.claude/plans/federated-bouncing-cupcake.md` deleted
  (one-way mirror per `feedback_plan_mirror_one_way.md`).
- Main Plan §5 QA-D entry now points at the canonical plan file.
- This running-notes file seeded.

### Spec calls — locked at pre-batch

S1, S2, S3, S4, S5, S6, S7, S8 all locked per the plan file's "Spec calls
already locked" table.  S6 (one-commit-per-item) and S8 (silly-name) were
the only ones Jeff picked outright; the rest he asked for industry-
standard recommendations and approved them.

### Sub-spec calls — locked at ExitPlanMode

- **Sub-A (singular vs plural):** singular form (`Layer N` / `Bass N` /
  `Drum N`).  Matches existing internal `mTabName` style in
  LayersPage.cpp:113 / BassPage.cpp:104 / DrumPage.cpp:275 + Vox/Inst
  singular convention.
- **Sub-B (0-based vs 1-based):** 1-based (`Layer 1`, not `Layer 0`).
  Matches existing Vox/Inst/Clips behavior; user-friendly DAW UX.
- **Sub-C (counter behavior on saved-project load):** restart from 1 on
  closeAllDynamicTabs, then advance to `max(restored-name-numbers) + 1`
  after deserializeUIState.

### Findings (mid-planning)

- **STATE-02 reframe (per Jeff at spec-call surface):** original §5
  scope ("Guitar/Bass counters don't reset on new project") was the tip
  of a broader bug.  Actual user-visible symptom is that *every*
  dynamic-tab type's name-numbering reuses the freed-slot index when a
  tab is deleted and a new one is added (e.g. delete "Layer 2", add
  new → name becomes "Layer 2 again" instead of "Layer 4").  Layers/
  Bass/Drums tabs additionally don't carry a number suffix in the
  ribbon at all, while Vox/Inst/Clips do — unified inconsistency.
  Plan re-scoped: unified monotonic-counter scheme across all 8
  tab-naming sites (Layers / Bass / Drums / Vox / Inst-LiveInput /
  Inst-BaySickGuitars / Inst-BaySickBasses / Clips), all counters
  reset on closeAllDynamicTabs, all advanced past max-restored after
  deserializeUIState.
- **MenuBarModel ownership (Explore agent finding):** the bug was
  suppressed in vendored JUCE.  Current declaration order in
  PianoRoll.h:645-646 / BuilderPage.h:750-751 / DrumKitGrid.h:494-495
  has component declared FIRST, model declared SECOND, which means
  model is destroyed FIRST (reverse declaration order) — backwards.
  Fix shape: swap declaration order (model FIRST → destroyed LAST) +
  defensive `mMenuBar->setModel(nullptr); mMenuBar.reset()` at top of
  each container's destructor.
- **Closed-form check (Carry-Forward §2):** `ProjectManager::openProject`
  body at ProjectManager.cpp:280-300 already has `mIgnoreDirty` for the
  main APVTS but per-engine `ApvtsDirtyTracker` instances aren't
  gated by it — that's the STATE-01 gap.  Fix: wrap the dirty-hook
  callback in StandaloneEditor with an `isLoadingProject()` check
  (12 wrap sites per pre-batch grep).

### Routing notes (Rule 3, opened)

- (none yet — file held open for findings during execution)

---

### 2026-05-10 — Task 0 close — commit landed

- Commit: `003cfb1` — "QA-D open: plan file + Main Plan pointer (Project State Reset batch)."
- 3 files committed:
  - `Plans & Specs/Main Plan.md` (modified — §5 QA-D entry repointed at canonical plan file)
  - `Plans & Specs/Batch Plans/federated-bouncing-cupcake.md` (added — mirrored from plan-mode, home-dir copy deleted)
  - `Plans & Specs/Running Notes/federated-bouncing-cupcake.md` (added — this file, seeded)
- Working tree clean.
- 5 commits ahead of `origin/main`; not pushing per standing convention (push happens at batch close or explicit request).
- **Next:** Task 1 — STATE-04 (project-load transport stop).  Shape: `StandalonePlayHead::stop()` invoked at top of `ProjectManager::openProject` via a callback hook installed by `StandaloneEditor` (ProjectManager doesn't link against StandalonePlayHead; callback indirection keeps the dependency direction one-way).

---

### 2026-05-10 — Task 1 source edits applied (uncommitted)

- Added `std::function<void()> onBeforeOpenProject` public field to [Source/ProjectManager.h:157](Source/ProjectManager.h:157) (between existing `onDirtyChanged` and `setAutosaveIntervalSeconds`).
- Invoked `onBeforeOpenProject` at top of [Source/ProjectManager.cpp:285](Source/ProjectManager.cpp:285) — before file-existence check.
- Wired the callback in [Source/Standalone/StandaloneEditor.cpp:454](Source/Standalone/StandaloneEditor.cpp:454) right after `onDirtyChanged`: lambda stops transport + clears play-state if playing.
- Jeff verified in Debug: load while playing now stops transport cleanly before the load.  Release verify pending after Task 3 commit lands (paired build cycle).

---

### 2026-05-10 — Task 1 committed at `dcd771f`

- Commit: `dcd771f` — "QA-D Task 1 source: STATE-04 stop transport on project open."
- 4 files committed: ProjectManager.h, ProjectManager.cpp, StandaloneEditor.cpp, running-notes file.
- Working tree clean post-commit.  6 commits ahead of origin/main.

---

### 2026-05-10 — Task 3 (STATE-01) scope pivot + diagnostic + fix

#### Scope pivot

- Initial pre-batch plan: Task 3 wraps the `mProjectManager->markDirty()` callback at every wiring site (12 sites in StandaloneEditor) with `if (! isLoadingProject())` checks.
- During execution Claude noted that `ProjectManager::markDirty()` already short-circuits when `mIgnoreDirty` is true (line 100 — has been since original P5 implementation, not QA-C as initially recalled).  Surfaced to Jeff with option to drop Task 3.
- Jeff confirmed the bug is real: loaded a project via File → Open Recent, the title-bar `*` fired.  Existing gate is being bypassed somewhere.
- Memory rule `feedback_diagnose_before_fixing.md` applied: diagnose before shipping a speculative fix.

#### Diagnostic shipped + reverted

- **Shipped** (now reverted): added private members `mPostLoadDiagnosticUntilMs` + `mPostLoadDiagnosticFired` to ProjectManager; opened a 3-second window at end of `openProject`; in `setDirtyInternal`, popped a one-shot AlertWindow with `juce::SystemStats::getStackBacktrace()` when dirty transitioned FALSE → TRUE inside the window.
- Jeff built Debug + reproduced via File → Open Recent.  AlertWindow popped with the trace.

#### Bypass identified (verbatim trace, top → bottom = recent → oldest)

```
0:  juce::SystemStats::getStackBacktrace
1:  ProjectManager::setDirtyInternal
2:  ProjectManager::markDirty
3:  StandaloneEditor::StandaloneEditor lambda_3      <- the dirty hook from line ~482
... (lambda machinery)
7:  VibeSynthProcessor::prepareToPlay lambda_1       <- processor's onAnyStateChange wrapper
... (lambda machinery)
11: VibeGraph::rebindAllRackHooks lambda_1           <- rack-lifecycle hook wire-up
... (lambda machinery)
15: EffectRack::clearSlot + 0x373                    <- the rack-state replay clears a slot,
16: EffectRack::setStateInformation + 0xcb9             firing the lifecycle hook chain
17: VibeGraph::applyRackStates lambda_2 + 0x29d
18: VibeGraph::applyRackStates + 0x109
19: VibeGraph::loadRackStates + 0x81
20: VibeSynthProcessor::applyPendingRackStates + 0xd7
21: StandaloneEditor::restoreAudioStripsFromArrangement + 0x3a3  <- runs OUTSIDE gate
22-25: StandaloneEditor::menuItemSelected `20` lambda_1 (File → Open Recent handler)
```

#### Diagnosis

`restoreAudioStripsFromArrangement` is called from 5 menu-handler sites (StandaloneEditor.cpp:7790, 8213, 8251, 8355, 8507) — all load paths — AFTER `ProjectManager::openProject` returns.  The 2026-04-24 design intentionally defers per-insert rack-state replay until after `deserializeUIState` creates the InsertNodes, but no one wrapped that deferred replay (`applyPendingRackStates`) in the dirty-suppression gate.  The rack-state replay fires `EffectRack::clearSlot` lifecycle hooks → which chain through `VibeGraph::rebindAllRackHooks` lambda → `VibeSynthProcessor`'s `onAnyStateChange` lambda → StandaloneEditor's `markDirty` lambda → setDirtyInternal fires (outside the gate).

#### Fix shipped

- **Reverted** the diagnostic in [ProjectManager.h](Source/ProjectManager.h) + [ProjectManager.cpp:setDirtyInternal](Source/ProjectManager.cpp) + [ProjectManager.cpp:openProject](Source/ProjectManager.cpp) (3 reverts, all done before Task 1 commit).
- **Added** public accessors `isLoadingProject()` + `setIgnoreDirty(bool)` to [ProjectManager.h:153-163](Source/ProjectManager.h:153) so call sites can gate manually.
- **Wrapped** [`StandaloneEditor::restoreAudioStripsFromArrangement`](Source/Standalone/StandaloneEditor.cpp:9469) body with stash-set-restore-clear: `wasIgnoring = isLoadingProject()`; `setIgnoreDirty(true)` at top; body runs; `setIgnoreDirty(wasIgnoring); if (! wasIgnoring) clearDirty();` at end.  Every caller is a load path, so the clearDirty at end is correct (unsaved-edit confirmation prompts run earlier in each menu handler).
- Jeff verified in Debug: load via File → Open Recent no longer fires `*`; tweaking a knob after load still triggers `*` correctly.  Release verify deferred to next build cycle.

#### Routing notes

- (none new — STATE-01 contained in batch).
