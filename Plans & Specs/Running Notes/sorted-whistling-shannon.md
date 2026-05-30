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

**Jeff verified 2026-05-30: "this all looks good"** — the 65/35 split + filled gradient waveform + dynamics-tracking wave are all approved. Part 1 (inserts) visual + functional = DONE.

## 2026-05-30 — Task 2 — OUT-OF-SCOPE bug surfaced during testing: false "File Already in Library" prompt on a NEW empty project

Jeff (while testing the meter): opened a project containing multiple copies of his song → confusing → File > New (fresh empty project, no samples in the sample folder) → dropped the same `.mp3` → got the **"File Already in Library — already in your library on 'an existing page'"** prompt (Use Existing / New Page / Cancel) despite the new project being empty. The per-project dedup check is consulting STALE state — the previously-open project's page/library index NOT cleared on File > New (or a global library index that should be per-project). Same family as the QA-D STATE-* project-lifecycle reset bugs (New-Project-doesn't-reset-X). **UNRELATED to metering.** Routing surfaced to Jeff (fix-now vs own batch) per Main Plan §0 Rule 3 + `feedback_qa_batches_fix_bugs_dont_defer.md`. Captured here so it is not lost regardless of routing.

**Resolution (Jeff 2026-05-30): fix IN this batch as END-BATCH CLEANUP** (after the metering tasks; do not stop the current work). Added to the plan as **Task 4 — Project-lifecycle dedup fix**; Close renumbered to Task 5. In-batch resolution → recorded in the close-entry routing table at batch close (NOT a §9 cross-batch route).


**Possible build nits flagged to Jeff:** `<utility>` in VibeGraph.h (std::pair) + `<cmath>` in VibeGraph.cpp (std::sqrt) — both almost certainly already pulled via JUCE; easy add if the build complains.

**Files:** SharedUI.h/.cpp, VibeGraph.h/.cpp, PluginProcessor.h/.cpp, MixerPage.cpp, MixerTrackStrip.h/.cpp. Pre-build; awaiting Jeff's Debug+Release verify.








