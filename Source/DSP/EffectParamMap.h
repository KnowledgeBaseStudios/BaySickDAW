#pragma once
#include <JuceHeader.h>
#include "../EffectRack.h"   // EffectType
class DSPBase;

// ─────────────────────────────────────────────────────────────────────────────
// EffectParamMap - QA-ProjectSave Task 7 step 3 (2026-07-26).
//
// THE single home for "which rack-effect parameters exist, what range each has,
// and how a value reaches (and is read back from) the DSP".
//
// Why this exists.  Automation used to apply by driving the on-screen KNOB, so a
// lane went silently dead whenever its panel was destroyed -- switching the
// Effects page to another channel was enough (verified 2026-07-26).  Fixing that
// means automation has to reach the DSP with no UI in the chain, which in turn
// means the knob->DSP math cannot live inside a panel's `onValueChange` lambda.
//
// The mapping is genuinely non-trivial in places -- the FET compressor's Input
// knob is a DRIVE control that inverts onto threshold, and Attack/Release are
// integer positions into a ms table -- so transcribing it into a second
// implementation for automation would have been two copies of load-bearing math
// drifting apart, with an ear test as the only way to notice.  Instead the math
// moved OUT of the UI: the panel and the automation applicator both call the
// functions here, so there is exactly one definition of what a knob means.
//
// Both directions are needed.  `apply` drives the DSP (UI drag + automation
// tick); `read` recovers the natural value from the DSP (panel sync on
// construction + seeding a new automation lane from the current value).  Panels
// already did the reverse map inline for their initial sync, which was the same
// duplication in the other direction.
//
// `suffix` matches the automation key that EditorPanelBase::setSlotContext
// derives from the knob's label -- lowercased, spaces and slashes to
// underscores.  Full paramIds look like "layers_bus_{uuid}_wet_dry"; only the
// trailing suffix is looked up here.
//
// Message thread only.  These call plain DSP setters; the audio thread reads the
// resulting fields as it always has.
// ─────────────────────────────────────────────────────────────────────────────
namespace EffectParamMap
{
    struct ParamDef
    {
        const char* suffix;                        // automation key tail
        float       lo;                            // natural range, also the
        float       hi;                            //   slider's range
        void      (*apply) (DSPBase*, float natural);
        float     (*read)  (const DSPBase*);       // natural value from the DSP
    };

    // ── Variant is part of the key, not an afterthought ──────────────────────
    // One EffectType can build SEVERAL panels with different knobs, ranges and
    // mappings: EffectType::Compressor dispatches on CompressorDSP::Type to
    // Modern / FET / Opto / CS, and Saturation, Overdrive, Delay and Reverb have
    // their own character-mode umbrellas.
    //
    // Keying on EffectType alone is not merely incomplete, it is ACTIVELY WRONG
    // (found 2026-07-26 by Jeff's re-test).  Different variants reuse knob
    // LABELS -- Modern's "Attack" is milliseconds while FET's is a switch
    // position 0..7 -- so they derive identical automation suffixes.  A
    // type-only table therefore applied one variant's mapping to another
    // variant's DSP: position "3" fed into a millisecond setter, audible as a
    // wrong-sounding, quieter compressor.
    //
    // `variant` is read from the DSP itself so no UI is needed to resolve it;
    // 0 for effects with a single panel.
    int variantOf (EffectType type, const DSPBase* dsp);

    juce::Span<const ParamDef> defsFor (EffectType type, int variant);

    const ParamDef* find (EffectType type, int variant, const juce::String& suffix);

    // Natural-domain apply -- what a UI drag calls (the slider already works in
    // natural units).  False when the suffix is unknown.
    bool applyNatural (EffectType type, int variant, DSPBase* dsp,
                       const juce::String& suffix, float natural);

    // Normalised-domain apply -- what an automation tick calls.  Converts 0..1
    // through the SAME range the slider uses, then applies the same mapping.
    bool applyNorm (EffectType type, int variant, DSPBase* dsp,
                   const juce::String& suffix, float v01);

    // Current value as 0..1 for seeding a lane / reading back state.  Returns
    // `fallback` when the suffix is unknown or the DSP is null.
    float readNorm (EffectType type, int variant, const DSPBase* dsp,
                    const juce::String& suffix, float fallback = 0.5f);
}
