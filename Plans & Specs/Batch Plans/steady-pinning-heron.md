# QA-G — Timeline Geometry + Time-Signature System — Plan (steady-pinning-heron)

> **Canonical path (post-approval):** `Plans & Specs/Batch Plans/steady-pinning-heron.md`.
> Paired running notes: `Running Notes/steady-pinning-heron.md`.
> **Execution mode: bulk run** (swift-stampeding-caribou R1-R5): no per-task verify pauses;
> verify scenarios author into Master Test Plan §B at code-complete; Work Log entry drafted +
> HELD; ONE source commit at batch close; mid-run spec calls ASKED, always.

## Context

First batch of G3. Original §5 scope (BUILD-01/02/03) shrank at absorption (rows already 50,
zoom already float) and grew at the marathon (+track right-click set) and the G3 docket
(+note-preview fix per docket #7 "whichever runs first"; +the full time-signature system per
docket #14/A/B/B1/B2 — the metronome/TS regression routed from QA-Fe2 close, §9 fifty-ninth;
+pattern-block SLICE rework per the 2026-07-17 follow-up docket, pick (a) — the second half
of the ghost-notes cluster, an unrouted capture miss). Slice code truth: the tool exists but
cuts only at whole INT bars (a 1-bar block has no legal cut → clicks no-op) and blocks have
NO content-offset field, so a cut piece RESTARTS its pattern instead of continuing — notes
cannot "stay in place."
Risk: medium (TS system + playback scheduling touch the audio-side math; rest is UI geometry).
Effort: ~16-25h. Dependencies: none (QA-Ed/QA-TempoMap sample clock + stepped map are in).

## Spec calls already locked (G3 docket, 2026-07-17, all Jeff's)

| ID | Decision |
|----|----------|
| #7 | Note-preview fix (Jeff's G1 spec: mini notes at TRUE musical positions) lands in QA-G (first-executed batch) |
| #8a | Track groups + colors SAVE with the project |
| Marathon 5 | Track right-click adds: Insert track above / Group with above / Remove from group / Color group; groups VISUAL-ONLY for V1; keep 50 rows |
| #14 | Grid TS markers become fully functional and the SOLE driver of played time signatures (playback, recording, metronome); marker popup stays free TYPE-IN; metronome does the math for any entered signature |
| A=a | The TS work lives in QA-G |
| B | Pattern keeps its TS popup (same type-in popup) but it no longer drives playback directly; placing a block of a TS-bearing pattern AUTO-SPAWNS a linked grid marker at the block's start bar that MOVES with the block and DELETES with it; pattern-TS edits update all linked markers; manual markers unrestricted; manual beats auto on same-bar ties; linked markers get a visual tell |
| B1=a | Auto-marker changes the signature at block start; it PERSISTS until the next marker (no end-of-block restore) |
| B2=a | Moving/deleting a linked auto-marker UNLINKS it into a normal manual marker |
| Slice=(a) 2026-07-17 | Pattern-block slice folds into QA-G, after the note-preview fix: cuts at SNAP resolution (piano-roll slice parity), blocks gain a content-offset so sliced pieces keep their notes in place and play their true slice of the pattern; copy/paste preserves it; the pattern itself never changes (Jeff's isolate-and-repeat use case) |

**!! Option-semantics change (loud paper trail, per the standing rule):** the per-pattern
time-signature setting STOPS driving playback/metronome directly (grid markers become the only
played source). The pattern popup is NOT removed — it is re-purposed as the auto-marker link
source. Recorded here at plan write; running-notes line lands when the code does.

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open — all locked in the 2026-07-17 G3 docket rounds (items 1-18, A-D,
B1/B2/C follow-ups).

## Files to modify

- `Source/Standalone/BuilderPage.cpp/.h` — ruler paint `drawRuler` (grid-local y=0,
  :1920-1924), grid sizing `ArrangementGrid::resized` (:5726), `barToX` (:1599-1602), block
  width truncation (:2737 + siblings :1671/:1681/:1771/:2794/:2833/:2964/:5475), toolbar
  onZoom anchor (:6218-6228), `showTrackContextMenu` (:5899-5958), `TrackHeaderPanel::paint`
  (:5776-5829), `drawMidiShading` (:2149-2190), TS-marker surfaces (locate in Task 5)
- `Source/PatternManager.h/.cpp` — per-row group/color arrays + save/load; pattern TS field
  semantics; marker link-ownership field; `ArrangementBlock` content-offset field (ticks,
  default 0, serialized) for slice
- `Source/PluginProcessor.cpp` — metronome TS math (MetroDSP + TempoMap spans), playback
  beats-per-bar consumption; pattern-window scheduling honors block content-offset
  (`scheduleRollWindows` span derivation, :2219-2231)
- `Source/Standalone/BuilderPage.cpp` — Slice tool (:4674-4693: int-bar cut → snap-resolution
  via `onGetSnapDiv`; right piece offset += cut distance); copy/paste/duplicate paths carry
  the offset
- `Source/Standalone/StandaloneEditor.cpp` — pattern TS popup site; marker popup site

## Tasks

### Task 1 — Ruler pin (BUILD-02)
- [ ] Extract the timeline ruler from the vertically-scrolling grid surface so it stays pinned
      at the top during vertical scroll (the TrackHeaderPanel corner is already pinned — close
      the asymmetry). Preserve every `kRulerH`-anchored mapping (`rowToY`/`yToRow`/`drawGrid`/
      `blockAtPos`/playhead) or reroute them consistently.
- [ ] Horizontal behavior unchanged (ruler + grid share `barToX`).
- [ ] Tell Jeff: build, confirm clean. Running-notes checkpoint.

### Task 2 — Zoom alignment (BUILD-03)
- [ ] Block right edges: width = `barToX(start+len) − barToX(start)` (kill the independent
      truncation; keep the deliberate 1px gutter) so edges land on their grid lines at any zoom.
- [ ] Toolbar +/− zoom: anchor math all-float (kill the `(int)` centre truncation creep).
- [ ] Preserve stretch badge / follow dot / pre-roll arrow corner geometry (they derive from
      block rects — verify visually unchanged).
- [ ] Build-confirm gate + checkpoint.

### Task 3 — Track right-click additions (marathon 5, #8a)
- [ ] Extend `showTrackContextMenu`: Insert track above (shift rows + blocks down, Move Up/Down
      precedent), Group with above, Remove from group, Color group (color picker).
- [ ] New per-row group-id + color state in PatternManager alongside `mRowMuted`/`mRowSoloed`;
      serialized with the project (#8a). Visual signifier on the header rows (group band +
      color tint). Visual-only — no linked behavior.
- [ ] Build-confirm gate + checkpoint.

### Task 4 — Pattern-block note preview at true musical positions (Jeff's G1 spec)
- [ ] Rework `drawMidiShading`: note x/w derive from bar-normalized positions (`mPPBar`),
      NOT fraction-of-pattern stretched over block width; 1 bar of notes fills exactly 1 bar
      of block; looped/longer blocks TILE the pattern per cycle; clip at block bounds;
      non-4/4 patterns stop skewing.
- [ ] Build-confirm gate + checkpoint.

### Task 5 — Pattern-block slice rework (content-offset model; after Task 4 by design)
- [ ] `ArrangementBlock` gains a content-offset (ticks; default 0; serialized; full-snapshot
      undo covers it automatically).
- [ ] Playback honors it: the block plays its pattern starting at the offset; looping/tiling
      continues from there (window mapping = pattern-relative position + offset).
- [ ] Slice tool cuts at SNAP resolution (Builder snap div, piano-roll parity — no more
      int-bar truncation; short blocks become sliceable): left piece keeps its offset, right
      piece offset advances by the cut distance; lengths in ticks.
- [ ] Note display (Task 4's bar-normalized mapping) draws from the offset — a sliced piece
      shows exactly the notes it plays, at true positions.
- [ ] Copy/paste/duplicate preserve the offset; resize of a sliced piece extends/trims its
      window (tiling continues from the offset).
- [ ] Audio + automation blocks: same snap-resolution cut with correct content continuation
      (audio rides its existing content-base machinery — verify, fix if the right piece
      restarts; automation splits at the cut point).
- [ ] Build-confirm gate + checkpoint.

### Task 6 — Time-signature system (#14, A, B, B1a, B2a)
- [ ] Map current state first (bounded recon, results to running notes): TS marker storage /
      popup / any current consumption; pattern TS field + popup; metronome beat/accent
      derivation (MetroDSP + TempoMap).
- [ ] Grid TS markers drive EVERYTHING played: bar/beat derivation for playback + recording +
      metronome accents/clicks reads the marker timeline (stepped, like the tempo map);
      type-in popup unchanged; math correct for any entered signature (incl. odd meters).
- [ ] Pattern TS popup kept, re-semanticized per B: on block placement auto-spawn a linked
      marker at block start (persists until next marker per B1a); marker moves/deletes with
      the block; pattern-TS edit updates all linked markers; same-bar manual wins; linked
      markers drawn with a visual tell; touching a linked marker unlinks it (B2a).
- [ ] The loud paper-trail line for the semantics change lands in running notes with this task.
- [ ] Ruler bar numbering / grid bar lines reflect signature changes (bars resize at markers).
- [ ] Build-confirm gate + checkpoint.

### Task 7 — Close (bulk run)
- [ ] Author Master Test Plan §B section (scenarios below; `blocks:` = this batch's commit).
- [ ] Draft + HOLD Work Log entry in running notes; code-complete entry.
- [ ] ONE commit: message + full git status surfaced -> Jeff approves.

## Verification (§B-destined scenarios)

1. Scroll the track list down — ruler stays pinned; row labels + grid rows stay aligned.
2. Deep zoom in/out (wheel + toolbar) — block edges kiss their bar lines; toolbar zoom does
   not creep; badges/dots stay cornered.
3. Insert track above a populated row — blocks shift correctly; group two rows + color them —
   signifier draws; save/reopen — groups + colors restored (#8a).
4. 1-bar pattern on a 4-bar block — notes tile 4x at true positions; 2-bar pattern on 1-bar
   block — first bar only; non-4/4 pattern — no skew.
5. Type 7/8 into a grid marker mid-song — playback bars + metronome accents follow from that
   bar; readout/ruler bars resize; metronome correct in 3/4, 7/8, 5/4.
6. Pattern with TS set → drag block to grid — linked marker appears at block start with visual
   tell; move block — marker follows; delete block — marker gone; edit pattern TS — marker
   updates; drag the marker itself — it unlinks and behaves manual; manual marker on the same
   bar wins.
7. Metronome regression re-run (Jeff's #14 repro basis): signature set via marker (not pattern
   dropdown) — metronome correct in song mode.
8. Jeff's slice use case: 1-bar block, 3 one-beat notes + 2 half-beat notes in the last beat —
   slice at the last beat (snap resolution works on a 1-bar block), right piece shows + plays
   ONLY the 2 half-beat notes at their true timing; copy/paste it elsewhere — identical there;
   the pattern itself untouched (roll unchanged, other blocks of the same pattern unaffected).
9. Slice a looped 4-bar block of a 1-bar pattern mid-loop — both pieces play/display the
   correct continuation; undo restores the un-sliced block; slice an audio clip off-bar —
   right piece continues the file, no restart.

## Routing notes (Rule 3)

Real bugs on touched surfaces fix in-batch. TS recon surprises that reshape Task 5 get ASKED
before coding past them. Findings elsewhere → running notes, route at section pass.

## Carry-Forward Reference touch points

- Task 1/2: none (post-dates carry-forward; scout map in group-open notes is the reference).
- Task 5: QA-TempoMap §B.3 + G1 carry-over (TempoMap spans, metronome PDC deferral from QA-Fe2
  — do not regress the click-deferral work: `MetroDSP::countInDelaySamp` + delayed-clock grid).

## Carry-Over — 2026-07-17 (QA-G close; session end)

- **Completed:** QA-G in full — Tasks 1-6 + owner additions (row alignment + external
  H-scrollbar rework, 500 tracks superseding marathon-5's keep-50, Split by Player
  Engine + Insert-Above max-prompt retrofit) — ONE batch commit `928eca1d`, every task
  build-confirmed clean. §B.13 authored (20 scenarios); Work Log entry drafted + HELD in
  the running notes; 7 in-batch bug fixes + known seams all recorded there.
- **In-flight:** nothing in code. TWO expected doc stragglers in the tree for QA-H's
  commit (B.12 precedent): §B.13 `blocks:` hash backfill + this carry-over block.
- **Assumptions changed:** see the running notes Task-6 recon entry (autoDerive/
  barStartBeat were orphaned C.5b machinery; metronome accent was pattern-driven;
  count-in was hardcoded 4 beats; removePattern never re-indexed; File > New leaked
  markers; renames never persisted). Marathon-5 "keep 50 rows" superseded by owner call
  (500). Silent Insert-Above edge superseded by the max prompt (owner rev).
- **Resume action (NEW session):** paste the QA-H prompt (handed to Jeff at this
  session's close) — /standup -> Main Plan §0 -> caribou G3 sections -> ghostly-riffing-
  moth.md IN FULL -> confirm `928eca1d` at HEAD (+ the two stragglers dirty) -> QA-H
  Task 1.
- **Work-Log entry needed:** none new — QA-G's is drafted + HELD in
  `Running Notes/steady-pinning-heron.md` (applies at §B.13 section pass).
