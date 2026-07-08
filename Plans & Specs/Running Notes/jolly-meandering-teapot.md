# Running Notes — QA-UICleanup (jolly-meandering-teapot)

> Append-only running log for the QA-UICleanup batch. A new dated entry lands at every checkpoint — commit landed, sub-task verified, finding captured, spec call resolved, or scope pivot — per `feedback_draft_doc_running_notes_every_checkpoint.md`. At batch close, `/draft-doc batch-close` reads this file as the primary input for the single Implemented Work Log entry. Never edit prior entries; surprises get their own new entry.

**Paired plan file:** `Plans & Specs/Batch Plans/jolly-meandering-teapot.md`
**Conventions:** Main Plan §0 "Batch Plans + Running Notes layout" (locked 2026-05-11) + Document Formatting Conventions.

---

## Diagnostic Instrumentation Catalog

Per §0 Rule 4 — every temp `DBG` / `Logger` / `jassert` / debug `AlertWindow` / temp-file trace gets a row here IN THE SAME EDIT PASS as the code change. Strip every `Remove` row at task/batch close (surface the strip list first).

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| _(none yet)_ | | | |

---

## 2026-07-08 — Task 0 — open

- Batch opened. Plan drafted + approved (`jolly-meandering-teapot`); mirrored to `Plans & Specs/Batch Plans/`, home-dir copy deleted.
- Main Plan §5 QA-UICleanup docket marked `STATUS: OPEN` + plan-file pointer + effort estimate (~7-10h).
- Running notes seeded (this file).
- **Scope:** 7 items, all UI (no DSP). 16 spec calls + the default-quantize value all resolved in chat pre-plan (see the plan's Spec-calls-locked table). Task split B: Task 1 quit dialog / Task 2 Layers-Bass auto-name (diagnosis) / Task 3 snap dropdown + kit button / Task 4 menu consolidation (items 3+5+6+7).
- **Key pre-plan findings:**
  - (a) The quit prompt is `juce::AlertWindow` (draggable by its own ComponentDragger, not a DocumentWindow) → going native `TaskDialog`; JUCE 8's native path supports custom Save/Don't-Save/Cancel buttons ([juce_NativeMessageBox_windows.cpp:160-171](juce/modules/juce_gui_basics/native/juce_NativeMessageBox_windows.cpp)). Shared `confirmDiscardChanges` helper → fixes quit + New + Open + New-from-Template.
  - (b) Item 2 is NOT "editor doesn't fire onPatchLoaded" — the hooks fire (BaySickSynthEditor:1126, VibePlayerEditor 7 sites) and the page wiring is parallel to the working BaySickBass, so Task 2 is a runtime **diagnosis**, not a known fix.
  - (c) The drum-kit grid (`DrumKitGrid`) is a **separate** menu-bar model from the engine piano-roll; it gets items 3/4/5/6 but NOT 7 (no transpose; no Arpeggiate/Generate Chords).
  - (d) New `Unified_QuantizeDiv` APVTS param — separate from snap (SC10), shared across editors (SC16), per-project (SC11), default 1/4.
- **Next:** Task 1 — native centered quit/discard prompt.
