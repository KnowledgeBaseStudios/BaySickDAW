# QA-DirtyFlag — Transaction-pointer dirty tracking (undo-aware clean state) — Plan (clean-pointing-stoat)

> **MERGED INTO QA-UndoCoverage (Jeff's ruling, 2026-08-06, at QA-Layout close).**  This batch
> no longer runs standalone — its scope executes inside `long-rewinding-yak.md`, whose 2026-08-06
> ruling banner carries the merged premises.  TWO of this plan's premises are superseded there:
> the "deliberately NOT undoable" structural-ops class is REVOKED (every action is undoable, via
> engine-state snapshot temp files on the PagePresetIO serializer), and with it the dual
> structural counter loses its reason to exist — dirty = transaction-pointer mismatch alone
> should suffice (verify at the merged plan-open).  The transaction-pointer spec itself
> (Jeff's 2026-05-23 verbatim block: currentUndoStep/savedUndoStep, branch-kill, save sync,
> asterisk clears when Ctrl+Z lands on the save point) STANDS and executes in the merged batch.
> This file stays as the spec record; do not execute from it.

> **Canonical path:** `Plans & Specs/Batch Plans/clean-pointing-stoat.md` (mirrored at G4 group
> approval; home-dir copy deleted). **For execution:** ~~bulk-run G4 batch 8 of 8 — the last
> code batch before the campaign~~ MERGED into batch 7 — see the banner above. §B authored at
> code-complete; one source commit.

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

## Conflict-review note — 2026-07-27 (QA-ModelShell inserted upstream)

**QA-ModelShell** (`grand-inverting-mammoth.md`) now runs between QA-ProjectSave and
QA-UndoCoverage (G4 order: … badger → mammoth → yak → stoat → heron). Review outcome for
this plan (intent intact, mechanics adjusted):

1. **Task 3 lands cleaner, its scout is stale.** Post-inversion, tab add/delete/duplicate,
   engine pick, and kit load are MODEL operations — the "one processor hook" this plan
   already specifies is exactly where they now live. The scouted RibbonTabBar/page-spawn
   `markDirty` map predates the inversion; re-scout at batch open against the post-mammoth
   tree.
2. **New structural-op dispositions from mammoth:** window open/close counts as NOTHING
   (windows are disposable views; engines/model untouched). Freeze/unfreeze dirty semantics
   arrive ANSWERED from mammoth TS7's freeze spec call — consume that ruling, do not
   re-derive it here.
3. **Task 2's wiring refs move:** the `installEngineDirtyHook` sites ride engine creation
   into the model factory. Refs stale; the strip-the-dirty-half intent is unchanged.
4. **Exclusion surface grew:** the offline render's automation application + LUFS-normalize
   writes must never dirty the project (pairs with QA-UndoCoverage's history exclusion).
   This batch owns the dirty evaluator, so its §B verification adds an "export a song →
   project stays clean" scenario.
