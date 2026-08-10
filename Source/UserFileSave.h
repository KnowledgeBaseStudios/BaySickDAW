#pragma once
#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
// UserFileSave - the one path for "write this to a file the user just named".
//
// Every save gesture that takes a typed name had grown its own copy of the same
// four steps: turn the name into a filename, resolve a collision, write, tell
// the user if it failed.  Successive QA rounds found the same defect in a
// different one of those copies each time - the write result discarded, so a
// save that never happened reports success and the gesture goes on to clear a
// dirty flag, rename a tab, or delete the tab it was told to delete AFTER the
// save.  The copies also disagreed with each other: most did not sanitize the
// typed name at all, and the ones with no collision check silently destroyed
// the user's existing file.
//
// THE RULE: a user-named write calls writeTextAsync or writeXmlAsync rather
// than spelling those four steps again, so there is one behavior and one piece
// of wording to get right.  A gesture whose write genuinely fits neither shape
// still takes its naming from resolveTarget.  The wording ("Save failed" /
// "Couldn't write <path>.") is the pre-existing majority spelling, kept
// verbatim - it is what the user reads when their work fails to save.
//
// WHY THE WRITES ARE ASYNC: a collision now ASKS rather than deciding.  Silent
// overwrite destroyed the user's existing preset; the auto-suffix that replaced
// it was no better once the save dialogs began pre-filling the currently loaded
// name, because re-saving a patch you already named produced "Kick Tight (2)",
// renamed the tab to match, and gave you no way to ever update in place - every
// re-save added another number.  So a real collision raises Replace / Save a
// Copy / Cancel, JUCE modals are async, and a call that may prompt therefore
// cannot hand its result back on return.  Every caller is already inside the
// modal callback that produced the name, so this is one more hop of a shape
// they are in already.  The three outcomes are NOT interchangeable - see
// Outcome below.
//
// Deliberately NO adoption census here.  A count in this comment goes stale the
// moment a site is added or converted, and a stale one reads as "the rest are
// already covered" - which is how the previous version of this header ended up
// claiming a conversion that had not happened.  `grep -rn "UserFileSave::"
// Source/` lists who routes through it.  For the reverse - a typed name still
// turned into a path by hand - use
// `grep -rnE "getChildFile *\( *(name|safeName|presetName)" Source/`, which
// also turns up a few legitimate non-typed-name uses, so every hit needs a
// look.  Its predecessor grepped for resolveTarget's own `while
// (target.exists())` loop, which no non-adopter has ever contained: it matched
// only this file, so an empty result read as "everything is converted".
// ─────────────────────────────────────────────────────────────────────────────
namespace UserFileSave
{
    // The page-preset family's trailing sentence, for the saves that are the
    // first half of a "Save Page Preset & Delete" gesture.  Named so the
    // adopters cannot each spell it a different way.
    inline constexpr const char* kTabNotDeleted = "The tab was not deleted.";

    enum class Outcome
    {
        // The file named by Result::file is on disk.  Run the rest of the
        // gesture: clear the dirty flag, rename the tab, take the snapshot,
        // fire the chained delete.
        Succeeded,

        // Nothing was written and the user has ALREADY been shown why.  Abort
        // silently - a second box just repeats the first.
        Failed,

        // The user chose Cancel at the collision prompt.  Abort with NO error:
        // this is a deliberate choice, not a failure, and reporting it as one
        // puts a "Save failed" box on a decision the user made on purpose.
        Cancelled
    };

    struct Result
    {
        Outcome    outcome = Outcome::Failed;

        // What was actually written.  Replace and Save a Copy land on different
        // paths, so a caller that names anything after the save (a tab, a menu
        // entry) must read it from here rather than re-deriving
        // dir/<typed name>.  Empty on every outcome except Succeeded.
        juce::File file;

        bool succeeded() const noexcept { return outcome == Outcome::Succeeded; }
        bool failed()    const noexcept { return outcome == Outcome::Failed; }
        bool cancelled() const noexcept { return outcome == Outcome::Cancelled; }

        explicit operator bool() const noexcept { return succeeded(); }
    };

    // LIFETIME - read this before writing a completion callback.
    //
    // A collision gates the write behind a modal, so this can run long after
    // the call that scheduled it returned, by which time the user has had time
    // to close the window the gesture came from.  Capturing a bare `this` is a
    // use-after-free.  A Component caller (which is nearly all of them) must
    // capture a juce::Component::SafePointer and RE-TEST it inside this
    // callback - the test the naming callback already did proves nothing about
    // the moment the prompt is answered.  A non-Component owner captures a
    // std::weak_ptr token and locks it here.
    //
    // Always dispatched on the message thread.  An empty callback is legal for
    // a gesture with nothing left to do after the write.
    using Callback = std::function<void (const Result&)>;

    namespace detail
    {
        inline juce::String legalStem (const juce::String& typedName)
        {
            // createLegalFileName REMOVES the illegal characters rather than
            // substituting for them, so "Lead 1/2" collapses to "Lead 12" and a
            // name that was nothing but separators collapses to nothing at all.
            // Trailing whitespace survives it, and Windows will not let the user
            // delete a file whose name has any, so trim afterwards.
            return juce::File::createLegalFileName (typedName).trim();
        }
    }

    // The first free name: dir/<typed name> if nothing is there, otherwise the
    // " (2)", " (3)" suffix that Save a Copy lands on.  Empty File when the
    // typed name has no legal characters left in it.  Does NOT create dir and
    // touches nothing on disk - the write functions below create it before
    // calling this, and one caller needs to test a name before doing other
    // work.
    inline juce::File resolveTarget (const juce::File&   dir,
                                     const juce::String& typedName,
                                     const juce::String& extension = ".xml")
    {
        const juce::String safe = detail::legalStem (typedName);
        if (safe.isEmpty())
            return {};

        auto target = dir.getChildFile (safe + extension);
        int n = 2;
        while (target.exists())
            target = dir.getChildFile (safe + " (" + juce::String (n++) + ")" + extension);

        return target;
    }

    // The one spelling of the unusable-name text, for the out-param save shape
    // that has to hand its reason back instead of raising a box.  The character
    // list is the full set juce::File::createLegalFileName removes; listing a
    // subset sent the user back to a name that still sanitized away to nothing.
    inline juce::String unusableNameMessage (const juce::String& typedName)
    {
        return juce::String ("\"") + typedName
                 + "\" can't be used as a file name.  "
                   "Type one without / \\ : ; , ? * # @ ^ | < > or quote marks "
                   "in it.";
    }

    // Exposed for a write the two shapes below cannot do: one whose file was
    // never a typed name at all - a keymap, an undo snapshot.  Those are its
    // only callers, and they raise the family's message rather than a fresh
    // variant of it.
    //
    // NOT for the out-param shape (a save that reports through an out-param to
    // a caller which raises its own box).  That shape takes its naming from
    // resolveTarget and its unusable-name text from unusableNameMessage, and
    // must leave the box to its caller - raising one here as well stacks two
    // dialogs on a single failure.
    inline void showWriteFailure (const juce::File&   target,
                                  const juce::String& failureTail = {})
    {
        juce::String msg = "Couldn't write " + target.getFullPathName() + ".";
        if (failureTail.isNotEmpty())
            msg << "  " << failureTail;

        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon, "Save failed", msg, "OK");
    }

    namespace detail
    {
        // `write` returns false for "nothing landed on disk", whatever the
        // reason - the caller of this file never learns which, because the one
        // message covers them all.
        using WriteFn = std::function<bool (const juce::File&)>;

        inline void completeWrite (const juce::File&   target,
                                   const WriteFn&      write,
                                   const juce::String& failureTail,
                                   const Callback&     onDone)
        {
            if (! write (target))
            {
                showWriteFailure (target, failureTail);
                if (onDone) onDone (Result { Outcome::Failed, {} });
                return;
            }

            if (onDone) onDone (Result { Outcome::Succeeded, target });
        }

        inline void startWrite (const juce::File&   dir,
                                const juce::String& typedName,
                                const juce::String& extension,
                                WriteFn             write,
                                const juce::String& failureTail,
                                Callback            onDone)
        {
            dir.createDirectory();

            // An unusable name is a distinct outcome from a failed write:
            // there is no path to name in the message and the user can fix it
            // by retyping.
            const juce::String safe = legalStem (typedName);
            if (safe.isEmpty())
            {
                const juce::String msg = typedName.trim().isEmpty()
                                            ? juce::String ("Type a name for it first.")
                                            : unusableNameMessage (typedName);

                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon, "Save failed", msg, "OK");

                if (onDone) onDone (Result { Outcome::Failed, {} });
                return;
            }

            // No collision: write now.  An ordinary save must never make the
            // user confirm anything.
            const auto exact = dir.getChildFile (safe + extension);
            if (! exact.exists())
            {
                completeWrite (exact, write, failureTail, onDone);
                return;
            }

            const auto copy = resolveTarget (dir, typedName, extension);

            juce::String msg;
            msg << "\"" << exact.getFileName() << "\" is already saved in "
                << dir.getFullPathName() << ".\n\n"
                << "Replace it with what you have now, or save a copy named \""
                << copy.getFileName() << "\"?";

            auto* aw = new juce::AlertWindow ("Name Already Used", msg,
                                              juce::MessageBoxIconType::QuestionIcon);
            aw->addButton ("Replace",     1);
            aw->addButton ("Save a Copy", 2);
            aw->addButton ("Cancel",      0, juce::KeyPress (juce::KeyPress::escapeKey));

            aw->enterModalState (true, juce::ModalCallbackFunction::create (
                [exact, copy, write, failureTail, onDone] (int choice)
                {
                    // Anything that is not one of the two write choices is a
                    // Cancel - escape, and the degenerate path where no dialog
                    // could be raised at all.  Cancel writes nothing.
                    if (choice != 1 && choice != 2)
                    {
                        if (onDone) onDone (Result { Outcome::Cancelled, {} });
                        return;
                    }

                    completeWrite (choice == 1 ? exact : copy, write, failureTail, onDone);
                }), true);
        }
    }

    inline void writeTextAsync (const juce::File&   dir,
                                const juce::String& typedName,
                                const juce::String& contents,
                                Callback            onDone,
                                const juce::String& failureTail = {},
                                const juce::String& extension   = ".xml")
    {
        detail::startWrite (dir, typedName, extension,
            [contents] (const juce::File& target)
            {
                // Empty contents are a failure, not a successful empty file:
                // every exporter in the tree returns "" for "I could not build
                // this", and the guarded sites already fold both outcomes into
                // this one message.
                return contents.isNotEmpty() && target.replaceWithText (contents);
            },
            failureTail, std::move (onDone));
    }

    inline void writeXmlAsync (const juce::File&       dir,
                               const juce::String&     typedName,
                               const juce::XmlElement& xml,
                               Callback                onDone,
                               const juce::String&     failureTail = {},
                               const juce::String&     extension   = ".xml")
    {
        // Deep-copied because the write can outlive this call by a whole modal.
        // Callers build the element on the stack, or in a unique_ptr that dies
        // with the naming callback, so a captured reference would dangle.
        auto owned = std::make_shared<juce::XmlElement> (xml);

        detail::startWrite (dir, typedName, extension,
            [owned] (const juce::File& target) { return owned->writeTo (target, {}); },
            failureTail, std::move (onDone));
    }
}
