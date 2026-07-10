#pragma once
#include <JuceHeader.h>
#include <atomic>

// D-4 typing-keyboard MIDI (QA-TransportDisplay, 2026-07-08).
// Shared between StandaloneEditor (the key->note converter) and the three
// grid key handlers (PianoRollGrid / DrumKitGrid / ArrangementGrid), which
// must let mapped note keys BUBBLE UP unhandled while the mode is on -
// otherwise the focused grid's single-letter tool shortcuts eat them before
// the editor can turn them into notes.
namespace TypingKeyboardMap
{
    // Written by StandaloneEditor::toggleTypingKeyboard (message thread);
    // read by the grid key handlers (message thread). Atomic only so a
    // future non-UI reader can't tear it.
    inline std::atomic<bool> gActive { false };

    // Two-row layout (Jeff's A1 pick, 2026-07-08): Z-row = lower octave with
    // S D G H J as its black keys; Q-row = upper octave with the number row
    // as its black keys. Returns the semitone offset from the base C, or -1.
    inline int semitoneForKey (int keyCode) noexcept
    {
        // Windows delivers unmodified-uppercase key codes, but match both
        // cases like the rest of the codebase (PianoRoll's `code || code+32`
        // convention) so a future non-Windows target doesn't silently lose
        // every white key.
        if (keyCode >= 'a' && keyCode <= 'z')
            keyCode -= ('a' - 'A');
        switch (keyCode)
        {
            // lower octave (base C)
            case 'Z': return 0;   case 'S': return 1;   case 'X': return 2;
            case 'D': return 3;   case 'C': return 4;   case 'V': return 5;
            case 'G': return 6;   case 'B': return 7;   case 'H': return 8;
            case 'N': return 9;   case 'J': return 10;  case 'M': return 11;
            // upper octave (base C + 12)
            case 'Q': return 12;  case '2': return 13;  case 'W': return 14;
            case '3': return 15;  case 'E': return 16;  case 'R': return 17;
            case '5': return 18;  case 'T': return 19;  case '6': return 20;
            case 'Y': return 21;  case '7': return 22;  case 'U': return 23;
            case 'I': return 24;  case '9': return 25;  case 'O': return 26;
            case '0': return 27;  case 'P': return 28;
            default:  return -1;
        }
    }

    inline bool isOctaveShiftKey (int keyCode) noexcept
    {
        return keyCode == juce::KeyPress::pageUpKey
            || keyCode == juce::KeyPress::pageDownKey;
    }

    // True when a page-local key handler should decline this event so it
    // reaches the editor's converter. Only BARE keys are notes - any
    // modifier means the user wants the normal shortcut (Ctrl+Z undo etc.).
    inline bool shouldBypassLocalKeys (const juce::KeyPress& k) noexcept
    {
        if (! gActive.load (std::memory_order_relaxed)) return false;
        const auto mods = k.getModifiers();
        if (mods.isCtrlDown() || mods.isAltDown() || mods.isShiftDown()) return false;
        const int kc = k.getKeyCode();
        return semitoneForKey (kc) >= 0 || isOctaveShiftKey (kc);
    }
}
