# Running Notes — QA-SlideSliceGlide (wistful-sliding-otter)

> **Purpose:** append-only running log for the slide / Note-Properties / Builder-tiling-slice / sfizz-MPE-glide fix.
> Append at EVERY checkpoint — a build gate cleared, a finding captured, a spec call asked/resolved,
> a scope pivot, a commit landed (`feedback_draft_doc_running_notes_every_checkpoint`). At close,
> `/draft-doc batch-close` reads this as the primary input for the Implemented Work Log entry, which
> is drafted + HELD here until the campaign pass (bulk-run R2).
>
> **Pair file:** [`Plans & Specs/Batch Plans/wistful-sliding-otter.md`](../Batch Plans/wistful-sliding-otter.md)
> **Conventions:** Main Plan §0 (three-doc system, Rules 1-9; Batch Plans + Running Notes layout).

## 2026-07-20 — Batch opened (design locked, not yet executed)

Design workshopped + locked with Jeff on 2026-07-20 at the G3 boundary smoke; the full S-1..S-10 /
B-1..B-5 / A-1 decision tables + source-verified root causes live in the plan file.

Two research passes fed the plan (slides report + Builder/Aria report), plus a direct
re-verification of the mid-note-cut premise: the first research pass wrongly claimed FL-style
mid-note slicing "requires forking the pattern per slice." **That was wrong** — confirmed against the
code (`ArrangementBlock` is already a `patternIndex` + `contentOffsetTicks` window into the shared
pattern; the only thing dropping straddling notes is the deliberate `contentLo` cull at
PluginProcessor.cpp:2309). B-3 = clamp-and-play at read time, **no copies**. Recorded so the
executing session doesn't repeat the wrong premise.

Also corrected: the roll-slice finite-segment bug is **latent since the initial commit**, not a
QA-G/QA-L regression (git blame). Routes as a QA-G §9 entry with that correction.

**Scope decisions locked:** RT = cut-base + retrigger + block-length glide; Porta = cut-base +
retrigger + per-note "Porta Length In Beats" (default 1, ignores block); slice mid-note = A / no
copy; Shift-snap = active snap-div grid; sfizz glide = D3=B full per-voice MPE (with a required
capability-check first step in Task 6).

**State at open:** confirm the working tree first — the 12 G3 boundary review-fixes and possibly the
QA-L-Fix drum-trigger work may already be dirty from prior sessions; **do not disturb them.**

**Resume action:** confirm tree, then execute Task 1 of the plan file.

## 2026-07-20 — Task 1 — Slide engine (S-1/S-3/S-4/S-5) — code complete, awaiting build gate

Tree confirmed at open: dirty by design and fully accounted for — G3 boundary review-fixes
(QA-OctavePedal / `locked-doubling-frog`, ~18 Source files + 2 doc stragglers), QA-L-Fix drum-trigger
(`eager-thumping-marmot`: new `Source/MidiLearn/DrumTriggerMap.cpp/.h`, `MidiLearn/*`, `DrumKitGrid.*`,
`DrumPage.*`, `CMakeLists.txt`, + 2 doc stragglers), the test-plan straggler `v1-master-test-plan.md`,
and this batch's own plan+notes. Nothing unexplained; nothing disturbed. Several Task targets are already
dirty (PluginProcessor.cpp, PianoRoll.cpp, BuilderPage.cpp/.h, BaySickSynthVoice.h, AdditiveVoice.h) so
plan line numbers have shifted — every edit located by symbol/content, stacked on top of the prior work.

**Source verification before editing:**
- Voice glide protocol confirmed (CC5/37 = 14-bit ms glide time, CC84 = source note, CC85 = ramp
  bend target; fallback 0.06f = 60 ms = Issue 4) in VibePlayerDSP.cpp / BaySickSynthVoice.cpp /
  AdditiveVoice.cpp. Passing a real Porta `glideMs` overrides the 60 ms fallback — **voices need no
  change for Task 1.**
- PianoNote has NO positional brace-inits past field 7 (`type`) anywhere in Source/ — appending the
  new field at the struct end is safe (honors the "KEEP THESE LAST" caveat).

**Changes:**
- `PatternManager.h` — new `double portaLengthBeats { 1.0 }` at the PianoNote struct end.
- `PatternManager.cpp` — serialize as `"pl"` (write if != 1.0; read default 1.0).
- `PluginProcessor.cpp` `findGlideSourcePitch` (S-1) — strict-before → at-or-before; co-start tie
  prefers a Standard note over a slide (base wins).
- `PluginProcessor.cpp` `findRampAnchorPitch` (S-1) — same at-or-before relaxation, co-start tie
  prefers non-RampSlide so the back-walk terminates.
- `PluginProcessor.cpp` `scheduleRollWindows` dispatch — Porta glide time now = `portaLengthBeats`
  (S-4, was -1 → 60 ms), RT glide time stays = slide-note length (S-3). Added a shared **mono-cut**
  (S-5) for RT + Porta: record `(glideFrom, smp)` in a stack-local array during the note-on loop,
  emit the `noteOff`s AFTER the loop so a cut at a slide's start lands after its source's same-sample
  note-on regardless of note-vector order (MidiBuffer keeps equal-timestamp events in insertion
  order). RampSlide untouched (breaks before the glide block — RP does not cut, per S-5). Skipped
  when `glideFrom == target` (degenerate) so the fresh voice is never cut.

**Design note (why post-pass, not in-loop):** a raw in-loop `noteOff(glideFrom)` at the slide's start
sample can land BEFORE the source's `noteOn` when the slide precedes the source in the notes vector and
they co-start (same sample) — MidiBuffer orders equal-timestamp events by insertion, so the source
would out-live the cut (twin voice). Emitting all cuts after every note-on guarantees the cut is last
at any shared sample. Allocation-free (stack array, cap 64 slide note-ons/roll/block).

**Diagnostics added:** none (no DBG/Logger/jassert). Rule 4 catalog empty for Task 1.

**Follow-up (Jeff's build-gate question, same session):** Jeff asked whether RP should also glide when
the source and slide co-start now. Yes — S-1 explicitly clears RP's silence guard "once a source
exists," and the `findRampAnchorPitch` relaxation makes a co-starting base qualify as the anchor. But
the question exposed that the RP takeover emit (`emitRampSlide`) had the SAME same-sample ordering
fragility the mono-cut had: the CC84/CC85 bend only grabs the base voice if the base's note-on precedes
it at the shared sample, which in-loop depended on note-vector/draw order. Fixed by **deferring the RP
bend emit to the same post-pass** as the mono-cuts (new `rampBends[64]` stack array) so it always lands
after every note-on. Behavior-preserving for the pre-existing strict-before anchor case (anchor already
sounding from a prior sample); only newly-robust for co-start. Not a new spec call — implementing S-1
correctly. This lands in the same Task 1 build gate.

**Resume action:** Jeff runs `do_build.bat`. If clean → append "Task 1 build clean" + start Task 2. If
errors → diagnose + fix (no reverting the prior-session work).

**Build gate: CLEAN** (Jeff, 2026-07-20). Task 1 done + verified. Includes the RP co-start robustness
fix. Moving to Task 2.

## 2026-07-20 — Task 2 — Slide expression + app-wide panning (S-6/S-7) — in progress

Scope: S-6 = RP path emits the note's expression CCs; S-7 = CC10 pan consumer + per-voice pan stage in
BaySickSynth, BaySickBass (shares BaySickSynthDSP), Harmless/AdditiveVoice, VibePlayer (fixes panning
for EVERY note, not just slides — Issue 5B: pan was emitted as CC10 but no voice consumed it).

**Source verification (CC routing):**
- BaySickSynth + BaySickBass + Harmless all use `BroadcastSynthesiser` (broadcasts every CC to every
  voice incl. idle) — CC10 already reaches the voices; only a `cc==10` handler was missing.
- VibePlayer uses `juce::Synthesiser` with a MANUAL CC dispatch filter (VibePlayerDSP.cpp:1436) that
  listed 5/37/71/72/74/84/85 — **it dropped CC10 too**. Added `num == 10` to the filter.
- BaySickBass confirmed to wrap `BaySickSynthDSP` → uses `BaySickSynthVoice`; editing the synth voice
  covers bass (no separate bass-voice edit).

**S-6 done:** extracted the shared 5-CC expression block from `emitPianoNoteOn` into a new
`emitNoteExpression` helper; `emitRampSlide` now takes the `PianoNote` and calls it before the glide
transport, so RP carries pan/finePitch/cutoff/resonance/release. `RampBend` struct now stores a
`const PianoNote*` so the deferred emit has the note. Target arg dropped (uses `note.midiNote`).

**S-7 done (per-note pan, "live" channel-wide model like the existing CC74 cutoff, center-preserving
balance law so a centered note is bit-identical to pre-pan output):**
- BaySickSynthVoice.h/.cpp — `mNotePan` + `cc==10` + balance at the L/R write.
- AdditiveVoice.h/.cpp — `mNotePan` + `cc==10` + balance composed with `mMasterPanL/R` + mod-pan.
- VibePlayerDSP.h/.cpp — `mNotePan` + `cc==10` (inline controllerMoved) + dispatch-filter `+CC10` +
  balance composed with `mPanL/R` at the stereo `addFrom` (mono output branch left unpanned).

**Design note (pan value routing is channel-wide last-wins):** the CC10 value is broadcast to all
voices on the channel, so a chord with mixed per-note pans resolves as "last note wins" for the pan
VALUE — same accepted limitation the existing CC74 cutoff / CC71 reso ships with (per the
`emitPianoNoteOn` header comment). The per-VOICE pan STAGE (S-7's ask) is what applies in the render
output. True per-note-pan-in-a-chord needs MPE (that is Task 6's domain for the sfizz engines).

**SPEC CALL surfaced (RP velocity) — see chat, awaiting Jeff:** S-6 lists "velocity" among the RP
expression props, but velocity is the noteOn byte and RP is a takeover with NO noteOn (S-2), so it
can't ride the expression block. The other 5 props do. FL interpolates loudness during a slide, so
there's a real choice: (A) leave RP at the base attack loudness [matches "mirror the expression block"
literally, zero new code]; (B) step the takeover voice to the slide note's loudness at the slide start
[a loudness CC + per-voice apply, like pan]; (C) full FL-style loudness ramp base->slide over the glide
time [ramped, more work]. Posed numbered/lettered, no recommendation. Holding the Task 2 build gate on
the answer so any velocity work lands in the same build.

**SPEC CALL RESOLVED — Jeff picked (C):** full FL-style loudness ramp, base note's level -> slide
note's level over the glide time. Implemented via a new **CC86 = slide target loudness** transport
(verified free across Source/), emitted by `emitRampSlide` just before CC85:
- BaySickSynth/Bass + Harmless (per-sample render loops): ramp the engine's NATIVE velocity value
  (`mCurrentVelocity` / `mNoteVelocity`) from base to target over the glide, so amplitude AND the
  velocity-tracked filter both interpolate (most FL-faithful — "every property interpolates"). Members
  `mSlideTargetVel/mVelRampTarget/mVelRampStep/mVelRampLeft`; armed in the CC85 takeover block (reuses
  `glideSamples`); advanced per-sample at the top of the render loop; reset at `startNote`.
- VibePlayer (block `addFrom`, velocity CURVE): can't ramp a native per-sample velocity, so it ramps
  `mVelocityScale` toward a target computed through the engine's own velocity curve
  (`volMix(targetVel)/volMix(baseVel)` ratio, `mNoteVel` stored at `startNote`), applied to `mTmpBuffer`
  via `applyGainRamp` per segment for intra-block smoothness (`velGain=1.0` in the addFrom so it isn't
  double-counted). Reuses `mGlideSamplesLeft` as the ramp length.

CC86 fires ONLY for RP (RampSlide via `emitRampSlide`); RT/Porta velocity already rides their fresh
`noteOn`. Default RP velocity (0.8) == default base (0.8) => ratio 1 => no loudness change unless the
user sets a different velocity on the slide note (correct FL behavior). No audio-thread allocations
(all stack/member; `std::pow` only in the once-per-slide CC85 handler).

**Diagnostics added:** none. Rule 4 catalog still empty.

**Files touched (Task 2):** PluginProcessor.cpp (emitNoteExpression/emitRampSlide/RampBend/CC86);
BaySickSynth/BaySickSynthVoice.h+.cpp; Harmless/AdditiveVoice.h+.cpp; VibePlayer/VibePlayerDSP.h+.cpp.

**Resume action:** Jeff runs `do_build.bat` (Task 2 build gate). Clean -> start Task 3. Errors -> fix.

**Build gate: CLEAN** (Jeff, 2026-07-20). Task 2 done + verified (S-6 + S-7 + option-C loudness ramp).

## 2026-07-20 — Task 3 — Note Properties popup (S-8/S-9/S-10) — in progress

Scope: S-10 = "Porta Length In Beats" type-in box, greyed unless the note type is Porta, wired to
`portaLengthBeats`; S-8 = `setDoubleClickReturnValue` on the 6 sliders + the box (neutral defaults:
Velocity 80, Release 50, Fine Pitch 0, Panning 0, Filter Cutoff 50, Resonance 50, Porta Length 1);
S-9 = Close button at the bottom (dismisses the CallOutBox; dtor commit already fires on dismiss),
grow the panel one row. All in Source/Standalone/PianoRoll.cpp NotePropsPanel.

**Source verification:** the "BPM box like" reference (S-10) = `GlobalTransportBar` `mBpmField`, a
`juce::TextEditor` with `setInputRestrictions(6, "0123456789.")` + `onReturnKey`/`onFocusLost`. Matched
that widget (both callbacks confirmed present in the vendored JUCE via the BPM field's own usage).

**Done:**
- S-8: `addRow` now takes a `dflt` and calls `setDoubleClickReturnValue(true, dflt)` on each of the 6
  sliders. Defaults: Velocity 80, Release 50, Fine Pitch 0, Panning 0, Filter Cutoff 50, Resonance 50
  (all == the PianoNote neutral defaults). The Porta box double-click resets to 1.
- S-10: new `NoteNumberBox` (a `juce::TextEditor` subclass) as a BPM-box-style numeric type-in, wired
  to `portaLengthBeats` (onReturnKey/onFocusLost parse+clamp 0..64 and apply to all targets; text
  reflected via a trailing-zero-trimmed `formatBeats`). Enabled + full-alpha only when the note type is
  Porta (greyed 0.5 alpha + disabled otherwise) via `reflectType`, so it also updates live on a type
  switch inside the popup.
- S-8 for the box: a `mouseDoubleClick` override on `NoteNumberBox` fires `onDoubleClickReset` (a plain
  TextEditor's double-click word-select is useless for a single number) -> sets "1" + applies, matching
  the sliders' double-click-reset gesture.
- S-9: "Close" `TextButton` at the bottom, `findParentComponentOfClass<CallOutBox>()->dismiss()` (the
  panel dtor already commits the single undo edit on dismiss).
- Panel grew 7 -> 9 rows (`kRowH * 9`): +1 Porta row (S-10), +1 Close row (S-9).

**Diagnostics added:** none.

**Resume action:** Jeff runs `do_build.bat` (Task 3 build gate). Clean -> start Task 4. Errors -> fix.

**Build gate: CLEAN** (Jeff, 2026-07-20). Task 3 done + verified.

## 2026-07-20 — Task 4 — Builder pattern-block tiling (B-1) — code complete, awaiting build gate

**Done:**
- `PatternManager.cpp/.h` — extracted `getEffectivePatternLoopBeats()` body into a new
  `getPatternContentBeats(int patternIndex)` (same furthest-note/step-end, bar-ceiled-at-pattern-bpb,
  min-1-bar logic, parameterized by index instead of `mCurrentPattern`); `getEffectivePatternLoopBeats()`
  now just delegates `getPatternContentBeats(mCurrentPattern)` (zero behavior change for the current-
  pattern callers). Verified `getPatternBeatsPerBar(idx)` == the scheduler's inline `tsNum*4/tsDen`, so
  the bpb semantics are preserved.
- `PluginProcessor.cpp` (song-mode tiling) — `cycleBeats` now = `getPatternContentBeats(blk.patternIndex)`;
  removed the now-dead inline `patBpb` computation.
- `BuilderPage.cpp` (ghost preview) — `cycleBars` now = `getPatternContentBeats(b.patternIndex)/4.0`; the
  `>4-bar note cull` (`noteBar >= cycleBars`) self-fixes since the cycle is now the real content length.
  Kept `beatsPerBar` + its `<= 0` guard untouched (minimal change).

**Perf note:** in song mode the scheduler now calls `getPatternContentBeats` (an O(notes) scan) per
active block per processBlock instead of an O(1) `bars*bpb`. Call count is low (~1-2 active blocks/block)
and the scheduler ALREADY iterates the same note vectors on the audio thread (`sched(sPat.*Roll...)`), so
it is consistent with the existing threading model and not a new RT-safety concern. A per-pattern content-
beats cache (invalidated on edit) is the clean optimization if a perf-audit flags it later — NOT in scope
for B-1 (plan says "uses the beats value directly").

**HANDOFF to Task 5:** the Builder slice split (BuilderPage.cpp ~5370) wraps `contentOffsetTicks` by a
`cycleTicks` still derived from `pat.bars`. Task 5 (B-3/B-4) must align that to the content length so a
sliced piece's window matches the new tiling cycle. Unsliced blocks (offset 0) are unaffected now.

**Diagnostics added:** none.

**Resume action:** Jeff runs `do_build.bat` (Task 4 build gate). Clean -> start Task 5. Errors -> fix.

**Build gate: CLEAN** (Jeff, 2026-07-20). Task 4 done + verified.

## 2026-07-20 — Task 5 — Slice (B-2/B-3/B-4/B-5) — in progress (biggest task)

Sub-parts: B-2 finite-segment cut in `sliceNotesOnLine` (roll); B-3 `contentLo` clamp-and-play
(scheduler + preview, NO copies); B-4 Builder slice -> drag-line + guard/snap + visible seam + per-piece
window note-cut; B-5 Shift-snap (snap-div grid) on the roll drag then the Builder drag-line. Reading the
five surfaces first, then implementing, ONE build gate at the end.

**Source verification:** the roll ALREADY has a slice drag-line (`mSlicing`/`mSliceStart`/`mSliceEnd`) -
only `sliceNotesOnLine` needed the finite fix. `contentLo` = `blkStartBeat` (block's FIXED arrangement
start), so a clamped straddling note lands in a window only on the block-crossing block => fires once, NO
per-block retrigger (validated before implementing). Builder had a CLICK-split, not a drag-line.
`addBlock` = `push_back` (append, no reorder) => collected indices stay valid across a multi-block slice.

**Done:**
- **B-2** (PianoRoll.cpp `sliceNotesOnLine`): `[loY,hiY]` vertical-band gate on each note's centre (t in
  [0,1]) - the cut is the DRAWN SEGMENT, X interpolated not extrapolated past an endpoint.
- **B-3** (PluginProcessor.cpp scheduler + BuilderPage.cpp preview): straddling note CLAMPED to the left
  edge, plays its remaining fragment (no copy). Scheduler `rawStart`/`absStart`/`chainDur` split, off =
  `rawStart+chainDur` (real end). Preview `drawS = max(0, sBars)`, skip only if `eBars <= drawS`.
- **B-4** (BuilderPage.cpp/.h): click-split -> drag-line (`mSlicing`/`mSliceStart`/`mSliceEnd`; mouseDown
  starts, mouseDrag updates, mouseUp applies). New `applySliceLine` (collect-then-split every block whose
  ROW EXTENT the line's y-span overlaps - a plain click still slices the block under the cursor; t clamped)
  + `sliceOneBlock` (extracted per-block split). Guard/snap (ii): snapped cut, raw-cursor fallback for
  short blocks. Seam (iii): 2px white line at a continuation piece's left edge (Pattern
  `contentOffsetTicks != 0` / Audio `contentStartSamples != 0`). Note-cut (iv): per-piece
  `contentOffsetTicks` + B-3 makes the fragment play. **Task-4 handoff resolved:** `cycleTicks` now from
  `getPatternContentBeats` (content length), aligning the slice window with the tiling cycle.
- **B-5**: Shift branch in both mouseDrags forces a VERTICAL cut at the snap-div-snapped X (both endpoints
  share it); y extent from the raw drag. Two-dot line preview drawn in both paints.

**Diagnostics added:** none.

**Files touched (Task 5):** PianoRoll.cpp (B-2, B-5 roll); PluginProcessor.cpp (B-3 scheduler);
BuilderPage.cpp + BuilderPage.h (B-3 preview, B-4 drag-line/guard/seam/split, B-5 Builder).

**Resume action:** Jeff runs `do_build.bat` (Task 5 build gate). Clean -> start Task 6 (sfizz MPE -
capability check FIRST). Errors -> fix.

**Build gate: CLEAN** (Jeff, 2026-07-20). Task 5 done + verified (all of B-2/B-3/B-4/B-5).

## 2026-07-20 — Task 6 — sfizz MPE glide (A-1) — REQUIRED capability check first (in progress)

Per A-1, before building the ramp layer I MUST confirm the vendored sfizz honors per-note-per-channel
(MPE) pitch-bend cleanly. Cannot build/run (Jeff does), so verifying by SOURCE INSPECTION of the
vendored sfizz + the BaySickGuitars/Basses -> sfizz MIDI path. If it can't do clean per-note MPE bend =>
STOP + surface to Jeff as a spec call (fall back to simple channel-bend or defer).

**CAPABILITY CHECK RESULT: FAILED (source-verified 2026-07-20).** The vendored sfizz CANNOT do
per-note-per-channel (MPE) pitch bend. Evidence:
- `sfz::Sfizz` public API (`libs/sfizz/src/sfizz.hpp`): `noteOn/noteOff/cc/hdcc/pitchWheel/
  channelAftertouch` ALL take `(int delay, ...)` as the first arg = SAMPLE OFFSET, **no MIDI channel
  anywhere**. `pitchWheel(int delay, int pitch)` carries no channel and no note number.
- `Synth::pitchWheel` (Synth.cpp:1529) => `getMidiState().pitchBendEvent(delay, normalizedPitch)` =>
  a single GLOBAL bend state read by every sounding voice. One bend value moves ALL voices together.
- No `MPE` / `perNoteBend` / `noteExpression` / per-voice-bend mechanism anywhere in `libs/sfizz/src`.
- Per-note POLYaftertouch exists (`polyAftertouch(delay, noteNumber, ...)`) but per-note PITCH does not,
  and the loaded guitar/bass sample patches don't map aftertouch->pitch.
- BaySickGuitars AND BaySickBasses both feed sfizz channel-agnostically (single stream, channel 0):
  GuitarsProcessor.cpp:211 / BassesProcessor.cpp:211 `mSfizz->pitchWheel(delay, value)`.

=> A-1's locked decision (D3=B, "full per-voice parity via MPE") rests on a premise the source
falsifies. The researcher assumed sfizz could do MPE; it cannot without forking the vendored lib.
**STOPPED before building the ramp layer. Surfaced to Jeff as a spec call (see chat).** Task 6 NOT
implemented pending his decision.

**Jeff's decision (2026-07-20):** Option 1 (global bend) + a notice IN the Note Properties box (bottom,
Guitars/Basses only, box extended downward only on those rolls, noticeable font/color); bend range ±12
semitones; wording: "Note: on BaySickGuitars and BaySickBasses, a slide bends every playing note together
not just one. Useful for solos, not for chord bends." Notice always-on on those rolls (answer i).

**Bend-range gating check (source-verified 2026-07-20):**
- NO runtime bend-range API on `sfz::Sfizz` (only scala/tuning/quality/voices/volume setters). Bend
  range is the per-region SFZ opcode `bend_up`/`bend_down` (Region.h:361, default 200c = +/-2 semi).
- CANNOT force it cleanly by prepending `<global> bend_up=1200`: `Synth.cpp:104` `case hash("global"):
  globalOpcodes_ = members;` => a `<global>` header REPLACES the global opcode set, so a patch's own
  `<global>` wipes a prepended one. Robust forcing would need fragile per-`<global>` text-injection with
  #include expansion => rejected.
- CLEAN mechanism instead: **READ** the patch's `bend_up`/`bend_down` at load (reuse the existing
  loadKit #include-walker that already scans `set_cc`), store it, and scale the slide's pitch-wheel to
  it: `pw = clamp(+/-8192, (slideSemis / bendSemis) * 8192)`. Correct for ANY patch; a slide beyond the
  patch range clips at the range (not a wrong pitch). NECESSARY anyway - the decoder must know the range
  to emit the right pitch-wheel value.
- The karoryfer patches (BaySickGuitars/Basses' vendor - confirmed via the in-repo karoryfer drums patch
  `Files For Claude/karoryfer.../02-basic.sfz:112-113` `bend_down=1200 bend_up=1200`) already ship +/-12,
  = Jeff's chosen range. So the read approach delivers +/-12 on his actual sounds with zero injection.
- => Reporting back (as promised) + confirming the read-the-range mechanism with Jeff before building.

**Patch analysis (Jeff attached all 11 guitar + 11 bass Programs, 2026-07-20):**
- Playable-note regions get their bend range from #included control modules (`modules/controls/vibrato*.sfz`
  for guitars; `controls/common*.sfz` / `b_vibrato.sfz` for basses) - NOT set inline in the top-level
  Program files. Same vendor as the in-repo drums patch (bend_up=1200), so the playable range is the
  karoryfer standard (Jeff's chosen +/-12).
- The ONLY inline `bend_up`/`bend_down` in the top-level files is `bend_down=0 bend_up=0` on the
  "Background noise / microphonic feedback" `<master>` sections (comment: "//Not affected by pitch bends
  or vibrato"). => those ambient layers deliberately do NOT bend. With a global pitch-wheel they simply
  ignore it (bend_up=0) - a FREE correct behavior (noise shouldn't slide, only the notes do).
- Patches stack MULTIPLE simultaneous `<global>` layers (Center + Unison t1/t2; combo = Green+Black).
  A single note triggers all its layers; a global pitch-wheel bends them together = correct (they are
  the same pitch). Reinforces the non-MPE / chord-drag limitation (covered by the notice).
- => The **read-the-range** mechanism is confirmed viable WITHOUT needing the include files: `loadKit`
  already walks #includes (depth 4) for `set_cc`; extend it to capture the max `bend_up` / min
  `bend_down` (ignore 0). For the karoryfer library that reads +/-12 (Jeff's pick); adapts to any patch;
  bend_up=0 noise layers ignore the wheel automatically. Do NOT need to hardcode or see the modules.

**Task 6 build plan (pending Jeff's go):** (1) loadKit (Guitars+Basses): capture patch bend range via
the existing walker. (2) Decoder in the processor: intercept CC84 (source) / CC5+CC37 (glide ms) / CC85
(RP target) / CC86 (RP loudness - ignore for sfizz, no per-note vel path) + the noteOn-at-target for
RT/Porta, and drive a per-engine GLOBAL pitch-wheel RAMP scaled to the patch range - RT/Porta: noteOn at
target, pitch-wheel starts at (source-target) and ramps to 0; RP: note already sounding, ramp 0 ->
(target-source). Ramp state advanced per processBlock over the glide ms. (3) Guitars/Basses-only Note
Properties notice (bottom, box extended down, noticeable, always-on).

**STOP #2 (2026-07-20) — the ACTUAL patch bend ranges kill the +/-12 premise (read the module .sfz files
straight off disk during the build):**
- Guitar MAIN notes (`modules/controls/vibrato*.sfz`): `bend_up=270..320` `bend_down=270..320` (ALL
  POSITIVE) + author comments "//Bends up to a minor third ... to keep it simple" / "//Bends are up
  only". => ~**3 semitones, UP ONLY**. sfizz `getBendInCents` (Region.cpp:1850) = `bend>0 ? bend*bendUp
  : -bend*bendDown`; a POSITIVE bendDown makes the down-wheel bend UP as well, so there is no downward
  bend at all on the main articulation.
- Guitar behind-the-bridge (`maps_black/btb_tp.sfz`): `bend_up=225 bend_down=-225` = normal +/-2.25.
- Bass: NO `bend_up`/`bend_down` ANYWHERE in the library => sfizz default (`bend_up=+200 bend_down=-200`)
  = normal **+/-2 semitones**, both directions.
- => These libraries are deliberately built for REALISTIC small string bends (a minor third up on
  guitar; +/-2 on bass), NOT +/-12 glides. Jeff's chosen +/-12 is UNSUPPORTED without overriding each
  patch's bend range (the fragile SFZ text-injection already rejected) AND overriding the intentional
  "up only" guitar design - and a +/-12 pitch-bend of a sample recorded for +/-2..3 sounds heavily
  stretched.
- **STOPPED. Surfaced to Jeff (see chat) as a spec call:** (1) native ranges [realistic bends: bass
  +/-2, guitar ~+3 up-only]; (2) override to +/-12 [fragile injection + artificial sound + defeats the
  library design]; (3) defer sfizz glide entirely. Verified by reading the actual .sfz modules, NOT
  guessed. Task 6 NOT built pending his call.

**Jeff's redirect (2026-07-20):** correctly points out slide != bend (a slide moves any direction; the
native bend can't). Proposes a CUSTOM in-app blended slide: play the real samples along the start->end
path in order and crossfade them (finger-on-fretboard), and restructure guitar/bass note props into
"RP Slide" (this new blended slide) + "Bend" (native pitch-wheel, semitone-amount dropdown gated by
engine capability, block length = duration). Workshopped: the blend needs per-voice isolation (sfizz
mixes all voices to one buffer - can't crossfade its output) + the samples are locked in the complex
patch (keyswitch/vel-layer/RR/unison/feedback) => a NEW crossfading sample-slide engine, not a Task-6
tweak. The up-only wall is NOT fatal: always pick the sample at-or-below the current pitch and bend it
UP <=1 semitone => works both slide directions. **Jeff chose (C): a feasibility spike first (dedicated
sfizz instances vs. custom sampler), report real numbers, then decide.** Spike underway; nothing built.

**Spike - codebase numbers (2026-07-20):**
- Library sizes: Guitar 1776 .wav / 560 MB; Bass 2208 .wav / 1.1 GB. Each note maps to MANY regions
  (velocity layers x round-robin seq_length=4 x string variants x articulation x keyswitch).
- sfizz `setPreloadSize(4096)` frames/sample => RAM preload per FULL instance ~= 4096 x 8 bytes x
  nSamples ~= **58 MB (guitar) / 72 MB (bass)** per instance (rest disk-streamed).
- **Approach A (pool of dedicated sfizz instances):** each extra slide instance = +58/72 MB RAM +
  a full-patch load (parse + preload of a 560MB-1.1GB library, multi-second) + keyswitch/CC state-sync
  per instance. A 2-3 instance slide pool = +120-220 MB PER slide-capable tab, xN tabs. Heavy for an
  app targeting first-time users. Crossfade IS possible (render each instance separately) - solves the
  voice-isolation wall - but the memory/load/sync cost is real.
- **Approach B (purpose-built sampler):** loads only the slide-path samples (a few MB) - far lighter -
  but must re-implement region resolution (the maps are complex). Simplify to ONE representative sample
  per note (ignore vel-layers/RR) => tractable + light + real-sample timbre, at a fidelity cost the ear
  won't parse during a fast slide. VibePlayer (`loadSingleFile` + glide, NO crossfade) is a partial
  base, not a drop-in - would need a crossfade/multi-voice-blend layer added.
- **Likely-best (B-simplified):** at patch load, ALSO parse a note->one-sample table; a small custom
  crossfade-slider plays those with micro-bend (always bend a lower sample UP <=1 semi) + crossfade +
  attack-masking. Main sfizz keeps playing normal notes at full fidelity; only the slide uses the table.
- External research (how commercial legato/slide sample engines do this) dispatched to
  daw-architecture-research; synthesis pending its return.

**Spike CONCLUSION (2026-07-20, architecture research returned + parent-verified):** report saved at
`Plans & Specs/Research Reports/daw-architecture-sample-based-continuous-pitch-slide-2026-07-20.md`.
- **Approach A (multi-sfizz) is OUT** - and not just on cost. Decisive point I under-weighted (verified
  against my own patch analysis): each sfizz instance renders the FULL layer stack per note (Center +
  Unison t1/t2 + feedback; combo = Green+Black = 4-6 layers/note) already MIXED, so crossfading two
  instances = crossfading 8-12-layer MUD, not clean samples. Plus each instance keeps the up-only bend
  (no down micro-bend). 3x memory/CPU for a mediocre result. Don't build A even as a stopgap.
- **The pro recipe (Kontakt/Ample/Shreddage) validates B/C:** 2 voices steady + brief 3rd on handoff,
  equal-power crossfade of real SUSTAIN samples, sample OFFSET to hide the re-pluck attack, ≤1-semi
  micro-bend. The "always bend a lower sample UP ≤1 semi" trick is confirmed sound (small shift keeps
  formants fixed - no chipmunk - and works both directions).
- **Recommendation: Option C (hybrid)** = keep sfizz for every normal note + a small purpose-built
  SlideSampler for the slide gesture (reuse the existing loadKit #include-walker to extract a note->
  sustain-sample table). ~2-4 weeks honest => its OWN batch, NOT a Task-6 tweak. The "lost" patch
  layering is the CORRECT thinner sound for a slide (a finger slide is one string voice, not the
  unison/feedback stack). If no room: defer C + ship the native global "Bend" (small realistic bends)
  as the interim.
- => Surfaced to Jeff: how to CLOSE Task 6 in THIS batch (native Bend interim vs. defer) + Option C as
  a future dedicated batch. Awaiting his call.

**Jeff's decision (2026-07-20): Option 2 - DEFER Task 6 entirely.** No code for Task 6 this batch. The
WHOLE guitar/bass slide+bend story goes to a new dedicated **SlideSampler** batch: the blended "RP Slide"
(Option C hybrid) + the native "Bend" + the engine-aware Note Properties redo (strip the dead
RP/RT/Porta buttons on Guitars/Basses, add Flat/Bend/RP-Slide gated per engine + the notice). The dead
slide buttons on guitar/bass rolls are PRE-EXISTING (sfizz never decoded the in-house glide protocol),
so deferring the strip is not a regression - it lands in the SlideSampler batch. Task 6 = closed as
DEFERRED (0 files changed). Also directed: do Task 7 (all documentation, **NO commit**) + write the
SlideSampler plan file (incl. a task to review whether the SlideSampler can also back BaySickPlayer,
which loads SFZ via loadSFZ).

## 2026-07-20 — Task 7 — Close (documentation only, NO commit per Jeff)

Authored §B.22 in the Master Test Plan (SS-1..SS-12 + SS-A deferred marker), superseded §B.14
H-2..H-5 + §B.13 G-8..G-11. §9 sixty-first Forks entry applied to Main Plan (QA-H + QA-G back-refs +
the A-1 -> SlideSampler net-new routing + the latent-slice-bug correction). SlideSampler batch plan
(`silky-gliding-lynx`) + paired running notes written; spike report saved to Research Reports.
`/review-batch` (batch-code-reviewer) dispatched over the Tasks 1-5 diff (scoped to exclude the prior-
session G3 + QA-L-Fix work). No commit (Jeff directed docs-only).

**`/review-batch` result (2026-07-21):** 0 BLOCKERS; code READY. 15/15 active S-/B- decisions verified
present + correct; A-1 correctly deferred; prior-session hunks correctly excluded. Reviewer also
verified two interaction edges: the NIT-7 tile-scan lower bound still schedules tile 0 at the exact
block-crossing moment (B-3 clamp-and-play notes never skipped), and `findRampAnchorPitch`'s back-walk
stays bounded (co-starting double-RampSlide degrades to silent, not a hang).
- 1 NEEDS-FIX = the commit boundary is unsplittable at file granularity (3 batches co-mingled in
  PluginProcessor.cpp / PianoRoll.cpp / BuilderPage.cpp) - NOT a code defect, a commit-time call for
  Jeff (already surfaced).
- 4 NITs. FIXED the 3 comment-only ones (mine): US-spelling "centre"->"center" in the B-2 (PianoRoll)
  + B-4 (BuilderPage) comments; reorganized the orphaned expression/glide comment above
  emitNoteExpression/emitPianoNoteOn after the S-6 extraction. Comment-only => no behavior change, no
  re-verify.
- 2 behavioral NITs LEFT as documented-acceptable (flagged to Jeff, not fixed): (i) the RT/Porta
  mono-cut leaves the source's original pending note-off in place (a later no-op off at the source's
  natural end - harmless on a mono line; only a same-pitch retrigger inside the source span would be
  cut early - this was my deliberate Task-1 tradeoff to avoid a pending-off hang); (ii) VibePlayer's
  loudness ramp finishes a few ms late on its final partial block (`applyGainRamp` over the whole
  block - inaudible; the synth/Harmless per-sample ramps don't have it).

**Task 7 DONE (documentation + review; NO commit).** Batch code-complete for Tasks 1-5, verified clean;
A-1 deferred to QA-SlideSampler. HEAD unchanged (d6abc38b); nothing committed this session.

---

## HELD Implemented Work Log entry (applies at the §B.22 campaign pass, bulk-run R2)

> Draft below; NOT yet applied to `Implemented Work Log.md`. Applies with the §5 STATUS flip when §B.22
> passes the campaign walk (R2). Backfill the close commit hash at commit.

### 2026-07-20 — QA-SlideSliceGlide — note-type slides + Note Properties + Builder tiling/slice; sfizz slide deferred

**Bucket:** 2 Players (slide DSP + app-wide panning across the 4 in-house engines) + 4 System Pages
(Note Properties popup, Builder tiling/slice). Batch `wistful-sliding-otter`. `blocks:` (backfill hash).

#### Done
- **S-1** relaxed both slide source resolvers (`findGlideSourcePitch` / `findRampAnchorPitch`) to
  at-or-before so a slide off a co-starting base note resolves + sounds (was silent).
- **S-3/S-5** RT = cut the base note (shared mono-cut, deferred post-pass so it survives same-sample
  co-start regardless of note-vector order) + retrigger + block-length glide. No more twin voice.
- **S-4/S-10** new `PianoNote.portaLengthBeats` (default 1) + `"pl"` serialization; Porta glides over it
  (converted to ms at the note tempo), ignoring the block length — was a hard ~60 ms snap (Issue 4).
- **S-6** RP takeover now emits the per-note expression block (shared `emitNoteExpression`) — pan /
  fine-pitch / cutoff / resonance / release; velocity rides the option-C loudness ramp (new CC86:
  base->slide velocity over the glide, per-sample on synth/harmless, `applyGainRamp` on VibePlayer).
- **S-7 (app-wide fix)** panning was DEAD — CC10 was emitted but NO voice consumed it. Added a CC10
  consumer + per-voice center-preserving-balance pan stage to BaySickSynth (covers Bass via shared
  DSP), Harmless/AdditiveVoice, VibePlayer (whose manual CC dispatch also dropped CC10). Fixes panning
  for EVERY note, not just slides; a centered note is bit-identical.
- **S-8/S-9/S-10** Note Properties popup: double-click-to-neutral-default on the 6 sliders; a "Porta
  Length" numeric box (BPM-box style) greyed unless the note is Porta; a Close button; +2 rows.
- **B-1** Builder tiling cycle = the pattern's REAL content length (`getPatternContentBeats(idx)`), not
  `Pattern.bars` — scheduler + preview; the >4-bar note cull self-fixed.
- **B-2** roll slice is a finite drawn segment (vertical-band gate), not an infinite line.
- **B-3** mid-note slice = clamp-and-play the straddling note's fragment at read time (scheduler
  `contentLo` + preview), NO pattern copy.
- **B-4** Builder slice = a drag-line (state + two-dot preview + apply-on-mouseUp), multi-block, short-
  block guard fixed (snapped-or-raw cut), visible seam at continuation pieces; content-cycle aligned to
  `getPatternContentBeats`.
- **B-5** Shift = a vertical cut snapped to the active snap-div, on both the roll and Builder drag-lines.

#### Found along the way / routed
- **A-1 sfizz slide: source-falsified + DEFERRED.** sfizz has no per-note MPE bend and mixes all voices
  to one buffer; the karoryfer guitar patches ship ~+3 semi UP-ONLY native bend (bass ±2). A slide can't
  come from sfizz. Workshop + feasibility spike -> a purpose-built crossfading SlideSampler + native
  Bend + engine-aware note-props redo = new QA-SlideSampler batch (`silky-gliding-lynx`), §5 slot Jeff's
  call. Spike report in Research Reports. (§9 sixty-first entry.)
- **Record correction:** the roll-slice infinite-line bug (B-2) is LATENT since the initial commit, not
  a QA-G regression.
- The dead in-house RP/RT/Porta buttons on Guitars/Basses rolls (pre-existing; sfizz never decoded the
  glide CCs) are stripped as part of the SlideSampler batch's note-props redo, not this one.

#### Verified
Per-task build gates all reported CLEAN by Jeff (Tasks 1-5). Behavioral verification per §B.22 at the
campaign pass.
