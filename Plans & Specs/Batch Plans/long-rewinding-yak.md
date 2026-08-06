# QA-UndoCoverage — One global undo: authority move, engine params, gap wrapping, Event Editor unification — Plan (long-rewinding-yak)

> **RULED 2026-08-06 (Jeff, at QA-Layout close) — TWO CHANGES SUPERSEDING THIS PLAN'S PREMISES.**
> **(1) QA-DirtyFlag MERGES INTO THIS BATCH.** One batch, one plan, one close; `clean-pointing-stoat.md`
> is absorbed (banner there points here). The G4 order becomes ... layout -> yak (merged) -> heron.
> **(2) EVERY ACTION IS UNDOABLE — the "dead-owner model" exclusion is REVOKED.** Jeff's original
> spec said every action; the narrowing to "every edit, structural ops excluded" was baked
> pending-veto on 2026-07-25 and was never posed to him — the Rule 5 violation pattern — and the
> exclusion does not stand. Structural ops (tab add/delete/duplicate, engine pick, kit load)
> become undoable via ENGINE-STATE SNAPSHOT temp files captured at the destructive edge — Jeff's
> design direction: "temp files that screenshot the players like our page saves do".  VERIFIED
> against `PagePresetIO.h` (2026-08-06, after Jeff corrected an understated first reading): the
> page preset already captures the END-TO-END CHAIN — every engine state, every mixer-strip
> APVTS param (sends included), the insert rack, both pre + post EQ8 M/S, and owned bus racks —
> so it is the SPINE of the undo snapshot, not a piece of it.  The undo wrapper adds only what
> presets deliberately exclude: the instance's piano-roll/pattern content, automation lanes
> targeting it, INBOUND references from other strips (their sends/sidechain picks at this
> channel), and an IDENTITY-PRESERVING restore mode (the preset loader deliberately rewrites id
> prefixes for load-into-another-page; undo needs the same index/ids/uuids back so lanes and
> windows re-resolve). CONSEQUENCE for the absorbed DirtyFlag half: with every action
> undoable, the dual structural counter loses its reason to exist — dirty = transaction-pointer
> mismatch alone should suffice; verify at plan-open rather than carrying the counter forward.
> RE-PLAN AT BATCH OPEN from these premises; the task bodies below predate both rulings.

> **Canonical path:** `Plans & Specs/Batch Plans/long-rewinding-yak.md` (mirrored at G4 group
> approval; home-dir copy deleted). **For execution:** bulk-run G4 batch 7 of 8 — MERGED with
> QA-DirtyFlag per the 2026-08-06 ruling above. §B authored at code-complete; one source commit.

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

- **Risk:** medium-high. The parallel `mHistoryLabels` list assumes every transaction flows
  through `doUndoAction`; wiring the APVTS UndoManager makes attachments create transactions
  OUTSIDE it — the label list, cursor, and history window all need a new sync layer, and
  automation/programmatic writes must be excluded from history. Task 1 opens with a bounded
  source-reading spike on the vendored APVTS to pin exactly which writes transact.
- **Effort:** ~8-12 h.
- **Dependencies:** runs after QA-ProjectSave (its new flows get covered); QA-DirtyFlag
  consumes this batch's transaction-event infrastructure.

## Spec calls already locked

| ID | Decision | Reasoning |
|----|----------|-----------|
| Marathon 9b | PROCESSOR-owned UndoManager handed to the main APVTS at construction; StandaloneEditor's manager retires in favor of it; param changes undoable + count toward the dirty pointer | Locked 2026-07-08 |
| Marathon 19 | Boundary: this batch = plumbing + wiring; DirtyFlag = TransactionTracker on top | Locked 2026-07-08 |
| Docket 12=A | Full reshaped list: (i) APVTS wiring, (ii) gap wrapping of the dirty-but-not-undo gestures, (iii) Event Editor unification, (iv) manual-push gesture bracketing | Jeff 2026-07-25 |
| Docket 13=A+ii | ONE manager everywhere — all 10 engine APVTSes take the global manager; the 3 private managers + the Rusty-only branch retire. Dead-tab transactions: HIDDEN from the history list + auto-skipped on undo/redo | Jeff 2026-07-25 |
| Docket 14=a | Event Editor adopts Ctrl+Alt+Z redo; its keys get a DISPLAY-ONLY section in the Key Binds window | Jeff 2026-07-25 |

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open. Implementation shape stated for R5 (plain English): transactions get
tagged with an owner key (which tab/surface made them) via the transaction NAME — our own
gesture entry points name their transactions, and a listener-driven shadow list keeps the
history window truthful for transactions we didn't start (attachment gestures). "Hidden +
auto-skipped" = the history window filters dead-owner rows out, and Ctrl+Z walks straight
through them (undoing a dead transaction mutates only the dead engine's detached, ref-counted
state — memory-safe no-op — then continues to the next live one, bounded loop).

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

### Task 3 — Shadow list, owner keys, hide + skip (13=ii)

- [ ] Replace the write-side `mHistoryLabels` appends with a listener-driven shadow list:
  UndoManager ChangeListener detects new transactions (top undo-description changed while redo
  emptied) and appends {label, ownerKey}; `doUndoAction` keeps naming its own transactions
  (`<owner>|<label>` convention); attachment gestures get named at drag-start via a small hook
  at the attachment creation sites (editor knows its owner key).
- [ ] Owner keys: tab-scoped (`lay0`, `drm3`, `vox1`, `rusty`, …) for engine/page surfaces;
  `app` for global surfaces. On tab close, its key joins a dead set.
- [ ] History window filters dead-owner rows; `globalUndo/globalRedo` skip-loop over
  dead-owner transactions (undo the no-op, continue; hard cap = history depth).
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
  covered by Task 1's exclusion. Audit table -> running notes.
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
4. Two Layers tabs: edit knobs on both, delete tab 2 -> its rows vanish from the history
   window; Ctrl+Z walks tab 1's edits without ever landing on tab 2's (skip verified by the
   labels shown as you step).
5. Rusty ARIA knob edit undoes from ANY page via Ctrl+Z (the visible-page restriction is gone).
6. Event Editor: draw + move + delete lane points -> each appears in the history window;
   Ctrl+Z inside the editor undoes them; redo is Ctrl+Alt+Z (Ctrl+Y does nothing); the Key
   Binds window shows the Event Editor section.
7. Pattern remove undo restores the pattern WITH its notes at the same index; marker-set undo
   restores marker + the played tempo (readout check).
8. Undo History window depth setting (100/250/500/1000) still applies; labels match actions.
9. Load a project: history is EMPTY (loads don't transact); first post-load edit is row 1.

## Routing notes (Rule 3)

Dirty-pointer semantics (undo re-cleaning the asterisk, the three undo-path markDirty calls)
belong to QA-DirtyFlag — do not fix here; log and hand over. Spike findings that contradict the
mechanism above get surfaced to Jeff in chat BEFORE re-shaping (ask-always lock).

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
4. **Task 6's ~101-site census is stale** — mammoth's registration rework moves/removes
   push sites; re-scout at open.
5. **The audit surface grew:** "+"-driven tab adds, rack sidebar gestures, freeze/unfreeze,
   and VST3 slot edits are new user-changeable surfaces this coverage audit must include.
   The ~8-12 h estimate predates them.
