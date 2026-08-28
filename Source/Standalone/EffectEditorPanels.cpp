#include "EffectEditorPanels.h"
#include "UndoBracket.h"
#include "EffectPresetIO.h"       // userNamPedalsDir / irDir chooser homes
#include "../ProjectFileResolver.h"   // persisted refs are project-relative in a bundle
#include "../Hosting/HostedPluginEffect.h"   // QA-ModelShell TS6: hosted plugin editor
// D.4 (2026-05-01): force MSBuild to recompile this file - Compressor + Delay
// knob additions were missing from the previous incremental build.
#include "../DSP/CompressorDSP.h"
#include "../DSP/ReverbDSP.h"
#include "../DSP/ChorusDSP.h"
#include "../DSP/DelayDSP.h"
#include "../DSP/SaturationDSP.h"
#include "../DSP/FlangerDSP.h"
#include "../DSP/OverdriveDSP.h"
#include "../DSP/PhaserDSP.h"
#include "../DSP/TransientShaperDSP.h"
#include "../DSP/LimiterDSP.h"
#include "../DSP/DeEsserDSP.h"
#include "../DSP/GateDSP.h"      // QA-Fe2 vocal-chain Gate
#include "../DSP/DeReverbDSP.h"  // QA-Fe2 vocal-chain De-reverb
// I-5 (2026-05-02): Harmonics drive pedals batch -- 4 new DSP classes.
#include "../DSP/BluesDriveStyleDSP.h"
#include "../DSP/DistortionStyleDSP.h"
#include "../DSP/FuzzStyleDSP.h"
#include "../DSP/HighGainStyleDSP.h"
// I-6 (2026-05-02): Harmonics bass pedals batch -- 2 multi-band drives.
#include "../DSP/BassDriverStyleDSP.h"
#include "../DSP/BassOverdriveStyleDSP.h"
// I-7 (2026-05-02): OC Style Octave.
#include "../DSP/OctaveStyleDSP.h"
// I-8 (2026-05-02): Dynamics pedals batch.
#include "../DSP/NoiseGateStyleDSP.h"
#include "../DSP/BassCompressorStyleDSP.h"
// I-9 (2026-05-03): SY Style Polyphonic Synth.
#include "../DSP/SynthStyleDSP.h"
// I-10 (2026-05-03): PW Style Wah.
#include "../DSP/WahStyleDSP.h"
// I-11 (2026-05-03): AD Style Acoustic Preamp.
#include "../DSP/AcousticPreampStyleDSP.h"
// I-11 (2026-05-03): AC Style Acoustic Simulator.
#include "../DSP/AcousticSimulatorStyleDSP.h"
// I-12 (2026-05-03): EQ trio batch.
#include "../DSP/GraphicEQStyleDSP.h"
#include "../DSP/BassGraphicEQStyleDSP.h"
#include "../DSP/FurmanEQStyleDSP.h"
// I-13 (2026-05-03): TU Style Tuner.
#include "../DSP/TunerStyleDSP.h"
// I-15c (2026-05-03): User NAM Pedal.
#include "../DSP/NAMPedalStyleDSP.h"
#include "../DSP/EffectParamMap.h"   // QA-ProjectSave Task 7 step 3: shared knob<->DSP math

// ── Helpers ───────────────────────────────────────────────────────────────────
struct KnobDef { const char* label; float min, max, def, step; const char* tip; };

static void buildKnobs(juce::Component& parent,
                       std::vector<std::unique_ptr<VKnob>>& knobs,
                       const std::vector<KnobDef>& defs)
{
    for (auto& d : defs)
    {
        auto k = std::make_unique<VKnob>(d.label, d.def, d.tip);
        k->slider.setRange(d.min, d.max, d.step);
        k->slider.setValue(d.def, juce::dontSendNotification);
        parent.addAndMakeVisible(*k);
        knobs.push_back(std::move(k));
    }
}

static constexpr int kKnobSz = 44;   // uniform knob size across all effect panels

static void layoutKnobsH(juce::Rectangle<int> b,
                          std::vector<std::unique_ptr<VKnob>>& knobs,
                          int maxSz = kKnobSz)
{
    if (knobs.empty()) return;
    int w = b.getWidth() / (int)knobs.size();
    int sz = juce::jmin(w, b.getHeight(), maxSz);
    for (auto& k : knobs)
        k->setBounds(b.removeFromLeft(w).withSizeKeepingCentre(sz, sz));
}

// QA-EffectsReview Task 4: overload that lays out a hand-picked list of knobs
// (raw pointers).  Basic/Advanced panels build the list of VISIBLE knobs for the
// current mode and reflow around the ones hidden in Basic -- the full-vector
// version above always reserves a slot per knob, so it can't skip individuals.
static void layoutKnobsH(juce::Rectangle<int> b,
                          const std::vector<VKnob*>& knobs,
                          int maxSz = kKnobSz)
{
    if (knobs.empty()) return;
    int w = b.getWidth() / (int)knobs.size();
    int sz = juce::jmin(w, b.getHeight(), maxSz);
    for (auto* k : knobs)
        if (k) k->setBounds(b.removeFromLeft(w).withSizeKeepingCentre(sz, sz));
}

// ── LabeledToggle retired ────────────────────────────────────────────────────
// All usages migrated to DualLabelToggle (see SharedUI.h). LabeledToggle was
// removed 2026-04-17 during the toggle/combo visual sweep.

// ── EditorPanelBase method implementations ────────────────────────────────────
EditorPanelBase::EditorPanelBase()
{
    // NO PER-PANEL VU (Jeff, 2026-08-11).  The input VU cost 120px of width on
    // every effect panel, which is most of what made the panels wide.  There is
    // ONE VU now and it is its own window, showing the master OUTPUT -- opened
    // from the effects rack menu next to VU Calibration.
    //
    // vuIn stays as a member and every panel's resized() still guards on it, so
    // the layouts simply stop reserving the strip; disableVU() also still works
    // for anything that calls it.  Nothing else had to change in 13 panels.

    // DBFS output meter - compact mode always on (floor -20 dBFS, labels truncated)
    dbfsOut = std::make_unique<DBFSMeter>();
    dbfsOut->setCompact(true);
    dbfsOut->setTooltip("Output level (dBFS)");
    addAndMakeVisible(*dbfsOut);

    outputVolKnob = std::make_unique<VKnob>("Vol", 0.0f, "Slot output gain (dB) - double-click resets to 0");
    outputVolKnob->slider.setRange(-24.0, 12.0, 0.1);
    outputVolKnob->slider.setValue(0.0, juce::dontSendNotification);
    outputVolKnob->slider.setDoubleClickReturnValue(true, 0.0);
    outputVolKnob->slider.onValueChange = [this] {
        if (onOutputGainChanged)
            onOutputGainChanged((float)outputVolKnob->slider.getValue());
    };
    outputVolKnob->slider.setLookAndFeel(&BaySickLAF::get());
    // Default to white variant; Dynamics panels call setVolumeKnobVariant(true)
    outputVolKnob->slider.getProperties().set("volumeKnob", juce::var("white"));
    addAndMakeVisible(*outputVolKnob);
}

EditorPanelBase::~EditorPanelBase()
{
    // mSyncKnobs holds raw VKnob* into vectors the DERIVED panel owns, and the
    // derived destructor has already run by the time we get here.  Stopping the
    // refresh timer first is what makes that window unreachable rather than
    // merely unlikely.
    if (mDisplaySync) mDisplaySync->stopTimer();
    mSyncKnobs.clear();

    if (outputVolKnob) outputVolKnob->slider.setLookAndFeel(nullptr);
    for (auto* c : getChildren())
        if (auto* btn = dynamic_cast<juce::Button*>(c))
            btn->setLookAndFeel(nullptr);
}

void EditorPanelBase::setUndoContext(const UndoContext& ctx)
{
    mUndoCtx = ctx;

    auto wireKnob = [this](VKnob* k)
    {
        if (!k) return;
        // Capture the slider via SafePointer so rack-action rebuilds (which
        // destroy the editor panel and its knobs) don't leave stale raw
        // pointers in queued FloatParamActions. When the slider is gone, the
        // apply lambda silently no-ops instead of dereferencing freed memory.
        juce::Component::SafePointer<juce::Slider> slSafe(&k->slider);
        juce::String  lbl = k->label.getText();
        k->onDragEnded = [this, slSafe, lbl](float before, float after)
        {
            if (!mUndoCtx.isValid()) return;
            if (juce::approximatelyEqual(before, after)) return;
            mUndoCtx.perform(new FloatParamAction(lbl, before, after,
                [slSafe](float v) mutable {
                    if (auto* s = slSafe.getComponent())
                        s->setValue((double)v, juce::sendNotification);
                }),
                lbl);
        };
    };

    for (auto& k : knobs) wireKnob(k.get());
    for (auto* k : getExtraKnobs()) wireKnob(k);
    wireKnob(outputVolKnob.get());
}

void EditorPanelBase::setSlotContext(const juce::String& channelPrefix, const juce::String& slotUuid)
{
    // C13: paramIds are keyed by the slot's stable UUID, not its position.
    // Reorder/pack-to-top swaps Slot structs (UUID travels with the effect)
    // so automation lanes stay valid.  Empty UUID = nothing to stamp.
    if (slotUuid.isEmpty()) return;

    // Build a base prefix: e.g. "layers_bus_{a1b2c3...}_"
    juce::String base = channelPrefix + "_" + slotUuid + "_";

    auto stampId = [&base](VKnob* k)
    {
        if (!k) return;
        // Convert label to lowercase snake_case: "Wet/Dry" -> "wet_dry"
        juce::String id = k->label.getText()
                           .toLowerCase()
                           .replaceCharacter(' ', '_')
                           .replaceCharacter('/', '_');
        k->paramId = base + id;
    };

    // Stamp each DSP knob in the base-class vector
    for (int i = 0; i < (int)knobs.size(); ++i)
        stampId(knobs[i].get());

    // QA-ManualPress M-4c: the manual's "a panel knob" / "knob caption"
    // callouts (FX-3 / FX-4) anchor to the first knob every panel builds, so
    // the dots follow whatever panel the figure is shot from.
    if (! knobs.empty() && knobs[0] != nullptr)
    {
        knobs[0]->getProperties().set (kDotAnchor, "FX-3");
        knobs[0]->label.getProperties().set (kDotAnchor, "FX-4");
    }

    // Panels that keep knobs in their own row vectors (r1knobs/r2knobs) expose
    // them via getExtraKnobs(). Stamp those too so paramIds are complete.
    for (auto* k : getExtraKnobs())
        stampId(k);

    // Output vol knob
    if (outputVolKnob)
    {
        outputVolKnob->paramId = base + "output_vol";
        outputVolKnob->getProperties().set (kDotAnchor, "FX-6");   // M-4c
    }

    // M-4: the panel's output dBFS bar.  Null on pedal-native panels, which
    // strip it -- and those are not what the Effects Panel figure shoots.
    if (dbfsOut)
        dbfsOut->getProperties().set (kDotAnchor, "FX-7");

    // Task 9: automatable toggles (addAutomatableToggle).  ComponentID goes on
    // the wrapper AND every child -- GlobalAutoRightClick reads the clicked
    // component's id directly, and a click can land on the switch button or a
    // forwarding label.
    for (auto& t : mAutoToggles)
    {
        if (t.tog == nullptr) continue;
        const juce::String pid = base + t.suffix;
        t.tog->setComponentID(pid);
        if (&t == &mAutoToggles.front())
            t.tog->getProperties().set (kDotAnchor, "FX-5");   // M-4c: "a switch toggle"
        for (auto* child : t.tog->getChildren())
            if (child) child->setComponentID(pid);
    }

    // Build the display-refresh list from the controls just stamped, then start
    // it.  Suffix is re-derived from the paramId's tail rather than re-walking
    // the labels, so it is the SAME string the registry and EffectParamMap use.
    mSyncKnobs.clear();
    auto collect = [this, &base](VKnob* k)
    {
        if (k == nullptr || k->paramId.isEmpty()) return;
        mSyncKnobs.push_back ({ k, k->paramId.substring (base.length()) });
    };
    for (auto& k : knobs) collect (k.get());
    for (auto* k : getExtraKnobs()) collect (k);
    collect (outputVolKnob.get());

    if (mDisplaySync == nullptr)
    {
        mDisplaySync = std::make_unique<DisplaySync>();
        mDisplaySync->tick = [this] { refreshControlsFromDsp(); };
    }
    mDisplaySync->startTimerHz (10);
}

void EditorPanelBase::refreshControlsFromDsp()
{
    // Cheap early-out: a panel the user cannot see has nothing to keep in sync,
    // and only one rack panel is on screen at a time.
    if (mDsp == nullptr || ! isShowing()) return;

    for (auto& [k, suffix] : mSyncKnobs)
    {
        if (k == nullptr) continue;
        auto& sl = k->slider;
        if (sl.isMouseButtonDown()) continue;   // never fight a live drag

        // output_vol is the one stamped control that is NOT a DSP parameter --
        // it writes the RACK's per-slot gain, which EffectParamMap does not
        // cover -- so it has no read-back here and keeps its own value.
        const float cur = (float) sl.getValue();
        const float now = EffectParamMap::readNatural (mEffectType, mVariant, mDsp,
                                                       suffix, cur);
        if (! juce::approximatelyEqual (cur, now))
            sl.setValue ((double) now, juce::dontSendNotification);
    }

    for (auto& t : mAutoToggles)
    {
        if (t.tog == nullptr) continue;
        const float v = EffectParamMap::readNatural (mEffectType, mVariant, mDsp,
                                                     t.suffix, -1.0f);
        if (v < 0.0f) continue;   // not a mapped toggle
        auto& b = t.tog->btn();
        const bool on = (v >= 0.5f);
        if (b.getToggleState() != on)
            b.setToggleState (on, juce::dontSendNotification);
    }
}

void EditorPanelBase::addAutomatableToggle(DualLabelToggle& tog, const juce::String& paramSuffix)
{
    mAutoToggles.push_back({ &tog, paramSuffix });
}

void EditorPanelBase::setVolumeKnobVariant(bool dark)
{
    outputVolKnob->slider.getProperties().set("volumeKnob",
        dark ? juce::var("black") : juce::var("white"));
}

void EditorPanelBase::childrenChanged()
{
    // Whenever a child is added, if it's a toggle-state button, force BaySickLAF so
    // drawButtonBackground in BaySickLAF (switch-toggle filmstrip) is always called.
    // 2026-04-26: also opt in to "switchToggle" property - switch styling is
    // gated to opt-in callsites and effect panels qualify per the rule.
    for (auto* c : getChildren())
        if (auto* btn = dynamic_cast<juce::Button*>(c))
            if (btn->getClickingTogglesState())
            {
                if (&btn->getLookAndFeel() != &BaySickLAF::get())
                    btn->setLookAndFeel(&BaySickLAF::get());
                btn->getProperties().set("switchToggle", true);
            }
}

void EditorPanelBase::disableVU()
{
    // Panels without a VU input meter (Delay, Reverb, Chorus, Flanger, Phaser)
    // call this so knobs fill the full width between the left edge and DBFS+fader block.
    if (vuIn)
    {
        removeChildComponent(vuIn.get());
        vuIn.reset();
    }
}

void EditorPanelBase::disableOutputVolKnob()
{
    // I-4 (2026-05-02): pedal-style panels (CS Style Compressor + I-5+ new
    // pedals) own their own Level / Output knobs and don't need the base-
    // class right-edge "Output Vol" knob.  Removing it lets derived layouts
    // reclaim the horizontal slot.  The dBFS meter stays.
    if (outputVolKnob)
    {
        removeChildComponent (outputVolKnob.get());
        outputVolKnob.reset();
    }
}

void EditorPanelBase::disableDbfsMeter()
{
    // I-15 (2026-05-03): pedal-mode finalize -- pedal tiles render no level
    // meters per locked spec.  We hide rather than delete so existing panel
    // resized() bodies (which call dbfsOut->setBounds without null guards)
    // continue to work cleanly; the 32-px column is reserved but draws as
    // dark background so the visual matches the spec.
    if (dbfsOut)
        dbfsOut->setVisible (false);
}

void EditorPanelBase::setInputLevel(float rms01)  { if (vuIn)    vuIn->setLevel(rms01); }
void EditorPanelBase::setOutputLevel(float dbfs)  { if (dbfsOut) dbfsOut->setLevel(dbfs); }

void EditorPanelBase::resized()
{
    auto b = getLocalBounds().reduced(2, 4);

    // Left: VU input meter (only for panels that have one)
    if (vuIn)
        vuIn->setBounds(b.removeFromLeft(120).reduced(1, 2));

    // Right: vol knob + DBFS meter
    dbfsOut      ->setBounds(b.removeFromRight(32).reduced(1, 2));
    b.removeFromRight(2);
    // I-4 (2026-05-02): outputVolKnob may have been freed by
    // disableOutputVolKnob() (pedal-style panels own their own Level knob).
    if (outputVolKnob)
    {
        outputVolKnob->setBounds(b.removeFromRight(kKnobSz).withSizeKeepingCentre(kKnobSz, kKnobSz));
        b.removeFromRight(4);
    }

    // Center: combo, toggles, knobs
    juce::Rectangle<int> comboR;
    if (combo)
    {
        comboR = b.removeFromRight(72).withSizeKeepingCentre(68, 22);
        b.removeFromRight(4);
    }
    for (int i = (int)toggles.size() - 1; i >= 0; --i)
    {
        toggles[i]->setBounds(b.removeFromRight(94).withSizeKeepingCentre(90, 40));
        b.removeFromRight(2);
    }
    layoutKnobsH(b, knobs);
    if (combo) combo->setBounds(comboR);
}

// ─────────────────────────────────────────────────────────────────────────────
// CompressorPanel
// Thresh | Ratio | Gain | Attack | Release  +  KneeType combo (8 types)
// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
// FETCompressorPanel - H-7 (2026-05-01)
// 1176-style minimalist layout: Input + Output + Attack + Release + 4-position
// Ratio chickenhead (4:1 / 8:1 / 12:1 / 20:1) + GR meter.  Selected when the
// underlying CompressorDSP's mType == FET.
// ─────────────────────────────────────────────────────────────────────────────
struct FETCompressorPanel : public EditorPanelBase, public juce::Timer
{
    std::unique_ptr<ChickenHeadSelector> ratioSel;     // 4 / 8 / 12 / 20 / All-in
    std::unique_ptr<ChickenHeadSelector> meterSel;     // GR / +8 / +4 / OFF
    std::unique_ptr<GRMeter>             grMeter;      // GR display widget
    CompressorDSP*                       mDsp { nullptr };
    int                                  mMeterMode { 0 };   // 0=GR

    void disableGrMeter() override
    {
        if (grMeter) { removeChildComponent (grMeter.get()); grMeter.reset(); }
    }

    // 1176 datasheet attack/release tables.  Position 0 = OFF (bypass that
    // stage), 1..7 are real positions with 1 = slowest, 7 = fastest.
    static constexpr float kAttackMs[8]  = { 0.f, 0.8f, 0.5f, 0.3f, 0.15f, 0.075f, 0.035f, 0.020f };
    static constexpr float kReleaseMs[8] = { 0.f, 1100.f, 700.f, 450.f, 250.f, 150.f, 90.f, 50.f };

    explicit FETCompressorPanel (CompressorDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&DynamicsLAF::get());
        setVolumeKnobVariant (true);

        // Input + Output: continuous dB attenuators (1176 face plate).  Range
        // -60..0 dB; on the face plate "0" = max signal at top, "-∞" at bottom.
        // Attack + Release stored as integer position 0..7 (0=OFF) -- mapped to
        // ms via kAttackMs / kReleaseMs in the onValueChange callbacks.
        buildKnobs (*this, knobs, {
            { "Input",   -60.f,   0.f, -12.f, 0.5f, "Input drive (FET style): no threshold knob -- Input sets how hard the signal hits the fixed threshold, so MORE input (turn up) = MORE compression. The default is a gentle setting." },
            { "Output",  -30.f,  30.f,  12.f, 0.5f, "Output / makeup gain (dB): brings the level back up after compression. Up = more makeup (max +30); 0 dB = unity; down = attenuate. Default +12. Set it to match the bypassed level." },
            { "Attack",   0.f,    7.f,   4.f,  1.f, "Attack position (0=OFF, 1=slow 800us, 7=fast 20us)" },
            { "Release",  0.f,    7.f,   4.f,  1.f, "Release position (0=OFF, 1=slow 1100ms, 7=fast 50ms)" },
        });
        for (auto& k : knobs)
            k->slider.getProperties().set (DynamicsLAF::kKnobVariant, "modernAnalog");

        // Ratio chickenhead: 4 / 8 / 12 / 20 / All (all-buttons-in mode).
        ratioSel = std::make_unique<ChickenHeadSelector>();
        ratioSel->setOptions ({
            { "4",   "4 : 1",        "Mild compression" },
            { "8",   "8 : 1",        "Medium compression" },
            { "12",  "12 : 1",       "Heavy compression" },
            { "20",  "20 : 1",       "Limiting / heavy" },
            { "All", "All-buttons in", "All-buttons-in mode -- program-dependent rising ratio (climbs with level) plus extra grit; the aggressive crush character" },
        });
        ratioSel->setBodyTooltip ("Ratio (FET-style face plate)");
        ratioSel->setDefaultLabelColour (juce::Colours::black);
        const float r = dsp ? dsp->ratio : 4.0f;
        // All-buttons round-trips via the flag; legacy presets (no flag) stored a
        // high sentinel ratio, so r > 25 also reads as all-buttons (idx 4).
        const int initIdx = ((dsp && dsp->fetAllButtons) || r > 25.0f) ? 4
                          : (r < 6.f)  ? 0
                          : (r < 10.f) ? 1
                          : (r < 16.f) ? 2 : 3;
        ratioSel->setSelectedIndex (initIdx, juce::dontSendNotification);
        ratioSel->onChange = [dsp] (int idx)
        {
            // idx 0-3 = fixed 4/8/12/20:1 buttons; idx 4 = all-buttons-in, which
            // sets a serialized flag (the gain computer then uses the program-
            // dependent rising-ratio curve).  The flag -- not a magic ratio
            // number -- is what round-trips on reload (see initIdx).
            idx = juce::jlimit (0, 4, idx);
            if (! dsp) return;
            const bool allButtons = (idx == 4);
            dsp->setFetAllButtons (allButtons);
            const float ratios[] = { 4.f, 8.f, 12.f, 20.f };
            dsp->setRatio (allButtons ? 20.0f : ratios[idx]);   // all-buttons: nominal fallback; the curve overrides
        };
        addAndMakeVisible (*ratioSel);

        // Meter mode chickenhead: GR / +8 / +4 / OFF.
        meterSel = std::make_unique<ChickenHeadSelector>();
        meterSel->setOptions ({
            { "GR",  "Gain Reduction", "VU shows gain reduction (dB)" },
            { "+8",  "Output +8",      "VU shows output level (calibrated to +8 dBu)" },
            { "+4",  "Output +4",      "VU shows output level (calibrated to +4 dBu)" },
            { "OFF", "Off",            "Meter off" },
        });
        meterSel->setBodyTooltip ("Meter mode");
        meterSel->setDefaultLabelColour (juce::Colours::black);
        meterSel->setSelectedIndex (0, juce::dontSendNotification);
        meterSel->onChange = [this] (int idx) { mMeterMode = idx; };
        addAndMakeVisible (*meterSel);

        // Knob -> DSP wiring.  QA-ProjectSave Task 7 step 3 (2026-07-26): the
        // mapping math moved to EffectParamMap so automation can reach the DSP
        // without a panel in the chain, and so this and the automation
        // applicator cannot drift -- the Input drive inversion and the
        // Attack/Release position tables now have exactly one definition.
        static const char* const kSuffix[] = { "input", "output", "attack", "release" };
        for (int i = 0; i < 4; ++i)
        {
            auto* sl = &knobs[(size_t) i]->slider;
            const char* suffix = kSuffix[i];
            sl->onValueChange = [dsp, sl, suffix]
            {
                EffectParamMap::applyNatural (EffectType::Compressor,
                                              (int) CompressorDSP::Type::FET, dsp,
                                              suffix, (float) sl->getValue());
            };
        }

        if (dsp)
        {
            // Same table, read direction: put each knob where the DSP actually
            // is.  Previously the reverse map was written out a second time here.
            for (int i = 0; i < 4; ++i)
            {
                const auto* def = EffectParamMap::find (EffectType::Compressor,
                                                       (int) CompressorDSP::Type::FET,
                                                       kSuffix[i]);
                if (def == nullptr) continue;
                knobs[(size_t) i]->slider.setValue ((double) def->read (dsp),
                                                    juce::sendNotificationSync);
            }
        }

        grMeter = std::make_unique<GRMeter>();
        addAndMakeVisible (*grMeter);
        startTimerHz (30);
    }

    ~FETCompressorPanel() override { stopTimer(); setLookAndFeel (nullptr); }

    void timerCallback() override
    {
        // QA-Layout T13 crash fix: liveDsp() returns mDsp ONLY while the rack
        // still hands back that same pointer for this slot, so this one guard
        // makes every mDsp-> below safe.  A panel timer and its window's rebuild
        // poll are independent; without this the timer can tick on a DSP a
        // project load already destroyed.  See EditorPanelBase::liveDsp.
        if (liveDsp() == nullptr) return;
        if (grMeter && mDsp)
        {
            // Mode 0 = GR display.  Modes 1/2 (+8 / +4 output) and mode 3 (OFF)
            // route through the same widget; output-level routing lands in a
            // follow-up batch (needs DSP-side post-makeup peak), so for now
            // those modes display "--" by zeroing the GR widget.
            const float val = (mMeterMode == 0) ? mDsp->getGainReductionDb() : 0.0f;
            grMeter->setGainReduction (val);
        }
    }

    void paint (juce::Graphics& g) override
    {
        DynamicsLAF::paintLA2APanel (g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);

        // VU input meter + GR meter strip on the LEFT (matches CompressorPanel).
        if (vuIn) vuIn->setBounds (b.removeFromLeft (120).reduced (1, 2));
        if (grMeter)
        {
            int gw = juce::jmin (96, b.getHeight() + 8);
            grMeter->setBounds (b.removeFromLeft (gw).reduced (1, 2));
            b.removeFromLeft (4);
        }

        // Right edge: dBFS meter + output-vol knob.
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (2);
        outputVolKnob->setBounds (b.removeFromRight (kKnobSz).withSizeKeepingCentre (kKnobSz, kKnobSz));
        b.removeFromRight (4);

        // Right side: meter-mode chickenhead.
        if (meterSel)
        {
            auto col = b.removeFromRight (66);
            meterSel->setBounds (col.reduced (2));
            b.removeFromRight (4);
        }

        // Knob strip middle: Input | Output | Attack | Release | Ratio.
        const int n     = 5;
        const int slotW = juce::jmax (1, b.getWidth() / n);
        const int sz    = juce::jmin (slotW, b.getHeight(), kKnobSz);
        knobs[0]->setBounds (b.removeFromLeft (slotW).withSizeKeepingCentre (sz, sz));
        knobs[1]->setBounds (b.removeFromLeft (slotW).withSizeKeepingCentre (sz, sz));
        knobs[2]->setBounds (b.removeFromLeft (slotW).withSizeKeepingCentre (sz, sz));
        knobs[3]->setBounds (b.removeFromLeft (slotW).withSizeKeepingCentre (sz, sz));
        if (ratioSel) ratioSel->setBounds (b.reduced (2));
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// OptoCompressorPanel - H-7 (2026-05-01)
// LA-2A-style minimalist: Peak Reduction + Gain + Comp/Limit toggle + GR meter.
// Selected when the underlying CompressorDSP's mType == Opto.
// ─────────────────────────────────────────────────────────────────────────────
struct OptoCompressorPanel : public EditorPanelBase, public juce::Timer
{
    std::unique_ptr<DualLabelToggle>     compLimitTog;   // Comp = 3:1, Limit = inf:1
    std::unique_ptr<ChickenHeadSelector> meterSel;        // GR / Output +10 / Output +4
    std::unique_ptr<GRMeter>             grMeter;
    CompressorDSP*                       mDsp { nullptr };
    int                                  mMeterMode { 0 };  // 0=GR

    void disableGrMeter() override
    {
        if (grMeter) { removeChildComponent (grMeter.get()); grMeter.reset(); }
    }

    explicit OptoCompressorPanel (CompressorDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&DynamicsLAF::get());
        setVolumeKnobVariant (true);

        // LA-2A face plate: Peak Reduction + Gain knobs both displayed 0..100.
        // Internal mapping inside the onValueChange lambdas converts that
        // 0..100 user-facing scale to the DSP's threshold/gain dB.
        buildKnobs (*this, knobs, {
            { "Peak Reduction", 0.f, 100.f, 30.f, 0.5f, "Peak Reduction -- more = more compression (Opto-style face plate scale)" },
            { "Gain",           0.f, 100.f, 50.f, 0.5f, "Output gain (Opto-style face plate scale)" },
        });
        for (auto& k : knobs)
            k->slider.getProperties().set (DynamicsLAF::kKnobVariant, "modernAnalog");

        // Comp/Limit 2-position vertical toggle, top-left of the panel.
        compLimitTog = std::make_unique<DualLabelToggle>();
        compLimitTog->setupNamed (
            "Comp",  "Compress mode (3:1 ratio)",
            "Limit", "Limit mode (effectively infinity:1 -- Opto 'limit' position)");
        compLimitTog->btn().setToggleState (dsp && dsp->optoLimit, juce::dontSendNotification);
        compLimitTog->btn().onClick = [dsp, this]
        {
            const bool limit = compLimitTog->btn().getToggleState();
            if (dsp) dsp->setOptoLimit (limit);
        };
        addAndMakeVisible (*compLimitTog);

        // Meter mode chickenhead: GR / Output +10 / Output +4.
        meterSel = std::make_unique<ChickenHeadSelector>();
        meterSel->setOptions ({
            { "GR",  "Gain Reduction", "VU shows gain reduction (dB)" },
            { "+10", "Output +10",     "VU shows output level (calibrated to +10 dBu)" },
            { "+4",  "Output +4",      "VU shows output level (calibrated to +4 dBu)" },
        });
        meterSel->setBodyTooltip ("Meter mode");
        meterSel->setDefaultLabelColour (juce::Colours::black);
        meterSel->setSelectedIndex (0, juce::dontSendNotification);
        meterSel->onChange = [this] (int idx) { mMeterMode = idx; };
        addAndMakeVisible (*meterSel);

        // Slider value 0..100 -> DSP dB mapping.
        knobs[0]->slider.onValueChange = [dsp,this]
        {
            // Peak Reduction 0..100 -> threshold 0 dB .. -40 dB.
            const float v01 = (float) knobs[0]->slider.getValue() * 0.01f;
            if (dsp) dsp->setThreshold (juce::jmap (v01, 0.0f, 1.0f, 0.0f, -40.0f));
        };
        knobs[1]->slider.onValueChange = [dsp,this]
        {
            // Gain 0..100 -> output -inf..+30 dB (linear in dB above -inf).
            // Use 0 = -30 dB (effectively quiet) so the knob has musical range.
            const float v01 = (float) knobs[1]->slider.getValue() * 0.01f;
            if (dsp) dsp->setGain (juce::jmap (v01, 0.0f, 1.0f, -30.0f, 30.0f));
        };

        if (dsp)
        {
            // Reverse map current threshold -> 0..100 face plate.
            const float thr01  = juce::jlimit (0.0f, 1.0f,
                                                juce::jmap (dsp->threshold, 0.0f, -40.0f, 0.0f, 1.0f));
            const float gain01 = juce::jlimit (0.0f, 1.0f,
                                                juce::jmap (dsp->makeupDb, -30.0f, 30.0f, 0.0f, 1.0f));
            knobs[0]->slider.setValue (thr01  * 100.0, juce::sendNotificationSync);
            knobs[1]->slider.setValue (gain01 * 100.0, juce::sendNotificationSync);
        }

        grMeter = std::make_unique<GRMeter>();
        addAndMakeVisible (*grMeter);
        startTimerHz (30);
    }

    ~OptoCompressorPanel() override { stopTimer(); setLookAndFeel (nullptr); }

    void timerCallback() override
    {
        // QA-Layout T13 crash fix: liveDsp() returns mDsp ONLY while the rack
        // still hands back that same pointer for this slot, so this one guard
        // makes every mDsp-> below safe.  A panel timer and its window's rebuild
        // poll are independent; without this the timer can tick on a DSP a
        // project load already destroyed.  See EditorPanelBase::liveDsp.
        if (liveDsp() == nullptr) return;
        if (grMeter && mDsp)
        {
            // GR mode: real GR.  +10 / +4 modes wired but display "--" until
            // output-level routing lands in a follow-up.
            const float val = (mMeterMode == 0) ? mDsp->getGainReductionDb() : 0.0f;
            grMeter->setGainReduction (val);
        }
    }

    void paint (juce::Graphics& g) override
    {
        DynamicsLAF::paintLA2APanel (g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);

        // VU + GR cluster on the left (matches CompressorPanel).
        if (vuIn) vuIn->setBounds (b.removeFromLeft (120).reduced (1, 2));
        if (grMeter)
        {
            int gw = juce::jmin (96, b.getHeight() + 8);
            grMeter->setBounds (b.removeFromLeft (gw).reduced (1, 2));
            b.removeFromLeft (4);
        }

        // Right edge: dBFS meter + output-vol knob.
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (2);
        outputVolKnob->setBounds (b.removeFromRight (kKnobSz).withSizeKeepingCentre (kKnobSz, kKnobSz));
        b.removeFromRight (4);

        // Meter mode chickenhead (top-right on the LA-2A face plate).
        if (meterSel)
        {
            auto col = b.removeFromRight (70);
            meterSel->setBounds (col.reduced (2));
            b.removeFromRight (4);
        }

        // Compress/Limit toggle (top-left on the LA-2A face plate).
        if (compLimitTog)
        {
            auto col = b.removeFromLeft (60);
            compLimitTog->setBounds (col.reduced (2));
            b.removeFromLeft (4);
        }

        // Two big knobs (Peak Reduction + Gain) fill the rest.
        const int n     = 2;
        const int slotW = juce::jmax (1, b.getWidth() / n);
        const int sz    = juce::jmin (slotW, b.getHeight(), kKnobSz + 12);
        knobs[0]->setBounds (b.removeFromLeft (slotW).withSizeKeepingCentre (sz, sz));
        knobs[1]->setBounds (b.removeFromLeft (slotW).withSizeKeepingCentre (sz, sz));
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// CSStyleCompressorPanel - I-4 (2026-05-02)
// BOSS CS-Style sustain pedal layout.  4 user knobs: Level / Tone / Attack /
// Sustain.  Selected when the underlying CompressorDSP's mType == CS Style.
// QA-EffectsReview Task 6 (CS-3 fidelity): Sustain = pre-amp INTO a fixed -24 dB
// threshold (more drive = harder squash + sustain); Tone = treble high-shelf only
// (12 o'clock flat); Level is the post-EQ output trim; ratio is fixed 10:1 and
// Release follows Attack inversely (both set by setType / setAttack).
// ─────────────────────────────────────────────────────────────────────────────
struct CSStyleCompressorPanel : public EditorPanelBase, public juce::Timer
{
    std::unique_ptr<GRMeter>      grMeter;
    CompressorDSP*                mDsp { nullptr };

    void disableGrMeter() override
    {
        if (grMeter) { removeChildComponent (grMeter.get()); grMeter.reset(); }
    }

    explicit CSStyleCompressorPanel (CompressorDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&DynamicsLAF::get());
        setVolumeKnobVariant (true);

        // I-4 (2026-05-02): pedal-style panels own their own output Level
        // knob; drop the base-class right-edge "Output Vol" knob so the
        // 4 face-plate knobs claim the freed horizontal space.
        disableOutputVolKnob();

        // Level   -- output trim, -12..+12 dB (csLevelDb).
        // Tone    -- 0..1, center 0.5 = flat (csTone01); treble high-shelf only.
        // Attack  -- 1..50 ms (attackMs); also sets release inversely (CS-3).
        // Sustain -- 0..1 macro (csSustain01) -> input drive into fixed threshold.
        buildKnobs (*this, knobs, {
            { "Level",   -12.f,  12.f,   0.f, 0.1f, "Output level / makeup trim (dB)" },
            { "Tone",      0.f,   1.f,  0.5f, 0.01f, "Tone (treble): right = boost highs, left = cut highs, center (12 o'clock) = flat" },
            { "Attack",    1.f,  50.f,  10.f, 0.1f, "Attack time (ms) -- also sets release inversely (fast = more sustain, slow = punchier)" },
            { "Sustain",   0.f,   1.f,   0.f, 0.01f, "Sustain: drives the signal harder into the comp -- more = more compression + sustain" },
        });
        for (auto& k : knobs)
            k->slider.getProperties().set (DynamicsLAF::kKnobVariant, "modernAnalog");

        // Knob -> DSP wiring.
        knobs[0]->slider.onValueChange = [dsp, this]
        {
            if (dsp) dsp->setCsLevel ((float) knobs[0]->slider.getValue());
        };
        knobs[1]->slider.onValueChange = [dsp, this]
        {
            if (dsp) dsp->setCsTone ((float) knobs[1]->slider.getValue());
        };
        knobs[2]->slider.onValueChange = [dsp, this]
        {
            if (dsp) dsp->setAttack ((float) knobs[2]->slider.getValue());
        };
        knobs[3]->slider.onValueChange = [dsp, this]
        {
            if (dsp) dsp->setCsSustain ((float) knobs[3]->slider.getValue());
        };

        if (dsp)
        {
            knobs[0]->slider.setValue (dsp->csLevelDb,   juce::sendNotificationSync);
            knobs[1]->slider.setValue (dsp->csTone01,    juce::sendNotificationSync);
            knobs[2]->slider.setValue (dsp->attackMs,    juce::sendNotificationSync);
            knobs[3]->slider.setValue (dsp->csSustain01, juce::sendNotificationSync);
        }

        grMeter = std::make_unique<GRMeter>();
        addAndMakeVisible (*grMeter);
        startTimerHz (30);
    }

    ~CSStyleCompressorPanel() override { stopTimer(); setLookAndFeel (nullptr); }

    void timerCallback() override
    {
        // QA-Layout T13 crash fix: liveDsp() returns mDsp ONLY while the rack
        // still hands back that same pointer for this slot, so this one guard
        // makes every mDsp-> below safe.  A panel timer and its window's rebuild
        // poll are independent; without this the timer can tick on a DSP a
        // project load already destroyed.  See EditorPanelBase::liveDsp.
        if (liveDsp() == nullptr) return;
        if (grMeter && mDsp) grMeter->setGainReduction (mDsp->getGainReductionDb());
    }

    void paint (juce::Graphics& g) override
    {
        DynamicsLAF::paintLA2APanel (g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);

        // Left strip: VU input meter + GR meter (matches FET/Opto panels).
        if (vuIn) vuIn->setBounds (b.removeFromLeft (120).reduced (1, 2));
        if (grMeter)
        {
            int gw = juce::jmin (96, b.getHeight() + 8);
            grMeter->setBounds (b.removeFromLeft (gw).reduced (1, 2));
            b.removeFromLeft (4);
        }

        // Right edge: dBFS meter only (output knob disabled in ctor).
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);

        // 4 knobs evenly across the remaining width: Level / Tone / Attack / Sustain.
        const int n     = 4;
        const int slotW = juce::jmax (1, b.getWidth() / n);
        const int sz    = juce::jmin (slotW, b.getHeight(), kKnobSz + 8);
        for (int i = 0; i < n; ++i)
            knobs[(size_t) i]->setBounds (b.removeFromLeft (slotW).withSizeKeepingCentre (sz, sz));
    }
};

struct CompressorPanel : public EditorPanelBase,
                         public juce::Timer
{
    std::unique_ptr<ChickenHeadSelector> kneeSel;
    std::unique_ptr<DualLabelToggle>     autoMuTog;
    std::unique_ptr<DualLabelToggle>     linkTog;
    std::unique_ptr<DualLabelToggle>     peakRmsTog;  // C3: Peak vs RMS detection
    std::unique_ptr<GRMeter>             grMeter;
    CompressorDSP*                       mDsp { nullptr };

    void disableGrMeter() override
    {
        if (grMeter) { removeChildComponent (grMeter.get()); grMeter.reset(); }
    }

    // QA-EffectsReview Task 6: Basic = the exact FL Fruity Compressor control set
    // (Thresh/Ratio/Gain/Attack/Release/KneeType + meters); Advanced reveals our
    // additions (manual KneeW, Mix, Look-ahead, Det window, SC-HPF, Auto-MU,
    // Stereo Link, Peak/RMS).  resized() does the show/hide + reflow.
    bool hasAdvancedControls() const override { return true; }

    explicit CompressorPanel(CompressorDSP* dsp)
        : mDsp(dsp)
    {
        setLookAndFeel(&DynamicsLAF::get());
        setVolumeKnobVariant(true);  // black knob for cream Dynamics panel

        buildKnobs(*this, knobs, {
            { "Thresh",  -60.f,   0.f, -12.f, 0.5f, "Threshold (dB)" },
            { "Ratio",    0.4f,  30.f,   4.f, 0.1f, "Compression ratio (0.4=expand, >1=compress)" },
            // D.4-Q4 (2026-05-01): manual knee-width knob (was hidden APVTS).
            { "KneeW",    0.f,  18.f,   6.f,  0.1f, "Manual knee width (dB) - overrides knee-type's default smoothing" },
            { "Gain",   -30.f,  30.f,   0.f,  0.5f, "Output gain (dB) - ignored if Auto MU is on" },
            { "Attack",   0.f, 400.f,  10.f,  0.5f, "Attack (ms)" },
            { "Release",  1.f,4000.f, 100.f,  1.f,  "Release (ms)" },
            { "Mix",      0.f,   1.f,   1.f,  0.01f,"Dry/Wet mix (parallel compression)" },
            { "LookA",    0.f,   5.f,   0.f,  0.1f, "Look-ahead (ms) - adds latency; EffectRack PDC compensates" },
            { "Det",      1.f, 100.f,  10.f,  0.1f, "Detection window (ms) - RMS smoother time constant; smaller=faster tracking" },
            { "SCHPF",   20.f,2000.f, 20.f,   1.f,  "Sidechain HPF cutoff (Hz) - 20 = effectively off; raise to prevent bass triggering GR" },
        });

        // All knobs use modernAnalog variant (black cream-plate knob)
        for (int i = 0; i < 10; ++i)
            knobs[i]->slider.getProperties().set(DynamicsLAF::kKnobVariant, "modernAnalog");

        // KneeType - 8-position chicken-head selector
        kneeSel = std::make_unique<ChickenHeadSelector>();
        kneeSel->setOptions({
            { "H",   "Hard",     "Hard knee - sharp, snappy compression" },
            { "M",   "Medium",   "Medium knee - moderate transition (default)" },
            { "V",   "Vintage",  "Vintage knee - gentle, musical curve" },
            { "S",   "Soft",     "Soft knee - very smooth, transparent" },
            { "H/R", "Hard/R",   "Hard knee + TCR auto-release - fast program-dependent release" },
            { "M/R", "Medium/R", "Medium knee + TCR auto-release" },
            { "V/R", "Vintage/R","Vintage knee + TCR auto-release" },
            { "S/R", "Soft/R",   "Soft knee + TCR auto-release" },
        });
        kneeSel->setBodyTooltip("Knee type - /R variants enable TCR auto-release");
        kneeSel->setDefaultLabelColour(juce::Colours::black);  // cream Dynamics panel
        kneeSel->setSelectedIndex(1, juce::dontSendNotification);  // Medium default
        kneeSel->onChange = [dsp, this](int idx){ dsp->setKneeType(idx); repaint(); };
        addAndMakeVisible(*kneeSel);

        knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setThreshold   ((float)knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setRatio       ((float)knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setKnee        ((float)knobs[2]->slider.getValue()); };
        knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setGain        ((float)knobs[3]->slider.getValue()); };
        knobs[4]->slider.onValueChange = [dsp,this]{ dsp->setAttack      ((float)knobs[4]->slider.getValue()); };
        knobs[5]->slider.onValueChange = [dsp,this]{ dsp->setRelease     ((float)knobs[5]->slider.getValue()); };
        knobs[6]->slider.onValueChange = [dsp,this]{ dsp->setMix         ((float)knobs[6]->slider.getValue()); };
        knobs[7]->slider.onValueChange = [dsp,this]{
            dsp->setLookaheadMs ((float)knobs[7]->slider.getValue());
            if (onLatencyChanged) onLatencyChanged();   // lookahead changes getLatencySamples -> refresh PDC
        };
        knobs[8]->slider.onValueChange = [dsp,this]{ dsp->setDetectionMs ((float)knobs[8]->slider.getValue()); };
        knobs[9]->slider.onValueChange = [dsp,this]{ dsp->setSidechainHPF((float)knobs[9]->slider.getValue()); };

        // A9 slider sync from DSP state + clamp cleanup (sendNotificationSync fires the
        // onValueChange lambdas, which call the DSP setters with the slider-clamped value).
        knobs[0]->slider.setValue(dsp->threshold,    juce::sendNotificationSync);
        knobs[1]->slider.setValue(dsp->ratio,        juce::sendNotificationSync);
        knobs[2]->slider.setValue(dsp->kneeDb,       juce::sendNotificationSync);
        knobs[3]->slider.setValue(dsp->makeupDb,     juce::sendNotificationSync);
        knobs[4]->slider.setValue(dsp->attackMs,     juce::sendNotificationSync);
        knobs[5]->slider.setValue(dsp->releaseMs,    juce::sendNotificationSync);
        knobs[6]->slider.setValue(dsp->mix,          juce::sendNotificationSync);
        knobs[7]->slider.setValue(dsp->lookaheadMs,  juce::sendNotificationSync);
        knobs[8]->slider.setValue(dsp->detectionMs,  juce::sendNotificationSync);
        knobs[9]->slider.setValue(dsp->sidechainHPF, juce::sendNotificationSync);

        // -- Auto-MU on/off toggle ----------------------------------------
        autoMuTog = std::make_unique<DualLabelToggle>();
        autoMuTog->setupOnOff("Auto MU", "Auto makeup gain - compensates for GR at 0 dBFS");
        autoMuTog->btn().onClick = [dsp, this] { dsp->setAutoMakeup(autoMuTog->btn().getToggleState()); };
        addAndMakeVisible(*autoMuTog);

        // -- Stereo Link on/off toggle ------------------------------------
        linkTog = std::make_unique<DualLabelToggle>();
        linkTog->setupOnOff("Link", "Stereo link - single envelope driven by max(|L|,|R|)");
        linkTog->btn().setToggleState(true, juce::dontSendNotification);
        linkTog->btn().onClick = [dsp, this] { dsp->setStereoLink(linkTog->btn().getToggleState()); };
        addAndMakeVisible(*linkTog);

        // -- Peak/RMS detection named toggle (C3) -------------------------
        peakRmsTog = std::make_unique<DualLabelToggle>();
        peakRmsTog->setupNamed(
            "RMS",  "Mean-square detection (classic) - smoother, more musical compression",
            "Peak", "Peak detection - aggressive transient tracking, limiter-like character");
        peakRmsTog->btn().onClick = [dsp, this]{
            dsp->setPeakDetection(peakRmsTog->btn().getToggleState());
        };
        addAndMakeVisible(*peakRmsTog);

        // ── GR meter (SSL-style) ──────────────────────────────────────
        grMeter = std::make_unique<GRMeter>();
        addAndMakeVisible(*grMeter);

        startTimerHz(30);
        // H-7 (2026-05-01): the existing CompressorPanel is now used ONLY for
        // Type::Modern.  FET + Opto have dedicated minimalist panel classes
        // (FETCompressorPanel / OptoCompressorPanel) that SlotComponent
        // mounts when the user picks those modes.  No hide-some-knobs hack.
    }

    // QA-F chain-wiring fix (2026-07-10): see EditorPanelBase::bindToApvts.
    // The direct DSP lambdas stay live -- the attachment writes the param and
    // the host's per-block push re-imposes the same value (agreement, not a
    // stomp).  Param ranges mirror the knob ranges above exactly (a
    // SliderAttachment resets the slider's range from the param).
    void bindToApvts (juce::AudioProcessorValueTreeState& apvts,
                      const juce::String& prefix) override
    {
        auto bindSlider = [&] (juce::Slider& s, const char* suffix)
        {
            mChainSliderAtts.push_back (std::make_unique<TaggedSliderAttachment> (
                apvts, prefix + suffix, s));
        };
        bindSlider (knobs[0]->slider, "comp_threshold");
        bindSlider (knobs[1]->slider, "comp_ratio");
        bindSlider (knobs[2]->slider, "comp_knee");
        bindSlider (knobs[3]->slider, "comp_gain");
        bindSlider (knobs[4]->slider, "comp_attack");
        bindSlider (knobs[5]->slider, "comp_release");
        bindSlider (knobs[6]->slider, "comp_mix");
        bindSlider (knobs[7]->slider, "comp_lookahead");
        bindSlider (knobs[8]->slider, "comp_detection");
        bindSlider (knobs[9]->slider, "comp_scHpf");

        auto bindButton = [&] (juce::Button& btn, const char* suffix)
        {
            mChainButtonAtts.push_back (
                std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                    apvts, prefix + suffix, btn));
        };
        if (autoMuTog)  bindButton (autoMuTog ->btn(), "comp_autoMakeup");
        if (linkTog)    bindButton (linkTog   ->btn(), "comp_stereoLink");
        if (peakRmsTog) bindButton (peakRmsTog->btn(), "comp_peakDet");

        if (auto* p = apvts.getParameter (prefix + "comp_kneeType"))
        {
            auto prev = kneeSel->onChange;
            kneeSel->onChange = [prev, p, um = apvts.undoManager] (int idx)
            {
                if (prev) prev (idx);
                beginParamUndoGesture (um, p->paramID); // Task 6 (12-iv)
                p->setValueNotifyingHost (
                    p->getNormalisableRange().convertTo0to1 ((float) idx));
            };
            mChainSelAtts.push_back (std::make_unique<juce::ParameterAttachment> (*p,
                [this] (float v)
                {
                    if (kneeSel)
                        kneeSel->setSelectedIndex ((int) std::lround (v),
                                                   juce::dontSendNotification);
                }, nullptr));
            mChainSelAtts.back()->sendInitialUpdate();
        }
    }

    // Declared after the controls they attach to: members destruct in
    // reverse order, so the attachments detach before their targets die.
    std::vector<std::unique_ptr<TaggedSliderAttachment>> mChainSliderAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> mChainButtonAtts;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> mChainSelAtts;

    ~CompressorPanel() override
    {
        stopTimer();
        setLookAndFeel(nullptr);
    }

    void timerCallback() override
    {
        // QA-Layout T13 crash fix: liveDsp() returns mDsp ONLY while the rack
        // still hands back that same pointer for this slot, so this one guard
        // makes every mDsp-> below safe.  A panel timer and its window's rebuild
        // poll are independent; without this the timer can tick on a DSP a
        // project load already destroyed.  See EditorPanelBase::liveDsp.
        if (liveDsp() == nullptr) return;
        if (mDsp && grMeter) grMeter->setGainReduction(mDsp->getGainReductionDb());
    }

    // Layout: [VU] [GR] <knob strip> [toggles (Advanced)] [DBFS][Vol].
    // Basic shows only the 6 Fruity controls (Thresh|Ratio|Gain|KneeType|Atk|Rel);
    // Advanced inserts KneeW + Mix|LookA|Det|SCHPF knobs and the toggle column.
    void resized() override
    {
        auto b = getLocalBounds().reduced(4, 4);

        const bool adv = ! mBasicMode;
        knobs[2]->setVisible (adv);   // KneeW
        knobs[6]->setVisible (adv);   // Mix
        knobs[7]->setVisible (adv);   // LookA
        knobs[8]->setVisible (adv);   // Det
        knobs[9]->setVisible (adv);   // SCHPF
        if (autoMuTog)  autoMuTog ->setVisible (adv);
        if (linkTog)    linkTog   ->setVisible (adv);
        if (peakRmsTog) peakRmsTog->setVisible (adv);

        // VU input meter (left)
        if (vuIn) vuIn->setBounds(b.removeFromLeft(120).reduced(1, 2));
        // GR meter (square, same height as VU strip)
        if (grMeter) {
            int gw = juce::jmin(96, b.getHeight() + 8);
            grMeter->setBounds(b.removeFromLeft(gw).reduced(1, 2));
            b.removeFromLeft(4);
        }

        // Right-side meter + output fader
        dbfsOut      ->setBounds(b.removeFromRight(32).reduced(1, 2));
        b.removeFromRight(2);
        outputVolKnob->setBounds(b.removeFromRight(kKnobSz).withSizeKeepingCentre(kKnobSz, kKnobSz));
        b.removeFromRight(4);

        // Toggles (Advanced only): Auto MU (left), Link + Peak/RMS stacked (right).
        if (adv)
        {
            constexpr int kToggleColW = 62;
            auto togArea = b.removeFromRight(kToggleColW * 2 + 2);
            b.removeFromRight(4);

            auto rightCol = togArea.removeFromRight(kToggleColW);
            const int halfH = rightCol.getHeight() / 2;
            auto linkSlot = rightCol.removeFromTop(halfH);
            if (linkTog)    linkTog   ->setBounds(linkSlot.reduced(1));
            if (peakRmsTog) peakRmsTog->setBounds(rightCol.reduced(1));

            togArea.removeFromRight(2);
            auto autoMuSlot = togArea.withSizeKeepingCentre(kToggleColW, halfH);
            if (autoMuTog)  autoMuTog ->setBounds(autoMuSlot.reduced(1));
        }

        // Knob strip in display order; Advanced-only items inserted in place.
        std::vector<juce::Component*> strip;
        strip.push_back (knobs[0].get());              // Thresh
        strip.push_back (knobs[1].get());              // Ratio
        if (adv) strip.push_back (knobs[2].get());     // KneeW
        strip.push_back (knobs[3].get());              // Gain
        if (kneeSel) strip.push_back (kneeSel.get());  // KneeType chicken-head
        strip.push_back (knobs[4].get());              // Attack
        strip.push_back (knobs[5].get());              // Release
        if (adv)
        {
            strip.push_back (knobs[6].get());          // Mix
            strip.push_back (knobs[7].get());          // LookA
            strip.push_back (knobs[8].get());          // Det
            strip.push_back (knobs[9].get());          // SCHPF
        }

        const int n     = juce::jmax (1, (int) strip.size());
        const int slotW = b.getWidth() / n;
        const int sz    = juce::jmin(slotW, b.getHeight(), kKnobSz);
        for (auto* c : strip)
        {
            auto slot = b.removeFromLeft(slotW);
            if (c == kneeSel.get()) c->setBounds(slot.reduced(2));               // chicken-head fills slot
            else                    c->setBounds(slot.withSizeKeepingCentre(sz, sz));
        }
    }

    void paint(juce::Graphics& g) override
    {
        DynamicsLAF::paintLA2APanel(g, getLocalBounds());
        // Knee labels are drawn by ChickenHeadSelector itself - no custom paint needed.
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// ReverbPanel - 5F-9 §8 expanded (2 rows + right-side extras)
// Row 1: Room | Decay | HFRatio* | Diffuse | PreDly | WetTone* | Wet | Dry
//        (+ Duck | DkThr | DkAtt | DkRel in Advanced)
// Row 1 right: Mode combo + Freeze* toggle
// Row 2: LoCut | HiCut | BassMlt | BassCross | TailDep | TailRt | Stereo | HiDamp
// Row 2 right: ER knob + TailShape* combo + Sync + HiDmp-bypass* toggles
// (* = our addition vs the reference reverb -> Advanced only.  Basic = the
//  reference face; the Mode combo shows Mid/Side in Basic -- Stereo is our
//  superset mode, revealed in Advanced, kept visible while active.)
// ─────────────────────────────────────────────────────────────────────────────
struct ReverbPanel : public EditorPanelBase
{
    std::vector<std::unique_ptr<VKnob>>         r1knobs, r2knobs, duckKnobs;
    std::unique_ptr<VKnob>                      erKnob;       // row 2 right

    // QA-EffectsReview Task 9: Basic = the exact reference face.  Our
    // additions = HFRatio / WetTone / Freeze / TailShape / HiDamp-bypass /
    // the duck cluster / the Stereo processing mode -> Advanced.
    bool hasAdvancedControls() const override { return true; }

    std::vector<VKnob*> getExtraKnobs() override
    {
        std::vector<VKnob*> v;
        for (auto& k : r1knobs)   if (k) v.push_back(k.get());
        for (auto& k : r2knobs)   if (k) v.push_back(k.get());
        for (auto& k : duckKnobs) if (k) v.push_back(k.get());
        if (erKnob) v.push_back(erKnob.get());
        return v;
    }
    std::unique_ptr<DualLabelToggle>            tempoTog, hdBypassTog, freezeTog;
    std::unique_ptr<ChickenHeadSelector>        modeSel, tailShapeSel, syncDivSel;
    JewelIndicator                              mJewel;
    ReverbDSP*                                  mDsp { nullptr };
    std::vector<int>                            mModeOptionValues;   // visible option idx -> DSP mode

    // The reference M/S control is a 2-state Mid/Side selector (default Mid);
    // our Stereo mode is a superset addition.  Basic shows Mid/Side only --
    // unless Stereo is the ACTIVE mode (old projects), which stays visible
    // until moved off (same edge rule as the Tube type-C option).  Advanced
    // always shows all three.  Rebuild is count-guarded; selection re-resolves
    // through mModeOptionValues so index<->mode stays correct in both lists.
    void updateModeOptions()
    {
        if (! modeSel || ! mDsp) return;
        const int  mode       = juce::jlimit(0, 2, mDsp->getProcessingMode());
        const bool showStereo = ! mBasicMode || mode == 0;
        const int  want       = showStereo ? 3 : 2;
        if (modeSel->getNumOptions() != want)
        {
            std::vector<ChickenHeadSelector::Option> opts;
            mModeOptionValues.clear();
            if (showStereo)
            {
                opts.push_back({ "S", "Stereo", "Process left/right independently - classic stereo reverb" });
                mModeOptionValues.push_back(0);
            }
            opts.push_back({ "M", "Mid",  "Process mid (L+R) only - side channel passes dry" });
            mModeOptionValues.push_back(1);
            opts.push_back({ "D", "Side", "Process side (L-R) only - mid channel passes dry" });
            mModeOptionValues.push_back(2);
            modeSel->setOptions(opts);
        }
        for (int i = 0; i < (int) mModeOptionValues.size(); ++i)
            if (mModeOptionValues[(size_t) i] == mode)
            {
                modeSel->setSelectedIndex(i, juce::dontSendNotification);
                break;
            }
    }

    explicit ReverbPanel(ReverbDSP* dsp)
        : mDsp(dsp)
    {
        disableVU();   // Reverb has no input VU - full knob width
        setLookAndFeel(&TimeLAF::get());
        addAndMakeVisible(mJewel);
        mJewel.setActive(true);

        // Row 1: tonal / mix (8 knobs)
        buildKnobs(*this, r1knobs, {
            { "Room",    0.f,   2.f,   0.6f,  0.01f, "Room size scale" },
            { "Decay",   0.1f, 20.f,   2.0f,  0.1f,  "RT60 decay time (s)" },
            { "HFRatio", 0.3f,  2.f,   0.7f,  0.01f, "HF decay ratio (<1 = darker/shorter highs)" },
            { "Diffuse", 0.f,   1.f,   0.5f,  0.01f, "Pre-diffusion (0=clear, 1=dense)" },
            { "PreDly",  0.f, 200.f,  10.f,   0.5f,  "Pre-delay (ms)" },
            { "WetTone",-12.f, 12.f,   0.f,   0.1f,  "Wet tone tilt \u00b112 dB @ 1 kHz (-=warm, +=bright)" },
            { "Wet",     0.f,   1.f,   0.3f,  0.01f, "Wet level" },
            { "Dry",     0.f,   1.f,   0.7f,  0.01f, "Dry level" },
        });

        // Row 2: filters / modulation / stereo (8 knobs)
        buildKnobs(*this, r2knobs, {
            { "LoCut",   20.f,  800.f,   80.f,   1.f,   "Input HPF (Hz)" },
            { "HiCut", 1000.f,20000.f, 18000.f, 10.f,   "Input LPF (Hz)" },
            { "BassMlt", 0.5f,   3.f,    1.2f,   0.05f, "Bass tail multiplier" },
            { "BassX",   20.f,  800.f,  250.f,   1.f,   "Bass shelf crossover (Hz)" },
            { "TailDep", 0.f,   1.5f,    0.3f,   0.01f, "Tail modulation depth (ms)" },
            { "TailRt",  0.05f, 2.f,     0.35f,  0.01f, "Tail modulation rate (Hz)" },
            { "Stereo",  0.f,  200.f,   100.f,   1.f,   "Stereo separation (%)" },
            { "HiDamp",500.f, 20000.f,  8000.f, 10.f,   "High-freq damping (Hz)" },
        });

        // ER knob (row 2 right side, compact - standalone since it's not in r2knobs)
        erKnob = std::make_unique<VKnob>("ER", -6.0f, "Early reflections level (dB)");
        erKnob->slider.setRange(-60.0, 12.0, 0.1);
        // A9-style construct-time sync (was hardcoded -6, ignoring loaded state).
        erKnob->slider.setValue(dsp->getER(), juce::dontSendNotification);
        erKnob->slider.onValueChange = [dsp, this]{ dsp->setER((float)erKnob->slider.getValue()); };
        addAndMakeVisible(*erKnob);

        modeSel = std::make_unique<ChickenHeadSelector>();
        modeSel->setBodyTooltip("Channel-processing mode");
        // Task 9: options + selection come from updateModeOptions() (list is
        // Basic/Advanced-dependent); mModeOptionValues maps the visible index
        // to the DSP mode so the 2-option Basic list can't misroute.  Moving
        // off Stereo in Basic drops the Stereo option at the next relayout
        // (same eventual-consistency as the Tube type-C edge).
        modeSel->onChange = [dsp, this](int idx){
            if (idx >= 0 && idx < (int) mModeOptionValues.size())
                dsp->setProcessingMode(mModeOptionValues[(size_t) idx]);
        };
        addAndMakeVisible(*modeSel);
        updateModeOptions();

        // §8f tail-mod shape selector: sine / triangle / random S&H
        tailShapeSel = std::make_unique<ChickenHeadSelector>();
        tailShapeSel->setOptions({
            { "S", "Sine", "Smooth sinusoidal tail modulation" },
            { "T", "Tri",  "Triangle tail modulation - linear ramp, slightly more character" },
            { "R", "Rand", "Random sample-and-hold - organic, non-repeating wobble" },
        });
        tailShapeSel->setBodyTooltip("Tail modulation LFO shape");
        // A9: initialize from DSP so preset-loaded shape shows correctly.
        tailShapeSel->setSelectedIndex(juce::jlimit(0, 2, dsp->getTailModShape()),
                                       juce::dontSendNotification);
        tailShapeSel->onChange = [dsp](int idx){ dsp->setTailModShape(idx); };
        addAndMakeVisible(*tailShapeSel);

        tempoTog = std::make_unique<DualLabelToggle>();
        tempoTog->setupOnOff("Sync", "Sync pre-delay to host BPM");
        // A9: initialize from DSP so preset-loaded sync state shows correctly.
        tempoTog->btn().setToggleState(dsp->getTempoSync(), juce::dontSendNotification);
        tempoTog->btn().onClick = [dsp, this]{
            const bool on = tempoTog->btn().getToggleState();
            dsp->setTempoSync (on);
            // H-9 (2026-05-02): grey the division selector when sync is off.
            if (syncDivSel) syncDivSel->setLocked (! on);
        };
        tempoTog->setLabelColour(VC::Text);   // TimeLAF dark Pultec panel
        addAndMakeVisible(*tempoTog);

        // H-9 (2026-05-02): tempo-sync division selector.  Mirrors the
        // chicken-head pattern used by Chorus / Flanger / Phaser.  Locked
        // (greyed) when Sync toggle is off since division only matters when
        // tempo-sync is engaged.
        syncDivSel = std::make_unique<ChickenHeadSelector>();
        syncDivSel->setOptions({
            { "1/1",  "Whole",    "Whole note (1/1) pre-delay" },
            { "1/2",  "Half",     "Half note (1/2) pre-delay" },
            { "1/4",  "Quarter",  "Quarter note (1/4) pre-delay (default)" },
            { "1/8",  "Eighth",   "Eighth note (1/8) pre-delay" },
            { "1/8D", "8th Dot.", "Dotted 8th (= 3/16) pre-delay" },
            { "1/4T", "Qtr Trip", "Quarter triplet (= 1/6) pre-delay" },
            { "1/16", "16th",     "Sixteenth note (1/16) pre-delay" },
            { "1/8T", "8th Trip", "Eighth triplet (= 1/12) pre-delay" },
        });
        syncDivSel->setBodyTooltip("Tempo-sync note division (when Sync toggle is on)");
        syncDivSel->setSelectedIndex(
            juce::jlimit(0, ReverbDSP::kNumSyncDivisions - 1, dsp->getSyncDivision()),
            juce::dontSendNotification);
        syncDivSel->onChange = [dsp](int idx){ dsp->setSyncDivision(idx); };
        // Initial lockout follows the loaded sync state (A9).
        syncDivSel->setLocked (! dsp->getTempoSync());
        addAndMakeVisible(*syncDivSel);

        hdBypassTog = std::make_unique<DualLabelToggle>();
        hdBypassTog->setupOnOff("HiDmp", "Bypass high-freq damping");
        // A9: initialize from DSP so preset-loaded bypass shows correctly.
        hdBypassTog->btn().setToggleState(dsp->getHighDampBypass(), juce::dontSendNotification);
        hdBypassTog->btn().onClick = [dsp, this]{ dsp->setHighDampBypass(hdBypassTog->btn().getToggleState()); };
        hdBypassTog->setLabelColour(VC::Text);
        addAndMakeVisible(*hdBypassTog);

        // §8c Freeze toggle
        freezeTog = std::make_unique<DualLabelToggle>();
        freezeTog->setupOnOff("Freeze", "Freeze/infinite hold - hold the current reverb tail indefinitely.  Right-click to automate.");
        // A9: initialize from DSP so preset-loaded freeze shows correctly.
        freezeTog->btn().setToggleState(dsp->getFreeze(), juce::dontSendNotification);
        freezeTog->btn().onClick = [dsp, this]{ dsp->setFreeze(freezeTog->btn().getToggleState()); };
        freezeTog->setLabelColour(VC::Text);
        addAndMakeVisible(*freezeTog);
        // Task 9: freeze is the musically-automatable toggle -- opt it into
        // the right-click Automate system (paramId = <base>freeze).
        addAutomatableToggle(*freezeTog, "freeze");

        // Row 1 bindings
        r1knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setRoomSize ((float)r1knobs[0]->slider.getValue()); };
        r1knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setDecay    ((float)r1knobs[1]->slider.getValue()); };
        r1knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setHFRatio  ((float)r1knobs[2]->slider.getValue()); };
        r1knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setDiffusion((float)r1knobs[3]->slider.getValue()); };
        r1knobs[4]->slider.onValueChange = [dsp,this]{ dsp->setPreDelay ((float)r1knobs[4]->slider.getValue()); };
        r1knobs[5]->slider.onValueChange = [dsp,this]{ dsp->setWetTone  ((float)r1knobs[5]->slider.getValue()); };
        r1knobs[6]->slider.onValueChange = [dsp,this]{ dsp->setWet      ((float)r1knobs[6]->slider.getValue()); };
        r1knobs[7]->slider.onValueChange = [dsp,this]{ dsp->setDry      ((float)r1knobs[7]->slider.getValue()); };

        // Row 2 bindings
        r2knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setLowCut        ((float)r2knobs[0]->slider.getValue()); };
        r2knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setHighCut       ((float)r2knobs[1]->slider.getValue()); };
        r2knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setBassMult      ((float)r2knobs[2]->slider.getValue()); };
        r2knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setBassCrossover ((float)r2knobs[3]->slider.getValue()); };
        r2knobs[4]->slider.onValueChange = [dsp,this]{ dsp->setTailModDepth  ((float)r2knobs[4]->slider.getValue()); };
        r2knobs[5]->slider.onValueChange = [dsp,this]{ dsp->setTailModRate   ((float)r2knobs[5]->slider.getValue()); };
        r2knobs[6]->slider.onValueChange = [dsp,this]{ dsp->setStereoSep     ((float)r2knobs[6]->slider.getValue()); };
        r2knobs[7]->slider.onValueChange = [dsp,this]{ dsp->setHighDamp      ((float)r2knobs[7]->slider.getValue()); };

        // A9 slider sync from DSP state + clamp cleanup (via public getters on ReverbDSP).
        r1knobs[0]->slider.setValue(dsp->getRoomSize(),       juce::sendNotificationSync);
        r1knobs[1]->slider.setValue(dsp->getDecay(),          juce::sendNotificationSync);
        r1knobs[2]->slider.setValue(dsp->getHFRatio(),        juce::sendNotificationSync);
        r1knobs[3]->slider.setValue(dsp->getDiffusion(),      juce::sendNotificationSync);
        r1knobs[4]->slider.setValue(dsp->getPreDelayMs(),     juce::sendNotificationSync);
        r1knobs[5]->slider.setValue(dsp->getWetTiltDb(),      juce::sendNotificationSync);
        r1knobs[6]->slider.setValue(dsp->getWet(),            juce::sendNotificationSync);
        r1knobs[7]->slider.setValue(dsp->getDry(),            juce::sendNotificationSync);
        r2knobs[0]->slider.setValue(dsp->getLowCutHz(),       juce::sendNotificationSync);
        r2knobs[1]->slider.setValue(dsp->getHighCutHz(),      juce::sendNotificationSync);
        r2knobs[2]->slider.setValue(dsp->getBassMult(),       juce::sendNotificationSync);
        r2knobs[3]->slider.setValue(dsp->getBassCrossHz(),    juce::sendNotificationSync);
        r2knobs[4]->slider.setValue(dsp->getTailModDepthMs(), juce::sendNotificationSync);
        r2knobs[5]->slider.setValue(dsp->getTailModRateHz(),  juce::sendNotificationSync);
        r2knobs[6]->slider.setValue(dsp->getStereoSep(),      juce::sendNotificationSync);
        r2knobs[7]->slider.setValue(dsp->getHighDampHz(),     juce::sendNotificationSync);

        // QA-EffectsReview Task 9: ducking controls (Advanced) -- the DSP has
        // carried the full duck engine since H-9 with no panel controls; wired
        // here mirroring the Delay's duck cluster.
        buildKnobs(*this, duckKnobs, {
            { "Duck",   0.f, 100.f,   0.f, 1.f, "0..100 %.  Sidechain ducking amount.  When the trigger (SC source, or the dry input if none picked) crosses the DkThr threshold, the wet reverb is attenuated by this amount.  0 = disabled." },
            { "DkThr", -60.f,   0.f, -24.f, 1.f, "Duck threshold (dB) - trigger level where ducking starts" },
            { "DkAtt",   1.f, 200.f,  10.f, 1.f, "Duck attack (ms) - how fast the wet ducks once the trigger crosses threshold" },
            { "DkRel",  10.f,1000.f, 200.f, 5.f, "Duck release (ms) - how fast the wet recovers after the trigger falls back" },
        });
        duckKnobs[0]->slider.onValueChange = [dsp,this]{ dsp->setDuckAmount      ((float)duckKnobs[0]->slider.getValue()); };
        duckKnobs[1]->slider.onValueChange = [dsp,this]{ dsp->setDuckThresholdDb ((float)duckKnobs[1]->slider.getValue()); };
        duckKnobs[2]->slider.onValueChange = [dsp,this]{ dsp->setDuckAttackMs    ((float)duckKnobs[2]->slider.getValue()); };
        duckKnobs[3]->slider.onValueChange = [dsp,this]{ dsp->setDuckReleaseMs   ((float)duckKnobs[3]->slider.getValue()); };
        duckKnobs[0]->slider.setValue(dsp->getDuckAmount(),      juce::sendNotificationSync);
        duckKnobs[1]->slider.setValue(dsp->getDuckThresholdDb(), juce::sendNotificationSync);
        duckKnobs[2]->slider.setValue(dsp->getDuckAttackMs(),    juce::sendNotificationSync);
        duckKnobs[3]->slider.setValue(dsp->getDuckReleaseMs(),   juce::sendNotificationSync);
    }

    ~ReverbPanel() override { setLookAndFeel(nullptr); }

    void paint(juce::Graphics& g) override
    {
        TimeLAF::paintPultecPanel(g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(4, 2);

        // Meter strips (Reverb has no input VU - disableVU() was called)
        if (vuIn) vuIn->setBounds(b.removeFromLeft(120).reduced(1, 2));
        dbfsOut      ->setBounds(b.removeFromRight(32).reduced(1, 2));
        b.removeFromRight(2);
        outputVolKnob->setBounds(b.removeFromRight(kKnobSz).withSizeKeepingCentre(kKnobSz, kKnobSz));
        b.removeFromRight(4);

        // Jewel: top-right of content area
        mJewel.setBounds(b.getRight() - 14, b.getY(), 12, 12);

        // Task 9 Basic/Advanced: Basic = the reference face.  Advanced adds
        // HFRatio + WetTone (row 1), the duck cluster (row 1), Freeze,
        // TailShape, HiDamp-bypass -- and the Stereo mode option
        // (updateModeOptions).
        const bool adv = ! mBasicMode;
        if (r1knobs.size() > 2 && r1knobs[2]) r1knobs[2]->setVisible(adv);   // HFRatio
        if (r1knobs.size() > 5 && r1knobs[5]) r1knobs[5]->setVisible(adv);   // WetTone
        if (freezeTog)    freezeTog   ->setVisible(adv);
        if (tailShapeSel) tailShapeSel->setVisible(adv);
        if (hdBypassTog)  hdBypassTog ->setVisible(adv);
        for (auto& k : duckKnobs) if (k) k->setVisible(adv);
        updateModeOptions();

        auto r1 = b.removeFromTop(b.getHeight() / 2);
        auto r2 = b;

        // H-9 (2026-05-02): right-side controls grid (visual right-to-left).
        // The chicken-heads stack vertically by column so each pair reads as
        // a unit:
        //   Col A (rightmost, Advanced only): Freeze toggle / HiDmp toggle
        //   Col B:             SyncDiv combo  /  Sync toggle
        //   Col C:             Mode combo     /  TailShape combo (Advanced)
        //   Row 2 only:        ER knob (no row-1 partner)
        // Matched widths per column so the rows align cleanly.
        constexpr int kTogW = 62;   // toggle column width
        constexpr int kCmbW = 66;   // chicken-head column width

        // ── Row 1 right (right-to-left): [Freeze adv] | SyncDiv | Mode ────
        if (adv)
        {
            auto freezeSlot = r1.removeFromRight(kTogW); r1.removeFromRight(2);
            if (freezeTog) freezeTog->setBounds(freezeSlot.reduced(1));
        }
        auto syncDivSlot = r1.removeFromRight(kCmbW); r1.removeFromRight(2);
        if (syncDivSel)  syncDivSel->setBounds(syncDivSlot.reduced(2));
        auto modeSlot = r1.removeFromRight(kCmbW); r1.removeFromRight(2);
        if (modeSel)   modeSel->setBounds(modeSlot.reduced(2));
        std::vector<VKnob*> row1;
        for (int i = 0; i < (int) r1knobs.size(); ++i)
            if (r1knobs[i] && (adv || (i != 2 && i != 5)))
                row1.push_back(r1knobs[i].get());
        if (adv)
            for (auto& k : duckKnobs) if (k) row1.push_back(k.get());
        layoutKnobsH(r1, row1);

        // ── Row 2 right (right-to-left): [HiDmp adv] | Sync | [TailShape adv] | ER ────
        if (adv)
        {
            auto hiDmpSlot = r2.removeFromRight(kTogW); r2.removeFromRight(2);
            if (hdBypassTog) hdBypassTog->setBounds(hiDmpSlot.reduced(1));
        }
        auto syncSlot = r2.removeFromRight(kCmbW); r2.removeFromRight(2);
        if (tempoTog)    tempoTog->setBounds(syncSlot.reduced(1));
        if (adv)
        {
            auto tailShapeSlot = r2.removeFromRight(kCmbW); r2.removeFromRight(2);
            if (tailShapeSel) tailShapeSel->setBounds(tailShapeSlot.reduced(2));
        }
        auto erSlot = r2.removeFromRight(kKnobSz + 4); r2.removeFromRight(2);
        if (erKnob) erKnob->setBounds(erSlot.withSizeKeepingCentre(kKnobSz, kKnobSz));
        layoutKnobsH(r2, r2knobs);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// SaturationPanel  (2 rows)
// Row 1: Flowers | Dabs | Input | BassRlf
// Row 2: TonePre | TonePost | Wet | Out  +  Transformer toggle  +  TubeType combo
// ─────────────────────────────────────────────────────────────────────────────
struct SaturationPanel : public EditorPanelBase,
                         public juce::Timer
{
    std::vector<std::unique_ptr<VKnob>>         r1knobs, r2knobs;
    std::unique_ptr<DualLabelToggle>            transformerTog;
    std::unique_ptr<DualLabelToggle>            autoGainTog;       // 9b
    std::unique_ptr<ChickenHeadSelector>        tubeTypeSel;
    std::unique_ptr<ChickenHeadSelector>        osSel;             // C2
    std::unique_ptr<juce::Label>                autoGainCompLabel; // C4 - dB readout next to Auto-Gain toggle
    std::unique_ptr<ChickenHeadSelector>        harmModeSel;       // H-7: Keep Low / Normal / Keep High
    SaturationDSP*                              mDsp { nullptr };

    std::vector<VKnob*> getExtraKnobs() override
    {
        std::vector<VKnob*> v;
        for (auto& k : r1knobs) if (k) v.push_back(k.get());
        for (auto& k : r2knobs) if (k) v.push_back(k.get());
        return v;
    }

    // Tube panel = a near-1:1 replica of the reference (Beauty/Beast tube
    // saturator).  Basic = the reference's face controls (all 8 knobs +
    // Transformer + Oversampling + Tube Type A/B).  Advanced reveals our
    // additions: Auto-MU, the Keep-Low/Normal/Keep-High harmonics routing, and
    // the 3rd (C) tube-type voicing the reference doesn't have.
    bool hasAdvancedControls() const override { return true; }

    explicit SaturationPanel(SaturationDSP* dsp)
        : mDsp(dsp)
    {
        setLookAndFeel(&HarmonicLAF::get());

        buildKnobs(*this, r1knobs, {
            { "Flowers",  0.f,  10.f,  3.f,  0.1f, "Even harmonics (tanh)" },
            { "Dabs",     0.f,  10.f,  3.f,  0.1f, "Odd/even harmonics (per Tube type)" },
            { "Input",  -12.f,  12.f,  0.f,  0.5f, "Input sensitivity (dB)" },
            { "BassRlf",  0.f, 100.f, 30.f,  1.f,  "Bass relief - 100 = lows stay clean" },
        });

        buildKnobs(*this, r2knobs, {
            { "TonePre",  -9.f,  9.f,  0.f,  0.5f, "Tone pre (dB high shelf)" },
            { "TonePost", -9.f,  9.f,  0.f,  0.5f, "Tone post (dB high shelf)" },
            { "Wet",       0.f,100.f, 70.f,  1.f,  "Wet (%)" },
            { "Out",     -18.f, 18.f,  0.f,  0.5f, "Output gain (dB)" },
        });

        tubeTypeSel = std::make_unique<ChickenHeadSelector>();
        tubeTypeSel->setOptions({
            { "A", "Type A", "Aggressive odd harmonics - bold, gritty drive" },
            { "B", "Type B", "Mild odd harmonics - gentler, cleaner clip" },
            { "C", "Type C", "Warm even harmonics (foldback) - tube-like warmth" },
        });
        tubeTypeSel->setBodyTooltip("Dabs tube character");
        // A9: initialize from DSP so preset-loaded type shows correctly.
        tubeTypeSel->setSelectedIndex(juce::jlimit(0, 2, dsp->mTubeType), juce::dontSendNotification);
        tubeTypeSel->onChange = [dsp](int idx){ dsp->setTubeType(idx); };
        addAndMakeVisible(*tubeTypeSel);

        transformerTog = std::make_unique<DualLabelToggle>();
        transformerTog->setupOnOff("Trans", "Transformer - always-on even-harmonic base");
        // A9
        transformerTog->btn().setToggleState(dsp->mTransformer, juce::dontSendNotification);
        transformerTog->btn().onClick = [dsp, this]{ dsp->setTransformer(transformerTog->btn().getToggleState()); };
        addAndMakeVisible(*transformerTog);

        // 9b Auto-Gain toggle.
        autoGainTog = std::make_unique<DualLabelToggle>();
        autoGainTog->setupOnOff("Auto MU",
            "Auto-Gain compensation - cancels Input sensitivity boost at the output so drive character is decoupled from volume");
        autoGainTog->btn().setToggleState(dsp->getAutoGain(), juce::dontSendNotification);
        autoGainTog->btn().onClick = [dsp, this]{
            dsp->setAutoGain(autoGainTog->btn().getToggleState());
            updateAutoGainLabel();
        };
        addAndMakeVisible(*autoGainTog);

        // C4: small dB readout next to Auto-Gain toggle. Shows compensation amount when ON.
        autoGainCompLabel = std::make_unique<juce::Label>("autoGainComp", "");
        autoGainCompLabel->setJustificationType(juce::Justification::centred);
        autoGainCompLabel->setColour(juce::Label::textColourId, juce::Colours::black);
        autoGainCompLabel->setFont(juce::Font(11.0f, juce::Font::bold));
        autoGainCompLabel->setTooltip("Auto-Gain compensation amount (dB)");
        addAndMakeVisible(*autoGainCompLabel);
        updateAutoGainLabel();

        // C2 Oversampling factor chicken-head.
        osSel = std::make_unique<ChickenHeadSelector>();
        osSel->setOptions({
            { "2x",  "2x OS",  "2x oversampling - lowest CPU, some aliasing at high drive" },
            { "4x",  "4x OS",  "4x oversampling (default) - balanced quality vs CPU" },
            { "8x",  "8x OS",  "8x oversampling - cleaner high-drive saturation, more CPU" },
            { "16x", "16x OS", "16x oversampling - maximum alias suppression, highest CPU" },
        });
        osSel->setBodyTooltip("Oversampling factor around the tube engine");
        // A9: map mOsLog2 (1..4) to index (0..3).
        osSel->setSelectedIndex(juce::jlimit(0, 3, dsp->getOversamplingLog2() - 1),
                                juce::dontSendNotification);
        osSel->onChange = [dsp](int idx){ dsp->setOversamplingFactor(juce::jlimit(1, 4, idx + 1)); };
        addAndMakeVisible(*osSel);

        r1knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setFlowers    ((float)r1knobs[0]->slider.getValue()); };
        r1knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setDabs       ((float)r1knobs[1]->slider.getValue()); };
        r1knobs[2]->slider.onValueChange = [dsp,this]{
            dsp->setSensitivity((float)r1knobs[2]->slider.getValue());
            updateAutoGainLabel();   // C4 tracks sens changes
        };
        r1knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setBassRelief ((float)r1knobs[3]->slider.getValue()); };

        r2knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setTonePre ((float)r2knobs[0]->slider.getValue()); };
        r2knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setTonePost((float)r2knobs[1]->slider.getValue()); };
        r2knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setWet     ((float)r2knobs[2]->slider.getValue()); };
        r2knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setOut     ((float)r2knobs[3]->slider.getValue()); };

        // A9 slider sync from DSP state + clamp cleanup.
        r1knobs[0]->slider.setValue(dsp->mFlowers,     juce::sendNotificationSync);
        r1knobs[1]->slider.setValue(dsp->mDabs,        juce::sendNotificationSync);
        r1knobs[2]->slider.setValue(dsp->mSensitivity, juce::sendNotificationSync);
        r1knobs[3]->slider.setValue(dsp->mBassRelief,  juce::sendNotificationSync);
        r2knobs[0]->slider.setValue(dsp->mTonePre,     juce::sendNotificationSync);
        r2knobs[1]->slider.setValue(dsp->mTonePost,    juce::sendNotificationSync);
        r2knobs[2]->slider.setValue(dsp->mWet,         juce::sendNotificationSync);
        r2knobs[3]->slider.setValue(dsp->mOut,         juce::sendNotificationSync);

        // Timer keeps the comp-label synced against automation-driven Sens changes too.
        startTimerHz(10);
        // H-7 (2026-05-01): the existing SaturationPanel is now used ONLY for
        // Type::Tube.  Console mode mounts a dedicated ConsoleSaturationPanel.

        // H-7: Keep Low / Normal / Keep High harmonic-routing chickenhead.
        // Same selector as ConsoleSaturationPanel; both umbrella Types share
        // the routing logic at the DSP level.
        harmModeSel = std::make_unique<ChickenHeadSelector>();
        harmModeSel->setOptions ({
            { "Lo",  "Keep Low",  "Saturate highs only -- bass passes through dry" },
            { "Nrm", "Normal",    "Full-band saturation (default)" },
            { "Hi",  "Keep High", "Saturate lows only -- highs pass through dry" },
        });
        harmModeSel->setBodyTooltip ("Harmonics routing");
        if (dsp)
            harmModeSel->setSelectedIndex ((int) dsp->mHarmonicsMode, juce::dontSendNotification);
        harmModeSel->onChange = [dsp] (int idx) { if (dsp) dsp->setHarmonicsMode (idx); };
        addAndMakeVisible (*harmModeSel);
    }

    // QA-F chain-wiring fix (2026-07-10): see EditorPanelBase::bindToApvts.
    // Only the harmonics selector is param-backed on this panel (the host's
    // bsv_sat_* set: type rides the SlotComponent Mode dropdown, vocalBody
    // has no panel control).  The tube/tape knobs were never pushed from
    // params, so they are not stomped; they persist via the host's per-slot
    // DSP-state blob instead.
    void bindToApvts (juce::AudioProcessorValueTreeState& apvts,
                      const juce::String& prefix) override
    {
        if (auto* p = apvts.getParameter (prefix + "sat_harmonicsMode"))
        {
            auto prev = harmModeSel->onChange;
            harmModeSel->onChange = [prev, p, um = apvts.undoManager] (int idx)
            {
                if (prev) prev (idx);
                beginParamUndoGesture (um, p->paramID); // Task 6 (12-iv)
                p->setValueNotifyingHost (
                    p->getNormalisableRange().convertTo0to1 ((float) idx));
            };
            mChainSelAtts.push_back (std::make_unique<juce::ParameterAttachment> (*p,
                [this] (float v)
                {
                    if (harmModeSel)
                        harmModeSel->setSelectedIndex ((int) std::lround (v),
                                                       juce::dontSendNotification);
                }, nullptr));
            mChainSelAtts.back()->sendInitialUpdate();
        }
    }

    // Declared after the selector it attaches to (reverse-order destruction).
    std::vector<std::unique_ptr<juce::ParameterAttachment>> mChainSelAtts;

    void updateAutoGainLabel()
    {
        if (!autoGainCompLabel || !mDsp) return;
        if (mDsp->getAutoGain())
        {
            const float dB = mDsp->getAutoGainCompDb();
            autoGainCompLabel->setText(juce::String(dB, 1) + " dB", juce::dontSendNotification);
        }
        else
        {
            autoGainCompLabel->setText("", juce::dontSendNotification);
        }
    }

    void timerCallback() override { updateAutoGainLabel(); }

    ~SaturationPanel() override
    {
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        HarmonicLAF::paintHammeritePanel(g, getLocalBounds());
    }

    // Tube Type C is an Advanced voicing: Basic offers A/B only.  C stays
    // visible if it's the CURRENTLY-ACTIVE type so a C preset is neither hidden
    // nor silently changed; once you move off C in Basic it drops to A/B.
    void updateTubeTypeOptions()
    {
        if (!tubeTypeSel) return;
        const int cur = mDsp ? juce::jlimit(0, 2, mDsp->mTubeType) : 0;
        const bool showC = (! mBasicMode) || (cur == 2);
        const int want = showC ? 3 : 2;
        if (tubeTypeSel->getNumOptions() == want) return;   // no churn
        std::vector<ChickenHeadSelector::Option> opts = {
            { "A", "Type A", "Aggressive odd harmonics - bold, gritty drive" },
            { "B", "Type B", "Mild odd harmonics - gentler, cleaner clip" },
        };
        if (showC)
            opts.push_back({ "C", "Type C", "Warm even harmonics (foldback) - tube-like warmth" });
        tubeTypeSel->setOptions(opts);
        tubeTypeSel->setSelectedIndex(juce::jlimit(0, want - 1, cur), juce::dontSendNotification);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(4, 2);

        // Meter strips
        if (vuIn) vuIn->setBounds(b.removeFromLeft(120).reduced(1, 2));
        dbfsOut      ->setBounds(b.removeFromRight(32).reduced(1, 2));
        b.removeFromRight(2);
        outputVolKnob->setBounds(b.removeFromRight(kKnobSz).withSizeKeepingCentre(kKnobSz, kKnobSz));
        b.removeFromRight(4);

        // Advanced-only: Auto-MU toggle + its dB comp label (Row 1) and the
        // harmonics-routing selector (Row 2).  Tube Type A/B/C handled below.
        const bool adv = ! mBasicMode;
        if (autoGainTog)       autoGainTog->setVisible(adv);
        if (autoGainCompLabel) autoGainCompLabel->setVisible(adv);
        if (harmModeSel)       harmModeSel->setVisible(adv);
        updateTubeTypeOptions();

        auto r1 = b.removeFromTop(b.getHeight() / 2);
        auto r2 = b;

        // Row 1 right (right-to-left): OS chicken-head (Basic) | [Auto MU | dB comp label] (Advanced).
        auto osSlot = r1.removeFromRight(60); r1.removeFromRight(2);
        if (osSel) osSel->setBounds(osSlot.reduced(2));
        if (adv)
        {
            auto togSlot = r1.removeFromRight(52); r1.removeFromRight(2);
            if (autoGainTog) autoGainTog->setBounds(togSlot.reduced(1));
            auto labelSlot = r1.removeFromRight(38); r1.removeFromRight(2);
            if (autoGainCompLabel) autoGainCompLabel->setBounds(labelSlot.reduced(1));
        }
        layoutKnobsH(r1, r1knobs);

        // Row 2 right: [harmonics routing] (Advanced) | TubeType | Transformer (Basic); knobs fill left.
        if (adv)
        {
            auto hm = r2.removeFromRight(66); r2.removeFromRight(2);
            if (harmModeSel) harmModeSel->setBounds(hm.reduced(2));
        }
        auto tc = r2.removeFromRight(66); r2.removeFromRight(2);
        if (tubeTypeSel) tubeTypeSel->setBounds(tc.reduced(2));
        auto tb = r2.removeFromRight(62); r2.removeFromRight(2);
        if (transformerTog) transformerTog->setBounds(tb.reduced(1));
        layoutKnobsH(r2, r2knobs);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// ChorusPanel  (2 rows)
// Row 1: LFO1Freq | LFO2Freq | LFO3Freq  +  LFO1Wave | LFO2Wave | LFO3Wave combos
// Row 2: Delay | Depth | Stereo | CrossCut | Wet  +  Voices combo  +  CrossType combo
// ─────────────────────────────────────────────────────────────────────────────
struct ChorusPanel : public EditorPanelBase
{
    std::vector<std::unique_ptr<VKnob>>                    lfoFreqKnobs, r2knobs;
    std::array<std::unique_ptr<ChickenHeadSelector>, 3>    lfoWaveSel;
    std::unique_ptr<DualLabelToggle>                       voicesTog, crossTypeTog, wetOnlyTog;

    std::vector<VKnob*> getExtraKnobs() override
    {
        std::vector<VKnob*> v;
        for (auto& k : lfoFreqKnobs) if (k) v.push_back(k.get());
        for (auto& k : r2knobs)      if (k) v.push_back(k.get());
        return v;
    }

    // QA-EffectsReview Task 4: Basic = the exact FL Fruity Chorus control set
    // (Delay/Depth/Stereo + 3 LFO Freq + 3 LFO Wave + Cross Type/Cutoff + Wet
    // Only).  Our ONLY additions = the 3/6 Voices toggle + the continuous Wet
    // level knob -> those hide in Basic.  resized() does the show/hide + reflow.
    bool hasAdvancedControls() const override { return true; }

    // Modulation panels use default VC::Panel background
    ~ChorusPanel() override { setLookAndFeel(nullptr); }

    explicit ChorusPanel(ChorusDSP* dsp)
    {
        disableVU();   // Chorus has no input VU - full knob width
        setLookAndFeel(&ModulationLAF::get());
        buildKnobs(*this, lfoFreqKnobs, {
            { "LFO1",  0.01f, 10.f, 0.5f, 0.01f, "LFO 1 rate (Hz)" },
            { "LFO2",  0.01f, 10.f, 0.8f, 0.01f, "LFO 2 rate (Hz)" },
            { "LFO3",  0.01f, 10.f, 1.2f, 0.01f, "LFO 3 rate (Hz)" },
        });
        lfoFreqKnobs[0]->slider.onValueChange = [dsp,this]{ dsp->setLFOFreq(0,(float)lfoFreqKnobs[0]->slider.getValue()); };
        lfoFreqKnobs[1]->slider.onValueChange = [dsp,this]{ dsp->setLFOFreq(1,(float)lfoFreqKnobs[1]->slider.getValue()); };
        lfoFreqKnobs[2]->slider.onValueChange = [dsp,this]{ dsp->setLFOFreq(2,(float)lfoFreqKnobs[2]->slider.getValue()); };

        const std::vector<ChickenHeadSelector::Option> waveOpts = {
            { "S", "Sine",     "Pure sinusoidal modulation - smoothest, cleanest wobble" },
            { "T", "Triangle", "Triangle wave - linear ramp, slightly more pronounced pitch swing" },
            { "M", "Multi",    "Multi-sine (sin phi + sin 3phi) - harmonic blend, classic digital chorus" },
            { "O", "Organic",  "Non-harmonic (sin phi + sin 0.37phi) - slow secondary drift, hardware-like" },
        };
        for (int i = 0; i < 3; ++i)
        {
            lfoWaveSel[i] = std::make_unique<ChickenHeadSelector>();
            lfoWaveSel[i]->setOptions(waveOpts);
            lfoWaveSel[i]->setBodyTooltip("LFO " + juce::String(i+1) + " waveform");
            // A9: initialize from DSP so preset-loaded wave state shows correctly.
            lfoWaveSel[i]->setSelectedIndex(juce::jlimit(0, 3, dsp->lfoParams[(size_t) i].wave),
                                            juce::dontSendNotification);
            lfoWaveSel[i]->onChange = [dsp, idx = i](int newIdx) { dsp->setLFOWave(idx, newIdx); };
            addAndMakeVisible(*lfoWaveSel[i]);
        }

        buildKnobs(*this, r2knobs, {
            { "Delay",   0.5f,   30.f,  8.f,  0.1f, "Base delay (ms)" },
            { "Depth",   0.f,    20.f,  8.f,  0.1f, "Modulation depth (ms)" },
            { "Stereo",  0.f,   360.f,120.f,  1.f,  "L/R LFO phase offset (deg)" },
            { "CrossHz", 20.f,10000.f,800.f,  5.f,  "Crossover cutoff (Hz)" },
            { "Wet",     0.f,     1.f,  0.5f, 0.01f,"Wet level" },
        });

        voicesTog = std::make_unique<DualLabelToggle>();
        voicesTog->setupNamed(
            "3 voices", "Classic 3-voice chorus (1 per LFO) - lighter, cleaner",
            "6 voices", "Dense 6-voice chorus (2 per LFO, pi-offset pairs) - thicker, more diffuse");
        // A9: toggleState 'on' = 6 voices.
        voicesTog->btn().setToggleState(dsp->voices == 6, juce::dontSendNotification);
        voicesTog->btn().onClick = [dsp, this]{
            dsp->setVoices(voicesTog->btn().getToggleState() ? 6 : 3);
        };
        addAndMakeVisible(*voicesTog);

        crossTypeTog = std::make_unique<DualLabelToggle>();
        crossTypeTog->setupNamed(
            "HP band", "High-pass band is chorused (low end stays dry) - preserves bass tightness",
            "LP band", "Low-pass band is chorused (highs stay dry) - adds wobble to low end");
        // A9: toggleState 'on' = LP (crossType == 1).
        crossTypeTog->btn().setToggleState(dsp->crossType == 1, juce::dontSendNotification);
        crossTypeTog->btn().onClick = [dsp, this]{
            dsp->setCrossType(crossTypeTog->btn().getToggleState() ? 1 : 0);
        };
        addAndMakeVisible(*crossTypeTog);

        wetOnlyTog = std::make_unique<DualLabelToggle>();
        wetOnlyTog->setupOnOff("Wet Only",
            "Kills dry + pass-band return - output is chorused signal only (for effect sends)");
        // A9
        wetOnlyTog->btn().setToggleState(dsp->wetOnly, juce::dontSendNotification);
        wetOnlyTog->btn().onClick = [dsp, this]{ dsp->setWetOnly(wetOnlyTog->btn().getToggleState()); };
        addAndMakeVisible(*wetOnlyTog);

        r2knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setDelay      ((float)r2knobs[0]->slider.getValue()); };
        r2knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setDepth      ((float)r2knobs[1]->slider.getValue()); };
        r2knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setStereo     ((float)r2knobs[2]->slider.getValue()); };
        r2knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setCrossCutoff((float)r2knobs[3]->slider.getValue()); };
        r2knobs[4]->slider.onValueChange = [dsp,this]{ dsp->setWet        ((float)r2knobs[4]->slider.getValue()); };

        // A9 slider sync from DSP state + clamp cleanup.
        lfoFreqKnobs[0]->slider.setValue(dsp->lfoParams[0].freq, juce::sendNotificationSync);
        lfoFreqKnobs[1]->slider.setValue(dsp->lfoParams[1].freq, juce::sendNotificationSync);
        lfoFreqKnobs[2]->slider.setValue(dsp->lfoParams[2].freq, juce::sendNotificationSync);
        r2knobs[0]->slider.setValue(dsp->delayMs,     juce::sendNotificationSync);
        r2knobs[1]->slider.setValue(dsp->depth,       juce::sendNotificationSync);
        r2knobs[2]->slider.setValue(dsp->stereo,      juce::sendNotificationSync);
        r2knobs[3]->slider.setValue(dsp->crossCutoff, juce::sendNotificationSync);
        r2knobs[4]->slider.setValue(dsp->wet,         juce::sendNotificationSync);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(4, 2);

        // Meter strips
        if (vuIn) vuIn->setBounds(b.removeFromLeft(120).reduced(1, 2));
        dbfsOut      ->setBounds(b.removeFromRight(32).reduced(1, 2));
        b.removeFromRight(2);
        outputVolKnob->setBounds(b.removeFromRight(kKnobSz).withSizeKeepingCentre(kKnobSz, kKnobSz));
        b.removeFromRight(4);

        // QA-EffectsReview Task 4: Basic = the exact Fruity Chorus set; Advanced
        // adds the 3/6 Voices toggle + the continuous Wet level knob (r2knobs[4]).
        const bool adv = ! mBasicMode;
        if (voicesTog) voicesTog->setVisible (adv);
        if (r2knobs.size() > 4 && r2knobs[4]) r2knobs[4]->setVisible (adv);   // Wet level knob

        auto r1 = b.removeFromTop(b.getHeight() / 2);
        auto r2 = b;

        // Row 1: LFO freq knobs (left) + LFO wave selectors (right) -- all Fruity.
        int half = r1.getWidth() / 2;
        auto freqArea = r1.removeFromLeft(half);
        layoutKnobsH(freqArea, lfoFreqKnobs);
        int cw = r1.getWidth() / 3;
        for (int i = 0; i < 3; ++i)
            lfoWaveSel[i]->setBounds(r1.removeFromLeft(cw).reduced(2));

        // Row 2: WetOnly | CrossType always (Fruity); Voices (addition) Advanced only.
        const int togW = 66;
        auto wo  = r2.removeFromRight(togW); r2.removeFromRight(2);
        if (wetOnlyTog)   wetOnlyTog  ->setBounds(wo.reduced(1));
        auto ctc = r2.removeFromRight(togW); r2.removeFromRight(2);
        if (crossTypeTog) crossTypeTog->setBounds(ctc.reduced(1));
        if (adv)
        {
            auto vc  = r2.removeFromRight(togW); r2.removeFromRight(2);
            if (voicesTog) voicesTog   ->setBounds(vc.reduced(1));
        }
        // Row-2 knobs: Delay/Depth/Stereo/CrossHz always (Fruity); Wet (idx 4, our
        // addition) only in Advanced.
        std::vector<VKnob*> r2vis;
        for (int i = 0; i < (int) r2knobs.size(); ++i)
            if (adv || i != 4)
                if (r2knobs[i]) r2vis.push_back (r2knobs[i].get());
        layoutKnobsH(r2, r2vis);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// DelayPanel  (2 rows + a 3x2 selector grid at the right edge)
// Row 1: Time | Feed | LoFiSR | WetIn | Wet | Dry | FBCut | FBReso | Tone
//        (+ Duck | DkThr | DkAtt | DkRel in Advanced)   [Model][SyncDiv][FBFilter]
// Row 2: ModHz | ModTime | ModFB | Diff | DiffSprd | LoBit | FBDst | FBKnee | FBSym | Spread | Pan | Smooth
//                                                     [Pitch][ BPM ][Limit/Sat]
// Basic keeps Time/Feed/Wet/Dry/Tone on row 1, FBDst/FBKnee/FBSym/Smooth on
// row 2, and every selector; no control ever changes row between modes.
//
// The feedback transfer curve left this panel at QA-Layout T20 (2026-08-05) for
// the Delay's Visual window, so all of the Delay's drawing lives in one place
// and its 46 px box stops eating row 2.  The draw is a 96-point sweep of
// DelayDSP::shapeFeedbackForDisplay; T18 re-raises it there alongside the
// beat-grid repeats.
// ─────────────────────────────────────────────────────────────────────────────
struct DelayPanel : public EditorPanelBase,
                    private juce::Timer
{
    std::vector<std::unique_ptr<VKnob>>         r1knobs, r2knobs, duckKnobs;
    std::unique_ptr<DualLabelToggle>            tempoTog, keepPitchTog, fbDistTypeTog;
    std::unique_ptr<ChickenHeadSelector>        modelSel, fbFilterTypeSel, syncDivSel;
    DelayDSP*                                   mDelayDsp { nullptr };
    float                                       mLastRingLive { -1.0f };

    // QA-Layout T20 (Jeff, 2026-08-05): Basic is a locked short list, no longer
    // "the reference face minus our additions".  The reference unit is itself
    // fairly advanced, and hiding only the Duck cluster left Basic rendering
    // twelve row-2 knobs inside a 691 px window -- the same panel squeezed,
    // not a simpler one.  resized() does show/hide + reflow for BOTH rows.
    bool hasAdvancedControls() const override { return true; }

    std::vector<VKnob*> getExtraKnobs() override
    {
        std::vector<VKnob*> v;
        for (auto& k : r1knobs)   if (k) v.push_back(k.get());
        for (auto& k : r2knobs)   if (k) v.push_back(k.get());
        for (auto& k : duckKnobs) if (k) v.push_back(k.get());
        return v;
    }

    // CL-299 (4): selector position -> DelayDSP model value.  The DSP's
    // numbering (0=Stereo 1=Mono 2=PingPong 3=Off) is what gets serialized and
    // does not move; only the order they are OFFERED in does.
    static constexpr int kModelOptionValues[4] = { 1, 0, 2, 3 };

    static int displayIndexForModel (int dspModel)
    {
        for (int i = 0; i < 4; ++i)
            if (kModelOptionValues[i] == dspModel) return i;
        return 0;
    }

    // C4 -- numerator/denominator pairs for the 8 sync divisions the selector
    // can choose. Ordered long-to-short. `setSyncNote(num, den)` means the
    // delay time equals `num/den` of a whole note; triplets use den*3/2,
    // dotted notes use num*3 (so 1/8D = 3/16).
    static constexpr int kSyncDivsCount = 8;
    static constexpr int kSyncDivNumer[kSyncDivsCount] = { 1, 1, 1, 1, 3, 1, 1, 1 };
    static constexpr int kSyncDivDenom[kSyncDivsCount] = { 1, 2, 4, 8, 16, 6, 16, 12 };
    // ^ 1/1, 1/2, 1/4, 1/8, 1/8D (=3/16), 1/4T (=1/6), 1/16, 1/8T (=1/12)

    explicit DelayPanel(DelayDSP* dsp)
        : mDelayDsp (dsp)
    {
        disableVU();   // Delay has no input VU - full knob width
        setLookAndFeel(&TimeLAF::get());

        // Row 1: Time | Feed | LoFiSR | WetIn | Wet | Dry | FBCut | FBReso | Tone  (9 knobs)
        // D.4-Q5 (2026-05-01): LoFiSR sits next to Feed since it only colors the
        // feedback tail.  ModTime + Smooth moved to row 2 next to their dependencies.
        buildKnobs(*this, r1knobs, {
            { "Time",    1.f,  2000.f,  250.f,  1.f,   "Delay time (ms)" },
            { "Feed",    0.f,    1.2f,    0.4f, 0.01f, "Feedback level (>1 builds up)" },
            { "LoFiSR",100.f,48000.f,48000.f, 100.f,  "Lo-Fi sample rate (Hz) - 48000 = full quality; lower = bit-crusher / vintage character on every echo (drop below ~6000 for obvious grit)" },
            { "WetIn",   0.f,    1.f,     1.f,  0.01f, "Input gain INTO delay line - drop to freeze current feedback while muting new input" },
            { "Wet",     0.f,    1.f,     0.3f, 0.01f, "Wet output level" },
            { "Dry",     0.f,    1.f,     1.0f, 0.01f, "Dry output level" },
            { "FBCut",  20.f,18000.f,    80.f,  10.f,  "Feedback filter cutoff (Hz)" },
            { "FBReso",  0.1f,  20.f,   0.707f, 0.01f, "Feedback filter resonance (Q) - essential for BP character, subtle for LP/HP" },
            { "Tone",   -1.f,    1.f,     0.f,  0.01f, "Tone on the wet output (left = low-pass, right = high-pass, center = off)" },
        });

        // CL-299 (1): Feed runs to 1.2, and anything past 1.0 is additive
        // feedback that builds instead of decaying.  The ring goes green ->
        // orange -> red across that top zone so the runaway range is visible
        // before you hear it.  1.0 / 1.2 is where 100 % sits on this knob.
        if (r1knobs.size() > 1 && r1knobs[1])
            r1knobs[1]->slider.getProperties().set (TimeLAF::kWarnRingFrom, 1.0 / 1.2);


        // Row 2: ModHz | ModTime | ModFB | Diff | DiffSprd | LoBit | FBDst | FBKnee | FBSym | Spread | Pan | Smooth
        // (+ Duck at idx 12, laid into row 1 in Advanced).  ModTime placed next
        // to ModHz (its LFO-rate dependency).  Smooth at the end near the Pitch
        // toggle (its keep-pitch dependency).
        buildKnobs(*this, r2knobs, {
            { "ModHz",    0.f,  20.f,  0.f,  0.1f,  "LFO modulation rate (Hz)" },
            { "ModTime",  0.f,   1.f,  0.f,  0.01f, "LFO depth on delay TIME (chorus / flange-into-delay character). Requires ModHz > 0 (left of this knob) to hear any change" },
            { "ModFB",    0.f,   1.f,  0.f,  0.01f, "LFO depth on feedback filter cutoff (+/- octaves)" },
            { "Diff",     0.f,   1.f,  0.f,  0.01f, "Diffusion level (allpass smearing)" },
            { "DiffSprd", 0.f,   1.f,  0.5f, 0.01f, "Diffusion spread - scales the 4 allpass base delays (tight 0 to wide 1)" },
            { "LoBit",    1.f,  24.f, 24.f,  0.5f,  "Lo-Fi bit depth (24=off)" },
            { "FBDst",    0.f,  10.f,  1.f,  0.1f,  "Feedback distortion drive" },
            { "FBKnee",   0.f,   1.f,  0.5f, 0.01f, "Curve knee - Limit: 0=hard clip, 1=smooth; Sat: 0=sharp (solid-state), 1=soft (valve)" },
            { "FBSym",    0.f,   1.f,  0.f,  0.01f, "Sat-mode symmetry (0=symmetric, 1=asymmetric DC offset)" },
            { "Spread",   0.f,   1.f,  0.f,  0.01f, "Stereo spread (L/R base delay difference)" },
            { "Pan",     -1.f,   1.f,  0.f,  0.01f, "L/R offset pan" },
            { "Smooth",   0.f,   1.f,  0.5f, 0.01f, "Time smoothing - higher = slower transition when delay time changes. Only audible when the Pitch toggle (right of this row) is ON" },
            { "Duck",     0.f, 100.f,  0.f,  1.0f,  "0..100 %.  Sidechain ducking amount.  When the trigger (SC source, or the dry input if none picked) crosses the DkThr threshold, the wet output is attenuated by this amount.  0 = disabled (vocal-friendly setting around 60 %)." },
        });

        // QA-EffectsReview Task 9: duck envelope params (Advanced) -- were
        // DSP-only defaults; now knobs alongside the Duck amount.
        buildKnobs(*this, duckKnobs, {
            { "DkThr", -60.f,    0.f, -24.f, 1.f, "Duck threshold (dB) - trigger level where ducking starts" },
            { "DkAtt",   1.f,  200.f,  10.f, 1.f, "Duck attack (ms) - how fast the wet ducks once the trigger crosses threshold" },
            { "DkRel",  10.f, 1000.f, 200.f, 5.f, "Duck release (ms) - how fast the wet recovers after the trigger falls back" },
        });

        // CL-299 (4): display order matches the reference (Mono / Stereo /
        // PingPong / Off).  DISPLAY ONLY -- kModelOptionValues maps a position
        // back to the DSP's own numbering, which is unchanged, so every saved
        // project keeps the model it had.
        modelSel = std::make_unique<ChickenHeadSelector>();
        modelSel->setOptions({
            { "M", "Mono",     "Summed mono delay - both channels get the same repeats" },
            { "S", "Stereo",   "Independent left/right delay lines - natural stereo" },
            { "P", "PingPong", "Ping-pong - repeats alternate between left and right" },
            { "O", "Off",      "No echoes - the filter / lo-fi / distortion / tone chain runs instantly on the input (distortion-unit mode)" },
        });
        modelSel->setBodyTooltip("Delay topology");
        // A9: initialize from DSP so preset-loaded model shows correctly.
        modelSel->setSelectedIndex(displayIndexForModel (dsp->getDelayModel()),
                                   juce::dontSendNotification);
        modelSel->onChange = [dsp](int idx)
        {
            dsp->setDelayModel (kModelOptionValues[(size_t) juce::jlimit (0, 3, idx)]);
        };
        addAndMakeVisible(*modelSel);

        fbFilterTypeSel = std::make_unique<ChickenHeadSelector>();
        fbFilterTypeSel->setOptions({
            { "L", "FB LP",  "Low-pass feedback filter - progressively darker repeats" },
            { "H", "FB HP",  "High-pass feedback filter - thinner, brighter repeats" },
            { "B", "FB BP",  "Band-pass feedback filter - narrows on each repeat" },
            { "O", "FB Off", "Feedback filter off - repeats pass unfiltered" },
        });
        fbFilterTypeSel->setBodyTooltip("Feedback-path filter topology");
        // A9: initialize from DSP instead of hardcoded default so preset load reflects stored type.
        fbFilterTypeSel->setSelectedIndex(juce::jlimit(0, 3, dsp->getFBFilterType()), juce::dontSendNotification);
        fbFilterTypeSel->onChange = [dsp](int idx){ dsp->setFeedbackFilterType(idx); };
        addAndMakeVisible(*fbFilterTypeSel);

        fbDistTypeTog = std::make_unique<DualLabelToggle>();
        fbDistTypeTog->setupNamed(
            "Limit", "Feedback drive = hard-limiter - keeps peaks under control",
            "Sat",   "Feedback drive = saturator - softens peaks with harmonics");
        fbDistTypeTog->setLabelColour(VC::Text);
        // A9: toggleState 'on' = Sat (mFBDistType == 1).
        fbDistTypeTog->btn().setToggleState(dsp->getFBDistType() == 1, juce::dontSendNotification);
        fbDistTypeTog->btn().onClick = [dsp, this]{
            dsp->setFBDistType(fbDistTypeTog->btn().getToggleState() ? 1 : 0);
        };
        addAndMakeVisible(*fbDistTypeTog);

        tempoTog = std::make_unique<DualLabelToggle>();
        tempoTog->setupOnOff("BPM", "Sync delay time to host BPM (select note division in the Sync selector)");
        // A6 -- Initialize from DSP so preset-loaded sync state shows correctly.
        tempoTog->btn().setToggleState(dsp->syncBPM, juce::dontSendNotification);
        tempoTog->btn().onClick = [dsp, this]{
            const bool on = tempoTog->btn().getToggleState();
            dsp->setTempoSync(on);
            applyTimeLockout(on);   // A6: grey Time knob when sync is on
        };
        tempoTog->setLabelColour(VC::Text);   // TimeLAF dark Pultec panel
        addAndMakeVisible(*tempoTog);

        keepPitchTog = std::make_unique<DualLabelToggle>();
        keepPitchTog->setupOnOff("Pitch", "Keep pitch when changing delay time (smooth crossfade)");
        // A9
        keepPitchTog->btn().setToggleState(dsp->getKeepPitch(), juce::dontSendNotification);
        keepPitchTog->btn().onClick = [dsp, this]{ dsp->setKeepPitch(keepPitchTog->btn().getToggleState()); };
        keepPitchTog->setLabelColour(VC::Text);
        addAndMakeVisible(*keepPitchTog);

        // C4 -- Sync-division chicken-head. 8 positions, ordered long-to-short,
        // with triplet + dotted options. Only applies when BPM toggle is on.
        syncDivSel = std::make_unique<ChickenHeadSelector>();
        syncDivSel->setOptions({
            { "1/1",  "Whole",    "Whole note (1/1) per delay tap" },
            { "1/2",  "Half",     "Half note (1/2) per delay tap" },
            { "1/4",  "Quarter",  "Quarter note (1/4) per delay tap (default)" },
            { "1/8",  "Eighth",   "Eighth note (1/8) per delay tap" },
            { "1/8D", "8th Dot.", "Dotted 8th (= 3/16) per delay tap" },
            { "1/4T", "Qtr Trip", "Quarter triplet (= 1/6) per delay tap" },
            { "1/16", "16th",     "Sixteenth note (1/16) per delay tap" },
            { "1/8T", "8th Trip", "Eighth triplet (= 1/12) per delay tap" },
        });
        syncDivSel->setBodyTooltip("Tempo-sync note division (when BPM toggle is on)");
        // A9: reverse-lookup stored num/den against the table. Falls back to 1/4 (idx 2) on no match.
        {
            int foundIdx = 2;
            for (int i = 0; i < kSyncDivsCount; ++i)
            {
                if (kSyncDivNumer[i] == dsp->syncNumerator
                    && kSyncDivDenom[i] == dsp->syncDenominator)
                {
                    foundIdx = i;
                    break;
                }
            }
            syncDivSel->setSelectedIndex(foundIdx, juce::dontSendNotification);
        }
        syncDivSel->onChange = [dsp](int idx){
            const int i = juce::jlimit(0, kSyncDivsCount - 1, idx);
            dsp->setSyncNote(kSyncDivNumer[i], kSyncDivDenom[i]);
        };
        addAndMakeVisible(*syncDivSel);


        // Row 1 bindings (9 knobs)
        r1knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setDelayMs           ((float)r1knobs[0]->slider.getValue()); };
        r1knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setFeedbackLevel     ((float)r1knobs[1]->slider.getValue()); };
        r1knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setLoFiSampleRate    ((float)r1knobs[2]->slider.getValue()); };
        r1knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setWetIn             ((float)r1knobs[3]->slider.getValue()); };
        r1knobs[4]->slider.onValueChange = [dsp,this]{ dsp->setWetOut            ((float)r1knobs[4]->slider.getValue()); };
        r1knobs[5]->slider.onValueChange = [dsp,this]{ dsp->setDryOut            ((float)r1knobs[5]->slider.getValue()); };
        r1knobs[6]->slider.onValueChange = [dsp,this]{ dsp->setFeedbackCutoff    ((float)r1knobs[6]->slider.getValue()); };
        r1knobs[7]->slider.onValueChange = [dsp,this]{ dsp->setFeedbackResonance ((float)r1knobs[7]->slider.getValue()); };
        // Task 9: display negated vs the DSP (reference direction: left = LP,
        // right = HP; DSP keeps positive = LP, so old projects sound identical).
        r1knobs[8]->slider.onValueChange = [dsp,this]{ dsp->setTone             (-(float)r1knobs[8]->slider.getValue()); };

        // Row 2 bindings (ModTime at idx 1, Smooth at idx 11, Duck at idx 12)
        r2knobs[0] ->slider.onValueChange = [dsp,this]{ dsp->setModRate           ((float)r2knobs[0] ->slider.getValue()); };
        r2knobs[1] ->slider.onValueChange = [dsp,this]{ dsp->setModTimeMod        ((float)r2knobs[1] ->slider.getValue()); };
        r2knobs[2] ->slider.onValueChange = [dsp,this]{ dsp->setModCutoffMod      ((float)r2knobs[2] ->slider.getValue()); };
        r2knobs[3] ->slider.onValueChange = [dsp,this]{ dsp->setDiffusionLevel    ((float)r2knobs[3] ->slider.getValue()); };
        r2knobs[4] ->slider.onValueChange = [dsp,this]{ dsp->setDiffusionSpread   ((float)r2knobs[4] ->slider.getValue()); };
        r2knobs[5] ->slider.onValueChange = [dsp,this]{ dsp->setLoFiBits          ((float)r2knobs[5] ->slider.getValue()); };
        r2knobs[6] ->slider.onValueChange = [dsp,this]{ dsp->setFBDistLevel       ((float)r2knobs[6] ->slider.getValue()); };
        r2knobs[7] ->slider.onValueChange = [dsp,this]{ dsp->setFBDistKnee        ((float)r2knobs[7] ->slider.getValue()); };
        r2knobs[8] ->slider.onValueChange = [dsp,this]{ dsp->setFBDistSymmetry    ((float)r2knobs[8] ->slider.getValue()); };
        r2knobs[9] ->slider.onValueChange = [dsp,this]{ dsp->setStereoSpread      ((float)r2knobs[9] ->slider.getValue()); };
        r2knobs[10]->slider.onValueChange = [dsp,this]{ dsp->setOffsetPan         ((float)r2knobs[10]->slider.getValue()); };
        r2knobs[11]->slider.onValueChange = [dsp,this]{ dsp->setSmoothing         ((float)r2knobs[11]->slider.getValue()); };
        r2knobs[12]->slider.onValueChange = [dsp,this]{ dsp->setDuckAmount        ((float)r2knobs[12]->slider.getValue()); };

        duckKnobs[0]->slider.onValueChange = [dsp,this]{ dsp->setDuckThresholdDb ((float)duckKnobs[0]->slider.getValue()); };
        duckKnobs[1]->slider.onValueChange = [dsp,this]{ dsp->setDuckAttackMs    ((float)duckKnobs[1]->slider.getValue()); };
        duckKnobs[2]->slider.onValueChange = [dsp,this]{ dsp->setDuckReleaseMs   ((float)duckKnobs[2]->slider.getValue()); };

        // A9 slider sync from DSP state + clamp cleanup (via public getters).
        // delayMs is already a public field on DelayDSP; the rest need getters.
        r1knobs[0]->slider.setValue(dsp->delayMs,                juce::sendNotificationSync);
        r1knobs[1]->slider.setValue(dsp->getFeedbackLevel(),     juce::sendNotificationSync);
        r1knobs[2]->slider.setValue(dsp->getLoFiSampleRate(),    juce::sendNotificationSync);
        r1knobs[3]->slider.setValue(dsp->getWetIn(),             juce::sendNotificationSync);
        r1knobs[4]->slider.setValue(dsp->getWetOut(),            juce::sendNotificationSync);
        r1knobs[5]->slider.setValue(dsp->getDryOut(),            juce::sendNotificationSync);
        r1knobs[6]->slider.setValue(dsp->getFBCutoff(),          juce::sendNotificationSync);
        r1knobs[7]->slider.setValue(dsp->getFBResonance(),       juce::sendNotificationSync);
        r1knobs[8]->slider.setValue(-dsp->getTone(),             juce::sendNotificationSync);
        r2knobs[0] ->slider.setValue(dsp->getModRate(),           juce::sendNotificationSync);
        r2knobs[1] ->slider.setValue(dsp->getModTimeMod(),        juce::sendNotificationSync);
        r2knobs[2] ->slider.setValue(dsp->getModCutoffMod(),      juce::sendNotificationSync);
        r2knobs[3] ->slider.setValue(dsp->getDiffLevel(),         juce::sendNotificationSync);
        r2knobs[4] ->slider.setValue(dsp->getDiffSpread(),        juce::sendNotificationSync);
        r2knobs[5] ->slider.setValue(dsp->getLoFiBits(),          juce::sendNotificationSync);
        r2knobs[6] ->slider.setValue(dsp->getFBDistLevel(),       juce::sendNotificationSync);
        r2knobs[7] ->slider.setValue(dsp->getFBDistKnee(),        juce::sendNotificationSync);
        r2knobs[8] ->slider.setValue(dsp->getFBDistSymmetry(),    juce::sendNotificationSync);
        r2knobs[9] ->slider.setValue(dsp->getStereoSpread(),      juce::sendNotificationSync);
        r2knobs[10]->slider.setValue(dsp->getOffsetPan(),         juce::sendNotificationSync);
        r2knobs[11]->slider.setValue(dsp->getSmoothing(),         juce::sendNotificationSync);
        r2knobs[12]->slider.setValue(dsp->getDuckAmount(),        juce::sendNotificationSync);
        duckKnobs[0]->slider.setValue(dsp->getDuckThresholdDb(),  juce::sendNotificationSync);
        duckKnobs[1]->slider.setValue(dsp->getDuckAttackMs(),     juce::sendNotificationSync);
        duckKnobs[2]->slider.setValue(dsp->getDuckReleaseMs(),    juce::sendNotificationSync);

        // A6 -- apply initial Time-knob lockout state
        applyTimeLockout(dsp->syncBPM);
    }

    // A6 -- When BPM sync is engaged, Time knob is ignored by DSP (sync-division
    // selector drives the time). Grey out + swallow clicks but KEEP tooltip
    // reachable. Inverse for the sync-division chicken-head.
    void applyTimeLockout(bool syncOn)
    {
        if (! r1knobs.empty())
            r1knobs[0]->setLocked(syncOn);
        if (syncDivSel)
            syncDivSel->setLocked(! syncOn);
    }

    ~DelayPanel() override
    {
        stopTimer();
        setLookAndFeel(nullptr);
    }

    // Peer-keyed like every repeating UI cost in the shell: a panel can exist
    // unframed, and a hidden panel polling a meter is pure waste.
    void parentHierarchyChanged() override
    {
        if (getPeer() != nullptr) { if (! isTimerRunning()) startTimerHz (15); }
        else if (isTimerRunning()) stopTimer();
    }

    // CL-299 (1) second half: feed the Feed knob's ring the feedback level
    // actually occurring.  liveDsp() first, ALWAYS (Jeff, 2026-08-05 crash
    // class) -- a load can replace the DSP under this timer.
    void timerCallback() override
    {
        if (liveDsp() == nullptr || mDelayDsp == nullptr) return;
        if (r1knobs.size() < 2 || ! r1knobs[1]) return;

        // Level -> arc position: the ring shares the knob's 0..1.2 scale, so a
        // circulating level of 1.0 lights exactly up to the knob's "100%" mark
        // and the red zone is the same zone on both.
        const float norm = juce::jlimit (0.0f, 1.0f,
                                         mDelayDsp->getFeedbackEnvForDisplay() / 1.2f);
        if (std::abs (norm - mLastRingLive) < 0.004f) return;
        mLastRingLive = norm;
        r1knobs[1]->slider.getProperties().set (TimeLAF::kWarnRingLive, norm);
        r1knobs[1]->slider.repaint();
    }

    void paint(juce::Graphics& g) override
    {
        TimeLAF::paintPultecPanel(g, getLocalBounds());
    }

    // Which knobs survive into Basic, by index into their row's vector.
    // Locked by Jeff 2026-08-05: Time / Feed / Wet / Dry / Tone up top; the
    // three feedback-DRIVE knobs (not FBCut/FBReso, which are the feedback
    // FILTER) plus Smooth below.  Kept as members so the two lists sit beside
    // each other rather than being buried mid-resized().
    static constexpr int kR1Basic[] = { 0, 1, 4, 5, 8 };   // Time Feed Wet Dry Tone
    static constexpr int kR2Basic[] = { 6, 7, 8, 11 };     // FBDst FBKnee FBSym Smooth

    static bool inBasic (const int* list, int n, int idx)
    {
        for (int i = 0; i < n; ++i) if (list[i] == idx) return true;
        return false;
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(4, 2);

        // Meter strips
        if (vuIn) vuIn->setBounds(b.removeFromLeft(120).reduced(1, 2));
        dbfsOut      ->setBounds(b.removeFromRight(32).reduced(1, 2));
        b.removeFromRight(2);
        outputVolKnob->setBounds(b.removeFromRight(kKnobSz).withSizeKeepingCentre(kKnobSz, kKnobSz));
        b.removeFromRight(4);

        const bool adv = ! mBasicMode;

        // T20: BOTH rows filter now.  Row 2 never did, so Basic laid out the
        // same twelve knobs the Advanced view has inside a 691 px window --
        // ~23 px a slot, which is below the point where a VKnob's label has any
        // room and is why they read "M..." / "Df...".
        auto visibleFrom = [adv] (std::vector<std::unique_ptr<VKnob>>& src,
                                  const int* basicList, int basicCount, int upTo)
        {
            std::vector<VKnob*> out;
            for (int i = 0; i < upTo && i < (int) src.size(); ++i)
            {
                if (! src[i]) continue;
                const bool show = adv || inBasic (basicList, basicCount, i);
                src[i]->setVisible (show);
                if (show) out.push_back (src[i].get());
            }
            return out;
        };

        auto row1 = visibleFrom (r1knobs, kR1Basic, (int) (sizeof (kR1Basic) / sizeof (kR1Basic[0])),
                                 (int) r1knobs.size());
        auto row2 = visibleFrom (r2knobs, kR2Basic, (int) (sizeof (kR2Basic) / sizeof (kR2Basic[0])), 12);

        // Duck amount is stored in r2knobs but lays out with its envelope trio
        // at the end of row 1; the whole cluster is Advanced-only.
        if (r2knobs.size() > 12 && r2knobs[12])
        {
            r2knobs[12]->setVisible (adv);
            if (adv) row1.push_back (r2knobs[12].get());
        }
        for (auto& k : duckKnobs)
        {
            if (! k) continue;
            k->setVisible (adv);
            if (adv) row1.push_back (k.get());
        }

        auto r1 = b.removeFromTop(b.getHeight() / 2);
        auto r2 = b;

        // T20 selector grid, replacing two chicken-heads up top and four
        // controls plus the curve box below:
        //
        //   [ Model ] [ SyncDiv ] [ FB Filter ]
        //   [ Pitch ] [   BPM   ] [ Limit/Sat ]
        //
        // One column width across both rows so the pairs line up, and sync
        // division sits directly above the BPM switch that gates it (Jeff,
        // 2026-08-05 -- it used to float mid-row beside the curve).
        //
        // Cells are sized to their CONTENT, not to the row: a DualLabelToggle
        // draws from the top of its bounds, so a full-row-height cell left ~60 px
        // of blank under every switch, and ChickenHeadSelector's knob is
        // min(w,h) minus a 26 px letter ring, so a 66-wide cell shrank the head
        // to 32 px next to 44 px knobs.  74 is the width that puts the head back
        // at kKnobSz; 54 is the OnOff toggle's natural stack (12+10+22+10).
        constexpr int kSelW = 74;
        constexpr int kSelH = 74;
        constexpr int kTogH = 54;

        auto cell = [] (juce::Rectangle<int>& row, int w, int h)
        {
            auto slot = row.removeFromRight (w);
            row.removeFromRight (2);
            return slot.withSizeKeepingCentre (w, juce::jmin (h, slot.getHeight()));
        };

        if (fbFilterTypeSel) fbFilterTypeSel->setBounds (cell (r1, kSelW, kSelH));
        if (syncDivSel)      syncDivSel     ->setBounds (cell (r1, kSelW, kSelH));
        if (modelSel)        modelSel       ->setBounds (cell (r1, kSelW, kSelH));

        if (fbDistTypeTog)   fbDistTypeTog  ->setBounds (cell (r2, kSelW, kTogH));
        if (tempoTog)        tempoTog       ->setBounds (cell (r2, kSelW, kTogH));
        if (keepPitchTog)    keepPitchTog   ->setBounds (cell (r2, kSelW, kTogH));

        layoutKnobsH (r1, row1);
        layoutKnobsH (r2, row2);

    }
};

// ─────────────────────────────────────────────────────────────────────────────
// VocalDoublerDelayPanel - H-8 (2026-05-02)
// Renders when DelayDSP::Type == VocalDoubler.  Single row:
// Time L | Time R | Detune | Width | Rate | Mix (+ Duck | DkThr | DkAtt |
// DkRel in Advanced).  Shares the TimeLAF look with the Echo
// DelayPanel so switching Mode keeps a coherent visual family.
// No feedback / lo-fi / mod LFO / diffusion -- VocalDoubler is fundamentally
// a different effect (dual short detuned taps, no FB).
// ─────────────────────────────────────────────────────────────────────────────
struct VocalDoublerDelayPanel : public EditorPanelBase
{
    std::vector<std::unique_ptr<VKnob>> knobsRow, duckKnobs;

    // QA-EffectsReview Task 9: the doubler Type is itself our addition, but
    // its six core knobs ARE the effect -- only the Duck cluster (an addition
    // on top of the doubler) hides in Basic.
    bool hasAdvancedControls() const override { return true; }

    std::vector<VKnob*> getExtraKnobs() override
    {
        std::vector<VKnob*> v;
        for (auto& k : knobsRow)  if (k) v.push_back (k.get());
        for (auto& k : duckKnobs) if (k) v.push_back (k.get());
        return v;
    }

    explicit VocalDoublerDelayPanel (DelayDSP* dsp)
    {
        disableVU();
        setLookAndFeel (&TimeLAF::get());

        buildKnobs (*this, knobsRow, {
            { "Time L",  5.f,   50.f, 13.f, 0.1f,
                "5..50 ms.  Left-tap delay time.  Short delays (10-20 ms) sound like "
                "doubling; longer (25-50 ms) drift toward slapback territory." },
            { "Time R",  5.f,   50.f, 22.f, 0.1f,
                "5..50 ms.  Right-tap delay time.  Set different from Time L for the "
                "classic stereo-doubled vocal effect (e.g. 13 ms L / 22 ms R)." },
            { "Detune",  0.f,  100.f, 50.f, 1.0f,
                "0..100 %.  Drift LFO depth -- adds slow random pitch wobble per tap so the "
                "two virtual takes drift apart like a real double-tracked vocal.  0 = no drift." },
            { "Width",   0.f,  100.f, 80.f, 1.0f,
                "0..100 %.  Stereo spread of the two taps.  0 = both taps centred (mono "
                "doubling); 100 = full L/R split (wide stereo doubling)." },
            { "Rate",    0.1f,   2.f,  0.7f, 0.05f,
                "0.1..2 Hz.  Drift LFO rate.  Slow (0.3-0.7 Hz) feels like natural performer "
                "drift; faster reads as chorus-y wobble." },
            { "Mix",     0.f,  100.f, 50.f, 1.0f,
                "0..100 %.  Wet/dry blend.  100 = all doubled signal; 0 = bypass-equivalent.  "
                "30-60 % is typical for sit-behind doubling." },
            { "Duck",    0.f,  100.f,  0.f, 1.0f,
                "0..100 %.  Sidechain ducking amount.  Pick a SC source on the slot header "
                "(typically the lead vocal) -- when the source crosses the DkThr threshold, "
                "the doubled signal is attenuated so it sits behind the lead.  0 = disabled.  "
                "No SC source = self-ducks on the doubler's own input." },
        });

        // QA-EffectsReview Task 9: duck envelope params (Advanced) -- were
        // DSP-only defaults; now knobs alongside the Duck amount.
        buildKnobs (*this, duckKnobs, {
            { "DkThr", -60.f,    0.f, -24.f, 1.f, "Duck threshold (dB) - trigger level where ducking starts" },
            { "DkAtt",   1.f,  200.f,  10.f, 1.f, "Duck attack (ms) - how fast the doubled signal ducks once the trigger crosses threshold" },
            { "DkRel",  10.f, 1000.f, 200.f, 5.f, "Duck release (ms) - how fast the doubled signal recovers after the trigger falls back" },
        });

        knobsRow[0]->slider.onValueChange = [dsp,this]{ dsp->setDoubleTimeLMs ((float) knobsRow[0]->slider.getValue()); };
        knobsRow[1]->slider.onValueChange = [dsp,this]{ dsp->setDoubleTimeRMs ((float) knobsRow[1]->slider.getValue()); };
        knobsRow[2]->slider.onValueChange = [dsp,this]{ dsp->setDoubleDetune  ((float) knobsRow[2]->slider.getValue()); };
        knobsRow[3]->slider.onValueChange = [dsp,this]{ dsp->setDoubleWidth   ((float) knobsRow[3]->slider.getValue()); };
        knobsRow[4]->slider.onValueChange = [dsp,this]{ dsp->setDoubleRate    ((float) knobsRow[4]->slider.getValue()); };
        knobsRow[5]->slider.onValueChange = [dsp,this]{ dsp->setWetOut        ((float) knobsRow[5]->slider.getValue() * 0.01f); };
        knobsRow[6]->slider.onValueChange = [dsp,this]{ dsp->setDuckAmount    ((float) knobsRow[6]->slider.getValue()); };
        duckKnobs[0]->slider.onValueChange = [dsp,this]{ dsp->setDuckThresholdDb ((float) duckKnobs[0]->slider.getValue()); };
        duckKnobs[1]->slider.onValueChange = [dsp,this]{ dsp->setDuckAttackMs    ((float) duckKnobs[1]->slider.getValue()); };
        duckKnobs[2]->slider.onValueChange = [dsp,this]{ dsp->setDuckReleaseMs   ((float) duckKnobs[2]->slider.getValue()); };

        if (dsp)
        {
            knobsRow[0]->slider.setValue (dsp->getDoubleTimeLMs(), juce::sendNotificationSync);
            knobsRow[1]->slider.setValue (dsp->getDoubleTimeRMs(), juce::sendNotificationSync);
            knobsRow[2]->slider.setValue (dsp->getDoubleDetune(),  juce::sendNotificationSync);
            knobsRow[3]->slider.setValue (dsp->getDoubleWidth(),   juce::sendNotificationSync);
            knobsRow[4]->slider.setValue (dsp->getDoubleRate(),    juce::sendNotificationSync);
            knobsRow[5]->slider.setValue (dsp->getWetOut() * 100.0f, juce::sendNotificationSync);
            knobsRow[6]->slider.setValue (dsp->getDuckAmount(),    juce::sendNotificationSync);
            duckKnobs[0]->slider.setValue (dsp->getDuckThresholdDb(), juce::sendNotificationSync);
            duckKnobs[1]->slider.setValue (dsp->getDuckAttackMs(),    juce::sendNotificationSync);
            duckKnobs[2]->slider.setValue (dsp->getDuckReleaseMs(),   juce::sendNotificationSync);
        }

    }

    ~VocalDoublerDelayPanel() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        TimeLAF::paintPultecPanel (g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);

        // DBFS meter + output knob on the right (shared base-class layout).
        if (dbfsOut)         dbfsOut->setBounds (b.removeFromRight (24).reduced (1, 2));
        b.removeFromRight (2);
        if (outputVolKnob)   outputVolKnob->setBounds (b.removeFromRight (kKnobSz)
                                                          .withSizeKeepingCentre (kKnobSz, kKnobSz));
        b.removeFromRight (4);

        // Task 9 Basic/Advanced: the Duck cluster is our addition on top
        // of the doubler -> Advanced only.
        const bool adv = ! mBasicMode;
        if (knobsRow.size() > 6 && knobsRow[6]) knobsRow[6]->setVisible (adv);
        for (auto& k : duckKnobs) if (k) k->setVisible (adv);

        std::vector<VKnob*> row;
        for (int i = 0; i < (int) knobsRow.size(); ++i)
            if (knobsRow[i] && (adv || i != 6)) row.push_back (knobsRow[i].get());
        if (adv)
            for (auto& k : duckKnobs) if (k) row.push_back (k.get());
        layoutKnobsH (b, row, kKnobSz);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// FlangerPanel (2 rows)
// Row 1: Rate | Depth | Delay | Feed | Phase | Shape | DampHz | Wet | Cross
//        + SyncDiv chicken-head + BPM on/off
// Row 2: InvFB | InvW + output fader/meter
// Laid out as a single row when possible; split on narrow widths by convention.
// ─────────────────────────────────────────────────────────────────────────────
struct FlangerPanel : public EditorPanelBase
{
    std::unique_ptr<DualLabelToggle>      bpmTog, invFbTog, invWetTog;
    std::unique_ptr<ChickenHeadSelector>  syncDivSel;    // C4

    // Modulation panels use default VC::Panel background
    ~FlangerPanel() override { setLookAndFeel(nullptr); }

    explicit FlangerPanel(FlangerDSP* dsp)
    {
        disableVU();   // Flanger has no input VU - full knob width
        setLookAndFeel(&ModulationLAF::get());
        buildKnobs(*this, knobs, {
            { "Rate",   0.05f,    5.f,  0.5f,  0.01f, "LFO rate (Hz)" },
            { "Depth",  0.f,     10.f,  5.f,   0.1f,  "Sweep depth (ms)" },
            { "Delay",  0.f,     20.f,  0.f,   0.1f,  "Base (min) delay (ms)" },
            { "Feed",   0.f,    100.f, 50.f,   1.f,   "Feedback (%) - negative via InvFB toggle" },
            { "Phase",  0.f,    360.f,  0.f,   1.f,   "Stereo R-channel LFO offset (deg)" },
            { "Shape",  0.f,      1.f,  0.f,   0.01f, "LFO shape morph (0 = pure sine, 1 = pure triangle)" },
            { "Damp",   0.f,     1.f,  0.f,   0.01f, "Feedback damping: 0 = off/bright, 1 = max (warm, tames harshness)" },
            { "Wet",    0.f,      1.f,  0.5f,  0.01f, "Wet / dry blend" },
            { "Cross",-96.f,      0.f,-96.f,   0.5f,  "Cross-channel wet mix in dB (L->R, R->L). -96 = off; raise for stereo 'spinning' width" },
        });

        bpmTog = std::make_unique<DualLabelToggle>();
        bpmTog->setupOnOff("BPM", "Sync LFO to host BPM using the Sync-division selector (default 1/8)");
        // A6 -- Initialize toggle state from DSP so a preset-loaded sync state shows correctly.
        bpmTog->btn().setToggleState(dsp->mSyncBPM, juce::dontSendNotification);
        bpmTog->btn().onClick = [dsp, this]{
            const bool on = bpmTog->btn().getToggleState();
            dsp->setSyncBPM(on);
            applyRateLockout(on);   // A6: grey Rate knob when sync is engaged
        };
        bpmTog->setLabelColour(VC::Text);   // ModulationLAF dark panel
        addAndMakeVisible(*bpmTog);

        invFbTog = std::make_unique<DualLabelToggle>();
        invFbTog->setupOnOff("InvFB", "Invert feedback signal (through-zero flanging)");
        // A9
        invFbTog->btn().setToggleState(dsp->mInvertFeedback, juce::dontSendNotification);
        invFbTog->btn().onClick = [dsp, this]{ dsp->setInvertFeedback(invFbTog->btn().getToggleState()); };
        invFbTog->setLabelColour(VC::Text);
        addAndMakeVisible(*invFbTog);

        invWetTog = std::make_unique<DualLabelToggle>();
        invWetTog->setupOnOff("InvW", "Invert wet signal");
        // A9
        invWetTog->btn().setToggleState(dsp->mInvertWet, juce::dontSendNotification);
        invWetTog->btn().onClick = [dsp, this]{ dsp->setInvertWet(invWetTog->btn().getToggleState()); };
        invWetTog->setLabelColour(VC::Text);
        addAndMakeVisible(*invWetTog);

        // C4 -- Sync-division chicken-head selector (mirrors Delay panel).
        syncDivSel = std::make_unique<ChickenHeadSelector>();
        syncDivSel->setOptions({
            { "1/1",  "Whole",    "Whole note (1/1) per LFO cycle" },
            { "1/2",  "Half",     "Half note (1/2) per LFO cycle" },
            { "1/4",  "Quarter",  "Quarter note (1/4) per LFO cycle" },
            { "1/8",  "Eighth",   "Eighth note (1/8) per LFO cycle (default)" },
            { "1/8D", "8th Dot.", "Dotted 8th (= 3/16) per LFO cycle" },
            { "1/4T", "Qtr Trip", "Quarter triplet (= 1/6) per LFO cycle" },
            { "1/16", "16th",     "Sixteenth note (1/16) per LFO cycle" },
            { "1/8T", "8th Trip", "Eighth triplet (= 1/12) per LFO cycle" },
        });
        syncDivSel->setBodyTooltip("Tempo-sync note division (when BPM toggle is on)");
        // A9: initialize from DSP so preset-loaded division shows correctly.
        syncDivSel->setSelectedIndex(juce::jlimit(0, FlangerDSP::kNumSyncDivisions - 1,
                                                  dsp->mSyncDivIdx),
                                     juce::dontSendNotification);
        syncDivSel->onChange = [dsp](int idx){ dsp->setSyncDivision(idx); };
        addAndMakeVisible(*syncDivSel);

        knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setRate   ((float)knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setDepth  ((float)knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setDelay  ((float)knobs[2]->slider.getValue()); };
        knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setFeed   ((float)knobs[3]->slider.getValue()); };
        knobs[4]->slider.onValueChange = [dsp,this]{ dsp->setPhase  ((float)knobs[4]->slider.getValue()); };
        knobs[5]->slider.onValueChange = [dsp,this]{ dsp->setShape  ((float)knobs[5]->slider.getValue()); };
        knobs[6]->slider.onValueChange = [dsp,this]{   // 0-1 UI (FL domain) -> Hz DSP, full 200..20000 range
            const float amt = (float) knobs[6]->slider.getValue();
            dsp->setDampHz (20000.0f * std::pow (200.0f / 20000.0f, amt)); };
        knobs[7]->slider.onValueChange = [dsp,this]{ dsp->setWet    ((float)knobs[7]->slider.getValue()); };
        knobs[8]->slider.onValueChange = [dsp,this]{ dsp->setCrossLevel((float)knobs[8]->slider.getValue()); };

        // A9 slider sync from DSP state + clamp cleanup.
        // Feed knob is pct (0-100) on the UI but mFeedback is 0..1 internally; multiply by 100.
        knobs[0]->slider.setValue(dsp->mRate,               juce::sendNotificationSync);
        knobs[1]->slider.setValue(dsp->mDepth,              juce::sendNotificationSync);
        knobs[2]->slider.setValue(dsp->mDelay,              juce::sendNotificationSync);
        knobs[3]->slider.setValue(dsp->mFeedback * 100.0f,  juce::sendNotificationSync);
        knobs[4]->slider.setValue(dsp->mStereoPhase,        juce::sendNotificationSync);
        knobs[5]->slider.setValue(dsp->mShape,              juce::sendNotificationSync);
        knobs[6]->slider.setValue(juce::jlimit (0.0f, 1.0f,    // Hz DSP -> 0-1 UI (inverse of the onChange map)
            std::log (juce::jmax (1.0f, dsp->mDampHz) / 20000.0f) / std::log (200.0f / 20000.0f)),
            juce::sendNotificationSync);
        knobs[7]->slider.setValue(dsp->mWet,                juce::sendNotificationSync);
        knobs[8]->slider.setValue(dsp->mCrossLevelDb,       juce::sendNotificationSync);

        // A6 -- Apply initial lockout state to match DSP sync flag
        applyRateLockout(dsp->mSyncBPM);
    }

    // A6 -- When BPM sync is engaged, Rate knob is ignored by DSP. Grey it out
    // and block value-changing clicks/drags, BUT keep tooltips reachable so
    // users can read why the knob is locked. The inverse applies to the
    // sync-division chicken-head: it's only meaningful while sync is engaged,
    // so grey it out when sync is off.
    void applyRateLockout(bool syncOn)
    {
        if (! knobs.empty())
            knobs[0]->setLocked(syncOn);
        if (syncDivSel)
            syncDivSel->setLocked(! syncOn);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(4, 4);

        // Strip meters (no input VU - disableVU() was called; still need DBFS + fader)
        dbfsOut      ->setBounds(b.removeFromRight(32).reduced(1, 2));
        b.removeFromRight(2);
        outputVolKnob->setBounds(b.removeFromRight(kKnobSz).withSizeKeepingCentre(kKnobSz, kKnobSz));
        b.removeFromRight(4);

        // Right sidebar (taken right-to-left so visual order = SyncDiv | BPM | InvFB | InvW):
        if (invWetTog)
        {
            auto col = b.removeFromRight(56); b.removeFromRight(2);
            invWetTog->setBounds(col.reduced(1));
        }
        if (invFbTog)
        {
            auto col = b.removeFromRight(56); b.removeFromRight(2);
            invFbTog->setBounds(col.reduced(1));
        }
        if (bpmTog)
        {
            auto col = b.removeFromRight(56); b.removeFromRight(2);
            bpmTog->setBounds(col.reduced(1));
        }
        if (syncDivSel)
        {
            auto col = b.removeFromRight(66); b.removeFromRight(2);
            syncDivSel->setBounds(col.reduced(2));
        }
        b.removeFromRight(4);

        layoutKnobsH(b, knobs);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// OverdrivePanel
// Basic (reference): Drive | Color | Band | Filter | Out  +  x100
// Advanced (our additions): Bias + Wet knobs, Parallel toggle, Oversampling selector
// ─────────────────────────────────────────────────────────────────────────────
struct OverdrivePanel : public EditorPanelBase
{
    std::unique_ptr<DualLabelToggle>     x100Tog;
    std::unique_ptr<DualLabelToggle>     parallelTog;   // C4
    std::unique_ptr<ChickenHeadSelector> osSel;         // C5

    // QA-EffectsReview Task 5: Bias + Wet + Parallel + Oversampling are our additions
    // beyond the reference's 6 controls, so this panel gets the Basic/Advanced toggle.
    bool hasAdvancedControls() const override { return true; }

    explicit OverdrivePanel(OverdriveDSP* dsp)
    {
        setLookAndFeel(&HarmonicLAF::get());

        buildKnobs(*this, knobs, {
            { "Drive",   0.f,   10.f,   5.f,  0.1f, "Pre-amp drive" },
            { "Color",  200.f, 8000.f,1000.f, 10.f, "Pre-filter low-pass cutoff (Hz)" },
            { "Band",    0.f,   1.f,   0.5f,  0.01f,"Amount of pre low-pass into the drive (0 = full bandwidth, 1 = full LP at Color)" },
            { "Bias",   -1.f,   1.f,   0.f,   0.01f,"Pre-shaper DC bias (adds even harmonics / tube-like warmth). 0 = symmetric (odd harmonics only)" },
            { "Filter", 500.f,18000.f,8000.f, 10.f, "Post LPF cutoff (Hz)" },
            { "Out",   -18.f,   0.f,   0.f,   0.5f, "Output level -- attenuate-only (-18..0 dB; max = unity gain)" },
            { "Wet",    0.f,    1.f,   1.0f, 0.01f, "Wet/dry mix (1 = full wet)" },
        });

        // x100 on/off toggle
        x100Tog = std::make_unique<DualLabelToggle>();
        x100Tog->setupOnOff("x100", "Extreme x100 drive mode (hard-limited at output). Transition is smoothed over 20 ms so flipping doesn't click");
        x100Tog->btn().setToggleState(dsp->mX100, juce::dontSendNotification);
        x100Tog->btn().onClick = [dsp, this]{ dsp->setX100(x100Tog->btn().getToggleState()); };
        addAndMakeVisible(*x100Tog);

        // Parallel (C4) named toggle: blend mode vs parallel-add.
        parallelTog = std::make_unique<DualLabelToggle>();
        parallelTog->setupNamed(
            "Blend",    "Wet/dry blend mode (default): out = dry*in + wet*processed",
            "Parallel", "Parallel-add mode: out = in + wet*processed (dry stays full, wet adds on top)");
        parallelTog->btn().setToggleState(dsp->getParallel(), juce::dontSendNotification);
        parallelTog->btn().onClick = [dsp, this]{ dsp->setParallel(parallelTog->btn().getToggleState()); };
        addAndMakeVisible(*parallelTog);

        // Oversampling factor (C5) chicken-head: 2x / 4x / 8x / 16x.
        osSel = std::make_unique<ChickenHeadSelector>();
        osSel->setOptions({
            { "2",  "2x",  "2x oversampling - lowest CPU, acceptable aliasing for mild drive" },
            { "4",  "4x",  "4x oversampling (default) - balanced CPU vs aliasing; recommended for most use" },
            { "8",  "8x",  "8x oversampling - cleaner on extreme drive / x100 mode; ~2x CPU vs 4x" },
            { "16", "16x", "16x oversampling - maximum quality; highest CPU. For mastering / heavy fuzz" },
        });
        osSel->setBodyTooltip("Oversampling factor around the waveshaper. Higher = less aliasing on heavy drive, more CPU. Switching updates plugin latency.");
        osSel->setSelectedIndex(juce::jlimit(0, 3, dsp->getOversamplingLog2() - 1), juce::dontSendNotification);
        osSel->onChange = [dsp](int idx){ dsp->setOversamplingFactor(idx + 1); };
        addAndMakeVisible(*osSel);

        knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setPreAmp    ((float)knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setColor     ((float)knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setPreBand   ((float)knobs[2]->slider.getValue()); };
        knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setBias      ((float)knobs[3]->slider.getValue()); };
        knobs[4]->slider.onValueChange = [dsp,this]{ dsp->setPostFilter((float)knobs[4]->slider.getValue()); };
        knobs[5]->slider.onValueChange = [dsp,this]{ dsp->setPostGain  ((float)knobs[5]->slider.getValue()); };
        knobs[6]->slider.onValueChange = [dsp,this]{ dsp->setWet       ((float)knobs[6]->slider.getValue()); };

        // A9 slider sync from DSP state + clamp cleanup.
        knobs[0]->slider.setValue(dsp->mPreAmp,    juce::sendNotificationSync);
        knobs[1]->slider.setValue(dsp->mColor,     juce::sendNotificationSync);
        knobs[2]->slider.setValue(dsp->mPreBand,   juce::sendNotificationSync);
        knobs[3]->slider.setValue(dsp->mBias,      juce::sendNotificationSync);
        knobs[4]->slider.setValue(dsp->mPostFilter,juce::sendNotificationSync);
        knobs[5]->slider.setValue(dsp->mPostGain,  juce::sendNotificationSync);
        knobs[6]->slider.setValue(dsp->mWet,       juce::sendNotificationSync);
    }

    ~OverdrivePanel() override
    {
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        HarmonicLAF::paintHammeritePanel(g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(4, 4);

        // Strip meters (no input VU - Overdrive doesn't use one)
        if (vuIn) vuIn->setBounds(b.removeFromLeft(120).reduced(1, 2));
        dbfsOut      ->setBounds(b.removeFromRight(32).reduced(1, 2));
        b.removeFromRight(2);
        outputVolKnob->setBounds(b.removeFromRight(kKnobSz).withSizeKeepingCentre(kKnobSz, kKnobSz));
        b.removeFromRight(4);

        // QA-EffectsReview Task 5: Basic = the reference's 6 controls (Drive/Color/Band/
        // Filter/Out + x100); Advanced adds the Bias + Wet knobs + Parallel + Oversampling.
        const bool adv = ! mBasicMode;
        if (parallelTog) parallelTog->setVisible(adv);
        if (osSel)       osSel->setVisible(adv);
        if (knobs.size() > 6 && knobs[3]) knobs[3]->setVisible(adv);   // Bias
        if (knobs.size() > 6 && knobs[6]) knobs[6]->setVisible(adv);   // Wet

        // Right sidebar (right-to-left): x100 always; Parallel + OS Advanced only.
        if (x100Tog)
        {
            auto col = b.removeFromRight(62); b.removeFromRight(2);
            x100Tog->setBounds(col.reduced(1));
        }
        if (adv && parallelTog)
        {
            auto col = b.removeFromRight(68); b.removeFromRight(2);
            parallelTog->setBounds(col.reduced(1));
        }
        if (adv && osSel)
        {
            auto col = b.removeFromRight(66); b.removeFromRight(2);
            osSel->setBounds(col.reduced(2));
        }
        b.removeFromRight(4);

        // Knobs: reference knobs always; Bias (idx 3) + Wet (idx 6, our additions) Advanced only.
        std::vector<VKnob*> vis;
        for (int i = 0; i < (int) knobs.size(); ++i)
            if (adv || (i != 3 && i != 6))
                if (knobs[i]) vis.push_back(knobs[i].get());
        layoutKnobsH(b, vis);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// PhaserPanel
// Rate | MinHz | MaxHz | Feed | Stereo | Wet  +  BPM toggle  +  Stages combo  +  Range combo
// ─────────────────────────────────────────────────────────────────────────────
struct PhaserPanel : public EditorPanelBase
{
    std::unique_ptr<ChickenHeadSelector> stagesSel;
    std::unique_ptr<ChickenHeadSelector> waveSel;      // C3
    std::unique_ptr<ChickenHeadSelector> syncDivSel;   // C2
    std::unique_ptr<DualLabelToggle>     bpmTog, invFbTog, rangeTog;

    static constexpr int kStageValues[8] = { 1, 2, 4, 6, 8, 12, 16, 24 };
    static int stageValueToIndex (int v)
    {
        for (int i = 0; i < 8; ++i) if (kStageValues[i] == v) return i;
        return 2;   // default 4-stage
    }

    // Modulation panels use default VC::Panel background
    ~PhaserPanel() override { setLookAndFeel(nullptr); }

    // QA-EffectsReview Task 4: Basic = the exact FL Fruity Phaser control set
    // (Rate / Min / Max / Feedback / Stereo / Dry-Wet / Out-Gain + Range +
    // Stages).  Our additions = Cross knob + BPM sync + Sync division + LFO-wave
    // selector + Invert FB -> hidden in Basic.  resized() does show/hide + reflow.
    bool hasAdvancedControls() const override { return true; }

    explicit PhaserPanel(PhaserDSP* dsp)
    {
        disableVU();   // Phaser has no input VU - full knob width
        setLookAndFeel(&ModulationLAF::get());
        // C1: Rate range upper bound depends on dsp->mFreqRange (0=Slow 2Hz, 1=Fast 10Hz).
        const float rateMax = dsp->getRateMaxHz();
        buildKnobs(*this, knobs, {
            { "Rate",   0.05f, rateMax, juce::jmin(0.5f, rateMax), 0.01f, "LFO rate (Hz) - upper bound follows Slow/Fast Range" },
            { "MinHz",  20.f, 2000.f, 200.f, 5.f,  "Allpass freq lower bound (Hz)" },
            { "MaxHz",  200.f,8000.f,2000.f, 10.f, "Allpass freq upper bound (Hz)" },
            { "Feed",   0.f,   1.2f,  0.5f, 0.01f, "Feedback amount (up to 1.2)" },
            { "Stereo", 0.f,  360.f,  0.f,  1.f,   "L/R LFO phase offset (deg)" },
            { "Wet",    0.f,   1.f,   0.5f, 0.01f, "Wet level" },
            { "Cross",  0.f,   1.f,   0.f,  0.01f, "Cross-channel feedback (0=no cross, 1=full swap)" },
            { "Gain",  -18.f,  18.f,  0.f,  0.5f,  "Output gain (dB)" },
        });

        // C4: log-skew on Rate knob so low rates (0.05-0.5 Hz) get more knob travel.
        knobs[0]->slider.setSkewFactor(0.35);

        bpmTog = std::make_unique<DualLabelToggle>();
        bpmTog->setupOnOff("BPM", "Sync LFO to host BPM (select note division in the Sync selector)");
        // A9: initialize from DSP so preset-loaded sync state shows correctly.
        bpmTog->btn().setToggleState(dsp->mSyncBPM, juce::dontSendNotification);
        bpmTog->btn().onClick = [dsp, this]{
            const bool on = bpmTog->btn().getToggleState();
            dsp->setSyncBPM(on);
            applyRateLockout(on);   // A7: grey Rate knob when sync is engaged
        };
        bpmTog->setLabelColour(VC::Text);   // ModulationLAF dark panel
        addAndMakeVisible(*bpmTog);

        invFbTog = std::make_unique<DualLabelToggle>();
        invFbTog->setupOnOff("InvFB", "Invert feedback sign (flips resonance character)");
        // A9
        invFbTog->btn().setToggleState(dsp->mInvertFeedback, juce::dontSendNotification);
        invFbTog->btn().onClick = [dsp, this]{ dsp->setInvertFeedback(invFbTog->btn().getToggleState()); };
        invFbTog->setLabelColour(VC::Text);
        addAndMakeVisible(*invFbTog);

        stagesSel = std::make_unique<ChickenHeadSelector>();
        stagesSel->setOptions({
            { "1",  "1 stage",  "Single allpass - thin, subtle sweep" },
            { "2",  "2 stages", "2-stage - classic mild phaser" },
            { "4",  "4 stages", "4-stage - standard 'chorusy' phaser" },
            { "6",  "6 stages", "6-stage - richer, more resonant" },
            { "8",  "8 stages", "8-stage - thick, vowel-like notches" },
            { "12", "12 stages","12-stage - deep, dense phasing" },
            { "16", "16 stages","16-stage - very dense, approaching flanger territory" },
            { "24", "24 stages","24-stage - maximum density" },
        });
        stagesSel->setBodyTooltip("Number of allpass stages (more = richer phasing)");
        // A9: reverse-lookup from stored stage count to selector index.
        stagesSel->setSelectedIndex(stageValueToIndex(dsp->mNumStages), juce::dontSendNotification);
        stagesSel->onChange = [dsp](int idx){
            dsp->setStages(kStageValues[juce::jlimit(0, 7, idx)]);
        };
        addAndMakeVisible(*stagesSel);

        // C3: LFO wave chicken-head selector.
        waveSel = std::make_unique<ChickenHeadSelector>();
        waveSel->setOptions({
            { "Sin", "Sine",     "Pure sinusoidal LFO - smoothest, most musical sweep" },
            { "Tri", "Triangle", "Triangle LFO - linear ramp, subtly more pronounced notch travel" },
            { "Saw", "Saw",      "Descending saw LFO - snap-reset sweep, jet-like character" },
            { "S&H", "Smp&Hld",  "Sample-and-hold - random static notch positions, glitchy/stepped feel" },
        });
        waveSel->setBodyTooltip("LFO waveform");
        // A9
        waveSel->setSelectedIndex(dsp->mLFOWaveIdx, juce::dontSendNotification);
        waveSel->onChange = [dsp](int idx){ dsp->setLFOWave(idx); };
        addAndMakeVisible(*waveSel);

        // C2: Sync-division chicken-head (mirrors Delay/Flanger).
        syncDivSel = std::make_unique<ChickenHeadSelector>();
        syncDivSel->setOptions({
            { "1/1",  "Whole",    "Whole note (1/1) per LFO cycle" },
            { "1/2",  "Half",     "Half note (1/2) per LFO cycle" },
            { "1/4",  "Quarter",  "Quarter note (1/4) per LFO cycle (default)" },
            { "1/8",  "Eighth",   "Eighth note (1/8) per LFO cycle" },
            { "1/8D", "8th Dot.", "Dotted 8th (= 3/16) per LFO cycle" },
            { "1/4T", "Qtr Trip", "Quarter triplet (= 1/6) per LFO cycle" },
            { "1/16", "16th",     "Sixteenth note (1/16) per LFO cycle" },
            { "1/8T", "8th Trip", "Eighth triplet (= 1/12) per LFO cycle" },
        });
        syncDivSel->setBodyTooltip("Tempo-sync note division (when BPM toggle is on)");
        // A9
        syncDivSel->setSelectedIndex(dsp->mSyncDivIdx, juce::dontSendNotification);
        syncDivSel->onChange = [dsp](int idx){ dsp->setSyncDiv(idx); };
        addAndMakeVisible(*syncDivSel);

        rangeTog = std::make_unique<DualLabelToggle>();
        rangeTog->setupNamed(
            "Slow", "Slow range - Rate knob limited to 2 Hz max for subtle, drifting sweeps",
            "Fast", "Fast range - Rate knob opens to 10 Hz for aggressive, 'vintage' rate");
        rangeTog->setLabelColour(VC::Text);
        // A9 - rangeTog 'on' state = Fast = mFreqRange == 1.
        rangeTog->btn().setToggleState(dsp->mFreqRange == 1, juce::dontSendNotification);
        rangeTog->btn().onClick = [dsp, this]{
            const int newRange = rangeTog->btn().getToggleState() ? 1 : 0;
            dsp->setFreqRange(newRange);
            // C1: reflect new Rate upper bound on the knob's slider range + current value.
            const double maxHz = dsp->getRateMaxHz();
            knobs[0]->slider.setRange(0.05, maxHz, 0.01);
            knobs[0]->slider.setValue(dsp->mRate, juce::dontSendNotification);
        };
        addAndMakeVisible(*rangeTog);

        knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setSweepFreq((float)knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setMinDepth ((float)knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setMaxDepth ((float)knobs[2]->slider.getValue()); };
        knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setFeedback ((float)knobs[3]->slider.getValue()); };
        knobs[4]->slider.onValueChange = [dsp,this]{ dsp->setStereo   ((float)knobs[4]->slider.getValue()); };
        knobs[5]->slider.onValueChange = [dsp,this]{ dsp->setWet      ((float)knobs[5]->slider.getValue()); };
        knobs[6]->slider.onValueChange = [dsp,this]{ dsp->setCrossFB  ((float)knobs[6]->slider.getValue()); };   // C5
        knobs[7]->slider.onValueChange = [dsp,this]{ dsp->setOutGain  ((float)knobs[7]->slider.getValue()); };

        // A9 slider sync from DSP state + clamp cleanup.
        knobs[0]->slider.setValue(dsp->mSweepHz,     juce::sendNotificationSync);
        knobs[1]->slider.setValue(dsp->mMinDepthHz,  juce::sendNotificationSync);
        knobs[2]->slider.setValue(dsp->mMaxDepthHz,  juce::sendNotificationSync);
        knobs[3]->slider.setValue(dsp->mFeedback,    juce::sendNotificationSync);
        knobs[4]->slider.setValue(dsp->mStereoPhase, juce::sendNotificationSync);
        knobs[5]->slider.setValue(dsp->mWet,         juce::sendNotificationSync);
        knobs[6]->slider.setValue(dsp->mCrossFB,     juce::sendNotificationSync);
        knobs[7]->slider.setValue(dsp->mOutGainDb,   juce::sendNotificationSync);

        // A7: initial lockout state mirrors DSP sync flag.
        applyRateLockout(dsp->mSyncBPM);
    }

    // A7: when BPM sync is engaged, Rate knob is silently overwritten by the
    // sync-division calc - soft-lock it (grey + block clicks) while keeping
    // tooltips reachable. Inverse lockout on the sync-division selector when
    // sync is off (division only matters with sync engaged).
    void applyRateLockout(bool syncOn)
    {
        if (! knobs.empty())
            knobs[0]->setLocked(syncOn);
        if (syncDivSel)
            syncDivSel->setLocked(! syncOn);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(4, 4);

        // Strip meters (no VU - disableVU() was called; still need fader + DBFS)
        dbfsOut      ->setBounds(b.removeFromRight(32).reduced(1, 2));
        b.removeFromRight(2);
        outputVolKnob->setBounds(b.removeFromRight(kKnobSz).withSizeKeepingCentre(kKnobSz, kKnobSz));
        b.removeFromRight(4);

        // QA-EffectsReview Task 4: Basic = the exact Fruity Phaser set; Advanced
        // adds Cross (knob idx 6) + BPM + Sync division + LFO wave + Invert FB.
        const bool adv = ! mBasicMode;
        if (bpmTog)     bpmTog    ->setVisible(adv);
        if (syncDivSel) syncDivSel->setVisible(adv);
        if (waveSel)    waveSel   ->setVisible(adv);
        if (invFbTog)   invFbTog  ->setVisible(adv);
        if (knobs.size() > 6 && knobs[6]) knobs[6]->setVisible(adv);   // Cross

        // Right sidebar: Range + Stages always (Fruity); Wave/SyncDiv/InvFB/BPM Advanced.
        auto rc = b.removeFromRight(56); b.removeFromRight(2);
        if (rangeTog) rangeTog->setBounds(rc.reduced(1));
        auto sc = b.removeFromRight(60); b.removeFromRight(2);
        if (stagesSel) stagesSel->setBounds(sc.reduced(2));
        if (adv)
        {
            auto wc = b.removeFromRight(60); b.removeFromRight(2);
            if (waveSel) waveSel->setBounds(wc.reduced(2));
            auto sd = b.removeFromRight(60); b.removeFromRight(2);
            if (syncDivSel) syncDivSel->setBounds(sd.reduced(2));
            auto it = b.removeFromRight(52); b.removeFromRight(2);
            if (invFbTog) invFbTog->setBounds(it.reduced(1));
            auto bt = b.removeFromRight(52); b.removeFromRight(2);
            if (bpmTog) bpmTog->setBounds(bt.reduced(1));
        }

        // Knobs: all Fruity knobs always; Cross (idx 6, our addition) Advanced only.
        std::vector<VKnob*> vis;
        for (int i = 0; i < (int) knobs.size(); ++i)
            if (adv || i != 6)
                if (knobs[i]) vis.push_back(knobs[i].get());
        layoutKnobsH(b, vis);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// TransientShaperPanel
// Attack | Release | Sens | SplitHz | Balance | Drive | Gain
// +  AttackShape combo  +  ReleaseShape combo
// ─────────────────────────────────────────────────────────────────────────────
struct TransientShaperPanel : public EditorPanelBase
{
    std::vector<std::unique_ptr<VKnob>>  r1knobs, r2knobs;
    std::unique_ptr<ChickenHeadSelector> attackShapeSel, releaseShapeSel;
    std::unique_ptr<ChickenHeadSelector> osSel;            // C1
    std::unique_ptr<DualLabelToggle>     stereoDetectTog;  // C2

    std::vector<VKnob*> getExtraKnobs() override
    {
        std::vector<VKnob*> v;
        for (auto& k : r1knobs) if (k) v.push_back(k.get());
        for (auto& k : r2knobs) if (k) v.push_back(k.get());
        return v;
    }

    // Basic = the FL Transient Processor's controls (Attack / Release / Attack-
    // Shape / Release-Shape / Split / Wet (= FL Split Balance) / Gain / Drive).
    // Advanced = our additions (Balance / Sensitivity / FastRelease / SlowAttack
    // / Oversampling / Stereo-detect).
    bool hasAdvancedControls() const override { return true; }

    explicit TransientShaperPanel(TransientShaperDSP* dsp)
    {
        setLookAndFeel(&DynamicsLAF::get());
        setVolumeKnobVariant(true);  // black knob for cream Dynamics panel

        // Row 1: primary shaping controls (unchanged 7 knobs)
        buildKnobs(*this, r1knobs, {
            { "Attack",  -100.f, 100.f,   0.f, 1.f,  "Attack shaping (-100=reduce, 0=neutral, +100=enhance)" },
            { "Release", -100.f, 100.f,   0.f, 1.f,  "Release shaping (-100=shorten, +100=extend)" },
            { "Sens",      0.f,   1.f,   0.5f, 0.01f,"Detection sensitivity" },
            { "Split",     5.f, 1000.f, 260.f, 5.f,  "Band-split frequency (Hz)" },
            { "Balance", -100.f, 100.f,   0.f, 1.f,  "Band balance (-100=low, +100=high)" },
            { "Drive",     0.f,  10.f,   0.f,  0.1f, "Output drive (soft saturation; 4x oversampled)" },
            { "Gain",    -18.f,  18.f,   0.f,  0.5f, "Output gain (dB)" },
        });

        // Row 2: new controls (C3/C4 additions)
        buildKnobs(*this, r2knobs, {
            { "Wet",       0.f,   1.f,   1.0f, 0.01f, "Dry/Wet mix (C3). Default 1.0 = 100%-wet (v1 behavior); lower for parallel transient shaping" },
            { "FastRel",   1.f,  50.f,  10.0f, 0.1f,  "Fast envelope release time (ms). Used as the release time-constant of the peak follower. Previously hardcoded 10 ms; exposed for power-user tuning" },
            { "SlowAtt",   1.f,  50.f,  10.0f, 0.1f,  "Slow envelope attack time (ms). Used as the attack time-constant of the RMS follower. Previously hardcoded 10 ms; exposed for power-user tuning" },
        });

        // All knobs use the cream-panel modernAnalog variant for consistency.
        for (auto& k : r1knobs) k->slider.getProperties().set(DynamicsLAF::kKnobVariant, "modernAnalog");
        for (auto& k : r2knobs) k->slider.getProperties().set(DynamicsLAF::kKnobVariant, "modernAnalog");

        // Attack shape - 3-position chicken-head selector (Sh / Md / Sf)
        const std::vector<ChickenHeadSelector::Option> shapeOpts = {
            { "Sh", "Sharp",  "Fast, snappy envelope - preserves transient punch" },
            { "Md", "Medium", "Balanced attack/release curve (default)" },
            { "Sf", "Soft",   "Gentle, smoothed envelope - transparent, musical" },
        };
        attackShapeSel = std::make_unique<ChickenHeadSelector>();
        attackShapeSel->setOptions(shapeOpts);
        attackShapeSel->setBodyTooltip("Attack shape - fast envelope attack time-constant");
        attackShapeSel->setDefaultLabelColour(juce::Colours::black);
        // A9: sync from DSP state on construct (was hardcoded to index 1)
        attackShapeSel->setSelectedIndex(juce::jlimit(0, 2, dsp->mAttackShape), juce::dontSendNotification);
        attackShapeSel->onChange = [dsp](int idx){ dsp->setAttackShape(idx); };
        addAndMakeVisible(*attackShapeSel);

        releaseShapeSel = std::make_unique<ChickenHeadSelector>();
        releaseShapeSel->setOptions(shapeOpts);
        releaseShapeSel->setBodyTooltip("Release shape - slow envelope release time-constant");
        releaseShapeSel->setDefaultLabelColour(juce::Colours::black);
        // A9
        releaseShapeSel->setSelectedIndex(juce::jlimit(0, 2, dsp->mReleaseShape), juce::dontSendNotification);
        releaseShapeSel->onChange = [dsp](int idx){ dsp->setReleaseShape(idx); };
        addAndMakeVisible(*releaseShapeSel);

        // C1: OS factor chicken-head
        osSel = std::make_unique<ChickenHeadSelector>();
        osSel->setOptions({
            { "2x",  "2x OS",  "2x oversampling around the drive stage - lowest CPU" },
            { "4x",  "4x OS",  "4x oversampling (default) - balanced quality vs CPU" },
            { "8x",  "8x OS",  "8x oversampling - cleaner drive saturation, more CPU" },
            { "16x", "16x OS", "16x oversampling - maximum alias suppression" },
        });
        osSel->setBodyTooltip("Oversampling factor around the drive stage");
        osSel->setDefaultLabelColour(juce::Colours::black);
        osSel->setSelectedIndex(juce::jlimit(0, 3, dsp->getOsLog2() - 1), juce::dontSendNotification);
        osSel->onChange = [dsp](int idx){ dsp->setOsLog2(juce::jlimit(1, 4, idx + 1)); };
        addAndMakeVisible(*osSel);

        // C2: stereo envelope detection toggle
        stereoDetectTog = std::make_unique<DualLabelToggle>();
        stereoDetectTog->setupNamed(
            "Mono Det", "Mono detection - single envelope driven by avg(|L|, |R|); same gain applied to both channels (v1 behavior)",
            "Stereo Det","Stereo detection - per-channel envelopes, independent gain per channel (better for stereo sources with asymmetric transients)");
        stereoDetectTog->setLabelColour(juce::Colours::black);
        stereoDetectTog->btn().setToggleState(dsp->getStereoDetect(), juce::dontSendNotification);
        stereoDetectTog->btn().onClick = [dsp, this]{ dsp->setStereoDetect(stereoDetectTog->btn().getToggleState()); };
        addAndMakeVisible(*stereoDetectTog);

        // Row 1 bindings
        r1knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setAttack      ((float)r1knobs[0]->slider.getValue()); };
        r1knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setRelease     ((float)r1knobs[1]->slider.getValue()); };
        r1knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setSensitivity ((float)r1knobs[2]->slider.getValue()); };
        r1knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setSplitFreq   ((float)r1knobs[3]->slider.getValue()); };
        r1knobs[4]->slider.onValueChange = [dsp,this]{ dsp->setSplitBalance((float)r1knobs[4]->slider.getValue()); };
        r1knobs[5]->slider.onValueChange = [dsp,this]{ dsp->setDrive       ((float)r1knobs[5]->slider.getValue()); };
        r1knobs[6]->slider.onValueChange = [dsp,this]{ dsp->setGain        ((float)r1knobs[6]->slider.getValue()); };

        // Row 2 bindings (C3/C4)
        r2knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setWet       ((float)r2knobs[0]->slider.getValue()); };
        r2knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setFastRelMs ((float)r2knobs[1]->slider.getValue()); };
        r2knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setSlowAttMs ((float)r2knobs[2]->slider.getValue()); };

        // A9 slider sync from DSP state + clamp cleanup (sendNotificationSync fires the
        // onValueChange lambdas, which call the DSP setters with the clamped slider value).
        // Attack/Release: setter takes -100..100, DSP stores -1..1; scale by 100.
        r1knobs[0]->slider.setValue(dsp->mAttack * 100.0f,  juce::sendNotificationSync);
        r1knobs[1]->slider.setValue(dsp->mRelease * 100.0f, juce::sendNotificationSync);
        r1knobs[2]->slider.setValue(dsp->mSensitivity,      juce::sendNotificationSync);
        r1knobs[3]->slider.setValue(dsp->mSplitFreq,        juce::sendNotificationSync);
        r1knobs[4]->slider.setValue(dsp->mSplitBalance,     juce::sendNotificationSync);
        r1knobs[5]->slider.setValue(dsp->mDrive,            juce::sendNotificationSync);
        r1knobs[6]->slider.setValue(dsp->mOutGainDb,        juce::sendNotificationSync);
        r2knobs[0]->slider.setValue(dsp->mWet,              juce::sendNotificationSync);
        r2knobs[1]->slider.setValue(dsp->mFastRelMs,        juce::sendNotificationSync);
        r2knobs[2]->slider.setValue(dsp->mSlowAttMs,        juce::sendNotificationSync);
    }

    ~TransientShaperPanel() override
    {
        setLookAndFeel(nullptr);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(4, 4);

        // Strip meters first
        if (vuIn) vuIn->setBounds(b.removeFromLeft(120).reduced(1, 2));
        dbfsOut      ->setBounds(b.removeFromRight(32).reduced(1, 2));
        b.removeFromRight(2);
        outputVolKnob->setBounds(b.removeFromRight(kKnobSz).withSizeKeepingCentre(kKnobSz, kKnobSz));
        b.removeFromRight(4);

        // Advanced-only: Sens (r1[2]), Balance (r1[4]), FastRel (r2[1]),
        // SlowAtt (r2[2]) + Oversampling + Stereo-detect.  Basic = the FL
        // controls (Attack/Release/Split/Drive/Gain/Wet + the 2 Shape selectors).
        const bool adv = ! mBasicMode;
        if (r1knobs.size() > 4 && r1knobs[2]) r1knobs[2]->setVisible (adv);   // Sens
        if (r1knobs.size() > 4 && r1knobs[4]) r1knobs[4]->setVisible (adv);   // Balance
        if (r2knobs.size() > 2 && r2knobs[1]) r2knobs[1]->setVisible (adv);   // FastRel
        if (r2knobs.size() > 2 && r2knobs[2]) r2knobs[2]->setVisible (adv);   // SlowAtt
        if (osSel) osSel->setVisible (adv);
        if (stereoDetectTog) stereoDetectTog->setVisible (adv);

        if (adv)
        {
            auto r1 = b.removeFromTop(b.getHeight() / 2);
            auto r2 = b;

            // T20 grouping review (Jeff, 2026-08-05: "do what makes sense").
            // Advanced was split 7/3 by which vector a knob happened to live in,
            // which put FastRel and SlowAtt -- the release and attack time
            // constants of the two envelope followers Attack and Release drive --
            // on the far row from the controls they tune, and left row 2 with
            // three knobs sprawled across ~683px.  Split 5/5 by FUNCTION instead:
            // row 1 is detection + shaping, row 2 is the band split + output.
            // Stereo/Mono Det follows the detector knobs for the same reason.
            //
            // The knob VECTORS are untouched -- Basic below indexes into them,
            // and re-sorting them to match the rows would break those indices for
            // no gain.
            std::vector<VKnob*> row1 {
                r1knobs[0].get(),   // Attack
                r1knobs[1].get(),   // Release
                r2knobs[1].get(),   // FastRel  (peak follower release)
                r2knobs[2].get(),   // SlowAtt  (RMS follower attack)
                r1knobs[2].get(),   // Sens
            };
            std::vector<VKnob*> row2 {
                r1knobs[3].get(),   // Split
                r1knobs[4].get(),   // Balance
                r1knobs[5].get(),   // Drive
                r1knobs[6].get(),   // Gain
                r2knobs[0].get(),   // Wet
            };

            // Row 1 right: AttackShape + ReleaseShape chicken-heads + Stereo Det.
            int r1sz = juce::jmin(r1.getHeight(), kKnobSz);
            auto sdSlot = r1.removeFromRight(74); r1.removeFromRight(4);
            if (stereoDetectTog) stereoDetectTog->setBounds(sdSlot.reduced(1));
            auto rSlot = r1.removeFromRight(r1sz); r1.removeFromRight(4);
            if (releaseShapeSel) releaseShapeSel->setBounds(rSlot.reduced(2));
            auto aSlot = r1.removeFromRight(r1sz); r1.removeFromRight(4);
            if (attackShapeSel)  attackShapeSel->setBounds(aSlot.reduced(2));
            layoutKnobsH(r1, row1);

            // Row 2 right: OS.
            auto osSlot = r2.removeFromRight(60); r2.removeFromRight(4);
            if (osSel) osSel->setBounds(osSlot.reduced(2));
            layoutKnobsH(r2, row2);
        }
        else
        {
            // Basic single row: Attack/Release/Split/Drive/Gain/Wet + Shape selectors.
            int shSz = juce::jmin(b.getHeight(), kKnobSz + 10);
            auto rSlot = b.removeFromRight(shSz); b.removeFromRight(4);
            if (releaseShapeSel) releaseShapeSel->setBounds(rSlot.withSizeKeepingCentre(shSz, juce::jmin(b.getHeight(), 58)));
            auto aSlot = b.removeFromRight(shSz); b.removeFromRight(4);
            if (attackShapeSel)  attackShapeSel->setBounds(aSlot.withSizeKeepingCentre(shSz, juce::jmin(b.getHeight(), 58)));

            std::vector<VKnob*> basic;
            if (r1knobs[0]) basic.push_back(r1knobs[0].get());   // Attack
            if (r1knobs[1]) basic.push_back(r1knobs[1].get());   // Release
            if (r1knobs[3]) basic.push_back(r1knobs[3].get());   // Split
            if (r1knobs[5]) basic.push_back(r1knobs[5].get());   // Drive
            if (r1knobs[6]) basic.push_back(r1knobs[6].get());   // Gain
            if (r2knobs[0]) basic.push_back(r2knobs[0].get());   // Wet
            layoutKnobsH(b, basic);
        }
    }

    void paint(juce::Graphics& g) override
    {
        DynamicsLAF::paintLA2APanel(g, getLocalBounds());
        // Shape labels are drawn by ChickenHeadSelector itself - no custom paint needed.
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// TapeSatPanel - H-10 (2026-05-02): Saturation umbrella's Tape sub-panel.
// Binds SaturationDSP's setTape* setters; the standalone TapeDSP class it
// replaced is no longer reachable from any panel.
// SlotComponent's Mode dropdown picks Tube / Console / Tape; this panel
// is constructed when Tape is the active type.
// ─────────────────────────────────────────────────────────────────────────────
struct TapeSatPanel : public EditorPanelBase
{
    std::vector<std::unique_ptr<VKnob>>  r1knobs, r2knobs;
    std::unique_ptr<ChickenHeadSelector> tapeSpeedSel;   // C4
    std::unique_ptr<ChickenHeadSelector> osSel;          // C2
    std::unique_ptr<VKnob>               lowPassKnob;     // cassette HF rolloff (Basic)
    std::unique_ptr<DualLabelToggle>     irTog;           // cassette IR on/off (Basic)
    std::unique_ptr<ChickenHeadSelector> cassetteSel;     // 10-way cassette picker (Advanced)

    std::vector<VKnob*> getExtraKnobs() override
    {
        std::vector<VKnob*> v;
        for (auto& k : r1knobs) if (k) v.push_back(k.get());
        for (auto& k : r2knobs) if (k) v.push_back(k.get());
        if (lowPassKnob) v.push_back(lowPassKnob.get());
        return v;
    }

    // Basic = the reference cassette's face controls (Drive / Low-Pass / Noise /
    // Wow / Flutter).  Advanced reveals our deeper tape engine (Vibe/Hyst/Bias,
    // wow+flutter rates, emphasis shelves, tape speed, oversampling).
    bool hasAdvancedControls() const override { return true; }

    explicit TapeSatPanel(SaturationDSP* dsp)
    {
        setLookAndFeel(&HarmonicLAF::get());

        // Row 1: shaper character + drive + hiss
        buildKnobs(*this, r1knobs, {
            { "Vibe",    0.f,  1.f,   0.5f, 0.01f, "Shaper asymmetry driver - k = 0.3 * Vibe in the sigmoid" },
            { "Hyst",    0.f,  2.f,   1.0f, 0.01f, "Hysteresis amount (C1) - 0 = no tape memory, 1 = default, 2 = strong memory" },
            { "Bias",    0.f, 10.f,   5.0f, 0.1f,  "Tape bias (C5) - 5 = neutral (zero DC offset); extremes inject even harmonics" },
            { "Drive", -24.f, 24.f,   0.0f, 0.1f,  "Drive into the tape saturator (dB; 0 = unity, high noon)" },
            { "Hiss",  -80.f,  0.f, -60.0f, 0.5f,  "Tape hiss level (dB)" },
        });

        // Row 2: wow/flutter + pre/de-emphasis shelves
        buildKnobs(*this, r2knobs, {
            { "WowHz",   0.1f,  5.f,   0.5f, 0.05f, "Wow LFO rate (Hz)" },
            { "WowDp",   0.f,   1.f,   0.0f, 0.01f, "Wow depth (slow motor drift) - default 0 so tape starts clean; raise to taste" },
            { "FlutHz",  1.f,  15.f,   5.0f, 0.1f,  "Flutter LFO rate (Hz) - capstan friction shimmer" },
            { "FlutDp",  0.f,   1.f,   0.0f, 0.01f, "Flutter depth (fast modulation) - default 0 so tape starts clean; raise to taste" },
            { "PreShf", -12.f, 12.f,   0.0f, 0.1f,  "Pre-emphasis shelf @ 5 kHz (dB). Default 0 (flat); boost = more high-freq harmonics into shaper" },
            { "DeShf",  -12.f, 12.f,   0.0f, 0.1f,  "De-emphasis shelf @ 4 kHz (dB). Default 0 (flat); cut = tame high harmonics after shaper" },
        });

        // Tape speed chicken-head
        tapeSpeedSel = std::make_unique<ChickenHeadSelector>();
        tapeSpeedSel->setOptions({
            { "7.5",  "7.5 ips", "7.5 ips cassette speed - slowest, squishiest, strongest hysteresis memory" },
            { "15",   "15 ips",  "15 ips pro reel-to-reel - balanced character (default)" },
            { "30",   "30 ips",  "30 ips mastering speed - fastest, cleanest, minimal hysteresis" },
        });
        tapeSpeedSel->setBodyTooltip("Tape speed - affects hysteresis memory time constant");
        {
            const float ts = dsp->getTapeSpeed();
            const int idx = (ts < 0.25f) ? 0 : (ts < 0.75f) ? 1 : 2;
            tapeSpeedSel->setSelectedIndex(idx, juce::dontSendNotification);
        }
        tapeSpeedSel->onChange = [dsp](int idx){
            static constexpr float kTapeSpeedValues[3] = { 0.0f, 0.5f, 1.0f };
            dsp->setTapeSpeed(kTapeSpeedValues[juce::jlimit(0, 2, idx)]);
        };
        addAndMakeVisible(*tapeSpeedSel);

        // Oversampling factor chicken-head -- shared with the rest of Saturation
        osSel = std::make_unique<ChickenHeadSelector>();
        osSel->setOptions({
            { "2x",  "2x OS",  "2x oversampling - lowest CPU, some alias from shaper at high drive" },
            { "4x",  "4x OS",  "4x oversampling (default) - balanced quality vs CPU" },
            { "8x",  "8x OS",  "8x oversampling - cleaner high-drive character, more CPU" },
            { "16x", "16x OS", "16x oversampling - maximum alias suppression" },
        });
        osSel->setBodyTooltip("Oversampling factor (shared by Tube/Console/Tape) around shaper + hysteresis");
        osSel->setSelectedIndex(juce::jlimit(0, 3, dsp->getTapeOsLog2() - 1),
                                juce::dontSendNotification);
        osSel->onChange = [dsp](int idx){ dsp->setOversamplingFactor(juce::jlimit(1, 4, idx + 1)); };
        addAndMakeVisible(*osSel);

        // Cassette low-pass (HF rolloff) -- Basic-tier tone control.
        lowPassKnob = std::make_unique<VKnob>("LoPass", 18000.0f,
            "Cassette tone rolloff - lowers the high end (5-22 kHz; full right = off)");
        lowPassKnob->slider.setRange(5000.0, 22000.0, 1.0);
        lowPassKnob->slider.setValue(dsp->mTapeLpHz, juce::dontSendNotification);
        lowPassKnob->slider.onValueChange = [dsp,this]{ if (dsp) dsp->setTapeLpHz((float)lowPassKnob->slider.getValue()); };
        addAndMakeVisible(*lowPassKnob);

        // Cassette IR on/off (Basic-tier; matches the reference's single IR toggle).
        irTog = std::make_unique<DualLabelToggle>();
        irTog->setupOnOff("IR", "Cassette impulse response - tone coloration of a real cassette");
        if (dsp) irTog->btn().setToggleState(dsp->mTapeIrOn, juce::dontSendNotification);
        irTog->btn().onClick = [dsp, this]{ if (dsp) dsp->setTapeIrOn(irTog->btn().getToggleState()); };
        addAndMakeVisible(*irTog);

        // 10-way cassette picker (Advanced) -- selects the IR + its paired hiss bed.
        cassetteSel = std::make_unique<ChickenHeadSelector>();
        {
            std::vector<ChickenHeadSelector::Option> casOpts;
            for (int i = 1; i <= 10; ++i)
                casOpts.push_back({ juce::String(i), "Cassette " + juce::String(i),
                                    "Cassette " + juce::String(i) + " - impulse response + paired hiss" });
            cassetteSel->setOptions(casOpts);
        }
        cassetteSel->setBodyTooltip("Cassette type (IR + paired hiss)");
        if (dsp) cassetteSel->setSelectedIndex(juce::jlimit(0, 9, dsp->mTapeCassetteIdx), juce::dontSendNotification);
        cassetteSel->onChange = [dsp](int idx){ if (dsp) dsp->setTapeCassette(idx); };
        addAndMakeVisible(*cassetteSel);

        // Row 1 knob bindings -- call setTape* on SaturationDSP
        r1knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setTapeVibe       ((float)r1knobs[0]->slider.getValue()); };
        r1knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setTapeHystAmount ((float)r1knobs[1]->slider.getValue()); };
        r1knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setTapeBias       ((float)r1knobs[2]->slider.getValue()); };
        r1knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setTapeInputGain  (juce::Decibels::decibelsToGain ((float)r1knobs[3]->slider.getValue())); };
        r1knobs[4]->slider.onValueChange = [dsp,this]{ dsp->setTapeHiss       (juce::Decibels::decibelsToGain ((float)r1knobs[4]->slider.getValue())); };

        // Row 2 knob bindings
        r2knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setTapeWowRate      ((float)r2knobs[0]->slider.getValue()); };
        r2knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setTapeWowDepth     ((float)r2knobs[1]->slider.getValue()); };
        r2knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setTapeFlutterRate  ((float)r2knobs[2]->slider.getValue()); };
        r2knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setTapeFlutterDepth ((float)r2knobs[3]->slider.getValue()); };
        r2knobs[4]->slider.onValueChange = [dsp,this]{ dsp->setTapePreShelfDb   ((float)r2knobs[4]->slider.getValue()); };
        r2knobs[5]->slider.onValueChange = [dsp,this]{ dsp->setTapeDeShelfDb    ((float)r2knobs[5]->slider.getValue()); };

        // Sync sliders to DSP state.
        r1knobs[0]->slider.setValue (dsp->mTapeVibe,         juce::sendNotificationSync);
        r1knobs[1]->slider.setValue (dsp->mTapeHystAmount,   juce::sendNotificationSync);
        r1knobs[2]->slider.setValue (dsp->mTapeBias,         juce::sendNotificationSync);
        r1knobs[3]->slider.setValue (juce::Decibels::gainToDecibels (dsp->mTapeInputGain, -24.0f), juce::sendNotificationSync);
        r1knobs[4]->slider.setValue (juce::Decibels::gainToDecibels (dsp->mTapeHiss, -80.0f),      juce::sendNotificationSync);
        r2knobs[0]->slider.setValue (dsp->mTapeWowRate,      juce::sendNotificationSync);
        r2knobs[1]->slider.setValue (dsp->mTapeWowDepth,     juce::sendNotificationSync);
        r2knobs[2]->slider.setValue (dsp->mTapeFlutterRate,  juce::sendNotificationSync);
        r2knobs[3]->slider.setValue (dsp->mTapeFlutterDepth, juce::sendNotificationSync);
        r2knobs[4]->slider.setValue (dsp->mTapePreShelfDb,   juce::sendNotificationSync);
        r2knobs[5]->slider.setValue (dsp->mTapeDeShelfDb,    juce::sendNotificationSync);
    }

    ~TapeSatPanel() override
    {
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        HarmonicLAF::paintHammeritePanel(g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(4, 2);

        // Meter strips
        if (vuIn) vuIn->setBounds(b.removeFromLeft(120).reduced(1, 2));
        dbfsOut      ->setBounds(b.removeFromRight(32).reduced(1, 2));
        b.removeFromRight(2);
        outputVolKnob->setBounds(b.removeFromRight(kKnobSz).withSizeKeepingCentre(kKnobSz, kKnobSz));
        b.removeFromRight(4);

        // Advanced-only: the deeper tape engine (Vibe/Hyst/Bias + wow/flutter
        // rates + emphasis shelves + tape speed + oversampling) + the 10-way
        // cassette picker.  Basic = Drive / Low-Pass / Noise / Wow / Flutter +
        // the cassette IR on/off (IR toggle shows in both modes).
        const bool adv = ! mBasicMode;
        if (r1knobs[0]) r1knobs[0]->setVisible(adv);   // Vibe
        if (r1knobs[1]) r1knobs[1]->setVisible(adv);   // Hyst
        if (r1knobs[2]) r1knobs[2]->setVisible(adv);   // Bias
        if (r2knobs[0]) r2knobs[0]->setVisible(adv);   // Wow rate
        if (r2knobs[2]) r2knobs[2]->setVisible(adv);   // Flutter rate
        if (r2knobs[4]) r2knobs[4]->setVisible(adv);   // Pre-emphasis
        if (r2knobs[5]) r2knobs[5]->setVisible(adv);   // De-emphasis
        if (tapeSpeedSel) tapeSpeedSel->setVisible(adv);
        if (osSel)        osSel->setVisible(adv);
        // irTog + cassetteSel stay visible in both modes -- the cassette is
        // pickable wherever you can turn the IR on.

        if (adv)
        {
            auto r1 = b.removeFromTop(b.getHeight() / 2);
            auto r2 = b;

            // Row 1 right: OS | Tape Speed | Cassette picker | IR on/off
            auto osSlot = r1.removeFromRight(54); r1.removeFromRight(2);
            if (osSel) osSel->setBounds(osSlot.reduced(2));
            auto tsSlot = r1.removeFromRight(56); r1.removeFromRight(2);
            if (tapeSpeedSel) tapeSpeedSel->setBounds(tsSlot.reduced(2));
            auto casSlot = r1.removeFromRight(58); r1.removeFromRight(2);
            if (cassetteSel) cassetteSel->setBounds(casSlot.reduced(2));
            auto irSlot = r1.removeFromRight(44); r1.removeFromRight(2);
            if (irTog) irTog->setBounds(irSlot.reduced(1));

            // Row 1: Basic-tier Drive / Hiss / LoPass grouped at the front (so the
            // Basic set stays together in Advanced), then Vibe / Hyst.
            std::vector<VKnob*> row1 = {
                r1knobs[3].get(),    // Drive
                r1knobs[4].get(),    // Hiss
                lowPassKnob.get(),   // LoPass
                r1knobs[0].get(),    // Vibe
                r1knobs[1].get(),    // Hyst
            };
            layoutKnobsH(r1, row1);

            // Row 2: wow/flutter + emphasis shelves + Bias (swapped in where LoPass sat).
            std::vector<VKnob*> row2 = {
                r2knobs[0].get(), r2knobs[1].get(), r2knobs[2].get(),
                r2knobs[3].get(), r2knobs[4].get(), r2knobs[5].get(),
                r1knobs[2].get(),    // Bias
            };
            layoutKnobsH(r2, row2);
        }
        else
        {
            // Basic right cluster: Cassette picker + IR on/off (cassette is
            // pickable here too).  The toggle gets full label height so its
            // OFF/ON tags show.  Then the 5 knobs fill the rest.
            auto irSlot = b.removeFromRight(48); b.removeFromRight(3);
            if (irTog) irTog->setBounds(irSlot.withSizeKeepingCentre(48, juce::jmin(irSlot.getHeight(), 72)));
            auto casSlot = b.removeFromRight(60); b.removeFromRight(4);
            if (cassetteSel) cassetteSel->setBounds(casSlot.withSizeKeepingCentre(58, juce::jmin(casSlot.getHeight(), 58)));

            std::vector<VKnob*> basic;
            if (r1knobs[3])  basic.push_back(r1knobs[3].get());   // Drive
            if (lowPassKnob) basic.push_back(lowPassKnob.get());  // Low-Pass
            if (r1knobs[4])  basic.push_back(r1knobs[4].get());   // Noise (Hiss)
            if (r2knobs[1])  basic.push_back(r2knobs[1].get());   // Wow depth
            if (r2knobs[3])  basic.push_back(r2knobs[3].get());   // Flutter depth
            layoutKnobsH(b, basic);
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// ── LoudnessMeter (BLU-110) ──────────────────────────────────────────────────
// LUFS beside dBFS with a target line across the loudness bar, so "how loud is
// this" and "how close to clipping is this" are readable in one glance instead of
// from two unrelated indicators.
//
// Palette follows the governing Limiter UI spec (`Files For Claude/DSP Review/
// Limiter.txt` §2): electric cyan as the primary accent, safety orange as the
// warning, monospaced digital readouts for the numbers.
//
// Deliberately NOT part of the three-zone skeuomorphic rewrite that spec also
// describes -- `_APPROVED_CHANGES.md` §5 files that as a separate UI task, and
// the layout batch running after this one owns the app's appearance under the
// windowed shell.  This is the BLU-110 deliverable only.
struct LoudnessMeter : public juce::Component,
                       public juce::SettableTooltipClient,
                       private juce::Timer
{
    LoudnessMeter() { startTimerHz (15); }
    ~LoudnessMeter() override { stopTimer(); }

    // Audio-thread values are already atomics on the DSP; the panel's timer
    // forwards them here, so these are plain fields written on the message thread.
    void setValues (float lufsShortTerm, float dbfsPeak, float targetLufs,
                    bool targetActive)
    {
        mLufs         = lufsShortTerm;
        mDbfs         = dbfsPeak;
        mTargetLufs   = targetLufs;
        mTargetActive = targetActive;
    }

    void paint (juce::Graphics& g) override
    {
        // Limiter.txt §2 palette: electric cyan accent, safety orange warning.
        const juce::Colour kCyan   (0xff00fff2);
        const juce::Colour kOrange (0xffff9100);

        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour (0xff141618));
        g.fillRoundedRectangle (b, 2.0f);
        g.setColour (juce::Colour (0xff2a2f33));
        g.drawRoundedRectangle (b, 2.0f, 1.0f);

        auto inner = b.reduced (3.0f);
        auto labels = inner.removeFromBottom (10.0f);
        inner.removeFromBottom (2.0f);

        const float colW = (inner.getWidth() - 3.0f) * 0.5f;
        auto lufsCol = inner.removeFromLeft (colW);
        inner.removeFromLeft (3.0f);
        auto dbfsCol = inner.removeFromLeft (colW);

        // Shared scale.  -36..0 covers every target in the spec table (-14 through
        // ATSC's -24) with room to read a quiet passage below EBU's -23.
        auto yFor = [&] (const juce::Rectangle<float>& col, float db)
        {
            const float t = juce::jlimit (0.0f, 1.0f, (db - kMinDb) / (kMaxDb - kMinDb));
            return col.getBottom() - t * col.getHeight();
        };

        auto drawBar = [&] (juce::Rectangle<float> col, float db, juce::Colour fill)
        {
            g.setColour (juce::Colour (0xff0b0d0e));
            g.fillRect (col);
            if (db > kMinDb)
            {
                const float top = yFor (col, db);
                g.setColour (fill);
                g.fillRect (col.withTop (top));
            }
        };

        const bool overTarget = mTargetActive && (mLufs > mTargetLufs);
        drawBar (lufsCol, mLufs, overTarget ? kOrange : kCyan);
        // dBFS turns orange once it is inside the last dB, which is the margin
        // every spec in the table asks for.
        drawBar (dbfsCol, mDbfs, (mDbfs > -1.0f) ? kOrange : juce::Colour (0xffb8c0c6));

        if (mTargetActive)
        {
            const float ty = yFor (lufsCol, mTargetLufs);
            g.setColour (kCyan.withAlpha (0.85f));
            // Dashed so it reads as a reference line rather than as signal.
            const float dash[] = { 3.0f, 2.0f };
            juce::Path line, dashed;
            line.startNewSubPath (lufsCol.getX(), ty);
            line.lineTo (lufsCol.getRight(), ty);
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, line, dash, 2);
            g.fillPath (dashed);
        }

        g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 8.0f,
                               juce::Font::plain));
        g.setColour (juce::Colour (0xff8f9aa1));
        auto fmt = [] (float db) -> juce::String
        {
            return (db <= -119.0f) ? juce::String ("--")
                                   : juce::String (db, 1);
        };
        g.drawText (fmt (mLufs), labels.removeFromLeft (colW),
                    juce::Justification::centred, false);
        labels.removeFromLeft (3.0f);
        g.drawText (fmt (mDbfs), labels.removeFromLeft (colW),
                    juce::Justification::centred, false);
    }

private:
    void timerCallback() override { repaint(); }

    float mLufs         { -120.0f };
    float mDbfs         {  -96.0f };
    float mTargetLufs   {  -14.0f };
    bool  mTargetActive { false };

    static constexpr float kMinDb = -36.0f;
    static constexpr float kMaxDb =   0.0f;
};

// LimiterPanel (basic, functional - polished Limiter.txt UI is a separate task)
// Row 1: InGain | Ceil | SatTh | SatCv | SCHPF
// Row 2: Atk   | Rel  | Ahead | RelCv  + Sustain
// Row 3 (TS7, Advanced): Character selector | LUFS target | TP target
// Also: GR meter + LUFS/dBFS meter (left of knobs, right of input VU)
// ─────────────────────────────────────────────────────────────────────────────
struct LimiterPanel : public EditorPanelBase,
                      public juce::Timer
{
    std::vector<std::unique_ptr<VKnob>> r1knobs, r2knobs;
    std::unique_ptr<DualLabelToggle>    autoRelTog;
    std::unique_ptr<DualLabelToggle>    autoMuTog;    // C4
    std::unique_ptr<DualLabelToggle>    linkTog;      // C5
    std::unique_ptr<GRMeter>            grMeter;
    std::unique_ptr<VKnob>              sustainKnob;  // Fruity SUSTAIN (Basic)
    // ── TS7 maximizer suite ───────────────────────────────────────────────────
    std::unique_ptr<ChickenHeadSelector> charSel;      // CL-243
    std::unique_ptr<VKnob>               lufsTgtKnob;  // CL-244
    std::unique_ptr<DualLabelToggle>     lufsTgtTog;   // CL-244
    std::unique_ptr<VKnob>               tpTgtKnob;    // BLU-108
    std::unique_ptr<DualLabelToggle>     autoCeilTog;  // BLU-108
    std::unique_ptr<LoudnessMeter>       loudMeter;    // BLU-110
    LimiterDSP*                         mDsp { nullptr };

    // Chain context only.  The vocal chain re-applies its limiter from APVTS every
    // block, so a selector that wrote the DSP alone would be overwritten on the
    // next callback -- it has to write the PARAMETER there.  Null in a rack slot,
    // where the DSP is authoritative.
    juce::AudioProcessorValueTreeState* mChainApvts { nullptr };
    juce::String                        mChainPrefix;

    // Basic = the real Fruity Limiter controls (GAIN / Ceiling / SAT-thresh /
    // Attack / Release / Sustain + GR meter) -- the exact reference replica, so
    // every TS7 addition below is Advanced-tier by that rule, not by preference.
    // Advanced = our additions (SatCurve / Ahead / RelCurve / SC-HPF +
    // Auto-release / Auto-makeup / Link + the maximizer suite).
    bool hasAdvancedControls() const override { return true; }

    void disableGrMeter() override
    {
        if (grMeter) { removeChildComponent (grMeter.get()); grMeter.reset(); }
    }

    std::vector<VKnob*> getExtraKnobs() override
    {
        std::vector<VKnob*> v;
        for (auto& k : r1knobs) if (k) v.push_back(k.get());
        for (auto& k : r2knobs) if (k) v.push_back(k.get());
        if (sustainKnob) v.push_back(sustainKnob.get());
        if (lufsTgtKnob) v.push_back(lufsTgtKnob.get());
        if (tpTgtKnob)   v.push_back(tpTgtKnob.get());
        return v;
    }

    explicit LimiterPanel (LimiterDSP* dsp)
        : mDsp (dsp)
    {
        setLookAndFeel (&DynamicsLAF::get());
        setVolumeKnobVariant (true);   // black volume knob on cream Dynamics panel

        buildKnobs (*this, r1knobs, {
            { "InGain", -12.f,   24.f,   0.f,  0.1f,  "Input gain (dB)" },
            { "Ceil",   -24.f,   12.f,  -0.3f, 0.1f,  "Output ceiling (dB; +12 = headroom / no limiting)" },
            { "SatTh",    0.f,    1.f,   1.0f, 0.01f, "Soft-sat threshold (1 = off)" },
            { "SatCv",    0.f,    1.f,   0.5f, 0.01f, "Soft-sat knee curve" },
            { "SCHPF",   20.f, 2000.f,  20.f,  1.f,   "Sidechain HPF cutoff (Hz) - 20 = off. Raise to stop bass from pumping the limiter" },
        });

        buildKnobs (*this, r2knobs, {
            { "Atk",    0.1f,   20.f,   1.0f, 0.1f,  "Attack (ms)" },
            { "Rel",   10.f, 1000.f,  100.f,  1.f,   "Release (ms)" },
            { "Ahead",  0.f,   10.f,    2.0f, 0.1f,  "Look-ahead (ms)" },
            { "RelCv",  0.f,    1.f,    0.5f, 0.01f, "Release curve (0=linear, 1=exp)" },
        });

        // All knobs use the cream-panel modernAnalog variant for consistency
        for (auto& k : r1knobs) k->slider.getProperties().set (DynamicsLAF::kKnobVariant, "modernAnalog");
        for (auto& k : r2knobs) k->slider.getProperties().set (DynamicsLAF::kKnobVariant, "modernAnalog");

        autoRelTog = std::make_unique<DualLabelToggle>();
        autoRelTog->setupOnOff("Auto", "Auto release: 2-stage adaptive (fast/slow blend based on GR amount)");
        autoRelTog->btn().setToggleState(dsp->getAutoRelease(), juce::dontSendNotification);
        autoRelTog->btn().onClick = [dsp, this]{ dsp->setAutoRelease (autoRelTog->btn().getToggleState()); };
        addAndMakeVisible (*autoRelTog);

        // C4: Auto-makeup gain - post-limit boost by -ceilingDb for maximizer workflow.
        autoMuTog = std::make_unique<DualLabelToggle>();
        autoMuTog->setupOnOff("Auto MU", "Auto makeup gain: adds -ceilingDb of post-limit boost so lowering the ceiling doesn't quiet the signal");
        autoMuTog->btn().setToggleState(dsp->getAutoMakeup(), juce::dontSendNotification);
        autoMuTog->btn().onClick = [dsp, this]{ dsp->setAutoMakeup (autoMuTog->btn().getToggleState()); };
        addAndMakeVisible (*autoMuTog);

        // C5: Stereo-link detector. Default = linked (single envelope from max(|L|,|R|)).
        linkTog = std::make_unique<DualLabelToggle>();
        linkTog->setupOnOff("Link", "Stereo link: single envelope from max(|L|,|R|) drives both channels (default). Off = per-channel envelopes (dual-mono limiting)");
        linkTog->btn().setToggleState(dsp->getStereoLink(), juce::dontSendNotification);
        linkTog->btn().onClick = [dsp, this]{ dsp->setStereoLink (linkTog->btn().getToggleState()); };
        addAndMakeVisible (*linkTog);

        r1knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setInputGainDb   ((float) r1knobs[0]->slider.getValue()); };
        r1knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setCeilingDb     ((float) r1knobs[1]->slider.getValue()); };
        r1knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setSatThresh     ((float) r1knobs[2]->slider.getValue()); };
        r1knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setSatCurve      ((float) r1knobs[3]->slider.getValue()); };
        r1knobs[4]->slider.onValueChange = [dsp,this]{ dsp->setSidechainHPF  ((float) r1knobs[4]->slider.getValue()); };

        r2knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setAttackMs      ((float) r2knobs[0]->slider.getValue()); };
        r2knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setReleaseMs     ((float) r2knobs[1]->slider.getValue()); };
        r2knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setAheadMs       ((float) r2knobs[2]->slider.getValue()); };
        r2knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setReleaseCurve  ((float) r2knobs[3]->slider.getValue()); };

        // A9 slider sync from DSP state + clamp cleanup (LimiterDSP already exposes public getters).
        r1knobs[0]->slider.setValue(dsp->getInputGainDb(),  juce::sendNotificationSync);
        r1knobs[1]->slider.setValue(dsp->getCeilingDb(),    juce::sendNotificationSync);
        r1knobs[2]->slider.setValue(dsp->getSatThresh(),    juce::sendNotificationSync);
        r1knobs[3]->slider.setValue(dsp->getSatCurve(),     juce::sendNotificationSync);
        r1knobs[4]->slider.setValue(dsp->getSidechainHPF(), juce::sendNotificationSync);
        r2knobs[0]->slider.setValue(dsp->getAttackMs(),     juce::sendNotificationSync);
        r2knobs[1]->slider.setValue(dsp->getReleaseMs(),    juce::sendNotificationSync);
        r2knobs[2]->slider.setValue(dsp->getAheadMs(),      juce::sendNotificationSync);
        r2knobs[3]->slider.setValue(dsp->getReleaseCurve(), juce::sendNotificationSync);

        // GR meter (SSL-style - matches CompressorPanel)
        grMeter = std::make_unique<GRMeter>();
        addAndMakeVisible (*grMeter);

        // Fruity SUSTAIN: RMS-window hold (0-1000 ms; 0 = off).  Basic-tier.
        sustainKnob = std::make_unique<VKnob>("Sustain", 0.0f,
            "Sustain (ms) - RMS window that holds the limiter's gain reduction so it releases over the window instead of early. 0 = off");
        sustainKnob->slider.setRange (0.0, 1000.0, 1.0);
        sustainKnob->slider.setValue (dsp->getSustainMs(), juce::dontSendNotification);
        sustainKnob->slider.getProperties().set (DynamicsLAF::kKnobVariant, "modernAnalog");
        sustainKnob->slider.onValueChange = [dsp,this]{ if (dsp) dsp->setSustainMs ((float) sustainKnob->slider.getValue()); };
        addAndMakeVisible (*sustainKnob);

        // ── CL-243: character selector ────────────────────────────────────────
        // A chickenhead, not a Mode-menu entry: a character does not change WHICH
        // parameters exist, so making it a variant would have forced eight
        // EffectParamMap tables for one identical parameter set.  Same reasoning
        // (and the same control) as the Delay panel's model selector, which is
        // likewise display-side and unautomatable.
        charSel = std::make_unique<ChickenHeadSelector>();
        {
            // Two-letter bezel marks; the full name and its description ride the
            // per-option tooltip, which is where an eight-way selector has room to
            // explain itself.
            static const char* kMarks[] = { "Cl", "Sm", "Ti", "Pu", "Gl", "Lo", "Wa", "In" };
            static const char* kBlurbs[] =
            {
                "Most transparent. Longest, smoothest recovery - the default",
                "Longer and gentler still; recovery leans exponential",
                "Fast and controlled; shorter release for a firmer low end",
                "Transient-forward; hands over to the slow envelope early",
                "Bus-style: slow and programme-dependent",
                "Maximum density; release is short and saturation is engaged",
                "Saturation-forward colour with a relaxed release",
                "Near-zero look-ahead clip guard; works with Ahead at 0",
            };
            std::vector<ChickenHeadSelector::Option> opts;
            for (int i = 0; i < LimiterDSP::numCharacters(); ++i)
                opts.push_back ({ kMarks[i], LimiterDSP::characterName (i), kBlurbs[i] });
            charSel->setOptions (opts);
        }
        charSel->setBodyTooltip ("Character: release voicing and colour. Clean is the "
                                 "most transparent; Loud and Warm add saturation; "
                                 "Instant is designed to work with Ahead at 0");
        charSel->setDefaultLabelColour (juce::Colours::black);   // cream Dynamics plate
        charSel->setSelectedIndex (dsp->getCharacterIndex(), juce::dontSendNotification);
        charSel->onChange = [this] (int idx)
        {
            if (mDsp) mDsp->setCharacterIndex (idx);
            // Chain context: the parameter is the authority (see mChainApvts).
            if (mChainApvts != nullptr)
            {
                beginParamUndoGesture (*mChainApvts, mChainPrefix + "limiter_character"); // Task 6 (12-iv)
                if (auto* p = mChainApvts->getParameter (mChainPrefix + "limiter_character"))
                    p->setValueNotifyingHost (
                        p->getNormalisableRange().convertTo0to1 ((float) idx));
            }
        };
        addAndMakeVisible (*charSel);

        // ── CL-244: loudness target ───────────────────────────────────────────
        lufsTgtKnob = std::make_unique<VKnob>("LUFS", -14.0f,
            "Loudness target (LUFS). With Target on, the limiter measures its own "
            "output loudness and trims input gain toward this over several seconds");
        lufsTgtKnob->slider.setRange (-30.0, 0.0, 0.1);
        lufsTgtKnob->slider.setValue (dsp->getLoudnessTargetLufs(), juce::dontSendNotification);
        lufsTgtKnob->slider.getProperties().set (DynamicsLAF::kKnobVariant, "modernAnalog");
        lufsTgtKnob->slider.onValueChange = [dsp,this]
            { if (dsp) dsp->setLoudnessTargetLufs ((float) lufsTgtKnob->slider.getValue()); };
        addAndMakeVisible (*lufsTgtKnob);

        lufsTgtTog = std::make_unique<DualLabelToggle>();
        lufsTgtTog->setupOnOff("Target", "Loudness target mode: slowly trims input gain "
                                          "so the output settles at the LUFS target");
        lufsTgtTog->btn().setToggleState(dsp->getLoudnessTargetOn(), juce::dontSendNotification);
        lufsTgtTog->btn().onClick = [dsp, this]
            { dsp->setLoudnessTargetOn (lufsTgtTog->btn().getToggleState()); };
        addAndMakeVisible (*lufsTgtTog);

        // ── BLU-108: true-peak auto-ceiling ───────────────────────────────────
        tpTgtKnob = std::make_unique<VKnob>("dBTP", -1.0f,
            "True-peak target (dBTP). With Auto Ceil on, the ceiling is trimmed "
            "until the measured inter-sample peak stays under this - the margin "
            "streaming services and lossy transcodes need");
        tpTgtKnob->slider.setRange (-6.0, 0.0, 0.1);
        tpTgtKnob->slider.setValue (dsp->getTruePeakTargetDb(), juce::dontSendNotification);
        tpTgtKnob->slider.getProperties().set (DynamicsLAF::kKnobVariant, "modernAnalog");
        tpTgtKnob->slider.onValueChange = [dsp,this]
            { if (dsp) dsp->setTruePeakTargetDb ((float) tpTgtKnob->slider.getValue()); };
        addAndMakeVisible (*tpTgtKnob);

        autoCeilTog = std::make_unique<DualLabelToggle>();
        autoCeilTog->setupOnOff("Auto Ceil", "True-peak auto-ceiling: measures the real "
                                              "inter-sample peak of the output and lowers "
                                              "the ceiling until it is under the dBTP target");
        autoCeilTog->btn().setToggleState(dsp->getAutoCeiling(), juce::dontSendNotification);
        autoCeilTog->btn().onClick = [dsp, this]
            { dsp->setAutoCeiling (autoCeilTog->btn().getToggleState()); };
        addAndMakeVisible (*autoCeilTog);

        // ── BLU-110: LUFS beside dBFS ─────────────────────────────────────────
        loudMeter = std::make_unique<LoudnessMeter>();
        loudMeter->setTooltip ("Output loudness (LUFS short-term, left) beside output "
                               "peak (dBFS, right). The dashed line is the LUFS target");
        addAndMakeVisible (*loudMeter);

        // The output loudness meter costs K-weighting filters per sample, so the
        // DSP only runs it while something is watching.  Poked here so the first
        // frame has data, then re-poked from timerCallback.  No teardown call --
        // see pokeLufsMeter()'s comment for why a destructor must not touch the DSP.
        dsp->pokeLufsMeter();

        startTimerHz (30);
    }

    // QA-F chain-wiring fix (2026-07-10): see EditorPanelBase::bindToApvts +
    // the CompressorPanel override.  Every limiter control is param-backed
    // (the bsv_limiter_* set added with this fix -- the chain's limiter knobs
    // previously neither persisted nor had params at all).
    void bindToApvts (juce::AudioProcessorValueTreeState& apvts,
                      const juce::String& prefix) override
    {
        auto bindSlider = [&] (juce::Slider& s, const char* suffix)
        {
            mChainSliderAtts.push_back (std::make_unique<TaggedSliderAttachment> (
                apvts, prefix + suffix, s));
        };
        bindSlider (r1knobs[0]->slider, "limiter_inGain");
        bindSlider (r1knobs[1]->slider, "limiter_ceiling");
        bindSlider (r1knobs[2]->slider, "limiter_satThresh");
        bindSlider (r1knobs[3]->slider, "limiter_satCurve");
        bindSlider (r1knobs[4]->slider, "limiter_scHpf");
        bindSlider (r2knobs[0]->slider, "limiter_attack");
        bindSlider (r2knobs[1]->slider, "limiter_release");
        bindSlider (r2knobs[2]->slider, "limiter_ahead");
        bindSlider (r2knobs[3]->slider, "limiter_relCurve");
        if (sustainKnob)
            bindSlider (sustainKnob->slider, "limiter_sustain");

        auto bindButton = [&] (juce::Button& btn, const char* suffix)
        {
            mChainButtonAtts.push_back (
                std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                    apvts, prefix + suffix, btn));
        };
        if (autoRelTog) bindButton (autoRelTog->btn(), "limiter_autoRelease");
        if (autoMuTog)  bindButton (autoMuTog ->btn(), "limiter_autoMakeup");
        if (linkTog)    bindButton (linkTog   ->btn(), "limiter_stereoLink");

        // TS7 additions.  The chain drives its limiter from these parameters every
        // block, so anything unbound here would be reset on the next callback.
        if (lufsTgtKnob) bindSlider (lufsTgtKnob->slider, "limiter_loudTargetLufs");
        if (tpTgtKnob)   bindSlider (tpTgtKnob  ->slider, "limiter_truePeakTargetDb");
        if (lufsTgtTog)  bindButton (lufsTgtTog ->btn(),  "limiter_loudTargetOn");
        if (autoCeilTog) bindButton (autoCeilTog->btn(),  "limiter_autoCeiling");

        // The character selector is not a Slider or a Button, so it has no JUCE
        // attachment.  Remembering the APVTS lets its onChange write the parameter,
        // and the initial position is read back here.
        mChainApvts  = &apvts;
        mChainPrefix = prefix;
        if (charSel != nullptr)
            if (auto* cp = apvts.getRawParameterValue (prefix + "limiter_character"))
                charSel->setSelectedIndex ((int) std::lround (cp->load()),
                                           juce::dontSendNotification);
    }

    // Declared after the controls they attach to (reverse-order destruction).
    std::vector<std::unique_ptr<TaggedSliderAttachment>> mChainSliderAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> mChainButtonAtts;

    ~LimiterPanel() override
    {
        stopTimer();
        // Deliberately does NOT touch mDsp.  Removing a rack effect destroys the
        // DSP synchronously while this panel is still alive, so a dereference here
        // can read freed memory -- which is why the loudness meter is a watchdog
        // the DSP lets lapse on its own rather than something this releases.
        setLookAndFeel (nullptr);
    }

    void timerCallback() override
    {
        // liveDsp(), NOT mDsp -- see the note on EditorPanelBase::liveDsp.  A
        // null mDsp was never the danger; a DESTROYED one is, and only the
        // model can say which we are holding.
        auto* dsp = static_cast<LimiterDSP*> (liveDsp());
        if (dsp == nullptr) return;
        // Keep-alive for the output loudness meter (BLU-110).
        dsp->pokeLufsMeter();
        if (grMeter) grMeter->setGainReduction (dsp->getGainReductionDb());
        if (loudMeter)
            loudMeter->setValues (dsp->getOutputLufsShortTerm(),
                                  dsp->getOutputLevelDb(),
                                  dsp->getLoudnessTargetLufs(),
                                  dsp->getLoudnessTargetOn());
        // The two automatic modes move the ceiling and the input gain behind the
        // knobs, so the tooltips report the live trim -- otherwise the user has no
        // way to see why the sound differs from what the knobs read.
        // DualLabelToggle is not a tooltip client; its ToggleButton is.
        if (dsp->getLoudnessTargetOn() && lufsTgtTog)
            lufsTgtTog->btn().setTooltip ("Loudness target mode: active, currently trimming "
                                    + juce::String (dsp->getLoudnessServoDb(), 1)
                                    + " dB of input gain");
        if (dsp->getAutoCeiling() && autoCeilTog)
            autoCeilTog->btn().setTooltip ("True-peak auto-ceiling: measured "
                                     + juce::String (dsp->getOutputTruePeakDb(), 1)
                                     + " dBTP, ceiling trimmed "
                                     + juce::String (dsp->getCeilingTrimDb(), 1) + " dB");
    }

    void paint (juce::Graphics& g) override
    {
        DynamicsLAF::paintLA2APanel (g, getLocalBounds());
    }

    // The maximizer's own row: character selector then the two target knobs.
    // Shared by the Basic and Advanced layouts so the two cannot drift.
    void layoutMaximizerRow (juce::Rectangle<int> r)
    {
        if (charSel)
        {
            const int cs = juce::jmin (r.getHeight(), 56);
            charSel->setBounds (r.removeFromLeft (cs).withSizeKeepingCentre (cs, cs));
            r.removeFromLeft (6);
        }
        std::vector<VKnob*> row;
        if (lufsTgtKnob) row.push_back (lufsTgtKnob.get());
        if (tpTgtKnob)   row.push_back (tpTgtKnob.get());
        layoutKnobsH (r, row);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 2);

        // Strip meters
        if (vuIn) vuIn->setBounds (b.removeFromLeft (120).reduced (1, 2));

        // GR meter (left column, same height as content row)
        if (grMeter) {
            int gw = juce::jmin (96, b.getHeight() + 8);
            grMeter->setBounds (b.removeFromLeft (gw).reduced (1, 2));
            b.removeFromLeft (4);
        }

        dbfsOut      ->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (2);
        outputVolKnob->setBounds (b.removeFromRight (kKnobSz).withSizeKeepingCentre (kKnobSz, kKnobSz));
        b.removeFromRight (4);

        // Basic = the real Fruity controls (GAIN / Ceiling / SAT / Attack /
        // Release / Sustain) + the GR meter, i.e. the exact reference replica.
        // Advanced adds the pre-existing C1-C5 additions.
        //
        // TS7 (Jeff 2026-07-29): **Limiter mode is the FL reproduction and carries
        // NONE of the maximizer additions.**  The whole suite -- character, LUFS
        // target, dBTP target, loudness meter -- belongs to Maximizer mode, and
        // there it sits in BASIC so it is the first thing the user sees rather
        // than hidden behind Show Advanced.  The DSP gates its BEHAVIOUR on the
        // same mode (LimiterDSP::maximizerActive), so hiding these is not what
        // makes reproduction mode faithful -- it is faithful either way.
        const bool adv = ! mBasicMode;
        const bool maxMode = (mDsp != nullptr
                              && mDsp->getMode() == LimiterDSP::Mode::Maximizer);

        // Visibility is set EXPLICITLY for every control rather than only for the
        // ones that change: a control left visible but unpositioned by the branch
        // below would paint at whatever bounds it last had.
        auto show = [] (juce::Component* c, bool v) { if (c) c->setVisible (v); };
        const bool refBallistics = adv || ! maxMode;   // Atk/Rel/Sustain

        // Maximizer Basic is EXACTLY the six controls §1.4 lists (Jeff,
        // 2026-07-30): character selector, LUFS target knob + toggle, dBTP knob +
        // Auto-Ceil toggle, LUFS/dBFS meter, GR meter.  InGain / Ceil / SatTh
        // were mine on the argument that a maximizer "can't work without" them;
        // it can -- the servo drives gain and Auto-Ceil drives the ceiling, which
        // is the entire point of the mode.  They stay in Maximizer ADVANCED.
        const bool refLevels = adv || ! maxMode;   // InGain / Ceil / SatTh

        show (r1knobs.size() > 0 ? r1knobs[0].get() : nullptr, refLevels);   // InGain
        show (r1knobs.size() > 1 ? r1knobs[1].get() : nullptr, refLevels);   // Ceil
        show (r1knobs.size() > 2 ? r1knobs[2].get() : nullptr, refLevels);   // SatTh
        show (r1knobs.size() > 3 ? r1knobs[3].get() : nullptr, adv);         // SatCv
        show (r1knobs.size() > 4 ? r1knobs[4].get() : nullptr, adv);         // SCHPF
        show (r2knobs.size() > 0 ? r2knobs[0].get() : nullptr, refBallistics); // Atk
        show (r2knobs.size() > 1 ? r2knobs[1].get() : nullptr, refBallistics); // Rel
        show (r2knobs.size() > 2 ? r2knobs[2].get() : nullptr, adv);         // Ahead
        show (r2knobs.size() > 3 ? r2knobs[3].get() : nullptr, adv);         // RelCv
        show (sustainKnob.get(), refBallistics);
        show (autoRelTog.get(),  adv);
        show (autoMuTog.get(),   adv);
        show (linkTog.get(),     adv);

        // The maximizer suite is visible in Maximizer mode ONLY, and there it is
        // Basic-tier so it shows without touching Show Advanced.
        show (charSel.get(),     maxMode);
        show (lufsTgtKnob.get(), maxMode);
        show (lufsTgtTog.get(),  maxMode);
        show (tpTgtKnob.get(),   maxMode);
        show (autoCeilTog.get(), maxMode);
        show (loudMeter.get(),   maxMode);

        if (maxMode)
        {
            // BLU-110 meter sits beside the GR meter -- together they are the
            // "what is happening" column.
            if (loudMeter)
            {
                const int lw = juce::jmin (72, juce::jmax (40, b.getWidth() / 8));
                loudMeter->setBounds (b.removeFromLeft (lw).reduced (1, 2));
                b.removeFromLeft (4);
            }

            // Toggle columns.  Target + Auto Ceil are the maximizer's own pair and
            // stay visible in Basic; the three ballistics toggles are Advanced.
            constexpr int kToggleColW = 62;
            const int togCols = adv ? 3 : 1;
            auto togArea = b.removeFromRight (kToggleColW * togCols + 4);
            b.removeFromRight (2);

            auto colFor = [&] (juce::Rectangle<int>& area, DualLabelToggle* top,
                               DualLabelToggle* bottom)
            {
                auto col = area.removeFromRight (kToggleColW);
                const int halfH = col.getHeight() / 2;
                auto topSlot = col.removeFromTop (halfH);
                if (top)    top   ->setBounds (topSlot.reduced (1));
                if (bottom) bottom->setBounds (col    .reduced (1));
                area.removeFromRight (2);
            };
            if (adv)
            {
                colFor (togArea, autoRelTog.get(), linkTog.get());
                colFor (togArea, autoMuTog.get(),  nullptr);
            }
            colFor (togArea, lufsTgtTog.get(), autoCeilTog.get());

            // Basic: one row of the maximizer's own controls.  Advanced: the two
            // reference rows on top, maximizer controls on the third.
            if (adv)
            {
                const int rowH = b.getHeight() / 3;
                auto r1 = b.removeFromTop (rowH);
                auto r2 = b.removeFromTop (rowH);
                auto r3 = b;
                layoutKnobsH (r1, r1knobs);
                std::vector<VKnob*> row2;
                for (auto& k : r2knobs) if (k) row2.push_back (k.get());
                if (sustainKnob) row2.push_back (sustainKnob.get());
                layoutKnobsH (r2, row2);
                layoutMaximizerRow (r3);
            }
            else
            {
                // Maximizer Basic: the maximizer's own controls and nothing else.
                // Every reference knob is Advanced-tier here, so the row gets the
                // full panel height rather than sharing it.
                layoutMaximizerRow (b);
            }
        }
        else if (adv)
        {
            // Limiter mode, Advanced: the pre-existing C1-C5 additions only.
            constexpr int kToggleColW = 62;
            auto togArea = b.removeFromRight (kToggleColW * 2 + 2);
            b.removeFromRight (2);
            auto rightCol = togArea.removeFromRight (kToggleColW);
            const int halfH = rightCol.getHeight() / 2;
            auto autoRelSlot = rightCol.removeFromTop (halfH);
            if (autoRelTog) autoRelTog->setBounds (autoRelSlot.reduced (1));
            if (linkTog)    linkTog   ->setBounds (rightCol .reduced (1));
            togArea.removeFromRight (2);
            if (autoMuTog) autoMuTog->setBounds (
                togArea.withSizeKeepingCentre (kToggleColW, halfH).reduced (1));

            auto r1 = b.removeFromTop (b.getHeight() / 2);
            auto r2 = b;
            layoutKnobsH (r1, r1knobs);
            std::vector<VKnob*> row2;
            for (auto& k : r2knobs) if (k) row2.push_back (k.get());
            if (sustainKnob) row2.push_back (sustainKnob.get());
            layoutKnobsH (r2, row2);
        }
        else
        {
            // Basic single row: GAIN / Ceiling / SAT / Attack / Release / Sustain.
            std::vector<VKnob*> basic;
            if (r1knobs[0]) basic.push_back (r1knobs[0].get());   // InGain (GAIN)
            if (r1knobs[1]) basic.push_back (r1knobs[1].get());   // Ceiling
            if (r1knobs[2]) basic.push_back (r1knobs[2].get());   // SatTh (SAT)
            if (r2knobs[0]) basic.push_back (r2knobs[0].get());   // Attack
            if (r2knobs[1]) basic.push_back (r2knobs[1].get());   // Release
            if (sustainKnob) basic.push_back (sustainKnob.get()); // Sustain
            layoutKnobsH (b, basic);
        }
    }
};

// ── Factory ───────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
// ConsoleSaturationPanel - H-7 (2026-05-01)
// Minimalist preamp/console layout: Drive + Tone + Mix + Output.  Selected
// when SaturationDSP's mSatType == Console.  Tube-only fields (TubeType
// chickenhead, BassRelief, Sensitivity, Transformer) are not exposed here --
// Console mode interprets the underlying DSP differently and these knobs
// would be confusing.
// ─────────────────────────────────────────────────────────────────────────────
struct ConsoleSaturationPanel : public EditorPanelBase
{
    SaturationDSP* mDsp { nullptr };
    std::unique_ptr<ChickenHeadSelector> harmModeSel;       // Keep Low / Normal / Keep High
    std::unique_ptr<ChickenHeadSelector> consoleModeSel;    // Clean / Dirty voicing
    std::unique_ptr<DualLabelToggle>     colorTog;          // 2nd-harmonic color master enable

    // Basic = Drive / Color / Clean-Dirty / Color-toggle / Output (a reference
    // console saturator's set).  Advanced reveals our extras: Mix + the
    // Keep-Low/Normal/Keep-High harmonics routing.
    bool hasAdvancedControls() const override { return true; }

    explicit ConsoleSaturationPanel (SaturationDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&HarmonicLAF::get());

        buildKnobs (*this, knobs, {
            { "Drive",  0.f,  10.f,  3.f,   0.05f, "Preamp drive (gentler than Tube tanh)" },
            { "Color",  0.f,  10.f,  3.f,   0.05f, "Transformer 2nd-harmonic colour" },
            { "Mix",    0.f, 100.f, 70.f,   0.5f,  "Wet / dry blend (%)" },
            { "Output",-18.f, 18.f,  0.f,   0.5f,  "Output gain (dB)" },
        });

        knobs[0]->slider.onValueChange = [dsp,this]{ if (dsp) dsp->setFlowers ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this]{ if (dsp) dsp->setDabs    ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this]{ if (dsp) dsp->setWet     ((float) knobs[2]->slider.getValue()); };
        knobs[3]->slider.onValueChange = [dsp,this]{ if (dsp) dsp->setOut     ((float) knobs[3]->slider.getValue()); };

        if (dsp)
        {
            knobs[0]->slider.setValue (dsp->mFlowers, juce::sendNotificationSync);
            knobs[1]->slider.setValue (dsp->mDabs,    juce::sendNotificationSync);
            knobs[2]->slider.setValue (dsp->mWet,     juce::sendNotificationSync);
            knobs[3]->slider.setValue (dsp->mOut,     juce::sendNotificationSync);
        }

        // H-7: Keep Low / Normal / Keep High harmonic-routing chickenhead.
        harmModeSel = std::make_unique<ChickenHeadSelector>();
        harmModeSel->setOptions ({
            { "Lo",  "Keep Low",  "Saturate highs only -- bass passes through dry" },
            { "Nrm", "Normal",    "Full-band saturation (default)" },
            { "Hi",  "Keep High", "Saturate lows only -- highs pass through dry" },
        });
        harmModeSel->setBodyTooltip ("Harmonics routing");
        if (dsp)
            harmModeSel->setSelectedIndex ((int) dsp->mHarmonicsMode, juce::dontSendNotification);
        harmModeSel->onChange = [dsp] (int idx)
        {
            if (dsp) dsp->setHarmonicsMode (idx);
        };
        addAndMakeVisible (*harmModeSel);

        // Clean / Dirty console voicing.
        consoleModeSel = std::make_unique<ChickenHeadSelector>();
        consoleModeSel->setOptions ({
            { "Cln", "Clean", "Transparent console color - soft clip + subtle even-harmonic glue" },
            { "Drt", "Dirty", "Driven console - low-frequency-weighted saturation + low-end bloom" },
        });
        consoleModeSel->setBodyTooltip ("Console character");
        if (dsp)
            consoleModeSel->setSelectedIndex ((int) dsp->mConsoleMode, juce::dontSendNotification);
        consoleModeSel->onChange = [dsp] (int idx) { if (dsp) dsp->setConsoleMode (idx); };
        addAndMakeVisible (*consoleModeSel);

        // Color master enable -- the dead-Color fix exposes this (default on).
        colorTog = std::make_unique<DualLabelToggle>();
        colorTog->setupOnOff ("Color", "Harmonic color - adds 2nd-harmonic saturation (off = clean soft-clip only)");
        if (dsp)
            colorTog->btn().setToggleState (dsp->mConsoleColor, juce::dontSendNotification);
        colorTog->btn().onClick = [dsp, this] { if (dsp) dsp->setConsoleColor (colorTog->btn().getToggleState()); };
        addAndMakeVisible (*colorTog);
    }

    ~ConsoleSaturationPanel() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        // H-7 (2026-05-01): match the existing SaturationPanel's Hammerite
        // panel paint so Console + Tube look like siblings, not strangers.
        HarmonicLAF::paintHammeritePanel (g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);

        // VU meter on the left (matches existing SaturationPanel).
        if (vuIn) vuIn->setBounds (b.removeFromLeft (120).reduced (1, 2));
        b.removeFromLeft (4);

        dbfsOut->setBounds (b.removeFromRight (24).reduced (1, 2));
        b.removeFromRight (2);
        outputVolKnob->setBounds (b.removeFromRight (kKnobSz).withSizeKeepingCentre (kKnobSz, kKnobSz));
        b.removeFromRight (4);

        // Advanced-only: Mix knob (idx 2) + the harmonics-routing selector.
        const bool adv = ! mBasicMode;
        if (harmModeSel) harmModeSel->setVisible (adv);
        if (knobs.size() > 2 && knobs[2]) knobs[2]->setVisible (adv);

        // Right cluster (right-to-left): [harmonics routing if adv] | Color | Clean/Dirty.
        if (adv && harmModeSel)
        {
            auto col = b.removeFromRight (66);
            harmModeSel->setBounds (col.reduced (2));
            b.removeFromRight (4);
        }
        if (colorTog)
        {
            auto col = b.removeFromRight (62);
            colorTog->setBounds (col.reduced (1));
            b.removeFromRight (4);
        }
        if (consoleModeSel)
        {
            auto col = b.removeFromRight (66);
            consoleModeSel->setBounds (col.reduced (2));
            b.removeFromRight (4);
        }

        // Knobs fill the rest: Drive / Color / (Mix in Advanced) / Output.
        std::vector<VKnob*> vis;
        for (int i = 0; i < (int) knobs.size(); ++i)
            if (adv || i != 2)
                if (knobs[i]) vis.push_back (knobs[i].get());
        layoutKnobsH (b, vis, kKnobSz);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// DeEsserPanel - H-3/H-6 (2026-05-01)
// ─────────────────────────────────────────────────────────────────────────────
struct DeEsserPanel : public EditorPanelBase, private juce::Timer
{
    DeEsserDSP* mDsp { nullptr };
    std::unique_ptr<DualLabelToggle>     monitorTog;     // Basic (reference: solo detected ess)
    std::unique_ptr<DualLabelToggle>     engineTog;      // Advanced: Time <-> Spectral engine
    std::unique_ptr<DualLabelToggle>     qualityTog;     // Advanced: spectral HQ <-> low-latency
    std::unique_ptr<ChickenHeadSelector> msSel;          // Advanced (our M/S extra)

    // Basic = the reference's control set (Detection width / Threshold / Range /
    // Mode blend + Monitor); Advanced = our extras (explicit Freq+Q,
    // Attack/Release, lookahead knob, Mix, M/S, Spectral engine + quality).
    // Lookahead is Advanced-only as the continuous knob -- owner call
    // 2026-07-01 dropped the reference's on/off toggle + its playback lock;
    // the Engine/Quality switches DO lock during playback (they change
    // latency) per the same owner call.
    bool hasAdvancedControls() const override { return true; }

    ~DeEsserPanel() override { setLookAndFeel (nullptr); }

    explicit DeEsserPanel (DeEsserDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&DynamicsLAF::get());

        // Knobs 0-3 Basic, 4-9 Advanced.
        buildKnobs (*this, knobs, {
            { "Detect",  0.f,   100.f,  50.f,  1.f,   "Detection width. Narrow = S sounds only; Wide = also catches Sh sounds" },
            { "Thresh", -80.f,    0.f, -12.5f, 0.5f,  "Threshold (dB). Detector triggers above this level" },
            { "Range",  -48.f,    0.f, -14.f,  0.5f,  "Max reduction (dB). Caps how much the de-esser can pull down" },
            { "Mode",    0.f,   100.f,  50.f,  1.f,   "0 = Wide (duck the whole detected sound), 100 = Split (duck only above 4 kHz), between = blend" },
            { "Freq",   4000.f, 12000.f, 6500.f, 10.f, "Detector HPF cutoff (Hz). Direct control of what Detect sets" },
            { "Q",       0.5f,    4.0f,  1.4f, 0.05f, "Detector HPF Q. Higher = narrower detection band" },
            { "Atk",     0.1f,   30.f,   1.f,  0.1f,  "Attack (ms)" },
            { "Rel",    10.f,   500.f,  80.f,  1.f,   "Release (ms)" },
            { "Look",    0.f, DeEsserDSP::kMaxLookaheadMs, 0.f, 0.05f, "Lookahead (ms). Detector sees the audio early so the duck lands on the consonant. Not used by the Spectral engine" },
            { "Mix",     0.f,     1.f,   1.f,  0.01f, "Wet / dry blend" },
        });

        // DynamicsLAF "modernAnalog" knob variant -- matches Compressor +
        // Transient Shaper panels for visual consistency across all dynamics-
        // family effects.
        for (auto& k : knobs)
            k->slider.getProperties().set (DynamicsLAF::kKnobVariant, "modernAnalog");

        // Detect = macro over Freq+Q (the reference exposes only an abstract
        // 0..100 width, no Hz): narrow(0) -> 7500 Hz tight, wide(100) -> 4000 Hz
        // broad.  Advanced Freq edits reflect back into Detect's position.
        knobs[0]->slider.onValueChange = [dsp,this]{
            const float d  = (float) knobs[0]->slider.getValue();
            const float fq = 7500.0f - (d / 100.0f) * 3500.0f;
            const float qq = 2.2f    - (d / 100.0f) * 1.4f;
            dsp->setFrequencyHz (fq);
            dsp->setQ (qq);
            knobs[4]->slider.setValue (fq, juce::dontSendNotification);
            knobs[5]->slider.setValue (qq, juce::dontSendNotification);
        };
        knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setThresholdDb ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setRangeDb     ((float) knobs[2]->slider.getValue()); };
        knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setModeBlend   ((float) knobs[3]->slider.getValue()); };
        knobs[4]->slider.onValueChange = [dsp,this]{
            const float fq = (float) knobs[4]->slider.getValue();
            dsp->setFrequencyHz (fq);
            knobs[0]->slider.setValue (juce::jlimit (0.0, 100.0, (7500.0 - fq) / 35.0),
                                       juce::dontSendNotification);
        };
        knobs[5]->slider.onValueChange = [dsp,this]{ dsp->setQ         ((float) knobs[5]->slider.getValue()); };
        knobs[6]->slider.onValueChange = [dsp,this]{ dsp->setAttackMs  ((float) knobs[6]->slider.getValue()); };
        knobs[7]->slider.onValueChange = [dsp,this]{ dsp->setReleaseMs ((float) knobs[7]->slider.getValue()); };
        knobs[8]->slider.onValueChange = [dsp,this]{
            dsp->setLookaheadMs ((float) knobs[8]->slider.getValue());
            if (onLatencyChanged) onLatencyChanged();   // lookahead changes getLatencySamples -> refresh PDC
        };
        knobs[9]->slider.onValueChange = [dsp,this]{ dsp->setMix ((float) knobs[9]->slider.getValue()); };

        monitorTog = std::make_unique<DualLabelToggle>();
        monitorTog->setupOnOff ("Monitor", "Solo the detected ess -- hear only what the de-esser is removing.");
        monitorTog->btn().setToggleState (dsp->mListen, juce::dontSendNotification);
        monitorTog->btn().onClick = [dsp,this]{ dsp->setListen (monitorTog->btn().getToggleState()); };
        addAndMakeVisible (*monitorTog);

        // Engine + Quality switches change getLatencySamples() -> locked while
        // the transport runs (timer below) and poke a PDC refresh on flip.
        engineTog = std::make_unique<DualLabelToggle>();
        engineTog->setupOnOff ("Spectral", "Spectral engine: per-frequency de-essing -- only the ess "
                               "frequencies duck, vowels keep their brightness. Adds latency, so it is "
                               "locked while playback is running.");
        engineTog->btn().setToggleState (dsp->mEngine == DeEsserDSP::Engine::Spectral,
                                         juce::dontSendNotification);
        engineTog->btn().onClick = [dsp,this]{
            dsp->setEngine (engineTog->btn().getToggleState() ? 1 : 0);
            if (onLatencyChanged) onLatencyChanged();
        };
        addAndMakeVisible (*engineTog);

        qualityTog = std::make_unique<DualLabelToggle>();
        qualityTog->setupOnOff ("LowLat", "Spectral quality: Off = HQ (2048 FFT, ~43 ms, cleanest "
                                "ess/vowel separation); On = low-latency (1024 FFT, ~21 ms, slightly "
                                "coarser). Locked while playback is running -- switching resets the "
                                "spectral engine.");
        qualityTog->btn().setToggleState (dsp->mSpectralQuality == 1, juce::dontSendNotification);
        qualityTog->btn().onClick = [dsp,this]{
            dsp->setSpectralQuality (qualityTog->btn().getToggleState() ? 1 : 0);
            if (onLatencyChanged) onLatencyChanged();
        };
        addAndMakeVisible (*qualityTog);

        startTimerHz (10);   // transport lock for Engine/Quality + Look-in-Spectral greying

        msSel = std::make_unique<ChickenHeadSelector>();
        msSel->setOptions ({
            { "St", "Stereo", "Detect and duck the stereo signal (linked)" },
            { "M",  "Mid",    "Duck only the Mid (center) component" },
            { "S",  "Side",   "Duck only the Side (width) component" },
        });
        msSel->setSelectedIndex (juce::jlimit (0, 2, (int) dsp->mMsMode), juce::dontSendNotification);
        msSel->setBodyTooltip ("Mid/Side processing target");
        msSel->onChange = [dsp] (int idx) { dsp->setMsMode (idx); };
        addAndMakeVisible (*msSel);

        // Sync slider visual state from current DSP state.  Detect last, with
        // no notification: its position is derived from the saved Freq (the
        // macro must not stomp a custom Advanced Freq/Q on rebuild).
        knobs[1]->slider.setValue (dsp->mThresholdDb, juce::sendNotificationSync);
        knobs[2]->slider.setValue (dsp->mRangeDb,     juce::sendNotificationSync);
        knobs[3]->slider.setValue (dsp->mModeBlend,   juce::sendNotificationSync);
        knobs[4]->slider.setValue (dsp->mFreqHz,      juce::sendNotificationSync);
        knobs[5]->slider.setValue (dsp->mQ,           juce::sendNotificationSync);
        knobs[6]->slider.setValue (dsp->mAttackMs,    juce::sendNotificationSync);
        knobs[7]->slider.setValue (dsp->mReleaseMs,   juce::sendNotificationSync);
        knobs[8]->slider.setValue (dsp->mLookaheadMs, juce::sendNotificationSync);
        knobs[9]->slider.setValue (dsp->mMix,         juce::sendNotificationSync);
        knobs[0]->slider.setValue (juce::jlimit (0.0, 100.0, (7500.0 - dsp->mFreqHz) / 35.0),
                                   juce::dontSendNotification);
    }

    // QA-F chain-wiring fix (2026-07-10): see EditorPanelBase::bindToApvts +
    // the CompressorPanel override.  Knob 0 (Detect) is deliberately unbound
    // -- it is a MACRO over Freq+Q with no state of its own; binding Freq+Q
    // covers it (and its position re-derives from Freq on mount).
    void bindToApvts (juce::AudioProcessorValueTreeState& apvts,
                      const juce::String& prefix) override
    {
        auto bindSlider = [&] (juce::Slider& s, const char* suffix)
        {
            mChainSliderAtts.push_back (std::make_unique<TaggedSliderAttachment> (
                apvts, prefix + suffix, s));
        };
        bindSlider (knobs[1]->slider, "deesser_threshold");
        bindSlider (knobs[2]->slider, "deesser_range");
        bindSlider (knobs[3]->slider, "deesser_modeBlend");
        bindSlider (knobs[4]->slider, "deesser_freq");
        bindSlider (knobs[5]->slider, "deesser_q");
        bindSlider (knobs[6]->slider, "deesser_attack");
        bindSlider (knobs[7]->slider, "deesser_release");
        bindSlider (knobs[8]->slider, "deesser_lookahead");
        bindSlider (knobs[9]->slider, "deesser_mix");

        auto bindButton = [&] (juce::Button& btn, const char* suffix)
        {
            mChainButtonAtts.push_back (
                std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                    apvts, prefix + suffix, btn));
        };
        if (monitorTog) bindButton (monitorTog->btn(), "deesser_listen");
        if (engineTog)  bindButton (engineTog ->btn(), "deesser_spectral");
        if (qualityTog) bindButton (qualityTog->btn(), "deesser_lowlat");

        if (auto* p = apvts.getParameter (prefix + "deesser_msMode"))
        {
            auto prev = msSel->onChange;
            msSel->onChange = [prev, p, um = apvts.undoManager] (int idx)
            {
                if (prev) prev (idx);
                beginParamUndoGesture (um, p->paramID); // Task 6 (12-iv)
                p->setValueNotifyingHost (
                    p->getNormalisableRange().convertTo0to1 ((float) idx));
            };
            mChainSelAtts.push_back (std::make_unique<juce::ParameterAttachment> (*p,
                [this] (float v)
                {
                    if (msSel)
                        msSel->setSelectedIndex ((int) std::lround (v),
                                                 juce::dontSendNotification);
                }, nullptr));
            mChainSelAtts.back()->sendInitialUpdate();
        }
    }

    // Declared after the controls they attach to (reverse-order destruction).
    std::vector<std::unique_ptr<TaggedSliderAttachment>> mChainSliderAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> mChainButtonAtts;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> mChainSelAtts;

    void timerCallback() override
    {
        const bool locked = DSPBase::isTransportPlaying();
        if (engineTog)  engineTog ->setEnabled (! locked);
        if (qualityTog) qualityTog->setEnabled (! locked);
        // Look knob is a no-op in the Spectral engine (frame analysis sees
        // ahead inherently) -- grey it there.
        const bool spectral = (mDsp != nullptr
                               && mDsp->mEngine == DeEsserDSP::Engine::Spectral);
        if (knobs.size() > 8) knobs[8]->setEnabled (! spectral);
    }

    void paint (juce::Graphics& g) override
    {
        // LA-2A cream/wood panel paint (same as Compressor + Transient Shaper).
        DynamicsLAF::paintLA2APanel (g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);

        // Right-edge meter + output-vol knob (matches Compressor / Limiter).
        dbfsOut      ->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (2);
        outputVolKnob->setBounds (b.removeFromRight (kKnobSz).withSizeKeepingCentre (kKnobSz, kKnobSz));
        b.removeFromRight (4);

        const bool adv = ! mBasicMode;
        for (size_t i = 4; i < knobs.size(); ++i)
            knobs[i]->setVisible (adv);
        if (msSel)      msSel     ->setVisible (adv);
        if (engineTog)  engineTog ->setVisible (adv);
        if (qualityTog) qualityTog->setVisible (adv);

        // Monitor switch at the right of the knob area (Basic, always visible).
        auto togSlot = b.removeFromRight (52);
        if (monitorTog) monitorTog->setBounds (togSlot.withSizeKeepingCentre (50, 40));
        b.removeFromRight (2);

        if (! adv)
        {
            std::vector<VKnob*> row;
            for (int i = 0; i < 4; ++i) row.push_back (knobs[(size_t) i].get());
            layoutKnobsH (b, row, kKnobSz);
        }
        else
        {
            auto r1 = b.removeFromTop (b.getHeight() / 2);
            auto r2 = b;
            if (msSel)
            {
                auto s = r2.removeFromRight (56);
                msSel->setBounds (s.reduced (2));
                r2.removeFromRight (2);
            }
            if (qualityTog)
            {
                auto s = r2.removeFromRight (52);
                qualityTog->setBounds (s.withSizeKeepingCentre (50, 40));
                r2.removeFromRight (2);
            }
            if (engineTog)
            {
                auto s = r2.removeFromRight (52);
                engineTog->setBounds (s.withSizeKeepingCentre (50, 40));
                r2.removeFromRight (2);
            }
            std::vector<VKnob*> row1, row2;
            for (int i = 0; i < 6;  ++i) row1.push_back (knobs[(size_t) i].get());
            for (int i = 6; i < 10; ++i) row2.push_back (knobs[(size_t) i].get());
            layoutKnobsH (r1, row1);
            layoutKnobsH (r2, row2);
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// I-5 (2026-05-02): Harmonics drive pedals batch -- 5 inline panels.
//
// Layout: right-clustered per locked spec call #8.  All controls live on the
// right side of the panel near the dBFS meter; the left side is intentionally
// empty so BaySickPedals (post-I-15) can host the same panel in a smaller
// frame that crops the empty left portion.  In the FX rack the full panel
// shows; the empty left is harmless visually.  All five panels:
//   * inherit EditorPanelBase
//   * use HarmonicLAF (matches the existing OverdriveDSP / SaturationDSP
//     panel aesthetic; locked LAF group "Harmonics")
//   * call disableOutputVolKnob() so the panel-owned Level knob is the only
//     output control (no duplicate FX-rack Vol knob)
//   * disable the input VU meter (keeps the layout clean for cropping)
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    // Shared helper: paint Hammerite olive panel + lay out a right-clustered
    // knob row.  Knobs are laid right-to-left so the rightmost knob sits next
    // to the dBFS meter.  Returns the leftover empty rectangle on the left.
    juce::Rectangle<int> rightClusterKnobs (juce::Rectangle<int> bounds,
                                             std::vector<std::unique_ptr<VKnob>>& ks,
                                             int slotW = kKnobSz + 16,
                                             int sz    = kKnobSz)
    {
        for (int i = (int) ks.size() - 1; i >= 0; --i)
        {
            auto col = bounds.removeFromRight (slotW);
            ks[(size_t) i]->setBounds (col.withSizeKeepingCentre (sz, sz));
        }
        return bounds;   // remaining empty-left rect
    }

    // QA-Layout T7 (Specific-4): shared pedal-tile grid.  The BaySickPedals
    // tile (~190x137 inner) cannot fit the full-mode right-cluster + dBFS
    // column -- knobs collapsed onto each other (Jeff's overlap finding).
    // Grid every knob (plus an optional trailing widget, e.g. a mode
    // selector) into the smallest grid that holds them; the dBFS column is
    // dropped (hidden on the board anyway).  OctaveStylePanel / FurmanEQ's
    // hand-built 3x2/3x3 grids were the originals; this generalizes them.
    void pedalTileGrid (juce::Rectangle<int> b,
                        std::vector<std::unique_ptr<VKnob>>& ks,
                        std::initializer_list<juce::Component*> extras = {})
    {
        int nExtras = 0;
        for (auto* e : extras) if (e != nullptr) ++nExtras;
        const int n = (int) ks.size() + nExtras;
        if (n <= 0) return;
        const int cols  = n <= 3 ? juce::jmax (1, n) : (n == 4 ? 2 : 3);
        const int rows  = (n + cols - 1) / cols;
        const int cellW = b.getWidth()  / cols;
        const int cellH = b.getHeight() / rows;
        auto cell = [&] (int idx)
        {
            return juce::Rectangle<int> (b.getX() + (idx % cols) * cellW,
                                         b.getY() + (idx / cols) * cellH,
                                         cellW, cellH);
        };
        const int kSz = juce::jlimit (28, 56, juce::jmin (cellW - 6, cellH - 16));
        int idx = 0;
        for (auto& k : ks)
            k->setBounds (cell (idx++).withSizeKeepingCentre (kSz, kSz + 14));
        for (auto* e : extras)
            if (e != nullptr)
                e->setBounds (cell (idx++).reduced (3));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// BluesDriveStylePanel - BD Style Blues Drive
// 3 knobs: Drive / Tone / Level
// ─────────────────────────────────────────────────────────────────────────────
struct BluesDriveStylePanel : public EditorPanelBase
{
    BluesDriveStyleDSP* mDsp { nullptr };

    explicit BluesDriveStylePanel (BluesDriveStyleDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&HarmonicLAF::get());
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Drive", 0.f, 1.f, 0.5f, 0.001f, "Drive amount (0 = clean, 1 = heavy overdrive)" },
            { "Tone",  0.f, 1.f, 0.5f, 0.001f, "Tone (LPF cutoff sweep 500 Hz - 5 kHz)" },
            { "Level", -24.f, 12.f, 0.f, 0.1f, "Output level (dB)" },
        });

        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setDrive ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setTone  ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setLevel ((float) knobs[2]->slider.getValue()); };

        if (dsp)
        {
            knobs[0]->slider.setValue (dsp->mDrive, juce::sendNotificationSync);
            knobs[1]->slider.setValue (dsp->mTone,  juce::sendNotificationSync);
            knobs[2]->slider.setValue (dsp->mLevel, juce::sendNotificationSync);
        }
    }

    ~BluesDriveStylePanel() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        HarmonicLAF::paintHammeritePanel (g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): pedal tiles grid instead of right-clustering.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
            return pedalTileGrid (b, knobs);
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);
        rightClusterKnobs (b, knobs);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// OverdrivePedalPanel - OD Style Overdrive (Type::Pedal on OverdriveDSP)
// Reuses OverdriveDSP existing knob fields: PreAmp = Drive, PostFilter = Tone,
// PostGain = Level.
// ─────────────────────────────────────────────────────────────────────────────
struct OverdrivePedalPanel : public EditorPanelBase
{
    OverdriveDSP* mDsp { nullptr };

    explicit OverdrivePedalPanel (OverdriveDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&HarmonicLAF::get());
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Drive", 0.f,    10.f,   5.f,   0.01f, "Drive amount (PreAmp gain into the dual-stage soft clip)" },
            { "Tone",  200.f,  8000.f, 4000.f, 1.f,  "Tone -- post-clip LPF cutoff (Hz)" },
            { "Level", -18.f,  18.f,   0.f,   0.1f,  "Output level (dB)" },
        });
        knobs[1]->slider.setSkewFactor (0.4);   // log-feel sweep on the Hz knob

        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setPreAmp     ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setPostFilter ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setPostGain   ((float) knobs[2]->slider.getValue()); };

        if (dsp)
        {
            knobs[0]->slider.setValue (dsp->mPreAmp,     juce::sendNotificationSync);
            knobs[1]->slider.setValue (dsp->mPostFilter, juce::sendNotificationSync);
            knobs[2]->slider.setValue (dsp->mPostGain,   juce::sendNotificationSync);
        }
    }

    ~OverdrivePedalPanel() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        HarmonicLAF::paintHammeritePanel (g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): pedal tiles grid instead of right-clustering.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
            return pedalTileGrid (b, knobs);
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);
        rightClusterKnobs (b, knobs);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// DistortionStylePanel - DS Style Distortion
// 3 knobs: Tone / Level / Dist
// ─────────────────────────────────────────────────────────────────────────────
struct DistortionStylePanel : public EditorPanelBase
{
    DistortionStyleDSP* mDsp { nullptr };

    explicit DistortionStylePanel (DistortionStyleDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&HarmonicLAF::get());
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Dist",  0.f,   1.f,   0.5f, 0.001f, "Distortion amount (drives the hard-clipper into a square wave at max)" },
            { "Tone",  0.f,   1.f,   0.5f, 0.001f, "Tone (mid-scoop tilt EQ -- left = darker, right = brighter, center = scooped mids)" },
            { "Level", -24.f, 12.f,  0.f,  0.1f,   "Output level (dB)" },
        });

        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setDist  ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setTone  ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setLevel ((float) knobs[2]->slider.getValue()); };

        if (dsp)
        {
            knobs[0]->slider.setValue (dsp->mDist,  juce::sendNotificationSync);
            knobs[1]->slider.setValue (dsp->mTone,  juce::sendNotificationSync);
            knobs[2]->slider.setValue (dsp->mLevel, juce::sendNotificationSync);
        }
    }

    ~DistortionStylePanel() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        HarmonicLAF::paintHammeritePanel (g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): pedal tiles grid instead of right-clustering.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
            return pedalTileGrid (b, knobs);
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);
        rightClusterKnobs (b, knobs);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// FuzzStylePanel - FZ Style Fuzz
// 4 controls: Fuzz / Boost / Level + Mode chickenhead (Gated / Germanium / Octave)
// ─────────────────────────────────────────────────────────────────────────────
struct FuzzStylePanel : public EditorPanelBase
{
    FuzzStyleDSP* mDsp { nullptr };
    std::unique_ptr<ChickenHeadSelector> modeSel;

    explicit FuzzStylePanel (FuzzStyleDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&HarmonicLAF::get());
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Fuzz",  0.f,   1.f,   0.5f, 0.001f, "Fuzz amount" },
            { "Boost", 0.f,   20.f,  0.f,  0.1f,   "Input boost into the fuzz (dB) -- pushes the clipping harder without changing Fuzz" },
            { "Level", -24.f, 12.f,  0.f,  0.1f,   "Output level (dB)" },
        });

        modeSel = std::make_unique<ChickenHeadSelector>();
        modeSel->setOptions ({
            { "Gt", "Gated",     "Bias-starved gated fuzz" },
            { "Ge", "Germanium", "Warm germanium soft-clip fuzz" },
            { "Oc", "Octave",    "Full-wave rectifier + hard clip, upper-octave fuzz" },
        });
        modeSel->setBodyTooltip ("Fuzz mode");
        modeSel->setSelectedIndex (dsp ? (int) dsp->mMode : (int) FuzzStyleDSP::Mode::F,
                                    juce::dontSendNotification);
        modeSel->onChange = [dsp] (int idx) { if (dsp) dsp->setMode (idx); };
        addAndMakeVisible (*modeSel);

        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setFuzz  ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setBoost ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setLevel ((float) knobs[2]->slider.getValue()); };

        if (dsp)
        {
            knobs[0]->slider.setValue (dsp->mFuzz,  juce::sendNotificationSync);
            knobs[1]->slider.setValue (dsp->mBoost, juce::sendNotificationSync);
            knobs[2]->slider.setValue (dsp->mLevel, juce::sendNotificationSync);
        }
    }

    ~FuzzStylePanel() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        HarmonicLAF::paintHammeritePanel (g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): pedal tiles grid instead of right-clustering.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
            return pedalTileGrid (b, knobs, { modeSel ? &*modeSel : nullptr });
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);
        // Mode chickenhead sits left of the knobs.  Right-cluster the knobs
        // first, then place the chickenhead immediately left of them.
        auto remainingLeft = rightClusterKnobs (b, knobs);
        if (modeSel)
        {
            auto col = remainingLeft.removeFromRight (66);
            modeSel->setBounds (col.reduced (2));
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// HighGainStylePanel - MT Style High-Gain
// 6 knobs: Level / Dist / High / Low / Mid Hz / Mid dB
// ─────────────────────────────────────────────────────────────────────────────
struct HighGainStylePanel : public EditorPanelBase
{
    HighGainStyleDSP* mDsp { nullptr };

    explicit HighGainStylePanel (HighGainStyleDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&HarmonicLAF::get());
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Dist",   0.f,    1.f,   0.5f,  0.001f, "Distortion amount (cascading hard-clip drive)" },
            { "Low",    -15.f,  15.f,  0.f,   0.1f,   "Low shelf at 80 Hz (dB)" },
            { "Mid Hz", 200.f,  5000.f, 800.f, 1.f,   "Mid peak frequency (Hz)" },
            { "Mid dB", -15.f,  15.f,  0.f,   0.1f,   "Mid peak gain (dB)" },
            { "High",   -15.f,  15.f,  0.f,   0.1f,   "High shelf at 5 kHz (dB)" },
            { "Level",  -24.f,  12.f,  0.f,   0.1f,   "Output level (dB)" },
        });
        knobs[2]->slider.setSkewFactor (0.4);   // log-feel sweep on Mid Hz

        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setDist   ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setLowDb  ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setMidHz  ((float) knobs[2]->slider.getValue()); };
        knobs[3]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setMidDb  ((float) knobs[3]->slider.getValue()); };
        knobs[4]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setHighDb ((float) knobs[4]->slider.getValue()); };
        knobs[5]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setLevel  ((float) knobs[5]->slider.getValue()); };

        if (dsp)
        {
            knobs[0]->slider.setValue (dsp->mDist,   juce::sendNotificationSync);
            knobs[1]->slider.setValue (dsp->mLowDb,  juce::sendNotificationSync);
            knobs[2]->slider.setValue (dsp->mMidHz,  juce::sendNotificationSync);
            knobs[3]->slider.setValue (dsp->mMidDb,  juce::sendNotificationSync);
            knobs[4]->slider.setValue (dsp->mHighDb, juce::sendNotificationSync);
            knobs[5]->slider.setValue (dsp->mLevel,  juce::sendNotificationSync);
        }
    }

    ~HighGainStylePanel() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        HarmonicLAF::paintHammeritePanel (g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): pedal tiles grid instead of right-clustering.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
            return pedalTileGrid (b, knobs);
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);
        // 6 knobs is wider than the other pedals; tighter slot to keep the
        // right-cluster from spilling all the way across the panel.
        rightClusterKnobs (b, knobs, kKnobSz + 4, kKnobSz);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// BassDriverStylePanel - BB Style Bass Driver (5 knobs: Drive/Blend/Low/High/Level)
// ─────────────────────────────────────────────────────────────────────────────
struct BassDriverStylePanel : public EditorPanelBase
{
    BassDriverStyleDSP* mDsp { nullptr };

    explicit BassDriverStylePanel (BassDriverStyleDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&HarmonicLAF::get());
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Drive", 0.f,   1.f,  0.5f, 0.001f, "Drive amount on the Mid + High bands (Low band stays clean)" },
            { "Blend", 0.f,   1.f,  0.7f, 0.001f, "Clean / clipped mix on the Mid+High bands" },
            { "Low",   0.f,   1.f,  0.7f, 0.001f, "Low-band gain (clean VCA punch on the fundamental)" },
            { "High",  0.f,   1.f,  0.7f, 0.001f, "High-band gain (post-clip)" },
            { "Level", -24.f, 12.f, 0.f,  0.1f,   "Output level (dB)" },
        });
        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setDrive ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setBlend ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setLow   ((float) knobs[2]->slider.getValue()); };
        knobs[3]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setHigh  ((float) knobs[3]->slider.getValue()); };
        knobs[4]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setLevel ((float) knobs[4]->slider.getValue()); };

        if (dsp)
        {
            knobs[0]->slider.setValue (dsp->mDrive, juce::sendNotificationSync);
            knobs[1]->slider.setValue (dsp->mBlend, juce::sendNotificationSync);
            knobs[2]->slider.setValue (dsp->mLow,   juce::sendNotificationSync);
            knobs[3]->slider.setValue (dsp->mHigh,  juce::sendNotificationSync);
            knobs[4]->slider.setValue (dsp->mLevel, juce::sendNotificationSync);
        }
    }

    ~BassDriverStylePanel() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        HarmonicLAF::paintHammeritePanel (g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): pedal tiles grid instead of right-clustering.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
            return pedalTileGrid (b, knobs);
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);
        rightClusterKnobs (b, knobs);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// BassOverdriveStylePanel - ODB Style Bass Overdrive
// 5 knobs: Gain / Balance / EQ-Low / EQ-High / Level
// ─────────────────────────────────────────────────────────────────────────────
struct BassOverdriveStylePanel : public EditorPanelBase
{
    BassOverdriveStyleDSP* mDsp { nullptr };

    explicit BassOverdriveStylePanel (BassOverdriveStyleDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&HarmonicLAF::get());
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Gain",    0.f,   1.f,  0.5f, 0.001f, "Drive amount (full-range hard clip)" },
            { "Balance", 0.f,   1.f,  0.5f, 0.001f, "Dry / wet blend (0 = clean, 1 = fully clipped)" },
            { "Low",     -15.f, 15.f, 0.f,  0.1f,   "EQ Low shelf at 100 Hz (post-blend; dB)" },
            { "High",    -15.f, 15.f, 0.f,  0.1f,   "EQ High shelf at 3 kHz (post-blend; dB)" },
            { "Level",   -24.f, 12.f, 0.f,  0.1f,   "Output level (dB)" },
        });
        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setGain    ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setBalance ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setEqLow   ((float) knobs[2]->slider.getValue()); };
        knobs[3]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setEqHigh  ((float) knobs[3]->slider.getValue()); };
        knobs[4]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setLevel   ((float) knobs[4]->slider.getValue()); };

        if (dsp)
        {
            knobs[0]->slider.setValue (dsp->mGain,    juce::sendNotificationSync);
            knobs[1]->slider.setValue (dsp->mBalance, juce::sendNotificationSync);
            knobs[2]->slider.setValue (dsp->mEqLow,   juce::sendNotificationSync);
            knobs[3]->slider.setValue (dsp->mEqHigh,  juce::sendNotificationSync);
            knobs[4]->slider.setValue (dsp->mLevel,   juce::sendNotificationSync);
        }
    }

    ~BassOverdriveStylePanel() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        HarmonicLAF::paintHammeritePanel (g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): pedal tiles grid instead of right-clustering.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
            return pedalTileGrid (b, knobs);
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);
        rightClusterKnobs (b, knobs);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// OctaveStylePanel - OC Style Octave (Polyphonic + Vintage modes)
// 5 knobs (Direct / +1 Oct / -1 Oct / -2 Oct / Range) + Mode chickenhead.
// ─────────────────────────────────────────────────────────────────────────────
struct OctaveStylePanel : public EditorPanelBase
{
    OctaveStyleDSP* mDsp { nullptr };
    std::unique_ptr<ChickenHeadSelector> modeSel;

    explicit OctaveStylePanel (OctaveStyleDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&HarmonicLAF::get());
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Direct", 0.f, 1.f, 1.0f, 0.001f, "Dry signal level (0 = effect only)" },
            { "+1",     0.f, 1.f, 0.0f, 0.001f, "+1 octave shift level" },
            { "-1",     0.f, 1.f, 0.5f, 0.001f, "-1 octave shift level" },
            { "-2",     0.f, 1.f, 0.0f, 0.001f, "-2 octave shift level" },
            { "Range",  0.f, 1.f, 0.6f, 0.001f, "Input LP cutoff (300 Hz - 3 kHz). Lower = better tracking on bass; higher = more harmonic content." },
        });

        modeSel = std::make_unique<ChickenHeadSelector>();
        modeSel->setOptions ({
            { "Po", "Polyphonic", "Granular pitch shifter -- clean octave shifts, works on chords" },
            { "Vi", "Vintage",    "Schmitt-trigger divider + full-wave rectifier -- low-CPU squelchy character" },
        });
        modeSel->setBodyTooltip ("Octave mode");
        modeSel->setSelectedIndex (dsp ? (int) dsp->mMode : 0, juce::dontSendNotification);
        modeSel->onChange = [dsp] (int idx) { if (dsp) dsp->setMode (idx); };
        addAndMakeVisible (*modeSel);

        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setDirectLevel ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setOct1Up      ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setOct1Down    ((float) knobs[2]->slider.getValue()); };
        knobs[3]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setOct2Down    ((float) knobs[3]->slider.getValue()); };
        knobs[4]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setRange       ((float) knobs[4]->slider.getValue()); };

        if (dsp)
        {
            knobs[0]->slider.setValue (dsp->mDirectLevel, juce::sendNotificationSync);
            knobs[1]->slider.setValue (dsp->mOct1Up,      juce::sendNotificationSync);
            knobs[2]->slider.setValue (dsp->mOct1Down,    juce::sendNotificationSync);
            knobs[3]->slider.setValue (dsp->mOct2Down,    juce::sendNotificationSync);
            knobs[4]->slider.setValue (dsp->mRange,       juce::sendNotificationSync);
        }
    }

    ~OctaveStylePanel() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        HarmonicLAF::paintHammeritePanel (g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);

        // QA-OctavePedal: BaySickPedals tile (~190x137 inner) is too narrow for
        // the full-mode right-cluster + 66px mode column -- they collapse.  In
        // pedal mode drop the dBFS column entirely (it's hidden here anyway) and
        // grid the 5 knobs + mode chickenhead 2x3 so all six stay visible +
        // finger-sized.  FurmanEQ's 3x3 pedal grid is the reference.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
        {
            constexpr int kCols = 3, kRows = 2;
            const int cellW = b.getWidth()  / kCols;
            const int cellH = b.getHeight() / kRows;
            auto cell = [&] (int idx)
            {
                return juce::Rectangle<int> (b.getX() + (idx % kCols) * cellW,
                                             b.getY() + (idx / kCols) * cellH,
                                             cellW, cellH);
            };
            const int kSz = juce::jlimit (28, 56, juce::jmin (cellW - 6, cellH - 16));
            for (int i = 0; i < (int) knobs.size(); ++i)   // Direct / +1 / -1 / -2 / Range
                if (knobs[i])
                    knobs[i]->setBounds (cell (i).withSizeKeepingCentre (kSz, kSz + 14));
            if (modeSel)
                modeSel->setBounds (cell ((int) knobs.size()).reduced (3));   // last cell: Mode
            return;
        }

        // Full (FX rack) mode: right-skewed cluster, dBFS column at the edge.
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);
        auto remainingLeft = rightClusterKnobs (b, knobs);
        if (modeSel)
        {
            auto col = remainingLeft.removeFromRight (66);
            modeSel->setBounds (col.reduced (2));
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// NoiseGateStylePanel - NS Style Noise Gate
// 2 knobs (Threshold / Decay) + Mode chickenhead (Reduction / Mute) +
// Source chickenhead (DI / Self).
// ─────────────────────────────────────────────────────────────────────────────
struct NoiseGateStylePanel : public EditorPanelBase
{
    NoiseGateStyleDSP* mDsp { nullptr };
    std::unique_ptr<ChickenHeadSelector> modeSel;
    std::unique_ptr<ChickenHeadSelector> sourceSel;

    explicit NoiseGateStylePanel (NoiseGateStyleDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&DynamicsLAF::get());
        setVolumeKnobVariant (true);
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Threshold", -60.f, 0.f,    -40.f,  0.1f, "Gate opens above this RMS level (dB); closes below" },
            { "Decay",     1.f,   1000.f, 100.f,  1.f,  "Gate close release time (ms)" },
        });
        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setThresholdDb ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setDecayMs     ((float) knobs[1]->slider.getValue()); };

        modeSel = std::make_unique<ChickenHeadSelector>();
        modeSel->setOptions ({
            { "Re", "Reduction", "Below threshold the signal is attenuated to -20 dB (lets some bleed through)" },
            { "Mu", "Mute",      "Below threshold the signal is fully muted (-60 dB floor)" },
        });
        modeSel->setBodyTooltip ("Gate mode");
        modeSel->setDefaultLabelColour (juce::Colours::black);
        modeSel->setSelectedIndex (dsp ? (int) dsp->mMode : 0, juce::dontSendNotification);
        modeSel->onChange = [dsp] (int idx) { if (dsp) dsp->setMode (idx); };
        addAndMakeVisible (*modeSel);

        sourceSel = std::make_unique<ChickenHeadSelector>();
        sourceSel->setOptions ({
            { "DI", "DI Sidechain", "Detect from the strip's sidechain bus (clean pre-drive tap; preferred when this gate sits after distortion)" },
            { "Sf", "Self",         "Detect from this gate's own input" },
        });
        sourceSel->setBodyTooltip ("Detector source");
        sourceSel->setDefaultLabelColour (juce::Colours::black);
        sourceSel->setSelectedIndex (dsp ? (int) dsp->mSource : 0, juce::dontSendNotification);
        sourceSel->onChange = [dsp] (int idx) { if (dsp) dsp->setSource (idx); };
        addAndMakeVisible (*sourceSel);

        if (dsp)
        {
            knobs[0]->slider.setValue (dsp->mThresholdDb, juce::sendNotificationSync);
            knobs[1]->slider.setValue (dsp->mDecayMs,     juce::sendNotificationSync);
        }
    }

    ~NoiseGateStylePanel() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        DynamicsLAF::paintLA2APanel (g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): pedal tiles grid instead of right-clustering.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
            return pedalTileGrid (b, knobs, { modeSel ? &*modeSel : nullptr,
                                              sourceSel ? &*sourceSel : nullptr });
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);
        auto remainingLeft = rightClusterKnobs (b, knobs);
        if (modeSel)
        {
            auto col = remainingLeft.removeFromRight (66);
            modeSel->setBounds (col.reduced (2));
        }
        remainingLeft.removeFromRight (4);
        if (sourceSel)
        {
            auto col = remainingLeft.removeFromRight (66);
            sourceSel->setBounds (col.reduced (2));
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// BassCompressorStylePanel - BC Style Bass Compressor (BOSS BC-1X)
// 4 knobs (Thresh / Ratio / Release / Level) + LED GR meter
// ─────────────────────────────────────────────────────────────────────────────
struct BassCompressorStylePanel : public EditorPanelBase, public juce::Timer
{
    BassCompressorStyleDSP* mDsp { nullptr };
    std::unique_ptr<GRMeter> grMeter;

    void disableGrMeter() override
    {
        if (grMeter) { removeChildComponent (grMeter.get()); grMeter.reset(); }
    }

    explicit BassCompressorStylePanel (BassCompressorStyleDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&DynamicsLAF::get());
        setVolumeKnobVariant (true);
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Thresh",  -48.f, 0.f,    -24.f, 0.5f,   "Threshold (dB) -- compression begins above this level" },
            { "Ratio",   1.f,   10.f,   4.f,   0.1f,   "Per-band ratio (applied directly to all 3 bands)" },
            { "Release", 50.f,  500.f,  200.f, 1.f,    "Per-band release time (ms)" },
            { "Level",   -24.f, 12.f,   0.f,   0.1f,   "Output level (dB)" },
        });
        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setThresholdDb ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setRatio     ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setReleaseMs ((float) knobs[2]->slider.getValue()); };
        knobs[3]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setLevel     ((float) knobs[3]->slider.getValue()); };

        if (dsp)
        {
            knobs[0]->slider.setValue (dsp->mThresholdDb, juce::sendNotificationSync);
            knobs[1]->slider.setValue (dsp->mRatio,    juce::sendNotificationSync);
            knobs[2]->slider.setValue (dsp->mReleaseMs, juce::sendNotificationSync);
            knobs[3]->slider.setValue (dsp->mLevel,    juce::sendNotificationSync);
        }

        grMeter = std::make_unique<GRMeter>();
        addAndMakeVisible (*grMeter);
        startTimerHz (30);
    }

    ~BassCompressorStylePanel() override { stopTimer(); setLookAndFeel (nullptr); }

    void timerCallback() override
    {
        // QA-Layout T13 crash fix: liveDsp() returns mDsp ONLY while the rack
        // still hands back that same pointer for this slot, so this one guard
        // makes every mDsp-> below safe.  A panel timer and its window's rebuild
        // poll are independent; without this the timer can tick on a DSP a
        // project load already destroyed.  See EditorPanelBase::liveDsp.
        if (liveDsp() == nullptr) return;
        if (grMeter && mDsp) grMeter->setGainReduction (mDsp->getGainReductionDb());
    }

    void paint (juce::Graphics& g) override
    {
        DynamicsLAF::paintLA2APanel (g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): pedal tiles grid; the GR meter takes a cell.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
            return pedalTileGrid (b, knobs, { grMeter ? &*grMeter : nullptr });
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);
        auto remainingLeft = rightClusterKnobs (b, knobs);
        if (grMeter)
        {
            int gw = juce::jmin (96, remainingLeft.getHeight() + 8);
            auto col = remainingLeft.removeFromRight (gw);
            grMeter->setBounds (col.reduced (1, 2));
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// SynthStylePanel - SY Style Polyphonic Synth
// 6 knobs (Variation / Tone / Rate / Depth / Effect / Direct) + Type chickenhead
// ─────────────────────────────────────────────────────────────────────────────
// QA-EffectsReview Task 4: SY-1 TYPE-knob display order -> preset-stable enum value
// (the original 4 kept enum values 0..3; new types took 4..10, so display != value).
static constexpr int kSynthTypeOrder[SynthStyleDSP::kNumTypes] =
    { 0, 4, 1, 2, 5, 6, 7, 8, 9, 3, 10 };   // Lead1 Lead2 Pad Bass Str Organ Bell Sfx1 Sfx2 Seq1 Seq2
static int synthTypeValueToDisplay (int v)
{
    for (int i = 0; i < SynthStyleDSP::kNumTypes; ++i) if (kSynthTypeOrder[i] == v) return i;
    return 0;
}

struct SynthStylePanel : public EditorPanelBase
{
    SynthStyleDSP* mDsp { nullptr };
    std::unique_ptr<ChickenHeadSelector> typeSel;
    std::unique_ptr<DualLabelToggle>     polyTog;   // Mono / Poly
    std::unique_ptr<DualLabelToggle>     instTog;   // Guitar / Bass

    explicit SynthStylePanel (SynthStyleDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&ModulationLAF::get());
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Var",    1.f, 11.f, 1.f,  1.f,    "Variation (sub-character within the Type)" },
            { "Tone",   0.f, 1.f,  0.6f, 0.001f, "Synth voice tone (filter cutoff)" },
            { "Rate",   0.f, 1.f,  0.3f, 0.001f, "LFO rate (modulates filter on Lead/Pad, amplitude on Seq)" },
            { "Depth",  0.f, 1.f,  0.4f, 0.001f, "LFO depth" },
            { "Effect", 0.f, 1.f,  0.5f, 0.001f, "Synth voice level" },
            { "Direct", 0.f, 1.f,  1.0f, 0.001f, "Dry signal level" },
        });

        typeSel = std::make_unique<ChickenHeadSelector>();
        typeSel->setOptions ({   // displayed in the SY-1 TYPE-knob order; 2-char marks (full name in hover)
            { "L1", "Lead 1", "Lead voice -- fast envelope follow + filter LFO" },
            { "L2", "Lead 2", "Edgier square lead -- brighter, more cut" },
            { "Pd", "Pad",    "Sine/saw pad -- slow attack + gentle filter LFO" },
            { "Bs", "Bass",   "Square -1 oct bass voice -- fast envelope" },
            { "St", "Str",    "Saw string ensemble -- slow swell" },
            { "Or", "Organ",  "Sine organ -- rotary-style amplitude LFO" },
            { "Bl", "Bell",   "Bright bell -- fast metallic decay, +1 oct" },
            { "F1", "SFX 1",  "Synth FX voice" },
            { "F2", "SFX 2",  "Animated synth FX voice" },
            { "Q1", "Seq 1",  "Rhythmic filter sweep" },
            { "Q2", "Seq 2",  "LFO-stuttered amplitude (rhythmic)" },
        });
        typeSel->setBodyTooltip ("Voice type -- 11 synth voice categories");
        typeSel->setSelectedIndex (dsp ? synthTypeValueToDisplay ((int) dsp->mType) : 0, juce::dontSendNotification);
        typeSel->onChange = [dsp] (int idx) {
            if (dsp) dsp->setType (kSynthTypeOrder[juce::jlimit (0, SynthStyleDSP::kNumTypes - 1, idx)]); };
        addAndMakeVisible (*typeSel);

        // QA-EffectsReview Task 4: Mono/Poly + Guitar/Bass switches.
        polyTog = std::make_unique<DualLabelToggle>();
        polyTog->setupNamed ("Mono", "Monophonic YIN tracking -- one voice follows the dominant note (tight on single lines + bass)",
                             "Poly", "Polyphonic FFT tracking -- up to 6 voices follow a chord (best on sustained, cleanly-played chords)");
        polyTog->btn().setToggleState (dsp && dsp->mPolyMode, juce::dontSendNotification);
        polyTog->btn().onClick = [dsp, this] { if (dsp) dsp->setPolyMode (polyTog->btn().getToggleState()); };
        polyTog->setLabelColour (VC::Text);
        addAndMakeVisible (*polyTog);

        instTog = std::make_unique<DualLabelToggle>();
        instTog->setupNamed ("Gtr",  "Guitar range -- tracks ~70 Hz..1.3 kHz, full polyphony",
                             "Bass", "Bass range -- tracks ~35..400 Hz, polyphony capped (bass is usually single-note)");
        instTog->btn().setToggleState (dsp && dsp->mInstrument == SynthStyleDSP::Instrument::Bass, juce::dontSendNotification);
        instTog->btn().onClick = [dsp, this] { if (dsp) dsp->setInstrument (instTog->btn().getToggleState() ? 1 : 0); };
        instTog->setLabelColour (VC::Text);
        addAndMakeVisible (*instTog);

        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setVariation   ((int)   knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setTone        ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setRate        ((float) knobs[2]->slider.getValue()); };
        knobs[3]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setDepth       ((float) knobs[3]->slider.getValue()); };
        knobs[4]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setEffectLevel ((float) knobs[4]->slider.getValue()); };
        knobs[5]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setDirectLevel ((float) knobs[5]->slider.getValue()); };

        if (dsp)
        {
            knobs[0]->slider.setValue ((double) dsp->mVariation,  juce::sendNotificationSync);
            knobs[1]->slider.setValue (dsp->mTone,        juce::sendNotificationSync);
            knobs[2]->slider.setValue (dsp->mRate,        juce::sendNotificationSync);
            knobs[3]->slider.setValue (dsp->mDepth,       juce::sendNotificationSync);
            knobs[4]->slider.setValue (dsp->mEffectLevel, juce::sendNotificationSync);
            knobs[5]->slider.setValue (dsp->mDirectLevel, juce::sendNotificationSync);
        }
    }

    ~SynthStylePanel() override { setLookAndFeel (nullptr); }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): pedal tiles grid -- selector + toggles take cells.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
            return pedalTileGrid (b, knobs, { typeSel ? &*typeSel : nullptr,
                                              polyTog ? &*polyTog : nullptr,
                                              instTog ? &*instTog : nullptr });
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);

        // QA-EffectsReview Task 4: Type selector in its own full-width row ABOVE
        // the knobs (centred square) so the 11-letter chicken-head bezel has room,
        // instead of overlapping in a cramped 66 px side column.
        {
            const int typeH = juce::jlimit (64, 84, b.getHeight() - 60);
            auto topRow = b.removeFromTop (typeH);
            // Mono/Poly on the left, Gtr/Bass on the right; chicken-head centred between.
            const int togW = 62;
            const int togH = juce::jmin (topRow.getHeight() - 2, 74);   // room for top label + switch + bottom label
            if (polyTog) polyTog->setBounds (topRow.removeFromLeft  (togW).withSizeKeepingCentre (togW, togH));
            if (instTog) instTog->setBounds (topRow.removeFromRight (togW).withSizeKeepingCentre (togW, togH));
            if (typeSel)
            {
                const int sz = juce::jmin (topRow.getWidth(), topRow.getHeight());
                typeSel->setBounds (topRow.withSizeKeepingCentre (sz, sz));
            }
        }

        layoutKnobsH (b, knobs);   // the 6 knobs fill the row below
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// WahStylePanel - PW Style Wah
// 1 knob (Pedal Position) + Mode chickenhead (Vintage / Rich)
// ─────────────────────────────────────────────────────────────────────────────
struct WahStylePanel : public EditorPanelBase
{
    WahStyleDSP* mDsp { nullptr };
    std::unique_ptr<ChickenHeadSelector> modeSel;

    explicit WahStylePanel (WahStyleDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&ModulationLAF::get());
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Pedal", 0.f, 1.f, 0.5f, 0.001f, "Pedal Position (0 = heel-down / dark, 1 = toe-down / bright). MIDI Learn binds an expression pedal CC; right-click the knob to assign." },
        });
        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setPedalPosition ((float) knobs[0]->slider.getValue()); };

        modeSel = std::make_unique<ChickenHeadSelector>();
        modeSel->setOptions ({
            { "Vi", "Vintage", "Standard wah -- single resonant bandpass swept by Pedal Position" },
            { "Ri", "Rich",    "Vintage wah + parallel ~200 Hz lowpass blended at -6 dB so the bass fundamental survives the bandpass" },
        });
        modeSel->setBodyTooltip ("Wah mode");
        modeSel->setSelectedIndex (dsp ? (int) dsp->mMode : 0, juce::dontSendNotification);
        modeSel->onChange = [dsp] (int idx) { if (dsp) dsp->setMode (idx); };
        addAndMakeVisible (*modeSel);

        if (dsp)
            knobs[0]->slider.setValue (dsp->mPedalPosition, juce::sendNotificationSync);
    }

    ~WahStylePanel() override { setLookAndFeel (nullptr); }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): pedal tiles grid instead of right-clustering.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
            return pedalTileGrid (b, knobs, { modeSel ? &*modeSel : nullptr });
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);
        auto remainingLeft = rightClusterKnobs (b, knobs, kKnobSz + 4, kKnobSz);
        if (modeSel)
        {
            auto col = remainingLeft.removeFromRight (66);
            modeSel->setBounds (col.reduced (2));
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// AcousticPreampStylePanel - AD Style Acoustic Preamp
// 4 knobs (Resonance / Ambience / Notch-with-OFF-at-bottom / Level) + Body
// chickenhead (Dr/Pa/Ju/Us) + "Load IR..." button (active when Body == User).
// All controls always visible in both rack and board views -- no
// Basic/Advanced split, matching the Acoustic Simulator (Jeff, 2026-07-02).
// ─────────────────────────────────────────────────────────────────────────────
struct AcousticPreampStylePanel : public EditorPanelBase
{
    AcousticPreampStyleDSP* mDsp { nullptr };
    std::unique_ptr<ChickenHeadSelector> bodySel;
    std::unique_ptr<juce::TextButton>    loadBtn;
    std::unique_ptr<juce::FileChooser>   chooser;

    explicit AcousticPreampStylePanel (AcousticPreampStyleDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&TimeLAF::get());
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Resonance", 0.f,   1.f,    0.5f,  0.001f, "Acoustic resonance amount - adaptive: analyzes your playing and breathes the body resonance with level and pick attacks; this knob is the ceiling of that depth.  In User body mode: IR wet/dry mix." },
            { "Ambience",  0.f,   1.f,    0.2f,  0.001f, "Schroeder reverberator wet/dry mix" },
            { "Notch",     45.f, 1000.f,  45.f,  1.0f,   "Feedback-rejection notch.  Bottom = OFF (the normal position); raise to sweep 50 Hz - 1 kHz, Q ~10.  Turn until the feedback rumble disappears." },
            { "Level",    -24.f,  12.f,   0.f,   0.1f,   "Output level (dB)" },
        });
        knobs[2]->slider.setSkewFactorFromMidPoint (224.0); // log feel for 50..1000 Hz
        knobs[2]->slider.textFromValueFunction = [] (double v)
        { return v < 50.0 ? juce::String ("OFF") : juce::String ((int) v) + " Hz"; };
        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setResonance ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setAmbience  ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this]
        {
            if (! dsp) return;
            const float v = (float) knobs[2]->slider.getValue();
            if (v < 50.0f) dsp->setNotchOn (false);          // knob bottom = OFF
            else { dsp->setNotchOn (true); dsp->setNotchHz (v); }
        };
        knobs[3]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setLevelDb   ((float) knobs[3]->slider.getValue()); };

        bodySel = std::make_unique<ChickenHeadSelector>();
        bodySel->setOptions ({
            { "Dr", "Dreadnought", "Balanced warm punch - low lift, relaxed low-mids, breathing air/top/body resonances" },
            { "Pa", "Parlor",      "Small tight box - lows cut, boxy upper-mid bump, higher resonances" },
            { "Ju", "Jumbo",       "Biggest body - strong low bloom, scooped low-mids, deep wide resonances" },
            { "Us", "User",        "Custom impulse response loaded from disk via the Load IR button (static - no adaptive layer)" },
        });
        bodySel->setBodyTooltip ("Body type");
        bodySel->setSelectedIndex (dsp ? (int) dsp->mBody : 0, juce::dontSendNotification);
        bodySel->onChange = [this, dsp] (int idx)
        {
            if (dsp) dsp->setBody (idx);
            updateLoadBtnState();
        };
        addAndMakeVisible (*bodySel);

        loadBtn = std::make_unique<juce::TextButton> ("Load IR...");
        loadBtn->setTooltip ("Load a custom impulse response (.wav) for the User body type");
        loadBtn->onClick = [this]
        {
            if (! mDsp) return;
            auto irHome = EffectPresetIO::irDir();
            irHome.createDirectory();
            // The stored ref is what was persisted -- a bundled project holds
            // "Samples/<name>.wav", which a bare juce::File would resolve
            // against the process working directory.
            auto startDir = mDsp->getUserIRPath().isNotEmpty()
                              ? ProjectFileResolver::resolve (mDsp->getUserIRPath()).getParentDirectory()
                              : irHome;
            chooser = std::make_unique<juce::FileChooser> ("Pick acoustic IR", startDir, "*.wav");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this] (const juce::FileChooser& fc)
                {
                    auto f = fc.getResult();
                    if (f == juce::File() || ! mDsp) return;
                    juce::String err;
                    if (! mDsp->loadUserIR (f, err))
                    {
                        juce::AlertWindow::showMessageBoxAsync (
                            juce::MessageBoxIconType::WarningIcon,
                            "Load Acoustic IR", err, "OK");
                        return;
                    }
                    if (mDsp->mBody != AcousticPreampStyleDSP::Body::User)
                    {
                        mDsp->setBody ((int) AcousticPreampStyleDSP::Body::User);
                        if (bodySel) bodySel->setSelectedIndex ((int) AcousticPreampStyleDSP::Body::User, juce::dontSendNotification);
                    }
                });
        };
        addAndMakeVisible (*loadBtn);

        if (dsp)
        {
            knobs[0]->slider.setValue (dsp->mResonance01, juce::sendNotificationSync);
            knobs[1]->slider.setValue (dsp->mAmbience01,  juce::sendNotificationSync);
            knobs[2]->slider.setValue (dsp->getNotchOn() ? dsp->mNotchHz : 45.0f,
                                       juce::sendNotificationSync);
            knobs[3]->slider.setValue (dsp->mLevelDb,     juce::sendNotificationSync);
        }
        updateLoadBtnState();
    }

    ~AcousticPreampStylePanel() override { setLookAndFeel (nullptr); }

    void updateLoadBtnState()
    {
        if (! loadBtn) return;
        const bool isUser = mDsp && mDsp->mBody == AcousticPreampStyleDSP::Body::User;
        loadBtn->setAlpha (isUser ? 1.0f : 0.55f);
    }

    void paint (juce::Graphics& g) override
    {
        TimeLAF::paintPultecPanel (g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): pedal tiles grid instead of right-clustering.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
            return pedalTileGrid (b, knobs, { bodySel ? &*bodySel : nullptr,
                                              loadBtn ? &*loadBtn : nullptr });
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);
        auto remainingLeft = rightClusterKnobs (b, knobs);
        if (bodySel)
        {
            auto col = remainingLeft.removeFromRight (70);
            auto load = col.removeFromBottom (20);
            if (loadBtn) loadBtn->setBounds (load.reduced (2, 1));
            bodySel->setBounds (col.reduced (2));
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// AcousticSimulatorStylePanel - AC Style Acoustic Simulator
// 4 knobs (Top / Body / Reverb / Level) + Mode chickenhead (St/Ju/En/Pi/Us) +
// "Load IR..." button (active when Mode == User).
// ─────────────────────────────────────────────────────────────────────────────
struct AcousticSimulatorStylePanel : public EditorPanelBase
{
    AcousticSimulatorStyleDSP* mDsp { nullptr };
    std::unique_ptr<ChickenHeadSelector> modeSel;
    std::unique_ptr<juce::TextButton>    loadBtn;
    std::unique_ptr<juce::FileChooser>   chooser;

    explicit AcousticSimulatorStylePanel (AcousticSimulatorStyleDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&ModulationLAF::get());
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Top",    -15.f, 15.f, 0.f, 0.1f,   "Transient pick attack (HF shelf >4 kHz, modulated by envelope follower). +15 emphasizes pick clack; -15 ducks attack." },
            { "Body",   -15.f, 15.f, 0.f, 0.1f,   "Acoustic body depth - bottom = off, center = moderate, top = full voice (bundled body capture + the selected mode's voicing). In User mode: your IR's wet/dry mix." },
            { "Reverb",   0.f,  1.f, 0.2f, 0.001f,"Schroeder reverb wet/dry mix" },
            { "Level",  -24.f, 12.f, 0.f, 0.1f,   "Output level (dB)" },
        });
        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setTopDb   ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setBodyDb  ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setReverb  ((float) knobs[2]->slider.getValue()); };
        knobs[3]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setLevelDb ((float) knobs[3]->slider.getValue()); };

        modeSel = std::make_unique<ChickenHeadSelector>();
        modeSel->setOptions ({
            { "St", "Standard", "General acoustic emulation -- low-mid scoop, flat highs" },
            { "Ju", "Jumbo",    "Massive low-end shelf boost, slightly scooped mids" },
            { "En", "Enhanced", "High-shelf boost for hi-fi mix-ready tone" },
            { "Pi", "Piezo",    "Pronounced upper-mid peak (~2-3 kHz) -- under-saddle pickup quack" },
            { "Us", "User",     "Custom impulse response loaded from disk via Load IR. Body knob becomes IR wet/dry mix in this mode." },
        });
        modeSel->setBodyTooltip ("Body type / mode");
        modeSel->setSelectedIndex (dsp ? (int) dsp->mMode : 0, juce::dontSendNotification);
        modeSel->onChange = [this, dsp] (int idx)
        {
            if (dsp) dsp->setMode (idx);
            updateLoadBtnState();
        };
        addAndMakeVisible (*modeSel);

        loadBtn = std::make_unique<juce::TextButton> ("Load IR...");
        loadBtn->setTooltip ("Load a custom impulse response (.wav) for User mode");
        loadBtn->onClick = [this]
        {
            if (! mDsp) return;
            auto irHome = EffectPresetIO::irDir();
            irHome.createDirectory();
            // The stored ref is what was persisted -- a bundled project holds
            // "Samples/<name>.wav", which a bare juce::File would resolve
            // against the process working directory.
            auto startDir = mDsp->getUserIRPath().isNotEmpty()
                              ? ProjectFileResolver::resolve (mDsp->getUserIRPath()).getParentDirectory()
                              : irHome;
            chooser = std::make_unique<juce::FileChooser> ("Pick acoustic simulator IR", startDir, "*.wav");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this] (const juce::FileChooser& fc)
                {
                    auto f = fc.getResult();
                    if (f == juce::File() || ! mDsp) return;
                    juce::String err;
                    if (! mDsp->loadUserIR (f, err))
                    {
                        juce::AlertWindow::showMessageBoxAsync (
                            juce::MessageBoxIconType::WarningIcon,
                            "Load Acoustic IR", err, "OK");
                        return;
                    }
                    if (mDsp->mMode != AcousticSimulatorStyleDSP::Mode::User)
                    {
                        mDsp->setMode ((int) AcousticSimulatorStyleDSP::Mode::User);
                        if (modeSel) modeSel->setSelectedIndex ((int) AcousticSimulatorStyleDSP::Mode::User, juce::dontSendNotification);
                    }
                    updateLoadBtnState();
                });
        };
        addAndMakeVisible (*loadBtn);

        if (dsp)
        {
            knobs[0]->slider.setValue (dsp->mTopDb,    juce::sendNotificationSync);
            knobs[1]->slider.setValue (dsp->mBodyDb,   juce::sendNotificationSync);
            knobs[2]->slider.setValue (dsp->mReverb01, juce::sendNotificationSync);
            knobs[3]->slider.setValue (dsp->mLevelDb,  juce::sendNotificationSync);
        }
        updateLoadBtnState();
    }

    ~AcousticSimulatorStylePanel() override { setLookAndFeel (nullptr); }

    void updateLoadBtnState()
    {
        if (! loadBtn) return;
        const bool isUser = mDsp && mDsp->mMode == AcousticSimulatorStyleDSP::Mode::User;
        loadBtn->setAlpha (isUser ? 1.0f : 0.55f);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): pedal tiles grid instead of right-clustering.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
            return pedalTileGrid (b, knobs, { modeSel ? &*modeSel : nullptr,
                                              loadBtn ? &*loadBtn : nullptr });
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);
        auto remainingLeft = rightClusterKnobs (b, knobs);
        if (modeSel)
        {
            auto col = remainingLeft.removeFromRight (70);
            auto load = col.removeFromBottom (20);
            if (loadBtn) loadBtn->setBounds (load.reduced (2, 1));
            modeSel->setBounds (col.reduced (2));
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// EQFader - Self-painted vertical fader for I-12 graphic EQ panels.
// Bypasses TimeLAF/BaySickLAF entirely so we get the locked spec look:
//   * Vertical track with center-detent notch
//   * Rectangular thumb cap with bevel
//   * Snaps to 0 dB within +/-1.5 dB of center
// ─────────────────────────────────────────────────────────────────────────────
class EQFader : public BaySickSlider
{
public:
    EQFader() : BaySickSlider (juce::Slider::LinearVertical, juce::Slider::NoTextBox)
    {
        setDoubleClickReturnValue (true, 0.0);
        // Disable LAF interference -- we paint everything ourselves.
        setColour (juce::Slider::backgroundColourId,    juce::Colours::transparentBlack);
        setColour (juce::Slider::trackColourId,         juce::Colours::transparentBlack);
        setColour (juce::Slider::thumbColourId,         juce::Colours::transparentBlack);
        setColour (juce::Slider::rotarySliderFillColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::transparentBlack);
    }

    double snapValue (double v, DragMode) override
    {
        return (std::abs (v) < 1.5) ? 0.0 : v;
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (2.0f);
        if (b.isEmpty()) return;

        const float cx = b.getCentreX();

        // ── Track (dark inset rounded rectangle down the centre) ───────────
        const float trackW = 4.0f;
        juce::Rectangle<float> track (cx - trackW * 0.5f, b.getY(), trackW, b.getHeight());
        g.setColour (juce::Colour (0xff181818));
        g.fillRoundedRectangle (track, 2.0f);
        g.setColour (juce::Colour (0xff080808));
        g.drawRoundedRectangle (track, 2.0f, 1.0f);

        // ── Center-detent notch ─────────────────────────────────────────────
        const float midY = b.getY() + b.getHeight() * 0.5f;
        g.setColour (juce::Colour (0xffaaaaaa));
        g.fillRect (cx - 8.0f, midY - 0.75f, 16.0f, 1.5f);

        // ── Compute cap position from current value ─────────────────────────
        const auto range = getRange();
        const double rangeLen = range.getLength();
        if (rangeLen <= 0.0) return;

        // Apply the slider's skew so the cap visually matches the value.
        const double v01 = valueToProportionOfLength (getValue());
        // NaN survives every Slider clamp (jlimit passes it) and would trip
        // JUCE's coordinate assert inside fillRect.  Skip the cap and
        // Debug-assert here with the slider inspectable.
        if (! std::isfinite (v01)) { jassertfalse; return; }
        const float capY = b.getBottom() - (float) v01 * b.getHeight();

        // ── Rectangular cap (the fader thumb) ───────────────────────────────
        // JUCE 8's coordsToRectangle Debug-asserts on NEGATIVE fillRect sizes
        // (JUCE 7 silently drew nothing).  A pedals-tile fader is ~10px wide,
        // which made the inset stripe below -2px and crashed every Debug open
        // of a pedal board holding a graphic EQ (2026-08-01).
        const float capW = juce::jmin (b.getWidth() - 4.0f, 22.0f);
        if (capW <= 0.0f) return;
        const float capH = 14.0f;
        juce::Rectangle<float> cap (cx - capW * 0.5f, capY - capH * 0.5f, capW, capH);

        // Cap fill (light grey with a subtle vertical gradient for body).
        juce::ColourGradient grad (juce::Colour (0xffe6e6e6), cap.getX(), cap.getY(),
                                    juce::Colour (0xffb0b0b0), cap.getX(), cap.getBottom(),
                                    false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (cap, 2.0f);

        // Cap outline.
        g.setColour (juce::Colour (0xff303030));
        g.drawRoundedRectangle (cap, 2.0f, 1.0f);

        // Cap centre indicator stripe (the actual value reference line on the cap).
        const float stripeW = cap.getWidth() - 4.0f;
        if (stripeW > 0.0f)
        {
            g.setColour (juce::Colour (0xff202020));
            g.fillRect (cap.getX() + 2.0f, cap.getCentreY() - 0.75f,
                        stripeW, 1.5f);
        }
    }
};

// Seed guard for the fader ctor reads below: the EQ DSPs' setters reject
// non-finite input, so a non-finite READ here means the DSP object itself was
// corrupted in RAM -- Debug-assert at the bind, seed 0 in Release.
static double finiteOr0 (float v)
{
    if (std::isfinite (v)) return (double) v;
    jassertfalse;
    return 0.0;
}

// ─────────────────────────────────────────────────────────────────────────────
// GraphicEQStylePanel - GE Style 7-band guitar graphic EQ
// 7 vertical faders (each band) + 1 vertical fader (master Level).
// All faders snap to 0 dB at center detent; rectangular caps painted by EQFader.
// ─────────────────────────────────────────────────────────────────────────────
struct GraphicEQStylePanel : public EditorPanelBase
{
    GraphicEQStyleDSP* mDsp { nullptr };
    std::array<std::unique_ptr<EQFader>,    GraphicEQStyleDSP::kNumBands + 1> faders;
    std::array<std::unique_ptr<juce::Label>, GraphicEQStyleDSP::kNumBands + 1> labels;

    explicit GraphicEQStylePanel (GraphicEQStyleDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&TimeLAF::get());
        disableVU();
        disableOutputVolKnob();

        for (int i = 0; i < GraphicEQStyleDSP::kNumBands; ++i)
        {
            auto f = std::make_unique<EQFader>();
            f->setRange (-15.0, 15.0, 0.1);
            f->setValue (dsp ? finiteOr0 (dsp->getBandDb (i)) : 0.0, juce::dontSendNotification);
            f->onValueChange = [this, i] { if (mDsp) mDsp->setBandDb (i, (float) faders[i]->getValue()); };
            f->setTooltip (juce::String ((int) GraphicEQStyleDSP::kFreqs[i]) + " Hz - +/-15 dB "
                           + (i == GraphicEQStyleDSP::kNumBands - 1 ? "high-shelf band" : "peaking band"));
            addAndMakeVisible (*f);
            faders[i] = std::move (f);

            auto l = std::make_unique<juce::Label>();
            l->setJustificationType (juce::Justification::centred);
            const float hz = GraphicEQStyleDSP::kFreqs[i];
            l->setText (hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + "k" : juce::String ((int) hz),
                        juce::dontSendNotification);
            l->setFont (juce::Font (10.0f));
            l->setColour (juce::Label::textColourId, VC::Text);
            addAndMakeVisible (*l);
            labels[i] = std::move (l);
        }

        // Master Level fader (+/-15 dB, matching the GE-7 Level slider).
        {
            auto f = std::make_unique<EQFader>();
            f->setRange (-15.0, 15.0, 0.1);
            f->setSkewFactorFromMidPoint (0.0);
            f->setValue (dsp ? finiteOr0 (dsp->getLevelDb()) : 0.0, juce::dontSendNotification);
            f->onValueChange = [this] { if (mDsp) mDsp->setLevelDb ((float) faders[GraphicEQStyleDSP::kNumBands]->getValue()); };
            f->setTooltip ("Master output level (+/-15 dB).  Center detent at 0 dB.");
            addAndMakeVisible (*f);
            faders[GraphicEQStyleDSP::kNumBands] = std::move (f);

            auto l = std::make_unique<juce::Label>();
            l->setJustificationType (juce::Justification::centred);
            l->setText ("Lvl", juce::dontSendNotification);
            l->setFont (juce::Font (10.0f));
            l->setColour (juce::Label::textColourId, VC::Text);
            addAndMakeVisible (*l);
            labels[GraphicEQStyleDSP::kNumBands] = std::move (l);
        }
    }

    ~GraphicEQStylePanel() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        TimeLAF::paintPultecPanel (g, getLocalBounds());

        // Center detent line across all faders (0 dB reference).
        auto b = getLocalBounds().reduced (4, 4);
        b.removeFromRight (32 + 4);   // dBFS meter + spacing
        const int labelH = 14;
        b.removeFromBottom (labelH);

        const int total = GraphicEQStyleDSP::kNumBands + 1;
        const int colW = b.getWidth() / total;
        g.setColour (VC::Text.withAlpha (0.25f));
        for (int i = 0; i < total; ++i)
        {
            auto col = b.withX (b.getX() + i * colW).withWidth (colW).reduced (4, 0);
            const float midY = (float) col.getY() + (float) col.getHeight() * 0.5f;
            g.drawLine ((float) col.getX(), midY, (float) col.getRight(), midY, 1.0f);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): in a pedal tile the fader bank keeps its column
        // layout but reclaims the dBFS strip (hidden on the board anyway).
        if (mPanelMode != EditorPanelBase::PanelMode::Pedal)
        {
            dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
            b.removeFromRight (4);
        }

        const int labelH = 14;
        auto labelRow = b.removeFromBottom (labelH);

        const int total = GraphicEQStyleDSP::kNumBands + 1;
        const int colW = b.getWidth() / total;
        for (int i = 0; i < total; ++i)
        {
            auto col      = b.withX (b.getX() + i * colW).withWidth (colW);
            auto labelCol = labelRow.withX (labelRow.getX() + i * colW).withWidth (colW);
            if (faders[i]) faders[i]->setBounds (col.reduced (4, 2));
            if (labels[i]) labels[i]->setBounds (labelCol);
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// BassGraphicEQStylePanel - GEB Style 7-band bass graphic EQ
// Mirrors GE panel; bass-tuned freq labels.
// ─────────────────────────────────────────────────────────────────────────────
struct BassGraphicEQStylePanel : public EditorPanelBase
{
    BassGraphicEQStyleDSP* mDsp { nullptr };
    std::array<std::unique_ptr<EQFader>,    BassGraphicEQStyleDSP::kNumBands + 1> faders;
    std::array<std::unique_ptr<juce::Label>, BassGraphicEQStyleDSP::kNumBands + 1> labels;

    explicit BassGraphicEQStylePanel (BassGraphicEQStyleDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&TimeLAF::get());
        disableVU();
        disableOutputVolKnob();

        for (int i = 0; i < BassGraphicEQStyleDSP::kNumBands; ++i)
        {
            auto f = std::make_unique<EQFader>();
            f->setRange (-15.0, 15.0, 0.1);
            f->setValue (dsp ? finiteOr0 (dsp->getBandDb (i)) : 0.0, juce::dontSendNotification);
            f->onValueChange = [this, i] { if (mDsp) mDsp->setBandDb (i, (float) faders[i]->getValue()); };
            f->setTooltip (juce::String ((int) BassGraphicEQStyleDSP::kFreqs[i]) + " Hz - +/-15 dB peaking band");
            addAndMakeVisible (*f);
            faders[i] = std::move (f);

            auto l = std::make_unique<juce::Label>();
            l->setJustificationType (juce::Justification::centred);
            const float hz = BassGraphicEQStyleDSP::kFreqs[i];
            l->setText (hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + "k" : juce::String ((int) hz),
                        juce::dontSendNotification);
            l->setFont (juce::Font (10.0f));
            l->setColour (juce::Label::textColourId, VC::Text);
            addAndMakeVisible (*l);
            labels[i] = std::move (l);
        }

        {
            auto f = std::make_unique<EQFader>();
            f->setRange (-15.0, 15.0, 0.1);
            f->setSkewFactorFromMidPoint (0.0);   // 0 dB sits at visual midpoint
            f->setValue (dsp ? finiteOr0 (dsp->getLevelDb()) : 0.0, juce::dontSendNotification);
            f->onValueChange = [this] { if (mDsp) mDsp->setLevelDb ((float) faders[BassGraphicEQStyleDSP::kNumBands]->getValue()); };
            f->setTooltip ("Master output level (+/-15 dB).  Center detent at 0 dB.");
            addAndMakeVisible (*f);
            faders[BassGraphicEQStyleDSP::kNumBands] = std::move (f);

            auto l = std::make_unique<juce::Label>();
            l->setJustificationType (juce::Justification::centred);
            l->setText ("Lvl", juce::dontSendNotification);
            l->setFont (juce::Font (10.0f));
            l->setColour (juce::Label::textColourId, VC::Text);
            addAndMakeVisible (*l);
            labels[BassGraphicEQStyleDSP::kNumBands] = std::move (l);
        }
    }

    ~BassGraphicEQStylePanel() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        TimeLAF::paintPultecPanel (g, getLocalBounds());

        auto b = getLocalBounds().reduced (4, 4);
        b.removeFromRight (32 + 4);
        const int labelH = 14;
        b.removeFromBottom (labelH);

        const int total = BassGraphicEQStyleDSP::kNumBands + 1;
        const int colW = b.getWidth() / total;
        g.setColour (VC::Text.withAlpha (0.25f));
        for (int i = 0; i < total; ++i)
        {
            auto col = b.withX (b.getX() + i * colW).withWidth (colW).reduced (4, 0);
            const float midY = (float) col.getY() + (float) col.getHeight() * 0.5f;
            g.drawLine ((float) col.getX(), midY, (float) col.getRight(), midY, 1.0f);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): in a pedal tile the fader bank keeps its column
        // layout but reclaims the dBFS strip (hidden on the board anyway).
        if (mPanelMode != EditorPanelBase::PanelMode::Pedal)
        {
            dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
            b.removeFromRight (4);
        }

        const int labelH = 14;
        auto labelRow = b.removeFromBottom (labelH);

        const int total = BassGraphicEQStyleDSP::kNumBands + 1;
        const int colW = b.getWidth() / total;
        for (int i = 0; i < total; ++i)
        {
            auto col      = b.withX (b.getX() + i * colW).withWidth (colW);
            auto labelCol = labelRow.withX (labelRow.getX() + i * colW).withWidth (colW);
            if (faders[i]) faders[i]->setBounds (col.reduced (4, 2));
            if (labels[i]) labels[i]->setBounds (labelCol);
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// FurmanEQStylePanel - EQFH Style Pro Parametric EQ (Furman PQ-3)
// Per band (Low / Mid / High): Freq knob + Boost slider + Q knob.
// Plus master Input Volume knob + Bypass toggle.
// ─────────────────────────────────────────────────────────────────────────────
struct FurmanEQStylePanel : public EditorPanelBase,
                            private juce::Timer
{
    FurmanEQStyleDSP* mDsp { nullptr };

    struct BandStrip
    {
        std::unique_ptr<VKnob>      freq;
        std::unique_ptr<VKnob>      boost;
        std::unique_ptr<VKnob>      q;
        std::unique_ptr<juce::Label> name;
    };
    std::array<BandStrip, FurmanEQStyleDSP::kNumBands> strips;

    std::unique_ptr<VKnob>            inputVol;
    std::unique_ptr<juce::TextButton> bypassBtn;
    std::unique_ptr<juce::TextButton> hiLoBtn;     // QA-EffectsReview Task 3: Hi/Lo gain range
    bool                 mOverload { false };       // Task 3: preamp overload LED state
    juce::Rectangle<int> mLedBounds;                // Task 3: overload LED rect (set in resized)

    explicit FurmanEQStylePanel (FurmanEQStyleDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&TimeLAF::get());
        disableVU();
        disableOutputVolKnob();

        const char* bandNames[FurmanEQStyleDSP::kNumBands] = { "Low", "Mid", "High" };

        for (int i = 0; i < FurmanEQStyleDSP::kNumBands; ++i)
        {
            auto& s = strips[i];

            s.freq = std::make_unique<VKnob>("Freq",
                dsp ? dsp->getFreq (i) : FurmanEQStyleDSP::kFreqDef[i],
                "Center frequency");
            s.freq->slider.setRange (FurmanEQStyleDSP::kFreqMin[i], FurmanEQStyleDSP::kFreqMax[i], 1.0);
            s.freq->slider.setSkewFactorFromMidPoint
                ((double) std::sqrt (FurmanEQStyleDSP::kFreqMin[i] * FurmanEQStyleDSP::kFreqMax[i]));
            s.freq->slider.setValue (dsp ? dsp->getFreq (i) : FurmanEQStyleDSP::kFreqDef[i],
                                     juce::dontSendNotification);
            s.freq->slider.onValueChange = [this, i] { if (mDsp) mDsp->setFreq (i, (float) strips[i].freq->slider.getValue()); };
            addAndMakeVisible (*s.freq);

            s.boost = std::make_unique<VKnob>("Boost",
                dsp ? dsp->getBoostDb (i) : 0.0f,
                "Boost (-inf..+20 dB).  0 dB at center (knob noon).  Double-click to return to 0 dB.");
            s.boost->slider.setRange (-60.0, 20.0, 0.1);
            // 0 dB at knob noon; range is asymmetric (-60..+20) so skew the slider so 0 is the midpoint.
            s.boost->slider.setSkewFactorFromMidPoint (0.0);
            s.boost->slider.setDoubleClickReturnValue (true, 0.0);
            s.boost->slider.setValue (dsp ? dsp->getBoostDb (i) : 0.0, juce::dontSendNotification);
            s.boost->slider.onValueChange = [this, i] { if (mDsp) mDsp->setBoostDb (i, (float) strips[i].boost->slider.getValue()); };
            addAndMakeVisible (*s.boost);

            s.q = std::make_unique<VKnob>("Q",
                dsp ? dsp->getQ (i) : 0.7f,
                "Bandwidth (Q): low values = wider, high values = narrower");
            s.q->slider.setRange (0.2, 3.8, 0.01);   // QA-EffectsReview Task 3: PQ-3 Q range
            s.q->slider.setSkewFactorFromMidPoint (0.7);
            s.q->slider.setValue (dsp ? dsp->getQ (i) : 0.7, juce::dontSendNotification);
            s.q->slider.onValueChange = [this, i] { if (mDsp) mDsp->setQ (i, (float) strips[i].q->slider.getValue()); };
            addAndMakeVisible (*s.q);

            s.name = std::make_unique<juce::Label>();
            s.name->setText (bandNames[i], juce::dontSendNotification);
            s.name->setJustificationType (juce::Justification::centred);
            s.name->setFont (juce::Font (12.0f, juce::Font::bold));
            s.name->setColour (juce::Label::textColourId, VC::Text);
            addAndMakeVisible (*s.name);
        }

        inputVol = std::make_unique<VKnob>("Input",
            dsp ? dsp->getInputVolDb() : 0.0f,
            "Master Input Volume preamp (0 to +26 dB).  Above ~+12 dB the soft-clipper engages; the overload LED (top-right) lights when it clips.  The full gain range emerges by combining this with the boost bands + the Hi switch.");
        inputVol->slider.setRange (0.0, 26.0, 0.1);
        inputVol->slider.setSkewFactorFromMidPoint (12.0);
        inputVol->slider.setValue (dsp ? dsp->getInputVolDb() : 0.0, juce::dontSendNotification);
        inputVol->slider.onValueChange = [this] { if (mDsp) mDsp->setInputVolDb ((float) inputVol->slider.getValue()); };
        addAndMakeVisible (*inputVol);

        bypassBtn = std::make_unique<juce::TextButton>("EQ Bypass");
        bypassBtn->setClickingTogglesState (true);
        bypassBtn->setTooltip ("EQ Bypass -- bypasses the 3 parametric bands ONLY; preamp saturation stays active");
        bypassBtn->setToggleState (dsp && dsp->isEqBypassed(), juce::dontSendNotification);
        bypassBtn->onClick = [this] { if (mDsp) mDsp->setEqBypassed (bypassBtn->getToggleState()); };
        addAndMakeVisible (*bypassBtn);

        // QA-EffectsReview Task 3: Hi/Lo output gain-range switch (+20 dB in Hi).
        hiLoBtn = std::make_unique<juce::TextButton>();
        hiLoBtn->setClickingTogglesState (true);
        hiLoBtn->setTooltip ("Hi/Lo output gain range -- Hi adds +20 dB");
        const bool hi = (dsp && dsp->getGainRange() == FurmanEQStyleDSP::GainRange::Hi);
        hiLoBtn->setToggleState (hi, juce::dontSendNotification);
        hiLoBtn->setButtonText (hi ? "Hi +20" : "Lo");
        hiLoBtn->onClick = [this]
        {
            const bool on = hiLoBtn->getToggleState();
            if (mDsp) mDsp->setGainRange (on ? FurmanEQStyleDSP::GainRange::Hi
                                             : FurmanEQStyleDSP::GainRange::Lo);
            hiLoBtn->setButtonText (on ? "Hi +20" : "Lo");
        };
        addAndMakeVisible (*hiLoBtn);

        startTimerHz (20);   // poll the preamp overload level for the LED
    }

    ~FurmanEQStylePanel() override { stopTimer(); setLookAndFeel (nullptr); }

    void timerCallback() override
    {
        // QA-Layout T13 crash fix: liveDsp() returns mDsp ONLY while the rack
        // still hands back that same pointer for this slot, so this one guard
        // makes every mDsp-> below safe.  A panel timer and its window's rebuild
        // poll are independent; without this the timer can tick on a DSP a
        // project load already destroyed.  See EditorPanelBase::liveDsp.
        if (liveDsp() == nullptr) return;
        const bool ov = (mDsp && mDsp->getClipLevel() > 1.0f);
        if (ov != mOverload) { mOverload = ov; repaint (mLedBounds); }
    }

    void paint (juce::Graphics& g) override
    {
        TimeLAF::paintPultecPanel (g, getLocalBounds());

        // QA-EffectsReview Task 3: preamp overload LED (+ "OL" micro-label).
        if (! mLedBounds.isEmpty())
        {
            auto led = mLedBounds.toFloat().reduced (1.5f);
            g.setColour (mOverload ? juce::Colour (0xffff3030) : juce::Colour (0xff3a1414));
            g.fillEllipse (led);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawEllipse (led, 1.0f);
            g.setColour (VC::Text.withAlpha (0.75f));
            g.setFont (juce::Font (8.0f));
            g.drawText ("OL", mLedBounds.getX() - 18, mLedBounds.getY(), 16, mLedBounds.getHeight(),
                        juce::Justification::centredRight);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);

        // dBFS column (only in Full mode -- pedal mode strips it via createEffectEditor).
        if (dbfsOut)
        {
            dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
            b.removeFromRight (4);
        }

        // Right cluster: Input Volume knob (top, with overload LED) + Bypass + Hi/Lo.
        const int rightW = kKnobSz + 14;
        auto rightCol = b.removeFromRight (rightW);
        auto inputCell = rightCol.removeFromTop (kKnobSz + 14);
        if (inputVol)
            inputVol->setBounds (inputCell.withSizeKeepingCentre (kKnobSz, kKnobSz + 14));
        mLedBounds = juce::Rectangle<int> (inputCell.getRight() - 13, inputCell.getY() + 1, 12, 12);
        rightCol.removeFromTop (4);
        if (bypassBtn) bypassBtn->setBounds (rightCol.removeFromTop (22).reduced (2, 0));
        rightCol.removeFromTop (3);
        if (hiLoBtn)   hiLoBtn->setBounds (rightCol.removeFromTop (22).reduced (2, 0));

        b.removeFromRight (4);

        // Pedal mode = 3x3 grid (3 bands as rows, 3 knobs per row); the
        // BaySickPedals tile is tall enough to fit it cleanly.  Full mode =
        // single horizontal row of 9 knobs (FX rack panel is short + wide).
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
        {
            // 3x3 grid layout.
            const int rowH = b.getHeight() / FurmanEQStyleDSP::kNumBands;
            for (int i = 0; i < FurmanEQStyleDSP::kNumBands; ++i)
            {
                auto row  = b.removeFromTop (rowH);
                // Band-name label on the left.
                const int labelW = 38;
                auto label = row.removeFromLeft (labelW);
                if (strips[i].name) strips[i].name->setBounds (label);

                // 3 knobs spread across the rest of the row.
                const int colW = row.getWidth() / 3;
                const int kSz  = juce::jmin (colW - 2, juce::jmax (28, rowH - 14));
                auto place = [&] (VKnob* k, int idx)
                {
                    if (! k) return;
                    const int cellX = row.getX() + idx * colW;
                    k->setBounds (juce::Rectangle<int> (cellX, row.getY(), colW, row.getHeight())
                                      .withSizeKeepingCentre (kSz, juce::jmin (row.getHeight(), kSz + 14)));
                };
                place (strips[i].freq.get(),  0);
                place (strips[i].boost.get(), 1);
                place (strips[i].q.get(),     2);
            }
            return;
        }

        // Full mode: single horizontal row.
        const int headerH = 14;
        auto headerRow = b.removeFromTop (headerH);

        const int totalKnobs = FurmanEQStyleDSP::kNumBands * 3;
        const int knobCellW  = b.getWidth() / totalKnobs;
        const int kSz        = juce::jmin (knobCellW - 2, kKnobSz);

        for (int i = 0; i < FurmanEQStyleDSP::kNumBands; ++i)
        {
            const int bandX = b.getX() + i * knobCellW * 3;
            const int bandW = knobCellW * 3;
            if (strips[i].name)
                strips[i].name->setBounds (juce::Rectangle<int> (bandX, headerRow.getY(), bandW, headerH));

            auto place = [&] (VKnob* k, int slot)
            {
                if (! k) return;
                const int cellX = bandX + slot * knobCellW;
                k->setBounds (juce::Rectangle<int> (cellX, b.getY(), knobCellW, b.getHeight())
                                  .withSizeKeepingCentre (kSz, juce::jmin (b.getHeight(), kSz + 14)));
            };
            place (strips[i].freq.get(),  0);
            place (strips[i].boost.get(), 1);
            place (strips[i].q.get(),     2);
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// TunerStylePanel - TU Style Tuner
// Strobe LED display + chromatic note + cents indicator + Mute / 432 toggles
// + Trim knob + Mode chickenhead + display-style switch (Strobe / LED bar).
// 60 fps repaint; reads pitch via wait-free atomics from TunerStyleDSP.
// ─────────────────────────────────────────────────────────────────────────────
struct TunerStylePanel : public EditorPanelBase, private juce::Timer
{
    TunerStyleDSP* mDsp { nullptr };

    enum class DisplayStyle : int { Strobe = 0, LEDBar = 1 };
    DisplayStyle mDisplay { DisplayStyle::LEDBar };

    std::unique_ptr<ChickenHeadSelector> modeSel;
    std::unique_ptr<ChickenHeadSelector> displaySel;
    std::unique_ptr<ChickenHeadSelector> flatSel;      // 0..6 flat/drop tuning
    std::unique_ptr<juce::TextButton>    muteBtn;
    std::unique_ptr<juce::TextButton>    btn432;
    std::unique_ptr<VKnob>               trimKnob;

    // The tuner is a pedalboard pedal, not a rack effect -- the Basic/Advanced
    // slot-header toggle doesn't exist in that context, so a split would strand
    // Trim / 432 / display-style / flat with no way to reveal them.  All
    // controls stay visible; hasAdvancedControls stays at the default (false).

    // Latest cached reading for paint().
    bool                  mHasReading { false };
    TunerStyleDSP::Reading mReading {};

    // Strobe scroll phase (advanced each frame proportional to cents-error).
    float mStrobePhase { 0.0f };

    explicit TunerStylePanel (TunerStyleDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&ModulationLAF::get());
        disableVU();
        disableOutputVolKnob();

        // Trim knob -- range follows the 432-toggle.  We re-set range on toggle.
        trimKnob = std::make_unique<VKnob>("Trim",
            dsp ? dsp->getTrimHz() : 440.0f,
            "Reference Hz fine-trim.  Range is 436-445 Hz in 440 mode, 428-437 Hz in 432 mode.");
        if (dsp)
        {
            float lo = 0.0f, hi = 0.0f;
            dsp->getTrimRange (lo, hi);
            trimKnob->slider.setRange (lo, hi, 0.1);
            trimKnob->slider.setValue (dsp->getTrimHz(), juce::dontSendNotification);
        }
        else
        {
            trimKnob->slider.setRange (436.0, 445.0, 0.1);
            trimKnob->slider.setValue (440.0, juce::dontSendNotification);
        }
        trimKnob->slider.onValueChange = [this] { if (mDsp) mDsp->setTrimHz ((float) trimKnob->slider.getValue()); };
        addAndMakeVisible (*trimKnob);

        modeSel = std::make_unique<ChickenHeadSelector>();
        modeSel->setOptions ({
            { "Ch", "Chromatic", "Detect all 12 notes (no snap)" },
            { "Gt", "Guitar",    "Snap to nearest standard guitar tuning open string (E A D G B E)" },
            { "Bs", "Bass",      "Snap to nearest standard bass tuning open string (E A D G)" },
        });
        modeSel->setBodyTooltip ("Detection mode");
        modeSel->setSelectedIndex (dsp ? (int) dsp->getMode() : 0, juce::dontSendNotification);
        modeSel->onChange = [dsp] (int idx) { if (dsp) dsp->setMode (idx); };
        // QA-ManualPress M-4: the pedalboard figure numbers the whole
        // chicken-head cluster; its dot goes on the first of them.
        modeSel->getProperties().set (kDotAnchor, "BSPDL-9");
        addAndMakeVisible (*modeSel);

        displaySel = std::make_unique<ChickenHeadSelector>();
        displaySel->setOptions ({
            { "St", "Strobe", "Strobe LED -- moving stripes; freeze = in tune, scroll up = sharp, scroll down = flat" },
            { "LB", "LED Bar","LED bar -- horizontal row of LEDs; lit cell shows cents offset, center = in tune" },
        });
        displaySel->setBodyTooltip ("Display style");
        displaySel->setSelectedIndex ((int) mDisplay, juce::dontSendNotification);
        displaySel->onChange = [this] (int idx) { mDisplay = (DisplayStyle) idx; repaint(); };
        addAndMakeVisible (*displaySel);

        muteBtn = std::make_unique<juce::TextButton>("Mute");
        muteBtn->setClickingTogglesState (true);
        muteBtn->setTooltip ("Silence the audio output while tuning");
        muteBtn->setToggleState (dsp && dsp->isMuted(), juce::dontSendNotification);
        muteBtn->onClick = [this] { if (mDsp) mDsp->setMuted (muteBtn->getToggleState()); };
        addAndMakeVisible (*muteBtn);

        btn432 = std::make_unique<juce::TextButton>("432");
        btn432->setClickingTogglesState (true);
        btn432->setTooltip ("Toggle reference between 440 Hz (off) and 432 Hz (on).  Trim range follows.");
        btn432->setToggleState (dsp && dsp->is432(), juce::dontSendNotification);
        btn432->onClick = [this]
        {
            if (! mDsp) return;
            mDsp->set432 (btn432->getToggleState());
            // Update Trim knob range + value to follow the new mode.
            float lo = 0.0f, hi = 0.0f;
            mDsp->getTrimRange (lo, hi);
            trimKnob->slider.setRange (lo, hi, 0.1);
            trimKnob->slider.setValue (mDsp->getTrimHz(), juce::dontSendNotification);
        };
        addAndMakeVisible (*btn432);

        // Flat/drop tuning selector (0 = standard, 1-6 = semitones below).
        flatSel = std::make_unique<ChickenHeadSelector>();
        {
            std::vector<ChickenHeadSelector::Option> opts;
            opts.push_back ({ "0", "Standard", "No flat/drop tuning" });
            for (int i = 1; i <= 6; ++i)
                opts.push_back ({ juce::String (i), "Flat " + juce::String (i),
                                  juce::String (i) + " semitone(s) below standard" });
            flatSel->setOptions (opts);
        }
        flatSel->setBodyTooltip ("Flat / drop tuning (semitones below standard)");
        flatSel->setSelectedIndex (juce::jlimit (0, 6, dsp ? dsp->getFlat() : 0), juce::dontSendNotification);
        flatSel->onChange = [dsp] (int idx) { if (dsp) dsp->setFlat (idx); };
        addAndMakeVisible (*flatSel);

        startTimerHz (60);
    }

    ~TunerStylePanel() override
    {
        stopTimer();
        setLookAndFeel (nullptr);
    }

    void timerCallback() override
    {
        // QA-Layout T13 crash fix: liveDsp() returns mDsp ONLY while the rack
        // still hands back that same pointer for this slot, so this one guard
        // makes every mDsp-> below safe.  A panel timer and its window's rebuild
        // poll are independent; without this the timer can tick on a DSP a
        // project load already destroyed.  See EditorPanelBase::liveDsp.
        if (liveDsp() == nullptr) return;
        if (! mDsp) return;
        TunerStyleDSP::Reading r {};
        const bool ok = mDsp->getReading (r);
        if (ok != mHasReading || (ok && std::abs (r.frequencyHz - mReading.frequencyHz) > 0.01f))
        {
            mHasReading = ok;
            mReading    = r;
        }

        // Advance strobe phase based on cents-error.
        if (mHasReading)
        {
            const float cents = mReading.centsFromTarget;
            // Strobe scroll speed = cents * 0.02 per frame (clamped).
            mStrobePhase += juce::jlimit (-2.0f, 2.0f, cents * 0.02f);
            if (mStrobePhase >= 1.0f) mStrobePhase -= 1.0f;
            else if (mStrobePhase < 0.0f) mStrobePhase += 1.0f;
        }
        repaint();
    }

    static juce::String midiToName (int midi)
    {
        static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        const int oct  = (midi / 12) - 1;
        const int idx  = juce::jlimit (0, 11, midi % 12);
        return juce::String (names[idx]) + juce::String (oct);
    }

    void paint (juce::Graphics& g) override
    {
        // ModulationLAF panels use the default VC::Panel background; paint our own
        // dark display surface inside.
        auto full = getLocalBounds();
        g.fillAll (juce::Colour (0xff0a0a0a));

        auto display = full.reduced (4, 4);
        // Carve out right column for controls (knob + chickenheads + buttons).
        const int rightW = juce::jmin (display.getWidth() / 3 + 30, 230);
        display.removeFromRight (rightW);
        // Reserve dBFS column.
        display.removeFromRight (32 + 4);

        // Display surface bg (sub-component).
        auto disp = display.reduced (4, 4);
        g.setColour (juce::Colour (0xff111111));
        g.fillRoundedRectangle (disp.toFloat(), 4.0f);
        g.setColour (juce::Colour (0xff222222));
        g.drawRoundedRectangle (disp.toFloat(), 4.0f, 1.0f);

        // ── Note name + cents reading text ──────────────────────────────────
        const int textRowH = juce::jmax (24, disp.getHeight() / 3);
        auto textRow = disp.removeFromTop (textRowH);

        if (mHasReading)
        {
            const auto nameStr  = midiToName (mReading.targetNote);
            const float cents   = mReading.centsFromTarget;
            const bool inTune   = std::abs (cents) < 3.0f;

            g.setColour (inTune ? juce::Colour (0xff44ff88) : juce::Colour (0xfff0f0d0));
            g.setFont (juce::Font (24.0f, juce::Font::bold));
            g.drawText (nameStr, textRow.reduced (8, 0), juce::Justification::centredLeft);

            g.setColour (inTune ? juce::Colour (0xff44ff88) : juce::Colour (0xffaaaaaa));
            g.setFont (juce::Font (14.0f));
            const juce::String centsStr = (cents >= 0.0f ? "+" : "") + juce::String ((int) std::round (cents)) + "c";
            g.drawText (centsStr, textRow.reduced (8, 0), juce::Justification::centredRight);

            g.setColour (juce::Colour (0xff666666));
            g.setFont (juce::Font (10.0f));
            g.drawText (juce::String (mReading.frequencyHz, 1) + " Hz",
                        textRow.reduced (8, 0).translated (0, textRow.getHeight() / 2 - 6),
                        juce::Justification::centred);
        }
        else
        {
            g.setColour (juce::Colour (0xff444444));
            g.setFont (juce::Font (14.0f));
            g.drawText ("--", textRow, juce::Justification::centred);
        }

        // ── Strobe / LED bar visualization ──────────────────────────────────
        auto vis = disp.reduced (8, 6);
        if (mDisplay == DisplayStyle::Strobe)
            paintStrobe (g, vis);
        else
            paintLEDBar (g, vis);
    }

    void paintStrobe (juce::Graphics& g, juce::Rectangle<int> b)
    {
        const int n = 12;        // 12 stripes
        const float w = (float) b.getWidth() / (float) n;
        for (int i = 0; i < n; ++i)
        {
            const float x = (float) b.getX() + (i + mStrobePhase) * w;
            const float wrappedX = std::fmod (x - (float) b.getX(),
                                              (float) b.getWidth() + w);
            const auto cell = juce::Rectangle<float> (
                (float) b.getX() + wrappedX, (float) b.getY(),
                w * 0.55f, (float) b.getHeight());
            // Alternating colour based on which stripe is moving.
            const bool inTune = mHasReading && std::abs (mReading.centsFromTarget) < 3.0f;
            const auto colour = inTune
                ? juce::Colour (0xff44ff88)
                : juce::Colour (0xffffaa20);
            g.setColour (colour.withAlpha (mHasReading ? 0.9f : 0.25f));
            g.fillRect (cell);
        }
    }

    void paintLEDBar (juce::Graphics& g, juce::Rectangle<int> b)
    {
        const int n = 21;            // -50..+50 cents in 5-cent steps
        const float w  = (float) b.getWidth() / (float) n;
        const float h  = (float) b.getHeight();
        const int   cx = n / 2;

        const float cents      = mHasReading ? mReading.centsFromTarget : 0.0f;
        const int   activeIdx  = juce::jlimit (0, n - 1, cx + (int) std::round (cents / 5.0f));

        for (int i = 0; i < n; ++i)
        {
            const auto cell = juce::Rectangle<float> ((float) b.getX() + i * w + 1.0f,
                                                      (float) b.getY() + 2.0f,
                                                      w - 2.0f, h - 4.0f);
            const bool isCenter = (i == cx);
            const bool isLit    = mHasReading && (i == activeIdx);

            juce::Colour base = isCenter ? juce::Colour (0xff44ff88) : juce::Colour (0xffffaa20);
            g.setColour (base.withAlpha (isLit ? 0.9f : (isCenter ? 0.35f : 0.15f)));
            g.fillRoundedRectangle (cell, 1.5f);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): the tuner layout is width-proportional already; in
        // a pedal tile it just reclaims the dBFS strip.
        if (mPanelMode != EditorPanelBase::PanelMode::Pedal)
        {
            dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
            b.removeFromRight (4);
        }

        // All controls always visible -- the tuner is a pedal, no Basic/Advanced
        // toggle to reveal them.  Trim + Mode | Display | Flat + Mute / 432.
        if (trimKnob)   trimKnob  ->setVisible (true);
        if (displaySel) displaySel->setVisible (true);
        if (btn432)     btn432    ->setVisible (true);
        if (flatSel)    flatSel   ->setVisible (true);

        // Right cluster: knob (top) + chickenheads + Mute / 432 buttons.
        const int rightW = juce::jmin (b.getWidth() / 3 + 30, 230);
        auto right = b.removeFromRight (rightW);

        // QA-ManualPress M-4: the tuner readout is PAINTED, so it declares the
        // rect paint() carves - what is left once the right cluster and the
        // dBFS column paint() always reserves are taken, inset as drawn.  Pedal
        // mode only: that is the panel the pedalboard figure numbers, and it is
        // the only mode where paint()'s carve and this one agree.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
        {
            const auto disp = b.withTrimmedRight (32 + 4).reduced (4, 4);
            getProperties().set (kDotAnchor,
                                 "BSPDL-8@" + juce::String (disp.getX())
                                    + "," + juce::String (disp.getY())
                                    + "," + juce::String (disp.getWidth())
                                    + "," + juce::String (disp.getHeight()));
        }

        // Trim knob top-right.
        if (trimKnob) trimKnob->setBounds (right.removeFromTop (kKnobSz + 14)
                                                  .withSizeKeepingCentre (kKnobSz, kKnobSz + 14));
        // Mode | Display | Flat chickenheads (three across).
        auto sels = right.removeFromTop (50);
        const int cw = sels.getWidth() / 3;
        if (modeSel)    modeSel   ->setBounds (sels.removeFromLeft (cw).reduced (2));
        if (displaySel) displaySel->setBounds (sels.removeFromLeft (cw).reduced (2));
        if (flatSel)    flatSel   ->setBounds (sels.reduced (2));
        // Mute + 432 buttons.
        auto btnRow = right.removeFromTop (22);
        if (muteBtn) muteBtn->setBounds (btnRow.removeFromLeft (btnRow.getWidth() / 2).reduced (2, 0));
        if (btn432)  btn432 ->setBounds (btnRow.reduced (2, 0));
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// NAMPedalStylePanel - User NAM Pedal panel
// File-load button + filename label + 6 knobs (Input/Drive / Low / Mid /
// High / Blend / Output).  HarmonicLAF (drive family).  Default file picker
// location: Documents/BaySickDAW/Presets/Effects/Pedals/User NAM Pedals/.
// ─────────────────────────────────────────────────────────────────────────────
struct NAMPedalStylePanel : public EditorPanelBase
{
    NAMPedalStyleDSP* mDsp { nullptr };
    std::unique_ptr<juce::TextButton>  loadBtn;
    std::unique_ptr<juce::Label>       fileLabel;
    std::unique_ptr<juce::FileChooser> chooser;

    explicit NAMPedalStylePanel (NAMPedalStyleDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&HarmonicLAF::get());
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Input/Drive", -24.f, 24.f, 0.f, 0.1f, "Pre-model gain (-24 to +24 dB).  Pushes a hotter signal into the NAM model to fake more drive." },
            { "Low",         -15.f, 15.f, 0.f, 0.1f, "Post-model low shelf @ 100 Hz (+/-15 dB)" },
            { "Mid",         -15.f, 15.f, 0.f, 0.1f, "Post-model mid peak @ 1 kHz, Q=0.7 (+/-15 dB)" },
            { "High",        -15.f, 15.f, 0.f, 0.1f, "Post-model high shelf @ 5 kHz (+/-15 dB)" },
            { "Blend",         0.f,  1.f, 1.f, 0.001f, "Dry/wet mix.  100% = full NAM model.  Lower mixes the dry input back in." },
            { "Output",      -24.f, 12.f, 0.f, 0.1f, "Output trim (-24 to +12 dB).  Lets you tame loud captures." },
        });
        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setInputDb  ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setLowDb    ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setMidDb    ((float) knobs[2]->slider.getValue()); };
        knobs[3]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setHighDb   ((float) knobs[3]->slider.getValue()); };
        knobs[4]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setBlend    ((float) knobs[4]->slider.getValue()); };
        knobs[5]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setOutputDb ((float) knobs[5]->slider.getValue()); };

        loadBtn = std::make_unique<juce::TextButton>("Load .nam");
        loadBtn->setTooltip ("Load an amp capture (.nam) file");
        loadBtn->onClick = [this] { showFileChooser(); };
        addAndMakeVisible (*loadBtn);

        fileLabel = std::make_unique<juce::Label>();
        fileLabel->setJustificationType (juce::Justification::centredLeft);
        fileLabel->setFont (juce::Font (10.0f));
        fileLabel->setColour (juce::Label::textColourId, juce::Colour (0xffe0e0e0));
        fileLabel->setText (dsp && dsp->getModelName().isNotEmpty()
                                ? dsp->getModelName()
                                : juce::String ("(no file loaded)"),
                            juce::dontSendNotification);
        addAndMakeVisible (*fileLabel);

        if (dsp)
        {
            knobs[0]->slider.setValue (dsp->mInputDb,  juce::sendNotificationSync);
            knobs[1]->slider.setValue (dsp->mLowDb,    juce::sendNotificationSync);
            knobs[2]->slider.setValue (dsp->mMidDb,    juce::sendNotificationSync);
            knobs[3]->slider.setValue (dsp->mHighDb,   juce::sendNotificationSync);
            knobs[4]->slider.setValue (dsp->mBlend01,  juce::sendNotificationSync);
            knobs[5]->slider.setValue (dsp->mOutputDb, juce::sendNotificationSync);
        }
    }

    ~NAMPedalStylePanel() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        HarmonicLAF::paintHammeritePanel (g, getLocalBounds());
    }

    void showFileChooser()
    {
        if (! mDsp) return;
        auto userPedals = EffectPresetIO::userNamPedalsDir();
        userPedals.createDirectory();

        // getModelPath() returns the PERSISTED reference, not the file that was
        // opened: a bundled project stores "Samples/<name>.nam", which a bare
        // juce::File would resolve against the process working directory.
        auto startDir = mDsp->getModelPath().isNotEmpty()
                            ? ProjectFileResolver::resolve (mDsp->getModelPath()).getParentDirectory()
                            : userPedals;

        chooser = std::make_unique<juce::FileChooser>("Load NAM Pedal", startDir, "*.nam");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (! f.existsAsFile() || ! mDsp) return;
                juce::String err;
                if (mDsp->loadModel (f, err))
                {
                    if (fileLabel)
                        fileLabel->setText (mDsp->getModelName(), juce::dontSendNotification);
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::AlertWindow::WarningIcon, "NAM Load Failed", err);
                }
            });
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);

        // Top strip: Load button + filename label.
        auto topStrip = b.removeFromTop (22);
        if (loadBtn)   loadBtn  ->setBounds (topStrip.removeFromLeft (76).reduced (2, 1));
        if (fileLabel) fileLabel->setBounds (topStrip.reduced (4, 1));

        // 6 knobs right-clustered.
        rightClusterKnobs (b, knobs);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// I-14 (2026-05-03): Simplified pedalboard panels (PanelMode::Pedal) for the
// 7 existing effects that show up inside BaySickPedals as well as the FX rack.
// Same DSP as their full-mode siblings; just simpler subset of knobs per
// locked spec.  createEffectEditor dispatches to these when mode == Pedal.
// ─────────────────────────────────────────────────────────────────────────────

// LimiterDSP pedal panel - Ceiling + Release.
struct LimiterPedalPanel : public EditorPanelBase
{
    LimiterDSP* mDsp { nullptr };
    explicit LimiterPedalPanel (LimiterDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&DynamicsLAF::get());
        setVolumeKnobVariant (true);
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Ceiling", -24.f, 0.f,    -1.f,   0.1f, "Output ceiling (dB).  Anything above this gets pulled down." },
            { "Release",  10.f, 1000.f, 100.f,  1.f,  "Release time (ms)" },
        });
        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setCeilingDb ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setReleaseMs ((float) knobs[1]->slider.getValue()); };
    }
    ~LimiterPedalPanel() override { setLookAndFeel (nullptr); }
    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): pedal tiles grid instead of right-clustering.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
            return pedalTileGrid (b, knobs);
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);
        rightClusterKnobs (b, knobs);
    }
};

// SaturationDSP pedal panel - Drive + Mix + Tube/Console/Tape mode chickenhead.
struct SaturationPedalPanel : public EditorPanelBase
{
    SaturationDSP* mDsp { nullptr };
    std::unique_ptr<ChickenHeadSelector> typeSel;

    explicit SaturationPedalPanel (SaturationDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&HarmonicLAF::get());
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Drive", 0.f, 10.f,  3.f,   0.01f, "Saturation drive amount" },
            { "Mix",   0.f, 100.f, 100.f, 1.f,   "Wet/dry mix (%)" },
        });
        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setFlowers ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setWet     ((float) knobs[1]->slider.getValue()); };

        typeSel = std::make_unique<ChickenHeadSelector>();
        typeSel->setOptions ({
            { "Tu", "Tube",    "Tube saturation -- warm, asymmetric harmonics" },
            { "Co", "Console", "Console saturation -- transparent, low-distortion" },
            { "Ta", "Tape",    "Tape saturation -- compression + high-frequency rolloff" },
        });
        typeSel->setBodyTooltip ("Saturation type");
        typeSel->setSelectedIndex (dsp ? (int) dsp->mSatType : 0, juce::dontSendNotification);
        typeSel->onChange = [dsp] (int idx) { if (dsp) dsp->setSatType (idx); };
        addAndMakeVisible (*typeSel);
    }
    ~SaturationPedalPanel() override { setLookAndFeel (nullptr); }
    void paint (juce::Graphics& g) override { HarmonicLAF::paintHammeritePanel (g, getLocalBounds()); }
    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): pedal tiles grid instead of right-clustering.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
            return pedalTileGrid (b, knobs, { typeSel ? &*typeSel : nullptr });
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);
        auto remaining = rightClusterKnobs (b, knobs);
        if (typeSel)
        {
            auto col = remaining.removeFromRight (66);
            typeSel->setBounds (col.reduced (2));
        }
    }
};

// ChorusDSP pedal panel - Rate + Depth + Mix.
struct ChorusPedalPanel : public EditorPanelBase
{
    ChorusDSP* mDsp { nullptr };
    explicit ChorusPedalPanel (ChorusDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&ModulationLAF::get());
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Rate",  0.05f, 5.f,  0.5f,  0.01f, "LFO rate (Hz)" },
            { "Depth", 0.f,   20.f, 4.f,   0.1f,  "Modulation depth (ms)" },
            { "Mix",   0.f,   1.f,  0.5f,  0.01f, "Wet level" },
        });
        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setLFOFreq (0, (float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setDepth      ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setWet        ((float) knobs[2]->slider.getValue()); };
    }
    ~ChorusPedalPanel() override { setLookAndFeel (nullptr); }
    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): pedal tiles grid instead of right-clustering.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
            return pedalTileGrid (b, knobs);
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);
        rightClusterKnobs (b, knobs);
    }
};

// FlangerDSP pedal panel - Rate + Depth + Feedback + Mix.
struct FlangerPedalPanel : public EditorPanelBase
{
    FlangerDSP* mDsp { nullptr };
    explicit FlangerPedalPanel (FlangerDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&ModulationLAF::get());
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Rate",     0.05f, 5.f,  0.5f, 0.01f, "LFO rate (Hz)" },
            { "Depth",    0.f,  10.f,  3.f,  0.1f,  "Sweep depth (ms)" },
            { "Feedback", -1.f,  1.f,  0.5f, 0.01f, "Feedback (-1..+1)" },
            { "Mix",      0.f,   1.f,  0.5f, 0.01f, "Wet level" },
        });
        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setRate     ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setDepth    ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setFeedback ((float) knobs[2]->slider.getValue()); };
        knobs[3]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setWet      ((float) knobs[3]->slider.getValue()); };
    }
    ~FlangerPedalPanel() override { setLookAndFeel (nullptr); }
    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): pedal tiles grid instead of right-clustering.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
            return pedalTileGrid (b, knobs);
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);
        rightClusterKnobs (b, knobs);
    }
};

// PhaserDSP pedal panel - Rate + Depth + Feedback + Mix.
struct PhaserPedalPanel : public EditorPanelBase
{
    PhaserDSP* mDsp { nullptr };
    explicit PhaserPedalPanel (PhaserDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&ModulationLAF::get());
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Rate",     0.05f,  10.f,  0.5f, 0.01f, "LFO rate (Hz)" },
            { "Depth",    0.f,     1.f,  0.5f, 0.01f, "Sweep depth" },
            { "Feedback", -1.2f,   1.2f, 0.5f, 0.01f, "Feedback amount" },
            { "Mix",      0.f,     1.f,  0.5f, 0.01f, "Wet level" },
        });
        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setRate     ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setDepth    ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setFeedback ((float) knobs[2]->slider.getValue()); };
        knobs[3]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setWet      ((float) knobs[3]->slider.getValue()); };
    }
    ~PhaserPedalPanel() override { setLookAndFeel (nullptr); }
    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): pedal tiles grid instead of right-clustering.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
            return pedalTileGrid (b, knobs);
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);
        rightClusterKnobs (b, knobs);
    }
};

// DelayDSP pedal panel - Time + Feedback + Mix + Sync toggle.
struct DelayPedalPanel : public EditorPanelBase
{
    DelayDSP* mDsp { nullptr };
    std::unique_ptr<juce::TextButton> syncBtn;

    explicit DelayPedalPanel (DelayDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&TimeLAF::get());
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Time",     1.f, 2000.f, 375.f, 1.f,   "Delay time (ms).  Sync toggle locks to host BPM." },
            { "Feedback", 0.f, 1.2f,   0.4f,  0.01f, "Feedback level" },
            { "Mix",      0.f, 1.f,    0.4f,  0.01f, "Wet level" },
        });
        knobs[0]->slider.setSkewFactorFromMidPoint (250.0);
        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setDelayMs       ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setFeedbackLevel ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setWetIn         ((float) knobs[2]->slider.getValue()); };

        syncBtn = std::make_unique<juce::TextButton>("Sync");
        syncBtn->setClickingTogglesState (true);
        syncBtn->setTooltip ("Lock delay time to host BPM");
        syncBtn->onClick = [this] { if (mDsp) mDsp->setTempoSync (syncBtn->getToggleState()); };
        addAndMakeVisible (*syncBtn);
    }
    ~DelayPedalPanel() override { setLookAndFeel (nullptr); }
    void paint (juce::Graphics& g) override { TimeLAF::paintPultecPanel (g, getLocalBounds()); }
    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        // T7 (Specific-4): pedal tiles grid instead of right-clustering.
        if (mPanelMode == EditorPanelBase::PanelMode::Pedal)
            return pedalTileGrid (b, knobs, { syncBtn ? &*syncBtn : nullptr });
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);
        auto remaining = rightClusterKnobs (b, knobs);
        if (syncBtn)
        {
            auto col = remaining.removeFromRight (60);
            syncBtn->setBounds (col.withSizeKeepingCentre (54, 22));
        }
    }
};

// ReverbDSP pedal panel - Decay + Damp + Mix + Algorithm chickenhead.
struct ReverbPedalPanel : public EditorPanelBase
{
    ReverbDSP* mDsp { nullptr };
    std::unique_ptr<ChickenHeadSelector> algoSel;

    explicit ReverbPedalPanel (ReverbDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&TimeLAF::get());
        disableVU();
        disableOutputVolKnob();

        buildKnobs (*this, knobs, {
            { "Decay", 0.1f,   10.f,    1.5f,  0.01f, "RT60 decay (seconds)" },
            { "Damp",  500.f,  20000.f, 6000.f, 1.f,  "High-frequency damping cutoff (Hz)" },
            { "Mix",   0.f,    1.f,     0.3f,  0.01f, "Wet level" },
        });
        knobs[1]->slider.setSkewFactorFromMidPoint (4000.0);
        knobs[0]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setDecay    ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setHighDamp ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this] { if (dsp) dsp->setWet      ((float) knobs[2]->slider.getValue()); };

        algoSel = std::make_unique<ChickenHeadSelector>();
        algoSel->setOptions ({
            { "Pl", "Plate",    "Schroeder allpass cascade -- bright, dense plate reverb" },
            { "Hl", "Hall",     "Large FDN hall character" },
            { "Ch", "Chamber",  "Mid-size FDN with denser modulation" },
            { "Rm", "Room",     "Small FDN + early reflections" },
            { "VB", "VocalBth", "Tight short-decay vocal booth" },
        });
        algoSel->setBodyTooltip ("Reverb algorithm");
        algoSel->setSelectedIndex (dsp ? dsp->getAlgorithm() : 1, juce::dontSendNotification);
        algoSel->onChange = [dsp] (int idx) { if (dsp) dsp->setAlgorithm (idx); };
        addAndMakeVisible (*algoSel);
    }
    ~ReverbPedalPanel() override { setLookAndFeel (nullptr); }
    void paint (juce::Graphics& g) override { TimeLAF::paintPultecPanel (g, getLocalBounds()); }
    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        dbfsOut->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (4);
        auto remaining = rightClusterKnobs (b, knobs);
        if (algoSel)
        {
            auto col = remaining.removeFromRight (66);
            algoSel->setBounds (col.reduced (2));
        }
    }
};

// ── GatePanel - QA-Fe2 (2026-07-16) ──────────────────────────────────────────
// Vocal-chain channel gate (slot 0).  Cream Dynamics family.
struct GatePanel : public EditorPanelBase, private juce::Timer
{
    GateDSP* mDsp { nullptr };
    std::unique_ptr<GateGRMeter> grMeter;   // gate-scaled meter (0..-80, red at the open end)

    explicit GatePanel (GateDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&DynamicsLAF::get());

        buildKnobs (*this, knobs, {
            { "Thresh",  -80.f,    0.f, -80.f, 0.5f, "Gate threshold (dB). Signal below closes the gate; default -80 = fully open" },
            { "Range",   -80.f,    0.f, -60.f, 0.5f, "Attenuation when closed (dB)" },
            { "Attack",    0.1f, 100.f,   1.f, 0.1f, "Opening speed (ms)" },
            { "Hold",      0.f,  500.f,  50.f, 1.f,  "Time held open after the signal drops (ms)" },
            { "Release",   5.f, 2000.f, 100.f, 1.f,  "Closing speed (ms)" },
        });
        for (auto& k : knobs)
            k->slider.getProperties().set (DynamicsLAF::kKnobVariant, "modernAnalog");

        if (mDsp != nullptr)
        {
            knobs[0]->slider.setValue (mDsp->thresholdDb, juce::dontSendNotification);
            knobs[1]->slider.setValue (mDsp->rangeDb,     juce::dontSendNotification);
            knobs[2]->slider.setValue (mDsp->attackMs,    juce::dontSendNotification);
            knobs[3]->slider.setValue (mDsp->holdMs,      juce::dontSendNotification);
            knobs[4]->slider.setValue (mDsp->releaseMs,   juce::dontSendNotification);
        }
        knobs[0]->slider.onValueChange = [this] { if (mDsp) mDsp->setThresholdDb ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [this] { if (mDsp) mDsp->setRangeDb     ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [this] { if (mDsp) mDsp->setAttackMs    ((float) knobs[2]->slider.getValue()); };
        knobs[3]->slider.onValueChange = [this] { if (mDsp) mDsp->setHoldMs      ((float) knobs[3]->slider.getValue()); };
        knobs[4]->slider.onValueChange = [this] { if (mDsp) mDsp->setReleaseMs   ((float) knobs[4]->slider.getValue()); };

        grMeter = std::make_unique<GateGRMeter>();
        addAndMakeVisible (*grMeter);
        startTimerHz (30);
    }

    ~GatePanel() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        // LA-2A cream/wood plate -- identical to De-esser/Compressor/TS.
        DynamicsLAF::paintLA2APanel (g, getLocalBounds());
    }

    void timerCallback() override
    {
        // QA-Layout T13 crash fix: liveDsp() returns mDsp ONLY while the rack
        // still hands back that same pointer for this slot, so this one guard
        // makes every mDsp-> below safe.  A panel timer and its window's rebuild
        // poll are independent; without this the timer can tick on a DSP a
        // project load already destroyed.  See EditorPanelBase::liveDsp.
        if (liveDsp() == nullptr) return;
        if (grMeter && mDsp)
            grMeter->setGainReduction (mDsp->getGainReductionDb());
    }

    void bindToApvts (juce::AudioProcessorValueTreeState& apvts,
                      const juce::String& prefix) override
    {
        auto bindSlider = [&] (juce::Slider& s, const char* suffix)
        {
            mChainSliderAtts.push_back (std::make_unique<TaggedSliderAttachment> (
                apvts, prefix + suffix, s));
        };
        bindSlider (knobs[0]->slider, "gate_threshold");
        bindSlider (knobs[1]->slider, "gate_range");
        bindSlider (knobs[2]->slider, "gate_attack");
        bindSlider (knobs[3]->slider, "gate_hold");
        bindSlider (knobs[4]->slider, "gate_release");
    }
    std::vector<std::unique_ptr<TaggedSliderAttachment>> mChainSliderAtts;

    void resized() override
    {
        // De-esser layout rhythm: right-edge dbfs meter + output knob, then
        // the readout slot, then one knob row.
        auto b = getLocalBounds().reduced (4, 4);
        if (grMeter)
        {
            const int gw = juce::jmin (96, b.getHeight() + 8);
            grMeter->setBounds (b.removeFromLeft (gw).reduced (1, 2));
            b.removeFromLeft (4);
        }
        dbfsOut      ->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (2);
        outputVolKnob->setBounds (b.removeFromRight (kKnobSz).withSizeKeepingCentre (kKnobSz, kKnobSz));
        b.removeFromRight (4);
        std::vector<VKnob*> row;
        for (auto& k : knobs) row.push_back (k.get());
        layoutKnobsH (b, row, kKnobSz);
    }
};

// ── DeReverbPanel - QA-Fe2 (2026-07-16) ──────────────────────────────────────
// Vocal-chain De-reverb (slot 1).  Reduction / Tail / Mix + suppression readout.
struct DeReverbPanel : public EditorPanelBase, private juce::Timer
{
    DeReverbDSP* mDsp { nullptr };
    std::unique_ptr<GRMeter> grMeter;   // real GR asset, matches Compressor family

    explicit DeReverbPanel (DeReverbDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&DynamicsLAF::get());

        buildKnobs (*this, knobs, {
            { "Reduce", 0.f,  100.f,  50.f, 1.f, "How hard the room tail is suppressed (%)" },
            { "Tail",  100.f, 1000.f, 400.f, 5.f, "Modeled reverb tail length (ms). Match the room: bigger room = longer" },
            { "Mix",    0.f,  100.f, 100.f, 1.f, "Dry/wet blend (%)" },
        });
        for (auto& k : knobs)
            k->slider.getProperties().set (DynamicsLAF::kKnobVariant, "modernAnalog");

        if (mDsp != nullptr)
        {
            knobs[0]->slider.setValue (mDsp->reductionPct, juce::dontSendNotification);
            knobs[1]->slider.setValue (mDsp->tailMs,       juce::dontSendNotification);
            knobs[2]->slider.setValue (mDsp->mixPct,       juce::dontSendNotification);
        }
        knobs[0]->slider.onValueChange = [this] { if (mDsp) mDsp->setReductionPct ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [this] { if (mDsp) mDsp->setTailMs       ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [this] { if (mDsp) mDsp->setMixPct       ((float) knobs[2]->slider.getValue()); };

        grMeter = std::make_unique<GRMeter>();
        addAndMakeVisible (*grMeter);
        startTimerHz (30);
    }

    ~DeReverbPanel() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        // LA-2A cream/wood plate -- identical to De-esser/Compressor/TS.
        DynamicsLAF::paintLA2APanel (g, getLocalBounds());
    }

    void timerCallback() override
    {
        // QA-Layout T13 crash fix: liveDsp() returns mDsp ONLY while the rack
        // still hands back that same pointer for this slot, so this one guard
        // makes every mDsp-> below safe.  A panel timer and its window's rebuild
        // poll are independent; without this the timer can tick on a DSP a
        // project load already destroyed.  See EditorPanelBase::liveDsp.
        if (liveDsp() == nullptr) return;
        if (grMeter && mDsp)
            grMeter->setGainReduction (mDsp->getGainReductionDb());
    }

    void bindToApvts (juce::AudioProcessorValueTreeState& apvts,
                      const juce::String& prefix) override
    {
        auto bindSlider = [&] (juce::Slider& s, const char* suffix)
        {
            mChainSliderAtts.push_back (std::make_unique<TaggedSliderAttachment> (
                apvts, prefix + suffix, s));
        };
        bindSlider (knobs[0]->slider, "dereverb_reduction");
        bindSlider (knobs[1]->slider, "dereverb_tail");
        bindSlider (knobs[2]->slider, "dereverb_mix");
    }
    std::vector<std::unique_ptr<TaggedSliderAttachment>> mChainSliderAtts;

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        if (grMeter)
        {
            const int gw = juce::jmin (96, b.getHeight() + 8);
            grMeter->setBounds (b.removeFromLeft (gw).reduced (1, 2));
            b.removeFromLeft (4);
        }
        dbfsOut      ->setBounds (b.removeFromRight (32).reduced (1, 2));
        b.removeFromRight (2);
        outputVolKnob->setBounds (b.removeFromRight (kKnobSz).withSizeKeepingCentre (kKnobSz, kKnobSz));
        b.removeFromRight (4);
        std::vector<VKnob*> row;
        for (auto& k : knobs) row.push_back (k.get());
        layoutKnobsH (b, row, kKnobSz);
    }
};

std::unique_ptr<juce::Component> createEffectEditor (DSPBase* effect,
                                                     EffectType type,
                                                     EditorPanelBase::PanelMode mode)
{
    if (!effect) return nullptr;

    std::unique_ptr<juce::Component> panel;

    // I-14 (2026-05-03): Pedal-mode dispatch for the 7 existing effects that
    // appear inside BaySickPedals.  Same DSP, simplified knob set per locked
    // spec.  Falls through to the default Full-mode dispatch below for all
    // other effects (and for these 7 when mode == Full).
    if (mode == EditorPanelBase::PanelMode::Pedal)
    {
        switch (type)
        {
            case EffectType::Limiter:
                panel = std::make_unique<LimiterPedalPanel>    (static_cast<LimiterDSP*>    (effect));
                break;
            case EffectType::Saturation:
            case EffectType::Tape:   // Tape = SaturationDSP+Type::Tape alias
                panel = std::make_unique<SaturationPedalPanel> (static_cast<SaturationDSP*> (effect));
                break;
            case EffectType::Chorus:
                panel = std::make_unique<ChorusPedalPanel>     (static_cast<ChorusDSP*>     (effect));
                break;
            case EffectType::Flanger:
                panel = std::make_unique<FlangerPedalPanel>    (static_cast<FlangerDSP*>    (effect));
                break;
            case EffectType::Phaser:
                panel = std::make_unique<PhaserPedalPanel>     (static_cast<PhaserDSP*>     (effect));
                break;
            case EffectType::Delay:
                panel = std::make_unique<DelayPedalPanel>      (static_cast<DelayDSP*>      (effect));
                break;
            case EffectType::Reverb:
                panel = std::make_unique<ReverbPedalPanel>     (static_cast<ReverbDSP*>     (effect));
                break;
            default:
                break;   // not a pedal-variant effect; fall through to Full dispatch.
        }

        if (panel)
        {
            if (auto* base = dynamic_cast<EditorPanelBase*> (panel.get()))
            {
                base->mPanelMode = mode;
                base->mDsp        = effect;
                base->mEffectType = type;
                // This branch IS the pedal face, so the key is the pedal
                // variant -- not the DSP's character mode.  A Tape-mode
                // SaturationDSP shown here has a 0..10 "Drive" into setFlowers,
                // where its rack panel's "Drive" is dB into setTapeInputGain.
                base->mVariant    = EffectParamMap::variantOf (
                                        type, effect, EffectParamMap::PanelContext::Pedal);
                base->disableVU();
                base->disableDbfsMeter();
                base->disableGrMeter();
            }
            return panel;
        }
        // else: fall through and use the Full-mode panel for non-listed effects.
    }

    switch (type)
    {
        case EffectType::Compressor:
        {
            // H-7 (2026-05-01): dispatch to the right inline panel based on
            // the DSP's character mode.  Modern = full SSL-ish layout; FET +
            // Opto get authentic 1176 / LA-2A minimal layouts.
            // I-4 (2026-05-02): CS Style = BOSS CS-Style sustain pedal layout
            // (Level / Tone / Attack / Sustain).
            auto* c = static_cast<CompressorDSP*> (effect);
            switch (c->mType)
            {
                case CompressorDSP::Type::FET:    panel = std::make_unique<FETCompressorPanel>     (c); break;
                case CompressorDSP::Type::Opto:   panel = std::make_unique<OptoCompressorPanel>    (c); break;
                case CompressorDSP::Type::CS:     panel = std::make_unique<CSStyleCompressorPanel> (c); break;
                case CompressorDSP::Type::Modern: default: panel = std::make_unique<CompressorPanel> (c); break;
            }
            break;
        }
        case EffectType::Reverb:
            panel = std::make_unique<ReverbPanel>        (static_cast<ReverbDSP*>        (effect));
            break;
        case EffectType::Chorus:
            panel = std::make_unique<ChorusPanel>        (static_cast<ChorusDSP*>        (effect));
            break;
        case EffectType::Delay:
        {
            // H-8 (2026-05-02): Echo Type = full DelayPanel; VocalDoubler Type
            // = minimal dual-tap layout (Time L / R / Detune / Width / Rate / Mix).
            auto* d = static_cast<DelayDSP*> (effect);
            panel = (d->getType() == (int) DelayDSP::Type::VocalDoubler)
                ? std::unique_ptr<juce::Component> (new VocalDoublerDelayPanel (d))
                : std::unique_ptr<juce::Component> (new DelayPanel             (d));
            break;
        }
        case EffectType::Saturation:
        {
            // H-7: Tube = full layout; Console = minimalist Drive/Color/Mix/Out.
            // H-10 (2026-05-02): Tape routes to TapeSatPanel, which binds
            // SaturationDSP::setTape*.
            auto* s = static_cast<SaturationDSP*> (effect);
            switch (s->mSatType)
            {
                case SaturationDSP::Type::Console:
                    panel = std::unique_ptr<juce::Component> (new ConsoleSaturationPanel (s));
                    break;
                case SaturationDSP::Type::Tape:
                    panel = std::unique_ptr<juce::Component> (new TapeSatPanel           (s));
                    break;
                case SaturationDSP::Type::Tube:
                default:
                    panel = std::unique_ptr<juce::Component> (new SaturationPanel        (s));
                    break;
            }
            break;
        }
        case EffectType::Flanger:
            panel = std::make_unique<FlangerPanel>       (static_cast<FlangerDSP*>       (effect));
            break;
        case EffectType::Overdrive:
        {
            // I-5 (2026-05-02): Type::Pedal mounts the OD Style pedal panel
            // (HarmonicLAF, Drive/Tone/Level); Type::Rack keeps the existing
            // full FX-rack panel.  Same DSP, different layout.
            auto* o = static_cast<OverdriveDSP*> (effect);
            if (o->mType == OverdriveDSP::Type::Pedal)
                panel = std::make_unique<OverdrivePedalPanel> (o);
            else
                panel = std::make_unique<OverdrivePanel>      (o);
            break;
        }
        case EffectType::Phaser:
            panel = std::make_unique<PhaserPanel>        (static_cast<PhaserDSP*>        (effect));
            break;
        case EffectType::TransientShaper:
            panel = std::make_unique<TransientShaperPanel>(static_cast<TransientShaperDSP*>(effect));
            break;
        case EffectType::Tape:
            // H-10 cutover (2026-05-02): EffectType::Tape is now an alias
            // for SaturationDSP+Type::Tape; the slot's effect ptr is a
            // SaturationDSP, not a TapeDSP.  Construct the Saturation-bound
            // TapeSatPanel so knob bindings hit setTape* on the right object.
            panel = std::make_unique<TapeSatPanel>       (static_cast<SaturationDSP*>    (effect));
            break;
        case EffectType::Limiter:
            panel = std::make_unique<LimiterPanel>       (static_cast<LimiterDSP*>       (effect));
            break;
        case EffectType::DeEsser:
            panel = std::make_unique<DeEsserPanel>       (static_cast<DeEsserDSP*>       (effect));
            break;
        case EffectType::Gate:                                                       // QA-Fe2
            panel = std::make_unique<GatePanel>          (static_cast<GateDSP*>          (effect));
            break;
        case EffectType::DeReverb:                                                   // QA-Fe2
            panel = std::make_unique<DeReverbPanel>      (static_cast<DeReverbDSP*>      (effect));
            break;

        // I-5 (2026-05-02): BaySickPedals Harmonics drive pedals batch panels.
        case EffectType::BluesDriveStyle:
            panel = std::make_unique<BluesDriveStylePanel> (static_cast<BluesDriveStyleDSP*> (effect));
            break;
        case EffectType::DistortionStyle:
            panel = std::make_unique<DistortionStylePanel> (static_cast<DistortionStyleDSP*> (effect));
            break;
        case EffectType::FuzzStyle:
            panel = std::make_unique<FuzzStylePanel>       (static_cast<FuzzStyleDSP*>       (effect));
            break;
        case EffectType::HighGainStyle:
            panel = std::make_unique<HighGainStylePanel>   (static_cast<HighGainStyleDSP*>   (effect));
            break;

        // I-6 (2026-05-02): BaySickPedals Harmonics bass pedals batch panels.
        case EffectType::BassDriverStyle:
            panel = std::make_unique<BassDriverStylePanel>    (static_cast<BassDriverStyleDSP*>    (effect));
            break;
        case EffectType::BassOverdriveStyle:
            panel = std::make_unique<BassOverdriveStylePanel> (static_cast<BassOverdriveStyleDSP*> (effect));
            break;

        // I-7 (2026-05-02): OC Style Octave panel.
        case EffectType::OctaveStyle:
            panel = std::make_unique<OctaveStylePanel> (static_cast<OctaveStyleDSP*> (effect));
            break;

        // I-8 (2026-05-03): Dynamics pedals batch panels.
        case EffectType::NoiseGateStyle:
            panel = std::make_unique<NoiseGateStylePanel>      (static_cast<NoiseGateStyleDSP*>      (effect));
            break;
        case EffectType::BassCompressorStyle:
            panel = std::make_unique<BassCompressorStylePanel> (static_cast<BassCompressorStyleDSP*> (effect));
            break;

        // I-9 (2026-05-03): SY Style Polyphonic Synth panel.
        case EffectType::SynthStyle:
            panel = std::make_unique<SynthStylePanel> (static_cast<SynthStyleDSP*> (effect));
            break;

        // I-10 (2026-05-03): PW Style Wah panel.
        case EffectType::WahStyle:
            panel = std::make_unique<WahStylePanel> (static_cast<WahStyleDSP*> (effect));
            break;

        // I-11 (2026-05-03): AD Style Acoustic Preamp panel.
        case EffectType::AcousticPreampStyle:
            panel = std::make_unique<AcousticPreampStylePanel> (static_cast<AcousticPreampStyleDSP*> (effect));
            break;
        // I-11 (2026-05-03): AC Style Acoustic Simulator panel.
        case EffectType::AcousticSimulatorStyle:
            panel = std::make_unique<AcousticSimulatorStylePanel> (static_cast<AcousticSimulatorStyleDSP*> (effect));
            break;

        // I-12 (2026-05-03): EQ trio batch panels.
        case EffectType::GraphicEQStyle:
            panel = std::make_unique<GraphicEQStylePanel>     (static_cast<GraphicEQStyleDSP*>     (effect));
            break;
        case EffectType::BassGraphicEQStyle:
            panel = std::make_unique<BassGraphicEQStylePanel> (static_cast<BassGraphicEQStyleDSP*> (effect));
            break;
        case EffectType::FurmanEQStyle:
            panel = std::make_unique<FurmanEQStylePanel>      (static_cast<FurmanEQStyleDSP*>      (effect));
            break;

        // I-13 (2026-05-03): TU Style Tuner panel.
        case EffectType::TunerStyle:
            panel = std::make_unique<TunerStylePanel> (static_cast<TunerStyleDSP*> (effect));
            break;

        // I-15c (2026-05-03): User NAM Pedal panel.
        case EffectType::NAMPedalStyle:
            panel = std::make_unique<NAMPedalStylePanel> (static_cast<NAMPedalStyleDSP*> (effect));
            break;

        // QA-ModelShell TS6: the plugin brings its own UI, so this is the ONE
        // case that is not an EditorPanelBase.  Every caller already guards its
        // EditorPanelBase cast, and a plugin has nothing to stamp -- its lanes
        // are registered model-side from the instance's parameter list, not
        // from knob componentIDs.
        case EffectType::VST3Plugin:
            if (auto* hosted = dynamic_cast<Hosting::HostedPluginEffect*> (effect))
                if (auto* inst = hosted->getHosted())
                    panel = std::make_unique<Hosting::HostedPluginEditor> (*inst);
            break;

        default:
            panel = nullptr;
            break;
    }

    // I-2 (2026-05-02): stamp the panel's mPanelMode flag so derived layouts
    // can branch in resized().
    // I-15 (2026-05-03): pedal-mode panels strip VU + dBFS + GR meters per
    // locked spec (BaySickPedals tiles do not display level meters).
    if (auto* base = dynamic_cast<EditorPanelBase*> (panel.get()))
    {
        base->mPanelMode = mode;
        // Reached either as a normal FX-rack panel, or as the fall-through for a
        // pedal-mode slot holding a type with no pedal face (the picker offers
        // Compressor and Overdrive, which resolve through their own DSP-read
        // character modes) -- Rack is the right key for both.
        base->mDsp        = effect;
        base->mEffectType = type;
        base->mVariant    = EffectParamMap::variantOf (type, effect);
        if (mode == EditorPanelBase::PanelMode::Pedal)
        {
            base->disableVU();
            base->disableDbfsMeter();
            base->disableGrMeter();
        }
    }

    return panel;
}
