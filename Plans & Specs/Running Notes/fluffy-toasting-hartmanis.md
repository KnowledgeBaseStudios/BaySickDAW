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
