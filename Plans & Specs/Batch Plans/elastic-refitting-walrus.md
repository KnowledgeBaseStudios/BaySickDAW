# QA-Ec — Clip Resample/Stretch Follow-Tempo + Resize Re-Fit + True-Length Import — Plan (elastic-refitting-walrus)

> **Canonical path (post-approval):** `Plans & Specs/Batch Plans/elastic-refitting-walrus.md`
> (mirrored at G1 group approval; home-dir copy deleted). **For execution:** BULK-RUN mode — no
> per-task verify pauses; Verify scripts author into Master Test Plan §B; Work Log entry drafted +
> HELD; one source commit.

## Context

Absorption-corrected scope (run plan + 2026-07-08 surface map): Stretch-follows-tempo already works
(ratio `originalBPM/ctx.bpm`, PV always allocated). What this batch builds, per the locked G1
answers: **Resample varispeed tempo-follow**, **Shift+drag re-fit in both modes**, **true-length
import** (kills the hardcoded-120 default AND the import-time stretch it silently causes today),
the **outSamples<=0 silence-guard hardening**, and **replacing the Rubber Band no-op stub**. The
resize→rebuild trigger that BUILD-06 called missing is ALREADY WIRED (QA-Ea Task 0c;
commitEdit→onArrangementChanged→rebuildAudioClipPlayers) — the real gap is that rebuild never
re-fits, which is exactly the stub's job.

- Risk: medium-high — hot-path clip render, TWO mirrored paths (Audio bus + Vox/Inst FilePlay) that
  must stay in lockstep. Mitigations: DSP + persistence already exist; G1 ear-checks.
- Effort: ~5-8h. Dependencies: QA-TempoMap lands first in §6 order (per-span `ctx.bpm` seam);
  behavior here is defined against whatever bpm the block/span reports, so the seam is one value
  either way.
- **Bucket:** System Pages, Cross-cutting Infrastructure.

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| 2a | Remainder confirmed + block-resize re-fit is core scope | Jeff, marathon. fitRatio derives from block length — resize IS a fit trigger. |
| 2b | DUAL-TRIGGER: Resample = varispeed on tempo change AND stretch-resize (rate+pitch together); Stretch = pitch-locked on both | Jeff, marathon. |
| F | Plain right-edge drag = trim/extend (unchanged); Shift+drag = re-fit | Jeff, G1 round. Preserves both gestures; Shift+drag is the existing `mStretching` trigger (BuilderPage.cpp:4237) whose body is the no-op. |
| G | True-length import: block length = file duration in beats at the CURRENT project tempo; `originalBPM` = project tempo at import → plays 1:1, unstretched; NO rounding, no whole-bar fit | Jeff, G1 round (his correction of Claude's bar-rounding option text). Also fixes today's silent defect where any project tempo ≠ 120 stretches every import immediately. |
| 2d | Content-length/tempo-derived default replaces hardcoded 120 | Jeff, marathon, as corrected by G. |
| — | "Fit-to-grid" as a separate feature: DEAD | Follows from G — nothing rounds; tempo-follow + Shift-drag are the fit mechanisms. |
| — | Silence guard: clamp/fall back, never skip-to-silence | §5 locked scope. |

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open.

## Files to modify

### Task 1 — import
- `Source/Standalone/BuilderPage.cpp:3422-3427` (`importAudioFile` — hardcoded 120 + length math)
- `Source/Standalone/BuilderPage.cpp:3519-3524` (`placeAudioLibraryEntry` — same, library entries
  keep their OWN stored BPM when one exists; the 120 default only dies for unknown-tempo sources)

### Task 2 — Resample follow + guard (both render paths in lockstep)
- `Source/PluginProcessor.cpp:598-618` (ratio model + guard, Path A `renderAudioClipsForRow`)
- `Source/PluginProcessor.cpp:975-990` (mirror, Path B `decodeFilePlayClip`)

### Task 3 — Shift+drag re-fit
- `Source/Standalone/BuilderPage.cpp:4419-4457` (drag apply; the :4451-4454 no-op stub),
  `:4767-4773` (mouseUp commit), `:4237` (mStretching trigger — unchanged)
- `Source/PatternManager.cpp:1076-1077 / 1468-1469` (persistence — verify only; `originalBPM`
  already round-trips)

## Tasks

### Task 1 — true-length import (G)

- [ ] Both import sites: `originalBPM = (float) currentProjectBPM` (from the playhead/PatternManager
      global tempo at import time) replacing `120.f`; block length stays
      `durationSecs * (originalBPM/60.0)` beats — with originalBPM = project tempo that IS the true
      wall-clock footprint, exact, unrounded (`setLengthBeats`, tick-precision).
- [ ] `placeAudioLibraryEntry`: entries whose library `originalBPM` was USER-SET keep it (respect
      the browser master, `isOverride` semantics untouched); only the never-set/default case adopts
      the import-tempo rule. Verify which state distinguishes "user set a BPM" vs "default 120" —
      if the library can't distinguish, new entries simply get import-tempo at first add (the
      moment the file enters the library) and inherit from there.
- [ ] Net effect audible check: import at any project tempo → plays identical to the source file
      (ratio = 1, PV disengaged).

### Task 2 — Resample varispeed follow + silence-guard hardening (2b)

- [ ] Path A ratio model (PluginProcessor.cpp:598-604): Resample branch (`!stretchMode`) gets
      `varispeedFollow = ctx.bpm / originalBPM` folded into `effReadRatio`/`fileRate` (rate + pitch
      move together — vinyl); Stretch branch unchanged (already `originalBPM/ctx.bpm`, pitch
      locked). One formula comment block per Rule 6 category 5 (the ratio algebra is a
      magic-relationship calibration).
- [ ] Path B mirror (:975-990): same Resample term (Path B has no pitch/varispeed knobs — simpler
      fold). LOCKSTEP RULE: any Task 2 edit lands in both paths in the same commit.
- [ ] Guard (:618 and :990): `outSamples <= 0` → clamp to the valid remainder (recompute
      `effectiveClipEnd` against the degenerate ratio; if the content genuinely has zero samples in
      this block's window, that is silence-by-truth not silence-by-bug) — never `continue`-skip a
      clip whose window overlaps content. Degenerate-ratio clamp bounds: ratio clamped to
      [1/64, 64] before use (PV's own `mSynthHop` clamp at PhaseVocoder.cpp:44 backstops).
- [ ] `seekNeeded` (:650-660) sanity: unchanged logic, re-verify the 2 s staleness self-correct
      still fires under varispeed rates.

### Task 3 — Shift+drag re-fit (F, both modes), stub replaced

- [ ] Replace the :4451-4454 no-op: on Shift+drag, capture `L0 = mResizeOrigLen` (beats) and the
      live `L1`; on mouseUp commit, apply the fit by scaling the clip's tempo identity:
      `block.originalBPM *= (float)(L1 / L0)` — in Stretch mode the render ratio
      `originalBPM/ctx.bpm` then re-fits pitch-locked; in Resample mode the Task 2 varispeed term
      `ctx.bpm/originalBPM` re-speeds (pitch follows). One field drives both modes; it already
      persists (PatternManager.cpp:1076/1468) — no new XML.
- [ ] Live preview during the drag: cheap path = commit-on-release only (rebuild fires once);
      block paints its new length live (already does). No mid-drag audio rebuild (hot-path safety).
- [ ] Plain drag path untouched (trim/extend), `mStretching` gating exactly as today; Shift+LEFT
      edge: out of scope unless free (right edge is the shipped gesture).
- [ ] Rule 6 pass on touched regions (the stub's aspirational Rubber Band comment dies).

### Task 4 — batch close (bulk-run shape)

- [ ] Author Master Test Plan §B "QA-Ec" from the Verify scripts (`blocks:` = batch commit).
- [ ] Draft + HOLD Work Log entry in `Running Notes/elastic-refitting-walrus.md`; code-complete
      running-notes entry (records the BUILD-06 stale-claim correction + the G import-spec
      correction).
- [ ] One source commit (Rule 9): message + full status → approval → commit.

## Verify scripts (→ Master Test Plan §B; Debug first, then Release — G1 ear-check batch)

1. Project at 90 BPM, import a 2 s WAV → block spans exactly 3 beats (2 s at 90), plays identical
   to the source (no stretch artifacts, correct pitch). Readout confirms the wall-clock length.
2. Same import at 140 → different beat-length, still 1:1 playback. (The old behavior — instant
   stretch at any tempo ≠ 120 — is gone.)
3. Stretch-mode clip, change project 90 → 120: re-fits pitch-locked (same musical length, faster);
   ear-check quality.
4. Resample-mode clip, same change: speeds up AND pitches up (vinyl); ear-check.
5. Shift+drag a Stretch clip's right edge to 2x length → content plays across the full new length,
   pitch locked. Undo restores exactly.
6. Shift+drag a Resample clip to half length → double speed, pitch up an octave.
7. Plain drag right edge: trims/extends the window; playback speed unchanged (F split).
8. The old silence case: import at 120, crank tempo hard (e.g. 200) → clip still audible with
   meter (guard), correct Stretch behavior.
9. Same matrix on a Vox/Inst-routed clip (Path B lockstep): behaviors identical.
10. Save → reload after re-fits: modes, lengths, pitches identical (originalBPM round-trip).

## Routing notes (Rule 3 application during execution)

The stretch-path unmute staleness residual (self-corrects ≤2 s) is QA-J-Verify ledger territory —
do not chase here unless Task 2 breaks it. Library-entry BPM UX (user-editable clip BPM in the
Properties dialog already exists) stays as-is; findings about the Properties dialog route to QA-H/
QA-L surfaces at section pass.

## Carry-Forward Reference touch points

- §1 clip-render primitives + §4 "sum before insert DSP" context before Task 2. The 2026-07-08
  clip surface map (running-notes seed) supersedes the §5 entry's stale line refs (`:510-533`-era).
