# Manuals Rebuild Checklist

**THE working document for the Manual 1/2/3 rebuild.** Every ruling and every
unit of work is here; boxes get ticked as things actually complete, never
before. Source of structure: `Files For Claude/Manual Structure.xlsx` (Jeff,
2026-08-13) plus the corrections ruled in chat the same day.

Status line (updated as phases move): **PHASE G IN PROGRESS (2026-08-13): one-manual polish per Jeff - no landing page, frozen top bar, independent sidebar scroll, restored Sub-of hierarchy, full expand/collapse tree, STATIC sidebar (never changes with level), and the THIRD weeds-coverage review (synth/bass figures had zero code - shared topics get homes on their lead figures + per-figure audit). Then re-verify/stage.**

---

## Rules ledger - every ruling this rebuild works under

Nothing in this list is optional and none of it may be silently reinterpreted.

- [x] R1. Three groups, in order: **Shell, Instrument, Mixing & Effects**. Sidebar sectioned by them; within each, ordered by the sheet's Order column.
- [x] R2. Sidebar shows **the Name only** - no codes, no extras.
- [x] R3. **Codes are invisible everywhere** in all three manuals - no id chips, no `CODE .` headings, no id columns, no visible `IMP-n`. They live on as anchors, coordinate keys and search terms only. (They stay visible in the internal registry, which ships nowhere.)
- [x] R4. Every caption-table row gets two button columns: **In Depth** (to that control's Manual 2 material) and **In The Weeds** (to the Manual 3 mechanism). A button renders only where a real destination exists.
- [x] R5. **Callout sets are rebuilt from code, per figure**: what the user can interact with, see, and get information from - read from the editor/page source, not from screenshots. Trivia-class entries (the VOX-12 kind) are culled, not kept.
- [x] R6. **No "here is the Menu" dots.** Complex title bars get highlighted with their real contents (program pickers, preset buttons, A/B, Load...); trivial bars get nothing. Menus are documented in their own menu figures.
- [x] R7. Every window's title bar is covered **as that window's own** - on its main figure, with an optional dedicated bar crop where the bar is busy. No shared chrome figure (CHR is REMOVED, all its cross-references dissolve).
- [x] R8. Mains have **no parent except their group**. A Main named "...Titlebar" displays the player's name (BSRDTTL -> "BaySickRustyDrums").
- [x] R9. Sub-of drives **parent/child navigation links** on figure pages; multi-parent rows (BSGBM, BSNAM) link from every parent.
- [x] R10. Numbering contiguous per figure; **one renumber at the end** of the structural pass; coordinates carried through the saved map; stale nudge exports translated via the map, never pasted raw.
- [x] R11. Nudges on unchanged Current figures are **preserved** through the rename map. Fresh dots: the new figures (first pass mine), crops (remapped), PED (image was re-shot).
- [x] R12. Crops derive from existing masters, never re-shot; Jeff reshapes crop boxes in the authoring mode (boxes and dots independently adjustable). New surfaces (opened menus) are new captures - all 32 on disk (31 + the mid-pass `Mixer Strip Crop.png`).
- [x] R13. Manuals 2 and 3 are **prose a human reads** - sections about the thing, in sentences; not an entry-grid index. Manual 2 = what each usable control does and how it behaves when you move it. Manual 3 = how it really works (code + why), one shared topic per mechanism, sameness verified from code, per-engine differences inside the shared entry.
- [x] R14. No development history in any manual; no real brand names user-facing; US spelling; no em-dashes.
- [x] R15. Sheet corrections: Hosted Plugin Menu = **BSPLUGM**; **MIXADD** (was MIXVEW mistype); blank New Code = keep old code; blank Group = Shell; blacked-out Sub-of cells on Main rows are formatting, not data; "BaySickVocals Title" as parent = the BaySickVocals main; BSSB = stitched two-logo composite with a VERY BRIEF two-versions-one-family note; BSPDL keeps the full-board figure and gains a title-bar crop. Six sheet names read "BaySIckSynth/Bass ..." (capital I); normalized to "BaySick" for display.

---

## Phase A - structural pass (one shot)

- [x] A1. Copy the 32 `Added Images` into `Manuals/figures/`; verify one image per New row, none orphaned. (Done 2026-08-13; `BaySickSynth Menu.png` = `BaySickPlayer Menu.png` byte-identical is INTENTIONAL - code-verified the Layers menu literal is "Player" for both engines, StandaloneEditor.cpp:6869. Jeff added `Mixer Strip Crop.png` mid-pass - a Mixer shot carrying Vox/LiveInst strips, which have the input-source row engine strips lack.)
- [x] A2. Crops are DATA, not derived PNGs: each figure = one or more views {master file, percent rect}, rendered through a CSS window; dots live in master coordinates so reshaping a crop never moves them. Rects live beside marker coords in `marker-coords.py`. First-pass rects: BSPDL full+bar, BSST bar, BSBT bar, BSSB = synth bar + bass bar stacked, BSRDTTL bar+section tabs, MIXSTP = Bass 1 + Vox 1 strips from `Mixer Strip Crop.png`, six BSSB panels = panel region of their own masters (OSC keeps the waveform strip). Jeff reshapes in authoring mode.
- [x] A3. Rename map applied (32 renames) across registry, manual-2, manual-3, coordinates; map at `Manuals/assets/code-rename-map-full-2026-08-13.json`. CAUGHT AND FIXED: the backtick pass corrupted six on-screen literals that collide with old codes (BaySickSolstice `PITCH` section/knobs and `HARM` knob, BaySickPitch's `ALIGN`/`PITCH` boxes) - restored in registry + interim m2. Jeff's future exports translate through the map, never raw.
- [x] A4. Registry screen table rebuilt to the 90-figure tree (Group/Ord/Code/Name/Kind/Parents/Files/View); CHR row gone; restructure note added to the retirement log.
- [x] A5. Every `see CHR-n` dissolved (BLD blurb, PLUGT-2, allocation prose); CHR-1..7 retired.
- [x] A6. Generator rebuilt: grouped name-only sidebar with tree indent, name-only headings, Part of / Related links, In Depth + In The Weeds buttons gated on real anchors, crop views with master-space dots, crop-reshape authoring mode, native-scale crop rendering. Single authoritative copy: `Manuals/assets/generate-manual-1.py` (scratchpad copies retired).
- [x] A7. search.js: figures indexed by display name, result titles show names not codes, codes still work as search terms and Enter-jumps.
- [x] A8. index.html rewritten around names and the two buttons; numbering section replaced with the three-chapter organization.
- [x] A9. Regenerated + verified: 90 figures, 548 rows, 526 dots visible, zero visible ids, zero unresolved links in all three directions; staged both configs + zip refreshed. Four old synth-panel dots sit in cropped-out chrome (Phase B rebuilds those sets).
- [x] A10. Done via the strike renumbering in the audit patch; every figure asserted contiguous 1..n on 2026-08-13.

## Phase B - per-figure content rebuild (code-derived callouts)

For EACH figure below: read the source; enumerate interactables, visible
indicators, and information displays; write the callout set; place first-pass
dots (or remap surviving ones); cull trivia; no Menu dots (R6); bars covered
per R7. Tick only when the figure's callouts, dots and caption rows are done.

| Done | Figure | Code | Group | Ord | Kind | Parent | Img | Source to review |
|---|---|---|---|---|---|---|---|---|
| [x] | Main Window | FRAME | Shell | 1 | Main | (group) | Current | StandaloneEditor menus/frame |
| [x] | File Menu | FMENU | Shell | 2 | Sub | Main Window | New | StandaloneEditor buildMenu case 0 (File) |
| [x] | Export Audio | EXP | Shell | 3 | Sub | File Menu | Current | Export dialog code |
| [x] | Export Project Bundle | BUNDLE | Shell | 4 | Sub | File Menu | Current | Bundle dialog code |
| [x] | Edit Menu | EMENU | Shell | 5 | Sub | Main Window | New | StandaloneEditor (Edit) |
| [x] | Undo History | UNDO | Shell | 6 | Sub | Edit Menu | Current | UndoHistoryWindow |
| [x] | Event Editor | EVT | Shell | 7 | Sub | Edit Menu | Current | EventEditorWindow |
| [x] | Pattern Menu | PMENU | Shell | 8 | Sub | Main Window | New | StandaloneEditor (Patterns) |
| [x] | View Menu | VMENU | Shell | 9 | Sub | Main Window | New | StandaloneEditor (View) |
| [x] | Options Menu | OMENU | Shell | 10 | Sub | Main Window | New | StandaloneEditor (Options) |
| [x] | File Settings | FILE | Shell | 11 | Sub | Options Menu | Current | FileSettings dialog |
| [x] | Audio Settings | AUD | Shell | 12 | Sub | Options Menu | Current | Audio settings dialog |
| [x] | Plugin Scan | PLUG | Shell | 13 | Sub | Options Menu | Current | Plugin scan dialog |
| [x] | Help Menu | HMENU | Shell | 14 | Sub | Main Window | New | StandaloneEditor (Help) |
| [x] | Key Binds | KEYS | Shell | 15 | Sub | Help Menu | Current | KeyBindsWindow |
| [x] | Transport Bar | TRAN | Shell | 16 | Sub | Main Window | Current | Transport bar component |
| [x] | Recording Menu | TRANRM | Shell | 17 | Sub | Transport Bar | New | Record mode menu |
| [x] | Metronome Menu | TRANMM | Shell | 18 | Sub | Transport Bar | New | Metronome menu |
| [x] | Ribbon Tab Bar | TABBAR | Shell | 19 | Sub | Main Window | Current | RibbonTabBar |
| [x] | Ribbon + Button | TABUTN | Shell | 20 | Sub | Main Window | Current | RibbonTabBar::showAddMenu |
| [x] | Builder Page | BLD | Shell | 21 | Main | (group) | Current | BuilderPage + ArrangementGrid |
| [x] | Builder Menu | BLDM | Shell | 22 | Sub | Builder Page | New | Builder Menu builder |
| [x] | Builder Edit | BLDE | Shell | 23 | Sub | Builder Page | New | Builder Edit menu |
| [x] | Builder View | BLDV | Shell | 24 | Sub | Builder Page | New | Builder View menu |
| [x] | Piano Roll | PR | Shell | 25 | Main | (group) | Current | PianoRollPage |
| [x] | Piano Roll Edit | PRE | Shell | 26 | Sub | Piano Roll | New | PR Edit menu |
| [x] | Piano Roll Tools | PRT | Shell | 27 | Sub | Piano Roll | New | PR Tools menu |
| [x] | Piano Roll Scale | PRS | Shell | 28 | Sub | Piano Roll | New | PR Scale menu |
| [x] | Piano Roll Chords | PRC | Shell | 29 | Sub | Piano Roll | New | PR Chords menu |
| [x] | Piano Roll View | PRV | Shell | 30 | Sub | Piano Roll | New | PR View menu |
| [x] | Drum Kit | DKIT | Shell | 31 | Sub | Piano Roll | Current | Drum Kit grid |
| [x] | Drum Kit Edit | DKITE | Shell | 32 | Sub | Drum Kit | New | DK Edit menu |
| [x] | Drum Kit Tools | DKITT | Shell | 33 | Sub | Drum Kit | New | DK Tools menu |
| [x] | Drum Kit View | DKITV | Shell | 34 | Sub | Drum Kit | New | DK View menu |
| [x] | Parameter Menu | PRMMNU | Shell | 36 | Main | (group) | Current | VKnob right-click menu |
| [x] | BaySickVocals | BSV | Instrument | 1 | Main | (group) | Current | VoxPage |
| [x] | BaySickVocals Menu | BSVCM | Instrument | 2 | Sub | BaySickVocals | New | Vox page menu |
| [x] | BaySickVocalChain | BSVC | Instrument | 3 | Sub | BaySickVocals | Current | BaySickVocalEditor chain |
| [x] | BaySickAlign | BSA | Instrument | 4 | Sub | BaySickVocals | Current | BaySickAlignEditor |
| [x] | BaySickPitch | BSPIT | Instrument | 5 | Sub | BaySickVocals | Current | BaySickPitchEditor |
| [x] | BaySickPedals | BSPDL | Instrument | 6 | Main | (group) | Crop | BaySickPedalsEditor |
| [x] | BaySickPedals Menu | BSPDLM | Instrument | 7 | Sub | BaySickPedals | New | Pedals menu |
| [x] | BaySickPedals View | BSPDLV | Instrument | 8 | Sub | BaySickPedals | New | Pedals View menu |
| [x] | BaySickPedals Picker List | BSPDLP | Instrument | 9 | Sub | BaySickPedals | Current | Pedals picker (showChangePedalMenu) |
| [x] | BaySickGuitars | BSGTR | Instrument | 10 | Sub | BaySickPedals | Current | BaySickGuitars editor |
| [x] | BaySickBasses | BSBAS | Instrument | 11 | Sub | BaySickPedals | Current | BaySickBasses editor |
| [x] | BaySickGuitars/Basses Menu | BSGBM | Instrument | 12 | Sub | BSGTR, BSBAS | New | Inst nav menu (installInstNavMenu) |
| [x] | BaySickNAM/IR | BSNAM | Instrument | 13 | Sub | BSPDL, BSGTR, BSBAS & BSV | Current | BaySickNAMIREditor |
| [x] | BaySickSolstice | BSSOL | Instrument | 14 | Main | (group) | Current | BaySickSolsticeEditor |
| [x] | BaySickSynth/Bass Family | BSSB | Instrument | 15 | Main | (group) | Crop | -- composite, no controls |
| [x] | BaySickSynth Titlebar | BSST | Instrument | 16 | Sub | BaySickSynth/Bass Family | Crop | BaySickSynth title strip |
| [x] | BaySickSynth Menu | BSSM | Instrument | 17 | Sub | BaySickSynth/Bass Family | New | Synth page menu |
| [x] | BaySickBass Titlebar | BSBT | Instrument | 18 | Sub | BaySickSynth/Bass Family | New | BaySickBass title strip |
| [x] | BaySickBass Menu | BSBM | Instrument | 19 | Sub | BaySickSynth/Bass Family | New | Bass page menu |
| [x] | BaySIckSynth/Bass Filter | BSSBFLT | Instrument | 20 | Sub | BaySickSynth/Bass Family | Crop | BaySickSynth FLT panel |
| [x] | BaySIckSynth/Bass Filter Envelope | BSSBFENV | Instrument | 21 | Sub | BaySickSynth/Bass Family | Crop | BaySickSynth FLT ENV panel |
| [x] | BaySIckSynth/Bass LFO | BSSBLFO | Instrument | 22 | Sub | BaySickSynth/Bass Family | Crop | BaySickSynth LFO panel |
| [x] | BaySIckSynth/Bass Mod | BSSBMOD | Instrument | 23 | Sub | BaySickSynth/Bass Family | Crop | BaySickSynth MOD panel |
| [x] | BaySIckSynth/Bass Oscillator | BSSBOSC | Instrument | 24 | Sub | BaySickSynth/Bass Family | Crop | BaySickSynth OSC panel |
| [x] | BaySIckSynth/Bass Oscillator Envelope | BSSBOENV | Instrument | 25 | Sub | BaySickSynth/Bass Family | Crop | BaySickSynth OSC ENV panel |
| [x] | BaySickPlayer | BSP | Instrument | 26 | Main | (group) | Current | BaySickPlayerEditor |
| [x] | BaySickPlayer Menu | BSPM | Instrument | 27 | Sub | BaySickPlayer | New | Player page menu |
| [x] | BaySickRustyDrums | BSRDTTL | Instrument | 28 | Main | (group) | Crop | Rusty title strip (StandaloneEditor rusty branch) |
| [x] | BaySickRustyDrums Menu | BSRDMENU | Instrument | 29 | Sub | BaySickRustyDrums Titlebar | New | Rusty page menu builder |
| [x] | BaySickRustyDrums Main | BSRDMAIN | Instrument | 30 | Sub | BaySickRustyDrums Titlebar | Current | AriaControlPanel Main |
| [x] | BaySickRustyDrums Kick | BSRDKICK | Instrument | 31 | Sub | BaySickRustyDrums Titlebar | Current | Aria Kick |
| [x] | BaySickRustyDrums Snare | BSRDSNARE | Instrument | 32 | Sub | BaySickRustyDrums Titlebar | Current | Aria Snare |
| [x] | BaySickRustyDrums Toms | BSRDTOM | Instrument | 33 | Sub | BaySickRustyDrums Titlebar | Current | Aria Toms |
| [x] | BaySickRustyDrums Hi-Hat | BSRDHAT | Instrument | 34 | Sub | BaySickRustyDrums Titlebar | Current | Aria Hi-hat |
| [x] | BaySickRustyDrums Cymbals | BSRDCYMB | Instrument | 35 | Sub | BaySickRustyDrums Titlebar | Current | Aria Cymbals |
| [x] | BaySickRustyDrums Noises and Clicks | BSRDNOISE | Instrument | 36 | Sub | BaySickRustyDrums Titlebar | Current | Aria Noises |
| [x] | BaySickRustyDrums Drum Kit | BSRDKIT | Instrument | 37 | Sub | BaySickRustyDrums | New (2026-08-13 ruling) | BaySickRustyDrumsKitGraphic.cpp |
| [x] | BaySickRustyDrums Note Map | BSRDMAP | Instrument | 37 | Sub | BaySickRustyDrums Titlebar | Current | RustyDrumsMapWindow |
| [x] | Hosted Plugin | BSPLUG | Instrument | 38 | Main | (group) | Current | PluginsPage + HostedPluginEditor |
| [x] | Hosted Plugin Menu | BSPLUGM | Instrument | 39 | Sub | Hosted Plugin | New | Plugins page menu |
| [x] | Mixer | MIX | Mixing & Effects | 1 | Main | (group) | Current | MixerPage |
| [x] | Mixer Menu | MIXMNU | Mixing & Effects | 2 | Sub | Mixer | New | Mixer menu |
| [x] | Mixer Add | MIXADD | Mixing & Effects | 3 | Sub | Mixer | New | Mixer Add menu |
| [x] | Mixer Strip | MIXSTP | Mixing & Effects | 4 | Sub | Mixer | Crop | MixerPage strip layout |
| [x] | Mixer Send Menu | MIXSM | Mixing & Effects | 5 | Sub | Mixer | Current | Send jack menu |
| [x] | Master Analyzer | ANLZ | Mixing & Effects | 6 | Sub | Mixer | Current | Master Analyzer window |
| [x] | Master Analyzer Menu | ANLZM | Mixing & Effects | 7 | Sub | Master Analyzer | Current | Analyzer menu |
| [x] | Effects Rack | FXI | Mixing & Effects | 8 | Main | (group) | Current | EffectsPage |
| [x] | Effects Rack Menu | FXRM | Mixing & Effects | 9 | Sub | Effects Rack | Current | EffectsPage::buildTitleMenu |
| [x] | Effect Picker List | FXPICK | Mixing & Effects | 10 | Sub | Effects Rack | Current | showEffectPicker |
| [x] | Effect Panel | FX | Mixing & Effects | 11 | Main | (group) | Current | EffectEditorPanels + EffectWindows |
| [x] | Effect Panel w/ Visual | FXV | Mixing & Effects | 12 | Sub | Effect Panel | Current | EffectVisualWindow |
| [x] | Effect Panel Menu | FXM | Mixing & Effects | 13 | Sub | Effect Panel | Current | Effect window menu builder |
| [x] | EQ Window | EQ | Mixing & Effects | 14 | Sub | Effects Rack | Current | EQ window (SharedUI EQ8) |
| [x] | EQ Band Menu | EQB | Mixing & Effects | 15 | Sub | EQ Window | Current | EQ band menu |
| [x] | VU Meter | VUMTR | Mixing & Effects | 16 | Sub | Effects Rack | Current | MasterVuView + VUMeter |

## Phase C - Jeff's pass

- [ ] C1. Jeff nudges dots (one pass, after Phase B settles).
- [ ] C2. Jeff reshapes crop boxes in authoring mode.
- [ ] C3. Jeff reviews sidebar order/grouping and figure names as rendered.
- [ ] C4. Apply his export (translate through the map if taken pre-renumber).

## Phase D - Manual 2 rewrite (human prose)

Architecture (locked 2026-08-13): Manual 2 is HAND-AUTHORED prose, one source
file per figure under `Manuals/src-m2/<group>/<code>.md`, assembled by a new
`generate-manual-2.py` that adds the shared shell (masthead, sidebar from the
registry tree, search) and validates anchors. Each callout id gets an anchor
(`<a id="CODE-n">`) placed where that control's teaching lives, so Manual 1's
In Depth buttons light up per-row as chapters land - no big-bang cutover.
Crop images embed as the same master+rect views, with their own dots pointing
at the text below (D3). Switchover is PER FIGURE: the assembler walks the
registry tree; a figure WITH a `src-m2` chapter renders that prose; a figure
WITHOUT one gets its entries extracted verbatim from the interim
manual-2.html by callout-id anchor (`.ctrl[id]` divs, styled as a legacy
block) so nothing Jeff can reach today disappears mid-rewrite. Every chapter
that lands replaces its legacy block and its In Depth buttons keep working
through the same anchors. When the last chapter lands, the interim file has
no remaining consumers and D5 closes.

- [x] D1. Structure follows the three groups; chapters read as prose (R13).
- [x] D2. Every usable control on every figure taught properly: what it does, how it works, what up/down actually sounds like. Bundles split - every knob findable.
- [x] D3. Crop images embedded with their own dots pointing at the text below.
- [x] D4. No code chips (R3); In Depth buttons land on the right sections.
- [x] D5. Instruments first (worst affected), then Mixing & Effects, then Shell.

## Phase E - Manual 3 rewrite

- [x] E1. Mechanism topics (one ADSR, one filter, one LFO, one unison...), sameness verified from code; per-engine differences inside the shared entry.
- [x] E2. Kill the IMP-57 blanket: every control's In The Weeds lands on its real mechanism.
- [x] E3. Prose register per R13; no visible IMP ids (R3).
- [x] E4. Architecture half re-checked against the new structure; keep what is true, reframe headings to names.

## Phase F - ship checks

- [x] F1. Full link integrity across all three manuals, both directions.
- [x] F2. Codes invisible sweep (regex over rendered HTML for id patterns outside anchors/search data).
- [x] F3. Dev-history sweep (used to / previously / shipped / dates / QA-*).
- [ ] F4. Stage both configs, zip, F1-in-app spot check by Jeff.
- [ ] F5. Running notes brought current; batch close remains ONE commit at the end (Jeff's standing ruling).

## Phase G - one-manual polish (Jeff's rulings, 2026-08-13, confirmed)

- [x] G1. Landing page REMOVED: F1 / ManualsWindow opens `manual.html` directly
      (one-line C++ change in ManualsWindow + build gate - needs the app
      closed); `index.html` becomes an instant redirect so the zip and old
      links still land.
- [x] G2. The top bar ("BaySickDAW Manual" + the three level buttons) is
      FROZEN - sticky, always visible, slimmed so it does not eat reading
      space.
- [x] G3. The reference bar is its own scroll surface: fixed full-height pane
      with its OWN scrollbar, completely disconnected from the main window's
      scroll (main content offset beside it). If Jeff wants a literal separate
      OS window instead, that is an app change - flagged, not assumed.
- [x] G4. The reference bar's hierarchy RESTORED per the Sub-of structure:
      Mains at the left edge, subs indented beneath them - the flat list was a
      regression from the unification.
- [x] G5. Expand/collapse everywhere: the sidebar becomes a collapsible tree
      (groups and Mains fold), and the manual body collapses at every level -
      group chapter, figure section, and inside a figure each piece
      individually (caption list, each close-up block with its teaching, each
      mechanism topic, each Under-the-hood block). The figure's main picture
      shows whenever its section is open. Defaults: everything expanded,
      state resets on reopen, and every jump (search, dots, buttons)
      auto-expands whatever it lands in.
- [x] G6. The reference bar is STATIC - it is Jeff's figure list and never
      changes with the depth level. The "Under the hood" nav entries that
      appeared at Weeds level were never asked for and are REMOVED; what
      changes with level is what shows under the groupings, not the list.
- [x] G7. THIRD weeds-coverage review (Jeff's catch: BaySickSynth/Bass - a
      from-scratch engine - showed not one line of code on its figures,
      because its mechanisms sat only in the group-end block). Fix: shared
      topics get real HOMES on their lead figures - Envelopes -> the
      Oscillator Envelope panel, the Filter -> the Filter panel, LFOs -> the
      LFO panel, Unison -> the Mod panel, Voice modes -> the Oscillator
      panel, the Synth voice -> the Oscillator panel, the sampled
      instruments -> BaySickGuitars - with links from the other figures that
      use them. Then a scripted per-figure audit of what EVERY figure's Weeds
      layer actually shows (inline code / links only / nothing), and every
      major surface showing no code gets fixed. AUDIT RESULT (2026-08-13,
      after re-homing): INLINE CODE on 30 figures + the 4 group-end blocks -
      including all six BaySickSynth/Bass panels (engine voice + voice modes
      on the Oscillator panel, filter on Filter, envelopes on Osc Env, LFO on
      LFO, unison on Mod) and the sfizz topic on Rusty's Main panel. LINKS
      ONLY on 60 figures - all menus, dialogs and secondary views whose
      mechanisms are one click away at their home (e.g. Guitars/Basses link
      to the sfizz topic on Rusty Main; the Filter Envelope panel links to
      the envelopes topic one panel over; the effect windows link to the
      effects block). NOTHING only on the BaySickSynth/Bass family page,
      which has no controls by design. DONE.
- [x] G8. Re-verify (ids, links, anchors, coverage), restage both configs,
      re-zip.
- [x] G10. Caption rows lose their In Depth / In The Weeds buttons (Jeff,
      2026-08-13): the global level switch replaced their job; rows are
      number + label. Per-control navigation survives via the close-up dots
      and the How-this-works links.
- [x] G11. Layout polish (Jeff, 2026-08-13, from his screenshot): the
      sidebar's scroll pane runs the FULL window height - the header no
      longer cuts it off - and the header spans only the content column,
      collapsed flush to the window top with its bottom at the title text's
      bottom (dead space above and below removed).
- [x] G12. Body collapsers REMOVED (Jeff, 2026-08-13, reversing the body
      half of G5 after seeing it): fold controls live ONLY on the reference
      list; the manual body uses plain headings. The In The Weeds
      full-function code expandables stay - they are the option-C code
      presentation, not section headers.
- [x] G13. Tells-nothing bar rows struck (Jeff's FRAME catch, 2026-08-13):
      audited the whole registry for the class. STRUCK six - FRAME's OS
      title bar and window icon, Undo History's window title, and the three
      name-plate rows on the BaySickSynth / BaySickBass / BaySickRustyDrums
      bar figures (figures renumbered across registry, coords, chapters and
      fragments; accent-color teaching folded into chapter intros). KEPT as
      informative: the project-name title text with its unsaved star, the
      hosted-plugin title (explains tab naming), the effect window title
      (channel - effect naming).
- [x] G13b. The DEBUG-suffix row was initially kept as informative; Jeff
      overruled (2026-08-14) - users only ever run the Release exe, so a
      diagnostic-build tag has no place in the shipping manual. Row and
      chapter sentence struck, FRAME renumbered again; swept the whole
      manual for any other debug-build mention - none.
- [x] G13c. Two more FRAME strikes (Jeff, 2026-08-14): the OS
      minimize/maximize/close row and the workspace-backdrop row, both
      with their chapter paragraphs removed outright. FRAME is down to 11
      rows; the fixed-fullscreen fact survives in the chapter intro.
- [x] G14. Jeff's caption-audit notes (2026-08-14, from his nudge pass;
      each read back and confirmed before running): struck EVT resize
      grip, KEYS rows 6-9, TABBAR locked-prefix + frozen-mark (markings
      close-up dissolved, (missing) kept), TABUTN chevron row, PLUG
      status-line row, PR + BLD scrollbars, BLD colour-band row, FILE
      footnote row + the keep-captured-takes chapter prose (rows keep
      bare labels), BSP clip-file row (What-is-loaded close-up
      dissolved); AUD Apply+Close merged to one row; BLD Sort row moved
      to table end; BaySickSolstice LFO-Mod/Strum close-up split; all RustyDrums
      close-up boxes removed (text kept); BSST+BSBT -> BSSBT and
      BSSM+BSBM -> BSSBM (one entry each, both screens stacked, dots
      duplicated per view by the existing renderer), family children
      reordered OSC/OENV/FLT/FENV/LFO/MOD; BSGBM re-parented flat under
      Pedals after Basses; BSSB name plates equalized; new Builder +
      Piano Roll screenshots (ruler content + playhead) swapped in.
      Plus a Behind-list heal in src-m3: 54 anchors to struck rows
      dropped (incl. stale ones from earlier strike rounds) and the
      tripled Behind lines deduped. Manual regenerated green (89
      figures, 736 dots, 115 close-ups) + staged. CODE (build pending,
      app open): Audio Settings Close text VC::Text like Apply;
      ClipsPage clip-file box + its header strip removed.
- [x] G15. SUPERSEDED by G21 - the FXV meter row no longer exists, so no
      GR-needle crop is needed.
- [x] G16. Jeff's second notes round (2026-08-14, mid-nudge): DKIT 7/11/12
      struck (gesture rows a still image can't show; Key Binds is their
      catalog) and every Key-Binds-tabbed surface (Builder, Piano Roll,
      Drum Kit, Event Editor, Vocal Editor) now closes its chapter with a
      pointer to its tab; BSPIT Safety-nets close-up folded into Key and
      snapping (one crop covers both, dots now show); BSPDL updated board
      shot swapped in, Board-controls close-up repointed at the open
      Standard/Compact dropdown (was a crop of the word View), and the
      new Compact-view picture placed beside it as its own box (generator
      gained custom-file cluster support - a close-up may name a
      different master; dots suppressed there). Third coords export
      merged (733 dots after the strikes). Regenerated green + staged.
- [x] G17. Jeff's ruling: the on-screen box is for numbered dots ONLY.
      Registry-wide sweep kicked all 50 dotless rows out of the boxes;
      their teaching stays in the chapters as the prose it already was
      (anchors stripped, figures renumbered, Behind links healed).
      BSPDL's Standard/Compact rows now render REAL numbered buttons in
      the box, wired to their dots on the dropdown close-up (click raises
      the level and pulses the dot). Incident logged: the sweep's first
      run also caught the Namespace-3 IMP index (89 rows deleted) and the
      first recovery attempt briefly duplicated the tree region; both
      fully recovered - rows restored verbatim from session transcripts,
      duplication reversed byte-exact, junk purged. Final state verified
      green: 88 figures, 731 dots, 89/89 topics, zero dotless box rows.
- [x] G18. Jeff's fourth notes round (2026-08-14): fourth coords export
      merged (731 validated); BSSBT and BSRDTTL fill/close rows struck -
      no figure carries title-chrome rows (registry swept, none remain);
      multi-view figures now lay out side by side with a title above each
      (flex units - the two family menus sit beside each other, wide
      views keep their own rows); BSP collapsed 20 knob rows -> 7 section
      rows with first-pass section dots at the close-up centers (knob
      teaching stays as prose under each close-up); BSRDMAIN collapsed
      17 -> 9 rows (Presets / program picker / tabs + six instrument
      groups; the repeating-detail rows live on the per-drum pages).
      706 dots, verified green, staged.
- [x] G19. CORRECTED + DONE (2026-08-15): the swing knob was in the
      screenshots all along - it is the red ring beside Menu, which I had
      misread as a logo. Swing row + dot added to BSP, BSSOL, BSRDMAIN,
      BSGTR, BSBAS, BSPLUG and BSSBT (positions color-detected from the
      images). CORRECTION (Jeff): the Drum Kit view has NO swing knob by
      design - its sixteen pickers link to player pages that each carry
      their own - so DKIT gets no row and no re-shoot; the screenshot
      was accurate all along. Same pass: the crop-geometry
      root cause found by rendering the staged page in a browser - the
      global border-box reset made crop offsets resolve against the box
      minus its 1px borders, shifting short bottom-anchored close-ups
      (Jeff's mixer routing strip) ~31px up; .shot is content-box now and
      the routing crop measures sub-pixel exact. Also: FX re-parented
      under FXI so the reference bar nests Effect Panel inside Effects
      Rack between the picker list and EQ Window; --fsw hardened against
      a zero screen.width.
- [x] G20. Fifth coords export merged (706 validated); MIX 17 (send
      jack), 19 (routing cable), 21 (scrollbar) struck as visual-only -
      teaching stays as prose; `+` / `Analyzer` renumbered to 17/18.
      Routing close-up investigated: the crop shows down to the image's
      last pixel - Mixer.png itself is cut mid-jack at the bottom edge,
      so a re-shoot including the window's true bottom is needed (same
      size/layout keeps all dots valid). Re-shoot list now: Mixer.png +
      the G19 swing-knob set.
- [x] G21. Sixth coords export merged (696 validated; the export predated
      the MIX strike so its mixer dots were translated in the merge, and
      the FX-family / EQ / VU nudges landed). Effects Panel with Visual
      flattened per Jeff: no box rows, no close-ups - the picture plus
      the which-nine-effects blurb on top and the Visual-window blurb
      below; see-refs into the removed rows re-aimed; the old G15 GR-crop
      plan superseded.
- [x] G22. Swing dedupe (Jeff's catch, 2026-08-15): Guitars/Basses
      already documented the knob (BSGTR-9 / BSBAS-11) - my G19 adds
      duplicated them there. Duplicates struck; every player's swing row
      AND chapter paragraph now carries the BSGTR-9 wording verbatim
      (BSP-8, BSSOL-27, BSRDMAIN-10, BSPLUG-2, BSSBT-2, BSBAS-11
      rewritten to match). Seventh coords export merged (703 validated,
      incl. Jeff's nudged swing dots); 701 after the two duplicate dots
      dropped.
- [x] G9. ANSWERED (Jeff, 2026-08-15): the PDF ships as a CHOICE - one
      PDF per depth level, the reader picks which version to take.
- [x] G23. PDFs + installer (2026-08-15): three PDFs printed from the
      manual itself via headless Edge (landscape, one page per figure,
      ?level= override + eager-image print mode added to the page) into
      Manuals/ - "BaySickDAW Manual - In View / In Depth / In The
      Weeds.pdf" (8.7 / 9.6 / 11.3 MB, 142 pages at In Depth). Tester
      installer now packages $INSTDIR\Manuals (HTML manual + PDFs; F1
      needs it there), uninstall bounded-removes it, make_installer.bat
      stale "no manual" header corrected; new package built:
      Installer\BaySickDAW-1.2.0-20260814-1427-Tester-Setup.exe (44.7 MB,
      up from 17.9 with the manual aboard). Jeff's nudge pass COMPLETE -
      no dots left to move. Possible future item (Jeff): an in-manual
      text-edit mode pending tester feedback - not started.
- [x] G24. PDF black-box defect (Jeff, 2026-08-15): every backticked UI
      string - and every Weeds inline code block - printed as a solid
      black pill with invisible text. manual.css's @media print block
      remapped every palette variable to light EXCEPT --code-bg, leaving
      near-black text (--text #111) on the near-black screen pill.
      Fix: --code-bg -> #f2f2f4 in the print palette + kbd's screen cyan
      darkened to #00707e for print contrast. Verified by applying the
      page's own print rules on-screen and reading computed styles on
      the exact failing elements (code pill, kbd, codeblock pre); all
      three PDFs reprinted, restaged both configs, zip + installer
      rebuilt.
- [x] G25. Installed app lost every filmstrip + the VU meter (Jeff,
      2026-08-15; ruled option A). Filmstrips::getDir() resolved the art
      by walking FOUR parents up from the exe to the repo root, then into
      the gitignored "Files For Claude/Filmstrips" - a layout only the
      dev build tree has, so every install fell back to the drawn knobs
      and generic meter. The nine PNGs (8.2 MB: four group-knob strips,
      Chicken Head, Volume Black/White, Switch Toggle, VU Meter) also
      lived ONLY on this machine, untracked. Fix: PNGs moved into repo
      Resources/Filmstrips/ (now tracked; CMake already stages Resources/
      next to the exe in both configs and the installer already packages
      it - zero CMake/.nsi change), getDir() re-pointed to
      "<exe>\Resources\Filmstrips", stale path comments corrected in
      SharedUI.h + STANDALONE_UI_CHANGES.md. Footnote: the code's
      "Fader Slider.png" load has no file anywhere - faders were on the
      drawn fallback even in the dev tree, unchanged by this fix.
- [x] G26. Replace-feature menu pass (Jeff's spec + 4 new shots,
      2026-08-16): BSSBM re-based on the shared
      Synth-Player-BaySickSolstice picture + updated Bass picture, BSPM on the
      shared picture, BSPLUGM on the new Plugins picture; NEW BSSOLM
      (BaySickSolstice finally has a menu entry, Sub of BSSOL) and NEW BSDM
      (BaySickDrums Menu, Main, right before the Rusty block; documents
      both access routes) - 88 -> 90 figures. Replace rows added at
      BSSBM-8/BSPM-8/BSSOLM-8 and BSPLUGM-4..6
      (Rename/Replace/Duplicate), shifts swept across registry +
      chapters; In Depth Replace prose on BSSBM (full contract) with
      BSPM/BSSOLM deferring to it, full walk on BSDM + BSPLUGM. Fresh
      estimated dot sets on all five figures, PRE-NUDGE - Jeff
      rearranges. Regenerated green: 90 figures, 731 markers (delta
      +30 = +1+1+3+15+10, exact), 112 close-ups, 89/89 topics; staged
      both configs + zip. PDFs deliberately NOT reprinted - they go
      stale until Jeff's nudge lands, then one reprint + installer
      rebuild carries everything.
- [x] G27. Nudges merged + BSDRUMS parent entry (Jeff, 2026-08-16):
      eighth coords export merged, all 731 dots validated. Jeff's
      follow-up spec: NEW `BSDRUMS` entry (BaySickDrums, Main,
      `Drum Kit.png`) above the menu explaining the TWO ways of picking
      drums - by location (kit pad vs the ribbon +) and by type (sample
      via BaySickPlayer vs synth patch via BaySickSynth) - rowless
      prose entry like FXV; `BSDM` re-parented as its Sub so the nav
      reads BaySickDrums > BaySickDrums Menu. 91 figures. Three PDFs
      reprinted from the nudged manual, restaged both configs, zip
      refreshed, installer rebuilt:
      Installer\BaySickDAW-1.2.0-20260816-1157-Tester-Setup.exe
      (54.2 MB - carries the audio fix, Replace, kits, WebView2 loader
      and the 91-figure manual together).
- [x] G28. Nudge bar gated Debug-only (Jeff's call, 2026-08-16): testers
      saw the authoring bar at the bottom of F1. The page now renders it
      ONLY when asked via ?nudge=1 (gate class on <html> - setLevel
      replaces body.className wholesale), and only the Debug build's
      ManualsWindow asks (JUCE_DEBUG appends the param). Release, the
      installed copy and a plain browser open never show it; Jeff
      authors normally via F1 in the Debug exe (browser authoring still
      possible by adding ?nudge=1 by hand). Regenerated, restaged,
      installer rebuilt:
      Installer\BaySickDAW-1.2.0-20260816-2152-Tester-Setup.exe. PDFs
      untouched - the bar was already print-hidden.

## Parked / standing items

- [x] P1. PED figure re-nudge - CLOSED by Jeff's nudge pass (2026-08-15: "nothing left to move").
- [ ] P2. Jeff confirms the BaySickGuitars/Basses crash fix in-app (Debug first).
- [ ] P3. Task 10 cross-reference pass and Task 11 (batch close) follow the rebuild. The
  PDF half of Task 10 is DONE - see G9/G23.
