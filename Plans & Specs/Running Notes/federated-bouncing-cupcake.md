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

---

### 2026-05-10 — Task 3 committed at `6288e85`

- Commit: `6288e85` — "QA-D Task 3 source: STATE-01 suppress dirty `*` on project load."
- Files committed: [Source/ProjectManager.h](Source/ProjectManager.h) (public `isLoadingProject()` + `setIgnoreDirty(bool)` accessors added), [Source/ProjectManager.cpp](Source/ProjectManager.cpp) (3 diagnostic reverts), [Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp) (`restoreAudioStripsFromArrangement` wrapped with stash-set-restore-clear gate).
- Working tree clean at commit time.  7 commits ahead of `origin/main`; not pushing per standing convention.
- **Next:** Task 2 — STATE-02 (monotonic tab-name counters), starting with two pre-implementation sub-spec calls (Sub-D + Sub-E) before any source edits.

---

### 2026-05-10 — Task 2 pre-implementation sub-spec calls (Sub-D + Sub-E)

Two ambiguities surfaced before Task 2 source edits could begin; both routed to Jeff for resolution per `feedback_dont_make_unilateral_spec_calls.md`.

- **Sub-D — BaySickBasses Inst-tab prefix (plural disambiguation).**  The Inst-tab type covers two engines (BaySickGuitars + BaySickBasses).  Existing convention names Inst-LiveInput tabs `Inst N`, BaySickGuitars Inst-tabs `Guitar N`.  A literal "Bass" prefix for BaySickBasses Inst-tabs would collide with the Bass-slot tabs ("Bass N" — Layers/Bass page).  Decision: BaySickBasses Inst-tabs use plural `Basses N` prefix to disambiguate.  Counter `mNextBassesNameNum` is distinct from `mNextBassNameNum`.
- **Sub-E — Drums-from-file fallback name.**  When a drum tab is created from a dropped audio file, the existing code branches on whether the file path yields a stem name (used directly) or not (legacy fallback was the literal "Drums").  With monotonic counters in place, the no-stem branch needs a numbered fallback.  Decision: use `nextDrumTabName()` for the no-stem fallback (yields the next `Drum N` like the rest of the type).

Also approved at the same surface: **helper-method approach** for the 8 counters — each gets an inline `nextXxxTabName()` method on `StandaloneEditor` (advances counter + returns formatted name in one call), keeping all 15 addTab sites to single-line edits.

---

### 2026-05-10 — Task 2.1-2.5 source edits applied (uncommitted)

Monotonic tab-name counters across all 8 dynamic-tab types.  Counter values reset to 1 on `closeAllDynamicTabs()`; advanced past `max(restored-name-numbers) + 1` after `deserializeUIState`.

#### Header additions ([Source/Standalone/StandaloneEditor.h](Source/Standalone/StandaloneEditor.h))

- 8 counter members added to private section (after existing `mUsedDrumIndices`): `mNextLayerNameNum`, `mNextBassNameNum`, `mNextDrumNameNum`, `mNextVoxNameNum`, `mNextInstNameNum`, `mNextGuitarNameNum`, `mNextBassesNameNum`, `mNextClipNameNum` — all `int { 1 }`.
- 8 inline helper methods on `StandaloneEditor` — each `juce::String nextXxxTabName()` advances its counter + returns the formatted prefixed name.  Inlined in the header for single-line call-site clarity.
- Two private member declarations added: `void resetProjectState()`, `void advanceCountersFromRestoredTabs()`.

#### 15 `addTab` creation sites converted to helpers ([Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp))

- 3 default-ctor sites: Layers (~1514), Bass (~1561), Drums (~1607).
- 3 spawnDuplicate sites (Layer / Bass / Drum dropdown ▸ Duplicate).
- 3 onAddTabRequest sites (switch case body — `name = nextXxxTabName()` for Layer/Bass/Drum branches).
- 2 spawnTemplate sites (Layer + Bass — `spawnLayerTabFromTemplate` / `spawnBassTabFromTemplate`).
- 1 Drums-from-file site — preserves stem-name branch; replaces literal "Drums" fallback with `nextDrumTabName()` per Sub-E.
- 2 BaySick* sites: Guitars (~6505 — `nextGuitarTabName()`) + Basses (~6585 — `nextBassesTabName()`, **replacing the pre-existing scan-and-count loop** that walked `mPages` to find the next free `Basses N` slot).
- 1 Vox site (replaces ad-hoc `voxIdx + 1` increment).
- 1 Inst-LiveInput site (replaces ad-hoc `instIdx + 1` increment).
- 1 Clips site (replaces `audioRow + 1` fallback only — primary clip-name path that uses file stem name is untouched).

#### Sites left untouched

- BaySickRustyDrums (singleton — fixed name `BaySickRustyDrums`, no counter).
- 3 deserialize-restore paths (project-load creation uses the saved-XML `<Tab name="...">` attribute directly; counter advance happens after via `advanceCountersFromRestoredTabs`).

#### Internal `mTabName` sync (Task 2.3)

- `setTabName` syncs added at every Layer/Bass/Drum addTab call site (default + spawnDuplicate + spawnTemplate + onAddTabRequest paths) so each page's internal `mTabName` matches the ribbon label set by `addTab` (used by piano-roll context-label composition).  Pre-existing convention was to set `mTabName` separately from the ribbon label; this commit makes them set in lockstep.

#### Lifecycle wiring

- `void StandaloneEditor::resetProjectState()` added to .cpp after `closeAllDynamicTabs` — zeroes all 8 counters back to 1.
- `void StandaloneEditor::advanceCountersFromRestoredTabs()` added to .cpp — scans `mPages` post-deserialize, parses the trailing numeric suffix from each tab's display name per type (handles `Layer N`, `Bass N`, `Drum N`, `Vox N`, `Clip N`; for `TabType::Inst` walks all three prefixes `Inst N` / `Guitar N` / `Basses N`), advances each counter to `max(found) + 1`.
- `resetProjectState()` wired into `closeAllDynamicTabs` after the existing teardown loop, before `setProjectLoadInProgress(false)`.
- `advanceCountersFromRestoredTabs()` wired into the end of `deserializeUIState` (after the final `mRibbon->selectTab(preferred)` call) so saved-project loads pick up restored numbering and resume monotonic past max.

---

### 2026-05-10 — Test E failure mid-Jeff-verify

- Test E (from Task 2's verification plan): piano-roll context label should show `"{tabName} - {engineType}"` per CLAUDE.md 5F-6 design.
- Tests A-D all passed (counter monotonicity post-delete, counter reset on new project, counter advance from saved-project load, Sub-D plural `Basses N` disambiguation).
- Jeff observed Test E shows just `"{tabName}"` with no ` - engine` suffix.
- **Diagnosis:** [`PianoRollPage::registerEngine`](Source/Standalone/PianoRollPage.cpp:91) calls `setContextLabel(conn.displayName)` with the bare display name.  [`PianoRollPage::setEngineDisplayName`](Source/Standalone/PianoRollPage.cpp:141) does the same.  The per-page `LayersPage`/`BassPage`/`DrumPage::refreshPianoRollContextLabel` helpers DO correctly compose `"{tabName} - {engineType-or-(no engine)}"` — but they operate on each page's INTERNAL `mPianoRoll`, which is dead state post-D-5 (the user sees the unified `PianoRollPage`'s container, not the per-page one).
- **Origin:** pre-existing bug from the D-5 unified-piano-roll-page consolidation, not introduced by Task 2.  Surfaced now because Task 2's Test E paired tab-name changes with a piano-roll-label verification check.

---

### 2026-05-10 — Scope-pivot spec call + new memory rule

- I (Claude) initially proposed routing Test E with three options: (a) new batch, (b) QA-Audit "Pre-release decisions to revisit" docket, (c) fold the fix into Task 2.  Recommended (b) as deferral with "cosmetic-only" framing.
- Jeff overruled: *"QA is to find the bugs and take care of it, not suggest maybe in the future fixing the bug would be a cool idea."*  Folded fix into Task 2 (option c) — real bugs found mid-QA-batch get fixed in batch.
- Memory rule locked: `feedback_qa_batches_fix_bugs_dont_defer.md` — the default for any real bug surfaced mid-QA-batch is fix-in-batch.  Deferral requires explicit justification + Jeff's call.

---

### 2026-05-10 — Task 2.6 (folded fix) source edits applied (uncommitted)

Piano-roll context-label composition moved into the unified `PianoRollPage` so the engineType suffix appears on the label the user actually sees.

#### `PianoRollPage.h` ([Source/Standalone/PianoRollPage.h](Source/Standalone/PianoRollPage.h))

- Added `juce::String engineType` field to the `PianoRollConnection` struct.
- Added public method declaration `void setEngineType(EngineId id, const juce::String& engineType)`.

#### `PianoRollPage.cpp` ([Source/Standalone/PianoRollPage.cpp](Source/Standalone/PianoRollPage.cpp))

- Added file-scope helper `static juce::String composeContextLabel(const PianoRollConnection& conn)` — returns `displayName + " - " + (engineType.isEmpty() ? "(no engine)" : engineType)`.
- Updated `registerEngine` to use the helper.
- Updated `setEngineDisplayName` to recompose the label using the stored `engineType`.
- Added `setEngineType` method body — updates `engineType` on the stored connection + recomposes the label via the helper.

#### `StandaloneEditor.cpp` ([Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp))

- Wired 15 `onEngineSelected` callbacks (across default ctor, spawnDuplicate, onAddTabRequest, spawnTemplate, drums-from-file, deserialize-restore) to call `mPianoRollPage->setEngineType({EngineKind::Xxx, pageIdx}, p->getEngineType())` immediately after the existing `wireEngineDirtyHook` call.
- Seeded initial `conn.engineType = lp->getEngineType()` (and `bp` + `dp` variants) inside `registerLayerPianoRoll` / `registerBassPianoRoll` / `registerDrumPianoRoll` so deserialize-restore paths (where the engine state is already set when register is called) pick up `engineType` at registration time, not on a later setEngineType callback.

#### Diff totals

- 4 files: PianoRollPage.h, PianoRollPage.cpp, StandaloneEditor.h, StandaloneEditor.cpp.
- ~281 insertions / ~43 deletions combined across Tasks 2.1-2.6.

#### Verification

- Awaiting Jeff's build + Tests A/B/C/D regression check (must still pass) + Tests E/F re-verify (E: context label now shows engineType suffix; F: deserialize-restore path also picks it up).

---

### 2026-05-10 — Task 2.7 (folded fix) — Guitars/Basses context-label fix shipped + verified

Jeff verified Task 2.6 in Debug.  Tests A-D + Test E for Layer/Bass passed; Test E for Guitars/Basses failed with two distinct issues:

1. Tab name showed `Inst N` instead of `Guitar N` / `Basses N` in the piano-roll context label.
2. The engineType suffix was missing entirely.

#### Root cause

- [`addBaySickGuitarsTab`](Source/Standalone/StandaloneEditor.cpp) and [`addBaySickBassesTab`](Source/Standalone/StandaloneEditor.cpp) call `registerInstSourcePianoRoll(ip)` BEFORE renaming the tab from `Inst N` -> `Guitar N` / `Basses N`, so `PianoRollPage` receives the stale `displayName` at registration time.
- [`registerInstSourcePianoRoll`](Source/Standalone/StandaloneEditor.cpp:7178) wasn't setting `conn.engineType` at all — Task 2.6 only seeded the field in the Layer/Bass/Drum `register*` helpers; the Inst-source variant was missed.

#### Fix shipped

- [`registerInstSourcePianoRoll`](Source/Standalone/StandaloneEditor.cpp:7178) now sets `conn.engineType` to `"BaySickGuitars"` or `"BaySickBasses"` based on `ip->getSource()`.
- In [`addBaySickGuitarsTab`](Source/Standalone/StandaloneEditor.cpp) and [`addBaySickBassesTab`](Source/Standalone/StandaloneEditor.cpp), after the post-register rename to `Guitar N` / `Basses N`, added a call to `mPianoRollPage->setEngineDisplayName({EngineKind::BaySickGuitars, newIdx}, tabName)` (or `EngineKind::BaySickBasses`) so the stored `displayName` updates to match the ribbon name and the label recomposes.

#### Files

- [Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp) (3 edits — `registerInstSourcePianoRoll` engineType seed + post-rename `setEngineDisplayName` push in both addBaySick* helpers).

#### Verification

- Jeff verified in Debug: `Guitar 1 - BaySickGuitars` and `Basses 1 - BaySickBasses` labels now render correctly post-add.

---

### 2026-05-10 — Task 2.8 (folded fix) — Ribbon-rename propagation to piano-roll context label shipped + verified

Jeff verified Task 2.7; flagged a separate bug — when a tab is renamed via the ribbon's rename UI (right-click -> Rename -> type new name), the new name propagates to the ribbon + mixer strip + effects page, but NOT to the piano-roll context label.

#### Root cause

- The `onTabRenamed` ribbon handler at [Source/Standalone/StandaloneEditor.cpp:1221](Source/Standalone/StandaloneEditor.cpp:1221) had a comment at line 1252 saying it should sync to "mixer strip name AND piano-roll context label", but the implementation only did the mixer-strip half — the piano-roll-label sync was never wired.
- The handler also only handled the Layer/Bass/Drum branches; Inst, Clip, and Vox branches were missing entirely.

#### Fix shipped

- The handler now calls `mPianoRollPage->setEngineDisplayName({EngineKind::Layer/Bass/Drum, pageIdx}, finalName)` for the Layer/Bass/Drum branches.
- Added Inst branch — dispatches to `EngineKind::BaySickGuitars` or `EngineKind::BaySickBasses` based on `ip->getSource()`; LiveInput Inst tabs skip the piano-roll-label push (they don't register with `PianoRollPage`).
- Added Clip branch — `EngineKind::Clip`.
- Added Vox branch — only `vp->setTabName(finalName)`; Vox tabs don't register with `PianoRollPage` (per G-4 the Vox piano-roll registration was deleted).

#### Scope-limit noted

- [`MixerPage::renameChannel`](Source/Standalone/MixerPage.cpp) only supports `StripKind::Layer/Bass/Drum`.  Mixer-strip rename for Inst/Vox/Clip already works through a different path (Jeff confirmed post-rename behavior).  The `onTabRenamed` handler does NOT call `renameChannel` for those types because the enum doesn't expose them.  If a future audit finds that mixer-strip rename path is broken for Inst/Vox/Clip, the `StripKind` enum extension would be a separate task — not folded into QA-D.

#### Files

- [Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp) (1 edit covering all 5 page-type branches in the rename handler).

#### Verification

- Jeff verified in Debug: rename via right-click -> Rename now propagates to the piano-roll context label too, with the engineType suffix preserved.

---

### 2026-05-10 — Task 2 verify complete — ready for commit

All Tests A-G pass in Debug:

- **Test A** — counter monotonicity per type, delete-then-re-add does not reuse freed slot index.
- **Test B** — counter reset on File -> New Project.
- **Test C** — counter advance from saved-project load past max-restored.
- **Test D** — Sub-D plural `Basses N` disambiguation from `Bass N`.
- **Test E** — context-label engineType suffix across all 5 engine-bearing types: `Layer 1 - Harmless` / `Bass 1 - BaySickBass` / `Drum 1 - VibePlayer` / `Guitar 1 - BaySickGuitars` / `Basses 1 - BaySickBasses`.
- **Test F** — saved-project restore preserves correct labels (engineType + tab name both round-trip).
- **Test G** — ribbon-rename propagates to piano-roll context label with engineType suffix preserved.

#### Diff totals

- 5 files: PianoRollPage.h, PianoRollPage.cpp, StandaloneEditor.h, StandaloneEditor.cpp, and the running-notes file.
- 451 insertions / 43 deletions across Tasks 2.1-2.8.

#### Release verify

- Deferred to batch close per S7 (paired Release-build verification at end-of-batch) + the QA-D plan.

#### Next

- Commit Task 2 (single commit per S6).
- Then Task 4 — MenuBarModel listener-dangle fix (PianoRoll.h:645-646 / BuilderPage.h:750-751 / DrumKitGrid.h:494-495 declaration-order swap + defensive destructor cleanup).
