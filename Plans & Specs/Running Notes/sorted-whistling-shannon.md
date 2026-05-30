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

**Open commit:** pending (`/draft-commit` → surface → Jeff approval; plan docs only).
