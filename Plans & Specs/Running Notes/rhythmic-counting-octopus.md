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
