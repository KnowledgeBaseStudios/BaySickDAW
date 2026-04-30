#include "PluginProcessor.h"
// 2026-04-25: BaySickDrumsProcessor include removed — class deleted.
#include "BaySickSynth/BaySickSynthProcessor.h"   // D1.4-fix (c): drum transpose compensation
#include "VibePlayer/VibePlayerProcessor.h"       // D1.4-fix (c): drum tune compensation
#ifdef VIBESYNTH_VST
  #include "PluginEditor.h"
#endif

// 2026-04-30 (audit C11+C12): emit a noteOn with the per-note panning +
// fine-pitch values carried as standard MIDI CC10 + PitchWheel.  Was the
// missing half of the piano roll's Panning + Pitch Bend control lanes —
// users could draw values, the project saved them, but no audio response.
// Channel-wide (not MPE), so chord-with-mixed-per-note-pan behaves as
// "last note wins" for the channel state.  Acceptable for melodic lines.
static void emitPianoNoteOn (juce::MidiBuffer& dst,
                              const PianoNote& note, int samplePos)
{
    constexpr int ch = 1;
    const int vel = juce::jlimit (1, 127, (int) (note.velocity * 127.f));

    // CC10 pan: center 64, full L = 0, full R = 127.  Skip when value is
    // basically center to avoid spamming the channel with no-op CCs.
    if (std::abs (note.panning) > 0.005f)
    {
        const int pan = juce::jlimit (0, 127,
            (int) std::round (64.f + note.panning * 63.f));
        dst.addEvent (juce::MidiMessage::controllerEvent (ch, 10, pan), samplePos);
    }
    // PitchWheel: 14-bit, center 8192.  finePitch is documented as
    // ±100 cents, mapped to ±4096 from center — i.e. ±50 % of a typical
    // ±2-semitone synth pitch-bend range = ±1 semitone in the synth's
    // tuning.  Synths with non-default pitch-bend ranges will scale.
    if (std::abs (note.finePitch) > 0.005f)
    {
        const int wheel = juce::jlimit (0, 16383,
            (int) std::round (8192.f + note.finePitch * 4096.f));
        dst.addEvent (juce::MidiMessage::pitchWheel (ch, wheel), samplePos);
    }
    dst.addEvent (juce::MidiMessage::noteOn (ch, note.midiNote, (juce::uint8) vel),
                  samplePos);
}

// ── Parameter layout ──────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout
VibeSynthProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto addF = [&](const juce::String& id, const juce::String& name,
                    float lo, float hi, float def)
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            VID(id), name,
            juce::NormalisableRange<float>(lo, hi),
            def));
    };
    auto addB = [&](const juce::String& id, const juce::String& name, bool def)
    {
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            VID(id), name, def));
    };
    auto addI = [&](const juce::String& id, const juce::String& name,
                    int lo, int hi, int def)
    {
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            VID(id), name, lo, hi, def));
    };

    // Master
    addF("masterGain", "Master Gain", 0.f, 1.f, 0.8f);
    // Global "kill-all" — when true, every effects rack in the app is bypassed
    // regardless of its own _bypass state. Read by each bus/insert node per block.
    addB("master_fx_bypass", "Master FX Bypass", false);

    // 2026-04-29: project-level pan law (FL Studio parity).
    //   0 = Circular   (constant power, -3 dB at center, FL default)
    //   1 = Triangular (linear,         -6 dB at center)
    //   2 = Square     (0 dB at center, only attenuates the opposite side)
    // Read every audio block by each Insert/Bus/MasterBusNode when applying
    // the per-strip _pan param.  Default 0 matches FL's fresh-project default.
    addI("master_pan_law", "Pan Law", 0, 2, 0);

    // §P4.3 B7 (2026-04-22): legacy bus-EQ param blocks removed.
    // Pre-rack Layers/Bass/Drums EQs are now per-strip on the InsertNode/BusNode
    // (mixer_{kind}_<i>_preeq_mid_eq* / _preeq_side_eq*, registered lazily via
    // addParamsForTrackPreEQ in ensureMixerStripParams).  Post-rack EQs live on
    // mixer_{kind}_<i>_mid_eq* / _side_eq*.  The legacy `drums_*_eq*` block + the
    // matching `tk_lay_*_mid_eq*` / `tk_bas_*_mid_eq*` lazy registrations + the
    // mDrumsEQDSP / mLayerPageEQs / mBassPageEQs DSP instances are all gone.

    return { params.begin(), params.end() };
}

// ── Constructor / Destructor ──────────────────────────────────────────────────
VibeSynthProcessor::VibeSynthProcessor()
    : AudioProcessor(BusesProperties()
        // R3 (2026-04-23): declare an input bus so JUCE feeds the audio
        // device's input channels into the processBlock buffer.  Capped at
        // 16 channels (discreteChannels(16)) - covers most desktop ASIO
        // interfaces without forcing every machine to allocate big buffers.
        .withInput ("Input",  juce::AudioChannelSet::discreteChannels(16), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "BaySickDAWState", createParameterLayout())
{
    // Set up polyphonic synth -- 8 voices
    mSynth.addSound(new SynthSound());
    for (int i = 0; i < 8; ++i)
        mSynth.addVoice(new SynthVoice());

    mAudioFormatManager.registerBasicFormats();  // WAV, AIFF, MP3, OGG, FLAC

    for (int i = 0; i < kMaxAudioRows; ++i)
    {
        mAudioRowPeakDb [i].store(-60.0f, std::memory_order_relaxed);
        mAudioRowPeakDbL[i].store(-60.0f, std::memory_order_relaxed);
        mAudioRowPeakDbR[i].store(-60.0f, std::memory_order_relaxed);
    }

    // G-7 polish (2026-04-29): bumped low → normal.  At low priority the bg
    // thread was getting heavily preempted on Windows during the first few
    // audio blocks, especially with MP3 clips whose per-chunk decode is
    // ~10x slower than WAV.  Result: the 2-sec ring pre-fill ran dry around
    // the end of bar 1 at 120 BPM before the bg thread could top it up,
    // causing a single audible skip on first play.  Normal priority gets
    // the bg thread CPU time fast enough to keep the ring topped up.
    mAudioFileThread.startThread (juce::Thread::Priority::normal);

    // §P4.3 perf: subscribe to APVTS state changes.  ValueTree::Listener fires
    // for every param change (UI / automation / host-driven).  We just flip the
    // dirty flag so the next processBlock re-runs EQ sync; otherwise sync skips.
    apvts.state.addListener(this);
}

VibeSynthProcessor::~VibeSynthProcessor()
{
    apvts.state.removeListener(this);
    mAudioFileThread.stopThread (500);
}

// ── Preparation ───────────────────────────────────────────────────────────────
void VibeSynthProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mSampleRate = sampleRate;
    mBlockSize  = samplesPerBlock;

    mSynth.setCurrentPlaybackSampleRate(sampleRate);
    mBassSynth.prepare(sampleRate, samplesPerBlock);

    // Pre-allocate engine scratch buffers to avoid audio-thread allocation
    mLayerEngineSum    .setSize(2, samplesPerBlock, false, true, false);
    mLayerEngineScratch.setSize(2, samplesPerBlock, false, true, false);
    mBassEngineBuf     .setSize(2, samplesPerBlock, false, true, false);
    mBassEngineScratch .setSize(2, samplesPerBlock, false, true, false);
    mAudioRowScratch   .setSize(2, samplesPerBlock, false, true, false);
    mAudioClipScratch  .setSize(2, samplesPerBlock, false, true, false);
    // R3 (2026-04-23): pre-allocate live-input scratch + snapshot.  Snapshot
    // gets resized at first non-zero numInputs in processBlock; here we just
    // guarantee a safe initial state.  Slot scratch is always stereo.
    mLiveInputSlotBuf  .setSize(2, samplesPerBlock, false, true, false);
    mLiveInputSnapshot .setSize(2, samplesPerBlock, false, true, false);

    // Re-prepare any registered engine processors
    {
        juce::SpinLock::ScopedLockType lk(mLayerEngineLock);
        for (auto* eng : mLayerEngines)
            if (eng) eng->prepareToPlay(sampleRate, samplesPerBlock);
    }
    {
        juce::SpinLock::ScopedLockType lk(mBassEngineLock);
        for (int i = 0; i < kMaxBassPages; ++i)
            if (mBassEngines[i]) mBassEngines[i]->prepareToPlay(sampleRate, samplesPerBlock);
    }
    // 2026-04-25: legacy mDrumsEngine removed.  Per-drum-tab engines are
    // re-prepared by their owners (DrumPage::selectEngine).
    // G-3 (2026-04-28): re-prepare any registered Clip engines so host SR /
    // block-size changes (e.g. user switches audio device) propagate to
    // VibePlayer / BaySickNAM/IR instances owned by ClipsPage tabs.
    {
        juce::SpinLock::ScopedLockType lk(mClipEngineLock);
        for (int i = 0; i < kMaxClipPages; ++i)
            if (mClipEngines[i]) mClipEngines[i]->prepareToPlay(sampleRate, samplesPerBlock);
    }
    // G-4 (2026-04-28): same for Vox + Inst engines.
    {
        juce::SpinLock::ScopedLockType lk(mVoxEngineLock);
        for (int i = 0; i < kMaxVoxPages; ++i)
            if (mVoxEngines[i]) mVoxEngines[i]->prepareToPlay(sampleRate, samplesPerBlock);
    }
    {
        juce::SpinLock::ScopedLockType lk(mInstEngineLock);
        for (int i = 0; i < kMaxInstPages; ++i)
            if (mInstEngines[i]) mInstEngines[i]->prepareToPlay(sampleRate, samplesPerBlock);
    }
    mVibeGraph.prepare(sampleRate, samplesPerBlock);

    // Build the fixed bus topology the first time; no-op on subsequent calls.
    // §P4.3 B7: drumsEQ ref removed — DrumsBusNode now uses its own preEq member
    // (sync'd via updateAllPreRackEQsFromApvts from mixer_drumsbus_preeq_*).
    mVibeGraph.buildFixedTopology(mSynth, mBassSynth, apvts);

    // 5F-4a: register master + 5 bus strip params (idempotent).
    ensureMixerBusAndMasterParams();
    // 5F-4a Batch 6: cache APVTS pointers in bus + master nodes (needs params registered).
    mVibeGraph.rebindBusApvts();

    // Compute initial PDC (0 for all current effects) and report to host.
    setLatencySamples(mVibeGraph.updateBusLatencies());
}

void VibeSynthProcessor::releaseResources() {}

bool VibeSynthProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Output: stereo or mono.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono())
        return false;
    // R3 (2026-04-23): accept any input layout.  Standalone reports the
    // device's actual input count (0..16+), VST hosts may report 0 or
    // arbitrary - we read whatever's there + per-strip _inputChannelIdx
    // bounds-checks against the actual count each block.
    return true;
}

// ── processBlock ──────────────────────────────────────────────────────────────
void VibeSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midiMessages)
{
    // 1M: capture wall-clock start time (high-res, audio-thread safe)
    const auto t0 = juce::Time::getHighResolutionTicks();

    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    // R3 (2026-04-23): Snapshot the input audio BEFORE buffer.clear() so
    // armed Vox / Inst strips can pull from the audio interface's input
    // channels.  getTotalNumInputChannels reports the ACTUAL negotiated input
    // count (0 on machines without an interface; up to 16 with the Tascam
    // etc.).  We snapshot into a single non-clearing scratch buffer; per-strip
    // splitting happens below in the Vox / Inst loop so we don't allocate
    // per-strip buffers on the audio thread.
    const int numInputs = juce::jmin (buffer.getNumChannels(), getTotalNumInputChannels());
    if (numInputs > 0)
    {
        if (mLiveInputSnapshot.getNumChannels() < numInputs
            || mLiveInputSnapshot.getNumSamples() < numSamples)
            mLiveInputSnapshot.setSize (numInputs, numSamples, false, false, true);
        for (int c = 0; c < numInputs; ++c)
            mLiveInputSnapshot.copyFrom (c, 0, buffer, c, 0, numSamples);
    }

    buffer.clear();

    // Sync mixer state from PatternManager if present (standalone mode)
    syncMixerFromPatternManager();

    updateDrumMixLevels();
    // §P4.3 B7: legacy per-page EQ updaters (updateDrumsEQ /
    // updateLayerPageEQsFromApvts / updateBassPageEQsFromApvts) deleted along
    // with the DSP instances they fed.  All pre-rack EQs now live on
    // InsertNode/BusNode preEq members and are sync'd by the unified
    // updateAllPreRackEQsFromApvts pass below.
    // §P4.3 perf: dirty-flag short-circuit.  EQ sync only runs in blocks where
    // an APVTS param actually changed (listener flips the flag).  Untouched
    // blocks pay 1 atomic load + skip — eliminates ~1.4M string-concat hash
    // lookups/sec that were happening on the audio thread.
    if (mEQsDirty.exchange(false, std::memory_order_acquire))
    {
        updateAllPostRackEQsFromApvts();
        updateAllPreRackEQsFromApvts();   // §P4.3 (B4)
    }

    // ── Get playhead position ─────────────────────────────────────────────
    juce::AudioPlayHead::PositionInfo pos;
    if (auto* ph = getPlayHead())
        if (auto optPos = ph->getPosition())
            pos = *optPos;

    // ── Layers piano roll: build MIDI from piano roll + incoming MIDI ─────
    juce::MidiBuffer allMidi;
    allMidi.addEvents(midiMessages, 0, numSamples, 0);
    std::array<juce::MidiBuffer, kMaxLayerPages>  layerPageMidi;   // per-page MIDI for layer engines
    std::array<juce::MidiBuffer, kMaxBassPages>   bassPageMidi;    // per-page MIDI for bass engines
    std::array<juce::MidiBuffer, kMaxDrumPages>   drumPageMidi;    // D1.2: per-drum-page MIDI (dynamic-drum model)
    std::array<juce::MidiBuffer, kMaxClipPages>   clipPageMidi;    // G-3 (2026-04-28): per-clip-page MIDI (sampler-style triggering)
    std::array<juce::MidiBuffer, kMaxVoxPages>    voxPageMidi;     // G-4 (2026-04-28): per-Vox-page MIDI
    std::array<juce::MidiBuffer, kMaxInstPages>   instPageMidi;    // G-4 (2026-04-28): per-Inst-page MIDI

    // ── Flush-all request (from Stop button) ──────────────────────────────
    // Sends All-Notes-Off to every engine + clears pending offs. We do this
    // BEFORE the normal scheduling so any about-to-be-scheduled notes on this
    // block are silenced too.
    if (mFlushAllNotes.exchange(false, std::memory_order_acq_rel))
    {
        for (auto& off : mPRPendingOffs)
        {
            if (off.target < kMaxLayerPages)
                layerPageMidi[off.target].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
            else if (off.target >= kBassPRTarget && off.target < kBassPRTarget + kMaxBassPages)
                bassPageMidi[off.target - kBassPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
            else if (off.target >= kDrumPRTarget && off.target < kDrumPRTarget + kMaxDrumPages)
                drumPageMidi[off.target - kDrumPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
            else if (off.target >= kClipPRTarget && off.target < kClipPRTarget + kMaxClipPages)
                clipPageMidi[off.target - kClipPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
            else if (off.target >= kVoxPRTarget && off.target < kVoxPRTarget + kMaxVoxPages)
                voxPageMidi[off.target - kVoxPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
            else if (off.target >= kInstPRTarget && off.target < kInstPRTarget + kMaxInstPages)
                instPageMidi[off.target - kInstPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
        }
        mPRPendingOffs.clear();
        // Belt-and-suspenders: fire CC 123 (All Notes Off) on every engine
        // in case any voice was triggered from a source that didn't register
        // a pending-off (audition note, external MIDI etc.).
        for (auto& b : layerPageMidi) b.addEvent(juce::MidiMessage::allNotesOff(1), 0);
        for (auto& b : bassPageMidi)  b.addEvent(juce::MidiMessage::allNotesOff(1), 0);
        for (auto& b : drumPageMidi)  b.addEvent(juce::MidiMessage::allNotesOff(1), 0);
        for (auto& b : clipPageMidi)  b.addEvent(juce::MidiMessage::allNotesOff(1), 0);   // G-3
        for (auto& b : voxPageMidi)   b.addEvent(juce::MidiMessage::allNotesOff(1), 0);   // G-4
        for (auto& b : instPageMidi)  b.addEvent(juce::MidiMessage::allNotesOff(1), 0);   // G-4
    }

    // ── Piano roll note scheduling ────────────────────────────────────────
    {
        bool isPlayingPR = pos.getIsPlaying();
        if (!isPlayingPR)
        {
            if (!mPRPendingOffs.empty())
            {
                for (auto& off : mPRPendingOffs)
                {
                    if (off.target < kMaxLayerPages)
                        layerPageMidi[off.target].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                    else if (off.target >= kBassPRTarget && off.target < kBassPRTarget + kMaxBassPages)
                        bassPageMidi[off.target - kBassPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                    else if (off.target >= kDrumPRTarget && off.target < kDrumPRTarget + kMaxDrumPages)
                        drumPageMidi[off.target - kDrumPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                    else if (off.target >= kClipPRTarget && off.target < kClipPRTarget + kMaxClipPages)
                        clipPageMidi[off.target - kClipPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                    else if (off.target >= kVoxPRTarget && off.target < kVoxPRTarget + kMaxVoxPages)
                        voxPageMidi[off.target - kVoxPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                    else if (off.target >= kInstPRTarget && off.target < kInstPRTarget + kMaxInstPages)
                        instPageMidi[off.target - kInstPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                }
                mPRPendingOffs.clear();
            }
            mPRLastBeatEnd = -1.0;
        }
        else if (mPatternManager)
        {
            double bpmVal    = pos.getBpm().orFallback(120.0);
            double bs        = juce::jmax(1e-6, bpmVal / (60.0 * mSampleRate));
            double beatStart = pos.getPpqPosition().orFallback(0.0);
            double beatEnd   = beatStart + numSamples * bs;

            // ── SONG MODE ─────────────────────────────────────────────────
            if (mSongMode.load(std::memory_order_relaxed))
            {
                // Fire pending note-offs in this block's window BEFORE scheduling new ons.
                // (Same pattern as pattern-mode branch — prevents stuck notes when
                // blocks end mid-note.)
                {
                    std::vector<PRPendingOff> keep;
                    for (auto& off : mPRPendingOffs)
                    {
                        if (off.beatOff <= beatStart)
                        {
                            if (off.target < kMaxLayerPages)
                                layerPageMidi[off.target].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                            else if (off.target >= kBassPRTarget && off.target < kBassPRTarget + kMaxBassPages)
                                bassPageMidi[off.target - kBassPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                            else if (off.target >= kDrumPRTarget && off.target < kDrumPRTarget + kMaxDrumPages)
                                drumPageMidi[off.target - kDrumPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                            else if (off.target >= kClipPRTarget && off.target < kClipPRTarget + kMaxClipPages)
                                clipPageMidi[off.target - kClipPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                            else if (off.target >= kVoxPRTarget && off.target < kVoxPRTarget + kMaxVoxPages)
                                voxPageMidi[off.target - kVoxPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                            else if (off.target >= kInstPRTarget && off.target < kInstPRTarget + kMaxInstPages)
                                instPageMidi[off.target - kInstPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                            continue;
                        }
                        if (off.beatOff < beatEnd)
                        {
                            int smp = juce::jlimit(0, numSamples - 1,
                                                   (int)((off.beatOff - beatStart) / bs));
                            if (off.target < kMaxLayerPages)
                                layerPageMidi[off.target].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), smp);
                            else if (off.target >= kBassPRTarget && off.target < kBassPRTarget + kMaxBassPages)
                                bassPageMidi[off.target - kBassPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), smp);
                            else if (off.target >= kDrumPRTarget && off.target < kDrumPRTarget + kMaxDrumPages)
                                drumPageMidi[off.target - kDrumPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), smp);
                            else if (off.target >= kClipPRTarget && off.target < kClipPRTarget + kMaxClipPages)
                                clipPageMidi[off.target - kClipPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), smp);
                            else if (off.target >= kVoxPRTarget && off.target < kVoxPRTarget + kMaxVoxPages)
                                voxPageMidi[off.target - kVoxPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), smp);
                            else if (off.target >= kInstPRTarget && off.target < kInstPRTarget + kMaxInstPages)
                                instPageMidi[off.target - kInstPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), smp);
                        }
                        else
                        {
                            keep.push_back(off);
                        }
                    }
                    mPRPendingOffs = std::move(keep);
                }

                // Detect song end — request transport stop (or let playhead
                // wrap via mLoopBeats, which is set by the UI when loop mode is on).
                // 2026-04-26: also fire stop when songEnd <= 0 in play-through —
                // empty arrangement was previously playing indefinitely because the
                // `songEnd > 0` guard skipped this block entirely.
                const double songEnd = mSongEndBeats.load(std::memory_order_relaxed);
                const bool   loopOff = ! mSongLoopMode.load(std::memory_order_relaxed);
                if (loopOff && (songEnd <= 0.0 || beatStart >= songEnd))
                {
                    mRequestStop.store(true, std::memory_order_release);
                }

                // Schedule notes from all Pattern arrangement blocks that overlap current beat range.
                // No loop wrap — playhead advances linearly until stop.
                constexpr double kBPB = 4.0;  // beats per bar (4/4 time)
                for (int blkIdx = 0; blkIdx < mPatternManager->getNumBlocks(); ++blkIdx)
                {
                    const auto& blk = mPatternManager->getBlock(blkIdx);
                    if (blk.clipType != ClipType::Pattern || blk.muted) continue;
                    if (!mPatternManager->isRowAudible(blk.trackRow)) continue;
                    if (blk.patternIndex < 0 || blk.patternIndex >= mPatternManager->getNumPatterns()) continue;

                    double blkStartBeat = blk.startBar * kBPB;
                    double blkEndBeat   = (blk.startBar + blk.lengthBars) * kBPB;
                    if (beatEnd <= blkStartBeat || beatStart >= blkEndBeat) continue;

                    const auto& sPat   = mPatternManager->getPattern(blk.patternIndex);
                    double patOwnLen   = juce::jmax(1.0, (double)sPat.bars * kBPB);

                    // Helper: schedule notes from a roll, repeating within the block if needed.
                    // Also schedules note-offs into mPRPendingOffs so voices don't hang.
                    // `target` is the mPRPendingOffs engine key (layer idx, or kBassPRTarget+bass idx).
                    auto scheduleRoll = [&](const std::vector<PianoNote>& notes,
                                           juce::MidiBuffer& buf, int target)
                    {
                        for (const auto& note : notes)
                        {
                            if (note.muted) continue;
                            for (double rep = 0.0; ; rep += patOwnLen)
                            {
                                double absStart = blkStartBeat + rep + note.startBeat;
                                if (absStart >= blkEndBeat) break;
                                if (rep > blk.lengthBars * kBPB) break;  // safety
                                if (absStart >= beatStart && absStart < beatEnd)
                                {
                                    int smp = juce::jlimit(0, numSamples - 1,
                                        (int)juce::jmax(0.0, (absStart - beatStart) / bs));
                                    emitPianoNoteOn (buf, note, smp);
                                    // Schedule matching note-off; clamp to block end so a
                                    // stretched-short block silences notes that would
                                    // otherwise hang past it.
                                    double offBeat = juce::jmin(absStart + note.durationBeats,
                                                                blkEndBeat);
                                    mPRPendingOffs.push_back({ offBeat, note.midiNote, target });
                                }
                            }
                        }
                    };

                    for (int pi = 0; pi < kMaxLayerPages; ++pi)
                        scheduleRoll(sPat.layerRoll[pi].notes, layerPageMidi[pi], pi);
                    for (int bi2 = 0; bi2 < kMaxBassPages; ++bi2)
                        scheduleRoll(sPat.bassRoll[bi2].notes, bassPageMidi[bi2],
                                     kBassPRTarget + bi2);
                    // D1.2: per-drum-page rolls (dynamic-drum model).  Bypass
                    // when no DrumPage tabs exist (D1.3+).
                    // D1.4-fix (c) revert: NO transpose compensation.  Preset
                    // transpose IS the sound design — it positions the drum's
                    // acoustic frequency relative to the C5 trigger.  Sending
                    // the raw pitch lets each drum play its native voice when
                    // triggered from C5; pitches above/below C5 retune from
                    // there (standard drum-machine semantics).
                    if (mAnyDrumPageActive.load(std::memory_order_acquire))
                    {
                        juce::SpinLock::ScopedTryLockType dlk(mDrumEngineLock);
                        if (dlk.isLocked())
                        {
                            for (int di = 0; di < kMaxDrumPages; ++di)
                                if (mDrumEngines[di])
                                    scheduleRoll(sPat.drumRolls[di].notes, drumPageMidi[di],
                                                 kDrumPRTarget + di);
                        }
                    }

                    // G-3 (2026-04-28): per-clip-page rolls.  Same fast-path
                    // bypass + try-lock pattern as the drum branch above.
                    if (mAnyClipPageActive.load(std::memory_order_acquire))
                    {
                        juce::SpinLock::ScopedTryLockType clk(mClipEngineLock);
                        if (clk.isLocked())
                        {
                            for (int ci = 0; ci < kMaxClipPages; ++ci)
                                if (mClipEngines[ci])
                                    scheduleRoll(sPat.clipRoll[ci].notes, clipPageMidi[ci],
                                                 kClipPRTarget + ci);
                        }
                    }

                    // G-4 (2026-04-28): per-Vox / per-Inst-page rolls (same shape).
                    if (mAnyVoxPageActive.load(std::memory_order_acquire))
                    {
                        juce::SpinLock::ScopedTryLockType vlk(mVoxEngineLock);
                        if (vlk.isLocked())
                        {
                            for (int vi = 0; vi < kMaxVoxPages; ++vi)
                                if (mVoxEngines[vi])
                                    scheduleRoll(sPat.voxRoll[vi].notes, voxPageMidi[vi],
                                                 kVoxPRTarget + vi);
                        }
                    }
                    if (mAnyInstPageActive.load(std::memory_order_acquire))
                    {
                        juce::SpinLock::ScopedTryLockType ilk(mInstEngineLock);
                        if (ilk.isLocked())
                        {
                            for (int ii = 0; ii < kMaxInstPages; ++ii)
                                if (mInstEngines[ii])
                                    scheduleRoll(sPat.instRoll[ii].notes, instPageMidi[ii],
                                                 kInstPRTarget + ii);
                        }
                    }

                    // 2026-04-25: legacy sPat.drumRoll dispatch removed —
                    // notes are now in sPat.drumRolls[di] and dispatched
                    // through D1.2 per-drum loop above.
                }
                mPRLastBeatEnd = beatEnd;
            }
            else
            {
            // ── PATTERN MODE (existing loop-based scheduling) ─────────────
            auto& pat     = mPatternManager->currentPattern();
            double patLen = juce::jmax(1.0, mCachedPatternLoopBeats.load(std::memory_order_relaxed));

            // kWrapSlop = one block's worth of beats.  After a loop wrap, beatStart lands
            // slightly past 0 (e.g. 0.021 beats) due to fmod float arithmetic, so a note
            // at beat 0 would be missed without extending the window backward.
            // We ONLY extend the window on jumped (post-wrap) blocks — applying it to every
            // block would create a double-trigger: block N bumps beat-0's absStart to patLen
            // (falls in upper window) AND block N+1 catches it via the slop (lower window).
            const double kWrapSlop = beatEnd - beatStart;
            bool jumped = (mPRLastBeatEnd >= 0.0 && beatStart < mPRLastBeatEnd - 0.1);
            const double windowStart = jumped ? (beatStart - kWrapSlop) : beatStart;

            if (jumped)
            {
                // Loop restart (seek or wrap) — fire per-note offs for each pending voice
                // so their release envelopes play naturally.  allNotesOff() does a hard
                // kill on ALL channel-1 voices (click + cuts layer/bass cross-page) —
                // per-note offs only affect voices we actually started.
                for (auto& off : mPRPendingOffs)
                {
                    if (off.target < kMaxLayerPages)
                        layerPageMidi[off.target].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                    else if (off.target >= kBassPRTarget && off.target < kBassPRTarget + kMaxBassPages)
                        bassPageMidi[off.target - kBassPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                    else if (off.target >= kDrumPRTarget && off.target < kDrumPRTarget + kMaxDrumPages)
                        drumPageMidi[off.target - kDrumPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                    else if (off.target >= kClipPRTarget && off.target < kClipPRTarget + kMaxClipPages)
                        clipPageMidi[off.target - kClipPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                    else if (off.target >= kVoxPRTarget && off.target < kVoxPRTarget + kMaxVoxPages)
                        voxPageMidi[off.target - kVoxPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                    else if (off.target >= kInstPRTarget && off.target < kInstPRTarget + kMaxInstPages)
                        instPageMidi[off.target - kInstPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                }
                mPRPendingOffs.clear();
            }
            mPRLastBeatEnd = beatEnd;

            // Fire pending note-offs that fall within this block
            {
                std::vector<PRPendingOff> keep;
                for (auto& off : mPRPendingOffs)
                {
                    // Past-due: fire at sample 0 instead of silently leaking.
                    // This prevents stuck voices when patLen changes mid-playback.
                    if (off.beatOff <= beatStart)
                    {
                        if (off.target < kMaxLayerPages)
                            layerPageMidi[off.target].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                        else if (off.target >= kBassPRTarget && off.target < kBassPRTarget + kMaxBassPages)
                            bassPageMidi[off.target - kBassPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                        else if (off.target >= kDrumPRTarget && off.target < kDrumPRTarget + kMaxDrumPages)
                            drumPageMidi[off.target - kDrumPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                        else if (off.target >= kClipPRTarget && off.target < kClipPRTarget + kMaxClipPages)
                            clipPageMidi[off.target - kClipPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                        else if (off.target >= kVoxPRTarget && off.target < kVoxPRTarget + kMaxVoxPages)
                            voxPageMidi[off.target - kVoxPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                        else if (off.target >= kInstPRTarget && off.target < kInstPRTarget + kMaxInstPages)
                            instPageMidi[off.target - kInstPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                        continue;
                    }
                    if (off.beatOff < beatEnd)
                    {
                        int smp = juce::jlimit(0, numSamples - 1,
                                               (int)((off.beatOff - beatStart) / bs));
                        if (off.target < kMaxLayerPages)
                            layerPageMidi[off.target].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), smp);
                        else if (off.target >= kBassPRTarget && off.target < kBassPRTarget + kMaxBassPages)
                            bassPageMidi[off.target - kBassPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), smp);
                        else if (off.target >= kDrumPRTarget && off.target < kDrumPRTarget + kMaxDrumPages)
                            drumPageMidi[off.target - kDrumPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), smp);
                        else if (off.target >= kClipPRTarget && off.target < kClipPRTarget + kMaxClipPages)
                            clipPageMidi[off.target - kClipPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), smp);
                        else if (off.target >= kVoxPRTarget && off.target < kVoxPRTarget + kMaxVoxPages)
                            voxPageMidi[off.target - kVoxPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), smp);
                        else if (off.target >= kInstPRTarget && off.target < kInstPRTarget + kMaxInstPages)
                            instPageMidi[off.target - kInstPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), smp);
                    }
                    else
                    {
                        keep.push_back(off);
                    }
                }
                mPRPendingOffs = std::move(keep);
            }

            // Trigger note-ons from layer piano rolls (up to 8 pages)
            for (int i = 0; i < kMaxLayerPages; ++i)
            {
                for (const auto& note : pat.layerRoll[i].notes)
                {
                    if (note.muted) continue;
                    double baseLoop = std::floor(beatStart / patLen) * patLen;
                    double absStart = baseLoop + note.startBeat;
                    // Bump notes that fall behind the bump threshold into this cycle.
                    if (absStart < beatStart - kWrapSlop) absStart += patLen;
                    // If absStart landed AT or past patLen in a straddling block, skip it —
                    // the next (jumped) block will catch it via the windowStart slop.
                    if (absStart >= patLen && beatEnd > patLen) continue;
                    if (absStart >= windowStart && absStart < beatEnd)
                    {
                        int smp = juce::jlimit(0, numSamples - 1,
                                               (int)std::max(0.0, (absStart - beatStart) / bs));
                        emitPianoNoteOn (layerPageMidi[i], note, smp);
                        mPRPendingOffs.push_back(
                            { absStart + note.durationBeats, note.midiNote, i });
                    }
                }
            }

            // Trigger note-ons from bass piano rolls (up to kMaxBassPages pages)
            for (int bi = 0; bi < kMaxBassPages; ++bi)
            {
                for (const auto& note : pat.bassRoll[bi].notes)
                {
                    if (note.muted) continue;
                    double baseLoop = std::floor(beatStart / patLen) * patLen;
                    double absStart = baseLoop + note.startBeat;
                    if (absStart < beatStart - kWrapSlop) absStart += patLen;
                    if (absStart >= patLen && beatEnd > patLen) continue;
                    if (absStart >= windowStart && absStart < beatEnd)
                    {
                        int smp = juce::jlimit(0, numSamples - 1,
                                               (int)std::max(0.0, (absStart - beatStart) / bs));
                        emitPianoNoteOn (bassPageMidi[bi], note, smp);
                        mPRPendingOffs.push_back(
                            { absStart + note.durationBeats, note.midiNote, kBassPRTarget + bi });
                    }
                }
            }

            // D1.2: per-drum-page rolls (dynamic-drum model).  Bypass when
            // no DrumPage tabs exist (D1.3+).  No transpose compensation —
            // see song-mode block above for rationale.
            if (mAnyDrumPageActive.load(std::memory_order_acquire))
            {
                juce::SpinLock::ScopedTryLockType dlk(mDrumEngineLock);
                const bool drumLockHeld = dlk.isLocked();
                for (int di = 0; di < kMaxDrumPages; ++di)
                {
                    if (! drumLockHeld || ! mDrumEngines[di]) continue;
                    for (const auto& note : pat.drumRolls[di].notes)
                    {
                        if (note.muted) continue;
                        double baseLoop = std::floor(beatStart / patLen) * patLen;
                        double absStart = baseLoop + note.startBeat;
                        if (absStart < beatStart - kWrapSlop) absStart += patLen;
                        if (absStart >= patLen && beatEnd > patLen) continue;
                        if (absStart >= windowStart && absStart < beatEnd)
                        {
                            int smp = juce::jlimit(0, numSamples - 1,
                                                   (int)std::max(0.0, (absStart - beatStart) / bs));
                            emitPianoNoteOn (drumPageMidi[di], note, smp);
                            mPRPendingOffs.push_back(
                                { absStart + note.durationBeats, note.midiNote, kDrumPRTarget + di });
                        }
                    }
                }
            }

            // 2026-04-25: legacy pat.drumRoll dispatch removed —
            // notes are now in pat.drumRolls[di] and dispatched
            // through D1.2 per-drum loop above.

            // G-3 (2026-04-28): per-clip-page rolls — pattern mode.  Mirrors
            // the drum loop above; bypass when no Clips tabs are active.
            if (mAnyClipPageActive.load(std::memory_order_acquire))
            {
                juce::SpinLock::ScopedTryLockType clk(mClipEngineLock);
                const bool clipLockHeld = clk.isLocked();
                for (int ci = 0; ci < kMaxClipPages; ++ci)
                {
                    if (! clipLockHeld || ! mClipEngines[ci]) continue;
                    for (const auto& note : pat.clipRoll[ci].notes)
                    {
                        if (note.muted) continue;
                        double baseLoop = std::floor(beatStart / patLen) * patLen;
                        double absStart = baseLoop + note.startBeat;
                        if (absStart < beatStart - kWrapSlop) absStart += patLen;
                        if (absStart >= patLen && beatEnd > patLen) continue;
                        if (absStart >= windowStart && absStart < beatEnd)
                        {
                            int smp = juce::jlimit(0, numSamples - 1,
                                                   (int)std::max(0.0, (absStart - beatStart) / bs));
                            emitPianoNoteOn (clipPageMidi[ci], note, smp);
                            mPRPendingOffs.push_back(
                                { absStart + note.durationBeats, note.midiNote, kClipPRTarget + ci });
                        }
                    }
                }
            }

            // G-4 (2026-04-28): per-Vox / per-Inst-page rolls — pattern mode.
            if (mAnyVoxPageActive.load(std::memory_order_acquire))
            {
                juce::SpinLock::ScopedTryLockType vlk(mVoxEngineLock);
                const bool vlkHeld = vlk.isLocked();
                for (int vi = 0; vi < kMaxVoxPages; ++vi)
                {
                    if (! vlkHeld || ! mVoxEngines[vi]) continue;
                    for (const auto& note : pat.voxRoll[vi].notes)
                    {
                        if (note.muted) continue;
                        double baseLoop = std::floor(beatStart / patLen) * patLen;
                        double absStart = baseLoop + note.startBeat;
                        if (absStart < beatStart - kWrapSlop) absStart += patLen;
                        if (absStart >= patLen && beatEnd > patLen) continue;
                        if (absStart >= windowStart && absStart < beatEnd)
                        {
                            int smp = juce::jlimit(0, numSamples - 1,
                                                   (int)std::max(0.0, (absStart - beatStart) / bs));
                            emitPianoNoteOn (voxPageMidi[vi], note, smp);
                            mPRPendingOffs.push_back(
                                { absStart + note.durationBeats, note.midiNote, kVoxPRTarget + vi });
                        }
                    }
                }
            }
            if (mAnyInstPageActive.load(std::memory_order_acquire))
            {
                juce::SpinLock::ScopedTryLockType ilk(mInstEngineLock);
                const bool ilkHeld = ilk.isLocked();
                for (int ii = 0; ii < kMaxInstPages; ++ii)
                {
                    if (! ilkHeld || ! mInstEngines[ii]) continue;
                    for (const auto& note : pat.instRoll[ii].notes)
                    {
                        if (note.muted) continue;
                        double baseLoop = std::floor(beatStart / patLen) * patLen;
                        double absStart = baseLoop + note.startBeat;
                        if (absStart < beatStart - kWrapSlop) absStart += patLen;
                        if (absStart >= patLen && beatEnd > patLen) continue;
                        if (absStart >= windowStart && absStart < beatEnd)
                        {
                            int smp = juce::jlimit(0, numSamples - 1,
                                                   (int)std::max(0.0, (absStart - beatStart) / bs));
                            emitPianoNoteOn (instPageMidi[ii], note, smp);
                            mPRPendingOffs.push_back(
                                { absStart + note.durationBeats, note.midiNote, kInstPRTarget + ii });
                        }
                    }
                }
            }
            } // end pattern mode else
        }
    }

    // ── Drum + Bass basic-sequence step triggering ────────────────────────
    if (!pos.getIsPlaying())
    {
        mLastDrumStep = -1;
        mLastBassStep = -1;
    }
    else if (mPatternManager)
    {
        auto& pat      = mPatternManager->currentPattern();
        double ppqPos  = pos.getPpqPosition().orFallback(0.0);
        double stepLen = pat.stepLengthBeats();
        int    step    = (stepLen > 0.0 && pat.totalSteps() > 0)
            ? (int)(ppqPos / stepLen) % pat.totalSteps() : 0;

        if (step != mLastDrumStep && step >= 0 && step < pat.totalSteps())
        {
            mLastDrumStep = step;
            // Legacy basic-step-sequencer drum trigger removed (DrumSynth gone).
            // Per-drum-tab notes now flow through D1.2 dispatch above.
        }

        if (step != mLastBassStep && step >= 0 && step < pat.totalSteps())
        {
            mLastBassStep = step;
            const auto& bStep = pat.bassSeq.basicGrid[0][step];
            if (bStep.active)
                mBassSynth.noteOn((int)mBassSynth.getParams().pitch, bStep.velocity);
            else
                mBassSynth.noteOff();
        }
    }

    // ── Automation clip playback ──────────────────────────────────────────
    if (pos.getIsPlaying() && mPatternManager)
    {
        const double kBeatsPerBar = 4.0;   // assumes 4/4; good enough for 4C
        double autoBeat = pos.getPpqPosition().orFallback(0.0);
        double autoBar  = autoBeat / kBeatsPerBar;

        for (int bi = 0; bi < mPatternManager->getNumBlocks(); ++bi)
        {
            const auto& blk = mPatternManager->getBlock(bi);
            if (blk.clipType  != ClipType::Automation) continue;
            if (blk.muted)                              continue;
            if (!mPatternManager->isRowAudible(blk.trackRow)) continue;
            if (blk.automationLane.paramId.isEmpty())   continue;
            if (blk.automationLane.points.empty())      continue;

            double clipStart = (double)blk.startBar;
            double clipEnd   = clipStart + (double)blk.lengthBars;
            if (autoBar < clipStart || autoBar >= clipEnd) continue;

            float relPos = (float)((autoBar - clipStart) / (double)blk.lengthBars);
            relPos = juce::jlimit(0.f, 1.f, relPos);

            // Interpolate control points (assume sorted by timeTicks)
            const auto& pts = blk.automationLane.points;
            float value01 = pts[0].value01;  // default: first point

            if ((int)pts.size() == 1)
            {
                value01 = pts[0].value01;
            }
            else if (relPos <= pts.front().timeTicks)
            {
                value01 = pts.front().value01;
            }
            else if (relPos >= pts.back().timeTicks)
            {
                value01 = pts.back().value01;
            }
            else
            {
                for (int pi = 0; pi < (int)pts.size() - 1; ++pi)
                {
                    if (relPos >= pts[pi].timeTicks && relPos <= pts[pi + 1].timeTicks)
                    {
                        if (pts[pi].curveType == CurveType::Stepped)
                        {
                            value01 = pts[pi].value01;
                        }
                        else
                        {
                            float span = pts[pi + 1].timeTicks - pts[pi].timeTicks;
                            float t    = (span > 0.f)
                                ? (relPos - pts[pi].timeTicks) / span : 0.f;
                            value01 = pts[pi].value01
                                    + t * (pts[pi + 1].value01 - pts[pi].value01);
                        }
                        break;
                    }
                }
            }

            // Apply to APVTS parameter (setValue is audio-thread-safe)
            if (auto* param = apvts.getParameter(blk.automationLane.paramId))
                param->setValue(juce::jlimit(0.f, 1.f, value01));
        }
    }

    // ── Render per-page layer engine processors ───────────────────────────
    const int numRenderCh = 2;
    mLayerEngineSum.setSize(numRenderCh, numSamples, false, false, true);
    mLayerEngineSum.clear();

    // 5F-4a Batch 6: compute anySolo across all inserts once per block
    const bool anySolo = mVibeGraph.isAnyInsertSoloed();

    // 5F-4b B1b: refresh routing graph + clear per-channel accumulators
    mVibeGraph.rebuildRoutingFromApvts();
    mVibeGraph.clearChannelAccumulators();
    const auto& routingEdges = mVibeGraph.getRoutingGraph().edges();

    // Helper: fan this source channel's output out to all destinations listed
    // in the RoutingGraph (main-out + active sends). Called after each
    // InsertNode processBlock so the destination's accumulator is populated
    // before bus/master processing downstream.
    auto routeInsertOutput = [&](int srcChannelId,
                                  const juce::AudioBuffer<float>& buf,
                                  int n)
    {
        const int nc = juce::jmin(2, buf.getNumChannels());
        for (const auto& e : routingEdges)
        {
            if (e.srcId != srcChannelId) continue;
            if (auto* dst = mVibeGraph.getChannelAccumulator(e.dstId))
            {
                const float gain = e.isMainOut
                    ? 1.f
                    : juce::Decibels::decibelsToGain(e.amountDb, -60.f);
                for (int c = 0; c < nc; ++c)
                    dst->addFrom(c, 0, buf, c, 0, n, gain);
            }
        }
    };
    const double bpmForInserts = pos.getBpm().orFallback(120.0);

    // ── D3: choke-group dispatch ──────────────────────────────────────────
    // Scan synth note-ons + audio clip starts in this block; for each fire
    // whose source has chokeGroup G > 0, inject allNotesOff into peer synth
    // inserts AND set mutedByChoke on peer audio clips in the same group.
    // Runs before synth + audio rendering so both surfaces respect the cut.
    {
        const double bpmCh        = pos.getBpm().orFallback(120.0);
        const double secPerBeatCh = 60.0 / bpmCh;
        const double beatStartCh  = pos.getPpqPosition().orFallback(0.0);
        const juce::int64 projStartSamp = (juce::int64) (beatStartCh * secPerBeatCh * mSampleRate);
        applyChokeGroupDispatch(layerPageMidi, bassPageMidi, drumPageMidi,
                                voxPageMidi,   instPageMidi,
                                projStartSamp, numSamples, secPerBeatCh);
    }

    {
        juce::SpinLock::ScopedTryLockType lk(mLayerEngineLock);
        if (lk.isLocked())
        {
            for (int i = 0; i < kMaxLayerPages; ++i)
            {
                if (!mLayerEngines[i]) continue;
                mLayerEngineScratch.setSize(numRenderCh, numSamples, false, false, true);
                mLayerEngineScratch.clear();
                mLayerEngines[i]->processBlock(mLayerEngineScratch, layerPageMidi[i]);
                // §P4.3 B7: legacy per-page pre-rack EQ removed — pre-rack EQ is
                // now the InsertNode's own preEq member (runs inside processInsert).
                // 5F-4a Batch 6: InsertNode (polarity → preEq → width → rack →
                // post-rack EQ → fader × mute × solo → PDC → peak)
                mVibeGraph.processInsert(VibeGraph::InsertKind::Layer, i,
                                          mLayerEngineScratch, bpmForInserts, anySolo);
                // 5F-4b B1b: route this insert's output to its sendTo + active sends
                routeInsertOutput(MixerChannelIds::layerInsert(i),
                                   mLayerEngineScratch, numSamples);
            }
        }
    }

    // Guard against NaN/Inf from engine processors reaching the hardware.
    // Windows WASAPI silences the entire device stream when it sees NaN output.
    // Use a sum-based check — SIMD getMagnitude() may silently swallow NaN.
    {
        float check = 0.0f;
        if (mLayerEngineSum.getNumChannels() > 0)
        {
            const float* d = mLayerEngineSum.getReadPointer(0);
            for (int s = 0; s < numSamples; ++s) check += d[s];
        }
        if (!std::isfinite(check))
            mLayerEngineSum.clear();
    }

    // ── Render bass engine processors (up to kMaxBassPages) ──────────────
    mBassEngineBuf.setSize(numRenderCh, numSamples, false, false, true);
    mBassEngineBuf.clear();
    {
        juce::SpinLock::ScopedTryLockType lk(mBassEngineLock);
        if (lk.isLocked())
        {
            for (int i = 0; i < kMaxBassPages; ++i)
            {
                if (!mBassEngines[i]) continue;
                mBassEngineScratch.setSize(numRenderCh, numSamples, false, false, true);
                mBassEngineScratch.clear();
                mBassEngines[i]->processBlock(mBassEngineScratch, bassPageMidi[i]);
                // §P4.3 B7: legacy per-page pre-rack EQ removed — see Layer loop.
                // 5F-4a Batch 6: InsertNode handles polarity/preEq/width/rack/EQ/fader/mute/solo
                mVibeGraph.processInsert(VibeGraph::InsertKind::Bass, i,
                                          mBassEngineScratch, bpmForInserts, anySolo);
                // 5F-4b B1b: route this insert's output to its sendTo + active sends
                routeInsertOutput(MixerChannelIds::bassInsert(i),
                                   mBassEngineScratch, numSamples);
            }
        }
    }
    {
        float check = 0.0f;
        if (mBassEngineBuf.getNumChannels() > 0)
        {
            const float* d = mBassEngineBuf.getReadPointer(0);
            for (int s = 0; s < numSamples; ++s) check += d[s];
        }
        if (!std::isfinite(check))
            mBassEngineBuf.clear();
    }

    // 2026-04-25: legacy mDrumsEngine drum render block removed (legacy
    // BaySickDrumsProcessor + 16-slot dispatch deleted).  Per-drum-tab
    // engines now own all drum playback via the D1.2 loop below.

    // ── D1.2: Render per-drum-page engines (dynamic-drum model) ─────────────
    // Bypass entirely when no DrumPage tabs exist (D1.3+).
    if (mAnyDrumPageActive.load(std::memory_order_acquire))
    {
        juce::SpinLock::ScopedTryLockType lk(mDrumEngineLock);
        if (lk.isLocked())
        {
            for (int i = 0; i < kMaxDrumPages; ++i)
            {
                if (!mDrumEngines[i]) continue;
                mDrumEngineScratch.setSize(numRenderCh, numSamples, false, false, true);
                mDrumEngineScratch.clear();
                mDrumEngines[i]->processBlock(mDrumEngineScratch, drumPageMidi[i]);
                mVibeGraph.processInsert(VibeGraph::InsertKind::Drum, i,
                                          mDrumEngineScratch, bpmForInserts, anySolo);
                routeInsertOutput(MixerChannelIds::drumInsert(i),
                                   mDrumEngineScratch, numSamples);
            }
        }
    }

    // ── G-3 (2026-04-28): Render per-clip-page engines ──────────────────────
    // Same shape as the drum-engine loop above, but routes the engine output
    // through the existing Audio InsertNode for the bound row (clip-page-index
    // = audio-row-index, 1:1 mapping per VibesynthConstants.h).  In song mode
    // the row's InsertNode is also processed by the audio_clip_players loop
    // below (arrangement-playback path) — both paths feed the same rack /
    // EQ / fader sequentially.  Fast-path bypass via mAnyClipPageActive.
    if (mAnyClipPageActive.load(std::memory_order_acquire))
    {
        juce::SpinLock::ScopedTryLockType lk(mClipEngineLock);
        if (lk.isLocked())
        {
            for (int ci = 0; ci < kMaxClipPages; ++ci)
            {
                if (!mClipEngines[ci]) continue;
                mClipEngineScratch.setSize(numRenderCh, numSamples, false, false, true);
                mClipEngineScratch.clear();
                mClipEngines[ci]->processBlock(mClipEngineScratch, clipPageMidi[ci]);
                mVibeGraph.processInsert(VibeGraph::InsertKind::Audio, ci,
                                          mClipEngineScratch, bpmForInserts, anySolo);
                mAudioRowPeakDb[ci].store(
                    mVibeGraph.getInsertPeakDb(VibeGraph::InsertKind::Audio, ci),
                    std::memory_order_relaxed);
                routeInsertOutput(MixerChannelIds::audioInsert(ci),
                                   mClipEngineScratch, numSamples);
            }
        }
    }

    // ── G-4 (2026-04-28): Render per-Vox / per-Inst-page engines ────────────
    // Same shape as the Clip loop above; routes engine output through the
    // existing Vox / Inst InsertNode.  Fast-path bypass via mAnyXPageActive.
    if (mAnyVoxPageActive.load(std::memory_order_acquire))
    {
        juce::SpinLock::ScopedTryLockType lk(mVoxEngineLock);
        if (lk.isLocked())
        {
            for (int vi = 0; vi < kMaxVoxPages; ++vi)
            {
                if (!mVoxEngines[vi]) continue;
                mVoxEngineScratch.setSize(numRenderCh, numSamples, false, false, true);
                mVoxEngineScratch.clear();
                mVoxEngines[vi]->processBlock(mVoxEngineScratch, voxPageMidi[vi]);
                mVibeGraph.processInsert(VibeGraph::InsertKind::Vox, vi,
                                          mVoxEngineScratch, bpmForInserts, anySolo);
                routeInsertOutput(MixerChannelIds::voxInsert(vi),
                                   mVoxEngineScratch, numSamples);
            }
        }
    }
    if (mAnyInstPageActive.load(std::memory_order_acquire))
    {
        juce::SpinLock::ScopedTryLockType lk(mInstEngineLock);
        if (lk.isLocked())
        {
            for (int ii = 0; ii < kMaxInstPages; ++ii)
            {
                if (!mInstEngines[ii]) continue;
                mInstEngineScratch.setSize(numRenderCh, numSamples, false, false, true);
                mInstEngineScratch.clear();
                mInstEngines[ii]->processBlock(mInstEngineScratch, instPageMidi[ii]);
                mVibeGraph.processInsert(VibeGraph::InsertKind::Inst, ii,
                                          mInstEngineScratch, bpmForInserts, anySolo);
                routeInsertOutput(MixerChannelIds::instInsert(ii),
                                   mInstEngineScratch, numSamples);
            }
        }
    }

    // ── Audio clip playback — runs BEFORE VibeGraph so master rack sees clips ─────
    // Signal chain per clip: per-clip rack → per-clip fader/mute → bus accumulator
    // Then: bus rack → bus fader/mute → passed into VibeGraph as audioClipsPreRendered
    double bpm = pos.getBpm().orFallback(120.0);
    juce::AudioBuffer<float>* audioClipsBusForGraph = nullptr;

    if (mSongMode.load(std::memory_order_relaxed) && pos.getIsPlaying() && mPatternManager)
    {
        const double bpmAC       = bpm;
        const double secPerBeat  = 60.0 / bpmAC;
        const double beatStartAC = pos.getPpqPosition().orFallback(0.0);

        const int64 projectStart = (int64)(beatStartAC * secPerBeat * mSampleRate);
        const int64 projectEnd   = projectStart + numSamples;

        const auto& mx = mPatternManager->getMixer();

        // Master gain = APVTS knob × mixer master fader
        float masterGain = mx.masterLevel;
        if (auto* p = apvts.getRawParameterValue("masterGain"))
            masterGain *= p->load();

        juce::SpinLock::ScopedTryLockType tryLk (mAudioClipLock);
        if (tryLk.isLocked())
        {
            // Bus accumulation buffer: all per-clip processed audio sums here.
            const int numOut = buffer.getNumChannels();
            mAudioRowScratch .setSize(numOut, numSamples, false, false, true);
            mAudioClipScratch.setSize(numOut, numSamples, false, false, true);
            mAudioRowScratch.clear();

            for (auto& player : mAudioClipPlayers)
            {
                if (player.streamer == nullptr) continue;

                const int64 clipStart = (int64)(player.clipStartBeat * secPerBeat * mSampleRate);
                const int64 clipEnd   = (int64)(player.clipEndBeat   * secPerBeat * mSampleRate);

                if (projectEnd <= clipStart || projectStart >= clipEnd) continue;

                const int   row      = player.trackRow;
                const bool  inRange  = (row >= 0 && row < kMaxAudioRows);
                const bool  rowMuted = inRange && mx.audioRowMute[row];
                const float rowLevel = inRange ? mx.audioRowLevel[row] : 1.0f;
                const bool  builderRowMuted = !mPatternManager->isRowAudible(row);

                // D3: skip clips that have been silenced by a choke fire.
                if (player.mutedByChoke)
                {
                    if (inRange) mAudioRowPeakDb[row].store (-60.0f, std::memory_order_relaxed);
                    continue;
                }

                if (rowMuted || builderRowMuted)
                {
                    if (inRange) mAudioRowPeakDb[row].store (-60.0f, std::memory_order_relaxed);
                    continue;
                }

                // Output samples before clip starts in this block
                const int bufOffset = (int)juce::jmax ((int64)0, clipStart - projectStart);

                // Position within the clip in output samples
                const int64 outPosInClip = (projectStart + bufOffset) - clipStart;

                // readRatio = file samples per output sample (SR conversion only).
                const double readRatio = player.fileSampleRate / mSampleRate;
                const int64  filePos   = (int64)(outPosInClip * readRatio);

                const int64 fileTotalSamples = player.streamer->getTotalLength();
                if (filePos >= fileTotalSamples)
                {
                    if (inRange) mAudioRowPeakDb[row].store (-60.0f, std::memory_order_relaxed);
                    continue;
                }

                // Stretch ratio for EOF calculation (1.0 when not stretching).
                const double stretchRatio = (player.vocoder != nullptr
                                             && player.stretchMode
                                             && player.originalBPM > 0.f)
                    ? (double) player.originalBPM / bpmAC
                    : 1.0;

                // Actual file EOF in output-timeline samples.
                // Without stretch: fileTotalSamples / readRatio output samples from clipStart.
                // With stretch:    stretched duration = file_time * stretchRatio.
                const int64 fileEOFOutput = clipStart
                    + (int64) ((double) fileTotalSamples * stretchRatio / readRatio);

                // Output samples to fill this block — capped by BOTH block boundary
                // AND actual file EOF so playback stops when the audio ends.
                const int64 effectiveClipEnd = juce::jmin (clipEnd, fileEOFOutput);
                const int outSamples = (int)juce::jmin (
                    (int64)(numSamples - bufOffset),
                    effectiveClipEnd - (projectStart + bufOffset));

                if (outSamples <= 0) continue;

                // Per-clip scratch: clear for this clip's contribution.
                // Clip writes here → per-clip rack → fader/mute → add to bus accumulator.
                mAudioClipScratch.clear();

                const float gain   = masterGain;  // row fader applied separately after rack
                float       peak   = 0.0f;

                const bool usePV = (player.vocoder != nullptr)
                                && player.stretchMode
                                && (player.originalBPM > 0.f)
                                && (std::abs (bpmAC - player.originalBPM) > 0.01);

                if (usePV)
                {
                    // ── Phase vocoder path (BPM stretch + pitch preservation) ──────
                    player.vocoder->setStretchRatio (stretchRatio);

                    // The PV consumes file samples at readRatio/stretchRatio per output
                    // sample — NOT readRatio (which is the direct-path rate).  We must
                    // therefore track the file read position SEQUENTIALLY in
                    // player.expectedFilePos rather than recomputing it from outPosInClip
                    // each block, because the two rates differ whenever stretchRatio != 1.
                    //
                    // Use the stateless stretched position only for SEEK DETECTION
                    // (position jumped backward or far forward = loop / scrub).
                    const int64 pvRefPos  = (int64) ((double) outPosInClip
                                                     * readRatio / stretchRatio);
                    const int64 pvReadPos = player.expectedFilePos;

                    // Seek detection threshold: 2 s of file samples.
                    // Normal per-block drift from rounding is ≤ 1 sample; over 2 s of
                    // playback the accumulated drift stays well below this threshold.
                    const bool seekNeeded =
                        (pvReadPos == 0 && pvRefPos > (int64) mSampleRate) ||
                        (pvReadPos  > 0 &&
                         std::abs (pvRefPos - pvReadPos) > (int64) (mSampleRate * 2));

                    if (seekNeeded)
                    {
                        player.vocoder->reset();
                        player.streamer->seek (pvRefPos);
                        player.expectedFilePos = pvRefPos;
                    }

                    // File samples consumed per output block:
                    //   outSamples (output SR) × readRatio (→ file SR) ÷ stretchRatio
                    //   (→ file consumption rate accounting for BPM stretch).
                    // NO extra kHopSize padding — that caused the ring read-head to
                    // overshoot the next block's read position, making readRaw fail
                    // with "filePos < ringReadHead" on every subsequent block.
                    const int numFileSamples = (int) std::ceil (
                        (double) outSamples * readRatio / stretchRatio);

                    // Read raw file samples from disk streamer (sequential position)
                    player.pvInBuf.clear();
                    const bool gotRaw = player.streamer->readRaw (
                        player.pvInBuf, 0, numFileSamples, player.expectedFilePos);

                    if (gotRaw)
                    {
                        // Advance sequential tracker ONLY on successful read
                        player.expectedFilePos += numFileSamples;

                        // Push into vocoder
                        player.vocoder->push (player.pvInBuf, 0, numFileSamples);

                        // How many vocoder output samples (still at file SR) we need
                        const int numVocOut = (int) std::ceil (
                            (double) outSamples * readRatio) + 2;

                        // Pull stretched samples (file SR, pitch-preserved)
                        player.pvOutBuf.clear();
                        const int pulled = player.vocoder->pull (
                            player.pvOutBuf, 0, numVocOut);

                        // Mix into output buffer with SR interpolation
                        if (pulled > 0)
                        {
                            const int pvCh = player.pvOutBuf.getNumChannels();
                            for (int i = 0; i < outSamples; ++i)
                            {
                                const double exactFP = (double) i * readRatio;
                                const int    ip      = (int) exactFP;
                                const float  frac    = (float) (exactFP - ip);

                                if (ip + 1 >= pulled) break;

                                for (int ch = 0; ch < numOut; ++ch)
                                {
                                    const int   srcCh = ch % pvCh;
                                    const float s0    = player.pvOutBuf.getSample (srcCh, ip);
                                    const float s1    = player.pvOutBuf.getSample (srcCh, ip + 1);
                                    const float v     = (s0 + frac * (s1 - s0)) * gain;
                                    mAudioClipScratch.addSample (ch, bufOffset + i, v);
                                    peak = juce::jmax (peak, std::abs (v));
                                }
                            }
                        }
                    }
                    // If gotRaw was false (buffer not ready / seek in progress),
                    // expectedFilePos is NOT advanced so we retry the same position next block.
                }
                else
                {
                    // ── Direct path: SR-only interpolation (no BPM stretch) ────────
                    peak = player.streamer->readAndMix (
                        mAudioClipScratch, bufOffset, outSamples, filePos, readRatio, numOut, gain);
                    player.expectedFilePos = filePos + (int64) std::ceil (outSamples * readRatio);
                }

                // F3 (2026-04-24): clip-edge declick.  5 ms linear fade-in
                // from absolute clip-start and fade-out into clip-end, capped
                // at half the clip length for very short clips.  Applied on
                // the source side (pre-rack) so the declick rides through the
                // per-clip effects chain naturally.  Block-boundary aware:
                // absPos here is relative to clip start, so a clip playing
                // from block 3 onward gets full gain while a clip starting
                // mid-block gets its first few samples ramped.
                {
                    const int64 clipLenOutSamples = effectiveClipEnd - clipStart;
                    const int fadeSamples = juce::jmax (1, juce::jmin (
                        (int) std::round (mSampleRate * 0.005),   // 5 ms
                        (int) (clipLenOutSamples / 2)));           // or half clip
                    for (int s = 0; s < outSamples; ++s)
                    {
                        const int64 absPos = outPosInClip + s;
                        float g = 1.0f;
                        if (absPos < (int64) fadeSamples)
                            g = (float) absPos / (float) fadeSamples;
                        const int64 distFromEnd = clipLenOutSamples - 1 - absPos;
                        if (distFromEnd >= 0 && distFromEnd < (int64) fadeSamples)
                            g = juce::jmin (g, (float) distFromEnd / (float) fadeSamples);
                        if (g < 1.0f)
                            for (int ch = 0; ch < numOut; ++ch)
                                mAudioClipScratch.setSample (ch, bufOffset + s,
                                    mAudioClipScratch.getSample (ch, bufOffset + s) * g);
                    }
                }

                // 5F-4a Batch 6: route per-clip scratch through Audio InsertNode
                // (polarity → width → rack → post-rack EQ → fader × mute × solo → PDC → peak).
                if (inRange)
                {
                    mVibeGraph.processInsert(VibeGraph::InsertKind::Audio, row,
                                              mAudioClipScratch, bpmAC, anySolo);
                    // 2026-04-30: copy mono + stereo peakDb to per-row atomics
                    // for the split DBFSMeter.  Mono kept for legacy readers.
                    mAudioRowPeakDb[row].store(
                        mVibeGraph.getInsertPeakDb(VibeGraph::InsertKind::Audio, row),
                        std::memory_order_relaxed);
                    const auto [pkL, pkR] = mVibeGraph.getInsertPeakDbStereo(
                        VibeGraph::InsertKind::Audio, row);
                    mAudioRowPeakDbL[row].store(pkL, std::memory_order_relaxed);
                    mAudioRowPeakDbR[row].store(pkR, std::memory_order_relaxed);
                    // 5F-4b B1b: route this audio insert's output via graph (main + sends)
                    routeInsertOutput(MixerChannelIds::audioInsert(row),
                                       mAudioClipScratch, numSamples);
                }
            }

            // 2026-04-28 (G-3): the Clips Bus pre-processing block was moved
            // OUT of the song-mode + tryLk gate to the post-block region
            // below.  Reason: in pattern mode, Clip-engine output reaches the
            // ClipsBus accumulator (via routeInsertOutput) but used to die
            // there because the bus rack/EQ/fader/master-handoff only ran in
            // song mode.  Now it runs unconditionally so engine-triggered
            // audio reaches master regardless of transport mode.
        }
    }

    // ── G-3 (2026-04-28): Clips Bus pre-processing — runs in BOTH song and
    //    pattern modes so Clip engines triggered via piano roll always reach
    //    the master rack.  In song mode the accumulator also contains
    //    arrangement-playback audio summed in by the audio_clip_players loop
    //    above; both contributions mix here.
    {
        // 5F-4b B1b: the Clips Bus accumulator holds the sum of every audio
        // insert whose _sendTo = kClipsBus (default) plus any sends targeting
        // kClipsBus.
        auto* clipsAccum = mVibeGraph.getChannelAccumulator(MixerChannelIds::kClipsBus);
        if (clipsAccum != nullptr)
        {
            juce::AudioBuffer<float>& clipsBus = *clipsAccum;

            // §P4.3: Audio Clips Bus pre-rack EQ.
            if (auto* preEq = mVibeGraph.getAudioClipsBusPreEQ();
                preEq != nullptr && clipsBus.getNumChannels() >= 2)
                preEq->process(clipsBus);

            // Audio Clips Bus rack — strip-local FX Bypass OR global kill-all.
            if (auto* rack = mVibeGraph.getAudioClipsBusRack())
            {
                const bool globalBypass =
                    (apvts.getRawParameterValue("master_fx_bypass") != nullptr)
                    && (apvts.getRawParameterValue("master_fx_bypass")->load() > 0.5f);
                // 2026-04-29: also honor mixer_clipsbus_bypass strip button.
                const auto* bypP = apvts.getRawParameterValue("mixer_clipsbus_bypass");
                const bool stripBypass = bypP && bypP->load() > 0.5f;
                const bool bypass = stripBypass || globalBypass;
                if (rack->isRackBypassed() != bypass)
                    rack->setRackBypassed(bypass);
                rack->process(clipsBus);
            }

            // Audio Clips Bus post-rack EQ.
            if (auto* eq = mVibeGraph.getAudioClipsBusEQ();
                eq != nullptr && clipsBus.getNumChannels() >= 2)
                eq->process(clipsBus);

            // 5F-4a Batch 6: Audio Clips Bus polarity + M/S width.
            mVibeGraph.applyAudioClipsBusPolarityWidth(clipsBus);

            // Bus fader + mute + in-group solo.
            // 2026-04-30: solo wiring added — was previously a dead button
            // (mixer_clipsbus_solo registered + UI-bound but no audio code
            // read it).  ClipsBus shares the in-group solo set with the
            // Vox/Inst receive buses; if any sibling is soloed and this
            // one isn't, gain goes to zero.
            float clipsBusGain = 1.0f;
            const auto* clipsSoloP = apvts.getRawParameterValue ("mixer_clipsbus_solo");
            const auto* clipsAnySolo = (apvts.getRawParameterValue ("mixer_voxbus_solo")
                                        || apvts.getRawParameterValue ("mixer_instbus_solo"))
                                       ? apvts.getRawParameterValue ("mixer_clipsbus_solo")  // (handle re-derive below)
                                       : nullptr;
            (void) clipsAnySolo;   // suppress unused — we use the precomputed group flag
            // anySolo was computed by the bus loop above as `busAnySolo`; but
            // ClipsBus pre-processing runs BEFORE that loop in code order,
            // so re-derive locally.  (Cheap — five atomic loads per block.)
            const bool localAnySolo =
                   (apvts.getRawParameterValue ("mixer_clipsbus_solo") && apvts.getRawParameterValue ("mixer_clipsbus_solo")->load() > 0.5f)
                || (apvts.getRawParameterValue ("mixer_voxbus_solo")   && apvts.getRawParameterValue ("mixer_voxbus_solo")  ->load() > 0.5f)
                || (apvts.getRawParameterValue ("mixer_instbus_solo")  && apvts.getRawParameterValue ("mixer_instbus_solo") ->load() > 0.5f)
                || (apvts.getRawParameterValue ("mixer_voxbus2_solo")  && apvts.getRawParameterValue ("mixer_voxbus2_solo") ->load() > 0.5f)
                || (apvts.getRawParameterValue ("mixer_instbus2_solo") && apvts.getRawParameterValue ("mixer_instbus2_solo")->load() > 0.5f)
                || (apvts.getRawParameterValue ("mixer_instbus3_solo") && apvts.getRawParameterValue ("mixer_instbus3_solo")->load() > 0.5f);
            const bool clipsSoloed = clipsSoloP && clipsSoloP->load() > 0.5f;
            const bool clipsSilencedBySolo = localAnySolo && ! clipsSoloed;
            // 2026-04-30 (audit B.3): direct APVTS reads for level + mute.
            // Was: mx.audioClipsBusMute/Level (PatternManager) → 30 Hz UI
            // applicator timer ferried APVTS automation into MixerState before
            // audio could see it.  Now audio reads APVTS each block; the
            // MixerState fields are still kept in sync via the strip's
            // onFaderChanged / onMuteChanged callbacks for legacy code paths
            // that still consume them, but they are no longer the source of
            // truth for audio output gain.
            const auto* clipsMuteP = apvts.getRawParameterValue ("mixer_clipsbus_mute");
            const auto* clipsLvlP  = apvts.getRawParameterValue ("mixer_clipsbus_level");
            const bool  clipsMuted = clipsMuteP && clipsMuteP->load() > 0.5f;
            const float clipsFadDb = clipsLvlP  ? clipsLvlP->load()  : 0.0f;
            const float clipsFadLn = juce::Decibels::decibelsToGain (clipsFadDb, -60.0f);
            clipsBusGain = (clipsMuted || clipsSilencedBySolo) ? 0.0f : clipsFadLn;
            if (! juce::approximatelyEqual (clipsBusGain, 1.0f))
                clipsBus.applyGain(clipsBusGain);

            // 2026-04-29: ClipsBus pan applied AFTER fader using project-level law.
            if (const auto* panP = apvts.getRawParameterValue("mixer_clipsbus_pan");
                panP != nullptr && clipsBus.getNumChannels() >= 2)
            {
                const float pan = panP->load();
                if (std::abs (pan) > 1.0e-4f)
                {
                    const int law = (apvts.getRawParameterValue("master_pan_law") != nullptr)
                        ? (int) apvts.getRawParameterValue("master_pan_law")->load() : 0;
                    const float p = juce::jlimit (-1.f, 1.f, pan);
                    const float np = (p + 1.f) * 0.5f;
                    float gL = 1.f, gR = 1.f;
                    switch (law)
                    {
                        case 1: gL = 1.f - np; gR = np; break;
                        case 2: gL = (p <= 0.f ? 1.f : 1.f - p);
                                gR = (p >= 0.f ? 1.f : 1.f + p); break;
                        default: { const float a = np * juce::MathConstants<float>::halfPi;
                                   gL = std::cos (a); gR = std::sin (a); } break;
                    }
                    clipsBus.applyGain (0, 0, numSamples, gL);
                    clipsBus.applyGain (1, 0, numSamples, gR);
                }
            }

            // Bus peak meter (hold + decay).
            // 2026-04-30: stereo L/R peaks for the new split DBFSMeter.
            // Mono atomic kept (= max(L, R)) for legacy readers.
            {
                constexpr float kDecayDbPerSec = 30.0f;
                const float decayPerBlock = kDecayDbPerSec * (float) numSamples
                                            / (float) juce::jmax(1.0, getSampleRate());
                const int nc = clipsBus.getNumChannels();
                const float pkLin_L = clipsBus.getMagnitude(0, 0, numSamples);
                const float pkLin_R = (nc >= 2) ? clipsBus.getMagnitude(1, 0, numSamples)
                                                : pkLin_L;
                const float thisL = juce::Decibels::gainToDecibels(pkLin_L, -60.0f);
                const float thisR = juce::Decibels::gainToDecibels(pkLin_R, -60.0f);
                const float prevL = mAudioClipsBusPeakDbL.load(std::memory_order_relaxed);
                const float prevR = mAudioClipsBusPeakDbR.load(std::memory_order_relaxed);
                const float decL  = juce::jmax(-60.0f, prevL - decayPerBlock);
                const float decR  = juce::jmax(-60.0f, prevR - decayPerBlock);
                const float newL  = juce::jmax(thisL, decL);
                const float newR  = juce::jmax(thisR, decR);
                mAudioClipsBusPeakDbL.store(newL,                   std::memory_order_relaxed);
                mAudioClipsBusPeakDbR.store(newR,                   std::memory_order_relaxed);
                mAudioClipsBusPeakDb .store(juce::jmax(newL, newR), std::memory_order_relaxed);
            }

            // Hand the bus buffer to VibeGraph for the master rack.
            audioClipsBusForGraph = &clipsBus;
        }
    }

    // 5F-4b B2: process aux strips. Their input accumulators were populated by
    // routeInsertOutput calls from upstream source inserts. Each aux's output
    // is fanned out via the graph to its destinations (default = kMaster).
    // Note: aux → Clips-Bus has 1-block latency because Clips-Bus pre-processes
    // above this point. Aux → Layer/Bass/Drums/Master is 0-latency.
    mVibeGraph.processAuxInserts(bpmForInserts, anySolo,
        [&](int auxChId, juce::AudioBuffer<float>& buf)
        {
            routeInsertOutput(auxChId, buf, numSamples);
        });

    // R3 (2026-04-23): process Vox + Inst live-input strips.  For each
    // armed strip with a valid `_inputChannelIdx`, copy that one input
    // channel from the device snapshot into the per-slot scratch (mono ->
    // stereo via duplication), run the InsertNode (preEQ -> rack -> postEQ
    // -> fader / mute / solo / meter), and route to the strip's bus
    // accumulator.  After all strips processed, fan the Vox / Inst BUS
    // accumulators out to their destinations (default = Master).
    if (numInputs > 0)
    {
        if (mLiveInputSlotBuf.getNumSamples() < numSamples
            || mLiveInputSlotBuf.getNumChannels() < 2)
            mLiveInputSlotBuf.setSize (2, numSamples, false, false, true);

        struct LiveSet { VibeGraph::InsertKind kind; int max; const char* prefixBase; int chBase; };
        static const LiveSet kLive[] = {
            { VibeGraph::InsertKind::Vox,  MixerChannelIds::kMaxVoxStrips,  "mixer_vox_",  MixerChannelIds::kVoxBase  },
            { VibeGraph::InsertKind::Inst, MixerChannelIds::kMaxInstStrips, "mixer_inst_", MixerChannelIds::kInstBase },
        };
        for (const auto& set : kLive)
        {
            for (int i = 0; i < set.max; ++i)
            {
                if (mVibeGraph.getInsertNode (set.kind, i) == nullptr) continue;

                const juce::String prefix = juce::String (set.prefixBase) + juce::String (i);
                const auto* armP = apvts.getRawParameterValue (prefix + "_arm");
                const auto* idxP = apvts.getRawParameterValue (prefix + "_inputChannelIdx");
                if (armP == nullptr || idxP == nullptr) continue;
                if (armP->load() < 0.5f) continue;
                const int chIdx = (int) idxP->load();
                if (chIdx < 0 || chIdx >= numInputs) continue;

                // Mono -> stereo duplication so the InsertNode's stereo chain
                // (M/S width, EQ, etc.) processes a balanced signal.
                mLiveInputSlotBuf.copyFrom (0, 0, mLiveInputSnapshot, chIdx, 0, numSamples);
                mLiveInputSlotBuf.copyFrom (1, 0, mLiveInputSnapshot, chIdx, 0, numSamples);

                // R5d: if a per-strip recorder is armed for this channel id,
                // capture the RAW pre-rack input (mono) so the user gets a
                // clean source file.  Rack / EQ effects can be re-applied to
                // the WAV after import if desired.
                const int chId = set.chBase + i;
                for (auto& sr : mStripRecorders)
                {
                    if (sr.channelId == chId && sr.recorder && sr.recorder->isRecording())
                    {
                        juce::AudioBuffer<float> monoView (
                            mLiveInputSnapshot.getArrayOfWritePointers() + chIdx,
                            1, numSamples);
                        sr.recorder->writeBlock (monoView);
                        break;
                    }
                }

                mVibeGraph.processInsert (set.kind, i, mLiveInputSlotBuf,
                                            bpmForInserts, anySolo);
                // R4: only route downstream when Listen is on.  The InsertNode
                // already updated its peak meter so the UI still sees signal
                // even with monitoring muted.
                const auto* listenP = apvts.getRawParameterValue (prefix + "_listen");
                if (listenP != nullptr && listenP->load() > 0.5f)
                    routeInsertOutput (set.chBase + i, mLiveInputSlotBuf, numSamples);
            }
        }

        // R3.5: process Vox + Inst BUS accumulators through their full DSP
        // chain (preEQ -> rack -> postEQ -> polarity/width -> fader/mute) and
        // measure peak before fanning out to Master.  Mirrors the Audio Clips
        // Bus path above.
        // 2026-04-29: BusSet now carries a prefix string so per-block APVTS
        // lookups for _bypass / _pan don't need to be hard-coded per-bus.
        // Same APVTS ID convention the rest of the file uses.
        // 2026-04-30: stereo peak atomics added alongside mono for the new
        // split DBFSMeter.  All three are written each block.
        struct BusSet {
            int                                                      chId;
            EQ8MsDSP*                                                preEq;
            EffectRack*                                              rack;
            EQ8MsDSP*                                                postEq;
            void (VibeGraph::*polWidth)(juce::AudioBuffer<float>&);
            std::atomic<float>*                                      peak;     // mono = max(L,R)
            std::atomic<float>*                                      peakL;
            std::atomic<float>*                                      peakR;
            const char*                                              prefix;   // e.g. "mixer_voxbus"
        };
        const BusSet kBusSets[] = {
            { MixerChannelIds::kVoxBus,
              mVibeGraph.getVoxBusPreEQ(),  mVibeGraph.getVoxBusRack(),  mVibeGraph.getVoxBusEQ(),
              &VibeGraph::applyVoxBusPolarityWidth,
              &mVoxBusPeakDb,  &mVoxBusPeakDbL,  &mVoxBusPeakDbR,  "mixer_voxbus"  },
            { MixerChannelIds::kInstBus,
              mVibeGraph.getInstBusPreEQ(), mVibeGraph.getInstBusRack(), mVibeGraph.getInstBusEQ(),
              &VibeGraph::applyInstBusPolarityWidth,
              &mInstBusPeakDb, &mInstBusPeakDbL, &mInstBusPeakDbR, "mixer_instbus" },
            // G-6 (2026-04-29): secondary buses.  Always processed (cheap when
            // no inserts route to them — buffer is silent).  UI activation
            // (Mixer "Add Vox/Inst Bus" button) is independent of audio path.
            { MixerChannelIds::kVoxBus2,
              mVibeGraph.getVoxBus2PreEQ(),  mVibeGraph.getVoxBus2Rack(),  mVibeGraph.getVoxBus2EQ(),
              &VibeGraph::applyVoxBus2PolarityWidth,
              &mVoxBus2PeakDb,  &mVoxBus2PeakDbL,  &mVoxBus2PeakDbR,  "mixer_voxbus2"  },
            { MixerChannelIds::kInstBus2,
              mVibeGraph.getInstBus2PreEQ(), mVibeGraph.getInstBus2Rack(), mVibeGraph.getInstBus2EQ(),
              &VibeGraph::applyInstBus2PolarityWidth,
              &mInstBus2PeakDb, &mInstBus2PeakDbL, &mInstBus2PeakDbR, "mixer_instbus2" },
            { MixerChannelIds::kInstBus3,
              mVibeGraph.getInstBus3PreEQ(), mVibeGraph.getInstBus3Rack(), mVibeGraph.getInstBus3EQ(),
              &VibeGraph::applyInstBus3PolarityWidth,
              &mInstBus3PeakDb, &mInstBus3PeakDbL, &mInstBus3PeakDbR, "mixer_instbus3" },
        };
        const bool globalBypass =
            (apvts.getRawParameterValue("master_fx_bypass") != nullptr)
            && (apvts.getRawParameterValue("master_fx_bypass")->load() > 0.5f);
        // 2026-04-29: project-level pan law selector — each bus reads it once
        // per block when applying its _pan param.
        const int panLaw =
            (apvts.getRawParameterValue("master_pan_law") != nullptr)
                ? (int) apvts.getRawParameterValue("master_pan_law")->load()
                : 0;

        // 2026-04-30: cross-bus solo flag for the "post-rack receive" group
        // (Audio Clips, Vox, Inst, Vox2, Inst2, Inst3).  Was missing — _solo
        // params were registered + UI-bound but the bus loop only read level
        // + mute.  When ANY bus in this group is soloed, all non-soloed buses
        // in the group go silent (matches Layers/Bass/Drums in-group solo).
        auto soloOf = [&] (const char* prefix) -> bool
        {
            const auto* p = apvts.getRawParameterValue (juce::String (prefix) + "_solo");
            return p && p->load() > 0.5f;
        };
        const bool busAnySolo =
               soloOf ("mixer_clipsbus")
            || soloOf ("mixer_voxbus")  || soloOf ("mixer_instbus")
            || soloOf ("mixer_voxbus2") || soloOf ("mixer_instbus2")
            || soloOf ("mixer_instbus3");

        for (const auto& bs : kBusSets)
        {
            auto* accum = mVibeGraph.getChannelAccumulator (bs.chId);
            if (accum == nullptr) continue;
            auto& buf = *accum;
            if (buf.getNumChannels() < 2) continue;

            const juce::String prefix = bs.prefix;

            if (bs.preEq) bs.preEq->process(buf);
            if (bs.rack)
            {
                // 2026-04-29: strip-local FX Bypass OR global kill-all.
                const auto* bypP = apvts.getRawParameterValue (prefix + "_bypass");
                const bool stripBypass = bypP && bypP->load() > 0.5f;
                const bool bypass = stripBypass || globalBypass;
                if (bs.rack->isRackBypassed() != bypass)
                    bs.rack->setRackBypassed(bypass);
                bs.rack->process(buf);
            }
            if (bs.postEq) bs.postEq->process(buf);
            (mVibeGraph.*bs.polWidth)(buf);

            // Fader (dB -> linear) * mute * (in-group solo).
            // 2026-04-30: solo gating added — this bus is silenced if any
            // sibling bus in the receive group is soloed AND this one isn't.
            const auto* lvlP   = apvts.getRawParameterValue (prefix + "_level");
            const auto* muteP  = apvts.getRawParameterValue (prefix + "_mute");
            const auto* soloP  = apvts.getRawParameterValue (prefix + "_solo");
            const float dB     = lvlP  ? lvlP->load() : 0.0f;
            const bool  muted  = muteP && muteP->load() > 0.5f;
            const bool  soloed = soloP && soloP->load() > 0.5f;
            const bool  silenced = muted || (busAnySolo && ! soloed);
            const float gain   = silenced ? 0.0f : juce::Decibels::decibelsToGain (dB, -60.0f);
            if (gain != 1.0f) buf.applyGain (gain);

            // 2026-04-29: pan applied AFTER fader using project-level law.
            if (const auto* panP = apvts.getRawParameterValue (prefix + "_pan"))
            {
                const float pan = panP->load();
                if (std::abs (pan) > 1.0e-4f)
                {
                    float gL = 1.f, gR = 1.f;
                    // Inline pan law (matches VibeGraph::applyPanLaw).  Done
                    // here rather than calling into VibeGraph because that
                    // file's helper isn't exposed in the header.
                    const float p = juce::jlimit (-1.f, 1.f, pan);
                    const float np = (p + 1.f) * 0.5f;
                    switch (panLaw)
                    {
                        case 1: gL = 1.f - np;          gR = np;            break;
                        case 2: gL = (p <= 0.f ? 1.f : 1.f - p);
                                gR = (p >= 0.f ? 1.f : 1.f + p);            break;
                        default: { const float a = np * juce::MathConstants<float>::halfPi;
                                   gL = std::cos (a); gR = std::sin (a); }  break;
                    }
                    buf.applyGain (0, 0, numSamples, gL);
                    buf.applyGain (1, 0, numSamples, gR);
                }
            }

            // Hold + decay peak meter (matches InsertNode pattern).
            // 2026-04-30: stereo L/R for split DBFSMeter (mono = max(L,R)).
            {
                constexpr float kDecayDbPerSec = 30.0f;
                const float decayPerBlock = kDecayDbPerSec * (float) numSamples
                                            / (float) juce::jmax (1.0, getSampleRate());
                const int   nc      = buf.getNumChannels();
                const float pkLin_L = buf.getMagnitude (0, 0, numSamples);
                const float pkLin_R = (nc >= 2) ? buf.getMagnitude (1, 0, numSamples)
                                                : pkLin_L;
                const float thisL = juce::Decibels::gainToDecibels (pkLin_L, -60.0f);
                const float thisR = juce::Decibels::gainToDecibels (pkLin_R, -60.0f);
                const float prevL = bs.peakL->load (std::memory_order_relaxed);
                const float prevR = bs.peakR->load (std::memory_order_relaxed);
                const float decL  = juce::jmax (-60.0f, prevL - decayPerBlock);
                const float decR  = juce::jmax (-60.0f, prevR - decayPerBlock);
                const float newL  = juce::jmax (thisL, decL);
                const float newR  = juce::jmax (thisR, decR);
                bs.peakL->store (newL,                   std::memory_order_relaxed);
                bs.peakR->store (newR,                   std::memory_order_relaxed);
                bs.peak ->store (juce::jmax (newL, newR),std::memory_order_relaxed);
            }

            routeInsertOutput (bs.chId, buf, numSamples);
        }
    }

    // 5F-4b B1b: feed the Layer/Bass/Drums bus accumulators (populated above
    // via routeInsertOutput) into VibeGraph as preRendered inputs. Direct-to-
    // Master routing is picked up inside VibeGraph::processBlock via the kMaster
    // accumulator. The legacy mLayerEngineSum/mBassEngineBuf/mDrumsEngineBuf
    // kind-sum buffers are no longer authoritative — kept compiled for back-compat
    // but no longer consumed.
    auto* layersIn = mVibeGraph.getChannelAccumulator(MixerChannelIds::kLayersBus);
    auto* bassIn   = mVibeGraph.getChannelAccumulator(MixerChannelIds::kBassBus);
    auto* drumsIn  = mVibeGraph.getChannelAccumulator(MixerChannelIds::kDrumsBus);

    // ── Graph processes all rendering, EQ, effects, mixing, and gain ──────
    // audioClipsBusForGraph is non-null when clips are active; VibeGraph sums
    // it before the master rack so clips go through the full master chain.
    mVibeGraph.processBlock(buffer, allMidi, bpm,
                            layersIn, bassIn, drumsIn,
                            audioClipsBusForGraph);

    // ── MIDI recording: capture note events sent to the graph this block ─
    if (mMidiRecorder.isRecording())
    {
        double bps = bpm / (60.0 * mSampleRate);
        double beatStart = pos.getPpqPosition().orFallback(0.0);
        mMidiRecorder.processBlock(allMidi, beatStart, bps);
    }

    // 2026-04-26 (D-5 fix): write the master-output recorder BEFORE the
    // metronome adds its click samples to the buffer — otherwise the
    // recorded WAV contains the metronome click on every recorded beat.
    // The post-metronome write that used to live below the metro block has
    // been removed.
    if (mMasterRecorder.isRecording())
        mMasterRecorder.writeBlock (buffer);

    // ── Metronome click DSP ───────────────────────────────────────────────────
    // Count-in always runs (independent of metro button); transport metro requires enabled.
    {
        const float  metroVol  = mMetro.volume.load(std::memory_order_relaxed);
        const int    sndType   = mMetro.soundType.load(std::memory_order_relaxed);
        const float  twoPi     = juce::MathConstants<float>::twoPi;

        // Helper: fire one click burst of appropriate duration for sound type
        auto triggerClick = [&](bool accent)
        {
            int dur;
            switch (sndType) {
                case MetroDSP::Click: dur = juce::jmax(1, (int)(mSampleRate * 0.005)); break;
                case MetroDSP::Wood:  dur = juce::jmax(1, (int)(mSampleRate * 0.012)); break;
                case MetroDSP::Bell:  dur = juce::jmax(1, (int)(mSampleRate * 0.040)); break;
                default:              dur = juce::jmax(1, (int)(mSampleRate * 0.018)); break; // Sine
            }
            mMetro.clickSampLeft  = dur;
            mMetro.clickPhase     = 0.f;
            mMetro.clickIsAccent  = accent;
        };

        // Helper: synthesise one sample of the current click burst
        auto synthClick = [&]() -> float
        {
            if (mMetro.clickSampLeft <= 0) return 0.f;
            const int dur = [&]{
                switch (sndType) {
                    case MetroDSP::Click: return juce::jmax(1, (int)(mSampleRate * 0.005));
                    case MetroDSP::Wood:  return juce::jmax(1, (int)(mSampleRate * 0.012));
                    case MetroDSP::Bell:  return juce::jmax(1, (int)(mSampleRate * 0.040));
                    default:              return juce::jmax(1, (int)(mSampleRate * 0.018));
                }
            }();
            float t   = (float)mMetro.clickSampLeft / (float)dur; // 1→0
            float env = t * t;  // squared decay

            float sample = 0.f;
            if (sndType == MetroDSP::Click) {
                // Short noise burst
                sample = metroVol * 0.6f * env * env
                         * (juce::Random::getSystemRandom().nextFloat() * 2.f - 1.f);
            } else if (sndType == MetroDSP::Wood) {
                float freq = mMetro.clickIsAccent ? 450.f : 280.f;
                mMetro.clickPhase += twoPi * freq / (float)mSampleRate;
                if (mMetro.clickPhase > twoPi) mMetro.clickPhase -= twoPi;
                sample = metroVol * 0.6f * env * env * std::sin(mMetro.clickPhase);
            } else if (sndType == MetroDSP::Bell) {
                float freq = mMetro.clickIsAccent ? 1100.f : 660.f;
                mMetro.clickPhase += twoPi * freq / (float)mSampleRate;
                if (mMetro.clickPhase > twoPi) mMetro.clickPhase -= twoPi;
                sample = metroVol * 0.45f * t * std::sin(mMetro.clickPhase); // linear decay
            } else { // Sine
                float freq = mMetro.clickIsAccent ? 880.f : 440.f;
                mMetro.clickPhase += twoPi * freq / (float)mSampleRate;
                if (mMetro.clickPhase > twoPi) mMetro.clickPhase -= twoPi;
                sample = metroVol * 0.5f * env * std::sin(mMetro.clickPhase);
            }
            --mMetro.clickSampLeft;
            return sample;
        };

        // ── Count-in: runs independently of transport ────────────────────────
        // 2026-04-26 (D-5 fix): the rising edge now fires beat 1 IMMEDIATELY
        // (was silently waiting until phase crossed 0→1, which delayed the
        // first audible click by a full beat — user heard 3 clicks instead
        // of 4).  Loop continues to fire on each subsequent integer crossing
        // (beats 2, 3, 4, …).  countInBeatsFired tracks accent placement.
        bool ciActive = mMetro.countInActive.load(std::memory_order_relaxed);
        if (!mMetro.countInWasActive && ciActive) {
            mMetro.countInPhase      = 0.0;
            mMetro.lastBeatFloor     = -99999.0;
            mMetro.countInBeatsFired = 1;
            triggerClick(true);   // Beat 1 (always accented) — fires at sample 0.
        }
        mMetro.countInWasActive = ciActive;

        if (ciActive)
        {
            const double bpm = mMetro.countInBpm.load(std::memory_order_relaxed);
            const double bps = juce::jmax(1e-6, bpm / (60.0 * mSampleRate));
            for (int s = 0; s < numSamples; ++s)
            {
                double prevPhase = mMetro.countInPhase;
                mMetro.countInPhase += bps;
                if ((long long)mMetro.countInPhase > (long long)prevPhase)
                {
                    // Crossing into integer N means beat (N+1).  countInBeatsFired
                    // tracks how many beats have fired so far.
                    ++mMetro.countInBeatsFired;
                    triggerClick((mMetro.countInBeatsFired - 1) % 4 == 0);   // accent every 4
                }
                float s0 = synthClick();
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.addSample(ch, s, s0);
            }
        }
        else
        {
            mMetro.countInBeatsFired = 0;
        }

        // ── Transport-locked metro (runs while playing, count-in inactive, metro enabled) ───
        if (!ciActive && mMetro.enabled.load(std::memory_order_relaxed))
        {
            if (auto pi = getPlayHead() ? getPlayHead()->getPosition()
                                        : juce::Optional<juce::AudioPlayHead::PositionInfo>{})
            {
                if (pi->getIsPlaying())
                {
                    const double bpmV = pi->getBpm().orFallback(120.0);
                    const double bps  = juce::jmax(1e-6, bpmV / (60.0 * mSampleRate));
                    const double bs0  = pi->getPpqPosition().orFallback(0.0);

                    for (int s = 0; s < numSamples; ++s)
                    {
                        double sampleBeat = bs0 + s * bps;
                        double beatFloor  = std::floor(sampleBeat);
                        if (beatFloor < mMetro.lastBeatFloor - 1.0)
                            mMetro.lastBeatFloor = beatFloor - 1.0;
                        if (beatFloor > mMetro.lastBeatFloor)
                        {
                            mMetro.lastBeatFloor = beatFloor;
                            long long bf = (long long)std::round(beatFloor);
                            triggerClick((((bf % 4) + 4) % 4 == 0));
                        }
                        float s0 = synthClick();
                        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                            buffer.addSample(ch, s, s0);
                    }
                }
            }
        }
    }

    // ── Copy peak levels from graph to processor atomics (for Mixer UI) ──
    // 2026-04-30: stereo L/R copies added alongside mono for the new split
    // DBFSMeter.  Mono atomics (max(L,R)) kept for legacy readers.
    mLayersPeakDb .store(mVibeGraph.layersPeakDb .load(), std::memory_order_relaxed);
    mLayersPeakDbL.store(mVibeGraph.layersPeakDbL.load(), std::memory_order_relaxed);
    mLayersPeakDbR.store(mVibeGraph.layersPeakDbR.load(), std::memory_order_relaxed);
    mBassPeakDb   .store(mVibeGraph.bassPeakDb   .load(), std::memory_order_relaxed);
    mBassPeakDbL  .store(mVibeGraph.bassPeakDbL  .load(), std::memory_order_relaxed);
    mBassPeakDbR  .store(mVibeGraph.bassPeakDbR  .load(), std::memory_order_relaxed);
    mDrumsPeakDb  .store(mVibeGraph.drumsPeakDb  .load(), std::memory_order_relaxed);
    mDrumsPeakDbL .store(mVibeGraph.drumsPeakDbL .load(), std::memory_order_relaxed);
    mDrumsPeakDbR .store(mVibeGraph.drumsPeakDbR .load(), std::memory_order_relaxed);
    mMasterPeakDb .store(mVibeGraph.masterPeakDb .load(), std::memory_order_relaxed);
    mMasterPeakDbL.store(mVibeGraph.masterPeakDbL.load(), std::memory_order_relaxed);
    mMasterPeakDbR.store(mVibeGraph.masterPeakDbR.load(), std::memory_order_relaxed);

    // 2026-04-30: peak-meter decay for transport-stopped state.
    // The per-clip path (which writes mAudioRowPeakDb*) is gated by
    // pos.getIsPlaying(), and ditto for the song-mode-only ClipsBus
    // accumulator.  When transport stops they STOP firing — and without
    // fresh writes the atomics freeze at the last value, which makes the
    // strip meter wiggle ±1 segment around the frozen level instead of
    // decaying to -60.  Apply a uniform 30 dB/sec decay every block.
    // When clips ARE playing, the per-clip path writes fresh peaks BEFORE
    // this point, so the decay only affects rows that didn't get a write.
    {
        constexpr float kDecayDbPerSec = 30.0f;
        const float decayPerBlock = kDecayDbPerSec * (float) numSamples
                                    / (float) juce::jmax(1.0, mSampleRate);
        auto decay = [decayPerBlock](std::atomic<float>& a) noexcept
        {
            const float p = a.load(std::memory_order_relaxed);
            a.store(juce::jmax(-60.0f, p - decayPerBlock), std::memory_order_relaxed);
        };
        for (int r = 0; r < kMaxAudioRows; ++r)
        {
            decay(mAudioRowPeakDb [r]);
            decay(mAudioRowPeakDbL[r]);
            decay(mAudioRowPeakDbR[r]);
        }
    }

    // F4 reverted 2026-04-24: the master-bus Play/Stop fade silenced audition
    // when the transport wasn't running (audition produces audio without
    // pos.getIsPlaying() ever going true).  Master passes through at unity;
    // any Play/Stop click is small enough to live with, and engine voice
    // envelopes already handle most of it.  mMasterFadeGain member kept in
    // the header for now in case a smarter declick lands later (e.g. gated
    // by "is any voice active" instead of transport state).

    // 2026-04-26 (D-5 fix): mMasterRecorder.writeBlock moved up to before the
    // metronome block so the click stays out of the recorded WAV.  Used to
    // live here writing post-metronome buffer.

    // Clear incoming MIDI so we don't double-trigger on next block
    midiMessages.clear();

    // ── 1M: DSP load measurement + overload protection ────────────────────────
    {
        const auto   t1       = juce::Time::getHighResolutionTicks();
        const double elapsed  = (double)(t1 - t0)
                                / (double)juce::Time::getHighResolutionTicksPerSecond();
        const double bufDur   = numSamples / juce::jmax(1.0, mSampleRate);
        const float  rawLoad  = (bufDur > 0.0)
                                    ? juce::jlimit(0.f, 2.f, (float)(elapsed / bufDur))
                                    : 0.f;

        // Exponential smoothing — ~80 ms time constant at 512/44100 block rate
        const float prev     = mAudioDspLoad.load(std::memory_order_relaxed);
        const float smoothed = prev * 0.85f + rawLoad * 0.15f;
        mAudioDspLoad.store(smoothed,         std::memory_order_relaxed);
        mDspOverload95.store(smoothed > 0.95f, std::memory_order_relaxed);

        // Sustained 85% detection — accumulate samples while above threshold
        if (smoothed > 0.85f)
            mOverload85Samples += numSamples;
        else
            mOverload85Samples = 0;

        const bool over85 = (mOverload85Samples > (int64_t)(0.5 * mSampleRate));
        mDspOverload85.store(over85, std::memory_order_relaxed);

        if (over85)
        {
            // Steal all synth voices (tail-off = true → release envelopes play,
            // no hard click). DrumSynth + BassSynth one-shot voices decay naturally.
            mSynth.allNotesOff(1, true);
            // Back off the counter so we don't re-trigger every block —
            // next steal can only fire after another 250 ms of sustained overload.
            mOverload85Samples = (int64_t)(0.25 * mSampleRate);
        }
    }
}

// ── Parameter sync helpers ────────────────────────────────────────────────────
// §P4.3 B7 (2026-04-22): legacy updateDrumsEQ + updateLayerPageEQsFromApvts +
// updateBassPageEQsFromApvts deleted along with their DSP instances
// (mDrumsEQDSP / mLayerPageEQs / mBassPageEQs).  All pre-rack EQs now live on
// InsertNode / BusNode preEq members and are sync'd by the unified
// updateAllPreRackEQsFromApvts pass (which iterates every registered mixer
// strip prefix).


// ── Session B: universal EQ update helpers ────────────────────────────────────
// Generic APVTS-to-EQ syncer. Reads 9 params x 8 bands for both mid + side
// inner EQs and applies via the standard setBand* setters (all internally
// CPU-guarded so no-change calls are free). Works for any EQ8MsDSP whose
// params were lazily registered via addParamsForTrackEQ.
void VibeSynthProcessor::updateEQFromApvts(EQ8MsDSP* eq,
                                           const juce::String& midPrefix,
                                           const juce::String& sidePrefix)
{
    if (!eq) return;
    auto get = [this](const juce::String& id) -> float
    {
        if (auto* p = apvts.getRawParameterValue(id)) return p->load();
        return 0.f;
    };

    auto syncSide = [&](EQ8DSP& side, const juce::String& prefix)
    {
        for (int b = 0; b < 8; ++b)
        {
            juce::String bp = prefix + juce::String(b);
            // Param existence check via getRawParameterValue returning null for
            // unregistered params is handled by the get() lambda returning 0.f;
            // we skip the band entirely if Freq isn't registered (implies this
            // prefix has no params yet - new InsertNode not yet ensured etc).
            if (!apvts.getParameter(bp + "Freq")) continue;

            auto cur = side.getBand(b);
            float f  = get(bp + "Freq");
            if (f  != cur.freq)   side.setBandFreq (b, f);
            float gn = get(bp + "Gain");
            if (gn != cur.gainDb) side.setBandGain (b, gn);
            float q  = get(bp + "Q");
            if (q  != cur.q)      side.setBandQ    (b, q);
            int   t  = (int) get(bp + "Type");
            if (t  != cur.type)   side.setBandType (b, t);
            int   s  = (int) get(bp + "Slope");
            if (s  != cur.slope)  side.setBandSlope(b, s);
            side.setBandOn    (b, get(bp + "On")   > 0.5f);
            side.setBandMuted (b, get(bp + "Mute") > 0.5f);
            side.setBandSoloed(b, get(bp + "Solo") > 0.5f);
            int ch = juce::jlimit(0, 4, (int) get(bp + "Channel"));
            if (ch != cur.channel) side.setBandChannel(b, ch);
            // 12j Dynamic EQ params (read only when registered - Dynamic param's
            // absence short-circuits all subsequent reads cheaply).
            if (apvts.getParameter(bp + "Dynamic"))
            {
                side.setBandDynamic  (b, get(bp + "Dynamic") > 0.5f);
                float thr = get(bp + "Threshold"); if (thr != cur.threshold) side.setBandThreshold(b, thr);
                float rt  = get(bp + "Ratio");     if (rt  != cur.ratio)     side.setBandRatio    (b, rt);
                float at  = get(bp + "Attack");    if (at  != cur.attack)    side.setBandAttack   (b, at);
                float re  = get(bp + "Release");   if (re  != cur.release)   side.setBandRelease  (b, re);
                float rg  = get(bp + "Range");     if (rg  != cur.rangeDb)   side.setBandRange    (b, rg);
                side.setBandUpward(b, get(bp + "Upward") > 0.5f);
                int sc = (int) get(bp + "ScSource");
                if (sc != cur.scSourceId) side.setBandScSource(b, sc);
            }
        }
    };

    if (midPrefix.isNotEmpty())  syncSide(eq->mid(),  midPrefix);
    if (sidePrefix.isNotEmpty()) syncSide(eq->side(), sidePrefix);
}

// Iterate every post-rack EQ instance (6 buses + up to 94 inserts) and sync it
// from its APVTS prefix. Safe to call from processBlock every frame - getters
// return nullptr for indices that have no registered InsertNode, and the inner
// band loop short-circuits on unregistered prefixes.
void VibeSynthProcessor::updateAllPostRackEQsFromApvts()
{
    // Bus post-rack EQs (mixer_layers / mixer_bass / mixer_drums /
    // mixer_master / mixer_fx / mixer_clipsbus).
    struct BusPair { const char* prefix; EQ8MsDSP* (*getter)(VibeGraph&); };
    static const BusPair kBusEQs[] = {
        { "mixer_layers",   [](VibeGraph& g) { return g.getLayersBusEQ();    } },
        { "mixer_bass",     [](VibeGraph& g) { return g.getBassBusEQ();      } },
        { "mixer_drums",    [](VibeGraph& g) { return g.getDrumsBusEQ();     } },
        { "mixer_master",   [](VibeGraph& g) { return g.getMasterEQ();       } },
        { "mixer_fx",       [](VibeGraph& g) { return g.getEffectsBusEQ();   } },
        { "mixer_clipsbus", [](VibeGraph& g) { return g.getAudioClipsBusEQ();} },
        { "mixer_voxbus",   [](VibeGraph& g) { return g.getVoxBusEQ();       } },
        { "mixer_instbus",  [](VibeGraph& g) { return g.getInstBusEQ();      } },
        { "mixer_voxbus2",  [](VibeGraph& g) { return g.getVoxBus2EQ();      } },
        { "mixer_instbus2", [](VibeGraph& g) { return g.getInstBus2EQ();     } },
        { "mixer_instbus3", [](VibeGraph& g) { return g.getInstBus3EQ();     } },
    };
    for (const auto& bp : kBusEQs)
    {
        if (auto* eq = bp.getter(mVibeGraph))
            updateEQFromApvts(eq,
                              juce::String(bp.prefix) + "_mid_eq",
                              juce::String(bp.prefix) + "_side_eq");
    }

    // Insert post-rack EQs (Layer / Bass / Drum / Audio / Aux / Vox / Inst).
    struct InsertSet { VibeGraph::InsertKind kind; const char* prefixBase; int count; };
    static const InsertSet kInsertSets[] = {
        { VibeGraph::InsertKind::Layer, "mixer_layer_", 8  },
        { VibeGraph::InsertKind::Bass,  "mixer_bass_",  4  },
        { VibeGraph::InsertKind::Drum,  "mixer_drum_",  16 },
        { VibeGraph::InsertKind::Audio, "mixer_audio_", 50 },
        { VibeGraph::InsertKind::Aux,   "mixer_aux_",   MixerChannelIds::kMaxAuxStrips },
        { VibeGraph::InsertKind::Vox,   "mixer_vox_",   MixerChannelIds::kMaxVoxStrips  },
        { VibeGraph::InsertKind::Inst,  "mixer_inst_",  MixerChannelIds::kMaxInstStrips },
    };
    for (const auto& is : kInsertSets)
    {
        for (int i = 0; i < is.count; ++i)
        {
            if (auto* eq = mVibeGraph.getInsertEQ(is.kind, i))
            {
                const juce::String prefix = juce::String(is.prefixBase) + juce::String(i);
                updateEQFromApvts(eq, prefix + "_mid_eq", prefix + "_side_eq");
            }
        }
    }
}

// §P4.3: Pre-rack EQ sync — mirror of updateAllPostRackEQsFromApvts but uses
// the `_preeq_` sub-prefix to read pre-EQ params + the new pre-EQ accessors.
// Bus + insert pre-EQ DSPs were added in B2; their params in B3.  Called once
// per processBlock alongside the post-rack version.
void VibeSynthProcessor::updateAllPreRackEQsFromApvts()
{
    // Bus pre-rack EQs.
    struct BusPair { const char* prefix; EQ8MsDSP* (*getter)(VibeGraph&); };
    static const BusPair kBusPreEQs[] = {
        { "mixer_layers",   [](VibeGraph& g) { return g.getLayersBusPreEQ();    } },
        { "mixer_bass",     [](VibeGraph& g) { return g.getBassBusPreEQ();      } },
        { "mixer_drums",    [](VibeGraph& g) { return g.getDrumsBusPreEQ();     } },
        { "mixer_master",   [](VibeGraph& g) { return g.getMasterPreEQ();       } },
        { "mixer_fx",       [](VibeGraph& g) { return g.getEffectsBusPreEQ();   } },
        { "mixer_clipsbus", [](VibeGraph& g) { return g.getAudioClipsBusPreEQ();} },
        { "mixer_voxbus",   [](VibeGraph& g) { return g.getVoxBusPreEQ();       } },
        { "mixer_instbus",  [](VibeGraph& g) { return g.getInstBusPreEQ();      } },
        { "mixer_voxbus2",  [](VibeGraph& g) { return g.getVoxBus2PreEQ();      } },
        { "mixer_instbus2", [](VibeGraph& g) { return g.getInstBus2PreEQ();     } },
        { "mixer_instbus3", [](VibeGraph& g) { return g.getInstBus3PreEQ();     } },
    };
    for (const auto& bp : kBusPreEQs)
    {
        if (auto* eq = bp.getter(mVibeGraph))
            updateEQFromApvts(eq,
                              juce::String(bp.prefix) + "_preeq_mid_eq",
                              juce::String(bp.prefix) + "_preeq_side_eq");
    }

    // Insert pre-rack EQs (Layer / Bass / Drum / Audio / Aux / Vox / Inst).
    struct InsertSet { VibeGraph::InsertKind kind; const char* prefixBase; int count; };
    static const InsertSet kInsertSets[] = {
        { VibeGraph::InsertKind::Layer, "mixer_layer_", 8  },
        { VibeGraph::InsertKind::Bass,  "mixer_bass_",  4  },
        { VibeGraph::InsertKind::Drum,  "mixer_drum_",  16 },
        { VibeGraph::InsertKind::Audio, "mixer_audio_", 50 },
        { VibeGraph::InsertKind::Aux,   "mixer_aux_",   MixerChannelIds::kMaxAuxStrips },
        { VibeGraph::InsertKind::Vox,   "mixer_vox_",   MixerChannelIds::kMaxVoxStrips  },
        { VibeGraph::InsertKind::Inst,  "mixer_inst_",  MixerChannelIds::kMaxInstStrips },
    };
    for (const auto& is : kInsertSets)
    {
        for (int i = 0; i < is.count; ++i)
        {
            if (auto* eq = mVibeGraph.getInsertPreEQ(is.kind, i))
            {
                const juce::String prefix = juce::String(is.prefixBase) + juce::String(i);
                updateEQFromApvts(eq, prefix + "_preeq_mid_eq", prefix + "_preeq_side_eq");
            }
        }
    }
}

// ── Audio clip playback ───────────────────────────────────────────────────────
void VibeSynthProcessor::rebuildAudioClipPlayers()
{
    if (!mPatternManager) return;
    constexpr double kBPB = 4.0;  // beats per bar (4/4)

    std::vector<AudioClipPlayer> newPlayers;
    for (int i = 0; i < mPatternManager->getNumBlocks(); ++i)
    {
        const auto& blk = mPatternManager->getBlock(i);
        if (blk.clipType != ClipType::Audio || blk.audioFilePath.isEmpty() || blk.muted)
            continue;
        // NOTE: no isRowAudible() gate here — runtime mute/solo is handled in the
        // live render loop so toggling mute does not require a player rebuild.

        // P4: resolve relative paths like "Samples/kick.wav" against the
        // current project folder.  Absolute paths fall through unchanged
        // (legacy pre-P4 projects stored full paths).
        const auto resolvedFile = resolveProjectFile (blk.audioFilePath);
        std::unique_ptr<juce::AudioFormatReader> rawReader (
            mAudioFormatManager.createReaderFor (resolvedFile));
        if (!rawReader) continue;

        AudioClipPlayer p;
        p.clipStartBeat  = blk.startBar * kBPB;
        // 2026-04-24: prefer block.lengthBeats when set (sub-bar precision
        // from recordings) so playback ends at the real audio end, not the
        // ceil'd bar count.
        p.clipEndBeat    = blk.startBar * kBPB + effectiveLengthBeats (blk);
        p.trackRow       = blk.trackRow;
        p.originalBPM    = (blk.originalBPM > 0.f) ? blk.originalBPM : 120.f;
        p.stretchMode    = blk.stretchMode;
        p.fileSampleRate = rawReader->sampleRate;
        // D3: look up the source clip's choke group from the library.
        p.chokeGroup     = 0;
        for (int li = 0; li < mPatternManager->getNumAudioLibrary(); ++li)
            if (mPatternManager->getAudioLibraryPath(li) == blk.audioFilePath)
                { p.chokeGroup = mPatternManager->getAudioLibraryChokeGroup(li); break; }

        // Create disk-streaming player.
        // seek(0) synchronously pre-fills 2 seconds so the clip plays immediately.
        p.streamer = std::make_unique<AudioClipStreamer> (std::move (rawReader),
                                                         mAudioFileThread);
        p.streamer->seek (0);

        // Create phase vocoder when stretch mode is enabled.
        if (blk.stretchMode)
        {
            const int pvCh = p.streamer->getNumChannels();
            p.vocoder = std::make_unique<PhaseVocoder> (pvCh);
            // Pre-allocate scratch buffers — sized for worst-case block + PV headroom.
            // pvInBuf:  file samples fed into PV each block (up to ~4x block size for large stretch)
            // pvOutBuf: stretched output from PV (one full analysis window of headroom)
            const int maxBlockSamples = 8192;
            const int pvInCap  = maxBlockSamples * 4 + PhaseVocoder::kFFTSize;
            const int pvOutCap = maxBlockSamples * 4 + PhaseVocoder::kFFTSize;
            p.pvInBuf .setSize (pvCh, pvInCap,  false, true, false);
            p.pvOutBuf.setSize (pvCh, pvOutCap, false, true, false);
        }

        p.expectedFilePos = 0;
        newPlayers.push_back (std::move (p));
    }

    // Swap under lock — audio thread always sees a consistent list.
    // Old streamers are destroyed after the lock is released (no blocking under lock).
    {
        juce::SpinLock::ScopedLockType lk (mAudioClipLock);
        std::swap (mAudioClipPlayers, newPlayers);
    }
    // newPlayers (old streamers) destroyed here on the message thread.
}

// ─────────────────────────────────────────────────────────────────────────────
// D3: Choke-group dispatch (audio thread, wait-free).
// ─────────────────────────────────────────────────────────────────────────────
// Build the per-buffer noteOn list, then for each entry whose source insert
// has chokeGroup G > 0, scan all OTHER inserts (across all 3 engine types)
// and inject a noteOff (per-channel allNotesOff) into their buffers at the
// same sample position so the engines silence before consuming the buffer.
//
// Cost: O(buffers × notes × inserts).  In practice trivially small — typical
// block has 0-3 noteOns and there are ≤ 28 inserts (8 layer + 4 bass + 16 drum).
//
// Audio-clip choke is handled separately at clip-start time (Batch 4).
void VibeSynthProcessor::applyChokeGroupDispatch(
    std::array<juce::MidiBuffer, kMaxLayerPages>& layerMidi,
    std::array<juce::MidiBuffer, kMaxBassPages>&  bassMidi,
    std::array<juce::MidiBuffer, kMaxDrumPages>&  drumMidi,
    std::array<juce::MidiBuffer, kMaxVoxPages>&   voxMidi,
    std::array<juce::MidiBuffer, kMaxInstPages>&  instMidi,
    juce::int64 projectStartSamp,
    int         numSamples,
    double      secPerBeat)
{
    using Kind = VibeGraph::InsertKind;

    // ── 1. Reset mutedByChoke on audio clips not currently in range ─────
    // A clip silenced by choke during a previous playback should start fresh
    // when the playhead re-enters its range.  Reset before adding new fires
    // so the upcoming choke broadcast sticks for the rest of this playthrough.
    juce::SpinLock::ScopedTryLockType acLock(mAudioClipLock);
    if (acLock.isLocked())
    {
        const juce::int64 projectEnd = projectStartSamp + numSamples;
        for (auto& player : mAudioClipPlayers)
        {
            const juce::int64 cs = (juce::int64)(player.clipStartBeat * secPerBeat * mSampleRate);
            const juce::int64 ce = (juce::int64)(player.clipEndBeat   * secPerBeat * mSampleRate);
            if (projectEnd <= cs || projectStartSamp >= ce)
                player.mutedByChoke = false;
        }
    }

    // ── 2. Build the fires list (synth note-ons + audio clip starts) ────
    struct ChokeFire {
        // Distinguishes self when iterating peers.
        enum class Src { Synth, Audio };
        Src   src;
        Kind  kind { Kind::Layer };   // synth only
        int   index { -1 };           // synth: insert idx; audio: clip idx
        int   group { 0 };
        int   sample { 0 };
    };
    std::vector<ChokeFire> fires;
    fires.reserve(8);

    // Synth noteOns.
    auto scan = [&](Kind kind, int idx, juce::MidiBuffer& buf)
    {
        const int g = mVibeGraph.getInsertChokeGroup(kind, idx);
        if (g <= 0) return;
        for (const auto m : buf)
        {
            const auto msg = m.getMessage();
            if (msg.isNoteOn())
                fires.push_back({ ChokeFire::Src::Synth, kind, idx, g, m.samplePosition });
        }
    };
    for (int i = 0; i < kMaxLayerPages; ++i) scan(Kind::Layer, i, layerMidi[i]);
    for (int i = 0; i < kMaxBassPages;  ++i) scan(Kind::Bass,  i, bassMidi[i]);
    for (int i = 0; i < kMaxDrumPages;  ++i) scan(Kind::Drum,  i, drumMidi[i]);
    for (int i = 0; i < kMaxVoxPages;   ++i) scan(Kind::Vox,   i, voxMidi[i]);
    for (int i = 0; i < kMaxInstPages;  ++i) scan(Kind::Inst,  i, instMidi[i]);

    // Audio clip starts in this block.
    if (acLock.isLocked())
    {
        for (int ci = 0; ci < (int) mAudioClipPlayers.size(); ++ci)
        {
            const auto& p = mAudioClipPlayers[ci];
            if (p.chokeGroup <= 0) continue;
            const juce::int64 cs = (juce::int64)(p.clipStartBeat * secPerBeat * mSampleRate);
            // Clip "starts" if its absolute sample falls inside [projectStart, projectEnd).
            if (cs >= projectStartSamp && cs < projectStartSamp + numSamples)
            {
                const int sampInBlock = (int)(cs - projectStartSamp);
                fires.push_back({ ChokeFire::Src::Audio,
                                  Kind::Audio /* unused for audio */, ci,
                                  p.chokeGroup, sampInBlock });
            }
        }
    }

    if (fires.empty()) return;

    // ── 3. Dispatch each fire to peers in the same group ────────────────
    auto injectMidi = [&](Kind kind, int idx, juce::MidiBuffer& buf, const ChokeFire& f)
    {
        if (f.src == ChokeFire::Src::Synth && kind == f.kind && idx == f.index)
            return;   // don't choke self
        const int g = mVibeGraph.getInsertChokeGroup(kind, idx);
        if (g != f.group) return;
        // allNotesOff (channel 1) silences any held voices.
        buf.addEvent(juce::MidiMessage::allNotesOff(1), f.sample);
    };

    for (const auto& f : fires)
    {
        // Synth peers (always considered, audio fire chokes synths too).
        for (int i = 0; i < kMaxLayerPages; ++i) injectMidi(Kind::Layer, i, layerMidi[i], f);
        for (int i = 0; i < kMaxBassPages;  ++i) injectMidi(Kind::Bass,  i, bassMidi[i],  f);
        for (int i = 0; i < kMaxDrumPages;  ++i) injectMidi(Kind::Drum,  i, drumMidi[i],  f);
        for (int i = 0; i < kMaxVoxPages;   ++i) injectMidi(Kind::Vox,   i, voxMidi[i],   f);
        for (int i = 0; i < kMaxInstPages;  ++i) injectMidi(Kind::Inst,  i, instMidi[i],  f);

        // Audio peers — set mutedByChoke=true on others in same group.
        if (acLock.isLocked())
        {
            for (int ci = 0; ci < (int) mAudioClipPlayers.size(); ++ci)
            {
                if (f.src == ChokeFire::Src::Audio && ci == f.index) continue;
                auto& p = mAudioClipPlayers[ci];
                if (p.chokeGroup != f.group) continue;
                p.mutedByChoke = true;
            }
        }
    }
}

void VibeSynthProcessor::updateDrumMixLevels()
{
    // No-op now that DrumSynth is gone — per-drum-tab levels flow through
    // their mixer strips' faders, applied inside each drum InsertNode.
}

void VibeSynthProcessor::syncMixerFromPatternManager()
{
    if (!mPatternManager) return;
    const auto& mx = mPatternManager->getMixer();

    // Keep MixLevels in sync (still used for backwards-compatible access)
    mixLevels.master     = mx.masterLevel;
    mixLevels.layers     = mx.layersLevel;
    mixLevels.bass       = mx.bassLevel;
    mixLevels.drums      = mx.drumsLevel;
    mixLevels.layersMute = mx.layersMute;
    mixLevels.bassMute   = mx.bassMute;
    mixLevels.drumsMute  = mx.drumsMute;
    mixLevels.layersSolo = mx.layersSolo;
    mixLevels.bassSolo   = mx.bassSolo;
    mixLevels.drumsSolo  = mx.drumsSolo;

    // Sync the graph's bus mix (read on audio thread in bus nodes)
    auto& bm = mVibeGraph.busMix;
    bm.layersGain  = mx.layersLevel;
    bm.bassGain    = mx.bassLevel;
    bm.drumsGain   = mx.drumsLevel;
    bm.masterFader = mx.masterLevel;
    bm.layersMute  = mx.layersMute;
    bm.bassMute    = mx.bassMute;
    bm.drumsMute   = mx.drumsMute;
    bm.layersSolo  = mx.layersSolo;
    bm.bassSolo    = mx.bassSolo;
    bm.drumsSolo   = mx.drumsSolo;
}


// ── Recording ─────────────────────────────────────────────────────────────────
// R5d (2026-04-24): mode-aware recording engine.  Audio mode scans Vox / Inst
// strips for _arm ON; each armed strip gets a dedicated WAV.  With zero
// strips armed, the master output is captured to a single WAV.  MIDI mode
// skips audio writers entirely - MidiRecorder handles note capture; Editor
// drops the notes into the last-accessed piano roll on stop.
void VibeSynthProcessor::startRecording (RecordMode mode,
                                          double startBeat,
                                          const juce::String& projectName,
                                          const juce::File& samplesFolder)
{
    mRecordMode.store (mode, std::memory_order_relaxed);
    mRecordStartBeat = startBeat;
    mStripRecorders.clear();

    const auto now = juce::Time::getCurrentTime();
    // Windows-filename-safe: YYYY-MM-DD HH-MM-SS
    const auto ts  = now.formatted ("%Y-%m-%d %H-%M-%S");

    // Always arm MIDI; harmless when Editor ignores the notes in Audio mode.
    mMidiRecorder.startRecording (startBeat);

    if (mode != RecordMode::Audio) return;

    samplesFolder.createDirectory();

    // Scan Vox + Inst strips for _arm on.
    auto scan = [&](const char* prefixBase, int maxCount, int chBase,
                    const char* displayBase)
    {
        for (int i = 0; i < maxCount; ++i)
        {
            const juce::String prefix = juce::String (prefixBase) + juce::String (i);
            const auto* armP = apvts.getRawParameterValue (prefix + "_arm");
            if (armP == nullptr || armP->load() < 0.5f) continue;

            StripRecorder sr;
            sr.channelId   = chBase + i;
            sr.displayName = juce::String (displayBase) + " " + juce::String (i + 1);
            sr.file        = samplesFolder.getChildFile (
                projectName + " - " + sr.displayName + " - " + ts + ".wav");
            sr.recorder    = std::make_unique<AudioFileRecorder>();
            if (sr.recorder->startRecording (sr.file, mSampleRate, 1))
                mStripRecorders.push_back (std::move (sr));
        }
    };
    scan ("mixer_vox_",  MixerChannelIds::kMaxVoxStrips,
          MixerChannelIds::kVoxBase,  "Vox");
    scan ("mixer_inst_", MixerChannelIds::kMaxInstStrips,
          MixerChannelIds::kInstBase, "Inst");

    // No strips armed -> fall back to master output capture.
    if (mStripRecorders.empty())
    {
        auto file = samplesFolder.getChildFile (
            projectName + " - Master - " + ts + ".wav");
        mMasterRecorder.startRecording (file, mSampleRate, 2);
    }
}

VibeSynthProcessor::RecordResult VibeSynthProcessor::stopRecording()
{
    RecordResult out;
    out.startBeat = mRecordStartBeat;
    out.midiNotes = mMidiRecorder.stopRecording();

    if (mMasterRecorder.isRecording())
        out.masterFile = mMasterRecorder.stopRecording();

    for (auto& sr : mStripRecorders)
    {
        if (sr.recorder && sr.recorder->isRecording())
        {
            auto f = sr.recorder->stopRecording();
            if (f.existsAsFile())
                out.stripFiles.emplace_back (sr.channelId, f);
        }
    }
    mStripRecorders.clear();
    return out;
}

// ── State persistence ─────────────────────────────────────────────────────────
void VibeSynthProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    // Always rebuild the rack states child from scratch (avoid stale duplicate).
    state.removeChild(state.getChildWithName("VibeRackStates"), nullptr);
    juce::ValueTree rackStates("VibeRackStates");
    mVibeGraph.saveRackStates(rackStates);
    state.addChild(rackStates, -1, nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VibeSynthProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (!xmlState) return;

    auto state = juce::ValueTree::fromXml(*xmlState);
    if (!state.isValid()) return;

    // Extract and apply rack states before passing to APVTS (keeps APVTS tree clean).
    auto rackStates = state.getChildWithName("VibeRackStates");
    if (rackStates.isValid())
    {
        state.removeChild(rackStates, nullptr);
        mVibeGraph.loadRackStates(rackStates);   // deferred if topology not built yet
    }

    if (state.hasType(apvts.state.getType()))
        apvts.replaceState(state);

    // 5F-4b B7: re-register any aux strips that were in the saved project.
    // Must happen AFTER replaceState so the saved param values are in the tree.
    restoreAuxStripsFromState();
}

// ── Project persistence (P1, 2026-04-23) ────────────────────────────────────
// These methods produce / consume a full project snapshot including everything
// getStateInformation covers (APVTS + rack states) plus the PatternManager tree
// that today has no disk path.  The shape:
//
//   <BaySickDAWProject version="1">
//     <Processor>
//       <APVTSState>...</APVTSState>          ← APVTS + VibeRackStates child
//     </Processor>
//     <PatternManager version="1">
//       <Patterns>...</Patterns>
//       <Arrangement>...</Arrangement>
//       <AudioLibrary>...</AudioLibrary>
//       <AutomationTemplates>...</AutomationTemplates>
//       (etc.)
//     </PatternManager>
//   </BaySickDAWProject>
//
// ProjectManager writes this to <projectFolder>/project.xml.  The legacy
// getStateInformation/setStateInformation blob format is kept untouched so any
// prior state files still load (APVTS-only, no pattern content).
void VibeSynthProcessor::serializeProject (juce::XmlElement& root)
{
    root.setAttribute ("version", 1);

    // Processor state (APVTS + rack states) — reuse the same ValueTree we
    // produce in getStateInformation, but emit as XML child instead of a
    // MemoryBlock blob.
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("VibeRackStates"), nullptr);
    juce::ValueTree rackStates ("VibeRackStates");
    mVibeGraph.saveRackStates (rackStates);
    state.addChild (rackStates, -1, nullptr);

    auto* processor = root.createNewChildElement ("Processor");
    if (auto stateXml = state.createXml())
        processor->addChildElement (stateXml.release());

    // PatternManager — patterns, arrangement, piano-roll notes, libraries,
    // row mute/solo, drum-enabled flags, full mixer snapshot.
    if (mPatternManager != nullptr)
    {
        auto pmTree = mPatternManager->toValueTree();
        if (auto pmXml = pmTree.createXml())
            root.addChildElement (pmXml.release());
    }

    // P1+P2 persistence (2026-04-24): let StandaloneEditor append its tab +
    // engine state under a <UIState> child.  Callback is null in plugin /
    // headless contexts - that's fine; the project just omits UI state.
    if (onSerializeUIState)
        onSerializeUIState (root);
}

void VibeSynthProcessor::applyPendingRackStates()
{
    // 2026-04-24: re-sync APVTS parameter adapters to the loaded state tree.
    // JUCE's replaceState binds every adapter to its matching tree child
    // ONCE - for params registered LATER (lazy mixer-strip params added
    // inside the editor's deserializeUIState / restoreAudioStripsFromArrangement
    // after the replaceState call), their adapter was created with no tree
    // binding and the param kept its constructor-default value even though
    // the tree had the user's saved value.  Assigning state to a fresh copy
    // of itself triggers valueTreeRedirected -> updateParameterConnectionsToChildTrees
    // which rebinds every adapter, so newly-registered params pick up their
    // saved values.  Must happen BEFORE rack-state apply so any effect's
    // post-rack-EQ params also get their saved values before the EQs read.
    apvts.replaceState (apvts.copyState());

    if (! mPendingProjectRackState.isValid()) return;
    mVibeGraph.loadRackStates (mPendingProjectRackState);
    mPendingProjectRackState = {};
}

void VibeSynthProcessor::resetToBlankState()
{
    // Reset every registered APVTS param to its default value.  Iterate via
    // getParameters() so lazy-registered engine / mixer-strip / rack params
    // all get swept regardless of when they were added.
    for (auto* param : getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            // 2026-04-24 bugfix: getDefaultValue() already returns normalised
            // 0..1 (JUCE contract).  Wrapping it in convertTo0to1 caused
            // double-normalisation - Int params with large ranges (e.g.
            // _sendTo 0..999) collapsed to 0, silently breaking every strip's
            // routing after File > New.  Pass the normalised default straight
            // through.
            ranged->setValueNotifyingHost (ranged->getDefaultValue());
        }
    }

    // Clear every rack slot across buses + inserts so File > New / File >
    // Open never bleeds the previous session's effect chains through.
    // loadRackStates only RESTORES from the tree - with no matching entries
    // it wouldn't wipe pre-existing racks, which was the observed bug.
    mVibeGraph.clearAllRackStates();

    // PatternManager back to one empty default pattern.
    if (mPatternManager)
        mPatternManager->reset();
}

void VibeSynthProcessor::setCurrentProjectFolder (const juce::File& folder)
{
    juce::ScopedLock sl (mProjectFolderLock);
    mCurrentProjectFolder = folder;
}

juce::File VibeSynthProcessor::getCurrentProjectFolder() const
{
    juce::ScopedLock sl (mProjectFolderLock);
    return mCurrentProjectFolder;
}

juce::File VibeSynthProcessor::resolveProjectFile (const juce::String& storedPath) const
{
    if (storedPath.isEmpty()) return {};
    // Absolute paths pass through (pre-P4 projects stored absolute audio paths).
    if (juce::File::isAbsolutePath (storedPath)) return juce::File (storedPath);
    // Relative paths — resolve against current project folder.
    juce::ScopedLock sl (mProjectFolderLock);
    if (mCurrentProjectFolder == juce::File()) return {};
    return mCurrentProjectFolder.getChildFile (storedPath);
}

void VibeSynthProcessor::deserializeProject (const juce::XmlElement& root)
{
    // Processor state — first child under <Processor>.
    if (auto* processor = root.getChildByName ("Processor"))
    {
        // The saved APVTS tree is the single child of <Processor>.
        for (auto* child : processor->getChildIterator())
        {
            auto state = juce::ValueTree::fromXml (*child);
            if (! state.isValid()) continue;

            auto rackStates = state.getChildWithName ("VibeRackStates");
            if (rackStates.isValid())
            {
                state.removeChild (rackStates, nullptr);
                // 2026-04-24: stash - replay AFTER the editor finishes
                // rebuilding tabs + audio strips (so per-insert InsertNodes
                // exist).  loadRackStates still runs once here to cover
                // fixed-bus racks (Layers/Bass/Drums/Master/EffectsBus) +
                // ClipsBus/VoxBus/InstBus which persist across sessions.
                mPendingProjectRackState = rackStates.createCopy();
                mVibeGraph.loadRackStates (rackStates);
            }

            if (state.hasType (apvts.state.getType()))
                apvts.replaceState (state);
            break;   // only one APVTS state child expected
        }

        // Aux strips follow from APVTS param presence — same path as
        // setStateInformation.
        restoreAuxStripsFromState();
    }

    // PatternManager — top-level child named "PatternManager".
    if (mPatternManager != nullptr)
    {
        if (auto* pmXml = root.getChildByName ("PatternManager"))
        {
            auto pmTree = juce::ValueTree::fromXml (*pmXml);
            if (pmTree.isValid())
                mPatternManager->fromValueTree (pmTree);
        }
    }

    // P1+P2 persistence: fire after main state is loaded so the editor's
    // engine-processor creation can inherit any APVTS-driven defaults.
    if (onDeserializeUIState)
        onDeserializeUIState (root);
}

// 5F-4b B7: scan the APVTS ValueTree for saved mixer_aux_N params and
// re-register their InsertNodes + APVTS params so the audio path and UI
// can pick them up when the editor is created.
void VibeSynthProcessor::restoreAuxStripsFromState()
{
    for (int idx = 0; idx < MixerChannelIds::kMaxAuxStrips; ++idx)
    {
        const juce::String testId = "mixer_aux_" + juce::String(idx) + "_level";

        // Check if this param exists in the saved state tree.
        // APVTS stores params as children with property "id".
        bool found = false;
        for (int c = 0; c < apvts.state.getNumChildren(); ++c)
        {
            auto child = apvts.state.getChild(c);
            if (child.hasProperty("id") && child["id"].toString() == testId)
            {
                found = true;
                break;
            }
        }

        if (found)
            ensureAuxInsert(idx, "Aux " + juce::String(idx + 1));
    }
}

// ── Editor factory (VST only) ─────────────────────────────────────────────────
#ifdef VIBESYNTH_VST
juce::AudioProcessorEditor* VibeSynthProcessor::createEditor()
{
    return new VibesynthEditor(*this);
}
#else
juce::AudioProcessorEditor* VibeSynthProcessor::createEditor()
{
    return nullptr; // Standalone uses StandaloneEditor, not AudioProcessorEditor
}
#endif

// Required by JUCE VST3 plugin hosting
#ifdef VIBESYNTH_VST
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VibeSynthProcessor();
}
#endif

// ── Lazy APVTS registration ───────────────────────────────────────────────────
// Parameters are registered dynamically when a page/track is opened, and
// marked inactive when closed.  The param objects remain in the APVTS tree
// (JUCE has no remove API), but the mRegisteredTrackParams set tracks which
// trackIds are currently live so callers can query isTrackRegistered().
//
// Parameter naming convention:  tk_{trackId}_{engine}_{param}
//   e.g. tk_0_Harmless_macro0, tk_2_BaySickSynth_oscDetune
// EQ naming:    tk_{trackId}_eq_mid_band{b}_{freq|gain|q|type|on}
// Rack naming:  tk_{trackId}_rack_slot{s}_{param}

namespace
{
    // Helper: add a float parameter to APVTS and record its ID in outIds.
    void dynF(juce::AudioProcessorValueTreeState& apvts, juce::StringArray& ids,
              const juce::String& id, const juce::String& name,
              float lo, float hi, float def)
    {
        // Only add if not already present
        if (apvts.getParameter(id) == nullptr)
            apvts.createAndAddParameter(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{id, 1}, name,
                juce::NormalisableRange<float>(lo, hi), def));
        ids.add(id);
    }

    void dynB(juce::AudioProcessorValueTreeState& apvts, juce::StringArray& ids,
              const juce::String& id, const juce::String& name, bool def)
    {
        if (apvts.getParameter(id) == nullptr)
            apvts.createAndAddParameter(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID{id, 1}, name, def));
        ids.add(id);
    }

    void dynI(juce::AudioProcessorValueTreeState& apvts, juce::StringArray& ids,
              const juce::String& id, const juce::String& name,
              int lo, int hi, int def)
    {
        if (apvts.getParameter(id) == nullptr)
            apvts.createAndAddParameter(std::make_unique<juce::AudioParameterInt>(
                juce::ParameterID{id, 1}, name, lo, hi, def));
        ids.add(id);
    }
} // namespace

void VibeSynthProcessor::addParamsForHarmless(const juce::String& prefix)
{
    // 8 macro knobs (full Harmless param set registered here; individual partials
    // are NOT APVTS-registered — they live in the Harmless preset only).
    auto& ids = mRegisteredTrackParams[prefix];
    for (int m = 0; m < 8; ++m)
    {
        juce::String id   = prefix + "_macro" + juce::String(m);
        juce::String name = prefix + " Macro " + juce::String(m + 1);
        dynF(apvts, ids, id, name, 0.f, 1.f, 0.f);
    }
    // Master volume + pitch + pan
    dynF(apvts, ids, prefix + "_vol",   prefix + " Volume", 0.f, 1.f, 0.8f);
    dynF(apvts, ids, prefix + "_pitch", prefix + " Pitch",  -24.f, 24.f, 0.f);
    dynF(apvts, ids, prefix + "_pan",   prefix + " Pan",    -1.f, 1.f, 0.f);
}

void VibeSynthProcessor::addParamsForVibePlayer(const juce::String& prefix)
{
    auto& ids = mRegisteredTrackParams[prefix];
    dynF(apvts, ids, prefix + "_vol",     prefix + " Volume",     0.f, 1.f,   0.8f);
    dynF(apvts, ids, prefix + "_pan",     prefix + " Pan",        -1.f, 1.f,  0.f);
    dynF(apvts, ids, prefix + "_pitch",   prefix + " Pitch",      -24.f, 24.f, 0.f);
    dynF(apvts, ids, prefix + "_attack",  prefix + " Attack",     0.f, 4.f,   0.01f);
    dynF(apvts, ids, prefix + "_decay",   prefix + " Decay",      0.f, 4.f,   0.2f);
    dynF(apvts, ids, prefix + "_sustain", prefix + " Sustain",    0.f, 1.f,   0.7f);
    dynF(apvts, ids, prefix + "_release", prefix + " Release",    0.f, 8.f,   0.3f);
    dynF(apvts, ids, prefix + "_start",   prefix + " Start",      0.f, 1.f,   0.f);
    dynF(apvts, ids, prefix + "_end",     prefix + " End",        0.f, 1.f,   1.f);
    dynF(apvts, ids, prefix + "_loop",    prefix + " Loop",       0.f, 1.f,   1.f);
    dynB(apvts, ids, prefix + "_reverse", prefix + " Reverse",    false);
    dynI(apvts, ids, prefix + "_quality", prefix + " Quality",    0, 3, 2);
}

void VibeSynthProcessor::addParamsForBaySickSynth(const juce::String& prefix)
{
    auto& ids = mRegisteredTrackParams[prefix];
    // Oscillator
    dynI(apvts, ids, prefix + "_oscMode",     prefix + " Osc Mode",     0, 3, 0);
    dynI(apvts, ids, prefix + "_classicShape",prefix + " Classic Shape", 0, 3, 1);
    dynF(apvts, ids, prefix + "_wtPos",       prefix + " WT Position",  0.f, 1.f, 0.f);
    dynI(apvts, ids, prefix + "_unisonVoices",prefix + " Unison Voices",1, 8, 1);
    dynF(apvts, ids, prefix + "_unisonSpread",prefix + " Unison Spread",0.f, 1.f, 0.f);
    dynF(apvts, ids, prefix + "_detune",      prefix + " Detune",       -24.f, 24.f, 0.f);
    dynF(apvts, ids, prefix + "_fmRatio",     prefix + " FM Ratio",     0.f, 16.f, 2.f);
    dynF(apvts, ids, prefix + "_fmIndex",     prefix + " FM Index",     0.f, 10.f, 1.f);
    dynF(apvts, ids, prefix + "_noiseAmt",    prefix + " Noise Amount", 0.f, 1.f, 0.f);
    dynF(apvts, ids, prefix + "_vol",         prefix + " Volume",       0.f, 1.f, 0.7f);
    // Sub oscillator
    dynF(apvts, ids, prefix + "_subVol",      prefix + " Sub Volume",   0.f, 1.f, 0.f);
    dynI(apvts, ids, prefix + "_subOct",      prefix + " Sub Octave",   1, 3, 1);
    // Filter
    dynI(apvts, ids, prefix + "_filterMode",  prefix + " Filter Mode",  0, 4, 0);
    dynF(apvts, ids, prefix + "_filterCut",   prefix + " Filter Cutoff",20.f, 20000.f, 4000.f);
    dynF(apvts, ids, prefix + "_filterRes",   prefix + " Filter Res",   0.f, 1.f, 0.f);
    dynF(apvts, ids, prefix + "_filterEnvAmt",prefix + " Filter Env",   -96.f, 96.f, 0.f);
    dynF(apvts, ids, prefix + "_filterKbd",   prefix + " Filter Kbd",   0.f, 1.f, 0.f);
    // Amp ADSR
    dynF(apvts, ids, prefix + "_ampA",   prefix + " Amp Attack",  0.f, 4.f,  0.01f);
    dynF(apvts, ids, prefix + "_ampD",   prefix + " Amp Decay",   0.f, 4.f,  0.2f);
    dynF(apvts, ids, prefix + "_ampS",   prefix + " Amp Sustain", 0.f, 1.f,  0.7f);
    dynF(apvts, ids, prefix + "_ampR",   prefix + " Amp Release", 0.f, 8.f,  0.3f);
    // Filter ADSR
    dynF(apvts, ids, prefix + "_fltA",   prefix + " Flt Attack",  0.f, 4.f,  0.01f);
    dynF(apvts, ids, prefix + "_fltD",   prefix + " Flt Decay",   0.f, 4.f,  0.2f);
    dynF(apvts, ids, prefix + "_fltS",   prefix + " Flt Sustain", 0.f, 1.f,  1.f);
    dynF(apvts, ids, prefix + "_fltR",   prefix + " Flt Release", 0.f, 8.f,  0.3f);
    // LFO
    dynF(apvts, ids, prefix + "_lfoRate",  prefix + " LFO Rate",  0.01f, 20.f, 1.f);
    dynF(apvts, ids, prefix + "_lfoDepth", prefix + " LFO Depth", 0.f, 1.f, 0.f);
    dynI(apvts, ids, prefix + "_lfoShape", prefix + " LFO Shape", 0, 4, 0);
    dynI(apvts, ids, prefix + "_lfoRoute", prefix + " LFO Route", 0, 3, 0);
    // Glide
    dynF(apvts, ids, prefix + "_glide",    prefix + " Glide",     0.f, 2.f, 0.f);
    dynB(apvts, ids, prefix + "_legato",   prefix + " Legato",    false);
}

void VibeSynthProcessor::addParamsForBaySickBass(const juce::String& prefix)
{
    // BaySickBass uses the same DSP as BaySickSynth, same param set
    addParamsForBaySickSynth(prefix);
}

// 2026-04-25: addParamsForBaySickDrums removed — legacy 16-slot drum
// processor deleted; per-drum-tab engines register their own params.

void VibeSynthProcessor::addParamsForTrackEQ(const juce::String& prefix)
{
    // Post-rack EQ (existing behavior — IDs at prefix + "_mid_eq{b}{Suffix}").
    addParamsForEQBank(prefix, juce::String());
}

// §P4.3: Pre-rack EQ.  IDs at prefix + "_preeq_mid_eq{b}{Suffix}" so the
// post-rack and pre-rack banks coexist on the same strip without collision.
void VibeSynthProcessor::addParamsForTrackPreEQ(const juce::String& prefix)
{
    addParamsForEQBank(prefix, "preeq_");
}

// Internal helper — registers an 8-band M/S EQ param bank under prefix +
// "_" + subPrefix + "{mid|side}_eq{b}{Suffix}".
//   subPrefix ""        → post-rack ("EQ" labels)
//   subPrefix "preeq_"  → pre-rack  ("Pre EQ" labels — disambiguates automation menus)
void VibeSynthProcessor::addParamsForEQBank(const juce::String& prefix,
                                             const juce::String& subPrefix)
{
    auto& ids = mRegisteredTrackParams[prefix];
    static const float kFreqs[8] = { 40.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f, 12000.f };
    const juce::String labelTag = subPrefix.isEmpty() ? "EQ" : "Pre EQ";
    for (const char* ch : { "mid", "side" })
    {
        // 12h: per-band channel default matches EQ8MsDSP's constructor-seeded behaviour
        // (mid bands -> Channel::Mid = 1, side bands -> Channel::Side = 2). PRESET-SAFE.
        const int chanDefault = (juce::String(ch) == "mid") ? 1 : 2;
        for (int b = 0; b < 8; ++b)
        {
            juce::String bp = prefix + "_" + subPrefix + ch + "_eq" + juce::String(b);
            const juce::String labelBase = prefix + " " + labelTag + " " + ch + " B" + juce::String(b);
            dynF(apvts, ids, bp + "Freq",    labelBase + " Freq",    20.f, 20000.f, kFreqs[b]);
            dynF(apvts, ids, bp + "Gain",    labelBase + " Gain",    -18.f, 18.f, 0.f);
            dynF(apvts, ids, bp + "Q",       labelBase + " Q",       0.1f, 10.f, 0.707f);
            dynI(apvts, ids, bp + "Type",    labelBase + " Type",    0, 8, 0);
            dynB(apvts, ids, bp + "On",      labelBase + " On",      true);
            dynI(apvts, ids, bp + "Slope",   labelBase + " Slope",   0, 6, 0);
            // Session B: Mute + Solo + Channel rounded out so every EQ instance exposes
            // the full automatable 9-param set. All additive PRESET-SAFE defaults.
            dynB(apvts, ids, bp + "Mute",    labelBase + " Mute",    false);
            dynB(apvts, ids, bp + "Solo",    labelBase + " Solo",    false);
            dynI(apvts, ids, bp + "Channel", labelBase + " Channel", 0, 4, chanDefault);  // 12h
            // 12j Dynamic EQ: 7 dynamic params + 1 sidechain-source scaffolding.
            // PRESET-SAFE additive; defaults (Dynamic=off etc.) preserve v1 behaviour.
            dynB(apvts, ids, bp + "Dynamic",   labelBase + " Dynamic",   false);
            dynF(apvts, ids, bp + "Threshold", labelBase + " Threshold", -60.f,   0.f, -18.f);
            dynF(apvts, ids, bp + "Ratio",     labelBase + " Ratio",       1.f,  20.f,   2.f);
            dynF(apvts, ids, bp + "Attack",    labelBase + " Attack",    0.1f, 500.f,  10.f);
            dynF(apvts, ids, bp + "Release",   labelBase + " Release",    1.f,2000.f, 100.f);
            // 12j follow-up Q2: Range is bipolar. Negative = downward compression,
            // positive = upward expansion, zero = no modulation. Direction + amount
            // encoded in one value. PRESET-BREAK ⚠️ pre-v1 (old unsigned Range +
            // Upward split is gone). Upward is kept as unused scaffolding for
            // preset stability.
            // 2026-04-19 follow-up: range magnitude matched to the Gain param
            // magnitude (-18..+18) so Range can't exceed what the band's gain
            // could theoretically reach on its own. Keeps the live-animated curve
            // inside the -18..+18 dB grid for single-band modulation scenarios.
            dynF(apvts, ids, bp + "Range",     labelBase + " Range",    -18.f, 18.f, -12.f);
            dynB(apvts, ids, bp + "Upward",    labelBase + " Upward",    false);
            // Option B scaffolding for Tier 3 T11 external sidechain. Default -1
            // = internal (band's own input); integer routing id when ready.
            dynI(apvts, ids, bp + "ScSource",  labelBase + " ScSource", -1, 999, -1);
        }
    }
}

void VibeSynthProcessor::addParamsForEffectRack(const juce::String& prefix)
{
    // 6 slots × ~15 params each.  Param IDs follow tk_{trackId}_rack_slot{s}_{param}.
    auto& ids = mRegisteredTrackParams[prefix];
    for (int s = 0; s < 6; ++s)
    {
        juce::String sp = prefix + "_rack_slot" + juce::String(s);
        dynI(apvts, ids, sp + "_type",    prefix + " Rack S" + juce::String(s) + " Type",  -1, 11, -1);
        dynB(apvts, ids, sp + "_bypass",  prefix + " Rack S" + juce::String(s) + " Bypass",false);
        dynF(apvts, ids, sp + "_output",  prefix + " Rack S" + juce::String(s) + " Output",0.f, 1.f, 1.f);
        // Per-effect params use generic names; actual params populated when slot type is set
        for (int p = 0; p < 12; ++p)
        {
            juce::String pp = sp + "_p" + juce::String(p);
            dynF(apvts, ids, pp, prefix + " Rack S" + juce::String(s) + " P" + juce::String(p), 0.f, 1.f, 0.f);
        }
    }
}

// ── Engine processor registration ────────────────────────────────────────────
void VibeSynthProcessor::registerLayerEngine(int idx, juce::AudioProcessor* eng)
{
    {
        juce::SpinLock::ScopedLockType lk(mLayerEngineLock);
        if (idx >= 0 && idx < kMaxLayerPages) mLayerEngines[idx] = eng;
    }
    // 5F-4a: ensure mixer strip params + Layer InsertNode exist for this page.
    // Message thread only — safe to call APVTS and VibeGraph.
    if (idx >= 0 && idx < kMaxLayerPages && eng != nullptr)
    {
        const juce::String prefix = "mixer_layer_" + juce::String(idx);
        ensureMixerStripParams(prefix, MixerStripKind::Insert, MixerChannelIds::kLayersBus);
        mVibeGraph.ensureInsertNode(VibeGraph::InsertKind::Layer, idx,
                                     "Layer " + juce::String(idx + 1), prefix);
    }
}
void VibeSynthProcessor::unregisterLayerEngine(int idx)
{
    juce::SpinLock::ScopedLockType lk(mLayerEngineLock);
    if (idx >= 0 && idx < kMaxLayerPages) mLayerEngines[idx] = nullptr;
    // InsertNode retained on purpose — preserves mixer state if the page is re-opened.
}
void VibeSynthProcessor::registerBassEngine(int pageIdx, juce::AudioProcessor* eng)
{
    if (pageIdx < 0 || pageIdx >= kMaxBassPages) return;
    {
        juce::SpinLock::ScopedLockType lk(mBassEngineLock);
        mBassEngines[pageIdx] = eng;
    }
    if (eng != nullptr)
    {
        const juce::String prefix = "mixer_bass_" + juce::String(pageIdx);
        ensureMixerStripParams(prefix, MixerStripKind::Insert, MixerChannelIds::kBassBus);
        mVibeGraph.ensureInsertNode(VibeGraph::InsertKind::Bass, pageIdx,
                                     "Bass " + juce::String(pageIdx + 1), prefix);
    }
}
void VibeSynthProcessor::unregisterBassEngine(int pageIdx)
{
    if (pageIdx < 0 || pageIdx >= kMaxBassPages) return;
    juce::SpinLock::ScopedLockType lk(mBassEngineLock);
    mBassEngines[pageIdx] = nullptr;
}
// 2026-04-25: registerDrumsEngine / unregisterDrumsEngine removed — legacy
// 16-slot BaySickDrumsProcessor deleted.  Per-drum-tab registration uses
// registerDrumEngine (singular) below.

// D1.2 (2026-04-24): per-drum-page engine registration.  Each DrumPage tab
// owns one independent engine; this wires it into the audio graph the same
// way layers/bass do.  Mixer strip + InsertNode reuse the existing Drum
// kind/range so the mixer UI stays consistent during the D1 transition.
void VibeSynthProcessor::registerDrumEngine(int pageIdx, juce::AudioProcessor* eng)
{
    if (pageIdx < 0 || pageIdx >= kMaxDrumPages) return;
    {
        juce::SpinLock::ScopedLockType lk(mDrumEngineLock);
        mDrumEngines[pageIdx] = eng;
    }
    if (eng != nullptr)
    {
        const juce::String prefix = "mixer_drum_" + juce::String(pageIdx);
        ensureMixerStripParams(prefix, MixerStripKind::Insert, MixerChannelIds::kDrumsBus);
        mVibeGraph.ensureInsertNode(VibeGraph::InsertKind::Drum, pageIdx,
                                     "Drum " + juce::String(pageIdx + 1), prefix);
    }
    // Recompute fast-path flag (any engine alive?)
    bool any = false;
    for (auto* e : mDrumEngines) if (e) { any = true; break; }
    mAnyDrumPageActive.store(any, std::memory_order_release);
}
void VibeSynthProcessor::unregisterDrumEngine(int pageIdx)
{
    if (pageIdx < 0 || pageIdx >= kMaxDrumPages) return;
    {
        juce::SpinLock::ScopedLockType lk(mDrumEngineLock);
        mDrumEngines[pageIdx] = nullptr;
    }
    bool any = false;
    for (auto* e : mDrumEngines) if (e) { any = true; break; }
    mAnyDrumPageActive.store(any, std::memory_order_release);
    // InsertNode retained — preserves mixer state if drum is re-added.
}

// G-3 (2026-04-28): per-clip-page engine registration.  pageIdx is the audio-
// row index for the bound clip (1:1 mapping to mixer_audio_<row>).  Unlike
// Layer / Bass / Drum engines which create their own InsertNode + mixer
// strip, Clip engines share the existing Audio InsertNode for that row —
// arrangement-playback audio + piano-roll-triggered audio mix into the same
// strip so the user sees one channel per clip rather than two.
void VibeSynthProcessor::registerClipEngine(int pageIdx, juce::AudioProcessor* eng)
{
    if (pageIdx < 0 || pageIdx >= kMaxClipPages) return;
    {
        juce::SpinLock::ScopedLockType lk(mClipEngineLock);
        mClipEngines[pageIdx] = eng;
    }
    // The Audio InsertNode for this row was already created when the clip
    // was first dropped on Builder (onAudioClipAdded → ensureAudioInsert),
    // so we don't need to create one here.  Just flip the fast-path flag.
    bool any = false;
    for (auto* e : mClipEngines) if (e) { any = true; break; }
    mAnyClipPageActive.store(any, std::memory_order_release);
}

void VibeSynthProcessor::unregisterClipEngine(int pageIdx)
{
    if (pageIdx < 0 || pageIdx >= kMaxClipPages) return;
    {
        juce::SpinLock::ScopedLockType lk(mClipEngineLock);
        mClipEngines[pageIdx] = nullptr;
    }
    bool any = false;
    for (auto* e : mClipEngines) if (e) { any = true; break; }
    mAnyClipPageActive.store(any, std::memory_order_release);
}

// G-4 (2026-04-28): per-Vox / per-Inst engine registration.  pageIdx is the
// Vox / Inst insert index (1:1 with mixer_vox_<idx> / mixer_inst_<idx>).
// The Vox / Inst InsertNode for the row was created when the user clicked
// "Add Vox/Inst Strip" on the Mixer page (R3 wiring); we just register the
// engine for audio-thread dispatch.
void VibeSynthProcessor::registerVoxEngine(int pageIdx, juce::AudioProcessor* eng)
{
    if (pageIdx < 0 || pageIdx >= kMaxVoxPages) return;
    {
        juce::SpinLock::ScopedLockType lk(mVoxEngineLock);
        mVoxEngines[pageIdx] = eng;
    }
    bool any = false;
    for (auto* e : mVoxEngines) if (e) { any = true; break; }
    mAnyVoxPageActive.store(any, std::memory_order_release);
}

void VibeSynthProcessor::unregisterVoxEngine(int pageIdx)
{
    if (pageIdx < 0 || pageIdx >= kMaxVoxPages) return;
    {
        juce::SpinLock::ScopedLockType lk(mVoxEngineLock);
        mVoxEngines[pageIdx] = nullptr;
    }
    bool any = false;
    for (auto* e : mVoxEngines) if (e) { any = true; break; }
    mAnyVoxPageActive.store(any, std::memory_order_release);
}

void VibeSynthProcessor::registerInstEngine(int pageIdx, juce::AudioProcessor* eng)
{
    if (pageIdx < 0 || pageIdx >= kMaxInstPages) return;
    {
        juce::SpinLock::ScopedLockType lk(mInstEngineLock);
        mInstEngines[pageIdx] = eng;
    }
    bool any = false;
    for (auto* e : mInstEngines) if (e) { any = true; break; }
    mAnyInstPageActive.store(any, std::memory_order_release);
}

void VibeSynthProcessor::unregisterInstEngine(int pageIdx)
{
    if (pageIdx < 0 || pageIdx >= kMaxInstPages) return;
    {
        juce::SpinLock::ScopedLockType lk(mInstEngineLock);
        mInstEngines[pageIdx] = nullptr;
    }
    bool any = false;
    for (auto* e : mInstEngines) if (e) { any = true; break; }
    mAnyInstPageActive.store(any, std::memory_order_release);
}

// §P4.3 B7 (2026-04-22): register/unregister{Layer,Bass,Drums}PageEQ APIs
// deleted.  Per-page EQ DSPs (mLayerPageEQs / mBassPageEQs / mDrumsPageEQ)
// are gone — pages now bind their EQ display to the InsertNode / BusNode
// preEq directly via VibeGraph::getInsertPreEQ() / getXxxBusPreEQ().

void VibeSynthProcessor::registerParamsForTrack(const juce::String& trackId,
                                                 const juce::String& engineType)
{
    // Idempotent: if already registered, do nothing
    if (mRegisteredTrackParams.count(trackId)) return;

    // Ensure the entry exists (engine params will add to it)
    mRegisteredTrackParams[trackId] = {};

    const juce::String prefix = "tk_" + trackId;

    // Register engine-specific params
    if      (engineType == "Harmless")      addParamsForHarmless    (prefix);
    else if (engineType == "BaySickPlayer") addParamsForVibePlayer  (prefix);
    else if (engineType == "BaySickSynth")  addParamsForBaySickSynth(prefix);
    else if (engineType == "BaySickBass")   addParamsForBaySickBass (prefix);
    // 2026-04-25: "BaySickDrums" engine type removed (legacy processor deleted).

    // §P4.3 B7: legacy per-track EQ params (tk_{id}_mid_eq*/side_eq*) no
    // longer registered — pre-rack + post-rack EQs both live on the mixer
    // strip prefix (mixer_{kind}_<N>_preeq_* / _mid_eq*), registered in
    // ensureMixerStripParams.  Every track still gets a 6-slot effect rack.
    addParamsForEffectRack(prefix);
}

void VibeSynthProcessor::unregisterParamsForTrack(const juce::String& trackId)
{
    // JUCE APVTS has no removeParameter API — params stay in the tree but
    // we remove the trackId from our registry so isTrackRegistered() returns false.
    // Params are reset to default so stale automation data doesn't affect the next
    // engine loaded on the same trackId.
    auto it = mRegisteredTrackParams.find(trackId);
    if (it == mRegisteredTrackParams.end()) return;

    for (const auto& paramId : it->second)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(paramId)))
            p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(p->getDefaultValue()));
    }

    mRegisteredTrackParams.erase(it);
}

bool VibeSynthProcessor::isTrackRegistered(const juce::String& trackId) const
{
    return mRegisteredTrackParams.count(trackId) > 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  5F-4a: Mixer-strip lazy APVTS registration
// ═══════════════════════════════════════════════════════════════════════════════

void VibeSynthProcessor::addParamsForMixerStrip(const juce::String& prefix,
                                                 MixerStripKind kind,
                                                 int defaultSendTo)
{
    // Helper lambdas scoped to this call — params are pushed directly via
    // apvts.createAndAddParameter (same pattern as existing lazy registration).
    auto addF = [&](const juce::String& id, const juce::String& name,
                    float lo, float hi, float def)
    {
        apvts.createAndAddParameter(std::make_unique<juce::AudioParameterFloat>(
            VID(id), name, juce::NormalisableRange<float>(lo, hi), def));
    };
    auto addB = [&](const juce::String& id, const juce::String& name, bool def)
    {
        apvts.createAndAddParameter(std::make_unique<juce::AudioParameterBool>(
            VID(id), name, def));
    };

    // Universally present on every strip type:
    // 2026-04-30: max bumped down 10 → 5.6 dB to match the fader cap's
    // visual range (kFaderMax in MixerTrackStrip + kFMax in VibeLAF's
    // drawLinearSlider were both changed at meter-rebuild time, but this
    // APVTS range wasn't — and SliderAttachment auto-overrides the
    // slider's setRange to match the param's range, so the cap travelled
    // -60..+10 while the dB-mark column was drawn for -60..+5.6.  Net
    // effect: ~4 dB visual offset between cap position and the labelled
    // marks (e.g. cap at 0 dB sat next to the -4 mark).  Range now matches.
    addF(prefix + "_level", prefix + " Level", -60.f, 5.6f, 0.f);  // dB
    addF(prefix + "_pan",   prefix + " Pan",    -1.f,  1.f, 0.f);
    addF(prefix + "_width", prefix + " Width",   0.f,  2.f, 1.f);

    if (kind == MixerStripKind::Bus || kind == MixerStripKind::Insert)
    {
        addB(prefix + "_mute",     prefix + " Mute",     false);
        addB(prefix + "_solo",     prefix + " Solo",     false);
        addB(prefix + "_polarity", prefix + " Polarity", false);
    }
    else if (kind == MixerStripKind::Master)
    {
        // 2026-04-29: Master strip needs its mute param registered so the
        // strip's M button can attach (was previously visually toggling but
        // not bound to APVTS at all → MasterBusNode read a null pointer →
        // master mute did nothing).  Solo + polarity are intentionally
        // omitted — master has no peer to solo against and polarity at the
        // master is rarely useful (and would invert ALL output, easy to
        // mistake for "broken").
        addB(prefix + "_mute", prefix + " Mute", false);
    }

    // FX Bypass on ALL strip types (master/bus/insert) — each has its own rack.
    addB(prefix + "_bypass", prefix + " FX Bypass", false);

    if (kind == MixerStripKind::Insert)
    {
        addB(prefix + "_arm", prefix + " Arm", false);
    }

    // 5F-4b B1a: routing params — main-out + up to 4 sends
    auto addI = [&](const juce::String& id, const juce::String& name,
                    int lo, int hi, int def)
    {
        apvts.createAndAddParameter(std::make_unique<juce::AudioParameterInt>(
            VID(id), name, lo, hi, def));
    };

    // Main-out: covers every reserved channel id (0..999). Default = natural parent.
    addI(prefix + "_sendTo", prefix + " Send-To", 0, 999, defaultSendTo);

    // Sends 0..3: -1 = inactive, amount in dB (-60..+6), pre/post toggle.
    for (int s = 0; s < 4; ++s)
    {
        const juce::String sp = prefix + "_send" + juce::String(s);
        addI(sp + "_to",      prefix + " Send" + juce::String(s) + " To",      -1, 999, -1);
        addF(sp + "_amount",  prefix + " Send" + juce::String(s) + " Amount",  -60.f, 6.f, 0.f);
        addB(sp + "_prepost", prefix + " Send" + juce::String(s) + " PrePost", false);
    }

    // D3: choke group — 0 = none, 1..16 = group id.  When two inserts share a
    // group > 0, a noteOn (or audio-clip start) on one chokes all others in
    // the same group.  Inserts only — buses/master have no concept of voices
    // to choke.
    if (kind == MixerStripKind::Insert)
        addI(prefix + "_chokeGroup", prefix + " Choke Group", 0, 16, 0);
}

bool VibeSynthProcessor::ensureMixerStripParams(const juce::String& prefix,
                                                 MixerStripKind kind,
                                                 int defaultSendTo)
{
    if (mRegisteredMixerStrips.count(prefix) > 0)
        return false;

    addParamsForMixerStrip(prefix, kind, defaultSendTo);
    // Session B: every mixer strip also gets a full EQ band param set under the
    // same prefix (prefix + "_mid_eq" + b + suffix / "_side_eq" + b + suffix),
    // so the post-rack EQ on this strip is automatable. Idempotent via the
    // mRegisteredTrackParams guard inside addParamsForTrackEQ's dynF/I/B helpers.
    addParamsForTrackEQ(prefix);
    // §P4.3: every strip ALSO gets a pre-rack EQ block under
    // prefix + "_preeq_mid_eq{b}*" / "_preeq_side_eq{b}*".  Same idempotent
    // guard.  Bus/insert preEq DSPs (added in B2) bind to these params via
    // updateAllPreRackEQsFromApvts (B4).
    addParamsForTrackPreEQ(prefix);
    mRegisteredMixerStrips.insert(prefix);
    return true;
}

void VibeSynthProcessor::ensureMixerBusAndMasterParams()
{
    using namespace MixerChannelIds;
    ensureMixerStripParams("mixer_master",   MixerStripKind::Master, kOutput);
    ensureMixerStripParams("mixer_layers",   MixerStripKind::Bus,    kMaster);
    ensureMixerStripParams("mixer_bass",     MixerStripKind::Bus,    kMaster);
    ensureMixerStripParams("mixer_drums",    MixerStripKind::Bus,    kMaster);
    ensureMixerStripParams("mixer_fx",       MixerStripKind::Bus,    kMaster);
    ensureMixerStripParams("mixer_clipsbus", MixerStripKind::Bus,    kMaster);
    // R3.5 (2026-04-23): Vox + Inst buses — same shape as Clips/FX bus.
    ensureMixerStripParams("mixer_voxbus",   MixerStripKind::Bus,    kMaster);
    ensureMixerStripParams("mixer_instbus",  MixerStripKind::Bus,    kMaster);
    // G-6 (2026-04-29): secondary Vox/Inst buses — always register params so
    // routing + audio paths work regardless of UI activation state.  Strip
    // visibility on Mixer is a separate flag (see MixerPage::activate*Bus2/3).
    ensureMixerStripParams("mixer_voxbus2",  MixerStripKind::Bus,    kMaster);
    ensureMixerStripParams("mixer_instbus2", MixerStripKind::Bus,    kMaster);
    ensureMixerStripParams("mixer_instbus3", MixerStripKind::Bus,    kMaster);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  5F-4a: Audio-row mixer strip registration
// ═══════════════════════════════════════════════════════════════════════════════

void VibeSynthProcessor::ensureAudioInsert(int row, const juce::String& displayName)
{
    if (row < 0 || row >= kMaxAudioRows) return;

    const juce::String prefix = "mixer_audio_" + juce::String(row);
    ensureMixerStripParams(prefix, MixerStripKind::Insert, MixerChannelIds::kClipsBus);
    mVibeGraph.ensureInsertNode(VibeGraph::InsertKind::Audio, row,
                                 displayName.isNotEmpty() ? displayName
                                     : ("Audio " + juce::String(row + 1)),
                                 prefix);
}

// 5F-4b B2: Aux strip registration (receive-only, default routes to Master).
void VibeSynthProcessor::ensureAuxInsert(int idx, const juce::String& displayName)
{
    if (idx < 0 || idx >= MixerChannelIds::kMaxAuxStrips) return;   // matches MixerChannelIds aux range

    const juce::String prefix = "mixer_aux_" + juce::String(idx);
    ensureMixerStripParams(prefix, MixerStripKind::Insert, MixerChannelIds::kFxBus);
    mVibeGraph.ensureInsertNode(VibeGraph::InsertKind::Aux, idx,
                                 displayName.isNotEmpty() ? displayName
                                     : ("Aux " + juce::String(idx + 1)),
                                 prefix);
}

// R1 (2026-04-23): Vox / Inst strip registration.  Same pattern as Aux but
// each kind has its own bus parent (VoxBus / InstBus) instead of FxBus.
// R2 adds the ASIO input-channel APVTS param at the same registration site.
void VibeSynthProcessor::ensureVoxInsert(int idx, const juce::String& displayName)
{
    if (idx < 0 || idx >= MixerChannelIds::kMaxVoxStrips) return;
    const juce::String prefix = "mixer_vox_" + juce::String(idx);
    ensureMixerStripParams(prefix, MixerStripKind::Insert, MixerChannelIds::kVoxBus);
    addLiveInputParams (prefix);
    mVibeGraph.ensureInsertNode(VibeGraph::InsertKind::Vox, idx,
                                 displayName.isNotEmpty() ? displayName
                                     : ("Vox " + juce::String(idx + 1)),
                                 prefix);
}

void VibeSynthProcessor::ensureInstInsert(int idx, const juce::String& displayName)
{
    if (idx < 0 || idx >= MixerChannelIds::kMaxInstStrips) return;
    const juce::String prefix = "mixer_inst_" + juce::String(idx);
    ensureMixerStripParams(prefix, MixerStripKind::Insert, MixerChannelIds::kInstBus);
    addLiveInputParams (prefix);
    mVibeGraph.ensureInsertNode(VibeGraph::InsertKind::Inst, idx,
                                 displayName.isNotEmpty() ? displayName
                                     : ("Inst " + juce::String(idx + 1)),
                                 prefix);
}

// R2 (2026-04-23): Vox / Inst-only APVTS params.  Lazy-registered alongside
// the standard mixer-strip params for live-input strip types.
//   _inputChannelIdx  Int  -1..127  default -1  (no input assigned)
// Channel name is stored as a non-APVTS property on apvts.state (see
// setInputChannelName / getInputChannelName below) since APVTS only handles
// numeric ranged params.  Both round-trip via getStateInformation /
// serializeProject (apvts.state is a juce::ValueTree that copies all attrs).
void VibeSynthProcessor::addLiveInputParams (const juce::String& prefix)
{
    auto& ids = mRegisteredMixerStrips.count (prefix) > 0
                  ? mRegisteredTrackParams[prefix]   // unused entry safety
                  : mRegisteredTrackParams[prefix];
    juce::ignoreUnused (ids);
    // Use raw createAndAddParameter; idempotent (APVTS skips duplicates).
    if (apvts.getParameter (prefix + "_inputChannelIdx") == nullptr)
        apvts.createAndAddParameter (std::make_unique<juce::AudioParameterInt> (
            VID(prefix + "_inputChannelIdx"),
            prefix + " Input Channel Idx", -1, 127, -1));
    // R4 (2026-04-23): Listen toggle (audible monitor).  When ON, the strip's
    // processed audio is routed to its bus + master so the user hears
    // themselves.  When OFF, the strip still processes (rack/EQ/peak meter
    // animate) and recording can still happen, but the audio is silenced
    // before it leaves the strip - prevents painful feedback when the user
    // arms a mic while wearing speakers.
    if (apvts.getParameter (prefix + "_listen") == nullptr)
        apvts.createAndAddParameter (std::make_unique<juce::AudioParameterBool> (
            VID(prefix + "_listen"),
            prefix + " Listen", false));
}

void VibeSynthProcessor::setInputChannelName (const juce::String& stripPrefix,
                                                const juce::String& name)
{
    juce::ScopedLock sl (mInputChannelNamesLock);
    apvts.state.setProperty (juce::Identifier (stripPrefix + "_inputChannelName"),
                              name, nullptr);
}

juce::String VibeSynthProcessor::getInputChannelName (const juce::String& stripPrefix) const
{
    juce::ScopedLock sl (mInputChannelNamesLock);
    return apvts.state.getProperty (juce::Identifier (stripPrefix + "_inputChannelName"),
                                     juce::String()).toString();
}
