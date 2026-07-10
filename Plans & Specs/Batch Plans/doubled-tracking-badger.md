# QA-Fb′ — Recording Lifecycle: Dual-Buffer WET-Bleed Gate + Conditional-WET + Dirty-Flag — Plan (doubled-tracking-badger)

> **Canonical path** (mirrored after group approval):
> `Plans & Specs/Batch Plans/doubled-tracking-badger.md`
> Paired running notes: `Plans & Specs/Running Notes/doubled-tracking-badger.md`

> **For execution (BULK-RUN mode — see [`Batch Plans/swift-stampeding-caribou.md`](swift-stampeding-caribou.md)):** code ALL tasks, NO per-task verify pause. Jeff runs `do_build.bat`; fix to clean. Verify scenarios author into Master Test Plan **§B.8**. **ONE** source commit (Rule 9). Work Log close entry drafted + HELD; applied at §B.8 section pass (R2).

## Context

QA-Fb′ is the recording-lifecycle lift underneath the BaySickAlign/BaySickPitch DSP work. Per the marathon (item 3, locked) it is **smaller than the original §5 docket** because two of its five items moved/shrank:
- The **channel-composite renderer** now lives in **QA-F Task 1** (Call 1a) — QA-Fb′ **consumes/verifies** it, does not build it.
- **Clip-resize propagation is VERIFY-ONLY** — QA-Ec already wired it end-to-end (verified 2026-07-09, Explore B).

Remaining real work: the **dual-buffer WET-bleed fix** + **conditional-WET** + the **dirty-flag asymmetry** fix.

**Current state (verified 2026-07-09, Explore agent B):**
- Dual DRY/WET recorders exist (`StripRecorder`, [PluginProcessor.h:730-743](Source/PluginProcessor.h:730)). DRY tap fires from the strip tasks' live-input branch (`tapDryRecorder`, [VoxStripTask.cpp:154-157](Source/Engine/Tasks/VoxStripTask.cpp:154) / [InstStripTask.cpp:198-201](Source/Engine/Tasks/InstStripTask.cpp:198); helper [PluginProcessor.cpp:3261-3277](Source/PluginProcessor.cpp:3261)). WET tap fires inside [BaySickVocalProcessor.cpp:319-340](Source/BaySickVocal/BaySickVocalProcessor.cpp:319), triggered solely by the `mWetRecorder` pointer being non-null.
- **Multi-take bleed is REAL and unguarded.** WET recorder is attached in `startRecording` ([:3185](Source/PluginProcessor.cpp:3185)) gated on `_arm` only — never checks FilePlay-active — and stays live the whole session. When a prior take's clip overlaps, [VoxStripTask.cpp:46-120](Source/Engine/Tasks/VoxStripTask.cpp:46) takes the FilePlay branch → `finalizeFilePlayStrip` → `engine->processBlock` → the WET tap captures **take 1's playback** into take 2's file. Compounding: the FilePlay branch `return`s at :120, so take 2's **live DI isn't captured at all** that block.
- **Conditional WET does NOT exist** — the WET tap writes unconditionally whenever the recorder pointer is set (no "skip WET when realtime pitch bypassed").
- **Dirty-flag asymmetry:** record-finalize marks dirty (`commitRecordingResult` → `markDirty`, [StandaloneEditor.cpp:11314/11401](Source/Standalone/StandaloneEditor.cpp:11314)). But **Vox/Inst page-creation does NOT** ([MixerPage::addVoxChannelAtIndex :2026-2064](Source/Standalone/MixerPage.cpp:2026) / `addInstChannelAtIndex :2358` never call markDirty) — unlike `addAuxChannel` ([:1819](Source/Standalone/MixerPage.cpp:1819)) which explicitly fires `onAnyStateChange()` because `createAndAddParameter` doesn't trip the APVTS dirty hook.

**Risk:** medium — the dual-buffer flow touches the Vox/Inst recording path (armed-record critical path). **Effort:** medium (~4-7h, shrunk). **Dependencies:** QA-F Task 1 (composite renderer — consumed/verified here). **Bucket:** Cross-cutting Infrastructure, System Pages.

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| G2-3b | Bleed gate: **new take = live input ONLY; prior-take playback never bleeds in. No bounce toggle.** | Marathon 3b (Jeff 2026-07-08). During armed record you HEAR prior takes (perform along) but the take's DRY/WET files capture only fresh live input. |
| G2-condWET | `startRecording` reads `bsv_pitch_realtime_bypass`; if pitch is bypassed, the WET recorder is never allocated (DRY only). | §3 (locked). WET==DRY when no pitch → wasted disk + confusing semantics. |
| G2-1 | Composite renderer built in QA-F Task 1; QA-Fb′ CONSUMES + verifies it (not built here). | Call 1a. |
| G2-resize | Clip-resize propagation = VERIFY-ONLY (QA-Ec wired it). | Explore B 2026-07-09: `setLengthBeats`→`commitEdit`→`onArrangementChanged`→`rebuildAudioClipPlayers`→`clipEndBeat` bounds both decode paths. |
| G2-dirty | Fix Vox/Inst page-creation to mark dirty (mirror `addAuxChannel`'s `onAnyStateChange()` workaround); confirm record-finalize already marks dirty (both triggers per §9 Task-9 clarification). | Explore B asymmetry finding; real bug, fix in-batch. |

## Sub-spec calls surfaced for approval
**None open.** Bleed-gate behavior + conditional-WET locked at marathon; dirty-flag is a mechanical fix. Any mid-execution call stops that piece and surfaces to Jeff.

## Files to modify

### Task 1 — Dual-buffer WET-bleed gate + conditional-WET
- [Source/Engine/Tasks/VoxStripTask.cpp](Source/Engine/Tasks/VoxStripTask.cpp) — during **armed recording**, the FilePlay branch (:46-120) must NOT short-circuit the live-input capture. Restructure so an armed strip: (a) decodes + monitors overlapping FilePlay clips (hear prior takes) AND (b) runs the live-input capture path so DRY/WET taps fire on the LIVE stream only. The recorder taps fire on the live stream at the pre-merge point; monitored output sums both.
- [Source/BaySickVocal/BaySickVocalProcessor.h](Source/BaySickVocal/BaySickVocalProcessor.h) / [.cpp](Source/BaySickVocal/BaySickVocalProcessor.cpp) — the WET tap (:319-340) fires only when the engine is processing the **live-input** stream (add a "capture-eligible" gate; do not tap FilePlay-decoded audio). Keep it real-time safe (atomic flag, no locks/allocs — Rule 6 RT zone).
- [Source/PluginProcessor.cpp](Source/PluginProcessor.cpp) — `startRecording` (:3163-3191): only allocate `sr.wetRecorder` when `isVox && ! bsv_pitch_realtime_bypass` (conditional-WET). `stopRecording` (:3234) unchanged.

### Task 2 — Dirty-flag: Vox/Inst page-creation
- [Source/Standalone/MixerPage.cpp](Source/Standalone/MixerPage.cpp) — `addVoxChannelAtIndex` (:2026-2064) + `addInstChannelAtIndex` (:2358): fire `mProcessor.onAnyStateChange()` after the strip/param creation, mirroring `addAuxChannel` (:1819). Guard against project-load (reuse the existing `isLoadingProject`/`mIgnoreDirty` suppression if the add path runs during deserialize).
- Confirm `commitRecordingResult` (:11314/11401) still marks dirty (no change expected).

### Task 3 — Clip-resize + composite-renderer VERIFY (no source change expected)
- Confirm resize propagation ([BuilderPage.cpp:4690](Source/Standalone/BuilderPage.cpp:4690)→`commitEdit :5049`→[StandaloneEditor.cpp:2162-2165](Source/Standalone/StandaloneEditor.cpp:2162)→`rebuildAudioClipPlayers`→`clipEndBeat` [:1019-1020](Source/PluginProcessor.cpp:1019)/[:575-577](Source/PluginProcessor.cpp:575)) still holds; consume/verify QA-F's `renderChannelComposite` on a Vox channel. If a real gap surfaces, fix in-batch (standing rule).

## Tasks

### Task 1 — Dual-buffer WET-bleed gate + conditional-WET
- [ ] Read `VoxStripTask::run` FilePlay + live-input branches; restructure so an armed strip captures live DI while still monitoring FilePlay prior takes (no early `return` skipping capture).
- [ ] Gate the WET tap to the live-input stream only (atomic capture-eligible flag; RT-safe).
- [ ] `startRecording`: allocate WET only when `isVox && !bsv_pitch_realtime_bypass` (conditional-WET).

### Task 2 — Dirty-flag: Vox/Inst page-creation
- [ ] Fire `onAnyStateChange()` in `addVoxChannelAtIndex` + `addInstChannelAtIndex` (mirror `addAuxChannel`); load-suppression guarded.
- [ ] Confirm record-finalize dirty still fires.

### Task 3 — Verify (composite + clip-resize)
- [ ] Consume/verify `renderChannelComposite` on a Vox channel; confirm clip-resize propagation holds.

### Batch close (one commit)
- [ ] Jeff runs `do_build.bat`; fix Release+Debug to clean.
- [ ] Author Master Test Plan **§B.8** from Verify scenarios.
- [ ] Draft + HOLD Work Log close entry; append code-complete entry + Rule 4 rows.
- [ ] Surface message + FULL git status → approval → **one** commit: `QA-Fb Tasks 1-3: dual-buffer WET-bleed gate + conditional-WET + Vox/Inst page-create dirty fix (Engine/Tasks, BaySickVocal, PluginProcessor, MixerPage, test plan B.8 + running notes)`.

## Verify scenarios (→ Master Test Plan §B.8; `blocks:` QA-Fb commit)
1. **Multi-take no-bleed** — record take 1 on a Vox page (live DI). Record take 2 while take 1 plays back (you HEAR take 1). Take 2's DRY/WET files contain ONLY the fresh live input — no take-1 bleed. (Compare take 2's file audio to what you sang.)
2. **Live DI captured under overlap** — with take 1 overlapping the record window, take 2's DRY file is non-silent and matches the live mic (the old early-`return` no longer drops capture).
3. **Conditional-WET** — record with realtime pitch OFF → only a DRY file is produced (no WET). Record with pitch ON → both DRY + WET produced.
4. **Dirty on page-create** — new project (clean, no `*`). Add a Vox page → title shows `*`. Repeat for an Inst page. (Was clean-after-add before.)
5. **Dirty on record-finalize** — record + stop → `*` shows; save → clean; reopen → clean (no phantom dirty).
6. **Clip-resize (verify)** — resize a Vox audio clip on the Builder grid → playback length follows (plain drag trims/extends; Shift/Stretch re-fits) — regression confirm of QA-Ec.
7. **Composite consume (verify)** — QA-F's composite renderer returns the correct summed buffer for a multi-clip Vox channel (shared-dependency smoke).

## Routing notes (Rule 3)
- Findings on QA-Fc surface → fold + note; §9 routing at §B.8 section pass.
- QA-J overlap fork: SEQUENTIAL same-row clips only; overlapping-same-row multi-take → campaign QA-J-Verify (§C ledger items 1-2).
- The dual-buffer restructure is the QA-AudioMeters BLOCKER's neighbor (storeAxes CAS-max, overlapping same-row) — do NOT test overlapping same-row here; that is §C.

## Carry-Forward Reference touch points
- Read §4 (MT render path) before Task 1 — `VoxStripTask`/`InstStripTask` are the only render path (serial fallback deleted, QA-Ef); recorder taps live inside them.
- Read §6 (recording lifecycle: arm → record → finalize → reload) before Task 1.
