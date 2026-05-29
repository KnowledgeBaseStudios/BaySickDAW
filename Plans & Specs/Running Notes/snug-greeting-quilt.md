# Running Notes — QA-DispatcherAffinity (snug-greeting-quilt)

> **Purpose:** append-only mid-batch log populated at every checkpoint (commit landed / sub-task verified / finding captured / spec call resolved / scope pivot / diagnostic instrumentation added) per Main Plan §0 Rule 4 + the `/draft-doc running-notes` cadence.  At batch close, `/draft-doc batch-close` reads this file as the primary input when compiling the single Implemented Work Log entry.  Never edit prior entries — surprise findings get their own new entry below.

> **Pair file:** `Plans & Specs/Batch Plans/snug-greeting-quilt.md` (per-batch plan).
> **Convention reference:** Main Plan §0 "Running notes file required sections" (locked 2026-05-11; exemplar `federated-bouncing-cupcake.md`).

---

## 2026-05-28 — Task 0 — Batch open

**Mirror + delete:** plan file `snug-greeting-quilt.md` mirrored from `~/.claude/plans/snug-greeting-quilt.md` → `Plans & Specs/Batch Plans/snug-greeting-quilt.md`; home-dir copy deleted per `feedback_plan_mirror_one_way.md`.

**Main Plan updates:**
- §5 QA-DispatcherAffinity entry — STATUS banner ADDED summarizing the plan-mode double pivot (global barrier rejected → DAG upgrade rejected post-exploration → investigation-first sfizz Candidate B reframe); `**Plan file:**` updated from `<silly-name>.md (when started)` placeholder to backticked-path form `Plans & Specs/Batch Plans/snug-greeting-quilt.md`.
- §9 forty-first Forks entry ADDED — full pivot chronology + Jeff's verbatim quotes + source-verification findings + post-pivot task structure.
- §6 — no arrow change; QA-DispatcherAffinity 25-asterisk footnote stays as-is.

**Plan-mode spec calls resolved (carried over to plan file's Spec calls already locked table S1-S12):**
- S1: Q1 / Q1' = Option (e) investigation-first; all fix-shape picks deferred to Task 2 mid-batch spec call.
- S2: Q2 = REJECTED (no global synchronization barrier — Jeff verbatim).
- S3: Q2 pivot = DAG + topological sort upgrade — REJECTED post-exploration (dispatcher already implements dep-driven DAG via `mDeps` + `mInitialDeps` + `mChildren` + `mPredecessors` on `RenderTask` + `addSyntheticDep` at `PluginProcessor.cpp:4142`).
- S4: §9 fortieth Candidate A cross-block race hypothesis = dead (`mAllDone` gating).
- S5: Q3 (Candidate B implementation shape) = DEFERRED to Task 2.
- S6: Q4 = Option (C); re-interpreted post-pivot as "B.1 in scope for trace investigation alongside B.2/B.3/B.4".
- S7: Sub-K retirement = conditional on Task 3 cure verify.
- S8: Verify gate = BaySickRustyDrums 6-cymbal crash MT-on test (same as QA-Sfizz Sub-K).
- S9: Trace captures entry+exit timestamps + thread IDs (Sub-F=(e) entry-only-trace lesson).
- S10: Silly-name = `snug-greeting-quilt` (harness-assigned).
- S11: One commit per task structure (Task 0 / Task 1 / Task 3 / Task 4 conditional / Task 5 close).
- S12: Trace instrumentation Disposition = `Remove at Task 4 close` (or batch close if Task 4 skipped).

**Sub-spec calls genuinely deferred to mid-batch:**
- Sub-A: Task 3 fix shape — pick after Task 1 trace data lands at Task 2 spec call.

**Working tree state at Task 0 commit time:** 3 CRLF-residue files (`Source/BaySickBasses/BaySickBassesProcessor.h`, `Source/BaySickGuitars/BaySickGuitarsProcessor.h`, `Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.h`) — pure CRLF normalization residue from QA-Sfizz Sub-G + Sub-I `git restore --source=HEAD` reverts; verified zero content diff via `git diff --ignore-all-space`; documented harmless in QA-Sfizz Task 6 close commit `5079c5d`.  Left untouched (not staged) per pre-commit discipline.

**Task 0 commit:** TBD (after `/draft-commit` + Jeff approval; SHA appended here at commit landing).

## Diagnostic Instrumentation Catalog

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| (Task 1 will populate this section when trace instrumentation lands.) | | | |
