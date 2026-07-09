# Running Notes — QA-TempoMap (steady-marching-ibex)

> Append-only mid-batch log. New entry at every checkpoint. Under the BULK RUN this batch's Work
> Log entry is drafted at code-complete, HELD here under `## Held Work Log entry (apply at section
> pass)`, and applied only when its Master Test Plan §B section passes.

Pair file: [`Batch Plans/steady-marching-ibex.md`](../Batch Plans/steady-marching-ibex.md).
Conventions: Main Plan §0 + [`Batch Plans/swift-stampeding-caribou.md`](../Batch Plans/swift-stampeding-caribou.md).

## 2026-07-08 — G1 group open — plan approved + surface map (source-verified)

Locked: 11 (stepped markers; ramps stay automation's job), B (last-writer-wins), E (field edits
base / displays live effective). Derived: markers song-mode only; marker bar math = uniform
4 beats/bar (song TS decorative). Verified refs:

**Anchor (being replaced):** `StandaloneApp.h:56-89` (transport atomics `:63-68`; anchor 4-tuple
`:82-85`; invariant doc `:73-81`); `deriveBeat` seqlock read `StandaloneApp.cpp:142-158`;
`publishAnchor` write `:160-171`; `setBPM` re-base `:228-234`; `start` `:212-218`; `reset`
`:220-227`; `seekTo` `:235-244`; `getPosition` `:246-256`; `advanceBlock` `:173-210` — sole
`mSamplePos` writer, loop wrap measured from the anchor line (`:181-208`, why-comment `:184-191`),
NEVER writes the anchor (audio thread reads only — preserve).

**Writers (all message-thread):** field/tap `StandaloneEditor.cpp:757-764` (origins
`GlobalTransportBar.cpp:497/501/787`); global_tempo applicator `:604-616` (range 20-300
`:602-603`) — NOT an APVTS param: the audio-thread automation pass skips it
(`PluginProcessor.cpp:1704-1772` applies only via apvts `:1770-1771`); it runs on the editor's
30 Hz timer (`:1381` start, body `:2903-2941`, beat source `:2893`) = the coarse path the map
replaces; project load `:10804-10821` (setBPM `:10814`, called `:10918`). Base store:
`PatternManager` `mGlobalTempo` (`.h:623-624`, `:638`; save `.cpp:837`, load `:1167`) — keeps
meaning BASE tempo.

**Consumers:** one `PositionInfo` per block `PluginProcessor.cpp:1276-1279`; PR scheduler
`:1375-1420` (beats-per-sample `:1375-1378`, loop-seam `:1390-1400`, straddle `:1410-1420`);
automation beat→bar `:1708` (`kBeatsPerBar = 4.0` `:1707`); strip tasks `:1896-1898`/`:1916-1917`;
`BlockContext.bpm` `:1954-1956`; MIDI recorder `:2009-2019`; metronome `:2154-2187` (accents per
pattern tsNum `:2168-2169`); clip ratio `:598-599`/`:975-979` (PV gate `:998-1001`, set `:1006`);
rebuild captures `originalBPM` `:2688` (kBPB=4.0 "decorative TS" comment `:2636-2638`). UI:
`BuilderPage.cpp:5732-5744` (`/4.0` at `:5738`/`:5743`); pages `LayersPage.cpp:266` /
`BassPage.cpp:254` / `DrumPage.cpp:398` / `PianoRollPage.cpp:56`; BPM field resync
`GlobalTransportBar.cpp:750-757` (E's display half rides this); loop/TS push
`GlobalTransportBar.cpp:619-631` + `StandaloneEditor.cpp:782-866`.

**D-2 substrate:** structs `PatternManager.h:248-259`; vectors `:635-636`; API `:475-490`; impls
`.cpp:341-413` (add-sort `:344-346`, near-bar `:368-379`, TS same-bar replace `:394-397`); XML
save `:1108-1130` / load `:1506-1536`; ruler paint `BuilderPage.cpp:1697-1732` (`barToX`
`:1703`/`:1719`); right-click menu `:2895-2935` (dispatch `:2927-2933`); prompts `:2937-3015`;
Alt+T keys `:5000-5006`; tooltip `:2863-2889`.

**Publish pattern (chosen):** immutable sorted `vector<TempoSegment{startSample,bpm,startBeat}>`
behind an atomic shared_ptr swap + off-thread retire — RCU precedent `AudioClipSnapshot`
(`PluginProcessor.cpp:2640-2645`); double-buffer precedent `HarmonicEngine.h:130-145`/`.cpp:33-188`.
Seqlock rejected for a variable-length list. TS math available (read-only, unused by playback):
`getEffectiveTimeSigAtBar` `PatternManager.cpp:416-425`, `getBeatsPerBarAtBar` `:427-433`,
`beatToBarAndBeatInBar` `:443-493`, `barStartBeat` `.h:505`.

## 2026-07-08 — Tasks 1-4 CODE-COMPLETE (both configs build clean first pass, Jeff)

- **Publish pattern CHANGED from the plan's shared_ptr sketch** (implementation call, Rule 8):
  `std::atomic<shared_ptr>` is C++20 and the C++17 `std::atomic_load/store` free functions can take
  an internal lock — a real-time hazard on the audio thread. Shipped instead: **fixed-capacity
  seqlock** (`Source/TempoMapRead.h`, new) — namespace-global parallel atomic arrays
  `{startSample, startBeat, bpm}[512]` + count + sampleRate under the same two-fence protocol the
  QA-Ed anchor used. Zero locks/allocation on the audio thread; readers binary-search under seq
  validation and retry on torn reads. Globals (not playhead members) because PluginProcessor's
  scheduler must read the map and cannot include StandaloneApp.h (include cycle); `gCount == 0` =
  inactive → every consumer falls back to its pre-map linear math (legacy VST target unaffected).
- **StandalonePlayHead** (`StandaloneApp.h/.cpp`): anchor fields + `publishAnchor` DELETED (0 refs
  tree-wide); `deriveBeat` = map lookup (linear fallback pre-first-publish); `advanceBlock` loop
  bounds + `seekTo` = absolute `sampleAtBeat` lookups (the QA-Ed relative-anchor dance existed
  because one anchor re-based away from origin — the map never does, so those sites SIMPLIFIED);
  `getPosition()` reports the segment bpm → every block-rate consumer (BlockContext, strip tasks,
  clip stretch ratio) inherits per-block effective tempo with zero changes. `mBPM` = BASE tempo;
  `getBPM()` = effective-at-playhead (E display); `getBaseTempo()` added. New `setLiveTempo`
  (automation), `setTempoMarkers`, `rebuildTimeline(fwdOverride)`: PLAYING → truncate-and-append at
  the playhead (history never re-maps, sample clock never jumps; future markers re-added
  cumulatively → last-writer-wins falls out structurally); STOPPED → pure rebuild + beat-stable
  relocation (`sampleAtBeat(curBeat)` — tempo edits don't move your bar).
- **PluginProcessor**: scheduler loop bounds, `beatEnd`, wrapped-window end, note-on placement and
  pending-off placement all convert through the map when active (`beatToSmpInWindow` — window 1
  re-anchors at the wrap sample); metronome splits a boundary block into constant-tempo spans
  (it is the ear-check instrument; a linear sweep would misplace the boundary tick by up to a
  block); MIDI recorder gets the exact per-block average beats-per-sample while playing (an
  accumulation drift there is a persistent whole-take error) — count-in keeps the linear clock
  (transport frozen → map delta would be 0).
- **PatternManager**: `TempoChange{bar,bpm}` + sorted vector + CRUD (same-bar add replaces, clamps
  20-300 = the applicator range) + `<TempoChanges><Tempo bar bpm/>` XML beside the D-2 blocks
  (absent in old projects → clean empty default).
- **BuilderPage ruler**: amber BPM pill flags (distinct from yellow marker pennant + blue TS pill),
  right-click Add/Edit/Delete (menu ids 5/6/12 beside D-2's), prompts mirror the D-2 AlertWindow
  shape, hover tooltip. New `onTempoMapChanged` callback → editor pushes markers + markDirty.
- **StandaloneEditor**: `pushTempoMarkersToPlayHead()` (bar*4 beats, SONG-MODE GATED — markers are
  song-domain per the presented derived note; a long pattern crossing a marker beat in pattern
  mode was caught in self-review and gated); re-push on every mode switch + initial publish at
  ctor + load path (markers pushed BEFORE base so one rebuild sees both). Applicator →
  `setLiveTempo` and **no longer writes `setGlobalTempo`** — pre-map code persisted transient
  automation values into the base; under E (field owns base) that would corrupt it. Behavior
  change, deliberate, campaign-visible via TM-5.
- **Not done (stated limits):** tempo flags are prompt-driven, not undoable (matches D-2 markers
  exactly); two tempo boundaries inside ONE audio block degrade to one split (pathological at
  real block sizes); sample-rate changes require the existing Apply+Restart flow (map samples are
  SR-bound; SR is fixed per app run by design).
- **Diagnostics:** none added (nothing for the Rule 4 catalog).
- **Files:** `TempoMapRead.h` (new), `StandaloneApp.h/.cpp`, `PluginProcessor.cpp`,
  `PatternManager.h/.cpp`, `BuilderPage.h/.cpp`, `StandaloneEditor.h/.cpp`, test plan §B.3
  (+§B.2 hash backfill `805ca03`).

## 2026-07-08 — Spec clarification (Jeff): base-tempo persistence = option 1

Prompted by the TM-5 behavior-change flag, Jeff questioned whether the base should track
automation "like FL". Distinguishing scenario posed (save mid-automation at 87 with typed 140):
**Jeff picked 1 — the project remembers what you typed**; automation is a playback-only override
and never persists into the base. Matches the shipped code exactly — no change. Also stated as
fact + unchallenged: markers cannot write through (his own E pick — editing the base past a
marker changes only the pre-marker span — requires an independent base).

## Held Work Log entry (apply at section pass)

> Apply verbatim at §B.3 section pass; fill `<hash>` + section-pass date/outcome; group review
> line fills at the G1 boundary.

### <APPLY-DATE> — QA-TempoMap — Stepped sample-indexed tempo timeline replaces the single re-basing anchor (seqlock-published, lock-free audio reads, linear-fallback for the VST target) + Builder ruler tempo flags (add/edit/delete, XML round-trip) + field-edits-base/displays-effective + automation-as-live-override with markers re-asserting (last-writer-wins) + sample-exact scheduler/metronome/recorder conversions

**Bucket:** Cross-cutting Infrastructure

#### Done

- **Timeline engine** (`Source/TempoMapRead.h` new; `StandaloneApp.h/.cpp`): fixed-capacity
  (512-segment) seqlock-published map `{startSample, startBeat, bpm}`; audio thread binary-searches
  under seq validation — no locks, no allocation. The QA-Ed anchor (fields + `publishAnchor`) is
  deleted; `deriveBeat`/`advanceBlock`/`seekTo` became exact absolute lookups. Rebuild semantics:
  playing = truncate-and-append at the playhead (history immutable, no sample jump; markers ahead
  re-added → last-writer-wins structurally); stopped = pure rebuild + beat-stable relocation.
  `getPosition()` reports segment bpm → all block-rate consumers inherit correct per-block tempo.
- **Ruler tempo flags** (`PatternManager.h/.cpp`, `BuilderPage.h/.cpp`): `TempoChange{bar,bpm}`
  list (same-bar replace, 20-300 clamp) + `<TempoChanges>` XML; amber BPM pills on the ruler with
  right-click Add/Edit/Delete + tooltip; `onTempoMapChanged` → editor re-publish + dirty.
- **Semantics per the locked picks** (`StandaloneEditor.h/.cpp`): BPM field edits BASE / displays
  live effective (11+E); automation = live override via new `setLiveTempo`, no longer persists into
  the base; markers song-domain (empty set pushed in pattern mode; re-push on mode switch, load,
  ctor). Load pushes markers before base so one rebuild covers both.
- **Sample accuracy** (`PluginProcessor.cpp`): scheduler note-on/off placement + loop bounds +
  window ends convert through the map (in-block tempo steps land exactly); metronome splits
  boundary blocks into constant-tempo spans; MIDI recorder consumes exact per-block beat deltas
  while playing (count-in keeps its linear clock). All sites fall back to pre-map linear math when
  no timeline is published.

#### Found along the way

- The plan's shared_ptr/RCU publish sketch was replaced at implementation with the fixed-capacity
  seqlock: C++17 atomic shared_ptr ops can lock on the audio thread (implementation call under
  Rule 8, recorded in running notes 2026-07-08).
- Self-review catch: a global timeline would have applied markers to LONG patterns in pattern mode
  (crossing the marker's beat), contradicting the presented markers-song-only note — fixed by
  gating the marker push on song mode with re-push on mode switch.
- Pre-map behavior corrected as a consequence of E: the tempo-automation applicator used to write
  every 30 Hz value into the persisted project tempo (`setGlobalTempo`); it no longer touches the
  base. Campaign-visible via TM-5.

#### What was done about each finding

- All three folded in-batch (design/implementation calls within locked specs); none routed out.

#### Group review (R3 — one /review-batch per checkpoint group)

- <G1-boundary outcome — filled at group review>

#### Diagnostic Instrumentation Catalog

- None added.

#### Files touched

`Source/TempoMapRead.h` (new), `Source/Standalone/StandaloneApp.h/.cpp`,
`Source/PluginProcessor.cpp`, `Source/PatternManager.h/.cpp`,
`Source/Standalone/BuilderPage.h/.cpp`, `Source/Standalone/StandaloneEditor.h/.cpp`,
`Plans & Specs/Test Plans/v1-master-test-plan.md` (§B.3 + §B.2 hash backfill), paired plan +
running notes.

#### Commit(s)

`<hash>` (Tasks 1-4 + §B.3 + held entry + running notes — single batch commit per the bulk-run
model). Verified via Master Test Plan §B.3, <section-pass date/outcome>.

#### Next action

- <filled at apply: next unchecked §B section>.
