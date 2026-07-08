# QA-UICleanup — Piano-Roll + Misc UI Cleanup — Plan (jolly-meandering-teapot)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/jolly-meandering-teapot.md`
> Paired running notes: `Plans & Specs/Running Notes/jolly-meandering-teapot.md`

> **For execution:** steps use `- [ ]` checkbox syntax. Builds run by Jeff (`do_build.bat`) — never by Claude. Verify in the Debug exe FIRST, then Release (CLAUDE.md Build System rule). Commit messages stay brief per §0 Rule 9 — write the one-liner directly, skip `/draft-commit`, surface message + full `git status`, commit on approval.

---

## Context

QA-UICleanup is the Phase-3 batch at §6-arrow slot 32 (`QA-CutSelfReview → **QA-UICleanup** → QA-Chords`). **Bucket:** UI / L&F / Theming. Scope per §5: *"piano-roll menu + toolbar consolidation; no DSP."* Seven items — a draggable quit dialog, a Layers/Bass auto-name bug, and a menu/toolbar consolidation that spans **two separate menu-bar models**: the engine piano-roll (`PianoRollMenuBar` in `PianoRoll.cpp`) and the drum-kit grid (`DrumKitGrid`).

Scope was frozen at the §9 forty-ninth Forks entry (QA-Ee close, 2026-06-05) and unchanged since — no findings routed in (verified against the 50th–54th entries). All 16 spec calls + the default-quantize value were resolved with Jeff in chat before this plan (table below).

**Dependencies:** none. QA-CutSelfReview closed at `aa77348`; working tree clean.

**Risk:** low-medium. No DSP, no audio thread. Real risks: (a) native-dialog result-index remap (item 1); (b) menu command-ID collisions when folding the tools popup into the menu bar (items 3/5/7); (c) the Snap control behavior change (item 4); (d) **item 2 root cause is unknown** — static analysis shows the rename hooks already fire and are wired identically to the working engine, so it needs runtime diagnosis. The two menu-bar models (`PianoRoll` + `DrumKitGrid`) must stay in lockstep for items 3/5/6 (drum-kit skips item 7).

**Effort estimate:** ~7–10 h. Task 4 (menu consolidation across both editors + a new APVTS param) is the heavy one; Task 2 is an unknown-until-diagnosed.

---

## Spec calls already locked (resolved in chat pre-plan)

| ID | Decision | Reasoning |
|----|----------|-----------|
| SC1 | Item 1: swap the quit/discard prompt to the **native** Windows dialog. | Guaranteed centered + non-draggable. Verified JUCE 8's native path uses `TaskDialogIndirect` with custom buttons ([juce_NativeMessageBox_windows.cpp:160-171](juce/modules/juce_gui_basics/native/juce_NativeMessageBox_windows.cpp)), so "Save / Don't Save / Cancel" survive. |
| SC2 | Fix the **shared** `confirmDiscardChanges` helper → quit + File→New + File→Open + New-from-Template all become native/centered. | One helper backs all callers; forking for quit-only is more work for less consistency. |
| SC3/SC4 | Item 2 is a **bug**, not new UI. Tab / mixer-strip / piano-roll labels fail to auto-name on patch load for **BaySickSynth + BaySickPlayer on Layers** and **BaySickPlayer on Bass**; **BaySickBass works**. No picker/name-tag involved. | Jeff's repro. The 3 surfaces already update via `onSoundNameChanged` → `ribbon.renameTab` + mixer + context-label; static analysis shows the hooks fire + wiring is parallel to BaySickBass, so the break is **runtime** — Task 2 diagnoses it (my earlier "editors don't fire the hook" was wrong). |
| SC5 | Item 3: drop the **7 tool-selectors** (Draw/Paint/Delete/Mute/Slice/Select/Zoom) from the menu-bar Tools menu (both editors). Tools stay reachable via toolbar buttons + keyboard P/B/D/T/C/E/Z. | Jeff — they duplicate the toolbar tool buttons. |
| SC6 | Item 3: move **all** items from the Tools-button popup into the menu-bar Tools menu (both editors), in the popup's exact order. | Jeff. Piano-roll popup = Quantize/Strum/Arpeggiate/Chop/Glue/Articulate/Randomize/Generate Chords; drum-kit popup = Quantize/Strum/Chop/Glue/Articulate/Randomize. |
| SC7 | Item 3: remove the toolbar wrench button **and** the right-click-on-grid tools popup (both editors). | Jeff. Tools now live only in the menu bar. |
| SC8 | Item 4: Snap keeps its **button look + on/off highlight** but left-click now opens the resolution **dropdown**; default **Line** (already the param default). Highlighted when snap ≠ Off. Both editors. | Jeff (option B). |
| SC9 | Item 4: move the Drum-Kit **Kit** button to the **far-right end** of its toolbar (like the preset buttons on the other players). Drum-kit only. | Jeff. |
| SC10 | Item 5/6: the new Quantize resolution is a **separate** setting from Snap. | Jeff (A). Value sets differ (snap = Off/Line/Bar/…/1-6 Step; quantize = 1/4,1/8,1/16,1/32). |
| SC11 | Item 5/6: the Quantize resolution **persists per-project**. | Jeff (A). Mirrors the per-project snap param. |
| SC12 | Item 7: fix is **display-only** — correct the menu shortcut text to the real bindings (Shift+Up/Down, Ctrl+Up/Down) + ASCII-ify the arrows. **Keys unchanged.** | Jeff (confirm). Key Binds window is canonical. |
| SC13 | Consolidated piano-roll Tools menu order: button-tools in popup order → `Quantize Settings` → 4 Transpose. | Jeff ("looks good"). |
| SC14 | Task split = **B**: Task 1 quit / Task 2 auto-name / Task 3 snap+kit / Task 4 menu-consolidation (3+5+6+7). One commit per task. | Jeff (B). Items 3/5/6/7 rewrite the same two menus → one task. |
| SC15 | Drum-kit gets items **3, 4, 5, 6**; **skips item 7** (no transpose — [DrumKitGrid.h:168-170](Source/Standalone/DrumKitGrid.h)); Arpeggiate + Generate Chords stay absent. | Jeff. |
| SC16 | The new Quantize resolution is **one shared value** across piano-roll + drum-kit (like Snap). | Jeff. |
| default | Fresh-project Quantize resolution defaults to **1/4**. | Jeff. |

## Sub-spec calls surfaced for ExitPlanMode

**None open.** All decisions above were resolved in chat before this plan. Any new sub-spec call found during execution (e.g. the item-2 root-cause fix shape, once diagnosed) surfaces via chat per §0 Rule 5 before landing in code.

---

## Files to modify

- **Task 1:** [StandaloneEditor.cpp:9181](Source/Standalone/StandaloneEditor.cpp) `confirmDiscardChanges()` (callers `requestAppQuit`:9216, `doFileNew`:9224 + Open/New-from-Template unchanged).
- **Task 2 (diagnose→fix):** trace across [BaySickSynthEditor.cpp:1126](Source/BaySickSynth/BaySickSynthEditor.cpp), [VibePlayerEditor.cpp](Source/VibePlayer/VibePlayerEditor.cpp) (firing sites 539/555/572/610/624/632/819), [LayersPage.cpp:180-193](Source/Standalone/LayersPage.cpp) + [BassPage.cpp:173-185](Source/Standalone/BassPage.cpp) wiring, [StandaloneEditor.cpp:1640-1642](Source/Standalone/StandaloneEditor.cpp) `onSoundNameChanged`→`renameTab`; reference the working [BaySickBassEditor.cpp:1087](Source/BaySickBass/BaySickBassEditor.cpp). Fix site TBD by diagnosis.
- **Task 3:** [PianoRoll.cpp:2617-2650](Source/Standalone/PianoRoll.cpp) `mMagnetBtn`; `DrumKitGrid.cpp:3137-3169` (same snap change) + `mKitBtn`:3224 / toolbar `resized()`:3575-3605. Ref [BuilderPage.cpp:5416-5432](Source/Standalone/BuilderPage.cpp), [VibesynthConstants.h:45](Source/VibesynthConstants.h).
- **Task 4:** [PluginProcessor.cpp:119](Source/PluginProcessor.cpp) (new param); [PianoRoll.cpp](Source/Standalone/PianoRoll.cpp) menu `:4017-4154`, tools popup `:3414-3450`, wrench `:2609`, `toolQuantize`:3452; `DrumKitGrid.cpp` menu `:3641-3718`, popup `:2341-2372`, wrench `:3131`, its `toolQuantize`.

---

## Tasks

### Task 0 — Open
- [ ] Mirror `~/.claude/plans/jolly-meandering-teapot.md` → `Plans & Specs/Batch Plans/jolly-meandering-teapot.md` (Write); delete the home-dir copy.
- [ ] Add `**Plan file:** …` + `**STATUS: OPEN**` to the Main Plan §5 QA-UICleanup entry.
- [ ] Seed `Plans & Specs/Running Notes/jolly-meandering-teapot.md` (title / purpose blockquote / pair + convention refs / "Task 0 — open" entry).
- [ ] Surface full `git status`; write the brief Task-0 commit one-liner; commit on approval.

### Task 1 — Item 1: native centered quit/discard prompt
Replace the draggable `AlertWindow` with the native `TaskDialog` (JUCE 8), keeping the custom button labels. The helper is shared, so this fixes quit + New + Open + New-from-Template at once.

```cpp
// StandaloneEditor.cpp:9188 — BEFORE (juce::AlertWindow — draggable via its own ComponentDragger):
juce::AlertWindow::showYesNoCancelBox (
    juce::MessageBoxIconType::WarningIcon, "Unsaved changes",
    "Save changes to '" + mProjectManager->getCurrentName() + "' first?",
    "Save", "Don't Save", "Cancel", this,
    juce::ModalCallbackFunction::create ([this, continuation = std::move (continuation)] (int result) {
        if (result == 0) return;                 // 1=Save, 2=Don't Save, 0=Cancel
        if (result == 1) { /* saveProject()… */ }
        if (continuation) continuation();
    }));

// AFTER — native Windows TaskDialog: centered, non-draggable, custom labels preserved:
juce::NativeMessageBox::showAsync (
    juce::MessageBoxOptions{}
        .withIconType (juce::MessageBoxIconType::WarningIcon)
        .withTitle   ("Unsaved changes")
        .withMessage ("Save changes to '" + mProjectManager->getCurrentName() + "' first?")
        .withButton ("Save").withButton ("Don't Save").withButton ("Cancel")
        .withAssociatedComponent (this),
    [this, continuation = std::move (continuation)] (int result) {
        // NOTE: MessageBoxOptions is 0-indexed in button order → 0=Save, 1=Don't Save, 2=Cancel.
        // CONFIRM the convention against juce_MessageBoxOptions.cpp before wiring the branches.
        if (result == 2) return;                 // Cancel
        if (result == 0) { /* saveProject(); abort-on-fail as today */ }
        if (continuation) continuation();
    });
```
- [ ] Read the JUCE `MessageBoxOptions` result convention, then rewrite `confirmDiscardChanges()` as above with the correct index mapping; preserve the save-failed abort path.
- [ ] **Tell Jeff (Debug):** (1) edit → close window (X): prompt is **centered, non-draggable, native**, buttons Save/Don't Save/Cancel. (2) Cancel → stays open. (3) Don't Save → quits, no save. (4) Save → saves then quits. (5) same centered/non-drag check via **File → New** and **File → Open** on a dirty project.
- [ ] Brief commit `QA-UICleanup Task 1: native centered quit/discard prompt (StandaloneEditor confirmDiscardChanges)` → surface + commit on approval → `/draft-doc running-notes`.

### Task 2 — Item 2: DIAGNOSE + fix Layers/Bass auto-naming
The hooks already fire and the wiring is parallel to the working engine — so this is a diagnosis, not a known fix. Do NOT assume a root cause.

```cpp
// The firing is IDENTICAL between working + broken engines:
//   BaySickBassEditor.cpp:1087  (WORKS)   if (onPatchLoaded) onPatchLoaded (f.getFileNameWithoutExtension());
//   BaySickSynthEditor.cpp:1126 (BROKEN)  if (onPatchLoaded) onPatchLoaded (f.getFileNameWithoutExtension());
// And both pages wire the same lambda (LayersPage.cpp:184-192 / BassPage.cpp:177-185):
auto onPatch = [this](const juce::String& name) {
    if (name.isEmpty()) return;
    setTabName (name);                                  // -> piano-roll context label
    if (onSoundNameChanged) onSoundNameChanged (name);  // -> ribbon.renameTab + mixer strip
};
// => static wiring is correct; the break is at runtime for BaySickSynth/BaySickPlayer.
```
- [ ] **Diagnose** (walk Jeff through per the debug rule): add a temporary diagnostic (AlertWindow or `DBG`) at (a) each engine's firing site and (b) inside the `onPatch` lambda + `onSoundNameChanged`→`renameTab`, to reveal for each engine whether the hook fires, whether the lambda runs, and where the chain stops. **Catalog the diagnostic in running notes per §0 Rule 4 in the same edit pass.** Likely hypotheses to test first: a second load path in the Synth/Player editors that skips the hook; the user's actual load gesture not reaching line 1126; or an empty/failed `name`.
- [ ] Present the finding to Jeff; confirm the fix shape (may be a new sub-spec call per §0 Rule 5) before implementing.
- [ ] Implement the fix; re-verify; **strip the diagnostic** (surface the strip list first).
- [ ] **Tell Jeff (Debug):** (1) Layers + BaySickSynth → load a patch → tab + mixer + piano-roll labels all rename. (2) Layers + BaySickPlayer → load a sample/SFZ → same. (3) Bass + BaySickPlayer → same. (4) Bass + BaySickBass still renames (regression). (5) Layers + Harmless → renames.
- [ ] Brief commit `QA-UICleanup Task 2: fix Layers/Bass patch-load auto-rename (<root cause>)` → surface + commit on approval → `/draft-doc running-notes`.

### Task 3 — Item 4: snap dropdown (both editors) + kit button to right end
```cpp
// Snap — PianoRoll.cpp:2619-2649 BEFORE: left-click toggles Off<->last; right-click picks resolution.
mMagnetBtn->setClickingTogglesState (true);
mMagnetBtn->onClick = [this] { /* toggle global snap Off <-> last div */ };
mMagnetBtn->onRightMouseDown = [this](const MouseEvent&) { /* 11-label resolution popup, writes snap div */ };

// AFTER (SC8=B): left-click OPENS the resolution dropdown; the toggle state is now just the highlight.
mMagnetBtn->setClickingTogglesState (false);
mMagnetBtn->onClick = [this] { showSnapMenu(); };   // <- the old onRightMouseDown body, extracted
// after any snap change (in showSnapMenu's callback + on external sync):
mMagnetBtn->setToggleState (div != 0, juce::dontSendNotification);   // Off = not highlighted
```
```cpp
// Kit button — DrumKitGrid.cpp ~:3600 toolbar resized(), row1 laid out left->right.
// BEFORE: pinned left, just after the zoom buttons.
if (mKitBtn) mKitBtn->setBounds (row1.removeFromLeft (46).reduced (2, 3));
// AFTER: pin to the far-right END first (before the context label fills the middle).
if (mKitBtn) mKitBtn->setBounds (row1.removeFromRight (46).reduced (2, 3));
```
- [ ] Piano-roll snap: extract the resolution popup to `showSnapMenu()`, repoint `onClick` to it, drop the toggle branch, drive the highlight from `div != 0`. Keep "Snap" label + existing highlight styling.
- [ ] Drum-kit snap: same change at `DrumKitGrid.cpp:3137-3169`.
- [ ] Drum-kit `mKitBtn` → far-right via `removeFromRight`; context label fills the space to its left.
- [ ] **Tell Jeff (Debug):** (1) Layers roll — Snap now a dropdown; pick Beat → snaps + highlighted; Off → not highlighted; fresh roll = Line. (2) same on Drum-Kit. (3) Kit button far-right. (4) shared value: set Beat on the roll → drum-kit shows Beat.
- [ ] Brief commit `QA-UICleanup Task 3: snap toggle-button -> resolution dropdown (PianoRoll + DrumKitGrid) + drum-kit kit button to right end` → surface + commit on approval → `/draft-doc running-notes`.

### Task 4 — Items 3+5+6+7: menu-bar consolidation + Quantize Settings param
```cpp
// (5/6) New APVTS param — PluginProcessor.cpp, beside Unified_BuilderSnapDiv (:119):
addI ("Unified_BuilderSnapDiv", "Builder Snap Division", 0, 10, 1);   // existing (1 = Line)
addI ("Unified_QuantizeDiv",    "Quantize Division",     0,  3, 0);   // NEW: 0..3 -> 1/4,1/8,1/16,1/32, default 1/4
```
```cpp
// (3+5+7) getMenuForIndex, idx==1 (Tools) — AFTER (piano-roll). Replaces the 7 addTool(21..27) selectors.
menu.addItem (60, "Quantize\tAlt+Q");   menu.addItem (61, "Strum\tAlt+S");   menu.addItem (62, "Arpeggiate\tAlt+A");
juce::PopupMenu chop; /* 70..74 Into 2/3/4/6/8 */ menu.addSubMenu ("Chop", chop);
menu.addItem (63, "Glue\tCtrl+G");      menu.addItem (64, "Articulate\tAlt+L");
menu.addItem (65, "Randomize\tAlt+R");  menu.addItem (66, "Generate Chords\tAlt+P");
menu.addSeparator();
juce::PopupMenu qs; const int qd = mOwner.getQuantizeDiv();       // radio-checked to Unified_QuantizeDiv
qs.addItem (110, "1/4", true, qd==0); qs.addItem (111, "1/8", true, qd==1);
qs.addItem (112, "1/16", true, qd==2); qs.addItem (113, "1/32", true, qd==3);
menu.addSubMenu ("Quantize Settings", qs);
menu.addSeparator();
menu.addItem (7,  "Transpose Up\tShift+Up");        menu.addItem (8,  "Transpose Down\tShift+Down");
menu.addItem (9,  "Transpose Up Octave\tCtrl+Up");  menu.addItem (10, "Transpose Down Octave\tCtrl+Down");
```
```cpp
// menuItemSelected — route the folded tools (were in showToolsMenu's async cb); REMOVE the 21..27 + 101..104 blocks.
else if (id == 60) { if (auto* g=o.mGrid.get()) g->toolQuantize(); }     // 61 Strum … 65 Randomize, 66 GenerateChords
else if (id >= 70 && id <= 74) { /* toolChop(2/3/4/6/8) */ }
else if (id >= 110 && id <= 113) { o.setQuantizeDiv (id - 110); }        // (5) set resolution, NO quantize
// Transpose 7..10 unchanged (relocated only). Edit menu (idx==0) drops items 7..10 + the 101..104 submenu.
```
```cpp
// (6) PianoRollGrid::toolQuantize (:3458) — BEFORE: quantizes to the SNAP unit.
double snap = snapUnitBeats();
// AFTER: quantize to the Quantize Settings resolution.
double snap = quantizeUnitBeats();   // maps Unified_QuantizeDiv 0..3 (1/4,1/8,1/16,1/32) -> beats
```
- [ ] Register `Unified_QuantizeDiv` + add `getQuantizeDiv()/setQuantizeDiv()` accessors reachable by both menu bars + grids (parallel to the snap accessors; shared value per SC16, per-project per SC11).
- [ ] **Item 3 (both editors):** fold the popup tools into the Tools menu (exact popup order) with the new ID block; drop selectors 21-27; remove `mWrenchBtn` + its layout; remove the right-click→`showToolsMenu` path (confirm right-click has no other role first).
- [ ] **Item 5 (both):** add `Quantize Settings ▸` (radio-checked); remove the Edit→Quantize submenu (101-104) from both Edit menus.
- [ ] **Item 6 (both):** point `toolQuantize()` at `Unified_QuantizeDiv` via `quantizeUnitBeats()`.
- [ ] **Item 7 (piano-roll only):** relocate the four Transpose items to the Tools menu with the ASCII/real-binding strings above. **Do not touch the key handler** (PianoRoll.cpp:1086-1089).
- [ ] **Drum-kit:** same as above minus Arpeggiate, Generate Chords, and the Transpose block (its popup has no 62/66; it has no transpose).
- [ ] **Tell Jeff (Debug), Layers roll:** (1) wrench gone; right-click no longer opens the tools popup. (2) Tools menu = 8 tools → Quantize Settings → 4 Transpose; NO Draw/Paint/…/Zoom. (3) Edit menu has no Quantize/Transpose. (4) Quantize Settings→1/8 sets resolution (no note move); Quantize then moves the selection to the 1/8 grid. (5) Transpose shows Shift+Up/Down + Ctrl+Up/Down and the keys still work. (6) Drum-Kit: same Tools menu minus Arpeggiate/Generate Chords/Transpose; Edit has no Quantize. (7) set 1/16 on the roll → drum-kit also 1/16 (shared). (8) save + reopen → persists; fresh project → 1/4.
- [ ] Brief commit `QA-UICleanup Task 4: fold tools popup into menu-bar Tools + Quantize Settings resolution param + transpose glyph/shortcut fix (PianoRoll + DrumKitGrid)` → surface + commit on approval → `/draft-doc running-notes`.

### Task 5 — Close
- [ ] `/draft-doc batch-close` from running notes → review → apply to `Implemented Work Log.md` via Edit.
- [ ] `/review-batch QA-UICleanup` → address BLOCKER/NEEDS-FIX in-batch; defer NITs into the close entry.
- [ ] Confirm all `Remove`-tagged diagnostics (Task 2) are stripped (surface the list first).
- [ ] Route side findings (Rule 3): in-batch → close-entry table; out-of-batch → §9 Forks + §5/§6/Future-State edits (surface slot options to Jeff, don't pick).
- [ ] Update §5 QA-UICleanup `STATUS: CLOSED` + Work Log pointer + "Next batch: QA-Chords".
- [ ] Surface full `git status`; brief close-commit one-liner; commit on approval (separate from source commits).

---

## Verification (end-to-end smoke, after Task 4)
1. **Build clean** — `do_build.bat` Release + Debug both green.
2. **Quit prompt** — dirty project → X / File→New / File→Open all show the native centered non-draggable Save/Don't-Save/Cancel.
3. **Auto-name** — patch load renames tab + mixer + roll on BaySickSynth/BaySickPlayer (Layers) + BaySickPlayer (Bass); BaySickBass unchanged.
4. **Snap** — dropdown + highlight on both editors; shared value; drum-kit Kit button far-right.
5. **Menus** — both Tools menus consolidated, selectors gone, wrench + right-click popup gone; Edit menus stripped of Quantize (+ Transpose on piano-roll).
6. **Quantize** — Quantize Settings sets resolution; Quantize action honors it; shared across editors; persists per-project; fresh = 1/4.
7. **Transpose** — ASCII shortcuts match Key Binds (Shift/Ctrl + Up/Down); keys still transpose.

---

## Routing notes (Rule 3 during execution)
- UI/menu/toolbar findings scoped to these seven items → fold in-batch, log in the close-entry table.
- Anything outside the seven → §9 Forks + surface the §5/§6 slot options to Jeff (don't pick).
- **Known-done, don't re-trip:** the engine-editor knob double-click factory-default fix already landed in `SharedUI.cpp` at QA-ClipPlayback (`86e17d0`) — Task 3/4 toolbar/knob work must not disturb it.
- Rule-4: every temp `DBG`/`Logger`/`jassert`/AlertWindow trace (esp. Task 2) gets a Diagnostic Instrumentation Catalog row in running notes in the same edit pass; strip `Remove` rows at task/batch close (surface list first).

## Carry-Forward Reference touch points
- §6 "ASCII-only UI strings" — item 7 glyph fix + every new menu string.
- §6 "Lock-after-pick for picker buttons" — context for item 2's rename plumbing (bug fix, not new picker).
- §8 anti-patterns — ASCII-only; diagnose-before-fixing (Task 2 is explicitly a diagnosis); read-before-claiming (already applied — verified the AlertWindow drag cause, JUCE 8 TaskDialog, and that item-2 hooks already fire, before planning).
