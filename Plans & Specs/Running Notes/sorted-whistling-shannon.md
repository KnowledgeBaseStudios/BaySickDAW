# Running Notes — QA-RustyMeter (sorted-whistling-shannon)

> Append-only running log for the QA-RustyMeter batch. A new `## YYYY-MM-DD — Task N — <name>` entry is appended at every checkpoint (commit landed / sub-task verified / finding captured / spec call resolved / scope pivot) per `feedback_draft_doc_running_notes_every_checkpoint.md` + Main Plan §0. At batch close, `/draft-doc batch-close` reads this file as the primary input when compiling the single Implemented Work Log entry. Diagnostic instrumentation (Main Plan §0 Rule 4) gets a `## Diagnostic Instrumentation Catalog` row in the SAME edit pass as the code change (Site / Tag / Purpose / Disposition); every `Remove` row is stripped at task/batch close after surfacing the strip list to Jeff.

> **Pair file:** `Plans & Specs/Batch Plans/sorted-whistling-shannon.md`
> **Conventions:** Main Plan §0 (Document Formatting Conventions + Rule 4 Diagnostic Instrumentation Catalog + Running Notes required sections).

---

## 2026-05-29 — Task 0 — open

**Batch opened.** QA-RustyMeter: investigate + fix the pre-existing, BaySickRustyDrums-specific bug where the AriaControlPanel per-layer-volume CC sliders audibly change the rendered output but the per-strip dBFS meter on the Mixer page does not move. Routed forward from QA-DispatcherAffinity Task 3 Verify 2 (§9 forty-second Forks entry). Bucket: Players, Mixer / Routing.

**Task structure (Jeff-locked 2026-05-29, AskUserQuestion):** 3 tasks — Task 1 Investigate (static-first; PAUSE for root-cause review + fix-shape pick), Task 2 Fix (shape per Sub-A), Task 3 Close. Investigation-first; the fix shape is a genuinely deferred spec call (Sub-A), not pre-picked.

**Plan-mode pre-batch mapping (read, not assumed):**
- Per-layer sliders write APVTS `brd_cc<N>` → `parameterChanged` → `mSfizz->cc(0, cc, v)` (BaySickRustyDrumsProcessor.cpp:53).
- Exactly ONE sfizz render into `mMultiOutScratch` (`renderBlock`, :273); no separate stereo-mix render; one global `outVol` applied after (:277).
- Per-strip meter AND audible path both read the same `getStripBuffer` view into `mMultiOutScratch` (RustyInsertTask.cpp:68 → `InsertNode::processBlock` → `publishPeakReading`).
- Tension: shared buffer means the §9 "stereo-mix-down also scaled, per-strip bypasses it" hypothesis is suspect (no second mix). Likelier mechanism: `buildOutputRoutedSfzWrapper` (:656-776) injects `output=N` only into `<master>`/`<group>` via a sticky `currentPieceOutput` tracker; never annotates `<global>`/`<control>`/`<region>`. Confirmable statically against the in-repo kit SFZ (`Files For Claude/karoryfer.big-rusty-drums-1.100/Programs/`).

**Spec calls locked at open:** S1 task structure (3 tasks), S2 methodology (static-first → conditional runtime trace), S3 silly-name (`sorted-whistling-shannon`, plan-mode-runtime-assigned), S4 verify ladder (§5's 3 scenarios). Deferred: Sub-A fix shape (Task 1 close), Sub-B runtime-trace style (conditional, Task 1).

**Task 0 actions:** plan mirrored to `Batch Plans/sorted-whistling-shannon.md` + home-dir copy deleted; Main Plan §5 QA-RustyMeter `**Plan file:**` pointer updated; this running-notes file seeded.

**Open commit:** landed at `8e27a31` (plan docs only — Main Plan §5 pointer + 2 new files; +173/-1). Per Jeff (2026-05-29), the 3 `BaySick{Basses,Guitars,RustyDrums}Processor.h` CRLF-residue files were cleared via `git checkout --` (zero content diff) rather than carried forward again; working tree now clean. Commit trailer corrected to `Claude Opus 4.8` — the commit-drafter mirrors prior commits' `Claude Opus 4.7 (1M context)` trailer from `git log`; corrected to the accurate current model.

---

## 2026-05-29 — Task 1 — investigate (static phase): SFZ + wrapper route correctly; §9 prime hypothesis DISPROVED; escalating to a zero-cost meter A/B

**Static read (no build) of the in-repo Big Rusty Drums kit SFZ (`Files For Claude/karoryfer.big-rusty-drums-1.100/Programs/`) + `buildOutputRoutedSfzWrapper`.**

- **Per-layer-volume sliders → CC numbers** (from `01-full.sfz` `<control>` `label_cc`): KICK = cc70 (Kick mic) / cc71 (Kick OH) / cc72 (Kick dirt) / cc74 (Kick punch); SNARE = cc80 (bottom/Btm) / cc81 (top/Top) / cc82 (OH) / cc83 (snap) / cc84 (punch) / cc85 (epic). Exactly match Jeff's sliders.
- **Each per-layer-volume CC scales one mic/layer via `amplitude_cc<N>` at the `<master>` level** inside the per-piece mapping file (`kick_24_map.sfz` lines 3/22/40/58; `snare_14_map.sfz` lines 6/24/42/60/79/98 + edge/rims repeats), each `<master>` `#include`-ing its mic sub-file (`k_kick`/`k_oh`/`k_sn`; `sn_center/edge/rims_btm/top/oh`).
- **The wrapper appends `output=N` to EVERY `<master>`/`<group>` line** in the inlined map file (`annotateMastersWithOutput`, BaySickRustyDrumsProcessor.cpp:694-707); `classifyIncludePath` maps `kick_24_map.sfz` → kick piece, `snare_14_map.sfz` → snare piece. So all kick mic `<master>` blocks get `output=kickStrip`, all snare layer `<master>` blocks get `output=snareStrip`. The `amplitude_cc` and the `output=` sit on the SAME `<master>` block → the CC-scaled voices land on the correct strip's output bus. **Verified for both kick and snare.**
- **Strip-index consistency:** the wrapper's `output=N` uses `stripIdxForDrummerOrder` (index into `mChannels`); `getStripBuffer(stripIdx)` + RustyInsertTask `mStripIndex` use the same `mChannels` index. Consistent — no wrapper↔meter index mismatch.

**Conclusion:** the §9 "prime suspect" — wrapper extracts raw per-channel audio BEFORE the per-layer-volume CC scaling — is **DISPROVED**. The CC scaling and the output routing are on the same regions; the strip output bus should carry the scaled audio, so the per-strip meter SHOULD track the slider.

**Unresolved tension (requires runtime observation):** the audible path AND the per-strip meter read the SAME `mMultiOutScratch` strip buffer. So audible-louder implies meter-louder — UNLESS the louder audio lands on a DIFFERENT strip than the one being watched. The static read says routing is correct, so a file-only read can't explain the symptom; the decisive next step is a runtime A/B showing WHICH strip's level actually changes.

**Methodology pivot — cheapest decisive test (zero build, zero instrumentation):** the Mixer ALREADY shows every Rusty per-strip dBFS meter. Before adding any trace (Sub-B), ask Jeff to play a kick+snare pattern, move a KICK/SNARE per-layer-volume slider, and report which strip meter (if any) responds. This is the diagnose-before-fix A/B (`feedback_diagnose_before_fixing.md`) using existing instrumentation. A Sub-B runtime trace is the fallback only if the existing meters don't resolve which strip moves.

**Surfaced to Jeff:** static finding + the zero-cost meter A/B. Awaiting his observation.

---

## 2026-05-29 — Task 1 — investigate (meter path): dBFS meter confirmed PEAK; Jeff reframed symptom; diagnosis pivots to peak-vs-loudness

**Jeff reframed the symptom (2026-05-29, before running the A/B):** the per-strip dBFS meter is NOT flat-as-in-silent — it reads a level, but that level does NOT increase between base fader settings and the faders cranked to max, even though the audio audibly gets louder. True for BOTH the individual strips AND the bus. Jeff's hypothesis: "this could also just be a visual issue in terms of what the dbfs meter is reading."

**Code confirmation (VibeGraph.cpp + SharedUI):**
- The per-strip / bus dBFS meter is a PEAK meter: `bufferPeakDbStereo` (VibeGraph.cpp:61) uses `buf.getMagnitude()` = max absolute sample in the block. Floor −60 dB; no artificial cap below 0.
- `publishPeakReading` (VibeGraph.cpp:119) CAS-maxes the per-block (latency-compensated) peak into the node atomics; `DBFSMeter` (SharedUI.h:1623, .cpp:6436+) displays it with FL-style decay + peak-hold ballistics (`dbToNorm` log mapping). No cap/smoothing that would hide a genuine peak increase.

**Diagnosis (reframed from routing → meter semantics):** the per-layer-volume sliders are mic-mix controls (close / OH / room / punch / snap / etc.). A PEAK meter shows the loudest instantaneous sample, dominated by the close-mic transient. Boosting overhead/room/body mics — or summing multiple decorrelated mics — raises perceived loudness + RMS substantially while raising the PEAK little, so the peak meter correctly shows ~no change even though it is audibly louder. The kit also ships an internal `com.mda.Limiter` (`<effect>` at 01-full.sfz:348; cc400 thresh / cc401 level) which, IF it sits in the per-output path, would further hold peaks down. The audio routing to the strips is correct (static phase) → this is NOT a routing bug; the dBFS meter measures PEAK, not loudness. Jeff's hypothesis is essentially correct.

**Surfaced to Jeff:** the peak-vs-loudness diagnosis + a confirming A/B (close mic vs OH mic + the strip-fader as control) + the design decision (this is the reframed Sub-A): (A) accept peak metering as standard behavior, or (B) make the meter reflect loudness (RMS/LUFS) — noting that changing meter ballistics is a GLOBAL, all-meters scope decision, not Rusty-specific. Awaiting Jeff's confirmation + decision.

---

## 2026-05-29 — Task 1 → SCOPE PIVOT: diagnosis closed (not a bug); batch re-scoped to a metering architecture upgrade

**Diagnosis conclusion (settled):** the per-layer-volume "meter-vs-knob disconnect" is NOT a routing bug and NOT a Rusty-specific meter defect. ALL meters in the app are PEAK meters (one shared `bufferPeakDbStereo`/`publishPeakReading`/`DBFSMeter` path, unified by QA-AudioMeters). A peak meter tracks pure-gain controls (Guitars/Basses volume knobs, mixer strip faders, Rusty close-mic faders → all move the meter) but NOT loudness-only changes (Rusty's overhead/room/body mic faders, or decorrelated multi-mic summing → loudness/RMS up, peak ~flat). Rusty is the only engine with mic-mix faders, which is why the symptom appeared Rusty-specific. Jeff's "it's what the meter reads" hypothesis = correct.

**Jeff's decision (2026-05-29): PIVOT — upgrade the metering architecture (option 2, taken much further).** New spec (Jeff verbatim intent):
1. **Split meter (ALL strips):** split the meter component's visual height exactly in half. Bottom half = the existing dBFS PEAK bar. Top half = a scrolling waveform history of RMS values (circular/history buffer of recent RMS; in `paint()` divide the bounds, draw the peak bar on the bottom, draw the scrolling RMS wave on the top with a `juce::Path`, shifting it top→bottom a few px each frame).
2. **Master Strip LUFS readout (Master strip ONLY):** a live LUFS numeric readout in a dedicated box situated EXACTLY between the stereo width knob and the master fader. Fader thumb overlapping the box above unity (>0 dB) is acceptable per Jeff. Needs a Momentary or Short-Term EBU R128 LUFS calc in the master DSP chain broadcasting to a new custom Label/painted box.
Jeff: "Outline the JUCE classes we will need to modify or create (e.g., the circular buffer for the juce::Path, and the LUFS DSP node) so we can start building."

**Scope-pivot bookkeeping (to handle during re-plan, with Jeff's sign-off):** the original §5 QA-RustyMeter scope/verify (per-layer-volume CC fix) is superseded; §5 needs rewriting to the metering-upgrade scope (the original "bug" = diagnosed not-a-bug); a §9 Forks entry should document the pivot (diagnosis → not-a-bug → re-scope); batch name stays QA-RustyMeter (Jeff said "update the plan", i.e. in place — to confirm). This re-scope ALSO retires the original Sub-A fix-shape option space (wrapper/sfizz-internal patches) — superseded by the new design.

**Action launched:** "Understand" multi-agent workflow (run `wf_00df78f3-d19`) mapping the metering code surface (DBFSMeter internals, MixerTrackStrip + master strip layout, the peak publish path for adding RMS + master LUFS, reusable scrolling-waveform/history patterns) + an EBU R128 LUFS implementation recipe. On return: surface the new design spec calls (RMS window, history length, scroll speed/colors, LUFS Momentary-vs-Short-Term, master-meter-also-splits?, perf approach) to Jeff, then re-enter plan mode for the updated plan + class outline.

**No source changes yet.** Still pre-build; investigation/diagnosis only.

---

## 2026-05-29 — Task 1 — "Understand" workflow complete (metering code surface + LUFS recipe)

Multi-agent Understand pass (run `wf_00df78f3-d19`, 5 agents) returned. Key facts (file:line for the plan):

- **DBFSMeter** (`SharedUI.h:1623-1701`, `.cpp:6405-6663`): peak atomics `mLevelDbL/R` (`:1672-1673`); display/peak-hold state `mDisplayDbL/R`,`mPeakDbL/R` (`:1679-1681`); constants kFloor=-60/kCeiling=+6/kDecayDbPerSec=20/kPeakHoldMs=1000/kBreakDb=-18/kBreakNorm=0.7. `onVBlank()` (`.cpp:6466`) exchange-reset + ballistics + repaint. `paint()` (`.cpp:6627`) housing→L/R halves→`paintBar()`×2→gutter→bezel. `dbToNorm()` (`.cpp:6520`). **Split-in plan:** in `paint()` divide bounds → topRect (NEW `paintRmsWaveform()`) + bottomRect (existing bar); push RMS into a new ring in `onVBlank()`.
- **Master strip** = `MixerTrackStrip` (no subclass) with `StripType::Master`. resized() (`.cpp:529-659`): …→Pan→Width knob (ends y≈178)→Fader (starts y≈178). **CRITICAL: zero gap today between width knob + fader** → the LUFS box needs a NEW inserted row (`if (masterRow)`, ~18-20px) between them, pushing the fader down. Master audio: `MasterTask::run()`→`MasterBusNode::processBlock` (`VibeGraph.cpp:838-901`): preEq→rack→busEq→fader→pan→width(~:887)→publishPeakReading(~:897). LUFS node attaches after width. `masterPeakDbL/R` at `VibeGraph.h:628-629` (add `masterLufs` beside).
- **Peak publish path** = getMagnitude→`publishPeakReading` (`VibeGraph.cpp:119`, CAS-max+ring)→per-kind array exchange-store→`drainMeterAtomicsForUI` (`PluginProcessor.cpp:2112`)→`drainInsertPeakDbStereo` UI poll→`MixerPage` vblank drain (`:3251-3284`)→`strip->setStereoLevel`→`DBFSMeter`. **Parallel RMS** = extend `publishPeakReading` (compute block sum-of-squares, reuse `EngineSidechainHelper.h:75-82` pattern) + a parallel atomic/array across the SAME ~12 plumbing sites (all insert kinds + buses + master). Mechanical but broad.
- **Reuse templates:** `BaySickVisualizerScreen.cpp:375-418` (juce::Path build + 3-layer neon stroke) = best scrolling-wave paint template; `SpectrumFeed.h` (seqlock) if a multi-value frame is needed; `DBFSMeter` atomic+vblank = broadcast/ballistics.
- **LUFS recipe** saved to `Plans & Specs/Research Reports/daw-architecture-lufs-momentary-shortterm-metering-2026-05-29.md` (K-weighting coeffs + bilinear derivation + 400ms/3s rings + ungated formula + relaxed-atomic broadcast; new `Source/DSP/LufsMeterDSP.{h,cpp}` owned by MasterBusNode; JUCE ships no helper).

**Surfacing 4 design spec calls to Jeff** (LUFS mode / master-meter-also-splits / RMS rollout scope / top-half visual). Finer details (RMS window, history depth, scroll px/frame, colors) to be proposed in the plan for approval at ExitPlanMode. Then re-plan.

---

## 2026-05-30 — Task 1 — spec call #1 resolved (LUFS box = M/S/I, selectable)

**Decision #1 (Jeff 2026-05-30):** the Master LUFS box computes ALL THREE modes continuously (Momentary 400 ms + Short-Term 3 s + Integrated gated whole-play) but DISPLAYS only one at a time. Box layout: a stacked [LUFS value / mode-title label underneath] on the left + a dropdown triangle (`▾`) to the right of the number+title; clicking the triangle opens a menu to select which mode (M / S / I) is shown.

**Scope implication — Integrated is now IN (the larger LUFS build):** beyond Momentary + Short-Term (ungated sliding windows), Integrated needs (a) EBU gating — absolute −70 LUFS gate + relative −10 LU gate over a 400 ms-block histogram, and (b) a TRANSPORT HOOKUP so the Integrated accumulation RESETS on play-from-top / loop-start (Jeff's "since play up until song end or start of a new loop"). This pulls in the item the LUFS research report deferred (`Research Reports/daw-architecture-lufs-momentary-shortterm-metering-2026-05-29.md` "Deferred / out of scope" → now in scope). The class outline will call out the play/loop reset hook into the LUFS node.

**Remaining spec calls (#2-#11):** Jeff answering next ("I will now figure out answers to the other questions").

---

## 2026-05-30 — Task 1 — design spec calls #2-#11 RESOLVED (Jeff)

Locked design of record for the QA-RustyMeter metering upgrade (feeds the re-plan):

- **#1 LUFS box:** M + S + I all compute continuously; ONE displayed at a time; box = [LUFS value / mode-title label under] + `▾` dropdown selector to the right. Integrated needs EBU gating + transport reset-on-play/loop hook.
- **#2 = (b) Master keeps a FULL-HEIGHT peak bar** (no split) + gets the LUFS box. → `DBFSMeter` needs a per-instance mode flag: **Split** (peak bottom + RMS top) for all non-master strips vs **Full** (peak only) for master.
- **#3 = (a) All strips split THIS batch** → full per-strip RMS publish across every insert kind (Layer/Bass/Drum/Audio/Aux/Vox/Inst/Rusty) + the buses. Master excluded (full bar). Biggest plumbing, accepted.
- **#4 = (a) Centered scope trace** — RMS mirrored around a vertical centerline.
- **#5 = (b) Windowed RMS ~150-300 ms** (smoother than per-block).
- **#6 = (b) Medium history ~3-4 s**; scroll px/frame derived so the top-half span = chosen history.
- **#7 = (a) per-channel L/R, with L on the LEFT half + R on the RIGHT half**, both emanating from the center → reads as one stereo waveform; matches the bottom peak bar's L/R split.
- **#8 Color = dBFS palette keyed to horizontal deflection (= RMS level):** green near center → yellow → hot orange → **red at the outer edge**. Same colors as the LED bar. = **smooth gradient** through those colors (NOT hard-stepped at LED thresholds). Jeff confirmed 2026-05-30 ("That's exactly what I want"). LOCKED.
- **#9 LUFS box ~18-20 px** (single mode shown, not stacked).
- **#10 = 50/50 split ratio LOCKED.**
- **#11 = re-scope QA-RustyMeter IN PLACE** (Jeff: investigation + code map + LUFS research already done here; a new batch would reload that context = wasted tokens). Rewrite §5 scope to the metering upgrade + log the pivot in §9; same batch name.

**Next:** confirm #8 with Jeff, then re-enter plan mode for the full updated plan + JUCE class outline (RMS history-ring + `paintRmsWaveform` split-mode `DBFSMeter`; per-strip RMS publish across the meter plumbing; `LufsMeterDSP` master node with M/S/I + gating + transport reset; LUFS box component with mode selector).

**Task breakdown (Jeff 2026-05-30): Option A — 3 tasks** (chosen for context economy — fewer build/verify cycles): **Task 2 = Split meter** (per-strip windowed-RMS publish + `DBFSMeter` split display, all non-master strips); **Task 3 = Master LUFS readout** (`LufsMeterDSP` M/S/I + gating + play/loop reset + LUFS box UI with `▾` selector on master strip); **Task 4 = Close**. All 11 design decisions + the task breakdown now locked → re-entering plan mode for the full plan rewrite (re-scope in place; §5 scope rewrite + §9 pivot entry + plan mirror to follow at approval).

---

## 2026-05-30 — Task 2 step 0 — plan approved + re-scope docs landed

**Plan approved** (Jeff 2026-05-30, after a first ExitPlanMode where Jeff asked "Is there a reason there is no code laid out for these actions?" → plan rewritten with concrete code blocks per piece: `LufsMeterDSP` K-weighting derivation + bin logic, `paintRmsWaveform` + RMS ring, `publishRms` EMA, transport-reset detection, `LufsReadoutBox`, master-strip row insertion). Plan mirrored over `Batch Plans/sorted-whistling-shannon.md` (home copy deleted).

**Re-scope bookkeeping applied to Main Plan:** §5 QA-RustyMeter entry rewritten (header + Origin/diagnosis + Items + Scope + Risk + Effort + Bucket + Verify → metering upgrade); §6 footnote Scope updated; §9 forty-fourth Forks entry appended (diagnosis = not-a-bug + pivot). LUFS research report saved at `Research Reports/daw-architecture-lufs-momentary-shortterm-metering-2026-05-29.md`.

**Re-scope docs commit (Task 2 step 0):** landed at `217b9cf` (4 files, +556/-140; docs only). Working tree clean. Task 2 source (split meter) starting.

---

## 2026-05-30 — Task 2 (part 1) — split meter + insert-strip RMS implemented (pre-build)

**DBFSMeter (SharedUI.h/.cpp):** `Layout {Full, Split}` + `setMeterLayout`; RMS history ring (`kRmsHist=256`) + `mRmsHead`; `setRmsStereo` (raw per-frame in) + `onVBlank` EMA-smooth (~200 ms, `kRmsTimeConstSec`) → push to ring (Split only); `paintBars` (extracted L/R peak bars) + `paintRmsWaveform` (centered, L-left/R-right, `dbToNorm` deflection, smooth dBFS-palette gradient green-center → red-edge); `paint()` splits 50/50 (Split top RMS + bottom bars) or full bar (Full).

**RMS publish — SIMPLIFIED vs the plan (direct-node-read, NO mirror):** `publishRms` helper (per-block `sqrt(mean-square)` dB, CAS-max into node `rmsDbL/R` — multi-call safe like peak; audio thread never resets, UI exchange-resets). `InsertNode` + `rmsDbL/R` + `publishRms` call. `VibeGraph::drainInsertNodeRms` (exchange-reset node rms via `getInsertNode` — message-thread safe; insert nodes create/destroy on the message thread, so no race). `PluginProcessor::drainInsertRmsDbStereo` passthrough. `MixerPage::onVBlank` `drainStereoInsert` += RMS drain → `setRmsStereo`. `MixerTrackStrip::setRmsStereo` → `mMeter`. **No per-kind RMS arrays, no PluginProcessor mirrors, no `drainMeterAtomicsForUI` change** — RMS is a current value the UI reads off the node directly (the peak's two-hop mirror exists only because its drain runs audio-side). The ~200 ms windowing moved UI-side (vs the plan's audio-thread EMA) for multi-call correctness + zero sample-rate plumbing.

**Layout assignment:** all insert/channel strips = Split (verified: Layer=`LayerChannel`, Bass=`BassChannel`, Drum=`DrumChannel`, Audio=`LayerChannel`, Rusty=`DrumChannel`, Aux/Vox/Inst); Master + all `Bus` strips = Full (no empty split).

**Staging (part 1 of 2):** Task 2's RMS plumbing split into 2 builds (the buses use a different named-member peak path than the indexed inserts). Part 1 (this build) = split meter + RMS on all insert strips; buses keep their full peak bar. Part 2 = bus-node RMS + switch buses to Split.

**Part 2 MUST cover ALL bus strips (Jeff flag 2026-05-30), not just the core 8 in the `MixerPage::onVBlank` drain loop (Master/Layers/Bass/Drums/FX/Clips/Vox/Inst/RustyDrums):** there are also **Vox Bus 2 (`mVoxBus2Strip`, :1842), Inst Bus 2 (`mInstBus2Strip`, :1906), Inst Bus 3 (`mInstBus3Strip`, :1968)** — all `StripType::Bus` (so part 1 correctly gives them Full). These 3 are created in separate on-demand blocks; part 2 must confirm where they get peak-drained and wire their RMS + Split there too. Part-1 layout is type-based (`Master||Bus → Full`), so it already covers them correctly — but the part-2 RMS wiring needs each one explicitly.

---

## 2026-05-30 — Task 2 (part 1) — visual iteration after Jeff's first verify (4 tests PASS)

Jeff verified part 1 in Debug + Release: **4 tests PASS** (split meter renders, centered L/R, color gradient, scrolls). Feedback → 3 visual fixes applied (pre-rebuild):
- **Split ratio 50/50 → 65/35** (dBFS peak bar = 65% bottom, RMS wave = 35% top); `kRmsTopFrac = 0.35`.
- **"Doesn't wave"** (two near-static lines unless quiet — RMS is a near-constant loudness for sustained audio): shortened the UI smoothing `kRmsTimeConstSec` 200 ms → **50 ms** so the wave tracks the music's dynamics. Offered Jeff a switch to **peak-excursion** plotting if he wants true-waveform wiggle on every transient (one-line change, keeps the rest).
- **"Just lines — fill the middle":** rewrote `paintRmsWaveform` from two stroked traces to a **filled** stereo-waveform path (R deflects right of centre, L deflects left, band spans the centre line) with a **symmetric** dBFS-palette gradient (green centre → yellow → orange → red at both outer edges, #8).

**Jeff verified 2026-05-30: "this all looks good"** — the 65/35 split + filled gradient waveform + dynamics-tracking wave are all approved. Part 1 (inserts) visual + functional = DONE. Committed at `6a2e35e` (11 files, +257/-17, working tree clean). Part 2 (bus-node RMS + flip buses to Split, incl. Vox Bus 2 / Inst Bus 2 / Inst Bus 3) starting.

---

## 2026-05-30 — Task 2 (part 2) — bus-RMS architecture map (for implementation)

Mapped the bus meter path (12 buses; Master stays Full so 11 need RMS):
- **VibeGraph per-bus PEAK member atomics** (`VibeGraph.h:621-658`): `layers/bass/drums/master/fxBus/audioClips/voxBus/voxBus2/instBus/instBus2/instBus3/rustyDrumsBus` each `PeakDb`/`PeakDbL`/`PeakDbR`.
- **5 dedicated bus NODES** drained in `processBus`/`processMasterBus` branches (`VibeGraph.cpp:1536-1618`): `mLayersNode`/`mBassNode`/`mDrumsNode` (`processChainOnly` → exchange-store into `<bus>PeakDb*`), `mMasterNode` (`processMasterBus`), `mEffectsBusNode` (`processEffectsBus`, kFxBus branch).
- **7 generic buses** via the shared path (`processBus` :1620-1742+), `switch(busChId)` sets `preEq/rack/postEq/node/prefix` where `node` = `InstrChannelNode*` (`mAudioClipsBusNode`/`mVoxBusNode`/`mInstBusNode`/`mVoxBus2Node`/`mInstBus2Node`/`mInstBus3Node`/`mRustyDrumsBusNode`); chain → `publishPeakReading` → exchange-store into the per-bus member (peak store + busChId switch live just past :1742).
- **MixerPage::onVBlank** (`:3261-3269`) drains the CORE 9 buses via `drainStereoBus(strip, mProcessor.m<Bus>PeakDbL, ...R)`. **Vox2/Inst2/Inst3 are NOT in that loop** (created on-demand at MixerPage `:1842/1906/1968`) — their meter drain is elsewhere; part 2 must locate + wire it (Jeff's flag).

**Lean approach (mirrors part-1 direct-read, no PluginProcessor mirrors):** add 22 VibeGraph per-bus RMS member atomics (11 buses × L/R; Master excluded). CAS-max the per-block RMS into them via `publishRms(buf, <bus>RmsDbL, <bus>RmsDbR)` in each dedicated branch (Layers/Bass/Drums/FX) + ONE call in the generic path using a switch-set `std::atomic<float>* rmsL/rmsR`. Audio thread never resets them; UI exchange-resets via new `VibeGraph::drainBusRms(busChId)` → `PluginProcessor::drainBusRmsDbStereo` passthrough → MixerPage bus drain (pass `busChId`, add `setRmsStereo`). Construction: flip `Bus → Split` (change the ctor check from `(Master||Bus)?Full:Split` to `Master?Full:Split`). Find + wire the Vox2/Inst2/Inst3 drain. Est. ~25-30 edits across VibeGraph.h/.cpp, PluginProcessor.h/.cpp, MixerPage.cpp, MixerTrackStrip.cpp.


## 2026-05-30 — Task 2 — OUT-OF-SCOPE bug surfaced during testing: false "File Already in Library" prompt on a NEW empty project

Jeff (while testing the meter): opened a project containing multiple copies of his song → confusing → File > New (fresh empty project, no samples in the sample folder) → dropped the same `.mp3` → got the **"File Already in Library — already in your library on 'an existing page'"** prompt (Use Existing / New Page / Cancel) despite the new project being empty. The per-project dedup check is consulting STALE state — the previously-open project's page/library index NOT cleared on File > New (or a global library index that should be per-project). Same family as the QA-D STATE-* project-lifecycle reset bugs (New-Project-doesn't-reset-X). **UNRELATED to metering.** Routing surfaced to Jeff (fix-now vs own batch) per Main Plan §0 Rule 3 + `feedback_qa_batches_fix_bugs_dont_defer.md`. Captured here so it is not lost regardless of routing.

**Resolution (Jeff 2026-05-30): fix IN this batch as END-BATCH CLEANUP** (after the metering tasks; do not stop the current work). Added to the plan as **Task 4 — Project-lifecycle dedup fix**; Close renumbered to Task 5. In-batch resolution → recorded in the close-entry routing table at batch close (NOT a §9 cross-batch route).


**Possible build nits flagged to Jeff:** `<utility>` in VibeGraph.h (std::pair) + `<cmath>` in VibeGraph.cpp (std::sqrt) — both almost certainly already pulled via JUCE; easy add if the build complains.

**Files:** SharedUI.h/.cpp, VibeGraph.h/.cpp, PluginProcessor.h/.cpp, MixerPage.cpp, MixerTrackStrip.h/.cpp. Pre-build; awaiting Jeff's Debug+Release verify.

---

## 2026-05-30 — Task 2 (part 2) — bus RMS implemented (pre-build)

**Followed the part-2 architecture map exactly, lean direct-node-read (NO PluginProcessor mirror)** — same shape as part 1's insert RMS. Note the asymmetry vs the bus PEAK path: bus peak is an audio-side two-hop (node atom → VibeGraph member via `processBus` exchange-store → PluginProcessor mirror via `drainMeterAtomicsForUI`); bus RMS is a current value the UI reads straight off VibeGraph's per-bus atoms (the mirror exists for peak only because its drain runs audio-side). **6 source files, +119/-19.**

- **VibeGraph.h:** 22 new per-bus RMS member atomics (11 non-master buses × L/R, default −60) right after the per-bus PeakDb atoms — `layers/bass/drums/fxBus/audioClips/voxBus/voxBus2/instBus/instBus2/instBus3/rustyDrumsBus` each `RmsDbL/RmsDbR`; NO mono sibling (RMS has no legacy reader). Plus a `drainBusRms(int busChId)` decl next to `drainInsertNodeRms`.
- **VibeGraph.cpp:** reused the part-1 anon-namespace `publishRms` helper (per-block `sqrt(mean-square)` → dB, CAS-max into the atoms, audio thread never resets). Added a `publishRms(buf, <bus>RmsDbL/R)` in each of the 4 DEDICATED branches (Layers/Bass/Drums in `processBus` after their peak exchange-store; FX inside the kFxBus `if (mEffectsBusNode != nullptr)` block after its peak stores). For the 7 GENERIC buses (Clips/Vox/Inst/Vox2/Inst2/Inst3/Rusty via the `InstrChannelNode` shared path), added `std::atomic<float>* rmsL/rmsR` locals set per-bus in the existing switch (alongside node/prefix), then ONE `if (rmsL && rmsR) publishRms(buf, *rmsL, *rmsR);` inside the existing `if (node != nullptr)` peak block. Added the `drainBusRms(busChId)` definition (switch over the 11 bus channel ids; kMaster + unknown → {−inf,−inf}; exchange-resets the pair).
- **PluginProcessor.h/.cpp:** `drainBusRmsDbStereo(int busChId)` thin passthrough to `mVibeGraph.drainBusRms` (sibling of `drainInsertRmsDbStereo`; no mirror).
- **MixerPage.cpp:** `drainStereoBus` lambda gained a `busChId` param + `[this]` capture; after `setStereoLevel` it now drains `mProcessor.drainBusRmsDbStereo(busChId)` → `strip->setRmsStereo`. All 12 call sites pass the bus id (9 core in the `onVBlank` loop + the 3 on-demand Vox Bus 2 / Inst Bus 2 / Inst Bus 3 conditional drains lower down — explicitly wired per Jeff's part-1 flag). Master passes kMaster → returns −inf → harmless no-op on the Full meter.
- **MixerTrackStrip.cpp:** ctor layout flip — was `(Master || Bus) ? Full : Split`, now `Master ? Full : Split`, so every Bus strip is now Split (Master alone stays Full; it carries the LUFS box in Task 3).

**SharedUI.h/.cpp + MixerTrackStrip.h NOT touched** — the meter rendering + `setRmsStereo` already landed in part 1; part 2 is pure plumbing + the ctor flip.

**Verified statically (pre-build):** all 22 RMS atoms appear exactly 3× each (decl + publish site + drain switch); the `drainBusRms`/`drainBusRmsDbStereo` chain is wired across all 5 files; no leftover `StripType::Bus` Full check remains.

**Build nits to watch (same as part 1, likely already pulled via JUCE):** `std::pair` (VibeGraph.h already uses it via `drainInsertNodeRms`), `std::sqrt` + CAS in `publishRms` (already compiled in part 1).

**Still pre-build; awaiting Jeff's Debug-then-Release verify:** all 11 non-master bus strips show the split meter now (bottom peak bar + top scrolling RMS waveform reacting to bus level); Master stays a full peak bar; no regression on insert-strip meters or peak readings.

**Open offer still standing (carry from part 1):** the RMS top-half plots windowed RMS (~50 ms UI smoothing); a one-line switch to peak-excursion plotting is available if Jeff wants more per-transient "waveform" wiggle — to confirm at verify.

---

## 2026-05-30 — Task 2 (part 2) verified + Task 5 (bus collapse UI) folded in

### (A) Task 2 (part 2) — VERIFIED

**Jeff verified the bus-RMS build in Debug + Release 2026-05-30: "Everything passes."** All 11 non-master bus strips now show the split meter (bottom dBFS peak bar + top scrolling RMS waveform reacting to bus level); Master stays a full peak bar; no regression on the part-1 insert-strip meters or the peak readings. Part 2 = functionally + visually DONE.

**Open offer from part 1 (RMS top-half windowed-RMS → peak-excursion switch) — CLOSED / DECLINED.** Jeff: leave it as-is. The windowed RMS looks fine on individual tracks; the near-static "two lines" look that prompted the original offer was an artifact of Jeff monitoring a FULL SONG mastered to ~-10 LUFS (the loudness genuinely IS that loud + constant, so the wave correctly reads near-flat). No code change — the one-line peak-excursion path is retired as an option, not taken.

**Ready to commit (hash TBD — filled in by the commit that bundles this entry).** Part 2 = 6 source files (VibeGraph.h/.cpp, PluginProcessor.h/.cpp, MixerPage.cpp, MixerTrackStrip.cpp), +119/-19, per the prior pre-build entry. SharedUI.h/.cpp + MixerTrackStrip.h untouched (meter render + `setRmsStereo` landed in part 1).

### (B) NEW out-of-scope feature folded into end-batch cleanup — bus collapse/expand UI (plan Task 5; Close renumbered Task 5 → Task 6)

Jeff requested this right after the part-2 verify. Spec surfaced + confirmed in plain-English design terms BEFORE any plan/notes edit (per `feedback_plan_and_wait_for_explicit_confirm_on_semantics_changes.md`). Confirmed spec of record:

- **Arrow button placement (Jeff answer #1):** a small arrow button in the RibbonTabBar tab-arrow style on each BUS strip's top row, to the RIGHT of the name label. **BUSES ONLY — no arrow on the Master strip.**
- **Behavior:** default arrow DOWN = expanded (looks exactly as today). Click → flips UP → that bus's grouped member strips collapse/hide AND the layout closes the gap (member strips sit flush-right of their bus, so removing them pulls everything left). Click again → expand. The bus strip itself ALWAYS stays visible. Each bus collapses ONLY its own group (e.g. Vox != Vox Bus 2 — independent groups). Pure VIEW state — hidden member strips still process + meter; NO audio change.
- **Persistence (Jeff answer #3):** collapsed/expanded state PERSISTS THROUGH SAVE (per-project state).
- **Tooltip change on ALL strips (Jeff answer #2):** because the name label shrinks to make room for the arrow (and truncates when narrow), every strip's tooltip now shows the full displayed name on the top line + "Double-click to rename" on the line below.
- **Disabled state (Jeff answer #4):** the arrow is greyed-out / disabled when a bus has NO members to collapse.

**Layout grounding (read, not assumed):** `MixerPage::laidOutBus` (MixerPage.cpp:3529) + `layoutGroup` (:3494) lay each bus strip then its members so `buckets[busChId]` members render flush to the right of their bus. Collapse = skip the member strips in the layout pass + don't advance the x cursor past them. Persistence will most likely be a lazily-registered per-bus APVTS `_collapsed` bool (mirrors the existing lazy `_mute`/`_solo` registration) — exact impl settled at build. Tooltip set in the `MixerTrackStrip` ctor (MixerTrackStrip.cpp:52) + refreshed in `onTextChange` (:53).

**Slot (Jeff accepted):** new **Task 5 — Bus collapse/expand UI**, sequenced AFTER the dedup-fix Task 4; Close moved to **Task 6**. IN-BATCH resolution → recorded in the batch-close routing table at close, NOT a §9 cross-batch route (same disposition as the Task 4 dedup fold). No §9 Forks entry needed; the plan already carries the new Task 5.

**Net plan task list now:** Task 2 Split meter (part 1 inserts DONE `6a2e35e` + part 2 buses VERIFIED, commit TBD) → Task 3 Master LUFS readout → Task 4 Project-lifecycle dedup fix → Task 5 Bus collapse/expand UI → Task 6 Close.

> Reconcile (post prior entry): Task 2 part 2 committed at `58e3caa` (the "commit TBD" above predated the commit; append-only, so noted here).

---

## 2026-05-30 — Task 3 — Master LUFS readout implemented (pre-build)

**Master LUFS readout (Momentary / Short-Term / Integrated + selector) implemented per the locked design (#1 M/S/I selectable, #9 ~18-20 px box).** 11 modified + 2 new files. Owns a new EBU R128 / BS.1770 LUFS DSP on the master sum, broadcasts M/S/I via 3 relaxed atomics, and renders a click-to-select box between the master width knob and fader. Pre-build; statically cross-checked, not yet built/verified.

- **NEW DSP — `Source/DSP/LufsMeterDSP.{h,cpp}`:** EBU R128 / BS.1770 on the stereo master sum. K-weighting derived bilinear-from-constants per `prepareToPlay` (exact at any fs — Stage 1 high-shelf targets a1=-1.69066/a2=0.73248, Stage 2 RLB high-pass a1=-1.99005/a2=0.99007; constants + the 48 kHz acceptance table taken from the LUFS research report `Research Reports/daw-architecture-lufs-momentary-shortterm-metering-2026-05-29.md`). **Momentary (400 ms) + Short-Term (3 s) = ungated sliding windows** over a 50 ms per-bin energy ring (`kBinsPerSec=20`, `kShortTermBins=60`). **Integrated = gated.** 3 relaxed atomics (M/S/I) broadcast to the UI; `resetIntegrated()` zeroes the histogram (M/S keep running).
- **DESIGN REFINEMENT vs the plan's growing-`std::vector` sketch (my call, not a spec change — same LUFS numbers, invisible to user):** the Integrated gate is a **FIXED 751-bin loudness histogram** (libebur128's method) — -70 LUFS absolute gate at insert + -10 LU relative gate recomputed over the histogram each 100 ms gating block (75% overlap). This is **allocation-free on the audio thread** (standing project rule) and **O(bins) regardless of song length**, vs a per-block-growing vector. The histogram constants block precedes the array members in the header (static-verified).
- **AUDIO WIRING — `MasterBusNode` owns `LufsMeterDSP mLufs` (VibeGraph.cpp):** `prepareToPlay` in `MasterBusNode::prepare`; `mLufs.process(buf)` in `processBlock` **AFTER the fader/pan/width stage** (post-everything master sum, where the LUFS node was specced to attach per the Task 1 code map) **before the peak publish**. New `VibeGraph::getMasterLufs(mode 0/1/2)` + `resetMasterLufsIntegrated()` read/reset the node's `mLufs` — no header include needed (the struct is .cpp-defined). `PluginProcessor::getMasterLufs` passthrough. `CMakeLists.txt` += `Source/DSP/LufsMeterDSP.cpp`.
- **TRANSPORT RESET — lives in `PluginProcessor::processBlock`, NOT a graph node:** there is NO transport/posInfo plumbed into the graph nodes (they only get bpm), so the Integrated reset-detect uses the `pos` (`AudioPlayHead::PositionInfo`) already read in `processBlock`. New plain audio-thread members `mLufsWasPlaying`/`mLufsLastPpq`; on **stopped→playing OR a backward ppq jump while playing** (loop wrap / relocate-to-start) it calls `mVibeGraph.resetMasterLufsIntegrated()` — done BEFORE the graph runs so the block opens a fresh Integrated window. Uses the codebase's standard `pos.getPpqPosition().orFallback(0.0)` idiom (matches the existing reads; static-verified).
- **UI — new `LufsReadoutBox` in SharedUI.h/.cpp** (`juce::Component` + `SettableTooltipClient`). **KEY ARCHITECTURE NOTE — PUSH not poll:** `MixerTrackStrip` holds NO processor ref (only `setApvts`), so the plan's `LufsReadoutBox { mProcessor }` poll model was replaced with a PUSH model matching the existing meter drain — `MixerPage::onVBlank` feeds all 3 values via `mMasterStrip->setMasterLufs(getMasterLufs(0/1/2))`, the box displays the selected mode. Box visuals: dark recessed panel; value (selected mode) in 11px mono + a right tag column showing the mode letter (M/S/I) over a down-caret; click anywhere → `PopupMenu` (Momentary / Short Term / Integrated) → persists the mode to settings.xml via the `PatternColorPicker` preserve-other-sections idiom (`<MasterLufsMode mode="N"/>` under the root, `ProjectManager::getSettingsFile()`). Full mode name + LUFS value in the tooltip.
- **LAYOUT RESOLVED (Jeff 2026-05-30, before build):** the box layout was surfaced as a choice — compact (value + M/S/I letter + caret) vs spec #1's stacked value-over-title. Jeff chose **(b) stacked, "the thing I actually asked for"** — there's room in the ~44 px column both horizontally and vertically. Rebuilt before any build: value on top (13 px mono) + a small down-caret on the value row + the **full mode title underneath** ("Momentary" / "Short Term" / "Integrated", `drawFittedText` so it never clips); box height bumped `kLufsH` 20 → **30** (revises #9's ~18-20 px single-line note — superseded by Jeff's explicit stacked request). The compact-letter draft + the now-unused `modeAbbrev` helper were removed.
- **LAYOUT — `MixerTrackStrip`:** `LufsReadoutBox mLufsBox` member (master-only `addAndMakeVisible` in ctor); positioned in `resized()` between the width knob and the fader (`kLufsH=20`, masterRow only — matching #9's ~18-20 px and the Task 1 code map's "NEW inserted row between width knob + fader, push fader down"); the fader shifts down (thumb may overlap above unity — acceptable per Jeff's 2026-05-29 decision). `setMasterLufs` forwarder.

**Static cross-checks done (pre-build):** all Task 3 symbols (`getMasterLufs` / `resetMasterLufsIntegrated` / `mLufs` / `LufsReadoutBox` / `setMasterLufs` / `mLufsBox`) resolve across the files; the `orFallback` idiom matches the codebase; the histogram constants block precedes the array members in the header.

**Gitignore:** added `audio_settings_pending.xml` (runtime artifact) per Jeff; folds into the Task 3 commit.

**Possible build nits to watch (likely harmless):** `juce::Font(...)` ctors emit C4996 (harmless per CLAUDE.md); `LufsReadoutBox` is constructed for EVERY strip (reads settings.xml in ctor) though only shown on master — cheap, one-time at mixer build.

**Still pre-build; awaiting Jeff's Debug-then-Release verify:** master shows the LUFS box between width knob + fader; sane values for a loud master (~-10..-14); M lively / S steady / I climbs + resets on play-from-top/loop; mode selectable via the dropdown + persists across restart; no regression on the split meters or peak readings.

---

## 2026-05-30 — Task 3 — VERIFIED (Debug + Release)

**Jeff verified Task 3 in Debug + Release 2026-05-30: "We're good."** The master LUFS readout (stacked value-over-title box with the M/S/I dropdown) works: sane values for a loud master, the three modes behave (Momentary lively / Short-Term steady / Integrated accumulates + resets on play-from-top/loop), the selected mode persists across restart, and no regression on the Task 2 split meters or the peak bars. K-weighting math confirmed correct in practice (sane LUFS = the acceptance proxy for the 48 kHz coefficient table). Task 3 = DONE; ready to commit (DSP + audio wiring + LufsReadoutBox UI + the `.gitignore` line). Next: Task 4 (project-lifecycle dedup fix) → Task 5 (bus collapse UI) → Task 6 (close).

Task 3 committed at `63be14d` (14 files, +526).

---

## 2026-05-30 — Task 4 — Project-lifecycle dedup fix: diagnosis + one-line fix (pre-build)

**Root cause (static diagnosis, conclusive):** the false "File Already in Library" prompt on a fresh File > New is a missing reset, not a logic bug in the dedup itself. The dedup is `PatternManager::findAudioLibraryIndexByPath` (exact path match over `std::vector<AudioLibraryEntry> mAudioLibrary`, PatternManager.cpp:199); the disk-drop handler (`grid->onDuplicateFileDrop`, StandaloneEditor.cpp:2355) fires the prompt when that lookup hits. The audio library is cleared in exactly ONE place — `PatternManager::fromValueTree` (the project-LOAD path, :1139) — but **NOT in `PatternManager::reset()`** (:793), which clears `mPatterns` / `mArrangement` / `mMixer` / drum flags / `mAutomationTemplates` but omitted `mAudioLibrary`. `reset()` is the canonical "wipe to blank project" call, invoked ONLY via `VibeSynthProcessor::resetToBlankState()` (PluginProcessor.cpp:3181), which is itself the wipe path for all 4 File > New / New-from-template entry points (StandaloneEditor.cpp:8621/9040/9192/9344) + `mVibeGraph.clearAllRackStates()` + APVTS-defaults. So after File > New, `mAudioLibrary` still held the prior project's entries → a previously-used sample matched → false prompt. Textbook QA-D STATE-* family (New-Project-doesn't-reset-X); `reset()` was simply incomplete vs `fromValueTree`.

**Fix (one line):** added `mAudioLibrary.clear();` to `PatternManager::reset()` (PatternManager.cpp:~800), mirroring the clear `fromValueTree` already does. Well-scoped: `reset()` only runs on blank-reset, so in-project dedup is unaffected — entries still accumulate via `addAudioLibraryEntry` within a live project, and dropping a true duplicate still matches `findAudioLibraryIndexByPath` and prompts correctly. No spec call (one correct minimal fix; the plan's "reset the library index on New Project" option).

**Diagnosis method:** static code read only (grep the prompt string → trace the dedup lookup → compare the clear sites in `reset()` vs `fromValueTree` → confirm `reset()`'s single caller chain). Not an audio bug, so no runtime A/B needed — the root cause is unambiguous in the source.

**Files:** PatternManager.cpp (1 line + comment). **Pre-build; awaiting Jeff's Debug-then-Release verify:** (1) open a project with samples → File > New → drop a previously-used file → NO false "already in library" prompt (normal import). (2) Drop a true duplicate WITHIN one project → the prompt STILL fires (Use Existing / New Page / Cancel).

**VERIFIED (Jeff, Debug + Release 2026-05-30):** "Fixed and the real use case still works fine." Both scenarios pass — the false prompt on File > New is gone, and legitimate in-project duplicate detection still fires. Task 4 = DONE; ready to commit. Next: Task 5 (bus collapse/expand UI) → Task 6 (close).

Task 4 committed at `dc965ef` (2 files, +27).

---

## 2026-05-30 — Task 5 — Bus collapse/expand UI: architecture + integration map (pre-implementation)

**Persistence decision (my call — invisible impl detail):** per-bus collapse state = a new **APVTS `_collapsed` bool** registered on Bus strips in `addParamsForMixerStrip` (PluginProcessor.cpp:~3921, the Bus block). Chosen over a project-XML `<MixerView>` element because the APVTS state tree already serializes with the project (so it "persists through save" + restores on load with zero new serialization plumbing), matching the existing `_mute`/`_solo`/`_polarity` per-strip pattern. It's UI-only (no audio path reads it) — a slight category bend (a view flag in the param tree) accepted for the lean auto-persist.

**Integration map (~15 sites per `reference_mixer_strip_pattern_audit.md`):**
- **PluginProcessor** `addParamsForMixerStrip`: + `_collapsed` bool, `kind==Bus` only.
- **MixerTrackStrip** (.h/.cpp): a collapse arrow button (Bus strips only) in the top row right of a slightly-shrunk name label, RibbonTabBar tab-arrow style, **down=expanded / up=collapsed**, greyed/disabled when the bus has no members; `onCollapseToggled(channelId)` callback; `setCollapsed(bool)` + `setCollapseEnabled(bool)`; **tooltip-on-ALL-strips** (full name + "Double-click to rename", refreshed in ctor + `onTextChange` + `setTrackName`); resized() arrow placement on Bus strips.
- **MixerPage** (.cpp): wire each bus strip's `onCollapseToggled` → flip the bus's `_collapsed` param + `setCollapsed` + relayout (re-run the strip layout fn `:3490-3592` + `syncHScrollBar`); in `laidOutBus` (`:3529`) read the bus's `_collapsed` param → if collapsed, skip `layoutGroup` for its members (`setVisible(false)`) + don't advance `x`; set each bus's collapsed + arrow-enabled (has-members = `buckets[busChId]` non-empty) at layout time so it restores on load. MUST cover the on-demand Vox Bus 2 / Inst Bus 2 / Inst Bus 3 (`:1842/1906/1968`) too.
- Master gets NO arrow (Jeff #1).

Implementing now in this order: param → MixerTrackStrip → MixerPage. Pre-build entry to follow once it compiles-clean statically.

---

## 2026-05-30 — Task 5 — Bus collapse/expand UI implemented (pre-build)

**Implemented per the locked spec + the architecture above.** 6 files; statically cross-checked (all symbols resolve; `prefixFromChannelId` confirmed to return the exact registered bus prefixes so `prefix + "_collapsed"` looks up correctly).

- **Param (PluginProcessor.cpp:~3946):** `addB(prefix + "_collapsed", ...)` gated to `kind==MixerStripKind::Bus` in `addParamsForMixerStrip` — registered for all 11 buses at startup via `ensureMixerBusAndMasterParams` (incl. voxbus2 / instbus2 / instbus3). UI-only; persists with the project's APVTS state.
- **MixerTrackStrip:** new `MixerCollapseArrow` button class (paints a down triangle = expanded / up = collapsed; greyed when `!isEnabled()`; subtle hover). Members/API: `mCollapseBtn`, `onCollapseToggled(channelId)`, `setCollapsed(bool)`, `setCollapseEnabled(bool)`, `refreshNameTooltip()`. Ctor adds the arrow + click handler ONLY for `StripType::Bus` (fires `onCollapseToggled(mChannelId)` — channelId is set post-ctor, read at click time). `resized()` reserves 14 px on the right of the name row for the arrow (Bus only) so the name shrinks. **Tooltip-on-ALL-strips:** `refreshNameTooltip` sets the label tooltip to the full name (+ "Double-click to rename" where editable), called from ctor + `onTextChange` + `setTrackName` + `setRenameable` (replaces the old rename-only-tooltip).
- **MixerPage:** `isBusCollapsed(chId)` reads the `_collapsed` raw param; `onBusCollapseToggled(chId)` flips it (`setValueNotifyingHost`) + calls `layoutScrollContent()`. In `layoutScrollContent`, the `laidOutBus` lambda (10 buses incl. on-demand Vox2/Inst2/Inst3) + the inline FX-bus block both: wire `onCollapseToggled`, set `setCollapseEnabled(hasMembers)` + `setCollapsed(collapsed)`, and when collapsed hide the bus's member strips (`setVisible(false)`) + DON'T advance `x` (gap closes); when expanded `setVisible(true)` + `layoutGroup`. Master has no arrow (not a `laidOutBus` bus). Restore-on-load is implicit: the param is restored by `setStateInformation`, and `layoutScrollContent` re-reads it whenever the mixer relays out (insert strips are recreated on load → relayout).
- **FX bus nuance:** collapses its own `kFxBus` member group only; the aux-to-aux main-out chains (separate aux groups visually in the FX family) are left always-laid-out.

**Possible build nits (likely harmless):** `juce::Font`/`juce::Button` usage is all standard; `mNameLabel.isEditableOnDoubleClick()` is a real juce::Label getter; `BuilderPage`'s unrelated `mCollapseBtn`/`setCollapsed` is a different class (no collision).

**Still pre-build; awaiting Jeff's Debug-then-Release verify:** (1) every bus strip shows a small down-arrow right of its name; click → flips up + that bus's strips hide + the row closes the gap; click again → they return. (2) Master has NO arrow. (3) a bus with no member strips shows the arrow greyed/disabled. (4) hover any strip's name → tooltip shows the full name (+ rename hint where editable); long truncated names read fully in the tooltip. (5) collapse some buses, save + reload the project → collapsed state restored. (6) no audio change — a collapsed bus's hidden strips still play + meter.

**VERIFIED (Jeff, Debug + Release 2026-05-30): "All pass."** All 6 scenarios confirmed — arrow collapse/expand + gap-close, Master arrow-less, empty-bus arrow greyed, full-name tooltip on all strips, collapse state persists through save+reload, and no audio change on hidden strips. Task 5 = DONE; ready to commit. This was the LAST build task — only Task 6 (close) remains.

Task 5 committed at `db423b5` (6 files, +189/-9).

---

## 2026-05-30 — Task 6 — CLOSE: Rule 4 clean + /review-batch (1 NEEDS-FIX fixed)

**Rule 4 (diagnostic instrumentation):** NONE added across the batch (grep of the batch source diff for DBG/Logger/writeToLog/`[QA-RustyMeter` = empty). No Diagnostic Instrumentation Catalog needed; nothing to strip.

**`/draft-doc batch-close`:** compiled the full QA-RustyMeter Implemented Work Log entry (Bucket: Mixer/Routing, UI/L&F/Theming, Cross-cutting Infrastructure, Effects, Players; full Task 0-5 arc; FND-1..FND-5 routing table). Held for apply at the close commit (after the review fix below + the T-f Future State route).

**`/review-batch QA-RustyMeter`:** **no BLOCKERS.** The reviewer confirmed the load-bearing safety paths: K-weighting JUCE a-sign convention correct (passes natural a1/a2; JUCE subtracts internally), alloc-free Integrated histogram, null-coefficient lifecycle (prepare before process), audio→UI atomic thread-safety (CAS-max audio / exchange-reset UI, -inf clamped before EMA), and the -70/-10 gate math. 
- **NEEDS-FIX (fixed in-batch):** the RMS history ring (`mRmsHistL/R`) value-initialized to `0.0f`, but `dbToNorm(0 dB)` ~= 0.925 deflection, so on a Split meter the not-yet-written slots painted a misleading near-full-width "loud" band for the first ~4 s (ring fill) on every non-master strip at launch. Jeff's verifies missed it because he tested with audio playing (ring fills with real values fast), not staring at a silent meter at launch. Verified the finding against the code (ring is 0.0f-init while mRmsIn/Disp are kFloor-init), then fixed: `mRmsHistL.fill(kFloor); mRmsHistR.fill(kFloor);` in the `DBFSMeter` ctor so unfilled slots render flat/silent. **VERIFIED good by Jeff 2026-05-30** ("The fix is good").

**FND-6 (Jeff noticed during the RMS-fix verify) — peak (dBFS) bars flash full then drop on first load.** Investigated: NOT the same class as the RMS-ring bug. The peak meter's own state is correctly floor-initialized (`mLevelDbL/R` = −inf at `SharedUI.h:1716`; `mDisplayDbL/R`/`mPeakDbL/R` = −60 at `:1723-1724`; `onVBlank` clamps −inf→floor) — so the bar can only show full if the audio-driven publish path (`publishPeakReading` → node atom → mirror → `setStereoLevel`) genuinely fed it a high peak. I.e. the meter is honestly catching a brief real peak on load (engine/graph spin-up transient or an un-cleared first-block buffer), held ~1 s by the peak-hold, then decaying = "full then drop". **Pre-existing** (the peak path + its publish predate QA-RustyMeter; this batch added the RMS waveform + LUFS box, not the peak bar) — surfaced only because the close verify had Jeff watch a silent fresh launch. Jeff's read: "seems like a load transient"; he can't easily A/B the blank-project case since the Mixer isn't the page that loads. **Decision (Jeff 2026-05-30): move forward + commit** — out-of-scope for QA-RustyMeter (pre-existing, not the RMS/LUFS this batch touched), not chasing it in this batch. Routed to Future State as a candidate follow-up (investigate the on-load peak transient / first-block buffer clear) at this close.
- **NITs (no action / record-only):** (1) `LufsReadoutBox` is a value member on every strip though only shown on Master — harmless, matches the existing `mType`-gated member pattern; deferred. (2) spec #10 locked 50/50 but shipped **35/65** (Jeff-approved at part-1 verify) — not a drift; the Work Log close entry records 35/65 as the shipped value so plan + log don't contradict.

**Remaining close steps:** Jeff verifies the RMS-ring fix → commit the fix → apply the Work Log entry + route T-f (true-peak / Integrated LRA / per-strip LUFS) to Future State (slot = Jeff's call) + Main Plan §5 STATUS close-banner + §9 close route entry → close commit.








