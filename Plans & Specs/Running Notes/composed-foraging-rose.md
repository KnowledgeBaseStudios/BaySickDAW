# Running Notes — QA-EffectsReview (composed-foraging-rose)

> Append-only mid-batch log.  A new dated entry is added at EVERY checkpoint
> (commit landed / sub-task verified / finding captured / spec call resolved /
> scope pivot), per `feedback_draft_doc_running_notes_every_checkpoint.md`.
> At batch close, `/draft-doc batch-close` reads this file as the primary input
> for the single Implemented Work Log entry.  Never edit prior entries.

**Pair file:** [`Plans & Specs/Batch Plans/composed-foraging-rose.md`](../Batch Plans/composed-foraging-rose.md)
**Conventions:** Main Plan §0 Document Formatting Conventions + the Batch Plans / Running Notes required-sections rule (locked 2026-05-11).

## Diagnostic Instrumentation Catalog

Per §0 Rule 4 — every `DBG` / `juce::Logger` / temp `jassert` / debug `AlertWindow` /
temp-file trace gets a row IN THE SAME EDIT PASS.  Strip every `Remove` at task/batch
close after surfacing the strip list to Jeff.

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| (none yet) | | | |

## 2026-06-06 — Task 0 — Batch open

- Plan approved + mirrored to `Batch Plans/composed-foraging-rose.md`; home-dir copy deleted (plan-file hygiene).
- **Batch RE-SCOPED at open:** the 4-bug docket -> a full effects-subsystem max-clone fidelity rework (every rack effect + every pedal graded + reworked vs its reference; per-slot Basic/Advanced toggle; Console Clean/Dirty).  Item **(d) multi-call SPLIT** to a new batch **QA-MultiBlockHazard** (engine/hot-path, not effect fidelity), directly after.
- Step-1 fidelity audit (read-only, 8 research passes) complete; findings written to `Research Reports/effects-fidelity-audit-2026-06-06.md`.
- Spec calls locked at open (see plan SC-* table): wide scope; one cohesive batch; big build on all 4 heavy units (De-Esser, SY-1, AD-2, Tape); Console = Clean(SSL)/Dirty(Neve) reusing the Tube band-split + shapers; Basic/Advanced toggle = per-slot, saved with project, default Basic, FX-rack panels ONLY (pedals + board basic-panels untouched).
- Main Plan edits: §5 QA-EffectsReview STATUS:OPEN + Plan-file pointer + re-scope note; NEW §5 QA-MultiBlockHazard docket; §6 arrow + footnote; §9 2026-06-06 Forks entry.
- **Next:** Task 1 — Basic/Advanced toggle infrastructure (foundational; lands + verifies before any per-effect extras tagging).
