# Running Notes — QA-UICleanup (jolly-meandering-teapot)

> Append-only running log for the QA-UICleanup batch. A new dated entry lands at every checkpoint — commit landed, sub-task verified, finding captured, spec call resolved, or scope pivot — per `feedback_draft_doc_running_notes_every_checkpoint.md`. At batch close, `/draft-doc batch-close` reads this file as the primary input for the single Implemented Work Log entry. Never edit prior entries; surprises get their own new entry.

**Paired plan file:** `Plans & Specs/Batch Plans/jolly-meandering-teapot.md`
**Conventions:** Main Plan §0 "Batch Plans + Running Notes layout" (locked 2026-05-11) + Document Formatting Conventions.

---

## Diagnostic Instrumentation Catalog

Per §0 Rule 4 — every temp `DBG` / `Logger` / `jassert` / debug `AlertWindow` / temp-file trace gets a row here IN THE SAME EDIT PASS as the code change. Strip every `Remove` row at task/batch close (surface the strip list first).

All auto-name-trace sites write, in call order, to `Documents/BaySickDAW/qa_rename_trace.txt` (**silent file log** — switched from `AlertWindow` because the async popup perturbed the timing of the race). Confirmed 2026-07-08: the rename lambda runs with a valid name (screenshots) but the stored value is OVERWRITTEN afterward (stays stale through minimize/restore) — this trace hunts the overwriter.

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| `BaySickSynthEditor.cpp` loadPreset (~:1126, before the fire) | `[QA-UICleanup DIAG]` | Log `onPatchLoaded` SET/NULL + file at fire time (broken BaySickSynth) | Remove at Task 2 close |
| `LayersPage.cpp` `onPatch` lambda (~:184) | `[QA-UICleanup DIAG]` | Log that the rename lambda ran + engine/name (Layers) | Remove at Task 2 close |
| `LayersPage.cpp::setTabName` (~:324) | `[QA-UICleanup DIAG]` | Log EVERY write to `mTabName` — catches the overwrite of the piano-roll name | Remove at Task 2 close |
| `LayersPage.cpp::refreshPianoRollContextLabel` (~:330) | `[QA-UICleanup DIAG]` | Log the label value pushed to the piano roll | Remove at Task 2 close |
| `BassPage.cpp` `onPatch` lambda (~:177) | `[QA-UICleanup DIAG]` | Log the rename lambda on Bass (working reference: BaySickBass) | STRIPPED 2026-07-08 |

> **STRIPPED 2026-07-08** — all 5 sites removed (Jeff-approved) after the root cause was found. `grep qa_rename_trace / "QA-UICleanup DIAG"` over `Source/` = 0 matches. `qa_rename_trace.txt` can be deleted.

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

## 2026-07-08 — Task 1 — native quit/discard prompt (in progress)

- **Finding + scope pivot (Jeff: fix in-batch; NO routing out of this batch — he's opening a separate grouped set of batches for everything else).** The quit save-prompt was gated behind `hasProject()` ([ProjectManager.h:127](Source/ProjectManager.h:127) = `mCurrentFolder != juce::File()`), so a never-saved session with unsaved edits (title-bar `*` showing) quit **silently** with no prompt. Jeff's repro: fresh app -> add Layers + notes (`*` shows) -> hit X -> just closed. My native-dialog swap wasn't even reached — `requestAppQuit()` bailed on `!hasProject()` upstream.
- **Expanded item 1** ([StandaloneEditor.cpp:9181](Source/Standalone/StandaloneEditor.cpp:9181)): (a) native `TaskDialog` (`NativeMessageBox::showAsync` + `MessageBoxOptions`) replaces the draggable `AlertWindow` in `confirmDiscardChanges` — custom Save/Don't-Save/Cancel preserved (JUCE 8), plainIndex result mapping 0=Save/1=Don't-Save/2=Cancel; (b) both `confirmDiscardChanges` + `requestAppQuit` now gate on `isDirty()` alone (dropped `hasProject()`) so any unsaved edits prompt; (c) Save on an unnamed session routes through `promptForProjectName` -> `saveProjectAs(name)` -> continuation (mirrors the record-arm save pattern at :657). Shared helper => covers quit + File->New + File->Open + New-from-Template.
- Rare "Save failed" / "Invalid name" sub-dialogs stay `AlertWindow` (secondary error paths, out of item-1 scope; a name-entry box can't be a native TaskDialog anyway).
- **Draggability spec call (resolved 2026-07-08 — Jeff: Option 1).** Native Windows TaskDialogs are ALWAYS movable by their title bar (no flag disables it); native buys *centered + no click-body-drag* (the old AlertWindow's grab-the-body drag), NOT *immovable*. My SC1 pitch of native = "non-draggable" was wrong. Jeff accepted the native dialog as-is (Option 1) over a custom immovable in-app modal — the actual bug (click-drag) is gone and a rarely-seen prompt being title-bar-movable is benign.
- **VERIFIED (Debug + Release, all pass):** unnamed-session repro now prompts (native, centered, no click-drag); Save -> name box -> Create -> saves + quits; Cancel / Don't-Save behave; named-project File->New -> edit -> X prompts + saves in place.

## 2026-07-08 — Task 2 — Layers/Bass auto-name (ROOT CAUSE + fix)

- **Root cause — NOT engine-specific, NOT a race (both my earlier reads were wrong).** The "+"-add-tab path (`StandaloneEditor::onAddTabRequest`; Layers block [:3855](Source/Standalone/StandaloneEditor.cpp:3855), Bass block [:3893](Source/Standalone/StandaloneEditor.cpp:3893)) wired every per-tab callback EXCEPT `onSoundNameChanged` — the hook that renames the ribbon tab + mixer strip + shared piano-roll label on patch load. The startup-default tabs ([:1640](Source/Standalone/StandaloneEditor.cpp:1640) Layers / :1689 Bass), the Duplicate path (:1869 / :1922), AND the Drums add-path (:3947) all wire it — so only the FIRST-created Layers/Bass tab renamed; every "+"-added one had a null hook. Jeff's "engine-specific" symptom was coincidence (Harmless/BaySickBass happened to be on default tabs, BaySickSynth/BaySickPlayer on added tabs). Diagnosed via the silent file-log trace + Jeff's key observation: "only the first tab page created gets the name."
- **Fix:** added `p->onSoundNameChanged = [this, newId, pageIdx]{ renameTab + renameChannel + setEngineDisplayName }` to the Layers + Bass add-blocks, mirroring the initial-default form exactly ([:1640](Source/Standalone/StandaloneEditor.cpp:1640) / :1689). Drums already had it.
- **Process note:** chased "engine-specific" then "async race" before the trace + Jeff's tab-instance observation exposed the real (mundane) cause — a missing callback in one of four parallel tab-creation paths.
- **Next:** strip the 5 diagnostic file-log sites (surfaced for approval), then clean build + verify (added Layers/Bass tabs rename on patch load; first tabs + drums unaffected).
