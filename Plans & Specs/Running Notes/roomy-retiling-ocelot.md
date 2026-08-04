# Running Notes — QA-Layout (roomy-retiling-ocelot)

> Append-only mid-batch log for QA-Layout.  A new entry lands at every checkpoint —
> commit landed / sub-task verified / finding captured / spec call resolved / scope
> pivot — via `/draft-doc running-notes` dispatches, per
> `feedback_draft_doc_running_notes_every_checkpoint.md`.  At batch close,
> `/draft-doc batch-close` reads this file as the primary input when compiling the
> Implemented Work Log entry.  Per L32 the Work Log entry is expected to HOLD to the
> G4 boundary (confirm with Jeff at close).

Pair file: [`Plans & Specs/Batch Plans/roomy-retiling-ocelot.md`](../Batch Plans/roomy-retiling-ocelot.md).
Convention: Main Plan §0 "Batch Plans + Running Notes layout" (locked 2026-05-11).

## 2026-08-03 — Planning session — scope locked, plan approved

- **Batch plan approved by Jeff and landed** (this file's pair).  Planning session ran in-chat:
  Jeff's authored spec `Files For Claude/Final V1 Layout.md` read first, then §0 + Carry-Forward
  §1–§3 + the mammoth held entries, then five parallel read-only source sweeps, then 16 dockets
  + follow-up rulings.  All rulings are in the plan's locked table (L1–L32); genuinely deferred
  calls are D1–D7.
- **Supersessions on record:** locked call 5a REVERSED (full-screen toggle ships; order
  preset | full-screen | close); the held "engine pickers onto title bars" note superseded —
  pickers are DELETED (Layers/Bass combos, Clips decorative combo, Drums "Pick a sound"
  button); Test Plans §B.31.0's drag-and-report floor collection superseded by the diag-driven
  flow (rewritten in place at T6); the held "Live Instrument" rename lands as **BaySickLiveInst**
  (+ menu) / **LiveInst** (tab/strip/titles); + menu keeps "BaySickVocal" singular; VST entry
  reads "VSTPlugin".
- **Structural rulings worth restating:** Window-7 = five sub-page windows (Pitch / Align /
  Vocal Chain / Pedals / NAM-IR), in-page views retired; the pedalboard IS the LiveInst player
  (tab click fronts it; NAM/IR button on its title strip; no Inst page window for live-input
  tabs); ribbon dropdown "Pages:" becomes a per-instance window list (LiveInst rows read
  "Pedals"); three-lifetime persistence in-batch with crash survival = autosave timer flush;
  stretch = native resize + free transform zoom for fixed-size plugins; caps Layers 20 /
  Bass 10 / Drums 32 / Clips 100 / Vox 10 / Inst 30 with the PR-target shift accepted and the
  second drum-kit PR entry workshopped before code (D3); Add menu rework + four new group buses
  (Layers/Bass/Clips/Plugins, one each, kVoxBus2 pattern) with the used-once-then-hide
  lifecycle; SYS coloring per-token; BLU-110 three-zone limiter panel IN (Jeff: "Build it");
  VibePlayer knobs literal ~18px.
- **Sequencing (L7):** T1–T5 before the T6 diag handoff (title-bar work + Window-7 land before
  Jeff sizes anything, so he sizes the decoupled windows); T9/T10/T11 may run while his sizing
  pass is in flight; T7/T8 wait on his numbers; D1/D2 re-docketed with data.
- **Verification shape:** part of G4 — NO batch smoke; per-task build gates only; this batch's
  §B section authored at code-complete (T14) and walked at the G4 boundary.  Bridged-specific
  `1cd1f5d6` items (program-name relay, param-touch relay, 32-bit path) recorded as UNTESTED —
  the smoke must not assume them.
- **Source corrections made during planning, recorded so they are not re-derived:** the
  perf-readout overlap is a 120-vs-160 gutter mismatch, not a text-length problem alone;
  `kMaxAudioRows` caps audio CHANNELS (Clips pages / Audio strips), NOT Builder grid rows — the
  grid has 500 rows and clips route many-to-one via `routeChannel` (the constant's name is a
  pre-QA-E fossil); every channel-id type owns a 100-wide block, so no cap raise needs
  re-basing; raising instance caps is constants + literal sweep, NOT the new-strip-type ~15-site
  job (Inst was bumped 6→10→20 in G-4/G-6 exactly that way); the vocal-chain saturation bug is
  the `sat_type` 0..1 range clamping Tape=2 back to Console every block.
- **CLAUDE.md stale fact noted for T14:** ArrangementGrid constants say kNumRows=32; source says
  500 (`BuilderPage.h:601`).

## 2026-08-03 — Task 1 committed `80b2f1f2` — transport readout + ribbon visuals + app title

- **Build gate green:** five exit codes 0, four `vcxproj -> ...exe` link lines, zero
  `error C` / `error LNK` / `error MSB` greps.
- **Perf readout rebuilt as `TransportPerfReadout`** (custom component in
  GlobalTransportBar.h/.cpp, replaces the juce::Label): three 9pt monospaced rows —
  SYS/DSP, MEM/LAT, UND/PF — inside the same 40px bar (L24).  Per-token coloring per
  L20: the SYS token colors off whole-machine CPU alone (same 50/80% thresholds);
  DSP load/overload colors the DSP token plus rows 2–3; the 95% overload flash is
  unchanged.  A Label cannot draw split colors — that is why the custom component
  exists.
- **Gutter fix:** `kCPUReserve` in StandaloneEditor::resized() now derives from
  `TransportPerfReadout::kWidth` (120) + 12 (4px bar inset + 8px gap).  The old
  literal 120 disagreed with the label's 160px rect — the 120-vs-160 mismatch
  diagnosed in planning is what let the readout overlap the ribbon "+" slot.
- **L25 two-row ribbon tabs** (RibbonTabBar.h/.cpp): name on the top row
  (`kNameRowH` 22); badge + arrow on the bottom row; arrow hit zone = bottom-row
  right in `hitTestSlot` (name-row clicks navigate).  `naturalSingleLineWidth`
  dropped its arrow/badge width terms.  camelCase wrap machinery retired —
  `splitCamelCase` + `slotWraps` deleted, paint's wrap branch removed; long labels
  shrink via `drawFittedText` 0.75 minScale.  Frozen-tab dot moved to the bottom
  row's left edge so it cannot touch the name.  Arrow glyph 26pt → 16pt to fit the
  18px bottom row.  Stale "index 6" ctor comment rewritten in-region (Rule 6).
- **FINDING — fixed in-region (Rule 3 fold-in, T1's own surface):** `kMaxSlots` was
  11 ("10 types + '+' slot"), but QA-ModelShell TS6's Plugins type made it 11 types
  + the "+" slot = 12 possible slots — `slotRect()`'s stack arrays
  (`desired` / `minW` / `widths[kMaxSlots]`) overflowed by one whenever every type
  was visible at once.  Latent until a project holds all six instance types + a
  Plugins tab simultaneously.  Bumped to 12.
- **L27:** `order[]` reordered to Builder / Mixer / Effects / PianoRoll then the
  instance types — order array ONLY; the persisted `addFixed` ids untouched.
- **L26:** `VibeLAF::drawDocumentWindowTitleBar` (SharedUI.cpp) now stock-JUCE
  placement — icon + title centered as one unit, clamped into the title space;
  reverts TS7's left-align + icon drop.  Applies to every non-native DocumentWindow;
  the main frame is the only icon-setter.  Second wrong-comment fix in the same
  pass: SharedUI.h claimed the main app window keeps OS chrome via a native title
  bar — false; VibeSynthWindow never opts in and paints through this override
  (which is why TS7's left-align was visible on the app title at all).
- **`STANDALONE_UI_CHANGES.md`** gained a QA-Layout T1 entry per that file's
  standing convention for deliberate UI changes.

## 2026-08-03 — Task 2 committed `148e192a` — add menus + engine pickers deleted + page-menu merges + LiveInst rename

- **Build gate green:** five exit codes 0, four `vcxproj -> ...exe` link lines, zero
  `error C` / `error LNK` / `error MSB` greps.  17 files, 445 insertions / 580 deletions.
- **SPEC CALL (Rule 5, resolved in chat before landing):** the ribbon dropdowns' generic
  "+ Add New Layers/Bass/Drum" rows would have become dead ends once L4 deleted the engine
  pickers — they create ENGINELESS pages whose picker no longer exists.  Jeff's ruling: each
  tab type's dropdown bottom section lists the PLAYERS that type can load, engine-named.  His
  map, confirmed row by row: Layers = Harmless / BaySickPlayer / BaySickSynth; Bass =
  Harmless / BaySickPlayer / BaySickBass; Drums = BaySickPlayer / BaySickSynth (+ existing
  BaySickRustyDrums row); Vox = BaySickVocal (+ existing From-Export submenu); Inst =
  BaySickLiveInst (+ existing Guitars/Basses rows, all gated on the shared cap); Clips =
  "+ Add BaySickPlayer..." keeping the -3 file-picker route (BaySickPlayer implied — Jeff:
  fine, matches the + button, the manual will cover it); Plugins = "+ Add VSTPlugin >" side
  menu.  Added constraint: the Plugins side menu AND the + menu's VSTPlugin submenu both stay
  ALPHABETICAL (both ride the sorted `getAddedInstruments()`).  Rows fire
  `onAddEngineRequest` so the engine loads at creation — no route can create an engineless
  page anymore.
- **showAddMenu rebuilt to the locked L2/L3/L28 order:** BaySickVocal, BaySickLiveInst,
  BaySickGuitars, BaySickBasses, VSTPlugin submenu, Harmless > Layers/Bass, BaySickSynth
  (flat -> Layers), BaySickPlayer > Layers/Bass/Audio Clips, BaySickBass, BaySickDrums >
  BaySickPlayer/BaySickSynth (new, absorbs the drum routes), BaySickRustyDrums.  The old
  "BaySickPedals" + menu string had NO code consumers (`applyEngineToNewestTabOfType`
  ignores the engine string for Inst tabs) — the rename was display-only.  The
  EngineRow/Target table scaffolding was replaced by direct sequential construction.
- **L4 deletions:** LayersPage + BassPage LockableCombo + "Engine:" label rows (struct
  deleted from both headers); ClipsPage's decorative LockedClipsEngineCombo; DrumPage's
  "Pick a sound v" button + sound-name label + PickerRightClickListener.  Engine editors now
  fill their Player tabs full-height — the close-the-layout-gap step; full re-layout waits
  on T7 floors.  Placeholder paint texts updated: Layers/Bass "No engine loaded" (engineless
  pages only reachable from pre-QA-Layout saves), Drums "No sound loaded - pick one from the
  Drum Kit" (kit pads remain the empty-drum sound-pick route via `showSoundPicker`).
- **Menu merges — one Delete per page:** each affected page's engine context menu merged
  into its Menu-dropdown `showPageActionsMenu`.  LayersPage/BassPage: full merge (Lock /
  Polyphony / Rename / Duplicate / Choke / Save Patch / Load Preset + Save/Load Page Preset
  + Delete); old separate `showPageActionsMenu` deleted, `showContextMenu` deleted.
  DrumPage: `showPageActionsMenu` now FORWARDS to `showContextMenu(anchor, fromKit=false)`,
  which gained the page-preset entries in the `!fromKit` branch; kit pads keep
  `fromKit=true` (MIDI Note / MIDI Learn rows, no page presets).  ClipsPage:
  `showEngineContextMenu` merged into `showPageActionsMenu` — its "Save Current Patch
  As..." / "Load Preset" were already aliases of the page-preset routines, deduped; Load
  Page Preset keeps the context menu's WIDER root (factory + user recursive
  `clipsPresetsRootDir`) instead of the old page menu's My-Presets-only walk.
- **L2 LiveInst rename:** `nextInstTabName` -> "LiveInst N" (StandaloneEditor.h); InstPage
  ctor default; MixerPage strip default name + `getInstStripName` fallback; input-picker
  section header "LiveInst Input" (was "Instrument Input" — the third spelling, reconciled
  per the mammoth held note).  FLAVOR-vs-FAMILY line drawn and recorded: family-generic
  "Inst" labels that also cover Guitars/Basses strips stay "Inst" (automation "Inst N"
  prefix labels in PluginProcessor / StandaloneEditor, "Inst Bus" strips, browser category
  tag, channel-list fallbacks) — renaming those would mislabel Guitars/Basses.  No tab-name
  prefix parsing exists (grep-verified), so the rename breaks nothing.
- **L31:** PageMenuBar's "=" hamburger is now a 46px "Menu" TextButton (`kMenuBtnW` in
  SharedUI.h, shared by resized() and the paint() title x-offset).  Grep-verified it is the
  app's ONLY hamburger-style button — every window title strip shares PageMenuBar.
- **Stale comment fix (Rule 6, in-region):** RibbonTabBar.h's file-header comment block
  rewritten — it claimed "NO add" dropdowns and drag-drop-only Clip spawning, both long
  false.
- **`STANDALONE_UI_CHANGES.md`** gained the T2 entry.  VoxPage's decorative picker
  deliberately NOT touched — not in L4's locked list; its page is rebuilt wholesale in T4
  (Window-7).

## 2026-08-03 — Task 3 committed `5065a616` — full-screen toggle + engine title bars dissolved + preset relocation + Menu correction

- **Build gate green — TWICE:** the initial T3 build, then a re-run after the mid-approval
  L31 correction (below); five exit codes 0, four `vcxproj -> ...exe` link lines, zero
  `error C` / `error LNK` / `error MSB` greps both times.  28 files, 566 insertions /
  159 deletions.
- **L5 full-screen toggle (REVERSES locked call 5a's no-maximize half, as the plan
  records):** every WorkspaceWindow gets a `FillToggleButton` left of close — path-drawn
  maximize/restore glyph, no font dependency.  `toggleWorkspaceFill()` saves current
  bounds to `mRestoreBounds`, fills the workspace (parent-client space: workspace origin +
  size, then `clampToWorkspace`), toggles back.  A manual title-drag or border-resize
  while filled CLEARS the filled flag (`mouseDrag` + `Constrainer::applyBoundsToComponent`
  — only user gestures route through the constrainer), so the next click fills again
  rather than restoring a stale rect.  Button order right-to-left: close, fill toggle,
  then PageMenuBar right-extras (preset).
- **Window-4/L2 engine title bars dissolved:** VibePlayer / BaySickSynth / BaySickBass /
  Harmless / BaySickPedals editors lost their internal BaySickTitleBar members + the 32px
  layout row — content starts at 0 (VibePlayer's `boxRectFor` dropped its kHdrH terms,
  Synth/Bass visualizers to y=0, Harmless dropped its removeFromTop, Pedals grid
  full-height).  Each editor keeps identity as `getEngineTitle()` / `getEngineAccent()`
  statics + `getTitleStripPresetButton()`.  `VibePlayerEditor::setInfoText` DELETED —
  caller-less API whose only target was the dead bar.  The colored player name now renders
  CENTERED on the window title strip via new `PageMenuBar::setCenterTitle`
  (BaySickTitleBar::paintEngineName bloom painter, 15pt, rect sized to text since the
  painter is left-anchored).  The small grey tab-title suppression behavior unchanged —
  D7 reviews narrow-width collisions.
- **Window-3 preset relocation:** pages (Layers/Bass/Drum/Clips) expose
  `stripEngineTitle()` / `stripEngineAccent()` / `stripPresetButton()` (dynamic_cast
  cascade over their editor types) + fire a new `onEngineEditorRebuilt` callback at the
  end of `selectEngine`.  StandaloneEditor's page-show branches run a `syncStripChrome`
  lambda at show AND wire it to the rebuild callback — needed because the add path SHOWS
  the page BEFORE `applyEngineToNewestTabOfType` lands the engine, and a Drums kit-pad
  pick can SWAP the engine while visible.  `PageMenuBar::ExtraComp` hardened raw
  `Component*` -> SafePointer (an editor-owned mounted button dies on engine swap; dead
  entries skipped in resized without consuming width); double-mount guarded by parent
  check.
- **RustyDrums back on the strip — reverses QA-G3Smoke G-16:** Player Preset (110px) +
  Program combo (160px) moved from the AriaControlPanel title bar back onto the strip;
  the existing page accessors from the pre-G-16 era made this trivial.  The Aria bar
  itself STAYS (Guitars/Basses/Rusty identity — not in the dissolution list).
  BaySickPedals' strip mount deliberately WAITS for its T4 window — its preset button is
  unmounted in the interim; nobody runs the app before T4 lands (recorded, not silent).
- **L23 + FINDING — fold-in, resolved in-region:** the live-input Inst page's clip-name
  label mount removed (StandaloneEditor Inst branch).  The label is DUAL-USE — the member
  stays because sfizz sources drive it as the program display on the Aria bar; only the
  LiveInput strip mount (which nothing ever updated — permanent "(no audio loaded)")
  died.  Caller-less `getClipFileLabel()` accessor deleted; InstPage.h comments
  corrected.
- **MID-APPROVAL CORRECTION (Jeff, in chat, before the commit landed):** T2's cut of L31
  shipped "Menu" as a chrome TextButton — WRONG read of "text button".  Jeff wants a
  NATIVE MENU-BAR-STYLE text heading, like "File" on a main window: flat text,
  hover/press highlight only, no button chrome.  Fixed as `TitleStripMenuItem`
  (TextButton subclass painting flat: WindowChrome::titleText 13pt centered + subtle
  white hover fill) — one shared class, every window strip corrected at once.  The
  correction rides this commit; the build gate re-ran green after the fix.
- **PLAN CLARIFICATION (at Jeff's request, batch plan T10 body edited in this commit):**
  T10's L13 "Add" entry is pinned to render exactly like the corrected "Menu" — a flat
  TitleStripMenuItem-style text heading, NOT a chrome button; the mixer strip reads
  "Menu  Add" like a native window's menu bar.  Also answered in chat: L30 (MIDI trigger
  velocity -> Audio Settings beside the MIDI inputs) is confirmed T10 scope, and the
  mixer add-buttons-to-dropdown work is T10 per the plan's sequencing (runs during the
  sizing pass), not missing.
- **`STANDALONE_UI_CHANGES.md`** gained the T3 entry incl. the L31 correction bullet.

## 2026-08-03 — Task 4 in flight — D4 resolved (=c) + escapes + L22 landed (uncommitted)

- **D4 RESOLVED (Jeff in chat, option c) — enumeration first, per the plan:** the Vox/Inst
  ribbon dropdowns' "EQ" row has been MISLABELED DEAD WEIGHT since J-6 — it fires sub-page
  index 1, which on a Vox tab opens the VOCAL CHAIN (tabs shifted when the Pre Rack EQ tab
  was removed) and on an Inst tab hits whatever sits at slot 1 (NAM/IR on live-input,
  Pedals on Guitars/Basses).  Jeff's ruling: under the L11 rework, EVERY instance type's
  dropdown (Layers/Bass/Drums/Clips/Vox/Inst/Plugins) gets "Pre EQ" / "Post EQ" rows that
  open that strip's Pre/Post EQ windows directly (the Effects satellites via
  `openEffectEqWindow`), in addition to the per-instance window list.
- **D6 RESOLVED FROM SOURCE (no pose needed — the plan's "pose only if ambiguous"):**
  Guitars/Basses Inst tabs DO carry Pedals + NAM/IR today —
  `InstPage::getActiveTabLabels` returns { engineLabel, "BaySickPedals",
  "BaySickNAM/IR", "Piano Roll" } for sfizz sources — so they keep that carriage as
  satellite windows opened from their title strips.
- **FINDING (VoxPage, fix rides T4):** `VoxPage::showEngineContextMenu` is CALLER-LESS
  dead code — its Lock / Rename / Duplicate entries + the wider factory+user preset root
  are currently UNREACHABLE on Vox pages (the Menu dropdown's `showPageActionsMenu` only
  has Save/Load Page Preset + Delete).  T4 merges the orphaned content into
  `showPageActionsMenu` (the same treatment the other pages got in T2), restoring the
  lost access.  Also dead: VoxPage's `mEnginePicker` member + `buildEnginePicker` stub
  (H-6b left scaffolding, never shown) — cleaned in-region with the T4 VoxPage work.
- **LANDED (uncommitted, ride the T4 commit) — findParentComponentOfClass escapes,
  converted BEFORE re-hosting per the plan:** `BaySickPitchEditor::showSendNotesMenu` now
  uses injected `onListNoteTargets` / `onSendNotes` callbacks (local mirror types
  NoteTarget/SentNote keep StandaloneEditor.h out of the header);
  `BaySickPitchSubEditor::refreshFromRegion` now fires an injected `onTitleChanged`
  (wired by BaySickPitchSubEditorWindow to `setName`) instead of
  `findParentComponentOfClass<DocumentWindow>`.
- **LANDED (uncommitted, same commit) — L22 saturation fix:** BaySickVocalProcessor
  `sat_type` range widened 0..1 -> 0..2 with default Console (the 0..1 range clamped
  Tape=2 back to Console on every per-block APVTS push), plus SaturationDSP.h `mSatType`
  default Tube -> Console covering the pre-first-push window — both exactly per the
  plan's locked code block.
- **T4 remaining (design settled, in progress):** BaySickVocalEditor loses its in-page
  tab switcher (layout = BaySickVocals main panel only) + gains panel accessors; four Vox
  satellites (Vocal Chain / Pitch / Align / NAM-IR) + the Inst Pedals/NAM-IR satellites
  host the page-owned panels NON-OWNED through per-tick resolvers (the EffectWindows
  satellite pattern); L10 LiveInst = the pedals window IS the player (no Inst page
  window; NAM/IR button on the pedals strip); the ribbon "Pages:" section becomes a
  per-instance window-row model (StandaloneEditor builds label+action rows; the ribbon
  displays) with the D4=c EQ rows appended for every type; persistence keys designed per
  T5's scheme.

## 2026-08-03 — Task 4 committed `85128436` — Window-7 Vox satellites + L10 pedals-as-player + L11 window rows

- **Build gate — one intermediate FAILURE, then green on the final tree:** 8x C2440 —
  the new BaySickVocalEditor panel accessors upcast forward-declared panel types
  inline in the header; fixed by moving the four accessor bodies to the .cpp.  Final
  build: five exit codes 0, four `vcxproj -> ...exe` link lines, zero
  `error C` / `error LNK` / `error MSB` greps.  18 files, 797 insertions /
  637 deletions.
- **This entry COMPLEMENTS the Task 4 in-flight entry above** — D4=c, D6, the
  findParentComponentOfClass escapes, the L22 saturation fix, and the VoxPage
  dead-menu merge are recorded there and ride this commit; not restated here.
- **Window-7 Vox delivered:** BaySickVocalEditor's tab machinery (TabIdx enum,
  `setActiveTab`, `panelForTab`, `mActiveTab`) DELETED — content is the BaySickVocals
  main panel only, and the panel's internal BaySickTitleBar dissolved (the Window-4
  treatment extended: the panel IS the window content now; the Vox strip shows
  centered "BaySickVocals" teal).  Four panel accessors added (bodies in the .cpp —
  the C2440 failure above).  The four satellites open via
  `StandaloneEditor::openVoxSatelliteWindow` through a new file-local
  `PanelSatelliteView` — hosts the editor-OWNED panel NON-OWNED via a per-tick 4Hz
  peer-keyed resolver; resolve-fails-after-success => `onRequestClose` (the
  EffectSlotWindow contract).  Keys `voxsat:<idx>:{chain|pitch|align|namir}`,
  Session persistence until T5, provisional floors: chain 560x420,
  pitch/align 900x560, namir 640x420.  Vox strip = four launcher slots
  (activeIdx -1, the PluginsPage precedent) + page Menu + FX Rack slot; the
  satellites' strips carry the page Menu too.
- **L10 LiveInst delivered — the pedals window IS the player:** `showPageForTab`
  short-circuits a LiveInput InstPage (no `hostPageInWindow` —
  `openInstPedalsWindow` + front instead), AND `hostPageInWindow` itself refuses
  LiveInput InstPages (covers the project-load sweep until T5 reworks it).  The
  pedals window (`instsat:<idx>:pedals`, 880x480) wears the TAB NAME as title,
  centered "BaySickPedals", page Menu, the pedalboard preset button (T3's deferred
  mount, via `getPedalboardPresetButton`), and a "NAM/IR" launcher slot.
  `openInstNamIrWindow` (`instsat:<idx>:namir`, 640x420).  sfizz Inst tabs keep
  their page window (Aria player, `mPlayerTab` visible per `setSource`) with
  {Pedals, NAM/IR, Piano Roll} launcher slots.  InstPage sub-tab machinery
  (`switchTab`, `mActiveTab`, `getTabLabels`, `getActiveTabLabels`,
  `mPedalsPlaceholder`) deleted; the Pedals / NAM-IR editors stay page-owned, no
  longer page children.
- **L11 + D4=c delivered — ribbon "Pages:" is now a per-instance window-row model:**
  RibbonTabBar's hardcoded rows replaced by `onListPageWindowRows` /
  `onPageWindowRowPicked`; `StandaloneEditor::buildPageWindowRows` is ONE body
  serving labels AND pick dispatch (rebuilt at pick time — no stale indices).  Rows
  per type: Layers/Bass/Clips/Plugins = Player + Piano Roll (nav); Drums =
  Drum Kit (nav) + Player + Piano Roll (nav); Rusty = Drum Kit + Player + Piano
  Roll (its EQ rows tap `kRustyDrumsBus` — the kit's one whole-signal point); Vox =
  Player + the four satellites; Inst LiveInput = Pedals + NAM/IR; Inst sfizz =
  Player + Pedals + NAM/IR + Piano Roll.  EVERY list ends with the D4=c Pre EQ /
  Post EQ rows -> `openEffectEqWindow`.  `onSubPageSelected` slimmed to
  Effects/Builder (instance branches deleted).
- **Tab-close hygiene:** the Vox/Inst close branches call `closeVoxSatellites` /
  `closeInstSatellites` — a deliberate close beats the resolvers' timed one.
- **FOUND EN ROUTE — cleaned in-region (Rule 6):** BaySickVocalEditor's HostPanel +
  PlaceholderPanel classes were pre-existing DEAD code — every sub-tab they
  scaffolded shipped real content long ago and nothing instantiated either —
  deleted with their forward decls.
- **ENUMERATED, no edit needed:** the SlotComponent Saturation Mode menu already
  offers Tube/Console/Tape and pushes `setSatType` + mirrors `bsv_sat_type` via
  `onModeChanged` — L22's whole bug was the param range clamp, fixed in the
  in-flight entry's landed item.
- **`STANDALONE_UI_CHANGES.md`** gained the T4 entry.
- **Next:** T5 (window-state persistence, three lifetimes).

## 2026-08-03 — Task 5 committed `fba55012` — three-lifetime window persistence + persist-key collision closed

- **Build gate green FIRST TRY:** five exit codes 0, four `vcxproj -> ...exe` link
  lines, zero `error C` / `error LNK` / `error MSB` greps.  7 files, 393 insertions /
  72 deletions.
- **LIFETIME 1 (universal in-memory):** WorkspaceWindow's session map is now the ONE
  live store — `saveBounds` always writes it (workspace-local), `loadSavedBounds`
  always reads it FIRST, for Disk and Session windows alike.  Close/reopen returns to
  the same spot for every window type, players included, no disk involved.
- **LIFETIME 2 (settings.xml):** the per-close parse-and-rewrite of the whole file is
  GONE.  Disk-marked windows register in a `diskEligibleKeys` set;
  `WorkspaceWindow::writeSessionToSettings` runs ONCE in
  `VibesynthStandaloneApp::shutdown`, deliberately AFTER editor teardown (window
  destructors are the last map writers, so the flush sees final bounds).  Filtered per
  the mammoth ruling: w/h for every eligible window; x/y only for keys in
  `placementKeys` (`hostPageInWindow` registers the four default tabs —
  Mixer/Builder/Effects/PianoRoll); player x/y attributes are STRIPPED from old
  records.  `loadSavedBounds` treats a record without x/y as a SIZE-ONLY seed
  (`attachTo` keeps the size, takes the default cascade position) — "the size I like
  my windows" carries across projects, placement does not.
- **LIFETIME 3 (project file):** `serializeUIState` writes a `<Windows>` element — one
  `<W key x y w h>` per map entry (`flushAllWindowBounds` sweeps live windows first)
  plus one `<Open key>` per live window (pages by persist key; effect
  windows/satellites by aux key).  `deserializeUIState` REPLACES the map, sets an
  `mLoadingWindows` guard (`hostPageInWindow` refuses mid-load — kills the three
  force-framing sites without touching them), and at end-of-load frames EXACTLY the
  saved-open set: page keys match via `pageIndexOfEntry`; aux keys re-dispatch
  (`fx:<ch>:<uuid>` resolves the slot by uuid over `EffectRack::kNumSlots`; `eq:` /
  `voxsat:` / `instsat:` / `analyzer:master` route to their open functions) so content
  always rebuilds from live model state.  A pre-T5 project (no `<Windows>`) frames
  nothing — tabs are one ribbon click away (accepted, pre-v1 no-migration).
- **L16 crash survival rides the EXISTING 15-min autosave:**
  `ProjectManager::writeBackup` serializes the project, which now includes
  `<Windows>`, and `flushAllWindowBounds` inside the serializer is the "timer flush"
  (resizes have no end-of-gesture hook).  A crash loses at most one autosave interval
  of layout.
- **persistKeyFor defect CLOSED — plan's diagnosis half-corrected:** the fill cascade
  covered only Layers/Bass/Drums, so every Clip/Vox/Inst/Plugins window collided on
  `"type:-1"` (one shared saved position).  New `pageIndexOfEntry` resolver (full
  7-type cast cascade, -1 for system pages + the Rusty singleton) serves
  `persistKeyFor`'s fallback, `hostPageInWindow`'s hint fill, and the load-time reopen
  matcher.  The plan's "fix the wrong comment" half was already fixed upstream (the
  key body matched its comment); the LIVE defect was the fill gap.
- **`STANDALONE_UI_CHANGES.md`** gained the T5 entry.
- **Next:** T6 — the sizing diag + WxH readout + floors drop + the Test Plans §B.31.0
  rewrite, then the HANDOFF to Jeff's sizing pass.  Jeff asked in chat for a complete
  checklist of every window to size so coverage can be verified at hand-back — being
  compiled as part of the T6 handoff.

## 2026-08-03 — Task 6 committed `7ac6844e` — sizing diag + floors drop + §B.31.0 rewrite + the HANDOFF

- **Build gate green FIRST TRY:** five exit codes 0, four `vcxproj -> ...exe` link
  lines, zero `error C` / `error LNK` / `error MSB` greps.  7 files, 154 insertions /
  39 deletions.
- **THIS IS THE HANDOFF POINT (L7):** Jeff's sizing pass is now in flight.  T1–T5
  landed the decoupled windows first, per the plan's sequencing, so everything he
  sizes is final-shape chrome; everything below is the collection apparatus.
- **`[QA-Layout DIAG]` instrumentation landed** (full sites in the catalog row
  below — that row was updated from placeholder to the full site list in this same
  commit): live WxH strip readout (yellow monospace,
  `WorkspaceWindow::paintOverChildren` — drawn OVER the PageMenuBar, clear of the
  close/fill buttons); per-size-change append of
  `persist-key | title | WxH [| Basic/Advanced]` to
  `Documents/BaySickDAW/window-sizing-diag.txt` (deduped per size via
  `mLastDiagSize`; effect windows report their mode through a new
  `onDiagExtraInfo` hook -> `EffectSlotWindow::diagPanelMode`); ALL floors dropped
  to 120x80 (ctor default + `setMinimumSize` ignores every caller's provisional
  floor — the real body is preserved in a comment for T7 to restore).  Compiled
  into BOTH configs DELIBERATELY (Jeff asked in chat): layout collision points
  don't differ by config, so the pass can run in the Release exe.
- **L1 delivered — Test Plans §B.31.0 rewritten IN PLACE:** the drag-and-report
  12-row table replaced by the diag-driven flow — two takes per window (floor +
  comfortable; the last settled line per key wins), effects sized in both modes
  where the Basic/Advanced toggle exists, per-engine takes for Layers/Bass/Drums,
  "natural" notes for fixed grids, hand-back artifact = the diag file, coverage
  verified against the T6 checklist below before T7 starts.
- **THE COVERAGE CHECKLIST — delivered to Jeff in chat at his request; the diag
  file is verified against it at hand-back, before T7.  Recorded in full:**
  A. page windows — Builder, Mixer, Effects rack, Piano Roll, Layers x {Harmless,
  BaySickPlayer, BaySickSynth}, Bass x {Harmless, BaySickPlayer, BaySickBass},
  Drums x {BaySickPlayer, BaySickSynth}, Clips, Vox, Inst x {BaySickGuitars,
  BaySickBasses}, Rusty (Drum Kit + Player views).  B. sub-page windows — Vocal
  Chain, BaySickPitch, BaySickAlign, NAM/IR (Vox + LiveInst keys, same layout),
  Pedals board.  C. Pre EQ (covers Post) + Master Analyzer.  D. effect panels —
  Compressor {FET, Opto, CS}, Reverb, Chorus, Delay {Echo, VocalDoubler},
  Saturation {Tube, Console, Tape}, Flanger, Overdrive {Rack, Pedal}, Phaser,
  Transient Shaper, Tape, Limiter {Limiter, Maximizer — FLAGGED for a T13
  re-check since BLU-110 rebuilds the panel}, De-esser, Gate, De-reverb, plus the
  18 pedal-native rack-loadable types (BluesDrive, Distortion, Fuzz, Noise Gate,
  HighGain, Tuner, Acoustic Preamp, GraphicEQ, Synth, Octave, Wah, Bass
  GraphicEQ, Bass Compressor, Bass Driver, Bass Overdrive, FurmanEQ, Acoustic
  Simulator, NAM Pedal) — each x Basic/Advanced where the toggle exists.
- **Checklist EXCLUSIONS:** hosted VST3 windows (plugin-derived floors — T12
  stretch) + the desktop popups (Event Editor, Key Binds, Undo History, Plugins
  manager, Rusty Map, Pitch Sub-Editor).  The old §B.31.0 row 12 (Event Editor)
  is SUPERSEDED.
- **Sequencing now (per L7 + the session brief):** Jeff sizes — Debug smoke first,
  the pass itself runs in Release.  T9 (piano-roll control-lane resize) starts
  immediately, then T10 (mixer menus + Add menu + group buses), then T11 opening
  with the D3 drum-kit workshop (no cap code before that ruling).  T7 (real
  floors + layout reworks) and T8 (Window-6 collapse, D1/D2 re-docket) WAIT on
  the diag doc.

## Diagnostic Instrumentation Catalog (Rule 4)

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| `WorkspaceWindow.h` (onDiagExtraInfo + paintOverChildren decl + mLastDiagSize), `WorkspaceWindow.cpp` (ctor 120x80 floor, setMinimumSize override w/ commented-out real body, resized() diag append, paintOverChildren WxH readout, AppPaths include), `EffectWindows.h/.cpp` (diagPanelMode), `StandaloneEditor.cpp` (openEffectSlotWindow onDiagExtraInfo wire) | `[QA-Layout DIAG]` | Window-sizing collection (T6): per-size-change append of persist-key + title + WxH + effects panel mode to `Documents/BaySickDAW/window-sizing-diag.txt`; live strip readout; floors dropped to 120x80 | Remove at batch close (T7 restores real floors in setMinimumSize; readout + append + hook + diagPanelMode all strip) |
