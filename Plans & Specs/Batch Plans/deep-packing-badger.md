# QA-ProjectSave — Templates v2 + New-from-Template submenu + sample-retention hybrid + Pack — Plan (deep-packing-badger)

> **Canonical path:** `Plans & Specs/Batch Plans/deep-packing-badger.md` (mirrored at G4 group
> approval; home-dir copy deleted). **For execution:** bulk-run G4 batch 6 of 9 (G4 grew to nine
> when QA-Soundness was added 2026-07-25). §B authored at code-complete; one source commit.

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
- **Effort:** ~32-44 h total (originally ~10-16 h for 7 tasks). Re-planned to **12 tasks** on
  2026-07-26 after the second docket + the G4 findings sweep folded five self-dispositioned
  findings in, plus the engine-reference gap and the automation-registry rework. Roughly ~8 h is
  already spent (Tasks 1-3 complete + gate-green); ~24-36 h remains. Surfaced to Jeff as a scope
  concern before he approved it — the size is known, not accidental.
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
| Docket 15=B (2026-07-26) | Template v2 additionally carries a full `<Processor>` snapshot (main APVTS + `VibeRackStates`), applied on load — mixer faders/pans/widths/sends/cables, FX racks and per-insert EQ all restore | The plan's original note ("mixer strip settings ride each tab's engineData") was FALSE on read: `engineData` is the engine processor's own state only; every `mixer_*` param and all rack state live in the main APVTS under `<Processor>`, a sibling of `<UIState>` ([PluginProcessor.cpp:4996-5002](Source/PluginProcessor.cpp:4996)). UIState-only would not meet §5's "complete project skeleton / mirrors the project-save shape" |
| Docket 16 (2026-07-26) | RustyDrums' 3-button prompt reads **"Save Page Preset & Delete"** and chains `savePlayerPresetAs` — verbatim parity with the other six page types | Docket 11's "Save Kit" label named a flow that does not exist: Rusty's only user-writable save is the J-11 Player Preset ([BaySickRustyDrumsPage.h:96-151](Source/Standalone/BaySickRustyDrumsPage.h:96)); its "kit" is a read-only factory `.sfz`, and `StandaloneEditor::saveKitAs` is the unrelated DrumPage kit. Supersedes docket 11=A's label |
| Docket 17=b (2026-07-26) | No new "Pack Project…" item. QA-Export's already-shipped **"Export Project Bundle…"** (item 122) keeps its name and gains only the save-first prompt | `doExportProjectBundle` ([StandaloneEditor.cpp:10625](Source/Standalone/StandaloneEditor.cpp:10625)) already provides destination + zip-vs-folder + references-vs-self-contained over `ProjectBundler`. A second item would be a duplicate |
| Docket 18 (2026-07-26) | Layers/Bass/Drums load EMPTY and delete down to zero like every other tab type; their bus strips (plus Clips/Vox/Inst) hide when empty. Inserted as **Task 1**, bumping the original Tasks 1-6 to 2-7 | Jeff: the hard-coded trio is "bloat for a user that may not want those things," and empty buses are a defect to fix, not a note. Ordering is forced: with docket 15=B, every template written in Task 2 would otherwise bake in three phantom tabs and three phantom bus strips |

### Second docket — LOCKED 2026-07-26 (mid-execution; batch re-planned to 12 tasks)

| ID | Decision | Reasoning |
|----|----------|-----------|
| Docket 19 | **Template load = New Project semantics.** Tear everything down and build what the template says; the existing patterns/arrangement are CLEARED, not merged | Jeff: "There is no you have a song setup, and then you load a template onto that song. The template would be loaded at the very start and then you would make your song on the template." Amends Task 3, which shipped the merge reading |
| Docket 20 | **Templates carry no Builder-grid or pattern content** — confirmed already true by construction (the writer never emits `PatternManager`). A Clips tab's sample IS part of the rig, so it returns and re-registers in the browser panel; what does not return is that clip placed as a block on the timeline | Jeff: "The template isn't for saving a song to remix later, it's for saving a full setup so you don't have to rechoose and patch things every time" |
| Docket 21=a | **Close the `ProjectBundler` engine-reference gap IN THIS BATCH** (new Task 6). `enumerate` walks only PatternManager today, so Export Project Bundle silently omits NAM captures, user IRs and engine-loaded sample folders in BOTH modes | Shipped correctness hole in a feature whose whole purpose is "give someone else everything they need". Mishandled at QA-Export close (see walrus finding 5 correction) |
| Docket 22=b + | **Full/self-contained export STOPS copying Core Library content** — full means "every file not guaranteed to exist on a BaySickDAW install". **Plus a size estimate shown at export time** | Anyone who can open the project has the app, hence the library. Once Task 6 walks engine refs, the old semantics would copy 555 MB / 697 MB / 1,082 MB kits per bundle. Jeff added the size estimate |
| Docket 23=b | **Template sample copies land in `Documents\BaySickDAW\My Samples`**, not Core Library | Core Library is the installer-populated LocalAppData tree — an update could clobber it and it is not in most backups. My Samples is the existing user-sample home, visible and backed up |
| Docket 24=c | **Copy scope: Clips samples + NAM captures + IRs + BaySickPlayer sample paths.** sfizz kits EXCLUDED | Kits are already inside Core Library (no external-`.sfz` load path exists anywhere in the app), so they resolve by reference; copying one would drag its whole 555 MB-1 GB product folder per template |
| Docket 25=c | **`library:` normalization everywhere a Core-Library path is persisted**, presets included | The convention is half-built: FIVE readers, ONE writer (`DrumPage.cpp:1007`). Absolute paths embed the Windows username, so they cannot resolve under another account |
| Docket 26=a | The docket 23/24/25 cluster lands as a **new task before the menu work** | Everything downstream needs the resolver |
| Docket 27=a | **Run the G4 findings sweep NOW**, before continuing execution | Jeff: offering "don't run it" was itself a violation — every finding's disposition is his call, so the sweep is not optional |
| Docket 28=a | **Hidden-bus sidechain edge: leave it.** A bus with no members that something sidechains FROM will hide with its cable endpoint | Audio unaffected — an empty bus is silent, so the sidechain carries nothing |
| Sweep 1-5 = fix in batch | All five self-dispositioned findings from the G4 sweep are fixed HERE: app-root literal duplication (~43 sites), the wrong `mPartSel` comment, the misleading `engineRootTag` label, write-only `kStateVersion`, and the automation-registry lifetime | Jeff: "this group is the end of the code and we fix what we find period" |
| Automation = option 2 | **All three steps**: self-cleaning registry (b-prime) + dead-lane diagnostics + (e)-lite moving FX-rack and pedal applicators off the widget onto the DSP | `/architecture` pass (2026-07-26) found NO surveyed system keeps a UI-keyed applicator map, and confirmed neither (b) nor (c) alone fixes the verified FX-rack channel-switch hole |
| Commits = 4 | **Deliberate deviation from one-commit-per-batch** (Jeff, 2026-07-26): commit after Task 6, after Task 7, after Task 9, plus the batch-close commit | The batch grew from 7 tasks to 12 and roughly doubled in hours; a single rollback point spanning project save/load + the bundler + the automation registry + the audio path would cost the already-verified tasks if the smoke failed |

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open. Implementation notes for R5: template v2 = `<BaySickTemplate version="2">`
wrapping the UIState-shaped tab walk (all tab types + aux/vox/inst names + orders) — NO
arrangement/pattern content ("complete project skeleton, without arrangement content" per §5);
mixer strip settings do NOT ride engineData (that carries the engine processor's own state and
nothing else) — they come from the sibling `<Processor>` snapshot added per docket 15=B.
Template "samples" = the refs inside engineData (hybrid-inherited: library refs + absolute for
volatile; a template is one XML, so volatile files are referenced, not copied — same exposure
as today's engine refs, noted in the §B walk).

## Files to modify

- Task 1: `Source/Standalone/StandaloneEditor.cpp` (addDefaultDynamicTabs :1901 + its two
  callers :1886 / :10348 REMOVED; addDefaultDrumTab :2017 + kit-recovery caller :7593 REMOVED;
  15 `isLastOfType` guard sites REMOVED; three new empty states + `show*EmptyState()` beside the
  Clips/Vox/Inst trio :1414-1430; on-close cascade :4956-4989 incl. its now-false
  "Layer/Bass/Drum can't reach zero" comment), `Source/Standalone/RibbonTabBar.cpp/.h`
  (`closeTab` zeroAllowed set :174-179; three empty-state request hooks :505-518),
  `Source/Standalone/MixerPage.cpp/.h` (per-bus activation flags mirroring
  `mRustyDrumsBusActive` :1585-1588 / :2499-2534; `laidOutBus` + FX-bus block in
  layoutScrollContent :3672-3720; route-picker + hit-test gating :702 / :766)
- Task 2: `Source/Standalone/StandaloneEditor.cpp` (saveTemplateAs :7059-7135 -> v2 writer
  reusing `serializeUIState` sub-walk; Save-as-Template dialog text),
  `Source/PluginProcessor.cpp/.h` (docket 15=B: reusable `<Processor>` snapshot writer shared
  with `serializeProject` :4942-5033)
- Task 3: `Source/Standalone/StandaloneEditor.cpp` (loadTemplate :6972-7057 — v2 apply via the
  deserializeUIState per-type branches + the `<Processor>` apply; v1-factory branch preserved;
  teardown scoped to what the template replaces: v2 = all dynamic tabs, v1-factory = L/B/D only)
- Task 3 amendment: `Source/Standalone/StandaloneEditor.cpp` (`applyTemplate` v2 branch — clear
  patterns/arrangement/audio library per docket 19)
- Task 4: `Source/ProjectManager.cpp` (importSample hybrid :485-531), `Source/PluginProcessor.cpp`
  (resolveProjectFile :5096-5105 — new ref branches), `Source/Standalone/BuilderPage.cpp`
  (onResolveStoredPath mirror), `Source/Clips/ClipsPage.cpp` (import path alignment)
- Task 5: `Source/Standalone/LayersPage.cpp` + `BassPage.cpp` (savePatchAs -> write `library:`),
  `Source/Standalone/StandaloneEditor.cpp` (`serializeTabsInto` kitPath + clipPath normalization;
  template-save sample copy), `Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp` (KitPath),
  `Source/SampleLibrary.cpp/.h` (copy-into-My-Samples helper + dedupe)
- Task 6: `Source/Standalone/ProjectBundler.cpp/.h` (engine-reference walk; `shouldCopy` scope
  change; caveat comment removal), `Source/Standalone/StandaloneEditor.cpp`
  (doExportProjectBundle :10625 — scope labels + size estimate)
- Task 7: `Source/Standalone/StandaloneEditor.cpp/.h` (reverse index + ComponentListener +
  `mTearingDown`; dispatch diagnostics; `onIsParamStale`; DELETE
  `eraseAutomationEntriesWithPrefix` + ~17 call sites), `Source/Standalone/SharedUI.cpp/.h`
  (widen the two register hooks to carry the owning Component; five wrappers),
  `Source/Standalone/EffectEditorPanels.cpp` + `MixerTrackStrip.cpp` (pass the component),
  `Source/Standalone/EffectsPage.cpp` + `Source/BaySickPedals/BaySickPedalsEditor.cpp` +
  per-effect DSP headers (step 3: `setParamByKey` / suffix tables)
- Task 8: `Source/SampleLibrary.cpp/.h` or a new resolver home, plus the ~43 call sites that
  currently spell `Documents\BaySickDAW` by hand
- Task 9: `Source/Harmless/HarmlessEditor.cpp` (mPartSel comment), `Source/Inst/InstPage.cpp`
  (engineRootTag label), `Source/BaySickPedals/BaySickPedalsProcessor.*` (kStateVersion)
- Task 10: `Source/Standalone/StandaloneEditor.cpp` (File menu :9546-9550 + dispatch :9698-9701;
  doFileNewFromTemplate REMOVED; showTemplateMenu REMOVED; new submenu builder;
  default-template consumer), `Source/ProjectManager.cpp/.h` (folder-seed branch removal)
- Task 11: `Source/Standalone/BaySickRustyDrumsPage.cpp/.h` (dirty listener + `isPatchDirty()`;
  `savePlayerPresetAs` completion callback), `Source/Standalone/StandaloneEditor.cpp` (:8082-8105)
- Task 12: `Source/Standalone/StandaloneEditor.cpp` (doExportProjectBundle — save-first prompt
  replacing the "save the project first" bail; no new menu item per docket 17=b)

## Tasks

### Task 1 — Layers/Bass/Drums empty by default + empty buses hidden (docket 18)

Ordering is forced, not preferential: with docket 15=B, every template written in Task 2 would
otherwise capture three phantom tabs and three phantom bus strips into its `<Processor>` +
`<UIState>` snapshot. Clean the model first, then serialize it.

- [ ] Stop seeding the trio: delete `addDefaultDynamicTabs()` and both callers (editor
  construction + `doFileNew`). App opens on Builder with an empty ribbon, first launch included.
- [ ] Allow delete to zero: add `Layers` / `Bass` / `Drums` to `RibbonTabBar::closeTab`'s
  `zeroAllowed` set; strip all 15 `isLastOfType` guard sites and their "This is the only Layer
  tab" alerts. Retire `isLastOfType` itself if it ends with no callers.
- [ ] Delete `addDefaultDrumTab()` + the kit-recovery caller that re-spawns a Drums tab whenever
  none exists — it would silently undo a delete-to-zero.
- [ ] Three empty states (`LayersEmptyState` / `BassEmptyState` / `DrumsEmptyState`) mirroring
  the existing Clips/Vox/Inst trio, their `show*EmptyState()` methods, the ribbon body-click
  hooks, and the on-close cascade. Ribbon slots stay visible at zero with a 0 badge (hiding the
  slot would leave no way to add one back). Rewrite the cascade's now-false
  "Layer/Bass/Drum can't reach zero" comment (Rule 6 — wrong comments get fixed).
- [ ] Buses hide when empty, mirroring the existing `mRustyDrumsBusActive` pattern (child
  component, starts `setVisible(false)`, one activation flag driven by tab count, route-picker
  and cable hit-testing gated on the same flag). Applied to Layers/Bass/Drums **and**
  Clips/Vox/Inst, which have the identical dead-bus condition today. Master + FX bus never hide
  (terminal / default aux parent + standing send target).
- [ ] Visibility predicate = *zero tabs of this type AND nothing routes here* — a bus with no
  member tabs can still be a live send destination, and hiding it would strand that cable. Reuse
  `sweepSendsTargeting` as the "what routes to this channel" primitive. UI-only: InsertNodes stay
  allocated and audio routing is untouched, exactly as in `deleteSecondaryBus`.
- [ ] Sweep for other >=1-of-a-type assumptions. Three already verified safe:
  `PianoRollPage::unregisterEngine` falls back to Drum Kit; the ribbon instance dropdown hides
  Pages/Rename/Delete at count 0; project load falls back to the Builder tab.
- [ ] No migration (8b precedent + no-backward-compat-pre-v1): projects saved BEFORE this task
  restore their trio, which is correct — they genuinely contain those tabs.
- [ ] Build gate.

### Task 2 — Template writer v2

- [ ] Refactor the per-tab walk of `serializeUIState` so the tab-capture core (type/pageIndex/
  name/engine/engineData + per-type extras) is callable for a TEMPLATE capture (tabs + aux/vox/
  inst names + orders; NO arrangement, NO patterns, NO song-loop/UI-position extras).
- [ ] Docket 15=B: factor `serializeProject`'s `<Processor>` block (lazy-param materialization +
  `apvts.copyState()` + `VibeRackStates`) into a reusable writer, and have the template emit the
  same `<Processor>` child alongside `<UIState>`. This is what carries mixer faders/pans/widths/
  sends/cables, FX racks and per-insert EQ — `engineData` does not.
- [ ] `saveTemplateAs` writes `<BaySickTemplate version="2" name>` + that capture; dialog text
  updated from "kit + layers + basses" to the full-skeleton wording (ASCII).
- [ ] Build gate.

### Task 3 — Template loader: v2 branch + scoped teardown + dirty gate

- [ ] `loadTemplate`: version switch — v2 -> apply via the deserializeUIState per-type tab
  branches (same shield + aux-clear discipline as project load); v1 factory attribute schema ->
  existing code path unchanged.
- [ ] Docket 15=B apply half: restore the template's `<Processor>` child through the same route
  `deserializeProject` uses (replaceState + `applyPendingRackStates` + aux rebuild), ordered so
  the lazily-registered per-strip params exist before the state lands.
- [ ] Teardown symmetric to scope: v2 tears down all dynamic tabs (it restores all types);
  the v1-factory branch tears down ONLY L/B/D tabs (transitional per §5).
- [ ] Every template-load entry runs through `confirmDiscardChanges` (fixes the verified
  silent-discard bypass); blank/clean project loads directly.
- [ ] **AMENDMENT (docket 19, 2026-07-26 — applies to code already shipped in this task):**
  template load is NEW PROJECT semantics. `applyTemplate` must clear the current patterns /
  arrangement / audio library (the `resetToBlankState` -> `PatternManager::reset()` path) before
  rebuilding, NOT merge into them. The as-shipped v2 branch keeps them; change it.
- [ ] Build gate.

### Task 4 — Sample-retention hybrid (8a/8b) + path resolvers

> Moved ahead of the menu work: every task below needs its `library:` / `mysamples:` resolver.

- [ ] `ProjectManager::importSample` becomes source-aware: source under Core Library ->
  return `library:<rel>` (no copy); source under My Samples -> return `mysamples:<name>`
  (no copy); anything else (volatile) -> copy into `<project>/Samples/` exactly as today.
  This is the fix for Core Library samples being duplicated into every project that uses them.
- [ ] `resolveProjectFile` + `BuilderPage::onResolveStoredPath` gain the `library:` +
  `mysamples:` branches (Core Library dir / getUserSamplesDir). Existing absolute +
  project-relative refs untouched (8b: no migration).
- [ ] ClipsPage import keeps its My-Samples copy behavior but stores the `mysamples:` form for
  new imports (loader accepts bare names as before — no rewrite of old refs).
- [ ] Missing-file handling upgraded from silent skip to a one-shot load report dialog listing
  unresolved refs (no per-block spam) — reuses QA-Export's `MissingFileReport` collector.
- [ ] Build gate.

### Task 5 — `library:` normalization everywhere + template sample copying (dockets 23/24/25)

- [ ] Finish the half-built convention: `library:` currently has FIVE readers and ONE writer
  (`DrumPage.cpp:1007`). Write it everywhere a Core-Library-resident path is persisted —
  Layer/Bass "Save Current Patch As", the sfizz `kitPath` attribute in `serializeTabsInto` (one
  fix covers project save AND template save), Rusty's `KitPath` inside its engine blob, and the
  Clips `clipPath`. Absolute paths embed the Windows username and cannot resolve under another
  account; the existing default-kit fallback hides this as a silent downgrade.
- [ ] Template save copies outside-stable-root samples into `Documents\BaySickDAW\My Samples`
  (docket 23=b) and stores the `mysamples:` reference. Scope (docket 24=c): Clips samples +
  NAM captures + IRs + BaySickPlayer sample paths. sfizz kits EXCLUDED — already Core-Library
  resident, and copying one drags its whole product folder.
- [ ] Rule: copy ONLY what is not already under a stable root. Anything already in Core Library
  or My Samples gets a reference, never a copy — same source-aware logic as Task 4.
- [ ] Dedupe on copy (size + modtime, mirroring `importSample`) so saving two templates from the
  same sample does not accumulate duplicates.
- [ ] Build gate.

### Task 6 — Bundler engine-reference walk + export semantics (dockets 21/22)

- [ ] `ProjectBundler::enumerate` walks engine-held references, closing the documented gap:
  sfizz `kitPath` + Inst chain XML (plain attributes, easy), NAM captures + user IRs (inside the
  chain XML), BaySickPlayer sample paths (inside base64 engine blobs — decode required).
- [ ] Full / SelfContained STOPS copying Core Library content (docket 22=b). Full now means
  "every file not guaranteed to exist on a BaySickDAW install". Update `shouldCopy` +
  the dialog's scope labels so the two modes read honestly.
- [ ] Show a size estimate before the bundle is written (Jeff's addition to docket 22), so a
  multi-hundred-MB export is visible up front rather than discovered afterwards.
- [ ] Delete the now-satisfied "not every file the project needs" caveat from
  `ProjectBundler.h` once the walk is complete.
- [ ] Build gate.
- [ ] **COMMIT 1** (Tasks 1-6): message + FULL git status -> Jeff's approval.

### Task 7 — Automation registry: lifetime + diagnostics + DSP targeting (sweep 5, option 2)

> Backed by [`Research Reports/daw-architecture-non-parameter-automation-binding-2026-07-26.md`](../Research%20Reports/daw-architecture-non-parameter-automation-binding-2026-07-26.md).
> No surveyed system (VST3 / CLAP / Tracktion / Ardour / Vital / Surge / iPlug2 / JUCE) keeps a
> UI-keyed automation applicator map; automation universally targets a model-owned object.

- [ ] **Step 0 (before coding):** ear-confirm the FX-rack hole — automate an FX rack knob on
  Layer 0, switch the Effects page dropdown to Layer 1, play, listen for whether Layer 0's knob
  still moves. Code chain already desk-verified (`EffectsPage.cpp:23` -> `:494` -> `:500-502` ->
  `SlotComponent.cpp:127-133` destroys the prior panel).
- [ ] **Step 1 — b-prime self-cleaning registry.** Register a `ComponentListener` on the owning
  widget + keep a `Component* -> {paramIds}` reverse index; `componentBeingDeleted` (fires first
  in `~Component`, verified at `juce_Component.cpp:275`) erases those ids from both maps. Zero
  changes to the 29 helper call sites — the five wrappers already hold the widget reference.
  Then DELETE `eraseAutomationEntriesWithPrefix` and all ~17 hand-written prefix literals.
  Hazards: use `registerParameterAutomation`'s existing `lifetimeGuard` as the listener target;
  set an `mTearingDown` flag in `~StandaloneEditor` so the callback no-ops during teardown.
- [ ] **Step 2 — dead-lane diagnostics.** Dispatch distinguishes resolved from unresolved;
  `jassertfalse` in Debug once per paramId per session; extend `onIsParamStale` from "not in
  APVTS" to "not in APVTS AND not in the registry" so the Event Editor greys registry lanes.
  Keep the lane INERT, never auto-delete — matches Ardour/Tracktion, where automation data
  outlives the live control.
- [ ] **Step 3 — (e)-lite for FX rack + pedals.** Move those applicators' target from the
  `juce::Slider` to the DSP/slot object, resolved at apply time by the
  `(channelPrefix, slotUuid, knobSuffix)` key already stamped. Each effect type needs a
  `setParamByKey(suffix, value01)` or a per-type suffix table. THIS is the step that actually
  fixes the channel-switch hole; steps 1-2 do not.
- [ ] Keep the `SafePointer` inside the closures — costs nothing, seatbelt if the index misses.
- [ ] Build gate.
- [ ] **COMMIT 2** (Task 7 alone — highest-risk task, isolated rollback point).

### Task 8 — Central app-root resolver (sweep 1)

- [ ] ~43 sites spell `Documents\BaySickDAW` by hand; `EffectPresetIO::presetsRoot()`,
  `SampleLibrary::getUserSamplesDir()` and `ProjectManager::getDefaultProjectsRoot()` are each
  their own island. One resolver, every reader and writer through it.
- [ ] Build gate.

### Task 9 — Three small corrections (sweep 2/3/4)

- [ ] `mPartSel`'s comment is factually WRONG — it claims the control is "invisible by virtue of
  never being addAndMakeVisible'd"; it IS made visible, it just never gets bounds. Fix the
  comment (wrong comments get fixed wherever they live; Rule 6 scoping governs STYLE audits).
- [ ] `InstPage`'s `slot.engineRootTag = "BaySickPedalsState"` names the APVTS type, not the blob
  root, post-QA-Verify. Correct the label.
- [ ] `kStateVersion` is written by two sites and read by none. Either wire a reader or drop it —
  decide at execution and record which, with the reason.
- [ ] Build gate.
- [ ] **COMMIT 3** (Tasks 8-9).

### Task 10 — New-from-Template submenu + default-template consumer + removals

- [ ] File menu: submenu replacing 102+109 — `New from Default Template` (greyed when no
  default; label suffix = default's name) / `Premade Templates >` (recursive Factory walk) /
  `My Templates >` (recursive user walk). Every pick -> unified Load Template flow (Task 2).
- [ ] Default template: `setDefaultTemplate` keeps storing the .xml; the submenu item loads it
  through the same flow — the pointer finally has a consumer. Remove `newProject`'s dead
  folder-seed branch (ProjectManager.cpp:262-270) + update the header contract comment.
- [ ] Remove `doFileNewFromTemplate` (+ its ProjectBrowserWindow launch) and `showTemplateMenu`
  (+ its internal dup `kIdSaveAs`); item 106 top-level Save-as-Template stays. Grep zero refs.
- [ ] Build gate.

### Task 11 — FND-1 completion: Rusty "Save Page Preset & Delete" (docket 16)

- [ ] Rusty has no page-dirty signal at all — add the `mDirtyListener` -> `mPageDirty` ->
  `isPatchDirty()` trio verbatim from `ClipsPage.cpp:28` / `VoxPage.cpp:30`.
- [ ] Rusty's delete prompt gains the 3-button dirty branch mirroring the other six
  (ClipsPage.cpp:435-484 pattern): dirty -> [Save Page Preset & Delete / Delete / Cancel] where
  save chains `savePlayerPresetAs` (the J-11 Player Preset — Rusty's only user-writable save;
  its "kit" is a read-only factory `.sfz`) then fires the delete; clean -> current 2-button
  warning unchanged. `savePlayerPresetAs` gains a completion callback so the chain can fire.
- [ ] Build gate.

### Task 12 — Pack: save-first prompt on the existing Export Project Bundle (docket 17=b)

- [ ] NO new menu item. QA-Export already shipped this as item 122 "Export Project Bundle…"
  (`doExportProjectBundle`) with destination + zip-vs-folder + references-vs-self-contained per
  docket 8; the name stays.
- [ ] Only behavioral change: an UNSAVED project currently gets an info box telling the user to
  save first and bails. Replace that with the standard save flow, continuing into the bundle
  once the save completes (cancel aborts the bundle).
- [ ] Build gate.

## Batch close (bulk-run per-batch loop)

**Commit boundaries — DELIBERATE deviation from one-commit-per-batch (Jeff, 2026-07-26).** The
batch grew 7 -> 12 tasks and roughly doubled in hours; a single rollback point spanning project
save/load + the bundler + the automation registry + the effects audio path would cost every
already-verified task if the smoke failed. Four commits, each surfaced with message + FULL git
status and taken only on Jeff's approval:

| Commit | Covers | Why the seam |
|--------|--------|--------------|
| 1 | Tasks 1-6 | Templates + samples + bundler — one data-layer story |
| 2 | Task 7 | Automation registry alone; highest-risk task, isolated |
| 3 | Tasks 8-9 | Cleanup / corrections, independent of the above |
| 4 | Tasks 10-12 + docs | Batch close |

- [ ] Claude runs `do_build.bat` at every task gate; fix until BOTH configs build clean.
- [ ] Author this batch's Master Test Plan §B section (`blocks:` = the four commits, backfilled).
- [ ] `/draft-doc batch-close` -> held Work Log entry in running notes (R2); no §5 touch.
- [ ] Running-notes code-complete entry (+ Rule 4 rows if any).
- [ ] Final commit (Rule 9): `QA-ProjectSave: <one-line what> (<scope>)` + trailer.

## Verification (authors into Master Test Plan §B)

1. Fresh launch (and File > New): ribbon opens with NO Layers, Bass or Drums tab; app lands on
   Builder. Clicking each of those three ribbon slots shows its empty-state placeholder; badge
   reads 0. Adding one from the slot dropdown works, and the tab behaves as before.
2. Add one Layers tab, then delete it: it goes (no "This is the only Layer tab" refusal) and the
   empty state appears. Same for Bass and Drums. Deleting the last Drums tab does NOT silently
   re-spawn one (old kit-recovery path).
3. Mixer at zero tabs of a type: that type's bus strip is GONE, not an empty strip — checked for
   Layers, Bass, Drums, Clips, Vox and Inst. Master and FX bus always remain. Adding a tab back
   brings its bus back in the right group position with separators intact.
4. Bus visibility follows MEMBERSHIP, not tab-type count. Drag a BASS strip's main-out cable
   onto the Layers Bus (legal per `isRouteAllowed`; a Clips/Audio strip works too) — the strip
   physically moves into the Layers group. Now delete every Layers TAB: the Layers Bus must
   REMAIN (the Bass strip is still bucketed there) and that strip's audio must still flow.
   Re-point the Bass strip back at the Bass Bus -> the Layers Bus disappears.
   This is the scenario a naive "hide the Layers Bus when zero Layers tabs exist" implementation
   fails, which is why it is worth walking.
5. Build a project with one of EVERY tab type (Layers/Bass/Drums/Clips/Vox/Inst-Guitars/
   Inst-Basses/Rusty + an aux strip), Save as Template -> File > New (blank) -> load it from
   My Templates: every tab restores with engine state, names, aux strip back — and the template
   contains NO tabs the project didn't have (no phantom trio).
6. Docket 15=B mixer round-trip: before saving the template, set distinctive fader/pan/width
   values, draw a send cable, load an FX rack and dial in a per-insert EQ curve. After the
   template loads into a blank project, all of it is back.
7. Factory template still loads (v1 branch): kit + layers + basses appear as before; tabs of
   OTHER types present beforehand survive a v1-factory load (scoped teardown).
8. Dirty project + any template pick: Save / Don't Save / Cancel prompt appears (bypass fixed);
   Cancel aborts with nothing torn down.
9. New from Default Template: greyed with no default; after Set Default Template it loads that
   template; label shows the default's name.
10. "New from Template…" (old clone flow) is gone from the File menu; "Load Template…" gone;
    Save as Template dialog text reflects the full skeleton.
11. Rusty tab with a dirtied page -> Delete: 3-button prompt; "Save Page Preset & Delete" writes
    the Player Preset (verify it reloads from the Player Preset dropdown) then removes the tab;
    clean page -> 2-button warning.
12. Import a Core Library sample into the Builder: NO copy appears in `<project>/Samples/`;
    save/reopen: plays (library: ref). Import from Downloads: copy appears; plays after reopen.
    Clip add from My Samples: no duplicate copy; plays after reopen.
13. Delete a referenced volatile file on disk -> open project: the load report names it; the
    rest of the project loads normally.
14. Export Project Bundle from an UNSAVED project: the save flow runs and the bundle follows
    (cancel aborts cleanly). Bundle (references, folder) -> move the pack elsewhere + rename the
    original project folder: opens and plays. Bundle (self-contained, zip) -> unzip elsewhere:
    opens with Core-Library-independent samples. No second "Pack Project…" item exists.
15. Old project saved pre-batch opens unchanged (8b: absolute + Samples/ refs untouched) and
    still restores its original Layers/Bass/Drums tabs (no migration — those tabs are real).
16. **Docket 19:** load a template into a project that HAS a song. Patterns, notes and the
    arrangement are CLEARED — you land on the template's rig with a blank canvas, exactly as if
    you had done File > New then loaded it. (Save prompt still fires first if the project is dirty.)
17. **Docket 25:** save a Layer patch whose sample came from Core Library, then inspect the preset
    XML — the path reads `library:...`, not `C:\Users\<you>\...`. Same for a saved template's
    sfizz `kitPath`. Copy that file to a second Windows account and it still resolves.
18. **Dockets 23/24:** build a template using a Clips sample from Downloads, a NAM capture and a
    BaySickPlayer folder from outside the library. Save it -> those files appear in
    `Documents\BaySickDAW\My Samples`; the template references them there. Delete the originals
    from Downloads -> the template still loads and plays. sfizz kits do NOT get copied.
19. **Docket 21:** export a bundle from a project using a NAM capture + a user IR + an
    engine-loaded sample folder. All three are IN the bundle (before this batch they were
    silently omitted). Open the bundle on a clean machine: everything resolves.
20. **Docket 22:** the export dialog shows a size estimate before writing. Self-contained no
    longer copies Core Library content — a project using a 555 MB sfizz kit produces a small
    bundle, and that kit still loads on the target machine.
21. **Task 7 step 3 (the reason the task exists):** automate an FX rack knob on Layer 0, switch
    the Effects page dropdown to Layer 1, play. Layer 0's automation KEEPS APPLYING. Same for a
    pedal knob across a slot rebuild. Before this batch both went silently dead.
22. **Task 7 steps 1-2:** close a tab with automated rack knobs, reopen, re-automate — no stale
    behavior; an automation lane whose target no longer exists shows greyed in the Event Editor
    instead of silently doing nothing.
23. **Task 8:** every folder the app creates or reads still lands in the same place as before
    (Templates, Presets, My Samples, MIDI, Projects, IR) — the resolver is a refactor with no
    user-visible change.

## Routing notes (Rule 3)

Template/preset findings during execution fold here; anything touching the §E preset-walk
surfaces logs for the campaign. Undo/dirty behavior of the new menu flows routes to
QA-UndoCoverage/QA-DirtyFlag (next two batches).

## Carry-Forward Reference touch points

§3 persistence decisions + the QA-Ef close entries (Work Log) before Tasks 1-3; the
project-load shield discipline (loadTemplate comment block :6941-6951) is the pattern every new
load path must keep.
