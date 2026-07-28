#pragma once
#include <JuceHeader.h>
#include "../DSP/DSPBase.h"
#include "../EffectRack.h"
#include "SharedUI.h"
#include "UndoActions.h"

// ── EditorPanelBase ───────────────────────────────────────────────────────────
// Base class for all effect editor panels. Provides:
//   - VU input meter (left)   - Compressor, Saturation, Overdrive, Tape, TransientShaper only
//   - DBFS output meter (right, compact) - all panels
//   - Output gain fader (far right)      - all panels
// Panels without VU (Delay, Reverb, Chorus, Flanger, Phaser) call disableVU()
// in their constructor so knobs use the full remaining width.
// ─────────────────────────────────────────────────────────────────────────────
struct EditorPanelBase : public juce::Component
{
    // I-2 (2026-05-02): Universal pedal panel mode.
    //   * Full  - current FX-rack layout (all knobs visible).
    //   * Pedal - simplified pedalboard layout (subset of knobs, smaller
    //     footprint).  New pedal effects (I-5+) implement Full as a
    //     right-skewed cluster so the BaySickPedals view crops the empty
    //     left side; existing effects (I-14) get a separate Pedal layout.
    // Set by createEffectEditor before construction completes; panels read
    // `mPanelMode` in their constructor or first `resized()` to choose
    // layout.  Default is Full so untouched callers behave unchanged.
    enum class PanelMode { Full, Pedal };

    PanelMode mPanelMode { PanelMode::Full };

    // QA-EffectsReview Task 1: Basic/Advanced disclosure flag (mirrors
    // mPanelMode).  When true the panel hides its "advanced" knobs/toggles and
    // re-flows; when false it shows the full control set.  Per-slot, persisted
    // with the project (default Basic).  Stamped by EffectsPage::rebuildSlotEditor
    // before the first resized(); flipped at runtime by the SlotComponent
    // Basic/Advanced header button, which then calls applyBasicMode().  Panels
    // with NO advanced controls (and the *PedalPanel board panels) ignore it.
    bool mBasicMode { true };

    // Re-apply mBasicMode: default just re-lays-out (a panel with no advanced
    // controls looks identical either way).  Panels with advanced controls
    // override to setVisible(false) on those children first, then resized().
    virtual void applyBasicMode() { resized(); }

    // QA-EffectsReview Task 1: does this panel have any "advanced" (non-reference)
    // controls to hide behind the Basic/Advanced toggle?  Default false -> the
    // SlotComponent shows NO toggle button (an effect with only reference
    // controls -- e.g. a Wah loaded into the rack -- gets no button).  Panels
    // override to true ONLY when they tag advanced knobs (Tasks 3-8).
    virtual bool hasAdvancedControls() const { return false; }

    std::vector<std::unique_ptr<VKnob>>            knobs;
    std::vector<std::unique_ptr<juce::TextButton>> toggles;
    std::unique_ptr<juce::ComboBox>                combo;

    std::unique_ptr<VUMeter>      vuIn;       // null when disableVU() was called
    std::unique_ptr<DBFSMeter>    dbfsOut;
    std::unique_ptr<VKnob>        outputVolKnob;

    std::function<void(float db)> onOutputGainChanged;

    // Fired by panels whose controls change the DSP's getLatencySamples()
    // (lookahead toggles/knobs, FFT-size switches) so the host can refresh
    // bus PDC (EffectsPage wires this to VibeGraph::updateBusLatencies).
    // Without the poke, changing lookahead left compensation stale until the
    // next effect load.
    std::function<void()> onLatencyChanged;

    // Wire all knobs to the undo system.
    // Call this after the derived constructor has built the knobs vector.
    void setUndoContext(const UndoContext& ctx);

    // Stamp automation paramIds on all knobs.
    // channelPrefix: e.g. "layers_bus", "layer_1", "master"
    // slotUuid:     stable per-slot identity from EffectRack::Slot::uuid.  C13:
    //               drives paramIds so reorder/pack-to-top preserves automation
    //               (UUID travels with the Slot via std::swap).  Empty uuid =
    //               no paramIds stamped (slot is empty or pre-C13 fallback).
    // Call after the panel is created and placed in a slot.
    //
    // QA-ModelShell TS3 (2026-07-27): this STAMPS ONLY.  It used to also
    // register the automation applicators, which is precisely why a lane died
    // whenever its panel did.  Registration is now a model-side act keyed to
    // the rack (EffectsPage::registerSlotAutomationFor) or the pedals board
    // (StandaloneEditor::registerPedalAutomation), so the ids stamped here are
    // answered by the registry whether this panel exists or not.
    void setSlotContext(const juce::String& channelPrefix, const juce::String& slotUuid);

    // Which DSP this panel drives, and under which EffectParamMap key.  Set by
    // createEffectEditor, which is the one place that knows all three.  Used by
    // the display refresh below to read values back out of the DSP.
    DSPBase*   mDsp        { nullptr };
    EffectType mEffectType { EffectType::None };
    int        mVariant    { 0 };

    // Hook for panels that keep knobs in their own vectors (r1knobs, r2knobs)
    // instead of the base-class `knobs`. Return raw pointers to extra knobs so
    // setSlotContext() can stamp paramIds on them and register automation
    // applicators. Default implementation returns nothing; Chorus/Delay/Reverb/
    // Limiter/Saturation override this because they split their knobs across
    // two rows.
    virtual std::vector<VKnob*> getExtraKnobs() { return {}; }

    // QA-EffectsReview Task 9: opt a toggle into the right-click "Automate"
    // system (first user: Reverb Freeze).  Call from the derived constructor;
    // setSlotContext() stamps the paramId (base + suffix) as componentID on
    // the wrapper + its children (GlobalAutoRightClick reads the clicked
    // component's id directly) and registers a 0/1 applicator + reader that
    // drive the button with sendNotification -- the toggle's own onClick
    // pushes to the DSP, mirroring how knob applicators drive the slider.
    void addAutomatableToggle(DualLabelToggle& tog, const juce::String& paramSuffix);
    struct AutoToggle { DualLabelToggle* tog; juce::String suffix; };
    std::vector<AutoToggle> mAutoToggles;

    // H-7 (2026-05-01): hook fired when the slot's character mode (Compressor
    // Type / Saturation Type) changes via the SlotComponent's Mode dropdown,
    // and once at editor-mount time so the initial Type's layout is applied.
    // Default no-op; CompressorPanel + SaturationPanel override to show/hide
    // mode-specific knobs and re-layout.
    virtual void onTypeChanged() {}

    // QA-F chain-wiring fix (2026-07-10): opt-in two-way APVTS binding for
    // hosts whose params are the source of truth (BaySickVocal's locked
    // chain: prefix "bsv_").  Overriding panels attach their controls to the
    // host's params so knob edits write params (instead of being stomped by
    // the host's per-block param push) and param/preset loads reflect back
    // into the controls.  EffectsPage racks never call this -- their panels
    // stay DSP-direct.  Default no-op.
    virtual void bindToApvts (juce::AudioProcessorValueTreeState& /*apvts*/,
                              const juce::String& /*prefix*/) {}

    // Call from derived constructor to select filmstrip variant:
    //   dark=true  → Volume Black (Dynamics panels)
    //   dark=false → Volume White (all others, default)
    void setVolumeKnobVariant(bool dark);

    EditorPanelBase();
    ~EditorPanelBase();

    // Forces VibeLAF onto any toggle-state button child so switch-toggle filmstrip fires
    // regardless of which custom LAF the derived panel sets on itself.
    void childrenChanged() override;

    // Call from derived constructor for panels that have no input VU meter
    void disableVU();

    // I-4 (2026-05-02): Call from derived constructor for panels that own their
    // own output level control (pedal-style panels: CS Style Compressor and
    // every I-5+ new pedal effect).  The base-class right-edge "Output Vol"
    // knob is hidden + freed so derived layouts can claim that horizontal
    // space.  dBFS meter stays.
    void disableOutputVolKnob();

    // I-15 (2026-05-03): pedal-mode finalize -- call when the panel is mounted
    // inside BaySickPedals to strip the dBFS output meter.  Pedal tiles do
    // not show level meters per locked spec.
    void disableDbfsMeter();

    // I-15 (2026-05-03): pedal-mode hook -- frees a panel-specific gain-
    // reduction meter (Compressor only).  Default no-op; CompressorPanel
    // / FETCompressorPanel / OptoCompressorPanel / CSStyleCompressorPanel
    // override to clear their own grMeter member.
    virtual void disableGrMeter() {}

    void setInputLevel (float rms01);
    void setOutputLevel(float dbfs);

    void resized() override;

private:
    UndoContext mUndoCtx;

    // ── Display refresh (QA-ModelShell TS3) ──────────────────────────────────
    // Rack panels are DSP-direct: no APVTS, no attachment, so nothing carries a
    // DSP change back to the knob.  That was invisible while automation drove
    // the KNOB, but automation now drives the DSP, so an automated control would
    // sit frozen at its last user position while the sound moved underneath it.
    // (Compressor + the per-slot output knob already had this since their
    // DSP-targeting applicators shipped.)  This walks the stamped controls and
    // re-reads them through EffectParamMap.
    //
    // Display ONLY: values are pushed with dontSendNotification, so this never
    // writes back to the DSP and cannot form a loop.  Skipped while the panel is
    // hidden and while the user is dragging, so it can never fight the mouse.
    // Timer-by-composition rather than inheritance -- several derived panels
    // already inherit juce::Timer themselves.
    struct DisplaySync : juce::Timer
    {
        std::function<void()> tick;
        void timerCallback() override { if (tick) tick(); }
    };
    std::unique_ptr<DisplaySync>                        mDisplaySync;
    std::vector<std::pair<VKnob*, juce::String>>        mSyncKnobs;    // control + map suffix
    void refreshControlsFromDsp();
};

// ── createEffectEditor ────────────────────────────────────────────────────────
// Factory: creates the inline VKnob editor panel for the given DSPBase subclass.
// Returns nullptr for EffectType::None.
//
// I-2 (2026-05-02): `mode` lets BaySickPedalsProcessor request the simplified
// pedal-mode layout for the 7 existing effects (Limiter / Saturation / Chorus /
// Flanger / Phaser / Delay / Reverb).  Regular FX-rack callers omit the arg
// and get the full layout.  The factory sets `panel->mPanelMode = mode` after
// construction so derived panels can read it in their first `resized()` to
// pick layout.  I-14 ships the actual Pedal-mode panel variants for those 7
// existing effects; for now Pedal mode produces the same panel as Full but
// with the flag set (panels treat it as Full until I-14 implementations land).
// ─────────────────────────────────────────────────────────────────────────────
std::unique_ptr<juce::Component> createEffectEditor (DSPBase* effect,
                                                     EffectType type,
                                                     EditorPanelBase::PanelMode mode = EditorPanelBase::PanelMode::Full);
