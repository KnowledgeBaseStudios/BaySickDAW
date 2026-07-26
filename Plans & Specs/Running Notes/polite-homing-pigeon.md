# Running Notes — QA-NativeDialogs (polite-homing-pigeon)

> Append-only mid-batch log. Entries land at every checkpoint (commit landed / finding captured
> / spec call resolved / scope pivot) per `feedback_draft_doc_running_notes_every_checkpoint.md`.
> At code-complete, `/draft-doc batch-close` consumes this file to compile the Implemented Work
> Log entry, which is HELD here (bulk-run R2) and applied at the batch's campaign section pass.
>
> Pair file: [`Plans & Specs/Batch Plans/polite-homing-pigeon.md`](../Batch%20Plans/polite-homing-pigeon.md).
> Conventions: Main Plan §0 (Batch Plans + Running Notes layout, locked 2026-05-11).

## 2026-07-25 — Task 1 — native Open Project + Quick Open Project

- `doFileOpen` now opens a native Windows FOLDER picker rooted at
  `ProjectManager::getDefaultProjectsRoot()` (house `make_shared<juce::FileChooser>` +
  `launchAsync` pattern, `openMode | canSelectDirectories`), still inside the existing
  `confirmDiscardChanges` wrap. A project IS a folder, so a directory picker is the right shape.
- **Load-bearing ordering:** `project.xml` is validated BEFORE the teardown
  (`closeAllDynamicTabs` / `clearDynamicStrips` / `resetToBlankState`). The old browser never
  needed that check — it only ever listed folders that already contained a `project.xml`, so
  teardown-first was safe by construction. A native picker can land anywhere and the teardown is
  not undoable. This is the one real hazard the batch introduces; §B.26 ND-2 is its MUST-PASS test.
- Browser moved intact to `doFileQuickOpen()` (File menu id **110**, verified unused). Every
  feature untouched — sort, rename, duplicate, delete-to-Recycle-Bin, Show in Explorer, and the
  refuse-destructive-ops-on-the-open-project guards. Only the entry point + `opts.dialogTitle`
  changed.
- **Plan deviation:** `ProjectBrowserWindow.h/.cpp` NOT touched. The plan predicted a window-title
  parameter, but the title already comes from `opts.dialogTitle` at the launch site.
- Build gate: BOTH configs clean.

## 2026-07-25 — Task 2 — default folders + resolvers + dead code

- Set Default Template: both `factoryTemplatesDir()` and `userTemplatesDir()` created before the
  chooser (docket 2a); start dir stays the Templates root.
- Deleted the dead 26-line ProjectBrowserWindow block behind `doFileSetDefaultTemplate`'s
  unconditional `return`. Live `new ProjectBrowserWindow()` sites: 3 (one unreachable) -> 2.
- MIDI CC import start dir -> `Documents\BaySickDAW\MIDI` (created on demand).
- Builder Import Audio start dir -> `SampleLibrary::getUserSamplesDir()` (+ `ensureUserSamplesDir()`),
  matching the Clip "+"-add path. New `#include "../SampleLibrary.h"`.
- New shared resolvers `EffectPresetIO::userNamPedalsDir()` + `irDir()`. Verified `presetsRoot()`
  is `Documents/BaySickDAW/Presets/Effects`, so every pre-existing inline literal already resolved
  to the same place — pure centralization, NOT a folder move. Three NAM sites and both IR
  empty-state fallbacks now consume them. New `#include "EffectPresetIO.h"` in
  `EffectEditorPanels.cpp`.
- **These are two SEPARATE folders and stay separate** (Jeff, explicit, 2026-07-25):
  NAM pedals = `Presets\Effects\Pedals\User NAM Pedals`; IRs = `Presets\Effects\IR`.
- `irDir()` was a judgment call slightly past the plan's letter (the plan named only the NAM
  literal for centralization). Surfaced to Jeff; he confirmed the only thing he cared about was
  that the two folders stay distinct, which they do. Kept as-is.
- Build gate: BOTH configs clean.

## 2026-07-25 — process — permission-prompt friction resolved

- Root cause was NOT missing allowlist entries. Claude Code already auto-allows `git show/diff/log`,
  `grep`, `rg`, `sed`, `find`, `ls`, `cat`, `head`, `tail`, `wc`. I defeated that by CHAINING them
  (`cd X && grep ... && sed ...`) — a compound command with redirection is refused as a
  path-traversal guard and is deliberately un-allowlistable. Fix is behavioral: use Read/Grep/Glob,
  keep any real shell call to ONE simple command. Written into CLAUDE.md + memory.
- The pinned build string was also compound (`...do_build.bat"; Write-Output "$LASTEXITCODE"`) so
  "always allow" could never match it. Simplified to a single statement; the `Write-Output` half
  was useless anyway since the protocol reads the exit codes from `build_log.txt`.
- Subagents are the residual leak — they author their own compound commands. Dispatch sparingly.
- `.claude/settings.local.json` pruned 297 -> 3 entries on Jeff's call (option B). Removed seven
  `powershell -Command ":*"` arbitrary-execution wildcards, `Bash(git add *)`, several
  `Remove-Item -Force` rules, `Read(//tmp/**)`, and ~280 dead `/tmp/*.ps1` + Vibesynth-era entries.
  Backup at `.claude/settings.local.json.bak-20260725` (the file is gitignored — no other undo).

## Held Work Log entry (apply at section pass)

> Drafted at code-complete via `/draft-doc batch-close`; NOT yet applied to `Implemented Work Log.md`.
> Applies with the §5 STATUS flip when §B.26 passes the campaign walk (R2). Commit hash is filled
> (`f4112b17`); stamp the full `HH:MM PT` at apply.

### 2026-07-25 <HH:MM> PT — QA-NativeDialogs — Open Project moved off the custom `ProjectBrowserWindow` onto a native Windows FOLDER picker, with `project.xml` validated BEFORE the (non-undoable) session teardown; the browser survives intact as a new "Quick Open Project" File-menu item (id 110); four chooser start-dirs repointed (Templates both-subfolders-reachable, MIDI import, Builder audio import -> My Samples, IR fallback); two path literals centralized behind new `EffectPresetIO::userNamPedalsDir()` + `irDir()` resolvers; the 26-line dead ProjectBrowserWindow block deleted; the batch's founding "convert the choosers to native" premise was already void — all 18 pre-batch `juce::FileChooser` sites were native to begin with

**Bucket:** System Pages, Effects. Batch `polite-homing-pigeon`. `blocks:` `f4112b17`.
*(Main Plan §5 had pre-assigned "UI / L&F / Theming, System Pages" back when this batch was framed
as a look-and-feel sweep; nothing in the diff touches LAF, palette, theme or layout. The §5 Bucket
line was corrected in place on Jeff's call, 2026-07-25.)*

#### Done

- **Task 1 — native Open Project.** `doFileOpen` opens a native Windows FOLDER picker rooted at `ProjectManager::getDefaultProjectsRoot()` (created on demand), built with the house `std::make_shared<juce::FileChooser>` + `launchAsync` pattern and `openMode | canSelectDirectories`, still wrapped in the existing `confirmDiscardChanges` continuation so the dirty-project prompt is unchanged. A project IS a folder, so a directory picker is the right shape — a file-picker-for-`project.xml` would force a descend into every candidate folder. Cancel returns silently.
- **Task 1 — validation-before-teardown (the load-bearing part).** The success path is byte-for-byte the old browser continuation (`HeavyOperationOverlay::ScopedOp` -> `closeAllDynamicTabs()` -> `clearDynamicStrips()` -> `resetToBlankState()` -> `openProject` -> `restoreAudioStripsFromArrangement()` -> `refreshWindowTitle()`), but a `folder.getChildFile("project.xml").existsAsFile()` check now runs AHEAD of it and bails with "Not a BaySickDAW project folder." The old browser never needed that check — it only listed folders that already contained a `project.xml`. A native picker can land anywhere, and `resetToBlankState()` is not undoable. `openProject`'s own corrupt-XML branch is retained behind the new guard. **§B.26 ND-2 is the must-pass scenario for this ordering.**
- **Task 1 — "Quick Open Project".** The entire ProjectBrowserWindow launch moved across intact — `isCurrentProject` guard, `onOpenSelected`, `onNewProject` -> `doFileNew()`, the `DialogWindow::LaunchOptions` block, and the `confirmDiscardChanges` wrap. Every browser feature (sort by Name/Modified/Size, rename, duplicate, delete-to-Recycle-Bin, Show in Explorer, refuse-destructive-ops-on-the-open-project) untouched; only the entry point and `opts.dialogTitle` changed. New File-menu item id **110** sits directly under "Open Project...  (Ctrl+O)"; id verified unused before assignment. No accelerator assigned.
- **Task 1 — plan deviation (smaller than planned).** `ProjectBrowserWindow.h/.cpp` were NOT touched; the plan predicted a window-title parameter, but the title already comes from `opts.dialogTitle` at the launch site.
- **Task 2 — Set Default Template both-subfolders-reachable (docket 2a).** Both `factoryTemplatesDir()` and `userTemplatesDir()` are created before the chooser launches; the start dir stays the Templates ROOT, so Factory and My Templates are equally one click away on a fresh install instead of the root listing coming up empty or half-populated.
- **Task 2 — dead code deleted.** The 26-line unreachable ProjectBrowserWindow block behind `doFileSetDefaultTemplate`'s unconditional `return;` is gone, along with the now-pointless `return`. Live `new ProjectBrowserWindow()` call sites: 3 (one unreachable) -> 2 (`doFileNewFromTemplate`, `doFileQuickOpen`).
- **Task 2 — repointed start dirs.** (a) `EventEditorContent::doImportMidi` opens `Documents\BaySickDAW\MIDI`, created on demand, instead of the Documents root. (b) `BuilderPage::doImportAudio` moved from `File::userMusicDirectory` to `SampleLibrary::getUserSamplesDir()` with `ensureUserSamplesDir()` first — the exact pair the Clip "+"-add path already uses, so the two audio-import entries finally agree; needed a new `#include "../SampleLibrary.h"`.
- **Task 2 — shared path resolvers.** New `EffectPresetIO::userNamPedalsDir()` (`presetsRoot()/Pedals/User NAM Pedals`) and `irDir()` (`presetsRoot()/IR`), both non-per-`EffectType` and documented as such in the header. **Behavior-neutral by construction:** `presetsRoot()` is `Documents/BaySickDAW/Presets/Effects`, precisely what each pre-existing inline literal spelled out — centralization, not a folder move. **The two folders are and remain SEPARATE** (Jeff, explicit). Three NAM sites now consume the resolver (`EffectEditorPanels.cpp` `NAMPedalStylePanel::showFileChooser`, `BaySickPedalsEditor.cpp` pedal-slot Load, and `EffectPresetIO.cpp`'s own ensure-folder call, which held the literal a third time). Both IR empty-state fallbacks (Acoustic Preamp + Acoustic Simulator) now start at `irDir()` (created on demand) instead of the Documents root, keeping the loaded-IR-parent behavior when an IR is already loaded. `EffectEditorPanels.cpp` gained `#include "EffectPresetIO.h"`; `BaySickPedalsEditor.cpp` already had it.
- **Rule 6 pass over edited regions.** Five keeper comments added, all category 1 (architectural why): why Open Project is a directory picker; why validation precedes teardown; why both Templates subfolders get created; why the MIDI folder exists; why Builder audio import matches the Clip-add entry. The header's resolver block states why those folders live outside the per-`EffectType` tree. No WHAT-narration.
- **Build.** Per-task gates, BOTH configs clean at Task 1 and again at Task 2. First batch run end-to-end under the new protocol where Claude runs `do_build.bat`.
- **Master Test Plan §B.26 authored** — 9 scenarios (ND-1..ND-9), reconciled against what shipped rather than transcribed from the plan's ladder. ND-2 flagged MUST-PASS; ND-8 written so a wrong folder in any resolver-fed path indicts the resolver rather than the site; ND-9 a pure regression guard on DrumPage's deliberately-unchanged JUCE browser.

#### Found along the way

- **PREMISE FINDING — the batch's founding framing was void.** "Convert every chooser to native" had nothing to convert: `juce::FileChooser`'s ctor defaults `useOSNativeDialogBox = true` and a sweep of `Source/` finds zero overrides, so all 18 pre-batch chooser sites were already OS-native. The two genuinely non-native surfaces were custom Components, not choosers: `ProjectBrowserWindow` (retargeted here) and DrumPage's dual browse (deliberately kept, docket 3=c — Windows native dialogs cannot express folder-OR-file in one entry). Recorded at G4 group open in the §9 sixty-fourth Forks entry; this batch confirms it in code.
- **HAZARD FINDING (introduced by this batch, mitigated in the same edit).** A native folder picker can land on any folder, and the pre-existing open sequence tore the session down BEFORE it knew the folder was a project. Left unguarded, picking Documents would have wiped every dynamic tab, every mixer strip, and the arrangement with no undo.
- **FINDING (recorded, not routed) — no central resolver for the `Documents\BaySickDAW` app root.** ~43 sites across `Source/` spell the chain out by hand; `EffectPresetIO::presetsRoot()`, `SampleLibrary::getUserSamplesDir()` and `ProjectManager::getDefaultProjectsRoot()` are each their own island. The new MIDI folder joins the 43 rather than fixing the pattern — a one-off resolver for a single call site would be noise. Recorded for a future call.
- **PROCESS FINDING — the pinned `do_build.bat` string could never match an "always allow" rule** (compound PowerShell statement), so Jeff was re-prompted on every build; and the wider prompt friction came from chaining auto-allowed read-only commands rather than from a missing allowlist. Both fixed — see the process entry above.
- **Cross-batch note:** `doFileNewFromTemplate`'s ProjectBrowserWindow launch was left alone on purpose — QA-ProjectSave (G4 batch 6) removes that entry point outright. Save-as NAME prompts stayed out of scope per the plan.

#### What was done about each finding

- **Premise correction: no code consequence.** The §5 docket and §9 sixty-fourth entry were amended at G4 group open, before execution; this batch confirmed it in code and wrote the correction into §B.26's scope note so the campaign walker isn't hunting conversions that never existed.
- **Teardown hazard: FIXED in the same edit that created it,** and promoted to §B.26's MUST-PASS scenario (ND-2).
- **`irDir()`: SHIPPED.** A judgment call past the plan's letter; surfaced to Jeff, who confirmed the only requirement was that the NAM and IR folders stay distinct — they do.
- **App-root literal duplication: recorded only.** No §9 Forks entry, no §5/§6 change — the routing call is Jeff's and the pattern predates this batch by ~43 sites.
- **Build/permission friction: FIXED** in CLAUDE.md, memory, and `.claude/settings.local.json` (297 -> 3 entries, backed up first).

#### Group review (R3)

- **Pending — runs at the G4 boundary** (after `clean-pointing-stoat`'s commit) over the group's combined diff.

#### Carry-forward contradictions

- None. No audio thread, no DSP, no APVTS layout, no persistence FORMAT touched — every surface is message-thread UI plus folder resolution. Two notes to carry: (1) `Documents\BaySickDAW\MIDI` and `Presets\Effects\IR` are new on-disk folders created on demand, so neither exists until its chooser is first opened; (2) `EffectPresetIO` is now the home for shared asset folders that are NOT per-`EffectType` — anything else of that shape belongs beside `userNamPedalsDir()` / `irDir()`, not as a fresh literal.

#### Diagnostic Instrumentation Catalog

- **NONE added this batch.** Nothing to strip.

#### Commit(s)

`f4112b17` (whole batch — Tasks 1+2 + `irDir()` + Rule 6 pass + §B.26 + the CLAUDE.md build-invocation fix + the permissions prune + the two QA-VibeSlider hash backfills + held entry + running notes; single batch commit per the bulk-run model). Preceded by QA-VibeSlider `bd49d066`. Build clean in BOTH configs at both task gates; behavioral verification deferred to the R2 campaign pass against §B.26.

#### Next action

- Proceed to **QA-ApvtsAutomation** ([`wired-lassoing-crane.md`](Batch Plans/wired-lassoing-crane.md)), G4 batch 3 of 8.
