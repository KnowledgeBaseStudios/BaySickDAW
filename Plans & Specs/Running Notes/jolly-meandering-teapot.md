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
- **DONE — committed `b48e2d5`.** Diagnostics stripped (Jeff-approved, grep-clean); clean build verified all-pass (Debug + Release): added Layers/Bass tabs now rename on patch load (ribbon + mixer + piano-roll); first tabs + drums regression-clean.

## 2026-07-08 — Task 3 — snap dropdown (both editors) + drum-kit Kit button

- **SC8 (snap -> dropdown):** both `PianoRollContainer` + `DrumKitContainer` "Snap" button now opens the 11-value resolution dropdown on left-click (was: left-click on/off toggle + right-click resolution picker). Button highlight still reflects Off (dim) vs any active division; synced to the live shared div in `setSnapAccessors`; default Line = highlighted. Removed `onRightMouseDown` + `setClickingTogglesState`.
- **SC9 (Kit button):** `DrumKitContainer::resized()` pins the `Kit` button to the far-right end (`removeFromRight`); context label fills the space to its left.
- **Clean-in-batch:** removed `mLastSnapDiv` (member + all writes, both editors) — its only reader was the on/off toggle-restore that SC8 removed. Grep-confirmed 0 references.
- Snap stays a shared global (`Unified_BuilderSnapDiv`, read live via `onGetSnapDiv`) — no change to snapping itself, only the control. Highlight syncs on `setSnapAccessors` + local menu pick (not a live cross-editor poll; matches prior behavior).
- **Live highlight sync added (Jeff request after first verify):** both `PianoRollContainer` + `DrumKitContainer` now inherit `juce::Timer` (`startTimer(200)`); `timerCallback()` re-syncs the Snap highlight to the live shared div, guarded by `isShowing()` so only the visible roll polls. Covers cross-editor changes (change snap on the roll -> drum-kit highlight follows within ~200ms). `setToggleState` repaints only on an actual change, so the idle poll is effectively free. No explicit `stopTimer` needed (Timer auto-stops on destruct; single-threaded teardown = no callback race).
- **First verify: all pass** (snap dropdown + highlight on both toolbars; Kit far-right; shared snapping) — live highlight was the one add-on request.
- **DONE — committed `2a077da`.** Live highlight re-verified all-pass (cross-editor sync works); snap dropdown + Kit-right + `mLastSnapDiv` removal all shipped.

## 2026-07-08 — Task 4 — CARRY-OVER (mapped, NOT yet implemented)

**Status:** Tasks 0-3 done + committed (`307a211` open / `4845e50` T1 quit / `b48e2d5` T2 auto-name / `2a077da` T3 snap+kit). Task 4 (items 3+5+6+7) is fully mapped (Explore agent 2026-07-08) but NOT started. Working tree clean at Task 3.

**Two map corrections (verified):**
- **SC7 moot — there is NO right-click tools popup.** The wrench button (`mWrenchBtn->onClick`) is the ONLY caller of `showToolsMenu()` in both grids (full-tree grep). Right-click = erase/zoom. So item 3 = remove the wrench button + fold its popup into the menu-bar Tools menu; nothing else to remove.
- The piano-roll/drum-kit snap threads through **`Unified_PianoRollSnapDiv`** (not `Unified_BuilderSnapDiv`) via `setSnapAccessors`. Clone THAT chain for the new `Unified_QuantizeDiv`.

**Implementation plan (4 stages, ONE commit per SC14=B):**
- **Stage 1 — new param + accessor chain** (mirror `Unified_PianoRollSnapDiv` exactly):
  - Register: `PluginProcessor.cpp:124` -> add `addI("Unified_QuantizeDiv", "Quantize Division", 0, 3, 0);` (0..3 -> 1/4,1/8,1/16,1/32, default 0=1/4). `addI` helper at :78-83.
  - APVTS read/write lambdas: `StandaloneEditor.cpp:1472-1482` -> add a parallel `mPianoRollPage->setQuantizeAccessors(getter,setter)` reading/writing `Unified_QuantizeDiv`.
  - Page fan-out: `PianoRollPage.cpp:141-148` (`setSnapAccessors`) + re-apply at `:117` (registerEngine) + decl `PianoRollPage.h:109`; add `setQuantizeAccessors` + members `mQuantizeGetter/mQuantizeSetter`.
  - Container store + push getter to grid: `PianoRoll.cpp:3241-3252` / `DrumKitGrid.cpp:3534-3545`; members `PianoRoll.h:660-661` / `DrumKitGrid.h:491-492`; decls `PianoRoll.h:582` / `DrumKitGrid.h:445`. Add `mOnGetQuantizeDiv/mOnSetQuantizeDiv` + `setQuantizeAccessors` (push getter to `mGrid->onGetQuantizeDiv`).
  - Grid read hook: add `onGetQuantizeDiv` next to `onGetSnapDiv` (`PianoRoll.h:200` / `DrumKitGrid.h:199`).
- **Stage 2 — item 6, toolQuantize honors the quantize div** (sever the snap coupling): `PianoRollGrid::toolQuantize` `PianoRoll.cpp:3454-3469` + `DrumKitGrid::toolQuantize` `DrumKitGrid.cpp:2374-2395` -> replace `double snap = snapUnitBeats();` with a quantize unit from `onGetQuantizeDiv` (div 0..3 -> beats `1.0 / (1 << div)` = 1, 0.5, 0.25, 0.125). Keep `snapUnitBeats` for actual snapping.
- **Stage 3 — menu restructure (items 3+5+7), both editors:**
  - Tools menu `getMenuForIndex` (piano-roll `PianoRoll.cpp:4050-4064` idx==1; drum-kit `DrumKitGrid.cpp` Tools block): DROP the 7 tool-selectors (ids 21-27); ADD the folded popup tools in `showToolsMenu` order (piano-roll `:3416-3452` = Quantize/Strum/Arpeggiate/Chop>/Glue/Articulate/Randomize/GenerateChords; drum-kit `:2341-2372` = same minus Arpeggiate+GenerateChords) + a "Quantize Settings" submenu (1/4,1/8,1/16,1/32, radio-checked to `Unified_QuantizeDiv`) + (piano-roll only) the 4 Transpose items with ASCII/real-binding text (`Transpose Up  Shift+Up`, `Down  Shift+Down`, `Up Octave  Ctrl+Up`, `Down Octave  Ctrl+Down`).
  - Edit menu: REMOVE the Quantize submenu (build `PianoRoll.cpp:4039-4044` / `DrumKitGrid.cpp:3662-3667`; dispatch `:4124-4128` / `:3704-3708`) AND (piano-roll) the 4 Transpose items (`PianoRoll.cpp:4045-4048`, dispatch ids 7-10 at `:4117-4120`).
  - `menuItemSelected` (`PianoRoll.cpp:4106-4154` / `DrumKitGrid.cpp:3700-3718`): route the new Tools-menu ids -> `g->toolQuantize()/toolStrum()/...` (currently in `showToolsMenu`'s async cb) + Quantize Settings -> `setQuantizeDiv` (NO immediate quantize) + keep Transpose 7-10 (relocated). Assign a non-colliding id block (Edit uses 1-10, selectors 21-27, Quantize 101-104, Scale 201+/301+, Chords 401+, View 51-57 -> use e.g. 60-74 for folded tools, 110-113 for Quantize Settings).
  - **Item 5/6 sever:** `setSnapDenomAndQuantize` (`PianoRoll.cpp:3230-3237` / `DrumKitGrid.cpp:3523-3530`) is now unused (Edit>Quantize removed) -> remove it (clean-in-batch) OR repoint. Confirm no other caller.
- **Stage 4 — remove the wrench button** (`mWrenchBtn`) from both toolbars: setup `PianoRoll.cpp:2609-2614` / `DrumKitGrid.cpp:3129-3134` + its `resized()` layout slot + the header member. `showToolsMenu()` becomes dead -> remove it too (clean-in-batch).

**Consolidated Tools menus (SC13/SC15 locked):**
- Piano-roll: Quantize / Strum / Arpeggiate / Chop> / Glue / Articulate / Randomize / Generate Chords -- separator -- Quantize Settings> -- separator -- Transpose Up/Down/Up Octave/Down Octave.
- Drum-kit: Quantize / Strum / Chop> / Glue / Articulate / Randomize -- separator -- Quantize Settings> (no Arpeggiate, no Generate Chords, no Transpose).

**Item 7 detail:** current Transpose strings (`PianoRoll.cpp:4045-4048`) use UTF-8 arrow glyphs (U+2191/2193) + the two Octave entries DISPLAY `Shift+Ctrl+arrow` but the real binding is `Ctrl+Up/Down` (key handler `:1086-1089`). Fix = display-only: `Shift+Up`/`Shift+Down`/`Ctrl+Up`/`Ctrl+Down`, ASCII. DO NOT touch the key handler.

**Resume action:** start Stage 1 (register `Unified_QuantizeDiv` + clone the accessor chain), Debug-build after each stage optional; single verify + commit at Task 4 end.

- **Next (pending Jeff):** push through Task 4 now, or resume fresh (context checkpoint).

## 2026-07-08 — Task 4 — DONE (menu consolidation: items 3+5+6+7)

Items 3+5+6+7 shipped as ONE commit (SC14=B), built in the 4 mapped stages. Working tree was clean at Task 3, so this is the whole diff since `2a077da`. Verified all-pass Debug + Release (8 scenarios below).

- **Stage 1 — new param + accessor chain (SC10/SC11/SC16).** Registered `Unified_QuantizeDiv` (Int 0..3 -> 1/4,1/8,1/16,1/32 note, default 0=1/4) in `PluginProcessor.cpp` right after `Unified_PianoRollSnapDiv` (`addI` helper) — decoupled from snap (SC10), ONE param shared across every piano roll + the drum kit (SC16), per-project (SC11). Cloned the `Unified_PianoRollSnapDiv` accessor chain *exactly*: `StandaloneEditor` apvts read/write lambdas -> `PianoRollPage::setQuantizeAccessors` + `mQuantizeGetter/mQuantizeSetter` (+ re-apply in `registerEngine` so future rolls inherit) -> `PianoRollContainer`/`DrumKitContainer::setQuantizeAccessors` + `getQuantizeDiv`/`setQuantizeDiv` + `mOnGetQuantizeDiv`/`mOnSetQuantizeDiv` -> grid `onGetQuantizeDiv` read hook (added next to `onGetSnapDiv`).
- **Stage 2 — item 6: quantize honors its own div (sever the snap coupling).** Both `PianoRollGrid::toolQuantize` + `DrumKitGrid::toolQuantize` now round the selection to the quantize unit `1.0/(1<<div)` beats (1, 0.5, 0.25, 0.125), read live via `onGetQuantizeDiv`, replacing the old `snapUnitBeats()` round. `snapUnitBeats` stays for actual snapping — quantize + snap are now independent controls.
- **Stage 3 — items 3+5+7: menu restructure, both editors.**
  - Made the grid tool algorithms public (joining the already-public `toolQuantize`) so `*MenuBar::menuItemSelected` can route to them via `mGrid`.
  - Folded the wrench popup into the menu-bar **Tools** menu in exact popup order: piano-roll = Quantize / Strum / Arpeggiate / Chop> / Glue / Articulate / Randomize / Generate Chords (ids 60-66 tools, 70-74 chop); drum-kit = same minus Arpeggiate + Generate Chords (SC15).
  - Added a `Quantize Settings>` radio submenu (ids 110-113, radio-checked to `Unified_QuantizeDiv` via `getQuantizeDiv`) that sets the resolution ONLY — no immediate quantize.
  - Relocated the 4 Transpose items into piano-roll Tools (ids 7-10 unchanged) with ASCII/real-binding display text: `Shift+Up` / `Shift+Down` / `Ctrl+Up` / `Ctrl+Down` (item 7 = display-only fix; the U+2191/2193 glyphs + the wrong "Shift+Ctrl" octave labels are gone). Key handler at `PianoRoll.cpp:1086-1089` NOT touched.
  - Dropped the 7 tool-selectors (ids 21-27) from both Tools menus (SC5 — they dup the toolbar buttons + the P/B/D/T/C/E/Z keys); the `using T` blocks they needed went with them.
  - Removed the `Edit>Quantize` submenu (ids 101-104) from both Edit menus + `Edit>Transpose` from the piano-roll Edit menu.
- **Stage 4 — wrench button + dead-code sweep.** Removed the Tools wrench button (`mWrenchBtn`: setup + `resized()` slot + header member) from both toolbars — the Snap (magnet) button slides into its old slot. Removed the now-dead `showToolsMenu()` and `setSnapDenomAndQuantize()` (decls + defs, both editors) — clean-in-batch. Fixed a stale `mMagnetBtn` header comment ("snap toggle + right-click res." -> "snap resolution dropdown", a Task-3 SC8 leftover) in the edited region per Rule 6.
- **Two carry-over map corrections confirmed in code during execution:**
  - **SC7 moot** — there was NO right-click tools popup; the wrench was `showToolsMenu`'s ONLY caller, so removing it left nothing else to strip. Right-click stays erase/zoom.
  - The snap/quantize accessors thread through **`Unified_PianoRollSnapDiv`** (not `Unified_BuilderSnapDiv`); cloned that chain.
- **Implementation call (Rule 8 — NOT a spec call).** Made the grid tool methods public rather than `friend`-ing the grid into the menu bar — matches the existing public `toolQuantize` precedent (already public for exactly this reason). `getActiveTool` kept as a symmetric public accessor, paired with the still-used `setActiveTool` (its only prior caller was the dropped selector checkmarks).
- **Self-review (inline self-check, no per-unit review agent):** grep-clean of `mWrenchBtn` / `showToolsMenu` / `setSnapDenomAndQuantize` = 0 refs; `Unified_QuantizeDiv` chain wired end-to-end (param -> editor lambdas -> page -> container -> grid hook); no dangling `using T` left from the dropped selector blocks.
- **Diagnostics:** none added this task — nothing for the Diagnostic Instrumentation Catalog.
- **Files:** `PluginProcessor.cpp`, `Standalone/StandaloneEditor.cpp`, `Standalone/PianoRollPage.h/.cpp`, `Standalone/PianoRoll.h/.cpp`, `Standalone/DrumKitGrid.h/.cpp`.
- **VERIFIED (Debug + Release, all 8 scenarios pass):** (1) wrench button gone from both toolbars; (2) Tools menu consolidated, tool-selectors gone; (3) Edit menus stripped (no Quantize submenu / no Transpose on piano-roll); (4) Quantize Settings sets the resolution without moving notes, Quantize then honors it; (5) Transpose ASCII shortcuts show + the real keys still work; (6) drum-kit Tools menu = piano-roll minus Arpeggiate / Generate Chords / Transpose; (7) the shared quantize value tracks cross-editor (set it on the roll -> drum-kit menu radio follows); (8) persists per-project, fresh session defaults to 1/4.
- **DONE — committed as `104875f` (Task 4 source + this notes entry).**
- **Next:** Task 5 (batch close) — `/draft-doc batch-close` -> apply to Implemented Work Log -> `/review-batch QA-UICleanup` -> fix any BLOCKER / NEEDS-FIX in-batch (fold everything; Jeff: no routing out) -> Main Plan §5 QA-UICleanup `STATUS: CLOSED` + "Next batch: QA-Chords" -> close commit.

## 2026-07-08 — Task 5 — batch CLOSED

- `/draft-doc batch-close` -> compiled the QA-UICleanup Implemented Work Log entry (header 2026-07-08 15:04 PT); applied verbatim via Edit (parent filled the timestamp + `/review-batch` outcome + commit placeholder).
- `/review-batch QA-UICleanup` (`batch-code-reviewer`) -> **READY-TO-COMMIT**: 0 BLOCKER, 0 NEEDS-FIX, 1 NIT. Verified clean: menu-ID integrity (no collisions, every id handled both ways), the `Unified_QuantizeDiv` chain (byte-for-byte clone of the verified snap chain), Task 1 native-dialog button mapping (0=Save / 1=Don't-Save / 2=Cancel, Escape/X -> safe abort), ASCII-only, dead-code hygiene (0 refs to the removed symbols), SharedUI knob double-click fix undisturbed.
- **NIT FIXED in the close commit (not deferred):** a Rule-6 keeper comment in `StandaloneEditor.cpp` (Task 1 quit dialog) read "centered + non-draggable" — corrected to reflect the SC1 finding (native TaskDialog is title-bar movable; native only drops the AlertWindow click-body drag). A factually-wrong keeper comment gets fixed, not shipped.
- **`getActiveTool`** intentional keep confirmed (reviewer did not flag it as a finding).
- **Routing:** NONE out of batch (Jeff's standing decision — a separate grouped-batch set covers the rest). No §9 Forks entry; no §6 arrow change (CLOSED is tracked in §5).
- **Next-batch correction:** the §6 arrow was re-pointed 2026-07-08 by the bulk-run insertion (`387f02f` / fifty-fifth Forks entry) so **QA-TransportDisplay** now sits between QA-UICleanup and QA-Chords. Main Plan §5 QA-UICleanup docket -> **STATUS: CLOSED (2026-07-08)** + Work Log pointer + **Next batch: QA-TransportDisplay** (supersedes the stale "QA-Chords" in the Task 4 entry's Next line above, which predated the re-sequence).
- **Housekeeping:** `Documents/BaySickDAW/qa_rename_trace.txt` (Task 2 diagnostic file) can be deleted.
- **Batch complete.** Close commit: `<pending Jeff approval>`.
