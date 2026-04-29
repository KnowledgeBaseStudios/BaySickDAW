#include "StandaloneEditor.h"
#include "../PatternManager.h"
#include "../ProjectManager.h"
#include "ProjectBrowserWindow.h"
#include "../SampleLibrary.h"
#include "LayersPage.h"
#include "BassPage.h"
#include "DrumPage.h"
#include "BuilderPage.h"
#include "PianoRollPage.h"
#include "MixerPage.h"
#include "MetroPanel.h"
#include "SlotComponent.h"  // effectTypeName() for automation display-name resolver
#include "KeyBindings.h"
#include "KeyBindsWindow.h"
#include "PatternColorPicker.h"
#include "../BaySickSynth/BaySickSynthProcessor.h"   // D2 Batch 4: kit audition dispatch
#include "../VibePlayer/VibePlayerProcessor.h"       // D2 Batch 4: kit audition dispatch
#include "../Harmless/HarmlessProcessor.h"           // step 2 commit 2: layer/bass register helpers
#include "../BaySickBass/BaySickBassProcessor.h"     // step 2 commit 2: bass register helper
#include "../BaySickNAMIR/BaySickNAMIRProcessor.h"   // G-1.5: floating test window
#include "../BaySickNAMIR/BaySickNAMIREditor.h"      // G-1.5: floating test window
#include "../Clips/ClipsPage.h"                       // G-2: Clips page + empty state

namespace
{
    // Forward-decl for the async-prompt helper defined further down in this
    // TU.  Needed so lambdas in createBuilderPage (P4 drop-without-project
    // flow) can call it before the definition appears in source order.
    void promptForProjectName (const juce::String& title,
                                const juce::String& message,
                                const juce::String& defaultText,
                                std::function<void(juce::String)> onAccept,
                                std::function<void()> onCancel = {});
}

// ─────────────────────────────────────────────────────────────────────────────
// AudioSettingsDialog — safe device-switching dialog
//
// Why not AudioDeviceSelectorComponent?
//   JUCE's built-in selector does a live hot-swap the instant you pick a new
//   device. For exclusive-mode WASAPI devices (USB gaming headsets etc.) this
//   crashes: JUCE tries to close the active WASAPI stream on the message thread
//   while the device's internal render thread is still running → access violation
//   → JUCE exception handler calls shutdown() → window closes.
//
// This dialog instead:
//   1. Reads current settings UP FRONT (before touching anything).
//   2. Lets the user pick new settings without touching the device.
//   3. On Apply: removes our callback, closes the device cleanly, opens the
//      new device, re-adds the callback — all while no render thread is running.
// ─────────────────────────────────────────────────────────────────────────────
class AudioSettingsDialog : public juce::Component
{
public:
    AudioSettingsDialog(juce::AudioDeviceManager& dm,
                        juce::AudioIODeviceCallback* cb)
        : mMgr(dm), mCallback(cb)
    {
        setSize(480, 290);

        auto styleCombo = [](juce::ComboBox& b)
        {
            b.setColour(juce::ComboBox::backgroundColourId, VC::Panel);
            b.setColour(juce::ComboBox::textColourId,       VC::Text);
            b.setColour(juce::ComboBox::outlineColourId,    VC::Accent.withAlpha(0.4f));
            b.setColour(juce::ComboBox::arrowColourId,      VC::Accent);
        };
        auto styleLabel = [](juce::Label& l, const juce::String& t)
        {
            l.setText(t, juce::dontSendNotification);
            l.setColour(juce::Label::textColourId, VC::Text);
            l.setJustificationType(juce::Justification::centredRight);
        };

        styleLabel(mTypeLbl, "Audio Mode:");   styleCombo(mTypeBox); addAndMakeVisible(mTypeLbl); addAndMakeVisible(mTypeBox);
        styleLabel(mDevLbl,  "Audio Device:"); styleCombo(mDevBox);  addAndMakeVisible(mDevLbl);  addAndMakeVisible(mDevBox);
        styleLabel(mRateLbl, "Sample Rate:");  styleCombo(mRateBox); addAndMakeVisible(mRateLbl); addAndMakeVisible(mRateBox);
        styleLabel(mBufLbl,  "Buffer Size:");  styleCombo(mBufBox);  addAndMakeVisible(mBufLbl);  addAndMakeVisible(mBufBox);

        mApplyBtn.setButtonText("Apply");
        mApplyBtn.setColour(juce::TextButton::buttonColourId,  VC::Accent.withAlpha(0.25f));
        mApplyBtn.setColour(juce::TextButton::textColourOffId, VC::Text);
        mApplyBtn.onClick = [this] { applySettings(); };
        addAndMakeVisible(mApplyBtn);

        mCloseBtn.setButtonText("Close");
        mCloseBtn.setColour(juce::TextButton::buttonColourId,  VC::Panel);
        mCloseBtn.setColour(juce::TextButton::textColourOffId, VC::TextDim);
        mCloseBtn.onClick = [this] {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState(0);
        };
        addAndMakeVisible(mCloseBtn);

        populateFromManager();
    }

    void resized() override
    {
        const int kLblW = 110, kRowH = 36, kPad = 16, kComboH = 26;
        int y = kPad;
        auto row = [&](juce::Label& l, juce::ComboBox& b) {
            l.setBounds(kPad, y + 4, kLblW, kComboH);
            b.setBounds(kPad + kLblW + 8, y, getWidth() - kPad * 2 - kLblW - 8, kComboH);
            y += kRowH;
        };
        row(mTypeLbl, mTypeBox);
        row(mDevLbl,  mDevBox);
        row(mRateLbl, mRateBox);
        row(mBufLbl,  mBufBox);

        y += 12;
        const int btnW = 90, btnH = 28;
        mApplyBtn.setBounds(getWidth() - kPad - btnW * 2 - 8, y, btnW, btnH);
        mCloseBtn.setBounds(getWidth() - kPad - btnW,         y, btnW, btnH);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(VC::Bg);
        g.setColour(VC::Accent.withAlpha(0.15f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.f), 4.f, 1.f);
    }

private:
    // ── Populate — reads current state, never changes live manager state ──────
    void populateFromManager()
    {
        // Snapshot current setup before touching anything
        mMgr.getAudioDeviceSetup(mSnapshot);

        // Device types
        auto& types = mMgr.getAvailableDeviceTypes();
        for (int i = 0; i < types.size(); ++i)
            mTypeBox.addItem(types[i]->getTypeName(), i + 1);

        if (auto* cur = mMgr.getCurrentDeviceTypeObject())
            mTypeBox.setText(cur->getTypeName(), juce::dontSendNotification);
        else if (mTypeBox.getNumItems() > 0)
            mTypeBox.setSelectedItemIndex(0, juce::dontSendNotification);

        // When type changes, repopulate devices — still no manager changes
        mTypeBox.onChange = [this] { refreshDeviceList(); };
        refreshDeviceList();
    }

    void refreshDeviceList()
    {
        mDevBox.clear(juce::dontSendNotification);

        // Get device names directly from the type object — does NOT change
        // the manager's active device type.
        int typeIdx = mTypeBox.getSelectedItemIndex();
        auto& types = mMgr.getAvailableDeviceTypes();
        if (typeIdx < 0 || typeIdx >= types.size()) return;

        auto* typeObj = types[typeIdx];
        auto devNames = typeObj->getDeviceNames(false); // false = output
        for (int i = 0; i < devNames.size(); ++i)
            mDevBox.addItem(devNames[i], i + 1);

        // Pre-select: use snapshot name if this is the same type that's active
        bool sameType = (mMgr.getCurrentDeviceTypeObject() == typeObj);
        if (sameType && mSnapshot.outputDeviceName.isNotEmpty())
            mDevBox.setText(mSnapshot.outputDeviceName, juce::dontSendNotification);
        else if (mDevBox.getNumItems() > 0)
            mDevBox.setSelectedItemIndex(0, juce::dontSendNotification);

        populateRatesAndBuffers();
    }

    void populateRatesAndBuffers()
    {
        mRateBox.clear(juce::dontSendNotification);
        mBufBox.clear(juce::dontSendNotification);

        // Use a fixed standard list — avoids creating a temporary device object
        // (which can open COM interfaces and crash). If the device doesn't
        // support a chosen value, initialise() will return an error gracefully.
        static const int kRates[] = { 44100, 48000, 88200, 96000, 192000 };
        for (int r : kRates)
            mRateBox.addItem(juce::String(r) + " Hz", r);

        static const int kBufs[] = { 64, 128, 256, 512, 1024, 2048 };
        for (int b : kBufs)
            mBufBox.addItem(juce::String(b) + " samples", b);

        // Pre-select from snapshot; add the value if not in our standard list
        int curRate = (mSnapshot.sampleRate > 0.0) ? (int)mSnapshot.sampleRate : 44100;
        if (!mRateBox.getItemText(mRateBox.indexOfItemId(curRate)).isNotEmpty())
            mRateBox.addItem(juce::String(curRate) + " Hz", curRate);
        mRateBox.setSelectedId(curRate, juce::dontSendNotification);

        int curBuf = (mSnapshot.bufferSize > 0) ? mSnapshot.bufferSize : 512;
        if (!mBufBox.getItemText(mBufBox.indexOfItemId(curBuf)).isNotEmpty())
            mBufBox.addItem(juce::String(curBuf) + " samples", curBuf);
        mBufBox.setSelectedId(curBuf, juce::dontSendNotification);
    }

    // ── Apply — write settings to disk, prompt restart ────────────────────────
    // We never touch the live AudioDeviceManager here. WASAPI exclusive-mode
    // devices (USB gaming headsets etc.) cannot be closed and reopened safely
    // mid-session — any attempt crashes the message thread with no cleanup.
    // Instead we write the desired config to the settings XML and let the user
    // restart; initialise() on the next launch opens the new device cleanly.
    void applySettings()
    {
        // Start from the current saved state so we only change what we touched.
        auto xml = mMgr.createStateXml();
        if (!xml)
            xml = std::make_unique<juce::XmlElement>("DEVICESETUP");

        int typeIdx = mTypeBox.getSelectedItemIndex();
        auto& types = mMgr.getAvailableDeviceTypes();
        if (typeIdx >= 0 && typeIdx < types.size())
            xml->setAttribute("deviceType", types[typeIdx]->getTypeName());

        xml->setAttribute("audioOutputDeviceName", mDevBox.getText());
        xml->setAttribute("audioInputDeviceName",  "");

        if (mRateBox.getSelectedId() > 0)
            xml->setAttribute("audioDeviceRate",       (double)mRateBox.getSelectedId());
        if (mBufBox.getSelectedId() > 0)
            xml->setAttribute("audioDeviceBufferSize", mBufBox.getSelectedId());

        // Write to a PENDING file — not the live settings file.
        // shutdown() calls saveAudioSettings() which would overwrite the live
        // file with the OLD device state.  Using a pending file sidesteps that:
        // initialise() on next launch promotes it before the manager opens anything.
        // 2026-04-25: write the pending file as a SIBLING of the live settings
        // file (could be Documents or legacy Roaming after the P4b migration).
        // Hard-coding userApplicationDataDirectory dropped the pending file
        // into Roaming while startup looked for it next to the Documents file
        // → settings appeared "stuck".  VibesynthStandaloneApp::getAudioSettingsFile
        // is the single source of truth.
        auto settingsFile = VibesynthStandaloneApp::getAudioSettingsFile();
        settingsFile.getParentDirectory().createDirectory();
        auto f = settingsFile.getSiblingFile("audio_settings_pending.xml");
        bool written = f.replaceWithText(xml->toString());

        // Close the dialog first.
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);

        // Defer the prompt until AFTER the dialog is fully torn down.
        // Calling systemRequestedQuit() synchronously inside a ModalCallbackFunction
        // can crash because JUCE is mid-modal-cleanup — defer that too.
        juce::MessageManager::callAsync([written]
        {
            juce::String msg = written
                ? "Audio device changes will take effect after restarting BaySickDAW.\n\n"
                  "Note: any unsaved project changes will be lost.\n\nRestart now?"
                : "Could not write settings file - check folder permissions.";

            juce::AlertWindow::showOkCancelBox(
                juce::MessageBoxIconType::QuestionIcon,
                "Restart Required",
                msg,
                "Restart Now", "Later", nullptr,
                juce::ModalCallbackFunction::create([](int result)
                {
                    if (result == 1)
                        juce::MessageManager::callAsync([]
                        {
                            juce::JUCEApplication::getInstance()->systemRequestedQuit();
                        });
                }));
        });
    }

    juce::AudioDeviceManager&             mMgr;
    juce::AudioIODeviceCallback*          mCallback;
    juce::AudioDeviceManager::AudioDeviceSetup mSnapshot;

    juce::Label      mTypeLbl, mDevLbl, mRateLbl, mBufLbl;
    juce::ComboBox   mTypeBox, mDevBox, mRateBox, mBufBox;
    juce::TextButton mApplyBtn, mCloseBtn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsDialog)
};

// ─────────────────────────────────────────────────────────────────────────────
// Placeholder component used by Mixer and Effects tabs until Phase 2/5
// ─────────────────────────────────────────────────────────────────────────────
struct PlaceholderPage : public juce::Component
{
    juce::String mText;
    explicit PlaceholderPage(const juce::String& t) : mText(t) {}
    void paint(juce::Graphics& g) override
    {
        g.fillAll(VC::Bg);
        g.setColour(VC::TextDim);
        g.setFont(juce::Font(18.0f));
        g.drawText(mText, getLocalBounds(), juce::Justification::centred);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Ctor
// ─────────────────────────────────────────────────────────────────────────────
StandaloneEditor::StandaloneEditor(VibeSynthProcessor& p, StandalonePlayHead& ph,
                                   juce::AudioDeviceManager& dm)
    : mProcessor(p), mPlayHead(ph), mDeviceManager(dm),
      mUndoManager(100, 30)
{
    // Scan core sample library once at startup (non-blocking on message thread,
    // just a directory walk — typically < 10 ms)
    SampleLibrary::getInstance().scan();

    mPM = std::make_unique<PatternManager>();
    mProcessor.setPatternManager(mPM.get());

    // Project persistence (P1+). Constructed after PatternManager is wired so
    // the processor has what it needs for serializeProject/deserializeProject.
    mProjectManager = std::make_unique<ProjectManager>(mProcessor);
    // P4: one-shot first-launch tasks (Documents\BaySickDAW\ exists, Sample
    // Library.lnk points at CoreLibrary).  Idempotent; runs every launch but
    // guarded internally.
    mProjectManager->runFirstLaunchHousekeeping();

    // P5: dirty tracking wiring.
    mProjectManager->onDirtyChanged = [this] { refreshWindowTitle(); };
    mProcessor.onAnyStateChange = [this]
    {
        if (mProjectManager) mProjectManager->markDirty();
    };

    // P1+P2 persistence (2026-04-24): processor delegates tab + engine state
    // save/load to the editor.  These are fired inside serialize/deserialize.
    mProcessor.onSerializeUIState   = [this](juce::XmlElement& root)       { serializeUIState (root); };
    mProcessor.onDeserializeUIState = [this](const juce::XmlElement& root) { deserializeUIState (root); };

    // ── VKnob right-click → automation ───────────────────────────────────────
    VKnobAutomation::sOnAutomate = [this](const juce::String& pid)
    {
        openEventEditorForParam(pid);
    };

    // ── Automation applicator registration hook ───────────────────────────────
    VKnobAutomation::sOnRegisterApplicator = [this](const juce::String& pid,
                                                     std::function<void(float)> fn)
    {
        mAutomationApplicators[pid] = std::move(fn);
    };

    // ── Automation value reader registration hook ─────────────────────────────
    VKnobAutomation::sOnRegisterReader = [this](const juce::String& pid,
                                                 std::function<float()> fn)
    {
        mAutomationValueReaders[pid] = std::move(fn);
    };

    // ── Right-click menu label resolver ──────────────────────────────────────
    // Translates raw paramId to the friendly "Channel - Effect - Param" label
    // (honours userDisplayName) for the "Automate: X" menu item text.
    VKnobAutomation::sResolveMenuLabel = [this](const juce::String& pid) -> juce::String
    {
        return resolveAutomationDisplayName(pid);
    };

    // Auto-register all static APVTS params (instrument synths, EQ bands, etc.)
    for (auto* p : mProcessor.getParameters())
    {
        if (auto* rap = dynamic_cast<juce::RangedAudioParameter*>(p))
        {
            juce::String pid = rap->paramID;
            mAutomationApplicators[pid] = [rap](float v01)
            {
                rap->setValueNotifyingHost(v01);
            };
            mAutomationValueReaders[pid] = [rap]() -> float
            {
                return rap->getValue();  // already 0..1
            };
        }
    }

    // 2026-04-24: "global_tempo" automation.  Linear map 0..1 <-> 20..300 BPM.
    // Applicator updates the playhead + PatternManager's stored tempo; reader
    // returns normalised tempo.  Right-clicking the transport BPM field opens
    // the standard Event Editor via openEventEditorForParam("global_tempo"),
    // producing an Automation clip on the arrangement just like any other
    // automatable param.
    constexpr float kTempoMinBpm = 20.0f;
    constexpr float kTempoMaxBpm = 300.0f;
    mAutomationApplicators["global_tempo"] = [this, kTempoMinBpm, kTempoMaxBpm](float v01)
    {
        const double bpm = juce::jlimit (kTempoMinBpm, kTempoMaxBpm,
            kTempoMinBpm + v01 * (kTempoMaxBpm - kTempoMinBpm));
        mPlayHead.setBPM (bpm);
        if (mPM) mPM->setGlobalTempo (bpm);
    };
    mAutomationValueReaders["global_tempo"] = [this, kTempoMinBpm, kTempoMaxBpm]() -> float
    {
        const double bpm = mPlayHead.getBPM();
        return juce::jlimit (0.0f, 1.0f,
            (float) ((bpm - kTempoMinBpm) / (kTempoMaxBpm - kTempoMinBpm)));
    };

    // ── Global LAF + Tooltip ──────────────────────────────────────────────────
    juce::LookAndFeel::setDefaultLookAndFeel(&VibeLAF::get());
    mTooltipWindow = std::make_unique<VibeTooltip>(this, 600);

    // Global right-click listener — catches any slider with a componentID set
    addMouseListener(&mAutoRightClick, true);

    // ── Menu bar ─────────────────────────────────────────────────────────────
    mMenuBar = std::make_unique<juce::MenuBarComponent>(this);
    mMenuBar->setLookAndFeel(&VibeLAF::get());
    addAndMakeVisible(*mMenuBar);

    // ── Global Transport Bar — added FIRST so it is the background layer ──────
    // It spans the full combined toolbar width: transport controls left,
    // CPU/RAM label far right, empty in the middle (pattern + ribbon overlap on top).
    mTransport = std::make_unique<GlobalTransportBar>(ph);
    mTransport->onPlay  = [this]
    {
        // R5d (2026-04-24): mode-aware record start.  ASIO mode requires an
        // open project (for the Samples/ folder + project-name file prefix).
        // MIDI mode requires a last-accessed piano roll - wired in R5d-midi.
        if (mRecordArmed && ! mRecordingActive)
        {
            const auto mode = mTransport->getRecordMode();

            if (mode == GlobalTransportBar::RecordMode::Audio)
            {
                if (! mProjectManager || ! mProjectManager->hasProject())
                {
                    // 2026-04-24: prompt for a project name + Save As (NOT
                    // File > New).  Save As preserves the current in-memory
                    // state (any engines / patterns / arrangement the user
                    // built so far) and writes them into the new project
                    // folder.  File > New would have wiped all of it.  After
                    // the save succeeds, re-arm Record + continue into
                    // playback so the user doesn't have to hit the buttons
                    // again.
                    mRecordArmed = false;
                    if (mTransport) mTransport->setRecordArmed (false);
                    promptForProjectName (
                        "Save Project Before Recording",
                        "Your project isn't saved yet.  Give it a name so the "
                        "recorded audio has somewhere to live:",
                        "Untitled Project",
                        [this] (juce::String name)
                        {
                            if (! ProjectManager::isValidProjectName (name))
                            {
                                juce::AlertWindow::showMessageBoxAsync (
                                    juce::MessageBoxIconType::WarningIcon,
                                    "Invalid project name",
                                    "Project names can't contain < > : \" / \\ | ? * or be\n"
                                    "reserved device names.  Try another name.");
                                return;
                            }
                            if (! mProjectManager->saveProjectAs (name))
                            {
                                juce::AlertWindow::showMessageBoxAsync (
                                    juce::MessageBoxIconType::WarningIcon,
                                    "Save failed",
                                    "Couldn't create the project folder.  Try another name.");
                                return;
                            }
                            refreshWindowTitle();
                            // Pick up where the user left off: re-arm, start
                            // recording, start playback.
                            mRecordArmed = true;
                            if (mTransport) mTransport->setRecordArmed (true);
                            mProcessor.startRecording (
                                VibeSynthProcessor::RecordMode::Audio,
                                mPlayHead.getCurrentBeat(),
                                mProjectManager->getCurrentName(),
                                mProjectManager->getSamplesFolder());
                            mRecordingActive = true;
                            startPlayback (mTransport->getBPM());
                        });
                    return;
                }
                mProcessor.startRecording (
                    VibeSynthProcessor::RecordMode::Audio,
                    mPlayHead.getCurrentBeat(),
                    mProjectManager->getCurrentName(),
                    mProjectManager->getSamplesFolder());
            }
            else
            {
                // R5d-midi (2026-04-24): block if the user hasn't opened any
                // piano-roll tab yet this session - we'd have nowhere to put
                // the captured notes.  Once they've touched a Layers / Bass /
                // Drums tab, mLastRollKind stays set across tab switches so
                // MIDI record works even while they're viewing Mixer / Effects.
                if (mLastRollKind == LastRollKind::None)
                {
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::MessageBoxIconType::WarningIcon,
                        "No Active Piano Roll Selected",
                        "MIDI recording writes notes into whichever piano roll\n"
                        "you were last editing.  Open a Layers, Bass, or Drums\n"
                        "tab first, then arm Record and press Play.");
                    mRecordArmed = false;
                    if (mTransport) mTransport->setRecordArmed (false);
                    return;
                }
                mProcessor.startRecording (
                    VibeSynthProcessor::RecordMode::Midi,
                    mPlayHead.getCurrentBeat(),
                    {}, {});
            }
            mRecordingActive = true;
        }
        startPlayback (mTransport->getBPM());
    };
    mTransport->onPause = [this] {
        // R5b (2026-04-24): Pause always commits any active recording, same
        // as Stop.  The audible playback can resume on the next Play, but a
        // fresh Record arm is required for the next take.  Matches Jeff's
        // explicit spec: "Pause ALWAYS stops recording".
        if (mRecordingActive)
        {
            auto res = mProcessor.stopRecording();
            commitRecordingResult (res);
            mRecordingActive = false;
        }
        mRecordArmed = false;
        if (mTransport) mTransport->setRecordArmed (false);
        if (mPlayHead.isPlaying()) { mPlayHead.stop(); mTransport->setPlayState(false, true); }
    };
    mTransport->onStop  = [this]
    {
        // R5b: stop committing first so any in-flight buffer block lands in
        // the recorder before playback halts.  Disarm Record on Stop too -
        // matches FL Studio (one-shot record per Play) and prevents surprise
        // re-records on the next Play press.
        if (mRecordingActive)
        {
            auto res = mProcessor.stopRecording();
            commitRecordingResult (res);
            mRecordingActive = false;
        }
        mRecordArmed = false;
        if (mTransport) mTransport->setRecordArmed (false);
        stopPlayback();
        mTransport->setPlayState (false, false);
    };
    mTransport->onTempoChanged = [this](double bpm) {
        mPlayHead.setBPM(bpm);   // always update BPM so timer doesn't revert tap tempo
        if (mPlayHead.isPlaying()) mPlayHead.start(bpm);
        // 2026-04-24: tempo is a global PROJECT value; persist it through the
        // PatternManager so save/reload round-trips correctly.
        if (mPM) mPM->setGlobalTempo (bpm);
        if (mProjectManager) mProjectManager->markDirty();
    };
    mTransport->onSongModeChanged = [this](bool songMode) {
        mProcessor.setSongMode(songMode);
    };
    mTransport->onSongLoopModeChanged = [this](bool loop) {
        mProcessor.mSongLoopMode.store(loop, std::memory_order_relaxed);
    };
    mTransport->onGetLoopBeats = [this]() -> double {
        const bool songMode = mTransport->isSongMode();

        // ── Time-selection has priority in both modes ────────────────────
        if (songMode)
        {
            if (mBuilderPage && mBuilderPage->hasTimeSelection())
            {
                double startBeats = mBuilderPage->getTimeSelStartBars() * 4.0;
                double endBeats   = mBuilderPage->getTimeSelEndBars()   * 4.0;
                if (endBeats > startBeats)
                {
                    mPlayHead.setLoopStart(startBeats);
                    mProcessor.mCachedPatternLoopBeats.store(endBeats, std::memory_order_relaxed);
                    mProcessor.mSongEndBeats.store(0.0, std::memory_order_relaxed);
                    return endBeats;
                }
            }
        }
        else  // pattern mode
        {
            if (auto* roll = getActivePianoRollForLoop())
            {
                if (roll->hasTimeSelection())
                {
                    double startBeats = roll->getTimeSelBeatStart();
                    double endBeats   = roll->getTimeSelBeatEnd();
                    if (endBeats > startBeats)
                    {
                        mPlayHead.setLoopStart(startBeats);
                        mProcessor.mCachedPatternLoopBeats.store(endBeats, std::memory_order_relaxed);
                        mProcessor.mSongEndBeats.store(0.0, std::memory_order_relaxed);
                        return endBeats;
                    }
                }
            }
        }

        // ── No time-selection: clear loop start so wrap goes to 0 ───────
        mPlayHead.setLoopStart(0.0);

        if (songMode)
        {
            // Compute song end = max block end-beat across EVERY block
            // type.  2026-04-24 fix: earlier ClipType::Pattern filter was
            // wrongly copied from the pattern-loop-length code in
            // PatternManager::getEffectivePatternLoopBeats; in Song mode all
            // block types (Pattern / Audio / Automation) contribute to the
            // end of the song.  Also uses `effectiveLengthBeats` so recorded
            // audio clips' sub-bar exact length drives song end instead of
            // a ceil'd bar count.
            double songEnd = 0.0;
            if (mPM)
            {
                for (int i = 0; i < mPM->getNumBlocks(); ++i)
                {
                    const auto& blk = mPM->getBlock(i);
                    if (blk.muted) continue;
                    const double blkEnd = blk.startBar * 4.0 + effectiveLengthBeats (blk);
                    if (blkEnd > songEnd) songEnd = blkEnd;
                }
            }
            // No blocks → honor loop toggle. ON = 1-bar audition loop;
            // OFF = no wrap (playhead advances freely; no auto-stop since
            // there's no song end to detect).
            if (songEnd <= 0.0)
            {
                const bool loopMode = mTransport->isSongLoopMode();
                mProcessor.mSongLoopMode.store(loopMode, std::memory_order_relaxed);
                mProcessor.mSongEndBeats.store(0.0, std::memory_order_relaxed);
                mProcessor.mCachedPatternLoopBeats.store(loopMode ? 4.0 : 0.0,
                                                         std::memory_order_relaxed);
                return loopMode ? 4.0 : 0.0;
            }
            // With blocks: publish songEnd so the audio thread can detect the end.
            // If loop-mode is on, return songEnd as the wrap point. Otherwise 0 (no wrap);
            // the audio thread sets mRequestStop when beatStart passes songEnd.
            mProcessor.mSongEndBeats.store(songEnd, std::memory_order_relaxed);
            const bool loopMode = mTransport->isSongLoopMode();
            mProcessor.mSongLoopMode.store(loopMode, std::memory_order_relaxed);
            mProcessor.mCachedPatternLoopBeats.store(loopMode ? songEnd : 0.0,
                                                     std::memory_order_relaxed);
            return loopMode ? songEnd : 0.0;
        }

        // ── Pattern mode, no time-selection: loop full pattern content ──
        double beats = mPM ? mPM->getEffectivePatternLoopBeats() : 4.0;
        mProcessor.mCachedPatternLoopBeats.store(beats, std::memory_order_relaxed);
        mProcessor.mSongEndBeats.store(0.0, std::memory_order_relaxed);
        return beats;
    };
    // 1M: DSP load readout — poll processor atomics each timer tick
    mTransport->onGetDspLoad = [this] {
        return mProcessor.mAudioDspLoad.load(std::memory_order_relaxed);
    };
    mTransport->onGetDsp95 = [this] {
        return mProcessor.mDspOverload95.load(std::memory_order_relaxed);
    };
    // 12f: total host PDC + sample rate for the LAT readout in the 2x2 perf grid.
    mTransport->onGetLatencySamples = [this] {
        return mProcessor.getLatencySamples();
    };
    mTransport->onGetSampleRate = [this] {
        return mProcessor.getSampleRate();
    };
    mTransport->onRecord = [this](bool armed)
    {
        // R5b (2026-04-23): Record button now ONLY toggles the arm state.  No
        // playback / capture starts here - that happens when the user presses
        // Play (see onPlay).  Disarming mid-capture stops the recorder; a
        // fresh arm + Play sequence starts a new file.  Matches FL Studio.
        mRecordArmed = armed;
        if (! armed && mRecordingActive)
        {
            auto res = mProcessor.stopRecording();
            commitRecordingResult (res);
            mRecordingActive = false;
        }
    };
    mTransport->onMetronomeToggle = [this](bool on) {
        mProcessor.mMetro.enabled.store(on, std::memory_order_relaxed);
    };
    // 2026-04-24: tempo automation entry point.  Right-click on the BPM
    // field routes here; create an automation clip targeting "global_tempo"
    // (applicator registered in the ctor).
    mTransport->onAutomateTempo = [this] { openEventEditorForParam ("global_tempo"); };
    mTransport->onMetroSettings = [this] {
        auto* panel = new MetroPanel();
        panel->setVolume (mProcessor.mMetro.volume.load(std::memory_order_relaxed));
        panel->setSound  (mProcessor.mMetro.soundType.load(std::memory_order_relaxed));
        panel->setPrecountEnabled(mPrecountEnabled);
        panel->onVolumeChanged   = [this](float v) {
            mProcessor.mMetro.volume.store(v, std::memory_order_relaxed);
        };
        panel->onSoundChanged    = [this](int t) {
            mProcessor.mMetro.soundType.store(t, std::memory_order_relaxed);
        };
        panel->onPrecountChanged = [this](bool on) {
            mPrecountEnabled = on;
        };
        juce::CallOutBox::launchAsynchronously(std::unique_ptr<MetroPanel>(panel),
                                               mTransport->getMetroArrowScreenBounds(),
                                               nullptr);
    };
    addAndMakeVisible(*mTransport);

    // ── Title label — hidden (title now lives in the OS window title bar) ─────
    mTitleLabel = std::make_unique<juce::Label>();
    mTitleLabel->setText("BaySickDAW", juce::dontSendNotification);
    mTitleLabel->setFont(juce::Font(18.0f, juce::Font::bold));
    mTitleLabel->setColour(juce::Label::textColourId, VC::Highlight);
    mTitleLabel->setVisible(false);
    addAndMakeVisible(*mTitleLabel);

    // ── Pattern dropdown button ───────────────────────────────────────────────
    mPatternBtn = std::make_unique<juce::TextButton>();
    mPatternBtn->setTooltip("Select, rename or delete patterns");
    mPatternBtn->onClick = [this]
    {
        juce::PopupMenu m;
        int cur = mPM->getCurrentPatternIndex();
        int n   = mPM->getNumPatterns();

        // List all patterns — tick marks current
        for (int i = 0; i < n; ++i)
            m.addItem(i + 1, mPM->getPattern(i).name, true, i == cur);

        m.addSeparator();
        m.addItem(-1, juce::String(juce::CharPointer_UTF8("\xe2\x9e\x95")) + "  New Pattern");
        m.addSeparator();
        m.addItem(-2, "Rename...");
        m.addItem(-4, "Change Color...");   // F-1 (2026-04-26)
        m.addItem(-3, "Delete", n > 1);   // grey out if only one pattern

        auto bounds = mPatternBtn->getScreenBounds();
        m.showMenuAsync(
            juce::PopupMenu::Options().withTargetScreenArea(bounds),
            [this](int result)
            {
                if (result == 0) return;

                if (result > 0)
                {
                    // Pattern selected by index
                    mPM->setCurrentPattern(result - 1);
                    refreshPatternBox();
                }
                else if (result == -1)
                {
                    // New pattern
                    mPM->addPattern();
                    mPM->setCurrentPattern(mPM->getNumPatterns() - 1);
                    refreshPatternBox();
                }
                else if (result == -2)
                {
                    // Rename current pattern
                    int idx = mPM->getCurrentPatternIndex();
                    auto* aw = new juce::AlertWindow(
                        "Rename Pattern", "Enter a new name:",
                        juce::MessageBoxIconType::NoIcon);
                    aw->addTextEditor("name", mPM->currentPattern().name);
                    aw->addButton("OK", 1);
                    aw->addButton("Cancel", 0);
                    aw->enterModalState(true,
                        juce::ModalCallbackFunction::create(
                            [this, idx, aw](int r)
                            {
                                if (r == 1)
                                {
                                    auto newName = aw->getTextEditorContents("name").trim();
                                    if (newName.isNotEmpty())
                                    {
                                        mPM->renamePattern(idx, newName);
                                        refreshPatternBox();
                                    }
                                }
                            }),
                        true);
                }
                else if (result == -4)
                {
                    // F-1 (2026-04-26): Change current pattern's colour with
                    // a live-preview picker.
                    int idx = mPM->getCurrentPatternIndex();
                    if (idx < 0 || idx >= mPM->getNumPatterns()) return;
                    const juce::Colour curCol = mPM->getPattern(idx).color;
                    PatternColorPicker::showAsync (mPatternBtn.get(), curCol,
                        [this, idx] (juce::Colour newCol)
                        {
                            if (! mPM || idx < 0 || idx >= mPM->getNumPatterns()) return;
                            mPM->getPattern(idx).color = newCol;
                            refreshPatternBox();
                            if (mBuilderPage) mBuilderPage->repaint();
                        });
                }
                else if (result == -3)
                {
                    // Delete current pattern
                    int idx = mPM->getCurrentPatternIndex();
                    auto* aw = new juce::AlertWindow(
                        "Delete Pattern",
                        "Delete \"" + mPM->currentPattern().name + "\"? This cannot be undone.",
                        juce::MessageBoxIconType::WarningIcon);
                    aw->addButton("Delete", 1);
                    aw->addButton("Cancel", 0);
                    aw->enterModalState(true,
                        juce::ModalCallbackFunction::create(
                            [this, idx](int r)
                            {
                                if (r == 1)
                                {
                                    mPM->removePattern(idx);
                                    // clamp selection
                                    int newIdx = juce::jlimit(0, mPM->getNumPatterns() - 1, idx);
                                    mPM->setCurrentPattern(newIdx);
                                    refreshPatternBox();
                                }
                            }),
                        true);
                }
            });
    };
    addAndMakeVisible(*mPatternBtn);

    refreshPatternBox();

    // ── Ribbon Tab Bar ────────────────────────────────────────────────────────
    mRibbon = std::make_unique<RibbonTabBar>();
    mRibbon->onTabSelected    = [this](int id) { onTabSelected(id); };
    mRibbon->onTabClosed      = [this](int id) { onTabClosed(id);   };
    // D2: dropdown Delete routes through the page's requestDelete() so the
    // Save & Delete / Delete Anyway / Cancel prompt matches the right-click
    // context menu's Delete prompt exactly (single source of truth).
    mRibbon->onTabDeleteRequested = [this](int tabId) {
        for (auto* entry : mPages)
        {
            if (! entry || entry->ribbonTabId != tabId) continue;
            if (auto* lp = dynamic_cast<LayersPage*>(entry->component.get())) lp->requestDelete();
            else if (auto* bp = dynamic_cast<BassPage*> (entry->component.get())) bp->requestDelete();
            else if (auto* dp = dynamic_cast<DrumPage*> (entry->component.get())) dp->requestDelete();
            return;
        }
    };
    mRibbon->onAddTabRequest  = [this](RibbonTabBar::TabType t) { onAddTabRequest(t); };
    // G-2 (2026-04-28): empty-state hook for the Clip ribbon slot.
    mRibbon->onClipsEmptyStateRequested = [this]() { showClipsEmptyState(); };

    // G-2: Clips empty-state placeholder, shown when the user clicks the
    // Clip ribbon body and 0 Clip tabs exist.  Drop target — files dropped
    // here route through BuilderPage's existing importAudioFile flow which
    // fires onAudioClipAdded → spawns the first Clips tab automatically.
    mClipsEmptyState = std::make_unique<ClipsEmptyState>();
    addChildComponent (*mClipsEmptyState);   // hidden by default
    mClipsEmptyState->onClipDropped = [this] (const juce::String& filePath)
    {
        if (mBuilderPage && mBuilderPage->getGrid())
            mBuilderPage->getGrid()->importAudioFile (filePath, 0, 0.0f);
    };
    mRibbon->onSubPageSelected = [this](RibbonTabBar::TabType t, int idx) { onSubPageSelected(t, idx); };
    // D1.4-fix: intercept rename for Drum tabs whose name == "User Patch" so
    // we can route to Save Patch As (which prompts for name + saves the
    // preset XML, then auto-renames the tab via onSoundNameChanged).
    mRibbon->onRenameInterceptRequested = [this](int tabId) -> bool {
        for (auto* entry : mPages)
        {
            if (! entry || entry->ribbonTabId != tabId) continue;
            if (auto* dp = dynamic_cast<DrumPage*>(entry->component.get()))
            {
                const auto* tab = mRibbon->getTabById (tabId);
                const juce::String currentName = tab ? tab->name : juce::String();
                if (currentName == "User Patch")
                {
                    dp->savePatchAs();
                    return true;   // suppress default rename
                }
            }
            break;
        }
        return false;   // default rename
    };
    mRibbon->onTabRenamed     = [this](int id, const juce::String& name) {
        // 2026-04-21: auto-suffix duplicate tab names so automation-menu labels
        //   don't become ambiguous ("Drums - Harmless - Cutoff" twice). Build
        //   the set of existing names (excluding the tab being renamed) and
        //   append " (N)" until the candidate is unique.
        auto isDuplicate = [this, id](const juce::String& candidate) -> bool
        {
            for (auto* e : mPages)
            {
                if (!e || !e->component || e->ribbonTabId == id) continue;
                juce::String other;
                if (auto* lp = dynamic_cast<LayersPage*>(e->component.get())) other = lp->getTabName();
                else if (auto* bp = dynamic_cast<BassPage*>  (e->component.get())) other = bp->getTabName();
                else if (auto* dp = dynamic_cast<DrumPage*> (e->component.get())) other = dp->getTabName();
                if (other == candidate) return true;
            }
            return false;
        };

        juce::String finalName = name.isEmpty() ? juce::String("Untitled") : name;
        if (isDuplicate(finalName))
        {
            int n = 2;
            juce::String candidate;
            do { candidate = finalName + " (" + juce::String(n++) + ")"; }
            while (isDuplicate(candidate));
            finalName = candidate;
            // Reflect the auto-suffix back into the ribbon so the user sees it.
            if (mRibbon) mRibbon->renameTab(id, finalName);
        }

        // Sync ribbon tab rename → mixer strip name AND piano-roll context label.
        // Mixer Layer/Bass strips are keyed by pageIndex (NOT ribbonTabId), so
        // translate via mPages first.
        for (auto* entry : mPages)
        {
            if (!entry || entry->ribbonTabId != id) continue;
            if (auto* lp = dynamic_cast<LayersPage*>(entry->component.get()))
            {
                if (mMixerPage) mMixerPage->renameChannel(lp->getPageIndex(), finalName);
                lp->setTabName(finalName);
            }
            else if (auto* bp = dynamic_cast<BassPage*>(entry->component.get()))
            {
                if (mMixerPage) mMixerPage->renameChannel(bp->getPageIndex(), finalName);
                bp->setTabName(finalName);
            }
            else if (auto* dp = dynamic_cast<DrumPage*>(entry->component.get()))
            {
                if (mMixerPage) mMixerPage->renameChannel(dp->getPageIndex(), finalName);
                dp->setTabName(finalName);
            }
            break;
        }
        refreshAllKitViews();   // D2: ribbon rename → kit row labels
    };
    addAndMakeVisible(*mRibbon);

    // ── Page Menu Bar (Tier 2) ────────────────────────────────────────────────
    mPageMenuBar = std::make_unique<PageMenuBar>();
    mPageMenuBar->setPageTitle("BaySickDAW");
    addAndMakeVisible(*mPageMenuBar);

    // ── Build default tabs ────────────────────────────────────────────────────
    buildDefaultTabs();

    // ── Keymap framework (Phase A — 2026-04-26) ──────────────────────────────
    // ApplicationCommandManager registers all commands defined in BSCommands,
    // dispatches keypresses through KeyPressMappingSet -> perform().  The
    // GlobalTransportBar's old KeyListener role is retired; its public
    // togglePlayPause / stopAndDisarm / toggleRecord methods are invoked from
    // perform() so the same actions can be re-bound through Help > Key Binds.
    mCmdMgr.registerAllCommandsForTarget (this);

    if (auto* set = mCmdMgr.getKeyMappings())
    {
        // Apply defaults first; then overlay any user-saved bindings on top.
        set->resetToDefaultMappings();
        BSCommands::loadMappings (*set);
        addKeyListener (set);
    }
    setWantsKeyboardFocus(true);

    // 2026-04-26: deferred keyboard-focus grab.  At ctor time the window isn't
    // on screen yet so grabKeyboardFocus() is a no-op — defer to the next
    // message-loop tick.  Without this, keybinds don't fire until the user
    // clicks somewhere in the app first.
    juce::Component::SafePointer<StandaloneEditor> safe (this);
    juce::MessageManager::callAsync ([safe]
    {
        if (safe != nullptr) safe->grabKeyboardFocus();
    });

    // ── Automation playback timer ─────────────────────────────────────────────
    mAutomationTimer.startTimerHz(30);
}

StandaloneEditor::~StandaloneEditor()
{
    mAutomationTimer.stopTimer();
    removeMouseListener(&mAutoRightClick);
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    stopPlayback();
    mProcessor.setPatternManager(nullptr);
    // Detach the keymap-set listener installed in the ctor.  GlobalTransportBar
    // is no longer a KeyListener (Phase A 2026-04-26 — keymap migration).
    if (auto* set = mCmdMgr.getKeyMappings())
        removeKeyListener(set);

    // Null legacy pointers before OwnedArray destroys everything
    mLegacyLayersPage = nullptr;
    mLegacyBassPage   = nullptr;
    mBuilderPage      = nullptr;
    mEffectsPage      = nullptr;
    mMixerPage        = nullptr;
    mVisiblePage      = nullptr;
    mPages.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// Default tab construction
// ─────────────────────────────────────────────────────────────────────────────
void StandaloneEditor::buildDefaultTabs()
{
    // The RibbonTabBar already has 3 system tabs (Mixer=0, Effects=1, Builder=2).
    // We create matching PageEntry objects for each, then add default Layers/Bass/Drums.

    // Helper: add a page entry for a ribbon tab id
    auto addEntry = [this](int ribbonId, RibbonTabBar::TabType type,
                            std::unique_ptr<juce::Component> comp)
    {
        auto* entry = new PageEntry();
        entry->ribbonTabId = ribbonId;
        entry->type        = type;
        entry->component   = std::move(comp);
        addChildComponent(*entry->component);
        mPages.add(entry);
    };

    // The four system tabs already exist in the ribbon with IDs 1/2/3/4.
    // 2026-04-26: PianoRoll added as a fixed slot (id=4) — dynamic Layers /
    // Bass / Drums tabs now start at id=5.
    addEntry(1, RibbonTabBar::TabType::Mixer,     createMixerPage());
    addEntry(2, RibbonTabBar::TabType::Effects,   createEffectsPage());
    addEntry(3, RibbonTabBar::TabType::Builder,   createBuilderPage());
    addEntry(4, RibbonTabBar::TabType::PianoRoll, createPianoRollPage());

    // 2026-04-26 (1b + step 2): wire the unified Piano Roll page's DrumKit
    // container + capture the raw ptr for engine register / unregister.
    mPianoRollPage = dynamic_cast<PianoRollPage*> (mPages.getLast()->component.get());
    if (mPianoRollPage != nullptr)
    {
        mPianoRollPage->setPlayHead    (&mPlayHead);
        mPianoRollPage->isSongMode     = [this] { return mProcessor.isSongMode(); };
        mPianoRollPage->setUndoContext (makeUndoContext());
        wirePianoRollPageKitView (mPianoRollPage);

        // Step 2: dropdown enumerates engines from mPages (already in ribbon
        // order since pages are appended as ribbon tabs are added).  DrumKit
        // is added by buildEngineDropdown itself at the top of the menu.
        mPianoRollPage->dropdownEnumerator = [this]() {
            std::vector<PianoRollPage::DropdownEntry> out;
            for (auto* entry : mPages)
            {
                if (! entry) continue;
                EngineKind k;
                int idx = -1;
                if (auto* lp = dynamic_cast<LayersPage*> (entry->component.get()))
                {
                    k = EngineKind::Layer;
                    idx = lp->getPageIndex();
                }
                else if (auto* bp = dynamic_cast<BassPage*> (entry->component.get()))
                {
                    k = EngineKind::Bass;
                    idx = bp->getPageIndex();
                }
                else if (auto* dp = dynamic_cast<DrumPage*> (entry->component.get()))
                {
                    k = EngineKind::Drum;
                    idx = dp->getPageIndex();
                }
                else if (auto* cp = dynamic_cast<ClipsPage*> (entry->component.get()))
                {
                    // G-3 (2026-04-28): Clips engines surface in the dropdown
                    // alongside Layer/Bass/Drum so the user can pick them from
                    // the unified Piano Roll page.
                    k = EngineKind::Clip;
                    idx = cp->getPageIndex();
                }
                else continue;
                if (idx < 0 || ! mRibbon) continue;
                const auto* tab = mRibbon->getTabById (entry->ribbonTabId);
                if (! tab) continue;
                PianoRollPage::DropdownEntry e;
                e.id    = { k, idx };
                e.label = tab->name;
                out.push_back (std::move (e));
            }
            return out;
        };

        mPianoRollPage->onEngineSelected = [this](EngineId) {
            // Refresh the menu-bar pill label on the next showPageForTab pass
            // (onEngineSelected fires AFTER selectEngine completes).  If the
            // PianoRollPage is currently visible, force the setTabSlots
            // rebuild now so the label updates immediately.
            if (mVisiblePage == mPianoRollPage)
                showPageForTab (4);
        };
    }

    // Default dynamic tabs.
    addDefaultDynamicTabs();

    // Start on Builder tab (id=3)
    mRibbon->selectTab(3);
    onTabSelected(3);
}

void StandaloneEditor::addDefaultDynamicTabs()
{
    auto addEntry = [this](int ribbonId, RibbonTabBar::TabType type,
                            std::unique_ptr<juce::Component> comp)
    {
        auto* entry = new PageEntry();
        entry->ribbonTabId = ribbonId;
        entry->type        = type;
        entry->component   = std::move (comp);
        addChildComponent (*entry->component);
        mPages.add (entry);
    };

    int layersId = mRibbon->addTab (RibbonTabBar::TabType::Layers, "Layers");
    {
        auto lp = createLayersPage();
        if (auto* p = dynamic_cast<LayersPage*> (lp.get()))
        {
            const int pageIdx = p->getPageIndex();
            p->onEngineSelected = [this, layersId, pageIdx] {
                const auto* tab = mRibbon->getTabById (layersId);
                if (mMixerPage)   mMixerPage->addLayerChannel (pageIdx, tab ? tab->name : "Layers");
                if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
            };
            // D1.4-fix (c): per-layer Delete + Duplicate.
            p->onDeleteRequested = [this, layersId] {
                if (! mRibbon) return;
                if (mRibbon->isLastOfType (RibbonTabBar::TabType::Layers))
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                        "Cannot Delete",
                        "This is the only Layer tab. Add another first.");
                    return;
                }
                mRibbon->closeTab (layersId);
            };
            p->onDuplicateRequested = [this] (const juce::String& clipboardXml) {
                spawnDuplicateLayerTab (clipboardXml);
            };
            p->onLockChanged = [this, layersId, p] {
                if (mRibbon) mRibbon->setTabLocked (layersId, p->isLocked());
            };
            p->onRenameRequested = [this, layersId] {
                if (mRibbon) mRibbon->startRename (layersId);
            };
            p->onSoundNameChanged = [this, layersId, pageIdx] (const juce::String& nm) {
                if (nm.isEmpty()) return;
                if (mRibbon)    mRibbon->renameTab (layersId, nm);
                if (mMixerPage) mMixerPage->renameChannel (pageIdx, nm);
                if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Layer, pageIdx }, nm);
            };
            registerLayerPianoRoll (p);
        }
        addEntry (layersId, RibbonTabBar::TabType::Layers, std::move (lp));
    }

    int bassId = mRibbon->addTab (RibbonTabBar::TabType::Bass, "Bass");
    {
        auto bp = createBassPage();
        if (auto* p = dynamic_cast<BassPage*> (bp.get()))
        {
            const int pageIdx = p->getPageIndex();
            p->onEngineSelected = [this, bassId, pageIdx] {
                const auto* tab = mRibbon->getTabById (bassId);
                if (mMixerPage)   mMixerPage->addBassChannel (pageIdx, tab ? tab->name : "Bass");
                if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
            };
            p->onDeleteRequested = [this, bassId] {
                if (! mRibbon) return;
                if (mRibbon->isLastOfType (RibbonTabBar::TabType::Bass))
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                        "Cannot Delete",
                        "This is the only Bass tab. Add another first.");
                    return;
                }
                mRibbon->closeTab (bassId);
            };
            p->onDuplicateRequested = [this] (const juce::String& clipboardXml) {
                spawnDuplicateBassTab (clipboardXml);
            };
            p->onLockChanged = [this, bassId, p] {
                if (mRibbon) mRibbon->setTabLocked (bassId, p->isLocked());
            };
            p->onRenameRequested = [this, bassId] {
                if (mRibbon) mRibbon->startRename (bassId);
            };
            p->onSoundNameChanged = [this, bassId, pageIdx] (const juce::String& nm) {
                if (nm.isEmpty()) return;
                if (mRibbon)    mRibbon->renameTab (bassId, nm);
                if (mMixerPage) mMixerPage->renameChannel (pageIdx, nm);
                if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Bass, pageIdx }, nm);
            };
            registerBassPianoRoll (p);
        }
        addEntry (bassId, RibbonTabBar::TabType::Bass, std::move (bp));
    }

    int drumsId = mRibbon->addTab (RibbonTabBar::TabType::Drums, "Drums");
    {
        auto dp = createDrumPage();
        if (auto* p = dynamic_cast<DrumPage*> (dp.get()))
        {
            const int pageIdx = p->getPageIndex();
            p->onEngineSelected = [this, drumsId, pageIdx] {
                const auto* tab = mRibbon->getTabById (drumsId);
                if (mMixerPage)   mMixerPage->addDrumChannel (pageIdx, tab ? tab->name : "Drums");
                if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
                refreshAllKitViews();
            };
            p->onSoundNameChanged = [this, drumsId, pageIdx, p] (const juce::String& nm) {
                if (nm.isEmpty()) return;
                if (mRibbon)    mRibbon->renameTab (drumsId, nm);
                if (mMixerPage) mMixerPage->renameChannel (pageIdx, nm);
                p->setTabName (nm);
                refreshAllKitViews();
                if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Drum, pageIdx }, nm);
            };
            p->onDeleteRequested = [this, drumsId] {
                if (! mRibbon) return;
                if (mRibbon->isLastOfType (RibbonTabBar::TabType::Drums))
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                        "Cannot Delete",
                        "This is the only Drum tab. Add another first.");
                    return;
                }
                mRibbon->closeTab (drumsId);   // fires onTabClosed -> mPages cleanup
            };
            p->onDuplicateRequested = [this] (const juce::String& clipboardXml) {
                spawnDuplicateDrumTab (clipboardXml);
            };
            p->onLockChanged = [this, drumsId, p] {
                if (mRibbon) mRibbon->setTabLocked (drumsId, p->isLocked());
                refreshAllKitViews();
            };
            p->onRenameRequested = [this, drumsId] {
                if (mRibbon) mRibbon->startRename (drumsId);
            };
            wireDrumPageKitView (p);
            registerDrumPianoRoll (p);
        }
        addEntry (drumsId, RibbonTabBar::TabType::Drums, std::move (dp));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Page factory methods
// ─────────────────────────────────────────────────────────────────────────────
std::unique_ptr<juce::Component> StandaloneEditor::createLayersPage()
{
    // Find first available page index (0–kMaxLayerPages-1)
    int idx = -1;
    for (int i = 0; i < kMaxLayerPages; ++i)
    {
        if (!mUsedLayerIndices[i]) { idx = i; break; }
    }
    if (idx < 0) return nullptr;  // all 8 slots in use

    mUsedLayerIndices[idx] = true;
    auto page = std::make_unique<LayersPage>(mProcessor, *mPM, idx);
    page->setPlayHead(&mPlayHead);
    page->setUndoContext(makeUndoContext());
    // 2026-04-22: Removed auto-mode-swap (sub-tab → Pattern/Song).  User now
    // chooses Pattern or Song explicitly via the transport button.
    mLegacyLayersPage = page.get();
    return page;
}

std::unique_ptr<juce::Component> StandaloneEditor::createLayersPageAtIndex (int idx)
{
    if (idx < 0 || idx >= kMaxLayerPages) return nullptr;
    if (mUsedLayerIndices[idx]) return nullptr;
    mUsedLayerIndices[idx] = true;
    auto page = std::make_unique<LayersPage> (mProcessor, *mPM, idx);
    page->setPlayHead (&mPlayHead);
    page->setUndoContext (makeUndoContext());
    mLegacyLayersPage = page.get();
    return page;
}

std::unique_ptr<juce::Component> StandaloneEditor::createBassPageAtIndex (int idx)
{
    if (idx < 0 || idx >= kMaxBassPages) return nullptr;
    if (mUsedBassIndices[idx]) return nullptr;
    mUsedBassIndices[idx] = true;
    auto page = std::make_unique<BassPage> (mProcessor, *mPM, idx);
    page->setPlayHead (&mPlayHead);
    page->setUndoContext (makeUndoContext());
    mLegacyBassPage = page.get();
    return page;
}

std::unique_ptr<juce::Component> StandaloneEditor::createBassPage()
{
    // Find first available page index (0–kMaxBassPages-1)
    int idx = -1;
    for (int i = 0; i < kMaxBassPages; ++i)
    {
        if (!mUsedBassIndices[i]) { idx = i; break; }
    }
    if (idx < 0) return nullptr;  // all 4 slots in use

    mUsedBassIndices[idx] = true;
    auto page = std::make_unique<BassPage>(mProcessor, *mPM, idx);
    page->setPlayHead(&mPlayHead);
    page->setUndoContext(makeUndoContext());
    // 2026-04-22: Removed auto-mode-swap.  See createLayersPage comment.
    mLegacyBassPage = page.get();
    return page;
}

std::unique_ptr<juce::Component> StandaloneEditor::createDrumPage()
{
    int idx = -1;
    for (int i = 0; i < kMaxDrumPages; ++i)
        if (! mUsedDrumIndices[i]) { idx = i; break; }
    if (idx < 0) return nullptr;   // all 16 slots in use

    mUsedDrumIndices[idx] = true;
    auto page = std::make_unique<DrumPage>(mProcessor, *mPM, idx);
    page->setPlayHead(&mPlayHead);
    page->setUndoContext(makeUndoContext());
    mLegacyDrumPage = page.get();
    return page;
}

void StandaloneEditor::spawnDuplicateLayerTab (const juce::String& clipboardXml)
{
    auto page = createLayersPage();
    if (! page) return;
    auto* lp = dynamic_cast<LayersPage*> (page.get());
    if (lp == nullptr) return;
    const int pageIdx = lp->getPageIndex();
    const int newId = mRibbon->addTab (RibbonTabBar::TabType::Layers, "Layers");
    lp->onEngineSelected = [this, newId, pageIdx] {
        const auto* tab = mRibbon->getTabById (newId);
        if (mMixerPage)   mMixerPage->addLayerChannel (pageIdx, tab ? tab->name : "Layers");
        if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
    };
    lp->onDeleteRequested = [this, newId] {
        if (! mRibbon) return;
        if (mRibbon->isLastOfType (RibbonTabBar::TabType::Layers)) return;
        mRibbon->closeTab (newId);
    };
    lp->onDuplicateRequested = [this] (const juce::String& xml) {
        spawnDuplicateLayerTab (xml);
    };
    lp->onLockChanged = [this, newId, lp] {
        if (mRibbon) mRibbon->setTabLocked (newId, lp->isLocked());
    };
    lp->onRenameRequested = [this, newId] {
        if (mRibbon) mRibbon->startRename (newId);
    };
    lp->onSoundNameChanged = [this, newId, pageIdx] (const juce::String& nm) {
        if (nm.isEmpty()) return;
        if (mRibbon)    mRibbon->renameTab (newId, nm);
        if (mMixerPage) mMixerPage->renameChannel (pageIdx, nm);
        if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Layer, pageIdx }, nm);
    };
    registerLayerPianoRoll (lp);
    auto* entry = new PageEntry();
    entry->ribbonTabId = newId;
    entry->type        = RibbonTabBar::TabType::Layers;
    entry->component   = std::move (page);
    addChildComponent (*entry->component);
    mPages.add (entry);
    lp->importLayerState (clipboardXml);
    mRibbon->selectTab (newId);
    onTabSelected (newId);
}

void StandaloneEditor::spawnDuplicateBassTab (const juce::String& clipboardXml)
{
    auto page = createBassPage();
    if (! page) return;
    auto* bp = dynamic_cast<BassPage*> (page.get());
    if (bp == nullptr) return;
    const int pageIdx = bp->getPageIndex();
    const int newId = mRibbon->addTab (RibbonTabBar::TabType::Bass, "Bass");
    bp->onEngineSelected = [this, newId, pageIdx] {
        const auto* tab = mRibbon->getTabById (newId);
        if (mMixerPage)   mMixerPage->addBassChannel (pageIdx, tab ? tab->name : "Bass");
        if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
    };
    bp->onDeleteRequested = [this, newId] {
        if (! mRibbon) return;
        if (mRibbon->isLastOfType (RibbonTabBar::TabType::Bass)) return;
        mRibbon->closeTab (newId);
    };
    bp->onDuplicateRequested = [this] (const juce::String& xml) {
        spawnDuplicateBassTab (xml);
    };
    bp->onLockChanged = [this, newId, bp] {
        if (mRibbon) mRibbon->setTabLocked (newId, bp->isLocked());
    };
    bp->onRenameRequested = [this, newId] {
        if (mRibbon) mRibbon->startRename (newId);
    };
    bp->onSoundNameChanged = [this, newId, pageIdx] (const juce::String& nm) {
        if (nm.isEmpty()) return;
        if (mRibbon)    mRibbon->renameTab (newId, nm);
        if (mMixerPage) mMixerPage->renameChannel (pageIdx, nm);
        if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Bass, pageIdx }, nm);
    };
    registerBassPianoRoll (bp);
    auto* entry = new PageEntry();
    entry->ribbonTabId = newId;
    entry->type        = RibbonTabBar::TabType::Bass;
    entry->component   = std::move (page);
    addChildComponent (*entry->component);
    mPages.add (entry);
    bp->importBassState (clipboardXml);
    mRibbon->selectTab (newId);
    onTabSelected (newId);
}

void StandaloneEditor::spawnDuplicateDrumTab (const juce::String& clipboardXml)
{
    // D1.4-fix (c): Duplicate Drum action — find the next free drum index,
    // create a new DrumPage at that slot, wire its callbacks, then apply the
    // serialized state.  Mirrors the onAddTabRequest(Drums) flow + paste.
    auto page = createDrumPage();
    if (! page) return;   // 16-drum cap reached
    auto* dp = dynamic_cast<DrumPage*> (page.get());
    if (dp == nullptr) return;

    const int pageIdx = dp->getPageIndex();
    const int newId = mRibbon->addTab (RibbonTabBar::TabType::Drums, "Drums");
    dp->onEngineSelected = [this, newId, pageIdx] {
        const auto* tab = mRibbon->getTabById (newId);
        if (mMixerPage)   mMixerPage->addDrumChannel (pageIdx, tab ? tab->name : "Drums");
        if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
        refreshAllKitViews();
    };
    dp->onSoundNameChanged = [this, newId, pageIdx, dp] (const juce::String& nm) {
        if (nm.isEmpty()) return;
        if (mRibbon)    mRibbon->renameTab (newId, nm);
        if (mMixerPage) mMixerPage->renameChannel (pageIdx, nm);
        dp->setTabName (nm);
        refreshAllKitViews();
        if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Drum, pageIdx }, nm);
    };
    dp->onDeleteRequested = [this, newId] {
        if (! mRibbon) return;
        if (mRibbon->isLastOfType (RibbonTabBar::TabType::Drums))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                "Cannot Delete",
                "This is the only Drum tab. Add another first.");
            return;
        }
        mRibbon->closeTab (newId);
    };
    dp->onDuplicateRequested = [this] (const juce::String& xml) {
        spawnDuplicateDrumTab (xml);
    };
    dp->onLockChanged = [this, newId, dp] {
        if (mRibbon) mRibbon->setTabLocked (newId, dp->isLocked());
        refreshAllKitViews();
    };
    dp->onRenameRequested = [this, newId] {
        if (mRibbon) mRibbon->startRename (newId);
    };
    wireDrumPageKitView (dp);
    registerDrumPianoRoll (dp);

    auto* entry = new PageEntry();
    entry->ribbonTabId = newId;
    entry->type        = RibbonTabBar::TabType::Drums;
    entry->component   = std::move (page);
    addChildComponent (*entry->component);
    mPages.add (entry);

    dp->importDrumState (clipboardXml);

    mRibbon->selectTab (newId);
    onTabSelected (newId);
    refreshAllKitViews();
}

std::unique_ptr<juce::Component> StandaloneEditor::createDrumPageAtIndex (int idx)
{
    if (idx < 0 || idx >= kMaxDrumPages) return nullptr;
    if (mUsedDrumIndices[idx]) return nullptr;
    mUsedDrumIndices[idx] = true;
    auto page = std::make_unique<DrumPage>(mProcessor, *mPM, idx);
    page->setPlayHead(&mPlayHead);
    page->setUndoContext(makeUndoContext());
    mLegacyDrumPage = page.get();
    return page;
}

// 2026-04-25: createDrumsPage() removed — legacy DrumsPage class deleted.
// Drum tabs are now created via createDrumPage() (singular) which mirrors
// the LayersPage / BassPage pattern.

// 2026-04-26 (1a): Unified Piano Roll page.  Stub body; populated in step 1b
// (DrumKit container) + step 2 (per-engine rolls).
std::unique_ptr<juce::Component> StandaloneEditor::createPianoRollPage()
{
    return std::make_unique<PianoRollPage>();
}

std::unique_ptr<juce::Component> StandaloneEditor::createBuilderPage()
{
    auto page = std::make_unique<BuilderPage>(mProcessor, *mPM);
    page->setPlayHead(&mPlayHead);
    page->setUndoContext(makeUndoContext());
    mBuilderPage = page.get();

    // Wire grid callbacks
    if (auto* grid = page->getGrid())
    {
        grid->onOpenEventEditor = [this](int blockIdx)
        {
            openEventEditor(blockIdx);
        };

        grid->onAudioClipAdded = [this](int row, const juce::String& rowName, const juce::String& filePath)
        {
            juce::String name = rowName.isNotEmpty() ? rowName
                                                     : "Audio " + juce::String(row + 1);
            // Create mixer strip for this row if not already present
            if (mMixerPage)
                mMixerPage->addAudioChannel(row, name);

            // Create per-clip EffectRack in VibeGraph (ID 400+row)
            mProcessor.mVibeGraph.addAudioRowChannel(row, name);
            // 5F-4a: ensure APVTS params + Audio InsertNode for this row.
            mProcessor.ensureAudioInsert(row, name);

            // Rebuild the Effects dropdown to include the new clip channel
            if (mEffectsPage)
                mEffectsPage->rebuildChannelDropdown();

            // Rebuild audio readers so the new clip plays back immediately
            mProcessor.rebuildAudioClipPlayers();

            // 2026-04-28 (G-2/G-3): spawn a Clips ribbon tab + page for this
            // audio file if one doesn't already exist (idempotent on re-
            // import).  pageIdx = audioRow so the engine's audio mixes into
            // the matching mixer_audio_<row> insert.
            spawnClipsTabIfMissing (row, filePath);
        };
        grid->onArrangementChanged = [this]()
        {
            mProcessor.rebuildAudioClipPlayers();
        };
        // P4: copy-on-drop.  When the user drops a WAV onto the arrangement,
        // ProjectManager::importSample copies it into <project>/Samples/ and
        // returns the relative "Samples/<filename>" string to store on the
        // block.  No project open -> returns empty, triggering
        // onDropWithoutProject below to prompt + retry.
        grid->onImportSampleRequest = [this](const juce::File& src) -> juce::String
        {
            if (mProjectManager && mProjectManager->hasProject())
                return mProjectManager->importSample (src);
            return {};
        };
        // P4: path resolver for stored (possibly relative) audioFilePath.
        grid->onResolveStoredPath = [this](const juce::String& stored)
        {
            return mProcessor.resolveProjectFile (stored);
        };
        // P4: drop-without-project -> async New Project prompt, then retry
        // the drop so the audio file lands on the arrangement as intended.
        grid->onDropWithoutProject = [this, grid](const juce::File& src,
                                                   int row, float bar)
        {
            promptForProjectName (
                "New Project",
                "To save your audio, give this project a name.\n\n"
                "A folder will be created at:\n"
                + ProjectManager::getDefaultProjectsRoot().getFullPathName()
                + "\\<name>\\\n\n"
                  "Your audio file will be copied into that project's Samples\n"
                  "folder automatically.",
                "Untitled Project",
                [this, grid, src, row, bar] (juce::String name)
                {
                    if (! ProjectManager::isValidProjectName (name))
                    {
                        juce::AlertWindow::showMessageBoxAsync (
                            juce::MessageBoxIconType::WarningIcon,
                            "Invalid project name",
                            "Project names can't contain < > : \" / \\ | ? * or be\n"
                            "reserved device names (CON, PRN, AUX, NUL, COM1-9,\n"
                            "LPT1-9).  Try dropping the file again.");
                        return;
                    }
                    if (! mProjectManager->newProject (name))
                    {
                        juce::AlertWindow::showMessageBoxAsync (
                            juce::MessageBoxIconType::WarningIcon,
                            "Could not create project",
                            "Check that the Projects folder is writable and try again.");
                        return;
                    }
                    mProjectManager->saveProject();
                    refreshWindowTitle();
                    // Retry the drop - onImportSampleRequest will now succeed
                    // since hasProject() is true.
                    grid->importAudioFile (src.getFullPathName(), row, bar);
                });
        };
        // Display-name resolver so on-grid automation block labels use the
        // friendly "Channel - Effect - Param" format (honours userDisplayName).
        grid->onResolveDisplayName = [this](const AutomationLane& lane) -> juce::String
        {
            return displayNameFor(lane);
        };
    }

    // Wire the browser panel's display-name resolver so automation items in
    // the Browser show the same "Channel - Effect - Param" / userDisplayName
    // labels as the Event Editor.
    if (auto* panel = page->getBrowserPanel())
    {
        panel->onResolveDisplayName = [this](const AutomationLane& lane) -> juce::String
        {
            return displayNameFor(lane);
        };
    }

    return page;
}

void StandaloneEditor::openEventEditor(int blockIdx)
{
    if (!mPM || blockIdx < 0 || blockIdx >= mPM->getNumBlocks()) return;

    // If an editor for this block is already open, bring it to front
    for (auto* ed : mEventEditors)
    {
        if (ed->getBlockIdx() == blockIdx)
        {
            ed->toFront(true);
            return;
        }
    }

    // Build window title using the display resolver so it matches the right-click
    // menu / Browser / grid label ("Channel - Effect - Param" or userDisplayName).
    juce::String title = "Event Editor";
    if (blockIdx < mPM->getNumBlocks())
    {
        const auto& lane = mPM->getBlock(blockIdx).automationLane;
        const juce::String pretty = displayNameFor(lane);
        if (pretty.isNotEmpty())
            title = "Event Editor - " + pretty;
    }

    auto* ed = new EventEditor(mProcessor, mUndoManager, mPM.get(), blockIdx, title);
    ed->onClosed = [this](EventEditor* w)
    {
        // Remove from owned list (this deletes the object)
        mEventEditors.removeObject(w, true);
    };

    // Wire browser pane callbacks
    if (auto* content = ed->getContent())
    {
        content->onGetParamList = [this]() -> juce::StringArray
        {
            juce::StringArray params;
            for (auto& kv : mAutomationApplicators)
                params.add(kv.first);
            params.sort(true);
            return params;
        };
        content->onCreateAutomation = [this](const juce::String& pid) -> int
        {
            return createAutomationBlock(pid);
        };
        // Display-name resolver used by the title label and window caption.
        content->onResolveDisplayName = [this](const AutomationLane& lane) -> juce::String
        {
            return displayNameFor(lane);
        };
        // Forward the resolver to the browser pane inside the content so its
        // row labels match (pane is private; expose via accessor on content).
        if (auto* pane = content->getBrowserPane())
            pane->onResolveDisplayName = content->onResolveDisplayName;
    }

    mEventEditors.add(ed);
}

void StandaloneEditor::applyAutomationAtCurrentPosition()
{
    // Audio thread may have requested transport stop (end of song in play-through mode).
    // Handle on the UI thread so we can safely call stopPlayback().
    if (mProcessor.mRequestStop.exchange(false, std::memory_order_acq_rel))
    {
        if (mPlayHead.isPlaying()) stopPlayback();
    }

    if (!mPM || !mPlayHead.isPlaying()) return;

    const double beatsPerBar   = 4.0;   // TODO: read from PatternManager time signature
    const double currentBeats  = mPlayHead.getCurrentBeat();

    for (int i = 0; i < mPM->getNumBlocks(); ++i)
    {
        const auto& block = mPM->getBlock(i);
        if (block.clipType != ClipType::Automation) continue;
        if (block.muted) continue;

        const double blockStart     = block.startBar * beatsPerBar;
        const double clipLenBeats   = block.lengthBars * beatsPerBar;
        const double blockEnd       = blockStart + clipLenBeats;

        if (currentBeats < blockStart || currentBeats >= blockEnd) continue;

        const auto& lane = block.automationLane;
        if (lane.paramId.isEmpty()) continue;

        // Normalised position within clip (0..1)
        const float pos01 = (float)((currentBeats - blockStart) / clipLenBeats);
        const float value = lane.evaluateAt(pos01);

        auto it = mAutomationApplicators.find(lane.paramId);
        if (it != mAutomationApplicators.end())
            it->second(value);
    }
}

int StandaloneEditor::createAutomationBlock(const juce::String& paramId)
{
    if (!mPM || paramId.isEmpty()) return -1;

    // Search for existing block
    for (int i = 0; i < mPM->getNumBlocks(); ++i)
    {
        const auto& b = mPM->getBlock(i);
        if (b.clipType == ClipType::Automation && b.automationLane.paramId == paramId)
            return i;
    }

    // Find next free track row
    int usedRows = 0;
    for (int i = 0; i < mPM->getNumBlocks(); ++i)
        usedRows = juce::jmax(usedRows, mPM->getBlock(i).trackRow + 1);

    // Determine start + length
    int newStart  = 0;
    int newLength = 1;
    if (mBuilderPage && mBuilderPage->hasTimeSelection())
    {
        newStart  = (int)std::floor(mBuilderPage->getTimeSelStartBars());
        int selEnd = (int)std::ceil(mBuilderPage->getTimeSelEndBars());
        newLength = std::max(1, selEnd - newStart);
    }
    else
    {
        int maxEnd = 0;
        for (int i = 0; i < mPM->getNumBlocks(); ++i)
        {
            const auto& b = mPM->getBlock(i);
            maxEnd = std::max(maxEnd, b.startBar + b.lengthBars);
        }
        if (maxEnd > 0) newLength = maxEnd;
    }

    // Seed value
    float seedVal = 0.5f;
    {
        auto it = mAutomationValueReaders.find(paramId);
        if (it != mAutomationValueReaders.end() && it->second)
            seedVal = juce::jlimit(0.f, 1.f, it->second());
    }

    ArrangementBlock block;
    block.clipType       = ClipType::Automation;
    block.trackRow       = usedRows;
    block.startBar       = newStart;
    block.lengthBars     = newLength;
    block.patternIndex   = mPM->getCurrentPatternIndex();
    block.layerTrack     = false;
    block.automationLane.paramId = paramId;

    ControlPoint cpStart, cpEnd;
    cpStart.timeTicks = 0.f;  cpStart.value01 = seedVal;
    cpEnd  .timeTicks = 1.f;  cpEnd  .value01 = seedVal;
    block.automationLane.points = { cpStart, cpEnd };

    mPM->addBlock(block);
    // Also register the lane in the persistent template library so the new
    // automation shows up in the Browser panel alongside builder-created
    // blocks. addAutomationTemplate dedupes by paramId, so revisiting the
    // same control won't spam duplicates.
    mPM->addAutomationTemplate(block.automationLane);
    return mPM->getNumBlocks() - 1;
}

void StandaloneEditor::openEventEditorForParam(const juce::String& paramId)
{
    if (!mPM || paramId.isEmpty()) return;
    int idx = createAutomationBlock(paramId);
    if (idx >= 0) openEventEditor(idx);
}

// ─────────────────────────────────────────────────────────────────────────────
// Automation display-name resolver
//
// Naming convention — every label starts with one of two prefixes so the
// source surface is unambiguous at a glance:
//   Mx ...   anything controllable on a mixer strip (rack effects, EQ bands,
//            level / pan / mute / solo / polarity / width / bypass / arm,
//            sendTo / sends). Includes Master + bus strips.
//   Pg ...   anything controllable on an instrument page tab (engine knobs
//            on the Player tab, per-page EQ tab on Layers / Bass).
//
// Pages have only Player + Piano Roll + EQ tabs; they have NO effects rack.
// All effect racks live on mixer strips.
//
// Drum-slot engine params (tk_drm_N_s{S}_*) — intentionally NOT prefixed;
// labelled as "{Drums tab name} Slot {S+1} - {engine} - {param}" (no Mx/Pg
// prefix since drum slots span both surfaces).  §P4.3 B7 removed the legacy
// drums_{mid,side}_eq* block — per-slot pre-rack EQ now resolves through the
// mixer_drum_{N}_preeq_* form handled by tryMixerNonSlot.
//
// If parsing fails entirely, returns the paramId unchanged so old presets and
// edge cases never show blank labels.
// ─────────────────────────────────────────────────────────────────────────────
juce::String StandaloneEditor::resolveAutomationDisplayName(const juce::String& paramId) const
{
    if (paramId.isEmpty()) return paramId;

    using Kind = VibeGraph::InsertKind;

    // 2026-04-21: engine-tag -> user-facing engine name
    //   (must match the 3-char trackId tags in each engine ctor).
    auto engineLabelFromTag = [](const juce::String& tag) -> juce::String
    {
        if (tag == "harm") return "Harmless";
        if (tag == "bsp")  return "BaySickPlayer";
        if (tag == "bss")  return "BaySickSynth";
        if (tag == "bsb")  return "BaySickBass";
        return {};
    };

    // Resolve a page's current tab name (user-renameable). Falls back to the
    // default "Layer N" / "Bass N" / "Drums" if the page isn't found.
    auto lookupPageTabName = [this](const juce::String& pagePrefix, int pageIndex) -> juce::String
    {
        for (auto* entry : mPages)
        {
            if (entry == nullptr || entry->component == nullptr) continue;
            if (pagePrefix == "lay")
            {
                if (auto* lp = dynamic_cast<LayersPage*>(entry->component.get()))
                    if (lp->getPageIndex() == pageIndex) return lp->getTabName();
            }
            else if (pagePrefix == "bas")
            {
                if (auto* bp = dynamic_cast<BassPage*>(entry->component.get()))
                    if (bp->getPageIndex() == pageIndex) return bp->getTabName();
            }
            else if (pagePrefix == "drm")
            {
                if (auto* dp = dynamic_cast<DrumPage*>(entry->component.get()))
                    if (dp->getPageIndex() == pageIndex) return dp->getTabName();
            }
        }
        return {};
    };

    // "Title Case" a snake-case suffix. "wet_dry" -> "Wet Dry"; "outputvol" stays "Outputvol".
    auto prettifyParam = [](juce::String raw) -> juce::String
    {
        raw = raw.replaceCharacter('_', ' ').trim();
        if (raw.isEmpty()) return raw;
        juce::String out;
        bool capitalizeNext = true;
        for (int i = 0; i < raw.length(); ++i)
        {
            const juce::juce_wchar c = raw[i];
            if (c == ' ') { out += ' '; capitalizeNext = true; continue; }
            out += capitalizeNext ? juce::CharacterFunctions::toUpperCase(c) : c;
            capitalizeNext = false;
        }
        return out;
    };

    // Look up the effect loaded in slot `slotN` of a given rack.
    auto effectFromRack = [](EffectRack* rack, int slotN) -> juce::String
    {
        if (rack == nullptr) return {};
        if (slotN < 0 || slotN >= EffectRack::kNumSlots) return {};
        const auto& slot = rack->getSlot(slotN);
        if (slot.type == EffectType::None) return {};
        return SlotComponent::effectTypeName(slot.type);
    };

    // Parse on "_s{N}_" boundary to separate channel prefix + slot + param.
    auto splitSlotAndParam = [](const juce::String& id,
                                juce::String& outBase, int& outSlot,
                                juce::String& outParam) -> bool
    {
        const int len = id.length();
        int i = 0;
        while (i < len)
        {
            i = id.indexOfIgnoreCase(i, "_s");
            if (i < 0) return false;
            int digitStart = i + 2;
            int j = digitStart;
            while (j < len && juce::CharacterFunctions::isDigit((char) id[j])) ++j;
            if (j > digitStart && j < len && id[j] == '_')
            {
                outBase  = id.substring(0, i);
                outSlot  = id.substring(digitStart, j).getIntValue();
                outParam = id.substring(j + 1);
                return true;
            }
            i = digitStart;
        }
        return false;
    };

    juce::String base, param;
    int slotN = -1;
    const bool hasSlot = splitSlotAndParam(paramId, base, slotN, param);

    auto& vg = const_cast<VibeSynthProcessor&>(mProcessor).mVibeGraph;

    auto stitch = [](const juce::String& channelLabel,
                     const juce::String& effectLabel,
                     const juce::String& paramLabel) -> juce::String
    {
        juce::String out = channelLabel;
        if (effectLabel.isNotEmpty()) out += " - " + effectLabel;
        if (paramLabel.isNotEmpty())  out += " - " + paramLabel;
        return out;
    };

    if (hasSlot)
    {
        const juce::String prettyParam = prettifyParam(param);

        // Bus racks (short-prefix form used by EffectsPage::getChannelPrefix).
        if (base == "layers_bus") return stitch("Mx Layers Bus",  effectFromRack(vg.getLayersBusRack(),    slotN), prettyParam);
        if (base == "bass_bus")   return stitch("Mx Bass Bus",    effectFromRack(vg.getBassBusRack(),      slotN), prettyParam);
        if (base == "drums_bus")  return stitch("Mx Drums Bus",   effectFromRack(vg.getDrumsBusRack(),     slotN), prettyParam);
        if (base == "master")     return stitch("Mx Master",      effectFromRack(vg.getMasterRack(),       slotN), prettyParam);
        if (base == "fx_bus")     return stitch("Mx Effects Bus", effectFromRack(vg.getEffectsBusRack(),   slotN), prettyParam);
        if (base == "clips_bus")  return stitch("Mx Clips Bus",   effectFromRack(vg.getAudioClipsBusRack(),slotN), prettyParam);

        // Also accept the older mixer_* prefix form defensively.
        if (base == "mixer_layers")   return stitch("Mx Layers Bus",  effectFromRack(vg.getLayersBusRack(),    slotN), prettyParam);
        if (base == "mixer_bass")     return stitch("Mx Bass Bus",    effectFromRack(vg.getBassBusRack(),      slotN), prettyParam);
        if (base == "mixer_drums")    return stitch("Mx Drums Bus",   effectFromRack(vg.getDrumsBusRack(),     slotN), prettyParam);
        if (base == "mixer_master")   return stitch("Mx Master",      effectFromRack(vg.getMasterRack(),       slotN), prettyParam);
        if (base == "mixer_fx")       return stitch("Mx Effects Bus", effectFromRack(vg.getEffectsBusRack(),   slotN), prettyParam);
        if (base == "mixer_clipsbus") return stitch("Mx Clips Bus",   effectFromRack(vg.getAudioClipsBusRack(),slotN), prettyParam);

        // Legacy per-page rack accessors (pre-5F-4a). Pages have NO effects
        // rack today — these arrays in VibeGraph are stale leftovers kept as
        // state-restore safety nets for old preset files. Labelled "Mx" since
        // they originated as mixer-rack concepts before the InsertNode
        // migration.
        auto extractPageRack = [&](const juce::String& prefix,
                                   EffectRack* (VibeGraph::*getter)(int),
                                   const juce::String& pagePrefix,
                                   const juce::String& channelSingular) -> juce::String
        {
            if (! base.startsWith(prefix + "_")) return {};
            int pageIdx = base.substring(prefix.length() + 1).getIntValue();
            EffectRack* rack = (vg.*getter)(pageIdx);
            juce::String name = lookupPageTabName(pagePrefix, pageIdx);
            if (name.isEmpty()) name = channelSingular + " " + juce::String(pageIdx + 1);
            return stitch("Mx " + name, effectFromRack(rack, slotN), prettyParam);
        };

        juce::String tryIt;
        tryIt = extractPageRack("layer", &VibeGraph::getLayerPageRack, "lay", "Layer"); if (tryIt.isNotEmpty()) return tryIt;
        tryIt = extractPageRack("bass",  &VibeGraph::getBassPageRack,  "bas", "Bass");  if (tryIt.isNotEmpty()) return tryIt;

        // Legacy instr_{ID}: pre-5F-4a Batch 6 dynamic InstrChannel system.
        // Live audio now goes through InsertNodes; this branch only fires for
        // state-restored params from old preset files. Labelled "Mx" since
        // these were always mixer-side rack concepts.
        if (base.startsWith("instr_"))
        {
            const int channelId = base.substring(6).getIntValue();
            EffectRack* rack = vg.getInstrChannelRack(channelId);
            juce::String channelLabel = vg.getInstrChannelName(channelId);
            if (channelLabel.isEmpty()) channelLabel = "Channel " + juce::String(channelId);
            return stitch("Mx " + channelLabel, effectFromRack(rack, slotN), prettyParam);
        }

        // InsertNode-keyed forms used by the dynamic strip system
        // (mixer_{kind}_{index}_s{slot}_{param}).
        // Channel label honors the user-renamed strip name (mixer rename ->
        // ribbon tab rename -> LayersPage/BassPage::getTabName for Layer/Bass;
        // MixerPage::getDrumStripName / getAudioStripName / getAuxStripName for
        // the other kinds). Falls back to the hardcoded "{singular} {N+1}"
        // default if no user name is set.
        auto extractIndexed = [&](const juce::String& prefix, Kind kind,
                                  const juce::String& channelSingular) -> juce::String
        {
            if (! base.startsWith(prefix + "_")) return {};
            int insertIdx = base.substring(prefix.length() + 1).getIntValue();
            EffectRack* rack = vg.getInsertRack(kind, insertIdx);

            juce::String channelLabel;
            if      (kind == Kind::Layer) channelLabel = lookupPageTabName("lay", insertIdx);
            else if (kind == Kind::Bass)  channelLabel = lookupPageTabName("bas", insertIdx);
            else if (mMixerPage != nullptr)
            {
                if      (kind == Kind::Drum)  channelLabel = mMixerPage->getDrumStripName(insertIdx);
                else if (kind == Kind::Audio) channelLabel = mMixerPage->getAudioStripName(insertIdx);
                else if (kind == Kind::Aux)   channelLabel = mMixerPage->getAuxStripName(insertIdx);
            }
            if (channelLabel.isEmpty())
                channelLabel = channelSingular + " " + juce::String(insertIdx + 1);

            return stitch("Mx " + channelLabel, effectFromRack(rack, slotN), prettyParam);
        };
        tryIt = extractIndexed("mixer_layer", Kind::Layer, "Layer"); if (tryIt.isNotEmpty()) return tryIt;
        tryIt = extractIndexed("mixer_bass",  Kind::Bass,  "Bass");  if (tryIt.isNotEmpty()) return tryIt;
        tryIt = extractIndexed("mixer_drum",  Kind::Drum,  "Drum");  if (tryIt.isNotEmpty()) return tryIt;
        tryIt = extractIndexed("mixer_audio", Kind::Audio, "Audio Row"); if (tryIt.isNotEmpty()) return tryIt;
        tryIt = extractIndexed("mixer_aux",   Kind::Aux,   "Aux");   if (tryIt.isNotEmpty()) return tryIt;

        // 2026-04-21: Drum-slot engine params — tk_drm_{N}_s{S}_{engineTag}_{param}.
        //   After splitSlotAndParam: base = "tk_drm_{N}", slotN = S, param = "{engineTag}_{param}".
        if (base.startsWith("tk_drm_"))
        {
            const int drumPageIdx = base.substring(7).getIntValue();
            const int usr = param.indexOfChar('_');
            if (usr > 0)
            {
                const juce::String engTag = param.substring(0, usr);
                const juce::String engineLabel = engineLabelFromTag(engTag);
                if (engineLabel.isNotEmpty())
                {
                    juce::String pageName = lookupPageTabName("drm", drumPageIdx);
                    if (pageName.isEmpty()) pageName = "Drums";
                    return pageName + " Slot " + juce::String(slotN + 1)
                         + " - " + engineLabel
                         + " - " + prettifyParam(param.substring(usr + 1));
                }
            }
        }

        // Unknown slot-form prefix: prettify `{base} slot{N} {param}` so at least
        // something readable shows rather than a raw snake-case id.
        return stitch(prettifyParam(base), {}, prettyParam);
    }

    // Format a mixer-strip parameter suffix. EQ band params get expanded to
    // "{Pre }EQ Mid/Side B{n} {Param}"; everything else falls through to
    // prettify.  §P4.3 B8: a leading "preeq_" token flags pre-rack EQ so the
    // label reads "Pre EQ Mid B4 Freq" instead of the post-rack "EQ Mid ...".
    auto formatMixerSuffix = [&](const juce::String& suffix) -> juce::String
    {
        const bool isPre = suffix.startsWith("preeq_");
        const juce::String eqPart = isPre ? suffix.substring(6) : suffix;
        const juce::String eqLabel = isPre ? "Pre EQ " : "EQ ";
        const bool isMid  = eqPart.startsWith("mid_eq");
        const bool isSide = eqPart.startsWith("side_eq");
        if (isMid || isSide)
        {
            const int prefixLen = isMid ? 6 : 7;
            int j = prefixLen;
            while (j < eqPart.length()
                   && juce::CharacterFunctions::isDigit((char) eqPart[j])) ++j;
            if (j > prefixLen)
            {
                const int band = eqPart.substring(prefixLen, j).getIntValue();
                const juce::String paramName = prettifyParam(eqPart.substring(j));
                juce::String out = eqLabel + (isMid ? "Mid" : "Side")
                                  + " B" + juce::String(band + 1);
                if (paramName.isNotEmpty()) out += " " + paramName;
                return out;
            }
        }
        return prettifyParam(suffix);
    };

    // 2026-04-21: Layer / Bass engine instance params (no effect-slot form).
    //   Format: tk_{pagePrefix}_{N}_{engineTag}_{param}
    //   e.g.    tk_lay_0_harm_flt_cutoff   -> "Pg Layer 1 - Harmless - Flt Cutoff"
    //           tk_bas_2_bss_osc_wave      -> "Pg Bass 3 - BaySickSynth - Osc Wave"
    auto tryEngineOnPage = [&](const juce::String& pagePrefix,
                                const juce::String& defaultLabel) -> juce::String
    {
        const juce::String fullPrefix = "tk_" + pagePrefix + "_";
        if (! paramId.startsWith(fullPrefix)) return {};
        const juce::String afterPrefix = paramId.substring(fullPrefix.length());
        const int firstUsr = afterPrefix.indexOfChar('_');
        if (firstUsr < 1) return {};
        const int pageIdx = afterPrefix.substring(0, firstUsr).getIntValue();
        const juce::String afterIdx = afterPrefix.substring(firstUsr + 1);
        const int tagEnd = afterIdx.indexOfChar('_');
        if (tagEnd < 1) return {};
        const juce::String engTag = afterIdx.substring(0, tagEnd);
        const juce::String engineLabel = engineLabelFromTag(engTag);
        if (engineLabel.isEmpty()) return {};
        juce::String pageName = lookupPageTabName(pagePrefix, pageIdx);
        if (pageName.isEmpty()) pageName = defaultLabel + " " + juce::String(pageIdx + 1);
        const juce::String paramSuffix = afterIdx.substring(tagEnd + 1);
        return "Pg " + pageName + " - " + engineLabel + " - " + prettifyParam(paramSuffix);
    };

    // §P4.3 B7: legacy per-page EQ-tab params (tk_{pagePrefix}_{N}_mid_eq* /
    // _side_eq*) no longer registered — pre-rack EQ on Layer/Bass/Drum pages
    // now writes to the unified mixer_{kind}_<N>_preeq_* params and resolves
    // via tryMixerNonSlot below (labelled "Mx ... - Pre EQ Mid B{n} {param}").

    juce::String tryIt;
    tryIt = tryEngineOnPage("lay", "Layer"); if (tryIt.isNotEmpty()) return tryIt;
    tryIt = tryEngineOnPage("bas", "Bass");  if (tryIt.isNotEmpty()) return tryIt;

    // Mixer strip non-slot params: strip controls (_level / _pan / _mute /
    // _solo / _polarity / _width / _bypass / _arm / _sendTo / _sendN_*) and
    // post-rack EQ bands (_mid_eq{b}* / _side_eq{b}*) on every strip family.
    auto tryMixerNonSlot = [&]() -> juce::String
    {
        if (! paramId.startsWith("mixer_")) return {};

        // Bus + Master strips (fixed names).
        struct BusEntry { const char* prefix; const char* label; };
        static const BusEntry kBusEntries[] = {
            { "mixer_master_",   "Master"      },
            { "mixer_layers_",   "Layers Bus"  },
            { "mixer_bass_",     "Bass Bus"    },
            { "mixer_drums_",    "Drums Bus"   },
            { "mixer_fx_",       "Effects Bus" },
            { "mixer_clipsbus_", "Clips Bus"   },
        };
        for (const auto& e : kBusEntries)
        {
            const juce::String pfx = e.prefix;
            if (paramId.startsWith(pfx))
                return "Mx " + juce::String(e.label) + " - "
                     + formatMixerSuffix(paramId.substring(pfx.length()));
        }

        // Indexed insert strips: mixer_{kind}_{N}_{suffix}.
        struct InsertEntry { const char* prefix; Kind kind; const char* singular; };
        static const InsertEntry kInsertEntries[] = {
            { "mixer_layer_", Kind::Layer, "Layer"     },
            { "mixer_bass_",  Kind::Bass,  "Bass"      },
            { "mixer_drum_",  Kind::Drum,  "Drum"      },
            { "mixer_audio_", Kind::Audio, "Audio Row" },
            { "mixer_aux_",   Kind::Aux,   "Aux"       },
        };
        for (const auto& e : kInsertEntries)
        {
            const juce::String pfx = e.prefix;
            if (! paramId.startsWith(pfx)) continue;
            const juce::String tail = paramId.substring(pfx.length());
            int j = 0;
            while (j < tail.length()
                   && juce::CharacterFunctions::isDigit((char) tail[j])) ++j;
            if (j == 0 || j >= tail.length() || tail[j] != '_') continue;
            const int insertIdx = tail.substring(0, j).getIntValue();
            const juce::String suffix = tail.substring(j + 1);

            juce::String channelLabel;
            if      (e.kind == Kind::Layer) channelLabel = lookupPageTabName("lay", insertIdx);
            else if (e.kind == Kind::Bass)  channelLabel = lookupPageTabName("bas", insertIdx);
            else if (mMixerPage != nullptr)
            {
                if      (e.kind == Kind::Drum)  channelLabel = mMixerPage->getDrumStripName(insertIdx);
                else if (e.kind == Kind::Audio) channelLabel = mMixerPage->getAudioStripName(insertIdx);
                else if (e.kind == Kind::Aux)   channelLabel = mMixerPage->getAuxStripName(insertIdx);
            }
            if (channelLabel.isEmpty())
                channelLabel = juce::String(e.singular) + " " + juce::String(insertIdx + 1);

            return "Mx " + channelLabel + " - " + formatMixerSuffix(suffix);
        }
        return {};
    };

    tryIt = tryMixerNonSlot(); if (tryIt.isNotEmpty()) return tryIt;

    // Non-slot paramIds that didn't match any branch above (raw engine APVTS
    // params, etc.). Best-effort prettify of the whole id.
    return prettifyParam(paramId);
}

juce::String StandaloneEditor::displayNameFor(const AutomationLane& lane) const
{
    if (lane.userDisplayName.isNotEmpty()) return lane.userDisplayName;
    return resolveAutomationDisplayName(lane.paramId);
}

std::unique_ptr<juce::Component> StandaloneEditor::createMixerPage()
{
    auto page = std::make_unique<MixerPage>(mProcessor, *mPM);
    page->setUndoContext(makeUndoContext());
    mMixerPage = page.get();
    // FX Rack button on any strip → switch to Effects tab (ID=2) and pre-select
    // that strip's rack. `identifier` is the strip's APVTS prefix (e.g.
    // "mixer_layer_0") — unambiguous across renames.
    page->onEffectsTabRequested = [this](const juce::String& identifier)
    {
        mLastFXChannel = identifier;
        mRibbon->selectTab(2);
        onTabSelected(2);
    };
    // Mixer strip rename → ribbon tab rename (mixer → ribbon direction).
    // Layer/Bass mixer strips are keyed by pageIndex; translate to ribbonTabId.
    page->onChannelRenamed = [this](int pageIndex, const juce::String& newName)
    {
        for (auto* entry : mPages)
        {
            if (!entry) continue;
            if (auto* lp = dynamic_cast<LayersPage*>(entry->component.get()))
                if (lp->getPageIndex() == pageIndex) {
                    mRibbon->renameTab(entry->ribbonTabId, newName);
                    return;
                }
            if (auto* bp = dynamic_cast<BassPage*>(entry->component.get()))
                if (bp->getPageIndex() == pageIndex) {
                    mRibbon->renameTab(entry->ribbonTabId, newName);
                    return;
                }
            if (auto* dp = dynamic_cast<DrumPage*>(entry->component.get()))
                if (dp->getPageIndex() == pageIndex) {
                    mRibbon->renameTab(entry->ribbonTabId, newName);
                    return;
                }
        }
    };
    // Audio clip strip rename → rebuild Effects dropdown so names stay in sync
    page->onAudioStripRenamed = [this]()
    {
        if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
    };
    // Any strip's main-out cable reroute → rebuild Effects dropdown so the
    // Direct Routing group (and cross-bus moves) reflect in the dropdown.
    page->onSendToChanged = [this]()
    {
        if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
    };
    // R2 (2026-04-23): expose AudioDeviceManager input-channel names to the
    // Vox / Inst Arm-LED picker.  Returns the friendly names for the active
    // device's input channels (or empty when no device or zero inputs).
    page->getInputChannelNames = [this]() -> juce::StringArray
    {
        juce::StringArray out;
        if (auto* dev = mDeviceManager.getCurrentAudioDevice())
            out = dev->getInputChannelNames();
        return out;
    };
    return page;
}

std::unique_ptr<juce::Component> StandaloneEditor::createEffectsPage()
{
    auto page = std::make_unique<EffectsPage>(mTrackSel, mProcessor);
    page->setUndoContext(makeUndoContext());
    mEffectsPage = page.get();

    // ── Active channel list callback ──────────────────────────────────────────
    // Returns only channels that actually have an open tab / assigned sound.
    page->onGetActiveChannels = [this]() -> std::vector<std::pair<int, juce::String>>
    {
        std::vector<std::pair<int, juce::String>> result;

        // Bus channels — always present
        result.push_back({4, "Master"});
        result.push_back({1, "Layers Bus"});
        result.push_back({2, "Bass Bus"});
        result.push_back({3, "Drums Bus"});
        result.push_back({5, "FX Bus"});
        result.push_back({6, "Clips Bus"});

        // Active Layer pages (engine selected)
        for (auto* entry : mPages)
        {
            if (auto* lp = dynamic_cast<LayersPage*>(entry->component.get()))
            {
                if (lp->isEngineLocked())
                {
                    const auto* tab = mRibbon->getTabById(entry->ribbonTabId);
                    juce::String name = tab ? tab->name : ("Layer " + juce::String(lp->getPageIndex() + 1));
                    result.push_back({200 + lp->getPageIndex(), name});
                }
            }
        }

        // Active Bass pages (engine selected)
        for (auto* entry : mPages)
        {
            if (auto* bp = dynamic_cast<BassPage*>(entry->component.get()))
            {
                if (bp->isEngineLocked())
                {
                    const auto* tab = mRibbon->getTabById(entry->ribbonTabId);
                    juce::String name = tab ? tab->name : ("Bass " + juce::String(bp->getPageIndex() + 1));
                    result.push_back({300 + bp->getPageIndex(), name});
                }
            }
        }

        auto& vg  = mProcessor.mVibeGraph;
        auto  ids = vg.getInstrChannelIds();

        // Active Drum slots — enumerated via MixerPage (matches the mixer's
        // visible drum strips). Dropdown ID 100+slot maps to mixer_drum_N via
        // EffectsPage::getMixerApvtsPrefixForChannel. Legacy InstrChannelNode
        // drums (5F-3 and earlier) are no longer registered — all drum audio
        // now routes through VibeGraph's per-slot InsertNode (InsertKind::Drum).
        if (mMixerPage)
        {
            for (int slot : mMixerPage->getDrumStripIndices())
                result.push_back({100 + slot, mMixerPage->getDrumStripName(slot)});
        }

        // Fallback: any legacy InstrChannelNode-style drums that happen to still
        // be registered (belt-and-suspenders for stray state-restore paths).
        for (int id : ids)
        {
            if (id >= 100 && id < 200)
            {
                const juce::String& chName = vg.getInstrChannelName(id);
                if (chName.startsWith("Drum"))
                {
                    // De-dupe: skip if MixerPage already added this ID
                    bool alreadyAdded = false;
                    for (auto& p : result) if (p.first == id) { alreadyAdded = true; break; }
                    if (! alreadyAdded) result.push_back({id, chName});
                }
            }
        }

        // Per-clip audio row channels (IDs 400+row) — name live from mixer strip
        for (int id : ids)
        {
            if (id >= 400 && id < 500)
            {
                const int row = id - 400;
                juce::String name = mMixerPage ? mMixerPage->getAudioStripName(row)
                                               : vg.getInstrChannelName(id);
                result.push_back({id, name});
            }
        }

        // Aux strips — dropdown-internal ID range 600+idx to avoid collision
        // with drum (100-series) and audio (400-series).
        if (mMixerPage)
        {
            for (int auxIdx : mMixerPage->getAuxStripIndices())
                result.push_back({600 + auxIdx, mMixerPage->getAuxStripName(auxIdx)});
        }

        return result;
    };

    // The Effects page constructor builds its dropdown before the callback is
    // wired (so it falls back to a plain Bus list with no apvts-prefix mapping).
    // Re-rebuild now that the callback is in place — gives us colored section
    // headers + populates mIdToApvtsPrefix so FX Rack buttons on bus strips
    // route correctly even before any insert is added.
    page->rebuildChannelDropdown();

    return page;
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab events
// ─────────────────────────────────────────────────────────────────────────────
void StandaloneEditor::onAddTabRequest(RibbonTabBar::TabType type)
{
    // D1.4: Layers, Bass, and Drums all support adding new instances.
    if (type != RibbonTabBar::TabType::Layers
        && type != RibbonTabBar::TabType::Bass
        && type != RibbonTabBar::TabType::Drums)
        return;

    std::unique_ptr<juce::Component> page;
    juce::String name;

    switch (type)
    {
    case RibbonTabBar::TabType::Layers:
        page = createLayersPage();
        if (!page) return;  // all 8 Layers slots occupied
        name = "Layers";
        break;
    case RibbonTabBar::TabType::Bass:
        page = createBassPage();
        if (!page) return;  // all 4 Bass slots occupied
        name = "Bass";
        break;
    case RibbonTabBar::TabType::Drums:
        page = createDrumPage();
        if (!page) return;  // all 16 Drums slots occupied
        name = "Drums";
        break;
    default:
        return;
    }

    int newId = mRibbon->addTab(type, name);

    // Wire mixer strip creation to engine selection (lazy — not on tab open)
    if (type == RibbonTabBar::TabType::Layers)
    {
        if (auto* p = dynamic_cast<LayersPage*>(page.get()))
        {
            const int pageIdx = p->getPageIndex();
            p->onEngineSelected = [this, newId, pageIdx] {
                const auto* tab = mRibbon->getTabById(newId);
                if (mMixerPage) mMixerPage->addLayerChannel(pageIdx, tab ? tab->name : "Layers");
                if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
            };
            p->onDeleteRequested = [this, newId] {
                if (! mRibbon) return;
                if (mRibbon->isLastOfType (RibbonTabBar::TabType::Layers))
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                        "Cannot Delete",
                        "This is the only Layer tab. Add another first.");
                    return;
                }
                mRibbon->closeTab (newId);
            };
            p->onDuplicateRequested = [this] (const juce::String& clipboardXml) {
                spawnDuplicateLayerTab (clipboardXml);
            };
            p->onLockChanged = [this, newId, p] {
                if (mRibbon) mRibbon->setTabLocked (newId, p->isLocked());
            };
            p->onRenameRequested = [this, newId] {
                if (mRibbon) mRibbon->startRename (newId);
            };
            registerLayerPianoRoll (p);
        }
    }
    else if (type == RibbonTabBar::TabType::Bass)
    {
        if (auto* p = dynamic_cast<BassPage*>(page.get()))
        {
            const int pageIdx = p->getPageIndex();
            p->onEngineSelected = [this, newId, pageIdx] {
                const auto* tab = mRibbon->getTabById(newId);
                if (mMixerPage) mMixerPage->addBassChannel(pageIdx, tab ? tab->name : "Bass");
                if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
            };
            p->onDeleteRequested = [this, newId] {
                if (! mRibbon) return;
                if (mRibbon->isLastOfType (RibbonTabBar::TabType::Bass))
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                        "Cannot Delete",
                        "This is the only Bass tab. Add another first.");
                    return;
                }
                mRibbon->closeTab (newId);
            };
            p->onDuplicateRequested = [this] (const juce::String& clipboardXml) {
                spawnDuplicateBassTab (clipboardXml);
            };
            p->onLockChanged = [this, newId, p] {
                if (mRibbon) mRibbon->setTabLocked (newId, p->isLocked());
            };
            p->onRenameRequested = [this, newId] {
                if (mRibbon) mRibbon->startRename (newId);
            };
            registerBassPianoRoll (p);
        }
    }
    else if (type == RibbonTabBar::TabType::Drums)
    {
        if (auto* p = dynamic_cast<DrumPage*>(page.get()))
        {
            const int pageIdx = p->getPageIndex();
            p->onEngineSelected = [this, newId, pageIdx] {
                const auto* tab = mRibbon->getTabById(newId);
                if (mMixerPage)   mMixerPage->addDrumChannel(pageIdx, tab ? tab->name : "Drums");
                if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
                refreshAllKitViews();
            };
            p->onSoundNameChanged = [this, newId, pageIdx, p] (const juce::String& nm) {
                if (nm.isEmpty()) return;
                if (mRibbon)    mRibbon->renameTab (newId, nm);
                if (mMixerPage) mMixerPage->renameChannel (pageIdx, nm);
                p->setTabName (nm);
                refreshAllKitViews();
                if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Drum, pageIdx }, nm);
            };
            p->onDeleteRequested = [this, newId] {
                if (! mRibbon) return;
                if (mRibbon->isLastOfType (RibbonTabBar::TabType::Drums))
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                        "Cannot Delete",
                        "This is the only Drum tab. Add another first.");
                    return;
                }
                mRibbon->closeTab (newId);
            };
            p->onDuplicateRequested = [this] (const juce::String& clipboardXml) {
                spawnDuplicateDrumTab (clipboardXml);
            };
            p->onLockChanged = [this, newId, p] {
                if (mRibbon) mRibbon->setTabLocked (newId, p->isLocked());
                refreshAllKitViews();
            };
            p->onRenameRequested = [this, newId] {
                if (mRibbon) mRibbon->startRename (newId);
            };
            wireDrumPageKitView (p);
            registerDrumPianoRoll (p);
        }
    }

    auto* entry = new PageEntry();
    entry->ribbonTabId = newId;
    entry->type        = type;
    entry->component   = std::move(page);
    addChildComponent(*entry->component);
    mPages.add(entry);

    mRibbon->selectTab(newId);
    onTabSelected(newId);
    if (type == RibbonTabBar::TabType::Drums) refreshAllKitViews();
}

void StandaloneEditor::onTabSelected(int tabId)
{
    showPageForTab(tabId);

    // R5d-midi (2026-04-24): track the last-accessed piano roll so MIDI
    // record captures land in whichever roll the user was last looking at.
    for (auto& e : mPages)
    {
        if (e->ribbonTabId != tabId) continue;
        if (auto* lp = dynamic_cast<LayersPage*> (e->component.get()))
        {
            mLastRollKind  = LastRollKind::Layer;
            mLastRollIndex = lp->getPageIndex();
        }
        else if (auto* bp = dynamic_cast<BassPage*> (e->component.get()))
        {
            mLastRollKind  = LastRollKind::Bass;
            mLastRollIndex = bp->getPageIndex();
        }
        else if (auto* dp = dynamic_cast<DrumPage*> (e->component.get()))
        {
            mLastRollKind  = LastRollKind::Drums;
            mLastRollIndex = dp->getPageIndex();
        }
        break;
    }

    // D2: when the active ribbon tab changes (especially among Drums),
    // every kit view's active-row border needs to follow.
    refreshAllKitViews();

    // Effects tab (ID=2): pre-select the channel whose FX Rack button was clicked.
    // mLastFXChannel is the strip's APVTS prefix (e.g. "mixer_layer_0"); falls
    // back to the legacy name-based lookup for bus-strip aliases.
    // 2026-04-26: only re-select when an FX button explicitly set mLastFXChannel.
    // After consuming, reset to empty so plain ribbon-tab clicks leave the page's
    // current channel alone — matches per-channel sub-tab persistence (user lands
    // back on the channel + sub-tab they last had open).
    if (tabId == 2 && mEffectsPage && mLastFXChannel.isNotEmpty())
    {
        if (mLastFXChannel.startsWith("mixer_"))
            mEffectsPage->selectChannelByApvtsPrefix(mLastFXChannel);
        else
            mEffectsPage->selectChannelByName(mLastFXChannel);
        mLastFXChannel.clear();
    }

    // 2026-04-22: Removed auto-switch of transport mode based on visible page.
    // Original behavior: Builder → Song; piano-roll sub-tab → Pattern.  Now the
    // user picks Pattern or Song explicitly via the transport button (default
    // Pattern at startup, set in GlobalTransportBar's button construction).
}

void StandaloneEditor::onTabClosed(int tabId)
{
    // Find and remove the page entry
    for (int i = 0; i < mPages.size(); ++i)
    {
        if (mPages[i]->ribbonTabId == tabId)
        {
            // Free layer index slot for any LayersPage being closed
            if (auto* lp = dynamic_cast<LayersPage*>(mPages[i]->component.get()))
            {
                int idx = lp->getPageIndex();
                if (idx >= 0 && idx < kMaxLayerPages)
                    mUsedLayerIndices[idx] = false;
                // 2026-04-26 (step 2): drop the corresponding registry entry
                // BEFORE the page is destroyed - the connection's lambdas
                // capture the page raw ptr.
                if (mPianoRollPage && idx >= 0)
                    mPianoRollPage->unregisterEngine ({ EngineKind::Layer, idx });
            }

            // Free bass index slot for any BassPage being closed
            if (auto* bp = dynamic_cast<BassPage*>(mPages[i]->component.get()))
            {
                int idx = bp->getPageIndex();
                if (idx >= 0 && idx < kMaxBassPages)
                    mUsedBassIndices[idx] = false;
                if (mPianoRollPage && idx >= 0)
                    mPianoRollPage->unregisterEngine ({ EngineKind::Bass, idx });
            }

            // D1.4: Free drum index slot for any DrumPage being closed
            if (auto* dp = dynamic_cast<DrumPage*>(mPages[i]->component.get()))
            {
                int idx = dp->getPageIndex();
                if (idx >= 0 && idx < kMaxDrumPages)
                    mUsedDrumIndices[idx] = false;
                if (mPianoRollPage && idx >= 0)
                    mPianoRollPage->unregisterEngine ({ EngineKind::Drum, idx });
            }

            // G-3 (2026-04-28): Clips tab close — unregister both the audio
            // engine (so the audio thread stops dispatching MIDI to it) and
            // the piano-roll connection (so PianoRollPage drops the now-
            // dangling closure).  The audio file stays in mAudioLibrary + the
            // mixer audio insert stays intact — closing a Clips tab only
            // removes the page, never the underlying clip data (no-file-
            // delete contract).
            if (auto* cp = dynamic_cast<ClipsPage*>(mPages[i]->component.get()))
            {
                int idx = cp->getPageIndex();
                if (idx >= 0)
                {
                    mProcessor.unregisterClipEngine (idx);
                    if (mPianoRollPage)
                        mPianoRollPage->unregisterEngine ({ EngineKind::Clip, idx });
                }
            }

            // Clear legacy raw pointers if we're removing those pages
            if (mPages[i]->component.get() == mLegacyLayersPage)
                mLegacyLayersPage = nullptr;
            if (mPages[i]->component.get() == mLegacyBassPage)
                mLegacyBassPage = nullptr;
            if (mPages[i]->component.get() == mLegacyDrumPage)
                mLegacyDrumPage = nullptr;
            if (mPages[i]->component.get() == mBuilderPage)
                mBuilderPage = nullptr;

            if (mVisiblePage == mPages[i]->component.get())
                mVisiblePage = nullptr;

            mPages.remove(i);
            resized();
            refreshAllKitViews();   // D2: drum row freed → kit view shrinks
            return;
        }
    }
}

void StandaloneEditor::onSubPageSelected(RibbonTabBar::TabType type, int subPageIndex)
{
    // Dispatch the sub-page index to the matching destination page so its
    // internal sub-tab actually changes. The ribbon dropdown already navigated
    // to the page via onTabSelected before invoking us.

    switch (type)
    {
    case RibbonTabBar::TabType::Effects:
        // Ribbon Effects ▾ dropdown: 0 = Rack, 1 = EQ.  Map by TabKind (not raw
        // visible index) so non-player channels' 3-tab layout doesn't redirect
        // "Rack" → PreEQ.  EQ resolves to PostEQ on every channel (the only EQ
        // common to player + bus/aux/audio layouts).
        // 2026-04-26: channel pre-selection already happened in onTabSelected
        // (only when mLastFXChannel is set, e.g. via FX-button click).  Leave
        // current channel alone here — user explicitly picked a sub-tab, not a
        // channel.
        if (mEffectsPage)
        {
            mEffectsPage->switchTab(subPageIndex == 1
                                    ? EffectsPage::TabKind::PostEQ
                                    : EffectsPage::TabKind::Rack);
        }
        break;

    case RibbonTabBar::TabType::Builder:
        // 0 = Patterns, 1 = Audio Clips, 2 = Automation — drives browser pane.
        if (mBuilderPage) mBuilderPage->setBrowserTab(subPageIndex);
        break;

    case RibbonTabBar::TabType::Drums:
    case RibbonTabBar::TabType::Layers:
    case RibbonTabBar::TabType::Bass:
    {
        // 0 = Player, 1 = Piano Roll, 2 = EQ.  Dispatch to the ACTIVE page
        // for this type (not the last-created legacy ptr, which gets stale
        // once multiple tabs of the same type exist).
        const int activeId = mRibbon->getActiveTabForType(type);
        for (auto* entry : mPages)
        {
            if (! entry || entry->ribbonTabId != activeId) continue;
            if (auto* lp = dynamic_cast<LayersPage*>(entry->component.get()))
                lp->switchTab(subPageIndex);
            else if (auto* bp = dynamic_cast<BassPage*>(entry->component.get()))
                bp->switchTab(subPageIndex);
            else if (auto* dp = dynamic_cast<DrumPage*>(entry->component.get()))
                dp->switchTab(subPageIndex);
            break;
        }
        break;
    }

    default:
        break;
    }
}

void StandaloneEditor::showPageForTab(int tabId)
{
    // Hide all
    for (auto* entry : mPages)
        if (entry->component) entry->component->setVisible(false);
    // G-2: hide empty-state when a real page is being shown.
    if (mClipsEmptyState) mClipsEmptyState->setVisible(false);

    mVisiblePage = nullptr;

    // Show selected + update page menu bar title
    for (auto* entry : mPages)
    {
        if (entry->ribbonTabId == tabId && entry->component)
        {
            entry->component->setVisible(true);
            mVisiblePage = entry->component.get();

            // Update Tier 2 title from tab name
            if (mPageMenuBar)
            {
                const auto* tab = mRibbon->getTabById(tabId);
                mPageMenuBar->setPageTitle(tab ? tab->name : juce::String());
            }
            break;
        }
    }

    // Configure PageMenuBar tab slots for Layers/Bass/Drums pages
    if (mPageMenuBar)
    {
        // 2026-04-19: PageMenuBar's hamburger ≡ becomes the universal page-
        // actions menu. ParametricEQDisplay installs its options menu on the
        // hamburger when its tab is active and clears it on switching away.
        // Bank indicator (A/B compare current-bank pill) also injects into
        // the bar's extra-right slot on EQ-tab activation, removed on switch.
        auto syncEQHamburger = [this] (ParametricEQDisplay* eq, bool onEqTab)
        {
            if (! mPageMenuBar) return;
            if (eq && onEqTab)
            {
                eq->installPageMenu (*mPageMenuBar);
                eq->refreshBankIndicator();
                // Dedicated slot adjacent to MID/SIDE (not the right-extras list).
                // setBankIndicator is idempotent so repeat tab clicks don't stack.
                mPageMenuBar->setBankIndicator (eq->getBankIndicator());
            }
            else if (eq)
            {
                eq->uninstallPageMenu (*mPageMenuBar);
                mPageMenuBar->setBankIndicator (nullptr);
            }
            else
            {
                mPageMenuBar->setMenuBuilder (nullptr);
                mPageMenuBar->setBankIndicator (nullptr);
            }
        };

        // Always start by clearing - each branch below restores its own state.
        // 2026-04-19: also clear extra-right components so per-page extras
        // (Effects meters/bypass/trackBox/trackLabel, Drums Kit/Nav, Mixer
        // AddAux, EQ bank indicator etc.) don't leak across page switches.
        // Pre-existing leak that became visible with the new bank indicator.
        mPageMenuBar->setMenuBuilder (nullptr);
        mPageMenuBar->setBankIndicator (nullptr);
        mPageMenuBar->clearTabSlots();
        mPageMenuBar->clearExtraRightComponents();

        if (auto* ep = dynamic_cast<EffectsPage*>(mVisiblePage))
        {
            // §P4.3 (B6.2): tab layout is dynamic — Layer/Bass/Drum channels
            // get [Rack | Post EQ8 M/S] (pre-EQ on player page); Aux/Audio/Bus
            // get [Pre EQ8 M/S | Rack | Post EQ8 M/S].  Re-runs on channel
            // change via ep->onTabsNeedRefresh.  The callback sent into
            // setTabSlots interprets the visible-index via EffectsPage's
            // tabKindForVisibleIndex so it works with either layout.
            auto setupEffectsTabs = [this, ep, syncEQHamburger]()
            {
                // §P4.3 (B6.2 fix #1): bail if EffectsPage isn't the visible page.
                // onTabsNeedRefresh fires whenever the EffectsPage's channel
                // selection changes — even when the user has navigated away to
                // a player page.  Without this guard, the player page's tab
                // slots get stomped with EffectsPage's labels.
                if (mVisiblePage != ep) return;

                using TabKind = EffectsPage::TabKind;
                const juce::StringArray labels = ep->currentChannelHasPagePreEQ()
                    ? juce::StringArray{ "Rack", "Post EQ8 M/S" }
                    : juce::StringArray{ "Pre EQ8 M/S", "Rack", "Post EQ8 M/S" };

                auto isEqTabIndex = [ep](int idx) {
                    auto kind = ep->tabKindForVisibleIndex(idx);
                    return kind == TabKind::PreEQ || kind == TabKind::PostEQ;
                };
                auto displayForIndex = [ep](int idx) -> ParametricEQDisplay* {
                    return ep->tabKindForVisibleIndex(idx) == TabKind::PreEQ
                                ? ep->getPreEQDisplay()
                                : ep->getEQDisplay();
                };

                // 2026-04-26: trust EffectsPage's current visible index — onChannelChanged
                // restores the per-channel saved TabKind (and clamps PreEQ → Rack on
                // player channels) before firing onTabsNeedRefresh, so getActiveTab()
                // already points at the right slot for this layout.
                const int activeIdx = ep->getActiveTab();

                mPageMenuBar->setTabSlots(labels,
                    [this, ep, syncEQHamburger, isEqTabIndex, displayForIndex](int i) {
                        ep->switchTab(i);
                        mPageMenuBar->updateTabActive(i);
                        const bool isEq = isEqTabIndex(i);
                        mPageMenuBar->setMidSideVisible(isEq);
                        syncEQHamburger(displayForIndex(i), isEq);
                    }, activeIdx);

                const bool activeIsEq = isEqTabIndex(activeIdx);
                mPageMenuBar->setMidSideVisible(activeIsEq);
                syncEQHamburger(displayForIndex(activeIdx), activeIsEq);
            };

            // §P4.3 (B6.2 fix #2): setMidSideSlots BEFORE setupEffectsTabs so
            // setupEffectsTabs gets the final word on visibility.  Original
            // refactor had this reversed → setMidSideSlots' side-effects
            // overrode the visibility we'd just set.
            mPageMenuBar->setMidSideSlots(
                [this, ep] { ep->setEQMid(true);  mPageMenuBar->updateMidSideActive(true);  },
                [this, ep] { ep->setEQMid(false); mPageMenuBar->updateMidSideActive(false); },
                ep->isEQMidActive());

            setupEffectsTabs();
            ep->onTabsNeedRefresh = setupEffectsTabs;
            // Right side: label, channel box, bypass, meters
            mPageMenuBar->addExtraRightComponent(ep->getMetersBtn(),   72);
            mPageMenuBar->addExtraRightComponent(ep->getFxBypassBtn(), 82);
            mPageMenuBar->addExtraRightComponent(ep->getTrackBox(),   176);
            mPageMenuBar->addExtraRightComponent(ep->getTrackLabel(),  58);
        }
        else if (auto* lp = dynamic_cast<LayersPage*>(mVisiblePage))
        {
            // §P4.3 (B5): EQ tab renamed "EQ" → "Pre EQ8 M/S".  Page-side EQ
            // is the per-insert pre-rack EQ (post-rack EQ lives on the mixer
            // strip's Effects-page Post EQ8 M/S tab).
            // 2026-04-26 (step 2 commit 3): Piano Roll sub-tab (index 1) is
            // a nav shortcut - clicking it switches to PianoRollPage and
            // selects this engine in the dropdown.  Player (0) and EQ (2)
            // remain local sub-pages.
            mPageMenuBar->setTabSlots({"Player", "Piano Roll", "Pre EQ8 M/S"},
                [this, lp, syncEQHamburger](int i) {
                    if (i == 1)
                    {
                        if (mRibbon) mRibbon->selectTab (4);
                        onTabSelected (4);
                        if (mPianoRollPage)
                            mPianoRollPage->selectEngine ({ EngineKind::Layer, lp->getPageIndex() });
                        return;
                    }
                    lp->switchTab(i);
                    mPageMenuBar->updateTabActive(i);
                    mPageMenuBar->setMidSideVisible(i == 2);
                    syncEQHamburger (lp->getEQDisplay(), i == 2);
                }, lp->getActiveTab(), lp->getPageColor());
            syncEQHamburger (lp->getEQDisplay(), lp->getActiveTab() == 2);
            mPageMenuBar->setMidSideSlots(
                [this, lp] { lp->setEQMid(true);  mPageMenuBar->updateMidSideActive(true);  },
                [this, lp] { lp->setEQMid(false); mPageMenuBar->updateMidSideActive(false); },
                lp->isEQMidActive());
            mPageMenuBar->setMidSideVisible(lp->getActiveTab() == 2);
        }
        else if (auto* bp = dynamic_cast<BassPage*>(mVisiblePage))
        {
            // §P4.3 (B5): same rename as Layers above.
            // 2026-04-26 (step 2 commit 3): Piano Roll sub-tab redirects to
            // PianoRollPage with this Bass engine selected.
            mPageMenuBar->setTabSlots({"Player", "Piano Roll", "Pre EQ8 M/S"},
                [this, bp, syncEQHamburger](int i) {
                    if (i == 1)
                    {
                        if (mRibbon) mRibbon->selectTab (4);
                        onTabSelected (4);
                        if (mPianoRollPage)
                            mPianoRollPage->selectEngine ({ EngineKind::Bass, bp->getPageIndex() });
                        return;
                    }
                    bp->switchTab(i);
                    mPageMenuBar->updateTabActive(i);
                    mPageMenuBar->setMidSideVisible(i == 2);
                    syncEQHamburger (bp->getEQDisplay(), i == 2);
                }, bp->getActiveTab(), bp->getPageColor());
            syncEQHamburger (bp->getEQDisplay(), bp->getActiveTab() == 2);
            mPageMenuBar->setMidSideSlots(
                [this, bp] { bp->setEQMid(true);  mPageMenuBar->updateMidSideActive(true);  },
                [this, bp] { bp->setEQMid(false); mPageMenuBar->updateMidSideActive(false); },
                bp->isEQMidActive());
            mPageMenuBar->setMidSideVisible(bp->getActiveTab() == 2);
        }
        else if (auto* cp = dynamic_cast<ClipsPage*>(mVisiblePage))
        {
            // 2026-04-28 (G-2): Clips page sub-tabs mirror Layer/Bass shape
            // (Player / Piano Roll / Pre EQ8 M/S).  Piano Roll redirects to
            // PianoRollPage with this Clip's engine selected; Pre EQ8 M/S is
            // a stub placeholder for now.
            mPageMenuBar->setTabSlots({"Player", "Piano Roll", "Pre EQ8 M/S"},
                [this, cp](int i) {
                    if (i == 1)
                    {
                        if (mRibbon) mRibbon->selectTab (4);
                        onTabSelected (4);
                        if (mPianoRollPage)
                            mPianoRollPage->selectEngine ({ EngineKind::Clip, cp->getPageIndex() });
                        return;
                    }
                    cp->switchTab (i);
                    mPageMenuBar->updateTabActive (i);
                    mPageMenuBar->setMidSideVisible (false);   // EQ stub doesn't need M/S yet
                }, cp->getActiveTab(), cp->getPageColor());
            mPageMenuBar->setMidSideVisible (false);
        }
        else if (auto* dp = dynamic_cast<DrumPage*>(mVisiblePage))
        {
            // D2 (2026-04-25): Drum Kit added as the first sub-tab.
            // 2026-04-26 (step 2 commit 3): Drum Kit (index 0) and Piano Roll
            // (index 2) are nav shortcuts - clicking them switches to
            // PianoRollPage with that view selected.  Player (1) and EQ (3)
            // remain local sub-pages.
            mPageMenuBar->setTabSlots({"Drum Kit", "Player", "Piano Roll", "Pre EQ8 M/S"},
                [this, dp, syncEQHamburger](int i) {
                    if (i == 0)
                    {
                        if (mRibbon) mRibbon->selectTab (4);
                        onTabSelected (4);
                        if (mPianoRollPage)
                            mPianoRollPage->selectEngine ({ EngineKind::DrumKit, 0 });
                        return;
                    }
                    if (i == 2)
                    {
                        if (mRibbon) mRibbon->selectTab (4);
                        onTabSelected (4);
                        if (mPianoRollPage)
                            mPianoRollPage->selectEngine ({ EngineKind::Drum, dp->getPageIndex() });
                        return;
                    }
                    dp->switchTab(i);
                    mPageMenuBar->updateTabActive(i);
                    mPageMenuBar->setMidSideVisible(i == 3);
                    syncEQHamburger (dp->getEQDisplay(), i == 3);
                }, dp->getActiveTab(), dp->getPageColor());
            syncEQHamburger (dp->getEQDisplay(), dp->getActiveTab() == 3);
            mPageMenuBar->setMidSideSlots(
                [this, dp] { dp->setEQMid(true);  mPageMenuBar->updateMidSideActive(true);  },
                [this, dp] { dp->setEQMid(false); mPageMenuBar->updateMidSideActive(false); },
                dp->isEQMidActive());
            mPageMenuBar->setMidSideVisible(dp->getActiveTab() == 3);
        }
        else if (auto* mxp = dynamic_cast<MixerPage*>(mVisiblePage))
        {
            // R1 (2026-04-23): three add-strip buttons in the Mixer page menu bar:
            //   [Add Aux Strip] [Add Vox Strip] [Add Inst Strip]
            // Added right-to-left so visual order matches (extra-right slot stacks
            // from the right edge inward).
            mPageMenuBar->addExtraRightComponent(mxp->getAddInstBtn(), 120);
            mPageMenuBar->addExtraRightComponent(mxp->getAddVoxBtn(),  120);
            mPageMenuBar->addExtraRightComponent(mxp->getAddAuxBtn(),  120);
        }
        else if (mPianoRollPage != nullptr && mVisiblePage == mPianoRollPage)
        {
            // 2026-04-26 (step 2): Piano Roll page's engine-picker dropdown -
            // single pill right after the hamburger.  Click pops up the menu
            // (Drum Kit at top, every Layer/Bass/Drum engine after).  Active
            // engine selection drives the pill label.
            const auto active = mPianoRollPage->getActiveEngineId();
            juce::String pillLabel = "Drum Kit";
            if (active.kind != EngineKind::DrumKit)
            {
                if (auto* tab = mRibbon ? mRibbon->getTabById (mRibbon->getActiveTabForType (
                        active.kind == EngineKind::Layer ? RibbonTabBar::TabType::Layers
                      : active.kind == EngineKind::Bass  ? RibbonTabBar::TabType::Bass
                      :                                    RibbonTabBar::TabType::Drums)) : nullptr)
                {
                    juce::ignoreUnused (tab);
                }
                // Resolve label by index via the dropdown enumerator.
                if (mPianoRollPage->dropdownEnumerator)
                {
                    for (auto& e : mPianoRollPage->dropdownEnumerator())
                        if (e.id == active) { pillLabel = e.label; break; }
                }
            }
            pillLabel += "  ";
            pillLabel += juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbe"));

            mPageMenuBar->setTabSlots (
                { pillLabel },
                [this] (int)
                {
                    if (! mPianoRollPage) return;
                    juce::PopupMenu m = mPianoRollPage->buildEngineDropdown();
                    m.showMenuAsync (juce::PopupMenu::Options(),
                        [this] (int r)
                        {
                            if (r <= 0 || ! mPianoRollPage) return;
                            if (r == 1)
                            {
                                mPianoRollPage->selectEngine ({ EngineKind::DrumKit, 0 });
                                return;
                            }
                            // Items >= 100 map to dropdownEnumerator entries
                            const int idx = r - 100;
                            if (mPianoRollPage->dropdownEnumerator)
                            {
                                const auto entries = mPianoRollPage->dropdownEnumerator();
                                if (idx >= 0 && idx < (int) entries.size())
                                    mPianoRollPage->selectEngine (entries[idx].id);
                            }
                        });
                },
                0,
                juce::Colours::white);
        }
    }

    resized();
}

// ─────────────────────────────────────────────────────────────────────────────
// Pattern selector helpers
// ─────────────────────────────────────────────────────────────────────────────
void StandaloneEditor::refreshPatternBox()
{
    if (!mPM || mPM->getNumPatterns() == 0) return;
    juce::String label = mPM->currentPattern().name
                       + "  "
                       + juce::String(juce::CharPointer_UTF8("\xe2\x96\xbe"));  // ▾
    if (mPatternBtn) mPatternBtn->setButtonText(label);
}

// ─────────────────────────────────────────────────────────────────────────────
// Transport
// ─────────────────────────────────────────────────────────────────────────────
void StandaloneEditor::startPlayback(double bpm)
{
    // 2026-04-26 (D-5): precount fires only when both record-arm AND the
    // precount toggle are on.  Always exactly 1 bar (4 beats) lead-in;
    // recording engages when bar 1 arrives.  Was variable 1/2/4 bars,
    // selectable in the metronome panel — simplified per user feedback.
    if (mPrecountEnabled && mRecordArmed)
    {
        const double totalBeats = 4.0;   // 1 bar
        const int    delayMs    = juce::roundToInt(totalBeats * (60000.0 / juce::jmax(1.0, bpm)));

        mCountInPendingBpm = bpm;
        mProcessor.mMetro.countInBpm.store(bpm, std::memory_order_relaxed);
        mProcessor.mMetro.countInActive.store(true, std::memory_order_relaxed);
        mTransport->setPlayState(true, false);
        mCountInTimer.startTimer(delayMs);
    }
    else
    {
        mPlayHead.start(bpm);
        mTransport->setPlayState(true, false);
    }
}

// Returns the active piano-roll container if the visible page is the
// unified PianoRollPage AND a non-Drum-Kit engine is currently selected.
// Used for time-selection-aware loop and stop-seek.
// 2026-04-26 (step 2 commit 3): rewritten - engine pages no longer host
// piano rolls; PianoRollPage owns them all and exposes the active one.
PianoRollContainer* StandaloneEditor::getActivePianoRollForLoop() const
{
    if (mPianoRollPage == nullptr || mVisiblePage != mPianoRollPage) return nullptr;
    return mPianoRollPage->getActivePianoRollForLoop();
}

// ── D2 Drum Kit support ──────────────────────────────────────────────────────
std::vector<KitDrumInfo> StandaloneEditor::getKitDrumList() const
{
    std::vector<KitDrumInfo> result;
    if (! mRibbon) return result;

    const int activeId = mRibbon->getActiveTabForType (RibbonTabBar::TabType::Drums);

    for (auto* entry : mPages)
    {
        if (! entry || entry->type != RibbonTabBar::TabType::Drums) continue;
        auto* dp = dynamic_cast<DrumPage*> (entry->component.get());
        if (! dp) continue;

        KitDrumInfo info;
        info.ribbonTabId = entry->ribbonTabId;
        info.pageIndex   = dp->getPageIndex();
        if (auto* tab = mRibbon->getTabById (entry->ribbonTabId))
            info.displayName = tab->name;
        info.hasEngine = ! dp->getEngineType().isEmpty();
        info.locked    = dp->isLocked();
        info.isActive  = (entry->ribbonTabId == activeId);
        info.color     = dp->getPageColor();
        result.push_back (info);
    }
    return result;
}

void StandaloneEditor::refreshAllKitViews()
{
    for (auto* entry : mPages)
    {
        if (! entry) continue;
        if (entry->type == RibbonTabBar::TabType::Drums)
        {
            if (auto* dp = dynamic_cast<DrumPage*> (entry->component.get()))
                dp->refreshKitView();
        }
        // 2026-04-26 (1b): unified Piano Roll page also hosts a kit.
        else if (entry->type == RibbonTabBar::TabType::PianoRoll)
        {
            if (auto* prp = dynamic_cast<PianoRollPage*> (entry->component.get()))
                if (auto* kit = prp->getDrumKitContainer())
                    kit->refreshKitView();
        }
    }
}

void StandaloneEditor::moveDrumTab (int srcRow, int dstRow)
{
    std::vector<int> drumIdxs;
    for (int i = 0; i < mPages.size(); ++i)
        if (mPages[i] != nullptr
            && mPages[i]->type == RibbonTabBar::TabType::Drums)
            drumIdxs.push_back (i);

    const int n = (int) drumIdxs.size();
    if (srcRow < 0 || srcRow >= n) return;
    if (dstRow < 0 || dstRow >= n) return;
    if (srcRow == dstRow) return;

    mPages.move (drumIdxs[(size_t) srcRow], drumIdxs[(size_t) dstRow]);
    if (mRibbon) mRibbon->moveTabOfType (RibbonTabBar::TabType::Drums, srcRow, dstRow);
    refreshAllKitViews();
}

void StandaloneEditor::wireDrumPageKitView (DrumPage* dp)
{
    if (! dp) return;

    dp->setKitListProvider ([this]() { return getKitDrumList(); });

    // D2 Batch 4: per-row audition (press-and-hold) routed to the drum's
    // engine via auditionNoteOn(60) / auditionNoteOff(60).  60 = C5 = the
    // kit-grid placement note.
    auto auditionDispatch = [this] (int row, bool on)
    {
        auto list = getKitDrumList();
        if (row < 0 || row >= (int) list.size()) return;
        const int targetTabId = list[(size_t) row].ribbonTabId;
        for (auto* entry : mPages)
        {
            if (! entry || entry->ribbonTabId != targetTabId) continue;
            if (auto* targetDp = dynamic_cast<DrumPage*> (entry->component.get()))
            {
                auto* eng = targetDp->getEngineProcessor();
                if (auto* s = dynamic_cast<BaySickSynthProcessor*> (eng))
                {
                    if (on) s->auditionNoteOn (60); else s->auditionNoteOff (60);
                }
                else if (auto* v = dynamic_cast<VibePlayerProcessor*> (eng))
                {
                    if (on) v->auditionNoteOn (60); else v->auditionNoteOff (60);
                }
            }
            return;
        }
    };
    dp->setKitAuditionHandlers (
        [auditionDispatch] (int row) { auditionDispatch (row, true);  },
        [auditionDispatch] (int row) { auditionDispatch (row, false); });

    // D2 Batch 4: drag-reorder.  Kit view fires srcRow → dstRow on drop.
    dp->setKitReorderHandler ([this] (int srcRow, int dstRow)
    {
        moveDrumTab (srcRow, dstRow);
    });

    dp->setKitRowClickHandler ([this] (int row, juce::Component* anchor)
    {
        auto list = getKitDrumList();

        if (row < (int) list.size())
        {
            // Existing drum at this row: activate its ribbon tab (matching
            // the ribbon-click flow) + force kit-view sub-tab so the user
            // stays in the kit context, then open the picker (unlocked) or
            // the per-drum context menu (locked).
            const int targetId = list[row].ribbonTabId;
            if (mRibbon) mRibbon->selectTab (targetId);
            onTabSelected (targetId);   // updates mVisiblePage + refreshAllKitViews

            if (auto* targetDp = dynamic_cast<DrumPage*> (mVisiblePage))
            {
                targetDp->switchTab (0);
                // Mirror Player tab's picker click logic: engine picked →
                // context menu; engine empty → sound picker.  isEngineLocked
                // returns true once an engine type has been selected (it's
                // independent of the user's Lock Drum toggle).
                if (targetDp->isEngineLocked())
                    targetDp->showContextMenu (anchor);
                else
                    targetDp->showSoundPicker (anchor);
            }
        }
        else
        {
            // Empty row: create a new drum (onAddTabRequest auto-activates
            // and refreshes kit views via onTabSelected), force kit view,
            // then open the sound picker on the just-created drum.
            onAddTabRequest (RibbonTabBar::TabType::Drums);

            if (auto* newDp = dynamic_cast<DrumPage*> (mVisiblePage))
            {
                newDp->switchTab (0);
                newDp->showSoundPicker (anchor);
            }
        }
    });

    // Batch 5: Kit menu — opens Save Kit As / Load Kit popup anchored to the
    // Kit button in this DrumKitContainer's toolbar.
    dp->setKitMenuHandler ([this] (juce::Component* anchor) { showKitMenu (anchor); });

    // 2026-04-26: Global Lock/Unlock — confirmable cross-slot toggle.
    dp->setGlobalLockHandler ([this] { showGlobalLockPrompt(); });
}

// 2026-04-26 (1b): wires the unified Piano Roll page's DrumKitContainer with
// the same callbacks `wireDrumPageKitView` installs on each DrumPage's
// internal kit container.  Both kit views share `Pattern::drumRolls[]` data
// so edits propagate either way; UI state (selection, scroll) is per-instance
// until step 2 collapses the duplication.
void StandaloneEditor::wirePianoRollPageKitView (PianoRollPage* prp)
{
    if (! prp) return;
    auto* kit = prp->getDrumKitContainer();
    if (! kit) return;

    kit->setPatternManager (mPM.get());
    kit->setApvts (&mProcessor.apvts);
    kit->onSeek = [this] (double b) { mPlayHead.seekTo (b); };

    // Adapter: `getKitDrumList()` returns `std::vector<KitDrumInfo>`; the
    // container wants `std::vector<DrumKitRowInfo>`.  Mirror the conversion
    // DrumPage::setKitListProvider does.
    kit->setKitRowProvider ([this]() {
        std::vector<DrumKitRowInfo> out;
        const auto src = getKitDrumList();
        out.reserve (src.size());
        for (const auto& s : src)
        {
            DrumKitRowInfo r;
            r.pageIndex   = s.pageIndex;
            r.displayName = s.displayName;
            r.hasEngine   = s.hasEngine;
            r.locked      = s.locked;
            r.isActive    = s.isActive;
            r.color       = s.color;
            out.push_back (std::move (r));
        }
        return out;
    });

    auto auditionDispatch = [this] (int row, bool on)
    {
        auto list = getKitDrumList();
        if (row < 0 || row >= (int) list.size()) return;
        const int targetTabId = list[(size_t) row].ribbonTabId;
        for (auto* entry : mPages)
        {
            if (! entry || entry->ribbonTabId != targetTabId) continue;
            if (auto* targetDp = dynamic_cast<DrumPage*> (entry->component.get()))
            {
                auto* eng = targetDp->getEngineProcessor();
                if (auto* s = dynamic_cast<BaySickSynthProcessor*> (eng))
                {
                    if (on) s->auditionNoteOn (60); else s->auditionNoteOff (60);
                }
                else if (auto* v = dynamic_cast<VibePlayerProcessor*> (eng))
                {
                    if (on) v->auditionNoteOn (60); else v->auditionNoteOff (60);
                }
            }
            return;
        }
    };
    kit->setAuditionHandlers (
        [auditionDispatch] (int row) { auditionDispatch (row, true);  },
        [auditionDispatch] (int row) { auditionDispatch (row, false); });

    kit->setReorderHandler ([this] (int srcRow, int dstRow)
    {
        moveDrumTab (srcRow, dstRow);
    });

    kit->setRowClickHandler ([this] (int row, juce::Component* anchor)
    {
        auto list = getKitDrumList();
        if (row < (int) list.size())
        {
            const int targetId = list[row].ribbonTabId;
            if (mRibbon) mRibbon->selectTab (targetId);
            onTabSelected (targetId);
            if (auto* targetDp = dynamic_cast<DrumPage*> (mVisiblePage))
            {
                targetDp->switchTab (0);
                if (targetDp->isEngineLocked())
                    targetDp->showContextMenu (anchor);
                else
                    targetDp->showSoundPicker (anchor);
            }
        }
        else
        {
            onAddTabRequest (RibbonTabBar::TabType::Drums);
            if (auto* newDp = dynamic_cast<DrumPage*> (mVisiblePage))
            {
                newDp->switchTab (0);
                newDp->showSoundPicker (anchor);
            }
        }
    });

    kit->onKitMenuRequested    = [this] (juce::Component* anchor) { showKitMenu (anchor); };
    kit->onGlobalLockRequested = [this] { showGlobalLockPrompt(); };
}

// 2026-04-26 (step 2 commit 2): per-engine register helpers.  Each builds a
// PianoRollConnection whose lambdas capture the engine page raw ptr; the
// closures read getEngineProcessor() per-call so engine swaps survive
// without re-registering the connection.
void StandaloneEditor::registerLayerPianoRoll (LayersPage* lp)
{
    if (! mPianoRollPage || ! lp) return;
    PianoRollConnection conn;
    conn.dataAccessor = [this, lp]() -> PianoRollData*
    {
        const int idx = lp ? lp->getPageIndex() : -1;
        if (idx < 0) return nullptr;
        return &mPM->currentPattern().layerRoll[idx];
    };
    conn.noteColor   = lp->getPageColor();
    conn.displayName = lp->getTabName();
    auto cast = [lp]() { return lp->getEngineProcessor(); };
    conn.auditionMomentary = [cast](int n) {
        if (auto* s = dynamic_cast<BaySickSynthProcessor*>(cast())) s->auditionNote(n);
        else if (auto* h = dynamic_cast<HarmlessProcessor*>(cast())) h->auditionNote(n);
        else if (auto* v = dynamic_cast<VibePlayerProcessor*>(cast())) v->auditionNote(n);
    };
    conn.auditionOn = [cast](int n) {
        if (auto* s = dynamic_cast<BaySickSynthProcessor*>(cast())) s->auditionNoteOn(n);
        else if (auto* h = dynamic_cast<HarmlessProcessor*>(cast())) h->auditionNoteOn(n);
        else if (auto* v = dynamic_cast<VibePlayerProcessor*>(cast())) v->auditionNoteOn(n);
    };
    conn.auditionOff = [cast](int n) {
        if (auto* s = dynamic_cast<BaySickSynthProcessor*>(cast())) s->auditionNoteOff(n);
        else if (auto* h = dynamic_cast<HarmlessProcessor*>(cast())) h->auditionNoteOff(n);
        else if (auto* v = dynamic_cast<VibePlayerProcessor*>(cast())) v->auditionNoteOff(n);
    };
    mPianoRollPage->registerEngine ({ EngineKind::Layer, lp->getPageIndex() }, std::move (conn));
}

void StandaloneEditor::registerBassPianoRoll (BassPage* bp)
{
    if (! mPianoRollPage || ! bp) return;
    PianoRollConnection conn;
    conn.dataAccessor = [this, bp]() -> PianoRollData*
    {
        const int idx = bp ? bp->getPageIndex() : -1;
        if (idx < 0) return nullptr;
        return &mPM->currentPattern().bassRoll[idx];
    };
    conn.noteColor   = bp->getPageColor();
    conn.displayName = bp->getTabName();
    auto cast = [bp]() { return bp->getEngineProcessor(); };
    conn.auditionMomentary = [cast](int n) {
        if (auto* s = dynamic_cast<BaySickBassProcessor*>(cast())) s->auditionNote(n);
        else if (auto* y = dynamic_cast<BaySickSynthProcessor*>(cast())) y->auditionNote(n);
        else if (auto* h = dynamic_cast<HarmlessProcessor*>(cast())) h->auditionNote(n);
        else if (auto* v = dynamic_cast<VibePlayerProcessor*>(cast())) v->auditionNote(n);
    };
    conn.auditionOn = [cast](int n) {
        if (auto* s = dynamic_cast<BaySickBassProcessor*>(cast())) s->auditionNoteOn(n);
        else if (auto* y = dynamic_cast<BaySickSynthProcessor*>(cast())) y->auditionNoteOn(n);
        else if (auto* h = dynamic_cast<HarmlessProcessor*>(cast())) h->auditionNoteOn(n);
        else if (auto* v = dynamic_cast<VibePlayerProcessor*>(cast())) v->auditionNoteOn(n);
    };
    conn.auditionOff = [cast](int n) {
        if (auto* s = dynamic_cast<BaySickBassProcessor*>(cast())) s->auditionNoteOff(n);
        else if (auto* y = dynamic_cast<BaySickSynthProcessor*>(cast())) y->auditionNoteOff(n);
        else if (auto* h = dynamic_cast<HarmlessProcessor*>(cast())) h->auditionNoteOff(n);
        else if (auto* v = dynamic_cast<VibePlayerProcessor*>(cast())) v->auditionNoteOff(n);
    };
    mPianoRollPage->registerEngine ({ EngineKind::Bass, bp->getPageIndex() }, std::move (conn));
}

void StandaloneEditor::registerDrumPianoRoll (DrumPage* dp)
{
    if (! mPianoRollPage || ! dp) return;
    PianoRollConnection conn;
    conn.dataAccessor = [this, dp]() -> PianoRollData*
    {
        const int idx = dp ? dp->getPageIndex() : -1;
        if (idx < 0) return nullptr;
        return &mPM->currentPattern().drumRolls[idx];
    };
    conn.noteColor   = dp->getPageColor();
    conn.displayName = dp->getTabName();
    auto cast = [dp]() { return dp->getEngineProcessor(); };
    conn.auditionMomentary = [cast](int n) {
        if (auto* s = dynamic_cast<BaySickSynthProcessor*>(cast())) s->auditionNote(n);
        else if (auto* v = dynamic_cast<VibePlayerProcessor*>(cast())) v->auditionNote(n);
    };
    conn.auditionOn = [cast](int n) {
        if (auto* s = dynamic_cast<BaySickSynthProcessor*>(cast())) s->auditionNoteOn(n);
        else if (auto* v = dynamic_cast<VibePlayerProcessor*>(cast())) v->auditionNoteOn(n);
    };
    conn.auditionOff = [cast](int n) {
        if (auto* s = dynamic_cast<BaySickSynthProcessor*>(cast())) s->auditionNoteOff(n);
        else if (auto* v = dynamic_cast<VibePlayerProcessor*>(cast())) v->auditionNoteOff(n);
    };
    mPianoRollPage->registerEngine ({ EngineKind::Drum, dp->getPageIndex() }, std::move (conn));
}

void StandaloneEditor::showGlobalLockPrompt ()
{
    // Skip the prompt if the user previously checked "Don't show again".
    if (mProjectManager && mProjectManager->getSkipGlobalLockPrompt())
    {
        applyGlobalLockToggle();
        return;
    }

    auto dontAsk = std::make_unique<juce::ToggleButton> ("Don't show again");
    dontAsk->setSize (160, 24);
    auto* aw = new juce::AlertWindow ("Lock/Unlock All Drums?",
        "This will lock/unlock all drum slots, do you wish to proceed?",
        juce::AlertWindow::QuestionIcon);
    aw->addCustomComponent (dontAsk.get());
    aw->addButton ("Proceed", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel",  0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<StandaloneEditor> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, aw, dontAsk = std::move (dontAsk)] (int r) mutable
        {
            std::unique_ptr<juce::AlertWindow> own (aw);
            if (! safeThis || r != 1) return;
            auto* se = safeThis.getComponent();
            if (dontAsk && dontAsk->getToggleState() && se->mProjectManager)
                se->mProjectManager->setSkipGlobalLockPrompt (true);
            se->applyGlobalLockToggle();
        }), false);
}

void StandaloneEditor::applyGlobalLockToggle ()
{
    // Smart toggle: if any drum is unlocked → lock all; if all locked → unlock all.
    bool anyUnlocked = false;
    for (auto& e : mPages)
    {
        if (! e || e->type != RibbonTabBar::TabType::Drums) continue;
        if (auto* dp = dynamic_cast<DrumPage*> (e->component.get()))
            if (! dp->isLocked()) { anyUnlocked = true; break; }
    }
    const bool newLocked = anyUnlocked;   // lock all if any unlocked, else unlock all
    for (auto& e : mPages)
    {
        if (! e || e->type != RibbonTabBar::TabType::Drums) continue;
        if (auto* dp = dynamic_cast<DrumPage*> (e->component.get()))
            dp->setLocked (newLocked);
    }
    refreshAllKitViews();   // ribbon + sidebar pickers redraw with new [L] prefixes
}

// ─────────────────────────────────────────────────────────────────────────────
// 2026-04-26: Templates (kit + 8 layers + 4 basses bundle)
// Template XML format:
//   <BaySickTemplate name="..." version="1">
//     <Kit path="TR-808/TR-808 Full.xml"/>            (relative to Kits/Factory/)
//     <Layer slot="N" engine="X" presetPath="..." locked="1"/>
//     <Bass  slot="N" engine="X" presetPath="..." locked="1"/>
//   </BaySickTemplate>
// loadTemplate tears down all dynamic tabs, loads the kit (drums), then
// creates Layer/Bass tabs and applies their presets.
// ─────────────────────────────────────────────────────────────────────────────
juce::File StandaloneEditor::templatesDir()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("BaySickDAW")
               .getChildFile ("Templates");
}
juce::File StandaloneEditor::factoryTemplatesDir() { return templatesDir().getChildFile ("Factory"); }
juce::File StandaloneEditor::userTemplatesDir   () { return templatesDir().getChildFile ("My Templates"); }

juce::Component* StandaloneEditor::spawnLayerTabFromTemplate (const juce::String& engine,
                                                               const juce::File& presetFile,
                                                               bool locked)
{
    auto page = createLayersPage();
    if (! page) return nullptr;
    auto* lp = dynamic_cast<LayersPage*> (page.get());
    if (lp == nullptr) return nullptr;

    const int pageIdx = lp->getPageIndex();
    const int newId = mRibbon->addTab (RibbonTabBar::TabType::Layers,
                                        "Layer " + juce::String (pageIdx + 1));

    lp->onEngineSelected = [this, newId, pageIdx] {
        const auto* tab = mRibbon->getTabById (newId);
        if (mMixerPage)   mMixerPage->addLayerChannel (pageIdx, tab ? tab->name : "Layer");
        if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
    };
    lp->onDeleteRequested = [this, newId] {
        if (! mRibbon) return;
        if (mRibbon->isLastOfType (RibbonTabBar::TabType::Layers)) return;
        mRibbon->closeTab (newId);
    };
    lp->onDuplicateRequested = [this] (const juce::String& xml) { spawnDuplicateLayerTab (xml); };
    lp->onLockChanged = [this, newId, lp] {
        if (mRibbon) mRibbon->setTabLocked (newId, lp->isLocked());
    };
    lp->onRenameRequested = [this, newId] {
        if (mRibbon) mRibbon->startRename (newId);
    };
    lp->onSoundNameChanged = [this, newId, pageIdx] (const juce::String& nm) {
        if (nm.isEmpty()) return;
        if (mRibbon)    mRibbon->renameTab (newId, nm);
        if (mMixerPage) mMixerPage->renameChannel (pageIdx, nm);
        if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Layer, pageIdx }, nm);
    };
    registerLayerPianoRoll (lp);

    auto* entry = new PageEntry();
    entry->ribbonTabId = newId;
    entry->type        = RibbonTabBar::TabType::Layers;
    entry->component   = std::move (page);
    addChildComponent (*entry->component);
    mPages.add (entry);

    lp->selectEngine (engine);
    if (presetFile.existsAsFile())
        lp->loadPreset (presetFile);
    if (locked)
        lp->setLocked (true);
    return entry->component.get();
}

juce::Component* StandaloneEditor::spawnBassTabFromTemplate (const juce::String& engine,
                                                              const juce::File& presetFile,
                                                              bool locked)
{
    auto page = createBassPage();
    if (! page) return nullptr;
    auto* bp = dynamic_cast<BassPage*> (page.get());
    if (bp == nullptr) return nullptr;

    const int pageIdx = bp->getPageIndex();
    const int newId = mRibbon->addTab (RibbonTabBar::TabType::Bass,
                                        "Bass " + juce::String (pageIdx + 1));

    bp->onEngineSelected = [this, newId, pageIdx] {
        const auto* tab = mRibbon->getTabById (newId);
        if (mMixerPage)   mMixerPage->addBassChannel (pageIdx, tab ? tab->name : "Bass");
        if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
    };
    bp->onDeleteRequested = [this, newId] {
        if (! mRibbon) return;
        if (mRibbon->isLastOfType (RibbonTabBar::TabType::Bass)) return;
        mRibbon->closeTab (newId);
    };
    bp->onDuplicateRequested = [this] (const juce::String& xml) { spawnDuplicateBassTab (xml); };
    bp->onLockChanged = [this, newId, bp] {
        if (mRibbon) mRibbon->setTabLocked (newId, bp->isLocked());
    };
    bp->onRenameRequested = [this, newId] {
        if (mRibbon) mRibbon->startRename (newId);
    };
    bp->onSoundNameChanged = [this, newId, pageIdx] (const juce::String& nm) {
        if (nm.isEmpty()) return;
        if (mRibbon)    mRibbon->renameTab (newId, nm);
        if (mMixerPage) mMixerPage->renameChannel (pageIdx, nm);
        if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Bass, pageIdx }, nm);
    };
    registerBassPianoRoll (bp);

    auto* entry = new PageEntry();
    entry->ribbonTabId = newId;
    entry->type        = RibbonTabBar::TabType::Bass;
    entry->component   = std::move (page);
    addChildComponent (*entry->component);
    mPages.add (entry);

    bp->selectEngine (engine);
    if (presetFile.existsAsFile())
        bp->loadPreset (presetFile);
    if (locked)
        bp->setLocked (true);
    return entry->component.get();
}

void StandaloneEditor::loadTemplate (const juce::File& templateXml)
{
    if (! templateXml.existsAsFile()) return;
    auto parsed = juce::XmlDocument::parse (templateXml);
    if (! parsed || ! parsed->hasTagName ("BaySickTemplate")) return;

    // 1. Tear down everything dynamic — Layers, Bass, Drums tabs all go.
    closeAllDynamicTabs();
    if (mMixerPage) mMixerPage->clearDynamicStrips();

    // 2. Load kit (creates Drums tabs).  Path is relative to factoryTemplatesDir's
    //    sibling Kits/Factory/ tree, which is what generate_factory_templates writes.
    juce::String kitPathStr;
    if (auto* kitEl = parsed->getChildByName ("Kit"))
        kitPathStr = kitEl->getStringAttribute ("path");
    if (kitPathStr.isNotEmpty())
    {
        const auto kitFile = factoryKitsDir().getChildFile (kitPathStr);
        if (kitFile.existsAsFile())
            loadKitImpl (kitFile);
    }

    // 3. Walk Layer + Bass entries in order so slots N=0,1,2,… line up.
    const auto presetsRoot = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                 .getChildFile ("BaySickDAW")
                                 .getChildFile ("Presets");

    int firstNewLayer = -1;
    for (int i = 0; i < parsed->getNumChildElements(); ++i)
    {
        auto* el = parsed->getChildElement (i);
        if (! el) continue;
        const juce::String engine     = el->getStringAttribute ("engine");
        const juce::String presetPath = el->getStringAttribute ("presetPath");
        const bool         locked     = el->getIntAttribute    ("locked", 0) != 0;
        const auto presetFile = presetsRoot.getChildFile (presetPath);

        if (el->hasTagName ("Layer"))
        {
            spawnLayerTabFromTemplate (engine, presetFile, locked);
            if (firstNewLayer < 0)
            {
                // Find the just-created layer tab id (the newest Layers entry).
                for (auto& e : mPages)
                    if (e && e->type == RibbonTabBar::TabType::Layers)
                        firstNewLayer = e->ribbonTabId;   // overwrites; last entry wins
            }
        }
        else if (el->hasTagName ("Bass"))
        {
            spawnBassTabFromTemplate (engine, presetFile, locked);
        }
    }

    // 4. Land on the first layer tab so the user sees something populated.
    if (firstNewLayer >= 0 && mRibbon)
    {
        mRibbon->selectTab (firstNewLayer);
        onTabSelected (firstNewLayer);
    }

    refreshAllKitViews();
    if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
}

void StandaloneEditor::saveTemplateAs ()
{
    auto* aw = new juce::AlertWindow ("Save Template As",
        "Enter a name for this template (saved kit + layers + basses):",
        juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", "My Template");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<StandaloneEditor> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, aw] (int r)
        {
            std::unique_ptr<juce::AlertWindow> own (aw);
            if (! safeThis || r != 1) return;
            const juce::String name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;
            auto* se = safeThis.getComponent();

            auto dir = userTemplatesDir();
            dir.createDirectory();
            const auto file = dir.getChildFile (name + ".xml");

            juce::XmlElement root ("BaySickTemplate");
            root.setAttribute ("name", name);
            root.setAttribute ("version", 1);

            // Kit — embed each drum tab's state inline (mirror saveKitAs's
            // user-saved kit format).  Loader handles both this and the
            // factory <Kit path="..."/> form.
            auto* kitEl = root.createNewChildElement ("Kit");
            int drumSlot = 0;
            for (auto& e : se->mPages)
            {
                if (! e || e->type != RibbonTabBar::TabType::Drums) continue;
                auto* dp = dynamic_cast<DrumPage*> (e->component.get());
                if (! dp) continue;
                auto* drumEl = kitEl->createNewChildElement ("Drum");
                drumEl->setAttribute ("slot",   drumSlot++);
                drumEl->setAttribute ("locked", dp->isLocked() ? 1 : 0);
                if (auto stateXml = juce::XmlDocument::parse (dp->exportDrumState()))
                    drumEl->addChildElement (stateXml.release());
            }

            // Layers — embed each layer's serialized state.
            int layerSlot = 0;
            for (auto& e : se->mPages)
            {
                if (! e || e->type != RibbonTabBar::TabType::Layers) continue;
                auto* lp = dynamic_cast<LayersPage*> (e->component.get());
                if (! lp) continue;
                auto* layerEl = root.createNewChildElement ("Layer");
                layerEl->setAttribute ("slot",   layerSlot++);
                layerEl->setAttribute ("engine", lp->getEngineType());
                layerEl->setAttribute ("locked", lp->isLocked() ? 1 : 0);
                if (auto stateXml = juce::XmlDocument::parse (lp->exportLayerState()))
                    layerEl->addChildElement (stateXml.release());
            }

            // Basses — same pattern.
            int bassSlot = 0;
            for (auto& e : se->mPages)
            {
                if (! e || e->type != RibbonTabBar::TabType::Bass) continue;
                auto* bp = dynamic_cast<BassPage*> (e->component.get());
                if (! bp) continue;
                auto* bassEl = root.createNewChildElement ("Bass");
                bassEl->setAttribute ("slot",   bassSlot++);
                bassEl->setAttribute ("engine", bp->getEngineType());
                bassEl->setAttribute ("locked", bp->isLocked() ? 1 : 0);
                if (auto stateXml = juce::XmlDocument::parse (bp->exportBassState()))
                    bassEl->addChildElement (stateXml.release());
            }

            root.writeTo (file, {});
        }), false);
}

void StandaloneEditor::showTemplateMenu (juce::Component* anchor)
{
    if (anchor == nullptr) return;

    constexpr int kIdSaveAs = 1;
    constexpr int kLoadBase = 100;

    juce::PopupMenu m;
    m.addItem (kIdSaveAs, "Save Template As...");
    m.addSeparator();

    juce::Array<juce::File> templateFiles;
    std::function<void (juce::PopupMenu&, const juce::File&)> walk;
    walk = [&] (juce::PopupMenu& sub, const juce::File& dir)
    {
        if (! dir.isDirectory()) return;
        juce::Array<juce::File> dirs;
        dir.findChildFiles (dirs, juce::File::findDirectories, false);
        dirs.sort();
        for (auto& d : dirs)
        {
            juce::PopupMenu inner;
            walk (inner, d);
            if (inner.getNumItems() > 0)
                sub.addSubMenu (d.getFileName(), inner);
        }
        auto files = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
        files.sort();
        for (auto& f : files)
        {
            sub.addItem (kLoadBase + templateFiles.size(), f.getFileNameWithoutExtension());
            templateFiles.add (f);
        }
    };

    {
        juce::PopupMenu mySub;
        walk (mySub, userTemplatesDir());
        if (mySub.getNumItems() == 0)
            mySub.addItem (-1, "(no user templates)", false, false);
        m.addSubMenu ("My Templates", mySub);
    }
    {
        juce::PopupMenu facSub;
        walk (facSub, factoryTemplatesDir());
        if (facSub.getNumItems() == 0)
            facSub.addItem (-1, "(no factory templates)", false, false);
        m.addSubMenu ("Factory Templates", facSub);
    }

    m.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (anchor),
        [this, templateFiles = std::move (templateFiles), kIdSaveAs, kLoadBase] (int r) mutable
        {
            if (r <= 0) return;
            if (r == kIdSaveAs) { saveTemplateAs(); return; }
            if (r >= kLoadBase && r < kLoadBase + templateFiles.size())
                loadTemplate (templateFiles[r - kLoadBase]);
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// Batch 5: Kit save / load
// ─────────────────────────────────────────────────────────────────────────────
juce::File StandaloneEditor::kitsDir()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("BaySickDAW")
               .getChildFile ("Kits");
}

juce::File StandaloneEditor::userKitsDir()
{
    return kitsDir().getChildFile ("My Kits");
}

juce::File StandaloneEditor::factoryKitsDir()
{
    return kitsDir().getChildFile ("Factory");
}

void StandaloneEditor::showKitMenu (juce::Component* anchor)
{
    if (anchor == nullptr) return;

    constexpr int kIdSaveAs   = 1;
    constexpr int kLoadBase   = 100;   // 100 + i indexes into kitFiles[]

    juce::PopupMenu m;
    m.addItem (kIdSaveAs, "Save Kit As...");
    m.addSeparator();

    juce::Array<juce::File> kitFiles;
    // Recursive: subfolders become real cascading submenus, XMLs become items.
    // Matches the sample / preset picker UX so Factory > TR-808 > TR-808 Basic
    // works without flattening the style folders into a single list.
    std::function<void (juce::PopupMenu&, const juce::File&)> addKitsFromDir;
    addKitsFromDir = [&] (juce::PopupMenu& sub, const juce::File& dir)
    {
        if (! dir.isDirectory()) return;
        juce::Array<juce::File> dirs;
        dir.findChildFiles (dirs, juce::File::findDirectories, false);
        dirs.sort();
        for (auto& d : dirs)
        {
            juce::PopupMenu inner;
            addKitsFromDir (inner, d);
            if (inner.getNumItems() > 0)
                sub.addSubMenu (d.getFileName(), inner);
        }
        auto files = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
        files.sort();
        for (auto& f : files)
        {
            sub.addItem (kLoadBase + kitFiles.size(), f.getFileNameWithoutExtension());
            kitFiles.add (f);
        }
    };

    {
        juce::PopupMenu mySub;
        addKitsFromDir (mySub, userKitsDir());
        if (mySub.getNumItems() == 0)
            mySub.addItem (-1, "(no user kits)", false, false);
        m.addSubMenu ("My Kits", mySub);
    }
    {
        juce::PopupMenu facSub;
        addKitsFromDir (facSub, factoryKitsDir());
        if (facSub.getNumItems() == 0)
            facSub.addItem (-1, "(no factory kits)", false, false);
        m.addSubMenu ("Factory Kits", facSub);
    }

    m.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (anchor),
        [this, kitFiles = std::move (kitFiles), kIdSaveAs, kLoadBase] (int r) mutable
        {
            if (r <= 0) return;
            if (r == kIdSaveAs) { saveKitAs(); return; }
            if (r >= kLoadBase && r < kLoadBase + kitFiles.size())
                loadKit (kitFiles[r - kLoadBase]);
        });
}

void StandaloneEditor::saveKitAs()
{
    auto* aw = new juce::AlertWindow (
        "Save Kit As",
        "Enter a name for this 16-drum kit:",
        juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", "My Kit");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, aw] (int r)
        {
            std::unique_ptr<juce::AlertWindow> own (aw);
            if (r != 1) return;
            const juce::String name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;

            auto dir = userKitsDir();
            dir.createDirectory();
            auto file = dir.getChildFile (name + ".xml");

            // Build the kit XML: walk every DrumPage in mPages, snapshot
            // each via exportDrumState() and embed under a slot wrapper.
            juce::XmlElement root ("BaySickKit");
            root.setAttribute ("name",    name);
            root.setAttribute ("version", "1");

            for (auto& entry : mPages)
            {
                if (! entry || entry->type != RibbonTabBar::TabType::Drums) continue;
                auto* dp = dynamic_cast<DrumPage*> (entry->component.get());
                if (dp == nullptr) continue;
                const juce::String stateXml = dp->exportDrumState();
                if (stateXml.isEmpty()) continue;   // empty slot — skip
                auto* slotEl = root.createNewChildElement ("Drum");
                slotEl->setAttribute ("slot", dp->getPageIndex());
                if (auto parsed = juce::XmlDocument::parse (stateXml))
                    slotEl->addChildElement (parsed.release());
            }

            root.writeTo (file, {});
        }), false);
}

void StandaloneEditor::loadKit (const juce::File& kitXml)
{
    // 2026-04-26: kit-replace confirmation.  If any existing drum tab has an
    // engine loaded, prompt the user before tearing it all down.  Skipped if
    // the user previously checked "Don't show again".
    bool anyExistingDrum = false;
    for (auto& e : mPages)
    {
        if (! e || e->type != RibbonTabBar::TabType::Drums) continue;
        if (auto* dp = dynamic_cast<DrumPage*> (e->component.get()))
            if (! dp->getEngineType().isEmpty()) { anyExistingDrum = true; break; }
    }
    if (anyExistingDrum && mProjectManager && ! mProjectManager->getSkipKitReplacePrompt())
    {
        auto dontAsk = std::make_unique<juce::ToggleButton> ("Don't show again");
        dontAsk->setSize (160, 24);
        auto* aw = new juce::AlertWindow ("Replace Drums?",
            "This action will remove all currently selected drums, do you wish to proceed?",
            juce::AlertWindow::QuestionIcon);
        aw->addCustomComponent (dontAsk.get());
        aw->addButton ("Proceed", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel",  0, juce::KeyPress (juce::KeyPress::escapeKey));

        juce::Component::SafePointer<StandaloneEditor> safeThis (this);
        const juce::File capturedKit = kitXml;
        aw->enterModalState (true, juce::ModalCallbackFunction::create (
            [safeThis, aw, dontAsk = std::move (dontAsk), capturedKit] (int r) mutable
            {
                std::unique_ptr<juce::AlertWindow> own (aw);
                if (! safeThis || r != 1) return;
                auto* se = safeThis.getComponent();
                if (dontAsk && dontAsk->getToggleState() && se->mProjectManager)
                    se->mProjectManager->setSkipKitReplacePrompt (true);
                se->loadKitImpl (capturedKit);   // proceed (skip prompt this time)
            }), false);
        return;
    }
    // No existing drums (or prompt opted out) — load directly.
    loadKitImpl (kitXml);
}

void StandaloneEditor::loadKitImpl (const juce::File& kitXml)
{
    juce::StringArray loadFailures;   // accumulates per-drum failure reasons for an end-of-load AlertWindow
    if (! kitXml.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
            "Kit Load Failed",
            "Kit file not found:\n" + kitXml.getFullPathName());
        return;
    }
    auto px = juce::XmlDocument::parse (kitXml);
    if (! px)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
            "Kit Load Failed",
            "Kit XML could not be parsed:\n" + kitXml.getFullPathName());
        return;
    }
    if (! px->hasTagName ("BaySickKit"))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
            "Kit Load Failed",
            "Kit XML root tag is not BaySickKit (found: "
                + px->getTagName() + ")");
        return;
    }

    // Tear down every existing drum tab.  Drums-only — keep Layers + Bass.
    // Mirrors closeAllDynamicTabs's pattern (onTabClosed handles mPages /
    // slot freeing; ribbon needs a separate wipe via clearTabsOfType).
    {
        juce::Array<int> drumIds;
        for (auto& e : mPages)
            if (e && e->type == RibbonTabBar::TabType::Drums)
                drumIds.add (e->ribbonTabId);
        for (int id : drumIds)
            onTabClosed (id);
        if (mRibbon)
            mRibbon->clearTabsOfType (RibbonTabBar::TabType::Drums);
    }

    // Rebuild.  Supports two <Drum slot="N"> formats:
    //   (a) User-saved (embedded state): <Drum slot="N"><DrumPageState data=base64.../></Drum>
    //       — the data attribute carries the engine's getStateInformation() blob.
    //       Loaded via DrumPage::importDrumState.
    //   (b) Factory format (preset reference):
    //         <Drum slot="N" engine="BaySickSynth" presetPath="BaySickDrums/808 Group/808 Kick.xml" locked="1"/>
    //       — presetPath is relative to Documents/BaySickDAW/Presets/ (includes source folder).
    //       Loaded via DrumPage::loadSynthPreset / loadPlayerPreset.
    int firstNewTabId = -1;   // for selectTab/onTabSelected at the end
    for (int i = 0; i < px->getNumChildElements(); ++i)
    {
        auto* drumEl = px->getChildElement (i);
        if (! drumEl || ! drumEl->hasTagName ("Drum")) continue;
        const int slot = drumEl->getIntAttribute ("slot", -1);
        if (slot < 0 || slot >= kMaxDrumPages)
        {
            loadFailures.add ("slot " + juce::String (slot) + ": out of range");
            continue;
        }

        const juce::String presetPath = drumEl->getStringAttribute ("presetPath");
        const juce::String drumEngine = drumEl->getStringAttribute ("engine");
        const bool         drumLocked = drumEl->getIntAttribute    ("locked", 0) != 0;

        auto* stateEl = drumEl->getChildByName ("DrumPageState");
        const bool isFactoryRef = presetPath.isNotEmpty() && drumEngine.isNotEmpty();
        if (! isFactoryRef && stateEl == nullptr)
        {
            loadFailures.add ("slot " + juce::String (slot) + ": no presetPath and no DrumPageState");
            continue;
        }

        const juce::String stateXml = (stateEl != nullptr)
            ? stateEl->toString (juce::XmlElement::TextFormat().singleLine())
            : juce::String();

        auto page = createDrumPageAtIndex (slot);
        if (! page)
        {
            loadFailures.add ("slot " + juce::String (slot) + ": createDrumPageAtIndex failed (slot may be in use)");
            continue;
        }
        auto* dp = dynamic_cast<DrumPage*> (page.get());
        if (dp == nullptr)
        {
            loadFailures.add ("slot " + juce::String (slot) + ": dynamic_cast<DrumPage> failed");
            continue;
        }

        // Tab name: either the embedded DrumPageState's name, or the preset
        // file's stem for factory references.
        juce::String tabName;
        if (stateEl != nullptr)
            tabName = stateEl->getStringAttribute ("name");
        else if (isFactoryRef)
        {
            const auto stem = presetPath.fromLastOccurrenceOf ("/", false, false)
                                        .upToLastOccurrenceOf (".", false, false);
            tabName = stem;
        }
        const int newId = mRibbon->addTab (RibbonTabBar::TabType::Drums,
                                           tabName.isNotEmpty() ? tabName : "Drums");
        if (firstNewTabId < 0) firstNewTabId = newId;
        if (tabName.isNotEmpty()) dp->setTabName (tabName);

        const int pageIdx = slot;
        dp->onEngineSelected = [this, newId, pageIdx] {
            const auto* tab = mRibbon->getTabById (newId);
            if (mMixerPage)   mMixerPage->addDrumChannel (pageIdx, tab ? tab->name : "Drums");
            if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
            refreshAllKitViews();
        };
        dp->onSoundNameChanged = [this, newId, pageIdx, dp] (const juce::String& nm) {
            if (nm.isEmpty()) return;
            if (mRibbon)    mRibbon->renameTab (newId, nm);
            if (mMixerPage) mMixerPage->renameChannel (pageIdx, nm);
            dp->setTabName (nm);
            refreshAllKitViews();
            if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Drum, pageIdx }, nm);
        };
        dp->onDeleteRequested = [this, newId] {
            if (! mRibbon) return;
            if (mRibbon->isLastOfType (RibbonTabBar::TabType::Drums))
            {
                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                    "Cannot Delete",
                    "This is the only Drum tab. Add another first.");
                return;
            }
            mRibbon->closeTab (newId);
        };
        dp->onDuplicateRequested = [this] (const juce::String& xml) {
            spawnDuplicateDrumTab (xml);
        };
        dp->onLockChanged = [this, newId, dp] {
            if (mRibbon) mRibbon->setTabLocked (newId, dp->isLocked());
            refreshAllKitViews();
        };
        dp->onRenameRequested = [this, newId] {
            if (mRibbon) mRibbon->startRename (newId);
        };
        wireDrumPageKitView (dp);
        registerDrumPianoRoll (dp);

        auto* entry = new PageEntry();
        entry->ribbonTabId = newId;
        entry->type        = RibbonTabBar::TabType::Drums;
        entry->component   = std::move (page);
        addChildComponent (*entry->component);
        mPages.add (entry);

        // Apply state — factory ref or embedded.
        if (isFactoryRef)
        {
            // presetPath is relative to Documents/BaySickDAW/Presets/
            // (it includes the source-folder prefix, e.g. "BaySickDrums/808 Group/808 Kick.xml").
            // engine names the runtime engine type to instantiate (BaySickSynth or BaySickPlayer),
            // which can differ from the source folder.
            const auto presetFile = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                        .getChildFile ("BaySickDAW")
                                        .getChildFile ("Presets")
                                        .getChildFile (presetPath);
            if (! presetFile.existsAsFile())
            {
                loadFailures.add ("slot " + juce::String (slot) + ": preset not found on disk -> " + presetPath);
            }
            else if (drumEngine == "BaySickSynth")
            {
                dp->loadSynthPreset (presetFile);
            }
            else if (drumEngine == "BaySickPlayer")
            {
                dp->loadPlayerPreset (presetFile);
            }
            else
            {
                loadFailures.add ("slot " + juce::String (slot) + ": unknown engine '" + drumEngine + "'");
            }
            if (drumLocked) dp->setLocked (true);
        }
        else
        {
            dp->importDrumState (stateXml);
        }
    }

    // Ensure at least one Drums tab exists (ribbon expects this).  If the
    // kit was empty for some reason, spawn a default empty drum.
    bool anyDrumTab = false;
    for (auto& e : mPages)
        if (e && e->type == RibbonTabBar::TabType::Drums) { anyDrumTab = true; break; }
    if (! anyDrumTab) addDefaultDynamicTabs();   // rebuild the default trio

    // Select the first new drum tab so the user lands on a populated page.
    // Without this, the previously-visible page was destroyed by the tear-down
    // loop and nothing replaced it on screen → click felt like a no-op.
    // 2026-04-26 (step 2 polish): if the user was already on the unified
    // PianoRollPage with Drum Kit selected, stay there instead of jumping
    // to the freshly-spawned Drums page.  The kit view will repaint with
    // the new contents in place.
    const bool stayOnPianoRollKit = (mPianoRollPage != nullptr
        && mVisiblePage == mPianoRollPage
        && mPianoRollPage->getActiveEngineId().kind == EngineKind::DrumKit);
    if (! stayOnPianoRollKit && firstNewTabId >= 0 && mRibbon)
    {
        mRibbon->selectTab (firstNewTabId);
        onTabSelected (firstNewTabId);
    }

    refreshAllKitViews();
    if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();

    // Diagnostic: surface any per-drum failures so we can debug without VS.
    if (! loadFailures.isEmpty())
    {
        juce::String body = "Kit \"" + kitXml.getFileNameWithoutExtension() + "\" loaded with "
            + juce::String (loadFailures.size()) + " problem(s):\n\n";
        for (auto& f : loadFailures)
            body += "  - " + f + "\n";
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
            "Kit Load Report", body);
    }
}

void StandaloneEditor::stopPlayback()
{
    // Cancel any in-progress count-in
    mCountInTimer.stopTimer();
    mProcessor.mMetro.countInActive.store(false, std::memory_order_relaxed);

    mPlayHead.stop();

    // Seek to the start of the active time-selection if any, otherwise to 0.
    // Matches the rule: Stop stays in your work area when you're looping a section.
    double seekBeat = 0.0;
    if (mTransport && mTransport->isSongMode())
    {
        if (mBuilderPage && mBuilderPage->hasTimeSelection())
            seekBeat = mBuilderPage->getTimeSelStartBars() * 4.0;
    }
    else
    {
        // Pattern mode: check the currently-active piano roll for a time selection
        if (auto* roll = getActivePianoRollForLoop())
            if (roll->hasTimeSelection())
                seekBeat = roll->getTimeSelBeatStart();
    }
    mPlayHead.seekTo(seekBeat);

    // Flush every engine's active voices (CC 123 All-Notes-Off + pending note-offs).
    // The old path only hit the built-in mSynth; modern engines (Harmless/BaySick/
    // VibePlayer) ignored it, which is why song-mode stuck notes after Stop.
    mProcessor.mFlushAllNotes.store(true, std::memory_order_release);
    mProcessor.allNotesOff();   // keep legacy built-in flush too
    mProcessor.getBassSynth().noteOff();
    mTransport->setPlayState(false, false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Access
// ─────────────────────────────────────────────────────────────────────────────
PatternManager& StandaloneEditor::getPatternManager() { return *mPM; }

// ─────────────────────────────────────────────────────────────────────────────
// ApplicationCommandTarget (Phase A — 2026-04-26)
// ─────────────────────────────────────────────────────────────────────────────
void StandaloneEditor::getAllCommands (juce::Array<juce::CommandID>& out)
{
    for (const auto& ci : BSCommands::getAllCommands())
        out.add (ci.id);
}

void StandaloneEditor::getCommandInfo (juce::CommandID id,
                                       juce::ApplicationCommandInfo& info)
{
    if (auto* ci = BSCommands::findCommand ((int) id))
    {
        info.setInfo (ci->name, ci->tooltip,
                      BSCommands::categoryName (ci->category), 0);
        info.addDefaultKeypress (ci->defaultKey.getKeyCode(),
                                 ci->defaultKey.getModifiers());
    }
}

bool StandaloneEditor::perform (const InvocationInfo& info)
{
    switch ((int) info.commandID)
    {
        case BSCommands::cmdPlayPause:
            if (mTransport) mTransport->togglePlayPause();
            return true;

        case BSCommands::cmdStopAndDisarm:
            if (mTransport) mTransport->stopAndDisarm();
            return true;

        case BSCommands::cmdToggleRecord:
            if (mTransport) mTransport->toggleRecord();
            return true;

        // ── Page switches (Phase B-1) ────────────────────────────────────
        case BSCommands::cmdShowMixer:
            selectFirstTabOfType (RibbonTabBar::TabType::Mixer);
            return true;
        case BSCommands::cmdShowEffects:
            selectFirstTabOfType (RibbonTabBar::TabType::Effects);
            return true;
        case BSCommands::cmdShowBuilder:
            handleCommandMessage (3);   // existing Builder switch helper
            return true;
        case BSCommands::cmdShowLayers:
            handleCommandMessage (0);
            return true;
        case BSCommands::cmdShowBass:
            handleCommandMessage (1);
            return true;
        case BSCommands::cmdShowDrums:
            handleCommandMessage (2);
            return true;
        case BSCommands::cmdShowPianoRoll:
            // 2026-04-26 (1a + step 2): F11 routes to the unified Piano Roll
            // page and restores the last-used engine selection if one is
            // tracked (mLastRollKind/Index from the most-recently-active
            // engine page).  Falls back to whatever the dropdown currently
            // points at (Drum Kit on first launch).
            if (mRibbon) mRibbon->selectTab (4);
            onTabSelected (4);
            if (mPianoRollPage)
            {
                EngineKind lastKind = EngineKind::DrumKit;
                int        lastIdx  = 0;
                switch (mLastRollKind)
                {
                    case LastRollKind::Layer:  lastKind = EngineKind::Layer; lastIdx = mLastRollIndex; break;
                    case LastRollKind::Bass:   lastKind = EngineKind::Bass;  lastIdx = mLastRollIndex; break;
                    case LastRollKind::Drums:  lastKind = EngineKind::Drum;  lastIdx = mLastRollIndex; break;
                    default: break;
                }
                if (lastKind != EngineKind::DrumKit && lastIdx >= 0)
                    mPianoRollPage->selectEngine ({ lastKind, lastIdx });
            }
            return true;

        // ── File operations (Phase B-1) ─────────────────────────────────
        case BSCommands::cmdFileNew:    doFileNew();    return true;
        case BSCommands::cmdFileOpen:   doFileOpen();   return true;
        case BSCommands::cmdFileSave:   doFileSave();   return true;
        case BSCommands::cmdFileSaveAs: doFileSaveAs(); return true;

        // ── Pattern navigation (Phase B-2) ──────────────────────────────
        case BSCommands::cmdRenameActivePattern: showRenamePatternDialog(); return true;
        case BSCommands::cmdNextEmptyPattern:    jumpToNextEmptyPattern();  return true;
        case BSCommands::cmdNewPattern:          createNewPattern();        return true;
        case BSCommands::cmdNextPattern:         cyclePattern (+1);         return true;
        case BSCommands::cmdPrevPattern:         cyclePattern (-1);         return true;

        // ── Transport extensions (Phase B-3) ────────────────────────────
        case BSCommands::cmdToggleSongMode:
            if (mTransport) mTransport->toggleSongMode();
            return true;

        case BSCommands::cmdSeekHome:
            mPlayHead.seekTo (0.0);
            return true;

        case BSCommands::cmdFastForward:
            // 4 bars × 4 beats/bar = 16 beats.  Project is 4/4 globally; per-bar
            // time signatures (D-2) will refine this when they land.
            mPlayHead.seekTo (mPlayHead.getCurrentBeat() + 16.0);
            return true;

        case BSCommands::cmdPrevBarSong:
            if (mTransport && mTransport->isSongMode())
                mPlayHead.seekTo (juce::jmax (0.0, mPlayHead.getCurrentBeat() - 4.0));
            return true;

        case BSCommands::cmdNextBarSong:
            if (mTransport && mTransport->isSongMode())
                mPlayHead.seekTo (mPlayHead.getCurrentBeat() + 4.0);
            return true;

        case BSCommands::cmdToggleMetronome:
            if (mTransport) mTransport->toggleMetronome();
            return true;

        // ── Undo / Redo (Phase B-5) ─────────────────────────────────────
        case BSCommands::cmdGlobalUndo: globalUndo(); return true;
        case BSCommands::cmdGlobalRedo: globalRedo(); return true;

        // ── Recording precount (Phase D-5) ──────────────────────────────
        case BSCommands::cmdToggleRecordingPrecount:
            mPrecountEnabled = ! mPrecountEnabled;
            return true;

        default:
            return false;
    }
}

// ── Phase B-1 helpers ────────────────────────────────────────────────────
void StandaloneEditor::selectFirstTabOfType (RibbonTabBar::TabType type)
{
    for (auto* e : mPages)
    {
        if (e == nullptr || e->type != type) continue;
        if (mRibbon) mRibbon->selectTab (e->ribbonTabId);
        onTabSelected (e->ribbonTabId);
        return;
    }
}

void StandaloneEditor::showLastUsedPianoRoll()
{
    // Determine the target page kind + index.  Default to the first Layers tab
    // when no piano roll has been touched yet this session.
    auto kind  = mLastRollKind;
    auto index = mLastRollIndex;

    if (kind == LastRollKind::None)
    {
        // Fall back to first Layers tab so F11 always lands somewhere usable.
        kind  = LastRollKind::Layer;
        index = -1;   // -1 = "any" — selectFirstTabOfType handles it
    }

    // Find the matching ribbon tab.
    PageEntry* match = nullptr;
    for (auto* e : mPages)
    {
        if (e == nullptr) continue;

        if (kind == LastRollKind::Layer && e->type == RibbonTabBar::TabType::Layers)
        {
            if (auto* lp = dynamic_cast<LayersPage*> (e->component.get()))
                if (index < 0 || lp->getPageIndex() == index) { match = e; break; }
        }
        else if (kind == LastRollKind::Bass && e->type == RibbonTabBar::TabType::Bass)
        {
            if (auto* bp = dynamic_cast<BassPage*> (e->component.get()))
                if (index < 0 || bp->getPageIndex() == index) { match = e; break; }
        }
        else if (kind == LastRollKind::Drums && e->type == RibbonTabBar::TabType::Drums)
        {
            if (auto* dp = dynamic_cast<DrumPage*> (e->component.get()))
                if (index < 0 || dp->getPageIndex() == index) { match = e; break; }
        }
    }

    // No exact match (rare — page closed since last visit).  Try first tab of kind.
    if (match == nullptr)
    {
        const auto fallback = (kind == LastRollKind::Bass)  ? RibbonTabBar::TabType::Bass
                            : (kind == LastRollKind::Drums) ? RibbonTabBar::TabType::Drums
                                                            : RibbonTabBar::TabType::Layers;
        for (auto* e : mPages)
            if (e != nullptr && e->type == fallback) { match = e; break; }
    }

    if (match == nullptr) return;

    if (mRibbon) mRibbon->selectTab (match->ribbonTabId);
    onTabSelected (match->ribbonTabId);

    // Land on the Piano Roll sub-tab.  Sub-tab indices:
    //   LayersPage / BassPage : Player(0), Piano Roll(1), EQ(2)
    //   DrumPage              : Drum Kit(0), Player(1), Piano Roll(2), EQ(3)
    if (auto* lp = dynamic_cast<LayersPage*> (match->component.get())) lp->switchTab (1);
    else if (auto* bp = dynamic_cast<BassPage*>  (match->component.get())) bp->switchTab (1);
    else if (auto* dp = dynamic_cast<DrumPage*>  (match->component.get())) dp->switchTab (2);
}

// ── Phase B-2 helpers (pattern navigation) ──────────────────────────────────
bool StandaloneEditor::isPatternEmpty (int idx) const
{
    if (mPM == nullptr || idx < 0 || idx >= mPM->getNumPatterns()) return true;
    const auto& p = mPM->getPattern (idx);

    for (auto& r : p.layerRoll) if (! r.notes.empty()) return false;
    for (auto& r : p.bassRoll)  if (! r.notes.empty()) return false;
    for (auto& r : p.drumRolls) if (! r.notes.empty()) return false;
    if (! p.drumRoll.notes.empty()) return false;   // legacy single-roll drum data

    // Arrangement blocks live at project scope, not per-pattern, so don't check.
    return true;
}

void StandaloneEditor::showRenamePatternDialog()
{
    if (mPM == nullptr || mPM->getNumPatterns() == 0) return;

    const int idx = mPM->getCurrentPatternIndex();
    auto* aw = new juce::AlertWindow ("Rename Pattern",
                                      "Enter a new name:",
                                      juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", mPM->currentPattern().name);
    aw->addButton ("OK",     1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true,
        juce::ModalCallbackFunction::create (
            [this, idx, aw] (int r)
            {
                if (r != 1) return;
                const auto newName = aw->getTextEditorContents ("name").trim();
                if (newName.isEmpty()) return;
                mPM->renamePattern (idx, newName);
                refreshPatternBox();
            }),
        true);
}

void StandaloneEditor::jumpToNextEmptyPattern()
{
    if (mPM == nullptr || mPM->getNumPatterns() == 0) return;

    const int n   = mPM->getNumPatterns();
    const int cur = mPM->getCurrentPatternIndex();

    // Search forward from the next pattern, wrapping back around.
    for (int step = 1; step <= n; ++step)
    {
        const int candidate = (cur + step) % n;
        if (isPatternEmpty (candidate))
        {
            mPM->setCurrentPattern (candidate);
            refreshPatternBox();
            return;
        }
    }
    // No empty pattern found — leave selection unchanged (F4 covers create).
}

void StandaloneEditor::createNewPattern()
{
    if (mPM == nullptr) return;
    mPM->addPattern();
    mPM->setCurrentPattern (mPM->getNumPatterns() - 1);
    refreshPatternBox();
}

void StandaloneEditor::cyclePattern (int delta)
{
    if (mPM == nullptr) return;
    const int n = mPM->getNumPatterns();
    if (n <= 1) return;
    const int cur  = mPM->getCurrentPatternIndex();
    const int next = ((cur + delta) % n + n) % n;   // wrap (handles negative delta)
    mPM->setCurrentPattern (next);
    refreshPatternBox();
}

void StandaloneEditor::showKeyBindsWindow()
{
    if (mKeyBindsWin != nullptr)
    {
        mKeyBindsWin->toFront (true);
        return;
    }
    auto* w = new KeyBindsWindow (mCmdMgr);
    mKeyBindsWin = w;     // SafePointer — auto-clears when the window deletes itself
}

// ─────────────────────────────────────────────────────────────────────────────
// G-1.5 (temporary): BaySickNAM/IR layout test window.
// Owns a private BaySickNAMIRProcessor + its editor; not routed into the
// audio graph.  Lets the engine's UI be opened for layout review without
// requiring an ASIO device or graph integration.  Removed once the engine
// gets a real home (Inst tab / FX-rack effect type / wherever).
// ─────────────────────────────────────────────────────────────────────────────
namespace
{
    class NamIrTestWindow : public juce::DocumentWindow
    {
    public:
        NamIrTestWindow()
            : juce::DocumentWindow ("BaySickNAM/IR Test Layout",
                                    juce::Colour (0xff1a1a1a),
                                    juce::DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar (true);
            setResizable (false, false);

            mProc = std::make_unique<BaySickNAMIRProcessor>();
            // Prepare with a sane SR / block so loadImpulseResponse paths
            // work for layout review (no audio is actually routed through).
            mProc->prepareToPlay (44100.0, 512);

            setContentOwned (mProc->createEditor(), true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override { delete this; }

    private:
        std::unique_ptr<BaySickNAMIRProcessor> mProc;
        // Local tooltip window — the StandaloneEditor's main VibeTooltip only
        // monitors the editor's subtree, not this separate DocumentWindow,
        // so without a local instance the per-widget setTooltip() calls in
        // BaySickNAMIREditor would never fire.
        juce::TooltipWindow                    mTooltips { this, 600 };
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NamIrTestWindow)
    };
}

void StandaloneEditor::showNamIrTestWindow()
{
    if (mNamIrTestWin != nullptr)
    {
        mNamIrTestWin->toFront (true);
        return;
    }
    auto* w = new NamIrTestWindow();
    mNamIrTestWin = w;     // SafePointer — auto-clears on window self-delete
}

// ─────────────────────────────────────────────────────────────────────────────
// G-2: Clips ribbon helpers.
// ─────────────────────────────────────────────────────────────────────────────
void StandaloneEditor::showClipsEmptyState()
{
    // Hide every other page; show the empty-state drop zone.
    for (auto* entry : mPages)
        if (entry && entry->component) entry->component->setVisible (false);
    mVisiblePage = nullptr;

    if (! mClipsEmptyState) return;
    mClipsEmptyState->setVisible (true);
    mClipsEmptyState->toFront (false);
    if (mPageMenuBar)
    {
        mPageMenuBar->setPageTitle ("Clips");
        mPageMenuBar->clearTabSlots();
        mPageMenuBar->clearExtraRightComponents();
        mPageMenuBar->setBankIndicator (nullptr);
        mPageMenuBar->setMenuBuilder (nullptr);
    }
    resized();   // make sure the empty state has its bounds
}

void StandaloneEditor::spawnClipsTabIfMissing (int audioRow, const juce::String& path)
{
    if (path.isEmpty()) return;
    if (audioRow < 0 || audioRow >= kMaxClipPages) return;

    // Idempotent: skip if a Clips tab already exists for this audio file
    // (file-based dedup; the same file dropped on multiple Builder rows
    // produces ONE Clips page bound to the FIRST drop's audio row).
    for (auto* entry : mPages)
    {
        if (! entry) continue;
        if (entry->type != RibbonTabBar::TabType::Clip) continue;
        if (auto* cp = dynamic_cast<ClipsPage*> (entry->component.get()))
        {
            if (cp->getClipFilePath() == path) return;
        }
    }
    // Also skip if the row slot is already taken (defensive — shouldn't happen
    // since the row uniqueness is maintained by the audio insert flow).
    for (auto* entry : mPages)
    {
        if (! entry) continue;
        if (entry->type != RibbonTabBar::TabType::Clip) continue;
        if (auto* cp = dynamic_cast<ClipsPage*> (entry->component.get()))
            if (cp->getPageIndex() == audioRow) return;
    }

    // Page name = the filename without extension (or "Clip N" fallback).
    juce::String tabName = juce::File (path).getFileNameWithoutExtension();
    if (tabName.isEmpty()) tabName = "Clip " + juce::String (audioRow + 1);

    const int newId = mRibbon->addTab (RibbonTabBar::TabType::Clip, tabName);

    auto cpHolder = std::make_unique<ClipsPage> (audioRow);
    auto* cpRaw = cpHolder.get();
    cpRaw->setTabName (tabName);

    // G-3 (2026-04-28): resolve the imported path BEFORE handing it to the
    // engine.  P4's copy-on-drop flow returns RELATIVE paths (e.g.
    // "Samples/file.wav") that are valid for project save/load but break
    // VibePlayer's loadSampleFile (which uses juce::File without resolving
    // CWD).  resolveProjectFile produces an absolute path the engine can
    // open directly.
    juce::String resolvedPath = path;
    {
        const juce::File abs = mProcessor.resolveProjectFile (path);
        if (abs.existsAsFile())
            resolvedPath = abs.getFullPathName();
    }
    cpRaw->setClipFilePath (resolvedPath);

    // G-3 (2026-04-28): dual-engine swap pattern — onEngineDestroying fires
    // BEFORE the active engine pointer changes, so we unregister with the
    // OLD pointer still valid (no audio-thread dangling pointer).
    // onEngineChanged fires AFTER, so we register the new active processor.
    // Both engine instances stay alive across swaps inside ClipsPage so
    // settings persist.
    cpRaw->onEngineDestroying = [this, audioRow]()
    {
        mProcessor.unregisterClipEngine (audioRow);
    };
    cpRaw->onEngineChanged = [this, audioRow, cpRaw]()
    {
        if (auto* eng = cpRaw->getEngineProcessor())
        {
            const double sr = mProcessor.getSampleRate() > 0.0
                                ? mProcessor.getSampleRate() : 44100.0;
            const int    bs = mProcessor.getBlockSize() > 0
                                ? mProcessor.getBlockSize() : 512;
            eng->prepareToPlay (sr, bs);
            mProcessor.registerClipEngine (audioRow, eng);
        }
    };

    // 2026-04-28 (G-3): NO auto-pick.  Per Jeff's spec the picker stays
    // unlocked / unselected on first spawn — the user must pick BaySickPlayer
    // or BaySickNAM/IR explicitly, mirroring how Layer / Bass tabs behave.

    // G-3: register the piano roll with the unified PianoRollPage so the
    // Piano Roll sub-tab redirect (already wired in showPageForTab Clip
    // branch) finds an actual roll to display.
    registerClipPianoRoll (audioRow, cpRaw);

    addChildComponent (*cpRaw);

    auto entry           = std::make_unique<PageEntry>();
    entry->ribbonTabId   = newId;
    entry->type          = RibbonTabBar::TabType::Clip;
    entry->component     = std::move (cpHolder);
    mPages.add (entry.release());

    // Hide empty state, select the new tab.
    if (mClipsEmptyState) mClipsEmptyState->setVisible (false);
    mRibbon->selectTab (newId);
    onTabSelected (newId);
}

void StandaloneEditor::registerClipPianoRoll (int idx, ClipsPage* cp)
{
    if (! mPianoRollPage || ! cp) return;
    if (! mPM) return;

    PianoRollConnection conn;
    // dataAccessor closure — re-resolves &currentPattern().clipRoll[idx] each
    // tick so pattern switches stay live (mirrors the layer/bass/drum reg).
    auto* pmRaw = mPM.get();
    conn.dataAccessor = [pmRaw, idx]() -> PianoRollData*
    {
        if (! pmRaw) return nullptr;
        if (idx < 0 || idx >= (int) pmRaw->currentPattern().clipRoll.size()) return nullptr;
        return &pmRaw->currentPattern().clipRoll[idx];
    };
    conn.noteColor   = juce::Colour (0xffd4a017);   // VC::Warm — Clips amber
    conn.displayName = cp->getTabName();

    // Audition closures capture cp (raw ptr) and read getEngineProcessor()
    // per call so engine swaps via the picker survive without re-registering.
    conn.auditionMomentary = [cp](int n)
    {
        if (auto* vp = dynamic_cast<VibePlayerProcessor*> (cp->getEngineProcessor()))
            vp->auditionNote (n);
    };
    conn.auditionOn = [cp](int n)
    {
        if (auto* vp = dynamic_cast<VibePlayerProcessor*> (cp->getEngineProcessor()))
            vp->auditionNoteOn (n);
    };
    conn.auditionOff = [cp](int n)
    {
        if (auto* vp = dynamic_cast<VibePlayerProcessor*> (cp->getEngineProcessor()))
            vp->auditionNoteOff (n);
    };
    conn.rollMode = PianoRollContainer::RollMode::Standard;

    mPianoRollPage->registerEngine ({ EngineKind::Clip, idx }, conn);
}

// ─────────────────────────────────────────────────────────────────────────────
// Legacy command-message dispatch (page-switch ints sent by sub-pages)
// ─────────────────────────────────────────────────────────────────────────────
void StandaloneEditor::handleCommandMessage(int commandId)
{
    // Legacy: pages send integer page-switch commands
    // Map old PAGE_* constants to ribbon tab types for backwards compat
    // (Layers=0, Bass=1, Drums=2, Builder=3, Mastering=4)
    RibbonTabBar::TabType targetType;
    switch (commandId)
    {
    case 0: targetType = RibbonTabBar::TabType::Layers;  break;
    case 1: targetType = RibbonTabBar::TabType::Bass;    break;
    case 2: targetType = RibbonTabBar::TabType::Drums;   break;
    case 3: targetType = RibbonTabBar::TabType::Builder; break;
    default: return;
    }
    // Find first tab of that type and select it
    for (auto* entry : mPages)
    {
        if (entry->type == targetType)
        {
            mRibbon->selectTab(entry->ribbonTabId);
            onTabSelected(entry->ribbonTabId);
            return;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MenuBarModel
// ─────────────────────────────────────────────────────────────────────────────
juce::StringArray StandaloneEditor::getMenuBarNames()
{
    return { "File", "Edit", "Patterns", "View", "Options", "Help" };
}

juce::PopupMenu StandaloneEditor::getMenuForIndex(int menuIndex, const juce::String&)
{
    juce::PopupMenu m;

    switch (menuIndex)
    {
    case 0: // File — Project persistence (P2+P3+P6, 2026-04-23)
        m.addItem(101, "New Project...  (Ctrl+N)");
        m.addItem(102, "New from Template...");           // P6: pick + seed
        m.addSeparator();
        m.addItem(103, "Open Project...  (Ctrl+O)");
        // P3: Open Recent submenu — last 10 projects, missing ones greyed out.
        {
            juce::PopupMenu recent;
            if (mProjectManager)
            {
                const auto list = mProjectManager->getRecentProjects();
                int idx = 0;
                for (const auto& f : list)
                {
                    const bool exists = f.isDirectory()
                                         && f.getChildFile ("project.xml").existsAsFile();
                    const juce::String label = f.getFileName()
                        + (exists ? juce::String() : juce::String (" (missing)"));
                    recent.addItem (130 + idx, label, exists);
                    ++idx;
                    if (idx >= 10) break;
                }
                if (list.isEmpty())
                    recent.addItem (1, "(no recent projects)", false);
                else
                {
                    recent.addSeparator();
                    recent.addItem (140, "Clear Recent Projects");
                }
            }
            m.addSubMenu ("Open Recent", recent, mProjectManager != nullptr);
        }
        m.addSeparator();
        m.addItem(104, "Save  (Ctrl+S)");
        m.addItem(105, "Save As...  (Shift+Ctrl+S)");
        // 2026-04-26: Save as Preset moved out — the per-engine preset
        // pickers handle that.  Save as Template lands in this slot since
        // it's the project-level save-as cousin (kit + 8 layers + 4 basses).
        m.addItem(106, "Save as Template...");
        m.addItem(109, "Load Template...");
        m.addSeparator();
        m.addItem(108, "Restore from Backup...");
        m.addSeparator();
        m.addItem(107, "Import Audio...");
        m.addSeparator();
        {
            juce::PopupMenu exportSub;
            exportSub.addItem(120, "Export as WAV...");
            exportSub.addItem(121, "Export as MP3...");
            m.addSubMenu("Export", exportSub);
        }
        break;

    case 1: // Edit
        m.addItem(201, "Undo  (Ctrl+Z)",    mUndoManager.canUndo());
        m.addItem(202, "Redo  (Ctrl+Alt+Z)", mUndoManager.canRedo());
        m.addItem(203, "History...");
        m.addSeparator();
        m.addItem(204, "New Layers Tab");
        m.addItem(205, "New Bass Tab");
        m.addItem(206, "New Drums Tab", false);  // Drums is permanent
        m.addSeparator();
        m.addItem(207, "New Automation Clip");
        break;

    case 2: // Patterns
        m.addItem(301, "Find Next Empty  (F4)");
        m.addItem(302, "Rename / Color  (F2)");
        m.addSeparator();
        m.addItem(303, "Insert One  (Shift+Ctrl+Ins)");
        m.addItem(304, "Clone  (Alt+C)");
        m.addItem(305, "Delete  (Del)");
        m.addSeparator();
        m.addItem(306, "Move Up  (Shift+Ctrl+Up)");
        m.addItem(307, "Move Down  (Shift+Ctrl+Down)");
        break;

    case 3: // View — Phase B-1 keymap (2026-04-26): F-keys reassigned per spreadsheet.
        m.addItem(405, "Mixer  (F5)");
        m.addItem(406, "Effects  (F6)");
        m.addItem(404, "Builder  (F7)");
        m.addItem(401, "Layers  (F8)");
        m.addItem(402, "Bass  (F9)");
        m.addItem(403, "Drums  (F10)");
        m.addItem(407, "Piano Roll  (F11)");
        break;

    case 4: // Options
        // P6: General submenu hosts default-template management.
        {
            juce::PopupMenu generalSub;
            if (mProjectManager && mProjectManager->getDefaultTemplate() != juce::File())
            {
                generalSub.addItem (530, "Set Default Template... (current: "
                                          + mProjectManager->getDefaultTemplate().getFileName()
                                          + ")");
                generalSub.addItem (531, "Clear Default Template");
            }
            else
            {
                generalSub.addItem (530, "Set Default Template...");
                generalSub.addItem (531, "Clear Default Template", false);
            }
            m.addSubMenu ("General", generalSub);
        }
        m.addItem(502, "File Settings...");
        m.addItem(503, "Audio Settings...");
        m.addSeparator();
        {
            juce::PopupMenu undoSub;
            undoSub.addItem(510, "100  steps",  true, mUndoHistorySize == 100);
            undoSub.addItem(511, "250  steps",  true, mUndoHistorySize == 250);
            undoSub.addItem(512, "500  steps",  true, mUndoHistorySize == 500);
            undoSub.addItem(513, "1000 steps",  true, mUndoHistorySize == 1000);
            m.addSubMenu("Undo History Size", undoSub);
        }
        m.addSeparator();
        m.addItem(520, "MIDI is Omni (all devices)  -  Read Only",  false);
        break;

    case 5: // Help
        m.addItem(601, "Help Index  (F1)");
        m.addItem(603, "Key Binds...");
        m.addSeparator();
        m.addItem(604, "BaySickNAM/IR Test Layout...");   // G-1.5 temporary
        m.addSeparator();
        m.addItem(602, "About BaySickDAW v1.9");
        break;
    }

    return m;
}

void StandaloneEditor::menuItemSelected(int id, int)
{
    switch (id)
    {
    // File — Project persistence (P2+P3, 2026-04-23)
    case 101: doFileNew();     break;
    case 103: doFileOpen();    break;
    case 104: doFileSave();    break;
    case 105: doFileSaveAs();  break;
    case 106: saveTemplateAs(); break;                      // 2026-04-26
    case 107: if (mBuilderPage) mBuilderPage->doImportAudio(); break;
    case 108: doFileRestoreBackup(); break;
    case 109:                                                // 2026-04-26 Load Template
    {
        // Anchor on the menu bar — close enough for the popup.
        auto anchor = mMenuBar.get();
        if (anchor != nullptr) showTemplateMenu (anchor);
        break;
    }
    case 102: doFileNewFromTemplate();    break;
    case 530: doFileSetDefaultTemplate(); break;
    case 531:
        if (mProjectManager) { mProjectManager->clearDefaultTemplate(); }
        break;
    case 140: // Clear Recent
        if (mProjectManager) mProjectManager->clearRecentProjects();
        break;
    // Recent Projects items (130..139) — open indexed project.  Explicit
    // cases rather than a mid-switch default (there's already a `default:`
    // at the end of the switch).
    case 130: case 131: case 132: case 133: case 134:
    case 135: case 136: case 137: case 138: case 139:
        if (mProjectManager)
        {
            const auto list = mProjectManager->getRecentProjects();
            const int idx = id - 130;
            if (idx < list.size())
            {
                const auto target = list[idx];
                // P5: dirty prompt first, then open.
                confirmDiscardChanges ([this, target]
                {
                    // 2026-04-24: wipe in-memory state before loading a
                    // different project so residual tabs / rack effects /
                    // etc. from the prior session don't bleed through.
                    closeAllDynamicTabs();
                    if (mMixerPage) mMixerPage->clearDynamicStrips();
                    mProcessor.resetToBlankState();
                    if (! mProjectManager->openProject (target))
                    {
                        juce::AlertWindow::showMessageBoxAsync (
                            juce::MessageBoxIconType::WarningIcon,
                            "Could not open project",
                            "The project folder may have been moved or deleted.");
                        return;
                    }
                    restoreAudioStripsFromArrangement();
                    refreshWindowTitle();
                });
            }
        }
        break;

    // Edit
    case 201: globalUndo();        break;
    case 202: globalRedo();        break;
    case 203: showHistoryWindow(); break;
    case 204: onAddTabRequest(RibbonTabBar::TabType::Layers);  break;
    case 205: onAddTabRequest(RibbonTabBar::TabType::Bass);    break;
    case 206: onAddTabRequest(RibbonTabBar::TabType::Drums);   break;
    case 207: if (mBuilderPage) mBuilderPage->doNewAutomationClip(); break;

    // View shortcuts
    case 401: handleCommandMessage(0); break; // Layers
    case 402: handleCommandMessage(1); break; // Bass
    case 403: handleCommandMessage(2); break; // Drums
    case 404: handleCommandMessage(3); break; // Builder
    case 405: // Mixer
        for (auto* e : mPages)
            if (e->type == RibbonTabBar::TabType::Mixer)
            { mRibbon->selectTab(e->ribbonTabId); onTabSelected(e->ribbonTabId); break; }
        break;
    case 406: // Effects
        for (auto* e : mPages)
            if (e->type == RibbonTabBar::TabType::Effects)
            { mRibbon->selectTab(e->ribbonTabId); onTabSelected(e->ribbonTabId); break; }
        break;
    case 407: showLastUsedPianoRoll(); break;   // Phase B-1: Piano Roll (F11)

    // Undo history size — also cap the label list
    case 510: mUndoHistorySize = 100;  mUndoManager.setMaxNumberOfStoredUnits(100,  30);
              while ((int)mHistoryLabels.size() > 100)  mHistoryLabels.pop_front();
              mHistoryCursor = juce::jmin(mHistoryCursor, 100);  break;
    case 511: mUndoHistorySize = 250;  mUndoManager.setMaxNumberOfStoredUnits(250,  30);
              while ((int)mHistoryLabels.size() > 250)  mHistoryLabels.pop_front();
              mHistoryCursor = juce::jmin(mHistoryCursor, 250);  break;
    case 512: mUndoHistorySize = 500;  mUndoManager.setMaxNumberOfStoredUnits(500,  30);
              while ((int)mHistoryLabels.size() > 500)  mHistoryLabels.pop_front();
              mHistoryCursor = juce::jmin(mHistoryCursor, 500);  break;
    case 513: mUndoHistorySize = 1000; mUndoManager.setMaxNumberOfStoredUnits(1000, 30);
              while ((int)mHistoryLabels.size() > 1000) mHistoryLabels.pop_front();
              mHistoryCursor = juce::jmin(mHistoryCursor, 1000); break;

    // Audio & MIDI Settings dialog — uses AudioSettingsDialog (safe Apply flow)
    case 503:
    {
        auto* dlg = new AudioSettingsDialog(mDeviceManager, mAudioCallback);

        juce::DialogWindow::LaunchOptions opts;
        opts.dialogTitle            = "Audio & MIDI Settings";
        opts.dialogBackgroundColour = VC::Bg;
        opts.content.setOwned(dlg);
        opts.resizable              = false;
        opts.useNativeTitleBar      = true;
        opts.launchAsync();
        break;
    }

    case 602:
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon, "BaySickDAW v1.9",
            "BaySickDAW v1.9 - Phase 1 build\n"
            "Built with JUCE 7  |  (c) KnowledgeBase Studios",
            "OK");
        break;

    case 603:   // Help > Key Binds...
        showKeyBindsWindow();
        break;

    case 604:   // Help > BaySickNAM/IR Test Layout...  (G-1.5 temporary)
        showNamIrTestWindow();
        break;

    default: break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Paint + Resize
// ─────────────────────────────────────────────────────────────────────────────
void StandaloneEditor::paint(juce::Graphics& g)
{
    g.fillAll(VC::Bg);

    // Subtle grid background
    g.setColour(VC::Accent.withAlpha(0.06f));
    for (int x = 0; x < getWidth();  x += 40) g.drawVerticalLine  (x, 0, (float)getHeight());
    for (int y = 0; y < getHeight(); y += 40) g.drawHorizontalLine(y, 0, (float)getWidth());

    // Menu bar panel background (combined toolbar paints its own brushed-aluminum bg)
    g.setColour(VC::Panel);
    g.fillRect(0, 0, getWidth(), 24);
}

void StandaloneEditor::paintOverChildren(juce::Graphics& /*g*/)
{
    // LRX-5 global lens vignette disabled 2026-04-21.
    //   Previously drew a radial gradient (center clear, 0.38 alpha at corners)
    //   over every page. JUCE's CPU renderer produced visible banding / ghost
    //   rings rather than a smooth fall-off. Reducing alpha to 0.10 did not
    //   resolve the banding, so the overlay is disabled until GL rendering is
    //   available. See blueprint T3-LRX5Vignette for the re-enable plan.
}

void StandaloneEditor::resized()
{
    auto b = getLocalBounds();

    static constexpr int kMenuH     = 24;
    static constexpr int kBarH      = 40;   // combined toolbar height
    static constexpr int kPageMenuH = PageMenuBar::kHeight;

    mMenuBar->setBounds(b.removeFromTop(kMenuH));

    // ── Combined toolbar (single 40px row) ────────────────────────────────────
    // mTransport spans full width:   left  → transport controls (▶⏸■ BPM TAP SONG METRO)
    //                                right → CPU/RAM readout
    //                                middle → empty (pattern + ribbon overlap on top)
    auto bar = b.removeFromTop(kBarH);
    mTransport->setBounds(bar);

    // Pattern dropdown button — single control replacing old ComboBox + Add button
    static constexpr int kCPUReserve = 120; // space kept clear on right for CPU label
    static constexpr int kPatBtnW    = 176; // 140 + 32 + 4 gap, same total footprint
    static constexpr int kPatStart   = GlobalTransportBar::kControlsWidth + 8;
    int py = bar.getY() + (kBarH - 28) / 2;
    mPatternBtn->setBounds(bar.getX() + kPatStart, py, kPatBtnW, 28);

    // Ribbon tabs: from end of pattern section to just before CPU readout
    int ribX = kPatStart + kPatBtnW + 8;
    int ribW = bar.getWidth() - ribX - kCPUReserve;
    if (ribW > 60)
        mRibbon->setBounds(bar.getX() + ribX, bar.getY(), ribW, kBarH);

    // ── Page menu bar + content ───────────────────────────────────────────────
    mPageMenuBar->setBounds(b.removeFromTop(kPageMenuH));
    for (auto* entry : mPages)
        if (entry->component) entry->component->setBounds(b);
    // G-2: Clips empty-state placeholder shares the same content area.
    if (mClipsEmptyState) mClipsEmptyState->setBounds(b);
}

// ─────────────────────────────────────────────────────────────────────────────
// Undo dispatch
// ─────────────────────────────────────────────────────────────────────────────
bool StandaloneEditor::doUndoAction(juce::UndoableAction* action,
                                    const juce::String&    label)
{
    // Trim any future (redo) labels from the cursor onward
    if (mHistoryCursor < (int)mHistoryLabels.size())
        mHistoryLabels.erase(mHistoryLabels.begin() + mHistoryCursor,
                             mHistoryLabels.end());

    mHistoryLabels.push_back(label);
    if ((int)mHistoryLabels.size() > mUndoHistorySize)
        mHistoryLabels.pop_front();
    mHistoryCursor = (int)mHistoryLabels.size();

    mUndoManager.beginNewTransaction(label);
    bool ok = mUndoManager.perform(action, label);

    // P5: undoable edits mark the project dirty.
    if (ok && mProjectManager) mProjectManager->markDirty();

    if (mHistoryWindow) mHistoryWindow->refresh();
    return ok;
}

void StandaloneEditor::globalUndo()
{
    // 2026-04-26: decrement unconditionally (matches the original behaviour
    // before my conditional-on-undo-return tweak which left the cursor stuck
    // at the end on the user's machine).  Cursor + UndoManager can drift on
    // no-op undos, but the user-visible cursor movement matters more.
    if (mHistoryCursor > 0) --mHistoryCursor;
    mUndoManager.undo();
    if (mProjectManager) mProjectManager->markDirty();
    if (mHistoryWindow) mHistoryWindow->refresh();
}

void StandaloneEditor::globalRedo()
{
    if (mHistoryCursor < (int)mHistoryLabels.size()) ++mHistoryCursor;
    mUndoManager.redo();
    if (mProjectManager) mProjectManager->markDirty();
    if (mHistoryWindow) mHistoryWindow->refresh();
}

void StandaloneEditor::showHistoryWindow()
{
    if (!mHistoryWindow)
    {
        mHistoryWindow = std::make_unique<UndoHistoryWindow>(
            mUndoManager,
            mHistoryLabels,
            mHistoryCursor,
            [this] { globalUndo(); },
            [this] { globalRedo(); });

        // 2026-04-26: Ctrl+Z / Ctrl+Alt+Z still route through the command
        // manager when this window has focus.  Register the editor's
        // KeyPressMappingSet as a key listener on the new window so its
        // dispatch is identical to the main app's.
        if (auto* set = mCmdMgr.getKeyMappings())
            mHistoryWindow->addKeyListener (set);
    }
    mHistoryWindow->setVisible(true);
    mHistoryWindow->toFront(true);
}

UndoContext StandaloneEditor::makeUndoContext()
{
    UndoContext ctx;
    ctx.manager = &mUndoManager;
    ctx.perform  = [this](juce::UndoableAction* a, const juce::String& l)
    {
        return doUndoAction(a, l);
    };
    ctx.undo        = [this] { globalUndo(); };
    ctx.redo        = [this] { globalRedo(); };
    ctx.showHistory = [this] { showHistoryWindow(); };
    return ctx;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Project persistence — File menu handlers (P2, 2026-04-23)
// ═══════════════════════════════════════════════════════════════════════════════
// All flows use `juce::AlertWindow` with async LaunchOptions so the audio
// thread never blocks waiting on UI.  Errors surface as async message boxes.
// Recent / Open browser UX comes in P3; here Open uses a native file picker
// pointed at the default Projects root as a stopgap.
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    // Show an AlertWindow with a single text input.  onAccept receives the
    // entered string (already passed through ProjectManager::sanitizeProjectName);
    // onCancel fires on cancel or empty-name submit.  Retry-until-valid is
    // handled by the caller by re-invoking this function from onAccept.
    void promptForProjectName (const juce::String& title,
                                const juce::String& message,
                                const juce::String& defaultText,
                                std::function<void(juce::String)> onAccept,
                                std::function<void()> onCancel)
    {
        auto* aw = new juce::AlertWindow (title, message,
                                           juce::MessageBoxIconType::QuestionIcon);
        aw->addTextEditor ("projectName", defaultText, {});
        aw->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true,
            juce::ModalCallbackFunction::create (
                [aw, onAccept = std::move (onAccept),
                     onCancel = std::move (onCancel)] (int result)
                {
                    if (result == 1)
                    {
                        const auto raw = aw->getTextEditorContents ("projectName");
                        const auto sanitized = ProjectManager::sanitizeProjectName (raw);
                        if (onAccept) onAccept (sanitized);
                    }
                    else
                    {
                        if (onCancel) onCancel();
                    }
                    delete aw;
                }),
            false);
    }
}

void StandaloneEditor::confirmDiscardChanges (std::function<void()> continuation)
{
    if (! mProjectManager || ! mProjectManager->hasProject() || ! mProjectManager->isDirty())
    {
        if (continuation) continuation();
        return;
    }
    juce::AlertWindow::showYesNoCancelBox (
        juce::MessageBoxIconType::WarningIcon,
        "Unsaved changes",
        "Save changes to '" + mProjectManager->getCurrentName() + "' first?",
        "Save",
        "Don't Save",
        "Cancel",
        this,
        juce::ModalCallbackFunction::create (
            [this, continuation = std::move (continuation)] (int result)
            {
                // Result: 1=Save, 2=Don't Save, 0=Cancel.
                if (result == 0) return;   // cancel - abort continuation
                if (result == 1)
                {
                    if (! mProjectManager->saveProject())
                    {
                        juce::AlertWindow::showMessageBoxAsync (
                            juce::MessageBoxIconType::WarningIcon,
                            "Save failed",
                            "Couldn't write project.xml.  Aborting.");
                        return;
                    }
                }
                if (continuation) continuation();
            }));
}

bool StandaloneEditor::requestAppQuit()
{
    if (! mProjectManager || ! mProjectManager->hasProject() || ! mProjectManager->isDirty())
        return true;   // nothing to prompt about - proceed synchronously
    confirmDiscardChanges ([] { juce::JUCEApplication::getInstance()->quit(); });
    return false;
}

void StandaloneEditor::doFileNew()
{
    // P5: prompt Save/Don't Save/Cancel if current project is dirty.
    confirmDiscardChanges ([this]
    {
    promptForProjectName (
        "New Project",
        "Name this project.  A folder will be created at:\n"
        + ProjectManager::getDefaultProjectsRoot().getFullPathName()
        + "\\<name>\\\n\n"
          "You can rename or move it later.",
        "Untitled Project",
        [this] (juce::String name)
        {
            if (! ProjectManager::isValidProjectName (name))
            {
                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon,
                    "Invalid project name",
                    "Project names can't contain < > : \" / \\ | ? * or be\n"
                    "reserved device names (CON, PRN, AUX, NUL, COM1-9,\n"
                    "LPT1-9).  Try another name.",
                    "OK",
                    this,
                    juce::ModalCallbackFunction::create (
                        [this] (int) { doFileNew(); }));
                return;
            }
            // 2026-04-26: default template is now an XML file (kit + 8 layers
            // + 4 basses), not a project folder.  Always create a blank
            // project, then apply the template via loadTemplate().
            const auto tpl = mProjectManager->getDefaultTemplate();
            if (! mProjectManager->newProject (name, juce::File()))
            {
                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon,
                    "Could not create project",
                    "Check that the Projects folder is writable and try again.");
                return;
            }
            // 2026-04-24 File > New reset: wipe in-memory state from prior
            // project before applying any template content.
            closeAllDynamicTabs();
            if (mMixerPage) mMixerPage->clearDynamicStrips();
            mProcessor.resetToBlankState();

            // Apply template XML if set — populates kit + layers + basses.
            if (tpl.existsAsFile() && tpl.hasFileExtension ("xml"))
                loadTemplate (tpl);

            // Rebuild the three default Layers / Bass / Drums tabs.  When we
            // seeded from a template, deserializeUIState already added the
            // template's tabs (and closeAllDynamicTabs inside that call
            // cleared our stub); mPages.isEmpty() check guards against
            // double-adding.
            bool needDefaults = true;
            for (auto& e : mPages)
            {
                if (e->type == RibbonTabBar::TabType::Layers
                    || e->type == RibbonTabBar::TabType::Bass
                    || e->type == RibbonTabBar::TabType::Drums)
                { needDefaults = false; break; }
            }
            if (needDefaults)
            {
                addDefaultDynamicTabs();
                if (mRibbon) { mRibbon->selectTab (3); }
                onTabSelected (3);   // land on Builder
            }

            restoreAudioStripsFromArrangement();
            mProjectManager->saveProject();
            refreshWindowTitle();
        });
    });   // close confirmDiscardChanges continuation
}

void StandaloneEditor::doFileNewFromTemplate()
{
    // Pick any existing project as a one-shot template (independent of the
    // default-template setting).  Uses the Project Browser - on selection,
    // immediately prompts for the new project's name.
    confirmDiscardChanges ([this]
    {
        auto* browser = new ProjectBrowserWindow();
        browser->isCurrentProject = [this] (const juce::File& f)
        {
            return mProjectManager && mProjectManager->isCurrentProject (f);
        };
        browser->onOpenSelected = [this, browser] (const juce::File& tpl)
        {
            if (auto* w = browser->findParentComponentOfClass<juce::DialogWindow>())
                w->exitModalState (1);

            promptForProjectName (
                "New Project From Template",
                "Naming the new project (will copy '"
                + tpl.getFileName() + "' as a starting point):",
                "Untitled Project",
                [this, tpl] (juce::String name)
                {
                    if (! ProjectManager::isValidProjectName (name)) return;
                    if (! mProjectManager->newProject (name, tpl)) return;
                    auto seedXml = mProjectManager->getCurrentFolder()
                                        .getChildFile ("project.xml");
                    if (seedXml.existsAsFile())
                        if (auto parsed = juce::XmlDocument::parse (seedXml))
                            mProcessor.deserializeProject (*parsed);
                    restoreAudioStripsFromArrangement();
                    mProjectManager->saveProject();
                    refreshWindowTitle();
                });
        };
        browser->onNewProject = [browser]
        {
            if (auto* w = browser->findParentComponentOfClass<juce::DialogWindow>())
                w->exitModalState (0);
        };

        juce::DialogWindow::LaunchOptions opts;
        opts.dialogTitle            = "Pick Template Project";
        opts.dialogBackgroundColour = juce::Colours::black;
        opts.content.setOwned (browser);
        opts.escapeKeyTriggersCloseButton = true;
        opts.useNativeTitleBar      = true;
        opts.resizable              = true;
        opts.launchAsync();
    });
}

void StandaloneEditor::doFileSetDefaultTemplate()
{
    // 2026-04-26: repointed from project-folder picker to the Templates
    // folder.  Templates are XML files (kit + 8 layers + 4 basses) generated
    // by gen_factory_presets.py, plus user-saved ones under My Templates/.
    auto initialDir = templatesDir();
    initialDir.createDirectory();   // ensure exists so the chooser opens cleanly

    mTemplateChooser = std::make_unique<juce::FileChooser> (
        "Pick Default Template", initialDir, "*.xml", true);
    const int flags = juce::FileBrowserComponent::openMode
                    | juce::FileBrowserComponent::canSelectFiles;
    mTemplateChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        const auto picked = fc.getResult();
        if (! picked.existsAsFile()) return;
        if (mProjectManager) mProjectManager->setDefaultTemplate (picked);
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon,
            "Default Template Set",
            "Future 'New Project' will start from '" + picked.getFileNameWithoutExtension()
            + "'.  Use Options > Clear Default Template to undo.");
    });
    return;

    // (Old ProjectBrowserWindow path retired — no longer reachable.)
    auto* browser = new ProjectBrowserWindow();
    browser->isCurrentProject = [this] (const juce::File& f)
    {
        return mProjectManager && mProjectManager->isCurrentProject (f);
    };
    browser->onOpenSelected = [this, browser] (const juce::File& folder)
    {
        if (mProjectManager) mProjectManager->setDefaultTemplate (folder);
        if (auto* w = browser->findParentComponentOfClass<juce::DialogWindow>())
            w->exitModalState (1);
    };
    browser->onNewProject = [browser]
    {
        if (auto* w = browser->findParentComponentOfClass<juce::DialogWindow>())
            w->exitModalState (0);
    };

    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle            = "Pick Default Template";
    opts.dialogBackgroundColour = juce::Colours::black;
    opts.content.setOwned (browser);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar      = true;
    opts.resizable              = true;
    opts.launchAsync();
}

void StandaloneEditor::doFileOpen()
{
    // P5: prompt Save/Don't Save/Cancel first if current project is dirty.
    confirmDiscardChanges ([this]
    {
    // P3: custom Project Browser window.  Lists subfolders of the projects
    // root, sortable by name/modified/size, with right-click menu for
    // Rename / Duplicate / Delete / Show in Explorer.
    auto* browser = new ProjectBrowserWindow();

    browser->isCurrentProject = [this] (const juce::File& f)
    {
        return mProjectManager && mProjectManager->isCurrentProject (f);
    };

    browser->onOpenSelected = [this, browser] (const juce::File& folder)
    {
        // 2026-04-24: wipe before load (see Open Recent path for details).
        closeAllDynamicTabs();
        if (mMixerPage) mMixerPage->clearDynamicStrips();
        mProcessor.resetToBlankState();
        if (! mProjectManager->openProject (folder))
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Could not open project",
                "That folder doesn't contain a project.xml, or the file is corrupt.");
            return;
        }
        restoreAudioStripsFromArrangement();
        refreshWindowTitle();
        if (auto* w = browser->findParentComponentOfClass<juce::DialogWindow>())
            w->exitModalState (1);
    };

    browser->onNewProject = [this, browser]
    {
        if (auto* w = browser->findParentComponentOfClass<juce::DialogWindow>())
            w->exitModalState (0);
        doFileNew();
    };

    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle            = "Open Project";
    opts.dialogBackgroundColour = juce::Colours::black;
    opts.content.setOwned (browser);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar      = true;
    opts.resizable              = true;
    opts.launchAsync();
    });   // close confirmDiscardChanges continuation
}

void StandaloneEditor::doFileSave()
{
    if (! mProjectManager->hasProject())
    {
        // No project yet — Save behaves like Save As.  User sees the naming
        // prompt once, then subsequent saves overwrite silently.
        doFileSaveAs();
        return;
    }
    if (! mProjectManager->saveProject())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Save failed",
            "Couldn't write project.xml.  Check the Projects folder is writable.");
    }
    refreshWindowTitle();
}

void StandaloneEditor::doFileSaveAs()
{
    const juce::String defaultName = mProjectManager->hasProject()
                                       ? mProjectManager->getCurrentName()
                                       : juce::String ("Untitled Project");
    promptForProjectName (
        "Save Project As",
        "Save a copy of this project under a new name.",
        defaultName,
        [this] (juce::String name)
        {
            if (! ProjectManager::isValidProjectName (name))
            {
                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon,
                    "Invalid project name",
                    "Try another name (no < > : \" / \\ | ? * or reserved names).",
                    "OK",
                    this,
                    juce::ModalCallbackFunction::create (
                        [this] (int) { doFileSaveAs(); }));
                return;
            }
            if (! mProjectManager->saveProjectAs (name))
            {
                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon,
                    "Save As failed",
                    "Couldn't create the new project folder.  Check that the\n"
                    "Projects folder is writable.");
                return;
            }
            refreshWindowTitle();
        });
}

void StandaloneEditor::doFileRestoreBackup()
{
    if (! mProjectManager) return;
    const auto backups = mProjectManager->listBackups();
    if (backups.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon,
            "No backups available",
            mProjectManager->hasProject()
                ? "No backups exist yet for this project.  Autosave writes one\n"
                  "every 15 minutes once the app is running."
                : "No unsaved-session backups exist.  Autosave starts writing\n"
                  "15 minutes after launch.");
        return;
    }

    // Build a popup menu listing the backups, newest first.
    juce::PopupMenu m;
    m.addSectionHeader (mProjectManager->hasProject()
                          ? "Backups for " + mProjectManager->getCurrentName()
                          : juce::String ("Unsaved-session backups"));
    const auto now = juce::Time::getCurrentTime();
    for (int i = 0; i < backups.size(); ++i)
    {
        const auto t = backups[i].getLastModificationTime();
        const auto deltaMin = (now.toMilliseconds() - t.toMilliseconds()) / 60000;
        juce::String age;
        if      (deltaMin <  1)    age = "just now";
        else if (deltaMin < 60)    age = juce::String (deltaMin) + " min ago";
        else if (deltaMin < 1440)  age = juce::String (deltaMin / 60) + " hr ago";
        else                       age = juce::String (deltaMin / 1440) + " days ago";
        m.addItem (1000 + i, t.formatted ("%Y-%m-%d %H:%M") + "   (" + age + ")");
    }

    m.showMenuAsync (juce::PopupMenu::Options(),
        [this, backups] (int chosen)
        {
            if (chosen < 1000) return;
            const int idx = chosen - 1000;
            if (idx < 0 || idx >= backups.size()) return;
            const auto file = backups[idx];

            const juce::String warning =
                "Restore '" + file.getFileName() + "'?\n\n"
                "Current unsaved changes will be replaced.\n\n"
                "Note: any audio samples or other content that was deleted\n"
                "from the original project folder since this backup was made\n"
                "will be missing from the restored project.";

            juce::AlertWindow::showOkCancelBox (
                juce::MessageBoxIconType::WarningIcon,
                "Restore from Backup",
                warning,
                "Restore",
                "Cancel",
                this,
                juce::ModalCallbackFunction::create (
                    [this, file] (int result)
                    {
                        if (result != 1) return;
                        // 2026-04-24: wipe before restore.
                        closeAllDynamicTabs();
                        if (mMixerPage) mMixerPage->clearDynamicStrips();
                        mProcessor.resetToBlankState();
                        if (! mProjectManager->restoreBackup (file))
                        {
                            juce::AlertWindow::showMessageBoxAsync (
                                juce::MessageBoxIconType::WarningIcon,
                                "Restore failed",
                                "Couldn't read the backup file.  It may be corrupt.");
                            return;
                        }
                        restoreAudioStripsFromArrangement();
                        refreshWindowTitle();
                    }));
        });
}

bool StandaloneEditor::promptCreateProject (const juce::String& reasonExplanation)
{
    // Synchronous-looking API but the dialog is actually async; caller should
    // re-check hasProject() in its continuation.  Used by P4 (Builder audio
    // drop) to prompt before the drop proceeds.
    juce::ignoreUnused (reasonExplanation);
    doFileNew();
    return mProjectManager->hasProject();
}

// ── P1+P2 persistence (2026-04-24): tab + engine save / load ────────────────
//
// Saves every Layers / Bass / Drums tab as a <Tab> element inside <UIState>
// under the project root.  Each record carries: type, pageIndex, tab name,
// engine type (empty if the user never picked one), and a base64 blob of the
// engine processor's own getStateInformation (so ALL engine knobs /
// modulations / sample paths / Harmless partials come back verbatim).
// ─────────────────────────────────────────────────────────────────────────────
void StandaloneEditor::serializeUIState (juce::XmlElement& root)
{
    auto* ui = root.createNewChildElement ("UIState");
    ui->setAttribute ("version", 1);
    auto* tabs = ui->createNewChildElement ("Tabs");

    auto encodeEngineState = [](juce::AudioProcessor* eng) -> juce::String
    {
        if (eng == nullptr) return {};
        juce::MemoryBlock mb;
        eng->getStateInformation (mb);
        return mb.toBase64Encoding();
    };

    // P4 persistence: active ribbon tab, mixer scroll, arrangement view/sel.
    if (mRibbon)
        ui->setAttribute ("activeTabId", mRibbon->getSelectedTabId());
    if (mMixerPage)
        ui->setAttribute ("mixerScrollX", mMixerPage->getScrollX());

    // 2026-04-24 Cycle 2 persistence additions:
    //   <Metronome>    volume / sound type / enabled toggle / count-in bars
    //   <VUCalibration> global VUMeter::sCalibrationDb (was session-only)
    //   <SongLoop>     transport's play-through-vs-loop toggle
    //   <AuxNames>     per-aux-strip custom names (user-renamed aux channels)
    {
        auto* metro = ui->createNewChildElement ("Metronome");
        metro->setAttribute ("volume",      (double) mProcessor.mMetro.volume    .load (std::memory_order_relaxed));
        metro->setAttribute ("soundType",   (int)    mProcessor.mMetro.soundType .load (std::memory_order_relaxed));
        metro->setAttribute ("enabled",     mProcessor.mMetro.enabled.load (std::memory_order_relaxed) ? 1 : 0);
        metro->setAttribute ("precountEnabled", mPrecountEnabled ? 1 : 0);   // D-5
    }
    {
        auto* vu = ui->createNewChildElement ("VUCalibration");
        vu->setAttribute ("dbfs", (double) VUMeter::getCalibrationDb());
    }
    if (mTransport)
    {
        auto* sl = ui->createNewChildElement ("SongLoop");
        sl->setAttribute ("on", mTransport->isSongLoopMode() ? 1 : 0);
    }
    if (mMixerPage)
    {
        auto writeStripNames = [&](const char* tag, const juce::String& defaultPrefix,
                                     const std::vector<int>& indices,
                                     std::function<juce::String(int)> getName)
        {
            auto* list = ui->createNewChildElement (tag);
            for (int idx : indices)
            {
                const auto name = getName (idx);
                const auto defaultName = defaultPrefix + juce::String (idx + 1);
                if (name.isNotEmpty() && name != defaultName)
                {
                    auto* rec = list->createNewChildElement ("Name");
                    rec->setAttribute ("idx",   idx);
                    rec->setAttribute ("value", name);
                }
            }
        };
        writeStripNames ("AuxNames",  "Aux ",  mMixerPage->getAuxStripIndices(),
                         [this](int i) { return mMixerPage->getAuxStripName (i); });
        writeStripNames ("VoxNames",  "Vox ",  mMixerPage->getVoxStripIndices(),
                         [this](int i) { return mMixerPage->getVoxStripName (i); });
        writeStripNames ("InstNames", "Inst ", mMixerPage->getInstStripIndices(),
                         [this](int i) { return mMixerPage->getInstStripName (i); });
    }
    if (mBuilderPage)
    {
        if (auto* grid = mBuilderPage->getGrid())
        {
            auto* arr = ui->createNewChildElement ("Arrangement");
            arr->setAttribute ("ppBar",   grid->mPPBar);
            arr->setAttribute ("barOff",  grid->mBarOff);
            if (grid->hasTimeSelection())
            {
                arr->setAttribute ("selStart", grid->getTimeSelStart());
                arr->setAttribute ("selEnd",   grid->getTimeSelEnd());
            }
        }
    }

    // 2026-04-26 (step 2 polish): persist the unified Piano Roll page's
    // active engine selection per-project so the user lands back on the
    // engine they were last editing when reopening this project.
    if (mPianoRollPage)
    {
        auto* prSel = ui->createNewChildElement ("PianoRollSelection");
        const auto id = mPianoRollPage->getActiveEngineId();
        const char* kindStr = "DrumKit";
        switch (id.kind)
        {
            case EngineKind::Layer:   kindStr = "Layer";   break;
            case EngineKind::Bass:    kindStr = "Bass";    break;
            case EngineKind::Drum:    kindStr = "Drum";    break;
            case EngineKind::DrumKit: kindStr = "DrumKit"; break;
        }
        prSel->setAttribute ("kind",  kindStr);
        prSel->setAttribute ("index", id.index);
    }

    for (auto& e : mPages)
    {
        juce::XmlElement* rec = nullptr;
        if (auto* lp = dynamic_cast<LayersPage*> (e->component.get()))
        {
            rec = tabs->createNewChildElement ("Tab");
            rec->setAttribute ("type",       "Layers");
            rec->setAttribute ("pageIndex",  lp->getPageIndex());
            rec->setAttribute ("name",       lp->getTabName());
            rec->setAttribute ("engine",     lp->getEngineType());
            rec->setAttribute ("engineData", encodeEngineState (lp->getEngineProcessor()));
        }
        else if (auto* bp = dynamic_cast<BassPage*> (e->component.get()))
        {
            rec = tabs->createNewChildElement ("Tab");
            rec->setAttribute ("type",       "Bass");
            rec->setAttribute ("pageIndex",  bp->getPageIndex());
            rec->setAttribute ("name",       bp->getTabName());
            rec->setAttribute ("engine",     bp->getEngineType());
            rec->setAttribute ("engineData", encodeEngineState (bp->getEngineProcessor()));
        }
        else if (auto* dp = dynamic_cast<DrumPage*> (e->component.get()))
        {
            // D1.4: dynamic-drum tab (new model — one engine per drum).
            // Legacy "Drums" type (16-slot kit) emit removed 2026-04-25.
            rec = tabs->createNewChildElement ("Tab");
            rec->setAttribute ("type",       "Drum");
            rec->setAttribute ("pageIndex",  dp->getPageIndex());
            rec->setAttribute ("name",       dp->getTabName());
            rec->setAttribute ("engine",     dp->getEngineType());
            rec->setAttribute ("engineData", encodeEngineState (dp->getEngineProcessor()));
        }
        juce::ignoreUnused (rec);
    }
}

// Close every Layers / Bass / Drums tab.  `RibbonTabBar::closeTab` refuses
// to remove Drums tabs OR the last instance of a type (correct for user
// clicks; wrong here), so we drive the ribbon wipe through the new
// `clearAllDynamicTabs` path + clean up mPages explicitly via onTabClosed.
void StandaloneEditor::closeAllDynamicTabs()
{
    // Collect ids first - onTabClosed mutates mPages.
    juce::Array<int> toClose;
    for (auto& e : mPages)
    {
        if (e->type == RibbonTabBar::TabType::Layers
            || e->type == RibbonTabBar::TabType::Bass
            || e->type == RibbonTabBar::TabType::Drums)
        {
            toClose.add (e->ribbonTabId);
        }
    }
    for (int id : toClose)
        onTabClosed (id);   // removes mPages entry + frees index slot
    if (mRibbon)
        mRibbon->clearAllDynamicTabs();   // unconditional ribbon wipe
}

void StandaloneEditor::deserializeUIState (const juce::XmlElement& root)
{
    auto* ui = root.getChildByName ("UIState");
    if (ui == nullptr) return;
    auto* tabs = ui->getChildByName ("Tabs");
    if (tabs == nullptr) return;

    closeAllDynamicTabs();

    auto applyEngineState = [](juce::AudioProcessor* eng, const juce::String& base64)
    {
        if (eng == nullptr || base64.isEmpty()) return;
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (base64))
            eng->setStateInformation (mb.getData(), (int) mb.getSize());
    };

    for (auto* rec : tabs->getChildWithTagNameIterator ("Tab"))
    {
        const juce::String type      = rec->getStringAttribute ("type");
        const int          pageIndex = rec->getIntAttribute    ("pageIndex", 0);
        const juce::String name      = rec->getStringAttribute ("name");
        const juce::String engine    = rec->getStringAttribute ("engine");
        const juce::String engineData= rec->getStringAttribute ("engineData");

        if (type == "Layers")
        {
            auto page = createLayersPageAtIndex (pageIndex);
            if (! page) continue;
            auto* lp = dynamic_cast<LayersPage*> (page.get());
            const int newId = mRibbon->addTab (RibbonTabBar::TabType::Layers,
                                                name.isNotEmpty() ? name : "Layers");
            if (lp && name.isNotEmpty()) lp->setTabName (name);
            lp->onEngineSelected = [this, newId, pageIndex] {
                const auto* tab = mRibbon->getTabById (newId);
                if (mMixerPage)   mMixerPage->addLayerChannel (pageIndex, tab ? tab->name : "Layers");
                if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
            };
            lp->onDeleteRequested = [this, newId] {
                if (! mRibbon) return;
                if (mRibbon->isLastOfType (RibbonTabBar::TabType::Layers))
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                        "Cannot Delete",
                        "This is the only Layer tab. Add another first.");
                    return;
                }
                mRibbon->closeTab (newId);
            };
            lp->onDuplicateRequested = [this] (const juce::String& clipboardXml) {
                spawnDuplicateLayerTab (clipboardXml);
            };
            lp->onLockChanged = [this, newId, lp] {
                if (mRibbon) mRibbon->setTabLocked (newId, lp->isLocked());
            };
            lp->onRenameRequested = [this, newId] {
                if (mRibbon) mRibbon->startRename (newId);
            };
            lp->onSoundNameChanged = [this, newId, pageIndex] (const juce::String& nm) {
                if (nm.isEmpty()) return;
                if (mRibbon)    mRibbon->renameTab (newId, nm);
                if (mMixerPage) mMixerPage->renameChannel (pageIndex, nm);
                if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Layer, pageIndex }, nm);
            };
            if (lp) registerLayerPianoRoll (lp);
            if (lp && engine.isNotEmpty())
                lp->selectEngine (engine);
            applyEngineState (lp ? lp->getEngineProcessor() : nullptr, engineData);

            auto* entry = new PageEntry();
            entry->ribbonTabId = newId;
            entry->type        = RibbonTabBar::TabType::Layers;
            entry->component   = std::move (page);
            addChildComponent (*entry->component);
            mPages.add (entry);
        }
        else if (type == "Bass")
        {
            auto page = createBassPageAtIndex (pageIndex);
            if (! page) continue;
            auto* bp = dynamic_cast<BassPage*> (page.get());
            const int newId = mRibbon->addTab (RibbonTabBar::TabType::Bass,
                                                name.isNotEmpty() ? name : "Bass");
            if (bp && name.isNotEmpty()) bp->setTabName (name);
            bp->onEngineSelected = [this, newId, pageIndex] {
                const auto* tab = mRibbon->getTabById (newId);
                if (mMixerPage)   mMixerPage->addBassChannel (pageIndex, tab ? tab->name : "Bass");
                if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
            };
            bp->onDeleteRequested = [this, newId] {
                if (! mRibbon) return;
                if (mRibbon->isLastOfType (RibbonTabBar::TabType::Bass))
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                        "Cannot Delete",
                        "This is the only Bass tab. Add another first.");
                    return;
                }
                mRibbon->closeTab (newId);
            };
            bp->onDuplicateRequested = [this] (const juce::String& clipboardXml) {
                spawnDuplicateBassTab (clipboardXml);
            };
            bp->onLockChanged = [this, newId, bp] {
                if (mRibbon) mRibbon->setTabLocked (newId, bp->isLocked());
            };
            bp->onRenameRequested = [this, newId] {
                if (mRibbon) mRibbon->startRename (newId);
            };
            bp->onSoundNameChanged = [this, newId, pageIndex] (const juce::String& nm) {
                if (nm.isEmpty()) return;
                if (mRibbon)    mRibbon->renameTab (newId, nm);
                if (mMixerPage) mMixerPage->renameChannel (pageIndex, nm);
                if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Bass, pageIndex }, nm);
            };
            if (bp) registerBassPianoRoll (bp);
            if (bp && engine.isNotEmpty())
                bp->selectEngine (engine);
            applyEngineState (bp ? bp->getEngineProcessor() : nullptr, engineData);

            auto* entry = new PageEntry();
            entry->ribbonTabId = newId;
            entry->type        = RibbonTabBar::TabType::Bass;
            entry->component   = std::move (page);
            addChildComponent (*entry->component);
            mPages.add (entry);
        }
        else if (type == "Drums")
        {
            // 2026-04-25: legacy 16-slot DrumsPage removed.  Old project files
            // that saved a "Drums" tab are silently skipped — the user can
            // re-add drums via the Drums dropdown ▾ in the new model.  Notes
            // from the legacy single drumRoll were already migrated into
            // drumRolls[slot] by PatternManager::fromValueTree (D1.1).
            continue;
        }
        else if (type == "Drum")
        {
            // D1.4: dynamic-drum tab restore.
            auto page = createDrumPageAtIndex (pageIndex);
            if (! page) continue;
            auto* dp2 = dynamic_cast<DrumPage*> (page.get());
            const int newId = mRibbon->addTab (RibbonTabBar::TabType::Drums,
                                                name.isNotEmpty() ? name : "Drums");
            if (dp2 && name.isNotEmpty()) dp2->setTabName (name);
            if (dp2)
            {
                dp2->onEngineSelected = [this, newId, pageIndex] {
                    const auto* tab = mRibbon->getTabById (newId);
                    if (mMixerPage)   mMixerPage->addDrumChannel (pageIndex, tab ? tab->name : "Drums");
                    if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
                    refreshAllKitViews();
                };
                dp2->onSoundNameChanged = [this, newId, pageIndex, dp2] (const juce::String& nm) {
                    if (nm.isEmpty()) return;
                    if (mRibbon)    mRibbon->renameTab (newId, nm);
                    if (mMixerPage) mMixerPage->renameChannel (pageIndex, nm);
                    dp2->setTabName (nm);
                    refreshAllKitViews();
                    if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Drum, pageIndex }, nm);
                };
                dp2->onDeleteRequested = [this, newId] {
                    if (! mRibbon) return;
                    if (mRibbon->isLastOfType (RibbonTabBar::TabType::Drums))
                    {
                        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                            "Cannot Delete",
                            "This is the only Drum tab. Add another first.");
                        return;
                    }
                    mRibbon->closeTab (newId);
                };
                dp2->onDuplicateRequested = [this] (const juce::String& clipboardXml) {
                    spawnDuplicateDrumTab (clipboardXml);
                };
                dp2->onLockChanged = [this, newId, dp2] {
                    if (mRibbon) mRibbon->setTabLocked (newId, dp2->isLocked());
                    refreshAllKitViews();
                };
                dp2->onRenameRequested = [this, newId] {
                    if (mRibbon) mRibbon->startRename (newId);
                };
                wireDrumPageKitView (dp2);
                registerDrumPianoRoll (dp2);
            }
            if (dp2 && engine.isNotEmpty())
                dp2->selectEngine (engine);
            applyEngineState (dp2 ? dp2->getEngineProcessor() : nullptr, engineData);

            auto* entry = new PageEntry();
            entry->ribbonTabId = newId;
            entry->type        = RibbonTabBar::TabType::Drums;
            entry->component   = std::move (page);
            addChildComponent (*entry->component);
            mPages.add (entry);
        }
    }

    if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
    refreshAllKitViews();   // D2: project load may have added drums

    // 2026-04-26 (step 2 polish): restore the unified Piano Roll page's
    // active engine selection.  Runs AFTER all engine pages have been
    // restored so registerEngine has populated the registry.  Falls back
    // to Drum Kit silently when the saved engine isn't valid (e.g. it
    // was deleted before save, or the saved index is out of range).
    if (mPianoRollPage)
    {
        if (auto* prSel = ui->getChildByName ("PianoRollSelection"))
        {
            const juce::String kindStr = prSel->getStringAttribute ("kind", "DrumKit");
            const int idx = prSel->getIntAttribute ("index", 0);
            EngineKind kind = EngineKind::DrumKit;
            if      (kindStr == "Layer") kind = EngineKind::Layer;
            else if (kindStr == "Bass")  kind = EngineKind::Bass;
            else if (kindStr == "Drum")  kind = EngineKind::Drum;
            mPianoRollPage->selectEngine ({ kind, idx });
        }
    }

    // P4: restore saved view state.  Apply AFTER tabs have been added so
    // pages exist in mPages for the onTabSelected / component lookups.
    if (auto* arr = ui->getChildByName ("Arrangement"))
    {
        if (mBuilderPage)
        {
            if (auto* grid = mBuilderPage->getGrid())
            {
                grid->mPPBar  = (float) arr->getDoubleAttribute ("ppBar",  grid->mPPBar);
                grid->mBarOff = (float) arr->getDoubleAttribute ("barOff", grid->mBarOff);
                if (arr->hasAttribute ("selStart") && arr->hasAttribute ("selEnd"))
                    grid->setTimeSelection (
                        (float) arr->getDoubleAttribute ("selStart"),
                        (float) arr->getDoubleAttribute ("selEnd"));
                grid->resized();
                grid->repaint();
            }
        }
    }
    if (mMixerPage && ui->hasAttribute ("mixerScrollX"))
        mMixerPage->setScrollX (ui->getIntAttribute ("mixerScrollX"));

    // 2026-04-24 Cycle 2 restore.
    if (auto* metro = ui->getChildByName ("Metronome"))
    {
        mProcessor.mMetro.volume   .store ((float) metro->getDoubleAttribute ("volume",    0.7),  std::memory_order_relaxed);
        mProcessor.mMetro.soundType.store (metro->getIntAttribute ("soundType", 0),               std::memory_order_relaxed);
        mProcessor.mMetro.enabled  .store (metro->getIntAttribute ("enabled",   0) != 0,          std::memory_order_relaxed);
        // D-5 (2026-04-26): legacy projects stored `countInBars` (0/1/2/4).
        // New schema is `precountEnabled` (bool).  Migrate: any non-zero
        // legacy value → on; pure-D-5 saves use the new attribute directly.
        if (metro->hasAttribute ("precountEnabled"))
            mPrecountEnabled = metro->getIntAttribute ("precountEnabled", 0) != 0;
        else if (metro->hasAttribute ("countInBars"))
            mPrecountEnabled = metro->getIntAttribute ("countInBars", 0) > 0;
    }
    if (auto* vu = ui->getChildByName ("VUCalibration"))
    {
        VUMeter::setCalibrationDb ((float) vu->getDoubleAttribute ("dbfs", -18.0));
    }
    if (auto* sl = ui->getChildByName ("SongLoop"))
    {
        const bool loopOn = sl->getIntAttribute ("on", 0) != 0;
        mProcessor.mSongLoopMode.store (loopOn, std::memory_order_relaxed);
        // Note: the transport's LoopModeButton syncs from mSongLoopMode on
        // its own timer, so no explicit setSongLoopMode call is needed here.
    }
    auto restoreStripNames = [this, ui](const char* tag,
                                         std::function<void(int)> ensureStrip,
                                         std::function<void(int, const juce::String&)> setName)
    {
        auto* list = ui->getChildByName (tag);
        if (list == nullptr || mMixerPage == nullptr) return;
        for (auto* rec : list->getChildWithTagNameIterator ("Name"))
        {
            const int  idx    = rec->getIntAttribute    ("idx",   -1);
            const auto value  = rec->getStringAttribute ("value");
            if (idx < 0 || value.isEmpty()) continue;
            ensureStrip (idx);    // idempotent
            setName (idx, value);
        }
    };
    restoreStripNames ("AuxNames",
                       [this](int i) { mMixerPage->addAuxChannelAtIndex  (i); },
                       [this](int i, const juce::String& n) { mMixerPage->setAuxStripName  (i, n); });
    restoreStripNames ("VoxNames",
                       [this](int i) { mMixerPage->addVoxChannelAtIndex  (i); },
                       [this](int i, const juce::String& n) { mMixerPage->setVoxStripName  (i, n); });
    restoreStripNames ("InstNames",
                       [this](int i) { mMixerPage->addInstChannelAtIndex (i); },
                       [this](int i, const juce::String& n) { mMixerPage->setInstStripName (i, n); });

    // Prefer the saved active tab, then the first dynamic tab, then Builder.
    int preferred = ui->getIntAttribute ("activeTabId", -1);
    const bool savedValid = (preferred > 0 && mRibbon
                              && mRibbon->getTabById (preferred) != nullptr);
    if (! savedValid)
    {
        preferred = -1;
        for (auto& e : mPages)
        {
            if (e->type == RibbonTabBar::TabType::Layers
                || e->type == RibbonTabBar::TabType::Bass
                || e->type == RibbonTabBar::TabType::Drums)
            {
                preferred = e->ribbonTabId;
                break;
            }
        }
        if (preferred < 0) preferred = 3;   // Builder
    }
    if (mRibbon) mRibbon->selectTab (preferred);
    onTabSelected (preferred);
}

// ── 2026-04-24: post-load tempo sync ────────────────────────────────────────
// Push the loaded project's global tempo into the playhead + transport.
// Called at the end of every project-load path.  Prevents the transport BPM
// field from showing 120 when the saved project was at a different tempo.
static void syncTempoFromPatternManager (StandalonePlayHead& ph,
                                          GlobalTransportBar* transport,
                                          PatternManager* pm)
{
    if (pm == nullptr) return;
    const double bpm = pm->getGlobalTempo();
    ph.setBPM (bpm);
    if (transport)
    {
        // Transport polls mPlayHead.getBPM() on its timer and writes to the
        // field when focus isn't held, so no explicit UI write needed here.
        juce::ignoreUnused (bpm, transport);
    }
}

// ── R5d follow-up (2026-04-24): post-load audio-strip restore ───────────────
void StandaloneEditor::restoreAudioStripsFromArrangement()
{
    if (mPM == nullptr) return;

    for (int i = 0; i < mPM->getNumBlocks(); ++i)
    {
        const auto& b = mPM->getBlock (i);
        if (b.clipType != ClipType::Audio) continue;
        const int row = b.trackRow;
        if (row < 0 || row >= VibeSynthProcessor::kMaxAudioRows) continue;

        const juce::String stripName =
            b.displayAlias.isNotEmpty() ? b.displayAlias
                                         : juce::String ("Audio ") + juce::String (row + 1);
        if (mMixerPage)
            mMixerPage->addAudioChannel (row, stripName);
        mProcessor.mVibeGraph.addAudioRowChannel (row, stripName);
        mProcessor.ensureAudioInsert (row, stripName);
    }
    if (mEffectsPage)
        mEffectsPage->rebuildChannelDropdown();
    mProcessor.rebuildAudioClipPlayers();

    // 2026-04-24: apply any stashed per-insert rack state now that every
    // InsertNode exists (Layer/Bass/Drum via deserializeUIState, Audio via
    // the loop above, Aux via restoreAuxStripsFromState already inside
    // deserializeProject).  deserializeProject runs loadRackStates once for
    // fixed-bus racks; this second apply reaches the InsertNodes that
    // weren't born yet at that point.
    mProcessor.applyPendingRackStates();

    // 2026-04-24: push the saved global tempo into the playhead now that the
    // full project has been restored.  Transport BPM field picks it up on
    // its next timer tick.
    syncTempoFromPatternManager (mPlayHead, mTransport.get(), mPM.get());
}

// ── R5d (2026-04-24): post-stop recording routing ───────────────────────────
void StandaloneEditor::commitRecordingResult (const VibeSynthProcessor::RecordResult& res)
{
    if (! mPM) return;

    // Drop each captured WAV as an Audio clip on the next free arrangement
    // row.  Start bar comes from the playhead position at record-start; length
    // in bars is ceil(file duration in beats / beats-per-bar).
    auto dropWavAsClip = [&](const juce::File& wavFile)
    {
        if (! wavFile.existsAsFile()) return;

        // Compute length in bars via file header (no re-read of audio data).
        double fileSeconds = 0.0;
        {
            juce::AudioFormatManager fmt;
            fmt.registerBasicFormats();
            if (auto reader = std::unique_ptr<juce::AudioFormatReader> (
                    fmt.createReaderFor (wavFile)))
            {
                if (reader->sampleRate > 0.0)
                    fileSeconds = (double) reader->lengthInSamples / reader->sampleRate;
            }
        }
        const double bpm        = juce::jmax (20.0, mTransport ? mTransport->getBPM() : 120.0);
        const double fileBeats  = fileSeconds * bpm / 60.0;
        constexpr double kBeatsPerBar = 4.0;
        const int lengthBars = juce::jmax (1, (int) std::ceil (fileBeats / kBeatsPerBar));
        const int startBar   = (int) std::floor (res.startBeat / kBeatsPerBar);

        // Find next free trackRow (scan existing blocks).
        int nextRow = 0;
        for (int i = 0; i < mPM->getNumBlocks(); ++i)
            nextRow = juce::jmax (nextRow, mPM->getBlock (i).trackRow + 1);

        ArrangementBlock block;
        block.clipType      = ClipType::Audio;
        block.trackRow      = nextRow;
        block.startBar      = startBar;
        block.lengthBars    = lengthBars;                           // ceil'd bar count (for bar-aligned UI)
        block.lengthBeats   = (float) fileBeats;                    // 2026-04-24: exact end so Song-end
                                                                     // + playback + loop match the take
        block.patternIndex  = mPM->getCurrentPatternIndex();
        block.layerTrack    = false;
        block.audioFilePath = "Samples/" + wavFile.getFileName();   // relative to project
        block.originalBPM   = (float) bpm;
        block.stretchMode   = true;
        // 2026-04-24 recorded-clip library registration: matches what
        // BuilderPage::importAudioFile does on user drop so the clip shows
        // up in the Builder's Audio tab and survives save/close/reopen.
        mPM->addAudioToLibrary (block.audioFilePath);
        mPM->addBlock (block);

        // R5d follow-up (2026-04-24): mirror the full onAudioClipAdded path so
        // the recorded clip gets the same wiring a user-dragged clip does.
        // Without these calls, the row has no mixer strip + no VibeGraph
        // InsertNode + no routing edges, so processInsert is a no-op and
        // routeInsertOutput fans into nothing -> silence on playback.
        const juce::String stripName = "Audio " + juce::String (nextRow + 1);
        if (mMixerPage)
            mMixerPage->addAudioChannel (nextRow, stripName);
        mProcessor.mVibeGraph.addAudioRowChannel (nextRow, stripName);
        mProcessor.ensureAudioInsert (nextRow, stripName);
        if (mEffectsPage)
            mEffectsPage->rebuildChannelDropdown();
    };

    if (res.masterFile.existsAsFile())
        dropWavAsClip (res.masterFile);
    for (const auto& [chId, f] : res.stripFiles)
    {
        juce::ignoreUnused (chId);
        dropWavAsClip (f);
    }

    // Kick the audio-clip streamer so the new block(s) are playable immediately.
    if (! res.masterFile.existsAsFile() && res.stripFiles.empty())
    {
        // nothing to do
    }
    else
    {
        mProcessor.rebuildAudioClipPlayers();
        if (mProjectManager) mProjectManager->markDirty();
    }

    // R5d-midi (2026-04-24): route captured MIDI notes to whichever piano
    // roll was last accessed.  onPlay already gated on mLastRollKind != None,
    // so if we got any notes the kind + index are valid.  If the pattern
    // changed mid-record, notes still land in the NOW-current pattern under
    // the same kind/index - matches FL Studio behavior.
    if (! res.midiNotes.empty() && mPM != nullptr)
    {
        PianoRollData* target = nullptr;
        auto& pat = mPM->currentPattern();
        switch (mLastRollKind)
        {
            case LastRollKind::Layer:
                if (mLastRollIndex >= 0 && mLastRollIndex < (int) pat.layerRoll.size())
                    target = &pat.layerRoll[mLastRollIndex];
                break;
            case LastRollKind::Bass:
                if (mLastRollIndex >= 0 && mLastRollIndex < (int) pat.bassRoll.size())
                    target = &pat.bassRoll[mLastRollIndex];
                break;
            case LastRollKind::Drums:
                target = &pat.drumRoll;
                break;
            case LastRollKind::None:
                break;
        }
        if (target != nullptr)
        {
            for (const auto& n : res.midiNotes)
                target->notes.push_back (n);
            if (mProjectManager) mProjectManager->markDirty();
        }
    }
}

void StandaloneEditor::refreshWindowTitle()
{
    juce::String title = "BaySickDAW";
    if (mProjectManager && mProjectManager->hasProject())
    {
        title += " - " + mProjectManager->getCurrentName();
        if (mProjectManager->isDirty()) title += " *";
    }
    if (auto* tlw = getTopLevelComponent())
        if (auto* dw = dynamic_cast<juce::DocumentWindow*> (tlw))
            dw->setName (title);
}
