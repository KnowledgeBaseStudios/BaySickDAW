#include "StandaloneApp.h"
#include "../AppPaths.h"
#include "StandaloneEditor.h"
#include "HeavyOperationOverlay.h"
#include "BaySickAssets.h"   // BaySickDAWLogo_png / _pngSize (logo for splash + window icon)
#include "EffectPresetIO.h"  // H-9 prep: seed factory presets at launch
#include "../VibeGraph.h"    // MeterLatencyComp::recomputeFromDevice (2026-05-02)
#include "../ProjectManager.h"            // ProjectManager::getSettingsFile (Batch 10 Phase 3)
#include "../Engine/RenderEngineFlags.h"  // gMultiThreadedEngineEnabled atomic (Batch 10 Phase 3)

#if JUCE_WINDOWS
 #include <windows.h>   // SetPriorityClass (process priority class)
#endif

// ── VibeSynthWindow ───────────────────────────────────────────────────────────
// Subclass so the OS close button actually quits the application.
class VibeSynthWindow : public juce::DocumentWindow
{
public:
    VibeSynthWindow()
        : juce::DocumentWindow("BaySickDAW", juce::Colours::black,
                               juce::DocumentWindow::allButtons)
    {}
    void closeButtonPressed() override
    {
        // P5: route through StandaloneEditor so unsaved-changes prompt fires
        // before the app actually quits.
        if (auto* editor = dynamic_cast<StandaloneEditor*> (getContentComponent()))
        {
            if (! editor->requestAppQuit())
                return;   // editor showing async dialog - will quit on accept
        }
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

    // G1 review fix (D-4): flush held typing-keyboard notes when the app
    // deactivates - Alt+Tab mid-hold sends the key-up to the OTHER app, so
    // keyStateChanged never fires here and the note drones until refocus.
    void activeWindowStatusChanged() override
    {
        juce::DocumentWindow::activeWindowStatusChanged();
        if (! isActiveWindow())
            if (auto* editor = dynamic_cast<StandaloneEditor*> (getContentComponent()))
                editor->releaseAllTypingNotes();
    }
};

// ── PlayHeadAdvancer ──────────────────────────────────────────────────────────
// Wraps AudioProcessorPlayer and advances the standalone playhead PPQ position
// before each audio block so the Sequencer sees time advancing.
//
// J-A2 (2026-05-04): also routes the processor's stereo master output to a
// user-chosen output channel pair (or single channel for mono).  Multi-output
// audio interfaces (e.g. Tascam Model 24, Focusrite 18i20) typically expose
// 8-22 output channels; the user selects which pair the master mix lands on
// via a dropdown in the Audio Settings dialog.  Stored as two atomics so the
// audio thread reads lock-free.
struct PlayHeadAdvancer : public juce::AudioIODeviceCallback
{
    StandalonePlayHead&          playHead;
    juce::AudioProcessorPlayer&  player;
    double                       sampleRate { 44100.0 };

    juce::AudioBuffer<float> mScratchStereo;   // pre-sized in audioDeviceAboutToStart

    PlayHeadAdvancer(StandalonePlayHead& ph, juce::AudioProcessorPlayer& pl)
        : playHead(ph), player(pl) {}

    void audioDeviceAboutToStart(juce::AudioIODevice* d) override
    {
        if (d == nullptr) return;
        sampleRate = d->getCurrentSampleRate();
        if (sampleRate <= 0.0) sampleRate = 44100.0;
        const int blockSize = juce::jmax (32, d->getCurrentBufferSizeSamples());
        mScratchStereo.setSize (2, blockSize, /*keepContent*/false, /*clearExtra*/true,
                                /*avoidReallocating*/false);
        player.audioDeviceAboutToStart(d);
    }
    void audioDeviceStopped() override
    {
        player.audioDeviceStopped();
        // Don't stop the playhead here: advanceBlock() only runs from
        // audioDeviceIOCallbackWithContext(), which JUCE will have already
        // halted before calling audioDeviceStopped(). Calling playHead.stop()
        // here silenced playback after any device switch.
    }

    void audioDeviceIOCallbackWithContext(const float* const* in, int ni,
                                          float* const* out, int no, int n,
                                          const juce::AudioIODeviceCallbackContext& ctx) override
    {
        // J-A2: zero every device output channel so untouched ones go silent
        // even if the player's prior callback left residue.
        for (int c = 0; c < no; ++c)
            if (out[c] != nullptr)
                juce::FloatVectorOperations::clear (out[c], n);

        const int  rawFirst = MasterOutputRouting::gFirstOutputChannel.load (std::memory_order_relaxed);
        const bool isMono   = MasterOutputRouting::gMasterIsMono.load (std::memory_order_relaxed);
        // 2026-05-05 bug fix: clamp the saved channel index so a stereo pair
        // (firstCh + 1) still fits inside the current device's channel count.
        // Without this, a saved value larger than the new device's count (e.g.
        // firstChannel=20 saved on a 22-out ASIO interface, then plugging in
        // a 2-out USB headset) clamped to no-1 → both mapped[0] and mapped[1]
        // collapsed onto out[no-1] → player's L and R both written to the same
        // physical channel → user hears stereo only out of the right speaker.
        const int  maxFirstStereo = juce::jmax (0, no - 2);
        const int  firstCh = isMono
                                 ? juce::jlimit (0, juce::jmax (0, no - 1), rawFirst)
                                 : juce::jlimit (0, maxFirstStereo,         rawFirst);

        if (isMono || no <= 1)
        {
            // Player writes stereo into the scratch; we sum L+R*0.5 into the
            // chosen output channel.  Skips silently if the chosen channel is
            // out of range (firstCh clamped above).
            if (n > mScratchStereo.getNumSamples())
                mScratchStereo.setSize (2, n, false, true, true);   // last-resort grow
            float* scratchPtrs[2] = { mScratchStereo.getWritePointer (0),
                                       mScratchStereo.getWritePointer (1) };
            juce::FloatVectorOperations::clear (scratchPtrs[0], n);
            juce::FloatVectorOperations::clear (scratchPtrs[1], n);
            player.audioDeviceIOCallbackWithContext (in, ni, scratchPtrs, 2, n, ctx);
            if (auto* dst = (firstCh < no) ? out[firstCh] : nullptr)
                for (int s = 0; s < n; ++s)
                    dst[s] = (scratchPtrs[0][s] + scratchPtrs[1][s]) * 0.5f;
        }
        else
        {
            // Stereo: route the player's L/R into the chosen pair.  If
            // firstCh+1 exceeds device channel count, fall back to mono on
            // firstCh (single-channel device or last-channel selection).
            const int secondCh = juce::jmin (firstCh + 1, no - 1);
            float* mapped[2] = {
                out[firstCh],
                (secondCh != firstCh) ? out[secondCh] : out[firstCh]
            };
            player.audioDeviceIOCallbackWithContext (in, ni, mapped, 2, n, ctx);
        }

        playHead.advanceBlock(n, sampleRate);
    }
};

// J-A2 master-output routing globals (declarations live in StandaloneApp.h).
namespace MasterOutputRouting
{
    std::atomic<int>  gFirstOutputChannel { 0 };
    std::atomic<bool> gMasterIsMono       { false };
}

// ── StandalonePlayHead ────────────────────────────────────────────────────────
// QA-Ed: position is an int64 ABSOLUTE sample count (mSamplePos), advanced by
// exact integer addition each block; the loop-wrap is an exact integer-sample
// modulo (no float fmod).  QA-TempoMap: beat is DERIVED from the sample count
// through the stepped sample-indexed timeline (TempoMapRead.h), so multiple
// tempo changes sit at exact sample positions; the float-accumulation drift
// fix from QA-Ed is preserved and the public API stays in beats.

double StandalonePlayHead::deriveBeat (int64_t sample) const
{
    if (TempoMap::isActive())
        return TempoMap::beatAtSample (sample);
    // Pre-first-publish fallback (startup order) - linear on the base tempo.
    const double sr = mSampleRate.load();
    return (double) sample * mBPM.load() / (60.0 * (sr > 0.0 ? sr : 44100.0));
}

double StandalonePlayHead::markerRuleTempoAtBeat (double beat) const
{
    // The base+markers rule: base tempo until the first marker, then each
    // marker's bpm from its beat forward.  Live automation overrides are NOT
    // part of the rule - they enter as explicit tail segments.
    double t = mBPM.load();
    for (const auto& m : mMarkers)
    {
        if (m.first <= beat) t = m.second;
        else break;
    }
    return t;
}

void StandalonePlayHead::rebuildTimeline (double forwardTempoOverride,
                                          bool   rebaseWhilePlaying)
{
    // Message thread only (the timeline's single writer).
    const double  sr  = juce::jmax (1.0, mSampleRate.load());
    const int64_t now = mSamplePos.load();

    static_assert (TempoMap::kMaxSegs >= 2, "timeline needs headroom");
    int64_t smp[TempoMap::kMaxSegs];
    double  bt [TempoMap::kMaxSegs];
    double  bp [TempoMap::kMaxSegs];
    int n = 0;

    const bool   playing    = mPlaying.load();
    const bool   wasActive  = TempoMap::isActive();
    const double curBeatOld = deriveBeat (now);

    if (playing && wasActive && ! rebaseWhilePlaying)
    {
        // Truncate-and-append: mapping already heard stays untouched (history
        // never re-maps mid-play, so the sample clock and every derived beat
        // behind the playhead are stable); the pivot re-states the current
        // position, and everything ahead is rebuilt from the rule.  This is
        // the only writer, so plain reads of the published globals are safe.
        const int oldN = juce::jlimit (1, TempoMap::kMaxSegs,
                                       TempoMap::gCount.load (std::memory_order_relaxed));
        for (int i = 0; i < oldN && n < TempoMap::kMaxSegs - 1; ++i)
        {
            const int64_t s = TempoMap::gSegSample[i].load (std::memory_order_relaxed);
            if (s >= now) break;
            smp[n] = s;
            bt [n] = TempoMap::gSegBeat[i].load (std::memory_order_relaxed);
            bp [n] = TempoMap::gSegBpm [i].load (std::memory_order_relaxed);
            ++n;
        }
        // G1 review fix: coalesce consecutive live-automation pivots.  If the
        // newest kept history segment IS the previous live pivot (no marker
        // landed between it and now), drop it - the new pivot replaces it and
        // the segment count stays "history + one live tail" no matter how
        // long an automation ramp runs.
        if (forwardTempoOverride > 0.0 && n > 1
            && smp[n-1] == mLastLivePivot)
            --n;
        const double fwd = forwardTempoOverride > 0.0 ? forwardTempoOverride
                                                      : markerRuleTempoAtBeat (curBeatOld);
        smp[n] = now; bt[n] = curBeatOld; bp[n] = fwd; ++n;
        mLastLivePivot = (forwardTempoOverride > 0.0) ? now : -1;

        for (const auto& m : mMarkers)
        {
            if (m.first <= curBeatOld) continue;
            if (n >= TempoMap::kMaxSegs) break;
            const int64_t s = smp[n-1] + (int64_t) std::llround (
                (m.first - bt[n-1]) * 60.0 * sr / juce::jmax (1e-6, bp[n-1]));
            smp[n] = s; bt[n] = m.first; bp[n] = m.second; ++n;
        }
    }
    else
    {
        // Pure rebuild from origin: base tempo, then every marker cumulatively.
        smp[0] = 0; bt[0] = 0.0;
        bp[0]  = forwardTempoOverride > 0.0 && ! playing && mMarkers.empty()
                   ? forwardTempoOverride : mBPM.load();
        n = 1;
        for (const auto& m : mMarkers)
        {
            if (n >= TempoMap::kMaxSegs) break;
            if (m.first <= 0.0) { bp[0] = m.second; continue; }   // a bar-1 marker IS the opening tempo
            const int64_t s = smp[n-1] + (int64_t) std::llround (
                (m.first - bt[n-1]) * 60.0 * sr / juce::jmax (1e-6, bp[n-1]));
            smp[n] = s; bt[n] = m.first; bp[n] = m.second; ++n;
        }
        mLastLivePivot = -1;   // pure rebuild discards any live tail
    }

    TempoMap::publish (smp, bt, bp, n, sr);

    if (! playing)
    {
        // Stopped: keep the MUSICAL position (the user sits "at bar N"; a
        // tempo edit must not move their bar) - relocate the sample clock.
        mSamplePos.store (TempoMap::sampleAtBeat (juce::jmax (0.0, curBeatOld)));
    }
    else if (rebaseWhilePlaying)
    {
        // G1 boundary smoke (Jeff): a BASE edit mid-play re-tempos the WHOLE
        // timeline - nothing gets "recorded" at the playhead the way live
        // automation does.  Keep the musical position (same bar, new tempo)
        // by relocating the sample clock, and raise the discontinuity flag so
        // the scheduler + clip players resync in one clean step instead of
        // chasing a position mismatch.  Same accepted message-thread-store
        // pattern as seekTo.
        mSamplePos.store (TempoMap::sampleAtBeat (juce::jmax (0.0, curBeatOld)));
        mSeekDiscontinuity.store (true);
    }
}

void StandalonePlayHead::advanceBlock(int numSamples, double sampleRate)
{
    mSampleRate.store (sampleRate);
    if (!mPlaying.load()) return;

    const int64_t oldPos = mSamplePos.load();
    int64_t pos = oldPos + numSamples;                  // exact integer advance

    const double loopEndBeat = mLoopBeats.load();
    if (loopEndBeat > 0.0)
    {
        // QA-TempoMap: the timeline is an absolute sample<->beat map, so the
        // loop bounds are exact from-origin lookups at ANY number of tempo
        // changes (the QA-Ed relative-anchor dance existed because one anchor
        // re-based away from the origin; the map never does).  Falls back to
        // base-tempo linear math before the first publish.
        int64_t loopEndSamp, loopStartSamp;
        if (TempoMap::isActive())
        {
            loopEndSamp   = TempoMap::sampleAtBeat (loopEndBeat);
            loopStartSamp = TempoMap::sampleAtBeat (mLoopStart.load());
        }
        else
        {
            const double spb = 60.0 * sampleRate / juce::jmax (1e-6, mBPM.load());
            loopEndSamp   = (int64_t) std::llround (loopEndBeat       * spb);
            loopStartSamp = (int64_t) std::llround (mLoopStart.load() * spb);
        }
        const int64_t span = loopEndSamp - loopStartSamp;
        // Exact integer wrap, overshoot PRESERVED (carries into the next pass)
        // so looped content stays sample-locked to absolute time -> no drift.
        if (span > 0 && pos >= loopEndSamp)
        {
            pos = loopStartSamp + ((pos - loopStartSamp) % span);
            // QA-Ee Task 1c: signal the scheduler a loop wrap occurred this block
            // so it flushes any note-off stranded exactly on the loop point in the
            // first post-wrap block (covers the wrap-on-a-block-boundary case the
            // scheduler's straddle test misses).
            mLoopWrapped.store (true);
        }
    }
    mSamplePos.store (pos);
}

void StandalonePlayHead::start()
{
    // G1 review fix: no tempo edit here - see the header note.  Field edits
    // land through onTempoChanged -> setBPM when the editor commits them.
    mPlaying.store (true);
}
void StandalonePlayHead::stop()  { mPlaying.store (false); }
void StandalonePlayHead::reset()
{
    mSamplePos.store (0);
    mSeekDiscontinuity.store (false);
    mLoopWrapped.store (false);   // QA-Ee Task 1c
    mPlaying.store (false);
    rebuildTimeline();            // pure map from origin
}
void StandalonePlayHead::setBPM(double bpm)
{
    // QA-TempoMap: edits the BASE tempo (Jeff's E pick - the field edits the
    // base; markers own their own spans).  G1 boundary smoke: a base edit is
    // a RULE change, not a live event - even mid-play it rebuilds the pure
    // base+markers map from the origin (beat position preserved), so replay
    // never crosses a phantom step at wherever the playhead happened to be.
    if (std::abs (mBPM.load() - bpm) < 1e-9) return;   // value-change guard
    mBPM.store (bpm);
    rebuildTimeline (-1.0, true);
}
void StandalonePlayHead::setLiveTempo(double bpm)
{
    // Tempo-automation write: an explicit tail segment at the playhead; later
    // markers stay in the timeline and re-assert at their samples
    // (last-writer-wins).  Does NOT touch the base tempo.
    bpm = juce::jmax (1.0, bpm);
    // G1 review fix: value-change guard (the standing CPU-safeguard rule) -
    // the 30 Hz applicator fires every tick even on a FLAT lane; identical
    // values must not rebuild/republish at all.
    if (TempoMap::isActive()
        && std::abs (TempoMap::bpmAtSample (mSamplePos.load()) - bpm) < 0.01)
        return;
    rebuildTimeline (bpm);
}
void StandalonePlayHead::setTempoMarkers(std::vector<std::pair<double,double>> markers)
{
    std::sort (markers.begin(), markers.end(),
               [] (const auto& a, const auto& b) { return a.first < b.first; });
    mMarkers = std::move (markers);
    rebuildTimeline();
}
void StandalonePlayHead::seekTo(double beat)
{
    beat = juce::jmax (0.0, beat);
    const double  oldBeat = deriveBeat (mSamplePos.load());
    int64_t s;
    if (TempoMap::isActive())
        s = TempoMap::sampleAtBeat (beat);   // exact through every tempo step
    else
        s = (int64_t) std::llround (beat * 60.0 * mSampleRate.load()
                                    / juce::jmax (1e-6, mBPM.load()));
    mSamplePos.store (s);
    if (beat < oldBeat) mSeekDiscontinuity.store (true);  // backward seek -> scheduler off-flush
}

juce::Optional<juce::AudioPlayHead::PositionInfo> StandalonePlayHead::getPosition() const
{
    PositionInfo info;
    // QA-TempoMap: per-block EFFECTIVE tempo (the segment under the playhead)
    // - every block-rate consumer (BlockContext, strip tasks, clip stretch
    // ratio, metronome) inherits the right value through PositionInfo.
    info.setBpm(TempoMap::isActive() ? TempoMap::bpmAtSample (mSamplePos.load())
                                     : mBPM.load());
    info.setPpqPosition(deriveBeat (mSamplePos.load()));
    info.setTimeInSamples(mSamplePos.load());   // exact integer clock the scheduler shares
    info.setIsPlaying(mPlaying.load());
    info.setIsRecording(mRecording.load());
    info.setTimeSignature (juce::AudioPlayHead::TimeSignature{ mTsNum.load(), mTsDen.load() });
    return info;
}

// ── Audio settings persistence ────────────────────────────────────────────────
juce::File VibesynthStandaloneApp::getAudioSettingsFile()
{
    // P4b (2026-04-23): canonical location moved to Documents\BaySickDAW\.
    // On the FIRST launch with this build, ProjectManager's migration has
    // not yet run (ctor order: app loads device settings BEFORE editor +
    // ProjectManager are created), so we fall back to the legacy Roaming
    // path if the new Documents file doesn't exist yet.  Once migration
    // moves the file, this transparently flips to the Documents path.
    auto neu = AppPaths::appRoot().getChildFile("audio_settings.xml");
    if (neu.existsAsFile()) return neu;
    auto legacy = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                      .getChildFile("BaySickDAW")
                      .getChildFile("audio_settings.xml");
    if (legacy.existsAsFile()) return legacy;
    return neu;   // fresh install - return the new path so save lands there
}

// J-A2 (2026-05-04): master-output routing persistence.  Lives in a sibling
// file of audio_settings.xml so it stays per-machine (different audio
// interfaces per workstation).
juce::File VibesynthStandaloneApp::getMasterOutputFile()
{
    return getAudioSettingsFile().getSiblingFile ("master_output.xml");
}

void VibesynthStandaloneApp::loadMasterOutputRouting()
{
    const auto f = getMasterOutputFile();
    if (! f.existsAsFile()) return;
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName ("MASTEROUT")) return;
    const int  first = xml->getIntAttribute ("firstChannel", 0);
    const bool mono  = xml->getBoolAttribute ("mono", false);
    MasterOutputRouting::gFirstOutputChannel.store (juce::jmax (0, first), std::memory_order_relaxed);
    MasterOutputRouting::gMasterIsMono.store (mono, std::memory_order_relaxed);
}

void VibesynthStandaloneApp::saveMasterOutputRouting()
{
    juce::XmlElement xml ("MASTEROUT");
    xml.setAttribute ("firstChannel",
                      MasterOutputRouting::gFirstOutputChannel.load (std::memory_order_relaxed));
    xml.setAttribute ("mono",
                      MasterOutputRouting::gMasterIsMono.load (std::memory_order_relaxed));
    auto f = getMasterOutputFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (xml.toString());
}

// 2026-05-07 (Batch 10 Phase 3): load + save the Mixer hamburger menu's
// "Multi-core Rendering" preference from Documents/BaySickDAW/settings.xml.
// Stored as <MultiCoreRendering on="0|1"/> child of <BaySickDAWSettings>;
// preserves any other sections (RecentPatternColors, RecentNAMFiles,
// RecentIRFiles, RecentProjects, etc.) the file already contains.  Default
// is true when the key is missing, which matches the constexpr-default we
// shipped in Phase 1 + 2.
void VibesynthStandaloneApp::loadMultiCoreRenderingPref()
{
    const auto f = ProjectManager::getSettingsFile();
    if (! f.existsAsFile()) return;   // first launch -- keep the in-memory default (true)

    auto root = juce::XmlDocument::parse (f);
    if (root == nullptr) return;

    if (auto* node = root->getChildByName ("MultiCoreRendering"))
    {
        const bool on = node->getBoolAttribute ("on", true);
        // release-store pairs with the worker threads' acquire-load in
        // VibeThreadPool::workerLoop (QA-Ef, 2026-05-21).  Called before
        // mDeviceManager->initialise so the very first audio callback already
        // sees the persisted value -- workers either run the graph in parallel
        // (true) or park immediately so the audio thread drains it itself
        // (false; single-core diagnostic).
        RenderEngine::gMultiThreadedEngineEnabled.store (on, std::memory_order_release);
    }
}

void VibesynthStandaloneApp::saveMultiCoreRenderingPref()
{
    const auto f = ProjectManager::getSettingsFile();
    f.getParentDirectory().createDirectory();

    // Read existing root or create a new one -- preserves every other
    // sibling section already in settings.xml (PatternColorPicker writes
    // <RecentPatternColors>, BaySickNAMIREditor writes <RecentNAMFiles> /
    // <RecentIRFiles>, ProjectManager writes <RecentProjects>, etc.).
    std::unique_ptr<juce::XmlElement> root;
    if (f.existsAsFile())
        root = juce::XmlDocument::parse (f);
    if (root == nullptr)
        root = std::make_unique<juce::XmlElement> ("BaySickDAWSettings");

    if (auto* existing = root->getChildByName ("MultiCoreRendering"))
        root->removeChildElement (existing, true);

    auto* node = root->createNewChildElement ("MultiCoreRendering");
    node->setAttribute ("on",
                        RenderEngine::gMultiThreadedEngineEnabled.load (std::memory_order_acquire));

    root->writeTo (f);
}

// QA-L-Fix D-11 (2026-07-19): "MIDI trigger velocity" preference.  Stored as
// <MidiTriggerVelocity fixed="0|1"/> in settings.xml, same sibling-preserving
// pattern as MultiCoreRendering above.  Default (key missing) = From
// controller, matching the in-memory default.
void VibesynthStandaloneApp::loadMidiTriggerVelocityPref()
{
    const auto f = ProjectManager::getSettingsFile();
    if (! f.existsAsFile()) return;

    auto root = juce::XmlDocument::parse (f);
    if (root == nullptr) return;

    if (auto* node = root->getChildByName ("MidiTriggerVelocity"))
        DrumTriggerVelocity::gUseFixed.store (node->getBoolAttribute ("fixed", false),
                                              std::memory_order_release);
}

void VibesynthStandaloneApp::saveMidiTriggerVelocityPref()
{
    const auto f = ProjectManager::getSettingsFile();
    f.getParentDirectory().createDirectory();

    std::unique_ptr<juce::XmlElement> root;
    if (f.existsAsFile())
        root = juce::XmlDocument::parse (f);
    if (root == nullptr)
        root = std::make_unique<juce::XmlElement> ("BaySickDAWSettings");

    if (auto* existing = root->getChildByName ("MidiTriggerVelocity"))
        root->removeChildElement (existing, true);

    auto* node = root->createNewChildElement ("MidiTriggerVelocity");
    node->setAttribute ("fixed",
                        DrumTriggerVelocity::gUseFixed.load (std::memory_order_acquire));

    root->writeTo (f);
}

void VibesynthStandaloneApp::saveAudioSettings()
{
    if (!mDeviceManager) return;
    auto xml = mDeviceManager->createStateXml();
    if (!xml) return;
    auto f = getAudioSettingsFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText(xml->toString());
}

void VibesynthStandaloneApp::changeListenerCallback(juce::ChangeBroadcaster*)
{
    // Defer to next message-loop tick so the device manager finishes switching
    // before we serialize its state. Calling saveAudioSettings() synchronously
    // here could crash if the manager is mid-reinit.
    juce::MessageManager::callAsync([this]
    {
        saveAudioSettings();

        // Update device output latency in the processor so the visual playhead
        // compensation stays accurate after a device change.
        if (mDeviceManager && mProcessor)
            if (auto* dev = mDeviceManager->getCurrentAudioDevice())
            {
                mProcessor->setDeviceOutputLatency (dev->getOutputLatencyInSamples());
                // 2026-05-02: refresh meter latency-comp block count too so
                // the user's hamburger toggle stays accurate across device
                // changes (different driver -> different latency).
                MeterLatencyComp::recomputeFromDevice (dev->getCurrentSampleRate(),
                                                         dev->getCurrentBufferSizeSamples(),
                                                         dev->getOutputLatencyInSamples());
            }
    });
}

// ── VibesynthStandaloneApp ────────────────────────────────────────────────────
void VibesynthStandaloneApp::initialise(const juce::String&)
{
   #if JUCE_WINDOWS
    // Lift the whole process above NORMAL class so audio/render scheduling
    // wins against background apps under load.  ABOVE_NORMAL, not HIGH or
    // REALTIME -- the aggressive classes starve input/compositing and can
    // priority-invert against drivers.  The per-thread boost for the render
    // workers is MMCSS "Pro Audio" in VibeThreadPool::workerLoop.
    SetPriorityClass (GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
   #endif

    // ── Splash screen (2026-04-21) ───────────────────────────────────────────
    // Shows the BaySickDAW logo on launch while the DAW initialises. The
    // juce::SplashScreen is self-managing - it deletes itself after the delay
    // (or on mouse click), so we don't store a pointer.
    const juce::Image logo = juce::ImageCache::getFromMemory(
        BaySickAssets::BaySickDAWLogo_png,
        BaySickAssets::BaySickDAWLogo_pngSize);
    if (logo.isValid())
    {
        // Half-size splash (logo PNG is 1024x1024; rendered at 512x512).
        const juce::Image splashImg = logo.rescaled (
            logo.getWidth()  / 2,
            logo.getHeight() / 2,
            juce::Graphics::highResamplingQuality);
        auto* splash = new juce::SplashScreen ("BaySickDAW", splashImg,
                                                /*useDropShadow=*/true);
        splash->deleteAfterDelay (juce::RelativeTime::seconds (4.0),
                                   /*removeOnMouseClick=*/true);
    }

    // H-9 prep (2026-05-02): seed effect-rack factory presets on every launch.
    // Idempotent -- only writes preset XML files that aren't already on disk,
    // so re-runs are nearly free.  Replaces any user-deleted factory presets.
    // H-10 cutover (2026-05-02): migrate legacy Tape preset folder into
    // Saturation BEFORE seeding -- Tape's My Presets get moved over and
    // its stale Factory dir gets wiped, so the new Saturation/Factory entries
    // ("Tape Vintage" etc) seed cleanly.
    EffectPresetIO::migrateTapeFolderToSaturation();
    EffectPresetIO::seedFactoryPresets();

    mProcessor    = std::make_unique<VibeSynthProcessor>();
    mPlayHead     = std::make_unique<StandalonePlayHead>();
    mProcessor->setPlayHead(mPlayHead.get());
    mProcessor->setSeekDiscontinuityFlag(mPlayHead->getSeekDiscontinuityFlag());   // QA-Ed: backward-seek note-off flush
    mProcessor->setLoopWrappedFlag(mPlayHead->getLoopWrappedFlag());               // QA-Ee Task 1c: loop-seam note-off flush

    mDeviceManager = std::make_unique<juce::AudioDeviceManager>();

    // Load previously saved device settings, or fall back to system defaults.
    // If a pending file exists (written by the audio-settings dialog before a
    // restart), promote it to the live settings file before opening any device.
    auto settingsFile  = getAudioSettingsFile();
    auto pendingFile   = settingsFile.getSiblingFile("audio_settings_pending.xml");
    if (pendingFile.existsAsFile())
        pendingFile.moveFileTo(settingsFile);   // atomic rename; overwrites old file

    // R3 (2026-04-23): bumped input channel request from 0 -> 16 so live-input
    // (Vox / Inst) strips can pull audio from connected ASIO interfaces.  16
    // covers most desktop interfaces (Tascam Model 24, Focusrite 18i20, etc.);
    // larger devices still work because JUCE clamps to the device's actual
    // channel count.  The 2-output count is unchanged (stereo master).
    // J-A2 (2026-05-04): bumped output channel request from 2 -> 64 so the
    // Mixer hamburger's Master Output picker has every channel of a multi-out
    // interface available.  Tascam Model 24 = 22 outs, Focusrite 18i20 = 8 outs,
    // most consumer audio = 2 outs.  JUCE clamps to the device's actual count
    // (request 64 -> get all 22 on the Tascam, all 2 on a USB headset).  Same
    // clamp behavior the input request (16) already relied on.
    // J-A2 / C3 (2026-05-04): two-stage channel-mask override.
    //
    // Stage 1 - strip the saved channel masks BEFORE initialise.  JUCE saves
    // the active-input / active-output BigInteger as `audioDeviceInChans` /
    // `audioDeviceOutChans` (binary strings).  When you switch from a
    // 2-channel device (e.g. laptop mic) to a 22+ channel ASIO interface, the
    // saved bits stay restrictive: JUCE preserves them across the swap, so
    // the new device opens with most of its channels DISABLED.  Removing the
    // attributes forces JUCE to use the defaults inferred from the
    // `numInputChannelsNeeded` / `numOutputChannelsNeeded` initialise() args.
    //
    // Stage 2 - after initialise, write a fresh setup with EVERY channel
    // enabled and call restartLastAudioDevice() so the change actually takes.
    // setAudioDeviceSetup alone sometimes no-ops if JUCE thinks nothing
    // material changed; the explicit restart guarantees the new mask sticks.
    // 2026-05-07 (Batch 10 Phase 3): restore the Mixer hamburger's "Multi-core
    // Rendering" preference BEFORE mDeviceManager->initialise so the very
    // first audio callback already routes via the correct path (MT vs serial).
    // Defaults to true when the key is missing.  Independent of audio device
    // settings -- lives in settings.xml not audio_settings.xml.
    loadMultiCoreRenderingPref();

    // QA-L-Fix D-11: kit-trigger velocity source.  Same file, same timing --
    // the audio thread reads it from the first block onward.
    loadMidiTriggerVelocityPref();

    if (settingsFile.existsAsFile())
    {
        auto xml = juce::XmlDocument::parse(settingsFile);
        if (xml != nullptr)
        {
            xml->removeAttribute ("audioDeviceInChans");
            xml->removeAttribute ("audioDeviceOutChans");
            // G1 boundary fix (2026-07-08, found at the smoke): switching
            // between two ASIO devices leaves the OLD device's name in
            // audioInputDeviceName (the dialog has no input picker and
            // deliberately preserves the prior entry).  ASIO drivers are
            // one-device-per-driver - JUCE cannot open a mismatched in/out
            // pair, so initialise silently fell back to the old device and
            // the user's pick never took (retry loop + pending files piling
            // up).  The post-open net further down only covers the
            // EMPTY-input case, so it never saw this.  Force input = output
            // for ASIO BEFORE opening.
            if (xml->getStringAttribute ("deviceType").equalsIgnoreCase ("ASIO"))
            {
                const auto outName = xml->getStringAttribute ("audioOutputDeviceName");
                if (outName.isNotEmpty()
                    && xml->getStringAttribute ("audioInputDeviceName") != outName)
                    xml->setAttribute ("audioInputDeviceName", outName);
            }
        }
        mDeviceManager->initialise(64, 64, xml.get(), true);
    }
    else
    {
        mDeviceManager->initialiseWithDefaultDevices(64, 64);
    }

    // J-A2 / C3 (2026-05-04): diagnostic log to disk - captures what JUCE
    // actually does with the audio device setup at startup, so we can see
    // why the input mask refuses to enable channels.  Written next to
    // audio_settings.xml as `audio_setup_log.txt`.
    juce::String diagLog;
    diagLog << "Settings file: " << settingsFile.getFullPathName() << "\n";
    diagLog << "Settings exists: " << (settingsFile.existsAsFile() ? "yes" : "no") << "\n\n";

    if (auto* dev = mDeviceManager->getCurrentAudioDevice())
    {
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        mDeviceManager->getAudioDeviceSetup (setup);

        diagLog << "BEFORE setAudioDeviceSetup:\n";
        diagLog << "  type: " << mDeviceManager->getCurrentAudioDeviceType() << "\n";
        diagLog << "  inputDeviceName: '" << setup.inputDeviceName << "'\n";
        diagLog << "  outputDeviceName: '" << setup.outputDeviceName << "'\n";
        diagLog << "  inputChannels: " << setup.inputChannels.toString (2) << "\n";
        diagLog << "  outputChannels: " << setup.outputChannels.toString (2) << "\n";
        diagLog << "  useDefaultInputChannels: " << (setup.useDefaultInputChannels ? "true" : "false") << "\n";
        diagLog << "  useDefaultOutputChannels: " << (setup.useDefaultOutputChannels ? "true" : "false") << "\n";
        diagLog << "  device active inputs:  " << dev->getActiveInputChannels().toString (2) << "\n";
        diagLog << "  device active outputs: " << dev->getActiveOutputChannels().toString (2) << "\n";
        diagLog << "  device input names count:  " << dev->getInputChannelNames().size() << "\n";
        diagLog << "  device output names count: " << dev->getOutputChannelNames().size() << "\n\n";

        // 2026-05-05: gate the J-A2 mask override to ASIO only.  For Windows-mode
        // drivers (DirectSound, WASAPI shared/exclusive, "Windows Audio") the
        // device's natural stereo defaults are already correct - forcing every
        // device channel active causes JUCE to expose 4/6/8 channels in
        // processBlock and the Windows audio mixer can route the stereo write
        // to channels 0+1 onto the wrong physical pair (user-reported bug:
        // every page audible only on the right speaker after this patch
        // landed).  ASIO devices still need the patch because their saved XML
        // typically lacks the input device name + has a zero output mask
        // (input dialog deliberately left untouched).
        const auto driverType = mDeviceManager->getCurrentAudioDeviceType();
        const bool isAsio = driverType.equalsIgnoreCase ("ASIO");

        if (isAsio)
        {
            // For ASIO devices, input and output device names must match.  The
            // settings dialog deliberately leaves audioInputDeviceName alone (to
            // avoid clobbering across configs), so the saved XML may have only the
            // output name.  Force them equal when input is empty.
            if (setup.inputDeviceName.isEmpty() && setup.outputDeviceName.isNotEmpty())
                setup.inputDeviceName = setup.outputDeviceName;

            const int devIns  = dev->getInputChannelNames().size();
            const int devOuts = dev->getOutputChannelNames().size();
            if (devIns > 0)
            {
                juce::BigInteger ins;
                ins.setRange (0, devIns, true);
                setup.inputChannels           = ins;
                setup.useDefaultInputChannels = false;
            }
            if (devOuts > 0)
            {
                juce::BigInteger outs;
                outs.setRange (0, devOuts, true);
                setup.outputChannels           = outs;
                setup.useDefaultOutputChannels = false;
            }
        }
        else
        {
            diagLog << "Non-ASIO driver (" << driverType
                    << ") - leaving channel masks at JUCE defaults.\n\n";
        }

        diagLog << "ATTEMPTING setAudioDeviceSetup with:\n";
        diagLog << "  inputDeviceName: '" << setup.inputDeviceName << "'\n";
        diagLog << "  outputDeviceName: '" << setup.outputDeviceName << "'\n";
        diagLog << "  inputChannels: " << setup.inputChannels.toString (2) << "\n";
        diagLog << "  outputChannels: " << setup.outputChannels.toString (2) << "\n\n";

        const auto err = mDeviceManager->setAudioDeviceSetup (setup, /*treatAsChosenDevice*/ true);
        diagLog << "setAudioDeviceSetup result: '" << (err.isEmpty() ? juce::String ("(ok)") : err) << "'\n\n";

        mDeviceManager->restartLastAudioDevice();

        juce::AudioDeviceManager::AudioDeviceSetup after;
        mDeviceManager->getAudioDeviceSetup (after);
        diagLog << "AFTER restartLastAudioDevice:\n";
        diagLog << "  inputDeviceName: '" << after.inputDeviceName << "'\n";
        diagLog << "  outputDeviceName: '" << after.outputDeviceName << "'\n";
        diagLog << "  inputChannels: " << after.inputChannels.toString (2) << "\n";
        diagLog << "  outputChannels: " << after.outputChannels.toString (2) << "\n";
        if (auto* d2 = mDeviceManager->getCurrentAudioDevice())
        {
            diagLog << "  device active inputs:  " << d2->getActiveInputChannels().toString (2) << "\n";
            diagLog << "  device active outputs: " << d2->getActiveOutputChannels().toString (2) << "\n";
        }
        else
        {
            diagLog << "  device: NULL after restart\n";
        }
    }
    else
    {
        diagLog << "getCurrentAudioDevice() returned null after initialise.\n";
    }

    // G1 boundary diagnostics (2026-07-08, Keep): the log could not answer
    // "was the requested ASIO driver even enumerable at launch?" - JUCE's
    // ASIO createDevice returns null for a name missing from its scan and
    // the manager silently falls back, which looks identical to a settings
    // bug from the outside.  Dump every device type's scan list.
    diagLog << "\nDevice-type scan lists:\n";
    for (auto* type : mDeviceManager->getAvailableDeviceTypes())
    {
        type->scanForDevices();
        diagLog << "  [" << type->getTypeName() << "] outputs:";
        for (const auto& n : type->getDeviceNames (false)) diagLog << " '" << n << "'";
        diagLog << "\n";
    }

    {
        const auto logFile = settingsFile.getSiblingFile ("audio_setup_log.txt");
        logFile.getParentDirectory().createDirectory();
        logFile.replaceWithText (diagLog);
    }

    // J-A2 (2026-05-04): restore the user's master-output channel pick from
    // master_output.xml (sibling of audio_settings.xml).  Done AFTER
    // mDeviceManager->initialise so the chosen index is clamped against the
    // current device's actual channel count later in the audio callback.
    loadMasterOutputRouting();

    // Push initial device output latency into the processor.
    if (auto* dev = mDeviceManager->getCurrentAudioDevice())
    {
        mProcessor->setDeviceOutputLatency (dev->getOutputLatencyInSamples());
        // 2026-05-02: also update the meter latency-compensation block count
        // so vsync-locked meters can align with the audio you actually hear.
        MeterLatencyComp::recomputeFromDevice (dev->getCurrentSampleRate(),
                                                 dev->getCurrentBufferSizeSamples(),
                                                 dev->getOutputLatencyInSamples());
    }

    // Auto-save whenever the user changes device settings via the dialog.
    mDeviceManager->addChangeListener(this);

    // C.3 (2026-04-30): hardware MIDI input.  After initialise reads the saved
    // device-state XML, enumerate all detected MIDI inputs and register the
    // app as a callback for each.  First-launch default = enable all (Q1=B
    // multi-device, all on); subsequent launches respect the saved state by
    // only enabling devices that already had a MIDIINPUT entry in the XML.
    {
        bool anySavedMidiState = false;
        juce::StringArray savedMidiIds;
        if (settingsFile.existsAsFile())
        {
            if (auto savedXml = juce::XmlDocument::parse (settingsFile))
            {
                for (auto* c : savedXml->getChildIterator())
                    if (c->hasTagName ("MIDIINPUT"))
                    {
                        anySavedMidiState = true;
                        savedMidiIds.add (c->getStringAttribute ("identifier"));
                    }
            }
        }

        const auto availableMidi = juce::MidiInput::getAvailableDevices();

        // Stale-state safety net (QA-EffectsReview 2026-07-01): the saved
        // MIDIINPUT set pins enablement, but a device that re-enumerates (USB
        // re-instancing) changes identifier -- the saved entries then match
        // NOTHING available and every input stays silently disabled with no
        // UI hint (owner hit this: FLkey dead until settings were re-applied).
        // If no saved identifier matches any available device, the state is
        // stale -> fall back to enable-all.  A partially-matching set is
        // respected as-is (user intent intact).  Mirrors the ASIO
        // input/output-name net earlier in initialise().
        bool savedMatchesAnyDevice = false;
        for (const auto& d : availableMidi)
            if (savedMidiIds.contains (d.identifier))
            {
                savedMatchesAnyDevice = true;
                break;
            }

        const bool enableAll = ! anySavedMidiState || ! savedMatchesAnyDevice;
        for (const auto& d : availableMidi)
        {
            if (enableAll)
                mDeviceManager->setMidiInputDeviceEnabled (d.identifier, true);
            // Register the callback for every available device.  JUCE only
            // invokes it while the device is enabled, so toggling
            // setMidiInputDeviceEnabled later is sufficient.
            mDeviceManager->addMidiInputDeviceCallback (d.identifier, this);
        }

        // G1 boundary diagnostics (2026-07-08, Keep): append the MIDI
        // enumeration story to audio_setup_log.txt - "(no MIDI devices
        // detected)" in the settings dialog is indistinguishable from a
        // wedged OS MIDI stack without this.
        {
            juce::String midiLog;
            midiLog << "\nMIDI at startup:\n";
            midiLog << "  available (" << availableMidi.size() << "):";
            for (const auto& d : availableMidi)
                midiLog << " '" << d.name << "'";
            midiLog << "\n  saved MIDIINPUT ids: " << savedMidiIds.size()
                    << "  matchesAvailable: " << (savedMatchesAnyDevice ? "yes" : "no")
                    << "  enableAll: " << (enableAll ? "yes" : "no") << "\n";
            for (const auto& d : availableMidi)
                midiLog << "  enabled('" << d.name << "'): "
                        << (mDeviceManager->isMidiInputDeviceEnabled (d.identifier) ? "yes" : "no")
                        << "\n";
            getAudioSettingsFile().getSiblingFile ("audio_setup_log.txt")
                                  .appendText (midiLog);
        }
    }

    // I-3b (2026-05-02): Load the user's global MIDI Learn defaults if the
    // file exists.  Per-project setStateInformation overlays its own mapping
    // table on top later, so per-project mappings win.  No-op if the file
    // doesn't exist (first launch).
    mProcessor->getMidiLearnRegistry().loadGlobalDefaults();

    mPlayer = std::make_unique<juce::AudioProcessorPlayer>();
    mPlayer->setProcessor(mProcessor.get());
    mAdvancer = std::make_unique<PlayHeadAdvancer>(*mPlayHead, *mPlayer);
    mDeviceManager->addAudioCallback(mAdvancer.get());

    mWindow = std::make_unique<VibeSynthWindow>();
    // Window title-bar icon (reuses the embedded splash logo).
    {
        const juce::Image winIcon = juce::ImageCache::getFromMemory(
            BaySickAssets::BaySickDAWLogo_png,
            BaySickAssets::BaySickDAWLogo_pngSize);
        if (winIcon.isValid())
            mWindow->setIcon (winIcon);
    }
    auto* editor = new StandaloneEditor(*mProcessor, *mPlayHead, *mDeviceManager);
    editor->setAudioCallback(mAdvancer.get());   // allows safe unregister/re-register around device switches
    mWindow->setContentOwned(editor, true);

    // QA-Eb (2026-07-08): user-resizable window with a working maximize
    // button; launch stays maximized (unchanged behavior).  No corner
    // resizer widget - border drag, matching the resizable child-window
    // precedents (EventEditor / KeyBinds / UndoHistory).
    mWindow->setResizable(true, false);
    // Floor below which the layout stops being usable: the 40px bar's fixed
    // occupants (controls 520 + pattern 176 + readout 100 + CPU 120 + gaps)
    // need ~1050px before the ribbon guard (>60) kicks in, and the fixed
    // chrome (24+40+26) plus the smallest workable page area needs ~700px
    // of height.  Starting values - Jeff tunes at the G1 boundary smoke.
    //
    // ORDER MATTERS (G1 smoke, the maximize-restore hunt): the limits MUST be
    // installed BEFORE any setFullScreen/restore below.  setResizeLimits runs
    // setBoundsConstrained, and the default constrainer's keep-on-screen
    // clamp nudges a maximized window (which legitimately sits a few px
    // above the screen edge) - that nudge is a real SetWindowPos, and
    // Windows answers it by silently demoting the window from MAXIMIZED to
    // NORMAL placement at the same size.  Every relaunch then came up
    // windowed-almost-full and the next close saved maximized=0.  The
    // first-launch path always had this order right, which is why it never
    // showed the bug.
    mWindow->setResizeLimits(1100, 700, 32000, 32000);
    // QA-Eb follow-up (Jeff, G1 smoke): remember the window size across runs
    // - a maximized close restores maximized, a sized close restores those
    // exact bounds.  Global pref in settings.xml (WindowState child).
    bool windowRestored = false;
    if (auto wsRoot = juce::XmlDocument::parse (ProjectManager::getSettingsFile()))
    {
        if (auto* ws = wsRoot->getChildByName ("WindowState"))
        {
            if (ws->getBoolAttribute ("maximized", true))
            {
                mWindow->setFullScreen (true);
            }
            else
            {
                const juce::Rectangle<int> saved (
                    ws->getIntAttribute ("x", 100),
                    ws->getIntAttribute ("y", 100),
                    juce::jmax (1100, ws->getIntAttribute ("w", 1280)),
                    juce::jmax (700,  ws->getIntAttribute ("h", 800)));

                // QA-ProjectSave (2026-07-26, found by Jeff): a saved position is
                // only valid while the monitor it was saved on still exists.
                // Restoring it unchecked put the window into coordinate space with
                // no display behind it -- the app launched and vanished off the
                // side of the screen, unreachable, and re-attaching monitors did
                // not help because the stale coordinate is what was stored.
                //
                // Require a real overlap with some attached display, not merely a
                // touching edge, so a window that is 99% off-screen is also
                // rejected rather than left almost-unreachable.
                const auto desktop = juce::Desktop::getInstance()
                                        .getDisplays().getTotalBounds (true);
                const auto overlap = desktop.getIntersection (saved);
                const bool reachable = overlap.getWidth()  >= 200
                                    && overlap.getHeight() >= 100;

                if (reachable)
                    mWindow->setBounds (saved);
                else
                    mWindow->setFullScreen (true);   // sane, always-visible fallback
            }
            windowRestored = true;
        }
    }
    if (! windowRestored)
        mWindow->setFullScreen(true);   // first launch / no saved state
    mWindow->setVisible(true);
}

void VibesynthStandaloneApp::shutdown()
{
    // Only auto-save the current device state if the user hasn't written a
    // pending-restart settings file.  If a pending file exists it means the
    // user just clicked Apply+Restart; saving now would overwrite their chosen
    // device with the old one still running in this session.
    auto pendingFile = getAudioSettingsFile().getSiblingFile("audio_settings_pending.xml");
    if (!pendingFile.existsAsFile())
        saveAudioSettings();
    // QA-Eb follow-up: persist the window size/maximized state (global
    // settings.xml, sibling-preserving write like every other pref there).
    if (mWindow)
    {
        const auto f = ProjectManager::getSettingsFile();
        f.getParentDirectory().createDirectory();
        std::unique_ptr<juce::XmlElement> root;
        if (f.existsAsFile())
            root = juce::XmlDocument::parse (f);
        if (root == nullptr)
            root = std::make_unique<juce::XmlElement> ("BaySickDAWSettings");
        if (auto* old = root->getChildByName ("WindowState"))
            root->removeChildElement (old, true);
        auto* ws = root->createNewChildElement ("WindowState");
        // isFullScreen() for an on-desktop window reads the peer's real OS
        // placement (GetWindowPlacement == SW_SHOWMAXIMIZED on Windows), so
        // this save is honest for every maximize gesture.  The historical
        // maximize-restore bug lived in initialise's restore ORDER, not here.
        ws->setAttribute ("maximized", mWindow->isFullScreen());
        const auto wb = mWindow->getBounds();
        ws->setAttribute ("x", wb.getX());
        ws->setAttribute ("y", wb.getY());
        ws->setAttribute ("w", wb.getWidth());
        ws->setAttribute ("h", wb.getHeight());
        root->writeTo (f);
    }
    // Shutdown overlay is owned HERE, parented on the window -- the editor's
    // own overlay dies with the content.  The window stays alive (visible,
    // overlaid) through editor + engine teardown and is destroyed LAST, which
    // kills the black-window period (previously `mWindow = nullptr` ran
    // first, leaving a bare black DocumentWindow on screen while
    // ~StandaloneEditor ground through closeAllDynamicTabs + engine
    // destruction).  The message loop is already stopped at this point;
    // the overlay's own peer paint-pump is what keeps it rendering.
    std::unique_ptr<HeavyOperationOverlay> shutdownOverlay;
    if (mWindow)
    {
        shutdownOverlay = std::make_unique<HeavyOperationOverlay>();
        // Base-class call: ResizableWindow hides addChildComponent behind a
        // Debug-only misuse trap (juce_ResizableWindow.h:376) whose own docs
        // say to call the Component base method when a direct child (not
        // content) is genuinely intended -- which an overlay that must outlive
        // clearContentComponent() is.
        mWindow->Component::addChildComponent (*shutdownOverlay);
        shutdownOverlay->beginOp ("Shutting Down...", true);
        shutdownOverlay->setStepLabel ("Closing tabs and engines...");
        // Editor teardown (the heavy half) happens now, under the overlay.
        mWindow->clearContentComponent();
    }

    if (shutdownOverlay)
        shutdownOverlay->setStepLabel ("Releasing audio device...");
    if (mDeviceManager)
        mDeviceManager->removeChangeListener(this);
    // C.3 (2026-04-30): unregister MIDI input callbacks before tearing down
    // the device manager.  Defensive: ~AudioDeviceManager would clear them
    // anyway, but explicit removal prevents any race against in-flight MIDI
    // thread work during shutdown.
    if (mDeviceManager)
    {
        const auto availableMidi = juce::MidiInput::getAvailableDevices();
        for (const auto& d : availableMidi)
            mDeviceManager->removeMidiInputDeviceCallback (d.identifier, this);
    }
    if (mDeviceManager && mAdvancer)
        mDeviceManager->removeAudioCallback(mAdvancer.get());
    mAdvancer      = nullptr;
    mPlayer        = nullptr;
    mProcessor     = nullptr;
    mPlayHead      = nullptr;
    mDeviceManager = nullptr;

    if (shutdownOverlay)
    {
        shutdownOverlay->endOp();
        shutdownOverlay = nullptr;   // detach from the window before it dies
    }
    mWindow = nullptr;
}

// C.3 (2026-04-30): MIDI input thread -> processor's collector.  Keep this
// short.  NOTE: addMessageToQueue is NOT wait-free (this comment claimed it was
// until 2026-07-19) -- it takes MidiMessageCollector::midiCallbackLock, briefly
// contending with the audio thread's removeNextBlockOfMessages.  That is JUCE's
// design and the hold is tiny; the point stands that no heavy work belongs here.
//
// I-3b (2026-05-02): Also fan the same event into mProcessor's MIDI Learn
// event queue, tagged with the source device's name.  The audio thread
// drains both the live MIDI collector (engine-page routing) and the learn
// queue (CC -> APVTS dispatch) in processBlock.  Two parallel paths: the
// live collector loses device origin (fine for engine-page MIDI), the learn
// queue preserves it (so device-locked mappings can filter).
void VibesynthStandaloneApp::handleIncomingMidiMessage (juce::MidiInput* source,
                                                         const juce::MidiMessage& message)
{
    if (mProcessor == nullptr) return;

    mProcessor->getLiveMidiCollector().addMessageToQueue (message);

    // Only learnable channel-voice messages (CC / pitch-bend / channel
    // pressure) need to enter the learn queue.  Filter here to keep the
    // queue from filling with note-on/off traffic that the registry would
    // discard anyway.
    if (message.isController() || message.isPitchWheel() || message.isChannelPressure())
    {
        const juce::String deviceName = (source != nullptr) ? source->getName() : juce::String();
        mProcessor->getMidiLearnEventQueue().push (deviceName, message);
    }
}

START_JUCE_APPLICATION(VibesynthStandaloneApp)
