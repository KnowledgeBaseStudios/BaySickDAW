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

## Diagnostic Instrumentation Catalog (Rule 4)

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| (T6 diag, when built) | `[QA-Layout DIAG]` | Window-sizing collection: per-resize append of persist-key + title + WxH + effects panel mode to `Documents/BaySickDAW/window-sizing-diag.txt` | Remove at batch close |
