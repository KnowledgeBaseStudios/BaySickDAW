# QA-Fd — Vocal Editor Rework (pitch-editor parity + align semantics + sub-edit system + engage-tick) — Plan (snug-orbiting-catmull)

> Canonical path after approval: `Plans & Specs/Batch Plans/snug-orbiting-catmull.md`
> (mirrored at ExitPlanMode; home-dir copy deleted). Paired running notes:
> `Plans & Specs/Running Notes/snug-orbiting-catmull.md`.
> For execution: bulk-run mode (swift-stampeding-caribou governs). ONE commit at batch
> close — no mid-task commits. Per-task "verify" lines feed the Master Test Plan §B.10
> section authored at code-complete (R4); Jeff's hands-on verification happens at the
> G2 boundary smoke completion, not per-task.

## Context

Fifth batch of G2 (Jeff, 2026-07-11 — placement answer 1a). The G2 boundary walkthrough
halted at Part 4 when the pitch editor surfaced 9 problems; the owner-mandated review
that followed (Newtone reference review, parity picks, semantics rework, the
PsolaShifter root-cause hunt) produced a consolidated rework spec instead of piecemeal
fixes. This batch implements ALL of it in one pass. The full history + every locked
decision lives in `Batch Plans/swift-stampeding-caribou.md` (G2 boundary bullets +
Carry-Over FINAL, 2026-07-10/11); the 2026-07-11 chat docket (items 1-16, plus 17-20)
locked the remaining calls.

Why one batch: the pieces interlock — the time-edit model changes the align pipeline's
input (pitch feeds align, docket 13), the align semantics rework changes what its knobs
mean, the sub-edit popup replaces the in-canvas curve gestures, and the test scenarios
for all of it get rewritten together. Shipping it piecemeal would re-verify the same
surfaces repeatedly.

Risk: largest batch of the run (an editor rebuild + a DSP semantics rework + a new
time-warp stage). Mitigations: the PsolaShifter fix (`703f06e4`) is already in and
owner-verified; the align live-warp machinery (glide, seek net, snapshot lookup) is
already ear-verified and gets REUSED not rebuilt; per-task build checkpoints (build,
no commit) bound debugging blast radius.

Effort: honest estimate 2-4 full sessions of coding + Jeff's build/fix cycles, then the
boundary smoke. This is the biggest single batch in the run.

Dependencies: G2 batches QA-F/Fa/Fb'/Fc all committed (`e5c62218..703f06e4`). The G2
boundary stays OPEN through this batch and closes after its smoke completion.

## Spec calls already locked

| ID | Decision | Reasoning / source |
|----|----------|--------------------|
| 1a | Batch = QA-Fd, fifth batch of G2; boundary closes after ITS smoke | Jeff 2026-07-11. Tail = full boundary smoke completion (Parts 4-5 + FB-11 + re-run of Part-3 items the semantics change touches + realtime first-listen), not just a re-listen. |
| 2b | Caribou carry-over doc block rides the batch's single close commit | Jeff 2026-07-11. One commit per batch, period. |
| 3a/12b | Pitch/align edits NEVER silenced by a page bypass; the BaySickVocals master Bypass button is REMOVED entirely | Jeff: realtime section already has its own bypass; no other player has a page-master bypass. `bsv_bypass` param retires; early-return branch deleted; FB-11 gate-set text amends. |
| 4a | FIRST analysis of a never-analyzed channel runs immediately even mid-play (provably inaudible — no edits exist, fresh snapshot is a no-op); RE-analysis stays stop-gated; both editors get explicit visible "analyzing" / "deferred until stop" states | Jeff 2026-07-11. Kills the felt pitch-depends-on-align coupling. |
| 5/13a | PITCH TAB IS UPSTREAM: pitch-tab edits (pitch + time) redefine the performance; ALIGN consumes the EDITED performance (its analysis matches edited word-starts against the leader; time edits mark align stale → stop-gated re-analysis). Playback applies pitch edits first, align warp on top. Pill-vs-audio region lookup resolves through the same chain (closes the wrong-syllable hole found in planning). | Jeff 2026-07-11, confirmed restatement. |
| 6 | Segmentation: slice pills (Newtone-style) for unvoiced/short material AND "pitched material never silently drops" — onset-glide fragments merge into the note they lead into. Jeff's two test wavs (align screenshot pair, missing-first-note case) are the re-check material. | Jeff 2026-07-11. Root-cause lead: fast rising glide splits into sub-60 ms fragments, all discarded. |
| 7a | Pitch editor's Loose/Close/Tight preset combo REMOVED (`bsp_preset`/`bsp_preset_dirty` retire; knobs stand alone) | Jeff 2026-07-11. No reference counterpart; name-collision with Align modes. |
| 8 | Editor gets Root + Scale dropdowns + Snap toggle above Undo/Redo. Focus = snap strength (Newtone-Center-like): pulls note centers toward target AND pills visibly render at corrected pitch (green F0 curve stays put as as-sung reference). Realtime board label "Key" → "Root" (param id unchanged) for app-wide uniformity. Editor Root/Scale independent from realtime board's (intentional). | Jeff 2026-07-11. |
| 9a | Pitch-editor undo migrates to the GLOBAL undo system (UndoContext + full-snapshot edit action, PianoRollEditAction pattern): real Ctrl+Z/redo, history panel, 100+ steps. Toolbar Undo/Redo buttons drive the global stack. | Jeff 2026-07-11. |
| 10 | Sub-edit system per Jeff's design (verbatim spec in Task 7): display-only box (taller, + Pitch bar = vib/frm/vol/pitch), Edit button over titles, Reset button over bars, popup automation-editor with pill-waveform ghost + pill browser panel (multi-select-aware) + own Play (space bar when focused) + per-pill bipolar Vib/Frm knobs, Volume/Pitch lane toggle. Reset = selected pill(s) only; multi-select reset prompts with never-show-again checkbox (persisted app-wide). | Jeff 2026-07-11 (his design). |
| 14a | Scale dropdown list = the piano roll's 13 scales; the realtime board gains the 3 it's missing (see OPEN 17 for ordering) | Jeff 2026-07-11. |
| 14b | Snap OFF → Focus pulls to nearest semitone (today's behavior); Snap ON → target set = the scale | Jeff 2026-07-11. |
| 14c | Snap ON → vertical drags land on in-scale lanes; Ctrl+drag = free fine movement | Jeff 2026-07-11. |
| 15a | BOTH surfaces: slim on-pill handles (proposal = OPEN 19) + the popup editor, PLUS a third per-pill Variation knob in the popup (scales the note's own pitch wiggle around its center) | Jeff 2026-07-11. |
| 15b | Display-box Reset button = clears boxed params (vib/frm/variation + vol/pitch curves) of selected pill(s); pill right-click menu gains "Restore to Original State" = the whole thing (drag offset + time edits + curves + knobs → as-analyzed) | Jeff 2026-07-11. |
| 11/16a | Engage-tick fix IN this batch. Design = edge crossfade (option iii): engines' input history kept warm continuously (cheap copy), engines process only when ON, ~30-50 ms crossfade masks the engage/disengage transition. OFF = zero added latency (Jeff's requirement; latency-matched bypass rejected as permanent-latency-in-disguise). | Jeff 2026-07-11. |
| 9a/10a (G2) | Align timing semantics: matching window INTERNAL (~400 ms, calibration mine); Mode/Fine Tune = RESIDUAL tightness cap (within cap = natural timing untouched; beyond = pulled to cap edge). Tight=0 / Close~50 / Loose~100 ms, fine ±50 clamped ≥0. Docket 8 superseded (0 = fully locked, valid). | Jeff 2026-07-11 (caribou). |
| 12a/13a (G2) | Align +Pitch controls, OUR names: "Pitch Variation" = tuning-variation cap (within untouched, beyond pulled to edge); "Pitch Blend" = % toward leader contour, 0-100 free (existing `bsa_pitch_range` renamed at display level; mode-clamp travel windows DROPPED — modes preset values); "Pitch Types" = per-side detection-band pickers, bands Normal / High Vocal / Low Vocal / High Instrument / Low Instrument (Hz calibrations mine). Pitch percent/cap applied at PUBLISH (live knobs, no re-analysis). | Jeff 2026-07-11 (caribou). |
| 14a/15a (G2) | Flexibility rungs Low/Normal/High/Max All (stretch-ratio bounds ~1.5:1 / 2:1 / 4:1 / unbounded, calibration mine) + per-word Maximum-Shift cap knob | Jeff 2026-07-11 (caribou). |
| 16a (G2) | High-res (384 kHz-class) oversampled processing lands on the offline RENDER path, folded into the render-parity work | Jeff 2026-07-11 (caribou). |
| P1-14 | Newtone parity items 1-14 IN, 15-16 OUT. Motion model = elastic warp + Ctrl-detach + stretch/squeeze (NewTime marker gestures = family conventions). FL conventions in scope for the vocal editors: detents at default, Ctrl fine-adjust, type-in value. | Jeff 2026-07-11 (caribou). |
| — | GranularShifter + PvShifter blast-radius audit: survey verdict = NOT the Psola bug class (read rate driven by ratio directly; clamps only dip amplitude / hold priming silence). Re-confirmed at implementation desk-check; render-algo listen stays in verify. | Survey 2026-07-11; premise re-check in Task 8. |
| — | Publish-vs-stop-gate reconciliation (design note, follows from two locked rules): snapshot republish that changes ONLY pitch values (Blend/Variation knobs) is allowed mid-play (time map identical — glide no-ops). Knobs that change the TIME map (Mode/Fine/Flexibility/Max Shift) mark stale and swap at stop, honoring "maps only change while stopped". | Reconciles caribou stop-gate lock with 12a "live knobs". |
| 17b | Realtime board scale list reordered to MATCH the piano-roll order (+ its 3 missing scales) — editor and realtime menus read identically. Accepted: a pre-batch project with a non-default realtime scale loads shifted (low impact — the corrector was inaudibly broken until `703f06e4`). `bsv_pitch_scale` range 0..9 → 0..12; corrector DSP scale-mask table rebuilt in the new order. | Jeff 2026-07-11. |
| 18a | Pill right-click = context menu holding Restore to Original State + Snap to Semitone (+ future pill ops). The parity list's instant right-click-snap becomes the menu item. | Jeff 2026-07-11. |
| 19a | On-pill handles as proposed: selected pill only, hidden below a zoom threshold; top corners = volume fade-in/out (horizontal drag = fade length); bottom corners = pitch approach/release ramps (2-axis: horizontal = length, vertical = semitone offset scooped from / released to); green=in / red=out + hover arrows advertising drag axis; undoable; InfoBar live readout during drags. SYNC CONTRACT (Jeff's condition, confirmed): single storage — handles write ordinary points into the SAME per-pill volume/pitch curves the popup edits, and render FROM those curves (popup shows handle-made points; handles sit at popup-made fade edges; display box = third read-only view; pill waveform drawn post-gain so fades show on-canvas). The only overwrite path is explicit: grabbing a handle over a complex boundary span replaces that span with the dragged clean ramp — deliberate, single undo step, never a background sync. Retired gestures: direct drags on the vib/frm/vol box (display-only now); edge-drag trim (edges = stretch; re-partition via Slice + merge/join; Alt+edge-drag reserved if trim is missed at the smoke). | Jeff 2026-07-11 (19a + sync confirm). |
| 20a | Pill copy/paste = edit-state transfer: Ctrl+C captures the source pill's treatment (pitch offset, vol/pitch curves, vib/frm/variation values); Ctrl+V applies it to all selected pills as one undo step. Audio-relocation paste NOT in scope. | Jeff 2026-07-11. |

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open — items 17-20 posed and answered in chat 2026-07-11; their
locked rows are in the table above (17b / 18a / 19a / 20a).

Genuinely deferred (not blocking plan approval):

- Calibrations tuned at the boundary re-listen: internal match window (~400 ms),
  residual-cap mode values, Flexibility ratio bounds, Pitch Types Hz bands, Max Shift
  range/default, crossfade length for the engage fix, glide-merge segmentation
  thresholds, snap-pull display curve.
- NIT-4 (sync auto re-analyze UI hitch on long channels) — Jeff's deliberate judgment
  at the resumed ear-check.
- Which 3 scales the realtime list is missing — diffed at implementation
  (piano-roll table vs `PitchCorrectorDSP` table).

## Files to modify

Line refs = 2026-07-11 survey snapshot; re-resolve at execution.

**Task 1 (align semantics + publish-time pitch):**
- `Source/DSP/BaySickAlignDSP.h/.cpp` — `pairOnsets` tolerance goes internal (~400 ms;
  call site `.cpp:766`, DP gate `:247`, greedy gate `:199-206`); residual cap + Max
  Shift at anchor-offset build (`:795`, tail `:812`); Flexibility bounds replace the
  0.5/2.0 literals (`:842`); anchors store RAW pitch deltas (`:864-865` drops the
  `* range` bake); analysis output carries raw pair offsets for publish-time capping.
- `Source/BaySickVocal/BaySickVocalProcessor.h/.cpp` — `alignToleranceSec()` (`:615-624`)
  becomes the residual-cap magnitude; new params (`bsa_align_flex`, `bsa_align_maxShift`,
  `bsa_pitch_variation`, `bsa_pitch_typeGuide`, `bsa_pitch_typeDub`); `bsa_pitch_range`
  display → "Pitch Blend"; Blend/Variation applied in `publishAlignPlayback()`
  (`:798-858`, delta copy at `:821`); republish-on-knob-change (pitch-only mid-play,
  time-map knobs stale→at-stop); scale table +3, "Key"→"Root" label pass-through.
- `Source/BaySickVocal/BaySickAlignEditor.h/.cpp` — mode/fine tooltips + display for
  residual semantics (`fineTuneBaseForMode` `:63-66`, display `:797-798`); Flexibility
  picker + Max Shift knob; Pitch Variation knob + per-side Pitch Types combos;
  `rangeWindowForMode`/`applyModeWindowToRange` (`:56-61`, `:933-963`) DELETED — mode
  presets the Blend value instead; analyzing/deferred visible state.
- `Source/BaySickVocal/BaySickVocalEditor.cpp` — "Key" label → "Root" (`:205-231`).
- `Source/DSP/PitchCorrectorDSP.h/.cpp` — scale table +3 entries, rebuilt in
  piano-roll order + index semantics updated (17b).

**Task 2 (segmentation + analysis UX + bypass removal):**
- `Source/DSP/BaySickPitchDSP.h/.cpp` — `analyzeComposite` (`:55-185`): glide-fragment
  merge, slice-pill regions (new `isSlice` kind), no-silent-drop rule; per-side band
  hookup for detection where applicable.
- `Source/DSP/BaySickAlignDSP.cpp` — `estimateF0Hz` band parameterization
  (`:111-119`) for Pitch Types.
- `Source/Vox/VoxPage.cpp` — poller carve-out (never-analyzed → immediate, `:566`
  guard) + first-analysis paths in both editors (`BaySickPitchEditor.cpp:1013`,
  `BaySickAlignEditor.cpp:1216`); analyzing/deferred badges.
- `Source/BaySickVocal/BaySickVocalProcessor.cpp` — `bsv_bypass` early return
  (`:457-474`) deleted; param retired; editor button removed
  (`BaySickVocalEditor.cpp:328`, record-lock set `:392`, tooltip `:400-404`).

**Task 3 (time-edit engine):**
- `Source/DSP/BaySickPitchDSP.h/.cpp` — region gains dst-span fields (+detach flag);
  published PitchTimeMap (anchors, Hermite tangents — reuse the AlignPlaySnapshot
  math); applicator region lookup by SOURCE position from decode stamps; per-region
  pitchShape + variation in `applyEditsToBuffer` (`:252-364`).
- `Source/PluginProcessor.cpp` — decode-layer law composition (warp law lambda
  `:1174-1181` consumes pitch map then align map); per-block source-pos stamp to the
  vocal processor (extend `finalizeFilePlayStrip` `:1682-1686`); align stale signature
  gains the pitch-edit-map hash.
- `Source/BaySickVocal/BaySickVocalProcessor.cpp` — align analysis input = edited
  timeline (raw onsets transformed through the pitch map before pairing).

**Tasks 4-6 (editor gestures / view / selection+undo):**
- `Source/BaySickVocal/BaySickPitchEditor.h/.cpp` — everything: drag kinds (move/
  stretch/detach), Root/Scale/Snap toolbar controls, snap+Focus rendering, wheel map
  (Alt v-zoom per PianoRoll.cpp:2018-2024 idiom; `mLaneH` becomes variable, clamped),
  middle-drag pan, zoom-to-selection, bottom clamp (paint loops `:75`/`:133`),
  auto-scroll edge-trigger fix (`:929-945`), 30 Hz timer + transport-following playhead
  incl. stop-reset (main-roll pattern: PianoRollPage.cpp:23, StandaloneEditor.cpp:6943-6975),
  marquee/Ctrl+A/C/V/B/Shift-arrows (PianoRollGrid patterns `:679-875`), merge/join,
  batch re-pitch via key click, in-block pitch text, fine modifier, InfoBar refresh on
  undo/reset (#9) + `hasEdits` unity-point handling, right-click menu, global-undo
  migration (`UndoContext` + new `PitchEditAction` in `Source/Standalone/UndoActions.h`).

**Task 7 (sub-edit system):**
- `Source/BaySickVocal/BaySickPitchEditor.h/.cpp` (display box + handles) + new
  popup component (own file `Source/BaySickVocal/BaySickPitchSubEditor.h/.cpp`);
  `Source/DSP/BaySickPitchDSP.h/.cpp` — pitchShape/variation fields + ValueTree
  persistence (back-compat: absent fields default neutral); preview-play path;
  never-show-again flag in settings.

**Task 8 (offline render parity + high-res):**
- `Source/DSP/BaySickAlignDSP.cpp` — `applyWarp` (`:488-697`) smooth-map port (cubic
  Hermite read replaces segment-constant ratio `:537`); render path honors pitch time
  edits; oversampled (high-res) render option; Granular/Pv desk-check.

**Task 9 (engage-tick):**
- `Source/DSP/PitchCorrectorDSP.h/.cpp` — always-primed input ring; engage crossfade
  (formant engage reset `:241-244` + shifter spin-up covered).

**Task 10 (docs/tests):**
- `Plans & Specs/Test Plans/v1-master-test-plan.md` — new §B.10 + amendments (F-3/F-4
  rewrite, FA-9 carve-out, FB-11 bypass removal, F-6 wording).
- Running notes, held Work Log entry, Main Plan §5 QA-Fd row + §9 Forks entry,
  caribou G2 note. [PITCH DIAG] strip per catalog AFTER the smoke passes.

## Tasks

### Task 1 — Align semantics rework + publish-time pitch controls
- [ ] Internal match window: `pairOnsets` tolerance fixed (~400 ms); mode/fine stop
      feeding it.
- [ ] Residual-cap model at anchor build: offset d = guide−dub; |d| ≤ R → anchor at
      dub (natural timing kept); |d| > R → pulled to cap edge (residual = R, signed).
      Max Shift M caps the applied movement: move = min(max(|d|−R, 0), M). Sync points
      exempt (hard user intent). Tight R=0 / Close ~50 / Loose ~100 ms; fine ±50 trims,
      sum clamped ≥ 0.
- [ ] Flexibility rungs → segment-slope bound at map build ([1/r, r], r per rung;
      Max All = unbounded, lookup clamp still rails at 1/64..64). Live recovery-glide
      2:1 cap is a SEPARATE constant and stays.
- [ ] Raw pitch deltas on anchors; Blend (%) + Variation cap applied at publish;
      republish rules per the reconciliation note (pitch-only = live; time-map knobs =
      stale → swap at stop).
- [ ] Pitch Types per-side band combos → per-side YIN lag ranges (5 bands, Hz mine).
- [ ] Editor: Flexibility picker + Max Shift + Pitch Variation knobs + per-side Pitch
      Types combos; mode presets Blend value (window re-seat machinery deleted);
      tooltips rewritten (ASCII, US spelling); "Key"→"Root" on the realtime board;
      realtime scale list +3, reordered to piano-roll order (17b: `bsv_pitch_scale`
      0..12, corrector scale-mask table rebuilt to match).
- [ ] Build checkpoint (no commit). Verify lines → §B.10: mode-feel scenarios (Tight
      locks, Loose keeps natural timing within 100 ms), Blend/Variation live-knob
      audibility without re-analysis, Flexibility audible reach, Max Shift guard.

### Task 2 — Segmentation + analysis-UX + bypass removal
- [ ] No-silent-drop segmentation: glide fragments merge forward into the stable note
      (curve visible inside pill); unvoiced/too-short → slice pills (`isSlice`: no
      pitch handle, time-editable, no F0 requirements); thresholds recalibrated.
      Re-check material = Jeff's two wavs (missing-first-note case must produce a pill).
- [ ] First-analysis carve-out at the three stop-gates (never-analyzed → run now, even
      mid-play); explicit "Analyzing..." / "Analysis deferred until stop" states in
      both editors (no more silent empty canvas); pitch analyze failure surfaces state
      instead of silence.
- [ ] Remove `bsv_bypass`: param, button, early return, record-lock set + FB-11 text.
- [ ] Build checkpoint. Verify lines: first-note pill present on the test wav; slice
      pills visible; analyze-while-playing (first) vs deferred (re-analysis); bypass
      button gone, edits always audible.

### Task 3 — Time-edit engine (pitch upstream → align downstream)
- [ ] Region dst spans + detach flag; channel PitchTimeMap published with edits
      (anchor/Hermite structure reusing the align snapshot math; monotone enforced for
      elastic edits, jump discontinuities allowed for detached pills with short
      crossfades at boundaries).
- [ ] Decode-layer composition: read = pitchMap(alignMap(t)) [align ON] or pitchMap(t)
      [align OFF]; glide/seek-net machinery unchanged (law-agnostic).
- [ ] Decode stamps per strip per block: timeline t0 + actual source pos u0 + rate →
      applicator resolves the active pill in SOURCE domain (glide-exact; closes the
      wrong-syllable hole).
- [ ] Pitch time edits → align stale (signature includes the time-map hash); align
      analysis consumes EDITED word-start times (raw onsets transformed through the
      map).
- [ ] Persistence: dst spans/detach in the region ValueTree (absent = neutral;
      old projects load unchanged).
- [ ] Build checkpoint. Verify lines: move a word → hear it move (align off); align on
      → align re-analyzes from edited timing at stop; both engaged → correction follows
      the edited word.

### Task 4 — Core gestures (motion model + snap + menus)
- [ ] Body drag = 2-axis: vertical pitch (0.1 st; Ctrl fine 0.01; Snap ON → in-scale
      lanes), horizontal elastic move (gap counter-warp, clamp at gap exhaustion —
      pills never cross). Edge drag = stretch/squeeze. Ctrl+move/stretch = detach.
      Old edge-trim retires (re-partition via Slice/merge; note to Jeff at smoke).
- [ ] Root/Scale/Snap controls above Undo/Redo (13-scale list); Focus pull rendered:
      pills draw at corrected pitch (detected + drag + Focus-toward-target), curve
      stays as-sung.
- [ ] Right-click pill menu (18a): Restore to Original State (full as-analyzed reset
      per 15b) + Snap to Semitone (menu item, replacing instant-snap).
- [ ] Merge/join adjacent pills; batch re-pitch: keyboard-key click with a selection
      re-centers selected pills on that note (audition-hold preserved); double-click
      pill = open sub-editor popup; in-block pitch readout at sufficient lane height;
      scrub-audition on pill middle while stopped (shares the preview-play path from
      Task 7).
- [ ] Slice mode: keep 30 ms interior guard; works on slice pills too.
- [ ] Detents-at-default + Ctrl-fine + right-click type-in on the vocal editors' knobs.
- [ ] Build checkpoint. Verify lines: every gesture, snap behaviors ON/OFF, menu items.

### Task 5 — View, navigation, playhead
- [ ] Wheel map per family idiom: Ctrl h-zoom (exists), Alt v-zoom (new; `mLaneH`
      variable, clamped ~4-24 px, cursor-anchored), Shift h-scroll (exists), bare
      v-scroll (exists); middle-drag pan; zoom-to-selection + return (family
      Ctrl+RMB-drag rect / RMB toggle-back idiom).
- [ ] Bottom clamp: paint loops floor at the view bottom (no lanes below C0 territory);
      top stays 120-clamped.
- [ ] Auto-scroll: edge-triggered page-flip only while the playhead ADVANCES across the
      view edge with A on; never recenters on a frozen/stale stamp; never fights manual
      scroll. A toggle stays, default on.
- [ ] Playhead follows the main transport at 30 Hz incl. stop-reset (stop → seek target
      per the app's stop policy); editor timer 400 ms → 30 Hz (playhead/scroll only —
      poll-ish work stays slow-path).
- [ ] #9: InfoBar refreshes on undo/redo/reset; `hasEdits` ignores unity/empty curve
      residue; [edited] tag truthful.
- [ ] Build checkpoint. Verify lines: zoom/pan set, no sub-C0 lanes, playhead resets on
      stop, no view-reset-to-zero, tag clears on undo.

### Task 6 — Selection, clipboard, global undo
- [ ] Marquee + Ctrl+A / Ctrl+C / Ctrl+V / Ctrl+B / Shift+arrows nudge / Delete
      semantics per family (Delete on a pill = Restore to Original State — pills are
      analysis segments, not deletable objects; slice pills likewise).
- [ ] Multi-select-aware gestures (drag applies to selection like the roll's
      multi-note move/resize).
- [ ] Copy/paste = edit-state transfer (20a): Ctrl+C captures pitch offset + vol/pitch
      curves + vib/frm/variation from the focused pill; Ctrl+V applies to all selected
      pills, one undo step.
- [ ] Global undo migration: `PitchEditAction` (full before/after region vectors) via
      `UndoContext`; toolbar buttons drive it; popup edits + per-pill knob drags wrap
      in the same action; local stacks deleted; FA-13's Ctrl+Z line becomes true.
- [ ] Build checkpoint. Verify lines: selection ops, Ctrl+Z through the global stack
      incl. history panel, undo across popup edits.

### Task 7 — Sub-edit system (Jeff's design, verbatim scope)
- [ ] Display box: display-only, slightly taller, 4 bars (VIB / FRM / VOL / PITCH),
      Edit button over titles, Reset button over bars. Reset = selected pill(s) boxed
      params only; multi-select reset → warning prompt + never-show-again checkbox
      (settings-persisted).
- [ ] Popup sub-editor (automation-editor pattern): lane length = pill length,
      pill waveform ghosted behind, editable points + curves with the EventEditor
      gesture set (add/drag/right-click point ops), Volume/Pitch lane toggle; pill
      browser panel on the right listing pills in order (selection-filtered when
      entered from a multi-select); own Play button previewing the pill span through
      current edits, space bar = play/stop while the popup has focus; per-pill bipolar
      Vib + Frm knobs + Variation knob at top.
- [ ] On-pill slim handles (19a, sync contract locked): corner handles write ramp
      points into the SAME curves the popup edits and render from curve state;
      boundary-span replace only on explicit handle re-drag (one undo step); pill
      waveform drawn post-gain so fades show on-canvas.
- [ ] DSP: per-region `pitchShape` (additive semis over t01, same pattern as
      `volGainAt`) + `variation` (contour-deviation scaler around the note center —
      per-sample deviation source from the F0 track); applicator applies both;
      ValueTree persistence, neutral defaults.
- [ ] Preview-play path (shared with scrub-audition): plays the strip's composite span
      through the applicator while the main transport is stopped.
- [ ] Build checkpoint. Verify lines: popup flow end-to-end, multi-select browser
      filter, reset prompts, preview play + space bar, pitch lane audibly bends inside
      one pill, variation knob tames/exaggerates wiggle.

### Task 8 — Offline render parity + high-res
- [ ] `applyWarp` smooth-map port: continuous Hermite-slope read (same tangent math as
      live) replaces segment-constant ratios — renders stop stepping at anchors.
- [ ] Renders honor pitch-editor time edits (same composed map).
- [ ] High-res render option: oversampled processing on the render path (16a fold-in);
      surface = render dialog choice.
- [ ] Granular/Pv desk-check (survey verdict re-confirmed at my desk); render-algo
      listen scenario stays in §B.10.
- [ ] Build checkpoint. Verify lines: render == live warp by ear (no anchor steps),
      renders reflect time edits, all three algos render sane, high-res render
      completes and sounds >= standard.

### Task 9 — Realtime engage-tick fix
- [ ] Always-primed input history (corrector input ring fed even while OFF — copy only,
      no processing); engage/disengage = ~30-50 ms equal-power crossfade between dry
      and corrected taps; formant-engine engage reset covered by the same fade.
- [ ] OFF path latency unchanged (zero added).
- [ ] Build checkpoint. Verify lines (feeds the realtime FIRST-listen): toggle while
      monitoring = no click, no doubling beyond the brief fade; hard-tune snap audible
      (first real listen — every prior impression was on the broken shifter); NIT-4
      judgment rides this listen.

### Task 10 — Docs, tests, close ritual
- [ ] Master Test Plan: author §B.10 (QA-Fd, `FD-*` scenarios) from the tasks' verify
      lines (scenarios physically executable, derived from the shipped UI); amend
      F-3/F-4 (new semantics/controls), FA-9 (first-analysis carve-out), FB-11 (bypass
      removal), F-6 wording.
- [ ] Running notes appended at code-complete; held Work Log entry drafted
      (`/draft-doc batch-close`), applied at section pass per R2.
- [ ] Main Plan: §5 QA-Fd row + §9 Forks entry (G2 composition change) — drafted,
      surfaced, applied via Edit.
- [ ] `/review-batch` over the batch diff at code-complete (fix findings before the
      build handoff).
- [ ] ONE commit at close (message + FULL git status → Jeff approves): all source +
      docs + the caribou carry-over block (2b).
- [ ] AFTER the boundary smoke passes: strip [PITCH DIAG] per the caribou catalog
      (10 sites, BaySickPitchDSP.h/.cpp + BaySickPitchEditor.h/.cpp + flag poll) —
      surfaced for approval, then stripped (rides the smoke-fix commit or a follow-up
      per Jeff's call at that moment).

## Verification (end-to-end — the G2 boundary smoke completion)

Jeff-driven, Debug exe first then Release, after the batch commit:
1. §A global smoke ladder (launch, audio, big project, save/reopen).
2. Boundary walkthrough completion: Part 4 (pitch editor — now the full rework
   surface), Part 5, FB-11 (amended).
3. Re-run of Part 3 align items the semantics rework touches (mode feel, analyze
   flows, glide/toggle) — F-3/F-4 as rewritten.
4. Realtime corrector FIRST real listen (Task 9 scenarios + NIT-4 judgment).
5. §B.10 scenario walk (or its campaign slot — Jeff's cadence call at the smoke).
6. Then: [PITCH DIAG] strip → boundary close entries → G3 group-open per caribou.

## Routing notes (Rule 3 during execution)

- Real bugs found mid-batch: fixed in-batch (standing rule).
- New spec calls: STOP and ask (bulk-run ask-always lock) — chat prose, numbered,
  lettered options, no recommendations.
- Findings on non-vocal surfaces: running notes → route at section pass.
- QA-ApvtsAutomation interaction (automation writes vs record-locked params) stays
  parked for G4 (recorded in caribou).

## Carry-Forward Reference touch points

The vocal engine post-dates the frozen carry-forward; the governing context docs for
this batch are the caribou G2-boundary bullets + Carry-Over FINAL (read at session
open) and the QA-F/Fa/Fb' plan files for original design intent. Carry-Forward §1-3
skim only if PluginProcessor plumbing questions arise.
