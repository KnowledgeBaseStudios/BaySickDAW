#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <utility>
#include <vector>
#include "SampleLibrary.h"

// -----------------------------------------------------------------------------
// CoreLibraryInstaller - content delivery for the Core Library (2026-08-09)
//
// The ten content packs are the difference between a working app and an app
// where every sampled instrument comes up silently empty.  First-run
// housekeeping creates an EMPTY CoreLibrary folder so the Sample Library
// shortcut has somewhere to point; this is the half that fills it.
//
// TRANSPORT: one plain HTTPS GET per release asset, then a local unzip.
//
// Why not a git clone (which is what this used to be): the content repository
// stores the archives under Git LFS, so a clone pulls the real bytes through
// LFS bandwidth, which is metered per account and would start failing testers
// part-way through a four gigabyte fetch.  Release-asset downloads are served
// from object storage with no such quota, they need no git on the user's
// machine, and they are addressable one pack at a time.
//
// PER-PACK GRANULARITY is the whole design.  Each asset is downloaded,
// unpacked, verified and recorded on its own, so:
//   - a pack that fails is the only thing retried, never the other nine;
//   - a re-run costs ten directory checks and fetches only what is absent;
//   - the progress bar is weighted by the real byte sizes below, so it does
//     not lurch.
//
// RESUMABLE AND CRASH-SAFE, in three layers:
//   1. Bytes land in "<asset>.part" and are only renamed to "<asset>" once the
//      file is exactly the expected length.  A part file is resumed by HTTP
//      byte range on the next run, so a 1 GB pack that dies at 900 MB picks up
//      at 900 MB rather than at zero.
//   2. A pack is unpacked in a staging folder that carries an in-progress
//      marker (SampleLibrary::getPackInstallMarkerFile) from before the first
//      file is written.  The marker travels with the folder into CoreLibrary
//      and is deleted last, so a pack interrupted at ANY point in between reads
//      as incomplete rather than as content - the same discipline
//      SampleLibrary::adoptIntoUserSamples uses for My Samples.
//   3. The library-wide marker (SampleLibrary::getCoreContentMarkerFile) is
//      planted before the first write and removed only when all ten packs are
//      in place, so an interrupted run is visible at the next launch.
//
// The installed folder names are NOT hardcoded.  An archive that carries its
// own top-level folder installs under that folder's name, and which asset
// produced which folder is recorded in the manifest.  SampleLibrary enumerates
// whatever is on disk and classifies it by substring (SampleLibrary::
// isDrumPack), so nothing downstream needs a list either.
// -----------------------------------------------------------------------------
namespace CoreLibraryInstaller
{
    // =========================================================================
    // THE CONTENT SOURCE.  Single point of change for where content comes from.
    // The asset names are already sanitized ASCII with no spaces, so the URL is
    // a plain concatenation with no percent-encoding.
    // =========================================================================
    inline constexpr const char* kReleaseBaseUrl =
        "https://github.com/KnowledgeBaseStudios/BaySickDAW-Downloads"
        "/releases/download/Content-v1/";

    struct Asset
    {
        const char* fileName;
        juce::int64 bytes;      // exact, read back from the release; never an estimate
        // The folder this pack MUST install as.  Not derivable from fileName:
        // GitHub sanitizes asset names, so "Black&Blue Basses" uploaded as
        // "Black.Blue.Basses.zip" and a dots-to-spaces guess yields
        // "Black Blue Basses" - a folder the sfizz kit paths do not resolve
        // against.  Verified against the installed CoreLibrary on disk.
        const char* folderName;
    };

    // Sizes are the published asset lengths and are load-bearing twice: they
    // weight the progress bar, and a finished download that is not EXACTLY this
    // long is rejected rather than unpacked.
    inline const std::vector<Asset>& assets()
    {
        static const std::vector<Asset> a
        {
            { "Big.Rusty.Drums.zip",       619062262, "Big Rusty Drums"       },
            { "Black.Blue.Basses.zip",    1008690586, "Black&Blue Basses"     },
            { "Black.Green.Guitars.zip",   481838512, "Black&Green Guitars"   },
            { "Brass.Package.zip",         479804303, "Brass Package"         },
            { "EDM.Drums.Package.zip",      37364606, "EDM Drums Package"     },
            { "Hip.Hop.Drums.Package.zip",  63700833, "Hip Hop Drums Package" },
            { "Keys.Package.zip",          382424944, "Keys Package"          },
            { "Percussion.Package.zip",    145628096, "Percussion Package"    },
            { "Strings.Package.zip",       537952770, "Strings Package"       },
            { "Woodwinds.Package.zip",     283777305, "Woodwinds Package"     }
        };
        return a;
    }

    // The folder an asset installs as when its archive does not carry one of
    // its own, and the name shown to the user either way.
    inline juce::String packDisplayName (const Asset& a)
    {
        return juce::String (juce::CharPointer_UTF8 (a.folderName));
    }

    // How long the connection may sit with no NEW BYTES before it is treated as
    // dead.  Driven by observed progress, never by any other signal: a healthy
    // link that is merely slow keeps moving the byte counter, so it is never
    // killed, while a connection that has genuinely gone away moves nothing.
    inline constexpr double kStallTimeoutMs    = 120000.0;
    inline constexpr int    kConnectTimeoutMs  = 30000;
    inline constexpr int    kChunkBytes        = 262144;

    // The packs are compressed audio, which decompresses to close to its
    // archived size; the margin is slack, not a measurement.  Peak disk use is
    // everything unpacked plus ONE archive, since each archive is deleted as
    // soon as its own pack is unpacked.
    inline constexpr double kUnpackedSizeFactor = 1.15;

    // Downloading dominates the wall clock, so it owns most of each pack's
    // slice of the bar and unpacking owns the rest.
    inline constexpr double kDownloadShareOfPack = 0.85;

    enum class Status
    {
        Complete,       // every pack is installed
        Missing,        // no content at all
        Incomplete,     // some packs installed, some absent
        Interrupted     // a previous fetch died part-way (marker present)
    };

    struct State
    {
        Status            status { Status::Missing };
        juce::StringArray installed;
        juce::StringArray missing;
        juce::int64       missingBytes { 0 };
        juce::String      summary;    // plain English, ready to show a user
    };

    // -- internals -----------------------------------------------------------
    namespace detail
    {
        inline std::atomic<bool>& busyFlag()
        {
            static std::atomic<bool> b { false };
            return b;
        }

        // Sibling of CoreLibrary, never inside it: a directory under CoreLibrary
        // would be enumerated as a pack by everything that walks that folder,
        // and being on the same volume is what makes installing a finished pack
        // a rename rather than a second full-size copy.
        inline juce::File stagingDir()
        {
            return SampleLibrary::getCoreLibraryDir().getParentDirectory()
                       .getChildFile ("CoreLibraryDownload");
        }

        inline juce::String formatBytes (juce::int64 b)
        {
            if (b >= (juce::int64) 1024 * 1024 * 1024)
                return juce::String (b / 1073741824.0, 1) + " GB";
            if (b >= 1024 * 1024)
                return juce::String (b / 1048576.0, 0) + " MB";
            return juce::String (b / 1024.0, 0) + " KB";
        }

        inline juce::String installedFolderFor (const SampleLibrary::CoreContentManifest& m,
                                                const Asset& a)
        {
            for (const auto& p : m.packs)
                if (p.asset.equalsIgnoreCase (a.fileName))
                    return p.folder;

            return packDisplayName (a);
        }

        inline bool isAssetInstalled (const SampleLibrary::CoreContentManifest& m,
                                      const Asset& a)
        {
            const auto dir = SampleLibrary::getCoreLibraryDir()
                                 .getChildFile (installedFolderFor (m, a));

            if (SampleLibrary::isPackInstallIncomplete (dir)) return false;
            return SampleLibrary::isPackPopulated (dir);
        }

        // Written the moment a pack lands, not at the end of the run: a fetch
        // stopped after three packs has to leave those three recorded, or the
        // next run cannot tell them from packs it never fetched.
        inline void recordInstalled (const Asset& a, const juce::String& folder)
        {
            auto m = SampleLibrary::readCoreContentManifest();

            bool found = false;
            for (auto& p : m.packs)
            {
                if (! p.asset.equalsIgnoreCase (a.fileName)) continue;
                p.folder = folder;
                found = true;
                break;
            }

            if (! found)
            {
                SampleLibrary::CoreContentPack p;
                p.asset  = juce::String (a.fileName);
                p.folder = folder;
                m.packs.add (p);
            }

            m.sourceUrl = kReleaseBaseUrl;
            SampleLibrary::writeCoreContentManifest (m);
        }

        // Same-volume rename by design (staging is a sibling of CoreLibrary), so
        // installing a finished pack is instant.  The copy path only exists for
        // the case where someone has the two on different volumes; the pack's
        // in-progress marker is inside the folder, so a copy that dies part-way
        // arrives already flagged.
        inline bool movePackInto (const juce::File& from, const juce::File& to)
        {
            if (from.moveFileTo (to)) return true;

            if (! from.copyDirectoryTo (to))
            {
                // juce::File::deleteRecursively ANDs its result across every
                // child rather than stopping at the first failure, so a payload
                // the OS refuses to remove leaves a partial tree behind.  Same
                // answer as SampleLibrary's wipeIncompleteAdoption: re-plant the
                // marker so the stump stays flagged instead of reading as a
                // finished pack.
                if (! to.deleteRecursively())
                    SampleLibrary::getPackInstallMarkerFile (to).create();

                return false;
            }

            from.deleteRecursively();
            return true;
        }

        // Returns the archive's single top-level folder, or an empty string when
        // the files sit at the archive root.  Those two shapes need different
        // handling: unpacking a root-shaped archive straight into CoreLibrary
        // would splat loose files where every top-level entry is read as a pack.
        inline juce::String singleTopLevelFolder (juce::ZipFile& zip)
        {
            juce::String top;

            for (int i = 0; i < zip.getNumEntries(); ++i)
            {
                const auto* e = zip.getEntry (i);
                if (e == nullptr) continue;

                auto name = e->filename.replaceCharacter ('\\', '/');
                while (name.startsWith ("./")) name = name.substring (2);

                if (name.isEmpty()) continue;
                if (name.startsWith ("__MACOSX/")) continue;   // archiver metadata

                const int slash = name.indexOfChar ('/');
                if (slash <= 0) return {};                     // a file at the root

                const auto first = name.substring (0, slash);
                if (top.isEmpty())    top = first;
                else if (top != first) return {};
            }

            return top;
        }

        // Watches the byte counter from OUTSIDE the transfer, because the read
        // that fills it blocks: a thread sitting in InputStream::read cannot
        // also notice a cancel or a dead connection.  juce::WebInputStream::
        // cancel is the documented way to break that read from another thread.
        class TransferWatchdog : public juce::Thread
        {
        public:
            TransferWatchdog (juce::WebInputStream* web,
                              std::atomic<juce::int64>& received,
                              std::atomic<bool>& stalledFlag,
                              std::function<bool()> shouldAbort)
                : juce::Thread ("BaySick content transfer watchdog"),
                  mWeb (web), mReceived (received), mStalled (stalledFlag),
                  mAbort (std::move (shouldAbort))
            {
            }

            ~TransferWatchdog() override { stopThread (3000); }

            void run() override
            {
                juce::int64 last        = mReceived.load();
                double      lastMovedMs = juce::Time::getMillisecondCounterHiRes();

                while (! threadShouldExit())
                {
                    wait (250.0);
                    if (threadShouldExit()) break;

                    const auto now = mReceived.load();
                    if (now != last)
                    {
                        last        = now;
                        lastMovedMs = juce::Time::getMillisecondCounterHiRes();
                    }

                    if (mAbort && mAbort())
                    {
                        if (mWeb != nullptr) mWeb->cancel();
                        break;
                    }

                    if (juce::Time::getMillisecondCounterHiRes() - lastMovedMs > kStallTimeoutMs)
                    {
                        mStalled.store (true);
                        if (mWeb != nullptr) mWeb->cancel();
                        break;
                    }
                }
            }

        private:
            juce::WebInputStream*     mWeb;
            std::atomic<juce::int64>& mReceived;
            std::atomic<bool>&        mStalled;
            std::function<bool()>     mAbort;

            JUCE_DECLARE_NON_COPYABLE (TransferWatchdog)
        };
    }

    // -- state ---------------------------------------------------------------

    inline State check()
    {
        State s;

        const auto manifest = SampleLibrary::readCoreContentManifest();

        for (const auto& a : assets())
        {
            const auto folder = detail::installedFolderFor (manifest, a);

            if (detail::isAssetInstalled (manifest, a))
            {
                s.installed.add (folder);
            }
            else
            {
                s.missing.add (folder);
                s.missingBytes += a.bytes;
            }
        }

        if (s.missing.isEmpty())
        {
            s.status  = Status::Complete;
            s.summary = juce::String (s.installed.size()) + " sound packs installed.";
            return s;
        }

        const auto sizeLine = "The download is about " + detail::formatBytes (s.missingBytes)
                            + " and can be stopped and picked up later.";

        if (SampleLibrary::getCoreContentMarkerFile().existsAsFile()
            && ! s.installed.isEmpty())
        {
            s.status  = Status::Interrupted;
            s.summary = "The last download of the BaySick sound content did not finish.\n\n"
                        "Still missing:\n  " + s.missing.joinIntoString ("\n  ")
                      + "\n\nUntil it finishes, instruments that use those sounds will be "
                        "silent.\nPicking up where it left off only fetches what is still "
                        "missing.\n\n" + sizeLine;
            return s;
        }

        if (s.installed.isEmpty())
        {
            s.status  = Status::Missing;
            s.summary = "The BaySick sound content is not on this PC yet.\n\n"
                        "Without it the sampled instruments - drums, guitars, basses, keys,\n"
                        "strings, brass and woodwinds - have nothing to play and will be\n"
                        "silent.  Everything else in the app works normally.\n\n" + sizeLine;
            return s;
        }

        s.status  = Status::Incomplete;
        s.summary = "Some of the BaySick sound content is missing from this PC:\n\n  "
                  + s.missing.joinIntoString ("\n  ")
                  + "\n\nInstruments that use those sounds will be silent.\n"
                    "Only the missing part is downloaded.\n\n" + sizeLine;
        return s;
    }

    inline bool isBusy() { return detail::busyFlag().load(); }

    // -- the fetch -----------------------------------------------------------

    // Progress window + Cancel, matching BuilderPage's export job: launchThread
    // (not runThread - modal loops are not enabled in this build), heap
    // allocated, retires itself in threadComplete.
    class FetchJob : public juce::ThreadWithProgressWindow
    {
    public:
        explicit FetchJob (std::function<void (bool, juce::String)> onDone)
            // Cancel budget must exceed the longest uninterruptible call, or
            // ThreadWithProgressWindow's default 10 s stopThread expires while a
            // connect is still outstanding and the worker is killed mid-write.
            : juce::ThreadWithProgressWindow ("Getting BaySick sound content...", true, true, 60000),
              mOnDone (std::move (onDone))
        {
        }

        void run() override
        {
            mOk = fetch();
        }

        void threadComplete (bool userPressedCancel) override
        {
            detail::busyFlag().store (false);

            if ((userPressedCancel || mCancelled) && mMessage.isEmpty())
                mMessage = "Download stopped.  Every pack that finished is kept, and starting "
                           "again picks up from there.";

            // The browser lists are built from the folder we just changed.
            SampleLibrary::getInstance().scan();

            if (mOnDone)
            {
                mOnDone (mOk, mMessage);
            }
            else if (! mOk && mMessage.isNotEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon,
                    "Sound content download", mMessage, "OK");
            }

            delete this;
        }

    private:
        enum class PackResult { installed, failed, cancelled };

        bool fetch()
        {
            const auto root    = SampleLibrary::getCoreLibraryDir();
            const auto staging = detail::stagingDir();
            const auto marker  = SampleLibrary::getCoreContentMarkerFile();

            root.createDirectory();

            setStatusMessage ("Checking what is already installed...");
            setProgress (-1.0);

            const auto manifest = SampleLibrary::readCoreContentManifest();

            std::vector<const Asset*> todo;
            juce::int64 plannedBytes = 0;
            juce::int64 largestBytes = 0;

            for (const auto& a : assets())
            {
                if (detail::isAssetInstalled (manifest, a)) continue;

                todo.push_back (&a);
                plannedBytes += a.bytes;
                largestBytes  = juce::jmax (largestBytes, a.bytes);
            }

            if (todo.empty())
            {
                marker.deleteFile();
                staging.deleteRecursively();
                mMessage = "The sound content is already installed.";
                return true;
            }

            const auto required  = (juce::int64) (plannedBytes * kUnpackedSizeFactor) + largestBytes;
            const auto freeBytes = root.getBytesFreeOnVolume();

            if (freeBytes > 0 && freeBytes < required)
            {
                mMessage = "Not enough free space on this drive.\n\n"
                           "Getting the missing sound packs needs about "
                         + detail::formatBytes (required)
                         + " while it runs (about " + detail::formatBytes (plannedBytes)
                         + " once it finishes tidying up), and there is "
                         + detail::formatBytes (freeBytes) + " free.";
                return false;
            }

            if (! staging.createDirectory().wasOk())
            {
                mMessage = "Could not create a working folder at\n"
                         + staging.getFullPathName()
                         + "\n\nCheck that the drive is not full or write-protected.";
                return false;
            }

            // Marker first, before anything is written.  Removed only once every
            // pack is verified, so every abnormal exit from here on leaves the
            // library flagged incomplete rather than silently trusted.
            marker.create();

            juce::int64 doneBytes = 0;
            int         index     = 0;
            juce::StringArray failures;

            for (const auto* a : todo)
            {
                if (threadShouldExit()) { mCancelled = true; break; }

                ++index;

                const auto display = packDisplayName (*a);

                juce::String installedFolder;
                juce::String err;
                bool         connectFailed = false;

                const auto r = installOnePack (*a, display, index, (int) todo.size(),
                                               doneBytes, plannedBytes, staging, root,
                                               installedFolder, err, connectFailed);

                if (r == PackResult::cancelled) { mCancelled = true; break; }

                if (r == PackResult::failed)
                {
                    failures.add (display + " - " + err);

                    // Nothing arrived at all, so the next nine would fail the
                    // same way against the same dead link.  A pack that failed
                    // mid-transfer is a different story: the rest are still
                    // worth trying, and this one resumes on the next run.
                    if (connectFailed) break;

                    continue;
                }

                detail::recordInstalled (*a, installedFolder);
                doneBytes += a->bytes;
                setProgress (juce::jlimit (0.0, 1.0, (double) doneBytes / (double) plannedBytes));
            }

            // Asked of the disk, not of the loop's own bookkeeping.
            const auto after = check();

            if (after.missing.isEmpty())
            {
                marker.deleteFile();
                setStatusMessage ("Tidying up...");
                staging.deleteRecursively();
                mMessage = "Sound content installed: " + juce::String (after.installed.size())
                         + " packs are ready to use.";
                return true;
            }

            if (mCancelled) return false;

            mMessage = "Some sound packs could not be downloaded:\n\n  "
                     + (failures.isEmpty() ? after.missing.joinIntoString ("\n  ")
                                           : failures.joinIntoString ("\n  "))
                     + "\n\nEverything that did arrive is installed and kept.  Trying again "
                       "picks up only what is still missing.";
            return false;
        }

        // One pack, start to finish: fetch the archive, unpack it, put it in
        // place.  Every exit leaves either a complete pack in CoreLibrary or
        // nothing there at all.
        PackResult installOnePack (const Asset& a, const juce::String& display,
                                   int index, int total,
                                   juce::int64 doneBytes, juce::int64 plannedBytes,
                                   const juce::File& staging, const juce::File& root,
                                   juce::String& installedFolderOut,
                                   juce::String& errOut, bool& connectFailedOut)
        {
            const auto reportPack = [this, doneBytes, plannedBytes, &a] (double fractionOfPack)
            {
                if (plannedBytes <= 0) return;

                const auto within = (juce::int64) (a.bytes * juce::jlimit (0.0, 1.0, fractionOfPack));
                setProgress (juce::jlimit (0.0, 1.0,
                                           (double) (doneBytes + within) / (double) plannedBytes));
            };

            const auto part    = staging.getChildFile (juce::String (a.fileName) + ".part");
            const auto archive = staging.getChildFile (a.fileName);

            // A complete archive already on disk is a crash between download and
            // unpack.  Re-use it rather than paying for the bytes twice.
            if (! (archive.existsAsFile() && archive.getSize() == a.bytes))
            {
                const auto d = downloadAsset (a, part, display, index, total,
                                              reportPack, errOut, connectFailedOut);
                if (d != PackResult::installed) return d;

                archive.deleteFile();
                if (! part.moveFileTo (archive))
                {
                    errOut = "the downloaded file could not be renamed";
                    return PackResult::failed;
                }
            }

            setStatusMessage ("Unpacking " + display + "  (" + juce::String (index)
                              + " of " + juce::String (total) + ")...");
            reportPack (kDownloadShareOfPack);

            const auto e = extractPack (archive, a, staging, root,
                                        reportPack, installedFolderOut, errOut);
            if (e != PackResult::installed) return e;

            archive.deleteFile();
            reportPack (1.0);
            return PackResult::installed;
        }

        PackResult downloadAsset (const Asset& a, const juce::File& part,
                                  const juce::String& display, int index, int total,
                                  const std::function<void (double)>& reportPack,
                                  juce::String& errOut, bool& connectFailedOut)
        {
            connectFailedOut = false;

            juce::int64 resumeFrom = part.existsAsFile() ? part.getSize() : 0;

            if (resumeFrom == a.bytes) return PackResult::installed;   // caller renames it
            if (resumeFrom > a.bytes)
            {
                // Longer than the asset can possibly be, so it is not this file.
                part.deleteFile();
                resumeFrom = 0;
            }

            int statusCode = 0;

            const auto makeOptions = [&] () -> juce::URL::InputStreamOptions
            {
                const auto base = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                      .withConnectionTimeoutMs (kConnectTimeoutMs)
                                      .withNumRedirectsToFollow (10)
                                      .withStatusCode (&statusCode);

                // A release asset redirects to object storage that honours byte
                // ranges, which is what turns a dead transfer at 900 MB into a
                // 100 MB retry instead of a 1 GB one.
                if (resumeFrom > 0)
                    return base.withExtraHeaders ("Range: bytes=" + juce::String (resumeFrom) + "-");

                return base;
            };

            setStatusMessage ("Connecting for " + display + "  (" + juce::String (index)
                              + " of " + juce::String (total) + ")...");

            const juce::URL url (juce::String (kReleaseBaseUrl) + a.fileName);
            auto stream = url.createInputStream (makeOptions());

            if (threadShouldExit()) return PackResult::cancelled;

            if (stream == nullptr)
            {
                connectFailedOut = true;
                errOut = "could not reach the download server";
                return PackResult::failed;
            }

            bool appending = false;

            if (resumeFrom > 0)
            {
                if (statusCode == 206)
                {
                    appending = true;
                }
                else if (statusCode == 200 || statusCode == 0)
                {
                    resumeFrom = 0;   // the range was ignored, so start over
                }
                else
                {
                    errOut = "the server refused the download (code "
                           + juce::String (statusCode) + ")";
                    return PackResult::failed;
                }
            }
            else if (statusCode >= 400)
            {
                connectFailedOut = true;
                errOut = "the server refused the download (code "
                       + juce::String (statusCode) + ")";
                return PackResult::failed;
            }

            juce::FileOutputStream out (part);
            if (out.failedToOpen())
            {
                errOut = "could not write to " + part.getFullPathName();
                return PackResult::failed;
            }

            // FileOutputStream opens positioned at the end, which is exactly
            // what the resume case wants and exactly what the restart case must
            // undo before the first write.
            if (! appending)
            {
                out.setPosition (0);
                out.truncate();
            }

            std::atomic<juce::int64> received { resumeFrom };
            std::atomic<bool>        stalled  { false };
            bool                     writeFailed = false;

            auto* web = dynamic_cast<juce::WebInputStream*> (stream.get());

            {
                detail::TransferWatchdog dog (web, received, stalled,
                                              [this] { return threadShouldExit(); });
                dog.startThread();

                juce::HeapBlock<char> buf ((size_t) kChunkBytes);
                double lastUiMs = 0.0;

                while (! stream->isExhausted())
                {
                    if (threadShouldExit()) break;

                    const int n = stream->read (buf.getData(), kChunkBytes);
                    if (n <= 0) break;

                    if (! out.write (buf.getData(), (size_t) n)) { writeFailed = true; break; }

                    const auto got = received.fetch_add (n) + n;

                    const double now = juce::Time::getMillisecondCounterHiRes();
                    if (now - lastUiMs >= 250.0)
                    {
                        lastUiMs = now;
                        reportPack (kDownloadShareOfPack * (double) got / (double) a.bytes);
                        setStatusMessage ("Downloading " + display + "  (" + juce::String (index)
                                          + " of " + juce::String (total) + ")   "
                                          + detail::formatBytes (got) + " of "
                                          + detail::formatBytes (a.bytes));
                    }
                }

                dog.stopThread (3000);
            }

            out.flush();

            if (threadShouldExit()) return PackResult::cancelled;

            if (writeFailed || out.getStatus().failed())
            {
                errOut = "could not write to disk (the drive may be full)";
                return PackResult::failed;
            }

            if (stalled.load())
            {
                errOut = "the download stopped responding";
                return PackResult::failed;
            }

            const auto got = received.load();
            if (got != a.bytes)
            {
                // The part file is deliberately kept: it is where the next run
                // resumes from.
                errOut = "the download ended early (" + detail::formatBytes (got)
                       + " of " + detail::formatBytes (a.bytes) + ")";
                return PackResult::failed;
            }

            return PackResult::installed;
        }

        PackResult extractPack (const juce::File& archive, const Asset& a,
                                const juce::File& staging, const juce::File& root,
                                const std::function<void (double)>& reportPack,
                                juce::String& installedFolderOut, juce::String& errOut)
        {
            const auto work = staging.getChildFile ("unpack");
            work.deleteRecursively();

            if (! work.createDirectory().wasOk())
            {
                errOut = "could not create a working folder";
                return PackResult::failed;
            }

            juce::ZipFile zip (archive);
            const int numEntries = zip.getNumEntries();

            if (numEntries <= 0)
            {
                errOut = "the downloaded file is not a readable zip";
                work.deleteRecursively();
                archive.deleteFile();   // nothing to resume from; refetch it next time
                return PackResult::failed;
            }

            const auto ownFolder = detail::singleTopLevelFolder (zip);

            // THE INSTALL NAME IS OURS, NEVER THE ARCHIVE'S.  Several of these
            // packs are the vendor's original download rezipped, so the folder
            // inside carries the vendor's versioned name -- Big Rusty Drums
            // unpacks as "karoryfer.big-rusty-drums-1.100".  Trusting that
            // installed the pack under a name no kit path resolves against, and
            // the engine failed with a missing-SFZ box pointing at the name it
            // DID expect.  packDisplayName is the name on disk that every kit
            // path is written against, so it wins; an archive folder is only a
            // level to strip.
            installedFolderOut = packDisplayName (a);

            // Archive carries its own folder: unpack into the working folder and
            // let that folder appear, then rename it below.  Files at the archive
            // root: unpack straight into the pack folder, so they never splat
            // loose into CoreLibrary.
            const auto produced = work.getChildFile (installedFolderOut);
            const auto unpacked = ownFolder.isNotEmpty() ? work.getChildFile (ownFolder)
                                                         : produced;
            const auto target   = ownFolder.isNotEmpty() ? work : produced;

            if (! unpacked.createDirectory().wasOk())
            {
                errOut = "could not create the pack folder";
                work.deleteRecursively();
                return PackResult::failed;
            }

            // Planted before the first file is written and carried into
            // CoreLibrary by the move below, so the pack reads as incomplete for
            // the whole window in which it could be interrupted.  Goes in the
            // folder the files actually land in, which survives the rename.
            if (! SampleLibrary::getPackInstallMarkerFile (unpacked).create().wasOk())
            {
                errOut = "could not create the pack folder";
                work.deleteRecursively();
                return PackResult::failed;
            }

            for (int i = 0; i < numEntries; ++i)
            {
                if (threadShouldExit())
                {
                    work.deleteRecursively();
                    return PackResult::cancelled;
                }

                const auto r = zip.uncompressEntry (i, target,
                                                    juce::ZipFile::OverwriteFiles::yes,
                                                    juce::ZipFile::FollowSymlinks::no);
                if (r.failed())
                {
                    errOut = r.getErrorMessage().trim();
                    if (errOut.isEmpty()) errOut = "the pack could not be unpacked";
                    work.deleteRecursively();
                    return PackResult::failed;
                }

                if ((i % 32) == 0)
                    reportPack (kDownloadShareOfPack + (1.0 - kDownloadShareOfPack)
                                                       * (double) i / (double) numEntries);
            }

            // Strip the archive's own folder level by naming it ours.  A rename
            // inside the staging folder, so it is one directory entry rather
            // than a copy of the whole pack.
            if (unpacked != produced && ! unpacked.moveFileTo (produced))
            {
                errOut = "could not name the unpacked pack folder";
                work.deleteRecursively();
                return PackResult::failed;
            }

            if (! SampleLibrary::isPackPopulated (produced))
            {
                errOut = "the pack unpacked to nothing";
                work.deleteRecursively();
                return PackResult::failed;
            }

            const auto dest = root.getChildFile (installedFolderOut);

            if (dest.exists() && ! dest.deleteRecursively())
            {
                // deleteRecursively ANDs across every child rather than stopping
                // at the first failure, so a partial wipe leaves a folder that
                // still reads as populated.  Flag it so the next run refetches
                // instead of blessing the stump.
                SampleLibrary::getPackInstallMarkerFile (dest).create();
                errOut = "an older copy of this pack is in use and could not be replaced";
                work.deleteRecursively();
                return PackResult::failed;
            }

            if (! detail::movePackInto (produced, dest))
            {
                errOut = "could not move the pack into the sound library folder";
                work.deleteRecursively();
                return PackResult::failed;
            }

            // Last act, and the only thing that turns the folder into content.
            SampleLibrary::getPackInstallMarkerFile (dest).deleteFile();

            if (SampleLibrary::isPackInstallIncomplete (dest))
            {
                errOut = "the pack could not be marked as finished";
                return PackResult::failed;
            }

            work.deleteRecursively();
            return PackResult::installed;
        }

        std::function<void (bool, juce::String)> mOnDone;
        juce::String mMessage;
        bool         mOk        { false };
        bool         mCancelled { false };
    };

    // Starts the fetch.  Returns false when one is already running.  onDone runs
    // on the message thread with (succeeded, message-for-the-user).
    inline bool startFetch (std::function<void (bool, juce::String)> onDone = {})
    {
        bool expectedIdle = false;
        if (! detail::busyFlag().compare_exchange_strong (expectedIdle, true))
            return false;

        (new FetchJob (std::move (onDone)))->launchThread();
        return true;
    }

    // The offer.  Three answers, returned by index: 0 download, 1 not now,
    // 2 never ask again.  Caller owns what "never ask again" persists to.
    inline void offerDownload (const State& s, std::function<void (int)> onChoice)
    {
        const auto title = s.status == Status::Interrupted
                             ? juce::String ("Finish the sound content download")
                             : juce::String ("Sound content is missing");

        const auto firstButton = s.status == Status::Interrupted
                                   ? juce::String ("Finish Downloading")
                                   : juce::String ("Download Now");

        juce::NativeMessageBox::showAsync (
            juce::MessageBoxOptions{}
                .withIconType (juce::MessageBoxIconType::WarningIcon)
                .withTitle (title)
                .withMessage (s.summary)
                .withButton (firstButton)
                .withButton ("Not Now")
                .withButton ("Don't Ask Again"),
            [onChoice] (int result) { if (onChoice) onChoice (result); });
    }
}
