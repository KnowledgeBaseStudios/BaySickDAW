# Callout Registry

The join table between the three manuals. Built at QA-Manuals Task 2, before any
manual body is written, so the three can be written in any order and nothing
ever renumbers.

**Numbering is contiguous per screen.** A figure's dots and caption list always
run 1..n with no gaps - a picture whose first dot is 2 reads as broken. When a
callout is removed, its screen renumbers and every cross-reference is updated
mechanically in the same pass, with each marker's coordinates carried through
the old-to-new map so nudged dots never move. (An earlier version of this file
declared ids permanent and gaps deliberate - a constraint Claude invented,
never asked for, corrected 2026-08-13 when Jeff caught a figure starting at 2.
The retirement notes below keep their original pre-renumber numbers.)

Three namespaces:

| Namespace | Form | Owned by | Counts |
|---|---|---|---|
| Callouts | `<SCREEN>-<n>` | Manual 1 | 90 figures |
| Manual 2 sections | the seven `INDEX.md` groups | Manual 2 | - |
| Implementation topics | `IMP-<n>` | Manual 3 | 65 |

---

## Namespace 1 - the figure tree

**90 figures in three groups - Shell, Instrument, Mixing & Effects** - per
Jeff's `Files For Claude/Manual Structure.xlsx` (2026-08-13). The sidebar and
reading order follow Group then Ord. Display name is what the manual shows;
codes appear NOWHERE user-facing (anchors, coordinate keys and search terms
only). Kind Main/Sub plus Parent(s) drive the figure pages' part-of / contains
navigation links. Files live in `Manuals/figures/` (the build stages that
folder beside the exe). A `crop` view renders a percent rect of its master
file, defined next to the marker coordinates in
`Manuals/assets/marker-coords.py` - dots are stored in master-image percent,
so reshaping a crop box never moves a dot.

| Group | Ord | Code | Display name | Kind | Parent(s) | File(s) | View |
|---|---|---|---|---|---|---|---|
| Shell | 1 | `FRAME` | Main Window | Main | - | `Main frame.png` | full |
| Shell | 2 | `FMENU` | File Menu | Sub | `FRAME` | `File Menu.png` | full |
| Shell | 3 | `EXP` | Export Audio | Sub | `FMENU` | `Export Audio.png` | full |
| Shell | 4 | `BUNDLE` | Export Project Bundle | Sub | `FMENU` | `Export Project Bundle.png` | full |
| Shell | 5 | `EMENU` | Edit Menu | Sub | `FRAME` | `Edit Menu.png` | full |
| Shell | 6 | `UNDO` | Undo History | Sub | `EMENU` | `UndoHistory.png` | full |
| Shell | 7 | `EVT` | Event Editor | Sub | `EMENU` | `Event Editor.png` | full |
| Shell | 8 | `PMENU` | Pattern Menu | Sub | `FRAME` | `Pattern Menu.png` | full |
| Shell | 9 | `VMENU` | View Menu | Sub | `FRAME` | `View Menu.png` | full |
| Shell | 10 | `OMENU` | Options Menu | Sub | `FRAME` | `Options Menu.png` | full |
| Shell | 11 | `FILE` | File Settings | Sub | `OMENU` | `File Settings.png` | full |
| Shell | 12 | `AUD` | Audio Settings | Sub | `OMENU` | `Audio & Midi Settings.png` | full |
| Shell | 13 | `PLUG` | Plugin Scan | Sub | `OMENU` | `Plugin Search.png` | full |
| Shell | 14 | `HMENU` | Help Menu | Sub | `FRAME` | `Help Menu.png` | full |
| Shell | 15 | `KEYS` | Key Binds | Sub | `HMENU` | `Keybinds.png` | full |
| Shell | 16 | `TRAN` | Transport Bar | Sub | `FRAME` | `Transport Bar.png` | full |
| Shell | 17 | `TRANRM` | Recording Menu | Sub | `TRAN` | `Recording Menu.png` | full |
| Shell | 18 | `TRANMM` | Metronome Menu | Sub | `TRAN` | `Metronome Menu.png` | full |
| Shell | 19 | `TABBAR` | Ribbon Tab Bar | Sub | `FRAME` | `Ribbon Tab Bar.png` | full |
| Shell | 20 | `TABUTN` | Ribbon + Button | Sub | `FRAME` | `Ribbon + Menu.png` | full |
| Shell | 21 | `BLD` | Builder Page | Main | - | `Builder.png` | full |
| Shell | 22 | `BLDM` | Builder Menu | Sub | `BLD` | `Builder Menu.png` | full |
| Shell | 23 | `BLDE` | Builder Edit | Sub | `BLD` | `Builder Edit.png` | full |
| Shell | 24 | `BLDV` | Builder View | Sub | `BLD` | `Builder View.png` | full |
| Shell | 25 | `PR` | Piano Roll | Main | - | `Piano Roll.png` | full |
| Shell | 26 | `PRE` | Piano Roll Edit | Sub | `PR` | `Piano Roll Edit.png` | full |
| Shell | 27 | `PRT` | Piano Roll Tools | Sub | `PR` | `Piano Roll Tools.png` | full |
| Shell | 28 | `PRS` | Piano Roll Scale | Sub | `PR` | `Piano Roll Scale.png` | full |
| Shell | 29 | `PRC` | Piano Roll Chords | Sub | `PR` | `Piano Roll Chords.png` | full |
| Shell | 30 | `PRV` | Piano Roll View | Sub | `PR` | `Piano Roll View.png` | full |
| Shell | 31 | `DKIT` | Drum Kit | Sub | `PR` | `Drum Kit.png` | full |
| Shell | 32 | `DKITE` | Drum Kit Edit | Sub | `DKIT` | `Drum Kit Edit.png` | full |
| Shell | 33 | `DKITT` | Drum Kit Tools | Sub | `DKIT` | `Drum Kit Tools.png` | full |
| Shell | 34 | `DKITV` | Drum Kit View | Sub | `DKIT` | `Drum Kit View.png` | full |
| Shell | 36 | `PRMMNU` | Parameter Menu | Main | - | `RightClick Knob or Slider.png` | full |
| Instrument | 1 | `BSV` | BaySickVocals | Main | - | `BaySickVocals.png` | full |
| Instrument | 2 | `BSVCM` | BaySickVocals Menu | Sub | `BSV` | `BaySickVocals Menu.png` | full |
| Instrument | 3 | `BSVC` | BaySickVocalChain | Sub | `BSV` | `Vocal Chain.png` | full |
| Instrument | 4 | `BSA` | BaySickAlign | Sub | `BSV` | `BaySickAlign.png` | full |
| Instrument | 5 | `BSPIT` | BaySickPitch | Sub | `BSV` | `BaySickPitch.png` | full |
| Instrument | 6 | `BSPDL` | BaySickPedals | Main | - | `BaySickPedals.png` | full + bar crop |
| Instrument | 7 | `BSPDLM` | BaySickPedals Menu | Sub | `BSPDL` | `BaySickPedals Menu.png` | full |
| Instrument | 8 | `BSPDLP` | BaySickPedals Picker List | Sub | `BSPDL` | `BaySickPedals List.png` | full |
| Instrument | 9 | `BSGTR` | BaySickGuitars | Sub | `BSPDL` | `BaySickGuitars.png` | full |
| Instrument | 10 | `BSBAS` | BaySickBasses | Sub | `BSPDL` | `BaySickBasses.png` | full |
| Instrument | 11 | `BSGBM` | BaySickGuitars/Basses Menu | Sub | `BSPDL` `BSGTR` `BSBAS` | `BaySickGuitars & Basses Menu.png` | full |
| Instrument | 12 | `BSNAM` | BaySickNAM/IR | Sub | `BSPDL` `BSGTR` `BSBAS` `BSV` | `BaySickNAMIR.png` | full |
| Instrument | 13 | `BSSOL` | BaySickSolstice | Main | - | `BaySickSolstice.png` | full |
| Instrument | 14 | `BSSOLM` | BaySickSolstice Menu | Sub | `BSSOL` | `BaySickSynth-BaySickPlayer-BaySickSolstice Menu.png` | full |
| Instrument | 15 | `BSSB` | BaySickSynth/Bass Family | Main | - | `BaySickSynth FLT.png` + `BaySickBass Titlebar Crop.png` | crop |
| Instrument | 16 | `BSSBT` | BaySickSynth/Bass Titlebar | Sub | `BSSB` | `BaySickSynth FLT.png` + `BaySickBass Titlebar Crop.png` | crop |
| Instrument | 17 | `BSSBM` | BaySickSynth/Bass Menu | Sub | `BSSB` | `BaySickSynth-BaySickPlayer-BaySickSolstice Menu.png` + `BaySickBass Menu Updated.png`  | full |
| Instrument | 18 | `BSSBOSC` | BaySickSynth/Bass Oscillator | Sub | `BSSB` | `BaySickSynth OSC.png` | crop |
| Instrument | 19 | `BSSBOENV` | BaySickSynth/Bass Oscillator Envelope | Sub | `BSSB` | `BaySickSynth OSC ENV.png` | crop |
| Instrument | 20 | `BSSBFLT` | BaySickSynth/Bass Filter | Sub | `BSSB` | `BaySickSynth FLT.png` | crop |
| Instrument | 21 | `BSSBFENV` | BaySickSynth/Bass Filter Envelope | Sub | `BSSB` | `BaySickSynth FLT ENV.png` | crop |
| Instrument | 22 | `BSSBLFO` | BaySickSynth/Bass LFO | Sub | `BSSB` | `BaySickSynth LFO.png` | crop |
| Instrument | 23 | `BSSBMOD` | BaySickSynth/Bass Mod | Sub | `BSSB` | `BaySickSynth MOD.png` | crop |
| Instrument | 24 | `BSP` | BaySickPlayer | Main | - | `BaySickPlayer.png` | full |
| Instrument | 25 | `BSPM` | BaySickPlayer Menu | Sub | `BSP` | `BaySickSynth-BaySickPlayer-BaySickSolstice Menu.png`  | full |
| Instrument | 26 | `BSDRUMS` | BaySickDrums | Main | - | `Drum Kit.png` | full |
| Instrument | 27 | `BSDM` | BaySickDrums Menu | Sub | `BSDRUMS` | `BaySickDrum Menu.png` | full |
| Instrument | 28 | `BSRDTTL` | BaySickRustyDrums | Main | - | `BaySickRustyDrums Main.png` | crop |
| Instrument | 29 | `BSRDMENU` | BaySickRustyDrums Menu | Sub | `BSRDTTL` | `BaySickRustyDrums Menu.png` | full |
| Instrument | 30 | `BSRDMAIN` | BaySickRustyDrums Main | Sub | `BSRDTTL` | `BaySickRustyDrums Main.png` | full |
| Instrument | 31 | `BSRDKICK` | BaySickRustyDrums Kick | Sub | `BSRDTTL` | `BaySickRustyDrums Kick.png` | full |
| Instrument | 32 | `BSRDSNARE` | BaySickRustyDrums Snare | Sub | `BSRDTTL` | `BaySickRustyDrums Snare.png` | full |
| Instrument | 33 | `BSRDTOM` | BaySickRustyDrums Toms | Sub | `BSRDTTL` | `BaySickRustyDrums Toms.png` | full |
| Instrument | 34 | `BSRDHAT` | BaySickRustyDrums Hi-Hat | Sub | `BSRDTTL` | `BaySickRustyDrums Hi-Hat.png` | full |
| Instrument | 35 | `BSRDCYMB` | BaySickRustyDrums Cymbals | Sub | `BSRDTTL` | `BaySickRustyDrums Cymbals.png` | full |
| Instrument | 36 | `BSRDNOISE` | BaySickRustyDrums Noises and Clicks | Sub | `BSRDTTL` | `BaySickRustyDrums Noises and Clicks.png` | full |
| Instrument | 37 | `BSRDKIT` | BaySickRustyDrums Drum Kit | Sub | `BSRDTTL` | `BaySickRustyDrums Drum Kit.png` | full |
| Instrument | 38 | `BSRDMAP` | BaySickRustyDrums Note Map | Sub | `BSRDTTL` | `Rusty Keys.png` | full |
| Instrument | 39 | `BSPLUG` | Hosted Plugin | Main | - | `Hosted Plugin.png` | full |
| Instrument | 40 | `BSPLUGM` | Hosted Plugin Menu | Sub | `BSPLUG` | `BaySickPlugins Menu.png`  | full |
| Mixing & Effects | 1 | `MIX` | Mixer | Main | - | `Mixer.png` | full |
| Mixing & Effects | 2 | `MIXMNU` | Mixer Menu | Sub | `MIX` | `Mixer Menu.png` | full |
| Mixing & Effects | 3 | `MIXADD` | Mixer Add | Sub | `MIX` | `Mixer Add.png` | full |
| Mixing & Effects | 4 | `MIXSTP` | Mixer Strip | Sub | `MIX` | `Mixer Strip Crop.png` | crop |
| Mixing & Effects | 5 | `MIXSM` | Mixer Send Menu | Sub | `MIX` | `Send Menu.png` | full |
| Mixing & Effects | 6 | `ANLZ` | Master Analyzer | Sub | `MIX` | `Analyzer.png` | full |
| Mixing & Effects | 7 | `ANLZM` | Master Analyzer Menu | Sub | `ANLZ` | `Analyzer Menu.png` | full |
| Mixing & Effects | 8 | `FXI` | Effects Rack | Main | - | `Effects Rack.png` | full |
| Mixing & Effects | 9 | `FXRM` | Effects Rack Menu | Sub | `FXI` | `Effects Rack Menu.png` | full |
| Mixing & Effects | 10 | `FXPICK` | Effect Picker List | Sub | `FXI` | `Effects Panel List.png` | full |
| Mixing & Effects | 11 | `FX` | Effect Panel | Sub | `FXI` | `Effects Panel.png` | full |
| Mixing & Effects | 12 | `FXV` | Effect Panel w/ Visual | Sub | `FX` | `Effects Panel with Visual.png` | full |
| Mixing & Effects | 13 | `FXM` | Effect Panel Menu | Sub | `FX` | `Effects Panel Menu.png` | full |
| Mixing & Effects | 14 | `EQ` | EQ Window | Sub | `FXI` | `EQ.png` | full |
| Mixing & Effects | 15 | `EQB` | EQ Band Menu | Sub | `EQ` | `EQ Band Menu.png` | full |
| Mixing & Effects | 16 | `VUMTR` | VU Meter | Sub | `FXI` | `VU Meter.png` | full |
### Codes that changed from the plan's proposal

The plan's group table (batch plan, Namespace 1) proposed codes before the
gap-fill and 2026-08-12 rounds. Three files had no code and got one here;
naming is mine per standing convention.

| Code | File | Why |
|---|---|---|
| `FXPICK` | `Effects Panel List.png` | The plan's `FXI` sits in the **System pages** group beside `BLD` / `MIX` / `PR` / `DKIT`, all of which are page WINDOWS. `FXI` was therefore meant for the Effects rack page, not for the picker menu. |
| `FXRM` | `Effects Rack Menu.png` | New figure, 2026-08-12. Sibling of `FXM` (panel menu). |
| `VUMTR` | `VU Meter.png` | New figure, 2026-08-12. New window. |

### `FXI` - resolved 2026-08-12

`FXI` was reserved for the **Effects rack page** and, at first allocation, no
image on disk showed it: `Mixer.png` carries the `FX Rack` button but not the
window it opens, and the `Effects Panel*` figures are the individual effect
windows the rack launches, not the six-slot rack itself.

Jeff shot it the same day. `Effects Rack.png` takes the code the plan intended
for it, and nothing renumbered - which is the whole point of allocating the code
before the figure existed.

### Manual 2 grouping follows `INDEX.md`, not intuition

Corrected at Task 4 (2026-08-12).  Four screens were first tagged by what they
FEEL like rather than by where `INDEX.md` files their document, and Manual 2
mirrors INDEX:

| Screens | Was | Is | Because |
|---|---|---|---|
| `BSNAM`, `BSPDL`, `BSPDLP`, `FXPICK-5` | Instruments | Mixing | `NAM Amp and Cab.md` and `Pedalboard.md` are both filed under **Mixing, effects and tone**. An amp and a pedalboard are tone shaping, not instruments. |
| `BSV` | Vocal | Tabs | `Vox Page.md` is filed under **Making a track - the tab families**. The Vox PAGE is a tab; the **vocal chain** group is the three satellite editors it opens. |
| `FXRM-1..3`, `FXM-4`, `ANLZ-3`, `BSNAM-12`, `BSPDL-4` | Content | Mixing | Preset and export ITEMS that live on a mixing surface. `Presets.md` and `Freeze and Export.md` describe the systems; a `Save FX Rack Preset...` line on the rack menu is documented where the reader meets it, which is the rack. |

Caught because the plan splits Tasks 3-6 by DOCUMENT and the tags disagreed with
it.  The document list governs.

### Retired ids - struck 2026-08-12

Eleven callouts described nothing a reader could act on and were struck at
Jeff's ruling after he read the generated atlas. **Their numbers are retired and
will never be reissued**, which is the whole point of the append-only rule: the
gaps below are load-bearing, not tidiness debt.

| Id | Was | Why it went |
|---|---|---|
| `BLD-4` `BSP-2` `BSP-8` `BSP-15` `BSV-4` `BSRDKICK-1` | Section captions and a panel heading | Numbering a printed word teaches nobody anything. These are group headings in Manual 2 instead. |
| `BSGTR-6` `FX-4` `BSPDL-10` `BSPLUG-3` | The region CONTAINING controls already numbered inside it | Jeff's catch: a marker on "the panel" whose description is "the panel holds the controls below" is noise pointing at itself. |
| `BSBAS-9` | A pointer reading "same as BSGTR-1, BSGTR-4, BSGTR-5" | Not a control. The cross-reference belongs in prose, not as a numbered thing. |

A further **21** entries KEPT their text and LOST their marker: real information
with nothing on screen to point at (why drum samples play at pitch, why one key
can do two things, that plugins run out of process, that an export is not a
recording of playback). Manual 1 lists these under each figure as *Not numbered
on the picture* and still links them to Manual 2. The generator always had that
mechanism; the first pass forced a marker onto all 593 and never used it.

**Callouts after the prune: 582.** Markers on pictures: 561.

### Misfiled ids - struck 2026-08-12

A second pass after Jeff read the atlas: three entries were filed on a screen
they were not about. All three were the SAME subject - what happens when you
click a key to hear a note - scattered across three figures instead of living on
the one that teaches it.

| Id | Was filed on | Is about | Outcome |
|---|---|---|---|
| `BSGTR-11` | BaySickGuitars | The Piano Roll keyboard | Retired. It duplicated `PR-17`, which already teaches it. Jeff's catch. |
| `BSRDMAP-2` | The Rusty Drums MAP window | The Rusty kit GRAPHIC, a different surface | Retired and RELOCATED to `BSRDMAIN-16`, on the figure that shows the graphic. |
| `BSPLUG-6` | Plugins tab | An internal routing difference with no user-visible effect | Retired. Clicking a key works the same either way; the mechanism belongs in Manual 3 (`IMP-77`), not on a picture. |

`PR-17` absorbed the complete story: it now says the audition follows whichever
engine the roll is pointed at, hosted plugins included, and that it goes quiet
rather than erroring when the tab has no instrument.

Eight further entries were checked and cleared. `TABBAR-6`/`TABBAR-7`/`TABBAR-9` name
ribbon slots after the pages they open, `EXP-10` and `FILE-7` quote on-screen
text verbatim, `KEYS-1` lists the category names, and `BSSOL-25` / `BSNAM-12`
deliberately contrast themselves AGAINST another surface - which is the point of
those entries, not a misfiling.

**Callouts after both prunes: 580.** Markers on pictures: 559.

### Labels that pointed at things already numbered - struck 2026-08-12

Jeff's third pass, after `BSA-11` put a marker on what looks like a plain
divider. Seven more of the same shape: a caption or label whose only content was
naming the control sitting next to it, which already had its own number.

| Id | Was | Pointed at something already numbered as |
|---|---|---|
| `TRAN-6` | `BPM` label | `TRAN-7`, the tempo field it labels |
| `BSSBOSC-3` | Display caption | Part of `BSSBOSC-2`, the display itself |
| `BSNAM-31` | `TOP` / `SIDE` label | `BSNAM-20`, the button that sets the view |
| `FXV-8` | Source label | Nothing - "What is being drawn" says less than the drawing |
| `EVT-1` | Event Editor title | `CHR-3`, which teaches window names once |
| `BSVC-1` | Vocal Chain window title | `CHR-3`, same |
| `EQ-10` | Rail GAIN/PAN knobs | `EQ-6`, the selected band they edit |

**The two Align strips were the opposite problem and were KEPT.** `SYNC POINTS`
and `PROTECTED` genuinely look like dividers, and both are places you click and
drag - the entries simply never said so. Rewritten to lead with the gesture:
sync points are hard anchors you place when the automatic pairing gets a phrase
wrong, protected areas are stretches you mark as leave-alone. A strip you can
edit is worth a number; a strip you cannot is not.

**Callouts after three prunes: 573.** Markers on pictures: 555.

### Coverage lost to a bundled entry - restored 2026-08-12

`BSBAS-9` had bundled FOUR controls into one row - Menu, logo, `CUT SELF`,
`SAME PITCH` - as a pointer at the Guitars entries. Retiring it as "not a
control" was right about the row and wrong about the consequence: the Basses
picture was left with visible buttons that nothing numbered. Jeff spotted it.

Checking the rest of the window figures for the same hole found three more
showing a `Menu` button with no number on it.

| Added | On | Why it was missing |
|---|---|---|
| `BSBAS-10` `BSBAS-11` `BSBAS-12` | BaySickBasses | Collateral from the `BSBAS-9` retirement |
| `BSVC-14` | Vocal Chain | Never had one |
| `BSPIT-21` | BaySickPitch | Never had one |
| `BSA-17` | BaySickAlign | Never had one |

The lesson is the bundling, not the retirement. **One row per control**, and a
repeated control gets its own number on the second picture with a one-line
cross-reference rather than a second explanation. A row covering four things
cannot be retired or kept without being wrong about three of them.

Ids are append-only, so `BSBAS-9` stays struck and Basses resumes at 10.

**Callouts: 579.** Markers on pictures: 561.

### A control that stopped existing - struck 2026-08-12

`BaySickPedals.png` was re-shot after the pedalboard strip lost its **NAM/IR**
button earlier the same day. The button had been an inconsistency - a strip
button for one destination, abbreviated to `N/I` in Compact view and crowding the
centred logo - and the route moved into the Menu, where every other surface puts
its navigation.

`BSPDL-3` documented that button. It is struck rather than repointed: the route
still exists, but it is now part of what the **Menu** does, so it belongs in
`BSPDL-1` rather than owning a marker of its own. `BSPDL-1` says so.

The re-shot image is also a different shape - 1296x692 against the old 1917x567 -
so **every** `BSPDL` marker was re-placed against the new layout. This is the case
SC-7's percentages do NOT rescue: percentages survive an image being resized,
not an image being re-taken at a different aspect with the furniture moved.
A re-shoot means re-placing.

**Callouts: 578.** Markers on pictures: 559.

### The `Menu` rows - struck 2026-08-13

Jeff's ruling after reading the atlas end to end: twenty-one callouts existed to
put a number on the `Menu` button of a window, a button `CHR-1` already teaches
once for every window. A dot whose caption is "Menu - see the chrome figure" is
the table telling you what your own eyes already say.

All 21 struck: `BLD-1` `MIX-1` `PR-1` `EQ-1` `FX-1` `FXI-1` `VUMTR-1` `BSSOL-1`
`BSP-1` `BSSBOSC-1` `BSGTR-1` `BSBAS-10` `BSRDMAIN-1` `BSV-1` `BSVC-14` `BSPIT-21`
`BSA-17` `BSNAM-1` `BSPDL-1` `ANLZ-1` `BSPLUG-1`. The variants that also named
the logo or the strip preset picker are the same class - those are `CHR-2` and
`CHR-4`.

Five of them carried per-window ROUTE information that had to survive the row.
It moved into the figure blurbs - plain prose above the picture, unnumbered:
which entries `BLD`'s Menu carries, `MIX`'s VU entries, `BSV`'s four editors,
`BSPDL`'s NAM/IR route, and `VUMTR`'s calibration-only menu.

`FRAME-17` and `BSSOL-2` are the same family but were NOT in this ruling; they
stand until Jeff says otherwise.

**Callouts: 557.** Markers on pictures: 539. Retired to date: 43.

### FRAME-17 and BSSOL-2 struck, and the whole set renumbered - 2026-08-13

The two same-family strays went the way of the 21 `Menu` rows: `FRAME-17` was
"a contained window - see CHR" and `BSSOL-2` was the strip preset picker, taught
once at `CHR-4`. BSSOL-2's presets-are-how-you-learn line became the Manual 2
Presets section's opening prose and a line in the BSSOL figure blurb.

Then every screen was RENUMBERED CONTIGUOUS. The gaps were fallout from a
never-renumber constraint Claude had invented and Jeff never asked for -
figures whose first dot was 2, lists that skipped numbers, broken-looking to
the person the manual is for. Jeff caught it on BSVC. Coordinates travelled
with their controls through the map, so no nudged dot moved. The notes above
this one refer to pre-renumber ids throughout.

**Callouts: 555.** Markers on pictures: 537.

### The restructure - 2026-08-13

Jeff's Manual Structure spreadsheet reorganized the whole atlas: three groups
(Shell / Instrument / Mixing & Effects) with his order and display names, 32
codes renamed (`Manuals/assets/code-rename-map-full-2026-08-13.json`), 35
figures added (31 new menu/bar captures, `Mixer Strip Crop.png`, and crop-view
figures over existing masters), and `CHR` REMOVED ENTIRELY - `CHR-1..7`
retired with it. No shared chrome figure: every window's title bar is covered
on its own figure, busy bars get a dedicated crop view, trivial bars get
nothing, and a Menu button never gets a dot (menus are their own figures).
Callout sets are being rebuilt per figure from the code that defines each
surface - Phase B of `Plans & Specs/Manuals Rebuild Checklist.md`.

### Phase B code-review strikes - 2026-08-13

Seven read-only code audits swept every already-authored set. Struck (with
their figures renumbered): the three synth-panel rows describing the visualizer
that sits ABOVE the tab row and so outside the panel crops (old BSSBFLT-1,
BSSBLFO-1, BSSBOENV-1), the MOD panel's Menu-tooltip row for an internal title
bar that no longer exists (old BSSBMOD-7), the Vocal Chain's move-up/down and
`x` rows (old BSVC-8/9 - the glyphs are painted but have NO mouse handler;
flagged to Jeff as a UI defect, the chain order is locked anyway), the effect
window's "expand handles" row (old FX-8 - nothing draws them), and the Rusty
Main kit-graphic row (old BSRDMAIN-16 - the clickable kit photo lives on the
Rusty Drum Kit sub-tab, a surface no figure captured until Jeff's same-day shot - see `BSRDKIT`). Dozens of
stale labels reworded and missing controls appended in the same pass; the
running notes carry the inventory.

### Tells-nothing bar rows struck - 2026-08-13 (G13)

Jeff's catch on the unified page: FRAME's first dot was "OS title bar" -
a marker that informs no one. The class swept: struck FRAME's OS-title-bar
and window-icon rows, Undo History's window-title row, and the three
name-plate rows on the BaySickSynth / BaySickBass / BaySickRustyDrums bar
figures (figures renumbered; accent-color teaching folded into the chapter
intros). KEPT as genuinely informative: the project-name title text with
its unsaved `*` and `[DEBUG]` rows, the hosted-plugin window title (it
explains tab naming), and the effect window title (channel - effect).

### `[DEBUG]` suffix row struck - 2026-08-14 (G13 follow-up)

The G13 keep on `FRAME`'s `[DEBUG]` row was overruled by Jeff: users only
ever run the Release exe, so a diagnostic-build tag has no business in the
shipping manual. Row struck, the chapter sentence removed, `FRAME`
renumbered again (4..14 -> 3..13).

### `FRAME` OS-buttons and workspace-backdrop rows struck - 2026-08-14 (G13 follow-up)

Jeff's second pass on the Main Window figure: the minimize / maximize /
close row (the OS's own buttons) and the workspace-backdrop row are
pointless - both struck, their chapter paragraphs removed outright, and
`FRAME` renumbered a third time. The fixed-fullscreen fact survives in the
chapter intro.

### Caption-audit strikes - 2026-08-14 (Jeff's nudge-pass notes)

Jeff's sweep of the caption boxes during his dot pass. STRUCK: the Event
Editor resize grip; the Key Binds binding-row / dispatch-order / (none) /
reference-rows tail; the ribbon locked-prefix and frozen-mark rows (the
Slot markings close-up dissolved; the (missing) row survives as prose);
the + menu chevron row; the Plugins status-line row; both Piano Roll and
both Builder scrollbars; the Builder track-group colour band; the File
Settings footnote row plus the keep-captured-takes chapter prose (rows
keep their labels, no added teaching); the BaySickPlayer clip-file row
(the What-is-loaded close-up dissolved, the Preset row survives). MERGED:
Audio Settings Apply + Close into one row. MOVED: the Builder Sort row
(dotless) to the end of its table. Every figure renumbered in physical
row order.

### BaySickSynth/Bass family folded to eight flat children - 2026-08-14 (Jeff)

`BSSBT`+`BSSBT` merged into `BSSBT` (one Titlebar entry, both bars stacked)
and `BSSBM`+`BSSBM` into `BSSBM` (one Menu entry, both menus stacked; the
duplicate 14-row sets collapse to one). Children reordered: Titlebar,
Menu, Oscillator, Oscillator Envelope, Filter, Filter Envelope, LFO, Mod
- all direct children of the Family. Also `BSGBM` gained `BSPDL` as its
placement parent so the sidebar shows it flat under the Pedals family
after Basses instead of folded under Guitars.

### Replace-feature menu pass - 2026-08-16 (Jeff's spec)

The Replace feature changed four menus and Jeff shot four new pictures, so
the menu figures re-based: `BSSBM` now shows the shared
`BaySickSynth-BaySickPlayer-BaySickSolstice Menu.png` beside
`BaySickBass Menu Updated.png`; `BSPM` shows the shared picture alone;
`BSPLUGM` shows `BaySickPlugins Menu.png`. Two NEW figures (88 -> 90):
`BSSOLM` (BaySickSolstice Menu - BaySickSolstice had no menu entry; Sub of `BSSOL`) and
`BSDM` (BaySickDrums Menu - Main, placed right before the BaySickRustyDrums
block; reached from a kit pad's right-click or the drum window's Menu).
New rows: `Replace Engine` at `BSSBM-8`/`BSPM-8`/`BSSOLM-8` (old 8+
renumbered +1), and `Rename...`/`Replace Plugin`/`Duplicate Plugin` at
`BSPLUGM-4`..`6` (old 4-7 -> 7-10). Jeff's nudge export merged same day
(731 dots validated). Follow-up (Jeff): NEW `BSDRUMS` (BaySickDrums, Main,
`Drum Kit.png`, rowless prose entry - the two ways of picking drums, by
location and by type) sits above the menu, and `BSDM` re-parented as its
Sub - 91 figures total. Instrument-group Ords renumbered sequentially to
make room.

### Drum Kit gesture rows struck; keybind pointers added - 2026-08-14 (Jeff)

`DKIT` rows 7 (active-row accent), 11 (wheel scroll) and 12 (drag handle)
struck - 11 and 12 are gestures a still picture cannot show, and Key
Binds is their catalog. In exchange every surface with a Key Binds tab
(Builder, Piano Roll, Drum Kit, Event Editor, Vocal Editor) now closes
its chapter with a pointer to that tab. Also this pass: the BaySickPitch
Safety-nets close-up folded into Key and snapping (same crop already
covers it), and BaySickPedals gained the updated board shot, the open
Standard/Compact dropdown as its Board-controls close-up, and the
Compact-view picture beside it.

### `BSPDLV` folded into `BSPDL` Board controls - 2026-08-14 (Jeff)

The separate BaySickPedals View figure duplicated the dropdown image the
Board-controls close-up already shows, so it retired: its Standard /
Compact rows became `BSPDL-3`/`BSPDL-4` with their dots drawn on that
close-up (the first custom-file cluster dots), its teaching merged into
the Board-controls paragraph, and the rest of `BSPDL` renumbered +2.

### Every dotless box row kicked to prose - 2026-08-14 (Jeff's ruling)

The on-screen box is for numbered dots ONLY.  A registry-wide sweep
removed all 50 rows with no dot on any picture; their teaching stays in
the chapters as the plain prose it already was.  (The sweep's first run
also caught the Namespace-3 topic index by mistake; those 89 rows were
recovered verbatim from session transcripts and reinserted.)  Rows whose dots live on a
custom-file close-up (BSPDL Standard/Compact) now get real numbered
buttons in the box wired to those close-up dots.

### `BSPDL` Standard / Compact rows removed from the box - 2026-08-14 (Jeff)

The main board's box lists the BOARD's dots only; the zoomed dropdown
close-up right below needs no dots or box rows of its own.  Rows 3/4
struck (their teaching stays as plain prose in the Board-controls
paragraph), the close-up's hand-placed dots removed, and the figure
renumbered back down.  Standing rule: additions of that kind get ASKED
about first.

### Title-chrome rows gone everywhere; BSP + Rusty Main to section dots - 2026-08-14 (Jeff)

`BSSBT` and `BSRDTTL` lose their fill/close rows - no figure carries
title-chrome filler.  `BSP` collapses from 20 knob rows to 7 section rows
(the close-ups below the box teach the knobs, as prose).  `BSRDMAIN`
drops its repeating-detail rows (Low cut, Crush, Close/OH, Pan, Tune,
Stick picker, tom EXTRAS, cymbal Dry) - the per-drum section pages are
where the detail lives - keeping Presets, program picker, tabs and the
seven instrument groups.

### Mixer visual rows struck - 2026-08-14 (Jeff)

`MIX` rows 17 (send jack - a marker, not a button), 19 (the drawn routing
cable) and 21 (the horizontal scrollbar) are visual, not controllable -
struck from the box; the jack/cable/right-click teaching stays as chapter
prose.  Renumbered: the `+` and `Analyzer` rows became 17/18.

### Effects Panel with Visual flattened - 2026-08-14 (Jeff)

`FXV` keeps no box rows and no close-ups: the figure is the picture plus
two blurbs - which nine effects have a Visual window, and what the Visual
window is.  The panel/meter and tempo-sync sections are gone with their
rows (the GR-crop plan for the old row 2 is superseded); see-refs into
the removed rows re-aimed.

### Swing rows on every player; Effect Panel nested under the rack - 2026-08-15 (Jeff)

The page swing knob (the red ring beside `Menu`, on every player title
strip since 2026-08-04) was undocumented on every player figure - one row
+ dot added to BSP, BSSOL, BSRDMAIN, BSGTR, BSBAS, BSPLUG and BSSBT.
The Drum Kit view has no swing knob BY DESIGN - its sixteen pickers link
to player pages that each carry their own - so `DKIT` gets no row (the
claim that its screenshot was stale was wrong; Jeff corrected it).
`FX` re-parented Sub of `FXI` so the reference bar nests the
Effect Panel family inside Effects Rack between the picker list and the
EQ window (the body already read in that order).  Also this pass: the
crop-geometry root cause found and fixed - the global border-box reset
made crop image offsets resolve against the box minus its 1px borders,
shifting short bottom-anchored close-ups (the mixer routing strip) ~31px
up; `.shot` is now content-box and renders sub-pixel exact.

### One swing row, one wording - 2026-08-15 (Jeff)

Guitars and Basses already documented the knob (`BSGTR-9` / `BSBAS-11`);
the newly added duplicates are struck, and every player's swing row and
chapter paragraph now carries the `BSGTR-9` text verbatim.

---

## Namespace 3 - `IMP-<n>` implementation topics

One id per implementation topic. Per SC-11 (resolving SSC-5 pick a), Manual 3
covers all DSP modules in full with the magic-number tables transcribed, so
every module below is a full treatment, not a summary row.

Allocated flat and append-only. Blocks are contiguous only as a convenience for
reading; new topics append at the end of the namespace, never inside a block.

### DSP modules - core effect chain (IMP-1..IMP-16)

| IMP | Topic | Source |
|---|---|---|
| IMP-1 | Compressor - Modern / FET / Opto / Pedal modes, TCR auto-release, opto history blend | `Source/DSP/CompressorDSP` |
| IMP-2 | Limiter - Maximizer mode, BS.1770-4 true-peak ceiling, auto-ceiling trim | `Source/DSP/LimiterDSP` |
| IMP-3 | Gate - hysteresis, hold, range | `Source/DSP/GateDSP` |
| IMP-4 | De-esser - split-band detection and the spectral sibilance processor | `Source/DSP/DeEsserDSP`, `SibilanceSpectralProcessor` |
| IMP-5 | Transient Shaper - attack / sustain envelope differencing | `Source/DSP/TransientShaperDSP` |
| IMP-6 | Overdrive - stage cascade and voicing | `Source/DSP/OverdriveDSP` |
| IMP-7 | Saturation - Console / Tape / Tube characters, the Tape Vibe control | `Source/DSP/SaturationDSP` |
| IMP-8 | Chorus - voice count, spread, LFO shaping | `Source/DSP/ChorusDSP` |
| IMP-9 | Flanger - through-zero behaviour and feedback path | `Source/DSP/FlangerDSP` |
| IMP-10 | Phaser - allpass stage count, notch spacing | `Source/DSP/PhaserDSP` |
| IMP-11 | Delay - Echo and Vocal Doubler modes, Spread / Pan tap offsets, sync divisions | `Source/DSP/DelayDSP` |
| IMP-12 | Reverb - tail model, early reflections, bass multiplier | `Source/DSP/ReverbDSP` |
| IMP-13 | De-reverb - reduction and tail estimation | `Source/DSP/DeReverbDSP` |
| IMP-14 | The kbs EQ engine - the 96-band pool, one design path, two-way/onset/spectral dynamics, color, modulators | `Source/DSP/Kbs/ParametricEq` |
| IMP-15 | Domain views + per-band routing + the StripEq wrapper | `Source/DSP/Kbs/ParametricEq`, `Source/DSP/StripEq` |
| IMP-16 | Linear phase - overlap-save FIR + the per-domain 2x2 matrix | `Source/DSP/Kbs/EqLinearPhase` |
### DSP modules - pedal and amp Style models (IMP-17..IMP-34)

| IMP | Topic | Source |
|---|---|---|
| IMP-17 | Blues Drive | `Source/DSP/BluesDriveStyleDSP` |
| IMP-18 | Distortion | `Source/DSP/DistortionStyleDSP` |
| IMP-19 | Fuzz | `Source/DSP/FuzzStyleDSP` |
| IMP-20 | High Gain | `Source/DSP/HighGainStyleDSP` |
| IMP-21 | Octave | `Source/DSP/OctaveStyleDSP` |
| IMP-22 | Wah | `Source/DSP/WahStyleDSP` |
| IMP-23 | Tuner | `Source/DSP/TunerStyleDSP` |
| IMP-24 | Noise Gate (pedal) | `Source/DSP/NoiseGateStyleDSP` |
| IMP-25 | Graphic EQ (pedal) | `Source/DSP/GraphicEQStyleDSP` |
| IMP-26 | Furman EQ | `Source/DSP/FurmanEQStyleDSP` |
| IMP-27 | Synth (pedal) | `Source/DSP/SynthStyleDSP` |
| IMP-28 | NAM Pedal | `Source/DSP/NAMPedalStyleDSP` |
| IMP-29 | Acoustic Preamp | `Source/DSP/AcousticPreampStyleDSP` |
| IMP-30 | Acoustic Simulator | `Source/DSP/AcousticSimulatorStyleDSP` |
| IMP-31 | Bass Compressor | `Source/DSP/BassCompressorStyleDSP` |
| IMP-32 | Bass Driver | `Source/DSP/BassDriverStyleDSP` |
| IMP-33 | Bass Overdrive | `Source/DSP/BassOverdriveStyleDSP` |
| IMP-34 | Bass Graphic EQ | `Source/DSP/BassGraphicEQStyleDSP` |
### DSP modules - mic, amp and cabinet (IMP-35..IMP-37)

| IMP | Topic | Source |
|---|---|---|
| IMP-35 | Mic Sim - the ten archetype EQ curves, four bands each (magic-number table) | `Source/DSP/MicSimDSP` |
| IMP-36 | Mic Placement - 1/sqrt(r) gain, air absorption, proximity shelf, polar response, and the angle/height combination | `Source/DSP/MicPlacementDSP`, `BaySickNAMIRProcessor::combinePlacement` |
| IMP-37 | NAM model loading and the IR convolution cabinet | `Source/BaySickNAMIR` |
### DSP modules - pitch, time and detection (IMP-38..IMP-47)

| IMP | Topic | Source |
|---|---|---|
| IMP-38 | Realtime pitch correction | `Source/DSP/PitchCorrectorDSP` |
| IMP-39 | BaySickPitch offline engine | `Source/DSP/BaySickPitchDSP` |
| IMP-40 | BaySickAlign | `Source/DSP/BaySickAlignDSP` |
| IMP-41 | Phase vocoder - ring sizing and the buffer-matrix fixes | `Source/DSP/PhaseVocoder` |
| IMP-42 | In-house pitch shifters | `Source/DSP/PitchShifters` |
| IMP-43 | Vendored pitch shifters (Signalsmith / Rubber Band / WORLD) | `Source/DSP/LibraryPitchShifters` |
| IMP-44 | Monitor pitch shifter (live path) | `Source/DSP/MonitorPitchShifter` |
| IMP-45 | YIN pitch tracker and its decimation | `Source/DSP/PitchTrackerYIN` |
| IMP-46 | Polyphonic pitch tracker | `Source/DSP/PolyPitchTracker` |
| IMP-47 | BPM detection | `Source/DSP/BpmDetect` |
### DSP modules - metering and support (IMP-48..IMP-56)

| IMP | Topic | Source |
|---|---|---|
| IMP-48 | LUFS metering - momentary / short / integrated / LRA | `Source/DSP/LufsMeterDSP`, `LoudnessSpec` |
| IMP-49 | True-peak meter - BS.1770-4 polyphase | `Source/DSP/TruePeakMeter` |
| IMP-50 | VU ballistics and the app-wide calibration reference | `VUMeter` in `Source/Standalone/SharedUI` |
| IMP-51 | Polyphase oversampling | `Source/DSP/PolyphaseOversampler` |
| IMP-52 | Denoise | `Source/DSP/DenoiseDSP` |
| IMP-53 | Engine sidechain helper and the four receive lines per strip | `Source/DSP/EngineSidechainHelper` |
| IMP-54 | Audio clip streaming and the offline blocking read | `Source/DSP/AudioClipStreamer` |
| IMP-55 | MP3 decode (vendored mpglib) and MP3 write | `Source/DSP/Mp3Writer`, `SafeAudioFormats` |
| IMP-56 | Spectrum and visual feeds - the seqlock audio-to-UI handoff | `Source/DSP/SpectrumFeed`, `EffectVisualFeed` |
### Architecture subsystems (IMP-57..IMP-80)

| IMP | Topic | System Reference source |
|---|---|---|
| IMP-57 | EngineRig - model-owned engines, pages as non-owning views, teardown order | `Workspace and Windows.md` |
| IMP-58 | The contained-window shell - native child peers, z-order, peer-keyed suspend | `Workspace and Windows.md` |
| IMP-59 | Window chrome, sizing and persistence | `Workspace and Windows.md` |
| IMP-60 | Routing graph and the mixer strip pattern | `Mixer.md` |
| IMP-61 | Sends, main-out routing and sidechain edges | `Mixer.md` |
| IMP-62 | Effect racks - slot lifecycle, uuid identity, rack presets | `Effect Racks.md` |
| IMP-63 | Automation - lane registration through the model, offline lane replay | `Automation.md` |
| IMP-64 | MIDI Learn | `MIDI Learn.md` |
| IMP-65 | Patterns and arrangement | `Patterns and Arrangement.md` |
| IMP-66 | Transport, playback and the metronome | `Transport and Playback.md` |
| IMP-67 | Piano Roll | `Piano Roll.md` |
| IMP-68 | Event Editor | `Event Editor.md` |
| IMP-69 | Undo - one app-wide manager, typed actions, structural snapshots | `Undo History.md` |
| IMP-70 | Projects and saving | `Projects and Saving.md` |
| IMP-71 | Project bundles | `Project Bundles.md` |
| IMP-72 | Presets | `Presets.md` |
| IMP-73 | Templates | `Templates.md` |
| IMP-74 | Sample library and file resolution | `Sample Library.md` |
| IMP-75 | Freeze - content stamping, staleness, substitution | `Freeze and Export.md` |
| IMP-76 | True offline render and export | `Freeze and Export.md` |
| IMP-77 | Out-of-process plugin hosting - the bridge protocol and sandbox helpers | `Plugins Page.md` |
| IMP-78 | Hosted plugin editors - sizing, scaling and the DPI contract | `Plugins Page.md` |
| IMP-79 | Keyboard shortcut dispatch and the key-listener ordering rule | `Keyboard Shortcuts.md` |
| IMP-80 | Idle suspend and the dispatcher predicates | `Workspace and Windows.md` |
| IMP-81 | Envelopes - the ADSR shape across every engine | `Manuals/manual-3.html` |
| IMP-82 | The subtractive filter - cutoff, resonance, tracking | `Manuals/manual-3.html` |
| IMP-83 | LFOs - free-running and tempo-synced modulation | `Manuals/manual-3.html` |
| IMP-84 | Unison and voice stacking - the sqrt(N) gain law | `Manuals/manual-3.html` |
| IMP-85 | Voice modes and note cutting - poly / mono / legato, CUT SELF, choke groups | `Manuals/manual-3.html` |
| IMP-86 | The BaySickSynth / BaySickBass voice - oscillator, modifier, noise, transients | `Manuals/manual-3.html` |
| IMP-87 | Additive synthesis - how BaySickSolstice makes sound | `Manuals/manual-3.html` |
| IMP-88 | The sample player - roots, stretch, velocity routing | `Manuals/manual-3.html` |
| IMP-89 | The sampled instruments - SFZ programs, keyswitches, articulations | `Manuals/manual-3.html` |## Namespace 1 - callout allocation

Two collapse rules, per the batch plan:

- A control that repeats WITHIN one image gets ONE callout, labelled as the
  pattern (`MIX-6 Channel fader - every strip has one`).
- A control that repeats ACROSS images is owned by the image where it is taught;
  other images cross-reference it (`see MIX-6`) rather than minting a second id.

Element source is the retired `MANUAL-1 Screenshot List.md` inventory (728 SHOT
entries across 28 sittings), collapsed against what each image actually shows.

**Total at first allocation: 593 callouts across the then-56 screens**, one
section per figure code below. The 2026-08-13 restructure took the set to 90
figures in three groups; per-figure callout sets are being rebuilt from code
(Phase B of `Plans & Specs/Manuals Rebuild Checklist.md`).

That is well under every estimate this batch carried - 1,100-1,500 against 42
images, then 1,450-1,950 against 55 - and the gap IS the two collapse rules
working. A mixer strip is 22 callouts in total rather than 22 per strip times
the number of strips; window title strips resolved to a shared chrome figure at the time
(removed by the 2026-08-13 restructure - bars are per-window now); and Mic A and Mic B, or the eight EQ bands,
or the four tom columns, are each one control set numbered once.

Density runs from 2 (`BSSBFENV`, a panel that is four sliders and a knob) to 32
(`BSNAM`, which carries two full mic chains and a draggable placement view).

---

### `FRAME` - `Main frame.png`

The application frame only. The transport, the ribbon and every contained
window are TAUGHT elsewhere and cross-referenced here, per the across-image
rule - this image exists to show how the pieces sit together, not to name them
a second time.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| FRAME-1 | `BaySickDAW - <project>` title text | Content | IMP-70 | `Projects and Saving.md` |
| FRAME-2 | ` *` unsaved marker | Content | IMP-70 | `Projects and Saving.md` |
| FRAME-3 | `File` menu - *see FMENU* | Content | IMP-70 | `Projects and Saving.md` |
| FRAME-4 | `Edit` menu - *see EMENU* | Writing | IMP-69 | `Undo History.md` |
| FRAME-5 | `Patterns` menu - *see PMENU* | Writing | IMP-65 | `Patterns and Arrangement.md` |
| FRAME-6 | `View` menu - *see VMENU* | Shell | IMP-58 | `Workspace and Windows.md` |
| FRAME-7 | `Options` menu - *see OMENU* | Content | IMP-77 | `Plugins Page.md` |
| FRAME-8 | `Help` menu - *see HMENU* | Shell | - | `Keyboard Shortcuts.md` |
| FRAME-9 | Global transport bar - *see TRAN* | Writing | IMP-66 | `Transport and Playback.md` |
| FRAME-10 | Ribbon tab bar - *see TABBAR* | Tabs | IMP-57 | `Workspace and Windows.md` |
| FRAME-11 | Perf readout - *see TABBAR-12* | Shell | IMP-80 | `Workspace and Windows.md` |
### `TRAN` - `Transport Bar.png`

Owns every transport control. Left to right as the bar reads.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| TRAN-1 | Play | Writing | IMP-66 | `Transport and Playback.md` |
| TRAN-2 | Pause | Writing | IMP-66 | `Transport and Playback.md` |
| TRAN-3 | Stop | Writing | IMP-66 | `Transport and Playback.md` |
| TRAN-4 | Record - the DOT arms and records | Writing | IMP-66 | `Transport and Playback.md` |
| TRAN-5 | Record mode chevron - the right 14px only. Same split hit-target as a ribbon slot, *see TABBAR-3* | Writing | IMP-66 | `Transport and Playback.md` |
| TRAN-6 | BPM field - amber LCD, editable. Right-click offers `Automate tempo` | Writing | IMP-47 | `Transport and Playback.md` |
| TRAN-7 | `TAP` | Writing | IMP-47 | `Transport and Playback.md` |
| TRAN-8 | `PATTERN` - pattern / song toggle | Writing | IMP-65 | `Patterns and Arrangement.md` |
| TRAN-9 | Loop-mode toggle | Writing | IMP-66 | `Transport and Playback.md` |
| TRAN-10 | Metronome | Writing | IMP-66 | `Transport and Playback.md` |
| TRAN-11 | Metronome chevron | Writing | IMP-66 | `Transport and Playback.md` |
| TRAN-12 | Typing-keyboard MIDI - four painted piano keys | Writing | IMP-64 | `MIDI Learn.md` |
| TRAN-13 | Swing knob | Writing | IMP-65 | `Patterns and Arrangement.md` |
| TRAN-14 | Pattern dropdown - `Pattern 1` | Writing | IMP-65 | `Patterns and Arrangement.md` |
| TRAN-15 | Position readout - amber LCD, `0:00.000` | Writing | IMP-66 | `Transport and Playback.md` |

### `TABBAR` - `Ribbon Tab Bar.png`

Owns the ribbon and the perf readout. The eleven slot types and the slot
anatomy collapse to one callout each: every slot is the same control.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| TABBAR-1 | A ribbon slot - every tab type has one, variable width, locked order | Tabs | IMP-57 | `Workspace and Windows.md` |
| TABBAR-2 | Slot name row - bold + full white when that type is selected, dim otherwise. Long names shrink rather than clip | Tabs | IMP-57 | `Workspace and Windows.md` |
| TABBAR-3 | Slot chevron - the bottom-right 22px ONLY. Clicking anywhere else NAVIGATES; only this opens the dropdown | Tabs | IMP-57 | `Workspace and Windows.md` |
| TABBAR-4 | Instance-count badge - white circle, dark number, on the seven instance types | Tabs | IMP-57 | `Workspace and Windows.md` |
| TABBAR-5 | Active-slot top stripe, 2px | Tabs | IMP-57 | `Workspace and Windows.md` |
| TABBAR-6 | `Builder` slot - 8-colour tie-dye gradient, always present | Tabs | IMP-57 | `Builder Page.md` |
| TABBAR-7 | `Mixer` slot - purple, always present, NO chevron | Tabs | IMP-60 | `Mixer.md` |
| TABBAR-8 | `Effects` slot - pink, always present | Tabs | IMP-62 | `Effect Racks.md` |
| TABBAR-9 | `Piano Roll` slot - near-black, always present, NO chevron | Tabs | IMP-67 | `Piano Roll.md` |
| TABBAR-10 | An instance-type slot (Clips gold / Vox teal / Inst navy / Layers orange / Bass green / Drums red / Plugins purple) - EXISTS only while it holds at least one tab, and vanishes at zero | Tabs | IMP-57 | `Engine Tabs (Layers, Bass, Drums).md` |
| TABBAR-11 | `+` add-tab slot - the only route back to a type at zero. *see TABUTN* | Tabs | IMP-57 | `Workspace and Windows.md` |
| TABBAR-12 | Perf readout - three rows, 9pt monospaced, flush right. `SYS` / `DSP` on row 1 (`SYS`: green under 50%, dim yellow 50-80%, yellow above; `DSP`: 50% / 85% thresholds, flashing red above 95%), `MEM` / `LAT` on row 2, `UND` / `PF` on row 3. Its TOOLTIP is the only place the six abbreviations are defined | Shell | IMP-80 | `Workspace and Windows.md` |
### `TABUTN` - `Ribbon + Menu.png`

The `+` slot add-tab menu. This is the whole entry point for creating any tab
type, including the ones whose ribbon slot has vanished at zero instances.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| TABUTN-1 | `BaySickVocal` - adds a Vox tab | Vocal | IMP-57 | `Vox Page.md` |
| TABUTN-2 | `BaySickLiveInst` - adds a live-input Inst tab. Greys at the Inst tab cap | Instruments | IMP-57 | `Inst Page.md` |
| TABUTN-3 | `BaySickGuitars` | Instruments | IMP-57 | `BaySickGuitars.md` |
| TABUTN-4 | `BaySickBasses` | Instruments | IMP-57 | `BaySickBasses.md` |
| TABUTN-5 | `VSTPlugin` submenu - the added third-party instruments, alphabetical. Empty state: `None added - see Options > Plugins` | Instruments | IMP-77 | `Plugins Page.md` |
| TABUTN-6 | `BaySickSolstice` submenu | Instruments | IMP-57 | `BaySickSolstice.md` |
| TABUTN-7 | `BaySickSynth` | Instruments | IMP-57 | `BaySickSynth.md` |
| TABUTN-8 | `BaySickPlayer` submenu | Instruments | IMP-57 | `BaySickPlayer.md` |
| TABUTN-9 | `BaySickBass` | Instruments | IMP-57 | `BaySickBass.md` |
| TABUTN-10 | `BaySickDrums` submenu | Instruments | IMP-57 | `Engine Tabs (Layers, Bass, Drums).md` |
| TABUTN-11 | `BaySickRustyDrums` - greys while a Rusty tab is already live (one kit at a time) | Instruments | IMP-57 | `BaySickRustyDrums.md` |
### `BLD` - `Builder.png`

Owns the arrangement grid and the source picker.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BLD-1 | `Edit` menu | Writing | IMP-69 | `Builder Page.md` |
| BLD-2 | `View` menu | Writing | IMP-65 | `Builder Page.md` |
| BLD-3 | `Patterns` / `Files` / `Auto` - the three picker sources | Writing | IMP-65 | `Builder Page.md` |
| BLD-4 | `+ Add` - new pattern | Writing | IMP-65 | `Patterns and Arrangement.md` |
| BLD-5 | `Delete` - remove the selected pattern. Marker-aware: deleting a pattern retires its markers with it | Writing | IMP-65 | `Patterns and Arrangement.md` |
| BLD-6 | A pattern row in the picker - `Pattern 1`. Every pattern has one | Writing | IMP-65 | `Patterns and Arrangement.md` |
| BLD-7 | `Snap` toggle | Writing | IMP-65 | `Builder Page.md` |
| BLD-8 | A tool button - `Draw(P)` / `Paint(B)` / `Select(E)` / `Delete(D)` / `Mute(T)` / `Slice(C)` / `Zoom(Z)` / `Play(Y)`. One is active at a time and the bracketed letter is its key bind | Writing | IMP-79 | `Keyboard Shortcuts.md` |
| BLD-9 | `Stretch (S) v` - tool plus its own dropdown, tempo-relative stretch | Writing | IMP-65 | `Builder Page.md` |
| BLD-10 | `Undo` / `Redo` - greyed when the app-wide stack is empty at that end | Writing | IMP-69 | `Undo History.md` |
| BLD-11 | `H` - opens the undo history (*see UNDO*) | Writing | IMP-65 | `Builder Page.md` |
| BLD-12 | `-` / `+` - horizontal zoom out / in | Writing | IMP-65 | `Builder Page.md` |
| BLD-13 | Breadcrumb - `Playlist > Pattern 1`, the grid current scope | Writing | IMP-65 | `Patterns and Arrangement.md` |
| BLD-14 | Bar ruler with bar numbers and beat ticks | Writing | IMP-65 | `Patterns and Arrangement.md` |
| BLD-15 | A track row - 500 rows exist, 40px each. Every row has the same three controls | Writing | IMP-65 | `Builder Page.md` |
| BLD-16 | `M` - row mute | Writing | IMP-65 | `Builder Page.md` |
| BLD-17 | `S` - row solo | Writing | IMP-65 | `Builder Page.md` |
| BLD-18 | Row name - `Track 1`, renameable, and rename is a first-class undoable action | Writing | IMP-69 | `Undo History.md` |
| BLD-19 | The grid canvas - blocks are drawn here; an edit snapshots blocks, row names and per-row group / colour / mute / solo as one undo unit | Writing | IMP-69 | `Undo History.md` |
| BLD-20 | Browser edge divider - drag resizes the browser (magnetic floor); drag past the floor to collapse it, and a chevron marks it collapsed | Writing | IMP-65 | `Builder Page.md` |
| BLD-21 | Ruler markers - yellow time-marker pennant, blue time-signature pill (`4/4`; outline = pattern-linked, solid = manual), amber tempo pill | Writing | IMP-65 | `Builder Page.md` |
| BLD-22 | Playhead - the green flag in the ruler with its line down the grid | Writing | IMP-66 | `Transport and Playback.md` |
### `MIX` - `Mixer.png`

Owns the mixer strip. Every strip is the same control set, so the strip anatomy is numbered once. The live-input strip extras are numbered on the Mixer Strip figure (see MIXSTP).

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| MIX-1 | `Add` - new bus or channel | Mixing | IMP-60 | `Mixer.md` |
| MIX-2 | Strip header - the channel name, with the strip accent colour as a top rule. `Master`, `FX Bus`, `Bass Bus`, `Bass 1` here | Mixing | IMP-60 | `Mixer.md` |
| MIX-3 | Bus collapse triangle - Bus strips only; collapses / expands that bus's channel strips, greyed when the bus has no members | Mixing | IMP-60 | `Mixer.md` |
| MIX-4 | `M` - strip mute. Every strip has one | Mixing | IMP-60 | `Mixer.md` |
| MIX-5 | `S` - strip solo. Every strip has one | Mixing | IMP-60 | `Mixer.md` |
| MIX-6 | `FX Rack` - opens that channel six-slot rack. Every strip has one | Mixing | IMP-62 | `Effect Racks.md` |
| MIX-7 | `FX Bypass` - bypasses the whole rack for that strip | Mixing | IMP-62 | `Effect Racks.md` |
| MIX-8 | `Master FX By...` - Master only, bypasses the master rack | Mixing | IMP-62 | `Effect Racks.md` |
| MIX-9 | Pan knob - every strip has one | Mixing | IMP-60 | `Mixer.md` |
| MIX-10 | Width knob - every strip has one | Mixing | IMP-60 | `Mixer.md` |
| MIX-11 | Polarity toggle - `Standard` / `Reverse`, click to invert. Pan law lives in the window Menu (*see MIXMNU*) | Mixing | IMP-60 | `Mixer.md` |
| MIX-12 | Master metering-mode dropdown - `Integrated` / `Momentary` and the rest | Mixing | IMP-48 | `Mixer.md` |
| MIX-13 | Channel fader - every strip has one | Mixing | IMP-60 | `Mixer.md` |
| MIX-14 | Fader scale, dB | Mixing | IMP-60 | `Mixer.md` |
| MIX-15 | Strip meter - post fader, pan and width. Split layout on every strip but Master: peak bars below, a scrolling RMS-history band above | Mixing | IMP-48 | `Mixer.md` |
| MIX-16 | Gain readout - `0.0 dB` | Mixing | IMP-60 | `Mixer.md` |
| MIX-17 | `+` - add a send on that strip | Mixing | IMP-61 | `Mixer.md` |
| MIX-18 | `Analyzer` - opens the Master Analyzer, *see ANLZ* | Mixing | IMP-48 | `Mixer.md` |
### `PR` - `Piano Roll.png`

Owns the note grid, the keyboard and the controller lane. The Drum Kit is the
same window with a drum kit bound to it (see DKIT), so it cross-references
most of this.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| PR-1 | Tab picker - `Bass 1 v`. Which tab the roll is editing; the roll follows this, not the ribbon selection | Writing | IMP-67 | `Piano Roll.md` |
| PR-2 | `Player Page` - jumps to the engine window for that tab | Tabs | IMP-57 | `Piano Roll.md` |
| PR-3 | `FX Rack` - that tab rack | Mixing | IMP-62 | `Effect Racks.md` |
| PR-4 | `Edit` menu | Writing | IMP-69 | `Piano Roll.md` |
| PR-5 | `Tools` menu | Writing | IMP-67 | `Piano Roll.md` |
| PR-6 | `Scale` menu | Writing | IMP-67 | `Piano Roll.md` |
| PR-7 | `Chords` menu | Writing | IMP-67 | `Piano Roll.md` |
| PR-8 | `View` menu | Writing | IMP-67 | `Piano Roll.md` |
| PR-9 | `Snap` toggle | Writing | IMP-67 | `Piano Roll.md` |
| PR-10 | Tool buttons - `Draw` / `Paint` / `Del` / `Mute` / `Slice` / `Select` / `Zoom`, one active at a time | Writing | IMP-79 | `Keyboard Shortcuts.md` |
| PR-11 | `Undo` / `Redo` - the app-wide stack, *see BLD-10* | Writing | IMP-69 | `Undo History.md` |
| PR-12 | `H` - opens the undo history (*see UNDO*) | Writing | IMP-67 | `Piano Roll.md` |
| PR-13 | Target readout - `Bass 1 - BaySickBass`, the tab and the engine the notes will reach | Writing | IMP-57 | `Piano Roll.md` |
| PR-14 | Bar ruler | Writing | IMP-65 | `Patterns and Arrangement.md` |
| PR-15 | Playhead - the green marker at bar 1 | Writing | IMP-66 | `Transport and Playback.md` |
| PR-16 | Keyboard - clicking a key auditions that note on whichever engine the roll is pointed at, including hosted plugins. Silent if that tab has no instrument loaded yet, rather than erroring | Instruments | IMP-57 | `Piano Roll.md` |
| PR-17 | Octave label - `C6` / `C7` / `C8` on each C | Writing | IMP-67 | `Piano Roll.md` |
| PR-18 | Note grid - rows are semitones, the C rows are banded lighter | Writing | IMP-67 | `Piano Roll.md` |
| PR-19 | Controller lane picker - `Control > Velocity v`. Chooses which per-note controller the lane below edits. The header also drag-resizes the lane vertically - a clean click picks the mode, a drag sets the app-wide lane height | Writing | IMP-67 | `Piano Roll.md` |
| PR-20 | Controller lane - the bars under each note, with the 25 / 50 / 75 % guide lines | Writing | IMP-67 | `Piano Roll.md` |

| PR-21 | Armed note-type button - reads `Flat` / `RP Slide` / `RT Slide` / `Porta` / `Bend`. Click or `S` cycles it; `S` with notes selected converts them | Writing | IMP-67 | `Piano Roll.md` |
### `DKIT` - `Drum Kit.png`

The same roll window bound to a drum kit. Only what DIFFERS from the Piano
Roll is numbered (see PR); everything else cross-references it.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| DKIT-1 | Tab picker reading `Drum Kit v` - *see PR-1* | Writing | IMP-67 | `Engine Tabs (Layers, Bass, Drums).md` |
| DKIT-2 | `Lock/Unlock 1-16` - locks the whole visible bank of rows against edits | Writing | IMP-67 | `Engine Tabs (Layers, Bass, Drums).md` |
| DKIT-3 | A kit row - sixteen per bank. Every row has the same four controls | Instruments | IMP-57 | `Engine Tabs (Layers, Bass, Drums).md` |
| DKIT-4 | `Pick a sound v` - the row sound picker. This is what an empty row reads, and picking here is what gives the row a voice | Instruments | IMP-74 | `Sample Library.md` |
| DKIT-5 | `M` - row mute | Instruments | IMP-57 | `Engine Tabs (Layers, Bass, Drums).md` |
| DKIT-6 | `S` - row solo | Instruments | IMP-57 | `Engine Tabs (Layers, Bass, Drums).md` |
| DKIT-7 | `1-16` / `17-32` - switches between the TWO INDEPENDENT kits: separate drums bus, separate kit file, separate lock; a drum never moves between them | Instruments | IMP-57 | `Engine Tabs (Layers, Bass, Drums).md` |
| DKIT-8 | `Kit v` - kit load / save menu - saves / loads the 16 drums of the kit you are on | Content | IMP-72 | `Presets.md` |
| DKIT-9 | `Sel` - the roll `Select` tool under its drum-kit label, *see PR-10* | Writing | IMP-79 | `Keyboard Shortcuts.md` |
| DKIT-10 | Row audition key - the white piano key at the row's right end; greys while held | Writing | IMP-67 | `Engine Tabs (Layers, Bass, Drums).md` |
### `EQ` - `EQ.png`

The EQ window (96-band pool, QA-EqFlagship). Every band is the same control
set, so each band control is ONE callout per the within-image rule.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| EQ-1 | `Pre EQ` / `Post EQ` - which of the strip's two EQs this window is showing (QA-EqPro window) | Mixing | IMP-14 | `EQ.md` |
| EQ-2 | `ST` / `MID` / `SIDE` - the three domain views. The view a band lives in IS its domain; the other views ghost | Mixing | IMP-15 | `EQ.md` |
| EQ-3 | The band chips - 24 numbered pills per PAGE of the 96-band pool (arrows appear when a page fills), one shared pool across the views, plus `+` | Mixing | IMP-14 | `EQ.md` |
| EQ-4 | The `A`/`B` pill - two complete setups; click swaps, right-click Copy A to B / Lock. The MORPH strip beside it drags the live setup toward the other bank, one undo step | Mixing | IMP-14 | `EQ.md` |
| EQ-5 | Grid + frequency scale - gain scale selectable 3..30 dB; 20 Hz to 20 kHz, logarithmic | Mixing | IMP-14 | `EQ.md` |
| EQ-6 | A band handle - numbered dot, type glyph above, dynamic ring + mini GR meter, L/R badge | Mixing | IMP-14 | `EQ.md` |
| EQ-7 | The live spectrum - pre dim / post bright / sidechain, with spectrogram, phase and piano opt-ins | Mixing | IMP-14 | `EQ.md` |
| EQ-8 | The headphone button - latches band LISTEN on the selected band | Mixing | IMP-14 | `EQ.md` |
| EQ-9 | The crosshair - arms the spectrum grab (max-held peaks; one grab per arming) | Mixing | IMP-14 | `EQ.md` |
| EQ-10 | Rail `GAIN` / `SAT` / `PAN` knobs - the band's gain, its saturation, its stereo placement | Mixing | IMP-15 | `EQ.md` |
| EQ-11 | Rail `FREQ` / `Q` drag-numbers - drag or double-click to type | Mixing | IMP-14 | `EQ.md` |
| EQ-12 | Rail type glyph grid (9 incl. All Pass) + `ST`/`L`/`R` + `SLOPE` (continuous dB/oct) / `PHASE` numbers | Mixing | IMP-14 | `EQ.md` |
| EQ-13 | Rail `DYNAMICS` - `DYN`/`AUTO`/`EXT`, `DOWN`/`UP`, `THR`/`RATIO`/`ATK`/`REL`, the second stage `THR B`/`RATIO B`/`RANGE B`, `ONSET`, `DENSE`, GR meter | Mixing | IMP-14 | `EQ.md` |
| EQ-14 | Rail `MOD` block - `RATE`/`LFO`/`ENV` knobs with `F`/`G`/`Q` target rows (per-band modulators, IIR modes) | Mixing | IMP-14 | `EQ.md` |
| EQ-15 | The window menu - modes with computed latencies, Color, Whole Curve, Delta Listen, Sketch, Instances, analyser/view options, EQ Match, presets | Mixing | IMP-16 | `EQ.md` |

### `EQB` - `EQ Band Menu.png`

The per-band menu. Reached from a band handle or its column chevron.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| EQB-1 | `Type` submenu - `Bell` / `Low Pass` / `High Pass` / `Low Shelf` / `High Shelf` / `Notch` / `Band Pass` / `Tilt` / `All Pass` | Mixing | IMP-14 | `EQ.md` |
| EQB-2 | `Slope` submenu - detents `6`..`96 dB/oct` + `Brickwall` (filter types only; the rail's SLOPE number covers every value between) | Mixing | IMP-16 | `EQ.md` |
| EQB-3 | `Channel` submenu - `Stereo` / `Left` / `Right`, Stereo view ONLY (Mid/Side are the views) | Mixing | IMP-15 | `EQ.md` |
| EQB-4 | `Move to` submenu - `Stereo view` / `Mid view` / `Side view`, settings kept | Mixing | IMP-15 | `EQ.md` |
| EQB-5 | `Dynamic` submenu - `Make Dynamic` / `Auto Release` / `Spectral (linear modes)` (gain types + Notch) | Mixing | IMP-14 | `EQ.md` |
| EQB-6 | `Listen` / `Isolate` / `Delta Listen` / `Mute` rows | Mixing | IMP-14 | `EQ.md` |
| EQB-7 | `Split to Left + Right` / `Link Selected Bands` / `Unlink` rows (context-gated) | Mixing | IMP-15 | `EQ.md` |
| EQB-8 | `Reset Band` / `Delete Band` | Mixing | IMP-14 | `EQ.md` |

### `FX` - `Effects Panel.png`

One effect window. Every effect panel shares this frame, so the panel anatomy
is taught here and the individual effects cross-reference it.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| FX-1 | Window title - `<channel> - <effect>`, e.g. `Master - De-esser` | Mixing | IMP-62 | `Effect Racks.md` |
| FX-2 | Bypass LED in the title strip - green = active, RED = bypassed; click toggles the slot bypass | Mixing | IMP-62 | `Effect Racks.md` |
| FX-3 | A panel knob - every effect parameter is one of these, and every one answers to right-click, *see PRMMNU* | Mixing | IMP-63 | `Effect Modules.md` |
| FX-4 | Knob caption - the parameter name under the knob | Mixing | - | `Effect Modules.md` |
| FX-5 | A switch toggle - opt-in styling, used where a parameter is a two-state choice rather than a range | Mixing | - | `Effect Modules.md` |
| FX-6 | `Vol` knob - slot output trim on most panels; pedal-native panels and the CS Style Compressor use their own `Level` instead | Mixing | IMP-62 | `Effect Racks.md` |
| FX-7 | Output dBFS meter - the vertical bar at the right, scale in dBFS. Every panel has one. **This is not a VU**: the app one VU is its own window, *see VUMTR* | Mixing | IMP-48 | `Effect Racks.md` |
### `FXV` - `Effects Panel with Visual.png`

Nine effects have a Visual window - Chorus, Compressor, Delay, Flanger, Limiter, Phaser, Reverb, Saturation, Transient Shaper. Elsewhere the menu entry is absent, not greyed. The Visual window's own Menu has a single item: `Lock to Effect Window` / `Unlock from Effect Window`.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
### `FXM` - `Effects Panel Menu.png`

An effect window own Menu. The entries present depend on the effect.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| FXM-1 | `Show Advanced Controls` - Advanced is a MODE, not a tab. The entry reads `Show Basic Controls` when Advanced is already on | Mixing | IMP-62 | `Effect Modules.md` |
| FXM-2 | `Mode: <name>...` - the effect character menu. Present only on effects that have modes | Mixing | IMP-1 | `Effect Modules.md` |
| FXM-3 | `SC: <state>...` - sidechain source pick, from the four receive lines the strip carries | Mixing | IMP-53 | `Mixer.md` |
| FXM-4 | `Presets...` | Mixing | IMP-72 | `Presets.md` |
| FXM-5 | `Visual` - opens that effect Visual window, *see FXV*. Absent - not greyed - on effects with no visual | Mixing | IMP-56 | `Effect Racks.md` |
### `FXPICK` - `Effects Panel List.png`

The picker that fills an empty rack slot. Six groups, fixed order.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| FXPICK-1 | `Dynamic` group - `Compressor`, `De-esser`, `Gate`, `Limiter`, `Transient Shaper` | Mixing | IMP-1 | `Effect Modules.md` |
| FXPICK-2 | `Harmonics` group - `Overdrive`, `Saturation` | Mixing | IMP-6 | `Effect Modules.md` |
| FXPICK-3 | `Modulation` group - `Chorus`, `Flanger`, `Phaser` | Mixing | IMP-8 | `Effect Modules.md` |
| FXPICK-4 | `Time` group - `De-reverb`, `Delay`, `Reverb` | Mixing | IMP-11 | `Effect Modules.md` |
| FXPICK-5 | `Pedals` submenu - the pedal-native effects, *see BSPDLP*. The rack's `Pedals` submenu is a SUBSET with its own section headers, not the pedalboard's full list | Mixing | IMP-17 | `Pedalboard.md` |
| FXPICK-6 | `VST Plugins` submenu - third-party effects added under `Options > Plugins` | Mixing | IMP-77 | `Plugins Page.md` |

### `FXRM` - `Effects Rack Menu.png`

The rack window own Menu.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| FXRM-1 | `Save FX Rack Preset...` - all six slots plus both EQs for the selected channel | Mixing | IMP-62 | `Effect Racks.md` |
| FXRM-2 | `Load FX Rack Preset` submenu - greyed while nothing is saved | Mixing | IMP-62 | `Effect Racks.md` |
| FXRM-3 | `Open Presets Folder` | Mixing | IMP-72 | `Presets.md` |
| FXRM-4 | `VU Calibration (0 VU = ...)` submenu - app-wide, -18 through -14 dBFS, tick on the current value. The SAME submenu appears on the VU window own menu, *see VUMTR-3* | Mixing | IMP-50 | `Effect Racks.md` |
| FXRM-5 | `VU Meter` - opens the master VU window, *see VUMTR* | Mixing | IMP-50 | `Effect Racks.md` |

### `FXI` - `Effects Rack.png`

The rack page: six slots for one channel at a time, plus that channel two EQs.
Every slot is the same control, so the slot collapses to one callout per control.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| FXI-1 | `Channel:` picker - which channel rack is on screen. The rack window is ONE window that re-points, not one window per channel | Mixing | IMP-62 | `Effect Racks.md` |
| FXI-2 | `FX Bypass` - bypasses all six slots for the selected channel. The same control as the strip `FX Bypass`, *see MIX-7* | Mixing | IMP-62 | `Effect Racks.md` |
| FXI-3 | `Pre EQ` - opens that channel pre-rack EQ, *see EQ* | Mixing | IMP-14 | `EQ.md` |
| FXI-4 | `Post EQ` - opens that channel post-rack EQ, *see EQ* | Mixing | IMP-14 | `EQ.md` |
| FXI-5 | A rack slot - a loaded row has six affordances (bypass LED, name plate, up, down, chevron, remove); an empty row has three | Mixing | IMP-62 | `Effect Racks.md` |
| FXI-6 | Slot label - reads `Empty` until an effect is in it, then the effect name. Clicking an empty slot opens the picker, *see FXPICK* | Mixing | IMP-62 | `Effect Racks.md` |
| FXI-7 | Slot move up / move down - the two triangles. Reordering moves the EFFECT; its window follows it, because a slot window is keyed on the effect uuid rather than the slot index | Mixing | IMP-62 | `Effect Racks.md` |
| FXI-8 | Slot chevron - opens the effect picker for that slot | Mixing | IMP-62 | `Effect Racks.md` |
| FXI-9 | Per-slot bypass LED - green = active, red = bypassed; click toggles | Mixing | IMP-62 | `Effect Racks.md` |
| FXI-10 | Per-slot remove `x` - red cross at the row's right end, loaded rows only | Mixing | IMP-62 | `Effect Racks.md` |
### `MIXSM` - `Send Menu.png`

Reached from a strip's `+` button - the jack itself is a painted indicator, not a control. On Master the `+` opens the Analyzer instead, and main-out-locked strips omit the routing rows entirely.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| MIXSM-1 | `Send...` submenu - pick a destination AUX strip, or `New Aux Strip` to make one | Mixing | IMP-61 | `Mixer.md` |
| MIXSM-2 | `Sidechain...` submenu - feed this strip signal to another strip sidechain receive line | Mixing | IMP-53 | `Mixer.md` |
| MIXSM-3 | `Move Output...` submenu - change where the strip main output lands | Mixing | IMP-61 | `Mixer.md` |
| MIXSM-4 | `Add Main Out...` submenu - a strip may feed more than one main output | Mixing | IMP-61 | `Mixer.md` |
| MIXSM-5 | `Remove Main Out...` submenu | Mixing | IMP-61 | `Mixer.md` |

### `VUMTR` - `VU Meter.png`

The app one VU. Master output only. Its `Menu` carries the calibration submenu and nothing else.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| VUMTR-1 | Analog VU face - needle ballistics, not a peak meter | Mixing | IMP-50 | `Effect Racks.md` |
| VUMTR-2 | `CURRENT` and `MAX` readout boxes | Mixing | IMP-50 | `Effect Racks.md` |
| VUMTR-3 | `VU Calibration (0 VU = ...)` on this window Menu - the same app-wide value the rack menu sets, *see FXRM-4* | Mixing | IMP-50 | `Effect Racks.md` |
### `BSSOL` - `BaySickSolstice.png`

Additive engine, single page, no tabs. Grouped by the painted section captions. The `Preset` picker on its strip loads a ready-made sound - the fastest way to learn the engine is to load one and move things.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSSOL-1 | `OUTPUT` section - `VOL`, `PAN`, and the `VEL` / `CUT SELF` / `SAME PITCH` / `AG: REL` voice switches | Instruments | IMP-85 | `BaySickSolstice.md` |
| BSSOL-2 | `TREMOLO` section - `WAVE` selector plus `DEPTH`, `SPEED`, `GAP` | Instruments | IMP-87 | `BaySickSolstice.md` |
| BSSOL-3 | `ROUTING` section - `SUB`, `PROT`, `CLIP`, `FX`, `VOL`, `ENV` | Instruments | IMP-87 | `BaySickSolstice.md` |
| BSSOL-4 | `VIBRATO / LEGATO` section - `WAVE`, `DEPTH`, `SPEED`, `ENV`, `GLIDE`, `LIMIT` and the `LEGATO` switch | Instruments | IMP-87 | `BaySickSolstice.md` |
| BSSOL-5 | `UNISON` section - `VOICES` with its `ALT` switch, then `PAN`, `PITCH`, `PHASE`. Voice gain follows sqrt(N) so stacking voices does not raise level | Instruments | IMP-84 | `BaySickSolstice.md` |
| BSSOL-6 | `FILTER 1 + ADSR` - type picker (`LP` here), `ENV`, `FR...`, `RES`, `KB`, then `ATK` / `DEC` / `SUS` / `REL`. Its envelopes run per-sample | Instruments | IMP-87 | `BaySickSolstice.md` |
| BSSOL-7 | `FILTER 2 + ADSR` - the same control set, second filter, `Notch` here | Instruments | IMP-87 | `BaySickSolstice.md` |
| BSSOL-8 | `TIMBRE` section, `A` / `B` part selector - **view state only**. Both parts always render; nothing in the DSP reads this | Instruments | IMP-87 | `BaySickSolstice.md` |
| BSSOL-9 | `PART A` / `PART B` waveform buttons | Instruments | IMP-87 | `BaySickSolstice.md` |
| BSSOL-10 | `MIX` - the A/B timbre crossfade. This is the control that blends the two parts, not the selector | Instruments | IMP-87 | `BaySickSolstice.md` |
| BSSOL-11 | `VOICE A` / `VOICE B` / `BROWN` / `F1 OFS` / `F2 OFS` / `A MASK` / `B MASK` | Instruments | IMP-87 | `BaySickSolstice.md` |
| BSSOL-12 | `PITCH` section - `FREQ`, `DETUNE`, `FRAC`, and the `OCT` / `Hz` unit switch | Instruments | IMP-87 | `BaySickSolstice.md` |
| BSSOL-13 | `LFO MOD` section - `RATE`, `SHAPE`, the `TEMPO` sync switch, `VEL`, `VOL`, `PITCH` | Instruments | IMP-83 | `BaySickSolstice.md` |
| BSSOL-14 | `STRUM` section - `DIR`, `TIME`, `TNS` | Instruments | IMP-87 | `BaySickSolstice.md` |
| BSSOL-15 | `FX - PLUCK / PHASER / EQ` - `PLUCK`, the `BLU` switch, `MIX`, `DEPTH`, `RATE`, `WIDTH`, `OFS`, `MASK`, `EQ` | Instruments | IMP-87 | `BaySickSolstice.md` |
| BSSOL-16 | `AMP ENV / PHASE` - `ATK` / `DEC` / `SUS` / `REL` plus `START`, `RAND` | Instruments | IMP-81 | `BaySickSolstice.md` |
| BSSOL-17 | `BLUR / PRISM` - `BLUR`, `TIME`, `HARM`, `PRISM`, `MODE` | Instruments | IMP-87 | `BaySickSolstice.md` |
| BSSOL-18 | `MOD` XY pad with the `X` / `Y` / `Z` knobs beneath it | Instruments | IMP-87 | `BaySickSolstice.md` |
| BSSOL-19 | `SPECTROGRAM` - live picture of the harmonics as they sound | Instruments | IMP-56 | `BaySickSolstice.md` |
| BSSOL-20 | Modulation target picker - `Volume` | Writing | IMP-87 | `Automation.md` |
| BSSOL-21 | Modulation source picker - `Envelope` / `LFO` / `Velocity` / `Keyboard` / `Mod X` / `Mod Y` / `Mod Z` | Writing | IMP-87 | `Automation.md` |
| BSSOL-22 | Mod editor toolbar - `ENV` / `CURVE` / `STEP` / `SNAP` / `FREEZE` / `+` / `-` / `Step` | Writing | IMP-87 | `Automation.md` |
| BSSOL-23 | Mod curve canvas - per-note 0-1 phase, NOT song position. A BaySickSolstice mod curve and a Builder automation clip are different domains | Writing | IMP-87 | `Automation.md` |
| BSSOL-24 | Zoom / pan scroll bar under the curve - the thumb is the visible phase window | Writing | IMP-87 | `Automation.md` |
| BSSOL-25 | `DEPTH` / `LENGTH` / `TEMPO` / `SPD` / `TNS` / `SKEW` / `PW` - the curve shaping row | Writing | IMP-87 | `Automation.md` |
| BSSOL-26 | `TYPE` chicken-head in `UNISON` - `Pure` / `Random` / `Drifting` / `Alt-only` | Instruments | IMP-84 | `BaySickSolstice.md` |
| BSSOL-27 | `Swing Mix` knob on the title bar, right of `Menu` - scales the global Swing for this player; right-click offers `Truncate Swing Notes` | Tabs | IMP-65 | `Patterns and Arrangement.md` |
### `BSSOLM` - `BaySickSynth-BaySickPlayer-BaySickSolstice Menu.png`

The BaySickSolstice tab's menu - the same tab menu every Layers-family engine shows
(one shared picture; the menu belongs to the TAB, not the engine). BaySickSolstice
had no menu entry of its own until 2026-08-16. `Polyphony` reads `(n/a)` and
greys on a BaySickSolstice layer - BaySickSolstice is always polyphonic.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSSOLM-1 | `Player` - the engine view, tick when shown | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSSOLM-2 | `Piano Roll` - jumps the roll to this tab | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSSOLM-3 | `FX Rack` - *see FXI* | Mixing | IMP-62 | `Effect Racks.md` |
| BSSOLM-4 | `Freeze` - renders this tab and plays the render instead. Greys when unavailable, with the reason in its tooltip; label color signals state - cyan while frozen, orange once the render goes stale | Tabs | IMP-75 | `Freeze and Export.md` |
| BSSOLM-5 | `Lock Layer` - tick when locked; a locked tab shows `[L] ` on its ribbon slot | Tabs | IMP-70 | `Workspace and Windows.md` |
| BSSOLM-6 | `Polyphony: (n/a)` - greyed on BaySickSolstice; the engine is always polyphonic | Tabs | IMP-85 | `Workspace and Windows.md` |
| BSSOLM-7 | `Rename...` | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSSOLM-8 | `Replace Engine` submenu - swap this tab's engine in place; the notes, mixer settings, effects and window all stay. Tick = the current engine; greys while the tab is locked | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSSOLM-9 | `Duplicate Layer (new tab)` - greys with no engine loaded | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSSOLM-10 | `Choke Group` submenu - `None` / `Group 1` through `Group 16`; same-group tabs cut each other off | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSSOLM-11 | `Save Current Patch As...` | Tabs | IMP-72 | `Presets.md` |
| BSSOLM-12 | `Load Preset` submenu - the engine's preset tree; `(no presets installed)` when empty | Tabs | IMP-72 | `Presets.md` |
| BSSOLM-13 | `Save Page Preset As...` | Tabs | IMP-72 | `Presets.md` |
| BSSOLM-14 | `Load Page Preset` submenu | Tabs | IMP-72 | `Presets.md` |
| BSSOLM-15 | `Delete Layer` | Tabs | IMP-72 | `Workspace and Windows.md` |

### `BSP` - `BaySickPlayer.png`

The sample player. Also what a Clips tab is, so `Clips Page.md` documents the
tab lifecycle and points here for the controls.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSP-1 | Playback - `SMPL START` / `STRETCH` / `REVERSE` | Instruments | IMP-88 | `BaySickPlayer.md` |
| BSP-2 | Note cutting and voices - `CUT SELF` / `SAME P...` / `VOICE CAP` | Instruments | IMP-85 | `BaySickPlayer.md` |
| BSP-3 | Tuning and thickness - the `PITCH & VOICING` column | Instruments | IMP-84 | `BaySickPlayer.md` |
| BSP-4 | Velocity - the `DYNAMICS` column | Instruments | IMP-88 | `BaySickPlayer.md` |
| BSP-5 | `AMP ENVELOPE` and `VIBRATO` | Instruments | IMP-81 | `BaySickPlayer.md` |
| BSP-6 | `FILTER` and `OUTPUT` | Instruments | IMP-82 | `BaySickPlayer.md` |
| BSP-7 | `Preset` button on the window title bar - the patch picker | Instruments | IMP-72 | `Presets.md` |
| BSP-8 | `Swing Mix` knob on the title bar, right of `Menu` - scales the global Swing for this player; right-click offers `Truncate Swing Notes` | Tabs | IMP-65 | `Patterns and Arrangement.md` |
### `BSSBOSC` - `BaySickSynth OSC.png`

BaySickSynth is ONE window with six panel tabs, so the frame - strip, display,
tab row - is taught here and the other five panels cross-reference it.
**BaySickBass is the same editor**; `BaySickBass.md` documents where its ranges
and defaults differ, and it mints no callouts of its own.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSSBOSC-1 | Display - draws whatever the selected panel is about: the waveform on OSC, the envelope shape on the two ENV panels, the filter response on FILTER, the LFO shape on LFO | Instruments | IMP-56 | `BaySickSynth.md` |
| BSSBOSC-2 | Panel tab row - `OSC` / `OSC ENV` / `FILTER` / `FLT ENV` / `LFO` / `MOD`. The active tab is green. These are TABS, unlike the Advanced MODE on an effect panel | Instruments | IMP-57 | `BaySickSynth.md` |
| BSSBOSC-3 | `WAVEFORM` picker - `SAW` | Instruments | IMP-86 | `BaySickSynth.md` |
| BSSBOSC-4 | Tuning-mode picker - `Musical` | Instruments | IMP-86 | `BaySickSynth.md` |
| BSSBOSC-5 | `TRANSPOSE` | Instruments | IMP-86 | `BaySickSynth.md` |
| BSSBOSC-6 | `MODIFIER` | Instruments | IMP-86 | `BaySickSynth.md` |
| BSSBOSC-7 | `NOISE` | Instruments | IMP-86 | `BaySickSynth.md` |
| BSSBOSC-8 | `SYNC` | Instruments | IMP-86 | `BaySickSynth.md` |
| BSSBOSC-9 | `RING` | Instruments | IMP-86 | `BaySickSynth.md` |
| BSSBOSC-10 | `VOICE MODE` - `Poly` / `Mono` / `Legato`. Three buttons, not a dropdown | Instruments | IMP-85 | `BaySickSynth.md` |
| BSSBOSC-11 | `CUT SELF` | Instruments | IMP-85 | `BaySickSynth.md` |
| BSSBOSC-12 | `SAME PITCH`; the face flips to `CUT ALL` while on | Instruments | IMP-85 | `BaySickSynth.md` |
| BSSBOSC-13 | `SLIDE` | Instruments | IMP-85 | `BaySickSynth.md` |
| BSSBOSC-14 | `OUT VOL` | Instruments | IMP-86 | `BaySickSynth.md` |
| BSSBOSC-15 | `MOD WHEEL` destination - `Filter` / `LFO` | Instruments | IMP-86 | `MIDI Learn.md` |
| BSSBOSC-16 | Mod-wheel `AMOUNT` | Instruments | IMP-86 | `MIDI Learn.md` |
### `BSSBOENV` - `BaySickSynth OSC ENV.png`

Two envelopes on one panel. The frame is *see BSSBOSC*.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSSBOENV-1 | `AMP ENV` group - `ATTACK` / `DECAY` / `SUSTAIN` / `RELEASE` as vertical sliders | Instruments | IMP-81 | `BaySickSynth.md` |
| BSSBOENV-2 | Amp `VEL` - velocity depth into the amp envelope | Instruments | IMP-81 | `BaySickSynth.md` |
| BSSBOENV-3 | `PITCH ENV` group - the same four stages applied to pitch | Instruments | IMP-81 | `BaySickSynth.md` |
| BSSBOENV-4 | Pitch `AMOUNT` - how far the pitch envelope moves the note. Zero by default, so the envelope exists but does nothing until this is raised | Instruments | IMP-81 | `BaySickSynth.md` |
### `BSSBFLT` - `BaySickSynth FLT.png`


The panel below the section tabs. The section tab row along the top is taught once on the Oscillator panel (see BSSBOSC).

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSSBFLT-1 | Filter XY pad - drag the dot: X = `CUTOFF` (20 Hz to 20 kHz), Y = `RES`. The pad is the ONLY resonance control - there is no RES knob | Instruments | IMP-82 | `BaySickSynth.md` |
| BSSBFLT-2 | `TYPE` - `LP` / `HP` / `BP` / `Notch` | Instruments | IMP-82 | `BaySickSynth.md` |
| BSSBFLT-3 | `TRACKING` `KEYBOARD` - how far cutoff follows the played note | Instruments | IMP-82 | `BaySickSynth.md` |
| BSSBFLT-4 | `TRACKING` `VELOCITY` | Instruments | IMP-82 | `BaySickSynth.md` |
### `BSSBFENV` - `BaySickSynth FLT ENV.png`


The panel below the section tabs. The section tab row along the top is taught once on the Oscillator panel (see BSSBOSC).

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSSBFENV-1 | `ATTACK` / `DECAY` / `SUSTAIN` / `RELEASE` sliders - the filter envelope | Instruments | IMP-81 | `BaySickSynth.md` |
| BSSBFENV-2 | `AMOU...` / `AMOUNT` - how far the envelope moves cutoff. Zero by default | Instruments | IMP-81 | `BaySickSynth.md` |
### `BSSBLFO` - `BaySickSynth LFO.png`


The panel below the section tabs. The section tab row along the top is taught once on the Oscillator panel (see BSSBOSC).

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSSBLFO-1 | `SHAPE` - `Sine` / `Saw` / `Square` | Instruments | IMP-83 | `BaySickSynth.md` |
| BSSBLFO-2 | `RATE` knob - free rate in Hz | Instruments | IMP-83 | `BaySickSynth.md` |
| BSSBLFO-3 | Sync division picker - `1/4` | Instruments | IMP-83 | `BaySickSynth.md` |
| BSSBLFO-4 | `SYNC` - greys the `RATE` knob and enables the division picker beside it; both stay visible | Instruments | IMP-83 | `BaySickSynth.md` |
| BSSBLFO-5 | `DEST` - `Filter` / `Pitch` / `Osc M...` | Instruments | IMP-83 | `BaySickSynth.md` |
| BSSBLFO-6 | `AMOUNT` | Instruments | IMP-83 | `BaySickSynth.md` |
### `BSSBMOD` - `BaySickSynth MOD.png`


The panel below the section tabs. The section tab row along the top is taught once on the Oscillator panel (see BSSBOSC).

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSSBMOD-1 | `NO...` / noise group - `NOISE ONLY` switch and the colour picker (`White`) | Instruments | IMP-86 | `BaySickSynth.md` |
| BSSBMOD-2 | `TRANSIENT` group - `AMT` / `DUR` / `COLOUR` | Instruments | IMP-86 | `BaySickSynth.md` |
| BSSBMOD-3 | `BURST ENV` `BURST` switch | Instruments | IMP-86 | `BaySickSynth.md` |
| BSSBMOD-4 | `COUNT` / `SPACING` | Instruments | IMP-86 | `BaySickSynth.md` |
| BSSBMOD-5 | `DRI...` / `DRIFT` | Instruments | IMP-86 | `BaySickSynth.md` |
| BSSBMOD-6 | `UNISON` - `VOICES` / `DETUNE` / `SPREAD`. Voice gain follows sqrt(N), *see BSSOL-5* | Instruments | IMP-84 | `BaySickSynth.md` |
### `BSGTR` - `BaySickGuitars.png`

An sfizz Inst tab. The control surface is drawn from the loaded instrument, so
what is on screen depends on which guitar is loaded - the callouts below are the
frame plus the Green Keyswitch surface.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSGTR-1 | Loaded-instrument field - `Green Keyswitch` | Instruments | IMP-89 | `BaySickGuitars.md` |
| BSGTR-2 | `Load Guitar` - the instrument picker. This engine is PROCESSOR-owned with its own race-safe load path, not rig-owned like the dynamic tabs | Instruments | IMP-89 | `BaySickGuitars.md` |
| BSGTR-3 | `CUT SELF` | Instruments | IMP-85 | `BaySickGuitars.md` |
| BSGTR-4 | `SAME PITCH`; the face flips to `CUT ALL` while on | Instruments | IMP-85 | `BaySickGuitars.md` |
| BSGTR-5 | `Unison` / `Width` / `Detune` knobs | Instruments | IMP-84 | `BaySickGuitars.md` |
| BSGTR-6 | `Releases` slider | Instruments | IMP-89 | `BaySickGuitars.md` |
| BSGTR-7 | `VIBRATO` group - `Guitar`, `Violin`, `Tailpiece`, `Speed`, `Fade`, `Humanize` sliders | Instruments | IMP-89 | `BaySickGuitars.md` |
| BSGTR-8 | `PERFORMANCE` group - `TP bends` knob, `Mute` and `Feedback` sliders | Instruments | IMP-89 | `BaySickGuitars.md` |
| BSGTR-9 | `Swing Mix` knob on the title bar, right of `Menu` - scales the global Swing for this player; right-click offers `Truncate Swing Notes` | Tabs | IMP-65 | `Patterns and Arrangement.md` |
### `BSBAS` - `BaySickBasses.png`

The sibling sfizz Inst tab. **A different engine from BaySickGuitars, not a variant** (see BSGTR) -
different instruments, different surface, its own load path.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSBAS-1 | Loaded-instrument field - `Darkblack Keysw` | Instruments | IMP-89 | `BaySickBasses.md` |
| BSBAS-2 | `Load Bass` | Instruments | IMP-89 | `BaySickBasses.md` |
| BSBAS-3 | `Mono` / `Preroll` | Instruments | IMP-85 | `BaySickBasses.md` |
| BSBAS-4 | `Swell` / `Release` | Instruments | IMP-89 | `BaySickBasses.md` |
| BSBAS-5 | `Unison` / `Width` | Instruments | IMP-84 | `BaySickBasses.md` |
| BSBAS-6 | `VIBRATO` group - `Depth`, `Tremolo`, `Rate` sliders | Instruments | IMP-89 | `BaySickBasses.md` |
| BSBAS-7 | `Delay` / `Fade` / `Humanize` knobs | Instruments | IMP-89 | `BaySickBasses.md` |
| BSBAS-8 | `FILTER` group - `Cutoff` slider, `Reso` / `Veltrack` / `Wobble` / `Attack` / `Decay` / `Depth` knobs | Instruments | IMP-82 | `BaySickBasses.md` |
| BSBAS-9 | `CUT SELF` - a new note stops the one before it, *see BSSBOSC-11* | Instruments | IMP-85 | `BaySickBasses.md` |
| BSBAS-10 | `SAME PITCH` - cut only when it is the same note repeating, *see BSSBOSC-12*; the face flips to `CUT ALL` while on | Instruments | IMP-85 | `BaySickBasses.md` |
| BSBAS-11 | `Swing Mix` knob on the title bar, right of `Menu` - scales the global Swing for this player; right-click offers `Truncate Swing Notes` | Tabs | IMP-65 | `Patterns and Arrangement.md` |
### `BSRDMAIN` - `BaySickRustyDrums Main.png`

Rusty is ONE window with a section tab row. The frame and the shared per-section
controls are taught here; the six section panels number only what is theirs.

The section panels are ARIA control surfaces and their headings are the Aria
panel names (`HI-HAT`, `NOISES AND CLICKS`), which are not always what the tab
button abbreviates them to.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSRDMAIN-1 | `Presets` - saves and loads a **Player Preset**: the kit sound only, every kit control value plus the engine output level, tagged with the program it was saved on. Different from the page preset on the `Menu` | Content | IMP-72 | `BaySickRustyDrums.md` |
| BSRDMAIN-2 | Program picker - `Full` / `Basic`. Which Aria program is loaded; the section tabs follow it | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDMAIN-3 | Section tab row - `Main` / `Kick` / `Snare` / `Toms` / `Hi-hat` / `Cymbals` / `Noises`. Auto-discovered from the program zoom-in files, so a kit without them shows `Main` alone | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDMAIN-4 | Main-page `KICK` group - `Kick` / `OH` / `Punch`, with `Tune` / `Dirt` / `Dead` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDMAIN-5 | Main-page `SNARE` group - `Btm` / `Top` / `OH` / `Snap` / `Punch` / `Epic` / `Length` with the `Sticks` and `Reg Stir` pickers, `Tail`, `Crossfade` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDMAIN-6 | Main-page tom columns - `22 IN` / `18 IN` / `15 IN` / `14 IN`, each with its own `Close` / `OH` / `Pan` / `Tune` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDMAIN-7 | Main-page `HI-HAT` group with its `Full o` position picker and `Dry` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDMAIN-8 | Main-page cymbal columns - `CRASH` / `RIDE` / `CHINA` / `SIZ RIDE` / `SIZ CRASH` / `STACK` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDMAIN-9 | Main-page `PERC` and `MECH` groups | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDMAIN-10 | `Swing Mix` knob on the title bar, right of `Menu` - scales the global Swing for this player; right-click offers `Truncate Swing Notes` | Tabs | IMP-65 | `Patterns and Arrangement.md` |
### `BSRDKICK` - `BaySickRustyDrums Kick.png`

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSRDKICK-1 | `NORMAL VOLUMES` - `Kick out` / `Overhead` / `Dirt` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDKICK-2 | `TRANSPOSED` - `Punch` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDKICK-3 | `Tune` / `Deaden` / `Snare tune` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
### `BSRDSNARE` - `BaySickRustyDrums Snare.png`

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSRDSNARE-1 | `NORMAL VOLUMES` - `Bottom` / `Top` / `Overhead` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDSNARE-2 | `TRANSPOSED` - `Snap` / `Punch` / `Epic` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDSNARE-3 | `BRUSH STIRS` - `Length` / `Tail` / `Crossfade` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDSNARE-4 | `Select` picker - `Sticks regular` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDSNARE-5 | Stir picker - `Regular Stir` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDSNARE-6 | `Pan` / `Tune` / `Deaden` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
### `BSRDTOM` - `BaySickRustyDrums Toms.png`

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSRDTOM-1 | A tom column - `22 IN` / `18 IN` / `15 IN` / `14 IN`, each with `Close` / `OH` / `Pan` / `Tune`. Four columns, one control set | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDTOM-2 | `EXTRAS` - `Lo Punch` / `Hi Punch` / `Dirt` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDTOM-3 | `BRUSH STIRS` - `Length` / `Tail` / `Crossfade` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDTOM-4 | `Sn pan` / `Sn tune` / `Deaden` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDTOM-5 | Stir-style picker under `BRUSH STIRS` - `Smooth Stir` / `Regular Stir` / `Peaky Stir` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
### `BSRDHAT` - `BaySickRustyDrums Hi-Hat.png`

Panel heading `HI-HAT`; the tab button abbreviates it to `Hi-hat`.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSRDHAT-1 | `VOLUMES` - `Close` / `OH` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDHAT-2 | `Select` picker - `Sticks` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDHAT-3 | `Position` picker - `Fully open` and the other openings | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDHAT-4 | `Pan` / `Tune` / `Dryness` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
### `BSRDCYMB` - `BaySickRustyDrums Cymbals.png`

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSRDCYMB-1 | A cymbal column - `CRASH` / `RIDE` / `CHINA` / `SIZ RIDE` / `SIZ CRASH` / `STACK`, each with `Close` / `OH` / `Pan`. Six columns, one control set | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDCYMB-2 | `Select` picker - `Sticks` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDCYMB-3 | `Tune` / `Dryness` - one pair for the whole section, not per cymbal | Instruments | IMP-89 | `BaySickRustyDrums.md` |
### `BSRDNOISE` - `BaySickRustyDrums Noises and Clicks.png`

Panel heading `NOISES AND CLICKS`; the tab button abbreviates it to `Noises`.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSRDNOISE-1 | `MECH NOIS...` group - `Close` / `Overhead` with `Tune` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDNOISE-2 | `PERCUSSION` group - `Close` / `Overhead` / `Snare btm` with `Tune` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDNOISE-3 | `PAN` group - one pan per source: `Snare` / `Hi-hat` / `22 in` / `18 in` / `15 in` / `14 in` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
### `BSRDKIT` - `BaySickRustyDrums Drum Kit.png`

The Rusty window's Drum Kit view - the kit photograph with live hit targets,
opened from the window Menu's `Drum Kit` entry (see BSRDMENU). The title bar
is the family bar figure (see BSRDTTL); this view has no section tab row.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSRDKIT-1 | The kit photograph - every drum and cymbal is a click target; clicking plays that piece. A thin blue outline marks the piece while pressed, and hovering names it in the tooltip | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDKIT-2 | Hi-hat pedal indicator - always on: a green ring with `PEDAL: OPEN`, red with `PEDAL: CLOSED`, following the engine's pedal state | Instruments | IMP-89 | `BaySickRustyDrums.md` |
| BSRDKIT-3 | `Pick a program to begin` - the overlay until a kit program is loaded | Instruments | IMP-89 | `BaySickRustyDrums.md` |
### `BSRDMAP` - `Rusty Keys.png`

The note-to-drum map, opened from `Rusty Drums Map...` on the Rusty window's
own Menu. Its own desktop window, not a section panel.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSRDMAP-1 | The map itself - which MIDI note fires which articulation. Columns: `Key` (note name) / `MIDI` (raw number) / `Sound` / `Articulation` | Instruments | IMP-89 | `BaySickRustyDrums.md` |
### `BSV` - `BaySickVocals.png`

The Vox PAGE window - the tab itself, not a satellite. Its four satellites
(Vocal Chain, BaySickPitch, BaySickAlign, NAM/IR) open from its `Menu`. Its `Menu` is the route to the four editor windows: Vocal Chain, BaySickPitch, BaySickAlign and NAM/IR.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSV-1 | `Mix` knob - the page wet/dry | Tabs | IMP-38 | `Vox Page.md` |
| BSV-2 | A/B snapshot picker - `A`. The Vox page own pair, independent of the NAM/IR A/B and the EQ A/B | Tabs | IMP-72 | `Vox Page.md` |
| BSV-3 | `Realtime Pitch OFF` - the on/off button, reading its CURRENT state | Tabs | IMP-38 | `Vox Page.md` |
| BSV-4 | `Root` picker - `C` | Tabs | IMP-38 | `Vox Page.md` |
| BSV-5 | `Scale` picker - `Chromatic` | Tabs | IMP-38 | `Vox Page.md` |
| BSV-6 | `Retune ms` | Tabs | IMP-38 | `Vox Page.md` |
| BSV-7 | `Strength` | Tabs | IMP-38 | `Vox Page.md` |
| BSV-8 | `Humanize` | Tabs | IMP-38 | `Vox Page.md` |
| BSV-9 | `Throat` | Tabs | IMP-38 | `Vox Page.md` |
| BSV-10 | `Formant Preserve` | Tabs | IMP-38 | `Vox Page.md` |
| BSV-11 | Live readout - `Detected: -- Hz   Target: -- Hz   Shift: -- cents`. Dashes until the tracker has a pitch | Tabs | IMP-45 | `Vox Page.md` |
### `BSVC` - `Vocal Chain.png`

Six effect slots in a fixed order, each drawn as an embedded effect panel. The
panel anatomy is *see FX*; only what is chain-specific is numbered.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSVC-1 | A chain slot - six, fixed order, always present. Unlike a rack slot these cannot be added, removed or reordered | Vocal | IMP-62 | `Vocal Chain.md` |
| BSVC-2 | Bypass dot - green = stage active, red = bypassed. CLICK toggles it | Vocal | IMP-62 | `Vocal Chain.md` |
| BSVC-3 | Slot name - `Gate` / `De-reverb` / `De-esser` / `Compressor` / `Saturation` / `Limiter` | Vocal | IMP-3 | `Vocal Chain.md` |
| BSVC-4 | `Basic` - the Advanced/Basic MODE toggle, on the slots that have one | Vocal | IMP-62 | `Effect Modules.md` |
| BSVC-5 | `Preset` - per-slot preset picker | Content | IMP-72 | `Presets.md` |
| BSVC-6 | Slot mode button - `Modern` on the Compressor, `Console` on Saturation, `Limiter` on the Limiter. **The Compressor here offers three modes, not four**: Pedal is a pedal sustainer and is not a vocal-chain compressor | Vocal | IMP-1 | `Vocal Chain.md` |
| BSVC-7 | `SC: Off` - sidechain pick on the slots that support it | Vocal | IMP-53 | `Vocal Chain.md` |
| BSVC-8 | Gain-reduction meter - the analog face at a slot left, on the stages that reduce gain | Vocal | IMP-1 | `Vocal Chain.md` |
| BSVC-9 | Slot `Vol` and its dBFS meter - *see FX-6* and *see FX-7* | Vocal | IMP-48 | `Vocal Chain.md` |
### `BSPIT` - `BaySickPitch.png`

The offline pitch editor. Works on the channel audio clips; it is not the
realtime corrector, which lives on the Vox page, *see BSV-3*.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSPIT-1 | `Save` / `Load` - the editor own analysis state | Vocal | IMP-39 | `Pitch Editor.md` |
| BSPIT-2 | `Slice` tool | Vocal | IMP-39 | `Pitch Editor.md` |
| BSPIT-3 | `Edit` tool | Vocal | IMP-39 | `Pitch Editor.md` |
| BSPIT-4 | `Send Notes to...` - pushes the detected pitches out as MIDI notes | Vocal | IMP-46 | `Pitch Editor.md` |
| BSPIT-5 | `ON` checkbox - the stage enable | Vocal | IMP-39 | `Pitch Editor.md` |
| BSPIT-6 | `ANALYSIS FAILED` - the analysis status readout. Reads a failure reason where there is one | Vocal | IMP-39 | `Pitch Editor.md` |
| BSPIT-7 | `Root` picker - `C` | Vocal | IMP-39 | `Pitch Editor.md` |
| BSPIT-8 | `Scale` picker - `Chromatic` | Vocal | IMP-39 | `Pitch Editor.md` |
| BSPIT-9 | `Snap` | Vocal | IMP-39 | `Pitch Editor.md` |
| BSPIT-10 | Engine picker - `Rubber Band - Balanced` and the other shifters | Vocal | IMP-43 | `Pitch Editor.md` |
| BSPIT-11 | `Snapshot` | Vocal | IMP-39 | `Pitch Editor.md` |
| BSPIT-12 | `Versions` | Vocal | IMP-39 | `Pitch Editor.md` |
| BSPIT-13 | `Reset` | Vocal | IMP-39 | `Pitch Editor.md` |
| BSPIT-14 | `Render` - commits the edit to audio | Vocal | IMP-76 | `Pitch Editor.md` |
| BSPIT-15 | `Undo` / `Redo` | Vocal | IMP-69 | `Undo History.md` |
| BSPIT-16 | `A` - auto-scroll: the canvas follows playback. Keyboard `A` toggles it too | Vocal | IMP-72 | `Pitch Editor.md` |
| BSPIT-17 | `Focus` / `Mod` / `Speed` / `Throat` knobs with their readouts | Vocal | IMP-39 | `Pitch Editor.md` |
| BSPIT-18 | Keyboard and octave labels down the left, `C2`..`C6` | Vocal | IMP-39 | `Pitch Editor.md` |
| BSPIT-19 | Bar ruler | Vocal | IMP-65 | `Pitch Editor.md` |
| BSPIT-20 | Pitch canvas - the detected curve and its editable notes. `This channel has no audio clips to analyze.` is the empty state | Vocal | IMP-39 | `Pitch Editor.md` |

| BSPIT-21 | Info bar along the bottom - `Pitch:` note, `Cents:`, `Length:`, plus `[detached]` / `[moved]` / `[edited]` flags on edited notes | Vocal | IMP-39 | `Pitch Editor.md` |
| BSPIT-22 | Selection readout left of `ON` - `SEL <n> bars / M:SS.f` | Vocal | IMP-39 | `Pitch Editor.md` |
| BSPIT-23 | Canvas scrollbars - both axes, always drawn | Vocal | IMP-39 | `Pitch Editor.md` |
### `BSA` - `BaySickAlign.png`

Time-aligns a follower take to a leader take.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSA-1 | Preset picker - `Close-Align` | Content | IMP-72 | `Align Editor.md` |
| BSA-2 | `Save` / `Load` | Vocal | IMP-40 | `Align Editor.md` |
| BSA-3 | `Analyze/Apply` | Vocal | IMP-40 | `Align Editor.md` |
| BSA-4 | `Versions` | Vocal | IMP-40 | `Align Editor.md` |
| BSA-5 | `Render` | Vocal | IMP-76 | `Align Editor.md` |
| BSA-6 | `Undo` / `Redo` | Vocal | IMP-69 | `Undo History.md` |
| BSA-7 | Time ruler in minutes and seconds - this editor works in TIME, not bars | Vocal | IMP-40 | `Align Editor.md` |
| BSA-8 | `LEADER` lane - the reference take. `Pick Leader...` chooses it. Empty state: `No clips on this channel yet` (all three lanes paint it) | Vocal | IMP-40 | `Align Editor.md` |
| BSA-9 | `SYNC POINTS` strip - looks like a divider, is where you CLICK and DRAG to correct the matching by hand | Vocal | IMP-40 | `Align Editor.md` |
| BSA-10 | `FOLLOWER` lane - `(this page)`, the take being moved | Vocal | IMP-40 | `Align Editor.md` |
| BSA-11 | `PROTECTED` strip - looks like a divider, is where you DRAG to mark a stretch as leave-alone | Vocal | IMP-40 | `Align Editor.md` |
| BSA-12 | `OUTPUT` lane - `Analyze to preview the aligned output` is its empty state | Vocal | IMP-40 | `Align Editor.md` |
| BSA-13 | `ALIGN` box - `ON` checkbox, mode picker (`Close`), `Fine Tune` and `Max Shift` with their readouts (`100 ms`, `No Limit`) | Vocal | IMP-40 | `Align Editor.md` |
| BSA-14 | `PITCH` box - `ON`, engine picker (`Rubber Band - Balanced`), `Leader Type` / `Follower Type`, `Blend` / `Variation` / `Transpose` / `Formant Shift` with readouts, and `Formant` | Vocal | IMP-43 | `Align Editor.md` |
| BSA-15 | `Wave` / `Pitch` / `Energy` view buttons | Vocal | IMP-40 | `Align Editor.md` |
| BSA-16 | `-` / `+` zoom | Vocal | IMP-40 | `Align Editor.md` |


| BSA-17 | Green dot beside the preset picker - unsaved preset edits | Vocal | IMP-40 | `Align Editor.md` |
| BSA-18 | Status badge right of Save / Load - `ANALYZING...`, `RE-ANALYZE`, `RE-ANALYZE ON STOP` | Vocal | IMP-40 | `Align Editor.md` |
### `BSNAM` - `BaySickNAMIR.png`

Amp head, cabinet IR, and two independent virtual microphones. Mic A and Mic B
are the SAME control set, so each control is one callout with the Mic B twin
noted rather than numbered again.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSNAM-1 | `AMP` row - `Load .nam...` and the model field, `(no model loaded)` when empty. A missing file shows its name plus ` (missing)` in red | Mixing | IMP-37 | `NAM Amp and Cab.md` |
| BSNAM-2 | Amp bypass - the `OFF` toggle right of the AMP row | Mixing | IMP-37 | `NAM Amp and Cab.md` |
| BSNAM-3 | `CAB` row - `Load .wav IR...` and the IR field, `(no IR loaded)` when empty. Same missing-file state as the model field | Mixing | IMP-37 | `NAM Amp and Cab.md` |
| BSNAM-4 | Cab bypass | Mixing | IMP-37 | `NAM Amp and Cab.md` |
| BSNAM-5 | `Input Gain` | Mixing | IMP-37 | `NAM Amp and Cab.md` |
| BSNAM-6 | `Gate Thr` / `Gate Rel` | Mixing | IMP-3 | `NAM Amp and Cab.md` |
| BSNAM-7 | `Low Cut` / `High Cut` - pre-cab | Mixing | IMP-37 | `NAM Amp and Cab.md` |
| BSNAM-8 | `Cab Mix` | Mixing | IMP-37 | `NAM Amp and Cab.md` |
| BSNAM-9 | `Output` | Mixing | IMP-37 | `NAM Amp and Cab.md` |
| BSNAM-10 | `OS` chicken-head - oversampling, 1x / 2x / 4x | Mixing | IMP-51 | `NAM Amp and Cab.md` |
| BSNAM-11 | `A` / `B` slot buttons - this engine own A/B pair, independent of the Vox page one | Mixing | IMP-72 | `NAM Amp and Cab.md` |
| BSNAM-12 | `MIC SIM A` heading. `MIC SIM B` is the same column for the second mic | Mixing | IMP-35 | `NAM Amp and Cab.md` |
| BSNAM-13 | Mic Active switch, `OFF` / `ON` - one per mic, both default OFF. Switching a mic crossfades over 15 ms rather than stepping, so it is safe to automate. With both off the cabinet reaches the output with no mic model on it at all | Mixing | IMP-36 | `NAM Amp and Cab.md` |
| BSNAM-14 | `Mode` dropdown - `Built-in` / `User IR`. TWO entries: whether there is a mic at all is the switch job, not a Mode | Mixing | IMP-35 | `NAM Amp and Cab.md` |
| BSNAM-15 | `Model` dropdown - the ten built-in archetypes. Shown in Built-in mode only | Mixing | IMP-35 | `NAM Amp and Cab.md` |
| BSNAM-16 | User-IR path label - shares the Model dropdown's rectangle; the `Load Mic IR...` button is its own row below it | Mixing | IMP-35 | `NAM Amp and Cab.md` |
| BSNAM-17 | Mic Sim `Mix` | Mixing | IMP-35 | `NAM Amp and Cab.md` |
| BSNAM-18 | `MIC PLACEMENT A` heading. `MIC PLACEMENT B` is its twin | Mixing | IMP-36 | `NAM Amp and Cab.md` |
| BSNAM-19 | `Top` / `Side` view button - ONE PER MIC, so the two mics can be looked at independently | Mixing | IMP-36 | `NAM Amp and Cab.md` |
| BSNAM-20 | `Polar` chicken-head - `O` / `Card` / `Sup` / `Hyp` / `8` = Omni / Cardioid / Supercardioid / Hypercardioid / Figure-8 | Mixing | IMP-36 | `NAM Amp and Cab.md` |
| BSNAM-21 | `Distance` - 1 to 150 cm | Mixing | IMP-36 | `NAM Amp and Cab.md` |
| BSNAM-22 | `Angle` - -90 to +90 degrees, left and right off the speaker axis | Mixing | IMP-36 | `NAM Amp and Cab.md` |
| BSNAM-23 | `Height` - -30 to +30 cm above or below the cone centre. Combines with Angle into the true off-axis angle AND lengthens the path, so raising a mic darkens it and backs it off at once | Mixing | IMP-36 | `NAM Amp and Cab.md` |
| BSNAM-24 | Placement `Mix` | Mixing | IMP-36 | `NAM Amp and Cab.md` |
| BSNAM-25 | Mic picture - draggable. In TOP the drag sets Distance and Angle; in SIDE it sets Height and Angle and Distance stays on its knob. Double-click returns the mic to 30 cm, on axis, at cone height | Mixing | IMP-36 | `NAM Amp and Cab.md` |
| BSNAM-26 | Distance rings (TOP) / 10 cm height rings (SIDE) | Mixing | IMP-36 | `NAM Amp and Cab.md` |
| BSNAM-27 | On-axis bright zone - inside 15 degrees the model applies NO off-axis darkening at all | Mixing | IMP-36 | `NAM Amp and Cab.md` |
| BSNAM-28 | Proximity boundary - the 20 cm mark. In SIDE it closes up as the mic backs off and disappears past 20 cm, because proximity follows the TRUE distance and not the height alone | Mixing | IMP-36 | `NAM Amp and Cab.md` |
| BSNAM-29 | Placement readout - `30 cm   0 deg`, gaining a third field in SIDE | Mixing | IMP-36 | `NAM Amp and Cab.md` |
### `BSPDL` - `BaySickPedals.png`

The pedalboard. Eight slots; each holds one pedal-native effect. Its `Menu` is the route to NAM/IR - the amp and cabinet the pedals feed into - and the Piano Roll.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSPDL-1 | `View` - swaps the compact board for the full board. Two window sizes, 331x331 and 1038x554 | Mixing | IMP-59 | `Pedalboard.md` |
| BSPDL-2 | `Preset` picker - the whole board | Mixing | IMP-72 | `Presets.md` |
| BSPDL-3 | An empty slot - dashed outline with a `+`, on the six FREE positions (2-7). Clicking it opens the picker, *see BSPDLP* | Mixing | IMP-17 | `Pedalboard.md` |
| BSPDL-4 | A filled slot - solid outline, the pedal name at the top left | Mixing | IMP-17 | `Pedalboard.md` |
| BSPDL-5 | `...` - the pedal's PRESET menu: `Save Preset As...` / `Factory Presets` / `My Presets` / `Restore Defaults` / `Save as Default` / `Reveal Folder...`. Changing the PEDAL is right-click on the tile | Mixing | IMP-17 | `Pedalboard.md` |
| BSPDL-6 | EQ type dropdown - the LAST slot only. Three EQs to choose between; the slot is always an EQ | Mixing | IMP-25 | `Pedalboard.md` |
| BSPDL-7 | Slot `ON` footswitch - engages that pedal. Green when on | Mixing | IMP-17 | `Pedalboard.md` |
| BSPDL-8 | Tuner display - scrolling strobe stripes or a 21-cell LED bar (style is switchable), plus note name / cents / Hz text | Mixing | IMP-23 | `Pedalboard.md` |
| BSPDL-9 | Tuner chicken-heads - detection `Ch` / `Gt` / `Bs` (chromatic / guitar / bass), display style `St` / `LB` (strobe / LED bar), and flat/drop tuning `0` to `6` semitones below standard; `432` is a toggle for the A=432 reference | Mixing | IMP-23 | `Pedalboard.md` |
| BSPDL-10 | The EQ's own controls - what is here changes with the type picked above, so the knobs in the picture are one of three possible sets | Mixing | IMP-25 | `Pedalboard.md` |
| BSPDL-11 | Per-slot `X` - removes that pedal; populated slots only | Instruments | IMP-57 | `Pedalboard.md` |
| BSPDL-12 | Per-slot reorder arrows - filled triangles, greyed at the chain ends | Instruments | IMP-57 | `Pedalboard.md` |
### `BSPDLP` - `BaySickPedals List.png`

The pedal picker. Its groups are NOT the same as the rack picker groups, *see
FXPICK*: this one leads with the user NAM pedal and carries pedal-only models.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSPDLP-1 | `User NAM Pedal` group - `Load NAM Pedal`, a user-supplied capture rather than a modelled type | Mixing | IMP-28 | `Pedalboard.md` |
| BSPDLP-2 | `Dynamics` group - `Bass Compressor`, `Compressor`, `Noise Gate` | Mixing | IMP-31 | `Pedalboard.md` |
| BSPDLP-3 | `Harmonics` group - `Bass Driver`, `Bass Overdrive`, `Blues Drive`, `Distortion`, `Fuzz`, `High-Gain`, `Octave`, `Overdrive`, `Saturation` | Mixing | IMP-17 | `Pedalboard.md` |
| BSPDLP-4 | `Modulation` group - `Acoustic Simulator`, `Chorus`, `Flanger`, `Phaser`, `Polyphonic Synth`, `Wah` | Mixing | IMP-27 | `Pedalboard.md` |
| BSPDLP-5 | `Time` group - `Acoustic Preamp`, `Delay`, `Reverb` | Mixing | IMP-29 | `Pedalboard.md` |
| BSPDLP-6 | `Clear` - empties the slot. Greyed while the slot is already empty | Mixing | IMP-17 | `Pedalboard.md` |


### `ANLZ` - `Analyzer.png`

The Master Analyzer. Loudness measurement on the master output, plus captured
takes. Opened from the Mixer, *see MIX-18*.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| ANLZ-1 | Source picker - `Live`, or one of the captured takes (`(report)` marks a take loaded from a report) | Mixing | IMP-48 | `Mixer.md` |
| ANLZ-2 | `vs ...` - overlay a second take on the Loudness view for A/B | Mixing | IMP-48 | `Mixer.md` |
| ANLZ-3 | `Export Take...` - writes the selected take as its own report (plus its audio when captured). Greyed on Live | Mixing | IMP-76 | `Freeze and Export.md` |
| ANLZ-4 | `Remove Take` - drops the selected take from the list and from its session report | Mixing | IMP-76 | `Freeze and Export.md` |
| ANLZ-5 | View bar - `Levels` / `Loudness` / `Spectrum`, the current one lit; `Tilt` and `1/3 oct` appear on Spectrum; `Reset` clears everything that accumulates | Mixing | IMP-48 | `Mixer.md` |
| ANLZ-6 | Levels view - big INTEGRATED graded against the target (green within 1 LU, amber within 3, red beyond) with the offset in LU; `M` and `S` bars centered on the target; TRUE PEAK per channel with the ceiling marked and the running max, the count of moments over the ceiling, PLR / PSR; CORR filling right for correlated, left (red) for out-of-phase | Mixing | IMP-48 | `Mixer.md` |
| ANLZ-7 | Loudness view - short-term as a filled area, momentary as a line, the dashed green target, the loudness-range band shaded amber; wheel zooms, drag pans, double-click shows the whole history, hover reads time / S / M. Side cells INTEGRATED, SHORT-TERM, MOMENTARY, LRA, MAX TP L / R | Mixing | IMP-48 | `Mixer.md` |
| ANLZ-8 | Spectrum view - log-frequency, per-column averaged trace over a grey peak hold, MAX TP L / R in the corner, dBFS bars at the side | Mixing | IMP-49 | `Mixer.md` |
| ANLZ-9 | `Tilt` - 0, 3 or 4.5 dB per octave slope (pink noise reads flat at 3) | Mixing | IMP-49 | `Mixer.md` |
| ANLZ-10 | `1/3 oct` - third-octave bars instead of the line | Mixing | IMP-49 | `Mixer.md` |
### `ANLZM` - `Analyzer Menu.png`

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| ANLZM-1 | `View` group - `Levels` / `Loudness` / `Spectrum`, ticked | Mixing | IMP-48 | `Mixer.md` |
| ANLZM-2 | `Source` group - `Live`, then the captured takes. `No captured takes yet` is the greyed empty state | Mixing | IMP-48 | `Mixer.md` |
| ANLZM-3 | `Target` group - `Streaming (-14 LUFS)`, `Streaming (-16 LUFS)`, `EBU R128 (-23 LUFS)`, `ATSC A/85 (-24 LKFS)`. Sets the target AND the true-peak ceiling the views grade against | Mixing | IMP-34 | `Mixer.md` |
| ANLZM-4 | `Custom...` - a target of your own. While a custom target is active the row reads `Custom (-X.X LUFS)...` and carries the tick | Mixing | IMP-34 | `Mixer.md` |
| ANLZM-5 | `Reset history` - clears the graph and the integrated reading | Mixing | IMP-48 | `Mixer.md` |

### `EVT` - `Event Editor.png`

The automation-clip editor, F12. A DESKTOP window with its own menu bar, not a
contained window - so it has no `Menu` button and no workspace chrome.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| EVT-1 | `File` menu | Writing | IMP-68 | `Event Editor.md` |
| EVT-2 | `Edit` menu | Writing | IMP-69 | `Event Editor.md` |
| EVT-3 | `Tools` menu | Writing | IMP-68 | `Event Editor.md` |
| EVT-4 | `View` menu | Writing | IMP-68 | `Event Editor.md` |
| EVT-5 | `Target Control` menu - re-points the editor at a different automatable control | Writing | IMP-63 | `Automation.md` |
| EVT-6 | `Import MIDI` menu | Writing | IMP-68 | `Event Editor.md` |
| EVT-7 | Target label - `mixer master fader`, the parameter id in words | Writing | IMP-63 | `Automation.md` |
| EVT-8 | `Auto` / `LFO` - which kind of source drives the target | Writing | IMP-63 | `Automation.md` |
| EVT-9 | `New Automation Clip` | Writing | IMP-63 | `Automation.md` |
| EVT-10 | `Snap:` picker - `1/16` | Writing | IMP-68 | `Event Editor.md` |
| EVT-11 | Value readout - `0.915`, the value under the cursor | Writing | IMP-68 | `Event Editor.md` |
| EVT-12 | Curve canvas - scale 0.00 to 1.00, tick-based SONG position. Not the per-note 0-1 phase a BaySickSolstice mod curve uses, *see BSSOL-23* | Writing | IMP-63 | `Automation.md` |
| EVT-13 | A control point - the circles at each end of the line | Writing | IMP-63 | `Automation.md` |
| EVT-14 | Tool row - `D` / `P` / `E` / `I` / `S` / `Z` | Writing | IMP-79 | `Keyboard Shortcuts.md` |
| EVT-15 | Status line - `Beat 0.97   0.997`, position and value under the cursor | Writing | IMP-68 | `Event Editor.md` |
| EVT-16 | `Automation Clips` list - every clip on this target. Clicking one loads it into the canvas | Writing | IMP-63 | `Automation.md` |
### `PRMMNU` - `RightClick Knob or Slider.png`

The right-click menu on ANY automatable control. Small, and load-bearing: this
is the whole entry point for both automation and MIDI Learn. **MIDI Learn has no
window of its own** - this menu is it.
The picture shows the everyday shape; three of the rows below appear only in
the situations they name.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| PRMMNU-1 | `Automate: <target>` - names the control it will create a lane for, e.g. `Automate: Mx Master - Fader`. Opens the Event Editor on that target, *see EVT* | Writing | IMP-63 | `Automation.md` |
| PRMMNU-2 | `Type in value...` - exact numeric entry, for when a drag will not land on the number you want. Value controls only - a bare selector does not offer it | Writing | - | `Automation.md` |
| PRMMNU-3 | `MIDI Learn` - arms the control; the next controller move binds to it. Reads `MIDI Learn (no MIDI input devices)` and greys when nothing is plugged in - plug a device in and reopen the menu, hot-plug is not watched | Writing | IMP-64 | `MIDI Learn.md` |
### `UNDO` - `UndoHistory.png`

The history window. One app-wide undo stack, so this lists every action from
every page in one list.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| UNDO-1 | `>> Current` - the marker for where you are in the stack. Entries above it are done, entries below are redoable | Writing | IMP-69 | `Undo History.md` |
| UNDO-2 | A history entry - one per transaction, named by the action. Clicking one jumps the whole app to that state | Writing | IMP-69 | `Undo History.md` |
### `KEYS` - `Keybinds.png`

The Key Binds window. Every binding in the app, rebindable. A desktop window,
taller than a screen - the figure shows the General category from the top and
the manual lists the rest as text rather than as more images.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| KEYS-1 | Category tabs - `General` / `Builder Page` / `Piano Roll` / `Drum Kit` / `Vocal Editor` / `Event Editor` Key Binds. A binding lives in exactly one category and applies where that category is focused | Shell | IMP-79 | `Keyboard Shortcuts.md` |
| KEYS-2 | `Action` column - what the binding does | Shell | IMP-79 | `Keyboard Shortcuts.md` |
| KEYS-3 | `Shortcut` column - the current binding, in the app own wording (`spacebar`, `ctrl + shift + S`, `numpad 0`, `cursor up`) | Shell | IMP-79 | `Keyboard Shortcuts.md` |
| KEYS-4 | `Set` - arms the row; the next chord you press becomes the binding | Shell | IMP-79 | `Keyboard Shortcuts.md` |
| KEYS-5 | `Reset` - returns that one row to its default | Shell | IMP-79 | `Keyboard Shortcuts.md` |
### `BSPLUG` - `Hosted Plugin.png`

A Plugins tab hosting a third-party VST3 instrument. Everything inside the frame
is the PLUGIN own UI and belongs to its maker, not to us - so the callouts are
the frame and the hosting contract only.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSPLUG-1 | Window title - the TAB name (`Plugin 1`), not the plugin name. Renaming the tab renames the window | Tabs | IMP-57 | `Plugins Page.md` |
| BSPLUG-2 | `Swing Mix` knob on the title bar, right of `Menu` - scales the global Swing for this player; right-click offers `Truncate Swing Notes` | Tabs | IMP-65 | `Patterns and Arrangement.md` |
### `PLUG` - `Plugin Search.png`

`Options > Plugins`. Where third-party plugins are scanned and added. Nothing
appears in the `+` menu or the effect picker until it is added here.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| PLUG-1 | `Filter:` - `name or manufacturer` | Content | IMP-77 | `Plugins Page.md` |
| PLUG-2 | `1. Scan folders` - the folders that get searched | Content | IMP-77 | `Plugins Page.md` |
| PLUG-3 | `Add Folder...` / `Remove` / `Reset to Defaults` | Content | IMP-77 | `Plugins Page.md` |
| PLUG-4 | `2. Added plugins` table - `Name` / `Kind` / `Manufacturer` / `File`. `Kind` is `Instrument` or `Effect`, and it decides whether the plugin appears in the tab menu or the effect picker | Content | IMP-77 | `Plugins Page.md` |
| PLUG-5 | `Remove Selected` | Content | IMP-77 | `Plugins Page.md` |
| PLUG-6 | `3. Scan results` table - `Name` / `Kind` / `File / reason skipped`. The reason column is where a refused plugin explains itself | Content | IMP-77 | `Plugins Page.md` |
| PLUG-7 | `Scan` - runs the scan OUT OF PROCESS, so a plugin that crashes on load cannot take the app down with it | Content | IMP-77 | `Plugins Page.md` |
| PLUG-8 | `Add Checked` - greyed until a scan has produced something to check. Greys until at least one row is ticked; the label becomes `Add Checked (N)` | Content | IMP-77 | `Plugins Page.md` |
| PLUG-9 | Per-row checkbox - first column of the results table; what `Add Checked` adds. Skipped rows draw no box | Content | IMP-77 | `Plugins Page.md` |
### `EXP` - `Export Audio.png`

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| EXP-1 | `Selection` - `Full Arrangement` or a narrower range. `Selected Section` greys when the ruler has no time selection | Content | IMP-76 | `Freeze and Export.md` |
| EXP-2 | `Tail` - `Included` or not. Whether reverb and delay tails past the end get written | Content | IMP-76 | `Freeze and Export.md` |
| EXP-3 | `Format` - `WAV` and the alternatives | Content | IMP-55 | `Freeze and Export.md` |
| EXP-4 | `Quality` - `24-bit` | Content | IMP-76 | `Freeze and Export.md` |
| EXP-5 | `Sample rate` - `44100 Hz`. A render runs at the session rate unless this changes it. Rates above 48 kHz grey for MP3 and drop to 48000 | Content | IMP-76 | `Freeze and Export.md` |
| EXP-6 | `Dither` - `Off` | Content | IMP-76 | `Freeze and Export.md` |
| EXP-7 | `Normalize to` checkbox with its LUFS field - `-14.0` | Content | IMP-48 | `Freeze and Export.md` |
| EXP-8 | `Check against` - `Streaming (-14 LUFS)` and the other targets | Content | IMP-34 | `Freeze and Export.md` |
| EXP-9 | `Measure` - measures without exporting, through the same offline render core the export uses | Content | IMP-76 | `Freeze and Export.md` |
| EXP-10 | `Export stems (one file per mixer strip)`. On, it reveals one checkbox per mixer strip in a scrolling list - tick the stems you want | Content | IMP-76 | `Freeze and Export.md` |
| EXP-11 | `Export` / `Cancel` | Content | IMP-76 | `Freeze and Export.md` |
### `BUNDLE` - `Export Project Bundle.png`

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BUNDLE-1 | `Bundle as` - `Single .zip file` or a folder | Content | IMP-71 | `Project Bundles.md` |
| BUNDLE-2 | `Contents` - `Include my samples + outside files` and the narrower choices. This decides whether the bundle opens on a machine that has none of your library | Content | IMP-71 | `Project Bundles.md` |
| BUNDLE-3 | `Export` / `Cancel` | Content | IMP-71 | `Project Bundles.md` |
### `FILE` - `File Settings.png`

What gets written when you stop recording, and what is kept afterwards.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| FILE-1 | Take-type checkboxes - `Dry` / `Dry Cleaned` / `Wet` / `Wet Cleaned`. Written at record stop; at least one stays checked | Content | IMP-70 | `Projects and Saving.md` |
| FILE-2 | `De-noise strength:` - `Strong`. What "Cleaned" means | Content | IMP-52 | `Projects and Saving.md` |
| FILE-3 | `Auto-freeze above:` slider and its `80 %` readout - the DSP load at which tabs start freezing themselves | Content | IMP-75 | `Freeze and Export.md` |
| FILE-4 | `Keep captured takes:` - `This session only` and the longer options | Content | IMP-70 | `Projects and Saving.md` |
| FILE-5 | `Also keep the audio of each take` | Content | IMP-70 | `Projects and Saving.md` |
| FILE-6 | `Enable Instrument Level Freeze` | Content | IMP-75 | `Freeze and Export.md` |
### `AUD` - `Audio & Midi Settings.png`

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| AUD-1 | `Audio Mode:` - `Windows Audio`, ASIO and the rest | Shell | IMP-66 | `Transport and Playback.md` |
| AUD-2 | `Audio Device:` | Shell | IMP-66 | `Transport and Playback.md` |
| AUD-3 | `Sample Rate:` - `48000 Hz` | Shell | IMP-66 | `Transport and Playback.md` |
| AUD-4 | `Buffer Size:` - `480 samples`. The `DSP` perf readout is load as a percentage of THIS window, *see TABBAR-12* | Shell | IMP-80 | `Transport and Playback.md` |
| AUD-5 | `MIDI Inputs:` - one checkbox per detected input | Shell | IMP-64 | `MIDI Learn.md` |
| AUD-6 | `Trigger Velocity:` - `From controller` or a fixed value | Shell | IMP-64 | `MIDI Learn.md` |
| AUD-7 | `Open ASIO Control Panel` - enabled when the live device has a control panel or the selected type is ASIO. ASIO needs the input and output device NAMES to match or the input mask stays empty | Shell | IMP-66 | `Transport and Playback.md` |
| AUD-8 | `Apply` / `Close` - Apply takes effect on restart (the pending choice is written to a sibling settings file and consumed then); Close leaves things as they were | Shell | IMP-66 | `Transport and Playback.md` |
---

## Review note - for Jeff

Task 2 asks you to review the screen codes and spot-check three images' callout
lists. The three worth checking, because they are where the collapse rules do the
most work and so where a miss would hide:

1. **`MIX`** - every strip is one control set. If a per-strip control is missing
   from those 22 rows it is missing from the manual.
2. **`BSRDMAIN`** - it owns the controls that repeat across all seven Rusty
   sections (`Low cut`, `Crush`, `Close`/`OH`, `Pan`, `Tune`, the stick picker),
   so the six section panels only number what is genuinely theirs.
3. **`BSNAM`** - the densest screen at 32, and the one that changed most this
   week.

Two things to know rather than decide:

- **`FXI` cost nothing to allocate early.** The code was reserved before the
  figure existed, you shot it, and it slotted in with zero renumbering. That is
  the scheme doing its job.
- **Nothing here is renumberable.** Ids are append-only per screen. A new control
  on an existing screen takes the next free number on that screen; a deleted one
  gets struck through here and its number is never re-issued.

### `FMENU` - `File Menu.png`

The app `File` menu - project persistence end to end.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| FMENU-1 | `New Project...  (Ctrl+N)` - fresh empty project, behind the unsaved-changes gate | Content | IMP-70 | `Projects and Saving.md` |
| FMENU-2 | `New from Template` submenu - `New from Default Template (<name>)`, `Premade Templates` and `My Templates`. Every pick runs the same unsaved-changes gate | Content | IMP-73 | `Templates.md` |
| FMENU-3 | `Open Project...  (Ctrl+O)` | Content | IMP-70 | `Projects and Saving.md` |
| FMENU-4 | `Quick Open Project...` | Content | IMP-70 | `Projects and Saving.md` |
| FMENU-5 | `Open Recent` submenu - the last 10 projects, a missing one greyed with ` (missing)`, plus `Clear Recent Projects` | Content | IMP-70 | `Projects and Saving.md` |
| FMENU-6 | `Save  (Ctrl+S)` | Content | IMP-70 | `Projects and Saving.md` |
| FMENU-7 | `Save As...  (Shift+Ctrl+S)` | Content | IMP-70 | `Projects and Saving.md` |
| FMENU-8 | `Save as Template...` - the project-level save-as cousin: the whole project as a reusable skeleton | Content | IMP-73 | `Templates.md` |
| FMENU-9 | `Restore from Backup...` | Content | IMP-70 | `Projects and Saving.md` |
| FMENU-10 | `Import Audio...` | Content | IMP-74 | `Clips Page.md` |
| FMENU-11 | `Export Audio...` - the render dialog, *see EXP* | Content | IMP-76 | `Freeze and Export.md` |
| FMENU-12 | `Export Project Bundle...` - *see BUNDLE* | Content | IMP-71 | `Project Bundles.md` |


### `EMENU` - `Edit Menu.png`

The app `Edit` menu. `Undo` and `Redo` grey out when there is nothing to step.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| EMENU-1 | `Undo  (Ctrl+Z)` | Writing | IMP-69 | `Undo History.md` |
| EMENU-2 | `Redo  (Ctrl+Alt+Z)` | Writing | IMP-69 | `Undo History.md` |
| EMENU-3 | `History...` - the full undo list, *see UNDO* | Writing | IMP-69 | `Undo History.md` |
| EMENU-4 | `New Tab` submenu - the same engine list the ribbon `+` opens, *see TABUTN* | Tabs | IMP-57 | `Workspace and Windows.md` |
| EMENU-5 | `New Automation Clip` | Writing | IMP-63 | `Automation.md` |


### `PMENU` - `Pattern Menu.png`

The app `Patterns` menu - pattern-list housekeeping. The notes themselves live in the editing surfaces.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| PMENU-1 | `Rename / Color  (F2)` | Writing | IMP-65 | `Patterns and Arrangement.md` |
| PMENU-2 | `Find Next Empty Pattern  (F3)` | Writing | IMP-65 | `Patterns and Arrangement.md` |
| PMENU-3 | `Insert One  (Shift+Ctrl+Ins)` | Writing | IMP-65 | `Patterns and Arrangement.md` |
| PMENU-4 | `Clone  (Alt+C)` | Writing | IMP-65 | `Patterns and Arrangement.md` |
| PMENU-5 | `Delete` - deliberately no shortcut: bare Delete belongs to whichever editing surface is focused. Greyed while only one pattern exists | Writing | IMP-65 | `Patterns and Arrangement.md` |
| PMENU-6 | `Move Up  (Shift+Ctrl+Up)` | Writing | IMP-65 | `Patterns and Arrangement.md` |
| PMENU-7 | `Move Down  (Shift+Ctrl+Down)` | Writing | IMP-65 | `Patterns and Arrangement.md` |


### `VMENU` - `View Menu.png`

The app `View` menu - one row per F-key navigation command, same labels as the key-bind catalog.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| VMENU-1 | `Show Builder  (F5)` - *see BLD* | Shell | IMP-58 | `Workspace and Windows.md` |
| VMENU-2 | `Show Mixer  (F6)` - *see MIX* | Shell | IMP-58 | `Workspace and Windows.md` |
| VMENU-3 | `Show Player (Most Recent)  (F7)` | Shell | IMP-58 | `Workspace and Windows.md` |
| VMENU-4 | `Show Effects Rack (Most Recent)  (F8)` - *see FXI* | Shell | IMP-58 | `Workspace and Windows.md` |
| VMENU-5 | `Show Effect Panel (Most Recent)  (F9)` - *see FX* | Shell | IMP-58 | `Workspace and Windows.md` |
| VMENU-6 | `Show Piano Roll (Most Recent)  (F10)` - *see PR* | Shell | IMP-58 | `Workspace and Windows.md` |
| VMENU-7 | `Show Drum Kit (Most Recent)  (F11)` - *see DKIT* | Shell | IMP-58 | `Workspace and Windows.md` |
| VMENU-8 | `Show Event Editor (Most Recent)  (F12)` - *see EVT* | Shell | IMP-58 | `Workspace and Windows.md` |


### `OMENU` - `Options Menu.png`

The app `Options` menu.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| OMENU-1 | `General` submenu - `Set Default Template... (current: <name>)` and `Clear Default Template` | Content | IMP-73 | `Templates.md` |
| OMENU-2 | `File Settings...` - *see FILE* | Content | IMP-70 | `Projects and Saving.md` |
| OMENU-3 | `Audio Settings...` - *see AUD* | Shell | IMP-66 | `Transport and Playback.md` |
| OMENU-4 | `Plugins...` - scan and add third-party VST3s, *see PLUG* | Content | IMP-77 | `Plugins Page.md` |
| OMENU-5 | `Get Sound Content...` - fetches the Core Library sound content | Content | IMP-74 | `Sample Library.md` |
| OMENU-6 | `Undo History Size` submenu - `100` / `250` / `500` / `1000  steps`, tick on the current value | Writing | IMP-69 | `Undo History.md` |
| OMENU-7 | `MIDI is Omni (all devices)  -  Read Only` - permanently greyed on purpose: information, not a setting. Every connected MIDI device is always listened to | Writing | IMP-64 | `MIDI Learn.md` |


### `HMENU` - `Help Menu.png`

The app `Help` menu.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| HMENU-1 | `Help Index  (F1)` - opens these manuals | Shell | - | `Workspace and Windows.md` |
| HMENU-2 | `Key Binds...` - *see KEYS* | Shell | IMP-79 | `Keyboard Shortcuts.md` |
| HMENU-3 | `View Projects MidiMap` - the project's MIDI mappings | Shell | IMP-64 | `MIDI Learn.md` |
| HMENU-4 | `About BaySickDAW v1.0` | Shell | - | `Workspace and Windows.md` |


### `TRANRM` - `Recording Menu.png`

The record button's chevron menu - the right edge of the button opens this; clicking the dot itself arms recording (see TRAN).

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| TRANRM-1 | `ASIO` - record audio through the audio device. The tick marks the active mode | Writing | IMP-66 | `Transport and Playback.md` |
| TRANRM-2 | `MIDI (piano roll tabs only)` - captures notes into whichever piano-roll page is open. Not a general MIDI input router | Writing | IMP-66 | `Transport and Playback.md` |
| TRANRM-3 | `Global Record-Quantize` submenu - `Off` / `Line` / `Bar` / `Beat` down to `1/6 Step`, the same 11-step list as the Builder and Piano Roll snap menus. Snaps captured notes to the grid at commit time; `Off` keeps raw timing | Writing | IMP-66 | `Transport and Playback.md` |


### `TRANMM` - `Metronome Menu.png`

The metronome chevron's settings panel - a floating box, not a list. The metronome on/off toggle is the transport button itself (see TRAN).

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| TRANMM-1 | `Sound:` - `Sine` / `Click` / `Wood` / `Bell` | Writing | IMP-66 | `Transport and Playback.md` |
| TRANMM-2 | `Volume:` - metronome level, 0-200%, with its readout | Writing | IMP-66 | `Transport and Playback.md` |
| TRANMM-3 | `Precount:` - `1-bar lead-in when recording (Ctrl+P)`. Fires only when record is armed AND this is on | Writing | IMP-66 | `Transport and Playback.md` |


### `BLDM` - `Builder Menu.png`

The Builder window's `Menu` dropdown.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BLDM-1 | `Import Audio...` - brings an audio file in as a clip | Content | IMP-74 | `Clips Page.md` |
| BLDM-2 | `Rename Pattern`  (`F2`) | Writing | IMP-65 | `Patterns and Arrangement.md` |
| BLDM-3 | `Find Next Empty`  (`F3`) | Writing | IMP-65 | `Patterns and Arrangement.md` |
| BLDM-4 | `New Automation Clip...` | Writing | IMP-63 | `Automation.md` |
| BLDM-5 | `Render Pattern to WAV...` - renders the current pattern through the render options dialog | Content | IMP-76 | `Freeze and Export.md` |


### `BLDE` - `Builder Edit.png`

The Builder window's `Edit` menu. Everything routes to the arrangement grid.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BLDE-1 | `Undo` - greyed until the grid has something to step back | Writing | IMP-69 | `Undo History.md` |
| BLDE-2 | `Redo` | Writing | IMP-69 | `Undo History.md` |
| BLDE-3 | `Select All`  (`Ctrl+A`) | Writing | IMP-65 | `Builder Page.md` |
| BLDE-4 | `Deselect`  (`Esc`) | Writing | IMP-65 | `Builder Page.md` |
| BLDE-5 | `Copy`  (`Ctrl+C`) | Writing | IMP-65 | `Builder Page.md` |
| BLDE-6 | `Paste`  (`Ctrl+V`) | Writing | IMP-65 | `Builder Page.md` |
| BLDE-7 | `Delete`  (`Del`) | Writing | IMP-65 | `Builder Page.md` |
| BLDE-8 | `Duplicate`  (`Ctrl+B`) | Writing | IMP-65 | `Builder Page.md` |


### `BLDV` - `Builder View.png`

The Builder window's `View` menu.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BLDV-1 | `Zoom In`  (`+`) | Writing | IMP-65 | `Builder Page.md` |
| BLDV-2 | `Zoom Out`  (`-`) | Writing | IMP-65 | `Builder Page.md` |
| BLDV-3 | `Performance Mode`  (`Ctrl+P`) - tick when on | Writing | IMP-65 | `Builder Page.md` |


### `PRE` - `Piano Roll Edit.png`

The Piano Roll `Edit` menu.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| PRE-1 | `Select All`  (`Ctrl+A`) | Writing | IMP-67 | `Piano Roll.md` |
| PRE-2 | `Deselect` | Writing | IMP-67 | `Piano Roll.md` |
| PRE-3 | `Copy`  (`Ctrl+C`) | Writing | IMP-67 | `Piano Roll.md` |
| PRE-4 | `Paste`  (`Ctrl+V`) | Writing | IMP-67 | `Piano Roll.md` |
| PRE-5 | `Delete` | Writing | IMP-67 | `Piano Roll.md` |
| PRE-6 | `Duplicate`  (`Ctrl+B`) | Writing | IMP-67 | `Piano Roll.md` |


### `PRT` - `Piano Roll Tools.png`

The Piano Roll `Tools` menu - the note processors.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| PRT-1 | `Quantize`  (`Alt+Q`) - snaps the selection to the `Quantize Settings` grid | Writing | IMP-67 | `Piano Roll.md` |
| PRT-2 | `Strum`  (`Alt+S`) | Writing | IMP-67 | `Piano Roll.md` |
| PRT-3 | `Arpeggiate`  (`Alt+A`) | Writing | IMP-67 | `Piano Roll.md` |
| PRT-4 | `Chop...` submenu - `Into 2  (halves)` / `Into 3  (thirds)` / `Into 4  (quarters)` / `Into 6` / `Into 8` | Writing | IMP-67 | `Piano Roll.md` |
| PRT-5 | `Glue`  (`Ctrl+G`) | Writing | IMP-67 | `Piano Roll.md` |
| PRT-6 | `Articulate`  (`Alt+L`) | Writing | IMP-67 | `Piano Roll.md` |
| PRT-7 | `Randomize`  (`Alt+R`) | Writing | IMP-67 | `Piano Roll.md` |
| PRT-8 | `Humanize...` | Writing | IMP-67 | `Piano Roll.md` |
| PRT-9 | `Riff Machine...`  (`Alt+E`) - the 8-step riff generator | Writing | IMP-67 | `Piano Roll.md` |
| PRT-10 | `Generate Chords`  (`Alt+P`) | Writing | IMP-67 | `Piano Roll.md` |
| PRT-11 | `Quantize Settings` submenu - `1/4` / `1/8` / `1/16` / `1/32`, tick on the current grid | Writing | IMP-67 | `Piano Roll.md` |
| PRT-12 | `Transpose Up`  (`Shift+Up`) | Writing | IMP-67 | `Piano Roll.md` |
| PRT-13 | `Transpose Down`  (`Shift+Down`) | Writing | IMP-67 | `Piano Roll.md` |
| PRT-14 | `Transpose Up Octave`  (`Ctrl+Up`) | Writing | IMP-67 | `Piano Roll.md` |
| PRT-15 | `Transpose Down Octave`  (`Ctrl+Down`) | Writing | IMP-67 | `Piano Roll.md` |


### `PRS` - `Piano Roll Scale.png`

The Piano Roll `Scale` menu.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| PRS-1 | `Snap to Scale` - tick when a scale is active; drawn and dragged notes land in it | Writing | IMP-67 | `Piano Roll.md` |
| PRS-2 | `Root` submenu - `C` through `B`, tick on the current root | Writing | IMP-67 | `Piano Roll.md` |
| PRS-3 | `Scale` submenu - the scale list, tick on the current one | Writing | IMP-67 | `Piano Roll.md` |


### `PRC` - `Piano Roll Chords.png`

The Piano Roll `Chords` menu.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| PRC-1 | The chord types - `Major` through `Add 9`, one tick at a time. With a chord ticked, the Draw tool places that chord instead of single notes | Writing | IMP-67 | `Piano Roll.md` |


### `PRV` - `Piano Roll View.png`

The Piano Roll `View` menu.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| PRV-1 | `Zoom In` | Writing | IMP-67 | `Piano Roll.md` |
| PRV-2 | `Zoom Out` | Writing | IMP-67 | `Piano Roll.md` |
| PRV-3 | `Zoom In Vertical` | Writing | IMP-67 | `Piano Roll.md` |
| PRV-4 | `Zoom Out Vertical` | Writing | IMP-67 | `Piano Roll.md` |
| PRV-5 | `Scroll to Playhead` | Writing | IMP-67 | `Piano Roll.md` |
| PRV-6 | `Ghost Notes` - tick when other tabs' notes ghost behind yours | Writing | IMP-67 | `Piano Roll.md` |
| PRV-7 | `Velocity Lane` - tick when the bottom lane is shown | Writing | IMP-67 | `Piano Roll.md` |


### `DKITE` - `Drum Kit Edit.png`

The Drum Kit `Edit` menu - the same edit set as the Piano Roll (see PRE).

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| DKITE-1 | `Select All`  (`Ctrl+A`) | Writing | IMP-67 | `Piano Roll.md` |
| DKITE-2 | `Deselect` | Writing | IMP-67 | `Piano Roll.md` |
| DKITE-3 | `Copy`  (`Ctrl+C`) | Writing | IMP-67 | `Piano Roll.md` |
| DKITE-4 | `Paste`  (`Ctrl+V`) | Writing | IMP-67 | `Piano Roll.md` |
| DKITE-5 | `Delete` | Writing | IMP-67 | `Piano Roll.md` |
| DKITE-6 | `Duplicate`  (`Ctrl+B`) | Writing | IMP-67 | `Piano Roll.md` |


### `DKITT` - `Drum Kit Tools.png`

The Drum Kit `Tools` menu. No `Arpeggiate`, `Generate Chords` or `Transpose` here - those are pitch-based and do not translate to a slot-per-row drum layout.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| DKITT-1 | `Quantize`  (`Alt+Q`) | Writing | IMP-67 | `Piano Roll.md` |
| DKITT-2 | `Strum`  (`Alt+S`) | Writing | IMP-67 | `Piano Roll.md` |
| DKITT-3 | `Chop...` submenu - `Into 2  (halves)` / `Into 3  (thirds)` / `Into 4  (quarters)` / `Into 6` / `Into 8` | Writing | IMP-67 | `Piano Roll.md` |
| DKITT-4 | `Glue`  (`Ctrl+G`) | Writing | IMP-67 | `Piano Roll.md` |
| DKITT-5 | `Articulate`  (`Alt+L`) | Writing | IMP-67 | `Piano Roll.md` |
| DKITT-6 | `Randomize`  (`Alt+R`) | Writing | IMP-67 | `Piano Roll.md` |
| DKITT-7 | `Quantize Settings` submenu - `1/4` / `1/8` / `1/16` / `1/32`, tick on the current grid | Writing | IMP-67 | `Piano Roll.md` |


### `DKITV` - `Drum Kit View.png`

The Drum Kit `View` menu. No vertical zoom (rows are fixed height) and no `Ghost Notes` here.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| DKITV-1 | `Zoom In` | Writing | IMP-67 | `Piano Roll.md` |
| DKITV-2 | `Zoom Out` | Writing | IMP-67 | `Piano Roll.md` |
| DKITV-3 | `Scroll to Playhead` | Writing | IMP-67 | `Piano Roll.md` |
| DKITV-4 | `Velocity Lane` - tick when the bottom lane is shown | Writing | IMP-67 | `Piano Roll.md` |


### `BSVCM` - `BaySickVocals Menu.png`

The Vox page menu - the four editors, then the tab housekeeping.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSVCM-1 | `Vocal Chain` - *see BSVC* | Vocal | IMP-57 | `Vox Page.md` |
| BSVCM-2 | `BaySickPitch` - *see BSPIT* | Vocal | IMP-57 | `Vox Page.md` |
| BSVCM-3 | `BaySickAlign` - *see BSA* | Vocal | IMP-57 | `Vox Page.md` |
| BSVCM-4 | `NAM/IR` - *see BSNAM* | Vocal | IMP-57 | `Vox Page.md` |
| BSVCM-5 | `FX Rack` - this channel's rack, *see FXI* | Mixing | IMP-62 | `Effect Racks.md` |
| BSVCM-6 | `Freeze` - renders this tab and plays the render instead. Greys when unavailable, with the reason in its tooltip; label color signals state - cyan while frozen, orange once the render goes stale. On a Vox tab the freeze prints the WHOLE chain - gate to limiter plus pitch and alignment - so a confirm warns first | Tabs | IMP-75 | `Freeze and Export.md` |
| BSVCM-7 | `Lock` - tick when locked; a locked tab shows `[L] ` on its ribbon slot | Tabs | IMP-70 | `Workspace and Windows.md` |
| BSVCM-8 | `Rename...` | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSVCM-9 | `Duplicate Vox (new tab)` | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSVCM-10 | `Save Page Preset As...` | Tabs | IMP-72 | `Presets.md` |
| BSVCM-11 | `Load Page Preset` submenu | Tabs | IMP-72 | `Presets.md` |
| BSVCM-12 | `Delete Vox` | Tabs | IMP-57 | `Workspace and Windows.md` |


### `BSPDLM` - `BaySickPedals Menu.png`

The Pedals window menu.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSPDLM-1 | `Pedals` - this window, tick when shown | Instruments | IMP-57 | `Pedalboard.md` |
| BSPDLM-2 | `NAM/IR` - *see BSNAM* | Instruments | IMP-57 | `Pedalboard.md` |
| BSPDLM-3 | `Lock` - tick when locked; a locked tab shows `[L] ` on its ribbon slot | Tabs | IMP-70 | `Workspace and Windows.md` |
| BSPDLM-4 | `Rename...` | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSPDLM-5 | `Duplicate Inst (new tab)` | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSPDLM-6 | `Save Page Preset As...` | Tabs | IMP-72 | `Presets.md` |
| BSPDLM-7 | `Load Page Preset` submenu | Tabs | IMP-72 | `Presets.md` |
| BSPDLM-8 | `Delete Inst` | Tabs | IMP-57 | `Workspace and Windows.md` |


### `BSGBM` - `BaySickGuitars & Basses Menu.png`

The BaySickGuitars / BaySickBasses window menu - identical on both.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSGBM-1 | `Pedals` - *see BSPDL* | Instruments | IMP-57 | `Pedalboard.md` |
| BSGBM-2 | `NAM/IR` - *see BSNAM* | Instruments | IMP-57 | `Pedalboard.md` |
| BSGBM-3 | `Piano Roll` - jumps the roll to this tab | Instruments | IMP-57 | `Workspace and Windows.md` |
| BSGBM-4 | `FX Rack` - *see FXI* | Mixing | IMP-62 | `Effect Racks.md` |
| BSGBM-5 | `Freeze` - renders this tab and plays the render instead. Greys when unavailable, with the reason in its tooltip; label color signals state - cyan while frozen, orange once the render goes stale | Tabs | IMP-75 | `Freeze and Export.md` |
| BSGBM-6 | `Lock` - tick when locked; a locked tab shows `[L] ` on its ribbon slot | Tabs | IMP-70 | `Workspace and Windows.md` |
| BSGBM-7 | `Rename...` | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSGBM-8 | `Duplicate Inst (new tab)` | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSGBM-9 | `Save Page Preset As...` | Tabs | IMP-72 | `Presets.md` |
| BSGBM-10 | `Load Page Preset` submenu | Tabs | IMP-72 | `Presets.md` |
| BSGBM-11 | `Delete Inst` | Tabs | IMP-57 | `Workspace and Windows.md` |


### `BSSB` - `BaySickSynth FLT.png`

Two versions of one family: BaySickSynth (Layers tabs) and BaySickBass (Bass tabs). Same window, same six panels, same controls - the Bass differs only in accent color and defaults: monophonic voicing, a small built-in slide, and a darker filter start.



### `BSSBT` - `BaySickSynth FLT.png` + `BaySickBass Titlebar Crop.png`

The family's window title bar - both bars in one entry, the same three
controls on each; the accent color (synth blue / bass green) says whose
window you are in.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSSBT-1 | `Preset` dropdown - the patch picker | Instruments | IMP-72 | `Presets.md` |
| BSSBT-2 | `Swing Mix` knob on the title bar, right of `Menu` - scales the global Swing for this player; right-click offers `Truncate Swing Notes` | Tabs | IMP-65 | `Patterns and Arrangement.md` |
### `BSSBM` - `BaySickSynth-BaySickPlayer-BaySickSolstice Menu.png` + `BaySickBass Menu Updated.png`

The family's tab menu - both menus in one entry; one shape, two namings
(`Layer` / `Bass`). The menu belongs to the TAB, not the engine, so the
BaySickPlayer picture is identical (see BSPM).

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSSBM-1 | `Player` - the engine view, tick when shown | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSSBM-2 | `Piano Roll` - jumps the roll to this tab | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSSBM-3 | `FX Rack` - *see FXI* | Mixing | IMP-62 | `Effect Racks.md` |
| BSSBM-4 | `Freeze` - renders this tab and plays the render instead. Greys when unavailable, with the reason in its tooltip; label color signals state - cyan while frozen, orange once the render goes stale | Tabs | IMP-75 | `Freeze and Export.md` |
| BSSBM-5 | `Lock Layer` / `Lock Bass` - tick when locked; a locked tab shows `[L] ` on its ribbon slot | Tabs | IMP-70 | `Workspace and Windows.md` |
| BSSBM-6 | `Polyphony: Polyphonic` / `Monophonic` - reads the engine's voice mode; click toggles it. The synth starts polyphonic, the bass monophonic. `Polyphony: (n/a)`, greyed, on a BaySickSolstice layer | Tabs | IMP-85 | `Workspace and Windows.md` |
| BSSBM-7 | `Rename...` | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSSBM-8 | `Replace Engine` submenu - swap this tab's engine in place; the notes, mixer settings, effects and window all stay. Tick = the current engine; greys while the tab is locked | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSSBM-9 | `Duplicate Layer / Bass (new tab)` - greys with no engine loaded | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSSBM-10 | `Choke Group` submenu - `None` / `Group 1` through `Group 16`; same-group tabs cut each other off | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSSBM-11 | `Save Current Patch As...` | Tabs | IMP-72 | `Presets.md` |
| BSSBM-12 | `Load Preset` submenu - the engine's preset tree; `(no presets installed)` when empty | Tabs | IMP-72 | `Presets.md` |
| BSSBM-13 | `Save Page Preset As...` | Tabs | IMP-72 | `Presets.md` |
| BSSBM-14 | `Load Page Preset` submenu | Tabs | IMP-72 | `Presets.md` |
| BSSBM-15 | `Delete Layer` / `Delete Bass` | Tabs | IMP-72 | `Workspace and Windows.md` |


### `BSPM` - `BaySickSynth-BaySickPlayer-BaySickSolstice Menu.png`

The Layers-tab menu as it reads on a BaySickPlayer tab - identical to the BaySickSynth one (see BSSBM); the menu belongs to the TAB, not the engine.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSPM-1 | `Player` - the engine view, tick when shown | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSPM-2 | `Piano Roll` - jumps the roll to this tab | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSPM-3 | `FX Rack` - *see FXI* | Mixing | IMP-62 | `Effect Racks.md` |
| BSPM-4 | `Freeze` - renders this tab and plays the render instead. Greys when unavailable, with the reason in its tooltip; label color signals state - cyan while frozen, orange once the render goes stale | Tabs | IMP-75 | `Freeze and Export.md` |
| BSPM-5 | `Lock Layer` - tick when locked; a locked tab shows `[L] ` on its ribbon slot | Tabs | IMP-70 | `Workspace and Windows.md` |
| BSPM-6 | `Polyphony: Polyphonic` / `Monophonic` - reads the engine's voice cap; click toggles it | Tabs | IMP-85 | `Workspace and Windows.md` |
| BSPM-7 | `Rename...` | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSPM-8 | `Replace Engine` submenu - swap this tab's engine in place; the notes, mixer settings, effects and window all stay. Tick = the current engine; greys while the tab is locked | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSPM-9 | `Duplicate Layer (new tab)` - greys with no engine loaded | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSPM-10 | `Choke Group` submenu - `None` / `Group 1` through `Group 16`; same-group tabs cut each other off | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSPM-11 | `Save Current Patch As...` | Tabs | IMP-72 | `Presets.md` |
| BSPM-12 | `Load Preset` submenu - the engine's preset tree; `(no presets installed)` when empty | Tabs | IMP-72 | `Presets.md` |
| BSPM-13 | `Save Page Preset As...` | Tabs | IMP-72 | `Presets.md` |
| BSPM-14 | `Load Page Preset` submenu | Tabs | IMP-72 | `Presets.md` |
| BSPM-15 | `Delete Layer` | Tabs | IMP-57 | `Workspace and Windows.md` |


### `BSDRUMS` - `Drum Kit.png`

Where drums come from - the entry that frames the menu below. Two choices
happen when a drum is picked, and they are independent: WHERE you pick it
(the Drum Kit's pads, or the ribbon `+`) and WHAT KIND of sound it is
(a sample through BaySickPlayer, or a synth patch through BaySickSynth).
No callout rows - the picture is the Drum Kit page where most picking
happens; the mechanics live in the chapter text and the menu entry below.

### `BSDM` - `BaySickDrum Menu.png`

The per-drum menu - right-click any pad on the Drum Kit, or open the Menu on
that drum's own player window (the window route adds the view rows and page
presets around this same core). Every drum tab is its own player, so this is
the drum twin of the Layers tab menu.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSDM-1 | `Lock Drum` - tick when locked; a locked drum can't be deleted or replaced | Tabs | IMP-70 | `Workspace and Windows.md` |
| BSDM-2 | `Polyphony: Polyphonic` / `Monophonic` - reads the drum engine's voice mode; click toggles it | Tabs | IMP-85 | `Workspace and Windows.md` |
| BSDM-3 | `Rename...` | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSDM-4 | `Replace Sound...` - opens the same picker as adding a drum (samples, library sounds, SFZ, synth patches); the drum's notes, mixer settings and effects stay. Backing out changes nothing | Tabs | IMP-57 | `Drum Kit.md` |
| BSDM-5 | `Duplicate Drum (new tab)` - greys with no sound loaded | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSDM-6 | `Choke Group` submenu - `None` / `Group 1` through `Group 16`; the classic open/closed hi-hat cutoff | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSDM-7 | `MIDI Note` submenu - the drum's play pitch, with the current assignment at the top | Tabs | IMP-57 | `Drum Kit.md` |
| BSDM-8 | `MIDI Learn` - bind a pad or key to trigger this drum; the label carries the current binding, and `MIDI Forget` appears below it once bound | Writing | IMP-64 | `MIDI Learn.md` |
| BSDM-9 | `Save Current Patch As...` | Tabs | IMP-72 | `Presets.md` |
| BSDM-10 | `Delete Drum` - greys while locked | Tabs | IMP-57 | `Workspace and Windows.md` |

### `BSRDTTL` - `BaySickRustyDrums Main.png`

The BaySickRustyDrums window's title bar and section tabs.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSRDTTL-1 | `Presets` button | Instruments | IMP-72 | `Presets.md` |
| BSRDTTL-2 | Kit dropdown (`Full`) | Instruments | IMP-72 | `BaySickRustyDrums.md` |
| BSRDTTL-3 | Section tabs - `Main` / `Kick` / `Snare` / `Toms` / `Hi-hat` / `Cymbals` / `Noises`; each opens its panel figure | Instruments | IMP-57 | `BaySickRustyDrums.md` |


### `BSRDMENU` - `BaySickRustyDrums Menu.png`

The BaySickRustyDrums window menu. No FX Rack entry here.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSRDMENU-1 | `Drum Kit` - the kit photograph view, *see BSRDKIT* | Instruments | IMP-57 | `BaySickRustyDrums.md` |
| BSRDMENU-2 | `Player` - this window's kit view, tick when shown | Instruments | IMP-57 | `BaySickRustyDrums.md` |
| BSRDMENU-3 | `Piano Roll` - jumps the roll to this tab | Instruments | IMP-57 | `Workspace and Windows.md` |
| BSRDMENU-4 | `Rusty Drums Map...` - the note-to-drum table, *see BSRDMAP* | Instruments | IMP-57 | `BaySickRustyDrums.md` |
| BSRDMENU-5 | `Freeze` - renders this tab and plays the render instead. Greys when unavailable, with the reason in its tooltip; label color signals state - cyan while frozen, orange once the render goes stale | Tabs | IMP-75 | `Freeze and Export.md` |
| BSRDMENU-6 | `Save Page Preset As...` | Tabs | IMP-72 | `Presets.md` |
| BSRDMENU-7 | `Load Page Preset` submenu | Tabs | IMP-72 | `Presets.md` |


### `BSPLUGM` - `BaySickPlugins Menu.png`

The hosted-plugin window menu.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| BSPLUGM-1 | `Piano Roll` - jumps the roll to this plugin tab | Instruments | IMP-57 | `Plugins Page.md` |
| BSPLUGM-2 | `FX Rack` - *see FXI* | Mixing | IMP-62 | `Effect Racks.md` |
| BSPLUGM-3 | `Freeze` - renders this tab and plays the render instead. Greys when unavailable, with the reason in its tooltip; label color signals state - cyan while frozen, orange once the render goes stale | Tabs | IMP-75 | `Freeze and Export.md` |
| BSPLUGM-4 | `Rename...` | Tabs | IMP-57 | `Workspace and Windows.md` |
| BSPLUGM-5 | `Replace Plugin` submenu - swap the hosted plugin in place; the notes, mixer settings, effects and window all stay. Lists your added instruments; tick = the current plugin | Tabs | IMP-57 | `Plugins Page.md` |
| BSPLUGM-6 | `Duplicate Plugin (new tab)` - clones plugin + settings into a fresh tab; greys with no plugin loaded | Tabs | IMP-57 | `Plugins Page.md` |
| BSPLUGM-7 | `Save Page Preset As...` | Tabs | IMP-72 | `Presets.md` |
| BSPLUGM-8 | `Load Page Preset` submenu | Tabs | IMP-72 | `Presets.md` |
| BSPLUGM-9 | `Automate` submenu - the hosted plugin's parameters; picking one opens that parameter's automation lane | Writing | IMP-63 | `Automation.md` |
| BSPLUGM-10 | `Delete Plugin` | Tabs | IMP-57 | `Workspace and Windows.md` |


### `MIXMNU` - `Mixer Menu.png`

The Mixer window menu.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| MIXMNU-1 | `Pan Law` submenu - `Ramped` / `Flat`, hover tooltips | Mixing | IMP-60 | `Mixer.md` |
| MIXMNU-2 | `Master Output` submenu | Mixing | IMP-61 | `Mixer.md` |
| MIXMNU-3 | `Latency-compensate meters` | Mixing | IMP-48 | `Mixer.md` |
| MIXMNU-4 | `Multi-core Rendering` - tick when on | Mixing | IMP-60 | `Mixer.md` |


### `MIXADD` - `Mixer Add.png`

The Mixer `Add` menu.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| MIXADD-1 | `Aux Strip` - adds a free aux strip | Mixing | IMP-60 | `Mixer.md` |
| MIXADD-2 | `Vox Bus` through `Plugins Bus` - the six group buses; each adds that bus strip | Mixing | IMP-61 | `Mixer.md` |


### `MIXSTP` - `Mixer Strip Crop.png`

Two strips side by side: an engine strip (`Bass 1`) and a live-input strip (`Vox 1`). The engine strip's anatomy is the Mixer figure's rows (see MIX); the live-input extras are numbered here.

| Callout | On-screen label | Manual 2 | IMP | System Reference |
|---|---|---|---|---|
| MIXSTP-1 | `A` arm LED - red while armed. The tooltip names the assigned input channel; right-click opens the input picker (`Vocal Input` / `LiveInst Input` channel list, `No channels available` when the device has none, Vox adds `Builder Grid Default` routes `Dry` / `Dry Cleaned` / `Wet` / `Wet Cleaned`, and `Disarm` while armed) | Mixing | IMP-60 | `Mixer.md` |
| MIXSTP-2 | Headphones Listen LED - hear this input through the bus and master. Right-click picks the monitor mode: Vox `True Dry` / `Bypass Pitch Corrector` / `With Effect`; Inst `Dry` / `With Effect` | Mixing | IMP-60 | `Mixer.md` |
| MIXSTP-3 | Split meter - peak bars below, scrolling RMS history above (every strip but Master) | Mixing | IMP-48 | `Mixer.md` |
