#include "BaySickPlayerProcessor.h"
#include "SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)
#include "../SampleLibrary.h"   // QA-ProjectSave Task 5: stable-root sample refs
#include "../MissingFileReport.h"
#include "../PluginProcessor.h"   // host audio shield around every sample load
#include "../ProjectFileResolver.h"
#include "BaySickPlayerEditor.h"

BaySickPlayerProcessor::BaySickPlayerProcessor (const juce::String& trackId, juce::UndoManager* undoMgr)
    : juce::AudioProcessor (BusesProperties()
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, undoMgr, "BaySickPlayerState",
             createLayout ("tk_" + trackId + "_bsp_")),
      mTrackId (trackId),
      mPrefix  ("tk_" + trackId + "_bsp_")
{
    // Resolve the timeline-WAV control atoms once (alloc-free per-block read; see
    // ClipCtlPtrs).  getRawParameterValue returns nullptr for any absent id, which
    // readClipCtl's load helper treats as the field default (same as the old path).
    auto rp = [this] (const char* n) { return apvts.getRawParameterValue (pid (n)); };
    mClipCtlPtrs.volume   = rp ("volume");   mClipCtlPtrs.pan         = rp ("pan");
    mClipCtlPtrs.cutoff   = rp ("cutoff");   mClipCtlPtrs.muffle      = rp ("muffle");
    mClipCtlPtrs.hardness = rp ("hardness"); mClipCtlPtrs.res         = rp ("res");
    mClipCtlPtrs.drive    = rp ("drive");    mClipCtlPtrs.reduct      = rp ("reduct");
    mClipCtlPtrs.lfoAmt   = rp ("lfoAmt");   mClipCtlPtrs.lfoRate     = rp ("lfo_rate");
    mClipCtlPtrs.treble   = rp ("treble");   mClipCtlPtrs.stereo      = rp ("stereo");
    mClipCtlPtrs.attack   = rp ("attack");   mClipCtlPtrs.decay       = rp ("decay");
    mClipCtlPtrs.sustain  = rp ("sustain");  mClipCtlPtrs.release     = rp ("release");
    mClipCtlPtrs.tune     = rp ("tune");     mClipCtlPtrs.detune      = rp ("detune");
    mClipCtlPtrs.reverse  = rp ("reverse");  mClipCtlPtrs.sampleStart = rp ("sampleStart");
    mClipCtlPtrs.stretch  = rp ("stretch");

    updateFromApvts();   // pre-warm cache so first processBlock skips all setters
}

//==============================================================================
bool BaySickPlayerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void BaySickPlayerProcessor::prepareToPlay (double sampleRate, int maxBlockSize)
{
    mSynth.prepare (sampleRate, maxBlockSize);
}

juce::AudioProcessorEditor* BaySickPlayerProcessor::createEditor()
{
    return new BaySickPlayerEditor (*this);
}

//==============================================================================
void BaySickPlayerProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    if (mDirtyTracker.hasChangedSinceLastBlock())   // QA-EngineApvts: skip per-block param LOAD when idle
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

    // Stuck-note fix: drain EVERY pending hold-off (mask), then the last-wins
    // hold-on -- see auditionNoteOff.
    juce::uint64 offWords[2];
    for (int w = 0; w < 2; ++w)
    {
        offWords[w] = mAuditionHoldOff[w].exchange (0, std::memory_order_acq_rel);
        if (offWords[w])
            for (int b = 0; b < 64; ++b)
                if (offWords[w] & (1ull << b))
                    midi.addEvent (juce::MidiMessage::noteOff (1, w * 64 + b), 0);
    }
    const int holdOnNote = mAuditionHoldOn.exchange (-1);
    if (holdOnNote >= 0 && holdOnNote <= 127)
    {
        midi.addEvent (juce::MidiMessage::noteOn  (1, holdOnNote, (juce::uint8) 100), 0);
        // Review fix: press AND release landed inside ONE block -- close the
        // note at block-end or it rings forever (see HarmlessProcessor).
        if (offWords[holdOnNote >> 6] & (1ull << (holdOnNote & 63)))
            midi.addEvent (juce::MidiMessage::noteOff (1, holdOnNote),
                           juce::jmax (1, buffer.getNumSamples() - 1));
    }

    // Sub-M: keyswitch pre-scan.  Walk the MIDI buffer once; for every
    // note-on/note-off whose MIDI note is a keyswitch (per the loaded SFZ's
    // sw_lokey..sw_hikey range), route to the manager's keyswitch state
    // handlers + STRIP the event from the buffer.  BaySickPlayerSynth never sees
    // keyswitch events - no voice-stealing trigger, no wasted cycles.
    // Non-keyswitch events copy through unchanged.  When no SFZ is loaded
    // (or no regions declare sw_lokey/sw_hikey), isKeyswitchNote returns
    // false for every note and the buffer passes through identically.
    {
        auto& mgr = mSynth.getManager();
        mKeyswitchFilteredMidi.clear();
        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();
            if (msg.isNoteOn() || msg.isNoteOff())
            {
                const int note = msg.getNoteNumber();
                if (mgr.isKeyswitchNote (note))
                {
                    if (msg.isNoteOn())
                        mgr.handleKeyswitchNoteOn  (note);
                    else
                        mgr.handleKeyswitchNoteOff (note);
                    continue;   // strip
                }
            }
            mKeyswitchFilteredMidi.addEvent (msg, meta.samplePosition);
        }
        midi.swapWith (mKeyswitchFilteredMidi);
    }

    mSynth.renderNextBlock (buffer, midi);

    // Flush NaN/Inf - prevents Windows WASAPI from permanently silencing the device.
    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        float* d = buffer.getWritePointer (c);
        for (int s = buffer.getNumSamples(); --s >= 0;)
            if (!std::isfinite (d[s])) d[s] = 0.0f;
    }
}

//==============================================================================
void BaySickPlayerProcessor::updateFromApvts()
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
    const float cutSelfMode  = get ("cutSelfMode");
    const float reverse      = get ("reverse");
    const int   unisonVoices = juce::roundToInt (get ("unisonVoices"));
    const float unisonSpread = get ("unisonSpread");

    if (cutSelf      != mCache.cutSelf)      { mCache.cutSelf      = cutSelf;      mSynth.setCutSelf      (cutSelf > 0.5f); }
    if (cutSelfMode  != mCache.cutSelfMode)  { mCache.cutSelfMode  = cutSelfMode;  mSynth.setCutSelfMode  (cutSelfMode > 0.5f); }
    if (reverse      != mCache.reverse)      { mCache.reverse      = reverse;      mSynth.setReverse      (reverse > 0.5f); }
    if (unisonVoices != mCache.unisonVoices) { mCache.unisonVoices = unisonVoices; mSynth.setUnisonVoices (unisonVoices); }
    if (unisonSpread != mCache.unisonSpread) { mCache.unisonSpread = unisonSpread; mSynth.setUnisonSpread (unisonSpread); }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
BaySickPlayerProcessor::createLayout (const juce::String& p)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto vid = [] (const juce::String& id) { return juce::ParameterID { id, 1 }; };

    // ── UI knobs ──────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        // The ids stay "lfoAmt" / "lfo_rate": they are saved-project keys and the
        // automation lane ids, and the clip-control atoms resolve by exactly those
        // strings.  Only the DISPLAY names change, to say what the control now does.
        vid (p + "lfoAmt"), "Vibrato Depth",
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
        // QA-ClipPlayback: bipolar width -- -1 = mono, 0 = full stereo (default), +1 = 2x wide.
        juce::NormalisableRange<float> (-1.f, 1.f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vid (p + "lfo_rate"), "Vibrato Rate",
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
    // Cut Self mode (QA-CutSelfReview): false = Same Pitch, true = Cut All.
    // BaySickPlayer defaults to Cut All to preserve today's drum behaviour.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        vid (p + "cutSelfMode"), "Cut Self Mode", true));

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

void BaySickPlayerProcessor::loadIntoManager (const juce::String& kind, const juce::File& f,
                                           int normalizeRoot)
{
    const bool isFolder = (kind == "folder");
    const bool isSfz    = (kind == "sfz");
    const bool isFile   = (kind == "file");
    if (! (isFolder || isSfz || isFile)) return;

    // THREAD SAFETY: every manager loader rebuilds mRegions IN PLACE - clear()
    // drops the shared_ptr each region holds, then one push_back per decoded
    // file - while the audio thread walks the same vector with no lock of its
    // own (isKeyswitchNote per MIDI event, findRegion + the region field reads
    // in BaySickPlayerVoice::startNote).  The push_back reallocation is the sharp edge:
    // it frees the old block after moving, so a reader that already loaded the
    // size can index released heap.  normalizeRootNotes writes every region's
    // rootNote and has the same exposure, which is why it sits inside the
    // bracket rather than after it.
    //
    // The host shield is the app-wide mechanism for this: it bails processBlock
    // at its top, and the acknowledgement-based settle waits for the block that
    // may already be inside the render to return.  shieldWasUp save/restore (not
    // a refcount) because the shield is raised only from the message thread, so
    // a load nested inside a project load leaves the outer shield up and pays no
    // second settle.
    const bool shieldWasUp = (mHost != nullptr) && mHost->isProjectLoadInProgress();
    if (mHost != nullptr)
    {
        mHost->setProjectLoadInProgress (true);
        if (! shieldWasUp) mHost->settleAudioThread();
    }

    if      (isFolder) mSynth.getManager().loadFolder     (f);
    else if (isSfz)    mSynth.getManager().loadSFZ        (f);
    else               mSynth.getManager().loadSingleFile (f);

    if (normalizeRoot >= 0) mSynth.getManager().normalizeRootNotes (normalizeRoot);

    if (mHost != nullptr) mHost->setProjectLoadInProgress (shieldWasUp);
}

void BaySickPlayerProcessor::loadSampleFolder (const juce::File& folder, int normalizeRoot)
{
    loadIntoManager ("folder", folder, normalizeRoot);
    apvts.state.setProperty (kLoadKindProp, "folder",                  nullptr);
    apvts.state.setProperty (kLoadPathProp, SampleLibrary::refForPersist (folder),  nullptr);
    apvts.state.setProperty (kLoadNormProp, normalizeRoot,              nullptr);
}

void BaySickPlayerProcessor::loadSampleSFZ (const juce::File& sfzFile, int normalizeRoot)
{
    loadIntoManager ("sfz", sfzFile, normalizeRoot);
    apvts.state.setProperty (kLoadKindProp, "sfz",                      nullptr);
    apvts.state.setProperty (kLoadPathProp, SampleLibrary::refForPersist (sfzFile), nullptr);
    apvts.state.setProperty (kLoadNormProp, normalizeRoot,              nullptr);
}

void BaySickPlayerProcessor::loadSampleFile (const juce::File& wavFile, int normalizeRoot)
{
    loadIntoManager ("file", wavFile, normalizeRoot);
    apvts.state.setProperty (kLoadKindProp, "file",                     nullptr);
    apvts.state.setProperty (kLoadPathProp, SampleLibrary::refForPersist (wavFile), nullptr);
    apvts.state.setProperty (kLoadNormProp, normalizeRoot,              nullptr);
}

void BaySickPlayerProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void BaySickPlayerProcessor::setStateInformation (const void* data, int sz)
{
    if (auto xml = SafeXml::parseBinaryBlob (data, sz))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        apvts.replaceStateKeepingUndoHistory (tree);

        // P3: replay the saved sample load.  Kind tells us which API to use.
        // Missing / empty path = nothing to restore (fresh engine).
        const auto kind = apvts.state.getProperty (kLoadKindProp, juce::String()).toString();
        const auto path = apvts.state.getProperty (kLoadPathProp, juce::String()).toString();
        const int  norm = (int) apvts.state.getProperty (kLoadNormProp, -1);
        if (kind.isNotEmpty() && path.isNotEmpty())
        {
            // resolvePersistedRef reads the stable roots but hands anything
            // else back as a bare juce::File, so a bundled project's
            // "Samples/<name>" resolved against the process working directory.
            const juce::File f = ProjectFileResolver::resolve (path);
            if (f.exists())
            {
                loadIntoManager (kind, f, norm);
                if (! mSynth.getManager().hasAnyRegions())
                    MissingFileReport::add ("BaySickPlayer sound", f.getFullPathName());
            }
            else
            {
                // The STORED reference, not the resolved file: a project-
                // relative reference with no project folder open resolves to
                // nothing, and an empty line in the missing-files dialog names
                // nothing the user can go find.
                MissingFileReport::add (kind == "folder" ? "Sample folder"
                                      : kind == "sfz"    ? "SFZ instrument"
                                                         : "Sample file",
                                        path);
            }
        }
    }
}
