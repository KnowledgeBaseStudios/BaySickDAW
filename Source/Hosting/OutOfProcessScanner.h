#pragma once
#include <JuceHeader.h>
#include "PluginBridgeProtocol.h"
#include "../SafeXml.h"

namespace Hosting
{
// ─────────────────────────────────────────────────────────────────────────────
// OutOfProcessScanner - scans one plugin file in a helper process.
//
// ADDED 2026-08-10 (QA-Cleanup, CL-289 Tier-1 part 3 follow-on).
//
// The scan used to run IN THIS PROCESS, on a background thread
// (PluginManager::run).  That was wrong in two independent ways:
//
// 1. CONTRACT.  JUCE asserts the VST3 scan path must run on the MESSAGE thread
//    - juce_VST3PluginFormatImpl.h:299, `JUCE_ASSERT_MESSAGE_THREAD`, with the
//    comment "The VST3 spec requires that IComponent::setupProcessing() is
//    called on the message thread.  If you call it from a different thread,
//    some plugins may break."  We were breaking it on every scan.  The assert
//    compiles out in Release, which is the only reason it went unnoticed until
//    a Debug run caught it.
//
// 2. ROBUSTNESS.  Scanning LOADS the plugin.  A plugin that crashes while being
//    scanned took the entire DAW down with it, losing whatever the user had
//    open.  That is not hypothetical for a first-time-user app that will be
//    pointed at whatever plugin folder the machine happens to have.
//
// Out of process fixes both: the helper runs the scan on ITS message thread,
// and it is allowed to die.  A crash shows up here as a lost connection, the
// file gets blacklisted through JUCE's existing dead-man's-pedal file, and the
// scan moves on to the next plugin.
//
// ONE HELPER PER FILE, deliberately.  Reusing a single helper across the whole
// scan would be faster, but one bad plugin could then leave the shared helper
// in a state that corrupts every subsequent result - and the crash-isolation
// property, which is the entire point, would be lost after the first casualty.
// ─────────────────────────────────────────────────────────────────────────────
class OutOfProcessScanner final : public juce::ChildProcessCoordinator
{
public:
    OutOfProcessScanner() = default;

    ~OutOfProcessScanner() override
    {
        killWorkerProcess();
    }

    // Returns false only when the helper could not be started at all - that is
    // a broken installation, not a broken plugin, and the caller falls back to
    // an in-process scan rather than refusing to find anything.
    bool scan (const juce::File& helperExe,
               const juce::String& pathToScan,
               juce::OwnedArray<juce::PluginDescription>& results,
               int timeoutMs)
    {
        if (! helperExe.existsAsFile())
            return false;

        mDone.store (false, std::memory_order_release);
        mXml.clear();

        if (! launchWorkerProcess (helperExe, Bridge::kBridgeUid, timeoutMs))
            return false;

        const auto utf8 = pathToScan.toRawUTF8();
        sendMessageToWorker (Bridge::frame (Bridge::MessageType::ScanFile, 0,
                                            nullptr, 0,
                                            utf8, (std::uint32_t) std::strlen (utf8)));

        // A plugin that hangs its scan is as bad as one that crashes it, so the
        // wait is bounded rather than open-ended.  Polled rather than a
        // WaitableEvent because the reply arrives on the coordinator's own
        // thread and the caller may be asked to abort mid-scan.
        const auto deadline = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;

        while (! mDone.load (std::memory_order_acquire))
        {
            if (juce::Time::getMillisecondCounter() > deadline)
                break;

            juce::Thread::sleep (5);
        }

        killWorkerProcess();

        if (! mDone.load (std::memory_order_acquire))
            return true;   // helper started but never answered: treat as "no types found"

        // Through SafeXml, not juce::parseXML.  This XML is built by the helper
        // from a third-party plugin binary's own name / manufacturer / category
        // strings, so it is untrusted by this batch's own definition - and it was
        // the single raw parse left in the tree.
        if (auto root = SafeXml::parse (mXml))
            for (auto* e : root->getChildIterator())
                if (auto d = std::make_unique<juce::PluginDescription>(); d->loadFromXml (*e))
                    results.add (d.release());

        return true;
    }

private:
    void handleMessageFromWorker (const juce::MemoryBlock& mb) override
    {
        if (mb.getSize() < sizeof (Bridge::Header))
            return;

        Bridge::Header h {};
        std::memcpy (&h, mb.getData(), sizeof (h));

        if (static_cast<Bridge::MessageType> (h.type) != Bridge::MessageType::ScanResult)
            return;

        const auto* body = static_cast<const char*> (mb.getData()) + sizeof (h);
        const auto  n    = (int) (mb.getSize() - sizeof (h));

        mXml = juce::String::fromUTF8 (body, n);
        mDone.store (true, std::memory_order_release);
    }

    // The helper died mid-scan - which is the case this class exists for.  Wake
    // the waiter with no results; JUCE's dead-man's pedal blacklists the file.
    void handleConnectionLost() override
    {
        mDone.store (true, std::memory_order_release);
    }

    std::atomic<bool> mDone { false };
    juce::String      mXml;
};

// ─────────────────────────────────────────────────────────────────────────────
// BridgedPluginScanner - the KnownPluginList hook that routes every scan
// through a helper.  Installed on the list PluginDirectoryScanner walks.
// ─────────────────────────────────────────────────────────────────────────────
class BridgedPluginScanner final : public juce::KnownPluginList::CustomScanner
{
public:
    // `helperFor` maps a plugin path to the right-architecture helper exe: a
    // 32-bit plugin can only be scanned by the 32-bit helper, same reason it can
    // only be LOADED by one.
    explicit BridgedPluginScanner (std::function<juce::File (const juce::File&)> helperFor)
        : mHelperFor (std::move (helperFor)) {}

    bool findPluginTypesFor (juce::AudioPluginFormat& format,
                             juce::OwnedArray<juce::PluginDescription>& result,
                             const juce::String& fileOrIdentifier) override
    {
        if (shouldExit())
            return true;

        const juce::File pluginFile (fileOrIdentifier);
        const auto helper = mHelperFor ? mHelperFor (pluginFile) : juce::File();

        if (helper.existsAsFile())
        {
            OutOfProcessScanner scanner;

            if (scanner.scan (helper, fileOrIdentifier, result, kScanTimeoutMs))
                return true;
        }

        // Helper missing or unstartable = broken install, not a broken plugin.
        // Falling back keeps the app usable rather than reporting that the
        // machine has no plugins at all.
        format.findAllTypesForFile (result, fileOrIdentifier);
        return true;
    }

private:
    // Generous: a large sample-library instrument can take a while to answer
    // its first scan, and a false timeout looks identical to a missing plugin.
    static constexpr int kScanTimeoutMs = 30000;

    std::function<juce::File (const juce::File&)> mHelperFor;
};
}
