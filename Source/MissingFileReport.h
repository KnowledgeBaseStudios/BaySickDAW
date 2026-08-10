#pragma once

#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
// MissingFileReport - QA-Export Task 5 (folded in from QA-Verify close).
//
// Several engines persist an ABSOLUTE path to an external file (a NAM capture,
// an sfizz kit, a user IR) and, on restore, quietly skip the load when the file
// is not where it was -- or quietly substitute a default.  The failure is worse
// than silent: the control often still displays the remembered NAME, so the
// project presents as loaded while that engine either renders nothing or plays
// something the user never picked.  Move the samples folder, rename a file, or
// open the project on another machine and there is no signal at all.
//
// Restore sites call add() instead of skipping quietly; a load drains at its
// tail and shows the user a single list.  One dialog naming five missing files
// beats five dialogs, and beats none.
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

        // What the queued dialog will name, and the headline noun it will use.
        // Separate from entries() so a drain that lands while a dialog is
        // already queued adds to that dialog instead of raising a second one.
        inline std::vector<Entry>& pending()
        {
            static std::vector<Entry> p;
            return p;
        }
        inline juce::String& pendingNoun()
        {
            static juce::String n;
            return n;
        }

        // Owned by the post that set it: showPending clears it when it runs.
        // Anything else resetting it would let the next report queue a second
        // dialog behind the first.
        inline bool& dialogQueued()
        {
            static bool q = false;
            return q;
        }

        // Message thread only, and deliberately outside lock(): it tracks the
        // depth of a UI gesture stack, not the store.  add() runs off the
        // message thread during a project load; ScopedGesture never does.
        inline bool& gestureOpen()
        {
            static bool g = false;
            return g;
        }

        // Message thread (the callAsync reportIfAny posts).
        //
        // One string covers two outcomes: an engine that skipped the load and
        // renders nothing, and one that substituted a default (a kit, a
        // program) and is audibly playing an instrument the user never picked.
        // The wording has to be true of both -- there is no second dialog.
        inline void showPending()
        {
            std::vector<Entry> batch;
            juce::String       noun;
            {
                const juce::ScopedLock sl (lock());
                batch.swap (pending());
                noun = pendingNoun();
                pendingNoun().clear();
                // Cleared before the box goes up, so anything reported from
                // here on gets its own dialog rather than being swallowed.
                dialogQueued() = false;
            }

            if (batch.empty())
                return;

            juce::String msg = "This " + noun
                               + " refers to files that are no longer where they were saved.\n"
                                 "The affected parts loaded without them, so they may be silent\n"
                                 "or may be playing a substitute:\n\n";
            const int shown = juce::jmin (12, (int) batch.size());
            for (int i = 0; i < shown; ++i)
                msg << "  " << batch[(size_t) i].what << ": " << batch[(size_t) i].path << "\n";
            if ((int) batch.size() > shown)
                msg << "  ...and " << ((int) batch.size() - shown) << " more\n";
            msg << "\nRe-pick them on the relevant tab, or put the files back.";

            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon, "Missing files", msg, "OK");
        }
    }

    // House convention for "we are presenting a name we did not actually
    // load".  NOT the single source yet: NAMPedalStyleDSP::getModelName, the
    // BaySickNAM/IR file labels, PluginsPage's missingSuffix, the effect-rack
    // slot row and the Recent Projects menu each hardcode this same literal
    // and none of them reference the constant.  RibbonTabBar's substituted-kit
    // marker is its only consumer, so editing the wording here changes ONE of
    // six surfaces; pointing the other five at it is outstanding.
    inline constexpr const char* kNameSuffix = " (missing)";

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

    inline bool isEmpty()
    {
        const juce::ScopedLock sl (detail::lock());
        return detail::entries().empty();
    }

    // True while a ScopedGesture is open.  For a drain that is NOT a gesture
    // tail -- a producer that banks entries as a side effect of whatever the
    // user is actually doing -- so it can hold off and let the gesture that IS
    // running own the report and the headline noun.  Message thread only, same
    // as the flag it reads.
    inline bool isGestureOpen() noexcept { return detail::gestureOpen(); }

    // Drain-and-show.  Lives here rather than only on the processor because the
    // dialog belongs to whatever finished loading, and an effect-preset load is
    // driven from UI surfaces that hold no processor reference at all -- a
    // second copy of this text at those call sites would drift from the one the
    // project path shows.  sourceNoun names what was loaded ("project",
    // "template", "preset").
    //
    // COALESCING, not one dialog per call: opening a project drains at more
    // than one tail -- the restore walker's and the arrangement clip bank's --
    // and two drains in the same message-loop turn used to stack two dialogs on
    // the user.  Dropping a drain instead would re-open the silence that drain
    // exists to close, so each call folds its entries into one pending batch and
    // only the first posts the dialog: everything reported before that post runs
    // arrives in one list.  One batch gets one headline, so the first noun in
    // wins -- naming both would tell the user nothing they can act on.
    inline void reportIfAny (const juce::String& sourceNoun)
    {
        if (isEmpty())
            return;

        auto fresh = drain();
        bool postDialog = false;
        {
            const juce::ScopedLock sl (detail::lock());
            auto& p = detail::pending();
            p.insert (p.end(), fresh.begin(), fresh.end());
            if (detail::pendingNoun().isEmpty())
                detail::pendingNoun() = sourceNoun;

            postDialog = ! detail::dialogQueued() && ! p.empty();
            if (postDialog)
                detail::dialogQueued() = true;
        }

        if (postDialog)
            juce::MessageManager::callAsync ([] { detail::showPending(); });
    }

    // RAII drain.  reportIfAny closes one half of the problem (two drains in one
    // turn stacking two dialogs); this closes the other half - a gesture that
    // reaches add() and never drains at all, so its entry sits in the store and
    // surfaces later under an unrelated gesture wearing that gesture's noun.
    // Three QA rounds turned up a new one of those each time, which is what a
    // hand-written drain at every tail buys you.  Declare one of these at the
    // top of the gesture instead and it cannot be forgotten or doubled up.
    //
    // Nest-aware the same way the processor's project-load shield is - save the
    // "was one already open" flag and restore it, not a refcount - so an inner
    // gesture cannot drain early and steal the outer one's entries.  Only the
    // outermost scope drains, and its noun is the one the user reads: same
    // "first noun in wins" rule reportIfAny applies to a coalesced batch.
    //
    // THE FLAG GATES ScopedGesture DESTRUCTORS AND NOTHING ELSE.  A direct
    // reportIfAny inside an open scope - including VibeSynthProcessor::
    // reportMissingFilesIfAny, which forwards straight to it - drains there and
    // then and claims pendingNoun, so the OUTER gesture's entries get reported
    // under the INNER one's headline.  Coalescing still gets the user a single
    // dialog; it is just titled wrong, and the outer scope has nothing left to
    // name.  Nesting only means something if the inner drain is a scope too, or
    // holds off on isGestureOpen().
    //
    // Inert when nothing was banked - reportIfAny returns on an empty store.
    class ScopedGesture
    {
    public:
        explicit ScopedGesture (const juce::String& sourceNoun)
            : mNoun (sourceNoun), mOuterWasOpen (detail::gestureOpen())
        {
            detail::gestureOpen() = true;
        }

        ~ScopedGesture()
        {
            detail::gestureOpen() = mOuterWasOpen;
            if (! mOuterWasOpen)
                reportIfAny (mNoun);
        }

        ScopedGesture (const ScopedGesture&)            = delete;
        ScopedGesture& operator= (const ScopedGesture&) = delete;

    private:
        const juce::String mNoun;
        const bool         mOuterWasOpen;
    };
}
