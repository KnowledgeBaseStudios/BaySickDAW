# QA-Ee — 96 PPQ Universal Timebase + Decoupled Snap Params — Plan (rhythmic-counting-octopus)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/rhythmic-counting-octopus.md`
> Paired running notes: `Plans & Specs/Running Notes/rhythmic-counting-octopus.md`
> (The plan-mode home-dir file `ticklish-yawning-kazoo.md` is transient UI scaffolding — on
> mirror it is copied to the **reserved** name `rhythmic-counting-octopus` per Main Plan.md:4412/4447
> and the home-dir copy is deleted, so only one canonical file exists.)

> **For execution:** use `superpowers:executing-plans` inline. Steps use `- [ ]` checkbox syntax.
> Build commands run by Jeff (`do_build.bat`) — never by Claude. Verify in the **Debug** exe FIRST,
> then **Release** (CLAUDE.md Build System standing rule). Each source stage is its own commit
> (Jeff: staged-commit structure) routed through `/draft-commit`; long messages via
> `git commit -F .git/COMMIT_EDITMSG_QA-Ee-<task>.txt` (CLAUDE.md Git Commit Mechanics).

---

## Context

**Why this batch.** QA-Ee establishes a **96 PPQ (ticks-per-quarter-note) universal musical
timebase** as BaySickDAW's authoritative musical-domain clock. Today musical positions are stored
as float **beats**; there is **no PPQ/tick constant anywhere in the tree** (confirmed — searched
`VibesynthConstants.h` + whole Source tree). The pivot originated 2026-05-20 mid QA-Ea Task 0c:
the interim `record_quantize_div` (Int 0..5, straight-time only — Off / 1/4 / 1/8 / 1/16 / 1/32 /
1/64) cannot express triplet divisions, which are fundamental to FL-parity workflow. Jeff reframed
the range gap into a full architectural pivot (Option iii) → a 96 PPQ tick foundation + an APVTS
`Unified_*` snap-param convention. At 96 PPQ every musical division — straight **and** triplet —
lands on an integer tick count, so triplet snap "just works" with no float drift. See Main Plan §5
QA-Ee entry + §9 twenty-sixth Forks (originating) + forty-eighth Forks (QA-Ed close re-sequencing).

**What it does.** `kTicksPerBeat = 96` becomes the musical-domain source-of-truth. Clip + note
start/length positions store as `int64` ticks; the float-beat view becomes derived via getter
helpers (Jeff SC-3 = defensive bridge — the audio engine + serdes run on ticks while the public
playhead API + most read sites keep seeing beats). Old projects migrate `beats x 96 -> ticks` on
load. The interim `record_quantize_div` is renamed `Unified_RecordQuantizeDiv` and joined by
`Unified_BuilderSnapDiv` + `Unified_PianoRollSnapDiv` (decoupled — Jeff SC-5), all three `Int 0..9`
on one triplet-aware 10-label scheme. BuilderPage `SnapMode` drops `Events` + `Line` (Jeff SC-ii);
PianoRoll snap goes **global** (Jeff: drop per-roll `snapDenominator`); the MIDI-commit consumer +
slip-edit + grid math move to ticks; triplet grid lines render identically to straight (Jeff SC-4).

**Composes on QA-Ed (locked dependency).** QA-Ed (closed 2026-06-01, `ffc6dc7`) rebuilt
`StandalonePlayHead` on an `int64 mSamplePos` source-of-truth + a seqlock tempo anchor
`{mAnchorBeat, mAnchorSample, mAnchorBpm}`, with the **public playhead API kept in beats (SC-5)**
and a shared `scheduleRollWindows` / `RollWindow[2]` scheduler doing integer straddle off
`timeInSamples`. QA-Ee's tick layer rides **above** this sample clock: sample-domain authoritative
(QA-Ed) -> musical-domain authoritative (QA-Ee). **Inherited math constraint (QA-Ed Problem 2):**
all loop/position conversions must be **anchor-relative, not from beat 0** (the anchor re-bases off
origin under tempo automation). QA-Ee adds no new beat<->sample conversions on the audio thread —
the snapshot keeps reading derived beats via getters (see Stage 1).

**Risk: HIGH.** Touches the data model (`ArrangementBlock` + `PianoNote` fields + XML serdes +
old-project migration), the audio-clip render read sites, message-thread grid math (BuilderPage +
PianoRoll), the APVTS layer (rename + 2 new params), three UI snap surfaces, and the MIDI-commit
consumer. Mandatory `/review-batch` before close (QA-Ea/QA-Ed precedent for high-risk batches).

**Risk note — audio-clip length precision (documented tradeoff, verify item, NOT a re-opened spec
call).** Migrating an audio clip's timeline **length** to `int64` ticks quantizes the clip's
grid-end to 1/96 beat (<= ~2.6 ms at 120 BPM). Actual file playback is sample-driven
(`contentStartSamples` + file reads) and already bounded by the QA-Ea file-EOF guard
(`fileTotalSamples - contentStart`), so audible impact is negligible — but Stage-1 verify checks a
recorded clip's start **and** end explicitly. If Jeff wants bit-exact audio-clip length preserved,
that is a Stage-1 refinement (keep length sample-authoritative for `ClipType::Audio`) decided at
verify, not now.

**Effort estimate:** ~10–16 hours. Stage 1 (data model + migration) ~3–4 h; Stage 2 (Builder)
~2–3 h; Stage 3 (PianoRoll + note tick refactor) ~3–4 h; Stage 4 (Record-Quantize) ~1–2 h; verify
~2–3 h; `/review-batch` ~1 h.

**Dependencies.** QA-Ed transport int-sample source-of-truth (landed, `ffc6dc7`). QA-Ea Task 0c
slip-edit / `contentStartSamples` / `startBeats` / `effective*` helpers (landed, `c5c5deb`).

**QA-ClipDrop (concurrent, HELD OPEN) — baseline refreshed 2026-06-02.** QA-ClipDrop became a real
fix batch, not just diagnostics: it committed Tasks 0–3 (`122ff3f` / `4a9342c` / `c616f0d` /
`1e53a2d` = **current main**) and **stays open** watching for an intermittent saved-project
copy-failure (its Task 1 diagnostic trap is still armed in the tree). It touched shared files
(`PluginProcessor.cpp renderAudioClipsForRow`, `BuilderPage.cpp`, `StandaloneEditor.cpp`) but **did
NOT touch** QA-Ee's core surfaces (`ArrangementBlock` struct, XML serdes, `PianoNote`, PianoRoll) —
the tick model + `Unified_*` design holds. Two coordination facts:
- **(a) New clip-routing model** — audio clips route by their owning Clips-page strip
  (`routeChannel`), NOT the grid `trackRow` (now just visual position;
  [PluginProcessor.cpp:416-420](Source/PluginProcessor.cpp:416)). Preserve this when
  auditing/touching `renderAudioClipsForRow`; QA-Ee changes positions, never routing.
- **(b) Coexisting load migration** — QA-ClipDrop added a second load-time fixup
  (`routeChannel == 0 → audioInsert(trackRow)` at
  [PluginProcessor.cpp:2314](Source/PluginProcessor.cpp:2314), in `rebuildAudioClipPlayers`).
  QA-Ee's `startBeats × 96 → startTicks` migration lives in `PatternManager::fromValueTree` —
  different field + function, composes cleanly; keep both coherent.

**Action:** re-`/standup` + re-read `renderAudioClipsForRow` + `rebuildAudioClipPlayers` on current
main at Stage 1 start (line numbers in this plan are indicative — QA-ClipDrop may add commits while
held open). QA-Ee's Task 0 doc edits target the **QA-Ee** §5 entry (distinct row from QA-ClipDrop's),
so no clobber; still re-read §5 fresh. The armed QA-ClipDrop trap is **theirs (Keep)** — do not strip
it during QA-Ee close.

**QA-TempoMap runs AFTER QA-Ee** as the deliberate bridge between QA-Ee's tick clock and QA-Ed's
sample clock — keep sample-accurate-tempo-map scope OUT of QA-Ee (sanctioned SC-1 deferral).

**Bucket:** Cross-cutting Infrastructure.

---

## Spec calls already locked (with reasoning)

| ID | Decision | Source / Reasoning |
|----|----------|--------------------|
| SC-i | **Slot:** after QA-ClipDrop, before QA-TempoMap (musical-clock refactor follows sample-clock refactor). | Jeff, §6 arrow + §9 forty-eighth Forks. |
| SC-3 | **Defensive bridge:** ticks authoritative; float-beat is a derived read-only view via getter helpers. No app-breaking simultaneous rewrite. | §5 / Jeff. Audio engine + serdes on ticks; public API + most read sites keep beats. |
| SC-5 | **Decoupled snap:** three separate `Unified_*` APVTS params (Builder / PianoRoll / Record). | §5 / Jeff — FL-parity workflow. |
| SC-ii | **Drop `Events` + `Line`** BuilderPage snap modes; replace with the 10-label scheme. | §5 / Jeff — vestigial/no-op modes. |
| SC-4 | **Triplet grid lines render identically** to straight-time (no dashed / shading distinction). | §5 / Jeff — matches FL Studio. |
| SC-labels | **10-label scheme**, Int 0..9: `Off / Bar / Beat / 1/2 Beat / 1/3 Beat / Step / 1/2 Step / 1/3 Step / 1/4 Step / 1/6 Step`. Tick grid: `1 / 384 / 96 / 48 / 32 / 24 / 12 / 8 / 6 / 4`. ("Step" = 1/16 note = 24 ticks.) | §5 / Jeff — every label is an integer tick count at 96 PPQ. |
| SC-A | **Staged commits** (one per source stage). **Stage 1 is strictly the data-model migration** (block `startBeats`->`startTicks` + getters + tick serdes + old-project migration) — verify old projects load + play before any UI is touched. UI snap stages (Builder / PianoRoll / Record) follow. | Jeff 2026-06-02. |
| SC-B | **PianoRoll snap = global.** Drop the per-roll `PianoRollData.snapDenominator`; single `Unified_PianoRollSnapDiv` APVTS param drives every piano roll. | Jeff 2026-06-02 — snap resolution is a global user-workflow state, not a per-pattern property (FL parity). |
| SC-C | **Automation out of scope.** Curve points stay `0..1` clip-length fractions (already correct architecture); `lfoRate` stays a float (beats). Only the automation **clip's** `ArrangementBlock` start/length migrates (via the universal block migration). | Jeff 2026-06-02 — confirmed against `ControlPoint.timeTicks` ([PatternManager.h:10](Source/PatternManager.h:10)). |
| SC-def | **Snap param defaults preserve current behavior:** `Unified_BuilderSnapDiv` = 1 (Bar, today's combo default), `Unified_PianoRollSnapDiv` = 6 (1/2 Step = 1/32 note, today's `snapDenominator=32`), `Unified_RecordQuantizeDiv` = 0 (Off, today's default). | Correctness-preserving (no gratuitous behavior change). Flagged for Jeff visibility; not a free choice. |
| SC-name | Plan-file silly-name = `rhythmic-counting-octopus` (reserved 2026-05-20, Main Plan.md:4412/4447). | Honor the reservation; do not use the auto-assigned home-dir name. |

---

## Sub-spec calls surfaced for ExitPlanMode

**No sub-spec calls open.** All three open decisions (commit structure SC-A, PianoRoll snap scope
SC-B, automation scope SC-C) were surfaced in chat and answered by Jeff 2026-06-02 before any plan
body was written (Main Plan §0 Rule 5). The audio-clip-length precision item is a documented Risk +
Stage-1 verify scenario (see Context), not a deferred decision.

---

## Files to modify (by stage)

### Stage 1 — Data-model migration (blocks only; no UI)
- [Source/VibesynthConstants.h](Source/VibesynthConstants.h) — add `kTicksPerBeat = 96` + `beatsToTicks()` / `ticksToBeats()` inline converters.
- [Source/PatternManager.h](Source/PatternManager.h) — `ArrangementBlock`: replace `float startBeats`/`lengthBeats` ([:251/:262](Source/PatternManager.h:251)) with `int64 startTicks`/`lengthTicks` (sentinel-preserving); rewrite `effectiveStartBeats`/`effectiveLengthBeats`/`effectiveStartBars`/`effectiveLengthBars` ([:317-345](Source/PatternManager.h:317)) to derive from ticks; add `setStartBeats(double)`/`setLengthBeats(double)` + `effectiveStartTicks`/`effectiveLengthTicks` helpers.
- [Source/PatternManager.cpp](Source/PatternManager.cpp) — block `toValueTree` ([:1052-1090](Source/PatternManager.cpp:1052)) writes `startTicks`/`lengthTicks`; `fromValueTree` ([:1436-1469](Source/PatternManager.cpp:1436)) reads tick props if present else migrates from legacy `startBeats`/`lengthBeats`. Audit `getEffectivePatternLoopBeats` ([:614](Source/PatternManager.cpp:614)) — stays in beats (derived; loop length is a computed extent, public-API-in-beats).
- [Source/Standalone/BuilderPage.cpp](Source/Standalone/BuilderPage.cpp) — **mechanical writer swaps only** (no snap/UI logic change): slip-edit `blk.startBeats =`/`blk.lengthBeats =` ([:4551/4553/4616/4375](Source/Standalone/BuilderPage.cpp:4551)) -> `blk.setStartBeats(...)`/`blk.setLengthBeats(...)`; `commitRecordingResult` `b.lengthBeats =` ([:3375-3487](Source/Standalone/BuilderPage.cpp:3375)) -> setter. Direct `b.startBeats`/`b.lengthBeats` **reads** -> `effectiveStartBeats(b)`/`effectiveLengthBeats(b)`.
- [Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp) — `commitRecordingResult` Option-Y placement writers (`startBeats`/`lengthBeats`) -> setters.
- [Source/PluginProcessor.cpp](Source/PluginProcessor.cpp) — **audit only, expect no change** (refreshed to current main `1e53a2d`): clip beat->sample sites now at [:430/650/1581/2433/2477](Source/PluginProcessor.cpp:430) read `clipStartBeat`/`clipEndBeat` from the snapshot ([:2334/2338](Source/PluginProcessor.cpp:2334) via the `effective*` getters, which now derive beats from ticks) — unchanged under the bridge. `renderAudioClipsForRow` ([:384](Source/PluginProcessor.cpp:384)) is a HIGH-risk per-row-per-block hot path QA-ClipDrop Task 2 just reworked (routeChannel routing at [:416-420](Source/PluginProcessor.cpp:416)) — **preserve that routing, do not change it**. Confirm no direct `blk.startBeats`/`blk.lengthBeats` field reads remain tree-wide.

### Stage 2 — Builder snap UI (ticks + 10-label + `Unified_BuilderSnapDiv`)
- [Source/PluginProcessor.cpp](Source/PluginProcessor.cpp) — register `addI("Unified_BuilderSnapDiv", "Builder Snap Div", 0, 9, 1)` near [:106](Source/PluginProcessor.cpp:106).
- [Source/PatternManager.h](Source/PatternManager.h) (or VibesynthConstants.h) — shared `snapDivToTicks(int idx)` + `kUnifiedSnapLabels[10]` (ASCII) reused by all three surfaces.
- [Source/Standalone/BuilderPage.h](Source/Standalone/BuilderPage.h) — `SnapMode` enum ([:324](Source/Standalone/BuilderPage.h:324)) drops `Events`+`Line`; `mSnapMode` becomes the 10-value index (or read APVTS directly).
- [Source/Standalone/BuilderPage.cpp](Source/Standalone/BuilderPage.cpp) — `mSnapCombo` rebuild ([:5350-5368](Source/Standalone/BuilderPage.cpp:5350)) to the 10 labels bound to APVTS; `snapBar`/`snapBarAlt` ([:1305-1321](Source/Standalone/BuilderPage.cpp:1305)) -> tick snap (Bar = TS-aware bar boundary via `barStartBeat`; others = fixed tick grid); slip-edit tick math ([:4480-4616](Source/Standalone/BuilderPage.cpp:4480)); grid-line render at tick positions (triplet identical to straight, SC-4).
- [Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp) — wire `mSnapCombo` <-> `Unified_BuilderSnapDiv` (read/write, listener) replacing the local `onSnapChanged` plumbing.

### Stage 3 — PianoRoll snap UI (global) + note tick refactor
- [Source/PluginProcessor.cpp](Source/PluginProcessor.cpp) — register `addI("Unified_PianoRollSnapDiv", "Piano Roll Snap Div", 0, 9, 6)`.
- [Source/PatternManager.h](Source/PatternManager.h) — `PianoNote`: add `int64 startTicks`/`durationTicks` (dual-representation; ticks authoritative for serdes); `PianoRollData.snapDenominator` ([:70](Source/PatternManager.h:70)) deprecated (kept for read-compat or removed — see Stage 3 notes).
- [Source/PatternManager.cpp](Source/PatternManager.cpp) — `noteToValueTree`/`noteFromValueTree` ([:688-720](Source/PatternManager.cpp:688)) write `st`/`dt` tick props, read tick-first-else-legacy `s`/`d`.
- [Source/Standalone/PianoRoll.cpp](Source/Standalone/PianoRoll.cpp) — `snapBeat` ([:409-413](Source/Standalone/PianoRoll.cpp:409)) -> tick snap reading the global param; `mMagnetBtn` on/off ([:2570-2573](Source/Standalone/PianoRoll.cpp:2570)) maps to Off (idx 0) <-> last-non-Off; right-click resolution menu -> 10-label menu writing the global param; grid render at tick positions ([:1915-1930](Source/Standalone/PianoRoll.cpp:1915)); the ~40 note start/length edit sites move to tick-grid snapping.
- [Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp) — wire PianoRoll snap menu <-> `Unified_PianoRollSnapDiv`; push the global value into every piano-roll instance.

### Stage 4 — Record-Quantize (triplet-aware + rename)
- [Source/PluginProcessor.cpp](Source/PluginProcessor.cpp) — rename `record_quantize_div` -> `Unified_RecordQuantizeDiv`, range `0..5` -> `0..9` ([:106](Source/PluginProcessor.cpp:106)).
- [Source/Standalone/GlobalTransportBar.cpp](Source/Standalone/GlobalTransportBar.cpp) — "Global Record-Quantize" submenu ([:422-439](Source/Standalone/GlobalTransportBar.cpp:422)) 6 items -> 10 (the 10 labels), ids 100..109.
- [Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp) — MIDI-commit `quantizeBeats` switch ([:10840-10871](Source/Standalone/StandaloneEditor.cpp:10840)) -> triplet-aware tick mapping (snap post-clamp `startBeat` to nearest tick grid); `onGetRecordQuantizeDiv`/`onRecordQuantizeDivChanged` wiring (~:918-935) + the `getRawParameterValue("record_quantize_div")` read ([:10841](Source/Standalone/StandaloneEditor.cpp:10841)) updated to the new id.

---

## Tasks

### Task 0 — Open (docs + commit) — GATED on QA-ClipDrop docs landing
- [ ] **QA-ClipDrop docs are landed** (Task 0 `122ff3f` + Task 3 `1e53a2d` running-notes; §5 QA-ClipDrop STATUS=open, held open for the copy-failure watch). My edits target the **QA-Ee** §5 entry (a distinct row), so no clobber — but re-read Main Plan §5 fresh anyway (QA-ClipDrop may commit more while open).
- [ ] Mirror `~/.claude/plans/ticklish-yawning-kazoo.md` -> `Plans & Specs/Batch Plans/rhythmic-counting-octopus.md` (Write); delete the home-dir copy (`feedback_plan_mirror_one_way.md`).
- [ ] Add `**Plan file:** Plans & Specs/Batch Plans/rhythmic-counting-octopus.md` (backticked-path form) to the Main Plan §5 QA-Ee entry header.
- [ ] Seed `Plans & Specs/Running Notes/rhythmic-counting-octopus.md` per §0 required sections (title / purpose blockquote / pair ref / convention ref).
- [ ] Surface full git status. `/draft-commit`. Surface drafted message + git status to Jeff for approval. Commit Task 0 (docs only) on approval.

### Task 1 — Stage 1: data-model migration (blocks; verify load + play)
- [ ] **Baseline refresh (do FIRST):** re-`/standup` on current main; re-read `renderAudioClipsForRow` + `rebuildAudioClipPlayers` ([PluginProcessor.cpp:384/2286](Source/PluginProcessor.cpp:384)); confirm the `effective*`-getter snapshot path + QA-ClipDrop's `routeChannel` routing model are intact, and note QA-ClipDrop's coexisting `routeChannel==0 → audioInsert(trackRow)` load fixup ([:2314](Source/PluginProcessor.cpp:2314)) so the two load migrations stay coherent. Refresh any drifted line numbers in this plan.
- [ ] **VibesynthConstants.h** — add the foundation:
  ```cpp
  // QA-Ee (96 PPQ): authoritative musical-domain resolution. One quarter-note
  // (one "beat") = 96 ticks. Every straight + triplet division lands on an
  // integer tick count (Bar=384, Beat=96, 1/8=48, 1/8T=32, 1/16=24, 1/32=12,
  // 1/32T=8, 1/64=6, 1/64T=4). Rides ABOVE QA-Ed's int64-sample transport.
  constexpr int kTicksPerBeat = 96;
  inline juce::int64 beatsToTicks (double beats) noexcept
      { return (juce::int64) std::llround (beats * (double) kTicksPerBeat); }
  inline double ticksToBeats (juce::int64 ticks) noexcept
      { return (double) ticks / (double) kTicksPerBeat; }
  ```
- [ ] **PatternManager.h `ArrangementBlock`** — replace float position fields with tick fields (sentinel-preserving) + bridge getters/setters:
  ```cpp
  // QA-Ee: int64-tick authoritative. Sentinels preserve the legacy "derive
  // from startBar/lengthBars (4/4 fallback)" semantics for pre-Task-0c blocks.
  static constexpr juce::int64 kStartTicksUnset  = std::numeric_limits<juce::int64>::min();
  static constexpr juce::int64 kLengthTicksUnset = -1;
  juce::int64 startTicks  { kStartTicksUnset };
  juce::int64 lengthTicks { kLengthTicksUnset };
  // ... contentStartSamples etc. unchanged ...
  void setStartBeats  (double b) noexcept { startTicks  = beatsToTicks (b); }
  void setLengthBeats (double b) noexcept { lengthTicks = (b > 0.0) ? beatsToTicks (b) : kLengthTicksUnset; }
  ```
  Rewrite the `effective*` free helpers to derive beats from ticks, preserving the existing 4/4 `startBar*4` / `lengthBars*4` fallback exactly:
  ```cpp
  inline double effectiveStartBeats (const ArrangementBlock& b) noexcept
  { return (b.startTicks != ArrangementBlock::kStartTicksUnset) ? ticksToBeats (b.startTicks)
                                                                : (double) b.startBar * 4.0; }
  inline double effectiveLengthBeats (const ArrangementBlock& b) noexcept
  { return (b.lengthTicks > 0) ? ticksToBeats (b.lengthTicks) : (double) b.lengthBars * 4.0; }
  ```
- [ ] **PatternManager.cpp block serdes** — write tick props; read tick-first-else-migrate-legacy:
  ```cpp
  // toValueTree (save): write ticks; skip startTicks when unset (clean XML).
  bNode.setProperty ("lengthTicks", b.lengthTicks, nullptr);   // -1 = unset
  if (b.startTicks != ArrangementBlock::kStartTicksUnset)
      bNode.setProperty ("startTicks", b.startTicks, nullptr);
  // fromValueTree (load): prefer ticks; else migrate legacy float beats x 96.
  if (bNode.hasProperty ("lengthTicks"))
      b.lengthTicks = (juce::int64) bNode.getProperty ("lengthTicks", (juce::int64) -1);
  else { float lb = (float) bNode.getProperty ("lengthBeats", -1.f);
         b.lengthTicks = (lb > 0.f) ? beatsToTicks ((double) lb) : ArrangementBlock::kLengthTicksUnset; }
  if (bNode.hasProperty ("startTicks"))
      b.startTicks = (juce::int64) bNode.getProperty ("startTicks", ArrangementBlock::kStartTicksUnset);
  else { double sb = (double) bNode.getProperty ("startBeats", (double) -1.0e6);
         b.startTicks = (sb > -1.0e5) ? beatsToTicks (sb) : ArrangementBlock::kStartTicksUnset; }
  ```
  (Keep writing nothing for the old `startBeats`/`lengthBeats` props — new format is tick-only; downgrade unsupported. `startBar`/`lengthBars` still written for the fallback path.)
- [ ] **Writer swaps** — BuilderPage slip-edit + `commitRecordingResult` (BuilderPage + StandaloneEditor): `blk.startBeats = X` -> `blk.setStartBeats(X)`, `blk.lengthBeats = X` -> `blk.setLengthBeats(X)`. Direct field **reads** -> `effectiveStartBeats(blk)` / `effectiveLengthBeats(blk)`.
- [ ] **Audit** PluginProcessor clip sites + any other tree-wide `.startBeats`/`.lengthBeats` access (grep) — route through getters/setters; confirm the audio snapshot path is unchanged.
- [ ] **Notes untouched this stage** (still beat fields + beat serdes; beat-scheduled playback; migrate in Stage 3).
- [ ] Tell Jeff (verify — Debug first, then Release): "Run `do_build.bat`. (1) Open 2–3 of your existing saved projects (pre-QA-Ee). Confirm every arrangement block + audio clip appears at the **same** position/length as before. (2) Play each — pattern blocks + audio clips start on time and play through; a recorded/slip-edited audio clip starts at the right offset **and** ends at the right point (no early cut / overhang). (3) Save one, reload it — byte-stable positions, identical playback. (4) Drop a fresh WAV on the grid — it places + plays. (5) Existing Builder snap still behaves exactly as before (combo unchanged this stage)."
- [ ] On pass: `/draft-commit` -> surface message + full git status -> commit on approval. `/draft-doc running-notes` -> apply.

### Task 2 — Stage 2: Builder snap (ticks + 10-label + `Unified_BuilderSnapDiv`)
- [ ] Register `Unified_BuilderSnapDiv` (Int 0..9, default 1=Bar). Add shared `snapDivToTicks(int)` + `kUnifiedSnapLabels[10]` (ASCII: `"Off","Bar","Beat","1/2 Beat","1/3 Beat","Step","1/2 Step","1/3 Step","1/4 Step","1/6 Step"`).
- [ ] Rebuild `mSnapCombo` to the 10 labels bound to the APVTS param; drop `SnapMode::Events`/`Line`.
- [ ] Convert `snapBar`/`snapBarAlt` to tick snap. Pattern:
  ```cpp
  // Bar (idx 1): TS-aware bar boundary (nearest bar via the TS table).
  // idx 2..9: fixed tick grid g = snapDivToTicks(idx); tick = round(t/g)*g.
  // idx 0 (Off): no snap. Alt held: no snap (existing mAltSnapActive).
  ```
  Slip-edit math ([:4480-4616](Source/Standalone/BuilderPage.cpp:4480)) -> tick domain (replace the bar-divide/snap/multiply with tick snap).
- [ ] Grid lines render at tick positions; triplet lines drawn identically to straight (SC-4).
- [ ] Tell Jeff (verify): "(1) Builder snap combo shows the 10 labels. (2) Pick `1/3 Beat`, drag a clip — it lands on eighth-note-triplet positions (3 per beat). (3) Pick `Step`, drag — lands on 1/16 positions. (4) `Bar` snaps to bar starts; if you have a 3/4 pattern, confirm Bar respects it (288-tick bars, not 384). (5) Grid lines for a triplet division look the same as straight (no dashes/shading). (6) Old projects still load + play (Stage-1 regression check)."
- [ ] On pass: `/draft-commit` -> surface -> commit. `/draft-doc running-notes` -> apply.

### Task 3 — Stage 3: PianoRoll snap (global) + note tick refactor
- [ ] Register `Unified_PianoRollSnapDiv` (Int 0..9, default 6 = 1/2 Step = 1/32 note, today's value).
- [ ] `PianoNote`: add `int64 startTicks`/`durationTicks` (ticks authoritative for serdes); `noteToValueTree`/`noteFromValueTree` write `st`/`dt`, read tick-first-else-legacy `s`/`d` (migration).
- [ ] `snapBeat` -> tick snap reading the **global** param (drop `mSnapDenom`/`PianoRollData.snapDenominator` as the source). `mMagnetBtn` on/off maps to param Off (0) <-> last-non-Off. Right-click resolution menu -> 10-label menu writing the global param. Push the global value to every piano-roll instance.
- [ ] Grid render at tick positions; the ~40 note edit sites snap to the tick grid.
- [ ] Tell Jeff (verify): "(1) Open a Layer piano roll + a Bass piano roll. Change snap in one — it changes in the other (global). (2) Pick `1/3 Beat`, place notes — they land on eighth-triplet positions. (3) Pick `Step`, place — 1/16. (4) Triplet grid lines look identical to straight. (5) Snap toggle (magnet) turns snap fully off/on. (6) Load a pre-QA-Ee project — its notes appear at the same positions (note migration), play correctly."
- [ ] On pass: `/draft-commit` -> surface -> commit. `/draft-doc running-notes` -> apply.

### Task 4 — Stage 4: Record-Quantize (triplet-aware + rename)
- [ ] Rename `record_quantize_div` -> `Unified_RecordQuantizeDiv`, range 0..9 (default 0=Off). Update the registration + the `getRawParameterValue` read site + the `onGet/onChanged` wiring.
- [ ] GlobalTransportBar submenu: 10 items (ids 100..109) with the 10 labels.
- [ ] MIDI-commit consumer: replace the binary `quantizeBeats` switch with tick-grid snapping:
  ```cpp
  // post Early-Strike clamp: snap startBeat to the chosen tick grid.
  const int div = (int) qDiv->load();
  if (div > 0) {
      const juce::int64 g  = snapDivToTicks (div);          // 384..4
      const juce::int64 t  = std::llround (n.startBeat * kTicksPerBeat);
      n.startBeat = ticksToBeats ((t + g/2) / g * g);       // round to nearest grid tick
  }
  ```
- [ ] Tell Jeff (verify): "(1) Record dropdown -> Global Record-Quantize shows 10 labels. (2) Set `1/3 Beat`, record a MIDI take loosely — notes land on eighth-triplet boundaries. (3) Set `Step`, record — 1/16 boundaries. (4) `Off` keeps raw timing. (5) Early-strike clamp + noodling-discard still behave (record notes straddling the downbeat)."
- [ ] On pass: `/draft-commit` -> surface -> commit. `/draft-doc running-notes` -> apply.

### Task 5 — Close sequence
- [ ] `/draft-doc batch-close` from the running-notes file.
- [ ] Apply the close entry to `Plans & Specs/Implemented Work Log.md` via Edit (parent session).
- [ ] `/review-batch QA-Ee` (mandatory — high-risk). Address BLOCKERs / NEEDS-FIX in-batch; defer NITs into the close-entry routing table.
- [ ] Strip any Stage-tagged diagnostic instrumentation per the running-notes Diagnostic Catalog (surface the strip list to Jeff first, §0 Rule 4).
- [ ] Route side findings per §0 Rule 3 (in-batch -> close routing table; outside-batch -> §9 Forks + §5/§6/Future State, slot surfaced to Jeff).
- [ ] Surface full git status. `/draft-commit` for the close commit. Commit the close (separate from source commits — clean rollback boundary).

---

## MT-awareness note

QA-Ee adds **no new audio-thread beat<->sample conversions**. The MT scheduler + clip render read
derived beats (`clipStartBeat`/`clipEndBeat` from the snapshot via getters) exactly as today; the
tick layer is message-thread (serdes + snap + grid). The QA-Ed seqlock anchor + integer
`scheduleRollWindows` straddle are untouched. Serial<->MT parity verifies in the normal
Debug-then-Release cycle (MT works in Debug, QA-Md). No serial mirror to keep in step.

---

## Verification (end-to-end smoke, after Task 4)

1. **Build clean** — `do_build.bat` Release + Debug both green.
2. **Migration** — 2–3 pre-QA-Ee projects load with identical block + note positions; save/reload byte-stable; playback identical (audio-clip start **and** end correct).
3. **Triplet snap, all three surfaces** — Builder / PianoRoll / Record each: `1/3 Beat` lands on eighth-triplets, `Step` on 1/16, `Bar` TS-aware.
4. **Global PianoRoll snap** — changing snap in one piano roll changes it in all.
5. **Decoupled** — Builder, PianoRoll, Record snap settings are independent of each other.
6. **Triplet rendering** — triplet grid lines identical to straight (no dashes/shading).
7. **No drift on long projects** — place triplet content far down a long arrangement; positions stay integer-tick-aligned (no float drift).

---

## Routing notes (Rule 3 application during execution)

- Findings scoped to the tick/snap/data-model surface -> fold into the appropriate stage; record in the close routing table.
- Sample-accurate-tempo-map needs -> **route to QA-TempoMap** (do NOT absorb; sanctioned SC-1 deferral). §9 only if scope shifts.
- Audio-clip-length bit-exact preservation (if Stage-1 verify shows a problem) -> Stage-1 refinement (`ClipType::Audio` keeps sample-authoritative length), decided with Jeff at verify.
- Old-project load regressions (cf. QA-Ea FND-4) -> fix in-batch (Stage 1), not deferred.
- Outside-surface findings -> §9 Forks + §5/§6/Future State; slot/placement surfaced to Jeff, never self-picked.
- **QA-ClipDrop's armed diagnostic trap** (Task 1, still live in the tree for the held-open copy-failure watch) is **theirs — Keep**. Do NOT strip it during QA-Ee's close-time Diagnostic Catalog pass (§0 Rule 4); it belongs to QA-ClipDrop's catalog. If it fires during QA-Ee verify, that is the QA-ClipDrop bug recurring — report to Jeff, route to QA-ClipDrop, do not absorb.

---

## Carry-Forward Reference touch points

- **Stage 1:** Carry-Forward §3 (BuilderPage grid drop/resize — pre-QA-Ea state; superseded by QA-Ea Task 0c + this batch) + §6 (RCU + `effective*` helper pattern). Implemented Work Log QA-Ea Task 0c entry (the `startBeats`/`contentStartSamples`/`effective*` data model this stage migrates) + QA-Ed entry (the int-sample transport this rides above; **source is authority** post-2026-05-07 freeze).
- **Stages 2–4:** the QA-Ea Task 0c `record_quantize_div` + slip-edit + `EditMode` notes; QA-Ed scheduler (`scheduleRollWindows`) for the beat-scheduled note playback that stays in beats.
