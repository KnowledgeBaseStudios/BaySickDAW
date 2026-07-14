# Running Notes — QA-Fd (snug-orbiting-catmull)

> Append-only mid-batch artifact. Entries appended at every checkpoint (build
> checkpoint reached / finding captured / spec call resolved / scope pivot) per
> `feedback_draft_doc_running_notes_every_checkpoint.md`. Consumed at batch close by
> `/draft-doc batch-close` as the primary input for the held Implemented Work Log
> entry (applied at section pass per R2 — bulk-run mode).

Pair file: [Batch Plans/snug-orbiting-catmull.md](../Batch Plans/snug-orbiting-catmull.md)
Convention: Main Plan §0 "Batch Plans + Running Notes layout (locked 2026-05-11)".

## 2026-07-11 — Batch open — plan approved (single R4/R5 approval)

- QA-Fd locked as the fifth batch of G2 (placement 1a); G2 boundary stays open through
  it and closes after the batch's full smoke completion (Parts 4-5 + FB-11 + Part-3
  re-runs + realtime first-listen).
- The 2026-07-11 spec docket (items 1-20) walked in chat and locked into the plan's
  "Spec calls already locked" table — headline calls: Bypass button removed entirely
  (3a/12b), pitch tab upstream of align (5/13a), slice pills + no-silent-drop
  segmentation with Jeff's two test wavs as re-check material (6), preset combo removed
  (7a), Root/Scale/Snap + Focus-as-snap-strength with visual pull (8, 14a-c), global
  undo migration (9a), Jeff's sub-edit popup design verbatim (10, 15a/15b) with the
  on-pill handle set + single-storage sync contract (19a), realtime scale list
  reordered to piano-roll order accepting the saved-pick shift (17b), pill right-click
  menu (18a), copy/paste = edit-state transfer (20a), engage-tick fix in-batch as edge
  crossfade with always-primed input (11/16a).
- Survey corrections recorded during planning (premise-checked against source):
  GranularShifter/PvShifter NOT the Psola bug class (re-confirm at Task 8 desk-check);
  pitch editor never had Ctrl+Z (undo was button-only local stack); `setTopNote` clamps
  both ends — the sub-C0 lanes come from the paint loops; nothing was baked-at-analysis
  on the pitch-editor side (the baked percent was the ALIGN side's `bsa_pitch_range` at
  anchor build, which is what moves to publish-time).
- Found-during-planning correctness hole (fix rides Task 3): with Align engaged, the
  pitch applicator resolves pills by linear timeline time while align has warped what
  actually sounds there — wrong-syllable application at real offsets. Closed by the
  decode-layer source-pos stamps + composed law.
- One commit at batch close (2b); caribou Carry-Over FINAL doc block rides it.

## 2026-07-11 — Task 1 code-complete — align semantics rework + publish-time pitch

- Ritual correction (Jeff, session start): NO per-task build stops — code the ENTIRE
  plan, then one build check, then the smoke script. Per-task "build checkpoint"
  lines in the plan are collapsed into the single code-complete handoff.
- DSP (`BaySickAlignDSP`): matching window now INTERNAL (`kMatchWindowSec` 0.4 s);
  `analyzeOffline` signature reworked — `AlignBuildParams { residualCapSec,
  maxShiftSec, flexRatio, per-side Hz bands }` replaces strength/tolerance/range01.
  Residual-cap anchor build (move = min(max(|d|-R,0), M), sync points exempt);
  Flexibility slope bound [1/r, r] replaces the 0.5..2.0 literals (Max All skips the
  pass; live glide 2:1 cap untouched); END anchor now offset-continues the last
  anchor at slope 1 (the old dubDur->guideDur pin imposed a duration-equalizing tail
  stretch — contradicts "within cap = natural timing"; calibration-class call, mine).
- Pitch deltas: stored RAW (always computed), sampled at the RAW pair positions —
  found-while-implementing correctness point: the old code sampled the leader's F0 at
  the anchor's MAPPED time, which under residual-cap is deliberately NOT the leader's
  word position; deltas would have been garbage. Blend/Variation apply at PUBLISH
  (`effectiveAlignPitchSemis`: cap first, excess scaled by blend) — shared by
  `publishAlignPlayback` + `buildWarpedFollower` so live == render.
- Back-compat note: pre-QA-Fd saves carry Range-baked deltas in the WarpMap; they
  read under-pulled until the next analyze (transition-state accepted, heals on any
  re-analysis; real-world exposure ~= Jeff's test projects since QA-F is days old).
- Params: `bsa_align_flex` (0..3 def Normal), `bsa_align_maxShift` (10..400 ms def
  400), `bsa_pitch_variation` (0..6 st def 0 — default 0 preserves the old
  pure-percent behavior), `bsa_pitch_typeGuide`/`bsa_pitch_typeDub` (5 bands:
  Normal/High Vocal/Low Vocal/High Instrument/Low Instrument; Hz calibrations mine,
  YIN window floors the practical low bound ~43 Hz); `bsa_pitch_range` display name
  -> "Align Pitch Blend" (id unchanged). Mode bases now RESIDUAL (Tight 0 / Close 50
  / Loose 100 ms, fine +/-50 clamped >= 0).
- Republish rules (reconciliation note): Blend/Variation knob hooks republish the
  snapshot live (time map identical); Mode/Fine/Flex/MaxShift/Types bump a new
  `mAlignSettingsGen` counter -> `isAlignStale()` ORs a gen mismatch -> stop-gated
  auto re-analyze; VoxPage poller XORs the gen into its debounce signature (else
  `lastAttempt` suppression would eat knob-only staleness at an unchanged grid).
  `setStateInformation` re-baselines the gen after replaceState (attachment sync
  fires the hooks during load — would false-stale every project open).
- Editor: Flexibility combo + Max Shift knob (Align box), Blend (stock attachment,
  mode PRESETS the value — `rangeWindowForMode`/`applyModeWindowToRange` window
  machinery deleted) + Variation knob + per-side Pitch Types combos (Pitch box);
  tooltips rewritten for residual semantics (ASCII, US).
- FOUND BUG (pre-existing, fixed in-batch): align editor Undo/Redo restored the map
  into `mAlignState` but never republished — playback kept the undone map while the
  UI showed the old one; an un-gated undo could also swap maps mid-play. Fix:
  `restoreEdits` republishes + re-baselines the gen; Undo/Redo join the stop-gate
  set (buttons grey during play + guards in doUndo/doRedo — consistent completion of
  the locked maps-only-change-while-stopped rule).
- 17b: corrector scale table rebuilt to the piano roll's 13 scales in piano-roll
  order (`bsv_pitch_scale` 0..12); ASSUMPTION CORRECTED vs the lock text: the diff
  is FOUR missing scales (Mel. Minor, Pentatonic Maj, Pentatonic Min, Blues), not 3
  — and the old table's "Custom" slot was DEAD (zero `setCustomScaleNotes` callers,
  no UI) so it retires without loss. Masks copied verbatim from PianoRoll.cpp
  kScaleDefs (8 shared scales' masks verified identical). "Key" -> "Root" on the
  realtime board (param ids unchanged; display names "Pitch Root" / "Align Pitch
  Blend" updated).

## 2026-07-11 — Task 2 code-complete — segmentation + analysis-UX + bypass removal

- Segmentation (`analyzeComposite`) restructured to runs -> merge -> materialize:
  onset-glide fragments merge FORWARD into the note they lead into (right-to-left
  pass; merge when near-contiguous AND sub-60 ms fragment OR a sub-120 ms ramp
  trending into the next run's pitch with >= 0.8 st internal range); merged glide
  frames extend the pill start but median/vibrato stats stay anchored on the stable
  tail (`anchorCount`) so ramps can't skew the note center. NOTHING silently drops:
  voiced leftovers + energetic unvoiced spans (frame RMS > max(2% peak, 1e-4))
  become slice pills (`PitchNoteRegion::isSlice`, persisted as "sl", absent =
  false back-compat). Edit carry now kind-matched (note vs slice).
- Applicator: slice regions apply volume shape only — pitch/focus/vibrato targets
  stay neutral, PSOLA period keeps its last real value.
- Analysis UX (4a): pitch editor gains an explicit state machine (Idle/Analyzing/
  Deferred/Failed) — canvas empty-state names its state, toolbar badge slot shows
  ANALYZING... / DEFERRED UNTIL STOP / ANALYSIS FAILED (wins over the stale text);
  analyze failure surfaces the error text instead of silence. FIRST analysis of a
  never-analyzed channel runs immediately even mid-play (no edits = fresh snapshot
  provably a no-op); RE-analysis stays stop-gated; a Deferred analysis fires from
  the editor timer at the next stop (faster than the poller debounce). Align editor:
  same carve-out on Analyze (never-analyzed pair analyzes mid-play — user intent,
  capped glide absorbs engagement; `setPlaybackGate(playing, analyzed)` un-greys the
  button accordingly) + ANALYZING... badge; both editors' sync analyze paths run
  after a 30 ms deferred hop so the badge actually paints (NIT-4 hitch itself rides
  Jeff's boundary judgment). Plan's "poller carve-out" line satisfied by
  construction — the VoxPage poller only ever RE-analyzes (guards on analyzed);
  first analysis is editor-owned.
- Bypass removal (3a/12b): `bsv_bypass` param + editor button + early return +
  record-lock gate membership + PluginProcessor conditional-WET master-bypass
  condition all deleted; old saves load fine (unknown params ignored). "Key/Scale"
  tooltip wording -> "Root/Scale".

## 2026-07-11 — Task 3 code-complete — time-edit engine (pitch upstream)

- Region model: `PitchNoteRegion` gains `dstStartSec/dstEndSec` (-1 = no time edit)
  + `detached`; persisted as "ds"/"de"/"dt" (absent = neutral -> old projects load
  unchanged). `dstStart()/dstEnd()/hasTimeEdit()` helpers.
- Published time map: `publishTimeMap()` (rides every `publishEdits`) builds an
  EDITED->SOURCE map REUSING `AlignPlaySnapshot` as the type (guideSec = edited,
  dubSec = source; same Fritsch-Butland/Hermite math, zero new interpolation code).
  Anchor set: identity pin at 0, per-region dst/src edge pairs (inter-region gaps
  map linearly = the gap counter-warp), slope-1 tail pin. Strict-monotone nudge on
  the dst axis turns detach jumps into ~1 ms ramps; src side deliberately NOT
  clamped (jumps legal). nullptr published when no time edits (decode fast path
  free). Retire-ring liveness identical to the edit snapshot.
- Decode composition: `decodeFilePlayClip` now computes `sourcePosAt(t)` =
  pitchMap(alignMap(t)) — align stage drops out when bsa_align_on off, pitch stage
  when bsp_on off; `warpLawAt = rawLawAt(sourcePosAt(t))`; the glide/seek machinery
  is law-agnostic and untouched. Block entries gain `pitchMap` + `pitchChainOn`
  (page-index matched — the map always describes the page's own channel; align
  stays followerChannelId-matched, so a cross-channel follower resolves BOTH maps
  correctly). `semisTarget` guard tightened to alignActive (was wantWarp — would
  have crashed pitch-only channels reading ae->pitchOn through null).
- Detach jumps at decode: a law step > 0.25 s (file domain) in the rebase detector
  = a CUT, not a bend (a backward jump can never drain — consumption is
  forward-only): posCorr zeroed + `forceSeek` routes through the existing
  seek path (vocoder reset + requestSeek). Brief refill gap at the boundary =
  the v1 "short crossfade"; smoke judges.
- Source-domain stamps (closes the wrong-syllable hole): decode writes per-page
  `srcX0/srcRate/srcSet` into the block entry (glide-exact: the outstanding
  correction folds back through the raw-law slope); `finalizeFilePlayStrip`
  forwards to the engine (identity default when no law); VoxStripTask's live
  monitor branch forwards too (no finalize on that path). Applicator resolves
  pills at `srcX0 + i*srcRate` when valid — both streams (FilePlay + monitor).
- Align consumes the EDITED performance: `AlignBuildParams.dubEditTransform`
  (follower raw->edited through `srcToEditedSec`, common-origin pad converted);
  applied to dub onsets (raw twin kept — anchor pitch deltas still sample RAW
  audio at raw positions), sync-point follower times, protected-area bounds, and
  the end anchor (edited duration). Onsets re-sorted in edited time (detach can
  reorder). Anchor dub times land in EDITED time = exactly the domain the composed
  decode law feeds them.
- Staleness: `AlignState.analyzedPitchMapHash` captured at analyze/revert/load;
  `isAlignStale()` ORs a mismatch vs `followerPitchMapHash()` (own channel direct;
  cross-channel via new `onPitchDspForChannel` hook wired in VoxPage through a new
  public `voxEngineAt()` accessor — mVoxEngines is private); poller sig XORs the
  hash so a time edit restarts the debounce.
- Deliberate Task-8 dependency: `buildWarpedFollower`/`applyWarp` (render/preview)
  still read the RAW follower at edited-domain anchor times — the render-path
  composition lands in Task 8 as planned (same batch, same build).

## 2026-07-11 — Tasks 4-7 code-complete — pitch editor rebuilt (one pass)

- Implementation shape: Tasks 4/5/6/7 all rework the same file, so
  `BaySickPitchEditor.h/.cpp` was REWRITTEN once to the final state instead of
  four conflicting passes. New sibling file `BaySickPitchSubEditor.h/.cpp`
  (popup) + CMake entry.
- Motion model (P1-14 + 14c): body drag = 2-axis (vertical 0.1 st / Ctrl 0.01
  fine; Snap ON lands on in-scale lanes via the shared snap static; horizontal =
  elastic dst-span move with collective never-cross clamp against non-selected
  neighbors — gap counter-warp falls out of the Task 3 map build); edge drag =
  stretch/squeeze (dst spans); Ctrl+horizontal(>3 px) = detach (free placement,
  amber outline). OLD EDGE-TRIM RETIRED (edges now stretch; re-partition = Slice
  + merge — flag for Jeff at the smoke per plan; Alt+edge-drag reserved).
- Root/Scale/Snap toolbar (13-scale shared table via new
  `PitchCorrectorDSP::scaleName/scaleMask/snapMidiToScaleStatic` statics — single
  source with the realtime board); Focus target = nearest semitone (Snap OFF) or
  the scale (ON), computed identically in the DSP applicator AND the pill
  renderer, so pills draw at the corrected pitch while the green curve stays
  as-sung (lock 8). FOUND BUG fixed in passing: the corrector's old pc-space
  snap walk had an octave-wrap defect (B snapping "up" to C landed an octave
  DOWN — `octave + snappedPc` ignores the wrap); the shared absolute-walk static
  replaces it and the realtime corrector now delegates to it.
- Pill right-click menu (18a): Restore to Original State (15b full reset) /
  Snap to Semitone / Merge With Next / Merge Selected / Open Sub-Editor / Zoom
  to Selection. Merge keeps the LONGEST member's pitch identity + edits and
  absorbs src+dst spans (calibration-class call, mine). Slice mode: 30 ms
  interior guard kept, works on slice pills, splits time-edited spans
  proportionally. Slice pills render in a dedicated bottom lane (my design call
  — they have no pitch identity to place on a lane).
- Batch re-pitch: keyboard-key click with a selection re-centers selected pills
  on that note; audition-hold = holding the key previews the focused pill
  through current edits (the preview-play path). Scrub-audition = hearing the
  pill re-render during a drag while stopped (150 ms throttle).
- View/nav (#5/#6/#8 + parity 2): Ctrl wheel h-zoom (cursor-anchored), Alt
  wheel v-zoom (mLaneH 4-24 px, cursor-anchored), Shift h-scroll, bare
  v-scroll, middle-drag pan, Ctrl+RMB-drag zoom-to-rect + plain-RMB-on-empty
  restores the saved view, menu Zoom to Selection; view floor at C0 (MIDI 12) in
  paint loops + keyboard + a dynamic setTopNote lower clamp.
- Playhead (#7): follows the MAIN transport at 30 Hz including stop-reset — new
  `onTransportBeat` hook chain (StandaloneEditor `mPlayHead.getCurrentBeat()` ->
  VoxPage::setTransportBeatProvider -> engine hook), beat -> TempoMap sample ->
  composite sec. Editor timer 400 ms -> 30 Hz with a ~2 s slow path for badges/
  length/diag. Auto-scroll (#8): edge-triggered page-flip ONLY while the
  playhead advances across the right edge during playback.
- Selection/clipboard/undo (Task 6): marquee + Shift-click toggle + Ctrl+A;
  Ctrl+C/V = edit-state transfer (20a: pitch offset + curves + vib/frm/
  variation from the FOCUSED pill onto the selection, one undo step); Ctrl+B =
  the family duplicate analog (copy focused + paste to selection); Shift+arrows
  = pitch nudge 0.1 st / time nudge 10 ms (elastic-clamped); Delete = Restore
  to Original State (pills are analysis segments). GLOBAL UNDO (9a): new
  `PitchEditAction` in UndoActions.h (full before/after region vectors,
  first-perform skip, SafePointer apply = safe no-op after tab close);
  UndoContext plumbed StandaloneEditor -> VoxPage -> BaySickVocalEditor ->
  pitch editor; toolbar Undo/Redo drive the app-wide stack (Ctrl+Z rides the
  existing global BSCommands binding); local undo stacks DELETED; every
  gesture/menu/popup/knob edit wraps in beginEdit/commitEdit.
- Preset combo retired (7a): bsp_preset/bsp_preset_dirty params gone (old saves
  unaffected — unknown values ignored); Save/Load user presets (the 3 knobs)
  kept; dirty-dot/mirror machinery deleted. New bsp_root/bsp_scale/bsp_snap
  params; pushApvtsToDsp feeds the applicator's snap target.
- Sub-edit system (Task 7, Jeff's design): display-only box under the selected
  pill — 4 bars VIB/FRM/VOL/PITCH, EDIT button (opens the popup) + RESET button
  (boxed params of the selection only; multi-select reset prompts with a
  never-show-again checkbox persisted app-wide in Documents/BaySickDAW/
  ui_prefs.xml). Popup `BaySickPitchSubEditor`: lane = pill length with the
  waveform ghosted (post-gain), EventEditor gestures (click add / drag move /
  right-click delete), Volume/Pitch lane toggle, ordered pill browser
  (selection-filtered from multi-select entry), own Play button + SPACE while
  focused (streams through the new processor-side preview player, pre-rack so
  it is voiced like playback), bipolar Vib/Frm + Variation knobs (each gesture
  = one undo step; typed entries self-wrap).
- On-pill handles (19a single-storage contract): 4 corner handles on the
  single-selected pill (hidden below zoom threshold) — top = volume fade in/out
  (horizontal = length), bottom = pitch approach/release (2-axis: length +
  semitone offset); green=in/red=out + hover arrows; handles write ORDINARY
  points into the SAME volShape/pitchShape the popup edits (span-replace on
  re-drag = the explicit overwrite, one undo step); pill waveform drawn
  post-gain so fades show on-canvas; InfoBar live readout during handle drags.
- DSP for the box/popup: `PitchNoteRegion` gains `pitchShape` points (additive
  semis over t01, "pit" persistence) + `variation` ("va", default 1); the
  applicator applies pitchShape additively and Variation as an F0-track-
  deviation scaler (snapshot now carries the analysis F0 track) — Variation
  genuinely flattens/exaggerates the note's own wiggle. FL knob conventions
  (P1-14): new shared `applyFLKnobFeel` (SharedUI.h) — detent at default
  (Shift bypasses), Ctrl = fine velocity drag, editable value box for type-in —
  applied to the pitch editor knobs, popup knobs, and all six align-editor
  rotaries.

## 2026-07-11 — Tasks 8-9 code-complete — render parity + high-res + engage fix

- applyWarp Phase 1 PORTED (Task 8): the per-anchor-segment assembly (constant
  ratio per segment = steps at every anchor) is replaced by ONE streaming
  PhaseVocoder whose ratio follows the same Fritsch-Butland/Hermite slope the
  live decode glides on, hop-quantized like the live rReq/rEff feedback;
  per-window produced-length bookkeeping reads each output window from its
  exact span so hop quantization never accumulates as drift. `pvStretchExact`
  deleted with the old assembly (own-batch dead code).
- Renders honor time edits (Task 8): `buildWarpedFollower` composes
  guide -> align(edited) -> pitch-map(source) by sampling a dense guide set
  (align anchor guides + pitch anchors' dst pulled back through a monotone
  bisection of the align lookup; common-origin pad converted); the composed
  WarpMap feeds the ported applyWarp, so render == live law. renderPitchedTake
  post-warps the pitched bake through the published time map (phase-1-only
  applyWarp reuse).
- High-Res render (16a): both Render buttons now open a Standard / High
  Resolution (slower) / Cancel dialog; High-Res runs the WARP phase at
  sampleRate x round(384000/sr) (8x at 44.1/48k = 384 kHz-class) via
  Lagrange up/down-sampling (8-sample guard pads — the interpolator reads past
  ratio*numOut); the pitch algos stay at device rate (their trackers assume
  it; the YIN window would break at 352k+). Memory note: a 3-minute take
  peaks at several hundred MB transient during a High-Res render (message
  thread, one-shot) — owner judges at the smoke.
- Granular/Pv desk-check (Task 8): CONFIRMED not the Psola bug class at
  implementation desk — GranularShifter reads at `readPos += ratio` with a
  ratio-scaled spawn lag (clamps only retire grains early); PvShifter's
  consumption is `mReadPos += mEffRatio` with priming holding silence without
  advancing. Render-algo listen stays in the smoke.
- Engage-tick fix (Task 9, locked 11/16a design iii): `PsolaShifter` gains
  `feedSample` (ring write only — zero synthesis, zero latency) +
  `resyncToWriteHead`; the corrector's fully-bypassed path now warm-feeds the
  rings + tracks the period, and the engage/disengage edge runs a ~40 ms
  equal-power crossfade (sin/cos) between dry and corrected taps — the
  formant-engine engage reset rides the same fade. OFF path adds zero latency
  (Jeff's requirement); fade step is construction-safe. Calibration (40 ms)
  tuned at the realtime first-listen.
- Addendum (drafter-flagged omissions, all shipped in the Tasks 4-7 pass):
  in-block pitch readout inside pills at sufficient zoom (laneH >= 12, width
  >= 44); double-click pill = open the sub-editor popup; #9 fix — the InfoBar
  refreshes on undo/redo/reset (`applyRegionsFromUndo` + `commitEdit` both
  re-drive it) so the [edited] tag is truthful, and `hasEdits`'s existing
  0.01 epsilon absorbs quantize residue.

## 2026-07-11 — Task 10 — docs + test plan + Main Plan edits

- Master Test Plan: §B.10 authored (FD-1..FD-20 from the tasks' verify lines;
  blocks: backfills at commit). Amendments beyond the planned four — the
  reconcile-sweep over the doc found more scenarios describing retired
  behavior: F-3/F-4 rewritten (residual semantics + Blend/Variation/Types),
  F-6 (knob-staleness + Undo/Redo gate + first-analysis carve-out), F-10
  (Key->Root wording), FA-2/FA-3/FA-5 (edge-trim / draggable sub-curves /
  preset combo superseded markers + §B.7 preamble note), FA-9 (4a carve-out +
  visible states), FB-3 (case 3 retired with bsv_bypass), FB-11 (Bypass out
  of the gate set, Key->Root).
- Main Plan: §5 QA-Fd entry (after QA-Fc, with the reused-ID disambiguation —
  the 2026-05-14 "QA-Fd" was a dropped conditional wiring batch, standup
  premise verified at §9 eighteenth entry + :1633); §6 arrow `-> QA-Fd`
  (41 asterisks) + footnote; §9 fifty-sixth Forks entry (G2 composition
  change). Caribou plan: G2 composition note appended (QA-Fd = fifth batch;
  Carry-Over FINAL rides this batch's commit per 2b).
- Held Work Log entry drafted via /draft-doc batch-close, applied below per
  the QA-F/Fb'/Fc parking convention. Drafter's factual flags resolved:
  files list corrected against git status (VoxPage.h + BaySickVocalEditor.h
  are touched; no StandaloneEditor.h/SharedUI.cpp changes), FD range +
  amendment list filled, three omitted-but-shipped items added (the addendum
  above), Bucket widened to the QA-F precedent's three (Players / Effects /
  Cross-cutting Infrastructure — DSP shifter/corrector surface + decode
  layer + MT task + app-wide undo plumbing); Main Plan §5 Bucket line
  aligned to match. §B.9 blocks: already backfilled at the boundary session
  (nothing owed).

## 2026-07-11 — /review-batch outcome (code-complete review, pre-build)

- Verdict: **0 BLOCKER / 6 NEEDS-FIX / 7 NIT** (every finding premise-verified by
  the reviewer against source). ASCII/US-spelling/RT-safety/CPU-guard sweeps all
  clean; plan-vs-diff alignment confirmed (no undocumented scope creep; the two
  found-bug fixes + fold-ins all documented).
- All 6 NEEDS-FIX FIXED in-tree pre-build:
  1. `clearAllEdits` now clears the QA-Fd edit classes too (pitchShape,
     variation, dst spans, detached) — matches the toolbar tooltip + the
     restore-all semantics.
  2. Re-analysis edit-carry now carries pitchShape/variation AND the time
     edits (FA-9's carry contract — a grid nudge must not delete moves).
  3. `hasEdits` ignores neutral shape residue (`shapeIsMeaningful`) — the
     missed Task 5 #9 plan line; un-latches the [edited] tag + the audio
     fast path after a handle is dragged back to zero.
  4. Load-time Blend stomp: `isRestoringState()` flag (shared_ptr token,
     cleared via queued callAsync AFTER the restore's posted hooks) — the
     mode hook's async Blend preset no longer overwrites a restored Blend.
  5. `warpContinuous` hardened: feed chunks capped at 4096 (PV input ring
     overflow on steep detach ramps), rReq clamped to the PV trio's
     documented [0.25, 4] (plateau windows demanded synthesis hops beyond
     the rings), AND the render now SPLITS into monotone-source segments at
     backward jumps — a detached pill that reorders material renders
     correctly (clean splice = the render analog of the live forceSeek cut)
     instead of flattening; fixes live/render parity for FD-9/FD-17.
  6. `mergeSelection` bounds-guards its indices (async menu vs a deferred
     re-analysis shrinking the region list).
- 5 of 7 NITs also taken (cheap spec-conformance): popup Play stop-gated;
  realtime scale combo loops the shared `scaleName()` table (no literal
  copy); formant engines reset on the engage edge (stale-ring blend);
  one-in-flight guard on the deferred analyze; time-map tail pin anchors on
  the max-dst region (detached-last-pill case).
- 2 NITs deferred (benign): a zero-movement handle click lands one no-op
  undo entry; handle re-drags can leave neutral leftover points visible in
  the popup (audibly no-ops; hasEdits now ignores them).

## 2026-07-11 — Post-build owner-findings fix round — 5 pitch-editor findings + 2 in-batch additions (fixes coded, build pending); smoke re-issued 6-part

- Jeff built the code-complete tree CLEAN on the first try, then poked the pitch
  editor ahead of the smoke — five findings + two in-batch additions came back. All
  fixes coded this checkpoint (build pending). The G2 boundary smoke was RE-ISSUED in
  the plan's 6-part structure (Jeff caught that the first hand-off was the full B.10
  rig with Parts 4-6 missing) and now runs on the NEXT build.
- Finding 1 (placement): Root/Scale/Snap combos landed next to "Send Notes to..." in
  toolbar row 1 instead of directly above Undo/Redo as specified. Fix: right-aligned
  in row 1 above row 2's Undo/Redo stack; the ANALYZING/RE-ANALYZE badge re-anchored
  to the Root combo's left edge (was knob-block-relative — would have overlapped).
  File: BaySickPitchEditor.cpp (Toolbar::resized + paint).
- Finding 2 (frozen canvas): turning Focus / toggling Snap / changing Root/Scale while
  the transport is stopped never repainted the canvas — the 30 Hz timer only repaints
  on playhead movement — so the Focus drift + Snap re-target looked completely dead.
  Fix: fast-path render-signature watch (focus+snap+root+scale+mode composite) ->
  canvas repaint + mode-button mirror on any change; mirrorMode moved off the ~2 Hz
  slow path onto the watch. Files: BaySickPitchEditor.cpp/.h (new `mRenderSig`).
- Finding 3 (inaudible popup knobs): the Vib/Variation knobs + drawn pitch-lane curves
  routed through the Speed knob's ~150 ms one-pole glide smoother in the DSP
  applicator — a 5-6 Hz vibrato term comes out ~15 dB down (a few cents), i.e.
  mathematically near-inert. Fix: the applicator's per-sample pitch split into a
  smoothed note-CENTER (shiftSemis + Focus pull; glides at Speed as before) plus
  post-smoother fastSemis (pitchShape curve + Variation deviation + Vib sine) summed
  into the shifter ratio AFTER the glide; renderOffline inherits via the shared
  applyEditsToBuffer. Also: popup knob turns mid-gesture now publish live (snapshot
  republish per change) so main-transport playback tracks the drag; undo still lands
  once per gesture. Files: BaySickPitchDSP.cpp, BaySickPitchSubEditor.cpp.
- Finding 4 (slice mode reads as select): in Slice mode a pill click ran the selection
  update FIRST (plus display-box/handle/popup intercepts), and a split of a same-pitch
  pill drew two seamless abutting fills — so slicing looked like "it just selects".
  Fix: slice branch hoisted to the top of mouseDown — in Slice mode there is no
  marquee, no selection update, no handle/display-box intercepts, no double-click
  popup; a pill click splits immediately (30 ms interior guard kept). Always-on dark
  outline added to every pill (both lanes) so abutting halves show a seam; I-beam
  cursor over pills while Slice is armed. File: BaySickPitchEditor.cpp (PitchCanvas
  mouseDown/mouseMove/paint).
- Finding 5 (popup box style — owner call: uniformity with the Event Editor): the
  sub-edit popup was an inline rounded-overlay box, visually unlike the Event Editor.
  Fix: rehosted as `BaySickPitchSubEditorWindow` — a juce::DocumentWindow on the
  Event Editor's exact conventions (title-bar color 0xff1a1c1e, allButtons, resizable
  560x320..1600x900, screen-centered 760x430, close via X/Esc routes to the owner
  which resets its unique_ptr). Content component kept; its self-drawn title label +
  X button stripped (window chrome owns both; window title shows "Pitch Sub-Editor -
  <note> (<len>s)", updated per pill switch). Files: BaySickPitchSubEditor.h/.cpp,
  BaySickPitchEditor.h/.cpp.
- Addition A (owner-routed, in-batch) — FOUND BUG (pre-existing): Event Editor
  undo/redo appeared dead — undo/redo reverted the AutomationLane data but nothing
  repainted the grid (the 24 Hz timer only watches block count). Fix:
  EventEditorContent is now a ChangeListener on the shared UndoManager — any
  perform/undo/redo (from the EE window or the main window) repaints the grid +
  refreshes the value display. Files: EventEditor.h/.cpp.
- Addition B (owner spec call resolved, option a): Help-menu Key Binds window gains a
  fifth tab "Vocal Editor Key Binds" — `Category::VocalEditors` reference-only rows
  (none rebindable, same deal as Piano Roll local keys; rows feed
  findHardcodedConflicts so future rebinds warn on clashes). 20 rows covering the
  pitch editor's keys (A, Ctrl+A/C/V/B, Shift+Arrows, Delete = restore, Esc,
  SPACE-in-popup) and mouse gestures (drag/fine/detach/stretch, slice click, wheel
  matrix, zoom-rect, pan, pill menu, sub-editor lane gestures), plus the align
  editor's mouse-only surface (sync-point drag/menu, protected-area paint/menu).
  Files: KeyBindings.h/.cpp, KeyBindsWindow.cpp.
- STILL OPEN (runtime, not desk-resolvable): Jeff also reported drags/Focus made no
  AUDIBLE difference. Every static link desk-checks clean (params registered -> DSP
  atomics -> applicator target math -> publish chain carries regions + f0Track). The
  [PITCH DIAG] InfoBar readout (armed via Documents/BaySickDAW/enable_pitch_diag.txt)
  is folded into the re-issued smoke Part 2 to disambiguate: null/neut/inReg/chg
  counters isolate snapshot-missing vs publish-gate vs domain-mismatch vs
  shifter-inert if deadness persists on the fixed build.
- Held-entry amendments owed at apply time (the held draft below predates this
  round): (1) "ZERO new owner spec calls" is stale — Finding 5 + Addition B were
  owner calls; (2) files list gains EventEditor.h/.cpp + KeyBindings.h/.cpp +
  KeyBindsWindow.cpp; (3) Found-along-the-way gains Addition A; (4) Done section
  gains the DocumentWindow rehost, render-signature watch, fast/slow pitch-term
  split, and the Key Binds fifth tab. Bucket-line widening for the shared
  Standalone surfaces = Jeff's call at apply.

## 2026-07-11 — Second owner-findings round — align editor (renders bar / Fine / Max Shift / Flexibility) + a builder Move regression (fixes coded, build pending)

- Jeff poked the reworked align tab and surfaced five more items — three fixed, one
  discussion-only (Flexibility), one a builder regression he demanded a why/fix/prevention
  story on. All coded this checkpoint (build pending) EXCEPT the Flexibility call, which
  waits on his word.
- Item 1 (renders bar — owner directive): the align editor's bottom HistoryScrubber strip
  (render-version list + Del + zoom +/-) listed renders but drove nothing, and Del only
  removed the LIST entry (the wav stayed in the project's Aligned/ folder, no other in-app
  surface). REMOVED the strip entirely (class + member + layout slot + `kHistoryH`); the
  time-zoom +/- buttons moved onto the ViewModeBar (Wave/Pitch/Energy bar, right-aligned).
  `AlignState.renders` vector + persistence + the on-disk files are untouched (a write-only
  record now). Files: BaySickAlignEditor.h/.cpp.
- Item 2 (Fine knob respec — owner spec, verbatim): old model = bipolar +/-50 ms offset
  around the mode base (Tight 0 / Close 50 / Loose 100) clamped >= 0 — so on Tight the
  knob's whole LEFT half was dead (readout pinned at 0, "goes left to more 0"). NEW model:
  Fine sweeps the mode's ABSOLUTE residual window — Tight 0-50 ms (center 25), Close 50-150
  (center 100), Loose 100-200 (center 150), 12 o'clock = window center. Param stays
  -50..+50 normalized (no migration); effective ms = center + v*halfWidth/50 (halfWidth
  25/50/50). Twin math: `BaySickVocalProcessor::alignResidualCapSec()` + the editor's
  `fineTuneCenterForMode`/`fineTuneHalfWidthForMode`; readout `textFromValueFunction` shows
  live ms across the FULL travel now; tooltip + header comment rewritten. Semantic shift
  noted: the mode DEFAULTS move 0/50/100 -> 25/100/150 ms (old projects' baked maps play
  unchanged until the next analyze). Test plan FD-1 amended in place. Files:
  BaySickVocalProcessor.cpp, BaySickAlignEditor.cpp/.h, v1-master-test-plan.md.
- Item 3 (Max Shift vs the reference aligner — owner review request): shipped 10-400 ms
  default 400 — Jeff recalled the research said it should track VocAlign and be lower.
  Web-verified: VocAlign's Maximum Shift range is 10-150 ms, PLUS a "No Limit" setting that
  is the usual default (it auto-sets ~70 ms only for the gappy-dub preset). Reworked: param
  10..160 default 160; > 150 displays "No Limit" and `alignMaxShiftSec()` returns uncapped
  (1e9); knob text + tooltip match. No brand name in the UI strings (VocAlign cited in code
  comments only, nominative). Old saves at 400 clamp to 160 = No Limit (behavior preserved
  except offsets that used to cap at 400 ms are now uncapped — noted). Test plan FD-3
  amended. Files: BaySickVocalProcessor.cpp, BaySickAlignEditor.cpp, v1-master-test-plan.md.
- Item 4 (Flexibility combo — DISCUSSED then DECIDED + REMOVED): Jeff found only Normal
  sounded right (Low dropped valid pairings, High/Max All smeared); confirmed my read that
  it's half-broken, not mis-tuned. His call: remove the picker, keep Normal running in the
  background. IMPLEMENTED: `bsa_align_flex` param + the `mFlexCombo` UI (combo, attachment,
  bumpGen hook, caption, layout row, members) + the preset-id list entry all removed;
  `alignFlexRatio()` now returns the fixed 2.0 (Normal) unconditionally; the DSP slope-bound
  code + `AlignBuildParams.flexRatio` stay parameterized so the control can return without
  DSP work. `mAlignBoxH` 176 -> 140 (reclaimed the 36 px row). Stale-arming comments
  (processor.h x2, VoxPage.cpp) + the DSP rung comment updated to drop the dead control;
  test plan FD-3 retitled "Max Shift guard" (Flexibility removed). Old saves: `bsa_align_flex`
  is silently ignored on load (unknown param). Files: BaySickVocalProcessor.cpp/.h,
  BaySickAlignEditor.cpp/.h, BaySickAlignDSP.cpp, VoxPage.cpp, v1-master-test-plan.md.
- Item 5 (builder Properties "Move" regression — owner demanded why/fix/prevention): grid +
  browser wave Properties offered only Copy / Create-page; Jeff says it "used to be
  move/copy/create". INVESTIGATION (git pickaxe, not a guess): the Move/Copy code was BORN
  in one commit (714f0749, QA-E Task 7 FILE-02, 2026-05-17) and untouched since; the
  per-clip dialog was deliberately set copy-only there per Jeff's OWN 2026-05-15 call,
  recorded ONLY in a code comment ("per-clip routing pick = COPY (Jeff 2026-05-15)"). The
  BROWSER dialog kept Move the whole time (one submenu level down: Move here / Copy here per
  target). Pre-QA-E, the per-clip routing dropdown was a plain reassign (move) picker per
  FILE-02's original user-confirmed spec — so from Jeff's seat a capability vanished with no
  owner-facing paper trail. FIX (owner call 2026-07-11, reverses 2026-05-15): the grid
  per-clip dialog now `offerMove=true` (same Move here / Copy here submenus as the browser);
  new per-clip Move branch re-routes THIS block in place (`block.routeChannel = target`,
  page spun up on the create-page entries, NO file fork, library master untouched,
  follow-state derives red/green including the BPM term). Browser dialog unchanged. File:
  BuilderPage.cpp (`showAudioClipProperties` + its Apply handler). PREVENTION: new standing
  rule memory-filed (`option-removal-needs-loud-paper-trail`) — any change that removes or
  degrades a user-facing option gets an explicit removal line in the batch plan + running
  notes at the moment it happens, even for Jeff's own chat calls. Closed-batch carry-forward:
  the fix landed in open QA-Fd, so a §9 Forks back-ref to QA-E Task 7 is owed at batch close.
- Held-entry amendments this round adds (apply time): the QA-Fd 10a residual-base numbers in
  the Task-1 notes (Tight 0 / Close 50 / Loose 100) are SUPERSEDED by the Fine-window model;
  the held Work Log entry gains the renders-bar removal, the Fine-window respec, the Max
  Shift No-Limit rework, the builder Move restore, and the Flexibility-pending note; and
  BuilderPage.cpp joins the touched-files list (outside QA-Fd's three original buckets — a
  System Pages / builder bucket widening, Jeff's call at apply).

## 2026-07-11 — Third owner-findings round — align follower combo removed + grid Move relinked + PSOLA overlap-add fix (#1)

- Align FOLLOWER channel picker REMOVED (owner: the follower is always the page you're
  on).  `resolveFollowerChannel()` now returns `mOwnChannelId` unconditionally
  (`bsa_follower_channel` param kept but vestigial); the Follower LaneView loses its combo
  and shows a static "(this page)"; `rebuildChannelCombos` only builds the Leader list.
  Files: BaySickVocalProcessor.cpp, BaySickAlignEditor.cpp.
- Builder per-clip Move: my first "restore" (2026-07-11 second round) was on a FABRICATED
  premise -- git `714f0749^` proves pre-QA-E per-clip Properties had NO routing at all
  (pitch/BPM/mode only); "move/copy/create" is the BROWSER dialog, still intact.  That
  broken block-only Move (reverted on play) was REVERTED, then REIMPLEMENTED correctly per
  owner request: the grid dialog now offers Move + Copy and the Move shares the browser's
  exact `onApplyLibraryProperties` lambda (wired `grid->onApplyLibraryProperties =
  panel->onApplyLibraryProperties`), so grid + browser Move are one linked code path
  (relocate the library entry + following copies).  Detached/customized copies follow the
  browser's skip-until-reattached behavior (noted to owner).  Files: BuilderPage.h/.cpp,
  StandaloneEditor.cpp.
- Pill-edit CHOPPINESS root-caused (owner re-test: continuous, both directions, on any
  pill move).  Up/down proved it's NOT the time-map/warp path (pitch-only edits build no
  time map) -- it's the PSOLA pill shifter.  Traced: grains spawn at the SYNTHESIS spacing
  `pOut = period/ratio` but are a fixed 2*period long, so any downshift (pOut > period)
  loses overlap and by pOut >= 2*period the window-sum collapses to a per-epoch silence
  notch (the chop).  Confirmed clean at ratio 1, degrading with distance from it (why the
  realtime corrector's small nudges sounded OK).  Owner asked "which path reaches Newtone"
  -- researched: clean monophonic vocal editing IS TD-PSOLA done right (crisp transients);
  granular is a texture/smear tool, the wrong direction.  So NOT the granular swap -- FIX
  the PSOLA.  #1 (this commit): grain half-window grows with pOut (`hw = max(P, pOut)`) so
  adjacent grains keep >= 50% overlap in both directions; upshift unchanged (tight
  one-period window, crisp).  Trade: a little latency + spectral blur on deep downshifts
  (inherent TD-PSOLA).  File: PitchShifters.h (PsolaShifter::processSample + spawnGrain).
- REGRESSION SOURCE (owner-identified, git-confirmed): #1 alone did NOT fix the chop --
  owner pinned it to "the realtime-pitch bypass gate" era.  git `9262c746^` shows the
  pre-QA-F realtime corrector used a GRANULAR shifter ("simple but reliable for vocals",
  2 Hann grains / 50% overlap); QA-F's "realtime pitch quality pass" REPLACED it with the
  new PsolaShifter, which the new pill editor also uses.  That PSOLA lacked real
  pitch-mark detection -- it snapped grain centers to a uniform k*P grid tied to the write
  head (the 703f06e4 "grid-anchor" patch), which drifts in phase against the actual pitch
  cycles -> a per-period discontinuity in BOTH directions = the chop.  My swap-to-granular
  recommendation was owner-rejected (granular != Newtone; owner: do the real work).
- #2 DONE (pitch-mark PSOLA, this commit): added epoch detection to PsolaShifter --
  predict the next mark one period on, snap to the strongest |sample| within +-P/4
  (peak-locked, phase-consistent), keep a 32-mark ring; synthesis now centers each grain
  on the nearest DETECTED epoch to a fixed-latency read target instead of the grid.  Real
  glottal-pulse alignment makes consecutive grains phase-aligned so the overlap-add
  reconstructs a clean voice at the shifted pitch (the Newtone path).  Epochs detected in
  both processSample and feedSample (warm across the corrector's bypass); reset clears the
  mark ring.  ~O(1) amortized (one peak search per period).  #3 formant preservation
  already wired (cepstral engine).  File: PitchShifters.h (detectEpochs/nearestEpoch +
  epoch state + processSample rework).  Awaiting owner build + listen.

## 2026-07-12 — Pitch-shift residual-pitch BEATING root-caused + 3-part PSOLA fix (owner-approved)

- Long chop saga resolved to its real cause via MEASUREMENT (rendered a master wav; pure-Python
  envelope analysis): the "chop" on a ~2.4-semitone move is amplitude BEATING at 15-33 Hz
  (~pitch-difference of original 220 Hz vs shifted 253 Hz), not silence gaps (diag floor was low
  the whole time).  The shifter was leaking a remnant of the ORIGINAL pitch that beat against the
  shift.  (Ruled out the Mix knob: needDry gate is off at 100% default -- dead code.)
- 3-agent workflow (psola-beating-diagnosis) verified owner's three diagnostic angles, all real:
  (1) YIN F0 returns an INTEGER lag, no parabolic refine (~4 cents); (2) the grain period is a
  FROZEN per-note median (BaySickPitchDSP.cpp:635) set once per note, never tracking the live
  pitch, so the Hann window zeros drift off the neighbour glottal pulses and leak them at the
  original period; (3) epoch peak-pick uses max |sample| -> half-period POLARITY jitter.  Grain
  skeleton itself (Hann peak on the detected mark) was confirmed correct.
- FIX (owner approved all 3):
  1. LIVE per-frame period into the shifter -- applicator now samples the analysis f0Track
     (linear-interp between frames) every sample and setPeriodSamples continuously, instead of the
     once-per-note median.  Primary fix (window zeros stay on the real neighbour pulses ->
     single-pulse isolation -> no leak).  File: BaySickPitchDSP.cpp.
  2. SUB-SAMPLE YIN (parabolic vertex on the squared-difference function) + finer F0 track
     (kF0Hop 2048->512, 75% window overlap; segmentation gates kGapFrames/kSplitHoldFrames scaled
     x4 so sample thresholds are byte-identical -> note detection unchanged).  Files:
     BaySickAlignDSP.cpp, BaySickPitchDSP.h/.cpp.
  3. POLARITY-LOCKED epochs -- signed peak-pick locked to the seed pulse's sign (no half-period
     flip) + seed on a real detected pulse instead of an arbitrary write-head offset.  New
     mEpochPolarity member.  File: PitchShifters.h.
- Stays entirely within PSOLA (owner rejected engine swaps twice).  [PITCH DIAG] still armed for
  the confirm listen; strip after.  Awaiting owner build + ear check (beating gone?).
- STILL OPEN (separate decode bug, not the shifter): sideways pill move doesn't reposition the
  audio + chops via the decode PhaseVocoder warp (floor stayed ~300s = not the grain shifter).

## Held Work Log entry (apply at section pass)

> Apply to `Implemented Work Log.md` when §B.10 passes (R2). Stamp `HH:MM PT`
> at apply time. Drafted at code-complete 2026-07-11; /review-batch outcome
> appended below when resolved.

```markdown
### 2026-07-11 — QA-Fd — Vocal Editor Rework: pitch editor rebuilt (parity gestures + time edits + global undo + sub-edit system + on-pill handles) + pitch-upstream time-edit engine + align residual-cap semantics with publish-time Blend/Variation + no-silent-drop segmentation/slice pills + bypass removal + render parity/high-res + engage-tick crossfade

**Bucket:** Players, Effects, Cross-cutting Infrastructure
**Plan:** `Batch Plans/snug-orbiting-catmull.md` · **Running notes:** `Running Notes/snug-orbiting-catmull.md` · **Commit:** `<hash at commit>`

#### Done

- **Execution shape (Jeff, session start).** NO per-task build stops — the entire 10-task plan coded in one session to a single code-complete handoff (one build check, then the boundary smoke script); the plan's per-task build checkpoints collapsed accordingly. All 10 tasks code-complete 2026-07-11. Largest batch of the run; fifth batch of G2 — the boundary stays open and closes after THIS batch's smoke completion (Parts 4-5 + FB-11 + Part-3 re-runs + realtime first-listen).
- **Task 1 — align semantics rework + publish-time pitch controls.** Matching window now INTERNAL (`kMatchWindowSec` 0.4 s); `analyzeOffline` reworked around `AlignBuildParams { residualCapSec, maxShiftSec, flexRatio, per-side Hz bands }`. Residual-cap anchor build (move = min(max(|d|-R, 0), M); sync points exempt); Flexibility slope bound [1/r, r] replaces the 0.5..2.0 literals (Max All skips the pass; the live glide 2:1 cap is separate and untouched); END anchor now offset-continues the last anchor at slope 1 — the old dubDur->guideDur pin imposed a duration-equalizing tail stretch contradicting "within cap = natural timing" (calibration-class call, mine). Pitch deltas stored RAW and sampled at the RAW pair positions; Blend/Variation apply at PUBLISH (`effectiveAlignPitchSemis`: cap first, excess scaled by blend), shared by `publishAlignPlayback` + `buildWarpedFollower` so live == render. New params: `bsa_align_flex` (0..3, def Normal), `bsa_align_maxShift` (10..400 ms, def 400), `bsa_pitch_variation` (0..6 st, def 0 = old pure-percent behavior), `bsa_pitch_typeGuide`/`bsa_pitch_typeDub` (5 bands: Normal/High Vocal/Low Vocal/High Instrument/Low Instrument; Hz calibrations mine, YIN window floors ~43 Hz); `bsa_pitch_range` display name -> "Align Pitch Blend" (id unchanged); mode bases now RESIDUAL (Tight 0 / Close 50 / Loose 100 ms; fine +/-50 clamped >= 0). Republish reconciliation shipped as locked: Blend/Variation knob hooks republish the snapshot live (time map identical); Mode/Fine/Flex/MaxShift/Types bump a new `mAlignSettingsGen` -> `isAlignStale()` -> stop-gated auto re-analyze; the VoxPage poller XORs the gen into its debounce signature. Align editor: Flexibility combo + Max Shift knob + Variation knob + per-side Pitch Types combos; mode now PRESETS the Blend value (the `rangeWindowForMode`/`applyModeWindowToRange` window machinery deleted); tooltips rewritten for residual semantics (ASCII, US). Realtime board (17b): corrector scale table rebuilt to the piano roll's 13 scales in piano-roll order (`bsv_pitch_scale` 0..12, masks copied verbatim from PianoRoll kScaleDefs); "Key" -> "Root" (param ids unchanged). Back-compat: pre-QA-Fd saves carry Range-baked deltas in the WarpMap -> read under-pulled until the next analyze (accepted transition state; heals on any re-analysis; real-world exposure ~= Jeff's test projects).
- **Task 2 — no-silent-drop segmentation + analysis UX + bypass removal.** `analyzeComposite` restructured runs -> merge -> materialize: onset-glide fragments merge FORWARD into the note they lead into (near-contiguous AND sub-60 ms fragment OR a sub-120 ms ramp trending into the next run's pitch with >= 0.8 st internal range); merged glide frames extend the pill start while median/vibrato stats stay anchored on the stable tail. NOTHING silently drops — voiced leftovers + energetic unvoiced spans (frame RMS > max(2% peak, 1e-4)) become slice pills (`PitchNoteRegion::isSlice`, persisted "sl", absent = false back-compat); slice regions apply volume shape only (pitch/focus/vibrato neutral, PSOLA period holds its last real value); edit carry kind-matched. Analysis UX (4a): explicit Idle/Analyzing/Deferred/Failed state machine — canvas empty-state names its state, toolbar badge shows ANALYZING... / DEFERRED UNTIL STOP / ANALYSIS FAILED, analyze failure surfaces the error text instead of silence; FIRST analysis of a never-analyzed channel runs immediately even mid-play, RE-analysis stays stop-gated, a Deferred analysis fires from the editor timer at the next stop; align editor gets the same never-analyzed carve-out on Analyze (`setPlaybackGate(playing, analyzed)`) + the ANALYZING... badge; both sync analyze paths hop 30 ms deferred so the badge paints. The plan's poller carve-out holds by construction (the poller only ever RE-analyzes). Bypass removal (3a/12b): `bsv_bypass` param + editor button + early return + record-lock gate membership + the PluginProcessor conditional-WET master-bypass condition all deleted (old saves load fine — unknown params ignored); "Key/Scale" tooltip wording -> "Root/Scale".
- **Task 3 — time-edit engine (pitch upstream of align).** `PitchNoteRegion` gains `dstStartSec/dstEndSec` + `detached` (persisted "ds"/"de"/"dt", absent = neutral -> old projects load unchanged). `publishTimeMap()` (rides every `publishEdits`) builds an EDITED->SOURCE map REUSING `AlignPlaySnapshot` as the type (guideSec = edited, dubSec = source; same Fritsch-Butland/Hermite math, zero new interpolation code): identity pin at 0, per-region dst/src edge pairs with linear inter-region gaps (= the gap counter-warp), slope-1 tail; strict-monotone nudge on the dst axis turns detach jumps into ~1 ms ramps; nullptr published when no time edits (decode fast path free). Decode composition: `decodeFilePlayClip` computes `sourcePosAt(t) = pitchMap(alignMap(t))` with either stage dropping out when off; the glide/seek machinery is law-agnostic and untouched; a law step > 0.25 s in the rebase detector = a CUT (backward jumps can never drain) -> posCorr zeroed + `forceSeek` through the existing seek path (vocoder reset), brief refill gap = the v1 short crossfade (smoke judges); `semisTarget` guard tightened to alignActive (would have crashed pitch-only channels). Source-domain stamps close the wrong-syllable hole found in planning: decode writes per-page `srcX0/srcRate/srcSet` (glide-exact), `finalizeFilePlayStrip` + VoxStripTask's live monitor branch forward them, and the applicator resolves pills at `srcX0 + i*srcRate` on both streams. Align consumes the EDITED performance (`AlignBuildParams.dubEditTransform`: dub onsets, sync-point follower times, protected bounds, end anchor all in edited time; onsets re-sorted; the raw twin kept so anchor pitch deltas still sample RAW audio at raw positions). Staleness: `AlignState.analyzedPitchMapHash` captured at analyze/revert/load; `isAlignStale()` ORs a mismatch (own channel direct; cross-channel via the new `onPitchDspForChannel` hook + public `voxEngineAt()` accessor); the poller signature XORs the hash so a time edit restarts the debounce.
- **Tasks 4-7 — pitch editor rebuilt in ONE pass.** All four tasks rework the same file, so `BaySickPitchEditor.h/.cpp` was REWRITTEN once to the final state; new sibling `BaySickPitchSubEditor.h/.cpp` (popup) + CMake entry.
- **Task 4 — gestures / snap / menus.** Motion model (P1-14 + 14c): body drag = 2-axis (vertical 0.1 st, Ctrl 0.01 fine, Snap ON lands on in-scale lanes; horizontal = elastic dst-span move with collective never-cross clamp — the gap counter-warp falls out of the Task 3 map build); edge drag = stretch/squeeze; Ctrl+horizontal (>3 px) = detach (free placement, amber outline). OLD EDGE-TRIM RETIRED (edges now stretch; re-partition = Slice + merge; Alt+edge-drag reserved — flagged for Jeff at the smoke per plan). Root/Scale/Snap toolbar on the 13-scale shared table via new `PitchCorrectorDSP::scaleName/scaleMask/snapMidiToScaleStatic` statics (single source with the realtime board); new `bsp_root/bsp_scale/bsp_snap` params feed the applicator's snap target; Focus target = nearest semitone (Snap OFF) or the scale (ON), computed identically in the applicator AND the pill renderer, so pills draw at corrected pitch while the green F0 curve stays as-sung (lock 8). In-block pitch readout inside pills at sufficient zoom; double-click pill = open the sub-editor popup. Preset combo retired (7a): `bsp_preset`/`bsp_preset_dirty` gone (old saves unaffected); Save/Load user presets kept; dirty-dot/mirror machinery deleted. Pill right-click menu (18a): Restore to Original State (15b full reset) / Snap to Semitone / Merge With Next / Merge Selected / Open Sub-Editor / Zoom to Selection; merge keeps the LONGEST member's pitch identity + edits and absorbs src+dst spans (calibration-class, mine); Slice mode keeps the 30 ms interior guard, works on slice pills, splits time-edited spans proportionally; slice pills render in a dedicated bottom lane (my design call — no pitch identity to place on a lane). Batch re-pitch = keyboard-key click re-centers the selection on that note; audition-hold previews the focused pill through current edits; scrub-audition re-renders during a drag while stopped (150 ms throttle).
- **Task 5 — view / navigation / playhead.** Ctrl wheel h-zoom + Alt wheel v-zoom (both cursor-anchored; `mLaneH` 4-24 px), Shift h-scroll, bare v-scroll, middle-drag pan, Ctrl+RMB-drag zoom-to-rect + plain-RMB-on-empty restores the saved view, menu Zoom to Selection; view floor at C0 (MIDI 12) in paint loops + keyboard + a dynamic setTopNote lower clamp. Playhead follows the MAIN transport at 30 Hz including stop-reset — new `onTransportBeat` hook chain (StandaloneEditor `mPlayHead.getCurrentBeat()` -> `VoxPage::setTransportBeatProvider` -> engine hook), beat -> TempoMap sample -> composite sec; editor timer 400 ms -> 30 Hz with a ~2 s slow path for badges/length/diag. Auto-scroll = edge-triggered page-flip ONLY while the playhead advances across the right edge during playback. #9 fix: the InfoBar refreshes on undo/redo/reset so the [edited] tag is truthful (hasEdits's 0.01 epsilon absorbs quantize residue).
- **Task 6 — selection / clipboard / GLOBAL undo (9a).** Marquee + Shift-click toggle + Ctrl+A; Ctrl+C/V = edit-state transfer (20a: pitch offset + curves + vib/frm/variation from the FOCUSED pill onto the selection, one undo step); Ctrl+B = the family duplicate analog; Shift+arrows = 0.1 st pitch / 10 ms time nudge (elastic-clamped); Delete = Restore to Original State (pills are analysis segments). New `PitchEditAction` in `UndoActions.h` (full before/after region vectors, first-perform skip, SafePointer apply = safe no-op after tab close); UndoContext plumbed StandaloneEditor -> VoxPage -> BaySickVocalEditor -> pitch editor; toolbar Undo/Redo drive the app-wide stack (Ctrl+Z rides the existing global BSCommands binding); local undo stacks DELETED; every gesture/menu/popup/knob edit wraps in beginEdit/commitEdit.
- **Task 7 — sub-edit system (Jeff's design, verbatim) + on-pill handles (19a single-storage contract).** Display-only box under the selected pill — 4 bars VIB/FRM/VOL/PITCH, EDIT button (opens the popup) + RESET button (boxed params of the selection only; multi-select reset prompts with a never-show-again checkbox persisted app-wide in `Documents/BaySickDAW/ui_prefs.xml`). Popup `BaySickPitchSubEditor`: lane = pill length with the waveform ghosted (post-gain), EventEditor gestures (click add / drag move / right-click delete), Volume/Pitch lane toggle, ordered pill browser (selection-filtered from multi-select entry), own Play button + SPACE while focused (streams through the new processor-side preview player, pre-rack so it is voiced like playback), bipolar Vib/Frm + Variation knobs (one undo step per gesture; typed entries self-wrap). On-pill handles: 4 corner handles on the single-selected pill (hidden below a zoom threshold) — top = volume fade in/out (horizontal = length), bottom = pitch approach/release (2-axis: length + semitone offset), green=in / red=out + hover arrows; handles write ORDINARY points into the SAME volShape/pitchShape the popup edits (span-replace on re-drag = the explicit overwrite, one undo step); pill waveform drawn post-gain so fades show on-canvas; InfoBar live readout during handle drags. DSP: `PitchNoteRegion` gains `pitchShape` (additive semis over t01, "pit") + `variation` ("va", def 1); the applicator applies pitchShape additively and Variation as an F0-track-deviation scaler (the snapshot now carries the analysis F0 track — Variation genuinely flattens/exaggerates the note's own wiggle). FL knob conventions (P1-14) shared as `applyFLKnobFeel` (SharedUI.h) — detent at default (Shift bypasses), Ctrl fine, editable value box — applied to the pitch-editor knobs, popup knobs, and all six align-editor rotaries.
- **Task 8 — offline render parity + high-res (16a).** `applyWarp` Phase 1 ported: the per-anchor-segment assembly (constant ratio per segment = steps at every anchor) replaced by ONE streaming PhaseVocoder whose ratio follows the same Fritsch-Butland/Hermite slope the live decode glides on, hop-quantized like the live rReq/rEff feedback, with per-window produced-length bookkeeping so hop quantization never accumulates as drift (`pvStretchExact` deleted with the old assembly — own-batch dead code). Renders honor time edits: `buildWarpedFollower` composes guide -> align(edited) -> pitch-map(source) via a dense guide set (pitch anchors' dst pulled back through a monotone bisection of the align lookup; common-origin pad converted); `renderPitchedTake` post-warps the pitched bake through the published time map. High-Res render: both Render buttons open Standard / High Resolution (slower) / Cancel; High-Res runs the WARP phase at sampleRate x round(384000/sr) (8x at 44.1/48k) via Lagrange up/down-sampling (8-sample guard pads); the pitch algos stay at device rate (their trackers assume it; the YIN window breaks at 352k+). Memory note: a 3-minute take peaks at several hundred MB transient during a High-Res render (message thread, one-shot) — owner judges at the smoke. Granular/Pv desk-check CONFIRMED not the Psola bug class (GranularShifter reads at `readPos += ratio` with clamps only retiring grains early; PvShifter consumes at `mReadPos += mEffRatio` with priming holding silence without advancing); the render-algo listen stays in the smoke.
- **Task 9 — realtime engage-tick fix (locked design iii).** `PsolaShifter` gains `feedSample` (ring write only — zero synthesis, zero latency) + `resyncToWriteHead`; the corrector's fully-bypassed path now warm-feeds the rings + tracks the period; the engage/disengage edge runs a ~40 ms equal-power crossfade (sin/cos) between dry and corrected taps — the formant-engine engage reset rides the same fade. OFF path adds zero latency (Jeff's requirement); fade step construction-safe; the 40 ms calibration tunes at the realtime first-listen.
- **Task 10 — test plan + docs.** Master Test Plan §B.10 authored (FD-1..FD-20 from the tasks' verify lines) + amendments: F-3/F-4 rewritten to the residual-cap semantics + new controls, F-6 (knob staleness + gate set + carve-out), F-10 (Root wording), FA-2/FA-3/FA-5 superseded markers + §B.7 preamble note, FA-9 (4a carve-out + visible states), FB-3 (case 3 retired with bsv_bypass), FB-11 (Bypass out of the gate set, Key->Root). Main Plan §5 QA-Fd row + §6 arrow/footnote + §9 fifty-sixth Forks entry (G2 composition change); the caribou Carry-Over FINAL block + G2 composition note ride the close commit (2b).
- **Files:** `Source/DSP/BaySickAlignDSP.h/.cpp`, `Source/DSP/BaySickPitchDSP.h/.cpp`, `Source/DSP/PitchCorrectorDSP.h/.cpp`, `Source/DSP/PitchShifters.h`, `Source/BaySickVocal/BaySickVocalProcessor.h/.cpp`, `Source/BaySickVocal/BaySickVocalEditor.h/.cpp`, `Source/BaySickVocal/BaySickAlignEditor.h/.cpp`, `Source/BaySickVocal/BaySickPitchEditor.h/.cpp` (rewritten), `Source/BaySickVocal/BaySickPitchSubEditor.h/.cpp` (new) + CMakeLists.txt, `Source/Vox/VoxPage.h/.cpp`, `Source/PluginProcessor.h/.cpp`, `Source/Engine/Tasks/VoxStripTask.cpp`, `Source/Standalone/UndoActions.h`, `Source/Standalone/StandaloneEditor.cpp`, `Source/Standalone/SharedUI.h` + test plan (§B.10 + amendments) + running notes + Main Plan (§5 row + §6 + §9 Forks) + `Batch Plans/swift-stampeding-caribou.md` (Carry-Over FINAL + G2 note).

#### Found along the way

- **FOUND BUG (pre-existing) — align editor Undo/Redo restored the map into `mAlignState` but never republished:** playback kept the undone map while the UI showed the old one, and an un-gated undo could swap maps mid-play. FIXED in-batch: `restoreEdits` republishes + re-baselines the settings gen, and Undo/Redo join the stop-gate set (buttons grey during play + guards in doUndo/doRedo) — consistent completion of the locked maps-only-change-while-stopped rule.
- **FOUND BUG (pre-existing) — the realtime corrector's pc-space snap walk had an octave-wrap defect:** B snapping "up" to C landed an octave DOWN (`octave + snappedPc` ignores the wrap). FIXED in passing via the shared absolute-walk static (`snapMidiToScaleStatic`); the realtime corrector now delegates to it — one snap implementation for editor + board.
- **Assumption corrected vs the 17b lock text:** the realtime scale list was missing FOUR scales (Mel. Minor, Pentatonic Maj, Pentatonic Min, Blues), not 3 — and the old table's "Custom" slot was DEAD (zero `setCustomScaleNotes` callers, no UI), so it retires without loss. The 8 shared scales' masks verified identical to PianoRoll's kScaleDefs.
- **Found-while-implementing correctness point (Task 1):** the old align code sampled the leader's F0 at the anchor's MAPPED time — under residual-cap that is deliberately NOT the leader's word position, so deltas would have been garbage. Deltas now sample at the RAW pair positions.
- **Load-time false-stale:** attachment sync fires the knob hooks during `setStateInformation`'s replaceState, which would bump the settings gen and false-stale every project open — the load path re-baselines the gen after replaceState.

#### Spec calls

- Locked pre-batch: the full 2026-07-11 docket (items 1-20) + the G2 caribou calls — see the plan's locked table. Headlines: Bypass removed entirely (3a/12b), pitch tab upstream of align (5/13a), slice pills + no-silent-drop (6), preset combo removed (7a), Root/Scale/Snap + Focus-as-snap-strength with visual pull (8, 14a-c), global undo migration (9a), Jeff's sub-edit design verbatim (10, 15a/15b) with the on-pill handle set + single-storage sync contract (19a), residual-cap align semantics (G2 9a/10a), publish-time Blend/Variation + Pitch Types (G2 12a/13a), Flexibility + Max Shift (G2 14a/15a), high-res on the render path (G2 16a), realtime scale reorder accepting the saved-pick shift (17b), pill right-click menu (18a), copy/paste = edit-state transfer (20a), engage edge-crossfade with always-primed input (11/16a), P1-14 parity items 1-14 in / 15-16 out, ONE commit at close (2b).
- Execution correction (Jeff, session start): no per-task build stops — code the entire plan, then one build check + the smoke script.
- ZERO new owner spec calls surfaced mid-batch (bulk-run ask-always lock never triggered); the calibration-class calls the locked table delegated to me were made + logged in the running notes (kMatchWindowSec 0.4 s, END-anchor slope-1 offset-continue, Pitch Types Hz bands, merge keeps the longest member's identity, slice-pill bottom lane, 0.25 s decode cut threshold, 30 ms badge hop, 40 ms engage fade).
- Deferred by design (not routings): all calibrations tune at the boundary re-listen (match window, residual mode values, Flexibility bounds, Hz bands, Max Shift range/default, engage fade, glide-merge thresholds, snap-pull display curve); NIT-4 (sync re-analyze UI hitch) = Jeff's deliberate judgment at the realtime first-listen; edge-trim retirement flagged for Jeff at the smoke (Alt+edge-drag reserved if trim is missed).

#### Routed (Rule 3)

- Nothing routed out — the batch's two found bugs (align undo republish gap, corrector octave-wrap snap) were pulled in-batch per the QA fix-don't-defer default; the assumption corrections were absorbed into their tasks.
- The §9 Forks entry (G2 composition change: QA-Fd inserted as the fifth G2 batch; the boundary closes after ITS smoke completion) rides the close commit per the plan. QA-ApvtsAutomation (automation writes vs record-locked params) stays parked at its G4 slot (pre-existing routing, caribou).
- [PITCH DIAG] strip is sequenced AFTER the boundary smoke passes (rides the smoke-fix commit or a follow-up, Jeff's call at that moment).

#### Diagnostic Instrumentation Catalog

- No NEW instrumentation added in QA-Fd (Rule 4). The pre-existing temp [PITCH DIAG] applicator tracer (10 sites, added at the G2 boundary session `703f06e4`, cataloged in the caribou notes) was carried through the pitch-editor rewrite (the editor timer keeps a ~2 s slow path for the diag readout) and strips AFTER the boundary smoke passes per the plan + catalog.

#### /review-batch outcome

- Code-complete review (pre-build): 0 BLOCKER / 6 NEEDS-FIX / 7 NIT, every premise source-verified. All 6 NEEDS-FIX fixed in-tree before the build handoff (clearAllEdits + edit-carry cover the QA-Fd edit classes; hasEdits ignores neutral shape residue — the missed #9 plan line; load-time Blend stomp closed via a restoring-state flag; warpContinuous ring/ratio hardening + segmented rendering makes detach-reordering render correctly; mergeSelection bounds-guarded) plus 5 cheap NITs (popup Play stop-gate, shared scale-name table, formant engage reset, analyze in-flight guard, time-map tail pin); 2 benign NITs deferred (zero-move handle undo entry; neutral leftover points visible in the popup). Full detail in the running notes' review-outcome entry.
```
