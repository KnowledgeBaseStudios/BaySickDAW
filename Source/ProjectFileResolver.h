#pragma once

#include <JuceHeader.h>
#include <functional>
#include "SampleLibrary.h"

// ─────────────────────────────────────────────────────────────────────────────
// ProjectFileResolver - the reader half of the project-relative reference form.
//
// A self-contained bundle copies every externally-referenced file into
// <project>\Samples and rewrites the saved references to "Samples/<name>".
// BaySickDAWProcessor::resolveProjectFile is the ONE reader that understands that
// form; it also passes absolute paths straight through and hands
// "library:" / "mysamples:" references to SampleLibrary.  Every other engine and
// effect restore built a bare juce::File from the stored string instead, which
// resolves a relative path against the PROCESS WORKING DIRECTORY -- a folder
// with nothing to do with the project -- so a bundle opened with none of its
// captures, IRs or kits.
//
// The processor installs its resolver here; restore paths call resolve().  A
// header-only namespace rather than a per-object hook for the same reason
// MissingFileReport is one: an effect's setStateInformation is reached through
// an EffectRack that no processor reference passes through, so the hook
// alternative means threading a resolver down every rack, pedal board and Inst
// chain that can own a slot, plus every surface that loads an effect preset.
//
// Ownership / threading: one BaySickDAWProcessor exists per app; it installs on
// project-folder change and the installed callable outlives every reader.
// Engine restores can run off the message thread during a project load while the
// message thread installs, so the slot is lock-guarded; the callable is COPIED
// out before being invoked so this lock is never held across the processor's own
// project-folder lock.
// ─────────────────────────────────────────────────────────────────────────────
namespace ProjectFileResolver
{
    using Resolver = std::function<juce::File (const juce::String&)>;

    namespace detail
    {
        inline juce::CriticalSection& lock()
        {
            static juce::CriticalSection l;
            return l;
        }
        inline Resolver& resolver()
        {
            static Resolver r;
            return r;
        }
    }

    inline void install (Resolver r)
    {
        const juce::ScopedLock sl (detail::lock());
        detail::resolver() = std::move (r);
    }

    // Falls back to the stable-root reader when nothing is installed -- startup
    // before any project folder exists, and the tooling targets that build these
    // sources without a processor.  That fallback is exactly what these call
    // sites did before, so an uninstalled resolver changes nothing.
    inline juce::File resolve (const juce::String& storedPath)
    {
        if (storedPath.isEmpty()) return {};

        Resolver r;
        {
            const juce::ScopedLock sl (detail::lock());
            r = detail::resolver();
        }

        if (r) return r (storedPath);
        return SampleLibrary::resolvePersistedRef (storedPath);
    }
}
