# QA-UndoCoverage — MERGED batch: one global undo (every ACTION undoable, structural ops via snapshot resurrection) + transaction-pointer dirty tracking — Plan (long-rewinding-yak)

> **REVISED 2026-08-06 at QA-Layout close — the two Jeff rulings of that date ARE APPLIED in
> this revision; the task bodies below are current.**
> **(1) QA-DirtyFlag IS MERGED IN.** One batch, one plan, one close; `clean-pointing-stoat.md`
> is absorbed (its banner points here; its 2026-05-23 verbatim transaction-pointer spec STANDS
> and executes as Tasks 8-9 below). The G4 order: ... layout -> yak (merged) -> heron.
> **(2) EVERY ACTION IS UNDOABLE — the "dead-owner model" exclusion is REVOKED.** Jeff's
> original spec said every action; the narrowing to "every edit, structural ops excluded" was
> baked pending-veto on 2026-07-25 and never posed to him (the Rule 5 violation pattern).
> Structural ops (tab add/delete/duplicate, engine pick, kit load) are undoable via
> ENGINE-STATE SNAPSHOT temp files captured at the destructive edge — Jeff's design direction:
> "temp files that screenshot the players like our page saves do".  VERIFIED against
> `PagePresetIO.h` (2026-08-06, after Jeff corrected an understated first reading): the page
> preset already captures the END-TO-END CHAIN — every engine state, every mixer-strip APVTS
> param (sends included), the insert rack, both pre + post EQ8 M/S, and owned bus racks — so it
> is the SPINE of the undo snapshot.  The undo wrapper adds only what presets deliberately
> exclude: the instance's piano-roll/pattern content, automation lanes targeting it, INBOUND
> references from other strips (their sends/sidechain picks at this channel), and an
> IDENTITY-PRESERVING restore mode (the preset loader deliberately rewrites id prefixes for
> load-into-another-page; undo needs the same index/ids/uuids back so lanes and windows
> re-resolve).
> **STRUCTURAL CONSEQUENCES APPLIED BELOW:** (a) docket 13's "dead-owner hidden + auto-skipped"
> half is SUPERSEDED — under linear undo a deleted tab's edits are unreachable without first
> undoing the delete, which resurrects the tab, so dead transactions cannot exist and the
> hide/skip machinery is not built (owner keys survive as history LABELS only); (b) stoat's
> dual structural counter is DROPPED — structural ops transact now, so dirty = transaction-
> pointer mismatch alone; (c) `PagePresetIO::kitLoadCallback` already provides the race-safe
> sfizz re-entry the resurrection path needs (it exists because sfizz crashes on mid-render
> reload).

> **Canonical path:** `Plans & Specs/Batch Plans/long-rewinding-yak.md` (mirrored at G4 group
> approval; home-dir copy deleted). **For execution:** bulk-run G4 batch 7 (merged; heron
> follows, then the G4 boundary). §B authored at code-complete; one source commit.

## Context

Premise reshape (scout + desk verification 2026-07-25): the marathon-era "audit 477 setProperty
sites" work item is void — that population is serialization-only, and PatternManager's live
model is C++ structs with no ValueTree ([PatternManager.h:807-808](Source/PatternManager.h:807)
holds only serializer decls). What exists instead: a healthy central snapshot-action system on
`StandaloneEditor::mUndoManager` (decl .h:341, ctor `(100,30)` .cpp:549) with one choke point
(`doUndoAction` :9994-10015) + `UndoContext` distribution — piano roll, drum grid, arrangement,
mixer, effect rack, effect knobs, pitch editor all coalesce per-gesture already.

Verified gaps: main APVTS + 7 engines pass `nullptr` UndoManager
([PluginProcessor.cpp:377](Source/PluginProcessor.cpp:377)); Guitars/Basses own managers with
ZERO reachable consumers; RustyDrums' manager reachable only via the visible-page branch in
`globalUndo` (:10017-10042); the Event Editor calls the manager DIRECTLY (bypassing
`doUndoAction` -> history labels + cursor desync; local Ctrl+Y redo vs the app's Ctrl+Alt+Z);
~101 manual `setValueNotifyingHost` pushes; and a set of PatternManager user gestures reach
DIRTY but never UNDO (pattern add/duplicate/remove/rename, time-marker + time-sig +
tempo-change ops, Builder audio-library ops, automation-template ops).

The absorbed DirtyFlag context (verified, from `clean-pointing-stoat.md`): `ApvtsDirtyTracker`
fires `markDirty` on every property write; `ProjectManager::markDirty` (+~18 direct sites)
sets an unconditional bool; the three undo-path `markDirty` calls mean undo can only ever
DIRTY the project today — the exact inversion Tasks 8-9 fix.  Readers: title asterisk,
`confirmDiscardChanges`, quit gate.

- **Risk:** HIGH (was medium-high pre-merge). The parallel `mHistoryLabels` list assumes every
  transaction flows through `doUndoAction`; wiring the APVTS UndoManager makes attachments
  create transactions OUTSIDE it — the label list, cursor, and history window need a
  listener-driven sync layer, and automation/programmatic writes must be excluded from
  history. Task 1 opens with a bounded source-reading spike on the vendored APVTS. The new
  structural-undo subsystem (Task 7) touches engine lifecycle — the sfizz teardown-order and
  async-load rules apply in full.
- **Effort:** ~20-30 h honest for the merged whole (old yak 8-12 + stoat 4-7 + structural
  undo, the largest single addition; the QA-ModelShell conflict note's growth stands).
- **Dependencies:** QA-ModelShell (model factory, model-side registration) + QA-Layout
  (T21 windows resolve by uuid/persist-key — resurrection must restore the same identities).
- **ORDERING RULE (load-bearing):** widget-captured undo actions move to MODEL-ADDRESSED
  actions BEFORE structural resurrection lands (Tasks 1-6 before Task 7), or redo across a
  resurrection replays edits into dead widgets — the same class of bug model-side automation
  registration was built to kill.

## Spec calls already locked

| ID | Decision | Reasoning |
|----|----------|-----------|
| Jeff 2026-08-06 | EVERY ACTION undoable; structural ops via identity-preserving snapshot resurrection on the PagePresetIO spine; the 2026-07-25 structural exclusion REVOKED (never posed) | Ruled at QA-Layout close; Main Plan §9 sixty-ninth |
| Jeff 2026-08-06 | QA-DirtyFlag merges in; one batch, one close; dual structural counter dropped (structural ops transact) | Same ruling |
| §5 verbatim spec (stoat) | TransactionTracker: current/saved pointer, branch-kill (saved=-1 past an undone save point), save sync, asterisk clears the instant Ctrl+Z lands on the save point, dirty = pointer mismatch | Jeff, locked 2026-05-23 — STANDS |
| Marathon 9b | PROCESSOR-owned UndoManager handed to the main APVTS at construction; StandaloneEditor's manager retires in favor of it; param changes undoable + count toward the dirty pointer | Locked 2026-07-08 |
| Docket 12=A | (i) APVTS wiring, (ii) gap wrapping of the dirty-but-not-undo gestures, (iii) Event Editor unification, (iv) manual-push gesture bracketing | Jeff 2026-07-25 |
| Docket 13=A+ii | ONE manager everywhere — all 10 engine APVTSes take the global manager; the 3 private managers + the Rusty-only branch retire. ~~Dead-tab transactions: HIDDEN + auto-skipped~~ **SUPERSEDED 2026-08-06** — no dead transactions can exist under every-action undo (see banner); owner keys survive as history labels only | Jeff 2026-07-25 / 2026-08-06 |
| Docket 14=a | Event Editor adopts Ctrl+Alt+Z redo; its keys get a DISPLAY-ONLY section in the Key Binds window | Jeff 2026-07-25 |
| Marathon 9a | Live-state mutations only; detached-tree serialization untouched | Locked 2026-07-08 |

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open.  Convention-derived picks stated plainly (not new spec): snapshot temp
files live under `Documents/BaySickDAW/UndoSnapshots/` (existing folder convention), are
session-scoped (undo history dies with the session; files swept at startup + exit and dropped
as entries fall off the depth cap); a kit load is ONE undo entry (one user gesture — the
existing per-gesture coalescing convention); undo of a sample-engine resurrection is a LOAD,
not an instant snap-back (async loads; Jeff accepted this property knowingly 2026-08-06).
Execution-time confirmations that go to Jeff IN CHAT if they arise: spike findings that
contradict the exclusion mechanism; autosave dirty semantics (verify current behavior first,
change nothing unprompted).

## Files to modify

- Task 1: `JUCE/modules/juce_audio_processor…/juce_AudioProcessorValueTreeState.cpp`
  (READ-ONLY spike), `Source/PluginProcessor.h/.cpp` (UndoManager member before apvts; ctor
  arg; accessor), `Source/Standalone/StandaloneEditor.h/.cpp` (manager retire -> repoint
  UndoContext/doUndoAction/globalUndo/globalRedo/history window/menu depth 510-513)
- Task 2: all 10 engine processor ctors (+ their creation sites in
  PluginProcessor/LayersPage/BassPage/DrumPage/StandaloneEditor Inst-Vox paths);
  `Source/BaySickGuitars|BaySickBasses|BaySickRustyDrums/*Processor.h/.cpp` (private managers
  removed), `Source/Standalone/StandaloneEditor.cpp` (Rusty globalUndo branch removed)
- Task 3: `Source/Standalone/StandaloneEditor.h/.cpp` (shadow list + owner keys + hide/skip),
  `Source/Standalone/SharedUI.cpp` (gesture-start transaction naming hook at the attachment
  entry points)
- Task 4: `Source/Standalone/UndoActions.h` (new scoped actions), call-site wraps in
  `Source/Standalone/BuilderPage.cpp` (browser/library/marker entries),
  `Source/Standalone/StandaloneEditor.cpp` (pattern ops, renames)
- Task 5: `Source/Standalone/EventEditor.cpp/.h` (route via ctx; Ctrl+Alt+Z),
  `Source/Standalone/KeyBindsWindow` site (display-only EE section)
- Task 6: audit-driven manual-push sites (MixerPage/StandaloneEditor/NAMIR editor top of list)
- Task 7: `Source/Standalone/UndoActions.h` (StructuralOpAction), `Source/Standalone/PagePresetIO.h/.cpp`
  (identity-preserving restore mode), `Source/EngineRig.h/.cpp` + model tab add/delete/dup +
  engine-pick + kit-load entry points (snapshot capture at the destructive edge), PatternManager
  (per-instance note/lane slice capture), snapshot temp-file store under Documents/BaySickDAW/UndoSnapshots/
- Task 8: `Source/PluginProcessor.h/.cpp` (TransactionTracker beside the UndoManager),
  `Source/Standalone/StandaloneEditor.cpp` (the three undo-path markDirty calls removed; wrappers feed the tracker)
- Task 9: `Source/ProjectManager.cpp/.h` (markDirty -> dynamic evaluator; save sync; load reset),
  `Source/Standalone/ApvtsDirtyTracker.h` + engine wiring sites (dirty half stripped; audio-gate half kept if consumed),
  dirty consumers (title asterisk, confirmDiscardChanges, quit gate) read-through verification

## Tasks

### Task 1 — Authority move + the transaction-semantics spike

- [ ] SPIKE (read-only, bounded): read the vendored APVTS source and pin: (a) does the
  parameter->tree flush write properties WITH the ctor UndoManager (i.e. do host/automation
  `setValue` calls end up as transactions?); (b) what transaction naming attachments produce;
  (c) whether `beginNewTransaction` is called per gesture by each attachment type. Findings ->
  running notes; they select the exclusion mechanism below.
- [ ] `VibeSynthProcessor` owns `juce::UndoManager mUndoManager` (declared BEFORE apvts),
  passed at apvts construction; public accessor.
- [ ] StandaloneEditor's `mUndoManager` member retires: `makeUndoContext`, `doUndoAction`,
  `globalUndo/globalRedo`, history window, depth menu (510-513) all repoint to the processor's.
- [ ] **Automation/programmatic exclusion:** applicator + audio-thread + baseline-restore param
  writes must NOT create history. Mechanism per spike findings — either those paths write via a
  transaction-suppressed scope (drop + re-begin pattern around the applicator pass) or, if the
  spike shows flushes don't transact, document that and guard only the explicit UI entry
  points. Undo/redo history must stay byte-identical across a played song with lanes.
- [ ] Build gate.

### Task 2 — Engines join the one history (13=A)

- [ ] All 10 engine ctors accept `juce::UndoManager&` and hand it to their apvts; every
  creation site passes the processor's manager.
- [ ] Guitars/Basses/RustyDrums private managers + `getUndoManager()` accessors removed; the
  Rusty visible-page branch in `globalUndo/globalRedo` removed (its ARIA attachments now
  transact into the global history and Ctrl+Z reaches them anywhere).
- [ ] Build gate.

### Task 3 — Shadow list + owner-key labels (13=ii as superseded 2026-08-06)

- [ ] Replace the write-side `mHistoryLabels` appends with a listener-driven shadow list:
  UndoManager ChangeListener detects new transactions (top undo-description changed while redo
  emptied) and appends {label, ownerKey}; `doUndoAction` keeps naming its own transactions
  (`<owner>|<label>` convention); attachment gestures get named at drag-start via a small hook
  at the attachment creation sites (editor knows its owner key).
- [ ] Owner keys: tab-scoped (`lay0`, `drm3`, `vox1`, `rusty`, ...) for engine/page surfaces;
  `app` for global surfaces.  LABELS ONLY — the dead-set / hide / skip machinery is NOT built
  (superseded: under every-action undo a deleted tab's edits sit below its delete entry and
  cannot be reached without resurrecting the tab first; no dead transaction can exist).
- [ ] Depth-menu semantics preserved (`setMaxNumberOfStoredUnits` values unchanged).
- [ ] Build gate.

### Task 4 — Wrap the dirty-but-not-undo gestures (12-ii)

- [ ] New scoped actions in UndoActions.h (small before/after payloads, NOT full-PM snapshots):
  `PatternListAction` (add/duplicate/remove/rename — payload = the one Pattern + index),
  `MarkerSetAction` (time markers + time-sig + tempo-change lists), `AudioLibraryAction`
  (library entries + manual groups + clip defaults), `AutomationTemplateAction`.
- [ ] Wrap the UI entry points (BrowserPanel pattern ops, ruler/marker menus, library context
  menus, rename edits) via the existing `UndoContext::perform` pattern. Row renames: verify
  ArrangementEditAction already captures them (scout says rowNames ride it) — wrap only if a
  path bypasses commitEdit.
- [ ] Each wrapped op fires the same notifyContentChanged it does today (no behavior change
  beyond undoability).
- [ ] Build gate.

### Task 5 — Event Editor unification (12-iii + 14=a)

- [ ] EventEditor's direct `mUM.beginNewTransaction/perform` sites route through the
  UndoContext choke point (labels + cursor + history window now truthful for lane edits);
  its ChangeListener repaint stays.
- [ ] Local keymap: redo = Ctrl+Alt+Z (Ctrl+Y removed); Edit-menu labels updated.
- [ ] Key Binds window: new display-only "Event Editor" section listing its local keys
  (no Set buttons; ASCII).
- [ ] Build gate.

### Task 6 — Manual-push gesture bracketing (12-iv)

- [ ] Audit the ~101 `setValueNotifyingHost` sites; classify user-gesture vs programmatic.
  User-gesture sites get a named transaction bracket (begin + name at gesture start) so they
  coalesce + label properly; programmatic sites (applicators, load paths, sync code) are
  covered by Task 1's exclusion. Audit table -> running notes. (Census is stale post-mammoth
  and post-layout — re-scout, do not trust the number.)
- [ ] Build gate.

### Task 7 — Structural undo: snapshot resurrection (Jeff's 2026-08-06 ruling)

- [ ] `StructuralOpAction` in UndoActions.h: capture at the destructive edge = the per-tab
  slice via `PagePresetIO` (the chain spine: engines, strip params incl. sends, insert rack,
  pre+post EQ8 M/S, owned bus racks) PLUS the four preset-excluded surfaces: the instance's
  piano-roll/pattern content, automation lanes targeting it, inbound references from other
  strips (their sends/sidechain picks at this channel), and the identity record (pageIndex,
  strip/channel id, slot uuids, persist keys).
- [ ] `PagePresetIO` gains an IDENTITY-PRESERVING restore mode (no prefix rewrite; restore
  into the SAME index/ids/uuids so automation lanes and the T21 windows re-resolve).
- [ ] Wrapped ops, one transaction per user gesture: tab add (undo = delete), tab delete
  (undo = resurrect from snapshot), tab duplicate, engine pick/swap (undo = restore the prior
  engine's snapshot), kit load (ONE entry for the whole kit; undo = remove the created tabs;
  redo = reload).  Resurrection enters through the MODEL creation paths (the restore-walker
  pattern; Vox/Inst/Clips enter strip-first per their creation order; sfizz engines re-enter
  via `kitLoadCallback` — the race-safe path that exists because sfizz crashes on mid-render
  reload).  Teardown-order rules (Carry-Forward §2 + the sfizz notes) apply in full.
- [ ] Snapshot temp files under `Documents/BaySickDAW/UndoSnapshots/`: written at capture,
  dropped as entries fall off the depth cap, swept at startup + exit.  Session-scoped —
  project load clears history and the store.
- [ ] UX property (accepted by Jeff 2026-08-06): undoing a sample-engine deletion is a LOAD
  (async), not an instant snap-back; the existing load/progress surfaces cover it.
- [ ] Build gate.

### Task 8 — TransactionTracker (absorbed stoat T1, dual counter dropped)

- [ ] `TransactionTracker` on the processor beside the manager: `current`/`saved` only —
  structural ops transact now, so the `structural`/`structuralSaved` pair from the stoat plan
  is NOT built.  Fed by this batch's transaction events: new transaction -> ++current; undo ->
  --current; redo -> ++current; new transaction while current<saved -> saved=-1 (branch-kill).
- [ ] Remove the three undo-path `markDirty` calls; `doUndoAction`/`globalUndo/Redo` notify
  the tracker instead.
- [ ] `isDirty()` = `(current != saved)`; `onDirtyChanged` edge-fires so the asterisk updates
  the moment the pointer crosses the save point in either direction.
- [ ] Build gate.

### Task 9 — Retire the touch-model + consumer verification (absorbed stoat T2-T4)

- [ ] `ProjectManager::markDirty`'s unconditional-bool model retired; save ->
  `saved = current`.  `mIgnoreDirty` load-guard semantics preserved: project load clears
  history + the snapshot store, resets the counters, fires clean.
- [ ] `ApvtsDirtyTracker`'s `onAny -> markDirty` wiring removed at the engine sites (refs
  moved into the model factory post-mammoth — re-scout).  The class's lock-free
  `hasChangedSinceLastBlock` audio gate is a SEPARATE consumer — verify who reads it; keep
  that half, strip only the dirty half; remove the class whole if nothing else consumes it
  (grep-driven).
- [ ] The ~18 direct `markDirty` sites: user gestures are transactions now (redundant ->
  removed); the remainder map to Task 7 ops or load-path guards.  Disposition table ->
  running notes.
- [ ] Autosave: verify current behavior FIRST (does autosave clearDirty today?); replicate
  exactly against the tracker (change nothing unprompted; note the finding).  Freeze/unfreeze
  dirty semantics consume mammoth TS7's ruling — do not re-derive.
- [ ] Consumers re-verified against the dynamic evaluator (title asterisk,
  `confirmDiscardChanges`, quit gate — no consumer caches the old bool).
- [ ] Build gate.

## Batch close (bulk-run per-batch loop — one commit per batch)

- [ ] Tell Jeff to run `do_build.bat`; fix until BOTH configs build clean.
- [ ] Author this batch's Master Test Plan §B section (`blocks:` = this commit, backfilled).
- [ ] `/draft-doc batch-close` -> held Work Log entry in running notes (R2); no §5 touch.
- [ ] Running-notes code-complete entry (+ spike findings + audit tables).
- [ ] ONE batch commit (Rule 9): `QA-UndoCoverage: <one-line what> (<scope>)` + trailer;
  message + FULL git status; commit on Jeff's approval.

## Verification (authors into Master Test Plan §B)

1. Multi-surface unwind: place notes -> move a mixer fader -> load a rack effect -> add a
   pattern -> add a tempo marker -> rename a pattern -> drag a Harmless macro. Seven Ctrl+Z
   presses restore the exact starting state, in reverse order; Ctrl+Alt+Z replays all seven.
2. Knob drag on any engine editor = ONE history row, labeled with the tab + control name;
   double-click reset = one row.
3. Play a song with engine + mixer automation lanes for 30 s: the history window gains ZERO
   rows during playback (programmatic exclusion).
4. STRUCTURAL UNDO (the 2026-08-06 ruling, end to end): two Layers tabs, edit knobs on both,
   delete tab 2 -> Ctrl+Z resurrects it — same tab position, same engine settings, same rack +
   pre/post EQ, same strip params and sends, its piano-roll notes back, its automation lanes
   playing, its window reopening where it was; a further Ctrl+Z then reaches the knob edits
   made BEFORE the delete.  Redo re-deletes.  Same round-trip for: tab add (undo removes it),
   tab duplicate, engine pick/swap (undo restores the prior engine's full state), and a kit
   load (ONE history entry; undo removes all its tabs; redo reloads — expect a load wait, not
   an instant snap-back).
5. Rusty ARIA knob edit undoes from ANY page via Ctrl+Z (the visible-page restriction is gone).
6. Event Editor: draw + move + delete lane points -> each appears in the history window;
   Ctrl+Z inside the editor undoes them; redo is Ctrl+Alt+Z (Ctrl+Y does nothing); the Key
   Binds window shows the Event Editor section.
7. Pattern remove undo restores the pattern WITH its notes at the same index; marker-set undo
   restores marker + the played tempo (readout check).
8. Undo History window depth setting (100/250/500/1000) still applies; labels match actions.
9. Load a project: history is EMPTY (loads don't transact), the snapshot store is swept;
   first post-load edit is row 1.
10. DIRTY POINTER (absorbed stoat spec): the origin repro — Solo on, Solo off (via Ctrl+Z x2
    OR direct re-toggle) -> NO asterisk once state matches the save.  Edit -> Save -> edit x3
    -> Ctrl+Z x3 -> asterisk clears exactly at the save point; one more Ctrl+Z past it ->
    asterisk returns.
11. Branch-kill: edit -> Save -> Ctrl+Z -> make a DIFFERENT edit -> Ctrl+Alt+Z is dead and
    the project stays dirty through any further undo/redo until the next Save.
12. Add a tab, nothing else: dirty (it transacted).  Save: clean.  Delete the tab: dirty.
    Ctrl+Z (resurrect): asterisk CLEARS — the pointer is back at the save point, which is the
    whole reason the dual counter could be dropped.
13. Play a song with lanes 30 s (no hand edits): history gains ZERO rows AND the project
    stays clean.  Export a song offline: history byte-identical, project stays clean.
14. Quit gates: load -> quit immediately, no prompt; one edit -> prompt; autosave behavior
    identical to the Task 9 verification note.

## Routing notes (Rule 3)

The DirtyFlag handover is DISSOLVED by the merge — dirty-pointer semantics execute here as
Tasks 8-9.  Spike findings that contradict the exclusion mechanism, and any autosave-semantics
surprise, get surfaced to Jeff in chat BEFORE re-shaping (ask-always lock).  Any gesture found
not transacting is a coverage bug: fix in-batch.  This batch does NOT close G4 code — heron
(QA-Soundness) follows, then the boundary R3 + smoke.

## Carry-Forward Reference touch points

§1 UndoContext/UndoActions inventory (this plan's scout section) + the vendored
`juce_UndoManager.cpp` semantics (transactions, units) before Task 1. Constructor-order rule:
the manager member MUST precede apvts in every processor it's handed to.

## Conflict-review note — 2026-07-27 (QA-ModelShell inserted upstream)

**QA-ModelShell** (`grand-inverting-mammoth.md`) now runs between QA-ProjectSave and this
batch (G4 order: … badger → mammoth → yak → stoat → heron). Boundary locked: the G4 R3
review + smoke covers only yak/stoat/heron; mammoth verifies via its own per-set commits +
TS8 batch smoke. Review outcome for this plan:

1. **Task 2 SHRINKS to verification (Jeff 2026-07-27, conflict call 2=b).** Mammoth TS1
   pre-wires the plumbing: a processor-owned `juce::UndoManager` member (declared BEFORE
   apvts — constructor-order rule) threaded as `UndoManager&` through the model engine
   factory into every engine APVTS ctor, DORMANT — no semantics change, StandaloneEditor's
   manager stays authoritative until this batch. Task 2 here becomes: flip the semantics on
   + verify all 10 engines transact into the one history. The Files-to-modify Task 2 list
   (page creation sites) is dead — creation is the model factory after the inversion.
2. **Task 1's exclusion list grows.** Programmatic writes that must NOT create history now
   include mammoth's model-side automation applicators and the offline render's lane
   application + LUFS-normalize writes (an export must leave history byte-identical).
3. **Dead-owner keys = MODEL tab deletion, not view death.** Post-mammoth, closing a window
   destroys only the view; engines and tabs persist. Only deleting a tab from the model
   kills its owner key. Update the Task 3 wording at open.
   *(2026-08-06: doubly superseded — the dead-key machinery is not built at all under the
   every-action ruling; the model-vs-view distinction this point drew still governs Task 7's
   capture points.)*
4. **Task 6's ~101-site census is stale** — mammoth's registration rework moves/removes
   push sites; re-scout at open.
5. **The audit surface grew:** "+"-driven tab adds, rack sidebar gestures, freeze/unfreeze,
   and VST3 slot edits are new user-changeable surfaces this coverage audit must include.
   The ~8-12 h estimate predates them.
