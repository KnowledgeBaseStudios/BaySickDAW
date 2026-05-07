#include "HarmlessProcessor.h"
#include "HarmlessEditor.h"
#include <cmath>

HarmlessProcessor::HarmlessProcessor (const juce::String& trackId)
    : juce::AudioProcessor (BusesProperties()
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HarmlessState",
             createLayout ("tk_" + trackId + "_harm_")),
      mTrackId (trackId),
      mPrefix  ("tk_" + trackId + "_harm_")
{
    // S4: register the 16 modulatable articulation targets. Param IDs baked
    // in now are permanent: renaming any of these in future = PRESET-BREAK.
    registerModTargets();

    // Hand the registry to the synth so all voices see the same pointer.
    mSynth.setModRegistry (&mModRegistry);

    // Pre-warm the parameter cache so the first processBlock (audio thread) finds
    // all values already matching the APVTS defaults and skips every setter.
    // Without this, all cache values start at -1 and the first block fires 13+
    // buildWavetable() calls on the audio thread, which involves juce::dsp::FFT
    // and can destabilise the RT audio thread on Windows.
    updateFromApvts();
}

// ── 2026-04-19 (S4) Articulation target registration ─────────────────────────
// Exactly 16 targets per spec. Order = UI dropdown order.
void HarmlessProcessor::registerModTargets()
{
    auto add = [this] (const char* suffix, const char* displayName)
    {
        mModRegistry.registerTarget (pid (suffix), juce::String (displayName));
    };
    add ("volume",          "Volume");
    add ("pan",             "Pan");
    add ("pitch_semitones", "Pitch");
    add ("timbre_blend",    "Timbre Blend");
    add ("pluck_decay",     "Pluck Decay");
    add ("prism_amount",    "Prism Amount");
    add ("blur_harm",       "Blur Harm");
    add ("flt_cutoff",      "Filter 1 Cutoff");
    add ("flt2_cutoff",     "Filter 2 Cutoff");
    add ("flt_res",         "Filter 1 Resonance");
    add ("flt2_res",        "Filter 2 Resonance");
    add ("ophaser_mix",     "Phaser Mix");
    add ("ophaser_width",   "Phaser Width");
    add ("unison_detune",   "Unison Pitch Thickness");
    add ("partA_level",     "Part A Level");
    add ("partB_level",     "Part B Level");
}

//==============================================================================
bool HarmlessProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void HarmlessProcessor::prepareToPlay (double sampleRate, int maxBlockSize)
{
    mSynth.prepare (sampleRate, maxBlockSize);
}

juce::AudioProcessorEditor* HarmlessProcessor::createEditor()
{
    return new HarmlessEditor (*this);
}

void HarmlessProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    updateFromApvts();

    // C.4 Phase 2.2: refresh engine-level SC RMS for any internal mod source.
    mScHelper.updateLevel (buffer.getNumSamples());

    // S4 Batch 2b: feed transport tempo to the synth so tempo-synced envelopes
    // and LFO rates track the host. Fall back to 120 BPM when no transport.
    double bps = 2.0;   // 120 BPM default
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                bps = (*bpm) / 60.0;
    }
    mSynth.setBeatsPerSecond (bps);

    const int auditNote = mAuditionNote.exchange (-1);
    if (auditNote >= 0 && auditNote <= 127)
    {
        midi.addEvent (juce::MidiMessage::noteOn  (1, auditNote, (juce::uint8) 100), 0);
        midi.addEvent (juce::MidiMessage::noteOff (1, auditNote),
                       juce::jmax (1, buffer.getNumSamples() - 1));
    }

    const int holdOffNote = mAuditionHoldOff.exchange (-1);
    const int holdOnNote  = mAuditionHoldOn .exchange (-1);
    if (holdOffNote >= 0 && holdOffNote <= 127)
        midi.addEvent (juce::MidiMessage::noteOff (1, holdOffNote), 0);
    if (holdOnNote  >= 0 && holdOnNote  <= 127)
        midi.addEvent (juce::MidiMessage::noteOn  (1, holdOnNote, (juce::uint8) 100), 0);

    mSynth.renderNextBlock (buffer, midi);

    // Flush NaN/Inf - a diverging SVF filter or malformed wavetable can produce
    // these, and Windows WASAPI will permanently silence the device stream if
    // any NaN reaches the audio hardware.
    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        float* d = buffer.getWritePointer (c);
        for (int s = buffer.getNumSamples(); --s >= 0;)
            if (!std::isfinite (d[s])) d[s] = 0.0f;
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
HarmlessProcessor::createLayout (const juce::String& p)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto vid = [](const juce::String& id) { return juce::ParameterID { id, 1 }; };

    // ── Timbre ────────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        vid (p + "timbre_shape"), "Timbre Shape",
        juce::StringArray { "Sine", "Saw", "Square", "Triangle" }, 1));
    // 2026-04-19 (S3): Part B waveform shape - independent of Part A.
    // Was hardcoded to Square in HarmlessSynth ctor. Now user-selectable via
    // the previously-display-only mTimbreWavB button.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        vid (p + "partB_timbre_shape"), "Part B Shape",
        juce::StringArray { "Sine", "Saw", "Square", "Triangle" }, 2));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "timbre_blend"), "Timbre Blend",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    // ── Parts A / B ───────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "partA_level"), "Part A Level",
        juce::NormalisableRange<float> (0.f, 1.f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "partB_level"), "Part B Level",
        juce::NormalisableRange<float> (0.f, 1.f), 0.0f));

    // ── Filter ────────────────────────────────────────────────────────────────
    // Skew 0.25 gives a more musical log-like response on the slider.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "flt_cutoff"), "Filter Cutoff",
        juce::NormalisableRange<float> (20.f, 20000.f, 0.f, 0.25f), 20000.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "flt_res"), "Filter Resonance",
        juce::NormalisableRange<float> (0.1f, 1.f), 0.7071f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "flt_env_amt"), "Filter Env Amount",
        juce::NormalisableRange<float> (-1.f, 1.f), 0.f));

    // ── Amp ADSR ──────────────────────────────────────────────────────────────
    auto adsr = [&] (const char* suffix, const char* label, float def)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            vid (p + suffix), label,
            juce::NormalisableRange<float> (0.001f, 10.f, 0.f, 0.3f), def));
    };

    adsr ("amp_a", "Amp Attack",  0.01f);
    adsr ("amp_d", "Amp Decay",   0.10f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "amp_s"), "Amp Sustain",
        juce::NormalisableRange<float> (0.f, 1.f), 0.8f));
    adsr ("amp_r", "Amp Release", 0.30f);

    // ── Filter ADSR ───────────────────────────────────────────────────────────
    adsr ("flt_a", "Filter Attack",  0.01f);
    adsr ("flt_d", "Filter Decay",   0.10f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "flt_s"), "Filter Sustain",
        juce::NormalisableRange<float> (0.f, 1.f), 0.5f));
    adsr ("flt_r", "Filter Release", 0.30f);

    // ── Unison ────────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterInt> (
        vid (p + "unison_voices"), "Unison Voices", 1, 9, 1));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "unison_detune"), "Unison Detune",
        juce::NormalisableRange<float> (0.f, 100.f), 15.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "unison_spread"), "Unison Spread",
        juce::NormalisableRange<float> (0.f, 1.f), 0.7f));

    // ── Spectral modules (registered now; wired in Beta session) ──────────────
    // 2026-04-19 (S2 SLA #14): Prism Amount goes bipolar - sign encodes
    // polarity, magnitude encodes strength. PRESET-BREAK pre-v1: old 0..1
    // values would only cover the +0..+1 half. Centre 0 = no prism.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "prism_amount"),       "Prism Amount",
        juce::NormalisableRange<float> (-1.f, 1.f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "partB_prism_amount"), "Prism Amount (B)",
        juce::NormalisableRange<float> (-1.f, 1.f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "pluck_decay"),        "Pluck Decay",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "partB_pluck_decay"),  "Pluck Decay (B)",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "blur_size"),          "Blur Size",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "partB_blur_size"),    "Blur Size (B)",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));
    // 2026-04-19 (S2 SLA #8/#9): Blur time + harm extension knobs.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "blur_time"), "Blur Time",
        juce::NormalisableRange<float> (0.f, 2.f), 1.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "partB_blur_time"), "Blur Time (B)",
        juce::NormalisableRange<float> (0.f, 2.f), 1.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "blur_harm"), "Blur Harm",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "partB_blur_harm"), "Blur Harm (B)",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "filter_mask_cutoff"), "Filter Mask Cutoff",
        juce::NormalisableRange<float> (20.f, 20000.f, 0.f, 0.25f), 5000.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "partB_filter_mask_cutoff"), "Filter Mask Cutoff (B)",
        juce::NormalisableRange<float> (20.f, 20000.f, 0.f, 0.25f), 5000.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "phaser_mask_rate"),   "Phaser Mask Rate",
        juce::NormalisableRange<float> (0.1f, 10.f), 1.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "partB_phaser_mask_rate"), "Phaser Mask Rate (B)",
        juce::NormalisableRange<float> (0.1f, 10.f), 1.f));

    // ── Brownian spectral weighting ───────────────────────────────────────────
    // 2026-04-19 (S3.5): existing params become "Part A" semantically; new
    // partB_* twins drive Part B's independent spectral chain. Editor swaps
    // attachments via the Part A/B button toggle. All defaults match so
    // first-load behaviour is identical to S3 (when partB starts at default
    // values that mirror partA, the user hears the same single-engine sound).
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "brownian_amount"), "Brownian Amount",
        juce::NormalisableRange<float> (0.f, 1.f), 1.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "partB_brownian_amount"), "Brownian Amount (B)",
        juce::NormalisableRange<float> (0.f, 1.f), 1.0f));

    // ── Beta: portamento / legato / strum ─────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "glide_time"), "Glide Time",
        juce::NormalisableRange<float> (0.f, 2.f, 0.f, 0.4f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        vid (p + "legato"), "Legato", false));

    // Session E - Cut Self: noteOn cuts prior voices playing the same note.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        vid (p + "cutSelf"), "Cut Self", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "strum_time"), "Strum Time",
        juce::NormalisableRange<float> (0.f, 0.5f, 0.f, 0.4f), 0.f));

    // ── Global ────────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "volume"), "Volume",
        juce::NormalisableRange<float> (0.f, 1.f), 0.8f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "pan"), "Pan",
        juce::NormalisableRange<float> (-1.f, 1.f), 0.f));

    // 2026-04-19 (S1): reverb_amount removed - VibeDAW's effects rack handles
    // reverb/chorus/delay/etc on the Layers bus. In-instrument FX violates the
    // rack-everywhere pattern. PRESET-SAFE (param had no DSP effect).

    // 2026-04-19 (S1): output EQ mix - was referenced in editor with attachment
    // commented out ("oeq_mix param not yet in APVTS"). Now registered + wired.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "oeq_mix"), "Out EQ Mix",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    // ── Phase init ────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "phase_start"), "Phase Start",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "phase_rand"), "Phase Rand",
        juce::NormalisableRange<float> (0.f, 1.f), 1.f));

    // ── Tremolo ───────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterInt> (
        vid (p + "trem_shape"), "Trem Shape", 0, 3, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "trem_depth"), "Trem Depth",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "trem_speed"), "Trem Speed",
        juce::NormalisableRange<float> (0.1f, 20.f, 0.f, 0.5f), 3.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "trem_gap"), "Trem Gap",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    // ── Vibrato ───────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterInt> (
        vid (p + "vib_shape"), "Vib Shape", 0, 3, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "vib_depth"), "Vib Depth",
        juce::NormalisableRange<float> (0.f, 2.f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "vib_speed"), "Vib Speed",
        juce::NormalisableRange<float> (0.1f, 20.f, 0.f, 0.5f), 5.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "vib_env"), "Vib Env",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    // ── Filter 1 extensions ───────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterInt> (
        vid (p + "flt1_type"), "Filter 1 Type", 0, 3, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "flt1_kb_track"), "Flt1 KB Track",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));
    // 2026-04-19 (S1): flt2_kb_track was referenced in editor's
    // mFilter2Row.attachToApvts() but never registered - SliderAttachment
    // ctor would have jasserted on missing param. Now present.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "flt2_kb_track"), "Flt2 KB Track",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));
    // 2026-04-19 (S2 SLA #34): per-filter cutoff offset in semitones (-24..+24).
    // Shifts the cutoff relative to the played note. Combined with kb_track
    // gives full keyboard-tracked filter behaviour.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "flt1_cutoff_ofs"), "Flt1 Cutoff Offset",
        juce::NormalisableRange<float> (-24.f, 24.f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "flt2_cutoff_ofs"), "Flt2 Cutoff Offset",
        juce::NormalisableRange<float> (-24.f, 24.f), 0.f));

    // ── Filter 2 ──────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterInt> (
        vid (p + "flt2_type"), "Filter 2 Type", 0, 3, 3));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "flt2_cutoff"), "Filter 2 Cutoff",
        juce::NormalisableRange<float> (20.f, 20000.f, 0.f, 0.25f), 20000.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "flt2_res"), "Filter 2 Res",
        juce::NormalisableRange<float> (0.1f, 1.f), 0.7071f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "flt2_env_amt"), "Filter 2 Env",
        juce::NormalisableRange<float> (-1.f, 1.f), 0.f));

    // ── Filter 2 ADSR ─────────────────────────────────────────────────────────
    adsr ("flt2_a", "Flt2 Attack",  0.01f);
    adsr ("flt2_d", "Flt2 Decay",   0.10f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "flt2_s"), "Flt2 Sustain",
        juce::NormalisableRange<float> (0.f, 1.f), 0.5f));
    adsr ("flt2_r", "Flt2 Release", 0.30f);

    // ── Pitch offset ──────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "pitch_semitones"), "Pitch Semitones",
        juce::NormalisableRange<float> (-24.f, 24.f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "pitch_cents"), "Pitch Cents",
        juce::NormalisableRange<float> (-100.f, 100.f), 0.f));
    // 2026-04-19 (SLA-Impl) #19 Pitch fraction selector. Multiplies the note's
    // fundamental Hz by one of 7 ratios (chicken-head selector in the editor).
    // Default index 0 = 1/1 (no change). Maps:
    //   0=1/1, 1=1/2, 2=1/4, 3=1/8, 4=x2, 5=x4, 6=x8
    // DSP applies this as a multiplier on mNoteHz before pitch_semitones+cents.
    layout.add (std::make_unique<juce::AudioParameterInt> (
        vid (p + "pitch_freq_frac"), "Pitch Fraction", 0, 6, 0));

    // ── Legato limit ──────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "legato_limit"), "Legato Limit",
        juce::NormalisableRange<float> (0.001f, 1.f, 0.f, 0.3f), 0.5f));

    // ── Unison extensions ─────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterInt> (
        vid (p + "unison_type"), "Unison Type", 0, 3, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        vid (p + "unison_alt"), "Unison Alt", false));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "unison_phase"), "Unison Phase",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    // ── Prism extensions ──────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterInt> (
        vid (p + "prism_mode"), "Prism Mode", 0, 2, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        vid (p + "partB_prism_mode"), "Prism Mode (B)", 0, 2, 0));

    // 2026-04-19 (S1): Harmonizer (6 params: harm_amt/width/str/ofs_step/shift/gap)
    // removed - module deferred to Tier 3 wishlist (T3 "Harmonizer module"). Per
    // Jeff's "wire it OR remove it (don't leave ghost params)" rule. Re-add
    // here as a block when T3 ships. PRESET-SAFE (params had no DSP).

    // ── Routing matrix (T2-F): 6 sliders driving timbre/oscillator routing ───
    // Per the design doc Top-Left Mixer/Routing matrix - 6 vertical sliders
    // sub/prot/clip/fx/vol/env that route the partial generator output through
    // sub-osc add, nyquist protection, output saturation, FX wet amount, master
    // gain, and amp-envelope depth respectively. Defaults preserve current
    // behaviour: sub/prot/clip = 0 (off), fx/vol/env = 1 (full).
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "rm_sub"),  "RM Sub",  juce::NormalisableRange<float> (0.f, 1.f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "rm_prot"), "RM Prot", juce::NormalisableRange<float> (0.f, 1.f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "rm_clip"), "RM Clip", juce::NormalisableRange<float> (0.f, 1.f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "rm_fx"),   "RM FX",   juce::NormalisableRange<float> (0.f, 1.f), 1.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "rm_vol"),  "RM Vol",  juce::NormalisableRange<float> (0.f, 1.f), 1.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "rm_env"),  "RM Env",  juce::NormalisableRange<float> (0.f, 1.f), 1.f));

    // ── Output phaser ─────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "ophaser_mix"), "Out Phaser Mix",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "ophaser_depth"), "Out Phaser Depth",
        juce::NormalisableRange<float> (0.f, 1.f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "ophaser_rate"), "Out Phaser Rate",
        juce::NormalisableRange<float> (0.01f, 10.f, 0.f, 0.5f), 1.f));
    // 2026-04-19 (SLA-Impl) Phaser WIDTH + OFS knobs from design doc.
    // WIDTH maps to JUCE phaser feedback (0..0.95 - controls regenerative width
    // of the sweep). Higher values = more pronounced phasing character.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "ophaser_width"), "Out Phaser Width",
        juce::NormalisableRange<float> (0.f, 0.95f), 0.5f));
    // OFS maps to JUCE phaser centre frequency (200..2000 Hz - sets the all-pass
    // corner around which the LFO sweeps).
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "ophaser_ofs"), "Out Phaser Offset",
        juce::NormalisableRange<float> (200.f, 2000.f, 0.f, 0.5f), 1000.f));

    // ── Strum extensions ──────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterInt> (
        vid (p + "strum_dir"), "Strum Dir", 0, 2, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "strum_tns"), "Strum Tension",
        juce::NormalisableRange<float> (-1.f, 1.f), 0.f));

    // 2026-04-20 (S4 Batch 4 fix): Harmor-authentic global LFO. One shared
    // rate+shape+tempo for the whole synth; per-target depth + curve warps.
    //
    // lfo_rate: index into the 13-step musical selector (1/8..32 beats/sec).
    //           Sub-beat values are fast, whole-beat are slow.
    // lfo_shape: 0 Sine, 1 Triangle, 2 Saw, 3 Square.
    // lfo_tempo: true = rate in beats, false = rate in seconds.
    layout.add (std::make_unique<juce::AudioParameterInt>  (vid (p + "lfo_rate"),  "LFO Rate",  0, 12, 7)); // default index 7 = 1 beat
    layout.add (std::make_unique<juce::AudioParameterInt>  (vid (p + "lfo_shape"), "LFO Shape", 0, 3, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (vid (p + "lfo_tempo"), "LFO Tempo Sync", true));

    // 2026-04-30: re-add T2-B LFO depth shortcuts that were ripped in S4.
    // Three global sliders that set the LFO source DEPTH on specific mod
    // registry targets (vol → Volume, pitch → Pitch).  Bipolar -1..+1 so
    // negative depths invert the LFO swing relative to the user's setting.
    // lfo_vel routes outside the registry - see HarmlessSynth note-on
    // velocity scaling below - because the registry has no Velocity target
    // (velocity is a one-shot at note-on, not a continuous modulator).
    // Replace semantics: moving these global sliders OVERWRITES any
    // per-target depth the user set in the in-player Mod Editor for that
    // destination.  Mirrors the existing applyGlobalLfoToAllTargets pattern
    // for lfo_rate/lfo_shape - global sliders clobber per-target settings.
    layout.add (std::make_unique<juce::AudioParameterFloat> (vid (p + "lfo_vel"),
        "LFO Vel Depth",   juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (vid (p + "lfo_vol"),
        "LFO Vol Depth",   juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (vid (p + "lfo_pitch"),
        "LFO Pitch Depth", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));

    // ── Part selector + vel link ──────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterInt> (
        vid (p + "part_sel"), "Part Select", 0, 1, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        vid (p + "vel_link"), "Vel Link", false));

    // ── XYZ mod pad ───────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "mod_x"), "Mod X",
        juce::NormalisableRange<float> (-1.f, 1.f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "mod_y"), "Mod Y",
        juce::NormalisableRange<float> (-1.f, 1.f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "mod_z"), "Mod Z",
        juce::NormalisableRange<float> (-1.f, 1.f), 0.f));
    // 2026-04-19 (S4): destination dropdowns removed. Mod X/Y/Z values now
    // feed the HarmlessModRegistry as ModSource::ModX/Y/Z for any target.
    // PRESET-BREAK pre-v1.

    // 2026-04-19 (S4 AG-1) Auto-gain mode: 0=Relative, 1=Absolute. Forwarded
    // to both HarmonicEngines; scaled pass applied in buildWavetable.
    layout.add (std::make_unique<juce::AudioParameterInt> (
        vid (p + "auto_gain_mode"), "Auto-Gain Mode", 0, 1, 0));

    // ── Pluck blur toggle ─────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterBool> (
        vid (p + "pluck_blur"), "Pluck Blur", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        vid (p + "partB_pluck_blur"), "Pluck Blur (B)", false));

    return layout;
}

//==============================================================================
void HarmlessProcessor::updateFromApvts()
{
    // Lambda helpers to keep the code concise.
    auto getf = [&] (const char* name) -> float
    {
        if (auto* p = apvts.getRawParameterValue (mPrefix + name))
            return p->load();
        return 0.0f;
    };
    auto geti = [&] (const char* name) -> int
    {
        return int (getf (name));
    };

    // ── Amp ADSR ──────────────────────────────────────────────────────────────
    const float aA = getf ("amp_a"), aD = getf ("amp_d"),
                aS = getf ("amp_s"), aR = getf ("amp_r");
    if (aA != mCache.ampA || aD != mCache.ampD ||
        aS != mCache.ampS || aR != mCache.ampR)
    {
        mSynth.setAmpEnv (aA, aD, aS, aR);
        mCache.ampA = aA; mCache.ampD = aD;
        mCache.ampS = aS; mCache.ampR = aR;
    }

    // ── Filter ────────────────────────────────────────────────────────────────
    const float fc = getf ("flt_cutoff"), fr = getf ("flt_res");
    if (fc != mCache.fltCutoff || fr != mCache.fltRes)
    {
        mSynth.setFilterParams (fc, fr);
        mCache.fltCutoff = fc;
        mCache.fltRes    = fr;
    }

    // ── Part levels ───────────────────────────────────────────────────────────
    const float pA = getf ("partA_level"), pB = getf ("partB_level");
    if (pA != mCache.partALevel || pB != mCache.partBLevel)
    {
        mSynth.setPartLevels (pA, pB);
        mCache.partALevel = pA;
        mCache.partBLevel = pB;
    }

    // ── Unison ────────────────────────────────────────────────────────────────
    const int   uv = geti ("unison_voices");
    const float ud = getf ("unison_detune"), us = getf ("unison_spread");
    if (uv != mCache.unisonVoices || ud != mCache.unisonDetune || us != mCache.unisonSpread)
    {
        mSynth.setUnison (uv, ud, us);
        mCache.unisonVoices = uv;
        mCache.unisonDetune = ud;
        mCache.unisonSpread = us;
    }

    // ── Volume / pan ──────────────────────────────────────────────────────────
    const float vol = getf ("volume");
    if (vol != mCache.volume) { mSynth.setVolume (vol); mCache.volume = vol; }

    const float pan = getf ("pan");
    if (pan != mCache.pan) { mSynth.setPan (pan); mCache.pan = pan; }

    // ── Timbre shape → rebuild Part A wavetable ───────────────────────────────
    // The shape selector drives the harmonic content of Part A.
    // buildWavetable() (~10µs) is guarded so it only fires on an actual change.
    // TODO (3A-Final): move this to a background thread for zero audio-thread cost.
    const int shape = geti ("timbre_shape");
    if (shape != mCache.timbreShape)
    {
        static constexpr HarmonicEngine::Shape kShapes[] = {
            HarmonicEngine::Shape::Sine,
            HarmonicEngine::Shape::Saw,
            HarmonicEngine::Shape::Square,
            HarmonicEngine::Shape::Triangle
        };
        mSynth.partA().setShape (kShapes[juce::jlimit (0, 3, shape)]);
        mCache.timbreShape = shape;
    }

    // ── Spectral modules ──────────────────────────────────────────────────────
    // These call buildWavetable() internally - guard against fire-every-block.

    // 2026-04-19 (S3.5): existing param reads now drive Part A only via the
    // *A setters; Part B has its own partB_* twins read further down.
    const float prism = getf ("prism_amount");
    if (prism != mCache.prismAmount)
    {
        mSynth.setPrismAmountA (prism);
        mCache.prismAmount = prism;
    }

    const float pluck = getf ("pluck_decay");
    if (pluck != mCache.pluckDecay)
    {
        mSynth.setPluckDecayA (pluck);
        mCache.pluckDecay = pluck;
    }

    const float blur = getf ("blur_size");
    if (blur != mCache.blurSize)
    {
        mSynth.setBlurSizeA (blur);
        mCache.blurSize = blur;
    }

    // filter_mask_cutoff (Hz 20–20000) → 0-1 amount (0 = all harmonics pass).
    const float fmc = getf ("filter_mask_cutoff");
    if (fmc != mCache.filterMaskCutoff)
    {
        const float normLog = std::log (fmc / 20.f) / std::log (20000.f / 20.f);
        mSynth.setFilterMaskAmountA (1.0f - juce::jlimit (0.f, 1.f, normLog));
        mCache.filterMaskCutoff = fmc;
    }

    const float pmr = getf ("phaser_mask_rate");
    if (pmr != mCache.phaserMaskRate)
    {
        mSynth.setPhaserMaskRateA (pmr);
        mCache.phaserMaskRate = pmr;
    }

    const float brown = getf ("brownian_amount");
    if (brown != mCache.brownianAmount)
    {
        mSynth.setBrownianAmountA (brown);
        mCache.brownianAmount = brown;
    }

    // ── Portamento / legato / strum ───────────────────────────────────────────
    const float glide = getf ("glide_time");
    if (glide != mCache.glideTime)
    {
        mSynth.setGlide (glide);
        mCache.glideTime = glide;
    }

    const int legato = geti ("legato");
    if (legato != mCache.legato)
    {
        mSynth.setLegato (legato != 0);
        mCache.legato = legato;
    }

    const int cutSelf = geti ("cutSelf");
    if (cutSelf != mCache.cutSelf)
    {
        mSynth.setCutSelf (cutSelf != 0);
        mCache.cutSelf = cutSelf;
    }

    const float strum = getf ("strum_time");
    if (strum != mCache.strumTime)
    {
        mSynth.setStrumTime (strum);
        mCache.strumTime = strum;
    }

    // timbre_blend, flt_env_amt, flt ADSR, reverb_amount:
    // registered in APVTS for automation - wired in 3A-Final.

    // ── Phase init ────────────────────────────────────────────────────────────
    const float phStart = getf ("phase_start"), phRand = getf ("phase_rand");
    if (phStart != mCache.phaseStart || phRand != mCache.phaseRand)
    {
        mSynth.setPhaseInit (phStart, phRand);
        mCache.phaseStart = phStart;
        mCache.phaseRand  = phRand;
    }

    // ── Tremolo ───────────────────────────────────────────────────────────────
    const int   tremSh = geti ("trem_shape");
    const float tremDp = getf ("trem_depth"), tremSp = getf ("trem_speed"),
                tremGp = getf ("trem_gap");
    if (tremSh != mCache.tremShape || tremDp != mCache.tremDepth ||
        tremSp != mCache.tremSpeed || tremGp != mCache.tremGap)
    {
        mSynth.setTremoloParams (tremSh, tremDp, tremSp, tremGp);
        mCache.tremShape = tremSh; mCache.tremDepth = tremDp;
        mCache.tremSpeed = tremSp; mCache.tremGap   = tremGp;
    }

    // ── Vibrato ───────────────────────────────────────────────────────────────
    const int   vibSh = geti ("vib_shape");
    const float vibDp = getf ("vib_depth"), vibSp = getf ("vib_speed"),
                vibEn = getf ("vib_env");
    if (vibSh != mCache.vibShape || vibDp != mCache.vibDepth ||
        vibSp != mCache.vibSpeed || vibEn != mCache.vibEnv)
    {
        mSynth.setVibratoParams (vibSh, vibDp, vibSp, vibEn);
        mCache.vibShape = vibSh; mCache.vibDepth = vibDp;
        mCache.vibSpeed = vibSp; mCache.vibEnv   = vibEn;
    }

    // ── Filter 2 ──────────────────────────────────────────────────────────────
    const float f2c = getf ("flt2_cutoff"), f2r = getf ("flt2_res");
    if (f2c != mCache.flt2Cutoff || f2r != mCache.flt2Res)
    {
        mSynth.setFilter2Params (f2c, f2r);
        mCache.flt2Cutoff = f2c;
        mCache.flt2Res    = f2r;
    }

    // ── Pitch offset ──────────────────────────────────────────────────────────
    const float pitchSt = getf ("pitch_semitones"), pitchCt = getf ("pitch_cents");
    if (pitchSt != mCache.pitchSemitones || pitchCt != mCache.pitchCents)
    {
        mSynth.setPitchOffset (pitchSt, pitchCt);
        mCache.pitchSemitones = pitchSt;
        mCache.pitchCents     = pitchCt;
    }

    // ── Output phaser ─────────────────────────────────────────────────────────
    const float opMx = getf ("ophaser_mix"), opDp = getf ("ophaser_depth"),
                opRt = getf ("ophaser_rate");
    if (opMx != mCache.ophaserMix || opDp != mCache.ophaserDepth ||
        opRt != mCache.ophaserRate)
    {
        mSynth.setOutputPhaserParams (opMx, opDp, opRt);
        mCache.ophaserMix   = opMx;
        mCache.ophaserDepth = opDp;
        mCache.ophaserRate  = opRt;
    }

    // ── Strum dir ─────────────────────────────────────────────────────────────
    const int sDr = geti ("strum_dir");
    if (sDr != mCache.strumDir)
    {
        mSynth.setStrumDir (sDr);
        mCache.strumDir = sDr;
    }

    // ── 2026-04-19 (S1) newly-wired ghost params + routing matrix ────────────

    // T2-N timbre_blend (A<->B crossfade multiplier on top of partA/B levels)
    const float tBlend = getf ("timbre_blend");
    if (tBlend != mCache.timbreBlend)
    {
        mSynth.setTimbreBlend (tBlend);
        mCache.timbreBlend = tBlend;
    }

    // T2-D prism_mode (S3.5: A-only)
    const int prMode = geti ("prism_mode");
    if (prMode != mCache.prismMode)
    {
        mSynth.setPrismModeA (prMode);
        mCache.prismMode = prMode;
    }

    // T2-N pluck_blur (S3.5: A-only)
    const int pBlur = geti ("pluck_blur");
    if (pBlur != mCache.pluckBlur)
    {
        mSynth.setPluckBlurA (pBlur != 0);
        mCache.pluckBlur = pBlur;
    }

    // T2-N strum_tns (tension on strum delay distribution curve)
    const float sTns = getf ("strum_tns");
    if (sTns != mCache.strumTns)
    {
        mSynth.setStrumTension (sTns);
        mCache.strumTns = sTns;
    }

    // T2-N vel_link (ON: velocity scales both Part A + B equally)
    const int vLink = geti ("vel_link");
    if (vLink != mCache.velLink)
    {
        mSynth.setVelLink (vLink != 0);
        mCache.velLink = vLink;
    }

    // T2-N legato_limit (legato release-time threshold)
    const float lLimit = getf ("legato_limit");
    if (lLimit != mCache.legatoLimit)
    {
        mSynth.setLegatoLimit (lLimit);
        mCache.legatoLimit = lLimit;
    }

    // T1a oeq_mix (output EQ mix - tilt EQ amount on synth output)
    const float oeq = getf ("oeq_mix");
    if (oeq != mCache.oeqMix)
    {
        mSynth.setOutputEQMix (oeq);
        mCache.oeqMix = oeq;
    }

    // T1c filter type wiring (LP/HP/BP/Notch) for filter 1 + filter 2
    const int f1Type = geti ("flt1_type");
    if (f1Type != mCache.flt1Type)
    {
        mSynth.setFilterType (f1Type);
        mCache.flt1Type = f1Type;
    }
    const int f2Type = geti ("flt2_type");
    if (f2Type != mCache.flt2Type)
    {
        mSynth.setFilter2Type (f2Type);
        mCache.flt2Type = f2Type;
    }

    // S1's flt2_kb_track stub removed; S2 wires both flt1_kb_track and
    // flt2_kb_track to actual DSP further down via setFilter1KbTrack /
    // setFilter2KbTrack alongside filter envelopes + cutoff offsets.

    // T2-F routing matrix (6 sliders, broadcast as one set so the synth can
    // recompute derived gains/mixes once per change).
    const float rmS = getf ("rm_sub"),  rmP = getf ("rm_prot"),
                rmC = getf ("rm_clip"), rmF = getf ("rm_fx"),
                rmV = getf ("rm_vol"),  rmE = getf ("rm_env");
    if (rmS != mCache.rmSub  || rmP != mCache.rmProt || rmC != mCache.rmClip
     || rmF != mCache.rmFx   || rmV != mCache.rmVol  || rmE != mCache.rmEnv)
    {
        mSynth.setRoutingMatrix (rmS, rmP, rmC, rmF, rmV, rmE);
        mCache.rmSub  = rmS; mCache.rmProt = rmP; mCache.rmClip = rmC;
        mCache.rmFx   = rmF; mCache.rmVol  = rmV; mCache.rmEnv  = rmE;
    }

    // ── 2026-04-19 (SLA-Impl) - Pitch fraction + Phaser extras ──────────────
    const int pFrac = geti ("pitch_freq_frac");
    if (pFrac != mCache.pitchFreqFrac)
    {
        mSynth.setPitchFraction (pFrac);
        mCache.pitchFreqFrac = pFrac;
    }
    const float opW = getf ("ophaser_width");
    const float opO = getf ("ophaser_ofs");
    if (opW != mCache.ophaserWidth || opO != mCache.ophaserOfs)
    {
        mSynth.setOutputPhaserExtras (opW, opO);
        mCache.ophaserWidth = opW;
        mCache.ophaserOfs   = opO;
    }

    // ── 2026-04-19 (S2) Filter envelopes + ofs + kb track ────────────────────
    const float fA = getf ("flt_a"),  fD = getf ("flt_d"),
                fS = getf ("flt_s"),  fR = getf ("flt_r");
    if (fA != mCache.fltA || fD != mCache.fltD || fS != mCache.fltS || fR != mCache.fltR)
    {
        mSynth.setFilter1Env (fA, fD, fS, fR);
        mCache.fltA = fA; mCache.fltD = fD; mCache.fltS = fS; mCache.fltR = fR;
    }
    const float fEa = getf ("flt_env_amt");
    if (fEa != mCache.fltEnvAmt)
    {
        mSynth.setFilter1EnvAmt (fEa);
        mCache.fltEnvAmt = fEa;
    }
    const float f2A = getf ("flt2_a"), f2D = getf ("flt2_d"),
                f2S = getf ("flt2_s"), f2R = getf ("flt2_r");
    if (f2A != mCache.flt2A || f2D != mCache.flt2D || f2S != mCache.flt2S || f2R != mCache.flt2R)
    {
        mSynth.setFilter2Env (f2A, f2D, f2S, f2R);
        mCache.flt2A = f2A; mCache.flt2D = f2D; mCache.flt2S = f2S; mCache.flt2R = f2R;
    }
    const float f2Ea = getf ("flt2_env_amt");
    if (f2Ea != mCache.flt2EnvAmt)
    {
        mSynth.setFilter2EnvAmt (f2Ea);
        mCache.flt2EnvAmt = f2Ea;
    }
    const float f1Of = getf ("flt1_cutoff_ofs"), f2Of = getf ("flt2_cutoff_ofs");
    if (f1Of != mCache.flt1CutoffOfs)
    {
        mSynth.setFilter1CutoffOfs (f1Of);
        mCache.flt1CutoffOfs = f1Of;
    }
    if (f2Of != mCache.flt2CutoffOfs)
    {
        mSynth.setFilter2CutoffOfs (f2Of);
        mCache.flt2CutoffOfs = f2Of;
    }
    const float f1Kb = getf ("flt1_kb_track"), f2Kb = getf ("flt2_kb_track");
    if (f1Kb != mCache.flt1KbTrack)
    {
        mSynth.setFilter1KbTrack (f1Kb);
        mCache.flt1KbTrack = f1Kb;
    }
    if (f2Kb != mCache.flt2KbTrack)
    {
        mSynth.setFilter2KbTrack (f2Kb);
        mCache.flt2KbTrack = f2Kb;
    }

    // ── 2026-04-19 (S4) Mod XYZ pad input (destinations ripped) ──────────────
    const float mX = getf ("mod_x"), mY = getf ("mod_y"), mZ = getf ("mod_z");
    if (mX != mCache.modX || mY != mCache.modY || mZ != mCache.modZ)
    {
        mSynth.setModXYZ (mX, mY, mZ);
        mCache.modX = mX; mCache.modY = mY; mCache.modZ = mZ;
    }

    // ── 2026-04-19 (S4 AG-1) Auto-gain mode ──────────────────────────────────
    const int agMode = geti ("auto_gain_mode");
    if (agMode != mCache.autoGainMode)
    {
        mSynth.setAutoGainMode (agMode);
        mCache.autoGainMode = agMode;
    }

    // ── 2026-04-20 (S4 Batch 4 fix) Global LFO ───────────────────────────────
    const int   lfoRateIdx = geti ("lfo_rate");
    const int   lfoShape   = geti ("lfo_shape");
    const bool  lfoTempo   = (geti ("lfo_tempo") != 0);
    if (lfoRateIdx != mCache.lfoRateIdx
        || lfoShape != mCache.lfoShape
        || lfoTempo != (mCache.lfoTempo != 0))
    {
        mSynth.applyGlobalLfoToAllTargets (lfoRateIdx, lfoShape, lfoTempo);
        mCache.lfoRateIdx = lfoRateIdx;
        mCache.lfoShape   = lfoShape;
        mCache.lfoTempo   = lfoTempo ? 1 : 0;

        // 2026-04-30: T2-B lfo_vel uses a separate global LFO clock since
        // velocity isn't a mod registry target.  Mirror the rate/shape into
        // HarmlessSynth so its noteOn-velocity-scaling LFO ticks at the
        // same period as the registry's LFO.  Beats↔seconds conversion
        // happens at tick time using the latest bps so BPM changes track.
        static const float kLens[13] = {
            0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f,
            1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f
        };
        const float cycleLen = kLens[juce::jlimit (0, 12, lfoRateIdx)];
        mSynth.setGlobalLfoVelRate (cycleLen, lfoShape, lfoTempo);
    }

    // 2026-04-30: T2-B LFO depth shortcuts.  Bipolar -1..+1.  Each routes
    // to one specific destination - vol→Volume target, pitch→Pitch target,
    // vel→note-on velocity scaling (handled inside HarmlessSynth).
    const float lfoVel   = getf ("lfo_vel");
    const float lfoVol   = getf ("lfo_vol");
    const float lfoPitch = getf ("lfo_pitch");
    if (lfoVel   != mCache.lfoVel)   { mSynth.setGlobalLfoVelDepth   (lfoVel);   mCache.lfoVel   = lfoVel; }
    if (lfoVol   != mCache.lfoVol)   { mSynth.applyGlobalLfoVolDepth (lfoVol);   mCache.lfoVol   = lfoVol; }
    if (lfoPitch != mCache.lfoPitch) { mSynth.applyGlobalLfoPitchDepth (lfoPitch); mCache.lfoPitch = lfoPitch; }

    // ── 2026-04-19 (S2 SLA) Blur extensions (S3.5: A-only) ───────────────────
    const float bT = getf ("blur_time"), bH = getf ("blur_harm");
    if (bT != mCache.blurTime)
    {
        mSynth.setBlurTimeA (bT);
        mCache.blurTime = bT;
    }
    if (bH != mCache.blurHarm)
    {
        mSynth.setBlurHarmA (bH);
        mCache.blurHarm = bH;
    }

    // ── 2026-04-19 (S3) Unison Type / Alt / Phase + Part B shape ─────────────
    // Wires the existing unison_type/unison_alt/unison_phase ghost params from
    // S1 and the new partB_timbre_shape param. unison_voices/detune/spread are
    // already wired earlier via setUnison(int, float, float).
    const int uT = geti ("unison_type");
    if (uT != mCache.unisonTypeS3)
    {
        mSynth.setUnisonType (uT);
        mCache.unisonTypeS3 = uT;
    }
    const int uA = geti ("unison_alt");
    if (uA != mCache.unisonAltS3)
    {
        mSynth.setUnisonAlt (uA != 0);
        mCache.unisonAltS3 = uA;
    }
    const float uP = getf ("unison_phase");
    if (uP != mCache.unisonPhaseS3)
    {
        mSynth.setUnisonPhase (uP);
        mCache.unisonPhaseS3 = uP;
    }
    const int pBs = geti ("partB_timbre_shape");
    if (pBs != mCache.partBShape)
    {
        mSynth.setPartBShape (pBs);
        mCache.partBShape = pBs;
    }

    // ── 2026-04-19 (S3.5) Per-part Part B wavetable-domain reads ─────────────
    const float pbBr = getf ("partB_brownian_amount");
    if (pbBr != mCache.pBBrownian) { mSynth.setBrownianAmountB (pbBr); mCache.pBBrownian = pbBr; }
    const float pbBs = getf ("partB_blur_size");
    if (pbBs != mCache.pBBlurSize) { mSynth.setBlurSizeB (pbBs); mCache.pBBlurSize = pbBs; }
    const float pbBt = getf ("partB_blur_time");
    if (pbBt != mCache.pBBlurTime) { mSynth.setBlurTimeB (pbBt); mCache.pBBlurTime = pbBt; }
    const float pbBh = getf ("partB_blur_harm");
    if (pbBh != mCache.pBBlurHarm) { mSynth.setBlurHarmB (pbBh); mCache.pBBlurHarm = pbBh; }
    const float pbPa = getf ("partB_prism_amount");
    if (pbPa != mCache.pBPrismAmount) { mSynth.setPrismAmountB (pbPa); mCache.pBPrismAmount = pbPa; }
    const int   pbPm = geti ("partB_prism_mode");
    if (pbPm != mCache.pBPrismMode) { mSynth.setPrismModeB (pbPm); mCache.pBPrismMode = pbPm; }
    const float pbPd = getf ("partB_pluck_decay");
    if (pbPd != mCache.pBPluckDecay) { mSynth.setPluckDecayB (pbPd); mCache.pBPluckDecay = pbPd; }
    const int   pbPb = geti ("partB_pluck_blur");
    if (pbPb != mCache.pBPluckBlur) { mSynth.setPluckBlurB (pbPb != 0); mCache.pBPluckBlur = pbPb; }
    const float pbFmc = getf ("partB_filter_mask_cutoff");
    if (pbFmc != mCache.pBFilterMaskCutoff)
    {
        const float normLog = std::log (pbFmc / 20.f) / std::log (20000.f / 20.f);
        mSynth.setFilterMaskAmountB (1.0f - juce::jlimit (0.f, 1.f, normLog));
        mCache.pBFilterMaskCutoff = pbFmc;
    }
    const float pbPmr = getf ("partB_phaser_mask_rate");
    if (pbPmr != mCache.pBPhaserMaskRate) { mSynth.setPhaserMaskRateB (pbPmr); mCache.pBPhaserMaskRate = pbPmr; }
}

//==============================================================================
void HarmlessProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();

    // S4: attach the mod registry as a child ValueTree on the fly so it ships
    // inside the APVTS state chunk. Strip any existing copy first so we don't
    // stack duplicates across saves.
    state.removeChild (state.getChildWithName (HarmlessModRegistry::rootId()), nullptr);
    state.appendChild (mModRegistry.toValueTree(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void HarmlessProcessor::setStateInformation (const void* data, int sz)
{
    if (auto xml = getXmlFromBinary (data, sz))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);

            // Extract and apply mod registry child (if present) before handing
            // the tree to APVTS - APVTS::replaceState validates + preserves
            // unknown children, but do it explicitly for clarity.
            auto modTree = tree.getChildWithName (HarmlessModRegistry::rootId());
            if (modTree.isValid())
                mModRegistry.fromValueTree (modTree);

            apvts.replaceState (tree);
        }
    }
}
