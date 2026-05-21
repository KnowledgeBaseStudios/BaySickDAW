#include "PluginProcessor.h"
// 2026-04-25: BaySickDrumsProcessor include removed - class deleted.
#include "BaySickSynth/BaySickSynthProcessor.h"   // D1.4-fix (c): drum transpose compensation
#include "BaySickRustyDrums/BaySickRustyDrumsProcessor.h"  // J-5: singleton sfizz drum-kit engine
#include "BaySickGuitars/BaySickGuitarsProcessor.h"        // K-2: per-instance sfizz guitar engines
#include "BaySickBasses/BaySickBassesProcessor.h"          // L-2: per-instance sfizz bass engines
#include "VibePlayer/VibePlayerProcessor.h"       // D1.4-fix (c): drum tune compensation
#include "BaySickVocal/BaySickVocalProcessor.h"   // I-16 G-9: wet recorder hand-off
#include "DSP/EngineSidechainHelper.h"            // C.4 Phase 2.2: ISidechainEngine for engine-level SC push
#include <thread>                                 // 2026-05-06: hardware_concurrency for render worker count
#ifdef VIBESYNTH_VST
  #include "PluginEditor.h"
#endif

// 2026-04-30 (audit C11+C12): emit a noteOn with the per-note panning +
// fine-pitch values carried as standard MIDI CC10 + PitchWheel.  Was the
// missing half of the piano roll's Panning + Pitch Bend control lanes -
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
    // ±100 cents, mapped to ±4096 from center - i.e. ±50 % of a typical
    // ±2-semitone synth pitch-bend range = ±1 semitone in the synth's
    // tuning.  Synths with non-default pitch-bend ranges will scale.
    if (std::abs (note.finePitch) > 0.005f)
    {
        const int wheel = juce::jlimit (0, 16383,
            (int) std::round (8192.f + note.finePitch * 4096.f));
        dst.addEvent (juce::MidiMessage::pitchWheel (ch, wheel), samplePos);
    }
    // Batch E #2 (2026-05-01): per-note Filter Cutoff via standard CC 74
    // ("Brightness").  Engine voices listen for CC 74 and apply a +/-2-octave
    // offset on top of the master cutoff for the upcoming note.  0.5 = neutral,
    // 0 = full close (-2 oct), 1 = full open (+2 oct).
    if (std::abs (note.filterCutoff - 0.5f) > 0.005f)
    {
        const int cc = juce::jlimit (0, 127,
            (int) std::round (note.filterCutoff * 127.0f));
        dst.addEvent (juce::MidiMessage::controllerEvent (ch, 74, cc), samplePos);
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
    // Global "kill-all" - when true, every effects rack in the app is bypassed
    // regardless of its own _bypass state. Read by each bus/insert node per block.
    addB("master_fx_bypass", "Master FX Bypass", false);

    // 2026-04-29: project-level pan law (FL Studio parity).
    //   0 = Circular   (constant power, -3 dB at center, FL default)
    //   1 = Triangular (linear,         -6 dB at center)
    //   2 = Square     (0 dB at center, only attenuates the opposite side)
    // Read every audio block by each Insert/Bus/MasterBusNode when applying
    // the per-strip _pan param.  Default 0 matches FL's fresh-project default.
    addI("master_pan_law", "Pan Law", 0, 2, 0);

    // QA-Ea Task 0c (FL pre-roll record): global record-quantize divisor.
    //   0 = Off, 1 = 1/4, 2 = 1/8, 3 = 1/16, 4 = 1/32, 5 = 1/64.
    // Surfaced via "Global Record-Quantize" submenu in the Record-button
    // dropdown in GlobalTransportBar (alongside ASIO / MIDI mode toggles).
    // Read by commitRecordingResult's MIDI commit loop (StandaloneEditor)
    // to snap clamped startBeats to the grid divisor AFTER the FL
    // Early-Strike clamp.  Off = no snap (raw clamped startBeats kept).
    addI("record_quantize_div", "Record Quantize Divisor", 0, 5, 0);

    // §P4.3 B7 (2026-04-22): legacy bus-EQ param blocks removed.
    // Pre-rack Layers/Bass/Drums EQs are now per-strip on the InsertNode/BusNode
    // (mixer_{kind}_<i>_preeq_mid_eq* / _preeq_side_eq*, registered lazily via
    // addParamsForTrackPreEQ in ensureMixerStripParams).  Post-rack EQs live on
    // mixer_{kind}_<i>_mid_eq* / _side_eq*.  The legacy `drums_*_eq*` block + the
    // matching `tk_lay_*_mid_eq*` / `tk_bas_*_mid_eq*` lazy registrations + the
    // mDrumsEQDSP / mLayerPageEQs / mBassPageEQs DSP instances are all gone.

    return { params.begin(), params.end() };
}

// ── Multi-threaded render engine: pool sizing ────────────────────────────────
// Hardware concurrency minus one (leave a core for the OS audio thread to
// schedule on), capped at kMaxWorkers (8). Falls back to 4 on systems where
// hardware_concurrency reports 0.
int VibeSynthProcessor::computeRenderWorkerCount() noexcept
{
    const int hw      = (int) std::thread::hardware_concurrency();
    const int desired = hw > 0 ? juce::jmax (1, hw - 1) : 4;
    return juce::jmin (desired, RenderEngine::kMaxWorkers);
}

// ── Constructor / Destructor ──────────────────────────────────────────────────
VibeSynthProcessor::VibeSynthProcessor()
    : AudioProcessor(BusesProperties()
        // R3 (2026-04-23): declare an input bus so JUCE feeds the audio
        // device's input channels into the processBlock buffer.
        // J-A2 (2026-05-04): bumped 16 -> 64 to cover Tascam Model 24 (22 in),
        // Behringer X32 (32 in), Yamaha O1V (24 in), and other large interfaces.
        // JUCE clamps to the device's actual input count, so a 2-input USB
        // headset still gets 2 channels - only the upper bound moved.
        .withInput ("Input",  juce::AudioChannelSet::discreteChannels(64), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "BaySickDAWState", createParameterLayout())
{
    // Set up polyphonic synth -- 8 voices
    // QA-0a (2026-05-07): placeholder sample rate before addVoice (see
    // VibePlayerDSP::VibeSynth ctor for full reasoning).
    mSynth.setCurrentPlaybackSampleRate (44100.0);
    mSynth.addSound(new SynthSound());
    for (int i = 0; i < 8; ++i)
        mSynth.addVoice(new SynthVoice());

    mAudioFormatManager.registerBasicFormats();  // WAV, AIFF, MP3, OGG, FLAC

    for (int i = 0; i < kMaxAudioRows; ++i)
    {
        mAudioRowPeakDb [i].store(-60.0f, std::memory_order_relaxed);
        mAudioRowPeakDbL[i].store(-60.0f, std::memory_order_relaxed);
        mAudioRowPeakDbR[i].store(-60.0f, std::memory_order_relaxed);
        // 2026-05-02: running-max variants start at -inf so a "no audio
        // wrote this block" case skips promotion (snapshot keeps decaying).
        constexpr float kNI = -std::numeric_limits<float>::infinity();
        mAudioRowPeakDbRun [i].store(kNI, std::memory_order_relaxed);
        mAudioRowPeakDbLRun[i].store(kNI, std::memory_order_relaxed);
        mAudioRowPeakDbRRun[i].store(kNI, std::memory_order_relaxed);
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

    // 2026-05-06 (Batch 9c B1): bootstrap an empty AudioClipSnapshot at gen 0.
    // The audio thread's first load-acquire on mActiveAudioClips MUST see a
    // valid pointer (no null-checks in the iteration sites by design).
    auto* initialSnap = new AudioClipSnapshot();
    initialSnap->generation = 0;
    mActiveAudioClips.store (initialSnap, std::memory_order_release);
}

VibeSynthProcessor::~VibeSynthProcessor()
{
    apvts.state.removeListener(this);
    mAudioFileThread.stopThread (500);

    // 2026-05-06 (Batch 9c B1): the atomic doesn't own the active snapshot;
    // delete here so the AudioClipStreamer destructors (and their bg-thread
    // unregister calls) run before mAudioFileThread is fully gone.  By the
    // time this runs, the audio thread is no longer dispatching processBlock
    // (~StandaloneEditor's closeAllDynamicTabs barrier + JUCE's standard
    // plugin-shutdown ordering ensure that), so destroying on this thread
    // is safe and avoids routing through the retirement queue during
    // teardown (mClipRetirement is itself about to be destroyed via member
    // destruction; doing one final retire here would be a sequencing race
    // with that).
    if (auto* lastRaw = mActiveAudioClips.exchange (nullptr,
                                                     std::memory_order_acquire))
        delete lastRaw;
}

// ── Preparation ───────────────────────────────────────────────────────────────
void VibeSynthProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mSampleRate = sampleRate;
    mBlockSize  = samplesPerBlock;

    mSynth.setCurrentPlaybackSampleRate(sampleRate);
    mBassSynth.prepare(sampleRate, samplesPerBlock);

    // C.3 (2026-04-30): reset MIDI input collector for the current SR.  Without
    // this, removeNextBlockOfMessages can't compute correct sample offsets.
    mLiveMidiCollector.reset (sampleRate);

    // Pre-allocate engine scratch buffers to avoid audio-thread allocation
    mLayerEngineSum    .setSize(2, samplesPerBlock, false, true, false);
    mLayerEngineScratch.setSize(2, samplesPerBlock, false, true, false);
    mBassEngineBuf     .setSize(2, samplesPerBlock, false, true, false);
    mBassEngineScratch .setSize(2, samplesPerBlock, false, true, false);
    mAudioRowScratch   .setSize(2, samplesPerBlock, false, true, false);
    mAudioClipScratch  .setSize(2, samplesPerBlock, false, true, false);
    // J-7a (2026-05-03): BaySickRustyDrums singleton scratch.
    mRustyDrumsScratch .setSize(2, samplesPerBlock, false, true, false);
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
    // J-7a (2026-05-03): re-prepare the BaySickRustyDrums singleton on
    // host SR / block-size change.
    {
        juce::SpinLock::ScopedLockType lk(mRustyDrumsEngineLock);
        if (mRustyDrumsEngine) mRustyDrumsEngine->prepareToPlay(sampleRate, samplesPerBlock);
    }
    mVibeGraph.prepare(sampleRate, samplesPerBlock);

    // Build the fixed bus topology the first time; no-op on subsequent calls.
    // §P4.3 B7: drumsEQ ref removed - DrumsBusNode now uses its own preEq member
    // (sync'd via updateAllPreRackEQsFromApvts from mixer_drumsbus_preeq_*).
    mVibeGraph.buildFixedTopology(mSynth, mBassSynth, apvts);

    // 5F-4a: register master + 5 bus strip params (idempotent).
    ensureMixerBusAndMasterParams();
    // 5F-4a Batch 6: cache APVTS pointers in bus + master nodes (needs params registered).
    mVibeGraph.rebindBusApvts();

    // 2026-05-06 (Batch 9b): register peak-meter atomic refs for the buses
    // whose DSP migrated into VibeGraph::processBus.  Layers/Bass/Drums/
    // Master/FxBus carry their own peak atomics on their BusNode and don't
    // need registration.  Idempotent - registerBusPeakAtomics overwrites the
    // table entry, so repeated prepareToPlay calls (block-size / sample-rate
    // changes) just rewrite the same pointers.
    mVibeGraph.registerBusPeakAtomics(MixerChannelIds::kClipsBus,
                                       &mAudioClipsBusPeakDbRun,
                                       &mAudioClipsBusPeakDbLRun,
                                       &mAudioClipsBusPeakDbRRun);
    mVibeGraph.registerBusPeakAtomics(MixerChannelIds::kVoxBus,
                                       &mVoxBusPeakDbRun,
                                       &mVoxBusPeakDbLRun,
                                       &mVoxBusPeakDbRRun);
    mVibeGraph.registerBusPeakAtomics(MixerChannelIds::kInstBus,
                                       &mInstBusPeakDbRun,
                                       &mInstBusPeakDbLRun,
                                       &mInstBusPeakDbRRun);
    mVibeGraph.registerBusPeakAtomics(MixerChannelIds::kVoxBus2,
                                       &mVoxBus2PeakDbRun,
                                       &mVoxBus2PeakDbLRun,
                                       &mVoxBus2PeakDbRRun);
    mVibeGraph.registerBusPeakAtomics(MixerChannelIds::kInstBus2,
                                       &mInstBus2PeakDbRun,
                                       &mInstBus2PeakDbLRun,
                                       &mInstBus2PeakDbRRun);
    mVibeGraph.registerBusPeakAtomics(MixerChannelIds::kInstBus3,
                                       &mInstBus3PeakDbRun,
                                       &mInstBus3PeakDbLRun,
                                       &mInstBus3PeakDbRRun);
    mVibeGraph.registerBusPeakAtomics(MixerChannelIds::kRustyDrumsBus,
                                       &mRustyDrumsBusPeakDbRun,
                                       &mRustyDrumsBusPeakDbLRun,
                                       &mRustyDrumsBusPeakDbRRun);

    // 2026-05-05 dirty-flag wiring: route every VibeGraph rack's lifecycle
    // events into the editor's project-dirty hook the same way main-APVTS
    // edits do.  Effects-page slot type swap / move-up/down / clear / bypass
    // doesn't write apvts, so without this rack lifecycle slips past the
    // dirty listener.
    mVibeGraph.onAnyRackChanged = [this]
    {
        if (onAnyStateChange) onAnyStateChange();
    };
    mVibeGraph.rebindAllRackHooks();

    // Compute initial PDC (0 for all current effects) and report to host.
    setLatencySamples(mVibeGraph.updateBusLatencies());

    // ── Multi-threaded render engine (Phase 1 scaffolding, 2026-05-06) ───────
    // Resize the cache-aligned arena to fit the new block size and clear any
    // stale tasks from the pool's queues. The pool itself is NOT recreated -
    // workers persist for the plugin's lifetime per the lifetime contract.
    // Sample-rate / block-size changes only touch the arena views.
    mRenderArena.prepare (samplesPerBlock);
    mRenderDispatcher.prepare (sampleRate, samplesPerBlock);
    mRenderPool.clearQueues();

    // Batch 7 (2026-05-06): register the 11 always-on bus PassiveStripTasks
    // here (after buildFixedTopology so VibeGraph bus nodes exist).
    // Idempotent - guarded by null checks so prepareToPlay can be called
    // repeatedly (sample-rate / buffer changes).  Master is excluded; it
    // gets its own MasterTask in Batch 8.
    static constexpr std::array<int, kNumBatch7Buses> kBusChannelIds = {
        MixerChannelIds::kLayersBus,
        MixerChannelIds::kBassBus,
        MixerChannelIds::kDrumsBus,
        MixerChannelIds::kFxBus,
        MixerChannelIds::kClipsBus,
        MixerChannelIds::kVoxBus,
        MixerChannelIds::kInstBus,
        MixerChannelIds::kVoxBus2,
        MixerChannelIds::kInstBus2,
        MixerChannelIds::kInstBus3,
        MixerChannelIds::kRustyDrumsBus,
    };
    for (size_t i = 0; i < kBusChannelIds.size(); ++i)
    {
        if (mBusRenderTasks[i]) continue;   // already registered
        auto task = std::make_unique<PassiveStripTask>(
            PassiveStripTask::Kind::Bus, /*auxOrBusIndex*/ 0,
            kBusChannelIds[i], mVibeGraph, *this);
        mRenderDispatcher.registerTask(task.get());
        mBusRenderTasks[i] = std::move(task);
    }

    // Batch 8 (2026-05-06): register the always-on MasterTask.  Idempotent
    // - guarded by null check.  Must be registered AFTER the bus tasks
    // (above) so rebuildLinks finds the buses as predecessors of master.
    if (! mMasterRenderTask)
    {
        mMasterRenderTask = std::make_unique<MasterTask>(
            mVibeGraph, *this, mRenderDispatcher.getAllDoneFlag());
        mRenderDispatcher.registerTask(mMasterRenderTask.get());
    }
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

// ── Batch 5 (2026-05-06): routeInsertOutput member function ──────────────────
// Promoted from a stack lambda inside processBlock so helpers + render tasks
// outside that scope can call it.  Behavior is identical to the previous
// lambda - fan source's output buffer to every destination in the routing
// graph (main-out + sends), plus copy into any sidechain receive slots.
void VibeSynthProcessor::routeInsertOutput (int srcChannelId,
                                             const juce::AudioBuffer<float>& buf,
                                             int n)
{
    const auto& routingEdges   = mVibeGraph.getRoutingGraph().edges();
    const auto& routingScEdges = mVibeGraph.getRoutingGraph().scEdges();

    const int nc = juce::jmin (2, buf.getNumChannels());
    for (const auto& e : routingEdges)
    {
        if (e.srcId != srcChannelId) continue;
        if (auto* dst = mVibeGraph.getChannelAccumulator (e.dstId))
        {
            const float gain = e.isMainOut
                ? 1.f
                : juce::Decibels::decibelsToGain (e.amountDb, -60.f);
            for (int c = 0; c < nc; ++c)
                dst->addFrom (c, 0, buf, c, 0, n, gain);
        }
    }
    // C.4 Phase 1: SC fan -- copy (not add) src's tap into dst's SC receive
    // slot.  Per the encoding contract there is at most one source per
    // (dst, slot), so copy-replaces is correct.  Tap point = post-everything
    // per Q4=A: by the time routeInsertOutput is called for a strip, its
    // full pipeline (rack -> EQ -> fader -> mute -> solo -> pan) has
    // already run on `buf`.
    for (const auto& sce : routingScEdges)
    {
        if (sce.srcId != srcChannelId) continue;
        if (auto* recv = mVibeGraph.getScRecvBuffer (sce.dstId, sce.dstSlot))
        {
            const int rnc = juce::jmin (nc, recv->getNumChannels());
            for (int c = 0; c < rnc; ++c)
                recv->copyFrom (c, 0, buf, c, 0, n);
        }
    }
}

// ── Batch 5 (2026-05-06): renderAudioClipsForRow ─────────────────────────────
// Decode + insert-chain processing for every NON-FilePlay audio clip whose
// trackRow matches `row`.  Behavior is identical to the previous in-line
// loop body in processBlock; the extraction lets AudioInsertTask call the
// same code path.
//
// mtDest semantics:
//   nullptr  : serial mode -- final routeInsertOutput call fans the clip's
//              processed output via the routing graph (main-out + sends).
//   non-null : MT mode (Batch 9 will flip the flag) -- per-clip output is
//              added into *mtDest so the task's downstream pull-model
//              consumers see the summed row output.
//
// FilePlay clips (clip with routeChannel pointing at a Vox/Inst page) are
// silently skipped here; the inline FilePlay pass in processBlock owns
// that path.  See deferred notes in the Batch 5 entry of the recovery doc.
void VibeSynthProcessor::renderAudioClipsForRow (int row,
                                                  const AudioClipBlockContext& ctx,
                                                  juce::AudioBuffer<float>* mtDest)
{
    if (row < 0 || row >= kMaxAudioRows)
        return;
    if (mPatternManager == nullptr || ctx.mxState == nullptr || ctx.clipScratch == nullptr)
        return;

    const auto& mx = *ctx.mxState;
    auto& clipScratch = *ctx.clipScratch;

    auto arCasMax = [] (std::atomic<float>& a, float v) noexcept
    {
        if (v == -std::numeric_limits<float>::infinity()) return;
        float cur = a.load (std::memory_order_relaxed);
        while (cur < v
               && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
    };

    // 2026-05-06 (Batch 9c B1): iterate the audio-thread snapshot captured
    // at the top of processBlock.  Serial Pass 2 + AudioInsertTask::run
    // (MT) both call this helper after the audio thread published
    // mCurrentBlockClipSnapshot, so the players[] vector is guaranteed
    // alive for the duration of this block via the RetirementQueue ack
    // protocol -- no per-site lock needed.
    for (auto& player : mCurrentBlockClipSnapshot->players)
    {
        if (player.streamer == nullptr) continue;
        if (player.trackRow != row) continue;

        // FilePlay clips are handled by the inline pass in processBlock.
        const int routeCh = player.routeChannel;
        const bool isVoxRoute  = routeCh >= MixerChannelIds::kVoxBase
                              && routeCh <  MixerChannelIds::kVoxBase + kMaxVoxPages;
        const bool isInstRoute = routeCh >= MixerChannelIds::kInstBase
                              && routeCh <  MixerChannelIds::kInstBase + kMaxInstPages;
        if (isVoxRoute || isInstRoute)
            continue;

        const juce::int64 clipStart = (juce::int64) (player.clipStartBeat * ctx.secPerBeat * mSampleRate);
        const juce::int64 clipEnd   = (juce::int64) (player.clipEndBeat   * ctx.secPerBeat * mSampleRate);

        if (ctx.projectEnd <= clipStart || ctx.projectStart >= clipEnd) continue;

        const bool rowMuted        = mx.audioRowMute[(size_t) row];
        const bool builderRowMuted = ! mPatternManager->isRowAudible (row);

        if (player.mutedByChoke)
        {
            mAudioRowPeakDbRun[row].store (-60.0f, std::memory_order_relaxed);
            continue;
        }
        if (rowMuted || builderRowMuted)
        {
            mAudioRowPeakDbRun[row].store (-60.0f, std::memory_order_relaxed);
            continue;
        }

        const int bufOffset = (int) juce::jmax ((juce::int64) 0, clipStart - ctx.projectStart);
        const juce::int64 outPosInClip = (ctx.projectStart + bufOffset) - clipStart;

        const double readRatio = player.fileSampleRate / mSampleRate;
        // QA-Ea Task 0c (FL pre-roll record): shift file reads by the
        // content-start offset so the clip plays from sample N of the file
        // rather than 0.  Zero default preserves every pre-Task-0c clip.
        // Rule-4 defensive floor (belt+suspenders against a UI clamp miss):
        // contentStartSamples is clamped >= 0 at the UI layer (slip-edit
        // mouseDrag in BuilderPage.cpp -- Option A: no dead space on either
        // edge in Slip mode); the floor here protects the streamer seek
        // against the unlikely case of a stale / corrupt project value.
        const juce::int64 contentStart = juce::jmax ((juce::int64) 0,
                                                     player.contentStartSamples);
        const juce::int64 filePos = (juce::int64) (outPosInClip * readRatio)
                                    + contentStart;

        const juce::int64 fileTotalSamples = player.streamer->getTotalLength();
        // EOF guard: clip extends past file end -> output silence.
        // filePos < 0 is unreachable post-Rule-4 floor (outPosInClip * readRatio
        // is always >= 0 and contentStart is floored >= 0), so a single >=
        // check is sufficient.
        if (filePos >= fileTotalSamples)
        {
            mAudioRowPeakDbRun[row].store (-60.0f, std::memory_order_relaxed);
            continue;
        }

        const double stretchRatio = (player.vocoder != nullptr
                                     && player.stretchMode
                                     && player.originalBPM > 0.f)
            ? (double) player.originalBPM / ctx.bpm
            : 1.0;

        // QA-Ea Task 0c: playable file length reduced by contentStart (the
        // clip's first playable file frame is contentStart, not 0).
        const juce::int64 fileEOFOutput = clipStart
            + (juce::int64) ((double) (fileTotalSamples - contentStart)
                             * stretchRatio / readRatio);
        const juce::int64 effectiveClipEnd = juce::jmin (clipEnd, fileEOFOutput);
        const int outSamples = (int) juce::jmin (
            (juce::int64) (ctx.numSamples - bufOffset),
            effectiveClipEnd - (ctx.projectStart + bufOffset));

        if (outSamples <= 0) continue;

        clipScratch.clear();

        const float gain = ctx.masterGain;
        float       peak = 0.0f;

        const bool usePV = (player.vocoder != nullptr)
                        && player.stretchMode
                        && (player.originalBPM > 0.f)
                        && (std::abs (ctx.bpm - player.originalBPM) > 0.01);

        if (usePV)
        {
            player.vocoder->setStretchRatio (stretchRatio);

            // QA-Ea Task 0c: stretch-aware file reference includes the
            // content-start offset.  player.expectedFilePos / streamer->seek
            // track the absolute file frame so subsequent reads stay aligned.
            const juce::int64 pvRefPos  = (juce::int64) ((double) outPosInClip
                                                          * readRatio / stretchRatio)
                                          + contentStart;
            const juce::int64 pvReadPos = player.expectedFilePos;

            const bool seekNeeded =
                (pvReadPos == 0 && pvRefPos > (juce::int64) mSampleRate) ||
                (pvReadPos  > 0 &&
                 std::abs (pvRefPos - pvReadPos) > (juce::int64) (mSampleRate * 2));

            if (seekNeeded)
            {
                player.vocoder->reset();
                player.streamer->seek (pvRefPos);
                player.expectedFilePos = pvRefPos;
            }

            const int numFileSamples = (int) std::ceil (
                (double) outSamples * readRatio / stretchRatio);

            player.pvInBuf.clear();
            const bool gotRaw = player.streamer->readRaw (
                player.pvInBuf, 0, numFileSamples, player.expectedFilePos);

            if (gotRaw)
            {
                player.expectedFilePos += numFileSamples;
                player.vocoder->push (player.pvInBuf, 0, numFileSamples);

                const int numVocOut = (int) std::ceil (
                    (double) outSamples * readRatio) + 2;

                player.pvOutBuf.clear();
                const int pulled = player.vocoder->pull (
                    player.pvOutBuf, 0, numVocOut);

                if (pulled > 0)
                {
                    const int pvCh = player.pvOutBuf.getNumChannels();
                    for (int i = 0; i < outSamples; ++i)
                    {
                        const double exactFP = (double) i * readRatio;
                        const int    ip      = (int) exactFP;
                        const float  frac    = (float) (exactFP - ip);

                        if (ip + 1 >= pulled) break;

                        for (int ch = 0; ch < ctx.numOut; ++ch)
                        {
                            const int   srcCh = ch % pvCh;
                            const float s0    = player.pvOutBuf.getSample (srcCh, ip);
                            const float s1    = player.pvOutBuf.getSample (srcCh, ip + 1);
                            const float v     = (s0 + frac * (s1 - s0)) * gain;
                            clipScratch.addSample (ch, bufOffset + i, v);
                            peak = juce::jmax (peak, std::abs (v));
                        }
                    }
                }
            }
        }
        else
        {
            peak = player.streamer->readAndMix (
                clipScratch, bufOffset, outSamples, filePos, readRatio, ctx.numOut, gain);
            player.expectedFilePos = filePos + (juce::int64) std::ceil (outSamples * readRatio);
        }

        // F3 declick: 5 ms linear fade-in / -out, capped at half clip length.
        {
            const juce::int64 clipLenOutSamples = effectiveClipEnd - clipStart;
            const int fadeSamples = juce::jmax (1, juce::jmin (
                (int) std::round (mSampleRate * 0.005),
                (int) (clipLenOutSamples / 2)));
            for (int s = 0; s < outSamples; ++s)
            {
                const juce::int64 absPos = outPosInClip + s;
                float g = 1.0f;
                if (absPos < (juce::int64) fadeSamples)
                    g = (float) absPos / (float) fadeSamples;
                const juce::int64 distFromEnd = clipLenOutSamples - 1 - absPos;
                if (distFromEnd >= 0 && distFromEnd < (juce::int64) fadeSamples)
                    g = juce::jmin (g, (float) distFromEnd / (float) fadeSamples);
                if (g < 1.0f)
                    for (int ch = 0; ch < ctx.numOut; ++ch)
                        clipScratch.setSample (ch, bufOffset + s,
                            clipScratch.getSample (ch, bufOffset + s) * g);
            }
        }

        // 5F-4a Batch 6: route per-clip scratch through Audio InsertNode
        // (polarity → width → rack → post-rack EQ → fader × mute × solo → PDC → peak).
        mVibeGraph.processInsert (VibeGraph::InsertKind::Audio, row,
                                   clipScratch, ctx.bpm, ctx.anySolo);

        // 2026-05-02: drain-and-merge - exchange the InsertNode's L/R peaks
        // and CAS-max into the audio-row mirror.
        const auto [pkL, pkR] = mVibeGraph.drainInsertPeakDbStereo (
            VibeGraph::InsertKind::Audio, row);
        arCasMax (mAudioRowPeakDbLRun[row], pkL);
        arCasMax (mAudioRowPeakDbRRun[row], pkR);
        arCasMax (mAudioRowPeakDbRun [row], juce::jmax (pkL, pkR));

        // Publish: serial fans via routing graph; MT writes into the task's
        // downstream-pull buffer (additive so multiple clips on the row sum).
        if (mtDest == nullptr)
        {
            routeInsertOutput (MixerChannelIds::audioInsert (row),
                                clipScratch, ctx.numSamples);
        }
        else
        {
            const int nc = juce::jmin (mtDest->getNumChannels(),
                                       clipScratch.getNumChannels());
            for (int c = 0; c < nc; ++c)
                mtDest->addFrom (c, 0, clipScratch, c, 0, ctx.numSamples);
        }
    }
}

// ── Batch 9b Item 9 (2026-05-06): renderFilePlayPlayer ───────────────────────
// Decode + run the engine + insert + route for ONE FilePlay AudioClipPlayer.
// Used by both the serial Pass 1 loop in processBlock and (when MT flips) by
// VoxStripTask / InstStripTask FilePlay branches.  See header for invariants.
bool VibeSynthProcessor::renderFilePlayPlayer (AudioClipPlayer&             player,
                                                 const AudioClipBlockContext& ctx,
                                                 juce::MidiBuffer&            engineMidi,
                                                 juce::AudioBuffer<float>*    mtDest,
                                                 juce::AudioBuffer<float>&    engineScratch)
{
    using int64 = juce::int64;

    // ── Preconditions ────────────────────────────────────────────────────────
    const int routeCh = player.routeChannel;
    const bool isVoxRoute  = routeCh >= MixerChannelIds::kVoxBase
                           && routeCh <  MixerChannelIds::kVoxBase + kMaxVoxPages;
    const bool isInstRoute = routeCh >= MixerChannelIds::kInstBase
                           && routeCh <  MixerChannelIds::kInstBase + kMaxInstPages;
    if (! isVoxRoute && ! isInstRoute) return false;
    if (player.streamer == nullptr)    return false;
    if (mPatternManager == nullptr)    return false;
    if (ctx.clipScratch == nullptr)    return false;
    if (ctx.mxState     == nullptr)    return false;

    const auto& mx = *ctx.mxState;
    auto& clipScratch = *ctx.clipScratch;
    const int   numSamples = ctx.numSamples;
    const int   numOut     = ctx.numOut;
    const double secPerBeat = ctx.secPerBeat;

    // ── Clip-range + mute/choke checks (mirrors inline serial Pass 1) ────────
    const int64 clipStart = (int64)(player.clipStartBeat * secPerBeat * mSampleRate);
    const int64 clipEnd   = (int64)(player.clipEndBeat   * secPerBeat * mSampleRate);
    if (ctx.projectEnd <= clipStart || ctx.projectStart >= clipEnd) return false;

    const int   row      = player.trackRow;
    const bool  inRange  = (row >= 0 && row < kMaxAudioRows);
    const bool  rowMuted = inRange && mx.audioRowMute[(size_t) row];
    const bool  builderRowMuted = ! mPatternManager->isRowAudible (row);

    if (player.mutedByChoke)
    {
        if (inRange) mAudioRowPeakDbRun[(size_t) row].store (-60.0f, std::memory_order_relaxed);
        return false;
    }
    if (rowMuted || builderRowMuted)
    {
        if (inRange) mAudioRowPeakDbRun[(size_t) row].store (-60.0f, std::memory_order_relaxed);
        return false;
    }

    // ── Decode params ────────────────────────────────────────────────────────
    const int   bufOffset    = (int) juce::jmax ((int64) 0, clipStart - ctx.projectStart);
    const int64 outPosInClip = (ctx.projectStart + bufOffset) - clipStart;
    const double readRatio   = player.fileSampleRate / mSampleRate;
    // QA-Ea Task 0c (FL pre-roll record): mirror of Site A direct-read
    // offset (Vox/Inst FilePlay).  Rule-4 defensive floor: UI clamps
    // contentStartSamples >= 0; floor here is belt+suspenders against a
    // stale / corrupt project value.
    const int64 contentStart = juce::jmax ((int64) 0, player.contentStartSamples);
    const int64 filePos      = (int64)(outPosInClip * readRatio)
                               + contentStart;

    const int64 fileTotalSamples = player.streamer->getTotalLength();
    // EOF guard: clip extends past file end -> skip.  filePos < 0 is
    // unreachable post-Rule-4 floor (mirror of Site A).
    if (filePos >= fileTotalSamples)
    {
        if (inRange) mAudioRowPeakDbRun[(size_t) row].store (-60.0f, std::memory_order_relaxed);
        return false;
    }

    const double stretchRatio = (player.vocoder != nullptr
                                 && player.stretchMode
                                 && player.originalBPM > 0.f)
        ? (double) player.originalBPM / ctx.bpm
        : 1.0;

    // QA-Ea Task 0c: mirror of Site A EOF reduction (Vox/Inst FilePlay).
    const int64 fileEOFOutput = clipStart
        + (int64) ((double) (fileTotalSamples - contentStart)
                   * stretchRatio / readRatio);
    const int64 effectiveClipEnd = juce::jmin (clipEnd, fileEOFOutput);
    const int outSamples = (int) juce::jmin (
        (int64)(numSamples - bufOffset),
        effectiveClipEnd - (ctx.projectStart + bufOffset));

    if (outSamples <= 0) return false;

    // ── Decode into ctx.clipScratch (Phase vocoder OR direct path) ───────────
    clipScratch.clear();

    const float gain = ctx.masterGain;
    float       peak = 0.0f;

    const bool usePV = (player.vocoder != nullptr)
                    && player.stretchMode
                    && (player.originalBPM > 0.f)
                    && (std::abs (ctx.bpm - player.originalBPM) > 0.01);

    if (usePV)
    {
        // ── Phase vocoder path (BPM stretch + pitch preservation) ────────────
        player.vocoder->setStretchRatio (stretchRatio);

        // QA-Ea Task 0c: mirror of Site A pvRefPos offset (Vox/Inst FilePlay).
        const int64 pvRefPos  = (int64) ((double) outPosInClip * readRatio / stretchRatio)
                                + contentStart;
        const int64 pvReadPos = player.expectedFilePos;

        const bool seekNeeded =
            (pvReadPos == 0 && pvRefPos > (int64) mSampleRate) ||
            (pvReadPos  > 0
             && std::abs (pvRefPos - pvReadPos) > (int64)(mSampleRate * 2));

        if (seekNeeded)
        {
            player.vocoder->reset();
            player.streamer->seek (pvRefPos);
            player.expectedFilePos = pvRefPos;
        }

        const int numFileSamples = (int) std::ceil (
            (double) outSamples * readRatio / stretchRatio);

        player.pvInBuf.clear();
        const bool gotRaw = player.streamer->readRaw (
            player.pvInBuf, 0, numFileSamples, player.expectedFilePos);

        if (gotRaw)
        {
            player.expectedFilePos += numFileSamples;
            player.vocoder->push (player.pvInBuf, 0, numFileSamples);

            const int numVocOut = (int) std::ceil ((double) outSamples * readRatio) + 2;
            player.pvOutBuf.clear();
            const int pulled = player.vocoder->pull (player.pvOutBuf, 0, numVocOut);

            if (pulled > 0)
            {
                const int pvCh = player.pvOutBuf.getNumChannels();
                for (int i = 0; i < outSamples; ++i)
                {
                    const double exactFP = (double) i * readRatio;
                    const int    ip      = (int) exactFP;
                    const float  frac    = (float)(exactFP - ip);

                    if (ip + 1 >= pulled) break;

                    for (int ch = 0; ch < numOut; ++ch)
                    {
                        const int   srcCh = ch % pvCh;
                        const float s0    = player.pvOutBuf.getSample (srcCh, ip);
                        const float s1    = player.pvOutBuf.getSample (srcCh, ip + 1);
                        const float v     = (s0 + frac * (s1 - s0)) * gain;
                        clipScratch.addSample (ch, bufOffset + i, v);
                        peak = juce::jmax (peak, std::abs (v));
                    }
                }
            }
        }
        // gotRaw false: expectedFilePos NOT advanced - retry next block.
    }
    else
    {
        // ── Direct path: SR-only interpolation (no BPM stretch) ──────────────
        peak = player.streamer->readAndMix (
            clipScratch, bufOffset, outSamples, filePos, readRatio, numOut, gain);
        player.expectedFilePos = filePos + (int64) std::ceil (outSamples * readRatio);
    }

    juce::ignoreUnused (peak);

    // ── F3: clip-edge declick (5 ms linear fade-in/out, capped at half-clip) ─
    {
        const int64 clipLenOutSamples = effectiveClipEnd - clipStart;
        const int fadeSamples = juce::jmax (1, juce::jmin (
            (int) std::round (mSampleRate * 0.005),
            (int) (clipLenOutSamples / 2)));
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
                    clipScratch.setSample (ch, bufOffset + s,
                        clipScratch.getSample (ch, bufOffset + s) * g);
        }
    }

    // ── Drive Vox/Inst engine + processInsert + route ────────────────────────
    // pushScToEngine is a stack lambda inside processBlock; inline its body
    // here via dynamic_cast to ISidechainEngine + setSidechainBuffers.
    auto pushScToEng = [this] (juce::AudioProcessor* eng, int channelId)
    {
        if (auto* sc = dynamic_cast<ISidechainEngine*> (eng))
        {
            const auto arr = mVibeGraph.getScRecvArray (channelId);
            juce::AudioBuffer<float>* bufs[VibeGraph::kMaxScRecvSlots];
            for (int s = 0; s < VibeGraph::kMaxScRecvSlots; ++s)
                bufs[s] = arr[(size_t) s];
            sc->setSidechainBuffers (bufs, VibeGraph::kMaxScRecvSlots);
        }
    };

    // numRenderCh matches processBlock's local: stereo render bus.
    constexpr int numRenderCh = 2;

    if (isVoxRoute)
    {
        const int vi = routeCh - MixerChannelIds::kVoxBase;
        auto* eng = mVoxEngines[(size_t) vi];
        if (eng == nullptr) return false;

        engineScratch.setSize (numRenderCh, numSamples, false, false, true);
        engineScratch.clear();
        const int copyCh = juce::jmin (numOut, engineScratch.getNumChannels());
        for (int ch = 0; ch < copyCh; ++ch)
            engineScratch.copyFrom (ch, 0, clipScratch, ch, 0, numSamples);

        // setForcePitchBypass(true) - realtime pitch was baked into the wet
        // recording at capture time, so don't double-apply on FilePlay.
        if (auto* vp = dynamic_cast<BaySickVocalProcessor*> (eng))
            vp->setForcePitchBypass (true);

        pushScToEng (eng, MixerChannelIds::voxInsert (vi));
        eng->processBlock (engineScratch, engineMidi);
        mVibeGraph.processInsert (VibeGraph::InsertKind::Vox, vi,
                                   engineScratch, ctx.bpm, ctx.anySolo);

        if (mtDest == nullptr)
        {
            routeInsertOutput (MixerChannelIds::voxInsert (vi),
                                engineScratch, numSamples);
        }
        else
        {
            const int nc = juce::jmin (mtDest->getNumChannels(),
                                        engineScratch.getNumChannels());
            for (int c = 0; c < nc; ++c)
                mtDest->addFrom (c, 0, engineScratch, c, 0, numSamples);
        }
    }
    else   // isInstRoute
    {
        const int ii = routeCh - MixerChannelIds::kInstBase;
        auto* eng = mInstEngines[(size_t) ii];
        if (eng == nullptr) return false;

        engineScratch.setSize (numRenderCh, numSamples, false, false, true);
        engineScratch.clear();
        const int copyCh = juce::jmin (numOut, engineScratch.getNumChannels());
        for (int ch = 0; ch < copyCh; ++ch)
            engineScratch.copyFrom (ch, 0, clipScratch, ch, 0, numSamples);

        pushScToEng (eng, MixerChannelIds::instInsert (ii));
        eng->processBlock (engineScratch, engineMidi);
        mVibeGraph.processInsert (VibeGraph::InsertKind::Inst, ii,
                                   engineScratch, ctx.bpm, ctx.anySolo);

        if (mtDest == nullptr)
        {
            routeInsertOutput (MixerChannelIds::instInsert (ii),
                                engineScratch, numSamples);
        }
        else
        {
            const int nc = juce::jmin (mtDest->getNumChannels(),
                                        engineScratch.getNumChannels());
            for (int c = 0; c < nc; ++c)
                mtDest->addFrom (c, 0, engineScratch, c, 0, numSamples);
        }
    }

    return true;
}

// ── processBlock ──────────────────────────────────────────────────────────────
void VibeSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midiMessages)
{
    // 2026-05-06: project-load barrier - bail immediately if the message
    // thread is mid-teardown (closeAllDynamicTabs / openProject /
    // restoreBackup).  Prevents use-after-free crashes inside engines
    // currently being destroyed (NAMIR + MicPlacementDSP IIR filter
    // dereference was the observed crash signature).
    if (mProjectLoadInProgress.load (std::memory_order_acquire))
    {
        buffer.clear();
        midiMessages.clear();
        return;
    }

    // 2026-05-06 (Batch 9c B1): capture the active AudioClipSnapshot ONCE for
    // this block.  Every iteration site below + every MT worker reads
    // mCurrentBlockClipSnapshot for the rest of the block -- never re-loads
    // mActiveAudioClips -- so the message-thread mutator can swap a new
    // snapshot in mid-block without breaking consistency.
    //
    // The published mAudioInUseClipGen is read by RetirementQueue's drainer
    // to know which retired snapshots are safe to destroy.  Bumping it here
    // (release-store after the load-acquire) is what closes the GC race:
    // any retired snapshot whose retiredBeforeGen > mAudioInUseClipGen at
    // the time of retirement stays alive until this audio thread loads a
    // newer snapshot in a future block and bumps the gen past it.
    {
        auto* snap = mActiveAudioClips.load (std::memory_order_acquire);
        // Bootstrap (ctor) guarantees this is non-null on first entry; the
        // mutator only ever publishes non-null pointers.
        mCurrentBlockClipSnapshot = snap;
        mClipRetirement.setInUseGeneration (snap->generation);
    }

    // ── Multi-threaded render engine branch ─────────────────────────────────
    // Batch 9a (2026-05-06): the MT branch was MOVED from here (top of
    // processBlock, right after the project-load barrier) to AFTER all MIDI
    // scheduling + anySolo computation + routing-graph rebuild.  Reason:
    // BlockContext now needs to carry per-engine MIDI buffer pointers,
    // anySolo, the live-input snapshot, and a routing-graph-aware predecessor
    // list.  Building that context cleanly requires the serial code below
    // to populate the inputs first.  See the new MT branch site after
    // applyChokeGroupDispatch().
    //
    // The project-load barrier above still gates BOTH paths, so MT mode
    // never sees a half-torn-down state.

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
    // blocks pay 1 atomic load + skip - eliminates ~1.4M string-concat hash
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
            else if (off.target == kRustyPRTarget)
                mRustyDrumsMidi.addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
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
        mRustyDrumsMidi.addEvent(juce::MidiMessage::allNotesOff(1), 0);                   // J-7b
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
                    else if (off.target == kRustyPRTarget)
                        mRustyDrumsMidi.addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
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
                // (Same pattern as pattern-mode branch - prevents stuck notes when
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
                            else if (off.target == kRustyPRTarget)
                                mRustyDrumsMidi.addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
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
                            else if (off.target == kRustyPRTarget)
                                mRustyDrumsMidi.addEvent(juce::MidiMessage::noteOff(1, off.midiNote), smp);
                        }
                        else
                        {
                            keep.push_back(off);
                        }
                    }
                    mPRPendingOffs = std::move(keep);
                }

                // Detect song end - request transport stop (or let playhead
                // wrap via mLoopBeats, which is set by the UI when loop mode is on).
                // 2026-04-26: also fire stop when songEnd <= 0 in play-through -
                // empty arrangement was previously playing indefinitely because the
                // `songEnd > 0` guard skipped this block entirely.
                const double songEnd = mSongEndBeats.load(std::memory_order_relaxed);
                const bool   loopOff = ! mSongLoopMode.load(std::memory_order_relaxed);
                if (loopOff && (songEnd <= 0.0 || beatStart >= songEnd))
                {
                    mRequestStop.store(true, std::memory_order_release);
                }

                // Schedule notes from all Pattern arrangement blocks that overlap current beat range.
                // No loop wrap - playhead advances linearly until stop.
                // C.5b (post-revert): Builder grid is uniform 4-beat-per-bar
                // (song-level TS markers are decorative-only).  Block start/end
                // are simple bar*4.  Pattern's own loop length uses the
                // pattern's INTRINSIC TS (FL-style), so a 3/4 pattern in an
                // 8-bar block plays for 3-beat-bars and loops within the block.
                constexpr double kBPB = 4.0;
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
                    // Issue 2 fix (2026-05-17): a pattern clip on the grid is a
                    // VIEWPORT onto the pattern's own timeline, not a looping
                    // container.  Each note plays ONCE at its position; the clip
                    // width [blkStartBeat, blkEndBeat) masks it (content at/after
                    // the clip end = silence, NOT a re-loop; a note overrunning
                    // the clip is cut at blkEndBeat by the note-off clamp).  The
                    // old `rep += patOwnLen` re-loop + patBpb/patOwnLen are gone.
                    // (The `>= beatStart && < beatEnd` boundary gate is intentionally
                    // left strict — the intermittent loop-wrap missed-note is the
                    // transport float-slop bug, fixed in the transport-rework batch,
                    // NOT band-aided here.)
                    auto scheduleRoll = [&](const std::vector<PianoNote>& notes,
                                           juce::MidiBuffer& buf, int target)
                    {
                        for (const auto& note : notes)
                        {
                            if (note.muted) continue;
                            double absStart = blkStartBeat + note.startBeat;
                            if (absStart >= blkEndBeat) continue;   // viewport mask (no re-loop)
                            if (absStart >= beatStart && absStart < beatEnd)
                            {
                                int smp = juce::jlimit(0, numSamples - 1,
                                    (int)juce::jmax(0.0, (absStart - beatStart) / bs));
                                emitPianoNoteOn (buf, note, smp);
                                // Note-off clamped to the clip end so a note that
                                // overruns the viewport is cut at the clip edge.
                                double offBeat = juce::jmin(absStart + note.durationBeats,
                                                            blkEndBeat);
                                mPRPendingOffs.push_back({ offBeat, note.midiNote, target });
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
                    // transpose IS the sound design - it positions the drum's
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
                            {
                                if (! mInstEngines[ii]) continue;
                                // K-3 / L-2 (2026-05-05): only dispatch the Inst
                                // piano roll for sfizz-source pages (Guitars /
                                // Basses).  Live-input Inst pages have no MIDI-
                                // driven engine - the chain is fed by ASIO +
                                // recorded clips, so notes scheduled into
                                // instPageMidi[ii] would just be dropped by
                                // Pedals/NAMIR.
                                const bool sourceActive =
                                    mGuitarsActive[ii].load(std::memory_order_acquire)
                                    || mBassesActive[ii].load(std::memory_order_acquire);
                                if (! sourceActive) continue;
                                scheduleRoll(sPat.instRoll[ii].notes, instPageMidi[ii],
                                             kInstPRTarget + ii);
                            }
                        }
                    }

                    // J-7b (2026-05-03): BaySickRustyDrums singleton roll.
                    // Same fast-path bypass + try-lock pattern as the engine arrays.
                    if (mRustyDrumsActive.load(std::memory_order_acquire))
                    {
                        juce::SpinLock::ScopedTryLockType rlk(mRustyDrumsEngineLock);
                        if (rlk.isLocked() && mRustyDrumsEngine)
                            scheduleRoll(sPat.baySickRustyDrumsRoll.notes,
                                         mRustyDrumsMidi, kRustyPRTarget);
                    }

                    // 2026-04-25: legacy sPat.drumRoll dispatch removed -
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
            // We ONLY extend the window on jumped (post-wrap) blocks - applying it to every
            // block would create a double-trigger: block N bumps beat-0's absStart to patLen
            // (falls in upper window) AND block N+1 catches it via the slop (lower window).
            const double kWrapSlop = beatEnd - beatStart;
            bool jumped = (mPRLastBeatEnd >= 0.0 && beatStart < mPRLastBeatEnd - 0.1);
            const double windowStart = jumped ? (beatStart - kWrapSlop) : beatStart;

            if (jumped)
            {
                // Loop restart (seek or wrap) - fire per-note offs for each pending voice
                // so their release envelopes play naturally.  allNotesOff() does a hard
                // kill on ALL channel-1 voices (click + cuts layer/bass cross-page) -
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
                    else if (off.target == kRustyPRTarget)
                        mRustyDrumsMidi.addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
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
                        else if (off.target == kRustyPRTarget)
                            mRustyDrumsMidi.addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
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
                        else if (off.target == kRustyPRTarget)
                            mRustyDrumsMidi.addEvent(juce::MidiMessage::noteOff(1, off.midiNote), smp);
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
                    // If absStart landed AT or past patLen in a straddling block, skip it -
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
            // no DrumPage tabs exist (D1.3+).  No transpose compensation -
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

            // 2026-04-25: legacy pat.drumRoll dispatch removed -
            // notes are now in pat.drumRolls[di] and dispatched
            // through D1.2 per-drum loop above.

            // G-3 (2026-04-28): per-clip-page rolls - pattern mode.  Mirrors
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

            // G-4 (2026-04-28): per-Vox / per-Inst-page rolls - pattern mode.
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
                    // K-3 / L-2 (2026-05-05): only dispatch instRoll notes when
                    // the page's source is sfizz-driven (Guitars / Basses).
                    // Live input chains discard MIDI; skip the schedule for them.
                    if (! mGuitarsActive[ii].load(std::memory_order_acquire)
                        && ! mBassesActive[ii].load(std::memory_order_acquire))
                        continue;
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

            // J-7b (2026-05-03): BaySickRustyDrums singleton roll - pattern mode.
            // Mirrors the per-engine loops above; bypass via mRustyDrumsActive.
            if (mRustyDrumsActive.load(std::memory_order_acquire))
            {
                juce::SpinLock::ScopedTryLockType rlk(mRustyDrumsEngineLock);
                if (rlk.isLocked() && mRustyDrumsEngine)
                {
                    for (const auto& note : pat.baySickRustyDrumsRoll.notes)
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
                            emitPianoNoteOn (mRustyDrumsMidi, note, smp);
                            mPRPendingOffs.push_back(
                                { absStart + note.durationBeats, note.midiNote, kRustyPRTarget });
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
        // C.5b (post-revert): Builder grid is uniform 4-beat-per-bar.
        const double kBeatsPerBar = 4.0;
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
    // C.4 Phase 1 (2026-04-30): clear SC receive buffers each block before
    // sources fan their post-everything taps in.
    mVibeGraph.clearScRecvBuffers();

    // Batch 9a (2026-05-06): rebuild render-graph predecessor / child links
    // from the freshly-rebuilt RoutingGraph.  Runs on EVERY block regardless
    // of kEnableMultiThreadedEngine - keeps the new plumbing actively
    // exercised in serial mode (per Jeff's "no dead wiring" rule) so any
    // bug in rebuildLinks surfaces today, not when the flag flips.  The
    // serial path doesn't read mPredecessors; the MT path will once
    // dispatchBlock fires.
    mRenderDispatcher.rebuildLinks (mVibeGraph.getRoutingGraph());

    // 2026-05-06 (Batch 9c B1): the per-site try-lock pattern this comment
    // used to describe is gone.  Audio thread now reads the AudioClipSnapshot
    // captured at the top of processBlock (mCurrentBlockClipSnapshot) and
    // every iteration site -- FilePlay scan, song-mode Pass 1, Pass 2,
    // applyChokeGroupDispatch, renderAudioClipsForRow, AudioInsertTask,
    // VoxStripTask, InstStripTask -- reads from that single snapshot.
    // Mutator (rebuildAudioClipPlayers) atomic-exchanges a new snapshot in
    // and retires the old to mClipRetirement; the slow ~AudioClipStreamer
    // destruction runs on the GC drainer thread, never on audio.

    // 5F-4b B1b: routeInsertOutput is a private member function (Batch 5
    // promoted it from a stack lambda).  Existing call sites below resolve
    // to the member; the routing-graph getters are now re-fetched per call
    // (cheap - const ref to a vector).
    const double bpmForInserts = pos.getBpm().orFallback(120.0);

    // C.3 (2026-04-30): drain hardware MIDI input collector and route into
    // the engine page-buffer named by the Piano Roll's currently-focused
    // engine (set via setLiveMidiTarget on focus change).  Runs before
    // choke-group dispatch so live note-ons participate in the same choke
    // semantics as piano-roll scheduled notes.  Q3 spec: only Layer / Bass
    // / Drum receive - Vox / Inst / Clip / DrumKit grid drop.
    {
        juce::MidiBuffer liveMidi;
        mLiveMidiCollector.removeNextBlockOfMessages (liveMidi, numSamples);
        if (! liveMidi.isEmpty())
        {
            // QA-Ea Task 0b (2026-05-18): hardware-MIDI recording fix.  The
            // MIDI recorder reads `allMidi`, built only from host midiMessages
            // (:1038) - it never contained the hardware keyboard (that flows
            // via mLiveMidiCollector, dispatched per-page below), so hardware-
            // MIDI recording captured nothing in BOTH ST and MT (never worked,
            // build-independent).  Merge liveMidi into allMidi so the recorder
            // sees the performance.  Double-trigger-safe: allMidi's only real
            // consumer is the recorder (VibeGraph::processBlock does
            // ignoreUnused(midi) at VibeGraph.cpp:1545; the MT dispatcher
            // never receives allMidi); engines are driven by `dest` below.
            // Forks #25.
            for (const auto m : liveMidi)
                allMidi.addEvent (m.getMessage(), m.samplePosition);

            const int kind = mLiveMidiTargetKind .load (std::memory_order_relaxed);
            const int idx  = mLiveMidiTargetIndex.load (std::memory_order_relaxed);
            juce::MidiBuffer* dest = nullptr;
            // Encoding matches PianoRollPage::EngineKind ordering.
            if      (kind == 1 && idx >= 0 && idx < kMaxLayerPages) dest = &layerPageMidi[idx];
            else if (kind == 2 && idx >= 0 && idx < kMaxBassPages)  dest = &bassPageMidi [idx];
            else if (kind == 3 && idx >= 0 && idx < kMaxDrumPages)  dest = &drumPageMidi [idx];
            if (dest != nullptr)
            {
                for (const auto m : liveMidi)
                    dest->addEvent (m.getMessage(), m.samplePosition);
            }
            // else: drop for ENGINE routing only (DrumKit grid / Clip / Vox /
            // Inst / unset) - allMidi already has it above so the recorder
            // still captures the performance.
        }
    }

    // I-3b (2026-05-02): MIDI Learn dispatch.  Drain device-tagged events
    // pushed by StandaloneApp::handleIncomingMidiMessage.  For each event:
    //   1. If a learn capture is active, route to the registry's capture
    //      handler (which builds a Mapping from the event and commits it).
    //      If captured, the event is suppressed from regular dispatch -- a
    //      learn click that landed on a CC shouldn't ALSO move whatever was
    //      previously mapped to that CC.
    //   2. Otherwise, dispatch through the registry's mapping table; matching
    //      mappings call setValueNotifyingHost on their target APVTS params.
    //
    // Block-rate per locked spec (Jeff 2026-05-02): events apply at the audio
    // block boundary, not sample-accurate within the block.  Stair-step
    // automation behaviour matches every other DAW's MIDI Learn.
    mMidiLearnQueue.drainAndProcess (
        [this] (const juce::String& deviceName, const juce::MidiMessage& msg)
        {
            if (mMidiLearn.tryCaptureLearn (deviceName, msg))
                return;   // event was a learn-capture; don't double-dispatch
            mMidiLearn.dispatchEvent (apvts, deviceName, msg);
        });

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

    // ── QA-E (2026-05-12): FilePlay pre-scan -- MUST run BEFORE the MT branch.
    // Sets mVoxFilePlayActive / mInstFilePlayActive used by BOTH paths:
    //   - Serial: live engine loop skips FilePlay-active pages; Pass 1 drives
    //   - MT: VoxStripTask / InstStripTask gate their FilePlay branch on flag
    // Previously located AFTER the MT early return -- meant MT never saw the
    // flag set, so FilePlay clips never decoded under MT and arrangement
    // playback through Vox/Inst pages was silent.  Originally I-16 G-9
    // (2026-05-03) at the serial pre-scan site; moved here QA-E (2026-05-12).
    mVoxFilePlayActive .fill (false);
    mInstFilePlayActive.fill (false);
    if (mSongMode.load (std::memory_order_relaxed) && pos.getIsPlaying() && mPatternManager)
    {
        const double secPerBeatPS = 60.0 / juce::jmax (20.0, pos.getBpm().orFallback (120.0));
        const double beatStartPS  = pos.getPpqPosition().orFallback (0.0);
        const int64  blockStart   = (int64)(beatStartPS * secPerBeatPS * mSampleRate);
        const int64  blockEnd     = blockStart + numSamples;

        for (auto& p : mCurrentBlockClipSnapshot->players)
        {
            if (p.routeChannel == 0 || p.streamer == nullptr) continue;
            const int64 cs = (int64)(p.clipStartBeat * secPerBeatPS * mSampleRate);
            const int64 ce = (int64)(p.clipEndBeat   * secPerBeatPS * mSampleRate);
            if (blockEnd <= cs || blockStart >= ce) continue;

            const int chId = p.routeChannel;
            if (chId >= MixerChannelIds::kVoxBase
                && chId <  MixerChannelIds::kVoxBase + kMaxVoxPages)
            {
                mVoxFilePlayActive[chId - MixerChannelIds::kVoxBase] = true;
            }
            else if (chId >= MixerChannelIds::kInstBase
                     && chId <  MixerChannelIds::kInstBase + kMaxInstPages)
            {
                mInstFilePlayActive[chId - MixerChannelIds::kInstBase] = true;
            }
        }
    }

    // ── Batch 9a (2026-05-06): MT engine branch, NEW location ───────────────
    // All inputs the MT path needs are now in scope: numSamples + pos +
    // anySolo + per-engine MidiBuffers + mLiveInputSnapshot + routing graph
    // (rebuilt above).  Build BlockContext once and hand off to the
    // dispatcher; the serial path below is skipped via early return.
    //
    // 2026-05-07 (Batch 10): gMultiThreadedEngineEnabled is now a runtime
    // std::atomic<bool>, hot-toggleable from the Mixer hamburger menu.
    // Both branches live in the compiled binary; the audio thread picks
    // one per block via this acquire-load.  No glitches on flip --
    // dispatcher tasks + arena stay live regardless, and rebuildLinks at
    // line 1737 fires every block so MT-side predecessor / dep state is
    // always fresh.  Cost: one atomic-load + one branch per block.
    if (RenderEngine::gMultiThreadedEngineEnabled.load (std::memory_order_acquire))
    {
        // 2026-05-07 (Batch 9c follow-up): compute busAnySolo (bus-level
        // solo flag, distinct from strip-level anySolo).  Mirrors the serial
        // path's busAnySolo computation at PluginProcessor.cpp:2504.  Used
        // by PassiveStripTask to feed the correct solo signal into
        // processBus for Vox/Inst/Vox2/Inst2/Inst3 buses (whose default
        // useGroupSolo = anySolo formula would otherwise mute the bus
        // whenever any STRIP is soloed -- the serial bug surfaced under MT
        // because PassiveStripTask was passing strip-level anySolo).
        // QA-Ea Part A (2026-05-21): busAnySolo here is now DEAD STATE.
        // VibeGraph::processBus computes its own anyBus via the unified
        // anyBusSoloed() helper (all 11 bus _solo params) and ignores the
        // caller-passed anySolo param.  Kept compiled to avoid touching every
        // call site + every BlockContext field; QA-Ef ST deletion + the MT
        // BlockContext slim-down will drop this and the mtCtx.busAnySolo
        // field entirely.
        auto soloOfBus = [this] (const char* prefix) -> bool
        {
            const auto* p = apvts.getRawParameterValue (juce::String (prefix) + "_solo");
            return p && p->load() > 0.5f;
        };
        const bool busAnySolo =
               soloOfBus ("mixer_clipsbus")
            || soloOfBus ("mixer_voxbus")  || soloOfBus ("mixer_instbus")
            || soloOfBus ("mixer_voxbus2") || soloOfBus ("mixer_instbus2")
            || soloOfBus ("mixer_instbus3")
            || soloOfBus ("mixer_fx");

        BlockContext mtCtx;
        mtCtx.numSamples         = numSamples;
        mtCtx.bpm                = pos.getBpm().orFallback (120.0);
        mtCtx.anySolo            = anySolo;
        mtCtx.busAnySolo         = busAnySolo;
        // 2026-05-06 (Batch 9b): cache project pan law for bus tasks.
        mtCtx.panLaw             =
            (apvts.getRawParameterValue("master_pan_law") != nullptr)
                ? (int) apvts.getRawParameterValue("master_pan_law")->load()
                : 0;
        mtCtx.posInfo            = &pos;
        mtCtx.layerPageMidi      = layerPageMidi.data();
        mtCtx.bassPageMidi       = bassPageMidi .data();
        mtCtx.drumPageMidi       = drumPageMidi .data();
        mtCtx.clipPageMidi       = clipPageMidi .data();
        mtCtx.voxPageMidi        = voxPageMidi  .data();
        mtCtx.instPageMidi       = instPageMidi .data();
        mtCtx.rustyDrumsMidi     = &mRustyDrumsMidi;
        mtCtx.liveInputSnapshot  = &mLiveInputSnapshot;

        mRenderDispatcher.dispatchBlock (buffer, mtCtx);

        // QA-Ea Task 0b (2026-05-18): MT serial-tail divergence fix - feed
        // the master/MIDI recorders + run metronome/count-in in MT too.
        // buffer here is the final master, pre-metronome (MT has no metro
        // before this), so the recorder stays click-free exactly like the
        // serial path's D-5 ordering.  Forks #25.
        applyPostMixRecordAndMetro (buffer, allMidi, pos, numSamples);

        // 2026-05-07 (Batch 9c follow-up): drain UI meter atomics same as the
        // serial tail does, otherwise dBFS / VU / per-effect meters all sit
        // at -inf (the audio path's peak writes never reach the UI mirrors).
        // The MasterTask + per-strip tasks have already populated the
        // node-level atomics by this point; we just need to promote them.
        drainMeterAtomicsForUI();

        // 2026-05-07 (Batch 10): drive the DSP-load meter + overload-protection
        // path the same as the serial tail.  Without this, the in-app DSP
        // meter reads 0% under MT because the t0->t1 deltaT computation
        // never runs (we returned early before the inline block at the end
        // of processBlock).  Under MT this measures audio-thread wall-clock
        // -- naturally lower than serial when workers carry parallel load,
        // which IS the architectural win and is the metric to compare when
        // hot-toggling MT/serial via the Mixer hamburger.
        measureDspLoadAndOverload (t0, numSamples);
        return;
    }

    // C.4 Phase 2.2: helper that pushes the strip's SC array to the engine
    // via ISidechainEngine, then calls the engine's processBlock.  The cast
    // is safe - every engine processor type registered into mLayerEngines /
    // mBassEngines / mDrumEngines / mClipEngines / mVoxEngines / mInstEngines
    // inherits ISidechainEngine.  dynamic_cast cost is acceptable here (one
    // per active engine per block).
    auto pushScToEngine = [this] (juce::AudioProcessor* eng, int channelId)
    {
        if (auto* sc = dynamic_cast<ISidechainEngine*>(eng))
        {
            const auto arr = mVibeGraph.getScRecvArray(channelId);
            juce::AudioBuffer<float>* bufs[VibeGraph::kMaxScRecvSlots];
            for (int s = 0; s < VibeGraph::kMaxScRecvSlots; ++s)
                bufs[s] = arr[(size_t) s];
            sc->setSidechainBuffers(bufs, VibeGraph::kMaxScRecvSlots);
        }
    };

    {
        juce::SpinLock::ScopedTryLockType lk(mLayerEngineLock);
        if (lk.isLocked())
        {
            for (int i = 0; i < kMaxLayerPages; ++i)
            {
                if (!mLayerEngines[i]) continue;
                mLayerEngineScratch.setSize(numRenderCh, numSamples, false, false, true);
                mLayerEngineScratch.clear();
                pushScToEngine(mLayerEngines[i], MixerChannelIds::layerInsert(i));
                mLayerEngines[i]->processBlock(mLayerEngineScratch, layerPageMidi[i]);
                // §P4.3 B7: legacy per-page pre-rack EQ removed - pre-rack EQ is
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
    // Use a sum-based check - SIMD getMagnitude() may silently swallow NaN.
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
                pushScToEngine(mBassEngines[i], MixerChannelIds::bassInsert(i));
                mBassEngines[i]->processBlock(mBassEngineScratch, bassPageMidi[i]);
                // §P4.3 B7: legacy per-page pre-rack EQ removed - see Layer loop.
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
                pushScToEngine(mDrumEngines[i], MixerChannelIds::drumInsert(i));
                mDrumEngines[i]->processBlock(mDrumEngineScratch, drumPageMidi[i]);
                mVibeGraph.processInsert(VibeGraph::InsertKind::Drum, i,
                                          mDrumEngineScratch, bpmForInserts, anySolo);
                routeInsertOutput(MixerChannelIds::drumInsert(i),
                                   mDrumEngineScratch, numSamples);
            }
        }
    }

    // ── J-7b (2026-05-04): Render the BaySickRustyDrums singleton ──────────
    // Single sfizz instance renders one stereo pair per kit piece into
    // its multi-out scratch via the wrapper SFZ (output=N injected at
    // <master>/<group> level in every piece file's text).  We fan each
    // strip's stereo pair through its dedicated Rusty InsertNode so
    // faders / mute / solo / EQ / FX rack / sends operate per-piece.
    // Rusty InsertNodes route to kRustyDrumsBus by default.
    if (mRustyDrumsActive.load(std::memory_order_acquire))
    {
        juce::SpinLock::ScopedTryLockType lk(mRustyDrumsEngineLock);
        if (lk.isLocked() && mRustyDrumsEngine)
        {
            // 2026-05-06 Option A: Rusty idle suspend.  When MIDI is empty +
            // sfizz reports 0 active voices for kIdleSuspendBlocks consecutive
            // blocks, skip processStrips + the per-strip insert/route loop.
            // Drum kits release fast (no long sustain), so this kicks in
            // quickly after the last hit.  Big win because Rusty has 13
            // strips - each one runs an InsertNode pipeline per block.
            const bool midiEmpty = mRustyDrumsMidi.getNumEvents() == 0;
            const bool noVoices  = mRustyDrumsEngine->getNumActiveVoices() == 0;
            // QA-C DSP-10 (2026-05-10): see RustyDrumsProducerTask.cpp for
            // rationale.  Same predicate fix on the serial-path (MT engine
            // off) mirror.
            const bool auditionPending = mRustyDrumsEngine->isAuditionPending();
            bool suspended = false;
            if (midiEmpty && noVoices && ! auditionPending)
            {
                if (mRustyIdleBlocks >= kIdleSuspendBlocks)
                    suspended = true;
                else
                    ++mRustyIdleBlocks;
            }
            else
            {
                mRustyIdleBlocks = 0;
            }

            if (suspended)
            {
                mRustyDrumsMidi.clear();
            }
            else
            {
                mRustyDrumsEngine->processStrips(numSamples, mRustyDrumsMidi);
                const int stripCount = mRustyDrumsEngine->getStripCount();
                mRustyDrumsScratch.setSize(numRenderCh, numSamples, false, false, true);
                for (int s = 0; s < stripCount; ++s)
                {
                    mRustyDrumsScratch.clear();
                    auto stripBuf = mRustyDrumsEngine->getStripBuffer(s, numSamples);
                    if (stripBuf.getNumChannels() >= 2 && numRenderCh >= 2)
                    {
                        mRustyDrumsScratch.copyFrom(0, 0, stripBuf, 0, 0, numSamples);
                        mRustyDrumsScratch.copyFrom(1, 0, stripBuf, 1, 0, numSamples);
                    }
                    else if (stripBuf.getNumChannels() >= 1)
                    {
                        mRustyDrumsScratch.copyFrom(0, 0, stripBuf, 0, 0, numSamples);
                    }
                    mVibeGraph.processInsert(VibeGraph::InsertKind::Rusty, s,
                                              mRustyDrumsScratch, bpmForInserts, anySolo);
                    routeInsertOutput(MixerChannelIds::rustyInsert(s),
                                       mRustyDrumsScratch, numSamples);
                }
            }
        }
    }

    // ── G-3 (2026-04-28): Render per-clip-page engines ──────────────────────
    // Same shape as the drum-engine loop above, but routes the engine output
    // through the existing Audio InsertNode for the bound row (clip-page-index
    // = audio-row-index, 1:1 mapping per VibesynthConstants.h).  In song mode
    // the row's InsertNode is also processed by the audio_clip_players loop
    // below (arrangement-playback path) - both paths feed the same rack /
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
                pushScToEngine(mClipEngines[ci], MixerChannelIds::audioInsert(ci));
                mClipEngines[ci]->processBlock(mClipEngineScratch, clipPageMidi[ci]);
                mVibeGraph.processInsert(VibeGraph::InsertKind::Audio, ci,
                                          mClipEngineScratch, bpmForInserts, anySolo);
                // 2026-05-02: drain-and-merge into running-max companions.
                // End-of-processBlock promotion lifts them into the UI-visible
                // atomics atomically alongside every other meter.
                {
                    const auto [pkL, pkR] = mVibeGraph.drainInsertPeakDbStereo(
                        VibeGraph::InsertKind::Audio, ci);
                    auto rowCasMax = [] (std::atomic<float>& a, float v) noexcept
                    {
                        if (v == -std::numeric_limits<float>::infinity()) return;
                        float cur = a.load(std::memory_order_relaxed);
                        while (cur < v
                               && ! a.compare_exchange_weak(cur, v, std::memory_order_relaxed))
                        {}
                    };
                    rowCasMax(mAudioRowPeakDbLRun[ci], pkL);
                    rowCasMax(mAudioRowPeakDbRRun[ci], pkR);
                    rowCasMax(mAudioRowPeakDbRun [ci], juce::jmax(pkL, pkR));
                }
                routeInsertOutput(MixerChannelIds::audioInsert(ci),
                                   mClipEngineScratch, numSamples);
            }
        }
    }

    // ── G-4 (2026-04-28) / I-16 G-9 (2026-05-03): per-Vox / per-Inst engines.
    // Source mux: LiveASIO (armed) / Silence (else).  FilePlay-active pages
    // are skipped; the audio-clip loop below drives their engine with the
    // streamed clip samples + setForcePitchBypass(true).  Fast-path bypass
    // via mAnyXPageActive.
    if (mAnyVoxPageActive.load(std::memory_order_acquire))
    {
        juce::SpinLock::ScopedTryLockType lk(mVoxEngineLock);
        if (lk.isLocked())
        {
            for (int vi = 0; vi < kMaxVoxPages; ++vi)
            {
                if (!mVoxEngines[vi]) continue;
                if (mVoxFilePlayActive[vi]) continue;   // clip loop will drive

                mVoxEngineScratch.setSize(numRenderCh, numSamples, false, false, true);
                mVoxEngineScratch.clear();

                // I-16 G-9: LiveASIO source mux -- if this Vox strip is armed,
                // copy the selected ASIO input channel into the engine scratch
                // (mono -> dual-mono).  Else silence (engine generates from MIDI
                // or sits silent if it has no internal source).
                const juce::String voxPrefix = "mixer_vox_" + juce::String (vi);
                const auto* armP    = apvts.getRawParameterValue (voxPrefix + "_arm");
                const auto* idxP    = apvts.getRawParameterValue (voxPrefix + "_inputChannelIdx");
                const auto* stereoP = apvts.getRawParameterValue (voxPrefix + "_inputChannelStereo");
                const auto* listenP = apvts.getRawParameterValue (voxPrefix + "_listen");
                const int   chIdx    = (idxP != nullptr) ? (int) idxP->load() : -1;
                const bool  isStereo = (stereoP != nullptr) && stereoP->load() > 0.5f;
                const bool  channelOK = (chIdx >= 0 && chIdx < mLiveInputSnapshot.getNumChannels());
                const bool  armed    = (armP    != nullptr) && armP   ->load() > 0.5f && channelOK;
                const bool  listen   = (listenP != nullptr) && listenP->load() > 0.5f;
                // QA-E Task 5 (2026-05-15): live input flows through the chain
                // whenever EITHER arm OR listen is engaged (with a channel
                // selected).  Prior behavior gated on armed-only, making
                // "monitor without recording" impossible.
                const bool  active   = channelOK && (armed || listen);
                if (active)
                {
                    const int n = numSamples;
                    // I-16 G-9: dry recorder tap (RAW pre-chain mono ASIO).
                    // Captured here so the recorded file is the unprocessed
                    // DI -- chain runs ONCE on the dry source whether live
                    // or playing back (single-pass guarantee).
                    // 2026-05-06 (Batch 9b Item 8): inline loop migrated to
                    // tapDryRecorder helper so VoxStripTask::run can call
                    // the same path under MT mode.
                    // QA-E Task 5 (2026-05-15): only fire when ARMED
                    // (monitor-only mode produces no recording).
                    if (armed)
                        tapDryRecorder (MixerChannelIds::voxInsert (vi),
                                         mLiveInputSnapshot.getReadPointer (chIdx),
                                         n);

                    // B2 (2026-05-04): stereo input pair -> copy chIdx into L,
                    // chIdx+1 into R.  Falls back to dual-mono if the right
                    // channel is out of range (e.g. last channel selected as
                    // a pair on a device with one more channel than expected).
                    const int rightCh = (isStereo && chIdx + 1 < mLiveInputSnapshot.getNumChannels())
                                          ? (chIdx + 1) : chIdx;
                    if (mVoxEngineScratch.getNumChannels() > 0)
                        mVoxEngineScratch.copyFrom (0, 0, mLiveInputSnapshot, chIdx, 0, n);
                    if (mVoxEngineScratch.getNumChannels() > 1)
                        mVoxEngineScratch.copyFrom (1, 0, mLiveInputSnapshot, rightCh, 0, n);
                }

                // I-16 G-9: clear pitch force-bypass for non-FilePlay sources.
                if (auto* vp = dynamic_cast<BaySickVocalProcessor*> (mVoxEngines[vi]))
                    vp->setForcePitchBypass (false);

                pushScToEngine(mVoxEngines[vi], MixerChannelIds::voxInsert(vi));
                mVoxEngines[vi]->processBlock(mVoxEngineScratch, voxPageMidi[vi]);
                mVibeGraph.processInsert(VibeGraph::InsertKind::Vox, vi,
                                          mVoxEngineScratch, bpmForInserts, anySolo);
                // I-16 G-9 + QA-E Task 5: armed && !listen -> kill output.
                // Unarmed + listen -> routes naturally (input copied above).
                // Unarmed + !listen -> silent strip; routing zero-buffer is fine.
                if (! armed || listen)
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
                if (mInstFilePlayActive[ii]) continue;   // clip loop will drive

                mInstEngineScratch.setSize(numRenderCh, numSamples, false, false, true);
                mInstEngineScratch.clear();

                // K-2 / L-2 (2026-05-05): when this Inst slot's source =
                // BaySickGuitars or BaySickBasses, the engine chain has the
                // sfizz processor as its first stage and produces audio from
                // piano-roll MIDI - no live input is copied into the scratch.
                // The chain's processBlock will fill the cleared buffer with
                // sfizz output, then run Pedals/NAMIR on it.
                const bool guitarsActive = mGuitarsActive[ii].load (std::memory_order_acquire);
                const bool bassesActive  = mBassesActive [ii].load (std::memory_order_acquire);
                const bool sfizzActive   = guitarsActive || bassesActive;

                // 2026-05-06 Option A: per-tab idle suspension for sfizz Inst
                // tabs.  When the tab's MIDI buffer is empty, sfizz has 0
                // active voices, AND we've stayed in that state for >
                // kIdleSuspendBlocks blocks (~200ms), skip the ENTIRE chain
                // (sfizz + Pedals + NAMIR + insert rack + EQ).  Wake instantly
                // on the next block where any of those gates fails.  Live-
                // input Inst tabs are NOT suspended (live audio + arm/listen
                // can fire even with no MIDI).  This is the big DSP win for
                // many-tab sessions where most tabs are silent at any moment.
                if (sfizzActive)
                {
                    int activeVoices = 0;
                    if (auto* g = getBaySickGuitars (ii)) activeVoices = g->getNumActiveVoices();
                    if (activeVoices == 0)
                        if (auto* b = getBaySickBasses (ii)) activeVoices = b->getNumActiveVoices();

                    const bool midiEmpty = instPageMidi[ii].getNumEvents() == 0;
                    const bool noVoices  = activeVoices == 0;

                    // QA-C DSP-10 (2026-05-10): see InstStripTask.cpp for
                    // rationale.  Same predicate fix on the serial-path
                    // (MT engine off) mirror.
                    bool auditionPending = false;
                    if (auto* g = getBaySickGuitars (ii)) auditionPending = g->isAuditionPending();
                    if (! auditionPending)
                        if (auto* b = getBaySickBasses (ii)) auditionPending = b->isAuditionPending();

                    if (midiEmpty && noVoices && ! auditionPending)
                    {
                        if (mInstIdleBlocks[(size_t) ii] >= kIdleSuspendBlocks)
                            continue;   // suspended this block
                        ++mInstIdleBlocks[(size_t) ii];
                    }
                    else
                    {
                        mInstIdleBlocks[(size_t) ii] = 0;   // wake
                    }
                }

                const juce::String instPrefix = "mixer_inst_" + juce::String (ii);
                const auto* armP    = apvts.getRawParameterValue (instPrefix + "_arm");
                const auto* idxP    = apvts.getRawParameterValue (instPrefix + "_inputChannelIdx");
                const auto* stereoP = apvts.getRawParameterValue (instPrefix + "_inputChannelStereo");
                const auto* listenP = apvts.getRawParameterValue (instPrefix + "_listen");
                const int   chIdx    = (idxP != nullptr) ? (int) idxP->load() : -1;
                const bool  isStereo = (stereoP != nullptr) && stereoP->load() > 0.5f;
                const bool  channelOK = (chIdx >= 0 && chIdx < mLiveInputSnapshot.getNumChannels());
                const bool  armed    = ! sfizzActive
                                    && (armP    != nullptr) && armP   ->load() > 0.5f && channelOK;
                const bool  listen   = ! sfizzActive
                                    && (listenP != nullptr) && listenP->load() > 0.5f;
                // QA-E Task 5 (2026-05-15): live input flows through the chain
                // whenever EITHER arm OR listen is engaged (with a channel
                // selected).  sfizz-source slots ignore both -- sfizz is the
                // source, no live input.
                const bool  active   = channelOK && (armed || listen);
                if (active)
                {
                    const int n = numSamples;
                    // I-16 G-9: dry recorder tap for Inst (single file -- no
                    // realtime stage analogous to Vox's pitch correction).
                    // 2026-05-06 (Batch 9b Item 8): inline loop migrated to
                    // tapDryRecorder helper so InstStripTask::run can call
                    // the same path under MT mode.
                    // QA-E Task 5 (2026-05-15): only fire when ARMED.
                    if (armed)
                        tapDryRecorder (MixerChannelIds::instInsert (ii),
                                         mLiveInputSnapshot.getReadPointer (chIdx),
                                         n);

                    // B2 (2026-05-04): stereo input pair -> copy chIdx into L,
                    // chIdx+1 into R.  Falls back to dual-mono on the right
                    // channel if it's out of range.
                    const int rightCh = (isStereo && chIdx + 1 < mLiveInputSnapshot.getNumChannels())
                                          ? (chIdx + 1) : chIdx;
                    if (mInstEngineScratch.getNumChannels() > 0)
                        mInstEngineScratch.copyFrom (0, 0, mLiveInputSnapshot, chIdx, 0, n);
                    if (mInstEngineScratch.getNumChannels() > 1)
                        mInstEngineScratch.copyFrom (1, 0, mLiveInputSnapshot, rightCh, 0, n);
                }

                pushScToEngine(mInstEngines[ii], MixerChannelIds::instInsert(ii));
                mInstEngines[ii]->processBlock(mInstEngineScratch, instPageMidi[ii]);
                mVibeGraph.processInsert(VibeGraph::InsertKind::Inst, ii,
                                          mInstEngineScratch, bpmForInserts, anySolo);
                // I-16 G-9 + QA-E Task 5: armed && !listen -> kill output.
                // Unarmed + listen -> route naturally.  sfizz-source slots
                // always route (armed is false, listen is false; falls into
                // the "always route" else branch).
                if (! armed || listen)
                    routeInsertOutput(MixerChannelIds::instInsert(ii),
                                       mInstEngineScratch, numSamples);
            }
        }
    }

    // ── Audio clip playback - runs BEFORE VibeGraph so master rack sees clips ─────
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

        // 2026-05-06 (Batch 9c B1): try-lock removed -- read the audio-thread
        // snapshot captured at the top of processBlock.
        {
            // Bus accumulation buffer: all per-clip processed audio sums here.
            const int numOut = buffer.getNumChannels();
            mAudioRowScratch .setSize(numOut, numSamples, false, false, true);
            mAudioClipScratch.setSize(numOut, numSamples, false, false, true);
            mAudioRowScratch.clear();

            // 2026-05-06 (Batch 9b Item 9): clipCtx built once and shared by
            // both Pass 1 (FilePlay via renderFilePlayPlayer) and Pass 2
            // (non-FilePlay via renderAudioClipsForRow).  Pass 2 overrides
            // clipScratch per-row to the task's owned scratch (Item 10);
            // Pass 1 uses the shared mAudioClipScratch.
            AudioClipBlockContext clipCtx;
            clipCtx.bpm           = bpmAC;
            clipCtx.anySolo       = anySolo;
            clipCtx.secPerBeat    = secPerBeat;
            clipCtx.projectStart  = projectStart;
            clipCtx.projectEnd    = projectEnd;
            clipCtx.numSamples    = numSamples;
            clipCtx.numOut        = numOut;
            clipCtx.masterGain    = masterGain;
            clipCtx.mxState       = &mx;
            clipCtx.clipScratch   = &mAudioClipScratch;

            // ── Batch 5 (2026-05-06): split audio-clip rendering ────────────
            // Pass 1 below: ONLY FilePlay clips (clip routed to a Vox/Inst
            // engine). Non-FilePlay clips are skipped here and handled by
            // renderAudioClipsForRow per-row pass after this loop.
            //
            // 2026-05-06 (Batch 9b Item 9): per-clip body migrated into
            // renderFilePlayPlayer.  VoxStripTask / InstStripTask call the
            // same helper from their FilePlay branches under MT flag.
            // 2026-05-06 (Batch 9c B1): iterate the audio-thread snapshot.
            for (auto& player : mCurrentBlockClipSnapshot->players)
            {
                if (player.streamer == nullptr) continue;

                // Filter to FilePlay-only at the top of this pass.
                const int rch = player.routeChannel;
                const bool isVox  = rch >= MixerChannelIds::kVoxBase
                                 && rch <  MixerChannelIds::kVoxBase + kMaxVoxPages;
                const bool isInst = rch >= MixerChannelIds::kInstBase
                                 && rch <  MixerChannelIds::kInstBase + kMaxInstPages;
                if (! isVox && ! isInst) continue;   // non-FilePlay → Pass 2 handles it

                // Resolve per-page MIDI buffer + dispatch to helper.
                juce::MidiBuffer& engineMidi = isVox
                    ? voxPageMidi [(size_t)(rch - MixerChannelIds::kVoxBase)]
                    : instPageMidi[(size_t)(rch - MixerChannelIds::kInstBase)];

                // QA-E Task 3 follow-up (2026-05-12): serial Pass 1 is single-
                // threaded so processor-member scratches are safe -- pick the
                // matching one based on this player's route.  MT path passes
                // per-task scratches (see VoxStripTask / InstStripTask).
                auto& engineScratch = isVox ? mVoxEngineScratch : mInstEngineScratch;

                renderFilePlayPlayer (player, clipCtx, engineMidi, /*mtDest=*/ nullptr, engineScratch);
            }

            // ── Batch 5 Pass 2: non-FilePlay clips per-row ──────────────────
            // The shared helper is also called by AudioInsertTask in MT mode
            // (flag still false; dead at runtime).
            //
            // 2026-05-06 (Batch 9b Item 10): each row uses its task-owned
            // scratch buffer instead of the single shared mAudioClipScratch.
            // Eliminates the cross-row decode race that would surface when
            // kEnableMultiThreadedEngine flips and multiple AudioInsertTasks
            // run in parallel.  Serial loop routes through the same
            // per-task buffers so the new ownership is actively exercised
            // under flag=false ("no dead wiring" rule).  Fallback to
            // mAudioClipScratch only if a row has no registered task -
            // shouldn't happen in practice (ensureAudioInsert creates the
            // task on first use) but keeps the code defensive.
            //
            // 2026-05-06 (Batch 9b Item 9): clipCtx already built above for
            // Pass 1's renderFilePlayPlayer calls; Pass 2 reuses it and just
            // overrides clipScratch per-row to point at the task-owned buffer.
            for (int row = 0; row < kMaxAudioRows; ++row)
            {
                auto& task = mAudioRenderTasks[(size_t) row];
                clipCtx.clipScratch = task
                    ? &task->getClipScratch (numOut, numSamples)
                    : &mAudioClipScratch;
                renderAudioClipsForRow (row, clipCtx, /*mtDest=*/ nullptr);
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

    // ── G-3 (2026-04-28): Clips Bus pre-processing - runs in BOTH song and
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

            // 2026-05-06 (Batch 9b): ClipsBus DSP migrated into
            // VibeGraph::processBus.  Same chain (preEq -> rack -> postEq ->
            // polarity/width -> fader x mute x in-group solo -> pan -> peak
            // meter); same APVTS reads; same 6-bus localAnySolo formula
            // (preserved bug-for-bug - see processBus comment).  The
            // anySolo arg is ignored for kClipsBus; processBus re-derives
            // the receive-group flag internally because this block runs
            // BEFORE the Vox/Inst BusSet[] loop's busAnySolo is computed.
            const int clipsPanLaw =
                (apvts.getRawParameterValue("master_pan_law") != nullptr)
                    ? (int) apvts.getRawParameterValue("master_pan_law")->load()
                    : 0;
            mVibeGraph.processBus(MixerChannelIds::kClipsBus, clipsBus,
                                   bpmForInserts, /*anySolo (ignored)*/ false,
                                   clipsPanLaw);

            // QA-Ea Part B (Q2): serial currently reaches master for Clips ONLY
            // via the bespoke audioClipsPreRendered sum.  Route kClipsBus →
            // kMaster like every other bus (the kClipsBus→kMaster edge already
            // exists; MT/MasterTask uses it).
            routeInsertOutput (MixerChannelIds::kClipsBus, clipsBus, numSamples);

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

    // R3 (2026-04-23) / I-16 G-9 (2026-05-03): the standalone live-ASIO
    // loop that ran the InsertNode on raw ASIO input was REMOVED here.
    // The Vox / Inst engine loop above now handles armed live input via
    // the source mux (LiveASIO copies snapshot -> engine scratch -> chain
    // -> InsertNode -> bus).  Dry-recording tap moved to the engine loop's
    // armed branch.  Listen toggle gate moved to the engine loop's
    // routeOutput check.  Single audio path, no double-processing.
    if (numInputs > 0)
    {

        // R3.5: process Vox + Inst BUS accumulators through their full DSP
        // chain (preEQ -> rack -> postEQ -> polarity/width -> fader/mute) and
        // measure peak before fanning out to Master.  Mirrors the Audio Clips
        // Bus path above.
        // 2026-05-06 (Batch 9b): Vox / Inst / Vox2 / Inst2 / Inst3 bus DSP
        // migrated into VibeGraph::processBus.  All five buses share the
        // same DSP shape (preEq -> rack -> postEq -> polarity/width -> fader
        // x mute x in-group solo -> pan -> peak meter), so the for-loop is
        // now just a list of channel IDs that processBus dispatches on.
        // routeInsertOutput remains here (caller responsibility - processBus
        // does DSP only).
        // 2026-04-29: project-level pan law selector - each bus reads it once
        // per block when applying its _pan param.
        const int panLaw =
            (apvts.getRawParameterValue("master_pan_law") != nullptr)
                ? (int) apvts.getRawParameterValue("master_pan_law")->load()
                : 0;

        // 2026-04-30: cross-bus solo flag for the "post-rack receive" group
        // (Audio Clips, Vox, Inst, Vox2, Inst2, Inst3).  When ANY bus in this
        // group is soloed, all non-soloed buses in the group go silent
        // (matches Layers/Bass/Drums in-group solo).  C.1: FX joins the group.
        // QA-Ea Part A (2026-05-21): busAnySolo here is now DEAD STATE -- the
        // 7-bus subset was the root cause of "solo dead on 7 of 11 buses".
        // VibeGraph::processBus now computes its own anyBus via the unified
        // anyBusSoloed() helper (all 11 buses) and ignores the caller-passed
        // anySolo param.  Kept compiled to avoid touching every processBus
        // call site; QA-Ef ST deletion will drop this entirely along with
        // the rest of the serial tail.
        auto soloOf = [&] (const char* prefix) -> bool
        {
            const auto* p = apvts.getRawParameterValue (juce::String (prefix) + "_solo");
            return p && p->load() > 0.5f;
        };
        const bool busAnySolo =
               soloOf ("mixer_clipsbus")
            || soloOf ("mixer_voxbus")  || soloOf ("mixer_instbus")
            || soloOf ("mixer_voxbus2") || soloOf ("mixer_instbus2")
            || soloOf ("mixer_instbus3")
            || soloOf ("mixer_fx");   // C.1: FX Bus joins receive-group solo

        // G-6 (2026-04-29): secondary buses always processed (cheap when no
        // inserts route to them - buffer is silent).  UI activation (Mixer
        // "Add Vox/Inst Bus" button) is independent of audio path.
        for (const int busChId : { MixerChannelIds::kVoxBus,
                                    MixerChannelIds::kInstBus,
                                    MixerChannelIds::kVoxBus2,
                                    MixerChannelIds::kInstBus2,
                                    MixerChannelIds::kInstBus3 })
        {
            auto* accum = mVibeGraph.getChannelAccumulator (busChId);
            if (accum == nullptr) continue;
            if (accum->getNumChannels() < 2) continue;

            mVibeGraph.processBus (busChId, *accum, bpmForInserts, busAnySolo, panLaw);
            routeInsertOutput (busChId, *accum, numSamples);
        }
    }

    // C.1 (2026-04-30): FX Bus pipeline.  By this point its accumulator has
    // been populated by every upstream insert / aux / bus that targets it
    // (default destination for aux strips).  Pre-C.1 the accumulator was
    // built but never read back -- effects loaded into the FX Bus rack
    // produced zero audio change and aux output silently disappeared.
    {
        // Compute panLaw the same way the Vox/Inst bus loop did (whether
        // or not the loop ran -- we may be in numInputs == 0 path).
        const int fxPanLaw =
            (apvts.getRawParameterValue("master_pan_law") != nullptr)
                ? (int) apvts.getRawParameterValue("master_pan_law")->load()
                : 0;

        // Recompute busAnySolo for the FX Bus path -- the Vox/Inst loop's
        // local is scoped to the if (numInputs > 0) block above.
        auto soloOfFx = [&] (const char* p) -> bool {
            const auto* v = apvts.getRawParameterValue (juce::String (p) + "_solo");
            return v && v->load() > 0.5f;
        };
        const bool fxBusAnySolo =
               soloOfFx ("mixer_clipsbus")
            || soloOfFx ("mixer_voxbus")  || soloOfFx ("mixer_instbus")
            || soloOfFx ("mixer_voxbus2") || soloOfFx ("mixer_instbus2")
            || soloOfFx ("mixer_instbus3")
            || soloOfFx ("mixer_fx");

        if (auto* fxAccum = mVibeGraph.getChannelAccumulator(MixerChannelIds::kFxBus))
        {
            if (fxAccum->getNumChannels() >= 2)
            {
                mVibeGraph.processEffectsBus (*fxAccum, bpmForInserts,
                                                fxBusAnySolo, fxPanLaw);

                // 2026-05-02: drain-and-merge -- exchange the FxBus node's
                // running-max atomics (resets them to -inf so the next block
                // starts fresh) and CAS-max into the processor mirror so the
                // mirror accumulates running max across blocks.  UI vblank
                // exchange-and-resets the mirror to take a per-frame window.
                const auto [pkL, pkR] = mVibeGraph.drainEffectsBusPeakDbStereo();
                auto fxCasMax = [] (std::atomic<float>& a, float v) noexcept
                {
                    if (v == -std::numeric_limits<float>::infinity()) return;
                    float cur = a.load(std::memory_order_relaxed);
                    while (cur < v
                           && ! a.compare_exchange_weak(cur, v, std::memory_order_relaxed))
                    {}
                };
                fxCasMax (mFxBusPeakDbLRun, pkL);
                fxCasMax (mFxBusPeakDbRRun, pkR);
                fxCasMax (mFxBusPeakDbRun,  juce::jmax (pkL, pkR));

                // Fan the post-pipeline output to FX Bus's _sendTo destination
                // (default = Master).
                routeInsertOutput (MixerChannelIds::kFxBus, *fxAccum, numSamples);
            }
        }
    }

    // J-7a (2026-05-03): RustyDrums Bus pipeline.  Unconditional (engine-driven,
    // not gated by live input).  Mirrors the kBusSets loop pattern for Vox/Inst
    // but compact + always runs so the singleton's audio reaches Master.
    if (mRustyDrumsActive.load(std::memory_order_acquire))
    {
        if (auto* accum = mVibeGraph.getChannelAccumulator(MixerChannelIds::kRustyDrumsBus))
        {
            if (accum->getNumChannels() >= 2)
            {
                // 2026-05-06 (Batch 9b): RustyDrums Bus DSP migrated into
                // VibeGraph::processBus.  Standalone bus (no in-group solo);
                // anySolo arg is ignored for kRustyDrumsBus.  routeInsertOutput
                // remains here.
                const int rdPanLaw =
                    (apvts.getRawParameterValue("master_pan_law") != nullptr)
                        ? (int) apvts.getRawParameterValue("master_pan_law")->load()
                        : 0;
                mVibeGraph.processBus (MixerChannelIds::kRustyDrumsBus, *accum,
                                        bpmForInserts, /*anySolo (ignored)*/ false,
                                        rdPanLaw);
                routeInsertOutput (MixerChannelIds::kRustyDrumsBus, *accum, numSamples);
            }
        }
    }

    // QA-Ea Part B: route Layers/Bass/Drums through the generic path so the
    // serial path matches the MT MasterTask model.  Inserts already fan into
    // these accumulators (PluginProcessor.cpp:1972/2011/2047).
    for (int busChId : { MixerChannelIds::kLayersBus,
                         MixerChannelIds::kBassBus,
                         MixerChannelIds::kDrumsBus })
    {
        auto* accum = mVibeGraph.getChannelAccumulator (busChId);
        if (accum == nullptr || accum->getNumChannels() < 2) continue;
        mVibeGraph.processBus (busChId, *accum, bpmForInserts,
                               /*anySolo unused post-Part-A*/ false, /*panLaw*/ 0);
        routeInsertOutput (busChId, *accum, numSamples);
    }

    // 5F-4b B1b: feed the Layer/Bass/Drums bus accumulators (populated above
    // via routeInsertOutput) into VibeGraph as preRendered inputs. Direct-to-
    // Master routing is picked up inside VibeGraph::processBlock via the kMaster
    // accumulator. The legacy mLayerEngineSum/mBassEngineBuf/mDrumsEngineBuf
    // kind-sum buffers are no longer authoritative - kept compiled for back-compat
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

    // QA-Ea Task 0b (2026-05-18): post-mix recorders + metronome/count-in
    // extracted to applyPostMixRecordAndMetro so the MT branch (early
    // return after dispatchBlock) runs the identical path.  Forks #25.
    applyPostMixRecordAndMetro (buffer, allMidi, pos, numSamples);

    // 2026-05-02: bus drainAndMerge moved to the very end of processBlock
    // (right next to the insert snapshot promotion) so the entire UI-visible
    // meter state updates as one back-to-back block.  See the unified call
    // site at the bottom of this function.

    // 2026-05-02: transport-stopped decay was needed under the old "atomic
    // frozen at last value" model.  Under the new vsync architecture, UI
    // exchange-and-resets the row mirrors each vblank -- when no audio
    // writes them, the mirrors hold -inf (post-exchange).  The DBFSMeter's
    // own UI-thread ballistics decay the displayed value to -60 on its own.
    // No audio-side decay needed.

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
    // 2026-05-07 (Batch 10): extracted into measureDspLoadAndOverload so the
    // MT branch (early return) calls the same path.  Without that, the in-
    // app DSP meter reads 0% under MT.
    measureDspLoadAndOverload (t0, numSamples);

    // 2026-05-02: end-of-audio-block atomic snapshot for ALL meters.
    // 2026-05-07: extracted into drainMeterAtomicsForUI so the MT branch
    // (which returns early after dispatchBlock) can call the same path.
    drainMeterAtomicsForUI();
}

// 2026-05-18 (QA-Ea Task 0b): post-mix recorders + metronome/count-in,
// extracted from the serial tail so the MT branch (early return after
// dispatchBlock) feeds the master + MIDI recorders and runs the
// metronome/count-in identically.  Was serial-tail-only past the early
// return -> 104-byte empty master WAV, no MIDI capture, no metro/count-in
// under MT (Forks #25).  D-5 invariant preserved: MIDI rec -> master rec
// (pre-metronome buffer) -> metronome/count-in.  bpm derives from the
// passed playhead position so the serial + MT call sites can't diverge.
void VibeSynthProcessor::applyPostMixRecordAndMetro (juce::AudioBuffer<float>& buffer,
                                                     const juce::MidiBuffer& allMidi,
                                                     const juce::AudioPlayHead::PositionInfo& pos,
                                                     int numSamples)
{
    const double bpm = pos.getBpm().orFallback (120.0);

    // ── MIDI recording: capture note events sent to the graph this block ─
    if (mMidiRecorder.isRecording())
    {
        double bps = bpm / (60.0 * mSampleRate);
        double beatStart = pos.getPpqPosition().orFallback(0.0);
        mMidiRecorder.processBlock(allMidi, beatStart, bps);
    }

    // QA-Ea Task 0c (FL pre-roll record): accumulate count-in samples while
    // a Record session is active.  The visible Audio clip + MIDI commit
    // later shift content by preRollSamples so the visible clip starts at
    // the song downbeat (not file sample 0) while the WAV still contains
    // the full pre-roll bar -- this is the FL Studio model and avoids the
    // drum-transient slicing of the rejected whole-block-gate proposal.
    // Single global counter applies to master AND every strip block per
    // the Task 0c strip-recorder scope (plan spec line 120).  Gate
    // condition: isRecording() (master OR strips OR midi) AND countInActive
    // -- ensures the counter never ticks during ordinary playback even if
    // a future feature fires countInActive outside of a Record session.
    if (isRecording() && mMetro.countInActive.load(std::memory_order_relaxed))
        mPreRollSamples.fetch_add ((juce::int64) numSamples, std::memory_order_relaxed);

    // 2026-04-26 (D-5 fix): write the master-output recorder BEFORE the
    // metronome adds its click samples to the buffer - otherwise the
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
        // first audible click by a full beat - user heard 3 clicks instead
        // of 4).  Loop continues to fire on each subsequent integer crossing
        // (beats 2, 3, 4, …).  countInBeatsFired tracks accent placement.
        bool ciActive = mMetro.countInActive.load(std::memory_order_relaxed);
        if (!mMetro.countInWasActive && ciActive) {
            mMetro.countInPhase      = 0.0;
            mMetro.lastBeatFloor     = -99999.0;
            mMetro.countInBeatsFired = 1;
            triggerClick(true);   // Beat 1 (always accented) - fires at sample 0.
        }
        mMetro.countInWasActive = ciActive;

        if (ciActive)
        {
            const double bpm = mMetro.countInBpm.load(std::memory_order_relaxed);
            const double bps = juce::jmax(1e-6, bpm / (60.0 * mSampleRate));
            // C.5b: count-in accent honors the CURRENT pattern's intrinsic TS
            // (FL-style - patterns own their own TS).  Falls back to 4 when
            // no pattern.
            const int countInBeatsPerBar = mPatternManager
                ? juce::jmax (1, mPatternManager->currentPattern().tsNum)
                : 4;
            for (int s = 0; s < numSamples; ++s)
            {
                double prevPhase = mMetro.countInPhase;
                mMetro.countInPhase += bps;
                if ((long long)mMetro.countInPhase > (long long)prevPhase)
                {
                    // Crossing into integer N means beat (N+1).  countInBeatsFired
                    // tracks how many beats have fired so far.
                    ++mMetro.countInBeatsFired;
                    triggerClick((mMetro.countInBeatsFired - 1) % countInBeatsPerBar == 0);
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

                    // C.5b: accent on every Nth beat where N = current
                    // pattern's intrinsic numerator (FL-style - pattern owns
                    // its own TS).  Falls back to 4 when no pattern.
                    const int accentEvery = mPatternManager
                        ? juce::jmax (1, mPatternManager->currentPattern().tsNum)
                        : 4;
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
                            triggerClick ((((bf % accentEvery) + accentEvery) % accentEvery) == 0);
                        }
                        float s0 = synthClick();
                        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                            buffer.addSample(ch, s, s0);
                    }
                }
            }
        }
    }
}

// 2026-05-07 (Batch 9c follow-up): UI-meter atomic drain.  Single boundary
// point where every UI-visible peak atomic gets updated, called once per
// processBlock from BOTH the serial tail and the MT branch (right before
// `return;` after dispatchBlock).  Three groups:
//   1. Layers/Bass/Drums/Master bus mirrors -- drained from VibeGraph
//      mirror atomics (which were drained from node atomics earlier).
//   2. Audio rows + AudioClipsBus + FxBus + Vox/Inst (incl. secondary)
//      -- promoted from Run companion atomics (audio CAS-maxes Run during
//      processBlock; promotion lifts Run -> snapshot).
//   3. Inserts in every node + slot atomics in every rack -- promoted by
//      VibeGraph::promoteAllInsertPeakSnapshots().
// All three happen back-to-back so a UI vblank firing anywhere outside
// this small window catches a coherent snapshot across every meter.
void VibeSynthProcessor::drainMeterAtomicsForUI()
{
    constexpr float kPeakNegInf = -std::numeric_limits<float>::infinity();
    auto drainAndMerge = [kPeakNegInf] (std::atomic<float>& mirror, std::atomic<float>& nodeAtom) noexcept
    {
        const float v = nodeAtom.exchange (kPeakNegInf, std::memory_order_relaxed);
        if (v == kPeakNegInf) return;
        float cur = mirror.load (std::memory_order_relaxed);
        while (cur < v && ! mirror.compare_exchange_weak (cur, v, std::memory_order_relaxed))
        {}
    };
    // Group 1: bus mirrors (Layers/Bass/Drums/Master).
    drainAndMerge (mLayersPeakDb,  mVibeGraph.layersPeakDb);
    drainAndMerge (mLayersPeakDbL, mVibeGraph.layersPeakDbL);
    drainAndMerge (mLayersPeakDbR, mVibeGraph.layersPeakDbR);
    drainAndMerge (mBassPeakDb,    mVibeGraph.bassPeakDb);
    drainAndMerge (mBassPeakDbL,   mVibeGraph.bassPeakDbL);
    drainAndMerge (mBassPeakDbR,   mVibeGraph.bassPeakDbR);
    drainAndMerge (mDrumsPeakDb,   mVibeGraph.drumsPeakDb);
    drainAndMerge (mDrumsPeakDbL,  mVibeGraph.drumsPeakDbL);
    drainAndMerge (mDrumsPeakDbR,  mVibeGraph.drumsPeakDbR);
    drainAndMerge (mMasterPeakDb,  mVibeGraph.masterPeakDb);
    drainAndMerge (mMasterPeakDbL, mVibeGraph.masterPeakDbL);
    drainAndMerge (mMasterPeakDbR, mVibeGraph.masterPeakDbR);

    // Group 2: Run -> snapshot promotion for AudioClipsBus / FxBus / Vox /
    // Inst / secondary buses + per-row audio clip mirrors.
    drainAndMerge (mAudioClipsBusPeakDb,  mAudioClipsBusPeakDbRun);
    drainAndMerge (mAudioClipsBusPeakDbL, mAudioClipsBusPeakDbLRun);
    drainAndMerge (mAudioClipsBusPeakDbR, mAudioClipsBusPeakDbRRun);
    drainAndMerge (mFxBusPeakDb,    mFxBusPeakDbRun);
    drainAndMerge (mFxBusPeakDbL,   mFxBusPeakDbLRun);
    drainAndMerge (mFxBusPeakDbR,   mFxBusPeakDbRRun);
    drainAndMerge (mVoxBusPeakDb,   mVoxBusPeakDbRun);
    drainAndMerge (mVoxBusPeakDbL,  mVoxBusPeakDbLRun);
    drainAndMerge (mVoxBusPeakDbR,  mVoxBusPeakDbRRun);
    drainAndMerge (mInstBusPeakDb,  mInstBusPeakDbRun);
    drainAndMerge (mInstBusPeakDbL, mInstBusPeakDbLRun);
    drainAndMerge (mInstBusPeakDbR, mInstBusPeakDbRRun);
    drainAndMerge (mRustyDrumsBusPeakDb,  mRustyDrumsBusPeakDbRun);   // J-7b
    drainAndMerge (mRustyDrumsBusPeakDbL, mRustyDrumsBusPeakDbLRun);  // J-7b
    drainAndMerge (mRustyDrumsBusPeakDbR, mRustyDrumsBusPeakDbRRun);  // J-7b
    drainAndMerge (mVoxBus2PeakDb,  mVoxBus2PeakDbRun);
    drainAndMerge (mVoxBus2PeakDbL, mVoxBus2PeakDbLRun);
    drainAndMerge (mVoxBus2PeakDbR, mVoxBus2PeakDbRRun);
    drainAndMerge (mInstBus2PeakDb, mInstBus2PeakDbRun);
    drainAndMerge (mInstBus2PeakDbL,mInstBus2PeakDbLRun);
    drainAndMerge (mInstBus2PeakDbR,mInstBus2PeakDbRRun);
    drainAndMerge (mInstBus3PeakDb, mInstBus3PeakDbRun);
    drainAndMerge (mInstBus3PeakDbL,mInstBus3PeakDbLRun);
    drainAndMerge (mInstBus3PeakDbR,mInstBus3PeakDbRRun);
    for (int r = 0; r < kMaxAudioRows; ++r)
    {
        drainAndMerge (mAudioRowPeakDb [r], mAudioRowPeakDbRun [r]);
        drainAndMerge (mAudioRowPeakDbL[r], mAudioRowPeakDbLRun[r]);
        drainAndMerge (mAudioRowPeakDbR[r], mAudioRowPeakDbRRun[r]);
    }
    // Group 3: insert atomics + every slot atomic in every rack across all
    // nodes (Layers/Bass/Drums/Master/FxBus/AudioClipsBus + every InsertNode).
    mVibeGraph.promoteAllInsertPeakSnapshots();
}

// 2026-05-07 (Batch 10): DSP-load measurement + overload protection.
// Extracted from end-of-processBlock so the MT branch (returns early after
// dispatchBlock) drives the same path.  Under MT the audio thread isn't
// idle while workers run -- it participates as a worker via
// VibeThreadPool::runUntilOrTimeout, popping + executing tasks itself --
// so wall-clock t1-t0 captures meaningful work time and the meter reads
// LOWER than serial when worker parallelism saves audio-thread time
// (the architectural win).  Voice-stealing on sustained 85% overload
// fires identically under both paths.
void VibeSynthProcessor::measureDspLoadAndOverload (juce::int64 t0Ticks, int numSamples)
{
    const auto   t1       = juce::Time::getHighResolutionTicks();
    const double elapsed  = (double)(t1 - t0Ticks)
                            / (double)juce::Time::getHighResolutionTicksPerSecond();
    const double bufDur   = numSamples / juce::jmax (1.0, mSampleRate);
    // 2026-05-09 (QA-Md): cap raised from 2.f (200%) to 10.f (1000%) after
    // diagnostic capture proved both Debug-MT-on (450%) and Debug-MT-off
    // (870%) sit well above the original 200% cap, masking the true MT-vs-
    // serial gap.  Display side already supports up to 999% via
    // GlobalTransportBar's juce::jlimit(0, 999, ...).  HOLD-FOR-Phase-6-
    // review: V1 release value is a UX call (200/500/1000) deferred to the
    // QA-Audit "Pre-release decisions to revisit" docket -- see Main Plan
    // §5 QA-Audit + Future State CL-291.
    const float  rawLoad  = (bufDur > 0.0)
                                ? juce::jlimit (0.f, 10.f, (float)(elapsed / bufDur))
                                : 0.f;

    // Exponential smoothing - ~80 ms time constant at 512/44100 block rate
    const float prev     = mAudioDspLoad.load (std::memory_order_relaxed);
    const float smoothed = prev * 0.85f + rawLoad * 0.15f;
    mAudioDspLoad.store (smoothed,         std::memory_order_relaxed);
    mDspOverload95.store (smoothed > 0.95f, std::memory_order_relaxed);

    // Sustained 85% detection - accumulate samples while above threshold
    if (smoothed > 0.85f)
        mOverload85Samples += numSamples;
    else
        mOverload85Samples = 0;

    const bool over85 = (mOverload85Samples > (int64_t)(0.5 * mSampleRate));
    mDspOverload85.store (over85, std::memory_order_relaxed);

    if (over85)
    {
        // Steal all synth voices (tail-off = true → release envelopes play,
        // no hard click). DrumSynth + BassSynth one-shot voices decay naturally.
        mSynth.allNotesOff (1, true);
        // Back off the counter so we don't re-trigger every block -
        // next steal can only fire after another 250 ms of sustained overload.
        mOverload85Samples = (int64_t)(0.25 * mSampleRate);
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
        { "mixer_rustybus", [](VibeGraph& g) { return g.getRustyDrumsBusEQ(); } },  // J-6
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
        { VibeGraph::InsertKind::Rusty, "mixer_rusty_", MixerChannelIds::kMaxRustyStrips }, // J-6
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

// §P4.3: Pre-rack EQ sync - mirror of updateAllPostRackEQsFromApvts but uses
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
        { "mixer_rustybus", [](VibeGraph& g) { return g.getRustyDrumsBusPreEQ(); } },  // J-6
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
        { VibeGraph::InsertKind::Rusty, "mixer_rusty_", MixerChannelIds::kMaxRustyStrips }, // J-6
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
    // C.5b (post-revert): Builder grid is uniform 4-beat-per-bar (song-level
    // TS markers are decorative-only).
    constexpr double kBPB = 4.0;

    // 2026-05-06 (Batch 9c B1): build a fresh AudioClipSnapshot + assign it
    // a new monotonic generation.  Atomic-exchanges the publication pointer
    // and retires the OLD snapshot to mClipRetirement so its slow
    // ~AudioClipStreamer (file close + bg-thread unregister) runs on the
    // GC drainer thread instead of here on the message thread.
    auto newSnap = std::make_unique<AudioClipSnapshot>();
    auto& newPlayers = newSnap->players;
    for (int i = 0; i < mPatternManager->getNumBlocks(); ++i)
    {
        const auto& blk = mPatternManager->getBlock(i);
        if (blk.clipType != ClipType::Audio || blk.audioFilePath.isEmpty() || blk.muted)
            continue;
        // NOTE: no isRowAudible() gate here - runtime mute/solo is handled in the
        // live render loop so toggling mute does not require a player rebuild.

        // P4: resolve relative paths like "Samples/kick.wav" against the
        // current project folder.  Absolute paths fall through unchanged
        // (legacy pre-P4 projects stored full paths).
        const auto resolvedFile = resolveProjectFile (blk.audioFilePath);
        std::unique_ptr<juce::AudioFormatReader> rawReader (
            mAudioFormatManager.createReaderFor (resolvedFile));
        if (!rawReader) continue;

        AudioClipPlayer p;
        // QA-Ea Task 0c (2026-05-20 - Option A slip-edit + sub-bar):
        // effectiveStartBeats prefers blk.startBeats when set (sub-bar
        // precision, possibly negative) and falls back to startBar * 4 for
        // every pre-Task-0c block.  The audio loop math at PluginProcessor.cpp
        // :485-785 already handles negative clipStartBeat correctly
        // (outPosInClip = projectStart - clipStart works for clipStart < 0;
        // the (un)played pre-bar portion is naturally skipped by the
        // projectStart >= 0 transport).
        p.clipStartBeat  = effectiveStartBeats (blk);
        // 2026-04-24: prefer block.lengthBeats when set (sub-bar precision
        // from recordings) so playback ends at the real audio end, not the
        // ceil'd bar count.
        p.clipEndBeat    = effectiveStartBeats (blk) + effectiveLengthBeats (blk);
        p.trackRow       = blk.trackRow;
        p.routeChannel   = blk.routeChannel;   // I-16 G-9: Vox/Inst page link
        p.originalBPM    = (blk.originalBPM > 0.f) ? blk.originalBPM : 120.f;
        p.stretchMode    = blk.stretchMode;
        p.fileSampleRate = rawReader->sampleRate;
        // QA-Ea Task 0c (FL pre-roll record + non-destructive clip trim):
        // copy the block's file-position offset to the player so the audio
        // thread can read it without a back-pointer into ArrangementBlock.
        // Component 5 (below) consumes this in the file-position computation
        // sites in renderAudioClipsForRow + renderFilePlayPlayer.
        p.contentStartSamples = blk.contentStartSamples;
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
            // Pre-allocate scratch buffers - sized for worst-case block + PV headroom.
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

    // 2026-05-06 (Batch 9c B1): publish the new snapshot atomically and
    // retire the old to the GC queue.  retiredBeforeGen = newSnap->generation
    // means "the audio thread is safe to free the old once it has loaded
    // a snapshot with gen >= this value" (mirrors the contract documented
    // in Engine/RetirementQueue.h).  fetch_add is relaxed because the
    // synchronization is carried by the exchange's acq_rel below.
    newSnap->generation = mNextClipGen.fetch_add (1, std::memory_order_relaxed) + 1;
    const auto newGen   = newSnap->generation;
    auto* newRaw        = newSnap.release();   // ownership passes into atomic
    auto* oldRaw        = mActiveAudioClips.exchange (newRaw,
                                                       std::memory_order_acq_rel);
    if (oldRaw != nullptr)
        mClipRetirement.retire (std::unique_ptr<AudioClipSnapshot> (oldRaw),
                                 newGen);
}

// ─────────────────────────────────────────────────────────────────────────────
// D3: Choke-group dispatch (audio thread, wait-free).
// ─────────────────────────────────────────────────────────────────────────────
// Build the per-buffer noteOn list, then for each entry whose source insert
// has chokeGroup G > 0, scan all OTHER inserts (across all 3 engine types)
// and inject a noteOff (per-channel allNotesOff) into their buffers at the
// same sample position so the engines silence before consuming the buffer.
//
// Cost: O(buffers × notes × inserts).  In practice trivially small - typical
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
    // 2026-05-06 (Batch 9c B1): try-lock removed -- read the audio-thread
    // snapshot captured at the top of processBlock.  All three sub-loops
    // in this function now read the same snapshot.
    auto& clipPlayers = mCurrentBlockClipSnapshot->players;
    {
        const juce::int64 projectEnd = projectStartSamp + numSamples;
        for (auto& player : clipPlayers)
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
    {
        for (int ci = 0; ci < (int) clipPlayers.size(); ++ci)
        {
            const auto& p = clipPlayers[ci];
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

        // Audio peers - set mutedByChoke=true on others in same group.
        {
            for (int ci = 0; ci < (int) clipPlayers.size(); ++ci)
            {
                if (f.src == ChokeFire::Src::Audio && ci == f.index) continue;
                auto& p = clipPlayers[ci];
                if (p.chokeGroup != f.group) continue;
                p.mutedByChoke = true;
            }
        }
    }
}

void VibeSynthProcessor::updateDrumMixLevels()
{
    // No-op now that DrumSynth is gone - per-drum-tab levels flow through
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
    // QA-Ea Task 0c (FL pre-roll record): zero the pre-roll sample counter
    // at the start of every Record session.  Accumulates count-in samples in
    // applyPostMixRecordAndMetro; drained into RecordResult::preRollSamples
    // by stopRecording for commitRecordingResult's non-destructive clip-trim
    // placement + MIDI Noodling/Early-Strike/quantize rules.
    mPreRollSamples.store (0, std::memory_order_relaxed);

    const auto now = juce::Time::getCurrentTime();
    // Windows-filename-safe: YYYY-MM-DD HH-MM-SS
    const auto ts  = now.formatted ("%Y-%m-%d %H-%M-%S");

    // Always arm MIDI; harmless when Editor ignores the notes in Audio mode.
    mMidiRecorder.startRecording (startBeat);

    if (mode != RecordMode::Audio) return;

    samplesFolder.createDirectory();

    // Scan Vox + Inst strips for _arm on.
    // I-16 G-9 (2026-05-03): Vox strips also spin up a WET recorder for the
    // post-realtime-pitch / pre-vocal-chain tap inside BaySickVocalProcessor.
    // Inst strips have no realtime stage to bake in -> dry only.
    auto scan = [&](const char* prefixBase, int maxCount, int chBase,
                    const char* displayBase, bool isVox)
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
                projectName + " - " + sr.displayName + " - " + ts + " - DRY.wav");
            sr.recorder    = std::make_unique<AudioFileRecorder>();
            if (! sr.recorder->startRecording (sr.file, mSampleRate, 1)) continue;

            // I-16 G-9: Vox-only wet recorder + push pointer to the
            // BaySickVocalProcessor for this page so its processBlock taps
            // post-realtime-pitch audio into the wet file.
            if (isVox && i < kMaxVoxPages)
            {
                sr.wetFile     = samplesFolder.getChildFile (
                    projectName + " - " + sr.displayName + " - " + ts + " - WET.wav");
                sr.wetRecorder = std::make_unique<AudioFileRecorder>();
                if (sr.wetRecorder->startRecording (sr.wetFile, mSampleRate, 1))
                {
                    if (auto* eng = mVoxEngines[i])
                        if (auto* vp = dynamic_cast<BaySickVocalProcessor*> (eng))
                            vp->setWetRecorder (sr.wetRecorder.get());
                }
                else
                {
                    sr.wetRecorder.reset();   // failed to open -> drop wet recording
                }
            }

            mStripRecorders.push_back (std::move (sr));
        }
    };
    scan ("mixer_vox_",  MixerChannelIds::kMaxVoxStrips,
          MixerChannelIds::kVoxBase,  "Vox",  /*isVox=*/true);
    scan ("mixer_inst_", MixerChannelIds::kMaxInstStrips,
          MixerChannelIds::kInstBase, "Inst", /*isVox=*/false);

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
    // QA-Ea Task 0c (FL pre-roll record): drain the pre-roll counter into
    // the result.  exchange(0) leaves the counter clean for the next
    // session so startRecording's defensive zero is belt+suspenders.
    out.preRollSamples = mPreRollSamples.exchange (0, std::memory_order_relaxed);

    if (mMasterRecorder.isRecording())
        out.masterFile = mMasterRecorder.stopRecording();

    for (auto& sr : mStripRecorders)
    {
        // I-16 G-9 (2026-05-03): clear the wet-recorder pointer on the
        // BaySickVocalProcessor BEFORE stopping the writer, so the audio
        // thread can't push another block into a stopped recorder.
        if (sr.wetRecorder)
        {
            const int voxIdx = sr.channelId - MixerChannelIds::kVoxBase;
            if (voxIdx >= 0 && voxIdx < kMaxVoxPages)
                if (auto* eng = mVoxEngines[voxIdx])
                    if (auto* vp = dynamic_cast<BaySickVocalProcessor*> (eng))
                        vp->setWetRecorder (nullptr);
        }

        if (sr.recorder && sr.recorder->isRecording())
        {
            auto f = sr.recorder->stopRecording();
            if (f.existsAsFile())
                out.stripFiles.emplace_back (sr.channelId, f);
        }
        if (sr.wetRecorder && sr.wetRecorder->isRecording())
        {
            auto f = sr.wetRecorder->stopRecording();
            if (f.existsAsFile())
                out.stripWetFiles.emplace_back (sr.channelId, f);
        }
    }
    mStripRecorders.clear();
    return out;
}

// 2026-05-06 (Batch 9b Item 8): dry-recorder tap helper - see header for
// invariants.  Iterates mStripRecorders looking for the matching channel id
// and writes one mono block via AudioFileRecorder::writeBlock (queue-backed,
// drains on the recorder's own background thread - safe for the audio
// thread).  Builds a non-owning AudioBuffer view via const_cast: JUCE's
// AudioBuffer ctor wants non-const float**, but writeBlock takes its
// buffer arg as const ref + only reads.
void VibeSynthProcessor::tapDryRecorder (int channelId,
                                          const float* monoSource,
                                          int numSamples)
{
    if (monoSource == nullptr || numSamples <= 0) return;

    for (auto& sr : mStripRecorders)
    {
        if (sr.channelId != channelId) continue;
        if (! sr.recorder || ! sr.recorder->isRecording()) continue;

        float* monoPtrs[1] = { const_cast<float*> (monoSource) };
        juce::AudioBuffer<float> monoView (monoPtrs, 1, numSamples);
        sr.recorder->writeBlock (monoView);
        return;
    }
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

    // I-3b (2026-05-02): MIDI Learn registry persistence.  Per-project mapping
    // table sits under <MidiCCMappings> as a child of the saved state.  Load
    // path mirrors -- removeChild before APVTS replaceState, then restore.
    state.removeChild(state.getChildWithName(MidiLearnRegistry::kRootTag), nullptr);
    state.addChild(mMidiLearn.saveToValueTree(), -1, nullptr);

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

    // I-3b: extract MIDI Learn mappings before passing to APVTS so the
    // mapping tree doesn't end up living under apvts.state.  If the project
    // has no <MidiCCMappings> child, the registry retains whatever was
    // already in place (e.g., global defaults loaded at app startup).
    auto midiMaps = state.getChildWithName(MidiLearnRegistry::kRootTag);
    if (midiMaps.isValid())
    {
        state.removeChild(midiMaps, nullptr);
        mMidiLearn.loadFromValueTree(midiMaps);
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

    // Processor state (APVTS + rack states) - reuse the same ValueTree we
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

    // PatternManager - patterns, arrangement, piano-roll notes, libraries,
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
    // Relative paths - resolve against current project folder.
    juce::ScopedLock sl (mProjectFolderLock);
    if (mCurrentProjectFolder == juce::File()) return {};
    return mCurrentProjectFolder.getChildFile (storedPath);
}

void VibeSynthProcessor::deserializeProject (const juce::XmlElement& root)
{
    // Processor state - first child under <Processor>.
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

        // Aux strips follow from APVTS param presence - same path as
        // setStateInformation.
        restoreAuxStripsFromState();
    }

    // PatternManager - top-level child named "PatternManager".
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
    // are NOT APVTS-registered - they live in the Harmless preset only).
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

// 2026-04-25: addParamsForBaySickDrums removed - legacy 16-slot drum
// processor deleted; per-drum-tab engines register their own params.

void VibeSynthProcessor::addParamsForTrackEQ(const juce::String& prefix)
{
    // Post-rack EQ (existing behavior - IDs at prefix + "_mid_eq{b}{Suffix}").
    addParamsForEQBank(prefix, juce::String());
}

// §P4.3: Pre-rack EQ.  IDs at prefix + "_preeq_mid_eq{b}{Suffix}" so the
// post-rack and pre-rack banks coexist on the same strip without collision.
void VibeSynthProcessor::addParamsForTrackPreEQ(const juce::String& prefix)
{
    addParamsForEQBank(prefix, "preeq_");
}

// Internal helper - registers an 8-band M/S EQ param bank under prefix +
// "_" + subPrefix + "{mid|side}_eq{b}{Suffix}".
//   subPrefix ""        → post-rack ("EQ" labels)
//   subPrefix "preeq_"  → pre-rack  ("Pre EQ" labels - disambiguates automation menus)
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
            // C.4 follow-up (2026-04-30): default 0 (was -12) so the dotted
            // ghost curve is flat on first Make-Dynamic toggle.  User adjusts
            // the Range slider to dial in downward (-) or upward (+) intent.
            dynF(apvts, ids, bp + "Range",     labelBase + " Range",    -18.f, 18.f, 0.f);
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
    // Message thread only - safe to call APVTS and VibeGraph.
    if (idx >= 0 && idx < kMaxLayerPages && eng != nullptr)
    {
        const juce::String prefix = "mixer_layer_" + juce::String(idx);
        ensureMixerStripParams(prefix, MixerStripKind::Insert, MixerChannelIds::kLayersBus);
        mVibeGraph.ensureInsertNode(VibeGraph::InsertKind::Layer, idx,
                                     "Layer " + juce::String(idx + 1), prefix);

        // Batch 3 (2026-05-06): create + register the multi-threaded render
        // task for this Layer. Dead at runtime while
        // kEnableMultiThreadedEngine is constexpr false; ready when it flips.
        auto task = std::make_unique<EngineInsertTask>(
            eng, EngineInsertTask::Kind::Layer, idx,
            MixerChannelIds::layerInsert(idx), mVibeGraph);
        mRenderDispatcher.registerTask(task.get());
        mLayerRenderTasks[(size_t) idx] = std::move(task);
    }
}
void VibeSynthProcessor::unregisterLayerEngine(int idx)
{
    if (idx < 0 || idx >= kMaxLayerPages) return;
    // Batch 3: tear down the task BEFORE clearing the engine pointer so the
    // dispatcher never sees a task pointing at a dead engine.
    if (mLayerRenderTasks[(size_t) idx])
    {
        mRenderDispatcher.unregisterTask(MixerChannelIds::layerInsert(idx));
        mLayerRenderTasks[(size_t) idx].reset();
    }
    juce::SpinLock::ScopedLockType lk(mLayerEngineLock);
    mLayerEngines[idx] = nullptr;
    // InsertNode retained on purpose - preserves mixer state if the page is re-opened.
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

        // Batch 3 (2026-05-06): MT render task wrapper.
        auto task = std::make_unique<EngineInsertTask>(
            eng, EngineInsertTask::Kind::Bass, pageIdx,
            MixerChannelIds::bassInsert(pageIdx), mVibeGraph);
        mRenderDispatcher.registerTask(task.get());
        mBassRenderTasks[(size_t) pageIdx] = std::move(task);
    }
}
void VibeSynthProcessor::unregisterBassEngine(int pageIdx)
{
    if (pageIdx < 0 || pageIdx >= kMaxBassPages) return;
    if (mBassRenderTasks[(size_t) pageIdx])
    {
        mRenderDispatcher.unregisterTask(MixerChannelIds::bassInsert(pageIdx));
        mBassRenderTasks[(size_t) pageIdx].reset();
    }
    juce::SpinLock::ScopedLockType lk(mBassEngineLock);
    mBassEngines[pageIdx] = nullptr;
}
// 2026-04-25: registerDrumsEngine / unregisterDrumsEngine removed - legacy
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

        // Batch 3 (2026-05-06): MT render task wrapper.
        auto task = std::make_unique<EngineInsertTask>(
            eng, EngineInsertTask::Kind::Drum, pageIdx,
            MixerChannelIds::drumInsert(pageIdx), mVibeGraph);
        mRenderDispatcher.registerTask(task.get());
        mDrumRenderTasks[(size_t) pageIdx] = std::move(task);
    }
    // Recompute fast-path flag (any engine alive?)
    bool any = false;
    for (auto* e : mDrumEngines) if (e) { any = true; break; }
    mAnyDrumPageActive.store(any, std::memory_order_release);
}
void VibeSynthProcessor::unregisterDrumEngine(int pageIdx)
{
    if (pageIdx < 0 || pageIdx >= kMaxDrumPages) return;
    if (mDrumRenderTasks[(size_t) pageIdx])
    {
        mRenderDispatcher.unregisterTask(MixerChannelIds::drumInsert(pageIdx));
        mDrumRenderTasks[(size_t) pageIdx].reset();
    }
    {
        juce::SpinLock::ScopedLockType lk(mDrumEngineLock);
        mDrumEngines[pageIdx] = nullptr;
    }
    bool any = false;
    for (auto* e : mDrumEngines) if (e) { any = true; break; }
    mAnyDrumPageActive.store(any, std::memory_order_release);
    // InsertNode retained - preserves mixer state if drum is re-added.
}

// G-3 (2026-04-28): per-clip-page engine registration.  pageIdx is the audio-
// row index for the bound clip (1:1 mapping to mixer_audio_<row>).  Unlike
// Layer / Bass / Drum engines which create their own InsertNode + mixer
// strip, Clip engines share the existing Audio InsertNode for that row -
// arrangement-playback audio + piano-roll-triggered audio mix into the same
// strip so the user sees one channel per clip rather than two.
void VibeSynthProcessor::registerClipEngine(int pageIdx, juce::AudioProcessor* eng)
{
    if (pageIdx < 0 || pageIdx >= kMaxClipPages) return;
    {
        juce::SpinLock::ScopedLockType lk(mClipEngineLock);
        mClipEngines[pageIdx] = eng;
    }
    bool any = false;
    for (auto* e : mClipEngines) if (e) { any = true; break; }
    mAnyClipPageActive.store(any, std::memory_order_release);

    // QA-0 (2026-05-07): Strategy 1a -- set the Composite's clip-engine
    // pointer on the existing per-row task instead of registering a
    // separate task at the same channel id (which used to lose to
    // most-recent-wins under MT and silence one of the two flows).
    //
    // Defensive: ensure the per-row Composite exists.  ensureAudioInsert
    // is idempotent and creates it if no Builder drop has happened on
    // this row yet (e.g. project-restore that walks Clips tabs before
    // restoreAudioStripsFromArrangement runs).
    ensureAudioInsert (pageIdx, "Audio " + juce::String (pageIdx + 1));
    if (auto& task = mAudioRenderTasks[(size_t) pageIdx])
        task->setClipEngine (eng);
}

void VibeSynthProcessor::unregisterClipEngine(int pageIdx)
{
    if (pageIdx < 0 || pageIdx >= kMaxClipPages) return;

    // QA-0 (2026-05-07): Strategy 1a -- clear the Composite's clip-engine
    // pointer; the per-row Composite stays alive (it still owns the
    // arrangement-clip flow).
    if (auto& task = mAudioRenderTasks[(size_t) pageIdx])
        task->setClipEngine (nullptr);

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

    // Batch 4 (2026-05-06): MT render task wrapper.
    if (eng != nullptr)
    {
        auto task = std::make_unique<VoxStripTask>(
            eng, pageIdx, MixerChannelIds::voxInsert(pageIdx),
            mVibeGraph, *this);
        mRenderDispatcher.registerTask(task.get());
        mVoxRenderTasks[(size_t) pageIdx] = std::move(task);
    }
}

void VibeSynthProcessor::unregisterVoxEngine(int pageIdx)
{
    if (pageIdx < 0 || pageIdx >= kMaxVoxPages) return;

    if (mVoxRenderTasks[(size_t) pageIdx])
    {
        mRenderDispatcher.unregisterTask(MixerChannelIds::voxInsert(pageIdx));
        mVoxRenderTasks[(size_t) pageIdx].reset();
    }

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

    // Batch 4 (2026-05-06): MT render task wrapper.  Source-mode (LiveInput
    // / BaySickGuitars / BaySickBasses) is detected at run time inside the
    // task via the mGuitarsActive / mBassesActive atomics, so a single task
    // instance survives source-mode swaps.
    if (eng != nullptr)
    {
        auto task = std::make_unique<InstStripTask>(
            eng, pageIdx, MixerChannelIds::instInsert(pageIdx),
            mVibeGraph, *this);
        mRenderDispatcher.registerTask(task.get());
        mInstRenderTasks[(size_t) pageIdx] = std::move(task);
    }
}

void VibeSynthProcessor::unregisterInstEngine(int pageIdx)
{
    if (pageIdx < 0 || pageIdx >= kMaxInstPages) return;

    if (mInstRenderTasks[(size_t) pageIdx])
    {
        mRenderDispatcher.unregisterTask(MixerChannelIds::instInsert(pageIdx));
        mInstRenderTasks[(size_t) pageIdx].reset();
    }

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
// are gone - pages now bind their EQ display to the InsertNode / BusNode
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
    // longer registered - pre-rack + post-rack EQs both live on the mixer
    // strip prefix (mixer_{kind}_<N>_preeq_* / _mid_eq*), registered in
    // ensureMixerStripParams.  Every track still gets a 6-slot effect rack.
    addParamsForEffectRack(prefix);
}

void VibeSynthProcessor::unregisterParamsForTrack(const juce::String& trackId)
{
    // JUCE APVTS has no removeParameter API - params stay in the tree but
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
    // Helper lambdas scoped to this call - params are pushed directly via
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
    // APVTS range wasn't - and SliderAttachment auto-overrides the
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
        // omitted - master has no peer to solo against and polarity at the
        // master is rarely useful (and would invert ALL output, easy to
        // mistake for "broken").
        addB(prefix + "_mute", prefix + " Mute", false);
    }

    // FX Bypass on ALL strip types (master/bus/insert) - each has its own rack.
    addB(prefix + "_bypass", prefix + " FX Bypass", false);

    // Batch E #6 (2026-05-01): _arm only meaningful on Vox/Inst inserts
    // (record-arm for live audio capture).  Layer/Bass/Drum/Audio/Aux strips
    // never read it, so registering it on every Insert kind was zombie state
    // bloating presets.
    if (kind == MixerStripKind::Insert
        && (prefix.startsWith("mixer_vox_") || prefix.startsWith("mixer_inst_")))
    {
        addB(prefix + "_arm", prefix + " Arm", false);
    }

    // 5F-4b B1a: routing params - main-out + up to 4 sends
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

    // C.4 Phase 1 (2026-04-30): SC receive lines.  Per Q5=C, every strip can
    // receive up to 4 separate SC signals (white cables in the UI).  Each
    // receive slot stores the SOURCE strip's channel id; -1 = empty.  DSP
    // modules pick which receive line drives them via _sc_pick (per rack
    // slot) or scSourceId (per EQ8 band).  Source signal is the source
    // strip's post-everything tap (Q4=A - final output, post-fader/pan).
    for (int s = 0; s < 4; ++s)
    {
        const juce::String sp = prefix + "_sc_recv" + juce::String(s);
        addI(sp + "_from",    prefix + " SC Recv" + juce::String(s) + " From", -1, 999, -1);
    }

    // D3: choke group - 0 = none, 1..16 = group id.  When two inserts share a
    // group > 0, a noteOn (or audio-clip start) on one chokes all others in
    // the same group.  Inserts only - buses/master have no concept of voices
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
    // R3.5 (2026-04-23): Vox + Inst buses - same shape as Clips/FX bus.
    ensureMixerStripParams("mixer_voxbus",   MixerStripKind::Bus,    kMaster);
    ensureMixerStripParams("mixer_instbus",  MixerStripKind::Bus,    kMaster);
    // G-6 (2026-04-29): secondary Vox/Inst buses - always register params so
    // routing + audio paths work regardless of UI activation state.  Strip
    // visibility on Mixer is a separate flag (see MixerPage::activate*Bus2/3).
    ensureMixerStripParams("mixer_voxbus2",  MixerStripKind::Bus,    kMaster);
    ensureMixerStripParams("mixer_instbus2", MixerStripKind::Bus,    kMaster);
    ensureMixerStripParams("mixer_instbus3", MixerStripKind::Bus,    kMaster);
    // J-5 (2026-05-03): BaySickRustyDrums dedicated bus.  Always register so
    // routing + audio paths work the moment the singleton spawns its 13 strips.
    ensureMixerStripParams("mixer_rustybus", MixerStripKind::Bus,    kMaster);
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

    // Batch 5 (2026-05-06): create the per-row AudioInsertTask if it doesn't
    // exist yet.  No removeAudioInsert hook exists - audio inserts persist
    // for the project lifetime; the unique_ptr cleans up on plugin destroy.
    if (! mAudioRenderTasks[(size_t) row])
    {
        auto task = std::make_unique<CompositeAudioInsertTask>(
            row, MixerChannelIds::audioInsert(row), mVibeGraph, *this);
        mRenderDispatcher.registerTask(task.get());
        mAudioRenderTasks[(size_t) row] = std::move(task);
    }
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

    // Batch 7 (2026-05-06): create the per-aux PassiveStripTask if not yet
    // present.  No removeAuxInsert hook exists - auxes persist for the
    // project lifetime; the unique_ptr cleans up on plugin destroy.
    if (! mAuxRenderTasks[(size_t) idx])
    {
        auto task = std::make_unique<PassiveStripTask>(
            PassiveStripTask::Kind::Aux, idx,
            MixerChannelIds::auxStrip(idx), mVibeGraph, *this);
        mRenderDispatcher.registerTask(task.get());
        mAuxRenderTasks[(size_t) idx] = std::move(task);
    }
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

// J-5 (2026-05-03): BaySickRustyDrums per-strip registration.  Same pattern as
// Vox/Inst but the parent bus is kRustyDrumsBus (the dedicated BaySickRustyDrums
// bus), and there's no live-input param block (these strips receive only from
// the singleton sfizz engine, never from a hardware input).
void VibeSynthProcessor::ensureRustyInsert(int idx, const juce::String& displayName)
{
    if (idx < 0 || idx >= MixerChannelIds::kMaxRustyStrips) return;
    const juce::String prefix = "mixer_rusty_" + juce::String(idx);
    ensureMixerStripParams(prefix, MixerStripKind::Insert, MixerChannelIds::kRustyDrumsBus);
    mVibeGraph.ensureInsertNode(VibeGraph::InsertKind::Rusty, idx,
                                 displayName.isNotEmpty() ? displayName
                                     : ("Rusty " + juce::String(idx + 1)),
                                 prefix);

    // Batch 6 (2026-05-06): create the per-strip RustyInsertTask + synthetic
    // dep on the producer.  Producer is created in loadBaySickRustyDrumsKit
    // BEFORE the ensureRustyInsert loop runs, so it's available here.
    if (! mRustyRenderTasks[(size_t) idx] && mRustyProducerTask)
    {
        auto task = std::make_unique<RustyInsertTask>(
            idx, MixerChannelIds::rustyInsert(idx), mVibeGraph, *this);
        mRenderDispatcher.registerTask(task.get());
        mRenderDispatcher.addSyntheticDep(mRustyProducerTask.get(), task.get());
        mRustyRenderTasks[(size_t) idx] = std::move(task);
    }
}

void VibeSynthProcessor::removeRustyInsert(int idx)
{
    if (idx < 0 || idx >= MixerChannelIds::kMaxRustyStrips) return;

    // Batch 6: tear down the per-strip RustyInsertTask before clearing
    // the InsertNode so the dispatcher never holds a stale task pointer.
    if (mRustyRenderTasks[(size_t) idx])
    {
        mRenderDispatcher.unregisterTask(MixerChannelIds::rustyInsert(idx));
        mRustyRenderTasks[(size_t) idx].reset();
    }

    mVibeGraph.removeInsertNode(VibeGraph::InsertKind::Rusty, idx);
    // APVTS params persist (existing pattern - JUCE doesn't allow unregister).
    // Reset to defaults so a future re-create starts clean.  Common cleanup
    // covers: level (0 dB), pan (centre), mute/solo/polarity/bypass/arm (off).
    const juce::String prefix = "mixer_rusty_" + juce::String(idx);
    auto resetParam = [&](const juce::String& suffix, float defaultVal)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(prefix + suffix)))
            p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(defaultVal));
    };
    resetParam("_level",    0.0f);   // 0 dB
    resetParam("_pan",      0.0f);
    resetParam("_width",    1.0f);
    resetParam("_mute",     0.0f);
    resetParam("_solo",     0.0f);
    resetParam("_polarity", 0.0f);
    resetParam("_bypass",   0.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// J-5: BaySickRustyDrums singleton lifecycle
// ─────────────────────────────────────────────────────────────────────────────

bool VibeSynthProcessor::hasBaySickRustyDrums() const noexcept
{
    return mRustyDrumsActive.load (std::memory_order_acquire);
}

// ─────────────────────────────────────────────────────────────────────────────
// K-2 (2026-05-05): BaySickGuitars per-instance lifecycle.  Up to kMaxInstPages
// instances coexist; each Inst page whose source = BaySickGuitars owns one
// slot.  Mirrors the Rusty pattern but indexed by instIdx instead of singleton.
// ─────────────────────────────────────────────────────────────────────────────

BaySickGuitarsProcessor* VibeSynthProcessor::getBaySickGuitars (int instIdx) noexcept
{
    if (instIdx < 0 || instIdx >= (int) kMaxInstPages) return nullptr;
    return mGuitarsEngine[(size_t) instIdx].get();
}

bool VibeSynthProcessor::loadBaySickGuitarsKit (int instIdx, const juce::File& sfzPath)
{
    if (instIdx < 0 || instIdx >= (int) kMaxInstPages) return false;
    if (! sfzPath.existsAsFile()) return false;

    // Active flag dance (mirrors Rusty's J-7b race fix): keep the slot's flag
    // FALSE during the entire load.  loadSfzFile mutates sfizz internal state
    // for several ms; the audio thread must not call renderBlock against a
    // half-parsed kit.  Flip true only after load completes.
    // K-5 fix #5 (2026-05-05): also flip the engine's per-instance
    // mProcessingEnabled gate so its processBlock early-exits when the audio
    // thread happens to fire while the SFZ load is mutating sfizz hash maps.
    // mGuitarsActive[] alone wasn't enough - the chain processor calls the
    // engine's processBlock directly without checking that flag.
    mGuitarsActive[(size_t) instIdx].store (false, std::memory_order_release);

    {
        const juce::SpinLock::ScopedLockType sl (mGuitarsEngineLock[(size_t) instIdx]);
        if (! mGuitarsEngine[(size_t) instIdx])
        {
            mGuitarsEngine[(size_t) instIdx] = std::make_unique<BaySickGuitarsProcessor> (instIdx);
            mGuitarsEngine[(size_t) instIdx]->prepareToPlay (
                getSampleRate() > 0.0 ? getSampleRate() : 48000.0,
                getBlockSize()  > 0   ? getBlockSize()  : 512);
        }
    }

    if (auto* eng = mGuitarsEngine[(size_t) instIdx].get())
        eng->setProcessingEnabled (false);

    if (! mGuitarsEngine[(size_t) instIdx]->loadKit (sfzPath))
    {
        // Even on failure, re-enable processing so the slot doesn't sit
        // permanently silent (the engine will produce silence from the
        // partially-loaded state until a successful load replaces it).
        if (auto* eng = mGuitarsEngine[(size_t) instIdx].get())
            eng->setProcessingEnabled (true);
        return false;
    }

    // Now safe - engine fully loaded.  Audio thread can begin rendering.
    if (auto* eng = mGuitarsEngine[(size_t) instIdx].get())
        eng->setProcessingEnabled (true);
    mGuitarsActive[(size_t) instIdx].store (true, std::memory_order_release);
    return true;
}

void VibeSynthProcessor::destroyBaySickGuitars (int instIdx)
{
    if (instIdx < 0 || instIdx >= (int) kMaxInstPages) return;

    // Flip active flag BEFORE freeing the engine - audio thread reads the flag
    // first and skips engine access when false.
    mGuitarsActive[(size_t) instIdx].store (false, std::memory_order_release);

    {
        const juce::SpinLock::ScopedLockType sl (mGuitarsEngineLock[(size_t) instIdx]);
        mGuitarsEngine[(size_t) instIdx].reset();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// L-2 (2026-05-05): BaySickBasses per-instance lifecycle.  Same active-flag
// dance + per-slot lock as Guitars above; separate arrays so the two source
// modes don't collide.  Up to kMaxInstPages instances coexist; one Inst page
// whose source = BaySickBasses owns one slot.
// ─────────────────────────────────────────────────────────────────────────────

BaySickBassesProcessor* VibeSynthProcessor::getBaySickBasses (int instIdx) noexcept
{
    if (instIdx < 0 || instIdx >= (int) kMaxInstPages) return nullptr;
    return mBassesEngine[(size_t) instIdx].get();
}

bool VibeSynthProcessor::loadBaySickBassesKit (int instIdx, const juce::File& sfzPath)
{
    if (instIdx < 0 || instIdx >= (int) kMaxInstPages) return false;
    if (! sfzPath.existsAsFile()) return false;

    mBassesActive[(size_t) instIdx].store (false, std::memory_order_release);

    {
        const juce::SpinLock::ScopedLockType sl (mBassesEngineLock[(size_t) instIdx]);
        if (! mBassesEngine[(size_t) instIdx])
        {
            mBassesEngine[(size_t) instIdx] = std::make_unique<BaySickBassesProcessor> (instIdx);
            mBassesEngine[(size_t) instIdx]->prepareToPlay (
                getSampleRate() > 0.0 ? getSampleRate() : 48000.0,
                getBlockSize()  > 0   ? getBlockSize()  : 512);
        }
    }

    if (auto* eng = mBassesEngine[(size_t) instIdx].get())
        eng->setProcessingEnabled (false);

    if (! mBassesEngine[(size_t) instIdx]->loadKit (sfzPath))
    {
        if (auto* eng = mBassesEngine[(size_t) instIdx].get())
            eng->setProcessingEnabled (true);
        return false;
    }

    if (auto* eng = mBassesEngine[(size_t) instIdx].get())
        eng->setProcessingEnabled (true);
    mBassesActive[(size_t) instIdx].store (true, std::memory_order_release);
    return true;
}

void VibeSynthProcessor::destroyBaySickBasses (int instIdx)
{
    if (instIdx < 0 || instIdx >= (int) kMaxInstPages) return;
    mBassesActive[(size_t) instIdx].store (false, std::memory_order_release);
    {
        const juce::SpinLock::ScopedLockType sl (mBassesEngineLock[(size_t) instIdx]);
        mBassesEngine[(size_t) instIdx].reset();
    }
}

BaySickRustyDrumsProcessor* VibeSynthProcessor::getBaySickRustyDrums() noexcept
{
    return mRustyDrumsEngine.get();
}

bool VibeSynthProcessor::loadBaySickRustyDrumsKit (const juce::File& sfzPath)
{
    if (! sfzPath.existsAsFile()) return false;

    // J-7b race fix (2026-05-04): keep mRustyDrumsActive FALSE during the
    // entire load.  loadKit calls mSfizz->loadSfzString which mutates sfizz
    // internal state (regions, voices, output buses) for several seconds;
    // if the audio thread sees active=true mid-load and takes the try-lock
    // between two engine-state writes, it'll call renderBlock against a
    // half-parsed kit and crash inside sfizz.  Only flip active=true AFTER
    // load completes.
    mRustyDrumsActive.store (false, std::memory_order_release);

    // Create the singleton on first call.  Locked because the audio thread
    // may also access mRustyDrumsEngine via getBaySickRustyDrums().
    {
        const juce::SpinLock::ScopedLockType sl (mRustyDrumsEngineLock);
        if (! mRustyDrumsEngine)
        {
            mRustyDrumsEngine = std::make_unique<BaySickRustyDrumsProcessor>();
            mRustyDrumsEngine->prepareToPlay (getSampleRate() > 0.0 ? getSampleRate() : 48000.0,
                                              getBlockSize()  > 0   ? getBlockSize()  : 512);
        }
    }

    // Batch 6 (2026-05-06): create the producer task on first kit load.
    // Producer must exist before ensureRustyInsert runs (it adds a synthetic
    // dep from the producer to each insert task).
    if (! mRustyProducerTask)
    {
        mRustyProducerTask = std::make_unique<RustyDrumsProducerTask>(*this);
        mRenderDispatcher.registerTask(mRustyProducerTask.get());
    }

    if (! mRustyDrumsEngine->loadKit (sfzPath))
        return false;

    // Tear down any existing strips from a prior kit before spawning the new
    // ones - protects against accidental double-creation if the user loads a
    // different kit while the singleton already exists.
    for (int i = 0; i < MixerChannelIds::kMaxRustyStrips; ++i)
    {
        // Batch 6: also unregister the per-strip task so the dispatcher
        // doesn't keep dangling tasks pointing at recycled InsertNodes.
        // Note: mVibeGraph.removeInsertNode is the legacy direct path used
        // here (rather than removeRustyInsert) because removeRustyInsert
        // also resets APVTS params, which we want to preserve across kit
        // reloads.
        if (mRustyRenderTasks[(size_t) i])
        {
            mRenderDispatcher.unregisterTask(MixerChannelIds::rustyInsert(i));
            mRustyRenderTasks[(size_t) i].reset();
        }
        mVibeGraph.removeInsertNode (VibeGraph::InsertKind::Rusty, i);
    }

    // Spawn one strip per discovered channel, in drummer-conventional order.
    const auto& channels = mRustyDrumsEngine->getChannels();
    for (size_t i = 0; i < channels.size() && i < (size_t) MixerChannelIds::kMaxRustyStrips; ++i)
        ensureRustyInsert ((int) i, channels[i].name);

    // J-7b: now safe - the engine is fully loaded, output buses sized,
    // strip InsertNodes registered.  Audio thread can begin rendering.
    mRustyDrumsActive.store (true, std::memory_order_release);
    return true;
}

void VibeSynthProcessor::destroyBaySickRustyDrums()
{
    // Remove all 13 InsertNodes first (audio thread will see empty mRustyInserts
    // immediately even if the engine teardown takes another instant).
    // removeRustyInsert also unregisters the per-strip RustyInsertTask and
    // drops any synthetic deps the dispatcher had pointing at it.
    for (int i = 0; i < MixerChannelIds::kMaxRustyStrips; ++i)
        removeRustyInsert (i);

    // Batch 6 (2026-05-06): drop the producer task too.  Synthetic deps from
    // producer to inserts are already gone (each removeRustyInsert removed
    // its half of the pair); unregisterTask cleans up any stragglers.
    if (mRustyProducerTask)
    {
        mRenderDispatcher.unregisterTask(mRustyProducerTask.get());
        mRustyProducerTask.reset();
    }

    // Then drop the engine.  Audio thread reads mRustyDrumsActive before
    // touching the engine pointer, so flip the active flag before freeing.
    mRustyDrumsActive.store (false, std::memory_order_release);

    {
        const juce::SpinLock::ScopedLockType sl (mRustyDrumsEngineLock);
        mRustyDrumsEngine.reset();
    }

    // Reset bus-level mixer params to defaults so a future re-create starts
    // clean (mixer_rustybus_* params persist as APVTS zombies, harmless).
    auto resetBusParam = [&](const juce::String& suffix, float defaultVal)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter("mixer_rustybus" + suffix)))
            p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(defaultVal));
    };
    resetBusParam("_level",    0.0f);
    resetBusParam("_pan",      0.0f);
    resetBusParam("_width",    1.0f);
    resetBusParam("_mute",     0.0f);
    resetBusParam("_solo",     0.0f);
    resetBusParam("_polarity", 0.0f);
}

void VibeSynthProcessor::resetBaySickRustyDrumsMixerState()
{
    // Walks every `mixer_rusty_*` insert prefix + `mixer_rustybus_*` and
    // resets the standard strip/bus params to the registered defaults.
    // Called when the user switches programs (Full <-> Basic) so the
    // freshly-spawned strips for the new program start clean.
    auto resetParam = [&] (const juce::String& id, float defaultVal)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (defaultVal));
    };

    auto resetStripPrefix = [&] (const juce::String& prefix)
    {
        resetParam (prefix + "_level",    0.0f);
        resetParam (prefix + "_pan",      0.0f);
        resetParam (prefix + "_width",    1.0f);
        resetParam (prefix + "_mute",     0.0f);
        resetParam (prefix + "_solo",     0.0f);
        resetParam (prefix + "_polarity", 0.0f);
        resetParam (prefix + "_bypass",   0.0f);   // Insert-only; no-op for buses
        resetParam (prefix + "_arm",      0.0f);   // Insert-only; no-op for buses
        resetParam (prefix + "_chokeGroup", 0.0f);
        // Sends - `_send{0..3}_to` to -1 (inactive), `_send{0..3}_amount` to 0 dB,
        // `_send{0..3}_prepost` to 0 (post).
        for (int s = 0; s < 4; ++s)
        {
            resetParam (prefix + "_send" + juce::String (s) + "_to",      -1.0f);
            resetParam (prefix + "_send" + juce::String (s) + "_amount",   0.0f);
            resetParam (prefix + "_send" + juce::String (s) + "_prepost",  0.0f);
        }
        // EQ band defaults - every band Bell, 0 dB, freq mid, Q 0.7.
        for (int b = 0; b < 8; ++b)
        {
            for (auto side : { juce::String ("_mid_eq"), juce::String ("_side_eq") })
            {
                const auto bandPrefix = prefix + side + juce::String (b);
                resetParam (bandPrefix + "_freq", 1000.0f);
                resetParam (bandPrefix + "_gain",    0.0f);
                resetParam (bandPrefix + "_q",       0.7f);
                resetParam (bandPrefix + "_type",    0.0f);   // Bell
                resetParam (bandPrefix + "_on",      0.0f);
            }
            for (auto side : { juce::String ("_preeq_mid_eq"), juce::String ("_preeq_side_eq") })
            {
                const auto bandPrefix = prefix + side + juce::String (b);
                resetParam (bandPrefix + "_freq", 1000.0f);
                resetParam (bandPrefix + "_gain",    0.0f);
                resetParam (bandPrefix + "_q",       0.7f);
                resetParam (bandPrefix + "_type",    0.0f);
                resetParam (bandPrefix + "_on",      0.0f);
            }
        }
    };

    // Bus + every potential Rusty insert slot.
    resetStripPrefix ("mixer_rustybus");
    for (int i = 0; i < MixerChannelIds::kMaxRustyStrips; ++i)
        resetStripPrefix ("mixer_rusty_" + juce::String (i));
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
    // J-A2 / B2 (2026-05-04): when the picked input is a stereo pair (e.g.
    // Tascam Model 24's 13/14, 15/16, ...), this is true and the audio thread
    // copies _inputChannelIdx -> strip[L] and _inputChannelIdx+1 -> strip[R]
    // instead of dual-monoing _inputChannelIdx onto both.
    if (apvts.getParameter (prefix + "_inputChannelStereo") == nullptr)
        apvts.createAndAddParameter (std::make_unique<juce::AudioParameterBool> (
            VID(prefix + "_inputChannelStereo"),
            prefix + " Input Stereo", false));
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
