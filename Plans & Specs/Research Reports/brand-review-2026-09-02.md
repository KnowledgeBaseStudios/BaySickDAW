# Brand-Safety Review - everything that ships under a real name (2026-09-02)

**Status: awaiting Jeff's rulings.**  Batch QA-Solstice Task 4
(`Batch Plans/solar-scrubbing-sparrow.md`).  This file is the working docket -
rule in the **Ruling** column (rename / keep / defer, plus the new name where
you have one) and the renames become QA-Solstice Task 5.

## Why this exists

The additive engine shipped for four-plus months as "Harmless" - the name of
Image-Line's additive synth, same category, inside a DAW.  It was missed
because the brand-name rule's own memory listed `Harmless` as brand-safe.  The
engine is now `BaySickSolstice` in full (QA-Solstice T1-T3, commits `005fb2ee`,
`fe4931d3`, `363b5ee4`).  This review is the same question asked of everything
ELSE that ships: preset / kit / template names, user-facing strings, the
manual's prose and figures, and the installer.

## How it was produced

One read-only agent with judgment, not a keyword grep (a grep missed "UREI" in
June and "Harmless" for four months).  Coverage: every preset / kit / template
name (946 files); all 80 In Depth manual pages in full; the 90 In The Weeds
pages (prose in full, ~950 KB of code excerpts regex-scanned against a
~600-term brand list); the control blurbs; the installer script; every
capitalised string literal in `Source/` (4,565 unique) plus an ALL-CAPS pass;
14 manual figures viewed.  Findings that matter most were spot-checked against
source before being written here (Riff Machine, the Furman EQ topic title, the
VSTPlugin menu entry, Big Rusty Drums provenance, the About-box comment, every
NEW preset name, `Hosted Plugin.png` viewed).

## How to answer

Per group or per item.  Three answers exist: **rename** (say the new name if
you have one, otherwise it comes back as a naming docket), **keep**, **defer**
(logged in Future State, not shipped-blocking).  Group 4 is a policy call with
three shapes, not a per-item list.

---

## G1 - our own feature / engine / menu names built on someone else's product name

The Harmless class.  Five more.

| # | Item | Where | Mark + owner | Ruling |
|---|---|---|---|---|
| 1 | "Riff Machine" and its eight step names (Progression, Chords, Arpeggiation, Mirror, Levels, Articulation, Groove, Fit) | `Source/Standalone/KeyBindings.cpp:496-497`; the Piano Roll dialog tabs `PianoRoll.cpp:5343+`; manual `PRT.html` | Image-Line - FL Studio's piano-roll generator; ours is an FL replica carrying FL's tool name and FL's step list verbatim | |
| 2 | "BaySickRustyDrums", "+ Add BaySickRustyDrums", "Rusty Drums Map", "Rusty Player", Core Library pack "Big Rusty Drums" | `RibbonTabBar.cpp:587-930`, `RustyDrumsMapWindow.cpp:113`, `BaySickRustyDrumsPage.cpp:328`, `CoreLibraryInstaller.h:106`; manual `BSRD*`, `TABUTN` | Karoryfer Samples - "Big Rusty Drums" is their library's name (CC-BY content; the pack unpacks as `karoryfer.big-rusty-drums-1.100`).  Karoryfer is uncredited in About | |
| 3 | "BaySickNAMIR", "NAM/IR", "NAM Pedal", "Load NAM Pedal", "User NAM Pedal", "NAM Bypass", "Load .nam" | `BaySickNAMIREditor.cpp:74`, `SharedUI.h:352`, `SlotComponent.cpp:1307`, `BaySickPedalsEditor.cpp:493,521`, `BaySickNAMIRProcessor.cpp:113`, `EffectEditorPanels.cpp:6598`; manual `BSNAM`, `BSPDLP`, `BSGBM` | Neural Amp Modeler - Steven Atkinson's open-source project (MIT; no registration known).  Uncredited in About.  ".nam" as a file extension is fine; the name inside OUR engine / pedal names is the question | |
| 4 | "+ Add VSTPlugin" (product-style, no space) | `RibbonTabBar.cpp:893`; manual `TABUTN`, `PLUG` | VST - Steinberg (registered).  Nominative "VST3 plugin" elsewhere is fine; this one is used as our menu entry's name | |
| 5 | "Furman EQ" as the shipped In The Weeds topic title (x6 in `manual.html`) | `Plans & Specs/System Reference/Callout Registry.md:555` (IMP-26) | Furman (Nortek).  The panel itself is "EQFH Style" (safe) | |

## G2 - preset / kit / template names carrying a registered mark

These show verbatim in the preset browser, the kit menu, and File > New from Template.

| # | Item | Where | Mark + owner | Ruling |
|---|---|---|---|---|
| 6 | TR-606 / TR-808 / TR-909 kit folders (9 kit files) + 3 templates; the default kit is hardcoded `"TR-808/TR-808 Full.xml"` | `Kits/Factory/TR-*`, `Templates/Factory/TR-*.xml`, `StandaloneEditor.cpp:8581` | Roland | |
| 7 | Fat Juno Sub, Juno Poly, Juno Warm Pad, Jupiter Brass, Jupiter Brass Pad, JP-8000 Supersaw Pad; Prophet Sync | `Presets/BaySickBass`, `Presets/BaySickSynth` | Roland; Sequential | |
| 8 | `Presets/BaySickDrums/Yamaha Group/` (folder, 16 files; 33 references inside kit files) + DX7 Glass / Metal / Woodblock, DX Style Tubulum, RX-11 Kick / Snare; DX EP, DX Bass FM, DX Lead FM; CS-80 Bell, CS-80 Brass Lead | `Presets/BaySickDrums`, `BaySickSynth`, `BaySickBass`; templates `FM Digital`, `Hip Hop (Real)`, `TR-808` | Yamaha | |
| 9 | Moog Sub, 70s Moog Bass, Moog Minimoog Bass, Analog Moog Style, Moog Lead, Moog Lead Woop, Moog Style Lead, Moog Hz Interval | `Presets/BaySickBass`, `BaySickSynth` | Moog Music (inMusic); Minimoog is also a mark | |
| 10 | `Presets/BaySickDrums/Simmons Group/` (folder, 16 files; 50 references inside kit files) + Simmons 80s Tom, Simmons Kick, Simmons Low Tom, Simmons SDS-7 Kick, Simmons Snare, Simmons Sweep FX, Simmons Tom Hi / Lo, Vintage Simmons Snare; Disco Syndrum; Linn Disco Kick | `Presets/BaySickDrums` | Simmons (Guitar Center); Syndrum (Pollard); Linn (Roger Linn - a living person's name) | |
| 11 | Rhodes x6 (Warm / Boom-Bap / Soulful Rhodes, Rhodes EP, Rhodes Wurli Strike, Vintage Rhodes); Wurlitzer / Wurli x4; Hammond x3 + B3 x2; Clavinet + Synth Clav x2; Farfisa Organ; Vox Continental; Mellotron Pad; Solina Strings | `Presets/BaySickSolstice`, `BaySickSynth` | Rhodes Music Group; Wurlitzer; Hammond (Suzuki); Hohner (Clavinet); Farfisa; Vox (Korg); Mellotron; Solina (Eminent / ARP) | |
| 12 | OB-8 Brass x2, OB-8 String Pad; ARP Lead | `Presets/BaySickSolstice`, `BaySickSynth` | Oberheim; ARP (Korg) - capital ARP is the company, "Arp" meaning arpeggio is fine | |
| 13 | Gameboy Pulse x3 | `Presets/BaySickBass`, `BaySickSolstice`, `BaySickSynth` | Nintendo | |
| 14 | `1960-V30.wav` - the only shipped IR; the file name shows in the CAB field once loaded | `Presets/BaySickNAMIR/IR/` | Marshall 1960 cabinet; Celestion Vintage 30 | |
| 15 | Skrillex Reese (+ tab names in 4 templates); Rezz Style Bass; Vangelis Brass (+ template); Sci-fi Zap R2-D2; Doctor Who Theremin | `Presets/BaySickSolstice`, `BaySickBass`, `BaySickSynth`; templates `Bass Music`, `Dubstep (Real)`, `EDM Big Room`, `Hardstyle (Real)`, `80s Electronic` | Living people (Skrillex, Rezz - implied endorsement); the Vangelis estate; Lucasfilm / Disney (R2-D2); BBC (Doctor Who) | |

## G3 - tooltips

| # | String | Where | Mark | Ruling |
|---|---|---|---|---|
| 16 | "Metallic / bell / sci-fi character. CS-80 and ARP 2600 signature." (RING toggle) | `BaySickBassEditor.cpp:111`, `BaySickSynthEditor.cpp:112`; mirrored into `bsd-docs.json` and the manual's control tables | Yamaha; ARP (Korg) | |
| 17 | "Raise for Juno/Prophet/CS-80 analog warmth." (Analog Drift) | `BaySickBassEditor.cpp:311`, `BaySickSynthEditor.cpp:313` | Roland; Sequential; Yamaha | |
| 18 | "Classic 80s sync-lead sound (Van Halen 'Jump', Final Countdown brass, Moog/ARP)." | `BaySickSynthEditor.cpp:104` | Van Halen; a song title; Europe's song; Moog; ARP | |
| 19 | "(try 800 Hz with SQUARE+SQUARE for 808 cowbell)", "Classic 808 cowbell - SQUARE+SQUARE, osc2 at ~800 Hz, play A4." | `BaySickSynthEditor.cpp:98, 568` | Roland (generic-use class) | |

## G4 - the In The Weeds manual publishes source comments (a policy call)

The comment policy allows modeled-gear names in code comments (nominative
fair use, Jeff's legal research 2026-06-07).  The 2026-08-28 ruling that every
In The Weeds topic shows the COMPLETE code made those comments user-facing.
Shipping in `manual.html` today:

| Topic | Names now user-facing | Source |
|---|---|---|
| IMP-1 | LA-2A, 1176, "FL Studio / Ableton behavior" | `CompressorDSP.cpp` comments |
| IMP-7 | SSL-style, Neve-style, Neve transformer | `SaturationDSP.cpp` |
| IMP-11 | Eventide H910 | `DelayDSP.cpp` |
| IMP-18 | DS-1 (x3) | `DistortionStyleDSP.cpp` |
| IMP-35 | SM58, SM7B, U87, C414, ELA M 251 / C12 (the excerpt's comments) | `MicSimDSP.cpp` - widening the excerpt adds Beta 58, e835, TLM103, KM184, R-121, D112, Beta 52 |
| IMP-48 | Apple | `LoudnessSpec.h` |
| IMP-50 | FL / Logic / ProTools | `SharedUI.cpp:2643` |
| IMP-67 | Vital / Surge XT | `BroadcastSynthesiser` notes |
| IMP-68 | FL Studio | `PatternManager` |
| IMP-84 / 86 | 808 / 909, Hammond, DX7 | `BaySickSynthVoice.cpp:745-746` |

Shapes: **(a)** accept as nominative in a technical appendix (leave as is);
**(b)** strip gear names from comments in EXCERPTED ranges only, then re-quote
(touches the code, keeps the manual verbatim); **(c)** drop In The Weeds from
the shipped build and keep it as a repo-only doc.

**Ruling:** ______

## G5 - manual figures showing third-party products or artwork

| # | Figure | Content | Owner | Ruling |
|---|---|---|---|---|
| 20 | `Manuals/figures/Hosted Plugin.png` (hand capture, the Plugins-tab illustration) | ENVOSOUND's full GUI with the "Tsuga audio" logo | Tsuga Labs | |
| 21 | `Plugin Search.png` | "Envosound / Instrument / Tsuga Labs", "Filterjam / Effect / AudioThing", scan path `C:\Program Files\Steinberg\VSTPlugins` | Tsuga; AudioThing; Steinberg | |
| 22 | `Audio & Midi Settings.png` | "SAMSUNG (NVIDIA High Definition Audio)", "FLkey MIDI" | Samsung; NVIDIA; Novation (its "FL" is licensed from Image-Line) | |
| 23 | `BaySickRustyDrums Main / Drum Kit / Kick / Snare / Toms / Hi-Hat / Cymbals / Noises`, `BaySickGuitars`, `BaySickBasses` | Karoryfer's kit photograph, ARIA control-panel artwork, program names ("Green Keyswitch", "Darkblack Keysw") | Karoryfer (CC-BY - attribution required, not prohibition) | |

## G6 - attribution and notices (not marks)

These pair with `Plans & Specs/DSP Portability Matrix.md` section 10 (the
credits-file gap, fontaudio's missing OFL / CC BY texts, the ASIO SDK in git,
IR / filmstrip provenance).  One credits surface closes most of both lists.

| # | Item | Where | Ruling |
|---|---|---|---|
| 24 | Help > About says "Built with JUCE 7" (we ship JUCE 8) and credits sfizz + LAME only; the code comment calls the list INCOMPLETE - missing NAM core (MIT), Rubber Band (GPL), Signalsmith (MIT), WORLD (BSD), Karoryfer (CC-BY), VSCO, Steinberg VST3 / ASIO | `StandaloneEditor.cpp:12040-12047` | |
| 25 | No VST or ASIO trademark acknowledgement ships anywhere (the VST3 SDK and ASIO SDK licences call for one).  "ASIO" is also used as OUR record-mode name even when Windows Audio is the driver | `TRANRM.html`, `GlobalTransportBar.cpp:469`, `StandaloneApp.cpp:951` | |
| 26 | Third-party library names as the pitch-engine picker labels: "Rubber Band - Balanced", "Signalsmith - Lightest (Low CPU)", "WORLD - Highest Quality (High CPU)" | `BaySickAlignEditor.cpp:822-824`, `BaySickPitchEditor.cpp:1605, 2838`, `LibraryPitchShifters.cpp:219, 348`; manual `BSPIT` | |
| 27 | Microsoft nominative strings (installer pages, "Windows Audio", "Show in Explorer", "Recycle Bin") | `Installer/*.nsi`, `StandaloneApp.cpp`, `BuilderPage.cpp:1196`, `ProjectBrowserWindow.cpp:309` | |

## G7 - generic-use terms that are also someone's mark

| # | Item | Where | Ruling |
|---|---|---|---|
| 28 | Bare 303 / 808 / 909 / 606 folder and file names (~60 files); the default drum `"808 Group/808 Kick.xml"` | `Presets/*`, `StandaloneEditor.cpp:9384` | |
| 29 | "SUPERSAW" waveform label + "3-5 = classic supersaw stack." tooltip + Supersaw Lead / Supersaw Chords / Detuned Supersaw Lead / JP-8000 Supersaw Pad | `BaySickBassEditor.cpp:90,299`, `BaySickSynthEditor.cpp:90,301`, both processors, `BaySickVisualizerScreen.cpp:248` | |
| 30 | Reese folder + 9 files; Acid Hoover; VHS Keys x2; SID Chip x2; Outrun x3; Trumpet (Harmon Mute); Upright Piano (VS) - Versilian, uncredited | `Presets/*` | |
| 31 | Pedal names that equal the modeled products' own names: Blues Drive (Boss BD-2 Blues Driver), Bass Driver (Tech 21 SansAmp Bass Driver), Bass Overdrive (Boss ODB-3), Acoustic Simulator (Boss AC-3), Acoustic Preamp (Boss AD-2) | `BaySickPedalsEditor.cpp:530-549`, figures `Pedal *.png`, manual `BSPDLP`, registry IMP-17 / 29 / 30 / 32 / 33 | |
| 32 | Polyphonic Synth voice picker reproduces the SY-1's 11 categories | `EffectEditorPanels.cpp:5294-5306` | |
| 33 | Compressor vocabulary is the 1176 / LA-2A's: "All-buttons in", "Peak Reduction", "Compress / Limit", "+4 / +8 / +10" | `EffectEditorPanels.cpp:441-648` | |
| 34 | Vocal vocabulary is Auto-Tune's: "Retune ms", "Humanize", "Throat", "Formant Preserve"; `BSV.html`: 'the difference between "tuned" and "auto-tuned"' (Antares' registered mark used as a verb) | `BaySickVocalEditor.cpp:188-205`, `BaySickPitchEditor.cpp:1645, 1728`, manual `BSV`, `BSPIT` | |

## Not flagged, on purpose (overrule if you disagree)

"Arp" meaning arpeggio; "RAT-B" (Ratio B); "Pendulum I-V"; "Boss Battle Bass"
(video-game term); ordinary-English FL vocabulary (Playlist, Ghost Notes,
Stamp, Chop, Glue, Strum, the progression names); Maximizer / Glue / Auto MU;
the Mic Sim display names (verified generic in the binary); Dreadnought /
Parlor / Jumbo; Fuzz modes Gated / Germanium / Octave; standards bodies (EBU,
ATSC, BS.1770, General MIDI); Talkbox / Vocoder; slang and genre names
(Lasersaw, Donk, Wub, Motorik, Psybient, Hyperpop, Boom-Bap); Karoryfer program
names shown as loaded content; device-name matchers and comments; CSS font
names; MMCSS "Pro Audio"; our own names and the "<letters> Style" aliases.

## Not covered

The 4 GB Core Library is fetched at first run and is not in the repo - the
sample, SFZ and program names inside the ten packs (Karoryfer, VSCO) were not
read.  `Presets/BaySickNAMIR/NAM/NPN-1.nam` has no readable provenance.  About
134 figure PNGs were not opened (menus, panels, EQ, mixer, builder), so a
preset list captured in one could still carry a brand.  The three PDFs were
treated as renders of `manual.html`.  A brand outside the ~600-term list inside
a pasted code comment would be missed; string literals split across lines were
read as fragments.

## Already fixed, not a call

- Harmless -> BaySickSolstice in full (QA-Solstice T1-T3).
- "Vintage LDC '87" -> "Condenser Large": the binary at `030da1ae`; the four doc
  / excerpt stragglers at `52dbfd1a` and `6b0a583f`.

## After the rulings

Renames become QA-Solstice Task 5: preset / kit / template renames go through
`Tools/gen_factory_presets.py` recipes AND the committed files (the generator
has drifted from the committed presets for 16 files - reconcile, do not
blind-run); kit files reference preset paths by name, so a preset rename
cascades into `Kits/Factory/*` and `Templates/Factory/*`; UI strings by hand;
figure re-shoot + manual regeneration for anything visible; one build gate;
one commit.  Saved projects referencing renamed presets by path will not
re-find them (pre-v1 rule, same as the engine rename).
