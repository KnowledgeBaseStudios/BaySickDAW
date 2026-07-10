# QA-F — BaySickAlign Build-Out + Vox Pitch Quality Pass + Shared Composite/Shifter Foundation — Plan (crooning-warping-lynx)

> **Canonical path** (mirrored after group approval):
> `Plans & Specs/Batch Plans/crooning-warping-lynx.md`
> Paired running notes: `Plans & Specs/Running Notes/crooning-warping-lynx.md`

> **For execution (BULK-RUN mode — see [`Batch Plans/swift-stampeding-caribou.md`](swift-stampeding-caribou.md)):** code ALL tasks with NO per-task verify pause. Jeff runs `do_build.bat` (Release+Debug) after all tasks land; fix to clean. The **Verify scenarios** below author into Master Test Plan **§B.6** (physically executable, derived from actual code) — they are NOT inline execution gates. **ONE source commit** for the whole batch (Rule 9; G1's one-commit-per-batch model, TempoMap precedent). Work Log close entry drafted + HELD in running notes; applied at §B.6 section pass (R2). The **after-QA-F ear-check** (align/warp + realtime-pitch quality) is the one Jeff gate in-batch.

## Context

QA-F is the keystone of G2. It builds three things:
1. The shared **channel-composite renderer** + **pitch-shifter classes** that QA-Fa and QA-Fb′ consume (Call 1a — built here as first consumer; order F→Fa→Fb′→Fc).
2. The full **BaySickAlign** offline editor + DSP (currently a paint-only VocAlign-clone shell).
3. The **realtime Vox pitch-correction quality pass** (Call 2a).

**Current state (verified 2026-07-09, 3 Explore agents):**
- BaySickAlign/BaySickPitch are NOT separate engines — they are sub-tab editors in `Source/BaySickVocal/`, built by `BaySickVocalEditor.cpp:498-499`. All DSP + params live on the single `BaySickVocalProcessor` (40 params today; **zero** Align/Pitch params). `BaySickAlignDSP` lives in `Source/DSP/`.
- `BaySickAlignDSP` is **never instantiated**; `analyzeOffline` is real (FFT spectral-flux onset → `WarpMap`) but never called; `applyWarp` is a memcpy passthrough ([BaySickAlignDSP.cpp:257-266](Source/DSP/BaySickAlignDSP.cpp:257)). `BaySickAlignEditor` has **zero** APVTS attachments (fully cosmetic; Capture/Render disabled).
- The realtime Vox FX pipeline + YIN path are LIVE and correct. Pitch correction **defaults OFF** (`bsv_pitch_realtime_bypass`=true) and is bypassed during FilePlay. The "robotic"/"nothing" reports = the formant no-op + crude 2-grain shifter + on-key notes (nothing to correct), NOT a broken tracker (Call 2 premise correction). Formant Preserve + Throat Shift are no-op stubs (`juce::ignoreUnused`, [PitchCorrectorDSP.cpp:327](Source/DSP/PitchCorrectorDSP.cpp:327)). **All pitch params already exist** (`bsv_pitch_formantPreserve`, `_throatShift`, `_retuneSpeed`, `_strength`, `_key`, `_scale`, `_humanize`, `_realtime_bypass`).
- **No channel-composite renderer for analysis exists.** Reference walk: `VibeSynthProcessor::renderAudioClipsForRow` ([PluginProcessor.cpp:521](Source/PluginProcessor.cpp:521)) is realtime/per-row and skips FilePlay — the offline whole-channel analysis renderer is net-new.
- **Only `PhaseVocoder` is a reusable standalone shifter** ([PhaseVocoder.h:27](Source/DSP/PhaseVocoder.h:27)). PSOLA (`OctaveStyleDSP::PeriodDoubler`, octave-only) + Granular (`OctaveStyleDSP::GranularShifter`, `PitchCorrectorDSP::Shifter`) are nested/private — need extraction + generalization to arbitrary ratio.

**Risk:** VERY HIGH — largest batch in the program (audio-thread DSP + full editor rebuild + shared foundation + realtime quality pass, one commit, no per-task verify). Mitigation: the after-QA-F ear-check + §B.6; Tasks 1-2 are independently exercised by QA-Fa/Fb′ downstream.
**Effort:** very large (~18-26h). **Dependencies:** none blocking. **Consumers:** QA-Fa + QA-Fb′ depend on Tasks 1-2.
**Bucket:** Players, Effects, Cross-cutting Infrastructure.

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| G2-1 | Composite renderer + shifters built HERE (Tasks 1-2) as first consumer; QA-Fa + QA-Fb′ consume. Order F→Fa→Fb′→Fc. **ONE commit** for the whole batch. | Call 1a (Jeff 2026-07-09). §7 "lands in first consumer." Commit model = G1's one-per-batch; TempoMap (`753dddc7`, 4 tasks + new `TempoMapRead.h`) is the large-batch-one-commit precedent. |
| G2-2 | Full realtime-pitch quality pass: build formant-preserve cepstral DSP + wire Throat Shift + swap live shifter to **LOW-LATENCY** (PSOLA/improved granular, NOT phase vocoder) + tune retune-speed/strength. "Nothing happening" = on-key (expected; verify-not-fix). | Call 2a + latency refinement (Jeff 2026-07-09): a phase vocoder adds ~40ms and confuses learners monitoring themselves live. PV reserved for offline bake. All pitch params already exist — **no new params**. |
| G2-3 | Build all three shifters: extract+generalize PSOLA + Granular for arbitrary ratio; PhaseVocoder reused. Back the Algos dropdown (offline) + the live low-latency path. | Call 3a. No-stubs gate forbids a 3-option dropdown with 2 stubs. |
| G2-13 | BaySickAlign per-control redesign per [`Running Notes/phantom-recording-mongoose.md`](../Running Notes/phantom-recording-mongoose.md) §13a-§13g (locked 2026-05-14). ~20 APVTS params. Lanes Leader=Bass-green / Follower=Vox-teal / Output=Drums-red. | Full redesign locked; breaks VocAlign trade-dress (visual + control-naming legs) while keeping engine name + universal keybinds. |
| G2-warp | Channel-composite-driven **single WarpMap per channel pair** (time-warp + per-anchor pitch delta); offline `analyzeOffline`; render-to-bake `<project>/Aligned/{name}_align_v{N}.wav`; stale-on-grid-change → manual re-analyze (no auto). | §5 design lock. |
| G2-Q4 | DSP-04 (drag-drop import) DEAD — not in QA-F/QA-Fa. | Call 4a. |

## Sub-spec calls surfaced for approval
**None open.** All four G2 calls + the live-shifter latency refinement were resolved in chat (2026-07-09) before this plan body. Any call discovered mid-execution stops that piece and surfaces to Jeff (bulk-run ask-always lock).

## Files to modify

### Task 1 — Channel-composite renderer (shared)
- [Source/PluginProcessor.h](Source/PluginProcessor.h) / [.cpp](Source/PluginProcessor.cpp) — new `juce::AudioBuffer<float> renderChannelComposite(int channelId, double& outStartBeat) const` (offline, message-thread): walk every arrangement clip routed to `channelId` (INCLUDING FilePlay/recorded takes, unlike `renderAudioClipsForRow`), decode each at its grid position, sum into one mono buffer spanning the channel's clip extent. Factor the decode/position math shared with `renderAudioClipsForRow` (:521) + `decodeFilePlayClip` (:993) into a reusable helper; do not duplicate.
- Consumers: BaySickAlign (guide+dub composites), BaySickPitch (QA-Fa), verified in QA-Fb′.

### Task 2 — Pitch-shifter extraction (shared)
- New [Source/DSP/PitchShifters.h](Source/DSP/PitchShifters.h) (+ `.cpp` if needed) — three standalone classes with a common `float processSample(float in, float ratio)` + `prepare(sr, maxBlock)` interface:
  - `PsolaShifter` — generalize `OctaveStyleDSP::PeriodDoubler` ([OctaveStyleDSP.cpp:109-165](Source/DSP/OctaveStyleDSP.cpp:109)) from octave-only to arbitrary ratio (epoch/period detection + pitch-synchronous OLA). Low-latency (few pitch periods) — the LIVE-path default.
  - `GranularShifter` — generalize `OctaveStyleDSP::GranularShifter` ([OctaveStyleDSP.cpp:19-107](Source/DSP/OctaveStyleDSP.cpp:19)) / the 2-grain `PitchCorrectorDSP::Shifter` to arbitrary ratio with more grains + a Hann window. Low-latency alternative.
  - `PvShifter` — thin wrapper over the existing [PhaseVocoder](Source/DSP/PhaseVocoder.h:27) doing time-stretch→resample = pitch shift. OFFLINE-only (FFT latency).
- **Ring-read pointers MUST wrap into [0,ringSize) each sample** (memory `reference_wrap_ring_read_pointers` — granular/PSOLA read heads climb DSP% with play-time otherwise).

### Task 3 — BaySickAlignDSP wiring + warp/pitch engine
- [Source/BaySickVocal/BaySickVocalProcessor.h](Source/BaySickVocal/BaySickVocalProcessor.h) / [.cpp](Source/BaySickVocal/BaySickVocalProcessor.cpp) — add `BaySickAlignDSP mAlign;` member; register ~20 `bsa_*` APVTS params in `createLayout` ([:25-116](Source/BaySickVocal/BaySickVocalProcessor.cpp:25)); `updateFromApvts` push (isIdentity + dirty-flag pattern, memory `feedback_apvts_dirty_flag_pattern`); persist WarpMap + sync points + protected areas + render history under the channel-pair state (replaces the empty `<AlignEdits>` placeholder at [:399/:424](Source/BaySickVocal/BaySickVocalProcessor.cpp:399)).
- [Source/DSP/BaySickAlignDSP.h](Source/DSP/BaySickAlignDSP.h) / [.cpp](Source/DSP/BaySickAlignDSP.cpp) — extend `WarpMap` with per-anchor semitone delta; keep `analyzeOffline` (:271) + add YIN F0 delta per anchor (both composites); **replace the `applyWarp` memcpy stub (:257-266)** with real PhaseVocoder time-stretch (warp ratio) + pitch-shift (anchor semitones) in one pass; add render-to-bake (`Aligned/{name}_align_v{N}.wav`, mirrors the Option C comment in `BaySickAlignDSP.h`).

### Task 4 — BaySickAlignEditor full redesign (§13a-§13g)
- [Source/BaySickVocal/BaySickAlignEditor.h](Source/BaySickVocal/BaySickAlignEditor.h) / [.cpp](Source/BaySickVocal/BaySickAlignEditor.cpp) — full rewrite per §13. 3 lanes (Leader green / Follower teal / Output red, shared ruler), toolbar (6 presets Loose/Close/Tight ×±Pitch, Save/Load, preset-dirty green dot, Undo/Redo), Mode dropdown → drives Fine Tune base + Range center/travel, Sync Points (place/drag/delete/auto-seed) + Protected Areas (drag-create, right-click dimensions), ViewMode (Wave/Pitch/Energy render branches), HistoryScrubber (render list paint), right panel (Align box: ON/Mode/Fine Tune; Pitch box: ON/Range/Algos/Transpose/Formant Shift), composite waveform rendering via Task 1. **Every control APVTS-attached — no paint-only widgets.** Delete the `"VocAlign-clone"` comment ([:8](Source/BaySickVocal/BaySickAlignEditor.cpp:8)).
- Palette hexes from `VibeLAF` constants (Bass green / Vox teal / Drums red / Effects purple) at implement time.

### Task 5 — Realtime Vox pitch quality pass (Call 2a)
- [Source/DSP/PitchCorrectorDSP.h](Source/DSP/PitchCorrectorDSP.h) / [.cpp](Source/DSP/PitchCorrectorDSP.cpp) — build **formant-preserve** (cepstral spectral-envelope extract → shift excitation → re-impose envelope), replacing the `ignoreUnused(mFormantPreserve, mThroatSemis)` no-op ([:327](Source/DSP/PitchCorrectorDSP.cpp:327)); wire **Throat Shift** (deliberate formant shift by `mThroatSemis`, same envelope machinery); **swap the internal 2-grain `Shifter` ([:53-114](Source/DSP/PitchCorrectorDSP.cpp:53)) for the low-latency `PsolaShifter`/`GranularShifter` from Task 2**; tune `mCurrentShiftRatio` smoothing ([:306](Source/DSP/PitchCorrectorDSP.cpp:306)) + `retuneSpeed`/`strength` defaults so correction is musical (not over-snapped) at default settings. Leave `bsv_pitch_realtime_bypass` default unchanged (OFF) — no unprompted behavior change.

## Tasks

### Task 1 — Channel-composite renderer (shared foundation)
- [ ] Read `renderAudioClipsForRow` ([PluginProcessor.cpp:521](Source/PluginProcessor.cpp:521)) + `decodeFilePlayClip` (:993) to reuse the decode/position math.
- [ ] Add `renderChannelComposite(int channelId, double& outStartBeat)` — offline mono sum of ALL clips on the channel (FilePlay included) at grid positions; message-thread only (document thread-ownership per Rule 6).
- [ ] Unit-sanity: a single 1-bar clip on an empty channel → composite == that clip's samples at its start beat.

### Task 2 — Pitch-shifter extraction (shared)
- [ ] Create `Source/DSP/PitchShifters.h` with `PsolaShifter` / `GranularShifter` / `PvShifter` (common `prepare` + `processSample(in, ratio)`).
- [ ] Generalize PSOLA + Granular to arbitrary ratio (from OctaveStyleDSP + PitchCorrectorDSP::Shifter). Wrap all ring read pointers each sample.
- [ ] `PvShifter` wraps `PhaseVocoder` (offline). Mark FFT latency in a thread/latency comment.

### Task 3 — BaySickAlignDSP wiring + warp/pitch engine
- [ ] Add `mAlign` member on `BaySickVocalProcessor`; register ~20 `bsa_*` params; `updateFromApvts` with isIdentity + dirty flag.
- [ ] Extend `WarpMap` (per-anchor semitones); wire `analyzeOffline(guideComposite, dubComposite)` + YIN F0 delta per anchor.
- [ ] Replace `applyWarp` memcpy with PhaseVocoder time-stretch + pitch-shift; add render-to-bake `Aligned/{name}_align_v{N}.wav` + render history.
- [ ] Persist WarpMap / sync points / protected areas / history in project XML (channel-pair state).

### Task 4 — BaySickAlignEditor full redesign (§13a-§13g)
- [ ] Rebuild toolbar (6 presets, Save/Load, dirty dot, Undo/Redo); Mode dropdown drives Fine Tune + Range.
- [ ] 3 lanes (green/teal/red, shared ruler) with composite waveform rendering (Task 1).
- [ ] Sync Points (place/drag/delete/auto-seed) + Protected Areas (drag/right-click dimensions).
- [ ] ViewMode Wave/Pitch/Energy render branches; HistoryScrubber list paint.
- [ ] Right panel Align + Pitch boxes; **every control APVTS-attached**. Remove VocAlign-clone comment.
- [ ] **Brand-safety (semantic sweep, not keyword — the UREI lesson):** scrub ALL BaySickVocal user-facing strings (tooltips/labels/menus/dialogs) of real product/brand names — the `:8` clone comment, every VocAlign-flavored control name (MATCH PITCH / MAX DIFFERENCE / TARGET MODE / PITCH TARGET / SMART PITCH / ALGORITHM / ALIGNMENT RULE), AND the 2 `Auto-Tune` tooltips ([BaySickVocalEditor.cpp:189/:252](Source/BaySickVocal/BaySickVocalEditor.cpp:189)) → brand-safe generics. **KEEP by design (§12):** the engine name BaySickAlign (ours) + universal-DAW keybinds — breaking the visual + control-naming legs is sufficient to neutralize the trade-dress bundle.

### Task 5 — Realtime Vox pitch quality pass (Call 2a)
- [ ] Build formant-preserve (cepstral envelope) replacing the no-op; wire Throat Shift on the same machinery.
- [ ] Swap the 2-grain `Shifter` for the low-latency `PsolaShifter`/`GranularShifter` (Task 2) in the live path.
- [ ] Tune retune-speed/strength defaults + snap smoothing so it's musical at defaults, not robotic.

### Batch close (one commit)
- [ ] Jeff runs `do_build.bat`; fix Release+Debug to clean.
- [ ] Author Master Test Plan **§B.6** from the Verify scenarios below.
- [ ] Draft + HOLD the Work Log close entry under `## Held Work Log entry (apply at section pass)` in the running notes.
- [ ] Append running-notes code-complete entry + Rule 4 diagnostic catalog rows (same edit pass as any diagnostic code).
- [ ] Surface commit message + FULL git status → Jeff approval → **one** commit: `QA-F Tasks 1-5: composite renderer + shifter extraction + BaySickAlign editor/DSP build + realtime Vox pitch quality pass (BaySickVocal, DSP, test plan B.6 + running notes)`.

## Verify scenarios (→ Master Test Plan §B.6; `blocks:` QA-F commit)
Physically executable, derived from the code above. Debug-first, Release-confirm.
1. **Composite renderer** — put two audio clips on a Vox channel at bars 1 and 5; open BaySickAlign; the Follower lane's composite waveform shows both clips at their grid positions (no gaps/overlap errors).
2. **Shifter A/B (offline)** — on a sustained vocal clip, render Align with Algos=PSOLA vs Granular vs Phase Vocoder; each produces audibly shifted, artifact-bounded output (no dropouts).
3. **Align workflow** — pick Leader + Follower channels, Analyze; Output lane preview shows time-aligned Follower; play → Follower rides Leader's timing. Close preset (Close-Align) tightens vs Loose.
4. **Pitch matching (+Pitch preset)** — Close-Align+Pitch pulls a flat Follower note toward the Leader without erasing all human variation; Tight fully maps the contour.
5. **Render-to-bake** — Render writes `Aligned/{name}_align_v1.wav`; a second render → `_v2`; HistoryScrubber lists both with dates; reload plays the baked file.
6. **Stale handling** — move a Follower clip → warp marked stale + "re-analyze" shown (not auto-run).
7. **Presets + persistence** — Save a user Align preset → reload restores all Align+Pitch params + dirty dot clears; save project → reopen → WarpMap + sync points + protected areas restored.
8. **Sync points / protected areas** — place a sync point (auto-pairing respects it as a boundary); draw a protected area, right-click to protect pitch-only → that region keeps pitch on re-render.
9. **[EAR-CHECK] Realtime pitch quality** — on live/monitored vocal with realtime pitch ON at DEFAULT settings: correction is musical, not chipmunk/robotic (formant-preserve working); **no audible monitoring latency** (low-latency shifter); Formant Preserve toggle A/B audibly changes timbre naturalness; on-key notes pass through unaltered (expected). Throat Shift ±semis shifts character without re-pitching.
10. **Brand-safety** — no real product/brand name appears in any BaySickVocal / Align tooltip, label, menu, dialog, or source comment (visual pass); engine name BaySickAlign + universal keybinds retained by design.

## Routing notes (Rule 3)
- Findings touching not-yet-started G2 batches (Fa/Fb′/Fc surfaces) → fold into that batch's scope + note in running notes; §9 routing at §B.6 section pass.
- The **VocAlign brand-safety** sweep (2 tooltips + the `:8` comment + control renames) is folded into Task 4; §9 back-ref to QA-EffectsReview rides section-pass routing.
- QA-J overlap fork: verify scenarios use SEQUENTIAL same-row clips only; overlapping-same-row → campaign QA-J-Verify (§C ledger item 2).

## Carry-Forward Reference touch points
- Read §4 (audio-engine / MT render path) before Task 3/5 (BaySickVocalProcessor runs inside VoxStripTask.processBlock — MT-orthogonal at the engine level).
- Read §6 (lifecycle / mShuttingDown) before Task 4 editor teardown.
