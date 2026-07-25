# QA-ProjectSave — Templates v2 + New-from-Template submenu + sample-retention hybrid + Pack — Plan (deep-packing-badger)

> **Canonical path:** `Plans & Specs/Batch Plans/deep-packing-badger.md` (mirrored at G4 group
> approval; home-dir copy deleted). **For execution:** bulk-run G4 batch 6 of 8. §B authored at
> code-complete; one source commit.

## Context

Scout state (2026-07-25, refs current): `saveTemplateAs`
([StandaloneEditor.cpp:7022-7098](Source/Standalone/StandaloneEditor.cpp:7022)) writes an
inline-state USER schema for L/B/D only; `loadTemplate` (:6935-7020) reads only the FACTORY
attribute schema (`<Kit path>` / `<Layer|Bass engine presetPath locked>`) — user templates do
not round-trip at all (engine+locked survive; all state lost; inline children ignored). The
inline-load machinery already exists in the KIT loader's dual-format branch
(`importDrumState`, loadKit) — proven pattern. `loadTemplate` also tears down everything
unconditionally (:6960) and **bypasses `confirmDiscardChanges` entirely** (verified — the only
File-level teardown with no save prompt). Menu items: 102 "New from Template…" is actually a
clone-an-existing-PROJECT flow (ProjectBrowserWindow over Projects\ -> folder-copy seed);
109 "Load Template…" = `showTemplateMenu` walking Templates\*.xml; 106 "Save as Template…"
stays. The default-template pointer (`ProjectManager::get/setDefaultTemplate`) is consumed by
NOTHING today (File>New is always blank per QA-Ef #6) and its stored value (.xml file)
mismatches `newProject`'s folder-seed gate ([ProjectManager.cpp:262](Source/ProjectManager.cpp:262))
— this batch's submenu becomes its consumer and resolves the mismatch (.xml wins; the unused
folder-seed branch goes).

Sample layer today: project assets are copy-only into `<project>/Samples/`
(`ProjectManager::importSample`, :485-531); Clips copy into My Samples with bare-name refs;
engine samples are reference-only (`library:` for Core, absolute otherwise); missing files are
silently skipped at load. FND-1 premise flip (verified): six of seven page types ALREADY have
the 3-button Save-Page-Preset-&-Delete prompt — only RustyDrums has a plain Yes/Cancel
(StandaloneEditor.cpp:8045-8066), and its save concept is the KIT.

- **Risk:** high (unchanged from §5) — project/template XML, sample resolution, three menu
  surfaces. Mitigation: templates v2 REUSE the proven project UIState serializers; the bundle
  walker comes prebuilt from QA-Export.
- **Effort:** ~10-16 h.
- **Dependencies:** QA-Export's `ProjectBundler` (Pack reuses it). Runs after QA-Export per §6.

## Spec calls already locked

| ID | Decision | Reasoning |
|----|----------|-----------|
| Docket 9=A | Templates adopt the project UIState shape (base64 engineData per tab — all tab types incl. Vox/Inst/Clips/Rusty/Aux) with a dual-format load branch keeping v1 FACTORY attribute XMLs loading | Full coverage for free; loadKit precedent; factory set untouched |
| Docket 10=A | The clone-a-project capability dies with the submenu restructure | "We already have a save as" — Jeff |
| Docket 11=A | RustyDrums joins the 3-button pattern as "Save Kit & Delete" | FND-1 completion; kit = Rusty's save concept |
| Marathon 8a | Source-aware hybrid: library = reference, volatile = copy; explicit Pack action; templates inherit | Locked 2026-07-08 |
| Marathon 8b | Existing per-project copies left as-is; hybrid applies going forward | Locked 2026-07-08 |
| Docket 8 (Export) | Pack offers references vs self-contained per pack | Shared bundler scope switch |
| §5 locked | Submenu replaces items 102+109: `New from Default Template` (greyed when unset; suffix = default's name) / `Premade Templates >` / `My Templates >`; unified dirty-check flow; removals (doFileNewFromTemplate, showTemplateMenu, dup kIdSaveAs); Save-Template-As dialog text update | Locked 2026-05-23 + thirtieth Forks |
| — | loadTemplate dirty-bypass fixed by the unified flow; v1-USER inline templates (never loadable) get no loader — superseded by v2 | Baked-pending-veto 2026-07-25, no veto |

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open. Implementation notes for R5: template v2 = `<BaySickTemplate version="2">`
wrapping the UIState-shaped tab walk (all tab types + aux/vox/inst names + orders) — NO
arrangement/pattern content ("complete project skeleton, without arrangement content" per §5);
mixer strip settings ride each tab's engineData + the aux entries exactly as project save does.
Template "samples" = the refs inside engineData (hybrid-inherited: library refs + absolute for
volatile; a template is one XML, so volatile files are referenced, not copied — same exposure
as today's engine refs, noted in the §B walk).

## Files to modify

- Task 1: `Source/Standalone/StandaloneEditor.cpp` (saveTemplateAs :7022-7098 -> v2 writer
  reusing `serializeUIState` sub-walk; Save-as-Template dialog text)
- Task 2: `Source/Standalone/StandaloneEditor.cpp` (loadTemplate :6935-7020 — v2 apply via the
  deserializeUIState per-type branches; v1-factory branch preserved; teardown scoped to what
  the template replaces: v2 = all dynamic tabs, v1-factory = L/B/D only)
- Task 3: `Source/Standalone/StandaloneEditor.cpp` (File menu :9510-9552 + dispatch;
  doFileNewFromTemplate :10316-10370 REMOVED; showTemplateMenu :7100-7159 REMOVED; new
  submenu builder; default-template consumer), `Source/ProjectManager.cpp/.h` (folder-seed
  branch removal :262-270; default-template docs)
- Task 4: `Source/Standalone/StandaloneEditor.cpp` (Rusty delete prompt :8045-8066)
- Task 5: `Source/ProjectManager.cpp` (importSample hybrid), `Source/PluginProcessor.cpp`
  (resolveProjectFile :5096-5105 — new ref branches), `Source/Standalone/BuilderPage.cpp`
  (onResolveStoredPath mirror), `Source/Clips/ClipsPage.cpp` (import path alignment)
- Task 6: `Source/Standalone/StandaloneEditor.cpp` (File menu "Pack Project…" over
  QA-Export's `ProjectBundler`)

## Tasks

### Task 1 — Template writer v2

- [ ] Refactor the per-tab walk of `serializeUIState` so the tab-capture core (type/pageIndex/
  name/engine/engineData + per-type extras) is callable for a TEMPLATE capture (tabs + aux/vox/
  inst names + orders; NO arrangement, NO patterns, NO song-loop/UI-position extras).
- [ ] `saveTemplateAs` writes `<BaySickTemplate version="2" name>` + that capture; dialog text
  updated from "kit + layers + basses" to the full-skeleton wording (ASCII).
- [ ] Build gate.

### Task 2 — Template loader: v2 branch + scoped teardown + dirty gate

- [ ] `loadTemplate`: version switch — v2 -> apply via the deserializeUIState per-type tab
  branches (same shield + aux-clear discipline as project load); v1 factory attribute schema ->
  existing code path unchanged.
- [ ] Teardown symmetric to scope: v2 tears down all dynamic tabs (it restores all types);
  the v1-factory branch tears down ONLY L/B/D tabs (transitional per §5).
- [ ] Every template-load entry runs through `confirmDiscardChanges` (fixes the verified
  silent-discard bypass); blank/clean project loads directly.
- [ ] Build gate.

### Task 3 — New-from-Template submenu + default-template consumer + removals

- [ ] File menu: submenu replacing 102+109 — `New from Default Template` (greyed when no
  default; label suffix = default's name) / `Premade Templates >` (recursive Factory walk) /
  `My Templates >` (recursive user walk). Every pick -> unified Load Template flow (Task 2).
- [ ] Default template: `setDefaultTemplate` keeps storing the .xml; the submenu item loads it
  through the same flow — the pointer finally has a consumer. Remove `newProject`'s dead
  folder-seed branch (ProjectManager.cpp:262-270) + update the header contract comment.
- [ ] Remove `doFileNewFromTemplate` (+ its ProjectBrowserWindow launch) and `showTemplateMenu`
  (+ its internal dup `kIdSaveAs`); item 106 top-level Save-as-Template stays. Grep zero refs.
- [ ] Build gate.

### Task 4 — FND-1 completion: Rusty "Save Kit & Delete"

- [ ] Rusty's delete prompt gains the 3-button dirty branch mirroring the other six
  (ClipsPage.cpp:435-484 pattern): dirty kit -> [Save Kit & Delete / Delete / Cancel] where
  save chains the existing kit-save flow (StandaloneEditor kit menu save path) then fires the
  delete; clean -> current 2-button warning unchanged. Reuse Rusty's existing kit-dirty signal
  (verify which flag exists at execution; if none, derive from the engine dirty hook).
- [ ] Build gate.

### Task 5 — Sample-retention hybrid (8a/8b)

- [ ] `ProjectManager::importSample` becomes source-aware: source under Core Library ->
  return `library:<rel>` (no copy); source under My Samples -> return `mysamples:<name>`
  (no copy); anything else (volatile) -> copy into `<project>/Samples/` exactly as today.
- [ ] `resolveProjectFile` + `BuilderPage::onResolveStoredPath` gain the `library:` +
  `mysamples:` branches (Core Library dir / getUserSamplesDir). Existing absolute +
  project-relative refs untouched (8b: no migration).
- [ ] ClipsPage import keeps its My-Samples copy behavior but stores the `mysamples:` form for
  new imports (loader accepts bare names as before — no rewrite of old refs).
- [ ] Missing-file handling upgraded from silent skip to a one-shot load report dialog listing
  unresolved refs (no per-block spam) — the silent-skip finding from the scout.
- [ ] Build gate.

### Task 6 — Pack Project

- [ ] File menu "Pack Project…" -> QA-Export's `ProjectBundler` (destination, zip-vs-folder,
  references-vs-self-contained per docket 8). Pack of an UNSAVED project prompts save first
  (through the standard flow).
- [ ] Build gate.

## Batch close (bulk-run per-batch loop — one commit per batch)

- [ ] Tell Jeff to run `do_build.bat`; fix until BOTH configs build clean.
- [ ] Author this batch's Master Test Plan §B section (`blocks:` = this commit, backfilled).
- [ ] `/draft-doc batch-close` -> held Work Log entry in running notes (R2); no §5 touch.
- [ ] Running-notes code-complete entry (+ Rule 4 rows if any).
- [ ] ONE batch commit (Rule 9): `QA-ProjectSave: <one-line what> (<scope>)` + trailer;
  message + FULL git status; commit on Jeff's approval.

## Verification (authors into Master Test Plan §B)

1. Build a project with one of EVERY tab type (Layers/Bass/Drums/Clips/Vox/Inst-Guitars/
   Inst-Basses/Rusty + an aux strip), Save as Template -> File > New (blank) -> load it from
   My Templates: every tab restores with engine state, names, aux strip back.
2. Factory template still loads (v1 branch): kit + layers + basses appear as before; tabs of
   OTHER types present beforehand survive a v1-factory load (scoped teardown).
3. Dirty project + any template pick: Save / Don't Save / Cancel prompt appears (bypass fixed);
   Cancel aborts with nothing torn down.
4. New from Default Template: greyed with no default; after Set Default Template it loads that
   template; label shows the default's name.
5. "New from Template…" (old clone flow) is gone from the File menu; "Load Template…" gone;
   Save as Template dialog text reflects the full skeleton.
6. Rusty tab with a dirtied kit -> Delete: 3-button prompt; Save Kit & Delete writes the kit
   (verify it reloads from the kit menu) then removes the tab; clean kit -> 2-button warning.
7. Import a Core Library sample into the Builder: NO copy appears in `<project>/Samples/`;
   save/reopen: plays (library: ref). Import from Downloads: copy appears; plays after reopen.
   Clip add from My Samples: no duplicate copy; plays after reopen.
8. Delete a referenced volatile file on disk -> open project: the load report names it; the
   rest of the project loads normally.
9. Pack Project (references, folder) -> move the pack to another location + rename the
   original project folder: opens and plays. Pack (self-contained, zip) -> unzip elsewhere:
   opens with Core-Library-independent samples.
10. Old project saved pre-batch opens unchanged (8b: absolute + Samples/ refs untouched).

## Routing notes (Rule 3)

Template/preset findings during execution fold here; anything touching the §E preset-walk
surfaces logs for the campaign. Undo/dirty behavior of the new menu flows routes to
QA-UndoCoverage/QA-DirtyFlag (next two batches).

## Carry-Forward Reference touch points

§3 persistence decisions + the QA-Ef close entries (Work Log) before Tasks 1-3; the
project-load shield discipline (loadTemplate comment block :6941-6951) is the pattern every new
load path must keep.
