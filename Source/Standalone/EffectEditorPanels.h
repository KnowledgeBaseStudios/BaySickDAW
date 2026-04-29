#pragma once
#include <JuceHeader.h>
#include "../DSP/DSPBase.h"
#include "../EffectRack.h"
#include "SharedUI.h"
#include "UndoActions.h"

// ── EditorPanelBase ───────────────────────────────────────────────────────────
// Base class for all effect editor panels. Provides:
//   - VU input meter (left)   — Compressor, Saturation, Overdrive, Tape, TransientShaper only
//   - DBFS output meter (right, compact) — all panels
//   - Output gain fader (far right)      — all panels
// Panels without VU (Delay, Reverb, Chorus, Flanger, Phaser) call disableVU()
// in their constructor so knobs use the full remaining width.
// ─────────────────────────────────────────────────────────────────────────────
struct EditorPanelBase : public juce::Component
{
    std::vector<std::unique_ptr<VKnob>>            knobs;
    std::vector<std::unique_ptr<juce::TextButton>> toggles;
    std::unique_ptr<juce::ComboBox>                combo;

    std::unique_ptr<VUMeter>      vuIn;       // null when disableVU() was called
    std::unique_ptr<DBFSMeter>    dbfsOut;
    std::unique_ptr<VKnob>        outputVolKnob;

    std::function<void(float db)> onOutputGainChanged;

    // Wire all knobs to the undo system.
    // Call this after the derived constructor has built the knobs vector.
    void setUndoContext(const UndoContext& ctx);

    // Stamp automation paramIds on all knobs.
    // channelPrefix: e.g. "layers_bus", "layer_1", "master"
    // slotIndex: 0-5 (which rack slot this panel occupies)
    // Call after the panel is created and placed in a slot.
    void setSlotContext(const juce::String& channelPrefix, int slotIndex);

    // Hook for panels that keep knobs in their own vectors (r1knobs, r2knobs)
    // instead of the base-class `knobs`. Return raw pointers to extra knobs so
    // setSlotContext() can stamp paramIds on them and register automation
    // applicators. Default implementation returns nothing; Chorus/Delay/Reverb/
    // Limiter/Saturation override this because they split their knobs across
    // two rows.
    virtual std::vector<VKnob*> getExtraKnobs() { return {}; }

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

    void setInputLevel (float rms01);
    void setOutputLevel(float dbfs);

    void resized() override;

private:
    UndoContext mUndoCtx;
};

// ── createEffectEditor ────────────────────────────────────────────────────────
// Factory: creates the inline VKnob editor panel for the given DSPBase subclass.
// Returns nullptr for EffectType::None.
// ─────────────────────────────────────────────────────────────────────────────
std::unique_ptr<juce::Component> createEffectEditor(DSPBase* effect, EffectType type);
