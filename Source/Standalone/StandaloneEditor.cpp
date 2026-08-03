#include "StandaloneEditor.h"
#include "../AppPaths.h"
#include "../MissingFileReport.h"   // QA-ProjectSave Task 4: missing clip audio
#include "StandaloneApp.h"   // J-A2: MasterOutputRouting + saveMasterOutputRouting
#include "../TsMapRead.h"    // QA-G Task 6: played-TS source (marker map) for song mode
#include "../PatternManager.h"
#include "../ProjectManager.h"
#include "ProjectBrowserWindow.h"
#include "ProjectBundler.h"   // QA-Export: bundle walker + writer
#include "../SampleLibrary.h"
#include "../EngineRig.h"    // QA-ModelShell TS1: model-side engine owner
#include "EffectWindows.h"   // QA-ModelShell TS5: per-effect + EQ satellite windows
#include "LayersPage.h"
#include "BassPage.h"
#include "DrumPage.h"
#include "BuilderPage.h"
#include "PianoRollPage.h"
#include "MixerPage.h"
#include "../VibeGraph.h"   // MeterLatencyComp namespace (hamburger toggle)
#include "MetroPanel.h"
#include "SlotComponent.h"  // effectTypeName() for automation display-name resolver
#include "KeyBindings.h"
#include "TypingKeyboardMap.h"   // D-4 typing-keyboard MIDI (QA-TransportDisplay)
#include "KeyBindsWindow.h"
#include "PluginsManagerWindow.h"   // QA-ModelShell TS6: Options > Plugins
#include "MasterAnalyzerWindow.h"   // QA-ModelShell TS7 CL-044: master spectrum satellite
#include "LoudnessReportWriter.h"   // QA-ModelShell TS7 CL-227: HTML + CSV report
#include "PluginsPage.h"           // QA-ModelShell TS6: hosted-instrument tab view
#include "RustyDrumsMapWindow.h"
#include "PatternColorPicker.h"
#include "../BaySickSynth/BaySickSynthProcessor.h"   // D2 Batch 4: kit audition dispatch
#include "../VibePlayer/VibePlayerProcessor.h"       // D2 Batch 4: kit audition dispatch
#include "../Harmless/HarmlessProcessor.h"           // step 2 commit 2: layer/bass register helpers
#include "../BaySickBass/BaySickBassProcessor.h"     // step 2 commit 2: bass register helper
#include "../BaySickRustyDrums/BaySickRustyDrumsProcessor.h"  // J-3: kit loader verify
#include "BaySickRustyDrumsPage.h"                            // J-6: dedicated player page
#include "../BaySickGuitars/BaySickGuitarsProcessor.h"        // K-3: audition closure routes to engine->auditionNote
#include "../BaySickBasses/BaySickBassesProcessor.h"          // L-3: parallel BaySickBasses engine for the same dispatch
#include "../BaySickPedals/BaySickPedalsProcessor.h"          // 2026-05-05: dirty-hook wiring
#include "../DSP/EffectParamMap.h"   // QA-ModelShell TS3: pedal-board lane registration
#include "../BaySickNAMIR/BaySickNAMIRProcessor.h"             // 2026-05-05: dirty-hook wiring
#include "../BaySickVocal/BaySickVocalProcessor.h"             // 2026-05-05: dirty-hook wiring
// 2026-05-05: RustyDrumsPagePresetIO + AriaPagePresetIO consolidated into
// PagePresetIO.h.  Rusty's Save/Load Page Preset now uses PageKind::RustyDrums.
#include "PagePresetIO.h"
#include "../ClipDropDiag.h"                   // QA-ClipDrop: diagnostic trap (2026-06-02)
#include "../TempoMapRead.h"                   // QA-F: bake length in beats through the tempo map
#include "../Clips/ClipsPage.h"                       // G-2: Clips page + empty state
#include "../Vox/VoxPage.h"                           // G-4: Vox page + empty state
#include "../Inst/InstPage.h"                         // G-4: Inst page + empty state
#include "../MidiLearn/MidiLearnUI.h"                  // I-3c: MIDI Learn UI controller
#include "WindowChrome.h"                              // QA-ModelShell TS7 §9

namespace
{
    // Same reason as promptForProjectName below: defined near the bottom of this
    // TU, but the constructor reads capture settings out of it (TS7 §3).
    std::unique_ptr<juce::PropertiesFile> openUiPrefs();

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
// AudioSettingsDialog - safe device-switching dialog
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
//      new device, re-adds the callback - all while no render thread is running.
// ─────────────────────────────────────────────────────────────────────────────
class AudioSettingsDialog : public juce::Component
{
public:
    AudioSettingsDialog(juce::AudioDeviceManager& dm,
                        juce::AudioIODeviceCallback* cb)
        : mMgr(dm), mCallback(cb)
    {
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

        // C.3 (2026-04-30): MIDI input device list.  Toggling a device is
        // applied LIVE via setMidiInputDeviceEnabled (no restart needed unlike
        // audio); persistence happens via AudioDeviceManager's auto-save when
        // settings.xml is rewritten.  Layout: a bordered panel with one
        // ToggleButton per detected device.  Expands the dialog height to
        // fit; max 8 devices visible inline (current installs typically have
        // 1-3 detected; >8 will need a viewport - follow-up if it ever bites).
        styleLabel(mMidiLbl, "MIDI Inputs:");
        mMidiLbl.setJustificationType(juce::Justification::topRight);
        addAndMakeVisible(mMidiLbl);
        rebuildMidiToggles();

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

        // Vendor control panel exists only for ASIO devices (hasControlPanel).
        // Enablement keys on the LIVE device, not the combo selection -- this
        // dialog never touches the live device until Apply.
        mPanelBtn.setButtonText("Open ASIO Control Panel");
        mPanelBtn.setColour(juce::TextButton::buttonColourId,  VC::Panel);
        mPanelBtn.setColour(juce::TextButton::textColourOffId, VC::Text);
        {
            auto* liveDev = mMgr.getCurrentAudioDevice();
            mPanelBtn.setEnabled(liveDev != nullptr && liveDev->hasControlPanel());
        }
        mPanelBtn.onClick = [this] { showAsioControlPanel(); };
        addAndMakeVisible(mPanelBtn);

        populateFromManager();

        // Set the dialog size after the toggles are built so resized() can
        // measure (kRowH * (4 audio rows + N midi toggles + footer)).
        const int midiToggleCount = juce::jmax(1, (int) mMidiToggles.size());
        const int kRowH = 36, kPad = 16, kFooter = 60;
        setSize(480, kPad + 4 * kRowH + 8 + midiToggleCount * 26 + kFooter);
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

        // C.3 (2026-04-30): MIDI inputs list - label on the left, stacked
        // toggles on the right.  Empty list shows "(none detected)" italic.
        const int midiBlockTop = y;
        mMidiLbl.setBounds(kPad, midiBlockTop, kLblW, kComboH);
        const int togX = kPad + kLblW + 8;
        const int togW = getWidth() - kPad * 2 - kLblW - 8;
        const int togH = 24;
        if (mMidiToggles.empty())
        {
            mMidiNoneLbl.setBounds(togX, midiBlockTop, togW, togH);
        }
        else
        {
            for (size_t i = 0; i < mMidiToggles.size(); ++i)
                mMidiToggles[i]->setBounds(togX, midiBlockTop + (int) i * togH, togW, togH);
        }
        y = midiBlockTop + (int) juce::jmax((size_t) 1, mMidiToggles.size()) * togH + 12;

        const int btnW = 90, btnH = 28;
        mPanelBtn.setBounds(kPad, y, 176, btnH);
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
    // ── Populate - reads current state, never changes live manager state ──────
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

        // When type changes, repopulate devices - still no manager changes
        mTypeBox.onChange = [this] { refreshDeviceList(); };
        refreshDeviceList();
    }

    void refreshDeviceList()
    {
        mDevBox.clear(juce::dontSendNotification);

        // Get device names directly from the type object - does NOT change
        // the manager's active device type.
        int typeIdx = mTypeBox.getSelectedItemIndex();
        auto& types = mMgr.getAvailableDeviceTypes();
        if (typeIdx < 0 || typeIdx >= types.size()) return;

        auto* typeObj = types[typeIdx];
        // 2026-05-04: AudioDeviceManager only auto-scans the active device type
        // at initialise().  Switching the dropdown to a non-active type (e.g.
        // ASIO from Windows Audio) leaves that type's device list empty until
        // we explicitly trigger a scan.  scanForDevices() refreshes the type's
        // internal list without changing the manager's current device, so it's
        // safe to call from a passive-snapshot dialog like this one.
        typeObj->scanForDevices();
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

        // Use a fixed standard list - avoids creating a temporary device object
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

    // ── Apply - write settings to disk, prompt restart ────────────────────────
    // We never touch the live AudioDeviceManager here. WASAPI exclusive-mode
    // devices (USB gaming headsets etc.) cannot be closed and reopened safely
    // mid-session - any attempt crashes the message thread with no cleanup.
    // Instead we write the desired config to the settings XML and let the user
    // restart; initialise() on the next launch opens the new device cleanly.
    void applySettings()
    {
        // 2026-05-04: refuse to write a pending file with an empty device
        // name.  When the chosen audio mode has no installed drivers (e.g.
        // ASIO with no native ASIO driver and no ASIO4ALL), the device-list
        // ComboBox is empty.  Letting Apply through writes audioOutputDeviceName=""
        // to the pending XML, then the next launch fails to open any device,
        // processBlock never runs, the routing graph is never rebuilt, and
        // every cable on the Mixer page silently disappears.
        if (mDevBox.getText().isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "No audio device selected",
                "Pick an audio device from the list before applying.\n\n"
                "If the list is empty, the chosen audio mode has no installed drivers "
                "on this machine.  For ASIO mode, install your interface's native ASIO "
                "driver (recommended) or ASIO4ALL, then reopen this dialog.",
                "OK");
            return;
        }

        // Buffer-size-ONLY changes reconfigure the live device in place: same
        // type, same device, same rate, different buffer, device actually
        // open.  Everything else keeps the pending-file + restart flow below
        // (the WASAPI-exclusive hot-swap crash class makes arbitrary live
        // device swaps unsafe; a same-device buffer resize is the one narrow
        // reconfigure the startup live-reconfigure precedent already does).
        if (auto* liveDev = mMgr.getCurrentAudioDevice())
        {
            juce::AudioDeviceManager::AudioDeviceSetup cur;
            mMgr.getAudioDeviceSetup(cur);

            auto* curType = mMgr.getCurrentDeviceTypeObject();
            const bool sameType = curType != nullptr
                               && mTypeBox.getText() == curType->getTypeName();
            const bool sameDev  = mDevBox.getText() == cur.outputDeviceName;
            const bool sameRate = mRateBox.getSelectedId() > 0
                               && mRateBox.getSelectedId() == (int) std::llround(cur.sampleRate);
            const int  newBuf   = mBufBox.getSelectedId();

            // docket 3=b (defensive): gate the in-place live buffer reconfigure to
            // ASIO (hasControlPanel is the ASIO proxy the panel button uses).
            // WASAPI/DirectSound keep the pending-file + restart flow so the
            // exclusive-mode hot-swap path this dialog avoids is never reached.
            if (liveDev->hasControlPanel()
                && sameType && sameDev && sameRate
                && newBuf > 0 && newBuf != cur.bufferSize)
            {
                applyBufferSizeLive(newBuf);
                return;
            }
        }

        // Start from the current saved state so we only change what we touched.
        auto xml = mMgr.createStateXml();
        if (!xml)
            xml = std::make_unique<juce::XmlElement>("DEVICESETUP");

        int typeIdx = mTypeBox.getSelectedItemIndex();
        auto& types = mMgr.getAvailableDeviceTypes();
        if (typeIdx >= 0 && typeIdx < types.size())
            xml->setAttribute("deviceType", types[typeIdx]->getTypeName());

        xml->setAttribute("audioOutputDeviceName", mDevBox.getText());
        // 2026-04-30: do NOT clobber audioInputDeviceName here.  The dialog
        // has no input-device picker, so the old line `setAttribute(..., "")`
        // silently disabled ASIO inputs every Apply - Vox/Inst Listen mode
        // would stop receiving audio after any sample-rate / buffer-size
        // change.  Whatever input device was previously open survives via
        // the createStateXml() snapshot at the top of applySettings().
        // (Adding a real input-device picker is a follow-up; until then,
        // leaving the existing entry alone preserves the user's setup.)

        if (mRateBox.getSelectedId() > 0)
            xml->setAttribute("audioDeviceRate",       (double)mRateBox.getSelectedId());
        if (mBufBox.getSelectedId() > 0)
            xml->setAttribute("audioDeviceBufferSize", mBufBox.getSelectedId());

        // Write to a PENDING file - not the live settings file.
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
        // can crash because JUCE is mid-modal-cleanup - defer that too.
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

    // ── Live buffer-size reconfigure (buffer-only Apply path) ─────────────────
    void applyBufferSizeLive(int newBufferSize)
    {
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        mMgr.getAudioDeviceSetup(setup);
        setup.bufferSize = newBufferSize;

        // Quiesce our render callback around the reconfigure:
        // removeAudioCallback BLOCKS until any in-flight callback returns, so
        // none of BaySickDAW's code runs while the device closes + reopens at
        // the new size.  The documented WASAPI-exclusive crash class is stream
        // teardown racing the device's own render thread -- keeping our
        // callback out of that window is the practical shield here (the
        // dialog holds no processor reference for the bail-early shield).
        if (mCallback != nullptr)
            mMgr.removeAudioCallback(mCallback);

        const juce::String err = mMgr.setAudioDeviceSetup(setup, /*treatAsChosenDevice*/ true);

        if (mCallback != nullptr)
            mMgr.addAudioCallback(mCallback);

        if (err.isNotEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Buffer Size Change Failed",
                "The device rejected the new buffer size:\n\n" + err
                    + "\n\nThe previous settings remain active.",
                "OK");
            return;
        }

        // The manager now holds the new size: shutdown's saveAudioSettings
        // persists it to the live settings file.  No pending file, no restart
        // prompt.  The manager's change broadcast refreshes output-latency +
        // meter compensation via the existing StandaloneApp listener.
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    }

    // ── ASIO vendor control panel ─────────────────────────────────────────────
    // Mirrors juce::AudioDeviceSelectorComponent::showDeviceUIPanel: a modal
    // shield while the vendor panel is up, then close + restart the device when
    // the panel reports changed settings (ASIO buffer edits only take effect on
    // reopen).  The restart is safe here despite this dialog's no-live-touch
    // design: hasControlPanel gates this to ASIO only, so the WASAPI-exclusive
    // hot-swap crash class the design avoids can't reach this path.
    void showAsioControlPanel()
    {
        auto* dev = mMgr.getCurrentAudioDevice();
        if (dev == nullptr || ! dev->hasControlPanel())
            return;

        juce::Component modalShield;
        modalShield.setOpaque(true);
        modalShield.addToDesktop(0);
        modalShield.enterModalState();

        if (dev->showControlPanel())
        {
            mMgr.closeAudioDevice();
            mMgr.restartLastAudioDevice();
            // NIT-10: the vendor panel may have changed the buffer size; re-read
            // the live setup into the snapshot and refresh the combos so a later
            // Apply doesn't revert the panel's change via the live buffer path.
            mMgr.getAudioDeviceSetup(mSnapshot);
            populateRatesAndBuffers();
            if (auto* top = getTopLevelComponent())
                top->toFront(true);
        }
    }

    // C.3 (2026-04-30): build a ToggleButton per detected MIDI input device.
    // Each toggle's onClick calls setMidiInputDeviceEnabled live.  Persistence
    // happens via the device manager's auto-saved settings XML on shutdown.
    void rebuildMidiToggles()
    {
        for (auto& t : mMidiToggles)
            removeChildComponent(t.get());
        mMidiToggles.clear();
        removeChildComponent(&mMidiNoneLbl);

        const auto devices = juce::MidiInput::getAvailableDevices();
        if (devices.isEmpty())
        {
            mMidiNoneLbl.setText("(no MIDI devices detected)", juce::dontSendNotification);
            mMidiNoneLbl.setColour(juce::Label::textColourId, VC::TextDim);
            mMidiNoneLbl.setJustificationType(juce::Justification::centredLeft);
            mMidiNoneLbl.setFont(juce::Font(14.0f, juce::Font::italic));
            addAndMakeVisible(mMidiNoneLbl);
            return;
        }

        for (const auto& d : devices)
        {
            auto t = std::make_unique<juce::ToggleButton>(d.name);
            t->setColour(juce::ToggleButton::textColourId,         VC::Text);
            t->setColour(juce::ToggleButton::tickColourId,         VC::Accent);
            t->setColour(juce::ToggleButton::tickDisabledColourId, VC::TextDim);
            t->setToggleState(mMgr.isMidiInputDeviceEnabled(d.identifier),
                              juce::dontSendNotification);
            const juce::String id = d.identifier;
            t->onClick = [this, id, ptr = t.get()] {
                mMgr.setMidiInputDeviceEnabled(id, ptr->getToggleState());
            };
            addAndMakeVisible(*t);
            mMidiToggles.push_back(std::move(t));
        }
    }

    juce::AudioDeviceManager&             mMgr;
    juce::AudioIODeviceCallback*          mCallback;
    juce::AudioDeviceManager::AudioDeviceSetup mSnapshot;

    juce::Label      mTypeLbl, mDevLbl, mRateLbl, mBufLbl;
    juce::Label      mMidiLbl, mMidiNoneLbl;
    juce::ComboBox   mTypeBox, mDevBox, mRateBox, mBufBox;
    juce::TextButton mApplyBtn, mCloseBtn, mPanelBtn;
    std::vector<std::unique_ptr<juce::ToggleButton>> mMidiToggles;

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
    // QA-ModelShell TS4: the region contained windows live in.  This MUST be the
    // first statement in the constructor and must never run more than once.
    // hostPageInWindow silently declines to frame a page when it is missing, and
    // every WorkspaceWindow holds a reference to it -- so creating it late left
    // the early pages unframed, and re-creating it deleted the object the
    // already-open windows pointed at, which switched their containment and
    // magnetism off with no visible symptom other than windows escaping the
    // frame.  Both failures came from this one line sitting in the wrong place.
    mWorkspace = std::make_unique<Workspace>();
    addAndMakeVisible (*mWorkspace);

    // Scan core sample library once at startup (non-blocking on message thread,
    // just a directory walk - typically < 10 ms)
    SampleLibrary::getInstance().scan();

    // G-6 (2026-04-29): ensure the user-facing My Samples folder + Core
    // Library shortcut exist.  Idempotent - fast no-op when already present.
    // Done at startup so file pickers don't have to handle missing-folder
    // races and the user sees the folder in their Documents tree immediately.
    SampleLibrary::ensureUserSamplesDir();

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

    // QA-D STATE-04: stop transport before any project-open load begins.
    // Mirrors the existing stop-on-Pause pattern used elsewhere in this file
    // (search "mPlayHead.stop"); without this, opening a project mid-playback
    // streams silence (or partially torn-down engine state) until the load
    // finishes.
    mProjectManager->onBeforeOpenProject = [this]
    {
        if (mPlayHead.isPlaying())
        {
            mPlayHead.stop();
            if (mTransport) mTransport->setPlayState (false, true);
        }
    };
    mProcessor.onAnyStateChange = [this]
    {
        if (mProjectManager) mProjectManager->markDirty();
    };

    // 2026-05-05 dirty-flag wiring: PatternManager mutations (pattern CRUD,
    // arrangement add/remove, time-marker add/remove/rename) - none of which
    // route through APVTS - chain into ProjectManager::markDirty.
    if (mPM)
        mPM->onAnyChange = [this]
        {
            if (mProjectManager) mProjectManager->markDirty();

            // Re-arm the freeze re-render quiet period on EVERY content change.
            // A drag fires this continuously, which is the point: the re-render
            // waits until the movement stops instead of firing per mouse move
            // and locking the app for seconds each time.
            mLastContentEditMs = juce::Time::getMillisecondCounterHiRes();

            // TS7 §6.5: the last two missing invalidators -- ARRANGEMENT CONTENT
            // (what actually plays on a tab) and the TEMPO MAP -- are both
            // already covered by this one signal, which fires from every
            // pattern-side mutation including arrangement add/remove and
            // time-marker edits.  Only base BPM had a hook before, so moving a
            // clip or a tempo marker changed what a frozen tab should sound like
            // while its freeze still read as current.
            //
            // Deliberately markALL rather than per-tab: this signal does not
            // carry WHICH tab changed, and an arrangement edit can change what
            // plays on several at once.  Over-invalidating costs a re-render
            // (deferred to Stop); under-invalidating ships the wrong audio.
            mProcessor.engineRig().markAllFreezesStale();

            // §6.8 AXIS TWO: the edit landed in the CURRENT pattern, so that
            // pattern's cached render is wrong for every tab holding one.
            // markAllFreezesStale above is the broad song-scope signal; this is
            // the per-pattern half, without which a pattern's cache would go on
            // playing notes the user had already deleted.
            if (mPM) mProcessor.engineRig().markPatternContentChanged (mPM->getCurrentPatternIndex());
        };
    // QA-G Task 6: immediate UI refresh on TS lifecycle changes (the rolls +
    // pattern-button label also self-heal on their timers; this just makes
    // the response instant).
    if (mPM)
        mPM->onTimeSigStateChanged = [this]
        {
            refreshPatternBox();
            if (mBuilderPage) mBuilderPage->repaint();
        };

    // 2026-05-05 dirty-flag wiring: VU calibration target changes.  Default is
    // -18 dBFS; persists per-project via <VUCalibration> in UIState.  Static
    // callback so any caller of VUMeter::setCalibrationDb (Effects-page menu,
    // future API) flips the dirty bit without per-call-site plumbing.
    VUMeter::sOnCalibrationChanged = [this]
    {
        if (mProjectManager) mProjectManager->markDirty();
    };

    // P1+P2 persistence (2026-04-24): processor delegates tab + engine state
    // save/load to the editor.  These are fired inside serialize/deserialize.
    mProcessor.onSerializeUIState   = [this](juce::XmlElement& root)       { serializeUIState (root); };
    mProcessor.onDeserializeUIState = [this](const juce::XmlElement& root) { deserializeUIState (root); };

    // QA-ModelShell TS1: model-side automation registration.  The rig fires
    // this at EVERY engine creation (user pick, project restore, template
    // load, duplicate), so engine-parameter lanes exist without any editor
    // view being built.  View wrappers still overwrite these ids while views
    // live (identical param targets); TS3 retires the wrappers.
    mProcessor.engineRig().onEngineCreated = [this] (EngineTab& tab)
    {
        registerModelEngineAutomation (tab);
    };

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

    // ── Mixer-strip param materialization -> lane registration ───────────────
    // QA-ModelShell TS3: strips are created lazily, long after the startup
    // sweep, so their params were invisible to it.  The mixer strip and the EQ
    // display each covered that with their own view-scoped registration; this
    // model event replaces both.
    mProcessor.onMixerStripParamsCreated = [this] (const juce::String&)
    {
        registerStaticAutomationHandlers();
    };

    // ── sfizz engine ready -> lane registration ──────────────────────────────
    // QA-ModelShell TS3 fix: these three are processor-owned, so they need
    // their own model event; the rig's onEngineCreated never covers them.
    mProcessor.onSfizzEngineReady = [this] (VibeSynthProcessor::SfizzEngineKind kind,
                                            int instIdx)
    {
        registerSfizzEngineAutomation (kind, instIdx);
    };

    // ── Right-click menu label resolver ──────────────────────────────────────
    // Translates raw paramId to the friendly "Channel - Effect - Param" label
    // (honours userDisplayName) for the "Automate: X" menu item text.
    VKnobAutomation::sResolveMenuLabel = [this](const juce::String& pid) -> juce::String
    {
        return resolveAutomationDisplayName(pid);
    };

    // ── I-3c (2026-05-02): MIDI Learn UI wiring ──────────────────────────────
    // Owns the 30s auto-cancel timer + Escape-cancels keyboard hook.  Routes
    // every VKnob's right-click MIDI Learn / Forget / Save-as-default items
    // to the registry on the PluginProcessor.  Repaints VKnobs via a global
    // refresh whenever learn state or mappings change so the dashed-yellow
    // learn outline tracks the current target.
    mMidiLearnUI = std::make_unique<MidiLearnUI> (mProcessor.getMidiLearnRegistry());
    mMidiLearnUI->onLearnStateChanged = [this]
    {
        // Cheap message-thread repaint of every visible VKnob -- they read
        // sIsMidiLearningTarget in paintOverChildren.  In practice the
        // editor and all current pages get repainted; cost is negligible
        // because repaint() is async-coalesced.
        repaint();
    };

    VKnobAutomation::sIsMidiMapped = [this](const juce::String& pid) -> bool
    {
        return mMidiLearnUI && mMidiLearnUI->isMapped (pid);
    };
    VKnobAutomation::sIsMidiLearningTarget = [this](const juce::String& pid) -> bool
    {
        return mMidiLearnUI && mMidiLearnUI->isLearningTarget (pid);
    };
    VKnobAutomation::sDescribeMidiMapping = [this](const juce::String& pid) -> juce::String
    {
        return mMidiLearnUI ? mMidiLearnUI->describeMapping (pid) : juce::String();
    };
    VKnobAutomation::sOnMidiLearn = [this](const juce::String& pid)
    {
        if (mMidiLearnUI) mMidiLearnUI->beginLearn (pid);
    };
    VKnobAutomation::sOnMidiForget = [this](const juce::String& pid)
    {
        if (mMidiLearnUI) mMidiLearnUI->forget (pid);
    };
    VKnobAutomation::sOnMidiSaveAsDefault = [this]
    {
        if (! mMidiLearnUI) return;
        const bool ok = mMidiLearnUI->saveAsGlobalDefaults();
        juce::AlertWindow::showMessageBoxAsync (
            ok ? juce::MessageBoxIconType::InfoIcon
               : juce::MessageBoxIconType::WarningIcon,
            "MIDI Mappings",
            ok ? juce::String ("Saved current MIDI mappings as the global default. "
                                 "New projects will start with these mappings; existing "
                                 "projects keep their per-project settings.")
               : juce::String ("Could not write the global MIDI mappings file."));
    };
    VKnobAutomation::sHasAnyMidiMappings = [this]() -> bool
    {
        return mMidiLearnUI
            && mProcessor.getMidiLearnRegistry().getAllParamIds().size() > 0;
    };

    // Install the Escape-cancels listener on the editor itself (the JUCE
    // top-level Component wrapping the standalone window).  Focus rules:
    // KeyListener fires for any descendant whose key wasn't already handled.
    mMidiLearnUI->installOnTopLevelComponent (this);

    // Static registrations (all current APVTS params + "global_tempo") live in
    // registerStaticAutomationHandlers() so resetProjectState() can re-seed
    // them after its full map clear.
    registerStaticAutomationHandlers();

    // ── Global LAF + Tooltip ──────────────────────────────────────────────────
    juce::LookAndFeel::setDefaultLookAndFeel(&VibeLAF::get());
    mTooltipWindow = std::make_unique<VibeTooltip>(this, 600);

    // Global right-click listener - catches any slider with a componentID set
    addMouseListener(&mAutoRightClick, true);

    // ── Menu bar ─────────────────────────────────────────────────────────────
    mMenuBar = std::make_unique<juce::MenuBarComponent>(this);
    mMenuBar->setLookAndFeel(&VibeLAF::get());
    addAndMakeVisible(*mMenuBar);

    // ── Global Transport Bar - added FIRST so it is the background layer ──────
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
                            mPlayHead.setRecording (true);   // 2026-04-30 playhead PositionInfo
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
            mPlayHead.setRecording (false);   // 2026-04-30 playhead PositionInfo
        }
        mRecordArmed = false;
        if (mTransport) mTransport->setRecordArmed (false);
        if (mPlayHead.isPlaying())
        {
            mPlayHead.stop();
            // Playhead residual (c), Jeff 2026-07-24: quantize the paused
            // park to the nearest 16th so the marker sits ON a grid line and
            // snapped clicks beside it agree with it (resume shifts by at
            // most 1/32 note).
            mPlayHead.seekTo (std::round (mPlayHead.getCurrentBeat() * 4.0) / 4.0);
            mTransport->setPlayState(false, true);
        }
        // 2026-04-30: flush all-notes-off on Pause.  Was Stop-only - long-tail
        // notes (Harmless pads, big reverbs) would keep ringing after Pause
        // until the user hits Stop.  Same broadcast Stop uses, just promoted
        // up so the user never hears stuck voices regardless of which
        // halting button they pressed.
        mProcessor.mFlushAllNotes.store (true, std::memory_order_release);
    };
    mTransport->onStop  = [this]
    {
        stopTransportAndFinalizeRecording();
    };
    mTransport->onTempoChanged = [this](double bpm) {
        // QA-TempoMap: setBPM = BASE edit; its rebuild handles continuity
        // whether stopped or playing (the old play-time re-anchor call died
        // with the G1 review fix to start()).
        mPlayHead.setBPM(bpm);
        // 2026-04-24: tempo is a global PROJECT value; persist it through the
        // PatternManager so save/reload round-trips correctly.
        if (mPM) mPM->setGlobalTempo (bpm);
        if (mProjectManager) mProjectManager->markDirty();
        // TS7 §6.5: tempo moves EVERY engine's output in time, so it invalidates
        // every freeze rather than any one tab's -- the one item in the
        // invalidation list that is not per-tab.
        mProcessor.engineRig().markAllFreezesStale();
    };
    mTransport->onSongModeChanged = [this](bool songMode) {
        // Smoke round 3 (Jeff): applicator-lane baseline (engine params +
        // global_tempo) -- the processor's setSongMode handles the main-APVTS
        // lanes; these live outside the main APVTS, so capture/restore here
        // with the reader/applicator registries.
        if (songMode != mProcessor.isSongMode())
        {
            if (songMode)
            {
                mApplicatorBaseline.clear();
                if (mPM != nullptr)
                    for (int bi = 0; bi < mPM->getNumBlocks(); ++bi)
                    {
                        const auto& blk = mPM->getBlock (bi);
                        if (blk.clipType != ClipType::Automation)          continue;
                        if (blk.automationLane.paramId.isEmpty())          continue;
                        if (mProcessor.apvts.getParameter (blk.automationLane.paramId) != nullptr)
                            continue;   // main-APVTS lane: processor baseline owns it
                        bool seen = false;
                        for (const auto& p : mApplicatorBaseline)
                            if (p.first == blk.automationLane.paramId) { seen = true; break; }
                        if (seen) continue;
                        auto rd = mAutomationValueReaders.find (blk.automationLane.paramId);
                        if (rd != mAutomationValueReaders.end() && rd->second)
                            mApplicatorBaseline.push_back ({ blk.automationLane.paramId,
                                                             rd->second() });
                    }
            }
            else
            {
                for (const auto& p : mApplicatorBaseline)
                {
                    auto it = mAutomationApplicators.find (p.first);
                    if (it != mAutomationApplicators.end() && it->second)
                        it->second (p.second);
                }
                mApplicatorBaseline.clear();
            }
        }
        mProcessor.setSongMode(songMode);
        // QA-TempoMap: markers are song-domain - the timeline gains/loses
        // them on every mode switch (base tempo + live automation persist).
        pushTempoMarkersToPlayHead();
    };
    mTransport->onSongLoopModeChanged = [this](bool loop) {
        mProcessor.mSongLoopMode.store(loop, std::memory_order_relaxed);
    };
    // C.5b (post-revert): report the CURRENT PATTERN's intrinsic TS to the
    // playhead each tick.  QA-G Task 6 (docket #14): grid TS markers are the
    // SOLE played source -- song mode reads the marker map at the playhead
    // position; pattern mode uses the pattern's effective signature.
    // QA-G3Smoke SW-1: global Swing knob <-> main-APVTS `globalSwing`.
    {
        auto sb = mProcessor.makeSwingKnobBinding ("globalSwing", "globalSwing");
        mTransport->onGetSwing = sb.getMix;
        mTransport->onSetSwing = sb.setMix;
        mTransport->refreshSwingKnob();
    }

    mTransport->onGetTimeSig = [this](int& outNum, int& outDen) {
        outNum = 4; outDen = 4;
        if (mTransport && mTransport->isSongMode() && TsMap::isActive())
        {
            const auto bb = TsMap::barBeatAt (juce::jmax (0.0, mPlayHead.getCurrentBeat()));
            outNum = bb.num; outDen = bb.den;
        }
        else if (auto* pm = mProcessor.getPatternManager())
        {
            outNum = pm->currentPattern().tsNum;
            outDen = pm->currentPattern().tsDen;
        }
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
                    mProcessor.mLoopStartBeats.store(startBeats, std::memory_order_relaxed);   // QA-Ed: scheduler loop-seam window
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
                        mProcessor.mLoopStartBeats.store(startBeats, std::memory_order_relaxed);   // QA-Ed: scheduler loop-seam window
                        mProcessor.mCachedPatternLoopBeats.store(endBeats, std::memory_order_relaxed);
                        mProcessor.mSongEndBeats.store(0.0, std::memory_order_relaxed);
                        return endBeats;
                    }
                }
            }
        }

        // ── No time-selection: clear loop start so wrap goes to 0 ───────
        mPlayHead.setLoopStart(0.0);
        mProcessor.mLoopStartBeats.store(0.0, std::memory_order_relaxed);   // QA-Ed: scheduler loop-seam window

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
            // QA-Export: moved into PatternManager::getSongEndBeats() so offline
            // export stops at the SAME beat this transport loops at -- two copies
            // of this math would drift.  Semantics unchanged, including QA-H Task
            // 8 (#6): muted blocks still COUNT toward song length (mute silences a
            // block, it does not shorten the song).
            const double songEnd = mPM ? mPM->getSongEndBeats() : 0.0;
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
    // ── QA-TransportDisplay: position readout (overlay child, like the
    // pattern button + ribbon; placed by resized() between the two) ──────────
    mPosReadout = std::make_unique<TransportPositionReadout>();
    mPosReadout->onGetBeat         = [this] { return mPlayHead.getCurrentBeat(); };
    mPosReadout->onGetTimeSeconds  = [this] { return mPlayHead.getCurrentTimeSeconds(); };
    mPosReadout->onGetSongMode     = [this] { return mTransport && mTransport->isSongMode(); };
    mPosReadout->onGetPatternTs = [this] (int& outNum, int& outDen)
    {
        outNum = 4; outDen = 4;
        if (auto* pm = mProcessor.getPatternManager())
        {
            outNum = pm->currentPattern().tsNum;
            outDen = pm->currentPattern().tsDen;
        }
    };
    mPosReadout->onDisplayModeChanged = [this] (bool showTime) { saveTransportDisplayPref (showTime); };
    loadTransportDisplayPref();
    // NOTE: added as a child further down, AFTER addAndMakeVisible(*mTransport)
    // - the transport bar paints the full 40px row, so overlay children must
    // be added later to sit on top (same z-order rule as mPatternBtn/mRibbon).

    // D-4: the bar button reports clicks; Ctrl+T routes through perform() -
    // both land in toggleTypingKeyboard() so the two entry points can't drift.
    mTransport->onTypingKeyboardToggle = [this] { toggleTypingKeyboard(); };

    // 1M: DSP load readout - poll processor atomics each timer tick
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
    // 2026-04-30: button-state sync so SongLoop + Metronome buttons re-paint
    // correctly after project load.  Both the processor's atomics and the
    // PatternManager's mixer state are restored from XML before the
    // transport bar is re-laid out, but nothing pushed those values back
    // into the buttons' visual toggle state.  Polled via the transport's
    // existing 30 Hz timer.
    mTransport->onGetSongLoopMode = [this] {
        return mProcessor.mSongLoopMode.load (std::memory_order_relaxed);
    };
    mTransport->onGetMetronomeEnabled = [this] {
        return mProcessor.mMetro.enabled.load (std::memory_order_relaxed);
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
            mPlayHead.setRecording (false);   // 2026-04-30 playhead PositionInfo
        }
    };
    mTransport->onMetronomeToggle = [this](bool on) {
        mProcessor.mMetro.enabled.store(on, std::memory_order_relaxed);
    };
    // QA-Ea Task 0c (FL pre-roll record) + QA-Ee Stage 4: wire the
    // GlobalTransportBar Record-button submenu's "Global Record-Quantize"
    // picker to the APVTS param `Unified_RecordQuantizeDiv` (Int 0..10 on the
    // shared 11-label snap scheme -- see kUnifiedSnapLabels).  Getter for the
    // menu tick + setter for writing back through APVTS using the project's
    // standard setValueNotifyingHost pattern (matches every other dropdown
    // wiring in this file).
    mTransport->onGetRecordQuantizeDiv = [this]() -> int {
        if (auto* p = mProcessor.apvts.getRawParameterValue ("Unified_RecordQuantizeDiv"))
            return (int) p->load();
        return 0;
    };
    mTransport->onRecordQuantizeDivChanged = [this](int div) {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                mProcessor.apvts.getParameter ("Unified_RecordQuantizeDiv")))
            p->setValueNotifyingHost (
                p->getNormalisableRange().convertTo0to1 ((float) div));
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
    // QA-TransportDisplay: the readout overlays the bar - must be added AFTER
    // it (later child = higher z-order) or the bar's full-width paint hides it.
    if (mPosReadout) addAndMakeVisible (*mPosReadout);

    // ── Title label - hidden (title now lives in the OS window title bar) ─────
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

        // List all patterns - tick marks current.  QA-G Task 6 (docket #2):
        // non-4/4 effective signatures suffix the name ("Synths 7/8").
        for (int i = 0; i < n; ++i)
        {
            const auto& p = mPM->getPattern(i);
            const juce::String sfx = (p.tsNum == 4 && p.tsDen == 4)
                ? juce::String()
                : " " + juce::String(p.tsNum) + "/" + juce::String(p.tsDen);
            m.addItem(i + 1, p.name + sfx, true, i == cur);
        }

        m.addSeparator();
        m.addItem(-1, juce::String(juce::CharPointer_UTF8("\xe2\x9e\x95")) + "  New Pattern");
        m.addSeparator();
        m.addItem(-2, "Rename...");
        m.addItem(-4, "Change Color...");   // F-1 (2026-04-26)
        // QA-G Task 6 (docket B): the pattern TS surface is the same TYPE-IN
        // popup the grid markers use, with Reset to Default re-entering the
        // follower lifecycle.  Replaces the old 8-preset submenu.
        {
            const auto& curPat = mPM->currentPattern();
            m.addItem (-200, juce::String("Set Time Signature... (")
                             + juce::String (curPat.tsNum) + "/" + juce::String (curPat.tsDen)
                             + (curPat.tsLocked ? ", user-set)" : ", following)"));
        }
        // Docket #4 revision: which marker newly-created patterns bind to.
        // Grayed until 2+ markers exist; auto-selected by the add prompt.
        {
            juce::PopupMenu curSub;
            const int nTs    = mPM->getNumTimeSigChanges();
            const int curUid = mPM->getCurrentTsMarkerUid();
            for (int i = 0; i < nTs && i < 64; ++i)
            {
                const auto& ts = mPM->getTimeSigChange (i);
                curSub.addItem (-(300 + i),
                    "Bar " + juce::String (ts.bar + 1) + "  -  "
                    + juce::String (ts.num) + "/" + juce::String (ts.den),
                    true, ts.uid == curUid);
            }
            m.addSubMenu ("Current Time Signature (new patterns)", curSub, nTs >= 2);
        }
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
                else if (result == -200)
                {
                    // QA-G Task 6: type-in TS dialog (marker-popup parity)
                    // with Reset to Default (docket #1: clears the user-set
                    // lock, removes the pattern's still-linked auto-markers,
                    // re-enters the follower lifecycle).
                    const int idx = mPM->getCurrentPatternIndex();
                    const auto& curPat = mPM->currentPattern();
                    auto* aw = new juce::AlertWindow ("Pattern Time Signature",
                        "Time signature for \"" + curPat.name + "\":",
                        juce::MessageBoxIconType::NoIcon);
                    aw->addTextEditor ("num", juce::String (curPat.tsNum));
                    aw->addTextEditor ("den", juce::String (curPat.tsDen));
                    aw->addTextBlock ("Format: numerator (1-32) / denominator (power of 2: "
                                      "1, 2, 4, 8, 16, 32). Reset to Default returns the "
                                      "pattern to following the grid markers.");
                    aw->addButton ("Set", 1, juce::KeyPress (juce::KeyPress::returnKey));
                    aw->addButton ("Reset to Default", 2);
                    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                    aw->enterModalState (true,
                        juce::ModalCallbackFunction::create ([this, idx, aw](int r)
                        {
                            if (r == 1)
                            {
                                const int n2 = aw->getTextEditorContents("num").getIntValue();
                                const int d2 = aw->getTextEditorContents("den").getIntValue();
                                if (n2 > 0 && d2 > 0)
                                    mPM->setPatternTimeSig (idx, n2, d2);
                            }
                            else if (r == 2)
                            {
                                mPM->resetPatternTimeSig (idx);
                            }
                            refreshPatternBox();
                            if (mPianoRollPage) mPianoRollPage->repaint();
                            if (mBuilderPage)   mBuilderPage->repaint();
                        }), true);
                }
                else if (result <= -300 && result >= -363)
                {
                    // Docket #4: manual current-TS re-pick.
                    const int tsIdx = (-result) - 300;
                    if (tsIdx >= 0 && tsIdx < mPM->getNumTimeSigChanges())
                        mPM->setCurrentTsMarkerUid (mPM->getTimeSigChange (tsIdx).uid);
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
    // G-7 (2026-04-29): Clips/Vox/Inst now have requestDelete() too - wire
    // them through the same dispatch so the ribbon ▾ dropdown's Delete
    // shows the new G-7 prompt instead of silently doing nothing.
    mRibbon->onTabDeleteRequested = [this](int tabId) {
        for (auto* entry : mPages)
        {
            if (! entry || entry->ribbonTabId != tabId) continue;
            if      (auto* lp = dynamic_cast<LayersPage*>(entry->component.get())) lp->requestDelete();
            else if (auto* bp = dynamic_cast<BassPage*>  (entry->component.get())) bp->requestDelete();
            else if (auto* dp = dynamic_cast<DrumPage*>  (entry->component.get())) dp->requestDelete();
            else if (auto* cp = dynamic_cast<ClipsPage*> (entry->component.get())) cp->requestDelete();
            else if (auto* vp = dynamic_cast<VoxPage*>   (entry->component.get())) vp->requestDelete();
            else if (auto* ip = dynamic_cast<InstPage*>  (entry->component.get())) ip->requestDelete();
            else if (auto* pp = dynamic_cast<PluginsPage*> (entry->component.get())) pp->requestDelete();
            // J-6 (2026-05-03): BaySickRustyDrums uses the page's "Remove
            // BaySickRustyDrums" button as its delete action; if the user
            // hits dropdown Delete instead, route to the same flow.
            else if (auto* rp = dynamic_cast<BaySickRustyDrumsPage*> (entry->component.get()))
                if (rp->onDeleteRequested) rp->onDeleteRequested();
            return;
        }
    };
    mRibbon->onAddTabRequest  = [this](RibbonTabBar::TabType t) { onAddTabRequest(t); };
    // "+" menu picks an ENGINE; create that engine's tab, then select the
    // engine on the page it produced.  onAddTabRequest already owns every
    // per-type creation route (Clip opens the file picker, Vox/Inst run the
    // mixer strip-add cascade, L/B/D make the page), so this adds only the
    // engine selection on top of it rather than forking a second path.
    mRibbon->onAddEngineRequest = [this](RibbonTabBar::TabType t, const juce::String& engine)
    {
        onAddTabRequest (t);
        applyEngineToNewestTabOfType (t, engine);
    };
    // J-6 (2026-05-03): "+ Add BaySickRustyDrums" entry on the Drums dropdown.
    // 1-instance lock: hide entry when a BaySickRustyDrumsPage already exists
    // in mPages (the page can exist even before a kit is loaded, so checking
    // hasBaySickRustyDrums() on the processor isn't sufficient - that flag
    // only flips true after loadBaySickRustyDrumsKit succeeds).
    mRibbon->onAddBaySickRustyDrumsRequest = [this] { addBaySickRustyDrumsTab(); };
    // 2026-05-05 dirty-flag wiring: tab lock toggle.
    mRibbon->onTabLockChanged = [this] (int, bool) {
        if (mProjectManager) mProjectManager->markDirty();
    };
    mRibbon->onIsBaySickRustyDrumsActive   = [this]
    {
        for (auto* entry : mPages)
            if (entry && dynamic_cast<BaySickRustyDrumsPage*>(entry->component.get()))
                return true;
        return false;
    };
    // K-4 (2026-05-05): "+ Add BaySickGuitars" entry on the Inst dropdown.
    mRibbon->onAddBaySickGuitarsRequest = [this] { addBaySickGuitarsTab(); };
    // L-3 (2026-05-05): "+ Add BaySickBasses" entry on the Inst dropdown.
    mRibbon->onAddBaySickBassesRequest = [this] { addBaySickBassesTab(); };
    // QA-Fa recovery: "+ Add New Vox From Export" submenu on the Vox dropdown.
    mRibbon->onListVoxExports   = [this] { return listVoxExportEntries(); };
    mRibbon->onAddVoxFromExport = [this] (const juce::String& p) { addVoxFromExport (p); };
    mRibbon->onIsInstCapReached         = [this]
    {
        int n = 0;
        for (auto* entry : mPages)
            if (entry && entry->type == RibbonTabBar::TabType::Inst) ++n;
        return n >= (int) kMaxInstPages;
    };
    // TS7 §6.4: the ribbon shows ONE slot per type, so this reports the strongest
    // state across that type's instances -- stale wins over frozen, because a
    // stale tab is the one currently costing full CPU on its live engine.
    mRibbon->onIsTabFrozen = [this] (RibbonTabBar::TabType t) -> int
    {
        // ALL freezable kinds (fixed 2026-07-31) -- this mapped only the four
        // original engine types, so frozen Clips / Vox / Inst / Rusty tabs
        // showed no indicator at all.  Capacity comes from the rig per kind;
        // kMaxLayerPages was wrong for every kind except Layers.
        TabKind kind;
        switch (t)
        {
            case RibbonTabBar::TabType::Layers:  kind = TabKind::Layers;  break;
            case RibbonTabBar::TabType::Bass:    kind = TabKind::Bass;    break;
            case RibbonTabBar::TabType::Drums:   kind = TabKind::Drums;   break;
            case RibbonTabBar::TabType::Plugins: kind = TabKind::Plugins; break;
            case RibbonTabBar::TabType::Clip:    kind = TabKind::Clips;   break;
            case RibbonTabBar::TabType::Vox:     kind = TabKind::Vox;     break;
            case RibbonTabBar::TabType::Inst:    kind = TabKind::Inst;    break;
            default: return 0;
        }
        int best = 0;
        auto fold = [this, &best] (TabKind k, int count)
        {
            for (int i = 0; i < count && best < 2; ++i)
            {
                if (mProcessor.engineRig().isFreezeStale (k, i)) best = 2;
                else if (mProcessor.engineRig().isFrozen (k, i)) best = juce::jmax (best, 1);
            }
        };
        fold (kind, EngineRig::capacityOf (kind));
        // The kit has no ribbon type of its own -- it reports through the
        // Drums slot, the family it belongs to.
        if (t == RibbonTabBar::TabType::Drums)
            fold (TabKind::Rusty, 1);
        return best;
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
        //
        // QA-D STATE-02 follow-on: pre-fix this handler only renamed the mixer
        // strip + page mTabName; the piano-roll context label stayed stale
        // because no path called PianoRollPage::setEngineDisplayName.
        // Layer/Bass/Drum/Guitars/Basses/Clips all register with PianoRollPage
        // and need the label pushed through here.
        for (auto* entry : mPages)
        {
            if (!entry || entry->ribbonTabId != id) continue;
            if (auto* lp = dynamic_cast<LayersPage*>(entry->component.get()))
            {
                if (mMixerPage) mMixerPage->renameChannel(MixerPage::StripKind::Layer, lp->getPageIndex(), finalName);
                lp->setTabName(finalName);
                if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Layer, lp->getPageIndex() }, finalName);
            }
            else if (auto* bp = dynamic_cast<BassPage*>(entry->component.get()))
            {
                if (mMixerPage) mMixerPage->renameChannel(MixerPage::StripKind::Bass, bp->getPageIndex(), finalName);
                bp->setTabName(finalName);
                if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Bass, bp->getPageIndex() }, finalName);
            }
            else if (auto* dp = dynamic_cast<DrumPage*>(entry->component.get()))
            {
                if (mMixerPage) mMixerPage->renameChannel(MixerPage::StripKind::Drum, dp->getPageIndex(), finalName);
                dp->setTabName(finalName);
                if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Drum, dp->getPageIndex() }, finalName);
            }
            else if (auto* ip = dynamic_cast<InstPage*>(entry->component.get()))
            {
                // Inst tabs cover three sources: LiveInput (no piano roll),
                // BaySickGuitars, BaySickBasses.  No MixerPage::renameChannel
                // overload for StripKind::Inst exists -- the enum only covers
                // Layer/Bass/Drum -- so the mixer strip rename for Inst tabs
                // routes through a different path (left untouched here).  The
                // piano-roll-label push only fires for the two sfizz-source
                // engines that register with PianoRollPage.
                ip->setTabName(finalName);
                if (mPianoRollPage)
                {
                    const auto src = ip->getSource();
                    if (src == InstPage::Source::BaySickGuitars)
                        mPianoRollPage->setEngineDisplayName ({ EngineKind::BaySickGuitars, ip->getPageIndex() }, finalName);
                    else if (src == InstPage::Source::BaySickBasses)
                        mPianoRollPage->setEngineDisplayName ({ EngineKind::BaySickBasses, ip->getPageIndex() }, finalName);
                }
            }
            else if (auto* cp = dynamic_cast<ClipsPage*>(entry->component.get()))
            {
                // QA-ClipDrop Task 3 (SC-H, 2026-06-03): a Clips ribbon-tab
                // rename now syncs to its mixer strip via StripKind::Audio (the
                // strip is keyed in mAudioStrips by the Clips-page row index) --
                // parity with Layer/Bass/Drum above (previously left untouched
                // because the enum had no audio-row entry).
                if (mMixerPage) mMixerPage->renameChannel (MixerPage::StripKind::Audio, cp->getPageIndex(), finalName);
                cp->setTabName(finalName);
                if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Clip, cp->getPageIndex() }, finalName);
            }
            // TS6 (BLU-447) -- MISSED, fixed TS7 2026-07-30.  Same shape as Clips
            // above: without a StripKind entry a plugin tab rename reached neither
            // its mixer strip nor its roll label.
            else if (auto* pp = dynamic_cast<PluginsPage*>(entry->component.get()))
            {
                if (mMixerPage) mMixerPage->renameChannel (MixerPage::StripKind::Plugin, pp->getPageIndex(), finalName);
                pp->setTabName(finalName);
                if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Plugin, pp->getPageIndex() }, finalName);
            }
            else if (auto* vp = dynamic_cast<VoxPage*>(entry->component.get()))
            {
                // Vox tabs have a mixer strip + no piano-roll registration
                // (per G-4 the Vox piano-roll was deleted).  Mixer rename
                // routes through a different path (no enum entry here).
                vp->setTabName(finalName);
            }
            else if (auto* rp = dynamic_cast<BaySickRustyDrumsPage*>(entry->component.get()))
            {
                // QA-E Task 8 NIT-1 (QA-D carry-forward): Rusty was the only
                // page type missing from this dispatch, so renaming a Rusty
                // tab left its piano-roll context label stale.  Rusty is a
                // singleton engine registered at index 0 (see registerEngine
                // {EngineKind::BaySickRustyDrums, 0}).  Mixer-strip rename
                // routes through a separate path (parallel to Drum handling).
                rp->setTabName(finalName);
                if (mPianoRollPage)
                    mPianoRollPage->setEngineDisplayName ({ EngineKind::BaySickRustyDrums, 0 }, finalName);
            }
            break;
        }
        refreshAllKitViews();   // D2: ribbon rename → kit row labels
    };
    addAndMakeVisible(*mRibbon);

    // ── Page Menu Bar (Tier 2) ────────────────────────────────────────────────
    mDetachedPageMenu = std::make_unique<PageMenuBar>();
    mPageMenuBar      = mDetachedPageMenu.get();
    mPageMenuBar->setPageTitle("BaySickDAW");

    // ── Build default tabs ────────────────────────────────────────────────────
    buildDefaultTabs();

    // ── Keymap framework (Phase A - 2026-04-26) ──────────────────────────────
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

    // G1 smoke item-12 fix: the typing-keyboard note handler must outrank
    // the command map (bare note letters R / S / H collide with letter
    // command bindings).  FRAMEWORK QUIRK: ComponentPeer::handleKeyPress
    // iterates a component's key listeners in REVERSE registration order
    // (juce_ComponentPeer.cpp `for (int i = size(); --i >= 0;)`) and runs
    // them BEFORE the component's own keyPressed - so highest priority =
    // registered LAST.  The gate therefore registers AFTER the mapping set.
    addKeyListener (this);
    setWantsKeyboardFocus(true);

    // 2026-04-26: deferred keyboard-focus grab.  At ctor time the window isn't
    // on screen yet so grabKeyboardFocus() is a no-op - defer to the next
    // message-loop tick.  Without this, keybinds don't fire until the user
    // clicks somewhere in the app first.
    juce::Component::SafePointer<StandaloneEditor> safe (this);
    juce::MessageManager::callAsync ([safe]
    {
        if (safe != nullptr) safe->grabKeyboardFocus();
    });

    // ── TS7 §3: version capture ───────────────────────────────────────────────
    // The analysis half is ALWAYS on, so the master tap is armed for the whole
    // session rather than by a window's suspend hook.  This is the tap's second
    // client (the analyzer window is the first); VibeGraph ORs the two wants, so
    // closing that window no longer stops analysis.
    mProcessor.setMasterAnalysisActive (true);

    mVersionCapture.setAudioCaptureEnabled (
        openUiPrefs()->getBoolValue ("fsCaptureAudio", false));
    // §3.4: session-only is the default (id 1); id 2 is <project>\Reports\.
    mVersionCapture.setRetainInProject (
        openUiPrefs()->getIntValue ("fsCaptureRetain", 1) == 2);

    // Ruling 6a (2026-07-31): takes are judged against the export dialog's
    // persisted spec choice -- ONE loudness standard everywhere.  setSpec had
    // no caller at all, so every take was graded against the -14 default no
    // matter what the user worked to.
    applyCaptureSpecFromPrefs();

    mVersionCapture.onTakeBegan = [this]
    {
        mProcessor.resetMasterTruePeakMax();
        // Re-read per take, so a spec change in the export dialog applies from
        // the NEXT take with no wiring between the two surfaces.
        applyCaptureSpecFromPrefs();
    };

    mVersionCapture.onBeginAudio = [this] (const juce::File& target) -> bool
    {
        return mProcessor.startMasterCapture (target);
    };
    mVersionCapture.onEndAudio = [this]() -> juce::File
    {
        return mProcessor.stopMasterCapture();
    };
    mVersionCapture.onGetProjectDir = [this]() -> juce::File
    {
        return mProjectManager ? mProjectManager->getCurrentFolder() : juce::File();
    };
    // §3.4: a retained take's ANALYSIS persists as one of the existing loudness
    // reports.  Same folder, same format, same reader -- so it shows up in the
    // Files browser's Reports section and reopens in this analyzer with no new
    // machinery.  Violations are absent because a capture measures Short-Term
    // loudness on a UI timer rather than scanning blocks; the curve, the summary
    // and the verdicts are all real.
    mVersionCapture.onPersistTake = [this] (const VersionCapture::Version& v)
    {
        const auto dir = mProcessor.getProjectReportsDir();
        if (dir == juce::File()) return;

        BuilderPage::MeasureResult m;
        m.lufsCurve      = v.lufsCurve;
        m.integratedLufs = v.integratedLufs;
        m.lraLu          = v.lraLu;
        m.truePeakDb     = v.truePeakDb;
        m.maxShortTermLufs = v.maxShortTerm;
        m.durationSeconds  = v.durationSec;
        m.violations          = v.violations;          // §5.3 flagged moments
        m.violationsTruncated = v.violationsTruncated;
        m.specId              = mVersionCapture.getSpec().id;
        m.customTargetLufs    = mVersionCapture.getSpec().integratedLufs;
        m.truePeakInSpec      = (m.truePeakDb <= mVersionCapture.getSpec().maxTruePeakDb);
        m.integratedInSpec    = (! mVersionCapture.getSpec().checksIntegrated)
                              || (std::abs (m.integratedLufs
                                            - mVersionCapture.getSpec().integratedLufs)
                                  <= mVersionCapture.getSpec().toleranceLu);

        LoudnessReportWriter::Context ctx;
        ctx.projectName   = mProjectManager ? mProjectManager->getCurrentFolder()
                                                  .getFileNameWithoutExtension()
                                            : juce::String();
        ctx.scopeLabel    = v.scopeLabel;
        ctx.timestampLabel = v.label;

        juce::String err;
        if (! LoudnessReportWriter::write (dir, v.label.replaceCharacter (':', '-'),
                                           m, ctx, /*wantCsv*/ false, err))
        {
            // The writer's own contract: a failure is SHOWN, never swallowed.
            // Once per session, not per take -- this fires automatically at
            // every take end, and a broken Reports folder would otherwise
            // popup on every loop wrap.
            DBG ("[TS7 CAPTURE] take report write failed: " << err);
            if (! mTakeReportErrorShown)
            {
                mTakeReportErrorShown = true;
                juce::AlertWindow::showMessageBoxAsync (
                    juce::AlertWindow::WarningIcon, "Take report not saved",
                    err + "\n\nFurther failures this session will not be shown.");
            }
        }
    };

    mVersionCapture.onVersionsChanged = [this]
    {
        if (auto* win = findAuxWindow ("analyzer:master"))
            if (auto* v = dynamic_cast<MasterAnalyzerView*> (win->getContent()))
                v->refreshVersions();
    };

    // ── Automation playback timer ─────────────────────────────────────────────
    mAutomationTimer.startTimerHz(30);

    // ── Pattern-dropdown label sync (10 Hz, repaints only on change) ──────────
    mPatternLabelTimer.startTimerHz(10);

    // ── QA-Fe2 De-noise poll (input-assignment watch -> learner enable;
    // 5 Hz is plenty for human assign gestures, matches the timer-poll idiom) ──
    mVoxTakePick.fill (-1);
    mVoxInputIdxLast.fill (-999);
    mDenoisePollTimer.startTimerHz(5);
}

StandaloneEditor::~StandaloneEditor()
{
    // 2026-05-06 (Batch 9c B2-followup): Tear down all dynamic tabs (Vox /
    // Inst / Clip / Layers / Bass / Drums / Rusty) THROUGH THE SAFE PATH
    // first.  Without this, the bare mPages.clear() below would destroy
    // VoxPages (etc.) directly, leaving the raw pointers in
    // VibeSynthProcessor::mVoxEngines / mInstEngines / mLayerEngines /
    // mBassEngines / mDrumEngines / mClipEngines dangling while the audio
    // device is still running.  The audio thread's per-block iteration
    // (PluginProcessor.cpp:2136 etc.) would then dynamic_cast through a
    // freed vtable -> use-after-free crash inside the VC runtime's RTTI
    // walker.  Same crash signature appeared on app close on two different
    // save files; not addressed by the BaySickVocal N1 gate (which lives
    // INSIDE that processor's processBlock, downstream of the dangling
    // dereference).
    //
    // closeAllDynamicTabs sets setProjectLoadInProgress(true) +
    // Thread::sleep(30) BEFORE invoking onTabClosed on each tab (which
    // calls unregisterVoxEngine / etc. before destroying the page), then
    // clears the gate on its way out.  By the time it returns, every
    // engine pointer in mVox/Inst/Layer/Bass/Drum/ClipEngines is nullptr
    // and the rest of this destructor can clear mPages safely.
    closeAllDynamicTabs();

    mAutomationTimer.stopTimer();
    removeMouseListener(&mAutoRightClick);
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    stopPlayback();
    mProcessor.setPatternManager(nullptr);
    mProcessor.onLoadProgress = nullptr;
    // Detach the keymap-set listener installed in the ctor.  GlobalTransportBar
    // is no longer a KeyListener (Phase A 2026-04-26 - keymap migration).
    if (auto* set = mCmdMgr.getKeyMappings())
        removeKeyListener(set);
    removeKeyListener (this);   // G1 smoke item-12: the typing-note gate

    // Null legacy pointers before OwnedArray destroys everything
    mLegacyLayersPage = nullptr;
    mLegacyBassPage   = nullptr;
    mBuilderPage      = nullptr;
    mEffectsPage      = nullptr;
    mMixerPage        = nullptr;
    mVisiblePage      = nullptr;
    // Before the windows those menus live in are destroyed with their pages.
    mPageMenuBar      = mDetachedPageMenu.get();
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
        mPages.add(entry);
        hostPageInWindow (*entry);
    };

    // The four system tabs already exist in the ribbon with IDs 1/2/3/4.
    // 2026-04-26: PianoRoll added as a fixed slot (id=4) - dynamic Layers /
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
        // #30b regression fix (QA-G3Smoke): every roll/kit note mutation
        // republishes the scheduler's roll snapshot.  The roll editors never
        // call PatternManager::notifyContentChanged themselves (their
        // onNotesChanged tail was repaint-only), so without this hook a
        // freshly placed note exists on screen but never reaches the audio
        // thread.
        mPianoRollPage->setContentEditedHook ([this]
        {
            if (auto* pm = mProcessor.getPatternManager())
                pm->notifyContentChanged();

            // TS7 §6.5: a note edit changes what the ENGINE produces, so it makes
            // that tab's freeze stale.  Hooked HERE because this is the single
            // funnel every roll and kit mutation already passes through -- the
            // per-grid onNotesChanged sites are many and would drift.
            //
            // Marked across every frozen tab rather than just the edited one:
            // this hook does not carry which roll changed, and a freeze that is
            // wrongly re-rendered costs render time while one wrongly kept plays
            // the WRONG AUDIO.  Erring toward re-rendering is the safe direction,
            // and markEngineContentChanged only touches tabs that are actually
            // frozen (a no-op for the rest).
            mProcessor.engineRig().markAllFreezesStale();
        });
        // #30 (QA-G3Smoke): device info for the roll/kit playhead's visual
        // latency compensation (permanent, unlike the Debug diag below).
        mPianoRollPage->deviceInfoProvider = [this] (int& lat, double& sr)
        {
            lat = mProcessor.getTotalOutputLatency();
            sr  = mProcessor.getSampleRate();
        };
        // #31 (QA-G3Smoke): song beat -> viewed pattern's local beat (block of
        // the viewed pattern whose span contains the beat), -1 when none.
        mPianoRollPage->songLocalBeatProvider = [this] (double songBeat) -> double
        {
            auto* pm = mProcessor.getPatternManager();
            if (pm == nullptr) return -1.0;
            const int viewed = pm->getCurrentPatternIndex();
            for (int i = 0; i < pm->getNumBlocks(); ++i)
            {
                const auto& blk = pm->getBlock (i);
                if (blk.clipType != ClipType::Pattern || blk.muted) continue;
                if (blk.patternIndex != viewed) continue;
                if (! pm->isRowAudible (blk.trackRow)) continue;
                const double s = effectiveStartBeats (blk);
                if (songBeat < s || songBeat >= s + effectiveLengthBeats (blk)) continue;
                return songBeat - s + ticksToBeats (blk.contentOffsetTicks);
            }
            return -1.0;
        };
#if JUCE_DEBUG
        // [G3 PLAYHEAD] G-9 reading (QA-G3Smoke Task 1); Debug-only.
        mPianoRollPage->g3DiagDeviceInfo = [this] (int& lat, double& sr)
        {
            lat = mProcessor.getTotalOutputLatency();
            sr  = mProcessor.getSampleRate();
        };
#endif
        // Live-note monitor: the page timer reads held hardware-MIDI notes from
        // the processor each tick to light the active roll's keyboard.
        mPianoRollPage->liveHeldNotesProvider = [this](uint64_t& lo, uint64_t& hi)
        { mProcessor.getLiveHeldNotes (lo, hi); };
        // QA-Ee Stage 3: GLOBAL Piano Roll snap (Unified_PianoRollSnapDiv) read/write,
        // fanned into every roll + the drum kit so all share one snap.
        mPianoRollPage->setSnapAccessors (
            [this]() -> int {
                if (auto* p = mProcessor.apvts.getRawParameterValue ("Unified_PianoRollSnapDiv"))
                    return (int) p->load();
                return 1;
            },
            [this](int div) {
                if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                        mProcessor.apvts.getParameter ("Unified_PianoRollSnapDiv")))
                    p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 ((float) div));
            });
        // QA-UICleanup Task 4: GLOBAL Tools>Quantize resolution (Unified_QuantizeDiv),
        // decoupled from snap, fanned into every roll + the drum kit the same way.
        mPianoRollPage->setQuantizeAccessors (
            [this]() -> int {
                if (auto* p = mProcessor.apvts.getRawParameterValue ("Unified_QuantizeDiv"))
                    return (int) p->load();
                return 0;
            },
            [this](int div) {
                if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                        mProcessor.apvts.getParameter ("Unified_QuantizeDiv")))
                    p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 ((float) div));
            });
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
                else if (dynamic_cast<BaySickRustyDrumsPage*> (entry->component.get()))
                {
                    // J-7a (2026-05-03): BaySickRustyDrums singleton appears
                    // in the dropdown.  Index always 0 (1-instance lock).
                    k = EngineKind::BaySickRustyDrums;
                    idx = 0;
                }
                // TS6 (BLU-447) -- MISSED, fixed TS7 2026-07-30.  Without this
                // case a Plugins page fell through to the `else continue` below,
                // so the roll registered by registerPluginPianoRoll had no way to
                // be SELECTED and the whole plugin roll was unreachable.
                // Gated on a plugin actually being picked: an empty tab has no
                // engine to play notes into, matching the isEngineLocked test the
                // Layer / Bass entries use.
                else if (auto* pp = dynamic_cast<PluginsPage*> (entry->component.get()))
                {
                    if (pp->getEngineProcessor() == nullptr) continue;
                    k = EngineKind::Plugin;
                    idx = pp->getPageIndex();
                }
                else if (auto* ip = dynamic_cast<InstPage*> (entry->component.get()))
                {
                    // K-3 (2026-05-05): sfizz-source Inst pages (BaySickGuitars
                    // / BaySickBasses) surface in the dropdown.  LiveInput Inst
                    // pages stay hidden (no MIDI-driven engine to play notes
                    // through - the chain is fed by ASIO + recorded audio).
                    const auto src = ip->getSource();
                    if (src == InstPage::Source::LiveInput) continue;
                    k = (src == InstPage::Source::BaySickGuitars)
                            ? EngineKind::BaySickGuitars
                            : EngineKind::BaySickBasses;
                    idx = ip->getPageIndex();
                }
                // G-4 (2026-04-28): live-input Vox + Inst do NOT appear in
                // the unified Piano Roll dropdown - they're live-input /
                // recorded-audio destinations, not MIDI-triggered engines.
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

        mPianoRollPage->onEngineSelected = [this](EngineId id) {
            // C.3 (2026-04-30): push the focused engine into the processor as
            // the live MIDI input target.  Routing semantics in
            // PluginProcessor::processBlock: Layer / Bass / Drum + the sfizz
            // instruments Guitars / Basses / Rusty Drums receive; DrumKit grid,
            // Clip, Vox, and live-input Inst drop (no MIDI-driven engine).  The
            // enumeration above only surfaces the MIDI-driven kinds here.
            mProcessor.setLiveMidiTarget ((int) id.kind, id.index);

            // Refresh the menu-bar pill label on the next showPageForTab pass
            // (onEngineSelected fires AFTER selectEngine completes).  If the
            // PianoRollPage is currently visible, force the setTabSlots
            // rebuild now so the label updates immediately.
            if (mVisiblePage == mPianoRollPage)
                showPageForTab (4);
        };
        // C.3: push the initial focus once so the processor knows the target
        // before the user picks anything.  Default = whatever PianoRollPage
        // boots with (DrumKit grid -> drop, harmless).
        {
            const auto initial = mPianoRollPage->getActiveEngineId();
            mProcessor.setLiveMidiTarget ((int) initial.kind, initial.index);
        }
    }

    // Heavy-op overlay: added last + always-on-top so it covers every page.
    addChildComponent (mHeavyOpOverlay);
    mProcessor.onLoadProgress = [this] (const juce::String& label)
    {
        mHeavyOpOverlay.setStepLabel (label);
    };

    // QA-ProjectSave docket 18 (2026-07-26): no default dynamic tabs.  The app
    // opens with an empty ribbon and the user adds what they want, so nothing
    // the user never asked for ends up in a saved project or a template.

    mStartupComplete = true;

    // Every window framed at launch needs its title-strip menu built.
    // showPageForTab is what fills it, and it only ever ran for the ONE tab that
    // got selected -- so the other windows came up with a bare title strip until
    // the user closed and reopened them, which routed through tab selection and
    // configured it on the way (Jeff, 2026-07-28).  Only already-framed pages
    // are touched here, so this does not defeat the launch-open policy above.
    // The selection below runs last and leaves the active window in front.
    for (auto* e : mPages)
        if (e != nullptr && e->window != nullptr)
            showPageForTab (e->ribbonTabId);

    // Start on Builder tab (id=3)
    mRibbon->selectTab(3);
    onTabSelected(3);
}

HeavyOperationOverlay* StandaloneEditor::busyOverlayFor (juce::Component* c)
{
    if (c != nullptr)
        if (auto* ed = c->findParentComponentOfClass<StandaloneEditor>())
            return &ed->mHeavyOpOverlay;
    return nullptr;
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

// QA-ModelShell TS6 (BLU-447): hosted VST3 instrument tab.  No mLegacy* raw
// pointer is cached for it -- those caches are the dangling-pointer surface TS4
// measured, and a new page type has no reason to add another.
std::unique_ptr<juce::Component> StandaloneEditor::createPluginsPage()
{
    int idx = -1;
    for (int i = 0; i < (int) kMaxPluginPages; ++i)
        if (! mUsedPluginIndices[(size_t) i]) { idx = i; break; }

    if (idx < 0) return nullptr;   // all plugin slots in use

    mUsedPluginIndices[(size_t) idx] = true;
    return std::make_unique<PluginsPage> (mProcessor, idx);
}

// Project restore: the saved pageIndex is authoritative (it keys the mixer
// strip prefix and the InsertNode), so this takes it rather than allocating.
std::unique_ptr<juce::Component> StandaloneEditor::createPluginsPageAtIndex (int idx)
{
    if (idx < 0 || idx >= (int) kMaxPluginPages) return nullptr;
    if (mUsedPluginIndices[(size_t) idx]) return nullptr;

    mUsedPluginIndices[(size_t) idx] = true;
    return std::make_unique<PluginsPage> (mProcessor, idx);
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
    const juce::String layerName = nextLayerTabName();   // QA-D STATE-02
    lp->setTabName (layerName);                          // sync internal mTabName
    const int newId = mRibbon->addTab (RibbonTabBar::TabType::Layers, layerName);
    lp->onEngineSelected = [this, newId, pageIdx, lp] {
        const auto* tab = mRibbon->getTabById (newId);
        if (mMixerPage)   mMixerPage->addLayerChannel (pageIdx, tab ? tab->name : "Layers");
        if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
        wireEngineDirtyHook (lp->getEngineProcessor());
        // QA-D STATE-02 follow-on: piano-roll context label.
        if (mPianoRollPage)
            mPianoRollPage->setEngineType ({ EngineKind::Layer, pageIdx }, lp->getEngineType());
    };
    lp->onDeleteRequested = [this, newId] {
        if (! mRibbon) return;
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
        if (mMixerPage) mMixerPage->renameChannel (MixerPage::StripKind::Layer, pageIdx, nm);
        if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Layer, pageIdx }, nm);
    };
    registerLayerPianoRoll (lp);
    auto* entry = new PageEntry();
    entry->ribbonTabId = newId;
    entry->type        = RibbonTabBar::TabType::Layers;
    entry->component   = std::move (page);
    mPages.add (entry);
    hostPageInWindow (*entry);
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
    const juce::String bassName = nextBassTabName();   // QA-D STATE-02
    bp->setTabName (bassName);                         // sync internal mTabName
    const int newId = mRibbon->addTab (RibbonTabBar::TabType::Bass, bassName);
    bp->onEngineSelected = [this, newId, pageIdx, bp] {
        const auto* tab = mRibbon->getTabById (newId);
        if (mMixerPage)   mMixerPage->addBassChannel (pageIdx, tab ? tab->name : "Bass");
        if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
        wireEngineDirtyHook (bp->getEngineProcessor());
        // QA-D STATE-02 follow-on: piano-roll context label.
        if (mPianoRollPage)
            mPianoRollPage->setEngineType ({ EngineKind::Bass, pageIdx }, bp->getEngineType());
    };
    bp->onDeleteRequested = [this, newId] {
        if (! mRibbon) return;
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
        if (mMixerPage) mMixerPage->renameChannel (MixerPage::StripKind::Bass, pageIdx, nm);
        if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Bass, pageIdx }, nm);
    };
    registerBassPianoRoll (bp);
    auto* entry = new PageEntry();
    entry->ribbonTabId = newId;
    entry->type        = RibbonTabBar::TabType::Bass;
    entry->component   = std::move (page);
    mPages.add (entry);
    hostPageInWindow (*entry);
    bp->importBassState (clipboardXml);
    mRibbon->selectTab (newId);
    onTabSelected (newId);
}

void StandaloneEditor::spawnDuplicateDrumTab (const juce::String& clipboardXml)
{
    // D1.4-fix (c): Duplicate Drum action - find the next free drum index,
    // create a new DrumPage at that slot, wire its callbacks, then apply the
    // serialized state.  Mirrors the onAddTabRequest(Drums) flow + paste.
    auto page = createDrumPage();
    if (! page) return;   // 16-drum cap reached
    auto* dp = dynamic_cast<DrumPage*> (page.get());
    if (dp == nullptr) return;

    const int pageIdx = dp->getPageIndex();
    const juce::String drumName = nextDrumTabName();   // QA-D STATE-02
    dp->setTabName (drumName);                         // sync internal mTabName
    const int newId = mRibbon->addTab (RibbonTabBar::TabType::Drums, drumName);
    dp->onEngineSelected = [this, newId, pageIdx, dp] {
        const auto* tab = mRibbon->getTabById (newId);
        if (mMixerPage)   mMixerPage->addDrumChannel (pageIdx, tab ? tab->name : "Drums");
        if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
        refreshAllKitViews();
        wireEngineDirtyHook (dp->getEngineProcessor());
        // QA-D STATE-02 follow-on: piano-roll context label.
        if (mPianoRollPage)
            mPianoRollPage->setEngineType ({ EngineKind::Drum, pageIdx }, dp->getEngineType());
    };
    dp->onSoundNameChanged = [this, newId, pageIdx, dp] (const juce::String& nm) {
        if (nm.isEmpty()) return;
        if (mRibbon)    mRibbon->renameTab (newId, nm);
        if (mMixerPage) mMixerPage->renameChannel (MixerPage::StripKind::Drum, pageIdx, nm);
        dp->setTabName (nm);
        refreshAllKitViews();
        if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Drum, pageIdx }, nm);
    };
    dp->onDeleteRequested = [this, newId] {
        if (! mRibbon) return;
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
    mPages.add (entry);
    hostPageInWindow (*entry);

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

// 2026-04-25: createDrumsPage() removed - legacy DrumsPage class deleted.
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

    // QA-ModelShell TS2: the offline drive takes the LIVE processor, so the
    // 30 Hz automation timer must not fight the render's own lane
    // application.  Fires on the render thread -- marshal to the message
    // thread (Timer start/stop is message-thread-only).
    page->onOfflineRenderActive = [this] (bool active)
    {
        auto apply = [this, active]
        {
            if (active) mAutomationTimer.stopTimer();
            else        mAutomationTimer.startTimerHz (30);
        };
        if (juce::MessageManager::getInstance()->isThisTheMessageThread()) apply();
        else juce::MessageManager::callAsync (apply);
    };

    // Wire grid callbacks
    if (auto* grid = page->getGrid())
    {
        grid->onOpenEventEditor = [this](int blockIdx)
        {
            openEventEditor(blockIdx);
        };

        grid->onAudioClipAdded = [this](int row, const juce::String& rowName, const juce::String& filePath)
        {
            ClipDropDiag::log ("onAudioClipAdded ENTER", "row=" + juce::String (row) + " name=" + rowName + " path=" + filePath);
            // QA-ClipDrop Task 3 (SC-G/H, 2026-06-03): the strip trio +
            // Effects-dropdown rebuild + Clips-page spawn now live in the shared
            // createClipStripAndPage helper (reused by "+ Add New Clip" and
            // project reload).  SC-H: the strip + page are named from the SAMPLE,
            // not the Builder grid row label `rowName` ("Track N").  The helper
            // keeps the 2026-04-29 order-fix (InsertNode + APVTS params before the
            // strip so setApvts binds the fader/mute/solo/width controls).
            createClipStripAndPage (row, filePath);

            // Rebuild audio readers so the just-dropped clip plays back
            // immediately.  Order vs the helper's spawnClipsTabIfMissing is
            // irrelevant -- spawnClips creates only the page/engine/InsertNode and
            // never touches the clip players (verified 2026-06-03).
            mProcessor.rebuildAudioClipPlayers();

            // QA-E Task 5 (2026-05-15): the disk-drop block was created with
            // routeChannel=0 (required so THIS callback fires + spawns the
            // Clips page).  Now that the page exists, retag the block(s) so
            // they route through -- and colour as -- the Clips page instead
            // of staying stranded at routeChannel=0 (generic teal-grey).
            // Functionally a no-op for playback (routeChannel 0 and
            // audioInsert(row) both resolve to the same mixer_audio_<row>
            // insert the Clips page uses) -- this just makes the stored
            // routing explicit + correct so colouring + future explicit
            // routing both work.  Guarded on a Clips page actually existing
            // at this row (spawnClipsTabIfMissing is a no-op if the slot was
            // already taken -- in that case generic routing is correct).
            if (mPM)
            {
                bool clipsPageAtRow = false;
                for (auto* e : mPages)
                    if (e && e->type == RibbonTabBar::TabType::Clip)
                        if (auto* cp = dynamic_cast<ClipsPage*> (e->component.get()))
                            if (cp->getPageIndex() == row) { clipsPageAtRow = true; break; }

                if (clipsPageAtRow)
                {
                    const int chId = MixerChannelIds::audioInsert (row);
                    bool any = false;
                    for (int b = 0; b < mPM->getNumBlocks(); ++b)
                    {
                        auto& blk = mPM->getBlock (b);
                        if (blk.clipType    == ClipType::Audio
                            && blk.trackRow == row
                            && blk.routeChannel == 0
                            && blk.audioFilePath == filePath)
                        {
                            blk.routeChannel = chId;
                            any = true;
                        }
                    }
                    if (any && mBuilderPage)
                        mBuilderPage->notifyArrangementChanged();
                }
            }
        };
        grid->onArrangementChanged = [this]()
        {
            mProcessor.rebuildAudioClipPlayers();
        };
        // QA-TempoMap: ruler tempo-flag mutations rebuild the playhead's
        // timeline (markers ride the uniform 4-beats/bar the whole playback
        // path uses) + dirty the project.
        grid->onTempoMapChanged = [this]()
        {
            pushTempoMarkersToPlayHead();
            if (mProjectManager) mProjectManager->markDirty();
        };

        // QA-G (Split by Player Engine): resolve a (family, engine index) to
        // its ribbon tab's display name -- tab names auto-populate from
        // presets, so they are the most accurate split-pattern labels.
        grid->onGetEngineTabName = [this] (int kind, int index) -> juce::String
        {
            if (! mRibbon) return {};
            for (auto* e : mPages)
            {
                if (e == nullptr || e->component == nullptr) continue;
                int pi = -1;
                switch (kind)
                {
                    case 0: if (auto* p = dynamic_cast<LayersPage*> (e->component.get())) pi = p->getPageIndex(); break;
                    case 1: if (auto* p = dynamic_cast<BassPage*>   (e->component.get())) pi = p->getPageIndex(); break;
                    case 2: if (auto* p = dynamic_cast<DrumPage*>   (e->component.get())) pi = p->getPageIndex(); break;
                    case 3: if (auto* p = dynamic_cast<ClipsPage*>  (e->component.get())) pi = p->getPageIndex(); break;
                    case 4: if (auto* p = dynamic_cast<InstPage*>   (e->component.get())) pi = p->getPageIndex(); break;
                    default: break;
                }
                if (pi == index)
                    return mRibbon->getTabName (e->ribbonTabId);
            }
            return {};
        };
        pushTempoMarkersToPlayHead();   // initial publish (also seeds the timeline at startup)

        // QA-E Task 7 (FILE-02): enumerate every Vox/Inst/Clips page for the
        // Audio Clip Properties "Routes to:" dropdown.  channelId mapping
        // mirrors onClosePageForChannelId; display name = ribbon tab name.
        grid->onEnumerateRoutablePages = [this]() -> std::vector<RoutablePageInfo>
        {
            using namespace MixerChannelIds;
            std::vector<RoutablePageInfo> out;
            for (auto* entry : mPages)
            {
                if (! entry) continue;
                const auto* tab = mRibbon ? mRibbon->getTabById (entry->ribbonTabId)
                                          : nullptr;
                if (entry->type == RibbonTabBar::TabType::Clip)
                {
                    if (auto* cp = dynamic_cast<ClipsPage*> (entry->component.get()))
                        out.push_back ({ audioInsert (cp->getPageIndex()),
                                         tab ? tab->name
                                             : ("Clip " + juce::String (cp->getPageIndex() + 1)) });
                }
                else if (entry->type == RibbonTabBar::TabType::Vox)
                {
                    if (auto* vp = dynamic_cast<VoxPage*> (entry->component.get()))
                        out.push_back ({ voxInsert (vp->getPageIndex()),
                                         tab ? tab->name
                                             : ("Vox " + juce::String (vp->getPageIndex() + 1)) });
                }
                else if (entry->type == RibbonTabBar::TabType::Inst)
                {
                    if (auto* ip = dynamic_cast<InstPage*> (entry->component.get()))
                        out.push_back ({ instInsert (ip->getPageIndex()),
                                         tab ? tab->name
                                             : ("Inst " + juce::String (ip->getPageIndex() + 1)) });
                }
            }
            return out;
        };

        // QA-E Task 7 (FILE-02): "Add a new ___ Page" sentinel handler.
        // Vox/Inst: synchronous strip-add cascade (mirrors onAddTabRequest's
        // Vox/Inst branch).  Clip: free-row spawn backed by this clip's own
        // audio file (mirrors the duplicate-spawn "New page" free-row scan).
        // Returns the new page's MixerChannelIds channel id (or -1).
        grid->onCreateRoutablePage = [this](int kind, const juce::String& audioPath) -> int
        {
            using namespace MixerChannelIds;

            if (kind == 1 || kind == 2)   // Vox / Inst
            {
                if (! mMixerPage) return -1;
                const bool isVox = (kind == 1);
                const int  cap   = isVox ? (int) kMaxVoxPages : (int) kMaxInstPages;

                std::vector<bool> taken ((size_t) cap, false);
                for (auto* entry : mPages)
                {
                    if (! entry) continue;
                    if (isVox && entry->type == RibbonTabBar::TabType::Vox)
                    {
                        if (auto* vp = dynamic_cast<VoxPage*> (entry->component.get()))
                            if (vp->getPageIndex() >= 0 && vp->getPageIndex() < cap)
                                taken[(size_t) vp->getPageIndex()] = true;
                    }
                    else if (! isVox && entry->type == RibbonTabBar::TabType::Inst)
                    {
                        if (auto* ip = dynamic_cast<InstPage*> (entry->component.get()))
                            if (ip->getPageIndex() >= 0 && ip->getPageIndex() < cap)
                                taken[(size_t) ip->getPageIndex()] = true;
                    }
                }
                int newIdx = -1;
                for (int i = 0; i < cap; ++i)
                    if (! taken[(size_t) i]) { newIdx = i; break; }
                if (newIdx < 0)
                {
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::AlertWindow::WarningIcon,
                        isVox ? "No free Vox page" : "No free Inst page",
                        "All pages of this type are in use.  Close one before "
                        "adding another.");
                    return -1;
                }

                if (isVox) mMixerPage->addVoxChannelAtIndex  (newIdx);
                else       mMixerPage->addInstChannelAtIndex (newIdx);

                const auto wantType = isVox ? RibbonTabBar::TabType::Vox
                                            : RibbonTabBar::TabType::Inst;
                for (auto* entry : mPages)
                {
                    if (! entry || entry->type != wantType) continue;
                    int idx = -1;
                    if (isVox) { if (auto* vp = dynamic_cast<VoxPage*> (entry->component.get())) idx = vp->getPageIndex(); }
                    else       { if (auto* ip = dynamic_cast<InstPage*> (entry->component.get())) idx = ip->getPageIndex(); }
                    if (idx == newIdx && mRibbon)
                    {
                        mRibbon->selectTab (entry->ribbonTabId);
                        onTabSelected (entry->ribbonTabId);
                        break;
                    }
                }
                return isVox ? voxInsert (newIdx) : instInsert (newIdx);
            }

            if (kind == 0)   // Clip
            {
                if (audioPath.isEmpty()) return -1;
                int newRow = -1;
                for (int i = 0; i < kMaxClipPages; ++i)
                {
                    bool taken = false;
                    for (auto* e : mPages)
                    {
                        if (e && e->type == RibbonTabBar::TabType::Clip)
                            if (auto* cp = dynamic_cast<ClipsPage*> (e->component.get()))
                                if (cp->getPageIndex() == i) { taken = true; break; }
                    }
                    if (! taken) { newRow = i; break; }
                }
                if (newRow < 0)
                {
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::AlertWindow::WarningIcon,
                        "No free Clips page",
                        "All " + juce::String (kMaxClipPages) + " Clips pages "
                        "are in use.  Close one before adding another.");
                    return -1;
                }
                // QA-EffectsReview side-fix (2026-06-06): canonical helper so the
                // routable Clip page gets its mixer strip + InsertNode -- the bare
                // spawnClipsTabIfMissing made the page only, so the audioInsert()
                // channel returned below had no node to route to (and no strip).
                createClipStripAndPage (newRow, audioPath, /*allowDuplicate*/ true);

                for (auto* entry : mPages)
                {
                    if (! entry || entry->type != RibbonTabBar::TabType::Clip) continue;
                    if (auto* cp = dynamic_cast<ClipsPage*> (entry->component.get()))
                        if (cp->getPageIndex() == newRow && mRibbon)
                        {
                            mRibbon->selectTab (entry->ribbonTabId);
                            onTabSelected (entry->ribbonTabId);
                            break;
                        }
                }
                return audioInsert (newRow);
            }

            return -1;
        };

        // QA-E Task 7 (FILE-02): "Copy" action, split in two so "Copy to a
        // new Clip Page" can't double-register.  onDuplicateFileForCopy ONLY
        // makes the auto-numbered physical copy (no library entry, no page),
        // so the caller can duplicate FIRST and then create the new Clips
        // page bound to the DUPLICATE -- the Clips-page spawn's auto-register
        // then IS the single entry, and onTagCopiedEntry's add dedups to a
        // no-op.  For existing / Vox / Inst targets onTagCopiedEntry creates
        // the one entry.
        grid->onDuplicateFileForCopy = [this](const juce::String& src) -> juce::String
        {
            if (! mProjectManager) return {};
            const juce::File srcAbs = mProcessor.resolveProjectFile (src);
            if (! srcAbs.existsAsFile()) return {};
            return mProjectManager->duplicateSample (srcAbs);   // np or {}
        };
        grid->onTagCopiedEntry = [this](const juce::String& np, int targetChannel,
                                        float pitch, float bpm, bool stretch)
        {
            if (! mPM || np.isEmpty()) return;
            // addAudioToLibrary dedups on (path, owner): if the Clips-page
            // spawn already registered (np, targetChannel) this is a no-op.
            mPM->addAudioToLibrary (np, {}, targetChannel);
            const int idx = mPM->findAudioLibraryIndexByPath (np);
            if (idx >= 0)
                mPM->setAudioLibraryClipDefaults (idx, pitch, bpm, stretch);
            if (mBuilderPage) mBuilderPage->notifyArrangementChanged();
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
            ClipDropDiag::log ("onImportSampleRequest: NO PROJECT OPEN",
                               "src=" + src.getFullPathName() + " (copy skipped; New-Project prompt follows. "
                               "If you DO have a project open and still see this, that IS the 2nd case - flag it.)");
            return {};
        };
        // P4: path resolver for stored (possibly relative) audioFilePath.
        grid->onResolveStoredPath = [this](const juce::String& stored)
        {
            return mProcessor.resolveProjectFile (stored);
        };
        // QA-Ea Task 0c (FL pre-roll record + non-destructive clip trim):
        // wire the three slip-edit callbacks.  Project tempo, project sample
        // rate, and audio-clip player rebuild request after each drag delta
        // so the audio engine immediately reflects the new contentStartSamples
        // / lengthBeats / startBeats.  See ArrangementGrid::mSlipEditing for
        // the slip-in-point semantics.  (The slip-edit-mode read used to be
        // a callback to mSlipEditMode; replaced 2026-05-20 by the dropdown's
        // internal EditMode -- no callback needed anymore.)
        grid->onGetBPM = [this]() -> double {
            return mTransport ? mTransport->getBPM() : 120.0;
        };
        grid->onGetSampleRate = [this]() -> double {
            const double sr = mProcessor.getSampleRate();
            return sr > 0.0 ? sr : 44100.0;
        };
        grid->onRequestRebuildPlayers = [this]() {
            mProcessor.rebuildAudioClipPlayers();
            if (mProjectManager) mProjectManager->markDirty();
        };
        // QA-Ee Stage 2 (Builder snap): the grid reads the unified snap-division
        // index live for snap + grid rendering; the combo writes it back.  Mirrors
        // the Unified_RecordQuantizeDiv getter/setter pattern (default 1 = Line).
        grid->onGetSnapDiv = [this]() -> int {
            if (auto* p = mProcessor.apvts.getRawParameterValue ("Unified_BuilderSnapDiv"))
                return (int) p->load();
            return 1;
        };
        grid->onSnapDivChanged = [this](int div) {
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                    mProcessor.apvts.getParameter ("Unified_BuilderSnapDiv")))
                p->setValueNotifyingHost (
                    p->getNormalisableRange().convertTo0to1 ((float) div));
        };
        if (mBuilderPage) mBuilderPage->syncSnapComboFromParam();   // show the param's value in the combo
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
        // QA-E Task 5 (2026-05-15): disk drop of a file already in the library
        // -> "Use existing routing / New page / Cancel" prompt.  Existing
        // routing calls placeAudioLibraryEntry (no new entry, no new page).
        // New page spawns a fresh Clips tab (allowDuplicate=true), adds a
        // second library entry tagged to the new page's channelId, then
        // places a block routed to that new entry.
        grid->onDuplicateFileDrop = [this, grid](const juce::File& dropped,
                                                  int libIdx, int row, float bar)
        {
            if (! mPM) return;
            const juce::String path          = mPM->getAudioLibraryPath (libIdx);
            const int          existingOwner = mPM->getAudioLibraryPageOwner (libIdx);

            // Resolve the owning page's display name for the prompt.
            juce::String pageName = "an existing page";
            for (auto* entry : mPages)
            {
                if (! entry) continue;
                if (entry->type != RibbonTabBar::TabType::Clip) continue;
                if (auto* cp = dynamic_cast<ClipsPage*> (entry->component.get()))
                {
                    const int chId = MixerChannelIds::audioInsert (cp->getPageIndex());
                    if (chId == existingOwner)
                    {
                        pageName = cp->getTabName();
                        break;
                    }
                }
            }

            auto* aw = new juce::AlertWindow (
                "File Already in Library",
                "\"" + dropped.getFileName() + "\" is already in your library on \""
                  + pageName + "\".\n\n"
                  "Use existing routing, or create a new page and strip?",
                juce::AlertWindow::QuestionIcon);
            aw->addButton ("Use Existing", 1);
            aw->addButton ("New Page",     2);
            aw->addButton ("Cancel",       0);

            juce::Component::SafePointer<StandaloneEditor> safeThis (this);
            aw->enterModalState (true, juce::ModalCallbackFunction::create (
                [safeThis, grid, path, libIdx, row, bar] (int result)
                {
                    if (! safeThis) return;
                    auto* self = safeThis.getComponent();
                    if (! self || ! grid) return;

                    if (result == 1)
                    {
                        // Existing routing: drop block routed to existing entry.
                        grid->placeAudioLibraryEntry (libIdx, row, bar);
                    }
                    else if (result == 2)
                    {
                        // New page: find next free Clips page row.
                        int newPageRow = -1;
                        for (int i = 0; i < kMaxClipPages; ++i)
                        {
                            bool taken = false;
                            for (auto* e : self->mPages)
                            {
                                if (e && e->type == RibbonTabBar::TabType::Clip)
                                    if (auto* cp = dynamic_cast<ClipsPage*> (e->component.get()))
                                        if (cp->getPageIndex() == i) { taken = true; break; }
                            }
                            if (! taken) { newPageRow = i; break; }
                        }
                        if (newPageRow < 0)
                        {
                            juce::AlertWindow::showMessageBoxAsync (
                                juce::AlertWindow::WarningIcon,
                                "No free Clips page",
                                "All " + juce::String (kMaxClipPages) + " Clips pages are in "
                                "use.  Close one before adding another.");
                            return;
                        }

                        // QA-E Task 7 (FILE-02): "New Page" must NOT create a
                        // second identical-path library entry (the old dupe
                        // bug).  Force an auto-numbered physical duplicate so
                        // the new page is backed by a DISTINCT file with its
                        // own single source-of-truth entry.
                        const juce::File srcAbs =
                            self->mProcessor.resolveProjectFile (path);
                        const juce::String newStored =
                            (self->mProjectManager && srcAbs.existsAsFile())
                                ? self->mProjectManager->duplicateSample (srcAbs)
                                : juce::String();
                        if (newStored.isEmpty())
                        {
                            juce::AlertWindow::showMessageBoxAsync (
                                juce::AlertWindow::WarningIcon,
                                "Couldn't duplicate file",
                                "The file could not be copied for the new page.");
                            return;
                        }

                        // QA-EffectsReview side-fix (2026-06-06): create the strip
                        // AND the page via the canonical helper -- the old bare
                        // spawnClipsTabIfMissing made the page but skipped the
                        // mixer-strip trio, so "New Page" yielded a strip-less page.
                        self->createClipStripAndPage (newPageRow, newStored, /*allowDuplicate*/ true);

                        const int newCh = MixerChannelIds::audioInsert (newPageRow);
                        if (self->mPM)
                            self->mPM->addAudioToLibrary (newStored, {}, newCh);

                        int newLibIdx = -1;
                        for (int i = self->mPM->getNumAudioLibrary() - 1; i >= 0; --i)
                        {
                            if (self->mPM->getAudioLibraryPath (i) == newStored
                                && self->mPM->getAudioLibraryPageOwner (i) == newCh)
                            {
                                newLibIdx = i;
                                break;
                            }
                        }
                        if (newLibIdx >= 0)
                            grid->placeAudioLibraryEntry (newLibIdx, row, bar);
                    }
                    // result == 0 -> Cancel: no-op.
                }),
                true);
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

        // QA-E Task 7 (FILE-02): browser-entry "Properties..." routing.
        // Reuse the grid's enumerate/create callbacks (wired above in the
        // page->getGrid() block, which runs before this one) so the per-clip
        // and per-library "Routes to:" dialogs list identical targets -- no
        // logic duplication.
        if (auto* g = page->getGrid())
        {
            panel->onEnumerateRoutablePages = g->onEnumerateRoutablePages;
            panel->onCreateRoutablePage     = g->onCreateRoutablePage;
            panel->onDuplicateFileForCopy   = g->onDuplicateFileForCopy;
            panel->onTagCopiedEntry         = g->onTagCopiedEntry;
        }
        // onApplyLibraryProperties = the source-of-truth edit.  Write pitch /
        // BPM / stretch-mode + route onto the library entry, then propagate
        // ALL FOUR into every grid copy still FOLLOWING the original
        // (isOverride == false).  Copies the user customized individually are
        // detached (isOverride == true) and left untouched.  Mirrors the QA-E
        // Task 4 follower-retag pattern; notifyArrangementChanged() does the
        // grid repaint + rebuildAudioClipPlayers + dirty-flag in one call.
        panel->onApplyLibraryProperties = [this](int libIdx, float newPitch,
                                                 float newBPM, bool newStretch,
                                                 int newRoute)
        {
            if (! mPM) return;
            if (libIdx < 0 || libIdx >= mPM->getNumAudioLibrary()) return;
            mPM->setAudioLibraryPageOwner    (libIdx, newRoute);
            mPM->setAudioLibraryClipDefaults (libIdx, newPitch, newBPM, newStretch);

            const juce::File libFile =
                mProcessor.resolveProjectFile (mPM->getAudioLibraryPath (libIdx));
            for (int b = 0; b < mPM->getNumBlocks(); ++b)
            {
                auto& blk = mPM->getBlock (b);
                if (blk.clipType != ClipType::Audio) continue;
                if (blk.isOverride)                  continue;   // detached copy
                if (mProcessor.resolveProjectFile (blk.audioFilePath) == libFile)
                {
                    // Follower inherits ALL source-of-truth props.
                    blk.pitchSemitones = newPitch;
                    blk.originalBPM    = juce::jmax (1.f, newBPM);
                    blk.stretchMode    = newStretch;
                    blk.routeChannel   = newRoute;
                }
            }
            if (mBuilderPage) mBuilderPage->notifyArrangementChanged();
        };

        // Owner call 2026-07-11: the grid's per-clip Move shares this exact
        // lambda so grid + browser Move are one code path (linked).
        if (auto* g = page->getGrid())
            g->onApplyLibraryProperties = panel->onApplyLibraryProperties;

        // QA-E Task 5 (2026-05-15): browser Delete on the LAST library entry
        // owned by a page -> close that page's ribbon tab.  Walk mPages,
        // match by (page type, page index -> channelId), close the tab.
        // closeTab fires the existing onTabClosed cascade which handles
        // engine teardown + mixer strip removal + InsertNode unregister.
        panel->onClosePageForChannelId = [this](int channelId)
        {
            using namespace MixerChannelIds;
            int ribbonTabId = -1;

            for (auto* entry : mPages)
            {
                if (! entry) continue;
                if (entry->type == RibbonTabBar::TabType::Clip)
                {
                    if (auto* cp = dynamic_cast<ClipsPage*> (entry->component.get()))
                        if (audioInsert (cp->getPageIndex()) == channelId)
                        { ribbonTabId = entry->ribbonTabId; break; }
                }
                else if (entry->type == RibbonTabBar::TabType::Vox)
                {
                    if (auto* vp = dynamic_cast<VoxPage*> (entry->component.get()))
                        if (voxInsert (vp->getPageIndex()) == channelId)
                        { ribbonTabId = entry->ribbonTabId; break; }
                }
                else if (entry->type == RibbonTabBar::TabType::Inst)
                {
                    if (auto* ip = dynamic_cast<InstPage*> (entry->component.get()))
                        if (instInsert (ip->getPageIndex()) == channelId)
                        { ribbonTabId = entry->ribbonTabId; break; }
                }
            }

            if (ribbonTabId >= 0 && mRibbon)
                mRibbon->closeTab (ribbonTabId);
        };

        // G-5 (2026-04-29): page-walk based enumeration of audio files for
        // the unified Audio tree.  Mutually-exclusive categories (Clips /
        // Vox / Inst); orphan audioLibrary entries (no bound page) are not
        // emitted.  Vox + Inst entries only fire once recording-to-file
        // ships in G-9 - until then their getClipFilePath() returns empty
        // and we skip.  Helper resolves an audio file path to its global
        // audioLibrary index (drag descriptor needs the index, not the path).
        // G-6 (2026-04-29): Duplicate... right-click flow.  BrowserPanel has
        // already (a) copied the WAV to the destination + (b) resolved any
        // filename conflict.  This callback is the back-half: capture the
        // source page's full state, spawn a new page on the copied file,
        // then apply the captured state to the new page so engine choice +
        // every knob + both A/B engines' APVTS are cloned exactly.
        //
        // Target row = first arrangement-grid row with no Audio block
        // (avoids stacking blocks at bar 0); falls back to row 0 if all 32
        // rows are populated.
        panel->onDuplicateClipSpawn = [this](const juce::String& sourceAbsPath,
                                              const juce::String& copiedAbsPath)
        {
            if (copiedAbsPath.isEmpty() || ! mBuilderPage || ! mPM) return;
            auto* grid = mBuilderPage->getGrid();
            if (! grid) return;

            // ── Step 1: locate the source page by absolute path + capture
            // its full state.  Three possible page types (Clips / Vox /
            // Inst) - each has its own export* method.  We hold the saved
            // state as XML strings so we can apply after the new page exists.
            // QA-E Task 4 (2026-05-12): Vox/Inst branches removed -- those
            // pages no longer hold mClipPath (file-association moved to
            // library entries via pageOwnerChannelId).  The downstream
            // duplicate-spawn at line ~2207 only consumes savedClipState
            // anyway; Vox/Inst savedState was captured but never applied
            // (per the deferred-to-G-9 note that originally lived here).
            // When Vox/Inst duplicate UX lands, it would loop the audio
            // library by ownerChannelId + the source path, not per-page.
            juce::String savedClipState;
            for (auto* entry : mPages)
            {
                if (! entry || ! entry->component) continue;
                if (auto* cp = dynamic_cast<ClipsPage*> (entry->component.get()))
                {
                    const juce::String pageAbs =
                        mProcessor.resolveProjectFile (cp->getClipFilePath()).getFullPathName();
                    if (pageAbs == sourceAbsPath || cp->getClipFilePath() == sourceAbsPath)
                    {
                        savedClipState = cp->exportClipState();
                        break;
                    }
                }
            }

            // ── Step 2: find first arrangement row with no Audio block,
            // then spawn the new ClipsPage via the canonical import path.
            // ClipsPage spawn is the only auto-spawning page right now; Vox
            // / Inst would land here once G-9 wires recording-to-file but
            // those don't have an "import via Builder" path yet, so the
            // Vox/Inst branches below stay null at this stage.
            constexpr int kMaxRows = ArrangementGrid::kNumRows;
            std::array<bool, kMaxRows> rowHasAudio {};
            for (int i = 0; i < mPM->getNumBlocks(); ++i)
            {
                const auto& b = mPM->getBlock (i);
                if (b.clipType == ClipType::Audio
                    && b.trackRow >= 0 && b.trackRow < kMaxRows)
                    rowHasAudio[(size_t) b.trackRow] = true;
            }
            int targetRow = 0;
            for (int r = 0; r < kMaxRows; ++r)
                if (! rowHasAudio[(size_t) r]) { targetRow = r; break; }

            grid->importAudioFile (copiedAbsPath, targetRow, 0.0f);

            // ── Step 3: importAudioFile triggered onAudioClipAdded →
            // spawnClipsTabIfMissing(targetRow, ...).  The new ClipsPage is
            // now in mPages.  Apply the captured state to clone every knob
            // + engine selection + both A/B engines' APVTS.
            if (savedClipState.isNotEmpty())
            {
                for (auto* entry : mPages)
                {
                    if (! entry || ! entry->component) continue;
                    if (auto* newCp = dynamic_cast<ClipsPage*> (entry->component.get()))
                    {
                        if (newCp->getPageIndex() == targetRow)
                        {
                            newCp->importClipState (savedClipState);
                            break;
                        }
                    }
                }
            }
            // VoxPage / InstPage duplicate paths defer to G-9 - Vox/Inst
            // recordings don't exist yet and the current G-6 audio tree
            // only surfaces Clips entries.  When G-9 ships, these branches
            // will spawn on the matching mixer strip + apply saved state.
        };

        // QA-Fe2 De-noise: strength re-clean from the browser tree menu.
        panel->onRegenerateDenoise = [this] (const juce::String& relPath, int strength)
        {
            return regenerateDenoise (relPath, strength);
        };

        // QA-Fe2: recording-group disk-rename flow.
        panel->onRenameRecordingGroup = [this] (const juce::String& oldBase,
                                                const juce::String& newBase)
        {
            return renameRecordingGroup (oldBase, newBase);
        };

        // TS7 §11: where the project's rendered files live.  The panel does not
        // know about projects, so the editor supplies both folders.
        panel->onGetRenderFolders = [this]() -> std::pair<juce::File, juce::File>
        {
            return { mProcessor.getProjectExportsDir(),
                     mProcessor.getProjectReportsDir() };
        };

        // TS7 §11.6: a saved report opens IN THE APP, in the analyzer -- the
        // same view the user watched live -- by replaying the data block the
        // writer embedded in the HTML.  That block is why the report is not
        // just a picture: verdicts are recomputed against the CURRENT
        // LoudnessSpec table on open, so an old report re-read after a target
        // changes reflects the new target rather than the one frozen into the
        // page.  A file carrying no block is refused rather than opened empty.
        panel->onOpenReport = [this] (const juce::File& f)
        {
            BuilderPage::MeasureResult    m;
            LoudnessReportWriter::Context ctx;
            if (! LoudnessReportWriter::readEmbedded (f, m, ctx))
            {
                juce::AlertWindow::showMessageBoxAsync (
                    juce::AlertWindow::WarningIcon,
                    "Cannot open report",
                    "This file has no BaySickDAW measurement data in it, so "
                    "there is nothing for the analyzer to show.\n\n"
                    "Reports written by this app always carry it; one that has "
                    "been edited and re-saved by another program may not.");
                return;
            }
            openMasterAnalyzerWindow();
            if (auto* win = findAuxWindow ("analyzer:master"))
                if (auto* v = dynamic_cast<MasterAnalyzerView*> (win->getContent()))
                    v->showRenderedCurve (m.lufsCurve, m.integratedLufs, m.lraLu,
                                          m.truePeakDb,
                                          ctx.scopeLabel.isNotEmpty()
                                              ? ctx.scopeLabel
                                              : f.getFileNameWithoutExtension());
        };

        panel->onEnumerateAudio = [this]() -> std::vector<CategorizedAudioEntry>
        {
            std::vector<CategorizedAudioEntry> out;
            if (! mPM) return out;

            // QA-E Task 4 (2026-05-12): library-driven enumeration.  Walk
            // every AudioLibraryEntry once + group by pageOwnerChannelId
            // range.  Replaces the old per-page mClipPath round-trip --
            // Vox/Inst pages no longer hold mClipPath; their files live in
            // the library tagged via pageOwnerChannelId.  Clips still has
            // mClipPath (engine preload) but its drag handler now ALSO
            // tags the library so multi-file Clips browser visibility works.
            // See §9 17th Forks entry for the architectural rationale.
            for (int libIdx = 0; libIdx < mPM->getNumAudioLibrary(); ++libIdx)
            {
                const int owner          = mPM->getAudioLibraryPageOwner (libIdx);
                const juce::String path  = mPM->getAudioLibraryPath  (libIdx);
                const juce::String alias = mPM->getAudioLibraryAlias (libIdx);

                CategorizedAudioEntry e;
                e.audioLibIdx = libIdx;
                e.fullPath    = mProcessor.resolveProjectFile (path).getFullPathName();
                if (e.fullPath.isEmpty()) e.fullPath = path;
                e.displayName = alias.isNotEmpty() ? alias : juce::File (path).getFileName();
                e.groupName   = mPM->getAudioLibraryGroup (libIdx);   // QA-Fe2

                if (owner >= MixerChannelIds::kVoxBase
                    && owner <  MixerChannelIds::kVoxBase + MixerChannelIds::kMaxVoxStrips)
                {
                    e.category = "Vox";
                    e.accent   = juce::Colour (0xff0fafa5);
                }
                else if (owner >= MixerChannelIds::kInstBase
                         && owner <  MixerChannelIds::kInstBase + MixerChannelIds::kMaxInstStrips)
                {
                    e.category = "Inst";
                    e.accent   = juce::Colour (0xff1c3a8a);
                }
                else if (owner >= MixerChannelIds::kAudioBase
                         && owner <  MixerChannelIds::kAudioBase + 50)
                {
                    e.category = "Clips";
                    e.accent   = juce::Colour (0xffd4a017);
                }
                else
                {
                    e.category = "Audio";
                    e.accent   = juce::Colour (0xff808080);
                }
                out.push_back (std::move (e));
            }
            return out;
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
        {
            pane->onResolveDisplayName = content->onResolveDisplayName;
            // Batch E #3 (2026-05-01): stale-lane detector.  Pane uses this to
            // dim/red rows whose target paramId no longer exists in APVTS.
            pane->onIsParamStale = [this](const juce::String& pid) -> bool
            {
                if (pid.isEmpty()) return false;
                // QA-ModelShell TS3 (2026-07-27): RE-WIDENED to "not a main-APVTS
                // param AND not in the registry", which is what QA-ProjectSave
                // Task 7 step 2 tried and had to revert on 2026-07-26 when Jeff
                // saw every effects-rack lane tagged "deleted".
                //
                // That revert was right at the time: applicators were keyed to
                // PANELS, so a rack lane was unregistered whenever its channel
                // was not the one the Effects page happened to be showing -- a
                // normal viewing state, not a dead lane.  The tag was accurate
                // about the registry and a lie about the user's automation.
                //
                // It is honest now.  Registration is model-side and lasts as
                // long as the thing it targets, so "not registered" genuinely
                // means the target is gone -- a deleted tab, a cleared rack
                // slot, a removed pedal.  Without the widening, exactly those
                // lanes look alive.
                if (mProcessor.apvts.getParameter (pid) != nullptr) return false;
                return mAutomationApplicators.find (pid) == mAutomationApplicators.end();
            };
        }

        // QA-ProjectSave (2026-07-26, Jeff): an Event Editor edit has to repaint
        // the Builder grid too -- both draw the same lane, and before this the
        // arrangement block kept its old shape until the user navigated away and
        // back, which read as the edit not having taken.
        content->onLaneEdited = [this]
        {
            if (mBuilderPage)
                if (auto* g = mBuilderPage->getGrid())
                    g->repaint();
        };

        // Last point removed in the Event Editor -> same prompt the Builder grid
        // asks, and the same outcome, so the two routes cannot diverge.
        if (auto* grid = content->getGrid())
        {
            grid->onDeleteWholeAutomationRequested = [this, blockIdx]
            {
                if (mBuilderPage)
                    if (auto* g = mBuilderPage->getGrid())
                        g->promptDeleteWholeAutomation (blockIdx);
            };
        }

        // QA-Ed (Problem 1): value format/parse hooks on the grid -- the editor
        // reads out + accepts the param's REAL units.  Per the JUCE tip, APVTS
        // lanes use the parameter's own getText()/getValueForText() (identical
        // math to its knob + host automation lanes); "global_tempo" is the one
        // non-APVTS lane (project tempo range 20..300 BPM, as in the applicator).
        if (auto* grid = content->getGrid())
        {
            grid->onFormatValue = [this](const juce::String& pid, float v01) -> juce::String
            {
                if (auto* p = mProcessor.apvts.getParameter(pid))
                    return p->getText(juce::jlimit(0.0f, 1.0f, v01), 24);
                if (pid == "global_tempo")
                    return juce::String(juce::roundToInt(20.0 + (double) juce::jlimit(0.0f, 1.0f, v01) * (300.0 - 20.0)))
                           + " BPM";
                return juce::String(v01, 3);
            };
            grid->onParseValue = [this](const juce::String& pid, const juce::String& text) -> float
            {
                if (auto* p = mProcessor.apvts.getParameter(pid))
                    return juce::jlimit(0.0f, 1.0f, p->getValueForText(text));
                if (pid == "global_tempo")
                {
                    const double bpm = juce::jlimit(20.0, 300.0, (double) text.getFloatValue());
                    return (float) juce::jlimit(0.0, 1.0, (bpm - 20.0) / (300.0 - 20.0));
                }
                return juce::jlimit(0.0f, 1.0f, text.getFloatValue());
            };
        }
    }

    mEventEditors.add(ed);
}

void StandaloneEditor::applyAutomationAtCurrentPosition()
{
    // Audio thread may have requested transport stop (end of song in play-through mode).
    // Handle on the UI thread so we can safely call stopPlayback().
    if (mProcessor.mRequestStop.exchange(false, std::memory_order_acq_rel))
    {
        // QA-Ea Task 0b (2026-05-18): song-end auto-stop must behave
        // exactly like pressing Stop, including finalizing any active
        // recording.  Was stopPlayback-only -> the recorder kept writing
        // silence past song end until the user hit Stop manually.  Forks #25.
        if (mPlayHead.isPlaying() || mRecordingActive)
            stopTransportAndFinalizeRecording();
    }

    if (!mPM) return;

    // Smoke round 3 (Jeff): SONG MODE ONLY -- same gate as the audio-thread
    // pass.  This UI pass covers the stopped seek/scrub preview (APVTS lanes)
    // AND the applicator lanes (engine params + global_tempo), which
    // previously ran in EVERY mode on every tick: the second writer that
    // kept automation driving pattern-mode playback after the processor
    // pass was gated.
    if (! mProcessor.isSongMode()) return;

    const double beatsPerBar   = 4.0;   // TODO: read from PatternManager time signature
    const double currentBeats  = mPlayHead.getCurrentBeat();
    const bool   playing       = mPlayHead.isPlaying();

    // QA-Ed (Problem 3): apply automation whenever the playhead POSITION changes
    // -- continuously during playback, OR a one-shot when you seek/scrub/place the
    // playhead while stopped -- so any param sitting on an active automation snaps
    // to that automation's value at the playhead's beat.  Skip only when stopped
    // AND static, so a hand-nudged value isn't re-overridden every timer tick.
    if (! playing && std::abs (currentBeats - mLastAutomationBeat) < 1.0e-6) return;
    mLastAutomationBeat = currentBeats;

    for (int i = 0; i < mPM->getNumBlocks(); ++i)
    {
        const auto& block = mPM->getBlock(i);
        if (block.clipType != ClipType::Automation) continue;
        if (block.muted) continue;

        // Review fix: use effectiveLengthBars (sub-bar clip spans) so the
        // stopped/seek preview windows match the audio-thread evaluator.
        const double blockStart     = effectiveStartBeats (block);
        const double clipLenBeats   = effectiveLengthBars (block) * beatsPerBar;
        const double blockEnd       = blockStart + clipLenBeats;

        if (currentBeats < blockStart || currentBeats >= blockEnd) continue;

        const auto& lane = block.automationLane;
        if (lane.paramId.isEmpty()) continue;

        // Normalised position within clip (0..1)
        const float pos01 = (float)((currentBeats - blockStart) / clipLenBeats);
        const float value = lane.evaluateAt(pos01);

        // APVTS-backed lanes: during playback the audio-thread automation pass
        // (PluginProcessor::processBlock) writes these, so skip here to avoid a
        // double write.  But that pass only runs WHILE PLAYING -- so when STOPPED
        // we apply them here, so seeking/placing the playhead reflects the
        // automation on every automated knob, not just the tempo (Problem 3).
        if (auto* rap = mProcessor.apvts.getParameter(lane.paramId))
        {
            if (! playing)
                rap->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, value));
            continue;
        }

        // Non-APVTS lane (e.g. "global_tempo"): mutates the PlayHead +
        // PatternManager directly via the applicator; runs on the UI thread.
        auto it = mAutomationApplicators.find(lane.paramId);
        if (it != mAutomationApplicators.end() && it->second)
        {
            it->second(value);
        }
        else
        {
            // QA-ProjectSave Task 7 step 2: an unresolvable lane used to be
            // indistinguishable from a working one -- find() missed (or the
            // entry was a dead no-op) and the tick simply did nothing.  Say so
            // once per paramId per session: loud enough to catch in Debug, quiet
            // enough not to spam a 30 Hz automation tick.
            // The lane is deliberately NOT deleted -- Ardour and Tracktion both
            // keep automation data alive independent of the live control, so a
            // target that comes back (tab reopened, panel rebuilt) re-binds.
            // jassertfalse REMOVED 2026-07-26: it did its job (confirmed the
            // FX-rack hole on Jeff's first test) but until step 3 lands it fires
            // on the ordinary act of viewing a different channel's FX page, which
            // makes every Debug session a dialog fight.  Log only for now;
            // restore the assert once step 3 removes the false-positive source.
            if (! mReportedDeadLanes.contains (lane.paramId))
            {
                mReportedDeadLanes.add (lane.paramId);
                DBG ("[automation] lane targets nothing: " + lane.paramId);
            }
        }
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
            maxEnd = std::max(maxEnd, (int) std::ceil(effectiveStartBars(b) + (double) b.lengthBars - 1e-9));
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
    block.startBeats     = (double) newStart * 4.0;
    block.lengthBars     = newLength;
    block.patternIndex   = mPM->getCurrentPatternIndex();
    block.layerTrack     = false;
    block.automationLane.paramId = paramId;
    // Capture the live-resolved label while the target exists: deleted-slot
    // lanes fall back to it (the UUID never revives, so it stays correct).
    block.automationLane.lastKnownName = resolveAutomationDisplayName(paramId);

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
// Naming convention - every label starts with one of two prefixes so the
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
// Drum-slot engine params (tk_drm_N_s{S}_*) - intentionally NOT prefixed;
// labelled as "{Drums tab name} Slot {S+1} - {engine} - {param}" (no Mx/Pg
// prefix since drum slots span both surfaces).  §P4.3 B7 removed the legacy
// drums_{mid,side}_eq* block - per-slot pre-rack EQ now resolves through the
// mixer_drum_{N}_preeq_* form handled by tryMixerNonSlot.
//
// If parsing fails entirely, returns the paramId unchanged so old presets and
// edge cases never show blank labels.
// ─────────────────────────────────────────────────────────────────────────────
juce::String StandaloneEditor::resolveAutomationDisplayName(const juce::String& paramId) const
{
    if (paramId.isEmpty()) return paramId;

    // Plugins-tab instrument lanes: "plugtab<N>_vst_<paramId>" (2026-08-02).
    // The raw plugin parameter id must never surface -- resolve it to the
    // plugin's own display name, fronted by the tab's current name.
    if (paramId.startsWith ("plugtab"))
    {
        const juce::String rest = paramId.substring (7);
        const int us = rest.indexOfChar (0, '_');
        if (us > 0 && rest.substring (0, us).containsOnly ("0123456789")
            && rest.substring (us + 1).startsWith ("vst_"))
        {
            const int idx = rest.substring (0, us).getIntValue();
            const juce::String pid = rest.substring (us + 1 + 4);
            juce::String tabLabel = "Plugin " + juce::String (idx + 1);
            juce::String prmLabel = pid;
            for (auto* entry : mPages)
                if (entry != nullptr && entry->type == RibbonTabBar::TabType::Plugins)
                    if (auto* pp = dynamic_cast<PluginsPage*> (entry->component.get()))
                        if (pp->getPageIndex() == idx)
                        {
                            if (const auto* tab = mRibbon ? mRibbon->getTabById (entry->ribbonTabId) : nullptr)
                                tabLabel = tab->name;
                            break;
                        }
            if (auto* inst = dynamic_cast<Hosting::HostedPluginInstance*> (
                    const_cast<VibeSynthProcessor&> (mProcessor).engineRig()
                        .engineFor (TabKind::Plugins, idx)))
                for (const auto& p : inst->getAutomatableParams())
                    if (p.id == pid) { prmLabel = p.name; break; }
            return tabLabel + " - " + prmLabel;
        }
    }

    // QA-ApvtsAutomation: per-instance engine keys carry the owning page in the id
    // ("inst{N}_" / "vox{N}_") so lanes stay distinct across tabs.  Strip that,
    // resolve the remainder normally, and put the tab back on the front -- the
    // user should see "Inst 4 - ..." rather than a raw registry key.
    {
        auto stripInstance = [] (const juce::String& id, const char* tag,
                                 const char* label, juce::String& outLabel,
                                 juce::String& outRest) -> bool
        {
            const juce::String prefix (tag);
            if (! id.startsWith (prefix)) return false;
            const int us = id.indexOfChar (prefix.length(), '_');
            if (us < 0) return false;
            const juce::String num = id.substring (prefix.length(), us);
            if (num.isEmpty() || ! num.containsOnly ("0123456789")) return false;
            outLabel = juce::String (label) + " " + juce::String (num.getIntValue() + 1);
            outRest  = id.substring (us + 1);
            return true;
        };

        juce::String instLabel, rest;
        if (stripInstance (paramId, "inst", "Inst", instLabel, rest)
         || stripInstance (paramId, "vox",  "Vox",  instLabel, rest))
        {
            // Pedals keys embed the slot's uuid so a lane survives reordering.
            // That uuid is an implementation detail and must never surface.
            if (rest.startsWith ("pedals_"))
            {
                juce::String tail = rest.substring (7);
                const int us2 = tail.indexOfChar (0, '_');
                if (us2 == 32) tail = tail.substring (us2 + 1);
                return instLabel + " - Pedals - " + tail.replaceCharacter ('_', ' ');
            }
            return instLabel + " - " + resolveAutomationDisplayName (rest);
        }
    }

    using Kind = VibeGraph::InsertKind;

    auto& vg = const_cast<VibeSynthProcessor&>(mProcessor).mVibeGraph;

    // C13 (2026-04-30): UUID-keyed slot paramIds.  setSlotContext now stamps
    // "<channelPrefix>_<32hex>_<knob>" instead of "<channelPrefix>_s<N>_<knob>"
    // so reorder/pack-to-top doesn't break automation lanes.  Translate the
    // UUID back to the live slot index before the user-facing renderer below
    // sees it -- the user should keep seeing "Mx Layers Bus - Chorus - Wet Dry"
    // exactly as before, with no UUID ever surfacing.
    auto isUuidToken = [](const juce::String& s) -> bool
    {
        if (s.length() != 32) return false;
        for (int i = 0; i < 32; ++i)
        {
            const auto c = s[i];
            const bool hex = juce::CharacterFunctions::isDigit((char) c)
                          || (c >= 'a' && c <= 'f');
            if (! hex) return false;
        }
        return true;
    };
    auto splitByUuid = [&isUuidToken](const juce::String& id,
                                       juce::String& outBase,
                                       juce::String& outUuid,
                                       juce::String& outParam) -> bool
    {
        int us = id.indexOfChar(0, '_');
        while (us >= 0)
        {
            const int next = id.indexOfChar(us + 1, '_');
            if (next < 0) return false;
            const juce::String token = id.substring(us + 1, next);
            if (isUuidToken(token))
            {
                outBase  = id.substring(0, us);
                outUuid  = token;
                outParam = id.substring(next + 1);
                return true;
            }
            us = next;
        }
        return false;
    };
    auto findRackForBase = [&vg](const juce::String& b) -> EffectRack*
    {
        if (b == "layers_bus" || b == "mixer_layers")     return vg.getLayersBusRack();
        if (b == "bass_bus"   || b == "mixer_bass")       return vg.getBassBusRack();
        if (b == "drums_bus"  || b == "mixer_drums")      return vg.getDrumsBusRack();
        if (b == "master"     || b == "mixer_master")     return vg.getMasterRack();
        if (b == "fx_bus"     || b == "mixer_fx")         return vg.getEffectsBusRack();
        if (b == "clips_bus"  || b == "mixer_clipsbus")   return vg.getAudioClipsBusRack();
        if (b.startsWith("layer_"))
            return vg.getLayerPageRack(b.substring(6).getIntValue());
        if (b.startsWith("bass_"))
            return vg.getBassPageRack(b.substring(5).getIntValue());
        if (b.startsWith("instr_"))
            return vg.getInstrChannelRack(b.substring(6).getIntValue());
        if (b.startsWith("mixer_layer_"))
            return vg.getInsertRack(Kind::Layer, b.substring(12).getIntValue());
        if (b.startsWith("mixer_bass_"))
            return vg.getInsertRack(Kind::Bass,  b.substring(11).getIntValue());
        if (b.startsWith("mixer_drum_"))
            return vg.getInsertRack(Kind::Drum,  b.substring(11).getIntValue());
        if (b.startsWith("mixer_audio_"))
            return vg.getInsertRack(Kind::Audio, b.substring(12).getIntValue());
        if (b.startsWith("mixer_aux_"))
            return vg.getInsertRack(Kind::Aux,   b.substring(10).getIntValue());
        return nullptr;
    };
    {
        juce::String uBase, uUuid, uParam;
        if (splitByUuid(paramId, uBase, uUuid, uParam))
        {
            if (auto* rack = findRackForBase(uBase))
            {
                for (int i = 0; i < EffectRack::kNumSlots; ++i)
                {
                    if (rack->getSlotUuid(i) == uUuid)
                    {
                        // Recurse with the slot-index form so the existing
                        // renderer below handles it identically to legacy.
                        return resolveAutomationDisplayName(
                            uBase + "_s" + juce::String(i) + "_" + uParam);
                    }
                }
            }
            // UUID not in any rack -- effect was deleted after the lane was
            // created.  Best-effort label: channel + (deleted) + param so the
            // automation row is still legible without exposing the UUID.
            juce::String channelOnly = uBase;
            channelOnly = channelOnly.replaceCharacter('_', ' ').trim();
            juce::String prettyP = uParam;
            prettyP = prettyP.replaceCharacter('_', ' ').trim();
            return channelOnly + " - (deleted slot) - " + prettyP;
        }
    }

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
        // rack today - these arrays in VibeGraph are stale leftovers kept as
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

        // 2026-04-21: Drum-slot engine params - tk_drm_{N}_s{S}_{engineTag}_{param}.
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
    // _side_eq*) no longer registered - pre-rack EQ on Layer/Bass/Drum pages
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
        // 2026-04-30 (audit B.5): Vox / Inst / secondary buses added - were
        // missing, so right-click "Automate: ..." showed raw param IDs like
        // "Mixer Voxbus Level" instead of "Mx Vox Bus - Level".
        struct BusEntry { const char* prefix; const char* label; };
        static const BusEntry kBusEntries[] = {
            { "mixer_master_",    "Master"      },
            { "mixer_layers_",    "Layers Bus"  },
            { "mixer_bass_",      "Bass Bus"    },
            { "mixer_drums_",     "Drums Bus"   },
            { "mixer_fx_",        "Effects Bus" },
            { "mixer_clipsbus_",  "Clips Bus"   },
            { "mixer_voxbus_",    "Vox Bus"     },
            { "mixer_instbus_",   "Inst Bus"    },
            { "mixer_voxbus2_",   "Vox Bus 2"   },
            { "mixer_instbus2_",  "Inst Bus 2"  },
            { "mixer_instbus3_",  "Inst Bus 3"  },
            { "mixer_rustybus_",  "RustyDrums Bus" },
            { "mixer_pluginbus_", "Plugins Bus" },   // TS6 (missed, fixed TS7)
        };
        // PREFIX COLLISION GUARD (Jeff's call, 2026-07-30).  The Bass BUS prefix
        // is "mixer_bass_" and a bass INSERT is "mixer_bass_0_...", so the insert
        // also matches the bus row -- and this loop runs first, so EVERY per-bass-
        // strip automation entry was labelled "Mx Bass Bus - 0 Level".  Bass is the
        // only pair that collides (drums/layers use singular insert bases), but the
        // guard is stated generally so a future bus prefix cannot reintroduce it.
        //
        // A digit immediately after the prefix means "indexed insert, not this bus":
        // no bus suffix begins with one (level, pan, sendTo, send0_to, mid_eq0_freq
        // all start with a letter).
        for (const auto& e : kBusEntries)
        {
            const juce::String pfx = e.prefix;
            if (! paramId.startsWith(pfx)) continue;
            const juce::String tail = paramId.substring(pfx.length());
            if (tail.isNotEmpty()
                && juce::CharacterFunctions::isDigit((char) tail[0])) continue;
            return "Mx " + juce::String(e.label) + " - " + formatMixerSuffix(tail);
        }

        // Indexed insert strips: mixer_{kind}_{N}_{suffix}.
        // 2026-04-30 (audit B.5): Vox / Inst insert prefixes added - were
        // missing, so per-Vox / per-Inst strip params fell through to raw
        // prettify-fallback labels in the right-click Automate menu.
        struct InsertEntry { const char* prefix; Kind kind; const char* singular; };
        static const InsertEntry kInsertEntries[] = {
            { "mixer_layer_", Kind::Layer, "Layer"     },
            { "mixer_bass_",  Kind::Bass,  "Bass"      },
            { "mixer_drum_",  Kind::Drum,  "Drum"      },
            { "mixer_audio_", Kind::Audio, "Audio Row" },
            { "mixer_aux_",   Kind::Aux,   "Aux"       },
            { "mixer_vox_",   Kind::Vox,   "Vox"       },
            { "mixer_inst_",  Kind::Inst,  "Inst"      },
            { "mixer_rusty_", Kind::Rusty, "Rusty"     },
            { "mixer_plugin_", Kind::Plugin, "Plugin"  },   // TS6 (missed, fixed TS7)
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
                else if (e.kind == Kind::Plugin) channelLabel = mMixerPage->getPluginStripName(insertIdx);
                // 2026-04-30: MixerPage doesn't expose per-Vox / per-Inst
                // strip-name lookups yet - fall through to the default
                // "Vox N+1" / "Inst N+1" label.  Once those getters land
                // (G-9 strip rename UX), add lookups here.
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
    const juce::String autoName = resolveAutomationDisplayName(lane.paramId);
    // Deleted-slot lanes keep saying WHICH effect they drove: the resolver
    // can only offer "(deleted slot)" once the UUID is gone, but the label
    // captured at lane creation names it.
    if (lane.lastKnownName.isNotEmpty() && autoName.contains("(deleted slot)"))
        return lane.lastKnownName + " (deleted)";
    return autoName;
}

std::unique_ptr<juce::Component> StandaloneEditor::createMixerPage()
{
    auto page = std::make_unique<MixerPage>(mProcessor, *mPM);
    page->setUndoContext(makeUndoContext());
    mMixerPage = page.get();
    // FX Rack button on any strip → switch to Effects tab (ID=2) and pre-select
    // that strip's rack. `identifier` is the strip's APVTS prefix (e.g.
    // "mixer_layer_0") - unambiguous across renames.
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
    // TS7 (CL-044): the master strip's repurposed "+" opens the analyzer.  This
    // is the ONE route -- the View menu entry was removed with it, so there is no
    // second way in that could drift out of sync.
    page->onAnalyzerRequested = [this]() { openMasterAnalyzerWindow(); };

    // TS7 §6: the processor drives freeze but the RENDERER lives on BuilderPage,
    // so it arrives as a hook -- the model must not reach into a view.  Wired
    // here because this is where the editor already owns both.
    mProcessor.onRenderFreezeFile =
        [this] (VibeGraph::InsertKind kind, int index, RenderTask* target,
                int patternIndex, const juce::File& dest, juce::String& outErr) -> bool
        {
            if (mBuilderPage == nullptr) { outErr = "The Builder is not available."; return false; }

            // PROGRESS + CANCEL were dropped here (fixed 2026-07-30).
            // renderFreezeFile has always taken shouldAbort and onProgress, and
            // this single wiring point passed neither -- so BOTH the manual
            // button and auto-freeze showed an anonymous spinner with no
            // percentage and no way out, on a render that can run for a minute
            // or more.  The abort check inside the loop could never fire.
            return mBuilderPage->renderFreezeFile (
                kind, index, target, patternIndex, dest, outErr,
                [this] { return mHeavyOpOverlay.wasCancelled(); },
                [this] (double p)
                {
                    // setProgress pumps the peer's pending paint itself -- the
                    // render holds the message thread, so a plain repaint would
                    // never reach the screen and the bar would sit at 0, looking
                    // identical to the hang it exists to explain.
                    mHeavyOpOverlay.setProgress (p);
                });
        };

    // §6.8: stepped progress for a per-instrument freeze (song + one render per
    // pattern).  Without it a multi-render freeze showed one bar that reset to 0
    // on each pass, which reads as a hang rather than progress.
    mProcessor.onFreezeStep = [this] (int done, int total, const juce::String& label)
    {
        mHeavyOpOverlay.setStep (done, total, label);
    };

    // §6.9: the kit's thirteen strips, one pass.  Same progress + cancel wiring
    // as the single-track path.
    mProcessor.onRenderKitFreezeFiles =
        [this] (const std::vector<juce::File>& dests, RenderTask* target,
                int patternIndex, juce::String& outErr) -> bool
        {
            if (mBuilderPage == nullptr) { outErr = "The Builder is not available."; return false; }
            return mBuilderPage->renderKitFreezeFiles (
                dests, target, patternIndex, outErr,
                [this] { return mHeavyOpOverlay.wasCancelled(); },
                [this] (double p) { mHeavyOpOverlay.setProgress (p); });
        };

    // TS7 §6.6: a freeze going stale queues a re-render.  QUEUED rather than run
    // here because this fires from an edit -- rendering synchronously inside a
    // note drag would freeze the UI mid-gesture.  The 5 Hz poll drains it one at
    // a time, and the tab plays its live engine until its file lands.
    // §6.7: a removed frozen tab's file is deleted rather than left to accumulate.
    mProcessor.engineRig().onFreezeFileObsolete =
        [this] (TabKind kind, int pageIndex)
        {
            mProcessor.deleteFreezeFileFor (kind, pageIndex);
        };

    mProcessor.engineRig().onFreezeStateChanged =
        [this] (TabKind kind, int pageIndex)
        {
            // §6.4: the ribbon's frozen mark is painted from live rig state, so a
            // repaint is all it needs -- no cached copy to keep in sync.
            if (mRibbon) mRibbon->repaint();
            // The title-bar Freeze button reads live state the same way, but it
            // is a TextButton and will not restyle itself.
            if (mPageMenuBar) mPageMenuBar->refreshFreezeState();

            if (mProcessor.engineRig().isFreezeStale (kind, pageIndex))
            {
                const bool queued = std::any_of (
                    mFreezeRefreshQueue.begin(), mFreezeRefreshQueue.end(),
                    [&] (const PendingFreeze& p)
                    { return p.kind == kind && p.pageIndex == pageIndex; });
                if (! queued)
                    mFreezeRefreshQueue.push_back ({ kind, pageIndex, false });
            }
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
    // B2 + B1 (2026-05-04): device name for the input-picker pair detector.
    page->getInputDeviceName = [this]() -> juce::String
    {
        if (auto* dev = mDeviceManager.getCurrentAudioDevice())
            return dev->getName();
        return {};
    };
    // QA-Fe2: Builder Grid Default section in the same picker (replaced the
    // arm popup).  State lives here so commitRecordingResult reads it.
    page->onGetGridDefault = [this] (int voxIdx) -> int
    {
        return (voxIdx >= 0 && voxIdx < kDenoiseMaxVox)
                 ? mVoxTakePick[(size_t) voxIdx] : -1;
    };
    page->onSetGridDefault = [this] (int voxIdx, int pick)
    {
        if (voxIdx >= 0 && voxIdx < kDenoiseMaxVox)
            mVoxTakePick[(size_t) voxIdx] = juce::jlimit (0, 3, pick);
    };
    // G-4 (2026-04-28): "Add Vox Strip" / "Add Inst Strip" buttons in the
    // Mixer page are the spawn trigger for the matching ribbon page (no other
    // path).  spawnVoxTabIfMissing / spawnInstTabIfMissing are idempotent on
    // pageIdx so restoring a project (which calls addVoxChannelAtIndex during
    // load) is safe - duplicate spawns are a no-op.
    // G-6 (2026-04-29): Mixer "Add Vox/Inst Strip" should NOT auto-jump to
    // the new page - keep user on Mixer so they can add multiple strips in
    // a row without bouncing back each time.  Empty-state spawn flow (and
    // any other path that wants navigation) leaves selectAfter at default.
    page->onVoxStripAdded  = [this](int idx) { spawnVoxTabIfMissing  (idx, /*selectAfter*/ false); };
    page->onInstStripAdded = [this](int idx) { spawnInstTabIfMissing (idx, /*selectAfter*/ false); };
    return page;
}

std::unique_ptr<juce::Component> StandaloneEditor::createEffectsPage()
{
    auto page = std::make_unique<EffectsPage>(mTrackSel, mProcessor);
    page->setUndoContext(makeUndoContext());
    mEffectsPage = page.get();

    // QA-ModelShell TS5: the rack window asks; the editor owns the windows
    // (it owns the Workspace).  Requests carry the channel id because these
    // windows deliberately outlive the rack window's current selection.
    page->onOpenSlotPanel = [this] (int chId, int slotIndex)
    {
        openEffectSlotWindow (chId, slotIndex);
    };
    page->onOpenEqWindow = [this] (int chId, bool pre)
    {
        openEffectEqWindow (chId, pre);
    };
    page->onRackContentsChanged = [this] (int chId)
    {
        closeDeadEffectWindows (chId);
    };

    // ── Active channel list callback ──────────────────────────────────────────
    // Returns only channels that actually have an open tab / assigned sound.
    page->onGetActiveChannels = [this]() -> std::vector<std::pair<int, juce::String>>
    {
        std::vector<std::pair<int, juce::String>> result;

        // Bus channels - always present
        result.push_back({4, "Master"});
        result.push_back({1, "Layers Bus"});
        result.push_back({2, "Bass Bus"});
        result.push_back({3, "Drums Bus"});
        result.push_back({5, "FX Bus"});
        result.push_back({6, "Clips Bus"});
        // R3.5 (2026-04-23): Vox + Inst buses always-allocated.
        result.push_back({7, "Vox Bus"});
        result.push_back({8, "Inst Bus"});
        // G-6 (2026-04-29): secondary Vox/Inst buses surfaced only when active
        // (their UI strip is gated; without this, FX rack click on the bus
        // would fail to navigate even though the channel is wired).
        if (mMixerPage && mMixerPage->isVoxBus2Active())  result.push_back({9,  "Vox Bus 2"});
        if (mMixerPage && mMixerPage->isInstBus2Active()) result.push_back({10, "Inst Bus 2"});
        if (mMixerPage && mMixerPage->isInstBus3Active()) result.push_back({11, "Inst Bus 3"});
        // J-5 (2026-05-03): RustyDrums Bus surfaced only when active (singleton spawned).
        if (mProcessor.hasBaySickRustyDrums()) result.push_back({12, "RustyDrums Bus"});
        // TS6 (BLU-447) -- MISSED, fixed TS7 2026-07-30.  EffectsPage already
        // drew a "PLUGINS BUS" group heading; with neither the bus nor its
        // members in this list it was a heading over nothing.
        if (mMixerPage && mMixerPage->isPluginsBusActive()) result.push_back({13, "Plugins Bus"});

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

        // Active Drum slots - enumerated via MixerPage (matches the mixer's
        // visible drum strips). Dropdown ID 100+slot maps to mixer_drum_N via
        // EffectsPage::getMixerApvtsPrefixForChannel. Legacy InstrChannelNode
        // drums (5F-3 and earlier) are no longer registered - all drum audio
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

        // Per-clip audio row channels (IDs 400+row) - name live from mixer strip
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

        // Aux strips - dropdown-internal ID range 600+idx to avoid collision
        // with drum (100-series) and audio (400-series).
        if (mMixerPage)
        {
            for (int auxIdx : mMixerPage->getAuxStripIndices())
                result.push_back({600 + auxIdx, mMixerPage->getAuxStripName(auxIdx)});

            // J-6 (2026-05-03): Vox/Inst inserts surfaced on the Effects page.
            // Disambiguated dropdown ranges: Vox 700+, Inst 800+, Rusty 900+.
            for (int idx : mMixerPage->getVoxStripIndices())
                result.push_back({700 + idx, mMixerPage->getVoxStripName(idx)});
            for (int idx : mMixerPage->getInstStripIndices())
                result.push_back({800 + idx, mMixerPage->getInstStripName(idx)});
            // TS6 (BLU-447) -- MISSED, fixed TS7 2026-07-30.  EffectsPage already
            // resolves the 1000+ range to InsertKind::Plugin everywhere (rack, EQ,
            // pre-EQ, prefix, automation sweep); nothing ever EMITTED an id in it.
            for (int idx : mMixerPage->getPluginStripIndices())
                result.push_back({1000 + idx, mMixerPage->getPluginStripName(idx)});
        }

        // J-6 (2026-05-03): Rusty inserts (when singleton is spawned).
        if (mProcessor.hasBaySickRustyDrums())
        {
            if (auto* eng = mProcessor.getBaySickRustyDrums())
            {
                const auto& chans = eng->getChannels();
                for (size_t i = 0; i < chans.size() && i < (size_t) MixerChannelIds::kMaxRustyStrips; ++i)
                    result.push_back({900 + (int) i, chans[i].name});
            }
        }

        return result;
    };

    // The Effects page constructor builds its dropdown before the callback is
    // wired (so it falls back to a plain Bus list with no apvts-prefix mapping).
    // Re-rebuild now that the callback is in place - gives us colored section
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
    // G-6 (2026-04-29): Clip ribbon +Add opens an OS file picker.  Defaults
    // to Documents/BaySickDAW/My Samples (created on demand, with a Core
    // Library shortcut inside).  On choose, route through the Builder
    // grid's importAudioFile so onAudioClipAdded → spawnClipsTabIfMissing
    // fires uniformly with drag-drop.
    if (type == RibbonTabBar::TabType::Clip)
    {
        // QA-ClipDrop Task 3 (SC-G/J, 2026-06-03): "+ Add New Clip" no longer
        // routes through the Builder grid's importAudioFile (which dropped a
        // block on row 0 and named the strip after that grid row).  It opens the
        // file picker, then addClipPageFromFile copies the file + registers it in
        // the audio library + spawns a Clips page + mixer strip named from the
        // SAMPLE, with NO grid block.  No-project case still prompts to create a
        // project first, then retries (SC-J).
        SampleLibrary::ensureUserSamplesDir();
        auto chooser = std::make_shared<juce::FileChooser> (
            "Choose an audio file to add as a Clip",
            SampleLibrary::getUserSamplesDir(),
            "*.wav;*.mp3;*.aiff;*.aif;*.flac;*.ogg");
        const int flags = juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectFiles;
        chooser->launchAsync (flags, [this, chooser] (const juce::FileChooser& fc)
        {
            const juce::File f = fc.getResult();
            if (f == juce::File()) { ClipDropDiag::log ("AddNewClip", "picker cancelled (no file)"); return; }
            ClipDropDiag::log ("AddNewClip PICKED", "file=" + f.getFullPathName());
            addClipPageFromFile (f);
        });
        return;
    }

    // G-6 (2026-04-29): Vox + Inst now support ribbon +Add (in addition to
    // the Mixer page button).  These flows go through the Mixer's add-strip
    // path so the strip creation cascade fires the page spawn callback.
    if (type == RibbonTabBar::TabType::Vox || type == RibbonTabBar::TabType::Inst)
    {
        if (! mMixerPage) return;
        const bool isVox = (type == RibbonTabBar::TabType::Vox);
        const int  cap   = isVox ? (int) kMaxVoxPages : (int) kMaxInstPages;

        // Find first free idx not already used by an existing page.
        std::vector<bool> taken ((size_t) cap, false);
        for (auto* entry : mPages)
        {
            if (! entry || entry->type != type) continue;
            int idx = -1;
            if (isVox)
            {
                if (auto* vp = dynamic_cast<VoxPage*> (entry->component.get())) idx = vp->getPageIndex();
            }
            else
            {
                if (auto* ip = dynamic_cast<InstPage*> (entry->component.get())) idx = ip->getPageIndex();
            }
            if (idx >= 0 && idx < cap) taken[(size_t) idx] = true;
        }
        int newIdx = -1;
        for (int i = 0; i < cap; ++i)
            if (! taken[(size_t) i]) { newIdx = i; break; }
        if (newIdx < 0) return;

        // Mixer's addVoxChannelAtIndex / addInstChannelAtIndex creates the
        // strip and synchronously fires onVoxStripAdded / onInstStripAdded
        // → spawnVoxTabIfMissing(newIdx, false) / spawnInstTabIfMissing.
        if (isVox) mMixerPage->addVoxChannelAtIndex  (newIdx);
        else       mMixerPage->addInstChannelAtIndex (newIdx);

        // User clicked ribbon +Add - explicitly navigate to the new tab.
        for (auto* entry : mPages)
        {
            if (! entry || entry->type != type) continue;
            int idx = -1;
            if (isVox)
            {
                if (auto* vp = dynamic_cast<VoxPage*> (entry->component.get())) idx = vp->getPageIndex();
            }
            else
            {
                if (auto* ip = dynamic_cast<InstPage*> (entry->component.get())) idx = ip->getPageIndex();
            }
            if (idx == newIdx && mRibbon)
            {
                mRibbon->selectTab (entry->ribbonTabId);
                onTabSelected (entry->ribbonTabId);
                break;
            }
        }
        return;
    }

    // D1.4: Layers, Bass, and Drums all support adding new instances.
    // TS6: Plugins joins them -- same create-page-then-wire shape.
    if (type != RibbonTabBar::TabType::Layers
        && type != RibbonTabBar::TabType::Bass
        && type != RibbonTabBar::TabType::Drums
        && type != RibbonTabBar::TabType::Plugins)
        return;

    std::unique_ptr<juce::Component> page;
    juce::String name;

    switch (type)
    {
    case RibbonTabBar::TabType::Layers:
        page = createLayersPage();
        if (!page) return;  // all 8 Layers slots occupied
        name = nextLayerTabName();   // QA-D STATE-02
        break;
    case RibbonTabBar::TabType::Bass:
        page = createBassPage();
        if (!page) return;  // all 4 Bass slots occupied
        name = nextBassTabName();   // QA-D STATE-02
        break;
    case RibbonTabBar::TabType::Drums:
        page = createDrumPage();
        if (!page) return;  // all 16 Drums slots occupied
        name = nextDrumTabName();   // QA-D STATE-02
        break;
    case RibbonTabBar::TabType::Plugins:
        page = createPluginsPage();
        if (!page) return;  // all plugin slots occupied
        name = nextPluginTabName();
        break;
    default:
        return;
    }

    int newId = mRibbon->addTab(type, name);

    // Wire mixer strip creation to engine selection (lazy - not on tab open)
    if (type == RibbonTabBar::TabType::Layers)
    {
        if (auto* p = dynamic_cast<LayersPage*>(page.get()))
        {
            p->setTabName (name);   // QA-D STATE-02: sync internal mTabName to ribbon
            const int pageIdx = p->getPageIndex();
            p->onEngineSelected = [this, newId, pageIdx, p] {
                const auto* tab = mRibbon->getTabById(newId);
                if (mMixerPage) mMixerPage->addLayerChannel(pageIdx, tab ? tab->name : "Layers");
                if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
                wireEngineDirtyHook (p->getEngineProcessor());
                // QA-D STATE-02 follow-on: piano-roll context label.
                if (mPianoRollPage)
                    mPianoRollPage->setEngineType ({ EngineKind::Layer, pageIdx }, p->getEngineType());
            };
            p->onDeleteRequested = [this, newId] {
                if (! mRibbon) return;
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
            // QA-UICleanup Task 2: added Layers tabs were missing this hook (the
            // initial/default + duplicate + Drums-add paths all wire it), so a
            // patch load on any but the first-created tab never renamed the ribbon
            // tab / mixer strip / piano-roll label.  Mirror the initial form (:1640).
            p->onSoundNameChanged = [this, newId, pageIdx] (const juce::String& nm) {
                if (nm.isEmpty()) return;
                if (mRibbon)    mRibbon->renameTab (newId, nm);
                if (mMixerPage) mMixerPage->renameChannel (MixerPage::StripKind::Layer, pageIdx, nm);
                if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Layer, pageIdx }, nm);
            };
            registerLayerPianoRoll (p);
        }
    }
    // QA-ModelShell TS6 (BLU-447): hosted VST3 instrument tab.  Mirrors the
    // Layers block above; the mixer strip is created on the FIRST plugin pick
    // (lazy, like every other type) rather than at tab open.
    else if (type == RibbonTabBar::TabType::Plugins)
    {
        if (auto* p = dynamic_cast<PluginsPage*>(page.get()))
        {
            p->setTabName (name);
            const int pageIdx = p->getPageIndex();

            p->onEngineSelected = [this, newId, pageIdx, p] {
                // Tab + strip take the plugin's display name (program name
                // when published, else plugin name) -- the same linkage the
                // engine pages get from onSoundNameChanged on preset load.
                const juce::String nm = p->getDisplayName();
                if (nm.isNotEmpty() && mRibbon) { mRibbon->renameTab (newId, nm); p->setTabName (nm); }
                const auto* tab = mRibbon->getTabById(newId);
                if (mMixerPage) mMixerPage->addPluginChannel (pageIdx, tab ? tab->name : "Plugin");
                if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
                if (mPianoRollPage)
                    mPianoRollPage->setEngineType ({ EngineKind::Plugin, pageIdx }, p->getDisplayName());
            };
            // A later pick OR a program change inside the plugin (the page's
            // poll fires this on display-name changes) renames tab + strip +
            // roll label to follow.
            p->onPluginChanged = [this, newId, pageIdx, p] {
                const juce::String nm = p->getDisplayName();
                if (nm.isNotEmpty())
                {
                    if (mRibbon) { mRibbon->renameTab (newId, nm); p->setTabName (nm); }
                    if (mMixerPage) mMixerPage->renameChannel (MixerPage::StripKind::Plugin, pageIdx, nm);
                }
                if (mPianoRollPage)
                    mPianoRollPage->setEngineType ({ EngineKind::Plugin, pageIdx }, p->getDisplayName());
                if (mProjectManager) mProjectManager->markDirty();
            };
            p->onDeleteRequested = [this, newId] {
                if (! mRibbon) return;
                mRibbon->closeTab (newId);
            };
            // Instrument lanes (ruling 2-b): registration re-runs on every
            // param-count change (bridged lists arrive async).  The Automate
            // pick re-registers too, then opens the lane.
            p->onParamListChanged = [this, pageIdx] { registerPluginTabAutomation (pageIdx); };
            p->onAutomateParam = [this, pageIdx] (const juce::String& paramId) {
                registerPluginTabAutomation (pageIdx);
                openEventEditorForParam ("plugtab" + juce::String (pageIdx) + "_vst_" + paramId);
            };

            registerPluginPianoRoll (pageIdx, p);
        }
    }
    else if (type == RibbonTabBar::TabType::Bass)
    {
        if (auto* p = dynamic_cast<BassPage*>(page.get()))
        {
            p->setTabName (name);   // QA-D STATE-02: sync internal mTabName to ribbon
            const int pageIdx = p->getPageIndex();
            p->onEngineSelected = [this, newId, pageIdx, p] {
                const auto* tab = mRibbon->getTabById(newId);
                if (mMixerPage) mMixerPage->addBassChannel(pageIdx, tab ? tab->name : "Bass");
                if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
                wireEngineDirtyHook (p->getEngineProcessor());
                // QA-D STATE-02 follow-on: piano-roll context label.
                if (mPianoRollPage)
                    mPianoRollPage->setEngineType ({ EngineKind::Bass, pageIdx }, p->getEngineType());
            };
            p->onDeleteRequested = [this, newId] {
                if (! mRibbon) return;
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
            // QA-UICleanup Task 2: added Bass tabs were missing this hook (mirror :1689).
            p->onSoundNameChanged = [this, newId, pageIdx] (const juce::String& nm) {
                if (nm.isEmpty()) return;
                if (mRibbon)    mRibbon->renameTab (newId, nm);
                if (mMixerPage) mMixerPage->renameChannel (MixerPage::StripKind::Bass, pageIdx, nm);
                if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Bass, pageIdx }, nm);
            };
            registerBassPianoRoll (p);
        }
    }
    else if (type == RibbonTabBar::TabType::Drums)
    {
        if (auto* p = dynamic_cast<DrumPage*>(page.get()))
        {
            p->setTabName (name);   // QA-D STATE-02: sync internal mTabName to ribbon
            const int pageIdx = p->getPageIndex();
            p->onEngineSelected = [this, newId, pageIdx, p] {
                const auto* tab = mRibbon->getTabById(newId);
                if (mMixerPage)   mMixerPage->addDrumChannel(pageIdx, tab ? tab->name : "Drums");
                if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
                refreshAllKitViews();
                wireEngineDirtyHook (p->getEngineProcessor());
                // QA-D STATE-02 follow-on: piano-roll context label.
                if (mPianoRollPage)
                    mPianoRollPage->setEngineType ({ EngineKind::Drum, pageIdx }, p->getEngineType());
            };
            p->onSoundNameChanged = [this, newId, pageIdx, p] (const juce::String& nm) {
                if (nm.isEmpty()) return;
                if (mRibbon)    mRibbon->renameTab (newId, nm);
                if (mMixerPage) mMixerPage->renameChannel (MixerPage::StripKind::Drum, pageIdx, nm);
                p->setTabName (nm);
                refreshAllKitViews();
                if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Drum, pageIdx }, nm);
            };
            p->onDeleteRequested = [this, newId] {
                if (! mRibbon) return;
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
    mPages.add(entry);
    hostPageInWindow (*entry);

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
    // current channel alone - matches per-channel sub-tab persistence (user lands
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

void StandaloneEditor::jumpToFxRackForPrefix (const juce::String& mixerPrefix)
{
    mLastFXChannel = mixerPrefix;
    if (mRibbon) mRibbon->selectTab (2);
    onTabSelected (2);
}

// Both roll jumps resolve their target BEFORE any tab switch (the switch
// replaces PageMenuBar callbacks mid-invocation -- the Sub-Phase A
// use-after-free family) and then touch only locals.
void StandaloneEditor::jumpToRollPlayerPage()
{
    if (! mPianoRollPage) return;
    const EngineId id = mPianoRollPage->getActiveEngineId();

    int targetTabId = -1;
    for (auto* entry : mPages)
    {
        if (! entry) continue;
        auto* c = entry->component.get();
        bool match = false;
        if (id.kind == EngineKind::Layer)
        {
            auto* p = dynamic_cast<LayersPage*> (c);
            match = p != nullptr && p->getPageIndex() == id.index;
        }
        else if (id.kind == EngineKind::Bass)
        {
            auto* p = dynamic_cast<BassPage*> (c);
            match = p != nullptr && p->getPageIndex() == id.index;
        }
        else if (id.kind == EngineKind::Drum)
        {
            auto* p = dynamic_cast<DrumPage*> (c);
            match = p != nullptr && p->getPageIndex() == id.index;
        }
        else if (id.kind == EngineKind::Clip)
        {
            auto* p = dynamic_cast<ClipsPage*> (c);
            match = p != nullptr && p->getPageIndex() == id.index;
        }
        else if (id.kind == EngineKind::BaySickGuitars
              || id.kind == EngineKind::BaySickBasses)
        {
            auto* p = dynamic_cast<InstPage*> (c);
            const auto want = id.kind == EngineKind::BaySickGuitars
                            ? InstPage::Source::BaySickGuitars
                            : InstPage::Source::BaySickBasses;
            match = p != nullptr && p->getSource() == want
                                 && p->getPageIndex() == id.index;
        }
        else if (id.kind == EngineKind::BaySickRustyDrums)
        {
            match = dynamic_cast<BaySickRustyDrumsPage*> (c) != nullptr;
        }
        else if (id.kind == EngineKind::Plugin)   // TS6 (missed, fixed TS7)
        {
            auto* p = dynamic_cast<PluginsPage*> (c);
            match = p != nullptr && p->getPageIndex() == id.index;
        }
        else if (id.kind == EngineKind::DrumKit)
        {
            // The kit view spans every drum -- land on the first Drums tab.
            match = dynamic_cast<DrumPage*> (c) != nullptr;
        }
        if (match) { targetTabId = entry->ribbonTabId; break; }
    }
    if (targetTabId < 0) return;

    auto* rbn = mRibbon.get();
    if (rbn != nullptr) rbn->selectTab (targetTabId);
    onTabSelected (targetTabId);
}

void StandaloneEditor::jumpToRollFxRack()
{
    if (! mPianoRollPage) return;
    const EngineId id = mPianoRollPage->getActiveEngineId();

    juce::String prefix;
    if      (id.kind == EngineKind::Layer) prefix = "mixer_layer_" + juce::String (id.index);
    else if (id.kind == EngineKind::Bass)  prefix = "mixer_bass_"  + juce::String (id.index);
    else if (id.kind == EngineKind::Drum)  prefix = "mixer_drum_"  + juce::String (id.index);
    else if (id.kind == EngineKind::Clip)  prefix = "mixer_audio_" + juce::String (id.index);
    else if (id.kind == EngineKind::BaySickGuitars
          || id.kind == EngineKind::BaySickBasses)
        prefix = "mixer_inst_" + juce::String (id.index);
    else if (id.kind == EngineKind::Plugin)   // TS6 (missed, fixed TS7)
        prefix = "mixer_plugin_" + juce::String (id.index);
    else if (id.kind == EngineKind::BaySickRustyDrums)
        prefix = "mixer_rustybus";        // singleton engine -> its bus rack
    else if (id.kind == EngineKind::DrumKit)
        prefix = "mixer_drums";           // kit view spans drums -> Drums Bus rack
    if (prefix.isEmpty()) return;

    jumpToFxRackForPrefix (prefix);
}

void StandaloneEditor::onTabClosed(int tabId)
{
    // Find and remove the page entry
    for (int i = 0; i < mPages.size(); ++i)
    {
        if (mPages[i]->ribbonTabId == tabId)
        {
            // MIX-05 (QA-L): capture engine-page strip indices here; the
            // orphan-strip removal happens in the tail with the Inst/Vox/
            // Clips trio (same convention).
            int layerStripIdx = -1, bassStripIdx = -1, drumStripIdx = -1;
            int pluginStripIdx = -1;   // QA-ModelShell TS6 (BLU-447)

            // TS6: free the plugin index slot + drop its roll registration
            // BEFORE the page dies -- the connection's closures capture the
            // page raw pointer, same reason as the Layers block below.
            if (auto* pp = dynamic_cast<PluginsPage*>(mPages[i]->component.get()))
            {
                const int idx = pp->getPageIndex();
                if (idx >= 0 && idx < (int) kMaxPluginPages)
                    mUsedPluginIndices[(size_t) idx] = false;
                if (mPianoRollPage && idx >= 0)
                    mPianoRollPage->unregisterEngine ({ EngineKind::Plugin, idx });
                if (mMixerPage && idx >= 0)
                    mMixerPage->removePluginChannel (idx);
                pluginStripIdx = idx;
            }

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
                if (idx >= 0)
                {
                    layerStripIdx = idx;
                    // Rack-slot pids are 1-based for layers/basses
                    // (EffectsPage::getChannelPrefix maps dropdown id-199).
                    // QA-ApvtsAutomation: engine-editor registrations key off the
                    // engine's own prefix ("tk_" + trackId + "_<engine>_"), which
                    // neither erase above covers.
                }
            }

            // Free bass index slot for any BassPage being closed
            if (auto* bp = dynamic_cast<BassPage*>(mPages[i]->component.get()))
            {
                int idx = bp->getPageIndex();
                if (idx >= 0 && idx < kMaxBassPages)
                    mUsedBassIndices[idx] = false;
                if (mPianoRollPage && idx >= 0)
                    mPianoRollPage->unregisterEngine ({ EngineKind::Bass, idx });
                if (idx >= 0)
                {
                    bassStripIdx = idx;
                }
            }

            // D1.4: Free drum index slot for any DrumPage being closed
            if (auto* dp = dynamic_cast<DrumPage*>(mPages[i]->component.get()))
            {
                int idx = dp->getPageIndex();
                if (idx >= 0 && idx < kMaxDrumPages)
                    mUsedDrumIndices[idx] = false;
                if (mPianoRollPage && idx >= 0)
                    mPianoRollPage->unregisterEngine ({ EngineKind::Drum, idx });
                if (idx >= 0)
                {
                    drumStripIdx = idx;
                }
            }

            // Clips tab close - unregister the audio engine + piano-roll
            // connection so the audio thread + roll dropdown both drop the
            // page cleanly.
            //
            // QA-E Task 5 (2026-05-15): library + block cascade replaces the
            // pre-Task-5 "no-file-delete contract".  Walks every library
            // entry owned by this page (channelId = audioInsert(idx)),
            // removes the blocks routed to it (precise: path + routeChannel
            // match, so blocks routed to OTHER pages survive multi-owner
            // schema), then removes the library entries themselves.  Audio
            // files on disk are still preserved -- we only touch library +
            // grid.  Symmetric counterpart to the BrowserPanel last-file-out
            // cascade (BuilderPage.cpp confirmAndDeleteLibraryEntry).
            // QA-EffectsReview side-fix (2026-06-06): capture the Clips strip
            // index so the orphan Audio mixer strip is dropped on close -- mirrors
            // the Inst (2026-05-05) + Vox (QA-C MIX-01) fixes.  removeClipChannel
            // existed in MixerPage but was never wired into any close path, so
            // deleting a Clips page left its strip in the live mixer until reload.
            int clipStripIdx = -1;
            if (auto* cp = dynamic_cast<ClipsPage*>(mPages[i]->component.get()))
            {
                int idx = cp->getPageIndex();
                if (idx >= 0)
                {
                    mProcessor.unregisterClipEngine (idx);
                    clipStripIdx = idx;
                    if (mPianoRollPage)
                        mPianoRollPage->unregisterEngine ({ EngineKind::Clip, idx });
                }
                if (mPM && idx >= 0)
                {
                    using namespace MixerChannelIds;
                    const int chId = audioInsert (idx);
                    bool anyChanged = false;
                    for (int e = mPM->getNumAudioLibrary() - 1; e >= 0; --e)
                    {
                        if (mPM->getAudioLibraryPageOwner (e) != chId) continue;
                        const juce::String entryPath = mPM->getAudioLibraryPath (e);
                        for (int b = mPM->getNumBlocks() - 1; b >= 0; --b)
                        {
                            const auto& blk = mPM->getBlock (b);
                            if (blk.clipType == ClipType::Audio
                                && blk.audioFilePath == entryPath
                                && blk.routeChannel  == chId)
                            {
                                mPM->removeBlock (b);
                                anyChanged = true;
                            }
                        }
                        mPM->removeAudioFromLibraryAt (e);
                        anyChanged = true;
                    }
                    if (anyChanged && mBuilderPage)
                        mBuilderPage->notifyArrangementChanged();
                }
            }

            // G-4 (2026-04-28): Vox tab close - unregister the audio engine
            // so the audio thread stops processing it.  No piano-roll
            // unregister needed (Vox isn't registered with PianoRollPage).
            // QA-C MIX-01 (2026-05-10): mirror the 2026-05-05 Inst fix - drop
            // the orphan mixer strip widget on close so the slot index is
            // fully reusable.  APVTS params for the strip + any bound
            // recording stay intact (no-file-delete contract preserved); only
            // the strip widget + order entry drop, matching the post-2026-05-05
            // Inst convention.
            int voxStripIdx = -1;
            if (auto* vp = dynamic_cast<VoxPage*>(mPages[i]->component.get()))
            {
                int idx = vp->getPageIndex();
                if (idx >= 0)
                {
                    mProcessor.unregisterVoxEngine (idx);
                    voxStripIdx = idx;
                }
                // QA-E Task 5 (2026-05-15): library + block cascade for Vox
                // tab close.  Walks every library entry owned by this Vox
                // page (channelId = voxInsert(idx)) and removes the blocks
                // routed to it (precise match on path + routeChannel), then
                // removes the library entries.  Audio files on disk are
                // preserved.  Symmetric counterpart to the BrowserPanel
                // last-file-out cascade.
                if (mPM && idx >= 0)
                {
                    using namespace MixerChannelIds;
                    const int chId = voxInsert (idx);
                    bool anyChanged = false;
                    for (int e = mPM->getNumAudioLibrary() - 1; e >= 0; --e)
                    {
                        if (mPM->getAudioLibraryPageOwner (e) != chId) continue;
                        const juce::String entryPath = mPM->getAudioLibraryPath (e);
                        for (int b = mPM->getNumBlocks() - 1; b >= 0; --b)
                        {
                            const auto& blk = mPM->getBlock (b);
                            if (blk.clipType == ClipType::Audio
                                && blk.audioFilePath == entryPath
                                && blk.routeChannel  == chId)
                            {
                                mPM->removeBlock (b);
                                anyChanged = true;
                            }
                        }
                        mPM->removeAudioFromLibraryAt (e);
                        anyChanged = true;
                    }
                    if (anyChanged && mBuilderPage)
                        mBuilderPage->notifyArrangementChanged();
                }
            }

            // G-4 (2026-04-28): Inst tab close - same shape as Vox.
            // K-4 / L-3 (2026-05-05): sfizz-source Inst pages also unregister
            // their PianoRoll connection so the entry disappears from
            // PianoRollPage's dropdown.  Engine teardown happens AFTER
            // mPages.remove(i) (the page's destructor unregisters the chain
            // processor first; only then is the Guitars/Basses engine safe to
            // free).
            int instGuitarsIdx = -1;
            int instBassesIdx  = -1;
            int instStripIdx   = -1;   // 2026-05-05: every Inst tab close
                                        // drops its mixer strip so the index
                                        // is reusable.  Captured here BEFORE
                                        // mPages.remove(i) below invalidates
                                        // the InstPage pointer.
            if (auto* ip = dynamic_cast<InstPage*>(mPages[i]->component.get()))
            {
                int idx = ip->getPageIndex();
                if (idx >= 0)
                {
                    mProcessor.unregisterInstEngine (idx);
                    instStripIdx = idx;
                }
                if (ip->getSource() == InstPage::Source::BaySickGuitars)
                {
                    unregisterInstSourcePianoRoll (ip);
                    instGuitarsIdx = idx;
                }
                else if (ip->getSource() == InstPage::Source::BaySickBasses)
                {
                    unregisterInstSourcePianoRoll (ip);
                    instBassesIdx = idx;
                }
                // QA-E Task 5 (2026-05-15): library + block cascade for Inst
                // tab close.  Same shape as Vox + Clips cascades.  Audio
                // files on disk preserved; only library + grid touched.
                if (mPM && idx >= 0)
                {
                    using namespace MixerChannelIds;
                    const int chId = instInsert (idx);
                    bool anyChanged = false;
                    for (int e = mPM->getNumAudioLibrary() - 1; e >= 0; --e)
                    {
                        if (mPM->getAudioLibraryPageOwner (e) != chId) continue;
                        const juce::String entryPath = mPM->getAudioLibraryPath (e);
                        for (int b = mPM->getNumBlocks() - 1; b >= 0; --b)
                        {
                            const auto& blk = mPM->getBlock (b);
                            if (blk.clipType == ClipType::Audio
                                && blk.audioFilePath == entryPath
                                && blk.routeChannel  == chId)
                            {
                                mPM->removeBlock (b);
                                anyChanged = true;
                            }
                        }
                        mPM->removeAudioFromLibraryAt (e);
                        anyChanged = true;
                    }
                    if (anyChanged && mBuilderPage)
                        mBuilderPage->notifyArrangementChanged();
                }
            }

            // J-6 (2026-05-03): BaySickRustyDrums tab close - tear down the
            // singleton engine + clear its 13 mixer strips.  Defensive: most
            // close paths already do this (the page's onDeleteRequested fires
            // first), but if the user closes via a path that bypasses the
            // page button (project load purge, mass close), this catches it.
            // J-8 stage 2 (2026-05-04): destroy order matters - the ARIA panel's
            // SliderParameterAttachments live on the engine's APVTS, so the
            // page (and its widgets) must be destructed BEFORE the engine.
            // mPages.remove(i) below handles the page destruction; we just
            // tear down adjacent state here, then let the remove() destroy
            // the page, then null the engine on the next pass via a deferred
            // callback when control returns.
            const bool isRusty = dynamic_cast<BaySickRustyDrumsPage*>(mPages[i]->component.get()) != nullptr;
            if (isRusty)
            {
                // Last-kit session memory (LIFE-02) -- the engine is still
                // alive here (destroyed only after mPages.remove below).
                if (auto* eng = mProcessor.getBaySickRustyDrums())
                    if (eng->getCurrentKitPath().existsAsFile())
                        mLastRustyKitFile = eng->getCurrentKitPath();
                if (mMixerPage) mMixerPage->clearAllRustyChannels();
                if (mPianoRollPage)
                    mPianoRollPage->unregisterEngine ({ EngineKind::BaySickRustyDrums, 0 });
                for (int r = 0; r < (int) MixerChannelIds::kMaxRustyStrips; ++r)
                {
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

            // QA-E Task 5 (2026-05-15): capture whether the closed page was
            // the one on screen.  Used below to decide whether to surface the
            // empty-state placeholder.  When the close was triggered by the
            // browser last-file-out cascade (user is on Builder, NOT viewing
            // the page being closed), we must NOT yank them to the empty-
            // state page -- they should stay on the Builder browser.
            const bool closedPageWasVisible =
                (mVisiblePage == mPages[i]->component.get());

            if (closedPageWasVisible)
                mVisiblePage = nullptr;

            // The title-strip pointer follows the ACTIVE page window
            // (showPageForTab re-points it at each window's own menu).  If
            // that window dies with this page, fall back to the detached menu
            // -- the handler-side null checks pass on a DANGLING pointer, and
            // onFreezeStateChanged fires into it from every freeze event.
            if (mPages[i]->window != nullptr
                && mPageMenuBar == mPages[i]->window->getPageMenu())
                mPageMenuBar = mDetachedPageMenu.get();

            mPages.remove(i);
            // J-8 stage 2: now safe to destroy the engine - the page (and its
            // SliderParameterAttachment-bearing ARIA widgets) is already gone.
            if (isRusty && mProcessor.hasBaySickRustyDrums())
                mProcessor.destroyBaySickRustyDrums();
            // QA-ModelShell TS1: rig-owned engines follow the same page-first
            // ordering -- the view (and its APVTS-attached widgets) is gone,
            // now the model tears the engine down (unregister + settle +
            // destroy).  MUST run BEFORE the sfizz destroys below: the
            // rig-owned Inst chain still holds the spliced Guitars/Basses
            // stage pointer, and the chain's processBlock calls that engine
            // directly without checking the active flags (K-5 fix #5) --
            // removeTab unregisters the strip task and destroys the chain,
            // so the sfizz engine frees with zero referents.  No-ops for
            // tabs that never picked an engine.
            {
                // §6.8: a USER closing the tab orphans its freeze files; the
                // same close arriving via closeAllDynamicTabs (project switch,
                // app quit -- the load shield is up) keeps them, because they
                // are the cache the content stamp reuses on the next load.
                const bool userClose = ! mProcessor.isProjectLoadInProgress();
                auto& rig = mProcessor.engineRig();
                if (layerStripIdx >= 0) rig.removeTab (TabKind::Layers, layerStripIdx, userClose);
                if (bassStripIdx  >= 0) rig.removeTab (TabKind::Bass,   bassStripIdx,  userClose);
                if (drumStripIdx  >= 0) rig.removeTab (TabKind::Drums,  drumStripIdx,  userClose);
                if (clipStripIdx  >= 0) rig.removeTab (TabKind::Clips,  clipStripIdx,  userClose);
                if (voxStripIdx   >= 0) rig.removeTab (TabKind::Vox,    voxStripIdx,   userClose);
                if (instStripIdx  >= 0) rig.removeTab (TabKind::Inst,   instStripIdx,  userClose);
                if (pluginStripIdx >= 0) rig.removeTab (TabKind::Plugins, pluginStripIdx, userClose);
                // The kit's rig tab had NO removal route at all -- it leaked
                // (frozen state, open streamer handles, live orphan-sweep
                // prefixes) across every kit delete and project switch.
                if (isRusty) rig.removeTab (TabKind::Rusty, 0, userClose);
            }
            // K-4 / L-3 (2026-05-05): page AND chain are gone (chain destroyed
            // by removeTab above), now drop the sfizz engine.  Frees the slot
            // index so the user can `+ Add Bass` again after deleting one.
            if (instGuitarsIdx >= 0)
                mProcessor.destroyBaySickGuitars (instGuitarsIdx);
            if (instBassesIdx >= 0)
                mProcessor.destroyBaySickBasses  (instBassesIdx);

            // 2026-05-05 fix: also drop the orphan mixer strip so the slot
            // index is fully reusable.  Without this, addInstChannelAtIndex
            // bails at its `count(idx) > 0` guard on re-add and the spawn
            // flow silently aborts.  Applies to ALL Inst tabs (live-input,
            // BaySickGuitars, BaySickBasses) - the strip lifecycle is tied
            // to tab lifecycle.  APVTS params for the strip are intentionally
            // kept (matches Aux/Vox/Clips remove convention) so re-adding at
            // the same idx restores prior fader/pan/sends.
            if (instStripIdx >= 0 && mMixerPage)
                mMixerPage->removeInstChannel (instStripIdx);
            // QA-C MIX-01 (2026-05-10): mirror the Inst removal so Vox tabs
            // also drop their orphan strip on close.  APVTS params + recording
            // links stay alive (matches Inst convention).
            if (voxStripIdx >= 0 && mMixerPage)
                mMixerPage->removeVoxChannel (voxStripIdx);
            // QA-EffectsReview side-fix (2026-06-06): mirror Inst/Vox for Clips
            // pages -- removeClipChannel (MixerPage) was defined but never called,
            // so a deleted Clips page's Audio strip lingered in the live mixer
            // until reload.  APVTS params stay (matches the Inst/Vox/Aux convention).
            if (clipStripIdx >= 0 && mMixerPage)
                mMixerPage->removeClipChannel (clipStripIdx);
            // MIX-05: the engine pages join the orphan-strip removal (their
            // strips were never removed on close -- the overlap's real cause).
            if (layerStripIdx >= 0 && mMixerPage)
                mMixerPage->removeLayerChannel (layerStripIdx);
            if (bassStripIdx >= 0 && mMixerPage)
                mMixerPage->removeBassChannel (bassStripIdx);
            if (drumStripIdx >= 0 && mMixerPage)
                mMixerPage->removeDrumChannel (drumStripIdx);
            resized();
            refreshAllKitViews();   // D2: drum row freed → kit view shrinks

            // 2026-05-05 dirty-flag wiring: tab close is a project-state
            // change that doesn't go through APVTS.
            if (mProjectManager) mProjectManager->markDirty();

            // G-7: if we just closed the LAST tab of a type, surface the
            // empty-state placeholder so the user has a clear next action.
            // QA-ProjectSave docket 18 (2026-07-26): Layer/Bass/Drum reach zero
            // now too, so all six closeable types are handled here.
            auto remainingOfType = [this] (RibbonTabBar::TabType t)
            {
                int n = 0;
                for (auto* e : mPages)
                    if (e && e->type == t) ++n;
                return n;
            };

            // QA-ModelShell TS4: closing the last instance of a type no
            // longer navigates anywhere -- the TYPE TAB ITSELF disappears from
            // the ribbon (visibleSlotTypes) and comes back through "+".  The
            // empty-state placeholder pages that used to be shown here are
            // retired with it; there is no state left in which the user is
            // looking at a tab that has no instances.
            juce::ignoreUnused (closedPageWasVisible);
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
        // QA-ModelShell TS5: 0 = Rack, 1 = Pre EQ, 2 = Post EQ.  The rack IS
        // the page, so "Rack" is just the tab selection that already happened
        // in onTabSelected; the two EQ entries open their windows for whatever
        // channel the rack window is currently showing.
        // 2026-04-26: channel pre-selection already happened in onTabSelected
        // (only when mLastFXChannel is set, e.g. via FX-button click).  Leave
        // current channel alone here - user explicitly picked a sub-page, not a
        // channel.
        if (mEffectsPage != nullptr && subPageIndex >= 1)
            openEffectEqWindow (mEffectsPage->getCurrentChannelId(), subPageIndex == 1);
        break;

    case RibbonTabBar::TabType::Builder:
        // 0 = Patterns, 1 = Audio Clips, 2 = Automation - drives browser pane.
        if (mBuilderPage) mBuilderPage->setBrowserTab(subPageIndex);
        break;

    case RibbonTabBar::TabType::Drums:
    case RibbonTabBar::TabType::Layers:
    case RibbonTabBar::TabType::Bass:
    {
        // 0 = Player, 1 = Piano Roll, 2 = EQ.  Dispatch to the ACTIVE page
        // for this type (not the last-created legacy ptr, which gets stale
        // once multiple tabs of the same type exist).
        // Layers/Bass sub-tab 1 (Piano Roll) redirects to the unified
        // PianoRollPage with this engine selected - same logic the
        // page-menu-bar pill click uses.  Drums has Piano Roll at sub-tab 2
        // and an additional Drum Kit at sub-tab 0; both redirect.
        const int activeId = mRibbon->getActiveTabForType(type);
        for (auto* entry : mPages)
        {
            if (! entry || entry->ribbonTabId != activeId) continue;
            if (auto* lp = dynamic_cast<LayersPage*>(entry->component.get()))
            {
                if (subPageIndex == 1 && mPianoRollPage)
                {
                    if (mRibbon) mRibbon->selectTab (4);
                    onTabSelected (4);
                    mPianoRollPage->selectEngine ({ EngineKind::Layer, lp->getPageIndex() });
                }
                else lp->switchTab(subPageIndex);
            }
            else if (auto* bp = dynamic_cast<BassPage*>(entry->component.get()))
            {
                if (subPageIndex == 1 && mPianoRollPage)
                {
                    if (mRibbon) mRibbon->selectTab (4);
                    onTabSelected (4);
                    mPianoRollPage->selectEngine ({ EngineKind::Bass, bp->getPageIndex() });
                }
                else bp->switchTab(subPageIndex);
            }
            else if (auto* dp = dynamic_cast<DrumPage*>(entry->component.get()))
            {
                // Drums: 0=DrumKit (redirect), 1=Player, 2=PianoRoll (redirect), 3=EQ.
                if (subPageIndex == 0 && mPianoRollPage)
                {
                    if (mRibbon) mRibbon->selectTab (4);
                    onTabSelected (4);
                    mPianoRollPage->selectEngine ({ EngineKind::DrumKit, 0 });
                }
                else if (subPageIndex == 2 && mPianoRollPage)
                {
                    if (mRibbon) mRibbon->selectTab (4);
                    onTabSelected (4);
                    mPianoRollPage->selectEngine ({ EngineKind::Drum, dp->getPageIndex() });
                }
                else dp->switchTab(subPageIndex);
            }
            break;
        }
        break;
    }

    case RibbonTabBar::TabType::Clip:
    {
        // G-2 (2026-04-28): Clip dropdown sub-pages - 0=Player, 1=Piano Roll
        // (redirect), 2=Pre EQ8 M/S.  Sub-tab 1 jumps to the unified
        // PianoRollPage with this Clip's engine selected.
        const int activeId = mRibbon->getActiveTabForType(type);
        for (auto* entry : mPages)
        {
            if (! entry || entry->ribbonTabId != activeId) continue;
            if (auto* cp = dynamic_cast<ClipsPage*>(entry->component.get()))
            {
                if (subPageIndex == 1 && mPianoRollPage)
                {
                    if (mRibbon) mRibbon->selectTab (4);
                    onTabSelected (4);
                    mPianoRollPage->selectEngine ({ EngineKind::Clip, cp->getPageIndex() });
                }
                else cp->switchTab(subPageIndex);
            }
            break;
        }
        break;
    }

    case RibbonTabBar::TabType::Vox:
    case RibbonTabBar::TabType::Inst:
    {
        // G-4 (2026-04-28): Vox + Inst dropdown sub-pages - 0=Player, 1=EQ.
        // No Piano Roll redirect - these are live-input / recorded-audio
        // destinations, not MIDI-triggered engines.
        const int activeId = mRibbon->getActiveTabForType(type);
        for (auto* entry : mPages)
        {
            if (! entry || entry->ribbonTabId != activeId) continue;
            if (auto* vp = dynamic_cast<VoxPage*>(entry->component.get()))
                vp->switchTab(subPageIndex);
            else if (auto* ip = dynamic_cast<InstPage*>(entry->component.get()))
                ip->switchTab(subPageIndex);
            break;
        }
        break;
    }

    case RibbonTabBar::TabType::Plugins:
    {
        // 0 = Player (the navigation onTabSelected already did), 1 = Piano
        // Roll.  This switch had no Plugins case at all, so the dropdown's
        // Piano Roll entry navigated to the page and then did nothing.
        const int activeId = mRibbon->getActiveTabForType(type);
        for (auto* entry : mPages)
        {
            if (! entry || entry->ribbonTabId != activeId) continue;
            if (auto* pp = dynamic_cast<PluginsPage*>(entry->component.get()))
            {
                if (subPageIndex == 1 && mPianoRollPage)
                {
                    if (mRibbon) mRibbon->selectTab (4);
                    onTabSelected (4);
                    mPianoRollPage->selectEngine ({ EngineKind::Plugin, pp->getPageIndex() });
                }
            }
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
    // D-4: the live-MIDI target follows the visible tab, so a tab switch
    // while typing-keyboard notes are held would send their noteOffs to the
    // NEW tab's engine and strand the old one - release before switching.
    releaseAllTypingNotes();

    // QA-ModelShell TS4: windows are all live at once (FL-style contained
    // workspace), so selecting a tab no longer HIDES the others -- it brings
    // the selected one forward, recreating its window if it was closed.
    mVisiblePage = nullptr;

    // Show selected + update page menu bar title
    for (auto* entry : mPages)
    {
        if (entry->ribbonTabId != tabId) continue;
        // Destroy-on-close: the page may be gone.  Rebuild it from the model
        // before showing (no-op when it is still alive).
        if (entry->component == nullptr) rebuildPageForTab (*entry);
        if (entry->component)
        {
            if (entry->window == nullptr) hostPageInWindow (*entry);
            if (entry->window != nullptr)
            {
                entry->window->toFront (true);
                // Every mPageMenuBar-> call below now configures THIS window's
                // own title-strip menu.
                if (auto* pm = entry->window->getPageMenu()) mPageMenuBar = pm;
            }
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
            // QA-ModelShell TS5: the three sub-tabs and the four right-hand
            // extras are gone.  The channel picker and FX Bypass moved into the
            // window body (they belong with the rows they act on), each EQ is
            // its own window opened from the body's two buttons, and the title
            // strip carries just the rack menu: Save / Load FX Rack Preset plus
            // the VU calibration that used to be a "Meters" button.
            mPageMenuBar->setMidSideVisible (false);
            mPageMenuBar->setMenuBuilder ([this, ep] (juce::Component* anchor)
            {
                juce::PopupMenu m;
                ep->buildTitleMenu (m);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (anchor));
            });
        }
        else if (auto* lp = dynamic_cast<LayersPage*>(mVisiblePage))
        {
            // J-6 EQ unification (2026-05-03): EQ sub-tab removed.  Pre + post
            // EQ live exclusively on the Effects page.  Piano Roll sub-tab (1)
            // remains a nav shortcut to PianoRollPage; Player (0) is the
            // local sub-page.
            // QA-E Sub-Phase A (2026-05-11): SafePointer lifted to outer scope
            // so deep-link sub-tab lambdas can't deref a dangling page pointer
            // after engine swap / project reload / tab delete + re-add.
            juce::Component::SafePointer<LayersPage> safe (lp);
            // The slot lambdas run at CLICK time, when mPageMenuBar can point
            // at ANOTHER window's bar (every framed window has live slots
            // since the project-load sweep) or at a DESTROYED one (bars die
            // with their window).  Capture the bar the lambda was installed
            // on -- never the shared cache.  Same pattern in every branch.
            juce::Component::SafePointer<PageMenuBar> safeBar (mPageMenuBar);

            auto syncPagePresetMenu = [safe, safeBar] (int /*subTabIdx*/)
            {
                auto* bar = safeBar.getComponent();
                if (bar == nullptr) return;
                if (safe.getComponent() == nullptr) return;
                bar->setMenuBuilder (
                    [safe] (juce::Component* anchor)
                    {
                        if (auto* p = safe.getComponent())
                            p->showPageActionsMenu (anchor);
                    });
            };

            mPageMenuBar->setTabSlots({"Player", "Piano Roll"},
                [this, safe, safeBar, syncPagePresetMenu](int i) {
                    auto* p = safe.getComponent();
                    if (p == nullptr) return;
                    if (i == 1)
                    {
                        // QA-E Sub-Phase A (2026-05-11): capture ALL state via
                        // local stack variables BEFORE onTabSelected(4).  The page
                        // switch can destroy both the source page (so `p` becomes
                        // dangling) AND this lambda itself (mPageMenuBar replaces
                        // its callbacks during showPageForTab, freeing the lambda's
                        // capture struct mid-invocation), so any access to `this->X`
                        // or `p->X` after the switch reads freed memory.
                        const int pageIdx = p->getPageIndex();
                        auto* prp = mPianoRollPage;
                        auto* rbn = mRibbon.get();
                        if (rbn != nullptr) rbn->selectTab (4);
                        onTabSelected (4);
                        if (prp != nullptr)
                            prp->selectEngine ({ EngineKind::Layer, pageIdx });
                        return;
                    }
                    p->switchTab(i);
                    auto* bar = safeBar.getComponent();
                    if (bar == nullptr) return;
                    bar->updateTabActive(i);
                    bar->setMidSideVisible(false);
                    syncPagePresetMenu (i);
                }, lp->getActiveTab(), lp->getPageColor());
            syncPagePresetMenu (lp->getActiveTab());
            mPageMenuBar->setMidSideVisible(false);
            mPageMenuBar->setFxRackSlot ([this, safe]
            {
                auto* p = safe.getComponent();
                if (p == nullptr) return;
                // Build the prefix BEFORE the tab switch: the jump replaces
                // this lambda's callback slot mid-invocation (the Sub-Phase A
                // use-after-free family) -- no member/page access after it.
                const juce::String prefix = "mixer_layer_" + juce::String (p->getPageIndex());
                jumpToFxRackForPrefix (prefix);
            });
            // Smoke round 2 (Jeff): per-player Swing Mix knob on this bar,
            // right of FX Rack -- visible on every sub-tab of the page.
            {
                const juce::String swBase = "swing_layer_" + juce::String (lp->getPageIndex());
                auto sb = mProcessor.makeSwingKnobBinding (swBase + "_mix", swBase + "_trunc");
                mPageMenuBar->setSwingKnobSlot (sb.getMix, sb.setMix, sb.getTrunc, sb.setTrunc);
            }
        }
        // QA-ModelShell TS6 (BLU-447) -- MISSED ENTIRELY, fixed TS7 2026-07-30.
        // showPageForTab had NO Plugins branch, so a hosted-instrument page got no
        // slot cluster at all: no Piano Roll button, no FX Rack jump.  The Freeze
        // button was the only control on the bar, because that one is wired from a
        // single unconditional call at the end of this function -- which is the
        // only reason the omission was visible instead of silent.
        //
        // ONE slot, not the usual "Player / Piano Roll" pair (Jeff, 2026-07-30):
        // a PluginsPage has no sub-pages to switch BETWEEN -- it shows the
        // plugin's own editor and nothing else -- so an inert "Player" button
        // would be a control that does nothing.  activeIdx = -1 leaves it
        // unhighlighted, which is correct: you are on the player, not the roll.
        else if (auto* pp = dynamic_cast<PluginsPage*>(mVisiblePage))
        {
            juce::Component::SafePointer<PluginsPage> safe (pp);

            mPageMenuBar->setTabSlots ({"Piano Roll"},
                [this, safe](int) {
                    auto* p = safe.getComponent();
                    if (p == nullptr) return;
                    // Same use-after-free discipline as the Layers branch:
                    // capture every value on the stack BEFORE onTabSelected,
                    // which can destroy both `p` and this lambda.
                    const int pageIdx = p->getPageIndex();
                    auto* prp = mPianoRollPage;
                    auto* rbn = mRibbon.get();
                    if (rbn != nullptr) rbn->selectTab (4);
                    onTabSelected (4);
                    if (prp != nullptr)
                        prp->selectEngine ({ EngineKind::Plugin, pageIdx });
                }, -1, pp->getPageColor());
            mPageMenuBar->setMidSideVisible(false);
            // Hamburger: Save / Load Page Preset + Delete (G-7 parity; this
            // branch installed no menu at all until 2026-08-02).
            mPageMenuBar->setMenuBuilder ([safe] (juce::Component* anchor)
            {
                if (auto* p = safe.getComponent())
                    p->showPageActionsMenu (anchor);
            });
            mPageMenuBar->setFxRackSlot ([this, safe]
            {
                auto* p = safe.getComponent();
                if (p == nullptr) return;
                const juce::String prefix = "mixer_plugin_" + juce::String (p->getPageIndex());
                jumpToFxRackForPrefix (prefix);
            });
            // swing_plugin_N_* already exist (ensureSwingParams) and the scheduler
            // already applies them to plugin notes -- only the knob was missing.
            {
                const juce::String swBase = "swing_plugin_" + juce::String (pp->getPageIndex());
                auto sb = mProcessor.makeSwingKnobBinding (swBase + "_mix", swBase + "_trunc");
                mPageMenuBar->setSwingKnobSlot (sb.getMix, sb.setMix, sb.getTrunc, sb.setTrunc);
            }
        }
        else if (auto* bp = dynamic_cast<BassPage*>(mVisiblePage))
        {
            // J-6 EQ unification (2026-05-03): EQ sub-tab removed; pre+post EQ
            // on Effects page only.  Piano Roll sub-tab redirects to PianoRollPage.
            // QA-E Sub-Phase A (2026-05-11): SafePointer lifted to outer scope.
            juce::Component::SafePointer<BassPage> safe (bp);
            juce::Component::SafePointer<PageMenuBar> safeBar (mPageMenuBar);   // see Layers branch

            auto syncPagePresetMenu = [safe, safeBar] (int /*subTabIdx*/)
            {
                auto* bar = safeBar.getComponent();
                if (bar == nullptr) return;
                if (safe.getComponent() == nullptr) return;
                bar->setMenuBuilder (
                    [safe] (juce::Component* anchor)
                    {
                        if (auto* p = safe.getComponent())
                            p->showPageActionsMenu (anchor);
                    });
            };

            mPageMenuBar->setTabSlots({"Player", "Piano Roll"},
                [this, safe, safeBar, syncPagePresetMenu](int i) {
                    auto* p = safe.getComponent();
                    if (p == nullptr) return;
                    if (i == 1)
                    {
                        // QA-E Sub-Phase A (2026-05-11): capture ALL state via
                        // locals before onTabSelected(4) -- see LayersPage branch
                        // comment for full rationale.
                        const int pageIdx = p->getPageIndex();
                        auto* prp = mPianoRollPage;
                        auto* rbn = mRibbon.get();
                        if (rbn != nullptr) rbn->selectTab (4);
                        onTabSelected (4);
                        if (prp != nullptr)
                            prp->selectEngine ({ EngineKind::Bass, pageIdx });
                        return;
                    }
                    p->switchTab(i);
                    auto* bar = safeBar.getComponent();
                    if (bar == nullptr) return;
                    bar->updateTabActive(i);
                    bar->setMidSideVisible(false);
                    syncPagePresetMenu (i);
                }, bp->getActiveTab(), bp->getPageColor());
            syncPagePresetMenu (bp->getActiveTab());
            mPageMenuBar->setMidSideVisible(false);
            mPageMenuBar->setFxRackSlot ([this, safe]
            {
                auto* p = safe.getComponent();
                if (p == nullptr) return;
                // Prefix built pre-switch -- see the Layers slot comment.
                const juce::String prefix = "mixer_bass_" + juce::String (p->getPageIndex());
                jumpToFxRackForPrefix (prefix);
            });
            // Smoke round 2 (Jeff): per-player Swing Mix knob (see Layers).
            {
                const juce::String swBase = "swing_bass_" + juce::String (bp->getPageIndex());
                auto sb = mProcessor.makeSwingKnobBinding (swBase + "_mix", swBase + "_trunc");
                mPageMenuBar->setSwingKnobSlot (sb.getMix, sb.setMix, sb.getTrunc, sb.setTrunc);
            }
        }
        else if (auto* cp = dynamic_cast<ClipsPage*>(mVisiblePage))
        {
            // 2026-04-28 (G-2): Clips page sub-tabs mirror Layer/Bass shape
            // (Player / Piano Roll / Pre EQ8 M/S).  Piano Roll redirects to
            // PianoRollPage with this Clip's engine selected; Pre EQ8 M/S is
            // a stub placeholder for now.
            // G-7: Page Preset hamburger always installed (Save greys out
            // when no engine; Load Preset works regardless).  Pre EQ8 M/S
            // tab keeps the same menu since the EQ stub has no own menu.
            // QA-E Sub-Phase A (2026-05-11): SafePointer lifted to outer scope.
            juce::Component::SafePointer<ClipsPage> safe (cp);
            juce::Component::SafePointer<PageMenuBar> safeBar (mPageMenuBar);   // see Layers branch

            auto syncPagePresetMenu = [safe, safeBar] (int i)
            {
                juce::ignoreUnused (i);   // menu builder is the same on all sub-tabs
                auto* bar = safeBar.getComponent();
                if (bar == nullptr) return;
                if (safe.getComponent() == nullptr) return;
                bar->setMenuBuilder (
                    [safe] (juce::Component* anchor)
                    {
                        if (auto* p = safe.getComponent())
                            p->showPageActionsMenu (anchor);
                    });
            };

            // J-6 EQ unification (2026-05-03): Pre EQ8 M/S sub-tab removed;
            // Audio insert pre-rack EQ now lives on the Effects page only.
            mPageMenuBar->setTabSlots({"Player", "Piano Roll"},
                [this, safe, safeBar, syncPagePresetMenu](int i) {
                    auto* p = safe.getComponent();
                    if (p == nullptr) return;
                    if (i == 1)
                    {
                        // QA-E Sub-Phase A (2026-05-11): capture ALL state via
                        // locals before onTabSelected(4) -- see LayersPage branch
                        // comment for full rationale.
                        const int pageIdx = p->getPageIndex();
                        auto* prp = mPianoRollPage;
                        auto* rbn = mRibbon.get();
                        if (rbn != nullptr) rbn->selectTab (4);
                        onTabSelected (4);
                        if (prp != nullptr)
                            prp->selectEngine ({ EngineKind::Clip, pageIdx });
                        return;
                    }
                    p->switchTab (i);
                    auto* bar = safeBar.getComponent();
                    if (bar == nullptr) return;
                    bar->updateTabActive (i);
                    bar->setMidSideVisible (false);
                    syncPagePresetMenu (i);
                }, cp->getActiveTab(), cp->getPageColor());
            syncPagePresetMenu (cp->getActiveTab());
            mPageMenuBar->setMidSideVisible (false);
            mPageMenuBar->setFxRackSlot ([this, safe]
            {
                auto* p = safe.getComponent();
                if (p == nullptr) return;
                // Prefix built pre-switch -- see the Layers slot comment.
                const juce::String prefix = "mixer_audio_" + juce::String (p->getPageIndex());
                jumpToFxRackForPrefix (prefix);
            });
        }
        else if (auto* vp = dynamic_cast<VoxPage*>(mVisiblePage))
        {
            // 2026-04-28 (G-4): Vox page - Player + EQ only, no Piano Roll
            // (live-input / recorded-audio destination, not MIDI-triggered).
            // G-7: Page Preset hamburger menu installed regardless of sub-tab
            // since the EQ stub doesn't have its own menu.  Save Page Preset
            // greys out when there's no engine; Load Page Preset stays active
            // so users can apply a saved preset to an engineless Vox tab.
            // QA-E Sub-Phase A (2026-05-11): SafePointer lifted to outer scope.
            juce::Component::SafePointer<VoxPage> safe (vp);
            juce::Component::SafePointer<PageMenuBar> safeBar (mPageMenuBar);   // see Layers branch

            auto syncPagePresetMenu = [safe, safeBar] (int i)
            {
                juce::ignoreUnused (i);
                auto* bar = safeBar.getComponent();
                if (bar == nullptr) return;
                if (safe.getComponent() == nullptr) return;
                bar->setMenuBuilder (
                    [safe] (juce::Component* anchor)
                    {
                        if (auto* p = safe.getComponent())
                            p->showPageActionsMenu (anchor);
                    });
            };

            // H-6b (2026-05-01) / J-6 (2026-05-03): Vox page is BaySickVocal-only;
            // EQ unification dropped the "Pre Rack EQ" sub-tab so Vox now has 5 sub-tabs.
            mPageMenuBar->setTabSlots (VoxPage::getTabLabels(),
                [safe, safeBar, syncPagePresetMenu] (int i) {
                    auto* p = safe.getComponent();
                    if (p == nullptr) return;
                    p->switchTab (i);
                    auto* bar = safeBar.getComponent();
                    if (bar == nullptr) return;
                    bar->updateTabActive (i);
                    bar->setMidSideVisible (false);
                    syncPagePresetMenu (i);
                }, vp->getActiveTab(), vp->getPageColor());
            syncPagePresetMenu (vp->getActiveTab());
            mPageMenuBar->setMidSideVisible (false);
            mPageMenuBar->setFxRackSlot ([this, safe]
            {
                auto* p = safe.getComponent();
                if (p == nullptr) return;
                // Prefix built pre-switch -- see the Layers slot comment.
                const juce::String prefix = "mixer_vox_" + juce::String (p->getPageIndex());
                jumpToFxRackForPrefix (prefix);
            });
            // QA-E Task 4 (2026-05-12): mClipFileLabel removed from VoxPage;
            // file-association lives in PatternManager AudioLibrary now.
        }
        else if (auto* ip = dynamic_cast<InstPage*>(mVisiblePage))
        {
            // 2026-04-28 (G-4): Inst page - Player + EQ only, no Piano Roll
            // (same reasoning as Vox).
            // G-7: Page Preset hamburger menu installed regardless of sub-tab
            // (same reasoning as Vox above).
            // QA-E Sub-Phase A (2026-05-11): SafePointer lifted to outer scope.
            juce::Component::SafePointer<InstPage> safe (ip);
            juce::Component::SafePointer<PageMenuBar> safeBar (mPageMenuBar);   // see Layers branch

            auto syncPagePresetMenu = [safe, safeBar] (int i)
            {
                juce::ignoreUnused (i);
                auto* bar = safeBar.getComponent();
                if (bar == nullptr) return;
                if (safe.getComponent() == nullptr) return;
                bar->setMenuBuilder (
                    [safe] (juce::Component* anchor)
                    {
                        if (auto* p = safe.getComponent())
                            p->showPageActionsMenu (anchor);
                    });
            };

            // I-0b (2026-05-02) / J-6 (2026-05-03): Inst page sub-tabs restructured.
            //   0 = BaySickPedals (placeholder until I-15)
            //   1 = BaySickNAM/IR
            //  (Pre EQ8 M/S removed in J-6 EQ unification - Effects page only)
            // K-3 (2026-05-05): sfizz sources (BaySickGuitars / BaySickBasses)
            // add a "Piano Roll" tab at the end that nav-redirects to the
            // unified PianoRollPage with this engine selected.  Tab dispatch
            // identifies the redirect target by label so adding new tabs
            // (K-5's Player tab) doesn't break the index-based nav.
            const auto labels = ip->getActiveTabLabels();
            mPageMenuBar->setTabSlots(labels,
                [this, safe, safeBar, syncPagePresetMenu, labels](int i) {
                    auto* p = safe.getComponent();
                    if (p == nullptr) return;
                    if (i >= 0 && i < (int) labels.size() && labels[i] == "Piano Roll")
                    {
                        // QA-E Sub-Phase A (2026-05-11): capture ALL state via
                        // local stack variables BEFORE onTabSelected(4).  Two
                        // destruction surfaces: (a) onTabSelected destroys this
                        // page, so `p` becomes dangling; (b) onTabSelected calls
                        // showPageForTab, which calls mPageMenuBar->setTabSlots,
                        // which replaces THIS lambda's callback slot, freeing the
                        // lambda's capture struct mid-invocation -- so `this->X`
                        // access becomes a use-after-free too.  User-repro
                        // 2026-05-11 BaySickBasses Piano Roll click crashed
                        // initially at p->getPageIndex(); after that was
                        // pre-captured it then crashed at this->mPianoRollPage.
                        const auto src    = p->getSource();
                        const int pageIdx = p->getPageIndex();
                        const EngineKind k = (src == InstPage::Source::BaySickGuitars)
                                              ? EngineKind::BaySickGuitars
                                              : EngineKind::BaySickBasses;
                        auto* prp = mPianoRollPage;
                        auto* rbn = mRibbon.get();
                        if (rbn != nullptr) rbn->selectTab (4);
                        onTabSelected (4);
                        if (prp != nullptr)
                            prp->selectEngine ({ k, pageIdx });
                        return;
                    }
                    p->switchTab (i);
                    auto* bar = safeBar.getComponent();
                    if (bar == nullptr) return;
                    bar->updateTabActive (i);
                    bar->setMidSideVisible (false);
                    syncPagePresetMenu (i);
                }, ip->getActiveTab(), ip->getPageColor());
            syncPagePresetMenu (ip->getActiveTab());
            mPageMenuBar->setMidSideVisible (false);
            mPageMenuBar->setFxRackSlot ([this, safe]
            {
                auto* p = safe.getComponent();
                if (p == nullptr) return;
                // Prefix built pre-switch -- see the Layers slot comment.
                // Covers BOTH Inst variants (live input + Guitars/Basses
                // player) -- one branch, one strip family.
                const juce::String prefix = "mixer_inst_" + juce::String (p->getPageIndex());
                jumpToFxRackForPrefix (prefix);
            });
            // Smoke round 2 (Jeff): per-player Swing Mix knob (see Layers).
            {
                const juce::String swBase = "swing_inst_" + juce::String (ip->getPageIndex());
                auto sb = mProcessor.makeSwingKnobBinding (swBase + "_mix", swBase + "_trunc");
                mPageMenuBar->setSwingKnobSlot (sb.getMix, sb.setMix, sb.getTrunc, sb.setTrunc);
            }
            // QA-G3Smoke G-16: sfizz sources (BaySickGuitars / BaySickBasses)
            // now host the program label + Load-program button on the
            // AriaControlPanel title bar (InstPage wires them at setEngine);
            // only live-input pages keep the clip-name label up here (I-0b,
            // mirrors Vox).
            if (ip->getSource() == InstPage::Source::LiveInput)
                if (auto* lbl = ip->getClipFileLabel())
                    mPageMenuBar->addExtraRightComponent (lbl, 240);
        }
        else if (auto* dp = dynamic_cast<DrumPage*>(mVisiblePage))
        {
            // D2 (2026-04-25): Drum Kit added as the first sub-tab.
            // 2026-04-26 (step 2 commit 3): Drum Kit (index 0) and Piano Roll
            // (index 2) are nav shortcuts - clicking them switches to
            // PianoRollPage with that view selected.  Player (1) and EQ (3)
            // remain local sub-pages.
            // G-7: Page Preset hamburger menu - installed when sub-tab is
            // Player (1) only.  EQ (3) hands the menu off to ParametricEQDisplay
            // via syncEQHamburger; Drum Kit (0) + Piano Roll (2) are nav-only.
            // QA-E Sub-Phase A (2026-05-11): SafePointer lifted to outer scope.
            // User-reproduced crash on this branch's Drum Kit sub-tab click
            // after engine swap / project reload; this is the canonical fix
            // site for the page-type-branch use-after-free family.
            juce::Component::SafePointer<DrumPage> safe (dp);
            juce::Component::SafePointer<PageMenuBar> safeBar (mPageMenuBar);   // see Layers branch

            auto syncPagePresetMenu = [safe, safeBar] (int subTabIdx)
            {
                auto* bar = safeBar.getComponent();
                if (bar == nullptr) return;
                const bool onPlayer = (subTabIdx == 1);
                if (onPlayer && safe.getComponent() != nullptr)
                {
                    bar->setMenuBuilder (
                        [safe] (juce::Component* anchor)
                        {
                            if (auto* p = safe.getComponent())
                                p->showPageActionsMenu (anchor);
                        });
                }
            };

            // J-6 EQ unification (2026-05-03): Pre EQ8 M/S sub-tab removed.
            // Tabs: 0 Drum Kit (nav shortcut), 1 Player, 2 Piano Roll (nav shortcut).
            mPageMenuBar->setTabSlots({"Drum Kit", "Player", "Piano Roll"},
                [this, safe, safeBar, syncPagePresetMenu](int i) {
                    auto* p = safe.getComponent();
                    if (p == nullptr) return;
                    if (i == 0)
                    {
                        // QA-E Sub-Phase A (2026-05-11): capture ALL state via
                        // locals before onTabSelected(4) -- see LayersPage branch
                        // comment for full rationale.
                        auto* prp = mPianoRollPage;
                        auto* rbn = mRibbon.get();
                        if (rbn != nullptr) rbn->selectTab (4);
                        onTabSelected (4);
                        if (prp != nullptr)
                            prp->selectEngine ({ EngineKind::DrumKit, 0 });
                        return;
                    }
                    if (i == 2)
                    {
                        // QA-E Sub-Phase A (2026-05-11): capture ALL state via
                        // locals before onTabSelected(4) -- see LayersPage branch
                        // comment for full rationale.
                        const int pageIdx = p->getPageIndex();
                        auto* prp = mPianoRollPage;
                        auto* rbn = mRibbon.get();
                        if (rbn != nullptr) rbn->selectTab (4);
                        onTabSelected (4);
                        if (prp != nullptr)
                            prp->selectEngine ({ EngineKind::Drum, pageIdx });
                        return;
                    }
                    p->switchTab(i);
                    auto* bar = safeBar.getComponent();
                    if (bar == nullptr) return;
                    bar->updateTabActive(i);
                    bar->setMidSideVisible(false);
                    syncPagePresetMenu (i);
                }, dp->getActiveTab(), dp->getPageColor());
            syncPagePresetMenu (dp->getActiveTab());
            mPageMenuBar->setMidSideVisible(false);
            mPageMenuBar->setFxRackSlot ([this, safe]
            {
                auto* p = safe.getComponent();
                if (p == nullptr) return;
                // Prefix built pre-switch -- see the Layers slot comment.
                const juce::String prefix = "mixer_drum_" + juce::String (p->getPageIndex());
                jumpToFxRackForPrefix (prefix);
            });
            // Smoke round 2 (Jeff): per-player Swing Mix knob (see Layers).
            {
                const juce::String swBase = "swing_drum_" + juce::String (dp->getPageIndex());
                auto sb = mProcessor.makeSwingKnobBinding (swBase + "_mix", swBase + "_trunc");
                mPageMenuBar->setSwingKnobSlot (sb.getMix, sb.setMix, sb.getTrunc, sb.setTrunc);
            }
        }
        else if (auto* rp = dynamic_cast<BaySickRustyDrumsPage*>(mVisiblePage))
        {
            // J-8 stage 1 (2026-05-04): 3 sub-tabs (Drum Kit / Player / Piano
            // Roll) matching DrumPage's layout.  Drum Kit (0) hosts the
            // clickable kit-graphic auditioner; Player (1) hosts the ARIA
            // control panel (placeholder until J-8 stage 2 ships); Piano
            // Roll (2) redirects to the unified PianoRollPage with this
            // engine selected.
            // QA-E Sub-Phase A (2026-05-11): SafePointer lifted to outer scope.
            juce::Component::SafePointer<BaySickRustyDrumsPage> safe (rp);
            juce::Component::SafePointer<PageMenuBar> safeBar (mPageMenuBar);   // see Layers branch

            mPageMenuBar->setTabSlots({"Drum Kit", "Player", "Piano Roll"},
                [this, safe, safeBar](int i) {
                    auto* p = safe.getComponent();
                    if (p == nullptr) return;
                    if (i == 2)
                    {
                        // QA-E Sub-Phase A (2026-05-11): capture ALL state via
                        // locals before onTabSelected(4) -- see LayersPage branch
                        // comment for full rationale.
                        auto* prp = mPianoRollPage;
                        auto* rbn = mRibbon.get();
                        if (rbn != nullptr) rbn->selectTab (4);   // 4 = PianoRoll ribbon slot
                        onTabSelected (4);
                        if (prp != nullptr)
                            prp->selectEngine ({ EngineKind::BaySickRustyDrums, 0 });
                        return;
                    }
                    p->switchTab(i);
                    auto* bar = safeBar.getComponent();
                    if (bar == nullptr) return;
                    bar->updateTabActive(i);
                    bar->setMidSideVisible(false);
                }, rp->getActiveTab(), rp->getPageColor());
            mPageMenuBar->setMidSideVisible(false);
            // Smoke round 2 (Jeff): per-player Swing Mix knob (see Layers) --
            // Rusty has no FX Rack slot, so it sits right of the tab cluster.
            {
                auto sb = mProcessor.makeSwingKnobBinding ("swing_rusty_mix", "swing_rusty_trunc");
                mPageMenuBar->setSwingKnobSlot (sb.getMix, sb.setMix, sb.getTrunc, sb.setTrunc);
            }

            // QA-G3Smoke G-16: the Program selector + Player Preset button now
            // live on the BaySickRustyDrums title bar inside AriaControlPanel
            // (the page hosts them at build) -- no PageMenuBar parking.

            // 2026-05-05 consolidation: Save / Load Page Preset goes through
            // the unified PagePresetIO API (PageKind::RustyDrums).  Captures
            // the engine + every Rusty mixer strip + the RustyDrums Bus +
            // every Rusty insert rack with both pre + post EQ - piano roll
            // notes excluded.  Stored under Documents/BaySickDAW/Presets/
            // Rusty Drums Page/My Presets/.
            juce::Component::SafePointer<StandaloneEditor> safeThisRusty (this);
            auto buildRustyPresetCfg = [] (VibeSynthProcessor& processor)
            {
                PagePresetIO::PageChainConfig cfg;
                if (auto* eng = processor.getBaySickRustyDrums())
                {
                    PagePresetIO::EngineSlot slot;
                    slot.engine        = eng;
                    slot.engineApvts   = &eng->apvts;
                    slot.engineLabel   = "Sfizz";
                    slot.engineRootTag = "BaySickRustyDrumsState";
                    slot.enginePrefix  = "brd_";
                    slot.kitLoadCallback = [&processor] (const juce::File& kitPath)
                    {
                        return processor.loadBaySickRustyDrumsKit (kitPath);
                    };
                    cfg.engineSlots.add (slot);
                }
                cfg.stripPrefixes.add ("mixer_rustybus");
                for (int i = 0; i < MixerChannelIds::kMaxRustyStrips; ++i)
                    cfg.stripPrefixes.add ("mixer_rusty_" + juce::String (i));
                cfg.busRackIds.add ("RustyBus");
                cfg.insertRackKindLabel = "Rusty";
                // insertRackIndices empty → capture every Rusty insert.
                return cfg;
            };

            mPageMenuBar->setMenuBuilder (
                [safeThisRusty, buildRustyPresetCfg] (juce::Component* anchor)
                {
                    if (! safeThisRusty) return;
                    constexpr int kIdSave     = 100;
                    constexpr int kIdLoadBase = 1000;

                    juce::PopupMenu m;
                    const bool kitLoaded = safeThisRusty->mProcessor.hasBaySickRustyDrums();
                    m.addItem (kIdSave, "Save Page Preset As...", kitLoaded);

                    juce::Array<juce::File> presetXmls;
                    {
                        juce::PopupMenu loadSub;
                        const auto root = PagePresetIO::myPresetsDirForPageKind (
                            PagePresetIO::PageKind::RustyDrums);
                        if (root.isDirectory())
                        {
                            juce::Array<juce::File> files;
                            root.findChildFiles (files, juce::File::findFiles, false, "*.xml");
                            files.sort();
                            for (auto& f : files)
                            {
                                const int id = kIdLoadBase + presetXmls.size();
                                presetXmls.add (f);
                                loadSub.addItem (id, f.getFileNameWithoutExtension());
                            }
                        }
                        if (presetXmls.isEmpty())
                            loadSub.addItem (-1, "(no page presets saved)", false, false);
                        m.addSubMenu ("Load Page Preset", loadSub);
                    }

                    m.showMenuAsync (
                        juce::PopupMenu::Options().withTargetComponent (anchor),
                        [safeThisRusty, presetXmls, buildRustyPresetCfg] (int r)
                        {
                            if (! safeThisRusty || r <= 0) return;
                            if (r == kIdSave)
                            {
                                auto* aw = new juce::AlertWindow (
                                    "Save Page Preset",
                                    "Enter a name for this Rusty Drums page preset:",
                                    juce::AlertWindow::NoIcon);
                                aw->addTextEditor ("name", "My Rusty Drums Setup");
                                aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
                                aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                                juce::Component::SafePointer<StandaloneEditor> saveSafe (safeThisRusty);
                                aw->enterModalState (true, juce::ModalCallbackFunction::create (
                                    [saveSafe, aw, buildRustyPresetCfg] (int result)
                                    {
                                        std::unique_ptr<juce::AlertWindow> own (aw);
                                        if (result != 1 || ! saveSafe) return;
                                        const auto name = aw->getTextEditorContents ("name").trim();
                                        if (name.isEmpty()) return;

                                        auto dir = PagePresetIO::myPresetsDirForPageKind (
                                            PagePresetIO::PageKind::RustyDrums);
                                        dir.createDirectory();
                                        auto target = dir.getChildFile (name + ".xml");
                                        int n = 2;
                                        while (target.exists())
                                            target = dir.getChildFile (name + " (" + juce::String (n++) + ").xml");

                                        const auto cfg = buildRustyPresetCfg (saveSafe->mProcessor);
                                        const auto xml = PagePresetIO::exportPagePreset (
                                            saveSafe->mProcessor,
                                            PagePresetIO::PageKind::RustyDrums,
                                            cfg);
                                        if (xml.isNotEmpty())
                                            target.replaceWithText (xml);
                                    }), false);
                                return;
                            }
                            if (r >= kIdLoadBase && r < kIdLoadBase + presetXmls.size())
                            {
                                const auto& f = presetXmls[r - kIdLoadBase];
                                const auto cfg = buildRustyPresetCfg (safeThisRusty->mProcessor);
                                PagePresetIO::importPagePreset (
                                    safeThisRusty->mProcessor,
                                    PagePresetIO::PageKind::RustyDrums,
                                    cfg,
                                    f.loadFileAsString());
                                return;
                            }
                        });
                });
        }
        else if (auto* mxp = dynamic_cast<MixerPage*>(mVisiblePage))
        {
            // G-6 (2026-04-29): five add buttons in the Mixer page menu bar.
            // User-spec'd order (left → right):
            //   [Add Vox Bus] [Add Vox Strip] [Add Inst Bus] [Add Inst Strip] [Add Aux Strip]
            // Verified empirically: addExtraRightComponent appends rightward,
            // so the FIRST add lands leftmost.  (The pre-existing comment in
            // the R1 code claimed the opposite - it was wrong.)
            mPageMenuBar->addExtraRightComponent(mxp->getAddVoxBusBtn(),  120);
            mPageMenuBar->addExtraRightComponent(mxp->getAddVoxBtn(),     120);
            mPageMenuBar->addExtraRightComponent(mxp->getAddInstBusBtn(), 120);
            mPageMenuBar->addExtraRightComponent(mxp->getAddInstBtn(),    120);
            mPageMenuBar->addExtraRightComponent(mxp->getAddAuxBtn(),     120);

            // 2026-04-29: Mixer hamburger menu - project-level Pan Law selector.
            // 2026-05-02: + meter latency-compensation toggle (off by default).
            //   Pan Law: Circular (default), Triangular, Square -- matches FL.
            //   Latency Compensate: when on, every meter shows the peak from
            //   N audio blocks ago (where N = output device latency / blockSize)
            //   so the visual lines up with the sound the user actually hears.
            juce::Component::SafePointer<StandaloneEditor> safeThis (this);
            mPageMenuBar->setMenuBuilder (
                [safeThis] (juce::Component* anchor)
                {
                    if (! safeThis) return;
                    juce::PopupMenu m;
                    juce::PopupMenu panLawSub;
                    auto* p = safeThis->mProcessor.apvts.getParameter ("master_pan_law");
                    const int current = p ? (int) ((juce::AudioParameterInt*) p)->get() : 0;
                    panLawSub.addItem (101, "Circular (-3 dB at center)", true, current == 0);
                    panLawSub.addItem (102, "Triangular (-6 dB at center)", true, current == 1);
                    panLawSub.addItem (103, "Square (0 dB at center)", true, current == 2);
                    m.addSubMenu ("Pan Law", panLawSub);

                    // J-A2 (2026-05-04): Master Output channel selector.  Lists
                    // every stereo pair on the active audio device first, then
                    // every mono channel.  IDs:
                    //   300 + firstChannel  -> stereo pair starting at firstChannel
                    //   400 + channelIndex  -> mono on channelIndex
                    juce::PopupMenu masterOutSub;
                    int curFirst = MasterOutputRouting::gFirstOutputChannel.load (std::memory_order_relaxed);
                    bool curMono = MasterOutputRouting::gMasterIsMono.load (std::memory_order_relaxed);
                    if (auto* dev = safeThis->mDeviceManager.getCurrentAudioDevice())
                    {
                        const auto outNames = dev->getOutputChannelNames();
                        const int nOut = outNames.size();
                        // Stereo pairs (consecutive even-indexed pairs).
                        for (int i = 0; i + 1 < nOut; i += 2)
                        {
                            const juce::String label = juce::String (i + 1) + "/" + juce::String (i + 2)
                                + "  (" + outNames[i].trim() + " / " + outNames[i + 1].trim() + ")";
                            const bool ticked = (! curMono) && (curFirst == i);
                            masterOutSub.addItem (300 + i, label, true, ticked);
                        }
                        if (nOut > 0) masterOutSub.addSeparator();
                        for (int i = 0; i < nOut; ++i)
                        {
                            const juce::String label = "Output " + juce::String (i + 1) + " (mono)  ("
                                + outNames[i].trim() + ")";
                            const bool ticked = curMono && (curFirst == i);
                            masterOutSub.addItem (400 + i, label, true, ticked);
                        }
                    }
                    else
                    {
                        masterOutSub.addItem (-1, "(no audio device open)", false, false);
                    }
                    m.addSubMenu ("Master Output", masterOutSub);

                    // H-meter (2026-05-02): latency-compensate meters toggle.
                    const bool latCompOn = MeterLatencyComp::gEnabled.load (std::memory_order_relaxed);
                    m.addItem (201,
                                "Latency-compensate meters",
                                true,                  // enabled
                                latCompOn);            // checked

                    // 2026-05-07 (Batch 10): hot-swappable multi-core rendering
                    // toggle.  Click flips RenderEngine::gMultiThreadedEngineEnabled
                    // immediately.  QA-Ef (2026-05-21): the worker threads
                    // acquire-load this at the top of VibeThreadPool::workerLoop
                    // -- true = full parallel, false = workers park and the audio
                    // thread drains the whole graph itself (single-core diagnostic;
                    // identical dispatcher / task code, zero parallelism).  No
                    // restart needed; the very next audio block picks the new mode.
                    const bool mtOn = RenderEngine::gMultiThreadedEngineEnabled.load (std::memory_order_acquire);
                    m.addItem (202,
                                "Multi-core Rendering",
                                true,                  // enabled
                                mtOn);                 // checked

                    // 2026-05-08 (QA-Md): diagnostic capture for the
                    // MT-no-op-in-Debug investigation.  Click triggers a
                    // 2-second counter capture window and pops an
                    // AlertWindow with the per-thread task distribution.
                    m.addItem (203,
                                "Run MT Diagnostic (2s capture)",
                                true,                  // enabled
                                false);                // not checkable

                    // QA-L-Fix D-11 (2026-07-19): kit-trigger velocity source.
                    // Fixed exists for pads that aren't velocity sensitive.
                    {
                        const bool fixedVel =
                            DrumTriggerVelocity::gUseFixed.load (std::memory_order_acquire);
                        juce::PopupMenu velSub;
                        velSub.addItem (204, "From controller", true, ! fixedVel);
                        velSub.addItem (205, "Fixed",           true,   fixedVel);
                        m.addSubMenu ("MIDI trigger velocity", velSub);
                    }

                    m.showMenuAsync (
                        juce::PopupMenu::Options().withTargetComponent (anchor),
                        [safeThis] (int r)
                        {
                            if (! safeThis) return;
                            if (r >= 101 && r <= 103)
                            {
                                if (auto* param = safeThis->mProcessor.apvts.getParameter ("master_pan_law"))
                                    param->setValueNotifyingHost (
                                        param->convertTo0to1 ((float) (r - 101)));
                                return;
                            }
                            if (r == 201)
                            {
                                // Toggle + recompute compensation block count from
                                // the current device's output latency.  Stored in
                                // the per-project state on save (UI->XML).
                                const bool newOn = ! MeterLatencyComp::gEnabled.load (std::memory_order_relaxed);
                                MeterLatencyComp::gEnabled.store (newOn, std::memory_order_relaxed);
                                const int latSamples = safeThis->mProcessor.getDeviceOutputLatency();
                                const int blockSize  = safeThis->mProcessor.getBlockSize();
                                const double sr      = safeThis->mProcessor.getSampleRate();
                                MeterLatencyComp::recomputeFromDevice (sr, blockSize, latSamples);
                                return;
                            }
                            if (r == 202)
                            {
                                // 2026-05-07 (Batch 10): hot-swap multi-core
                                // rendering.  QA-Ef (2026-05-21): release-store
                                // pairs with the worker threads' acquire-load
                                // in VibeThreadPool::workerLoop; next block
                                // picks the new mode (workers park vs run).
                                // Phase 3 (2026-05-07): persist the new state
                                // to settings.xml so it survives restarts.
                                // Save runs synchronously on the message thread
                                // -- file I/O is brief (settings.xml is small,
                                // ~1 KB) and we'd rather lose the toggle change
                                // than the file under an abrupt shutdown.
                                const bool wasOn = RenderEngine::gMultiThreadedEngineEnabled.load (std::memory_order_acquire);
                                RenderEngine::gMultiThreadedEngineEnabled.store (! wasOn, std::memory_order_release);
                                VibesynthStandaloneApp::saveMultiCoreRenderingPref();
                                return;
                            }
                            if (r == 204 || r == 205)
                            {
                                // QA-L-Fix D-11: hot-swap; the next triggered
                                // hit uses the new source.  Persisted like the
                                // MT toggle above.
                                DrumTriggerVelocity::gUseFixed.store (r == 205,
                                                                      std::memory_order_release);
                                VibesynthStandaloneApp::saveMidiTriggerVelocityPref();
                                return;
                            }
                            if (r == 203)
                            {
                                // 2026-05-08 (QA-Md): 2-second diagnostic
                                // capture.  OkCancel prompt -> on OK,
                                // reset counters, set capture flag, sleep
                                // 2 s on the message thread (UI freezes
                                // briefly; audio thread keeps running),
                                // then snapshot + AlertWindow with the
                                // formatted body.
                                juce::AlertWindow::showOkCancelBox (
                                    juce::MessageBoxIconType::InfoIcon,
                                    "MT Diagnostic",
                                    "Start playback now, then click OK.\n"
                                    "Capture runs for 2 seconds (UI freezes briefly).\n"
                                    "(Cancel to abort.)",
                                    "OK", "Cancel", nullptr,
                                    juce::ModalCallbackFunction::create (
                                        [safeThis] (int result)
                                        {
                                            if (! safeThis) return;
                                            if (result != 1) return;  // 1 = OK, 0 = Cancel

                                            RenderEngine::MtDiagnostic::reset();
                                            RenderEngine::MtDiagnostic::gCaptureOn.store (true, std::memory_order_release);

                                            juce::Thread::sleep (2000);

                                            RenderEngine::MtDiagnostic::gCaptureOn.store (false, std::memory_order_release);
                                            const auto snap = RenderEngine::MtDiagnostic::snapshot();

                                            const bool      mtMode       = RenderEngine::gMultiThreadedEngineEnabled.load (std::memory_order_acquire);
                                            const long long totalSubmits = snap.leavesSubmitted + snap.childSubmits;
                                            const long long totalRun     = snap.mainThreadTasks + snap.workerTasks;
                                            const double    mainPct      = totalRun > 0
                                                ? 100.0 * (double) snap.mainThreadTasks / (double) totalRun
                                                : 0.0;
                                            const double    workerPct    = totalRun > 0
                                                ? 100.0 * (double) snap.workerTasks / (double) totalRun
                                                : 0.0;

                                            juce::String body;
                                            body
                                              << "Build: "
                                            #if JUCE_DEBUG
                                              << "Debug"
                                            #else
                                              << "Release"
                                            #endif
                                              << "    Multi-core: " << (mtMode ? "ON" : "OFF (single-core diagnostic)") << "\n"
                                              << "Capture window: 2 s\n\n"
                                              << "Blocks processed:    " << snap.blockCount       << "\n"
                                              << "Leaves submitted:    " << snap.leavesSubmitted  << "\n"
                                              << "Child submits:       " << snap.childSubmits     << "\n"
                                              << "Total submits:       " << totalSubmits          << "\n"
                                              << "Watchdog fires:      " << snap.watchdogFires    << "\n\n"
                                              << "Main-thread tasks:   " << snap.mainThreadTasks
                                                  << "  (" << juce::String (mainPct,   1) << "%)\n"
                                              << "Worker tasks (all):  " << snap.workerTasks
                                                  << "  (" << juce::String (workerPct, 1) << "%)\n"
                                              << "Total tasks run:     " << totalRun << "\n\n"
                                              << "Worker spin finds:   " << snap.workerSpinFinds  << "\n"
                                              << "Worker sleep finds:  " << snap.workerSleepFinds << "\n"
                                              << "Worker idle sleeps:  " << snap.workerIdleSleeps << "\n"
                                              << "Worker wakes:        " << snap.workerWakes      << "\n";

                                            juce::AlertWindow::showMessageBoxAsync (
                                                juce::MessageBoxIconType::InfoIcon,
                                                "MT Diagnostic Result",
                                                body,
                                                "OK");
                                        }));
                                return;
                            }
                            // J-A2: master output selector handlers.
                            if (r >= 300 && r < 400)
                            {
                                MasterOutputRouting::gFirstOutputChannel.store (r - 300, std::memory_order_relaxed);
                                MasterOutputRouting::gMasterIsMono.store (false, std::memory_order_relaxed);
                                VibesynthStandaloneApp::saveMasterOutputRouting();
                                return;
                            }
                            if (r >= 400 && r < 500)
                            {
                                MasterOutputRouting::gFirstOutputChannel.store (r - 400, std::memory_order_relaxed);
                                MasterOutputRouting::gMasterIsMono.store (true, std::memory_order_relaxed);
                                VibesynthStandaloneApp::saveMasterOutputRouting();
                                return;
                            }
                        });
                });
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

            // Nav pair immediately right of the roll dropdown: "Player Page"
            // lands on the selected roll's engine tab; "FX Rack" lands on its
            // strip on the Effects page.  Same slot row as the pill, so they
            // sit exactly where the dropdown is.
            mPageMenuBar->setTabSlots (
                { pillLabel, "Player Page", "FX Rack" },
                [this] (int slot)
                {
                    if (! mPianoRollPage) return;
                    if (slot == 1) { jumpToRollPlayerPage(); return; }
                    if (slot == 2) { jumpToRollFxRack();     return; }
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

    wireFreezeSlotForVisiblePage();

    resized();
}

// Resolves the currently visible page to the (TabKind, pageIndex) the freeze
// driver is keyed on.  Returns false for anything that is not a player page --
// Mixer, Effects, Builder, Piano Roll -- which is what hides the Freeze slot
// there rather than showing a button with nothing behind it.
bool StandaloneEditor::visiblePageTabIdentity (TabKind& outKind, int& outIndex) const
{
    if (auto* p = dynamic_cast<LayersPage*>  (mVisiblePage)) { outKind = TabKind::Layers;  outIndex = p->getPageIndex(); return true; }
    if (auto* p = dynamic_cast<BassPage*>    (mVisiblePage)) { outKind = TabKind::Bass;    outIndex = p->getPageIndex(); return true; }
    if (auto* p = dynamic_cast<DrumPage*>    (mVisiblePage)) { outKind = TabKind::Drums;   outIndex = p->getPageIndex(); return true; }
    if (auto* p = dynamic_cast<ClipsPage*>   (mVisiblePage)) { outKind = TabKind::Clips;   outIndex = p->getPageIndex(); return true; }
    if (auto* p = dynamic_cast<VoxPage*>     (mVisiblePage)) { outKind = TabKind::Vox;     outIndex = p->getPageIndex(); return true; }
    if (auto* p = dynamic_cast<InstPage*>    (mVisiblePage)) { outKind = TabKind::Inst;    outIndex = p->getPageIndex(); return true; }
    if (auto* p = dynamic_cast<PluginsPage*> (mVisiblePage)) { outKind = TabKind::Plugins; outIndex = p->getPageIndex(); return true; }
    // The kit is a SINGLETON page with no page index of its own -- index 0 by
    // definition.  It is also the one player whose engine is processor-owned
    // rather than rig-owned, so its tab record is pure identity + freeze state.
    if (dynamic_cast<BaySickRustyDrumsPage*> (mVisiblePage) != nullptr)
    { outKind = TabKind::Rusty; outIndex = 0; return true; }
    return false;
}

// ── TS7 §6: the per-player Freeze button (Jeff, 2026-07-30) ──────────────────
// ONE wiring site at the end of showPageForTab rather than a call inside each of
// the seven page branches above.  Those branches already each set the FX Rack
// slot and the swing knob, and that duplication is exactly how a new player type
// ends up silently missing a control.
void StandaloneEditor::wireFreezeSlotForVisiblePage()
{
    if (! mPageMenuBar) return;

    TabKind kind {};
    int     pageIndex = -1;
    if (! visiblePageTabIdentity (kind, pageIndex))
    {
        mPageMenuBar->setFreezeSlot (nullptr, nullptr);   // not a player page
        return;
    }

    // The kit has no rig tab of its own -- its engine is processor-owned, so
    // nothing ever created one, and without a tab record there is nowhere to
    // hold freeze state.  Created lazily here, identity only (null engine); the
    // teardown paths already early-return on that.
    if (kind == TabKind::Rusty && mProcessor.engineRig().findTab (kind, 0) == nullptr)
        mProcessor.engineRig().addTab (TabKind::Rusty, 0, "Drum Kit");

    // HIDDEN BY DEFAULT (Jeff, 2026-07-30).  Freeze ships auto-first: a
    // first-time user never sees a button and never learns the word, the machine
    // simply protects itself at the CPU threshold.  The per-player buttons come
    // back only when a power user ticks "Enable Instrument Level Freeze" beside
    // the threshold slider in File Settings.
    //
    // HIDDEN, not removed -- auto-freeze keeps running underneath either way, so
    // this is purely whether the manual control is on screen.
    if (! openUiPrefs()->getBoolValue ("fsInstrumentFreeze", false))
    {
        mPageMenuBar->setFreezeSlot (nullptr, nullptr);
        return;
    }

    mPageMenuBar->setFreezeSlot (
        [this, kind, pageIndex]() -> int
        {
            auto& rig = mProcessor.engineRig();
            if (! rig.isFrozen (kind, pageIndex))      return 0;
            return rig.isFreezeStale (kind, pageIndex) ? 2 : 1;
        },
        [this, kind, pageIndex] (bool wantFrozen)
        {
            if (! wantFrozen) { mProcessor.unfreezeTab (kind, pageIndex); return; }

            // A freeze is a BLOCKING offline render on the message thread, so
            // without this the app simply stops responding until it finishes --
            // on a long arrangement that reads as a hang rather than as work.
            // Every other heavy operation here already uses this overlay; freeze
            // was the one that did not (found by Jeff asking whether the
            // multi-pattern render would show progress).
            // Determinate (false), not indeterminate: the render reports a real
            // fraction now that onProgress is wired, and cancel is offered
            // because a freeze is abandonable in a way a project load is not.
            HeavyOperationOverlay::ScopedOp heavyOp (
                mHeavyOpOverlay,
                "Freezing " + mPageMenuBar->getPageTitle() + "...", false);
            mHeavyOpOverlay.setCancellable (true);

            juce::String err;
            // byUser = true: this is the ONLY path that sets it, which is why
            // §6.8's "a manual freeze restores on any machine" rule was dead
            // code until this button existed.  The kit routes to its own
            // thirteen-strip render -- freezeTab cannot express it, and probing
            // it through renderTaskForTab is what left this button permanently
            // disabled on the kit page.
            const bool ok = (kind == TabKind::Rusty)
                ? mProcessor.freezeRustyKit (err, /*byUser*/ true, /*reuseValid*/ false)
                : mProcessor.freezeTab (kind, pageIndex, err, /*byUser*/ true);
            if (! ok)
                juce::AlertWindow::showMessageBoxAsync (
                    juce::AlertWindow::WarningIcon, "Could not freeze", err);
        },
        [this, kind, pageIndex]() -> juce::String
        {
            // Shown DISABLED carrying the reason, never hidden.
            const bool freezable = (kind == TabKind::Rusty)
                ? mProcessor.hasBaySickRustyDrums()
                : (mProcessor.renderTaskForTab (kind, pageIndex) != nullptr);
            if (! freezable)
                return "This player cannot be frozen yet.";
            if (mProcessor.getProjectFreezeDir() == juce::File())
                return "Save the project first - the freeze file lives beside it.";
            return {};
        },
        kind == TabKind::Vox);   // §6.9 vocal warning
}

// ─────────────────────────────────────────────────────────────────────────────
// Pattern selector helpers
// ─────────────────────────────────────────────────────────────────────────────
void StandaloneEditor::refreshPatternBox()
{
    if (!mPM || mPM->getNumPatterns() == 0) return;

    // §6.8: the current pattern changed, so every frozen tab's task needs to be
    // pointed at the render for the NEW pattern.  Piggybacks this 10 Hz poll
    // rather than adding a timer -- the poll exists precisely because the current
    // pattern changes from places that cannot call us directly (Builder grid and
    // browser selection, project load).  Change-guarded: republishing every tick
    // would be pointless stores on the audio thread's read pointer.
    {
        const int nowPat = mPM->getCurrentPatternIndex();
        if (nowPat != mLastRepublishedPattern)
        {
            mLastRepublishedPattern = nowPat;
            mProcessor.engineRig().republishPatternSources (nowPat);
        }
    }
    // QA-G Task 6 (docket #2): non-4/4 EFFECTIVE signatures show as a name
    // suffix ("Synths 7/8") -- followers included.
    const auto& curPat = mPM->currentPattern();
    const juce::String tsSuffix = (curPat.tsNum == 4 && curPat.tsDen == 4)
        ? juce::String()
        : " " + juce::String(curPat.tsNum) + "/" + juce::String(curPat.tsDen);
    juce::String label = curPat.name + tsSuffix
                       + "  "
                       + juce::String(juce::CharPointer_UTF8("\xe2\x96\xbe"));  // ▾
    // Change-guarded: also called at 10 Hz by mPatternLabelTimer, so skip the
    // button write (and its repaint) unless the label actually changed.
    if (mPatternBtn && mPatternBtn->getButtonText() != label)
        mPatternBtn->setButtonText(label);
}

// ─────────────────────────────────────────────────────────────────────────────
// Transport
// ─────────────────────────────────────────────────────────────────────────────
void StandaloneEditor::startPlayback(double bpm)
{
    // 2026-04-26 (D-5): precount fires only when both record-arm AND the
    // precount toggle are on; exactly 1 bar of lead-in, recording engages
    // when bar 1 arrives.  QA-G Task 6: the bar is measured at the RECORD
    // POSITION'S signature (song = marker map at the playhead; pattern =
    // the pattern's effective TS) -- duration, click unit, and accent all
    // follow it (a 7/8 count-in is 3.5 quarter-beats of seven 8th clicks).
    if (mPrecountEnabled && mRecordArmed)
    {
        int ciN = 4, ciD = 4;
        if (mTransport && mTransport->isSongMode() && TsMap::isActive())
        {
            const auto bb = TsMap::barBeatAt (juce::jmax (0.0, mPlayHead.getCurrentBeat()));
            ciN = juce::jmax (1, bb.num); ciD = juce::jmax (1, bb.den);
        }
        else if (mPM)
        {
            ciN = juce::jmax (1, mPM->currentPattern().tsNum);
            ciD = juce::jmax (1, mPM->currentPattern().tsDen);
        }
        const double totalBeats = (double) ciN * 4.0 / (double) ciD;   // 1 bar
        const int    delayMs    = juce::roundToInt(totalBeats * (60000.0 / juce::jmax(1.0, bpm)));

        mProcessor.mMetro.countInNum.store(ciN, std::memory_order_relaxed);
        mProcessor.mMetro.countInDen.store(ciD, std::memory_order_relaxed);
        mProcessor.mMetro.countInBpm.store(bpm, std::memory_order_relaxed);
        mProcessor.mMetro.countInActive.store(true, std::memory_order_relaxed);
        mTransport->setPlayState(true, false);
        mCountInTimer.startTimer(delayMs);
    }
    else
    {
        mPlayHead.start();   // G1 review fix: Play never edits tempo (spec E)
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

    // QA-L-Fix (D-6): the user changed this drum's play pitch from the kit
    // menu.  Routed to the unified Piano Roll kit grid because that grid owns
    // the drumRolls undo stack; both kit views read the same data, so one
    // Ctrl+Z restores everywhere.
    dp->onPlayNoteChanged = [this] (int pageIdx, int oldNote, int newNote)
    {
        if (mPianoRollPage)
            if (auto* kit = mPianoRollPage->getDrumKitContainer())
                kit->repitchDrumHits (pageIdx, oldNote, newNote);
        refreshAllKitViews();
    };

    // D2 Batch 4: per-row audition (press-and-hold) routed to the drum's
    // engine via auditionNoteOn / auditionNoteOff.
    // QA-L-Fix (D-4, owner call 2026-07-19): audition fires at the drum's
    // assigned play note so the row preview matches how the drum sounds
    // everywhere else.  `heldNote` latches the pitch used at press --
    // auditionNoteOff targets a specific note, so releasing against a
    // freshly-read (possibly changed) assignment would strand a voice.
    // Shared by both handler copies below via shared_ptr.  Per-ROW rather than
    // one shared slot: press-and-hold is mouse-driven today so rows can't
    // overlap, but a single latch would silently mis-release the moment that
    // stops being true (touch, or a keyboard-driven kit).
    auto heldNotes = std::make_shared<std::array<int, kMaxDrumPages>> ();
    heldNotes->fill (60);
    auto auditionDispatch = [this, heldNotes] (int row, bool on)
    {
        auto list = getKitDrumList();
        if (row < 0 || row >= (int) list.size()) return;
        if (row >= kMaxDrumPages) return;
        const int targetTabId = list[(size_t) row].ribbonTabId;
        for (auto* entry : mPages)
        {
            if (! entry || entry->ribbonTabId != targetTabId) continue;
            if (auto* targetDp = dynamic_cast<DrumPage*> (entry->component.get()))
            {
                auto& held = (*heldNotes)[(size_t) row];
                if (on) held = targetDp->getPlayNote();
                const int n = held;

                auto* eng = targetDp->getEngineProcessor();
                if (auto* s = dynamic_cast<BaySickSynthProcessor*> (eng))
                {
                    if (on) s->auditionNoteOn (n); else s->auditionNoteOff (n);
                }
                else if (auto* v = dynamic_cast<VibePlayerProcessor*> (eng))
                {
                    if (on) v->auditionNoteOn (n); else v->auditionNoteOff (n);
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
        // 2026-04-28 (G-3 follow-up): post-D1.4 unified migration, DrumPage
        // no longer hosts its own kit container - `mDrumKitTab` stays null -
        // so this row-click handler is effectively dead code (PianoRollPage's
        // wireKitView handler is what actually fires).  Kept as a safety net
        // and updated to match the unified pattern (no navigation away,
        // no switchTab(0) which would render the empty redirect sub-tab).
        auto list = getKitDrumList();

        if (row < (int) list.size())
        {
            const int targetId = list[row].ribbonTabId;
            for (auto* entry : mPages)
            {
                if (! entry || entry->ribbonTabId != targetId) continue;
                if (auto* targetDp = dynamic_cast<DrumPage*> (entry->component.get()))
                {
                    if (targetDp->isEngineLocked())
                        targetDp->showContextMenu (anchor, true);   // kit entry point
                    else
                        targetDp->showSoundPicker (anchor);
                }
                return;
            }
        }
        else
        {
            onAddTabRequest (RibbonTabBar::TabType::Drums);
            DrumPage* newDp = dynamic_cast<DrumPage*> (mVisiblePage);
            if (mRibbon) mRibbon->selectTab (4);
            onTabSelected (4);
            if (mPianoRollPage)
                mPianoRollPage->selectEngine ({ EngineKind::DrumKit, 0 });
            if (newDp) newDp->showSoundPicker (anchor);
        }
    });

    // Batch 5: Kit menu - opens Save Kit As / Load Kit popup anchored to the
    // Kit button in this DrumKitContainer's toolbar.
    dp->setKitMenuHandler ([this] (juce::Component* anchor) { showKitMenu (anchor); });

    // 2026-04-26: Global Lock/Unlock - confirmable cross-slot toggle.
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

    // QA-L-Fix (D-4, owner call 2026-07-19): audition fires at the drum's
    // assigned play note so the row preview matches how the drum sounds
    // everywhere else.  `heldNote` latches the pitch used at press --
    // auditionNoteOff targets a specific note, so releasing against a
    // freshly-read (possibly changed) assignment would strand a voice.
    // Shared by both handler copies below via shared_ptr.  Per-ROW rather than
    // one shared slot: press-and-hold is mouse-driven today so rows can't
    // overlap, but a single latch would silently mis-release the moment that
    // stops being true (touch, or a keyboard-driven kit).
    auto heldNotes = std::make_shared<std::array<int, kMaxDrumPages>> ();
    heldNotes->fill (60);
    auto auditionDispatch = [this, heldNotes] (int row, bool on)
    {
        auto list = getKitDrumList();
        if (row < 0 || row >= (int) list.size()) return;
        if (row >= kMaxDrumPages) return;
        const int targetTabId = list[(size_t) row].ribbonTabId;
        for (auto* entry : mPages)
        {
            if (! entry || entry->ribbonTabId != targetTabId) continue;
            if (auto* targetDp = dynamic_cast<DrumPage*> (entry->component.get()))
            {
                auto& held = (*heldNotes)[(size_t) row];
                if (on) held = targetDp->getPlayNote();
                const int n = held;

                auto* eng = targetDp->getEngineProcessor();
                if (auto* s = dynamic_cast<BaySickSynthProcessor*> (eng))
                {
                    if (on) s->auditionNoteOn (n); else s->auditionNoteOff (n);
                }
                else if (auto* v = dynamic_cast<VibePlayerProcessor*> (eng))
                {
                    if (on) v->auditionNoteOn (n); else v->auditionNoteOff (n);
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
        // 2026-04-28 (G-3 follow-up): the prior code navigated to the per-
        // drum DrumPage tab and called switchTab(0) to "stay in kit context".
        // Post-D1.4 unified migration, DrumPage's sub-tab 0 is a REDIRECT
        // (Drum Kit lives on PianoRollPage now) - switching to it left the
        // page rendering an empty content area (the dreaded black page).
        // Fix: do NOT navigate to DrumPage at all.  The user is already on
        // the unified Drum Kit view; open the picker / menu in-place.  The
        // popup is anchored to the kit-row component so positioning works
        // regardless of which page hosts the kit.
        auto list = getKitDrumList();
        if (row < (int) list.size())
        {
            // Existing drum: find its DrumPage instance, open its picker
            // (unlocked) or context menu (locked) without leaving the kit.
            const int targetId = list[row].ribbonTabId;
            for (auto* entry : mPages)
            {
                if (! entry || entry->ribbonTabId != targetId) continue;
                if (auto* targetDp = dynamic_cast<DrumPage*> (entry->component.get()))
                {
                    if (targetDp->isEngineLocked())
                        targetDp->showContextMenu (anchor, true);   // kit entry point
                    else
                        targetDp->showSoundPicker (anchor);
                }
                return;
            }
        }
        else
        {
            // Empty row: spawn a new drum tab (onAddTabRequest auto-navigates
            // to it), then snap back to the unified Drum Kit view and open
            // the picker on the freshly-spawned drum.
            onAddTabRequest (RibbonTabBar::TabType::Drums);
            DrumPage* newDp = dynamic_cast<DrumPage*> (mVisiblePage);

            // Return user to the unified Drum Kit (PianoRoll ribbon = id 4).
            if (mRibbon) mRibbon->selectTab (4);
            onTabSelected (4);
            if (mPianoRollPage)
                mPianoRollPage->selectEngine ({ EngineKind::DrumKit, 0 });

            if (newDp) newDp->showSoundPicker (anchor);
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
    // C.5b: pattern's intrinsic TS feeds the piano roll's bar-line spacing.
    conn.patternTimeSigProvider = [this](int& outNum, int& outDen) {
        outNum = mPM ? mPM->currentPattern().tsNum : 4;
        outDen = mPM ? mPM->currentPattern().tsDen : 4;
    };
    conn.noteColor   = lp->getPageColor();
    conn.displayName = lp->getTabName();
    conn.engineType  = lp->getEngineType();   // QA-D STATE-02 follow-on (empty pre-pick)
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
    // QA-SfzGroup Sub-Q (2026-05-27): BaySickPlayer engines hosting an SFZ
    // with keyswitches expose human-readable labels via VibeSampleManager;
    // returns empty for non-BaySickPlayer engines or non-keyswitch notes.
    conn.keyswitchLabelProvider = [cast](int n) -> juce::String {
        if (auto* v = dynamic_cast<VibePlayerProcessor*>(cast()))
            return v->getSynth().getManager().getKeyswitchLabel(n);
        return {};
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
    conn.patternTimeSigProvider = [this](int& outNum, int& outDen) {
        outNum = mPM ? mPM->currentPattern().tsNum : 4;
        outDen = mPM ? mPM->currentPattern().tsDen : 4;
    };
    conn.noteColor   = bp->getPageColor();
    conn.displayName = bp->getTabName();
    conn.engineType  = bp->getEngineType();   // QA-D STATE-02 follow-on (empty pre-pick)
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
    // QA-SfzGroup Sub-Q (2026-05-27): BaySickPlayer engines hosting an SFZ
    // with keyswitches expose human-readable labels via VibeSampleManager.
    conn.keyswitchLabelProvider = [cast](int n) -> juce::String {
        if (auto* v = dynamic_cast<VibePlayerProcessor*>(cast()))
            return v->getSynth().getManager().getKeyswitchLabel(n);
        return {};
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
    conn.patternTimeSigProvider = [this](int& outNum, int& outDen) {
        outNum = mPM ? mPM->currentPattern().tsNum : 4;
        outDen = mPM ? mPM->currentPattern().tsDen : 4;
    };
    conn.noteColor   = dp->getPageColor();
    conn.displayName = dp->getTabName();
    conn.engineType  = dp->getEngineType();   // QA-D STATE-02 follow-on (empty pre-pick)
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
    // QA-SfzGroup Sub-Q (2026-05-27): BaySickPlayer engines hosting an SFZ
    // with keyswitches expose human-readable labels via VibeSampleManager.
    conn.keyswitchLabelProvider = [cast](int n) -> juce::String {
        if (auto* v = dynamic_cast<VibePlayerProcessor*>(cast()))
            return v->getSynth().getManager().getKeyswitchLabel(n);
        return {};
    };
    mPianoRollPage->registerEngine ({ EngineKind::Drum, dp->getPageIndex() }, std::move (conn));
}

// J-7a (2026-05-03): BaySickRustyDrums singleton - appears in the unified
// PianoRollPage's engine dropdown.  index always 0 (1-instance lock).
// Note dispatch routes to the singleton processor's auditionNote.  J-7b will
// add a drummer-conventional remap (piano-roll MIDI 60+ → kit-native note).
void StandaloneEditor::registerBaySickRustyDrumsPianoRoll()
{
    if (! mPianoRollPage) return;
    PianoRollConnection conn;
    conn.dataAccessor = [this]() -> PianoRollData*
    {
        return mPM ? &mPM->currentPattern().baySickRustyDrumsRoll : nullptr;
    };
    conn.patternTimeSigProvider = [this](int& outNum, int& outDen) {
        outNum = mPM ? mPM->currentPattern().tsNum : 4;
        outDen = mPM ? mPM->currentPattern().tsDen : 4;
    };
    conn.noteColor   = VC::DrumsCol;
    conn.displayName = "BaySickRustyDrums";
    conn.auditionMomentary = [this](int n)
    {
        if (auto* eng = mProcessor.getBaySickRustyDrums())
            eng->auditionNote(n);
    };
    // BaySickRustyDrumsProcessor only exposes auditionNote (one-shot), not
    // press-and-hold auditionOn/Off.  Wire both Hold callbacks to the same
    // one-shot trigger so click-and-hold still fires.
    conn.auditionOn  = conn.auditionMomentary;
    conn.auditionOff = [](int) {};

    // J-7b: per-MIDI-note label provider - pulls from the kit's parsed
    // keymap (e.g. MIDI 38 → "Snare Center", MIDI 42 → "Hi-hat Tight Closed").
    // Returns empty for MIDI notes outside the kit's keymap (which fall
    // through to the default "C5" naming).
    // J-8 (2026-05-04): when the loaded program is Basic, the kit's keymap
    // still lists every articulation (the keymap is shared across programs)
    // but the sounds for missing pieces don't actually trigger.  Filter the
    // label down to empty when the soundName is NOT in the loaded program's
    // channel set so the piano roll keys for those notes show no label
    // (matches the kit-graphic grey-out behavior).  Full keeps every label.
    conn.noteLabelProvider = [this](int midiNote) -> juce::String
    {
        auto* eng = mProcessor.getBaySickRustyDrums();
        if (! eng) return {};
        for (const auto& key : eng->getPianoRollKeymap())
            if (key.midiNote == midiNote)
            {
                bool inProgram = false;
                for (const auto& ch : eng->getChannels())
                    if (ch.name == key.soundName) { inProgram = true; break; }
                if (! inProgram) return {};

                if (key.articulation.isNotEmpty())
                    return key.soundName + " " + key.articulation;
                return key.soundName;
            }
        return {};
    };
    // J-7b: default top note = MIDI 60 (C5) so kit-native MIDI 24..60 fits
    // in roughly two octaves of view.  User can scroll to see the higher
    // percussive-click defines (83..96) on demand.
    conn.defaultTopNote = 60;

    // J-7b: drum kits don't have a meaningful sharp/flat distinction -
    // paint every row as white so engine labels (Snare Center, Hi-hat
    // Tight Closed, etc.) are readable across the full kit range.
    conn.allKeysWhite = true;

    // QA-Sfizz Task 2A: keyswitch label provider for sfizz-driven engine.
    // Mirrors the BaySickPlayer pattern at registerLayerPianoRoll (:5734-5738) /
    // registerBassPianoRoll (:5780-5784) / registerDrumPianoRoll (:5820-5824) /
    // registerClipPianoRoll (:7890-7894).
    conn.keyswitchLabelProvider = [this](int n) -> juce::String
    {
        if (auto* eng = mProcessor.getBaySickRustyDrums())
            return eng->getKeyswitchLabel (n);
        return {};
    };

    mPianoRollPage->registerEngine ({ EngineKind::BaySickRustyDrums, 0 }, std::move (conn));
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
// Templates.  Two on-disk formats, both still loadable:
//
//   v2 (QA-ProjectSave 2026-07-26) - written by saveTemplateAs.  The PROJECT
//   shape minus arrangement content, so every tab type round-trips and the
//   loader is the project restore path:
//     <BaySickTemplate name="..." version="2">
//       <Processor>   ... APVTS + VibeRackStates (mixer, routing, racks, EQ)
//       <UIState>     ... <Tabs> + strip names/orders (no session extras)
//
//   v1 FACTORY (shipped set, generate_factory_templates) - attribute-only:
//     <BaySickTemplate name="..." version="1">
//       <Kit path="TR-808/TR-808 Full.xml"/>    (relative to Kits/Factory/)
//       <Layer slot="N" engine="X" presetPath="..." locked="1"/>
//       <Bass  slot="N" engine="X" presetPath="..." locked="1"/>
//
// A third form existed: v1 USER templates, which saveTemplateAs wrote with
// inline per-tab state that loadTemplate never read (it only ever parsed the
// factory attribute schema above).  Those never round-tripped, so v2 supersedes
// them outright rather than gaining a loader - see docket 9=A.
// ─────────────────────────────────────────────────────────────────────────────
juce::File StandaloneEditor::templatesDir()
{
    return AppPaths::appRoot().getChildFile ("Templates");
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
    const juce::String layerName = nextLayerTabName();   // QA-D STATE-02
    lp->setTabName (layerName);                          // sync internal mTabName
    const int newId = mRibbon->addTab (RibbonTabBar::TabType::Layers, layerName);

    lp->onEngineSelected = [this, newId, pageIdx, lp] {
        const auto* tab = mRibbon->getTabById (newId);
        if (mMixerPage)   mMixerPage->addLayerChannel (pageIdx, tab ? tab->name : "Layer");
        if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
        wireEngineDirtyHook (lp->getEngineProcessor());
        // QA-D STATE-02 follow-on: piano-roll context label.
        if (mPianoRollPage)
            mPianoRollPage->setEngineType ({ EngineKind::Layer, pageIdx }, lp->getEngineType());
    };
    lp->onDeleteRequested = [this, newId] {
        if (! mRibbon) return;
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
        if (mMixerPage) mMixerPage->renameChannel (MixerPage::StripKind::Layer, pageIdx, nm);
        if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Layer, pageIdx }, nm);
    };
    registerLayerPianoRoll (lp);

    auto* entry = new PageEntry();
    entry->ribbonTabId = newId;
    entry->type        = RibbonTabBar::TabType::Layers;
    entry->component   = std::move (page);
    mPages.add (entry);
    hostPageInWindow (*entry);

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
    const juce::String bassName = nextBassTabName();   // QA-D STATE-02
    bp->setTabName (bassName);                         // sync internal mTabName
    const int newId = mRibbon->addTab (RibbonTabBar::TabType::Bass, bassName);

    bp->onEngineSelected = [this, newId, pageIdx, bp] {
        const auto* tab = mRibbon->getTabById (newId);
        if (mMixerPage)   mMixerPage->addBassChannel (pageIdx, tab ? tab->name : "Bass");
        if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
        wireEngineDirtyHook (bp->getEngineProcessor());
        // QA-D STATE-02 follow-on: piano-roll context label.
        if (mPianoRollPage)
            mPianoRollPage->setEngineType ({ EngineKind::Bass, pageIdx }, bp->getEngineType());
    };
    bp->onDeleteRequested = [this, newId] {
        if (! mRibbon) return;
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
        if (mMixerPage) mMixerPage->renameChannel (MixerPage::StripKind::Bass, pageIdx, nm);
        if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Bass, pageIdx }, nm);
    };
    registerBassPianoRoll (bp);

    auto* entry = new PageEntry();
    entry->ribbonTabId = newId;
    entry->type        = RibbonTabBar::TabType::Bass;
    entry->component   = std::move (page);
    mPages.add (entry);
    hostPageInWindow (*entry);

    bp->selectEngine (engine);
    if (presetFile.existsAsFile())
        bp->loadPreset (presetFile);
    if (locked)
        bp->setLocked (true);
    return entry->component.get();
}

// QA-ProjectSave Task 3 (2026-07-26): the dirty gate every template load goes
// through.  loadTemplate was the ONLY File-level teardown with no save prompt --
// picking a template silently discarded unsaved work.  The gate lives here, not
// at the menu entries, so Task 4's submenu (and anything added later) inherits
// it rather than having to remember it.  confirmDiscardChanges runs its
// continuation inline on a clean or blank project, so those load directly;
// Cancel simply never runs it, leaving the current project untouched.
void StandaloneEditor::loadTemplate (const juce::File& templateXml)
{
    if (! templateXml.existsAsFile()) return;

    juce::Component::SafePointer<StandaloneEditor> safeThis (this);
    confirmDiscardChanges ([safeThis, templateXml]
    {
        if (safeThis) safeThis->applyTemplate (templateXml);
    });
}

void StandaloneEditor::applyTemplate (const juce::File& templateXml)
{
    auto parsed = juce::XmlDocument::parse (templateXml);
    if (! parsed || ! parsed->hasTagName ("BaySickTemplate")) return;

    // QA-Ef (2026-05-22): shield the whole template apply -- this is a load-type
    // rebuild (loadKitImpl + spawn*FromTemplate register ~13 render tasks) and
    // runs while the audio device is live (e.g. apply-template mid-playback, or
    // the default New Project below).  Nest-aware: when doFileNew already raised
    // the shield this inherits it (no double-drain, no premature clear); the
    // standalone "Load Template" menu path (the other caller) is its outermost
    // owner.  closeAllDynamicTabs + the inner helpers run under this shield.
    const bool shieldWasUp = mProcessor.isProjectLoadInProgress();
    mProcessor.setProjectLoadInProgress (true);
    if (! shieldWasUp)
        juce::Thread::sleep (30);

    // QA-Ef #4 (2026-05-22): tear down prior-project aux inserts under the
    // shield, before the rebuild.  Pairs with the same call in
    // deserializeProject and doFileNew -- aux strips have no per-tab teardown
    // hook so we clear them explicitly at each load-type entry point.
    mProcessor.clearAllAuxInserts();

    // QA-ProjectSave Task 3 (2026-07-26, docket 9=A): v2 templates carry the
    // project shape, so they restore through the project's own path rather than
    // a second implementation.  Teardown is symmetric to what gets restored --
    // deserializeUIState opens with its own closeAllDynamicTabs, which is
    // correct here because a v2 template replaces every tab type.
    if (parsed->getIntAttribute ("version", 1) >= 2)
    {
        // QA-ProjectSave docket 19 (2026-07-26): loading a template is NEW
        // PROJECT semantics, not a merge.  A template is a rig you start from,
        // so the current song goes with everything else -- patterns, notes,
        // arrangement, audio library.  Without this the old notes survive and
        // play through whatever engines the template happened to put at those
        // indices, which is nobody's intent.
        //
        // Order mirrors doFileNew exactly, then applies the template on top of
        // the blank state.  clearDynamicStrips must precede applyProcessorState:
        // it resets the aux/vox/inst next-index counters, and running it after
        // would rewind them past the aux inserts that call had just registered.
        closeAllDynamicTabs();
        if (mMixerPage) mMixerPage->clearDynamicStrips();
        mProcessor.resetToBlankState();

        mProcessor.applyProcessorState (*parsed);   // mixer / routing / racks / EQ
        deserializeUIState (*parsed);               // tabs + strip names/orders
        // Per-insert racks are replayed INSIDE restoreAudioStripsFromArrangement,
        // after its ensureAudioInsert calls -- exactly where the project load
        // does it.  Calling applyPendingRackStates here instead would consume
        // the stash (it clears itself) before those Audio InsertNodes exist, so
        // their racks would come back empty.
        restoreAudioStripsFromArrangement();
        refreshAllKitViews();
        if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
        mProcessor.setProjectLoadInProgress (shieldWasUp);
        // Engines restoring from a template reference the same external files a
        // project does (NAM captures, sfizz kits, IRs) and report the same way.
        mProcessor.reportMissingFilesIfAny();
        return;
    }

    // ── v1 FACTORY schema below (the shipped template set) ──────────────────
    // 1. Tear down ONLY what this branch can restore.  It rebuilds Layers /
    //    Bass / Drums and nothing else, so Clips / Vox / Inst / Rusty tabs stay
    //    put rather than being destroyed by a load that cannot bring them back.
    //    Deliberately NO clearDynamicStrips() here: that wipes EVERY strip
    //    including the surviving tabs' -- onTabClosed already removes each
    //    closed tab's own strip (MIX-05), which is exactly the scoped subset.
    closeDynamicTabs (TabTeardownScope::LayersBassDrumsOnly);

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
    const auto presetsRoot = AppPaths::appRoot().getChildFile ("Presets");

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

    // QA-Ef (2026-05-22): rebuild complete -- restore prior shield state (clears
    // it only when this call was the outermost owner).
    mProcessor.setProjectLoadInProgress (shieldWasUp);
}

void StandaloneEditor::saveTemplateAs ()
{
    auto* aw = new juce::AlertWindow ("Save Template As",
        "Enter a name for this template.\n\n"
        "Templates save your whole setup - every tab and its sound, the mixer "
        "levels, routing, effects and EQ - but no patterns or arrangement, so "
        "you start writing on a blank canvas.",
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

            // QA-ProjectSave Task 2 (2026-07-26, docket 9=A + 15=B): template v2.
            //
            // v1 wrote a bespoke Layer/Bass/Drum-only schema with inline state
            // that loadTemplate never read back (it only understood the FACTORY
            // attribute form), so user templates did not round-trip at all.  v2
            // reuses the PROJECT shape instead: the same <Processor> + <UIState>
            // children a project.xml carries, minus the arrangement content.
            // Every tab type comes for free, and the loader is the project
            // restore path rather than a second implementation that can drift.
            juce::XmlElement root ("BaySickTemplate");
            root.setAttribute ("name", name);
            root.setAttribute ("version", 2);

            // Mixer strips, routing, racks + EQ.  Emitted FIRST so the child
            // order matches project.xml.
            se->mProcessor.writeProcessorState (root);

            // Tabs + strip names/orders.  Deliberately NOT serializeUIState:
            // that adds the session extras (active tab, scroll position,
            // arrangement view + time selection, metronome, VU calibration,
            // song-loop, piano-roll selection) which are properties of a
            // session, not of a skeleton to start new songs from.
            auto* ui = root.createNewChildElement ("UIState");
            ui->setAttribute ("version", 1);
            se->serializeStructuralUIState (*ui);

            // Dockets 23/24: a template is one loose XML with no folder beside
            // it, so anything it references from outside a stable root has to be
            // adopted into My Samples or the template breaks the moment that file
            // moves.  Project saves deliberately skip this -- they have their own
            // Samples folder that travels with them.
            if (auto* tabs = ui->getChildByName ("Tabs"))
                se->adoptTemplateSampleRefs (*tabs);

            root.writeTo (file, {});
        }), false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Batch 5: Kit save / load
// ─────────────────────────────────────────────────────────────────────────────
juce::File StandaloneEditor::kitsDir()
{
    return AppPaths::appRoot().getChildFile ("Kits");
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
            // (G-7 note 2026-04-29: kits stay drum-sound-only on purpose.
            // EQ / mixer-strip / effects-rack settings are NOT embedded -
            // those live on per-tab Page Presets, which are saved/loaded
            // separately via the page menu bar's hamburger menu.)
            juce::XmlElement root ("BaySickKit");
            root.setAttribute ("name",    name);
            root.setAttribute ("version", "1");

            for (auto& entry : mPages)
            {
                if (! entry || entry->type != RibbonTabBar::TabType::Drums) continue;
                auto* dp = dynamic_cast<DrumPage*> (entry->component.get());
                if (dp == nullptr) continue;
                const juce::String stateXml = dp->exportDrumState();
                if (stateXml.isEmpty()) continue;   // empty slot - skip
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
    // Kit-replace confirmation.  Scans DrumPage engines ONLY, deliberately
    // (docket pick 2): a kit load replaces DrumPage tabs and never touches
    // the Rusty singleton (LIFE-01), so a Rusty-only project loads with no
    // prompt -- nothing gets replaced.  Skipped if the user previously
    // checked "Don't show again".
    bool anyExistingDrum = false;
    for (auto& e : mPages)
    {
        if (! e || e->type != RibbonTabBar::TabType::Drums) continue;
        if (auto* dp = dynamic_cast<DrumPage*> (e->component.get()))
            if (! dp->getEngineType().isEmpty()) { anyExistingDrum = true; break; }
    }
    if (anyExistingDrum && mProjectManager && ! mProjectManager->getSkipKitReplacePrompt())
    {
        bool hasRusty = false;
        for (auto& e : mPages)
            if (e && dynamic_cast<BaySickRustyDrumsPage*> (e->component.get()) != nullptr)
                { hasRusty = true; break; }

        juce::String msg = "This will replace all of your current drum tabs "
                           "with the new kit's drums, do you wish to proceed?";
        if (hasRusty)
            msg += "\n\n(BaySickRustyDrums is not affected by kit loads.)";

        auto dontAsk = std::make_unique<juce::ToggleButton> ("Don't show again");
        dontAsk->setSize (160, 24);
        auto* aw = new juce::AlertWindow ("Replace Drums?",
            msg,
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
    // No existing drums (or prompt opted out) - load directly.
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

    HeavyOperationOverlay::ScopedOp heavyOp (mHeavyOpOverlay, "Loading Kit...", true);
    mHeavyOpOverlay.setStepLabel ("Building drum tabs...");

    // Tear down every existing DrumPage tab.  Drums-only - keep Layers +
    // Bass.  The Rusty tab is ALSO Drums-typed, but a kit load replaces
    // DrumPage kits, never the singleton Rusty engine (LIFE-01) -- filter it
    // out, and close per-id instead of the type-wide ribbon clear so Rusty's
    // ribbon tab survives.  closeTab fires onTabClosed itself, so one call per
    // id = the complete teardown.  (Until 2026-07-26 this needed a `force` flag
    // to get past closeTab's last-of-type guard when the DrumPage being torn
    // down was the sole Drums-typed tab; docket 18 removed that guard.)
    {
        juce::Array<int> drumIds;
        for (auto& e : mPages)
            if (e && e->type == RibbonTabBar::TabType::Drums
                && dynamic_cast<BaySickRustyDrumsPage*> (e->component.get()) == nullptr)
                drumIds.add (e->ribbonTabId);
        for (int id : drumIds)
        {
            if (mRibbon) mRibbon->closeTab (id);
            else         onTabClosed (id);
        }
    }

    // Rebuild.  Supports two <Drum slot="N"> formats:
    //   (a) User-saved (embedded state): <Drum slot="N"><DrumPageState data=base64.../></Drum>
    //       - the data attribute carries the engine's getStateInformation() blob.
    //       Loaded via DrumPage::importDrumState.
    //   (b) Factory format (preset reference):
    //         <Drum slot="N" engine="BaySickSynth" presetPath="BaySickDrums/808 Group/808 Kick.xml" locked="1"/>
    //       - presetPath is relative to Documents/BaySickDAW/Presets/ (includes source folder).
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
        // QA-D STATE-02 (Sub-E): fall back to monotonic Drum counter so even
        // unnamed loads get a sequential number instead of the literal "Drums".
        if (tabName.isEmpty()) tabName = nextDrumTabName();
        const int newId = mRibbon->addTab (RibbonTabBar::TabType::Drums, tabName);
        if (firstNewTabId < 0) firstNewTabId = newId;
        dp->setTabName (tabName);

        const int pageIdx = slot;
        dp->onEngineSelected = [this, newId, pageIdx, dp] {
            const auto* tab = mRibbon->getTabById (newId);
            if (mMixerPage)   mMixerPage->addDrumChannel (pageIdx, tab ? tab->name : "Drums");
            if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
            refreshAllKitViews();
            wireEngineDirtyHook (dp->getEngineProcessor());
            // QA-D STATE-02 follow-on: piano-roll context label.
            if (mPianoRollPage)
                mPianoRollPage->setEngineType ({ EngineKind::Drum, pageIdx }, dp->getEngineType());
        };
        dp->onSoundNameChanged = [this, newId, pageIdx, dp] (const juce::String& nm) {
            if (nm.isEmpty()) return;
            if (mRibbon)    mRibbon->renameTab (newId, nm);
            if (mMixerPage) mMixerPage->renameChannel (MixerPage::StripKind::Drum, pageIdx, nm);
            dp->setTabName (nm);
            refreshAllKitViews();
            if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Drum, pageIdx }, nm);
        };
        dp->onDeleteRequested = [this, newId] {
            if (! mRibbon) return;
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
        mPages.add (entry);
        hostPageInWindow (*entry);

        // Apply state - factory ref or embedded.
        if (isFactoryRef)
        {
            // presetPath is relative to Documents/BaySickDAW/Presets/
            // (it includes the source-folder prefix, e.g. "BaySickDrums/808 Group/808 Kick.xml").
            // engine names the runtime engine type to instantiate (BaySickSynth or BaySickPlayer),
            // which can differ from the source folder.
            const auto presetFile = AppPaths::appRoot()
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

    // QA-ProjectSave docket 18 (2026-07-26): the "ensure at least one Drums tab
    // exists" respawn is gone.  Zero Drums tabs is a legal state now, and the
    // respawn would have silently undone a delete-to-zero (and re-seeded a tab
    // into the next save) every time a kit loaded empty.

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

void StandaloneEditor::stopTransportAndFinalizeRecording()
{
    // R5b: stop committing first so any in-flight buffer block lands in
    // the recorder before playback halts.  Disarm Record on Stop too -
    // matches FL Studio (one-shot record per Play) and prevents surprise
    // re-records on the next Play press.
    // QA-Ea Task 0b (2026-05-18): shared by the manual Stop button AND the
    // song-end auto-stop path (mRequestStop) so play-through end finalizes
    // the recording exactly like pressing Stop - was stopPlayback-only, so
    // the recorder kept writing silence past song end until manual Stop.
    // Forks #25.
    if (mRecordingActive)
    {
        auto res = mProcessor.stopRecording();
        commitRecordingResult (res);
        mRecordingActive = false;
        mPlayHead.setRecording (false);   // 2026-04-30 playhead PositionInfo
    }
    mRecordArmed = false;
    if (mTransport) mTransport->setRecordArmed (false);
    stopPlayback();
    if (mTransport) mTransport->setPlayState (false, false);
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
// ApplicationCommandTarget (Phase A - 2026-04-26)
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
        case BSCommands::cmdExportAudio: doExportAudio(); return true;

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

        // ── Typing-keyboard MIDI (D-4, QA-TransportDisplay) ─────────────
        case BSCommands::cmdToggleTypingKeyboard:
            toggleTypingKeyboard();
            return true;

        // ── Undo / Redo (Phase B-5) ─────────────────────────────────────
        case BSCommands::cmdGlobalUndo: globalUndo(); return true;
        case BSCommands::cmdGlobalRedo: globalRedo(); return true;

        // ── Recording precount (Phase D-5) ──────────────────────────────
        case BSCommands::cmdToggleRecordingPrecount:
            mPrecountEnabled = ! mPrecountEnabled;
            return true;

        // ── Slip / Stretch edit-mode toggle (QA-Ea Task 0c) ─────────────
        // 'S' flips the BuilderPage toolbar's Slip/Stretch dropdown between
        // its two modes.  Mode determines what an edge drag does on an
        // Audio clip.  Pattern / Automation blocks are unaffected.
        case BSCommands::cmdToggleSlipStretchMode:
            if (mBuilderPage)
                if (auto* grid = mBuilderPage->getGrid())
                    grid->toggleEditMode();
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
        index = -1;   // -1 = "any" - selectFirstTabOfType handles it
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

    // No exact match (rare - page closed since last visit).  Try first tab of kind.
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
    // No empty pattern found - leave selection unchanged (F4 covers create).
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

void StandaloneEditor::showRustyDrumsMapWindow()
{
    if (mRustyDrumsMapWin != nullptr)
    {
        mRustyDrumsMapWin->toFront (true);
        return;
    }
    auto* w = new RustyDrumsMapWindow (mProcessor.getBaySickRustyDrums());
    WindowChrome::ownToMainWindow (*w, *this);   // TS7 §9.4
    mRustyDrumsMapWin = w;     // SafePointer - auto-clears when closed
}

void StandaloneEditor::showKeyBindsWindow()
{
    if (mKeyBindsWin != nullptr)
    {
        mKeyBindsWin->toFront (true);
        return;
    }
    auto* w = new KeyBindsWindow (mCmdMgr);
    // TS7 §9.4: owned by the main window instead of always-on-top.  Applied from
    // here rather than inside the window because the owner is the main frame THIS
    // editor lives in, which the window itself has no handle on.
    WindowChrome::ownToMainWindow (*w, *this);
    mKeyBindsWin = w;     // SafePointer - auto-clears when the window deletes itself
}

// QA-ModelShell TS6 (BLU-299): Options > Plugins.  The manager is
// processor-owned, so this window is a pure view over it and closing it loses
// nothing.
void StandaloneEditor::showPluginsWindow()
{
    if (mPluginsWin != nullptr)
    {
        mPluginsWin->toFront (true);
        return;
    }
    auto* w = new PluginsManagerWindow (mProcessor.pluginManager());
    // TS7 §9.4: same owner relationship as every other satellite -- without it
    // this window fell behind the main frame the moment focus returned there.
    WindowChrome::ownToMainWindow (*w, *this);
    mPluginsWin = w;      // SafePointer - auto-clears when the window deletes itself
}

// ── J-6 (2026-05-03): BaySickRustyDrums singleton spawn ──────────────────────
// Triggered by the "+ Add BaySickRustyDrums" entry on the Drums▾ ribbon
// dropdown.  Creates a Drums-type ribbon tab whose mPages component is a
// BaySickRustyDrumsPage (the dedicated singleton page).  1-instance lock
// enforced both at the ribbon (the menu entry hides via
// onIsBaySickRustyDrumsActive) and here (defensive early-return).
void StandaloneEditor::addBaySickRustyDrumsTab()
{
    // 1-instance lock: bail if a BaySickRustyDrumsPage is already registered
    // (regardless of whether its kit has been loaded yet).
    for (auto* entry : mPages)
        if (entry && dynamic_cast<BaySickRustyDrumsPage*>(entry->component.get()))
            return;
    if (! mRibbon) return;

    const int ribbonId = mRibbon->addTab (RibbonTabBar::TabType::Drums,
                                           "BaySickRustyDrums");

    auto page = std::make_unique<BaySickRustyDrumsPage> (mProcessor);
    auto* rawPage = page.get();

    // J-6 (2026-05-03): on successful loadKit, spawn the 13 visible mixer
    // strips.  PluginProcessor::loadBaySickRustyDrumsKit already created the
    // audio-graph InsertNodes; this is the visible-UI side that pairs with them.
    rawPage->onKitLoaded = [this]
    {
        if (! mMixerPage) return;
        mMixerPage->clearAllRustyChannels();
        if (auto* eng = mProcessor.getBaySickRustyDrums())
        {
            const auto& chans = eng->getChannels();
            for (size_t i = 0; i < chans.size() && i < (size_t) MixerChannelIds::kMaxRustyStrips; ++i)
                mMixerPage->addRustyChannelAtIndex ((int) i, chans[i].name);
            // 2026-05-05 dirty-flag wiring: engine exists post-load.
            wireEngineDirtyHook (eng);
            mLastRustyKitFile = eng->getCurrentKitPath();   // LIFE-02 session memory
        }
    };

    // QA-E Task 8 NIT-1 (engine-type half): push the loaded program
    // (Full/Basic) into the piano-roll context label so it reads
    // "{tab} - Full" / "{tab} - Basic" (Rusty was the only engine that
    // never wired conn.engineType -> showed "(no engine)").  Fired from
    // onProgramChanged, NOT onKitLoaded: onKitLoaded fires mid-loadKit
    // BEFORE mCurrentProgram updates, so it pushed the PREVIOUS program
    // (off-by-one).  onProgramChanged fires after mCurrentProgram = target
    // in loadProgram + reloadForProjectRestore, so getEngineType() is the
    // NEW program.
    rawPage->onProgramChanged = [this, rawPage]
    {
        if (mPianoRollPage)
            mPianoRollPage->setEngineType ({ EngineKind::BaySickRustyDrums, 0 },
                                           rawPage->getEngineType());
    };

    // J-7a (2026-05-03): NO ribbon-rename hookup for this engine.
    // BaySickRustyDrums is a singleton - its ribbon tab name represents the
    // engine's identity, not the active kit/program.  Layer/Bass/Drum/Clip
    // rename their tab to the loaded preset name because users juggle many
    // instances; here there is only one, so the tab stays "BaySickRustyDrums".
    // The Player-tab status label inside the page shows the loaded kit name
    // for users who want to see which program is active.

    // J-8 stage 2 (2026-05-04): confirm prompt before deleting.  Wiping a
    // BaySickRustyDrums tab cascades to: the singleton sfizz engine, every
    // Rusty mixer strip + the RustyDrums Bus, every Rusty effect rack + EQ,
    // and the BaySickRustyDrums piano-roll across all patterns.  Same prompt
    // pattern as the program-switch dialog for consistency.
    juce::Component::SafePointer<StandaloneEditor> safeThis (this);
    rawPage->onDeleteRequested = [safeThis, ribbonId, rawPage]
    {
        if (! safeThis) return;

        auto fireDelete = [safeThis, ribbonId, rawPage]
        {
            if (! safeThis) return;
            if (rawPage) rawPage->onDeleteRequested = nullptr;   // re-entry guard
            // closeTab fires onTabClosed → the defensive Rusty-tab-close
            // path in StandaloneEditor::onTabClosed handles the rest:
            // destroying the page (and its SliderParameterAttachment-
            // bearing ARIA widgets) first, then the engine.  Order matters.
            if (safeThis->mRibbon) safeThis->mRibbon->closeTab (ribbonId);
        };

        const juce::String warning =
            "Removing this tab will tear down the player, every Rusty mixer "
            "strip and the RustyDrums Bus, every Rusty effects rack and EQ, "
            "and clear the Rusty piano roll on every pattern.  This cannot "
            "be undone.";

        // QA-ProjectSave Task 11 (docket 16): FND-1 completion.  A dirty page
        // gets the same 3-button prompt as the other six page types; the save
        // chains the J-11 Player Preset (Rusty's only user-writable save)
        // and fires the delete only after the write.  Clean page keeps the
        // original 2-button warning unchanged.
        if (rawPage != nullptr && rawPage->isPatchDirty())
        {
            auto* aw = new juce::AlertWindow (
                "Delete BaySickRustyDrums?",
                warning + "\n\n\"Save Page Preset & Delete\" writes a Player "
                "Preset (program + every knob value) to disk first, then "
                "deletes the tab.",
                juce::AlertWindow::QuestionIcon);
            aw->addButton ("Save Page Preset & Delete", 1,
                           juce::KeyPress (juce::KeyPress::returnKey));
            aw->addButton ("Delete",                    2);
            aw->addButton ("Cancel",                    0,
                           juce::KeyPress (juce::KeyPress::escapeKey));
            aw->enterModalState (true, juce::ModalCallbackFunction::create (
                [rawPage, aw, fireDelete] (int r)
                {
                    std::unique_ptr<juce::AlertWindow> own (aw);
                    if (r == 0) return;
                    if (r == 1 && rawPage != nullptr)
                        rawPage->savePlayerPresetAs (fireDelete);   // chained
                    else if (r == 2)
                        fireDelete();                               // delete only
                }), false);
            return;
        }

        juce::AlertWindow::showOkCancelBox (
            juce::AlertWindow::WarningIcon,
            "Delete BaySickRustyDrums?",
            warning + "  Continue?",
            "Yes, delete", "Cancel", nullptr,
            juce::ModalCallbackFunction::create ([fireDelete] (int result)
            {
                if (result != 1) return;
                fireDelete();
            }));
    };

    auto* entry = new PageEntry();
    entry->ribbonTabId = ribbonId;
    entry->type        = RibbonTabBar::TabType::Drums;
    entry->component   = std::move (page);
    mPages.add (entry);
    hostPageInWindow (*entry);

    // J-7a (2026-05-03): register with the unified PianoRollPage so the
    // engine appears in its dropdown + the BaySickRustyDrumsPage's "Piano
    // Roll" sub-tab can redirect to it.
    registerBaySickRustyDrumsPianoRoll();

    // LIFE-02: a re-add within the session auto-reloads the last kit via the
    // restore primitive (program combo + ARIA panel synced; the loadKit
    // funnel shows its busy sign).  Skipped during project load -- the
    // restore path drives its own reload right after -- and on fresh
    // sessions with no memory.
    if (mLastRustyKitFile.existsAsFile()
        && ! mProcessor.isProjectLoadInProgress())
        rawPage->reloadForProjectRestore (mLastRustyKitFile);

    // J-8 stage 1 (2026-05-04): no auto-load on FIRST spawn.  The user must
    // pick Full or Basic from the "Load Player" dropdown on the sub-tab bar.
    // Until then the kit graphic shows the "Pick a program to begin" overlay.
    // (LIFE-02 above is the session-re-add exception.)
    // Auto-select the newly created tab.  RibbonTabBar::selectTab updates
    // internal state but doesn't fire onTabSelected, so call onTabSelected
    // explicitly to trigger showPageForTab + page-menu setup.
    if (mRibbon) mRibbon->selectTab (ribbonId);
    onTabSelected (ribbonId);
}

// ─────────────────────────────────────────────────────────────────────────────
// K-4 (2026-05-05): "+ Add BaySickGuitars" handler.  Spawns an Inst strip +
// InstPage + Guitars engine all wired together at the lowest free Inst slot
// index.  Cap check (20-page combined Inst total) is enforced by the ribbon
// menu (entry greys when full); this function defensively checks again.
// ─────────────────────────────────────────────────────────────────────────────
void StandaloneEditor::addBaySickGuitarsTab()
{
    if (! mRibbon || ! mMixerPage) return;

    // Find lowest free Inst slot (matches the existing Inst-add path).
    std::array<bool, kMaxInstPages> idxTaken {};
    for (auto* entry : mPages)
    {
        if (! entry || entry->type != RibbonTabBar::TabType::Inst) continue;
        if (auto* ip = dynamic_cast<InstPage*> (entry->component.get()))
        {
            const int i = ip->getPageIndex();
            if (i >= 0 && i < (int) kMaxInstPages) idxTaken[(size_t) i] = true;
        }
    }
    int newIdx = -1;
    for (int i = 0; i < (int) kMaxInstPages; ++i)
        if (! idxTaken[(size_t) i]) { newIdx = i; break; }
    if (newIdx < 0) return;   // cap reached - defensive (ribbon already disables)

    // Step 1: spawn the mixer strip.  Synchronously fires onInstStripAdded →
    // spawnInstTabIfMissing, which creates a default LiveInput InstPage.
    mMixerPage->addInstChannelAtIndex (newIdx);

    // Step 2: locate the just-spawned InstPage so we can flip its source mode.
    InstPage* ip = nullptr;
    int       ribbonId = -1;
    for (auto* entry : mPages)
    {
        if (! entry || entry->type != RibbonTabBar::TabType::Inst) continue;
        if (auto* p = dynamic_cast<InstPage*> (entry->component.get()))
            if (p->getPageIndex() == newIdx)
            {
                ip = p;
                ribbonId = entry->ribbonTabId;
                break;
            }
    }
    if (! ip) return;

    HeavyOperationOverlay::ScopedOp heavyOp (mHeavyOpOverlay,
                                             "Loading Instrument...", true);

    // Step 3: load the default kit BEFORE switching source so the engine
    // exists when rebuildEngineChain runs on setSource.  The race-safe wrapper
    // creates the engine slot on first call + flips the active flag once the
    // SFZ is fully parsed.
    const juce::File defaultKit =
        SampleLibrary::getCoreLibraryDir()
              .getChildFile ("Black&Green Guitars")
              .getChildFile ("Programs")
              .getChildFile ("01-green_keyswitch.sfz");
    const bool kitLoaded = mProcessor.loadBaySickGuitarsKit (newIdx, defaultKit);

    // 2026-05-05 dirty-flag wiring: BaySickGuitars APVTS edits + program
    // swap + per-program cache snapshot all flow through the engine's apvts.
    // Install the markDirty hook now that the engine exists.
    wireEngineDirtyHook (mProcessor.getBaySickGuitars (newIdx));

    // Step 4: flip source → triggers chain rebuild + onSourceChanged callback.
    // rebuildEngineChain pulls the engine pointer from PluginProcessor
    // (non-null after step 3 succeeded; null otherwise - silent until the
    // user manually loads a kit via the K-5 program selector).
    ip->setSource (InstPage::Source::BaySickGuitars);

    // Step 5: hide arm + listen LEDs on the strip - sfizz IS the source.
    if (mMixerPage) mMixerPage->setInstStripNoLiveInput (newIdx, true);

    // Step 6: register with the unified PianoRollPage so the engine appears
    // in the dropdown + the InstPage's "Piano Roll" sub-tab can nav-redirect.
    registerInstSourcePianoRoll (ip);

    // QA-D STATE-02: monotonic counter -- never reuses a deleted Guitar
    // number even when a slot is freed.  Replaces the prior scan-and-count
    // that recomputed N from live tabs (which reused deleted numbers).
    const juce::String tabName = nextGuitarTabName();
    ip->setTabName (tabName);
    if (mRibbon && ribbonId >= 0)
        mRibbon->renameTab (ribbonId, tabName);
    // QA-D STATE-02 follow-on: registerInstSourcePianoRoll above ran with the
    // pre-rename "Inst N" name (set by spawnInstTabIfMissing).  Push the new
    // "Guitar N" name through PianoRollPage so the piano-roll context label
    // reads "Guitar N - BaySickGuitars" instead of stale "Inst N".
    if (mPianoRollPage)
        mPianoRollPage->setEngineDisplayName ({ EngineKind::BaySickGuitars, newIdx }, tabName);

    // Program-name linkage (Jeff, 2026-08-02): the default kit just
    // autoloaded, so the tab takes its program name NOW rather than waiting
    // for the first manual pick.  "Guitar N" above stays the fallback when
    // the kit load failed.  Interactive path only -- restore never runs this.
    if (kitLoaded && ip->onSoundNameChanged)
        ip->onSoundNameChanged (InstPage::prettyProgramName (defaultKit));

    // Step 8: select the new tab (user just asked to add it).
    if (mRibbon && ribbonId >= 0)
    {
        mRibbon->selectTab (ribbonId);
        onTabSelected (ribbonId);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// L-3 (2026-05-05): "+ Add BaySickBasses" handler.  Mirrors the Guitars path
// above - same 8-step flow, just swaps the kit folder + engine API + tab name.
// ─────────────────────────────────────────────────────────────────────────────
void StandaloneEditor::addBaySickBassesTab()
{
    if (! mRibbon || ! mMixerPage) return;

    std::array<bool, kMaxInstPages> idxTaken {};
    for (auto* entry : mPages)
    {
        if (! entry || entry->type != RibbonTabBar::TabType::Inst) continue;
        if (auto* ip = dynamic_cast<InstPage*> (entry->component.get()))
        {
            const int i = ip->getPageIndex();
            if (i >= 0 && i < (int) kMaxInstPages) idxTaken[(size_t) i] = true;
        }
    }
    int newIdx = -1;
    for (int i = 0; i < (int) kMaxInstPages; ++i)
        if (! idxTaken[(size_t) i]) { newIdx = i; break; }
    if (newIdx < 0) return;

    mMixerPage->addInstChannelAtIndex (newIdx);

    InstPage* ip = nullptr;
    int       ribbonId = -1;
    for (auto* entry : mPages)
    {
        if (! entry || entry->type != RibbonTabBar::TabType::Inst) continue;
        if (auto* p = dynamic_cast<InstPage*> (entry->component.get()))
            if (p->getPageIndex() == newIdx)
            {
                ip = p;
                ribbonId = entry->ribbonTabId;
                break;
            }
    }
    if (! ip) return;

    HeavyOperationOverlay::ScopedOp heavyOp (mHeavyOpOverlay,
                                             "Loading Instrument...", true);

    const juce::File defaultKit =
        SampleLibrary::getCoreLibraryDir()
              .getChildFile ("Black&Blue Basses")
              .getChildFile ("Programs")
              .getChildFile ("01-darkblack_keysw.sfz");
    const bool kitLoaded = mProcessor.loadBaySickBassesKit (newIdx, defaultKit);

    wireEngineDirtyHook (mProcessor.getBaySickBasses (newIdx));

    ip->setSource (InstPage::Source::BaySickBasses);

    if (mMixerPage) mMixerPage->setInstStripNoLiveInput (newIdx, true);

    registerInstSourcePianoRoll (ip);

    // QA-D STATE-02 (Sub-D): monotonic counter + plural "Basses" prefix to
    // disambiguate from Bass-slot tabs (which use "Bass N").  Replaces the
    // prior scan-and-count that recomputed N from live tabs.
    const juce::String tabName = nextBassesTabName();
    ip->setTabName (tabName);
    if (mRibbon && ribbonId >= 0)
        mRibbon->renameTab (ribbonId, tabName);
    // QA-D STATE-02 follow-on: push the new "Basses N" name through
    // PianoRollPage so the piano-roll context label reads
    // "Basses N - BaySickBasses" instead of stale "Inst N".
    if (mPianoRollPage)
        mPianoRollPage->setEngineDisplayName ({ EngineKind::BaySickBasses, newIdx }, tabName);

    // Program-name linkage (Jeff, 2026-08-02) -- see the Guitars path above.
    if (kitLoaded && ip->onSoundNameChanged)
        ip->onSoundNameChanged (InstPage::prettyProgramName (defaultKit));

    if (mRibbon && ribbonId >= 0)
    {
        mRibbon->selectTab (ribbonId);
        onTabSelected (ribbonId);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// G-2: Clips ribbon helpers.
// ─────────────────────────────────────────────────────────────────────────────
// G-7 (2026-04-29): empty-state hamburger menus.  Empty states (no
// Clips/Vox/Inst tabs yet) used to leave the hamburger silent.  Now they
// install a "Load Page Preset" submenu so users can recall a saved preset
// without first manually adding a tab.  Picking a preset spawns a new tab
// at the next free index and applies the preset onto it.
void StandaloneEditor::createClipStripAndPage (int row, const juce::String& path, bool allowDuplicate)
{
    // QA-ClipDrop Task 3 (SC-G/H, 2026-06-03): shared strip + Clips-page
    // creation, reused by drag-drop (onAudioClipAdded), "+ Add New Clip", and
    // project reload.  SC-H: the strip is named from the SAMPLE (matching the
    // Clips tab, which spawnClipsTabIfMissing also derives from the filename),
    // NOT the Builder grid row label ("Track N").
    juce::String name = juce::File (path).getFileNameWithoutExtension();
    if (name.isEmpty())
        name = "Audio " + juce::String (row + 1);

    // 2026-04-29 ORDER FIX (carried from onAudioClipAdded): register the Audio
    // InsertNode + APVTS params BEFORE creating the strip, so addAudioChannel's
    // setApvts finds every param and binds the fader/mute/solo/width controls.
    // addAudioRowChannel / ensureAudioInsert / addAudioChannel are all
    // idempotent at an existing row, so repeat calls (reload, re-import) no-op.
    mProcessor.mVibeGraph.addAudioRowChannel (row, name);
    mProcessor.ensureAudioInsert (row, name);
    if (mMixerPage)
        mMixerPage->addAudioChannel (row, name);
    if (mEffectsPage)
        mEffectsPage->rebuildChannelDropdown();

    // Spawn the Clips ribbon tab + page (idempotent: no-op if a ClipsPage
    // already owns this row).  allowDuplicate lets the "New Page"/Duplicate flows
    // reuse this canonical helper (bypass the per-path dedup, keep the strip trio).
    spawnClipsTabIfMissing (row, path, allowDuplicate);
}

void StandaloneEditor::addClipPageFromFile (const juce::File& src)
{
    if (mPM == nullptr) return;

    // SC-J: no project open -> prompt to create one, then retry once it exists
    // (mirror of the drag-drop onDropWithoutProject flow at createBuilderPage).
    if (! (mProjectManager && mProjectManager->hasProject()))
    {
        ClipDropDiag::log ("AddNewClip: NO PROJECT OPEN",
                           "src=" + src.getFullPathName() + " (New-Project prompt follows; clip added on retry)");
        promptForProjectName (
            "New Project",
            "To save your audio, give this project a name.\n\n"
            "A folder will be created at:\n"
            + ProjectManager::getDefaultProjectsRoot().getFullPathName()
            + "\\<name>\\\n\n"
              "Your audio file will be copied into that project's Samples\n"
              "folder automatically.",
            "Untitled Project",
            [this, src] (juce::String name)
            {
                if (! ProjectManager::isValidProjectName (name))
                {
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::MessageBoxIconType::WarningIcon,
                        "Invalid project name",
                        "Project names can't contain < > : \" / \\ | ? * or be\n"
                        "reserved device names (CON, PRN, AUX, NUL, COM1-9,\n"
                        "LPT1-9).  Try adding the clip again.");
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
                addClipPageFromFile (src);   // retry -- hasProject() is now true
            });
        return;
    }

    // Copy the picked file into <project>/Samples/ (returns a project-relative
    // "Samples/<file>" path).  importSample alerts on a genuine copyFileTo
    // failure while a project is open (the held-open 2nd-case trap), so an empty
    // return here means that fired -- log + bail without a second popup.
    const juce::String storedPath = mProjectManager->importSample (src);
    if (storedPath.isEmpty())
    {
        ClipDropDiag::log ("AddNewClip BAIL: copy returned empty",
                           "src=" + src.getFullPathName() + " (project IS open -> copy failed; see importSample reason in log)");
        return;
    }

    // Find the next free Clips page row (mirror onCreateRoutablePage + the
    // onDuplicateFileDrop "New Page" scan).
    int newRow = -1;
    for (int i = 0; i < kMaxClipPages; ++i)
    {
        bool taken = false;
        for (auto* e : mPages)
            if (e && e->type == RibbonTabBar::TabType::Clip)
                if (auto* cp = dynamic_cast<ClipsPage*> (e->component.get()))
                    if (cp->getPageIndex() == i) { taken = true; break; }
        if (! taken) { newRow = i; break; }
    }
    if (newRow < 0)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon,
            "No free Clips page",
            "All " + juce::String (kMaxClipPages) + " Clips pages are in use.  "
            "Close one before adding another.");
        return;
    }

    // SC-G: register the library entry owned by the new Clips page, then create
    // the page + strip -- NO grid block.  The Builder browser repopulates via
    // its diff-based timer once the audio library changes.
    const int ownerCh = MixerChannelIds::audioInsert (newRow);
    mPM->addAudioToLibrary (storedPath, {}, ownerCh);
    if (mBuilderPage) mBuilderPage->notifyArrangementChanged();

    createClipStripAndPage (newRow, storedPath);

    // User explicitly clicked "+ Add New Clip" -> navigate to the new tab.
    for (auto* entry : mPages)
        if (entry && entry->type == RibbonTabBar::TabType::Clip)
            if (auto* cp = dynamic_cast<ClipsPage*> (entry->component.get()))
                if (cp->getPageIndex() == newRow && mRibbon)
                {
                    mRibbon->selectTab (entry->ribbonTabId);
                    onTabSelected (entry->ribbonTabId);
                    break;
                }

    ClipDropDiag::log ("AddNewClip OK",
                       "row=" + juce::String (newRow) + " storedPath=" + storedPath + " (page + strip, no grid block)");
}

void StandaloneEditor::spawnClipsTabIfMissing (int audioRow, const juce::String& path,
                                               bool allowDuplicate)
{
    if (path.isEmpty()) { ClipDropDiag::log ("spawnClips BAIL", "empty path"); return; }
    if (audioRow < 0 || audioRow >= kMaxClipPages) { ClipDropDiag::log ("spawnClips BAIL", "row out of range; row=" + juce::String (audioRow) + " max=" + juce::String (kMaxClipPages)); return; }

    // Idempotent (default): skip if a Clips tab already exists for this audio
    // file (file-based dedup; the same file dropped on multiple Builder rows
    // produces ONE Clips page bound to the FIRST drop's audio row).
    // G-6 (2026-04-29): allowDuplicate=true bypasses this check so the
    // picker's Duplicate flow can spawn a SECOND ClipsPage on the same WAV
    // at a different audio row (with cloned engine state applied separately).
    if (! allowDuplicate)
    {
        for (auto* entry : mPages)
        {
            if (! entry) continue;
            if (entry->type != RibbonTabBar::TabType::Clip) continue;
            if (auto* cp = dynamic_cast<ClipsPage*> (entry->component.get()))
            {
                if (cp->getClipFilePath() == path) { ClipDropDiag::log ("spawnClips DEDUP", "ClipsPage already exists for path=" + path + " (no new page created)"); return; }
            }
        }
    }
    // Always skip if the row slot is already taken (each ClipsPage must own
    // a unique audioRow == mixer_audio_<row> insert).
    for (auto* entry : mPages)
    {
        if (! entry) continue;
        if (entry->type != RibbonTabBar::TabType::Clip) continue;
        if (auto* cp = dynamic_cast<ClipsPage*> (entry->component.get()))
            if (cp->getPageIndex() == audioRow)
            { ClipDropDiag::log ("spawnClips ROW-TAKEN", "row=" + juce::String (audioRow) + " already owned by a ClipsPage (no new page created)"); return; }
    }

    ClipDropDiag::log ("spawnClips CREATE", "creating new ClipsPage; row=" + juce::String (audioRow) + " path=" + path);

    // Page name = the filename without extension (or "Clip N" fallback via
    // QA-D STATE-02 monotonic counter).  Counter only increments when fallback
    // fires, so named-clip loads don't burn counter numbers.
    juce::String tabName = juce::File (path).getFileNameWithoutExtension();
    if (tabName.isEmpty()) tabName = nextClipTabName();

    const int newId = mRibbon->addTab (RibbonTabBar::TabType::Clip, tabName);

    auto cpHolder = std::make_unique<ClipsPage> (audioRow);
    auto* cpRaw = cpHolder.get();
    cpRaw->setTabName (tabName);
    cpRaw->setProcessor (&mProcessor);   // G-7: Page Preset save/load access

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
    // QA-E Task 7 (FILE-02) root-cause fix: engine loads from resolvedPath
    // (absolute), but tag the library with the original STORED/RELATIVE
    // `path` so it dedups against every other (relative) library entry.
    cpRaw->setClipFilePath (resolvedPath, path);

    // QA-ModelShell TS1: engine construction/registration is model-side
    // (EngineRig) -- the view callback only wires the dirty hook onto the
    // just-installed engine.
    cpRaw->onEngineChanged = [this, cpRaw]()
    {
        if (auto* eng = cpRaw->getEngineProcessor())
            wireEngineDirtyHook (eng);
    };

    // G-6 (2026-04-29): right-click engine-picker context menu callbacks.
    cpRaw->onDuplicateRequested = [this, cpRaw] { spawnDuplicateClipsTab (cpRaw); };
    cpRaw->onRenameRequested    = [this, ribbonId = -1, cpRaw]() mutable
    {
        // Resolve the ribbon tab id lazily so we don't capture a stale value
        // (mPages may be in flux during construction).
        if (ribbonId < 0)
            for (auto* entry : mPages)
                if (entry && entry->component.get() == cpRaw)
                    { ribbonId = entry->ribbonTabId; break; }
        if (ribbonId >= 0 && mRibbon) mRibbon->startRename (ribbonId);
    };
    cpRaw->onDeleteRequested = [this, ribbonId = -1, cpRaw]() mutable
    {
        if (ribbonId < 0)
            for (auto* entry : mPages)
                if (entry && entry->component.get() == cpRaw)
                    { ribbonId = entry->ribbonTabId; break; }
        if (ribbonId >= 0 && mRibbon) mRibbon->closeTab (ribbonId);
    };
    cpRaw->onLockChanged = [this, ribbonId = -1, cpRaw]() mutable
    {
        if (ribbonId < 0)
            for (auto* entry : mPages)
                if (entry && entry->component.get() == cpRaw)
                    { ribbonId = entry->ribbonTabId; break; }
        if (ribbonId >= 0 && mRibbon) mRibbon->setTabLocked (ribbonId, cpRaw->isLocked());
    };
    cpRaw->onGetChokeGroup = [this, audioRow]() -> int
    {
        const juce::String pid = "mixer_audio_" + juce::String (audioRow) + "_chokeGroup";
        if (auto* p = mProcessor.apvts.getRawParameterValue (pid))
            return juce::jlimit (0, 16, (int) std::round (p->load()));
        return 0;
    };
    cpRaw->onSetChokeGroup = [this, audioRow] (int g)
    {
        const juce::String pid = "mixer_audio_" + juce::String (audioRow) + "_chokeGroup";
        if (auto* p = mProcessor.apvts.getParameter (pid))
            p->setValueNotifyingHost (
                p->getNormalisableRange().convertTo0to1 ((float) juce::jlimit (0, 16, g)));
    };

    // G-6 (2026-04-29): Clips is BaySickPlayer-only - no engine picker UI.
    // Auto-instantiate the player here AFTER callbacks are wired so the
    // onEngineChanged closure fires the audio-thread registration with the
    // freshly-created processor.  (Was previously a manual user picker
    // choice; NAM/IR removed from Clips entirely since it doesn't fit a
    // sample-playback context - moved to Inst page.)
    cpRaw->selectEngine (ClipsPage::EngineType::BaySickPlayer);

    // G-3: register the piano roll with the unified PianoRollPage so the
    // Piano Roll sub-tab redirect (already wired in showPageForTab Clip
    // branch) finds an actual roll to display.
    registerClipPianoRoll (audioRow, cpRaw);


    auto entry           = std::make_unique<PageEntry>();
    entry->ribbonTabId   = newId;
    entry->type          = RibbonTabBar::TabType::Clip;
    entry->component     = std::move (cpHolder);
    auto* addedEntry = entry.release();
    mPages.add (addedEntry);
    hostPageInWindow (*addedEntry);

    mRibbon->selectTab (newId);
    onTabSelected (newId);
}

// G-6 (2026-04-29): right-click "Duplicate" on a ClipsPage's engine picker.
// Captures full source state, finds the next free audio row, spawns a NEW
// ClipsPage bound to the same WAV file (allowDuplicate bypass), then
// applies the source's state.  No file copy - both pages reference the
// same source file.  Distinct from the Builder browser tree's Duplicate
// which copies the WAV first.
void StandaloneEditor::spawnDuplicateClipsTab (ClipsPage* sourceCp)
{
    if (! sourceCp) return;

    const juce::String savedState = sourceCp->exportClipState();
    const juce::String sourcePath = sourceCp->getClipFilePath();
    if (sourcePath.isEmpty()) return;

    // Find next free audio row not occupied by an existing ClipsPage.
    constexpr int kMaxRows = kMaxClipPages;
    std::array<bool, kMaxRows> rowHasPage {};
    for (auto* entry : mPages)
    {
        if (! entry || entry->type != RibbonTabBar::TabType::Clip) continue;
        if (auto* cp = dynamic_cast<ClipsPage*> (entry->component.get()))
        {
            const int idx = cp->getPageIndex();
            if (idx >= 0 && idx < kMaxRows) rowHasPage[(size_t) idx] = true;
        }
    }
    int targetRow = -1;
    for (int r = 0; r < kMaxRows; ++r)
        if (! rowHasPage[(size_t) r]) { targetRow = r; break; }
    if (targetRow < 0) return;   // Clip cap reached

    // QA-EffectsReview side-fix (2026-06-06): create the strip AND page via the
    // canonical helper (was a bare spawnClipsTabIfMissing -> strip-less duplicate).
    createClipStripAndPage (targetRow, sourcePath, /*allowDuplicate*/ true);

    // Find the freshly-spawned page and apply cloned state.
    for (auto* entry : mPages)
    {
        if (! entry || entry->type != RibbonTabBar::TabType::Clip) continue;
        if (auto* cp = dynamic_cast<ClipsPage*> (entry->component.get()))
        {
            if (cp->getPageIndex() == targetRow)
            {
                cp->importClipState (savedState);
                break;
            }
        }
    }
}

void StandaloneEditor::registerClipPianoRoll (int idx, ClipsPage* cp)
{
    if (! mPianoRollPage || ! cp) return;
    if (! mPM) return;

    PianoRollConnection conn;
    // dataAccessor closure - re-resolves &currentPattern().clipRoll[idx] each
    // tick so pattern switches stay live (mirrors the layer/bass/drum reg).
    auto* pmRaw = mPM.get();
    conn.dataAccessor = [pmRaw, idx]() -> PianoRollData*
    {
        if (! pmRaw) return nullptr;
        if (idx < 0 || idx >= (int) pmRaw->currentPattern().clipRoll.size()) return nullptr;
        return &pmRaw->currentPattern().clipRoll[idx];
    };
    conn.patternTimeSigProvider = [pmRaw](int& outNum, int& outDen) {
        outNum = pmRaw ? pmRaw->currentPattern().tsNum : 4;
        outDen = pmRaw ? pmRaw->currentPattern().tsDen : 4;
    };
    conn.noteColor   = juce::Colour (0xffd4a017);   // VC::Warm - Clips amber
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

    // QA-SfzGroup Sub-Q (2026-05-27): BaySickPlayer engines hosting an SFZ
    // with keyswitches expose human-readable labels via VibeSampleManager.
    conn.keyswitchLabelProvider = [cp](int n) -> juce::String {
        if (auto* v = dynamic_cast<VibePlayerProcessor*>(cp->getEngineProcessor()))
            return v->getSynth().getManager().getKeyswitchLabel(n);
        return {};
    };

    mPianoRollPage->registerEngine ({ EngineKind::Clip, idx }, conn);
}

// QA-ModelShell TS6 (BLU-447): the roll for a hosted VST3 instrument tab.
// Same closure discipline as its siblings -- everything re-resolves per call,
// so a plugin swap through the page's picker needs no re-registration.
void StandaloneEditor::registerPluginPianoRoll (int idx, PluginsPage* pp)
{
    if (! mPianoRollPage || ! pp || ! mPM) return;

    PianoRollConnection conn;
    auto* pmRaw = mPM.get();

    conn.dataAccessor = [pmRaw, idx]() -> PianoRollData*
    {
        if (! pmRaw) return nullptr;
        if (idx < 0 || idx >= (int) pmRaw->currentPattern().pluginRoll.size()) return nullptr;
        return &pmRaw->currentPattern().pluginRoll[idx];
    };
    conn.patternTimeSigProvider = [pmRaw](int& outNum, int& outDen) {
        outNum = pmRaw ? pmRaw->currentPattern().tsNum : 4;
        outDen = pmRaw ? pmRaw->currentPattern().tsDen : 4;
    };
    conn.noteColor   = VC::Purple;
    conn.displayName = pp->getTabName();
    conn.engineType  = pp->getPluginName();

    // AUDITION (fixed 2026-07-31 -- Jeff found roll-keyboard and grid clicks
    // silent on a plugin tab).  My TS6 note here claimed the keyboard reached the
    // plugin "through the live-MIDI path" and so needed no closures.  That was
    // wrong: the roll keyboard and grid clicks call THESE closures and never
    // touch the live path, so leaving them unset meant no audition at all.
    //
    // Every other engine's closure calls auditionNote() on the engine; a hosted
    // third-party plugin has no such API.  Instead we feed the live MIDI
    // collector -- the exact path the typing keyboard already uses, which routes
    // by setLiveMidiTarget, and which PianoRollPage::onEngineSelected has
    // already pointed at this tab (EngineKind::Plugin == target kind 10) by the
    // time any of these can fire.  So the roll's active engine and the live
    // target are the same tab BY CONSTRUCTION, and no new real-time state,
    // audition queue or per-block polling is needed.
    juce::Component::SafePointer<StandaloneEditor> safeThis (this);

    // NO FIXED DURATION (Jeff, 2026-07-31).  A first cut ended the one-shot
    // preview on a 250 ms timer; that matches nothing else in the app.  Every
    // other engine's one-shot is auditionNote(), documented as "noteOff-any,
    // noteOn n" -- it releases whatever was previewing and then RINGS until the
    // next preview or the engine's own envelope ends it.  Mirrored exactly here,
    // so a hosted plugin behaves like every other engine and a sustaining patch
    // sustains.  The held note is message-thread-only state; the tab-switch
    // releaseAllTypingNotes and the transport panic all-notes-off are the
    // existing backstops, the same ones the other engines rely on.
    //
    // The KEYBOARD and GRID-DRAG paths do not come through here at all: both use
    // the On/Off pair below, which is what makes a held key sound for as long as
    // it is held.
    conn.auditionMomentary = [safeThis] (int n)
    {
        auto* self = safeThis.getComponent();
        if (self == nullptr) return;
        if (self->mPluginAuditionHeldNote >= 0)
            self->sendTypingNote (self->mPluginAuditionHeldNote, false);
        self->mPluginAuditionHeldNote = n;
        self->sendTypingNote (n, true);
    };
    conn.auditionOn  = [safeThis] (int n)
    {
        auto* self = safeThis.getComponent();
        if (self == nullptr) return;
        // A press-and-hold supersedes a ringing one-shot, same as the grid's
        // triggerAudition releases its previous note before starting the next.
        if (self->mPluginAuditionHeldNote >= 0)
        {
            self->sendTypingNote (self->mPluginAuditionHeldNote, false);
            self->mPluginAuditionHeldNote = -1;
        }
        self->sendTypingNote (n, true);
    };
    conn.auditionOff = [safeThis] (int n)
    { if (auto* s = safeThis.getComponent()) s->sendTypingNote (n, false); };

    conn.rollMode = PianoRollContainer::RollMode::Standard;

    mPianoRollPage->registerEngine ({ EngineKind::Plugin, idx }, conn);
}

// ─────────────────────────────────────────────────────────────────────────────
// 2026-05-05 dirty-flag wiring helper.  Each per-engine processor owns its
// own APVTS that's invisible to the main PluginProcessor's project-dirty
// listener.  This helper installs a markDirty hook so any APVTS edit on the
// engine flips the project dirty bit.
// ─────────────────────────────────────────────────────────────────────────────
void StandaloneEditor::wireEngineDirtyHook (juce::AudioProcessor* eng)
{
    if (eng == nullptr) return;
    juce::Component::SafePointer<StandaloneEditor> safeThis (this);
    auto hook = [safeThis] {
        if (safeThis && safeThis->mProjectManager) safeThis->mProjectManager->markDirty();
    };
    if (auto* p = dynamic_cast<HarmlessProcessor*>           (eng)) { p->setOnAnyStateChange (hook); return; }
    if (auto* p = dynamic_cast<BaySickSynthProcessor*>       (eng)) { p->setOnAnyStateChange (hook); return; }
    if (auto* p = dynamic_cast<BaySickBassProcessor*>        (eng)) { p->setOnAnyStateChange (hook); return; }
    if (auto* p = dynamic_cast<VibePlayerProcessor*>         (eng)) { p->setOnAnyStateChange (hook); return; }
    if (auto* p = dynamic_cast<BaySickGuitarsProcessor*>     (eng)) { p->setOnAnyStateChange (hook); return; }
    if (auto* p = dynamic_cast<BaySickBassesProcessor*>      (eng)) { p->setOnAnyStateChange (hook); return; }
    if (auto* p = dynamic_cast<BaySickRustyDrumsProcessor*>  (eng)) { p->setOnAnyStateChange (hook); return; }
    if (auto* p = dynamic_cast<BaySickPedalsProcessor*>      (eng)) { p->setOnAnyStateChange (hook); return; }
    if (auto* p = dynamic_cast<BaySickNAMIRProcessor*>       (eng)) { p->setOnAnyStateChange (hook); return; }
    if (auto* p = dynamic_cast<BaySickVocalProcessor*>       (eng))
    {
        p->setOnAnyStateChange (hook);
        // BaySickVocal embeds a NAM/IR sub-processor; wire it too.
        wireEngineDirtyHook (&p->getNamIrProcessor());
        return;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// K-3 (2026-05-05): register / unregister a sfizz-source Inst page
// (BaySickGuitars / BaySickBasses) with the unified PianoRollPage.
// ─────────────────────────────────────────────────────────────────────────────
void StandaloneEditor::registerInstSourcePianoRoll (InstPage* ip)
{
    if (! mPianoRollPage || ! ip || ! mPM) return;
    const auto src = ip->getSource();
    if (src == InstPage::Source::LiveInput) return;   // nothing to register

    const int idx = ip->getPageIndex();
    if (idx < 0 || idx >= (int) kMaxInstPages) return;

    const EngineKind kind = (src == InstPage::Source::BaySickGuitars)
                                ? EngineKind::BaySickGuitars
                                : EngineKind::BaySickBasses;

    PianoRollConnection conn;
    auto* pmRaw = mPM.get();
    conn.dataAccessor = [pmRaw, idx]() -> PianoRollData*
    {
        if (! pmRaw) return nullptr;
        if (idx < 0 || idx >= (int) pmRaw->currentPattern().instRoll.size()) return nullptr;
        return &pmRaw->currentPattern().instRoll[idx];
    };
    conn.patternTimeSigProvider = [pmRaw](int& outNum, int& outDen) {
        outNum = pmRaw ? pmRaw->currentPattern().tsNum : 4;
        outDen = pmRaw ? pmRaw->currentPattern().tsDen : 4;
    };
    conn.noteColor   = juce::Colour (0xff1c3a8a);   // Inst navy
    conn.displayName = ip->getTabName();
    // QA-D STATE-02 follow-on: engineType matches the user-facing brand-mixed-
    // case name shown in the BaySick* family.  Picked from the source enum
    // since Inst tabs don't have a getEngineType() accessor (the source IS
    // the engine selection -- LiveInput / BaySickGuitars / BaySickBasses).
    conn.engineType  = (src == InstPage::Source::BaySickGuitars) ? juce::String ("BaySickGuitars")
                                                                  : juce::String ("BaySickBasses");

    // Audition routes directly to the per-instance sfizz processor.  PluginProcessor
    // owns the engine; query it each call so a kit-reload between clicks doesn't
    // leave a stale pointer.
    auto* proc = &mProcessor;
    if (kind == EngineKind::BaySickGuitars)
    {
        conn.auditionMomentary = [proc, idx](int n)
        {
            if (auto* eng = proc->getBaySickGuitars (idx))
                eng->auditionNote (n);
        };
        conn.auditionOn  = conn.auditionMomentary;
        conn.auditionOff = [](int) {};
        // QA-Sfizz Task 2B: keyswitch label provider for BaySickGuitars.
        // Mirrors BaySickRustyDrums Task 2A pattern at :5895; per-instance
        // engine access via proc->getBaySickGuitars(idx) (vs Rusty's singleton).
        conn.keyswitchLabelProvider = [proc, idx](int n) -> juce::String
        {
            if (auto* eng = proc->getBaySickGuitars (idx))
                return eng->getKeyswitchLabel (n);
            return {};
        };
        // QA-SlideSampler Task 4: Guitars use the engine-aware note-props panel
        // (Flat/RP Slide/Bend); Bend dropdown gated to the patch's native range.
        conn.noteEditContextProvider = [proc, idx]() -> PianoRollGrid::NoteEditContext
        {
            PianoRollGrid::NoteEditContext ctx;
            ctx.engineAware = true;
            if (auto* eng = proc->getBaySickGuitars (idx))
            {
                ctx.bendUpSemis   = eng->getSlideRegions().bendMaxUpSemis();
                ctx.bendDownSemis = eng->getSlideRegions().bendMaxDownSemis();
            }
            return ctx;
        };
    }
    else if (kind == EngineKind::BaySickBasses)
    {
        conn.auditionMomentary = [proc, idx](int n)
        {
            if (auto* eng = proc->getBaySickBasses (idx))
                eng->auditionNote (n);
        };
        conn.auditionOn  = conn.auditionMomentary;
        conn.auditionOff = [](int) {};
        // L-4 (2026-05-05): bass range - default top of view to C4 (MIDI 48)
        // so the user lands on the playable register on first open.
        conn.defaultTopNote = 48;
        // QA-Sfizz Task 2B: keyswitch label provider for BaySickBasses.
        // Mirrors BaySickRustyDrums Task 2A pattern at :5895; per-instance
        // engine access via proc->getBaySickBasses(idx) (vs Rusty's singleton).
        conn.keyswitchLabelProvider = [proc, idx](int n) -> juce::String
        {
            if (auto* eng = proc->getBaySickBasses (idx))
                return eng->getKeyswitchLabel (n);
            return {};
        };
        // QA-SlideSampler Task 4: Basses use the engine-aware note-props panel.
        conn.noteEditContextProvider = [proc, idx]() -> PianoRollGrid::NoteEditContext
        {
            PianoRollGrid::NoteEditContext ctx;
            ctx.engineAware = true;
            if (auto* eng = proc->getBaySickBasses (idx))
            {
                ctx.bendUpSemis   = eng->getSlideRegions().bendMaxUpSemis();
                ctx.bendDownSemis = eng->getSlideRegions().bendMaxDownSemis();
            }
            return ctx;
        };
    }

    conn.rollMode = PianoRollContainer::RollMode::Standard;

    mPianoRollPage->registerEngine ({ kind, idx }, conn);
}

void StandaloneEditor::unregisterInstSourcePianoRoll (InstPage* ip)
{
    if (! mPianoRollPage || ! ip) return;
    const auto src = ip->getSource();
    if (src == InstPage::Source::LiveInput) return;
    const int idx = ip->getPageIndex();
    const EngineKind kind = (src == InstPage::Source::BaySickGuitars)
                                ? EngineKind::BaySickGuitars
                                : EngineKind::BaySickBasses;
    mPianoRollPage->unregisterEngine ({ kind, idx });
}

// ─────────────────────────────────────────────────────────────────────────────
// G-4 (2026-04-28): Vox + Inst empty-state + spawn + piano-roll registration.
// Mirror of the G-2/G-3 Clips helpers - same shape, different page color +
// different default engine list.
// ─────────────────────────────────────────────────────────────────────────────
// QA-ModelShell TS4 (2026-07-28): hideAllEmptyStates + the six show*EmptyState
// pages are GONE, and so is the state they existed to render.  A type tab is
// only in the ribbon while it has >= 1 instance now (RibbonTabBar::
// visibleSlotTypes), so "you are looking at a tab with nothing in it" is no
// longer reachable -- the tab itself leaves and returns through "+".  This is
// the loud reversal of docket 18's Task 1 shape that the batch plan calls for.

void StandaloneEditor::spawnVoxTabIfMissing (int voxIdx, bool selectAfter)
{
    if (voxIdx < 0 || voxIdx >= kMaxVoxPages) return;

    // Idempotent on pageIndex.
    for (auto* entry : mPages)
    {
        if (! entry || entry->type != RibbonTabBar::TabType::Vox) continue;
        if (auto* vp = dynamic_cast<VoxPage*> (entry->component.get()))
            if (vp->getPageIndex() == voxIdx) return;
    }

    const juce::String tabName = nextVoxTabName();   // QA-D STATE-02
    const int newId = mRibbon->addTab (RibbonTabBar::TabType::Vox, tabName);

    auto cpHolder = std::make_unique<VoxPage> (mProcessor, voxIdx);
    auto* cpRaw = cpHolder.get();
    cpRaw->setTabName (tabName);
    cpRaw->setProcessor (&mProcessor);   // G-7: Page Preset save/load access
    cpRaw->setUndoContext (makeUndoContext());   // QA-Fd 9a: pitch editor global undo
    // QA-Fd #7: pitch editor playhead follows the main transport (incl. the
    // stop-reset seek) -- getCurrentBeat is the UI-safe playhead accessor.
    cpRaw->setTransportBeatProvider ([this] { return mPlayHead.getCurrentBeat(); });
    cpRaw->setTransportSeekProvider ([this] (double b) { mPlayHead.seekTo (b); });
    cpRaw->setSongTimeSelProviders (
        [this] (float s, float e)
        {
            if (mBuilderPage == nullptr) return;
            if (e > s) mBuilderPage->setTimeSelectionBars (s, e);
            else       mBuilderPage->clearTimeSelectionBars();
        },
        [this] (float& s, float& e) -> bool
        {
            if (mBuilderPage != nullptr && mBuilderPage->hasTimeSelection())
            { s = mBuilderPage->getTimeSelStartBars(); e = mBuilderPage->getTimeSelEndBars(); return true; }
            return false;
        });
    cpRaw->setBusActiveQuery ([this] (int chId)
    {
        // Bus fallback query: kVoxBus2 active iff MixerPage activated it.
        if (! mMixerPage) return true;
        if (chId == MixerChannelIds::kVoxBus2) return mMixerPage->isVoxBus2Active();
        return true;
    });

    // QA-ModelShell TS1: engine construction/registration is model-side
    // (EngineRig, done by VoxPage's ctor engine pick).  The view callback only
    // wires the dirty hook -- installed on the BaySickVocal processor, whose
    // embedded NAM/IR sub-processor is wired transitively inside
    // wireEngineDirtyHook's BaySickVocal branch.  The explicit fire covers the
    // ctor-created engine (this callback is wired after construction).
    cpRaw->onEngineChanged = [this, cpRaw]()
    {
        wireEngineDirtyHook (cpRaw->getVocalProcessor());
    };
    if (cpRaw->onEngineChanged) cpRaw->onEngineChanged();

    // G-6 (2026-04-29): right-click engine-picker context menu callbacks.
    cpRaw->onDuplicateRequested = [this, cpRaw] { spawnDuplicateVoxTab (cpRaw); };
    cpRaw->onRenameRequested = [this, ribbonId = -1, cpRaw]() mutable
    {
        if (ribbonId < 0)
            for (auto* entry : mPages)
                if (entry && entry->component.get() == cpRaw)
                    { ribbonId = entry->ribbonTabId; break; }
        if (ribbonId >= 0 && mRibbon) mRibbon->startRename (ribbonId);
    };
    cpRaw->onDeleteRequested = [this, ribbonId = -1, cpRaw]() mutable
    {
        if (ribbonId < 0)
            for (auto* entry : mPages)
                if (entry && entry->component.get() == cpRaw)
                    { ribbonId = entry->ribbonTabId; break; }
        if (ribbonId >= 0 && mRibbon) mRibbon->closeTab (ribbonId);
    };
    cpRaw->onLockChanged = [this, ribbonId = -1, cpRaw]() mutable
    {
        if (ribbonId < 0)
            for (auto* entry : mPages)
                if (entry && entry->component.get() == cpRaw)
                    { ribbonId = entry->ribbonTabId; break; }
        if (ribbonId >= 0 && mRibbon) mRibbon->setTabLocked (ribbonId, cpRaw->isLocked());
    };

    // G-4 (2026-04-28): NO piano-roll registration for Vox.  Vox is a
    // live-input / recorded-audio destination, not a MIDI-triggered engine.


    auto entry           = std::make_unique<PageEntry>();
    entry->ribbonTabId   = newId;
    entry->type          = RibbonTabBar::TabType::Vox;
    entry->component     = std::move (cpHolder);
    auto* addedEntry = entry.release();
    mPages.add (addedEntry);
    hostPageInWindow (*addedEntry);

    if (selectAfter)
    {
        mRibbon->selectTab (newId);
        onTabSelected (newId);
    }
}

void StandaloneEditor::spawnInstTabIfMissing (int instIdx, bool selectAfter)
{
    if (instIdx < 0 || instIdx >= kMaxInstPages) return;

    for (auto* entry : mPages)
    {
        if (! entry || entry->type != RibbonTabBar::TabType::Inst) continue;
        if (auto* ip = dynamic_cast<InstPage*> (entry->component.get()))
            if (ip->getPageIndex() == instIdx) return;
    }

    const juce::String tabName = nextInstTabName();   // QA-D STATE-02
    const int newId = mRibbon->addTab (RibbonTabBar::TabType::Inst, tabName);

    auto cpHolder = std::make_unique<InstPage> (mProcessor, instIdx);
    auto* cpRaw = cpHolder.get();
    cpRaw->setTabName (tabName);
    cpRaw->setProcessor (&mProcessor);   // G-7: Page Preset save/load access
    cpRaw->setBusActiveQuery ([this] (int chId)
    {
        // Bus fallback query: kInstBus2 / kInstBus3 active iff MixerPage activated.
        if (! mMixerPage) return true;
        if (chId == MixerChannelIds::kInstBus2) return mMixerPage->isInstBus2Active();
        if (chId == MixerChannelIds::kInstBus3) return mMixerPage->isInstBus3Active();
        return true;
    });

    // QA-ModelShell TS1: engine construction/registration is model-side
    // (EngineRig, done by InstPage's ctor trio bind) -- the old
    // onEngineDestroying/onEngineChanged register wiring is gone.

    // 2026-05-05 dirty-flag wiring: install the markDirty hook on the
    // BaySickPedals + BaySickNAM/IR processors owned by this InstPage.  Their
    // APVTS edits don't reach the main PluginProcessor's listener, so without
    // this wiring rack edits / NAM file load / IR load wouldn't flip the
    // project dirty bit.  BaySickGuitars (when source != LiveInput) is wired
    // separately in addBaySickGuitarsTab + the K-6 deserialize path.
    wireEngineDirtyHook (cpRaw->getPedalsProcessor());
    wireEngineDirtyHook (cpRaw->getNamIrProcessor());

    // G-6 (2026-04-29): right-click engine-picker context menu callbacks.
    cpRaw->onDuplicateRequested = [this, cpRaw] { spawnDuplicateInstTab (cpRaw); };
    cpRaw->onRenameRequested = [this, ribbonId = -1, cpRaw]() mutable
    {
        if (ribbonId < 0)
            for (auto* entry : mPages)
                if (entry && entry->component.get() == cpRaw)
                    { ribbonId = entry->ribbonTabId; break; }
        if (ribbonId >= 0 && mRibbon) mRibbon->startRename (ribbonId);
    };
    cpRaw->onDeleteRequested = [this, ribbonId = -1, cpRaw]() mutable
    {
        if (ribbonId < 0)
            for (auto* entry : mPages)
                if (entry && entry->component.get() == cpRaw)
                    { ribbonId = entry->ribbonTabId; break; }
        if (ribbonId >= 0 && mRibbon) mRibbon->closeTab (ribbonId);
    };
    cpRaw->onLockChanged = [this, ribbonId = -1, cpRaw]() mutable
    {
        if (ribbonId < 0)
            for (auto* entry : mPages)
                if (entry && entry->component.get() == cpRaw)
                    { ribbonId = entry->ribbonTabId; break; }
        if (ribbonId >= 0 && mRibbon) mRibbon->setTabLocked (ribbonId, cpRaw->isLocked());
    };
    // Program-name linkage (2026-08-02): a picked sfizz program renames the
    // tab + strip + roll label, matching the preset-name flow on the engine
    // pages.  InstPage fires this from the interactive picker only, so
    // project restore keeps the saved tab name.
    cpRaw->onSoundNameChanged = [this, ribbonId = -1, cpRaw] (const juce::String& nm) mutable
    {
        if (nm.isEmpty()) return;
        if (ribbonId < 0)
            for (auto* entry : mPages)
                if (entry && entry->component.get() == cpRaw)
                    { ribbonId = entry->ribbonTabId; break; }
        if (ribbonId >= 0 && mRibbon) mRibbon->renameTab (ribbonId, nm);
        cpRaw->setTabName (nm);
        if (mMixerPage) mMixerPage->setInstStripName (cpRaw->getPageIndex(), nm);
        if (mPianoRollPage && cpRaw->getSource() != InstPage::Source::LiveInput)
        {
            const auto k = cpRaw->getSource() == InstPage::Source::BaySickGuitars
                             ? EngineKind::BaySickGuitars
                             : EngineKind::BaySickBasses;
            mPianoRollPage->setEngineDisplayName ({ k, cpRaw->getPageIndex() }, nm);
        }
    };

    // G-4 (2026-04-28): NO piano-roll registration for Inst.  Same reasoning
    // as Vox - live-input / recorded-audio destination, not MIDI-triggered.


    auto entry           = std::make_unique<PageEntry>();
    entry->ribbonTabId   = newId;
    entry->type          = RibbonTabBar::TabType::Inst;
    entry->component     = std::move (cpHolder);
    auto* addedEntry = entry.release();
    mPages.add (addedEntry);
    hostPageInWindow (*addedEntry);

    if (selectAfter)
    {
        mRibbon->selectTab (newId);
        onTabSelected (newId);
    }
}

// G-4 (2026-04-28): registerVoxPianoRoll / registerInstPianoRoll DELETED.
// Vox + Inst are live-input / recorded-audio destinations, not MIDI-triggered
// engines - they don't appear in the unified Piano Roll page's engine
// dropdown.  Engine register/unregister still happens via mProcessor's
// registerVoxEngine / registerInstEngine for audio-thread routing.

// ─────────────────────────────────────────────────────────────────────────────
// G-6 (2026-04-29): right-click "Duplicate" on a VoxPage / InstPage engine
// picker - capture source state, create a new mixer strip via the Mixer's
// addVoxChannelAtIndex / addInstChannelAtIndex (cascade fires the page
// spawn callback), then apply the cloned state.  Both surfaces navigate to
// the new tab (user just asked for it).
// ─────────────────────────────────────────────────────────────────────────────
void StandaloneEditor::spawnDuplicateVoxTab (VoxPage* sourceVp)
{
    if (! sourceVp || ! mMixerPage) return;

    const juce::String savedState = sourceVp->exportVoxState();

    // Find next free voxIdx not occupied by an existing VoxPage.
    std::array<bool, kMaxVoxPages> idxTaken {};
    for (auto* entry : mPages)
    {
        if (! entry || entry->type != RibbonTabBar::TabType::Vox) continue;
        if (auto* vp = dynamic_cast<VoxPage*> (entry->component.get()))
        {
            const int i = vp->getPageIndex();
            if (i >= 0 && i < (int) kMaxVoxPages) idxTaken[(size_t) i] = true;
        }
    }
    int newIdx = -1;
    for (int i = 0; i < (int) kMaxVoxPages; ++i)
        if (! idxTaken[(size_t) i]) { newIdx = i; break; }
    if (newIdx < 0) return;

    // Create the mixer strip - cascade fires onVoxStripAdded → spawnVoxTabIfMissing(newIdx, false).
    mMixerPage->addVoxChannelAtIndex (newIdx);

    // Apply state to the freshly-spawned page + select the new tab.
    int newRibbonId = -1;
    for (auto* entry : mPages)
    {
        if (! entry || entry->type != RibbonTabBar::TabType::Vox) continue;
        if (auto* vp = dynamic_cast<VoxPage*> (entry->component.get()))
        {
            if (vp->getPageIndex() == newIdx)
            {
                vp->importVoxState (savedState);
                newRibbonId = entry->ribbonTabId;
                break;
            }
        }
    }
    if (newRibbonId >= 0 && mRibbon)
    {
        mRibbon->selectTab (newRibbonId);
        onTabSelected (newRibbonId);
    }
}

void StandaloneEditor::spawnDuplicateInstTab (InstPage* sourceIp)
{
    if (! sourceIp || ! mMixerPage) return;

    const juce::String savedState = sourceIp->exportInstState();

    std::array<bool, kMaxInstPages> idxTaken {};
    for (auto* entry : mPages)
    {
        if (! entry || entry->type != RibbonTabBar::TabType::Inst) continue;
        if (auto* ip = dynamic_cast<InstPage*> (entry->component.get()))
        {
            const int i = ip->getPageIndex();
            if (i >= 0 && i < (int) kMaxInstPages) idxTaken[(size_t) i] = true;
        }
    }
    int newIdx = -1;
    for (int i = 0; i < (int) kMaxInstPages; ++i)
        if (! idxTaken[(size_t) i]) { newIdx = i; break; }
    if (newIdx < 0) return;

    mMixerPage->addInstChannelAtIndex (newIdx);

    int newRibbonId = -1;
    for (auto* entry : mPages)
    {
        if (! entry || entry->type != RibbonTabBar::TabType::Inst) continue;
        if (auto* ip = dynamic_cast<InstPage*> (entry->component.get()))
        {
            if (ip->getPageIndex() == newIdx)
            {
                ip->importInstState (savedState);
                newRibbonId = entry->ribbonTabId;
                break;
            }
        }
    }
    if (newRibbonId >= 0 && mRibbon)
    {
        mRibbon->selectTab (newRibbonId);
        onTabSelected (newRibbonId);
    }
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

    // QA-ProjectSave docket 18 (2026-07-26): zero tabs of the type is a normal
    // state now, so F8 / F9 / F10 and the View menu land on the empty state
    // instead of doing nothing -- same destination as clicking the ribbon slot.
    // QA-ModelShell TS4: with zero instances the type has no tab at all, so
    // there is nothing to navigate TO -- the shortcut is a no-op rather than a
    // trip to a placeholder page.  "+" is how the user gets one.
    switch (targetType)
    {
        default: break;
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
    case 0: // File - Project persistence (P2+P3+P6, 2026-04-23)
        m.addItem(101, "New Project...  (Ctrl+N)");
        {
            // QA-ProjectSave Task 10: one submenu replaces old items 102 (clone
            // a project) + 109 (Load Template) -- every pick funnels through
            // loadTemplate's unified dirty gate.  File picks resolve through
            // mTemplateMenuFiles, rebuilt on every menu open.
            mTemplateMenuFiles.clearQuick();

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
                    sub.addItem (kTemplateMenuLoadBase + mTemplateMenuFiles.size(),
                                 f.getFileNameWithoutExtension());
                    mTemplateMenuFiles.add (f);
                }
            };

            juce::PopupMenu tpl;
            const auto def = mProjectManager ? mProjectManager->getDefaultTemplate()
                                             : juce::File();
            if (def == juce::File())
                tpl.addItem (111, "New from Default Template", false);
            else if (! def.existsAsFile())
                tpl.addItem (111, "New from Default Template ("
                                   + def.getFileNameWithoutExtension()
                                   + " - missing)", false);
            else
                tpl.addItem (111, "New from Default Template ("
                                   + def.getFileNameWithoutExtension() + ")");
            tpl.addSeparator();
            {
                juce::PopupMenu premade;
                walk (premade, factoryTemplatesDir());
                if (premade.getNumItems() == 0)
                    premade.addItem (-1, "(no premade templates)", false, false);
                tpl.addSubMenu ("Premade Templates", premade);
            }
            {
                juce::PopupMenu mine;
                walk (mine, userTemplatesDir());
                if (mine.getNumItems() == 0)
                    mine.addItem (-1, "(no user templates)", false, false);
                tpl.addSubMenu ("My Templates", mine);
            }
            m.addSubMenu ("New from Template", tpl);
        }
        m.addSeparator();
        m.addItem(103, "Open Project...  (Ctrl+O)");
        m.addItem(110, "Quick Open Project...");
        // P3: Open Recent submenu - last 10 projects, missing ones greyed out.
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
        // 2026-04-26: Save as Preset moved out - the per-engine preset
        // pickers handle that.  Save as Template lands in this slot since
        // it's the project-level save-as cousin (full project skeleton, v2).
        m.addItem(106, "Save as Template...");
        m.addSeparator();
        m.addItem(108, "Restore from Backup...");
        m.addSeparator();
        m.addItem(107, "Import Audio...");
        m.addSeparator();
        // QA-Export: the old WAV/MP3 submenu (ids 120/121) had NO dispatch cases
        // -- both were silent no-ops.  One item now, format chosen in the dialog;
        // id 120 reused, 121 retired.
        m.addItem(120, "Export Audio...");
        m.addItem(122, "Export Project Bundle...");
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

    case 3: // View - Phase B-1 keymap (2026-04-26): F-keys reassigned per spreadsheet.
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
        m.addItem(504, "Plugins...");
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
        m.addItem(604, "Rusty Drums Map...");
        m.addSeparator();
        m.addItem(602, "About BaySickDAW v1.0");
        break;
    }

    return m;
}

void StandaloneEditor::menuItemSelected(int id, int)
{
    // File > New from Template file picks (submenu built in getMenuForIndex).
    if (id >= kTemplateMenuLoadBase
        && id <  kTemplateMenuLoadBase + mTemplateMenuFiles.size())
    {
        loadTemplate (mTemplateMenuFiles[id - kTemplateMenuLoadBase]);
        return;
    }

    switch (id)
    {
    // File - Project persistence (P2+P3, 2026-04-23)
    case 101: doFileNew();     break;
    case 103: doFileOpen();    break;
    case 110: doFileQuickOpen(); break;
    case 120: doExportAudio(); break;
    case 122: doExportProjectBundle(); break;
    case 104: doFileSave();    break;
    case 105: doFileSaveAs();  break;
    case 106: saveTemplateAs(); break;                      // 2026-04-26
    case 107: if (mBuilderPage) mBuilderPage->doImportAudio(); break;
    case 108: doFileRestoreBackup(); break;
    case 111:  // New from Default Template - the default pointer's one consumer
        if (mProjectManager && mProjectManager->getDefaultTemplate().existsAsFile())
            loadTemplate (mProjectManager->getDefaultTemplate());
        break;
    case 530: doFileSetDefaultTemplate(); break;
    case 531:
        if (mProjectManager) { mProjectManager->clearDefaultTemplate(); }
        break;
    case 140: // Clear Recent
        if (mProjectManager) mProjectManager->clearRecentProjects();
        break;
    // Recent Projects items (130..139) - open indexed project.  Explicit
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
                    HeavyOperationOverlay::ScopedOp heavyOp (mHeavyOpOverlay,
                                                             "Loading Project...");
                    mHeavyOpOverlay.setStepLabel ("Closing old tabs...");
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

    // Undo history size - also cap the label list
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

    // QA-Fe2: File Settings (take-type checkboxes + De-noise strength)
    case 502:
        showFileSettingsDialog();
        break;

    // Audio & MIDI Settings dialog - uses AudioSettingsDialog (safe Apply flow)
    case 503:
    {
        auto* dlg = new AudioSettingsDialog(mDeviceManager, mAudioCallback);

        juce::DialogWindow::LaunchOptions opts;
        opts.dialogTitle            = "Audio & MIDI Settings";
        opts.dialogBackgroundColour = VC::Bg;
        opts.content.setOwned(dlg);
        opts.resizable              = false;
        opts.useNativeTitleBar      = false;   // TS7 §9.3
        opts.launchAsync();
        break;
    }

    // QA-ModelShell TS6 (BLU-299): hosted-plugin manager.
    case 504:
        showPluginsWindow();
        break;

    case 602:
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon, "BaySickDAW v1.0",
            "BaySickDAW v1.0\n"
            "Built with JUCE 7  |  (c) KnowledgeBase Studios\n\n"
            // QA-Export (2026-07-25): LAME added here because the LGPL wants the
            // use disclosed somewhere the user can see.  This list is INCOMPLETE
            // -- several other vendored libs are unlisted; QA-LegalReview owns the
            // full audit (see its Main Plan entry).
            "Powered by:\n"
            "  - sfizz (BSD 2-Clause) - SFZ player engine\n"
            "  - LAME (LGPL) - MP3 encoding",
            "OK");
        break;

    case 603:   // Help > Key Binds...
        showKeyBindsWindow();
        break;

    case 604:   // Help > Rusty Drums Map...
        showRustyDrumsMapWindow();
        break;

    // J-6 (2026-05-03): cases 605 + 606 (BaySickRustyDrums Help-menu test
    // entries) removed - replaced by the "+ Add BaySickRustyDrums" entry on
    // the Drums dropdown which spawns a real BaySickRustyDrumsPage.

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

    mMenuBar->setBounds(b.removeFromTop(kMenuH));

    // ── Combined toolbar (single 40px row) ────────────────────────────────────
    // mTransport spans full width:   left  → transport controls (▶⏸■ BPM TAP SONG METRO)
    //                                right → CPU/RAM readout
    //                                middle → empty (pattern + ribbon overlap on top)
    auto bar = b.removeFromTop(kBarH);
    mTransport->setBounds(bar);

    // Pattern dropdown button - single control replacing old ComboBox + Add button
    static constexpr int kCPUReserve = 120; // space kept clear on right for CPU label
    static constexpr int kPatBtnW    = 176; // 140 + 32 + 4 gap, same total footprint
    static constexpr int kPatStart   = GlobalTransportBar::kControlsWidth + 8;
    int py = bar.getY() + (kBarH - 28) / 2;
    mPatternBtn->setBounds(bar.getX() + kPatStart, py, kPatBtnW, 28);

    // QA-TransportDisplay: position readout between the pattern button and
    // the ribbon; the ribbon absorbs the width loss (standing no-expand rule).
    static constexpr int kPosReadoutW = 100;
    if (mPosReadout)
        mPosReadout->setBounds(bar.getX() + kPatStart + kPatBtnW + 8, py, kPosReadoutW, 28);

    // Ribbon tabs: from end of the readout to just before CPU readout
    int ribX = kPatStart + kPatBtnW + 8 + kPosReadoutW + 8;
    int ribW = bar.getWidth() - ribX - kCPUReserve;
    if (ribW > 60)
        mRibbon->setBounds(bar.getX() + ribX, bar.getY(), ribW, kBarH);

    // ── Page menu bar + content ───────────────────────────────────────────────
    // QA-ModelShell TS4: the page menu moved into each window's title strip,
    // so the chrome no longer reserves a row for it -- the workspace gets it.
    // QA-ModelShell TS4: the content rect is now the WORKSPACE.  Pages are laid
    // out by the contained window that frames them, not by this function -- the
    // old loop gave every page the full rect simultaneously, which is exactly
    // the always-alive stacking the shell replaces.
    if (mWorkspace) mWorkspace->setBounds(b);
}

// ─────────────────────────────────────────────────────────────────────────────
// QA-TransportDisplay: readout persistence + D-4 typing-keyboard MIDI

void StandaloneEditor::pushTempoMarkersToPlayHead()
{
    if (! mPM) return;
    // Markers are SONG-domain (a looping pattern has no song position, so
    // pattern mode plays the base tempo) - push the empty set there and the
    // full set in song mode; onSongModeChanged re-pushes on every switch.
    std::vector<std::pair<double,double>> markers;
    if (mTransport && mTransport->isSongMode())
        for (int i = 0; i < mPM->getNumTempoChanges(); ++i)
        {
            const auto& tc = mPM->getTempoChange (i);
            markers.push_back ({ (double) tc.bar * 4.0, tc.bpm });
        }
    mPlayHead.setTempoMarkers (std::move (markers));
}

void StandaloneEditor::loadTransportDisplayPref()
{
    if (! mPosReadout) return;
    const auto f = ProjectManager::getSettingsFile();
    if (! f.existsAsFile()) return;   // first launch - keep the beats default
    if (auto root = juce::XmlDocument::parse (f))
        if (auto* node = root->getChildByName ("TransportDisplay"))
            mPosReadout->setShowTime (node->getBoolAttribute ("showTime", false));
}

void StandaloneEditor::saveTransportDisplayPref (bool showTime)
{
    const auto f = ProjectManager::getSettingsFile();
    f.getParentDirectory().createDirectory();

    // Re-parse the existing root so every sibling section survives (same
    // pattern as saveMultiCoreRenderingPref - settings.xml is shared).
    std::unique_ptr<juce::XmlElement> root;
    if (f.existsAsFile())
        root = juce::XmlDocument::parse (f);
    if (root == nullptr)
        root = std::make_unique<juce::XmlElement> ("BaySickDAWSettings");

    if (auto* existing = root->getChildByName ("TransportDisplay"))
        root->removeChildElement (existing, true);
    root->createNewChildElement ("TransportDisplay")->setAttribute ("showTime", showTime);
    root->writeTo (f);
}

void StandaloneEditor::toggleTypingKeyboard()
{
    mTypingKeyboardOn = ! mTypingKeyboardOn;
    if (! mTypingKeyboardOn) releaseAllTypingNotes();
    TypingKeyboardMap::gActive.store (mTypingKeyboardOn, std::memory_order_relaxed);
    if (mTransport) mTransport->setTypingKeyboardOn (mTypingKeyboardOn);
}

void StandaloneEditor::sendTypingNote (int midiNote, bool noteOn)
{
    if (midiNote < 0 || midiNote > 127) return;
    auto msg = noteOn ? juce::MidiMessage::noteOn  (1, midiNote, 0.8f)
                      : juce::MidiMessage::noteOff (1, midiNote, 0.0f);
    // MidiMessageCollector asserts on zero timestamps.  Hardware input is
    // pre-stamped by juce::MidiInput; synthetic messages must self-stamp.
    msg.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
    mProcessor.getLiveMidiCollector().addMessageToQueue (msg);
}

void StandaloneEditor::releaseAllTypingNotes()
{
    for (auto& held : mTypingHeldNotes)
        sendTypingNote (held.second, false);
    mTypingHeldNotes.clear();

    // The plugin-roll audition's held note lives OUTSIDE the typing set, so
    // this walk never released it -- and since sendTypingNote routes to the
    // VISIBLE tab, releasing it after a tab switch would have sent the
    // note-off to the wrong engine and hung the plugin's voice.  Released
    // here, while the old tab is still the target, the audition comment's
    // "tab-switch backstop" claim is actually true.
    if (mPluginAuditionHeldNote >= 0)
    {
        sendTypingNote (mPluginAuditionHeldNote, false);
        mPluginAuditionHeldNote = -1;
    }
}

bool StandaloneEditor::keyPressed (const juce::KeyPress& key)
{
    if (! mTypingKeyboardOn) return false;

    // Only bare keys are notes - any modifier means a normal shortcut
    // (Ctrl+Z undo, Shift+Space stop, ...) and must pass through untouched.
    const auto mods = key.getModifiers();
    if (mods.isCtrlDown() || mods.isAltDown() || mods.isShiftDown()) return false;

    const int kc = key.getKeyCode();
    if (TypingKeyboardMap::isOctaveShiftKey (kc))
    {
        // Shifting while notes are held would strand the old pitches at
        // key-up (the noteOff would target the shifted pitch) - release first.
        releaseAllTypingNotes();
        mTypingOctaveOffset = juce::jlimit (-5, 3,
            mTypingOctaveOffset + (kc == juce::KeyPress::pageUpKey ? 1 : -1));
        return true;
    }

    const int semi = TypingKeyboardMap::semitoneForKey (kc);
    if (semi < 0) return false;

    for (auto& held : mTypingHeldNotes)
        if (held.first == kc) return true;    // OS key auto-repeat - already sounding

    const int note = juce::jlimit (0, 127, 60 + semi + mTypingOctaveOffset * 12);
    mTypingHeldNotes.push_back ({ kc, note });
    sendTypingNote (note, true);
    return true;
}

bool StandaloneEditor::keyStateChanged (bool /*isKeyDown*/)
{
    if (mTypingHeldNotes.empty()) return false;

    // JUCE reports "something changed", not which key - diff our held set
    // against the OS key state to find releases.
    for (int i = (int) mTypingHeldNotes.size(); --i >= 0;)
    {
        const auto held = mTypingHeldNotes[(size_t) i];
        if (! juce::KeyPress::isKeyCurrentlyDown (held.first))
        {
            sendTypingNote (held.second, false);
            mTypingHeldNotes.erase (mTypingHeldNotes.begin() + (int) i);
        }
    }
    return false;   // never consume - command-system key-up handling stays intact
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
    // J-8 stage 2 (2026-05-04): if the BaySickRustyDrums page is visible AND
    // its engine UndoManager has a pending action, prefer that - player CC
    // edits stack into the engine's APVTS undo manager (separate from the
    // editor's main mUndoManager).  This makes Ctrl+Z reverse the most recent
    // ARIA panel knob edit when you're on the player tab.
    if (auto* rp = dynamic_cast<BaySickRustyDrumsPage*> (mVisiblePage))
    {
        juce::ignoreUnused (rp);
        if (auto* eng = mProcessor.getBaySickRustyDrums())
        {
            auto& engUm = eng->getUndoManager();
            if (engUm.canUndo()) { engUm.undo(); return; }
        }
    }

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
    if (auto* rp = dynamic_cast<BaySickRustyDrumsPage*> (mVisiblePage))
    {
        juce::ignoreUnused (rp);
        if (auto* eng = mProcessor.getBaySickRustyDrums())
        {
            auto& engUm = eng->getUndoManager();
            if (engUm.canRedo()) { engUm.redo(); return; }
        }
    }

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

        WindowChrome::ownToMainWindow (*mHistoryWindow, *this);   // TS7 §9.4

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
//  Project persistence - File menu handlers (P2, 2026-04-23)
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
    if (! mProjectManager || ! mProjectManager->isDirty())
    {
        if (continuation) continuation();
        return;
    }

    // Prompt whenever there are unsaved edits - even a never-saved session (no
    // project folder yet), so work isn't lost silently on quit / New / Open.
    const juce::String message = mProjectManager->hasProject()
        ? "Save changes to '" + mProjectManager->getCurrentName() + "' first?"
        : juce::String ("You have unsaved changes.  Save them first?");

    // Native TaskDialog: centered, and it drops the old juce::AlertWindow's
    // click-anywhere-body drag (AlertWindow moves itself via its own ComponentDragger).
    // The native dialog is still title-bar movable -- Windows exposes no flag to pin it.
    // showAsync uses plainIndex result mapping, so button order gives
    // 0 = Save, 1 = Don't Save, 2 = Cancel.
    juce::NativeMessageBox::showAsync (
        juce::MessageBoxOptions{}
            .withIconType (juce::MessageBoxIconType::WarningIcon)
            .withTitle ("Unsaved changes")
            .withMessage (message)
            .withButton ("Save")
            .withButton ("Don't Save")
            .withButton ("Cancel")
            .withAssociatedComponent (this),
        [this, continuation] (int result)
        {
            if (result == 2) return;                                        // Cancel
            if (result == 1) { if (continuation) continuation(); return; }  // Don't Save

            // Save.  A named project saves in place; an unnamed session routes
            // through Save As (name it first), then continues on success.  A
            // failed or cancelled save leaves the app open with work intact.
            if (mProjectManager->hasProject())
            {
                if (mProjectManager->saveProject())
                {
                    if (continuation) continuation();
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::MessageBoxIconType::WarningIcon,
                        "Save failed",
                        "Couldn't write project.xml.  Aborting.");
                }
                return;
            }

            promptForProjectName (
                "Save Project",
                "Give your project a name so your work has somewhere to live:",
                "Untitled Project",
                [this, continuation] (juce::String name)
                {
                    if (! ProjectManager::isValidProjectName (name))
                    {
                        juce::AlertWindow::showMessageBoxAsync (
                            juce::MessageBoxIconType::WarningIcon,
                            "Invalid project name",
                            "Try another name (no < > : \" / \\ | ? * or reserved names).");
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
                    if (continuation) continuation();
                });
        });
}

bool StandaloneEditor::requestAppQuit()
{
    if (! mProjectManager || ! mProjectManager->isDirty())
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
            if (! mProjectManager->newProject (name))
            {
                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon,
                    "Could not create project",
                    "Check that the Projects folder is writable and try again.");
                return;
            }
            // QA-Ef (2026-05-22): shield the New Project rebuild -- File > New is
            // a load-type op that tears down + rebuilds the project graph (the
            // default-template loadTemplate) and can run while a prior project
            // is still playing.  Nest-aware so the inner
            // loadTemplate / closeAllDynamicTabs inherit it; restored at the end.
            const bool shieldWasUp = mProcessor.isProjectLoadInProgress();
            mProcessor.setProjectLoadInProgress (true);
            if (! shieldWasUp)
                juce::Thread::sleep (30);

            // QA-Ef #4 (2026-05-22): tear down prior-project aux inserts under
            // the shield, before the rebuild.  Pairs with the same call in
            // deserializeProject and loadTemplate -- aux strips have no per-
            // tab teardown hook so we clear them explicitly at each load-type
            // entry point.
            mProcessor.clearAllAuxInserts();

            // 2026-04-24 File > New reset: wipe in-memory state from prior
            // project before applying any template content.
            closeAllDynamicTabs();
            if (mMixerPage) mMixerPage->clearDynamicStrips();
            mProcessor.resetToBlankState();

            // QA-Ef #6 (2026-05-22): File > New ALWAYS loads blank, matching app
            // startup.  Default-template application lives on the dedicated
            // "New from Default Template" menu item, so a user's set default no
            // longer auto-applies on plain File > New.
            // QA-ProjectSave docket 18 (2026-07-26): "blank" is now literally
            // blank -- no seeded Layers / Bass / Drums trio, same as the editor
            // ctor.  A new project carries only what the user adds to it.
            if (mRibbon) { mRibbon->selectTab (3); }
            onTabSelected (3);   // land on Builder

            restoreAudioStripsFromArrangement();
            mProjectManager->saveProject();
            refreshWindowTitle();

            // QA-Ef (2026-05-22): rebuild complete -- restore prior shield state.
            mProcessor.setProjectLoadInProgress (shieldWasUp);
        });
    });   // close confirmDiscardChanges continuation
}

void StandaloneEditor::doFileSetDefaultTemplate()
{
    // 2026-04-26: repointed from project-folder picker to the Templates
    // folder.  Templates are XML files: v2 full-skeleton saves under
    // My Templates/, plus the v1-factory set (gen_factory_presets.py)
    // under Factory/.
    auto initialDir = templatesDir();
    initialDir.createDirectory();   // ensure exists so the chooser opens cleanly
    // Both subfolders must exist or a fresh install shows an empty/partial
    // root listing -- the chooser opens at the root, so Factory and My
    // Templates have to be equally reachable from the first click.
    factoryTemplatesDir().createDirectory();
    userTemplatesDir   ().createDirectory();

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
            "File > New from Template > New from Default Template will start from '"
            + picked.getFileNameWithoutExtension()
            + "'.  Use Options > General > Clear Default Template to undo.");
    });
}

void StandaloneEditor::doFileOpen()
{
    // P5: prompt Save/Don't Save/Cancel first if current project is dirty.
    confirmDiscardChanges ([this]
    {
        // A project IS a folder (project.xml lives inside), so this is a
        // directory picker, not a file picker -- picking project.xml itself
        // would force the user to descend into every candidate folder.
        auto root = ProjectManager::getDefaultProjectsRoot();
        root.createDirectory();

        auto chooser = std::make_shared<juce::FileChooser> (
            "Open Project", root, juce::String(), true);
        const int flags = juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectDirectories;
        chooser->launchAsync (flags, [this, chooser] (const juce::FileChooser& fc)
        {
            const juce::File folder = fc.getResult();
            if (folder == juce::File() || ! folder.isDirectory())
                return;   // cancelled

            // Validate BEFORE the teardown below -- the native picker can land
            // on any folder, and resetToBlankState() is not undoable.
            if (! folder.getChildFile ("project.xml").existsAsFile())
            {
                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon,
                    "Not a BaySickDAW project folder",
                    "That folder has no project.xml inside it.");
                return;
            }

            HeavyOperationOverlay::ScopedOp heavyOp (mHeavyOpOverlay,
                                                     "Loading Project...");
            mHeavyOpOverlay.setStepLabel ("Closing old tabs...");
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
        });
    });   // close confirmDiscardChanges continuation
}

// ─────────────────────────────────────────────────────────────────────────────
// QA-ModelShell TS2 -- the FL-style export dialog (locked UX call): the
// options box PERSISTS, the save dialog opens above it, and after the pick
// the same box flips to a progress bar + percent readout with a live Cancel.
// The render runs on a plain background thread driving
// BuilderPage::renderToFile (which owns the offline drive + restore set).
// Hosts Jeff's per-strip stems pick-list: every active mixer strip, Master +
// buses default UNCHECKED.  Settings first, destination second: the file
// extension follows the chosen format.
// ─────────────────────────────────────────────────────────────────────────────
class ExportAudioDialog : public juce::Component,
                          private juce::Thread,
                          private juce::Timer
{
public:
    ExportAudioDialog (BuilderPage& builder,
                       VibeSynthProcessor& proc,
                       juce::String defaultBaseName,
                       std::vector<MixerPage::StemPickEntry> stemEntries,
                       std::function<void (std::function<void()>)> ensureProjectSaved)
        : juce::Thread ("Export Render"),
          mBuilder (builder), mProc (proc),
          mBaseName (std::move (defaultBaseName)),
          mEnsureSaved (std::move (ensureProjectSaved))
    {
        auto addCombo = [this] (juce::ComboBox& c, juce::Label& l, const char* title,
                                const juce::StringArray& items)
        {
            l.setText (title, juce::dontSendNotification);
            addAndMakeVisible (l);
            c.addItemList (items, 1);
            c.setSelectedItemIndex (0, juce::dontSendNotification);
            addAndMakeVisible (c);
        };
        addCombo (mSel,    mSelLbl,    "Selection",   { "Full Arrangement", "Selected Section" });
        addCombo (mTail,   mTailLbl,   "Tail",        { "Included", "Cut" });
        addCombo (mFormat, mFormatLbl, "Format",      { "WAV", "OGG", "MP3" });
        addCombo (mQual,   mQualLbl,   "Quality",     { "-" });
        addCombo (mSrate,  mSrateLbl,  "Sample rate", { "44100 Hz", "48000 Hz" });

        // "Selected Section" is meaningless with nothing selected on the
        // ruler, so it is disabled rather than silently falling back.
        if (! mBuilder.hasTimeSelection())
            mSel.setItemEnabled (2, false);   // 1-based item id

        mFormat.onChange = [this] { repopulateQuality(); };
        repopulateQuality();

        // CL-043 + CL-045 riders.  CL-043 is SELECTABLE (Jeff's ruling (a),
        // 2026-07-31 -- it shipped as a single-algorithm checkbox, which was a
        // narrowing nobody approved).  The entry's third algorithm, POW-r, is
        // licensed and trademarked, so the third slot is our own noise-shaped
        // design rather than a name we cannot use.
        mDitherLbl.setText ("Dither", juce::dontSendNotification);
        mDitherLbl.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (mDitherLbl);
        mDither.addItem ("Off",          1);
        mDither.addItem ("Flat (TPDF)",  2);
        mDither.addItem ("Noise-Shaped", 3);
        mDither.setSelectedId (1, juce::dontSendNotification);
        mDither.setTooltip ("Applies to 16-bit WAV exports. Flat is the safe default; "
                            "Noise-Shaped moves the same noise out of the ear's most "
                            "sensitive range.");
        addAndMakeVisible (mDither);
        mNormToggle.setButtonText ("Normalize to");
        addAndMakeVisible (mNormToggle);
        // TS7 (Jeff 2026-07-29): a typed LUFS box replaces the four-item combo.
        // Four fixed picks could not express the number a user actually wants,
        // and "a couple random options no one would use" was the verdict.
        setupLufsBox (mLufsTarget, -14.0f);   // -14: streaming standard, the default
        addAndMakeVisible (mLufsTarget);
        mLufsSuffix.setText ("LUFS", juce::dontSendNotification);
        mLufsSuffix.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (mLufsSuffix);

        mStemsToggle.setButtonText ("Export stems (one file per mixer strip)");
        mStemsToggle.onClick = [this]
        {
            mStripViewport.setVisible (mStemsToggle.getToggleState());
            refreshSize();
        };
        addAndMakeVisible (mStemsToggle);

        // ── TS7: measure-before-render (CL-227 backend + BLU-108 true peak) ───
        // Runs the SAME offline loop the export does, with meters instead of
        // writers, so the number it reports is the number the render will produce
        // -- not an approximation of it.  Spec list comes from LoudnessSpec so the
        // report cannot drift from the maximizer's target line.
        mSpecLbl.setText ("Check against", juce::dontSendNotification);
        addAndMakeVisible (mSpecLbl);
        {
            juce::StringArray specs;
            for (int i = 0; i < LoudnessSpec::numSpecs(); ++i)
                specs.add (LoudnessSpec::nameOf (i));
            mSpecCombo.addItemList (specs, 1);
            // Ruling 6a (2026-07-31): the spec choice is PERSISTED -- it is the
            // one loudness standard Measure, export and version capture's take
            // verdicts all share, so it must survive the dialog.
            mSpecCombo.setSelectedItemIndex (
                openUiPrefs()->getIntValue ("exSpecId", (int) LoudnessSpec::Id::Streaming14),
                                             juce::dontSendNotification);
        }
        // Custom reveals its own reference box -- otherwise "Custom" would be a
        // pick with nothing behind it.
        mSpecCombo.onChange = [this] { refreshCustomSpecBox(); persistSpecChoice(); };
        addAndMakeVisible (mSpecCombo);

        setupLufsBox (mCustomLufs, -14.0f);
        addChildComponent (mCustomLufs);
        mCustomLufsLbl.setText ("Custom ref (LUFS)", juce::dontSendNotification);
        addChildComponent (mCustomLufsLbl);
        refreshCustomSpecBox();

        // The "Also write CSV report" checkbox that lived here is GONE (Jeff,
        // 2026-07-31, ruling a): its only consumer was the Measure report,
        // which ruling 5B removed -- a control gating nothing is dead UI.
        mMeasureBtn.setButtonText ("Measure");
        mMeasureBtn.setTooltip ("Measures the arrangement offline (faster than real time) "
                                "and reports integrated loudness, loudness range and true "
                                "peak against the selected spec. Writes no files");
        mMeasureBtn.onClick = [this] { onMeasureClicked(); };
        addAndMakeVisible (mMeasureBtn);

        for (auto* l : { &mMeasureLine1, &mMeasureLine2 })
        {
            l->setJustificationType (juce::Justification::centredLeft);
            l->setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 11.0f,
                                    juce::Font::plain));
            addAndMakeVisible (*l);
        }

        int y = 0;
        for (auto& e : stemEntries)
        {
            auto t = std::make_unique<juce::ToggleButton> (e.name);
            t->setToggleState (e.defaultChecked, juce::dontSendNotification);
            t->setBounds (0, y, kStripListW - 20, kRowH);
            y += kRowH;
            mStripList.addAndMakeVisible (*t);
            mStrips.push_back ({ e.channelId, std::move (t) });
        }
        mStripList.setSize (kStripListW - 20, juce::jmax (kRowH, y));
        mStripViewport.setViewedComponent (&mStripList, false);
        mStripViewport.setScrollBarsShown (true, false);
        addChildComponent (mStripViewport);

        mExportBtn.setButtonText ("Export");
        mCancelBtn.setButtonText ("Cancel");
        mExportBtn.onClick = [this] { onExportClicked(); };
        mCancelBtn.onClick = [this] { onCancelClicked(); };
        addAndMakeVisible (mExportBtn);
        addAndMakeVisible (mCancelBtn);

        mBar = std::make_unique<juce::ProgressBar> (mProgress);
        addChildComponent (*mBar);
        mPercent.setJustificationType (juce::Justification::centred);
        addChildComponent (mPercent);

        refreshSize();
    }

    ~ExportAudioDialog() override
    {
        // renderToFile polls shouldAbort per block and restores everything on
        // the abort path, so this join is bounded.
        stopThread (10000);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (14);
        auto row = [&area] { return area.removeFromTop (kRowH + 6); };

        auto place = [&] (juce::Label& l, juce::ComboBox& c)
        {
            auto r = row();
            l.setBounds (r.removeFromLeft (96));
            c.setBounds (r.reduced (0, 2));
        };
        place (mSelLbl, mSel);
        place (mTailLbl, mTail);
        place (mFormatLbl, mFormat);
        place (mQualLbl, mQual);
        place (mSrateLbl, mSrate);

        place (mDitherLbl, mDither);
        {
            auto r = row();
            mNormToggle.setBounds (r.removeFromLeft (130));
            mLufsTarget.setBounds (r.removeFromLeft (72).reduced (0, 3));
            r.removeFromLeft (4);
            mLufsSuffix.setBounds (r);
        }
        if (mCustomLufs.isVisible())
        {
            auto r = row();
            mCustomLufsLbl.setBounds (r.removeFromLeft (130));
            mCustomLufs.setBounds (r.removeFromLeft (72).reduced (0, 3));
        }
        {
            auto r = row();
            mSpecLbl.setBounds (r.removeFromLeft (96));
            mMeasureBtn.setBounds (r.removeFromRight (84).reduced (0, 2));
            r.removeFromRight (8);
            mSpecCombo.setBounds (r.reduced (0, 2));
        }
        mMeasureLine1.setBounds (area.removeFromTop (16));
        mMeasureLine2.setBounds (area.removeFromTop (16));

        mStemsToggle.setBounds (row());
        if (mStripViewport.isVisible())
            mStripViewport.setBounds (area.removeFromTop (kStripListH));

        auto btnRow = area.removeFromTop (kRowH + 10).reduced (0, 4);
        mCancelBtn.setBounds (btnRow.removeFromRight (92));
        btnRow.removeFromRight (8);
        mExportBtn.setBounds (btnRow.removeFromRight (92));

        auto progRow = area.removeFromTop (kRowH + 6);
        mPercent.setBounds (progRow.removeFromRight (56));
        if (mBar) mBar->setBounds (progRow.reduced (0, 4));
    }

private:
    enum class State { Options, Rendering };
    static constexpr int kRowH       = 26;
    static constexpr int kStripListW = 392;
    static constexpr int kStripListH = 160;

    void refreshSize()
    {
        const int stems  = mStripViewport.isVisible() ? kStripListH : 0;
        // Rows: 5 combos + dither + normalize + measure + CSV + stems toggle = 10,
        // plus the Custom reference row only while it is showing.
        const int rows   = 10 + (mCustomLufs.isVisible() ? 1 : 0);
        setSize (420, 14 * 2 + (kRowH + 6) * rows + 32 + stems
                      + (kRowH + 10) + (kRowH + 6));
    }

    // TS7: a LUFS entry box.  Restricted to the characters a signed decimal can
    // contain so a stray letter cannot silently read as 0 LUFS, which would be a
    // very loud mistake on a normalize pass.
    void setupLufsBox (juce::TextEditor& e, float defaultVal)
    {
        e.setMultiLine (false);
        e.setReturnKeyStartsNewLine (false);
        e.setInputRestrictions (7, "-0123456789.");
        e.setJustification (juce::Justification::centredRight);
        e.setText (juce::String (defaultVal, 1), juce::dontSendNotification);
    }

    static float readLufsBox (const juce::TextEditor& e, float fallback)
    {
        const auto t = e.getText().trim();
        if (t.isEmpty() || t == "-" || t == ".") return fallback;
        return juce::jlimit (-40.0f, 0.0f, t.getFloatValue());
    }

    void refreshCustomSpecBox()
    {
        const bool custom = (mSpecCombo.getSelectedItemIndex()
                             == (int) LoudnessSpec::Id::Custom);
        mCustomLufs.setVisible (custom);
        mCustomLufsLbl.setVisible (custom);
        refreshSize();
    }

    void repopulateQuality()
    {
        const int f = mFormat.getSelectedItemIndex();
        mQual.clear (juce::dontSendNotification);
        if (f == 1)      mQual.addItemList ({ "Low", "Medium", "High", "Highest" }, 1);
        else if (f == 2) mQual.addItemList ({ "128 kbps", "192 kbps", "256 kbps", "320 kbps" }, 1);
        else             mQual.addItemList ({ "16-bit", "24-bit", "32-bit float" }, 1);
        mQual.setSelectedItemIndex (f == 0 ? 1 : 2, juce::dontSendNotification);
    }

    static juce::String extFor (BuilderPage::RenderOptions::Format f)
    {
        return f == BuilderPage::RenderOptions::Format::Ogg ? ".ogg"
             : f == BuilderPage::RenderOptions::Format::Mp3 ? ".mp3" : ".wav";
    }

    BuilderPage::RenderOptions gatherOptions()
    {
        BuilderPage::RenderOptions o;

        if (mSel.getSelectedItemIndex() == 1 && mBuilder.hasTimeSelection())
        {
            o.scope = BuilderPage::RenderOptions::Scope::Section;
            // Ruler selection is in BARS; the render works in beats.
            o.startBeats = (double) mBuilder.getTimeSelStartBars() * 4.0;
            o.endBeats   = (double) mBuilder.getTimeSelEndBars()   * 4.0;
        }
        else
        {
            o.scope = BuilderPage::RenderOptions::Scope::Song;
        }

        o.tail = mTail.getSelectedItemIndex() == 1
               ? BuilderPage::RenderOptions::Tail::Cut
               : BuilderPage::RenderOptions::Tail::Included;

        const int fmtIdx = mFormat.getSelectedItemIndex();
        o.format = fmtIdx == 1 ? BuilderPage::RenderOptions::Format::Ogg
                 : fmtIdx == 2 ? BuilderPage::RenderOptions::Format::Mp3
                               : BuilderPage::RenderOptions::Format::Wav;

        o.sampleRate = mSrate.getSelectedItemIndex() == 1 ? 48000.0 : 44100.0;

        // One control, read three ways depending on format.
        const int qIdx = juce::jlimit (0, 3, mQual.getSelectedItemIndex());
        static constexpr int kDepths[] = { 16, 24, 32 };
        static constexpr int kOggQ[]   = { 3, 5, 7, 9 };      // JUCE OGG takes a quality INDEX
        static constexpr int kMp3Br[]  = { 128, 192, 256, 320 };
        if (fmtIdx == 1)      o.oggQuality = kOggQ[qIdx];
        else if (fmtIdx == 2) o.mp3Kbps    = kMp3Br[qIdx];
        else                  o.bitDepth   = kDepths[juce::jlimit (0, 2, qIdx)];

        if (mStemsToggle.getToggleState())
            for (auto& s : mStrips)
                if (s.toggle->getToggleState())
                    o.stems.push_back ({ s.channelId, s.toggle->getButtonText() });

        o.dither    = (BuilderPage::RenderOptions::Dither)
                        juce::jlimit (0, 2, mDither.getSelectedId() - 1);
        o.normalize = mNormToggle.getToggleState();
        o.lufsTarget = readLufsBox (mLufsTarget, -14.0f);

        // Ruling 6a: the moment the choice is USED it is also persisted, so
        // version capture's next take is judged against the same standard.
        persistSpecChoice();

        return o;
    }

    void persistSpecChoice()
    {
        auto prefs = openUiPrefs();
        prefs->setValue ("exSpecId",     mSpecCombo.getSelectedItemIndex());
        prefs->setValue ("exSpecCustom", (double) readLufsBox (mCustomLufs, -14.0f));
        prefs->saveIfNeeded();
    }

    void onExportClicked()
    {
        if (mState != State::Options) return;
        mOpts = gatherOptions();
        mJob  = Job::Render;
        mEnsureSaved ([sp = juce::Component::SafePointer<ExportAudioDialog> (this)]
        {
            if (sp != nullptr) sp->pickDestination();
        });
    }

    // TS7: measure needs no destination and no save-first interlock -- it writes
    // nothing.  Straight to the offline loop.
    void onMeasureClicked()
    {
        if (mState != State::Options) return;
        mOpts   = gatherOptions();
        mSpecId = (LoudnessSpec::Id) juce::jlimit (0, LoudnessSpec::numSpecs() - 1,
                                                  mSpecCombo.getSelectedItemIndex());
        // §4.2: the revealed Custom box is the reference value.  Through
        // readLufsBox, never a raw getFloatValue: an empty box or a lone "-"
        // parses as 0, and measuring against 0 LUFS flags everything.
        mCustomTarget = readLufsBox (mCustomLufs, -14.0f);
        mJob    = Job::Measure;
        mMeasureLine1.setText ("Measuring...", juce::dontSendNotification);
        mMeasureLine2.setText ({}, juce::dontSendNotification);
        beginJob();
    }

    // Formats the measurement into the two readout lines.  Kept here rather than in
    // run() because it touches Components and must stay on the message thread.
    void showMeasurement (const BuilderPage::MeasureResult& m)
    {
        const auto spec = m.resolvedSpec();   // §4.2: honours a Custom target

        mMeasureLine1.setText ("Integrated " + juce::String (m.integratedLufs, 1) + " LUFS"
                             + "   LRA " + juce::String (m.lraLu, 1) + " LU"
                             + "   True peak " + juce::String (m.truePeakDb, 1) + " dBTP",
                               juce::dontSendNotification);

        juce::String verdict = juce::String (spec.name) + ": ";
        if (m.integratedInSpec && m.truePeakInSpec)
        {
            verdict += spec.checksIntegrated ? "in spec" : "true peak in spec";
        }
        else
        {
            juce::StringArray fails;
            if (! m.integratedInSpec)
                fails.add ("loudness "
                           + juce::String (m.integratedLufs - spec.integratedLufs, 1)
                           + " LU off target");
            if (! m.truePeakInSpec)
                fails.add ("true peak "
                           + juce::String (m.truePeakDb - spec.maxTruePeakDb, 1)
                           + " dB over");
            verdict += fails.joinIntoString (", ");
        }
        if (! m.violations.empty())
            verdict += "   (" + juce::String ((int) m.violations.size())
                     + (m.violationsTruncated ? "+ flagged spans)" : " flagged spans)");
        mMeasureLine2.setText (verdict, juce::dontSendNotification);
    }

    void pickDestination()
    {
        const juce::String ext = extFor (mOpts.format);
        mChooser = std::make_shared<juce::FileChooser> (
            "Export Audio",
            mProc.getProjectExportsDir().getChildFile (
                mBaseName.replaceCharacter (' ', '_') + ext),
            "*" + ext);
        mChooser->launchAsync (
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [sp = juce::Component::SafePointer<ExportAudioDialog> (this)] (const juce::FileChooser& fc)
            {
                if (sp == nullptr) return;
                const juce::File dest = fc.getResult();
                if (dest == juce::File()) return;   // picker cancelled: options persist
                sp->mOpts.destination = dest;
                sp->beginRender();
            });
    }

    // One enable/disable sweep for both jobs -- a control missed here would stay
    // live during a render, which is how a mid-render option change becomes a
    // file that does not match the dialog.
    void setOptionControlsEnabled (bool on)
    {
        for (auto* c : { (juce::Component*) &mSel, (juce::Component*) &mTail,
                         (juce::Component*) &mFormat, (juce::Component*) &mQual,
                         (juce::Component*) &mSrate, (juce::Component*) &mStemsToggle,
                         (juce::Component*) &mDither, (juce::Component*) &mNormToggle,
                         (juce::Component*) &mLufsTarget, (juce::Component*) &mSpecCombo,
                         (juce::Component*) &mCustomLufs,
                         (juce::Component*) &mMeasureBtn,
                         (juce::Component*) &mStripViewport, (juce::Component*) &mExportBtn })
            c->setEnabled (on);
    }

    void beginJob()
    {
        mState = State::Rendering;
        mProgress = 0.0;
        setOptionControlsEnabled (false);
        mCancelBtn.setEnabled (true);
        mBar->setVisible (true);
        mPercent.setVisible (true);
        startTimerHz (30);
        startThread();
    }

    void beginRender() { beginJob(); }

    // TS7: a finished MEASURE returns to the options box rather than closing it --
    // the whole point is to read the number and then decide whether to export.
    void returnToOptions()
    {
        stopTimer();
        stopThread (4000);
        mState = State::Options;
        setOptionControlsEnabled (true);
        // §4.5 "Cancel stays live": onCancelClicked disables this button while a
        // job runs, and a MEASURE returns here instead of closing the dialog --
        // so without re-enabling it, cancelling a measure left the user with a
        // dead Cancel and only the window close box to get out.
        mCancelBtn.setEnabled (true);
        mBar->setVisible (false);
        mPercent.setVisible (false);
    }

    void onCancelClicked()
    {
        if (mState == State::Rendering)
        {
            // The abort path deletes partials + restores the session; the
            // completion callback closes the dialog.
            signalThreadShouldExit();
            mCancelBtn.setEnabled (false);
        }
        else if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        {
            dw->exitModalState (0);
        }
    }

    void timerCallback() override
    {
        mPercent.setText (juce::String ((int) std::round (mProgress * 100.0)) + "%",
                          juce::dontSendNotification);
    }

    void run() override
    {
        juce::String err;
        bool ok = true;

        // TS7: the measure job shares the offline loop and the progress plumbing
        // but writes nothing and leaves the dialog open.
        if (mJob == Job::Measure)
        {
            BuilderPage::MeasureResult m;
            ok = mBuilder.measureRender (mOpts, m, err,
                [this]            { return threadShouldExit(); },
                [this] (double p) { mProgress = p; },
                mSpecId, mCustomTarget);

            juce::MessageManager::callAsync (
                [sp = juce::Component::SafePointer<ExportAudioDialog> (this), ok, err, m]
                {
                    if (sp == nullptr) return;
                    sp->returnToOptions();
                    if (ok)
                    {
                        sp->mLastMeasure = m;
                        sp->mHaveMeasure = true;
                        sp->showMeasurement (m);
                        // NO report write here (Jeff's ruling 2026-07-31,
                        // option B): Measure is the pre-export check and its
                        // tooltip says "Writes no files" -- which was false
                        // while this path filed an HTML report per click.
                        // TS7 §2.7: the analyzer window is CL-227's face, so a
                        // completed measurement goes there too rather than living
                        // only as two lines of text in this dialog.
                        if (sp->mOnMeasurementReady) sp->mOnMeasurementReady (m);
                    }
                    else
                    {
                        sp->mMeasureLine1.setText (err.isEmpty() ? "Measurement cancelled."
                                                                : err,
                                                   juce::dontSendNotification);
                        sp->mMeasureLine2.setText ({}, juce::dontSendNotification);
                    }
                });
            return;
        }

        // CL-045: measure-then-gain.  Pass 1 = the CL-227 backend (meters,
        // no files, 0..50% of the bar); gain = target minus measured, BOTH
        // directions, with any boost capped so the estimated true peak stays
        // under the ceiling; pass 2 renders with the gain applied uniformly
        // at every writer (main + stems).
        if (mOpts.normalize)
        {
            BuilderPage::MeasureResult m;
            ok = mBuilder.measureRender (mOpts, m, err,
                [this]           { return threadShouldExit(); },
                [this] (double p) { mProgress = p * 0.5; });
            if (ok)
            {
                float gainDb = mOpts.lufsTarget - m.integratedLufs;
                gainDb = juce::jmin (gainDb, mOpts.ceilingDbTp - m.truePeakDb);
                mOpts.postGainDb = gainDb;
            }
        }

        if (ok)
            ok = mBuilder.renderToFile (mOpts, err,
                [this]           { return threadShouldExit(); },
                [this] (double p) { mProgress = mOpts.normalize ? 0.5 + p * 0.5 : p; });

        juce::MessageManager::callAsync (
            [sp = juce::Component::SafePointer<ExportAudioDialog> (this), ok, err]
            {
                if (sp == nullptr) return;   // window was closed mid-render
                sp->stopTimer();
                if (auto* dw = sp->findParentComponentOfClass<juce::DialogWindow>())
                    dw->exitModalState (ok ? 1 : 0);
                if (! ok && err != "Export cancelled.")
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::MessageBoxIconType::WarningIcon, "Export failed", err);
            });
    }

    BuilderPage&        mBuilder;
    VibeSynthProcessor& mProc;
    juce::String        mBaseName;
    std::function<void (std::function<void()>)> mEnsureSaved;

    juce::ComboBox mSel, mTail, mFormat, mQual, mSrate, mDither;
    juce::Label    mSelLbl, mTailLbl, mFormatLbl, mQualLbl, mSrateLbl, mDitherLbl;
    juce::ToggleButton mNormToggle;
    // TS7: typed LUFS entry, not a fixed pick list.
    juce::TextEditor   mLufsTarget, mCustomLufs;
    juce::Label        mLufsSuffix, mCustomLufsLbl;
    juce::ToggleButton mStemsToggle;
    // TS7 measure-before-render row.
    juce::Label      mSpecLbl, mMeasureLine1, mMeasureLine2;
    juce::ComboBox   mSpecCombo;
    juce::TextButton mMeasureBtn;
    struct StripRow { int channelId; std::unique_ptr<juce::ToggleButton> toggle; };
    juce::Component  mStripList;
    juce::Viewport   mStripViewport;
    std::vector<StripRow> mStrips;
    juce::TextButton mExportBtn, mCancelBtn;
    std::unique_ptr<juce::ProgressBar> mBar;
    juce::Label      mPercent;
    double           mProgress { 0.0 };
    State            mState { State::Options };
    enum class Job { Render, Measure };
    Job              mJob    { Job::Render };
    LoudnessSpec::Id mSpecId { LoudnessSpec::Id::Streaming14 };
    float            mCustomTarget { -14.0f };   // §4.2, only read when Custom
    BuilderPage::MeasureResult mLastMeasure;
    bool                       mHaveMeasure { false };
public:
    // Supplied by doExportAudio; fires on the message thread once a measurement
    // completes so the analyzer window can show the curve.
    std::function<void (const BuilderPage::MeasureResult&)> mOnMeasurementReady;
private:
    BuilderPage::RenderOptions      mOpts;
    std::shared_ptr<juce::FileChooser> mChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExportAudioDialog)
};

void StandaloneEditor::doExportAudio()
{
    if (mBuilderPage == nullptr) return;

    juce::String base = mProjectManager ? mProjectManager->getCurrentName() : juce::String();
    if (base.isEmpty()) base = "Song";

    // Locked destination interlock: exports land in <project>\Exports\, so an
    // unsaved session runs the standard save flow first (badger Task 12's
    // success-only continuation) and the export continues from it.
    auto ensureSaved = [this] (std::function<void()> then)
    {
        const juce::File proj = mProcessor.getCurrentProjectFolder();
        if (proj != juce::File() && proj.isDirectory()) { then(); return; }
        juce::AlertWindow::showOkCancelBox (
            juce::MessageBoxIconType::QuestionIcon,
            "Save project first",
            "Exports are saved into the project's Exports folder.\n"
            "Save the project now?",
            "Save...", "Cancel", nullptr,
            juce::ModalCallbackFunction::create ([this, then] (int r)
            {
                if (r == 1) doFileSaveAs (then);
            }));
    };

    juce::DialogWindow::LaunchOptions lo;
    auto* dlg = new ExportAudioDialog (
        *mBuilderPage, mProcessor, base,
        mMixerPage ? mMixerPage->getStemPickEntries()
                   : std::vector<MixerPage::StemPickEntry>(),
        ensureSaved);
    // TS7 §2.7: push a completed measurement into the analyzer window, opening it
    // if it is not already up -- the number is worth more as a curve than as two
    // lines of text in a dialog the user is about to close.
    dlg->mOnMeasurementReady = [this] (const BuilderPage::MeasureResult& m)
    {
        openMasterAnalyzerWindow();
        if (auto* win = findAuxWindow ("analyzer:master"))
            if (auto* v = dynamic_cast<MasterAnalyzerView*> (win->getContent()))
                v->showRenderedCurve (m.lufsCurve, m.integratedLufs, m.lraLu,
                                      m.truePeakDb,
                                      juce::String (LoudnessSpec::get (m.specId).name));
    };
    lo.content.setOwned (dlg);
    lo.dialogTitle                   = "Export Audio";
    lo.dialogBackgroundColour        = findColour (juce::ResizableWindow::backgroundColourId);
    lo.escapeKeyTriggersCloseButton  = true;
    lo.useNativeTitleBar             = false;
    lo.resizable                     = false;
    lo.launchAsync();
}

// ─────────────────────────────────────────────────────────────────────────────
// QA-Export Task 4 -- project bundle.  Walker + writer live in ProjectBundler
// so QA-ProjectSave's "Pack Project" reuses them rather than growing a copy.
// ─────────────────────────────────────────────────────────────────────────────
void StandaloneEditor::doExportProjectBundle()
{
    const juce::File projectFolder = mProcessor.getCurrentProjectFolder();
    if (projectFolder == juce::File() || ! projectFolder.isDirectory())
    {
        // QA-ProjectSave Task 12 (docket 17=b): an unsaved project runs the
        // standard save flow and continues into the bundle once the save
        // completes.  Cancel anywhere aborts the bundle (the completion only
        // fires on a successful save).
        juce::Component::SafePointer<StandaloneEditor> safe (this);
        doFileSaveAs ([safe]
        {
            if (safe) safe->doExportProjectBundle();
        });
        return;
    }

    auto* w = new juce::AlertWindow ("Export Project Bundle", {},
                                     juce::MessageBoxIconType::NoIcon);

    w->addComboBox ("mode",  { "Single .zip file", "Plain folder" }, "Bundle as");
    // Docket 22=b: neither scope copies Core Library any more, so the old
    // "includes Core Library" label was describing behaviour that no longer
    // exists.  The real distinction is whether files from outside the project
    // folder travel with it.
    w->addComboBox ("scope", { "Project files only (smallest)",
                               "Include my samples + outside files" },
                    "Contents");

    w->addButton ("Export", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, w, projectFolder] (int result)
        {
            if (result != 1) return;

            const bool asZip = w->getComboBoxComponent ("mode")->getSelectedItemIndex() == 0;
            const auto scope = w->getComboBoxComponent ("scope")->getSelectedItemIndex() == 1
                             ? ProjectBundler::Scope::SelfContained
                             : ProjectBundler::Scope::References;

            const juce::String name = projectFolder.getFileName();
            auto suggested = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                 .getChildFile (name + (asZip ? ".zip" : " Bundle"));

            auto chooser = std::make_shared<juce::FileChooser> (
                asZip ? "Save project bundle" : "Choose a folder for the bundle",
                suggested, asZip ? "*.zip" : "*");

            const int flags = asZip
                ? (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles)
                : (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectDirectories);

            chooser->launchAsync (flags,
                [this, chooser, projectFolder, asZip, scope] (const juce::FileChooser& fc)
                {
                    auto dest = fc.getResult();
                    if (dest == juce::File()) return;

                    if (mPM == nullptr) return;

                    // QA-ProjectSave Task 6 (docket 21): hand the bundler the
                    // same tab walk project save uses, so engine-held references
                    // (NAM captures, IRs, BaySickPlayer samples, sfizz kits) are
                    // part of the bundle instead of being silently omitted.
                    juce::XmlElement tabsXml ("Tabs");
                    serializeTabsInto (tabsXml);
                    auto refs = ProjectBundler::enumerate (*mPM, mProcessor, &tabsXml);

                    // Docket 22 (Jeff's addition): show the size BEFORE writing,
                    // so a large export is a decision rather than a surprise.
                    const auto bytes = ProjectBundler::estimateCopyBytes (refs, scope);
                    if (bytes > 0)
                    {
                        const juce::String sizeMsg =
                            "This bundle will copy "
                            + juce::File::descriptionOfSizeInBytes (bytes)
                            + " of audio alongside the project.\n\nContinue?";
                        if (! juce::NativeMessageBox::showOkCancelBox (
                                  juce::MessageBoxIconType::QuestionIcon,
                                  "Export Project Bundle", sizeMsg, nullptr, nullptr))
                            return;
                    }

                    auto res  = ProjectBundler::write (
                        refs, projectFolder, dest,
                        asZip ? ProjectBundler::Mode::Zip : ProjectBundler::Mode::Folder,
                        scope);

                    if (! res.ok)
                    {
                        juce::AlertWindow::showMessageBoxAsync (
                            juce::MessageBoxIconType::WarningIcon,
                            "Export Project Bundle", res.error, "OK");
                        return;
                    }

                    // Missing files are REPORTED, never silently dropped -- a
                    // bundle that quietly omits samples looks fine until it is
                    // opened somewhere else.
                    juce::String msg = "Bundle written to:\n" + dest.getFullPathName()
                                     + "\n\nExtra files copied: " + juce::String (res.filesCopied);
                    if (! res.missing.isEmpty())
                    {
                        msg << "\n\nWARNING - " << res.missing.size()
                            << " referenced file(s) could not be found and are NOT in the bundle:\n";
                        const int shown = juce::jmin (10, (int) res.missing.size());
                        for (int i = 0; i < shown; ++i)
                            msg << "  " << res.missing[i] << "\n";
                        if (res.missing.size() > 10)
                            msg << "  ...and " << (res.missing.size() - 10) << " more\n";
                    }

                    juce::AlertWindow::showMessageBoxAsync (
                        res.missing.isEmpty() ? juce::MessageBoxIconType::InfoIcon
                                              : juce::MessageBoxIconType::WarningIcon,
                        "Export Project Bundle", msg, "OK");
                });
        }), true);
}

void StandaloneEditor::doFileQuickOpen()
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
        HeavyOperationOverlay::ScopedOp heavyOp (mHeavyOpOverlay,
                                                 "Loading Project...");
        mHeavyOpOverlay.setStepLabel ("Closing old tabs...");
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
    opts.dialogTitle            = "Quick Open Project";
    opts.dialogBackgroundColour = juce::Colours::black;
    opts.content.setOwned (browser);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar      = false;   // TS7 §9.3
    opts.resizable              = true;
    opts.launchAsync();
    });   // close confirmDiscardChanges continuation
}

void StandaloneEditor::doFileSave()
{
    if (! mProjectManager->hasProject())
    {
        // No project yet - Save behaves like Save As.  User sees the naming
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

void StandaloneEditor::doFileSaveAs (std::function<void()> onSaved)
{
    const juce::String defaultName = mProjectManager->hasProject()
                                       ? mProjectManager->getCurrentName()
                                       : juce::String ("Untitled Project");
    promptForProjectName (
        "Save Project As",
        "Save a copy of this project under a new name.",
        defaultName,
        [this, onSaved] (juce::String name)
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
                        [this, onSaved] (int) { doFileSaveAs (onSaved); }));
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
            // QA-ProjectSave Task 12: success-only completion (cancel and
            // failure paths return above without firing).
            if (onSaved) onSaved();
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
                        HeavyOperationOverlay::ScopedOp heavyOp (mHeavyOpOverlay,
                                                                 "Restoring Backup...");
                        mHeavyOpOverlay.setStepLabel ("Closing old tabs...");
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
// QA-ProjectSave Task 2 (2026-07-26): the structural half of the UI state --
// which tabs exist, with what engines and state, and how the mixer strips are
// named + ordered.  Split out of serializeUIState because a TEMPLATE is exactly
// this and nothing else: a project skeleton with no arrangement content.  The
// project save wraps it with the session extras (metronome, VU calibration,
// scroll/selection, active tab, song-loop) that a template deliberately omits.
void StandaloneEditor::serializeStructuralUIState (juce::XmlElement& ui)
{
    auto* tabs = ui.createNewChildElement ("Tabs");
    serializeTabsInto (*tabs);
    serializeStripNamesAndOrders (ui);
}

// QA-ProjectSave Task 5 (2026-07-26, dockets 23/24).  Scope is deliberately the
// references that are REACHABLE as plain attributes: a Clips tab's clipPath, and
// the sample path inside each BaySickPlayer tab's engineData.  NAM captures and
// user IRs live inside the Inst chain XML and are adopted through the same
// helper by walking that blob's path attributes.
//
// sfizz kitPath is skipped by adoptIntoUserSamples itself, not by an exclusion
// here: every shipped kit is Core Library resident, so it takes the
// already-under-a-stable-root branch and is referenced rather than copied.
void StandaloneEditor::adoptTemplateSampleRefs (juce::XmlElement& tabs)
{
    auto adoptAttr = [] (juce::XmlElement& el, const char* attr)
    {
        const auto stored = el.getStringAttribute (attr);
        if (stored.isEmpty() || SampleLibrary::isStableRef (stored)) return;
        const auto src = SampleLibrary::resolvePersistedRef (stored);
        if (auto adopted = SampleLibrary::adoptIntoUserSamples (src); adopted.isNotEmpty())
            el.setAttribute (attr, adopted);
    };

    // BaySickPlayer's sample path lives inside the base64 engine blob rather
    // than as an attribute, so adopting it means decode -> rewrite -> re-encode.
    // Same decoder the bundler's engine walk uses (Task 6); the difference is
    // that this one writes the reference back.
    auto adoptInsideEngineBlob = [&adoptAttr] (juce::XmlElement& rec, const char* blobAttr)
    {
        const auto b64 = rec.getStringAttribute (blobAttr);
        if (b64.isEmpty()) return;

        juce::MemoryBlock mb;
        if (! mb.fromBase64Encoding (b64)) return;
        auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
        if (! xml) return;

        const auto before = xml->getStringAttribute ("bsp_loadPath");
        if (before.isEmpty()) return;
        adoptAttr (*xml, "bsp_loadPath");
        if (xml->getStringAttribute ("bsp_loadPath") == before) return;   // nothing adopted

        juce::MemoryBlock out;
        juce::AudioProcessor::copyXmlToBinary (*xml, out);
        rec.setAttribute (blobAttr, out.toBase64Encoding());
    };

    for (auto* rec : tabs.getChildWithTagNameIterator ("Tab"))
    {
        adoptAttr (*rec, "clipPath");
        adoptInsideEngineBlob (*rec, "engineData");

        // Inst chain XML carries the NAM capture + user IR paths as attributes
        // on its own nested elements; walk it and rewrite in place.
        const auto chainXml = rec->getStringAttribute ("instChainState");
        if (chainXml.isNotEmpty())
        {
            if (auto parsed = juce::XmlDocument::parse (chainXml))
            {
                bool changed = false;
                for (auto* el : parsed->getChildIterator())
                    for (const char* attr : { "path", "namPath", "irPath",
                                              "micUserIrPath", "micbUserIrPath" })
                    {
                        const auto before = el->getStringAttribute (attr);
                        if (before.isEmpty()) continue;
                        adoptAttr (*el, attr);
                        if (el->getStringAttribute (attr) != before) changed = true;
                    }
                if (changed)
                    rec->setAttribute ("instChainState", parsed->toString());
            }
        }
    }
}

void StandaloneEditor::serializeUIState (juce::XmlElement& root)
{
    auto* ui = root.createNewChildElement ("UIState");
    ui->setAttribute ("version", 1);
    serializeStructuralUIState (*ui);

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
    {
        // 2026-05-02: meter latency-compensation toggle (Mixer hamburger menu).
        // Defaults off; persisted with the project so saved-state matches user's
        // last preference within that project.
        auto* mlc = ui->createNewChildElement ("MeterLatencyComp");
        mlc->setAttribute ("on",
            MeterLatencyComp::gEnabled.load (std::memory_order_relaxed) ? 1 : 0);
    }
    if (mTransport)
    {
        auto* sl = ui->createNewChildElement ("SongLoop");
        sl->setAttribute ("on", mTransport->isSongLoopMode() ? 1 : 0);
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
        // EXHAUSTIVE (2026-07-30).  This table carried four of EngineKind's
        // eleven values, so selecting any other engine and saving wrote
        // "DrumKit" and the project reopened on the wrong roll.  Plugin is the
        // TS6 (BLU-447) omission; the other six predate this batch and are fixed
        // here rather than routed because the defect IS the missing rows -- a
        // partial fix would leave the same silent wrong-answer for six kinds.
        const char* kindStr = "DrumKit";
        switch (id.kind)
        {
            case EngineKind::Layer:             kindStr = "Layer";             break;
            case EngineKind::Bass:              kindStr = "Bass";              break;
            case EngineKind::Drum:              kindStr = "Drum";              break;
            case EngineKind::Clip:              kindStr = "Clip";              break;
            case EngineKind::Vox:               kindStr = "Vox";               break;
            case EngineKind::Inst:              kindStr = "Inst";              break;
            case EngineKind::BaySickGuitars:    kindStr = "BaySickGuitars";    break;
            case EngineKind::BaySickBasses:     kindStr = "BaySickBasses";     break;
            case EngineKind::BaySickRustyDrums: kindStr = "BaySickRustyDrums"; break;
            case EngineKind::Plugin:            kindStr = "Plugin";            break;
            case EngineKind::DrumKit:           kindStr = "DrumKit";           break;
        }
        prSel->setAttribute ("kind",  kindStr);
        prSel->setAttribute ("index", id.index);
    }

}

// Shared by project save and template save: one <Tab> child per dynamic page,
// carrying its type / page index / name / engine + base64 engine state, plus
// the per-type extras (clip path, Inst chain + sfizz source, Rusty kit).
// A template captures the identical tab set -- what differs between a project
// and a template is what wraps this, not the tabs themselves.
void StandaloneEditor::serializeTabsInto (juce::XmlElement& tabs)
{
    auto encodeEngineState = [](juce::AudioProcessor* eng) -> juce::String
    {
        if (eng == nullptr) return {};
        juce::MemoryBlock mb;
        eng->getStateInformation (mb);
        return mb.toBase64Encoding();
    };

    for (auto& e : mPages)
    {
        juce::XmlElement* rec = nullptr;
        // QA-ModelShell TS6 (BLU-447): a hosted VST3 instrument tab.  "engine"
        // is the plugin's stable identifier string and "engineData" is the
        // HostedPluginInstance blob (description + bridge preference + the
        // plugin's own state), so this needs no attributes the other kinds do
        // not already have.  Tested first because PluginsPage is not related to
        // the page types below it -- order is only about cast cost.
        if (auto* pp = dynamic_cast<PluginsPage*> (e->component.get()))
        {
            rec = tabs.createNewChildElement ("Tab");
            rec->setAttribute ("type",       "Plugins");
            rec->setAttribute ("pageIndex",  pp->getPageIndex());
            rec->setAttribute ("name",       pp->getTabName());
            rec->setAttribute ("engine",     pp->getEngineType());
            rec->setAttribute ("engineData", encodeEngineState (pp->getEngineProcessor()));
        }
        else if (auto* lp = dynamic_cast<LayersPage*> (e->component.get()))
        {
            rec = tabs.createNewChildElement ("Tab");
            rec->setAttribute ("type",       "Layers");
            rec->setAttribute ("pageIndex",  lp->getPageIndex());
            rec->setAttribute ("name",       lp->getTabName());
            rec->setAttribute ("engine",     lp->getEngineType());
            rec->setAttribute ("engineData", encodeEngineState (lp->getEngineProcessor()));
        }
        else if (auto* bp = dynamic_cast<BassPage*> (e->component.get()))
        {
            rec = tabs.createNewChildElement ("Tab");
            rec->setAttribute ("type",       "Bass");
            rec->setAttribute ("pageIndex",  bp->getPageIndex());
            rec->setAttribute ("name",       bp->getTabName());
            rec->setAttribute ("engine",     bp->getEngineType());
            rec->setAttribute ("engineData", encodeEngineState (bp->getEngineProcessor()));
        }
        else if (auto* dp = dynamic_cast<DrumPage*> (e->component.get()))
        {
            // D1.4: dynamic-drum tab (new model - one engine per drum).
            // Legacy "Drums" type (16-slot kit) emit removed 2026-04-25.
            rec = tabs.createNewChildElement ("Tab");
            rec->setAttribute ("type",       "Drum");
            rec->setAttribute ("pageIndex",  dp->getPageIndex());
            rec->setAttribute ("name",       dp->getTabName());
            rec->setAttribute ("engine",     dp->getEngineType());
            rec->setAttribute ("engineData", encodeEngineState (dp->getEngineProcessor()));
        }
        // 2026-04-30 (audit C6): Clips / Vox / Inst tabs were dropped on
        // every save+reopen - the mixer strip's APVTS state survived but
        // the ribbon tab + page disappeared.  Now serialized with the same
        // shape as Layer/Bass/Drum.  Clips tabs additionally carry their
        // bound audioFilePath so the engine reloads the same sample.
        else if (auto* cp = dynamic_cast<ClipsPage*> (e->component.get()))
        {
            rec = tabs.createNewChildElement ("Tab");
            rec->setAttribute ("type",       "Clips");
            rec->setAttribute ("pageIndex",  cp->getPageIndex());
            rec->setAttribute ("name",       cp->getTabName());
            rec->setAttribute ("engine",     (int) cp->getEngineType());
            rec->setAttribute ("engineData", encodeEngineState (cp->getEngineProcessor()));
            rec->setAttribute ("clipPath",   SampleLibrary::refForPersist (juce::File (cp->getClipFilePath())));
        }
        else if (auto* vp = dynamic_cast<VoxPage*> (e->component.get()))
        {
            rec = tabs.createNewChildElement ("Tab");
            rec->setAttribute ("type",       "Vox");
            rec->setAttribute ("pageIndex",  vp->getPageIndex());
            rec->setAttribute ("name",       vp->getTabName());
            rec->setAttribute ("engine",     (int) vp->getEngineType());
            rec->setAttribute ("engineData", encodeEngineState (vp->getEngineProcessor()));
        }
        else if (auto* ip = dynamic_cast<InstPage*> (e->component.get()))
        {
            rec = tabs.createNewChildElement ("Tab");
            rec->setAttribute ("type",       "Inst");
            rec->setAttribute ("pageIndex",  ip->getPageIndex());
            rec->setAttribute ("name",       ip->getTabName());
            rec->setAttribute ("engine",     (int) ip->getEngineType());
            rec->setAttribute ("engineData", encodeEngineState (ip->getEngineProcessor()));
            // K-6 follow-up (2026-05-05): the chain wrapper's getStateInformation
            // is empty - capturing it leaves BaySickPedals + BaySickNAM/IR state
            // unsaved.  exportInstState() walks the page's owned mPedalsProc +
            // mNamIrProc and serializes their state into a single XML blob
            // (the same one used for the right-click Duplicate flow).
            rec->setAttribute ("instChainState", ip->exportInstState());

            // K-6 (2026-05-05): persist sfizz-source state for BaySickGuitars
            // (and BaySickBasses, when L-5 lands).  source attribute drives
            // the deserialize dispatch; kitPath restores the chosen program;
            // sfizzEngineData captures the per-instance engine's APVTS + the
            // current kit path; ProgramStateCache child preserves every
            // program's user tweaks so swapping back restores them.
            const auto src = ip->getSource();
            if (src == InstPage::Source::BaySickGuitars
                || src == InstPage::Source::BaySickBasses)
            {
                rec->setAttribute ("source",
                    src == InstPage::Source::BaySickGuitars ? "BaySickGuitars" : "BaySickBasses");

                if (src == InstPage::Source::BaySickGuitars)
                {
                    if (auto* eng = mProcessor.getBaySickGuitars (ip->getPageIndex()))
                    {
                        rec->setAttribute ("kitPath", SampleLibrary::refForPersist (eng->getCurrentKitPath()));
                        rec->setAttribute ("sfizzEngineData", encodeEngineState (eng));
                    }
                }
                else if (src == InstPage::Source::BaySickBasses)
                {
                    if (auto* eng = mProcessor.getBaySickBasses (ip->getPageIndex()))
                    {
                        rec->setAttribute ("kitPath", SampleLibrary::refForPersist (eng->getCurrentKitPath()));
                        rec->setAttribute ("sfizzEngineData", encodeEngineState (eng));
                    }
                }

                if (auto* cacheXml = ip->serializeProgramCacheXml())
                    rec->addChildElement (cacheXml);
            }
        }
        // J-9 (2026-05-05): BaySickRustyDrums singleton tab.  Singleton means
        // pageIndex is always 0 and there's at most one of these in mPages.
        // engineData captures the BaySickRustyDrumsProcessor's full state
        // (apvts + KitPath); on load that re-runs loadKit which spawns the
        // 13 mixer InsertNodes via the wrapper.
        else if (dynamic_cast<BaySickRustyDrumsPage*> (e->component.get()))
        {
            rec = tabs.createNewChildElement ("Tab");
            rec->setAttribute ("type",       "BaySickRustyDrums");
            rec->setAttribute ("pageIndex",  0);
            rec->setAttribute ("engineData", encodeEngineState (mProcessor.getBaySickRustyDrums()));
        }
        // ── TS7 §6.8: freeze state is PROJECT DATA ───────────────────────────
        // Written once here rather than in each branch above, so a tab kind can
        // never be added without its freeze state travelling with it.
        //
        // The FILES are not saved -- they live in <project>\Freeze\ as
        // regenerable cache and the bundle excludes them.  What persists is
        // WHICH tabs are frozen and HOW they came to be:
        //   frozenBy "manual" -> restores frozen on ANY machine, whatever that
        //                        machine's auto-freeze threshold says.
        //   frozenBy "auto"   -> re-evaluated on load, because one computer's
        //                        performance adaptation means nothing on another.
        if (rec != nullptr)
        {
            const juce::String tt = rec->getStringAttribute ("type");
            const int          pi = rec->getIntAttribute ("pageIndex");
            TabKind kind  = TabKind::Layers;
            // ALL EIGHT KINDS (fixed 2026-07-31).  This listed only the four
            // engine-page types, so after freeze was extended to Clips / Vox /
            // Inst / Rusty those four saved nothing -- freezing one, saving and
            // reopening silently came back UNFROZEN with its file still on disk.
            bool    known = true;
            // The match strings are the RECORD types the branches above write,
            // not the TabKind names: "Drum" (singular) and "BaySickRustyDrums".
            // Matching "Drums"/"Rusty" here meant those two freezes were never
            // saved at all -- found by the 2026-07-31 batch review.
            if      (tt == "Layers")            kind = TabKind::Layers;
            else if (tt == "Bass")              kind = TabKind::Bass;
            else if (tt == "Drum")              kind = TabKind::Drums;
            else if (tt == "Plugins")           kind = TabKind::Plugins;
            else if (tt == "Clips")             kind = TabKind::Clips;
            else if (tt == "Vox")               kind = TabKind::Vox;
            else if (tt == "Inst")              kind = TabKind::Inst;
            else if (tt == "BaySickRustyDrums") kind = TabKind::Rusty;
            else                                known = false;

            if (known && mProcessor.engineRig().isFrozen (kind, pi))
            {
                rec->setAttribute ("frozen", 1);
                const auto* t = mProcessor.engineRig().findTab (kind, pi);
                rec->setAttribute ("frozenBy",
                                   (t != nullptr && t->frozenByUser) ? "manual" : "auto");
                // Save-while-stale travels too: without it, reopening reused
                // the matching-stamp file even though the freeze was already
                // known to be wrong when the project was written.
                if (t != nullptr && t->freezeStale)
                    rec->setAttribute ("freezeStale", 1);
                // §6.8 SPAN -- what the file on disk actually covers.  Without it
                // a restored freeze is a file of unknown length standing in for a
                // track, which is how a song-scope render ended up being played
                // at the pattern-mode playhead.
                if (t != nullptr)
                {
                    rec->setAttribute ("freezeScope",  t->freezeSpan.patternIndex);
                    rec->setAttribute ("freezeBeats",  t->freezeSpan.lengthBeats);
                    rec->setAttribute ("freezeBpm",    t->freezeSpan.bpm);
                    // Stored as a STRING: the stamp is a uint32 and XML integer
                    // attributes round-trip through int, so anything above
                    // 2^31 would come back negative.
                    rec->setAttribute ("freezeStamp",
                                       juce::String ((juce::uint64) t->freezeSpan.contentStamp));

                    // One child per cached pattern render, so each is validated
                    // independently on reload.
                    for (const auto& kv : t->patternStamps)
                    {
                        auto* ps = rec->createNewChildElement ("FreezePattern");
                        ps->setAttribute ("index", kv.first);
                        ps->setAttribute ("stamp", juce::String ((juce::uint64) kv.second));
                    }
                }
            }
        }
        juce::ignoreUnused (rec);
    }
}

// Shared by project save and template save: mixer strip custom names + the
// user's display order.  Both are structural -- which strips exist and how they
// are arranged -- so a template needs them.  Per-strip DSP values (fader, pan,
// width, sends, racks, EQ) are NOT here; they ride the <Processor> APVTS
// snapshot that both save paths emit alongside <UIState>.
void StandaloneEditor::serializeStripNamesAndOrders (juce::XmlElement& ui)
{
    if (mMixerPage)
    {
        auto writeStripNames = [&](const char* tag, const juce::String& defaultPrefix,
                                     const std::vector<int>& indices,
                                     std::function<juce::String(int)> getName)
        {
            auto* list = ui.createNewChildElement (tag);
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

        // D.3: persist mixer strip insertion order so re-ordered strips come
        // back in the user's display order, not numeric-index order.
        auto writeOrder = [&](const char* tag, const std::vector<int>& indices)
        {
            auto* list = ui.createNewChildElement (tag);
            for (int idx : indices)
            {
                auto* rec = list->createNewChildElement ("S");
                rec->setAttribute ("idx", idx);
            }
        };
        writeOrder ("AuxOrder",   mMixerPage->getAuxStripIndices());
        writeOrder ("VoxOrder",   mMixerPage->getVoxStripIndices());
        writeOrder ("InstOrder",  mMixerPage->getInstStripIndices());
        writeOrder ("AudioOrder", mMixerPage->getAudioStripIndices());
    }
}

// Close every dynamic tab.  Driven through `clearAllDynamicTabs` (an
// unconditional ribbon wipe) plus explicit mPages cleanup via onTabClosed,
// rather than per-tab closeTab calls, so system tab types are skipped in one
// pass.  (Until 2026-07-26 this also had to route around closeTab's refusal to
// remove the last instance of a type; docket 18 retired that floor.)
void StandaloneEditor::closeAllDynamicTabs()
{
    closeDynamicTabs (TabTeardownScope::AllDynamic);
}

// QA-ProjectSave Task 3 (2026-07-26): teardown scoped to what the caller will
// restore.  A v2 template restores every tab type, so it clears everything; a v1
// FACTORY template only ever restores Layers / Bass / Drums, so wiping Clips /
// Vox / Inst / Rusty on its way in would destroy work it cannot put back.
void StandaloneEditor::closeDynamicTabs (TabTeardownScope scope)
{
    // §6.6: queued re-renders belong to the tabs being torn down.  Left alone
    // they drain into the NEXT project against whatever tabs restore at the
    // same (kind, index) -- a full re-render of a freshly restored, perfectly
    // valid freeze.  Scope-gated: a partial (template) teardown keeps jobs for
    // the tabs it keeps; the drain's own liveness check purges the rest.
    if (scope == TabTeardownScope::AllDynamic)
    {
        mFreezeRefreshQueue.clear();
        mAutoFreezePending = false;
    }
    // 2026-05-06: project-load barrier.  Set BEFORE teardown so the audio
    // thread bails on the next callback while we're tearing down engines +
    // pages.  Sleep ~30ms to let any in-flight processBlock complete (covers
    // up to 1024-sample buffers at 44.1kHz).
    //
    // QA-Ef (2026-05-22): nest-aware.  When a project load already raised the
    // shield (deserializeProject), leave it raised on exit so the tab/engine
    // REBUILD that follows this teardown stays protected too -- only the
    // outermost owner clears it.  Standalone callers (New Project, editor
    // teardown) still set+clear it themselves (wasLoading == false).  The drain
    // sleep only fires for the outermost owner; nested calls skip it because
    // deserializeProject already drained.
    const bool wasLoading = mProcessor.isProjectLoadInProgress();
    mProcessor.setProjectLoadInProgress (true);
    if (! wasLoading)
        juce::Thread::sleep (30);

    // 2026-05-06: extended scope to ALL dynamic tab types (was Layers/Bass/
    // Drums only).  Inst/Vox/Clip/Rusty all hold engines that need teardown
    // through onTabClosed (engine destroy + mixer-strip removal + slot
    // index freeing) - without this, project re-open leaks the old engines
    // (each sfizz Guitars/Basses kit is ~400-500MB) and the next project
    // load piles a new instance on top of the zombie.
    const bool lbdOnly = (scope == TabTeardownScope::LayersBassDrumsOnly);

    juce::Array<int> toClose;
    for (auto& e : mPages)
    {
        if (! e) continue;
        const bool isRusty = dynamic_cast<BaySickRustyDrumsPage*> (e->component.get()) != nullptr;
        // A Rusty page carries TabType::Drums, so it must be excluded explicitly
        // from the L/B/D-only scope -- it is a singleton sfizz engine no
        // Layer/Bass/Drum restore path puts back.
        const bool inScope = lbdOnly
            ? ((e->type == RibbonTabBar::TabType::Layers
                || e->type == RibbonTabBar::TabType::Bass
                || e->type == RibbonTabBar::TabType::Drums) && ! isRusty)
            : (e->type == RibbonTabBar::TabType::Layers
                || e->type == RibbonTabBar::TabType::Bass
                || e->type == RibbonTabBar::TabType::Drums
                || e->type == RibbonTabBar::TabType::Inst
                || e->type == RibbonTabBar::TabType::Vox
                || e->type == RibbonTabBar::TabType::Clip
                || isRusty);
        if (inScope)
            toClose.add (e->ribbonTabId);
    }
    for (int id : toClose)
        onTabClosed (id);   // removes mPages entry + frees index slot + destroys engine
    if (mRibbon)
    {
        if (lbdOnly)
        {
            // Per-type wipe: anything the caller will not restore must survive.
            mRibbon->clearTabsOfType (RibbonTabBar::TabType::Layers);
            mRibbon->clearTabsOfType (RibbonTabBar::TabType::Bass);
            mRibbon->clearTabsOfType (RibbonTabBar::TabType::Drums);
        }
        else
        {
            mRibbon->clearAllDynamicTabs();   // unconditional ribbon wipe
        }
    }

    // QA-D STATE-02: reset monotonic tab-name counters so the next +Add
    // (after a New Project or before a project-load deserialize replays
    // saved tabs) starts at 1 instead of continuing from the prior project's
    // count.  Saved-project loads call advanceCountersFromRestoredTabs at
    // the end of deserializeUIState to bump each counter past max(restored).
    //
    // Scoped teardown does NOT run the full reset: it would zero the counters
    // for Vox / Inst / Clip tabs still on screen, so the next +Add would collide
    // with a live tab's name.  Only the three counters whose tabs just went are
    // rewound.
    if (lbdOnly)
    {
        mNextLayerNameNum = 1;
        mNextBassNameNum  = 1;
        mNextDrumNameNum  = 1;
    }
    else
    {
        resetProjectState();
    }

    // QA-Ef (2026-05-22): restore the prior shield state -- clears it only when
    // this call was the outermost owner (wasLoading == false); leaves it raised
    // when a project load wraps us so its rebuild stays shielded.
    mProcessor.setProjectLoadInProgress (wasLoading);
}

// ── QA-D STATE-02: monotonic tab-name counter lifecycle ─────────────────────
void StandaloneEditor::resetProjectState()
{
    // QA-ModelShell TS5: the per-effect and EQ windows are bound to racks that
    // this reset is about to destroy.  They would notice on their next poll and
    // close themselves, but closing them here means no window ever paints
    // against a rack that is mid-teardown.
    closeAllAuxWindows();

    mNextLayerNameNum   = 1;
    mNextBassNameNum    = 1;
    mNextDrumNameNum    = 1;
    mNextVoxNameNum     = 1;
    mNextInstNameNum    = 1;
    mNextGuitarNameNum  = 1;
    mNextBassesNameNum  = 1;
    mNextClipNameNum    = 1;

    // Project boundary: drop every automation applicator/reader so a load
    // never inherits the previous project's closures, then re-seed the
    // statics (APVTS params + "global_tempo") -- those only register at
    // construction; dynamic entries re-register as their owning UI rebuilds.
    mAutomationApplicators.clear();
    mAutomationValueReaders.clear();
    // QA-ModelShell TS3: this one call re-seeds everything the map holds that is
    // not model-event-driven, INCLUDING the "<prefix>_fader" aliases it derives.
    // Two things that used to follow it are gone: the owning-component index
    // teardown (no registration is component-scoped any more) and the
    // MixerPage::reRegisterStripAutomation shim that put back the strip
    // registrations this clear() used to drop.
    registerStaticAutomationHandlers();
}

// QA-ModelShell TS1: engine-parameter lane registration keyed to MODEL events
// (EngineRig::onEngineCreated), never to view builds.  Lane-id vocabulary
// matches what the engine editors stamp today:
//   Layers/Bass/Drums/Clips -- the engine APVTS param ids verbatim;
//   Vox  -- "vox<N>_" + id, covering the vocal APVTS and its embedded NAM/IR;
//   Inst -- "inst<N>_" + id for the NAM/IR stage.  Pedal lanes are uuid-keyed
//   rack lanes (EffectParamMap pedal tables land in TS3); the chain wrapper
//   has no parameters.
// Applicators are null-owner and re-resolve tab -> engine THROUGH the rig at
// apply time, so an engine swap can never leave a closure aimed at a freed
// processor.
void StandaloneEditor::registerModelEngineAutomation (EngineTab& tab)
{
    const TabKind kind = tab.kind;
    const int     idx  = tab.pageIndex;

    enum class Target { MainEngine, VocalNam, InstNam };

    auto resolveTarget = [this, kind, idx] (Target which) -> juce::AudioProcessor*
    {
        auto* t = mProcessor.engineRig().findTab (kind, idx);
        if (t == nullptr) return nullptr;
        switch (which)
        {
            case Target::MainEngine: return t->engine.get();
            case Target::VocalNam:
                if (auto* v = dynamic_cast<BaySickVocalProcessor*> (t->engine.get()))
                    return &v->getNamIrProcessor();
                return nullptr;
            case Target::InstNam:    return t->namIr;
        }
        return nullptr;
    };

    auto registerParamsOf = [resolveTarget] (Target which, juce::AudioProcessor* proc,
                                             const juce::String& lanePrefix)
    {
        if (proc == nullptr || EngineRig::apvtsOf (proc) == nullptr) return;

        for (auto* p : proc->getParameters())
        {
            auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
            if (rp == nullptr) continue;
            const juce::String pid     = lanePrefix + rp->paramID;
            const juce::String paramId = rp->paramID;

            // Vocal capture lock (owner call 2026-07-25, docket 4=A): while the
            // strip is recording, a write to the realtime-board set is vetoed
            // for the same reason the UI greys those controls out.  The lane is
            // not consumed -- the next tick after the veto clears applies
            // normally.  Moved here from BaySickVocalEditor's registration when
            // TS3 retired the widget wrappers; the veto had to travel with it or
            // an automated take would start clicking again.
            const bool gated = (which == Target::MainEngine)
                               && BaySickVocalProcessor::isCaptureGated (paramId);

            if (VKnobAutomation::sOnRegisterApplicator)
                VKnobAutomation::sOnRegisterApplicator (pid,
                    [resolveTarget, which, paramId, gated] (float v01)
                    {
                        auto* t = resolveTarget (which);
                        if (t == nullptr) return;
                        if (gated)
                            if (auto* v = dynamic_cast<BaySickVocalProcessor*> (t))
                                if (v->onIsStripRecording && v->onIsStripRecording())
                                    return;
                        if (auto* ap = EngineRig::apvtsOf (t))
                            if (auto* param = ap->getParameter (paramId))
                                param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v01));
                    });   // model-scoped: re-resolves through the rig at apply time

            if (VKnobAutomation::sOnRegisterReader)
                VKnobAutomation::sOnRegisterReader (pid,
                    [resolveTarget, which, paramId]() -> float
                    {
                        if (auto* t = resolveTarget (which))
                            if (auto* ap = EngineRig::apvtsOf (t))
                                if (auto* param = ap->getParameter (paramId))
                                    return param->getValue();
                        return 0.5f;
                    });
        }
    };

    switch (kind)
    {
        case TabKind::Layers:
        case TabKind::Bass:
        case TabKind::Drums:
        case TabKind::Clips:
            registerParamsOf (Target::MainEngine, tab.engine.get(), {});
            // BLU-344: Harmless also carries non-parameter mod-editor targets.
            // No-op for the other engine types.
            registerHarmlessModAutomation (kind, idx);
            break;

        case TabKind::Vox:
        {
            const juce::String pre = "vox" + juce::String (idx) + "_";
            registerParamsOf (Target::MainEngine, tab.engine.get(), pre);
            if (auto* v = dynamic_cast<BaySickVocalProcessor*> (tab.engine.get()))
                registerParamsOf (Target::VocalNam, &v->getNamIrProcessor(), pre);
            break;
        }

        case TabKind::Inst:
            registerParamsOf (Target::InstNam, tab.namIr,
                              "inst" + juce::String (idx) + "_");
            // The board's own lanes, plus the subscription that re-keys them
            // when the user swaps a pedal.  Wired here rather than in InstPage
            // because the board outlives every view of it.
            registerPedalAutomation (idx);
            if (tab.pedals != nullptr)
            {
                juce::Component::SafePointer<StandaloneEditor> safeThis (this);
                tab.pedals->onSlotAutomationChanged = [safeThis, idx]
                {
                    if (auto* ed = safeThis.getComponent())
                        ed->registerPedalAutomation (idx);
                };
            }
            break;
    }
}

void StandaloneEditor::registerSfizzEngineAutomation (VibeSynthProcessor::SfizzEngineKind kind,
                                                      int instIdx)
{
    using Kind = VibeSynthProcessor::SfizzEngineKind;

    // Re-resolve through the processor on every tick.  These engines are
    // destroyed and recreated by source switches and kit loads, so a captured
    // pointer would be exactly the stale-target bug the model-side rewrite
    // exists to prevent.
    auto resolveApvts = [this, kind, instIdx]() -> juce::AudioProcessorValueTreeState*
    {
        switch (kind)
        {
            case Kind::Guitars:
                if (auto* e = mProcessor.getBaySickGuitars (instIdx)) return &e->apvts;
                return nullptr;
            case Kind::Basses:
                if (auto* e = mProcessor.getBaySickBasses (instIdx))  return &e->apvts;
                return nullptr;
            case Kind::RustyDrums:
                if (auto* e = mProcessor.getBaySickRustyDrums())      return &e->apvts;
                return nullptr;
        }
        return nullptr;
    };

    auto* apvts = resolveApvts();
    if (apvts == nullptr) return;

    // Walk the engine's own parameter list so coverage stays complete as params
    // are added -- the same reason every other engine family uses a walk rather
    // than a hand-kept table.  Param ids are already globally unique (Guitars
    // "bgg_<idx>_", Basses "bbb_<idx>_", Rusty's singleton "brd_"), so the lane
    // id IS the param id -- which is exactly what the Aria panel's
    // "Automate: ..." menu already passes to sOnAutomate.
    for (auto* p : apvts->processor.getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr) continue;
        const juce::String paramId = rp->paramID;

        if (VKnobAutomation::sOnRegisterApplicator)
            VKnobAutomation::sOnRegisterApplicator (paramId,
                [resolveApvts, paramId] (float v01)
                {
                    if (auto* ap = resolveApvts())
                        if (auto* param = ap->getParameter (paramId))
                            param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v01));
                });

        if (VKnobAutomation::sOnRegisterReader)
            VKnobAutomation::sOnRegisterReader (paramId,
                [resolveApvts, paramId]() -> float
                {
                    if (auto* ap = resolveApvts())
                        if (auto* param = ap->getParameter (paramId))
                            return param->getValue();
                    return 0.5f;
                });
    }
}

void StandaloneEditor::registerHarmlessModAutomation (TabKind kind, int pageIndex)
{
    auto* tab = mProcessor.engineRig().findTab (kind, pageIndex);
    if (tab == nullptr) return;
    auto* harmless = dynamic_cast<HarmlessProcessor*> (tab->engine.get());
    if (harmless == nullptr) return;   // Harmless-only feature

    // Resolve the ModSourceState fresh on every tick.  The registry's target
    // pointers are stable for its lifetime, but the ENGINE is not -- a tab can
    // swap Harmless out for BaySickSynth and back -- so the walk starts from the
    // rig, exactly like the parameter applicators above.
    auto resolveSource = [this, kind, pageIndex] (const juce::String& targetId, int srcIdx)
                         -> std::pair<ModSourceState*, HarmlessModRegistry*>
    {
        auto* t = mProcessor.engineRig().findTab (kind, pageIndex);
        if (t == nullptr) return { nullptr, nullptr };
        auto* h = dynamic_cast<HarmlessProcessor*> (t->engine.get());
        if (h == nullptr) return { nullptr, nullptr };
        auto& reg = h->getModRegistry();
        auto* tgt = reg.findTarget (targetId);
        if (tgt == nullptr) return { nullptr, nullptr };
        if (srcIdx < 0 || srcIdx >= (int) ModSource::NumSources) return { nullptr, nullptr };
        return { &tgt->sources[(size_t) srcIdx], &reg };
    };

    for (const auto& tgtPtr : harmless->getModRegistry().getAllTargets())
    {
        if (tgtPtr == nullptr) continue;
        const juce::String targetId = tgtPtr->paramId;

        for (int s = 0; s < (int) ModSource::NumSources; ++s)
        {
            const juce::String base = targetId + "_mod" + juce::String (s) + "_";

            // DEPTH: bipolar, every source uses it.
            if (VKnobAutomation::sOnRegisterApplicator)
                VKnobAutomation::sOnRegisterApplicator (base + "depth",
                    [resolveSource, targetId, s] (float v01)
                    {
                        auto [src, reg] = resolveSource (targetId, s);
                        if (src == nullptr) return;
                        src->depth = -1.0f + juce::jlimit (0.0f, 1.0f, v01) * 2.0f;
                        reg->publishSnapshot();   // voices observe on their next block
                    });
            if (VKnobAutomation::sOnRegisterReader)
                VKnobAutomation::sOnRegisterReader (base + "depth",
                    [resolveSource, targetId, s]() -> float
                    {
                        auto [src, reg] = resolveSource (targetId, s);
                        juce::ignoreUnused (reg);
                        if (src == nullptr) return 0.5f;
                        return juce::jlimit (0.0f, 1.0f, (src->depth + 1.0f) * 0.5f);
                    });

            // LENGTH: only Envelope + LFO have time behavior -- the editor hides
            // the control for the other five sources, so a lane there would
            // address something the user cannot see or set.
            if (s != (int) ModSource::Envelope && s != (int) ModSource::LFO) continue;

            if (VKnobAutomation::sOnRegisterApplicator)
                VKnobAutomation::sOnRegisterApplicator (base + "length",
                    [resolveSource, targetId, s] (float v01)
                    {
                        auto [src, reg] = resolveSource (targetId, s);
                        if (src == nullptr) return;
                        // Same 13 discrete steps the knob offers, via the same
                        // table -- a lane cannot land between them.
                        const int last = HarmlessModLength::kNumSteps - 1;
                        const int i = juce::jlimit (0, last,
                            (int) std::lround ((double) juce::jlimit (0.0f, 1.0f, v01) * last));
                        src->length = HarmlessModLength::kBeats[i];
                        reg->publishSnapshot();
                    });
            if (VKnobAutomation::sOnRegisterReader)
                VKnobAutomation::sOnRegisterReader (base + "length",
                    [resolveSource, targetId, s]() -> float
                    {
                        auto [src, reg] = resolveSource (targetId, s);
                        juce::ignoreUnused (reg);
                        if (src == nullptr) return 0.5f;
                        const int last = HarmlessModLength::kNumSteps - 1;
                        return last > 0
                             ? (float) HarmlessModLength::nearestIndex (src->length) / (float) last
                             : 0.0f;
                    });
        }
    }
}

void StandaloneEditor::registerPedalAutomation (int instPageIndex)
{
    auto* tab = mProcessor.engineRig().findTab (TabKind::Inst, instPageIndex);
    if (tab == nullptr || tab->pedals == nullptr) return;

    // Matches what InstPage stamps on the board's panels, so a right-click
    // "Automate" and this registration name the same key.
    const juce::String channelPrefix = "inst" + juce::String (instPageIndex) + "_pedals";

    for (int s = 0; s < BaySickPedalsProcessor::kNumSlots; ++s)
    {
        const juce::String uuid = tab->pedals->getSlotUuid (s);
        const EffectType   type = tab->pedals->getSlotType (s);
        if (uuid.isEmpty() || type == EffectType::None) continue;

        // PanelContext::Pedal is the whole point of the second key dimension:
        // the board builds the *PedalPanel face for the 7 dual-panel types, and
        // those faces reuse rack knob labels for different setters and ranges.
        // For a pedal-native type it falls through to the DSP-read variant.
        const int variant = EffectParamMap::variantOf (
                                type, tab->pedals->getSlotEffect (s),
                                EffectParamMap::PanelContext::Pedal);
        const juce::String base = channelPrefix + "_" + uuid + "_";

        // Resolve board -> slot by UUID at APPLY time, never by index: the
        // board reorders slots, and an index-keyed lane would silently move to
        // whichever pedal landed in that position.
        auto resolveDsp = [this, instPageIndex, uuid]() -> DSPBase*
        {
            auto* t = mProcessor.engineRig().findTab (TabKind::Inst, instPageIndex);
            if (t == nullptr || t->pedals == nullptr) return nullptr;
            for (int i = 0; i < BaySickPedalsProcessor::kNumSlots; ++i)
                if (t->pedals->getSlotUuid (i) == uuid)
                    return t->pedals->getSlotEffect (i);
            return nullptr;
        };

        for (const auto& def : EffectParamMap::defsFor (type, variant))
        {
            const juce::String pid    = base + def.suffix;
            const juce::String suffix = def.suffix;

            if (VKnobAutomation::sOnRegisterApplicator)
                VKnobAutomation::sOnRegisterApplicator (pid,
                    [resolveDsp, type, variant, suffix] (float v01)
                    {
                        EffectParamMap::applyNorm (type, variant, resolveDsp(), suffix, v01);
                    });   // board-scoped: re-resolves through the rig at apply time

            if (VKnobAutomation::sOnRegisterReader)
                VKnobAutomation::sOnRegisterReader (pid,
                    [resolveDsp, type, variant, suffix]() -> float
                    {
                        return EffectParamMap::readNorm (type, variant, resolveDsp(), suffix, 0.5f);
                    });
        }
    }
}

// 2026-08-02 (ruling 2-b): lanes for the Plugins-tab instrument itself.  The
// tab index IS the identity (one engine per tab; a plugin swap leaves the old
// plugin's lanes resolving to a param id the new plugin does not have, which
// no-ops).  Applicators re-resolve through the rig at apply time -- never a
// cached engine pointer.
void StandaloneEditor::registerPluginTabAutomation (int pageIndex)
{
    auto* eng = dynamic_cast<Hosting::HostedPluginInstance*> (
        mProcessor.engineRig().engineFor (TabKind::Plugins, pageIndex));
    if (eng == nullptr) return;

    // Bridged lists arrive async; arm the arrival hook (self-re-arming, since
    // this function is the callback) so a restored project's lanes resolve
    // with no window open (Jeff, 2026-08-02).  SafePointer on the editor: the
    // engine is model-owned and outlives editor rebuilds.
    {
        juce::Component::SafePointer<StandaloneEditor> safeEd (this);
        eng->onParamListArrived = [safeEd, pageIndex]
        {
            if (auto* ed = safeEd.getComponent())
                ed->registerPluginTabAutomation (pageIndex);
        };
    }

    auto resolve = [this, pageIndex]() -> Hosting::HostedPluginInstance*
    {
        return dynamic_cast<Hosting::HostedPluginInstance*> (
            mProcessor.engineRig().engineFor (TabKind::Plugins, pageIndex));
    };

    const juce::String base = "plugtab" + juce::String (pageIndex) + "_vst_";
    for (const auto& p : eng->getAutomatableParams())
    {
        const juce::String pid     = base + p.id;
        const juce::String paramId = p.id;

        if (VKnobAutomation::sOnRegisterApplicator)
            VKnobAutomation::sOnRegisterApplicator (pid,
                [resolve, paramId] (float v01)
                {
                    if (auto* inst = resolve())
                        inst->applyParamNorm (paramId, v01);
                });

        if (VKnobAutomation::sOnRegisterReader)
            VKnobAutomation::sOnRegisterReader (pid,
                [resolve, paramId]() -> float
                {
                    auto* inst = resolve();
                    return inst != nullptr ? inst->readParamNorm (paramId, 0.5f) : 0.5f;
                });
    }
}

// QA-ModelShell TS4: frame a page in a contained window.
//
// The page stays owned by PageEntry::component -- the window hosts it
// non-owningly -- so the large amount of existing code that reaches through
// that pointer is untouched by the shell change.
void StandaloneEditor::hostPageInWindow (PageEntry& entry)
{
    if (mWorkspace == nullptr || entry.component == nullptr)
        return;

    // Already framed.  Several callers invoke this straight after adding a page
    // AND on tab selection, so the same tab can arrive twice; without this the
    // second call would destroy a live, positioned window and build a fresh one
    // in its place (the debug log showed it happening for two tabs).
    if (entry.window != nullptr)
    {
        entry.window->toFront (true);
        return;
    }

    // Launch opens the Builder grid and the Mixer and nothing else (Jeff,
    // 2026-07-28).  Everything else frames the first time its tab is selected --
    // showPageForTab already frames a page whose window is null, so a lazy page
    // costs one extra call at that moment and nothing before it.
    if (! mStartupComplete
        && entry.type != RibbonTabBar::TabType::Builder
        && entry.type != RibbonTabBar::TabType::Mixer)
        return;

    // Record the page index while the page still exists -- after
    // destroy-on-close there is nothing left to ask.
    if (entry.pageIndexHint < 0)
    {
        if (auto* lp = dynamic_cast<LayersPage*> (entry.component.get())) entry.pageIndexHint = lp->getPageIndex();
        else if (auto* bp = dynamic_cast<BassPage*> (entry.component.get())) entry.pageIndexHint = bp->getPageIndex();
        else if (auto* dp = dynamic_cast<DrumPage*> (entry.component.get())) entry.pageIndexHint = dp->getPageIndex();
    }

    const auto* tab = mRibbon != nullptr ? mRibbon->getTabById (entry.ribbonTabId) : nullptr;
    entry.window = std::make_unique<WorkspaceWindow> (persistKeyFor (entry),
                                                      tab != nullptr ? tab->name : juce::String());

    // Provisional floor.  The real per-window numbers are collected from Jeff
    // on screen (Test Plans B.31.0) -- they cannot be derived from source
    // because "usable" is a judgement about knob collisions, not a measurement.
    // Until then every window has SOME floor so none is unbounded.
    //
    // The Effects rack is the exception with a number behind it: TS5 made it an
    // INDEX (picker + two EQ buttons + six fixed-height rows), so its content
    // height is known and the general 640x400 would have forced a window three
    // times taller than anything it can draw.
    if (entry.type == RibbonTabBar::TabType::Effects)
        entry.window->setMinimumSize (300, 250);
    else
        entry.window->setMinimumSize (640, 400);

    entry.window->setContentNonOwned (entry.component.get());

    // Per-window key routing.  A contained window is its OWN desktop component,
    // so a key press inside it bubbles up to the window and stops -- it never
    // reaches the editor.  Without this, every global binding (transport, undo,
    // the typing keyboard) is dead in every window except the frame itself.
    // Same fix the History window already uses for the same reason.
    //
    // ORDER IS LOAD-BEARING and matches the editor's own constructor: the
    // mapping set is registered FIRST and the typing-note gate LAST, because
    // ComponentPeer::handleKeyPress iterates a component's key listeners in
    // REVERSE registration order -- so last registered outranks, and the bare
    // note letters must outrank the letter command bindings they collide with.
    if (auto* set = mCmdMgr.getKeyMappings())
        entry.window->addKeyListener (set);
    entry.window->addKeyListener (this);
    entry.window->setWantsKeyboardFocus (true);
    entry.window->onBroughtToFront = [this, id = entry.ribbonTabId]
    {
        if (mRibbon != nullptr) mRibbon->selectTab (id);
    };
    const int tabId = entry.ribbonTabId;
    entry.window->onCloseRequested = [this, tabId] { closeWindowForTab (tabId); };
    entry.window->attachTo (*mWorkspace);
}

// ── Satellite windows (QA-ModelShell TS5) ────────────────────────────────────
WorkspaceWindow* StandaloneEditor::findAuxWindow (const juce::String& key) const
{
    for (const auto& a : mAuxWindows)
        if (a.key == key) return a.window.get();
    return nullptr;
}

WorkspaceWindow* StandaloneEditor::openAuxWindow (const juce::String& key,
                                                  const juce::String& persistKey,
                                                  const juce::String& title,
                                                  std::unique_ptr<juce::Component> content,
                                                  int minW, int minH)
{
    if (auto* existing = findAuxWindow (key))
    {
        existing->toFront (true);
        return existing;
    }
    if (mWorkspace == nullptr || content == nullptr) return nullptr;

    auto win = std::make_unique<WorkspaceWindow> (persistKey, title);
    // Session-lifetime placement: these are per-strip and per-slot, which makes
    // their position project content -- writing it to the global settings.xml
    // would carry one project's effect layout into the next (Jeff's ruling).
    win->setPersistence (WorkspaceWindow::Persistence::Session);
    win->setMinimumSize (minW, minH);
    win->setContent (std::move (content));

    // Same per-window key routing the page windows get: a contained window is
    // its own desktop component, so without this every global binding is dead
    // inside it.  Order is load-bearing -- mapping set FIRST, the editor's own
    // listener LAST, because listeners dispatch in REVERSE registration order
    // and the typing-note gate has to outrank the letter commands.
    if (auto* set = mCmdMgr.getKeyMappings())
        win->addKeyListener (set);
    win->addKeyListener (this);
    win->setWantsKeyboardFocus (true);

    win->onCloseRequested = [this, key] { closeAuxWindow (key); };

    auto* raw = win.get();
    mAuxWindows.push_back ({ key, std::move (win) });
    raw->attachTo (*mWorkspace);
    return raw;
}

void StandaloneEditor::closeAuxWindow (const juce::String& key)
{
    for (size_t i = 0; i < mAuxWindows.size(); ++i)
    {
        if (mAuxWindows[i].key != key) continue;
        // Bounds are saved by the window's own close path before this runs;
        // reopening within the session lands in the same spot.
        mAuxWindows.erase (mAuxWindows.begin() + (long) i);
        return;
    }
}

void StandaloneEditor::closeAllAuxWindows()
{
    mAuxWindows.clear();
}

void StandaloneEditor::openEffectSlotWindow (int channelId, int slotIndex)
{
    auto* rack = EffectsPage::rackForChannelId (mProcessor.mVibeGraph, channelId);
    if (rack == nullptr) return;
    if (rack->getSlotType (slotIndex) == EffectType::None) return;

    const juce::String uuid = rack->getSlotUuid (slotIndex);
    if (uuid.isEmpty()) return;

    // Identity is the EFFECT (uuid): a reorder moves it between slots and the
    // window must follow it rather than the index it used to occupy.
    const juce::String key = "fx:" + juce::String (channelId) + ":" + uuid;
    // Position is remembered per SLOT, so replacing an effect reuses the window
    // spot the user already chose for that row.
    const juce::String posKey = "fxpos:" + juce::String (channelId) + ":" + juce::String (slotIndex);

    if (auto* existing = findAuxWindow (key)) { existing->toFront (true); return; }

    auto content = std::make_unique<EffectSlotWindow> (
        mProcessor, channelId, uuid,
        [this] (int chId) { return mEffectsPage != nullptr ? mEffectsPage->getChannelDisplayName (chId)
                                                           : juce::String(); },
        makeUndoContext());

    auto* contentRaw = content.get();
    contentRaw->onRequestClose = [this, key] { closeAuxWindow (key); };

    // Provisional floor.  Panels cap their knobs at a fixed size and shrink to
    // fit below that, so this is "before the knobs start colliding" rather than
    // a measured number -- Jeff's B.31.0 pass collects the real ones on screen.
    auto* win = openAuxWindow (key, posKey, contentRaw->windowTitle(),
                               std::move (content), 620, 170);
    if (win == nullptr) return;

    contentRaw->onTitleChanged = [win] (const juce::String& t) { win->setTitle (t); };
    contentRaw->configureTitleStrip (*win->getPageMenu());
}

void StandaloneEditor::openEffectEqWindow (int channelId, bool pre)
{
    const juce::String key = "eq:" + juce::String (channelId) + (pre ? ":pre" : ":post");
    if (auto* existing = findAuxWindow (key)) { existing->toFront (true); return; }

    auto content = std::make_unique<EffectEqWindow> (
        mProcessor, channelId, pre,
        [this] (int chId) { return mEffectsPage != nullptr ? mEffectsPage->getChannelDisplayName (chId)
                                                           : juce::String(); });

    auto* contentRaw = content.get();
    contentRaw->onRequestClose = [this, key] { closeAuxWindow (key); };
    contentRaw->onOpenOtherEq  = [this, channelId] (bool wantPre)
    {
        openEffectEqWindow (channelId, wantPre);
    };

    auto* win = openAuxWindow (key, key, contentRaw->windowTitle(),
                               std::move (content), 560, 320);
    if (win == nullptr) return;

    contentRaw->onTitleChanged = [win] (const juce::String& t) { win->setTitle (t); };
    contentRaw->configureTitleStrip (*win->getPageMenu());
}

// CL-044 (QA-ModelShell TS7): the floating master analyzer.  One instance -- there
// is one master bus, so a second window would draw the identical trace at twice
// the cost.  Its own key means it never collides with an effect window.
void StandaloneEditor::openMasterAnalyzerWindow()
{
    const juce::String key = "analyzer:master";
    if (auto* existing = findAuxWindow (key)) { existing->toFront (true); return; }

    auto content   = std::make_unique<MasterAnalyzerView> (mProcessor);
    auto* viewRaw  = content.get();

    // 420x220 is the point below which the log-frequency grid labels start
    // overlapping -- measured against the eight decade marks the grid draws, not
    // guessed.  Placement is session-scoped like every other satellite.
    auto* win = openAuxWindow (key, key, "Master Analyzer",
                               std::move (content), 420, 220);
    if (win == nullptr) return;

    // TS7 §3.6: the window READS capture, it does not own it -- capture has been
    // running since app start whether this window was ever opened.
    viewRaw->setVersionSource (&mVersionCapture);
    viewRaw->onExportTake = [this] (const VersionCapture::Version& v)
    {
        exportCapturedTake (v);
    };

    if (auto* bar = win->getPageMenu())
    {
        // SafePointer, not the raw pointer: the builder outlives nothing in
        // practice (the bar dies with the window that owns the view) but the
        // reverse-destruction trap that took the app down at TS6 was exactly this
        // shape, so it is not worth a raw capture.
        juce::Component::SafePointer<MasterAnalyzerView> safeView (viewRaw);
        // safeEditor as well as safeView: the Source section reads the editor's
        // VersionCapture, and the same reverse-destruction reasoning that made
        // safeView a SafePointer applies to the owner it hangs off.
        juce::Component::SafePointer<StandaloneEditor> safeEditor (this);
        bar->setMenuBuilder ([safeView, safeEditor] (juce::Component* anchor) mutable
        {
            if (safeView == nullptr || safeEditor == nullptr) return;
            using VM = MasterAnalyzerView::ViewMode;
            juce::PopupMenu m;

            m.addSectionHeader ("View");
            m.addItem ("Loudness", true, safeView->getViewMode() == VM::Loudness,
                       [safeView]() mutable
                       { if (safeView) safeView->setViewMode (VM::Loudness); });
            m.addItem ("Spectrum", true, safeView->getViewMode() == VM::Spectrum,
                       [safeView]() mutable
                       { if (safeView) safeView->setViewMode (VM::Spectrum); });

            // §2.5: source belongs on this menu.  It lists the SAME takes as the
            // in-view combo and routes through the SAME selectSource(), because
            // the previous shape -- menu calls showLive(), combo owns the take id
            // -- let the two disagree: picking Live left the combo showing a take
            // name over a live trace, and the next refresh re-applied it.
            m.addSectionHeader ("Source");
            const int curId = safeView->getSelectedTakeId();
            m.addItem ("Live", true, curId == 0,
                       [safeView]() mutable { if (safeView) safeView->selectSource (0); });
            if (safeEditor->mVersionCapture.versions().empty())
            {
                // Shown DISABLED rather than hidden so the capability is
                // discoverable before any take exists.
                m.addItem ("No captured takes yet", false, false, []{});
            }
            else
            {
                for (const auto& v : safeEditor->mVersionCapture.versions())
                {
                    const int id = v.id;
                    m.addItem (v.label, true, curId == id,
                               [safeView, id]() mutable
                               { if (safeView) safeView->selectSource (id); });
                }
            }

            m.addSectionHeader ("Target");
            // This target drives the WINDOW'S bar only -- deliberately separate
            // from the capture/export standard (exSpecId/exSpecCustom) so a view
            // tweak never silently changes take verdicts.  Custom is excluded
            // from the table loop because get(Id::Custom) is the -14 placeholder
            // LoudnessSpec.h warns about -- it gets its own prompt item below.
            bool matchesPreset = false;
            for (int i = 0; i < LoudnessSpec::numSpecs(); ++i)
            {
                const auto& sp = LoudnessSpec::get ((LoudnessSpec::Id) i);
                if (! sp.checksIntegrated) continue;   // measure-only has no target
                if (sp.id == LoudnessSpec::Id::Custom) continue;
                const float t = sp.integratedLufs;
                const bool ticked =
                    juce::approximatelyEqual (safeView->getTargetLufs(), t);
                matchesPreset = matchesPreset || ticked;
                m.addItem (juce::String (sp.name),
                           true,
                           ticked,
                           [safeView, t]() mutable
                           { if (safeView) safeView->setTargetLufs (t); });
            }

            {
                const juce::String customLabel = matchesPreset
                    ? juce::String ("Custom...")
                    : "Custom (" + juce::String (safeView->getTargetLufs(), 1)
                        + " LUFS)...";
                m.addItem (customLabel, true, ! matchesPreset,
                           [safeView]() mutable
                {
                    if (safeView == nullptr) return;
                    auto* aw = new juce::AlertWindow (
                        "Custom Target",
                        "Target loudness in LUFS (-40 to 0):",
                        juce::AlertWindow::NoIcon);
                    aw->addTextEditor ("lufs",
                                       juce::String (safeView->getTargetLufs(), 1));
                    if (auto* ed = aw->getTextEditor ("lufs"))
                        ed->setInputRestrictions (7, "-0123456789.");
                    aw->addButton ("OK",     1,
                                   juce::KeyPress (juce::KeyPress::returnKey));
                    aw->addButton ("Cancel", 0,
                                   juce::KeyPress (juce::KeyPress::escapeKey));
                    aw->enterModalState (true, juce::ModalCallbackFunction::create (
                        [safeView, aw] (int r)
                        {
                            if (r != 1 || safeView == nullptr) return;
                            const auto t = aw->getTextEditorContents ("lufs").trim();
                            if (t.isEmpty() || t == "-" || t == ".") return;
                            safeView->setTargetLufs (
                                juce::jlimit (-40.0f, 0.0f, t.getFloatValue()));
                        }), true);
                });
            }

            m.addSeparator();
            m.addItem ("Reset history",
                       [safeView]() mutable { if (safeView) safeView->resetHistory(); });

            m.showMenuAsync (juce::PopupMenu::Options()
                                 .withTargetComponent (anchor != nullptr ? anchor
                                                                         : safeView.getComponent()));
        });
    }
}

void StandaloneEditor::closeDeadEffectWindows (int channelId)
{
    auto* rack = EffectsPage::rackForChannelId (mProcessor.mVibeGraph, channelId);
    const juce::String prefix = "fx:" + juce::String (channelId) + ":";

    // Collect first, close after: closeAuxWindow mutates the vector.
    juce::StringArray doomed;
    for (const auto& a : mAuxWindows)
    {
        if (! a.key.startsWith (prefix)) continue;
        const juce::String uuid = a.key.fromFirstOccurrenceOf (prefix, false, false);

        bool stillThere = false;
        if (rack != nullptr)
            for (int i = 0; i < EffectRack::kNumSlots && ! stillThere; ++i)
                stillThere = (rack->getSlotUuid (i) == uuid);

        if (! stillThere) doomed.add (a.key);
    }

    for (const auto& k : doomed) closeAuxWindow (k);
}

juce::String StandaloneEditor::persistKeyFor (const PageEntry& entry) const
{
    // Keyed by TYPE + the page's OWN index: ribbon tab ids are handed out per
    // session, so keying on them -- which this did, against its own stated
    // rule -- lost every window's saved position on reopen.  pageIndexHint is
    // recorded before framing for exactly this key; -1 (the system pages)
    // still yields one stable key per type.
    return juce::String ((int) entry.type) + ":" + juce::String (entry.pageIndexHint);
}

// Close a window WITHOUT touching the model.  The engine keeps running and the
// tab stays in the ribbon -- reopening is a view rebuild, which is the whole
// point of destroy-on-close.
void StandaloneEditor::closeWindowForTab (int tabId)
{
    for (auto* e : mPages)
    {
        if (e->ribbonTabId != tabId) continue;

        e->window.reset();

        // PAGE DESTRUCTION IS OFF -- window-only close for now.
        //
        // CRASH FIX (Jeff, 2026-07-28): destroying the page dangles the
        // editor's CACHED RAW POINTERS into it -- mMixerPage, mBuilderPage,
        // mEffectsPage, mPianoRollPage, mLegacyLayersPage/Bass/Drum.  Closing
        // the Mixer window then left `mMixerPage` pointing at freed memory, and
        // the next `mMixerPage->removeLayerChannel(...)` during tab teardown
        // read a destroyed std::map (access violation at 0x8).  mMixerPage
        // alone is dereferenced 103 times, only a couple of them guarded, so
        // nulling it is not a fix on its own -- those call sites have to stop
        // assuming the page exists first.
        //
        // Everything else the shell needs still works: the window and its heavy
        // child components are gone, and the ENGINE is rig-owned (TS1) so audio
        // and every automation lane are untouched.  What is NOT yet claimed is
        // the full CPU dividend, which needs the page gone too.  Re-enable per
        // type as each one's cached pointers are made safe.
        return;
    }
}

// Which page types can be recreated from the model alone.  Clips / Vox / Inst /
// Rusty are absent ON PURPOSE: their construction is entangled with spawning a
// mixer strip (addVoxChannelAtIndex and friends), so a naive rebuild would
// either duplicate the strip or come back unwired.  Untangling that is its own
// job; until then those windows close view-and-frame but keep the page.
bool StandaloneEditor::canRebuildType (RibbonTabBar::TabType t)
{
    return t == RibbonTabBar::TabType::Layers
        || t == RibbonTabBar::TabType::Bass
        || t == RibbonTabBar::TabType::Drums
        || t == RibbonTabBar::TabType::Builder
        || t == RibbonTabBar::TabType::Mixer
        || t == RibbonTabBar::TabType::Effects
        || t == RibbonTabBar::TabType::PianoRoll;
}

bool StandaloneEditor::rebuildPageForTab (PageEntry& entry)
{
    if (entry.component != nullptr) return true;      // nothing to rebuild
    if (! canRebuildType (entry.type))  return false;

    using T = RibbonTabBar::TabType;
    std::unique_ptr<juce::Component> page;
    int pageIdx = -1;

    switch (entry.type)
    {
        case T::Builder:   page = createBuilderPage();   break;
        case T::Mixer:     page = createMixerPage();     break;
        case T::Effects:   page = createEffectsPage();   break;
        case T::PianoRoll: page = createPianoRollPage(); break;

        // The engine pages carry an index, and the create*AtIndex helpers
        // REFUSE an index already marked used -- which it still is, because the
        // tab never went away.  Release the claim first; the rebuild re-takes it.
        case T::Layers:
            pageIdx = entry.pageIndexHint;
            if (pageIdx >= 0 && pageIdx < kMaxLayerPages) mUsedLayerIndices[pageIdx] = false;
            page = createLayersPageAtIndex (pageIdx);
            break;
        case T::Bass:
            pageIdx = entry.pageIndexHint;
            if (pageIdx >= 0 && pageIdx < kMaxBassPages) mUsedBassIndices[pageIdx] = false;
            page = createBassPageAtIndex (pageIdx);
            break;
        case T::Drums:
            pageIdx = entry.pageIndexHint;
            if (pageIdx >= 0 && pageIdx < kMaxDrumPages) mUsedDrumIndices[pageIdx] = false;
            page = createDrumPageAtIndex (pageIdx);
            break;
        default: return false;
    }

    if (page == nullptr) return false;
    entry.component = std::move (page);

    // RE-BIND to the engine the rig still owns.  A fresh page has no engine
    // view (that is built in selectEngine, not the constructor), so without
    // this the window would reopen blank over a perfectly live engine.
    // selectEngine is safe to call here: EngineRig::setEngineType no-ops and
    // returns the EXISTING engine when the type already matches, so this
    // rebuilds the editor view without touching audio.
    if (pageIdx >= 0)
    {
        const TabKind k = entry.type == T::Layers ? TabKind::Layers
                        : entry.type == T::Bass   ? TabKind::Bass
                                                  : TabKind::Drums;
        if (auto* tab = mProcessor.engineRig().findTab (k, pageIdx))
            if (tab->engineType.isNotEmpty())
            {
                if (auto* lp = dynamic_cast<LayersPage*> (entry.component.get())) lp->selectEngine (tab->engineType);
                else if (auto* bp = dynamic_cast<BassPage*>  (entry.component.get())) bp->selectEngine (tab->engineType);
                else if (auto* dp = dynamic_cast<DrumPage*>  (entry.component.get())) dp->selectEngine (tab->engineType);
            }
    }

    hostPageInWindow (entry);
    return true;
}

// Select `engine` on the most recently created page of `type`.  Used by the
// "+" menu, where the user picked an engine and the tab was created to hold it.
// No-op for Clip (its engine is implicit) and for pages that never appeared --
// onAddTabRequest can legitimately decline (cap reached, picker cancelled).
void StandaloneEditor::applyEngineToNewestTabOfType (RibbonTabBar::TabType type,
                                                     const juce::String& engine)
{
    if (engine.isEmpty()) return;

    PageEntry* newest = nullptr;
    for (auto* e : mPages)
        if (e != nullptr && e->type == type)
            newest = e;                       // mPages is append-ordered
    if (newest == nullptr || newest->component == nullptr) return;

    if      (auto* lp = dynamic_cast<LayersPage*> (newest->component.get())) lp->selectEngine (engine);
    else if (auto* bp = dynamic_cast<BassPage*>   (newest->component.get())) bp->selectEngine (engine);
    else if (auto* dp = dynamic_cast<DrumPage*>   (newest->component.get())) dp->selectEngine (engine);
    // QA-ModelShell TS6 (BLU-447): for a plugin tab `engine` is the plugin's
    // identifier string, which is exactly what the "+" dropdown put in the
    // AddChoice -- so no separate route was needed.
    else if (auto* pp = dynamic_cast<PluginsPage*> (newest->component.get())) pp->selectPluginById (engine);
}

void StandaloneEditor::registerStaticAutomationHandlers()
{
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

            // QA-ModelShell TS3: the mixer fader lane is spelled
            // "<prefix>_fader" but its parameter is "<prefix>_level" -- the lane
            // id predates the param.  Because no APVTS param answered to the
            // lane's own name, mixer faders were the one strip control that
            // needed a view-scoped applicator, plus a re-registration shim to
            // put it back after every project boundary wiped the map.  Deriving
            // the alias from the param itself retires both, and saved lanes keep
            // their spelling so nothing on disk changes.
            if (pid.startsWith ("mixer_") && pid.endsWith ("_level"))
            {
                const juce::String faderId =
                    pid.upToLastOccurrenceOf ("_level", false, false) + "_fader";
                mAutomationApplicators[faderId] = [rap] (float v01)
                {
                    rap->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v01));
                };
                mAutomationValueReaders[faderId] = [rap]() -> float { return rap->getValue(); };
            }
        }
    }

    // "global_tempo" automation.  Linear map 0..1 <-> 20..300 BPM.
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
        // Automation is a LIVE override (truncate-and-append tail segment;
        // ruler tempo flags ahead re-assert = last-writer-wins).  It does not
        // write the persisted BASE tempo - the BPM field owns that (Jeff's E
        // pick); pre-map code wrote setGlobalTempo here, which would corrupt
        // the base with transient automation values.
        mPlayHead.setLiveTempo (bpm);
    };
    mAutomationValueReaders["global_tempo"] = [this, kTempoMinBpm, kTempoMaxBpm]() -> float
    {
        const double bpm = mPlayHead.getBPM();
        return juce::jlimit (0.0f, 1.0f,
            (float) ((bpm - kTempoMinBpm) / (kTempoMaxBpm - kTempoMinBpm)));
    };
}

// QA-ModelShell TS3 (2026-07-27): trackAutomationOwner + componentBeingDeleted
// are gone, and StandaloneEditor is no longer a juce::ComponentListener.
//
// They were the answer to a real problem in the widget-targeting era: a lane's
// applicator drove a control, so the registry had to notice when that control
// died or it would keep a live entry pointing at freed memory.  The machinery
// grew two hard-won guards on top of that -- clear the id's owner claim on a
// null-owner registration, and revoke on destruction only what the dying
// component still owns (a rebuilt panel registers BEFORE the old one dies).
//
// Both guards existed because view lifetime and lane lifetime were tangled.
// TS3 untangles them: every registration is model-side and re-resolves its
// target through the model at apply time, so no registration can outlive its
// target in a way that matters, and there is no component whose death should
// revoke a lane.  A view-lifetime index over a set that is now always empty is
// worse than nothing -- it would quietly half-work for anyone who re-added a
// view-scoped registration.

void StandaloneEditor::advanceCountersFromRestoredTabs()
{
    // After deserializeUIState rebuilds all tabs from saved XML, scan the
    // current ribbon (via mPages) and parse the trailing numeric suffix from
    // each tab's display name.  Advance each counter to max(found) + 1 so a
    // subsequent +Add doesn't collide with a restored tab number.
    //
    // Tabs whose saved name was user-renamed (e.g. "MyBass") contribute no
    // suffix and don't move the counter; the counter advances only when a
    // recognisable "Prefix N" pattern is found.
    auto parseTail = [] (const juce::String& name, const juce::String& prefix) -> int
    {
        if (! name.startsWith (prefix)) return 0;
        const auto tail = name.substring (prefix.length()).trim();
        return tail.containsOnly ("0123456789") && tail.isNotEmpty()
                   ? tail.getIntValue()
                   : 0;
    };

    int maxLayer = 0, maxBass = 0, maxDrum = 0, maxVox = 0, maxInst = 0;
    int maxGuitar = 0, maxBasses = 0, maxClip = 0, maxPlugin = 0;

    if (mRibbon != nullptr)
    {
        for (auto* entry : mPages)
        {
            if (! entry) continue;
            const auto* tab = mRibbon->getTabById (entry->ribbonTabId);
            if (tab == nullptr) continue;
            const auto& nm = tab->name;

            switch (entry->type)
            {
                case RibbonTabBar::TabType::Layers:
                    maxLayer = juce::jmax (maxLayer, parseTail (nm, "Layer "));
                    // QA-E Task 8 NIT-3 (QA-D carry-forward): pre-QA-D saves
                    // used bare "Layers"/"Bass"/"Drums" (no " N").  Count the
                    // bare name as instance #1 so the next +Add becomes
                    // "Layer 2" (not "Layer 1") via the maxLayer+1 line below.
                    if (nm == "Layers") maxLayer = juce::jmax (maxLayer, 1);
                    break;
                case RibbonTabBar::TabType::Bass:
                    maxBass = juce::jmax (maxBass, parseTail (nm, "Bass "));
                    if (nm == "Bass") maxBass = juce::jmax (maxBass, 1);   // NIT-3
                    break;
                case RibbonTabBar::TabType::Drums:
                    maxDrum = juce::jmax (maxDrum, parseTail (nm, "Drum "));
                    if (nm == "Drums") maxDrum = juce::jmax (maxDrum, 1);  // NIT-3
                    break;
                case RibbonTabBar::TabType::Vox:
                    maxVox = juce::jmax (maxVox, parseTail (nm, "Vox "));
                    break;
                case RibbonTabBar::TabType::Inst:
                    // Inst tabs may carry "Inst N" (LiveInput), "Guitar N"
                    // (BaySickGuitars), or "Basses N" (BaySickBasses).  Try
                    // each prefix.
                    maxInst   = juce::jmax (maxInst,   parseTail (nm, "Inst "));
                    maxGuitar = juce::jmax (maxGuitar, parseTail (nm, "Guitar "));
                    maxBasses = juce::jmax (maxBasses, parseTail (nm, "Basses "));
                    break;
                case RibbonTabBar::TabType::Clip:
                    maxClip = juce::jmax (maxClip, parseTail (nm, "Clip "));
                    break;
                case RibbonTabBar::TabType::Plugins:
                    // Missing from the re-seed entirely: after a project load
                    // the counter restarted at 1 and the next add duplicated
                    // an existing "Plugin N" name.
                    maxPlugin = juce::jmax (maxPlugin, parseTail (nm, "Plugin "));
                    break;
                default:
                    break;
            }
        }
    }

    mNextLayerNameNum   = juce::jmax (mNextLayerNameNum,   maxLayer  + 1);
    mNextBassNameNum    = juce::jmax (mNextBassNameNum,    maxBass   + 1);
    mNextDrumNameNum    = juce::jmax (mNextDrumNameNum,    maxDrum   + 1);
    mNextVoxNameNum     = juce::jmax (mNextVoxNameNum,     maxVox    + 1);
    mNextInstNameNum    = juce::jmax (mNextInstNameNum,    maxInst   + 1);
    mNextGuitarNameNum  = juce::jmax (mNextGuitarNameNum,  maxGuitar + 1);
    mNextBassesNameNum  = juce::jmax (mNextBassesNameNum,  maxBasses + 1);
    mNextClipNameNum    = juce::jmax (mNextClipNameNum,    maxClip   + 1);
    mNextPluginNameNum  = juce::jmax (mNextPluginNameNum,  maxPlugin + 1);
}


void StandaloneEditor::deserializeUIState (const juce::XmlElement& root)
{
    auto* ui = root.getChildByName ("UIState");
    if (ui == nullptr) return;
    auto* tabs = ui->getChildByName ("Tabs");
    if (tabs == nullptr) return;

    mHeavyOpOverlay.setStepLabel ("Closing old tabs...");
    closeAllDynamicTabs();

    auto applyEngineState = [](juce::AudioProcessor* eng, const juce::String& base64)
    {
        if (eng == nullptr || base64.isEmpty()) return;
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (base64))
            eng->setStateInformation (mb.getData(), (int) mb.getSize());
    };

    int tabTotal = 0;
    for (auto* rec : tabs->getChildWithTagNameIterator ("Tab"))
    {
        juce::ignoreUnused (rec);
        ++tabTotal;
    }
    int tabNum = 0;

    for (auto* rec : tabs->getChildWithTagNameIterator ("Tab"))
    {
        const juce::String type      = rec->getStringAttribute ("type");
        const int          pageIndex = rec->getIntAttribute    ("pageIndex", 0);
        const juce::String name      = rec->getStringAttribute ("name");
        const juce::String engine    = rec->getStringAttribute ("engine");
        const juce::String engineData= rec->getStringAttribute ("engineData");

        // TS7 §6.8: remember this tab's freeze for restorePendingFreezes, which
        // runs once the whole graph exists.  Applying it here would render a tab
        // whose strip and rack have not been built yet.
        if (rec->getIntAttribute ("frozen", 0) != 0)
        {
            const bool byUser = (rec->getStringAttribute ("frozenBy", "manual") != "auto");

            EngineTab::FreezeSpan span;
            span.patternIndex = rec->getIntAttribute    ("freezeScope", EngineTab::kSongScope);
            span.lengthBeats  = rec->getDoubleAttribute ("freezeBeats", 0.0);
            span.bpm          = rec->getDoubleAttribute ("freezeBpm",   0.0);
            span.contentStamp = (juce::uint32)
                rec->getStringAttribute ("freezeStamp", "0").getLargeIntValue();

            std::map<int, juce::uint32> patStamps;
            for (auto* ps : rec->getChildWithTagNameIterator ("FreezePattern"))
                patStamps[ps->getIntAttribute ("index", -1)] = (juce::uint32)
                    ps->getStringAttribute ("stamp", "0").getLargeIntValue();

            // All eight kinds, matching the save side -- which means matching
            // the RECORD types: "Drum" (singular) and "BaySickRustyDrums", not
            // the TabKind names.  The "Drums"/"Rusty" spellings matched nothing
            // and silently dropped both freezes on every load.
            TabKind k    = TabKind::Layers;
            bool    okay = true;
            if      (type == "Layers")            k = TabKind::Layers;
            else if (type == "Bass")              k = TabKind::Bass;
            else if (type == "Drum")              k = TabKind::Drums;
            else if (type == "Plugins")           k = TabKind::Plugins;
            else if (type == "Clips")             k = TabKind::Clips;
            else if (type == "Vox")               k = TabKind::Vox;
            else if (type == "Inst")              k = TabKind::Inst;
            else if (type == "BaySickRustyDrums") k = TabKind::Rusty;
            else                                  okay = false;

            if (okay)
            {
                PendingFreeze pf { k, pageIndex, byUser, span, std::move (patStamps) };
                pf.stale = rec->getIntAttribute ("freezeStale", 0) != 0;
                mPendingFreezes.push_back (std::move (pf));
            }
        }

        ++tabNum;
        mHeavyOpOverlay.setStep (tabNum, tabTotal,
            "Tab " + juce::String (tabNum) + " of " + juce::String (tabTotal)
            + " - " + (name.isNotEmpty() ? name : type));

        // QA-ModelShell TS6 (BLU-447): hosted VST3 instrument tab.  Restore
        // order matches its siblings -- page, ribbon tab, hooks, roll, THEN
        // selectPluginById (which is what constructs the plugin through the
        // rig), then the state blob onto the engine that call created.
        if (type == "Plugins")
        {
            auto page = createPluginsPageAtIndex (pageIndex);
            if (! page) continue;
            auto* pp = dynamic_cast<PluginsPage*> (page.get());
            const int newId = mRibbon->addTab (RibbonTabBar::TabType::Plugins,
                                                name.isNotEmpty() ? name : "Plugin");
            if (pp && name.isNotEmpty()) pp->setTabName (name);

            if (pp)
            {
                pp->onEngineSelected = [this, newId, pageIndex, pp] {
                    const auto* tab = mRibbon->getTabById (newId);
                    if (mMixerPage)   mMixerPage->addPluginChannel (pageIndex, tab ? tab->name : "Plugin");
                    if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
                    if (mPianoRollPage)
                        mPianoRollPage->setEngineType ({ EngineKind::Plugin, pageIndex }, pp->getPluginName());
                };
                pp->onPluginChanged = [this, pageIndex, pp] {
                    if (mPianoRollPage)
                        mPianoRollPage->setEngineType ({ EngineKind::Plugin, pageIndex }, pp->getPluginName());
                };
                registerPluginPianoRoll (pageIndex, pp);

                // Restore principle (HostedPlugin.h): the blob carries the
                // FULL description so the project keeps loading its plugin
                // after the user removed it from the added list.  Stashed
                // BEFORE the select so the rig factory can fall back to it.
                if (engine.isNotEmpty() && engineData.isNotEmpty())
                {
                    juce::MemoryBlock mb;
                    if (mb.fromBase64Encoding (engineData) && mb.getSize() > 0)
                        mProcessor.engineRig().stashPluginRestoreDescription (
                            pageIndex,
                            Hosting::HostedPluginInstance::descriptionFromState (
                                mb.getData(), (int) mb.getSize()));
                }

                if (engine.isNotEmpty())
                    pp->selectPluginById (engine);

                applyEngineState (pp->getEngineProcessor(), engineData);

                // The hooks above stay QUIET about names so the restore-time
                // select can't stomp the saved tab name.  Live picks and
                // program changes from here on follow the display-name
                // linkage, same as a fresh tab (the page's poll seeds its
                // first-learned name quietly for the same reason).
                pp->onEngineSelected = [this, newId, pageIndex, pp] {
                    const juce::String nm = pp->getDisplayName();
                    if (nm.isNotEmpty() && mRibbon) { mRibbon->renameTab (newId, nm); pp->setTabName (nm); }
                    const auto* tab = mRibbon->getTabById (newId);
                    if (mMixerPage)   mMixerPage->addPluginChannel (pageIndex, tab ? tab->name : "Plugin");
                    if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
                    if (mPianoRollPage)
                        mPianoRollPage->setEngineType ({ EngineKind::Plugin, pageIndex }, pp->getDisplayName());
                };
                pp->onPluginChanged = [this, newId, pageIndex, pp] {
                    const juce::String nm = pp->getDisplayName();
                    if (nm.isNotEmpty())
                    {
                        if (mRibbon) { mRibbon->renameTab (newId, nm); pp->setTabName (nm); }
                        if (mMixerPage) mMixerPage->renameChannel (MixerPage::StripKind::Plugin, pageIndex, nm);
                    }
                    if (mPianoRollPage)
                        mPianoRollPage->setEngineType ({ EngineKind::Plugin, pageIndex }, pp->getDisplayName());
                    if (mProjectManager) mProjectManager->markDirty();
                };
                pp->onDeleteRequested = [this, newId] {
                    if (! mRibbon) return;
                    mRibbon->closeTab (newId);
                };
                // Instrument lanes (ruling 2-b) -- see the spawn path.  The
                // param-count watch is what makes a restored project's plugin
                // lanes resolve: the bridged list lands after this block runs.
                pp->onParamListChanged = [this, pageIndex] { registerPluginTabAutomation (pageIndex); };
                pp->onAutomateParam = [this, pageIndex] (const juce::String& paramId) {
                    registerPluginTabAutomation (pageIndex);
                    openEventEditorForParam ("plugtab" + juce::String (pageIndex) + "_vst_" + paramId);
                };
                registerPluginTabAutomation (pageIndex);   // in-process params exist NOW
            }

            auto* entry = new PageEntry();
            entry->ribbonTabId = newId;
            entry->type        = RibbonTabBar::TabType::Plugins;
            entry->component   = std::move (page);
            mPages.add (entry);
            hostPageInWindow (*entry);
        }
        else if (type == "Layers")
        {
            auto page = createLayersPageAtIndex (pageIndex);
            if (! page) continue;
            auto* lp = dynamic_cast<LayersPage*> (page.get());
            const int newId = mRibbon->addTab (RibbonTabBar::TabType::Layers,
                                                name.isNotEmpty() ? name : "Layers");
            if (lp && name.isNotEmpty()) lp->setTabName (name);
            lp->onEngineSelected = [this, newId, pageIndex, lp] {
                const auto* tab = mRibbon->getTabById (newId);
                if (mMixerPage)   mMixerPage->addLayerChannel (pageIndex, tab ? tab->name : "Layers");
                if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
                wireEngineDirtyHook (lp->getEngineProcessor());
                // QA-D STATE-02 follow-on: piano-roll context label.
                if (mPianoRollPage)
                    mPianoRollPage->setEngineType ({ EngineKind::Layer, pageIndex }, lp->getEngineType());
            };
            lp->onDeleteRequested = [this, newId] {
                if (! mRibbon) return;
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
                if (mMixerPage) mMixerPage->renameChannel (MixerPage::StripKind::Layer, pageIndex, nm);
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
            mPages.add (entry);
            hostPageInWindow (*entry);
        }
        else if (type == "Bass")
        {
            auto page = createBassPageAtIndex (pageIndex);
            if (! page) continue;
            auto* bp = dynamic_cast<BassPage*> (page.get());
            const int newId = mRibbon->addTab (RibbonTabBar::TabType::Bass,
                                                name.isNotEmpty() ? name : "Bass");
            if (bp && name.isNotEmpty()) bp->setTabName (name);
            bp->onEngineSelected = [this, newId, pageIndex, bp] {
                const auto* tab = mRibbon->getTabById (newId);
                if (mMixerPage)   mMixerPage->addBassChannel (pageIndex, tab ? tab->name : "Bass");
                if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
                wireEngineDirtyHook (bp->getEngineProcessor());
                // QA-D STATE-02 follow-on: piano-roll context label.
                if (mPianoRollPage)
                    mPianoRollPage->setEngineType ({ EngineKind::Bass, pageIndex }, bp->getEngineType());
            };
            bp->onDeleteRequested = [this, newId] {
                if (! mRibbon) return;
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
                if (mMixerPage) mMixerPage->renameChannel (MixerPage::StripKind::Bass, pageIndex, nm);
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
            mPages.add (entry);
            hostPageInWindow (*entry);
        }
        else if (type == "Drums")
        {
            // 2026-04-25: legacy 16-slot DrumsPage removed.  Old project files
            // that saved a "Drums" tab are silently skipped - the user can
            // re-add drums via the Drums dropdown ▾ in the new model.  Notes
            // from the legacy single drumRoll were already migrated into
            // drumRolls[slot] by PatternManager::fromValueTree (D1.1).
            continue;
        }
        // 2026-04-30 (audit C6): Clips / Vox / Inst tab restore.  Each calls
        // the existing spawn-helper to recreate the page+ribbon+callbacks,
        // then looks up the spawned page in mPages and applies the
        // serialized engine state on top.  Mirror of Layer/Bass/Drum.
        else if (type == "Clips")
        {
            juce::String clipPath = rec->getStringAttribute ("clipPath");
            // QA-ClipDrop Task 3 dup-fix (2026-06-03): the saved clipPath is the
            // absolute engine path, but the audio-library entry stores the
            // project-relative path ("Samples/<file>").  Hand the Clips page the
            // RELATIVE library path so its library re-tag (ClipsPage::setClipFilePath
            // -> addAudioToLibrary) dedups against the already-deserialized entry by
            // exact string -- otherwise the absolute tag never matches and a
            // DUPLICATE browser entry appears on every reload.  Both forms point at
            // the same Samples-folder copy and the engine resolves the relative form
            // back to absolute on load, so playback is unchanged.  Match the library
            // entry for this page's owner channel (resolved-path equality so
            // multi-file pages pick the right clip; first-for-owner fallback; saved
            // attribute if the library has no entry).  Backward-compatible -- uses
            // whatever the library stored, so older absolute-library projects match.
            if (mPM != nullptr && mPM->getNumAudioLibrary() > 0)
            {
                const int        ownerCh  = MixerChannelIds::audioInsert (pageIndex);
                const juce::File savedAbs = mProcessor.resolveProjectFile (clipPath);
                int match = -1, firstForOwner = -1;
                for (int i = 0; i < mPM->getNumAudioLibrary(); ++i)
                {
                    if (mPM->getAudioLibraryPageOwner (i) != ownerCh) continue;
                    if (firstForOwner < 0) firstForOwner = i;
                    if (mProcessor.resolveProjectFile (mPM->getAudioLibraryPath (i)) == savedAbs)
                        { match = i; break; }
                }
                const int use = (match >= 0) ? match : firstForOwner;
                if (use >= 0) clipPath = mPM->getAudioLibraryPath (use);
            }
            // QA-ClipDrop Task 3 (SC-G/H reload, 2026-06-03): create the mixer
            // strip alongside the page here -- mirror of the Vox/Inst restore
            // branches below, which call addVoxChannelAtIndex / addInstChannelAtIndex
            // because their strips are not block-driven either.  A no-block
            // "+ Add New Clip" clip has no arrangement block, so the block-driven
            // restoreAudioStripsFromArrangement would never make its strip.
            // createClipStripAndPage's strip trio is idempotent, and
            // restoreAudioStripsFromArrangement is now owner-keyed (below), so a
            // block-backed clip lands on the SAME row -> exactly one strip.
            // QA-ProjectSave Task 4 (2026-07-26): a clip whose file no longer
            // resolves used to restore as a silent, empty player -- the tab and
            // strip appeared, nothing played, and nothing said why.  Collect it
            // for the one-shot report deserializeProject raises at the end of
            // the load (shared with QA-Export's engine-side reporting), rather
            // than a dialog per clip.
            if (clipPath.isNotEmpty()
                && ! mProcessor.resolveProjectFile (clipPath).existsAsFile())
            {
                MissingFileReport::add ("Clip audio", clipPath);
            }
            createClipStripAndPage (pageIndex, clipPath);
            for (auto* entry : mPages)
            {
                if (! entry) continue;
                if (entry->type != RibbonTabBar::TabType::Clip) continue;
                auto* cp = dynamic_cast<ClipsPage*> (entry->component.get());
                if (cp == nullptr || cp->getPageIndex() != pageIndex) continue;
                if (name.isNotEmpty())
                {
                    cp->setTabName (name);
                    if (mRibbon) mRibbon->renameTab (entry->ribbonTabId, name);
                    // SC-H: the saved Clips tab name wins on reload -> push it to
                    // the mixer strip (createClipStripAndPage seeded the strip
                    // from the filename a moment ago).
                    if (mMixerPage) mMixerPage->renameChannel (MixerPage::StripKind::Audio, pageIndex, name);
                }
                if (engine.isNotEmpty())
                    cp->selectEngine ((ClipsPage::EngineType) engine.getIntValue());
                applyEngineState (cp->getEngineProcessor(), engineData);
                break;
            }
        }
        else if (type == "Vox")
        {
            spawnVoxTabIfMissing (pageIndex, /*selectAfter*/ false);

            // QA-E MIX-02 (2026-05-11): parallel K-6 fix for Vox -- ensure the
            // mixer strip exists at pageIndex.  restoreStripNames only creates
            // strips that have a non-default name (legacy "persist renames
            // only" semantics) -- unrenamed Vox tabs get their strip dropped
            // on save/load, leaving the VoxPage with no audio path to the
            // bus.  addVoxChannelAtIndex is idempotent (early-returns when
            // the strip already exists), so calling it here is safe even
            // when restoreStripNames already ran.  Mirror of the K-6
            // follow-up landed for Inst on 2026-05-05.
            if (mMixerPage) mMixerPage->addVoxChannelAtIndex (pageIndex);

            for (auto* entry : mPages)
            {
                if (! entry) continue;
                if (entry->type != RibbonTabBar::TabType::Vox) continue;
                auto* vp = dynamic_cast<VoxPage*> (entry->component.get());
                if (vp == nullptr || vp->getPageIndex() != pageIndex) continue;
                if (name.isNotEmpty())
                {
                    vp->setTabName (name);
                    if (mRibbon) mRibbon->renameTab (entry->ribbonTabId, name);
                }
                if (engine.isNotEmpty())
                    vp->selectEngine ((VoxPage::EngineType) engine.getIntValue());
                applyEngineState (vp->getEngineProcessor(), engineData);
                break;
            }
        }
        else if (type == "Inst")
        {
            // K-6 (2026-05-05): branch on the saved source attribute.  Live-input
            // path (existing) when source is missing or "LiveInput".  sfizz-source
            // (BaySickGuitars / BaySickBasses) takes a separate restore flow that
            // spawns the page, switches source, loads the saved kit via the
            // race-safe wrapper, then overlays the saved engine state + cache.
            const juce::String source = rec->getStringAttribute ("source");
            const bool sfizzSource = (source == "BaySickGuitars" || source == "BaySickBasses");

            spawnInstTabIfMissing (pageIndex, /*selectAfter*/ false);

            // K-6 follow-up (2026-05-05): ensure the mixer strip exists at
            // pageIndex.  restoreStripNames only creates strips that have a
            // non-default name (legacy "persist renames only" semantics) -
            // BaySickGuitars / live-input Inst tabs that the user never
            // renamed get their strip dropped on save/load, leaving the
            // InstPage with no audio path to the bus.  addInstChannelAtIndex
            // is idempotent (early-returns when the strip already exists), so
            // calling it here is safe even when restoreStripNames already ran.
            if (mMixerPage) mMixerPage->addInstChannelAtIndex (pageIndex);

            for (auto* entry : mPages)
            {
                if (! entry) continue;
                if (entry->type != RibbonTabBar::TabType::Inst) continue;
                auto* ip = dynamic_cast<InstPage*> (entry->component.get());
                if (ip == nullptr || ip->getPageIndex() != pageIndex) continue;
                if (name.isNotEmpty())
                {
                    ip->setTabName (name);
                    if (mRibbon) mRibbon->renameTab (entry->ribbonTabId, name);
                }

                // K-6 follow-up (2026-05-05): restore BaySickPedals + BaySickNAM/IR
                // state via importInstState - applies to live-input AND sfizz
                // Inst tabs.  Without this, Pedals + NAM/IR settings reset to
                // defaults on every project load.
                {
                    const juce::String chainXml = rec->getStringAttribute ("instChainState");
                    if (chainXml.isNotEmpty())
                        ip->importInstState (chainXml);
                }

                if (sfizzSource
                    && (source == "BaySickGuitars" || source == "BaySickBasses"))
                {
                    const bool isGuitars = source == "BaySickGuitars";
                    const auto sourceMode = isGuitars
                                              ? InstPage::Source::BaySickGuitars
                                              : InstPage::Source::BaySickBasses;
                    const juce::String engineRootTag = isGuitars
                                                          ? "BaySickGuitarsState"
                                                          : "BaySickBassesState";

                    // 1) Switch source first so chain rebuild + Player tab + Piano
                    //    Roll sub-tab + program button all wire up correctly.
                    //    Engine doesn't exist yet - chain will fall through to
                    //    Pedals+NAMIR until step 3 creates it.
                    ip->setSource (sourceMode);

                    // 2) Hide arm/listen LEDs on the strip (sfizz IS the source).
                    if (mMixerPage) mMixerPage->setInstStripNoLiveInput (pageIndex, true);

                    // 3) Race-safe load of the saved kit (creates engine slot,
                    //    flips processing-enabled false → loadKit → true).
                    //    Falls back to that source's default kit if the saved
                    //    path is gone.
                    juce::File savedKit = SampleLibrary::resolvePersistedRef (rec->getStringAttribute ("kitPath"));
                    if (! savedKit.existsAsFile())
                    {
                        savedKit = SampleLibrary::getCoreLibraryDir();
                        savedKit = isGuitars
                            ? savedKit.getChildFile ("Black&Green Guitars")
                                       .getChildFile ("Programs")
                                       .getChildFile ("01-green_keyswitch.sfz")
                            : savedKit.getChildFile ("Black&Blue Basses")
                                       .getChildFile ("Programs")
                                       .getChildFile ("01-darkblack_keysw.sfz");
                    }
                    if (isGuitars)
                        mProcessor.loadBaySickGuitarsKit (pageIndex, savedKit);
                    else
                        mProcessor.loadBaySickBassesKit  (pageIndex, savedKit);

                    // 2026-05-05 dirty-flag wiring on the just-created engine.
                    wireEngineDirtyHook (isGuitars
                                         ? (juce::AudioProcessor*) mProcessor.getBaySickGuitars (pageIndex)
                                         : (juce::AudioProcessor*) mProcessor.getBaySickBasses  (pageIndex));

                    // 4) Apply saved engine state (APVTS).  J-9 race fix:
                    //    setStateInformation re-runs loadKit WITHOUT the active-
                    //    flag protection, so we decode the blob and replaceState
                    //    directly on the apvts instead.
                    const juce::String sfizzData = rec->getStringAttribute ("sfizzEngineData");
                    if (sfizzData.isNotEmpty())
                    {
                        juce::AudioProcessor* eng = isGuitars
                            ? (juce::AudioProcessor*) mProcessor.getBaySickGuitars (pageIndex)
                            : (juce::AudioProcessor*) mProcessor.getBaySickBasses  (pageIndex);
                        juce::AudioProcessorValueTreeState* apv = nullptr;
                        if (isGuitars)
                        {
                            if (auto* g = mProcessor.getBaySickGuitars (pageIndex)) apv = &g->apvts;
                        }
                        else
                        {
                            if (auto* b = mProcessor.getBaySickBasses (pageIndex))  apv = &b->apvts;
                        }
                        if (eng != nullptr && apv != nullptr)
                        {
                            juce::MemoryBlock mb;
                            if (mb.fromBase64Encoding (sfizzData))
                            {
                                if (auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()))
                                {
                                    if (xml->hasTagName (engineRootTag))
                                    {
                                        auto root2 = juce::ValueTree::fromXml (*xml);
                                        if (auto apvtsState = root2.getChildWithName (
                                                apv->state.getType()); apvtsState.isValid())
                                        {
                                            apv->replaceState (apvtsState);
                                            for (auto* p : eng->getParameters())
                                                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                                                    ranged->setValueNotifyingHost (ranged->getValue());
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // 5) Restore the per-program state cache.
                    if (auto* cacheXml = rec->getChildByName ("ProgramStateCache"))
                        ip->restoreProgramCacheFromXml (*cacheXml);

                    // 6) Register with PianoRollPage now that source + engine
                    //    are both live.
                    registerInstSourcePianoRoll (ip);

                    // 7) Refresh chain (engine pointer now non-null) + Player
                    //    panel (re-binds widgets to the restored APVTS state).
                    ip->notifySourceEngineChanged();
                }
                else
                {
                    // Live-input Inst (existing path).
                    if (engine.isNotEmpty())
                        ip->selectEngine ((InstPage::EngineType) engine.getIntValue());
                    applyEngineState (ip->getEngineProcessor(), engineData);
                }
                break;
            }
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
                dp2->onEngineSelected = [this, newId, pageIndex, dp2] {
                    const auto* tab = mRibbon->getTabById (newId);
                    if (mMixerPage)   mMixerPage->addDrumChannel (pageIndex, tab ? tab->name : "Drums");
                    if (mEffectsPage) mEffectsPage->rebuildChannelDropdown();
                    refreshAllKitViews();
                    wireEngineDirtyHook (dp2->getEngineProcessor());
                    // QA-D STATE-02 follow-on: piano-roll context label.
                    if (mPianoRollPage)
                        mPianoRollPage->setEngineType ({ EngineKind::Drum, pageIndex }, dp2->getEngineType());
                };
                dp2->onSoundNameChanged = [this, newId, pageIndex, dp2] (const juce::String& nm) {
                    if (nm.isEmpty()) return;
                    if (mRibbon)    mRibbon->renameTab (newId, nm);
                    if (mMixerPage) mMixerPage->renameChannel (MixerPage::StripKind::Drum, pageIndex, nm);
                    dp2->setTabName (nm);
                    refreshAllKitViews();
                    if (mPianoRollPage) mPianoRollPage->setEngineDisplayName ({ EngineKind::Drum, pageIndex }, nm);
                };
                dp2->onDeleteRequested = [this, newId] {
                    if (! mRibbon) return;
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
            mPages.add (entry);
            hostPageInWindow (*entry);
        }
        else if (type == "BaySickRustyDrums")
        {
            // J-9 (2026-05-05): BaySickRustyDrums singleton restore.
            //
            // Safe-load order:
            //   1. addBaySickRustyDrumsTab spawns the ribbon tab + page object
            //      + PianoRoll registry hooks.
            //   2. Decode saved engine state to recover the kit path.
            //   3. page->reloadForProjectRestore(kitPath) - this is the path
            //      that:
            //         (a) loads the kit through the active-flag-protected
            //             wrapper (no sfizz race),
            //         (b) fires onKitLoaded so the mixer strips spawn,
            //         (c) syncs mCurrentProgram + mProgramCombo so a later
            //             re-pick of the same program is a no-op (otherwise
            //             the kit's set_cc directives stomp saved CC values),
            //         (d) renders the ARIA control panel for the program.
            //   4. apvts.replaceState applies the saved CC values on top of
            //      the just-written kit defaults - saved values WIN.
            //
            // Calling engine->setStateInformation directly would re-run
            // loadKit WITHOUT the active-flag dance - that's the crash path:
            // sfizz's hash maps get mutated while renderBlock is walking them.
            addBaySickRustyDrumsTab();
            if (engineData.isNotEmpty())
            {
                juce::MemoryBlock mb;
                if (mb.fromBase64Encoding (engineData))
                {
                    if (auto kitXml = juce::AudioProcessor::getXmlFromBinary (
                            mb.getData(), (int) mb.getSize()))
                    {
                        if (kitXml->hasTagName ("BaySickRustyDrumsState"))
                        {
                            juce::File kitFile;
                            if (auto* kp = kitXml->getChildByName ("KitPath"))
                                kitFile = juce::File (kp->getStringAttribute ("path"));

                            // Find the just-spawned BaySickRustyDrumsPage and
                            // route through its restore method (handles UI
                            // state + ARIA panel + mixer-strip spawn together).
                            BaySickRustyDrumsPage* rustyPage = nullptr;
                            for (auto* entry : mPages)
                                if (auto* rp = dynamic_cast<BaySickRustyDrumsPage*> (entry->component.get()))
                                { rustyPage = rp; break; }

                            if (rustyPage != nullptr
                                && kitFile.existsAsFile()
                                && rustyPage->reloadForProjectRestore (kitFile))
                            {
                                // Saved CC values overlay the kit's just-written
                                // set_cc defaults.  Sliders update via the
                                // SliderParameterAttachment listener chain.
                                if (auto* eng = mProcessor.getBaySickRustyDrums())
                                {
                                    for (auto* child : kitXml->getChildIterator())
                                    {
                                        auto state = juce::ValueTree::fromXml (*child);
                                        if (state.isValid()
                                            && state.getType() == eng->apvts.state.getType())
                                        {
                                            eng->apvts.replaceState (state);
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
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
            if      (kindStr == "Layer")              kind = EngineKind::Layer;
            else if (kindStr == "Bass")               kind = EngineKind::Bass;
            else if (kindStr == "Drum")               kind = EngineKind::Drum;
            else if (kindStr == "Clip")               kind = EngineKind::Clip;
            else if (kindStr == "Vox")                kind = EngineKind::Vox;
            else if (kindStr == "Inst")               kind = EngineKind::Inst;
            else if (kindStr == "BaySickGuitars")     kind = EngineKind::BaySickGuitars;
            else if (kindStr == "BaySickBasses")      kind = EngineKind::BaySickBasses;
            else if (kindStr == "BaySickRustyDrums")  kind = EngineKind::BaySickRustyDrums;
            else if (kindStr == "Plugin")             kind = EngineKind::Plugin;
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
    if (auto* mlc = ui->getChildByName ("MeterLatencyComp"))
    {
        // 2026-05-02: restore meter latency-compensation toggle and recompute
        // the block count from the live device latency (project saved an
        // on/off bit; the actual ms-of-latency depends on the user's current
        // audio device, which may differ from the saved session).
        const bool on = mlc->getIntAttribute ("on", 0) != 0;
        MeterLatencyComp::gEnabled.store (on, std::memory_order_relaxed);
        const int latSamples = mProcessor.getDeviceOutputLatency();
        const int blockSize  = mProcessor.getBlockSize();
        const double sr      = mProcessor.getSampleRate();
        MeterLatencyComp::recomputeFromDevice (sr, blockSize, latSamples);
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
    // QA-Ef: recreate every aux UI strip from the engine's restored aux nodes.
    // restoreAuxStripsFromState (in deserializeProject) rebuilds the aux engine
    // InsertNodes, but on a project load into the already-open app the MixerPage
    // constructor (which rebuilds UI strips from getAuxIndices()) does NOT re-run,
    // and AuxNames below only carries USER-RENAMED strips -- so a default-named
    // aux strip was restored in the engine yet never got a UI strip (its incoming
    // send param survived, leaving a dangling cable on the source strip).  Rebuild
    // from the authoritative engine state here (idempotent); AuxNames just
    // overlays the custom names afterward.
    if (mMixerPage != nullptr)
        for (int idx : mProcessor.mVibeGraph.getAuxIndices())
            mMixerPage->addAuxChannelAtIndex (idx);
    restoreStripNames ("AuxNames",
                       [this](int i) { mMixerPage->addAuxChannelAtIndex  (i); },
                       [this](int i, const juce::String& n) { mMixerPage->setAuxStripName  (i, n); });
    restoreStripNames ("VoxNames",
                       [this](int i) { mMixerPage->addVoxChannelAtIndex  (i); },
                       [this](int i, const juce::String& n) { mMixerPage->setVoxStripName  (i, n); });
    restoreStripNames ("InstNames",
                       [this](int i) { mMixerPage->addInstChannelAtIndex (i); },
                       [this](int i, const juce::String& n) { mMixerPage->setInstStripName (i, n); });

    // D.3: restore mixer strip insertion order (was previously numeric-index
    // order, losing user re-order across project save/load).  Run after the
    // restoreStripNames calls above so all strips are registered before we
    // sort them.
    auto restoreOrder = [&](const char* tag, MixerPage::OrderKind kind)
    {
        auto* list = ui->getChildByName (tag);
        if (list == nullptr || mMixerPage == nullptr) return;
        std::vector<int> indices;
        indices.reserve ((size_t) list->getNumChildElements());
        for (auto* rec : list->getChildWithTagNameIterator ("S"))
        {
            const int idx = rec->getIntAttribute ("idx", -1);
            if (idx >= 0) indices.push_back (idx);
        }
        if (! indices.empty())
            mMixerPage->setStripOrder (kind, indices);
    };
    restoreOrder ("AuxOrder",   MixerPage::OrderKind::Aux);
    restoreOrder ("VoxOrder",   MixerPage::OrderKind::Vox);
    restoreOrder ("InstOrder",  MixerPage::OrderKind::Inst);
    restoreOrder ("AudioOrder", MixerPage::OrderKind::Audio);

    // Every window the restore loop framed needs its title-strip menu built.
    // Mirrors the launch-path sweep in the constructor (2026-07-28 fix):
    // showPageForTab is what fills the strip, and only the preferred tab
    // selected below would otherwise get it -- every other restored window
    // came up bare until its ribbon tab was clicked once.  The selection
    // below runs last and leaves the active window in front.
    for (auto* e : mPages)
        if (e != nullptr && e->window != nullptr)
            showPageForTab (e->ribbonTabId);

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

    // QA-D STATE-02: scan restored tabs and advance each monotonic counter
    // past max(parsed-name-suffix) so a subsequent +Add doesn't collide
    // with a restored tab number.  Reset-then-advance: closeAllDynamicTabs
    // zeroed every counter back to 1; this lifts each past any restored
    // values it can recognise.
    advanceCountersFromRestoredTabs();
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
    // QA-TempoMap: push the loaded ruler tempo flags BEFORE the base so the
    // single rebuild that setBPM triggers sees the full marker set.  Markers
    // are song-domain; pattern mode gets the empty set (and every later mode
    // switch re-pushes via onSongModeChanged).
    std::vector<std::pair<double,double>> markers;
    if (transport && transport->isSongMode())
        for (int i = 0; i < pm->getNumTempoChanges(); ++i)
        {
            const auto& tc = pm->getTempoChange (i);
            markers.push_back ({ (double) tc.bar * 4.0, tc.bpm });
        }
    ph.setTempoMarkers (std::move (markers));
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
// (openUiPrefs is forward-declared at the top of this TU -- the constructor
// needs it well before this point.)

// ── TS7 §6.9 / CL-055: smart auto-freeze + the re-render queue ───────────────
//
// Both jobs drain at most ONE tab per tick, deliberately.  A freeze is a
// blocking offline render on the message thread; doing several in one trip would
// stall the UI precisely when the machine is already struggling, which is the
// opposite of what the feature is for.
//
// The threshold is a MACHINE preference (§8.1): 0 means always freeze, 100 is the
// top real threshold, and anything above it is Off.
// ── TS7 §3: version capture ──────────────────────────────────────────────────
// Everything here is a READ of state audio already publishes.  The audio thread
// never calls into VersionCapture, and this never calls into audio beyond the
// two capture start/stop points, which are message-thread file operations.
juce::String StandaloneEditor::currentScopeLabel() const
{
    if (mProcessor.isSongMode()) return "Arrangement";
    if (mPM != nullptr)
    {
        const int cur = mPM->getCurrentPatternIndex();
        if (cur >= 0 && cur < mPM->getNumPatterns())
            return "Pattern: " + mPM->getPattern (cur).name;
    }
    return "Pattern";
}

void StandaloneEditor::pollVersionCapture()
{
    // Final meters are attached BEFORE poll(), because poll() is what closes the
    // take -- reading them afterwards would read the next take's fresh window.
    if (mVersionCapture.isCapturing())
        mVersionCapture.setFinalMeters (mProcessor.getMasterLufs (2),
                                        mProcessor.getMasterTruePeakMaxDb());

    mVersionCapture.poll (mProcessor.getPlayStartEdges(),
                          mProcessor.getLoopWrapEdges(),
                          DSPBase::isTransportPlaying(),
                          mProjectManager ? mProjectManager->getChangeStamp() : 0u,
                          mProcessor.getMasterLufs (1),
                          // INSTANTANEOUS, not the running max: a flagged moment
                          // has to be timecoded, and the max carries no time.
                          mProcessor.getMasterTruePeakDb(),
                          currentScopeLabel());
}

void StandaloneEditor::exportCapturedTake (const VersionCapture::Version& v)
{
    if (v.audioFile == juce::File() || ! v.audioFile.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon, "No audio for this take",
            "This take was captured as analysis only, so there is no audio file "
            "to export.\n\nTurn on version audio capture in File Settings to "
            "keep the audio for future takes.");
        return;
    }

    const auto suggested = juce::File::getSpecialLocation (
                               juce::File::userMusicDirectory)
                               .getChildFile (v.label.replaceCharacter (':', '-')
                                              + ".wav");

    auto chooser = std::make_shared<juce::FileChooser> (
        "Export take", suggested, "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles,
        [chooser, src = v.audioFile] (const juce::FileChooser& fc)
        {
            const auto dest = fc.getResult();
            if (dest == juce::File()) return;
            // Copy, not move: the take must stay playable in the analyzer after
            // the user has exported it.
            if (dest.existsAsFile()) dest.deleteFile();
            if (! src.copyFileTo (dest))
                juce::AlertWindow::showMessageBoxAsync (
                    juce::AlertWindow::WarningIcon, "Export failed",
                    "The take could not be written to:\n" + dest.getFullPathName());
        });
}

// TS7: the render notice for the AUTOMATIC paths (Jeff, 2026-07-30).
//
// Deliberately names the TRACK, not just "working": an automatic freeze happens
// uninvited, and "Freezing Layers 2..." tells the user both what the app is doing
// and which of their tracks it concerns.  Cancellable for the same reason the
// manual one is -- an automatic action the user cannot stop is worse than one
// they did not ask for.
//
// Caller pairs this with endOp().  Not ScopedOp, because these call sites run the
// render inline and need the overlay to close before they return rather than at
// the end of an enclosing scope.
void StandaloneEditor::showFreezeRenderNotice (TabKind kind, int pageIndex)
{
    const char* kindName = "track";
    switch (kind)
    {
        case TabKind::Layers:  kindName = "Layers";  break;
        case TabKind::Bass:    kindName = "Bass";    break;
        case TabKind::Drums:   kindName = "Drums";   break;
        case TabKind::Clips:   kindName = "Clips";   break;
        case TabKind::Vox:     kindName = "Vox";     break;
        case TabKind::Inst:    kindName = "Inst";    break;
        case TabKind::Plugins: kindName = "Plugin";  break;
        case TabKind::Rusty:   kindName = "Drum Kit"; break;
    }
    mHeavyOpOverlay.beginOp ("Freezing " + juce::String (kindName) + " "
                             + juce::String (pageIndex + 1) + "...", false);
    mHeavyOpOverlay.setCancellable (true);
}

void StandaloneEditor::pollAutoFreeze()
{
    // TS7 §6.5: drain the swing stamp the processor's APVTS listener publishes.
    // Handled HERE rather than in that listener because it can fire on the audio
    // thread.  Invalidates EVERY freeze rather than the one tab whose swing moved:
    // the id-to-tab mapping would have to happen on whichever thread wrote the
    // param, and over-invalidating costs a re-render while under-invalidating
    // leaves a freeze that no longer matches what the tab plays.
    {
        const auto s = mProcessor.getSwingChangeStamp();
        if (s != mLastSwingStamp)
        {
            mLastSwingStamp = s;
            mProcessor.engineRig().markAllFreezesStale();
        }
    }

    // §6.5: drain the per-tab engine-change stamps (hosted plugins, the Inst
    // chain, the kit) -- their listeners only bump atomics, because parameter
    // notifications carry no thread guarantee.
    mProcessor.engineRig().drainEngineChangeStamps();

    // Stale freezes first: a frozen tab whose content changed is currently
    // playing its LIVE engine (§6.6's fallback), so it is costing full CPU until
    // this catches up -- which makes it the more urgent of the two jobs.
    //
    // BUT NOT WHILE THE TRANSPORT RUNS.  An automation lane on a frozen tab's
    // engine parameter writes that APVTS on every 30 Hz applicator tick, and the
    // freeze watcher cannot tell an automation write from a knob turn -- so
    // draining here during playback re-rendered the same tab continuously:
    // render -> frozen -> next automation tick -> stale -> render.  A blocking
    // offline render mid-playback is the wrong thing to do regardless.
    // Deferring costs nothing correctness-wise because §6.6 already says a stale
    // freeze PLAYS LIVE until its file lands; it just lands at Stop instead.
    // AND ONLY ONCE EDITING HAS ACTUALLY STOPPED (2026-07-30).
    //
    // Deferring to "transport stopped" was aimed at exactly the wrong moment:
    // stopped is when the user is EDITING.  Every grid movement marks all
    // freezes stale (arrangement content is a real invalidator), each re-render
    // blocks the message thread for seconds, and the queue drained on the very
    // next tick -- so dragging a clip locked the app, twice over with two frozen
    // tabs.  Jeff's log caught it: twenty renders in three minutes, in pairs.
    //
    // A quiet period is the fix rather than a smaller invalidator: the
    // invalidation is CORRECT (the freeze really is stale), it is the response
    // that must not be instant.  Re-arming on every edit means a drag re-renders
    // ONCE when the hands come off, not once per mouse move.
    // Never start a render on top of another render: export / measure drive the
    // SAME live processor from a background thread, and two begin/endOffline
    // sequences interleaving their suspend/restore corrupts both.  The begin
    // call itself refuses too -- this early-out just avoids surfacing that
    // refusal as an error popup every poll tick.
    if (mProcessor.isOfflineRenderActive()) return;

    const double sinceEditMs = juce::Time::getMillisecondCounterHiRes() - mLastContentEditMs;
    if (! mFreezeRefreshQueue.empty()
        && ! DSPBase::isTransportPlaying()
        && sinceEditMs >= kFreezeRefreshQuietMs)
    {
        const auto job = mFreezeRefreshQueue.front();
        mFreezeRefreshQueue.erase (mFreezeRefreshQueue.begin());

        // The tab can die or be unfrozen between queueing and this drain (kit
        // destroy, tab delete, project switch, manual unfreeze) -- a dead job
        // must not flash the overlay for a render that will refuse anyway.
        if (! mProcessor.engineRig().isFrozen (job.kind, job.pageIndex))
            return;

        // RENDER NOTICE (Jeff, 2026-07-30).  This path renders SILENTLY today and
        // blocks the message thread for seconds -- so the app simply stopped
        // responding with nothing on screen, and with the buttons hidden by
        // default the user has no concept of "freeze" to explain it with.  An
        // unexplained multi-second stall is the single worst thing this feature
        // can do, and it is the automatic path that does it uninvited.
        showFreezeRenderNotice (job.kind, job.pageIndex);
        juce::String err;
        // songScopeOnly per ruling 2-b: the automatic re-render lands the song
        // scope; the filler below completes pattern coverage one per tick.
        if (! mProcessor.refreshFreeze (job.kind, job.pageIndex, err, /*songScopeOnly*/ true))
            DBG ("[TS7 FREEZE] re-render failed for tab " << (int) job.kind
                 << "/" << job.pageIndex << ": " << err);
        mHeavyOpOverlay.endOp();
        return;
    }

    // Ruling 2-b (2026-07-31): STAGGERED pattern coverage.  Automatic renders
    // (auto-freeze + the stale refresh above) are song-scope only; the
    // per-pattern files fill in HERE, one short render per quiet tick, so no
    // uninvited action ever stalls the app for a whole multi-pattern set.
    // Manual freezes still render their full set under the stepped overlay.
    if (! DSPBase::isTransportPlaying() && sinceEditMs >= kFreezeRefreshQuietMs)
    {
        TabKind fillKind {}; int fillPage = 0, fillPat = 0;
        if (mProcessor.findPendingPatternFreeze (fillKind, fillPage, fillPat))
        {
            showFreezeRenderNotice (fillKind, fillPage);
            juce::String fillErr;
            if (! mProcessor.renderPatternFreeze (fillKind, fillPage, fillPat, fillErr))
                DBG ("[TS7 FREEZE] pattern fill failed for tab " << (int) fillKind
                     << "/" << fillPage << " pat " << fillPat << ": " << fillErr);
            mHeavyOpOverlay.endOp();
            return;
        }
    }

    const int threshold = openUiPrefs()->getIntValue ("fsAutoFreezeCpu", 80);
    if (threshold > 100) return;                       // Off

    // ARM on sustained load, FIRE at Stop (Jeff's ruling, 2026-07-31).  The
    // sustained-load episode almost always happens DURING playback -- which is
    // exactly when a blocking modal render must not fire.  So crossing the
    // threshold only arms a pending freeze; the render itself waits for the
    // same stopped + quiet condition the stale-refresh drain above uses.  The
    // pending flag is what survives the load falling when playback stops --
    // without it, stopping cleared the very condition that asked for relief.
    const float load = mProcessor.mAudioDspLoad.load (std::memory_order_relaxed) * 100.0f;
    if (load < (float) threshold)
    {
        mAutoFreezeHoldTicks = 0;
    }
    else
    {
        // Sustained, not instantaneous.  A single spike -- opening a window,
        // loading a preset -- must not arm a freeze.  5 Hz poll, so 15 ticks is
        // three seconds continuously over the line.
        constexpr int kHoldTicks = 15;
        if (++mAutoFreezeHoldTicks >= kHoldTicks)
        {
            mAutoFreezeHoldTicks = 0;
            mAutoFreezePending   = true;
        }
    }

    if (! mAutoFreezePending) return;
    if (DSPBase::isTransportPlaying()) return;
    if (sinceEditMs < kFreezeRefreshQuietMs) return;

    // Nothing can succeed without the project folder; retrying every arm would
    // flash the overlay forever on an unsaved project.
    if (mProcessor.getProjectFreezeDir() == juce::File())
    {
        mAutoFreezePending = false;
        return;
    }
    mAutoFreezePending = false;

    // Freeze the first eligible tab that is not already frozen.  Engine-driven
    // kinds only -- the same set freezeTab accepts.
    // Every kind that can freeze, not just the original four (2026-07-30) --
    // Vox / Inst / Clips / Rusty became freezable and were never added here, so
    // auto-freeze would have kept passing over them while the machine struggled.
    // Capacity comes from the rig rather than kMaxLayerPages, which was wrong for
    // every kind except Layers.
    static constexpr TabKind kFreezable[] = { TabKind::Layers, TabKind::Bass,
                                              TabKind::Drums,  TabKind::Plugins,
                                              TabKind::Vox,    TabKind::Inst,
                                              TabKind::Clips,  TabKind::Rusty };
    for (auto kind : kFreezable)
    {
        for (int i = 0; i < EngineRig::capacityOf (kind); ++i)
        {
            auto* tab = mProcessor.engineRig().findTab (kind, i);
            if (tab == nullptr || tab->frozen) continue;
            // Jeff's ruling: an EXPLICIT unfreeze means leave that tab alone --
            // auto goes after tabs the user has expressed no opinion about.
            // Session-scoped: a hand re-freeze or a project reload puts the tab
            // back in play.
            if (tab->userUnfroze) continue;

            // Same render notice as the stale-refresh path: this fires uninvited
            // and blocks for seconds.  The kit routes to its own thirteen-strip
            // render -- freezeTab cannot express it, so auto-freezing the kit
            // through it failed on every trip.
            showFreezeRenderNotice (kind, i);
            juce::String err;
            // songScopeOnly per ruling 2-b: the uninvited render is ONE song
            // render; pattern coverage staggers in on later quiet ticks.
            const bool ok = (kind == TabKind::Rusty)
                ? mProcessor.freezeRustyKit (err, /*byUser*/ false, /*reuseValid*/ false,
                                             /*songScopeOnly*/ true)
                : mProcessor.freezeTab (kind, i, err, /*byUser*/ false, /*reuseValid*/ false,
                                        /*songScopeOnly*/ true);
            mHeavyOpOverlay.endOp();
            if (ok) return;   // one per trip
        }
    }
}

// ── TS7 §6.8: re-apply the freezes a project saved ───────────────────────────
// Collected during deserialize (the tabs do not exist yet at that point) and
// applied here, after every engine, rack and strip is in place -- freezing
// renders through the offline loop, so it needs the whole graph built.
//
// THE THRESHOLD NEVER RETRO-ACTS ON A DELIBERATE FREEZE.  A hand-frozen tab
// restores frozen on any machine, including one with auto-freeze switched off.
// An AUTO freeze is a performance adaptation to the machine that made it, so it
// restores only where auto-freeze is armed at all.
//
// Files are NOT saved with the project (regenerable cache, excluded from
// bundles), so this re-renders them.  Playback falls back to the live engine
// until each lands, per §6.6 -- which is why a bundle opened on a fresh machine
// is never silent, just briefly unfrozen.
void StandaloneEditor::applyCaptureSpecFromPrefs()
{
    const auto id = (LoudnessSpec::Id) juce::jlimit (
        0, (int) LoudnessSpec::Id::Count - 1,
        openUiPrefs()->getIntValue ("exSpecId", (int) LoudnessSpec::Id::Streaming14));
    const float customT = (float) openUiPrefs()->getDoubleValue ("exSpecCustom", -14.0);
    mVersionCapture.setSpec (LoudnessSpec::resolved (id, customT));
}

void StandaloneEditor::restorePendingFreezes()
{
    // §6.7's second cleanup rule, and it runs BEFORE the empty-list early return
    // on purpose: a project with NO frozen tabs is the case most likely to have
    // orphans (every freeze file in the folder belongs to a tab that is gone),
    // and sweeping only when something needs restoring would have skipped
    // exactly that case.  Before any re-freeze, so a file about to be rewritten
    // is not swept first.
    mProcessor.sweepOrphanFreezeFiles();

    if (mPendingFreezes.empty()) return;

    const int threshold = openUiPrefs()->getIntValue ("fsAutoFreezeCpu", 80);
    const bool autoFreezeArmed = (threshold <= 100);

    auto pending = std::move (mPendingFreezes);
    mPendingFreezes.clear();

    // §6.8: the span decides whether the file on disk still describes this
    // project.  A freeze rendered against a 64-beat song at 120 does not stand in
    // for a 96-beat song at 128 -- re-rendering is the only correct answer.  The
    // stamp comparison inside freezeTab is what catches it: a changed tempo or
    // arrangement moves the stamp, so the mismatched file is not reused.
    const double nowBeats = mPM != nullptr ? mPM->getSongEndBeats()  : 0.0;
    const double nowBpm   = mPM != nullptr ? mPM->getGlobalTempo()   : 120.0;

    for (const auto& p : pending)
    {
        if (! p.byUser && ! autoFreezeArmed) continue;

        // lengthBeats == 0 means a project saved before the span existed; treat
        // it as "unknown, re-render" rather than assuming it still fits.
        const bool spanKnown = p.span.lengthBeats > 0.0;
        if (spanKnown && ! p.span.matches (nowBeats, nowBpm))
            DBG ("[TS7 FREEZE] span changed for tab " << (int) p.kind << "/" << p.pageIndex
                 << " (was " << p.span.lengthBeats << "@" << p.span.bpm
                 << ", now " << nowBeats << "@" << nowBpm << ") -- re-rendering");

        // Seed the tab with the stamps the project recorded BEFORE freezeTab
        // runs -- it compares the tab's stored stamps against freshly computed
        // ones to decide what it can reuse, so without this every file would
        // read as unstamped and re-render, which is the cost this exists to
        // remove.
        if (auto* t = mProcessor.engineRig().findTab (p.kind, p.pageIndex))
        {
            t->freezeSpan    = p.span;
            t->patternStamps = p.patternStamps;
        }

        juce::String err;
        // reuseValid: a file whose content stamp still matches is reused as-is.
        // A project reopened unchanged renders NOTHING; one where a pattern was
        // edited re-renders that pattern and the song, and leaves the other
        // patterns alone.  Saved-stale re-renders regardless -- the stamp
        // cannot see every invalidator, which is how the tab went stale.
        // The kit has its own thirteen-strip render path; freezeTab cannot
        // express it, so restoring it through freezeTab restored nothing.
        const bool reuse = ! p.stale;
        const bool ok = (p.kind == TabKind::Rusty)
            ? mProcessor.freezeRustyKit (err, p.byUser, reuse)
            : mProcessor.freezeTab (p.kind, p.pageIndex, err, p.byUser, reuse);
        if (! ok)
            DBG ("[TS7 FREEZE] restore failed for tab " << (int) p.kind
                 << "/" << p.pageIndex << ": " << err);
    }
}

void StandaloneEditor::restoreAudioStripsFromArrangement (bool isLoadContext)
{
    if (mPM == nullptr) return;

    if (mHeavyOpOverlay.isActive())
        mHeavyOpOverlay.setStepLabel ("Rebuilding audio strips...");

    // QA-Fe2: grid-default picks are per-project session state ("locks until
    // the project is shut down") -- every load path runs through here.
    if (isLoadContext)
        mVoxTakePick.fill (-1);

    // QA-Ef (2026-05-22): shield the audio-row rebuild on load paths.  This runs
    // just after deserializeProject returns (its shield already lowered), and
    // ensureAudioInsert below calls registerTask while applyPendingRackStates
    // hot-swaps InsertNode racks -- both race the audio thread's render-graph
    // walk if audio is live.  Nest-aware + outermost-only drain, matching
    // deserializeProject / closeAllDynamicTabs.  Gated on isLoadContext so a
    // future non-load caller doesn't needlessly mute audio.
    const bool shieldWasUp = isLoadContext && mProcessor.isProjectLoadInProgress();
    if (isLoadContext)
    {
        mProcessor.setProjectLoadInProgress (true);
        if (! shieldWasUp)
            juce::Thread::sleep (30);
    }

    // QA-D STATE-01: this method runs after ProjectManager::openProject
    // returns -- outside its mIgnoreDirty window.  applyPendingRackStates
    // below fires EffectRack::clearSlot lifecycle dirty hooks that chain
    // through to markDirty, leaving the freshly-loaded project marked
    // dirty.  Suppress dirty fires for the duration of the restore so the
    // user doesn't see a spurious title-bar `*` on load.  Every caller of
    // this method is a load path (Open Recent / Open Project Browser /
    // New From Template / Restore Backup), so the clearDirty at the end
    // is also correct -- prior unsaved edits are already discarded by
    // reaching this point in the load sequence.  Stash + restore wasIgnoring
    // so nested calls (theoretical) don't flip the gate prematurely.
    const bool wasIgnoring = mProjectManager && mProjectManager->isLoadingProject();
    if (mProjectManager) mProjectManager->setIgnoreDirty (true);

    for (int i = 0; i < mPM->getNumBlocks(); ++i)
    {
        const auto& b = mPM->getBlock (i);
        if (b.clipType != ClipType::Audio) continue;
        // QA-E: Vox/Inst-routed blocks restore via their own page chain and
        // must NOT spawn an Audio row strip (MIX-02/03/04/06 phantom-strip
        // guard).  2026-05-18 §9 Forks fix: the old `routeChannel != 0` test
        // also skipped generic Audio clips -- QA-E Task 5 retags those
        // 0 -> audioInsert(row), so their strip was never restored on reload.
        // Skip ONLY genuine Vox/Inst routes; 0 or an Audio-range channel
        // still needs its Audio row strip.
        {
            using namespace MixerChannelIds;
            const int rc = b.routeChannel;
            const bool voxInstRouted =
                   (rc >= kVoxBase  && rc < kVoxBase  + kMaxVoxStrips)
                || (rc >= kInstBase && rc < kInstBase + kMaxInstStrips);
            if (voxInstRouted) continue;
        }
        // QA-ClipDrop Task 3 (SC-I reload parity, 2026-06-03): key the strip on
        // the clip's OWNING Clips-page row (derived from routeChannel), NOT the
        // grid row it currently sits on.  Identical owner-derivation to
        // renderAudioClipsForRow (PluginProcessor.cpp): a clip moved to another
        // grid row before saving keeps its strip at the owner row, and matches
        // the strip createClipStripAndPage made at the Clips restore branch
        // (idempotent -> exactly one strip, no stray strip at the moved row).
        // Legacy/unset routeChannel (0) falls back to trackRow, so projects from
        // before owner-routing are unchanged.
        const int routeCh = b.routeChannel;
        const int row =
            (routeCh >= MixerChannelIds::kAudioBase
          && routeCh <  MixerChannelIds::kAudioBase + VibeSynthProcessor::kMaxAudioRows)
                ? (routeCh - MixerChannelIds::kAudioBase)
                : b.trackRow;
        if (row < 0 || row >= VibeSynthProcessor::kMaxAudioRows) continue;

        const juce::String stripName =
            b.displayAlias.isNotEmpty() ? b.displayAlias
                                         : juce::String ("Audio ") + juce::String (row + 1);
        // 2026-04-29 ORDER FIX: register InsertNode + APVTS params BEFORE
        // creating the strip so setApvts can attach the fader/mute/etc.
        mProcessor.mVibeGraph.addAudioRowChannel (row, stripName);
        mProcessor.ensureAudioInsert (row, stripName);
        if (mMixerPage)
            mMixerPage->addAudioChannel (row, stripName);
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

    // QA-ModelShell TS1 (wire-at-load): racks are fully populated now (bus
    // racks via deserializeProject's loadRackStates, per-insert racks via the
    // apply above) -- register every slot's DSP-targeting automation without
    // waiting for the Effects page to be visited.
    EffectsPage::registerRackAutomationForAllChannels (mProcessor);

    // TS7 §6.8: re-apply saved freezes.  LAST, deliberately -- freezing renders
    // the tab through the offline loop, so every engine, rack and strip it
    // touches has to exist first.
    restorePendingFreezes();

    // 2026-04-24: push the saved global tempo into the playhead now that the
    // full project has been restored.  Transport BPM field picks it up on
    // its next timer tick.
    syncTempoFromPatternManager (mPlayHead, mTransport.get(), mPM.get());

    if (mProjectManager)
    {
        mProjectManager->setIgnoreDirty (wasIgnoring);
        // QA-E Task 8 NIT-2: gate the clearDirty on isLoadContext (defensive
        // -- all current callers are load paths and pass the default true, so
        // behavior is unchanged; a future non-load caller passing false will
        // not wrongly discard the user's dirty state).
        if (isLoadContext && ! wasIgnoring) mProjectManager->clearDirty();
    }

    // QA-Ef (2026-05-22): lower the load shield (restore prior state; clears
    // only when we were the outermost owner).  Audio resumes here.
    if (isLoadContext)
        mProcessor.setProjectLoadInProgress (shieldWasUp);
}

// ── QA-F (2026-07-09): BaySickAlign bake placement ───────────────────────────
// Bake -> the track row BELOW the follower's original clips; original rows
// row-muted (one-click A/B toggle); a prior bake for the same channel is
// replaced IN PLACE (no row stacking per render).  The block is routed
// through the follower's Vox chain so the A/B compares like-for-like, and
// alignBake-marked so re-analysis never composites the bake into its own
// source.
// QA-Fa recovery (2026-07-10): DORMANT by design -- Render is export-only
// and the automatic call from renderAlignedTake was retired.  Kept (with
// the alignBake guard sites) for any future same-channel bake placement;
// re-import goes through addVoxFromExport below.
void StandaloneEditor::placeAlignedBake (const juce::File& bakeFile, double startBeat,
                                         int followerChannelId)
{
    if (! mPM || ! bakeFile.existsAsFile() || followerChannelId <= 0)
        return;

    // File duration via the header only (dropWavAsClip precedent).
    double fileSeconds = 0.0;
    {
        juce::AudioFormatManager fmt;
        fmt.registerBasicFormats();
        if (auto reader = std::unique_ptr<juce::AudioFormatReader> (
                fmt.createReaderFor (bakeFile)))
            if (reader->sampleRate > 0.0)
                fileSeconds = (double) reader->lengthInSamples / reader->sampleRate;
    }
    if (fileSeconds <= 0.0)
        return;

    // Length in beats through the tempo map from the bake position (linear
    // fallback at the transport tempo when no timeline is published).
    const double bpm = juce::jmax (20.0, mTransport ? mTransport->getBPM() : 120.0);
    double lengthBeats;
    double bpmAtStart = bpm;
    if (TempoMap::isActive())
    {
        const juce::int64 s0 = TempoMap::sampleAtBeat (startBeat);
        const double mapSr   = TempoMap::gSampleRate.load (std::memory_order_relaxed);
        const juce::int64 s1 = s0 + (juce::int64) std::llround (
            fileSeconds * (mapSr > 0.0 ? mapSr : 44100.0));
        lengthBeats = TempoMap::beatAtSample (s1) - startBeat;
        bpmAtStart  = TempoMap::bpmAtSample (s0);
    }
    else
    {
        lengthBeats = fileSeconds * bpm / 60.0;
    }
    if (lengthBeats <= 0.0)
        return;

    const juce::String relPath = "Aligned/" + bakeFile.getFileName();
    constexpr double kBeatsPerBar = 4.0;

    // Originals = un-muted audio clips routed to the follower channel
    // (bakes excluded).  Their rows get muted; the bake row sits below the
    // deepest of them.
    int maxOriginalRow = -1;
    std::vector<int> originalRows;
    int existingBakeIdx = -1;
    for (int i = 0; i < mPM->getNumBlocks(); ++i)
    {
        const auto& b = mPM->getBlock (i);
        if (b.clipType != ClipType::Audio || b.routeChannel != followerChannelId)
            continue;
        if (b.alignBake)
        {
            existingBakeIdx = i;   // replace in place on re-render
            continue;
        }
        if (b.muted) continue;
        maxOriginalRow = juce::jmax (maxOriginalRow, b.trackRow);
        if (std::find (originalRows.begin(), originalRows.end(), b.trackRow)
                == originalRows.end())
            originalRows.push_back (b.trackRow);
    }
    if (maxOriginalRow < 0 && existingBakeIdx < 0)
        return;   // nothing routed to this channel -- nowhere sensible to place

    const int targetRow = (existingBakeIdx >= 0)
        ? mPM->getBlock (existingBakeIdx).trackRow
        : juce::jmin (kMaxArrangementRows - 1, maxOriginalRow + 1);

    // Browser entry for the bake (grouped under the Vox page's category).
    mPM->addAudioToLibrary (relPath, {}, followerChannelId);

    if (existingBakeIdx >= 0)
    {
        auto& b = mPM->getBlock (existingBakeIdx);
        b.audioFilePath = relPath;
        b.setStartBeats  (startBeat);
        b.lengthBars    = juce::jmax (1, (int) std::ceil (lengthBeats / kBeatsPerBar));
        b.setLengthBeats (lengthBeats);
        b.originalBPM   = (float) bpmAtStart;
        b.stretchMode   = true;
        b.muted         = false;
        b.contentStartSamples = 0;
    }
    else
    {
        ArrangementBlock block;
        block.clipType      = ClipType::Audio;
        block.trackRow      = targetRow;
        block.setStartBeats  (startBeat);
        block.lengthBars    = juce::jmax (1, (int) std::ceil (lengthBeats / kBeatsPerBar));
        block.setLengthBeats (lengthBeats);
        block.patternIndex  = mPM->getCurrentPatternIndex();
        block.layerTrack    = false;
        block.audioFilePath = relPath;
        block.originalBPM   = (float) bpmAtStart;   // unstretched at its own position (G law)
        block.stretchMode   = true;
        block.routeChannel  = followerChannelId;    // same Vox chain as the originals
        block.alignBake     = true;
        // Detached copy: the bake's identity is authoritative -- a library
        // Properties edit must not re-drive its BPM/mode/route.
        block.isOverride    = true;
        mPM->addBlock (block);
    }

    // Mute the original rows -- the owner-specified one-click A/B toggle.
    for (int row : originalRows)
        mPM->setRowMuted (row, true);

    mProcessor.rebuildAudioClipPlayers();
    if (mProjectManager) mProjectManager->markDirty();
    if (mBuilderPage) mBuilderPage->repaint();
}

// ── QA-Fa recovery (2026-07-10): "+ Add New Vox From Export" ─────────────────
int StandaloneEditor::findFreeVoxIndex() const
{
    std::array<bool, kMaxVoxPages> taken {};
    for (auto* entry : mPages)
    {
        if (! entry || entry->type != RibbonTabBar::TabType::Vox) continue;
        if (auto* vp = dynamic_cast<VoxPage*> (entry->component.get()))
        {
            const int i = vp->getPageIndex();
            if (i >= 0 && i < (int) kMaxVoxPages) taken[(size_t) i] = true;
        }
    }
    for (int i = 0; i < (int) kMaxVoxPages; ++i)
        if (! taken[(size_t) i]) return i;
    return -1;
}

std::vector<RibbonTabBar::VoxExportEntry> StandaloneEditor::listVoxExportEntries()
{
    std::vector<RibbonTabBar::VoxExportEntry> out;

    // Grey rules: empty list = greyed menu entry.
    juce::File projDir;
    {
        const juce::ScopedLock lk (mProcessor.mProjectFolderLock);
        projDir = mProcessor.mCurrentProjectFolder;
    }
    if (projDir == juce::File() || ! projDir.isDirectory()) return out;   // unsaved project
    if (findFreeVoxIndex() < 0) return out;                               // vox cap reached

    for (const char* folder : { "Aligned", "Pitched" })
    {
        auto files = projDir.getChildFile (folder).findChildFiles (
            juce::File::findFiles, false, "*.wav");
        std::sort (files.begin(), files.end(),
                   [] (const juce::File& a, const juce::File& b)
                   { return a.getFileName().compareNatural (b.getFileName()) < 0; });
        for (const auto& f : files)
            out.push_back ({ folder, f.getFileName(), f.getFullPathName() });
    }
    return out;
}

void StandaloneEditor::placeVoxExportClip (const juce::File& exportFile, double startBeat,
                                           int routeChannel)
{
    if (! mPM || ! exportFile.existsAsFile() || routeChannel <= 0)
        return;

    // File duration via the header only (dropWavAsClip precedent).
    double fileSeconds = 0.0;
    {
        juce::AudioFormatManager fmt;
        fmt.registerBasicFormats();
        if (auto reader = std::unique_ptr<juce::AudioFormatReader> (
                fmt.createReaderFor (exportFile)))
            if (reader->sampleRate > 0.0)
                fileSeconds = (double) reader->lengthInSamples / reader->sampleRate;
    }
    if (fileSeconds <= 0.0)
        return;

    const double bpm = juce::jmax (20.0, mTransport ? mTransport->getBPM() : 120.0);
    double lengthBeats;
    double bpmAtStart = bpm;
    if (TempoMap::isActive())
    {
        const juce::int64 s0 = TempoMap::sampleAtBeat (startBeat);
        const double mapSr   = TempoMap::gSampleRate.load (std::memory_order_relaxed);
        const juce::int64 s1 = s0 + (juce::int64) std::llround (
            fileSeconds * (mapSr > 0.0 ? mapSr : 44100.0));
        lengthBeats = TempoMap::beatAtSample (s1) - startBeat;
        bpmAtStart  = TempoMap::bpmAtSample (s0);
    }
    else
    {
        lengthBeats = fileSeconds * bpm / 60.0;
    }
    if (lengthBeats <= 0.0)
        return;

    const juce::String relPath = exportFile.getParentDirectory().getFileName()
                               + "/" + exportFile.getFileName();
    constexpr double kBeatsPerBar = 4.0;

    int nextRow = 0;
    for (int i = 0; i < mPM->getNumBlocks(); ++i)
        nextRow = juce::jmax (nextRow, mPM->getBlock (i).trackRow + 1);
    nextRow = juce::jmin (nextRow, kMaxArrangementRows - 1);

    ArrangementBlock block;
    block.clipType      = ClipType::Audio;
    block.trackRow      = nextRow;
    block.setStartBeats  (startBeat);
    block.lengthBars    = juce::jmax (1, (int) std::ceil (lengthBeats / kBeatsPerBar));
    block.setLengthBeats (lengthBeats);
    block.patternIndex  = mPM->getCurrentPatternIndex();
    block.layerTrack    = false;
    block.audioFilePath = relPath;
    block.originalBPM   = (float) bpmAtStart;
    block.stretchMode   = true;
    block.routeChannel  = routeChannel;
    // First-class clip -- alignBake deliberately FALSE: the new strip's own
    // Align/Pitch analyses must see this audio, and the cleaned take must be
    // listable as a Leader for other strips.  Cross-channel self-feedback
    // cannot occur (composites filter by routeChannel).
    block.isOverride    = true;
    mPM->addBlock (block);
    mPM->addAudioToLibrary (relPath, {}, routeChannel);

    mProcessor.rebuildAudioClipPlayers();
    if (mProjectManager) mProjectManager->markDirty();
    if (mBuilderPage) mBuilderPage->repaint();
}

void StandaloneEditor::addVoxFromExport (const juce::String& fullPath)
{
    if (! mMixerPage || ! mPM) return;
    const juce::File exportFile (fullPath);
    if (! exportFile.existsAsFile()) return;

    juce::File projDir;
    {
        const juce::ScopedLock lk (mProcessor.mProjectFolderLock);
        projDir = mProcessor.mCurrentProjectFolder;
    }

    // Original-position + source-strip lookup across every Vox engine's
    // render histories.  Orphan files (no history entry) place at beat 0
    // and skip both prompts -- there is no identifiable source strip.
    double   startBeat = 0.0;
    VoxPage* sourceVp  = nullptr;
    int      sourceIdx = -1;
    for (auto* entry : mPages)
    {
        if (! entry || entry->type != RibbonTabBar::TabType::Vox) continue;
        auto* vp = dynamic_cast<VoxPage*> (entry->component.get());
        if (vp == nullptr) continue;
        auto* bv = dynamic_cast<BaySickVocalProcessor*> (vp->getVocalProcessor());
        if (bv == nullptr) continue;

        auto matches = [&] (const BaySickVocalProcessor::AlignRenderEntry& r)
        { return r.file.isNotEmpty() && projDir.getChildFile (r.file) == exportFile; };

        bool found = false;
        for (const auto& r : bv->mAlignState.renders)
            if (matches (r)) { startBeat = r.startBeat; found = true; break; }
        if (! found)
            for (const auto& r : bv->mPitchRenders)
                if (matches (r)) { startBeat = r.startBeat; found = true; break; }
        if (found) { sourceVp = vp; sourceIdx = vp->getPageIndex(); break; }
    }

    const int newIdx = findFreeVoxIndex();
    if (newIdx < 0) return;

    // Strip + InsertNode + params + tab via the Mixer-add cascade
    // (addVoxChannelAtIndex synchronously fires onVoxStripAdded ->
    // spawnVoxTabIfMissing).
    mMixerPage->addVoxChannelAtIndex (newIdx);

    VoxPage* newVp = nullptr;
    int newRibbonId = -1;
    for (auto* entry : mPages)
    {
        if (! entry || entry->type != RibbonTabBar::TabType::Vox) continue;
        if (auto* vp = dynamic_cast<VoxPage*> (entry->component.get()))
            if (vp->getPageIndex() == newIdx)
                { newVp = vp; newRibbonId = entry->ribbonTabId; break; }
    }
    if (newVp == nullptr) return;

    placeVoxExportClip (exportFile, startBeat, MixerChannelIds::voxInsert (newIdx));

    if (newRibbonId >= 0 && mRibbon)
    {
        mRibbon->selectTab (newRibbonId);
        onTabSelected (newRibbonId);
    }

    if (sourceVp == nullptr)
        return;

    // PROMPT 1 (clone the source chain) then PROMPT 2 (mute the source
    // strip), sequential, both optional.
    juce::Component::SafePointer<StandaloneEditor> self (this);
    juce::Component::SafePointer<VoxPage> src (sourceVp);
    juce::Component::SafePointer<VoxPage> dst (newVp);
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon,
        "New Vox From Export",
        "Clone the source tab's vocal chain settings?",
        "Yes", "No", nullptr,
        juce::ModalCallbackFunction::create ([self, src, dst, sourceIdx] (int clone)
        {
            if (self == nullptr) return;
            if (clone == 1 && src != nullptr && dst != nullptr)
            {
                dst->importVoxState (src->exportVoxState());
                // Chain settings only: the source's Align/Pitch analyses,
                // versions, and render histories reference the SOURCE
                // channel's composites -- meaningless on the new strip.
                if (auto* nbv = dynamic_cast<BaySickVocalProcessor*> (
                        dst->getVocalProcessor()))
                {
                    nbv->mAlignState = BaySickVocalProcessor::AlignState();
                    nbv->mAlignVersions.clear();
                    nbv->mAlign.clearWarpMap();
                    nbv->publishAlignPlayback();
                    nbv->mPitchRenders.clear();
                    nbv->mPitchVersions.clear();
                    nbv->mPitchAnalyzedSig = 0;
                    nbv->mPitch.stateFromValueTree ({});
                }
            }
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon,
                "New Vox From Export",
                "Mute the original Vox strip?",
                "Yes", "No", nullptr,
                juce::ModalCallbackFunction::create ([self, sourceIdx] (int mute)
                {
                    if (self == nullptr || mute != 1) return;
                    const juce::String id = "mixer_vox_"
                        + juce::String (sourceIdx) + "_mute";
                    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
                            self->mProcessor.apvts.getParameter (id)))
                        p->setValueNotifyingHost (
                            p->getNormalisableRange().convertTo0to1 (1.0f));
                }));
        }));
}

// ── QA-Fa (2026-07-10): BaySickPitch "Send Notes to..." ─────────────────────
std::vector<StandaloneEditor::PitchNoteTarget> StandaloneEditor::listPitchNoteTargets() const
{
    std::vector<PitchNoteTarget> targets;
    for (auto* entry : mPages)
    {
        if (! entry || ! entry->component) continue;
        if (auto* lp = dynamic_cast<LayersPage*> (entry->component.get()))
            targets.push_back ({ 0, lp->getPageIndex(),
                                 "Layer " + juce::String (lp->getPageIndex() + 1) });
        else if (auto* bp = dynamic_cast<BassPage*> (entry->component.get()))
            targets.push_back ({ 1, bp->getPageIndex(),
                                 "Bass " + juce::String (bp->getPageIndex() + 1) });
        else if (auto* dp = dynamic_cast<DrumPage*> (entry->component.get()))
            targets.push_back ({ 2, dp->getPageIndex(),
                                 "Drum " + juce::String (dp->getPageIndex() + 1) });
        else if (auto* cp = dynamic_cast<ClipsPage*> (entry->component.get()))
            targets.push_back ({ 3, cp->getPageIndex(),
                                 "Clips " + juce::String (cp->getPageIndex() + 1) });
    }
    std::sort (targets.begin(), targets.end(),
               [] (const PitchNoteTarget& a, const PitchNoteTarget& b)
               { return a.kind != b.kind ? a.kind < b.kind
                                         : a.pageIndex < b.pageIndex; });
    return targets;
}

void StandaloneEditor::sendPitchNotesToTab (int kind, int pageIndex,
                                            const std::vector<ContourNote>& notes)
{
    if (! mPM || notes.empty()) return;

    auto& pat = mPM->getPattern (mPM->getCurrentPatternIndex());
    PianoRollData* roll = nullptr;
    switch (kind)
    {
        case 0: if (pageIndex >= 0 && pageIndex < (int) pat.layerRoll.size())
                    roll = &pat.layerRoll[(size_t) pageIndex];
                break;
        case 1: if (pageIndex >= 0 && pageIndex < (int) pat.bassRoll.size())
                    roll = &pat.bassRoll[(size_t) pageIndex];
                break;
        case 2: if (pageIndex >= 0 && pageIndex < (int) pat.drumRolls.size())
                    roll = &pat.drumRolls[(size_t) pageIndex];
                break;
        case 3: if (pageIndex >= 0 && pageIndex < (int) pat.clipRoll.size())
                    roll = &pat.clipRoll[(size_t) pageIndex];
                break;
        default: break;
    }
    if (roll == nullptr) return;

    // Seconds -> beats at the current transport tempo: the contour keeps its
    // rhythm at today's tempo, starting at the pattern's beat 0.
    const double bpm = juce::jmax (20.0, mTransport ? mTransport->getBPM() : 120.0);
    const double bps = bpm / 60.0;

    double maxBeat = 0.0;
    for (const auto& n : notes)
    {
        PianoNote pn;
        pn.midiNote      = juce::jlimit (0, 127, n.midiNote);
        pn.startBeat     = juce::jmax (0.0, n.startSec * bps);
        pn.durationBeats = juce::jmax (0.05, (n.endSec - n.startSec) * bps);
        roll->notes.push_back (pn);
        maxBeat = juce::jmax (maxBeat, pn.startBeat + pn.durationBeats);
    }
    roll->numBars = juce::jmax (roll->numBars, (int) std::ceil (maxBeat / 4.0));

    if (mProjectManager) mProjectManager->markDirty();
    repaint();
}

// ── QA-Fe2 De-noise (2026-07-16) ─────────────────────────────────────────────

namespace
{
    // `ui_prefs.xml` -- the editor's machine-scoped preferences, NOT project
    // data.  Carries the De-noise take-type picks, and since TS7 the auto-freeze
    // threshold and the capture-retention choice too.
    //
    // Same PropertiesFile as BaySickPitchEditor's openUiPrefs (file-local there;
    // duplicated rather than hoisted to keep that batch's blast radius inside
    // the editor).
    std::unique_ptr<juce::PropertiesFile> openUiPrefs()
    {
        juce::PropertiesFile::Options o;
        o.applicationName    = "BaySickDAW";
        o.filenameSuffix     = "xml";
        o.folderName         = "BaySickDAW";
        o.osxLibrarySubFolder = "Application Support";
        return std::make_unique<juce::PropertiesFile> (
            AppPaths::appRoot().getChildFile ("ui_prefs.xml"), o);
    }
}

StandaloneEditor::FileTakeSettings StandaloneEditor::readFileTakeSettings() const
{
    auto p = openUiPrefs();
    FileTakeSettings s;
    // Defaults preserve today's behavior (DRY + WET written) until the user
    // opts into cleaned takes; strength default = Strong (the ear-validated
    // "cleantake" setting from the WORLD arc).
    s.dry        = p->getBoolValue ("fsWriteDry",        true);
    s.dryCleaned = p->getBoolValue ("fsWriteDryCleaned", false);
    s.wet        = p->getBoolValue ("fsWriteWet",        true);
    s.wetCleaned = p->getBoolValue ("fsWriteWetCleaned", false);
    s.strength   = p->getIntValue  ("fsDenoiseStrength", (int) Denoise::Strong);
    return s;
}

void StandaloneEditor::showFileSettingsDialog()
{
    // Standard app-styled dialog (503-pattern).  Live >=1 enforcement:
    // unchecking the last checked take type re-checks it.
    struct FileSettingsComp : juce::Component
    {
        juce::ToggleButton boxes[4];
        juce::ComboBox strength;
        juce::Label note, strengthLbl;
        // TS7 §8.1 auto-freeze threshold + §8.2 capture retention.
        // The MAGNET the item asks for.  Over 0..101 in a ~184 px strip each step
        // is under two pixels, so without this, landing on exactly 100 rather
        // than 99 or Off is a pixel-hunt -- which is the "fighting the slider"
        // §8.1 says to solve.  Same snapValue idiom as SharedUI's SnapSlider.
        struct FreezeSlider : juce::Slider
        {
            FreezeSlider() : juce::Slider (juce::Slider::LinearHorizontal,
                                           juce::Slider::NoTextBox) {}
            double snapValue (double v, DragMode) override
            {
                return (std::abs (v - 100.0) < 2.0) ? 100.0 : v;
            }
        };
        FreezeSlider   freezeCpu;
        juce::Label    freezeLbl, freezeReadout;
        juce::ComboBox   captureRetain;
        juce::Label      captureLbl;
        juce::ToggleButton captureAudio;      // TS7 §3.1
        juce::ToggleButton instrumentFreeze;  // TS7 §6.9 (Jeff, 2026-07-30)
        // Applied live rather than at next launch: capture is running right now,
        // and a setting that needs a restart to take effect reads as broken.
        std::function<void()> onCaptureSettingsChanged;

        // The slider runs 0..101.  100 is a real threshold and 101 is OFF, which
        // is why the range is one wider than the percentage it shows: dragging
        // PAST the top is how the feature is switched off (Jeff's shape), and the
        // readout says "Off" there rather than a number that would read as
        // "freeze at 101%".  0 means always freeze, deliberately -- a weaker
        // machine gets the CPU saving immediately.
        //
        // An enum rather than `static constexpr int`: this class is defined
        // inside a function, and a local class cannot have static data members
        // (MSVC C2246).
        enum { kFreezeOff = 101 };

        juce::String freezeText() const
        {
            const int v = (int) freezeCpu.getValue();
            return v >= kFreezeOff ? juce::String ("Off") : juce::String (v) + " %";
        }

        FileSettingsComp()
        {
            static const char* names[4] = { "Dry", "Dry Cleaned", "Wet", "Wet Cleaned" };
            auto p = openUiPrefs();
            static const char* keys[4]  = { "fsWriteDry", "fsWriteDryCleaned",
                                            "fsWriteWet", "fsWriteWetCleaned" };
            for (int i = 0; i < 4; ++i)
            {
                boxes[i].setButtonText (names[i]);
                boxes[i].setToggleState (p->getBoolValue (keys[i], i == 0 || i == 2),
                                         juce::dontSendNotification);
                boxes[i].onClick = [this, i] { onBoxToggled (i); };
                addAndMakeVisible (boxes[i]);
            }
            strengthLbl.setText ("De-noise strength:", juce::dontSendNotification);
            addAndMakeVisible (strengthLbl);
            strength.addItem ("Light",  1);
            strength.addItem ("Strong", 2);
            strength.setSelectedId (p->getIntValue ("fsDenoiseStrength", (int) Denoise::Strong) == (int) Denoise::Light ? 1 : 2,
                                    juce::dontSendNotification);
            strength.onChange = [this] { save(); };
            addAndMakeVisible (strength);
            note.setText ("Take types written at record stop. At least one stays checked; "
                          "your Builder Grid Default pick is always written too.",
                          juce::dontSendNotification);
            note.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (note);

            // ── TS7 §8.1: auto-freeze threshold ──────────────────────────────
            freezeLbl.setText ("Auto-freeze above:", juce::dontSendNotification);
            addAndMakeVisible (freezeLbl);
            freezeCpu.setRange (0.0, (double) kFreezeOff, 1.0);
            // The snap itself lives in FreezeSlider::snapValue above; this
            // comment used to claim it while the only code under it was a
            // setSkewFactor(1.0) no-op.  Off sits one step past 100 so it cannot
            // be landed on by accident.
            freezeCpu.setValue ((double) p->getIntValue ("fsAutoFreezeCpu", 80),
                                juce::dontSendNotification);
            freezeCpu.setTooltip ("Freeze tracks automatically when CPU goes above this. "
                                  "0 always freezes; drag past 100 to switch it off");
            freezeCpu.onValueChange = [this]
            {
                freezeReadout.setText (freezeText(), juce::dontSendNotification);
                save();
            };
            addAndMakeVisible (freezeCpu);
            freezeReadout.setText (freezeText(), juce::dontSendNotification);
            freezeReadout.setJustificationType (juce::Justification::centredRight);
            addAndMakeVisible (freezeReadout);

            // ── TS7 §8.2: where captured takes live ──────────────────────────
            captureLbl.setText ("Keep captured takes:", juce::dontSendNotification);
            addAndMakeVisible (captureLbl);
            captureRetain.addItem ("This session only", 1);
            captureRetain.addItem ("In the project Reports folder", 2);
            captureRetain.setSelectedId (p->getIntValue ("fsCaptureRetain", 1),
                                         juce::dontSendNotification);
            captureRetain.onChange = [this] { save(); };
            addAndMakeVisible (captureRetain);

            // ── TS7 §3.1: the expensive half of a version ────────────────────
            // Off by default.  Analysis costs a few KB per take and is always
            // kept; audio is ~16 MB per minute, which is not a cost to impose on
            // someone who never asked to record anything.
            captureAudio.setButtonText ("Also keep the audio of each take");
            captureAudio.setTooltip ("Lets you play back and export a take, not "
                                     "just read its loudness.  About 16 MB per "
                                     "minute per take.");
            captureAudio.setToggleState (p->getBoolValue ("fsCaptureAudio", false),
                                         juce::dontSendNotification);
            captureAudio.onClick = [this] { save(); };
            addAndMakeVisible (captureAudio);

            // ── TS7 §6.9: the power-user escape hatch ────────────────────────
            // Sits beside the auto-freeze threshold because it modifies the same
            // feature: the slider decides WHEN the machine freezes for you, this
            // decides whether you can also do it by hand.  Off by default, so a
            // beginner never meets the concept.
            instrumentFreeze.setButtonText ("Enable Instrument Level Freeze");
            instrumentFreeze.setTooltip (
                "Adds a Freeze button to each player so you can freeze one "
                "instrument yourself.\n\nAutomatic freezing keeps working either "
                "way - this only shows the manual control.");
            instrumentFreeze.setToggleState (
                p->getBoolValue ("fsInstrumentFreeze", false),
                juce::dontSendNotification);
            instrumentFreeze.onClick = [this] { save(); };
            addAndMakeVisible (instrumentFreeze);

            setSize (400, 416);
        }

        void onBoxToggled (int i)
        {
            if (! boxes[0].getToggleState() && ! boxes[1].getToggleState()
                && ! boxes[2].getToggleState() && ! boxes[3].getToggleState())
                boxes[i].setToggleState (true, juce::dontSendNotification);
            save();
        }
        void save()
        {
            auto p = openUiPrefs();
            p->setValue ("fsWriteDry",        boxes[0].getToggleState());
            p->setValue ("fsWriteDryCleaned", boxes[1].getToggleState());
            p->setValue ("fsWriteWet",        boxes[2].getToggleState());
            p->setValue ("fsWriteWetCleaned", boxes[3].getToggleState());
            p->setValue ("fsDenoiseStrength", strength.getSelectedId() == 1
                                                ? (int) Denoise::Light : (int) Denoise::Strong);
            // TS7: machine preferences, deliberately NOT project data -- they
            // describe what this computer can cope with (see the plan's §6.8).
            p->setValue ("fsAutoFreezeCpu", (int) freezeCpu.getValue());
            p->setValue ("fsCaptureRetain", captureRetain.getSelectedId());
            p->setValue ("fsCaptureAudio",  captureAudio.getToggleState());
            p->setValue ("fsInstrumentFreeze", instrumentFreeze.getToggleState());
            p->saveIfNeeded();
            if (onCaptureSettingsChanged) onCaptureSettingsChanged();
        }
        void resized() override
        {
            auto b = getLocalBounds().reduced (12);
            for (int i = 0; i < 4; ++i)
                boxes[i].setBounds (b.removeFromTop (26));
            b.removeFromTop (8);
            auto row = b.removeFromTop (26);
            strengthLbl.setBounds (row.removeFromLeft (140));
            strength.setBounds (row.removeFromLeft (120));
            b.removeFromTop (10);
            {
                auto r = b.removeFromTop (26);
                freezeLbl.setBounds (r.removeFromLeft (140));
                freezeReadout.setBounds (r.removeFromRight (52));
                freezeCpu.setBounds (r.reduced (0, 2));
            }
            b.removeFromTop (6);
            {
                auto r = b.removeFromTop (26);
                captureLbl.setBounds (r.removeFromLeft (140));
                captureRetain.setBounds (r.removeFromLeft (210));
            }
            captureAudio.setBounds (b.removeFromTop (26));
            instrumentFreeze.setBounds (b.removeFromTop (26));
            b.removeFromTop (10);
            note.setBounds (b);
        }
    };

    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle            = "File Settings";
    opts.dialogBackgroundColour = VC::Bg;
    auto* comp = new FileSettingsComp();
    comp->onCaptureSettingsChanged = [this]
    {
        auto p = openUiPrefs();
        mVersionCapture.setAudioCaptureEnabled (p->getBoolValue ("fsCaptureAudio", false));
        mVersionCapture.setRetainInProject (p->getIntValue ("fsCaptureRetain", 1) == 2);
        // §6.9: the Freeze buttons appear/disappear on the spot.  A setting that
        // needs a restart to take effect reads as broken, and this one is the
        // only way to reach the feature at all.
        wireFreezeSlotForVisiblePage();
    };
    opts.content.setOwned (comp);
    opts.resizable              = false;
    opts.useNativeTitleBar      = false;   // TS7 §9.3
    opts.launchAsync();
}

void StandaloneEditor::pollDenoiseState()
{
    for (int i = 0; i < kDenoiseMaxVox; ++i)
    {
        const auto prefix = "mixer_vox_" + juce::String (i);
        auto* idxP = mProcessor.apvts.getRawParameterValue (prefix + "_inputChannelIdx");
        if (idxP == nullptr) continue;   // live-input params not registered yet

        const int idx = (int) idxP->load();
        if (idx != mVoxInputIdxLast[(size_t) i])
        {
            mVoxInputIdxLast[(size_t) i] = idx;
            // Reassignment restarts the learners (new room, new fingerprint).
            // The grid-default pick deliberately survives it (locks until the
            // project closes -- Jeff's Task-5 call).
            if (auto* vp = dynamic_cast<BaySickVocalProcessor*> (mProcessor.voxEngineAt (i)))
            {
                vp->resetDenoiseLearners();
                vp->setDenoiseLearnersEnabled (idx >= 0);
            }
        }
    }

    // QA-Fe2 PDC full-graph pass: re-solve every tick instead of watching
    // only the vox chains -- ANY latency source can now move the solution
    // (insert-rack bypass, engine switch, NAM/IR oversampling change,
    // De-reverb toggle, project load).  updateBusLatencies no-op-guards its
    // setDelay calls, so a steady-state tick costs a few hundred atomic
    // reads; the host report only refreshes on an actual change.  <= 200 ms
    // re-align lag at a live toggle -- inherent to toggling latent FX
    // mid-play.
    const int total = mProcessor.mVibeGraph.updateBusLatencies();
    if (total != mPdcTotalLast)
    {
        mPdcTotalLast = total;
        mProcessor.setLatencySamples (total);
    }
}

bool StandaloneEditor::regenerateDenoise (const juce::String& relPath, int strength)
{
    // Stop-gate (Jeff, 2026-07-16): the menu greys this during playback; the
    // hard guard covers every other entry path.
    if (DSPBase::isTransportPlaying())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
            "Regenerate De-noise", "Stop playback first.", "OK");
        return false;
    }
    const juce::File cleaned = mProcessor.resolveProjectFile (relPath);
    const juce::String name  = cleaned.getFileNameWithoutExtension();
    const bool isWet = name.endsWith (" - WET CLEANED");
    const bool isDry = name.endsWith (" - DRY CLEANED");
    if (! isWet && ! isDry) return false;

    const juce::String base = name.upToLastOccurrenceOf (isWet ? " - WET CLEANED"
                                                                : " - DRY CLEANED", false, false);
    const juce::File source = cleaned.getSiblingFile (base + (isWet ? " - WET.wav" : " - DRY.wav"));

    DenoiseProfile prof;
    if (auto* pair = mProcessor.findDenoiseProfiles (base))
        prof = isWet ? pair->second : pair->first;
    if (! prof.isValid() && source.existsAsFile())
        prof = Denoise::learnFromFile (source);      // pre-feature recording fallback

    juce::String err;
    if (! source.existsAsFile())
        err = "Source take " + source.getFileName() + " no longer exists.";
    else
        Denoise::cleanFile (source, cleaned, prof, (Denoise::Strength) strength, err);

    if (err.isNotEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
            "Regenerate De-noise",
            err + "\n\nIf this take is on the Builder grid, its file may be held "
                  "open by playback - remove the clip and try again.", "OK");
        return false;
    }
    mProcessor.rebuildAudioClipPlayers();
    if (mProjectManager) mProjectManager->markDirty();
    return true;
}

bool StandaloneEditor::renameRecordingGroup (const juce::String& oldBase,
                                             const juce::String& newBase)
{
    if (DSPBase::isTransportPlaying())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
            "Rename Group", "Stop playback first.", "OK");
        return false;
    }
    if (! mPM || newBase.isEmpty() || newBase == oldBase) return false;
    static const char* kTags[4] = { " - DRY", " - DRY CLEANED", " - WET", " - WET CLEANED" };

    struct MoveOp { juce::File from, to; juce::String oldRel, newRel; };
    std::vector<MoveOp> ops;
    for (auto* tag : kTags)
    {
        const juce::String oldRel = "Samples/" + oldBase + tag + ".wav";
        const juce::File   from   = mProcessor.resolveProjectFile (oldRel);
        if (! from.existsAsFile()) continue;
        const juce::File to = from.getSiblingFile (newBase + tag + ".wav");
        if (to.existsAsFile())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                "Rename Group",
                "A file named " + to.getFileName() + " already exists.", "OK");
            return false;
        }
        ops.push_back ({ from, to, oldRel, "Samples/" + newBase + tag + ".wav" });
    }
    if (ops.empty()) return false;

    // Order is load-bearing: (1) repoint every library/block reference at
    // the NEW paths, (2) rebuild players so the streamers release the OLD
    // files (new paths don't exist yet -> those clips skip for a moment),
    // (3) rename on disk, (4) rebuild again on the now-real files.
    for (const auto& op : ops)
        mPM->replaceAudioPath (op.oldRel, op.newRel);
    mProcessor.rebuildAudioClipPlayers();

    bool ok = true;
    for (size_t i = 0; i < ops.size(); ++i)
        if (! ops[i].from.moveFileTo (ops[i].to))
        {
            ok = false;
            for (size_t j = 0; j <= i; ++j)
                ops[j].to.moveFileTo (ops[j].from);   // roll back completed moves
            for (const auto& op : ops)
                mPM->replaceAudioPath (op.newRel, op.oldRel);
            break;
        }

    if (ok)
        mProcessor.renameDenoiseProfiles (oldBase, newBase);
    mProcessor.rebuildAudioClipPlayers();
    if (mProjectManager) mProjectManager->markDirty();
    if (! ok)
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
            "Rename Group",
            "Could not rename one of the takes (a file may be held open by "
            "playback).  All changes were rolled back.", "OK");
    return ok;
}

// ── R5d (2026-04-24): post-stop recording routing ───────────────────────────
void StandaloneEditor::commitRecordingResult (const VibeSynthProcessor::RecordResult& res)
{
    if (! mPM) return;

    // I-16 G-9 (2026-05-03): per-block routing decision.  Vox/Inst channelIds
    // produce a block linked to the originating page (block.routeChannel set;
    // playback fans through the page's chain).  Master / Audio-row files
    // (chId 0) keep the legacy "create new Audio row" behavior.
    //
    // For Vox: dry+wet both added to Audio Browser; only the WET file is
    // placed on the grid (per locked Option C of the spec).
    // For Inst: single dry file added to Browser AND placed on grid.
    auto dropWavAsClip = [&](const juce::File& wavFile, int routeChannel)
    {
        if (! wavFile.existsAsFile()) return;

        // Compute length in bars via file header (no re-read of audio data).
        double fileSeconds    = 0.0;
        double fileSampleRate = 44100.0;   // QA-Ea Task 0c: needed for pre-roll seconds
        {
            juce::AudioFormatManager fmt;
            fmt.registerBasicFormats();
            if (auto reader = std::unique_ptr<juce::AudioFormatReader> (
                    fmt.createReaderFor (wavFile)))
            {
                if (reader->sampleRate > 0.0)
                {
                    fileSampleRate = reader->sampleRate;
                    fileSeconds    = (double) reader->lengthInSamples / reader->sampleRate;
                }
            }
        }

        // QA-Ea Task 0c (FL pre-roll record + non-destructive clip trim):
        // compute the visible content length (file duration minus pre-roll
        // head).  The WAV contains the full pre-roll bar verbatim (no
        // transient slicing -- the rejected whole-block-gate proposal);
        // the visible clip on the grid starts at the song downbeat
        // (res.startBeat) and its length is the post-pre-roll content.
        // contentStartSamples on the block lets the audio render loop play
        // from sample N of the file rather than 0; slip-edit on the grid
        // lets the user later drag the left edge backward to reveal more
        // of the pre-roll if they want the early transient.
        const double preRollSeconds    = (fileSampleRate > 0.0)
            ? (double) res.preRollSamples / fileSampleRate
            : 0.0;
        const double effContentSeconds = fileSeconds - preRollSeconds;
        // QA-Ea Task 0c stop-during-count-in edge case: user pressed Stop
        // before transport started -- WAV is all count-in head, no useful
        // content (preRollSamples >= totalFileSamples).  WAV stays on disk
        // for reference; no Audio-Library entry + no grid clip.  Both
        // skipped so the browser doesn't accumulate empty-clip entries.
        if (effContentSeconds <= 0.0) return;

        const double bpm        = juce::jmax (20.0, mTransport ? mTransport->getBPM() : 120.0);
        // QA-Ea Task 0c: effContentBeats = the visible (post-pre-roll) beats.
        // Both lengthBars (ceil'd) and lengthBeats (sub-bar precision) reflect
        // the visible content, NOT the full file -- so the clip on the grid
        // ends exactly at the take's audible end.
        const double effContentBeats = effContentSeconds * bpm / 60.0;
        constexpr double kBeatsPerBar = 4.0;
        const int lengthBars = juce::jmax (1, (int) std::ceil (effContentBeats / kBeatsPerBar));
        const int startBar   = (int) std::floor (res.startBeat / kBeatsPerBar);

        // Find next free trackRow (scan existing blocks).
        int nextRow = 0;
        for (int i = 0; i < mPM->getNumBlocks(); ++i)
            nextRow = juce::jmax (nextRow, mPM->getBlock (i).trackRow + 1);

        ArrangementBlock block;
        block.clipType      = ClipType::Audio;
        block.trackRow      = nextRow;
        block.startBeats    = (double) startBar * 4.0;   // 8A: bar-truncated placement preserved
        block.lengthBars    = lengthBars;                           // ceil'd bar count (for bar-aligned UI)
        block.setLengthBeats (effContentBeats);                     // QA-Ea Task 0c: visible content beats
        block.patternIndex  = mPM->getCurrentPatternIndex();
        block.layerTrack    = false;
        block.audioFilePath = "Samples/" + wavFile.getFileName();   // relative to project
        block.originalBPM   = (float) bpm;
        block.stretchMode   = true;
        block.routeChannel  = routeChannel;                         // I-16 G-9: link to Vox/Inst page (0 = Audio row)
        // QA-Ea Task 0c: stamp the FL pre-roll content-start offset (in
        // file samples).  Zero when no count-in fired (existing pre-Task-0c
        // behavior unchanged).  Same value applies to master + every strip
        // block of this Record session per the strip-recorder scope (plan
        // SC line 120).  Audio engine adds this to every file-position read
        // in renderAudioClipsForRow / decodeFilePlayClip + finalizeFilePlayStrip (Component 5).
        block.contentStartSamples = res.preRollSamples;
        // 2026-04-24 recorded-clip library registration: matches what
        // BuilderPage::importAudioFile does on user drop so the clip shows
        // up in the Builder's Audio tab and survives save/close/reopen.
        // QA-E Task 4 (2026-05-12): tag the library entry's pageOwnerChannelId
        // with the same routeChannel so the browser groups Vox/Inst-routed
        // recordings under their originating page's category.  Master capture
        // / generic drops (routeChannel == 0) land in the generic Audio
        // category by default.
        mPM->addAudioToLibrary (block.audioFilePath, {}, routeChannel);
        mPM->addBlock (block);

        // I-16 G-9: only spin up a new Audio row + InsertNode + mixer strip
        // for non-routed clips (master capture, untagged files).  Vox/Inst-
        // routed clips play back through the originating page's chain via
        // PluginProcessor's audio-clip FilePlay branch -- no new strip needed.
        if (routeChannel == 0)
        {
            const juce::String stripName = "Audio " + juce::String (nextRow + 1);
            // 2026-04-29 ORDER FIX: register InsertNode + APVTS params BEFORE
            // creating the strip so setApvts can attach the fader/mute/etc.
            mProcessor.mVibeGraph.addAudioRowChannel (nextRow, stripName);
            mProcessor.ensureAudioInsert (nextRow, stripName);
            if (mMixerPage)
                mMixerPage->addAudioChannel (nextRow, stripName);
            if (mEffectsPage)
                mEffectsPage->rebuildChannelDropdown();
        }
    };

    // Master capture (no strips armed) -> Audio row, route channel 0.
    if (res.masterFile.existsAsFile())
        dropWavAsClip (res.masterFile, /*routeChannel=*/0);

    // I-16 G-9: per-strip files.  For Vox channelIds we have BOTH a dry and
    // a wet file; the WET one goes on the grid, both go in the Audio Browser.
    // Inst channelIds have only a dry file -- it goes on the grid.
    auto isVoxCh  = [] (int c) { return c >= MixerChannelIds::kVoxBase
                                       && c <  MixerChannelIds::kVoxBase + MixerChannelIds::kMaxVoxStrips; };
    auto isInstCh = [] (int c) { return c >= MixerChannelIds::kInstBase
                                       && c <  MixerChannelIds::kInstBase + MixerChannelIds::kMaxInstStrips; };

    auto findWet = [&] (int chId) -> juce::File
    {
        for (const auto& [c, f] : res.stripWetFiles)
            if (c == chId) return f;
        return {};
    };

    for (const auto& [chId, dryFile] : res.stripFiles)
    {
        if (isVoxCh (chId))
        {
            // QA-Fe2 De-noise: written take set = File Settings checkboxes
            // UNION the Builder Grid Default session pick; the pick lands on the grid,
            // the rest go browser-only; unselected source takes are deleted
            // (the checkboxes decide which files exist, per Jeff's spec).
            const int voxIdx = chId - MixerChannelIds::kVoxBase;
            const juce::File wetFile = findWet (chId);
            const bool haveWet = wetFile.existsAsFile();

            const auto fs = readFileTakeSettings();
            int pick = (voxIdx >= 0 && voxIdx < kDenoiseMaxVox)
                         ? mVoxTakePick[(size_t) voxIdx] : -1;

            bool want[4] = { fs.dry, fs.dryCleaned,
                             fs.wet && haveWet, fs.wetCleaned && haveWet };

            // AUTO pick = the HIGHEST-ORDER variant the user actually asked for
            // in File Settings (Jeff, 2026-07-30).  Order is
            // Dry < Dry Cleaned < Wet < Wet Cleaned, which is the enum order,
            // so "highest wanted index" IS the rule.
            //
            // Replaces the legacy "wet if it exists, else dry", which ignored
            // the checkboxes entirely -- so a user who ticked Wet Cleaned got
            // the file written and then the UNCLEANED wet take on the grid, and
            // the cleaned one sat unused in the browser.  That also mattered
            // more than it looked: both the pitch and align editors analyse
            // whatever is ON THE GRID, so the old rule quietly denied them the
            // cleaned audio the user had opted into.
            //
            // An explicit per-strip choice from the arm-LED menu still wins --
            // this only decides the -1 (auto) case.
            if (pick < 0)
            {
                pick = kTakeDry;   // floor: dry always exists
                for (int t = 3; t >= 0; --t)
                    if (want[t]) { pick = t; break; }
            }

            // Wet variants cannot be picked when no wet file was recorded
            // (realtime correction bypassed at record start -> no wet writer).
            if (! haveWet && pick >= kTakeWet)
                pick = (pick == kTakeWetCleaned) ? kTakeDryCleaned : kTakeDry;

            want[pick] = true;

            // Profiles: live learners first; a take recorded before the
            // learners warmed up self-learns from its own file (the method
            // the cleantake prototype validated).
            DenoiseProfile rawProf, wetProf;
            if (auto* vp = dynamic_cast<BaySickVocalProcessor*> (mProcessor.voxEngineAt (voxIdx)))
                vp->getDenoiseProfiles (rawProf, wetProf);
            if ((want[kTakeDryCleaned]) && ! rawProf.isValid())
                rawProf = Denoise::learnFromFile (dryFile);
            if ((want[kTakeWetCleaned] && haveWet) && ! wetProf.isValid())
                wetProf = Denoise::learnFromFile (wetFile);

            juce::String base = dryFile.getFileNameWithoutExtension();
            base = base.upToLastOccurrenceOf (" - DRY", false, false);
            if (rawProf.isValid() || wetProf.isValid())
                mProcessor.storeDenoiseProfiles (base, rawProf, wetProf);

            const auto strength = (Denoise::Strength) fs.strength;
            juce::File takes[4] = {
                dryFile,
                dryFile.getSiblingFile (base + " - DRY CLEANED.wav"),
                wetFile,
                haveWet ? wetFile.getSiblingFile (base + " - WET CLEANED.wav") : juce::File() };

            juce::String err;
            if (want[kTakeDryCleaned]
                && ! Denoise::cleanFile (dryFile, takes[kTakeDryCleaned], rawProf, strength, err))
            {
                want[kTakeDryCleaned] = false;
                if (pick == kTakeDryCleaned) pick = kTakeDry;
            }
            if (want[kTakeWetCleaned] && haveWet
                && ! Denoise::cleanFile (wetFile, takes[kTakeWetCleaned], wetProf, strength, err))
            {
                want[kTakeWetCleaned] = false;
                if (pick == kTakeWetCleaned) pick = kTakeWet;
            }
            want[pick] = true;   // a clean-failure fallback must still land

            for (int t = 0; t < 4; ++t)
            {
                if (takes[t] == juce::File() || ! takes[t].existsAsFile()) continue;
                if (! want[t])
                {
                    if (t == kTakeDry || t == kTakeWet)
                        takes[t].deleteFile();     // unselected source take
                    continue;
                }
                if (t == pick)
                    dropWavAsClip (takes[t], chId);
                else
                    mPM->addAudioToLibrary ("Samples/" + takes[t].getFileName(), {}, chId);
            }
        }
        else if (isInstCh (chId))
        {
            dropWavAsClip (dryFile, chId);         // single file on grid + linked to Inst page
        }
        else
        {
            // Unknown channel -> fall back to legacy Audio row behavior.
            dropWavAsClip (dryFile, /*routeChannel=*/0);
        }
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
        // #32 (QA-G3Smoke): drum recordings route PER NOTE through the trigger
        // bindings into pat.drumRolls[] (see the loop below) -- the legacy
        // shared pat.drumRoll is scheduler-dead (D1.2 reads drumRolls[] only)
        // and the D1.1 rescue migration never runs for a non-empty project,
        // so recordings were silent on playback AND permanently lost.  #33
        // falls out: the recorder no longer writes pat.drumRoll at all, so
        // the descending 51-midiNote migration is structurally unreachable
        // for recorder-written notes (it remains for true pre-D1.1 projects).
        const bool drumDemux = (mLastRollKind == LastRollKind::Drums);
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
                break;   // per-note routing below
            case LastRollKind::None:
                break;
        }
        if (target != nullptr || drumDemux)
        {
            // QA-Ea Task 0c (FL pre-roll record): shift captured MIDI notes
            // by the pre-roll offset (negative shift -> note startBeats now
            // measured from the song downbeat).  Then apply the three FL
            // rules locked 2026-05-19:
            //   (a) NOODLING discard -- if endBeat <= 0 the note both
            //       started AND ended before the downbeat (the user was
            //       practicing during count-in); drop it.
            //   (b) EARLY-STRIKE clamp -- if startBeat < 0 but endBeat > 0
            //       the user struck early but held through the downbeat;
            //       clamp startBeat to 0 and recompute durationBeats =
            //       endBeat - 0.  The hard wall is the downbeat -- no
            //       notes exist before beat 0 in the recorded pattern.
            //   (c) INPUT-QUANTIZE snap (param `Unified_RecordQuantizeDiv`, the
            //       shared 11-label snap scheme) -- when the division has a fixed
            //       tick grid (Bar..1/6 Step), snap the (post-clamp) startBeat to
            //       the nearest grid tick.  Applied AFTER the Early-Strike clamp
            //       so a clamped-to-0 note stays at 0.  Off (0) and Line (1) have
            //       no fixed grid -> no snap (raw timing kept).
            const double sampleRate    = mProcessor.getSampleRate();
            const double bpmForMidi    = juce::jmax (20.0, mTransport
                                                              ? mTransport->getBPM()
                                                              : 120.0);
            const double preRollBeats  = (sampleRate > 0.0)
                ? (double) res.preRollSamples * bpmForMidi / (60.0 * sampleRate)
                : 0.0;
            // Record-quantize grid in TICKS (96 PPQ).  snapDivToTicks maps the
            // FIXED divisions (Bar=384 .. 1/6 Step=4); Off (0) and Line (1)
            // return 0 -> no snap (Line has no fixed grid: there is no zoom
            // canvas at record-commit time, so raw timing is kept).
            int quantizeTicks = 0;
            if (auto* qDiv = mProcessor.apvts.getRawParameterValue ("Unified_RecordQuantizeDiv"))
                quantizeTicks = snapDivToTicks ((int) qDiv->load());

            for (auto n : res.midiNotes)   // mutable copy: shift + maybe clamp + maybe snap
            {
                n.startBeat -= preRollBeats;
                const double endBeat = n.startBeat + n.durationBeats;

                // (a) Noodling discard.
                if (endBeat <= 0.0) continue;

                // (b) Early-Strike clamp.
                if (n.startBeat < 0.0)
                {
                    n.startBeat     = 0.0;
                    n.durationBeats = endBeat;
                }

                // (c) Input quantize (snap startBeat to nearest grid tick).
                if (quantizeTicks > 0)
                {
                    const juce::int64 t = beatsToTicks (n.startBeat);
                    n.startBeat = ticksToBeats (((t + quantizeTicks / 2) / quantizeTicks) * quantizeTicks);
                }

                if (drumDemux)
                {
                    // #32: mirror the live dispatch -- the note lands in EVERY
                    // drum whose Note binding matches its number (the capture
                    // does not carry a channel, so channel-scoped bindings
                    // match any channel here), STAMPED at that drum's play
                    // note.  Unmatched notes fall back to the focused drum,
                    // pitch kept.
                    auto& trig    = mProcessor.getDrumTriggerMap();
                    bool  matched = false;
                    for (int di = 0; di < (int) pat.drumRolls.size(); ++di)
                    {
                        const auto b = trig.getBinding (di);
                        if (! b.isSet() || b.kind != DrumTriggerMap::Kind::Note) continue;
                        if (b.number != n.midiNote) continue;
                        auto stamped = n;
                        const int playNote = mProcessor.drumPlayNoteRT (di);
                        if (playNote >= 0) stamped.midiNote = playNote;
                        pat.drumRolls[(size_t) di].notes.push_back (stamped);
                        matched = true;
                    }
                    if (! matched && mLastRollIndex >= 0
                        && mLastRollIndex < (int) pat.drumRolls.size())
                        pat.drumRolls[(size_t) mLastRollIndex].notes.push_back (n);
                    continue;
                }
                target->notes.push_back (n);
            }
            if (drumDemux)
            {
                for (auto& dr : pat.drumRolls)
                    std::sort (dr.notes.begin(), dr.notes.end(),
                               [] (const PianoNote& a, const PianoNote& b)
                               { return a.startBeat < b.startBeat; });
                refreshAllKitViews();   // #32: recorded hits appear immediately
            }
            // #30b: recorded notes are a roll mutation OUTSIDE the grid's
            // onNotesChanged path -- publish the scheduler snapshot explicitly
            // (also fires onAnyChange -> markDirty).
            mPM->notifyContentChanged();
            if (mProjectManager) mProjectManager->markDirty();
        }
    }
}

void StandaloneEditor::refreshWindowTitle()
{
    // 2026-05-06 (Batch 9c B2): self-marshal to the message thread.
    //
    // Audio thread can land here via the engine dirty-hook chain:
    //   pushApvtsToDsp -> EffectRack::setSlotBypassed
    //                  -> rack.onSlotsChanged
    //                  -> ApvtsDirtyTracker::onAny  (or direct rack hook)
    //                  -> ProjectManager::markDirty
    //                  -> ProjectManager::onDirtyChanged
    //                  -> StandaloneEditor::refreshWindowTitle
    //                  -> DocumentWindow::setName
    //                  -> Win32 SetWindowText  <-- SYNCHRONOUS, blocks
    //                                              waiting for the message
    //                                              thread to pump.
    //
    // Calling SetWindowText from the audio thread can deadlock under load:
    // audio holds an EffectRack/Vox spinlock, message thread is waiting on
    // that lock while pumping, SetWindowText spins forever.  Marshal the
    // GUI mutation to the message thread and bail.  SafePointer guards
    // against the editor being torn down before the lambda runs.
    if (! juce::MessageManager::existsAndIsCurrentThread())
    {
        juce::Component::SafePointer<StandaloneEditor> safeThis (this);
        juce::MessageManager::callAsync ([safeThis]
        {
            if (safeThis) safeThis->refreshWindowTitle();
        });
        return;
    }

    juce::String title = "BaySickDAW";
    if (mProjectManager)
    {
        // QA-Ef #5 (2026-05-22): show "Untitled" + the dirty marker even when no
        // project folder has been opened/created yet, so a fresh-app-launch edit
        // (e.g. add an aux strip before doing File > New / Open) still surfaces
        // the unsaved indicator.  Previously the title only added " - name *"
        // inside the hasProject() branch, so markDirty fired but the user saw
        // no visible change on a fresh launch.
        if (mProjectManager->hasProject())
            title += " - " + mProjectManager->getCurrentName();
        else
            title += " - Untitled";
        if (mProjectManager->isDirty()) title += " *";
    }
    // QA-0a (2026-05-07): Debug builds append " [DEBUG]" so the user can
    // tell at a glance which exe is running.  Release builds are bit-for-bit
    // identical to before this change.
   #if JUCE_DEBUG
    title += " [DEBUG]";
   #endif
    if (auto* tlw = getTopLevelComponent())
        if (auto* dw = dynamic_cast<juce::DocumentWindow*> (tlw))
            dw->setName (title);
}
