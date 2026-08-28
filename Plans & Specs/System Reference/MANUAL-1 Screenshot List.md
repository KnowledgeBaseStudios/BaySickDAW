# MANUAL 1 - Element Inventory (formerly the Visual Atlas capture plan)

> **STATUS: RETIRED AS A CAPTURE PLAN. Kept as the ELEMENT INVENTORY.**
> **Every `SHOT-###` id below is retired. Do not cite one anywhere.**
>
> Jeff killed the 723-shot capture plan on 2026-08-09. Manual 1 is a set of
> roughly 53 densely annotated screens with **callout ids** (`<SCREEN>-<n>`,
> e.g. `MIX-14`) as the footnote anchors, state variants carried as insets and
> captions rather than as separate images. The complete figure set is the 53
> images in `Pictures/`, and it is closed. Recorded as spec calls SC-1, SC-2 and
> SC-3 in
> [`Plans & Specs/Batch Plans/lucid-annotating-lemur.md`](../Batch Plans/lucid-annotating-lemur.md).
>
> **What this file still is, and why it survives:** the code-read enumeration of
> every visible element in the app, produced by a seven-agent sweep. That is
> exactly the raw material a callout list is built from, and nothing else in the
> repo carries it. Read the `Visible:` lines as an element inventory; ignore the
> `Reach:` lines, the sitting structure, the shot counts and the time budgets -
> all of those describe a capture campaign that will never run.
>
> **The trap this banner exists to stop:** a reader who takes the numbers below
> at face value concludes Manual 1 needs 723 images and cannot be written. A
> session did exactly that on 2026-08-11 and reported the batch blocked. Do not
> repeat it.
>
> The single most useful page here for Manual 1's prose is the **APPENDIX** at
> the bottom: invisible hit targets, live-looking controls that do nothing, the
> native-vs-styled dialog split, and the one-per-app elements.

Compiled 2026-08-08 from a seven-agent parallel enumeration of the whole UI, then
deduped and re-ordered for capture. Source areas: shell-and-menus, system-pages,
synth-editors, sampler-and-drums, vocal-suite, effect-panels, dialogs-and-windows.

---

## THE NUMBER (historical - the plan this describes is retired)

**723 shots.**

- **684** reachable by ordinary navigation (Sittings 1-28).
- **39** fault-state shots that need something broken/missing on disk first
  (Sitting F, at the end).

Raw agent output was ~826 entries; ~103 were duplicates of shared surfaces
(title strips, preset menus, the Automate right-click, the shared collision
prompt, effect panels that appear in both the rack and the vocal chain, the
menu bar counted by two agents, etc.). Those are merged, and the surviving shot
carries a note where a second area needs to footnote it.

**That number is not padded and it has not been trimmed to look manageable.**
Every entry is a surface that looks meaningfully different from its neighbours.
Cutting one costs a manual page that cannot be written.

### Sittings (historical - never run)

**28 navigation sittings + 1 fault-state sitting = ~29-32 sessions.**

Average 24 shots per sitting; the biggest (Sitting 8, Builder) is 59 and the
smallest (Sitting 18, shared idioms) is 4. At roughly 1.5-2 minutes per shot
including setup and re-staging, budget **60-120 minutes per sitting** and
**20-26 hours total**. The fault sitting is the slowest per-shot because almost
every entry needs a save, a file move, and a relaunch.

### How the ids work (RETIRED)

**`SHOT-001` .. `SHOT-723` are all retired.** No manual cites them and none ever
will. Manual 1 anchors on callout ids of the form `<SCREEN>-<n>` (`MIX-14`,
`SYN-OSC-3`), assigned once per figure, append-only within a screen, and retired
rather than reused; Manuals 2 and 3 footnote against those. The join table is
`Callout Registry.md` in this folder.

The original text of this section, kept so the retirement is legible: the ids
were sequential and stable, manuals 2 and 3 were to footnote against them, a
dropped shot retired its id, and a late addition appended at SHOT-724+ rather
than inserting. That scheme was replaced wholesale, not amended - keeping both
would leave two competing anchor namespaces pointing at the same controls.

### Conventions used below

- **Reach** — click-by-click from a known state. No codebase knowledge assumed.
- **Visible** — the elements a reader needs labelled, roughly in layout order.
- **Why separate** — only present when the shot is a variant of another.
- **SETUP** at the head of a sitting means do that once before starting.
- "Ribbon" = the tab strip inside the global transport bar at the top of the frame.
- "Window Menu" = the `Menu` heading at the left of a contained window's title strip.

---

## SITTING 1 — Cold launch, bare shell

**SETUP:** Fresh profile if possible (rename `Documents\BaySickDAW\settings.xml`
aside). No project loaded. Do not open any window.

**SHOT-001 — Launch splash screen**
- Reach: Double-click `BaySickDAW.exe`, capture within the first ~4 seconds. It is a self-deleting splash, so one attempt per launch.
- Visible: BaySickDAW logo rendered at 512x512 on a borderless window with a drop shadow. No progress text, no chrome, no version string.

**SHOT-002 — Main frame, full fullscreen, fresh launch**
- Reach: Let the app finish loading. Do not open a project. Capture the whole screen.
- Visible: (1) OS title bar "BaySickDAW - Untitled" with the BaySick logo as window icon; (2) the 24px master menu bar strip — File, Edit, Patterns, View, Options, Help; (3) the 40px brushed-aluminium global transport bar spanning full width; (4) the workspace filling the rest, flat 0xFF141417, empty. The frame is FIXED fullscreen and non-resizable — there are no drag edges on the app frame itself.

**SHOT-003 — Workspace empty backdrop**
- Reach: Same state as SHOT-002, cropped to the workspace region only.
- Visible: Flat 0xFF141417 fill with nothing in it. No empty-state message, no placeholder text, no instructions.
- Why separate: A beginner who closes every window sees exactly this and needs told that closing a window does not stop the engine and the ribbon brings it back.

**SHOT-004 — OS window title bar, four states**
- Reach: Four crops of the top OS title bar: (a) fresh launch; (b) after File > New Project... and naming it; (c) after moving any knob so the project goes dirty; (d) running the Debug exe from `build\BaySickDAWStandalone_artefacts\Debug\`.
- Visible: "BaySickDAW - Untitled" / "BaySickDAW - <ProjectName>" / trailing " *" unsaved marker / trailing " [DEBUG]". BaySick logo as window icon. Standard Windows minimise / maximise / close buttons.

**SHOT-005 — Ribbon tab bar, fresh launch (required tabs only)**
- Reach: Crop the ribbon region of the transport bar with no project loaded.
- Visible: Four slots in fixed order — Builder (8-colour tie-dye gradient), Mixer (purple), Effects (pink), Piano Roll (near-black) — then a narrow trailing "+" slot drawn as a faint white-10% block with a bold white "+". Builder and Effects carry a small down-chevron on the bottom row; **Mixer and Piano Roll have no dropdown at all and draw no chevron.**

**SHOT-006 — Global transport bar, full width, stopped**
- Reach: Crop the 40px bar under the menu bar with the transport stopped.
- Visible: Left to right — Play (white triangle), Pause (double bar), Stop (square), Record (white dot + divider + a small chevron in its right 14px); "BPM" label + amber LCD BPM field + "TAP"; "PATTERN" toggle + loop-mode toggle; metronome icon button + its down-chevron; typing-keyboard MIDI button (four painted piano keys); small Swing knob; pattern dropdown button; amber LCD position readout; ribbon tab bar; three-row perf readout flush right. Background is a brushed-aluminium gradient with horizontal grain, a white top highlight line and a bottom separator.

**SHOT-007 — Perf readout, normal**
- Reach: Crop the 95px three-row block at the far right of the transport bar with the project idle.
- Visible: Row 1 "SYS 12%" and "DSP 4%" as two independently coloured right-anchored tokens (green under 50%, dim yellow 50-80%, yellow above). Row 2 "MEM 412  LAT 128". Row 3 "UND 0  PF 3.2". 9pt monospaced.

**SHOT-008 — Perf readout tooltip (and the app's one tooltip window)**
- Reach: Hover the perf readout for a second and hold still.
- Visible: The single app-wide tooltip window — a parentless desktop window, so it floats above every contained window. Multi-line body repeating the live values then the full legend: SYS = system-wide CPU, DSP = audio engine load as % of buffer window, MEM = process memory MB, LAT = total reported plugin latency in samples, UND = audio clip stream underruns during continuous playback (should stay 0), PF = slowest disk read this session in ms.
- Why separate: The tooltip IS the legend — the only place those six abbreviations are defined. There is exactly ONE tooltip window in the app, so this doubles as the tooltip-appearance reference for every other hover shot in this list.

---

## SITTING 2 — Master menu bar, empty-state pass

**SETUP:** Fresh profile, no recent projects, no user templates, no default
template set, nothing undoable done yet, exactly one pattern. Capture this whole
sitting BEFORE doing anything else — several of these states are unrecoverable
once you use the app.

**SHOT-009 — Master menu bar strip, closed**
- Reach: Crop the top 24px of the frame with no menu open.
- Visible: Six headings left-aligned: File, Edit, Patterns, View, Options, Help. VC::Panel background. Note the order — **Patterns sits before View.**

**SHOT-010 — File menu, open**
- Reach: Click "File".
- Visible: New Project... (Ctrl+N) / New from Template (submenu) / sep / Open Project... (Ctrl+O) / Quick Open Project... / Open Recent (submenu) / sep / Save (Ctrl+S) / Save As... (Shift+Ctrl+S) / Save as Template... / sep / Restore from Backup... / sep / Import Audio... / sep / Export Audio... / Export Project Bundle...

**SHOT-011 — File > New from Template, no default set**
- Reach: File > hover "New from Template" with no default template configured.
- Visible: First row reads plain "New from Default Template" and is GREYED. Separator. "Premade Templates" submenu, "My Templates" submenu.
- Why separate: That row has three distinct forms — unset (greyed, no name), set (named, enabled), and set-but-deleted (named with " - missing", greyed). This is the unset one.

**SHOT-012 — File > New from Template > Premade Templates**
- Reach: File > New from Template > hover "Premade Templates".
- Visible: Alphabetical folder submenus and .xml template names with the extension stripped, recursively nested — sub-folders on disk become nested submenus.

**SHOT-013 — File > New from Template > My Templates, empty**
- Reach: File > New from Template > hover "My Templates" before saving any template.
- Visible: A single greyed, non-selectable row "(no user templates)". The Premade equivalent reads "(no premade templates)".
- Why separate: Empty-state text is the first thing a new user meets here.

**SHOT-014 — File > Open Recent, empty**
- Reach: File > hover "Open Recent" on a fresh profile.
- Visible: A single greyed row "(no recent projects)". No separator, no "Clear Recent Projects".
- Why separate: Completely different content from the populated submenu (SHOT-023).

**SHOT-015 — Edit menu, undo and redo both greyed**
- Reach: Launch fresh, do nothing, click "Edit".
- Visible: Undo (Ctrl+Z) and Redo (Ctrl+Alt+Z) greyed / History... / sep / "New Tab" (submenu) / sep / New Automation Clip. **No top-level row here is hardcoded disabled** - Undo and Redo are the only ones that ever grey, and both are state-driven. "New Tab" is not a menu of its own: it is the ribbon "+" list embedded whole, so its rows, its submenus and its greying are exactly SHOT-050 through SHOT-056 and do not need re-capturing here.

**SHOT-016 — Patterns menu, single pattern (boundary greying)**
- Reach: On a fresh project with exactly one pattern, click "Patterns".
- Visible: Rename / Color (F2) / Find Next Empty Pattern (F3) / sep / Insert One (Shift+Ctrl+Ins) / Clone (Alt+C) / Delete (GREYED — needs >1 pattern) / sep / Move Up (GREYED — already first) / Move Down (GREYED — already last).

**SHOT-017 — View menu, open**
- Reach: Click "View".
- Visible: Eight rows, each an F-key nav command with the key printed: Show Builder (F5) / Show Mixer (F6) / Show Player (Most Recent) (F7) / Show Effects Rack (Most Recent) (F8) / Show Effect Panel (Most Recent) (F9) / Show Piano Roll (Most Recent) (F10) / Show Drum Kit (Most Recent) (F11) / Show Event Editor (Most Recent) (F12). No separators, no submenus, nothing greyed. This menu doubles as the printed F-key reference.

**SHOT-018 — Options menu, open**
- Reach: Click "Options".
- Visible: General (submenu) / File Settings... / Audio Settings... / Plugins... / sep / Undo History Size (submenu) / sep / "MIDI is Omni (all devices)  -  Read Only" — a **permanently GREYED informational row, not a control.**

**SHOT-019 — Options > General, no default template**
- Reach: Options > hover "General" with no default template set.
- Visible: Plain "Set Default Template..." and a GREYED "Clear Default Template".
- Why separate: With a default set the first row's label changes to embed the filename (SHOT-026) — two visibly different menus.

**SHOT-020 — Options > Undo History Size submenu**
- Reach: Options > hover "Undo History Size".
- Visible: Four radio-style rows with a tick on the active one: "100  steps", "250  steps", "500  steps", "1000 steps". This is the canonical tick-mark convention used across the app's menus.

**SHOT-021 — Help menu, open**
- Reach: Click "Help".
- Visible: three items, exact literals `Help Index  (F1)` (note the DOUBLE space before the parenthesis), `Key Binds...`, separator, `About BaySickDAW v1.0`. (`Rusty Drums Map...` moved to the Rusty window's own Menu, 2026-08-13.)
- **CORRECTED 2026-08-11 (QA-Manuals Task 1): "Help Index" is LIVE.** It was unwired from 2026-07-29 behind a `HOLD-FOR-MANUALS-WINDOW` marker; that marker is retired and `case 601` now calls `showManualsWindow()` (`StandaloneEditor.cpp:11785`, `:11968`, `:10028`). The window is a desktop `DocumentWindow` titled `BaySickDAW Manuals`, owned by the main window and re-fronted rather than duplicated. **F1 is live too, and was never wired to anything before** - `cmdShowManuals` (`0x1001b`) is a real rebindable command in the General category, labelled `Help Index` with the description `Open the BaySickDAW manuals. Same as Help > Help Index.` (`KeyBindings.cpp:88-91`). Do NOT repeat the retired line that said this item has no handler by design.

---

## SITTING 3 — Master menu bar, populated pass

**SETUP:** Open two or three projects so they enter the recent list. Set a
default template (Options > General > Set Default Template...). Create three
patterns and select a middle one. Perform an undoable action (drag a block on
the Builder grid).

**SHOT-022 — File > New from Template, default set**
- Reach: File > hover "New from Template".
- Visible: First row reads "New from Default Template (<name>)" and is ENABLED.
- Why separate: The row embeds the current filename in its own label. (Third form — "<name> - missing", greyed — is SHOT-687 in the fault section.)
- Variant of: SHOT-011

**SHOT-023 — File > Open Recent, populated**
- Reach: File > hover "Open Recent".
- Visible: Up to 10 project folder names, newest first. Below them a separator and "Clear Recent Projects".
- Variant of: SHOT-014

**SHOT-024 — Edit menu, undo available**
- Reach: Perform any undoable action, then click "Edit".
- Visible: Undo and Redo rows now enabled; every other top-level row identical to SHOT-015. Nothing else at this level ever greys - the greyed rows inside "New Tab" are cap-driven and belong to SHOT-056.
- Variant of: SHOT-015

**SHOT-025 — Patterns menu, multiple patterns**
- Reach: With three patterns and a middle one selected, click "Patterns".
- Visible: All rows enabled — Delete, Move Up and Move Down all live.
- Variant of: SHOT-016

**SHOT-026 — Options > General, default template set**
- Reach: Options > hover "General".
- Visible: "Set Default Template... (current: <filename>)" and an ENABLED "Clear Default Template".
- Variant of: SHOT-019

---

## SITTING 4 — Transport bar states, pop-outs and pattern surfaces

**SETUP:** A project with at least three patterns, and at least two
time-signature markers placed on the Builder ruler (needed for SHOT-040).

**SHOT-027 — Transport, playing**
- Reach: Press Space (or click Play), capture the left cluster while rolling.
- Visible: Play button body fills green (0xff1ea848) as a raised tinted slab with the white triangle still on top; Stop tints toward highlight.
- Why separate: The colour fill is the ONLY "is it playing" indicator on the bar.
- Variant of: SHOT-006

**SHOT-028 — Transport, paused**
- Reach: Press Space while playing, capture the left cluster.
- Visible: Pause button body tints yellow (45% alpha); Play returns to its off slab.
- Variant of: SHOT-006

**SHOT-029 — Transport, record armed**
- Reach: Click the white dot on the Record button (or press R). Capture before pressing Play.
- Visible: Record body fills red (0xffd02020) raised slab, white dot and chevron still drawn on it.
- Why separate: Armed vs not-armed is safety-critical and is signalled by body colour alone.
- Variant of: SHOT-006

**SHOT-030 — Record button dropdown (mode picker)**
- Reach: Click the small chevron in the RIGHT 14 pixels of the Record button — not the dot.
- Visible: "ASIO" and "MIDI (piano roll tabs only)" with a tick on the active one; separator; a "Global Record-Quantize" submenu row.
- Why separate: The Record button has two hit zones — dot vs chevron — which is completely invisible until pictured.

**SHOT-031 — Record dropdown > Global Record-Quantize submenu**
- Reach: Record chevron > hover "Global Record-Quantize".
- Visible: Eleven rows with a tick on the active one: Off, Line, Bar, Beat, 1/2 Beat, 1/3 Beat, Step, 1/2 Step, 1/3 Step, 1/4 Step, 1/6 Step.
- Why separate: **This is the canonical eleven-label snap ladder** shared by the Builder, Piano Roll, Drum Kit and Harmless Mod Editor snap menus. Shoot it once here; every other snap menu footnotes back to it. ("Line" is a no-op in this particular menu.)

**SHOT-032 — Metronome button, off vs on**
- Reach: Crop the metronome button, once off and once after clicking it on.
- Visible: Off — custom-painted trapezoid metronome body + pendulum + weight on the standard slab. On — amber (0xffd8a520) raised tinted slab, glyph in white with a dark inner pendulum.
- Why separate: Icon-only control; on/off is colour-only.

**SHOT-033 — Metronome settings CallOutBox**
- Reach: Click the small down-chevron immediately right of the metronome icon button.
- Visible: A 240x122 floating CallOutBox (bubble with a pointer tail, not a window) titled "Metronome": "Sound:" + combo (Sine / Click / Wood / Bell), "Volume:" + horizontal slider with a percentage box (0-200%), "Precount:" + checkbox "1-bar lead-in when recording (Ctrl+P)". Panel background, 1px accent border.
- Why separate: A whole settings surface that exists only as a pop-out from one 18px chevron.

**SHOT-034 — Pattern/Song mode button + loop-mode toggle, both states**
- Reach: Crop the "PATTERN" button and the 28px toggle beside it; click each to flip and capture both.
- Visible: Mode button text literally changes between "PATTERN" and "SONG" and tints purple in SONG. Loop toggle draws either an arrow-into-end-bar icon (play through then stop) or a 3/4 circular arrow with an arrowhead (loop back to song start); on = purple + white, off = dim.
- Why separate: Two hand-painted icons with no text label whose meanings are not guessable.

**SHOT-035 — Typing-keyboard MIDI toggle, off vs on**
- Reach: Crop the 32px button right of the metronome chevron; press Ctrl+T (or click) and capture both.
- Visible: Four painted white piano keys with three black keys over them on a rounded slab. On: body goes dark amber-brown (0xff3A2F18), keys turn amber (0xffFFB030). Tooltip "Play notes with your computer keyboard (Ctrl+T)".
- Why separate: Purely iconographic toggle that changes how the whole computer keyboard behaves.

**SHOT-036 — Global Swing knob with value popup**
- Reach: Crop the 24px rotary between the typing-keyboard button and the pattern dropdown; drag it slightly so the popup appears.
- Visible: Small rotary plus JUCE's value popup reading "Swing 25%". Double-click resets to 0.
- Why separate: The only rotary on the transport bar, and its value is invisible until hovered or dragged.

**SHOT-037 — BPM field right-click menu**
- Reach: Right-click the amber BPM number field.
- Visible: A one-item popup: "Automate tempo". (The normal text-editor copy/paste popup is deliberately disabled here.)
- Why separate: A hidden right-click route on a field that looks like a plain number box.

**SHOT-038 — Transport position readout, both display modes**
- Reach: Crop the 100x28 amber LCD between the pattern button and the ribbon. Click it once to toggle, capture the second mode.
- Visible: Mode A bars:beats:ticks, e.g. "5:3:48" (96 PPQ, beats counted in denominator units, 1-based). Mode B minutes:seconds.milliseconds, e.g. "1:23.456". Same dark LCD body, amber monospaced digits, rounded outline.
- Why separate: One box, two completely different number formats, switched by an undocumented single click.

**SHOT-039 — Pattern dropdown button and its menu**
- Reach: Click the wide button in the transport bar showing the current pattern name.
- Visible: Every pattern listed with a tick on the current one and a "7/8"-style time-signature suffix on any non-4/4 pattern; sep; a heavy-plus row "+  New Pattern"; sep; "Rename..."; "Change Color..."; "Set Time Signature... (4/4, following)" or "(7/8, user-set)"; "Current Time Signature (new patterns)" submenu; "Delete" (greyed when only one pattern exists).

**SHOT-040 — Pattern menu > Current Time Signature submenu**
- Reach: Place at least two time-signature markers on the Builder ruler, then pattern dropdown > hover "Current Time Signature (new patterns)". **The row is GREYED until 2+ markers exist.**
- Visible: Rows reading "Bar 5  -  7/8" with a tick on the currently bound marker; up to 64 rows.
- Why separate: Greyed-until-two-markers is a state a reader will otherwise think is broken.
- Variant of: SHOT-039

**SHOT-041 — Pattern Time Signature dialog**
- Reach: Pattern dropdown > "Set Time Signature... (...)".
- Visible: AlertWindow "Pattern Time Signature" naming the pattern; two text fields (numerator, denominator); an explanatory block about numerator 1-32 and power-of-2 denominators and what Reset does; THREE buttons — "Set" (Return), "Reset to Default", "Cancel" (Escape).
- Why separate: A three-button dialog whose middle button is a mode change, not a value.

**SHOT-042 — Rename / Color dialog (F2)**
- Reach: Press F2, or Patterns > Rename / Color (F2).
- Visible: AlertWindow "Rename / Color"; body "Name and color for this pattern:"; text field pre-filled with the pattern name; an EMBEDDED colour selector — current-colour bar, colour-space square, hue strip, RGB sliders, hex field, and a row of 10 recent-colour swatch cells (empty slots render transparent); OK and Cancel.

**SHOT-043 — Rename Pattern simple dialog**
- Reach: Pattern dropdown button > "Rename...".
- Visible: AlertWindow "Rename Pattern", body "Enter a new name:", text field pre-filled with the current name, OK and Cancel — **no colour selector.**
- Why separate: Two different rename surfaces exist for the same object and only one has a colour picker.
- Variant of: SHOT-042

**SHOT-044 — Pattern Color picker window**
- Reach: Pattern dropdown button > "Change Color...".
- Visible: A small always-on-top WINDOW titled "Pattern Color" with no OK/Cancel — distinct from the F2 dialog's embedded selector. Colour selector with current colour at top, colour space, sliders, editable hex field, and a persistent 10-cell recent-swatch row along the bottom (empty cells blank). Right-clicking a swatch offers "Set this colour". Colour applies live as you drag.
- Why separate: A third colour surface, this one a free-floating window that commits live.

**SHOT-045 — Delete Pattern confirmation**
- Reach: Patterns > Delete (or pattern dropdown > Delete) with two or more patterns.
- Visible: Warning box "Delete Pattern" reading Delete "<pattern name>"?; buttons "Delete" and "Cancel".

---

## SITTING 5 — Ribbon and contained-window chrome

**SETUP:** A project with at least one of every tab type. Use the "+" slot to
add: BaySickVocal (Vox), BaySickLiveInst (Inst), Harmless > Layers,
BaySickPlayer > Bass, BaySickPlayer > Audio Clips, BaySickDrums >
BaySickPlayer, BaySickRustyDrums, and a VST3 instrument if any are installed.
Lock one tab and freeze one tab before starting (needed for SHOT-048/049).

**SHOT-046 — Ribbon tab bar, fully populated (all 11 types)**
- Reach: Crop the ribbon with every type present.
- Visible: Eleven type slots in locked order — Builder, Mixer, Effects, Piano Roll, Clips (gold), Vox (teal), Inst (navy), Layers (orange), Bass (green), Drums (red), Plugins (purple) — each variable-width, plus the "+" at the end. Instance-count badges (white circle, dark number) on the bottom row of the seven instance types. Faint separator lines between slots.
- Why separate: **The seven instance-type slots only EXIST while they hold >= 1 tab and vanish entirely at zero**, returning only through the "+". Both pictures are needed.
- Variant of: SHOT-005

**SHOT-047 — Ribbon slot anatomy, close-up**
- Reach: Zoom hard on one populated instance slot (e.g. Layers with 3 tabs).
- Visible: Two-row slot — top 22px is the tab NAME (bold + full white when the slot's type is selected, dim otherwise; long names shrink rather than clip); bottom row carries the instance-count badge circle and, at the rightmost 22px, the down-chevron. An active slot gains a 2px bright top stripe.
- Why separate: **The split hit-target is the single most confusing ribbon behaviour** — clicking the name row (or anywhere except the bottom-right 22px) NAVIGATES; only the bottom-right chevron region opens the dropdown. Same dot-vs-chevron split as the Record button (SHOT-030). This shot wants a callout box in Manual 1.

**SHOT-048 — Ribbon slot, locked tab marker**
- Reach: Lock a Layers/Bass/Drums tab (via its window Menu > Lock), then crop its ribbon slot.
- Visible: The slot label gains a literal "[L] " prefix before the tab name. Composed at paint time — **it is NOT part of the saved name.**
- Variant of: SHOT-047

**SHOT-049 — Ribbon slot, frozen indicator (solid vs hollow)**
- Reach: Freeze a player tab (its window Menu > Freeze), crop its slot; then change that tab's content so the freeze goes stale and crop again.
- Visible: A 7px cyan (0xff00fff2) mark on the slot's left edge, bottom row. FILLED circle = frozen and playing its rendered file. HOLLOW ring = frozen but stale, currently back on the live engine while it re-renders. Deliberately drawn, not a snowflake glyph.
- Why separate: Two dots differing only by fill carry completely different meanings, and this is the ONLY signal a tab is playing cached audio.
- Variant of: SHOT-047

**SHOT-050 — "+" add menu, open**
- Reach: Click the narrow "+" slot at the right end of the ribbon.
- Visible: Rows in locked order: BaySickVocal / BaySickLiveInst / BaySickGuitars / BaySickBasses / "VSTPlugin" (submenu) / "Harmless" (submenu) / BaySickSynth / "BaySickPlayer" (submenu) / BaySickBass / "BaySickDrums" (submenu) / BaySickRustyDrums. **Every entry names an ENGINE, never a page type** — the engine decides which tab it lands in.
- Why separate: The main route to bring a page type back once its last instance is deleted. It is no longer the only one - the master menu bar's Edit > New Tab embeds this exact menu - but it is the one the ribbon puts in front of the user.

**SHOT-051 — "+" menu > Harmless submenu**
- Reach: "+" > hover "Harmless".
- Visible: Two rows: "Layers" and "Bass".
- Why separate: Side submenus exist only for engines that can live in more than one tab type; the pattern must be shown.
- Variant of: SHOT-050

**SHOT-052 — "+" menu > BaySickPlayer submenu**
- Reach: "+" > hover "BaySickPlayer".
- Visible: Three rows: "Layers", "Bass", "Audio Clips".
- Why separate: Different destination list from Harmless.
- Variant of: SHOT-050

**SHOT-053 — "+" menu > BaySickDrums submenu**
- Reach: "+" > hover "BaySickDrums".
- Visible: Two rows naming the PLAYER that backs the new Drums tab: "BaySickPlayer" and "BaySickSynth".
- Why separate: This submenu lists ENGINES rather than DESTINATIONS — the inverse shape of the two above, and easy to misread.
- Variant of: SHOT-050

**SHOT-054 — "+" menu > VSTPlugin submenu, empty**
- Reach: On an install with no instrument plugins added, "+" > hover "VSTPlugin".
- Visible: One greyed, non-selectable row: "None added - see Options > Plugins".
- Why separate: The empty state literally tells the user where to go — that instruction is the content.
- Variant of: SHOT-050

**SHOT-055 — "+" menu > VSTPlugin submenu, populated**
- Reach: Add at least one VST3 instrument via Options > Plugins..., then "+" > hover "VSTPlugin".
- Visible: An alphabetical list of the added instrument plugin names.
- Variant of: SHOT-054

**SHOT-056 — "+" menu, greyed entries at caps**
- Reach: Add BaySickRustyDrums once (it is a singleton) and fill the Inst page cap with BaySickLiveInst / BaySickGuitars / BaySickBasses tabs, then open the "+" menu.
- Visible: "BaySickLiveInst", "BaySickGuitars" and "BaySickBasses" all GREYED together (they share ONE Inst page cap); "BaySickRustyDrums" GREYED because the singleton is already live. **No explanation text on any of them.**
- Why separate: Four greyed rows with two different underlying reasons and no on-screen reason given.
- Variant of: SHOT-050

**SHOT-057 — Builder slot dropdown (sub-page)**
- Reach: Click the down-chevron on the Builder ribbon slot.
- Visible: Three rows: "Patterns", "Audio Clips", "Automation" — jumps the Builder's browser to that tab.

**SHOT-058 — Effects slot dropdown (sub-page)**
- Reach: Click the down-chevron in the bottom-right of the Effects ribbon slot.
- Visible: Three rows: "Rack", "Pre EQ", "Post EQ".
- Why separate: Second sub-page dropdown with its own list; structurally different from the instance dropdowns below.
- Variant of: SHOT-057

**SHOT-059 — Layers instance dropdown (canonical shape)**
- Reach: Add two or three Layers tabs, then click the chevron on the Layers ribbon slot.
- Visible: FOUR sections — (1) every Layers instance by name with a tick on the active one, plus any "[L] " / " (missing)" decorations; sep; (2) a greyed "Pages:" heading followed by indented rows naming that instance's windows, built live (includes Pre/Post EQ rows); sep; (3) "Rename..." and "Delete" (Delete always enabled unless the tab is locked); sep; (4) engine-named add rows "+ Add Harmless", "+ Add BaySickPlayer", "+ Add BaySickSynth".
- Why separate: This is the canonical instance-dropdown shape — every other type's dropdown is a variation on these four sections.

**SHOT-060 — Bass instance dropdown**
- Reach: Click the chevron on the Bass ribbon slot.
- Visible: Same four-section shape; add rows are "+ Add Harmless", "+ Add BaySickPlayer", "+ Add BaySickBass".
- Why separate: Different add-row set from Layers.
- Variant of: SHOT-059

**SHOT-061 — Drums instance dropdown, Rusty available**
- Reach: With no BaySickRustyDrums instance live, click the chevron on the Drums ribbon slot.
- Visible: Standard sections, then "+ Add BaySickPlayer", "+ Add BaySickSynth", then an extra row "+ Add BaySickRustyDrums".
- Variant of: SHOT-059

**SHOT-062 — Drums instance dropdown, Rusty already live**
- Reach: Add BaySickRustyDrums, then click the Drums chevron.
- Visible: Identical menu with the "+ Add BaySickRustyDrums" row **ABSENT — removed entirely, not greyed.**
- Why separate: Present-vs-absent rather than enabled-vs-greyed; genuinely a different menu.
- Variant of: SHOT-061

**SHOT-063 — Vox instance dropdown + "Add New Vox From Export" submenu**
- Reach: Add a Vox tab, export something into the project's Aligned/ or Pitched/ folder, then click the Vox chevron and hover "+ Add New Vox From Export".
- Visible: Standard sections plus "+ Add BaySickVocal", then a "+ Add New Vox From Export" submenu grouping entries under greyed "Aligned:" / "Pitched:" headings with indented file names.
- Why separate: The only instance dropdown with a file-list submenu.
- Variant of: SHOT-059

**SHOT-064 — Vox dropdown, export submenu greyed**
- Reach: On an unsaved project (or one with no exports, or with the Vox cap reached), click the Vox chevron.
- Visible: "+ Add New Vox From Export" shown GREYED with no submenu content. **Three different underlying causes, one identical look.**
- Variant of: SHOT-063

**SHOT-065 — Inst instance dropdown (plus cap-greyed variant)**
- Reach: Click the Inst chevron. Capture a second version with the shared Inst cap reached.
- Visible: Standard sections plus "+ Add BaySickLiveInst", "+ Add BaySickGuitars", "+ Add BaySickBasses" — all three GREYING TOGETHER at the cap because they share one page allowance.
- Why separate: Three add routes sharing one cap; the simultaneous greying is the thing to picture.
- Variant of: SHOT-059

**SHOT-066 — Clips instance dropdown**
- Reach: Drop an audio file to spawn a Clips tab (or "+" > BaySickPlayer > Audio Clips), then click the Clips chevron.
- Visible: Standard sections, but the add row is a single "+ Add BaySickPlayer..." with an ELLIPSIS — it opens the OS audio-file picker rather than creating an empty tab.
- Why separate: The ellipsis marks a file-picker route, unlike every other add row.
- Variant of: SHOT-059

**SHOT-067 — Plugins instance dropdown**
- Reach: Add a hosted VST3 instrument tab, click the Plugins chevron, hover "+ Add VSTPlugin".
- Visible: Standard sections plus a "+ Add VSTPlugin" SUBMENU listing added instruments alphabetically (or the greyed "None added - see Options > Plugins" row when none exist).
- Why separate: The only instance dropdown whose add route is itself a submenu.
- Variant of: SHOT-059

**SHOT-068 — Ribbon Rename dialog**
- Reach: Any instance dropdown > "Rename...".
- Visible: AlertWindow "Rename", message "Enter a new name:", a single text editor pre-filled with the tab's RAW name (no "[L] " or " (missing)" decoration), OK / Cancel.
- Why separate: Proves the decorations are paint-time only and not part of the stored name.

**SHOT-069 — Ribbon "Cannot Delete" locked alert**
- Reach: Lock a tab, then open that type's ribbon dropdown and click "Delete".
- Visible: Info-icon AlertWindow "Cannot Delete" — "This tab is locked. Unlock it first to delete."

**SHOT-070 — Contained window, full anatomy**
- Reach: Open any page window (click the Mixer ribbon slot) and capture the whole window with generous margin so the frame edge is visible.
- Visible: 1px black outer edge; a 4px resize border on all sides (drag zone, no visual affordance beyond the edge); a 26px title strip holding, left to right, the flat "Menu" heading, any extra headings, the Swing knob, tab slots, MID/SIDE, bank indicator, flush-right extras, then a fill-toggle square button and a close "x" (each 26px wide, 4px inset); the page content below. Body behind everything is 0xFF1B1B1F.
- Why separate: **Every page in the app lives inside this frame — the most-repeated chrome in the product.** One reference shot lets every later window shot skip re-labelling it.

**SHOT-071 — Contained window title strip, live vs not live**
- Reach: Capture one window's title strip while the mouse is over it (or its content has keyboard focus), then move the mouse away and capture again.
- Visible: Strip background switches between 0xFF34343D (live) and 0xFF2A2A31 (not live).
- Why separate: Low-contrast state change that is the app's ONLY focus indicator on contained windows; needs a side-by-side.
- Variant of: SHOT-070

**SHOT-072 — Title strip, centered engine wordmark**
- Reach: Open a Layers tab running Harmless (or any player) and crop its title strip.
- Visible: The engine's name painted centered in its accent colour at 15pt bold with a symmetric bloom halo (glyph path stroked at 30% alpha then filled). Centered in the FREE SPAN between the left cluster and the right extras, not on the whole strip. When a wordmark is present the small grey page-title text is suppressed.
- Variant of: SHOT-070

**SHOT-073 — Title strip, plain grey page title**
- Reach: Open a window with no engine identity (an effect panel or a satellite window) and crop its title strip.
- Visible: A small 10pt bold dim-grey title centered on the strip. Drawn only when there is no wordmark AND no tab slots.
- Why separate: The suppression rule means three different title treatments exist on the same strip.
- Variant of: SHOT-072

**SHOT-074 — Title strip with "Add" heading (Mixer)**
- Reach: Click the Mixer ribbon slot and crop its title strip.
- Visible: Two flat native-style headings reading "Menu  Add". "Add" appears only when a page installs an Add builder and takes no width otherwise. Hover/press draws a faint white highlight behind the heading — **there is no button bezel.**
- Why separate: The flat heading styling (deliberately NOT a chrome button) is a design decision a reader must be able to recognise.
- Variant of: SHOT-070

**SHOT-075 — Title strip with extra headings (Builder)**
- Reach: Click the Builder ribbon slot and crop its title strip.
- Visible: "Menu  Edit  View" — page-supplied headings sized to their own text. The most heading-dense strip in the app.
- Variant of: SHOT-074

**SHOT-076 — Title strip with a View mode menu open**
- Reach: Open the pedals window (Inst tab > window Menu > Pedals), then click the "View" heading on its title strip.
- Visible: A popup listing the available view mode names with a tick on the active one. On the pedals window the strip reads "Menu  View  NAM/IR".
- Why separate: The reusable view-swapper is how a window changes its entire layout.
- Variant of: SHOT-075

**SHOT-077 — Title strip with tab slots (EQ)**
- Reach: Effects ribbon slot chevron > "Pre EQ", then crop the resulting window's title strip.
- Visible: Chrome tab-slot buttons after the Menu heading (default 74px each), the active one carrying an accent outer GLOW RING rather than a filled body.  (QA-EqPro: the MID/SIDE buttons and the bank pill left the title strip - the views and the A/B pill live in the window's own top row now.)
- Why separate: Tab slots are a cluster that appears on only a few windows.
- Variant of: SHOT-070

**SHOT-078 — Title strip Swing Mix knob and its right-click menu**
- Reach: Crop the 24px knob immediately right of the Menu heading on any player window; then right-click it.
- Visible: Small rotary (value popup reads "Swing Mix 45%"; double-click returns 1.0). Right-click opens a one-item popup "Truncate Swing Notes" with a tick when on.
- Why separate: The one always-live CONTROL on the title strip, plus a hidden right-click menu on it.
- Variant of: SHOT-070

**SHOT-079 — Window Menu dropdown, player window**
- Reach: On any player window (Layers/Bass/Drums), click the "Menu" heading.
- Visible: Page navigation entries at the top (e.g. "Player", "Piano Roll" with a tick on the current view) plus that page's own actions, then a separator and the SHARED standard tail: "FX Rack", then "Freeze".
- Why separate: Every window has its own Menu and the shared tail is identical across all of them — shoot the tail once here.

**SHOT-080 — Window Menu, Freeze row, four states**
- Reach: Open a player window's Menu four times: (a) unfrozen; (b) after Menu > Freeze completes; (c) after changing that tab's content so the freeze goes stale; (d) on a window where freeze is locked (hover the greyed row to raise its tooltip).
- Visible: (a) "Freeze" in white-85%; (b) "Frozen" in cyan 0xff00fff2; (c) "Frozen" in orange 0xffff9100; (d) shown but GREYED at 38% alpha with a tooltip carrying the unlock path.
- Why separate: One menu row with four colour/enablement states and four tooltips — a textbook atlas case. (The Vox window's version of the greyed tooltip carries an extra whole-chain warning — that is SHOT-615.)
- Variant of: SHOT-079

**SHOT-081 — Contained window, filled vs restored**
- Reach: Click the square fill-toggle button left of the "x" on any window's title strip; capture the filled state, then click again and capture the restored state. Crop the button itself in both states too.
- Visible: Filled — the window occupies the entire workspace. The toggle glyph changes from a single drawn square (fill) to a restore glyph — a second square peeking out behind the first. A manual drag or resize while filled clears the flag, so the next click fills again instead of restoring.
- Why separate: Two glyph states on a 26px unlabelled button, plus a whole-workspace layout change.

**SHOT-082 — Window edge magnetism snap (before/after pair)**
- Reach: Drag one contained window until its edge comes within 10px of another window's opposing edge (or a workspace edge) and capture the moment it nudges flush. Capture the "before" too.
- Visible: Two windows sitting exactly edge-to-edge. **There is no snap guide line or highlight** — the only evidence is the alignment itself.
- Why separate: A behaviour with no visual affordance; a before/after pair is the only way to document it.

**SHOT-083 — Main frame, populated workspace with windows overlapping**
- Reach: Open Builder, Mixer, Effects and three or four engine windows. Drag them apart so each title strip is visible. Capture the whole screen.
- Visible: Multiple WorkspaceWindow frames inside the workspace, each with its own black 1px edge, 26px title strip and content; overlapping z-order; the ribbon showing extra type slots with instance-count badges.
- Why separate: Empty vs populated is the single biggest visual difference in the shell, and the contained-window model is unlike any normal DAW — a reader needs to see many windows inside one fixed frame.
- Variant of: SHOT-002

---

## SITTING 6 — Options and Help windows

**SETUP:** Have a MIDI controller connected for SHOT-084, then unplug it for
SHOT-085. Have at least one VST3 plugin folder available for the Plugins scan.
Perform several undoable actions before SHOT-102.

**SHOT-084 — Audio & MIDI Settings, populated**
- Reach: Options > Audio Settings...
- Visible: 480px-wide non-resizable dark dialog "Audio & MIDI Settings". Label+combo rows: Audio Mode, Audio Device, Sample Rate (44100 / 48000 / 88200 / 96000 / 176400 / 192000 Hz), Buffer Size (32..4096 samples), Trigger Velocity (From controller / Fixed). A "MIDI Inputs:" block of stacked checkboxes, one per detected device. Footer: "Open ASIO Control Panel" on the left, "Apply" and "Close" on the right.
- Note: The dialog's HEIGHT scales with the number of MIDI devices.

**SHOT-085 — Audio & MIDI Settings, no MIDI devices detected**
- Reach: Options > Audio Settings... with no MIDI inputs connected.
- Visible: Same dialog, but where the checkbox column would be there is a single italic dimmed label "(no MIDI devices detected)". The dialog is visibly shorter.
- Why separate: The empty-state a user with no controller always sees, and the height change.
- Variant of: SHOT-084

**SHOT-086 — Audio Settings, ASIO Control Panel button disabled**
- Reach: Options > Audio Settings..., set Audio Mode to a non-ASIO mode (e.g. Windows Audio).
- Visible: The "Open ASIO Control Panel" button greyed/disabled; everything else unchanged.
- Why separate: Its enablement keys on the LIVE device, not the combo selection — the hardest control in the dialog to explain.
- Variant of: SHOT-084

**SHOT-087 — Audio Settings, device list empty**
- Reach: Options > Audio Settings..., switch Audio Mode to ASIO on a machine with no ASIO driver installed.
- Visible: Audio Device combo showing nothing selected / no items; Sample Rate and Buffer Size still populated from the standard lists.
- Why separate: This is the state that produces SHOT-088.
- Variant of: SHOT-084

**SHOT-088 — "No audio device selected" warning**
- Reach: From SHOT-087's state, click Apply.
- Visible: Warning box "No audio device selected"; body explains to pick a device and that an empty list means the chosen mode has no drivers, with the ASIO4ALL / native-driver advice; OK.

**SHOT-089 — "Restart Required" prompt**
- Reach: Options > Audio Settings..., change Audio Mode / Device / Sample Rate, click Apply.
- Visible: Question box "Restart Required"; body says the change takes effect after restarting and warns unsaved changes will be lost; buttons "Restart Now" and "Later".

**SHOT-090 — File Settings dialog**
- Reach: Options > File Settings...
- Visible: Four take-type checkboxes (Dry / Dry Cleaned / Wet / Wet Cleaned — **at least one always stays checked; unchecking the last one re-checks it**); "De-noise strength:" combo (Light / Strong); an explanatory note about take types written at record stop; "Auto-freeze above:" horizontal slider + right-aligned readout showing a percentage; "Keep captured takes:" combo (This session only / In the project Reports folder); checkbox "Also keep the audio of each take"; checkbox "Enable Instrument Level Freeze".
- Note: This dialog is load-bearing for the vocal record/take story — Manual 1's Vox chapter should footnote it.

**SHOT-091 — File Settings, auto-freeze slider at Off**
- Reach: Options > File Settings..., drag "Auto-freeze above" all the way past 100 to the far right.
- Visible: Readout reads "Off" instead of a percentage, handle at the extreme right.
- Why separate: **The slider runs 0..101 and its top step reads "Off"** — genuinely unguessable, and it magnet-snaps to exactly 100 on the way past.
- Variant of: SHOT-090

**SHOT-092 — Plugins manager, never scanned**
- Reach: Options > Plugins...
- Visible: Resizable window "Plugins". Top filter row (label + text box, placeholder "name or manufacturer"). "1.  Scan folders" header + folder list + "Add Folder..." / "Remove" / "Reset to Defaults". "2.  Added plugins" header + 4-column table (Name / Kind / Manufacturer / File) + "Remove Selected". "3.  Scan results" header + 4-column table (checkbox / Name / Kind / "File / reason skipped"), empty. Bottom: "Scan" button, "Add Checked" (disabled), hidden progress bar, status text "Press Scan to search the folders above."

**SHOT-093 — Plugins manager, scan in progress**
- Reach: Options > Plugins..., click Scan and capture while it runs.
- Visible: Scan button now reads "Cancel Scan"; "Add Folder...", "Remove" and "Reset to Defaults" all greyed; progress bar visible; status text showing the scanner's live line.
- Why separate: Button labels, enablement and the progress bar all change.
- Variant of: SHOT-092

**SHOT-094 — Plugins manager, results with checked and skipped rows**
- Reach: Let a scan finish, then tick a couple of result checkboxes.
- Visible: Results table with alternating stripes; per-row checkbox drawn as an outlined rounded square, filled green when checked; found plugins showing Name / Kind (Instrument or Effect) / full file path; **dimmed skipped rows with NO checkbox**, filename in the Name column and the reason text in the "File / reason skipped" column; "Add Checked (N)" enabled with a count; finished-scan summary in the status line.
- Why separate: The checkbox-vs-no-checkbox distinction between found and skipped rows is the whole point of the section and is invisible until populated.
- Variant of: SHOT-092

**SHOT-095 — Plugins manager, added list populated**
- Reach: Tick results, click "Add Checked".
- Visible: Middle table filled: Name, Kind, Manufacturer, and the File column drawn in the dimmer text colour; "Remove" button below it.
- Variant of: SHOT-092

**SHOT-096 — Plugins manager, Add Folder native picker**
- Reach: Options > Plugins... > Add Folder.
- Visible: Native Windows folder-selection dialog titled "Choose a folder to scan for plugins".

**SHOT-097 — Key Binds window, General tab**
- Reach: Help > Key Binds...
- Visible: 880x680 resizable window "Key Binds". Six tabs: General, Builder, Piano Roll, Drum Kit, Vocal Editors, Event Editor. Four-column table — Action (240), Shortcut (180), Set (60), Reset (60) — with alternating row shading. Editable command rows carry Set and Reset buttons and show EVERY assigned key comma-separated (or "(none)"). Each row has a tooltip.

**SHOT-098 — Key Binds, a reference-only tab**
- Reach: Help > Key Binds... > click "Vocal Editors" (or "Event Editor").
- Visible: A table made entirely of greyed, non-editable reference rows below a thin accent separator line — mouse gestures and page-local hardcoded keys — in 55%-alpha text with NO Set/Reset buttons anywhere.
- Why separate: A tab with no editable rows at all looks broken next to the General tab unless pictured. (Piano Roll and Drum Kit tabs have BOTH kinds, split by that separator — worth a third crop if the atlas has room.)
- Variant of: SHOT-097

**SHOT-099 — Key Binds, "Set Shortcut" capture modal**
- Reach: Help > Key Binds... > click any "Set" button.
- Visible: 420x170 always-on-top modal "Set Shortcut" with centred text: "Press a key combination for:" / the command name / "Press Escape to cancel." **No buttons at all** — the whole component is the key catcher.
- Why separate: A dialog with zero buttons is unusual enough that a reader must be told it is waiting for a keystroke.

**SHOT-100 — Key Binds, "Key Already In Use" prompt**
- Reach: Key Binds > Set on a command, then press a key already bound to another editable command.
- Visible: Question-icon box "Key Already In Use" naming the key and the current owner, optionally appending an "It's also hardcoded for:" list; buttons "Replace" and "Cancel".
- Note: This dialog is where the manual states that Set REPLACES rather than accumulating bindings.

**SHOT-101 — Key Binds, "Key Hardcoded On Another Page" prompt**
- Reach: Key Binds > Set on a command, then press a key that is hardcoded on a page (e.g. a Piano Roll tool letter, or Ctrl+G) but not bound to an editable command.
- Visible: Question-icon box "Key Hardcoded On Another Page" listing each conflicting action with its category, explaining the binding will only fire when those pages are NOT in focus; buttons "Use Anyway" and "Cancel".
- Why separate: Different dialog, different buttons and a subtler rule than SHOT-100.
- Variant of: SHOT-100

**SHOT-102 — Undo History window, populated**
- Reach: Perform several undoable actions, press Ctrl+Z once or twice so there is redo history, then Edit > History...
- Visible: Narrow window "Undo History" with a single scrollable list: indented past-action labels above, a blue-tinted row reading ">>  Current" with a divider line above it, and dimmed future (redone-away) labels below. The Current row is selected and scrolled into view. Clicking any row time-travels to that state.
- Why separate: Click-to-time-travel is not obvious from the picture, and the past/current/future row styling needs labelling.

**SHOT-103 — Undo History window, fresh session**
- Reach: Launch and immediately choose Edit > History... without doing anything undoable.
- Visible: Same window containing only the ">>  Current" marker row and nothing above or below it.
- Variant of: SHOT-102

**SHOT-104 — Rusty Drums Map window**
- Reach: Rusty window > Menu > Rusty Drums Map... The window itself works with or without a kit loaded — it falls back to parsing the installed Big Rusty Drums kit.
- Visible: Resizable DocumentWindow "Rusty Drums Map" (640x600) with a 24px header and a four-column table — Key / MIDI / Sound / Articulation — alternating row stripes, one row per kit-native note (e.g. C3 / 36 / Kick / ...).
- Why separate: A whole reference table reachable only from the Help menu; a reader will never find it unless the atlas shows it.

**SHOT-105 — About BaySickDAW dialog**
- Reach: Help > About BaySickDAW v1.0
- Visible: Info-icon message box "BaySickDAW v1.0"; version line, "Built with JUCE 7  |  (c) KnowledgeBase Studios", and a "Powered by:" list naming sfizz (BSD 2-Clause) and LAME (LGPL); single OK button. (The attribution list is knowingly incomplete.)
**SHOT-724 - Manuals window (Help > Help Index, or F1)**
- Reach: Help > Help Index, or press F1 from the main frame or any page window.
- Visible (installed state): Resizable desktop DocumentWindow "BaySickDAW Manuals", opening 1100x800 centered, close button only, the shared 26px title strip, filled edge to edge by an embedded browser showing the manuals site staged at `<exe dir>\Manuals\index.html`. The app asks for the WebView2 backend, but JUCE silently substitutes the legacy IE control when WebView2 cannot be constructed, so do not caption the picture as WebView2 unless it has been confirmed on that machine.
- Visible (missing state, and the one a development build actually shows): no browser at all - plain text over the app background reading "The manuals are not installed." / "They belong at:" / the full path / "A full install places them there. If you are running a development build, the manuals are built separately and staged into that folder.", with a single 200x30 button "Open Manuals Folder" horizontally centered, sitting just below the vertical middle of the window and well clear of the text block.
- Why separate: Two completely different contents in the same frame, and the second is what every reader meets until the manuals are staged. Added 2026-08-11 (QA-Manuals Task 1). NOTE on the id: the SHOT-nnn scheme is RETIRED (see "How the ids work (RETIRED)" above). This number is a filing label for this list only, not an anchor any manual will cite, and it does not revive the retired numbering - do not read it as the append rule coming back.

---

## SITTING 7 — Project lifecycle dialogs

**SETUP:** Have two or three saved projects. Let the app run 15+ minutes at some
point so autosave backups exist (SHOT-117). Do SHOT-119 at the very start of a
fresh launch, before any backup is written.

**SHOT-106 — New Project name prompt**
- Reach: File > New Project... (Ctrl+N) on a clean session.
- Visible: Question-icon AlertWindow "New Project"; body naming the exact folder path that will be created (`<Projects root>\<name>\`) and noting it can be renamed or moved later; text field pre-filled "Untitled Project"; "Create" and "Cancel".
- Why separate: It tells the user where projects live on disk — reference content a beginner needs.

**SHOT-107 — Save Project As prompt**
- Reach: File > Save As... (Shift+Ctrl+S)
- Visible: AlertWindow "Save Project As", body "Save a copy of this project under a new name.", text field pre-filled with the current project name (or "Untitled Project"), Create and Cancel.
- Why separate: Same shape as New Project but different title, body and prefill — and it is the flow every unsaved export or record silently routes through.
- Variant of: SHOT-106

**SHOT-108 — Unsaved changes prompt (NATIVE task dialog)**
- Reach: With a dirty project (title bar shows " *"), do File > New Project..., File > Open Project..., File > Quick Open Project..., or close the app.
- Visible: A **NATIVE Windows TaskDialog** with a warning icon titled "Unsaved changes" — visibly different from every other prompt in the app. Message either "Save changes to '<name>' first?" or "You have unsaved changes.  Save them first?"; three buttons in order: Save / Don't Save / Cancel.
- Why separate: The app's only native-styled decision dialog, and it gates four separate destructive routes. Call it out so readers do not think they have left BaySickDAW.

**SHOT-109 — Open Project, native folder picker**
- Reach: File > Open Project... (Ctrl+O)
- Visible: Native folder-selection dialog "Open Project", opened at the default Projects root.

**SHOT-110 — "Not a BaySickDAW project folder" warning**
- Reach: File > Open Project..., select any folder with no project.xml inside it.
- Visible: Warning box "Not a BaySickDAW project folder" — "That folder has no project.xml inside it."; OK.

**SHOT-111 — Quick Open Project browser, populated**
- Reach: File > Quick Open Project...
- Visible: Resizable dialog "Quick Open Project" on a near-black background; a sortable three-column table — Name (240), Last Modified (170), Size (100) — with clickable sort headers, alternating row shading, a blue selected row; footer with "New Project" on the left and Open / Cancel on the right.
- Why separate: A full file-browser surface that is NOT the OS file picker.

**SHOT-112 — Quick Open Project browser, empty**
- Reach: File > Quick Open Project... on a fresh install.
- Visible: Same dialog with an empty table body under the three column headers; footer unchanged.
- Variant of: SHOT-111

**SHOT-113 — Quick Open, project right-click context menu**
- Reach: File > Quick Open Project... > right-click a project row.
- Visible: "Open" / sep / "Rename..." / "Duplicate..." / "Delete" / sep / "Show in Explorer". **Rename and Delete are GREYED on the currently-open project.**
- Why separate: A context menu with state-dependent greying, hidden behind a right-click inside a dialog.

**SHOT-114 — Quick Open, Rename Project prompt**
- Reach: Quick Open > right-click a project (not the open one) > Rename...
- Visible: Question AlertWindow "Rename Project", body "Enter a new name for <folder>:", text field pre-filled with the current folder name, OK and Cancel.

**SHOT-115 — Quick Open, Duplicate Project prompt**
- Reach: Quick Open > right-click a project > Duplicate...
- Visible: Question AlertWindow "Duplicate Project", body "Enter a name for the copy:", text field pre-filled "<name> Copy", OK and Cancel.
- Why separate: Different title, body and prefill from Rename; separate menu action a reader looks up by name.
- Variant of: SHOT-114

**SHOT-116 — Quick Open, Delete Project confirmation**
- Reach: Quick Open > right-click a project (not the open one) > Delete.
- Visible: Warning box "Delete Project": "Move '<name>' to the Recycle Bin?" plus the restore-from-there note; buttons "Delete" and "Cancel".

**SHOT-117 — Restore from Backup, backup list menu**
- Reach: File > Restore from Backup... on a project that has autosave backups (autosave writes one every 15 minutes).
- Visible: A POPUP MENU (not a dialog) with a section header "Backups for <project>" or "Unsaved-session backups", then one row per backup newest first, formatted "YYYY-MM-DD HH:MM   (12 min ago)".
- Why separate: A menu, not a dialog — unexpected for a "Restore..." item — and the age formatting is its own convention.

**SHOT-118 — Restore from Backup, confirmation**
- Reach: File > Restore from Backup... > pick a backup.
- Visible: Warning-icon OK/Cancel box "Restore from Backup" naming the file, warning that current unsaved changes will be replaced, and warning that content DELETED from the project folder since the backup will be missing; buttons "Restore" / "Cancel".
- Variant of: SHOT-117

**SHOT-119 — "No backups available" notice**
- Reach: File > Restore from Backup... within the first 15 minutes of a session, or on a project that has never autosaved.
- Visible: Info-icon box "No backups available"; the body differs slightly depending on whether a project is open, and both state the 15-minute autosave cadence.
- Why separate: **The 15-minute rule is stated only here.**
- Variant of: SHOT-117

**SHOT-120 — Save Template As dialog**
- Reach: File > Save as Template...
- Visible: AlertWindow "Save Template As"; a body paragraph explaining templates save every tab and its sound, mixer levels, routing, effects and EQ but NO patterns or arrangement; text field pre-filled "My Template"; Save and Cancel.

**SHOT-121 — Pick Default Template, file chooser**
- Reach: Options > General > Set Default Template...
- Visible: Native file dialog "Pick Default Template", opened at the Templates folder showing the Factory and My Templates subfolders, filtered to *.xml.

**SHOT-122 — "Default Template Set" confirmation**
- Reach: Options > General > Set Default Template..., pick a .xml file.
- Visible: Info box "Default Template Set" naming the template, pointing at File > New from Template > New from Default Template, and stating how to undo via Options > General > Clear Default Template; OK.

**SHOT-123 — Import Audio, file chooser**
- Reach: File > Import Audio...
- Visible: Native open dialog "Import Audio File", opened at the My Samples folder, filtered to `*.wav;*.mp3;*.aiff;*.flac;*.ogg;*.aif`.

**SHOT-124 — "Invalid project name" warning**
- Reach: File > New Project..., type a name containing one of `< > : " / \ | ? *` or a reserved device name (CON, PRN, AUX, NUL, COM1-9, LPT1-9), then Create.
- Visible: Warning box "Invalid project name" listing the forbidden characters and the reserved names; OK re-opens the name prompt.
- Why separate: Deliberately triggerable (type a bad name on purpose) and the ONLY place the naming rules are stated.

**SHOT-125 — "Name Already Used" collision prompt (SHARED)**
- Reach: Any typed-name save, done twice with the same name — a window Menu's "Save Page Preset As...", the Drum Kit's "Save Kit As...", a pedalboard preset, a template, an effect preset.
- Visible: Question box "Name Already Used"; body quotes the existing filename, names the folder it lives in, and offers the auto-numbered copy name "<name> (2).xml"; THREE buttons: "Replace", "Save a Copy", "Cancel".
- Why separate: **Every typed-name save in the app routes through this one prompt** — one shot documents all of them. Manual 1 should footnote it from each save dialog rather than repeating it.

**SHOT-126 — "Save failed" — unusable name**
- Reach: In any typed-name save dialog, type a name made only of illegal characters (e.g. `///`) and click Save.
- Visible: Warning box "Save failed" quoting the typed name and listing the characters that cannot be used: `/ \ : ; , ? * # @ ^ | < >` and quote marks; OK.
- Why separate: Same title as the disk-write failure (SHOT-710) but an entirely different body. This one is deliberately triggerable; that one is a fault state.

**SHOT-127 — Heavy-operation overlay, indeterminate (project load)**
- Reach: File > Open Recent > pick a project with several tabs, or Quick Open a large project. Capture during the load.
- Visible: Whole app dimmed behind a 55%-black wash; a centred rounded panel with an accent border; bold title "Loading Project..."; a SWEEPING indeterminate progress bar with no percentage; a step label such as "Closing old tabs..."; below it a faded ticker of the last few completed steps, oldest at top. **No Cancel button.**
- Why separate: It is the only thing on screen during a load, covers everything else (it promotes itself to its own always-on-top desktop window so it sits above the native child windows), and has no other route to being seen.

**SHOT-128 — Heavy-operation overlay, determinate with Cancel**
- Reach: Trigger a freeze render — enable Options > File Settings > "Enable Instrument Level Freeze", then a player window's Menu > Freeze on a tab with real content. Capture during the render.
- Visible: Same overlay panel, but the title row carries a right-aligned PERCENTAGE, the bar is FILLED rather than sweeping, and a "Cancel" button is drawn bottom-right (it reads "Cancelling..." and dims once clicked). The title names the track, e.g. "Freezing Layers 2...".
- Why separate: Percentage, filled bar and Cancel exist only here. A reader needs to know which long operations can be stopped.
- Variant of: SHOT-127

**SHOT-129 — Shutdown overlay**
- Reach: Close the app with a project loaded that has several tabs; capture the moment after the window content disappears.
- Visible: Same overlay style, title "Shutting Down...", step labels "Closing tabs and engines..." then "Releasing audio device...", indeterminate pulse with no percentage.
- Why separate: Different title and indeterminate mode; it is what the user stares at while the app tears down and is easy to mistake for a hang.
- Variant of: SHOT-127

---

## SITTING 8 — Builder page

**SETUP (do this once, it unlocks six otherwise-impossible shots):** a project
containing at least one imported clip, one Vox recording, one Inst recording,
one completed export, one completed Measure (for a Reports entry), a group of
take-tagged recordings, one clip that has been pitch-shifted AND time-stretched
AND customized in Properties AND slip-edited, one grouped track row, and at
least one automation clip. Take every Builder shot in one pass off that project.

**SHOT-130 — Builder window, full view, default state**
- Reach: Ribbon > Builder. Leave the window at its default size.
- Visible: Title strip with headings "Menu", "Edit", "View"; the 30px arrangement toolbar; left Browser panel titled "SOURCE PICKER" with its 3 tab buttons; the 5px vertical divider grip; the fixed 120px track-header column (blank corner square over the ruler, then per-row M and S LED dots + track name); the ruler band with 1-based bar numbers; the arrangement grid; the horizontal scrollbar under the grid.

**SHOT-131 — Builder toolbar, close-up**
- Reach: Photograph only the 30px toolbar row directly under the title strip.
- Visible: Snap magnet button (lit = snap active, dim = Off); eight tool buttons Draw(P) / Paint(B) / Select(E) / Delete(D) / Mute(T) / Slice(C) / Zoom(Z) / Play(Y) with the active one toggled; "Stretch (S) v" dropdown button; Undo; Redo; "H" history button; "-" and "+" zoom buttons; right-aligned context label reading "Playlist > <pattern name>".
- Why separate: This is where every tool name and its keyboard letter lives; it must be readable at label size.
- Variant of: SHOT-130

**SHOT-132 — Builder toolbar, Snap dropdown open**
- Reach: Click the Snap magnet button at the far left of the toolbar.
- Visible: The 11-entry snap-division ladder with a tick on the current pick. (Same label set as SHOT-031 — footnote back to it.)

**SHOT-133 — Builder toolbar, Slip/Stretch dropdown open (both label states)**
- Reach: Click the "Stretch (S) v" button just right of the Play(Y) tool. Capture the open menu, then pick Slip and capture the button's other label.
- Visible: Two-item menu Slip / Stretch; the button label itself changes between "Stretch (S) v" and "Slip (S) v".
- Why separate: Two-state button whose label changes, and the mode changes what an audio-clip edge drag does.

**SHOT-134 — Browser panel, Patterns tab**
- Reach: Builder > click "Patterns" (leftmost of the 3 browser tab buttons).
- Visible: "SOURCE PICKER" caption; the three tab buttons (Patterns / Files / Auto) with the active one highlighted; a "+ Add" and "Delete" button pair; then one draggable coloured pattern box per pattern with the selected one highlighted.

**SHOT-135 — Browser panel, Files tab, populated tree**
- Reach: Builder > click "Files".
- Visible: A "Sort" button row (Files tab only); a TreeView with five collapsible category headers each showing a count — "Clips (n)" amber, "Vox (n)" teal, "Inst (n)" navy, "Exports (n)" cyan, "Reports (n)" orange; expand triangles; 26px file leaves with their display names.

**SHOT-136 — Browser panel, Files tab, empty categories**
- Reach: File > New Project (nothing imported, recorded or exported), then Builder > Files.
- Visible: The same five category headers, each reading "(none)", no leaves.
- Why separate: Exactly the state a beginner sees first and cannot interpret.
- Variant of: SHOT-135

**SHOT-137 — Browser panel, Files tab with a group node**
- Reach: Builder > Files, expand a Vox category containing recorded takes (groups form automatically from take-tagged filenames), or right-click a category header > Create Group... first.
- Visible: A 24px group header row (name + "(n)" count, accent bar on its left edge) nested under the category header, with take leaves indented beneath it.
- Why separate: Groups are a THIRD row type in the tree and look different from both category headers and leaves.
- Variant of: SHOT-135

**SHOT-138 — Browser panel, Automation tab, empty**
- Reach: Builder > click "Auto" with no automation clips created.
- Visible: Centred empty-state text "No automation clips" with a smaller hint line "View > New Automation Clip" underneath.

**SHOT-139 — Browser panel, Automation tab, populated**
- Reach: Builder > Menu > New Automation Clip..., pick a parameter, Create. Then click "Auto".
- Visible: One draggable box per automation template, labelled "Channel - Effect - Param" (or the user's rename).
- Variant of: SHOT-138

**SHOT-140 — Browser panel, collapsed**
- Reach: Drag the thin vertical divider on the browser's right edge to the left, past the magnetic floor, until the panel closes.
- Visible: Browser width zero; only the 5px divider strip remains, carrying a small right-pointing chevron meaning "drag me back out". Track headers and grid have taken the space.
- Why separate: The chevron grip is the ONLY affordance left when collapsed — a reader who collapses it by accident needs this picture.
- Variant of: SHOT-130

**SHOT-141 — Browser Sort menu open**
- Reach: Builder > Files > click the "Sort" button under the tab buttons.
- Visible: Newest First / Oldest First / Alphabetical, tick on the active mode.

**SHOT-142 — Browser, pattern right-click menu**
- Reach: Builder > Patterns > right-click any pattern box.
- Visible: Rename... / Duplicate / Change Color... / sep / Render to WAV... / Split by Player Engine... / sep / Delete.

**SHOT-143 — Browser, Files leaf right-click menu (library file)**
- Reach: Builder > Files > right-click a leaf under Clips, Vox or Inst.
- Visible: (Locate... / sep, only when the file is missing) Rename... / Duplicate... / Show in Explorer / Properties... / (on a "CLEANED" take only) a "Regenerate De-noise" submenu with Light + Strong / sep / "Choke Group" submenu (None + Group 1..16, tick on current) / sep / Delete.

**SHOT-144 — Browser, Files leaf right-click menu (Exports / Reports)**
- Reach: Builder > Files > expand Exports (or Reports) after doing an export > right-click a leaf.
- Visible: For an audio export — "Add to Project..." + "Show in Explorer". For a report — "Open in Analyzer" + "Show in Explorer". For a Direct to Master row — "Rename..." / "Remove" + "Show in Explorer".
- Why separate: Renders are not library entries and get a completely different, much shorter menu.
- Variant of: SHOT-143

**SHOT-145 — Browser, category header right-click menu**
- Reach: Builder > Files > right-click the "Clips" (or Vox / Inst) category header row.
- Visible: A single "Create Group..." entry.

**SHOT-146 — Browser, group header right-click menu**
- Reach: Builder > Files > right-click a group header row inside a category.
- Visible: "Rename Group..." (capture both an auto group and a manual group if the labels differ).

**SHOT-147 — Audio Properties dialog, browser-entry version**
- Reach: Builder > Files > right-click a Clips/Vox/Inst leaf > Properties...
- Visible: Pitch shift (semitones) text field; an EDITABLE "Original BPM" text field; "Mode:" combo (Stretch (pitch locked) / Resample (pitch follows tempo)); a wide "Routes to: <page>" button that opens a target menu; Apply / Cancel.

**SHOT-148 — Audio Properties dialog, per-clip version**
- Reach: Builder > right-click an audio clip on the grid > Properties...
- Visible: Same box but the BPM row is a READ-ONLY detected-tempo text block, the route menu offers only "Copy to <name>" entries (no Move), and there is an extra "Reset to Browser Entry" button beside Apply / Cancel.
- Why separate: Same dialog builder, three visible differences — editable vs read-only BPM, Move+Copy vs Copy-only, and a third button.
- Variant of: SHOT-147

**SHOT-149 — Audio Properties, routing target menu open**
- Reach: In either Audio Properties dialog, click the "Routes to: ..." button.
- Visible: One entry per existing Vox / Inst / Clips page plus "a new Clip Page", "a new Vox Page", "a new Inst Page". In the browser version each target is itself a submenu with "Move here" / "Copy here".

**SHOT-150 — Track header column, close-up**
- Reach: Photograph the left 120px column beside the grid, covering 4-6 rows including one grouped row.
- Visible: Blank corner square level with the ruler; per row a small "M" and "S" caption over two LED dots (mute red when on, solo yellow when on, both dim grey when off), the track name text, alternating row tint; on grouped rows a coloured 3px band down the left edge plus a 10%-alpha colour wash across the row.

**SHOT-151 — Track header, right-click menu**
- Reach: Builder > right-click any row label in the track-header column.
- Visible: Rename... / sep / Move Up / Move Down / Insert Track Above / sep / Group with Above / Remove from Group / Color Group... / sep / Render Track to WAV... / sep / Delete Track Clips. Note which entries grey out on an ungrouped row.

**SHOT-152 — Track header, right-click on an audio row**
- Reach: Put at least one audio clip on a row, then right-click that row's header.
- Visible: Same menu with "Render Track to WAV..." ENABLED — it is greyed on pattern-only and automation rows.
- Why separate: That one row's enablement is the visible difference between an audio row and any other row.
- Variant of: SHOT-151

**SHOT-153 — Track rename dialog**
- Reach: Builder > right-click a track header row > Rename...
- Visible: Small "Rename Track" box: "New name:" text field, OK and Cancel.

**SHOT-154 — Color Group picker**
- Reach: Group two rows (right-click a row > Group with Above), then right-click a grouped row > Color Group...
- Visible: The shared colour-picker surface with live preview.

**SHOT-155 — Builder ruler, close-up with all marker kinds**
- Reach: Right-click the ruler and add a Time Marker, a Time Signature and a Tempo Change at different bars; Ctrl+drag on the ruler to make a time selection; press Play so the playhead shows. Photograph the 18px ruler band.
- Visible: 1-based bar numbers; adaptive sub-beat ticks; a YELLOW PENNANT FLAG = time marker; a SOLID BLUE PILL "4/4" = manual time-signature change; an OUTLINE BLUE PILL = auto/linked time-signature marker; an AMBER PILL with a BPM number = tempo change; the highlighted time-selection span with its two bracket lines; the GREEN right-hanging playhead flag.
- Why separate: Six different glyph vocabularies in one 18px band — the densest legend in the app.

**SHOT-156 — Ruler right-click menu, empty bar**
- Reach: Builder > right-click the ruler at a bar with no markers.
- Visible: "Add Time Marker at Bar N...", "Add Time Signature at Bar N...", "Add Tempo Change at Bar N...".

**SHOT-157 — Ruler right-click menu, on an existing marker**
- Reach: Builder > right-click the ruler directly on a marker / time-sig pill / tempo pill.
- Visible: A section header naming what is there (e.g. "Marker: Chorus", "Time Sig: 3/4 @ Bar 9", "Tempo: 128.0 BPM @ Bar 17") followed by Edit.../Delete pairs, then the three Add entries at the bottom.
- Why separate: The menu grows section headers and edit/delete rows only when something is already at that bar.
- Variant of: SHOT-156

**SHOT-158 — Add / Edit Time Marker dialog**
- Reach: Builder > right-click ruler > Add Time Marker at Bar N...
- Visible: "Add Time Marker" box with the prompt naming the bar, a label text field, Add and Cancel. (Edit variant reads "Edit Time Marker" with Save.)

**SHOT-159 — Add / Edit Time Signature dialog**
- Reach: Builder > right-click ruler > Add Time Signature at Bar N...
- Visible: Time-signature entry box (numerator / denominator) with its buttons.

**SHOT-160 — Add / Edit Tempo Change dialog**
- Reach: Builder > right-click ruler > Add Tempo Change at Bar N...
- Visible: "Add Tempo Change" box: prompt "Tempo from Bar N onward (20-300 BPM):", BPM text field, Add and Cancel. (Edit variant reads "Edit Tempo Change" with Save.)

**SHOT-161 — Grid with pattern clips**
- Reach: Builder > Patterns > drag a pattern from the browser onto a grid row (or use Draw and click).
- Visible: Pattern blocks in the pattern's own colour, with the pattern name and MIDI-note shading drawn inside the block, plus the resize handle at the right edge.

**SHOT-162 — Grid audio clips, the four route colours**
- Reach: Place four audio clips — one routed to a Clips page, one to Vox, one to Inst, and one unrouted (dropped straight onto a bare row).
- Visible: AMBER = routed to a Clips page, TEAL = Vox, NAVY = Inst, TEAL-GREY = unrouted / generic Audio row; each with waveform, filename label and resize handle.
- Why separate: **Block colour is the only on-grid clue to where a clip plays** — a manual needs all four side by side.

**SHOT-163 — Grid clip, muted**
- Reach: Builder > pick the Mute(T) tool and click a clip (or right-click > Mute).
- Visible: The block under a ~30% black wash with white diagonal hatch lines over it.
- Why separate: Muted is an overlay applied on top of EVERY clip type.
- Variant of: SHOT-161

**SHOT-164 — Grid audio clip, badge close-up**
- Reach: Zoom in so the prepared multi-badge clip is at least 120px wide and 40px tall.
- Visible: Filename label (top left); yellow pitch label like "+2.0st" (top right); amber "x1.25" stretch pill on a dark rounded pill (bottom right); the follow dot bottom-left (GREEN = still follows the browser entry, RED = customized); and a small left-pointing white/black triangle on the left edge meaning hidden pre-roll audio.
- Why separate: Five separate readouts a reader can otherwise never name.
- Variant of: SHOT-162

**SHOT-165 — Grid automation clip**
- Reach: Builder > Menu > New Automation Clip... > pick a parameter > Create.
- Visible: An automation block showing the resolved lane name ("Channel - Effect - Param") and the drawn point/curve line with its draggable point handles and curve handles.

**SHOT-166 — Clip right-click menu, audio clip**
- Reach: Builder > right-click an audio clip on the grid.
- Visible: Cut / Copy / Paste / sep / Delete / sep / Mute (or Unmute) / sep / Properties... / Reset Stretch (greyed unless the clip has been re-fitted).

**SHOT-167 — Clip right-click menu, pattern clip**
- Reach: Builder > right-click a pattern block on the grid.
- Visible: Cut / Copy / Paste / Delete / Mute, plus a "Set Time Signature" submenu listing 4/4, 3/4, 2/4, 6/8, 5/4, 7/8, 12/8, 9/8 with a tick on the current one.
- Why separate: Only pattern clips get the time-signature submenu; only audio clips get Properties / Reset Stretch.
- Variant of: SHOT-166

**SHOT-168 — Clip right-click menu, automation clip**
- Reach: Builder > right-click an automation block on the grid.
- Visible: Cut / Copy / Paste / Delete / Mute plus "Open in Event Editor...".
- Why separate: Third distinct menu shape for the third clip type.
- Variant of: SHOT-166

**SHOT-169 — Quantize popup (Alt + right-click)**
- Reach: Builder > select one or more clips with the Select(E) marquee > Alt + right-click on the grid.
- Visible: Section header "Quantize selection to nearest:" then Bar / 1/2 Bar / Beat (1/4 bar) / Step (1/16 bar).

**SHOT-170 — Grid, marquee selection in progress**
- Reach: Builder > Select(E) tool > drag a rectangle across several clips without releasing.
- Visible: The marquee rectangle overlay and the selected blocks in their selected state.

**SHOT-171 — Grid, slice line drag in progress**
- Reach: Builder > Slice(C) tool > press and drag across clips without releasing (hold Shift for a vertical line).
- Visible: The two-dot cut line preview crossing the rows it will split.

**SHOT-172 — Grid, zoom rectangle drag**
- Reach: Builder > hold Ctrl and right-click-drag a rectangle over part of the grid.
- Visible: The zoom-rect overlay showing the region that will fill the viewport.

**SHOT-173 — Grid, drag ghost from the browser**
- Reach: Builder > Patterns > start dragging a pattern box over the grid and hold it there without dropping.
- Visible: The semi-transparent ghost block previewing where the clip will land, snapped to the current snap division.

**SHOT-174 — Grid, OS file drag ghost**
- Reach: Drag a .wav from Windows Explorer over the Builder grid and hold it there.
- Visible: The file-drag highlight state on the grid.

**SHOT-175 — Builder Menu dropdown**
- Reach: Builder > click "Menu" on the window title strip.
- Visible: Import Audio... / sep / Rename Pattern (F2) / Find Next Empty (F3) / New Automation Clip... / sep / Render Pattern to WAV...

**SHOT-176 — Builder Edit dropdown**
- Reach: Builder > click "Edit" on the title strip.
- Visible: Undo / Redo (greyed when nothing to undo/redo) / sep / Select All (Ctrl+A) / Deselect (Esc) / sep / Copy (Ctrl+C) / Paste (Ctrl+V) / Delete (Del) / Duplicate (Ctrl+B).

**SHOT-177 — Builder View dropdown**
- Reach: Builder > click "View" on the title strip.
- Visible: Zoom In (+) / Zoom Out (-) / sep / Performance Mode (Ctrl+P) with a tick when on.

**SHOT-178 — Builder, Performance Mode on**
- Reach: Builder > View > Performance Mode (Ctrl+P), then start playback.
- Visible: The pulsing highlight band on the current bar in the ruler and the performance overlays over the grid.
- Why separate: A whole-page visual mode change driven by one menu tick.
- Variant of: SHOT-130

**SHOT-179 — New Automation Clip dialog**
- Reach: Builder > Menu > New Automation Clip... (also reachable from Edit > New Automation Clip on the master menu bar).
- Visible: "New Automation Clip" box; prompt "Select or type the parameter ID to automate:"; a "Parameter:" combo listing every registered parameter ID alphabetically; an "Or type ID:" text field pre-filled with the first ID; Create and Cancel.

**SHOT-180 — Pattern render options popup**
- Reach: Builder > Menu > Render Pattern to WAV... on a pattern with notes on at least one track.
- Visible: Section header "Render pattern", "Per Track  (n files)", "Full Mix  (1 file)", sep, "Select Tracks...".

**SHOT-181 — Select Tracks render dialog**
- Reach: Builder > Menu > Render Pattern to WAV... > Select Tracks...
- Visible: "Select Tracks" box: "Choose the tracks to render, and how:" with one checkbox per track carrying notes (all ticked by default); an "Output:" combo (One file per track / One file, mixed together); Render and Cancel.

**SHOT-182 — "Nothing to render" info box**
- Reach: Builder > Menu > Render Pattern to WAV... on a pattern with no notes anywhere.
- Visible: Info box "Nothing to render" / "This pattern has no notes on any track."
- Why separate: A deliberately reachable empty-state dialog, not a crash box.

**SHOT-183 — Split by Player Engine, group prompt**
- Reach: Builder > Patterns > right-click a pattern that is placed on a GROUPED row and has notes on several engines > Split by Player Engine...
- Visible: Question box "Split by Player Engine" / "Add the new tracks to the row group?" with Yes and No.

**SHOT-184 — Split by Player Engine, nothing-to-split box**
- Reach: Builder > Patterns > right-click an empty pattern > Split by Player Engine...
- Visible: Info box "Split by Player Engine" / "This pattern has no MIDI data to split."
- Why separate: Second, different box from the same menu entry.
- Variant of: SHOT-183

**SHOT-185 — Delete library entry confirmation**
- Reach: Builder > Files > right-click a leaf > Delete.
- Visible: The confirmation naming the file and warning that matching grid blocks go with it.

**SHOT-186 — Duplicate file name prompt (and its conflict prompt)**
- Reach: Builder > Files > right-click a leaf > Duplicate...
- Visible: A name-entry box pre-filled "<original> Duplicate"; if the name already exists, the follow-up conflict box with Overwrite / Cancel / Rename.

**SHOT-187 — "File Already in Library" prompt**
- Reach: Drag an audio file onto the Builder grid that has already been imported once.
- Visible: Question box "File Already in Library"; body quotes the filename and the page it already lives on; THREE buttons: "Use Existing", "New Page", "Cancel".

**SHOT-188 — "No free Clips / Vox / Inst page" warning**
- Reach: Fill every page slot of one kind, then try to add another (ribbon "+", a page menu's Duplicate, or the grid's create-page path).
- Visible: Warning box "No free Clips page" (or Vox / Inst) stating all pages of this type are in use and one must be closed first; OK.

---

## SITTING 9 — Export and bundle

**SETUP:** A saved project with real content on several mixer strips, and a
ruler time-selection made on the Builder so the "Selected Section" option is
live. Budget a long arrangement so SHOT-194's progress state is catchable.

**SHOT-189 — Export Audio dialog, options state**
- Reach: File > Export Audio...
- Visible: Non-resizable dialog "Export Audio". Label+combo rows: Selection (Full Arrangement / Selected Section — the latter greyed with no ruler selection), Tail (Included / Cut), Format (WAV / OGG / MP3), Quality (contents change per format), Sample rate (44100 / 48000 / 88200 / 96000 / 176400 / 192000 Hz - the four above 48000 gray out when Format is MP3), Dither (Off / Flat (TPDF) / Noise-Shaped). A "Normalize to" checkbox + typed LUFS field + "LUFS" suffix. A "Check against" spec combo + a "Measure" button. Two BLANK monospace readout lines. An "Export stems (one file per mixer strip)" checkbox. Export and Cancel. A hidden progress bar row.

**SHOT-190 — Export Audio, Quality combo per format (3 crops)**
- Reach: Open the Quality combo three times, once per Format setting.
- Visible: WAV — 16-bit / 24-bit / 32-bit float. OGG — Low / Medium / High / Highest. MP3 — 128 / 192 / 256 / 320 kbps.
- Why separate: The Quality combo repopulates from the Format combo; one shot of it cannot label the control.
- Variant of: SHOT-189

**SHOT-191 — Export Audio, stems list expanded**
- Reach: Tick "Export stems (one file per mixer strip)".
- Visible: The dialog GROWS and reveals a scrolling checkbox list of every currently-shown mixer strip — Master and buses default UNCHECKED, everything else checked.
- Why separate: The dialog physically resizes and gains a whole list that is otherwise invisible.
- Variant of: SHOT-189

**SHOT-192 — Export Audio, Custom spec row revealed**
- Reach: Set the "Check against" combo to "Custom".
- Visible: An extra row appears — "Custom ref (LUFS)" label + typed value field. The dialog grows by one row.
- Why separate: A control that exists only for one combo value.
- Variant of: SHOT-189

**SHOT-193 — Export Audio, measurement result**
- Reach: Click "Measure" and let it finish.
- Visible: The two monospace lines populated — line 1 "Integrated -13.4 LUFS   LRA 6.2 LU   True peak -0.9 dBTP"; line 2 the spec verdict ("<spec>: in spec", or the off-target / over amounts plus a flagged-spans count in parentheses). All option controls re-enabled.
- Why separate: Those readout lines are blank in every other state and carry the numbers the manual has to explain.
- Variant of: SHOT-189

**SHOT-194 — Export Audio, rendering state**
- Reach: Click Export, pick a destination, capture during the render.
- Visible: Every option control greyed; the progress bar visible with a right-aligned percentage ticking; Cancel still live; Export disabled.
- Why separate: The dialog flips wholesale into a progress screen.
- Variant of: SHOT-189

**SHOT-195 — Export Audio, destination file chooser**
- Reach: File > Export Audio... > Export (with a project already saved).
- Visible: Native save dialog "Export Audio", pre-pointed at the project's Exports folder with the song name and the extension matching the chosen format.

**SHOT-196 — "Save project first" prompt (export interlock)**
- Reach: On a project that has never been saved, File > Export Audio... > Export.
- Visible: Question-icon box "Save project first" explaining that exports land in the project's Exports folder; buttons "Save..." and "Cancel".
- Why separate: An interlock a beginner hits immediately, and it is where the export destination is explained.

**SHOT-197 — Export Project Bundle dialog**
- Reach: File > Export Project Bundle... (on a saved project).
- Visible: AlertWindow "Export Project Bundle" with two combos — "Bundle as" (Single .zip file / Plain folder) and "Contents" (Project files only (smallest) / Include my samples + outside files); Export (Return) and Cancel (Escape).

**SHOT-198 — Export Project Bundle, native size confirmation**
- Reach: Choose "Include my samples + outside files", click Export, pick a destination.
- Visible: A **NATIVE Windows question box** "Export Project Bundle": "This bundle will copy <size> of audio alongside the project." / "Continue?"; OK and Cancel.
- Why separate: The second of only two native decision dialogs in the app (the other is SHOT-108).

**SHOT-199 — Export Project Bundle, success summary**
- Reach: Complete a bundle export with all referenced files present.
- Visible: Info box "Export Project Bundle" naming the destination path and the count of extra files copied; OK.
- Note: The warning form of this box (missing / failed files) is SHOT-714.

---

## SITTING 10 — Mixer, cables and Master Analyzer

**SETUP:** A project with Layers, Bass, Drums, Clips, Vox and Inst tabs so every
strip class exists, plus one sfizz Inst tab (Guitars or Basses) for SHOT-207.
Create one send and one sidechain before the cable shots. Run an export +
Measure first so the Analyzer has a captured take (SHOT-229). A multi-channel
audio interface makes SHOT-212/214/216 far more useful.

**SHOT-200 — Mixer window, full view**
- Reach: Ribbon > Mixer (or F6).
- Visible: Title strip with "Menu" and "Add" headings; the FIXED Master strip pinned at the left with a 2px accent divider beside it; the horizontally scrolling console of bus + channel strips grouped by destination with neon divider lines (bright between a bus and its first member, dimmer between members); the cable overlay painting patch cables that dip below the strips; an always-visible 10px horizontal scrollbar under the strips.

**SHOT-201 — Mixer strip, anatomy close-up (insert strip)**
- Reach: Photograph a single Layer or Bass channel strip full height.
- Visible: Coloured accent bar (3px); editable name label; then two columns — LEFT: Mute / Solo LED pair, "FX Rack" button, utility row (FX Bypass LED), Pan knob, Polarity button reading "Standard", Width knob, vertical Level fader; RIGHT: 28px stereo dBFS meter with dB tick labels. Below both: the dB readout label ("-6.0 dB" or "-inf"), and the bottom socket row with the neon-green cable socket ring and the "+" add-cable button.
- Why separate: The canonical strip; every other strip class is described as a delta from this one.

**SHOT-202 — Mixer strip, Master**
- Reach: Photograph the pinned leftmost strip.
- Visible: Same anatomy PLUS a second utility row with the purple "Master FX Bypass" LED (global kill-all), a stacked LUFS readout box between the Width knob and the fader (value over mode title), NO polarity row, and **the "+" button repurposed as the Analyzer button.**
- Why separate: Master has two controls no other strip has and its "+" does something completely different.
- Variant of: SHOT-201

**SHOT-203 — Mixer strip, Bus strip expanded**
- Reach: Photograph a bus strip (Layers Bus / Bass Bus / Drums Bus / FX Bus / Clips Bus / Vox Bus / Inst Bus) with member strips to its right.
- Visible: The bus strip with its collapse arrow on the right of the name row pointing DOWN, plus the bright neon divider between it and its first member.

**SHOT-204 — Mixer strip, Bus strip collapsed**
- Reach: Click the small triangle on the right of a bus strip's name row.
- Visible: Arrow now points UP; the bus's member strips are hidden; the console re-packs.
- Why separate: A per-bus state with a two-direction arrow and a large layout consequence.
- Variant of: SHOT-203

**SHOT-205 — Mixer strip, Bus strip with nothing routed (arrow greyed)**
- Reach: Mixer > Add > Layers Bus — a freshly added secondary bus with no members.
- Visible: The bus strip with its collapse arrow greyed out and non-interactive.
- Variant of: SHOT-203

**SHOT-206 — Mixer strip, Vox strip (live input)**
- Reach: Create a Vox tab, then find its strip in the console.
- Visible: The utility row split THREE ways — Arm LED (red when armed), Listen LED (headphones glyph, green when monitoring), FX Bypass LED — versus the single full-width Bypass LED on non-live strips.
- Why separate: The three-LED utility row exists only on live-input Vox/Inst strips. **This is where record-arm lives — not on any vocal window** — so the Vox chapter must footnote it.
- Variant of: SHOT-201

**SHOT-207 — Mixer strip, Inst strip with no live input (sfizz engine)**
- Reach: Create an Inst tab sourced from BaySickGuitars or BaySickBasses, then look at its strip.
- Visible: Arm and Listen LEDs HIDDEN; only the full-width FX Bypass LED on the utility row.
- Why separate: Same strip type, two different utility rows depending on whether there is a live input.
- Variant of: SHOT-206

**SHOT-208 — Mixer strip name, rename in progress**
- Reach: Double-click a renameable strip's name label.
- Visible: The label as an editable text field with a caret. The hover tooltip on a renameable strip reads the full name plus "Double-click to rename".

**SHOT-209 — Mixer, Direct Routing group**
- Reach: On any insert strip click "+" > Move Output... > Master. The strip moves into the Direct Routing group.
- Visible: A narrow 28px vertical-text "Direct Routing" label panel at the left of the group, with the rerouted strips beside it.
- Why separate: A labelled group that only EXISTS when at least one strip sends straight to Master.

**SHOT-210 — Mixer, Add menu open**
- Reach: Mixer > click "Add" on the title strip.
- Visible: Aux Strip / sep / Vox Bus / Inst Bus / Layers Bus / Bass Bus / Clips Bus / Plugins Bus — each greyed once that bus is at its cap.

**SHOT-211 — Mixer, Menu dropdown open**
- Reach: Mixer > click "Menu" on the title strip.
- Visible: FOUR entries, no separators - "Pan Law" submenu (Ramped / Flat, tick on current, hover tooltip on each); "Master Output" submenu; "Latency-compensate meters" (tickable, unticked by default); "Multi-core Rendering" (tickable, ticked by default).

**SHOT-212 — Mixer, Master Output submenu open**
- Reach: Mixer > Menu > hover "Master Output".
- Visible: Stereo pairs listed as "1/2  (Out 1 / Out 2)", separator, then "Output N (mono)  (name)" entries, tick on the active routing. With no device open: a single greyed "(no audio device open)" row.
- Why separate: Device-dependent list plus a distinct no-device state.
- Variant of: SHOT-211

**SHOT-213 — Mixer, "+" cable menu on a strip**
- Reach: Click the "+" button at the bottom of any NON-Master strip.
- Visible: Five submenus — "Send..." (every legal aux target plus "New Aux Strip", with illegal/cycle-creating targets greyed), "Sidechain..." (every OTHER strip, with the ones whose four receive lines are full and the cycle-creating ones greyed), "Move Output..." (legal main-out destinations with a tick on the current one), "Add Main Out..." and "Remove Main Out..." (whose line-0 row is greyed and reads "<dest>  (main output)"). **The last three are ABSENT on strips whose output is locked, so a locked strip shows only two submenus.**

**SHOT-214 — Mixer, Arm LED input picker (Vox), armed**
- Reach: Mixer > RIGHT-click the Arm LED on a Vox strip (left-click just toggles arm).
- Visible: Section header "Vocal Input"; one entry per input channel / stereo pair with a tick on the current; sep; section header "Builder Grid Default" with Dry / Dry Cleaned / Wet / Wet Cleaned (tick = locked pick, no tick = automatic); sep; "Disarm".
- Why separate: **The only place the four take types are named in the UI.**

**SHOT-215 — Mixer, Arm LED input picker (Vox), unarmed**
- Reach: Same right-click on a strip that is not armed.
- Visible: Identical menu but with NO separator and no "Disarm" row.
- Variant of: SHOT-214

**SHOT-216 — Mixer, Arm LED input picker (Inst) and the no-device state**
- Reach: Right-click the Arm LED on a live-input Inst strip; then repeat with no audio device open.
- Visible: Section header "LiveInst Input" with the channel list and **NO "Builder Grid Default" section.** With no device: a single greyed "No channels available" row.
- Why separate: Inst drops a whole section, and the no-device state is a one-row menu.
- Variant of: SHOT-214

**SHOT-217 — Mixer, Listen LED monitor-mode menu (Vox)**
- Reach: Mixer > right-click the Listen (headphones) LED on a Vox strip.
- Visible: True Dry / Bypass Pitch Corrector / With Effect, tick on the current mode.

**SHOT-218 — Mixer, Listen LED monitor-mode menu (Inst)**
- Reach: Mixer > right-click the Listen LED on a live-input Inst strip.
- Visible: Dry / With Effect, tick on the current mode.
- Why separate: Two-mode menu vs the Vox three-mode menu on an identical-looking LED.
- Variant of: SHOT-217

**SHOT-219 — Mixer, Aux strip right-click menu**
- Reach: Mixer > Add > Aux Strip, then right-click the new strip's empty background.
- Visible: A single-entry menu "Delete Aux Strip".

**SHOT-220 — Mixer, secondary bus right-click menu**
- Reach: Mixer > Add > Vox Bus (or Inst / Layers / Bass / Clips / Plugins Bus), then right-click that bus strip's background.
- Visible: A single delete entry naming the bus, e.g. "Delete Vox Bus".
- Why separate: Only user-added secondary buses have a delete menu; system buses have none at all.
- Variant of: SHOT-219

**SHOT-221 — Mixer cable overlay, the three cable kinds**
- Reach: Create a send ("+" > Send... > an aux) and a sidechain ("+" > Sidechain... > another strip); leave main outputs as they are. Photograph the console with cables visible.
- Visible: Main-out cables, send cables and sidechain cables drawn as deep dual-stub beziers dipping BELOW the strips, plus the neon-green socket rings they terminate on and the Master cutout area.

**SHOT-222 — Mixer, send cable properties popup**
- Reach: Right-click directly on a SEND cable.
- Visible: A CallOutBox with a green header "Send -> <destination>", a horizontal amount slider (-60..+6 dB with a numeric box), a "Pre-Fader" toggle, and a red "Delete Send" button.

**SHOT-223 — Mixer, sidechain cable popup**
- Reach: Right-click directly on a SIDECHAIN cable.
- Visible: A CallOutBox with an info label "Sidechain: <source> -> <target>  (line N)" and a "Delete" button. **No amount slider, no pre/post toggle.**
- Why separate: Different popup with different controls from the send popup.
- Variant of: SHOT-222

**SHOT-224 — Mixer, overlapping cable chooser menu**
- Reach: Right-click at a point where two or more cables cross.
- Visible: A chooser menu listing every cable under the cursor; main-out cables appear greyed (they have no editable properties).
- Why separate: Only appears where cables overlap, and it is the only place main cables are listed at all.
- Variant of: SHOT-222

**SHOT-225 — MT Diagnostic prompt (RETIRED)**
- RETIRED (QA-Cleanup, commit ade5a10b): the "Run MT Diagnostic (2s capture)" entry and its prompt were deleted from the app. Nothing to photograph. The id is kept so the surrounding numbering does not shift.

**SHOT-226 — MT Diagnostic result (RETIRED)**
- RETIRED (QA-Cleanup, commit ade5a10b): the "MT Diagnostic Result" box and the counters behind it were deleted from the app. Nothing to photograph. The id is kept so the surrounding numbering does not shift.

**SHOT-227 — Master Analyzer, Loudness view, live**
- Reach: Ribbon > Mixer > on the MASTER strip click the button reading "Analyzer" (every other strip's reads "+"; its tooltip reads "Open the master analyzer - loudness, spectrum and the render report"). **This is the only entry point — there is no View-menu route.**
- Visible: Contained window "Master Analyzer" with its title strip and Menu button; a top control row with a source combo (defaulting to "Live"), a "vs ..." overlay combo, and disabled "Export Take..." / "Remove Take" buttons; a view bar — Levels / Loudness / Spectrum (current lit) and Reset; the Loudness body: short-term loudness as a filled cyan area with the momentary line over it, LU gridlines every 6 LU weighted below the target, a dashed green "TARGET -14.0" line, the loudness-range band shaded amber, a time axis, and side cells INTEGRATED (graded green / amber / red against the target), SHORT-TERM, MOMENTARY, LRA, MAX TP L / R.

**SHOT-228 — Master Analyzer, Spectrum view**
- Reach: Master Analyzer > Menu > View > Spectrum.
- Visible: Same frame; the view bar gains "Tilt 0" and "1/3 oct"; the plot is a log-frequency spectrum — vertical gridlines labelled 50, 100, 200, 500, 1k, 2k, 5k, 10k along the bottom, horizontal dB gridlines every 12 dB, a grey peak-hold trace over a filled cyan averaged trace, "MAX TP L / R" in the top-left corner, and two dBFS level bars at the right.
- Why separate: A completely different plot with different axes and legends.
- Variant of: SHOT-227

**SHOT-229 — Master Analyzer, showing a rendered / captured take**
- Reach: File > Export Audio... > "Measure" and let it finish (the Analyzer opens itself), or open the Analyzer and pick a take from its source combo.
- Visible: Loudness view with the take's curves (short-term fill + momentary line); the side cells carry the take's INTEGRATED, its last SHORT-TERM / MOMENTARY values, its LRA and its true peak; an orange "TAKE: <label>" caption top-right; "Export Take..." and "Remove Take" enabled. Wheel / drag zooms and pans the whole take.
- Why separate: Half the readout cells go blank and an orange caption appears — a state a reader will otherwise think is a broken meter.
- Variant of: SHOT-227

**SHOT-230 — Master Analyzer, source combo dropdown open**
- Reach: Open the Master Analyzer and click the source combo at the top.
- Visible: "Live" plus one row per captured take (timestamp-style labels); takes captured without audio carry a trailing "   (no audio)" suffix.

**SHOT-231 — Master Analyzer, window Menu dropdown**
- Reach: Master Analyzer > click "Menu" in its title strip.
- Visible: Section headers — "View" (Loudness / Spectrum, one ticked); "Source" (Live plus every take, or a disabled "No captured takes yet" row); "Target" (one row per loudness spec that checks integrated loudness, plus a "Custom..." / "Custom (-x.x LUFS)..." row); sep; "Reset history".

**SHOT-232 — Master Analyzer, Custom Target prompt**
- Reach: Master Analyzer > Menu > Target > Custom...
- Visible: Small dialog "Custom Target"; body "Target loudness in LUFS (-40 to 0):"; one text field pre-filled with the current target; OK and Cancel.

---

## SITTING 11 — Effects rack index and the EQ windows

**STANDARD ROUTE for this and the next two sittings:** Ribbon > Effects opens
the RACK window. Pick a strip in the "Channel:" dropdown. Click the CHEVRON on a
slot row to open the effect picker. After loading, click the slot's NAME PLATE
to open that effect in its own window. The effect window's hamburger Menu is
where Basic/Advanced, Mode, SC, Presets and Visual all live — **there are no
buttons on the effect window itself.**

**SETUP:** One strip with an empty rack (a fresh Master) and one strip with six
different effects loaded. At least one VST3 effect added via Options > Plugins.
At least one sidechain cable routed to one strip.

**SHOT-233 — Effects rack window, empty**
- Reach: Ribbon > Effects, pick a channel with no effects loaded.
- Visible: "Channel:" label + channel dropdown; blue "FX Bypass" LED button; "Pre EQ" and "Post EQ" buttons side by side; six fixed-height slot rows, each with a dark LED well (unlit), a dim name plate reading "Empty", up/down triangles, the chevron picker glyph, and **no bypass LED and no red X.**

**SHOT-234 — Effects rack window, populated**
- Reach: Load six effects (chevron on each row > pick), then bypass one by clicking its LED.
- Visible: Same chrome, but each row now shows a lit bypass LED at the left (one dimmed/bypassed), the effect name in BOLD on a brighter plate, active up/down move triangles, the chevron, and a red X remove cross.
- Why separate: Empty vs loaded rows differ in four ways at once — LED present, plate brightness, bold text, X present. This is exactly the ambiguity a visual atlas exists for.
- Variant of: SHOT-233

**SHOT-235 — Effects slot row, bypassed**
- Reach: Click a loaded row's LED at the far left.
- Visible: The row's bypass LED in its bypassed colour. Hovering shows "Bypassed - click to enable" (vs "Active - click to bypass").
- Why separate: Two-state LED and the most-clicked control on the row.
- Variant of: SHOT-234

**SHOT-236 — Effects, channel dropdown open**
- Reach: Ribbon > Effects > click the "Channel:" dropdown.
- Visible: The channel list grouped by bus with coloured section headings — MASTER, DIRECT ROUTING, Layers Bus, Bass Bus, Drums Bus, FX Bus, Clips Bus, Vox Bus, Inst Bus, Rusty Drums Bus, Plugins Bus — with each group's member strips underneath.

**SHOT-237 — Effect picker menu, top level**
- Reach: Click the chevron (or an "Empty" name plate) on any slot row.
- Visible: Section header "Dynamic" (Compressor, De-esser, Gate, Limiter, Transient Shaper); "Harmonics" (Overdrive, Saturation); "Modulation" (Chorus, Flanger, Phaser); "Time" (De-reverb, Delay, Reverb); then two bold group-heading rows that are themselves submenus — "Pedals" and "VST Plugins".

**SHOT-238 — Effect picker, Pedals submenu**
- Reach: In the effect picker, hover "Pedals".
- Visible: Sub-headers Dynamics (Bass Compressor, Noise Gate); Harmonics (Bass Driver, Bass Overdrive, Blues Drive, Distortion, Fuzz, High-Gain, Octave); Modulation (Acoustic Simulator, Polyphonic Synth, Wah); Time (Acoustic Preamp).
- Why separate: A whole second grouped list, invisible from the top level, and the only route to several panels in a rack slot.
- Variant of: SHOT-237

**SHOT-239 — Effect picker, VST Plugins populated**
- Reach: Add at least one VST3 effect via Options > Plugins, then open the picker and hover "VST Plugins".
- Visible: An alphabetical list of every added effect plugin.
- Variant of: SHOT-237

**SHOT-240 — Effect picker, VST Plugins with none added**
- Reach: With no plugins added, open the effect picker on a rack slot.
- Visible: Section header "VST Plugins" and ONE greyed, non-clickable row: "None added - see Options > Plugins".
- Why separate: A deliberate disabled empty-state row rather than a hidden section.
- Variant of: SHOT-239

**SHOT-241 — Effects, Menu dropdown (rack presets + VU calibration)**
- Reach: Ribbon > Effects > click "Menu" on the window title strip.
- Visible: Save FX Rack Preset... / "Load FX Rack Preset" submenu (greyed when nothing is saved) / sep / Open Presets Folder / sep / "VU Calibration (0 VU = ...)" submenu with -18 through -14 dBFS and a tick on the current setting / "VU Meter".

**SHOT-241b — VU Meter window**
- Reach: Effects > Menu > "VU Meter".
- Visible: A 180x200 window titled "VU Meter" holding one analog VU face reading the master output, with CURRENT and MAX readout boxes beneath the needle. Resizes diagonally only (ratio locked at 180:200) and caps at 290x320.
- Why separate: The only VU in the app and the only ratio-locked window.

**SHOT-241c — VU Meter window, Menu dropdown**
- Reach: VU Meter window > click "Menu".
- Visible: The "VU Calibration (0 VU = ...)" submenu alone, -18 through -14 dBFS with a tick on the current setting - the SAME app-wide value the Effects rack menu sets.

**SHOT-242 — Effects, Save FX Rack Preset dialog**
- Reach: Effects > Menu > Save FX Rack Preset...
- Visible: "Save FX Rack Preset" box with the line "Saves all six slots and both EQs for this channel.", a "Preset name:" field pre-filled "<channel> Rack", Save and Cancel.

**SHOT-243 — Effects, remove-effect confirmation**
- Reach: Click the red X on a loaded slot row.
- Visible: The confirmation prompt shown before the slot is cleared.

**RE-SHOOT REQUIRED (QA-EqPro 2026-08-26; scope grown at QA-EqFlagship
2026-08-27):** every EQ shot below describes the NEW window (the KBS EQ Pro
port: views + chips + rail), and the flagship pass changed the surface
again before any re-shoot happened - so the re-shoot set now also needs:
the paged chip row with its arrows (a >24-band project), the A/B MORPH
strip, the rail's SLOPE/PHASE/SAT numbers + second dynamics stage (THR B /
RATIO B / RANGE B / ONSET / DENSE) + MOD block, the 9-type grid, the
grown window menu (Color / Whole Curve / Delta Listen / Sketch a Curve /
Instances), the grown band menu (All Pass, Spectral, Delta Listen, Split
to Left + Right, Link/Unlink), the grown Match panel (Scan Track /
Selection, Stored Spectra, AMOUNT, Auto Cleanup), and one NEW master: the
Instances panel (`EQ Instances.png` - the browser open over the graph,
several rows with thumbnails + live mini-spectra, one row current).  The
old masters `EQ.png` and `EQ Band Menu.png` show the retired 8-band panel
and must be replaced; the manual's marker coordinates for `EQ` and `EQB`
are approximate until the new masters exist.

**SHOT-244 - Pre EQ and Post EQ open together (index proof)**
- Reach: Effects > click "Pre EQ", then click "Post EQ" so both windows are open at once alongside the rack window.
- Visible: Two EQ windows open simultaneously beside the rack index window.
- Why separate: **The Effects page is an INDEX, not an editor.** A reader looking for EQ controls on the Effects page will otherwise be lost; this is the only way to show the one-window-per-thing model.

**SHOT-245 - Pre EQ window (the master for `EQ.png`)**
- Reach: Ribbon > Effects, pick a channel, click "Pre EQ".  Add a few bands, one dynamic, one in the Side view, audio playing.
- Visible: Title strip with the "Pre EQ" / "Post EQ" tab pair (Pre active) and the hamburger Menu.  Top row: ST / MID / SIDE view segments, the 24 band chips, "+", and the A/B pill.  The graph: dark ground, dotted grid, live spectrum, white glowing summed curve, coloured numbered handles with type glyphs (a dynamic band with its second ring + mini meter; the ghost of the other views' curves faint behind), the crosshair grab button top-right, the selected band's headphone button.  Right: the rail - BAND header, GAIN/PAN knobs, FREQ/Q numbers, the type glyph grid, ST/L/R, slope box, and the DYNAMICS section with DOWN/UP, THR/RATIO/ATK/REL and the GR meter.

**SHOT-246 - Post EQ window**
- Reach: Ribbon > Effects, pick a channel, click "Post EQ".
- Visible: Identical layout; the title reads "<Strip> - Post EQ" and the Post EQ tab is the active one.
- Why separate: Two windows that can be open at once and look identical except for the title and which tab is lit.
- Variant of: SHOT-245

**SHOT-247 - EQ window, Side view with ghost**
- Reach: Pre EQ window > click SIDE in the view row, with bands living in more than one view.
- Visible: SIDE active; the Side bands full-strength; the Stereo/Mid views' curves and dots faint (the ghost); chips whose bands live elsewhere wear a tiny M or S tick.
- Why separate: The ghost is the only cue the other half still exists; without it a reader thinks their bands vanished.
- Variant of: SHOT-245

**SHOT-248 - EQ, out-of-the-box state**
- Reach: Pre EQ window > Menu > Presets > "Default".
- Visible: Flat response line, eight handles on the zero line at the home frequencies (40 to 12.5k), bands 9-24 off in the chip row.
- Why separate: The blank-slate state - what a reader sees on a brand-new strip.
- Variant of: SHOT-245

**SHOT-249 - EQ band right-click menu (the master for `EQ Band Menu.png`)**
- Reach: Pre EQ window, STEREO view > right-click a band handle.
- Visible: Submenus "Type", "Slope" (filter bands), "Channel" (Stereo view only), "Move to", "Dynamic"; then "Listen", "Isolate", "Mute"; sep; "Reset Band", "Delete Band".  The menu opens AT THE MOUSE.

**SHOT-250 - EQ band menu, Type submenu**
- Reach: Right-click a band handle > hover "Type".
- Visible: Bell, Low Pass, High Pass, Low Shelf, High Shelf, Notch, Band Pass, Tilt (current ticked; no "Off" - delete is the off).

**SHOT-251 - EQ band menu, Slope submenu**
- Reach: Right-click a FILTER band's handle > hover "Slope".
- Visible: 6 dB/oct, 12 dB/oct, 18 dB/oct, 24 dB/oct, 36 dB/oct, 48 dB/oct, 72 dB/oct, 96 dB/oct, Brickwall (current ticked).

**SHOT-252 - EQ band menu, Channel + Move to (Stereo view)**
- Reach: Stereo view > right-click a band handle > hover "Channel", then "Move to".
- Visible: Channel offers Stereo / Left / Right only; Move to offers "Mid view" / "Side view".  In the Mid or Side view the Channel submenu is ABSENT and Move to offers the other two views.
- Why separate: The picker deliberately never offers Mid/Side - the views carry that - and readers of any other EQ will look for them here.

**SHOT-253 - EQ, dynamic band engaged**
- Reach: DYN on a bell, direction DOWN, THR pulled down, audio playing.
- Visible: The dashed EXTENT curve outside the live curve (it moves with THR), the handle's mini GR meter filling, the rail's GR meter agreeing, DOWN lit in the direction row.
- Why separate: A second dotted curve with no legend anywhere - and the proof the two meters read the same movement.
- Variant of: SHOT-245

**SHOT-254 - EQ, EXT sidechain pick**
- Reach: A dynamic band selected, a source routed into one of the strip's receive slots > click EXT on the rail.
- Visible: A menu listing "This band's own input" and the four receive lines by their routed source names (unrouted lines greyed "not routed").

**SHOT-255 - EQ options menu (hamburger)**
- Reach: Pre EQ window > click the hamburger Menu in the title strip.
- Visible: "Processing Mode" submenu; "Oversampling 2x", "Proportional Q"; sep; "Auto-Gain" submenu, "Output Trim" submenu; sep; "Gain Scale", "Analyser", "View" submenus; sep; "Keyboard & Mouse...", "Reset All Bands"; sep; "EQ Match...", "Presets" submenu.

**SHOT-256 - EQ options, Processing Mode submenu**
- Reach: Menu > hover "Processing Mode".
- Visible: Zero Latency, Natural Phase, Linear Low, Linear Medium, Linear High, Linear Very High, Linear Maximum - **each with its real delay computed at the session rate, like "(35 ms (1535 sp))".**
- Why separate: The figures are computed live from the engine's own constants - a whole concept to explain.

**SHOT-257 - EQ, spectrum grab armed**
- Reach: Click the crosshair top-right of the graph, music with a resonance playing, hover empty graph.
- Visible: The crosshair lit, the found-peak marker (circle + drop line + "N Hz grab" readout) holding steady on the resonance.
- Why separate: The marker only exists while armed - the arming model needs its picture.

**SHOT-258 - EQ Match panel**
- Reach: Menu > "EQ Match...".
- Visible: The floating panel over the graph's right edge - Capture Current, Capture Reference (SC), Load Reference File..., the SMOOTH slider and BANDS counter, the status line, Match and Close, with the two capture lights.

**SHOT-259 - EQ Presets submenu**
- Reach: Menu > hover "Presets".
- Visible: "Default" first, then the factory categories (Cleanup / Vocals / Drums / Bass / Master) as submenus, any user presets, and "Save Preset...".

**SHOT-260 - EQ, spectrum analyser running**
- Reach: Pre EQ window with audio playing through the strip.
- Visible: A translucent grey PRE spectrum behind and the app-yellow POST spectrum in front, both under the white curve.
- Why separate: Two overlaid spectra that are easy to read as one.
- Variant of: SHOT-245

**SHOT-261 - EQ, spectrogram on**
- Reach: Menu > View > "Spectrogram", with audio playing.
- Visible: The scrolling heat-mapped history (blue through cyan and yellow into red) behind the curve.
- Variant of: SHOT-245

**SHOT-262 - EQ, phase overlay + piano strip on**
- Reach: Menu > View > "Phase Overlay" and "Piano Strip".
- Visible: The orange phase curve over the graph, and the piano strip along the bottom with every C named.
- Variant of: SHOT-245

**SHOT-263 - EQ A/B pill, A vs B**
- Reach: Capture the pill at the right of the chip row, click it, capture again.
- Visible: "A" (dim) then "B" (lit yellow) after the swap; right-click shows "Copy A to B" / "Lock banks".
- Why separate: A two-state control that changes what every band on the page is.

**SHOT-264 - EQ drag readout + hover panel**
- Reach: Drag a band (capture the floating readout), then release and hover it.
- Visible: While dragging - "1.02 kHz   C6 +2c   +4.5 dB   Q 1.41" floating at the handle; on hover - the band summary line (band number, frequency, gain, live GR when dynamic).

**SHOT-265 - EQ readout inline edit**
- Reach: Double-click the FREQ or Q number on the rail.
- Visible: A small text editor over the number, pre-filled and select-all'd, awaiting Enter or Escape.

---

## SITTING 12 — Effect window chrome and the shared effect menus

**SETUP:** A Compressor and a Gate loaded on one strip, plus a strip with a
sidechain routed to it and a strip with none.

**SHOT-267 — Effect window, chrome only**
- Reach: Load a Compressor and click its slot name plate. Photograph the window frame, not the panel guts.
- Visible: A native child window inside the fullscreen frame; title strip reading "<Strip> - <Effect>" (e.g. "Master - Compressor"); hamburger Menu button; bypass LED at the right of the title strip; resize edges; the panel bed below.

**SHOT-268 — Effect window, bypass LED lit vs bypassed**
- Reach: Screenshot the title-strip LED, click it, screenshot again.
- Visible: The LED in active state and in bypassed state (tooltip "Bypass this effect").
- Why separate: A two-state indicator with no text label.
- Variant of: SHOT-267

**SHOT-269 — Effect window Menu, full**
- Reach: Compressor effect window > click the hamburger Menu.
- Visible: "Show Advanced Controls" (or "Show Basic Controls"), "Mode: Modern...", "SC: Off...", sep, "Presets...", then the standard tail including "Visual".

**SHOT-270 — Effect window Menu, minimal**
- Reach: Gate effect window > Menu.
- Visible: Only "Presets..." — no Basic/Advanced row (Gate has no advanced controls), no Mode row, no SC row, and **no Visual row** (Gate publishes no visual).
- Why separate: The menu's rows are conditional. **Only ten effects have a Visual at all — Limiter, Compressor, Chorus, Flanger, Phaser, Delay, Reverb, Transient Shaper, Saturation and Tape.** On the rest the row is absent, not greyed, so "my effect has no Visual item" reads as a bug unless the atlas shows both menus.
- Variant of: SHOT-269

**SHOT-271 — Mode menu, Compressor**
- Reach: Compressor effect window > Menu > "Mode: Modern...".
- Visible: Modern (ticked), FET (Punchy), Opto (Smooth), Pedal (Sustain). On a VOCAL CHAIN slot the fourth row is absent - that menu is three items (Jeff, 2026-08-11: a pedal sustainer is not a vocal-chain compressor).

**SHOT-272 — Mode menu, Saturation**
- Reach: Saturation effect window > Menu > "Mode: Tube...".
- Visible: Tube, Console, Tape (current ticked).

**SHOT-273 — Mode menu, Delay**
- Reach: Delay effect window > Menu > "Mode: Echo...".
- Visible: Echo, Doubler (current ticked).

**SHOT-274 — Mode menu, Reverb**
- Reach: Reverb effect window > Menu > "Mode: Hall...".
- Visible: Plate, Hall, Chamber, Room, Booth (current ticked). **Reverb's Mode changes the algorithm only — it does NOT redraw the panel**, unlike Compressor / Saturation / Delay / Overdrive.

**SHOT-275 — Mode menu, Overdrive**
- Reach: Overdrive effect window > Menu > "Mode: Rack...".
- Visible: Rack, Pedal (current ticked).

**SHOT-276 — Mode menu, Limiter**
- Reach: Limiter effect window > Menu > "Mode: Limiter...".
- Visible: Limiter, Maximizer (current ticked).

**SHOT-277 — Sidechain menu, sources routed**
- Reach: On a strip with at least one sidechain cable routed to it, Compressor effect window > Menu > "SC: Off...".
- Visible: "Off" (ticked when no pick) plus one row per routed SC receive line, each labelled with the friendly source-strip name.
- Note: A hosted VST3 effect that declares a side-chain input opens the identical menu from the identical "SC: Off..." row; a plugin running bridged gets no "SC:" row at all, so shoot both if a bridged plugin is on hand.

**SHOT-278 — Sidechain menu, nothing routed**
- Reach: On a strip with no sidechain cables, Compressor effect window > Menu > "SC: Off...".
- Visible: "Off", a separator, and a greyed row "(no sidechain cables routed to this strip)".
- Why separate: Empty-state text a reader will otherwise assume is an error.
- Variant of: SHOT-277

**SHOT-279 — Presets menu**
- Reach: Any effect window > Menu > "Presets...".
- Visible: "Save Current Preset..."; "Load: Factory" submenu; "Load: My Presets" submenu; sep; "Restore Defaults"; "Save Current as Default"; sep; "Manage Presets... (open folder)".
- Why separate: **Identical on all 45 effect panels AND on all six vocal-chain stages** — shoot it once here and footnote it from everywhere else.

**SHOT-280 — Presets > Load: Factory submenu**
- Reach: Effect window > Menu > Presets... > hover "Load: Factory".
- Visible: One row per factory preset file name, or a single greyed "(no factory presets)" row.

**SHOT-281 — Presets > Load: My Presets, empty**
- Reach: Effect window > Menu > Presets... > hover "Load: My Presets" before saving anything.
- Visible: A single greyed row "(no user presets yet)".
- Variant of: SHOT-280

**SHOT-282 — Save Effect Preset dialog**
- Reach: Effect window > Menu > Presets... > "Save Current Preset...".
- Visible: Question-icon modal "Save Preset", prompt "Name this preset:", a single-line text editor with placeholder "Preset name", Save and Cancel.

**SHOT-283 — Recurring effect-widget plate (one labelled close-up)**
- Reach: Compose from any two or three effect panels; zoom each widget to readable size.
- Visible: One labelled figure covering the widgets that repeat across the whole effect set — the VKnob (knob + label + value ring); the CHICKEN-HEAD selector (a rotary with a lettered bezel, each mark carrying its own tooltip; used for every multi-choice control); the DualLabelToggle in both flavours (OnOff — feature name above, OFF/ON either side of the switch; Named — one label above and one below); the vertical VU input meter; the narrow dBFS output meter; the square GR meter; the GateGRMeter (0 to -80, red at the open end); the EQFader (rectangular cap, centre-detent notch, snaps to 0 dB); and the Reverb's JewelIndicator.
- Why separate: Saves re-explaining nine widgets on 57 panel pages. Manual 1's effect chapter should open with this plate.

---

## SITTING 13 — Effect panels (rack faces)

**SETUP:** One strip, and audio playing through it for the meter shots. Work
down the list loading each effect into slot 1 in turn. Remember: Basic/Advanced
is **per slot and persisted with the project**, and the header/menu label reads
the CURRENT state ("Basic" means it is currently basic).

**Panel background art groups the effects and is itself a labelling target:**
DynamicsLAF cream/wood plate (all compressors, Limiter, Transient Shaper,
De-esser, Gate, De-reverb, Noise Gate, Bass Compressor); HarmonicLAF olive
hammerite plate (Saturation, Tape, Overdrive, the drive pedals); TimeLAF dark
Pultec plate (Delay, Reverb, Acoustic Preamp, the graphic EQs); ModulationLAF
plain dark panel (Chorus, Flanger, Phaser, Wah, Synth, Acoustic Simulator, Tuner).

### Dynamics

**SHOT-284 — Compressor (Modern), Basic**
- Reach: Effects > chevron > Dynamic > Compressor > name plate. Basic is the default.
- Visible: Cream/wood LA-2A-style plate. Left: tall vertical VU input meter then a square GR meter. Centre knob strip: Thresh, Ratio, Gain, KneeType chicken-head (bezel H / M / V / S / H-R / M-R / V-R / S-R), Attack, Release. Right edge: Vol knob then a narrow dBFS output meter.

**SHOT-285 — Compressor (Modern), Advanced**
- Reach: Compressor window > Menu > "Show Advanced Controls".
- Visible: Same meters; the knob strip becomes Thresh, Ratio, KneeW, Gain, KneeType, Attack, Release, Mix, LookA, Det, SCHPF; a two-column toggle block appears left of the Vol knob — Auto MU switch, and stacked Link / RMS-Peak switches.
- Why separate: Five extra knobs mid-strip and a whole toggle column.
- Variant of: SHOT-284

**SHOT-286 — Compressor, FET mode**
- Reach: Compressor window > Menu > Mode > "FET (Punchy)".
- Visible: 1176-style minimal face — VU meter, GR meter; four knobs Input / Output / Attack / Release; a 5-position Ratio chicken-head (4 / 8 / 12 / 20 / All-buttons-in); a 4-position meter-mode chicken-head (GR / +8 / +4 / OFF); Vol knob; dBFS meter. **No Basic/Advanced toggle in the menu.**
- Why separate: A completely different panel CLASS, not a re-flow — and **it has no Threshold knob at all**, the single most confusing thing about it.

**SHOT-287 — Compressor, Opto mode**
- Reach: Compressor window > Menu > Mode > "Opto (Smooth)".
- Visible: LA-2A-style minimal face — VU + GR meters; a Comp/Limit two-position switch; two oversized 0-100 knobs "Peak Reduction" and "Gain"; a 3-position meter-mode chicken-head (GR / +10 / +4); Vol knob; dBFS meter.
- Why separate: Third panel class; knobs are 0-100 face-plate scales, not dB.
- Variant of: SHOT-286

**SHOT-288 — Compressor, Pedal mode**
- Reach: Compressor window > Menu > Mode > "Pedal (Sustain)".
- Visible: Pedal-style face — four knobs Level / Tone / Attack / Sustain; GR meter; dBFS meter. **This panel DISABLES the shared right-edge Output Vol knob** (it owns its own Level), so the right edge looks different from every other stage.
- Why separate: Fourth compressor face and the only one missing the standard Vol knob.
- Variant of: SHOT-286

**SHOT-289 — Compressor GR meter under signal**
- Reach: Modern Compressor panel, set Thresh low, play audio, capture while the meter moves.
- Visible: GR meter deflecting, VU input meter reading, dBFS output meter reading.
- Why separate: Meters read as dead boxes when nothing is playing; the atlas needs one live example to anchor every other meter caption.
- Variant of: SHOT-284

**SHOT-290 — Limiter, Limiter mode Basic**
- Reach: Effects > chevron > Dynamic > Limiter > name plate. Mode = Limiter, Basic on.
- Visible: Cream plate; VU meter, GR meter at the left; a single knob row InGain, Ceil, SatTh, Atk, Rel, Sustain; Vol knob; dBFS meter. **No character selector, no LUFS controls, no loudness meter.**

**SHOT-291 — Limiter, Limiter mode Advanced**
- Reach: Limiter window (Mode = Limiter) > Menu > "Show Advanced Controls".
- Visible: Two knob rows — row 1 InGain, Ceil, SatTh, SatCv, SCHPF; row 2 Atk, Rel, Ahead, RelCv, Sustain — plus a toggle block of Auto (release) over Link, and Auto MU beside them.
- Variant of: SHOT-290

**SHOT-292 — Limiter, Maximizer mode Basic**
- Reach: Limiter window > Menu > Mode > "Maximizer (Loudness)", Basic left on.
- Visible: EXACTLY six things — the 8-position Character chicken-head (bezel Cl / Sm / Ti / Pu / Gl / Lo / Wa / In); the "LUFS" target knob with its "Target" toggle; the "dBTP" knob with its "Auto Ceil" toggle; the LUFS/dBFS Loudness meter; and the GR meter. **InGain / Ceil / SatTh / Atk / Rel / Sustain are all HIDDEN here.**
- Why separate: Maximizer Basic hides everything Limiter Basic shows and shows controls Limiter mode never shows — the two modes look like different plugins, and it is the opposite of what "Basic" implies everywhere else.
- Variant of: SHOT-290

**SHOT-293 — Limiter, Maximizer mode Advanced**
- Reach: Limiter in Maximizer mode > Menu > "Show Advanced Controls".
- Visible: Three knob rows — row 1 InGain/Ceil/SatTh/SatCv/SCHPF; row 2 Atk/Rel/Ahead/RelCv/Sustain; row 3 Character chicken-head + LUFS + dBTP — plus three toggle columns (Auto over Link; Auto MU; Target over Auto Ceil); VU, GR, Loudness meter, Vol, dBFS.
- Why separate: The fullest state of the busiest panel in the set. Mode and Basic/Advanced are INDEPENDENT axes here, so all four combinations are genuinely different.
- Variant of: SHOT-292

**SHOT-294 — Loudness meter close-up**
- Reach: Limiter in Maximizer mode with audio playing; zoom on the twin-bar meter left of the knobs.
- Visible: Left bar = LUFS short-term (cyan, turning orange over target); right bar = output peak dBFS (grey, turning orange inside the last dB); a dashed cyan horizontal target line across the LUFS bar; two monospaced numeric readouts at the bottom ("--" when silent).
- Why separate: Two unlabelled bars with a dashed line — unreadable without a labelled figure.

**SHOT-295 — Transient Shaper, Basic**
- Reach: Effects > chevron > Dynamic > Transient Shaper > name plate.
- Visible: Cream plate; VU meter left; a single knob row Attack, Release, Split, Drive, Gain, Wet; two chicken-heads at the right — Attack Shape (Sh/Md/Sf) and Release Shape (Sh/Md/Sf); Vol knob; dBFS meter.

**SHOT-296 — Transient Shaper, Advanced**
- Reach: Transient Shaper window > Menu > "Show Advanced Controls".
- Visible: Two rows — row 1 Attack, Release, FastRel, SlowAtt, Sens with both Shape chicken-heads and a Mono Det / Stereo Det switch at the right; row 2 Split, Balance, Drive, Gain, Wet with the oversampling chicken-head (2x/4x/8x/16x) at the right.
- Why separate: Basic is one row, Advanced is two rows with knobs regrouped by function — the layouts do not overlap.
- Variant of: SHOT-295

**SHOT-297 — De-esser, Basic**
- Reach: Effects > chevron > Dynamic > De-esser > name plate.
- Visible: Cream plate; knob row Detect, Thresh, Range, Mode; a "Monitor" on/off switch to the right of the knobs; Vol knob; dBFS meter. **No input VU on this panel.**

**SHOT-298 — De-esser, Advanced**
- Reach: De-esser window > Menu > "Show Advanced Controls".
- Visible: Two rows — row 1 Detect, Thresh, Range, Mode, Freq, Q; row 2 Atk, Rel, Look, Mix plus the "Spectral" engine switch, the "LowLat" quality switch and the Stereo/Mid/Side chicken-head (St/M/S) at the right; Monitor switch stays at the far right.
- Why separate: Ten knobs vs four; a reader cannot find Freq or the Spectral engine from the Basic shot.
- Variant of: SHOT-297

**SHOT-299 — De-esser, locked during playback**
- Reach: Put the De-esser in Advanced, start the transport, capture. Then flip Spectral ON as well.
- Visible: "Spectral" and "LowLat" greyed/disabled while the transport runs (they change latency); separately, the "Look" knob greys out whenever the Spectral engine is active because it is a no-op there.
- Why separate: **Two independent greying rules on one panel, neither visible in a stopped screenshot and neither explained on screen.**
- Variant of: SHOT-298

**SHOT-300 — Gate panel**
- Reach: Effects > chevron > Dynamic > Gate > name plate.
- Visible: Cream plate; a gate-scaled GR meter on the left (0 to -80, red at the open end — NOT the standard GR meter); knobs Thresh, Range, Attack, Hold, Release; Vol knob; dBFS meter. No Basic/Advanced.

**SHOT-301 — De-reverb panel**
- Reach: Effects > chevron > Time > De-reverb > name plate.
- Visible: Cream plate; standard GR meter left; three knobs Reduce, Tail, Mix; Vol knob; dBFS meter. No Basic/Advanced.

### Harmonics

**SHOT-302 — Saturation (Tube), Basic**
- Reach: Effects > chevron > Harmonics > Saturation > name plate (Mode defaults to Tube).
- Visible: Olive hammerite plate; VU meter left; row 1 Flowers, Dabs, Input, BassRlf with the oversampling chicken-head (2x/4x/8x/16x) at the right; row 2 TonePre, TonePost, Wet, Out with the Tube Type chicken-head (A/B in Basic) and the "Trans" transformer on/off switch; Vol knob; dBFS meter.
- Note: The knob vocabulary ("Flowers", "Dabs") is unusual and must be labelled explicitly.

**SHOT-303 — Saturation (Tube), Advanced**
- Reach: Saturation window (Tube) > Menu > "Show Advanced Controls".
- Visible: Same two rows plus the "Auto MU" toggle with its small black dB-compensation READOUT label on row 1, the harmonics-routing chicken-head (Keep Low / Normal / Keep High) on row 2, and the Tube Type selector now offering A/B/**C**.
- Why separate: Advanced adds a numeric readout and changes the option COUNT on an existing selector.
- Variant of: SHOT-302

**SHOT-304 — Saturation (Console), Basic**
- Reach: Saturation window > Menu > Mode > "Console".
- Visible: Olive plate; VU meter; knobs Drive, Color, Output; a Color master on/off switch; a Clean/Dirty chicken-head (Cln/Drt); Vol knob; dBFS meter.
- Why separate: A different panel CLASS from Tube — different knob set entirely.
- Variant of: SHOT-302

**SHOT-305 — Saturation (Console), Advanced**
- Reach: Saturation in Console mode > Menu > "Show Advanced Controls".
- Visible: Adds the Mix knob between Color and Output, and the harmonics-routing chicken-head (Lo/Nrm/Hi) at the right.
- Variant of: SHOT-304

**SHOT-306 — Tape, Basic**
- Reach: Saturation window > Menu > Mode > "Tape".
- Visible: Olive plate; VU meter; knobs Drive, LoPass, Hiss, WowDp, FlutDp; a Cassette chicken-head numbered 1-10; an IR on/off switch; Vol knob; dBFS meter.
- Why separate: Third distinct Saturation panel class.
- Variant of: SHOT-302

**SHOT-307 — Tape, Advanced**
- Reach: Saturation in Tape mode > Menu > "Show Advanced Controls".
- Visible: Two rows — row 1 Drive, Hiss, LoPass, Vibe, Hyst with the OS chicken-head, the Tape Speed chicken-head (7.5 / 15 / 30 ips), the Cassette picker and the IR switch at the right; row 2 WowHz, WowDp, FlutHz, FlutDp, PreShf, DeShf, Bias.
- Variant of: SHOT-306

**SHOT-308 — Overdrive, Basic**
- Reach: Effects > chevron > Harmonics > Overdrive > name plate (Mode defaults to Rack).
- Visible: Olive plate; VU meter left; knobs Drive, Color, Band, Filter, Out; an "x100" on/off switch at the right; Vol knob; dBFS meter.

**SHOT-309 — Overdrive, Advanced**
- Reach: Overdrive window > Menu > "Show Advanced Controls".
- Visible: Adds the Bias knob (after Band) and the Wet knob (after Out), plus a Blend/Parallel switch and the oversampling chicken-head (2/4/8/16) in the right sidebar beside x100.
- Variant of: SHOT-308

**SHOT-310 — Overdrive, Pedal mode**
- Reach: Overdrive window > Menu > Mode > "Pedal".
- Visible: Olive plate; three right-clustered knobs Drive, Tone, Level; dBFS meter at the right edge; **a deliberately EMPTY region filling the left half of the panel.**
- Why separate: Mode swaps the whole panel class to a three-knob pedal face whose empty left side looks like a rendering bug if unexplained.
- Variant of: SHOT-308

### Modulation

**SHOT-311 — Chorus, Basic**
- Reach: Effects > chevron > Modulation > Chorus > name plate.
- Visible: Dark modulation plate; row 1 LFO1, LFO2, LFO3 rate knobs on the left half and three LFO-wave chicken-heads (S/T/M/O) on the right half; row 2 Delay, Depth, Stereo, CrossHz knobs with a Cross Type switch (HP band / LP band) and a Wet Only switch at the right; Vol knob; dBFS meter. **No input VU.**

**SHOT-312 — Chorus, Advanced**
- Reach: Chorus window > Menu > "Show Advanced Controls".
- Visible: Adds a "3 voices / 6 voices" switch to the row-2 right cluster and a continuous Wet knob at the end of the row-2 knobs.
- Variant of: SHOT-311

**SHOT-313 — Flanger panel**
- Reach: Effects > chevron > Modulation > Flanger > name plate.
- Visible: Dark modulation plate; knob row Rate, Depth, Delay, Feed, Phase, Shape, Damp, Wet, Cross; right sidebar (left to right) sync-division chicken-head (1/1, 1/2, 1/4, 1/8, 1/8D, 1/4T, 1/16, 1/8T), BPM switch, InvFB switch, InvW switch; Vol knob; dBFS meter. **No Basic/Advanced toggle.**

**SHOT-314 — Flanger, BPM sync engaged**
- Reach: Flanger window > click the BPM switch on.
- Visible: The Rate knob greyed/locked (clicks swallowed, tooltip still reachable) and the sync-division chicken-head un-greyed.
- Why separate: The lockout is silent — a greyed knob with no message is exactly what the atlas must label.
- Variant of: SHOT-313

**SHOT-315 — Phaser, Basic**
- Reach: Effects > chevron > Modulation > Phaser > name plate.
- Visible: Dark modulation plate; knobs Rate, MinHz, MaxHz, Feed, Stereo, Wet, Gain; right sidebar Slow/Fast Range switch and a Stages chicken-head (1/2/4/6/8/12/16/24); Vol knob; dBFS meter.

**SHOT-316 — Phaser, Advanced**
- Reach: Phaser window > Menu > "Show Advanced Controls".
- Visible: Adds the Cross knob to the knob row and, in the right sidebar, the LFO-wave chicken-head (Sin/Tri/Saw/S&H), the sync-division chicken-head, the InvFB switch and the BPM switch.
- Variant of: SHOT-315

### Time

**SHOT-317 — Reverb, Basic**
- Reach: Effects > chevron > Time > Reverb > name plate.
- Visible: Dark Pultec-style plate with a small JEWEL indicator at the top right of the content area; row 1 Room, Decay, Diffuse, PreDly, Wet, Dry with the sync-division chicken-head and the channel-mode chicken-head (M = Mid / D = Side only) at the right; row 2 LoCut, HiCut, BassMlt, BassX, TailDep, TailRt, Stereo, HiDamp with the ER knob and the Sync switch at the right; Vol knob; dBFS meter.

**SHOT-318 — Reverb, Advanced**
- Reach: Reverb window > Menu > "Show Advanced Controls".
- Visible: Row 1 gains HFRatio and WetTone plus the ducking cluster Duck, DkThr, DkAtt, DkRel and a "Freeze" switch at the far right; the channel-mode chicken-head now also offers **S (Stereo)**; row 2 gains the Tail Shape chicken-head (S/T/R) and a HiDmp bypass switch.
- Why separate: Four ducking knobs appear and an existing selector changes its option count.
- Variant of: SHOT-317

**SHOT-319 — Delay, Basic**
- Reach: Effects > chevron > Time > Delay > name plate (Mode defaults to Echo).
- Visible: Dark Pultec plate; row 1 Time, Feed, Wet, Dry, Tone; row 2 FBDst, FBKnee, FBSym, Smooth; a 3x2 selector grid at the right edge — top row Model (M/S/P/O), SyncDiv, FB Filter (L/H/B/O); bottom row Pitch switch, BPM switch, Limit/Sat switch; Vol knob; dBFS meter.

**SHOT-320 — Delay, Advanced**
- Reach: Delay window > Menu > "Show Advanced Controls".
- Visible: Row 1 becomes Time, Feed, LoFiSR, WetIn, Wet, Dry, FBCut, FBReso, Tone plus Duck, DkThr, DkAtt, DkRel; row 2 becomes ModHz, ModTime, ModFB, Diff, DiffSprd, LoBit, FBDst, FBKnee, FBSym, Spread, Pan, Smooth; the selector grid is unchanged.
- Why separate: Basic is a deliberately short locked list; Advanced roughly TRIPLES the knob count.
- Variant of: SHOT-319

**SHOT-321 — Delay, BPM sync engaged**
- Reach: Delay window > click the BPM switch in the selector grid.
- Visible: The Time knob greyed/locked; the sync-division chicken-head un-greyed and active.
- Variant of: SHOT-319

**SHOT-322 — Delay, Feed knob warning ring**
- Reach: Delay window, turn Feed past its 100% mark with audio playing; capture while the ring is coloured.
- Visible: The Feed knob's ring lit as a LIVE METER of circulating feedback — green through orange to red across the top zone above 100%.
- Why separate: A knob that is also a meter; a static screenshot at rest shows nothing.
- Variant of: SHOT-319

**SHOT-323 — Vocal Doubler (Delay), Basic**
- Reach: Delay window > Menu > Mode > "Doubler".
- Visible: Dark Pultec plate; a single knob row Time L, Time R, Detune, Width, Rate, Mix; Vol knob; dBFS meter.
- Why separate: A different panel class from the Echo delay — no feedback, no lo-fi, no selector grid.
- Variant of: SHOT-319

**SHOT-324 — Vocal Doubler, Advanced**
- Reach: Delay window in Doubler mode > Menu > "Show Advanced Controls".
- Visible: Adds the ducking cluster Duck, DkThr, DkAtt, DkRel to the end of the knob row.
- Variant of: SHOT-323

### Pedal-native panels, rack faces

Every one of these is a full-size RACK face — right-clustered knobs with a
deliberately empty left half and a dBFS meter column. Their much smaller
pedalboard-tile faces are Sitting 15.

**SHOT-325 — Blues Drive (rack face)**
- Reach: Effects > chevron > Pedals > Harmonics > Blues Drive > name plate.
- Visible: Olive hammerite plate; three right-clustered knobs Drive, Tone, Level; dBFS meter at the right; empty left region.

**SHOT-326 — Distortion (rack face)**
- Reach: Effects > chevron > Pedals > Harmonics > Distortion > name plate.
- Visible: Olive plate; right-clustered knobs Dist, Tone, Level; dBFS meter.

**SHOT-327 — Fuzz (rack face)**
- Reach: Effects > chevron > Pedals > Harmonics > Fuzz > name plate.
- Visible: Olive plate; right-clustered Fuzz, Boost, Level with a Mode chicken-head to their left (Gt = Gated / Ge = Germanium / Oc = Octave); dBFS meter.

**SHOT-328 — High-Gain (rack face)**
- Reach: Effects > chevron > Pedals > Harmonics > High-Gain > name plate.
- Visible: Olive plate; six tightly right-clustered knobs Dist, Low, Mid Hz, Mid dB, High, Level; dBFS meter.

**SHOT-329 — Bass Driver (rack face)**
- Reach: Effects > chevron > Pedals > Harmonics > Bass Driver > name plate.
- Visible: Olive plate; right-clustered Drive, Blend, Low, High, Level; dBFS meter.

**SHOT-330 — Bass Overdrive (rack face)**
- Reach: Effects > chevron > Pedals > Harmonics > Bass Overdrive > name plate.
- Visible: Olive plate; right-clustered Gain, Balance, Low, High, Level; dBFS meter.

**SHOT-331 — Octave (rack face)**
- Reach: Effects > chevron > Pedals > Harmonics > Octave > name plate.
- Visible: Olive plate; right-clustered Direct, +1, -1, -2, Range with a Mode chicken-head to their left (Po = Polyphonic / Vi = Vintage); dBFS meter.

**SHOT-332 — Noise Gate (rack face)**
- Reach: Effects > chevron > Pedals > Dynamics > Noise Gate > name plate.
- Visible: Cream dynamics plate; right-clustered Threshold, Decay with TWO chicken-heads to their left — Mode (Re = Reduction / Mu = Mute) and Detector Source (DI / Sf = Self); dBFS meter.

**SHOT-333 — Bass Compressor (rack face)**
- Reach: Effects > chevron > Pedals > Dynamics > Bass Compressor > name plate.
- Visible: Cream dynamics plate; right-clustered Thresh, Ratio, Release, Level with a square GR meter to their left; dBFS meter.

**SHOT-334 — Polyphonic Synth (rack face)**
- Reach: Effects > chevron > Pedals > Modulation > Polyphonic Synth > name plate.
- Visible: Dark modulation plate; a FULL-WIDTH top row containing a Mono/Poly switch (left), an 11-position Type chicken-head centred (L1/L2/Pd/Bs/St/Or/Bl/F1/F2/Q1/Q2) and a Gtr/Bass switch (right); below it a row of six knobs Var, Tone, Rate, Depth, Effect, Direct; dBFS meter.

**SHOT-335 — Wah (rack face)**
- Reach: Effects > chevron > Pedals > Modulation > Wah > name plate.
- Visible: Dark modulation plate; a single "Pedal position" knob right-clustered with a Mode chicken-head to its left (Vi = Vintage / Ri = Rich); dBFS meter.

**SHOT-336 — Acoustic Preamp, factory body**
- Reach: Effects > chevron > Pedals > Time > Acoustic Preamp > name plate.
- Visible: Dark Pultec plate; right-clustered Resonance, Ambience, Notch (reads "OFF" at the bottom of its travel, otherwise "<n> Hz"), Level; to their left a column holding a Body chicken-head (Dr/Pa/Ju/Us) with a **"Load IR..." button beneath it dimmed to ~55%**; dBFS meter.

**SHOT-337 — Acoustic Preamp, User body**
- Reach: Acoustic Preamp panel > turn the Body chicken-head to "Us" (User).
- Visible: Same layout with the "Load IR..." button at full opacity and enabled; **Resonance now acts as the IR wet/dry mix.**
- Why separate: The button's enabled state is the only visual cue that a body type behaves completely differently.
- Variant of: SHOT-336

**SHOT-338 — Acoustic Simulator, factory mode**
- Reach: Effects > chevron > Pedals > Modulation > Acoustic Simulator > name plate.
- Visible: Dark modulation plate; right-clustered Top, Body, Reverb, Level; to their left a Mode chicken-head (St/Ju/En/Pi/Us) with a dimmed "Load IR..." button beneath; dBFS meter.

**SHOT-339 — Acoustic Simulator, User mode**
- Reach: Acoustic Simulator panel > turn the Mode chicken-head to "Us".
- Visible: Same layout with "Load IR..." enabled; **Body becomes the IR wet/dry mix.**
- Variant of: SHOT-338

**SHOT-340 — Load IR file chooser**
- Reach: Acoustic Preamp or Acoustic Simulator in User mode > click "Load IR...".
- Visible: Native open-file dialog titled "Pick acoustic IR" / "Pick acoustic simulator IR", filtered to *.wav, starting in the app's IR folder.

---

## SITTING 14 — Effect visual windows

**SETUP:** Audio must be PLAYING for almost all of these. Stage a busy source
(a full mix) so the traces are dense. Every visual is opened the same way:
effect window > Menu > "Visual".

**SHOT-341 — Effect window + Visual window tethered**
- Reach: Compressor effect window > Menu > Visual. Capture BOTH windows together without moving either.
- Visible: The visual window sitting directly UNDER the effect window, centred, matching the effect window's width. Dragging either half moves both.
- Why separate: The tether is a relationship between two windows — it cannot be shown in a shot of either one alone. The geometry (under, centred, width-matched) is the whole feature.

**SHOT-342 — Visual window Menu, tether lock toggle (both labels)**
- Reach: With a visual window open, click the hamburger Menu on the VISUAL window's title strip. Capture, click the item, capture again.
- Visible: A one-item popup reading either "Lock to Effect Window" or "Unlock from Effect Window" depending on current state. **The wording states the ACTION, not the state, and there is no tick.**
- Why separate: The only control for the tether behaviour, and its label flips.

**SHOT-343 — Limiter visual**
- Reach: Limiter effect window > Menu > Visual. Play loud audio so the limiter works.
- Visible: A framed strip with the caption "<Effect> - cyan dips = volume being pulled down; orange = ceiling"; a scrolling history newest-at-the-right — grey output level envelope, cyan gain-reduction trace, orange ceiling trace.

**SHOT-344 — Compressor visual**
- Reach: Compressor effect window > Menu > Visual. Play audio above the threshold.
- Visible: Caption "<Effect> - cyan from top = volume pulled down; orange = threshold". Left ~66% labelled "Input + reduction" — faint grid, grey input waveform about the centre line, cyan gain-reduction bars hanging from the TOP edge, two orange horizontal threshold lines. Right ~34% labelled "Knee" — grid, grey unity 45-degree reference line, cyan transfer curve, a vertical orange threshold marker, and an orange LIVE operating dot.

**SHOT-345 — Chorus visual**
- Reach: Chorus effect window > Menu > Visual, audio playing.
- Visible: Caption "<Effect> - grey outline = in, solid = out | LFO + response". Left ~62% labelled "Audio" (grey ghost outline = input, solid cyan body = output, faint grid). Right column split — top labelled "LFO" showing one cycle of the selected LFO wave in cyan with an orange dot riding the live phase; bottom labelled "Notches" showing the comb response curve.

**SHOT-346 — Flanger visual**
- Reach: Flanger effect window > Menu > Visual, audio playing.
- Visible: Same three-part layout; the LFO scope shows sine or triangle per the Shape knob and the notch curve tightens with Delay + Depth.
- Why separate: Same drawing code, but the caption carries the effect name and the notch density differs visibly.
- Variant of: SHOT-345

**SHOT-347 — Phaser visual**
- Reach: Phaser effect window > Menu > Visual, audio playing.
- Visible: Same three-part layout; the notch COUNT follows the Stages selector (two allpass stages per notch) and the LFO scope shows the picked wave including saw and sample-and-hold.
- Variant of: SHOT-345

**SHOT-348 — Delay visual**
- Reach: Delay effect window > Menu > Visual, audio playing with BPM sync on.
- Visible: Caption "<Effect> - solid = the echoes, grey = what you played | drive curve". Left ~68% labelled "Echoes vs beat" — grey ghost input outline, solid cyan wet/echo body, vertical beat-grid lines derived from host BPM. Right ~32% labelled "Drive" — grid plus the cyan feedback transfer curve drawn input-vertical / output-horizontal.

**SHOT-349 — Delay visual, BUILDING warning**
- Reach: With the Delay visual open, turn Feed above 1.0.
- Visible: Orange "BUILDING" text in the top right of the audio pane.
- Why separate: Conditional warning text that appears only in runaway feedback.
- Variant of: SHOT-348

**SHOT-350 — Reverb visual**
- Reach: Reverb effect window > Menu > Visual. Play a short stab and capture while the tail rings out.
- Visible: Caption "<Effect> - solid keeps going after the grey stops: that is the tail". Left ~62% labelled "Audio" (grey ghost input STOPS, solid cyan output CONTINUES). Right ~38% decay map — a faintly shaded pre-delay region, an orange-tinted early-reflection region with discrete orange spikes, a cyan exponential tail curve, and three region labels "pre", "early", "tail" along the bottom.

**SHOT-351 — Transient Shaper visual**
- Reach: Transient Shaper effect window > Menu > Visual, with drums playing.
- Visible: Caption "<Effect> - grey outline = in, solid = shaped out | one-hit preview". Left ~66% labelled "Audio". Right ~34% labelled "Preview" — one idealised drum hit drawn twice, grey ghost = input envelope, cyan solid = the shaped result.

**SHOT-352 — Saturation visual**
- Reach: Saturation effect window (Tube or Console) > Menu > Visual, audio playing.
- Visible: Caption "<Effect> - grey = in, solid = out | bright bars = harmonics being added NOW". Left ~45% labelled "Audio". Middle labelled "Harmonics" — eight bars numbered 1-8, faint GHOST bars showing full-drive potential behind BRIGHT orange bars showing what is being added at the live level (bar 1 grey). Right labelled "Curve" — grid plus the cyan static transfer curve.

**SHOT-353 — Tape visual**
- Reach: Saturation effect window with Mode = Tape > Menu > Visual, audio playing.
- Visible: Same three-pane layout; the transfer curve and harmonic bars are the TAPE shaper's, so the picture differs.
- Variant of: SHOT-352

**SHOT-354 — Visual window with no signal**
- Reach: Open any visual window with the transport stopped and no audio on the strip.
- Visible: An empty framed strip with the caption and the faint grid only; harmonic bars show ghost bars but no bright bars; the LFO dot still moves.
- Why separate: **The at-rest state of every visual** — a reader who opens one before pressing play needs to know this is normal, not broken.
- Variant of: SHOT-344

---

## SITTING 15 — Pedalboard (Inst tab) and the board-only panels

**SETUP:** Ribbon > "+" > BaySickLiveInst to create an Inst tab, then click the
tab. On a live-input Inst tab the Pedals window opens on tab click; otherwise use
the window Menu > "Pedals". Have a guitar or any live input plugged in for the
Tuner shots, and a .nam capture available for SHOT-375.

**SHOT-355 — Pedalboard window, standard 4x2 view**
- Reach: Inst tab > Menu > "Pedals".
- Visible: Contained window "<tab name> - Pedals"; title strip carrying the page Menu, the pedalboard preset button and a NAM/IR launcher; an 8-slot pedalboard laid out 4 across x 2 down, each slot either holding a pedal graphic or showing an empty slot. **Slot 0 is a fixed, fully-locked Tuner and slot 7 is a fixed EQ slot** — only the six middle tiles are free.

**SHOT-356 — Pedalboard window, compact (single pedal) view**
- Reach: Open the Pedals window, then switch it to Compact via its view-mode control (title strip "View" heading).
- Visible: The same window, resized, showing ONE pedal at a time with a slot dropdown for stepping between the eight slots.
- Why separate: A different layout of the same window.
- Variant of: SHOT-355

**SHOT-357 — Pedalboard preset menu**
- Reach: Pedals window > click the pedalboard preset button in its title strip.
- Visible: "Save Pedalboard As...", then the saved pedalboard presets (or a greyed "(no saved pedalboards)" when none exist), plus a reveal-folder entry.

**SHOT-358 — Save Pedalboard Preset prompt**
- Reach: Pedals window > preset button > "Save Pedalboard As...".
- Visible: AlertWindow "Save Pedalboard Preset", body "Pedalboard preset name:", one text field, Save and Cancel.

**SHOT-359 — Pedal-tile face vs rack face (meters stripped)**
- Reach: Load Blues Drive into one of the six middle tiles, and photograph it beside SHOT-325's rack face.
- Visible: The same Blues Drive knobs arranged in a compact GRID inside the small tile, with the input VU, the dBFS column and any GR meter **removed entirely.**
- Why separate: Every pedal-capable panel has two faces. A reader comparing the board to the rack sees different-looking versions of the same effect and needs this comparison once.
- Variant of: SHOT-325

**SHOT-360 — Limiter pedal face**
- Reach: Inst tab > Menu > Pedals > a middle tile > Limiter.
- Visible: Cream dynamics plate, TWO knobs only — Ceiling and Release — gridded into the tile; no meters.
- Why separate: Same DSP as the rack Limiter (up to fifteen controls) reduced to two knobs — the biggest rack/board divergence in the set.

**SHOT-361 — Saturation pedal face**
- Reach: Inst tab > Menu > Pedals > a middle tile > Saturation.
- Visible: Olive plate; Drive and Mix knobs plus a type chicken-head (Tu/Co/Ta) in the tile grid.
- Variant of: SHOT-360

**SHOT-362 — Chorus pedal face**
- Reach: Pedals > a middle tile > Chorus.
- Visible: Rate, Depth, Mix knobs in the tile grid.
- Variant of: SHOT-360

**SHOT-363 — Flanger pedal face**
- Reach: Pedals > a middle tile > Flanger.
- Visible: Rate, Depth, Feedback, Mix knobs in the tile grid.
- Variant of: SHOT-360

**SHOT-364 — Phaser pedal face**
- Reach: Pedals > a middle tile > Phaser.
- Visible: Rate, Depth, Feedback, Mix knobs in the tile grid.
- Variant of: SHOT-360

**SHOT-365 — Delay pedal face**
- Reach: Pedals > a middle tile > Delay.
- Visible: Time, Feedback, Mix knobs plus a "Sync" toggle button in the tile grid.
- Variant of: SHOT-360

**SHOT-366 — Reverb pedal face**
- Reach: Pedals > a middle tile > Reverb.
- Visible: Decay, Damp, Mix knobs plus an algorithm chicken-head (Pl/Hl/Ch/Rm/VB) — **this face keeps its dBFS meter column**, unlike the other six.
- Variant of: SHOT-360

**SHOT-367 — Tuner pedal, LED Bar, in tune**
- Reach: Inst tab > Menu > Pedals. The Tuner is the fixed FIRST tile (slot 0) and cannot be changed. Play a note held close to pitch.
- Visible: Black display surface; note name in large GREEN text at the left; cents readout (+/- nn c) at the right; frequency in Hz beneath; a 21-cell LED bar with the CENTRE cell lit green. Right cluster: Trim knob, then Mode (Ch/Gt/Bs), Display (St/LB) and Flat (0-6) chicken-heads across, then Mute and 432 buttons.

**SHOT-368 — Tuner pedal, LED Bar, off pitch**
- Reach: Tuner pedal, play a note that is clearly sharp or flat.
- Visible: Note name and cents readout in cream/orange rather than green; a lit cell offset from the centre of the LED bar.
- Why separate: The colour change is the whole point of the display.
- Variant of: SHOT-367

**SHOT-369 — Tuner pedal, Strobe display**
- Reach: Tuner pedal > turn the Display chicken-head to "St" (Strobe), play a note.
- Visible: Twelve scrolling vertical stripes filling the display area (green when in tune, amber when not), with the same note/cents/Hz text row above.
- Why separate: A completely different display mode drawn in the same box.
- Variant of: SHOT-367

**SHOT-370 — Tuner pedal, no signal**
- Reach: Tuner pedal with nothing playing.
- Visible: A dim "--" in place of the note name; strobe stripes or LED cells drawn at low alpha.
- Variant of: SHOT-367

**SHOT-371 — Graphic EQ pedal**
- Reach: Inst tab > Menu > Pedals > click the EQ tile (slot 7, the last one) > "Graphic EQ".
- Visible: Dark Pultec plate; eight vertical fader columns — seven frequency bands (100, 200, 400, 800, 1.6k, 3.2k, 6.4k style labels) plus an "Lvl" master fader; a horizontal 0 dB centre-detent line drawn across every column; rectangular light-grey fader caps with a centre stripe; a 14px frequency-label row under the faders.
- Why separate: **Board-only** — it cannot be loaded into a rack slot, so it has to be photographed here.

**SHOT-372 — Bass Graphic EQ pedal**
- Reach: Pedals > EQ tile (slot 7) > "Bass Graphic EQ".
- Visible: Identical fader-bank layout with bass-tuned frequency labels; seven band faders plus "Lvl".
- Variant of: SHOT-371

**SHOT-373 — Pro Parametric EQ pedal, board grid**
- Reach: Pedals > EQ tile (slot 7) > "Pro Parametric EQ".
- Visible: Dark Pultec plate laid out as a 3x3 GRID — one row per band with the band name (Low / Mid / High) on the left and Freq, Boost, Q knobs across; a right column holding the Input volume knob with a small round overload LED and an "OL" micro-label at its top right, an "EQ Bypass" toggle button, and a Hi/Lo gain-range button reading "Lo" or "Hi +20".
- Why separate: Board-only, and the tile layout is a grid rather than the single row the rack layout uses. (The OL LED lit red is SHOT-374's companion — see below.)

**SHOT-374 — Pro Parametric EQ, overload LED lit**
- Reach: Pro Parametric EQ pedal, raise Input volume past ~+12 dB with signal playing until the LED turns red.
- Visible: The OL LED bright red instead of dark maroon.
- Why separate: A two-state indicator whose only label is two letters.
- Variant of: SHOT-373

**SHOT-375 — NAM Pedal, no file loaded**
- Reach: Inst tab > Menu > Pedals > click any of the six middle tiles > pick the User NAM Pedal entry.
- Visible: Olive hammerite plate; a top strip with a "Load .nam" button and a filename label reading "(no file loaded)"; six right-clustered knobs Input/Drive, Low, Mid, High, Blend, Output; dBFS meter.

**SHOT-376 — NAM Pedal, capture loaded + file chooser**
- Reach: NAM Pedal panel > "Load .nam" — capture the native dialog, then capture the loaded panel.
- Visible: Native open-file dialog "Load NAM Pedal", filtered to *.nam, starting in `Presets/Effects/Pedals/User NAM Pedals`; then the same panel with the filename label showing the loaded model name.
- Why separate: The label is the only indication the pedal is doing anything.
- Variant of: SHOT-375

---

## SITTING 16 — Piano Roll

**SETUP:** A project with several engine tabs carrying notes in the same pattern
(for ghost notes), a BaySickRustyDrums tab (for the labelled keyboard), a
BaySickPlayer engine whose SFZ declares keyswitches, and a BaySickGuitars or
BaySickBasses tab (for the engine-aware Note Properties).

**SHOT-377 — Piano Roll window, full view, engine roll**
- Reach: Ribbon > "Piano Roll", then click the engine pill on the window title strip and pick a Layer or Bass tab.
- Visible: Title strip with "Menu", the engine pill (current engine name + a down triangle), and the "Player Page" and "FX Rack" nav buttons; a 20px menu bar (Edit / Tools / Scale / Chords / View); a 28px toolbar; the 64px piano keyboard column on the left; the note grid with its 14px ruler; a vertical scrollbar on the right; a horizontal scrollbar under the grid; the control lane at the bottom with its header.

**SHOT-378 — Piano Roll toolbar, close-up**
- Reach: Photograph the 28px toolbar row.
- Visible: Snap magnet button (lit/dim); tool buttons Draw / Paint / Del / Mute / Slice / Select; the armed note-type button ("Flat" / "RP Slide" / "RT Slide" / "Porta"); the Zoom tool button; Undo; Redo; "H" history; "-" and "+" zoom; and a right-aligned context label reading "<tab name> - <engine type>".
- Variant of: SHOT-377

**SHOT-379 — Piano Roll, snap dropdown open**
- Reach: Click the Snap magnet button.
- Visible: The 11-value snap-division ladder with a tick on the current division. **This is the app-wide Piano Roll snap, shared by every roll.** (Same labels as SHOT-031.)

**SHOT-380 — Piano Roll, armed note-type button cycled (4 labels)**
- Reach: Click the note-type button (or press S) repeatedly; capture each label.
- Visible: The button showing Flat, then RP Slide, then RT Slide, then Porta, with its toggled-on highlight whenever the type is not Flat.
- Why separate: One button, four labels, and it determines what every new note becomes.
- Variant of: SHOT-378

**SHOT-381 — Piano Roll, engine pill dropdown open**
- Reach: Click the engine pill on the window title strip.
- Visible: **"Drum Kit" at the top** with a tick when active, a separator, then every registered engine roll in ribbon order (Layer / Bass / Drum / Clip / Inst / Guitars / Basses / Rusty / hosted plugin tabs) with the active one ticked.
- Why separate: Picking "Drum Kit" replaces the entire piano-roll surface with a different one (Sitting 17) that shares this tab and title strip.

**SHOT-382 — Piano Roll menu bar, Edit menu**
- Reach: Click "Edit" in the 20px menu bar.
- Visible: Select All (Ctrl+A) / Deselect / sep / Copy (Ctrl+C) / Paste (Ctrl+V) / Delete / Duplicate (Ctrl+B).

**SHOT-383 — Piano Roll menu bar, Tools menu**
- Reach: Click "Tools".
- Visible: Quantize (Alt+Q) / Strum (Alt+S) / Arpeggiate (Alt+A) / "Chop..." submenu / Glue (Ctrl+G) / Articulate (Alt+L) / Randomize (Alt+R) / Humanize... / Riff Machine... (Alt+E) / Generate Chords (Alt+P) / sep / "Quantize Settings" submenu / sep / Transpose Up (Shift+Up), Transpose Down, Transpose Up Octave (Ctrl+Up), Transpose Down Octave.

**SHOT-384 — Piano Roll, Chop submenu**
- Reach: Tools > hover "Chop...".
- Visible: Into 2 (halves) / Into 3 (thirds) / Into 4 (quarters) / Into 6 / Into 8.

**SHOT-385 — Piano Roll, Quantize Settings submenu**
- Reach: Tools > hover "Quantize Settings".
- Visible: 1/4 / 1/8 / 1/16 / 1/32 with a tick on the current resolution.
- Why separate: **This sets the resolution; it does not quantize** — easily confused with the Quantize action above it in the same menu.

**SHOT-386 — Piano Roll menu bar, Scale menu**
- Reach: Click "Scale".
- Visible: "Snap to Scale" (tickable) / sep / "Root" submenu (C through B, tick on current) / "Scale" submenu (every scale definition, tick on current).

**SHOT-387 — Piano Roll menu bar, Chords menu**
- Reach: Click "Chords".
- Visible: The full chord list with a tick on the armed chord. **Picking one ARMS the Stamp tool** (see SHOT-414).

**SHOT-388 — Piano Roll menu bar, View menu**
- Reach: Click "View".
- Visible: Zoom In / Zoom Out / Zoom In Vertical / Zoom Out Vertical / sep / Scroll to Playhead / sep / Ghost Notes (tickable) / Velocity Lane (tickable).

**SHOT-389 — Piano Roll grid, populated with notes**
- Reach: Pick an engine, draw several notes at different velocities, mute one, group two.
- Visible: Bar and beat grid lines spaced by the pattern's time signature; notes in the engine's note colour; selected notes in their selected state; the muted note's muted rendering; the playhead line; the ruler strip at the top of the grid.

**SHOT-390 — Piano Roll grid, ghost notes visible**
- Reach: View > Ghost Notes (ticked) with at least two engines carrying notes in the same pattern.
- Visible: The other engines' notes drawn behind the current roll's notes in their own per-source colours.
- Why separate: Ghosts are dim and easily mistaken for a bug when a reader does not know they can be turned off.
- Variant of: SHOT-389

**SHOT-391 — Piano Roll keyboard, standard piano mode**
- Reach: Pick a Layer or Bass engine, photograph the 64px keyboard column.
- Visible: Black and white keys with C-octave names; the lit key under the mouse (preview); lit keys for any hardware-MIDI notes currently held.

**SHOT-392 — Piano Roll keyboard, labelled all-white mode (Rusty Drums)**
- Reach: Engine pill > pick the BaySickRustyDrums roll.
- Visible: EVERY key painted white with an engine-supplied label per note ("Snare Center", "Hi-hat Tight Closed", ...) instead of piano key graphics.
- Why separate: Same column, entirely different rendering — a reader will not guess these are the same widget.
- Variant of: SHOT-391

**SHOT-393 — Piano Roll keyboard, keyswitch keys highlighted**
- Reach: Pick a BaySickPlayer engine whose loaded SFZ declares keyswitches; look at the keyswitch range.
- Visible: Keyswitch keys drawn with an amber highlight and their label ("C6 Sustain", "C#6 Staccato") — visually distinct from playable keys.
- Why separate: Amber keys look broken unless the reader is told they are keyswitches.
- Variant of: SHOT-391

**SHOT-394 — Piano Roll keyboard hidden**
- Reach: Click into the grid and press M.
- Visible: The keyboard column collapsed to zero width; grid, scrollbars and control lane expanded across the freed space.
- Why separate: A whole column disappears on one keystroke; readers hit this by accident.
- Variant of: SHOT-377

**SHOT-395 — Control lane, Velocity mode**
- Reach: With the lane visible (View > Velocity Lane ticked), photograph the lane at the bottom.
- Visible: The 16px lane header strip (which doubles as the resize handle) and one vertical stem + dot per note, with selected notes' dots drawn RED.

**SHOT-396 — Control lane, mode dropdown open**
- Reach: CLICK (do not drag) the control lane's header strip.
- Visible: A four-item menu: Velocity / Panning / Pitch Bend / Filter Cutoff.
- Why separate: The header is both a click target (this menu) and a drag target (SHOT-400) — the two outcomes need separate pictures.

**SHOT-397 — Control lane, Panning mode**
- Reach: Lane header > Panning.
- Visible: The same stems and dots plotting each note's pan value around the lane's CENTRE line.
- Variant of: SHOT-395

**SHOT-398 — Control lane, Pitch Bend mode**
- Reach: Lane header > Pitch Bend.
- Visible: Per-note fine-pitch dots plotted around the lane centre.
- Variant of: SHOT-395

**SHOT-399 — Control lane, Filter Cutoff mode**
- Reach: Lane header > Filter Cutoff.
- Visible: Per-note cutoff-offset dots (0..1) plotted from the lane FLOOR, not the centre.
- Variant of: SHOT-395

**SHOT-400 — Control lane, collapsed / mid-resize**
- Reach: Press and DRAG the control lane's header strip vertically down until the lane is at its minimum.
- Visible: The lane at header-only height with the grid grown to fill the space.
- Why separate: The state a reader reaches by accidentally dragging instead of clicking the header.
- Variant of: SHOT-395

**SHOT-401 — Note Properties popup, standard engine**
- Reach: Pick a Layer/Bass engine, double-click an existing note.
- Visible: A CallOutBox with the note-type buttons and labelled sliders: Velocity, Release, Fine Pitch, Panning, Filter Cutoff, Resonance, plus the Porta length box.

**SHOT-402 — Note Properties popup, Guitars/Basses (engine-aware)**
- Reach: Engine pill > pick a BaySickGuitars or BaySickBasses roll > double-click a note.
- Visible: A CallOutBox with Velocity plus three type buttons "Flat" / "RP Slide" / "Bend", a "Bend" combo gated to the patch's real bend range, and a "Shape" combo (Ramp + Hold / Ramp (whole) / Up + Back / Instant). **The five in-house-only sliders are absent.**
- Why separate: Same gesture, completely different panel depending on the engine — a reader comparing two rolls will think one is broken.
- Variant of: SHOT-401

**SHOT-403 — Humanize dialog**
- Reach: Select notes > Tools > Humanize...
- Visible: A CallOutBox panel with Start Time / Duration / Velocity rows, each carrying a range control, plus its apply/dismiss affordances.

**SHOT-404 — Randomize dialog**
- Reach: Select notes > Tools > Randomize (Alt+R).
- Visible: The Randomize panel with its per-property controls.

**SHOT-405 — Riff Machine, step tab row and page 1 (Prog)**
- Reach: Tools > Riff Machine... (Alt+E).
- Visible: Title "Riff Machine"; the 8 step tabs "1 Prog", "2 Chords", "3 Arp", "4 Mirror", "5 Levels", "6 Artic", "7 Groove", "8 Fit"; a "Step enabled" checkbox; page 1 controls ("Progression" combo, "Chord rate" combo); and the global row (Preview to step, Work on existing score, Length, Start Over, Dice, Accept).

**SHOT-406 — Riff Machine, page 2 (Chords)**
- Reach: Riff Machine > click "2 Chords".
- Visible: A "Chord" combo plus the persistent tab row and global row.
- Why separate: Eight different control sets behind one tab row — one picture per page or the pages cannot be documented.
- Variant of: SHOT-405

**SHOT-407 — Riff Machine, page 3 (Arp)**
- Reach: Riff Machine > click "3 Arp".
- Visible: Pattern / Mode / Sync combos plus a "Gate" knob.
- Variant of: SHOT-405

**SHOT-408 — Riff Machine, page 4 (Mirror)**
- Reach: Riff Machine > click "4 Mirror".
- Visible: A "Flip chance" knob.
- Variant of: SHOT-405

**SHOT-409 — Riff Machine, page 5 (Levels)**
- Reach: Riff Machine > click "5 Levels".
- Visible: That page's level controls.
- Variant of: SHOT-405

**SHOT-410 — Riff Machine, page 6 (Artic)**
- Reach: Riff Machine > click "6 Artic".
- Visible: That page's articulation controls.
- Variant of: SHOT-405

**SHOT-411 — Riff Machine, page 7 (Groove)**
- Reach: Riff Machine > click "7 Groove".
- Visible: That page's groove controls.
- Variant of: SHOT-405

**SHOT-412 — Riff Machine, page 8 (Fit)**
- Reach: Riff Machine > click "8 Fit".
- Visible: That page's fit controls.
- Variant of: SHOT-405

**SHOT-413 — Scale Levels dialog (Alt+X)**
- Reach: Select notes > press Alt+X.
- Visible: A "Scale Levels" box scaling the selection's velocity by a percentage, with OK/Cancel.

**SHOT-414 — Piano Roll, Stamp tool armed (chord ghost preview)**
- Reach: Chords menu > pick a chord (this ARMS the Stamp tool) > hover the grid.
- Visible: A ghost preview of the notes the stamp will place at the hovered pitch, obeying the current Root/Scale when Snap to Scale is on.
- Why separate: **The Stamp tool has no toolbar button** — it is only reachable via the Chords menu, so a reader looking at the toolbar cannot find it at all.

**SHOT-415 — Piano Roll, marquee selection and time selection**
- Reach: Select(E) tool > drag a rectangle over notes; separately Ctrl+drag along the grid's ruler strip.
- Visible: The marquee rectangle over selected notes, and the time-selection band drawn in the ruler with its start/end edges.

**SHOT-416 — Piano Roll, slice line drag**
- Reach: Slice(C) tool > press and drag across notes without releasing.
- Visible: The cut line preview crossing the notes it will split.

---

## SITTING 17 — Drum Kit grid, drum menus and drum pages

**IMPORTANT ROUTE:** The Drum Kit surface is **not** on the Drums page. Reach it
via Ribbon > "Piano Roll" > the engine pill > "Drum Kit". The two banks
(1-16 and 17-32) are **two INDEPENDENT KITS**, not a filtered view of one
32-slot kit — each has its own drums bus, its own add allocation and its own kit
file, and Save Kit / Load Kit / Lock all act on whichever bank is on screen.
That deserves a sidebar in Manual 1; the UI expresses it with two small toggle
buttons and one changing button label.

**SETUP:** Several Drums tabs with sounds loaded in bank 1, at least one locked
drum, at least one drum whose play note differs from a hit already placed on it,
one saved kit, and a Rusty tab present (SHOT-443's wording changes when one exists).

**SHOT-417 — Drum Kit grid, kit 1-16, populated**
- Reach: Ribbon > Piano Roll > engine pill > "Drum Kit".
- Visible: A 20px MenuBarComponent (Edit / Tools / View); a 28px toolbar — Snap button (38px, highlighted when a division is active), seven tool buttons (Draw / Paint / Del / Mute / Slice / Sel / Zoom, elastic, active one toggled), Undo, Redo, "H", "-" and "+" zoom, a right-aligned context label, and pinned far right the bank switch "1-16" / "17-32" plus the "Kit  v" button. Below: the 202px sidebar (14px drag-handle column of three stacked bars, 120px picker button per row, 20px red M, 20px yellow S, 28px white audition key) with a ruler-row "Lock/Unlock 1-16" button spanning the picker+M+S width; and the grid — 14px ruler with bar numbers, alternating row stripes for the 16 rows, snap-ladder and bar gridlines, velocity-shaded rounded note blocks in each drum's accent colour. Horizontal scrollbar, then the control lane. A 12px vertical scrollbar down the right-hand edge of the grid, present ONLY when the window is too short to show all 16 rows at their 18px minimum row height (it takes its column off the grid's width when it appears); the sidebar rows scroll with it while the ruler band and its Lock/Unlock button stay pinned.

**SHOT-418 — Drum Kit grid, empty kit (no drums)**
- Reach: Fresh project, or delete every Drums tab. Ribbon > Piano Roll > pill > "Drum Kit".
- Visible: All 16 sidebar rows show the picker text "Pick a sound  v"; both M and S greyed out and disabled; no drag-handle bars drawn beyond the drum count; no active-row accent border; an empty note grid.
- Why separate: The state a brand-new user sees, and where the add-a-drum flow starts.
- Variant of: SHOT-417

**SHOT-419 — Drum Kit grid, kit 2 (17-32) selected**
- Reach: On the Drum Kit view, click "17-32" at the right end of the toolbar.
- Visible: "17-32" toggled on and "1-16" off; the sidebar's ruler-row button relabels to "Lock/Unlock 17-32" (tooltip changes to match); all 16 rows now show the SECOND kit's drums (or all-empty pickers). Layout otherwise identical.
- Why separate: Two independent kits whose only on-screen difference is two small toggles and one button label — exactly the ambiguity an atlas must resolve.
- Variant of: SHOT-417

**SHOT-420 — Drum Kit sidebar, locked drum row**
- Reach: Lock a drum (per-drum context menu > "Lock Drum", or the sidebar Lock/Unlock button > Proceed), then look at the sidebar.
- Visible: The picker button text gains an "[L] " prefix before the sound name.
- Why separate: **Clicking a locked row opens the per-drum context menu instead of the sound picker**, and "Delete Drum" in that menu is greyed. A three-character prefix changes what clicking the pad does.
- Variant of: SHOT-417

**SHOT-421 — Drum Kit sidebar, active drum row highlight**
- Reach: Select a Drums tab in the ribbon, then return to the Drum Kit view.
- Visible: A 2px border in that drum's accent colour drawn around its PICKER BUTTON only, not the whole row.
- Why separate: Easy to miss; it is the only indicator of which drum tab is currently active.
- Variant of: SHOT-417

**SHOT-422 — Drum Kit sidebar, drag-reorder in progress**
- Reach: Press and hold on a row's drag handle (the leftmost 14px column of three stacked bars) and drag up or down without releasing.
- Visible: A solid white 2px horizontal drop-indicator line across the full sidebar width at the target row.
- Why separate: Transient feedback that also explains what the handle column is for.

**SHOT-423 — Drum Kit sidebar, audition key pressed**
- Reach: Press and hold on the white key rectangle at the right edge of any populated sidebar row.
- Visible: The key rectangle darkens from white to 0xffcccccc while held; the drum sounds for as long as the button is down.
- Why separate: The white rectangles read as decoration until shown pressed.

**SHOT-424 — Drum Kit grid, notes with selection and retune dots**
- Reach: Draw some hits, marquee-select a few with the Sel tool, and set one drum's play note differently from a hit already placed on it (per-drum menu > MIDI Note).
- Visible: FOUR block appearances at once — gradient fill in the drum's colour with lightness scaled by velocity; selected notes with a double white rounded outline; muted notes desaturated and semi-transparent; and any hit whose MIDI note differs from its drum's assigned play note carrying a small white black-ringed dot at its top-right.
- Why separate: Four distinct block appearances on one canvas, all of which a reader will otherwise misread.

**SHOT-425 — Drum Kit grid, time selection in the ruler**
- Reach: Drag horizontally inside the 14px ruler band above the grid.
- Visible: A translucent highlight band spanning the full grid height, a stronger highlight inside the ruler, and bright vertical edge lines at both ends inside the ruler.

**SHOT-426 — Drum Kit grid, marquee selection drag**
- Reach: Pick "Sel", drag a rectangle across empty grid space.
- Visible: A blue 15%-alpha filled rectangle with a solid blue 1px border.

**SHOT-427 — Drum Kit grid, slice line**
- Reach: Pick "Slice", drag across some notes.
- Visible: A 2px highlight-coloured line from drag start to current point with a filled 6px dot at each end.
- Why separate: Tool-specific drag overlay, visually unlike the marquee.
- Variant of: SHOT-426

**SHOT-428 — Drum Kit grid, zoom rectangle**
- Reach: Pick "Zoom", drag a region.
- Visible: A yellow 10%-alpha filled rectangle with a yellow 70%-alpha 1px border.
- Why separate: Third distinct drag overlay colour.
- Variant of: SHOT-426

**SHOT-429 — Drum Kit control lane, Velocity mode**
- Reach: Visible by default under the grid (View > "Velocity Lane" toggles it).
- Visible: A 16px header bar reading "Control > Velocity" with a small down-triangle; beat/bar grid lines; one GREEN stem+node per drum hit whose height encodes velocity; **selected hits' nodes paint RED**; muted hits desaturate.

**SHOT-430 — Drum Kit control lane, Panning mode**
- Reach: Click the lane header (a click, not a drag) and choose "Panning".
- Visible: Header reads "Control > Panning"; a horizontal centre line across the lane; nodes become bipolar (above/below centre) and paint BLUE instead of green.
- Why separate: Bipolar layout, a centre line and a different node colour — genuinely a different picture.
- Variant of: SHOT-429

**SHOT-431 — Drum Kit control lane, mode dropdown open**
- Reach: Click (do not drag) the lane's 16px header bar.
- Visible: A two-item popup: "Velocity" and "Panning". (The melodic roll's lane has four modes — see SHOT-396.)
- Why separate: The down-triangle is the only hint the menu exists, and the same header is also the drag-resize handle.

**SHOT-432 — Drum Kit menu bar, Edit menu**
- Reach: Drum Kit view > click "Edit" in the 20px menu bar.
- Visible: Select All (Ctrl+A) / Deselect / sep / Copy (Ctrl+C) / Paste (Ctrl+V) / Delete / Duplicate (Ctrl+B).

**SHOT-433 — Drum Kit menu bar, Tools menu**
- Reach: Drum Kit view > click "Tools".
- Visible: Quantize (Alt+Q) / Strum (Alt+S) / "Chop..." submenu (Into 2 halves / 3 thirds / 4 quarters / 6 / 8) / Glue (Ctrl+G) / Articulate (Alt+L) / Randomize (Alt+R) / sep / "Quantize Settings" submenu (1/4, 1/8, 1/16, 1/32, current ticked).
- Why separate: Two submenus plus a deliberate ABSENCE relative to the melodic roll — no Arpeggiate, no Generate Chords, no Transpose. Drums do not get them.

**SHOT-434 — Drum Kit menu bar, View menu**
- Reach: Drum Kit view > click "View".
- Visible: Zoom In / Zoom Out / sep / Scroll to Playhead / sep / "Velocity Lane" with a tick reflecting current visibility.

**SHOT-435 — Drum Kit toolbar, Snap dropdown open**
- Reach: Drum Kit view > click the "Snap" button.
- Visible: The shared snap-division ladder with the active one ticked; the Snap button itself is highlighted whenever the division is not Off.
- Note: LEFT-click opens this menu here — right-click is deliberately swallowed.

**SHOT-436 — Drum hit context menu (double-click)**
- Reach: Drum Kit view > DOUBLE-CLICK an existing note block in the grid.
- Visible: A popup anchored to the note: "Velocity...", "MIDI Note...", sep, "Delete".
- Why separate: Double-click (not right-click) is the trigger, which is non-obvious.

**SHOT-437 — Velocity entry dialog**
- Reach: Double-click a drum hit > "Velocity...".
- Visible: AlertWindow "Velocity", prompt "Enter velocity (0..127):", text editor pre-filled with the current value, OK / Cancel.

**SHOT-438 — MIDI Note entry dialog**
- Reach: Double-click a drum hit > "MIDI Note...".
- Visible: AlertWindow "MIDI Note" whose message states the accepted formats ("Enter note name or MIDI number (e.g. C5 / c5 / C#5 / Db5 / 60).") and shows "Current: <name> = <number>"; text editor pre-filled with the note name; OK / Cancel.
- Why separate: The message carries the note-naming convention (**C5 = 60**) the whole app uses — real teaching content.

**SHOT-439 — Invalid note alert**
- Reach: In the MIDI Note dialog, type something unparseable (e.g. "xyz") and press OK.
- Visible: Warning box "Invalid Note" — "Couldn't parse that as a note name or MIDI number. Examples: C5, c5, C#5, Db5, 60."
- Why separate: Deliberately summonable and its examples are useful reference content.

**SHOT-440 — Scale Levels dialog (drum)**
- Reach: Drum Kit view > select one or more hits > press Alt+X.
- Visible: AlertWindow "Scale Levels" with the message "Scale velocities for the selection (100 % = no change).", a CUSTOM percentage slider component, OK / Cancel.
- Why separate: The only dialog in the drum grid built from a custom component rather than a text field.

**SHOT-441 — Kit menu (Save / Load Kit)**
- Reach: Drum Kit view > click the "Kit  v" button at the far right of the toolbar.
- Visible: "Save Kit As...", sep, "My Kits" submenu, "Factory Kits" submenu. Both mirror the folder tree — subfolders become cascading submenus (Factory > TR-808 > TR-808 Basic) and .xml files become items. Empty folders show a disabled "(no user kits)" / "(no factory kits)".
- Why separate: The whole kit save/load flow lives behind one 46px button; both the populated tree and the empty placeholders need showing.

**SHOT-442 — Save Kit As dialog**
- Reach: Drum Kit view > "Kit  v" > "Save Kit As...".
- Visible: AlertWindow "Save Kit As" whose message states which BANK is being saved — "This saves the drums in 1-16 - up to 16 of them. The other kit is not included." — text editor pre-filled "My Kit", Save / Cancel.
- Why separate: The message is bank-specific and reads "17-32" on the other bank; it is where the two-kit model is explained to the user.

**SHOT-443 — Replace Drums confirm dialog**
- Reach: Drum Kit view > "Kit  v" > pick a kit from My Kits or Factory Kits, with at least one drum already loaded in the current bank.
- Visible: Question AlertWindow "Replace Drums 1-16?" with "This will replace the drums in 1-16 with the new kit's drums, do you wish to proceed?" plus "(Kit 17-32 is not affected.)" and, when a Rusty tab exists, "(BaySickRustyDrums is not affected by kit loads.)". A **"Don't show again" CHECKBOX inside the message body**; Proceed / Cancel.
- Why separate: One of only two dialogs in the app with an embedded suppress checkbox, and its wording changes depending on whether a Rusty tab exists. **Capture it BEFORE ticking the box.**

**SHOT-444 — "Nothing to save" alert**
- Reach: Drum Kit view on an EMPTY bank > "Kit  v" > "Save Kit As..." > type a name > Save.
- Visible: Warning box "Nothing to save" — "Kit 1-16 has no drums in it, so there is nothing to save."
- Why separate: Deliberately reachable and it explains a save that silently produces no file.

**SHOT-445 — Lock or Unlock Kit confirm dialog**
- Reach: Drum Kit view > click the "Lock/Unlock 1-16" button in the sidebar's ruler row.
- Visible: Question AlertWindow "Lock or Unlock Kit 1-16?" — "This will lock or unlock every drum in kit 1-16. The other kit is not affected. Do you wish to proceed?" with a "Don't show again" checkbox and Proceed / Cancel.
- Why separate: The second suppress-checkbox dialog, and the only place the per-bank lock scope is stated. **Capture before ticking.**
- Variant of: SHOT-443

**SHOT-446 — "Drum Kit Full" alert**
- Reach: Fill all 16 slots of one bank, then try to add another drum to it via the ribbon Drums add path with that bank active.
- Visible: Info box "Drum Kit Full" — "Kit 1-16 already has all 16 of its drums. Delete one first, or switch to kit 17-32."
- Why separate: A capacity message that also teaches the two-bank escape hatch.

**SHOT-447 — Per-pad sound picker menu (top level)**
- Reach: Drum Kit view > click any UNLOCKED row's picker button (populated, or an empty "Pick a sound  v" row — the empty row first spawns a new drum tab, then opens this menu).
- Visible: Two submenus — "Sample" and "Synth Patch" — plus, when a sound is already loaded, a separator and "None (clear)".
- Why separate: The main sound-loading route in the whole drum area; its top level is only three lines and needs its own figure before the submenus.

**SHOT-448 — Sound picker, Sample submenu expanded**
- Reach: Pad picker > hover "Sample".
- Visible: "Browse sample folder...", "Load SFZ file...", sep, a "Core Library" submenu cascading per drum pack (Hip Hop Drums Package, EDM Drums Package, Percussion Package, each expanding to its instruments), sep, per-folder submenus of the factory BaySickPlayer drum presets.
- Why separate: A deep cascading tree; this is where the shipped sample content actually lives.

**SHOT-449 — Sound picker, Synth Patch submenu expanded**
- Reach: Pad picker > hover "Synth Patch".
- Visible: "+ New Patch (Blank)", sep, the installed synth drum presets (or a disabled "(no presets installed)"), sep, "Save Current Patch As..." (enabled only when the drum currently holds a BaySickSynth or BaySickPlayer patch).
- Why separate: Different content and different enable rules from the Sample submenu, plus its own empty-state placeholder.
- Variant of: SHOT-448

**SHOT-450 — Per-drum context menu (from a kit pad)**
- Reach: Drum Kit view > click the picker button of a LOCKED drum (locked pads open this instead of the sound picker).
- Visible: "Lock Drum" (ticked) / "Polyphony: Polyphonic" or "Monophonic" or "(n/a)" / sep / "Rename..." / "Duplicate Drum (new tab)" / sep / "Choke Group" submenu / "MIDI Note" submenu / "MIDI Learn" (or "MIDI Learn: <binding>") and "MIDI Forget: <binding>" when bound / sep / "Save Current Patch As..." / sep / "Delete Drum" (greyed while locked).
- Why separate: This KIT-ROUTE menu is a different shape from the same page's Menu-dropdown route (SHOT-455) — it has the MIDI Note / MIDI Learn items and no page-preset items.

**SHOT-451 — Per-drum menu, Choke Group submenu**
- Reach: Per-drum context menu > hover "Choke Group".
- Visible: "None" plus "Group 1" .. "Group 16", the current assignment ticked.
- Why separate: A 17-entry submenu implementing a cross-drum cut bus that has no other UI anywhere.

**SHOT-452 — Per-drum menu, MIDI Note submenu**
- Reach: Per-drum context menu from a kit pad > hover "MIDI Note".
- Visible: A disabled header row "Assigned: <note name>", sep, then one submenu per octave ("C0 - B0", "C1 - B1", ...) each listing its 12 notes as "<name>  (<number>)" with the current one ticked. Default for every drum is C5 (60).
- Why separate: Two-level submenu with a status header, and it sets the drum's **PLAY pitch, not an input filter** — the single most misunderstood control in the drum area.

**SHOT-453 — MIDI Learn waiting dialog**
- Reach: Per-drum context menu from a kit pad > "MIDI Learn".
- Visible: AlertWindow "MIDI Learn" — "Hit a pad or key to assign it to this drum. Waiting 30 seconds..." with a single Cancel button. Closes on capture or after 30 seconds.
- Why separate: A modal wait state with a live timeout; the manual has to say what to do while it is up.

**SHOT-454 — MIDI Learn follow-up play-note prompt**
- Reach: Complete a MIDI Learn capture with a NOTE (not a CC) whose number differs from the drum's current play note.
- Visible: Question box "MIDI Learn" — "Also set this drum's play note to <note name>?" with Yes / No.
- Why separate: Appears only in that one branch and links the trigger binding to the play pitch.
- Variant of: SHOT-453

**SHOT-455 — Drum page Menu dropdown (page-scope route)**
- Reach: Open a Drums tab's own window (Ribbon > Drums slot > pick the instance) and click "Menu" on its title strip.
- Visible: Window/view nav entries at the top, sep, then Lock Drum, Polyphony, Rename..., Duplicate Drum (new tab), Choke Group submenu, Save Current Patch As..., sep, "Save Page Preset As...", "Load Page Preset" submenu (or "(no page presets saved)"), sep, "Delete Drum". **NO MIDI Note and NO MIDI Learn on this route.**
- Why separate: Same function, different item set from the kit-pad route — the pair must be shown side by side or readers will think items are missing.
- Variant of: SHOT-450

**SHOT-456 — Delete Drum confirm, clean patch**
- Reach: Drum page Menu > "Delete Drum" on a drum whose patch has not been edited since load.
- Visible: Warning AlertWindow "Delete Drum" — "Deleting this drum removes its Player, Mixer Strip, Effects Rack, and Piano Roll." with Delete / Cancel.

**SHOT-457 — Delete Drum confirm, dirty patch (three-way)**
- Reach: Edit a drum's BaySickSynth or BaySickPlayer patch, then Menu > "Delete Drum".
- Visible: QUESTION-icon AlertWindow "Delete Drum" with the same first paragraph plus an explanation that "Save Page Preset & Delete" writes the entire page state (engine + EQ + effects rack + strip settings) first. THREE buttons: "Save Page Preset & Delete" / "Delete" / "Cancel".
- Why separate: Different icon, different button count and extra body text, all driven by invisible dirty state.
- Variant of: SHOT-456

**SHOT-458 — Save Patch As dialog (drum)**
- Reach: Per-drum menu (either route) > "Save Current Patch As...".
- Visible: AlertWindow "Save Patch As" with a name text editor and Save / Cancel.
- Why separate: **Patch (engine only) vs Page Preset (whole chain) vs Kit (16 drums) are three different save scopes with three different dialogs** — SHOT-458, SHOT-459, SHOT-442. Manual 1 must show all three together.

**SHOT-459 — Save Page Preset dialog (drum)**
- Reach: Drum page Menu > "Save Page Preset As...".
- Visible: AlertWindow "Save Page Preset", prompt "Enter a name for this drum page preset:", text editor pre-filled "My Drum", Save / Cancel.
- Variant of: SHOT-458

**SHOT-460 — Drum page Player tab, empty (no sound)**
- Reach: Ribbon > Drums slot > add a drum but do not pick a sound; the page opens on its Player sub-tab.
- Visible: A plain dark page with a single centred 15pt dim line: "No sound loaded - pick one from the Drum Kit". **There is no picker button on the page itself** — the only sound-pick route is the kit grid's pads.
- Why separate: A dead-end-looking screen whose only instruction is that one sentence; it needs a labelled figure pointing at where to go.

**SHOT-461 — Drum page Player tab, engine loaded**
- Reach: Load any sound into a drum (kit pad picker > Sample or Synth Patch), then open that Drums tab's window.
- Visible: The hosted engine editor filling the entire page area — BaySickPlayer (in drum context, so its preset menu is filtered to drum folders) or BaySickSynth, depending on what was picked. **The engine type is never named on screen.**
- Why separate: The same page slot swaps between two totally different editors depending on the pad pick.
- Variant of: SHOT-460

---

## SITTING 18 — Shared control idioms (short but load-bearing)

These four appear on EVERY knob and fader in every engine editor and every ARIA
panel. Shoot them once here; Manual 1 footnotes them from all of the engine
chapters instead of repeating them.

**SETUP:** Any engine editor open. A MIDI controller connected, plus one knob
that already has a MIDI mapping saved.

**SHOT-462 — "Automate" right-click menu on any knob**
- Reach: Right-click any knob or fader in BaySickSynth, BaySickBass, BaySickPlayer, Harmless or any ARIA panel (e.g. the OSC tab's TRANSPOSE knob).
- Visible: A popup: "Automate: <friendly parameter name>", "Type in value...", then the MIDI Learn items appended by the shared automation helper.
- Why separate: **The doorway to automation and MIDI Learn for the entire app.**

**SHOT-463 — MIDI Learn rows in the right-click menu**
- Reach: Right-click a knob that already has a MIDI mapping, with a MIDI input device connected. Then repeat with nothing connected.
- Visible: "MIDI Learn" (greyed and RE-LABELLED "MIDI Learn (no MIDI input devices)" when nothing is connected), "MIDI Forget: <summary of the mapping>", and "Save MIDI mappings as global default".
- Why separate: Rows appear and disappear, and one of them re-words itself based on hardware and mapping state.
- Variant of: SHOT-462

**SHOT-464 — "Type in value" dialog**
- Reach: Right-click any knob > "Type in value...".
- Visible: Modal titled "Set <parameter name>"; prompt "Enter a new value:" with a second line "Range: <min> - <max>" formatted in the parameter's own units; a text field pre-filled with the current value; OK (Return) and Cancel (Escape).

**SHOT-465 — Knob value popup while dragging**
- Reach: Click and hold-drag any knob in Harmless or BaySickPlayer (these knobs have NO printed value box).
- Visible: A small floating value bubble beside the knob showing the current value in its own units.
- Why separate: **BaySickSynth and BaySickBass knobs carry a permanent numeric value box under the knob; Harmless and BaySickPlayer knobs do not** — the drag bubble is their only readout, and a reader will not know it exists. (The ARIA variant shows an integer 0-127 — SHOT-584.)

---

## SITTING 19 — BaySickSynth and BaySickBass

**Design sizes for framing:** both editors are 480x440, hosted inside a
WorkspaceWindow. **Engine accents:** BaySickSynth #A0DB2B (yellow-green),
BaySickBass #33FF88 (neon green); both use cyan #00CED1 for the LFO shape/dest
strips. The two editors are near-identical clones — same six tabs, same eleven
waveforms, same groups — differing in accent colour, engine name and preset
library. Both are shot because the atlas pictures every screen a user can open.

**Reach routes from the ribbon "+" slot:** BaySickSynth is a flat row (Layers
only). BaySickBass is a flat row (Bass only).

**SHOT-466 — BaySickSynth, window title strip + Preset button**
- Reach: Ribbon "+" > BaySickSynth. Shoot just the window's top title strip.
- Visible: Centred engine name "BaySickSynth" in accent green (#A0DB2B) with a bloom halo; the "Preset" button with its drawn down-chevron; window Menu; close control.
- Why separate: **The Preset button is NOT inside any engine editor** — each editor owns it but it is mounted on the hosting window's title strip. Readers will hunt for it in the editor body.

**SHOT-467 — BaySickSynth, OSC tab (full editor, default)**
- Reach: Ribbon "+" > BaySickSynth. The window opens on the OSC tab.
- Visible: Top to bottom — a 120px visualizer scope panel (dark rounded panel, 3 horizontal grid lines + 1 centre vertical line, green oscillator trace, waveform name in small green text along the bottom); a 30px tab row of six buttons OSC / OSC ENV / FILTER / FLT ENV / LFO / MOD with OSC lit; a divider; then the control deck in three grouped columns — WAVEFORM (waveform combo, dual-osc-mode combo, TRANSPOSE / MODIFIER / NOISE knobs each with a numeric value box beneath, SYNC and RING switch buttons on the bottom row); VOICE MODE (a 1x3 lit button strip Poly | Mono | Legato, CUT SELF button, CUT SELF mode button reading "SAME PITCH", SLIDE and OUT VOL knobs); MOD WHEEL (a 1x2 lit strip Filter | LFO, AMOUNT knob).

**SHOT-468 — BaySickSynth, waveform dropdown open**
- Reach: OSC tab > click the top combo in the WAVEFORM group.
- Visible: Eleven entries in order: SAW, SAW+SAW, PULSE, SAW+SQUARE, SQUARE+SQUARE, SUPERSAW, BELL, DEAF SAW, SPREAD OCT, SPREAD 5TH, SINE, with the current one ticked.
- Why separate: The eleven waveform names are the vocabulary the rest of the OSC page depends on.

**SHOT-469 — BaySickSynth, visualizer scope on an alternate waveform**
- Reach: OSC tab > set the waveform combo to SUPERSAW (or PULSE / BELL). Shoot the visualizer strip only.
- Visible: The 120px scope with a fat multi-layer green trace (glow stroke, mid stroke, crisp stroke) drawn as one cycle, and the waveform name "SUPERSAW" centred in small dim green at the bottom.
- Why separate: The scope redraws per waveform AND per MODIFIER position; one default SAW shot does not teach that the picture tracks the combo.
- Variant of: SHOT-467

**SHOT-470 — BaySickSynth, dual-osc tuning dropdown open**
- Reach: OSC tab > click the second (smaller) combo in the WAVEFORM group.
- Visible: Three entries: Musical, Hz Offset, Absolute Hz.

**SHOT-471 — BaySickSynth, MODIFIER readout in Hz Offset mode**
- Reach: OSC tab > set the dual-osc combo to "Hz Offset", then click-drag the MODIFIER knob and hold.
- Visible: The MODIFIER drag popup showing a value in Hz (e.g. "820 Hz") instead of the plain 0-1 number, with the knob's own value box below it.
- Why separate: **The same knob reports three different UNITS depending on the dual-osc mode** — a reader cannot deduce this from one picture.
- Variant of: SHOT-467

**SHOT-472 — BaySickSynth, MODIFIER readout in Absolute Hz mode**
- Reach: OSC tab > set the dual-osc combo to "Absolute Hz", then drag the MODIFIER knob and hold.
- Visible: The drag popup showing an absolute frequency, switching to kHz above 1000 (e.g. "1.24 kHz").
- Why separate: Third distinct readout format on the same control.
- Variant of: SHOT-471

**SHOT-473 — BaySickSynth, SYNC and RING engaged**
- Reach: OSC tab > click SYNC and click RING so both are lit.
- Visible: The two switch-style toggles at the bottom of the WAVEFORM group in their ON appearance.
- Why separate: Switch toggles look meaningfully different on/off and there is no other lit-toggle reference in this editor.
- Variant of: SHOT-467

**SHOT-474 — BaySickSynth, VOICE MODE strip, each mode selected**
- Reach: OSC tab > click Mono, then Legato in the VOICE MODE strip (one capture per pick, or a composite of the three).
- Visible: The 1x3 strip — the selected cell gets an accent-tinted fill, an accent top bar, an accent border and accent bold label text; unselected cells are flat dark with grey text.
- Why separate: **This is the app's LED-radio idiom and it appears in five places in this editor** — it needs one clear picture.
- Variant of: SHOT-467

**SHOT-475 — BaySickSynth, CUT SELF mode reading CUT ALL**
- Reach: OSC tab > click the button labelled "SAME PITCH" in the VOICE MODE group.
- Visible: The same button now reading "CUT ALL" in its toggled-on appearance.
- Why separate: The button's TEXT changes, not just its lit state — a reader looking for "CUT ALL" would never find it in a default shot.
- Variant of: SHOT-467

**SHOT-476 — BaySickSynth, MOD WHEEL destination = LFO**
- Reach: OSC tab > click "LFO" in the MOD WHEEL two-cell strip.
- Visible: The MOD WHEEL group with the LFO cell lit and Filter dark, plus the AMOUNT knob.
- Variant of: SHOT-467

**SHOT-477 — BaySickSynth, OSC ENV tab**
- Reach: Click the "OSC ENV" tab button.
- Visible: Visualizer showing an ADSR envelope trace (filled under-curve, glow strokes, and small A / D / S / R letters along the baseline of each segment); deck split into AMP ENV (four vertical faders with value boxes labelled ATTACK / DECAY / SUSTAIN / RELEASE plus a VEL knob at the right) and PITCH ENV (four vertical faders plus a bipolar AMOUNT knob).

**SHOT-478 — BaySickSynth, OSC ENV with an extreme envelope**
- Reach: On the OSC ENV tab, drag ATTACK and RELEASE most of the way up and SUSTAIN to about half.
- Visible: The ADSR trace redrawn with a long attack ramp, a low sustain shelf and a long release tail; the A/D/S/R baseline letters spread to match.
- Why separate: The envelope graph is the whole point of this tab and the default shape is nearly a spike; a second shape proves the graph is live.
- Variant of: SHOT-477

**SHOT-479 — BaySickSynth, FILTER tab**
- Reach: Click the "FILTER" tab button.
- Visible: Visualizer showing a filter frequency-response curve with a 0 dB horizontal reference line and a vertical cutoff marker; deck left half is the XY pad (dark rounded panel, quarter grid lines, "CUTOFF" label along the bottom edge, "RES" up the left edge, a glowing accent dot with crosshair lines through it); right half stacked as TYPE (a 1x4 lit strip LP | HP | BP | Notch) over TRACKING (KEYBOARD and VELOCITY knobs with value boxes).

**SHOT-480 — BaySickSynth, FILTER XY pad dot dragged**
- Reach: On the FILTER tab, click and drag inside the XY pad toward the top-left.
- Visible: The dot at a new position, its horizontal + vertical crosshair lines redrawn to it, the three-layer glow around it, and **the visualizer's response curve + cutoff marker moved to match.**
- Why separate: The pad-to-curve linkage is invisible in a single static shot.
- Variant of: SHOT-479

**SHOT-481 — BaySickSynth, FILTER type = HP**
- Reach: FILTER tab > click "HP" in the TYPE strip.
- Visible: TYPE strip with HP lit; the visualizer showing a high-pass response rising left-to-right.
- Why separate: Each filter type draws a visibly different curve — exactly what the atlas exists to disambiguate.
- Variant of: SHOT-479

**SHOT-482 — BaySickSynth, FILTER type = BP**
- Reach: FILTER tab > click "BP".
- Visible: TYPE strip with BP lit; the visualizer showing a band-pass peak at the cutoff marker.
- Variant of: SHOT-479

**SHOT-483 — BaySickSynth, FILTER type = Notch**
- Reach: FILTER tab > click "Notch".
- Visible: TYPE strip with Notch lit; the visualizer showing a notch dip at the cutoff marker.
- Variant of: SHOT-479

**SHOT-484 — BaySickSynth, FLT ENV tab**
- Reach: Click the "FLT ENV" tab button.
- Visible: Visualizer showing the filter ADSR trace with A/D/S/R baseline letters; deck is four vertical faders with value boxes (ATTACK / DECAY / SUSTAIN / RELEASE) across the left, and an AMOUNT group box on the right holding a single bipolar knob.

**SHOT-485 — BaySickSynth, LFO tab, SYNC off**
- Reach: Click the "LFO" tab button (SYNC is off by default).
- Visible: Visualizer showing a SCROLLING animated LFO waveform in cyan with the shape name ("Sine") centred at the bottom; deck in four labelled group boxes — SHAPE (a 1x3 cyan strip Sine | Saw | Square), RATE (rate knob with value box, a division combo, a SYNC button), DEST (a 1x3 cyan strip Filter | Pitch | Osc Mod), AMOUNT (one knob). **With SYNC off the RATE knob is live and the division combo is greyed.**

**SHOT-486 — BaySickSynth, LFO tab, SYNC on**
- Reach: LFO tab > click the SYNC button in the RATE group.
- Visible: SYNC lit; the RATE knob now greyed/disabled and the division combo now live. The scrolling visualizer speed follows the tempo-synced rate.
- Why separate: **Which of the two rate controls is greyed flips entirely** — the single most confusing state on this tab.
- Variant of: SHOT-485

**SHOT-487 — BaySickSynth, LFO division dropdown open**
- Reach: LFO tab with SYNC on > click the division combo.
- Visible: 1/1, 1/2, 1/4, 1/8, 1/16, 1/32.

**SHOT-488 — BaySickSynth, LFO shape = Square**
- Reach: LFO tab > click "Square" in the SHAPE strip.
- Visible: SHAPE strip with Square lit; the visualizer showing the scrolling square wave with the caption "Square" at the bottom.
- Why separate: Both the scope trace and its caption change with the shape pick.
- Variant of: SHOT-485

**SHOT-489 — BaySickSynth, LFO destination = Osc Mod**
- Reach: LFO tab > click "Osc Mod" in the DEST strip.
- Visible: DEST strip with Osc Mod lit, Filter and Pitch dark.
- Variant of: SHOT-485

**SHOT-490 — BaySickSynth, MOD tab**
- Reach: Click the "MOD" tab button.
- Visible: The visualizer STILL showing the scrolling LFO display — **the MOD tab is deliberately not in the scope rotation, so it holds the LFO view. That is intentional, not a bug, and a reader will notice.** Deck as five labelled group boxes across an eight-column grid: NOISE (NOISE ONLY switch, noise-colour combo); TRANSIENT (AMT / DUR / COLOUR knobs with value boxes); BURST ENV (BURST switch over COUNT / SPACING knobs); DRIFT (one DRIFT knob); UNISON (VOICES / DETUNE / SPREAD knobs).

**SHOT-491 — BaySickSynth, noise colour dropdown open**
- Reach: MOD tab > click the combo under the NOISE ONLY button.
- Visible: White, Pink, Brown.

**SHOT-492 — BaySickSynth, NOISE ONLY and BURST engaged**
- Reach: MOD tab > click NOISE ONLY and click BURST.
- Visible: Both switch buttons in their lit ON appearance inside their group boxes.
- Variant of: SHOT-490

**SHOT-493 — BaySickSynth, tooltip on the SYNC button**
- Reach: OSC tab > hover the SYNC button and hold still.
- Visible: The app-wide tooltip window showing the four-line hard-sync explanation — what it does, the classic sound it makes, which waveforms it applies to, how to sweep it.
- Why separate: Several controls here carry multi-line tooltips that are **the only in-app documentation of what they do.** (Others worth knowing while capturing: RING, noise colour, DRIFT, BURST, the TRANSIENT knobs, UNISON and CUT SELF.)

**SHOT-494 — BaySickSynth, tooltip on the dual-osc mode dropdown**
- Reach: OSC tab > hover the dual-osc combo and hold.
- Visible: A tooltip with the four-line explanation of Musical / Hz Offset / Absolute Hz including the 808-cowbell recipe.
- Why separate: Different content, and it is the key to the three MODIFIER readout variants.
- Variant of: SHOT-493

**SHOT-495 — BaySickSynth, Preset menu open**
- Reach: Click the "Preset" button on the window's title strip.
- Visible: One row per factory category folder, each opening a cascading submenu of preset names; a "My Presets" folder for user saves; a separator; "Save preset...".

**SHOT-496 — BaySickSynth, Preset menu with a folder submenu open**
- Reach: Open the Preset menu, then hover a category folder row.
- Visible: The cascading submenu of individual preset names for that folder, alongside the parent menu.
- Why separate: The cascade is how presets are actually found; a collapsed menu shows only folder names.
- Variant of: SHOT-495

**SHOT-497 — BaySickSynth, Save Preset dialog**
- Reach: Preset button > "Save preset...".
- Visible: Modal "Save Preset", prompt "Enter preset name:", a text field pre-filled "My Preset", Save / Cancel.

**SHOT-498 — BaySickBass, window title strip + Preset button**
- Reach: Ribbon "+" > BaySickBass. Shoot the title strip.
- Visible: Centred "BaySickBass" in neon green (#33FF88) with a bloom halo; the Preset button; window controls.
- Why separate: Different engine name and a visibly different accent green from BaySickSynth — the two are otherwise near-identical and readers will confuse them.
- Variant of: SHOT-466

**SHOT-499 — BaySickBass, OSC tab (full editor)**
- Reach: Ribbon "+" > BaySickBass. The window opens on the OSC tab.
- Visible: Identical layout to the BaySickSynth OSC tab but every accent is neon green — scope with green trace and waveform caption; the six-button tab row; WAVEFORM group (same 11 shapes, dual-osc combo, TRANSPOSE / MODIFIER / NOISE, SYNC + RING); VOICE MODE group; MOD WHEEL group.
- Variant of: SHOT-467

**SHOT-500 — BaySickBass, OSC ENV tab**
- Reach: BaySickBass window > click "OSC ENV".
- Visible: Green ADSR trace; AMP ENV box (4 vertical faders + VEL knob) and PITCH ENV box (4 vertical faders + bipolar AMOUNT knob).
- Variant of: SHOT-477

**SHOT-501 — BaySickBass, FILTER tab**
- Reach: BaySickBass window > click "FILTER".
- Visible: Green response curve; XY pad with CUTOFF / RES axis labels and a neon-green dot + crosshairs; TYPE strip LP | HP | BP | Notch; TRACKING group with KEYBOARD and VELOCITY knobs.
- Variant of: SHOT-479

**SHOT-502 — BaySickBass, FLT ENV tab**
- Reach: BaySickBass window > click "FLT ENV".
- Visible: Green filter ADSR trace; four vertical faders ATTACK / DECAY / SUSTAIN / RELEASE plus an AMOUNT group box with one knob.
- Variant of: SHOT-484

**SHOT-503 — BaySickBass, LFO tab**
- Reach: BaySickBass window > click "LFO".
- Visible: Cyan scrolling LFO trace with shape caption; SHAPE strip (Sine | Saw | Square); RATE group (knob, division combo, SYNC — division greyed while SYNC is off); DEST strip (Filter | Pitch | Osc Mod); AMOUNT group.
- Variant of: SHOT-485

**SHOT-504 — BaySickBass, LFO tab, SYNC on**
- Reach: BaySickBass LFO tab > click SYNC.
- Visible: RATE knob greyed, division combo live, SYNC lit.
- Why separate: Same grey-swap trap as the synth; a Bass reader will be looking at the Bass page.
- Variant of: SHOT-503

**SHOT-505 — BaySickBass, MOD tab**
- Reach: BaySickBass window > click "MOD".
- Visible: NOISE group (NOISE ONLY switch + colour combo); TRANSIENT group (AMT / DUR / COLOUR); BURST ENV group (BURST switch + COUNT / SPACING); DRIFT group; UNISON group (VOICES / DETUNE / SPREAD).
- Variant of: SHOT-490

**SHOT-506 — BaySickBass, Preset menu open**
- Reach: BaySickBass window > click "Preset" on the title strip.
- Visible: Cascading factory-folder submenus from the **BaySickBass** preset library, a My Presets folder, a separator and "Save preset...".
- Why separate: A distinct preset library — the contents differ from BaySickSynth's identically-shaped menu.
- Variant of: SHOT-495

**SHOT-507 — BaySickBass, Save Preset dialog**
- Reach: BaySickBass Preset button > "Save preset...".
- Visible: "Save Preset", prompt "Enter preset name:", text field pre-filled "My Bass Preset", Save / Cancel.
- Why separate: Different default text in the field.
- Variant of: SHOT-497

---

## SITTING 20 — Harmless

**Design size for framing:** 1039x421 content (window minimum 1047x455) — one
dense screen with **no tabs and no Basic/Advanced split.** Accent #FF6600
(orange). Reach: Ribbon "+" > Harmless > Layers (or > Bass).

Harmless has ~15 framed sections in one window, each a rounded recessed panel
with a small dim uppercase header at its top-left and tiny labels beneath every
knob. Each needs a labelled close-up or the atlas is unusable.

**SHOT-508 — Harmless, window title strip + Preset button**
- Reach: Shoot the top strip of the Harmless window.
- Visible: Centred "Harmless" in orange (#FF6600) with bloom halo; the Preset button; window controls.
- Variant of: SHOT-466

**SHOT-509 — Harmless, full editor**
- Reach: Ribbon "+" > Harmless > Layers. A wide short window opens showing the whole editor at once.
- Visible: Four-column dense layout on a dark chassis. TOP BAND: left column with OUTPUT / TREMOLO + ROUTING / VIBRATO-LEGATO framed sections; a middle narrow UNISON column; a right column with FILTER 1 + ADSR, FILTER 2 + ADSR and a full-width TIMBRE row. BOTTOM BAND: a left stack of PITCH + LFO MOD, STRUM + FX, AMP ENV/PHASE and BLUR/PRISM strips beside the XYZ/MOD pad; then the SPECTROGRAM column; then the Mod Editor filling the right-hand third.

**SHOT-510 — Harmless, OUTPUT section**
- Reach: Shoot the top-left section headed "OUTPUT".
- Visible: Header "OUTPUT"; VOL knob, PAN knob (bipolar), then four buttons in a row: VEL, CUT SELF, SAME PITCH, and the auto-gain button reading "AG: REL".

**SHOT-511 — Harmless, OUTPUT with AG: ABS**
- Reach: OUTPUT section > click the button reading "AG: REL".
- Visible: The same button now reading "AG: ABS" in its toggled-on state.
- Why separate: The button TEXT changes between two modes.
- Variant of: SHOT-510

**SHOT-512 — Harmless, OUTPUT with CUT ALL**
- Reach: OUTPUT section > click the button reading "SAME PITCH".
- Visible: The button now reading "CUT ALL", toggled on.
- Variant of: SHOT-510

**SHOT-513 — Harmless, TREMOLO section**
- Reach: Shoot the section headed "TREMOLO" in the top-left column.
- Visible: Header; a waveform ICON BUTTON labelled WAVE; then DEPTH, SPEED and GAP knobs with labels.

**SHOT-514 — Harmless, waveform icon button, all four shapes**
- Reach: Click the TREMOLO WAVE button repeatedly — each click cycles sine > saw > square > triangle. Capture all four.
- Visible: A small dark rounded box with an orange stroked waveform glyph inside: sine curve, then a two-ramp saw, then a square step, then a triangle.
- Why separate: **The same icon-button widget appears four times in Harmless** (Part A shape, Part B shape, Tremolo wave, Vibrato wave) and it is a cycle-on-click control with **no visible arrow** — a reader cannot tell it is clickable from one frame.
- Variant of: SHOT-513

**SHOT-515 — Harmless, ROUTING section**
- Reach: Shoot the section headed "ROUTING" beside TREMOLO.
- Visible: Header over a single packed row of six small knobs labelled SUB, PROT, CLIP, FX, VOL, ENV. No LEDs, no readouts.

**SHOT-516 — Harmless, VIBRATO / LEGATO section**
- Reach: Shoot the section headed "VIBRATO / LEGATO" at the bottom of the top-left column.
- Visible: Header; a waveform icon button labelled WAVE; DEPTH, SPEED, ENV, GLIDE, LIMIT knobs with labels; a LEGATO switch button.

**SHOT-517 — Harmless, UNISON column**
- Reach: Shoot the narrow full-height section headed "UNISON" between the left and right top columns.
- Visible: Header; VOICES knob; a full-width chicken-head type selector labelled TYPE; an ALT switch button; then a row of PAN, PITCH, PHASE knobs with labels.

**SHOT-518 — Harmless, FILTER 1 + ADSR row**
- Reach: Shoot the top row of the right-hand column, headed "FILTER 1 + ADSR".
- Visible: Header; on the left the filter row on its own recessed panel — a type combo reading LP, then ENV, FREQ, RES, KB knobs each with a small label; on the right four knobs ATK, DEC, SUS, REL.

**SHOT-519 — Harmless, filter type combo open**
- Reach: Click the type combo in the FILTER 1 row.
- Visible: LP, HP, BP, Notch.
- Variant of: SHOT-518

**SHOT-520 — Harmless, FILTER 2 + ADSR row**
- Reach: Shoot the second row of the right-hand column, headed "FILTER 2 + ADSR".
- Visible: Identical shape to Filter 1 — type combo, ENV / FREQ / RES / KB knobs, and ATK / DEC / SUS / REL knobs.
- Why separate: Two independent filters that look the same; the atlas must show both so a reader can tell which section they are pointing at.
- Variant of: SHOT-518

**SHOT-521 — Harmless, TIMBRE row, Part A active**
- Reach: Shoot the full-width bottom row of the right-hand column, headed "TIMBRE", with the A button lit.
- Visible: Header; A and B switch buttons (A lit); two waveform icon buttons labelled PART A and PART B; then knobs MIX, VOICE A, VOICE B, BROWN, F1 OFS, F2 OFS, A MASK, B MASK, each labelled beneath.

**SHOT-522 — Harmless, TIMBRE row, Part B active**
- Reach: TIMBRE row > click the "B" button.
- Visible: B lit, A unlit, and the shared knobs elsewhere in the editor (BROWN, BLUR, TIME, HARM, PRISM, MODE, PLUCK, MASK and the BLUR pluck button) jump to Part B's values.
- Why separate: **A/B is the single most confusing thing in this editor** — the same knobs edit different parameters depending on this button, and **both parts sound simultaneously** (timbre_blend crossfades them; part_sel is view state no DSP reads). The manual cannot explain it without both pictures.
- Variant of: SHOT-521

**SHOT-523 — Harmless, PITCH section**
- Reach: Shoot the section headed "PITCH" in the bottom band's left stack, top row.
- Visible: Header; FREQ knob, DETUNE knob, a chicken-head FRAC selector, and two small switch buttons OCT and Hz.

**SHOT-524 — Harmless, PITCH FREQ readout with OCT engaged**
- Reach: PITCH section > click OCT, then drag the FREQ knob and hold.
- Visible: The FREQ drag popup reading in octaves (e.g. "+1 oct"); the drag now snaps in octave steps.
- Why separate: OCT and Hz change the FREQ knob's UNITS and DRAG BEHAVIOUR without changing anything visible on the knob itself.
- Variant of: SHOT-523

**SHOT-525 — Harmless, PITCH FREQ readout with Hz engaged**
- Reach: PITCH section > click Hz, then drag the FREQ knob and hold.
- Visible: The FREQ drag popup reading an absolute frequency in Hz / kHz.
- Variant of: SHOT-523

**SHOT-526 — Harmless, LFO MOD section**
- Reach: Shoot the section headed "LFO MOD" beside PITCH.
- Visible: Header; RATE knob, a chicken-head SHAPE knob, a TEMPO switch button, then VEL, VOL, PITCH knobs with labels.

**SHOT-527 — Harmless, LFO MOD with TEMPO engaged**
- Reach: LFO MOD > click TEMPO, then hover/drag the RATE knob.
- Visible: TEMPO lit; the RATE popup reading a beat length ("1/8 beats" ... "32 beats") rather than a plain number.
- Why separate: The rate readout switches between beats and seconds on this toggle.
- Variant of: SHOT-526

**SHOT-528 — Harmless, STRUM section**
- Reach: Shoot the section headed "STRUM" in the bottom band's second row.
- Visible: Header; a chicken-head DIR selector, TIME knob, TNS knob, each labelled.

**SHOT-529 — Harmless, FX section**
- Reach: Shoot the section headed "FX - PLUCK / PHASER / EQ" beside STRUM.
- Visible: Header; PLUCK knob; a BLUR switch button; then MIX, DEPTH, RATE, WIDTH, OFS, MASK and EQ knobs each with a small label.

**SHOT-530 — Harmless, AMP ENV / PHASE section**
- Reach: Shoot the section headed "AMP ENV / PHASE" in the bottom band's left half.
- Visible: Header; six knobs labelled ATK, DEC, SUS, REL, START, RAND.

**SHOT-531 — Harmless, BLUR / PRISM section**
- Reach: Shoot the section headed "BLUR / PRISM" below AMP ENV.
- Visible: Header; five controls labelled BLUR, TIME, HARM, PRISM and a chicken-head MODE selector.

**SHOT-532 — Harmless, XYZ / MOD pad**
- Reach: Shoot the tall panel between the left knob stack and the spectrogram; it carries its own "MOD" caption.
- Visible: A recessed pad with a centre crosshair (one horizontal, one vertical hairline), an orange dot with a soft halo at centre, the word "MOD" in the top-left corner, and below the pad three knobs labelled X, Y, Z.

**SHOT-533 — Harmless, XYZ pad with the dot dragged off-centre**
- Reach: Click and drag inside the MOD pad toward a corner.
- Visible: The orange dot at the new position, and the X and Y knobs beneath rotated to match.
- Why separate: The pad-drives-knobs link is invisible at rest, and the default centred dot reads as decoration.
- Variant of: SHOT-532

**SHOT-534 — Harmless, SPECTROGRAM idle**
- Reach: Shoot the column headed "SPECTROGRAM" with no notes playing.
- Visible: Header; a pitch-black panel with three faint horizontal grid lines and a thin border — and nothing else.

**SHOT-535 — Harmless, SPECTROGRAM with a sustained chord**
- Reach: Hold a chord on the piano-roll keyboard (or a MIDI keyboard) while the Harmless window is open, and shoot the SPECTROGRAM column.
- Visible: Hundreds of thin orange vertical bars of varying height across the panel — one per harmonic partial — with peak-fall decay.
- Why separate: **An empty black box and a wall of orange bars are the same widget**; a beginner shown only the idle state will think it is broken.
- Variant of: SHOT-534

**SHOT-536 — Harmless, Mod Editor, Envelope source (default)**
- Reach: Shoot the right-hand third of the Harmless window. It opens on target 1 with the Envelope source.
- Visible: Top row — two dropdowns side by side: Articulations (target) left, Modulations (source) right. Second row — an "ENV" tab button, then CURVE / STEP / SNAP tool buttons, then FREEZE / + / - buttons and a snap-division dropdown. Middle — the curve graph: dark panel, vertical grid lines whose density follows the chosen division, three horizontal quarter lines plus a brighter centre line, an orange curve with a glow and round orange control-point dots. Below the graph — a horizontal scroll bar. Bottom strip — DEPTH knob, LENGTH knob, TEMPO button, SPD, TNS, SKEW, PW knobs with small labels beneath.
- Note: The Mod Editor is effectively a second editor inside the first, with its own undo (Ctrl+Z / Ctrl+Y while focused) and its own right-click delete on curve points (right-clicking a middle point deletes it; the two boundary points are protected and silently refuse).

**SHOT-537 — Harmless, Mod Editor, Articulations dropdown open**
- Reach: Click the LEFT dropdown at the top of the Mod Editor.
- Visible: The 16 modulation targets in order: Volume, Pan, Pitch, Timbre Blend, Pluck Decay, Prism Amount, Blur Harm, Filter 1 Cutoff, Filter 2 Cutoff, Filter 1 Resonance, Filter 2 Resonance, Phaser Mix, Phaser Width, Unison Pitch Thickness, Part A Level, Part B Level.

**SHOT-538 — Harmless, Mod Editor, Modulations dropdown open**
- Reach: Click the RIGHT dropdown at the top of the Mod Editor.
- Visible: The 7 sources: Envelope, LFO, Velocity, Keyboard, Mod X, Mod Y, Mod Z.

**SHOT-539 — Harmless, Mod Editor with LFO source**
- Reach: Set the Modulations dropdown to "LFO".
- Visible: A SHAPE knob appears in the bottom strip (hidden for every other source); LENGTH and TEMPO stay; the graph draws the selected LFO waveform as a continuous orange trace with **NO draggable dots.**
- Why separate: The bottom strip gains a control and the graph stops being editable — a fundamentally different screen.
- Variant of: SHOT-536

**SHOT-540 — Harmless, Mod Editor with Velocity source**
- Reach: Set the Modulations dropdown to "Velocity" (Keyboard / Mod X / Mod Y / Mod Z look the same).
- Visible: LENGTH, TEMPO and SHAPE are ALL GONE from the bottom strip — only DEPTH, SPD, TNS, SKEW, PW remain; the curve graph is still editable.
- Why separate: Three controls disappear; a reader comparing against the default will think something is broken.
- Variant of: SHOT-536

**SHOT-541 — Harmless, Mod Editor, STEP mode curve**
- Reach: Mod Editor > click STEP, then click a few times inside the graph to add points.
- Visible: A stair-stepped orange curve holding each value until the next point, versus the smooth curve CURVE mode produces from identical clicks.
- Variant of: SHOT-536

**SHOT-542 — Harmless, Mod Editor, SNAP on with division dropdown open**
- Reach: Mod Editor > click SNAP, then click the division dropdown at the right end of the tool row.
- Visible: SNAP lit; the dropdown listing the app's unified snap divisions; behind it the graph's vertical grid redrawn at the chosen density.

**SHOT-543 — Harmless, Mod Editor zoomed in**
- Reach: Mod Editor > click "+" two or three times.
- Visible: A small "4x" (or "2x" / "8x") indicator in the top-right corner of the graph; the scroll bar beneath now has a short thumb that can be dragged to pan; grid lines further apart with finer rungs visible.
- Why separate: The zoom indicator and the live scroll thumb exist only above 1x.
- Variant of: SHOT-536

**SHOT-544 — Harmless, Mod Editor with FREEZE engaged**
- Reach: Mod Editor > click FREEZE.
- Visible: FREEZE lit; clicks and drags inside the graph do nothing — no new points appear.
- Why separate: A locked editor that silently ignores clicks is exactly the state a beginner needs told about.
- Variant of: SHOT-536

**SHOT-545 — Harmless, Mod Editor bottom knob strip close-up**
- Reach: Shoot only the bottom ~50px of the Mod Editor with the Envelope source selected.
- Visible: DEPTH knob, LENGTH knob, TEMPO button, the SHAPE slot (empty for Envelope), SPD, TNS, SKEW, PW knobs, each with its label centred beneath; labels overhang their knobs into the gaps.
- Why separate: Eight controls at 16px with 8pt labels — unreadable in the full-editor shot.

**SHOT-546 — Harmless, right-click menu on a modulatable knob**
- Reach: Right-click the VOL knob in the OUTPUT section (or any of the 16 registered targets listed in SHOT-537).
- Visible: "Automate: <friendly parameter name>", "Type in value...", **"Modulate envelope..."**, then the MIDI Learn items.
- Why separate: The third item appears only on knobs the mod matrix knows about — this is how a knob gets into the Mod Editor.

**SHOT-547 — Harmless, right-click menu on a NON-modulatable knob**
- Reach: Right-click a knob that is not a registered target, e.g. TREMOLO DEPTH or STRUM TIME.
- Visible: "Automate: ...", "Type in value..." and the MIDI Learn items but **NO "Modulate envelope..." row.**
- Why separate: The missing row is the whole point — a reader needs both menus side by side to learn which knobs can be enveloped.
- Variant of: SHOT-546

**SHOT-548 — Harmless, Preset menu open**
- Reach: Click "Preset" on the Harmless window's title strip.
- Visible: One cascading submenu per genre folder (e.g. Modern Hip-Hop, Psytrance), a My Presets folder, a separator, "Save preset..." and **"Init (reset to default)"**.
- Why separate: Harmless's menu carries an "Init" row the other engines' preset menus do not have.
- Variant of: SHOT-495

**SHOT-549 — Harmless, Preset menu with no presets installed**
- Reach: Click Preset on a machine/profile where the Harmless preset folder is empty.
- Visible: A greyed, non-clickable "(no presets installed)" row above the separator, then "Save preset..." and "Init (reset to default)".
- Variant of: SHOT-548

**SHOT-550 — Harmless, Save Preset dialog**
- Reach: Harmless Preset button > "Save preset...".
- Visible: Modal "Save Preset", prompt "Enter a name:", text field pre-filled "My Preset", Save (bound to Enter) and Cancel.
- Why separate: Different prompt wording from the synth's ("Enter a name:" vs "Enter preset name:") and Save is Return-bound here.
- Variant of: SHOT-497

---

## SITTING 21 — BaySickPlayer

**Design size for framing:** 600x560. Accent #D4A017 (gold). No tabs — one
seven-box grid on a flat dark background with **no box outlines**; seven bold
centred section titles group the controls. Knobs have no permanent value box —
the drag bubble (SHOT-465) is their only readout.

**Reach routes:** Ribbon "+" > BaySickPlayer > Layers / Bass / Audio Clips; also
via "+" > BaySickDrums > BaySickPlayer for the drum-context variant.

**SHOT-551 — BaySickPlayer, window title strip + Preset button**
- Reach: Shoot the top strip of any BaySickPlayer window.
- Visible: Centred "BaySickPlayer" in gold (#D4A017) with bloom halo; the Preset button; window controls.
- Variant of: SHOT-466

**SHOT-552 — BaySickPlayer, full editor (7-box grid)**
- Reach: Ribbon "+" > BaySickPlayer > Layers. The window opens showing the whole editor.
- Visible: Top row of three boxes — SAMPLE ENGINE, PITCH & VOICING, DYNAMICS. Bottom row of four — AMP ENVELOPE, VIBRATO, FILTER, OUTPUT. Every control is a small rotary with an uppercase label beneath; the DYNAMICS box also carries two drawn arrow glyphs between its columns.

**SHOT-553 — BaySickPlayer, SAMPLE ENGINE box**
- Reach: Shoot the top-left box titled "SAMPLE ENGINE".
- Visible: Title; SMPL START and STRETCH knobs with labels; below them three tall switch toggles spanning the rest of the box — REVERSE (OFF/ON), CUT SELF (OFF/ON), and a NAMED switch showing SAME PITCH above and CUT ALL below.

**SHOT-554 — BaySickPlayer, SAMPLE ENGINE with toggles engaged**
- Reach: SAMPLE ENGINE box > click REVERSE, click CUT SELF, and flip the third switch to CUT ALL.
- Visible: The three switch filmstrips in their down/ON position with the active label highlighted.
- Why separate: A switch toggle's OFF and ON art differ, and the third switch is a named TWO-CHOICE switch rather than OFF/ON.
- Variant of: SHOT-553

**SHOT-555 — BaySickPlayer, PITCH & VOICING box**
- Reach: Shoot the top-middle box titled "PITCH & VOICING".
- Visible: Title; TUNE and VOICE CAP knobs on row 1; UNISON and DETUNE on row 2; a large chicken-head selector labelled DET MODE and a SPREAD knob on row 3.

**SHOT-556 — BaySickPlayer, detune-mode selector, S / R / P**
- Reach: PITCH & VOICING > click the DET MODE selector to cycle it. Capture the pointer in each of its three positions.
- Visible: The chicken-head rotary with a pointer and the letters S, R, P around its bezel, the current one highlighted.
- Why separate: A three-position selector whose meaning is carried by single letters; each position looks different and each needs naming.
- Variant of: SHOT-555

**SHOT-557 — BaySickPlayer, detune-mode selector menu open**
- Reach: Right-click (or click) the DET MODE selector.
- Visible: "Automate: <name>", a separator, then the three options S, R, P with the current one ticked.
- Why separate: The letters are only explained in tooltips and this menu; the menu is the readable form.
- Variant of: SHOT-556

**SHOT-558 — BaySickPlayer, DYNAMICS box with routing arrows**
- Reach: Shoot the top-right box titled "DYNAMICS".
- Visible: Title; six knobs in two columns of three — SENS and VEL>MASTER on row 1, VEL>MUFFLE and MUFFLE on row 2, VEL>HARD and HARDNESS on row 3 — with a drawn grey ARROW glyph pointing from the left knob to the right knob on rows 2 and 3.
- Why separate: **The two arrows are painted decoration, not controls**, and they encode the routing — a reader will otherwise not know why the knobs are paired.

**SHOT-559 — BaySickPlayer, AMP ENVELOPE box**
- Reach: Shoot the bottom-row box titled "AMP ENVELOPE".
- Visible: Title; four knobs in a 2x2 block labelled ATTACK, DECAY, SUSTAIN, RELEASE. **No envelope graph.**

**SHOT-560 — BaySickPlayer, VIBRATO box**
- Reach: Shoot the bottom-row box titled "VIBRATO".
- Visible: Title; two knobs centred in the middle row labelled VIB RATE and VIB DEPTH, with the rows above and below empty.
- Why separate: A near-empty box with two knobs floating in the middle reads as unfinished; the atlas should show it as intentional.

**SHOT-561 — BaySickPlayer, FILTER box**
- Reach: Shoot the bottom-row box titled "FILTER".
- Visible: Title; CUTOFF and RES knobs on row 1, REDUCT alone on row 2 with an empty cell beside it.

**SHOT-562 — BaySickPlayer, OUTPUT box**
- Reach: Shoot the bottom-right box titled "OUTPUT".
- Visible: Title; PAN and STEREO knobs on row 1; OVERDRIVE and TREBLE on row 2; MASTER VOL alone on row 3 — **drawn with a different WHITE knob graphic from every other knob in the editor.**
- Why separate: The white master-volume knob is visually unlike the rest and readers will ask why.

**SHOT-563 — BaySickPlayer, Preset / sample menu (melodic context)**
- Reach: On a Layers or Bass BaySickPlayer window, click "Preset" on the title strip.
- Visible: A "Load Sample" section header, then "Open Folder...", "Open SFZ...", "Open Sample..."; a separator and a "Core Library" submenu; a separator and a "Presets" section header, then per-genre preset folders (melodic packs only) and a My Presets folder; a separator and "Save preset...".
- Why separate: The biggest menu in the four editors, and it is how a sound gets loaded at all.

**SHOT-564 — BaySickPlayer, Core Library submenu expanded**
- Reach: Preset menu > hover "Core Library", then hover a pack folder.
- Visible: Cascading submenus, one level per library folder; **folders that directly contain audio become a single clickable row**; SFZ files show as "<name>  [SFZ]"; individual audio files show by filename.
- Why separate: The "[SFZ]" suffix and the folder-becomes-one-row rule are non-obvious and only visible when expanded.
- Variant of: SHOT-563

**SHOT-565 — BaySickPlayer, Preset menu in drum context**
- Reach: Ribbon "+" > BaySickDrums > BaySickPlayer to make a Drums tab, then click "Preset" on that window's title strip.
- Visible: The same Load Sample rows at the top, but **NO "Core Library" submenu**, and the Presets section lists only drum-pack folders plus My Presets.
- Why separate: Two visibly different menus from the same button depending on which page type hosts the engine — a classic atlas disambiguation.
- Variant of: SHOT-563

**SHOT-566 — BaySickPlayer, Open Folder file chooser**
- Reach: Preset menu > "Open Folder...".
- Visible: Native folder-picker "Select Sample Folder", opened at the installed Core Library folder (or the user's Music folder if the library is missing).

**SHOT-567 — BaySickPlayer, Open SFZ file chooser**
- Reach: Preset menu > "Open SFZ...".
- Visible: File picker "Select SFZ File", filtered to *.sfz, opened at the Core Library folder.
- Why separate: Different title and filter; the three load routes produce three different results.
- Variant of: SHOT-566

**SHOT-568 — BaySickPlayer, Open Sample file chooser**
- Reach: Preset menu > "Open Sample...".
- Visible: File picker "Select Sample File", filtered to wav / aif / aiff / flac / ogg / mp3.
- Variant of: SHOT-566

**SHOT-569 — BaySickPlayer, Save Preset dialog**
- Reach: Preset menu > "Save preset...".
- Visible: Modal "Save Preset", prompt "Enter preset name:", text field pre-filled "My BaySickPlayer Preset", Save / Cancel.
- Why separate: Different default field text again.
- Variant of: SHOT-497

**SHOT-570 — BaySickPlayer, editor hosted on a Drums tab**
- Reach: Ribbon "+" > BaySickDrums > BaySickPlayer, then open that window's player view.
- Visible: The identical seven-box grid embedded in a Drums page rather than a Layers page — same controls, but the title strip says the drum tab's name and the Preset menu behaves as SHOT-565.
- Why separate: The same editor appears under three different page types; readers navigating from the Drums chapter need to see it in place.
- Variant of: SHOT-552

**SHOT-571 — BaySickPlayer, editor hosted on a Clips tab**
- Reach: Ribbon "+" > BaySickPlayer > Audio Clips, then open the Player view on that window.
- Visible: The same seven-box grid on a Clips page, with the Clips page's own sub-tab chrome around it.
- Variant of: SHOT-552

**SHOT-572 — BaySickPlayer, "no playable samples" warning**
- Reach: Preset menu > "Open Folder..." and deliberately pick a folder containing no loadable audio.
- Visible: Warning alert "Load Sample" reading "No playable samples could be loaded from: <path>" plus "It may be damaged, or the samples it needs may be missing.", with OK.
- Why separate: Trivially triggerable on demand, and it is the **only feedback that a load silently produced silence.**

---

## SITTING 22 — BaySickRustyDrums (ARIA player + kit graphic)

**Reach:** Ribbon > click the Drums slot chevron > "+ Add BaySickRustyDrums".
The page opens in its own window on the Player sub-tab. **There is no sub-tab
bar on this page — the window Menu IS the navigation.**

**Two big honesty notes for Manual 1:**
1. The kit graphic has **25** clickable pieces, not 16. (16 is the OTHER surface
   — the Drum Kit sidebar's 16 rows per bank.) The 25 are: Crash 17; Stack Mid;
   Stack Edge; Crash Sizzle 17 Crash / Bow / Bell; Ride 22 Edge Crash / Bow /
   Bell; Ride Sizzle 19 Bow / Edge Crash / Bell; China 18; Hi-hat Tip; Hi-hat
   Shaft; Tom 14; Tom 15; Snare Center / Edge / Rim / Sidestick; Kick; Tom 18;
   Tom 22; Hi-hat Pedal.
2. **The hitboxes are INVISIBLE.** Nothing is drawn on hover — only a blue ring
   while a piece is held pressed, plus the permanent hi-hat pedal ring. Labelling
   all 25 needs either an annotated composite drawn over a clean kit shot, or 25
   press-and-hold captures. That is a production decision, not something a
   screenshot list can solve. SHOT-590 and SHOT-591 are the raw material.

**SHOT-573 — Rusty Player tab, no program loaded**
- Reach: Add a BaySickRustyDrums tab. It lands on the Player sub-tab automatically.
- Visible: Title strip: "Menu" dropdown, Swing Mix knob, then right-side extras — a "Presets" button (76px) and the Program ComboBox (80px) showing its placeholder **"Load Player"**. The widths matter: the strip centres "BaySickRustyDrums" in the span these leave, and at the old 110 + 160 the name rendered clipped. Below: a 32px BaySickTitleBar band (0xFF141618 fill, 1px 0xFF333537 divider) with NO section tab buttons yet. Body: a dark 0xff1a1a1a rectangle with grey centred 16pt text **"Loading control surface..."** — this IS the empty state despite the wording.

**SHOT-574 — Rusty Player tab, Full program, Main section**
- Reach: Set the Program dropdown on the title strip to "Full".
- Visible: The 32px band now hosting a centred section tab row — Main / Kick / Snare / Toms / Hi-hat / Cymbals / Noises (7 buttons, max 110px each, "Main" toggled). Below it the ratio-locked 775x335 kit artwork (control_tab_full.png) scaled and centred, with **72 filmstrip Knobs, 6 OptionMenu dropdowns** (bordered rounded rects with a chevron at the right edge and the current item name centred) and **87 static text labels** baked from the GUI XML — the mic/section names (Close, OH, Btm, Top, Punch, Tune, Dirt, Deaden, Snap, Buzz, Epic, Pan, Low Cut, High Cut) plus section headings.
- Why separate: The flagship control surface, and Full has 72 knobs / 6 menus vs Basic's 46 / 1 — a reader cannot map one from the other.

**SHOT-575 — Rusty Player tab, Basic program, Main section**
- Reach: With Full loaded, set the Program dropdown to "Basic" and confirm the switch prompt (SHOT-597).
- Visible: Same band + tab row. Different background art (control_tab_basic.png), **46 knobs, 1 OptionMenu, 54 static texts** — visibly sparser, with whole mic/section clusters absent.
- Why separate: Different artwork and a different control count. A manual page showing only Full leaves Basic users looking at a panel they cannot find in the book.
- Variant of: SHOT-574

**SHOT-576 — Rusty Player, Kick section tab**
- Reach: Rusty Player sub-tab > click "Kick" in the section tab row.
- Visible: The zoomed kick page (03-kick.xml): control_tab_kick.png background, 9 knobs, 11 static labels; section tab row still on top with "Kick" toggled.
- Why separate: Each zoom page is its own XML with its own artwork and its own control set — six separate screens auto-discovered from the kit folder.

**SHOT-577 — Rusty Player, Snare section tab**
- Reach: Click "Snare" in the section tab row.
- Visible: 04-snare.xml page — control_tab_snare.png background; snare knobs (Top / Btm / OH / Snap / Punch / Epic / Tune / Dirt / Deaden); any snare articulation OptionMenus (Snare Type, Stir Type); static labels.
- Variant of: SHOT-576

**SHOT-578 — Rusty Player, Toms section tab**
- Reach: Click "Toms".
- Visible: 05-toms.xml page — control_tab_toms.png background; per-tom knobs (Lo Tom Punch / Hi Tom Punch / Tom Dirt / Tom Deaden / Tune / Pan); a Tom Type OptionMenu if present; labels.
- Variant of: SHOT-576

**SHOT-579 — Rusty Player, Hi-hat section tab**
- Reach: Click "Hi-hat".
- Visible: 06-hihat.xml page — control_tab_hihat.png background; hi-hat knobs including **Hi-hat Position** (the CC4 pedal-openness macro that the kit graphic's pedal toggle drives), Close, OH, Pan, Tune.
- Variant of: SHOT-576

**SHOT-580 — Rusty Player, Cymbals section tab**
- Reach: Click "Cymbals".
- Visible: 07-cymbals.xml page — control_tab_cymbals.png background; per-cymbal knobs for Crash 17 / Crash Sizzle 17 / Ride 22 / Ride Sizzle 19 / China 18 / Stack.
- Variant of: SHOT-576

**SHOT-581 — Rusty Player, Noises section tab**
- Reach: Click "Noises".
- Visible: 08-noises.xml page — control_tab_noises.png background; the kit's incidental-noise knobs.
- Variant of: SHOT-576

**SHOT-582 — ARIA knob tooltip (decoded beginner explanation)**
- Reach: On any loaded ARIA panel (Rusty Full is densest), hover a knob whose label is jargon — "Snare Btm", "Kick Buzz", "Hi-hat Position" — and hold still.
- Visible: A tooltip in three parts: the kit's own label, a hyphen and the live 0-127 CC value, a blank line, then a full-sentence plain-English explanation from the built-in drum-term glossary (e.g. "Bottom-snare mic - captures the snare wires, gives the 'crack' and 'sizzle'.").
- Why separate: **This tooltip carries real teaching content that exists nowhere else on screen** — a manual that omits it loses the app's own glossary.

**SHOT-583 — ARIA knob right-click menu**
- Reach: On any loaded ARIA panel, right-click any knob, fader or option menu.
- Visible: "Automate: <resolved parameter name>", "Type in value...", then the MIDI Learn items.
- Why separate: The shared automation menu (SHOT-462) in ARIA context — the only route to automation and typed entry on Rusty, Guitars and Basses.
- Variant of: SHOT-462

**SHOT-584 — ARIA knob value bubble during drag**
- Reach: Click and hold on any ARIA knob and drag up or down without releasing.
- Visible: A popup value display above the knob showing the INTEGER CC value 0-127, with the knob's filmstrip frame changing under the cursor.
- Why separate: 0-127 integers rather than the engine editors' own units.
- Variant of: SHOT-465

**SHOT-585 — ARIA OptionMenu open**
- Reach: On the Rusty Full Main page, left-click one of the 6 dropdowns (e.g. Snare Type or Stir Type).
- Visible: The kit author's OptionItem list (e.g. Sticks / Brushes / Mallets / Tom-on-snare), current selection ticked.
- Why separate: The closed dropdown shows only one word; the choice set is only visible open, and it changes which articulations the kit plays.

**SHOT-586 — Rusty Drum Kit tab, no program loaded (overlay state)**
- Reach: Add a BaySickRustyDrums tab but do NOT pick a program. Window Menu > "Drum Kit".
- Visible: A full-bleed kit photo (big_rusty_drums.png, 2000x1200, 5:3 letterboxed and centred) rendered at **50% opacity**, with side bands filled by a 90-degree-rotated copy of the kit's ARIA control-panel art (left band CCW, right band CW) also at 50%. Centred over everything: a black 55%-alpha rounded rectangle (max 640x90) with white 24pt bold **"Pick a program to begin"**. Clicks on the kit are no-ops and the cursor is a normal arrow.
- Why separate: The dimmed + overlay state is a completely different picture from the live kit, and it is what a new user lands in.

**SHOT-587 — Rusty Drum Kit tab, Full program loaded**
- Reach: Program dropdown > "Full", then Menu > "Drum Kit".
- Visible: The kit photo at FULL opacity with the two rotated side bands. **No hitbox outlines are drawn.** The one always-drawn overlay is the hi-hat pedal — a rotated ellipse outline (2px) around the pedal at lower-left, GREEN when open, with white centred text "PEDAL: OPEN". Cursor is a pointing hand.
- Why separate: The live kit is the reference photo the whole drum-kit chapter is built on.
- Note: The side bands are decorative, not controls — they carry no state. Per-piece control lives on the ARIA Player tab's section pages.

**SHOT-588 — Rusty Drum Kit tab, Basic program (greyed-out pieces)**
- Reach: Program dropdown > "Basic" (confirm the switch prompt), then Menu > "Drum Kit".
- Visible: The same photo, but every piece whose channel is absent from Basic is covered by a filled 78%-alpha near-black ELLIPSE: Crash Sizzle 17 (bow/crash/bell), Stack (mid/edge), China 18, Tom 22, Ride Sizzle 19 (bow/edge/bell). Those regions read as dark blobs on the photo. The hi-hat pedal ring is still drawn.
- Why separate: **The most confusing visual in the drum area** — pieces are visibly blacked out with no caption explaining why. Without a labelled shot the reader cannot learn that Basic simply lacks those channels.
- Variant of: SHOT-587

**SHOT-589 — Rusty Drum Kit, hi-hat pedal CLOSED**
- Reach: On the loaded Drum Kit tab, click the hi-hat foot pedal at lower-left (the tilted ellipse near the stand base).
- Visible: The pedal ellipse turns RED (0xffe04040) and the centred label reads "PEDAL: CLOSED". Hi-hat Tip and Shaft hits now trigger the closed notes (42 / 54) instead of the open ones (46 / 58).
- Why separate: Two-state toggle, red vs green, and it silently changes what two OTHER pads sound like.
- Variant of: SHOT-587

**SHOT-590 — Rusty Drum Kit, piece pressed (blue hit ring)**
- Reach: On the loaded Drum Kit tab, press and HOLD the mouse on a piece (e.g. the snare or the ride bell) — the ring only shows while the button is down.
- Visible: A thin (1.5px) light-blue (0xff4dd2ff) rotated ellipse outline around the piece being struck, revealing that piece's hitbox shape. **Nothing is drawn on hover — only on press.**
- Why separate: The ONLY moment hitbox geometry is ever visible, and the only feedback that a click registered.

**SHOT-591 — Rusty Drum Kit, piece hover tooltip**
- Reach: On the loaded Drum Kit tab, hover (do not click) over a piece and hold still. Repeat for several pieces.
- Visible: A tooltip showing the articulation name only — "Ride 22 Bell", "Snare Sidestick", "Crash Sizzle 17 Bow", "Hi-hat Shaft", "Hi-hat Pedal".
- Why separate: **Since the hitboxes are invisible, the tooltip is the entire discovery mechanism for the kit graphic.** Worth repeating for several pieces.

**SHOT-592 — Rusty page Menu dropdown**
- Reach: Rusty window > click "Menu" on the title strip.
- Visible: "Drum Kit" (ticked when active), "Player" (ticked when active), "Piano Roll" (a redirect), the shared standard items (Freeze etc.), separator, "Save Page Preset As..." (greyed until a kit is loaded), "Load Page Preset" submenu.
- Why separate: **There is no sub-tab bar on this page — this menu IS the navigation.**

**SHOT-593 — Rusty Program dropdown open**
- Reach: Rusty window > click the Program ComboBox on the right of the title strip.
- Visible: Exactly two entries: "Full" and "Basic". The placeholder when nothing is selected is "Load Player".
- Why separate: A two-item list whose PLACEHOLDER wording matches neither item — exactly the kind of thing an atlas caption fixes.

**SHOT-594 — Rusty Player Preset menu, empty**
- Reach: Rusty window > click "Presets" on the title strip, with no presets saved.
- Visible: "Save Player Preset As..." (enabled only when an engine exists), separator, a section header "Load Player Preset", then a disabled greyed "(no presets saved)".
- Why separate: This is the SHIPPING state.

**SHOT-595 — Rusty Player Preset menu, populated**
- Reach: Save at least one player preset first, then click "Presets".
- Visible: The same menu with the saved names listed under "Load Player Preset" (read from `Documents/BaySickDAW/Presets/Rusty Player/My Presets`).
- Variant of: SHOT-594

**SHOT-596 — Save Player Preset dialog**
- Reach: Rusty window > "Presets" > "Save Player Preset As...".
- Visible: AlertWindow "Save Player Preset", message "Enter a name for this player preset:", text editor pre-filled "My Rusty Player", Save / Cancel.

**SHOT-597 — Switch Rusty Drums program confirm**
- Reach: With Full (or Basic) already loaded, pick the OTHER entry in the Program dropdown.
- Visible: Warning AlertWindow "Switch Rusty Drums program?" — "Switching will reset all mixer settings, clear the piano roll across every pattern, and reload the kit. Continue?" with "Yes, switch" / "Cancel".
- Why separate: A destructive gate the user meets the first time they try the other program.

**SHOT-598 — Load Player Preset cross-program confirm**
- Reach: Load a player preset saved on the OTHER program (save one on Full, switch to Basic, then load it).
- Visible: Warning AlertWindow "Load Player Preset?" naming both programs ("This preset was saved on 'Full' and will switch the player from 'Basic'...") plus the same mixer/piano-roll warning; buttons "Yes, switch + load" / "Cancel".
- Why separate: Different title and different button text from SHOT-597, and it is the one that explains why a preset can move you between programs.
- Variant of: SHOT-597

**SHOT-599 — Rusty Save Page Preset dialog**
- Reach: Rusty window > Menu > "Save Page Preset As..." (needs a loaded kit).
- Visible: AlertWindow "Save Page Preset", message "Enter a name for this Rusty Drums page preset:", text editor pre-filled "My Rusty Drums Setup", Save / Cancel.
- Why separate: **Page Preset (engine + every Rusty mixer strip + RustyDrums bus + racks) is a different thing from Player Preset (kit CCs only)** and the manual must distinguish them.

**SHOT-600 — Rusty Load Page Preset submenu**
- Reach: Rusty window > Menu > hover "Load Page Preset".
- Visible: Saved .xml page presets, or a disabled "(no page presets saved)" when the folder is empty.
- Why separate: The empty state is the shipping state and it looks like a broken menu.

---

## SITTING 23 — BaySickGuitars and BaySickBasses (sfizz Inst tabs)

**Reach:** Ribbon > Inst slot chevron > "+ Add BaySickGuitars" (or Basses). Both
**auto-load a default program on tab add** (01-green_keyswitch and
01-darkblack_keysw), so there is no normal empty ARIA panel for them.

**Why these have no section tab row:** the ARIA strip only exists when a kit
ships 03-08 zoom XMLs. Only Big Rusty Drums does. Guitars and Basses have one
GUI XML per program and no zoom pages, so the strip is suppressed entirely. Same
widget code, two very different-looking headers.

Both kits mostly share one layout across their 11 programs, differing by
background art with small control-cluster differences on the combo variants. I
list 3 guitar + 2 bass panels here rather than 22. **If Jeff wants every program
pictured, that is 22 shots instead of 5 — his call.**

**SHOT-601 — BaySickGuitars player, green program**
- Reach: Ribbon > Inst slot > "+ Add BaySickGuitars". The tab spawns and auto-loads 01-green_keyswitch.
- Visible: Title strip: "Menu" dropdown; centred navy (#1C3A8A) "BaySickGuitars"; right extras — the program-name label (blue text on 0xff1a1a1a showing the prettified name "Green Keyswitch", 133px) and the "Load Guitar" button (87px); plus the Swing Mix knob and FX Rack slot. Body: the full-bleed ARIA panel from 01-green_keyswitch.xml — control_tab_g.png background, **3 filmstrip knobs (Unison / Width / Detune), 9 vertical image faders** (Releases, Vibrato Guitar, Vibrato Violin, ...), 1 on/off image toggle, 15 static labels including the section heading "VIBRATO". **No section tab row.** Overlaid on the player's own top-left corner: the "CUT SELF" toggle (62px) and the cut-mode button reading "SAME PITCH" or "CUT ALL" (78px).
- Why separate: A different widget vocabulary from Rusty (faders and an on/off button instead of knobs and option menus), no section tab row, and the CUT SELF pair lives here and nowhere else.

**SHOT-602 — BaySickGuitars player, black program**
- Reach: On a BaySickGuitars tab, click "Load Guitar" and pick "Black Keyswitch".
- Visible: Same panel geometry but control_tab_b.png background art. The program label updates to "Black Keyswitch"; the ribbon tab and mixer strip rename to match.
- Why separate: Distinct artwork and colour scheme — the reader needs to know it is the same instrument in a different skin.
- Variant of: SHOT-601

**SHOT-603 — BaySickGuitars player, combo program**
- Reach: On a BaySickGuitars tab, click "Load Guitar" and pick "Combo Keyswitch".
- Visible: control_tab_c.png background, and a genuinely different control cluster from green/black — the first cluster loses one knob and the Unison/Width labels are stacked differently. Same fader bank and VIBRATO section.
- Why separate: Third artwork variant AND a different control count in the left cluster.
- Variant of: SHOT-601

**SHOT-604 — BaySickGuitars program picker menu**
- Reach: On a BaySickGuitars tab, click "Load Guitar" on the window title strip.
- Visible: All 11 programs found in the kit's Programs folder, names prettified from the filenames — Green Keyswitch, Black Keyswitch, Combo Keyswitch, Green Twang, Green Staccato, Green Hammer-On, Black Twang, Black Staccato, Black Behind The Bridge, Combo Twang, Combo Staccato — with the loaded one ticked.
- Why separate: An 11-entry menu that is the tab's ENTIRE sound-selection UI; the button label never shows the list.

**SHOT-605 — BaySickBasses player, dark black program**
- Reach: Ribbon > Inst slot > "+ Add BaySickBasses". Auto-loads 01-darkblack_keysw.
- Visible: Title strip with centred navy "BaySickBasses", program label "Darkblack Keysw" and a "Load Bass" button. Body: black_control_tab.png background with **13 filmstrip knobs, 4 vertical faders, 2 on/off toggles and 21 static labels** — noticeably knob-heavier than the Guitars panel. CUT SELF + cut-mode pair overlaid top-left. No section tab row.
- Why separate: Different engine, different artwork family, and a materially different control mix (13 knobs / 4 faders vs Guitars' 3 / 9).

**SHOT-606 — BaySickBasses player, baby blue program**
- Reach: On a BaySickBasses tab, click "Load Bass" and pick "Babyblue All".
- Visible: blue_control_tab.png background with blue-themed knob/fader filmstrips (blue_knob, blue_fader_vert).
- Why separate: A second artwork family within the same instrument.
- Variant of: SHOT-605

**SHOT-607 — BaySickBasses program picker menu**
- Reach: On a BaySickBasses tab, click "Load Bass".
- Visible: The 11 bass programs — Darkblack Keysw, Darkblack Keysw Warm, Babyblue All, Babyblue Warm, Darkblack Pluck, Darkblack Pluck Warm, Darkblack Ghost, Darkblack Ghost Warm, Darkblack Stac, Darkblack Btb, Darkblack Btb Open — current one ticked.
- Why separate: A different list from the Guitars picker.
- Variant of: SHOT-604

**SHOT-608 — sfizz Inst tab, Menu dropdown**
- Reach: On a BaySickGuitars or BaySickBasses window, click "Menu" on the title strip.
- Visible: "Pedals", "NAM/IR", "Piano Roll", the appended standard items (Freeze etc.), then the page-actions entries built by the page (Lock, Save/Load Page Preset, Delete).
- Why separate: **The only route to the tab's Pedals and NAM/IR windows** — there are no visible buttons for them anywhere.

---

## SITTING 24 — Vox page and the Vocal Chain

**Reach:** Ribbon "+" > BaySickVocal creates a teal "Vox" slot. Click the slot to
open the BaySickVocals window. **There is no engine picker on a Vox tab** — it
always builds BaySickVocal.

**Window sizes for framing:** Vocal Chain 1047x723, BaySickPitch 1534x724,
BaySickAlign 1047x723, NAM/IR 843x563.

**The six chain panels themselves are NOT re-shot here** — Gate, De-reverb,
De-esser, Compressor, Saturation and Limiter are SHOT-284..SHOT-307 in Sitting
13. Manual 1's Vox chapter footnotes those. What IS shot here is the chain's own
chrome, which exists nowhere else.

**SHOT-609 — BaySickVocals window, full, idle**
- Reach: Ribbon > click the Vox tab.
- Visible: Title strip — "Menu" heading left, centred teal "BaySickVocals" logo, maximize and "x" right. Panel top half: "Mix" caption + rotary Mix knob (top-left), an A/B combo to its right. A horizontal divider across the middle. Bottom half: bold caption "REALTIME PITCH CORRECTION"; a "Realtime Pitch ON" toggle; "Root" label + 12-entry note combo; "Scale" label + 13-entry scale combo; four rotaries captioned Retune ms / Strength / Humanize / Throat; a "Formant Preserve" toggle; and a monospace live readout pinned at the bottom reading "Detected: -- Hz   Target: -- Hz   Shift: -- cents".
- Note for the manual: **that readout is currently hardcoded and never shows real numbers.** Photograph it as-is and describe it as a placeholder rather than promising live values.

**SHOT-610 — BaySickVocals window, "Realtime Pitch OFF" state**
- Reach: Click the "Realtime Pitch ON" button once.
- Visible: The same panel with the button text now reading "Realtime Pitch OFF" and the toggle off.
- Why separate: The button RELABELS itself rather than just changing colour — two different-looking screens.
- Variant of: SHOT-609

**SHOT-611 — BaySickVocals window, record-gated**
- Reach: Right-click the Arm LED on this Vox strip in the Mixer and pick an input, left-click the LED to arm, then start the transport recording. Look at the BaySickVocals window while the take runs.
- Visible: The Realtime Pitch button, Root, Scale, all four knobs and their captions, Formant Preserve, and the A/B combo **all dimmed to 40% alpha and non-clickable; the Mix knob stays live at full brightness.** Tooltips on the Pitch button and A/B change to "Locked while recording - set the realtime sound before the take" / "Locked while recording - pick the A/B slot before the take".
- Why separate: A whole board of greyed controls with one still-live knob is the most confusing state on this page.
- Variant of: SHOT-609

**SHOT-612 — Vox window Menu dropdown**
- Reach: On any Vox-family window, click the "Menu" heading.
- Visible: Top block — "Vocal Chain", "BaySickPitch", "BaySickAlign", "NAM/IR" launcher rows. Sep. "Lock" (ticked when locked). Sep. "Rename...", "Duplicate Vox (new tab)". Sep. "Save Page Preset As...", "Load Page Preset" submenu. Sep. "Delete Vox" (disabled when locked). Sep. "FX Rack", then "Freeze".
- Why separate: **All five vocal windows share this same Menu** — it is the only route to the four satellite windows.

**SHOT-613 — Vox Menu, Load Page Preset submenu, empty**
- Reach: Vox window > Menu > hover "Load Page Preset" on a fresh install.
- Visible: A single disabled row "(no page presets saved)".

**SHOT-614 — Vox Menu, Load Page Preset submenu, populated**
- Reach: Save a page preset first, then Menu > hover "Load Page Preset".
- Visible: Nested folder submenus mirroring the `Presets/Vox Page` tree, with preset filenames (extension stripped) as leaf rows.
- Why separate: Folder-nesting behaviour is invisible in the empty state.
- Variant of: SHOT-613

**SHOT-615 — Vox Menu, Freeze row locked (vocal-specific tooltip)**
- Reach: Vox window > Menu, hover the greyed Freeze row at the bottom. Default state on a fresh install.
- Visible: A greyed "Freeze" row with a custom tooltip component carrying the unlock path **plus the long vocal-specific warning**: "On a vocal this prints the WHOLE chain - pitch, alignment, gate, de-reverb, de-esser, compressor, saturation, limiter and amp...".
- Why separate: SHOT-080 covers the four Freeze states generically; this tooltip's extra warning appears only on a vocal window and appears nowhere else in the app.
- Variant of: SHOT-080

**SHOT-616 — Save Page Preset dialog (Vox)**
- Reach: Vox window > Menu > "Save Page Preset As...".
- Visible: AlertWindow "Save Page Preset", message "Enter a name for this vox page preset:", a text editor pre-filled "My Vox", Save / Cancel.

**SHOT-617 — Delete Vox confirm, clean page**
- Reach: Vox window > Menu > "Delete Vox" on a tab whose settings have not been touched since the last save/load.
- Visible: WARNING-icon AlertWindow "Delete Vox"; body explaining the tab removes its Player, Mixer Strip, Effects Rack, Piano Roll and the audio-library entries for every recording on the tab, and that **the WAVs in the project's Samples folder stay on disk**; "Delete" and "Cancel".

**SHOT-618 — Delete Vox confirm, dirty page**
- Reach: Move any knob on the Vox page, then Menu > "Delete Vox".
- Visible: QUESTION-icon AlertWindow "Delete Vox" with the same body plus an extra paragraph about "Save Page Preset & Delete"; THREE buttons: "Save Page Preset & Delete", "Delete", "Cancel".
- Why separate: Different icon and a third button — genuinely a different screen.
- Variant of: SHOT-617

**SHOT-619 — Vocal Chain window, all six slots**
- Reach: Vox window > Menu > "Vocal Chain". Opens at 1047x723.
- Visible: Title strip ("Menu" heading, a plain centred title "<tab name> - Vocal Chain", maximize + close). Six stacked slot rows in signal order: 1 Gate, 2 De-reverb, 3 De-esser, 4 Compressor, 5 Saturation, 6 Limiter. Each row is a 28px header strip over the effect's own panel.
- Why separate: The whole-chain overview; a reader needs the order and count before any single stage.

**SHOT-620 — Vocal Chain slot header, loaded, close-up**
- Reach: Zoom on any slot header in the Vocal Chain window (slot 4, Compressor, is the busiest).
- Visible: Left to right — a round bypass LED (green = active, red = bypassed); the effect name in bold; then the right-hand cluster: a Basic/Advanced button (label reads the CURRENT state), a "Preset" button, a "Mode" button (label shows the current mode), a "SC: Off" button (Compressor and Limiter only), then up-triangle, down-triangle and red-x glyphs.
- Why separate: **This inline header exists nowhere else in the app** (rack effects put these controls in a window Menu instead). Two honest callouts belong on this figure: the up/down/x glyphs are PAINTED BUT INERT here (the chain's slots are locked), and the "SC" button opens NOTHING because the chain never installs a channel context. Both look clickable and both do nothing.
- Note: Gate and De-reverb declare no advanced controls, so **those two slots show no Basic/Advanced button at all** — worth catching when comparing headers.

**SHOT-621 — Vocal Chain slot header, bypassed**
- Reach: Click the round LED at the left of any slot header.
- Visible: The same header with the LED rendered red instead of green.
- Why separate: Bypass is the ONE header control that actually works here; the two LED colours are the whole affordance.
- Variant of: SHOT-620

---

## SITTING 25 — BaySickPitch

**SETUP:** Put audio clips on this Vox channel in the Builder grid first, and
let one analysis finish. **Transient-badge trick:** the auto re-analyze poller
runs at 4 Hz and fires ~1 second after the clip signature settles, and only when
STOPPED. To hold a badge on screen, make the clip change while PLAYING — the
pending badge then persists until you stop.

**SHOT-622 — BaySickPitch window, analyzed and populated**
- Reach: Vox window > Menu > "BaySickPitch". Opens at 1534x724.
- Visible: Title strip with a teal "BaySickPitch" logo centred; a two-row toolbar; a piano keyboard column down the left; a canvas with a bar/beat ruler across the top, black/white note lanes, C-octave lines, the GREEN as-sung pitch curve, PURPLE note PILLS carrying a teal waveform interior and in-pill note-name + cents readout, GREY slice pills, small dirty dots on edited pills, and the playhead line; a vertical scrollbar right; a horizontal scrollbar bottom; a single-line info bar pinned at the very bottom.

**SHOT-623 — BaySickPitch toolbar, close-up**
- Reach: Zoom on the top ~60px of the BaySickPitch window.
- Visible: Row 1 — (empty logo slot), "Save", "Load", a "Slice" / "Edit" radio pair, "Send Notes to..."; right-aligned: Root combo, Scale combo, "Snap" toggle. Row 2 — a monospace LENGTH readout left, an "ON" chain toggle; right-aligned: the pitch-engine combo ("Rubber Band - Balanced" / "Signalsmith - Lightest (Low CPU)" / "WORLD - Highest Quality (High CPU)"), "Snapshot", "Versions", "Reset", "Render", "Undo", "Redo", an "A" auto-scroll toggle. Far right of both rows: four labelled rotaries Focus / Mod / Speed / Throat with value text boxes.
- Why separate: Twenty-plus controls at real size; unreadable in the full-window shot.

**SHOT-624 — BaySickPitch canvas, empty (no clips)**
- Reach: Open BaySickPitch on a Vox tab with no audio clips on its channel.
- Visible: Ruler and note lanes only, with centred grey text "Put audio clips on this channel - notes appear here automatically".
- Why separate: The first thing a new user sees; no pills at all.

**SHOT-625 — BaySickPitch canvas, "Analyzing..."**
- Reach: Drop a long clip on the Vox channel and open the window right after the drop to catch the analysis.
- Visible: An empty canvas with centred text "Analyzing...", plus the toolbar's amber analysis badge.
- Variant of: SHOT-624

**SHOT-626 — BaySickPitch canvas, analysis deferred**
- Reach: Change the clip on the Vox channel while the transport is PLAYING, with the pitch window open.
- Visible: Centred text "Analysis deferred until stop - stop playback to refresh the notes".
- Why separate: A distinct message that explains why nothing happens during playback.
- Variant of: SHOT-624

**SHOT-627 — BaySickPitch toolbar, RE-ANALYZE badge**
- Reach: Analyze a channel, then move or trim its clip on the Builder grid while STOPPED; look at the toolbar before the ~1s auto re-analyze fires.
- Visible: Bold amber text "RE-ANALYZE" in the toolbar, left of the Root combo.
- Why separate: **A status readout, not a control** — a reader will look for a button.

**SHOT-628 — BaySickPitch toolbar, RE-ANALYZE ON STOP badge**
- Reach: The same clip change but with the transport PLAYING.
- Visible: Bold amber text "RE-ANALYZE ON STOP".
- Why separate: Second badge string; the pending-vs-now distinction is the whole message.
- Variant of: SHOT-627

**SHOT-629 — BaySickPitch, selected pill with handles and sub-edit display box**
- Reach: In the canvas (Edit mode, zoomed enough that the lane height is comfortable), click a SINGLE pill.
- Visible: The pill outlined white; slim corner handles on the pill body; below it the display-only sub-edit box drawn with "EDIT" and "RESET" hit areas and four labelled bar readouts VIB / FRM / VOL / PIT.
- Why separate: **The box is drawn, not real components**, and it is the launcher for the sub-editor — a reader will never guess EDIT/RESET are clickable.

**SHOT-630 — BaySickPitch, multi-selection and marquee**
- Reach: Drag a rubber-band box across several pills on empty canvas, or Ctrl+A.
- Visible: A translucent white marquee rectangle while dragging; several pills outlined white afterwards; **the display box is ABSENT** (it only shows for a single selection).
- Why separate: The display box disappearing on multi-select is a state a reader will think is a bug.
- Variant of: SHOT-629

**SHOT-631 — BaySickPitch, slice pills**
- Reach: Click "Slice" in the toolbar, then click inside a pill to split it.
- Visible: GREY slice pills in place of the purple note pill, each with a permanent black outline so abutting same-pitch slices stay visible; slice pills carry **no waveform interior and no note readout.**
- Why separate: A visually different pill class with its own fill colour and no interior.

**SHOT-632 — BaySickPitch, detached pill marker**
- Reach: Ctrl+Shift+drag a pill horizontally (detach move), or Ctrl+drag an edge (detach stretch).
- Visible: The pill gains a SECOND inner outline in the detach colour.
- Why separate: A one-pixel visual distinction that carries real meaning.
- Variant of: SHOT-622

**SHOT-633 — BaySickPitch, pill right-click menu, single pill**
- Reach: Right-click one selected pill in the canvas.
- Visible: "Restore to Original State"; "Snap to Semitone"; "Force to Scale" (disabled on Chromatic); "Merge With Next Pill"; "Open Sub-Editor..."; "Include in Pitch Correction" / "Exclude from Pitch Correction".

**SHOT-634 — BaySickPitch, pill right-click menu, multi-select**
- Reach: Select several pills, then right-click one of them.
- Visible: PLURALIZED labels: "Restore Selected to Original State", "Snap Selected to Semitone", "Force Selected to Scale", "Merge Selected Pills", "Zoom to Selection".
- Why separate: Different row set AND different wording from the single-pill menu.
- Variant of: SHOT-633

**SHOT-635 — BaySickPitch, "Send Notes to..." menu, populated**
- Reach: BaySickPitch toolbar > "Send Notes to..." with at least one Layers / Bass / Drums / Clips tab open.
- Visible: Each candidate target tab listed by name.
- Why separate: A cross-page action unique to this editor.

**SHOT-636 — BaySickPitch, "Send Notes to..." menu, empty**
- Reach: The same button with no Layers / Bass / Drums / Clips tabs in the project.
- Visible: A single disabled row "(no Layers / Bass / Drums / Clips tabs open)".
- Variant of: SHOT-635

**SHOT-637 — BaySickPitch, Versions menu, populated**
- Reach: Analyze at least once (or click "Snapshot"), then click "Versions".
- Visible: Newest-first rows "Revert to v<N>  <date time>", with "  (grid changed)" appended to entries whose grid signature no longer matches.

**SHOT-638 — BaySickPitch, Versions menu, empty**
- Reach: Click "Versions" on a never-analyzed channel.
- Visible: A single disabled row "No versions yet - analyze or Snapshot creates one".
- Variant of: SHOT-637

**SHOT-639 — BaySickPitch, Versions button gated during playback**
- Reach: Start the transport and hover the "Versions" button.
- Visible: The button greyed with the tooltip swapped to "Stop playback to revert".
- Why separate: A playback-gated control whose only explanation is a swapped tooltip.
- Variant of: SHOT-623

**SHOT-640 — BaySickPitch, Render dialog**
- Reach: BaySickPitch toolbar > "Render".
- Visible: AlertWindow "Render", body "Bake the edited channel to Pitched/{name}_pitch_v{N}.wav (file only - playback is already live).", buttons "Render" and "Cancel".
- Why separate: Also the place the export path is spelled out.

**SHOT-641 — BaySickPitch, Render success alert**
- Reach: Complete a Render on a channel with audio.
- Visible: Info-icon box "Render" reading "Baked to <filename>".
- Variant of: SHOT-640

**SHOT-642 — BaySickPitch, "WORLD works offline" notice**
- Reach: BaySickPitch toolbar > pitch-engine combo > "WORLD - Highest Quality (High CPU)". **Appears only until "Do not show this again" is ticked — capture it first time.**
- Visible: Info-icon AlertWindow "WORLD works offline" explaining WORLD processes offline and to use Rubber Band or Signalsmith for instant edits; a "Do not show this again" checkbox; OK.
- Why separate: A one-time educational dialog carrying real information the manual must reproduce.

**SHOT-643 — BaySickPitch, "Reset Sub-Edits" confirm**
- Reach: Select two or more pills, then press Delete or use the display box's RESET. **Also suppressible — capture first time.**
- Visible: Warning-icon AlertWindow "Reset Sub-Edits" naming the count of selected pills; a "Do not show this again" checkbox; "Reset" and "Cancel".

**SHOT-644 — BaySickPitch, Save Pitch Preset dialog**
- Reach: BaySickPitch toolbar > "Save".
- Visible: AlertWindow "Save Pitch Preset" with a name text editor and Save/Cancel.

**SHOT-645 — BaySickPitch, Load preset menu (populated and empty)**
- Reach: BaySickPitch toolbar > "Load".
- Visible: Saved preset filenames, or a single disabled row "(no presets saved)". Capture both forms.

**SHOT-646 — BaySickPitch sub-editor window, Volume lane**
- Reach: Double-click a pill in the canvas, or select a pill and click EDIT in its display box. A separate floating window opens (title-bar colour 0xff1a1c1e, resizable, closed by x or Esc).
- Visible: Top row — bipolar "Vib" and "Frm" knobs, a "Variation" knob, with drawn captions above each; a "Vol" / "Pitch" radio pair and a "Play" button. Main lane spanning the pill length with the pill waveform ghosted behind, editable points and straight-line segments, and left-edge scale text "x2" (top) / "x0" (bottom). Right column — a "PILLS" browser listing the pills in order with a trailing " *" on any that carry edits.
- Why separate: A separate WINDOW with its own gesture set; the volume lane is its default view.

**SHOT-647 — BaySickPitch sub-editor window, Pitch lane**
- Reach: In the open sub-editor, click the "Pitch" button.
- Visible: The same layout but the lane's edge scale reads "+12 st" (top) / "-12 st" (bottom) and the curve is additive semitones around zero.
- Why separate: The lane's meaning and scale labels change completely.
- Variant of: SHOT-646

---

## SITTING 26 — BaySickAlign

**SETUP:** Clips on BOTH the leader and follower channels, and one completed
Analyze/Apply. Same badge-catching trick as Sitting 25.

**SHOT-648 — BaySickAlign window, analyzed**
- Reach: Vox window > Menu > "BaySickAlign". Opens at 1047x723.
- Visible: Title strip with a teal "BaySickAlign" logo; a toolbar across the top; a shared time ruler (m:ss). Then top to bottom: LEADER lane (green waveform, header column with a channel-picker combo); SYNC POINTS strip; FOLLOWER lane (teal waveform, header shows a static "(this page)"); PROTECTED strip; OUTPUT lane (red waveform). Bottom bar: Wave / Pitch / Energy buttons and "+" / "-" zoom buttons. Right panel with ALIGN and PITCH boxes.
- Why separate: A five-lane stack nothing else in the app resembles.

**SHOT-649 — BaySickAlign window, empty / never analyzed**
- Reach: Open BaySickAlign with no clips on the channels and no analysis run.
- Visible: THREE different empty-state strings at once — Leader and Follower lanes read centred grey "No clips on this channel yet"; the Output lane reads "Analyze to preview the aligned output"; the Leader combo shows its placeholder "Pick Leader...". Sync and protected strips empty.
- Variant of: SHOT-648

**SHOT-650 — BaySickAlign toolbar, close-up**
- Reach: Zoom on the top strip of the BaySickAlign window.
- Visible: Left — (empty logo slot); a preset combo listing Loose-Align / Loose-Align+Pitch / Close-Align / Close-Align+Pitch / Tight-Align / Tight-Align+Pitch / (User); a GREEN DIRTY DOT immediately right of the combo when values diverge from the preset; "Save"; "Load". Right — "Analyze/Apply", "Versions", "Render", "Undo", "Redo". An amber badge slot left of Analyze/Apply.

**SHOT-651 — BaySickAlign toolbar, playback-gated**
- Reach: Analyze once, then start the transport and look at the toolbar.
- Visible: "Analyze/Apply", "Versions", "Undo" and "Redo" ALL greyed; tooltips change to "Stop playback to re-analyze" / "...to revert" / "...to undo" / "...to redo".
- Why separate: Four buttons dead at once — the reader needs to know it is deliberate.
- Variant of: SHOT-650

**SHOT-652 — BaySickAlign toolbar, RE-ANALYZE / ANALYZING badges**
- Reach: Change a clip on either channel after an analysis — stopped gives "RE-ANALYZE", playing gives "RE-ANALYZE ON STOP"; "ANALYZING..." shows during the run.
- Visible: Bold amber text in the badge slot immediately left of the Analyze/Apply button.
- Why separate: Three different badge strings in one slot.
- Variant of: SHOT-650

**SHOT-653 — BaySickAlign, Leader channel picker open**
- Reach: Click the combo in the LEADER lane's header column (bottom-left of that lane).
- Visible: Every candidate audio channel by display name; placeholder "Pick Leader..." when nothing is chosen.
- Why separate: **The only channel-selection control in this editor** — the Follower has none; its header just reads "(this page)".

**SHOT-654 — BaySickAlign, Sync Points strip with points**
- Reach: Click inside the SYNC POINTS strip to place a point, or right-click it > "Automatic Sync Points" after an analysis.
- Visible: A strip labelled "SYNC POINTS" in its header column, with paired handles connecting a follower time to a leader time; handles draggable on either side.
- Why separate: A control strip with no buttons — purely gestural, so a labelled picture is the only way to explain it.

**SHOT-655 — BaySickAlign, Sync Points right-click menu**
- Reach: Right-click inside the SYNC POINTS strip — once on a point, once on empty strip space.
- Visible: "Delete Sync Point" (only when the click hit a point) and "Automatic Sync Points".
- Why separate: The row set changes depending on whether a point was hit.

**SHOT-656 — BaySickAlign, "Automatic Sync Points" needs-analysis alert**
- Reach: Right-click the SYNC POINTS strip > "Automatic Sync Points" BEFORE running Analyze/Apply.
- Visible: Info-icon box "Automatic Sync Points" — "Analyze first - automatic sync points seed from the analysis pairing."
- Why separate: A deliberately summonable guidance dialog, not an error.

**SHOT-657 — BaySickAlign, Protected Areas strip with regions**
- Reach: Drag horizontally inside the PROTECTED strip to create a region.
- Visible: A strip labelled "PROTECTED"; each region drawn as a block with a small text tag showing which protection dimensions are on.
- Why separate: A second gestural strip, visually distinct from the sync strip.

**SHOT-658 — BaySickAlign, Protected Areas right-click menu**
- Reach: Right-click a region in the PROTECTED strip.
- Visible: "Protect Timing" (tickable), "Protect Pitch" (tickable), "Delete Protected Area".
- Why separate: Two independent tick states drive the region's tag text.

**SHOT-659 — BaySickAlign, Pitch view mode**
- Reach: Bottom bar > "Pitch".
- Visible: All three lanes redraw as F0 CONTOUR LINES (60-1200 Hz mapped logarithmically over the lane height) in each lane's role colour instead of waveforms.
- Why separate: The lane stack looks nothing like the Wave view.
- Variant of: SHOT-648

**SHOT-660 — BaySickAlign, Energy view mode**
- Reach: Bottom bar > "Energy".
- Visible: All three lanes redraw as bottom-anchored RMS BARS in the role colour.
- Why separate: Third distinct lane rendering.
- Variant of: SHOT-648

**SHOT-661 — BaySickAlign, right panel close-up**
- Reach: Zoom on the right-hand column of the BaySickAlign window.
- Visible: ALIGN box — heading "ALIGN", an "ON" toggle, a Mode combo (Loose / Close / Tight), two rotaries "Fine Tune" (value text in ms, range follows the Mode) and "Max Shift" (value in ms, or **"No Limit" at full right**). PITCH box — heading "PITCH", an "ON" toggle, an algorithm combo (Rubber Band - Balanced / Signalsmith - Lightest (Low CPU) / WORLD - Highest Quality (High CPU)), "Leader Type" and "Follower Type" band combos (Normal / High Vocal / Low Vocal / High Instrument / Low Instrument), rotaries "Blend" and "Variation", rotaries "Transpose" and "Formant Shift", and a "Formant" toggle.
- Why separate: Sixteen controls in a narrow column, illegible at full-window scale.

**SHOT-662 — BaySickAlign, Versions menu, populated**
- Reach: Run Analyze/Apply at least once, then click "Versions".
- Visible: Newest-first "Revert to v<N>  <date time>" rows with "(grid changed)" markers where the grid has moved.
- Why separate: Align keeps its OWN version list, separate from the pitch editor's.

**SHOT-663 — BaySickAlign, Versions menu, empty**
- Reach: Click "Versions" before any Analyze/Apply.
- Visible: A single disabled row "No versions yet - Analyze/Apply creates one".
- Why separate: A different empty string from the pitch editor's.
- Variant of: SHOT-662

**SHOT-664 — BaySickAlign, Render dialog (three buttons)**
- Reach: BaySickAlign toolbar > "Render".
- Visible: AlertWindow "Render", body "Export the aligned Follower to Aligned/{name}_align_v{N}.wav (file only - playback is already live).", THREE buttons: "Standard", "High Resolution (slower)", "Cancel".
- Why separate: Three buttons where the pitch editor's Render has two — the High Resolution choice exists here only.

**SHOT-665 — BaySickAlign, Save / Load preset surfaces**
- Reach: BaySickAlign toolbar > "Save" (dialog), then "Load" (menu).
- Visible: "Save Align Preset" AlertWindow with a name editor and Save/Cancel; a Load popup listing saved preset names or a disabled "(no presets saved)".
- Why separate: Align presets are a separate library from pitch presets.

---

## SITTING 27 — BaySickNAM/IR

**Reach:** Vox window > Menu > "NAM/IR", or Inst tab > Menu > "NAM/IR". Window
843x563; the panel itself is 760x560. **The whole panel is a file drop target**
— .nam and .wav both work anywhere on it.

**SHOT-666 — BaySickNAM/IR window, nothing loaded**
- Reach: Vox window > Menu > "NAM/IR".
- Visible: An internal title bar (nameless — the red "BaySickNAM/IR" logo sits on the window strip). AMP row: "AMP" section label, "Load .nam..." button, an amber LCD-style label reading "(no model loaded)", an OFF/ON bypass switch at the right. CAB row: "CAB" label, "Load .wav IR..." button, a green LCD reading "(no IR loaded)", an OFF/ON bypass switch. Knob row of seven: Input Gain / Gate Thr / Gate Rel / Low Cut / High Cut / Cab Mix / Output, then the "OS" chicken-head (1 / 2 / 4) with the A and B slot buttons stacked below it. Status row: an empty hint label (left) and an empty error label (right). Then the Mic Sim and Mic Placement columns with a painted vertical divider between A and B.

**SHOT-667 — BaySickNAM/IR, model and IR loaded**
- Reach: Click "Load .nam..." and pick a .nam capture, then "Load .wav IR..." and pick a cabinet IR (or drag both files onto the window).
- Visible: The amber LCD showing the .nam filename; the green LCD showing the .wav filename.
- Why separate: The LCDs are the main readouts on this panel and the loaded form is what the manual describes.
- Variant of: SHOT-666

**SHOT-668 — BaySickNAM/IR, full-rig hint**
- Reach: Load a .nam capture that includes a cabinet (a full-rig capture).
- Visible: Orange hint text in the status row's LEFT half: "Full-rig model - cabinet already included.  Consider bypassing the IR."
- Why separate: A conditional status line that only appears for one class of file.
- Variant of: SHOT-667

**SHOT-669 — BaySickNAM/IR, A/B slot buttons (A active vs B active)**
- Reach: Click the "B" button under the OS dial, then "A" to swap back. Capture both.
- Visible: Two connected 28x20 buttons under the OS dial, the active one toggled. Switching swaps **BOTH LCDs, both mic-sim user-IR labels, and every manual-sync selector position** to the other slot's state.
- Why separate: One click changes readouts across the whole panel — the two states must be shown side by side.
- Variant of: SHOT-667

**SHOT-670 — BaySickNAM/IR, recent .nam menu**
- Reach: **RIGHT-click** the "Load .nam..." button.
- Visible: Up to 10 recent .nam filenames plus a "Clear recent" row; a single disabled "(no recent .nam files)" row when empty. Capture both forms.
- Why separate: A right-click-only surface the reader would never discover.

**SHOT-671 — BaySickNAM/IR, recent IR menu**
- Reach: RIGHT-click the "Load .wav IR..." button.
- Visible: The same shape — up to 10 recent IR filenames, "Clear recent", or a disabled "(no recent IRs)".
- Variant of: SHOT-670

**SHOT-672 — BaySickNAM/IR, Mic A column OFF (default)**
- Reach: Look at the left half of the mic area with the "Mic A Active" switch in its up/OFF position — the default.
- Visible: Section labels "MIC SIM A" and "MIC PLACEMENT A", the Mode dropdown, model combo, IR button/label, Mix knob, polar selector, Distance / Angle / Height / Mix knobs AND the mic picture — **ALL dimmed to 40% alpha and disabled**; the polar chicken-head drawn locked (it still shows tooltips). The OFF/ON "Mic A Active" switch sits in the MIC SIM A heading band, 60px in from the column's right edge.
- Why separate: The panel's DEFAULT state is both mics off, so this is what the reader opens to.

**SHOT-673 — BaySickNAM/IR, Mic Sim A, mode Built-in**
- Reach: Switch Mic A Active ON, then set the MIC SIM A "Mode" dropdown to "Built-in" (the default).
- Visible: A "Model" caption + the 10-entry model dropdown appear in the slot; the Load Mic IR button and path label stay hidden; Mix knob at the right. The Mode dropdown has TWO entries only — Built-in and User IR; the mic being off is the switch's job, not a Mode.
- Variant of: SHOT-672

**SHOT-674 — BaySickNAM/IR, Mic Sim A, mode User IR**
- Reach: Set the MIC SIM A "Mode" dropdown to "User IR".
- Visible: The Model dropdown is REPLACED IN THE SAME RECTANGLE by a green LCD-style path label reading "(no IR loaded)" or a filename, with a "Load Mic IR..." button beneath it (right-clicking that button clears the IR).
- Why separate: **The combo and the label occupy the SAME rectangle** — comparing the two shots is the only way to see that.
- Variant of: SHOT-672

**SHOT-675 — BaySickNAM/IR, built-in mic model dropdown open**
- Reach: MIC SIM A in Built-in mode > click the "Model" dropdown.
- Visible: Ten rows — Live Vocal Dynamic, Broadcast Dynamic, Workhorse Cardioid, Vintage LDC '87, Modern LDC, Multi-Pattern LDC, Tube LDC, Pencil SDC, Ribbon, Kick Drum. The combo's tooltip carries "Built-in mic archetype: <name>. Typical use: <use>" for the current pick.
- Why separate: **The ten model names appear nowhere else in the UI.**

**SHOT-676 — BaySickNAM/IR, Mic Placement A column**
- Reach: Look at the MIC PLACEMENT A section below MIC SIM A.
- Visible: Section label "MIC PLACEMENT A"; a "Top"/"Side" view button on the heading row; a "Polar" caption + 5-position chicken-head (O / Card / Sup / Hyp / 8 = Omni / Cardioid / Supercardioid / Hypercardioid / Figure-8); four knobs Distance / Angle / Height / Mix; and below them the draggable mic picture, showing distance rings, the bright on-axis zone and the red proximity disc, with the readout "30 cm   0 deg" at its bottom-right and "TOP" at its bottom-left.
- Why separate: Its own control group with a five-way selector whose bezel marks are abbreviations, plus the only draggable spatial control in the engine.

**SHOT-676b — BaySickNAM/IR, mic picture in SIDE view**
- Reach: Click the "Top" button on the MIC PLACEMENT A heading so it reads "Side".
- Visible: The picture re-centres on the speaker face — cab drawn face-on with the cone in the middle, 10 cm height rings around it, and the readout gaining a third field ("30 cm   0 deg   0 cm H"), "SIDE" at the bottom-left. Dragging now sets Height and Angle; Distance stays on its knob. Each mic has its OWN view button, so A can be in Side while B is in Top.
- Variant of: SHOT-676

**SHOT-677 — BaySickNAM/IR, Mic B column OFF (default)**
- Reach: Look at the right half of the mic area with the "Mic B Active" switch in its up/OFF position — the default.
- Visible: MIC SIM B and MIC PLACEMENT B section labels, Mode dropdown, model combo, IR button/label, Mix knob, polar selector, Distance / Angle / Height / Mix knobs and the mic picture — **ALL dimmed to 40% alpha and disabled**; the polar chicken-head drawn locked (it still shows tooltips). The OFF/ON "Mic B Active" switch sits in the MIC SIM B heading band, 60px in from the column's right edge — the SAME offset as Mic A's.
- Why separate: An entire dimmed half-panel; a beginner will read it as broken.

**SHOT-678 — BaySickNAM/IR, Mic B column ON**
- Reach: Click the "Mic B Active" switch down to ON.
- Visible: The whole Mic B column at full brightness and interactive, mirroring the Mic A control set. **Its output SUMS with Mic A rather than crossfading.**
- Variant of: SHOT-677

**SHOT-679 — BaySickNAM/IR, OS chicken-head close-up**
- Reach: Zoom on the 66x66 dial at the right end of the knob row.
- Visible: A three-position chicken-head with bezel marks 1 / 2 / 4, an "OS" caption below it, and the A/B slot button pair below that.
- Why separate: A dial with numeric bezel marks whose meaning (oversampling factor) lives only in the tooltip.

**SHOT-680 — BaySickNAM/IR, file chooser**
- Reach: Click "Load .nam..." (also worth one crop each for "Load .wav IR..." and "Load Mic IR...").
- Visible: A native Windows open-file dialog opened at `Presets/BaySickNAMIR/NAM` (or /IR, or /MIC IR) with the matching file filter.
- Why separate: Shows the default folder layout the app ships; the three variants differ only by start folder and filter.

---

## SITTING 28 — Hosted VST3 plugins

**SETUP:** At least one VST3 effect and one VST3 instrument added via
Options > Plugins.

**SHOT-681 — Hosted VST3 plugin in an effect window**
- Reach: Options > Plugins, add a VST3 effect. Then Effects > slot chevron > VST Plugins > pick it > click the name plate.
- Visible: The plugin's OWN editor UI filling the window, our title strip above it (strip name + plugin name), the Menu button and the bypass LED. **The window sizes itself to the plugin's declared editor size.**
- Also capture, same window, using a FIXED-SIZE plugin: (a) the window dragged LARGER, so the plugin sits centered inside a flat 0xff1c1c1e surround; (b) the window dragged SMALLER, so the plugin anchors top-left and the overflow is clipped at the window edge with NO scrollbars. Nothing scales in either shot - that is the point of capturing both.

**SHOT-682 — Hosted plugin Menu extras**
- Reach: With a hosted plugin's effect window open, click Menu.
- Visible: The normal Presets row PLUS an "Automate" submenu — containing a "Last Touched: <param>" row (or "Last Touched (move a control in the plugin first)" when nothing has been touched), then the parameter list chunked into "1 - 30", "31 - 60" submenus when long, or "(no parameters reported)" — and a "Run bridged (separate process)" row with a tick.
- Why separate: Two menu items that exist on no in-house effect.

**SHOT-683 — Plugin bridge confirmation box**
- Reach: Hosted plugin effect window > Menu > "Run bridged (separate process)".
- Visible: A native info message box "Plugin Bridge" reading "This takes effect the next time the plugin loads."

**SHOT-684 — Hosted VST3 instrument tab (Plugins page)**
- Reach: Ribbon "+" > VSTPlugin > pick an instrument. Click the resulting purple Plugins ribbon slot.
- Visible: The plugin's own editor UI hosted in a contained window with our title strip. Note for the manual: **hosted instrument tabs have no auditionNote**- Also capture: the same tab with a RESIZABLE plugin, mid-drag on the window edge - its UI re-lays out to follow the frame, which a fixed-size plugin never does. Fixed-size vs resizable is the whole sizing story for hosted plugins and the manual has to show both. If the plugin is running bridged, note that its UI does not follow the window at all: it is centered while it fits and clipped when it does not. — the piano-roll keyboard reaches them through the live-MIDI route, so clicking roll keys works but behaves slightly differently from the in-house engines.

---

## SITTING F — FAULT STATES

**These need something broken, missing or overloaded before they will appear.**
Almost every one needs a save, a file move on disk, and a relaunch — so do them
as one deliberate sitting rather than trying to catch them during normal capture.

**Recommended staging order:** build one "sacrificial" project containing a
sfizz Inst kit, a hosted VST3 effect, a hosted plugin in a rack slot, a NAM
capture + IR on a Vox tab, an audio clip on the Builder grid, a frozen tab, and
a couple of engine presets. Save it. Then close the app, move/rename/delete the
referenced files on disk in one pass, and relaunch — that single relaunch
surfaces SHOT-686, 687, 688, 689, 690, 693, 703 and 705 together.

**SHOT-685 — Perf readout, DSP overload flash**
- Reach: Load the engine past 95% DSP (stack effects and instruments until it flashes) and capture the readout.
- Visible: The DSP token flashing between bright red (0xffff2222) and dark red (0xff991111) at 10 Hz; the SYS token keeps its own independent colour.
- Why separate: **The app's only overload alarm**, and it cannot be staged without real load.
- Variant of: SHOT-007

**SHOT-686 — Ribbon slot, substituted-kit "(missing)" marker**
- Reach: Save a project with an sfizz Inst tab, move or delete the kit folder on disk, reopen the project. Crop the affected slot AND open that type's dropdown.
- Visible: The slot label gains a trailing " (missing)" suffix; in the instance dropdown the affected instance row shows the same suffix so the user can tell WHICH instance is broken (the slot can only ever mark the active one).
- Why separate: The app's only warning that a default kit was substituted.
- Variant of: SHOT-047

**SHOT-687 — File > Open Recent, "(missing)" entry**
- Reach: Open a project, close the app, rename or move that project folder, relaunch, then File > Open Recent. (Also capture File > New from Template with a deleted default template — that row reads "<name> - missing" and greys.)
- Visible: A recent entry rendered greyed/disabled with " (missing)" appended, alongside normal enabled entries.
- Why separate: Exactly the disambiguation a visual atlas exists for.
- Variant of: SHOT-023

**SHOT-688 — Grid audio clip, missing file (red)**
- Reach: Place an audio clip, close the app, move or rename the WAV on disk, reopen the project, go to Builder.
- Visible: The clip renders in DIM RED instead of its route colour, with no waveform.
- Why separate: A dead-file state a beginner will hit and must recognise.
- Variant of: SHOT-162

**SHOT-689 — Mixer Inst strip, missing kit (red name)**
- Reach: Save a project with an sfizz Inst kit loaded, remove/rename the kit files, reopen, go to Mixer.
- Visible: The strip NAME LABEL rendered in error red; hovering shows the tooltip "Saved kit is missing - playing the default kit instead".
- Why separate: A missing-content marker that shows as a COLOUR change only, never as label text.
- Variant of: SHOT-201

**SHOT-690 — Effects slot row, hosted plugin "(missing)"**
- Reach: Load a VST3 effect into a rack slot, save, move the plugin DLL, reopen the project, return to Effects.
- Visible: The same plugin name on the plate with " (missing)" appended; the effect window title carries it too.
- Why separate: Four characters of text are the only signal a hosted plugin is not actually processing.
- Variant of: SHOT-234

**SHOT-691 — Hosted plugin, "Retry Loading Plugin" menu row**
- Reach: With a dead hosted plugin loaded, open its effect window > Menu.
- Visible: The Menu gains a "Retry Loading Plugin" row that does not exist on a healthy plugin.
- Variant of: SHOT-682

**SHOT-692 — Hosted plugin, bridge row locked**
- Reach: Add a 32-bit VST3 effect, load it into a rack slot, open its window > Menu.
- Visible: The bridge row SHOWN BUT GREYED, reading "Run bridged (<reason>)" — never hidden.
- Why separate: A shown-but-disabled row with the reason embedded in its own text.
- Variant of: SHOT-682

**SHOT-693 — sfizz Inst tab, kit missing marker**
- Reach: Open a project whose saved sfizz program file is gone (or rename the kit folder), so a default kit is substituted at restore.
- Visible: The ribbon tab name AND the mixer strip name gain " (missing)". **The player panel renders the substituted program normally**, so the label is the only indication the tab is playing something other than its name.
- Why separate: A purely textual change on a page that otherwise looks healthy.
- Variant of: SHOT-601

**SHOT-694 — Rusty program file missing alert**
- Reach: Pick a program whose SFZ is absent from the Core Library (e.g. Big Rusty Drums not installed).
- Visible: Warning AlertWindow "Big Rusty Drums" with "Could not find program SFZ file:" plus the full path, OK. The Program dropdown reverts to the previously-loaded program.

**SHOT-695 — Rusty Drum Kit, kit graphic asset missing**
- Reach: Only appears if the embedded big_rusty_drums_png resource fails to load.
- Visible: A plain 0xff0d0d0d fill with grey 14pt centred text "Kit graphic unavailable".
- Why separate: A distinct failure screen with its own message.
- Variant of: SHOT-587

**SHOT-696 — ARIA panel, program XML absent (placeholder)**
- Reach: A kit whose Programs SFZ exists but whose matching GUI/<name>.xml does not.
- Visible: A dark 0xff1a1a1a rectangle with grey 16pt "Loading control surface..." filling the panel area, no artwork, no controls.
- Why separate: The same placeholder string as the Rusty pre-load state (SHOT-573) but reached from a different cause — worth one caption saying so.
- Variant of: SHOT-573

**SHOT-697 — Engine preset read failure alert (shared)**
- Reach: From BaySickSynth, BaySickBass, Harmless or BaySickPlayer, pick a preset whose XML file is corrupt or came from another engine.
- Visible: Warning-icon alert "Load Preset" reading "That preset file could not be read." with OK.
- Why separate: All four engines share this one alert; needs a damaged file to trigger.

**SHOT-698 — BaySickSynth visualizer, "No engine" placeholder**
- Reach: Not reachable by normal use — the scope only draws this when it has no parameter source attached.
- Visible: The scope panel with grid lines and centred dim grey text "No engine".
- Why separate: Documented so a reader who ever sees it knows it is not a normal state.
- Variant of: SHOT-467

**SHOT-699 — BaySickPlayer, "preset's sample is missing"**
- Reach: Load a preset that references a sample file not present on this machine.
- Visible: Warning alert "Load Preset" reading "This preset's sample is missing from this machine: <path>" plus "The preset's settings were applied but it will not make sound.", OK.

**SHOT-700 — Layers / Bass page with no engine**
- Reach: Only reachable by opening a project saved before engines became mandatory at add-time; there is no live route.
- Visible: An otherwise empty page with centred dim text "No engine loaded".
- Why separate: A legacy empty state the manual should name and explain rather than leave a reader guessing.

**SHOT-701 — BaySickPitch, analysis failed**
- Reach: Occurs when the analyzer returns an error (e.g. unreadable source audio).
- Visible: Centred reddish-grey text — the analyzer's error string, or the fallback "Analysis failed - check the channel has audio clips".
- Why separate: A different message class from the empty state.
- Variant of: SHOT-624

**SHOT-702 — BaySickPitch sub-editor, orphaned pill**
- Reach: The underlying region disappears (e.g. a re-analysis) while the sub-editor is open.
- Visible: The lane replaced by centred text "(pill gone - re-analyze?)".
- Variant of: SHOT-646

**SHOT-703 — BaySickNAM/IR, LCD "(missing)" state**
- Reach: Load a .nam and an IR, save the project, move or delete those files, reopen the project. **This one IS reproducible on demand.**
- Visible: The affected LCD's text turns RED and gains a " (missing)" suffix after the filename; the other LCD keeps its normal amber/green.
- Why separate: Colour AND text both change — exactly the disambiguation an atlas exists for.
- Variant of: SHOT-667

**SHOT-704 — BaySickNAM/IR, error label**
- Reach: Attempt to load a corrupt or unsupported .nam / .wav.
- Visible: Red text right-aligned in the status row's RIGHT half carrying the loader's error string. Every NAM / IR failure on the interactive routes (browse, drag-drop, recents menu) also raises a `BaySickNAM/IR` warning box with the same text prefixed by "Couldn't load NAM model:", "Couldn't load IR:" or "Couldn't load dropped file:" - there is no label-only state. The MIC SIM A / MIC SIM B user-IR pickers are the reverse: box only, no red label. A refused capture reads "This NAM capture was refused: <reason>." A failure during a project restore raises NEITHER the label nor the box; it only reaches the Missing files report.
- Why separate: The third state of the status row.
- Variant of: SHOT-666

**SHOT-705 — Missing files report**
- Reach: Open a project (or load a template / page preset) after moving or deleting a referenced NAM capture, sfizz kit, user IR or sample.
- Visible: Warning box "Missing files"; body explains the project/template/preset refers to files no longer where they were saved and that affected parts may be silent or playing a substitute; then up to 12 lines of "<what>: <path>" (e.g. "NAM capture: C:\..."), an "...and N more" tail, and closing advice to re-pick them or put the files back; OK.
- Also lands here, with wording that does not fit: a kit REFUSED by the SFZ safety gate, listed as "Instrument kit refused - <reason>: <path>". The file is present and the headline still says it is no longer where it was saved. This is the only place the refusal reason is ever displayed, and it can arrive under an unrelated headline noun ("sound", "preset", "kit", "undo") long after the pick that caused it.
- Why separate: The consolidated report a user meets on reopening a moved project.

**SHOT-706 — "Using a different audio device" startup notice**
- Reach: Set BaySickDAW to a device (e.g. an ASIO interface), power it off or unplug it, then relaunch.
- Visible: Warning box "Using a different audio device"; body shows "Wanted:" and "Now using:" lines, states the choice has been kept for next launch, lists common causes, and appends the driver's own error text after "The driver said:"; OK.

**SHOT-707 — "No audio device" startup notice**
- Reach: Make every audio device unavailable (unplug/disable all outputs) and launch.
- Visible: Warning box "No audio device"; body names what was tried, states the Windows default would not open either, lists common causes, points at Options > Audio Settings, and warns that **mixer buses and master are not created until a device opens, so the effects rack cannot add effects.**
- Why separate: Different title and a materially different body from the fallback notice — and it explains why other parts of the app look broken.
- Variant of: SHOT-706

**SHOT-708 — "Could not open project" warning**
- Reach: Delete or corrupt a project's project.xml, then open it from File > Open Recent or Quick Open.
- Visible: Warning box "Could not open project". Body from Open Recent: "The project folder may have been moved or deleted." Body from Quick Open Project...: "That folder doesn't contain a project.xml, or the file is corrupt." OK on both. Open Project... is NOT a route to this box for a deleted project.xml - it pre-checks the file and raises "Not a BaySickDAW project folder" / "That folder has no project.xml inside it." instead; it reaches "Could not open project" only when the file is present but the parser refuses it (a `<!DOCTYPE`, over 512 levels of nesting, or malformed XML), where the body then names the wrong problem.

**SHOT-709 — "Save failed" / "Save As failed" / "Could not create project"**
- Reach: Make the Projects folder read-only, then try File > Save, File > Save As... and File > New Project...
- Visible: Three warning boxes with those titles, each stating what could not be written and to check the Projects folder is writable; OK.

**SHOT-710 — "Save failed" — write failed (typed-name saves)**
- Reach: Make a preset/kit target folder read-only, then complete any typed-name save.
- Visible: Warning box "Save failed" reading "Couldn't write <full path>.", sometimes with a trailing sentence such as "The tab was not deleted."; OK.
- Why separate: Same title as SHOT-126 but an entirely different body — one is a bad name, this is a disk failure.
- Variant of: SHOT-126

**SHOT-711 — Project browser failure boxes**
- Reach: In Quick Open Project, rename to a name that already exists, or delete a folder Windows will not move to the Recycle Bin.
- Visible: Warning boxes "Rename failed", "Duplicate failed" or "Delete failed" with the corresponding one-line explanation; OK.

**SHOT-712 — "Restore failed" warning**
- Reach: Corrupt a backup file, then File > Restore from Backup... and restore it.
- Visible: Warning box "Restore failed" — the backup may be corrupt, or project.xml may not be writable; OK.

**SHOT-713 — "Export failed" warning**
- Reach: Run File > Export Audio... to a path that cannot be written (read-only folder, removed drive).
- Visible: Warning box "Export failed" with the render's own error text; OK.

**SHOT-714 — Export Project Bundle, missing / failed files summary**
- Reach: Move or delete a sample the project references, then File > Export Project Bundle... and complete the export.
- Visible: WARNING-icon box "Export Project Bundle"; destination + copied count, then a WARNING block listing up to 10 referenced files that could not be found, and/or a second WARNING block for files that could not be copied, each with an "...and N more" tail.
- Why separate: The warning form of SHOT-199 carries content the clean form never shows.
- Variant of: SHOT-199

**SHOT-715 — "Freeze failed" warning**
- Reach: Make the project's Freeze folder unwritable, then let an automatic freeze fire or trigger a manual one.
- Visible: Warning box "Freeze failed" naming the track and page number, quoting the error, showing the Freeze folder path, and noting the track keeps playing live and further failures this session will not be shown; OK.

**SHOT-716 — "Frozen tracks could not be restored"**
- Reach: Delete a project's Freeze folder contents, then reopen the project.
- Visible: Warning box "Frozen tracks could not be restored" with the affected list; OK.

**SHOT-717 — "Take report not saved"**
- Reach: Make the project Reports folder unwritable and let a recorded take end.
- Visible: Warning box "Take report not saved" with the writer's error and a note that further failures this session will not be shown; OK.

**SHOT-718 — "Cannot open report"**
- Reach: In the browser's Reports tree, open an HTML report that has been edited and re-saved by another program.
- Visible: Warning box "Cannot open report" explaining the file carries no BaySickDAW measurement data so there is nothing for the analyzer to show; OK.

**SHOT-719 — "Buffer Size Change Failed"**
- Reach: Options > Audio Settings... with an ASIO device open, change ONLY the Buffer Size to a size the driver rejects, click Apply.
- Visible: Warning box "Buffer Size Change Failed" with the driver's own error text quoted and a note that the previous settings remain active; OK.

**SHOT-720 — "No control panel"**
- Reach: Options > Audio Settings..., select an ASIO mode/device whose driver will not load, click "Open ASIO Control Panel".
- Visible: Warning box "No control panel" naming the mode / device pair and explaining only ASIO drivers have a panel; OK.

**SHOT-721 — Master Analyzer, "No audio for this take"**
- Reach: Master Analyzer, select a take captured as analysis-only, then click "Export Take...".
- Visible: Warning box "No audio for this take" explaining the take was analysis-only and pointing at the version-audio-capture setting in File Settings.

**SHOT-722 — Effect preset load / save failure alerts**
- Reach: Make a preset folder read-only, or corrupt a preset file, then use an effect's Presets menu.
- Visible: Warning boxes "Could not save preset", "Could not load preset" or "Could not save default" with the error text.

**SHOT-723 — IR / NAM load failure alerts (effect panels)**
- Reach: In the Acoustic Preamp / Acoustic Simulator / NAM Pedal, pick a non-audio .wav or a malformed .nam capture.
- Visible: Warning alerts "Load Acoustic IR" or "NAM Load Failed" carrying the DSP's error string.
**SHOT-724 - "Could not load program" (kit refused or unparseable)**
- Reach: Copy a program out of `Core Library\Black&Green Guitars\Programs\` back into that same `Programs` folder under a new name, add a line reading `#define $A $A` to the copy, then pick the copy from Load Guitar. The Rusty tab cannot be staged by copying - it only ever loads `01-full.sfz` and `02-basic.sfz` by name - so for that variant back up `Core Library\Big Rusty Drums\Programs\01-full.sfz`, add the same line to the shipped file, pick Full, then restore the backup.
- Visible: Warning box titled "Load Program" on a Guitars / Basses tab, or "Big Rusty Drums" on the Rusty tab, reading "Could not load program:" and the full path, OK. **No reason is given in the box.** On the Rusty tab the program that was loaded has already been torn down by the time the box appears, so the page is left empty and the Program dropdown clears; on an Inst tab the previous program keeps playing.
- Why separate: Same wording as an ordinary parse failure but a different cause, and the one refusal in the app whose written explanation never reaches the box that reports it.

---

## APPENDIX — Things that are NOT shots, but will bite during capture

Read this before starting. Every line came out of the code sweep and several
would otherwise cost a wasted sitting.

**Hit targets that are invisible**
- A ribbon slot has TWO hit zones: the name row navigates, only the bottom-right
  22px chevron region opens the dropdown (SHOT-047).
- The Record button splits the same way: the dot arms, the right 14px chevron
  opens the mode menu (SHOT-030).
- The control lane header is BOTH a click target (mode menu) and a drag target
  (resize) — SHOT-396 / SHOT-400, and SHOT-431 for drums.
- Rusty's 25 kit hitboxes are completely invisible; only press-and-hold draws a
  ring (SHOT-590) and only hover raises a name (SHOT-591).

**Live-looking things that intentionally do nothing**
- ~~Help > "Help Index (F1)" has no handler — reserved for these manuals.~~
  **RETIRED 2026-08-11 (QA-Manuals Task 1): this is now FALSE.** The manuals
  window shipped; `Help > Help Index  (F1)` and the bare F1 key both open it.
  Manual 1 documents it as live. Left visible rather than deleted because three
  separate documents carried this claim and a silent removal would let it
  reappear from one of the others.
- Options > "MIDI is Omni (all devices) - Read Only" is a permanently disabled
  status row.
- ~~Edit > "New Drums Tab" is hardcoded disabled in every state.~~
  **RETIRED 2026-08-11 (QA-Manuals): this is now FALSE.** The three fixed
  page-type rows were replaced by a "New Tab" submenu that embeds the ribbon
  "+" list whole, so the Edit menu has no hardcoded-disabled row left. Left
  visible rather than deleted for the same reason as the Help Index entry
  above - a second file still carries the claim.
- On Vocal Chain slot headers the up/down/x glyphs are painted but inert, and the
  "SC" button opens nothing (SHOT-620).
- The BaySickVocals live readout is hardcoded to "-- Hz / -- Hz / -- cents"
  (SHOT-609).

**Native vs JUCE dialogs** — only TWO decision dialogs in the whole app are
native Windows task dialogs and look completely different: the unsaved-changes
prompt (SHOT-108) and the Export Project Bundle size confirmation (SHOT-198).
Every file/folder picker is also native. Everything else uses the app's dark
styling.

**Desktop vs contained windows** — page windows are native CHILD windows inside
the fixed frame. Key Binds, Undo History, Plugins, Rusty Drums Map, Manuals,
Master Analyzer, Event Editor and every dialog are real DESKTOP windows owned
by the main window (they float above it and minimise with it, but not above other
apps). They share the same 26px title-strip look, so photograph at least one of
each with desktop background visible so the difference reads.

**One-per-app elements** — exactly ONE tooltip window (parentless, so it floats
over every child window), ONE app-wide undo manager, and ONE heavy-operation
overlay that promotes itself to a desktop window while showing.

**Dialogs with a "Don't show again" checkbox** — Replace Drums Kit (SHOT-443),
Lock/Unlock Kit (SHOT-445), WORLD works offline (SHOT-642), Reset Sub-Edits
(SHOT-643). **Capture all four BEFORE ticking anything**, or they are gone.

**Transient states that need staging** — the RE-ANALYZE / ANALYZING badges in
BaySickPitch and BaySickAlign (make the clip change while PLAYING so the pending
badge persists); the Delay Feed warning ring; the perf DSP overload flash; the
"Analyzing..." canvas; the heavy-operation overlays; the drag ghosts and
marquee/slice/zoom overlays on all four grids.

**Meters and visuals are dead boxes when stopped.** Every shot that mentions a
meter, spectrum, GR bar, LFO scope or visual strip needs audio actually playing.
Budget one "audio running" pass through Sittings 10-14 rather than starting and
stopping the transport per shot.

**Things a screenshot cannot solve, flagged as production decisions**
1. Labelling all 25 Rusty kit pieces needs an annotated composite over a clean
   photo, or 25 press-and-hold captures. Jeff's call.
2. Guitars and Basses have 11 programs each; this list pictures 5 of the 22. Full
   coverage is +17 shots.
3. Every pedal-capable effect has a rack face AND a board face. This list
   pictures all the rack faces and a representative set of board faces. Full
   board coverage is roughly +17 shots.
4. Key Binds has six tabs; two are pictured. Tab-by-tab coverage is +4 shots.

**Dead code found during the sweep — do NOT try to capture it**
- `RibbonTabBar::showSubPageDropdown` contains a Drums branch building
  "Sound / Piano Roll / EQ", but `showDropdown` routes Drums to
  `showInstanceDropdown`, so that menu can never appear.
- The `count == 0` branch inside `RibbonTabBar::showInstanceDropdown` is
  unreachable — instance-type slots vanish entirely at zero tabs.
- ~~`DrumPage::buildDrumKitTab()` exists but is never called~~ **CORRECTED
  2026-08-11 (QA-Manuals Task 1): the function name is wrong and the dead-code
  half is no longer true.** There is no `buildDrumKitTab` on `DrumPage` at all -
  QA-Cleanup folded that dead member out. The only `buildDrumKitTab()` in the
  tree is `BaySickRustyDrumsPage::buildDrumKitTab()`
  (`BaySickRustyDrumsPage.cpp:158`), and it IS called (`:76`) - it builds
  Rusty's own Drum Kit sub-tab, a different surface from the 16-pad grid.
  **The routing conclusion still holds:** the 16-pad Drum Kit grid for ordinary
  Drums tabs is owned by `PianoRollPage` (`DrumPage.cpp:126-127`, `:136` -
  sub-tab index 0 redirects), which is why every Drum Kit view routes through
  Ribbon > Piano Roll > engine pill.

**Also true and worth a manual sidebar rather than a shot**
- Drum banks 1-16 and 17-32 are two INDEPENDENT KITS, not one 32-slot kit.
- The Effects page is an INDEX; the six rows and two EQ buttons open separate
  windows.
- The Pedals window doubles as the player window for live-input Inst tabs, which
  is why those tabs have no separate "Player" row.
- Rusty's Piano Roll uses a note-label provider, so its keyboard shows
  articulation names on all-white keys (SHOT-392) rather than note names.
