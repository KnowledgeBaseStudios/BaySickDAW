# QA-G3Smoke — G3 Boundary Smoke Defect Sweep (all 37 defects) + Voiced SlideSampler + Swing — Plan (burly-restringing-bison)

> **Canonical path:** `Plans & Specs/Batch Plans/burly-restringing-bison.md`.
> **For execution:** the SINGLE implementation plan for every defect in
> `Plans & Specs/G3 Smoke - Master Defect Dossier.md` plus the full guitar/bass slide rework and the
> net-new Swing feature. Compiled 2026-07-23 after a full review session: every load-bearing dossier
> line number was re-verified against the working tree (four verification agents + a direct read of
> the slide stack and the karoryfer library on disk), and every spec call below was answered by Jeff
> in chat. **Every decision in the locked table is answered — do NOT re-litigate or re-ask.**
> Jeff's directive: ONE plan, tasks ordered by surface so no region is written twice.

## Context

The G3 boundary smoke (2026-07-22) surfaced 37 defects across seven clusters (slide/bend, piano
roll, FL tools, tracks, builder grid, transport/playhead, drums, engine) plus the finding that the
guitar/bass slide needs its full patch voicing reproduced. The dossier is the compiled input; this
plan is the corrected, verified, ordered execution of all of it.

**Risk:** high — a scheduler-core rewrite, a new data-model domain (8A), and a multi-week DSP build
(the voiced SlideSampler). **Effort:** honest ~4-6 weeks (slide rework ~2-4 weeks; everything else
~1.5-2.5 weeks). **Dependencies / tree state:** ALL of this stacks on the uncommitted tree at HEAD
`d6abc38b` (QA-SlideSliceGlide + QA-SlideSampler + QA-L-Fix + the G3 review fixes + QA-OctavePedal
all ride there uncommitted). Certify the tree at session open exactly like silky-gliding-lynx did;
**do not disturb the prior batches' hunks.** Commit points are Jeff's call (surface the Rule-9
one-liner + FULL git status at close, or earlier if he asks).

**Hard sequencing constraints (dossier §11) and how this plan satisfies them:**
1. #24 (remove loop) + #25 (drop length) ship together → Tasks 2+4, same plan; the interim window
   between them is fine because nothing is user-verified until the smoke (bulk-run).
2. #27 (builder snap) lands with 8A (playhead domain) → 8A data-side lands FIRST (Task 2), #27 in
   Task 4 — the bar-index-vs-map-position mismatch is never user-visible at any point.
3. sfizz pitch-wheel fix is a slide prerequisite → Task 1 precedes Tasks 10-12.
4. #32/#33/#34 are one unit → Task 8.
5. #30b dropped-notes fix is non-negotiable → Task 2, early.

## Dossier corrections (verified this session — the plan below builds on THESE, not the dossier text)

- **#17:** "every Riff step-4-7 default is a no-op" is FALSE — Mirror (step 4) defaults Flip chance
  30 and its math runs when enabled. Only Levels/Artic/Groove defaults are inert.
- **#28:** the resize half is WITHDRAWN — Builder resize already keeps sub-bar precision
  (`setLengthBeats`, BuilderPage.cpp:5868) and honors Alt (`snapBar` at :5859). Only the MOVE path
  truncates. `:5482/:5506` feed automation-point rescaling only.
- **Bass filter:** "always-on 250 Hz lowpass REQUIRED even at default controls" is overstated — the
  program ships `set_cc90=127` and `var02_cutoff=9000` cents through curve 0 (linear), so the LPF is
  effectively OPEN at default. It matters when cc90 is turned down (+ velocity/fileg/wobble terms).
  Do not chase default-timbre problems with the filter; the default gap is articulation + gain/
  velcurve + envelope.
- **Guitar cc116 = vibrato DELAY** (`lfo0x_delay_oncc116`, label "Vibrato delay"), not Fade. Bass
  has BOTH cc115 Delay and cc116 Fade.
- **#32 is worse than written:** recorded drum notes are SILENT on playback (scheduler reads
  `drumRolls[]` only) and the D1.1 rescue migration runs only when `drumRolls` is entirely empty
  (PatternManager.cpp:1882 guard) — in any pattern with existing drum hits the recording is
  permanently lost, not "invisible until reload."
- **Idle-suspend:** `kIdleSuspendBlocks = 9` (~100-200 ms; PluginProcessor.h:1081) — with the anchor
  suppressed and no other MIDI, a slide can be truncated MID-GESTURE, not just during ring-out.
- **#30b:** the Rusty singleton is a third category — no lock at all (song :2618, pattern :2680),
  like layers/bass. The snapshot covers ALL roll families uniformly.
- **#35:** the artifact is full-depth AM (the de-anchored window pair's sum sweeps ~0→2.0, nulls
  included), not a static +6 dB; the down voices also fall back to the granular path when the
  PeriodDoubler disengages (chords/unvoiced), so the OLA fix covers them too.
- **#37:** whitelist-add alone is insufficient — CC86 currently falls through to VibePlayer's
  DEFERRED path which lands after CC85 armed+cleared the ramp; CC86 must be handled in the same
  INLINE pre-pass as CC84/85.
- **#26:** the "must also copy block velocity" note in the dossier was wrong (Jeff, 2026-07-23):
  #26 is copy-what-a-block-is (pattern/clip identity, length, content offset). Velocity is #12,
  piano roll only. `ArrangementBlock` has no velocity field.
- **Unison memory:** t1/t2 unison layers reference NEIGHBORING keys of the same sample pool
  (guitar t1 at key 40 = the key-41 wav with `pitch_keycenter=41`, resampled down; bass adds
  `fil2_type=hpf_1p cutoff2=250`) — full unison costs ~zero extra decoded RAM (path-keyed cache
  dedupes). Extraction captures t1/t2 as their own zone tables (their per-note cc113 depths differ).
- Trivial line drifts folded into Files-to-modify below (pattern-mode locks `:2640-2678`,
  `startTicks` clear `:5923`, drag origins `:5523/:5609`, `UndoActions.h:136`, Builder pixel
  truncation lives in `barToX` `:1632`).

## Spec calls already locked (dossier §13 + this session's chat — do NOT re-ask)

| ID | Decision | Reasoning |
|----|----------|-----------|
| G-1 | Slide architecture = voiced SlideSampler reproducing the FULL patch voicing + FULL control surface; full fidelity incl. bass cc105 Mono + cc117 Humanize wobble; ENTIRE dossier in scope (dossier §13.1-3). | sfizz can't hop samples or crossfade voices; formant correctness is a hard requirement. |
| G-2 | Pitch offset #1 = the clean pitch-wheel convention bug, guitar +3 / bass +2, no residual (dossier §13.4). | Jeff re-tested both; verification confirmed the single dominant cause. |
| G-3 | Humanize: seed dropdown 1-10 default 1 no "None"; distribution Uniform/Triangular/Quasi-Normal default Quasi-Normal; interval 1/32-1/64-1/128 default 1/64 standalone list; defaults Start 10% / Duration 10% / Velocity 20% / offsets 0; KEEP our column headers + percent display (dossier §13.5-9). | FL reference screenshots; beginner-friendly. |
| G-4 | Pan ramp #11: RP/RT only, pan-only, in-house engines only (BaySickSynth/Bass, Harmless, VibePlayer); withdrawn for Guitars/Basses (dossier §13.10). | No reachable pan control on the sfizz engines. |
| G-5 | 8A: block start stored as absolute beats, bar index derived for display (dossier §13.11). | Kills the bar-index-vs-map-position latent mismatch. |
| G-6 | 10B: lock-free roll snapshot — audio thread never waits, never discards (dossier §13.12; NON-NEGOTIABLE). | Jeff's word. |
| G-7 | Swing #18: global + per-instrument, applied at scheduling (dossier §13.13); concrete spec = Jeff 2026-07-23, see SW-1..SW-6 below. | Net-new feature, fully specified. |
| G-8 | Ghost notes #29: add Inst + Clips + Rusty, NOT Vox (dossier §13.14). | No vox MIDI. |
| G-9 | Roll playhead residual #30: add a DIAGNOSTIC reading, not a fix (dossier §13.15); fix routes after Jeff characterizes it in Debug. | Residual unexplained; don't guess. |
| G-10 | Slide markers #9/#10: right-edge arrow, white border, black fill, Porta + Bend (dossier §13.16). | Matches RP/RT markers. |
| G-11 | Articulation residency = decode ALL articulations of the loaded program synchronously at load (Jeff 2026-07-23, option A). | Zero keyswitch latency ever; load block grows; RAM ~2-3x per patch (measure + log the real number at build). Consistent with the lynx rejection of lazy decode. |
| G-12 | Old-slide-tail policy rides a NEW Cut Self control on Guitars/Basses: Cut Self ON → new gesture kills the old tail with a 5-10 ms anti-click ramp; OFF → old tail choked through the patch `ampeg_release` (~0.25 s). Option (a) "tail keeps ringing" is DROPPED (explicit removal). | Jeff 2026-07-23. A re-slide is a re-fretted string. |
| G-13 | Cut Self scope on Guitars/Basses = FULL parity with the in-house QA-CutSelfReview pattern: CUT SELF toggle + Same Pitch / Cut All mode, cutting normal sfizz notes too, with the slide-tail wiring on top (Jeff 2026-07-23, option b). | A visible CUT SELF must behave like every other engine's. |
| G-14 | Cut Self UI home = the player title bar (Jeff 2026-07-23, option a) — the bar this plan already normalizes in Task 6. | One layout pass. |
| G-15 | #26 = option A: builder click-copy carries pattern/clip identity + length + content offset. No velocity (none exists on blocks). | Jeff 2026-07-23; dossier note corrected. |
| G-16 | Title-bar normalization (Jeff 2026-07-23): Guitars/Basses Load-program button + program label move from the page tab bar extras-right DOWN to the player title bar; NAM A/B slot buttons move off the title bar to below the oversampling toggle; Rusty Player Preset button + program dropdown move onto the BaySickRustyDrums title bar. | Uniform "title bar with preset dropdown" pattern so the swing knob has one home. |

**Swing spec (SW — Jeff verbatim 2026-07-23 + baked semantics he reviewed):**

| ID | Decision |
|----|----------|
| SW-1 | Global Swing knob on the transport bar BETWEEN the computer-keyboard button and the pattern dropdown. 0-100%, default 0. At 100%, every second step is pushed forward by half a step. |
| SW-2 | Step = a 16th note (24 ticks at 96 PPQ). A note starting off-grid (humanized) swings with the step it starts inside (floor), so humanized patterns stay coherent. |
| SW-3 | Per-player Swing Mix knob on each player title bar, LEFT of the preset dropdown. 0-100%, default 100% (0% = ignores global; 50% = half; 100% = full). |
| SW-4 | Effective delay = global x mix x (half a 16th), applied at scheduling time to note start times, all scheduled MIDI rolls. |
| SW-5 | Truncate Swing Notes = right-click toggle on the Swing Mix knob, default off. When on, a swung note's end is clipped so it cannot overlap the next SAME-PITCH note's (post-swing) start on the same roll (same-pitch scope — truncating cross-pitch overlap would mangle chords). |
| SW-6 | Global + per-player mix + truncate persist with the project (APVTS params, eager-registered). |

## Sub-spec calls surfaced for ExitPlanMode

**No sub-spec calls open.** Everything is locked above. Two items are deliberately DEFERRED, not
open: (1) the roll-playhead residual FIX — unscheduled until Jeff runs the Task-1 diagnostic in
Debug and reports the reading (G-9); route per Rule 3 when characterized. (2) The slide landing
thinness A/B + the SS-Q5 tuning values — ear checks at the bulk-run smoke (carried from
silky-gliding-lynx; grep `SS-Q5 TUNE`). If execution surfaces a genuinely NEW spec call, ASK Jeff in
chat (numbered options, no recommendations) and WAIT — never code past a call on a guess.

## Files to modify (by task; line refs verified 2026-07-23 against the uncommitted tree)

- **Task 1:** `Source/BaySickGuitars/BaySickGuitarsProcessor.cpp` (:321/:326/:332/:355-356),
  `Source/BaySickBasses/BaySickBassesProcessor.cpp` (:317/:322/:328/:352),
  `Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp` (:220);
  `Source/Standalone/PianoRollPage.cpp` (+ `PianoRoll.cpp` paint if needed) — the #30 diagnostic.
- **Task 2:** `Source/PluginProcessor.cpp` (locks song :2579-2614 / pattern :2640-2678; tiling
  :2538-2568 nTiles :2541; `rampChainDurationBeats` :139-160; `findGlideSourcePitch` :111-132;
  mono-cut :2455-2457/:2476-2478; `emitNoteExpression` :54-76; `emitRampSlide` :201-220; new swing
  transform + `ensureSwingParams`), `Source/PluginProcessor.h`, `Source/PatternManager.h/.cpp`
  (ArrangementBlock beats-authoritative + serialization).
- **Task 3:** `Source/BaySickSynth/BaySickSynthVoice.h/.cpp` (takeover :256-257; ramp precedent
  :279-284/:309-313), `Source/Harmless/AdditiveVoice.h/.cpp` (:313-319),
  `Source/VibePlayer/VibePlayerDSP.h/.cpp` (whitelist :1455-1456; ramp .h:354-366; wheel no-op .h:308).
- **Task 4:** `Source/Standalone/BuilderPage.cpp` (menu :6850-6993; move :6885-6920 swap :6888 group
  ids :6907-6908; color :6956-6969; group items :6932-6955; undo :3118/:3138-3154/:3180 +
  `UndoActions.h:136`; `insertBlankRowsAt` :3520-3550 model; preview :2335/:2355/:2365/:2371-2386;
  rolls :2393-2396; slice mod :5675-5676; drops :5018/:4964/:5553→:6302/:5576/:5948; idiom
  :6045-6046; click-copy consumer :5545-5556→:6332-6341, block-hit :5460-5540/:5586-5629,
  `BuilderPage.h:775-778`; `snapBarAlt` :1679-1708; cast :5915-5916; `startTicks` :5923; origins
  :5523/:5609; alt :5101/:1675; `barToX` :1632 / `xToBar` :1637), `Source/Standalone/KeyBindings.cpp`
  (2 doc rows), `Source/Standalone/PianoRoll.cpp:2100-2113` (delta-snap reference, read-only).
- **Task 5:** `Source/Standalone/PianoRollPage.cpp` (:81-82), `Source/Standalone/BuilderPage.cpp`
  (:7476/:7480 latency reference, read-only).
- **Task 6:** `Source/Standalone/GlobalTransportBar.h/.cpp`; the engine editor title bars
  (`HarmlessEditor`, `BaySickSynthEditor`, `BaySickBassEditor`, `VibePlayerEditor`);
  `Source/BaySickNAMIR/BaySickNAMIREditor.cpp` (title bar + slot buttons ~:35-41/:92-117;
  oversampling toggle); `Source/Standalone/BaySickRustyDrumsPage.cpp` (title bar ~:165; preset btn
  :286-292); `Source/Standalone/StandaloneEditor.cpp` (:5437-5443 extras-right removal);
  `Source/PluginProcessor.cpp` (params from Task 2).
- **Task 7:** `Source/Standalone/PianoRoll.cpp` (click memory :1855-1856/:1978-1979 placement :2287;
  markers :2664-2695 orange :2693 name :2660; label :3964-3973; S-cycle :658-664; humanize
  :4085-4344 — interval :4150-4155/:4282, combo :4146, hardcode :4290-4292 consumers :4300-4302,
  seed :4157-4163/:4280/:4190-4193, defaults :4252; riff — enables :5249, gate :5622-5624, math
  :5740-5848, dice :5439-5444, randomize :5579-5597, start-over :5427-5438, checkbox vis
  :5545/:5474-5475, lambdas :5274-5287/:5288-5298, `mWorkExisting` :5405-5408/:5435/:5633, accept
  :5891-5906, tool :5935-5951), `Source/PatternManager.h/.cpp` (`PianoRollData` field;
  `rollToValueTree` :1108-1114).
- **Task 8:** `Source/Standalone/StandaloneEditor.cpp` (:12811 target; :4334-4335 `mLastRollIndex`;
  commit ends :12875-12878), `Source/PluginProcessor.cpp` (:2867 allMidi merge;
  `dispatchDrumTriggers` :5496-5560, binding match :5531-5538), `Source/PatternManager.cpp`
  (:1862-1870/:1884-1891 migration, :1882 guard), `Source/Standalone/DrumKitGrid.cpp`
  (:1759-1793 drag; `playNoteForPage` :645 / `DrumKitGrid.h:152`).
- **Task 9:** `Source/DSP/OctaveStyleDSP.h` (:109) `.cpp` (:29/:101-102/:289/:479/:486-491/
  :546-547/:560-576/:688).
- **Tasks 10-12:** `Source/SlideSampler/SlideRegionMap.h/.cpp` (filter :127-135; capture :163-180),
  `SlideSampler.h` (Zone :59-64; knobs :109-110) `.cpp` (steal :121-127; gain :185; release
  :223-230; trigger :129-163; render :232-286), `SlideSampleCache.h/.cpp`,
  `Source/BaySickGuitars/BaySickGuitarsProcessor.h/.cpp` (armSlide :159-185 continuation :169-174
  suppress :176; interception :283-297; noteOff :311-318; all-notes-off :328-333; outVol :417-418;
  loadKit :434-573) + `BaySickBassesProcessor` twins, `Source/Engine/Tasks/InstStripTask.cpp`
  (:161-199), `Source/PluginProcessor.cpp` (emit side only if needed), `CMakeLists.txt` (any new
  files).
- **Task 13:** docs only.

---

## Tasks

### Task 1 — sfizz pitch-wheel convention fix + roll-playhead diagnostic

Every wheel send passes JUCE raw 0..16383 where sfizz wants centered −8192..+8192
(`sfizz.hpp:550`; `normalizeBend` clamps ±8191 — raw 8192 ⇒ +1.0 = FULL BEND UP). Neutral notes ride
sharp by the patch bend range (guitar +3, bass +2); Bend ramps are pinned. Fixes #1 + #8; §11.3
prerequisite for Tasks 10-12.

- [ ] Certify the working tree first (map every dirty/untracked entry to its prior batch; disturb
      nothing) — log the certification in running notes.
- [ ] Convert ALL wheel sends in all three sfizz processors to centered values:
```cpp
// raw JUCE 0..16383 -> sfizz centered; neutral 8192 -> 0
mSfizz->pitchWheel (delay, juce::jlimit (-8191, 8191, msg.getPitchWheelValue() - 8192));
// recenter sites: pitchWheel (delay, 8192)  ->  pitchWheel (delay, 0)
// bend ramp:      8192 + lround(w*target)   ->  jlimit (-8191, 8191, (int) lround (w * mBendTargetWheel))
```
      Guitars :321/:326/:332/:355-356 · Basses :317/:322/:328/:352 · Rusty :220.
- [ ] #30 diagnostic (G-9, Debug-only): a `[G3 PLAYHEAD]` readout logging, on every roll click and
      per playhead paint tick: click x → computed beat, playhead beat, `getTotalOutputLatency()`
      samples, sample rate, snap div. Rule 4 catalog row in running notes (`Remove at batch close`
      unless the residual investigation still needs it). Tell Jeff at the build gate: run the DEBUG
      exe, click roll positions against a playing pattern, report the log lines — the reading routes
      the residual fix per Rule 3.
- [ ] Build gate (Jeff runs `do_build.bat`, Debug + Release). Fix until clean.
- [ ] `/draft-doc running-notes` → append.

### Task 2 — scheduler core rework (one pass over the scheduling path)

One coherent rewrite so the scheduler is touched once: #30b, #24 audio half, 8A, #36, swing
transform, #11 emit half.

- [ ] **#30b (G-6):** replace the scheduler's roll access with a lock-free snapshot for ALL roll
      families (layers/bass/drums/clips/vox/inst/rusty — the try-locks at song :2579-2614 / pattern
      :2640-2678 discard notes on contention; layers/bass/rusty are bare unsynchronized reads).
      Shape: copy-on-write per-roll snapshot published by the message thread on every roll edit via
      an atomic two-slot pointer swap (no `std::atomic<shared_ptr>` — verify the chosen primitive is
      actually lock-free); audio thread acquires the snapshot pointer per block, never blocks, never
      misses. Delete the try-locks.
- [ ] **#24 audio half:** remove the tiling loop (:2538-2568, `nTiles` :2541) — schedule ONE pass
      from the content offset; block length past available content is silent.
      `getPatternContentBeats` stays as "content length past which it's silent."
- [ ] **8A (G-5):** `ArrangementBlock` start becomes beats-authoritative (`startBeats` double, the
      existing `effectiveStartBeats`/`setStartBeats`/`startTicks` machinery collapses onto it);
      `startBar` derived for display only. XML keeps writing/reading `startBar`+`startTicks` (they
      already carry full precision — no migration, pre-v1). Scheduler + ruler + playhead all read
      the musical map-position domain.
- [ ] **#36:** `rampChainDurationBeats` (:139-160) gains a lineage predicate — only absorb a
      RampSlide whose own `findRampAnchorPitch` resolves to this source note; butt-joined
      other-pitch chains no longer defer the base note-off. `findGlideSourcePitch` (:111-132) gains
      the missing end-time check (an already-ended note can't be a glide source). Mono-cut
      (:2455-2457/:2476-2478) only cuts a source that is actually sounding INTO the slide's start.
- [ ] **Swing transform (SW-2/SW-4/SW-5):** at scheduling, per note:
```cpp
const double step = 0.25;                                   // 16th in beats
const int    idx  = (int) std::floor (start / step);
if ((idx & 1) != 0)
    start += gSwing * playerMix * (step * 0.5);             // push the off-beat 16th
// SW-5 truncate (per-player toggle): clip end to the next same-pitch note's post-swing start
```
      Applied to every scheduled MIDI roll; reads the Task-2-registered params (below).
- [ ] **Swing params (SW-6):** `ensureSwingParams()` bulk-registers at startup (mirror
      `ensureMixerBusAndMasterParams`): `globalSwing` Float 0..1 def 0; per player family+index
      `swing_{layer|bass|drum|inst}_{i}_mix` Float 0..1 def 1.0 + `_trunc` Bool def false;
      `swing_rusty_mix`/`_trunc`. If clip pages turn out to have a scheduled roll but no standard
      title bar, surface to Jeff before inventing a home (routing note).
- [ ] **#11 emit half (G-4):** RP/RT slide notes emit **CC89 = pan target** (verified unused) in
      place of the instant CC10 inside `emitNoteExpression`'s slide branch, so pan — and only pan —
      ramps over the glide; plain notes keep the existing channel-live CC10 model unchanged.
- [ ] Build gate → `/draft-doc running-notes`.

### Task 3 — in-house voice fixes (BaySickSynthVoice covers Bass · AdditiveVoice · VibePlayerDSP)

- [ ] **#11 consumer:** CC89 handler stores the pan-ramp target; the CC85 takeover (RP) / glide
      noteOn (RT) arms a pan ramp current→target over `glideSamples`, modeled on the S-6(C)
      velocity ramp (:279-284/:309-313, AdditiveVoice :313-319, VibePlayerDSP.h:354-366). At
      `startNote`, the voice SNAPSHOTS the channel pan into its per-voice pan (no blunt reset —
      CC10 legitimately precedes noteOn in emit order; the snapshot kills stale-pan inheritance on
      voices that never saw a CC10).
- [ ] **#37:** VibePlayer handles CC86 (and CC89) in the same INLINE pre-pass as CC84/85 — the
      whitelist (:1455-1456) alone is insufficient; the deferred path lands after CC85 cleared the
      ramp state. The S-6(C) loudness ramp comes alive in BaySickPlayer for the first time.
- [ ] **#36 voice half:** first-match guard on the CC85 takeover (:256-257) so only ONE voice
      (the true anchor) retargets when two voices hold the same note number.
- [ ] Build gate → `/draft-doc running-notes`.

### Task 4 — Builder grid + tracks (BuilderPage written once)

- [ ] **#24 preview + #29 in one pass** (both live in `drawMidiShading`): de-tile the ghost preview
      (:2335/:2355/:2365/:2371-2386 — single pass, tail blank) AND add the three missing rolls
      (`instRoll`, `clipRoll`, `baySickRustyDrumsRoll`; NOT vox — G-8) alongside :2393-2396.
      Drop the `% cycleTicks` wrap in `sliceOneBlock` (:5675-5676).
- [ ] **#25 (§11.1):** the four drop/draw/paint sites (:5018/:4964/:5553→:6302/:5576/:5948) size new
      blocks from `getPatternContentBeats(idx)` via the `setLengthBeats`/`lengthBars` idiom
      (:6045-6046) instead of `pat.bars`/1.
- [ ] **#26 (G-15):** the two block-hit branches (:5460-5540/:5586-5629) prime
      `mBrowserSelection`/`mDropKind` + a new `mClickMemoryLenBeats`/content-offset from the clicked
      block, so empty-space click places a copy — pattern/clip identity + length + content offset,
      nothing else.
- [ ] **#27 (§11.2, on Task-2's 8A):** move handler writes fractional — seed drag origins from
      `effectiveStartBeats` (:5523/:5609), snap the DELTA not the absolute (PianoRoll.cpp:2100-2113
      pattern), write `setStartBeats` + derived `startBar`, stop clearing `startTicks` (:5923),
      delete the `(int)` casts (:5915-5916). `snapBarAlt` (:1679-1708) already returns fractional.
- [ ] **#28 (move path only — resize withdrawn):** the same de-truncation makes Alt+drag fine-move
      real (`mAltSnapActive` :5101/:1675). Add the two missing keybind doc rows (Builder + roll:
      "Alt+Drag = bypass snap / fine PPQ positioning").
- [ ] **#30 builder half:** pixel-center — `barToX` (:1632) gets rounding, `xToBar` (:1637) gets
      `+0.5`, mirroring the roll's LDT-394 fix (PianoRoll.cpp:508-512).
- [ ] **#21+#22 (one fix):** `groupSpan(row, &first, &last)` helper; Move Up/Down (:6885-6920)
      rotates the WHOLE contiguous span past the neighbor's full span (model:
      `insertBlankRowsAt` :3520-3550); group ids travel with the span — delete the id swap
      (:6907-6908).
- [ ] **#23:** wrap "Color Group…" (:6956-6969), Group-with-Above (:6932-6948), and
      Remove-from-Group (:6949-6955) in the existing `beginEdit`/`commitEdit` bracket (colors
      already snapshot — :3118/:3138-3154/`UndoActions.h:136`/:3180); capture the before-state
      before the async color picker opens.
- [ ] Build gate → `/draft-doc running-notes`.

### Task 5 — playhead behavior (PianoRollPage)

- [ ] **#30 roll latency:** subtract `getTotalOutputLatency` in the roll playhead read
      (PianoRollPage.cpp:82), mirroring Builder (:7476/:7480).
- [ ] **#31:** replace the unconditional song-mode hide (:81-82): in song mode, find a block with
      `patternIndex == viewed` whose span contains the playhead beat; pass the pattern-local beat
      (`songBeat - startBeats + ticksToBeats(contentOffset)` — no modulo, tiling is gone); `-1`
      only when none. Pattern mode unchanged (roll only). Builder side already correct (:7466-7469).
- [ ] Build gate → `/draft-doc running-notes`.

### Task 6 — swing UI + title-bar normalization (G-16, SW-1, SW-3, G-14)

- [ ] **Global knob (SW-1):** on `GlobalTransportBar` between the computer-keyboard button and the
      pattern dropdown; bound to `globalSwing`; double-click → 0.
- [ ] **Guitars/Basses title bar (G-16):** move the Load-program button + program label off the page
      tab bar extras-right (StandaloneEditor.cpp:5437-5443) onto the player title bar; relocate the
      NAM A/B slot buttons from the NAMIR title bar (~:92-117) into the NAM body below the
      oversampling toggle.
- [ ] **Rusty title bar (G-16):** move the Player Preset button (:286-292) + program dropdown onto
      the "BaySickRustyDrums" title bar (~:165).
- [ ] **Swing Mix knobs (SW-3):** on every player title bar LEFT of the preset dropdown —
      Harmless / BaySickSynth / BaySickBass / VibePlayer editors (covers Layers, Bass, and per-drum
      Player sub-tabs, each bound to its own `swing_*_mix`), the normalized Guitars/Basses bar, the
      normalized Rusty bar. Right-click → PopupMenu with the "Truncate Swing Notes" checkbox
      (SW-5, `_trunc`). Double-click → 100%.
- [ ] **Cut Self placeholder space (G-14):** while laying out the Guitars/Basses title bar, reserve
      the CUT SELF + mode toggle slots (controls land in Task 12; lay the bar out once).
- [ ] Build gate → `/draft-doc running-notes`.

### Task 7 — piano-roll tools (PianoRoll.cpp regions + one PatternManager field)

- [ ] **#12:** `mClickMemoryVel` captured at both memory sites (:1855-1856/:1978-1979), used at
      placement (:2287) in place of the hard-coded 0.8.
- [ ] **#9/#10 markers (G-10):** restructure :2664-2695 into one right-edge arrow block gated on
      `type != Standard` with per-type fill — Porta + Bend get white-border/black-fill; the orange
      left arc (:2693) dies. Add the `Bend` case to `refreshNoteTypeButton` (:3964-3973) and to the
      S-key cycle (:658-664).
- [ ] **#13 (G-3):** interval combo = standalone {1/32, 1/64, 1/128} beats list, default 1/64
      (replaces the app snap-table walk :4150-4155; bypasses `snapDivToTicks` :4282).
- [ ] **#14 (G-3):** distribution combo gets Uniform / Triangular / Quasi-Normal (default QN) + an
      `onChange`; 3-way switch in the `qn` lambda (:4290-4292; Triangular `(a+b)*0.5`, Uniform
      `rng.nextFloat()`); all three consumers (:4300-4302) already funnel through it.
- [ ] **#15 (G-3):** seed = dropdown 1-10 default 1, no "None" (replaces the 0-99999 IncDec
      :4157-4163); Regenerate (:4190-4193) rolls 1-10.
- [ ] **#16 (G-3):** :4252 → `kDefStartRange=10, kDefDurRange=10, kDefVelRange=20`, offsets 0;
      headers + percent display unchanged.
- [ ] **#17:** default ALL step enables on (:5249); Dice (:5439-5444) + `randomizeStep`
      (:5579-5597) + Start-over (:5427-5438) set/reset the enables; give Levels/Artic/Groove
      non-neutral reset defaults (Mirror's 30% already live — corrected premise). Fix the
      one-visible-checkbox layout only if it obstructs (all 8 share bounds :5474-5475 by design —
      the per-step page shows the current step's).
- [ ] **#19:** `setDoubleClickReturnValue(true, init)` inside `addKnob` (:5274-5287) +
      `addIncDec` (:5288-5298) — covers all Riff AND Humanize sliders.
- [ ] **#20:** `bool riffMachineUsed` on `PianoRollData`, set in `accept()` (:5891-5906), read at
      panel construction to pre-check `mWorkExisting` (:5405-5408); serialized in
      `rollToValueTree`/`rollFromValueTree` (:1108-1114+), loader defaults false.
- [ ] Build gate → `/draft-doc running-notes`.

### Task 8 — drums, one unit (§11.4: #32 → #33 → #34)

- [ ] **#32:** recording commit demuxes captured notes through `mDrumTriggers.getBindingRT` (the
      binding maps input note/CC → drum index, :5531-5538) — each matched note lands in
      `pat.drumRolls[thatDrum]` stamped at that drum's play note (`drumPlayNoteRT`), NOT
      `pat.drumRoll` (:12811); unmatched notes fall back to the focused drum (`mLastRollIndex`,
      :4334-4335). `commitRecordingResult` ends with `refreshAllKitViews()` (+ the existing
      `markDirty`). This also closes the silent-playback/permanent-loss hole (scheduler reads
      `drumRolls[]` only; migration guard :1882 never rescues non-empty projects).
- [ ] **#33:** falls out of #32 (recorder writes `drumRolls[]` directly → the descending
      `51 - midiNote` migration :1862-1870/:1884-1891 is never entered for new recordings); gate the
      migration so it is unreachable for recorder-written notes.
- [ ] **#34:** read from the CURRENT row's array — `rowToPageIndex(mMoveRefs[i].row)` at :1762-1765
      (the one-liner) — and re-stamp the moved note via `playNoteForPage(destPage)` at the
      re-insert (:1793), guarded so a hit deliberately re-pitched away from the OLD row's play note
      keeps its pitch (D-6).
- [ ] Build gate → `/draft-doc running-notes`.

### Task 9 — octave-up granular artifact (#35)

- [ ] Normalize the overlap-add at OctaveStyleDSP.cpp:101-102:
```cpp
const float wsum = w1 + w2;
output[i] = (wsum > 1.0e-4f) ? (s1 * w1 + s2 * w2) / wsum : 0.0f;
```
      Kills the full-depth AM from `setGrainSize` (:479) de-anchoring the head pair (unity only
      established in `reset()` :29). Covers the up voice AND the down voices' granular fallback.
      If the normalized path still shows artifacts at the smoke, the backup is re-anchoring heads in
      `setGrainSize` + hysteresis on :479 (dossier's alternate).
- [ ] Build gate → `/draft-doc running-notes`.

### Task 10 — slide rework A: full-voicing extraction + articulation residency (G-1, G-11)

- [ ] Extend `SlideRegionMap`/`extractSlideRegions` to capture per region (scope-inherited):
      `volume`/`amplitude*`, `amp_veltrack`, `amp_velcurve_N` points, full `ampeg_*` incl. `_oncc`/
      `_curvecc` (guitar env.sfz hold 0.3/decay 1.5/sustain 100 + cc70 mods; global attack 0.01 /
      release 0.25; bass common.sfz volume 3 / attack 0.001 + cc24 swell mods / `off_time`),
      all `lfoNN_*` incl. cross-LFO rate mod (`lfo03_freq_lfo01_oncc117`) + extended-CC inputs
      (131 velocity, 135/136 random, 133 note number), `pitcheg_*`, the bass filter block
      (`cutoff`/`fil_keytrack`/`resonance_cc91`/`var01`/`var02` mult kludge + curves /
      `cutoff_cc92` / `fileg_*` / `lfo01_cutoff_oncc113`), custom `<curve>` tables (curves.sfz),
      `offset`/`offset_oncc25` (=882) , `tune`, `group`/`off_by`/`note_polyphony`/`rt_decay`,
      `trigger=release`, `loop_mode` (noise layer), unison opcodes (`pan_oncc101`,
      `amplitude_oncc100`, `tune_cc102`, `fil2_type`/`cutoff2`), `bend_up/down` (existing).
- [ ] Table sets: per ARTICULATION (keyed `sw_last`; programs WITHOUT keyswitches extract their
      single articulation — slides start working on the twang/staccato-only programs instead of
      going silent) x per velocity band x {center, t1, t2 unison} + the extra map sets (tailpiece
      `*_tp` via cc118, feedback pitched + looped noise via cc29, release maps via cc27/cc107).
      t1/t2 are the neighbor-key trick — same sample pool, cache dedupes, ~zero extra RAM.
- [ ] Parser hardening: tokenize multi-opcode lines + opcodes on `<header>` lines (the maps are
      one-per-line today — verified — but the extension reads program/control files too).
- [ ] **G-11 (option A):** `setProgram` decodes ALL articulations synchronously at load (extends the
      existing gate-off load block). Measure + log decoded RAM per patch in running notes (expected
      ~2-3x the ~110/141 MB main-sustain figure; tp/fb/rel sets add on top — log the real number).
- [ ] Keep the Task-1 `[SlideSampler]` DBG summary in step (counts per articulation/band/layer).
- [ ] Build gate → `/draft-doc running-notes`.

### Task 11 — slide rework B: the voiced voice DSP (G-1)

- [ ] Per-voice chain: zone sample → Lagrange resample (ratio = keycenter delta + micro-bend ≤1 semi
      + `tune` + unison detune (guitar ∓7c x cc102) + LFO pitch sum + pitcheg) → gain (volume x
      velcurve/veltrack x modulated AHDSR x tremolo (bass lfo01 volume x cc114) x swell (cc24) x
      crossfade `sin` gain) → bass keytracked LPF (var-modulated, fileg, cc113 wobble, cc91 reso) /
      unison 1-pole HPF 250 → per-voice pan (unison width cc101) → stereo sum.
- [ ] LFO bank per voice: guitar lfo01/02/03/04 + pitcheg, bass lfo01/02/03 — waves, phases
      (cc131/135/136-seeded), delay/fade, cross-LFO rate modulation, per-note cc113 depth tables.
- [ ] Custom curve evaluation (breakpoint tables from `<curve>` defs; linear default).
- [ ] Voice pool 4 → ~16-24 (crossfade pair x unison x tails + headroom); steal = oldest-quietest
      (current lowest-`fade` steal at :121-127 kills the incoming voice — the worst pick).
- [ ] **#6 zone thump:** time-align the incoming hop voice's start offset to the outgoing voice's
      elapsed position (its `readIdx` is the alignment source; one int64 of bookkeeping, zero
      per-sample cost) + raise the offset floor toward ~120-150 ms + shorten the crossfade toward
      ~25-30 ms (SS-Q5 knobs stay grep-able). Zero-crossing snap retained. Backup if the ear still
      catches steps: the per-sample RMS envelope table (~4 KB/sample) from the research.
- [ ] `stopAllNow()` — hard-stop all voices through a 5-10 ms declick ramp (used by #5 and by the
      G-12 cut-self-ON tail path).
- [ ] Carry the captured `offset` opcode into `Zone` (`SlideSampler.h:59-64` currently drops it).
- [ ] Extra-layer playback support: release-triggered zones (note-off spawned, `rt_decay`),
      looped noise (loop_continuous), tailpiece table switch (cc118 hi/lo), feedback layer gain
      curves (cc29).
- [ ] Build gate → `/draft-doc running-notes`.

### Task 12 — slide rework C: engine integration (G-1, G-12, G-13, G-14)

- [ ] **Per-engine CC tables** (NOT shared — the traps: CC112 guitar violin-vibrato depth / bass
      vibrato rate; CC113 guitar tailpiece vibrato / bass filter wobble; CC114 guitar vibrato speed
      / bass tremolo; guitar cc116 = DELAY, bass cc115 delay + cc116 fade). Sampler reads the live
      APVTS `<prefix>cc<N>` atomics per block — the kit panel (AriaControlPanel `param="N"` → CC)
      drives the slide for free.
- [ ] **Keyswitch tracking:** watch `sw_lokey..sw_hikey` noteOns in the interception loop; slide
      plays the SELECTED articulation's tables (#2 timbre half). Gain staging + band pick from the
      real velcurves replaces `jlimit(0.05,1,vel/100)` at :185 (#2 loudness half).
- [ ] **#3:** 128-entry last-noteOn-velocity array per engine; `armSlide` sources the slide voice's
      velocity from `vel[anchor]` (optionally ramping toward CC86 over the glide for option-C
      parity) — a bottom-velocity anchor yields a near-silent slide; "sound from nowhere" dies.
- [ ] **#4:** anchor note-off / chain-end applies the patch `ampeg_release` (guitar 0.25 s) —
      the 2-bar ring dies; `release()` stops being a no-op.
- [ ] **#5:** both all-notes-off paths (:328-333 + Basses twin) call `stopAllNow()`.
- [ ] **#7:** monotonic gesture id on the slide transport (or equivalent state) so a copied adjacent
      slide re-triggers instead of being misread as a continuation at :169-174; the sfizz base
      suppress (:176) fires per gesture.
- [ ] **G-12/G-13/G-14 Cut Self on Guitars/Basses:** new `<prefix>cutSelf` (Bool def false) +
      `<prefix>cutSelfMode` (Bool def false = Same Pitch) params, QA-CutSelfReview parity: on
      noteOn with cutSelf ON → noteOff same-pitch (or ALL sounding, per mode) sfizz notes first,
      and choke matching SlideSampler tails. Slide-tail policy: new gesture with cutSelf ON →
      `stopAllNow` declick on the old tail; OFF → old tail through `ampeg_release`. UI = CUT SELF +
      SAME PITCH/CUT ALL toggle pair on the Task-6 title bar (switchToggle style, mirror
      BaySickBassEditor).
- [ ] **Idle-suspend:** add the slide-active term to the predicate (InstStripTask.cpp:161-199) —
      protects the gesture AND the ring-out (mid-gesture truncation confirmed possible at
      `kIdleSuspendBlocks = 9`).
- [ ] **Landing:** stays in our engine (no sfizz hand-off, no seam). The landing-thinness A/B +
      SS-Q5 value tuning remain smoke-time ear items (G-9-adjacent; grep `SS-Q5 TUNE`).
- [ ] Bass cc105 Mono: choke groups honored (`off_time` 0.2) across center/t1/t2 layer sets.
- [ ] Build gate → `/draft-doc running-notes`.

### Task 13 — close (bulk-run)

- [ ] Author the Master Test Plan §B section: supersede the stale §B.23 slide scenarios where this
      batch changes behavior; new scenarios for every defect # (physically-executable gestures read
      from the real components), the swing feature (SW-1..SW-6), Cut Self, and the SS-Q5 +
      landing-thinness ear checks. PASS/FAIL + `blocks:` per bulk-run R4.
- [ ] Walk the Rule-4 diagnostic catalog (this batch: the Task-1 `[G3 PLAYHEAD]` readout; inherited:
      the lynx `[SlideSampler]` DBG) — surface the strip list to Jeff before stripping.
- [ ] `/review-batch` over this batch's diff (scope: exclude the prior uncommitted batches' hunks).
- [ ] `/draft-doc batch-close` → HELD Implemented Work Log entry in the running notes (applies at
      the campaign pass, R2). Include the dossier-corrections record.
- [ ] Route the roll-playhead residual per Jeff's Task-1 Debug reading (fix in-batch if
      characterized + small; otherwise §9 route — Jeff's call).
- [ ] Surface the Rule-9 commit one-liner + FULL `git status` (every entry + disposition) → commit
      only on Jeff's explicit approval. §5/§9/Forks doc applies queue with the commit per the
      prior-batch convention.

## Verification (end-to-end smoke)

Per bulk-run R4: scenarios author into the Master Test Plan at code-complete (Task 13), NOT run
mid-batch. **Do not ask Jeff to test mid-batch** — build gates only (he runs `do_build.bat`; never
run builds from the session). The one exception is the Task-1 DIAGNOSTIC READING (a Debug log
report, not a verify): Jeff runs the Debug exe once after Task 1 and reports the `[G3 PLAYHEAD]`
lines so the residual can be routed.

## Routing notes (Rule 3 application during execution)

- At close: §9 Forks entries back-referencing the G3 smoke sweep; the dossier-corrections record
  (Mirror default, resize withdrawal, bass-filter default, cc116 naming, #26 restatement, #32
  severity, idle mid-gesture, #30b Rusty, #37 inline placement) goes into the Work Log entry so the
  dossier is never the last word.
- The roll-playhead residual (#30 roll half) is NOT scheduled — diagnostic first (Task 1), Jeff
  characterizes, then route (in-batch fix vs §9) per his call.
- Real bugs found mid-execution: fix in-batch (standing rule); not-yet-started-surface findings fold
  forward; anything else → §9 at close. Any NEW spec call → ask Jeff in chat immediately (numbered
  options, no recommendations) and WAIT.
- Swing on clip pages: if `clipRoll` scheduling is live but the page has no standard title bar,
  surface to Jeff (do not invent a knob home).

## Carry-Forward Reference touch points

- §1-3 (architectural primitives, file index) at Task 2 start (audio-thread scheduler), Task 10
  start (sfizz MIDI path + sample loading), Task 12 start (engine dispatch).
- CLAUDE.md "Source Layout" + the 96-PPQ tick model; `reference_audio_thread_fast_path_bypass` +
  `feedback_apvts_dirty_flag_pattern` memories for every new per-block read this plan adds.
- The silky-gliding-lynx running notes (SlideSampler as-built + SS-Q5 checklist) before Task 10.
- The G3 dossier itself for any defect's full narrative — with the corrections section above
  overriding it where they conflict.
