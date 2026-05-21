#pragma once
#include <JuceHeader.h>

// ── KeyBindings ──────────────────────────────────────────────────────────────
// Central catalog of every editable keyboard shortcut in BaySickDAW.
//
// Each command has:
//   - a stable integer ID (the CommandIDs enum below)
//   - a Category (which tab in the Key Binds popup it lives under)
//   - a human-readable Name shown in the editor row
//   - a beginner-friendly Tooltip string shown on hover
//   - a default KeyPress (the out-of-the-box shortcut)
//
// Mouse-modifier interactions (Alt+RClick etc.) are surfaced as MouseRefRow
// entries - they're documentation-only and not editable through the popup.
//
// Phase A scope: only commands whose handlers were already wired before this
// refactor.  Phase B and beyond extend the catalog as features come online.
// ─────────────────────────────────────────────────────────────────────────────
namespace BSCommands
{
    // App-defined IDs start above 0xf000 to stay clear of JUCE's reserved range.
    enum CommandIDs : int
    {
        // ── General - transport ─────────────────────────────────────────────
        cmdPlayPause       = 0x10001,
        cmdStopAndDisarm   = 0x10002,
        cmdToggleRecord    = 0x10003,

        // ── General - page switches (Phase B-1, 2026-04-26) ─────────────────
        cmdShowMixer       = 0x10010,
        cmdShowEffects     = 0x10011,
        cmdShowBuilder     = 0x10012,
        cmdShowLayers      = 0x10013,
        cmdShowBass        = 0x10014,
        cmdShowDrums       = 0x10015,
        cmdShowPianoRoll   = 0x10016,

        // ── General - file operations (Phase B-1) ───────────────────────────
        cmdFileNew         = 0x10020,
        cmdFileOpen        = 0x10021,
        cmdFileSave        = 0x10022,
        cmdFileSaveAs      = 0x10023,

        // ── General - pattern navigation (Phase B-2, 2026-04-26) ────────────
        cmdRenameActivePattern = 0x10030,
        cmdNextEmptyPattern    = 0x10031,
        cmdNewPattern          = 0x10032,
        cmdNextPattern         = 0x10033,
        cmdPrevPattern         = 0x10034,

        // ── General - transport extensions (Phase B-3, 2026-04-26) ──────────
        cmdToggleSongMode      = 0x10040,
        cmdSeekHome            = 0x10041,
        cmdFastForward         = 0x10042,
        cmdPrevBarSong         = 0x10043,
        cmdNextBarSong         = 0x10044,
        cmdToggleMetronome     = 0x10045,

        // ── General - undo / redo (Phase B-5, 2026-04-26) ───────────────────
        cmdGlobalUndo          = 0x10050,
        cmdGlobalRedo          = 0x10051,

        // ── General - recording precount (Phase D-5, 2026-04-26) ────────────
        cmdToggleRecordingPrecount = 0x10060,

        // ── Builder - Slip / Stretch edit-mode toggle (QA-Ea Task 0c).
        // Flips the BuilderPage toolbar's Slip/Stretch dropdown between
        // its two modes.  Slip mode: Audio-clip edge drags slip the
        // content (reveal pre-roll / trim leading).  Stretch mode: time-
        // stretch (QA-Ec ships it; left-edge no-op for Task 0c, right-edge
        // keeps existing length-resize UX).  Default keybind 'S'.
        cmdToggleSlipStretchMode = 0x10070,

        // (later phases continue from 0x10071.)
    };

    enum class Category : int
    {
        General        = 0,
        Builder        = 1,
        PianoRoll      = 2,
        DrumKit        = 3,    // 2026-04-26 (D-7 sub-2): drum-kit specifics
        MouseReference = 4,
    };

    juce::String categoryName (Category c);

    struct CommandInfo
    {
        int            id;
        Category       category;
        juce::String   name;
        juce::String   tooltip;
        juce::KeyPress defaultKey;
    };

    // Catalog of editable commands.  Static - initialized on first call.
    const std::vector<CommandInfo>& getAllCommands();

    // Look up a single command by id.  Returns nullptr if id isn't in the catalog.
    const CommandInfo* findCommand (int id);

    // Return all commands belonging to a given category.
    std::vector<const CommandInfo*> getCommandsInCategory (Category c);

    // Mouse-modifier reference rows (documentation only - non-editable).
    struct MouseRefRow
    {
        Category     category;
        juce::String shortcut;
        juce::String name;
        juce::String tooltip;
    };
    const std::vector<MouseRefRow>& getMouseRefRows();
    std::vector<const MouseRefRow*> getMouseRefsInCategory (Category c);

    // Phase B-6 (2026-04-26): scan reference rows for any hardcoded key that
    // matches the given KeyPress.  Returns rows whose shortcut text parses as
    // a valid KeyPress and matches `kp`.  Mouse-only rows ("Ctrl + Drag" etc.)
    // never match because their shortcut text doesn't parse as a key.  Used
    // by the Set-binding capture flow to warn when the new binding clashes
    // with a page-local handler (which fires before the command system).
    std::vector<const MouseRefRow*> findHardcodedConflicts (const juce::KeyPress& kp);

    // ── Persistence ─────────────────────────────────────────────────────────
    // Keymap XML lives next to audio_settings.xml in <Documents>/BaySickDAW/.
    juce::File getKeymapFile();

    // Write current mappings to keymap.xml.  Called whenever the user edits a
    // shortcut in the popup.  Safe to call during editing - JUCE buffers writes.
    void saveMappings (const juce::KeyPressMappingSet& set);

    // Restore mappings from disk.  Returns false if the file doesn't exist or
    // failed to parse - caller should leave defaults in place when false.
    bool loadMappings (juce::KeyPressMappingSet& set);
}
