#include "VibePlayerProcessor.h"
#include "VibePlayerEditor.h"

VibePlayerProcessor::VibePlayerProcessor (const juce::String& trackId)
    : juce::AudioProcessor (BusesProperties()
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BaySickPlayerState",
             createLayout ("tk_" + trackId + "_bsp_")),
      mTrackId (trackId),
      mPrefix  ("tk_" + trackId + "_bsp_")
{
    updateFromApvts();   // pre-warm cache so first processBlock skips all setters
}

//==============================================================================
bool VibePlayerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void VibePlayerProcessor::prepareToPlay (double sampleRate, int maxBlockSize)
{
    mSynth.prepare (sampleRate, maxBlockSize);
}

juce::AudioProcessorEditor* VibePlayerProcessor::createEditor()
{
    return new VibePlayerEditor (*this);
}

//==============================================================================
void VibePlayerProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    updateFromApvts();

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

    // Flush NaN/Inf — prevents Windows WASAPI from permanently silencing the device.
    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        float* d = buffer.getWritePointer (c);
        for (int s = buffer.getNumSamples(); --s >= 0;)
            if (!std::isfinite (d[s])) d[s] = 0.0f;
    }
}

//==============================================================================
void VibePlayerProcessor::updateFromApvts()
{
    auto get = [&] (const char* name) -> float
    {
        if (auto* p = apvts.getRawParameterValue (mPrefix + name))
            return p->load();
        return 0.f;
    };

    // ── Core controls ─────────────────────────────────────────────────────────
    const float lfoAmt = get ("lfoAmt");
    const float cutoff = get ("cutoff");
    const float res    = get ("res");
    const float drive  = get ("drive");
    const float reduct = get ("reduct");

    if (lfoAmt != mCache.lfoAmt) { mCache.lfoAmt = lfoAmt; mSynth.setLfoAmt (lfoAmt); }
    if (cutoff != mCache.cutoff) { mCache.cutoff  = cutoff; mSynth.setFilterParams (cutoff, mCache.res); }
    if (res    != mCache.res)    { mCache.res     = res;    mSynth.setFilterParams (mCache.cutoff, res); }
    if (drive  != mCache.drive)  { mCache.drive   = drive;  mSynth.setDrive (drive); }
    if (reduct != mCache.reduct) { mCache.reduct  = reduct; mSynth.setReduct (reduct); }

    // ── ADSR (S1 2026-04-21: attack + sustain now APVTS-driven) ───────────────
    const float attack  = get ("attack");
    const float decay   = get ("decay");
    const float sustain = get ("sustain");
    const float release = get ("release");
    if (attack  != mCache.attack
     || decay   != mCache.decay
     || sustain != mCache.sustain
     || release != mCache.release)
    {
        mCache.attack  = attack;
        mCache.decay   = decay;
        mCache.sustain = sustain;
        mCache.release = release;
        mSynth.setAdsr (attack, decay, sustain, release);
    }

    // ── Spatial / tone ────────────────────────────────────────────────────────
    const float pan     = get ("pan");
    const float volume  = get ("volume");
    const float stereo  = get ("stereo");
    const float treble  = get ("treble");
    const float stretch = get ("stretch");

    if (pan     != mCache.pan)     { mCache.pan     = pan;     mSynth.setPan (pan); }
    if (volume  != mCache.volume)  { mCache.volume  = volume;  mSynth.setVolume (volume); }
    if (stereo  != mCache.stereo)  { mCache.stereo  = stereo;  mSynth.setStereo (stereo); }
    if (treble  != mCache.treble)  { mCache.treble  = treble;  mSynth.setTreble (treble); }
    if (stretch != mCache.stretch) { mCache.stretch = stretch; mSynth.setStretch (stretch); }

    // ── Velocity response ─────────────────────────────────────────────────────
    const float muffle       = get ("muffle");
    const float velToMuffle  = get ("velToMuffle");
    const float hardness     = get ("hardness");
    const float velToHardness= get ("velToHardness");
    const float sensitivity  = get ("sensitivity");

    if (muffle       != mCache.muffle)       { mCache.muffle       = muffle;       mSynth.setMuffle (muffle); }
    if (velToMuffle  != mCache.velToMuffle)  { mCache.velToMuffle  = velToMuffle;  mSynth.setVelToMuffle (velToMuffle); }
    if (hardness     != mCache.hardness)     { mCache.hardness     = hardness;     mSynth.setHardness (hardness); }
    if (velToHardness!= mCache.velToHardness){ mCache.velToHardness= velToHardness;mSynth.setVelToHardness (velToHardness); }
    if (sensitivity  != mCache.sensitivity)  { mCache.sensitivity  = sensitivity;  mSynth.setSensitivity (sensitivity); }

    // ── Articulation group ────────────────────────────────────────────────────
    const int artic = juce::roundToInt (get ("artic_group"));
    if (artic != mCache.articGroup)
    {
        mCache.articGroup = artic;
        mSynth.setArticulationGroup (artic);
    }

    // ── S1 2026-04-21 additions ───────────────────────────────────────────────
    const float tune        = get ("tune");
    const float detune      = get ("detune");
    const int   detuneMode  = juce::roundToInt (get ("detuneMode"));
    const float lfoRate     = get ("lfo_rate");
    const float velToVolume = get ("velToVolume");
    const float sampleStart = get ("sampleStart");
    const int   voiceCap    = juce::roundToInt (get ("voiceCap"));

    if (tune        != mCache.tune)        { mCache.tune        = tune;        mSynth.setTune (tune); }
    if (detune      != mCache.detune)      { mCache.detune      = detune;      mSynth.setDetune (detune); }
    if (detuneMode  != mCache.detuneMode)  { mCache.detuneMode  = detuneMode;  mSynth.setDetuneMode (detuneMode); }
    if (lfoRate     != mCache.lfoRate)     { mCache.lfoRate     = lfoRate;     mSynth.setLfoRate (lfoRate); }
    if (velToVolume != mCache.velToVolume) { mCache.velToVolume = velToVolume; mSynth.setVelToVolume (velToVolume); }
    if (sampleStart != mCache.sampleStart) { mCache.sampleStart = sampleStart; mSynth.setSampleStart (sampleStart); }
    if (voiceCap    != mCache.voiceCap)    { mCache.voiceCap    = voiceCap;    mSynth.setVoiceCap (voiceCap); }

    // ── S1 Incr3 2026-04-21 ───────────────────────────────────────────────────
    const float cutSelf      = get ("cutSelf");
    const float reverse      = get ("reverse");
    const int   unisonVoices = juce::roundToInt (get ("unisonVoices"));
    const float unisonSpread = get ("unisonSpread");

    if (cutSelf      != mCache.cutSelf)      { mCache.cutSelf      = cutSelf;      mSynth.setCutSelf      (cutSelf > 0.5f); }
    if (reverse      != mCache.reverse)      { mCache.reverse      = reverse;      mSynth.setReverse      (reverse > 0.5f); }
    if (unisonVoices != mCache.unisonVoices) { mCache.unisonVoices = unisonVoices; mSynth.setUnisonVoices (unisonVoices); }
    if (unisonSpread != mCache.unisonSpread) { mCache.unisonSpread = unisonSpread; mSynth.setUnisonSpread (unisonSpread); }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
VibePlayerProcessor::createLayout (const juce::String& p)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto vid = [] (const juce::String& id) { return juce::ParameterID { id, 1 }; };

    // ── UI knobs ──────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "lfoAmt"), "LFO Amount",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "cutoff"), "Cutoff",
        juce::NormalisableRange<float> (20.f, 20000.f, 0.f, 0.25f), 20000.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "res"), "Resonance",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    // ── UI sliders ────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "drive"), "Drive",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "reduct"), "Reduct",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    // ── Extended params ───────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "decay"), "Decay",
        juce::NormalisableRange<float> (0.001f, 10.f, 0.f, 0.3f), 0.5f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "release"), "Release",
        juce::NormalisableRange<float> (0.001f, 10.f, 0.f, 0.3f), 0.3f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "pan"), "Pan",
        juce::NormalisableRange<float> (-1.f, 1.f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "volume"), "Volume",
        juce::NormalisableRange<float> (0.f, 1.f), 0.8f));

    // tremolo param removed 2026-04-21 (S1 audit) \u2014 redundant with lfoAmt. PRESET-BREAK.

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "stereo"), "Stereo",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "lfo_rate"), "LFO Rate",
        juce::NormalisableRange<float> (0.1f, 20.f, 0.f, 0.5f), 5.5f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "treble"), "Treble",
        juce::NormalisableRange<float> (-12.f, 12.f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "stretch"), "Stretch",
        juce::NormalisableRange<float> (0.5f, 2.0f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "velToMuffle"), "Vel->Muffle",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "muffle"), "Muffle",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "velToHardness"), "Vel->Hardness",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "hardness"), "Hardness",
        juce::NormalisableRange<float> (0.f, 1.f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "sensitivity"), "Sensitivity",
        juce::NormalisableRange<float> (0.f, 1.f), 0.5f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "tune"), "Tune",
        juce::NormalisableRange<float> (-24.f, 24.f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "detune"), "Detune",
        juce::NormalisableRange<float> (-100.f, 100.f), 0.f));

    // ── Articulation group (0-3 → A/B/C/D) ───────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterInt> (
        vid (p + "artic_group"), "Articulation Group", 0, 3, 0));

    // S1 2026-04-21 additions (all preserve v1 behaviour on defaults):
    layout.add (std::make_unique<juce::AudioParameterBool> (
        vid (p + "cutSelf"), "Cut Self", false));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        vid (p + "detuneMode"), "Detune Mode", 0, 2, 0));   // 0=simple 1=random 2=pair

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "attack"), "Attack",
        juce::NormalisableRange<float> (0.001f, 10.f, 0.f, 0.3f), 0.001f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "sustain"), "Sustain",
        juce::NormalisableRange<float> (0.f, 1.f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        vid (p + "voiceCap"), "Voice Cap", 1, 16, 16));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        vid (p + "reverse"), "Reverse", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "sampleStart"), "Sample Start",
        juce::NormalisableRange<float> (0.f, 1.f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "velToVolume"), "Vel->Volume",
        juce::NormalisableRange<float> (0.f, 1.f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        vid (p + "unisonVoices"), "Unison Voices", 1, 8, 1));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "unisonSpread"), "Unison Spread",
        juce::NormalisableRange<float> (0.f, 100.f), 0.f));

    return layout;
}

//==============================================================================
// P3 persistence (2026-04-24): sample-path wrappers.  Stash the loaded path
// as a non-APVTS property on apvts.state so it rides inside copyState() and
// gets reloaded in setStateInformation.
static const juce::Identifier kLoadKindProp ("bsp_loadKind");
static const juce::Identifier kLoadPathProp ("bsp_loadPath");
static const juce::Identifier kLoadNormProp ("bsp_loadNormalize");   // MIDI root; -1 = none

void VibePlayerProcessor::loadSampleFolder (const juce::File& folder, int normalizeRoot)
{
    mSynth.getManager().loadFolder (folder);
    if (normalizeRoot >= 0) mSynth.getManager().normalizeRootNotes (normalizeRoot);
    apvts.state.setProperty (kLoadKindProp, "folder",                  nullptr);
    apvts.state.setProperty (kLoadPathProp, folder.getFullPathName(),   nullptr);
    apvts.state.setProperty (kLoadNormProp, normalizeRoot,              nullptr);
}

void VibePlayerProcessor::loadSampleSFZ (const juce::File& sfzFile, int normalizeRoot)
{
    mSynth.getManager().loadSFZ (sfzFile);
    if (normalizeRoot >= 0) mSynth.getManager().normalizeRootNotes (normalizeRoot);
    apvts.state.setProperty (kLoadKindProp, "sfz",                      nullptr);
    apvts.state.setProperty (kLoadPathProp, sfzFile.getFullPathName(),  nullptr);
    apvts.state.setProperty (kLoadNormProp, normalizeRoot,              nullptr);
}

void VibePlayerProcessor::loadSampleFile (const juce::File& wavFile, int normalizeRoot)
{
    mSynth.getManager().loadSingleFile (wavFile);
    if (normalizeRoot >= 0) mSynth.getManager().normalizeRootNotes (normalizeRoot);
    apvts.state.setProperty (kLoadKindProp, "file",                     nullptr);
    apvts.state.setProperty (kLoadPathProp, wavFile.getFullPathName(),  nullptr);
    apvts.state.setProperty (kLoadNormProp, normalizeRoot,              nullptr);
}

juce::File VibePlayerProcessor::getLoadedSampleFile() const
{
    const auto path = apvts.state.getProperty (kLoadPathProp, juce::String()).toString();
    return path.isEmpty() ? juce::File() : juce::File (path);
}

void VibePlayerProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void VibePlayerProcessor::setStateInformation (const void* data, int sz)
{
    if (auto xml = getXmlFromBinary (data, sz))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        apvts.replaceState (tree);

        // P3: replay the saved sample load.  Kind tells us which API to use.
        // Missing / empty path = nothing to restore (fresh engine).
        const auto kind = apvts.state.getProperty (kLoadKindProp, juce::String()).toString();
        const auto path = apvts.state.getProperty (kLoadPathProp, juce::String()).toString();
        const int  norm = (int) apvts.state.getProperty (kLoadNormProp, -1);
        if (kind.isNotEmpty() && path.isNotEmpty())
        {
            juce::File f (path);
            if (f.exists())
            {
                if      (kind == "folder") mSynth.getManager().loadFolder     (f);
                else if (kind == "sfz")    mSynth.getManager().loadSFZ        (f);
                else if (kind == "file")   mSynth.getManager().loadSingleFile (f);
                if (norm >= 0) mSynth.getManager().normalizeRootNotes (norm);
            }
        }
    }
}
