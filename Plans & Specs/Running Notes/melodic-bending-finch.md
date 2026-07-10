# Running Notes — QA-Fa (melodic-bending-finch)

> Append-only running log for QA-Fa. New `## YYYY-MM-DD — <checkpoint>` entry at every checkpoint per `feedback_draft_doc_running_notes_every_checkpoint.md`. Under BULK-RUN mode ([`Batch Plans/swift-stampeding-caribou.md`](../Batch Plans/swift-stampeding-caribou.md)) there are no per-task verify entries; the Work Log close entry is drafted + HELD here under `## Held Work Log entry (apply at section pass)` at code-complete, applied at Master Test Plan §B.7 section pass (R2).
>
> Pair file: [`Plans & Specs/Batch Plans/melodic-bending-finch.md`](../Batch Plans/melodic-bending-finch.md). Conventions: Main Plan §0.

## 2026-07-09 — Group open (G2) — seeded

Plan approved 2026-07-09 (G2 group approval, R5). QA-Fa consumes QA-F's shared foundation (composite renderer + shifters); §14 fully locked. Code starts after QA-F Tasks 1-2 land.

### Locked spec calls
- **§14a-§14g** — full BaySickPitch redesign locked; composite-driven, Slice/Edit modes, per-note vibrato/formant/volume sub-curves, realtime applicator (Mode C), render-to-bake `Pitched/`, ~10-15 `bsp_*` params.
- **Call 4a** — DSP-04 drag-drop import DEAD; composite-auto-resolve only (§14b removed the Load button).
- **Colors** (§14c/§15) — pills Effects-purple fill / Vox-teal waveform / Bass-green pitch curve. 3 knobs → Focus/Mod/Speed.
- **Send Notes to** (§14b) — active Layers/Bass/Drums/Clips; MIDI only (not audio).
- **Brand-safety** — remove Newtone trade-dress (renames + `:8` comment); engine name BaySickPitch stays. QA-F owns the BaySickVocal-wide sweep; QA-LegalReview does the tree-wide pass.

### Surface map (current code, verified 2026-07-09 via Explore agent A)
- **No `BaySickPitchDSP` exists** anywhere. Net-new DSP.
- `BaySickPitchEditor` zero APVTS attachments; CENTER/VARIATION/TRANS inert (`bigKnob` lambda `:127-146`, no onValueChange); `Load` no `onClick` (`:89`); grid handlers early-return `!hasAudio()` (never true); `:8` = "Newtone-clone".
- No drag-drop import path (no `FileDragAndDropTarget`).
- Consumes QA-F: `renderChannelComposite` (Task 1) + `PitchShifters.h` (Task 2) — confirm interfaces final before Task 1 (architectural-alignment risk per §5).

### Fork-out (Rule 3)
- QA-J overlap: SEQUENTIAL same-row clips only; overlap → campaign QA-J-Verify (§C ledger item 2).
