# Running Notes — QA-Fb′ (doubled-tracking-badger)

> Append-only running log for QA-Fb′. New `## YYYY-MM-DD — <checkpoint>` entry at every checkpoint per `feedback_draft_doc_running_notes_every_checkpoint.md`. Under BULK-RUN mode ([`Batch Plans/swift-stampeding-caribou.md`](../Batch Plans/swift-stampeding-caribou.md)) there are no per-task verify entries; the Work Log close entry is drafted + HELD here under `## Held Work Log entry (apply at section pass)` at code-complete, applied at Master Test Plan §B.8 section pass (R2).
>
> Pair file: [`Plans & Specs/Batch Plans/doubled-tracking-badger.md`](../Batch Plans/doubled-tracking-badger.md). Conventions: Main Plan §0.

## 2026-07-09 — Group open (G2) — seeded

Plan approved 2026-07-09 (G2 group approval, R5). QA-Fb′ SHRANK vs the §5 docket: composite renderer moved to QA-F (Call 1a), clip-resize is now verify-only (QA-Ec wired it). Remaining real work: dual-buffer WET-bleed gate + conditional-WET + dirty-flag fix.

### Locked spec calls
- **Marathon 3b** — bleed gate: new take = live input ONLY; prior-take playback never bleeds in; no bounce toggle.
- **§3** — conditional-WET: skip WET recorder allocation when realtime pitch bypassed.
- **Call 1a** — composite renderer built in QA-F; QA-Fb′ consumes/verifies (not built here).
- **Verify-only** — clip-resize propagation (QA-Ec); dirty-flag fix = mirror Aux's `onAnyStateChange()` on Vox/Inst page-create.

### Surface map (current code, verified 2026-07-09 via Explore agent B)
- Dual recorders exist: `StripRecorder` `PluginProcessor.h:730-743`. DRY tap fires from strip tasks' live branch (`tapDryRecorder`, `VoxStripTask.cpp:154-157` / `InstStripTask.cpp:198-201`; helper `PluginProcessor.cpp:3261-3277`), gated on `armed`. WET tap `BaySickVocalProcessor.cpp:319-340`, triggered solely by `mWetRecorder` non-null.
- **Bleed unguarded:** WET attached `startRecording :3185` (arm-only, no FilePlay check), stays live whole session. Prior-take overlap → `VoxStripTask.cpp:46-120` FilePlay branch → engine processBlock → WET captures take 1; the branch `return`s at :120 so take 2's live DI isn't captured either.
- **Conditional-WET absent** — WET writes unconditionally.
- **Clip-resize WORKS** (verify): `BuilderPage.cpp:4690`→`commitEdit :5049`→`StandaloneEditor.cpp:2162-2165`→`rebuildAudioClipPlayers`→`clipEndBeat` bounds both decode paths (`PluginProcessor.cpp:1019-1020`/`:575-577`).
- **Dirty asymmetric:** record-finalize marks dirty (`commitRecordingResult` `StandaloneEditor.cpp:11314/11401`); Vox/Inst page-create does NOT (`MixerPage::addVoxChannelAtIndex :2026-2064` / `addInstChannelAtIndex :2358`), unlike `addAuxChannel :1819` (`onAnyStateChange()`).

### Fork-out (Rule 3)
- Do NOT test overlapping same-row multi-take here — that is the QA-AudioMeters BLOCKER's neighbor → campaign QA-J-Verify (§C ledger items 1-2).
