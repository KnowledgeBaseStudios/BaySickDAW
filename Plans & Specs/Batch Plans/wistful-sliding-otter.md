# QA-SlideSliceGlide — Note-Type Slides + Note Properties + Builder Tiling/Slice + sfizz MPE Glide — Plan (wistful-sliding-otter)

> **Canonical path:** `Plans & Specs/Batch Plans/wistful-sliding-otter.md`.
> **For execution:** implementation plan for the G3-smoke findings on note-type slides, the Note
> Properties popup, Builder pattern-block ghost/tiling + slice, and sfizz/Aria glide. The design was
> locked with Jeff in a full workshop on 2026-07-20 — **every decision below is already answered; do
> NOT re-litigate or re-ask them.** Runs in a dedicated session with fresh context. Found during the
> G3 boundary smoke; routes to QA-H (slides/note-props) + QA-G (tiling/slice) via §9 back-refs, plus
> net-new scope for the sfizz glide (see Routing).

## Context

Jeff hit these testing note-type slides + the Note Properties popup on an instrument tab during the
G3 boundary smoke (2026-07-20), then flagged the Builder pattern-block ghost/tiling + slice and the
sfizz glide. All root causes are source-verified below (two research passes 2026-07-20 + a direct
re-verification of the mid-note-cut premise after the researcher got it wrong — see B-3).

**Risk:** high — touches the slide DSP across 4 engines, engine-wide panning, the Builder tiling +
slice model, and adds MPE routing for sfizz. **Effort:** ~20-30h honestly (this is a large batch;
six work tasks). **Dependencies:** the 12 G3 boundary review-fixes + the QA-L-Fix drum-trigger work
may already be in the tree from prior sessions — do NOT disturb them; confirm the tree at open.

## FL Studio reference (locked facts, manual-sourced 2026-07-20)

- FL has exactly ONE piano-roll slide: a silent controller note that bends the already-playing note
  as **one continuous voice** over the overlap, reaching target pitch at the slide note's end. That
  is our **RP**. FL's *retrigger* behavior is a separate channel Porta/Mono setting — our RT/Porta
  are our own types. Slide events carry the usual note properties; during a slide every property
  interpolates from base toward the slide note's value. Sources: Image-Line manual (Piano Roll +
  Misc Channel Settings pages).
- Song-mode arrangement: FL reads pattern **clips as placed** — a clip is a *window* into the
  pattern (offset + length), **no copy**. Slicing/moving a clip changes the window; a note crossing
  the window boundary plays its fragment from the boundary at read time. **Our `ArrangementBlock`
  already works this way** (`patternIndex` + `contentOffsetTicks`) — see B-3.

## Root causes — source-verified (do NOT re-derive)

**Slides** — the piano-roll scheduler `scheduleRollWindows` (Source/PluginProcessor.cpp:2300-2359),
shared by song + pattern modes. `emitPianoNoteOn` (:58-99), `emitRampSlide` (:182-193). NoteType
enum `{Standard, RampSlide, Portamento, RetrigSlide}` at PatternManager.h:69; `PianoNote.type` :80.
- **Issue 1 (same-start does nothing):** `findGlideSourcePitch` (:112) + `findRampAnchorPitch`
  (:164) require the source note to start *strictly before* the slide note, so a co-starting base
  note is excluded → no source. RP then hits its `if (anchor >= 0)` silence guard (:2333).
- **Issue 2 (two voices on RT/Porta):** RP is a takeover bend (:2326-2338 — no noteOn, retargets the
  anchor voice, anchor note-off extended). RT/Porta call `emitPianoNoteOn` (:2350) → a fresh voice
  at the target, and the scheduler **never cuts the base note** (no mono-cut anywhere on the
  retrigger path — confirmed in all 3 voice classes).
- **Issue 3 (RT "too fast"):** not a real timing bug — RT glide time already = the slide-note length
  (:2347). The perception is the twin-voice from Issue 2; fixing Issue 2 resolves it.
- **Issue 4 (Porta near-instant):** the Porta branch leaves glideMs = -1 (:2341), so voices fall
  back to a hard ~60 ms (BaySickSynthVoice.cpp:53, AdditiveVoice.cpp:124, VibePlayerDSP.cpp:843).
- **Issue 5 (properties don't affect a slide):** (A) RP goes through `emitRampSlide` and never
  reaches `emitPianoNoteOn`, so it emits ONLY the pitch transport — no velocity/pan/finePitch/
  cutoff/reso/release. (B) **Panning is dead for EVERY note on these engines** — pan is emitted as
  CC10 (:65) but NO voice consumes CC10 (BaySickSynthVoice::controllerMoved has no cc==10 case; same
  across engines) and there is no per-voice pan stage. RT/Porta DO deliver their other properties;
  their apparent failure is just Issue 2's twin voice.
- **Issue 6 (no double-click default):** the 6 sliders in NotePropsPanel (PianoRoll.cpp:1406-1422)
  never call `setDoubleClickReturnValue`.
- **Issue 7 (no Close button):** NotePropsPanel builds only the type buttons + 6 rows and sizes to
  them (PianoRoll.cpp:1437, :1450); dismiss is click-outside only. The dtor commit (:1440) fires on
  any dismissal, so a Close button inherits it free.

**Builder tiling (B1)** — ONE root cause: `Pattern.bars` defaults to `DEFAULT_BARS=4`
(PatternManager.h:180, VibesynthConstants.h:31) and is **never grown from note content** (no
piano-roll edit writes it; only XML-load fallback + pattern-dup do). Both surfaces read it as the
tile length: song playback `cycleBeats = jmax(1.0, sPat.bars * patBpb)` (PluginProcessor.cpp:2419)
and the ghost preview `cycleBars = jmax(1, pat.bars) * beatsPerBar / 4.0` (BuilderPage.cpp:2333),
plus a cull `if (noteBar >= cycleBars) continue;` (BuilderPage.cpp:2363) that drops >4-bar notes
from the drawing. `getEffectivePatternLoopBeats()` (PatternManager.cpp:929-1022) already computes
the correct content length (furthest note-end, bar-ceiled, min 1 bar) but is hardcoded to
`mCurrentPattern`.

**Slice** —
- **B2a (roll cuts an infinite line):** `sliceNotesOnLine` (PianoRoll.cpp:960; the fault at :975-986)
  interpolates the cut X at each note's vertical centre but **never clamps `t` to [0,1]** nor checks
  the note lies between the endpoints — so the drawn segment is treated as an unbounded line. `git
  blame` shows this geometry is present since the **initial commit** — a LATENT bug, NOT a QA-G/QA-L
  regression. (Correct the record: my earlier "it regressed" was wrong.)
- **B2b (Builder slice):** the Slice tool IS wired and DOES split blocks with content-offset
  continuation (QA-G, BuilderPage.cpp:5337-5410). It "does nothing" because (1) it never cuts notes,
  (2) a guard (:5346) rejects short/edge blocks, (3) a successful split has no visible seam. **The
  note-cut does NOT require copying the pattern** — see B-3.
- **Shift-snap:** neither path reads Shift today. Roll hook = `mouseDrag` (PianoRoll.cpp:1995-2003,
  anchor :1749). Builder is a single click-split with no drag-line (ArrangementGrid `mouseDrag`
  :5682 ignores Slice) — needs a drag-line first.

**sfizz/Aria glide (A1)** — slides ride an in-house CC protocol (CC84 source, CC5/37 time, CC85
target) decoded ONLY by our voices (VibePlayer decodes it at VibePlayerDSP.cpp:1433). The sfizz
engines (BaySickGuitars/Basses) receive every CC generically (GuitarsProcessor.cpp:207,
BassesProcessor.cpp:210) — **no drop, no scope-out guard** — but have no decoder, so the CCs sit
inert. sfizz's only continuous-pitch path is the MIDI pitch-wheel, scaled by `bend_up`/`bend_down`
(default ±200 cents = ±2 semitones; libs/sfizz Defaults.cpp:126); it has **no portamento opcode**.
Documented QA-H scope-out (test plan H-3, ghostly-riffing-moth.md:166). So this is net-new scope.

## Locked decisions (Jeff, 2026-07-20 workshop — do NOT re-ask)

### Slides
| ID | Decision |
|----|----------|
| S-1 | **Issue 1:** relax the strict-before test in `findGlideSourcePitch` + `findRampAnchorPitch` so a note starting at-or-before the slide note counts as the source; the **non-slide** note is the source. RP's silence guard clears once a source exists. |
| S-2 | **RP** keeps its current takeover-bend behavior (one voice, no retrigger, bends over the block) — unchanged EXCEPT it now emits expression (S-6). |
| S-3 | **RT** = **cut the base note** + retrigger (fresh attack at the previous note's pitch) + glide to the target over the **block length**. One voice. |
| S-4 | **Porta** = same single-voice (cut base + retrigger) but glides over a per-note **"Porta Length In Beats"** value, **ignoring the block length** — uses only the note's start point. New per-note field, default **1 beat**. |
| S-5 | The **base-note cut** for RT + Porta = a mono-cut on the retrigger path: stop the glide-source voice at the moment the slide note starts (advance its note-off to the slide start). RP does NOT cut (it reuses the source voice). |
| S-6 | **Issue 5A:** the RP path also emits the note's per-note expression (velocity/pan/finePitch/cutoff/reso/release) — mirror `emitPianoNoteOn`'s expression block. |
| S-7 | **Issue 5B (app-wide):** add a CC10 pan consumer + a per-voice pan stage to each engine voice (BaySickSynth, BaySickBass, Harmless/AdditiveVoice, VibePlayer). Fixes panning for ALL notes, not just slides. |
| S-8 | **Issue 6:** double-click-return-to-default on the 6 Note Properties sliders + the new Porta Length box. Neutral defaults: Velocity 80, Release 50, Fine Pitch 0, Panning 0, Filter Cutoff 50, Resonance 50, Porta Length 1 beat. |
| S-9 | **Issue 7:** add a **Close** button at the bottom of the Note Properties popup (dismisses the CallOutBox; the dtor commit already fires on dismiss). Grow the panel height one row. |
| S-10 | The **"Porta Length In Beats"** control lives in the Note Properties popup, a type-in box like the BPM box, **greyed/disabled unless the note type is Porta**. |

### Builder
| ID | Decision |
|----|----------|
| B-1 | **Tiling cycle = the pattern's REAL content length, note-for-note** (an 8-bar pattern loops every 8, a 2-bar every 2). Generalize `getEffectivePatternLoopBeats` to a per-`patternIndex` variant and feed BOTH surfaces: the scheduler uses the beats value directly (PluginProcessor.cpp:2419); the preview uses beats/4 for its uniform-bar space (BuilderPage.cpp:2333) — which also fixes the :2363 cull. `Pattern.bars` stays as a field but is no longer the tile length (do not spend effort retiring it). |
| B-2 | **Roll slice = finite segment.** Add a vertical-bounds check in `sliceNotesOnLine` (reject notes whose vertical centre is outside `[min(start.y,end.y), max(...)]`, i.e. `t` outside `[0,1]`) so only notes crossed by the drawn segment between the two dots are cut. |
| B-3 | **Builder mid-note cut = Decision 1 = A, NO copy.** Blocks stay windows into the shared pattern (`patternIndex` + `contentOffsetTicks` — already so). The ONLY change: `scheduleRollWindows` currently **drops** notes that start before the window (`if (absStart < contentLo ...) continue;`, PluginProcessor.cpp:2309, "QA-G Task 5"). Change that to **clamp-and-play**: a note crossing the window's left edge emits at the boundary with its remaining duration (the existing `offHi` already clamps the right edge). Same change in the preview cull (BuilderPage.cpp:2363). No pattern copies, no clones; a sliced piece keeps following the original pattern's edits. |
| B-4 | **Builder slice made real:** (i) convert the click-split into a **drag-line** (add `mSliceStart/mSliceEnd/mSlicing` to ArrangementGrid, two-dot preview in paint, apply on mouseUp); (ii) fix the guard/snap (:5346) so short blocks are sliceable; (iii) give split pieces a **visible seam**; (iv) the notes cut via the B-3 windowing (per-piece `contentOffsetTicks`), FL-style mid-note. |
| B-5 | **Shift-snap = Decision 2 = the active snap-div grid** (sub-bar allowed, NOT bar-lines only). Roll: an `isShiftDown` branch in `mouseDrag` forces the line vertical (both endpoints' X = the snap-div-snapped X under the cursor) and drives the cut extent from raw `p.y`. Builder: the same branch after the B-4 drag-line conversion. B-2 (finite segment) is a prerequisite. |

### sfizz/Aria
| ID | Decision |
|----|----------|
| A-1 | **Decision 3 = B = full per-voice parity via MPE.** Translate the CC84/CC5/CC85 glide transport into **per-note pitch-bend ramps on MPE channels** for BaySickGuitars/Basses (each sounding note on its own channel; ramp its channel's pitch-wheel from the source→target delta over the glide time; RT/Porta noteOn at target then bend in, RP holds + bends). Set a **wide bend range** (the ±2-semitone default clips wider slides — push `bend_up`/`bend_down` or a global range). **REQUIRED first step of this task:** confirm our vendored sfizz build honors multi-channel / MPE pitch-bend cleanly (per-note-per-channel) BEFORE building the ramp layer; if it cannot, STOP and surface it to Jeff as a spec call (fall back to the simple channel-bend version or defer). |

## Files to modify (by task)

- **Task 1 (slide DSP):** Source/PluginProcessor.cpp (scheduler :2300-2359, `findGlideSourcePitch` :104, `findRampAnchorPitch` :153, `emitPianoNoteOn` :58, `emitRampSlide` :182, the base-note mono-cut); Source/PatternManager.h (`PianoNote` — new `portaLengthBeats` field + serialization in PatternManager.cpp).
- **Task 2 (slide expression + panning):** Source/PluginProcessor.cpp (RP expression emit); Source/BaySickSynth/BaySickSynthVoice.h/.cpp, Source/Harmless/AdditiveVoice.h/.cpp, Source/VibePlayer/VibePlayerDSP.h/.cpp (+ BaySickBass shares BaySickSynthDSP) — CC10 consumer + per-voice pan stage.
- **Task 3 (Note Properties popup):** Source/Standalone/PianoRoll.cpp (NotePropsPanel :1378-1465 — Porta Length box + greying, double-click defaults, Close button, +1 row height).
- **Task 4 (tiling):** Source/PatternManager.cpp/.h (`getEffectivePatternLoopBeats` → per-index variant, e.g. `getPatternContentBeats(int idx)`); Source/PluginProcessor.cpp:2419; Source/Standalone/BuilderPage.cpp:2333 + :2363.
- **Task 5 (slice):** Source/Standalone/PianoRoll.cpp (`sliceNotesOnLine` :960-1000 finite-segment; `mouseDrag` :1995 shift-snap; snap helpers :522/:538); Source/PluginProcessor.cpp:2309 (contentLo clamp-and-play, B-3); Source/Standalone/BuilderPage.cpp:2363 (preview clamp) + :5337-5410 (drag-line slice, guard/snap, seam, per-piece window note-cut) + ArrangementGrid mouseDrag :5682 + paint.
- **Task 6 (sfizz MPE glide):** Source/BaySickGuitars/BaySickGuitarsProcessor.cpp (:207 CC path → MPE channel bend), Source/BaySickBasses/BaySickBassesProcessor.cpp (:210); the scheduler's per-target emit for sfizz Inst pages (PluginProcessor.cpp :2491/:2554 gate); libs/sfizz bend-range setup; MPE channel allocation.

## Tasks

### Task 1 — Slide engine (Issues 1-4, S-1/3/4/5)
- [ ] S-1: relax strict-before in both source resolvers; source = nearest non-slide note starting at-or-before the slide note.
- [ ] S-3: RT — cut the base/source note at the slide note's start (advance its pending note-off to the slide start), keep the retrigger + note-length glide.
- [ ] S-4/S-10: add `PianoNote.portaLengthBeats` (default 1) + XML serialization; Porta uses it (converted to ms at the note's tempo) for glide time, ignoring the block length; single-voice (cut base) like RT.
- [ ] S-5: implement the mono-cut once, shared by RT + Porta. RP untouched (no cut).
- [ ] Build gate (Jeff runs do_build.bat) → fix clean → `/draft-doc running-notes`.

### Task 2 — Slide expression + app-wide panning (Issue 5, S-6/S-7)
- [ ] S-6: RP path emits the note's expression CCs (mirror `emitPianoNoteOn`'s CC10/pitchwheel/CC74/CC71/CC72 block).
- [ ] S-7: add a CC10 pan consumer to each engine voice + a per-voice pan stage in the render loop (BaySickSynth, BaySickBass, Harmless, VibePlayer). Verify pan works for a plain note first, then a slide.
- [ ] Build gate → `/draft-doc running-notes`.

### Task 3 — Note Properties popup (Issues 6-7, S-8/S-9/S-10)
- [ ] S-10: add the "Porta Length In Beats" type-in box, greyed/disabled unless the note type is Porta; wired to `portaLengthBeats`.
- [ ] S-8: `setDoubleClickReturnValue` on all sliders + the box, with the neutral defaults listed in S-8.
- [ ] S-9: Close button at the bottom, dismisses the CallOutBox; grow panel height one row.
- [ ] Build gate → `/draft-doc running-notes`.

### Task 4 — Builder pattern-block tiling (B-1)
- [ ] Generalize `getEffectivePatternLoopBeats` to `getPatternContentBeats(int patternIndex)` (furthest note-end across that pattern's rolls, bar-ceiled at its bpb, min 1 bar).
- [ ] Scheduler (PluginProcessor.cpp:2419): `cycleBeats` = that value (beats). Preview (BuilderPage.cpp:2333): `cycleBars` = value/4; the :2363 cull auto-fixes once the cycle is real.
- [ ] Verify: an >4-bar pattern plays + previews its full length in song mode, looping at its real length; a stretched block tiles the real content.
- [ ] Build gate → `/draft-doc running-notes`.

### Task 5 — Slice (B-2, B-3, B-4, B-5)
- [ ] B-2: finite-segment cut in `sliceNotesOnLine` (vertical-bounds / `t in [0,1]` check).
- [ ] B-3: change the `contentLo` drop (PluginProcessor.cpp:2309) to clamp-and-play straddling notes; same in the preview cull (BuilderPage.cpp:2363). No copies.
- [ ] B-4: Builder slice → drag-line (state + preview + mouseUp apply); fix the short-block guard/snap; visible seam; notes cut via B-3 windowing (per-piece `contentOffsetTicks`).
- [ ] B-5: Shift-snap (snap-div grid) on the roll `mouseDrag`, then the Builder drag-line.
- [ ] Verify: roll slice cuts only between the two dots; Shift = vertical, snapped to the snap-div; Builder slice splits a stretched block mid-note and each piece plays its fragment (no copy, still tracks the source pattern).
- [ ] Build gate → `/draft-doc running-notes`.

### Task 6 — sfizz MPE glide (A-1)
- [ ] **FIRST:** confirm the vendored sfizz honors per-note-per-channel (MPE) pitch-bend. If not → STOP + surface to Jeff (spec call). Log the finding in running notes either way.
- [ ] Set a wide bend range (bend_up/bend_down or global) so slides > 2 semitones don't clip.
- [ ] MPE channel allocation for sfizz slide notes + translate CC84/CC5/CC85 into per-note pitch-bend ramps (RT/Porta noteOn at target + bend in; RP hold + bend). Non-slide notes unaffected.
- [ ] Verify on BaySickGuitars + BaySickBasses: mono line slides cleanly; a chord slides per-voice without dragging non-slide notes.
- [ ] Build gate → `/draft-doc running-notes`.

### Task 7 — Close (bulk-run)
- [ ] Author the Master Test Plan scenarios (R4): **supersede** the stale slide/note-prop scenarios in §B.14 (H-2, H-3, H-4, H-5) and the tiling/slice scenarios in §B.13, and add the new scenarios covering S-1..S-10, B-1..B-5, A-1 (Debug-first/Release, PASS/FAIL, `blocks:` this batch's commit). Physically-executable gestures from the actual components.
- [ ] `/review-batch` over the diff.
- [ ] Draft + HOLD the Implemented Work Log entry in the running notes (applies at the campaign pass, R2).
- [ ] Surface the Rule-9 commit one-liner + FULL `git status` (every entry + disposition) → commit only on Jeff's explicit approval.
- [ ] Do NOT run the boundary smoke; do NOT touch the boundary review-fixes or the QA-L-Fix work.

## Verification

Per bulk-run R4 the scenarios author into the Master Test Plan at code-complete (Task 7), not run
mid-batch. **Do NOT ask Jeff to test mid-batch** — build gates only. Verification happens at the §B
campaign pass + Jeff's smoke.

## Routing notes (Rule 3)

- §9 Forks entries at doc-close: back-ref **QA-H** (the slide-type + Note Properties redesign — RP/RT/
  Porta semantics, the base-note cut, the Porta-length control, the panning-app-wide bug, popup UI)
  and **QA-G** (tiling content-length + slice finite-segment + mid-note windowing + Shift-snap). The
  **sfizz MPE glide is NET-NEW scope** (QA-H deliberately scoped sfizz slides out per H-3) — record
  it as a new §9 entry, not a QA-H back-ref.
- Correct the record in the QA-G §9 entry: the roll-slice finite-segment bug is **latent since the
  initial commit**, not a QA-G regression.
- Any NEW spec call mid-execution → ask Jeff in chat immediately (numbered options, no
  recommendations). The A-1 sfizz-MPE-capability check is the most likely to surface one.

## Carry-Forward Reference touch points

- §1-3 (architectural primitives, file index) at Task 1 + Task 5 + Task 6 start (audio-thread
  scheduler + slice model + sfizz MIDI path).
- CLAUDE.md "Source Layout" for the engine families + the 96-PPQ tick model (`kTicksPerBeat = 96`).
