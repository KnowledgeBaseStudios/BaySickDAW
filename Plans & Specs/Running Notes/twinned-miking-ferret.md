# Running Notes — QA-Fc (twinned-miking-ferret)

> Append-only running log for QA-Fc. New `## YYYY-MM-DD — <checkpoint>` entry at every checkpoint per `feedback_draft_doc_running_notes_every_checkpoint.md`. Under BULK-RUN mode ([`Batch Plans/swift-stampeding-caribou.md`](../Batch Plans/swift-stampeding-caribou.md)) there are no per-task verify entries; the Work Log close entry is drafted + HELD here under `## Held Work Log entry (apply at section pass)` at code-complete, applied at Master Test Plan §B.9 section pass (R2).
>
> Pair file: [`Plans & Specs/Batch Plans/twinned-miking-ferret.md`](../Batch Plans/twinned-miking-ferret.md). Conventions: Main Plan §0.

## 2026-07-09 — Group open (G2) — seeded

Plan approved 2026-07-09 (G2 group approval, R5). QA-Fc is fully locked by §23; zero open spec calls. Orthogonal to the rest of G2; sequenced last.

### Locked spec calls
- **§23** — parallel Mic B path, **summed** (not blended). 8 new `_b_` params; `nam_micb_active` default false = byte-identical when off. Editor Mic A|Mic B column split. Picks up on both Vox + Inst automatically.
- SlotSnapshot gains 9 fields (8 params + `micbUserIrPath`); A/B slots preserve dual-mic state independently.

### Surface map (current code, verified 2026-07-09 via Explore agent C)
- 18 params (`createLayout` `BaySickNAMIRProcessor.cpp:69-136`), incl. `nam_micsim_*` (3) + `nam_placement_*` (4). No `_b_` params exist.
- Chain (`processBlock :240-457`): … → Cab IR (:405-427) → Mic Sim (:429-439) → Mic Placement (:441-452) → Master (:454-456). ONE `mMicSim`/`mMicPlacement` (`:h258-259`). Need a 2nd of each + `mPreMicScratch`/`mMicBScratch` (scratch buffers `:h244-248`, prepared `:186-191`).
- `SlotSnapshot` 17 fields (`:h139-168`; §23 said 16 — use 17); serializers `:704-746`; capture/restore `:751-830`; state `:860-1017`, per-slot IR reload `:969-978`.
- Editor fixed 760×560 (`:9-10`), single-column: Mic Sim row `:465-484`, Mic Placement `:486-499` (`kMicSimRowH=100`); members `:h111-136`. Split into Mic A|Mic B columns; bump `kEditorH` only if needed + update host bounds (`BaySickVocalEditor.cpp:37-40/:500`, `InstPage.cpp:84/:345`).
- Hosted per-page on BOTH Vox + Inst (each owns its own `BaySickNAMIRProcessor`) → change lands once, propagates to both.
