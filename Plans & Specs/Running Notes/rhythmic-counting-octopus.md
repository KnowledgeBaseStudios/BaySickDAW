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

## 2026-06-03 — Task 2 / Stage 3a — Piano Roll + drum kit GLOBAL tick snap (param + grids + wiring)

Stage 3 decisions (Jeff, AskUserQuestion 2026-06-03): Piano Roll snap goes GLOBAL (SC-B) on the same
11-label scheme as the Builder; Line mode INCLUDED; default = Line; the drum kit grid SHARES the same
global snap (it's a notes editor on the same page).  Split into 3a (snap) + 3b (note tick serdes).
Sub-commits per SC-A.

**3a (this pass) -- the global tick snap:**
- New APVTS Int param `Unified_PianoRollSnapDiv` (0..10, default 1 = Line), registered next to
  `Unified_BuilderSnapDiv`.
- Both grids (PianoRollGrid + DrumKitGrid): `snapBeat` rewritten to tick-based reading the live div
  via a new `onGetSnapDiv` callback (div 0 Off / 1 Line=`dynamicSnapTicks(mPPB*4)` / 2..10 fixed
  `snapDivToTicks`) -- tick-exact, so triplets land precisely (no 1/3 float drift).  New
  `snapUnitBeats()` helper; every `4.0/mSnapDenom` site (note length + nudge/quantize/arp/randomize)
  routed through it (replace_all, 7 PianoRoll + 5 drum-kit sites, all grid methods).
- Both containers: magnet right-click menu rewritten to the 11-label unified list writing the GLOBAL
  param via `onSetSnapDiv`; the legacy quantize submenu's `setSnapDenomAndQuantize(denom)` maps the
  old 4/8/16/32 -> unified div + writes the global param.  New `setSnapAccessors(getter,setter)`
  stores the accessors + pushes the getter into `mGrid->onGetSnapDiv`.
- GLOBAL is automatic: every grid reads the SAME param live, so no per-instance push needed.
  `PianoRollPage::setSnapAccessors` fans the accessors into `mDrumKit` + every roll in `mRolls`, and
  `registerEngine` wires future rolls.  StandaloneEditor provides the apvts read/write (mirrors the
  Builder's `Unified_BuilderSnapDiv` wiring).  `mSnapDenom` + `mSnapEnabled` are now dead (left in
  place, harmless).

**On/off is GLOBAL too (Jeff, AskUserQuestion 2026-06-03):** the Snap button's left-click toggles the
GLOBAL div (Off <-> the last non-Off div, stored in `mLastSnapDiv`) instead of a local `mSnapEnabled`
-- so on/off AND resolution are both global + agree with the menu's "Off" (idx 0).  `snapBeat` drops
the `mSnapEnabled` check (div 0 = the single global on/off).  The button's lit state is set from the
div on click / menu / `setSnapAccessors`.  KNOWN NIT: a roll you switch TO may show a stale lit state
until you click/menu its Snap button (no becomes-visible refresh wired) -- the snap itself is always
correct + global; flagged for a quick follow-up if it bugs.

**Build pending (3a).**  Verify: snap menu on any piano roll OR the drum kit shows the 11 labels
(Off / Line / Bar / Beat / 1/2 Beat / 1/3 Beat / Step / 1/2 Step / 1/3 Step / 1/4 Step / 1/6 Step),
default Line; changing it on ONE roll changes ALL of them (global); finer placement (down to 1/64 /
triplets / Off=any tick); Line follows the zoom.  3b (note `st`/`dt` tick serdes + old-project
migration) follows after this verifies.

**3a follow-up (Jeff verify):** notes couldn't be made smaller than 1/32 -- a pre-existing hardcoded
`4.0/32.0` (1/32-note) floor on note resize / draw / slice in BOTH grids (left over from before the
finer snap).  First pass flat-lowered all 6 to 1 tick -- but Jeff caught that snap ON then let you
resize BELOW one step (the floor collapsed to 1 tick when the snapped edge hit the note start).  Fix:
the resize + drag-draw floors are now CONDITIONAL -- snap ON (not Alt, div > 0) -> `snapUnitBeats()`
(one snap step, so snap-on can't go sub-step); free (Alt-drag or div 0 = Off) -> 1 tick
(`1.0/kTicksPerBeat`).  The slice min-fragment stays 1 tick (its point already snaps to the grid, so
the snap governs fragment size).  LEFT as-is: the two `kGraceLen = 1/32` flam grace-note lengths
(intentional) + the arpeggiate note-duration floor (`jmax(0.125,..)`, an auto-tool default).

## 2026-06-04 — Task 2 / Stage 3 — Grid renderer unified on snap ladders (decoupled from snap) + triplet even-scaling

**Problem (Jeff verify):** the Piano Roll + Drum Kit grids never drew finer than 1/32 in ANY snap
mode -- they used a hardcoded `{8,4,2,1}` per-beat subdivision table totally separate from the snap
engine, so a note could snap to 1/64 (or finer) with no grid line under it ("snap to open space").

**Spec evolution (Jeff, this session):** first idea (cap the grid AT the snap division) was REVERTED
-- wrong.  Final spec: the visual grid is fully DECOUPLED from the snap.  Snap = pure magnetism (where
notes/blocks land); the grid draws straight subdivisions Bar->1/64 purely by zoom/pixel room,
independent of the snap division.  The snap TYPE (straight vs triplet) picks which ladder is drawn;
the snap DIVISION never caps the visual.

**Implemented -- single source of truth in `VibesynthConstants.h`:**
- `kTripletGridLadder = {384,96,32,16,8,4}` (triplet ladder, ticks) beside the straight
  `kDynamicSnapLadder = {384,96,48,24,12,6}`.
- `isTripletSnapDiv(div)` (1/3 Beat=5, 1/3 Step=8, 1/6 Step=10) + `gridLadderForSnap(div, count)` ->
  returns the straight or triplet ladder for the active snap.  All three grids + the snap key off these.
- `BuilderPage::drawGrid`: dropped the fixed-snap cap; iterates `gridLadderForSnap()` (full ladder,
  zoom-gated).  `PianoRoll` + `DrumKitGrid`: dropped their `{8,4,2,1}` tables; same shared-ladder loop
  (skip the 384 bar rung -- bars are a separate TS-aware pass; the Builder draws bar from the ladder
  since it's uniform 4/4).

**Jeff calls this session:**
- Builder LEFT AS-IS -- its straight grid caps at 16 cells/bar (`kMinLinePx=12`); Jeff confirmed FL's
  playlist caps at 16/bar, so that's correct.  Did NOT lower the Builder grid threshold.  Snap
  thresholds untouched (Line snap stays `kMinLinePx=12`); grid + snap stay decoupled.
- Triplet even-scaling: the first triplet ladder `{384,96,32,8,4}` jumped 3->12 lines/beat (4x --
  "split into 4, not triplets").  Added the 1/16-triplet rung (16t) -> `{384,96,32,16,8,4}` so it
  doubles evenly 3->6->12->24/beat.  The 16t rung is grid-only (no snap target; snap set is
  1/8T/1/32T/1/64T) -- every actual snap target still lands on a drawn line.  Jeff: per-bar counts now
  correct; accepts the visual sub-split, moving on.

**No separate commit yet** -- bundles with Stage 3a for the Stage 3 source commit.

## Out-of-scope findings — discovered during QA-Ee, ROUTE AT BATCH CLOSE (placement = Jeff's call)

Not QA-Ee scope (grid/snap).  Jeff flagged 2026-06-04 to route at close (§9 Forks -> batch(es); slot
is a spec call):
1. **Quit save-prompt dialog is draggable.**  On app close, click-and-hold grabs the save-confirmation
   box and drags it around.  Should be a fixed, centered modal -- not movable.  (Likely the
   close-confirm AlertWindow/DialogWindow needs its movable behavior disabled + centered.)
2. **"Cut Self" doesn't work on Layers or Bass.**  Works on drum-kit grid entries; not on Layers/Bass.
   Needs program-wide investigation (confirm the exact feature + why it's drum-kit-only) when routed.
3. **Layers don't auto-name from the loaded patch.**  Patch name not propagating to (a) the Layers tab
   dropdown, (b) the piano-roll picker dropdown, or (c) the piano-roll top-right patch/player name tag.
   Drums already auto-rename on sound load; Layers should match.

## 2026-06-04 — Task 3 / Stage 3b — PianoNote tick serdes + legacy migration (pre-build)

Completes Stage 3 (3a global snap + the grid-renderer unification already landed this session).  Mirrors
the Stage 1 block tick serdes, for notes.
- `PianoNote` (PatternManager.h): added `int64 startTicks {0}` / `durationTicks {24}` -- the 96 PPQ
  on-disk representation.  startBeat/durationBeats remain the in-memory editing authority (tick-aligned
  by the snap engine); the tick fields are filled on load, and `st`/`dt` are derived from the live beats
  at save.
- `noteToValueTree` (PatternManager.cpp): writes `st = beatsToTicks(startBeat)` / `dt =
  beatsToTicks(durationBeats)`; legacy float-beat `s`/`d` props dropped -- new format is tick-only
  (downgrade unsupported, matching the block serdes).
- `noteFromValueTree`: tick-first (`st`/`dt` -> ticks + beats) else legacy (`s`/`d` float beats ->
  nearest tick via beatsToTicks).  So pre-QA-Ee projects migrate to the nearest 96 PPQ tick on load, and
  re-saving upgrades them to tick format.
- `PianoRollData.snapDenominator` serdes LEFT as-is (dead per SC-B global snap; harmless; removing it is
  out of scope).

**Build pending (3b).**  Verify: (1) load a pre-QA-Ee project -- notes appear at the same positions +
play correctly (grid-aligned notes unchanged; free-placed notes land on the nearest tick); (2) make +
save + reload a new pattern -- notes round-trip exactly.  After verify, the whole Stage 3 (3a + grid
unification + 3b) commits as one via /draft-commit.

**3b build fix (build round 1 failed):** first pass put `startTicks`/`durationTicks` in the MIDDLE of
`PianoNote` (right after `durationBeats`), which shifted its positional aggregate-initializers -- 4 sites
in PianoRoll.cpp (388 / 1382 / 1546 / 1736), shape `PianoNote{ midiNote, startBeat, durationBeats,
velocity, ... }` -- so `velocity` landed in the int64 slot (C2397 narrowing) and `NoteType` in a float
slot (C2665).  Fix: relocated both fields to the END of the struct (trailing fields just take defaults in
those brace-inits; the serdes sets them by name, position-independent) + a KEEP-LAST comment so they're
not moved back.  Rebuild pending.

## 2026-06-04 — Stage 3 committed + 2 more out-of-scope findings (effects regressions)

Stage 3 committed as `56ecc38` (13 files, +453/-112) -- 3a global snap + grid unification + 3b note
serdes, all verified by Jeff in Debug + Release.  Batch stays open for Stage 4 (Record-Quantize) + close.

**Two more out-of-scope findings (Jeff, 2026-06-04) -- ADD TO CLOSE ROUTING (join findings 1-3 above):**
4. **Compressor: multiple types broken -- gain reduction only in a narrow band.**  Jeff: "small band
   shows reduction but further down or up no reduction at all."  GR works in a narrow range, not above/below.
5. **Flanger BPM button is one-way.**  Turns BPM/sync mode ON but won't turn it OFF (the toggle won't
   release).

**Regression lead (git trace, NOT a deliberate edit):** Jeff reports both worked for a long time with
nobody touching the effects.  `git log` on the cores: CompressorDSP.cpp + FlangerDSP.cpp UNCHANGED since
the pre-QA commit `984466e`; EffectEditorPanels.cpp (UI for both -- flanger BPM button + compressor type
controls) last changed only by `a472a44` (QA-0a em-dash sweep, 217 files).  An em-dash sweep can only
touch string literals + comments (em-dashes aren't legal in C++ code outside those), so it cannot have
altered the toggle/compressor logic -- almost certainly innocent.  => the breakage is most likely in a
SHARED dependency the effects use but that lives in another file (EffectRack, per-insert effect
processing in VibeGraph/PluginProcessor, APVTS effect-param wiring, a LAF) or a DELETED helper.  Jeff's
"did deleting ST remove shared logic?" hypothesis is plausible IF ST was a shared dependency -- pending
Jeff confirming what "ST" refers to so its removal commit can be checked against the effects' code path.

**Jeff wants a broad review sweep** -- silent multi-feature breakage with no deliberate edit to the
feature is exactly the signal for one.  PROPOSED at close: a dedicated correctness/regression sweep,
distinct from the mandatory QA-Ee /review-batch (covers only the QA-Ee diff -- won't catch pre-existing
regressions) and from /perf-audit (perf-only).  Scope = the 12 effect modules + their panels + the shared
rack/insert/param/LAF code they depend on + a pass over recent deletions for collateral.  Slot/scope =
Jeff's call at close.

**ST clarified (Jeff, 2026-06-04): ST = SINGLE THREAD.**  The app's original single-threaded render path;
the build then moved to a multi-thread render engine (Engine/Tasks/ -- RenderTask / RenderGraphDispatcher
/ MasterTask / *StripTask, the very files throwing the C4324 padding warnings in the Stage-3 build log),
and the dedicated single-thread path was DELETED, replaced by a "single worker mode" (the multi-thread
engine run with 1 worker) to test ST-like scenarios.  This reframes the sweep: the leading hypothesis is
now a single->multi-thread MIGRATION regression in how EffectRack inserts (+ the pedal-board effects,
which share the rack connection) get their per-block STATE + PARAMETERS across the worker boundary -- not
a generic shared-dep issue.  The compressor symptom (GR only in a narrow band) is consistent with an
envelope-follower whose state isn't carried across blocks (a reset-per-block detector reacts only to
instantaneous level); the flanger BPM toggle-won't-release is consistent with a per-block parameter
snapshot not picking up the OFF state.  (Both are hypotheses -- not yet verified in code.)

**Cheap diagnostic for Jeff (before any code dig):** run the broken compressor + flanger in SINGLE-WORKER
mode vs MULTI-WORKER mode.  Different behavior -> the threading migration is the culprit (state/param per
worker; multi-worker-only = a parallelism / state-split issue).  Same broken in both -> the migration
changed effect processing fundamentally (not parallelism-specific).  Works in single-worker only -> a
parallelism race / state split.  This A/B narrows the whole sweep before reading a line of engine code.

**Revised sweep scope:** the multi-thread render engine <-> EffectRack / InsertNode processing handoff
(per-block effect STATE + PARAMETER continuity across workers); EVERY stateful effect (all 12 modules) +
the pedal-board effects (BaySickPedals -- shares the rack connection) for state-continuity; + the git diff
of the single-thread-deletion / multi-thread-migration commit(s) as the regression window.

**A/B result (Jeff ran it): NO CHANGE single-worker vs multi-worker.**  Jeff correctly anticipated this --
"single worker mode" is the MT engine with all-but-one worker PARKED running the IDENTICAL dispatcher/task
code (QA-Ef's workerLoop park-when-OFF gate), NOT the deleted ST path.  So the A/B can't isolate ST-vs-MT;
what it DOES establish: the bug is NOT parallelism (no race / worker state-split) -- present with 1 worker
OR N, so it lives in the MT effect-processing LOGIC itself.

**Regression window IDENTIFIED: QA-Ef** (silly-name `synchronous-dreaming-hummingbird`, closed 2026-05-23,
plan `Plans & Specs/Batch Plans/synchronous-dreaming-hummingbird.md`) -- the Serial (ST) Render-Path
Deletion.  Deleted the serial render tail + collapsed the shared insert/clip helpers (`routeInsertOutput`,
`renderAudioClipsForRow`, `renderFilePlayPlayer`) to MT-only, making MT the single unconditional render
path.  Leading read: the compressor/flanger were already DIVERGENT (broken) under MT and worked under the
ST tail; QA-Ef removing the ST fallback EXPOSED the MT breakage -- consistent with QA-Ef's own §9-25
finding that 3 feeds had leaked ST-only (master/MIDI recorders + metronome).  This is more MT divergence.

**Compressor detail (Jeff): NOT all modes -- Vintage-knee + FET + Opto broken; cleaner modes OK.**  Those
three are the program-dependent / state-heavy / time-constant detector modes -- strong signature of the MT
path not carrying per-block detector STATE (or a wrong prepare / sample-rate / block-size feeding the time
constants).  Flanger BPM-toggle-won't-release is a separate param/latch issue on the same path.

**Investigation dispatched** (read-only agent): QA-Ef commit diffs + the live MT EffectRack / InsertNode /
StripTask insert-processing + CompressorDSP modes + FlangerDSP/panel toggle.  Findings land here + scope
the routed sweep.

**MAP RESULT (investigation complete 2026-06-04) -- OVERTURNS the ST-deletion hypothesis.**
- **ST->MT render parity is CLEAN.**  No audio/feed behavior the deleted ST tail did is unreplicated by MT
  beyond the 4 QA-Ef already caught (master/MIDI recorders, metro, FX-bus meter).  VERIFIED by main session:
  QA-Ef close commit `ad956bf` touched ZERO effect-DSP/rack/panel LOGIC (only a 2-line comment in
  `Source/DSP/EngineSidechainHelper.h`).  CompressorDSP/FlangerDSP/EffectRack/EffectEditorPanels are
  UNCHANGED in the QA era (git: last touched pre-QA `984466e`).  So the ST deletion did NOT directly break
  these -- the relevant effect code is untouched by it.
- **Flanger BPM -- VERIFIED root cause, pre-existing (NOT ST):** `FlangerDSP::setSyncBPM(true)` recomputes +
  snaps the rate; `setSyncBPM(false)` clears the flag but NEVER restores mRate/mRateSmooth -> LFO stays at
  the last synced rate (off-in-flag, stuck-by-ear).  Same one-directional pattern in DelayDSP + PhaserDSP
  `setSyncBPM`.  (FlangerDSP.cpp:49-66.)  OPEN for Jeff: is "won't turn off" the audible rate stuck (code
  predicts this) vs the button visually refusing to un-toggle (the widget is symmetric in code, so a visual
  refusal would be a separate UI bug)?
- **Compressor -- UNCONFIRMED; render path EXCLUDED.**  State continuity intact (single contiguous block per
  callback; no per-block `rack.reset()` -- reset() is dead code), sidechain replicated, instance identity
  preserved.  Candidates (pre-existing, not QA-Ef): Vintage-knee narrow-band BY DESIGN (ratio tapers ->1.0
  over 12 dB above threshold, CompressorDSP.cpp:247-254); FET tanh GR cap (~:482-493); Opto release-blend +
  meter ballistics; + per-panel threshold REMAPPING (FET Input->-60..0; Opto PeakRed 0..100->0..-40) makes
  cross-mode A/B misleading.  THE decider: WHERE is the broken compressor?  On Audio/Vox/Inst strips,
  stateful effects get `processInsert` called MULTIPLE times per block (partial sub-buffers) -> envelope
  corruption -> exactly the "narrow band" symptom; Layer/Bass/Drum/Bus inserts are single-call / safe.
  (VibeGraph.cpp:2497-2511.)  Need Jeff's strip location + a fixed-sine GR-per-mode bench test.
- **GENUINE NEW HAZARD (real engine bug):** stateful effects (compressor envelope, reverb tail, delay
  feedback, chorus/phaser/flanger LFO) on Audio/Vox/Inst inserts processed multiple times per block.

**Batch reframed (still warranted):** NOT "restore ST/MT parity" (clean) -> an EFFECTS-CORRECTNESS batch:
(1) flanger/delay/phaser one-directional un-sync (verified); (2) multi-call-per-block for stateful effects
on Audio/Vox/Inst strips (real); (3) compressor per-mode bench-test confirm + fix real bug / misleading
meters + threshold remap.  Distinct from QA-Eg (bus-meter G1/G2 split, already routed at QA-Ef close).
Slot: immediately after QA-Ee (Jeff).  Silly-name: mine, TBD at draft.  Optional hardening: pull the actual
deleted-ST diff (agent's session was blocked from git) to confirm parity + whether the audio-clip
multi-call is itself an ST->MT divergence.

**UPDATE (2026-06-04, post-strip-location + ST-diff verify):**
- **Jeff: the broken compressor is on a LAYER strip.**  VERIFIED: Layer inserts are SINGLE-CALL in MT --
  `EngineInsertTask::run` renders the engine once + calls `processInsert(Layer, i)` once per block
  (EngineInsertTask.cpp:92,102-103).  AND the deleted ST tail processed Layer inserts IDENTICALLY -- one
  `processInsert(InsertKind::Layer, i)` per block (ad956bf diff, removed line ~447).  => the multi-call
  hazard does NOT apply to a Layer compressor, and the ST deletion did NOT change Layer-insert processing.
  Both the render path and the ST deletion are EXCLUDED for the Layer compressor.  (My earlier "multi-call
  is probably your culprit" was premature -- it assumed an Audio/Vox/Inst strip.)
- **The multi-call (Audio/Vox/Inst) is PRE-EXISTING, not an ST->MT regression.**  The deleted ST tail also
  looped per-clip-page for Audio (`for ci < kMaxClipPages` -> processInsert(Audio, ci)) + called
  `renderAudioClipsForRow` (per-clip) + `renderFilePlayPlayer` per player; QA-Ef only swapped the OUTPUT
  routing (routeInsertOutput -> mtDest), not the per-clip looping.  So it's a long-standing trait -- still a
  real bug for stateful effects on those strips, but QA-Ef did not introduce it.
- **CONCLUSION on the Layer compressor:** NOT the render path, NOT the ST deletion (both verified single-
  call + unchanged), and CompressorDSP/panel code is unchanged in the QA era.  Remaining: a latent bug in
  the Opto/FET/Vintage mode DSP, OR intended per-mode behavior (Vintage knee narrow-band by design) + the
  per-panel threshold mapping (FET/Opto emulate different hardware controls -> cross-mode A/B by knob
  position is misleading).  DECIDER: (1) read the Opto/FET/Vintage mode math + FET/Opto threshold mapping
  (agent flagged a FET mapping comment that contradicts the code -- bug smell); (2) fixed-sine GR-per-mode
  bench at identical settings.

**COMPRESSOR ROOT CAUSES FOUND (2026-06-04 code read by main session) -- 2 real bugs + Opto cleared:**
- **Vintage knee (kneeType 2/6) -- REAL BUG, CompressorDSP.cpp:245-254 (`computeGainDb`).**  The "optical"
  model fades effectiveRatio -> 1.0 linearly over 12 dB above threshold, so GR = overshoot*(1/effRatio - 1)
  becomes a HUMP: 0 at threshold, peaks mid, back to 0 at >= 12 dB over.  => compresses only in a ~12 dB
  band and NOTHING above it.  Exactly Jeff's "reduction in a narrow band, none above."  Intentional in code
  (comment "mimicking electro-optical behavior") but the math is wrong -- real optos don't stop compressing
  at high level.  Pedal-board-era (file unchanged since pre-QA `984466e`).
- **FET (mType FET) -- REAL BUG, EffectEditorPanels.cpp:402-407 (FET panel knob[0] wiring).**  The "Input"
  knob maps slider dB DIRECTLY to threshold, meaning inverted: top of knob (0 dB) -> setThreshold(0) =
  threshold 0 dBFS = ~no compression; the code comment literally claims "0 dB = aggressive threshold; -60 =
  no comp" which is backwards (threshold 0 = no comp; -60 = max comp).  So turning Input UP (expecting more
  squash, 1176-style) gives LESS/none; only behaves near the default (-12).  Exact slider orientation to pin
  at fix, but the threshold semantics are definitely wrong.  Pedal-board-era (panel logic unchanged).
- **Opto (mType Opto) -- DSP is CORRECT, NOT an amount bug.**  Threshold map (PeakRed 0..100 -> 0..-40 dB,
  EffectEditorPanels.cpp:559-563) correct; release coefs `exp(-1/(0.060/0.500/1.000 * sr))` = 60/500/1000 ms
  (CompressorDSP.cpp:220-222) correct; history-weighted fast/slow release blend (:460-461) correct.  So
  Opto's "broken" is NOT compression-amount -- suspect the Opto panel's separate timer-driven GR meter, or
  its slow LA-2A release reading as "barely compressing" next to the others.  Needs the Opto panel meter
  checked OR Jeff's exact Opto symptom (sound vs meter).

=> Batch docket (effects-correctness) now CONCRETE: (1) Vintage-knee GR-hump; (2) FET inverted Input->
threshold map; (3) Opto meter/behavior verify; (4) flanger/delay/phaser one-way un-sync; (5) Audio/Vox/Inst
multi-call-per-block for stateful effects.  All pre-QA / pedal-board-era origin (none introduced by the QA
batches incl. QA-Ef).

## 2026-06-05 — Stage 4 + recording-displacement fix verified; 2 more out-of-scope findings (piano-roll UI)

**RECORDING-DISPLACEMENT FIX (MIDI count-in pre-roll) — surfaced in Stage 4 verify, fixed in QA-Ee, verified.**
Symptom (Jeff): with a count-in, recorded MIDI notes land a full measure early + a held full-bar note
collapses to a 1/64.  Present with snap ON and OFF -> NOT the Stage 4 quantize (the quantize block is
skipped when snap is off, so it's provably inert here).
- **Root cause (confirmed in code):** the MIDI recorder timestamps notes from `pos.getPpqPosition()`
  (PluginProcessor.cpp:1706).  The count-in defers `mPlayHead.start()` to a msg-thread timer
  (StandaloneEditor.cpp:5459-5474 + CountInTimer .h:450-458), so the playhead is NOT "playing" during the
  count-in.  Post-QA-Ed `advanceBlock` gates on `mPlaying` (StandaloneApp.cpp:176) -> `mSamplePos`/PPQ
  FREEZE during the count-in.  The old float `mPPQPos` advanced through the count-in; the new int64 clock
  doesn't.  So the count-in dropped out of the recorded timeline, but the commit still subtracts one bar
  (`startBeat -= preRollBeats`) -> everything a measure too negative; downbeat notes go negative + the
  early-strike clamp shears their length.  QA-Ed (ffc6dc7) changed ONLY the playhead internals
  (startPlayback/count-in/preRoll untouched per `git show`) -- its int64 rebuild EXPOSED the QA-Ea pre-roll
  assumption.
- **Audio (Inst/Vox/master) UNAFFECTED — verified:** audio recording never reads the playhead (real-time
  WAV capture + `preRollSamples` sample-count + arm-position placement + `contentStartSamples` trim = the
  slip-editable negative space).  `git diff | grep` confirmed my change touches ZERO contentStart /
  slip-edit / audio-recorder code.  That asymmetry is WHY MIDI broke and audio didn't (MIDI needed a
  per-note beat number from the frozen playhead; audio just records samples).
- **Fix (MIDI recorder only, 3 files):** the recorder keeps its OWN count-in-inclusive beat clock
  (`mElapsedBeats`, advanced by numSamples*bps every block from arm) instead of the frozen playhead.  The
  count-in re-occupies [0, preRollBeats) of the recorded timeline; the commit's `-= preRollBeats` lands the
  downbeat on bar 0 and pushes the count-in into negative space (bar -1 -> 0) where the locked (a)/(b)
  noodling-discard + early-strike rules handle it -- design RESTORED, not amputated.  MidiRecorder.h/.cpp
  (`processBlock` arg `beatStart`->`numSamples` + new `mElapsedBeats`, reset in startRecording) +
  PluginProcessor.cpp:1711 (caller passes numSamples).  Brings MIDI in line with how audio already
  self-times.  (Rejected the 1-line "stop subtracting preRoll for MIDI" -- it would delete the
  noodling-discard feature; Jeff overruled, correctly.)
- **VERIFIED (Jeff, Debug+Release 2026-06-05):** count-in take lands where played; full-bar note stays full;
  noodling-discard + early-strike intact; Stage 4 quantize works on top.
- **Routing:** spans QA-Ed (closed, the playhead change) -> fix lands in QA-Ee.  §9 Forks back-ref to QA-Ed
  at close.

**STAGE 4 — Record-Quantize (`Unified_RecordQuantizeDiv`) — VERIFIED.**
- `record_quantize_div` (Int 0..5) -> `Unified_RecordQuantizeDiv` (Int 0..10, default 0=Off) on the shared
  11-label scheme (PluginProcessor.cpp).
- GlobalTransportBar "Global Record-Quantize" submenu built from `kUnifiedSnapLabels` (11 items, ids
  100..110) -- identical to the Builder/PianoRoll snap pickers; handler range widened.
- StandaloneEditor: getter/setter repointed; MIDI-commit consumer rewritten from the beat-halving switch to
  96-PPQ tick-grid snapping (`snapDivToTicks` -> round startBeat to nearest grid tick), triplet-aware for
  free.  Guard on the TICK value (`g>0`), NOT the index -> Off (0) AND Line (1) are both no-snap (Line has
  no fixed grid at record-commit; the plan's literal `div>0` would div-by-zero on Line in the 11-label
  scheme -- a real correction over the plan snippet).  Old projects reset record-quantize to Off on load
  (id change; old indices don't map; Off is the safe default).  Stale `record_quantize_div` refs cleaned
  (GlobalTransportBar.h + the Stage-2 Builder-snap comment).
- **VERIFIED (Jeff, Debug+Release 2026-06-05):** 11 menu labels; Step->1/16, 1/3 Beat->eighth-triplet,
  Off->raw timing.

**TWO MORE OUT-OF-SCOPE FINDINGS (Jeff, 2026-06-05) — ADD TO CLOSE ROUTING (join findings 1-5; piano-roll
UI layout, not grid/snap scope):**
6. **Piano Roll — fold the Tools BUTTON into the Tools MENU; drop the duplicate tool-selectors.**
   - The menu-bar "Tools" entry (PianoRoll.cpp:4036-4050, menu idx 1) lists Draw/Paint/Delete/Mute/Slice/
     Select/Zoom == the `mToolBtns[]` toolbar buttons (PianoRoll.cpp:2658) -> REMOVE those from the menu
     (the buttons already cover them, right there on the bar).
   - Move the Tools BUTTON's tools (the wrench `mWrenchBtn`, PianoRoll.cpp:2600 -> `mGrid->showToolsMenu()`:
     Quantize / Strum / Glue / Chop / Randomize / Articulate...) INTO the menu-bar "Tools" entry, then
     REMOVE the wrench button.
   - Net: menu-bar Tools = the wrench's advanced tools; the per-tool selectors leave the menu; the wrench
     button is gone.
7. **Piano Roll — snap button -> Builder-style dropdown + reposition; kit button to the bar's right end.**
   - Replace the snap TOGGLE button with a BuilderPage-style snap DROPDOWN (the `mSnapCombo` 11-label combo)
     on BOTH toolbars: the engine-roll `PianoRollContainer` toolbar (`mMagnetBtn`, PianoRoll.cpp:2608) AND
     the Drum Kit `DrumKitContainer` toolbar (DrumKitGrid.*).
   - Position the snap dropdown where the kit button currently sits.  The kit button exists ONLY on the Drum
     Kit roll's toolbar; on that toolbar, move it to the RIGHT END of the bar.
   - (The snap -> dropdown change applies to BOTH toolbars per Jeff; only the Drum Kit toolbar has a kit
     button to relocate.)

**Effects-docket correction (Opto):** the earlier line "Opto DSP is correct" is SUPERSEDED -- Jeff's
screenshots show Opto humping like Vintage (3 dB GR mid-knob, none above/below).  FET + Opto panels expose
NO knee control, and all modes share the same `computeGainDb` knee/ratio computer, so the Vintage-knee
GR-hump (CompressorDSP.cpp:245-254) is the prime shared suspect for all three (Vintage / FET / Opto).
Docket items (1) Vintage-knee hump + (2) FET inverted Input->threshold map stand; (3) "Opto verify" folds
into (1).  Exact wiring (why FET/Opto exhibit the Vintage-style hump) TBD in the dedicated
effects-correctness batch.

## 2026-06-05 (cont.) — dead `snapDenominator` removed IN-BATCH + close routing finalized

**`snapDenominator` chain REMOVED in-batch** (was on the route-to-later list as "remove the dead
`PianoRollData.snapDenominator`" -- Jeff overruled: clean your OWN batch's dead code in-batch, do NOT
defer it).  QA-Ee Stage 3 moved piano-roll + drum-kit snap to the GLOBAL `Unified_PianoRollSnapDiv` (read
live via `onGetSnapDiv()`), which orphaned the per-roll `PianoRollData.snapDenominator` field + the
`mSnapDenom` locals it fed (set, never read).  The Stage-3b note "left as-is, out of scope" was wrong --
it's THIS batch's garbage, so it's gone now:
- `PatternManager.h`: dropped the `PianoRollData::snapDenominator` field.  `PatternManager.cpp`: dropped
  its save + load serdes (old projects' `<snapDenominator>` property is now harmlessly ignored on load --
  load-compatible, no migration).
- `PianoRoll.cpp/.h` + `DrumKitGrid.cpp/.h`: dropped both `mSnapDenom` members per file, the
  `setScrollState(..., int snapDenom)` param (def + decl + the single caller each), the `setData`/reader
  assignment from the field, and the now-empty drum-kit `if (mPM)` reader block.  `setSnapDenomAndQuantize`
  (the Edit-menu Quantize submenu) is LIVE + UNTOUCHED -- Stage 3 already rewired it to the global snap; it
  never referenced `snapDenominator`/`mSnapDenom`.
- 6 files, +8/-20 (pure removal).  Zero behavior change (all removed state was set-but-never-read).
  Verified by Jeff (Debug + Release 2026-06-05): snap still works in piano roll + drum kit, Edit>Quantize
  still works, a pre-QA-Ee project loads intact.  Grep confirms zero `snapDenominator` / `mSnapDenom` /
  `snapDenom`-param remain.

**CLOSE ROUTING FINALIZED (Jeff, 2026-06-05).**  Three NEW batches, IN THIS ORDER, immediately after QA-Ee
(silly-names are mine; formalized at close in Main Plan §5 dockets + §6 sequencing + §9 Forks rationale):
1. **QA-EffectsReview** (1st) -- effects-correctness docket (all pre-QA / pedal-board-era origin):
   (a) Compressor Vintage-knee GR-hump (`CompressorDSP.cpp:245-254`) -- narrow-band reduction (finding #4);
   FET + Opto inherit it via the shared `computeGainDb` path.  (b) FET inverted Input->threshold knob map
   (`EffectEditorPanels.cpp:402-407`).  (c) Flanger/Delay/Phaser one-way un-sync (`FlangerDSP.cpp:49-66` +
   Delay + Phaser) -- sync won't turn off (finding #5).  (d) Audio/Vox/Inst multi-call-per-block hazard for
   stateful effects (delicate -- regression-test vs live + playback).
2. **QA-CutSelfReview** (2nd) -- "Cut Self" broken on Layers/Bass (works on the drum-kit grid) (finding #2).
3. **QA-UICleanup** (3rd) -- piano-roll + misc UI:
   - Quit save-prompt dialog draggable -> fixed centered modal (finding #1).
   - Layers don't auto-name from the loaded patch (finding #3).
   - Fold the Piano-Roll Tools BUTTON's advanced tools into the menu-bar "Tools" entry + drop the menu's
     duplicate per-tool selectors + remove the wrench button (finding #6).
   - Snap button -> Builder-style snap DROPDOWN on BOTH the engine-roll + Drum-Kit toolbars; kit button ->
     right end of the Drum-Kit toolbar (finding #7).
   - **NEW (Jeff 2026-06-05) "Quantize Settings":** move the Edit-menu Quantize submenu (1/4..1/32) into the
     Tools menu, rename it "Quantize Settings", and make it the quantize-RESOLUTION SETTING -- it configures
     how quantize works; it no longer quantizes on the spot.
   - **NEW (Jeff 2026-06-05) Tools-menu "Quantize" action:** the Quantize tool moved over from the wrench/
     Tools button (finding #6) quantizes to whatever "Quantize Settings" is set to.

(Finding #N = the "Out-of-scope findings" list earlier in this doc: #1 quit-prompt, #2 Cut Self, #3 Layers
auto-name, #4 compressor, #5 flanger, #6 Tools->menu, #7 snap dropdown.  The old "remove dead
snapDenominator" item is DONE in-batch above -- no longer routed.)

## 2026-06-05 (cont.) — /review-batch follow-up (1 fix + dead-code sweep) + 2 more routing items

**/review-batch QA-Ee: READY-TO-COMMIT, zero blockers.**  Validated all four stages against plan +
conventions (tick math integer-exact, migration sound, audio-thread atomics correct, no ASCII / casing /
aggregate-init violations).  Surfaced 1 NEEDS-FIX + dead-code NITs + 1 default-confirm -- all resolved
in-batch (Jeff 2026-06-05):

**FIX (NEEDS-FIX) -- Line snap now reaches the finest visible grid line on the piano roll + drum kit.**  The
grid drew rungs down to 5px (`kMinLineSpacing`) but Line snap (`dynamicSnapTicks`) only targeted >= 12px
(`kMinLinePx`), so at zooms where a rung sat 5-12px you saw a finer line that Line snap skipped (the inverse
of the "snap to open space" bug Stage 3 killed).  Builder was already consistent (12px on both sides -- its
FL 16-cell cap).  Fix (Jeff: "fix to 5px"): new shared `kMinGridLinePx = 5` (VibesynthConstants.h);
`dynamicSnapTicks(pixelsPerBar, minLinePx = kMinLinePx)` parameterized; the piano-roll + drum-kit snap calls
pass `kMinGridLinePx`, and their grid `kMinLineSpacing` now sources from it too -- so grid + Line snap lock
to the same finest line (down to 1/64).  Builder's call (`dynamicSnapTicks((double) mPPBar)`) takes the
default 12 -- unchanged.

**DEAD-CODE SWEEP (NITs, cleaned in-batch per Jeff's standing directive).**  More QA-Ee orphans removed:
`mSnapEnabled` / `setSnapEnabled` (snap on/off is now the global param = 0; never read) on both grids +
containers; the drum-kit `onVZoom` / `applyVZoom` vertical-zoom path (dead after Stage 2's 16-fixed-rows
change pulled Alt-scroll + menu items 53/54); and the now-vestigial `mRowHScale` (its only writer was
`applyVZoom`; `rowH` is int so the x1.0 collapsed to `rowH`).  PianoRoll's `onVZoom`/`applyVZoom` + Harmless'
own `mSnapEnabled` are LIVE -- untouched.  Zero behavior change.

**DEFAULT (confirm) -- PianoRoll snap default = Line (idx 1), ACCEPTED.**  Shipped as Line, not the SC-def
1/2 Step (1/32); old projects reopen at Line, not their prior 1/32.  Jeff 2026-06-05: "I don't care about
this at all as nothing has released" -> Line stays; recorded as accepted (the SC-def deviation gets a §9
note, no behavior change).

Fix + sweep verified by Jeff in Debug + Release 2026-06-05 (Line snap reaches the fine lines on both
editors; Builder still caps at 16 cells/bar; snap on/off + piano-roll vertical zoom still work).

**TWO MORE ROUTING ITEMS (Jeff, 2026-06-05):**
- **-> QA-UICleanup:** the piano-roll transpose menu -- (i) the items render non-ASCII arrow glyphs (the
  boxes); (ii) the two "Transpose Octave" entries show the wrong shortcut vs the Key Binds window (which
  lists Transpose Octave = Ctrl + Up/Down); (iii) move ALL FOUR transpose options (Up / Down / Up Octave /
  Down Octave) from the Edit menu into the Tools menu.
- **-> NEW batch QA-Chords (4th / last of the new batches, after QA-UICleanup):** the Chord Stamp tool --
  (a) a stamped chord can't be stretched / resized (places as-is only); (b) dual-mode Root/Scale/Snap-to-
  Scale behavior (Jeff's spec):
    - Mode 1 (Snap-to-Scale OFF): chord dropdown (Major/Minor/Sus2/...) must NOT use static hardcoded
      semitone intervals -- it reads the globally active Root + Scale and, on click, generates a
      context-aware chord that natively fits the selected scale degrees relative to the clicked note.
    - Mode 2 (Snap-to-Scale ON): strict scale compliance; if interval snapping collides (two chord notes
      forced onto the same MIDI note number), run an Octave Resolution Pass -- detect the duplicate and
      shift it up to the next valid scale degree in the next octave up, preserving harmonic thickness
      (not stacking, not deleting).
    - Deliverable for that batch: clean JUCE-compatible C++ data structures + algorithm separating the two
      modes, the MIDI-note generation array for the 96 PPQ grid, and the octave-collision resolver.

**UPDATED new-batch order: QA-EffectsReview -> QA-CutSelfReview -> QA-UICleanup -> QA-Chords** (all
immediately after QA-Ee).  Formalized in the close docs (§5 dockets + §6 sequencing + §9 Forks).
