# Running Notes — QA-Ee (rhythmic-counting-octopus)

> **Purpose.** Append-only running log for QA-Ee (96 PPQ Universal Timebase + Decoupled Snap
> Params). A new dated entry is appended at **every checkpoint** — commit landed / sub-task
> verified / finding captured / spec call resolved / scope pivot (Main Plan §0 +
> `feedback_draft_doc_running_notes_every_checkpoint.md`). At batch close, `/draft-doc batch-close`
> reads this file as the primary input for the single Implemented Work Log entry. Never edit prior
> entries; later surprises get their own new entry.

> **Pair file:** `Plans & Specs/Batch Plans/rhythmic-counting-octopus.md` (the QA-Ee plan).
> **Conventions:** Main Plan §0 (Document Formatting Conventions + the Batch Plans / Running Notes
> required-sections rule, locked 2026-05-11; exemplar `federated-bouncing-cupcake.md`).

## Diagnostic Instrumentation Catalog

_(Main Plan §0 Rule 4. Row format: Site | Tag | Purpose | Disposition. Append a row in the SAME
edit pass as any `DBG` / `Logger` / temp-`jassert` / debug-`AlertWindow` / temp-file diagnostic
added during QA-Ee. At task/batch close, strip every `Remove` row after surfacing the strip list to
Jeff. NOTE: QA-ClipDrop's armed Task-1 trap is **not** catalogued here — it belongs to QA-ClipDrop.)_

_None yet._

## 2026-06-03 — Task 0 — open

- Batch opened. Plan approved (`rhythmic-counting-octopus`). Structure = staged commits (Jeff
  SC-A): Stage 1 strictly the block data-model migration verified to load + play before any UI;
  Stages 2-4 = Builder / PianoRoll / Record snap.
- Spec calls locked by Jeff 2026-06-02: **SC-A** staged commits, **SC-B** global PianoRoll snap
  (drop per-roll `snapDenominator`), **SC-C** automation out of scope (curve points stay 0..1
  fractions, `lfoRate` stays float; only the automation clip's `ArrangementBlock` migrates).
  SC-3 (bridge) / SC-5 (decoupled) / SC-ii (drop Events+Line) / SC-4 (triplet lines identical) +
  the 10-label scheme were locked in §5. Defaults preserve current behavior (SC-def).
- Research findings folded in: automation curve points already store as clip-length fractions
  (`ControlPoint.timeTicks`, PatternManager.h:10) — no migration needed; PianoRoll already has a
  snap control (`mMagnetBtn` + per-roll `snapDenominator`) going global.
- Mirrored the approved plan to the reserved canonical name (Main Plan.md:4412); deleted the
  transient home-dir copy. Added the `**Plan file:**` pointer to the Main Plan §5 QA-Ee entry.
- **Baseline refreshed** against current main `1e53a2d` (QA-ClipDrop Tasks 0-3, batch held open for
  an intermittent saved-project copy-failure). QA-Ee core surfaces (ArrangementBlock struct, XML
  serdes, PianoNote, PianoRoll) confirmed **untouched** by QA-ClipDrop -> tick design holds. Folded
  into the plan: the new clip-routing model (clips route by `routeChannel` / owning Clips strip,
  `trackRow` is visual-only) + QA-ClipDrop's coexisting load fixup (`routeChannel==0 ->
  audioInsert(trackRow)`, PluginProcessor.cpp:2314) which composes cleanly with QA-Ee's
  `startBeats x 96 -> startTicks` XML migration. QA-ClipDrop's armed trap is theirs (Keep).
- **Finding (resolved in Task 0):** the §5 QA-Ee "Sequencing" line read the stale "after QA-Ed,
  before QA-Eb" (the §6 arrow + the inserted QA-ClipDrop / QA-TempoMap rows are authoritative;
  actual slot is after QA-ClipDrop, before QA-TempoMap). Jeff approved the one-line coherence fix;
  corrected in the Task 0 commit (kept the SC-i = (b) provenance + noted the original slot).

## 2026-06-03 — Task 1 — Stage 1 source landed (pre-build/verify)

Block data-model migration source complete; awaiting Jeff's Debug-then-Release build + verify
before commit. No diagnostic instrumentation added (Catalog stays empty).

- **Foundation:** `kTicksPerBeat = 96` in `VibesynthConstants.h`; `beatsToTicks` / `ticksToBeats`
  converters in `PatternManager.h` (need `juce::int64`, so not in the JUCE-free constants header).
- **`ArrangementBlock`:** `float startBeats` / `lengthBeats` -> `juce::int64 startTicks` /
  `lengthTicks`, authoritative.  Sentinels `kStartTicksUnset` (INT64_MIN) / `kLengthTicksUnset`
  (-1).  `effectiveStartBeats` / `effectiveLengthBeats` rewritten to derive beats from ticks (so
  every existing reader -- audio snapshot, UI render, hit-test, slip-edit capture -- is unchanged).
  Bridge setters `setStartBeats(double)` / `setLengthBeats(double)` added.
- **Serdes (`PatternManager.cpp`):** save writes `startTicks` (only when set) + `lengthTicks`
  (always); load prefers the tick props, else migrates legacy float `startBeats` / `lengthBeats`
  (`beats x 96 -> ticks`), resolving the old -1e6 / -1 sentinels.  New format is tick-only
  (downgrade unsupported).
- **Writer swaps -> setters:** `BuilderPage.cpp` x7 (import x2 / resize / move-clear-to-unset /
  left-slip x2 / right-slip) + `StandaloneEditor.cpp` x1 (`commitRecordingResult` Option-Y).
  `PluginProcessor.cpp` = audit-only (snapshot reads the `effective*` getters, unchanged) + 2
  stale comments updated to the tick field names.
- **Audit confirmed:** zero `.startBeats` / `.lengthBeats` field accesses remain tree-wide (only
  comments + the intentional legacy XML-property-string reads in the migration path).
- **Notes untouched** this stage (still beat fields + beat serdes; beat-scheduled playback) -
  migrate in Stage 3 (PianoRoll).
- Files: `VibesynthConstants.h`, `PatternManager.h`, `PatternManager.cpp`,
  `Standalone/BuilderPage.cpp`, `Standalone/StandaloneEditor.cpp`, `PluginProcessor.cpp` (comments).

## 2026-06-03 — Task 1 — Stage 1 verify round (findings; commit pending migration confirm)

- **Jeff's Stage-1 verify: "everything passed"** (fresh-project drop-WAV played + survived
  save/reload -> exercises the new tick block path + serdes round-trip).
- **Finding A (NOT Stage 1; resolved by Jeff; move-forward per Jeff):** an old project
  (`Projects/hARD BASS`) loaded with badly broken playback (first note held, notes firing wrong,
  distorted, playback progressively slower than the 60 BPM set tempo). Deep file-diff (general-
  purpose agent) vs a working faithful remake (`Untitled Project (65)`) found IDENTICAL note data
  (40 notes each), tempo, patterns, routing (no feedback loops), and empty effect racks -- the only
  difference was engine count (broken: 3 sfizz [Guitars+Basses+RustyDrums] + 2 NAM/IR chains + 8
  Inst strips; working: 1 sfizz [Basses] + 1 NAM). My first hypothesis (extra-engine audio-thread
  overload) was DISPROVEN by Jeff's tab-delete test (deleting Guitars + RustyDrums tabs changed
  nothing). Jeff root-caused it: the broken **Basses sfizz PLAYER STATE / CC settings were wrong**
  (tied to the recent QA-Sfizz / QA-Sfizz-Followup CC-default adjustments); loading a working
  page-save onto it fixed it. NOT a QA-Ee surface (file has zero arrangement blocks + byte-identical
  notes -> Stage 1 code never runs on it). Resolved; no saves in the wild can hit it (single-user
  pre-release). **Route at close:** out-of-scope-resolved row in the close routing table; cross-ref
  QA-Sfizz (old sfizz player-state may load wrong post-CC-adjustment) -- Jeff's call whether it
  warrants a §9 Forks back-ref or stays a logged-and-closed verify finding.
- **Finding B (NOT Stage 1; QA-ClipDrop-adjacent):** drop-fresh-WAV failed inside the troubled
  `hARD BASS` file but worked cleanly in a fresh project (+ survived save/reload). Consistent with
  the QA-ClipDrop held-open intermittent clip-drop bug, not Stage 1. Data point for the QA-ClipDrop
  watch if it recurs.
- **Migration confirmed (Jeff): all Stage-1 verify scenarios passed**, including opening pre-QA-Ee
  projects (old->tick migration of arrangement-grid blocks loaded correctly) + fresh-project
  drop-WAV + save/reload. Stage 1 cleared for commit.

## 2026-06-03 — Task 1b — Playback-length precision fix (issues 1 + 2; pre-build)

Two PRE-EXISTING playback bugs surfaced during Stage-1 verify (Jeff), both "playback length !=
the precise/displayed length." Fixed in-batch (qa-fix-bugs-dont-defer) as a dedicated commit BEFORE
Stage 2 (Jeff's option A, "fix both now" — he is actively hitting issue 1). Awaiting Jeff's
Debug+Release verify before commit. No diagnostic instrumentation added (Catalog stays empty).

- **Spec resolved (Jeff):** pattern blocks ARE sub-bar adjustable and STAY that way (NOT
  snap-to-whole-bar — I wrongly floated that option; scrapped). Playback must HONOR the sub-bar
  length. Sequencing: fix both now (option A); issue 1 gets an interim tick-snap, Stage 3 makes it
  native.
- **Issue 1 (pattern-mode loop point):** `getEffectivePatternLoopBeats` -> `ceilToBarStart`
  ([PatternManager.cpp:596](Source/PatternManager.cpp:596)) ceil'd the furthest note/step end (a
  float) to the next bar with a too-small `1e-9` guard; a note end that float-drifted a hair PAST a
  bar boundary (invisible on screen) ceil'd up an extra bar -> pattern loops a bar late (Jeff's two
  screenshots: same "note short of the line," one loops 1 bar, the other 2). FIX: snap `endBeat` to
  the 96 PPQ tick grid (`ticksToBeats(beatsToTicks(endBeat))`) before the ceil -> deterministic.
  Interim; Stage 3 (note->tick) makes note ends native ticks so the snap becomes a no-op.
- **Issue 2 (song-mode block playback length):** the pattern-block scheduler
  ([PluginProcessor.cpp:1229-1230](Source/PluginProcessor.cpp:1229)) + the automation-clip window
  ([:1408-1412](Source/PluginProcessor.cpp:1408)) computed the play span from the ceil'd integer
  `startBar`/`lengthBars`, ignoring the sub-bar `effective*` length the UI draws. FIX: use
  `effectiveStartBeats`/`effectiveLengthBeats` (block scheduler) + `effectiveStartBars`/
  `effectiveLengthBars` (automation) -> a sub-bar block plays its exact drawn length. Bar-aligned
  blocks unchanged (effective* fall back to the bar fields). The audio-clip path already used
  effective* (line 2334/2338) -- this brings pattern + automation blocks in line.
- **Route at close:** both are in-batch-resolved pre-existing-bug findings -> close routing table.
- Files: `PatternManager.cpp` (ceilToBarStart) + `PluginProcessor.cpp` (block scheduler +
  automation window).

## 2026-06-03 — Task 1b verify PASS + loop-seam stuck-note found (-> Task 1c)

- **Task 1b verified (Jeff):** issue 1 (pattern loop) now loops at the bar line with no phantom
  extra bar; issue 2 (song-mode block playback) exercised in song mode (pattern block from the piano
  roll on the Builder grid). Committing Task 1b now (Jeff chose option A: commit Task 1b, fix the
  new stuck-note as its own focused task).
- **New finding -> Task 1c (intermittent loop-seam stuck note):** a note ending exactly at the loop
  point occasionally hangs (held until the next loop re-fires it), repro'd in BOTH pattern + song
  mode. NOT created by Task 1b -- pre-existing QA-Ed scheduler edge case, EXPOSED by Task 1b (the
  loop-tightening puts notes on the seam). Mechanism (code-traced): the straddle test
  ([PluginProcessor.cpp:1111](Source/PluginProcessor.cpp:1111)) uses strict `>` so a wrap landing
  exactly on a block boundary is NOT a straddle; the off at loopEnd then matches no off-pass case
  ([:1156-1163](Source/PluginProcessor.cpp:1156)) -- not past-due (`<= beatStart`), not straddle,
  not strictly `< beatEnd` (it is equal) -- so it is stranded, held until a later mid-block-wrap
  iteration fires it. Intermittent because the wrap aligns to a block boundary only periodically
  (loop length in samples is not a whole multiple of the audio buffer).
  - **Fix direction (Task 1c):** flush the stranded loop-end off in the FIRST post-wrap block
    (likely a one-shot "just wrapped" signal); soak-test verify (intermittent). MUST preserve
    QA-Ed's integer-exact playhead/scheduler wrap agreement -- canNOT just loosen `>` to `>=`
    (that would make the scheduler straddle a block the playhead does not wrap in = the desync
    QA-Ed was built to remove).
  - **Cross-ref QA-Ed at close** (carry-forward-via-§9-Forks): QA-Ed scheduler edge case exposed by
    QA-Ee; fixed in-batch as Task 1c.

## 2026-06-03 — Task 1c — loop-seam stuck-note fix (pre-build/soak-verify)

Implemented (Jeff: skip the design gate, proceed -- the change is additive + mirrors an established
pattern). Verified by Jeff (soak test, Debug + Release): zero hangs over several minutes of looping in both
pattern + song mode; normal loops + mid-bar notes unaffected. Committing now. No diagnostic
instrumentation added.

- **Approach:** a one-shot `mLoopWrapped` flag on `StandalonePlayHead`, set the block the playhead
  wraps and consumed once by the scheduler -- a direct mirror of QA-Ed's `mSeekDiscontinuity`
  backward-seek flush. In the first post-wrap block the scheduler fires any pending note-off sitting
  at the loop point (`off.beatOff >= loopEndBeat`) at sample 0, so a note ending on the loop point
  is released at the seam whether the wrap lands mid-block (straddle, already handled) or exactly on
  a block boundary (the bug).
- **Additive / low-risk:** does NOT touch the straddle test, the wrap math, or QA-Ed's
  integer-exact playhead<->scheduler agreement. New flag + one new off-pass release line. The new
  release only matches offs at the loop point (same set the straddle rule fires), so it can't cut a
  legitimate in-progress note early. No double-fire: in the straddle case the off already fired at
  `wrapSmp` so the post-wrap flush finds nothing; in the boundary case the off was stranded so the
  flush releases it.
- **7 edits, 4 files** (mirrors `mSeekDiscontinuity` one-for-one): `StandaloneApp.h` (+`mLoopWrapped`
  + `getLoopWrappedFlag`), `StandaloneApp.cpp` (set in `advanceBlock` wrap branch + clear in
  `reset`), `PluginProcessor.h` (+ptr + `setLoopWrappedFlag`), `StandaloneApp.cpp` startup (wire),
  `PluginProcessor.cpp` off-pass (consume + flush).
- **Verify = soak test** (intermittent): loop a note sitting on the loop seam for a few minutes in
  pattern mode AND song mode, confirm zero hangs; plus a normal-loop regression check.

## 2026-06-03 — Task 2 / Stage 2 — Builder snap -> ticks + dynamic "Line" grid (pre-build)

**SCOPE UPGRADE (Jeff, 2026-06-03):** Stage 2 was upgraded mid-execution from a static 10-label
snap to a **dynamic zoom-aware grid + snap ("Line" / FL-Studio parity)**.  Amends SC-labels + SC-ii
-> record a §9 Forks entry + Main Plan §5 amendment at close.  Jeff-locked sub-decisions:
- Ladder extends to **1/64** (`{Bar 384, Beat 96, 1/8 48, 1/16 24, 1/32 12, 1/64 6}`).
- **11-label scheme, `Int 0..10`, uniform across all surfaces:** `0 Off / 1 Line / 2 Bar / 3 Beat /
  4 1/2 Beat / 5 1/3 Beat / 6 Step / 7 1/2 Step / 8 1/3 Step / 9 1/4 Step / 10 1/6 Step`.  (Kept
  both Off + Line per FL; did NOT diverge idx 0 by surface -- Jeff's call for a safer uniform map.)
- **Builder default = Line (idx 1)** -- flag at verify for confirmation.
- `kMinLinePx = 12` (tune at verify).

**Threshold math (the lock-to-view core):** a ladder rung of `g` ticks is "live" when its on-screen
spacing `(g / 384) * pixelsPerBar >= kMinLinePx`.  **Line snap = the finest live rung**; the grid
draws every live rung.  One threshold drives both, so snap + grid can never disagree -- you snap to
exactly the lines you see.  Floor = Bar.  Fixed modes (2..10) snap to their fixed division
regardless of zoom; their grid shows Bar + Beat + the chosen division (drawn while it clears
`kMinLinePx`).  Triplet lines render solid (SC-4).

**Architecture:** the param `Unified_BuilderSnapDiv` is the single source of truth; the grid reads
it LIVE via a new `onGetSnapDiv` callback (in `snapBarAlt` + `drawGrid`) and writes it via
`onSnapDivChanged` -- so `mSnapMode` / `setSnapMode` / `getSnapMode` / the `SnapMode` enum are all
gone (no cache to desync).  All 28 `snapBar` call sites (incl. slip-edit) inherit the new snap
automatically.

**Files:**
- `VibesynthConstants.h` -- `kUnifiedSnapLabels[11]`, `kNumUnifiedSnapDivs`, `snapDivToTicks(idx)`,
  `kDynamicSnapLadder[6]`, `kMinLinePx`, `dynamicSnapTicks(pixelsPerBar)`.
- `PluginProcessor.cpp` -- register `Unified_BuilderSnapDiv` Int 0..10 default 1 (Line).
- `BuilderPage.h` -- removed `SnapMode` enum + `mSnapMode` + accessors; added grid `onGetSnapDiv` /
  `onSnapDivChanged`; retyped toolbar `onSnapChanged` -> `void(int)`; `setSnapDivIndex` (combo
  silent-set) + `syncSnapComboFromParam` (display sync).
- `BuilderPage.cpp` -- `snapBarAlt` -> unified tick snap; `drawGrid` -> dynamic ladder / fixed
  division; 11-label combo (id = idx+1) + onChange -> `onSnapChanged(int)`; toolbar->grid wiring
  writes the param + repaints.
- `StandaloneEditor.cpp` -- wire grid `onGetSnapDiv` / `onSnapDivChanged` to the APVTS param
  (mirrors `record_quantize_div`) + `syncSnapComboFromParam()` at createBuilderPage.

**Known minor (NIT for verify/close):** combo display is synced from the param at startup only;
project-load resync isn't wired, so after loading a project that saved a non-Line snap the combo
may show Line until clicked.  Snap itself is always correct (grid reads the param live).

**Verify (build pending):** zoom in/out -> grid simplifies Bar<-Beat<-1/8<-1/16<-1/32<-1/64 and the
Line snap follows the finest visible line; fixed divisions (incl. triplets) snap + draw solid; place
/ drag / resize / slip all lock to the visible grid; Off = no snap.  Tune `kMinLinePx`.

## 2026-06-03 — Task 2 / Stage 2 — Builder zoom reset (FL macro + micro parity)

**Spec (Jeff, 2026-06-03):** expand the Builder's horizontal-zoom clamp on BOTH ends for true FL
parity (replaces the earlier "match the Piano Roll exactly / 15 clicks" framing):
- **Max zoom-IN** = Piano Roll density: **`vpW` px/bar** (= PianoRoll `vpW/4` px/beat x 4 -> 1 bar
  fills the viewport).  Micro-editing down to 1/64.
- **Max zoom-OUT** (macro): **`vpW/512` px/bar** -> 512 bars on screen (~15-min arrangement
  overview).  Piano Roll zoom-out left at `vpW/32` px/beat (unchanged -- it doesn't need macro).
- **Factor stays 1.15 on ALL paths** (no "chunky" Builder).  The wider total range naturally takes
  many more clicks end-to-end than the Piano Roll -- that is the correct, expected behavior.

**Implemented:** Builder clamp `[vpW/512, vpW]` px/bar at all **7** horizontal-zoom sites
(fitBlockToViewport, zoom-tool click, zoom-to-rect, Ctrl+scroll wheel, PgUp/PgDn, toolbar onZoom
lambda, doZoom).  Normalized all **6** discrete-zoom factors `1.25 -> 1.15` (zoom-tool click,
PgUp/PgDn keyboard, toolbar +/- buttons, menu Zoom In/Out) -- the keyboard + menu share doZoom/onZoom
with the toolbar buttons, so all must match or the button vs key/menu would differ (the chunkiness
Jeff flagged).  Ctrl+scroll was already 1.15.  Vertical zoom (Alt+scroll rows) untouched.  Stale
"full out = 32 bars / full in = 8 bars" limit comment updated to "512 bars / 1 bar".

**Open decision (SURFACED, not executed -- semantics change):** at full 512-bar zoom-out `drawGrid`
exempts the Bar line from the `kMinLinePx` threshold (line ~1773, `gTicks != 384 && ...`), so it
would draw ~512 bar lines at ~1.5px = a dense gray wash that defeats the macro overview.  Recommended:
gate Bar by the same threshold so the grid declutters cleanly at extreme zoom-out; bar-SNAP still
floors at Bar via `dynamicSnapTicks` (minor, intended draw/snap divergence at the extreme -- you
can't precisely place at 1.5px/bar anyway, and bar-snap stays useful for macro clip placement).
Awaiting Jeff's call at verify.

**PianoRoll:** untouched (already 1.15 on Ctrl+scroll; zoom-out stays `vpW/32`).  Its zoom-tool
single-click is a separate +30% step (different mechanism) -- left as-is per "PianoRoll looks
excellent"; flag if 1.15 parity is wanted there too.

## 2026-06-03 — Task 2 / Stage 2 — Content-bound dynamic zoom (full architecture reset, both windows)

**SUPERSEDES the static `[vpW/512, vpW]` Builder clamp above.**  Jeff's spec (2026-06-03): drop ALL
static hardcoded zoom constraints; implement FL's "monitor-restricted-empty + incremental-expansion"
clamp, bifurcated for both windows.  The PianoRoll's earlier "leave zoom-out alone" is ALSO superseded
-- both windows are now content-bound.

**Architecture (single source of truth per window):**
- Zoom-OUT min is computed, not stored: `minPP = vpW / maxThings`, where
  `maxThings = max(emptyBaseline, furthestContent + pad)` and `emptyBaseline = vpW / kDefault*EmptyPx`
  (monitor-dependent).  Empty project -> restricted workspace; grows as content is added.
- Builder content = furthest clip right edge (`effectiveStartBars+effectiveLengthBars`) + furthest
  time marker + furthest TS change; pad = 8 bars.  Piano Roll content = furthest note edge
  (`startBeat+durationBeats`, active pattern `mData->notes`); pad = 1 bar.
- Zoom-IN max = tick-level: `kMaxZoomInBeatsAcross` (0.5) beats fill the viewport at deepest zoom
  (~16 px/tick on 800px).  Shared by both windows.  (Replaces the old "1 bar fills viewport" cap.)
- Enforced ON ZOOM (helpers called at each clamp site).  Content shrink -> next zoom re-clamps
  (no jarring auto-snap).  Default mPPBar/mPPB stay 80 (inside every range).

**New constants (VibesynthConstants.h, all TUNABLE -- flagged for verify):**
`kDefaultPlaylistEmptyPx = 24` (Builder empty ~vpW/24 bars), `kDefaultPianoRollEmptyPx = 160`
(Piano Roll empty ~vpW/160 bars), `kBuilderZoomPadBars = 8`, `kPianoRollZoomPadBars = 1`,
`kMaxZoomInBeatsAcross = 0.5`.

**Helpers (centralize the clamp -- every zoom path calls them):**
- `ArrangementGrid::contentMaxBars() / minZoomPPBar(vpW) / maxZoomPPBar(vpW)` (BuilderPage.h/.cpp) --
  all 7 Builder clamp sites + the Shift+1..6 keyboard presets now route through these.
- `PianoRollContainer::contentMaxBars() / minZoomPPB(vpW) / maxZoomPPB(vpW)` (PianoRoll.h/.cpp) --
  `applyZoom` + `onZoomTo` (the only 2 PianoRoll clamp sites; everything funnels through `applyZoom`).
- const-method access to `mPM.getBlock()` / `mData->notes` is safe (reference/pointer members --
  same pattern as the existing `maxRevealableNegativeBars()`).

**Declutter (spec part 4 -- now implemented, not just surfaced):**
- Builder `drawGrid`: removed the Bar-line exemption -> ALL rungs incl. Bar gate on `kMinLinePx` (12).
- Piano Roll bar-line loop: wrapped in `if (mPPB*barBpb >= kMinLinePx)` (was "always drawn").
- Snap unaffected (Line snap floors at Bar via `dynamicSnapTicks`; only rendering culled).

**Scroll factor:** unchanged -- 1.15 on every path, both windows (done in the prior commit).

**Snap combo (spec part 5):** unchanged -- 11-label Int 0..10, Off=0 / Line=1 (Builder default).

**THIRD window included (Jeff's call, AskUserQuestion 2026-06-03):** the spec said "both windows,"
but the sweep found `DrumKitGrid` / `DrumKitContainer` (the drum kit sequencer) sharing the exact
old static clamp.  Jeff chose "Include it" so all timeline editors match.  Same treatment applied:
- `DrumKitContainer::contentMaxBars() / minZoomPPB(vpW) / maxZoomPPB(vpW)` (DrumKitGrid.h/.cpp).
  contentMaxBars scans ALL 16 drum rows (`mPM->currentPattern().drumRolls[0..kMaxDrumPages).notes`).
- 2 clamp sites converted (`applyZoom` + `onZoomTo`); bar-line loop wrapped in the kMinLinePx cull.
- Uses the Piano Roll constants (kDefaultPianoRollEmptyPx / kPianoRollZoomPadBars) -- it IS a drum
  piano roll.  Scroll factor already 1.15 (wheel); its +30% click-zoom left as-is (same pre-existing
  pattern as the Piano Roll's -- both flagged together if 1.15 parity is wanted there).

Final sweep: ZERO hardcoded zoom limits remain in Source/Standalone (all 3 editors content-bound).

**Build pending.**  Verify: empty project restricts zoom-out (no 100s of empty bars); add a clip far
right -> zoom-out expands to reach it +8 bars; Piano Roll + drum grid same with notes +1 bar; max
zoom-in reaches tick level all 3 windows; grid declutters cleanly (no bar wash) at extreme zoom-out
while snap still works; tune the 5 constants.

## 2026-06-03 — Task 2 / Stage 2 — Zoom follow-ups (Jeff verify feedback)

Two issues from Jeff's verify of the zoom work:

1. **Stale Builder ruler hint removed.** A placeholder string on the Builder ruler
   ("Ctrl+Scroll=zoom  Alt+Scroll=vZoom  P=Draw  B=Paint  E=Select  D=Delete  T=Mute") predated the
   real toolbar buttons.  Deleted the 5-line draw block in `ArrangementGrid::paint` ruler section.

2. **Cursor-anchored Ctrl+scroll for the Piano Roll + drum grid.** The Builder already anchored zoom
   to the mouse (bar under cursor stays put); the two piano-roll-style grids zoomed from the left
   edge instead.  Root cause: their grid `mouseWheelMove` emitted a bare `onZoom(delta)` with no
   cursor x, so the container couldn't anchor.  Fix mirrors the Builder:
   - New grid callback `onZoomAnchored(float factor, int anchorX)` (PianoRollGrid + DrumKitGrid),
     fired by the Ctrl+scroll branch with `e.x` (falls back to `onZoom` if unwired).
   - New container method `applyZoomAnchored(factor, anchorX)` (PianoRollContainer +
     DrumKitContainer): `anchorBeat = mBeatOff + anchorX/mPPB` (pre-zoom) -> clamp mPPB ->
     `mBeatOff = max(0, anchorBeat - anchorX/newMPPB)` so the beat under the cursor stays fixed.
   - Wired `mGrid->onZoomAnchored` next to the existing `onZoom` wiring.  Other zoom paths
     (toolbar / keys / click) unchanged -- only Ctrl+scroll anchors, per Jeff's request.

Both grids share the `xToBeat(x) = mBeatOff + x/mPPB` mapping (no x-offset), so the anchor math is
exact.  Build pending.

## 2026-06-03 — Task 2 / Stage 2 — Zoom-to-cursor BOTH axes + drum-kit horizontal-only + keybinds

Jeff verify feedback (plan restated + confirmed before executing per the semantics-change rule):
correction -- it's not just Ctrl+scroll; BOTH Ctrl+scroll (horizontal) AND Alt+scroll (vertical)
must anchor to the mouse on the Builder + Piano Roll.  The drum kit gets NO vertical at all (16 rows
fixed by design) -- horizontal only.  ("browser" = Builder, confirmed; drum-kit plain wheel =
horizontal scroll, confirmed.)

**Vertical zoom-to-cursor (Builder + Piano Roll):**
- Builder (`ArrangementGrid::mouseWheelMove` Alt branch): vertical zoom scrolls via the parent
  Viewport, so after `mEffectiveRowH` changes I shift `setViewPosition` by
  `(e.y - kRulerH) * (actualFactor - 1)` -> the row under the cursor stays put.  Self-contained in
  the grid (like its horizontal anchor).
- Piano Roll: pans vertically via `mTopNote` (internal, not viewport).  New grid callback
  `onVZoomAnchored(factor, anchorY)` + container `applyVZoomAnchored`: capture the pitch under the
  cursor, `applyVZoom`, then `onVScroll(noteBefore - noteAfter)` to put it back.  Note math mirrors
  `PianoRollGrid::yToNote` using the public `kNoteH` / `kRulerH` statics (yToNote itself is private;
  `mNoteYOffset` is always `kRulerH`).

**Drum kit -> horizontal only (16 fixed rows):**
- Removed the wheel Alt vertical-zoom branch -> Alt+scroll now falls through to horizontal scroll
  (plain wheel + Shift already did horizontal scroll; no vertical scrollbar exists).
- Removed the two right-click menu items "Zoom In/Out Vertical" (ids 53/54) + their handlers.
  `applyVZoom`/`onVZoom` left as harmless dead code (no callers).  The Control-Lane Alt+Wheel
  (velocity/pan value adjust) is a separate handler -- untouched.

**Keybinds window (replaces the deleted ruler hint):** mouse-reference rows already existed for the
Builder + Piano Roll -- refined Ctrl+Wheel / Alt+Wheel descriptions to say "cursor-anchored ... stays
under the mouse."  Added a Drum Kit mouse-modifiers block (Mouse Wheel + Shift+Wheel = Horizontal
Scroll, Ctrl+Wheel = Horizontal Zoom cursor-anchored, explicit "no vertical / 16 rows fixed").

**Build pending.**  Verify: Builder + Piano Roll Alt+scroll vertical-zooms toward the cursor (row /
pitch under the mouse holds); drum kit has zero vertical zoom/scroll (Alt+scroll = horizontal scroll,
rows fixed); Key Binds window documents all of it under the right tabs.

## 2026-06-03 — Task 2 / Stage 2 — Ctrl+scroll-to-cursor scrollbar bug (Piano Roll + drum kit)

Jeff verify: Alt+scroll (vertical) anchors correctly everywhere, but Ctrl+scroll (horizontal) on the
Piano Roll + drum kit snapped the view to bar 0 instead of holding the cursor.  Builder was fine.

**Root cause (the asymmetry was the tell):** a cursor-anchored *horizontal* zoom can push `mBeatOff`
PAST the last note (you can anchor on empty space right of the content).  But
`pushScrollStateToBars()` capped the H-scrollbar's `totalBeats` at `lastNoteEnd + 4` (Piano Roll) /
`max(patternBars, last+bpb)` (drum kit), so `setCurrentRange` clamped the thumb to 0, and the
scrollbar's (async) `scrollBarMoved` -> `mBeatOff = newStart` snapped `mBeatOff` back to bar 0.
Vertical never hit this because `mTopNote` is always inside `[minTop, 127]` (always representable).
Builder never hit it because it pans via `mBarOff` + a Viewport (negative bars allowed), not this
content-capped scrollbar.

**Fix (both containers, in `pushScrollStateToBars`):** include the current offset in the scrollbar
extent -- `totalBeats = max(content, mBeatOff + visibleBeats0)` -- so the anchored offset is always
representable, the thumb never clamps, and `scrollBarMoved` can't reset it.  `applyZoomAnchored` (the
direct mBeatOff math) was correct all along; the scrollbar was eating the result.  No new clamp on
normal scrolling (offset only exceeds content when you deliberately zoom into empty space).

**Build pending.**  Verify: Ctrl+scroll on Piano Roll + drum kit now holds the beat under the cursor
(no snap to bar 0), including when anchoring on empty space right of the last note; normal H-scroll +
the Builder unaffected.
