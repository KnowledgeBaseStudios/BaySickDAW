#include "KeyBindings.h"
#include "../AppPaths.h"
#include "../UserFileSave.h"

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
        case Category::VocalEditors:   return "Vocal Editor Key Binds";
        case Category::EventEditor:    return "Event Editor Key Binds";
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
            { cmdShowBuilder, Category::General,
              "Show Builder",
              "Switch to the Builder page.",
              juce::KeyPress (juce::KeyPress::F5Key) },

            { cmdShowMixer, Category::General,
              "Show Mixer",
              "Switch to the Mixer page.",
              juce::KeyPress (juce::KeyPress::F6Key) },

            { cmdShowPlayer, Category::General,
              "Show Player (Most Recent)",
              "Switch to the instrument tab you were last on - Layers, Bass, Drums, Clip, Vox, Inst or Plugins. Falls back to the first one you have open; does nothing when you have none.",
              juce::KeyPress (juce::KeyPress::F7Key) },

            { cmdShowEffectsRack, Category::General,
              "Show Effects Rack (Most Recent)",
              "Switch to the Effects page, which comes back up on the channel you last had open there.",
              juce::KeyPress (juce::KeyPress::F8Key) },

            { cmdShowEffectPanel, Category::General,
              "Show Effect Panel (Most Recent)",
              "Bring the single-effect panel you last opened back to the front. Does nothing until you have opened one, and nothing once you have closed it.",
              juce::KeyPress (juce::KeyPress::F9Key) },

            { cmdShowPianoRoll, Category::General,
              "Show Piano Roll (Most Recent)",
              "Switch to the unified Piano Roll page on whichever engine its own dropdown is set to. That choice is saved with the project, so reopening a session lands on the same view.",
              juce::KeyPress (juce::KeyPress::F10Key) },

            { cmdShowDrumKit, Category::General,
              "Show Drum Kit (Most Recent)",
              "Switch to the Piano Roll page and show the 16-pad Drum Kit grid.",
              juce::KeyPress (juce::KeyPress::F11Key) },

            { cmdShowEventEditor, Category::General,
              "Show Event Editor (Most Recent)",
              "Bring the Event Editor you last opened back to the front. Does nothing until you have opened one, and nothing once you have closed it.",
              juce::KeyPress (juce::KeyPress::F12Key) },

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

            // QA-Export (FSW-065): Ctrl+R.  Verified free before binding -- no
            // other command uses R with a modifier; bare R stays record-toggle.
            { cmdExportAudio, Category::General,
              "Export Audio",
              "Render the whole arrangement out to an audio file (WAV, OGG or MP3).",
              juce::KeyPress ('R', juce::ModifierKeys::ctrlModifier, 0) },

            // ── Pattern navigation ──────────────────────────────────────────
            { cmdRenameActivePattern, Category::General,
              "Rename / Color",
              "Open one dialog holding the name AND the color of the pattern currently selected in the transport-bar dropdown. OK applies both, Cancel applies neither.",
              juce::KeyPress (juce::KeyPress::F2Key) },

            { cmdNextEmptyPattern, Category::General,
              "Find Next Empty Pattern",
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

            // ── Pattern list editing ────────────────────────────────────────
            // Ctrl+Shift+Insert, Alt+C and Ctrl+Shift+Up/Down were each checked
            // against the catalog above AND every page-local keyPressed handler
            // before binding: Insert appears nowhere in the tree, no Alt+letter
            // table claims C, and every arrow handler excludes the Ctrl+Shift
            // pair (Builder nudge is Shift-only, roll transpose is Ctrl-only).
            { cmdInsertPattern, Category::General,
              "Insert Pattern",
              "Add a new empty pattern directly after the one selected in the transport-bar dropdown, and select it. Patterns after it shift down a slot; everything that pointed at them follows.",
              juce::KeyPress (juce::KeyPress::insertKey,
                              juce::ModifierKeys::ctrlModifier
                              | juce::ModifierKeys::shiftModifier, 0) },

            { cmdClonePattern, Category::General,
              "Clone Pattern",
              "Make a full copy of the selected pattern - all of its notes come with it - and select the copy.",
              juce::KeyPress ('C', juce::ModifierKeys::altModifier, 0) },

            { cmdMovePatternUp, Category::General,
              "Move Pattern Up",
              "Swap the selected pattern with the one above it in the list. Does nothing when it is already first.",
              juce::KeyPress (juce::KeyPress::upKey,
                              juce::ModifierKeys::ctrlModifier
                              | juce::ModifierKeys::shiftModifier, 0) },

            { cmdMovePatternDown, Category::General,
              "Move Pattern Down",
              "Swap the selected pattern with the one below it in the list. Does nothing when it is already last.",
              juce::KeyPress (juce::KeyPress::downKey,
                              juce::ModifierKeys::ctrlModifier
                              | juce::ModifierKeys::shiftModifier, 0) },

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

            // ── Typing-keyboard MIDI (D-4, QA-TransportDisplay 2026-07-08) ──
            { cmdToggleTypingKeyboard, Category::General,
              "Toggle Typing Keyboard (MIDI)",
              "Play the active tab's instrument with your computer keyboard. Bottom row Z-M is one octave (S D G H J are the black keys), top row Q-P is the octave above (number row black keys). Page Up / Page Down shift octaves. Notes record just like a hardware MIDI keyboard.",
              juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0) },
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
              "Click to place a single clip at the cursor position.  The clip type follows the last browser entry clicked - pattern, audio clip, or automation." },
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
              "Cursor-anchored vertical zoom - the row under the mouse stays under the mouse while the row height grows / shrinks." },
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
            { Category::Builder, "Alt + Drag (clip)",
              "Fine Move (No Snap)",
              "Hold Alt while dragging a clip to bypass grid snap for free sub-bar (PPQ) positioning." },
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

            // ── Builder browser (QA-Fe2, 2026-07-16) ────────────────────────
            { Category::Builder, "Right-Click (Clips / Vox / Inst header)",
              "Create Group",
              "Creates a browser group under that category.  Vox and Inst recordings auto-group by take; groups made here are for organizing anything else (Clips never auto-group)." },
            { Category::Builder, "Right-Click (browser group)",
              "Rename Group",
              "Renames the group.  For a recording group this renames every take file in it on disk (the Dry / Dry Cleaned / Wet / Wet Cleaned tags are kept) and every clip keeps playing.  Stop playback first for recording groups." },
            { Category::Builder, "Right-Click (cleaned take)",
              "Regenerate De-noise",
              "Re-cleans a Dry Cleaned / Wet Cleaned take at Light or Strong strength from its stored room profile.  Stop playback first." },
            { Category::Builder, "Drag (browser right edge)",
              "Resize Browser",
              "Drags the browser panel wider (up to 3x) or back to its default width.  Resets each session." },
            { Category::Builder, "Right-Click (Vox strip Arm LED)",
              "Input + Builder Grid Default",
              "Picks the interface input for the strip, and sets which recorded take (Dry / Dry Cleaned / Wet / Wet Cleaned) lands on the Builder grid.  No pick = automatic (Wet when realtime correction is on, else Dry); a pick locks until the project closes." },

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
              "Cycles the armed note type: Flat, RP Slide (ramp - the sounding note bends into this note's pitch, no new attack), RT Slide (retrigger - new attack gliding in from the previous note's pitch), Portamento. Shown on the toolbar button next to Select - clicking it cycles too. New notes inherit the armed type. When notes are selected, S also converts them to the newly armed type." },
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
            { Category::PianoRoll, "Alt + Drag (note)",
              "Fine Move (No Snap)",
              "Hold Alt while dragging notes to bypass grid snap for free sub-beat (PPQ) positioning." },
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
              "Open the Randomize dialog: Pattern section generates random notes (octave/key/scale pool, population, stack, random portamento, merge), Levels section randomizes velocity/pan/fine-pitch/release/cutoff/resonance with seeded wheels.  Live preview; Accept applies as one undo step." },
            { Category::PianoRoll, "Alt + E",
              "Riff Machine",
              "Open the Riff Machine generator: 8 steps (Progression, Chords, Arpeggiation, Mirror, Levels, Articulation, Groove, Fit) with per-step enables, preview-to-step, dice, and work-on-existing-score mode.  Live preview; Accept applies as one undo step." },
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
              "Cursor-anchored zoom - the beat under the mouse stays under the mouse." },
            { Category::PianoRoll, "Alt + Wheel (over grid)",
              "Vertical Zoom",
              "Over the note grid: cursor-anchored vertical zoom - the pitch under the mouse stays under the mouse while the row height grows / shrinks." },
            { Category::PianoRoll, "Double-Click Note",
              "Note Properties",
              "Open the Note Properties popup for the note under the cursor (Draw or Select tool): type Flat/RP Slide/RT Slide/Porta plus Velocity, Release, Fine Pitch, Panning, Filter Cutoff, Resonance.  When the clicked note is part of the selection, edits apply to EVERY selected note.  Changes apply live; each popup session is one undo step." },
            { Category::PianoRoll, "Alt + Wheel (over Control Lane)",
              "Adjust Lane Value",
              "Over the velocity / pan / pitch-bend / filter-cutoff bar strip: bump the lane's currently-displayed property for the note whose bar is under the cursor by +/-0.05 (Shift+Alt+Wheel = +/-0.01 for fine adjustment).  When notes are selected, only those notes' bars are targetable - solves the chord-overlap drag-targets-wrong-note bug.  Selected bars also paint RED in the lane." },
            { Category::PianoRoll, "Ctrl + Click (piano key)",
              "Select Pitch Row",
              "Select every note at that key's pitch.  Additive - Ctrl+clicking more keys adds their notes to the selection.  No audition fires for the click." },
            { Category::PianoRoll, "Ctrl + Drag (over Control Lane)",
              "Scrub Lane Values",
              "Sweep the cursor across the lane to set the SELECTED notes' values as it passes their bars - the value follows the cursor's height along the path.  Selected notes only; one undo step per sweep.  Plain drag still edits a single bar." },
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
            // Drum Kit - mouse modifiers (16 rows are fixed: horizontal only)
            { Category::DrumKit, "Mouse Wheel",
              "Horizontal Scroll",
              "Bare wheel scrolls the timeline left / right.  The 16 drum rows are fixed in place, so there is no vertical scroll." },
            { Category::DrumKit, "Shift + Wheel",
              "Horizontal Scroll",
              "Also scrolls the timeline left / right." },
            { Category::DrumKit, "Ctrl + Wheel",
              "Horizontal Zoom",
              "Cursor-anchored zoom - the beat under the mouse stays under the mouse.  There is no vertical zoom; the 16 rows are fixed by design." },
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
              "Cycle Note Type (Flat / RP Slide / RT Slide / Portamento) applies only to the Piano Roll - drum hits don't carry slide or portamento, so S is unbound here." },
            { Category::DrumKit, "Ctrl + L",
              "(Piano Roll only)",
              "Quick Legato applies only to the Piano Roll - it extends each note up to the start of the next note, which doesn't make sense for drum hits that end naturally on their sample." },
            { Category::DrumKit, "Ctrl + Up / Down",
              "(Piano Roll only)",
              "Transpose Octave applies only to the Piano Roll - drum-kit rows are slot-based (one row per drum), not pitch-based, so transposing isn't meaningful." },
            { Category::DrumKit, "Alt + A / Alt + P",
              "(Piano Roll only)",
              "Arpeggiate (Alt+A) and Generate Chords (Alt+P) apply only to the Piano Roll - both are pitch-based and don't translate to a slot-per-row drum layout." },

            // ── Vocal Editors (QA-Fd, 2026-07-11) - BaySickPitch tab ────────
            // Page-local hardcoded keys + mouse gestures; none rebindable.
            { Category::VocalEditors, "Click / Drag (note)",
              "Select + Repitch",
              "Click selects a note pill; drag repitches it VERTICALLY in 0.1-semitone steps (Snap on = the drag walls onto in-scale lanes but keeps the note's natural cents; Center is what flattens cents).  Plain drags never move a note in time -- timing moves are the deliberate Ctrl+Alt / Ctrl+Shift gestures below." },
            { Category::VocalEditors, "Ctrl + Drag (note)",
              "Fine Repitch",
              "Fine 0.01-semitone vertical movement that bypasses Snap." },
            { Category::VocalEditors, "Ctrl + Alt + Drag (note)",
              "Move Note (elastic)",
              "Moves the note in time (pitch still rides the vertical axis).  Neighbors stretch elastically so the phrase stays continuous, and the neighboring pills act as walls." },
            { Category::VocalEditors, "Ctrl + Shift + Drag (note)",
              "Detach Move",
              "Moves the note in time as a HARD CUT (past 3 px): ignores the neighbor walls entirely so the note can jump anywhere; the gap becomes a splice instead of an elastic warp." },
            { Category::VocalEditors, "Drag (note edge)",
              "Stretch / Squeeze",
              "Dragging the first or last 6 px of a pill stretches or squeezes that note's timing against its neighbors.  Ctrl past 3 px detaches the edge (hard cut, no walls)." },
            { Category::VocalEditors, "Double-Click (note)",
              "Open Sub-Editor",
              "Opens the floating sub-edit window: volume / pitch point lanes over the note's waveform, Vib / Frm / Variation knobs, a pill browser, and its own Play button." },
            { Category::VocalEditors, "Shift + Click (note)",
              "Toggle in Selection",
              "Adds the note to the selection, or removes it if already selected.  Marquee-drag on empty space also selects (Shift adds)." },
            { Category::VocalEditors, "Click (Slice mode)",
              "Split Note",
              "With the Slice tool armed, clicking a note splits it (snaps to the beat grid; hold Alt to slice anywhere) -- e.g. chop a consonant off its vowel.  Slice mode never selects - switch back to Edit for selection gestures." },
            { Category::VocalEditors, "Right-Click (note)",
              "Note Menu",
              "Restore to original / snap to semitone / merge selection / re-pitch to a note / copy + paste edits / open sub-editor / zoom to selection." },
            { Category::VocalEditors, "Ctrl + Right-Drag",
              "Zoom to Rect",
              "Drag a rectangle to zoom the view onto it.  Plain right-click on empty space returns to the saved view." },
            { Category::VocalEditors, "Middle-Drag",
              "Pan View",
              "Pans time and pitch together." },
            { Category::VocalEditors, "Mouse Wheel",
              "Scroll / Zoom",
              "Bare wheel scrolls pitch up / down.  Shift+Wheel scrolls time.  Ctrl+Wheel zooms time (cursor-anchored).  Alt+Wheel zooms the lane height vertically (Piano Roll idiom)." },
            { Category::VocalEditors, "A",
              "Toggle Auto-Scroll",
              "Playhead page-flip follow on / off (same as the >> toolbar button)." },
            { Category::VocalEditors, "Ctrl + A / C / V",
              "Select All / Copy / Paste Edits",
              "Copy takes the focused note's treatment (pitch offset, curves, knob settings); Paste applies it to the selection.  Audio never relocates - only the edit state transfers." },
            { Category::VocalEditors, "Ctrl + B",
              "Duplicate Treatment",
              "Copy + paste in one stroke: the focused note's treatment applies across the selection as a single undo step." },
            { Category::VocalEditors, "Shift + Arrows",
              "Nudge",
              "Up / Down = +/- 0.1 semitone.  Left / Right = +/- 10 ms time nudge (elastic rules apply)." },
            { Category::VocalEditors, "Delete / Backspace",
              "Restore to Original",
              "Notes are analysis segments, not deletable objects - Delete returns the selection to its as-sung state." },
            { Category::VocalEditors, "Escape",
              "Close / Deselect",
              "Closes the sub-edit window if open, otherwise clears the selection." },
            { Category::VocalEditors, "SPACE (sub-editor)",
              "Preview Note",
              "Plays the open note's span through its current edits; press again to stop.  Works while the main transport is stopped." },
            { Category::VocalEditors, "Click / Drag / Right-Click (sub-editor lane)",
              "Add / Move / Delete Point",
              "Event Editor gesture set on the volume / pitch lanes: click empty space adds a point, drag moves one, right-click deletes it." },
            { Category::VocalEditors, "Ctrl + A, then Right-Click (grid)", "Force All to Scale",
              "After Select All, right-click the grid to force every out-of-scale note onto the center of its nearest in-key scale note. In-key notes keep their natural cents. Needs a non-Chromatic scale; independent of the Snap toggle." },

            // ── Vocal Editors - BaySickAlign tab (mouse-only surface) ───────
            { Category::VocalEditors, "Drag (sync point strip)",
              "Move Sync Point",
              "Drags a sync-point marker's leader or follower time.  Changes arm the RE-ANALYZE badge; the re-match runs at the next stop." },
            { Category::VocalEditors, "Right-Click (sync point strip)",
              "Sync Point Menu",
              "Delete the clicked sync point, or generate Automatic Sync Points." },
            { Category::VocalEditors, "Drag (protected strip)",
              "Paint Protected Area",
              "Drags out a region the aligner must leave alone.  Right-click it to toggle Protect Timing / Protect Pitch or delete the area." },

            // ── Event Editor (QA-UndoCoverage Task 5, docket 14=a) ──────────
            // Window-local hardcoded keys; display-only, none rebindable.
            { Category::EventEditor, "P",
              "Pencil Tool",
              "Arms the Draw tool: click empty space to add a point, drag a point to move it." },
            { Category::EventEditor, "B",
              "Paint Tool",
              "Arms the Paint brush: drag to lay a run of points along the curve." },
            { Category::EventEditor, "D",
              "Erase Tool",
              "Arms the Erase tool: click or drag across points to remove them." },
            { Category::EventEditor, "I",
              "Interpolate Tool",
              "Arms the Interpolate tool: smooths the curve between the points you drag across." },
            { Category::EventEditor, "E",
              "Select Tool",
              "Arms the Select tool: marquee-drag to select points for group edits." },
            { Category::EventEditor, "Z",
              "Zoom Tool",
              "Arms the Zoom tool: drag a rectangle to zoom the lane view onto it." },
            { Category::EventEditor, "Delete / Backspace",
              "Delete Selected Points",
              "Removes every selected point.  Deleting the last point prompts to remove the whole automation clip instead of leaving an empty block." },
            { Category::EventEditor, "Ctrl + Z",
              "Undo",
              "Undoes the most recent edit in the app's ONE history - lane edits land in the same Ctrl+Z stream as everything else." },
            { Category::EventEditor, "Ctrl + Alt + Z",
              "Redo",
              "Redo, matching the app-wide binding.  (The editor's old local Ctrl+Y redo was retired when its history joined the app's.)" },
            { Category::EventEditor, "Ctrl + A",
              "Select All Points",
              "Selects every point in the lane." },
            { Category::EventEditor, "Ctrl + M",
              "Import MIDI CC",
              "Imports a .mid file's CC lane as automation points into this lane." },
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
    auto dir = AppPaths::appRoot();
    if (! dir.exists())
        dir.createDirectory();
    return dir.getChildFile ("keymap.xml");
}

void saveMappings (const juce::KeyPressMappingSet& set)
{
    // The rebind is already live in the mapping set, so there is no gesture to
    // abort here -- but a write that never lands reverts every shortcut on the
    // next launch, and silence there reads as "I must have forgotten to save".
    const auto f = getKeymapFile();
    auto x = std::unique_ptr<juce::XmlElement> (set.createXml (true));
    if (x == nullptr || ! x->writeTo (f))
        UserFileSave::showWriteFailure (f, "Your shortcuts work for now, but "
                                           "they will be back to the defaults "
                                           "next time you open the app.");
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
