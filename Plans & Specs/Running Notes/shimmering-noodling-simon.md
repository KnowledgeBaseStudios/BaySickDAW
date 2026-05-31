# Running Notes — QA-EngineApvts (shimmering-noodling-simon)

> **Purpose:** Append-only running log for the QA-EngineApvts batch. A new entry is appended at every checkpoint — commit landed / sub-task verified / finding captured / spec call resolved / scope pivot — per `feedback_draft_doc_running_notes_every_checkpoint.md` and the Main Plan §0 running-notes rule. At batch close, `/draft-doc batch-close` reads this file as the primary input when compiling the single Implemented Work Log entry. Never edit prior entries; surprises get their own new entry.

> **Pair file:** `Plans & Specs/Batch Plans/shimmering-noodling-simon.md` (the batch plan).
> **Conventions:** Main Plan §0 "Document Formatting Conventions" + "Batch Plans + Running Notes layout (locked 2026-05-11)".

## Diagnostic Instrumentation Catalog

Per Main Plan §0 Rule 4. None added yet — this is a mechanical refactor; verification is via Jeff's audio/CPU testing, not log traces. Any temp `DBG` / `juce::Logger` / `AlertWindow` / temp `jassert` added during a verify-fail investigation gets a row here IN THE SAME EDIT PASS, and every `Remove` row is stripped (with the strip list surfaced to Jeff) before the relevant commit.

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| (none yet) | | | |

## 2026-05-30 — Task 0 — open

- Batch opened. Pre-batch ritual: `/standup`; full self-read of Main Plan §0 (Rules 1–5 + formatting conventions + buckets + orchestration rules); `/read-doc` bulk extractions (§5 QA-EngineApvts entry + §6 slot + §9 thirty-fifth / thirty-seventh / forty-fifth Forks; Carry-Forward primitives; Work Log recent closes). CLAUDE.md cross-check: "Next batch: QA-Md" + "Current position (2026-05-08)" confirmed **stale** vs Work Log — relying on Work Log / Main Plan for status (real next = QA-EngineApvts; QA-Md closed 2026-05-09).
- **Architecture surfaced + Jeff-locked across 4 rounds** (plan §"Spec calls already locked" L1–L8): Option B (reuse the existing `ApvtsDirtyTracker`) + centralized per-processor `replaceApvtsState()` reattach choke-point + RT-safe pointer-compare self-heal backstop + tempo-as-change for BaySickSynth/BaySickBass + ONE consolidated source commit + `BaySickSynthVoice::startNote` osc-reset fold-in. Scope = the 4 legacy engines only (L7).
- **Key mid-planning findings (all source-verified):**
  - (1) `ApvtsDirtyTracker` (2026-05-05) already exists in all engines as the project-dirty title-bar `*` marker bridge — Concept B, distinct from the perf gate (Concept A) the Work Log warned not to conflate.
  - (2) `apvts.replaceState` (`juce_AudioProcessorValueTreeState.cpp:407` → `state = newState`; `juce_ValueTree.cpp:612` `operator=`) **orphans** the tracker's separate `mState` wrapper — verified in JUCE source. Naive gate → engine stops responding to knobs after any preset/project load. Drove the reattach architecture.
  - (3) `replaceState` is called from **17 target-engine sites** (4 `setStateInformation` + 5 editors + 8 pages), not the 4 first assumed. `StandaloneEditor.cpp:10262` is a `BaySickRustyDrums` (sfizz) swap → out of scope.
  - (4) Tempo correctness: BaySickSynth/Bass `updateFromApvts` consume per-block `mHostBPM` for synced-LFO rate (Synth `.cpp:521/527/534`); VibePlayer/Harmless are APVTS-only. → tempo-as-change OR (L4).
  - (5) **Anticipated close-routing finding:** the 6 sfizz engines share the same latent tracker orphaning (project-dirty marker only; no gate here) — route at close per Rule 3 (slot is Jeff's).
- Plan written + approved (ExitPlanMode); mirrored to `Batch Plans/shimmering-noodling-simon.md`, home-dir copy deleted; §5 `**Plan file:**` pointer updated.
- Next: Task 0 doc commit, then Task 1 implementation (single consolidated source commit).
