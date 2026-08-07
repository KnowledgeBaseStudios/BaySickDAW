# Running Notes — QA-UndoCoverage (long-rewinding-yak)

> Append-only mid-batch log. Entries land at every checkpoint (commit landed / finding captured
> / spec call resolved / scope pivot) per `feedback_draft_doc_running_notes_every_checkpoint.md`.
> At code-complete, `/draft-doc batch-close` consumes this file to compile the Implemented Work
> Log entry, which is HELD here (bulk-run R2) and applied at the batch's campaign section pass.
> (Task 1's APVTS transaction-semantics spike findings land here FIRST — the absorbed
> DirtyFlag work, Tasks 8-9 of the merged plan, builds on them.)
>
> Pair file: [`Plans & Specs/Batch Plans/long-rewinding-yak.md`](../Batch%20Plans/long-rewinding-yak.md).
> Conventions: Main Plan §0 (Batch Plans + Running Notes layout, locked 2026-05-11).

## 2026-08-06 — Task 1 — Authority move + APVTS transaction-semantics spike

Spike findings recorded FIRST per the plan's Task 1 contract; the authority-move
implementation record follows.

#### Spike findings — vendored JUCE 8 undo/transaction semantics (source read 2026-08-06)

- **(a) The parameter->tree flush DOES write with the ctor UndoManager.**
  `ParameterAdapter::flushToTree` (`juce_AudioProcessorValueTreeState.cpp:120`) calls
  `tree.setProperty(key, value, um)` with the APVTS's ctor manager, driven by a
  message-thread timer (10 Hz idle, 50 Hz active — `timerCallback` at :479).  Every
  `setValueNotifyingHost` AND every plain `setValue` from ANY thread marks the adapter
  `needsUpdate`; the next flush transacts it.  The flush cannot distinguish a UI drag
  from an automation write — so host / automation / programmatic `setValue` calls DO
  end up as transactions, and worse, they land in whatever transaction is currently
  OPEN (contaminating the last user gesture) or open a new unnamed one.
- **(b) Attachment transaction naming: NONE.**  `ParameterAttachment::beginGesture`
  (`juce_ParameterAttachments.cpp:69`) calls `undoManager->beginNewTransaction()` with
  NO name — attachments produce unnamed transactions.  Task 3's naming hook must supply
  names; JUCE will not.
- **(c) Per-gesture beginNewTransaction — sliders bracket the DRAG only.**  Slider
  attachments call it at drag start only (`sliderDragStarted` override,
  `juce_ParameterAttachments.h:166`); the whole drag then accumulates/coalesces into
  that one transaction (`ValueTree::SetPropertyAction::createCoalescedAction` merges
  same-property writes preserving the original oldValue, `juce_ValueTree.cpp:457`).
  Button/ComboBox attachments wrap each click as begin+set (one transaction per click)
  via `setValueAsCompleteGesture`.  GAP: a mouse-wheel or keyboard slider change fires
  `sliderValueChanged` with NO drag start — no `beginNewTransaction` — so wheel tweaks
  append into whatever transaction is open.  Task 3/6 must bracket wheel gestures.
  **[CORRECTED at /review-batch, 2026-08-06: this gap DOES NOT EXIST in our vendored
  JUCE.  `Slider::Pimpl::mouseWheelMove` (juce_Slider.cpp:1164), text-box entry,
  double-click-reset, and the accessibility setter all wrap the value change in
  `ScopedDragNotification`, which fires `sliderDragStarted/Ended` -> the attachment's
  begin/end gesture.  Every user-facing slider change is bracketed stock.  Do NOT
  build a fix for this — one was built and reverted at the review pass.]**
- **(d) UndoManager semantics** (`juce_UndoManager.cpp`): `perform()` appends into the
  current ActionSet until the `newTransaction` flag is set; `undo()`/`redo()` auto-begin
  a new unnamed transaction; `sendChangeMessage()` fires on every
  perform/undo/redo/clearUndoHistory (Task 3's listener hook point);
  `getUndoDescriptions()`/`getRedoDescriptions()` expose the real stack names.
  `beginNewTransaction` alone creates NO ActionSet — a begun-but-empty gesture leaves
  no history row (load-bearing for the exclusion mechanism below: suppressed values
  mean no phantom rows).
- **(e) DEPTH-MENU FICTION (pre-existing, unchanged).**  `setMaxNumberOfStoredUnits(n, 30)`
  is UNIT-based; action sizes are `sizeof(*this)`-scale (SetPropertyAction ~100 units,
  our snapshot actions far larger), so every menu value 100/250/500/1000 exceeds max on
  the first entries and retention collapses to `minimumTransactionsToKeep = 30`
  transactions at every setting.  Preserved verbatim per the plan; noted for Jeff.
- **(f) HAZARD FOR TASK 2 (surfaced to Jeff in chat).**  `APVTS::replaceState` calls
  `undoManager->clearUndoHistory()` (`juce_AudioProcessorValueTreeState.cpp:403-411`).
  ~25 live call sites (engine preset loads, kit loads, PagePresetIO, project load,
  BassPage/LayersPage/DrumPage preset paths).  Once all 10 engine APVTSes share the
  global manager (Task 2), ANY engine preset load would wipe the ENTIRE app undo
  history mid-session.  Needs neutralizing on the engine-preset paths before Task 2
  flips semantics; also interacts with Task 7 (kit load / engine pick are supposed to
  be undoable ops, and undo history spanning a replaceState holds actions bound to the
  DETACHED old trees — undoing past an unneutralized preset load would write into dead
  trees, silently doing nothing to live state).
- **(g) STALE PLAN REFS CORRECTED.**  The plan's "main APVTS + 7 engines pass nullptr
  UndoManager (PluginProcessor.cpp:377)" is stale — QA-ModelShell TS1 already binds the
  main APVTS to the processor-owned manager
  (`apvts(*this, &mUndoManager, "BaySickDAWState", ...)`, now `PluginProcessor.cpp:381`)
  and EngineRig threads the same manager into every page-family engine APVTS.  The
  manager has been silently RECEIVING transactions since QA-ModelShell (nothing
  consumed them).  The authority move therefore reduced to the StandaloneEditor-side
  repoint + exclusion.
- **(h) There is an AUDIO-THREAD automation writer.**  processBlock's automation-clip
  pass (`PluginProcessor.cpp:~2913-2948`) writes main-APVTS lanes via `param->setValue`
  on the audio thread during live song-mode playback (and on the message thread during
  offline render).  Its values reach the flush timer at arbitrary later times — so any
  caller-scoped "suppress undo around the pass" would leak.  This finding selected the
  mechanism below; the plan's parenthetical "drop + re-begin around the applicator
  pass" cannot work against an async flush.

#### Exclusion mechanism selected (per the spike, within the plan's "mechanism per spike findings" latitude)

Write-time programmatic marking, consumed at flush time — a small vendored-JUCE patch
(precedented: the QA-0a lazy-registration jassert patch in the same file).

- `AudioProcessorValueTreeState` gains `static thread_local bool programmaticWritePhase`
  + RAII `ScopedProgrammaticParamWrites` (`juce_AudioProcessorValueTreeState.h`, after
  the `undoManager` member).
- `ParameterAdapter` gains `std::atomic<bool> pendingIsProgrammatic`;
  `parameterValueChanged` (which runs SYNCHRONOUSLY on the writer's thread) stores the
  phase flag — last writer wins, matching whose value the flush will see.  `flushToTree`
  consumes the mark: marked values write to the tree with `um = nullptr`.
- Works for EVERY APVTS instance automatically (engine APVTSes included — Task 2
  inherits exclusion for free).  Hand edits DURING playback still transact
  (thread_local keeps the audio thread's phase off the message thread) — faithful to
  the every-action-undoable ruling.  A begun gesture whose values were all suppressed
  produces no history row at all (see (d)).
- Phase scopes installed at the five programmatic writer passes: (1) processBlock
  automation-clip pass (`PluginProcessor.cpp` — covers live audio-thread + offline
  message-thread replay); (2) setSongMode EXIT baseline restore
  (`PluginProcessor.cpp:~7477`); (3) `applyAutomationAtCurrentPosition`
  (`StandaloneEditor.cpp:~3624` — stopped-seek APVTS writes + engine applicator-lane
  invocations); (4) `onSongModeChanged` EXIT applicator-baseline restore
  (`StandaloneEditor.cpp:~1157`); (5) the whole of `BuilderPage::runOfflineLoop` (one
  scope covers begin/endOfflineRender, both `applyOfflineLaneValue` variants, the
  restore set, and any normalize write — the message thread is blocked for the
  duration, so no flush can interleave).
- Remaining programmatic push sites (kit-default pushes, load-path defaults, MIDI-learn
  writes) get classified and phase-marked as needed in Task 6's census, per plan.

#### Authority move (code landed)

- StandaloneEditor's owned `juce::UndoManager mUndoManager(100,30)` member RETIRED —
  replaced by `juce::UndoManager& mUndoManager` bound to `p.mUndoManager` (the
  processor's, declared before apvts).  Every consumer (doUndoAction,
  globalUndo/globalRedo, history window, depth menu cases 510-513, makeUndoContext,
  the EventEditor ctor pass-through, getUndoManager()) follows through the reference
  with zero call-site churn.
- Depth default preserved: editor ctor now calls
  `mUndoManager.setMaxNumberOfStoredUnits(100, 30)` on the processor's manager (which
  previously sat at JUCE's (30000, 30) default).
- The processor accessor requirement is satisfied by the existing public `mUndoManager`
  member (QA-ModelShell TS1 placement, `PluginProcessor.h:176`).
- Immediate semantic effect: Ctrl+Z now pops attachment-created transactions too
  (engine knob drags etc. were already transacting invisibly into the processor's
  manager since QA-ModelShell).  `mHistoryLabels`/`mHistoryCursor` are OUT OF SYNC with
  the real stack until Task 3's listener-driven shadow list lands — known intra-batch
  state, one commit at close.

#### Task 1 status

- **Files touched:** `juce_AudioProcessorValueTreeState.h`/`.cpp` (vendored patch),
  `Source/PluginProcessor.cpp` (2 phase scopes), `Source/Standalone/StandaloneEditor.h`/`.cpp`
  (authority move + 2 phase scopes), `Source/Standalone/BuilderPage.cpp` (offline-loop
  phase scope).
- **Build gate:** running at entry-write time; result recorded at the next checkpoint.

## 2026-08-06 — Task 2 — Engines join the one history (13=A)

Ran as the conflict-call 2=b shape predicted (flip + verify) PLUS the replaceState
neutralization that Task 1's spike finding (f) made mandatory.

#### Verification half — the 7 page-family engines were already on the global manager

- Harmless, VibePlayer, BaySickSynth, BaySickBass, BaySickVocal, BaySickPedals,
  BaySickNAMIR: every ctor takes `juce::UndoManager*` and threads it straight into
  its apvts ctor (TS1), and every creation site passes it — `EngineRig.cpp:429-432`
  (the Layers/Bass/Drums four), `:443` (Vocal), `:489`/`:491` (Pedals + NAM/IR), and
  `BaySickVocalProcessor.cpp:296` forwards `undoMgr` into its internal NAM/IR stage.
- Desk-verified by grep: all 11 APVTS constructions in the codebase (main + 10
  engines) now bind the global manager; zero engine APVTS constructions pass nullptr.

#### Flip half (code landed) — the 3 sfizz private managers retired

- BaySickGuitarsProcessor / BaySickBassesProcessor / BaySickRustyDrumsProcessor each
  replace `juce::UndoManager mUndoManager` (owned) with
  `juce::UndoManager& mUndoManager` (the global one); ctor signature gains the
  reference (Guitars/Basses: `(int instIdx, juce::UndoManager&)`; Rusty:
  `(juce::UndoManager&)`); declaration-before-apvts ordering preserved (the reference
  must be bound before apvts stores its address).
- The three `getUndoManager()` accessors DELETED (grep-verified: their only consumers
  were the two StandaloneEditor Rusty branches below).
- Creation sites updated: `PluginProcessor.cpp:7692` (Guitars), `:7772` (Basses),
  `:7849` (Rusty) pass the processor's `mUndoManager`.
- The J-8 stage-2 Rusty visible-page branches in `StandaloneEditor::globalUndo` /
  `globalRedo` REMOVED — Rusty ARIA knob edits now transact into the one history and
  Ctrl+Z reaches them from ANY page (verification item 5).

#### replaceState neutralization (spike finding (f) follow-through)

- Vendored addition:
  `AudioProcessorValueTreeState::replaceStateKeepingUndoHistory(const ValueTree&)` —
  replaceState minus the `clearUndoHistory()` call
  (`juce_AudioProcessorValueTreeState.h`/`.cpp`; comment documents the rationale plus
  the knowingly-carried caveat: undo entries recorded BEFORE a state swap hold
  actions bound to the replaced/detached trees, so those particular param rows become
  inert no-ops for that engine — strictly better than wiping the whole app history,
  and Task 7's snapshot ops cover the full-state cases).
- ALL engine-APVTS replaceState call sites switched to the keep-history variant (27
  sites across 20 files, grep-exact): the 10 engine processors' setStateInformation
  paths (Guitars/Basses/Rusty/NAMIR/Pedals/Vocal/Synth/Bass/VibePlayer/Harmless), the
  4 engine editors' preset loads (BaySickSynthEditor, BaySickBassEditor,
  HarmlessEditor x2, VibePlayerEditor), the page preset dispatchers (LayersPage x3,
  BassPage x3, DrumPage x2, `PagePresetIO:194`, `InstPage:1146`), and the two
  StandaloneEditor kit-restore sites (`:15392`, `:15543` region).
- The THREE main-APVTS sites deliberately KEEP the clearing replaceState — all load
  paths: project/template load (`PluginProcessor.cpp:6058`, `:6385`) and
  applyPendingRackStates' adapter-rebind trick (`:6210`; single caller
  `StandaloneEditor.cpp:16359`, load-flow only — verified it cannot fire
  mid-session).  A load is supposed to start with an empty history; Task 9 adds the
  explicit load-end clear + counter reset.

#### Routed forward within the batch

- Kit-load CC-defaults pushes and other load/create-path `setValueNotifyingHost`
  writes now transact (harmlessly during project load, which ends cleared;
  contaminating for LIVE kit loads until wrapped) — classification and phase-marking
  of every push site is Task 6's census; the live kit-load / engine-pick ops become
  single snapshot transactions in Task 7.

#### Task 2 status

- **Files touched:** `Source/BaySickGuitars/BaySickGuitarsProcessor.h`/`.cpp`,
  `Source/BaySickBasses/BaySickBassesProcessor.h`/`.cpp`,
  `Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.h`/`.cpp` (manager flip +
  setStateInformation switch), `juce_AudioProcessorValueTreeState.h`/`.cpp` (vendored
  keep-history variant), `Source/PluginProcessor.cpp` (3 creation sites),
  `Source/Standalone/StandaloneEditor.cpp` (J-8 branch removal + 2 kit-restore
  sites), and the remaining replaceState call-site files:
  `Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp`,
  `Source/BaySickPedals/BaySickPedalsProcessor.cpp`,
  `Source/BaySickVocal/BaySickVocalProcessor.cpp`,
  `Source/BaySickSynth/BaySickSynthProcessor.cpp`/`BaySickSynthEditor.cpp`,
  `Source/BaySickBass/BaySickBassProcessor.cpp`/`BaySickBassEditor.cpp`,
  `Source/Harmless/HarmlessProcessor.cpp`/`HarmlessEditor.cpp`,
  `Source/VibePlayer/VibePlayerProcessor.cpp`/`VibePlayerEditor.cpp`,
  `Source/Standalone/LayersPage.cpp`, `Source/Standalone/BassPage.cpp`,
  `Source/Standalone/DrumPage.cpp`, `Source/Standalone/PagePresetIO.cpp`,
  `Source/Inst/InstPage.cpp`.
- **Build gate:** running at entry-write time; result recorded at the next
  checkpoint.  (Task 1 gate result, recording forward from the previous entry:
  PASSED clean — five exit codes 0, four link lines, zero error hits.)

## 2026-08-06 — Task 3 — Shadow list + owner-key labels (13=ii as superseded)

Ran with one mechanism shift inside the plan's latitude, driven by the Task 1 spike
findings; the shadow list itself landed listener-driven exactly per plan.

#### Mechanism shift — one vendored line supersedes the per-site attachment hooks

- The plan's "small hook at the attachment creation sites" was superseded by the
  attachment census: 109 attachment occurrences across 26 files made per-site hooks
  a sprawling sweep.  Instead ONE vendored line names every attachment gesture:
  `ParameterAttachment::beginGesture` (`juce_ParameterAttachments.cpp`) now calls
  `beginNewTransaction("param:" + parameter.paramID)` in place of the stock unnamed
  begin (spike finding (b)).  The app-side history rebuild resolves `"param:<id>"`
  to an owner key + a human-readable label centrally.  Covers every attachment in
  the codebase and every future one, with zero app-side churn at creation sites.

#### Shadow list (listener-driven, per plan)

- StandaloneEditor gains a `juce::ChangeListener` base; registered on the global
  manager in the ctor (BEFORE the history window can exist, so the rebuild always
  runs before the window's own change-driven repaint), deregistered first thing in
  the dtor (the manager is processor-owned and outlives the editor).
- `rebuildHistoryLabels()` rebuilds `mHistoryLabels` + `mHistoryCursor` from the
  manager's REAL stack on every change message: `getUndoDescriptions()` (reversed
  to oldest-first) + cursor = undo depth + `getRedoDescriptions()` appended as the
  future.  The deque/cursor pair survives because UndoHistoryWindow references both
  by reference — the window needed ZERO interface changes.
- Write-side label maintenance is GONE: doUndoAction's append/trim/cap block
  deleted; globalUndo/globalRedo's manual cursor ++/-- deleted (the 2026-04-26
  "cursor can drift on no-op undos" hack is obsolete — the cursor is now derived,
  so it CANNOT drift); the depth-menu cases 510-513 keep their exact
  setMaxNumberOfStoredUnits values (semantics preserved per plan) but drop their
  label-capping lines in favor of a rebuild call.
- doUndoAction also rebuilds SYNCHRONOUSLY before its immediate window refresh
  (perform's change message is async — one message-loop tick later) so the new row
  paints immediately.

#### Label resolution (`historyDisplayFor`)

- `"param:<pid>"` → pretty name via the existing `displayNameFor` /
  `resolveAutomationDisplayName` machinery (the same resolver the Event Editor
  titles use) + owner key via the new `ownerKeyForParamId`; displayed as
  `"<owner>|<pretty>"` (or bare pretty for app-global).
- `"<owner>|<label>"` doUndoAction names display verbatim; empty names (possible
  when a wheel tweak lands right after an undo's auto-begin) display `"(edit)"`.

#### Owner keys (labels only, per the superseded-13=ii ruling — NO hide/skip machinery built)

- doUndoAction gains an ownerKey param (default `"app"`); transaction names become
  `"<owner>|<label>"` for tab-scoped owners.  makeUndoContext gains an ownerKey
  param; the per-tab page creators stamp it: LayersPage x2 sites `"lay<idx>"`,
  BassPage x2 `"bass<idx>"`, DrumPage x2 `"drm<idx>"`, VoxPage `"vox<idx>"`.
  Piano roll / Builder / Mixer / Effects / EffectSlotWindow contexts stay `"app"`
  (cross-tab or global surfaces).
- `ownerKeyForParamId`: sfizz trio by prefix (`brd_` → `"rusty"`, `bgg_<i>_` →
  `"inst<i>"`, `bbb_<i>_` → `"inst<i>"`); rig-owned engines by walking every tab
  (7 kinds x capacity via `EngineRig::findTab` + `apvtsOf` on engine/namIr/pedals
  stages + the Vox engine's internal NAM/IR) asking each APVTS whether it owns the
  parameter; `"app"` fallback (mixer/global params).
- **KNOWN LABEL LIMITATION (recorded deliberately):** Vocal/NAM-IR/Pedals use
  UNPREFIXED param ids, so with multiple same-kind tabs open the rig walk
  first-matches and can tag the wrong tab INDEX on a history label.  Labels only —
  the undo action itself is bound to the correct tree and undoes the right engine
  regardless.  Exact-index labeling would need per-APVTS owner threading through
  the vendored attachment ctor chain; deferred unless Jeff wants it.

#### Routed forward within the batch

- Spike finding (c)'s wheel/keyboard slider gap (no drag start = no transaction
  bracket) is NOT addressed here — it rides Task 6's manual-push census, which
  classifies and brackets non-drag gesture paths.  **[CORRECTED at
  /review-batch, 2026-08-06: the gap does not exist — see the correction on
  finding (c) in the spike section above.]**

#### Task 3 status

- **Files touched:** `juce_ParameterAttachments.cpp` (vendored gesture naming),
  `Source/Standalone/StandaloneEditor.h` (ChangeListener base,
  doUndoAction/makeUndoContext signatures, rebuild/resolver decls),
  `Source/Standalone/StandaloneEditor.cpp` (ctor/dtor listener wiring, dispatch
  rework, rebuild + historyDisplayFor + ownerKeyForParamId, depth-menu cases,
  7 page-creator owner keys).
- **Build gate:** running at entry-write time; result recorded at the next
  checkpoint.  (Task 2 gate result, recording forward from the previous entry:
  PASSED clean — five exit codes 0, four link lines, zero error hits; the
  pre-existing C4458 blockSize-shadowing warning at
  `BaySickRustyDrumsProcessor.cpp:157` predates the batch.)

## 2026-08-06 — Task 4 — Wrap the dirty-but-not-undo gestures (12-ii)

Ran wider than the plan's enumeration — the four action classes became SIX (the
pattern family split three ways), plus five PatternManager bulk-restore seams the
plan didn't enumerate but the actions cannot exist without (mirroring the existing
`restorePatternList` precedent), plus UndoContext plumbing into BrowserPanel.

#### New action classes (`UndoActions.h`)

- **`PatternListSnapshot` + `PatternListAction`** — project-slice snapshot
  {patterns, currentPattern, blocks, row state}.  **DEVIATION FROM PLAN, recorded
  deliberately:** the plan said "payload = the one Pattern + index"; that payload
  is INCORRECT for remove — `PatternManager::removePattern` cascades (blocks of
  the dead pattern erased, every higher patternIndex re-indexed, currentPattern
  clamped), so a single-Pattern payload cannot restore it.  The slice is the
  smallest CORRECT payload and is the exact shape the shipped Split-by-Engine
  undo already used.  Linked TS markers stay OUTSIDE the undo domain (the
  established Split seam).
- **`PatternRenameAction` / `PatternColorAction`** — light single-field actions
  (a full slice would copy every pattern's note content for a name/color write).
- **`MarkerSetSnapshot` + `MarkerSetAction`** — the three ruler lists (time
  markers, time-sig changes, tempo changes) + the current-TS uid, restored
  atomically.  Restored uids stay valid (`mNextTsUid` only climbs).
- **`AudioLibrarySnapshot` + `AudioLibraryAction`** — library entries + manual
  groups, optionally + blocks (entry delete cascades into clips; alias rename
  rewrites block displayAlias).
- **`AutomationTemplateSnapshot` + `AutomationTemplateAction`** — template list,
  optionally + blocks.

#### PatternManager restore seams (new public API, `restorePatternList` precedent)

- `restoreTimeMarkers`, `restoreTimeSigState(list, currentUid)` (the pair
  restores atomically — `removeTimeSigChange` re-picks the uid),
  `restoreTempoChanges`, `restoreAudioLibrary(entries, manualGroups)`,
  `restoreAutomationTemplates`.
- Raw getters `getAudioLibraryEntries` / `getManualAudioGroupsRaw` /
  `getAutomationTemplatesRaw`; `AudioLibraryEntry` moved from the private
  storage section to the public API (AudioLibraryAction snapshots the list by
  value).

#### Shared capture/apply promoted out of the Split flow

- The Split-by-Engine local `SplitState` / `SplitPatternUndoAction`
  (BuilderPage.cpp anon namespace) DELETED — promoted to
  `ArrangementGrid::capturePatternSlice()` / `applyPatternSlice()` + the shared
  `PatternListAction`; the Split flow now uses them (identical semantics,
  SafePointer'd apply).  `applyPatternSlice` guards degenerate empty snapshots.
- New public grid helpers: `performPatternSliceOp`, `performMarkerSetOp`,
  `performPatternTsOp` (a pattern-TS gesture writes pattern fields AND marker
  lists, so a `MarkerSetAction` RIDES the slice action inside ONE transaction
  via `manager->perform` without `beginNewTransaction` — one Ctrl+Z reverses
  both), `riderManager()`.

#### BrowserPanel plumbing

- Gains `UndoContext mUndoCtx` (wired via `BuilderPage::setUndoContext`),
  `onCapturePatternSlice` / `onApplyPatternSlice` (wired to the grid's
  implementations), private wrap helpers `performPatternSliceOp` /
  `performLibraryOp(label, withBlocks, op)` /
  `performTemplateOp(label, withBlocks, op)`, and public
  `refreshAutomationTab()` / `refreshPatternTab()` for undo-apply paths living
  outside the class.

#### Sites wrapped (~30)

- **Pattern list:** browser +Add / Delete toolbar buttons, context-menu
  Duplicate / Delete, `renamePatternAt` (light rename), pattern color picker
  (browser + transport-dropdown variants), transport dropdown New / Rename /
  Delete, keybind `createNewPattern` + `showRenamePatternDialog`.  The transport
  Delete dialog's "This cannot be undone." text REMOVED — it can now.
- **Markers:** ruler menu Delete Marker / Delete TS / Delete Tempo,
  promptAdd/Edit TempoChange, promptAdd/Rename TimeMarker, promptAdd/Edit
  TimeSigChange, `showCurrentTsPicker`, the transport-dropdown current-TS
  re-pick; pattern-TS ops (clip-menu TS presets, Lock Previous TS,
  transport-dropdown Set/Reset) via `performPatternTsOp`.
- **Library:** Create Group, Rename Group (manual branch), Assign Group, Choke
  Group (tree + flat), `confirmAndDeleteLibraryEntry` (withBlocks cascade),
  `renameAudioAt` (withBlocks), editor-side `onApplyLibraryProperties` ("Audio
  Properties", withBlocks) + `onTagCopiedEntry` ("Copy Audio Entry") via new
  editor helpers `captureAudioLibrarySnapshot` / `makeAudioLibraryApply`.
- **Templates:** browser Delete Automation (withBlocks cascade),
  `renameAutomationAt` (withBlocks; also covers "Revert to auto name"), New
  Automation Clip template-registration RIDER appended into the block's
  `commitEdit` transaction, editor `createAutomationBlock` ("Add Automation" —
  block + template in one transaction; this Automate-menu path had NO undo
  bracket at all).
- **Row renames:** the two TrackHeaderPanel rename dialogs now bracket with
  `mGrid.beginEdit("Rename Track")` / `commitEdit` — the plan's "verify renames
  ride ArrangementEditAction" check came back NEGATIVE (they bypassed commitEdit
  entirely; the fix shape already existed in the same function's Move/Delete
  handlers).
- **Import Audio:** the library entry + clip defaults now RIDE the Import Audio
  transaction (`AudioLibraryAction` appended after commitEdit) — undoing an
  import no longer strands a browser entry.

#### Deliberately NOT wrapped (recorded)

- `afterPatternBlockPlaced`'s auto-spawned linked TS marker (the established
  outside-the-undo-domain seam).
- `beginAddRenderToProject`'s direct entry-add + owner-set fallback — the flow
  creates PAGES (structural, Task 7 domain) and its dominant path routes through
  the now-wrapped `onApplyLibraryProperties`; wrapping the fragments would
  create overlapping-snapshot transactions.  Composes with Task 7.

#### Routed forward within the batch

- Known interaction for Task 7: undoing "Delete Audio" when it was the last
  file on a page restores library + blocks but NOT the auto-closed tab (tab
  close is structural; Task 7's resurrection must compose here).

#### Task 4 status

- **Files touched:** `Source/Standalone/UndoActions.h`,
  `Source/PatternManager.h`/`.cpp`, `Source/Standalone/BuilderPage.h`/`.cpp`,
  `Source/Standalone/StandaloneEditor.h`/`.cpp`.
- **Build gate:** FIRST RUN FAILED (two real defects, both fixed same-pass):
  `TimeMarker`/`TimeSigChange`/`TempoChange` are FREE structs, not
  PatternManager-nested — MarkerSetSnapshot's qualifiers dropped; and the
  slice-apply ctor wiring called private `rebuildPatternRows` from outside
  BrowserPanel — routed through the new public `refreshPatternTab()`.  RE-RUN
  PASSED clean — five exit codes 0, four link lines, zero error hits.  (Task 3
  gate result, recording forward: PASSED clean on the same criterion.)

## 2026-08-06 — Task 5 — Event Editor unification (12-iii + 14=a)

Tasks 1+3 already dissolved most of the original desync premise — the Event
Editor's `mUM` reference has pointed at the ONE global manager since the Task 1
authority move, and Task 3's listener-driven label rebuild reads transaction
names straight off the manager, so its direct named transactions were already
labeling correctly.  What Task 5 actually delivers: choke-point routing (dirty
marking + owner semantics + globalUndo/globalRedo consistency), the
redo-keybinding unification, and the Key Binds reference section.

#### Choke-point routing (12-iii)

- **Context plumbing.**  `EEAutomationGrid`, `EventEditorContent`, and the
  `EventEditor` window all gain `setUndoContext(const UndoContext&)`;
  StandaloneEditor wires `ed->setUndoContext(makeUndoContext())` at Event
  Editor creation (owner `"app"`).  The direct-manager path survives only as
  an unwired fallback.
- **All four direct transaction sites rerouted.**  Every
  `mUM.beginNewTransaction/perform` site now routes through the ctx:
  `EEAutomationGrid::commitEdit` (every point add/move/erase/curve-change
  gesture), "Erase to Range Start", "Import MIDI CC", "Simplify Curve" — via a
  new `EventEditorContent::performViaCtx(label, action)` helper.  Lane edits
  now hit `doUndoAction` (named transaction + markDirty + synchronous label
  rebuild) like every other surface.
- **Undo/redo dispatch.**  The Edit menu items + keyboard route through
  `ctx.undo/redo` → `globalUndo/globalRedo` via new `doUndo()`/`doRedo()` —
  the editor no longer steps the manager behind the app's back.
- **Repaint hook.**  The editor's ChangeListener repaint stays (per plan).

#### Redo unification (14=a)

- Local keymap: Ctrl+Alt+Z = redo (matches the app binding); the old local
  Ctrl+Y redo REMOVED (the key now does nothing).  Ctrl+Z still undoes.
  Edit-menu label "Redo (Ctrl+Y)" → "Redo (Ctrl+Alt+Z)".

#### Key Binds window (14=a)

- New `BSCommands::Category::EventEditor = 6` + categoryName "Event Editor
  Key Binds" + a new tab in KeyBindsContent (after Vocal Editors) —
  display-only reference rows via the existing MouseRefRow mechanism (the
  QA-Fd VocalEditors precedent: documented, not rebindable, no Set buttons;
  ASCII).
- 11 rows documented: P/B/D/I/E/Z tool picks, Delete/Backspace (delete
  points, last-point whole-clip prompt), Ctrl+Z Undo, Ctrl+Alt+Z Redo (with
  a tooltip noting the retired Ctrl+Y), Ctrl+A Select All, Ctrl+M Import
  MIDI CC.

#### Task 5 status

- **Files touched:** `Source/Standalone/EventEditor.h`/`.cpp`,
  `Source/Standalone/KeyBindings.h`/`.cpp`,
  `Source/Standalone/KeyBindsWindow.cpp`,
  `Source/Standalone/StandaloneEditor.cpp` (one wiring line).
- **Build gate:** running at entry-write time; result recorded at the next
  checkpoint.  (Task 4 gate result is already recorded in the Task 4 entry —
  re-run PASSED clean.)

## 2026-08-06 — Task 6 — Manual-push gesture bracketing (12-iv) + the census

The stale "~101 sites" number re-scouted from scratch (post-mammoth,
post-layout, post-Tasks-1-5): **87 `setValueNotifyingHost` sites** across
Source/, classified 52 USER-GESTURE / 25 PROGRAMMATIC / 8 ALREADY-EXCLUDED
(inside Task 1's five phase scopes, incl. every applicator lambda — verified
`mAutomationApplicators` has exactly two invocation sites, both scoped) /
2 AMBIGUOUS.  Plus 1 audio-thread `AudioProcessorParameter::setValue` (the
processBlock replay, already scoped), 1 `AudioParameterBool operator=`
(NAM-IR snapshot, handled below), 0 `getParameterAsValue` writes in project
code (the immediate-transaction Value path does not exist outside vendored
JUCE).  This entry is the audit record; the full per-site census with
file:line lives in the classification below.

#### Mechanism recap

- USER-GESTURE sites get a named bracket via the new
  `Source/Standalone/UndoBracket.h` helper —
  `beginParamUndoGesture(apvts, paramId)` opens a `"param:<id>"` transaction
  (the same marker the vendored attachment naming uses, so the history
  rebuild resolves owner + pretty label).  ONE bracket per gesture;
  multi-param gestures coalesce into that one transaction.
- PROGRAMMATIC sites get `ScopedProgrammaticParamWrites` phase scopes
  (write-time marking; Task 1's mechanism).

#### Programmatic sites phase-marked this task (14 new scopes)

- `PluginProcessor.cpp` `resetToBlankState` (File New/Open default sweep);
  `removeRustyInsert` + `destroyBaySickRustyDrums` + 
  `resetBaySickRustyDrumsMixerState` (teardown / program-swap default pushes).
- `MidiLearnRegistry::dispatchEvent` — hardware CC streams have no gesture
  boundaries; mapped writes are EXCLUDED from history (an un-bracketed
  stream would contaminate whatever transaction is open).  **OPEN QUESTION
  FOR JEFF (recorded, non-blocking):** making hardware MIDI tweaks undoable
  needs idle-timeout gesture bracketing — not built.
- `InstPage` program-switch restore + `StandaloneEditor` project-load sfizz
  restore (both are the force-fire loops after
  `replaceStateKeepingUndoHistory`); `PagePresetIO` engine-param walk +
  strip-param restore; `FxRackPresetIO` EQ restore; `BaySickRustyDrumsPage`
  preset apply (rides the existing mSuppressDirty block);
  `MixerPage::syncApvtsFromMixerState` (ctor-time model->APVTS mirror);
  `BaySickVocalProcessor` pre-QA-Fe migration write; the three sfizz
  `loadKit` CC sweeps (Guitars/Basses/Rusty — the kit load itself becomes
  ONE structural transaction in Task 7).
- `BaySickNAMIRProcessor::applySnapshotToCurrent` phase-marked WHOLE:
  undo of an A/B flip stays correct without the restored values in history,
  because undoing the `ab_slot` param re-fires parameterChanged which
  re-applies the other slot's snapshot (self-healing handler).  This also
  covers both load-path entries (setStateInformation + the async slot-flip
  edge during load).

#### User gestures bracketed (67 insertions, 16 files)

The census's 52 gestures expanded to 67 inserts because the shared EQ writer
(`setAPVTSFromBand`) is bracketed at its DISCRETE callers, not inside the
helper.  Per-file: SharedUI 13+1 (the +1 below), MixerPage 10, NAMIREditor 8,
StandaloneEditor 7, DrumPage 4, EffectEditorPanels 4, BassPage 3,
LayersPage 3, HarmlessEditor 3, BaySickAlignEditor 3, PluginProcessor 2
(swing knob mix + truncate), MixerTrackStrip 2, BssEditorComponents 2,
RustyDrumsProcessor 1 (hi-hat pedal click CC), BaySickVocalEditor 1,
BaySickPitchEditor 1.  Highlights:
- Mixer: cable add/delete menus, input-channel pick (+disarm, one bracket —
  the disarm result flows through the same callback and writes the channel
  params first), bus collapse, Listen-LED modes, aux/secondary-bus delete
  send-reroute cascades (bracketed at the confirm-Yes gesture with the
  strip's real `_level` id).
- EQ display: 11 discrete callers (type/slope/channel menus, band enable,
  M/S chips, solo clear, reset-gain, reset-all, typed-readout commit) +
  "Make Dynamic" + BOTH A/B Compare branches (the census missed the
  APVTS-only compare branch — the sweep agent caught it; bracket added).
  Continuous paths (slider commit, curve drag, wheel Q) deliberately NOT
  bracketed here — they ride attachments/gesture flows.
- Engine editors: Harmless auto-gain + Part A/B, NAM-IR six selector
  onChanges + two chooser mode auto-switches, BssLedRadio click, XY-pad
  drag bracketed at mouseDown only (guarded with
  `mouseWasDraggedSinceMouseDown` because mouseDrag delegates to mouseDown).
- Vocal editors: the two AMBIGUOUS sites resolved by splitting — factory
  preset apply / user-preset load / save's "(User)" tag are BRACKETED as
  single gestures (a preset pick is one undoable action, matching the EQ
  A/B treatment), while their engine setStateInformation loads were already
  covered by load-path marks.
- Pattern-family / marker / library / template gestures are NOT here — they
  are Task 4's snapshot actions.

#### Recorded, not fixed

- `BaySickGuitarsProcessor::sendCc` / `BaySickBassesProcessor::sendCc` have
  ZERO callers (dead code, pre-existing — routing per Rule 3 at close, not
  cleaned here since this batch did not orphan them).
- The four applicator lambdas' exclusion is DYNAMIC (depends on
  `mAutomationApplicators` keeping exactly two invocation sites, both
  scoped) — any future third invoker must add its own phase scope.

#### Task 6 status

- **Files touched:** `Source/Standalone/UndoBracket.h` (new),
  `Source/MidiLearn/MidiLearnRegistry.cpp`, `Source/PluginProcessor.cpp`,
  `Source/Inst/InstPage.cpp`, `Source/Standalone/` (StandaloneEditor,
  MixerPage, MixerTrackStrip, SharedUI, EffectEditorPanels, DrumPage,
  BassPage, LayersPage, PagePresetIO, FxRackPresetIO,
  BaySickRustyDrumsPage), `Source/Harmless/HarmlessEditor.cpp`,
  `Source/BaySickNAMIR/` (editor + processor), `Source/BaySickSynth/
  BssEditorComponents.cpp`, `Source/BaySickVocal/` (VocalEditor,
  AlignEditor, PitchEditor, VocalProcessor), the three sfizz processors.
- **Build gate:** running at entry-write time; result recorded at the next
  checkpoint.  (Task 5 gate result, recording forward: PASSED clean — five
  exit codes 0, four link lines, zero error hits.)

## 2026-08-06 — Task 7 (mid-task checkpoint) — Structural undo spine landed

Task 6 gate PASSED clean (five exit codes 0, four link lines, zero errors).
Task 7 is mid-flight; this checkpoint records the landed spine + the precise
remainder so a session break loses nothing.

#### Landed (compiles clean — intermediate error-probe build had zero error hits)

- **Store:** `Source/Standalone/UndoSnapshotStore.h` (new, header-only) —
  `Documents/BaySickDAW/UndoSnapshots/`, `writeNew(xml)` unique-named files,
  `sweepAll()` wired at editor ctor (crash-stray sweep) + editor dtor.  The
  dtor also now calls `mUndoManager.clearUndoHistory()` — every action
  references editor-lifetime objects, so the history dies WITH the editor
  (and action dtors delete their snapshot files).
- **Action:** `StructuralOpAction` (UndoActions.h) — undoFn/redoFn lambdas +
  owned snapshot files deleted in its dtor; skip-first perform convention.
- **Identity findings (scout-verified):** everything load-bearing keys off
  pageIndex (mixer channelId, strip params, window persist keys, automation
  lane paramIds); ribbonTabId is session-monotonic and NEVER persisted — a
  resurrect takes a fresh ribbon id by design.  Effect-slot uuids round-trip
  inside the rack blob (`EffectRack` C13 uuid persistence + uuidOverride), so
  rack lanes survive resurrection via the PagePresetIO racks payload.  Strip
  APVTS params + piano-roll content + automation lanes SURVIVE tab deletion
  in the model (deliberate; only the engine + racks die), so the snapshot
  only needs the PagePresetIO chain slice + the tab record.
- **Capture/apply seams:** `PagePresetIO::peekEngineTypeFromXml` (string
  variant); `LayersPage`/`BassPage` gain `capturePagePresetXml()` +
  `applyPagePresetXml()` (loadPagePreset refactored through them); DrumPage
  already had `exportPagePresetXml`/`importPagePresetXml` (G-7 kit
  machinery).
- **Resurrection = the duplicate spawns, generalized:** the three
  `spawnDuplicate{Layer,Bass,Drum}Tab` functions gain
  `(xml, forcedPageIndex = -1, forcedName = {}, fullPreset = false)` and
  return the new ribbon id — with a forced index they ARE the resurrect path
  (create page at index + full wiring + host + apply full-chain preset).
- **Wrapped ops (pass 1 = Layers/Bass/Drums):**
  - `deleteTabWithUndo(ribbonId)` — captures the chain slice to a snapshot
    file, closes, performs a StructuralOpAction (undo = resurrect at same
    pageIndex/name, redo = close the LIVE id via a shared id tracker).  ALL
    14 `onDeleteRequested` wirings rewired to it by a uniform replace
    (kinds without a capture path fall back to plain closeTab — no false
    undo entry).
  - `"+"-menu add+pick` = ONE transaction (wrap in `onAddEngineRequest`
    after `applyEngineToNewestTabOfType`): undo closes; redo re-creates at
    the same index from the fresh-state snapshot.
  - `duplicateTabWithUndo(type, xml)` — all 14 duplicate wirings rewired;
    undo removes the duplicate, redo re-creates it at the same identity from
    its post-spawn snapshot.
  - **Drum sound pick/swap/clear** — `DrumPage::performSoundSwapGesture`
    (page now STORES its UndoContext): one gesture = one StructuralOpAction
    with before/after full-chain snapshots; empty side = "no engine" applies
    as `clearSoundInternal()` (split out of the now-wrapped `clearSound`).
    Wrapped at the sound-picker dispatch terminals only (the load helpers
    stay bare — the KIT loader calls them programmatically).  Undo of a
    sample-engine load is itself a LOAD (async) — accepted property.

#### REMAINDER (in flight; updated as pieces land)

- (b) DONE — `loadKitWithUndo(kitXml)` wraps `loadKitImpl` as ONE
  transaction (both loadKit continuations rewired to it): captures every
  DrumPage before the teardown sweep + the created set after; no-change
  guard (identical post-load id set = snapshots deleted, no transaction);
  undo closes created + resurrects captured via the forced-index drum
  spawn, redo the inverse; live-id vectors track resurrected identities.
- (c) IN FLIGHT — Guitars/Basses program pick: the switch body lives in the
  `InstPage::showProgramPickerMenu` menu callback (InstPage.cpp ~:1078-1160,
  steps 1-4: cache outgoing state -> race-safe kit load -> restore cached
  incoming state (already phase-marked) -> notify).  PLAN: extract to
  `InstPage::switchSfizzProgram(target)`; wrap = StructuralOpAction
  {undo=switch(prior), redo=switch(new)} — NO snapshots needed, the
  program-state CACHE carries the CC state.  BLOCKER FOUND: InstPage has NO
  UndoContext (no setUndoContext) — needs a ctx member + wiring at the
  spawnInstTabIfMissing / restore creation sites (mirror VoxPage's
  "vox<idx>" wiring; key "inst<idx>").  Rusty kit/program picks: same shape
  via BaySickRustyDrumsPage (its loadKit + program combo sites; page HAS
  setUndoContext? unverified — check first).
- (d) Clips/Vox/Inst + Plugins delete capture (library+blocks slice via the
  Task 4 machinery + per-kind resurrect mirroring the restore-walker
  branches).  Until then those kinds delete WITHOUT an undo entry (honest
  fallback, no false rows).
- Then Tasks 8-9 (TransactionTracker + touch-model retirement).

## 2026-08-06 — Task 7 (second checkpoint) + Tasks 8-9 — dirty pointer live

#### Task 7 additions since the first checkpoint

- **(c) Guitars/Basses program pick wrapped:** the switch body extracted from
  the `InstPage::showProgramPickerMenu` callback into
  `InstPage::switchSfizzProgram(target)` (verbatim steps 1-5);
  `switchSfizzProgramWithUndo` performs a StructuralOpAction {undo =
  switch(prior), redo = switch(new)} — NO snapshot files: the session
  program-state CACHE carries each program's CC tweaks, and undo re-enters
  through the same cache the pick always used.  InstPage gains
  setUndoContext/mUndoCtx (wired "inst<idx>" at the spawn site).  The .h's
  `rebuildProgramCombo`/`onProgramComboChanged` decls turned out to be DEAD
  (no definitions) — pre-existing, routed at close per Rule 3, not cleaned.
- **SURFACE TO JEFF #1 (spec collision, deliberately not resolved
  unilaterally):** the Rusty PROGRAM SWITCH ships a Jeff-approved confirm
  that says "This cannot be undone." (it resets 13 strips + clears the Rusty
  roll across every pattern via destroyBaySickRustyDrums).  Making it
  undoable = pattern-slice + strip + CC capture (heavy, buildable);
  keeping it = a standing exception to the every-action ruling that his own
  dialog wording created.  Left UNWRAPPED pending his call.
- **SURFACE TO JEFF #2 (pass-2 completeness):** Clips / Vox / Inst /
  Plugins / Rusty TAB DELETES currently close WITHOUT an undo entry (the
  deleteTabWithUndo fallback — honest, no false rows).  Their capture needs
  the library+blocks slice + per-kind resurrects mirroring the
  restore-walker branches (the map is in the Task 7 scout).  L/B/D deletes,
  all adds, all duplicates, drum sound swaps, drum-kit loads, and sfizz
  program picks ARE undoable.

#### Task 8 — TransactionTracker (dual counter dropped, per the merge ruling)

- `VibeSynthProcessor::TransactionTracker mTxTracker` declared beside the
  manager: current/saved only, branch-kill (new transaction while
  current < saved -> saved = -1), edge-fired `onDirtyChanged(bool)`.
- **Feed = the editor's UndoManager ChangeListener** (implementation
  latitude over the plan's "doUndoAction/globalUndo/Redo notify" wording):
  choke transactions AND attachment gestures both broadcast, so deriving
  events from manager depth deltas in `changeListenerCallback` counts every
  source exactly once — explicit choke notifications would double-count
  what the listener already sees.  Deltas apply by COUNT (coalesced change
  windows of N transactions advance by N); the depth-cap-drop + new
  transaction ambiguity resolves via the top-name change; a window mixing
  undo with a new transaction cannot be produced by mouse-driven gestures
  (recorded caveat).
- The three undo-path markDirty calls REMOVED (doUndoAction / globalUndo /
  globalRedo) — undo can now CLEAN the project, the exact inversion the
  absorbed stoat spec exists to fix.

#### Task 9 — touch-model retired

- `ProjectManager`: `mDirty` bool + `setDirtyInternal` DELETED.  `isDirty()`
  delegates to the tracker.  `markDirty()` keeps ONLY the TS7 change stamp —
  **reasoned deviation:** the plan expected the ~18 direct markDirty sites
  removed, but the stamp is version capture's change detector and its scope
  comment explicitly enumerates those same paths; removing the calls would
  shrink capture's detection.  They stay AS the stamp feed (dispositioned
  wholesale: stamp-only now, zero dirty effect).  Same reasoning keeps the
  engine `ApvtsDirtyTracker` onAny wiring (feeds the stamp; its lock-free
  `hasChangedSinceLastBlock` audio gate was always a separate consumer);
  the class stays whole.
- `markSaved()` NEW = tracker.onSave(); `saveProject` pins the pointer
  instead of clearing anything (saveProjectAs rides it).  `clearDirty()`
  REPURPOSED as the load-boundary reset: `clearUndoHistory()` + snapshot
  sweep + tracker.onLoadReset() — its callers are exactly the load/new
  boundaries (openProject, createProject, restoreBackup, the end-of-restore
  gate at StandaloneEditor's isLoadContext site), so loads open clean with
  an empty history and row 1 = the first post-load edit.
- Tracker dirty-edge forwards through the EXISTING
  `ProjectManager::onDirtyChanged` hook (wired in the PM ctor), so the
  title asterisk + confirmDiscardChanges + quit gate keep their wiring
  untouched (all read the delegating isDirty()).
- **Autosave verified FIRST, changed NOTHING:** `writeBackup` never cleared
  dirty (explicit comment in the code: backups are a safety net, dirty
  stays until a real Save) and fires unconditionally on its timer — the
  tracker preserves both properties by construction.
- Freeze/unfreeze dirty semantics ride mammoth TS7's ruling untouched.

## 2026-08-06 — Jeff's five rulings received + implemented

Jeff ruled on the held-open docket: 1)a 2)a 3)a 4)b 5)required.
(Chronology note: the Tasks 7-9 combined gate entry BELOW ran before these
rulings; an ordering slip during entry-write put it after — content verbatim,
titles corrected, nothing re-worded.)

- **(5) Depth-menu honesty — REQUIRED, shipped.**  Jeff's reading confirmed:
  every setting retained ~30 transactions (unit-based cap, every action
  over-budget).  Fix: `setMaxNumberOfStoredUnits(1, N)` — maxUnits 1 keeps
  the unit test permanently over, so minimumTransactionsToKeep IS the menu
  number.  Ctor default (1, 100); cases 510-513 = (1, 100/250/500/1000).
  Trade-off stated to Jeff: an honest 1000 can hold heavy snapshot actions
  N-deep in RAM.
- **(2a) ALL remaining tab deletes undoable.**  `captureTabRecord(entry)`
  mirrors serializeTabsInto's per-kind Tab record (Clips/Vox/Inst/Plugins/
  Rusty); `resurrectTabFromRecord(rec)` mirrors the project-restore branches
  verbatim (same creation order, same race-safe sfizz loads, same
  quiet-then-loud Plugins hook staging).  deleteTabWithUndo's fallback became
  the pass-2 path: record -> snapshot file; Clips/Vox/Inst additionally
  capture the library+blocks slice (restored BEFORE resurrect so the Clips
  path-match finds its entries); Rusty additionally captures the pattern
  slice (the delete cascade clears the Rusty roll on every pattern; restored
  AFTER resurrect).  FOUR more user-delete wirings found un-rewired (the
  Rusty fireDelete uses `ribbonId`, and the three Clips/Vox/Inst dynamic-id
  wirings) — all four now route through deleteTabWithUndo.  The Rusty delete
  warning drops "This cannot be undone."
- **(1a) Rusty program switch undoable.**  Page hook
  `onWrapStructuralOp(label, op)` (editor-wired in addBaySickRustyDrumsTab):
  composite capture = the Rusty tab record (engine blob = kit path + CC
  state) + the pattern slice; restore re-enters through
  resurrectTabFromRecord (addBaySickRustyDrumsTab no-ops while the tab is
  alive -> pure state restore) + slice apply.  The switch prompt drops
  "This cannot be undone."; first-load (None -> program) stays unwrapped
  (nothing to restore).  The player-preset load routes through the same hook
  ("Load Player Preset").
- **(3a) Preset loads become snapshot ops.**  Vendored:
  `replaceStateKeepingUndoHistory` gains an optional undoTransactionName —
  non-empty = the swap itself performs ONE StateSwapAction (wholesale old
  <-> new tree; undo/redo re-assignment runs under
  ScopedProgrammaticParamWrites so the rebind flush cannot perform nested
  actions mid-undo, which UndoManager forbids).  Opt-in ONLY at direct user
  preset gestures: the four engine editors (BaySickSynth, BaySickBass,
  Harmless x2 incl. "Init Patch", VibePlayer) pass names; load paths and
  calls inside wrapped structural gestures keep the silent default.  Page
  presets: LayersPage + BassPage gain `performChainSwapGesture` (full-chain
  before/after via capture/applyPagePresetXml; skipped on an engine-less
  page); DrumPage's page-preset load rides its existing
  performSoundSwapGesture; PluginsPage gains a plugin-state-blob swap
  gesture + its creators now wire a "plug<idx>" UndoContext.
- **(4b) MIDI-learn stays excluded** — no change (the Task 6 phase mark
  stands).

Ruling-set build gate: PASSED clean FIRST TRY — five exit codes 0, four
`vcxproj -> .exe` link lines, zero `error C|LNK|MSB` hits, full rebuild.
BATCH CODE-COMPLETE: Tasks 1-9 + all five rulings.  §B.33 authored (18 UND-B
rows).  Close sequence from here: /draft-doc batch-close (held entry, R2) ->
/review-batch (restored per Jeff's catch) -> ONE batch commit surfaced with
the full git status for approval.

PLAN-FILE CORRECTION (Jeff, in chat): the yak plan's batch-close checklist
omitted the `/review-batch` step.  That is a plan-authoring gap — Main Plan
§0's mandatory close sequence (draft-doc batch-close -> /review-batch with
BLOCKERs addressed -> apply -> commit) governs and WILL run at this close,
same as QA-Layout's did.

## 2026-08-06 — Tasks 7-9 combined gate result (pre-rulings; title corrected)

Combined build gate for Tasks 7-9: FIRST RUN failed on DrumPage.cpp alone
(three missing pieces from the sound-swap edit set — the `mUndoCtx` member
decl, the `clearSoundInternal` decl, and the UndoSnapshotStore include; every
other Task 7-9 file compiled clean on that run).  All three fixed; RE-RUN
PASSED clean — five exit codes 0, four `vcxproj -> .exe` link lines, zero
`error C|LNK|MSB` hits.  Tree at this checkpoint: 59 modified + 2 new files
(`UndoBracket.h`, `UndoSnapshotStore.h`), all accounted to Tasks 1-9; no
commit yet (one batch commit at close per the bulk-run G4 convention).
Batch HELD OPEN on the two spec calls posed to Jeff in chat (Rusty
program-switch collision; pass-2 tab-delete completeness) plus the smaller
surfaced items (preset-load undoability, MIDI-learn undoability, depth-menu
honesty, the plan file's stale batch-close build line).

## 2026-08-06 — /review-batch result + review-fix pass (close sequence, step 2)

`/review-batch` ran per the Main Plan §0 close sequence (the step Jeff's catch
restored): **1 BLOCKER / 4 NEEDS-FIX / 6 NIT.**  Every BLOCKER and NEEDS-FIX
fixed in-batch same day; re-gate GREEN FIRST TRY (five exit codes 0, four
`vcxproj -> .exe` link lines, zero `error C|LNK|MSB` hits, both configs).

- **BLOCKER — StateSwapAction use-after-free — FIXED.**  The ruling-3a swap
  action held a raw `AudioProcessorValueTreeState&` in the PROCESSOR-owned
  history while every opt-in site targets a DELETABLE engine's APVTS.  Repro:
  engine "Load Preset" -> delete the tab -> Ctrl+Z (resurrects a NEW engine)
  -> Ctrl+Z again reaches `apply()` on the destroyed APVTS.  Fix (vendored,
  same patch file): a message-thread liveness registry — every APVTS registers
  at ctor / deregisters at dtor; `StateSwapAction::apply` holds a POINTER and
  no-ops-AS-SUCCESS on a dead owner (a false return makes UndoManager wipe the
  entire history), plus a root-type guard against pointer reuse.  Degrades to
  the recorded inert-entry class instead of a crash.
- **NEEDS-FIX 1 — TS7 change-stamp regression — FIXED.**  Removing the
  undo-path markDirty calls (Task 8) also removed the version-capture stamp
  feed for every choke transaction: a notes-only session left
  `getChangeStamp()` static and VersionCapture skipped takes it used to keep.
  Fix: `changeListenerCallback` feeds `markDirty()` (stamp-only post-Task 9)
  on every non-clear manager broadcast — restores the old coverage, adds
  attachment gestures, zero dirty-state effect.
- **NEEDS-FIX 2 — C/V/I/Plugins tab ADDS were not undoable — FIXED, coverage
  extended.**  The Task 7 add wrap was L/B/D-only while the recorded surface
  said "all adds" — the statement Jeff's rulings relied on.  Fix set:
  (i) `wrapTabAddUndo(ribbonTabId, label, asRider)` on the captureTabRecord
  spine (undo closes; redo resurrects from the add-time record);
  (ii) `onAddEngineRequest` wrap extended to ALL kinds it dispatches —
  Vox/Inst/Plugins wrap AFTER the engine/plugin pick — plus a page-count
  guard fixing a LATENT flaw: an at-cap add minted a bogus Add transaction
  against a pre-existing tab (undo would close an innocent tab);
  (iii) the Mixer strip-add cascade (`onVoxStripAdded`/`onInstStripAdded`)
  wraps gated on "actually spawned" — `spawnVox/InstTabIfMissing` now return
  the new tab id or -1, and resurrection/load re-entries spawn FIRST then add
  the strip, so the cascade no-ops and can never perform during an undo;
  (iv) suppression counter `mSuppressAddUndoWrap` for composite flows —
  Guitars/Basses adds and Vox/Inst duplicates suppress the mid-flow cascade
  wrap (it would capture the pre-Guitars / pre-clone page) and wrap
  themselves at the end with the full record ("Add Guitar N" /
  "Duplicate <name>"); Vox/Inst duplicates were themselves silently
  un-undoable — now covered;
  (v) Rusty add wraps at the ribbon callback (the add function is shared
  with load restore AND resurrection, which re-enters through it);
  (vi) "+ Add New Clip" composes library entry + tab spawn as ONE
  transaction — AudioLibraryAction first, tab rider second, so undo closes
  the tab then removes the entry, and redo restores the entry BEFORE the
  path-matching resurrect (the deleteTabWithUndo ordering).
- **NEEDS-FIX 3 — last-file "Delete Audio" auto-close — FIXED.**  The Task 4
  carry item resolved: `onClosePageForChannelId` captures the tab record and
  RIDES the browser's open Delete Audio transaction as a StructuralOpAction
  rider (no beginNewTransaction) — ONE Ctrl+Z restores library, blocks, AND
  the auto-closed tab.  ActionSet undo runs in reverse: the tab resurrects
  first, then the wholesale library restore overwrites any interim re-adds.
- **NEEDS-FIX 4 — page-SafePointer structural entries silently skip after a
  delete+resurrect cycle — RECORDED as a knowingly-carried caveat.**
  Sound-swap / chain-swap / program-pick entries bind SafePointers to the
  page OBJECT; resurrection creates a NEW page, so undo walking past a
  resurrection no-ops those entries.  Safe (no crash), lossy — the same
  degrade class as the detached-tree caveat, which WAS recorded; this one now
  is too.  The stronger fix (pageIndex-keyed page resolution through the
  editor at apply time, across 4+ page classes) is surfaced at close for
  Jeff's call, not built unrequested.
- **NIT — spike finding (c) was FALSE — notes corrected, built fix
  REVERTED.**  `Slider::Pimpl::mouseWheelMove` (juce_Slider.cpp:1164), text
  entry, double-click-reset, and the accessibility setter ALL wrap in
  `ScopedDragNotification` -> dragStarted/Ended -> attachment begin/end
  gesture: the wheel/keyboard bracket gap NEVER EXISTED in our vendored
  JUCE.  A dragging-flag fix built early in this close pass was reverted —
  it would have turned programmatic `slider.setValue(sendNotification)`
  calls into named history transactions (an unprompted behavior change).
  Corrections written at finding (c) and its Task 3 routing note;
  `juce_ParameterAttachments.h` is byte-identical to stock again (the
  vendored patch set stays exactly three files).
- **NIT — Rusty player-preset wrap guards — FIXED.**  The wrapped menu branch
  restored the upper id bound the unwrapped fallback kept, and
  `onWrapStructuralOp` gained a before==after record guard — re-loading the
  current preset no longer mints a phantom row + two snapshot files (uniform
  with the kit-load and chain-swap guards).
- **NIT — kit-load undo drops engine-less drum tabs — RECORDED.**
  `loadKitWithUndo` skips empty-xml tabs from its restore set, so a
  pre-existing EMPTY Drums tab doesn't come back on undo of a kit load.
  Marginal; disposition surfaced at close for Jeff.
- **NIT — en-GB "colour" in a new UndoActions.h comment — FIXED.**
- **NIT — stale "This action cannot be undone." in RibbonTabBar's unreachable
  defensive fallback — FIXED** (matches the batch's dialog sweep).
- **NIT — SharedUI EQ brackets repeat a 5-line manager ternary ~11 times —
  DEFERRED** as a later cleanup pass; green and correct as shipped.
- **Review-pass coverage caveats (for the §B.33 walk):** tab spawns that are
  SIDE EFFECTS of import/route flows remain un-wrapped as adds — grid
  drag-drop import spawn, browser route-to-new-page spawn,
  duplicate-file-drop new-page spawn, the Clips-duplicate flow, and
  `addVoxFromExport` (its spawn IS wrapped via the cascade; the compound
  flow's clip placement + optional chain-clone prompts land as separate
  transactions).  Multi-Ctrl+Z through those flows; the walk judges whether
  any warrant composition.
- **Reviewer's alignment verdict:** Tasks 1-9 + all five rulings present and
  verifiable in the diff; both vendored mechanism shifts inside the spike
  latitude; no unexplained diff content.

Tree after the review-fix pass: 66 entries — 59 Source modified + 3 vendored
JUCE + 2 docs modified + 2 new headers.  Re-gate green as above; nothing
committed yet (ONE batch commit at close).

## 2026-08-06 — HELD Work Log entry (applies at the §B.33 campaign pass per bulk-run R2)

Drafted by the doc-drafter at code-complete; TBDs filled and the /review-batch
outcome + review-fix pass folded in by the main session.  Applies to
`Implemented Work Log.md` — with the §5 STATUS flip — when §B.33 passes the
campaign walk.  Backfill the batch commit hash (here and in the §B.33 `blocks:`
line) at commit; stamp the full `HH:MM PT` at apply.  Held here per the
mammoth/badger/layout precedent.

---

### 2026-08-06 (time at apply) PT — QA-UndoCoverage — One global undo with EVERY action undoable + transaction-pointer dirty (the MERGED batch — QA-DirtyFlag absorbed as Tasks 8-9, `clean-pointing-stoat.md` banner points here): the processor-owned UndoManager is now the app's ONE history with all 10 engine APVTSes on it (3 sfizz private managers + the Rusty visible-page undo branch retired); three vendored-JUCE patches on the QA-0a precedent make the coverage total — write-time programmatic marking (automation replay, offline export, and load paths leave history byte-identical; selected after the spike found an audio-thread automation writer that defeats any caller-scoped suppression), attachment gesture naming ("param:<id>" — one line covers all 109 attachment sites), and replaceStateKeepingUndoHistory (engine preset loads had been wiping the ENTIRE app history through 27 live call sites), upgraded by ruling 3a's StateSwapAction so user preset picks are single undoable snapshot ops (with a liveness registry from the review pass so a swap whose engine died degrades inert instead of crashing); history labels went listener-driven off the real stack with owner keys (write-side shadow list + the 2026-04-26 cursor-drift hack retired); the dirty-but-not-undo families wrapped as six snapshot action classes over five new PatternManager restore seams; the Event Editor routed through the choke point (Ctrl+Alt+Z redo unified; Key Binds reference section); the re-scouted 87-site setValueNotifyingHost census closed (52 user gestures bracketed, 25 programmatic phase-marked, 8 already-excluded, 2 ambiguous resolved); structural undo shipped as identity-preserving snapshot resurrection on the PagePresetIO spine — tab delete/add/duplicate for ALL tab kinds (ruling 2a + the review pass extending adds and Vox/Inst duplicates to every kind incl. Mixer strip-adds and the Clips library-composed add), drum sound pick/swap/clear, kit loads as ONE transaction, Guitars/Basses/Rusty program switches (ruling 1a), and page/engine preset loads (ruling 3a), with three "This cannot be undone." dialogs corrected because it now can be; the TransactionTracker current/saved pointer replaces the touched-bool so undo can CLEAN the project (branch-kill; loads reset clean with an empty history); the depth menu's ~30-transaction retention lie confirmed and fixed to honest counts (ruling 5); MIDI-learn hardware streams stay excluded from history (ruling 4b); §B.33 authored (18 UND-B scenarios)

**Bucket:** Cross-cutting Infrastructure, System Pages, UI / L&F / Theming. Batch `long-rewinding-yak`. `blocks:` `<hash>` (the ONE batch commit — backfill at commit, here and in §B.33).

**HELD PER BULK-RUN R2.** Drafted at code-complete 2026-08-06; lands in the log when §B.33 (UND-B1..B22) passes the campaign walk. ONE batch commit per the bulk-run G4 convention — no batch smoke; per-task build gates (five exit codes 0, four `vcxproj -> .exe` link lines, zero `error C|LNK|MSB` greps) were the only in-batch gates; ALL functional verification rides the campaign pass.

#### Done

- **Task 1 — APVTS transaction-semantics spike + authority move.** The bounded source-read spike pinned eight findings (a-h, verbatim in the running notes; (c)'s wheel-gap half later proven FALSE at review — see the /review-batch outcome) that shaped the whole batch: the parameter->tree flush transacts EVERY write with the ctor manager (host/automation/programmatic included, contaminating whatever transaction is open); attachments produce UNNAMED transactions; begun-but-empty gestures leave no history row (load-bearing for exclusion); the depth menu is unit-based fiction; `replaceState` calls `clearUndoHistory()`; the plan's nullptr-manager premise was stale (QA-ModelShell TS1 already bound main + the 7 page-family engines — the manager had been silently RECEIVING transactions with nothing consuming them); and processBlock's automation-clip pass writes APVTS lanes ON THE AUDIO THREAD, so caller-scoped suppression cannot work against the async flush. Mechanism selected within the plan's latitude: **write-time programmatic marking** — vendored `static thread_local` phase flag + RAII `ScopedProgrammaticParamWrites` + a per-adapter atomic mark consumed at flush time (marked values write with `um = nullptr`); works for every APVTS instance automatically, and hand edits DURING playback still transact (thread_local keeps the audio thread's phase off the message thread). Five phase scopes installed (processBlock automation pass, setSongMode exit restore, `applyAutomationAtCurrentPosition`, `onSongModeChanged` exit restore, the whole of `BuilderPage::runOfflineLoop`). Authority move reduced to the editor side: StandaloneEditor's owned manager retired for `juce::UndoManager&` bound to `p.mUndoManager`; every consumer follows through the reference with zero call-site churn.
- **Task 2 — engines join the one history (13=A) + the replaceState neutralization.** Verification half: all 7 page-family engines already on the global manager (grep-exact: all 11 APVTS constructions bind it; zero pass nullptr). Flip half: Guitars/Basses/RustyDrums private managers replaced by the global reference (declaration-before-apvts ordering preserved), the three `getUndoManager()` accessors deleted, and the J-8 Rusty visible-page branches in `globalUndo`/`globalRedo` removed — Rusty ARIA knob edits now undo from ANY page. Spike finding (f) made the neutralization mandatory before the flip: vendored `replaceStateKeepingUndoHistory` (replaceState minus the history wipe; the knowingly-carried caveat — pre-swap param rows bound to detached trees become inert no-ops — is documented at the patch and strictly better than wiping the app history) switched in at ALL 27 engine-APVTS replaceState sites across 20 files; the THREE main-APVTS sites deliberately keep the clearing variant (all load paths — a load is supposed to start with an empty history; Task 9 adds the explicit load-end reset).
- **Task 3 — listener-driven history + owner-key labels (13=ii as superseded).** ONE vendored line names every attachment gesture — `ParameterAttachment::beginGesture` now begins `"param:<paramID>"` — superseding the plan's per-site hooks (the attachment census found 109 occurrences across 26 files; the vendored line covers every current and future attachment with zero app-side churn). StandaloneEditor became the manager's ChangeListener; `rebuildHistoryLabels()` derives `mHistoryLabels` + `mHistoryCursor` from the REAL stack (`getUndoDescriptions` reversed + cursor = undo depth + redo descriptions appended); the window needed zero interface changes. Write-side label maintenance deleted wholesale — doUndoAction's append/trim/cap block, the manual cursor ++/--, and the 2026-04-26 "cursor can drift on no-op undos" hack (the cursor is derived now; it CANNOT drift). `historyDisplayFor` resolves `"param:<pid>"` to `"<owner>|<pretty>"` via the existing automation display-name machinery + the new `ownerKeyForParamId` (sfizz trio by prefix; rig-owned engines by walking every tab's APVTSes; `"app"` fallback). Owner keys are LABELS ONLY per the 2026-08-06 supersession — no hide/skip machinery exists, because under every-action undo no dead transaction can exist. Per-tab page creators stamp `lay<idx>` / `bass<idx>` / `drm<idx>` / `vox<idx>`; cross-tab surfaces stay `app`.
- **Task 4 — the dirty-but-not-undo gestures wrapped (12-ii).** Six action classes (the plan's four split by payload correctness): `PatternListSnapshot`/`Action` (project-slice — see Deviations), light `PatternRenameAction`/`PatternColorAction`, `MarkerSetSnapshot`/`Action` (three ruler lists + current-TS uid, restored atomically), `AudioLibrarySnapshot`/`Action` (entries + manual groups, optionally + blocks — delete cascades into clips), `AutomationTemplateSnapshot`/`Action`. Five new PatternManager restore seams on the `restorePatternList` precedent + raw getters. The Split-by-Engine local undo machinery DELETED and promoted to shared `ArrangementGrid::capturePatternSlice()`/`applyPatternSlice()` + grid helpers (`performPatternSliceOp` / `performMarkerSetOp` / `performPatternTsOp` — a pattern-TS gesture rides a MarkerSetAction inside ONE transaction). ~30 sites wrapped across pattern list / markers / library / templates (full list in the Task 4 notes entry); the transport pattern-Delete dialog's "This cannot be undone." text removed — it can now. Row renames now bracket via `beginEdit("Rename Track")`/`commitEdit` (the plan's "renames ride ArrangementEditAction" verification came back NEGATIVE). Import Audio's library entry + clip defaults RIDE the import transaction so undoing an import no longer strands a browser entry. Deliberately NOT wrapped (recorded): the auto-spawned linked TS marker (established outside-the-undo-domain seam) and `beginAddRenderToProject`'s fragments (dominant path routes through the wrapped `onApplyLibraryProperties`; structural half composed at the review pass — see the /review-batch outcome).
- **Task 5 — Event Editor unification (12-iii + 14=a).** Tasks 1+3 had already dissolved the desync premise (the editor's `mUM` points at the one manager; labels rebuild off the real stack), so Task 5 delivered the routing: all four direct transaction sites re-routed through the UndoContext choke point via `performViaCtx` (lane edits now hit doUndoAction — named transaction + markDirty + synchronous label rebuild), undo/redo dispatch through `ctx.undo/redo` -> globalUndo/globalRedo, redo unified to Ctrl+Alt+Z (local Ctrl+Y retired; menu label updated), and a display-only "Event Editor Key Binds" tab (11 rows, MouseRefRow mechanism, the QA-Fd VocalEditors precedent).
- **Task 6 — the census + manual-push bracketing (12-iv).** The stale "~101 sites" number re-scouted from scratch: **87 `setValueNotifyingHost` sites** — 52 USER-GESTURE / 25 PROGRAMMATIC / 8 ALREADY-EXCLUDED (inside Task 1's phase scopes, incl. every applicator lambda) / 2 AMBIGUOUS (both resolved by splitting: vocal preset picks bracketed as single gestures, their load halves already phase-marked). Plus the one audio-thread `setValue` (already scoped) and the NAM-IR snapshot `operator=` (whole-function phase mark; undo of an A/B flip self-heals through the `ab_slot` parameterChanged handler). User gestures bracket via new `Source/Standalone/UndoBracket.h` (`beginParamUndoGesture` opens `"param:<id>"` — same marker the vendored attachment naming uses, so labels resolve identically); 52 gestures became **67 insertions across 16 files** (the shared EQ writer is bracketed at its 11 discrete callers, not inside the helper). 14 new programmatic phase scopes incl. `resetToBlankState`, the sfizz `loadKit` CC sweeps, the load-path force-fire loops, and `MidiLearnRegistry::dispatchEvent` (hardware CC streams have no gesture boundaries — excluded; undoability posed to Jeff, ruled 4b below).
- **Task 7 — structural undo: identity-preserving snapshot resurrection.** `UndoSnapshotStore.h` (session-scoped files under `Documents/BaySickDAW/UndoSnapshots/`; crash-stray sweep at ctor, sweep + `clearUndoHistory()` at editor dtor — the history dies WITH the editor) + `StructuralOpAction` (undoFn/redoFn + owned snapshot files deleted in its dtor). Identity scout confirmed the spine: everything load-bearing keys off pageIndex; ribbonTabId is session-monotonic by design; effect-slot uuids round-trip inside the rack blob; strip params / piano-roll content / automation lanes SURVIVE tab deletion in the model — so the snapshot is the PagePresetIO chain slice + the tab record. Resurrection generalizes the duplicate spawns (forced pageIndex/name = the resurrect path). Wrapped: `deleteTabWithUndo` (all 14 `onDeleteRequested` wirings + FOUR more found un-rewired at ruling 2a), "+"-menu add+pick as ONE transaction, `duplicateTabWithUndo` (all 14 wirings), drum sound pick/swap/clear via `DrumPage::performSoundSwapGesture` (before/after full-chain snapshots; wrapped at the picker dispatch terminals only — the KIT loader calls the bare helpers programmatically), `loadKitWithUndo` (ONE transaction; captures every DrumPage before the teardown sweep + the created set after; no-change guard), and `InstPage::switchSfizzProgram` extracted + wrapped (NO snapshot files — the session program-state cache carries the CC state; InstPage gained the UndoContext it never had, wired `inst<idx>`). Undo of a sample-engine load is itself a LOAD (async) — Jeff's knowingly-accepted property.
- **Jeff's five rulings (2026-08-06: 1a / 2a / 3a / 4b / 5=required) — all implemented same day.** **(2a)** ALL remaining tab deletes undoable: `captureTabRecord` mirrors serializeTabsInto's per-kind record; `resurrectTabFromRecord` mirrors the project-restore branches verbatim (same creation order, race-safe sfizz loads, quiet-then-loud Plugins hook staging); Clips/Vox/Inst additionally capture the library+blocks slice (restored BEFORE resurrect so the Clips path-match finds its entries); Rusty additionally captures the pattern slice (its delete cascade clears the Rusty roll on every pattern; restored AFTER resurrect); the Rusty delete warning drops "This cannot be undone.". **(1a)** Rusty program switch undoable via the new page hook `onWrapStructuralOp` — composite capture = the tab record (kit path + CC state) + the pattern slice; restore re-enters through resurrectTabFromRecord (no-op while the tab is alive -> pure state restore); the switch prompt drops "This cannot be undone."; first-load stays unwrapped (nothing to restore); the player-preset load routes through the same hook. **(3a)** Preset loads become snapshot ops: `replaceStateKeepingUndoHistory` gains an optional undoTransactionName — non-empty performs ONE `StateSwapAction` (wholesale old <-> new tree; the undo/redo re-assignment runs under `ScopedProgrammaticParamWrites` so the rebind flush cannot perform nested actions mid-undo). Opt-in ONLY at direct user preset gestures (the four engine editors, incl. Harmless "Init Patch"); load paths and calls inside wrapped structural gestures keep the silent default. Page presets: LayersPage/BassPage `performChainSwapGesture`; DrumPage rides performSoundSwapGesture; PluginsPage gains a plugin-state-blob swap gesture + `plug<idx>` UndoContext wiring. **(4b)** MIDI-learn stays excluded — the Task 6 phase mark stands. **(5)** Depth honesty REQUIRED and shipped: `setMaxNumberOfStoredUnits(1, N)` — maxUnits 1 keeps the unit test permanently over, so `minimumTransactionsToKeep` IS the menu number; ctor default (1, 100), menu cases (1, 100/250/500/1000); trade-off stated (an honest 1000 can hold heavy snapshot actions N-deep in RAM).
- **Task 8 — TransactionTracker (absorbed stoat T1; dual counter dropped per the merge ruling).** `VibeSynthProcessor::TransactionTracker mTxTracker` beside the manager: current/saved only, branch-kill (new transaction while current < saved -> saved = -1), edge-fired `onDirtyChanged(bool)`. Feed = the editor's UndoManager ChangeListener deriving events from manager depth deltas (implementation latitude — see Deviations); deltas apply by count; the cap-drop-vs-new-transaction ambiguity resolves via the top-name change. The three undo-path markDirty calls REMOVED — undo can now CLEAN the project, the exact inversion the absorbed stoat spec exists to fix (the version-capture stamp feed those calls also carried was restored at the review pass — see the /review-batch outcome).
- **Task 9 — touch-model retired (absorbed stoat T2-T4).** `ProjectManager::mDirty` + `setDirtyInternal` deleted; `isDirty()` delegates to the tracker; `markSaved()` = tracker.onSave() (saveProjectAs rides it); `clearDirty()` repurposed as the load-boundary reset (`clearUndoHistory()` + snapshot sweep + tracker.onLoadReset()) at exactly the load/new boundaries — loads open clean, row 1 = the first post-load edit. The dirty edge forwards through the EXISTING `ProjectManager::onDirtyChanged` hook, so the title asterisk / confirmDiscardChanges / quit gate kept their wiring untouched. Autosave verified FIRST, changed NOTHING (`writeBackup` never cleared dirty and fires unconditionally — the tracker preserves both by construction). Freeze/unfreeze dirty semantics ride mammoth TS7 untouched. The ~18 direct markDirty sites KEPT as the version-capture change stamp (see Deviations).
- **Review-fix pass (close sequence step 2, same day).** The /review-batch BLOCKER + four NEEDS-FIX findings fixed and re-gated green first try: the StateSwapAction liveness registry (vendored), the TS7 stamp feed restoration, the all-kinds tab-ADD coverage extension (wrapTabAddUndo + cascade gating + suppression counter + the Clips library-composed add + Vox/Inst duplicates + Rusty/Guitars/Basses adds + the at-cap bogus-wrap latent flaw), and the last-file Delete Audio auto-close rider. Full detail in the /review-batch outcome section below.
- **Close artifacts.** Master Test Plan §B.33 authored — 22 scenarios (UND-B rows; B19-B22 added at the runtime pass), `blocks:` = the batch commit (backfilled). Two plan-authoring gaps in the yak plan corrected-by-record (see Found #14). Main Plan §5/§9 handling per the close sequence; this entry HELD here per R2.

#### Found along the way

1. **The APVTS flush transacts EVERY write** — host, automation, programmatic — into whatever transaction is open, and one writer is the AUDIO THREAD (processBlock's automation-clip pass), so any caller-scoped "suppress around the pass" leaks against the async flush.
2. **`replaceState` wipes the entire app undo history** — ~27 live engine-APVTS call sites; once all engines share the manager, ANY preset load would have destroyed the whole session's history (and undo across an unneutralized swap writes into detached dead trees).
3. **Two stale plan premises:** the "nullptr UndoManager" wiring claim (QA-ModelShell TS1 had already bound main + 7 engines — the manager was silently receiving transactions with nothing consuming them) and the "~101 sites" census (real number: 87).
4. **The depth-menu lie confirmed:** `setMaxNumberOfStoredUnits(n, 30)` is unit-based and every action is over-budget, so 100/250/500/1000 all retained ~30 transactions.
5. **Attachment gestures are unnamed** (fixed by the vendored naming line). The spike's companion claim — that wheel/keyboard slider changes fire NO transaction bracket — was proven FALSE at the review pass: the vendored Slider wraps wheel, text entry, double-click-reset, and the accessibility setter in `ScopedDragNotification`, which brackets via dragStarted/Ended. No gap existed.
6. **Track-row renames bypassed `commitEdit` entirely** — the plan's "renames ride ArrangementEditAction" verification came back negative.
7. **The census missed the EQ A/B Compare's APVTS-only branch** — caught by the sweep agent; bracket added.
8. **Spec collision (surfaced, not resolved unilaterally):** the Rusty program-switch confirm — Jeff-approved wording — said "This cannot be undone.", colliding with the every-action ruling his own merge decision made.
9. **Four user-delete wirings were un-rewired** after the pass-1 sweep (the Rusty `ribbonId` fireDelete + the three Clips/Vox/Inst dynamic-id wirings) — found and rewired at ruling 2a.
10. **Known label limitation (recorded deliberately):** Vocal/NAM-IR/Pedals use UNPREFIXED param ids, so with multiple same-kind tabs the owner-key rig walk first-matches and can tag the wrong tab INDEX on a history label. Labels only — the action binds to the correct tree and undoes the right engine regardless.
11. **Task 4 / Task 7 composition seam:** undoing "Delete Audio" when it was the last file on a page restored library + blocks but NOT the auto-closed tab.
12. **`BaySickGuitarsProcessor::sendCc` / `BaySickBassesProcessor::sendCc` have ZERO callers** — pre-existing dead code; this batch did not orphan them.
13. **InstPage's `rebuildProgramCombo` / `onProgramComboChanged` declarations are DEAD** (no definitions) — pre-existing, discovered during the program-switch extraction.
14. **Two plan-authoring gaps in the yak plan** (Jeff's catch on the second): the batch-close checklist still opened "Tell Jeff to run do_build.bat" (stale since the 2026-07-25 builds handover) and omitted the `/review-batch` step from the mandatory close sequence.
15. **MIDI-learn hardware CC streams have no gesture boundaries** — an un-bracketed stream would contaminate whatever transaction is open; making them undoable needs idle-timeout bracketing.
16. **Autosave never cleared dirty** (explicit in the code: backups are a safety net) and fires unconditionally — verified before Task 9 touched anything.
17. **/review-batch BLOCKER — StateSwapAction raw-reference lifetime hole:** the ruling-3a action could apply() on a destroyed engine APVTS (preset load -> tab delete -> undo x2) — the exact bug class the plan's ORDERING RULE was written to kill, reachable through the review's documented repro.
18. **/review-batch — TS7 change-stamp regression:** Task 8's markDirty removal also deleted the version-capture stamp feed for choke transactions.
19. **/review-batch — C/V/I/Plugins adds (and Vox/Inst duplicates) were not undoable**, the recorded "all adds" surface was wrong, and the L/B/D add wrap carried a latent at-cap flaw (bogus Add transaction against a pre-existing tab whose undo would close it).
20. **/review-batch — two recorded degrades:** page-SafePointer structural entries silently skip after a delete+resurrect cycle (same class as the detached-tree caveat), and `loadKitWithUndo` drops engine-less drum tabs from its restore set.

#### What was done about each finding

- **1 -> selected the mechanism:** vendored write-time programmatic marking (thread_local phase + adapter mark consumed at flush); five phase scopes at the programmatic writer passes; 14 more scopes in Task 6.
- **2 -> neutralized before the Task 2 flip:** `replaceStateKeepingUndoHistory` at all 27 engine sites; the 3 main-APVTS load-path sites deliberately keep the clearing variant; ruling 3a later upgraded the user-facing preset gestures to StateSwapAction snapshot ops.
- **3 -> Task 1 reduced to the editor-side repoint; Task 6 re-scouted from scratch** — both recorded as stale-premise corrections, no plan reopen.
- **4 -> surfaced to Jeff with the spike; ruled 5=required; honest (1, N) shipped.**
- **5 -> the vendored naming line covers attachments.** The wheel-gap half of the finding was corrected at review (NO gap exists — see the /review-batch outcome); a fix built for it during the close pass was reverted, and the spike notes carry the correction so nobody builds it again.
- **6 -> fixed in Task 4** (`beginEdit("Rename Track")`/`commitEdit`, the fix shape already present in the same function's Move/Delete handlers).
- **7 -> bracket added in Task 6** (both A/B branches).
- **8 -> surfaced as a spec collision; Jeff ruled 1a** — wrapped via `onWrapStructuralOp`, dialog wording corrected.
- **9 -> all four rewired through `deleteTabWithUndo`** in the ruling-2a pass.
- **10 -> recorded; exact-index labeling deferred unless Jeff wants it** (needs per-APVTS owner threading through the vendored attachment ctor chain).
- **11 -> FIXED at the review pass:** the auto-close now rides the Delete Audio transaction as a StructuralOpAction rider — one Ctrl+Z restores library, blocks, and the tab.
- **12 + 13 -> routed at close per Rule 3** (below).
- **14 -> corrected-by-record:** Main Plan §0's close sequence governs and RAN at this close (`/draft-doc batch-close` -> `/review-batch` -> apply -> commit); the stale build line is superseded by the 2026-07-25 handover. The closed plan file is not reopened.
- **15 -> excluded via the Task 6 phase mark; undoability posed to Jeff; ruled 4b — stays excluded.**
- **16 -> nothing changed** — the tracker preserves both autosave properties by construction; finding noted per the plan's verify-first instruction.
- **17 -> FIXED (vendored):** message-thread liveness registry; apply() no-ops-as-success on a dead owner + root-type guard against pointer reuse; degrades to the recorded inert-entry class.
- **18 -> FIXED:** the ChangeListener derivation feeds `markDirty()` (stamp-only) on every non-clear manager broadcast.
- **19 -> FIXED, coverage extended to every kind** (wrapTabAddUndo + engine-request extension + gated strip-cascade wrap + suppression counter for composite flows + the Clips library-composed add + the page-count at-cap guard); side-effect spawns of import/route flows recorded as walk caveats.
- **20 -> RECORDED as knowingly-carried caveats**; the stronger fixes (pageIndex-keyed page resolution at apply time; empty-tab records in the kit-load restore set) surfaced at close for Jeff's call.

#### Deviations from plan

- **Task 1's exclusion mechanism differs from the plan's parenthetical** ("drop + re-begin around the applicator pass") — impossible against an async flush fed by an audio-thread writer; the write-time marking patch was selected within the plan's "mechanism per spike findings" latitude. The authority move itself SHRANK to the editor-side repoint (TS1 had already wired the processor side).
- **Task 2 GREW the replaceState neutralization** (27 sites / 20 files) — not in the plan; made mandatory by spike finding (f) before the flip could be safe.
- **Task 3's per-site attachment hooks superseded by ONE vendored line** (109 attachment occurrences across 26 files made per-site hooks a sprawling sweep; the vendored begin covers every current and future attachment).
- **Task 4's `PatternListAction` payload corrected:** the plan's "the one Pattern + index" is INCORRECT for remove (`removePattern` cascades — blocks erased, higher indices re-numbered, currentPattern clamped); the project-slice snapshot is the smallest CORRECT payload and matches the shipped Split-by-Engine shape. Four action classes became six; five PatternManager restore seams + the BrowserPanel UndoContext plumbing were unenumerated but required.
- **Task 5's desync premise was already dissolved by Tasks 1+3** — delivered as choke-point routing + redo unification + Key Binds, not a desync fix.
- **Task 6:** census = 87 not ~101; 52 gestures = 67 insertions (shared EQ writer bracketed at its discrete callers, not inside the helper).
- **Task 8's tracker feed** = the ChangeListener's manager depth deltas, not the plan's "doUndoAction/globalUndo/Redo notify" — choke transactions AND attachment gestures both broadcast, so explicit notifications would double-count what the listener already sees. Recorded caveat: a change window mixing an undo with a new transaction cannot be produced by mouse-driven gestures.
- **Task 9 keeps the ~18 direct markDirty sites AS the version-capture change stamp** (the plan expected them removed) — the stamp is capture's change detector and removing the calls would shrink its detection; dispositioned wholesale (stamp-only, zero dirty effect). `ApvtsDirtyTracker` stays whole for the same reason (stamp feed + the separate lock-free audio gate).
- **Task 3's "depth-menu semantics preserved" superseded by ruling 5** — the preserved values were preserving a lie; honest counts shipped on Jeff's required call.

#### Routed at close (Rule 3)

- **Dead `sendCc` (Guitars + Basses processors, zero callers, pre-existing):** not cleaned in-batch (this batch did not orphan them). Routing disposition: <TBD — Jeff's call at close>.
- **Dead `InstPage::rebuildProgramCombo` / `onProgramComboChanged` declarations (pre-existing, no definitions):** same treatment. Routing disposition: <TBD — Jeff's call at close>.
- Everything else found in-batch was fixed in-batch per the QA default; the DirtyFlag handover is DISSOLVED by the merge (executed here as Tasks 8-9) — nothing to route there.

#### `/review-batch` outcome

- **Ran 2026-08-06 (the step Jeff's catch restored to this close): 1 BLOCKER / 4 NEEDS-FIX / 6 NIT.** BLOCKER (StateSwapAction use-after-free on deleted engine APVTSes) fixed via the vendored liveness registry + type guard. NEEDS-FIX: TS7 stamp regression FIXED (listener-side stamp feed); C/V/I/Plugins adds FIXED with coverage extended to every kind incl. Mixer strip-adds, Vox/Inst duplicates, Rusty/Guitars/Basses adds, and the Clips library-composed add (plus the latent at-cap bogus-wrap flaw); last-file Delete Audio auto-close FIXED as a same-transaction rider; the page-SafePointer resurrection degrade RECORDED as a knowingly-carried caveat (stronger fix surfaced for Jeff). NITs: spike finding (c) proven false (notes corrected, built fix reverted — `juce_ParameterAttachments.h` back to stock); Rusty preset-wrap bound + no-change guard fixed; kit-load empty-tab drop recorded (disposition Jeff's); en-GB comment + stale RibbonTabBar dialog text fixed; the SharedUI EQ ternary repetition deferred as cleanup. Re-gate GREEN FIRST TRY. Full detail in the running-notes /review-batch entry (this file, same date).

#### Diagnostic instrumentation

- None added or removed this batch.

#### Carry-forward contradictions

- **None against Carry-Forward §1-§3.** The constructor-order rule held everywhere a manager reference was threaded (declaration before apvts, all three sfizz flips); resurrection enters through the model creation paths with the sfizz race-safe loads and teardown order intact.
- **Supersessions recorded at the point of reversal:** docket 13's hide/skip machinery NOT built (2026-08-06 banner — no dead transaction can exist under linear every-action undo; owner keys are labels only); the 2026-04-26 cursor-drift hack obsolete by construction; three "This cannot be undone." dialog texts corrected; the depth-menu values' "semantics preserved" plan line superseded by ruling 5.
- **Notes to carry:** (1) engine-APVTS state swaps go through `replaceStateKeepingUndoHistory` — never raw `replaceState` (the three main-APVTS load paths are the deliberate exceptions); (2) the applicator-lambda exclusion is DYNAMIC — any future third invoker of `mAutomationApplicators` must add its own `ScopedProgrammaticParamWrites` scope; (3) three vendored JUCE files carry batch-critical patches (`juce_AudioProcessorValueTreeState.h`/`.cpp` — write phase, keep-history + StateSwapAction, APVTS liveness registry — and `juce_ParameterAttachments.cpp` — gesture naming); any vendored-JUCE update must re-apply them or undo coverage silently degrades; (4) the undo history + snapshot store die WITH the editor (dtor clear + sweep) and reset at every load boundary via the repurposed `clearDirty()`; (5) the depth cap is `(1, N)` on purpose — the maxUnits value IS the honesty mechanism, do not "fix" it back; (6) user-initiated tab ADDS wrap at the gesture sites, never inside the shared spawn/creator functions (load restore and resurrection re-enter through those), and the strip-cascade wrap keys on spawn*IfMissing's returned id + the `mSuppressAddUndoWrap` counter — a new composite add flow must either suppress-and-wrap-at-end or accept the mid-flow fresh-state wrap.

#### Files touched

66 tree entries: 59 Source modified + 3 vendored JUCE + 2 docs modified + 2 new headers. By area: **Vendored JUCE** — `juce_AudioProcessorValueTreeState.h`/`.cpp` (programmatic write phase + `ScopedProgrammaticParamWrites`; `replaceStateKeepingUndoHistory` + `StateSwapAction` + the liveness registry), `juce_ParameterAttachments.cpp` (gesture naming). **New** — `Source/Standalone/UndoBracket.h`, `Source/Standalone/UndoSnapshotStore.h`. **Core** — PluginProcessor.h/.cpp (TransactionTracker, phase scopes, sfizz creation sites, brackets), PatternManager.h/.cpp (restore seams + raw getters), ProjectManager.h/.cpp (touch-model retirement). **Standalone** — StandaloneEditor.h/.cpp (authority move, listener-driven history + owner keys, structural spine incl. deleteTabWithUndo / duplicateTabWithUndo / captureTabRecord / resurrectTabFromRecord / wrapTabAddUndo, kit-restore sites), UndoActions.h (six snapshot classes + StructuralOpAction), BuilderPage.h/.cpp (Split promotion, BrowserPanel wraps, offline-loop scope), EventEditor.h/.cpp, KeyBindings.h/.cpp, KeyBindsWindow.cpp, LayersPage, BassPage, DrumPage (sound-swap gesture), PagePresetIO, FxRackPresetIO, BaySickRustyDrumsPage, PluginsPage, MixerPage, MixerTrackStrip, RibbonTabBar.cpp, SharedUI, EffectEditorPanels. **MIDI** — MidiLearnRegistry.cpp. **Engines** — the three sfizz processors (Guitars/Basses/RustyDrums .h/.cpp), BaySickNAMIR (processor + editor), BaySickPedalsProcessor, the BaySickVocal family (processor + Vocal/Align/Pitch editors), BaySickSynth (processor + editor + BssEditorComponents), BaySickBass (processor + editor), Harmless (processor + editor), VibePlayer (processor + editor), Inst/InstPage. **Docs** — the paired batch plan + running notes (incl. this held entry), Test Plans/v1-master-test-plan.md (§B.33, 18 UND-B rows).

#### Commit(s)

`<hash>` — the ONE batch commit (bulk-run G4 convention): Tasks 1-9 + the five rulings + the review-fix pass + §B.33 + the close docs. Build gates green in both configs at every task on the five-exit-codes / four-link-lines / zero-error-greps criterion; two first-run failures, both re-run clean same-pass (Task 4: two real defects — free-struct qualifiers + a private-method call routed through the new public `refreshPatternTab()`; Tasks 7-9 combined: three missing pieces in DrumPage.cpp alone); the ruling-set gate AND the review-fix re-gate both passed clean FIRST TRY on full rebuilds. NO batch smoke — all functional verification rides the §B.33 campaign pass (R2).

#### Next action

- **QA-Soundness ([`keen-combing-heron.md`](../Batch%20Plans/keen-combing-heron.md)) is next** — the last G4 batch before the boundary R3 review + smoke (merged order: … layout -> yak -> heron). This batch's functional verification — the seven-gesture multi-surface unwind, the structural resurrection round-trips, the dirty-pointer / branch-kill scenarios, and the playback-leaves-history-untouched checks — rides §B.33 (UND-B1..B18) at the campaign pass, where this entry applies with the §5 STATUS flip.

## 2026-08-06 — JEFF'S RUNTIME CHECK: the batch's headline feature DID NOT WORK.  Root causes + the regression-fix pass

Jeff ran the freshly built app and found: (1) Ctrl+Z dead everywhere while
the Edit-menu Undo works; (2) NO param/knob edits in the history and none
undoable, app-wide ("every knob I try"); (3) the Rusty program/player pick
not undoable; plus two layout items (browser collapse strip; Rusty button
placement).  He also ruled: **1A** (first Rusty pick undoable), **2B**
(collapsed-divider drag opens at the threshold), ALL global keys fixed,
ONE unified undo machinery, and recordings undoable from the Builder.

OWNERSHIP, for the record: param undo WORKED pre-batch through the app's own
undo actions; this batch rerouted param undo onto the APVTS flush path and
shipped it without a runtime check.  Whether a given hole pre-dates the
batch is irrelevant -- the batch's charter was "undo works."  The static
/review-batch could not catch runtime behavior, and under bulk-run nothing
ran the app until Jeff did.  A runtime sanity pass of the headline feature
now precedes any future hand-back regardless of bulk-run.

### Root causes (all confirmed by code trace, five-agent diagnosis + first-hand verification)

- **Param undo Break A (app-wide):** LAZILY-registered params (QA-0a --
  mixer strips, buses, master, EQs, sends, rack effects; in an unsaved
  session everything registered after the ctor layout) never get an
  adapter->tree binding: `updateParameterConnectionsToChildTrees` only runs
  on wholesale state (re)assignment.  Their flush hits the invalid-tree
  else-branch (`setProperty` on a null object = no-op, and hard-coded
  nullptr um anyway) -- so the begun `"param:<id>"` gesture transaction
  stayed EMPTY and empty transactions leave no history row.  Values still
  worked (DSP reads the raw atomic) and saves still worked (QA-Ef's
  writeProcessorState materializes nodes separately), so undo was the only
  casualty -- invisible pre-batch because nothing consumed the flush
  transactions.  **FIX (vendored, same patch file):** `addParameterAdapter`
  now binds the child tree at registration when `state` is valid, with the
  value property PRE-SET to the param's live value (an id-only node would
  reset the param to default at setNewState -- the known QA-Ef failure
  mode); appended with nullptr um.
- **Param undo Break B (Rusty/Inst ARIA panels):** all four ARIA widget
  classes built their SliderParameterAttachment WITHOUT the UndoManager arg
  (defaults nullptr) -- no gesture ever began, so knob turns appended into
  whatever transaction was open (fused into the last structural row).
  **FIX:** pass `mBinding.apvts->undoManager` at all four sites; same fix
  at VibePlayer's detune-mode ParameterAttachment (a WRITE path with
  nullptr um).  The three EffectEditorPanels chain-sel ParameterAttachments
  are LISTEN-only (their writes go through UndoBracket-bracketed onChange
  lambdas) -- left as-is.
- **Ctrl+Z (and EVERY global bind incl. Space) dead with focus in any
  contained window:** `KeyPressMappingSet::keyPressed` only fires when the
  command manager can resolve a target; `setFirstCommandTarget` was never
  called, so resolution walked UP from the focused component -- and
  WorkspaceWindows are parentless desktop components, so the walk found
  nothing and the mapping set silently declined.  The per-window
  `addKeyListener(set)` registrations delivered the KEY but not the TARGET.
  The Edit menu worked because it calls globalUndo() directly (id 201).
  **FIX:** `mCmdMgr.setFirstCommandTarget(this)` in the ctor (nulled in the
  dtor) -- revives Ctrl+Z / Ctrl+Alt+Z / Space / every global bind in every
  contained window + the history window.  **Unification rider:**
  DrumKitGrid's local Ctrl+Z/Ctrl+Alt+Z keyPressed handlers deleted -- they
  swallowed the key even with a dead context (PianoRoll's B-5 migration,
  finally applied to the straggler).
- **Rusty pick undo -- three real causes, all fixed:** (i) the resurrect
  branch read the persisted kit ref ("library:<rel>") as a RAW path --
  existsAsFile() false -- the whole restore silently skipped, killing every
  wrapped Rusty undo on a library kit; now decoded via
  `SampleLibrary::resolvePersistedRef` (mirroring the engine's own
  setStateInformation).  (ii) a player-preset load targeting a DIFFERENT
  program did its real work in an ASYNC confirm callback the wrap closed
  before -- the no-change guard ate the empty transaction and the switch ran
  unwrapped; the wrap moved to the TERMINALS inside loadPlayerPresetFromFile
  (incl. inside the confirm's OK), and that dialog's stale "This cannot be
  undone." dropped.  (iii) the first pick of a session (None -> program) was
  exempt by design -- Jeff ruled 1A: now wrapped ("Load Rusty Program" /
  "Load Player Preset"); new `BaySickRustyDrumsPage::unloadToNone()`
  (teardown without reload + UI reset) and the editor's wrap-restore lambda
  recognizes an engine-less before-record and unloads instead of calling
  resurrectTabFromRecord (which no-ops on empty engineData for a live tab).
- **"Two machineries" CORRECTION:** my earlier "drum/master/bus faders would
  produce two rows per drag" claim was WRONG -- those faders write the
  PatternManager's MixerState (non-APVTS; MixerStateAction is their ONLY
  mechanism) while param-backed strips ride the APVTS flush.  No control
  files two rows; the two mechanisms cover two disjoint data models.  To be
  proven at the runtime sanity pass (one drag = one row on both fader
  classes), recorded here so the false claim doesn't outlive its retraction.
- **Recording (Jeff's confirm): a take is now ONE undoable gesture.**
  `commitRecordingResult` captures a pattern slice (blocks + captured MIDI
  roll notes) + a library slice around the whole commit and performs
  "Record Take" (PatternListAction + AudioLibraryAction rider, one
  transaction) -- undo removes the take from the PROJECT (library entries,
  grid blocks, MIDI notes); the WAV files stay on disk.  Skipped when
  nothing landed (stop during count-in).  KNOWN RESIDUE: a master-capture
  (routeChannel 0) take also creates an Audio row strip; undo does not
  tear the strip back down (empty strip remains) -- recorded for the walk.

### Layout items

- **Browser collapse (ruling 2B):** collapsed = width ZERO; the 28px
  click-strip, its grip/arrow paint, `onExpandRequest`, the
  PointingHandCursor swap, and dead `doToggleBrowser()` all deleted.  The
  5px BrowserEdgeGrip now stays visible in both states, paints a chevron
  when collapsed, seeds its drag from zero, and the setWidth lambda opens
  the panel once the outward drag passes the collapse threshold (136px),
  landing on the magnetic 180 default.
- **Rusty buttons (Jeff's screenshot):** Program combo (160) + Player
  Preset button (110) moved to the WINDOW title strip via
  `mPageMenuBar->addExtraRightComponent` in the Rusty page-show branch --
  the mount lays out INSIDE the menu bar whose right edge already excludes
  the close/fill chrome (the T3 failure laid out against the whole window
  and slid behind the chrome; structurally impossible on this mount; ~899px
  free vs 270 needed at the fixed 1047px window).  The Aria band (32px,
  hostTitleBar) now hosts the section TAB ROW (Main/Kick/Snare/...) --
  AriaControlPanel::resized places the buttons centered in the band when a
  title bar exists; band-less hosts (Guitars/Basses InstPage) keep the
  artwork-overlay placement as the fallback.  All four stale ruling
  comments rewritten (BaySickRustyDrumsPage.cpp x2, AriaControlPanel.h,
  StandaloneEditor.cpp) -- third reversal on these buttons, paper trail
  updated so no session "fixes" it back.  G-16's now-caller-less
  `addHostedTrailingWidget` + the mHosted machinery deleted from
  BaySickTitleBar (orphaned by this change; own-dead-code rule).

Build gate: RUNNING at entry time; result appended below.  After green:
Debug runtime sanity pass (mixer fader / Rusty ARIA knob / engine-editor
knob / automated param / EQ + rack knob -> row appears + Ctrl+Z reverts;
Ctrl+Z + Space from inside page windows; Rusty pick undo round-trip;
record-take undo) -- then hand-back to Jeff for the real check.

Gate result (regression-fix pass, first run): COMPILE CLEAN in both configs
(zero error C / zero stale hits); Release FAILED at the LINK step only --
LNK1104 on BaySickDAW.exe, which Jeff has open (he found the regressions by
running it).  Debug exit 0 + linked; helpers 0/0/0.  Per the exe-lock
convention: not a code failure -- re-run the gate once Jeff closes the app,
judge by error grep + FOUR link lines (stale-object caveat).

Gate re-run (after Jeff closed the exe): GREEN -- five exit codes 0, all
FOUR link lines (Release + Debug BaySickDAW.exe + both plugin hosts), zero
error C / LNK / MSB hits.  The regression-fix pass is code-complete and
compiled; runtime verification handed to Jeff (five-point list: param rows +
Ctrl+Z revert from any focus incl. mixer + Rusty ARIA knobs; Space from
inside a page window; Rusty pick undo round-trip; record-take undo).

## 2026-08-06 — Master captures rerouted to the Exports flow (Jeff's call)

Jeff's design fix for the record-take residue: master captures should land
in the browser's EXPORTS folder like every other export, and enter the
project only through the user's drag / "Add to Project..." gesture -- whose
route prompt (existing pages + "a new Clip/Vox/Inst Page") already owns
channel creation.  Flow CONFIRMED in code before building: Exports rows and
grid drops both funnel through BrowserPanel::beginAddRenderToProject (single
path, register-then-create-then-route, library entry upgraded not
duplicated).

CHANGE: commitRecordingResult's master-capture branch (routeChannel 0, no
strips armed) no longer calls dropWavAsClip -- the WAV MOVES from
<project>/Samples/ to getProjectExportsDir() (dupe-suffixed if a name
collides) and the browser's Exports list refreshes immediately via new
public BrowserPanel::refreshRenderRows() (the Task 4 refresh-hook pattern;
rebuildRenderRows stays private).  Move-failure falls back to the old
auto-drop rather than losing the take.  CONSEQUENCES: (1) the orphan-Audio-
strip-on-undo residue is GONE -- nothing project-side exists to undo until
the user routes the file, and THAT step already has its own undo; (2) the
"Record Take" transaction now covers strip-armed takes + captured MIDI only
(master captures add nothing at commit, so no phantom row); (3) KNOWN
BEHAVIOR: a master capture recorded with count-in keeps the count-in bar at
the head of the exported WAV -- the old grid auto-drop hid it via
contentStartSamples, the export flow places files verbatim (slip-edit trims
it after placement).  Surfaced to Jeff with the hand-back.

Gate: PENDING -- waiting on Jeff to be out of the exe (he may be mid-check
of the previous build).

Gate (master-captures-to-Exports change): GREEN -- five exit codes 0, four
link lines, zero error hits.  Full regression-fix set + the Exports reroute
now built in both configs; runtime verification with Jeff.

## 2026-08-06 — Jeff's second runtime check: redo across a resurrection was a silent no-op.  Tag-resolved undo targets (the detached-tree caveat RETIRED)

Jeff: knob edits now show and rewind, but after undoing past the Rusty
player load and redoing forward, "the player will come back but the edits
listed on the undo history don't actually change anything."  Exact match
for the knowingly-carried detached-tree caveat: param undo entries were
JUCE SetPropertyActions bound to the SPECIFIC child-tree objects of the
engine instance alive at edit time; resurrection/program switches/kit
loads create a NEW engine + NEW trees, so redo fired into detached corpses.
Under every-action undo that caveat is not acceptable -- round-tripping is
the core promise -- so it is now FIXED, not carried:

- **Vendored (same APVTS patch file):** the flush no longer performs
  tree-bound SetPropertyActions.  New `ApvtsParamValueUndoAction` stores
  (ownerTag, paramId, oldValue, newValue) and re-resolves the LIVE APVTS
  by tag at apply time -- the same model-addressed-at-apply convention the
  automation applicators follow.  Apply writes the PARAMETER under
  ScopedProgrammaticParamWrites (engine hears it immediately; the next
  flush copies to the tree WITHOUT the manager -- nothing performs during
  an undo).  Coalescing preserved (per-drag flush actions merge keeping the
  first oldValue, mirroring SetPropertyAction).  The tree itself is now
  ALWAYS written untransacted.  Fallback for untagged instances: the
  construction-time pointer, liveness-checked.
- **New APVTS identity surface:** public `String undoOwnerTag` + static
  `findByUndoOwnerTag()` over the existing liveness registry.
  `StateSwapAction` (ruling-3a preset loads) upgraded to the same tag
  resolution, so preset-load rows also survive engine swaps; its type
  guard stays.
- **App-side tags (all 11 APVTS constructions):** main = "main" (processor
  ctor); rig engines = "rig:<kind>:<pageIndex>" at the EngineRig creation
  chokepoint (Layers/Bass/Drums/Clips engine types, Vox + its embedded
  NAM/IR ".namir", Inst chain ".pedals"/".namir"; hosted Plugins have no
  APVTS); sfizz trio in their ctors -- "sfizz:<prefix>" (Guitars/Basses,
  per-instIdx) and "rusty" (singleton).  Tags ride the pageIndex-keyed
  resurrection spine, so a re-created engine answers to its old identity.
- **Degrade semantics:** owner absent at apply (tab still deleted) = inert
  success (a false return would wipe the manager's whole history); an
  engine-KIND swap at the same tag no-ops via getParameter miss (param ids
  differ) or the StateSwap type guard.  The history ORDER makes the normal
  round trip correct: redo re-creates the engine (structural row) BEFORE
  the param rows that follow it.

CONSEQUENCE FOR THE RECORD: the Task 2 "detached-tree inert no-ops"
knowingly-carried caveat is SUPERSEDED -- param and preset rows now survive
engine death/re-creation.  The residual inert case is only "owner genuinely
absent at apply time," which the history ordering prevents in normal
undo/redo walks.

Gate: launched; result appended below.

Gate (tag-resolved undo targets): GREEN after one exe-locked Release link
(Jeff was in the app; Debug had already compiled + linked the fix clean).
Re-run: five exit codes 0, four link lines, zero error hits.  Both configs
now carry the full set: regression fixes + Exports reroute + tag-resolved
redo-across-resurrection.

## 2026-08-06 — Jeff's third runtime check: "Ctrl+Z undoes 2 items at a time."  Gesture-merge fix (flush-pinned transaction boundaries)

Redo round-trip confirmed WORKING by Jeff (tag resolution holds).  New
report: one Ctrl+Z consumes two history items.  Dispatch exhaustively
traced CLEAN first (KeyPressMappingSet single-invoke-and-consume verified
in the vendored source; command flags uniformly zero -- no key-up
invocation; one mapping-set registration per window, hostPageInWindow
early-returns on a framed entry; EventEditor doUndo/doRedo symmetric
if/else; grids migrated; UndoManager::undo()/redo() verified drop-free and
symmetric; the (1,N) reaper fires on perform only).  The REAL mechanical
bug: TRANSACTION CONTENT WAS BOUND TO THE FLUSH TIMER, NOT THE GESTURE.
beginNewTransaction only sets a flag; the set materializes at the first
perform -- which comes from the 10-50 Hz flush (decaying to 500 ms idle).
Two quick knob gestures inside one flush window = the first gesture's late
flush lands INSIDE the second gesture's set: ONE merged row (named after
the second knob) holding BOTH edits; one Ctrl+Z reverts both.

FIX -- pin every transaction boundary: new vendored static
`AudioProcessorValueTreeState::flushAllLiveInstancesToValueTrees()` (walks
the liveness registry; message-thread, human-gesture rate) called FIRST at
every boundary site: the vendored `ParameterAttachment::beginGesture`, both
`UndoBracket::beginParamUndoGesture` overloads, `doUndoAction`, and
`globalUndo`/`globalRedo` (the undo-side call also makes "undo right after
a tweak" undo THAT tweak instead of leaking it past the boundary into the
post-undo auto-begin).  Pending edits are thereby filed into THEIR
transaction before any boundary moves.

HARDENING (same pass, same class): `updateParameterConnectionsToChildTrees`,
and BOTH branches of `replaceStateKeepingUndoHistory`, now run under
`ScopedProgrammaticParamWrites` -- a rebind's setNewState value pushes are
programmatic by definition and must never mark as user writes (an unmarked
rebind could mint undo entries into whatever transaction was open).

Gate: launched; result appended below.

Gate (gesture-merge fix): GREEN after one exe-locked Release link -- five
exit codes 0, four link lines, zero error hits.  Both configs current with
the full accumulated set.  Jeff's discriminating test outstanding: slow
knob pair = two rows undone one at a time; fast knob pair = still two rows
(the previously-merging case); if two ROWS per press persists, get the row
labels (would indicate a second, distinct mechanism).

## 2026-08-06 — Double-undo persists after the gesture-merge fix.  DIAGNOSTIC INSTRUMENTATION installed

Jeff: one Ctrl+Z still consumes TWO history entries (his symptom was never
speed-dependent; my merge fix was a real bug but NOT his bug -- owned in
chat).  Static tracing is exhausted: dispatch single-fire (mapping set
verified single-invoke-and-consume; win32 doKeyDown defers to the pending
WM_CHAR so Ctrl+Z dispatches ONCE -- stock suppression intact in the
vendored peer), UndoManager::undo() single-pop, all handler paths
symmetric.  Per the diagnose-before-fixing rule: TEMP INSTRUMENTATION in
instead of a fourth blind fix.

DIAGNOSTIC INSTRUMENTATION CATALOG (strip when the culprit is found):
- `undoDiagLog()` (StandaloneEditor.cpp, file-local) appends to
  Documents/BaySickDAW/undo_diag.txt with ms timestamps.
- `globalUndo`/`globalRedo` gain a `diagSource` tag param; all call sites
  tagged: "cmd" (keybind via ApplicationCommandManager), "menu" (Edit menu
  201/202), "histwin" (history-window button/click), "ctx" (UndoContext
  consumers incl. EventEditor + grids).  Each invocation logs source +
  depth before -> after-flush -> after-undo + the top transaction name.
- `rebuildHistoryLabels` logs u/r sizes + top name on every rebuild (the
  display-model side, in case the manager moves one but the display moves
  two).

One reproduction (tweak knobs, ONE Ctrl+Z) pins: two invocations (and from
where) vs one invocation moving depth by two vs display-only.  Jeff runs
Debug, reproduces once, I read the file.

## 2026-08-06 — DOUBLE-UNDO ROOT CAUSE CAUGHT BY THE TRACER + FIXED (vendored win32 key forward)

undo_diag.txt from Jeff's one-press reproduction: TWO "UNDO src=cmd"
invocations per physical press, ~12 ms apart, each moving depth by exactly
one (6->5 then 5->4, etc.).  Mechanism confirmed in the vendored peer
(juce_Windowing_windows.cpp): pages live in WS_CHILD peers; a Ctrl+letter
press hits the child twice (WM_KEYDOWN + translated WM_CHAR).  doKeyDown
defers to the pending WM_CHAR and returns UNUSED -> the wndProc's
`forwardMessageToParent(WM_KEYDOWN)` posts the raw keydown to the PARENT
(the main frame, ~12 ms of queue latency); the parent's doKeyDown finds no
pending WM_CHAR on ITS hwnd and SYNTHESIZES the keypress -> second command
dispatch.  Meanwhile the child's WM_CHAR path had already fired the first.
The Ctrl+Alt redo asymmetry: WM_SYSKEYDOWN/WM_SYSCHAR -- the sys variants'
WM_SYSCHAR is outside doKeyDown's WM_CHAR..WM_DEADCHAR peek range, so the
child self-generates + consumes (used=true) and the wndProc never forwards
-> redo always fired ONCE.  Invisible pre-batch: with no first command
target, BOTH dispatches were no-ops.

FIX (vendored, juce_Windowing_windows.cpp -- the FOURTH patched vendored
file): `forwardMessageToParent` now skips the forward when the parent HWND
is a JUCE peer in this process (`getOwnerOfWindow(parentH) != nullptr`) --
the forward exists for plugin-in-a-foreign-host embedding, where the
parent never saw the key; in-process parents already had the key offered
through the child peer's own dispatch chain.  Covers WM_KEYDOWN/WM_KEYUP/
WM_CHAR forwards uniformly.

Tracer STAYS IN for Jeff's confirmation run (the log will show exactly one
UNDO per press); strip it + this catalog entry after his confirm.
Vendored-patch carry-note count: FOUR files now (APVTS .h/.cpp,
ParameterAttachments.cpp, Windowing_windows.cpp).

## 2026-08-06 — Double-undo CONFIRMED FIXED by Jeff; tracer stripped

Jeff's confirmation run (in undo_diag.txt before removal): five presses,
each exactly ONE "UNDO src=cmd" line, depth stepping one per press at
human intervals -- then "it works" in chat.  DIAGNOSTIC INSTRUMENTATION
STRIPPED per the catalog entry: undoDiagLog + both diagSource params +
all four call-site tags + the rebuild log removed (grep-verified zero
references); Documents/BaySickDAW/undo_diag.txt deleted.  The
flushAllLiveInstancesToValueTrees calls in globalUndo/globalRedo STAY --
they are the gesture-merge fix, not diagnostics.  Final clean-build gate
pending (Jeff in the app).

## 2026-08-06 — HELD-ENTRY ADDENDUM (fold into the held Work Log entry at apply): the post-review RUNTIME pass

The held entry above was drafted at static code-complete.  Jeff's runtime
checks then found the batch's headline feature broken; everything below
shipped the same day and is part of the ONE batch commit.  At apply time,
fold this addendum into the held entry (Done + Found + the revised
carry-forward notes) rather than treating the held draft as complete.

### Runtime regressions found by Jeff + fixes (all confirmed or pending walk)

1. **Param undo dead app-wide** -- two breaks: (A) lazily-registered params
   never bound an adapter tree (flush filed into a void; empty gesture
   transactions leave no rows) -> vendored addParameterAdapter now binds the
   child at registration with the live value pre-set; (B) all four ARIA
   widget classes + VibePlayer detune built attachments with a defaulted-
   null UndoManager -> manager passed.  CONFIRMED working by Jeff.
2. **Every global key bind dead with focus in any contained window** --
   command-target resolution walked up from focus and found nothing in
   parentless WorkspaceWindows -> mCmdMgr.setFirstCommandTarget(editor)
   pinned (dtor un-pins).  DrumKitGrid's dead-ctx-swallowing local Ctrl+Z
   handlers deleted (B-5 migration completed).  CONFIRMED (keys work; led
   to finding 4).
3. **Redo across a resurrection silently no-oped** -- param/preset undo
   entries were bound to dead engine objects -> tag-resolved undo targets
   (undoOwnerTag + ApvtsParamValueUndoAction + StateSwapAction tag
   resolution; tags stamped at all 11 APVTS constructions on the
   pageIndex-keyed identity spine).  SUPERSEDES the Task 2 detached-tree
   knowingly-carried caveat.  CONFIRMED working by Jeff.
4. **One Ctrl+Z consumed two history entries** -- vendored win32
   forwardMessageToParent posted the raw WM_KEYDOWN to the parent peer
   after the child's WM_CHAR path had already dispatched (two command
   invocations ~12 ms apart, tracer-proven); Ctrl+Alt combos ride the
   never-forwarded SYS messages, which is why redo was single.  Forward now
   skipped when the parent HWND is an in-process JUCE peer (kept for the
   real plugin-in-host case).  CONFIRMED by tracer log + Jeff ("it works").
   Along the way: the gesture-merge fix (flush-pinned transaction
   boundaries at beginGesture/UndoBracket/doUndoAction/globalUndo/Redo) --
   a real merge bug, fixed, though it was not Jeff's symptom.
5. **Rusty picks not undoable** -- three causes fixed: resurrect read the
   "library:" kit ref as a raw path (resolvePersistedRef now); the
   cross-program preset load escaped the wrap via its async confirm (wrap
   moved to the terminals; stale "This cannot be undone." dropped); first
   pick was exempt -> ruling 1A wraps it (unloadToNone + empty-record
   restore branch).  Pending Jeff's walk.
6. **Recording (Jeff's ruling: everything undoable)** -- a take commits as
   ONE "Record Take" transaction (pattern slice incl. captured MIDI +
   library rider); undo removes it from the project, WAVs stay on disk.
   Master captures (Jeff's design): land in the browser's EXPORTS list via
   getProjectExportsDir move -- project entry happens only through the
   standard route prompt (its own undoable step); the auto-drop's
   orphan-strip residue is structurally gone.  KNOWN: count-in head stays
   in an exported master WAV (slip-edit after placement; separate call if
   he wants it trimmed).  Pending walk.
7. **Layout** -- browser collapse = width-zero + chevroned draggable
   divider (ruling 2B threshold-open); Rusty Program/Player Preset on the
   window title strip + section tab row in the Aria band (T3's
   behind-the-chrome failure structurally impossible on the inside-the-bar
   mount); four stale placement-ruling comments rewritten;
   addHostedTrailingWidget machinery deleted (orphaned).  Pending walk.

### Corrections to the held draft

- "Two machineries" note: the reviewer-era "drum/master/bus faders will
  produce two rows per drag" claim was WRONG (different data domains; no
  control double-files) -- retracted in the notes; nothing to reconcile.
- Vendored-patch carry-note: FOUR files (juce_AudioProcessorValueTreeState
  .h/.cpp -- write phase, keep-history + StateSwapAction, liveness registry
  + tags + lazy bind + boundary flush; juce_ParameterAttachments.cpp --
  gesture naming + boundary flush; juce_Windowing_windows.cpp -- in-process
  key-forward suppression).  Any vendored-JUCE update must re-apply ALL
  FOUR or undo coverage silently degrades.
- Process note for the record: the batch was presented commit-ready on
  green builds + static review alone; the runtime pass (Jeff) then found
  the headline feature broken.  A runtime sanity check of the headline
  feature precedes any future hand-back regardless of bulk-run.

Final clean-build gate (tracer stripped): GREEN -- five exit codes 0, four
link lines, zero error hits.  §B.33 extended to 22 rows (B19 one-press-one-
entry from every window; B20 record-take undo; B21 master-capture Exports
routing; B22 tag-resolved cross-resurrection round-trips); held-entry row
references updated.  Close surface (ONE batch commit) goes to Jeff now.

## 2026-08-06 — Jeff's dispositions on the five parked calls (1-4 = FIX NOW, 5 = leave) + the window fill-size request

Rulings: items 1-4 fix now, item 5 (count-in head in exported master WAVs)
stays as is.  All four shipped:

1. **Dead `sendCc` deleted** from BaySickGuitarsProcessor + BaySickBasses-
   Processor (.h decl + .cpp def, both).  Rusty's sendCc KEPT -- it has a
   live caller (hi-hat pedal CC4 dispatch); only the Guitars/Basses copies
   were caller-less.
2. **Dead `InstPage::rebuildProgramCombo` / `onProgramComboChanged`
   declarations deleted** (never had definitions).
3. **Page-lifetime undo entries survive delete+resurrect cycles:** new
   `UndoContext::resolveOwnerPage` -- a zero-arg live-page resolver the
   editor binds per ctx from the OWNER KEY ("drm3" -> Drums page index 3;
   prefix map lay/bass/drm/vox/inst/plug, no prefix prefixes another).
   All five page gesture sites (DrumPage sound swap, LayersPage + BassPage
   + PluginsPage chain swaps, InstPage program pick) now resolve the LIVE
   page at apply time, with the old SafePointer kept only as the
   no-resolver fallback.  A row recorded before a delete+resurrect cycle
   now applies onto the resurrected page instead of silently skipping.
4. **Kit-load undo restores engine-less drum tabs:** the capture no longer
   skips empty-xml tabs -- they record with an EMPTY snapshot (File()) and
   respawn as EMPTY tabs (importPagePresetXml guards empty; owned-file list
   skips the null entries).

ALSO (Jeff, same session): the four Disk-persistent windows now remember
BOTH sizes across restarts.  A window saved while FILLED writes its
pre-fill restore rect alongside its bounds (session map + settings.xml
rx/ry/rw/rh); on open, `loadSavedFillState()` (end of attachTo) re-seeds
mFilled + mRestoreBounds so the fill toggle restores the pre-fill size
instead of "restoring" to the same full-workspace rect.  Un-filled saves
erase the record (manual drag/resize already drops mFilled, so it
self-heals).  Workspace-local space throughout, matching the bounds store.

Gate: launched; result appended below.

Gate (rulings 1-4 + dual window sizes): GREEN -- five exit codes 0, four
link lines, zero error hits.  Everything through Jeff's latest rulings is
built in both configs.  Close surface (ONE batch commit) re-presented.
