# Running Notes — QA-Ec (elastic-refitting-walrus)

> Append-only mid-batch log. New entry at every checkpoint. Under the BULK RUN this batch's Work
> Log entry is drafted at code-complete, HELD here under `## Held Work Log entry (apply at section
> pass)`, and applied only when its Master Test Plan §B section passes.

Pair file: [`Batch Plans/elastic-refitting-walrus.md`](../Batch Plans/elastic-refitting-walrus.md).
Conventions: Main Plan §0 + [`Batch Plans/swift-stampeding-caribou.md`](../Batch Plans/swift-stampeding-caribou.md).

## 2026-07-08 — G1 group open — plan approved + surface map (source-verified)

Locked: 2a/2b (dual-trigger; Resample=varispeed, Stretch=pitch-locked), F (plain drag trims;
Shift+drag re-fits), G (true-length import at project tempo; `originalBPM` = import tempo; NO
rounding — supersedes the marathon table's bar-rounding wording, corrected there 2026-07-08).
Two findings already recorded: BUILD-06's "missing rebuild trigger" claim is STALE (wired at QA-Ea
Task 0c); today any project tempo != 120 silently stretches every import (the 120 default) — G's
import rule removes the root cause. Verified refs (§5's `:510-533`-era refs are stale):

**Path A** `renderAudioClipsForRow` `PluginProcessor.cpp:508-905`: snapshot `:531`; `readRatio`
`:581`; ratio model `:598-604` (stretchRatio `:598-599` — Resample hardwired 1.0 = the gap; pitch
`:600`; varispeed `:601`; eff terms `:602-604`); window math `:611-617`; **GUARD `:618`**
(`outSamples<=0 → continue` = the silence bug); PV gate `:640-641`, `setStretchRatio` `:645`;
seekNeeded `:650-660`; direct read passes `fileRate` `:765-773`.

**Path B** `decodeFilePlayClip` `:912+` (split note `:906-911`): ratio `:975-979`; guard `:990`;
PV `:998-1006`. No pitch/varispeed terms. LOCKSTEP RULE: every Task-2 edit lands in both paths in
the same commit.

**Streamer:** stateless contract `AudioClipStreamer.h:10-13`; `readAndMix` `:65-71` — fit ratio
applies ABOVE it in the callers.

**Data:** `ArrangementBlock` `PatternManager.h:265` — `originalBPM` `:304` {120}, `stretchMode`
`:305` {true}, `contentStartSamples` `:343`, ticks `:291`/`:282`, `effectiveLengthBeats`
`:351-355`; `AudioLibraryEntry` `:648-664` (bpm/stretch `:662-663`); audio-thread copy
`AudioClipPlayer` `PluginProcessor.h:492-540`.

**Import sites (both get G):** `importAudioFile` `BuilderPage.cpp:3388` — reader `:3411`, length
`:3424-3426`, **120 default `:3422`**, beats `:3427`, block build `:3459-3470`;
`placeAudioLibraryEntry` `:3498` — **120 default `:3519`**, block `:3530-3540`; drop entry
`:3610`→`:3660` (+ callers `:5533`/`:5776`); rebuild reopens the reader
`PluginProcessor.cpp:2668-2669` (content length available there too).

**Stub + resize:** no-op stub `BuilderPage.cpp:4450-4454`; `mStretching` = Shift+right-edge
`:4237`; drag apply `:4419-4457` (`setLengthBeats` `:4430`); mouseUp `commitEdit` `:4767`
(+why-note `:4769-4773`) → `onArrangementChanged` `:2536` → `rebuildAudioClipPlayers` wiring
`StandaloneEditor.cpp:2118-2121`. Mode UI: props dialog combo `:3176-3178`, apply
`:3316-3318`/`:3380-3382`, menu `:3074`/`:3121`; distinct from grid `EditMode{Slip,Stretch}`
(`BuilderPage.h:365`/`:683`) — do not conflate.

**Persistence (already round-trips):** block `PatternManager.cpp:1076-1077`/`:1468-1469` (+
contentStart `:1094-1095`/`:1485-1486`, ticks `:1070`/`:1456-1461`); library
`:1143-1145`/`:1553-1555`. PV degenerate-ratio backstop `PhaseVocoder.cpp:44`.

## 2026-07-08 — Tasks 1-3 CODE-COMPLETE (built clean with QA-Eb, Jeff)

- **Task 1 — true-length import (G).** Both import sites: `originalBPM` = the tempo in effect at
  the TARGET BAR (base + ruler tempo flags via a local `tempoAtBar` walk — a drop into a
  marker-section sizes at that section's tempo, "where it lands is actually how long it is").
  New library entries get the import tempo stamped via `setAudioLibraryClipDefaults` — GUARDED by
  a count-before/after check so re-importing an existing file never clobbers a user-set BPM.
  `placeAudioLibraryEntry` now reads the entry's stored BPM (was: fresh hardcoded 120) AND
  inherits the entry's stretch/resample mode (was: hardcoded Stretch — ignored a Properties edit;
  aligns with the FILE-02 source-of-truth design).
- **Task 2 — Resample follow (2b) + clamps.** Path A: `tempoFollow = bpm/originalBPM` folded into
  the existing varispeed slots (read rate + consumption, never the vocoder ratio) — one term, all
  downstream math (EOF window, srcEnd, reverse) inherits it. Path B: folded into `readRatio` (its
  single rate slot). Both paths' stretch/follow ratios clamped [1/64, 64] (degenerate-ratio
  hardening; the `outSamples<=0` skip stays — with the 120-premise gone it only fires on TRUE
  content exhaustion). LOCKSTEP kept: both paths edited in the same commit.
- **Task 3 — Shift+drag re-fit (F).** Applies ONCE at mouseUp (no mid-drag rebuilds):
  `originalBPM *= newBeats/origBeats` (clamped 1..999). New `mStretchOrigBeats` member captures
  the EXACT beat length at drag start — `mResizeOrigLen` is whole bars and would misfit sub-bar
  clips. Stretch: doubles length -> half speed pitch-locked; Resample: the follow term makes the
  same field re-speed with pitch (vinyl). Plain drags untouched. Rubber Band no-op stub deleted.
- **Seam fix (found reading Path B): clip POSITIONS now resolve through the tempo map.** All five
  `clipStartBeat/clipEndBeat * secPerBeat * sr` sites (render gate A, decode gate B, MT pre-scan,
  choke scan x2) were linear-in-bpm — under ruler tempo flags clips would drift off the grid. New
  file-static `clipBeatToSample` helper -> `TempoMap::sampleAtBeat` when active, linear fallback
  (VST). This is the Ec<->TempoMap seam both plans flagged for coordination; §B.5 EC-9 tests it.
- **Diagnostics:** none added. **Files:** `PluginProcessor.cpp`, `BuilderPage.h/.cpp`, test plan
  §B.5 (+§B.3/§B.4 hash backfills ride the commits).

## Held Work Log entry (apply at section pass)

> Apply verbatim at §B.5 section pass; fill `<hash>` + date/outcome; group review line at G1 boundary.

### <APPLY-DATE> — QA-Ec — True-length import at the target-bar tempo (kills the hardcoded-120 default + its silent instant-stretch) + Resample varispeed tempo-follow in both render paths + Shift+drag re-fit via exact beat-ratio BPM scaling (replaces the Rubber Band no-op) + degenerate-ratio clamps + clip positions resolved through the tempo map (grid-lock across tempo flags)

**Bucket:** System Pages, Cross-cutting Infrastructure

#### Done

- **True-length import (G):** both import sites size the block at the tempo in effect at the
  target bar and set `originalBPM` to it (render ratio = exactly 1 at import); new library entries
  inherit the import tempo as source-of-truth BPM (guarded against clobbering user-set values);
  `placeAudioLibraryEntry` reads the entry's BPM + stretch mode instead of hardcoding 120/Stretch.
- **Resample tempo-follow (2b):** `bpm/originalBPM` varispeed term in Path A (folded into the
  Stretch-knob varispeed slots) and Path B (read rate), rate + pitch together; 1:1 at the clip's
  own tempo; ratios clamped [1/64, 64] both paths.
- **Shift+drag re-fit (F):** at mouseUp, `originalBPM` scales by the exact beat-length ratio (new
  `mStretchOrigBeats` capture — whole-bar math would misfit sub-bar clips); one persisted field
  drives both modes; plain drags remain trim/extend; the no-op stub is gone.
- **Tempo-map position seam:** five clip beat->sample sites route through
  `TempoMap::sampleAtBeat` (file-static `clipBeatToSample`, linear VST fallback) so clips stay
  grid-locked across ruler tempo flags.

#### Found along the way

- The BUILD-06 "missing rebuild trigger" claim was already stale at plan time (wired at QA-Ea
  Task 0c) — recorded at group open; the REAL gap (rebuild without re-fit) is what Task 3 closed.
- Clip-position linear math (the seam fix above) — found reading Path B during Task 2; fixed
  in-batch (real bug under the new tempo-flag feature).

#### What was done about each finding

- Both folded in-batch; EC-9 verifies the seam. Nothing routed out.

#### Group review (R3)

- G1 group review 2026-07-08: **clean for this batch** — 0 findings. Reviewer verified: Stretch
  ratios byte-identical to pre-batch for valid data (the clamp can't engage in 20-300 range);
  scheduler + advanceBlock share the same map lookups (wrap points cannot disagree); the
  clipBeatToSample seam + library-stamp guard confirmed as recorded fold-ins.

#### Diagnostic Instrumentation Catalog

- None added.

#### Files touched

`Source/PluginProcessor.cpp`, `Source/Standalone/BuilderPage.h/.cpp`, test plan §B.5, paired plan
+ running notes.

#### Commit(s)

`<hash>` (Tasks 1-3 + §B.5 + held entry — single batch commit). Verified via Master Test Plan
§B.5, <section-pass date/outcome>.

#### Next action

- <filled at apply>.
