# Verbatim Strings - what the manuals must quote character-for-character

Harvested 2026-08-11 at QA-Manuals Task 1, straight out of the shipping source
at commit `ade5a10b`. Every string below is copied from the literal in the code,
not retyped from a screenshot and not paraphrased.

**Why this file exists.** A manual that paraphrases a dialog is a manual the
reader cannot search. Three separate errors during Task 1 came from quoting a
string from memory: `Help Index  (F1)` carries TWO spaces before the parenthesis;
the Pan Law rows all end `at center`, which two docs had dropped from Triangular
and Square; and the Compressor mode is `Pedal (Sustain)` in the menu but `Pedal`
on the button. None of those survive being retyped from a picture.

**Rules for using it.**

- Quote from here, not from a screenshot and not from another doc.
- Where a string is assembled at runtime the recipe is given rather than one
  example, because the example is not the string.
- A literal backslash-n marks a real newline inside the source literal.
- If the code changes, this file is wrong until someone re-harvests it. It is a
  transcription, not a source of truth - the source of truth is the code.

---

## A. THE MIXER HAMBURGER (StandaloneEditor.cpp:7626-7699)
Four items, in this order, and nothing else:

  1. "Pan Law"                        (submenu, rows carry hover tooltips)
       "Ramped"
       "Flat"
  2. "Master Output"                  (submenu)
       stereo-pair rows built as  <n>/<n+1> + "  (" + <name1> + " / " + <name2> + ")"
         e.g.  1/2  (Out 1 / Out 2)      [note: TWO spaces before the paren]
       separator
       mono rows built as  "Output " + <n> + " (mono)  (" + <name> + ")"
         e.g.  Output 1 (mono)  (Out 1)  [note: TWO spaces before the second paren]
       with no device open, one disabled row: "(no audio device open)"
  3. "Latency-compensate meters"      (tickable, item id 201)
  4. "Multi-core Rendering"           (tickable, item id 202)

The hamburger button itself is a TitleStripMenuItem whose text is "Menu",
tooltip "Page menu" (SharedUI.cpp:1260-1262).

## B. REFUSAL DIALOGS ON FILE-LOAD PATHS
(\n shown where the source literal has a newline)

B1. Missing / substituted files after any load  (MissingFileReport.h:99-111)
    Icon:  WarningIcon        Button: "OK"
    Title: "Missing files"
    Body (assembled):
      "This " + <noun> + " refers to files that are no longer where they were saved.\n"
      "The affected parts loaded without them, so they may be silent\n"
      "or may be playing a substitute:\n\n"
      then up to 12 lines of  "  " + <what> + ": " + <path> + "\n"
      then, if truncated,     "  ...and " + <n> + " more\n"
      then                    "\nRe-pick them on the relevant tab, or put the files back."
    <noun> is the loading gesture's own word - "project", "template", "preset".
    Entry <what> strings seen on the load paths include: "NAM model",
    "NAM model (failed to load)", "Amp IR", "Amp IR (failed to load)",
    "Mic A user IR", "Mic B user IR", "Guitar kit", "Bass kit", "Drum kit",
    "Drum kit (failed to load)", "Clip audio", "Clip audio (failed to load)",
    "VST3 instrument", "Plugin not in your added list - not loaded",
    "VST3 plugin (failed to load)", "Instrument kit refused - " + <reason>,
    "Sample reference rejected (escapes the library)",
    "Sample reference rejected (escapes My Samples)",
    "Content pack not installed", "Engine settings (corrupt data)",
    "Template preset", "BaySickPlayer sample", "BaySickPlayer sound".

B2. Kit XML load  (StandaloneEditor.cpp:9302-9324) - THREE bodies, one title
    Icon: WarningIcon
    Title: "Kit Load Failed"
    Bodies:
      "Kit file not found:\n" + <full path>
      "Kit XML could not be parsed:\n" + <full path>
      "Kit XML root tag is not BaySickKit (found: " + <tag> + ")"

B3. Template load  (StandaloneEditor.cpp:8726-8729)
    Icon: WarningIcon        Button: "OK"
    Title: "Load Template"
    Body: "Could not load template: '" + <file name> + "' is not a BaySickDAW template file (or is damaged)."

B4. NAM / IR load  (BaySickNAMIREditor.cpp:1099-1102, bodies at 1066/1094/1138/1241/1282)
    Icon: WarningIcon        Button: "OK"
    Title: "BaySickNAM/IR"
    Bodies: "Couldn't load NAM model:\n" + <err>
            "Couldn't load IR:\n" + <err>
            "Couldn't load dropped file:\n" + <err>

Also on file-load paths, worth having:
B5. Engine preset load  (BaySickSynthEditor.cpp:1099-1102, BaySickBassEditor.cpp:1063-1066)
    Icon: WarningIcon   Title: "Load Preset"   Body: "That preset file could not be read."   Button: "OK"
B6. Effect preset load  (SlotComponent.cpp:1192-1194)
    Icon: WarningIcon   Title: "Could not load preset"   Body: the returned <err>
B7. Placing a clip whose file is gone  (BuilderPage.cpp:1417-1422, 5438-5443, 5608-5613)
    Icon: WarningIcon   Title: "Audio File Not Found"
    Body: "Cannot place this clip - the file is missing:\n\n" + <path>
          + "\n\nIt may have been moved or deleted. Restore the file, or "
            "remove the entry from the browser (right-click > Remove)."

## C. THE EFFECT-PANEL (EffectSlotWindow) MENU  (EffectWindows.cpp:232-388)
Built fresh on every open; rows appear only when they apply, in this order:

  "Show Advanced Controls"      <- shown when the panel is in Basic mode
  "Show Basic Controls"         <- the same row when it is in Advanced mode
        (one row, label flips; only when the panel has advanced controls)
  "Mode: " + <modeLabel> + "..."
        <modeLabel> literals, by effect:
          Compressor : "Modern" | "FET" | "Opto" | "Pedal"
          Saturation : "Tube" | "Console" | "Tape"
          Delay      : "Echo" | "Doubler"
          Reverb     : "Plate" | "Hall" | "Chamber" | "Room" | "Booth"
          Overdrive  : "Rack" | "Pedal"
          Limiter    : "Limiter" | "Maximizer"
          fallback   : "Mode"
        so the row renders as e.g.  Mode: Modern...
  <scLabel> + "..."
        <scLabel> is "SC: Off" or "SC: " + <source strip name>
        so the row renders as  SC: Off...   or   SC: Kick...
  ---- separator (only if any of the above were added) ----
  "Presets..."
  ---- hosted plugin only, after a separator ----
  "Automate"                                   (submenu)
       "Last Touched: " + <param name>
       or, disabled: "Last Touched (move a control in the plugin first)"
       separator, then the parameter list; over 30 params it is chunked into
       submenus labelled  <start> + " - " + <end>   e.g.  1 - 30
       if the plugin reports none: disabled row "(no parameters reported)"
  "Run bridged (separate process)"             (tickable)
       or, disabled when locked: "Run bridged (" + <lockReason> + ")"
       toggling raises an InfoIcon box titled "Plugin Bridge" whose body is
       "This takes effect the next time the plugin loads."
  "Retry Loading Plugin"                       (only while the plugin is not alive)
  ---- standard tail (SharedUI.cpp:1562-1586), after a separator ----
  "FX Rack"        (only on pages that set it; NOT on an effect window)
  "Visual"         (only when the effect's hasVisual() is true)
  Freeze row       (only on pages that set it; NOT on an effect window)

RECON CORRECTIONS: "Show Advanced Controls", "Mode: Modern...", "SC: Off..."
and "Presets..." are all correct as recon'd. "Visual" is correct but is NOT
part of this menu's own block - it comes from appendStandardItems, after a
separator, and is absent (not greyed) when the effect has no visual.

## D. THE SIDECHAIN PICKER MENU  (SlotComponent.cpp:354-394)
  "Off"                                              (ticked when scPick < 0)
  one row per active receive line, labelled with the source strip's name
     (falls back to "Ch " + <id> if the resolver returns empty)
  when NO line is active: a separator, then one DISABLED row reading exactly
  "(no sidechain cables routed to this strip)"
Button form (Vocal Chain header / rack row): text "SC: Off", tooltip "Sidechain source".

## E. THE MIXER "+" ROUTING MENU  (MixerPage.cpp:650-835)
  "Send..."            submenu; last row is "New Aux Strip"
  "Sidechain..."       submenu
  "Move Output..."     submenu   } omitted entirely on a main-out-locked strip
  "Add Main Out..."    submenu   } (Master, every bus, Rusty inserts)
  "Remove Main Out..." submenu   } line 0 shown disabled as <dest> + "  (main output)"
Button tooltips: "Add send cable from this strip" on a normal strip;
on Master the button text is "Analyzer" with tooltip
"Open the master analyzer - loudness, spectrum and the render report".

## F. THE PRESETS MENU  (SlotComponent.cpp:1130-1243)
  "Save Current Preset..."   -> AlertWindow titled "Save Preset", message
                                "Name this preset:", editor placeholder
                                "Preset name", buttons "Save" / "Cancel"
  "Load: Factory"            submenu; empty state "(no factory presets)"
  "Load: My Presets"         submenu; empty state "(no user presets yet)"
  ---- separator ----
  "Restore Defaults"
  "Save Current as Default"
  ---- separator ----
  "Manage Presets... (open folder)"
Failure boxes: "Could not save preset" / "Could not load preset" / "Could not save default".

## G. THE SFZ AND NAM REFUSAL REASONS (SafeSfzKit.h, SafeNamModel.h)

Only TWO of the five `Safe*` gates produce a reason string at all. `SafeXml`,
`SafeAudioReader` and `SafeAudioFormats` are boolean gates with no reason
channel, so a refused project.xml or a refused audio file surfaces as exactly
the same box a MISSING one would. **Do not write a manual sentence promising a
reason on those paths.**

### `SafeSfzKit::rejectReason` (SafeSfzKit.h)

    "the kit file is missing"                                        <- DEAD, unreachable
    "its #include chain is nested too deeply"
    "it pulls in too many files through #include"
    "its #include chain refers back to itself"
    "one of its files is too large to be an SFZ"
    "it contains binary data where text was expected"
    "one of its $macros expands to another macro, which never terminates"
    "it includes a file from a network share"
    "it includes a file from outside the kit folder"                 (two call sites)
    "it loads a sample from a network share"
    "it uses a sample format we cannot load safely (" + <extension> + ")"
    "one of its opcodes uses an absurd index (" + <digits> + ")"

Limits: `#include` depth 8, `kMaxIncludeFiles` **4096** (raised from 256 on
2026-08-11 because the shipped Big Rusty Drums `01-full.sfz` needs more than
256 visits and was being falsely refused), 16 MB per file, opcode index 512.

**Where the reason actually surfaces: nowhere the user is looking.** All three
call sites (`BaySickGuitarsProcessor::loadKit`, `BaySickBassesProcessor::loadKit`,
`BaySickRustyDrumsProcessor::loadKit`) bank it with
`MissingFileReport::add ("Instrument kit refused - " + why, path)` and return
false. Neither `InstPage::switchSfizzProgram` nor
`BaySickRustyDrumsPage::loadProgram` opens a `MissingFileReport::ScopedGesture`,
so the interactive user sees only the reasonless "Could not load program:" box
and the reason sits in the store until an unrelated gesture drains it - under
that gesture's noun and under a headline claiming the file is no longer where it
was saved, which is untrue of a refused file.

### `SafeNamModel::rejectReason` (SafeNamModel.h)

    "the file does not exist"                                        <- DEAD, unreachable
    "the file is empty"
    "its '" + <field> + "' value is out of range (" + <n> + ")"
         <field> is one of: hidden_size, channels, input_size, head_size,
         output_size, kernel_size, condition_size
    "it declares too many layers (" + <n> + ")"
    "it declares too many dilations (" + <n> + ")"
    "one of its dilation values is out of range"
    "its dilations would take too long to warm up"
    "it declares too many layer blocks"
    "it declares too many submodels"
    "its model definition is nested too deeply"
    "it declares an impossible sample rate (" + <n> + ")"

Limits: 64 layers, 8192 dimension, 8 nesting levels, dilation cap 1048576,
sample-rate window 8000-768000.

NAM reasons DO reach a dialog on the interactive paths (section B4 above), but
they are **discarded on project restore** - `BaySickNAMIRProcessor::setStateInformation`
writes the reason only to a Debug-only file log and reports the bare
`"NAM model (failed to load)"`, and `NAMPedalStyleDSP::setState` never reads its
`err` local at all.

**The two dead strings** are `"the kit file is missing"` (SafeSfzKit.h) and
`"the file does not exist"` (SafeNamModel.h). Both are unreachable because all
five call sites test `existsAsFile()` immediately before calling `rejectReason`.
Do not document either.

### The other three gates, for completeness

- `SafeXml`: refuses empty text, anything containing `<!DOCTYPE` (case-insensitive,
  anywhere in the document), and nesting past `kMaxDepth = 512`. No reason string.
- `SafeAudioReader`: `kMaxChannels = 32`, bits 1-64, sample rate >0 and <=768000,
  `kMaxBytesPerFrame = 5760`. No reason string - a refusal is a null reader, and
  most call sites absorb it silently.
- `SafeAudioFormats::registerAll`: the ONE registration point for input formats,
  replacing `registerBasicFormats()` at 19 call sites across 11 files.

---

## H. THE HELP MENU AND THE MANUALS WINDOW

Help menu (`StandaloneEditor.cpp:11783-11791`), in order:

    "Help Index  (F1)"            id 601   <- TWO spaces before the parenthesis
    "Key Binds..."                id 603
    ---- separator ----
    "About BaySickDAW v1.0"       id 602

The rebindable command (`KeyBindings.cpp:88-91`), General category:

    name        "Help Index"
    description "Open the BaySickDAW manuals. Same as Help > Help Index."
    default key F1

The manuals window (`ManualsWindow.cpp`):

    window title  "BaySickDAW Manuals"
    content       <exe dir>/Manuals/index.html, via ManualsWindow::manualsIndexFile()
    button        "Open Manuals Folder"
    placeholder body, shown when index.html is absent:
      "The manuals are not installed.\n\nThey belong at:\n" + <full path> +
      "\n\nA full install places them there. If you are running a development "
      "build, the manuals are built separately and staged into that folder."

The About box (`StandaloneEditor.cpp:11958-11961`) - **knowingly incomplete on
attributions; QA-LegalReview owns the full audit, so do not quote it as the
license list**:

    title  "BaySickDAW v1.0"
    body   "BaySickDAW v1.0\nBuilt with JUCE 7  |  (c) KnowledgeBase Studios\n\n"
           "Powered by:\n  - sfizz (BSD 2-Clause) - SFZ player engine\n"
           "  - LAME (LGPL) - MP3 encoding"
    button "OK"

---

## I. THE MASTER ANALYZER WINDOW

Window title `Master Analyzer`, persist key `analyzer:master`, opened only from
the Mixer MASTER strip's button - whose text is the literal `Analyzer` where
every other strip reads `+`, tooltip
`Open the master analyzer - loudness, spectrum and the render report`.

Six readout labels, exactly: `MOM`, `SHORT`, `INT`, `LRA`, `PEAK`, `TRUE PK`.
`PEAK` turns orange above -0.1 dBFS; `TRUE PK` turns orange above -1.0 dBTP.

Window Menu:

    section "View"
      "Loudness"
      "Spectrum"
    section "Source"
      "Live"
      one row per captured take, or the disabled row "No captured takes yet"
    section "Target"
      "Streaming (-14 LUFS)"
      "Streaming (-16 LUFS)"
      "EBU R128 (-23 LUFS)"
      "ATSC A/85 (-24 LKFS)"
      "Custom..."   or   "Custom (-18.0 LUFS)..."  when no preset is ticked
        -> prompt window titled "Custom Target",
           message "Target loudness in LUFS (-40 to 0):", OK / Cancel
    ---- separator ----
    "Reset history"

Export button text is `Export Take...`; its chooser is titled `Export take`;
failure box `Export failed`; a take with no audio reports `No audio for this take`.

**The recon's Target names were given without their parentheses. The source
literals carry them** (`Source/DSP/LoudnessSpec.h:54-57`). ITU-R BS.1770-4 is
deliberately absent from the list.


---

## THE EQ WINDOW (QA-EqFlagship 2026-08-27, EffectWindows.cpp + EqWindowUI/)

Tab pair: `Pre EQ` / `Post EQ`.  Window titles: `<Strip> - Pre EQ` /
`<Strip> - Post EQ` (falls back to `Pre EQ` / `Post EQ` with no strip name).

View row segments: `ST` / `MID` / `SIDE`.  Chip row: bands numbered
`1`..`96` in pages of 24 (page arrows are drawn triangles, no text), the
morph strip's end letters are `A` / `B`, the add control is `+`, the A/B
pill reads `A` or `B`.

Rail captions: `BAND <n>` / `NO BAND`; knob captions `GAIN` `SAT` `PAN`;
numbers `FREQ` (the caption becomes the live note name, e.g. `A4` /
`A4 +13c`, while a band is selected) `Q` `SLOPE` `PHASE`; dynamics header
`DYNAMICS`; toggles `DYN` `AUTO` `EXT`; direction segments `DOWN` / `UP`;
dyn knob captions `THR` `RATIO` `ATK` `REL` `THR B` `RATIO B` `RANGE B`
`ONSET` `DENSE`; the MOD block: `RATE` `LFO` `ENV` with two `F`/`G`/`Q`
target rows; the pan readout is `C` or `L<n>` / `R<n>`, the release
readout shows `auto` under AUTO.  Slope menu rows: `6 dB/oct` `12 dB/oct`
`18 dB/oct` `24 dB/oct` `36 dB/oct` `48 dB/oct` `72 dB/oct` `96 dB/oct`
`Brickwall`.

Band menu: submenus `Type` (`Bell` `Low Pass` `High Pass` `Low Shelf`
`High Shelf` `Notch` `Band Pass` `Tilt` `All Pass`), `Slope` (rows
above), `Channel` (`Stereo` `Left` `Right` - Stereo view only), `Move to`
(`Stereo view` `Mid view` `Side view`), `Dynamic` (`Make Dynamic` /
`Auto Release` / `Spectral (linear modes)`); rows `Listen` `Isolate`
`Delta Listen` `Mute`; sep; `Split to Left + Right` (Stereo-view stereo
bands), `Link Selected Bands` (a multi-selection exists), `Unlink` (the
band is linked); sep; `Reset Band` `Delete Band`.

Window menu: `Processing Mode` (rows `Zero Latency` `Natural Phase`
`Linear Low` `Linear Medium` `Linear High` `Linear Very High`
`Linear Maximum` `Mixed Phase`, each suffixed `   (<n> ms (<n> sp))`
computed at the session rate, or `   (0 ms)`); `Oversampling 2x`;
`Proportional Q`; `Color` (`None` `Smooth` `Warm` `Changed`, then `25%`
`50%` `75%` `100%`); `Auto-Gain` (submenu: `Auto-Gain`, then `25%` `50%`
`75%` `100%`); `Output Trim` (`Polarity Flip`, then `-12 dB` .. `+12 dB`);
`Gain Scale` (`+/-3 dB` .. `+/-30 dB`); `Analyser` (`Pre` `Post`
`Sidechain`; `Fast` `Medium` `Slow`; `Tilt 4.5 dB/oct` `Tilt 3 dB/oct`
`Flat`; `Freeze` `Peak Hold`); `View` (`Analyser` `Spectrogram`
`Phase Overlay` `Piano Strip`); `Whole Curve` (`Scale -100% (invert)`
`Scale -50%` `Scale 50%` `Scale 75%` `Scale 100%` `Scale 125%`
`Scale 150%` `Scale 200%`; `Shift -12 st` .. `Shift +12 st`;
`Reset Transforms`); `Delta Listen`; `Sketch a Curve...`;
`Instances...`; `Keyboard & Mouse...`; `Reset All Bands`; `EQ Match...`;
`Presets` (`Default` first, factory categories `Cleanup` `Vocals`
`Drums` `Bass` `Master` as submenus, user presets, `Save Preset...`).

EXT picker rows: `This band's own input`, then `Receive <n> - <source>` or
`Receive <n> - not routed`.

Sketch overlay: "Sketch: draw the curve you want. Right-click cancels."

Match panel: title `EQ MATCH`; buttons `Capture Current`,
`Load Current Export...`, `Scan Track / Selection`,
`Capture Reference (SC)`, `Load Reference File...`, `Stored Spectra...`,
`Match`, `Auto Cleanup`, `Close`; slider labels `DETAIL` `AMOUNT` (the
amount readout is `<n>% of the fit`); scan status `Scanned: whole song,
<m>:<ss>` / `Scanned: selection, <m>:<ss>`; cleanup tally `Cleanup: <n>
cuts, <m> dynamic - ordinary bands, one undo step.` / "Nothing stands out
over its own neighborhood - no cleanup needed."; match tally recipe
`<n> stereo, <n> mid, <n> side, <n> dynamic  -  residual <x> dB RMS
(was <y>)`.  Stored Spectra menu: `Save Current As...`,
`Save Reference As...`, then the stored names.  Save prompt: title
`Save Spectrum`, message `Name it:`.  Scan overlay title
`Scanning for EQ Match...`.

Instances panel: title `EQ INSTANCES`; subtitle "Click re-points this
window. Right-click for more."; row tag `PRE` / `POST` (plus `MATCH REF`
/ `COLLISION` when picked); row menu `Open in Its Own Window`,
`Match Reference`, `Collision Reference`; `Close`.

Save preset prompt: title `Save EQ Preset`, message `Name it:`, buttons
`Save` / `Cancel`.  Keyboard card title `Keyboard & Mouse` (body is the
seventeen-line map in EffectWindows.cpp; quote from source when needed).

Empty-state and dot decorations: L/R badge letters `L` / `R`; the grab
readout suffix is `  grab` after the Hz figure; the drag readout carries
the note name after the frequency.
