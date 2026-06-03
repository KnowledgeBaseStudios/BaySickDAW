# Running Notes — QA-Ee (rhythmic-counting-octopus)

> **Purpose.** Append-only running log for QA-Ee (96 PPQ Universal Timebase + Decoupled Snap
> Params). A new dated entry is appended at **every checkpoint** — commit landed / sub-task
> verified / finding captured / spec call resolved / scope pivot (Main Plan §0 +
> `feedback_draft_doc_running_notes_every_checkpoint.md`). At batch close, `/draft-doc batch-close`
> reads this file as the primary input for the single Implemented Work Log entry. Never edit prior
> entries; later surprises get their own new entry.

> **Pair file:** `Plans & Specs/Batch Plans/rhythmic-counting-octopus.md` (the QA-Ee plan).
> **Conventions:** Main Plan §0 (Document Formatting Conventions + the Batch Plans / Running Notes
> required-sections rule, locked 2026-05-11; exemplar `federated-bouncing-cupcake.md`).

## Diagnostic Instrumentation Catalog

_(Main Plan §0 Rule 4. Row format: Site | Tag | Purpose | Disposition. Append a row in the SAME
edit pass as any `DBG` / `Logger` / temp-`jassert` / debug-`AlertWindow` / temp-file diagnostic
added during QA-Ee. At task/batch close, strip every `Remove` row after surfacing the strip list to
Jeff. NOTE: QA-ClipDrop's armed Task-1 trap is **not** catalogued here — it belongs to QA-ClipDrop.)_

_None yet._

## 2026-06-03 — Task 0 — open

- Batch opened. Plan approved (`rhythmic-counting-octopus`). Structure = staged commits (Jeff
  SC-A): Stage 1 strictly the block data-model migration verified to load + play before any UI;
  Stages 2-4 = Builder / PianoRoll / Record snap.
- Spec calls locked by Jeff 2026-06-02: **SC-A** staged commits, **SC-B** global PianoRoll snap
  (drop per-roll `snapDenominator`), **SC-C** automation out of scope (curve points stay 0..1
  fractions, `lfoRate` stays float; only the automation clip's `ArrangementBlock` migrates).
  SC-3 (bridge) / SC-5 (decoupled) / SC-ii (drop Events+Line) / SC-4 (triplet lines identical) +
  the 10-label scheme were locked in §5. Defaults preserve current behavior (SC-def).
- Research findings folded in: automation curve points already store as clip-length fractions
  (`ControlPoint.timeTicks`, PatternManager.h:10) — no migration needed; PianoRoll already has a
  snap control (`mMagnetBtn` + per-roll `snapDenominator`) going global.
- Mirrored the approved plan to the reserved canonical name (Main Plan.md:4412); deleted the
  transient home-dir copy. Added the `**Plan file:**` pointer to the Main Plan §5 QA-Ee entry.
- **Baseline refreshed** against current main `1e53a2d` (QA-ClipDrop Tasks 0-3, batch held open for
  an intermittent saved-project copy-failure). QA-Ee core surfaces (ArrangementBlock struct, XML
  serdes, PianoNote, PianoRoll) confirmed **untouched** by QA-ClipDrop -> tick design holds. Folded
  into the plan: the new clip-routing model (clips route by `routeChannel` / owning Clips strip,
  `trackRow` is visual-only) + QA-ClipDrop's coexisting load fixup (`routeChannel==0 ->
  audioInsert(trackRow)`, PluginProcessor.cpp:2314) which composes cleanly with QA-Ee's
  `startBeats x 96 -> startTicks` XML migration. QA-ClipDrop's armed trap is theirs (Keep).
- **Finding (resolved in Task 0):** the §5 QA-Ee "Sequencing" line read the stale "after QA-Ed,
  before QA-Eb" (the §6 arrow + the inserted QA-ClipDrop / QA-TempoMap rows are authoritative;
  actual slot is after QA-ClipDrop, before QA-TempoMap). Jeff approved the one-line coherence fix;
  corrected in the Task 0 commit (kept the SC-i = (b) provenance + noted the original slot).
