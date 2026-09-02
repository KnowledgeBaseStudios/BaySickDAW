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
  map, confirmed row by row: Layers = BaySickSolstice / BaySickPlayer / BaySickSynth; Bass =
  BaySickSolstice / BaySickPlayer / BaySickBass; Drums = BaySickPlayer / BaySickSynth (+ existing
  BaySickRustyDrums row); Vox = BaySickVocal (+ existing From-Export submenu); Inst =
  BaySickLiveInst (+ existing Guitars/Basses rows, all gated on the shared cap); Clips =
  "+ Add BaySickPlayer..." keeping the -3 file-picker route (BaySickPlayer implied — Jeff:
  fine, matches the + button, the manual will cover it); Plugins = "+ Add VSTPlugin >" side
  menu.  Added constraint: the Plugins side menu AND the + menu's VSTPlugin submenu both stay
  ALPHABETICAL (both ride the sorted `getAddedInstruments()`).  Rows fire
  `onAddEngineRequest` so the engine loads at creation — no route can create an engineless
  page anymore.
- **showAddMenu rebuilt to the locked L2/L3/L28 order:** BaySickVocal, BaySickLiveInst,
  BaySickGuitars, BaySickBasses, VSTPlugin submenu, BaySickSolstice > Layers/Bass, BaySickSynth
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
  BaySickSolstice / BaySickPedals editors lost their internal BaySickTitleBar members + the 32px
  layout row — content starts at 0 (VibePlayer's `boxRectFor` dropped its kHdrH terms,
  Synth/Bass visualizers to y=0, BaySickSolstice dropped its removeFromTop, Pedals grid
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
  A. page windows — Builder, Mixer, Effects rack, Piano Roll, Layers x {BaySickSolstice,
  BaySickPlayer, BaySickSynth}, Bass x {BaySickSolstice, BaySickPlayer, BaySickBass},
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

## 2026-08-03 — Task 9 committed `43313911` — piano-roll control-lane header-drag resize

- **Build gate — two locked-exe blocks, then green:** the first run hit the EXPECTED
  LNK1104 on the Release link (Jeff mid-sizing-pass in the Release exe; per the
  exe-lock convention, no rebuild until he was out) — the code compiled clean in both
  configs, and Debug + both helpers were green on that run.  A second relink attempt
  also hit the lock (Jeff was adding a late Vocal Chain take).  Final relink after he
  closed the app: five exit codes 0, four `vcxproj -> ...exe` link lines, zero
  `error C` / `error LNK` / `error MSB` greps.  7 files, 270 insertions /
  31 deletions (PianoRoll.h/.cpp, DrumKitGrid.h/.cpp, StandaloneEditor.cpp,
  STANDALONE_UI_CHANGES.md, this file).
- **Header-drag resize:** the 16px lane header doubles as the resize handle — a 3px
  drag threshold decides drag-vs-click, and the mode dropdown moved from mouseDown
  to mouseUp so a clean click still opens it; up-down resize cursor over the header.
- **ONE shared height for every lane app-wide:** new `ControlLane::get/setUserHeight`
  statics (clamped kHeaderH 16 = collapsed min .. kHeight 240 = max).
  DrumKitControlLane mirrors with the identical gesture and reads the same statics
  (DrumKitGrid.cpp now includes PianoRoll.h); the other containers lockstep off
  their existing 200ms timers (resized() when lane height != the shared value).
  The grid keeps its 120px floor.  Dead constants deleted: both containers' kLaneH
  aliases + DrumKitControlLane::kHeight.
- **Persistence — new `<ControlLane h= visible=>` element in the project
  `<UIState>`** (the T5 lifetime-3 store — rides autosave/crash flush), restored at
  the top of `deserializeUIState` BEFORE the tab rebuild.  `visible` = the last
  settled Velocity Lane toggle from ANY container, stored via a
  `ControlLane::get/setDefaultVisible` static — the default new containers open
  with; in-session toggles stay per-container.
- **Placement call (flagged to Jeff on the commit surface):** the element lives in
  the PROJECT store, NOT settings.xml — the settings writer is strictly
  WorkspaceWindow's window-record store — so lane height follows the project, not
  the machine.
- **`STANDALONE_UI_CHANGES.md`** gained the T9 entry.

## 2026-08-03 — Sizing pass complete: diag doc reviewed + map approved; new Task 15 ruled in (strip buttons dissolve into Menu)

- **The hand-back:** Jeff finished the sizing pass and handed back
  `window-sizing-diag.txt` — 13,287 raw lines -> 37 settled groups after the
  last-line-per-key/title/mode parse, plus a late Vocal Chain take he flagged in
  chat.
- **MID-SIZING FINDING + RULING — lands NOW as Task 15, before T10:** every player
  page window's title-strip buttons sitting between the Menu dropdown and the swing
  knob stop being strip buttons and become entries inside that window's own Menu
  dropdown.  Per page: Rusty + Drums {Drum Kit, Player, Piano Roll};
  Layers/Bass/Clips {Player, Piano Roll}; Vox {Vocal Chain, BaySickPitch,
  BaySickAlign, NAM/IR}; Inst {Pedals, NAM/IR, Piano Roll}; Plugins {Piano Roll}.
  Explicitly EXCLUDED (Jeff: fine as-is): the Piano Roll page's cluster (target
  pill / Player Page / FX Rack) and the Pedals satellite window's NAM/IR button.
  Purpose: declutter + make the centered strip titles visible.  ALSO in Task 15:
  BaySickRustyDrums, BaySickGuitars, BaySickBasses never got the T3 title
  treatment — their internal title bands dissolve and their names go centered on
  the title strip like every other engine.  (Jeff caught this from his sizing
  screenshots; my first two readings of the ask were wrong — the corrected scope
  is the above.)
- **SIZING MAP APPROVED (Jeff, single approval over the 8-line list):**
  engine-attributed page sizes — BaySickSolstice 1047x455, BaySickPlayer 490x455 (Clips
  too), BaySickSynth + BaySickBass 558x455; Mixer 486x455, Builder 486x268,
  Effects rack 357x268, Piano Roll page 691x268, Vox 1534x455, Inst
  Guitars/Basses 1047x455 each, Rusty 1047x455; sub-pages — Vocal Chain 1047x723,
  BaySickPitch 1534x724, BaySickAlign 1047x723, NAM/IR 843x563 (Vox + LiveInst
  same layout), Pedals board 1534x455; Pre/Post EQ + Master Analyzer 1047x455
  each.
- **Effect generics:** Basic 691x268, Advanced 1047x268 (the 13 toggled panels);
  the no-toggle full panels (Flanger, Gate, De-reverb, FET/Opto/CS compressor
  variants) 691x268 per the Gate exemplar; ALL pedal-native panels 358x268
  (normalized from Bass Compressor 358x267 + Polyphonic Synth 355x268).  Three
  stray 1022x482 lines discarded as default-open-size noise (LiveInst 10 pedals +
  no-mode Compressor + no-mode Limiter).  The Limiter/Maximizer re-check after
  T13 (BLU-110) stays flagged; anything not fitting its generic gets fixed when
  found (Jeff's rule).
- **Sequencing:** T7/T8 now UNBLOCKED (the approved map = T7's real floors input).
  Order: Task 15 now -> T10 -> T11 (D3 workshop first) -> T7 -> T8.  T15 lands
  before T7 DELIBERATELY: it changes strip contents, which affects the min widths
  T7 will encode.

## 2026-08-04 — Task 15 committed `7a0758b0` — strip nav buttons into Menu dropdowns + sfizz titles

- **Build gate green FIRST TRY:** five exit codes 0, four `vcxproj -> ...exe` link
  lines, zero `error C` / `error LNK` / `error MSB` greps.  20 files, 422 insertions /
  237 deletions.
- **Batch plan updated at Jeff's explicit request ("Update the plan"):** new Task 15
  section after Task 14, and the ordering paragraph now records T15 executing between
  T9 and T10 plus the post-hand-back order T15 -> T10 -> T11 -> T7 -> T8 -> T12+.
- **Buttons -> Menu entries:** every player page's title-strip buttons (between Menu
  and the swing knob) are GONE; the same entries now sit at the TOP of that window's
  Menu dropdown with a tick on the active local view.  Mechanism: a new
  `onBuildWindowNavMenu` std::function hook on all seven page classes, invoked at the
  top of `showPageActionsMenu` (DrumPage: inside `showContextMenu`, !fromKit only, so
  kit pads keep their per-drum shape); Rusty gets the entries directly inside its
  editor-side menu builder.  The entries are JUCE action-lambda PopupMenu items —
  verified in the vendored JUCE source that they carry itemID -1 and self-dispatch,
  so they coexist with every id-dispatched page menu (Rusty's `r <= 0` guard skips
  them by design).
- **Per page:** Rusty + Drums {Drum Kit, Player, Piano Roll}; Layers/Bass/Clips
  {Player, Piano Roll}; Vox {Vocal Chain, BaySickPitch, BaySickAlign, NAM/IR}; Inst
  {Pedals, NAM/IR, Piano Roll}; Plugins {Piano Roll}.  The QA-E Sub-Phase A
  capture-locals-before-onTabSelected discipline is preserved verbatim in every
  Piano Roll nav action.  EXCLUDED per Jeff: the PianoRollPage jump cluster, the
  pedals window's NAM/IR launcher, and the EQ windows' Pre/Post pair — those keep
  setTabSlots (the API's remaining users; the SharedUI.h usage comment was updated
  to say so).
- **Drums' Menu builder now installs unconditionally** (was Player-sub-tab-only) so
  the nav entries are always reachable — post-J-6 Player is the only local sub-tab,
  the gate was vestigial.
- **The small grey tab title returns to player strips** (instance name, e.g.
  "Layer 1") — it was suppressed only while tab slots existed.
- **sfizz titles (the missed T3 treatment):** BaySickRustyDrums + BaySickGuitars +
  BaySickBasses internal AriaControlPanel title bands dissolved by leaving
  `binding.engineName` empty (the panel only builds its BaySickTitleBar when the
  name is non-empty); the names now center on the window strip via setCenterTitle
  (Rusty red #CC2222, Inst navy #1C3A8A).  The four widgets the Inst band hosted
  (program label 200px + Load button 130px + CUT SELF 62px + mode toggle 78px)
  re-home to the strip's right extras per page-show; the CUT SELF/mode APVTS
  ButtonAttachments are now wired independently of any title bar — previously the
  whole attachment block was gated on `getTitleBar() != nullptr`, so clearing the
  name WITHOUT this decoupling would have silently killed the G-14 cut-self
  feature.
- **Dead/stale cleanup in touched regions:** the Rusty branch's unused safeBar
  SafePointer deleted; wrong comments fixed (ClipsPage.h header, SharedUI.h
  setTabSlots note, the Vox/Inst/Drums/Plugins branch comments).
- **`STANDALONE_UI_CHANGES.md`** gained the T15 entry.
- **Next:** T10 — mixer menu moves, per-strip "+" target dropdowns, the Add titled
  menu + four group buses, the L14 lifecycle.

## 2026-08-04 — Task 10 committed `3639cb98` — mixer rework: group buses, Add menu, target submenus, L30, L14

- **Build gate green FIRST TRY:** five exit codes 0, four `vcxproj -> ...exe` link
  lines, zero `error C` / `error LNK` / `error MSB` greps.  13 files, 995 insertions /
  625 deletions.
- **PLAN-PREMISE CHECK at task open:** an Explore survey claimed the plan's "four new
  group buses" already exist (kLayersBus=1 etc.) — verified the agent misread the
  PLAN, not the code: the four are SECONDARY buses on the kVoxBus2 pattern (the
  "Layers Bus" Add row = add a second Layers bus), ids 14-17 =
  kLayersBus2/kBassBus2/kClipsBus2/kPluginsBus2, consistent with "next bus ids after
  13".  Also verified L13's dropped "Add Vox/Inst Strip" buttons lose nothing: the
  ribbon "+" add flow already creates the mixer strip (StandaloneEditor
  onAddTabRequest calls addVoxChannelAtIndex/addInstChannelAtIndex).
- **FOUR SECONDARY GROUP BUSES — full kVoxBus2-pattern registration, cross-checked
  against `reference_mixer_strip_pattern_audit.md`:** VibeGraph registry rows
  (prefix/isBus/friendlyName/defaultSendTo), always-allocated InstrChannelNodes,
  processBus + peak/RMS atomics + drains, latency-solver slots (array 11->15), SC bus
  lists, addNode/wipe/restoreNode, rebindApvts, kBusSoloPrefixes 12->16 (+ mBusSoloPtr
  array), getScSourceTap, armBusMeters, pushScArrayToStrip, rebuildRoutingFromApvts
  mActiveChannels (the critical site), PluginProcessor params
  (ensureMixerBusAndMasterParams) + kNumBatch7Buses 12->16 + render tasks + pre/post
  EQ tables + peak mirrors, MixerPage strips (ONE shared activateGroupBus body + four
  thin wrappers, family accent colors), strip cache, stems list, meter drains, route
  rules (active-gated), EffectsPage dropdown ids 14-17 + prefix maps + rack/preEQ
  resolvers, StandaloneEditor automation labels (kBusEntries) + active-channel rows,
  PagePresetIO inactive-bus fallbacks.  Automation lanes: live via the generic
  mixer-strip param registration; offline via the APVTS-id-generic
  applyOfflineLaneValue — no per-bus offline branch exists to add (verified; satisfies
  the EngineRig rule).
- **L13 — the Add menu:** PageMenuBar gained setAddMenuBuilder — a second flat
  TitleStripMenuItem heading ("Menu  Add"), hidden on pages without a builder, cleared
  in the branch-top clear.  Mixer's Add menu = the ruled SEVEN rows exactly (Aux
  Strip; Vox/Inst/Layers/Bass/Clips/Plugins Bus), bus rows greyed at cap.  The five
  title-strip buttons DELETED (members, accessors, creation); dead
  addVoxChannel/addInstChannel wrappers deleted too.
- **L12 — per-strip "+" target submenus:** the "+" now opens Send... / Sidechain... /
  Move Output... submenus enumerating concrete targets from getStemPickEntries(),
  filtered by isValidBusSendTarget / isRouteAllowed / wouldCreateCycle (illegal =
  disabled rows, current main-out ticked, "New Aux Strip" row preserves the old
  auto-create).  RETIRED: click-to-place send/SC modes, main-out socket drag, ghost
  cables, red rejected-drop flash, CableOverlay's Timer base,
  findSocketNear/findStripUnder.  KEPT: cable painting, right-click cable menus,
  Master's "+" = Analyzer, the slot finders (the menu commits through them).
- **L14 — used-once-then-hide lifecycle:** per-secondary-bus has-ever-had-route flags
  (all seven: Vox2/Inst2/Inst3 + the four new).  A fresh bus stays visible while
  never-routed; used-then-emptied auto-deactivates in laidOutBus (flag-only drop, no
  param sweep, no re-entrant relayout).  New Buses element in
  serializeStripNamesAndOrders (structural — rides templates) persists active +
  everRouted; restore pass after the strip-order restore; clearDynamicStrips now
  resets ALL seven secondary buses on project load (Vox2/Inst2/3 previously leaked
  across projects — with activation persisted, the reset is the correct half of the
  contract).  Plus: a routed-to inactive secondary bus SELF-ACTIVATES via a deferred
  callAsync in layoutScrollContent (preset/project loads write _sendTo before any flag
  arrives; also makes per-page setBusActiveQuery wiring unnecessary for the new
  buses).
- **L30 — trigger velocity relocated:** the "MIDI trigger velocity" submenu (ids
  204/205) removed from the Mixer Menu; AudioSettingsDialog gained a "Trigger
  Velocity:" combo (From controller / Fixed) below the MIDI inputs, applied live
  (QA-L-Fix D-11 hot-swap), settings.xml persistence unchanged, dialog height math
  +1 row.
- **Next:** T11 opens with the D3 drum-kit workshop in chat — no cap code before
  Jeff's ruling.

## 2026-08-04 — D3 RULED (drum-kit second-16 mechanics)

- **Posed at T11 open per the docket; Jeff ruled 1(c) + 2(a).**  ONE "Drum Kit" entry
  stays in the piano-roll target list; a switch INSIDE the kit view flips between the
  two sixteens.
- **Mapping is FIXED by page index:** drum pages 1-16 belong to view 1, pages 17-32
  to view 2; a drum NEVER moves between views — deleting one leaves a gap in its own
  view.
- **Restated as ruled-and-accepted:** the PR-target shift lands with T11, and
  pre-existing projects' piano-roll routing is invalidated once.

## 2026-08-04 — Task 11 committed `4722c27c` — L18 caps + literal sweep + two-sixteens kit

- **Build gate green FIRST TRY:** five exit codes 0, four `vcxproj -> ...exe` link
  lines, zero `error C` / `error LNK` / `error MSB` greps.  16 files, 305 insertions /
  88 deletions.
- **L18 caps:** kMaxLayerPages 8->20, kMaxBassPages 4->10, kMaxDrumPages 16->32,
  kMaxClipPages 50->100, kMaxVoxPages 6->10, kMaxInstPages 20->30 (Plugins stays 20 —
  not in L18).  New MixerChannelIds mirrors kMaxLayerStrips / kMaxBassStrips /
  kMaxDrumStrips / kMaxAudioStrips + kMaxVoxStrips 6->10, kMaxInstStrips 20->30.
  kMaxAudioRows / kMaxAudioInserts 50->100 across PatternManager / PluginProcessor /
  VibeGraph (the existing static_assert keeps the trio locked).  ID space verified:
  audio 400..499 ends flush at kDrumBase 500; every range fits its century.
- **PR-target shift:** the PRPendingOff bases are derived cap sums
  (VibesynthConstants), so the bump shifts every downstream target — the accepted
  one-time invalidation (D3 entry above), noted in the constants comment.
- **LITERAL SWEEP FINDINGS — all fixed in-batch.**  TWO LATENT OVERFLOWS:
  StandaloneEditor::mUsedLayerIndices was `std::array<bool, 8>` while its fill loops
  run to kMaxLayerPages (writes past the end at cap 20), and Pattern::layerRoll was
  `std::array<PianoRollData, 8>` (same overflow class).  TWO STALE RANGE BUGS live
  at the OLD caps: PatternManager ownerCategory checked Vox 600..605 / Inst 700..705
  (6-wide since before the caps grew to 6/20 — audio-group category detection was
  already broken for Inst strips 7-20), and BuilderPage's group-assign prompt +
  clip-block colors had the same stale 606/706 bounds.  Plus stale-but-loose
  literals normalized to constants: VibeGraph
  prefix/friendly/defaultSendTo/pushScArrayToStrip (+16/+50), MixerPage
  pickStripColor + isRouteAllowed + aux checks (aux 16 vs kMaxAuxStrips 18),
  EffectsPage aux dropdown 16 -> kMaxAuxStrips + audio dropdown 450 -> 500,
  PluginProcessor pre/post EQ-sync tables (8/4/16/50 -> constants).
- **Two-sixteens kit (D3 1c+2a):** DrumKitContainer gained a "1-16 / 17-32" toggle
  pair beside the Kit button — the PianoRollPage kit AND DrumPage's kit both inherit
  it (one container implementation).  The container now stores the RAW row provider
  + raw row-click/audition/reorder handlers; children get a view-filtered provider,
  and every child row index is translated back to a raw index before the stored
  handlers fire — they index the raw kit list (verified in wirePianoRollPageKitView
  BEFORE coding; unwrapped filtering would have mis-dispatched view 2's clicks).
  The sidebar's add row maps past the RAW end so the add branch still fires; noted
  consequence: a new drum always fills the lowest free page slot, so an add clicked
  from view 2 can land in view 1.
- **`STANDALONE_UI_CHANGES.md`** gained the T11 entry.
- **Next:** T7 — real window floors from the approved sizing map + the layout reworks.

## 2026-08-04 — Task 7 committed `9797f19d` — measured floors, window-placement bug (4 causes), BaySickSolstice redo, pedal tiles

- **Build gate green:** five exit codes 0, four `vcxproj -> ...exe` link lines, zero
  `error C` / `error LNK` / `error MSB` greps.  16 files, 949 insertions /
  517 deletions.  Several earlier runs were blocked by LNK1104 on a locked Release
  exe — twice the cause was a STALE `BaySickDAW` process still running after Jeff
  had closed the window; found via `Get-Process` and the PID surfaced to him rather
  than killed (his exe, his call).
- **REAL FLOORS FROM THE SIZING MAP:** `WorkspaceWindow`'s real `setMinimumSize` body
  is restored (the T6 diag-era override ignored every caller and forced 120x80), and
  a new `setMinimumWindowSize` serves callers that pass WINDOW dimensions rather than
  content dimensions.
- **Floors resolve per PLAYER TYPE, not per tab type** — `StandaloneEditor::floorSizeFor`
  keys off the engine a page currently holds, re-applied on every page-show so a Drums
  tab's live engine swap tracks its floor.  Jeff's correction 2026-08-04, which drove
  the rewrite: "there is no split between a layers harmless and a bass harmless, its
  all the same size".
- **The values as landed:** BaySickSolstice 1047x455; BaySickSynth AND BaySickBass 558x455
  (Jeff corrected a first pass that split those two and used a non-existent 586x549);
  BaySickPlayer 490x455; BaySickGuitars / BaySickBasses 1047x455; Rusty 1047x455;
  Vox 1534x455; Mixer 486x455; Builder 486x268; Effects rack 357x268; Piano Roll
  691x268; Vocal Chain 1047x723; BaySickPitch 1534x724; BaySickAlign 1047x723;
  NAM/IR 843x563 (Vox AND Inst identical — Jeff asked that the identity be verified,
  and it was); Pedals 1534x455; Pre/Post EQ + Master Analyzer 1047x455.
- **Effect panel floors are LIVE, not one-shot:** Basic 691x268 / Advanced 1047x268 /
  pedal-native 358x268, pushed through a new `EffectSlotWindow::onFloorChanged` hook so
  a Mode swap or a Basic/Advanced toggle re-floors the open window instead of leaving a
  stale minimum behind.
- **Minimums are HARD (Jeff ruled option B, grow-only):** "why would you offer something
  that makes the windows so small you can't see the shit on it which is the whole point
  of sizing them".
- **PROCESS FINDING, recorded so it is not repeated:** my first floor map was built from
  a MID-PASS snapshot of `window-sizing-diag.txt` taken while Jeff was still sizing, and
  I treated it as final — the file kept growing afterwards and several numbers were
  wrong.  Lesson: read the hand-back artifact once the hand-back is actually MADE, not
  while the producing pass is in flight.
- **WINDOW PLACEMENT — the headline bug of this task** ("the Mixer doesn't save where I
  move it").  FOUR independent causes, found in this order; the fourth was only isolated
  after three failed fixes by adding temporary save/write/restore tracing to a file (with
  Jeff's agreement, removed before the commit):
  1. **A project overwrote the global store.**  A project file stored the four default
     tabs' bounds AND replaced the whole in-memory map on load, so its stale copy beat
     `settings.xml`.  Projects no longer write or restore those keys; live global entries
     now survive a project load; the key set is SEEDED at editor construction, because
     registering at first framing was too late whenever a project loaded first.
  2. **The teardown save clobbered good values.**  The destructor-time save persisted
     whatever a half-dismantled window happened to report, and it ran LAST, so it won.
     Destructors no longer save; explicit saves fire on drag-release, resize/move, the
     close button, and the fill toggle, plus one flush at shutdown BEFORE editor teardown.
  3. **THE BIG ONE — restore ran against a workspace that was not laid out yet.**
     `originInParentClient()` returned (0,0) instead of the real (1,91), so every restored
     window landed 91px high on every launch; the same unlaid-out read also collapsed the
     first-open default size (a workspace fraction) down to the 480x320 floor, which is
     what littered `settings.xml` with junk records.  Attach now waits for real workspace
     BOUNDS, not merely a native handle, and a fresh window opens at its own measured
     minimum instead of a workspace fraction.
  4. **The startup clamp captured windows.**  The frame passes through a partial size
     (~1098x608) before reaching the real 1534x724, and `clampWindowsIntoView` squeezed
     everything into that partial size with nothing left to restore them.  A GROWING
     workspace now re-applies each window's stored bounds before clamping again, and
     programmatic clamps are excluded from the store via a scoped suppression so a clamp
     can never be mistaken for a user placement.  Jeff verified: "It works now".
- **CORRECTION recorded (twice wrong before the above):** I first asserted the behavior was
  "working as intended" per the T5 spec — wrong, the Mixer is a DEFAULT tab and does persist
  placement — and then asserted the workspace genuinely was 1098x608 — also wrong, that is a
  mid-layout reading, and Jeff pointed out BaySickPitch was sized to the full 1534x724
  workspace, which proved it.
- **BaySickSolstice (Specific-2) REDONE, not re-hung** — Jeff rejected a first attempt that only
  re-flowed the existing sections into columns.  Two root causes fixed: `layoutRow` laid
  every item in ONE row and let wide sets overflow their cell (Output / Timbre / FX), so it
  now WRAPS and centers the block; and a blank grid row plus a blank bottom-left half plus a
  blank row-C half were being held as "future space" while real sections were squeezed, so
  every cell now carries content.  New map — TOP band: Output / (Tremolo | Routing) /
  Vibrato-Legato, then Unison alone at full height, then (Filter 1 | ADSR) / (Filter 2 |
  ADSR) / Timbre at FULL width.  BOTTOM band, six columns: Pitch-LFO Mod | Strum/XYZ |
  Blur-Prism/AmpEnv | FX | Spectrogram | Mod Editor.  The cramped 2x2 filter-offset /
  part-mask stack inside Timbre is DISSOLVED (those knobs were shrinking to ~12px).  Design
  size is now 1039x421 — the content area of the measured 1047x455 window.
- **VibePlayer L15:** `kKnobSz` 55 -> 18, stacks vertically centered, routing-arrow math
  follows the new size.
- **Specific-4 pedal tiles:** all 26 pedal-capable panels now have a `PanelMode::Pedal`
  branch, via a shared `pedalTileGrid` that generalizes the hand-built Octave / FurmanEQ
  grids.  The fader-bank EQs and the Tuner keep their own layouts and reclaim the dBFS
  strip.  `isPedalNativeType` moved to `EffectRack.h`, now shared with preset routing.
- **ALSO IN THIS COMMIT (both Jeff-reported during T7):** Effects and Piano Roll now open at
  launch alongside Builder and Mixer.  Correction to an earlier draft of this entry, which
  blamed a "2026-07-28 Builder + Mixer only rule" — no such rule was ever made.  Jeff's rule
  has always been that those FOUR are the default launch windows; the launch policy simply
  framed two of them, which was an implementation error on my side, not a spec change.  Two
  of the four global-placement windows were shut, so half the placement store was never
  exercised.
  And a kit load no longer frames its drums: loading a 16-drum kit put SIXTEEN player windows
  on screen.  Tabs, mixer strips and piano rolls are still created; a drum's window now
  appears when its tab is selected, and the post-load landing spot moved to the DRUM KIT view.
- **Option B recorded as Future State `CL-305`** (one shared drum-player window with a
  dropdown) — Jeff's ruling was "do A for now and notate B as a possible future state"; the
  entry ships in this same commit with its three open questions unanswered.
- **DISPOSITION flagged to Jeff at the commit surface:** the plan's subtractive-size-math
  sweep has NOTHING to sweep — restored floors make below-design-size states unreachable, so
  no live sub-floor paint path remains.
- **ALL Rule 4 diagnostics REMOVED in this commit:** the `[QA-Layout DIAG]` WxH title-bar
  readout, the per-resize file append, the `onDiagExtraInfo` / `diagPanelMode` hooks, AND the
  temporary `[WINPOS DIAG]` placement tracing added for cause 4.
- **Next:** T8 — the D1/D2 collapse re-docket, now that real floor numbers exist.

## 2026-08-05 — LEDGER GAP at T20 open — five commits carry no running-notes entry — RULED: no backfill

The last entry above is T7 (`9797f19d`); the batch has since shipped `8c610c6c` (T8),
`3cfdf4c2` (T16), `94da6a6f` (T12), `d07d710f` (T17) and `9bcb510c` (T13 + T19) with no
entry for any of them.  Those sessions did the work and wrote the commit messages but
skipped the per-commit ledger step the Tasks preamble mandates.

**Jeff ruled 2026-08-05: no backfill — "it is what it is."**  The five commit messages
stand as the record for those tasks; they are unusually detailed, which is what makes that
workable.  Anyone reconstructing T8 / T16 / T12 / T17 / T13 / T19 reads
`git log 9797f19d..9bcb510c`, not this file.  Recorded so the gap reads as a decision
rather than as an oversight nobody noticed.

## 2026-08-05 — Task 20 committed `a6d6ed60` — Delay panel rebuild + reference-grouping review

- **Build gate green first run:** five exit codes 0 (`RELEASE` / `DEBUG` / `HELPER64` /
  `HELPER32_CONFIG` / `HELPER32`), four `vcxproj -> ...exe` link lines, zero
  `error C` / `error LNK` / `error MSB`.  No new warnings in either touched file — the
  `C4996 juce::Font::Font` hits in `EffectEditorPanels.cpp` are pre-existing and at
  untouched lines.  2 files, 106 insertions / 141 deletions (EffectEditorPanels.cpp,
  DelayDSP.h).
- **Root finding CONFIRMED by arithmetic, not by eye.**  Basic filtering covered ROW 1
  only.  At the 691px Basic window the panel has 589px of knob room per row after the
  dBFS + Vol cluster; twelve knobs in it is ~23px a slot, and `layoutKnobsH` sizes the
  WHOLE VKnob to `min(slotW, rowH, kKnobSz)` — so a 23px slot produces a 23px-wide
  label, which is what rendered "M..." / "Df...".  Above 44px the slot width stops
  mattering (the label is capped at the knob size), which is why every other row in
  every mode was fine and only this one was not.  Basic was the same panel squeezed.
- **Locked Basic list falls out as PURE INDEX FILTERING** — `r1knobs {0,1,4,5,8}` =
  Time / Feed / Wet / Dry / Tone, `r2knobs {6,7,8,11}` = FBDst / FBKnee / FBSym /
  Smooth, both in exactly the order Jeff enumerated, with no knob reordering and no
  knob changing row between modes.  New widths: Basic 77px (row 1) and 96px (row 2);
  Advanced 55px and 59px.  All clear 44.
- **The flagged ambiguity RESOLVED from source, as the plan asked.**  "The 3 fb knobs"
  = FBDst / FBKnee / FBSym: they bind `setFBDistLevel` / `setFBDistKnee` /
  `setFBDistSymmetry` and are selected by the Limit/Sat toggle.  FBCut / FBReso bind
  `setFeedbackCutoff` / `setFeedbackResonance` and are selected by the FB Filter
  chicken-head — the feedback FILTER, Advanced.
- **Sync division moved up** into a single 3x2 selector grid, directly above the BPM
  switch that gates it: `[Model][SyncDiv][FBFilter]` over `[Pitch][BPM][Limit/Sat]`,
  one column width across both rows so the pairs align.  Pitch sits at the left of its
  row so Smooth (its documented dependency, and the last knob in row 2) stays beside it.
- **Dead space traced — it was NOT horizontal.**  Two causes, both in the right-hand
  cluster.  `DualLabelToggle` draws from the TOP of its bounds (54px of content in
  OnOff mode), so a full-row-height cell left ~60px of blank under every switch; and
  `ChickenHeadSelector::getKnobBounds` sizes the head to `min(w,h)` minus a 26px letter
  ring, so the old 66px cell drew a 32px head next to 44px knobs.  Cells are
  content-sized now: 74 wide is the width that puts the head back at `kKnobSz`, 54 tall
  is the toggle's natural stack.
- **`FbCurveDisplay` DELETED (93 lines), not parked.**  It was a Component wrapper with
  its own timer and its own `liveDsp()` resolver, and T18's Delay visual is an
  `EffectVisualStrip` paint callback inside `EffectVisualWindow` — so the wrapper does
  not survive the move and keeping it alive for one commit to delete it next would be
  dead code either way.  The reusable half is `DelayDSP::shapeFeedbackForDisplay`,
  untouched.  **T18 INPUT:** the Delay visual owes a 96-point sweep of that function
  (input vertical, output horizontal, `#00FFF2`) alongside the beat-grid repeats; the
  note is parked in the DelayPanel header comment where T18 will be reading.
- **T18 PLUMBING CONSEQUENCE, recorded now:** `Menu > Visual` greys off
  `DSPBase::hasVisualFeed()`, which means "publishes to the visual feed".  Most of
  T18's ten are PARAMETRIC draws with no feed at all (LFO scopes, transfer curves,
  harmonic bars, the delay grid, the reverb envelope — `EffectVisual.h` says so in its
  own design note), so every one of them would show greyed-and-unusable under the
  current predicate.  T18 needs a `hasVisual()` distinct from `hasVisualFeed()`, or the
  predicate widened, in the same pass as the first parametric visual.
- **Wrong comment fixed outside the edited region** (`DelayDSP.h`): the
  `shapeFeedbackForDisplay` block said "for the panel's graph", which stopped being
  true with this commit.
- **Grouping review (the task's second bullet) — walked all 14 Basic-capable panels.**
  The Delay was the ONLY one with the defect; the plan's "thirteen other panels do this
  properly" holds.  Checked the two shapes where it could hide: panels calling the
  full-vector `layoutKnobsH` overload (which cannot skip individuals) are Reverb row 2,
  Saturation both rows, TransientShaper and Limiter — and in the last two those calls
  sit inside `adv`-only branches while Basic runs a hand-built list.  Reverb's
  unfiltered row 2 is 8 knobs at 58px in Basic and Saturation's are 4 at 84-101px, so
  neither is at risk.
- **ONE grouping candidate surfaced to Jeff, deliberately NOT acted on:**
  TransientShaper Advanced row 2 is three knobs (Wet / FastRel / SlowAtt) spread across
  ~683px — 227px a slot — while FastRel and SlowAtt are functionally Attack/Release
  refinements sitting a row away from Attack and Release.  Which knobs group with which
  is a spec call (Jeff locked the Delay's list himself), so it waits on his ruling.
- **No Rule 4 diagnostics added.**
- **Next:** T18 — the remaining nine effect visuals, starting from the `hasVisualFeed()`
  predicate problem above.

## 2026-08-05 — T20 follow-up — Visual entry becomes a presence gate (T17 ruling REVERSED) + TransientShaper regroup

Jeff ruled all three items surfaced at the T20 commit.

- **T17's "greyed + unusable" Visual entry is SUPERSEDED — it is a PRESENCE GATE now**
  (Jeff verbatim: "so it's there or it's not there instead of grey out and make
  unusable").  The batch plan's T17 bullet is struck through in place with the reversal
  recorded beside it.  Jeff's original T17 reasoning was the locked-Freeze parallel — an
  entry that vanishes tells the user nothing about whether the feature exists — and the
  parallel does not survive contact: Freeze is a capability the user can UNLOCK, so it
  has to announce itself and its tooltip carries the unlock path.  An effect with no
  visual has nothing to offer and no route to acquiring one, so the row is permanent
  noise in every other effect's menu.  `mVisualDisabledReason`
  (`std::function<juce::String()>`) became `mVisualAvailable`
  (`std::function<bool()>`); `appendStandardItems` folds it into `haveVisual`, so it
  also drives the early-return and the separator rather than leaving a bare separator
  above nothing.
- **`DSPBase::hasVisual()` added, DELIBERATELY distinct from `hasVisualFeed()`.**  This
  is the T18 blocker flagged at the T20 commit, and the gate made it sharper rather than
  softer: `hasVisualFeed()` answers "does the AUDIO thread publish columns", which is
  true for only four of the ten visuals — the rest (LFO scopes, transfer curves,
  harmonic bars, the delay grid, the reverb envelope) are parametric draws that read DSP
  state at paint time and push nothing.  Under the old greying that predicate showed six
  visuals greyed-with-a-reason while their windows drew fine; under a presence gate it
  would have DELETED the row silently, which is strictly harder to notice.
  `hasVisual()` defaults to `hasVisualFeed()`, so LimiterDSP's single existing override
  still answers both and nothing regressed; each T18 parametric visual overrides
  `hasVisual()` alone.  `hasVisualFeed()`'s own comment was rewritten — it described the
  greyed-entry behaviour that no longer exists.
- **TransientShaper Advanced regrouped 7/3 -> 5/5** (Jeff: "do what makes sense").  The
  old split was by which VECTOR a knob lived in, not by function, which put FastRel and
  SlowAtt — the release constant of the peak follower and the attack constant of the RMS
  follower, i.e. the two envelopes Attack and Release act on — on the far row from the
  controls they tune.  Row 1 is detection + shaping (Attack / Release / FastRel /
  SlowAtt / Sens, plus both Shape chicken-heads and Mono/Stereo Det, which is a
  detection control); row 2 is band split + output (Split / Balance / Drive / Gain /
  Wet, plus OS).  Built as explicit `VKnob*` lists — the vectors are NOT re-sorted,
  because the Basic branch indexes into them and re-sorting would break those indices
  for no gain.
- **The residual sprawl is a WINDOW-SIZE issue, not a grouping one — recorded, not
  fixed.**  Worst slot goes 227px -> 152px, but TransientShaper Advanced has ten knobs
  total and the generic Advanced window is 1047 wide, so 44px knobs in ~150px slots is
  what that arithmetic gives.  Closing it properly means a per-panel Advanced width,
  which is a T7/T8 sizing decision and outside T20.
- **Ledger gap RULED: no backfill** (Jeff: "it is what it is") — see the entry above.

## 2026-08-05 — RECOVERED: the effect-window <-> visual-window TETHER was workshopped, RULED, and never built

Found by transcript search on 2026-08-06 when Jeff asked whether it had shipped.  It had
not, and it was in no doc — the rulings existed only in the T17 session's chat, and that
is the session whose running-notes entry was never written.  The ledger gap ruled "no
backfill" an hour earlier was assessed as five missing COMMIT entries; it also contained
an unbuilt spec, which nobody checked for.  **Anti-recurrence:** a ledger gap gets checked
for unbuilt rulings before it is written off, not just for missing narrative.

**Jeff's request, verbatim (2026-08-05):** "I also want the visual window to open
automatically along with the effect window.  Also is there any way to link the two windows
together with the visual window spawning under the effects window centered and they move
together as one piece and then give the visual window a menu that has a lock unlock option
to lock the window to the parent effect or not.  Could we do that?  Workshop before doing
please."

**Workshopped, then RULED by Jeff in the same session:**

| Piece | Ruling |
|-------|--------|
| Visual opens automatically with the effect window | YES — needs a per-slot "user closed this one" flag, or auto-open and the persisted open/closed state cancel each other out |
| Spawn position | Under the effect window, centered |
| Dragging a locked pair | **Moves the pair** — grabbing EITHER half moves both (rejected: refuse-the-drag, auto-unlock) |
| Locked width | **Visual matches the effect window's width** |
| Lock / unlock | Menu item on the VISUAL window, persisted on the aux record the way the pedals window stores its Compact mode |
| Sequencing | **T19 first**, then the tether, then T13's panel |

**T19 has since shipped (`9bcb510c`), so the stated prerequisite is met — and it made the
tether CHEAPER than the workshop predicted.**  Workshop concern 3 was that containment
would clamp each half independently and tear a locked pair apart at the workspace edge.
That concern is void: T19 dropped the size fit from the drag path entirely and made the
bound the CURSOR, which is one point regardless of which half was grabbed.  No
combined-rect clamp is needed.

**Jeff's open sub-question — "does that follow basic and advanced?" — ANSWERED: yes,
in both directions.**  The visual window's floor is 420x220 (`openAuxWindow(..., 420,
220)`); Basic 691 and Advanced 1047 both clear it, so a width-matching visual is never
clamped.  The hook already exists — `onFloorChanged` fires on every Basic/Advanced swap
and already calls `setDefaultWindowSize` + `setSize` on the effect window (T19), so the
tether rides that same callback rather than needing new plumbing.

**What is missing, verified in source 2026-08-06:**

- `StandaloneEditor::openEffectSlotWindow` never calls `openEffectVisualWindow` — no
  auto-open.  The visual is reachable ONLY through Menu > Visual.
- `openEffectVisualWindow` opens on its own independent `vispos:` position key and never
  reads the parent window's bounds — no spawn-under-centered.
- `WorkspaceWindow` has no follower list and no shared fronting — nothing pairs two
  windows.  (`broughtToFront` / `onBroughtToFront` is the existing hook the z-order half
  would ride.)
- `EffectVisualWindow` has NO `configureTitleStrip` (`EffectWindows.h:208`), unlike
  `EffectSlotWindow` and `EffectEqWindow` — it has no Menu at all, so the lock/unlock item
  has nowhere to live until one is built.
- `onFloorChanged` drives its own window only.

**SLOTTED by Jeff 2026-08-06: its own task, executed NEXT — ahead of T18.**  Landed in the
batch plan as Task 21 with the full ruled spec.  Sequencing rationale: T19 is done so the
prerequisite is met, the nine T18 visuals then land into a window that already behaves the
way it is meant to, and the lock/unlock item needs a title strip built on
`EffectVisualWindow` from scratch — a class T18 would otherwise be touching nine more
times first.

## 2026-08-06 — Task 21 built — effect-window <-> visual-window tether

- **Build gate green on a RE-RUN.**  The first run reported five exit codes 0, but two
  edits to `StandaloneEditor.cpp` landed after it was launched, so that result did not
  cover the tree and was discarded rather than counted.  Re-run: five exit codes 0, four
  `vcxproj -> ...exe` link lines, zero `error C` / `error LNK` / `error MSB`,
  `StandaloneEditor.cpp` confirmed recompiled in the log, no new warnings in any touched
  file.
- **The tether primitive lives on `WorkspaceWindow`, asymmetric in ONE axis only.**
  DRAGGING is symmetric — `mouseDrag` translates the partner by the delta actually taken,
  in either direction, so grabbing either half moves both (Jeff's ruling).  PLACEMENT has
  a leader: `layoutTetherFollower` puts the follower under it, centred, at its width.
  Both links are `SafePointer`s because these windows are destroy-on-close and either half
  can go first.
- **The delta is measured from the position ACTUALLY TAKEN, not from `desired`** — so
  magnetism and the cursor bound are already inside it and the pair cannot drift apart by
  whatever either of those corrected.
- **T19 made the hard half free, confirmed.**  The workshop expected to need a
  combined-rect containment clamp, because clamping each half independently tears a pair
  apart at the workspace edge.  T19 dropped the size fit from the drag path and made the
  bound the CURSOR — one point regardless of which half was grabbed — so there is nothing
  left to separate them and no combined clamp was written.
- **Basic/Advanced carries across with no new plumbing.**  `resized()` calls
  `layoutTetherFollower`, and the variant swap already resizes the effect window (T19), so
  the follower picks the width up from there.  `moved()` calls it too, so a programmatic
  move (workspace clamp, restore) carries the follower like a drag does.
- **Fronting**: `broughtToFront` fronts the partner then re-fronts itself, so the half the
  user clicked ends on top.  One static re-entrancy guard covers both re-entries and also
  suppresses `onBroughtToFront` during the swap, or the partner's pass would sync the
  ribbon to the wrong window on its way through.  The MOVE half needs no guard — only the
  window under the mouse runs `mouseDrag`.
- **FOUND EN ROUTE: the visual window has been showing a DEAD "Menu" heading since T17.**
  `PageMenuBar` builds `mHamburgerBtn` unconditionally in its constructor, so every window
  has the heading; `showHamburgerMenu` with no builder and no items hits
  `if (mMenuItems.empty()) return;` and silently does nothing.  `EffectVisualWindow` was
  the only one of the three effect window classes with no `configureTitleStrip`, so its
  Menu had never done anything.  T21 gives it one, which fixes that incidentally.
- **Auto-open + the closed-by-hand flag.**  `openEffectSlotWindow` opens the slot's visual
  last (the visual tethers itself by looking the effect window up in the registry, so that
  window must be registered first), gated on `hasVisual()` and on the user not having
  dismissed it.  The two close paths are deliberately distinguished: `onCloseRequested`
  (the X) sets the flag, `onRequestClose` (slot died) does not — so a visual that vanished
  because its effect was cleared comes back with it, and one the user shut stays shut.
- **RE-TETHER path, caught in self-review before the gate.**  Closing the effect window
  destroys its half and nulls the SafePointer.  Reopening it while the visual survived hit
  `openEffectVisualWindow`'s existing-window early return and never re-tethered — two
  windows that look paired, report unlocked and move independently.  The early return now
  re-tethers.
- **Persistence.**  Lock rides the visual's `<Open>` record like the pedals window's view
  mode, written ONLY when unlocked — absent means locked, so an older project restores
  tethered.  The closed-by-hand set needs its own `<VisClosed>` elements because a closed
  window has no `<Open>` record to ride.  Both stores REPLACE on load rather than merge:
  they are project content, and carrying the previous project's forward would unlock or
  suppress visuals belonging to effects this project never had.
- **No Rule 4 diagnostics added.**

**TWO BUGS, both Jeff-found on the first run, both fixed before the commit:**

- **1. A locked pair drifted out of alignment.**  The leader->follower move was applied
  TWICE.  `setBounds` in `mouseDrag` fires `moved()`, which already calls
  `layoutTetherFollower` and re-seats the follower; the delta block then shoved it one
  more frame's delta past that.  Every frame re-seated correctly and then knocked it off
  by one delta, so the drag ended slightly out and stayed out.  Propagation is now
  follower->leader ONLY — the direction that has no other mechanism, since nothing else
  moves a leader.  The leader's `moved()` re-seats the follower afterwards, so the pair
  self-corrects every frame instead of accumulating.
  Second source of the same symptom: a follower resized by its own border kept the new
  width and sat off-centre until the leader happened to move.  `requestTetherReseat` on
  the follower's `resized()`/`moved()` fixes that, guarded by `mDraggingTitle` — re-seating
  during the follower's OWN drag would snap it back to the leader's old position, the
  measured delta would come out zero, and the follower would be undraggable.
- **2. A locked pair closed and opened separately** (Jeff: "defeats the whole purpose").
  Closing either half now closes both while locked; unlocked, each closes alone, which is
  what the unlock is for.  **One deliberate asymmetry:** closing a LOCKED visual does NOT
  set the closed-by-hand flag, because the effect window is going with it and has to bring
  it back on reopen.  Setting it there produces the same bug mirrored — the pair closes
  together and reopens as one window.
  **Lifetime hazard in both close paths:** `closeAuxWindow` destroys the window that owns
  the lambda currently running, so every captured value is dead the moment the first close
  returns.  Both handlers copy what they need to locals before the first call and touch
  only the copies afterwards.  The pre-existing single-close handlers got away with it by
  never using a capture after the call.
- **A shadowing warning of mine was fixed rather than shipped**: the tether's `delta` hid
  `mouseDrag`'s raw pointer-travel `delta` (C4456).  Renamed `applied`, since it is
  specifically the movement taken AFTER magnetism and the cursor bound — carrying the raw
  one would drift the pair by whatever either corrected.
- **NOT VERIFIED IN THE APP** beyond Jeff's two reports — this batch is inside G4 and takes
  no batch smoke, so the rest rides the G4 boundary walk.  The G4 smoke needs steps for it:
  drag either half, resize either half, lock/unlock from the visual's Menu, Basic/Advanced
  with a locked pair, close either half locked and unlocked, close-and-reopen the effect
  window with the visual still up, and save/reload with a pair both locked and unlocked.
- **Next:** T18 — the remaining nine effect visuals, now landing into a window that
  behaves the way it is meant to.

## 2026-08-06 — Task 18 complete — all 10 effect visuals live

- **SCOPE CORRECTED BY JEFF at the top of this task.**  I had read `Limiter.txt` §1-2 as
  live scope and posed a spec call about skeuomorphic Zone B knobs, brushed-aluminium
  rendering and a three-zone panel rewrite.  Jeff: *"None of these I didn't write this up
  you did and are now trying to push on me something I never asked for.  I don't care if
  the knobs stay the same I just care that we have all the knobs for the setup and that the
  different visuals that are attached to them display on the visual window and show edits
  on the knobs in the display like they are supposed to."*  The Limiter panel rewrite is
  **OUT**; knobs stay as they are.  The requirement is: full knob set on the panel, a
  visual per effect in the Visual window, and the visual MOVES when a knob moves.
- **Limiter CONFIRMED already complete** against that requirement, verified in source
  rather than assumed: `LimiterDSP.cpp:736` pushes `(-lvl, lvl, gr, ceilNorm)` where the
  ceiling line comes from `mCeilingTargetDb + mCeilingTrimDb`, so the orange line tracks
  the Ceiling knob and GR/level track the rest.
- **The plumbing gap that made the rest possible:** the paint callback was handed the
  audio FEED and nothing else, but most of these visuals are PARAMETRIC -- they read knob
  state at paint time.  `EffectVisualWindow::resolveDsp()` added, resolving through the
  rack by uuid on every call and never cached, same discipline as the feed resolver and
  for the same reason (a project load destroys the DSP under an open window).  Parametric
  is also *why* a knob edit shows up immediately: nothing is published and nothing waits.
- **Display-only accessors added** following the `DelayDSP::shapeFeedbackForDisplay`
  precedent -- paint-thread read, never called from processBlock, unlocked by design
  (a float read racing the audio thread costs at worst one stale frame at 30 Hz):
  `ChorusDSP::lfoPhase/lfoShapeForDisplay`, `FlangerDSP::lfoPhase`, `PhaserDSP::lfoPhase`,
  `DelayDSP::hostBpmForDisplay`.  Everything else was already public state.
- **SHIPPED (all 10):**
  - **Limiter** — scrolling level + GR + ceiling (pre-existing, re-verified).
  - **Chorus / Flanger / Phaser** — ONE shape for all three, deliberately: they are the
    same idea (an LFO sweeping a delay or a filter) and three different pictures would
    teach a difference that is not there.  Left is the LFO's actual wave with a dot on the
    real phase; right is the comb/notch response.  Per-effect derivations differ (chorus
    notch spacing from base delay, flanger depth from feedback, phaser notches from stage
    count -- two allpass stages make one notch).
  - **Delay** — repeats as bars against the beat grid, ping-pong split above/below the
    centre line, BUILDING warning above unity feedback, plus the feedback drive curve that
    arrived from the panel at T20.  Beat grid reads `hostBpmForDisplay` off the DSP rather
    than the transport so grid and repeats can never disagree.
  - **Reverb** — decay envelope with pre-delay / early-reflections / tail as shaded named
    regions; the tail is a true -60 dB exponential over the Decay knob's time, so the curve
    IS the number on the knob.
  - **Compressor** — in/out transfer curve with the soft knee drawn and a live dot.  The
    dot solves the curve BACKWARDS from `getGainReductionDb()`: GR = (in-T)(1-1/R), so the
    operating point comes from the one number the DSP already publishes.
  - **Transient Shaper** — one hit ghosted twice, input vs shaped, driven by Attack /
    Release and the two Shape selectors.
  - **Saturation / Tape** — one branch (`EffectType::Tape` is an ALIAS onto
    `SaturationDSP`, H-10 cutover).  Harmonic bars + the drive transfer curve.  The bars
    are MEASURED, not modelled: one sine cycle runs through `shapeForDisplay` at paint
    time and harmonics 1-8 are correlated out of what comes back, drawn in dB below the
    fundamental (linear magnitude flattens everything past the 2nd) with the fundamental
    dim at the left as the note-you-play reference.  A Tube-type swap or Color toggle
    moves the bars because it changed the math, not a hand-tuned picture.
- **`SaturationDSP::shapeForDisplay` dispatches on the ACTIVE Type and calls the REAL
  static shapers** (`processTube` / `processConsole` / `tapeAsymShaper`) with live member
  values -- the shapers are stateless statics, so the picture cannot drift from the audio
  path because it IS the audio path.  A generic tanh approximation was rejected for
  exactly that reason.  Console maps Drive->mFlowers / Color->mDabs (the process()
  argument mapping); Tape mirrors the phase-2 call-site input math (bias offset, then
  k = 0.3 * vibe) with an edit-together note on both sides.  Deliberately omits
  everything AROUND the shaper (band split, oversampling, hysteresis, wow/flutter):
  static transfer curve, not a simulation.
- **Limiter caption upgraded** while closing the manual-rule sweep: the plan requires the
  GR trace's meaning stated AT the component, and T13 had left a bare effect name.  Now
  "cyan dips = volume being pulled down; orange = ceiling".

**RECOVERED RULING #2 from the same ledger gap (Jeff, 2026-08-06): the Feed warn ring was
supposed to be LIVE.**  Jeff's words in the T13/T19 session (recovered by transcript
search): the ring "was supposed to actually show you hitting red when it was causing the
extreme clipping and what not that comes with high feedback."  That session fixed only the
glow clipping (`9bcb510c`) and dropped this half -- the ring colored purely off the KNOB
position and showed nothing about the audio.  Same anti-recurrence note as the tether: a
ledger gap gets checked for unbuilt rulings, and this one had TWO in it.

- **DSP**: `DelayDSP` accumulates the peak of the loop-injection signal (step 5, post
  `mFeedbackLevel` scale -- the signal actually re-entering the line) and publishes it
  through `getFeedbackEnvForDisplay()` (relaxed atomic, instant rise, ~250 ms decay so a
  building runaway reads live without flickering at the repeat rate).  The Off model
  skips the accumulate on purpose: no loop, no feedback occurring, ring dark.  Near 1.0
  means step 4's limiter/saturator is clamping every cycle -- the exact "extreme
  clipping" state Jeff described.  Zeroed in `reset()`.
- **Ring redesign** (`TimeLAF::drawWarnRing`, new `kWarnRingLive` slider property): two
  layers.  SETTING track, thin -- green through the safe range, dim orange outline
  through the over-unity zone, so the runaway RANGE stays visible before anything sounds
  (the original CL-299 purpose survives).  LIVE arc, thick -- a meter of the level
  actually circulating; its lit head IS the current level, green heating to orange as
  the loop approaches unity, red only past it.  Both share the knob's 0..1.2 scale so a
  circulating level of 1.0 lights exactly to the knob's "100%" mark.
- **Position rework** (Jeff: "sitting on top of our knob... looks weird as hell"): a
  ring-carrying knob now draws its face INSET 6 px so the ring orbits in clear space
  around it -- the chicken-head letter-ring relationship -- instead of being painted
  over the filmstrip face.
- **Panel**: `DelayPanel` gains a 15 Hz peer-keyed timer (starts/stops on
  `parentHierarchyChanged`, `liveDsp()` guard first per the T13 crash class) that maps
  the level onto the arc scale and pokes `kWarnRingLive` + a repaint, change-guarded at
  0.004 so a silent panel repaints nothing.
- Build gate green after each batch; final gate five exit codes 0, four link lines, zero
  `error C` / `error LNK` / `error MSB`.
- **No Rule 4 diagnostics added.**
- **NOT VERIFIED IN THE APP** — no batch smoke (G4).  The G4 walk needs, per effect: open
  Menu > Visual, confirm the picture draws, then turn its knobs and confirm the picture
  moves.  That last half is the actual requirement and cannot be inferred from a build.

## 2026-08-06 — T18 REWORKED audio-first after Jeff rejected the parametric-only pass

Jeff's verdict on the first T18 pass, direct: the visuals were "just weird lines that do
slightly move with the knobs but there is nothing displaying the sound at all ... I need
way more audio displayed in these as this is kind of piss poor work."  He was right, and
the tell was already in the record: the limiter -- the ONE visual showing real audio --
was the one he had no complaint about.  The parametric pass showed the SETTINGS; the
requirement is the SOUND, with the settings shown against it.

- **Every effect now publishes real audio to its visual feed** (self-gated -- one relaxed
  load per block unwatched, unchanged from T17's design).  Shared helpers on `DSPBase`
  (`visualCaptureIn` / `visualPushInOut`: hi/lo = output envelope, a = input envelope,
  linear so the strip reads as a waveform) so six effects cannot drift six ways.
  Per-effect exceptions: Compressor pushes input + GR + threshold; Delay accumulates the
  WET path separately in-loop, because a post-scan of the mix would bury the echoes under
  the dry signal while it plays and the echoes are the entire point.  Reverb's
  five-algorithm dispatch was restructured (done-flag instead of per-case returns) so
  every path exits through the push.  The `hasVisual()` overrides became
  `hasVisualFeed()` -- every effect genuinely publishes now.
- **Every window is audio-first**: main area = scrolling in-vs-out (ghost outline = what
  went in, solid = what came out; the difference IS the effect), parametric drawing
  demoted to a side strip.  Per effect: Compressor = input waveform + cyan GR from the
  top + orange threshold lines ON the waveform (which is where Attack/Release finally
  SHOW -- how fast the cyan digs in, how long it hangs on; they never bend the static
  knee, which is a level map, and Jeff's question about that is answered by putting the
  audio on screen rather than by the curve).  Delay = the actual echoes (wet-only trace)
  with beat-grid verticals derived from block duration + the DSP's own BPM, so grid and
  echoes cannot disagree.  Reverb = the solid keeps going after the ghost stops -- that
  hang-over IS the tail.  TransientShaper = real before/after; the canned one-hit stays
  as a small side preview because it shows the settings while nothing plays.
  Saturation/Tape = in-vs-out plus harmonic bars measured AT THE LIVE INPUT LEVEL (from
  the feed's recent input columns): a soft clipper adds nothing quiet and plenty driven,
  so the bars rise and fall with the sound -- Jeff's "where those harmonics are hitting
  as it passes through" -- with dim ghost bars behind them showing the potential at full
  drive.  Chorus/Flanger/Phaser = the audio with the LFO scope + comb stacked at the side.
- **`delayRepeats` deleted** (the parametric repeats grid the audio view replaces) --
  own-batch dead code, cleaned in-batch.
- **Warn-ring position: knob restored to full size** (the 6 px inset was mine, not asked
  for, reverted on Jeff's correction) and the ring's placement made adjustable:
  `TimeLAF::sWarnRingOffX/OffY/Scale` statics applied in `drawWarnRing`, driven by a
  draggable placement box (below).  Once Jeff settles a placement the numbers get
  hardcoded and the box deleted.
- Build gate green: five exit codes 0, four link lines, zero errors.
- **NOT VERIFIED IN THE APP** — Jeff re-tests against the audio-first list; the knob
  half was already verified once, the audio half cannot be inferred from a build.

**Ring placement SETTLED (Jeff, 2026-08-06).**  The move-only box was rejected same day —
the knob art is in perspective, so fitting the ring needed stretch and rotation, not just
position.  The box gained corner handles (stretch X/Y independently), a top-stem rotation
knob, and wheel uniform scale; Jeff fitted it by eye and delivered
`scaleX=1.417 scaleY=0.889 rotDeg=-1.5` (offsets 0/0).  Hardcoded as
`TimeLAF::kWarnRingScaleX/ScaleY/RotDeg` constexpr calibration (Rule 6 category 5 — value
derivation recorded at the declaration); `drawWarnRing` draws the tilted ellipse via
`addCentredArc(rx, ry, rotation)`.  Box, statics, file write and the on-disk placement
file all deleted per the catalog disposition.

## 2026-08-06 — T13 closed by scope-down; T14 close bookkeeping

- **T13 CLOSED inside the T18 commits, no separate commit.**  Jeff's 2026-08-06 ruling
  scoped the panel-side three-zone rewrite OUT ("I don't care if the knobs stay the
  same...").  Against the delivered requirement — full knob set, Zone A in the Visual
  window, visuals tracking knobs — T13 was already complete at `d6e57dfa`.  The batch
  plan's T13 bullets are struck through in place with the supersession recorded.
- **Z-order overlay sweep: CLEAN.**  Every `setAlwaysOnTop` hit is a real desktop window
  (EventEditor, PatternColorPicker, KeyBindsWindow, HeavyOperationOverlay); CableOverlay
  and LockoutOverlay paint inside their own window's content, which is correct.  T16 had
  already promoted the two genuine offenders (editor tooltip, per-window listeners); no
  drawn-into-editor overlay expecting to cover the workspace remains.
- **CLAUDE.md stale facts fixed:** ArrangementGrid `kNumRows` 32 -> 500 (verified against
  `BuilderPage.h:606`); the Limiter UI note rewritten — it still directed a future
  session to build the three-zone panel Jeff scoped out, now records the supersession
  and points Zone A at the Visual window.  `mPPBar` 80 and undo depth 100 re-verified
  still true and left alone.
- **Rule 4 strip list: EMPTY.**  All three catalog entries (T6 sizing diag, T7 WINPOS
  diag, the ring placement box) were already removed in their own tasks; grep for
  `QA-Layout DIAG` / `WINPOS DIAG` over Source/ returns zero hits.  Nothing to strip,
  nothing to approve.
- **Test Plans §B.32 authored at close** per the plan: section retitled, LAY-B1..B21
  layout scenarios appended after the LAY-A audio-device slice, reconciled against what
  actually shipped (incl. T15-T21 mid-batch additions, the audio-first visuals, the
  tether, the live warn ring, and the T13 supersession).  Walked at the G4 boundary per
  L32, not at close.
- **Close sequence dispatched:** doc-drafter (batch-close mode) + batch-code-reviewer
  running in parallel; reviewer's deep pass focused on this session's four commits
  (`a6d6ed60`, `b42ba705`, `49520684`, `d6e57dfa`), earlier commits having been reviewed
  in their own sessions.

## 2026-08-06 — `/review-batch` outcome + review fixes + caribou reconciliation

- **Review verdict: READY-TO-COMMIT.  0 BLOCKER / 1 NEEDS-FIX / 5 NIT** (deep pass on this
  session's four commits `a6d6ed60` / `b42ba705` / `49520684` / `d6e57dfa`; earlier commits
  reviewed in their own sessions).  Reviewer independently cleared the tether re-entrancy
  guards, the paired-close lambda lifetimes, the feed gating on every DSP push, and the
  diag strip.
- **NEEDS-FIX premise VERIFIED in source, then fixed** (`StandaloneEditor.cpp` save
  walker): the tether-lock writer read WINDOW state, but closing an effect window nulls
  its partner — an unlocked visual left open with its effect window closed at save time
  never got `lock=0` written and reload re-LOCKED the pair against the user's choice.
  Now gated on `mVisualUnlockedKeys` — the same in-session store both tether paths read.
  This is exactly LAY-B19's save/reload path.
- **Two NITs fixed in the same pass:** `onTetherLockChanged` deleted (own-batch
  speculative hook, fired but never assigned — own dead code cleaned in-batch); en-GB
  spellings in the new T21 comments corrected to US ("centred"/"re-centres";
  `juce::Justification::centred` API refs exempt).
- **Three NITs RECORDED, not fixed (dispositions):** TransientShaper Advanced rows
  hard-index the knob vectors (not a live crash — ctor fully populates before layout;
  cleanup-pass material); `mVisualUserClosed`/`mVisualUnlockedKeys` never prune uuids of
  deleted effects (cosmetic XML growth, replaced-on-load so it cannot cross projects);
  the warn-ring env accumulation runs ungated (documented deliberate at the site — the
  panel's ring timer is not a feed watcher, and the cost is two fabs+max per sample).
- **Caribou composition note reconciled (Jeff approved in chat):** the G4 run plan's
  order string had never learned of QA-Layout — the 2026-08-03 insertion updated Main
  Plan §5/§6/§9 but missed `swift-stampeding-caribou.md` line 156.  Now carries the
  2026-08-06 update note and the corrected order (… badger → mammoth → **layout** → yak
  → stoat → heron).
- Build gate green after the fixes: five exit codes 0, four link lines, zero errors.

## 2026-08-06 — HELD Work Log entry (applies at the G4 boundary per L32)

Drafted by the doc-drafter at close, reviewed and TBDs filled by the main session.
Applies to `Implemented Work Log.md` — with the §5 STATUS flip to CLOSED — when the G4
boundary walk passes §B.31 + §B.32.  Held here per the mammoth/badger precedent.

---

### 2026-08-06 (time at apply) PT — QA-Layout — Whole-app layout under the contained-window shell: 14 planned tasks grew to 21 via Jeff-directed mid-batch additions (T15-T21) — window chrome/menus/titles consolidated, Window-7 satellite windows + pedals-as-LiveInst-player, three-lifetime window persistence, Jeff's measured sizing pass -> real per-player floors + BaySickSolstice/VibePlayer/pedal-tile reworks + the four-cause window-placement bug, four secondary group buses + mixer Add/target menus, L18 instance caps + the two-sixteens kit, hosted-plugin stretch, and the effect-visual system (T17 feed foundation -> all ten visuals REWORKED audio-first after Jeff rejected the parametric-only pass -> effect/visual window tether + live Feed warn ring, both recovered from the ledger gap); T8's general page-collapse dropped to Future State CL-306; T13's three-zone panel rewrite scoped out by Jeff; NO batch smoke — all functional verification rides the G4 boundary walk (L32)

**Bucket:** UI / L&F / Theming, Effects, Players, Mixer / Routing, System Pages, Cross-cutting Infrastructure, Other / Platform / Deferred

**HELD TO THE G4 BOUNDARY (L32, mammoth/badger precedent).** Drafted at batch close 2026-08-06; lands in this log when the G4 boundary walk runs. No batch smoke was taken — per-task build gates (five exit codes 0, four link lines, zero error greps) were the only in-batch gates. Everything functional walks at the boundary against Test Plans §B.31 (B.31.0 rewritten by T6) + §B.32 (LAY-A audio-device slice added at T8; LAY-B1..B21 layout scenarios authored at close, reconciled against what actually shipped incl. T15-T21, the audio-first visuals, the tether, the live warn ring, and the T13 supersession). The bridged-specific `1cd1f5d6` items remain UNTESTED (no 32-bit VST3 on hand) — the smoke must not assume them.

**RECORDED LEDGER GAP (Jeff's ruling 2026-08-05: no backfill — "it is what it is").** Five commits — `3cfdf4c2` (T16), `8c610c6c` (T8), `94da6a6f` (T12), `d07d710f` (T17), `9bcb510c` (T13+T19) — carry no running-notes entries; their unusually detailed commit messages stand as the record, and this entry's bullets for those tasks are compiled from the messages alone. The gap turned out to contain TWO workshopped-and-ruled-but-never-built specs (the T21 tether; the live Feed warn ring), both recovered by transcript search on 2026-08-06. Anti-recurrence note on the record: a ledger gap gets checked for unbuilt rulings before it is written off, not just for missing narrative.

#### Done

- **Planning (2026-08-03).** Jeff's authored spec `Files For Claude/Final V1 Layout.md` reconciled against the held mammoth scope — Jeff's doc wins throughout; 32 locked calls (L1-L32) + deferred D1-D7 in the plan. Supersessions on record: locked call 5a REVERSED (full-screen toggle ships); engine pickers DELETED, not moved; §B.31.0's drag-and-report table superseded by the diag flow; naming = BaySickLiveInst (+ menu) / LiveInst (tab/strip/titles).
- **T1 (`80b2f1f2`).** Perf readout rebuilt as a custom three-row component (SYS/DSP | MEM/LAT | UND/PF) with L20 per-token coloring; the 120-vs-160 gutter mismatch fixed; L25 two-row ribbon tab labels (camelCase wrap machinery retired); L26 app title + icon re-centered (reverts TS7); L27 Piano Roll ordered next to Effects (order[] only); latent `kMaxSlots` overflow fixed (11 -> 12 — TS6's Plugins type had outgrown the stack arrays).
- **T2 (`148e192a`).** "+" menu rebuilt to the locked L2/L3/L28 order incl. the new BaySickDrums > BaySickPlayer/BaySickSynth submenu; L4 engine pickers deleted (Layers/Bass combos, Clips decorative combo, Drums "Pick a sound"); page engine context menus merged into each window's Menu dropdown (one Delete per page, nothing dropped silently); L2 LiveInst rename sweep; L31 hamburger -> "Menu". Mid-task ruling (Rule 5, in chat): every tab type's ribbon dropdown gained engine-named add rows firing `onAddEngineRequest`, so no route can create an engineless page.
- **T3 (`5065a616`).** L5 full-screen fill toggle on every WorkspaceWindow (reverses 5a); engine-internal BaySickTitleBars dissolved, colored player names centered on the window strip via `PageMenuBar::setCenterTitle`; preset buttons relocated to title strips (Rusty's Player Preset + Program back to the strip — reverses QA-G3Smoke G-16); L23 dead LiveInput clip-label mount removed. Mid-approval L31 correction rode the commit: "Menu" re-shipped as `TitleStripMenuItem` — a flat native-menu-bar-style text heading, not a chrome button.
- **T4 (`85128436`).** Window-7: five sub-pages (Pitch / Align / Vocal Chain / Pedals / NAM-IR) became satellite windows hosting page-owned panels NON-OWNED via per-tick resolvers; L10 the pedals window IS the LiveInst player (tab click fronts it; no Inst page window for live-input tabs); L11 + D4=c ribbon "Pages:" replaced by a per-instance window-row model with Pre/Post EQ rows on every type; the two `findParentComponentOfClass` escapes converted to injected callbacks pre-rehost; L22 vocal-chain fix — `sat_type` range 0..1 -> 0..2 (the clamp that forced Tape back to Console) + DSP default Console.
- **T5 (`fba55012`).** Three-lifetime window persistence: universal in-memory session map as the ONE live store; settings.xml written once at exit from a filtered view (sizes for all, placement for the four default tabs); project `<Windows>` map + saved-open framing, load-time force-framing killed; L16 crash survival rides the 15-min autosave serialize; the persistKeyFor collision closed via `pageIndexOfEntry` (the live defect was the Clip/Vox/Inst fill gap, not the comment the plan named).
- **T6 (`7ac6844e`) + THE HANDOFF.** `[QA-Layout DIAG]` sizing instrumentation (live WxH strip readout; per-resize append with effects Basic/Advanced mode; floors dropped to 120x80), §B.31.0 rewritten in place (L1), full coverage checklist delivered to Jeff. His sizing pass returned 13,287 raw lines -> 37 settled groups; the sizing map approved in one pass (engine-attributed page sizes; effect generics Basic 691x268 / Advanced 1047x268 / pedal-native 358x268).
- **T9 (`43313911`).** L29 piano-roll control-lane header-drag resize: one shared height static app-wide, DrumKit lane lockstep mirror, `<ControlLane>` project persistence, mode dropdown moved to mouseUp so a clean click still opens it.
- **T15 (`7a0758b0`, ruled mid-sizing 2026-08-03).** Every player page's strip nav buttons dissolved into that window's own Menu dropdown (`onBuildWindowNavMenu` across all seven page classes; Jeff's exclusions honored); the missed T3 treatment for the sfizz trio — Rusty/Guitars/Basses internal title bands dissolved, names centered on the strip, the Inst band's widgets re-homed with the CUT SELF attachments decoupled from any title bar (would otherwise have silently killed G-14's feature).
- **T10 (`3639cb98`).** Mixer rework: FOUR secondary group buses (kLayersBus2 / kBassBus2 / kClipsBus2 / kPluginsBus2, ids 14-17) on the full kVoxBus2 pattern, cross-checked against the mixer-strip pattern audit, automation live + offline covered by the generic paths (verified — no per-bus offline branch exists to add); L13 "Add" titled menu (seven rows) replacing the five strip buttons; L12 per-strip "+" Send/Sidechain/Move-Output target submenus with the drag-placement paths retired (cable painting + right-click menus stay); L14 used-once-then-hide lifecycle + `Buses` persistence + all seven secondary buses reset on project load (Vox2/Inst2/3 previously leaked across projects); L30 MIDI trigger velocity relocated to Audio Settings.
- **T11 (`4722c27c`).** L18 caps — Layers 20 / Bass 10 / Drums 32 / Clips 100 / Vox 10 / Inst 30 — with `kMax*Strips` mirrors + a repo-wide stale-literal sweep; D3 ruled 1(c)+2(a) at task open: ONE piano-roll "Drum Kit" entry with an in-view 1-16 / 17-32 switch and FIXED page-index mapping; the accepted one-time PR-target invalidation landed.
- **T7 (`9797f19d`).** Real floors from the approved map, keyed per PLAYER TYPE (Jeff's correction: "there is no split between a layers harmless and a bass harmless") and HARD (Jeff option B, grow-only); the "Mixer doesn't save where I move it" placement bug root-caused to FOUR independent causes (project overwriting the global store; destructor-time teardown save clobber; restore against an unlaid-out workspace — the big one; the startup clamp capturing windows) — all four fixed; BaySickSolstice REDONE, not re-hung (layoutRow wraps + centers; every grid cell carries content; design 1039x421); L15 VibePlayer knobs 55 -> 18; all 26 pedal-capable panels gained `PanelMode::Pedal` tile grids via a shared `pedalTileGrid`; Effects + Piano Roll restored to the four-window launch set (implementation error owned — the four-default rule was always Jeff's); a kit load no longer frames sixteen drum windows (option B recorded as Future State CL-305); all T6 diag stripped.
- **T16 (`3cfdf4c2`, Jeff-directed mid-batch 2026-08-04).** Two QA-ModelShell regressions fixed: right-click Automate restored inside contained windows (per-window listeners — the app-wide GlobalAutoRightClick cannot cross a native child peer) and tooltips promoted to a single parentless desktop window; sizing model reworked to DEFAULTS not floors (`defaultSizeFor` optional, installed at engine-bind + a 5 Hz healing sweep; content minimums suspended pending T8); settings.xml cut to the four default tabs, size AND position, stale records stripped; `applyResizeMagnetism` added (the magnet had never run on a resize); FX Rack + Freeze into the per-window Menu with Freeze greyed + unlock path in its tooltip; Builder's Edit/Tools/Clips/View row dissolved, browser collapse as a magnetic ramp, vertical zoom decoupled from window size; ribbon "+" sized to its glyph; perf readout trimmed with live values in its tooltip; BaySickSolstice re-laid out incl. snap + grid on the app's unified divisions WITH triplets (the one place a triplet could not be snapped to).
- **T8 (`8c610c6c`, D1/D2 ruled 2026-08-05).** The general page-collapse is NOT built for V1 — dropped to Future State CL-306 (bespoke per-engine second design; the T16 BaySickSolstice re-layout is the cost evidence); content minimums RESTORED at the measured sizes; the pedalboard one-pedal-at-a-time SHIPS as an explicit Standard/Compact View on the pedals window strip (Compact = slot dropdown + one display box at the Effects-window footprint), with the view-swap machinery built for reuse per Jeff (Future State CL-307). Plus the audio-device slice: JUCE's silent device substitution disabled (a refused device no longer opens a different one or overwrites the user's choice), driver error text captured + surfaced, ASIO control panel reachable from the fallback state, ASIO4ALL removed from the fallback chain, Apply writes a coherent ASIO input name, JUCE ASIO trace to gitignored `asio_trace.txt`, splash painted before init blocks, Piano Roll engine pill rebuilt outside the visible-page gate; §B.32 LAY-A added with K-3 marked superseded.
- **T12 (`94da6a6f`).** L17 hosted-plugin stretch: resizable plugins pushed through their own resize/constraint path; fixed-size plugins transform-scaled with aspect preserved + centered letterboxing; bridged surfaces centered at natural size and clipped (a native child peer cannot be transform-scaled); natural size tracked separately from frame size so a plugin self-resize is distinguishable from our own layout; window resize floor from a minimum usable scale.
- **T17 (`d07d710f`, Jeff-directed mid-batch 2026-08-05).** Effect-visual foundation: `EffectVisualFeed` on DSPBase behind a visibility gate (an unwatched effect costs one relaxed atomic load per block) with refcounted watchers; one shared 30 Hz clock that stops entirely when no visual is on screen; `EffectVisualStrip` whose existence IS the watcher registration; per-slot visual windows keyed by slot uuid (follow the effect through a reorder) riding the aux-window registry for open-state + bounds persistence; Menu > Visual entry; plus the freeze-menu entry fixed to read the unlock preference live instead of a page-show snapshot.
- **T13 + T19 (`9bcb510c`).** Limiter publishes to the feed and its window draws it — Zone A built AS T17's reusable component per Jeff, not standalone; Menu > Visual actually emitted (`appendStandardItems` was never called from the effect window — registered but never rendered); Basic/Advanced swap resizes in BOTH directions; windows land on screen at open; drag bound = the CURSOR, not the window (supersedes locked call 2b's containment); the dangling-model-pointer crash class on project LOAD fixed (a panel timer and its window's rebuild poll are independent, so a load replacing a slot's DSP left the panel calling into freed memory) via `liveDsp()` liveness checks across ten panel timers + the Delay curve child + the dynamic-EQ popout + the visual strip's feed resolver, with slot windows also tracking the DSP instance; restored windows no longer self-close on their first poll; warn-ring glow no longer clipped.
- **T20 (`a6d6ed60`) + follow-up (`b42ba705`).** Delay panel rebuilt: Basic now filters BOTH rows to Jeff's locked list (row 2 was NEVER filtered — twelve knobs at ~23px a slot, labels truncating to "M..." / "Df..."; the "3 fb knobs" ambiguity resolved from source as FBDst/FBKnee/FBSym); sync division moved up beside the other chicken-heads above the BPM switch; feedback curve out of the panel for the Delay's Visual window (`FbCurveDisplay` deleted, 93 lines); right-hand controls re-laid as one content-sized 3x2 grid; grouping review walked all 14 Basic-capable panels — the Delay was the ONLY defect. Follow-up: Menu > Visual became a PRESENCE GATE (Jeff's reversal of T17's show-it-disabled call: "so it's there or it's not there") on a new `DSPBase::hasVisual()` kept deliberately distinct from `hasVisualFeed()` (the feed question is about audio-thread publishing and would have silently deleted the row on parametric visuals); TransientShaper Advanced regrouped 7/3 -> 5/5 by function; T21 docketed from the recovered tether ruling.
- **T21 (`49520684`).** Effect-window <-> visual-window tether (workshopped + RULED 2026-08-05, RECOVERED from the T17 session transcript 2026-08-06, slotted by Jeff as its own task ahead of T18): visual auto-opens under its effect window, centered, at matching width; grabbing EITHER half moves both (the delta measured from the position actually taken, so magnetism/cursor-bound corrections cannot drift the pair); fronting either half fronts both; locked pairs close and reopen together (closing a LOCKED visual deliberately does NOT set the closed-by-hand flag); lock/unlock on the visual's Menu — which required giving `EffectVisualWindow` the title strip it never had; lock + closed-by-hand state persist with the project, replace-on-load. T19's cursor-bound drag made the workshop's combined-rect containment clamp unnecessary. Two Jeff-found first-run bugs fixed before the commit (double-applied leader->follower move accumulating drift; locked pairs closing separately — "defeats the whole purpose").
- **T18 (`d6e57dfa`) — REWORKED AUDIO-FIRST.** The first pass shipped parametric-only visuals and Jeff rejected it ("just weird lines that do slightly move with the knobs but there is nothing displaying the sound at all") — the tell was the limiter, the one visual showing real audio, drawing no complaint. Rework: EVERY effect publishes real audio to its self-gated visual feed via shared `DSPBase` capture/push helpers (Compressor adds GR + threshold; Delay accumulates the wet path in-loop so echoes stay visible under the dry); every window leads with the sound — ghost in vs solid out, parametric draws demoted to side strips; Saturation/Tape harmonic bars are MEASURED at the live input level through `SaturationDSP::shapeForDisplay`, which calls the real shaper statics so the picture IS the audio path; manual-facing explanations written at the components per the plan's rule. Plus RECOVERED RULING #2 from the same ledger gap: the Delay Feed warn ring made LIVE — DelayDSP publishes the loop-injection peak (red only on real runaway; the setting track keeps CL-299's purpose), drawn as a tilted ellipse fitted to the perspective knob face by Jeff via a same-day placement box, his numbers hardcoded as `TimeLAF::kWarnRingScaleX/ScaleY/RotDeg` constexpr calibration and the box removed.
- **T13 CLOSED BY SCOPE-DOWN (Jeff, 2026-08-06) — no separate commit.** The panel-side three-zone rewrite (Zones B/C, skeuomorphic LAF, glass overlay) is OUT: "I don't care if the knobs stay the same I just care that we have all the knobs for the setup and that the different visuals that are attached to them display on the visual window and show edits on the knobs in the display." Against that delivered requirement — full knob set on the panel, Zone A in the Visual window (`9bcb510c`, colors per spec), visuals tracking knobs (ceiling line rides `mCeilingTargetDb + mCeilingTrimDb`, verified in source) — T13 was complete at `d6e57dfa`. Plan bullets struck through with the supersession recorded.
- **T14 (close bookkeeping + the close commit).** Z-order overlay sweep CLEAN (every `setAlwaysOnTop` hit is a real desktop window; T16 had already promoted the two genuine offenders); CLAUDE.md stale facts fixed (ArrangementGrid kNumRows 32 -> 500; the Limiter UI note rewritten — it still directed a future session to build the panel Jeff scoped out); Rule 4 strip list EMPTY (grep zero); §B.32 LAY-B1..B21 authored; the G4 run plan's composition note reconciled (QA-Layout was missing from `swift-stampeding-caribou.md` — the 2026-08-03 insertion updated Main Plan §5/§6/§9 but not the run plan; Jeff approved the fix); `/review-batch` 0 BLOCKER / 1 NEEDS-FIX / 5 NIT with the NEEDS-FIX (tether-unlock persistence read window state instead of the store) premise-verified and fixed + two NITs fixed (dead `onTetherLockChanged` hook deleted; en-GB spellings) + three NITs recorded in the close notes entry.

#### Found along the way

- **THE LEDGER GAP + two unbuilt recovered rulings.** Five commits (T16/T8/T12/T17/T13+T19) shipped with no running-notes entries. Assessed first as five missing narrative entries; transcript search on 2026-08-06 (triggered by Jeff asking whether the tether had shipped) found it also contained TWO workshopped-and-ruled specs that existed in no doc and no code: the effect/visual window tether and the live Feed warn ring.
- **T1** — latent `kMaxSlots` stack-array overflow (11 vs the 12 slots TS6's Plugins type made possible).
- **T2** — L4's picker deletion would have turned the ribbon's generic "+ Add New" rows into dead ends creating engineless pages (spec call surfaced pre-landing).
- **T4** — `VoxPage::showEngineContextMenu` was caller-less dead code; plus H-6b's dead `mEnginePicker` scaffolding and BaySickVocalEditor's dead HostPanel/PlaceholderPanel classes.
- **T5** — the plan's persistKeyFor diagnosis was half-wrong: the comment was already fixed; the LIVE defect was the `pageIndexHint` fill gap collapsing every Clip/Vox/Inst/Plugins window onto `"type:-1"`.
- **T11 literal sweep** — two LATENT OVERFLOWS (`mUsedLayerIndices` and `Pattern::layerRoll` both `std::array<...,8>` against cap 20) and two STALE RANGE BUGS live at the old caps (ownerCategory's 6-wide Vox/Inst checks — audio-group detection already broken for Inst strips 7-20 — and BuilderPage's matching stale bounds).
- **T7** — the window-placement bug was FOUR independent causes; PROCESS finding: the first floor map was built from a MID-PASS snapshot of the diag file and treated as final — read the hand-back artifact when the hand-back is actually made. The plan's subtractive-size-math sweep had NOTHING to sweep once floors were restored (disposition flagged to Jeff).
- **T16** — both QA-ModelShell regressions were Jeff-found, not surfaced by the shell work that caused them; the recorded fact "magnetism composes correctly with resize" was WRONG and withdrawn (the magnet never ran on resize at all — nothing to compose).
- **T13+T19** — the dangling-model-pointer class crashed a project LOAD; Menu > Visual was registered but never emitted.
- **T20** — Delay Basic's row 2 was never filtered (Basic was the same panel squeezed, confirmed by arithmetic); `hasVisualFeed()` was the wrong gate for the Visual entry (parametric visuals publish nothing); one grouping candidate (TransientShaper 7/3) surfaced to Jeff and regrouped on his "do what makes sense"; the residual TransientShaper sprawl is a WINDOW-SIZE issue (per-panel Advanced width) — recorded, not fixed.
- **T21** — the visual window's "Menu" heading had rendered DEAD since T17 (`PageMenuBar` builds the heading unconditionally; `EffectVisualWindow` had no `configureTitleStrip`); self-review caught the missing RE-TETHER path on effect-window reopen before the gate; review caught the tether-unlock persistence gap (window state vs store) at close.
- **T18** — the parametric-only first pass showed the SETTINGS; the requirement is the SOUND with the settings shown against it (Jeff's rejection, owned).
- **T12** — `canRebuildType`'s stated reason ("entangled with strip spawning") is confirmed for Vox/Inst/Clips but NOT for BaySickRustyDrums, whose strips arrive from a kit-LOAD event like the rebuildable pages; recorded as a guess in the plan's routing notes — a potential memory dividend on one of the heaviest pages, worth a look, not worth assuming.

#### What was done about each finding

- **Everything fixable folded in-batch** per the QA default — the T1 overflow, T4 dead code + menu merge, T5 fill gap, T7 four placement causes, T11 overflows + range bugs, T16 regressions, T13+T19 crash class + emission fix, T20 row-2 filtering + `hasVisual()` split, the T21 dead-Menu / re-tether / drift / paired-close fixes, and the close review's tether-unlock persistence fix all shipped inside their own tasks or the close commit. Own-batch dead code (`FbCurveDisplay`, `delayRepeats`, the T18-obsoleted `hasVisual` overrides, the unwired `onTetherLockChanged` hook) cleaned in-batch.
- **Jeff-ruled routings out:** T8's general page-collapse -> Future State **CL-306** (with the open "two players on screen at once" question carried); the view-swap machinery notated as **CL-307**; the shared drum-player-window option as **CL-305** (T7). All three Future State entries shipped in their tasks' own commits.
- **The two recovered rulings** became built work: the tether as Task 21 (`49520684`), the live warn ring inside T18 (`d6e57dfa`).
- **T18's rejection** became the audio-first rework in the same task — no deferral.
- **Recorded, NOT fixed (dispositions on record):** the TransientShaper per-panel Advanced width; the Rusty `canRebuildType` question; the three review NITs (TransientShaper hard-index, uuid pruning, ungated ring env accumulation).
- **§9 Forks:** the close is chronicled as the sixty-eighth Forks entry (mid-batch growth to 21 tasks, supersessions, ledger gap + recovered rulings, audio-first rework, T13 scope-down, caribou reconciliation); no closed-batch surface needed a back-ref beyond those already recorded in their own tasks.

#### `/review-batch` outcome

- **0 BLOCKER / 1 NEEDS-FIX / 5 NIT — READY-TO-COMMIT.** NEEDS-FIX (tether-unlock persistence read window state; unlocked pair with the effect window closed at save time re-locked on reload) premise-verified in source and fixed in the close commit. Two NITs fixed (dead hook, en-GB spellings); three recorded with dispositions. Deep pass covered this session's four commits; earlier commits were reviewed in their own sessions.

#### Carry-forward contradictions

- None against Carry-Forward §1-§3. T10/T11 ran under the §1/§3 registration discipline (kVoxBus2 pattern + strip-pattern audit cross-check); T4/T5 honored the §2 lifecycle primitives. The batch's supersessions were of prior LOCKED CALLS and test-plan rows, all recorded at the point of reversal: 5a (T3), the 2b containment rule (T19), T17's greyed Visual entry (T20 follow-up), G-16 (T3), TS7's title treatment (T1), K-3 (T8), §B.31.0 row 12 (T6). T16 additionally WITHDREW the recorded "magnetism composes with resize" fact (correction in the plan's Verification section — the G4 walk must test snapping on resize as well as on move).

#### Diagnostic Instrumentation Catalog

- **Three entries this batch — ALL REMOVED; nothing to strip at close** (grep for `QA-Layout DIAG` / `WINPOS DIAG` over `Source/` = 0). (1) T6 sizing diag — removed in `9797f19d` (T7). (2) T7 WINPOS placement tracing — added and removed inside T7 itself. (3) The ring placement box — removed same day 2026-08-06: Jeff's settled numbers (`scaleX=1.417 scaleY=0.889 rotDeg=-1.5`, offsets 0/0) hardcoded as `TimeLAF::kWarnRingScaleX/ScaleY/RotDeg` constexpr calibration; box, statics, file write and the on-disk placement file all deleted.

#### Files touched

Per-commit scope lists live in the twenty-one commit messages (the ruled record for the five no-notes tasks). By area: **Shell / chrome** — WorkspaceWindow, StandaloneEditor, StandaloneApp, SharedUI (incl. TitleStripMenuItem, PageMenuBar, TimeLAF, VibeLAF), RibbonTabBar, GlobalTransportBar, BaySickTitleBar. **Pages** — LayersPage, BassPage, DrumPage, ClipsPage, VoxPage, InstPage, PluginsPage, BaySickRustyDrumsPage, MixerPage, MixerTrackStrip, BuilderPage, PianoRoll, PianoRollPage, DrumKitGrid, EffectsPage, SlotComponent. **Effects** — EffectWindows, EffectEditorPanels, EffectVisual (new, T17), EffectVisualFeed (new, T17), EffectRack, EffectPresetIO. **DSP** — DSPBase, DelayDSP, ChorusDSP, FlangerDSP, PhaserDSP, ReverbDSP, CompressorDSP, TransientShaperDSP, SaturationDSP, LimiterDSP. **Engine editors / Vox family** — BaySickSolsticeEditor, VibePlayerEditor, BaySickPedalsEditor, BaySickVocalEditor, BaySickVocalProcessor, BaySickPitchEditor, BaySickPitchSubEditor, BaySickNAMIREditor. **Model / graph / infra** — VibeGraph, VibesynthConstants, PatternManager, PluginProcessor, PagePresetIO, Hosting/HostedPlugin, CMakeLists.txt, .gitignore. **Docs** — STANDALONE_UI_CHANGES.md, CLAUDE.md, Test Plans/v1-master-test-plan.md (§B.31.0 rewrite; §B.32 LAY-A + LAY-B), Future State.md (CL-305/306/307 + K-3 supersession), swift-stampeding-caribou.md (composition note), the paired batch plan + running notes, this Work Log entry, Main Plan §5 + §9.

#### Commit(s)

`80b2f1f2` (T1) - `148e192a` (T2) - `5065a616` (T3) - `85128436` (T4) - `fba55012` (T5) - `7ac6844e` (T6 + handoff) - `43313911` (T9) - `7a0758b0` (T15) - `3639cb98` (T10) - `4722c27c` (T11) - `9797f19d` (T7) - `3cfdf4c2` (T16) - `8c610c6c` (T8) - `94da6a6f` (T12) - `d07d710f` (T17) - `9bcb510c` (T13 + T19) - `a6d6ed60` (T20) - `b42ba705` (T20 follow-up + T21 docketed) - `49520684` (T21) - `d6e57dfa` (T18 audio-first + live warn ring) - plus the close commit (the commit carrying this entry: review fixes + close bookkeeping docs; hash recorded at the boundary apply). NO batch smoke (G4) — per-task build gates only; Jeff's in-flight confirmations during the batch (the placement "It works now", the T21 first-run reports, the sizing-pass hand-back, the ring-fit delivery) are recorded in the running notes; the functional walk is the G4 boundary.

#### Next action

- **QA-UndoCoverage (`long-rewinding-yak`) is next in G4** (order: … layout -> yak -> stoat -> heron -> the G4 boundary R3 review + smoke). This batch's functional verification — including the T21 tether steps and the per-effect open-Visual / turn-knobs / confirm-the-picture-moves walk — rides the G4 boundary smoke (§B.31 + §B.32).

---

## Diagnostic Instrumentation Catalog (Rule 4)

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| `EffectEditorPanels.cpp` `RingPlacementBox` (+ mount/layout in DelayPanel), `SharedUI.h` `TimeLAF::sWarnRingOffX/OffY/ScaleX/ScaleY/RotDeg`, `SharedUI.cpp` `drawWarnRing` offset/stretch/rotation application | `[QA-Layout DIAG]` | Warn-ring placement (Jeff, 2026-08-06): dashed frame around the Feed knob.  Move-only was rejected same day ("can't rotate or stretch it at all which is required to make it actually fit on the knob" -- the knob art is in perspective, so the fit needs a squashed, tilted ellipse).  Final form: edge drag = move, corner handles = stretch X/Y independently, top-stem knob = rotate, wheel = uniform scale; interior passed through so the knob stayed usable.  Ring geometry via `addCentredArc(rx, ry, rotation)` | **REMOVED same day** -- Jeff's settled numbers (`scaleX=1.417 scaleY=0.889 rotDeg=-1.5`, offsets 0/0) hardcoded as `TimeLAF::kWarnRingScaleX/ScaleY/RotDeg` constexpr calibration; box, statics, file write and the on-disk `warn-ring-placement.txt` all deleted |
| `WorkspaceWindow.h` (onDiagExtraInfo + paintOverChildren decl + mLastDiagSize), `WorkspaceWindow.cpp` (ctor 120x80 floor, setMinimumSize override w/ commented-out real body, resized() diag append, paintOverChildren WxH readout, AppPaths include), `EffectWindows.h/.cpp` (diagPanelMode), `StandaloneEditor.cpp` (openEffectSlotWindow onDiagExtraInfo wire) | `[QA-Layout DIAG]` | Window-sizing collection (T6): per-size-change append of persist-key + title + WxH + effects panel mode to `Documents/BaySickDAW/window-sizing-diag.txt`; live strip readout; floors dropped to 120x80 | **REMOVED in `9797f19d` (T7)** — real floors restored in setMinimumSize; readout, per-resize append, onDiagExtraInfo and diagPanelMode all stripped |
| `WorkspaceWindow.h/.cpp` (winPosDiag + SAVE/REST call sites), `WorkspaceWindow.cpp` writeSessionToSettings (WRITE call site) | `[WINPOS DIAG]` | Window-placement bug (T7): one line per store / write / restore with raw bounds, screen bounds, workspace origin and workspace screen bounds, so the failing step was identified from data after three wrong diagnoses.  Wrote `Documents/BaySickDAW/window-pos-diag.txt` | **REMOVED in `9797f19d` (T7)** — added and removed inside the same task; it is what isolated causes 3 and 4 |
