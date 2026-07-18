# Running Notes — QA-I (patient-veiling-tortoise)

> Append-only mid-batch log. New entry at every checkpoint (commit landed / sub-task
> verified / finding captured / spec call resolved / scope pivot). Consumed at doc-close
> (section pass under bulk-run R2) as the primary input for the held Work Log entry.

Pair file: `Plans & Specs/Batch Plans/patient-veiling-tortoise.md`
Convention: Main Plan §0 "Batch Plans + Running Notes layout" (exemplar
`federated-bouncing-cupcake.md`).

## 2026-07-17 — Group open (G3)

Seeded at G3 group approval. Greenfield overlay build; scout truths baked in the plan
(fully-synchronous message-thread load w/ three 30 ms sleeps -> overlay must pump paints;
black screen = window destroyed before teardown). Coding starts after QA-H.

## 2026-07-18 — Task 1 — Overlay component

New `Source/Standalone/HeavyOperationOverlay.h/.cpp` — reusable overlay for heavy
synchronous message-thread ops. Dimmed full-parent backdrop + centered panel (operation
title, step label, progress bar — determinate fill; indeterminate = pulsing sweep whose
phase advances per pump) + wait cursor (MouseCursor::showWaitCursor/hideWaitCursor, plus
WaitCursor on the component for hover). API: beginOp(title, indeterminate=false) /
setStep(1-based i, n, label) — bar = completed fraction (i-1)/n, n<=0 = indeterminate /
setStepLabel / endOp / isActive / pumpPaint. Core mechanism (the batch's locked
approach): every state change calls pumpPaint() = repaint() +
ComponentPeer::performAnyPendingRepaintsNow() — forces the pending WM_PAINT through the
peer synchronously so progress renders while the op holds the message thread (verified a
real synchronous paint in vendored JUCE 7.0.12: juce_Windowing_windows.cpp:4897 —
InvalidateRect of deferred repaints then PeekMessage WM_PAINT + handlePaintMessage).
Nesting legal (engine swap inside a project load): depth counter — an inner beginOp
shows as the outer op's step label, only the outermost endOp hides. Blocks input
inherently while visible (topmost hit-target swallows clicks); ops are synchronous so
nothing dispatches mid-op anyway. CMakeLists.txt: BaySickDAWStandalone target only
(Standalone-shell UI cluster). No call sites yet — wiring is Tasks 2-4. No findings, no
spec calls, no diagnostics added.

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-18.

## 2026-07-18 — Task 2 — Project load wiring (APP-03)

New `onLoadProgress` std::function hook on VibeSynthProcessor (`PluginProcessor.h`, next
to `onDeserializeUIState`; message-thread-only) — `deserializeProject` emits "Reading
project state..." (after the shield + 30 ms sleep) and "Restoring patterns..." (before
the PatternManager block). StandaloneEditor: new `mHeavyOpOverlay`, always-on-top child
added LAST in the ctor so it covers every page; ctor wires the hook -> `setStepLabel`,
dtor nulls it (processor outlives editor). `deserializeUIState` sets "Closing old
tabs..." before the inner `closeAllDynamicTabs`, counts `<Tab>` records upfront, then
per-tab determinate `setStep(i, N, "Tab i of N - <name-or-type>")` (ASCII " - ", not the
plan text's em-dash, per the ASCII-only UI-string rule). `restoreAudioStripsFromArrangement`:
"Rebuilding audio strips..." via `setStepLabel`, gated on `isActive()`. New RAII
`HeavyOperationOverlay::ScopedOp` (nested struct in the header) — `beginOp` in ctor /
`endOp` in dtor; every exit path (failed loads, early returns) drops the overlay, can
never stick. Four load entry points wrapped, each also setting "Closing old tabs..."
before its own pre-wipe `closeAllDynamicTabs`: Open Recent ("Loading Project..."), Open
Project browser `onOpenSelected` ("Loading Project..."), `doFileNewFromTemplate` inner
lambda ("Creating Project..." + "Copying template files..." over the template Samples/
copy), Restore-from-Backup modal callback ("Restoring Backup...").

PREMISE CORRECTION (assumptions-changed, carry-over material): the plan's "startup
restore" flow does not exist — app startup is blank + default tabs (QA-Ef #6, editor
ctor `addDefaultDynamicTabs`), no project auto-open. The real fourth load path is
Restore-from-Backup, confirmed by `restoreAudioStripsFromArrangement`'s own comment
naming all four load callers; wired under the §5 lock ("wired to load" ops). No other
findings, no spec calls, no diagnostics added.

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-18.

## 2026-07-18 — Task 3 — Shutdown overlay + teardown reorder (APP-02)

`VibesynthStandaloneApp::shutdown()` (StandaloneApp.cpp) reworked. Old order: WindowState
save -> `mWindow = nullptr` FIRST (window teardown destroyed the editor content — the
closeAllDynamicTabs + engine-destruction grind — behind a bare black DocumentWindow) ->
device/processor teardown after. New order: WindowState save (untouched, still first,
reads real placement) -> shutdown overlay created + parented on the WINDOW ->
beginOp("Shutting Down...", indeterminate) + "Closing tabs and engines..." ->
`mWindow->clearContentComponent()` runs the heavy editor/engine teardown under the
overlay -> "Releasing audio device..." -> unchanged device/MIDI/processor teardown
sequence -> endOp + overlay detached -> `mWindow = nullptr` LAST. Black-window period
eliminated. Overlay is owned by shutdown() itself (local unique_ptr), NOT the editor's
member — that one dies with the content. Works after the message loop has stopped: the
overlay's peer paint-pump renders directly, no dispatch loop needed. QA-Eb
maximize-restore guard honored (WindowState save block untouched and still ahead of
everything; initialise's restore order untouched). VibeSynthWindow black background left
as-is — the fix is the ORDER (window never sits bare), not the color.

IN-BATCH BUILD FIX (first attempt failed Debug-only, Release passed):
`mWindow->addChildComponent(*overlay)` hit ResizableWindow's JUCE_DEBUG-only misuse trap
(juce_ResizableWindow.h:376-391 hides the base overloads to catch children added instead
of setContentOwned; its own doc block says make a base-class call when a direct child is
genuinely intended). Fix: `mWindow->Component::addChildComponent(...)` + framework-quirk
comment. Trap compiles only under JUCE_DEBUG — hence Release passed while Debug errored
C2664. No findings, no spec calls, no diagnostics added.

Build-confirm: first run Debug FAIL (C2664, above); fix applied; second run Jeff "clean"
(Release+Debug), 2026-07-18.

## 2026-07-18 — Task 4 — Engine swap + heavy load busy overlays (NAV-02)

`HeavyOperationOverlay::ScopedOp` gained a null-tolerant POINTER ctor overload (null
overlay -> whole op is a no-op); the Task-2 reference ctor stays. New static
`StandaloneEditor::busyOverlayFor(juce::Component*)` — findParentComponentOfClass
walk-up returning the editor's overlay, null while the component is unparented — so
pages need zero wiring, and project-restore selectEngine calls (which run BEFORE
addChildComponent) resolve null and no-op cleanly under the already-active load overlay.
selectEngine wrapped in LayersPage (:139 area, after the mEngineLocked guard), BassPage
(same), and DrumPage (after the same-engine no-op guard) — title "Loading
<engineName>...", indeterminate, per the plan's busy-indeterminate lock. DrumPage
sound-picker load funnel wrapped x5: loadSampleFile ("Loading Sample..."),
loadSampleFolder ("Loading Samples..."), loadSampleSFZ ("Loading SFZ..."),
loadSynthPreset + loadPlayerPreset ("Loading Preset...") — these nest over their
internal selectEngine (inner title becomes the outer op's step label, by design).
BaySickRustyDrumsPage::loadKit wrapped ("Loading Kit...") — the single funnel covering
user program switches AND reloadForProjectRestore. StandaloneEditor::loadKitImpl wrapped
("Loading Kit..." + setStepLabel "Building drum tabs..."), placed AFTER the three
validation early-returns so failed validation never flashes the overlay; bonus coverage:
legacy loadTemplate calls loadKitImpl, so template loads get the busy sign over their
heaviest chunk for free. addBaySickGuitarsTab + addBaySickBassesTab wrapped ("Loading
Instrument...", before the default-SFZ kit load blocks) — the sfizz-class default kit
loads (Black&Green Guitars / Black&Blue Basses keyswitch SFZs). New includes:
StandaloneEditor.h into LayersPage.cpp / BassPage.cpp / DrumPage.cpp /
BaySickRustyDrumsPage.cpp (cpp-only, no cycles — the editor header only forward-declares
those page classes or includes their headers with guards). Scope note: engine-EDITOR
internal preset menus (e.g. VibePlayerEditor's own preset list) NOT wrapped —
engine-source dirs are outside this batch's files list; a gap surfacing in the campaign
routes per Rule 3. No findings, no spec calls, no diagnostics added.

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-18.

## 2026-07-18 — QA-I CODE-COMPLETE

Tasks 1-4 shipped, every gate build-confirmed clean (one Debug-only C2664 fixed at the
Task 3 gate). §B.15 authored (7 scenarios; `blocks:` hash backfills at the next docs
commit per the B.12/B.13/B.14 precedent). Work Log entry drafted + HELD below. ONE
batch commit surfaced for approval (carries the two QA-H doc stragglers per plan).

## Held Work Log entry

> HELD per bulk-run R2 — apply to `Implemented Work Log.md` verbatim when §B.15 passes.

### 2026-07-18 09:30 PT — QA-I — Heavy operation progress overlay + project-load modal + shutdown teardown overlay + engine-swap/kit-load busy signs

**Bucket:** Players, System Pages, UI / L&F / Theming, Cross-cutting Infrastructure

#### Done

- **Task 1 — overlay component (paint-pump mechanism):** new
  `Source/Standalone/HeavyOperationOverlay.h/.cpp` — reusable overlay for heavy
  synchronous message-thread ops: dimmed full-parent backdrop + centered panel
  (operation title, step label, progress bar — determinate fill; indeterminate =
  pulsing sweep whose phase advances per pump) + wait cursor. API: beginOp /
  setStep(i, n, label) (bar = (i-1)/n; n<=0 = indeterminate) / setStepLabel / endOp /
  isActive / pumpPaint. **Core mechanism (the batch's locked approach):** every state
  change calls pumpPaint() = repaint() +
  `ComponentPeer::performAnyPendingRepaintsNow()` — forces the pending WM_PAINT
  through the peer synchronously so progress renders while the op holds the message
  thread (verified a real synchronous paint in vendored JUCE 7.0.12). Nesting legal
  via depth counter (an inner beginOp shows as the outer op's step label; only the
  outermost endOp hides); visible overlay swallows input. Standalone CMake target
  only.
- **Task 2 — project-load wiring (APP-03; STATE-03/04 fold in as this visual
  layer):** new `onLoadProgress` hook on VibeSynthProcessor (message-thread-only;
  editor ctor wires it -> setStepLabel, dtor nulls it) — `deserializeProject` emits
  "Reading project state..." / "Restoring patterns...". StandaloneEditor's
  `mHeavyOpOverlay` added LAST as an always-on-top child so it covers every page.
  `deserializeUIState` sets "Closing old tabs..." before the inner
  closeAllDynamicTabs, counts `<Tab>` records upfront, then steps per-tab determinate
  "Tab i of N - <name-or-type>" (ASCII " - " per the ASCII-only UI-string rule);
  `restoreAudioStripsFromArrangement` labels "Rebuilding audio strips..." gated on
  isActive(). New RAII `HeavyOperationOverlay::ScopedOp` — beginOp in ctor / endOp in
  dtor, so every exit path (failed loads, early returns) drops the overlay; it can
  never stick. All four load entry points wrapped, each also setting "Closing old
  tabs..." before its own pre-wipe: Open Recent + Open Project browser ("Loading
  Project..."), doFileNewFromTemplate ("Creating Project..." + "Copying template
  files..." over the template Samples copy), Restore-from-Backup ("Restoring
  Backup..."). **Premise correction folded in:** the plan's "startup restore" flow
  does not exist — startup is blank + default tabs (QA-Ef #6), no project auto-open;
  Restore-from-Backup is the real fourth load path (confirmed by
  restoreAudioStripsFromArrangement's own four-caller comment) and was wired instead
  under the §5 "wired to load" lock.
- **Task 3 — shutdown overlay + teardown reorder (APP-02):**
  `VibesynthStandaloneApp::shutdown()` reordered — WindowState save stays first
  (QA-Eb maximize-restore guard honored; initialise's restore order untouched), then
  a shutdown overlay owned by shutdown() itself (local unique_ptr — the editor's
  member dies with the content) is parented on the WINDOW, beginOp("Shutting
  Down...", indeterminate) + "Closing tabs and engines...",
  `mWindow->clearContentComponent()` runs the heavy editor/engine teardown under the
  overlay, "Releasing audio device..." ahead of the unchanged device/MIDI/processor
  teardown sequence, endOp, and `mWindow = nullptr` LAST. Black-window period
  eliminated by ORDER (window background left as-is); works after the message loop
  has stopped — the peer paint-pump renders directly. **In-batch build fix:**
  ResizableWindow's JUCE_DEBUG-only misuse trap hides the addChildComponent base
  overloads, so the first attempt failed Debug-only (C2664; Release passed) — fixed
  with an explicit `Component::` base-class call + framework-quirk comment.
- **Task 4 — engine-swap + heavy-load busy wraps (NAV-02):** ScopedOp gains a
  null-tolerant POINTER ctor overload (null overlay -> whole op no-ops; the Task-2
  reference ctor stays); new static `StandaloneEditor::busyOverlayFor(Component*)`
  walk-up (null while unparented) means pages need zero wiring and project-restore
  selectEngine calls — which run before addChildComponent — resolve null and no-op
  cleanly under the already-active load overlay. Wrapped: selectEngine x3
  (LayersPage / BassPage / DrumPage — "Loading <engineName>...", indeterminate per
  the busy-indeterminate lock); DrumPage sound-picker load funnel x5 (loadSampleFile
  "Loading Sample..." / loadSampleFolder "Loading Samples..." / loadSampleSFZ
  "Loading SFZ..." / loadSynthPreset + loadPlayerPreset "Loading Preset..." — these
  nest over their internal selectEngine, inner title becoming the outer op's step
  label by design); BaySickRustyDrumsPage::loadKit ("Loading Kit..." — the single
  funnel covering user program switches AND reloadForProjectRestore);
  StandaloneEditor::loadKitImpl ("Loading Kit..." + "Building drum tabs...", placed
  AFTER the three validation early-returns so failed validation never flashes the
  overlay; bonus — legacy loadTemplate routes through loadKitImpl, so template loads
  get the busy sign over their heaviest chunk for free); addBaySickGuitarsTab +
  addBaySickBassesTab ("Loading Instrument..." over the sfizz-class default-SFZ kit
  loads). New cpp-only StandaloneEditor.h includes into the four page cpps (no
  cycles). Engine-EDITOR internal preset menus deliberately NOT wrapped —
  engine-source dirs are outside this batch's files list; a gap surfacing in the
  campaign routes per Rule 3.

#### Found along the way (both handled in-batch; none deferred)

The plan's "startup restore" load path does not exist — app startup is blank +
default tabs (QA-Ef #6), no project auto-open; the real fourth load entry is
Restore-from-Backup, wired in Task 2 (premise correction, carry-over material).
ResizableWindow's JUCE_DEBUG-only addChildComponent trap (base overloads hidden to
catch children added instead of setContentOwned) broke the Debug config only —
C2664 at the Task 3 gate, Release passed — fixed with the explicit `Component::`
base-class call. Nothing else surfaced.

#### Known seams (campaign-visible)

Indeterminate ops advance the pulsing sweep only at step boundaries — mid-grind the
bar sits static (ops stay synchronous on the message thread by design);
engine-editor INTERNAL preset menus carry no busy sign (outside the batch files
list — a campaign flag routes per Rule 3); very fast engine picks may only flash
the overlay.

**Verification:** bulk-run R2 — campaign section §B.15 (7 scenarios).
Build-confirmed clean (Release+Debug) at every task gate, 2026-07-18 (one
Debug-only C2664 fixed at the Task 3 gate).
