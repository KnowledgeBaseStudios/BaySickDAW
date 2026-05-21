#include "KeyBindings.h"

namespace BSCommands
{
juce::String categoryName (Category c)
{
    switch (c)
    {
        case Category::General:        return "General Key Binds";
        case Category::Builder:        return "Builder Page Key Binds";
        case Category::PianoRoll:      return "Piano Roll Key Binds";
        case Category::DrumKit:        return "Drum Kit Key Binds";
        case Category::MouseReference: return "Mouse Reference";
    }
    return {};
}

namespace
{
    // Build catalog once - order here is the order shown in each tab.
    std::vector<CommandInfo> buildCatalog()
    {
        return
        {
            // ── General ─────────────────────────────────────────────────────
            { cmdPlayPause, Category::General,
              "Start / Stop Playback",
              "Press to start. Press again to pause. Shift+Space stops and disarms recording.",
              juce::KeyPress (juce::KeyPress::spaceKey) },

            { cmdStopAndDisarm, Category::General,
              "Stop and Disarm Record",
              "Stop playback and clear the record-arm state.",
              juce::KeyPress (juce::KeyPress::spaceKey,
                              juce::ModifierKeys::shiftModifier, 0) },

            { cmdToggleRecord, Category::General,
              "Toggle Recording",
              "Arm or disarm recording. While playback is running, this toggles between recording and just monitoring.",
              juce::KeyPress ('R') },

            // ── Page switches ────────────────────────────────────────────────
            { cmdShowMixer, Category::General,
              "Show Mixer",
              "Switch to the Mixer page.",
              juce::KeyPress (juce::KeyPress::F5Key) },

            { cmdShowEffects, Category::General,
              "Show Effects (Most Recent)",
              "Switch to the Effects page on the channel you last had open.",
              juce::KeyPress (juce::KeyPress::F6Key) },

            { cmdShowBuilder, Category::General,
              "Show Builder (Most Recent)",
              "Switch to the Builder page.",
              juce::KeyPress (juce::KeyPress::F7Key) },

            { cmdShowLayers, Category::General,
              "Show Layers (Most Recent)",
              "Switch to the most-recently-used Layers tab.",
              juce::KeyPress (juce::KeyPress::F8Key) },

            { cmdShowBass, Category::General,
              "Show Bass (Most Recent)",
              "Switch to the most-recently-used Bass tab.",
              juce::KeyPress (juce::KeyPress::F9Key) },

            { cmdShowDrums, Category::General,
              "Show Drums (Most Recent)",
              "Switch to the most-recently-used Drums tab. Lands on the Drum Kit sub-tab if that was your last view.",
              juce::KeyPress (juce::KeyPress::F10Key) },

            { cmdShowPianoRoll, Category::General,
              "Show Piano Roll",
              "Switch to the unified Piano Roll page (Drum Kit + every engine's piano roll, picked via the page dropdown).  Lands on the engine you were last editing; falls back to Drum Kit on first use.  Project save/load round-trips the active engine so reopening a session lands on the same view.",
              juce::KeyPress (juce::KeyPress::F11Key) },

            // ── File operations ──────────────────────────────────────────────
            { cmdFileNew, Category::General,
              "New Project",
              "Close the current project (prompts to save) and open a fresh one based on your default template, or empty if no default is set.",
              juce::KeyPress ('N', juce::ModifierKeys::ctrlModifier, 0) },

            { cmdFileOpen, Category::General,
              "Open Project",
              "Open an existing BaySickDAW project from disk.",
              juce::KeyPress ('O', juce::ModifierKeys::ctrlModifier, 0) },

            { cmdFileSave, Category::General,
              "Save Project",
              "Save the current project to its existing folder. First-time save behaves like Save As.",
              juce::KeyPress ('S', juce::ModifierKeys::ctrlModifier, 0) },

            { cmdFileSaveAs, Category::General,
              "Save Project As",
              "Save the current project to a new folder of your choosing.",
              juce::KeyPress ('S', juce::ModifierKeys::ctrlModifier
                                   | juce::ModifierKeys::shiftModifier, 0) },

            // ── Pattern navigation ──────────────────────────────────────────
            { cmdRenameActivePattern, Category::General,
              "Rename Active Pattern",
              "Open a name editor on the pattern currently selected in the transport-bar dropdown.",
              juce::KeyPress (juce::KeyPress::F2Key) },

            { cmdNextEmptyPattern, Category::General,
              "Jump to Next Empty Pattern",
              "Jump the pattern dropdown to the first pattern that contains no notes.",
              juce::KeyPress (juce::KeyPress::F3Key) },

            { cmdNewPattern, Category::General,
              "New Pattern",
              "Create a new empty pattern and select it in the transport-bar dropdown.",
              juce::KeyPress (juce::KeyPress::F4Key) },

            { cmdNextPattern, Category::General,
              "Next Pattern",
              "Cycle the transport-bar pattern dropdown to the next pattern. Wraps around at the end.",
              juce::KeyPress ('+') },

            { cmdPrevPattern, Category::General,
              "Previous Pattern",
              "Cycle the transport-bar pattern dropdown to the previous pattern. Wraps around at the start.",
              juce::KeyPress ('-') },

            // ── Transport extensions ────────────────────────────────────────
            { cmdToggleSongMode, Category::General,
              "Pattern / Song Mode Toggle",
              "Toggle the transport between Pattern mode (loop the active pattern) and Song mode (play the Builder arrangement).",
              juce::KeyPress ('L') },

            { cmdSeekHome, Category::General,
              "Seek Playhead to Start",
              "Send the transport playhead to bar 1 (start of the project). Works in both Pattern and Song mode.",
              juce::KeyPress (juce::KeyPress::homeKey) },

            { cmdFastForward, Category::General,
              "Fast Forward (4 bars)",
              "Skip the playhead 4 bars forward each press.",
              juce::KeyPress (juce::KeyPress::numberPad0) },

            { cmdPrevBarSong, Category::General,
              "Previous Bar (Song mode)",
              "Move the playhead one bar backward. Active only in Song mode.",
              juce::KeyPress ('/') },

            { cmdNextBarSong, Category::General,
              "Next Bar (Song mode)",
              "Move the playhead one bar forward. Active only in Song mode.",
              juce::KeyPress ('*') },

            { cmdToggleMetronome, Category::General,
              "Toggle Metronome",
              "Turn the click metronome on or off during playback.",
              juce::KeyPress ('M', juce::ModifierKeys::ctrlModifier, 0) },

            // ── Undo / Redo (Phase B-5) ─────────────────────────────────────
            { cmdGlobalUndo, Category::General,
              "Undo",
              "Step backward through your recent edits - note moves, pattern changes, mixer tweaks, the lot.",
              juce::KeyPress ('Z', juce::ModifierKeys::ctrlModifier, 0) },

            { cmdGlobalRedo, Category::General,
              "Redo",
              "Re-apply the edit you just undid.",
              juce::KeyPress ('Z', juce::ModifierKeys::ctrlModifier
                                   | juce::ModifierKeys::altModifier, 0) },

            // ── Recording precount (Phase D-5, 2026-04-26) ──────────────────
            { cmdToggleRecordingPrecount, Category::General,
              "Toggle Recording Precount",
              "When recording is armed, plays a 1-bar (4-beat) lead-in click before the actual recording starts.",
              juce::KeyPress ('P', juce::ModifierKeys::ctrlModifier, 0) },

            // ── Slip / Stretch edit-mode toggle (QA-Ea Task 0c, 2026-05-20)
            // Lives in the Builder tab because the dropdown surfaces on
            // the BuilderPage toolbar.  Default 'S'.  Toggles between the
            // two modes; the toolbar button label updates in lockstep.
            { cmdToggleSlipStretchMode, Category::Builder,
              "Toggle Slip/Stretch Editing",
              "Flips the Builder grid's Slip/Stretch dropdown between modes.  Slip: drag an Audio clip's left or right edge to expose pre-roll / trim leading or tail audio.  Stretch: drag an Audio clip's right edge to resize (time-stretching from the left edge ships in a later update).  Pattern and Automation blocks ignore this mode.",
              juce::KeyPress ('s', 0, 0) },
        };
    }

    std::vector<MouseRefRow> buildMouseRefs()
    {
        // Phase B-6 (2026-04-26): Path A - populate Builder + Piano Roll tabs
        // with documentation-only rows for every page-local key + mouse
        // modifier.  These render dimmed and are non-editable.  Rebindable
        // commands live in the editable list above (via getAllCommands()).
        return
        {
            // ── General ─────────────────────────────────────────────────────
            { Category::General, "Mouse Wheel",
              "Scroll Vertically",
              "Mouse wheel scrolls the view under the cursor up/down." },
            { Category::General, "Shift + Mouse Wheel",
              "Scroll Horizontally",
              "Hold Shift and scroll the mouse wheel to scroll left/right across whatever scrollable view you're hovering over." },

            // ── Builder Page - tool letters (page-local) ────────────────────
            { Category::Builder, "P",
              "Draw Tool",
              "Click to place a single clip at the cursor position." },
            { Category::Builder, "B",
              "Paint Tool",
              "Drag to place identical clips along a row - useful for repeating loops." },
            { Category::Builder, "C",
              "Slice Tool",
              "Click on a clip to split it at the click point into two independent clips." },
            { Category::Builder, "D",
              "Delete Tool",
              "Click clips to remove them." },
            { Category::Builder, "E",
              "Select Tool",
              "Click and drag for selection rectangles." },
            { Category::Builder, "S",
              "Slip Edit Tool",
              "Drag the clip's contents inside its bounds without moving the clip itself." },
            { Category::Builder, "T",
              "Mute Tool",
              "Click clips to mute them - they stay in place but produce no sound." },
            { Category::Builder, "Y",
              "Playback Tool",
              "Click a clip to audition it once from the start." },
            { Category::Builder, "Z",
              "Zoom Tool",
              "Click + drag to zoom into a region; right-click to zoom out. While the Zoom tool is active, PgUp / PgDn zoom in / out." },

            // ── Builder Page - edit operations (page-local) ─────────────────
            // (Ctrl + Z / Ctrl + Alt + Z migrated to global commands - see
            // Undo / Redo in the General tab.)
            { Category::Builder, "Ctrl + A",
              "Select All",
              "Select every clip in the arrangement." },
            { Category::Builder, "Ctrl + B",
              "Duplicate Right",
              "Repeat the selected clips immediately after themselves. With no selection, advances to the next step." },
            { Category::Builder, "Ctrl + C",
              "Copy Selection",
              "Copy the selected clips to the clipboard." },
            { Category::Builder, "Ctrl + V",
              "Paste",
              "Paste the clipboard at the playhead." },
            { Category::Builder, "Ctrl + Shift + 1..6",
              "Zoom Preset",
              "Snap the horizontal zoom to one of six preset levels." },
            { Category::Builder, "Delete / Backspace",
              "Delete Selected",
              "Remove the selected clips from the arrangement." },
            { Category::Builder, "Ctrl + Delete / Backspace",
              "Delete Time Region",
              "Remove every clip whose start lies in the highlighted time span and slide every later clip left by the removed bar count - closing the gap.  Source: ruler Ctrl+drag range first, falls back to the bounding span of the current clip selection." },
            { Category::Builder, "Ctrl + Left / Right",
              "Shift Time Selection",
              "Slide the highlighted ruler time-selection box LEFT or RIGHT by its own length.  The clips underneath don't move - only the highlighted span and the clip selection it auto-populates.  No-op when no ruler range is set; clamps to bar 0 on left shift." },
            { Category::Builder, "Escape",
              "Clear Selection",
              "Deselect everything." },
            { Category::Builder, "Shift + Arrows",
              "Move Selection",
              "Shift + Left / Right nudges the selection by one bar; Shift + Up / Down moves it between rows." },
            { Category::Builder, "PgUp / PgDn",
              "Vertical Scroll / Zoom",
              "Default: scroll the view by one viewport height. With the Zoom tool active: zoom in / out instead." },

            // ── Builder Page - mouse modifiers ──────────────────────────────
            { Category::Builder, "Mouse Wheel",
              "Scroll Vertically",
              "Bare wheel scrolls the grid up / down by ~1 row per click." },
            { Category::Builder, "Shift + Wheel",
              "Scroll Horizontally",
              "FL convention: wheel up scrolls right (advance through the timeline), wheel down scrolls left." },
            { Category::Builder, "Ctrl + Wheel",
              "Horizontal Zoom",
              "Cursor-anchored zoom - the bar under the mouse stays under the mouse." },
            { Category::Builder, "Alt + Wheel",
              "Vertical Zoom",
              "Adjusts the row height so more or fewer rows fit in the viewport." },
            { Category::Builder, "Ctrl + Left-Click",
              "Add to Selection",
              "Add the clicked clip to the active selection." },
            { Category::Builder, "Ctrl + Drag (empty area)",
              "Marquee Select",
              "Click and drag from any empty grid area to draw a selection rectangle, regardless of which tool is active." },
            { Category::Builder, "Ctrl + Drag (ruler)",
              "Time Selection",
              "On the ruler, Ctrl + drag defines a time range used by the loop point in Song mode." },
            { Category::Builder, "Click on Ruler",
              "Seek Playhead",
              "Bare click on the ruler moves the transport playhead to that position." },
            { Category::Builder, "Right-Click on Ruler",
              "Ruler Menu",
              "Open the ruler menu - add time markers and time-signature changes here." },
            { Category::Builder, "Alt + Right-Click",
              "Audition Clip",
              "Alt + right-click a clip to play it once from the start." },
            { Category::Builder, "Right-Alt + Right-Click",
              "Quantize Selection",
              "Open the quantize-options popup for the selected clips." },
            { Category::Builder, "Right-Alt + Left-Click",
              "Mute Clip",
              "Toggle mute on the clicked clip." },
            { Category::Builder, "Ctrl + Shift + Right-Click",
              "Zoom to Selected",
              "Zoom the viewport so the selected clip fills it." },
            { Category::Builder, "Ctrl + Right-Click + Drag",
              "Drag-Rect Zoom",
              "Drag a rectangle and release to zoom-fit it to the viewport." },
            { Category::Builder, "Shift + Right-Click + Drag",
              "Pan View",
              "Shift + right-click and drag to pan the view freely." },
            { Category::Builder, "Middle-Click + Drag",
              "Pan View",
              "Press the middle mouse button and drag to pan." },
            { Category::Builder, "Shift + Alt + Wheel",
              "Nudge Clip Position",
              "Hover over a clip and Shift + Alt + scroll to nudge it by tiny increments." },

            // ── Piano Roll - tool letters (page-local) ──────────────────────
            { Category::PianoRoll, "P",
              "Draw Tool",
              "Click to place a single note." },
            { Category::PianoRoll, "B",
              "Paint Tool",
              "Drag to place repeated notes following the grid." },
            { Category::PianoRoll, "C",
              "Slice Tool",
              "Click + drag a vertical line to slice notes at grid positions. Hold Alt to bypass snap." },
            { Category::PianoRoll, "D",
              "Delete Tool",
              "Click notes to remove them." },
            { Category::PianoRoll, "E",
              "Select Tool",
              "Click and drag to make selection rectangles." },
            { Category::PianoRoll, "T",
              "Mute Tool",
              "Click notes to mute them." },
            { Category::PianoRoll, "Z",
              "Zoom Tool",
              "Click + drag to zoom into a region; right-click to zoom out. While Zoom tool is active, PgUp / PgDn zoom in / out." },
            { Category::PianoRoll, "S",
              "Cycle Note Type",
              "Cycles the active note type between Standard, Slide, and Portamento. New notes inherit the active type. Selected notes are converted instead when there is a selection." },
            { Category::PianoRoll, "M",
              "Toggle Keyboard Column",
              "Hide / show the on-screen piano keyboard column on the left of the grid.  Per-tab state - each Piano Roll tab remembers its own visibility." },

            // ── Piano Roll - edit operations (page-local) ───────────────────
            // (Ctrl + Z / Ctrl + Alt + Z migrated to global commands - see
            // Undo / Redo in the General tab.)
            { Category::PianoRoll, "Ctrl + A",
              "Select All",
              "Select every note." },
            { Category::PianoRoll, "Ctrl + B",
              "Duplicate Right",
              "Repeat the selected notes immediately after themselves." },
            { Category::PianoRoll, "Ctrl + C",
              "Copy Selection",
              "Copy the selected notes to the clipboard." },
            { Category::PianoRoll, "Ctrl + V",
              "Paste",
              "Paste from the clipboard at the playhead." },
            { Category::PianoRoll, "Ctrl + G",
              "Glue",
              "Merge selected and overlapping notes into one." },
            { Category::PianoRoll, "Shift + G",
              "Group Selected",
              "Lock selected notes so they move and resize together." },
            { Category::PianoRoll, "Alt + G",
              "Ungroup Selected",
              "Break the selected note group apart so each note moves independently." },
            { Category::PianoRoll, "Shift + I",
              "Invert Selection",
              "Invert the current selection - selected notes deselect and vice versa." },
            { Category::PianoRoll, "Delete / Backspace",
              "Delete Selected",
              "Remove the selected notes." },
            { Category::PianoRoll, "Ctrl + Delete / Backspace",
              "Delete Time Region",
              "Remove every note whose start lies in the highlighted time span and slide every later note left by the removed length - closing the gap.  Source: ruler Ctrl+drag range first, falls back to the bounding span of the current note selection." },
            { Category::PianoRoll, "Shift + Arrows",
              "Move Selection",
              "Shift + Left / Right nudges by one snap unit; Shift + Up / Down transposes by one semitone." },
            { Category::PianoRoll, "Alt + Arrows",
              "Fine Nudge",
              "Move selection by one pixel in any direction (no snap)." },
            { Category::PianoRoll, "PgUp / PgDn",
              "Zoom In / Out",
              "Active only when the Zoom tool is selected." },

            // ── Piano Roll - tool dialogs (Alt + letter) ────────────────────
            { Category::PianoRoll, "Alt + Q",
              "Quantize",
              "Open the quantize options dialog for selected notes." },
            { Category::PianoRoll, "Alt + S",
              "Strum",
              "Stagger selected note start times to simulate a strum." },
            { Category::PianoRoll, "Alt + A",
              "Arpeggiate",
              "Generate an arpeggio from the selected notes." },
            { Category::PianoRoll, "Alt + U",
              "Chop",
              "Open the chop dialog - split selected notes into N equal pieces." },
            { Category::PianoRoll, "Alt + L",
              "Articulate",
              "Apply articulation curve to selected notes." },
            { Category::PianoRoll, "Alt + R",
              "Randomize",
              "Apply random offsets to selected note properties." },
            { Category::PianoRoll, "Alt + P",
              "Generate Chords",
              "Generate scale-aware chords from selected single notes." },
            { Category::PianoRoll, "Alt + F",
              "Flam",
              "Add a 1/32-note grace note before each selected note (same pitch, reduced velocity)." },
            { Category::PianoRoll, "Alt + M",
              "Mute Selected",
              "Mute every selected note - they stay in the pattern but render silent.  Alt + Shift + M unmutes." },
            { Category::PianoRoll, "Alt + X",
              "Scale Levels",
              "Open a popup with a velocity slider + numeric input.  OK applies the percentage (100 % = no change) to every selected note's velocity." },

            // ── Piano Roll - D-7 quick shortcuts ────────────────────────────
            { Category::PianoRoll, "Ctrl + Q",
              "Quick Quantize 1/4",
              "Snap each selected note's start to the nearest 1/4-note boundary - bypasses the snap setting (selection-only)." },
            { Category::PianoRoll, "Ctrl + U",
              "Quick Chop into 4",
              "Split each selected note into four equal pieces - same as Alt+U but skips the dialog (selection-only)." },
            { Category::PianoRoll, "Ctrl + L",
              "Quick Legato",
              "Extend each selected note's length so it ends right when the next note begins (selection-only)." },
            { Category::PianoRoll, "Ctrl + Up / Down",
              "Transpose Octave",
              "Shift the selected notes up or down by 12 semitones (selection-only)." },
            { Category::PianoRoll, "Ctrl + Left / Right",
              "Shift Time Selection",
              "Slide the highlighted ruler time-selection box LEFT or RIGHT by its own length.  The notes underneath don't move - only the highlighted span and the selection it auto-populates.  No-op when no ruler range is set; clamps to beat 0 on left shift." },
            { Category::PianoRoll, "Ctrl + Alt + Home",
              "Flip Resize Edge",
              "Toggle whether note-edge drag-resize grabs the LEFT or the RIGHT edge.  When ON, dragging a note's left edge extends its start backward; when OFF, dragging the right edge extends the length forward." },

            // ── Piano Roll - mouse modifiers ────────────────────────────────
            { Category::PianoRoll, "Mouse Wheel",
              "Vertical Scroll",
              "Scroll vertically through the keyboard range (lower / higher pitches)." },
            { Category::PianoRoll, "Shift + Wheel",
              "Horizontal Scroll",
              "Scroll horizontally through the timeline." },
            { Category::PianoRoll, "Ctrl + Wheel",
              "Horizontal Zoom",
              "Zoom in / out on the timeline." },
            { Category::PianoRoll, "Alt + Wheel (over grid)",
              "Vertical Zoom",
              "Over the note grid: adjust the note row height so more or fewer pitches fit in the viewport." },
            { Category::PianoRoll, "Alt + Wheel (over Control Lane)",
              "Adjust Lane Value",
              "Over the velocity / pan / pitch-bend bar strip: bump the lane's currently-displayed property for the note whose bar is under the cursor by +/-0.05 (Shift+Alt+Wheel = +/-0.01 for fine adjustment).  When notes are selected, only those notes' bars are targetable - solves the chord-overlap drag-targets-wrong-note bug.  Selected bars also paint RED in the lane." },
            { Category::PianoRoll, "Right-Click + Wheel",
              "Cycle Tools",
              "Hold the right mouse button and roll the wheel to cycle through the available tools." },
            { Category::PianoRoll, "Ctrl + Left-Click",
              "Toggle Note Selection",
              "Add the clicked note to the selection without clearing what's already selected." },
            { Category::PianoRoll, "Ctrl + Drag (empty area)",
              "Marquee Select",
              "Click and drag from any empty piano-roll area to draw a selection rectangle, regardless of which tool is active." },
            { Category::PianoRoll, "Ctrl + Drag (ruler)",
              "Time Selection",
              "On the ruler, Ctrl + drag defines a time range used as a loop region during playback." },
            { Category::PianoRoll, "Click on Ruler",
              "Seek Playhead",
              "Bare click on the ruler moves the transport playhead to that position." },
            { Category::PianoRoll, "Right-Click",
              "Delete Note",
              "Right-clicking a note hard-deletes it (matches the Delete tool)." },

            // ── Drum Kit (D-7 sub-2 - self-contained: every drum-applicable
            //    keybind is documented in full so beginners never need to
            //    cross-reference the Piano Roll tab) ─────────────────────
            { Category::DrumKit, "P",
              "Draw Tool",
              "Click on the grid to place a single drum hit." },
            { Category::DrumKit, "B",
              "Paint Tool",
              "Drag across the grid to place repeated drum hits at every grid position." },
            { Category::DrumKit, "C",
              "Slice Tool",
              "Click + drag a vertical line to slice drum hits at grid positions.  Hold Alt to bypass snap and slice at the exact mouse X." },
            { Category::DrumKit, "D",
              "Delete Tool",
              "Click drum hits to remove them.  Drag to erase a swath." },
            { Category::DrumKit, "E",
              "Select Tool",
              "Click and drag to make selection rectangles.  Click an existing hit to grab it for moving." },
            { Category::DrumKit, "T",
              "Mute Tool",
              "Click drum hits to mute them - they stay in the pattern but render silent." },

            { Category::DrumKit, "Ctrl + A",
              "Select All",
              "Select every drum hit on every row of the current pattern." },
            { Category::DrumKit, "Ctrl + B",
              "Duplicate Right",
              "Repeat the selected drum hits immediately after themselves on the same rows." },
            { Category::DrumKit, "Ctrl + C",
              "Copy",
              "Copy the selection to the clipboard." },
            { Category::DrumKit, "Ctrl + V",
              "Paste",
              "Paste from the clipboard at the playhead." },
            { Category::DrumKit, "Ctrl + G",
              "Glue",
              "Merge selected and overlapping drum hits on the same row into one longer hit." },
            { Category::DrumKit, "Ctrl + Q",
              "Quick Quantize 1/4",
              "Snap each selected drum hit's start to the nearest 1/4-note boundary - bypasses the snap setting (selection-only)." },
            { Category::DrumKit, "Ctrl + U",
              "Quick Chop into 4",
              "Split each selected drum hit into four equal pieces - same as Alt+U but skips the dialog (selection-only).  Pieces smaller than 1/16 note are blocked." },
            { Category::DrumKit, "Ctrl + Delete / Backspace",
              "Delete Time Region",
              "Remove every drum hit whose start lies in the highlighted time span and slide every later hit left by the removed length - closing the gap.  Source: ruler Ctrl+drag range first, falls back to the bounding span of the current selection." },
            { Category::DrumKit, "Ctrl + Alt + Home",
              "Flip Resize Edge",
              "Toggle whether drum-hit edge drag-resize grabs the LEFT or the RIGHT edge.  When ON, dragging a hit's left edge extends its start backward; when OFF, dragging the right edge extends the length forward." },
            { Category::DrumKit, "Ctrl + Left / Right",
              "Shift Time Selection",
              "Slide the highlighted ruler time-selection box LEFT or RIGHT by its own length.  The drum hits underneath don't move - only the highlighted span and the selection it auto-populates.  No-op when no ruler range is set; clamps to beat 0 on left shift." },

            { Category::DrumKit, "Alt + Q",
              "Quantize",
              "Open the quantize options dialog for selected drum hits." },
            { Category::DrumKit, "Alt + S",
              "Strum",
              "Stagger selected drum-hit start times to simulate a strum / roll across rows." },
            { Category::DrumKit, "Alt + U",
              "Chop",
              "Open the chop dialog - split selected drum hits into N equal pieces." },
            { Category::DrumKit, "Alt + L",
              "Articulate",
              "Apply articulation curve to selected drum hits (length / velocity tapering)." },
            { Category::DrumKit, "Alt + R",
              "Randomize",
              "Apply random offsets to selected drum-hit timing / velocity." },
            { Category::DrumKit, "Alt + F",
              "Flam",
              "Add a 1/32-note grace hit before each selected drum hit (same row, 60 % velocity).  The classic drum flam." },
            { Category::DrumKit, "Alt + X",
              "Scale Levels",
              "Open a popup with a velocity slider + numeric input.  OK applies the percentage (100 % = no change) to every selected drum hit's velocity." },
            { Category::DrumKit, "Alt + Wheel (over Control Lane)",
              "Adjust Lane Value",
              "Over the velocity / pan bar strip below the grid: bump the lane's currently-displayed property for the drum hit whose bar is under the cursor by +/-0.05 (Shift+Alt+Wheel = +/-0.01 for fine adjustment).  When drum hits are selected, only those hits' bars are targetable - solves the chord-overlap drag-targets-wrong-hit bug.  Selected bars paint RED in the lane." },

            { Category::DrumKit, "Shift + I",
              "Invert Selection",
              "Invert the current selection - selected drum hits deselect and vice versa." },
            { Category::DrumKit, "Delete / Backspace",
              "Delete Selected",
              "Remove the selected drum hits." },
            { Category::DrumKit, "Shift + Arrows",
              "Move Selection",
              "Shift + Left / Right nudges by one snap unit; Shift + Up / Down moves the selection between drum rows." },

            { Category::DrumKit, "M",
              "(Piano Roll only)",
              "Toggle Keyboard Column applies only to the Piano Roll - the Drum Kit grid uses a sidebar instead of a piano keyboard, so M is unbound here." },
            { Category::DrumKit, "S",
              "(Piano Roll only)",
              "Cycle Note Type (Standard / Slide / Portamento) applies only to the Piano Roll - drum hits don't carry slide or portamento, so S is unbound here." },
            { Category::DrumKit, "Ctrl + L",
              "(Piano Roll only)",
              "Quick Legato applies only to the Piano Roll - it extends each note up to the start of the next note, which doesn't make sense for drum hits that end naturally on their sample." },
            { Category::DrumKit, "Ctrl + Up / Down",
              "(Piano Roll only)",
              "Transpose Octave applies only to the Piano Roll - drum-kit rows are slot-based (one row per drum), not pitch-based, so transposing isn't meaningful." },
            { Category::DrumKit, "Alt + A / Alt + P",
              "(Piano Roll only)",
              "Arpeggiate (Alt+A) and Generate Chords (Alt+P) apply only to the Piano Roll - both are pitch-based and don't translate to a slot-per-row drum layout." },
        };
    }
}

const std::vector<CommandInfo>& getAllCommands()
{
    static const std::vector<CommandInfo> catalog = buildCatalog();
    return catalog;
}

const CommandInfo* findCommand (int id)
{
    for (auto& c : getAllCommands())
        if (c.id == id) return &c;
    return nullptr;
}

std::vector<const CommandInfo*> getCommandsInCategory (Category c)
{
    std::vector<const CommandInfo*> out;
    for (auto& cmd : getAllCommands())
        if (cmd.category == c) out.push_back (&cmd);
    return out;
}

const std::vector<MouseRefRow>& getMouseRefRows()
{
    static const std::vector<MouseRefRow> rows = buildMouseRefs();
    return rows;
}

std::vector<const MouseRefRow*> getMouseRefsInCategory (Category c)
{
    std::vector<const MouseRefRow*> out;
    for (auto& r : getMouseRefRows())
        if (r.category == c) out.push_back (&r);
    return out;
}

std::vector<const MouseRefRow*> findHardcodedConflicts (const juce::KeyPress& kp)
{
    std::vector<const MouseRefRow*> out;
    if (! kp.isValid()) return out;

    for (auto& r : getMouseRefRows())
    {
        // createFromDescription returns an invalid KeyPress for shortcuts that
        // don't parse as a single keypress (mouse modifiers, ambiguous strings
        // like "PgUp / PgDn").  We just skip those.
        const juce::KeyPress parsed = juce::KeyPress::createFromDescription (r.shortcut);
        if (parsed.isValid() && parsed == kp)
            out.push_back (&r);
    }
    return out;
}

// ── Persistence ─────────────────────────────────────────────────────────────
juce::File getKeymapFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                  .getChildFile ("BaySickDAW");
    if (! dir.exists())
        dir.createDirectory();
    return dir.getChildFile ("keymap.xml");
}

void saveMappings (const juce::KeyPressMappingSet& set)
{
    if (auto x = std::unique_ptr<juce::XmlElement> (set.createXml (true)))
        x->writeTo (getKeymapFile());
}

bool loadMappings (juce::KeyPressMappingSet& set)
{
    auto f = getKeymapFile();
    if (! f.existsAsFile()) return false;
    if (auto x = juce::XmlDocument::parse (f))
    {
        set.restoreFromXml (*x);
        return true;
    }
    return false;
}

} // namespace BSCommands
