# Running Notes — QA-L (tidy-unsticking-magpie)

> Append-only mid-batch log. New entry at every checkpoint (commit landed / sub-task
> verified / finding captured / spec call resolved / scope pivot). Consumed at doc-close
> (section pass under bulk-run R2) as the primary input for the held Work Log entry.

Pair file: `Plans & Specs/Batch Plans/tidy-unsticking-magpie.md`
Convention: Main Plan §0 "Batch Plans + Running Notes layout" (exemplar
`federated-bouncing-cupcake.md`).

## 2026-07-17 — Group open (G3)

Seeded at G3 group approval.

## 2026-07-18 — Task 1 — UI-01 right-click menu activation + UI-02 lane names

UI-01: VENDORED-JUCE EDIT (ledger note): juce_PopupMenu.cpp checkButtonState
(:1492-1500 post-edit) — `isDown` now tracks isLeftButtonDown() only (was
isAnyMouseButtonDown x2), so the press/release transition that drives
triggerCurrentlyHighlightedItem fires for the LEFT button only; right-release over an
open menu no longer activates items, anywhere in the app (one central fix).
Outside-click dismissal unaffected (modal input-attempt path owns it; verified the
:1507-1512 release path is trigger/dismiss-backstop only). Framework-workaround comment
per Rule 6. Right-press-drag-release item selection (the hold-right-and-drag pattern) no
longer selects — accepted consequence of the locked left-only spec; quick right-clicks
never triggered anyway (250 ms creation guard). UI-02: AutomationLane gains persisted
`lastKnownName` (PatternManager.h struct + XML round-trip "lastKnownName" property),
stamped in createAutomationBlock at creation while the target resolves live;
displayNameFor falls back to `lastKnownName + " (deleted)"` when the resolver returns
the anonymous "(deleted slot)" label; correctness argument: a slot UUID never revives
(C13 — effect swap = old-slot deletion + fresh UUID), so the creation-time name is
right for the lane's whole life. userDisplayName precedence + Batch-E stale-row flag
untouched. Both EventEditor raw-paramId fallbacks (browser rows :1118-area + title
:1382-area) route through new static readableParamIdFallback() — 32-hex UUID token ->
"(slot)", underscores -> spaces (the plan named one site; the second is the identical
class in the same file, folded in). Pre-existing lanes without the field keep today's
anonymous label (empty-field fallback; no migration per pre-v1). No spec calls, no
diagnostics added.

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-18.

## 2026-07-18 — Task 3 — NAV-01 grid/header alignment

Header/grid vertical sync event-driven on all three legs (was timer-only,
BuilderPage:7325 "belt-and-suspenders" tick — stale-until-tick desync): (1) scroll —
mGridViewport is now a ctor-local HeaderSyncViewport subclass whose visibleAreaChanged
override pushes setViewportYOffset immediately (also catches every re-clamp a resize or
zoom causes); (2) zoom — the existing mGrid->onRowHeightChanged hook pushes the offset
before its header repaint; (3) resize — BuilderPage::resized pushes after
mGridViewport->setBounds (covers no-clamp resizes). Timer call stays as backstop per
plan. setViewportYOffset already no-ops on unchanged values (repaint only on delta), so
the extra pushes are free. Plan's line refs (:5738/:6410) were stale post-QA-G;
re-located (TrackHeaderPanel::setViewportYOffset :6517, timer sync :7325). No spec
calls, no findings, no diagnostics added.

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-18.

## 2026-07-18 — Task 7 — Per-drum MIDI trigger notes (#10)

Param: mixer_drum_{N}_inputNote (Int -1..127, default -1 = unmapped) added in
addParamsForMixerStrip behind the _chokeGroup precedent, drum-gated
(prefix.startsWith("mixer_drum_")) so other inserts stay clean. Assignment UI:
DrumPage::showContextMenu gains a "MIDI Note" submenu next to Choke Group — id space
299 (Unassign) / 300+n; Unassigned + 11 octave submenus covering 0..127; labels via
juce::MidiMessage::getMidiNoteName(n, true, true, 5) — octaveForMiddleC=5 MATCHES the
roll's FL-style note/12 numbering (PianoRoll.cpp:262 + :2450, verified before picking);
submenu title shows the current assignment; write via the standard
setValueNotifyingHost path (choke handler mirror). Kit fan-out (audio thread): in the
liveMidi loop (PluginProcessor.cpp:2716-area), when kind==3 (any drum tab holds MIDI
focus) each isNoteOnOrOff event ALSO routes to every other drum whose assigned note
matches (di == idx skipped — the focused tab already receives the full stream, so an
assigned drum still plays normally when focused; unassigned drums get nothing extra;
note-offs follow the same route so fanned voices release). LOCK-FREE: new
mDrumInputNotePtr[16] (std::atomic<std::atomic<float>*>) published with a release
store at registerDrumEngine (message thread; the param exists as of the
ensureMixerStripParams call two lines up), nulled at unregisterDrumEngine,
acquire-loaded per event — no strings/allocs/map lookups on the audio thread (the
ClipCtl pre-resolved-atomics convention). CCs/pitchbend stay focused-tab only. No spec
calls, no diagnostics added.

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-18.

## 2026-07-18 — QA-L CODE-COMPLETE

Tasks 1-7 shipped (7 build gates, all clean; the Task-4 flagged interpretation
honored via the mid-task veto window — Jeff: "This is correct, proceed"). §B.18
authored (8 scenarios; `blocks:` hash backfills at the next docs commit per
precedent). Work Log entry drafted + HELD below. ONE batch commit surfaced for
approval (carries the QA-K doc straggler: the B.17 hash backfill). Vendored-JUCE
edit (juce_PopupMenu.cpp) logged for the license/vendor ledger per the plan's
routing note.

## Held Work Log entry

> HELD per bulk-run R2 — apply to `Implemented Work Log.md` verbatim when §B.18 passes.

### 2026-07-18 22:00 PT — QA-L — UI polish + navigation + per-drum MIDI notes (menu trigger fix, lane names, strip lifecycle, header sync, nav buttons, dup handling, roll accuracy, kit fan-out)

**Bucket:** UI / L&F / Theming, Mixer / Routing, System Pages, Players

#### Done

- **Task 1 — UI-01 + UI-02:** vendored-JUCE PopupMenu edit — the press/release
  transition that triggers the highlighted item keys on the LEFT button only (stock
  counted any button, so right-click over any open menu activated entries); one
  central fix, outside-click dismissal untouched (modal input-attempt path).
  AutomationLane gains persisted `lastKnownName` stamped at lane creation;
  deleted-slot lanes resolve to "<label> (deleted)" instead of the anonymous
  "(deleted slot)" (a slot UUID never revives, so the creation-time name stays
  correct); both EventEditor raw-paramId fallbacks format readable (UUID token ->
  "(slot)"); userDisplayName precedence + stale-row flag untouched.
- **Task 2 — MIX-05 + MIX-07:** the real MIX-05 cause fixed — Layer/Bass/Drum tab
  closes never removed their mixer strip (orphan overlap + blocked re-adds); new
  removeLayer/Bass/DrumChannel helpers mirror the Inst/Vox/Clips removers (widget +
  order entry only; InsertNode + APVTS persist) and onTabClosed calls them in the
  same tail block. ALL seven removers now fire the refresh callback
  (deleteSecondaryBus parity) so the Effects dropdown rebuilds on every close/delete
  path (wiring verified to rebuildChannelDropdown).
- **Task 3 — NAV-01:** header/grid sync event-driven on all three legs (viewport
  subclass visibleAreaChanged push; onRowHeightChanged zoom push; resized() push);
  the timer tick stays as backstop.
- **Task 4 — #18 + C-revised:** shared PageMenuBar "FX Rack" slot at the right end of
  the page-tab cluster, wired on all six page types (both Inst variants) via the
  mixer-FX three-line handoff (new jumpToFxRackForPrefix); the piano-roll pill row
  gains "Player Page" + "FX Rack" immediately right of the roll dropdown (new
  jumpToRollPlayerPage / jumpToRollFxRack; edge picks: DrumKit -> first Drums tab +
  Drums Bus rack, Rusty -> Rusty Bus rack — stated to Jeff, veto open). Flagged
  interpretation confirmed by Jeff mid-task before coding.
- **Task 5 — FILE-03:** rename's row->libIdx first-match collapse killed (row index
  IS the library index by construction); the rename block-stamp keyed (path, owner)
  like the delete cascade; single-point auto-numbering in
  PatternManager::addAudioToLibrary — colliding display names get "name (N)" aliases
  on the NEW entry (all add paths inherit; New-Page flow already distinct via
  duplicateSample).
- **Task 6 — LDT-394:** xToBeat pixel-center sampling; beatToX round-not-truncate;
  yToNote floor division — clicks land on the intended beat/row at deep zoom and at
  row/bar boundaries; resize hit zones + note rects move together (tolerance
  semantics unchanged).
- **Task 7 — #10 per-drum MIDI notes:** mixer_drum_{N}_inputNote (default unmapped) +
  right-click "MIDI Note" submenu (FL-style C5=60 labels matching the roll); live-MIDI
  kit fan-out — with any drum tab focused, notes fire every other drum whose
  assignment matches (simultaneous; note-offs release; focused tab keeps the full
  stream; CCs focused-only); lock-free pre-published param pointers on the audio
  thread.

#### Found along the way

renameAudioAt's block-stamp loop shared the FILE-03 collapse (path-only match
restamped other owners' clips) — fixed in-task with (path, owner) keying. A dropped
still-used local was caught in self-check before the Task-5 gate. Nothing routed out
of QA-L.

#### Known seams (campaign-visible)

Right-press-drag-release menu selection no longer selects (left-only trigger — the
250 ms guard already blocked quick right-clicks). Pre-existing lanes without
lastKnownName keep the old anonymous deleted-slot label until re-created. DrumKit /
Rusty roll nav-button edge targets are the stated picks pending Jeff's veto.

**Verification:** bulk-run R2 — campaign section §B.18 (8 scenarios).
Build-confirmed clean (Release+Debug) at all seven task gates, 2026-07-18.

## 2026-07-18 — Task 6 — LDT-394 piano-roll mouse accuracy

The three scouted offenders fixed in place (PianoRoll.cpp :505-513 post-QA-H numbering):
xToBeat samples the CENTER of the clicked pixel column ((x + 0.5) / mPPB — left-edge
sampling biased every click a half-pixel early, visible at deep zoom); beatToX
std::llround instead of C-cast truncation (truncation rounds toward zero for
off-view-left geometry — 1 px the wrong way); yToNote floor division via std::floor
(int division truncates toward zero — clicks in a partial top row mapped one row low,
the row-boundary complaint).  noteToY untouched (exact at row tops; yToNote(noteToY(n))
== n round-trip verified by inspection).  Edge-tolerance sanity re-check per plan:
resize hit zones (:581-591) compare mouse x against beatToX(edge) +/- kResizeZone and
note rects draw through the same beatToX — hit bands and visuals move together (<= half
pixel), tolerance semantics unchanged.  No spec calls, no findings, no diagnostics
added.

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-18.

## 2026-07-18 — Task 5 — FILE-03 duplicate-name handling

Collapse kill: BrowserPanel::renameAudioAt re-resolved row->libIdx by path FIRST-MATCH
(BuilderPage.cpp:1469-1474 pre-edit) — same-path entries under multiple owners collapsed
onto the first (wrong entry renamed).  mAudioPaths is populated in audioLibrary index
order (rebuildAudioRows :452-455 invariant, documented there), so idx IS the libIdx —
used directly now.  Same collapse in the rename's block-stamp loop (path-only match
restamped OTHER pages' clips): now keyed (path, owner) like the delete cascade.  Delete
flow already exact-index since QA-E Task 5 (:927) — verified, untouched.  SELF-CHECK
CATCH pre-gate: the first edit dropped the still-used `path` local (:1481 consumer) —
restored with the owner keying.  Auto-number: single point in
PatternManager::addAudioToLibrary (:196 append) — when the new entry's display name
(alias, else filename) collides with ANY existing entry's display name, the new entry's
alias auto-numbers "name (N)" (matches ensureUniqueBrowserName's convention; local
lambda, no UI dep in the data layer).  All add paths inherit (drops / + Add New Clip
re-picking an imported file — the live dup-name creator — / recordings, whose take
names are already unique = no-op).  New-Page flow unaffected (duplicateSample already
auto-numbers the physical file).  No spec calls, no diagnostics added.

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-18.

## 2026-07-18 — Task 4 — Navigation builds (#18 + C-revised)

FLAGGED-INTERPRETATION protocol honored: #18 half coded first, then the C-revised
half held for Jeff's veto window with the baked interpretation restated (two piano-roll
buttons); Jeff: "This is correct, proceed" — coded as written. #18: the page-tab button
row turned out to be ONE shared component (PageMenuBar in SharedUI — CLAUDE.md's
PageMenuBar.h layout entry is stale) so the build is single-point: new owned
setFxRackSlot(onClick) slot ("FX Rack", 58 px) laid out at the right end of the
tab-button cluster (after tabs/MID-SIDE/bank pill), cleared by clearTabSlots (the tab
cluster's lifecycle — every existing page-switch reset path clears it for free). Six
page-show branches wire it (Layers/Bass/Clips/Vox/Inst-both-variants/Drums), each
jumping via new StandaloneEditor::jumpToFxRackForPrefix (the mixer-FX three-line
handoff: mLastFXChannel = "mixer_{kind}_{idx}" -> selectTab(2) -> onTabSelected(2));
jump lambdas follow the QA-E Sub-Phase A capture discipline (prefix built before the
switch; no member/page access after — the switch frees the lambda's own capture
struct). C-revised: the piano-roll branch's setTabSlots grows from { pillLabel } to
{ pillLabel, "Player Page", "FX Rack" } — the pair sits in the same slot row
immediately right of the dropdown; dispatch on slot index (0 = existing dropdown menu).
New jumpToRollPlayerPage (resolves the active EngineId -> matching mPages entry by
kind+index -> selectTab(entry->ribbonTabId) + onTabSelected pair, the :2449 precedent)
+ jumpToRollFxRack (kind -> mixer prefix -> jumpToFxRackForPrefix). EDGE PICKS stated
to Jeff at the gate (veto open): DrumKit roll -> Player Page = first Drums tab, FX
Rack = "mixer_drums" (Drums Bus rack); BaySickRustyDrums -> "mixer_rustybus" (bus
rack; 13 per-strip racks ambiguous for a kit-level roll). Guitars/Basses rolls ->
"mixer_inst_{idx}". No diagnostics added.

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-18.

## 2026-07-18 — Task 2 — Mixer strip lifecycle (MIX-05) + Effects dropdown (MIX-07)

MIX-05 real cause (scout premise held): Layer/Bass/Drum tab closes never removed their
mixer strip — orphans overlapped the packed layout and the add-side count(idx) guard
blocked re-adds. New MixerPage::removeLayerChannel/removeBassChannel/removeDrumChannel
(keyed pageIndex/pageIndex/slot — the .cpp add-side is pageIndex-authoritative; the
header's stale "tabId" param names + the "Strips never get destroyed" doc block fixed in
the touched region per Rule 6), each mirroring removeInstChannel: strip map erase +
order-vector erase (mLayerTabOrder/mBassTabOrder/mDrumSlotOrder) + stripCacheDirty +
resized. InsertNode + APVTS params persist (house convention — re-add restores prior
settings). onTabClosed: three indices captured in the per-type blocks (next to the
QA-J' automation-map erases), removal calls in the tail with the Inst/Vox/Clips trio.
MIX-07: ALL seven removers (3 new + Aux/Inst/Vox/Clips) now fire onAudioStripRenamed —
deleteSecondaryBus parity — wiring verified: StandaloneEditor:3817 routes it to
EffectsPage::rebuildChannelDropdown, so the dropdown rebuilds on every close/delete
path. Mixer-strip audit checklist walked for the REMOVAL direction: routing
mActiveChannels untouched (channel stays registered, matches Vox/Inst precedent);
EffectsPage covered via the callback; no APVTS surface. No spec calls, no diagnostics
added.

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-18. Composition: BLU-378/379/492 OUT (QA-ApvtsAutomation, marathon
18); per-drum MIDI notes IN (#11=B, QA-Drum-Polish batch dropped; kit fan-out #10=a/a,
default unmapped); #17=c Clips picker gating dropped; #18 FX Rack button on all six
page-tab rows (both Inst variants); C = two piano-roll menu-bar buttons ("Player Page" +
"FX Rack") right of the roll dropdown. Scout premise corrections in the plan (MIX-05 real
cause = orphaned Layer/Bass/Drum strips on page close; MIX-07 same asymmetry; UI-01 =
vendored-JUCE any-button trigger). Coding starts after QA-K.
