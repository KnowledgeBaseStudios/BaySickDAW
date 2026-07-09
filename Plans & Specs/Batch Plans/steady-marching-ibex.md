# QA-TempoMap — Sample-Indexed Stepped Tempo Map + Ruler Tempo Markers — Plan (steady-marching-ibex)

> **Canonical path (post-approval):** `Plans & Specs/Batch Plans/steady-marching-ibex.md` (mirrored
> at G1 group approval; home-dir copy deleted). **For execution:** BULK-RUN mode — no per-task verify
> pauses; Verify scripts author into Master Test Plan §B; Work Log entry drafted + HELD; one source
> commit. G1's highest-risk batch (hot-path transport).

## Context

QA-Ed shipped the int64-sample clock with a SINGLE re-basing tempo anchor: `beat = anchorBeat +
(sample - anchorSample) * bpm/(60*sr)`, re-published on every BPM change. One tempo at a time;
tempo automation today = coarse 30 Hz message-thread `setBPM` re-bases (drift-prone). This batch
replaces the single anchor with a **stepped, sample-indexed tempo timeline** so multiple tempo
changes sit at exact sample positions ("140 until bar 65, then 70"), authored as **ruler tempo
markers** riding the shipped D-2 marker substrate. Ramps stay the automation path's job (locked 11);
markers are steps.

- Risk: **high** — the derive/advance/schedule core under MT. Mitigations: the timeline preserves
  the anchor's read discipline (audio thread only ever reads), immutable-publish pattern, and the
  QA-TransportDisplay readout (lands first) as the measuring instrument.
- Effort: ~8-12h. Dependencies: QA-Ed + QA-Ee (closed); QA-TransportDisplay (readout aids verify).
- **Bucket:** Cross-cutting Infrastructure.

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| 11 | Ruler flags = STEPPED tempo-change points; RAMPS via the existing global_tempo automation, not the map | Jeff, marathon + follow-up. Keeps the map a clean lookup list. |
| B | Last-writer-wins precedence | Jeff, G1 round. Automation writes override while playing; markers re-assert at their boundaries. Falls out of the timeline design (a live write appends a segment at the current sample; later marker entries still follow). |
| E | BPM field edits the BASE tempo; displays live effective tempo | Jeff, G1 round. Field re-sync already polls `getBPM()` when unfocused (GlobalTransportBar.cpp:750-757) — display half is free once `getBPM()` returns effective-at-playhead. |
| SC-1 heritage | This is QA-Ed's explicitly deferred map | §5/§9 forty-eighth Forks entry. |

Derived (stated in presentation, not new calls): markers are SONG-domain — in pattern mode the base
tempo applies (markers/automation are song-positioned; a looping pattern has no song position);
marker bar→sample conversion uses the same uniform 4-beats/bar math playback uses everywhere
(song-level TS changes are decorative today — composing tempo with real TS playback is QA-TempoMap's
successor problem, not smuggled in here).

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open.

## Files to modify

### Task 1 — the timeline engine
- `Source/Standalone/StandaloneApp.h:56-89` (anchor fields → timeline publication + base-tempo API)
- `Source/Standalone/StandaloneApp.cpp:142-171` (`deriveBeat`/`publishAnchor` → timeline lookup /
  timeline publish), `:173-210` (`advanceBlock` loop-wrap via map inverse), `:212-244`
  (`start`/`reset`/`seekTo`/`setBPM` → timeline rebuilds), `:246-256` (`getPosition` per-block bpm)

### Task 2 — marker data + persistence
- `Source/PatternManager.h:243-259` (new `TempoChange { int bar; double bpm; }` beside
  `TimeSigChange`), `:475-490` (CRUD API), `.cpp:341-413` (impls), `.cpp:1108-1130` + `:1506-1536`
  (`<TempoChanges>` XML save/load beside `<TimeSigChanges>`)

### Task 3 — ruler UI
- `Source/Standalone/BuilderPage.cpp:1697-1732` (tempo glyph paint), `:2895-2935` (right-click menu
  items), new prompt beside `:2973`, `:5000-5006` (keyboard add beside Alt+T), `:2863-2889` (tooltip)

### Task 4 — writers + consumers rewire
- `Source/Standalone/StandaloneEditor.cpp:757-764` (tempo field/tap → base tempo), `:604-616`
  (global_tempo applicator → live write), `:10804-10821` (project-load sync)
- `Source/PluginProcessor.cpp:1375-1420` (scheduler sub-spans), `:598-604` + `:975-979` (clip ratio
  reads per-span bpm), `:1954-1956` (BlockContext), `:2154-2187` (metronome), `:2009-2019` (MIDI rec)

## Tasks

### Task 1 — TempoTimeline engine (replaces the single anchor)

- [ ] `struct TempoSegment { juce::int64 startSample; double bpm; double startBeat; }` — immutable
      sorted `std::vector<TempoSegment>` with precomputed cumulative `startBeat` (the integral), so:
      `deriveBeat(sample)` = binary-search segment + linear within; `beatToSample(beat)` = inverse
      via the same prefix. Published as `std::shared_ptr<const TempoTimeline>` via atomic
      exchange (HarmonicEngine/AudioClipSnapshot pattern — the map's variable length makes the
      seqlock the wrong tool); old timelines retire on the message thread (RCU precedent
      PluginProcessor.cpp:2640-2645). Audio thread acquire-loads ONCE per block.
- [ ] Rebuild-on-write (all message-thread, preserving the single-writer discipline):
      **base edit** (BPM field/tap/load) rewrites segment 0 and every pre-first-marker span;
      **marker CRUD** rebuilds from base + marker list; **live automation write at sample S**
      truncates at S and appends `{S, newBpm}` — later marker segments remain ⇒ last-writer-wins (B)
      falls out structurally. Continuity invariant: every rebuild preserves `deriveBeat(now)`
      (pivot at the playhead, exactly like today's re-base).
- [ ] `advanceBlock` (StandaloneApp.cpp:173-210): loop-wrap bounds via `beatToSample` instead of the
      single-slope math; comment block :184-191 updated (Rule 6 keeper — the why changes).
- [ ] `getPosition()`: bpm = segment-at-`mSamplePos` bpm (block-entry value; sub-block handling is
      Task 4); `getBPM()` = effective-at-playhead (feeds the field display, E).
- [ ] Pattern mode: timeline collapses to a single base segment (markers song-domain).

### Task 2 — TempoChange data model + persistence

- [ ] `TempoChange { int bar{0}; double bpm{120.0}; }`, sorted vector, same-bar add replaces
      (mirror `addTimeSigChange` PatternManager.cpp:394-397); CRUD + `findTempoChangeNearBar`.
- [ ] XML: `<TempoChanges><Tempo bar=".." bpm=".."/></TempoChanges>` beside the D-2 blocks; load
      re-sorts; project-load pushes base + markers into the timeline
      (StandaloneEditor.cpp:10804-10821 extension).
- [ ] Undoable via the existing arrangement edit bracket the ruler prompts use.

### Task 3 — ruler tempo flags (D-2 substrate)

- [ ] Paint: tempo flag glyph (distinct from yellow label-pennant + blue TS pill — amber flag with
      the BPM number) in the ruler loop (BuilderPage.cpp:1700-1731 pattern, `barToX`).
- [ ] Right-click ruler menu gains "Add Tempo Change...", and near an existing flag
      "Edit Tempo Change..." / "Delete Tempo Change" (dispatch beside :2927-2933); numeric prompt
      clamped 20-300 (the applicator's kTempoMin/MaxBpm range).
- [ ] Tooltip shows "N BPM from bar M" (ASCII); marker edits rebuild the timeline immediately
      (audible live per last-writer-wins).

### Task 4 — writers + consumers on the timeline

- [ ] Tempo field + tap (StandaloneEditor.cpp:757-764) → `setBaseTempo`; field display already
      live via `getBPM()` poll (E).
- [ ] global_tempo applicator (:604-616) → live write (truncate-and-append). PatternManager
      `mGlobalTempo` continues to store the BASE (save/load unchanged semantics).
- [ ] Scheduler sample accuracy: in `processBlock`, when the block straddles a segment boundary,
      split the scheduling/ratio work into sub-spans at the boundary sample (the loop-wrap
      mid-block split is the in-file precedent) — notes on and clip ratios flip EXACTLY at the
      marker sample. Consumers taking one bpm per block (BlockContext :1954-1956, metronome,
      MIDI recorder, strip tasks) get the span's bpm; per-span is v1-sufficient for all of them.
- [ ] Clip ratio (:598-604 / :975-979): `ctx.bpm` per sub-span so Stretch/Resample follow tempo
      steps sample-accurately (composes with QA-Ec — that batch runs BEFORE this one is verified
      by ear at the boundary, but lands after it in §6 order... §6 order: TempoMap is third, Ec
      fifth — Ec builds on per-span bpm being available; coordinate the seam signature here).
- [ ] Rule 6 pass on all touched regions (the anchor doc-comment rewrite is the big one).

### Task 5 — batch close (bulk-run shape)

- [ ] Author Master Test Plan §B "QA-TempoMap" from the Verify scripts (`blocks:` = batch commit).
- [ ] Draft + HOLD Work Log entry in `Running Notes/steady-marching-ibex.md`; code-complete
      running-notes entry (include the derived pattern-mode + bar-math notes).
- [ ] One source commit (Rule 9): message + full status → approval → commit.

## Verify scripts (→ Master Test Plan §B; Debug first, then Release — G1 ear-check batch)

1. Ruler right-click at bar 5 → "Add Tempo Change" 90 (project at 140): play from bar 1 —
   metronome + pattern audibly slow at EXACTLY bar 5's downbeat; the position readout's bar rolls
   over in lockstep (no drift vs the click).
2. Readout cross-check: at the marker the beat display stays continuous (no jump); time display
   slope changes.
3. Multiple markers (140 → 90 → 160) + loop the song across both boundaries → transitions stay
   sample-tight every loop pass.
4. BPM field: shows 140 before bar 5, live-flips to 90 as the playhead crosses (E display); typing
   120 while playing past bar 5 changes the BASE — pre-marker section now 120, marker section still
   90 (E edit).
5. Tempo automation lane over a marker span: automation values win while its points play; past the
   automation block the next marker re-asserts (B last-writer-wins).
6. Stretch-mode audio clip spanning a marker → clip's rate follows the step at the boundary
   (pitch locked); Resample-mode clip varispeeds at the boundary (ear-check).
7. Pattern mode: markers ignored; field edits take effect immediately (base).
8. Save → reload: markers + base round-trip; playback identical.
9. Seek/loop/stop-start around boundaries: `getCurrentBeat()` continuous, no stuck transport, no
   Debug jasserts.
10. MT on: stress arrangement across markers — no dropouts vs pre-batch baseline.

## Routing notes (Rule 3 application during execution)

Real-TS-aware playback (song-level TS currently decorative; hardcoded 4 beats/bar at
PluginProcessor.cpp:1707, BuilderPage.cpp:5738, PluginProcessor.cpp:2636-2638) is adjacent but NOT
this batch — if findings force a call, log + route (likely Future State or a dedicated batch, Jeff's
slot call). The QA-Ec ratio seam coordination note lives in both plans.

## Carry-Forward Reference touch points

- §1 transport primitives + §3 QA-Ed/QA-Ee decisions before Task 1. The 2026-07-08 tempo surface
  map (running-notes seed) is the authoritative line-ref index; §5-cited legacy refs are stale.
