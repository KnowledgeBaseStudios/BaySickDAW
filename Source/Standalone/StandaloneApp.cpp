#include "StandaloneApp.h"
#include "StandaloneEditor.h"
#include "BaySickAssets.h"   // BaySickDAWLogo_png / _pngSize (logo for splash + window icon)
#include "EffectPresetIO.h"  // H-9 prep: seed factory presets at launch
#include "../VibeGraph.h"    // MeterLatencyComp::recomputeFromDevice (2026-05-02)
#include "../ProjectManager.h"            // ProjectManager::getSettingsFile (Batch 10 Phase 3)
#include "../Engine/RenderEngineFlags.h"  // gMultiThreadedEngineEnabled atomic (Batch 10 Phase 3)

// ── VibeSynthWindow ───────────────────────────────────────────────────────────
// Subclass so the OS close button actually quits the application.
class VibeSynthWindow : public juce::DocumentWindow
{
public:
    VibeSynthWindow()
        : juce::DocumentWindow("BaySickDAW", juce::Colours::black,
                               juce::DocumentWindow::allButtons)
    {}
    void closeButtonPressed() override
    {
        // P5: route through StandaloneEditor so unsaved-changes prompt fires
        // before the app actually quits.
        if (auto* editor = dynamic_cast<StandaloneEditor*> (getContentComponent()))
        {
            if (! editor->requestAppQuit())
                return;   // editor showing async dialog - will quit on accept
        }
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

// ── PlayHeadAdvancer ──────────────────────────────────────────────────────────
// Wraps AudioProcessorPlayer and advances the standalone playhead PPQ position
// before each audio block so the Sequencer sees time advancing.
//
// J-A2 (2026-05-04): also routes the processor's stereo master output to a
// user-chosen output channel pair (or single channel for mono).  Multi-output
// audio interfaces (e.g. Tascam Model 24, Focusrite 18i20) typically expose
// 8-22 output channels; the user selects which pair the master mix lands on
// via a dropdown in the Audio Settings dialog.  Stored as two atomics so the
// audio thread reads lock-free.
struct PlayHeadAdvancer : public juce::AudioIODeviceCallback
{
    StandalonePlayHead&          playHead;
    juce::AudioProcessorPlayer&  player;
    double                       sampleRate { 44100.0 };

    juce::AudioBuffer<float> mScratchStereo;   // pre-sized in audioDeviceAboutToStart

    PlayHeadAdvancer(StandalonePlayHead& ph, juce::AudioProcessorPlayer& pl)
        : playHead(ph), player(pl) {}

    void audioDeviceAboutToStart(juce::AudioIODevice* d) override
    {
        if (d == nullptr) return;
        sampleRate = d->getCurrentSampleRate();
        if (sampleRate <= 0.0) sampleRate = 44100.0;
        const int blockSize = juce::jmax (32, d->getCurrentBufferSizeSamples());
        mScratchStereo.setSize (2, blockSize, /*keepContent*/false, /*clearExtra*/true,
                                /*avoidReallocating*/false);
        player.audioDeviceAboutToStart(d);
    }
    void audioDeviceStopped() override
    {
        player.audioDeviceStopped();
        // Don't stop the playhead here: advanceBlock() only runs from
        // audioDeviceIOCallbackWithContext(), which JUCE will have already
        // halted before calling audioDeviceStopped(). Calling playHead.stop()
        // here silenced playback after any device switch.
    }

    void audioDeviceIOCallbackWithContext(const float* const* in, int ni,
                                          float* const* out, int no, int n,
                                          const juce::AudioIODeviceCallbackContext& ctx) override
    {
        // J-A2: zero every device output channel so untouched ones go silent
        // even if the player's prior callback left residue.
        for (int c = 0; c < no; ++c)
            if (out[c] != nullptr)
                juce::FloatVectorOperations::clear (out[c], n);

        const int  rawFirst = MasterOutputRouting::gFirstOutputChannel.load (std::memory_order_relaxed);
        const bool isMono   = MasterOutputRouting::gMasterIsMono.load (std::memory_order_relaxed);
        // 2026-05-05 bug fix: clamp the saved channel index so a stereo pair
        // (firstCh + 1) still fits inside the current device's channel count.
        // Without this, a saved value larger than the new device's count (e.g.
        // firstChannel=20 saved on a 22-out ASIO interface, then plugging in
        // a 2-out USB headset) clamped to no-1 → both mapped[0] and mapped[1]
        // collapsed onto out[no-1] → player's L and R both written to the same
        // physical channel → user hears stereo only out of the right speaker.
        const int  maxFirstStereo = juce::jmax (0, no - 2);
        const int  firstCh = isMono
                                 ? juce::jlimit (0, juce::jmax (0, no - 1), rawFirst)
                                 : juce::jlimit (0, maxFirstStereo,         rawFirst);

        if (isMono || no <= 1)
        {
            // Player writes stereo into the scratch; we sum L+R*0.5 into the
            // chosen output channel.  Skips silently if the chosen channel is
            // out of range (firstCh clamped above).
            if (n > mScratchStereo.getNumSamples())
                mScratchStereo.setSize (2, n, false, true, true);   // last-resort grow
            float* scratchPtrs[2] = { mScratchStereo.getWritePointer (0),
                                       mScratchStereo.getWritePointer (1) };
            juce::FloatVectorOperations::clear (scratchPtrs[0], n);
            juce::FloatVectorOperations::clear (scratchPtrs[1], n);
            player.audioDeviceIOCallbackWithContext (in, ni, scratchPtrs, 2, n, ctx);
            if (auto* dst = (firstCh < no) ? out[firstCh] : nullptr)
                for (int s = 0; s < n; ++s)
                    dst[s] = (scratchPtrs[0][s] + scratchPtrs[1][s]) * 0.5f;
        }
        else
        {
            // Stereo: route the player's L/R into the chosen pair.  If
            // firstCh+1 exceeds device channel count, fall back to mono on
            // firstCh (single-channel device or last-channel selection).
            const int secondCh = juce::jmin (firstCh + 1, no - 1);
            float* mapped[2] = {
                out[firstCh],
                (secondCh != firstCh) ? out[secondCh] : out[firstCh]
            };
            player.audioDeviceIOCallbackWithContext (in, ni, mapped, 2, n, ctx);
        }

        playHead.advanceBlock(n, sampleRate);
    }
};

// J-A2 master-output routing globals (declarations live in StandaloneApp.h).
namespace MasterOutputRouting
{
    std::atomic<int>  gFirstOutputChannel { 0 };
    std::atomic<bool> gMasterIsMono       { false };
}

// ── StandalonePlayHead ────────────────────────────────────────────────────────
void StandalonePlayHead::advanceBlock(int numSamples, double sampleRate)
{
    mSampleRate = sampleRate;
    if (!mPlaying.load()) return;
    double bpm = mBPM.load();
    double beatsPerSample = bpm / (60.0 * sampleRate);
    double newPos = mPPQPos.load() + numSamples * beatsPerSample;

    double loopEnd   = mLoopBeats.load();
    double loopStart = mLoopStart.load();
    if (loopEnd > 0.0 && newPos >= loopEnd)
        newPos = loopStart + std::fmod(newPos - loopStart, loopEnd - loopStart);

    mPPQPos.store(newPos);
}
void StandalonePlayHead::start(double bpm)  { mBPM.store(bpm); mPlaying.store(true); }
void StandalonePlayHead::stop()             { mPlaying.store(false); }
void StandalonePlayHead::reset()            { mPPQPos.store(0.0); mPlaying.store(false); }
void StandalonePlayHead::seekTo(double beat){ mPPQPos.store(juce::jmax(0.0, beat)); }

juce::Optional<juce::AudioPlayHead::PositionInfo> StandalonePlayHead::getPosition() const
{
    PositionInfo info;
    info.setBpm(mBPM.load());
    info.setPpqPosition(mPPQPos.load());
    info.setIsPlaying(mPlaying.load());
    info.setIsRecording(mRecording.load());
    info.setTimeSignature (juce::AudioPlayHead::TimeSignature{ mTsNum.load(), mTsDen.load() });
    return info;
}

// ── Audio settings persistence ────────────────────────────────────────────────
juce::File VibesynthStandaloneApp::getAudioSettingsFile()
{
    // P4b (2026-04-23): canonical location moved to Documents\BaySickDAW\.
    // On the FIRST launch with this build, ProjectManager's migration has
    // not yet run (ctor order: app loads device settings BEFORE editor +
    // ProjectManager are created), so we fall back to the legacy Roaming
    // path if the new Documents file doesn't exist yet.  Once migration
    // moves the file, this transparently flips to the Documents path.
    auto neu = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("BaySickDAW")
                   .getChildFile("audio_settings.xml");
    if (neu.existsAsFile()) return neu;
    auto legacy = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                      .getChildFile("BaySickDAW")
                      .getChildFile("audio_settings.xml");
    if (legacy.existsAsFile()) return legacy;
    return neu;   // fresh install - return the new path so save lands there
}

// J-A2 (2026-05-04): master-output routing persistence.  Lives in a sibling
// file of audio_settings.xml so it stays per-machine (different audio
// interfaces per workstation).
juce::File VibesynthStandaloneApp::getMasterOutputFile()
{
    return getAudioSettingsFile().getSiblingFile ("master_output.xml");
}

void VibesynthStandaloneApp::loadMasterOutputRouting()
{
    const auto f = getMasterOutputFile();
    if (! f.existsAsFile()) return;
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName ("MASTEROUT")) return;
    const int  first = xml->getIntAttribute ("firstChannel", 0);
    const bool mono  = xml->getBoolAttribute ("mono", false);
    MasterOutputRouting::gFirstOutputChannel.store (juce::jmax (0, first), std::memory_order_relaxed);
    MasterOutputRouting::gMasterIsMono.store (mono, std::memory_order_relaxed);
}

void VibesynthStandaloneApp::saveMasterOutputRouting()
{
    juce::XmlElement xml ("MASTEROUT");
    xml.setAttribute ("firstChannel",
                      MasterOutputRouting::gFirstOutputChannel.load (std::memory_order_relaxed));
    xml.setAttribute ("mono",
                      MasterOutputRouting::gMasterIsMono.load (std::memory_order_relaxed));
    auto f = getMasterOutputFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (xml.toString());
}

// 2026-05-07 (Batch 10 Phase 3): load + save the Mixer hamburger menu's
// "Multi-core Rendering" preference from Documents/BaySickDAW/settings.xml.
// Stored as <MultiCoreRendering on="0|1"/> child of <BaySickDAWSettings>;
// preserves any other sections (RecentPatternColors, RecentNAMFiles,
// RecentIRFiles, RecentProjects, etc.) the file already contains.  Default
// is true when the key is missing, which matches the constexpr-default we
// shipped in Phase 1 + 2.
void VibesynthStandaloneApp::loadMultiCoreRenderingPref()
{
    const auto f = ProjectManager::getSettingsFile();
    if (! f.existsAsFile()) return;   // first launch -- keep the in-memory default (true)

    auto root = juce::XmlDocument::parse (f);
    if (root == nullptr) return;

    if (auto* node = root->getChildByName ("MultiCoreRendering"))
    {
        const bool on = node->getBoolAttribute ("on", true);
        // release-store pairs with the audio thread's acquire-load at the top
        // of processBlock.  Called before mDeviceManager->initialise so the
        // very first audio callback already sees the persisted value.
        RenderEngine::gMultiThreadedEngineEnabled.store (on, std::memory_order_release);
    }
}

void VibesynthStandaloneApp::saveMultiCoreRenderingPref()
{
    const auto f = ProjectManager::getSettingsFile();
    f.getParentDirectory().createDirectory();

    // Read existing root or create a new one -- preserves every other
    // sibling section already in settings.xml (PatternColorPicker writes
    // <RecentPatternColors>, BaySickNAMIREditor writes <RecentNAMFiles> /
    // <RecentIRFiles>, ProjectManager writes <RecentProjects>, etc.).
    std::unique_ptr<juce::XmlElement> root;
    if (f.existsAsFile())
        root = juce::XmlDocument::parse (f);
    if (root == nullptr)
        root = std::make_unique<juce::XmlElement> ("BaySickDAWSettings");

    if (auto* existing = root->getChildByName ("MultiCoreRendering"))
        root->removeChildElement (existing, true);

    auto* node = root->createNewChildElement ("MultiCoreRendering");
    node->setAttribute ("on",
                        RenderEngine::gMultiThreadedEngineEnabled.load (std::memory_order_acquire));

    root->writeTo (f);
}

void VibesynthStandaloneApp::saveAudioSettings()
{
    if (!mDeviceManager) return;
    auto xml = mDeviceManager->createStateXml();
    if (!xml) return;
    auto f = getAudioSettingsFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText(xml->toString());
}

void VibesynthStandaloneApp::changeListenerCallback(juce::ChangeBroadcaster*)
{
    // Defer to next message-loop tick so the device manager finishes switching
    // before we serialize its state. Calling saveAudioSettings() synchronously
    // here could crash if the manager is mid-reinit.
    juce::MessageManager::callAsync([this]
    {
        saveAudioSettings();

        // Update device output latency in the processor so the visual playhead
        // compensation stays accurate after a device change.
        if (mDeviceManager && mProcessor)
            if (auto* dev = mDeviceManager->getCurrentAudioDevice())
            {
                mProcessor->setDeviceOutputLatency (dev->getOutputLatencyInSamples());
                // 2026-05-02: refresh meter latency-comp block count too so
                // the user's hamburger toggle stays accurate across device
                // changes (different driver -> different latency).
                MeterLatencyComp::recomputeFromDevice (dev->getCurrentSampleRate(),
                                                         dev->getCurrentBufferSizeSamples(),
                                                         dev->getOutputLatencyInSamples());
            }
    });
}

// ── VibesynthStandaloneApp ────────────────────────────────────────────────────
void VibesynthStandaloneApp::initialise(const juce::String&)
{
    // ── Splash screen (2026-04-21) ───────────────────────────────────────────
    // Shows the BaySickDAW logo on launch while the DAW initialises. The
    // juce::SplashScreen is self-managing — it deletes itself after the delay
    // (or on mouse click), so we don't store a pointer.
    const juce::Image logo = juce::ImageCache::getFromMemory(
        BaySickAssets::BaySickDAWLogo_png,
        BaySickAssets::BaySickDAWLogo_pngSize);
    if (logo.isValid())
    {
        // Half-size splash (logo PNG is 1024x1024; rendered at 512x512).
        const juce::Image splashImg = logo.rescaled (
            logo.getWidth()  / 2,
            logo.getHeight() / 2,
            juce::Graphics::highResamplingQuality);
        auto* splash = new juce::SplashScreen ("BaySickDAW", splashImg,
                                                /*useDropShadow=*/true);
        splash->deleteAfterDelay (juce::RelativeTime::seconds (4.0),
                                   /*removeOnMouseClick=*/true);
    }

    // H-9 prep (2026-05-02): seed effect-rack factory presets on every launch.
    // Idempotent -- only writes preset XML files that aren't already on disk,
    // so re-runs are nearly free.  Replaces any user-deleted factory presets.
    // H-10 cutover (2026-05-02): migrate legacy Tape preset folder into
    // Saturation BEFORE seeding -- Tape's My Presets get moved over and
    // its stale Factory dir gets wiped, so the new Saturation/Factory entries
    // ("Tape Vintage" etc) seed cleanly.
    EffectPresetIO::migrateTapeFolderToSaturation();
    EffectPresetIO::seedFactoryPresets();

    mProcessor    = std::make_unique<VibeSynthProcessor>();
    mPlayHead     = std::make_unique<StandalonePlayHead>();
    mProcessor->setPlayHead(mPlayHead.get());

    mDeviceManager = std::make_unique<juce::AudioDeviceManager>();

    // Load previously saved device settings, or fall back to system defaults.
    // If a pending file exists (written by the audio-settings dialog before a
    // restart), promote it to the live settings file before opening any device.
    auto settingsFile  = getAudioSettingsFile();
    auto pendingFile   = settingsFile.getSiblingFile("audio_settings_pending.xml");
    if (pendingFile.existsAsFile())
        pendingFile.moveFileTo(settingsFile);   // atomic rename; overwrites old file

    // R3 (2026-04-23): bumped input channel request from 0 -> 16 so live-input
    // (Vox / Inst) strips can pull audio from connected ASIO interfaces.  16
    // covers most desktop interfaces (Tascam Model 24, Focusrite 18i20, etc.);
    // larger devices still work because JUCE clamps to the device's actual
    // channel count.  The 2-output count is unchanged (stereo master).
    // J-A2 (2026-05-04): bumped output channel request from 2 -> 64 so the
    // Mixer hamburger's Master Output picker has every channel of a multi-out
    // interface available.  Tascam Model 24 = 22 outs, Focusrite 18i20 = 8 outs,
    // most consumer audio = 2 outs.  JUCE clamps to the device's actual count
    // (request 64 -> get all 22 on the Tascam, all 2 on a USB headset).  Same
    // clamp behavior the input request (16) already relied on.
    // J-A2 / C3 (2026-05-04): two-stage channel-mask override.
    //
    // Stage 1 - strip the saved channel masks BEFORE initialise.  JUCE saves
    // the active-input / active-output BigInteger as `audioDeviceInChans` /
    // `audioDeviceOutChans` (binary strings).  When you switch from a
    // 2-channel device (e.g. laptop mic) to a 22+ channel ASIO interface, the
    // saved bits stay restrictive: JUCE preserves them across the swap, so
    // the new device opens with most of its channels DISABLED.  Removing the
    // attributes forces JUCE to use the defaults inferred from the
    // `numInputChannelsNeeded` / `numOutputChannelsNeeded` initialise() args.
    //
    // Stage 2 - after initialise, write a fresh setup with EVERY channel
    // enabled and call restartLastAudioDevice() so the change actually takes.
    // setAudioDeviceSetup alone sometimes no-ops if JUCE thinks nothing
    // material changed; the explicit restart guarantees the new mask sticks.
    // 2026-05-07 (Batch 10 Phase 3): restore the Mixer hamburger's "Multi-core
    // Rendering" preference BEFORE mDeviceManager->initialise so the very
    // first audio callback already routes via the correct path (MT vs serial).
    // Defaults to true when the key is missing.  Independent of audio device
    // settings -- lives in settings.xml not audio_settings.xml.
    loadMultiCoreRenderingPref();

    if (settingsFile.existsAsFile())
    {
        auto xml = juce::XmlDocument::parse(settingsFile);
        if (xml != nullptr)
        {
            xml->removeAttribute ("audioDeviceInChans");
            xml->removeAttribute ("audioDeviceOutChans");
        }
        mDeviceManager->initialise(64, 64, xml.get(), true);
    }
    else
    {
        mDeviceManager->initialiseWithDefaultDevices(64, 64);
    }

    // J-A2 / C3 (2026-05-04): diagnostic log to disk — captures what JUCE
    // actually does with the audio device setup at startup, so we can see
    // why the input mask refuses to enable channels.  Written next to
    // audio_settings.xml as `audio_setup_log.txt`.
    juce::String diagLog;
    diagLog << "Settings file: " << settingsFile.getFullPathName() << "\n";
    diagLog << "Settings exists: " << (settingsFile.existsAsFile() ? "yes" : "no") << "\n\n";

    if (auto* dev = mDeviceManager->getCurrentAudioDevice())
    {
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        mDeviceManager->getAudioDeviceSetup (setup);

        diagLog << "BEFORE setAudioDeviceSetup:\n";
        diagLog << "  type: " << mDeviceManager->getCurrentAudioDeviceType() << "\n";
        diagLog << "  inputDeviceName: '" << setup.inputDeviceName << "'\n";
        diagLog << "  outputDeviceName: '" << setup.outputDeviceName << "'\n";
        diagLog << "  inputChannels: " << setup.inputChannels.toString (2) << "\n";
        diagLog << "  outputChannels: " << setup.outputChannels.toString (2) << "\n";
        diagLog << "  useDefaultInputChannels: " << (setup.useDefaultInputChannels ? "true" : "false") << "\n";
        diagLog << "  useDefaultOutputChannels: " << (setup.useDefaultOutputChannels ? "true" : "false") << "\n";
        diagLog << "  device active inputs:  " << dev->getActiveInputChannels().toString (2) << "\n";
        diagLog << "  device active outputs: " << dev->getActiveOutputChannels().toString (2) << "\n";
        diagLog << "  device input names count:  " << dev->getInputChannelNames().size() << "\n";
        diagLog << "  device output names count: " << dev->getOutputChannelNames().size() << "\n\n";

        // 2026-05-05: gate the J-A2 mask override to ASIO only.  For Windows-mode
        // drivers (DirectSound, WASAPI shared/exclusive, "Windows Audio") the
        // device's natural stereo defaults are already correct — forcing every
        // device channel active causes JUCE to expose 4/6/8 channels in
        // processBlock and the Windows audio mixer can route the stereo write
        // to channels 0+1 onto the wrong physical pair (user-reported bug:
        // every page audible only on the right speaker after this patch
        // landed).  ASIO devices still need the patch because their saved XML
        // typically lacks the input device name + has a zero output mask
        // (input dialog deliberately left untouched).
        const auto driverType = mDeviceManager->getCurrentAudioDeviceType();
        const bool isAsio = driverType.equalsIgnoreCase ("ASIO");

        if (isAsio)
        {
            // For ASIO devices, input and output device names must match.  The
            // settings dialog deliberately leaves audioInputDeviceName alone (to
            // avoid clobbering across configs), so the saved XML may have only the
            // output name.  Force them equal when input is empty.
            if (setup.inputDeviceName.isEmpty() && setup.outputDeviceName.isNotEmpty())
                setup.inputDeviceName = setup.outputDeviceName;

            const int devIns  = dev->getInputChannelNames().size();
            const int devOuts = dev->getOutputChannelNames().size();
            if (devIns > 0)
            {
                juce::BigInteger ins;
                ins.setRange (0, devIns, true);
                setup.inputChannels           = ins;
                setup.useDefaultInputChannels = false;
            }
            if (devOuts > 0)
            {
                juce::BigInteger outs;
                outs.setRange (0, devOuts, true);
                setup.outputChannels           = outs;
                setup.useDefaultOutputChannels = false;
            }
        }
        else
        {
            diagLog << "Non-ASIO driver (" << driverType
                    << ") — leaving channel masks at JUCE defaults.\n\n";
        }

        diagLog << "ATTEMPTING setAudioDeviceSetup with:\n";
        diagLog << "  inputDeviceName: '" << setup.inputDeviceName << "'\n";
        diagLog << "  outputDeviceName: '" << setup.outputDeviceName << "'\n";
        diagLog << "  inputChannels: " << setup.inputChannels.toString (2) << "\n";
        diagLog << "  outputChannels: " << setup.outputChannels.toString (2) << "\n\n";

        const auto err = mDeviceManager->setAudioDeviceSetup (setup, /*treatAsChosenDevice*/ true);
        diagLog << "setAudioDeviceSetup result: '" << (err.isEmpty() ? juce::String ("(ok)") : err) << "'\n\n";

        mDeviceManager->restartLastAudioDevice();

        juce::AudioDeviceManager::AudioDeviceSetup after;
        mDeviceManager->getAudioDeviceSetup (after);
        diagLog << "AFTER restartLastAudioDevice:\n";
        diagLog << "  inputDeviceName: '" << after.inputDeviceName << "'\n";
        diagLog << "  outputDeviceName: '" << after.outputDeviceName << "'\n";
        diagLog << "  inputChannels: " << after.inputChannels.toString (2) << "\n";
        diagLog << "  outputChannels: " << after.outputChannels.toString (2) << "\n";
        if (auto* d2 = mDeviceManager->getCurrentAudioDevice())
        {
            diagLog << "  device active inputs:  " << d2->getActiveInputChannels().toString (2) << "\n";
            diagLog << "  device active outputs: " << d2->getActiveOutputChannels().toString (2) << "\n";
        }
        else
        {
            diagLog << "  device: NULL after restart\n";
        }
    }
    else
    {
        diagLog << "getCurrentAudioDevice() returned null after initialise.\n";
    }

    {
        const auto logFile = settingsFile.getSiblingFile ("audio_setup_log.txt");
        logFile.getParentDirectory().createDirectory();
        logFile.replaceWithText (diagLog);
    }

    // J-A2 (2026-05-04): restore the user's master-output channel pick from
    // master_output.xml (sibling of audio_settings.xml).  Done AFTER
    // mDeviceManager->initialise so the chosen index is clamped against the
    // current device's actual channel count later in the audio callback.
    loadMasterOutputRouting();

    // Push initial device output latency into the processor.
    if (auto* dev = mDeviceManager->getCurrentAudioDevice())
    {
        mProcessor->setDeviceOutputLatency (dev->getOutputLatencyInSamples());
        // 2026-05-02: also update the meter latency-compensation block count
        // so vsync-locked meters can align with the audio you actually hear.
        MeterLatencyComp::recomputeFromDevice (dev->getCurrentSampleRate(),
                                                 dev->getCurrentBufferSizeSamples(),
                                                 dev->getOutputLatencyInSamples());
    }

    // Auto-save whenever the user changes device settings via the dialog.
    mDeviceManager->addChangeListener(this);

    // C.3 (2026-04-30): hardware MIDI input.  After initialise reads the saved
    // device-state XML, enumerate all detected MIDI inputs and register the
    // app as a callback for each.  First-launch default = enable all (Q1=B
    // multi-device, all on); subsequent launches respect the saved state by
    // only enabling devices that already had a MIDIINPUT entry in the XML.
    {
        bool anySavedMidiState = false;
        if (settingsFile.existsAsFile())
        {
            if (auto savedXml = juce::XmlDocument::parse (settingsFile))
            {
                for (auto* c : savedXml->getChildIterator())
                    if (c->hasTagName ("MIDIINPUT")) { anySavedMidiState = true; break; }
            }
        }

        const auto availableMidi = juce::MidiInput::getAvailableDevices();
        for (const auto& d : availableMidi)
        {
            if (! anySavedMidiState)
                mDeviceManager->setMidiInputDeviceEnabled (d.identifier, true);
            // Register the callback for every available device.  JUCE only
            // invokes it while the device is enabled, so toggling
            // setMidiInputDeviceEnabled later is sufficient.
            mDeviceManager->addMidiInputDeviceCallback (d.identifier, this);
        }
    }

    // I-3b (2026-05-02): Load the user's global MIDI Learn defaults if the
    // file exists.  Per-project setStateInformation overlays its own mapping
    // table on top later, so per-project mappings win.  No-op if the file
    // doesn't exist (first launch).
    mProcessor->getMidiLearnRegistry().loadGlobalDefaults();

    mPlayer = std::make_unique<juce::AudioProcessorPlayer>();
    mPlayer->setProcessor(mProcessor.get());
    mAdvancer = std::make_unique<PlayHeadAdvancer>(*mPlayHead, *mPlayer);
    mDeviceManager->addAudioCallback(mAdvancer.get());

    mWindow = std::make_unique<VibeSynthWindow>();
    // Window title-bar icon (reuses the embedded splash logo).
    {
        const juce::Image winIcon = juce::ImageCache::getFromMemory(
            BaySickAssets::BaySickDAWLogo_png,
            BaySickAssets::BaySickDAWLogo_pngSize);
        if (winIcon.isValid())
            mWindow->setIcon (winIcon);
    }
    auto* editor = new StandaloneEditor(*mProcessor, *mPlayHead, *mDeviceManager);
    editor->setAudioCallback(mAdvancer.get());   // allows safe unregister/re-register around device switches
    mWindow->setContentOwned(editor, true);

    // Full window mode -- launch maximized
    mWindow->setResizable(false, false);
    mWindow->setFullScreen(true);
    mWindow->setVisible(true);
}

void VibesynthStandaloneApp::shutdown()
{
    // Only auto-save the current device state if the user hasn't written a
    // pending-restart settings file.  If a pending file exists it means the
    // user just clicked Apply+Restart; saving now would overwrite their chosen
    // device with the old one still running in this session.
    auto pendingFile = getAudioSettingsFile().getSiblingFile("audio_settings_pending.xml");
    if (!pendingFile.existsAsFile())
        saveAudioSettings();
    mWindow = nullptr;
    if (mDeviceManager)
        mDeviceManager->removeChangeListener(this);
    // C.3 (2026-04-30): unregister MIDI input callbacks before tearing down
    // the device manager.  Defensive: ~AudioDeviceManager would clear them
    // anyway, but explicit removal prevents any race against in-flight MIDI
    // thread work during shutdown.
    if (mDeviceManager)
    {
        const auto availableMidi = juce::MidiInput::getAvailableDevices();
        for (const auto& d : availableMidi)
            mDeviceManager->removeMidiInputDeviceCallback (d.identifier, this);
    }
    if (mDeviceManager && mAdvancer)
        mDeviceManager->removeAudioCallback(mAdvancer.get());
    mAdvancer      = nullptr;
    mPlayer        = nullptr;
    mProcessor     = nullptr;
    mPlayHead      = nullptr;
    mDeviceManager = nullptr;
}

// C.3 (2026-04-30): MIDI input thread -> processor's collector.  Keep this
// short and lock-free; MidiMessageCollector::addMessageToQueue is wait-free.
//
// I-3b (2026-05-02): Also fan the same event into mProcessor's MIDI Learn
// event queue, tagged with the source device's name.  The audio thread
// drains both the live MIDI collector (engine-page routing) and the learn
// queue (CC -> APVTS dispatch) in processBlock.  Two parallel paths: the
// live collector loses device origin (fine for engine-page MIDI), the learn
// queue preserves it (so device-locked mappings can filter).
void VibesynthStandaloneApp::handleIncomingMidiMessage (juce::MidiInput* source,
                                                         const juce::MidiMessage& message)
{
    if (mProcessor == nullptr) return;

    mProcessor->getLiveMidiCollector().addMessageToQueue (message);

    // Only learnable channel-voice messages (CC / pitch-bend / channel
    // pressure) need to enter the learn queue.  Filter here to keep the
    // queue from filling with note-on/off traffic that the registry would
    // discard anyway.
    if (message.isController() || message.isPitchWheel() || message.isChannelPressure())
    {
        const juce::String deviceName = (source != nullptr) ? source->getName() : juce::String();
        mProcessor->getMidiLearnEventQueue().push (deviceName, message);
    }
}

START_JUCE_APPLICATION(VibesynthStandaloneApp)
