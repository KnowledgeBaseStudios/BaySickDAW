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
    // I-2 (2026-05-02): Universal pedal panel mode.
    //   * Full  — current FX-rack layout (all knobs visible).
    //   * Pedal — simplified pedalboard layout (subset of knobs, smaller
    //     footprint).  New pedal effects (I-5+) implement Full as a
    //     right-skewed cluster so the BaySickPedals view crops the empty
    //     left side; existing effects (I-14) get a separate Pedal layout.
    // Set by createEffectEditor before construction completes; panels read
    // `mPanelMode` in their constructor or first `resized()` to choose
    // layout.  Default is Full so untouched callers behave unchanged.
    enum class PanelMode { Full, Pedal };

    PanelMode mPanelMode { PanelMode::Full };

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
    // slotUuid:     stable per-slot identity from EffectRack::Slot::uuid.  C13:
    //               drives paramIds so reorder/pack-to-top preserves automation
    //               (UUID travels with the Slot via std::swap).  Empty uuid =
    //               no paramIds stamped (slot is empty or pre-C13 fallback).
    // Call after the panel is created and placed in a slot.
    void setSlotContext(const juce::String& channelPrefix, const juce::String& slotUuid);

    // Hook for panels that keep knobs in their own vectors (r1knobs, r2knobs)
    // instead of the base-class `knobs`. Return raw pointers to extra knobs so
    // setSlotContext() can stamp paramIds on them and register automation
    // applicators. Default implementation returns nothing; Chorus/Delay/Reverb/
    // Limiter/Saturation override this because they split their knobs across
    // two rows.
    virtual std::vector<VKnob*> getExtraKnobs() { return {}; }

    // H-7 (2026-05-01): hook fired when the slot's character mode (Compressor
    // Type / Saturation Type) changes via the SlotComponent's Mode dropdown,
    // and once at editor-mount time so the initial Type's layout is applied.
    // Default no-op; CompressorPanel + SaturationPanel override to show/hide
    // mode-specific knobs and re-layout.
    virtual void onTypeChanged() {}

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
