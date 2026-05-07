#include "BaySickSynthProcessor.h"
#include "BaySickSynthEditor.h"

BaySickSynthProcessor::BaySickSynthProcessor (const juce::String& trackId)
    : juce::AudioProcessor (BusesProperties()
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BaySickSynthState",
             createLayout ("tk_" + trackId + "_bss_")),
      mTrackId (trackId),
      mPrefix  ("tk_" + trackId + "_bss_")
{
    updateFromApvts();
}

//==============================================================================
bool BaySickSynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void BaySickSynthProcessor::prepareToPlay (double sampleRate, int maxBlockSize)
{
    mSynth.prepare (sampleRate, maxBlockSize);
}

juce::AudioProcessorEditor* BaySickSynthProcessor::createEditor()
{
    return new BaySickSynthEditor (*this);
}

void BaySickSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Read host tempo for LFO sync (falls back to 120 BPM when no transport).
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                mHostBPM = (float) *bpm;

    updateFromApvts();

    // C.4 Phase 2.2: refresh engine-level SC RMS for any internal mod source.
    mScHelper.updateLevel (buffer.getNumSamples());

    const int auditNote = mAuditionNote.exchange (-1);
    if (auditNote >= 0 && auditNote <= 127)
    {
        midi.addEvent (juce::MidiMessage::noteOn  (1, auditNote, (juce::uint8) 100), 0);
        midi.addEvent (juce::MidiMessage::noteOff (1, auditNote),
                       juce::jmax (1, buffer.getNumSamples() - 1));
    }

    // Press-and-hold audition: hold-off first so a transition (release prev,
    // press next) within the same block sequences as off-then-on.
    const int holdOffNote = mAuditionHoldOff.exchange (-1);
    const int holdOnNote  = mAuditionHoldOn .exchange (-1);
    if (holdOffNote >= 0 && holdOffNote <= 127)
        midi.addEvent (juce::MidiMessage::noteOff (1, holdOffNote), 0);
    if (holdOnNote  >= 0 && holdOnNote  <= 127)
        midi.addEvent (juce::MidiMessage::noteOn  (1, holdOnNote, (juce::uint8) 100), 0);

    mSynth.renderNextBlock (buffer, midi);

    // 2026-04-25: master output gain (parity with BaySickPlayer's "volume").
    // Default 0.8 = ~ -1.94 dB headroom matches the sample player's authored
    // baseline.  Apply BEFORE NaN scrubbing so the limit catches both gain
    // and synthesis edge cases.
    {
        const float v = mCache.outVol;
        if (v != 1.0f) buffer.applyGain (v);
    }

    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        float* d = buffer.getWritePointer (c);
        for (int s = buffer.getNumSamples(); --s >= 0;)
            if (!std::isfinite (d[s])) d[s] = 0.0f;
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
BaySickSynthProcessor::createLayout (const juce::String& p)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto vid = [] (const juce::String& id) { return juce::ParameterID { id, 1 }; };

    // 2026-04-25: master output volume (parity with BaySickPlayer).
    // Default 0.8 ≈ -1.94 dB headroom - matches the sampler's authored
    // baseline so synth + sample sources sit at comparable levels.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "outVol"), "Out Vol",
        juce::NormalisableRange<float> (0.f, 1.f), 0.8f));

    // ── OSC ───────────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        vid (p + "waveform"), "Waveform",
        juce::StringArray {
            "SAW", "SAW+SAW", "PULSE", "SAW+SQUARE", "SQUARE+SQUARE",
            "SUPERSAW", "BELL", "DEAF SAW", "SPREAD OCT", "SPREAD 5TH", "SINE"
        }, 0));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        vid (p + "transpose"), "Transpose", -24, 24, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "modifier"), "Modifier",
        juce::NormalisableRange<float> (0.f, 1.f), 0.5f));

    // Dual-osc tuning mode (P3.4) - reinterprets Modifier for SAW+SAW / SAW+SQUARE
    // / SQUARE+SQUARE waveforms. Default Musical = current behaviour.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        vid (p + "dualOscMode"), "Dual-Osc Tuning",
        juce::StringArray { "Musical", "Hz Offset", "Absolute Hz" }, 0));

    // Hard sync (P3.7) - osc2 phase resets when osc1 completes a cycle.
    // Default false preserves current behaviour.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        vid (p + "oscSync"), "Hard Sync", false));

    // Ring modulation (P3.8) - sample = osc1 * osc2. Default false.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        vid (p + "ringMod"), "Ring Mod", false));

    // Per-voice analog drift (P3.10) - slow pitch wander ±10 cents at max.
    // Default 0 preserves current digitally-perfect behaviour.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "drift"), "Analog Drift",
        juce::NormalisableRange<float> (0.f, 1.f), 0.0f));

    // Unison (P3.11) - detuned saw stack with stereo spread. Default 1 voice = off.
    layout.add (std::make_unique<juce::AudioParameterInt> (
        vid (p + "unison_voices"), "Unison Voices", 1, 7, 1));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "unison_detune"), "Unison Detune",
        juce::NormalisableRange<float> (0.f, 1.f), 0.2f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "unison_spread"), "Unison Spread",
        juce::NormalisableRange<float> (0.f, 1.f), 0.8f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "noise"), "Noise",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    // Noise-only mode (P3.3): when on, oscillator is muted and the Noise knob
    // acts as overall level. Default false preserves current osc+noise mix.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        vid (p + "noiseOnly"), "Noise Only", false));

    // Noise colour (P3.9): White = current LCG, Pink = -3 dB/oct, Brown = -6 dB/oct.
    //   Default White preserves current behaviour.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        vid (p + "noiseColor"), "Noise Color",
        juce::StringArray { "White", "Pink", "Brown" }, 0));

    // ── Voice mode ────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        vid (p + "voiceMode"), "Voice Mode",
        juce::StringArray { "Poly", "Mono", "Lead", "Legato" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "glide"), "Glide",
        juce::NormalisableRange<float> (0.f, 2.f, 0.f, 0.4f), 0.f));

    // Cut self (Session E) - Poly-mode same-note retrig cut. Default false.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        vid (p + "cutSelf"), "Cut Self", false));

    // ── Mod wheel ─────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        vid (p + "modWheelDest"), "Mod Wheel Dest",
        juce::StringArray { "Filter", "LFO" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "modWheelAmt"), "Mod Wheel Amount",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    // ── Amp ADSR ──────────────────────────────────────────────────────────────
    auto adsr = [&] (const char* sfx, const char* lbl, float def)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            vid (p + sfx), lbl,
            juce::NormalisableRange<float> (0.001f, 10.f, 0.f, 0.3f), def));
    };

    adsr ("amp_attack",  "Amp Attack",  0.01f);
    adsr ("amp_decay",   "Amp Decay",   0.10f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "amp_sustain"), "Amp Sustain",
        juce::NormalisableRange<float> (0.f, 1.f), 0.8f));
    adsr ("amp_release", "Amp Release", 0.30f);

    // Velocity → Amp tracking (default 1.0 preserves current "amp follows velocity" behaviour)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "velAmpTrack"), "Vel to Amp",
        juce::NormalisableRange<float> (0.f, 1.f), 1.0f));

    // Pitch envelope (P3.1): one-shot ADSR on osc pitch, bipolar amount.
    //   Default Amt=0 preserves current behaviour (no pitch mod).
    adsr ("pEnv_attack",  "Pitch Env Attack",  0.01f);
    adsr ("pEnv_decay",   "Pitch Env Decay",   0.10f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "pEnv_sustain"), "Pitch Env Sustain",
        juce::NormalisableRange<float> (0.f, 1.f), 0.0f));
    adsr ("pEnv_release", "Pitch Env Release", 0.30f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "pEnv_amt"), "Pitch Env Amount",
        juce::NormalisableRange<float> (-24.f, 24.f), 0.0f));

    // Transient injector (P3.5): short HPF'd noise burst at noteOn. Default off.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "trans_amount"), "Transient Amount",
        juce::NormalisableRange<float> (0.f, 1.f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "trans_duration"), "Transient Duration",
        juce::NormalisableRange<float> (0.f, 20.f), 5.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "trans_colour"), "Transient Colour",
        juce::NormalisableRange<float> (200.f, 10000.f, 0.f, 0.3f), 5000.0f));

    // Multi-burst envelope (P3.6): N attack pulses that modulate the amp env.
    //   Default mode off preserves plain ADSR behaviour.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        vid (p + "burst_mode"), "Burst Mode", false));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        vid (p + "burst_count"), "Burst Count", 1, 8, 4));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "burst_spacing"), "Burst Spacing",
        juce::NormalisableRange<float> (1.f, 100.f), 20.0f));

    // ── Filter ────────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        vid (p + "flt_type"), "Filter Type",
        juce::StringArray { "Low Pass", "High Pass", "Band Pass", "Notch" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "flt_cutoff"), "Filter Cutoff",
        juce::NormalisableRange<float> (20.f, 20000.f, 0.f, 0.25f), 20000.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "flt_res"), "Filter Resonance",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "flt_env_amt"), "Filter Env Amount",
        juce::NormalisableRange<float> (-1.f, 1.f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "flt_kbtrack"), "KB Track",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "flt_veltrack"), "Vel Track",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    // ── Filter ADSR ───────────────────────────────────────────────────────────
    adsr ("flt_attack",  "Filter Attack",  0.01f);
    adsr ("flt_decay",   "Filter Decay",   0.10f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "flt_sustain"), "Filter Sustain",
        juce::NormalisableRange<float> (0.f, 1.f), 0.5f));
    adsr ("flt_release", "Filter Release", 0.30f);

    // ── LFO ───────────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        vid (p + "lfo_shape"), "LFO Shape",
        juce::StringArray { "Sine", "Saw", "Square" }, 0));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        vid (p + "lfo_dest"), "LFO Dest",
        juce::StringArray { "Filter", "Pitch", "Osc Modifier" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "lfo_rate"), "LFO Rate",
        juce::NormalisableRange<float> (0.01f, 30.f, 0.f, 0.3f), 1.f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        vid (p + "lfo_sync"), "LFO Sync", false));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        vid (p + "lfo_division"), "LFO Division",
        juce::StringArray { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" }, 2));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "lfo_amount"), "LFO Amount",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    return layout;
}

//==============================================================================
void BaySickSynthProcessor::updateFromApvts()
{
    auto getf = [&] (const char* name) -> float
    {
        if (auto* param = apvts.getRawParameterValue (mPrefix + name)) return param->load();
        return 0.f;
    };
    auto geti = [&] (const char* name) -> int { return (int) getf (name); };

    // ── Master out volume (2026-04-25) ────────────────────────────────────────
    const float outVol = getf ("outVol");
    if (outVol != mCache.outVol) mCache.outVol = outVol;

    // ── Waveform ──────────────────────────────────────────────────────────────
    const int wf = geti ("waveform");
    if (wf != mCache.waveform)
    {
        mSynth.setWaveform ((BssWaveform) juce::jlimit (0, 10, wf));
        mCache.waveform = wf;
    }

    const int tr = geti ("transpose");
    if (tr != mCache.transpose) { mSynth.setTranspose (tr); mCache.transpose = tr; }

    const float mod = getf ("modifier");
    if (mod != mCache.modifier) { mSynth.setModifier (mod); mCache.modifier = mod; }

    const int dualOscMode = geti ("dualOscMode");
    if (dualOscMode != mCache.dualOscMode)
    {
        mSynth.setDualOscMode (dualOscMode);
        mCache.dualOscMode = dualOscMode;
    }

    const int oscSync = geti ("oscSync");
    if (oscSync != mCache.oscSync)
    {
        mSynth.setOscSync (oscSync != 0);
        mCache.oscSync = oscSync;
    }

    const int ringMod = geti ("ringMod");
    if (ringMod != mCache.ringMod)
    {
        mSynth.setRingMod (ringMod != 0);
        mCache.ringMod = ringMod;
    }

    const float drift = getf ("drift");
    if (drift != mCache.drift) { mSynth.setDriftAmount (drift); mCache.drift = drift; }

    const int uniVoices = geti ("unison_voices");
    if (uniVoices != mCache.unisonVoices)
    {
        mSynth.setUnisonVoices (uniVoices);
        mCache.unisonVoices = uniVoices;
    }
    const float uniDet = getf ("unison_detune");
    if (uniDet != mCache.unisonDetune)
    {
        mSynth.setUnisonDetune (uniDet);
        mCache.unisonDetune = uniDet;
    }
    const float uniSpr = getf ("unison_spread");
    if (uniSpr != mCache.unisonSpread)
    {
        mSynth.setUnisonSpread (uniSpr);
        mCache.unisonSpread = uniSpr;
    }

    const float noise = getf ("noise");
    if (noise != mCache.noise) { mSynth.setNoiseLevel (noise); mCache.noise = noise; }

    const int noiseOnly = geti ("noiseOnly");
    if (noiseOnly != mCache.noiseOnly)
    {
        mSynth.setNoiseOnlyMode (noiseOnly != 0);
        mCache.noiseOnly = noiseOnly;
    }

    const int noiseColor = geti ("noiseColor");
    if (noiseColor != mCache.noiseColor)
    {
        mSynth.setNoiseColor (noiseColor);
        mCache.noiseColor = noiseColor;
    }

    const int vm = geti ("voiceMode");
    if (vm != mCache.voiceMode)
    {
        mSynth.setVoiceMode ((BssVoiceMode) juce::jlimit (0, 3, vm));
        mCache.voiceMode = vm;
    }

    const float glide = getf ("glide");
    if (glide != mCache.glide) { mSynth.setGlideTime (glide); mCache.glide = glide; }

    const int cutSelf = geti ("cutSelf");
    if (cutSelf != mCache.cutSelf)
    {
        mSynth.setCutSelf (cutSelf != 0);
        mCache.cutSelf = cutSelf;
    }

    const int mwd = geti ("modWheelDest");
    if (mwd != mCache.modWheelDest) { mSynth.setModWheelDest (mwd); mCache.modWheelDest = mwd; }

    const float mwa = getf ("modWheelAmt");
    if (mwa != mCache.modWheelAmt) { mSynth.setModWheelAmt (mwa); mCache.modWheelAmt = mwa; }

    // ── Amp ADSR ──────────────────────────────────────────────────────────────
    const float aA = getf ("amp_attack"),  aD = getf ("amp_decay"),
                aS = getf ("amp_sustain"), aR = getf ("amp_release");
    if (aA != mCache.ampA || aD != mCache.ampD || aS != mCache.ampS || aR != mCache.ampR)
    {
        mSynth.setAmpEnv (aA, aD, aS, aR);
        mCache.ampA = aA; mCache.ampD = aD; mCache.ampS = aS; mCache.ampR = aR;
    }

    const float velAmp = getf ("velAmpTrack");
    if (velAmp != mCache.velAmpTrack) { mSynth.setVelAmpTrack (velAmp); mCache.velAmpTrack = velAmp; }

    // Pitch envelope (P3.1)
    const float pA = getf ("pEnv_attack"),  pD = getf ("pEnv_decay"),
                pS = getf ("pEnv_sustain"), pR = getf ("pEnv_release");
    if (pA != mCache.pEnvA || pD != mCache.pEnvD || pS != mCache.pEnvS || pR != mCache.pEnvR)
    {
        mSynth.setPitchEnv (pA, pD, pS, pR);
        mCache.pEnvA = pA; mCache.pEnvD = pD; mCache.pEnvS = pS; mCache.pEnvR = pR;
    }

    const float pAmt = getf ("pEnv_amt");
    if (pAmt != mCache.pEnvAmt) { mSynth.setPitchEnvAmt (pAmt); mCache.pEnvAmt = pAmt; }

    // Transient injector (P3.5)
    const float tAmt = getf ("trans_amount");
    if (tAmt != mCache.transAmt) { mSynth.setTransientAmount (tAmt); mCache.transAmt = tAmt; }
    const float tDur = getf ("trans_duration");
    if (tDur != mCache.transDur) { mSynth.setTransientDuration (tDur); mCache.transDur = tDur; }
    const float tCol = getf ("trans_colour");
    if (tCol != mCache.transCol) { mSynth.setTransientColour (tCol); mCache.transCol = tCol; }

    // Multi-burst envelope (P3.6)
    const int burstMode = geti ("burst_mode");
    if (burstMode != mCache.burstMode)
    {
        mSynth.setBurstMode (burstMode != 0);
        mCache.burstMode = burstMode;
    }
    const int burstCount = geti ("burst_count");
    if (burstCount != mCache.burstCount)
    {
        mSynth.setBurstCount (burstCount);
        mCache.burstCount = burstCount;
    }
    const float burstSpacing = getf ("burst_spacing");
    if (burstSpacing != mCache.burstSpacing)
    {
        mSynth.setBurstSpacing (burstSpacing);
        mCache.burstSpacing = burstSpacing;
    }

    // ── Filter ────────────────────────────────────────────────────────────────
    const int fltType = geti ("flt_type");
    if (fltType != mCache.fltType)
    {
        mSynth.setFilterType ((BssFilterType) juce::jlimit (0, 3, fltType));
        mCache.fltType = fltType;
    }

    const float fc = getf ("flt_cutoff");
    if (fc != mCache.fltCutoff) { mSynth.setFilterCutoff (fc); mCache.fltCutoff = fc; }

    const float fr = getf ("flt_res");
    if (fr != mCache.fltRes) { mSynth.setFilterRes (fr); mCache.fltRes = fr; }

    const float fea = getf ("flt_env_amt");
    if (fea != mCache.fltEnvAmt) { mSynth.setFilterEnvAmt (fea); mCache.fltEnvAmt = fea; }

    const float kbt = getf ("flt_kbtrack");
    if (kbt != mCache.fltKbTrack) { mSynth.setFilterKbTrack (kbt); mCache.fltKbTrack = kbt; }

    const float vt = getf ("flt_veltrack");
    if (vt != mCache.fltVelTrack) { mSynth.setFilterVelTrack (vt); mCache.fltVelTrack = vt; }

    // ── Filter ADSR ───────────────────────────────────────────────────────────
    const float fA = getf ("flt_attack"),  fD = getf ("flt_decay"),
                fS = getf ("flt_sustain"), fR = getf ("flt_release");
    if (fA != mCache.fltA || fD != mCache.fltD || fS != mCache.fltS || fR != mCache.fltR)
    {
        mSynth.setFltEnv (fA, fD, fS, fR);
        mCache.fltA = fA; mCache.fltD = fD; mCache.fltS = fS; mCache.fltR = fR;
    }

    // ── LFO ───────────────────────────────────────────────────────────────────
    const int lfoShape = geti ("lfo_shape");
    if (lfoShape != mCache.lfoShape)
    {
        // 0=Sine, 1=Saw(Up), 2=Square
        static constexpr LFOShape kShapes[] = {
            LFOShape::Sine, LFOShape::SawUp, LFOShape::Square
        };
        mSynth.setLFOShape (kShapes[juce::jlimit (0, 2, lfoShape)]);
        mCache.lfoShape = lfoShape;
    }

    const int lfoDest = geti ("lfo_dest");
    if (lfoDest != mCache.lfoDest)
    {
        mSynth.setLFODest ((BssLFODest) juce::jlimit (0, 2, lfoDest));
        mCache.lfoDest = lfoDest;
    }

    // LFO rate: knob-Hz when sync off, else BPM-synced division Hz.
    //   Division multipliers (cycles per quarter note):
    //     1/1 = 0.25, 1/2 = 0.5, 1/4 = 1.0, 1/8 = 2.0, 1/16 = 4.0, 1/32 = 8.0
    const float knobRate = getf ("lfo_rate");
    const int   syncOn   = geti ("lfo_sync");
    const int   divIdx   = geti ("lfo_division");

    float effectiveRate = knobRate;
    if (syncOn != 0)
    {
        static constexpr float kDivMults[6] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };
        const int  clampedDiv = juce::jlimit (0, 5, divIdx);
        effectiveRate = (mHostBPM / 60.0f) * kDivMults[clampedDiv];
    }

    if (effectiveRate != mCache.lfoRate
        || syncOn     != mCache.lfoSync
        || divIdx     != mCache.lfoDivision
        || mHostBPM   != mCache.lfoHostBPM)
    {
        mSynth.setLFORate (effectiveRate);
        mEffectiveLfoRate.store (effectiveRate);
        mCache.lfoRate     = effectiveRate;
        mCache.lfoSync     = syncOn;
        mCache.lfoDivision = divIdx;
        mCache.lfoHostBPM  = mHostBPM;
    }

    const float lfoAmt = getf ("lfo_amount");
    if (lfoAmt != mCache.lfoAmount) { mSynth.setLFOAmount (lfoAmt); mCache.lfoAmount = lfoAmt; }
}

//==============================================================================
void BaySickSynthProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void BaySickSynthProcessor::setStateInformation (const void* data, int sz)
{
    if (auto xml = getXmlFromBinary (data, sz))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}
