# Running Notes — QA-MultiBlockHazard (fluffy-toasting-hartmanis)

> Append-only mid-batch log.  A new dated entry is added at EVERY checkpoint
> (commit landed / sub-task verified / finding captured / spec call resolved /
> scope pivot), per `feedback_draft_doc_running_notes_every_checkpoint.md`.
> At batch close, `/draft-doc batch-close` reads this file as the primary input
> for the single Implemented Work Log entry.  Never edit prior entries.

**Pair file:** [`Plans & Specs/Batch Plans/fluffy-toasting-hartmanis.md`](../Batch Plans/fluffy-toasting-hartmanis.md)
**Conventions:** Main Plan §0 Document Formatting Conventions + the Batch Plans / Running Notes required-sections rule (locked 2026-05-11).

## Diagnostic Instrumentation Catalog

Per §0 Rule 4 — every `DBG` / `juce::Logger` / temp `jassert` / debug `AlertWindow` /
temp-file trace gets a row IN THE SAME EDIT PASS.  Strip every `Remove` at task/batch
close after surfacing the strip list to Jeff.

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| (none yet) | | | |

## 2026-07-02 — Task 0 — Batch open

- Plan approved + mirrored to `Batch Plans/fluffy-toasting-hartmanis.md`; home-dir copy deleted (plan-file hygiene).
- **Batch scope:** engine/hot-path restructure split from QA-EffectsReview item (d).  On Audio/Vox/Inst strips a strip's insert chain (`processInsert` = polarity/preEQ/width/rack/postEQ/fader) runs once PER source instead of once per block, so stateful DSP (delay lines / reverb tails / LFO phase / compressor envelopes) advances N times/block and corrupts on source overlap.  Fix = sum a strip's sources into one buffer, run the shared processing exactly once.  Completes the original Composite-task intent (Carry-Forward §4 "sums them internally before insert DSP. Matches serial-mode summation").
- **Bug sites confirmed by source read:** Audio = `renderAudioClipsForRow` calls `processInsert` inside the per-clip loop (PluginProcessor.cpp:623) + Flow A engine pass (CompositeAudioInsertTask.cpp:94).  Vox/Inst = `renderFilePlayPlayer` called per routed clip (VoxStripTask.cpp:107 / InstStripTask.cpp:95), each running BOTH `eng->processBlock` AND `processInsert` per clip.  CAS-max metering (VibeGraph.cpp:2497-2511) already documents the multi-call as a meter-only compensation.
- **Spec calls locked (pre-plan chat, AskUserQuestion):** SC1 two source commits (Task 1 Audio / Task 2 Vox+Inst — they share `renderFilePlayPlayer`); SC2 CAS-max metering comment (`VibeGraph.cpp:2497`) gets a comment-only update in Task 2; SC3 overlap = sum -> single pass (Vox/Inst engine sees the summed audio); SC4 N->1 only, never 0->1 (idle/tail behavior unchanged); SC5 no decode-path unification.
- **Blast radius:** 5 files (`PluginProcessor.cpp` 2 helpers + `CompositeAudioInsertTask.cpp` + `VoxStripTask.cpp` + `InstStripTask.cpp`), + headers + the VibeGraph comment.  No serial-path callers remain post-QA-Ef (verified).
- **Note:** local `main` is 9 commits ahead of `origin/main` (the QA-EffectsReview + QA-Rules run is unpushed) — flagged to Jeff; push-before-risky-hot-path-work is his call.
- Main Plan §5 edits: QA-MultiBlockHazard docket gains STATUS:OPEN + Plan-file pointer + Effort estimate.
- **Next:** commit Task 0 (open, docs-only), then Task 1 — Audio path (rack once per block).

## 2026-07-02 — Task 1 (Audio multi-call) build + verify; clip-knobs finding routed to own batch

**Task 0 committed** `23503c8` (docs-only; tree clean; origin pushed by Jeff — was 9 commits ahead, now current).

**Task 1 status:** Audio-path edits landed — `renderAudioClipsForRow` now returns `bool` + sums RAW (no per-clip `processInsert`); `CompositeAudioInsertTask::run` runs ONE `processInsert` gated on `anySource`; Rule 6 comment updates (4 files). Jeff built Debug + tested:
- **Single-clip regression PASS** — mixer strip fader / mute / FX rack all affect a dropped clip → `processInsert` runs once correctly. The change is the intended no-op for a single source.
- Grainy/louder on the first test = the sample itself (ruled out on a second file).
- **Still pending:** overlap test (2+ simultaneous sources on one strip through a Delay → one clean pass, not doubled) + Release confirmation. Then commit Task 1.

**FINDING — clip-knobs gap (pre-existing, unrelated to the multi-call fix):** the timeline-WAV decode path (`renderAudioClipsForRow`, "Flow B") never runs a clip through the ClipsPage BaySickPlayer Player controls (volume / pan / pitch / filter / tone / width / ADSR). Only the piano-roll / sampler path ("Flow A") honors them. It's the **playback MODE** (WAV vs sampler) that gates the knobs, NOT the add-path — both add-paths (Builder drop + Clip-tab dropdown) behave identically (Jeff retest: knobs work on piano roll, dead on Builder/WAV for both). Confirmed pre-existing: Flow B never read engine params, before or after Task 1.
- Corrected two wrong claims made mid-investigation: the dropdown clip is NOT piano-roll-only (it also makes a draggable Builder WAV); ADSR CAN apply to a timeline clip (attack @ clipStart / release @ clipEnd), so essentially all Player controls can map.
- Intended design (Jeff): every clip is dual-purpose — piano roll = sampler, Builder = editable WAV — and BOTH should honor the Player setup; the WAV half is only half-wired.

**DECISION (Jeff):** route the clip-knobs feature to its **OWN batch** (not folded — it's a DSP feature build, not the hot-path multi-call fix). Behavior target: WAV playback honors the full Player control set while keeping stretch + trim. **Slot TBD** — surface placement options to Jeff at close (slot-placement is his call). Route formally per Rule 3 at close: new §5 batch row + §9 Forks entry.

**Next:** finish Task 1 verify (overlap + Release) → commit Task 1 → Task 2 (Vox+Inst).

## 2026-07-02 — Task 1 VERIFIED + committed; grid-mute finding routed to clip-knobs batch

**Task 1 (Audio multi-call) VERIFIED — PASS:** single-clip regression (strip fader / mute / FX all apply) + overlap test (2 sources on one strip through a Delay → clean single rack pass) both pass in Jeff's Debug+Release cycle. Committing the 4-file Audio-path change.

**FINDING #2 — clip builder-grid mute keys on the OWNER page, not the grid row (pre-existing; rode in with the route-by-owner refactor `c616f0d4` 2026-06-02):** two clips on one player page share a mute — muting the owner-row's grid track silences both; the other grid row's mute is inert. `renderAudioClipsForRow` (~PluginProcessor.cpp:457) checks `isRowAudible(row)` where `row` = owner page (audioInsert index), so both clips (same owner) resolve to the same audibility. NOT caused by the Task 1 summing — `git diff` + `git blame` confirm the mute/row-keying lines are untouched by this batch; the summing is audio-only.
- **Behavior confirmed (Jeff):** builder-grid track mutes act PER-GRID-ROW (each clip follows the grid row it sits on); the mixer STRIP mute stays per-owner-page. Fix = key the builder-grid mute on `player.trackRow` instead of `row` (verify `trackRow` tracks block moves before implementing); strip mute (`audioRowMute[row]`) + routing stay owner-keyed.
- **DECISION (Jeff):** route this fix WITH the clip-knobs feature into the SAME new dedicated batch ("kind of all one thing" — same clip-drop subsystem). Slot TBD at close. Route formally per Rule 3 at close: fold into the same new §5 batch row + §9 Forks entry as the knobs feature.

**Next:** commit Task 1 → Task 2 (Vox+Inst).
