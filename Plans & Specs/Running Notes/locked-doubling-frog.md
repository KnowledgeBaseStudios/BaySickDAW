# Running Notes — QA-OctavePedal (locked-doubling-frog)

> Append-only mid-batch log. New entry at every checkpoint (commit landed / sub-task
> verified / finding captured / spec call resolved / scope pivot). Consumed at doc-close
> (section pass under bulk-run R2) as the primary input for the held Work Log entry.

Pair file: `Plans & Specs/Batch Plans/locked-doubling-frog.md`
Convention: Main Plan §0 "Batch Plans + Running Notes layout" (exemplar
`federated-bouncing-cupcake.md`).

## 2026-07-17 — Group open (G3)

Seeded at G3 group approval. Scout diagnosis in the plan (free-running grain phase = the
bell; mid-conf dual-engine comb; fixed 64-sample seam; integer periods; lag-guard snaps).
Locked: #12=a real poly tracking; #13=a-modified two-mode Dry/With-Effect inst monitor
(flagged default = With Effect, Jeff may veto); Jeff's folded Rule-3 PDC item (pull model)
+ scout companion (octave pedal reports its own internal delay); back-ref Main Plan §9
fifty-ninth. Coding starts after QA-N (last batch of the group).

## 2026-07-18 — Task 1 — Pitch-synchronous octave-down (code-complete, at build gate)

**FINDING (major, beyond the scout diagnosis):** the shipped PeriodDoubler was an
IDENTITY on stationary tones — "play period A, replay A, skip" reconstructs the input
pitch exactly when adjacent periods match (replaying A == playing B when B ~= A). The
dev-side A/B proved it: old engine output F0 == input F0 in every section (82.4 in ->
82.4 out where 41.2 expected). The shipped -1/-2 voices never actually shifted confident
single notes; all "octave" content came from period-to-period differences + seam
discontinuities — which IS the broken bell, at its root. Scout's seam-phase diagnosis was
correct but partial.

**What shipped (Source/DSP/OctaveStyleDSP.h/.cpp):**
- PeriodDoubler rebuilt as a mark-anchored 1/N-speed segment player: each inter-mark
  segment plays at 1/octaveDiv speed (sub-sample interpolated -- genuine full-spectrum
  half/quarter pitch), then hops forward octaveDiv-1 marks at the seam
  (consumption-balanced). Unwrapped double coordinates; O(1) fmod ring mapping (no wrap
  loops -- honors the wrap-ring-pointers gotcha's intent via bounded cost).
- Pitch marks: Schmitt rising crossings (same +/-0.05 thresholds as Vintage, separate
  state) on the mono Range-filtered input, sub-sample refined, validated >= 0.5 x YIN
  period; synth-fill bridges Schmitt dropouts (<= 6P) on the tracked-period grid; long
  dropouts re-sync. Marks pushed to all 4 doublers (2ch x [-1,-2]); mono-input blocks
  keep ch-1 doubler clocks in lockstep (spare scratch channel dump).
- Seam crossfade now period-proportional: clamp(P/4, 4, 64) output samples (fixed 64 is
  dead); both fade heads advance at the shifted rate.
- Fractional-period replay: segment lengths = fractional mark distances (round(P) beat
  dead); readRate 0.5/0.25 exact binary.
- Engine handoff: hysteresis switch (engage conf > 0.55, release < 0.35, plus period
  eligibility 24..2048 samples) with the existing 0.25/block one-pole as the short fade +
  snap-to-target; standing mid-blend comb dead. Unvoiced/low-conf/chord fallback =
  GRANULAR (not mute): the plan's own Task 2 (poly tracking feeds the granular path) +
  §B scenario 3 (chords must track audibly) rule out mute — flagged as my reading, not
  a docket item.
- Lag corrections: +/-1 mark hop chosen AT seams through the normal crossfade; hard
  readPos snap deleted.
- kRingSize 8192 -> 16384, kMaxPeriod 1024 -> 2048.0 (48 kHz low-note coverage; the old
  int-1024 clamp mis-periods 40-43 Hz at 48k), kMinPeriod 24.0 now an ELIGIBILITY bound
  (out-of-range = switch to granular, not clamp-to-wrong-period).
- Adjacent hardening in my edit region: C3 voicing LP state array is [3][2] but indexed
  by raw channel -- clamped to jmin(ch,1) (latent OOB for >2-ch buffers; unreachable
  through today's stereo-only rack). ensureSize floors scratch buffers at 2 channels
  (mono-shrink edge + the lockstep dump channel).

**Dev-side A/B (Python 1:1 ports of old + new, synthesized DI riff, 44.1k):**
F0 out (want in/2): lowE 41.21/41.20, A 54.99/55.00, E3 82.43/82.41, legato-drop E2
41.25/41.20, hiB 246.37/246.94 -- old engine: unshifted input pitch in all sections.
Glide D3->E3 tracked smoothly at half (75.77->81.97 vs 75.42->81.60 wanted; residual =
analyzer window blur). Period-to-period consistency 1.0; zero click events across glide +
legato octave drop. Known-minors for the campaign ear-check: ~4-cent flat on the decaying
high-B section + detrended envelope ripple 6.2% vs old 3.2% baseline -- both from
threshold-mark amplitude sensitivity; C3 duck/LP + dry blend sit on top in the real
chain. Artifacts: `octave_ab_{dry,old_m1,new_m1,old_mix,new_mix}.wav` at repo root
(untracked; disposition at the batch commit). Not covered by the A/B: the handoff mix
(block-gain logic, verified by inspection) and real-guitar transients (campaign).

**Rule 4:** no diagnostic instrumentation added. **Build:** at the Task 1 gate,
awaiting Jeff's do_build.bat.

**BUILD CLEAN (Jeff, 2026-07-18)** — Task 1 Release+Debug clean. Bell-fix engine ships.

## 2026-07-18 — Task 2 — Real polyphonic tracking (#12) (integration coded, one spec call open)

**SPEC CALL RESOLVED (Jeff 2026-07-18 = a):** single wide auto range, no instrument selector.
The plan's "range hints (guitar vs bass)" assumes a selector the octave pedal doesn't have
(reference poly-octave pedals auto-cover both; plan Files-to-modify lists no new control).
Picked (a) over (b) add-a-selector. `mPoly.setFreqRange(40, 1300)` -- 4-string bass low E up
through guitar high register; max notes stays the 6-string default. No Task 3 layout ripple.

**Coded (answer-independent), Source/DSP/OctaveStyleDSP.h/.cpp:**
- `PolyPitchTracker mPoly` member (+ include); prepare/reset wired alongside mYin; runs its
  own background worker like YIN (SY-style precedent, both prepared, pushed in Polyphonic mode).
- One mono pass (`mMonoScratch` = 0.5(L+R) of the Range-filtered input) now feeds YIN, Poly,
  AND the mark detector -- replaces the old per-loop mono recompute + the YIN stereo overload.
- Grain sizing rewritten: confident mono (conf > 0.55) -> YIN period (mono behavior preserved);
  else lowest detected poly note -> grain period (chord no longer sized by the flailing mono
  YIN reading = the "#12 stops being mono-driven" fix); else low-conf mono as last resort.
  Doubler eligibility + tracked period stay YIN-only (it's a mono single-note engine).
- Poly tracker currently at its 70-1200 Hz default (HOLD-FOR-spec-call comment at prepare);
  final range/selector lands when Jeff picks (a) vs (b).

**Rule 4:** none. **Build:** at the Task 2 gate (range finalized), awaiting Jeff's do_build.bat.

**BUILD CLEAN (Jeff, 2026-07-18)** — Task 2 Release+Debug clean. Poly-informed grain sizing ships.

## 2026-07-18 — Task 3 — Pedal-mode layout rework (code-complete, at build gate)

Single-file change: `OctaveStylePanel::resized()` in Source/Standalone/EffectEditorPanels.cpp.
Added a `PanelMode::Pedal` branch (FurmanEQ 3x3 pedal grid at :5549 is the reference).

- **Diagnosis confirmed:** pre-rework panel had ONE resized() = the full-mode right-cluster +
  32px dBFS column + 66px mode column. In the ~190x137 BaySickPedals inner tile those collapse
  (the plan's "collapses in the ~200px tile"). Not a crash -- `disableDbfsMeter()` only hides
  dbfsOut (doesn't null), so the unconditional `dbfsOut->setBounds` was safe but wasteful.
- **Pedal branch:** drop the dBFS column (hidden in pedal mode anyway -> reclaim the width);
  2x3 grid, cells = width/3 x height/2 (~63x68); knob size jlimit(28,56, min(cellW-6,cellH-16))
  ~= 52px (roomier than FurmanEQ's 34px). Order = buildKnobs (Direct/+1/-1/-2/Range) across
  row0+row1, mode chickenhead in the last cell (bottom-right). Zero overlap by construction
  (disjoint grid cells). `return` before the full-mode code.
- **Full/rack branch:** byte-identical to the original (rack layout untouched per plan).
- Tile math: pedals editor 880x480, 4x2 grid (kGridGap 10, kPagePad 12) -> tile ~206x205;
  panel inner after kTitleH(24)+kBypassH(28)+reduces ~= 190x137.

**Rule 4:** none. **Build:** at the Task 3 gate, awaiting Jeff's do_build.bat. (Eye-check --
all six controls visible, no overlap, rack unchanged -- is deferred to the campaign per B
scenario 4; not a gate item.)

**BUILD CLEAN (Jeff, 2026-07-18)** — Task 3 Release+Debug clean. Pedal-mode octave layout ships.

## 2026-07-18 — Task 4 — PDC latency (plumbing coded; ONE spec call + one clarification open)

**Plumbing coded (answer-independent, sums whatever each DSP reports):**
- `BaySickPedalsProcessor::getChainLatencySamples()` (new, .h decl + .cpp def) -- message-thread
  pull, mSlotsLock (per Jeff's note; mirrors moveSlot, audio try-locks -> ~1 dropped block per
  several hours at 5 Hz = inaudible), per-slot bypass honored exactly as processBlock (so the
  report == the live audio delay). getSlotEffect resolution (pending-aware).
- `EngineChainProcessor::getChainLatencySamples()` moved inline->.cpp; special-cases the Pedals
  stage via dynamic_cast -> pulls the board sum (the AudioProcessor accessor is a fixed 0);
  other stages (NAMIR OS) keep the cached getLatencySamples(). Lock order EngineChain::mLock ->
  Pedals::mSlotsLock is consistent with processBlock's order (no deadlock).
- End-to-end wiring CONFIRMED already live: PluginProcessor:487 wires onGetInstStripEngineLatency
  -> chain->getChainLatencySamples(); VibeGraph:2087-2089 folds it into `own`; the 5 Hz
  pollDenoiseState solve calls updateBusLatencies. So drive-pedal OS latency (~6-12 smp) STARTS
  being reported the moment this builds. (Audit report 2's "Inst engine assumed zero VG:2002"
  is STALE -- VG:2087 queries it now.)

**FINDING (audit report 2 stale):** logged above; route at section pass.

**SPEC CALL OPEN (A) -- octave dry/wet PDC handling:** the octave DSP outputs dry (0 latency,
summed undelayed) + shifted (delayed ~1/2 grain ~512 smp / doubler ~1-2 periods). Jeff LOCKED
"reports its internal delay," which rules out report-0. Remaining fork: (1) report L AND
internally delay the dry path by L so dry+octave stay coherent + the whole output PDC-aligns
(adds ~11ms to the pedal's dry in the record/mix path; Task 5 monitor path stays low-latency) vs
(3) report L only -- pure-octave (Direct=0, the signature use) lands on grid, but a blended dry
(Direct>0) goes ~11ms early. HELD: `OctaveStyleDSP::getLatencySamples()` value + possible dry
delay line until Jeff picks. (The L value itself -- proposing 512 = half the ~1024 period-synced
grain, SR/pitch-independent so PDC stays stable -- is a DSP detail, my call, not surfaced.)

**CLARIFICATION OPEN (B) -- "board-level bypass honored":** no whole-board bypass mechanism
exists on the pedalboard (grep clean; processBlock honors only per-slot bsp_slot{N}_bypass; the
Pedals stage is always in the InstPage chain). The getter honors per-slot, which matches the
audio path exactly. Flagging to confirm Jeff didn't intend an actual board on/off feature (would
be a separate add). The EffectRack precedent HAS a whole-rack bypass (mRackBypassed) -- likely
where the phrase came from.

**MONITOR ARCHITECTURE CORRECTED (Jeff, 2026-07-18):** I wrongly attributed a record-vs-monitor
SPLIT to Jeff. It was NOT in his #13 docket ("TWO modes: Dry / With Effect") -- it came from the
Task 5 plan-body wording + me importing the QA-Fe2 VOCAL split (docket 2a: low-latency
MonitorPitchShifter for monitor while WET recording keeps R3 -- that split exists ONLY because
R3 is ~48-58ms, unmonitorable). The octave engine after Task 1 is already low-latency (~1
period), so NO separate monitor path is needed. **Jeff's ruling: "With Effect" = the SAME
processed signal that gets recorded; "Dry" = the raw signal coming into that strip's input.**
One engine, no monitor tap. The plan's Task 5 "monitor tap vs full engine" split is DROPPED.
-> Task 5 is now just the Dry/With-Effect monitor MODE on the listen LED (no MonitorPitchShifter,
no fixed-ratio monitor doubler). Also collapses the entangled PDC framing: monitor=record=one
signal.

**SPEC CALL (A) RESOLVED (Jeff, 2026-07-18 = option 2):** octave reports L (512 smp Poly / 0
Vintage), dry stays INSTANT -- NO dry delay line. Consequence accepted: pure octave (Direct=0,
the signature use) lands on grid; a blended dry (Direct>0) sits ~11ms early. Simplest + zero
added latency on the processed path. `OctaveStyleDSP::getLatencySamples()` override added
(kPolyLatencySamples=512, SR-independent, stable so it never thrashes the 5 Hz solve). Vintage
Schmitt divider = 0. Mode flip's 512<->0 latency change is carried by the 5 Hz solve within
~200ms (Jeff's "re-align <=200ms" verify) -- no onLatencyChanged poke needed.

**CLARIFICATION (B):** Jeff moved past it without asking for a board on/off feature -> proceeding
per-slot-only (the getter matches the audio path exactly). No whole-board bypass added. If Jeff
wants one later it's a separate feature (would touch processBlock + the getter together).

**Task 4 CODE-COMPLETE.** All three code items done (octave getLatencySamples report + pedals
per-slot pull-sum + EngineChain Pedals special-case); the verify lines are campaign (B) items.

**Rule 4:** none. **Build:** at the Task 4 gate, awaiting Jeff's do_build.bat.

**BUILD CLEAN (Jeff, 2026-07-18)** — Task 4 Release+Debug clean. Pedal PDC pull-model ships.

## 2026-07-18 — Task 5 — Low-latency Inst monitoring (#13) (code-complete, at build gate)

**Re-flag RESOLVED (Jeff, 2026-07-18 = a):** default Inst monitor mode = **With Effect**
(the baked-pending-veto interpretation, confirmed). New Inst strips monitor the processed
tone; flip to Dry for zero-latency tight tracking when a latency pedal (octave) is loaded.

**Scope simplified** (per the monitor-architecture correction above): NO separate low-latency
monitor tap / MonitorPitchShifter. "With Effect" monitors the SAME engine that records/mixes;
"Dry" is the raw pre-engine strip signal. One engine.

**Shipped:**
- `mixer_inst_{n}_monitorMode` param -- 2-value (0 Dry / 1 With Effect), default 1. Registered
  in `addLiveInputParams` alongside the vox 3-value one (parallel `mixer_inst_` branch);
  round-trips with the project via APVTS.
- Inst Listen-LED right-click menu (MixerTrackStrip.cpp) -- `else if (mType==Inst)` after the
  vox branch; two items "Dry" / "With Effect" (vs vox's three). Writes the param.
- Monitor fork in InstStripTask: stash the pre-engine live signal (`mMonitorDryBuf`), run the
  engine as before, then cross-fade the output back toward the raw signal in Dry mode
  (`mMonitorDryGain`, ~15ms per-sample ramp per flip = click-free). Gated `active && (mode==0
  || gain>0)` so steady With-Effect (the default) pays ZERO extra work (no buffer copy, no
  crossfade). The raw DI recorder tap + FilePlay playback are untouched -> recordings + mix
  identical in both modes (monitor-only fork).
- Coverage: all three `_monitorMode` sites now have an Inst parallel (register / task-read /
  menu); grep-confirmed no fourth site.

**Interpretation noted (not a spec call, common case is exact):** "Dry" taps the pre-engine
blockView. For pure live tracking that's exactly the raw DI (Jeff's "signal coming into the
input"). In the rare FilePlay+live-monitor edge it's DI+clips (you still hear the playback,
just without the pedals) -- more sensible than dropping the clips. Flag at campaign if wrong.

**Rule 4:** none. **Build:** at the Task 5 gate, awaiting Jeff's do_build.bat.

**BUILD CLEAN (Jeff, 2026-07-18)** — Task 5 Release+Debug clean. Inst Dry/With-Effect monitor ships.

## 2026-07-18 — CODE-COMPLETE (all 5 source tasks) — Task 6 close

All five source tasks built clean at their gates (T1 pitch-sync octave-down / T2 poly grain
sizing / T3 pedal-mode layout / T4 PDC pull-model / T5 Inst Dry-With-Effect monitor). Authoring
the §B.21 section; Work Log entry drafted + HELD (applies at the §B.21 campaign pass per R2);
one commit surfaced for approval next.

---

### HELD Work Log entry (apply verbatim at the §B.21 campaign pass — R2; stamp time at apply)

### 2026-07-18 — QA-OctavePedal — Octave engine fix + poly grain + pedal UI + PDC pull + Inst monitor

**Bucket:** 2 Players (octave/BaySickPedals engine) + 6 Cross-cutting (PDC) + 3 Mixer/Routing
(Inst monitor) + 5 UI (pedal-mode layout). Final batch of bulk-run group G3.

#### Done
- **T1 — pitch-synchronous octave-down (the bell fix).** Rebuilt `OctaveStyleDSP::PeriodDoubler`
  (`Source/DSP/OctaveStyleDSP.h/.cpp`) as a mark-anchored 1/N-speed segment player: each
  inter-mark segment plays at 1/octaveDiv speed (sub-sample interp = genuine full-spectrum
  octave-down), hopping octaveDiv-1 marks at each seam. Pitch marks = Schmitt zero-crossings
  (Vintage thresholds, separate state) sub-sample-refined + YIN-validated (>= 0.5 period);
  synth-fill bridges Schmitt dropouts, long dropouts re-sync. Period-proportional seam
  crossfade (P/4, clamp 4..64; the fixed 64 is gone), fractional-period replay (round(P) beat
  gone), hysteresis-SWITCHED doubler<->granular handoff (engage conf>0.55 / release<0.35 +
  period eligibility 24..2048; the standing mid-blend comb is gone), lag correction as +/-1
  mark hop through the normal crossfade (hard readPos snap gone). Unwrapped double coords,
  O(1) fmod ring map. kRingSize 8192->16384, kMaxPeriod 1024->2048.
- **T2 — real polyphonic tracking (#12).** `PolyPitchTracker mPoly` added; one mono pass feeds
  YIN + Poly + the mark detector. Confident single notes stay on YIN->doubler (mono preserved);
  chords size the granular grain from the LOWEST detected note instead of the flailing mono YIN
  reading. Single wide auto range 40-1300 Hz (Jeff docket = a; no instrument selector), 6-note
  default cap.
- **T3 — pedal-mode layout.** `OctaveStylePanel::resized()` gained a `PanelMode::Pedal` branch
  (`EffectEditorPanels.cpp`): drops the dBFS column, grids the 5 knobs + mode chickenhead 2x3
  (~52px knobs) sized to the ~190x137 BaySickPedals tile. Full/rack branch byte-identical.
- **T4 — PDC latency (Jeff's folded Rule-3 item + scout companion).** Octave reports a stable
  512-smp latency in Polyphonic / 0 in Vintage via `getLatencySamples()`. New
  `BaySickPedalsProcessor::getChainLatencySamples()` (message-thread, mSlotsLock, per-slot
  bypass honored = matches the audio path). `EngineChainProcessor::getChainLatencySamples()`
  special-cases the Pedals stage (dynamic_cast -> pull the board sum vs the always-0 accessor).
  Consumed by the existing 5 Hz updateBusLatencies solve via onGetInstStripEngineLatency (wiring
  already live). Dry left INSTANT (Jeff docket = option 2): pure octave lands on grid, blended
  dry ~11ms early by design.
- **T5 — low-latency Inst monitoring (#13).** `mixer_inst_{n}_monitorMode` 2-value param
  (Dry / With Effect, default With Effect per Jeff's re-flag = a). Inst Listen-LED right-click
  menu (MixerTrackStrip, 2 entries). InstStripTask monitor-only fork: With Effect = the same
  processed engine that records/mixes; Dry = raw pre-engine signal, ~15ms click-free crossfade
  on flips, zero overhead in the steady default. Recording (raw DI tap) + playback untouched.

#### Found along the way
- **Shipped octave doubler was an IDENTITY on stationary tones** (root of the bell, deeper than
  the scout's seam-phase diagnosis) — dev-side A/B proved old output F0 == input F0 in every
  section. Fixed by the 1/N-speed rework.
- **PDC audit report 2 stale**: it said Inst engine latency is "assumed zero (VG:2002)", but
  VG:2087-2089 already queries onGetInstStripEngineLatency and folds it into `own`. No fix
  needed — the hook was live; QA-OctavePedal just gives it a non-zero pedal number to carry.
  Route as a §9 Forks doc-correction at section pass.
- **No whole-board bypass exists** on BaySickPedals (only per-slot). "board-level bypass honored"
  in the folded spec = the EffectRack `mRackBypassed` pattern by analogy; getter honors per-slot
  only, which matches the audio path. Not a feature gap unless Jeff wants a board on/off later.
- Latent OOB hardened in-region: OctaveStyleDSP C3 voicing LP state `[3][2]` was indexed by raw
  channel -> clamped `jmin(ch,1)`; scratch buffers floored at 2ch for the mono-lockstep dump.

#### Spec calls resolved (all asked, none self-decided)
#12 = a (single wide auto poly range, no selector); monitor architecture = one engine
(monitor "With Effect" IS the recorded/mixed signal, "Dry" = raw input — corrected a
wrongly-imported vocal-side split); octave PDC dry = option 2 (report L, dry instant);
Inst monitor default = a (With Effect). Board-bypass = per-slot only (no feature added).

#### Verification
Deferred to the §B.21 campaign pass (bulk-run R2): OP-1..OP-6. Dev-side offline A/B (Python
1:1 ports) confirmed the T1 engine outputs half-F0 across a held-note/glide/legato/chord/high-B
riff where the old engine output unshifted input pitch. Artifacts `octave_ab_*.wav` at repo root.

## 2026-07-18 — COMMIT LANDED — `d6abc38b` (QA-OctavePedal close)

One commit, 15 files (10 Source + test plan §B.21 + running notes + falcon carry-over straggler
+ .gitignore). Folded the two QA-N stragglers (test-plan §B.20 hash `2e44ab78` + falcon
session-end carry-over). WAVs added to .gitignore (`/octave_ab_*.wav`) per Jeff -> left on disk
for the OP-1 campaign ear-check, harmless if not deleted. On `main` (not detached; standup agent
was read-only). Held Work Log entry stands (applies at the §B.21 campaign pass, R2).

**Straggler for the NEXT commit:** §B.21's own `blocks:` hash backfill (`d6abc38b`) in
v1-master-test-plan.md + this running-notes block + the plan-file Carry-Over block (B.13-B.20
pattern: the batch commit lands, its own §B hash rides the next commit).

**Position:** QA-OctavePedal is the LAST G3 batch. Batch closed -> **G3 GROUP BOUNDARY** next:
R3 `/review-batch` over the combined QA-G..QA-OctavePedal diff (findings fixed before proceeding),
then Jeff's 15-30 min smoke (Debug first, then Release; §7 items 1-5). Then G4 opens. Awaiting
Jeff to open the group-boundary gate.
