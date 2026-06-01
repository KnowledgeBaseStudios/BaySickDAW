# Running Notes — QA-Ed (virtual-moseying-cocoa)

> Append-only mid-batch log. A new `## YYYY-MM-DD — Task N — <name>` entry is appended at **every checkpoint** (commit landed / sub-task verified / finding captured / spec call resolved / scope pivot), per `feedback_draft_doc_running_notes_every_checkpoint.md` + Main Plan §0 Rule 4. At batch close, `/draft-doc batch-close` consumes this file as the primary input for the single Implemented Work Log entry.

**Pair file:** `Plans & Specs/Batch Plans/virtual-moseying-cocoa.md` (the QA-Ed plan).
**Conventions:** Main Plan §0 "Batch Plans + Running Notes layout (locked 2026-05-11)" + Document Formatting Conventions.

---

## 2026-06-01 — Task 0 — open

- **Pre-batch research:** `/standup`; Main Plan §0 full self-read; §5 QA-Ed entry + §6 arrow + §9 (2026-05-18 QA-Ed-created / 2026-05-20 QA-Ee-inserted / 2026-06-01 QA-Sfizz-Followup close); Carry-Forward + Implemented Work Log extraction (transport/playhead/scheduler are **greenfield** in the frozen 2026-05-07 snapshot — source code is the authority); CLAUDE.md cross-check (the "Next batch: QA-Md" line is stale; QA-Ed is next per the §6 arrow + Work Log).
- **Direct code reads:** `StandalonePlayHead` (`StandaloneApp.h/.cpp` — `advanceBlock` float beat accumulator + `fmod` wrap); the song + pattern scheduler (`PluginProcessor.cpp:1035-1561`) incl. the `mPRLastBeatEnd`/`kWrapSlop`/`jumped`/`windowStart` band-aid; `onGetLoopBeats` loop config (`mLoopBeats` == `mCachedPatternLoopBeats`; loop-start 0 except time-selection); tempo automation (`global_tempo` applicator → `mPlayHead.setBPM`, message-thread-driven via `applyAutomationAtCurrentPosition`).
- **Spec calls locked by Jeff 2026-06-01:** SC-1 int64 absolute sample counter + tempo anchor (no tempo map — future batch); SC-2 one atomic source commit; SC-3 no `/architecture` pass; SC-4 **sample-accurate loop seam (option B)**, chosen over block-granular (A).
- **QA-Ee boundary verified clean:** QA-Ed = sample-domain transport position + band-aid removal (permanent foundation); QA-Ee = musical-domain 96-PPQ tick storage that rides on top + re-expresses the scheduler's comparison units. QA-Ee does not rework QA-Ed.
- **Finding surfaced + acknowledged (mechanism correction vs §5 wording):** the §5 "exact clock → plain `>= beatStart` gate" premise is incomplete — the scheduler runs *before* `advanceBlock`, so the wrap lands one block's overshoot past loop-start, leaving a sub-block seam gap that drops the boundary note even with an exact clock. The fix retains exact-valued loop-seam logic (window split); the float band-aid is still fully removed.
- **Plan-agent design review folded in:** (1) **seqlock** anchor + single-writer model (torn-read of the 3-field anchor tuple could yield a wildly-wrong beat for one block); (2) integer **straddle test via `PositionInfo::timeInSamples`** (the playhead's `llround` wrap point vs a float `beatEnd>loopEnd` test could disagree by one block → intermittent double-fire/gap — the original symptom resurrected); (3) shared **window-based scheduler helper** (`RollWindow[2]` + `scheduleRollWindows`) replacing the ~14 per-engine inline loops across both modes; (4) sample-accurate **off-sweep** at the wrap (offs `>= loopEnd` fire at the exact wrap sample — no leak); (5) **sub-block-loop guard**; (6) `mLoopStartBeats` wired to the scheduler.
- **Plan written + approved** (ExitPlanMode, no user edits). Mirrored `~/.claude/plans/` → `Plans & Specs/Batch Plans/virtual-moseying-cocoa.md`; home-dir copy deleted (one-way mirror). Main Plan §5 plan-file pointer updated. This running-notes file seeded.
- **Next:** surface full git status → `/draft-commit` → Task 0 open commit; then Task 1a (playhead int-sample clock + seqlock anchor).
