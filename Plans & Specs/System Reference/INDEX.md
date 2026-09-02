# System Reference - Index

This directory describes what BaySickDAW **is** and what it **does**, system by
system. It is the source material the user manuals are written from, so every
document leads with plain-English coverage of what a person sees on screen and
what each control does, and puts the implementation notes around it for whoever
has to maintain the thing.

Read one document cold and you should be able to write the manual chapter for
that system without opening the code.

**The code is the authority.** Every statement in these documents was checked
against the shipping source. Where a document could not settle a question it says
"not determined" rather than guessing - an honest gap is worth more than a
confident error, because a manual writer cannot tell the two apart. Each document
also ends with a **Differs from Carry-Forward** section recording where it departs
from the frozen architectural snapshot in the parent folder
(`Plans & Specs/Carry-Forward Reference.md`), which is deliberately not updated;
where the two disagree, these documents and the code win.

**One fact to know before anything else: the Core Library is installed separately
from the application.** It is BaySickDAW's factory sound content - drum kits,
keys, strings, brass, the sampled guitars and basses, and the Rusty Drums kit -
and it lives at `%LOCALAPPDATA%\BaySickDAW\CoreLibrary\`. The app never creates
that folder; it only reads it. On a machine where it has not been installed, the
app runs perfectly well but the sampled instruments come up empty: a BaySickGuitars
or BaySickBasses tab opens with no program loaded, a Rusty Drums program pick
reports that the file could not be found, and the in-app Core Library browsers on
the sampler pages have no packs to list. Nothing in the app downloads the library.
That single fact explains the most confusing first-run experience a new user can
have. See `Sample Library.md`.

---

## Start here - the shell everything else lives in

| Document | Covers |
|---|---|
| `Workspace and Windows.md` | The fixed full-screen frame and the movable windows inside it: the application menu bar and its Help menu, the manuals window, opening, closing, dragging, resizing, magnetism, the effect/visual tether, and where window positions are remembered. |
| `Transport and Playback.md` | Play, stop, record, tempo, the position readout, Pattern vs Song mode, looping, the metronome and count-in, and what a recording captures. |
| `Keyboard Shortcuts.md` | Every rebindable command and its default key, the Key Binds window, and a transcription of every page-local key and mouse gesture in the app. |
| `Projects and Saving.md` | A project is a folder. New / Open / Save / Save As, the unsaved-changes prompt, autosave and backups, and the full per-project vs per-machine split. |
| `Undo History.md` | The one app-wide undo history, Ctrl+Z, the History window, what is and is not undoable, and the depth setting. |
| `MIDI Learn.md` | Binding a hardware knob or pad to a control, the 30-second learn window, per-drum triggers, and where the bindings are stored. |

## Making a track - the tab families

| Document | Covers |
|---|---|
| `Engine Tabs (Layers, Bass, Drums).md` | The three note-driven tab families: the ribbon "+" menu, the page menu, the Drum Kit grid, and the two independent kits of sixteen drums. |
| `Clips Page.md` | One audio file turned into a playable tab, with its own mixer strip, effects rack and piano roll. |
| `Inst Page.md` | The guitar and bass rig tab: live input or a sampled instrument, into a pedalboard, into an amp and cabinet. |
| `Plugins Page.md` | Hosting a third-party VST3 instrument, the Options > Plugins manager, and the out-of-process bridge. |
| `Vox Page.md` | The vocal track: recording a take, monitoring, the four take types, and the four editor windows a Vox tab owns. |

## The instruments

| Document | Covers |
|---|---|
| `BaySickSolstice.md` | The additive synthesizer. Part A and Part B as simultaneous layers, the modulation matrix, and every box on its single-view panel. |
| `BaySickSynth.md` | The subtractive synthesizer used on Layers and Drums tabs: oscillators, filter, envelopes, LFO and modulation across six panel tabs. |
| `BaySickBass.md` | The same engine tuned for bass, with the defaults that make it sound like a bass on the first note. |
| `BaySickPlayer.md` | The sample player: loading a folder, a single file or an SFZ, the voice pool, and the per-voice signal path. |
| `BaySickGuitars.md` | The sampled electric guitar on an Inst tab: kit loading, the ARIA control panel, the program picker, and slides and bends from the piano roll. |
| `BaySickBasses.md` | The sampled electric bass on an Inst tab. Same shape as the guitars, with the bass-only differences called out. |
| `BaySickRustyDrums.md` | The multi-microphone drum kit: the kit graphic and its 25 zones, the note map, the section tabs, and the per-piece mixer strips. |

## Writing the music

| Document | Covers |
|---|---|
| `Builder Page.md` | The arrangement timeline: tools, snapping, track rows, the ruler and its markers, the browser, drag-and-drop, and the Export Audio dialog. |
| `Piano Roll.md` | Entering and editing notes: the tools, the five note types, the note-properties popup, the control lane, scales and chords, and swing. |
| `Patterns and Arrangement.md` | What a pattern is, Pattern vs Song mode, the pattern list, placing blocks on the timeline, and time-signature handling. |
| `Event Editor.md` | The close-up editor for one automation clip: drawing the curve, the curve types, LFO mode, and importing MIDI CC data. |
| `Automation.md` | How a control moves by itself: creating a lane, what can be automated, song-mode replay, and how a lane survives the thing it drives being rebuilt. |

## Mixing, effects and tone

| Document | Covers |
|---|---|
| `Mixer.md` | The console: channel strips, buses, faders and pans, solo, routing cables, sends and sidechains, and the input pickers on Vox and Inst strips. |
| `Effect Racks.md` | The six-slot effect rack every channel owns: loading and reordering effects, the windows, sidechain picks, presets, and hosted VST3 effect slots. |
| `Effect Modules.md` | Every loadable effect, with a full control table for each: range, default, and what moving it does to the sound. |
| `EQ.md` | The 8-band parametric EQ - two per channel, one before the rack and one after: the graph, the band menu, dynamic bands, and the phase modes. |
| `Pedalboard.md` | The 8-position guitar/bass pedal chain on an Inst tab: the locked Tuner and EQ positions, the six free ones, and the board presets. |
| `NAM Amp and Cab.md` | The amp capture, cabinet impulse response and two virtual microphones, with the ten built-in mic voicings and their placement controls. |

## The vocal chain

| Document | Covers |
|---|---|
| `Vocal Chain.md` | The six-stage vocal rack (Gate, De-reverb, De-esser, Compressor, Saturation, Limiter), the realtime pitch board, and the A/B compare. |
| `Pitch Editor.md` | BaySickPitch: correcting a sung take note by note, the pill canvas, the toolbar, the gesture map, and the per-note sub-editor. |
| `Align Editor.md` | BaySickAlign: pulling one vocal's timing onto another's, the three lanes, sync points, and the alignment controls. |

## Content, files and output

| Document | Covers |
|---|---|
| `Sample Library.md` | The Core Library and My Samples, how a sample path is stored so it survives being moved, and when a file is copied into a project. |
| `Presets.md` | The four preset families - page preset, engine patch, effect preset, FX rack preset - what each captures, and the save flow. |
| `Templates.md` | Saving a whole starting-point rig and loading one into a new project, and what a template does and does not carry. |
| `Project Bundles.md` | Packing a project up to send somewhere else, what travels with it, and what is deliberately left out. |
| `Freeze and Export.md` | The one offline render engine, freezing a track to reclaim CPU, and the Export Audio dialog, stems and loudness measurement. |

---

## How to read a document

Every document follows the same shape:

| Section | What it is for |
|---|---|
| **Purpose** | Two to four sentences: what the system is and why it exists. |
| **How it operates** | The mechanism - signal flow, ownership, which thread does what, and the real class and file names. Skip it if you are writing user-facing copy. |
| **User-facing behavior** | The manual section, and the longest one. Every control the system exposes: its on-screen name, what it does, its range, units and default, and what changing it sounds or looks like. |
| **Parameters and persistence** | Parameter ids and state shape; what is saved with a project, with a preset, per machine, or not at all. |
| **Lifetime and teardown** | When the thing is created and destroyed, what owns it, and any ordering that matters. |
| **Cross-references** | The sibling documents to read next. |
| **Differs from Carry-Forward** | Where this document departs from the frozen architectural snapshot, stated plainly. Some documents say only that the snapshot does not cover the system. |

A few documents add a short **Not determined** list where the code did not settle
a question. Those are real gaps, not oversights - do not paper over them in a
manual.

---

## Two files here that are not system documents

| File | What it is |
|---|---|
| `Verbatim Strings.md` | Every menu item, dialog title and dialog body the manuals must quote character-for-character, transcribed from the source literals. **Quote from there, never from a screenshot** - three separate Task 1 errors came from retyping a string from a picture. |
| `Manual Pipeline.md` | The generated-manual operating manual: the bsd_shot figure harness (--shot / --docs), the headless menu composer + capture hook, the control tables + blurbs, dot generation with its exception list, and the full regeneration workflow. |
| `MANUAL-1 Screenshot List.md` | **Not a capture plan.** The 723-shot plan and its `SHOT-###` ids are retired; the file survives as the code-read ELEMENT INVENTORY that callout lists are built from. Read its `Visible:` lines and its APPENDIX; ignore the sittings, counts and time budgets. |

**Still to be written:** `Master Analyzer.md`. The window is in scope (SC-13) and
its full string set is already harvested in `Verbatim Strings.md` section I, but
the document itself lands with Manual 2's mixing chapter. Its INDEX row goes in
alongside it, so that this index never points at a file that is not there.
