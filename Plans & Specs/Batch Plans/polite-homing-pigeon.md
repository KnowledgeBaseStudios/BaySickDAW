# QA-NativeDialogs — Native Open Project + Quick Open + per-context default folders — Plan (polite-homing-pigeon)

> **Canonical path:** `Plans & Specs/Batch Plans/polite-homing-pigeon.md` (mirrored at G4 group
> approval; home-dir copy deleted). **For execution:** bulk-run G4 batch 2 of 8. No per-task
> verify pauses; §B section authored at code-complete. One source commit.

## Context

Scout audit (2026-07-25, all 18 `juce::FileChooser` sites enumerated) collapsed the old framing:
**every chooser is already OS-native** — JUCE's `useOSNativeDialogBox` ctor default is `true`
([juce_FileChooser.h:127](JUCE/modules/juce_gui_basics/filebrowser/juce_FileChooser.h)) and no
app site overrides it. The one non-native surface is DrumPage "Browse sample folder…" whose
files+dirs-in-one-dialog combo Windows native dialogs cannot express — **Jeff locked keep-as-is
(docket 3=c)**. Remaining real work: the Open Project surface (custom ProjectBrowserWindow),
four default-folder fixes, one path-resolver centralization, and dead code.

ProjectBrowserWindow facts (verified): lists real projects only (folders containing
project.xml), sortable Name/Modified/Size, rename / duplicate / delete-to-Recycle-Bin /
show-in-Explorer, refuses destructive ops on the open project, double-click opens. Launched
from `doFileOpen` (StandaloneEditor.cpp:10433), `doFileNewFromTemplate` (:10323 — dies in
QA-ProjectSave this group), and a dead block behind `doFileSetDefaultTemplate`'s unconditional
`return` (:10396-10422).

- **Risk:** low. UX-only; no audio thread, no DSP.
- **Effort:** ~2-4 h (shrunk from ~4-7 h; native conversion is mostly moot).
- **Dependencies:** none. QA-ProjectSave (batch 6) later removes the doFileNewFromTemplate
  launch site — this batch leaves it alone.

## Spec calls already locked

| ID | Decision | Reasoning |
|----|----------|-----------|
| 1 (6a revised) | Open Project -> native dialog; browser survives as new **"Quick Open Project"** menu item | Jeff 2026-07-25; browser's list/metadata/management features are wanted, native look for the standard path |
| 2a | Set Default Template keeps Templates ROOT + both subfolders guaranteed reachable | Neutral start showing Factory + My Templates equally; batch ensures both dirs exist pre-launch |
| 2b | Create `Documents\BaySickDAW\MIDI`; Event Editor CC import starts there | Conventional home for .mid files |
| 2c | Create `Presets\Effects\IR`; Acoustic Preamp/Simulator "Load IR…" empty-state fallback points there | These IRs had no home |
| 2d | Builder Import Audio + Clip "+"-add BOTH open My Samples | Unify the two audio-import entries |
| 3 | DrumPage dual browse keeps the JUCE browser | Capability (folder OR file in one entry) outweighs the style mismatch |
| — | Save-as NAME prompts out of scope; dead ProjectBrowserWindow block deleted; NAM-pedal folder literal centralized | Baked-pending-veto 2026-07-25, no veto |

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open. One implementation call stated plainly for the R5 read: the native Open
Project dialog is a **folder picker** rooted at `Projects\` (a project IS a folder); after pick,
validate `project.xml` exists inside and show a plain error dialog if not. (A file-picker-for-
project.xml alternative forces a double-descend into every folder — worse from the user seat.)

## Files to modify

- Task 1: `Source/Standalone/StandaloneEditor.cpp` (doFileOpen :10428-10433; File menu build
  :9510-9552; menuItemSelected dispatch :9641+), `Source/Standalone/ProjectBrowserWindow.h/.cpp`
  (window title param only)
- Task 2: `Source/Standalone/StandaloneEditor.cpp` (doFileSetDefaultTemplate :10372-10395 +
  dead block :10396-10422), `Source/Standalone/EventEditor.cpp` (doImportMidi :1856),
  `Source/Standalone/EffectEditorPanels.cpp` (:4962-4965, :5073-5076 IR fallbacks;
  :6000-6005 NAM literal), `Source/BaySickPedals/BaySickPedalsEditor.cpp` (:443-451 NAM literal),
  `Source/Standalone/BuilderPage.cpp` (doImportAudio :7698), `Source/EffectPresetIO.h/.cpp`
  (new resolver home)

## Tasks

### Task 1 — Native Open Project + "Quick Open Project"

- [ ] `doFileOpen`: replace the ProjectBrowserWindow launch with a native folder chooser
  (`FileChooser("Open Project", ProjectManager::getDefaultProjectsRoot(), {}, true)` +
  `canSelectDirectories`), inside the existing `confirmDiscardChanges` wrap. On result:
  validate `project.xml` exists in the picked folder -> `mProjectManager->openProject(folder)`
  path as today; else AlertWindow "Not a BaySickDAW project folder."
- [ ] File menu: add **"Quick Open Project"** directly under "Open" (first free id in the File
  block — verify id unused in menuItemSelected before assigning). Handler = the current
  ProjectBrowserWindow launch code (moved intact), window title "Quick Open Project".
- [ ] Keep every browser feature untouched (rename/duplicate/delete/sort/guards) — only the
  entry point and title change.
- [ ] Rule 6 pass over edited regions.

### Task 2 — Default folders + resolver + dead code

- [ ] `doFileSetDefaultTemplate`: before launching the chooser, `createDirectory()` BOTH
  `factoryTemplatesDir()` and `userTemplatesDir()` (2a "reachable" guarantee); start dir stays
  `templatesDir()` root. Delete the dead ProjectBrowserWindow block after the `return`
  (:10396-10422) — grep confirms no other caller.
- [ ] `EventEditor::doImportMidi`: new start dir = `Documents\BaySickDAW\MIDI` (create on
  demand), replacing plain Documents.
- [ ] Acoustic Preamp + Acoustic Simulator "Load IR…": empty-state fallback -> new
  `Presets\Effects\IR` (create on demand); keep the loaded-IR-parent behavior when one is loaded.
- [ ] `BuilderPage::doImportAudio`: start dir `userMusicDirectory` ->
  `SampleLibrary::ensureUserSamplesDir()` (matches the Clip-add path; Core Library shortcut
  inside gives factory access).
- [ ] Centralize the duplicated `…\Presets\Effects\Pedals\User NAM Pedals` literal: new
  `EffectPresetIO::userNamPedalsDir()`; both call sites consume it
  (`reference_single_source_of_truth_for_paths`).

## Batch close (bulk-run per-batch loop — one commit per batch)

- [ ] Tell Jeff to run `do_build.bat`; fix until BOTH configs build clean.
- [ ] Author this batch's Master Test Plan §B section from the Verification list below
  (`blocks:` = this batch's commit, hash backfilled at commit).
- [ ] `/draft-doc batch-close` -> append under `## Held Work Log entry (apply at section pass)`
  in the running notes. Do NOT touch the Implemented Work Log or the §5 STATUS line now (R2).
- [ ] Append the running-notes code-complete entry (+ any Rule 4 catalog rows, same edit pass).
- [ ] ONE batch commit (Rule 9): `QA-NativeDialogs: <one-line what> (<scope>)` +
  `Co-Authored-By` trailer; surface message + FULL git status; commit only on Jeff's approval.

## Verification (authors into Master Test Plan §B)

1. File > Open Project: native Windows folder dialog opens at Projects; picking a project folder
   opens it; picking a non-project folder shows the error and changes nothing; Cancel is a no-op.
2. File > Quick Open Project: the browser appears titled "Quick Open Project"; double-click
   opens; rename/duplicate/delete-to-Recycle-Bin still work; both destructive ops still refuse
   the currently-open project.
3. Unsaved-changes prompt still fires before both open paths (dirty project).
4. Options > Set Default Template: dialog opens at Templates root showing BOTH Factory and
   My Templates folders (fresh-install case included — delete the folders first to test
   creation); an .xml picked from EITHER subfolder sets the default (Options label updates).
5. Event Editor Ctrl+M: dialog opens in `Documents\BaySickDAW\MIDI` (folder created if absent).
6. Acoustic Preamp with no IR loaded: Load IR opens in `Presets\Effects\IR`; with an IR loaded
   it opens beside that IR. Same for Acoustic Simulator.
7. Builder Import Audio AND Clip "+"-add: both open My Samples; the Core Library shortcut inside
   works from the native dialog.
8. NAM Pedal "Load NAM Pedal" (pedal slot) + NAM Pedal FX panel: both still open the User NAM
   Pedals folder (now via the shared resolver).
9. Drum tab > Pick a sound > Browse sample folder…: unchanged JUCE browser; folder pick loads a
   folder, file pick loads a single sample (3=c regression guard).

## Routing notes (Rule 3)

Findings about template-menu structure route INTO QA-ProjectSave (this group). Any further
wrong-folder discoveries fold here in-batch.

## Carry-Forward Reference touch points

None load-bearing — surfaces are all message-thread UI. Path-resolver conventions per the
single-source-of-truth memory note.
