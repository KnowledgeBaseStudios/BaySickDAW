#include "EffectEditorPanels.h"
#include "SlotComponent.h"   // H-8: VocalDoubler's Slap button calls remountEditor
// D.4 (2026-05-01): force MSBuild to recompile this file — Compressor + Delay
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
#include "../DSP/TapeDSP.h"
#include "../DSP/LimiterDSP.h"
#include "../DSP/DeEsserDSP.h"

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

// Pink = active (engaged), dark = off — Change C colors
static void setPink(juce::TextButton& btn)
{
    btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffce3f8e));
    btn.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff2e2e33));
}

// ── LabeledToggle retired ────────────────────────────────────────────────────
// All usages migrated to DualLabelToggle (see SharedUI.h). LabeledToggle was
// removed 2026-04-17 during the toggle/combo visual sweep.

// ── EditorPanelBase method implementations ────────────────────────────────────
EditorPanelBase::EditorPanelBase()
{
    // VU input meter — all panels create it; panels without VU call disableVU()
    vuIn = std::make_unique<VUMeter>(VUMeter::Vertical);
    vuIn->setTooltip("Input level");
    addAndMakeVisible(*vuIn);

    // DBFS output meter — compact mode always on (floor -20 dBFS, labels truncated)
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
    outputVolKnob->slider.setLookAndFeel(&VibeLAF::get());
    // Default to white variant; Dynamics panels call setVolumeKnobVariant(true)
    outputVolKnob->slider.getProperties().set("volumeKnob", juce::var("white"));
    addAndMakeVisible(*outputVolKnob);
}

EditorPanelBase::~EditorPanelBase()
{
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

    // Panels that keep knobs in their own row vectors (r1knobs/r2knobs) expose
    // them via getExtraKnobs(). Stamp those too so paramIds are complete.
    for (auto* k : getExtraKnobs())
        stampId(k);

    // Output vol knob
    if (outputVolKnob)
        outputVolKnob->paramId = base + "output_vol";

    // Register playback applicators + value readers so automation can drive and seed these knobs.
    // SafePointer guards against the slider being destroyed (effect swap in slot, panel rebuild,
    // etc.) between registration and invocation -- without it, a stale applicator stored in the
    // StandaloneEditor's map would dereference freed memory on the next automation tick and crash
    // inside NormalisableRange::snapToLegalValue (empty-state std::function with corrupted vptr).
    auto regKnob = [](VKnob* k)
    {
        if (!k || k->paramId.isEmpty()) return;
        juce::Component::SafePointer<juce::Slider> safeSl(&k->slider);
        double lo = k->slider.getMinimum(), hi = k->slider.getMaximum();
        juce::String pid = k->paramId;

        if (VKnobAutomation::sOnRegisterApplicator)
            VKnobAutomation::sOnRegisterApplicator(pid, [safeSl, lo, hi](float v01)
            {
                if (auto* sl = safeSl.getComponent())
                    sl->setValue(lo + v01 * (hi - lo), juce::sendNotification);
            });

        if (VKnobAutomation::sOnRegisterReader)
            VKnobAutomation::sOnRegisterReader(pid, [safeSl, lo, hi]() -> float
            {
                auto* sl = safeSl.getComponent();
                if (!sl) return 0.5f;
                double range = hi - lo;
                return range > 0.0 ? (float)((sl->getValue() - lo) / range) : 0.5f;
            });
    };
    for (auto& k : knobs) regKnob(k.get());
    for (auto* k : getExtraKnobs()) regKnob(k);
    regKnob(outputVolKnob.get());
}

void EditorPanelBase::setVolumeKnobVariant(bool dark)
{
    outputVolKnob->slider.getProperties().set("volumeKnob",
        dark ? juce::var("black") : juce::var("white"));
}

void EditorPanelBase::childrenChanged()
{
    // Whenever a child is added, if it's a toggle-state button, force VibeLAF so
    // drawButtonBackground in VibeLAF (switch-toggle filmstrip) is always called.
    // 2026-04-26: also opt in to "switchToggle" property — switch styling is
    // gated to opt-in callsites and effect panels qualify per the rule.
    for (auto* c : getChildren())
        if (auto* btn = dynamic_cast<juce::Button*>(c))
            if (btn->getClickingTogglesState())
            {
                if (&btn->getLookAndFeel() != &VibeLAF::get())
                    btn->setLookAndFeel(&VibeLAF::get());
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
    outputVolKnob->setBounds(b.removeFromRight(kKnobSz).withSizeKeepingCentre(kKnobSz, kKnobSz));
    b.removeFromRight(4);

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
// FETCompressorPanel — H-7 (2026-05-01)
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
            { "Input",   -60.f,   0.f, -12.f, 0.5f, "Input drive into the FET stage (dB) -- more = more compression" },
            { "Output",  -60.f,   0.f,   0.f, 0.5f, "Output level (dB)" },
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
            { "All", "All-buttons in", "All-buttons-in mode -- the famous UREI sound (~30:1 with bias shift)" },
        });
        ratioSel->setBodyTooltip ("Ratio (1176 face plate)");
        ratioSel->setDefaultLabelColour (juce::Colours::black);
        const float r = dsp ? dsp->ratio : 4.0f;
        const int initIdx = (r < 6.f)  ? 0
                          : (r < 10.f) ? 1
                          : (r < 16.f) ? 2
                          : (r < 50.f) ? 3 : 4;
        ratioSel->setSelectedIndex (initIdx, juce::dontSendNotification);
        ratioSel->onChange = [dsp] (int idx)
        {
            const float ratios[] = { 4.f, 8.f, 12.f, 20.f, 1000.f };
            if (dsp) dsp->setRatio (ratios[juce::jlimit (0, 4, idx)]);
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

        // Knob -> DSP wiring.
        knobs[0]->slider.onValueChange = [dsp,this]
        {
            // Input drives signal hot into the FET; map slider dB to threshold.
            // Slider 0 dB (top of face plate) = aggressive threshold; -60 = no comp.
            if (dsp) dsp->setThreshold ((float) knobs[0]->slider.getValue());
        };
        knobs[1]->slider.onValueChange = [dsp,this]
        {
            if (dsp) dsp->setGain ((float) knobs[1]->slider.getValue());
        };
        knobs[2]->slider.onValueChange = [dsp,this]
        {
            const int pos = juce::jlimit (0, 7, (int) std::round (knobs[2]->slider.getValue()));
            if (dsp) dsp->setAttack (kAttackMs[pos]);
        };
        knobs[3]->slider.onValueChange = [dsp,this]
        {
            const int pos = juce::jlimit (0, 7, (int) std::round (knobs[3]->slider.getValue()));
            if (dsp) dsp->setRelease (kReleaseMs[pos]);
        };

        if (dsp)
        {
            knobs[0]->slider.setValue (dsp->threshold, juce::sendNotificationSync);
            knobs[1]->slider.setValue (dsp->makeupDb,  juce::sendNotificationSync);
            // Best-effort sync from current ms back to the closest 1..7 position.
            int aPos = 4, rPos = 4;
            for (int i = 1; i < 8; ++i) if (std::abs (dsp->attackMs  - kAttackMs[i])  < std::abs (dsp->attackMs  - kAttackMs[aPos]))  aPos = i;
            for (int i = 1; i < 8; ++i) if (std::abs (dsp->releaseMs - kReleaseMs[i]) < std::abs (dsp->releaseMs - kReleaseMs[rPos])) rPos = i;
            knobs[2]->slider.setValue ((double) aPos, juce::sendNotificationSync);
            knobs[3]->slider.setValue ((double) rPos, juce::sendNotificationSync);
        }

        grMeter = std::make_unique<GRMeter>();
        addAndMakeVisible (*grMeter);
        startTimerHz (30);
    }

    ~FETCompressorPanel() override { stopTimer(); setLookAndFeel (nullptr); }

    void timerCallback() override
    {
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
// OptoCompressorPanel — H-7 (2026-05-01)
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

    explicit OptoCompressorPanel (CompressorDSP* dsp) : mDsp (dsp)
    {
        setLookAndFeel (&DynamicsLAF::get());
        setVolumeKnobVariant (true);

        // LA-2A face plate: Peak Reduction + Gain knobs both displayed 0..100.
        // Internal mapping inside the onValueChange lambdas converts that
        // 0..100 user-facing scale to the DSP's threshold/gain dB.
        buildKnobs (*this, knobs, {
            { "Peak Reduction", 0.f, 100.f, 30.f, 0.5f, "Peak Reduction -- more = more compression (LA-2A face plate scale)" },
            { "Gain",           0.f, 100.f, 50.f, 0.5f, "Output gain (LA-2A face plate scale)" },
        });
        for (auto& k : knobs)
            k->slider.getProperties().set (DynamicsLAF::kKnobVariant, "modernAnalog");

        // Comp/Limit 2-position vertical toggle, top-left of the panel.
        compLimitTog = std::make_unique<DualLabelToggle>();
        compLimitTog->setupNamed (
            "Comp",  "Compress mode (3:1 ratio)",
            "Limit", "Limit mode (effectively infinity:1 -- LA-2A 'limit' position)");
        compLimitTog->btn().setToggleState (dsp && dsp->ratio > 50.f, juce::dontSendNotification);
        compLimitTog->btn().onClick = [dsp, this]
        {
            const bool limit = compLimitTog->btn().getToggleState();
            if (dsp) dsp->setRatio (limit ? 100.0f : 3.0f);
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

struct CompressorPanel : public EditorPanelBase,
                         public juce::Timer
{
    std::unique_ptr<ChickenHeadSelector> kneeSel;
    std::unique_ptr<DualLabelToggle>     autoMuTog;
    std::unique_ptr<DualLabelToggle>     linkTog;
    std::unique_ptr<DualLabelToggle>     peakRmsTog;  // C3: Peak vs RMS detection
    std::unique_ptr<GRMeter>             grMeter;
    CompressorDSP*                       mDsp { nullptr };

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

        // KneeType — 8-position chicken-head selector
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
        knobs[7]->slider.onValueChange = [dsp,this]{ dsp->setLookaheadMs ((float)knobs[7]->slider.getValue()); };
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

    ~CompressorPanel() override
    {
        stopTimer();
        setLookAndFeel(nullptr);
    }

    void timerCallback() override
    {
        if (mDsp && grMeter) grMeter->setGainReduction(mDsp->getGainReductionDb());
    }

    // Layout: [VU] [GR] Thresh|Ratio|KneeW|Gain|Knee|Atk|Rel|Mix|LookA|Det|SCHPF [AutoMU/Link/PeakRMS toggles] [DBFS][Vol]
    void resized() override
    {
        auto b = getLocalBounds().reduced(4, 4);

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

        // Toggles laid out as two columns: Auto MU on its own (left), Link +
        // Peak/RMS stacked (right).
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

        // Knob strip: 11 slots (D.4-Q4: KneeW added between Ratio and Gain).
        // Thresh|Ratio|KneeW|Gain|KneeType|Atk|Rel|Mix|LookA|Det|SCHPF
        const int n     = 11;
        const int slotW = b.getWidth() / n;
        const int sz    = juce::jmin(slotW, b.getHeight(), kKnobSz);

        knobs[0]->setBounds(b.removeFromLeft(slotW).withSizeKeepingCentre(sz, sz));   // Thresh
        knobs[1]->setBounds(b.removeFromLeft(slotW).withSizeKeepingCentre(sz, sz));   // Ratio
        knobs[2]->setBounds(b.removeFromLeft(slotW).withSizeKeepingCentre(sz, sz));   // KneeW (D.4-Q4)
        knobs[3]->setBounds(b.removeFromLeft(slotW).withSizeKeepingCentre(sz, sz));   // Gain
        {
            auto slot = b.removeFromLeft(slotW);
            if (kneeSel) kneeSel->setBounds(slot.reduced(2));                          // KneeType chicken-head
        }
        knobs[4]->setBounds(b.removeFromLeft(slotW).withSizeKeepingCentre(sz, sz));   // Attack
        knobs[5]->setBounds(b.removeFromLeft(slotW).withSizeKeepingCentre(sz, sz));   // Release
        knobs[6]->setBounds(b.removeFromLeft(slotW).withSizeKeepingCentre(sz, sz));   // Mix
        knobs[7]->setBounds(b.removeFromLeft(slotW).withSizeKeepingCentre(sz, sz));   // LookA
        knobs[8]->setBounds(b.removeFromLeft(slotW).withSizeKeepingCentre(sz, sz));   // Det
        knobs[9]->setBounds(b.withSizeKeepingCentre(sz, sz));                          // SCHPF
    }

    void paint(juce::Graphics& g) override
    {
        DynamicsLAF::paintLA2APanel(g, getLocalBounds());
        // Knee labels are drawn by ChickenHeadSelector itself — no custom paint needed.
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// ReverbPanel — 5F-9 §8 expanded (2 rows, 8 knobs each + right-side extras)
// Row 1: Room | Decay | HFRatio | Diffuse | PreDly | WetTone | Wet | Dry
// Row 1 right: Mode combo + Freeze toggle switch
// Row 2: LoCut | HiCut | BassMlt | BassCross | TailDep | TailRt | Stereo | HiDamp
// Row 2 right: ER knob + TailShape combo + Sync + HiDmp toggle switches
// ─────────────────────────────────────────────────────────────────────────────
struct ReverbPanel : public EditorPanelBase
{
    std::vector<std::unique_ptr<VKnob>>         r1knobs, r2knobs;
    std::unique_ptr<VKnob>                      erKnob;       // row 2 right

    std::vector<VKnob*> getExtraKnobs() override
    {
        std::vector<VKnob*> v;
        for (auto& k : r1knobs) if (k) v.push_back(k.get());
        for (auto& k : r2knobs) if (k) v.push_back(k.get());
        if (erKnob) v.push_back(erKnob.get());
        return v;
    }
    std::unique_ptr<DualLabelToggle>            tempoTog, hdBypassTog, freezeTog;
    std::unique_ptr<ChickenHeadSelector>        modeSel, tailShapeSel, syncDivSel;
    JewelIndicator                              mJewel;

    explicit ReverbPanel(ReverbDSP* dsp)
    {
        disableVU();   // Reverb has no input VU — full knob width
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

        // ER knob (row 2 right side, compact — standalone since it's not in r2knobs)
        erKnob = std::make_unique<VKnob>("ER", -6.0f, "Early reflections level (dB)");
        erKnob->slider.setRange(-60.0, 12.0, 0.1);
        erKnob->slider.setValue(-6.0, juce::dontSendNotification);
        erKnob->slider.onValueChange = [dsp, this]{ dsp->setER((float)erKnob->slider.getValue()); };
        addAndMakeVisible(*erKnob);

        modeSel = std::make_unique<ChickenHeadSelector>();
        modeSel->setOptions({
            { "S", "Stereo", "Process left/right independently - classic stereo reverb" },
            { "M", "Mid",    "Process mid (L+R) only - side channel passes dry" },
            { "D", "Side",   "Process side (L-R) only - mid channel passes dry" },
        });
        modeSel->setBodyTooltip("Channel-processing mode");
        modeSel->onChange = [dsp](int idx){ dsp->setProcessingMode(idx); };
        addAndMakeVisible(*modeSel);

        // §8f tail-mod shape selector: sine / triangle / random S&H
        tailShapeSel = std::make_unique<ChickenHeadSelector>();
        tailShapeSel->setOptions({
            { "S", "Sine", "Smooth sinusoidal tail modulation" },
            { "T", "Tri",  "Triangle tail modulation - linear ramp, slightly more character" },
            { "R", "Rand", "Random sample-and-hold - organic, non-repeating wobble" },
        });
        tailShapeSel->setBodyTooltip("Tail modulation LFO shape");
        tailShapeSel->onChange = [dsp](int idx){ dsp->setTailModShape(idx); };
        addAndMakeVisible(*tailShapeSel);

        tempoTog = std::make_unique<DualLabelToggle>();
        tempoTog->setupOnOff("Sync", "Sync pre-delay to host BPM");
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
        // Initial lockout state -- tempoTog defaults off, so dropdown locked.
        syncDivSel->setLocked (true);
        addAndMakeVisible(*syncDivSel);

        hdBypassTog = std::make_unique<DualLabelToggle>();
        hdBypassTog->setupOnOff("HiDmp", "Bypass high-freq damping");
        hdBypassTog->btn().onClick = [dsp, this]{ dsp->setHighDampBypass(hdBypassTog->btn().getToggleState()); };
        hdBypassTog->setLabelColour(VC::Text);
        addAndMakeVisible(*hdBypassTog);

        // §8c Freeze toggle
        freezeTog = std::make_unique<DualLabelToggle>();
        freezeTog->setupOnOff("Freeze", "Freeze/infinite hold - hold the current reverb tail indefinitely");
        freezeTog->btn().onClick = [dsp, this]{ dsp->setFreeze(freezeTog->btn().getToggleState()); };
        freezeTog->setLabelColour(VC::Text);
        addAndMakeVisible(*freezeTog);

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
    }

    ~ReverbPanel() override { setLookAndFeel(nullptr); }

    void paint(juce::Graphics& g) override
    {
        TimeLAF::paintPultecPanel(g, getLocalBounds());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(4, 2);

        // Meter strips (Reverb has no input VU — disableVU() was called)
        if (vuIn) vuIn->setBounds(b.removeFromLeft(120).reduced(1, 2));
        dbfsOut      ->setBounds(b.removeFromRight(32).reduced(1, 2));
        b.removeFromRight(2);
        outputVolKnob->setBounds(b.removeFromRight(kKnobSz).withSizeKeepingCentre(kKnobSz, kKnobSz));
        b.removeFromRight(4);

        // Jewel: top-right of content area
        mJewel.setBounds(b.getRight() - 14, b.getY(), 12, 12);

        auto r1 = b.removeFromTop(b.getHeight() / 2);
        auto r2 = b;

        // H-9 (2026-05-02): right-side controls grid (visual right-to-left).
        // The chicken-heads stack vertically by column so each pair reads as
        // a unit:
        //   Col A (rightmost): Freeze toggle  /  HiDmp toggle
        //   Col B:             SyncDiv combo  /  Sync toggle
        //   Col C:             Mode combo     /  TailShape combo
        //   Row 2 only:        ER knob (no row-1 partner)
        // Matched widths per column so the rows align cleanly.
        constexpr int kTogW = 62;   // toggle column width
        constexpr int kCmbW = 66;   // chicken-head column width

        // ── Row 1 right (right-to-left): Freeze | SyncDiv | Mode ──────────
        auto freezeSlot = r1.removeFromRight(kTogW); r1.removeFromRight(2);
        if (freezeTog) freezeTog->setBounds(freezeSlot.reduced(1));
        auto syncDivSlot = r1.removeFromRight(kCmbW); r1.removeFromRight(2);
        if (syncDivSel)  syncDivSel->setBounds(syncDivSlot.reduced(2));
        auto modeSlot = r1.removeFromRight(kCmbW); r1.removeFromRight(2);
        if (modeSel)   modeSel->setBounds(modeSlot.reduced(2));
        layoutKnobsH(r1, r1knobs);

        // ── Row 2 right (right-to-left): HiDmp | Sync | TailShape | ER ────
        auto hiDmpSlot = r2.removeFromRight(kTogW); r2.removeFromRight(2);
        if (hdBypassTog) hdBypassTog->setBounds(hiDmpSlot.reduced(1));
        auto syncSlot = r2.removeFromRight(kCmbW); r2.removeFromRight(2);
        if (tempoTog)    tempoTog->setBounds(syncSlot.reduced(1));
        auto tailShapeSlot = r2.removeFromRight(kCmbW); r2.removeFromRight(2);
        if (tailShapeSel) tailShapeSel->setBounds(tailShapeSlot.reduced(2));
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

    void resized() override
    {
        auto b = getLocalBounds().reduced(4, 2);

        // Meter strips
        if (vuIn) vuIn->setBounds(b.removeFromLeft(120).reduced(1, 2));
        dbfsOut      ->setBounds(b.removeFromRight(32).reduced(1, 2));
        b.removeFromRight(2);
        outputVolKnob->setBounds(b.removeFromRight(kKnobSz).withSizeKeepingCentre(kKnobSz, kKnobSz));
        b.removeFromRight(4);

        auto r1 = b.removeFromTop(b.getHeight() / 2);
        auto r2 = b;

        // Row 1 right (right-to-left): OS chicken-head | Auto MU toggle | dB comp label
        auto osSlot = r1.removeFromRight(60); r1.removeFromRight(2);
        if (osSel) osSel->setBounds(osSlot.reduced(2));

        auto togSlot = r1.removeFromRight(52); r1.removeFromRight(2);
        if (autoGainTog) autoGainTog->setBounds(togSlot.reduced(1));

        auto labelSlot = r1.removeFromRight(38); r1.removeFromRight(2);
        if (autoGainCompLabel) autoGainCompLabel->setBounds(labelSlot.reduced(1));

        layoutKnobsH(r1, r1knobs);

        // Row 2: TubeType chicken-head + Transformer on/off toggle on right, knobs fill left.
        // H-7 (2026-05-01): Harmonic-mode chickenhead added at the right edge.
        auto hm = r2.removeFromRight(66); r2.removeFromRight(2);
        if (harmModeSel) harmModeSel->setBounds(hm.reduced(2));
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

    // Modulation panels use default VC::Panel background
    ~ChorusPanel() override { setLookAndFeel(nullptr); }

    explicit ChorusPanel(ChorusDSP* dsp)
    {
        disableVU();   // Chorus has no input VU — full knob width
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

        auto r1 = b.removeFromTop(b.getHeight() / 2);
        auto r2 = b;

        // Row 1: left 50% = freq knobs, right 50% = wave selectors
        int half = r1.getWidth() / 2;
        auto freqArea = r1.removeFromLeft(half);
        layoutKnobsH(freqArea, lfoFreqKnobs);
        int cw = r1.getWidth() / 3;
        for (int i = 0; i < 3; ++i)
            lfoWaveSel[i]->setBounds(r1.removeFromLeft(cw).reduced(2));

        // Row 2: right side stack = WetOnly (on/off) | CrossType (named) | Voices (named)
        const int togW = 66;
        auto wo  = r2.removeFromRight(togW); r2.removeFromRight(2);
        if (wetOnlyTog)   wetOnlyTog  ->setBounds(wo.reduced(1));
        auto ctc = r2.removeFromRight(togW); r2.removeFromRight(2);
        if (crossTypeTog) crossTypeTog->setBounds(ctc.reduced(1));
        auto vc  = r2.removeFromRight(togW); r2.removeFromRight(2);
        if (voicesTog)    voicesTog   ->setBounds(vc.reduced(1));
        layoutKnobsH(r2, r2knobs);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// DelayPanel  (2 rows)
// Row 1: Time | Feed | Wet | Dry | FBCut | Tone          +  Model combo  +  FBFilter combo
// Row 2: ModHz | ModFB | Diff | LoBit | FBDist | Spread | Pan  +  BPM  +  Pitch  +  FBDistType combo
// ─────────────────────────────────────────────────────────────────────────────
struct DelayPanel : public EditorPanelBase
{
    std::vector<std::unique_ptr<VKnob>>         r1knobs, r2knobs;
    std::unique_ptr<DualLabelToggle>            tempoTog, keepPitchTog, fbDistTypeTog;
    std::unique_ptr<ChickenHeadSelector>        modelSel, fbFilterTypeSel, syncDivSel;
    std::unique_ptr<juce::TextButton>           slapbackBtn;   // H-8 (2026-05-02)

    std::vector<VKnob*> getExtraKnobs() override
    {
        std::vector<VKnob*> v;
        for (auto& k : r1knobs) if (k) v.push_back(k.get());
        for (auto& k : r2knobs) if (k) v.push_back(k.get());
        return v;
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
    {
        disableVU();   // Delay has no input VU — full knob width
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
            { "Tone",   -1.f,    1.f,     0.f,  0.01f, "Tone (-=HP, +=LP)" },
        });

        // Row 2: ModHz | ModTime | ModFB | Diff | DiffSprd | LoBit | FBDst | FBKnee | FBSym | Spread | Pan | Smooth  (12 knobs)
        // ModTime placed next to ModHz (its LFO-rate dependency).  Smooth at the
        // end near the Pitch toggle (its keep-pitch dependency).
        buildKnobs(*this, r2knobs, {
            { "ModHz",    0.f,  20.f,  0.f,  0.1f,  "LFO modulation rate (Hz)" },
            { "ModTime",  0.f,   1.f,  0.f,  0.01f, "LFO depth on delay TIME (chorus / flange-into-delay character). Requires ModHz > 0 (left of this knob) to hear any change" },
            { "ModFB",    0.f,   1.f,  0.f,  0.01f, "LFO depth on feedback filter cutoff (+/- octaves)" },
            { "Diff",     0.f,   1.f,  0.f,  0.01f, "Diffusion level (allpass smearing)" },
            { "DiffSprd", 0.f,   1.f,  0.5f, 0.01f, "Diffusion spread - scales the 4 allpass base delays (tight 0 to wide 1)" },
            { "LoBit",    1.f,  24.f, 24.f,  0.5f,  "Lo-Fi bit depth (24=off)" },
            { "FBDst",    0.f,  10.f,  1.f,  0.1f,  "Feedback distortion drive" },
            { "FBKnee",   0.f,   1.f,  0.5f, 0.01f, "Limit-mode knee (0=hard clip, 1=smooth curve)" },
            { "FBSym",    0.f,   1.f,  0.f,  0.01f, "Sat-mode symmetry (0=symmetric, 1=asymmetric DC offset)" },
            { "Spread",   0.f,   1.f,  0.f,  0.01f, "Stereo spread (L/R base delay difference)" },
            { "Pan",     -1.f,   1.f,  0.f,  0.01f, "L/R offset pan" },
            { "Smooth",   0.f,   1.f,  0.5f, 0.01f, "Time smoothing - higher = slower transition when delay time changes. Only audible when the Pitch toggle (right of this row) is ON" },
            { "Duck",     0.f, 100.f,  0.f,  1.0f,  "0..100 %.  Sidechain ducking amount.  When dry input crosses threshold, wet output is attenuated by this amount.  0 = disabled (vocal-friendly setting around 60 %).  Threshold/attack/release at sane defaults (-24 dB / 10 ms / 200 ms)." },
        });

        modelSel = std::make_unique<ChickenHeadSelector>();
        modelSel->setOptions({
            { "S", "Stereo",   "Independent left/right delay lines - natural stereo" },
            { "M", "Mono",     "Summed mono delay - both channels get the same repeats" },
            { "P", "PingPong", "Ping-pong - repeats alternate between left and right" },
            { "O", "Off",      "Delay line disabled - only dry signal passes" },
        });
        modelSel->setBodyTooltip("Delay topology");
        // A9: initialize from DSP so preset-loaded model shows correctly.
        modelSel->setSelectedIndex(juce::jlimit(0, 3, dsp->getDelayModel()), juce::dontSendNotification);
        modelSel->onChange = [dsp](int idx){ dsp->setDelayModel(idx); };
        addAndMakeVisible(*modelSel);

        fbFilterTypeSel = std::make_unique<ChickenHeadSelector>();
        fbFilterTypeSel->setOptions({
            { "L", "FB LP", "Low-pass feedback filter - progressively darker repeats" },
            { "H", "FB HP", "High-pass feedback filter - thinner, brighter repeats" },
            { "B", "FB BP", "Band-pass feedback filter - narrows on each repeat" },
        });
        fbFilterTypeSel->setBodyTooltip("Feedback-path filter topology");
        // A9: initialize from DSP instead of hardcoded default so preset load reflects stored type.
        fbFilterTypeSel->setSelectedIndex(juce::jlimit(0, 2, dsp->getFBFilterType()), juce::dontSendNotification);
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

        // H-8 (2026-05-02): Slapback preset button.  Loads classic-slapback
        // values into the Echo Type (single short delay ~110 ms, ~12 % feedback,
        // 30 % wet, no diffusion).  After loading, re-syncs all panel knobs
        // from the DSP so the UI reflects the new state.
        slapbackBtn = std::make_unique<juce::TextButton>("Slap");
        slapbackBtn->setTooltip ("Slapback preset -- single short delay, low "
                                   "feedback, low wet.  Classic 50s-rockabilly / "
                                   "vintage-vocal effect.  Replaces current settings.");
        slapbackBtn->onClick = [this, dsp]
        {
            if (! dsp) return;
            dsp->presetSlapback();
            // Re-sync r1 + r2 knob sliders from the new DSP state so the UI
            // reflects the loaded preset.  setValue with sendNotificationSync
            // would re-trigger our onValueChange and write back to the DSP --
            // use dontSendNotification + the public DSP fields/getters.
            r1knobs[0]->slider.setValue (dsp->delayMs,             juce::dontSendNotification);
            r1knobs[1]->slider.setValue (dsp->getFeedbackLevel(),  juce::dontSendNotification);
            r1knobs[2]->slider.setValue (dsp->getLoFiSampleRate(), juce::dontSendNotification);
            r1knobs[3]->slider.setValue (dsp->getWetIn(),          juce::dontSendNotification);
            r1knobs[4]->slider.setValue (dsp->getWetOut(),         juce::dontSendNotification);
            r1knobs[5]->slider.setValue (dsp->getDryOut(),         juce::dontSendNotification);
            r1knobs[6]->slider.setValue (dsp->getFBCutoff(),       juce::dontSendNotification);
            r1knobs[7]->slider.setValue (dsp->getFBResonance(),    juce::dontSendNotification);
            r1knobs[8]->slider.setValue (dsp->getTone(),           juce::dontSendNotification);
            r2knobs[0] ->slider.setValue (dsp->getModRate(),        juce::dontSendNotification);
            r2knobs[1] ->slider.setValue (dsp->getModTimeMod(),     juce::dontSendNotification);
            r2knobs[2] ->slider.setValue (dsp->getModCutoffMod(),   juce::dontSendNotification);
            r2knobs[3] ->slider.setValue (dsp->getDiffLevel(),      juce::dontSendNotification);
            r2knobs[4] ->slider.setValue (dsp->getDiffSpread(),     juce::dontSendNotification);
            r2knobs[5] ->slider.setValue (dsp->getLoFiBits(),       juce::dontSendNotification);
            r2knobs[6] ->slider.setValue (dsp->getFBDistLevel(),    juce::dontSendNotification);
            r2knobs[7] ->slider.setValue (dsp->getFBDistKnee(),     juce::dontSendNotification);
            r2knobs[8] ->slider.setValue (dsp->getFBDistSymmetry(), juce::dontSendNotification);
            r2knobs[9] ->slider.setValue (dsp->getStereoSpread(),   juce::dontSendNotification);
            r2knobs[10]->slider.setValue (dsp->getOffsetPan(),      juce::dontSendNotification);
            r2knobs[11]->slider.setValue (dsp->getSmoothing(),      juce::dontSendNotification);
            // Selectors
            if (modelSel)        modelSel       ->setSelectedIndex (juce::jlimit (0, 3, dsp->getDelayModel()),   juce::dontSendNotification);
            if (fbFilterTypeSel) fbFilterTypeSel->setSelectedIndex (juce::jlimit (0, 2, dsp->getFBFilterType()), juce::dontSendNotification);
            if (tempoTog)        tempoTog->btn().setToggleState (dsp->syncBPM,                  juce::dontSendNotification);
            if (keepPitchTog)    keepPitchTog->btn().setToggleState (dsp->getKeepPitch(),       juce::dontSendNotification);
            if (fbDistTypeTog)   fbDistTypeTog->btn().setToggleState (dsp->getFBDistType() == 1, juce::dontSendNotification);
        };
        addAndMakeVisible(*slapbackBtn);

        // Row 1 bindings (9 knobs)
        r1knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setDelayMs           ((float)r1knobs[0]->slider.getValue()); };
        r1knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setFeedbackLevel     ((float)r1knobs[1]->slider.getValue()); };
        r1knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setLoFiSampleRate    ((float)r1knobs[2]->slider.getValue()); };
        r1knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setWetIn             ((float)r1knobs[3]->slider.getValue()); };
        r1knobs[4]->slider.onValueChange = [dsp,this]{ dsp->setWetOut            ((float)r1knobs[4]->slider.getValue()); };
        r1knobs[5]->slider.onValueChange = [dsp,this]{ dsp->setDryOut            ((float)r1knobs[5]->slider.getValue()); };
        r1knobs[6]->slider.onValueChange = [dsp,this]{ dsp->setFeedbackCutoff    ((float)r1knobs[6]->slider.getValue()); };
        r1knobs[7]->slider.onValueChange = [dsp,this]{ dsp->setFeedbackResonance ((float)r1knobs[7]->slider.getValue()); };
        r1knobs[8]->slider.onValueChange = [dsp,this]{ dsp->setTone              ((float)r1knobs[8]->slider.getValue()); };

        // Row 2 bindings (12 knobs — ModTime at idx 1, Smooth at idx 11)
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
        r1knobs[8]->slider.setValue(dsp->getTone(),              juce::sendNotificationSync);
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

    ~DelayPanel() override { setLookAndFeel(nullptr); }

    void paint(juce::Graphics& g) override
    {
        TimeLAF::paintPultecPanel(g, getLocalBounds());
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

        auto r1 = b.removeFromTop(b.getHeight() / 2);
        auto r2 = b;

        // Row 1: Model + FBFilter chicken-head selectors on right
        auto mc = r1.removeFromRight(66); r1.removeFromRight(2);
        if (modelSel) modelSel->setBounds(mc.reduced(2));
        auto fbc = r1.removeFromRight(66); r1.removeFromRight(2);
        if (fbFilterTypeSel) fbFilterTypeSel->setBounds(fbc.reduced(2));
        // H-8: Slapback preset button between FB filter selector and the knob row.
        auto slap = r1.removeFromRight(46); r1.removeFromRight(2);
        if (slapbackBtn) slapbackBtn->setBounds(slap.reduced(2, 4));
        layoutKnobsH(r1, r1knobs);

        // Row 2 right: SyncDiv chicken-head | BPM on/off | Pitch on/off | FBDistType named
        // (taken right-to-left; order left-to-right on screen = SyncDiv / BPM / Pitch / FBDist)
        auto fdt = r2.removeFromRight(62); r2.removeFromRight(2);
        if (fbDistTypeTog) fbDistTypeTog->setBounds(fdt.reduced(1));
        auto kp = r2.removeFromRight(62); r2.removeFromRight(2);
        if (keepPitchTog) keepPitchTog->setBounds(kp.reduced(1));
        auto tb = r2.removeFromRight(62); r2.removeFromRight(2);
        if (tempoTog) tempoTog->setBounds(tb.reduced(1));
        auto sd = r2.removeFromRight(66); r2.removeFromRight(2);
        if (syncDivSel) syncDivSel->setBounds(sd.reduced(2));
        layoutKnobsH(r2, r2knobs);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// VocalDoublerDelayPanel — H-8 (2026-05-02)
// Renders when DelayDSP::Type == VocalDoubler.  Single row of 6 knobs:
// Time L | Time R | Detune | Width | Rate | Mix.  Shares the TimeLAF look
// with the Echo DelayPanel so switching Mode keeps a coherent visual family.
// No feedback / lo-fi / mod LFO / diffusion -- VocalDoubler is fundamentally
// a different effect (dual short detuned taps, no FB).  Slapback button
// flips back to Echo Type and writes the slapback preset.
// ─────────────────────────────────────────────────────────────────────────────
struct VocalDoublerDelayPanel : public EditorPanelBase
{
    std::vector<std::unique_ptr<VKnob>> knobsRow;
    std::unique_ptr<juce::TextButton>    slapBtn;

    std::vector<VKnob*> getExtraKnobs() override
    {
        std::vector<VKnob*> v;
        for (auto& k : knobsRow) if (k) v.push_back (k.get());
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
                "(typically the lead vocal) -- when the source crosses threshold, the doubled "
                "signal is attenuated so it sits behind the lead.  0 = disabled.  No SC source "
                "= self-ducks on the doubler's own input." },
        });

        knobsRow[0]->slider.onValueChange = [dsp,this]{ dsp->setDoubleTimeLMs ((float) knobsRow[0]->slider.getValue()); };
        knobsRow[1]->slider.onValueChange = [dsp,this]{ dsp->setDoubleTimeRMs ((float) knobsRow[1]->slider.getValue()); };
        knobsRow[2]->slider.onValueChange = [dsp,this]{ dsp->setDoubleDetune  ((float) knobsRow[2]->slider.getValue()); };
        knobsRow[3]->slider.onValueChange = [dsp,this]{ dsp->setDoubleWidth   ((float) knobsRow[3]->slider.getValue()); };
        knobsRow[4]->slider.onValueChange = [dsp,this]{ dsp->setDoubleRate    ((float) knobsRow[4]->slider.getValue()); };
        knobsRow[5]->slider.onValueChange = [dsp,this]{ dsp->setWetOut        ((float) knobsRow[5]->slider.getValue() * 0.01f); };
        knobsRow[6]->slider.onValueChange = [dsp,this]{ dsp->setDuckAmount    ((float) knobsRow[6]->slider.getValue()); };

        if (dsp)
        {
            knobsRow[0]->slider.setValue (dsp->getDoubleTimeLMs(), juce::sendNotificationSync);
            knobsRow[1]->slider.setValue (dsp->getDoubleTimeRMs(), juce::sendNotificationSync);
            knobsRow[2]->slider.setValue (dsp->getDoubleDetune(),  juce::sendNotificationSync);
            knobsRow[3]->slider.setValue (dsp->getDoubleWidth(),   juce::sendNotificationSync);
            knobsRow[4]->slider.setValue (dsp->getDoubleRate(),    juce::sendNotificationSync);
            knobsRow[5]->slider.setValue (dsp->getWetOut() * 100.0f, juce::sendNotificationSync);
            knobsRow[6]->slider.setValue (dsp->getDuckAmount(),    juce::sendNotificationSync);
        }

        // H-8: Slapback preset button (matches the one on the Echo DelayPanel).
        // presetSlapback() flips Type back to Echo + writes the preset values;
        // we then ask the parent SlotComponent to re-mount so the Echo panel
        // shows the loaded settings.  Without the re-mount the user would be
        // stuck with VocalDoubler knobs while the DSP is now in Echo mode.
        slapBtn = std::make_unique<juce::TextButton>("Slap");
        slapBtn->setTooltip ("Slapback preset -- single short delay, low feedback, "
                              "low wet.  Switches to Echo Type and loads the classic "
                              "vintage-vocal effect (50s rockabilly / John Lennon "
                              "doubling).  Replaces current settings.");
        slapBtn->onClick = [this, dsp]
        {
            if (! dsp) return;
            dsp->presetSlapback();
            if (auto* sc = findParentComponentOfClass<SlotComponent>())
                sc->remountEditor();
        };
        addAndMakeVisible (*slapBtn);
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

        // Slap button on the right between Output and the knob row.
        if (slapBtn) slapBtn->setBounds (b.removeFromRight (46).reduced (2, 14));
        b.removeFromRight (4);

        layoutKnobsH (b, knobsRow, kKnobSz);
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
        disableVU();   // Flanger has no input VU — full knob width
        setLookAndFeel(&ModulationLAF::get());
        buildKnobs(*this, knobs, {
            { "Rate",   0.05f,    5.f,  0.5f,  0.01f, "LFO rate (Hz)" },
            { "Depth",  0.f,     10.f,  5.f,   0.1f,  "Sweep depth (ms)" },
            { "Delay",  0.f,     20.f,  0.f,   0.1f,  "Base (min) delay (ms)" },
            { "Feed",   0.f,    100.f, 50.f,   1.f,   "Feedback (%) - negative via InvFB toggle" },
            { "Phase",  0.f,    360.f,  0.f,   1.f,   "Stereo R-channel LFO offset (deg)" },
            { "Shape",  0.f,      1.f,  0.f,   0.01f, "LFO shape morph (0 = pure sine, 1 = pure triangle)" },
            { "DampHz", 200.f,20000.f,20000.f, 10.f,  "Feedback-damp low-pass cutoff (Hz). 20 kHz = off (transparent); lower = warmer feedback, tames harshness" },
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
        knobs[6]->slider.onValueChange = [dsp,this]{ dsp->setDampHz ((float)knobs[6]->slider.getValue()); };
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
        knobs[6]->slider.setValue(dsp->mDampHz,             juce::sendNotificationSync);
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

        // Strip meters (no input VU — disableVU() was called; still need DBFS + fader)
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
// Drive | Color | Band | Filter | Out | Wet   +  x100 toggle switch
// ─────────────────────────────────────────────────────────────────────────────
struct OverdrivePanel : public EditorPanelBase
{
    std::unique_ptr<DualLabelToggle>     x100Tog;
    std::unique_ptr<DualLabelToggle>     parallelTog;   // C4
    std::unique_ptr<ChickenHeadSelector> osSel;         // C5

    explicit OverdrivePanel(OverdriveDSP* dsp)
    {
        setLookAndFeel(&HarmonicLAF::get());

        buildKnobs(*this, knobs, {
            { "Drive",   0.f,   10.f,   5.f,  0.1f, "Pre-amp drive" },
            { "Color",  200.f, 8000.f,1000.f, 10.f, "BPF center frequency (Hz)" },
            { "Band",    0.f,   1.f,   0.5f,  0.01f,"BPF width (0=narrow, 1=wide)" },
            { "Bias",   -1.f,   1.f,   0.f,   0.01f,"Pre-shaper DC bias (adds even harmonics / tube-like warmth). 0 = symmetric (odd harmonics only)" },
            { "Filter", 500.f,18000.f,8000.f, 10.f, "Post LPF cutoff (Hz)" },
            { "Out",   -18.f,  18.f,   0.f,   0.5f, "Output gain (dB)" },
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

        // Strip meters (no input VU — Overdrive doesn't use one)
        if (vuIn) vuIn->setBounds(b.removeFromLeft(120).reduced(1, 2));
        dbfsOut      ->setBounds(b.removeFromRight(32).reduced(1, 2));
        b.removeFromRight(2);
        outputVolKnob->setBounds(b.removeFromRight(kKnobSz).withSizeKeepingCentre(kKnobSz, kKnobSz));
        b.removeFromRight(4);

        // Right sidebar (taken right-to-left; visual order L->R = OS | Parallel | x100)
        if (x100Tog)
        {
            auto col = b.removeFromRight(62); b.removeFromRight(2);
            x100Tog->setBounds(col.reduced(1));
        }
        if (parallelTog)
        {
            auto col = b.removeFromRight(68); b.removeFromRight(2);
            parallelTog->setBounds(col.reduced(1));
        }
        if (osSel)
        {
            auto col = b.removeFromRight(66); b.removeFromRight(2);
            osSel->setBounds(col.reduced(2));
        }
        b.removeFromRight(4);

        // Knobs fill remaining width
        layoutKnobsH(b, knobs);
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

        // Right sidebar, taken right-to-left so visual order (left-to-right) =
        // SyncDiv | Wave | Range | Stages | InvFB | BPM
        auto rc = b.removeFromRight(56); b.removeFromRight(2);
        if (rangeTog) rangeTog->setBounds(rc.reduced(1));
        auto sc = b.removeFromRight(60); b.removeFromRight(2);
        if (stagesSel) stagesSel->setBounds(sc.reduced(2));
        auto wc = b.removeFromRight(60); b.removeFromRight(2);
        if (waveSel) waveSel->setBounds(wc.reduced(2));
        auto sd = b.removeFromRight(60); b.removeFromRight(2);
        if (syncDivSel) syncDivSel->setBounds(sd.reduced(2));
        if (invFbTog)
        {
            auto it = b.removeFromRight(52); b.removeFromRight(2);
            invFbTog->setBounds(it.reduced(1));
        }
        if (bpmTog)
        {
            auto bt = b.removeFromRight(52); b.removeFromRight(2);
            bpmTog->setBounds(bt.reduced(1));
        }
        layoutKnobsH(b, knobs);
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

    explicit TransientShaperPanel(TransientShaperDSP* dsp)
    {
        setLookAndFeel(&DynamicsLAF::get());
        setVolumeKnobVariant(true);  // black knob for cream Dynamics panel

        // Row 1: primary shaping controls (unchanged 7 knobs)
        buildKnobs(*this, r1knobs, {
            { "Attack",  -100.f, 100.f,  50.f, 1.f,  "Attack shaping (-100=reduce, +100=enhance)" },
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
        r1knobs[1]->slider.setValue(dsp->mSustain * 100.0f, juce::sendNotificationSync);
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

        auto r1 = b.removeFromTop(b.getHeight() / 2);
        auto r2 = b;

        // Row 1 right: AttackShape + ReleaseShape chicken-heads (visual order: Attack | Release, right-to-left in removeFromRight)
        int r1sz = juce::jmin(r1.getHeight(), kKnobSz);
        auto rSlot = r1.removeFromRight(r1sz); r1.removeFromRight(4);
        if (releaseShapeSel) releaseShapeSel->setBounds(rSlot.reduced(2));
        auto aSlot = r1.removeFromRight(r1sz); r1.removeFromRight(4);
        if (attackShapeSel)  attackShapeSel->setBounds(aSlot.reduced(2));
        layoutKnobsH(r1, r1knobs);

        // Row 2 right: OS chicken-head + StereoDetect toggle (visual order: StereoDet | OS, right-to-left)
        auto osSlot = r2.removeFromRight(60); r2.removeFromRight(4);
        if (osSel) osSel->setBounds(osSlot.reduced(2));
        auto sdSlot = r2.removeFromRight(74); r2.removeFromRight(4);
        if (stereoDetectTog) stereoDetectTog->setBounds(sdSlot.reduced(1));
        layoutKnobsH(r2, r2knobs);
    }

    void paint(juce::Graphics& g) override
    {
        DynamicsLAF::paintLA2APanel(g, getLocalBounds());
        // Shape labels are drawn by ChickenHeadSelector itself — no custom paint needed.
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// TapePanel — Vibe | WowRate | WowDepth | InGain | Hiss  (OutGain removed — use output fader)
// ─────────────────────────────────────────────────────────────────────────────
struct TapePanel : public EditorPanelBase
{
    std::vector<std::unique_ptr<VKnob>>  r1knobs, r2knobs;
    std::unique_ptr<ChickenHeadSelector> tapeSpeedSel;   // C4
    std::unique_ptr<ChickenHeadSelector> osSel;          // C2

    std::vector<VKnob*> getExtraKnobs() override
    {
        std::vector<VKnob*> v;
        for (auto& k : r1knobs) if (k) v.push_back(k.get());
        for (auto& k : r2knobs) if (k) v.push_back(k.get());
        return v;
    }

    explicit TapePanel(TapeDSP* dsp)
    {
        setLookAndFeel(&HarmonicLAF::get());

        // Row 1: shaper character + input + hiss
        buildKnobs(*this, r1knobs, {
            { "Vibe",    0.f,  1.f,   0.5f, 0.01f, "Shaper asymmetry driver - k = 0.3 * Vibe in the sigmoid" },
            { "Hyst",    0.f,  2.f,   1.0f, 0.01f, "Hysteresis amount (C1) - 0 = no tape memory, 1 = default, 2 = strong memory" },
            { "Bias",    0.f, 10.f,   5.0f, 0.1f,  "Tape bias (C5) - 5 = neutral (zero DC offset); extremes inject even harmonics" },
            { "InGain",  0.f,  4.f,   1.0f, 0.01f, "Input gain (linear)" },
            { "Hiss",    0.f,  1.f,   0.0f, 0.01f, "Hiss level (pink-filtered + 200 Hz HPF, independent L/R) - default 0; raise to taste" },
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

        // OutGain removed - output volume is handled by the per-slot output fader (L2B).

        // C4 Tape speed chicken-head (3 positions wrap mTapeSpeed 0..1)
        tapeSpeedSel = std::make_unique<ChickenHeadSelector>();
        tapeSpeedSel->setOptions({
            { "7.5",  "7.5 ips", "7.5 ips cassette speed - slowest, squishiest, strongest hysteresis memory" },
            { "15",   "15 ips",  "15 ips pro reel-to-reel - balanced character (default)" },
            { "30",   "30 ips",  "30 ips mastering speed - fastest, cleanest, minimal hysteresis" },
        });
        tapeSpeedSel->setBodyTooltip("Tape speed - affects hysteresis memory time constant");
        // A9: map mTapeSpeed 0..1 to nearest of (0.0, 0.5, 1.0) -> idx 0/1/2.
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

        // C2 Oversampling factor chicken-head
        osSel = std::make_unique<ChickenHeadSelector>();
        osSel->setOptions({
            { "2x",  "2x OS",  "2x oversampling - lowest CPU, some alias from shaper at high drive" },
            { "4x",  "4x OS",  "4x oversampling (default) - balanced quality vs CPU" },
            { "8x",  "8x OS",  "8x oversampling - cleaner high-drive character, more CPU" },
            { "16x", "16x OS", "16x oversampling - maximum alias suppression" },
        });
        osSel->setBodyTooltip("Oversampling factor around shaper + hysteresis");
        osSel->setSelectedIndex(juce::jlimit(0, 3, dsp->getOsLog2() - 1),
                                juce::dontSendNotification);
        osSel->onChange = [dsp](int idx){ dsp->setOsLog2(juce::jlimit(1, 4, idx + 1)); };
        addAndMakeVisible(*osSel);

        // Row 1 knob bindings
        r1knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setVibe       ((float)r1knobs[0]->slider.getValue()); };
        r1knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setHystAmount ((float)r1knobs[1]->slider.getValue()); };
        r1knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setBias       ((float)r1knobs[2]->slider.getValue()); };
        r1knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setInputGain  ((float)r1knobs[3]->slider.getValue()); };
        r1knobs[4]->slider.onValueChange = [dsp,this]{ dsp->setHiss       ((float)r1knobs[4]->slider.getValue()); };

        // Row 2 knob bindings
        r2knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setWowRate      ((float)r2knobs[0]->slider.getValue()); };
        r2knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setWowDepth     ((float)r2knobs[1]->slider.getValue()); };
        r2knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setFlutterRate  ((float)r2knobs[2]->slider.getValue()); };
        r2knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setFlutterDepth ((float)r2knobs[3]->slider.getValue()); };
        r2knobs[4]->slider.onValueChange = [dsp,this]{ dsp->setPreShelfDb   ((float)r2knobs[4]->slider.getValue()); };
        r2knobs[5]->slider.onValueChange = [dsp,this]{ dsp->setDeShelfDb    ((float)r2knobs[5]->slider.getValue()); };

        // A9 slider sync (real value + clamp cleanup). Uses sendNotificationSync so the
        // existing onValueChange lambda fires -> calls the DSP setter with the slider's
        // (possibly clamped) value. If the DSP holds a value outside the slider's range,
        // the slider clamps it on setValue, and the setter call reconciles DSP to the
        // clamped value. Setter CPU guards make it a no-op when DSP and slider already
        // agree. Without this, sliders would show buildKnobs defaults while the DSP ran
        // with stale / preset-restored / out-of-range state.
        r1knobs[0]->slider.setValue (dsp->mVibe,         juce::sendNotificationSync);
        r1knobs[1]->slider.setValue (dsp->mHystAmount,   juce::sendNotificationSync);
        r1knobs[2]->slider.setValue (dsp->mBias,         juce::sendNotificationSync);
        r1knobs[3]->slider.setValue (dsp->mInputGain,    juce::sendNotificationSync);
        r1knobs[4]->slider.setValue (dsp->mHiss,         juce::sendNotificationSync);
        r2knobs[0]->slider.setValue (dsp->mWowRate,      juce::sendNotificationSync);
        r2knobs[1]->slider.setValue (dsp->mWowDepth,     juce::sendNotificationSync);
        r2knobs[2]->slider.setValue (dsp->mFlutterRate,  juce::sendNotificationSync);
        r2knobs[3]->slider.setValue (dsp->mFlutterDepth, juce::sendNotificationSync);
        r2knobs[4]->slider.setValue (dsp->mPreShelfDb,   juce::sendNotificationSync);
        r2knobs[5]->slider.setValue (dsp->mDeShelfDb,    juce::sendNotificationSync);
    }

    ~TapePanel() override
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

        auto r1 = b.removeFromTop(b.getHeight() / 2);
        auto r2 = b;

        // Row 1 right: OS chicken-head | Tape Speed chicken-head
        auto osSlot = r1.removeFromRight(60); r1.removeFromRight(2);
        if (osSel) osSel->setBounds(osSlot.reduced(2));
        auto tsSlot = r1.removeFromRight(62); r1.removeFromRight(2);
        if (tapeSpeedSel) tapeSpeedSel->setBounds(tsSlot.reduced(2));
        layoutKnobsH(r1, r1knobs);

        // Row 2: knobs only
        layoutKnobsH(r2, r2knobs);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// TapeSatPanel — H-10 (2026-05-02): Saturation umbrella's Tape sub-panel.
// Mirrors TapePanel's knob layout + chicken-heads exactly but binds to
// SaturationDSP's setTape* setters instead of the legacy TapeDSP class.
// SlotComponent's Mode dropdown picks Tube / Console / Tape; this panel
// is constructed when Tape is the active type.
// ─────────────────────────────────────────────────────────────────────────────
struct TapeSatPanel : public EditorPanelBase
{
    std::vector<std::unique_ptr<VKnob>>  r1knobs, r2knobs;
    std::unique_ptr<ChickenHeadSelector> tapeSpeedSel;   // C4
    std::unique_ptr<ChickenHeadSelector> osSel;          // C2

    std::vector<VKnob*> getExtraKnobs() override
    {
        std::vector<VKnob*> v;
        for (auto& k : r1knobs) if (k) v.push_back(k.get());
        for (auto& k : r2knobs) if (k) v.push_back(k.get());
        return v;
    }

    explicit TapeSatPanel(SaturationDSP* dsp)
    {
        setLookAndFeel(&HarmonicLAF::get());

        // Row 1: shaper character + input + hiss (matches TapePanel exactly)
        buildKnobs(*this, r1knobs, {
            { "Vibe",    0.f,  1.f,   0.5f, 0.01f, "Shaper asymmetry driver - k = 0.3 * Vibe in the sigmoid" },
            { "Hyst",    0.f,  2.f,   1.0f, 0.01f, "Hysteresis amount (C1) - 0 = no tape memory, 1 = default, 2 = strong memory" },
            { "Bias",    0.f, 10.f,   5.0f, 0.1f,  "Tape bias (C5) - 5 = neutral (zero DC offset); extremes inject even harmonics" },
            { "InGain",  0.f,  4.f,   1.0f, 0.01f, "Input gain (linear)" },
            { "Hiss",    0.f,  1.f,   0.0f, 0.01f, "Hiss level (pink-filtered + 200 Hz HPF, independent L/R) - default 0; raise to taste" },
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

        // Row 1 knob bindings -- call setTape* on SaturationDSP
        r1knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setTapeVibe       ((float)r1knobs[0]->slider.getValue()); };
        r1knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setTapeHystAmount ((float)r1knobs[1]->slider.getValue()); };
        r1knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setTapeBias       ((float)r1knobs[2]->slider.getValue()); };
        r1knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setTapeInputGain  ((float)r1knobs[3]->slider.getValue()); };
        r1knobs[4]->slider.onValueChange = [dsp,this]{ dsp->setTapeHiss       ((float)r1knobs[4]->slider.getValue()); };

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
        r1knobs[3]->slider.setValue (dsp->mTapeInputGain,    juce::sendNotificationSync);
        r1knobs[4]->slider.setValue (dsp->mTapeHiss,         juce::sendNotificationSync);
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

        auto r1 = b.removeFromTop(b.getHeight() / 2);
        auto r2 = b;

        // Row 1 right: OS chicken-head | Tape Speed chicken-head
        auto osSlot = r1.removeFromRight(60); r1.removeFromRight(2);
        if (osSel) osSel->setBounds(osSlot.reduced(2));
        auto tsSlot = r1.removeFromRight(62); r1.removeFromRight(2);
        if (tapeSpeedSel) tapeSpeedSel->setBounds(tsSlot.reduced(2));
        layoutKnobsH(r1, r1knobs);

        // Row 2: knobs only
        layoutKnobsH(r2, r2knobs);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// LimiterPanel (basic, functional — polished Limiter.txt UI is a separate task)
// Row 1: InGain | Ceil | SatTh | SatCv
// Row 2: Atk   | Rel  | Ahead | RelCv   + Auto toggle
// Also: GR meter (left of knobs, right of input VU)
// ─────────────────────────────────────────────────────────────────────────────
struct LimiterPanel : public EditorPanelBase,
                      public juce::Timer
{
    std::vector<std::unique_ptr<VKnob>> r1knobs, r2knobs;
    std::unique_ptr<DualLabelToggle>    autoRelTog;
    std::unique_ptr<DualLabelToggle>    autoMuTog;    // C4
    std::unique_ptr<DualLabelToggle>    linkTog;      // C5
    std::unique_ptr<GRMeter>            grMeter;
    LimiterDSP*                         mDsp { nullptr };

    std::vector<VKnob*> getExtraKnobs() override
    {
        std::vector<VKnob*> v;
        for (auto& k : r1knobs) if (k) v.push_back(k.get());
        for (auto& k : r2knobs) if (k) v.push_back(k.get());
        return v;
    }

    explicit LimiterPanel (LimiterDSP* dsp)
        : mDsp (dsp)
    {
        setLookAndFeel (&DynamicsLAF::get());
        setVolumeKnobVariant (true);   // black volume knob on cream Dynamics panel

        buildKnobs (*this, r1knobs, {
            { "InGain", -12.f,   24.f,   0.f,  0.1f,  "Input gain (dB)" },
            { "Ceil",   -24.f,    0.f,  -0.3f, 0.1f,  "Output ceiling (dB)" },
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

        // C4: Auto-makeup gain — post-limit boost by -ceilingDb for maximizer workflow.
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

        // GR meter (SSL-style — matches CompressorPanel)
        grMeter = std::make_unique<GRMeter>();
        addAndMakeVisible (*grMeter);

        startTimerHz (30);
    }

    ~LimiterPanel() override
    {
        stopTimer();
        setLookAndFeel (nullptr);
    }

    void timerCallback() override
    {
        if (mDsp && grMeter) grMeter->setGainReduction (mDsp->getGainReductionDb());
    }

    void paint (juce::Graphics& g) override
    {
        DynamicsLAF::paintLA2APanel (g, getLocalBounds());
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

        // Toggle layout: Auto MU as its own column LEFT of the Auto Release + Link
        // stack (same-size toggles, no overlap). Reserve two columns from the
        // right BEFORE splitting knob rows so both knob rows don't steal space.
        constexpr int kToggleColW = 62;
        auto togArea = b.removeFromRight (kToggleColW * 2 + 2);
        b.removeFromRight (2);

        // Right column: Auto Release (top half) + Link (bottom half), stacked.
        auto rightCol = togArea.removeFromRight (kToggleColW);
        const int halfH = rightCol.getHeight() / 2;
        auto autoRelSlot = rightCol.removeFromTop (halfH);
        if (autoRelTog) autoRelTog->setBounds (autoRelSlot.reduced (1));
        if (linkTog)    linkTog   ->setBounds (rightCol .reduced (1));

        // Left column (inside togArea after removing rightCol): Auto MU centered
        // vertically at the same height as one of the stacked toggles.
        togArea.removeFromRight (2);   // gutter between the two toggle columns
        auto autoMuSlot = togArea.withSizeKeepingCentre (kToggleColW, halfH);
        if (autoMuTog) autoMuTog->setBounds (autoMuSlot.reduced (1));

        auto r1 = b.removeFromTop (b.getHeight() / 2);
        auto r2 = b;
        layoutKnobsH (r1, r1knobs);
        layoutKnobsH (r2, r2knobs);
    }
};

// ── Factory ───────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
// ConsoleSaturationPanel — H-7 (2026-05-01)
// Minimalist preamp/console layout: Drive + Tone + Mix + Output.  Selected
// when SaturationDSP's mSatType == Console.  Tube-only fields (TubeType
// chickenhead, BassRelief, Sensitivity, Transformer) are not exposed here --
// Console mode interprets the underlying DSP differently and these knobs
// would be confusing.
// ─────────────────────────────────────────────────────────────────────────────
struct ConsoleSaturationPanel : public EditorPanelBase
{
    SaturationDSP* mDsp { nullptr };
    std::unique_ptr<ChickenHeadSelector> harmModeSel;   // Keep Low / Normal / Keep High

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

        // Harmonics-mode chickenhead on the right (before the meters strip).
        if (harmModeSel)
        {
            auto col = b.removeFromRight (66);
            harmModeSel->setBounds (col.reduced (2));
            b.removeFromRight (4);
        }

        layoutKnobsH (b, knobs, kKnobSz);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// DeEsserPanel — H-3/H-6 (2026-05-01)
// ─────────────────────────────────────────────────────────────────────────────
struct DeEsserPanel : public EditorPanelBase
{
    ~DeEsserPanel() override { setLookAndFeel (nullptr); }

    explicit DeEsserPanel (DeEsserDSP* dsp)
    {
        setLookAndFeel (&DynamicsLAF::get());

        // 8 knobs in a single horizontal row matching the Compressor / Limiter
        // visual vocabulary.  Mode / Listen / M-S toggles deferred to a polish
        // pass — defaults are preset-safe (Wide / Off / Stereo).
        buildKnobs (*this, knobs, {
            { "Freq",   4000.f, 12000.f, 6500.f, 10.f,  "Sidechain HPF cutoff (Hz). 4-12 kHz typical sibilance band" },
            { "Q",      0.5f,    4.0f,   1.4f,  0.05f, "Sidechain HPF Q. Higher = narrower detection band" },
            { "Thresh",-40.f,    0.f,  -24.f,   0.5f,  "Threshold (dB). Detector triggers above this level" },
            { "Range", -20.f,    0.f,  -12.f,   0.5f,  "Max reduction (dB). Caps how much the de-esser can pull down" },
            { "Atk",    0.1f,   30.f,   1.f,   0.1f,  "Attack (ms)" },
            { "Rel",   10.f,   500.f,  80.f,   1.f,   "Release (ms)" },
            { "Look",   0.f,     5.f,   0.f,   0.05f, "Lookahead (ms). Catches consonant before it lands" },
            { "Mix",    0.f,     1.f,   1.f,   0.01f, "Wet / dry blend" },
        });

        // DynamicsLAF "modernAnalog" knob variant -- matches Compressor +
        // Transient Shaper panels for visual consistency across all dynamics-
        // family effects.
        for (auto& k : knobs)
            k->slider.getProperties().set (DynamicsLAF::kKnobVariant, "modernAnalog");

        knobs[0]->slider.onValueChange = [dsp,this]{ dsp->setFrequencyHz ((float) knobs[0]->slider.getValue()); };
        knobs[1]->slider.onValueChange = [dsp,this]{ dsp->setQ           ((float) knobs[1]->slider.getValue()); };
        knobs[2]->slider.onValueChange = [dsp,this]{ dsp->setThresholdDb ((float) knobs[2]->slider.getValue()); };
        knobs[3]->slider.onValueChange = [dsp,this]{ dsp->setRangeDb     ((float) knobs[3]->slider.getValue()); };
        knobs[4]->slider.onValueChange = [dsp,this]{ dsp->setAttackMs    ((float) knobs[4]->slider.getValue()); };
        knobs[5]->slider.onValueChange = [dsp,this]{ dsp->setReleaseMs   ((float) knobs[5]->slider.getValue()); };
        knobs[6]->slider.onValueChange = [dsp,this]{ dsp->setLookaheadMs ((float) knobs[6]->slider.getValue()); };
        knobs[7]->slider.onValueChange = [dsp,this]{ dsp->setMix         ((float) knobs[7]->slider.getValue()); };

        // Sync slider visual state from current DSP state.
        knobs[0]->slider.setValue (dsp->mFreqHz,      juce::sendNotificationSync);
        knobs[1]->slider.setValue (dsp->mQ,           juce::sendNotificationSync);
        knobs[2]->slider.setValue (dsp->mThresholdDb, juce::sendNotificationSync);
        knobs[3]->slider.setValue (dsp->mRangeDb,     juce::sendNotificationSync);
        knobs[4]->slider.setValue (dsp->mAttackMs,    juce::sendNotificationSync);
        knobs[5]->slider.setValue (dsp->mReleaseMs,   juce::sendNotificationSync);
        knobs[6]->slider.setValue (dsp->mLookaheadMs, juce::sendNotificationSync);
        knobs[7]->slider.setValue (dsp->mMix,         juce::sendNotificationSync);
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

        layoutKnobsH (b, knobs, kKnobSz);
    }
};

std::unique_ptr<juce::Component> createEffectEditor(DSPBase* effect, EffectType type)
{
    if (!effect) return nullptr;

    switch (type)
    {
        case EffectType::Compressor:
        {
            // H-7 (2026-05-01): dispatch to the right inline panel based on
            // the DSP's character mode.  Modern = full SSL-ish layout; FET +
            // Opto get authentic 1176 / LA-2A minimal layouts.
            auto* c = static_cast<CompressorDSP*> (effect);
            switch (c->mType)
            {
                case CompressorDSP::Type::FET:    return std::make_unique<FETCompressorPanel>  (c);
                case CompressorDSP::Type::Opto:   return std::make_unique<OptoCompressorPanel> (c);
                case CompressorDSP::Type::Modern: default: return std::make_unique<CompressorPanel> (c);
            }
        }
        case EffectType::Reverb:
            return std::make_unique<ReverbPanel>        (static_cast<ReverbDSP*>        (effect));
        case EffectType::Chorus:
            return std::make_unique<ChorusPanel>        (static_cast<ChorusDSP*>        (effect));
        case EffectType::Delay:
        {
            // H-8 (2026-05-02): Echo Type = full DelayPanel; VocalDoubler Type
            // = minimal dual-tap layout (Time L / R / Detune / Width / Rate / Mix).
            auto* d = static_cast<DelayDSP*> (effect);
            return (d->getType() == (int) DelayDSP::Type::VocalDoubler)
                ? std::unique_ptr<juce::Component> (new VocalDoublerDelayPanel (d))
                : std::unique_ptr<juce::Component> (new DelayPanel             (d));
        }
        case EffectType::Saturation:
        {
            // H-7: Tube = full layout; Console = minimalist Drive/Color/Mix/Out.
            // H-10 (2026-05-02): Tape routes to TapeSatPanel -- mirrors the
            // legacy TapePanel layout but binds to SaturationDSP::setTape*.
            auto* s = static_cast<SaturationDSP*> (effect);
            switch (s->mSatType)
            {
                case SaturationDSP::Type::Console:
                    return std::unique_ptr<juce::Component> (new ConsoleSaturationPanel (s));
                case SaturationDSP::Type::Tape:
                    return std::unique_ptr<juce::Component> (new TapeSatPanel           (s));
                case SaturationDSP::Type::Tube:
                default:
                    return std::unique_ptr<juce::Component> (new SaturationPanel        (s));
            }
        }
        case EffectType::Flanger:
            return std::make_unique<FlangerPanel>       (static_cast<FlangerDSP*>       (effect));
        case EffectType::Overdrive:
            return std::make_unique<OverdrivePanel>     (static_cast<OverdriveDSP*>     (effect));
        case EffectType::Phaser:
            return std::make_unique<PhaserPanel>        (static_cast<PhaserDSP*>        (effect));
        case EffectType::TransientShaper:
            return std::make_unique<TransientShaperPanel>(static_cast<TransientShaperDSP*>(effect));
        case EffectType::Tape:
            // H-10 cutover (2026-05-02): EffectType::Tape is now an alias
            // for SaturationDSP+Type::Tape; the slot's effect ptr is a
            // SaturationDSP, not a TapeDSP.  Construct the Saturation-bound
            // TapeSatPanel so knob bindings hit setTape* on the right object.
            return std::make_unique<TapeSatPanel>       (static_cast<SaturationDSP*>    (effect));
        case EffectType::Limiter:
            return std::make_unique<LimiterPanel>       (static_cast<LimiterDSP*>       (effect));
        case EffectType::DeEsser:
            return std::make_unique<DeEsserPanel>       (static_cast<DeEsserDSP*>       (effect));
        default:
            return nullptr;
    }
}
