#include "StandaloneApp.h"
#include "StandaloneEditor.h"
#include "BaySickAssets.h"   // BaySickDAWLogo_png / _pngSize (logo for splash + window icon)

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
struct PlayHeadAdvancer : public juce::AudioIODeviceCallback
{
    StandalonePlayHead&          playHead;
    juce::AudioProcessorPlayer&  player;
    double                       sampleRate { 44100.0 };

    PlayHeadAdvancer(StandalonePlayHead& ph, juce::AudioProcessorPlayer& pl)
        : playHead(ph), player(pl) {}

    void audioDeviceAboutToStart(juce::AudioIODevice* d) override
    {
        if (d == nullptr) return;
        sampleRate = d->getCurrentSampleRate();
        if (sampleRate <= 0.0) sampleRate = 44100.0;
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
        // Process FIRST so beatStart reads the position at the START of this block.
        // Advancing before processing was off by one block, causing notes to play late.
        player.audioDeviceIOCallbackWithContext(in, ni, out, no, n, ctx);
        playHead.advanceBlock(n, sampleRate);
    }
};

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
    info.setTimeSignature(juce::AudioPlayHead::TimeSignature{4,4});
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
                mProcessor->setDeviceOutputLatency (dev->getOutputLatencyInSamples());
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
    if (settingsFile.existsAsFile())
    {
        auto xml = juce::XmlDocument::parse(settingsFile);
        mDeviceManager->initialise(16, 2, xml.get(), true);
    }
    else
    {
        mDeviceManager->initialiseWithDefaultDevices(16, 2);
    }

    // Push initial device output latency into the processor.
    if (auto* dev = mDeviceManager->getCurrentAudioDevice())
        mProcessor->setDeviceOutputLatency (dev->getOutputLatencyInSamples());

    // Auto-save whenever the user changes device settings via the dialog.
    mDeviceManager->addChangeListener(this);

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
    if (mDeviceManager && mAdvancer)
        mDeviceManager->removeAudioCallback(mAdvancer.get());
    mAdvancer      = nullptr;
    mPlayer        = nullptr;
    mProcessor     = nullptr;
    mPlayHead      = nullptr;
    mDeviceManager = nullptr;
}

START_JUCE_APPLICATION(VibesynthStandaloneApp)
