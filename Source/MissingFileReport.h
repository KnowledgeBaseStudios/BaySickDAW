#pragma once

#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
// MissingFileReport - QA-Export Task 5 (folded in from QA-Verify close).
//
// Several engines persist an ABSOLUTE path to an external file (a NAM capture,
// an sfizz kit, a user IR) and, on restore, quietly skip loading when the file
// is not where it was.  The failure is worse than silent: the control often
// still displays the remembered NAME, so the project presents as loaded while
// that engine renders nothing.  Move the samples folder, rename a file, or open
// the project on another machine and there is no signal at all.
//
// Restore sites call add() instead of skipping quietly; the project-load path
// drains once and shows the user a single list.  One dialog naming five missing
// files beats five dialogs, and beats none.
//
// Thread-safe: engine restores can run off the message thread during a project
// load, while the drain happens on the message thread.
// ─────────────────────────────────────────────────────────────────────────────
namespace MissingFileReport
{
    struct Entry
    {
        juce::String what;   // "NAM capture", "Guitar kit", "User IR", ...
        juce::String path;   // the stored path that did not resolve
    };

    namespace detail
    {
        inline juce::CriticalSection& lock()
        {
            static juce::CriticalSection l;
            return l;
        }
        inline std::vector<Entry>& entries()
        {
            static std::vector<Entry> e;
            return e;
        }
    }

    inline void add (const juce::String& what, const juce::String& path)
    {
        if (path.isEmpty()) return;
        const juce::ScopedLock sl (detail::lock());

        // A kit shared by three tabs should be reported once, not three times.
        for (const auto& e : detail::entries())
            if (e.path == path && e.what == what)
                return;

        detail::entries().push_back ({ what, path });
    }

    // Takes everything collected so far and clears the store.
    inline std::vector<Entry> drain()
    {
        const juce::ScopedLock sl (detail::lock());
        auto out = detail::entries();
        detail::entries().clear();
        return out;
    }

    inline void clear()
    {
        const juce::ScopedLock sl (detail::lock());
        detail::entries().clear();
    }

    inline bool isEmpty()
    {
        const juce::ScopedLock sl (detail::lock());
        return detail::entries().empty();
    }
}
