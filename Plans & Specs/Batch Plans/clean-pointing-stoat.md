# QA-DirtyFlag — Transaction-pointer dirty tracking (undo-aware clean state) — Plan (clean-pointing-stoat)

> **Canonical path:** `Plans & Specs/Batch Plans/clean-pointing-stoat.md` (mirrored at G4 group
> approval; home-dir copy deleted). **For execution:** bulk-run G4 batch 8 of 8 — the last
> code batch before the campaign. §B authored at code-complete; one source commit.

## Context

Jeff's locked spec (2026-05-23, §5 verbatim block): replace "anything touched since load" dirty
tracking with an undo-aware transaction pointer — `currentUndoStep`/`savedUndoStep`, undo
decrements / redo increments / new edit increments, save syncs, branch-kill (`savedUndoStep=-1`
when editing past an undone save point), dirty = pointer mismatch, asterisk clears the instant
Ctrl+Z lands on the save point. QA-UndoCoverage (previous batch) supplies the substrate: one
global manager, listener-driven transaction events, programmatic-write exclusion.

Current state to replace (verified): `ApvtsDirtyTracker` fires `markDirty` on every property
write; `ProjectManager::markDirty` (+~18 direct call sites) sets an unconditional bool;
`doUndoAction`/`globalUndo`/`globalRedo` themselves call `markDirty`
(StandaloneEditor.cpp:10011/:10040/:10058) — undo can only ever DIRTY the project today, the
exact inversion this batch fixes. Readers: title asterisk (refreshWindowTitle :13098),
`confirmDiscardChanges` early-out, quit gate.

One real design hole the pointer model opens (stated for R5): structural ops that are
deliberately NOT undoable (tab add/delete/duplicate, engine pick, kit load — the dead-owner
model from QA-UndoCoverage) would leave the pointer untouched -> a project with only a new tab
would read CLEAN. Fix = a second counter: `structuralEdits`/`structuralSaved` bumped by those
ops; dirty = pointer mismatch OR structural mismatch. Undo never crosses structural ops, so the
two counters stay independent and the semantics stay honest.

- **Risk:** medium. Every dirty consumer re-reads through one evaluator; load/save guard
  semantics must not drift.
- **Effort:** ~4-7 h (down from ~10-16; the plumbing half moved to QA-UndoCoverage).
- **Dependencies:** QA-UndoCoverage (hard — transaction events + exclusion + one manager).

## Spec calls already locked

| ID | Decision | Reasoning |
|----|----------|-----------|
| §5 verbatim spec | TransactionTracker + branch-kill + dynamic evaluation + save sync | Locked 2026-05-23 |
| Marathon 9a | Live-state mutations only; detached-tree serialization untouched | Locked 2026-07-08 (population reshaped per docket 12, intent unchanged) |
| Marathon 9b / 19 | Pointer sits on the processor-owned manager; boundary = tracker on top of coverage | Locked 2026-07-08 |
| — | Dual-counter for non-undoable structural ops; autosave keeps its CURRENT dirty semantics (verified at execution, not changed unprompted) | Baked-pending-veto 2026-07-25 |

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open.

## Files to modify

- Task 1: `Source/PluginProcessor.h/.cpp` (TransactionTracker beside the UndoManager),
  `Source/Standalone/StandaloneEditor.cpp` (undo/redo wrappers feed the tracker; the three
  undo-path markDirty calls removed)
- Task 2: `Source/ProjectManager.cpp/.h` (markDirty model -> dynamic evaluator; save sync;
  load reset), `Source/Standalone/ApvtsDirtyTracker.h` + its 10 engine wiring sites
  (installEngineDirtyHook :8937-8945, :9300)
- Task 3: structural-op sites (tab add/delete/dup, engine pick, kit load — the RibbonTabBar /
  page-spawn markDirty map from the scout)
- Task 4: dirty consumers (refreshWindowTitle, confirmDiscardChanges, quit gate) — read-through
  verification only

## Tasks

### Task 1 — TransactionTracker

- [ ] `TransactionTracker` on the processor beside the manager: `current`/`saved` +
  `structural`/`structuralSaved`; fed by QA-UndoCoverage's transaction events (new transaction
  -> ++current; undo -> --current; redo -> ++current; new transaction while current<saved ->
  saved=-1 branch-kill).
- [ ] Remove the three undo-path `markDirty` calls; `doUndoAction`/`globalUndo/Redo` notify the
  tracker instead.
- [ ] `isDirty()` becomes `(current != saved) || (structural != structuralSaved)`;
  `onDirtyChanged` edge-fires from the tracker so the asterisk updates the moment the pointer
  crosses the save point in either direction.
- [ ] Build gate.

### Task 2 — Retire the touch-model

- [ ] `ProjectManager::markDirty`'s unconditional-bool model retired; `clearDirty`/save ->
  `saved = current; structuralSaved = structural`. `mIgnoreDirty` load-guard semantics
  preserved: project load clears history (QA-UndoCoverage guarantees loads don't transact),
  resets all four counters, fires clean.
- [ ] `ApvtsDirtyTracker`'s `onAny -> markDirty` wiring removed at the 10 engine sites. The
  class's lock-free `hasChangedSinceLastBlock` audio gate is a SEPARATE consumer — verify who
  reads it (scout: audio-thread param-sync gating); keep that half intact, strip only the
  dirty-flag half. If nothing else consumes the class, remove it whole (grep-driven).
- [ ] The ~18 direct `markDirty` call sites: user-gesture ones are already transactions after
  QA-UndoCoverage (their markDirty becomes redundant -> removed); the remainder map to Task 3
  structural ops or load-path guards. Full disposition table -> running notes.
- [ ] Build gate.

### Task 3 — Structural (non-undoable) ops

- [ ] Tab add/delete/duplicate, engine pick, kit load, aux add/remove -> `++structural` (via
  one processor hook). These ops also mark their owner keys dead per QA-UndoCoverage where
  applicable (delete) — no double-counting with transactions (they create none).
- [ ] Autosave: verify current behavior first (does autosave clearDirty today?); replicate it
  exactly against the new counters (change nothing unprompted; note the finding).
- [ ] Build gate.

### Task 4 — Consumer verification

- [ ] Title asterisk, `confirmDiscardChanges`, quit gate, and every other `isDirty()` reader
  re-verified against the dynamic evaluator; no consumer caches the old bool.
- [ ] Build gate.

## Batch close (bulk-run per-batch loop — one commit per batch)

- [ ] Tell Jeff to run `do_build.bat`; fix until BOTH configs build clean.
- [ ] Author this batch's Master Test Plan §B section (`blocks:` = this commit, backfilled).
- [ ] `/draft-doc batch-close` -> held Work Log entry in running notes (R2); no §5 touch.
- [ ] Running-notes code-complete entry (+ disposition tables).
- [ ] ONE batch commit (Rule 9): `QA-DirtyFlag: <one-line what> (<scope>)` + trailer; message +
  FULL git status; commit on Jeff's approval. **This closes G4 code — group boundary follows
  (R3 review over the combined diff + smoke).**

## Verification (authors into Master Test Plan §B)

1. The origin repro: open a project, click Solo on, click Solo off — via Ctrl+Z x2 OR directly
   toggling back — title shows NO asterisk once state matches the save.
2. Drag a knob -> asterisk appears; Ctrl+Z -> asterisk clears instantly. Redo -> reappears.
3. Edit -> Save -> edit x3 -> Ctrl+Z x3 -> asterisk clears exactly at the save point; one more
   Ctrl+Z (past the save) -> asterisk returns.
4. Branch-kill: edit -> Save -> Ctrl+Z -> make a DIFFERENT edit -> Ctrl+Alt+Z is dead (branch
   destroyed) and the project stays dirty through any further undo/redo until the next Save.
5. Add a tab (no other edits): dirty. Save: clean. Delete a tab: dirty again (structural
   counter); knob undo/redo still tracks the pointer correctly alongside it.
6. Play a song with automation lanes 30 s, no hand edits: project stays CLEAN (programmatic
   exclusion holds end-to-end through the tracker).
7. Load a project: clean, empty history; quit immediately: no save prompt. Make one edit,
   quit: prompt appears.
8. Autosave cycle: behavior identical to pre-batch (whatever Task 3's verification recorded).

## Routing notes (Rule 3)

Any gesture found NOT transacting (slips through both counters) is a QA-UndoCoverage coverage
bug — fix in-batch here if trivial (one wrap), otherwise log against the yak §B section for the
campaign walk. This is the last G4 batch: the group boundary (R3 combined-diff review + Jeff's
smoke) runs immediately after this commit per the run plan.

## Carry-Forward Reference touch points

QA-UndoCoverage's running notes (spike findings + exclusion mechanism) are REQUIRED reading
before Task 1 — the tracker's correctness rides on which writes transact. §3 persistence
decisions for the load/save guard semantics.
