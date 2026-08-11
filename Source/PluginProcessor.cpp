#include "PluginProcessor.h"
#include "SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)
#include "SafeAudioReader.h"   // channel/frame sanity gate (QA-Cleanup)
#include "TempoMapRead.h"   // QA-TempoMap: stepped tempo timeline (standalone publishes; VST falls back)
#include "TsMapRead.h"      // QA-G Task 6: stepped time-signature timeline (PatternManager publishes)
#include "G3PlayheadDiag.h" // [G3 BAR1] smoke General-1 dropout reading (Debug-only)
#include "MissingFileReport.h" // QA-Export Task 5: missing external-file collector
#include "ProjectFileResolver.h"
#include "SampleLibrary.h"  // QA-ProjectSave Task 4: stable-root reference resolver
#include "EngineRig.h"      // QA-ModelShell TS1: model-side engine owner
#include "Hosting/PluginManager.h"   // QA-ModelShell TS6: hosted-plugin scan + added list
#include "Standalone/UndoBracket.h"

// QA-Ec x QA-TempoMap seam: audio-clip block boundaries are BEAT-authored, so
// with a published timeline their sample positions must resolve through it -
// a clip sitting past a ruler tempo flag would otherwise drift off the grid
// (linear beat*secPerBeat assumes one tempo).  Falls back to the caller's
// linear math when no timeline is published (legacy VST target).
static inline juce::int64 clipBeatToSample (double beat, double secPerBeat, double sampleRate)
{
    if (TempoMap::isActive())
        return TempoMap::sampleAtBeat (beat);
    return (juce::int64) (beat * secPerBeat * sampleRate);
}

// QA-F: the beat-domain file-position law shared by every audio-clip decode
// path (realtime Paths A + B and the offline channel-composite renderer).
// File consumption per musical beat is fileSR*60/originalBPM in BOTH clip
// modes -- stretch pins it by definition, and resample's tempo-follow term
// cancels -- so the returned position is exact through any number of tempo
// steps.  varispeed = the Stretch-knob rate multiplier (Path A only).
static inline double clipFilePosForBeat (double beatsIntoClip, double fileSampleRate,
                                         double originalBPM, double contentStart,
                                         double varispeed = 1.0)
{
    return contentStart + juce::jmax (0.0, beatsIntoClip)
                          * fileSampleRate * 60.0 * varispeed / originalBPM;
}
// 2026-04-25: BaySickDrumsProcessor include removed - class deleted.
#include "BaySickSynth/BaySickSynthProcessor.h"   // D1.4-fix (c): drum transpose compensation
#include "BaySickRustyDrums/BaySickRustyDrumsProcessor.h"  // J-5: singleton sfizz drum-kit engine
#include "BaySickGuitars/BaySickGuitarsProcessor.h"        // K-2: per-instance sfizz guitar engines
#include "BaySickBasses/BaySickBassesProcessor.h"          // L-2: per-instance sfizz bass engines
#include "BaySickPlayer/BaySickPlayerProcessor.h"       // D1.4-fix (c): drum tune compensation
#include "BaySickVocal/BaySickVocalProcessor.h"   // I-16 G-9: wet recorder hand-off
#include "Standalone/EngineChainProcessor.h"      // QA-Fe2 PDC: Inst strip engine-chain latency hook
#include "DSP/EngineSidechainHelper.h"            // C.4 Phase 2.2: ISidechainEngine for engine-level SC push
#include <thread>                                 // 2026-05-06: hardware_concurrency for render worker count
#include "SafeAudioFormats.h"   // MP3 decode via vendored LAME (QA-Cleanup)
#ifdef VIBESYNTH_VST
  #include "PluginEditor.h"
#endif

// ─────────────────────────────────────────────────────────────────────────────
// ClipSource - cached-PCM / streamed clip reads (CL-281)
//
// The streamed branch forwards verbatim.  The cached branch reproduces
// AudioClipStreamer's RAM-mode reads: that path runs against a ring whose
// capacity IS the file length, so its `pos % mCapacity` is the identity and the
// only real difference here is that the index is used directly.  The 1:1 snap,
// the Catmull-Rom kernel and the EOF breaks are deliberately identical -- a
// clip must not change character because it happened to hit the cache.
// ─────────────────────────────────────────────────────────────────────────────

bool ClipSource::readRaw (juce::AudioBuffer<float>& dest,
                          int destOffset,
                          int numSamples,
                          juce::int64 filePos)
{
    if (mData == nullptr)
        return mStreamer->readRaw (dest, destOffset, numSamples, filePos);

    if (numSamples <= 0)
        return false;
    if (filePos < 0 || filePos + numSamples > mData->lengthInSamples)
        return false;

    const int numDestCh = juce::jmin (dest.getNumChannels(), mData->numChannels);
    for (int ch = 0; ch < numDestCh; ++ch)
        juce::FloatVectorOperations::copy (dest.getWritePointer (ch) + destOffset,
                                           mData->samples.getReadPointer (ch) + (int) filePos,
                                           numSamples);
    return true;
}

float ClipSource::readAndMix (juce::AudioBuffer<float>& dest,
                              int    destOffset,
                              int    numOutputSamples,
                              double fileStartPos,
                              double readRatio,
                              int    numDestChannels,
                              float  gain)
{
    if (mData == nullptr)
        return mStreamer->readAndMix (dest, destOffset, numOutputSamples,
                                      fileStartPos, readRatio, numDestChannels, gain);

    if (numOutputSamples <= 0)
        return 0.0f;

    // At a true 1:1 rate the fractional start is a CONSTANT sub-sample delay,
    // and interpolating for it trades bit-exact playback for a program-
    // dependent interpolation floor.  Snapping to the nearest frame keeps the
    // advance at exactly one frame per sample, so consecutive blocks stay
    // continuous and the exact-copy path below takes over.
    if (readRatio == 1.0)
        fileStartPos = (double) (juce::int64) std::llround (fileStartPos);

    const juce::int64 total      = mData->lengthInSamples;
    const juce::int64 startFloor = (juce::int64) std::floor (fileStartPos);
    // Everything below indexes from startFloor upward, so this is also what
    // keeps every read in bounds.
    if (startFloor < 0 || startFloor >= total)
        return 0.0f;

    const int srcChCount = juce::jmax (1, mData->numChannels);
    float     peak       = 0.0f;

    if (readRatio == 1.0 && fileStartPos == (double) startFloor)
    {
        for (int ch = 0; ch < numDestChannels; ++ch)
        {
            const float* src = mData->samples.getReadPointer (ch % srcChCount);
            for (int i = 0; i < numOutputSamples; ++i)
            {
                const juce::int64 fp = startFloor + i;
                if (fp >= total) break;

                const float v = src[(int) fp] * gain;
                dest.addSample (ch, destOffset + i, v);
                peak = juce::jmax (peak, std::abs (v));
            }
        }
    }
    else
    {
        // 4-point Catmull-Rom for fractional rates (varispeed / tempo-follow /
        // SR conversion).  Edge frames clamp at the file head and EOF.
        for (int ch = 0; ch < numDestChannels; ++ch)
        {
            const float* src = mData->samples.getReadPointer (ch % srcChCount);
            for (int i = 0; i < numOutputSamples; ++i)
            {
                const double      exactFP = fileStartPos + (double) i * readRatio;
                const juce::int64 ip      = (juce::int64) exactFP;
                const float       frac    = (float) (exactFP - (double) ip);

                if (ip + 1 >= total) break;

                const juce::int64 im1 = juce::jmax ((juce::int64) 0, ip - 1);
                const juce::int64 ip2 = juce::jmin (ip + 2, total - 1);

                const float p0 = src[(int) im1];
                const float p1 = src[(int) ip];
                const float p2 = src[(int) (ip + 1)];
                const float p3 = src[(int) ip2];
                const float v  = (p1 + 0.5f * frac * ((p2 - p0)
                              + frac * (2.f * p0 - 5.f * p1 + 4.f * p2 - p3
                              + frac * (3.f * (p1 - p2) + p3 - p0)))) * gain;
                dest.addSample (ch, destOffset + i, v);
                peak = juce::jmax (peak, std::abs (v));
            }
        }
    }

    return peak;
}

void ClipSource::requestSeek (juce::int64 filePos)
{
    if (mStreamer != nullptr)
        mStreamer->requestSeek (filePos);
}

// 2026-04-30 (audit C11+C12) / S-6: the shared per-note expression block, carried as
// standard MIDI so engines stay decoupled from the roll -- CC10 pan, PitchWheel fine
// pitch (±100 cents mapped to ±4096 = ±1 semi at a ±2-semi bend range), CC74 cutoff
// (±2 oct around 0.5), CC71 resonance offset (0.5 neutral), CC72 release-time scale
// (0.5 neutral).  Channel-wide (not MPE), so a chord with mixed per-note values is
// "last note wins" for the channel state -- acceptable for melodic lines.  All five
// emit UNCONDITIONALLY: the old skip-at-neutral shortcut left the previous note's
// channel state in place, so a neutral note inherited the last non-neutral value.
// Used by both a fresh note-on and a RampSlide takeover (S-6); velocity is deliberately
// NOT here -- it rides the noteOn byte, so a takeover (no noteOn) cannot carry it.
// QA-G3Smoke #11 (G-4): panAsRampTarget routes the pan byte to CC89 (ramp
// TARGET -- the voice glides current->target over the slide) instead of the
// instant CC10, for RP takeovers + RT glide note-ons only.  Plain notes keep
// the channel-live CC10 model unchanged.
static void emitNoteExpression (juce::MidiBuffer& dst, const PianoNote& note, int samplePos,
                                bool panAsRampTarget = false)
{
    constexpr int ch = 1;
    const int pan = juce::jlimit (0, 127,
        (int) std::round (64.f + note.panning * 63.f));
    dst.addEvent (juce::MidiMessage::controllerEvent (ch, panAsRampTarget ? 89 : 10, pan), samplePos);

    const int wheel = juce::jlimit (0, 16383,
        (int) std::round (8192.f + note.finePitch * 4096.f));
    dst.addEvent (juce::MidiMessage::pitchWheel (ch, wheel), samplePos);

    const int cutoff = juce::jlimit (0, 127,
        (int) std::round (note.filterCutoff * 127.0f));
    dst.addEvent (juce::MidiMessage::controllerEvent (ch, 74, cutoff), samplePos);

    const int reso = juce::jlimit (0, 127,
        (int) std::round (note.resonance * 127.0f));
    dst.addEvent (juce::MidiMessage::controllerEvent (ch, 71, reso), samplePos);

    const int rel = juce::jlimit (0, 127,
        (int) std::round (note.releaseAmt * 127.0f));
    dst.addEvent (juce::MidiMessage::controllerEvent (ch, 72, rel), samplePos);
}

// Slide/Portamento glides ride CC84 (portamento control = source note) plus an optional
// CC5/CC37 14-bit glide time in ms (slide = glide spans the note; absent for porta =
// engine uses its own glide time).  Voices consume the stash one-shot at the next noteOn.
static void emitPianoNoteOn (juce::MidiBuffer& dst,
                              const PianoNote& note, int samplePos,
                              int glideFromNote = -1, int glideTimeMs = -1,
                              bool panAsRampTarget = false)
{
    constexpr int ch = 1;
    const int vel = juce::jlimit (1, 127, (int) (note.velocity * 127.f));

    emitNoteExpression (dst, note, samplePos, panAsRampTarget);

    if (glideFromNote >= 0)
    {
        if (glideTimeMs >= 0)
        {
            const int t = juce::jlimit (0, 16383, glideTimeMs);
            dst.addEvent (juce::MidiMessage::controllerEvent (ch, 5,  (t >> 7) & 0x7F), samplePos);
            dst.addEvent (juce::MidiMessage::controllerEvent (ch, 37,  t       & 0x7F), samplePos);
        }
        dst.addEvent (juce::MidiMessage::controllerEvent (ch, 84,
                          juce::jlimit (0, 127, glideFromNote)), samplePos);
    }

    dst.addEvent (juce::MidiMessage::noteOn (ch, note.midiNote, (juce::uint8) vel),
                  samplePos);
}

// QA-SlideSliceGlide S-1: glide source for a RetrigSlide/Portamento note = the
// roll's nearest note starting at-or-before this one (a co-starting base note now
// counts, so a slide drawn on top of a held note has a source).  On a co-start
// tie a plain (Standard) note wins over a slide so the base is preferred.
// Returns -1 (no glide) when nothing starts at-or-before this note.
static int findGlideSourcePitch (const std::vector<PianoNote>& notes,
                                 const PianoNote& target)
{
    constexpr double eps = 1.0e-9;
    int    best      = -1;
    double bestBeat  = -1.0e18;
    bool   bestSlide = true;
    for (const auto& n : notes)
    {
        if (&n == &target || n.muted) continue;
        if (n.startBeat > target.startBeat + eps) continue;     // at-or-before only
        // #36 (QA-G3Smoke): an already-ended note can't be a glide source --
        // the pitch must still be sounding INTO the slide's start (this also
        // keeps the S-5 mono-cut from cutting a long-dead voice's pitch).
        if (n.startBeat + n.durationBeats < target.startBeat - eps) continue;
        const bool nSlide = (n.type != NoteType::Standard);
        if (n.startBeat > bestBeat + eps
            || (n.startBeat > bestBeat - eps && bestSlide && ! nSlide))
        {
            bestBeat  = n.startBeat;
            best      = n.midiNote;
            bestSlide = nSlide;
        }
    }
    return best;
}

// QA-H Ramp Slide: anchor of a ramp chain = the first non-RampSlide note found
// walking back through connected predecessors (declared ahead -- the chain-
// duration lineage predicate below calls it).
static const PianoNote* findRampAnchorNote (const std::vector<PianoNote>& notes,
                                            const PianoNote& slide);

// QA-H Ramp Slide: a note's audible length extends through any connected
// chain of RampSlide notes riding it - ramp slides never retrigger, they
// keep the source voice sounding and bend it, so the source's noteOff must
// move to the chain's end.  Connected = each ramp starts at/inside the
// current audible span (butt-joined counts).
// #36 (QA-G3Smoke): lineage predicate -- only absorb a RampSlide whose OWN
// anchor walk resolves to THIS source note, so a butt-joined chain riding a
// different base no longer defers this note's off.
static double rampChainDurationBeats (const std::vector<PianoNote>& notes,
                                      const PianoNote& src)
{
    constexpr double eps = 1.0e-6;
    double end  = src.startBeat + src.durationBeats;
    bool   grew = true;
    while (grew)
    {
        grew = false;
        for (const auto& n : notes)
        {
            if (n.muted || n.type != NoteType::RampSlide) continue;
            if (n.startBeat >= src.startBeat - eps && n.startBeat <= end + eps
                && n.startBeat + n.durationBeats > end
                && findRampAnchorNote (notes, n) == &src)
            {
                end  = n.startBeat + n.durationBeats;
                grew = true;
            }
        }
    }
    return end - src.startBeat;
}

// Returns the anchor NOTE, or null when the chain is broken (nothing would be
// sounding at the slide's start, so the ramp is silent - FL-style takeover
// semantics).
static const PianoNote* findRampAnchorNote (const std::vector<PianoNote>& notes,
                                            const PianoNote& slide)
{
    constexpr double eps = 1.0e-6;
    const PianoNote* cur = &slide;
    for (int hops = 0; hops < 1024; ++hops)
    {
        const PianoNote* prev = nullptr;
        for (const auto& n : notes)
        {
            if (&n == cur || n.muted) continue;
            // S-1: at-or-before cur's start (a co-starting base counts).  Take the
            // latest start; on a co-start tie prefer a non-RampSlide so a base note
            // wins over a sibling ramp and the back-walk terminates.
            if (n.startBeat > cur->startBeat + eps) continue;
            if (prev == nullptr) { prev = &n; continue; }
            if (n.startBeat > prev->startBeat + eps) { prev = &n; continue; }
            if (n.startBeat > prev->startBeat - eps
                && prev->type == NoteType::RampSlide && n.type != NoteType::RampSlide)
                prev = &n;
        }
        if (prev == nullptr) return nullptr;
        if (prev->startBeat + prev->durationBeats < cur->startBeat - eps)
            return nullptr;   // gap - the chain is not sounding here
        if (prev->type != NoteType::RampSlide) return prev;
        cur = prev;
    }
    return nullptr;
}

static int findRampAnchorPitch (const std::vector<PianoNote>& notes,
                                const PianoNote& slide)
{
    const auto* anchor = findRampAnchorNote (notes, slide);
    return anchor != nullptr ? anchor->midiNote : -1;
}

// QA-H Ramp Slide: bend the sounding anchor voice - deliberately NO noteOn.
// CC5/37 glide time + CC84 anchor note + CC85 bend target; the voice whose
// (juce) playing note matches CC84 retargets its pitch over the time, and
// every voice clears the glide stash when CC85 lands so nothing leaks into
// a later noteOn.
static void emitRampSlide (juce::MidiBuffer& dst, const PianoNote& note, int anchorNote,
                           int timeMs, int samplePos)
{
    constexpr int ch = 1;
    // S-6: the takeover carries the slide note's per-note expression too, so pan /
    // cutoff / resonance / release / fine-pitch track the slide (velocity excepted -
    // there is no re-attack to carry it).  #11 (G-4): pan rides CC89 as a ramp
    // TARGET so it glides over the slide instead of jumping.
    emitNoteExpression (dst, note, samplePos, true);
    const int t = juce::jlimit (0, 16383, timeMs);
    dst.addEvent (juce::MidiMessage::controllerEvent (ch, 5,  (t >> 7) & 0x7F), samplePos);
    dst.addEvent (juce::MidiMessage::controllerEvent (ch, 37,  t       & 0x7F), samplePos);
    dst.addEvent (juce::MidiMessage::controllerEvent (ch, 84,
                      juce::jlimit (0, 127, anchorNote)), samplePos);
    // S-6(C): target loudness for the takeover's velocity ramp (base -> slide
    // velocity over the glide time), delivered before CC85 which arms the ramp.
    dst.addEvent (juce::MidiMessage::controllerEvent (ch, 86,
                      juce::jlimit (1, 127, (int) std::round (note.velocity * 127.0f))), samplePos);
    dst.addEvent (juce::MidiMessage::controllerEvent (ch, 85,
                      juce::jlimit (0, 127, note.midiNote)), samplePos);
}

// QA-SlideSampler Task 4: native Bend note (Guitars/Basses).  Plays a normal
// noteOn plus a bend transport - CC87 = amount (64 + signed semitones), CC88 =
// shape (BendShape 0..3), CC5/CC37 = duration ms - that the sfizz processor turns
// into a pitch-wheel ramp scaled to the patch's real bend range.  The in-house
// engines don't map CC87/88 so it degrades to a plain note there.
static void emitBend (juce::MidiBuffer& dst, const PianoNote& note, int timeMs, int samplePos)
{
    constexpr int ch = 1;
    emitNoteExpression (dst, note, samplePos);
    const int t = juce::jlimit (0, 16383, timeMs);
    dst.addEvent (juce::MidiMessage::controllerEvent (ch, 5,  (t >> 7) & 0x7F), samplePos);
    dst.addEvent (juce::MidiMessage::controllerEvent (ch, 37,  t       & 0x7F), samplePos);
    dst.addEvent (juce::MidiMessage::controllerEvent (ch, 87,
                      juce::jlimit (0, 127, 64 + (int) std::lround (note.bendSemitones))), samplePos);
    dst.addEvent (juce::MidiMessage::controllerEvent (ch, 88,
                      juce::jlimit (0, 3, (int) note.bendShape)), samplePos);
    const int vel = juce::jlimit (1, 127, (int) (note.velocity * 127.f));
    dst.addEvent (juce::MidiMessage::noteOn (ch, note.midiNote, (juce::uint8) vel), samplePos);
}

// ── Parameter layout ──────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout
BaySickDAWProcessor::createParameterLayout()
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
    // Read every audio block by each Insert/InstrChannelNode when applying
    // the per-strip _pan param.  Default 0 matches FL's fresh-project default.
    addI("master_pan_law", "Pan Law", 0, 2, 0);

    // QA-Ea Task 0c (FL pre-roll record) + QA-Ee Stage 4 (unified snap scheme):
    // global record-quantize divisor, now on the shared 11-label scheme (Int
    // 0..10, same kUnifiedSnapLabels / snapDivToTicks as Builder + Piano Roll):
    //   0 Off, 1 Line, 2 Bar, 3 Beat, 4 1/2 Beat, 5 1/3 Beat, 6 Step,
    //   7 1/2 Step, 8 1/3 Step, 9 1/4 Step, 10 1/6 Step.
    // Surfaced via "Global Record-Quantize" submenu in the Record-button
    // dropdown in GlobalTransportBar (alongside ASIO / MIDI mode toggles).
    // Read by commitRecordingResult's MIDI commit loop (StandaloneEditor) to
    // snap clamped startBeats to the grid tick AFTER the FL Early-Strike clamp.
    // Off (0) and Line (1) both = no snap (Line has no fixed grid -- there is no
    // zoom canvas at record-commit time -- so raw clamped startBeats are kept).
    // Renamed from `record_quantize_div` (old 0..5 = Off/1/4/1/8/1/16/1/32/1/64);
    // old projects reset this global setting to Off on load (old indices do not
    // map onto the new scheme and Off is the safe default).
    addI("Unified_RecordQuantizeDiv", "Record Quantize Division", 0, 10, 0);

    // QA-Ee Stage 2 (Builder snap): unified snap-division param, Int 0..10 on the
    // shared 11-label scheme (0=Off, 1=Line dynamic, 2=Bar ... 10=1/6 Step).
    // Default 1 = Line (FL-style dynamic grid that locks snap to the visible zoom
    // grid).  Read live by the BuilderPage grid via onGetSnapDiv.
    addI("Unified_BuilderSnapDiv", "Builder Snap Division", 0, 10, 1);

    // QA-Ee Stage 3: GLOBAL Piano Roll snap division (Int 0..10, same 11-label
    // scheme).  ONE param drives EVERY piano roll instance (SC-B global snap) --
    // each grid reads it live via onGetSnapDiv.  Default 1 = Line (matches Builder).
    addI("Unified_PianoRollSnapDiv", "Piano Roll Snap Division", 0, 10, 1);

    // QA-UICleanup Task 4 (Tools > Quantize Settings): GLOBAL quantize division,
    // decoupled from snap (SC10).  ONE param shared across every piano roll + the
    // drum kit (SC16), read live by each grid via onGetQuantizeDiv; the Quantize
    // action rounds the selection to it.  Int 0..3 -> 1/4, 1/8, 1/16, 1/32 note.
    // Default 0 = 1/4.  Per-project (SC11), same as the snap params above.
    addI("Unified_QuantizeDiv", "Quantize Division", 0, 3, 0);

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
int BaySickDAWProcessor::computeRenderWorkerCount() noexcept
{
    const int hw      = (int) std::thread::hardware_concurrency();
    const int desired = hw > 0 ? juce::jmax (1, hw - 1) : 4;
    return juce::jmin (desired, RenderEngine::kMaxWorkers);
}

// ── Constructor / Destructor ──────────────────────────────────────────────────
BaySickDAWProcessor::BaySickDAWProcessor()
    : AudioProcessor(BusesProperties()
        // R3 (2026-04-23): declare an input bus so JUCE feeds the audio
        // device's input channels into the processBlock buffer.
        // J-A2 (2026-05-04): bumped 16 -> 64 to cover Tascam Model 24 (22 in),
        // Behringer X32 (32 in), Yamaha O1V (24 in), and other large interfaces.
        // JUCE clamps to the device's actual input count, so a 2-input USB
        // headset still gets 2 channels - only the upper bound moved.
        .withInput ("Input",  juce::AudioChannelSet::discreteChannels(64), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, &mUndoManager, "BaySickDAWState", createParameterLayout())
{
    // QA-UndoCoverage: stable undo identity for the main APVTS (never
    // re-created, but tagged so its undo entries use the same tag-resolving
    // path as every engine's).
    apvts.undoOwnerTag = "main";

    mEngineRig = std::make_unique<EngineRig> (*this, mUndoManager);

   #if JUCE_DEBUG
    // Starts the message-thread drain that turns the audio thread's POD [G3]
    // records into log lines (G3PlayheadDiag.h).  Constructed here so the
    // Timer's lifetime is the processor's, on the message thread.
    mG3DiagDrainer = std::make_unique<G3PlayheadDiagDrainer>();
   #endif

    // QA-ModelShell TS6: reads plugins.xml at construction (scan folders + the
    // added list), so both are available to project load without any UI.
    mPluginManager = std::make_unique<Hosting::PluginManager>();

    // QA-L-Fix: -1 = no held note-trigger voice.  Value-initialisation would
    // give 0, which is a valid MIDI note.
    mNoteTriggerHeld.fill (-1);

    SafeAudioFormats::registerAll (mAudioFormatManager);  // WAV, AIFF, MP3, OGG, FLAC

    // QA-AudioMeters (2026-05-24): init all 8 per-kind insert peak mirror sets to
    // -60 dB.  These are the UI poll targets; audio thread writes via
    // drainMeterAtomicsForUI's per-kind drainAndMerge loops (which drain
    // mVibeGraph.<kind>InsertPeakDb*[index]); UI vblank exchange-and-resets via
    // drainInsertPeakDbStereo() to start a fresh max-since-last-frame window.
    auto initMirrorArr = [] (auto& arr, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
            arr[i].store (-60.0f, std::memory_order_relaxed);
    };
    initMirrorArr (mAudioRowPeakDbL,    kMaxAudioRows);
    initMirrorArr (mAudioRowPeakDbR,    kMaxAudioRows);
    initMirrorArr (mLayerInsertPeakDbL, kMaxLayerPages);
    initMirrorArr (mLayerInsertPeakDbR, kMaxLayerPages);
    initMirrorArr (mBassInsertPeakDbL,  kMaxBassPages);
    initMirrorArr (mBassInsertPeakDbR,  kMaxBassPages);
    initMirrorArr (mDrumInsertPeakDbL,  kMaxDrumPages);
    initMirrorArr (mDrumInsertPeakDbR,  kMaxDrumPages);
    initMirrorArr (mAuxInsertPeakDbL,   MixerChannelIds::kMaxAuxStrips);
    initMirrorArr (mAuxInsertPeakDbR,   MixerChannelIds::kMaxAuxStrips);
    initMirrorArr (mVoxInsertPeakDbL,   MixerChannelIds::kMaxVoxStrips);
    initMirrorArr (mVoxInsertPeakDbR,   MixerChannelIds::kMaxVoxStrips);
    initMirrorArr (mInstInsertPeakDbL,  MixerChannelIds::kMaxInstStrips);
    initMirrorArr (mInstInsertPeakDbR,  MixerChannelIds::kMaxInstStrips);
    initMirrorArr (mRustyInsertPeakDbL, MixerChannelIds::kMaxRustyStrips);
    initMirrorArr (mRustyInsertPeakDbR, MixerChannelIds::kMaxRustyStrips);

    // QA-AudioMeters: BaySickGraph::kMaxAudioInserts must match this processor's
    // kMaxAudioRows (both = MixerState::kMaxAudioRows).  Static-asserted here
    // since BaySickGraph.cpp doesn't include PluginProcessor.h (circular include).
    static_assert (BaySickGraph::kMaxAudioInserts == kMaxAudioRows,
                   "BaySickGraph::kMaxAudioInserts must equal BaySickDAWProcessor::kMaxAudioRows");

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

    // THREAD SAFETY: no device has been opened, so no audio thread exists to be
    // holding a retired snapshot -- the strongest form of the idle assertion
    // RetirementQueue's CONSUMER-IDLE CONTRACT asks for.  Seeded here rather
    // than defaulted inside the queue so the queue keeps leaking-not-dangling
    // as its default; without it, a session where a device is never opened
    // never publishes a generation and so never frees anything retired.
    setRetirementConsumersIdle (true);
}

BaySickDAWProcessor::~BaySickDAWProcessor()
{
    // The installed resolver captures `this`, so it has to be retired before
    // the object is.  Retiring it here (rather than never) is what stops a
    // late restore path resolving through a dangling processor at shutdown.
    ProjectFileResolver::install ({});

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
void BaySickDAWProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mSampleRate = sampleRate;
    mBlockSize  = samplesPerBlock;


    // C.3 (2026-04-30): reset MIDI input collector for the current SR.  Without
    // this, removeNextBlockOfMessages can't compute correct sample offsets.
    mLiveMidiCollector.reset (sampleRate);
    // Only NOW may the MIDI input thread push into it -- see isLiveMidiReady.
    mLiveMidiReady.store (true, std::memory_order_release);
    // A device is live, so settleAudioThread can expect acknowledgements.  Only
    // a real device open may say so: the offline render path re-prepares this
    // processor twice per render with no device behind it (see
    // mOfflineReconfigureThread).
    if (mOfflineReconfigureThread.load (std::memory_order_acquire)
          != juce::Thread::getCurrentThreadId())
        mAudioDevicePrepared.store (true, std::memory_order_release);

    // THREAD SAFETY: a consumer is about to start publishing generations.  JUCE
    // prepares before the first callback, so clearing idle anywhere in here is
    // strictly ahead of the first setInUseGeneration -- the ordering half of the
    // CONSUMER-IDLE CONTRACT that keeps the drainers from freeing a snapshot the
    // audio thread still holds.
    setRetirementConsumersIdle (false);

    // Idle-suspend hold, re-derived because both terms it depends on are device
    // state (see kIdleSuspendSeconds in the header for why it is a duration).
    // A short final block only makes the realized hold LONGER than the
    // calibration, which is the safe direction -- it suspends later, never
    // earlier.
    kIdleSuspendBlocks.store (
        juce::jmax (1, (int) std::ceil (kIdleSuspendSeconds * sampleRate
                                        / (double) juce::jmax (1, samplesPerBlock))),
        std::memory_order_relaxed);

    // QA-ClipPlayback Task 2's per-clip control filter is prepared when the clip
    // player is BUILT, so it kept whatever rate was live at that moment.
    // StateVariableTPTFilter bakes g = tan(pi*fc/sampleRate) at prepare time and
    // never re-reads the rate, so a filter built at 96k and then run at 44.1k
    // lowpasses at fc*44100/96000 -- bouncing a 96k session to a 44.1k file came
    // out shelved off at ~9.2 kHz with live monitoring clean.  Re-preparing here
    // covers every case that changes the running rate: a device change, the
    // render-config prepare in beginOfflineRender, and its restore in
    // endOfflineRender.
    //
    // THREAD SAFETY: the audio callback is stopped for a device change and
    // suspended + settled for an offline render, so mutating the published
    // snapshot in place is safe here.  The load is deliberately AFTER the
    // consumer-idle clear above, so a concurrent message-thread rebuild cannot
    // retire-and-free this snapshot underneath the sweep.
    if (auto* clipSnap = mActiveAudioClips.load (std::memory_order_acquire))
    {
        const juce::dsp::ProcessSpec clipSpec { sampleRate,
                                                (juce::uint32) juce::jmax (1, samplesPerBlock),
                                                (juce::uint32) 2 };
        for (auto& p : clipSnap->players)
        {
            p.clipFilter.prepare (clipSpec);
            p.clipFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        }
    }

    // R3 (2026-04-23): pre-allocate live-input scratch + snapshot.  Slot
    // scratch is always stereo.  The snapshot is sized for the ACTUAL
    // negotiated input count (AudioProcessorPlayer calls setPlayConfigDetails
    // before prepareToPlay), not a hardcoded stereo: any interface with more
    // than two inputs used to reach the setSize in processBlock, i.e. a malloc
    // in the render callback.
    mLiveInputSlotBuf  .setSize(2, samplesPerBlock, false, true, false);
    mLiveInputSnapshot .setSize(juce::jmax (2, getTotalNumInputChannels()),
                                samplesPerBlock, false, true, false);

    // Audio-thread scratch: capacity established here so the per-block
    // clear()/push_back cycles never call the allocator.
    mPRPendingOffs .reserve (256);
    mPRKeepScratch .reserve (256);
    mChokeFireScratch.reserve (64);

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
    // QA-ModelShell TS2 (2026-07-27): drum engines were NEVER swept here --
    // the old comment claimed page-owners re-prepared them, which was only
    // true at creation time, so a device rate change (and any offline render
    // at a non-device rate) left them at the stale rate.  Model-owned now;
    // swept like every sibling.
    {
        juce::SpinLock::ScopedLockType lk(mDrumEngineLock);
        for (int i = 0; i < kMaxDrumPages; ++i)
            if (mDrumEngines[i]) mDrumEngines[i]->prepareToPlay(sampleRate, samplesPerBlock);
    }
    // G-3 (2026-04-28): re-prepare any registered Clip engines so host SR /
    // block-size changes (e.g. user switches audio device) propagate to
    // BaySickPlayer / BaySickNAM/IR instances owned by ClipsPage tabs.
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
    // Hosted VST3 instruments (BLU-447) were the one registration array missing
    // from this sweep -- a device-rate change or an offline render at a
    // non-device rate left them prepared at the stale rate and block size.
    {
        juce::SpinLock::ScopedLockType lk(mPluginEngineLock);
        for (int i = 0; i < kMaxPluginPages; ++i)
            if (mPluginEngines[i]) mPluginEngines[i]->prepareToPlay(sampleRate, samplesPerBlock);
    }
    // J-7a (2026-05-03): re-prepare the BaySickRustyDrums singleton on
    // host SR / block-size change.  prepareToPlay runs on the message
    // thread (JUCE stops the audio callback before calling it on the
    // processor); no audio-thread race possible here.
    if (mRustyDrumsEngine) mRustyDrumsEngine->prepareToPlay(sampleRate, samplesPerBlock);
    // QA-ModelShell TS2 (2026-07-27): the per-instance sfizz engines had the
    // same never-swept gap as drums above (prepared only at kit load) --
    // closed for device rate changes and offline renders alike.  Per-slot
    // locks match the load/destroy discipline.
    for (int i = 0; i < (int) kMaxInstPages; ++i)
    {
        {
            const juce::SpinLock::ScopedLockType sl (mGuitarsEngineLock[(size_t) i]);
            if (mGuitarsEngine[(size_t) i])
                mGuitarsEngine[(size_t) i]->prepareToPlay(sampleRate, samplesPerBlock);
        }
        {
            const juce::SpinLock::ScopedLockType sl (mBassesEngineLock[(size_t) i]);
            if (mBassesEngine[(size_t) i])
                mBassesEngine[(size_t) i]->prepareToPlay(sampleRate, samplesPerBlock);
        }
    }
    mVibeGraph.prepare(sampleRate, samplesPerBlock);

    // Build the fixed bus topology the first time; no-op on subsequent calls.
    // §P4.3 B7: the drums bus node owns its own preEq member, sync'd via
    // updateAllPreRackEQsFromApvts from mixer_drums_preeq_*.
    mVibeGraph.buildFixedTopology(apvts);

    ensureMixerBusAndMasterParams();
    // QA-G3Smoke Swing (SW-6): global + per-player swing params + cached atomics.
    ensureSwingParams();
    // 5F-4a Batch 6: cache APVTS pointers in bus + master nodes (needs params registered).
    mVibeGraph.rebindBusApvts();

    // 2026-05-05 dirty-flag wiring: route every BaySickGraph rack's lifecycle
    // events into the editor's project-dirty hook the same way main-APVTS
    // edits do.  Effects-page slot type swap / move-up/down / clear / bypass
    // doesn't write apvts, so without this rack lifecycle slips past the
    // dirty listener.
    mVibeGraph.onAnyRackChanged = [this]
    {
        if (onAnyStateChange) onAnyStateChange();
    };
    mVibeGraph.rebindAllRackHooks();

    // QA-Fe2 PDC: the graph queries each Vox strip's engine-side vocal chain
    // latency (De-reverb / spectral De-esser FFT frames) through this hook so
    // updateBusLatencies can compensate it like any bus rack.
    mVibeGraph.onGetVoxStripChainLatency = [this](int voxIdx) -> int
    {
        if (auto* vp = dynamic_cast<BaySickVocalProcessor*>(voxEngineAt(voxIdx)))
            return vp->getChainLatencySamples();
        return 0;
    };

    // QA-Fe2 PDC full-graph pass: Inst analog -- the strip engine is an
    // EngineChainProcessor (sfizz -> Pedals -> NAM/IR) whose NAM/IR stage
    // reports oversampling latency.  Message thread (same serialization as
    // register/unregisterInstEngine), so no mInstEngineLock needed here.
    mVibeGraph.onGetInstStripEngineLatency = [this](int instIdx) -> int
    {
        if (instIdx < 0 || instIdx >= (int) kMaxInstPages) return 0;
        if (auto* chain = dynamic_cast<EngineChainProcessor*>(mInstEngines[(size_t) instIdx]))
            return chain->getChainLatencySamples();
        return 0;
    };

    // Compute initial PDC and report to host.
    setLatencySamples(mVibeGraph.updateBusLatencies());

    // ── Multi-threaded render engine (Phase 1 scaffolding, 2026-05-06) ───────
    // Resize the cache-aligned arena to fit the new block size and clear any
    // stale tasks from the pool's queues. The pool itself is NOT recreated -
    // workers persist for the plugin's lifetime per the lifetime contract.
    // Sample-rate / block-size changes only touch the arena views.
    mRenderArena.prepare (samplesPerBlock);
    mRenderDispatcher.prepare (sampleRate, samplesPerBlock);
    mRenderPool.clearQueues();

    // Batch 7 (2026-05-06): register the always-on bus PassiveStripTasks --
    // kNumBatch7Buses of them -- here
    // (after buildFixedTopology so BaySickGraph bus nodes exist).
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
        MixerChannelIds::kPluginsBus,       // QA-ModelShell TS6
        MixerChannelIds::kLayersBus2,       // QA-Layout T10
        MixerChannelIds::kBassBus2,
        MixerChannelIds::kClipsBus2,
        MixerChannelIds::kPluginsBus2,
        MixerChannelIds::kDrumsBus2,        // QA-SOUNDNESS
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

void BaySickDAWProcessor::releaseResources()
{
    // The device has stopped calling processBlock, so no settle can be
    // acknowledged from here on.  Without this clear, teardown after a device
    // stop waits out its whole timeout on every gesture.
    mAudioDevicePrepared.store (false, std::memory_order_release);

    // THREAD SAFETY: JUCE calls this only once the device has stopped invoking
    // the audio callback, and nothing can invoke it again before prepareToPlay,
    // so no thread can still be holding a retired snapshot.
    setRetirementConsumersIdle (true);
}

// Both deferred-destruction queues gate on generations published by THIS
// processor's audio thread, so the owner-side idle assertion tracks the device
// lifecycle: asserted only where the callback provably cannot run, cleared
// before it can run again (Engine/RetirementQueue.h, CONSUMER-IDLE CONTRACT).
// Message thread only -- setConsumerIdle takes the drainer's mutex.
void BaySickDAWProcessor::setRetirementConsumersIdle (bool consumerIsIdle)
{
    mClipRetirement.setConsumerIdle (consumerIsIdle);

    // The roll queue is PatternManager's, and the audio thread reaches it only
    // through this pointer, so a null one means no consumer can have touched it
    // yet; setPatternManager seeds it against the device state on arrival.
    if (mPatternManager != nullptr)
        mPatternManager->setRollConsumerIdle (consumerIsIdle);
}

bool BaySickDAWProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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

// renderAudioClipsForRow: decode every NON-FilePlay audio clip whose trackRow
// matches `row` and sum its RAW output into the row's block buffer (mtDest,
// always non-null).  Called per audio row by CompositeAudioInsertTask::run.
// QA-MultiBlockHazard: the insert chain (rack / EQ / fader) is NOT run here --
// the caller runs processInsert ONCE per block on the summed sources, so a
// stateful rack advances once per block instead of once per clip.  Returns true
// if >=1 clip contributed (the caller uses this to gate the single chain pass).
// FilePlay clips (routeChannel pointing at a Vox/Inst page) are skipped here.
namespace {

// QA-ClipPlayback Task 2: resolved BaySickPlayer control values for the
// timeline-WAV decode chain, read once per row per block from the ClipsPage
// engine's APVTS.  Velocity-driven routings (velTo*) are inert on a timeline
// clip; muffle/hardness fold into the filter with velocity N/A (matches
// BaySickPlayerVoice::startNote).  active=false when the engine isn't a BaySickPlayer
// (S7 guard) -> the decode stays raw (pre-batch behavior).
struct ClipCtl
{
    bool  active = false;
    float volume = 1.f, panL = 1.f, panR = 1.f;
    float cutoff = 20000.f, q = 0.5f;
    float drive  = 1.f;                    // 1..12, skipped at 1
    float reduct = 0.f;                    // 0..1
    float lfoAmt = 0.f, lfoRate = 5.5f;    // pitch-vibrato depth 0-1 + rate in Hz
    float trebleGain = 0.f;                // -1..1 shelf
    float width = 1.f;                     // M/S width (WAV baseline 1.0)
    float atk = 0.f, dec = 0.f, sus = 1.f, rel = 0.f;
    // Task 3 (decode-read domain): length-preserving pitch, reverse, sample-start.
    float pitchRatio  = 1.f;   // 2^((tune semis + detune cents)/12)
    bool  reverse     = false;
    float sampleStart = 0.f;   // 0..1 skip-into fraction (composes with slip-trim)
    float stretchSpeed = 1.f;  // Stretch knob = varispeed 0.5..2.0 (couples pitch+content-speed)
};

ClipCtl readClipCtl (BaySickPlayerProcessor* pl)
{
    ClipCtl c;
    if (pl == nullptr) return c;
    c.active = true;
    const auto& p = pl->clipCtlPtrs();   // atoms pre-resolved at ctor -> no per-block alloc
    auto rd = [] (std::atomic<float>* a) { return a != nullptr ? a->load() : 0.f; };

    c.volume = rd (p.volume);
    const float pan = juce::jlimit (-1.f, 1.f, rd (p.pan));
    const float ang = (pan + 1.f) * juce::MathConstants<float>::halfPi * 0.5f;   // equal-power
    c.panL = std::cos (ang);
    c.panR = std::sin (ang);

    // res 0..1 -> Q 0.5..10 (BaySickPlayerSynth::setFilterParams); hardness adds to Q,
    // muffle lowers cutoff toward 200 Hz -- both with velocity N/A on a timeline
    // clip (BaySickPlayerVoice::startNote velFactor/velScale reduce to 1.0 at default).
    const float baseCut  = juce::jlimit (20.f, 20000.f, rd (p.cutoff));
    const float muffle   = juce::jlimit (0.f, 1.f, rd (p.muffle));
    const float hardness = juce::jlimit (0.f, 1.f, rd (p.hardness));
    c.cutoff = juce::jlimit (20.f, 20000.f, baseCut - muffle * (baseCut - 200.f));
    c.q      = juce::jlimit (0.5f, 10.f, 0.5f + (rd (p.res) + hardness) * 9.5f);

    c.drive      = 1.f + juce::jlimit (0.f, 1.f, rd (p.drive)) * 11.f;
    c.reduct     = juce::jlimit (0.f, 1.f, rd (p.reduct));
    c.lfoAmt     = juce::jlimit (0.f, 1.f, rd (p.lfoAmt));
    c.lfoRate    = juce::jlimit (0.1f, 20.f, rd (p.lfoRate));
    c.trebleGain = juce::jlimit (-1.f, 1.f, rd (p.treble) / 12.f);
    // Bipolar stereo (QA-ClipPlayback): param -1..1 -> M/S width (0 mono, 1 full,
    // 2 wide).  Applied BEFORE pan below so pan always survives (unlike the
    // sampler's per-voice pan, which thins at full mono).
    c.width = juce::jlimit (0.f, 2.f, 1.f + rd (p.stereo));

    c.atk = juce::jmax (0.f, rd (p.attack));
    c.dec = juce::jmax (0.f, rd (p.decay));
    c.sus = juce::jlimit (0.f, 1.f, rd (p.sustain));
    c.rel = juce::jmax (0.f, rd (p.release));

    // Task 3: pitch (tune semitones + detune cents) -> ratio; reverse + sample-start.
    const float tune   = juce::jlimit (-48.f, 48.f, rd (p.tune));
    const float detune = juce::jlimit (-100.f, 100.f, rd (p.detune));
    c.pitchRatio  = std::pow (2.f, (tune + detune / 100.f) / 12.f);
    c.reverse     = rd (p.reverse) > 0.5f;
    c.sampleStart = juce::jlimit (0.f, 0.999f, rd (p.sampleStart));
    // Stretch knob = varispeed (couples pitch + content-speed, tape-style; NOT
    // length-preserving, unlike tune/detune above).  Null-guard defaults to 1.0 (off),
    // NOT rd()'s 0.0 -> which would clamp to 0.5 (half-speed).
    const float sp = (p.stretch != nullptr) ? p.stretch->load() : 1.f;
    c.stretchSpeed = juce::jlimit (0.5f, 2.f, sp);
    return c;
}

// Clip-level ADSR: attack ramp at clipStart, decay to sustain, hold, release
// ending at clipEnd.  pos/len in output samples.  The 5 ms declick still runs
// after this as the anti-click floor.
inline float clipAdsr (juce::int64 pos, juce::int64 len,
                       float atk, float dec, float sus, float rel, double sr)
{
    if (len <= 0) return 0.f;
    const double t        = (double) pos / sr;
    const double end      = (double) len / sr;
    const double relStart = juce::jmax (0.0, end - (double) rel);
    float g = sus;
    if (atk > 0.f && t < (double) atk)              g = (float) (t / atk);
    else if (dec > 0.f && t < (double) (atk + dec)) g = 1.f - (1.f - sus) * (float) ((t - atk) / dec);
    if (rel > 0.f && t >= relStart)                 g = juce::jmin (g, sus * (float) ((end - t) / (double) rel));
    return juce::jlimit (0.f, 1.f, g);
}

} // namespace

bool BaySickDAWProcessor::renderAudioClipsForRow (int row,
                                                  const AudioClipBlockContext& ctx,
                                                  juce::AudioBuffer<float>* mtDest)
{
    if (row < 0 || row >= kMaxAudioRows)
        return false;
    if (mPatternManager == nullptr || ctx.mxState == nullptr || ctx.clipScratch == nullptr)
        return false;

    const auto& mx = *ctx.mxState;
    auto& clipScratch = *ctx.clipScratch;
    bool anyContributed = false;

    // QA-ClipPlayback Task 2: all clips on this row share the ClipsPage engine, so
    // read its Player controls once (null/non-BaySickPlayer -> ctl.active false).
    const ClipCtl ctl = readClipCtl (ctx.clipPlayer);

    // 2026-05-06 (Batch 9c B1): iterate the audio-thread snapshot captured
    // at the top of processBlock.  CompositeAudioInsertTask calls this helper
    // after the audio thread published mCurrentBlockClipSnapshot, so the
    // players[] vector is guaranteed
    // alive for the duration of this block via the RetirementQueue ack
    // protocol -- no per-site lock needed.
    for (auto& player : mCurrentBlockClipSnapshot->players)
    {
        if (player.source == nullptr) continue;

        // FilePlay clips are handled by the inline pass in processBlock.
        const int routeCh = player.routeChannel;
        const bool isVoxRoute  = routeCh >= MixerChannelIds::kVoxBase
                              && routeCh <  MixerChannelIds::kVoxBase + kMaxVoxPages;
        const bool isInstRoute = routeCh >= MixerChannelIds::kInstBase
                              && routeCh <  MixerChannelIds::kInstBase + kMaxInstPages;
        if (isVoxRoute || isInstRoute)
            continue;

        // QA-ClipDrop Task 2 (SC-I): route a clip by its OWNING Clips-page strip
        // (the audioInsert range == the page's audio row), NOT the grid row it sits
        // on, so moving the block never breaks playback and nothing attaches to a
        // grid track.  routeChannel is stamped to audioInsert(ownerPage) at creation
        // (onAudioClipAdded retag / placeAudioLibraryEntry) and migrated for legacy
        // projects on load.  Legacy/unset routeChannel (0) falls back to trackRow.
        // `row` is the owner row below.  The mixer STRIP mute (audioRowMute) +
        // routing key on the owner strip; the builder-grid track mute keys on the
        // clip's own grid row (player.trackRow) so two clips on one page mute
        // independently.
        const int ownerRow =
            (routeCh >= MixerChannelIds::kAudioBase
          && routeCh <  MixerChannelIds::kAudioBase + kMaxAudioRows)
                ? (routeCh - MixerChannelIds::kAudioBase)
                : player.trackRow;
        if (ownerRow != row) continue;

        const juce::int64 clipStart = clipBeatToSample (player.clipStartBeat, ctx.secPerBeat, mSampleRate);
        const juce::int64 clipEnd   = clipBeatToSample (player.clipEndBeat,   ctx.secPerBeat, mSampleRate);

        if (ctx.projectEnd <= clipStart || ctx.projectStart >= clipEnd) continue;

        const bool rowMuted        = mx.audioRowMute[(size_t) row];
        const bool builderRowMuted = ! mPatternManager->isRowAudible (player.trackRow);

        if (player.mutedByChoke)
        {
            player.unmuteResync = true;
            continue;
        }
        if (rowMuted || builderRowMuted)
        {
            player.unmuteResync = true;
            continue;
        }

        const int bufOffset = (int) juce::jmax ((juce::int64) 0, clipStart - ctx.projectStart);
        const juce::int64 outPosInClip = (ctx.projectStart + bufOffset) - clipStart;

        const double readRatio = player.fileSampleRate / mSampleRate;
        const juce::int64 fileTotalSamples = player.source->getTotalLength();

        // QA-Ea Task 0c slip-trim + QA-ClipPlayback Task 3 sample-start: the clip's
        // first playable file frame.  Rule-4 defensive floor + clamp keep it in range.
        const juce::int64 contentStart = juce::jmax ((juce::int64) 0,
                                                     player.contentStartSamples);
        const juce::int64 contentBase = juce::jlimit ((juce::int64) 0,
            juce::jmax ((juce::int64) 0, fileTotalSamples - 1),
            contentStart + (juce::int64) ((double) ctl.sampleStart * (double) fileTotalSamples));

        // BPM stretch + length-preserving pitch + varispeed.  Pitch scales the vocoder
        // stretch AND the output resample but CANCELS in the source read rate (fileRate),
        // so the disk read is unchanged by pitch (grid length held).  Varispeed (Stretch
        // knob) instead scales the output resample + source consumption but NOT the
        // vocoder stretch -> it couples pitch + content-speed (tape-style) on top:
        // net pitch = tune x varispeed, source consumed x varispeed.
        // QA-Ec: ratio clamps [1/64, 64] keep a corrupt/degenerate originalBPM
        // from collapsing the window math into silence (PhaseVocoder's own
        // mSynthHop floor backstops the DSP side).
        const double stretchRatio    = (player.stretchMode && player.originalBPM > 0.f)
            ? juce::jlimit (1.0 / 64.0, 64.0, (double) player.originalBPM / ctx.bpm) : 1.0;
        // QA-Ec (2b): Resample mode follows tempo as VARISPEED - rate and
        // pitch move together (vinyl) - so the follow term rides the same
        // slots the Stretch-knob varispeed uses (read rate + consumption,
        // never the vocoder ratio).  Exactly 1.0 when the project sits at the
        // clip's own tempo, so imports play untouched (G).
        const double tempoFollow     = (! player.stretchMode && player.originalBPM > 0.f)
            ? juce::jlimit (1.0 / 64.0, 64.0, ctx.bpm / (double) player.originalBPM) : 1.0;
        const double pitchRatio      = (double) ctl.pitchRatio;
        const double varispeed       = (double) ctl.stretchSpeed * tempoFollow;
        const double effStretchRatio = stretchRatio * pitchRatio;
        const double effReadRatio    = readRatio    * pitchRatio * varispeed;
        const double fileRate        = (readRatio / stretchRatio) * varispeed;   // source frames per output frame

        // reverse: resident-audio clips only (the forward-only disk streamer
        // can't read backward without thrashing; clips past the RAM threshold
        // play forward).
        const bool doReverse = ctl.reverse && player.source->isRamLoaded();

        // Timeline frame where the file runs out (from contentBase, via eff ratios).
        const juce::int64 playableFile = juce::jmax ((juce::int64) 0, fileTotalSamples - contentBase);
        const juce::int64 fileEOFOutput = clipStart
            + (juce::int64) ((double) playableFile * effStretchRatio / effReadRatio);
        const juce::int64 effectiveClipEnd = juce::jmin (clipEnd, fileEOFOutput);
        const int outSamples = (int) juce::jmin (
            (juce::int64) (ctx.numSamples - bufOffset),
            effectiveClipEnd - (ctx.projectStart + bufOffset));
        if (outSamples <= 0) continue;

        // Source frame for this block's first output sample.  Forward advances from
        // contentBase; reverse counts down from the clip's forward end frame.
        const juce::int64 clipOutLen  = effectiveClipEnd - clipStart;
        const juce::int64 srcEndFrame = juce::jmin (fileTotalSamples,
            contentBase + (juce::int64) ((double) clipOutLen * fileRate));
        // QA-Ec G1-boundary fix: the linear elapsed*rate mapping is wrong the
        // moment tempo changes mid-clip (the rate it multiplies by only holds
        // for THIS block).  File consumption per musical BEAT is tempo-
        // independent in BOTH modes - stretch pins it by definition, and
        // resample's follow term cancels: (bpm/orig) * (SR*60/bpm) * (fileSR/SR)
        // = fileSR*60/orig - so the beat-domain position is exact through any
        // number of tempo steps.  Kept FRACTIONAL end to end (readAndMix takes
        // a double now); pitch cancels in consumption exactly as it does in
        // fileRate; the varispeed KNOB (not the follow term) still scales it.
        // Reverse keeps the linear model (reverse across mid-clip tempo steps
        // is unsupported; RAM-only path, corner case).
        double posD;
        if (! doReverse && TempoMap::isActive() && player.originalBPM > 0.f)
        {
            const double beatsIn = TempoMap::beatAtSample (ctx.projectStart + bufOffset)
                                   - player.clipStartBeat;
            posD = clipFilePosForBeat (beatsIn, player.fileSampleRate,
                                       (double) player.originalBPM,
                                       (double) contentBase, (double) ctl.stretchSpeed);
        }
        else
        {
            posD = doReverse
                ? (double) srcEndFrame - (double) outPosInClip * fileRate
                : (double) contentBase + (double) outPosInClip * fileRate;
        }
        const juce::int64 pvRefPos = (juce::int64) std::llround (posD);

        if (! doReverse && pvRefPos >= fileTotalSamples) continue;   // forward EOF

        // Muted-edge flag (set by the gates above): consume on the first
        // audible block so the PV path force-resyncs instead of resuming
        // from the frozen feed position.
        const bool unmuteResync = player.unmuteResync;
        player.unmuteResync = false;

        clipScratch.clear();
        const float gain = ctx.masterGain;
        float       peak = 0.0f;

        // Vocoder path for actual time-stretch / pitch only.  Reverse no longer forces
        // it: feeding independently-flipped chunks to a phase vocoder broke phase
        // continuity at the block seams (audible crackle), so plain reverse takes the
        // direct backward read below.  Reverse + stretch still flips into the vocoder
        // (the doReverse block inside this branch).
        // Resolved BEFORE the vibrato setup because that branch consumes the modulation
        // as a stream and carries no position offset, so the gate-close walk-back below
        // must not arm for it.
        const bool usePV = (player.vocoder != nullptr)
                        && (std::abs (effStretchRatio - 1.0) > 0.001);

        // The Player's LFO is PITCH vibrato, not amplitude tremolo: it modulates the
        // clip's READ POSITION, exactly as BaySickPlayerVoice::renderNextBlock modulates the
        // voice read increment, so one knob means one thing on a played note and on a
        // timeline clip.  Depth law and cents mapping are the voice path's verbatim
        // (BaySickPlayerDSP.h kVibratoMaxCents), the phase increment comes off the LIVE
        // device rate so the wobble speed is sample-rate independent, and ONE per-clip
        // phase feeds all three read branches below so they cannot drift apart.
        constexpr double kClipVibratoMaxCents = 50.0;   // == BaySickPlayerVoice::kVibratoMaxCents
        // 2^(50/1200) - 1: the read-rate deviation the vibrato itself produces at full
        // depth, reused as the cap on the gate-close walk-back so the walk home never
        // bends pitch harder than the effect the user was already hearing.
        constexpr double kClipVibGlideRatio = 0.0293022366434921;
        const bool vibOn = ctl.active && ctl.lfoAmt > 0.001f;
        // Hard-zeroing the carried offset the moment the knob crosses the gate STEPS the
        // read position.  The offset is the integral of a sine started at phase 0, so it
        // lives in [0, 2A] instead of straddling zero, and 2A is ~75 file samples at the
        // 5.5 Hz default and ~4.1k at the 0.1 Hz minimum - a click on turning vibrato
        // off.  So while the gate is shut and the offset is still non-zero the modulated
        // branch keeps running at zero depth and walks the offset home at a bounded rate.
        // The walk assigns exactly 0.0 on its last step, which is what lets the plain
        // readAndMix path resume bit-identically.
        const bool vibGliding = (! vibOn) && (! usePV) && (player.clipVibOffset != 0.0);
        const bool vibActive  = vibOn || vibGliding;
        if (! vibActive)
            player.clipVibOffset = 0.0;

        const double vibInc     = (juce::MathConstants<double>::twoPi * (double) ctl.lfoRate) / mSampleRate;
        const double vibIncSafe = juce::jmax (1.0e-9, vibInc);
        const double vibPhase0  = player.clipLfoPhase;

        // Reverse fetches its whole raw window in ONE readRaw, so the request has to fit
        // pvInBuf.  Truncation is not a graceful degradation there: the reverse read
        // descends from the TOP of the window, so a short buffer pins the first output
        // samples to one held frame.  Trim the modulation depth for THIS block until the
        // window fits instead.  Linear-in-scale is a conservative estimate of the
        // widening, because exp(s*x) - 1 <= s*(exp(x) - 1) on s in [0, 1] by convexity.
        // BOTH raw-read branches, not just reverse.  Neither can overrun the buffer -
        // each clamps numRaw to pvInBuf - but a clamped window makes the kernel pin to
        // its last frame, which is an audible held sample.  Trimming depth instead
        // degrades gracefully.  The forward branch reaches this at a large buffer with a
        // high-rate source on a lower-rate device and Stretch up (about 4096 x 8.5
        // against a 34,816-sample buffer); rarer than the reverse case but the same
        // failure, so it gets the same answer rather than being left to buzz.
        double vibScale = 1.0;
        if (vibActive && ! usePV)
        {
            const double semisF = vibOn ? (double) ctl.lfoAmt * (kClipVibratoMaxCents / 100.0) : 0.0;
            const double peakF  = std::pow (2.0, semisF / 12.0);
            const double excF   = vibOn
                ? fileRate * (peakF - 1.0) * juce::jmin ((double) outSamples, 2.0 / vibIncSafe)
                : juce::jmin (std::abs (player.clipVibOffset),
                              kClipVibGlideRatio * fileRate * (double) outSamples);
            const double rateMaxF = vibOn ? peakF : (1.0 + kClipVibGlideRatio);
            // Forward spans outSamples (not outSamples-1) and reaches 4 past the top for
            // the kernel's ip2, so it is the wider of the two by one step plus one sample.
            const double spanN  = doReverse ? (double) (outSamples - 1) : (double) outSamples;
            const double baseW  = spanN * fileRate + (doReverse ? 3.0 : 4.0);
            const double extraW = spanN * fileRate * (rateMaxF - 1.0)
                                + (doReverse ? 2.0 * excF + 6.0    // 4 = vibBlockExc's own
                                                                   // constant on both edges,
                                                                   // 2 = ceil() slack
                                             : excF);              // forward shifts, not widens
            const double headW  = (double) player.pvInBuf.getNumSamples() - baseW;
            if (extraW > headW)
                vibScale = juce::jlimit (0.0, 1.0, headW / juce::jmax (1.0e-9, extraW));
        }
        // The walk home keeps a floor under that trim - it has to terminate - and at a
        // quarter rate its own contribution to the window is under 1% of the base read.
        const double vibGlide = kClipVibGlideRatio * fileRate * juce::jmax (0.25, vibScale);

        const double vibSemis = vibOn
            ? (double) ctl.lfoAmt * (kClipVibratoMaxCents / 100.0) * vibScale : 0.0;
        const double vibPeak  = std::pow (2.0, vibSemis / 12.0);
        // E[2^(d.sin)] = I0(d.ln2) > 1, so an uncorrected vibrato consumes source
        // FASTER than the timeline advances: the read would creep permanently ahead of
        // position and the vocoder branch would slowly starve its output queue.
        // Subtracting the series mean 1 + a^2/4 (exact to ~1e-8 across this 50-cent
        // range) makes the deviation zero-mean; the residual 0.36 cents of flatness at
        // full depth is inaudible.
        const double vibLnPeak = vibSemis * (0.6931471805599453 / 12.0);   // ln(2)/12
        const double vibMean   = 1.0 + vibLnPeak * vibLnPeak * 0.25;
        // Fastest per-sample read advance this block can ask for, as a multiple of
        // fileRate.  While the gate is shut the vibrato contributes nothing but the walk
        // home still does: it moves the carried offset, and a NEGATIVE offset walking up
        // toward zero adds its step on top of the unmodulated advance, so the read
        // windows sized off this have to carry it.  Exactly 1.0 with no vibrato and no
        // walk pending, which is what keeps the untouched paths bit-identical.
        const double vibRateMax = vibOn ? (vibPeak - vibMean + 1.0)
                                : (vibGliding ? 1.0 + kClipVibGlideRatio * juce::jmax (0.25, vibScale)
                                              : 1.0);
        // Integrating a bounded zero-mean deviation gives a bounded position offset:
        // the limit is twice that analytic amplitude, so it never engages in normal
        // running and exists only to stop a mid-flight depth / rate change from walking
        // the read past the windows sized against it below.  While the gate is shut the
        // limit is the carried magnitude itself, because the walk home only ever shrinks
        // it and a zero limit would defeat the walk.
        const double vibOffsetBound = vibOn
            ? 2.0 * fileRate * (vibPeak - 1.0) / vibIncSafe + 8.0
            : std::abs (player.clipVibOffset);
        // The excursion the offset can accumulate WITHIN this block, either direction.
        // This is the only term that WIDENS a read window: the carried offset SHIFTS it,
        // because every loop below advances position and offset together.  Bounded both
        // by the per-sample deviation over the block and by the analytic integral 2A/w,
        // so it stays small even where the carried bound blows up at low LFO rates.
        const double vibBlockExc = vibActive
            ? 2.0 + (vibOn
                     ? fileRate * (vibPeak - 1.0) * juce::jmin ((double) outSamples, 2.0 / vibIncSafe)
                     : juce::jmin (std::abs (player.clipVibOffset), vibGlide * (double) outSamples))
            : 0.0;

        // Magic-circle (coupled-form) oscillator, RESEEDED from player.clipLfoPhase every
        // block so that phase stays the authority across block boundaries.  eps =
        // 2*sin(w/2) places the recurrence's eigenvalues exactly at exp(+-jw), and its
        // state matrix has determinant 1, so the trajectory is pinned to a fixed ellipse:
        // amplitude cannot drift over a long block the way a direct-form rotation's
        // would.  y = sin(p) paired with x = cos(p - w/2) is the seed that lands on that
        // exact trajectory (the pair sits a half sample apart, not in quadrature).
        // Gated: vibNextDev is only ever reached with the knob up, and a normal
        // project has every clip at zero, so an ungated seed would charge three libm
        // calls per clip per block for a feature nobody switched on.
        const double vibHalfInc = vibOn ? vibInc * 0.5 : 0.0;
        const double vibEps     = vibOn ? 2.0 * std::sin (vibHalfInc) : 0.0;
        double vibOscX = vibOn ? std::cos (vibPhase0 - vibHalfInc) : 0.0;
        double vibOscY = vibOn ? std::sin (vibPhase0)               : 0.0;

        // Signed rate DEVIATION for the next output sample (0 = read at the unmodulated
        // rate), advancing the oscillator one step.  SEQUENTIAL BY CONTRACT: exactly one
        // read branch runs per block and each consumes this in output order from sample
        // 0, its catch-up tail loop included.
        // Audio thread: this runs per output sample per clip, so neither std::sin nor
        // std::pow may appear in it.  exp() by cubic Taylor series - |x| <= 0.0289 by
        // construction (vibLnPeak at the full 50-cent depth), so the truncation error is
        // under 3e-8, about 5e-5 cents, and it underestimates, which keeps vibRateMax a
        // true upper bound on the modulated rate.
        auto vibNextDev = [&] () noexcept -> double
        {
            const double x = vibLnPeak * vibOscY;
            vibOscX -= vibEps * vibOscY;
            vibOscY += vibEps * vibOscX;
            return (1.0 + x * (1.0 + x * (0.5 + x * (1.0 / 6.0)))) - vibMean;
        };
        // One output sample of carried-offset motion: the modulated advance while the
        // gate is open, the bounded walk home while it is shut.
        auto vibStepOffset = [&] (double& off) noexcept
        {
            if (vibOn)                off += fileRate * vibNextDev();
            else if (off >  vibGlide) off -= vibGlide;
            else if (off < -vibGlide) off += vibGlide;
            else                      off = 0.0;
        };

        if (usePV)
        {
            player.vocoder->setStretchRatio (effStretchRatio);

            // player.expectedFilePos / source->requestSeek track the absolute file frame.
            // For reverse it counts DOWN and the read chunk is flipped before push.
            const juce::int64 pvReadPos = player.expectedFilePos;
            const bool seekNeeded = unmuteResync ||
                (pvReadPos == 0 && pvRefPos > (juce::int64) mSampleRate) ||
                (pvReadPos != 0 &&
                 std::abs (pvRefPos - pvReadPos) > (juce::int64) (mSampleRate * 2));

            if (seekNeeded)
            {
                player.vocoder->reset();
                // G1 smoke round 6: requestSeek, NOT seek() - seek() does a
                // synchronous disk prefill under the reader lock; calling it
                // here (audio thread) blew the callback deadline on streamed
                // files.  The bg thread fills; readRaw is silent until ready.
                player.source->requestSeek (pvRefPos);
                player.expectedFilePos = pvRefPos;
                player.pvOutFrac       = 0.0;   // G1 fix: fresh fractional output position
            }

            // Clamp to pvInBuf capacity: fileRate can exceed the buffer's 4x-block
            // headroom (high-SR file x slow BPM stretch x varispeed) which would overrun
            // readRaw's write.  Truncating degrades to a brief glitch in that extreme
            // instead of corrupting memory (mirrors the reverse branch's clamp below).
            const int numFileSamples = (int) juce::jmin (
                (juce::int64) std::ceil ((double) outSamples * fileRate),
                (juce::int64) player.pvInBuf.getNumSamples());
            const juce::int64 readAt = doReverse
                ? player.expectedFilePos - numFileSamples
                : player.expectedFilePos;

            // Audio thread: every pv scratch clear in this file is bounded to the
            // region actually written.  The buffers are allocated for the worst
            // case (~34,816 samples/ch) while a block touches a fraction of it, so
            // a full-capacity memset is a per-output-sample cost that grows as
            // 1/blockSize -- worst exactly where headroom is tightest.  The ranged
            // clear zeroes that range on EVERY channel, so anything the reader
            // leaves untouched still reads as silence.
            player.pvInBuf.clear (0, numFileSamples);
            const bool inRange = (readAt >= 0) && (readAt + numFileSamples <= fileTotalSamples);
            const bool gotRaw  = inRange
                && player.source->readRaw (player.pvInBuf, 0, numFileSamples, readAt);

            if (gotRaw)
            {
                if (doReverse)
                {
                    // flip the freshly-read chunk so the vocoder is fed source frames
                    // in descending-time order (reverse playback).
                    for (int ch = 0; ch < player.pvInBuf.getNumChannels(); ++ch)
                    {
                        float* d = player.pvInBuf.getWritePointer (ch);
                        for (int a = 0, b = numFileSamples - 1; a < b; ++a, --b)
                        { const float t = d[a]; d[a] = d[b]; d[b] = t; }
                    }
                    player.expectedFilePos -= numFileSamples;
                }
                else
                    player.expectedFilePos += numFileSamples;

                player.vocoder->push (player.pvInBuf, 0, numFileSamples);

                // QA-Ec G1-boundary fix: peek + fractional advance.  pull()'s
                // consume-what-you-request discarded the +2 interp lookahead
                // every block (a 2-sample skip per buffer = audible crackle
                // on every stretched clip), and the interp phase restarted at
                // zero per block (fraction lost).  pvOutFrac carries the exact
                // fractional output position across blocks.
                // Vibrato rides the OUTPUT read rate here (the vocoder's stretch law
                // is per-block by construction), so the peek window is sized against
                // the fastest sample the modulation can ask for.
                const double needMax = player.pvOutFrac
                                     + (double) outSamples * effReadRatio * vibRateMax;
                const int    peekWant = juce::jmin ((int) needMax + 2,
                                                    player.pvOutBuf.getNumSamples());
                player.pvOutBuf.clear (0, peekWant);
                const int peeked = player.vocoder->peekOutput (player.pvOutBuf, 0, peekWant);

                if (peeked > 0)
                {
                    const int pvCh = player.pvOutBuf.getNumChannels();
                    // The vocoder output is a stream this branch consumes, so the
                    // modulated position needs no carried offset - pvOutFrac plus the
                    // advanceOutput consume already carry it across the boundary.
                    double modFP = player.pvOutFrac;
                    int    i     = 0;
                    for (; i < outSamples; ++i)
                    {
                        const double exactFP = vibOn ? modFP
                                             : player.pvOutFrac + (double) i * effReadRatio;
                        const int    ip      = (int) exactFP;
                        const float  frac    = (float) (exactFP - ip);

                        if (ip + 1 >= peeked) break;

                        // G1 smoke round 5: Catmull-Rom (was linear) - the
                        // linear error tracks the program material = fizz on
                        // dense mixes.  Edge frames clamp into [0, peeked).
                        const int im1 = juce::jmax (0, ip - 1);
                        const int ip2 = juce::jmin (ip + 2, peeked - 1);

                        for (int ch = 0; ch < ctx.numOut; ++ch)
                        {
                            const int   srcCh = ch % pvCh;
                            const float p0    = player.pvOutBuf.getSample (srcCh, im1);
                            const float p1    = player.pvOutBuf.getSample (srcCh, ip);
                            const float p2    = player.pvOutBuf.getSample (srcCh, ip + 1);
                            const float p3    = player.pvOutBuf.getSample (srcCh, ip2);
                            const float v     = (p1 + 0.5f * frac * ((p2 - p0)
                                              + frac * (2.f * p0 - 5.f * p1 + 4.f * p2 - p3
                                              + frac * (3.f * (p1 - p2) + p3 - p0)))) * gain;
                            clipScratch.addSample (ch, bufOffset + i, v);
                            peak = juce::jmax (peak, std::abs (v));
                        }
                        if (vibOn) modFP += effReadRatio * (vibNextDev() + 1.0);
                    }
                    // A peek shortfall must not shorten the consume bookkeeping: finish
                    // the modulated advance for the samples the break skipped, so the
                    // demand stays the full block exactly as it did before vibrato.
                    if (vibOn)
                        for (; i < outSamples; ++i)
                            modFP += effReadRatio * (vibNextDev() + 1.0);

                    const double needF   = vibOn ? modFP
                                         : player.pvOutFrac + (double) outSamples * effReadRatio;
                    const int    consume = (int) needF;
                    const int adv = juce::jmin (consume, peeked);
                    player.vocoder->advanceOutput (adv);
                    player.pvOutFrac = needF - (double) adv;
                    // G1 smoke round 4: a shortfall (peeked < consume) must
                    // NOT carry as whole-sample DEBT - it compounds every
                    // block into an unbounded demand (the big-streaming-file
                    // lockup + the MP3 crackle).  The missed samples are
                    // gone; keep only the sub-sample phase.
                    if (player.pvOutFrac >= 1.0)
                        player.pvOutFrac -= std::floor (player.pvOutFrac);
                }
            }
        }
        else if (doReverse)
        {
            // Plain reverse (no stretch/pitch): read the source backward straight from RAM
            // (doReverse is RAM-only) and interpolate DESCENDING -- click-free, because it
            // never touches the phase vocoder.  fileRate carries any varispeed.  pvInBuf is
            // free here (usePV is false so the vocoder branch didn't run) and is reused as
            // the raw-read scratch.
            // The window has to cover the fastest descent the vibrato can ask for, and it
            // starts at the block's ACTUAL first read frame, pvRefPos - carried offset.
            // Sizing the two edges off the symmetric vibOffsetBound instead - which grows
            // as 1/lfoRate, ~4.1k file samples at 0.1 Hz on a 44.1 kHz file and ~17.9k on
            // a 192 kHz one - pushed the request past pvInBuf, and the truncation lands on
            // the TOP of the window, which is exactly where a reverse read begins.  The
            // carried offset only SHIFTS the window (position and offset advance together
            // in the loop); only vibBlockExc widens it, and that is bounded by the block
            // length.  Collapses to the unmodulated window exactly at zero depth.
            const double vibOffCarried = player.clipVibOffset;
            const juce::int64 lowFrame = pvRefPos
                - (juce::int64) std::ceil ((double) (outSamples - 1) * fileRate * vibRateMax
                                           + vibBlockExc + vibOffCarried) - 1;
            const juce::int64 loRead   = juce::jmax ((juce::int64) 0, lowFrame);
            const juce::int64 hiFrame  = pvRefPos + 2
                + (juce::int64) std::ceil (vibBlockExc - vibOffCarried);
            const int numRaw = (int) juce::jmax ((juce::int64) 0, juce::jmin (
                juce::jmin (hiFrame - loRead, fileTotalSamples - loRead),
                (juce::int64) player.pvInBuf.getNumSamples()));
            double revOff   = player.clipVibOffset;
            int    revDone  = 0;
            player.pvInBuf.clear (0, numRaw);
            if (numRaw > 1 && player.source->readRaw (player.pvInBuf, 0, numRaw, loRead))
            {
                const int pvCh = player.pvInBuf.getNumChannels();
                for (; revDone < outSamples; ++revDone)
                {
                    const double idx = ((double) pvRefPos - (double) revDone * fileRate - revOff)
                                       - (double) loRead;
                    if (idx < 0.0) break;                          // reached content start
                    int   ip   = (int) idx;
                    float frac = (float) (idx - ip);
                    // Top edge (reverse starting at the file's true end): no s0/s1 pair
                    // exists above the last frame, so pin to it instead of emitting
                    // silence.  numRaw > 1 is guaranteed above, so numRaw-2 >= 0.
                    if (ip >= numRaw - 1) { ip = numRaw - 2; frac = 1.0f; }
                    for (int ch = 0; ch < ctx.numOut; ++ch)
                    {
                        const int   srcCh = ch % pvCh;
                        const float s0    = player.pvInBuf.getSample (srcCh, ip);
                        const float s1    = player.pvInBuf.getSample (srcCh, ip + 1);
                        const float v     = (s0 + frac * (s1 - s0)) * gain;
                        clipScratch.addSample (ch, bufOffset + revDone, v);
                        peak = juce::jmax (peak, std::abs (v));
                    }
                    if (vibActive) vibStepOffset (revOff);
                }
            }
            if (vibActive)
            {
                // Advance for whatever the read bailed on too: the phase below moves by
                // the whole block unconditionally, and an offset that lagged it would
                // re-phase the wobble on the next block.
                for (; revDone < outSamples; ++revDone)
                    vibStepOffset (revOff);
                player.clipVibOffset = juce::jlimit (-vibOffsetBound, vibOffsetBound, revOff);
            }
            player.expectedFilePos = pvRefPos;
        }
        else if (vibActive)
        {
            // Per-sample pitch vibrato on the plain forward read.  readAndMix takes ONE
            // rate for a whole block, so folding the LFO into that rate would quantize
            // the wobble to one step per buffer and make its depth a function of the
            // buffer size -- the exact defect this replaces.  Instead the raw window is
            // fetched once and the same Catmull-Rom kernel runs here against a position
            // advanced per sample.  pvInBuf is free (usePV is false) and is already the
            // raw-read scratch for the reverse branch above.
            //
            // Bounds: the per-sample increment fileRate*(1+deviation) is strictly
            // positive, so positions rise monotonically from startPos and can never
            // exceed endPos; the window spans [floor(startPos)-1, ceil(endPos)+3)
            // clamped into [0, fileTotalSamples), which is what the kernel's im1/ip2
            // reach needs, and readRaw refuses anything outside the file anyway.  The
            // gate-close walk home preserves both properties: it shifts the offset by at
            // most kClipVibGlideRatio of fileRate per sample, too small to reverse the
            // advance, and vibRateMax carries that step so endPos still bounds it when a
            // negative offset walks UP toward zero.  Unlike the reverse branch this
            // window was already anchored on the CARRIED offset rather than on its
            // symmetric bound, so it never had that branch's low-LFO-rate blow-up -- but
            // it shares the depth trim above, because a window clamped to pvInBuf pins
            // the kernel to its last frame either way and a held sample is audible.
            const double startPos = posD + player.clipVibOffset;
            const double endPos   = startPos + (double) outSamples * fileRate * vibRateMax;
            const juce::int64 loRead = juce::jmax ((juce::int64) 0,
                                                   (juce::int64) std::floor (startPos) - 1);
            const juce::int64 hiRead = juce::jmin (fileTotalSamples,
                                                   (juce::int64) std::ceil (endPos) + 3);
            const int numRaw = (int) juce::jmax ((juce::int64) 0,
                juce::jmin (hiRead - loRead, (juce::int64) player.pvInBuf.getNumSamples()));

            double fwdOff  = player.clipVibOffset;
            int    fwdDone = 0;
            player.pvInBuf.clear (0, numRaw);
            if (numRaw > 1 && player.source->readRaw (player.pvInBuf, 0, numRaw, loRead))
            {
                const int pvCh = player.pvInBuf.getNumChannels();
                for (; fwdDone < outSamples; ++fwdDone)
                {
                    const double rel = posD + (double) fwdDone * fileRate + fwdOff
                                       - (double) loRead;
                    const juce::int64 ipL = (juce::int64) std::floor (rel);
                    if (ipL + 1 >= (juce::int64) numRaw) break;      // EOF inside the window
                    if (ipL >= 0)
                    {
                        const int   ip   = (int) ipL;
                        const float frac = (float) (rel - (double) ipL);
                        const int   im1  = juce::jmax (0, ip - 1);
                        const int   ip2  = juce::jmin (ip + 2, numRaw - 1);
                        for (int ch = 0; ch < ctx.numOut; ++ch)
                        {
                            const int   srcCh = ch % pvCh;
                            const float p0 = player.pvInBuf.getSample (srcCh, im1);
                            const float p1 = player.pvInBuf.getSample (srcCh, ip);
                            const float p2 = player.pvInBuf.getSample (srcCh, ip + 1);
                            const float p3 = player.pvInBuf.getSample (srcCh, ip2);
                            const float v  = (p1 + 0.5f * frac * ((p2 - p0)
                                          + frac * (2.f * p0 - 5.f * p1 + 4.f * p2 - p3
                                          + frac * (3.f * (p1 - p2) + p3 - p0)))) * gain;
                            clipScratch.addSample (ch, bufOffset + fwdDone, v);
                            peak = juce::jmax (peak, std::abs (v));
                        }
                    }
                    vibStepOffset (fwdOff);
                }
            }
            for (; fwdDone < outSamples; ++fwdDone)
                vibStepOffset (fwdOff);
            player.clipVibOffset = juce::jlimit (-vibOffsetBound, vibOffsetBound, fwdOff);
            player.expectedFilePos = (juce::int64) std::llround (
                posD + (double) outSamples * fileRate + fwdOff);
        }
        else
        {
            // Direct read runs only when the vocoder is bypassed (effStretchRatio ~ 1),
            // so fileRate here == readRatio scaled by varispeed -- pass fileRate (not the
            // raw readRatio) so the Stretch knob varispeeds the plain-playback path too.
            // posD keeps the fractional position (sub-sample block continuity).
            peak = player.source->readAndMix (
                clipScratch, bufOffset, outSamples, posD, fileRate, ctx.numOut, gain);
            player.expectedFilePos = (juce::int64) std::llround (posD + (double) outSamples * fileRate);
        }

        // One phase for the whole clip, advanced by OUTPUT samples, so whichever read
        // branch above ran the wobble is the same shape at the same place.
        if (vibOn)
            player.clipLfoPhase = std::fmod (player.clipLfoPhase + vibInc * (double) outSamples,
                                             juce::MathConstants<double>::twoPi);

        // QA-ClipPlayback Task 2: run the ClipsPage BaySickPlayer control chain on
        // the decoded clip (before the declick + raw sum) so a timeline-WAV clip
        // tracks the Player knobs.  Chain: drive -> reduction -> filter -> M/S width
        // -> treble shelf -> volume x ADSR x pan.  Pan is LAST (after width) so it
        // survives even at full-mono width.  The LFO is NOT in this chain: it is a
        // pitch control and rides the read position above.  Per-clip state on `player`.
        if (ctl.active)
        {
            const int chs = clipScratch.getNumChannels();

            if (ctl.drive > 1.001f)    // tanh waveshaper
            {
                const float norm = std::tanh (ctl.drive);
                for (int ch = 0; ch < chs; ++ch)
                {
                    auto* d = clipScratch.getWritePointer (ch, bufOffset);
                    for (int s = 0; s < outSamples; ++s) d[s] = std::tanh (d[s] * ctl.drive) / norm;
                }
            }

            if (ctl.reduct > 0.01f)    // sample-rate reduction (sample-and-hold)
            {
                const int holdN     = 1 + juce::roundToInt (ctl.reduct * 15.f);
                const int startStep = player.clipReductStep;
                int       endStep   = startStep;
                for (int ch = 0; ch < chs; ++ch)
                {
                    auto* d = clipScratch.getWritePointer (ch, bufOffset);
                    int step = startStep; float held = d[0];
                    for (int s = 0; s < outSamples; ++s) { if (step == 0) held = d[s]; d[s] = held; step = (step + 1) % holdN; }
                    endStep = step;
                }
                player.clipReductStep = endStep;
            }

            player.clipFilter.setCutoffFrequency (ctl.cutoff);   // SVF lowpass
            player.clipFilter.setResonance       (ctl.q);
            for (int ch = 0; ch < juce::jmin (chs, 2); ++ch)
            {
                auto* d = clipScratch.getWritePointer (ch, bufOffset);
                for (int s = 0; s < outSamples; ++s) d[s] = player.clipFilter.processSample (ch, d[s]);
            }

            if (chs >= 2)   // M/S width + treble shelf, BEFORE pan so pan survives at any width
            {
                auto* L = clipScratch.getWritePointer (0, bufOffset);
                auto* R = clipScratch.getWritePointer (1, bufOffset);
                if (ctl.width != 1.f)   // bipolar: 0 = mono, 1 = full, 2 = wide
                    for (int s = 0; s < outSamples; ++s)
                    {
                        const float mid = (L[s] + R[s]) * 0.5f, side = (L[s] - R[s]) * 0.5f * ctl.width;
                        L[s] = mid + side; R[s] = mid - side;
                    }
                if (std::abs (ctl.trebleGain) > 0.001f)
                {
                    const float omega = 2.f * juce::MathConstants<float>::pi * 8000.f / (float) mSampleRate;
                    const float a     = omega / (1.f + omega);
                    for (int s = 0; s < outSamples; ++s)
                    {
                        player.clipTrebleLp[0] += a * (L[s] - player.clipTrebleLp[0]); L[s] += ctl.trebleGain * (L[s] - player.clipTrebleLp[0]);
                        player.clipTrebleLp[1] += a * (R[s] - player.clipTrebleLp[1]); R[s] += ctl.trebleGain * (R[s] - player.clipTrebleLp[1]);
                    }
                }
            }

            const juce::int64 clipLen = effectiveClipEnd - clipStart;   // volume x ADSR x pan (pan LAST -> survives width)
            for (int s = 0; s < outSamples; ++s)
            {
                const float g = ctl.volume * clipAdsr (outPosInClip + s, clipLen,
                                                       ctl.atk, ctl.dec, ctl.sus, ctl.rel, mSampleRate);
                if (chs >= 2)
                {
                    clipScratch.setSample (0, bufOffset + s, clipScratch.getSample (0, bufOffset + s) * g * ctl.panL);
                    clipScratch.setSample (1, bufOffset + s, clipScratch.getSample (1, bufOffset + s) * g * ctl.panR);
                }
                else
                    clipScratch.setSample (0, bufOffset + s, clipScratch.getSample (0, bufOffset + s) * g);
            }
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

        // QA-MultiBlockHazard (Task 1): sum this clip's RAW decoded output into
        // the row block buffer (additive so multiple clips on the row sum).  The
        // insert chain runs ONCE per block on this summed buffer in
        // CompositeAudioInsertTask::run -- not per-clip -- so a stateful rack
        // (delay / reverb / LFO / compressor) advances once per block.
        {
            const int nc = juce::jmin (mtDest->getNumChannels(),
                                       clipScratch.getNumChannels());
            for (int c = 0; c < nc; ++c)
                mtDest->addFrom (c, 0, clipScratch, c, 0, ctx.numSamples);
        }
        anyContributed = true;
    }

    return anyContributed;
}

// ── QA-MultiBlockHazard (Task 2): Vox/Inst FilePlay decode + finalize ────────
// Split from the former renderFilePlayPlayer (Batch 9b Item 9, 2026-05-06) so
// the engine + insert chain run ONCE per block on the summed clips, not once
// per clip.  decodeFilePlayClip decodes ONE clip into the sum; the caller then
// calls finalizeFilePlayStrip once.  Called by VoxStripTask / InstStripTask.
// See header for invariants.
bool BaySickDAWProcessor::decodeFilePlayClip (AudioClipPlayer&             player,
                                             const AudioClipBlockContext& ctx,
                                             juce::AudioBuffer<float>&    sumDest)
{
    using int64 = juce::int64;

    // ── Preconditions ────────────────────────────────────────────────────────
    const int routeCh = player.routeChannel;
    const bool isVoxRoute  = routeCh >= MixerChannelIds::kVoxBase
                           && routeCh <  MixerChannelIds::kVoxBase + kMaxVoxPages;
    const bool isInstRoute = routeCh >= MixerChannelIds::kInstBase
                           && routeCh <  MixerChannelIds::kInstBase + kMaxInstPages;
    if (! isVoxRoute && ! isInstRoute) return false;
    if (player.source == nullptr)    return false;
    if (mPatternManager == nullptr)    return false;
    if (ctx.clipScratch == nullptr)    return false;
    if (ctx.mxState     == nullptr)    return false;

    const auto& mx = *ctx.mxState;
    auto& clipScratch = *ctx.clipScratch;
    const int   numSamples = ctx.numSamples;
    const int   numOut     = ctx.numOut;
    const double secPerBeat = ctx.secPerBeat;

    // ── Clip-range + mute/choke checks ────────
    const int64 clipStart = clipBeatToSample (player.clipStartBeat, secPerBeat, mSampleRate);
    const int64 clipEnd   = clipBeatToSample (player.clipEndBeat,   secPerBeat, mSampleRate);
    if (ctx.projectEnd <= clipStart || ctx.projectStart >= clipEnd) return false;

    const int   row      = player.trackRow;
    const bool  inRange  = (row >= 0 && row < kMaxAudioRows);
    const bool  rowMuted = inRange && mx.audioRowMute[(size_t) row];
    const bool  builderRowMuted = ! mPatternManager->isRowAudible (row);

    if (player.mutedByChoke)
    {
        player.unmuteResync = true;
        return false;
    }
    if (rowMuted || builderRowMuted)
    {
        player.unmuteResync = true;
        return false;
    }

    // ── Decode params ────────────────────────────────────────────────────────
    const int   bufOffset    = (int) juce::jmax ((int64) 0, clipStart - ctx.projectStart);
    const int64 outPosInClip = (ctx.projectStart + bufOffset) - clipStart;
    // QA-Ec (2b): Path B mirror of Path A's Resample tempo-follow - varispeed
    // (rate + pitch together) folded into the read rate.  Path B has no
    // per-clip pitch/varispeed knobs, so the read rate is the single slot.
    const double tempoFollowB = (! player.stretchMode && player.originalBPM > 0.f)
        ? juce::jlimit (1.0 / 64.0, 64.0, ctx.bpm / (double) player.originalBPM) : 1.0;
    const double readRatio   = (player.fileSampleRate / mSampleRate) * tempoFollowB;
    // QA-Ea Task 0c (FL pre-roll record): mirror of Site A direct-read
    // offset (Vox/Inst FilePlay).  Rule-4 defensive floor: UI clamps
    // contentStartSamples >= 0; floor here is belt+suspenders against a
    // stale / corrupt project value.
    const int64 contentStart = juce::jmax ((int64) 0, player.contentStartSamples);
    // QA-Ec G1-boundary fix: beat-domain file position (mirror of Path A -
    // tempo-independent in both modes, exact across tempo steps, fractional
    // for sub-sample block continuity).  Linear fallback when no timeline.
    double posDB;
    if (TempoMap::isActive() && player.originalBPM > 0.f)
    {
        const double beatsIn = TempoMap::beatAtSample (ctx.projectStart + bufOffset)
                               - player.clipStartBeat;
        posDB = clipFilePosForBeat (beatsIn, player.fileSampleRate,
                                    (double) player.originalBPM,
                                    (double) contentStart);
    }
    else
    {
        posDB = (double) outPosInClip * readRatio + (double) contentStart;
    }
    const int64 filePos = (int64) std::llround (posDB);
    // (readRatio above already carries the QA-Ec Resample tempo-follow term.)

    const int64 fileTotalSamples = player.source->getTotalLength();
    // EOF guard: clip extends past file end -> skip.  filePos < 0 is
    // unreachable post-Rule-4 floor (mirror of Site A).
    if (filePos >= fileTotalSamples)
    {
        return false;
    }

    const double stretchRatio = (player.vocoder != nullptr
                                 && player.stretchMode
                                 && player.originalBPM > 0.f)
        ? juce::jlimit (1.0 / 64.0, 64.0, (double) player.originalBPM / ctx.bpm)   // QA-Ec degenerate-ratio clamp
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

    // Muted-edge flag (set by the gates above): consume on the first audible
    // block so the PV paths below (warp + pristine) force-resync instead of
    // resuming from the frozen feed position.
    const bool unmuteResync = player.unmuteResync;
    player.unmuteResync = false;

    // ── Decode into ctx.clipScratch (Phase vocoder OR direct path) ───────────
    clipScratch.clear();

    const float gain = ctx.masterGain;
    float       peak = 0.0f;

    const bool usePV = (player.vocoder != nullptr)
                    && player.stretchMode
                    && (player.originalBPM > 0.f)
                    && (std::abs (ctx.bpm - player.originalBPM) > 0.01);

    // ── QA-Fa recovery: align live-warp applicator (decode layer) ────────────
    // When a Vox channel has an APPLIED warp map with the chain ON, its clips
    // decode through the PhaseVocoder with the read position remapped through
    // the map (guide -> dub inverse) and the stretch compounding warp slope x
    // tempo-stretch x pitch ratio in one pass (the locked May design).  All
    // law changes (ON/OFF toggle, Apply, revert) glide over ~50 ms as a
    // consumption-rate correction -- never a splice (rule 5); the only path
    // swaps (direct <-> PV) crossfade within one block at identical read
    // positions.  Idle cost per clip: the mBlockAlignEntries scan below
    // (<= kMaxVoxPages pointer compares) + one bool check.
    //
    // QA-Fd time-edit engine (locked 5/13a): the law COMPOSES the channel's
    // pitch TIME map downstream of the align map -- read =
    // rawLaw(pitchMap(alignMap(t))) with either stage dropping out when its
    // chain is off.  The glide/seek machinery is law-agnostic and unchanged.
    bool warpHandled = false;
    if (isVoxRoute && player.vocoder != nullptr)
    {
        const AlignBlockEntry* ae = nullptr;
        for (const auto& e : mBlockAlignEntries)
            if (e.snap != nullptr && e.snap->followerChannelId == routeCh)
                { ae = &e; break; }
        // The channel's OWN pitch time map (index-matched; also the stamp slot).
        auto& ownEntry = mBlockAlignEntries[(size_t) (routeCh - MixerChannelIds::kVoxBase)];
        const bool alignActive = (ae != nullptr) && ae->chainOn;
        // QA-Fe Option A (owner-confirmed 2026-07-15): the pitch tab's timing AND
        // pitch are ALWAYS baked into the cache -- never the realtime file-read
        // warp.  A not-yet-baked edit plays DRY (raw) through the async bake
        // window (the decode varispeed was the pre-bake "funky/rolling"
        // artifact); once the cache publishes, processFilePlay owns the sound.
        // QA-Fe2 item 3: the runtime-dead pitchMap/pitchOrigin decode branch is
        // REMOVED -- sourcePosAt is align-only (or linear).
        const bool wantWarp = alignActive;

        if (player.alignEngaged || wantWarp)
        {
            const juce::int64 T0 = ctx.projectStart + bufOffset;

            // The existing beat-domain / linear file-position law, callable
            // at ANY timeline sample (posDB above is rawLawAt(T0) exactly).
            auto rawLawAt = [&] (double timelineSample) -> double
            {
                if (TempoMap::isActive() && player.originalBPM > 0.f)
                {
                    const double beatsIn = TempoMap::beatAtSample (
                        (juce::int64) std::llround (timelineSample))
                        - player.clipStartBeat;
                    return clipFilePosForBeat (beatsIn, player.fileSampleRate,
                                               (double) player.originalBPM,
                                               (double) contentStart);
                }
                return (timelineSample - (double) clipStart) * readRatio
                       + (double) contentStart;
            };

            juce::int64 alignOrigin = 0;
            if (ae != nullptr)
            {
                // Composite t=0 in CURRENT-rate timeline samples.  The stored
                // origin is exact while the device rate matches analysis; on
                // a rate change, re-derive from the origin beat (seconds are
                // rate-invariant, sample frames are not).
                alignOrigin = ae->snap->commonStartSample;
                if (std::abs (ae->snap->analysisSampleRate - mSampleRate) > 0.5)
                    alignOrigin = TempoMap::isActive()
                        ? TempoMap::sampleAtBeat (ae->snap->commonStartBeat)
                        : (juce::int64) std::llround (ae->snap->commonStartBeat
                                                      * secPerBeat * mSampleRate);
            }
            // SOURCE position (timeline-equivalent samples): align maps output
            // time onto the edited performance; pitch timing is baked, so no
            // second stage exists (QA-Fe2 item 3).
            auto sourcePosAt = [&] (double timelineSample) -> double
            {
                double x = timelineSample;
                if (alignActive)
                {
                    const double g = (x - (double) alignOrigin) / mSampleRate;
                    double d = g, dPerG = 1.0; float semis = 0.0f;
                    ae->snap->lookupAtGuideSec (g, d, dPerG, semis);
                    x = (double) alignOrigin + d * mSampleRate;
                }
                return x;
            };
            auto warpLawAt = [&] (double timelineSample) -> double
            {
                return rawLawAt (sourcePosAt (timelineSample));
            };

            const double lawStart = wantWarp ? warpLawAt ((double) T0)
                                             : rawLawAt  ((double) T0);
            const double lawEnd   = wantWarp ? warpLawAt ((double) (T0 + outSamples))
                                             : rawLawAt  ((double) (T0 + outSamples));

            float semisTarget = 0.0f;
            if (alignActive && ae->pitchOn)
            {
                const double g0 = ((double) T0 - (double) alignOrigin) / mSampleRate;
                double d = g0, dPerG = 1.0; float semis = 0.0f;
                ae->snap->lookupAtGuideSec (g0, d, dPerG, semis);
                semisTarget = semis + ae->transpose;
            }

            const double blockSec = (double) outSamples / mSampleRate;
            const double glideK   = juce::jmin (1.0, blockSec / 0.05);   // rule 5: ~50 ms

            int  fadeMode  = 0;     // +1 = direct->PV engage fade, -1 = PV->direct exit fade
            bool engageOk  = true;
            bool forceSeek = false; // QA-Fd: detach-pill law jump -> hard resync

            const bool contiguous = (player.alignLastEndTimeline == T0)
                                 && (player.alignLastLawEnd >= 0.0);

            if (! player.alignEngaged)
            {
                // Engage.  If the clip was audible last block (mid-play
                // toggle), start exactly where the pristine path was reading
                // and glide to the warped law -- no position jump.  A fresh
                // clip start / play-from-middle starts at the warped law
                // directly (nothing was audible to glide from).
                const bool wasAudible = (player.alignLastEndTimeline == T0);
                player.alignPosCorr = wasAudible
                    ? rawLawAt ((double) T0) - lawStart : 0.0;
                player.alignInFrac  = 0.0;
                player.alignRho     = 1.0;
                player.alignEngaged = true;
                if (! usePV)
                    fadeMode = +1;   // pristine side was the direct path
            }
            else if (! contiguous)
            {
                // Transport seek / loop wrap / re-entry: hard resync (seeks
                // jump by design -- the glide is for law changes only).
                player.alignPosCorr = 0.0;
                player.alignInFrac  = 0.0;
                player.alignRho     = std::pow (2.0, (double) semisTarget / 12.0);
            }
            // alignLastLawEnd stores the ACTUAL end read position (law +
            // outstanding correction), so the law-change test must strip the
            // correction back out before comparing law ends -- comparing the
            // raw stored value re-detected "a change" on every mid-glide
            // block, and the old += then COMPOUNDED the correction ~1.9x per
            // block (the G2-boundary runaway: starved PV = dropout, position
            // pinned wrong until a player rebuild).
            else if (std::abs ((player.alignLastLawEnd - player.alignPosCorr)
                               - lawStart) > 4.0)
            {
                // QA-Fd detach-pill jump: crossing a detached boundary makes
                // the law JUMP (relocated audio) -- that is a cut, not a bend
                // the 2:1 glide should grind through (a backward jump could
                // never drain at all: consumption is forward-only).  Hard
                // resync through the existing seek path instead.
                if (std::abs ((player.alignLastLawEnd - player.alignPosCorr)
                              - lawStart) > 0.25 * player.fileSampleRate)
                {
                    player.alignPosCorr = 0.0;
                    player.alignInFrac  = 0.0;
                    forceSeek = true;
                }
                else
                {
                    // Law changed under us (the ON/OFF toggle -- analyze/
                    // revert are stop-gated): REBASE onto the new law so the
                    // read stays continuous, then drain the difference out.
                    player.alignPosCorr = player.alignLastLawEnd - lawStart;
                }
            }

            const double corrBefore = player.alignPosCorr;

            // Drain wanted by the ~50 ms exponential, capped so the audible
            // bend never exceeds 2:1 either direction -- the reference
            // aligner's own deliberate warp restriction (owner calibration
            // 2026-07-10; glide time ~= the offset being traveled).  The
            // uncapped drain demanded backward consumption for any real-size
            // alignment offset and starved the vocoder.
            const double lawAdvance = lawEnd - lawStart;
            double drain = corrBefore * glideK;
            if (lawAdvance > 0.0)
                drain = juce::jlimit (-lawAdvance, 0.5 * lawAdvance, drain);
            else
                drain = 0.0;

            const double rhoTarget = std::pow (2.0, (double) semisTarget / 12.0);
            player.alignRho += (rhoTarget - player.alignRho) * glideK;
            if (std::abs (player.alignRho - 1.0) < 1.0e-4) player.alignRho = 1.0;

            const double pStart = lawStart + corrBefore;
            double tc = lawAdvance - drain;
            // Degenerate guard (extreme warp slope could zero a block's
            // consumption; the invariant below re-absorbs whatever it alters).
            tc = juce::jmax (tc, (double) outSamples * readRatio / 64.0);

            // Exact-bookkeeping invariant: the outstanding correction is
            // DERIVED from the consumption actually scheduled, so the law
            // bookkeeping and the fed stream can never drift apart (the
            // pre-fix divergence sat below the 2 s seek net and never
            // healed while the chain stayed engaged).
            player.alignPosCorr = (pStart + tc) - lawEnd;
            if (std::abs (player.alignPosCorr) < 1.0e-3) player.alignPosCorr = 0.0;

            const bool settled = corrBefore == 0.0 && player.alignPosCorr == 0.0
                              && player.alignRho == 1.0;

            if (! wantWarp && settled)
            {
                // Chain OFF and fully glided home: hand decode back to the
                // pristine paths.  PV-native clips continue seamlessly (same
                // law, same machinery); direct clips get the one-block
                // crossfade below, at identical read positions.
                if (usePV)
                {
                    player.alignEngaged    = false;
                    player.alignLastLawEnd = -1.0;
                }
                else
                    fadeMode = -1;
            }

            if (player.alignEngaged || fadeMode == -1)
            {
                const double rReqRaw = (double) outSamples * readRatio
                                       * player.alignRho / tc;
                const double rReq = juce::jlimit (1.0 / 64.0, 64.0, rReqRaw);
                const double rEff = (double) juce::jmax (1, juce::roundToInt (
                                        (double) PhaseVocoder::kHopSize * rReq))
                                    / (double) PhaseVocoder::kHopSize;
                // Position is the hard target: the (hop-quantized) effective
                // stretch feeds back into the output read rate so file
                // consumption stays law-exact; the ~0.1% quantization lands
                // on pitch (cents) instead.
                const double outRate = tc * rEff / (double) outSamples;

                if (fadeMode == +1)
                {
                    // Prime the vocoder so its first delivered output sample
                    // corresponds to input position pStart (OLA accounting:
                    // output o <-> input (o - N/2) / rEff + N/2, relative to
                    // the reset).  Feed comes from the clip source, which is
                    // resident around the position the direct path was
                    // just reading.
                    player.vocoder->reset();
                    player.vocoder->setStretchRatio (rReq);
                    // LIVE window, not the compile-time maximum: the window is a
                    // duration now, so a 96/192 kHz file runs a larger FFT.  Left
                    // as the constant, feedCap fed 12,288 samples where the prime
                    // needs 20,480, the engage gate below never passed, and every
                    // warped Vox clip at those rates fell silently back to the
                    // direct path - align simply never engaged.
                    const int    winNow = player.vocoder->getFFTSize();
                    const int    kPre   = winNow;
                    const double pStarTarget =
                        ((double) kPre - (double) winNow * 0.5) * rEff
                        + (double) winNow * 0.5;
                    juce::int64 feedPos = (juce::int64) std::llround (pStart) - kPre;
                    int fed = 0;
                    const int feedCap = winNow * 6;
                    while (player.vocoder->getOutputAvailable()
                               < (int) std::ceil (pStarTarget) + 16
                           && fed < feedCap)
                    {
                        const int chunk = PhaseVocoder::kHopSize;
                        player.pvInBuf.clear (0, chunk);
                        if (feedPos >= 0
                            && ! player.source->readRaw (player.pvInBuf, 0,
                                                           chunk, feedPos))
                        {
                            engageOk = false;
                            break;
                        }
                        player.vocoder->push (player.pvInBuf, 0, chunk);
                        feedPos += chunk;
                        fed     += chunk;
                    }
                    if (engageOk && player.vocoder->getOutputAvailable()
                                        >= (int) pStarTarget)
                    {
                        const int adv = (int) pStarTarget;
                        player.vocoder->advanceOutput (adv);
                        player.pvOutFrac       = pStarTarget - (double) adv;
                        player.expectedFilePos =
                            (juce::int64) std::llround (pStart) - kPre + fed;
                    }
                    else
                        engageOk = false;

                    if (! engageOk)
                    {
                        // Ring not resident (fresh seek etc.): stay on the
                        // original path this block, retry the engage next.
                        player.alignEngaged    = false;
                        player.alignPosCorr    = 0.0;
                        player.alignLastLawEnd = -1.0;
                    }
                }

                if (engageOk)
                {
                    player.vocoder->setStretchRatio (rReq);

                    // Seek detection on the effective position (mirror of the
                    // pristine PV path; requestSeek, never seek()).  QA-Fd:
                    // forceSeek = a detach-pill law jump this block.
                    const juce::int64 pvRefPos  = (juce::int64) std::llround (pStart);
                    const juce::int64 pvReadPos = player.expectedFilePos;
                    // unmuteResync skips a just-primed engage (fadeMode +1):
                    // the prefill above already positioned the feed at pStart;
                    // a forced reset here would empty the OLA it just built.
                    const bool seekNeeded = forceSeek ||
                        (unmuteResync && fadeMode != +1) ||
                        (pvReadPos == 0 && pvRefPos > (juce::int64) mSampleRate) ||
                        (pvReadPos  > 0
                         && std::abs (pvRefPos - pvReadPos) > (juce::int64)(mSampleRate * 2));
                    if (seekNeeded)
                    {
                        player.vocoder->reset();
                        player.source->requestSeek (pvRefPos);
                        player.expectedFilePos = pvRefPos;
                        player.pvOutFrac       = 0.0;
                        player.alignInFrac     = 0.0;
                    }

                    // Law-exact input feed: floor + fractional carry (the
                    // pristine path's ceil would slowly inflate the OLA
                    // queue; the carry keeps consumption == law long-run).
                    // Capacity clamp = readRaw writes dest[0..n) unchecked.
                    const double needIn = tc + player.alignInFrac;
                    const int numFileSamples = juce::jlimit (0,
                        player.pvInBuf.getNumSamples(), (int) needIn);
                    player.alignInFrac = needIn - (double) numFileSamples;

                    player.pvInBuf.clear (0, numFileSamples);
                    const bool gotRaw = numFileSamples > 0
                        && player.source->readRaw (player.pvInBuf, 0,
                                                     numFileSamples,
                                                     player.expectedFilePos);
                    if (gotRaw)
                    {
                        player.expectedFilePos += numFileSamples;
                        player.vocoder->push (player.pvInBuf, 0, numFileSamples);
                    }
                    // gotRaw false: expectedFilePos NOT advanced - retry next
                    // block (pristine-path convention).

                    if (fadeMode != 0)
                    {
                        // Complementary direct leg at the SAME read position
                        // (no flam): engage fades it out, exit fades it in.
                        const double dirRate = tc / (double) outSamples;
                        player.source->readAndMix (clipScratch, bufOffset,
                                                     outSamples, pStart, dirRate,
                                                     numOut, gain);
                        for (int i = 0; i < outSamples; ++i)
                        {
                            const float t  = ((float) i + 0.5f) / (float) outSamples;
                            const float g2 = (fadeMode > 0) ? (1.0f - t) : t;
                            for (int ch = 0; ch < numOut; ++ch)
                                clipScratch.setSample (ch, bufOffset + i,
                                    clipScratch.getSample (ch, bufOffset + i) * g2);
                        }
                    }

                    {
                        const double needF   = player.pvOutFrac
                                             + (double) outSamples * outRate;
                        const int    consume = (int) needF;
                        const int    peekWant = juce::jmin (consume + 2,
                                                            player.pvOutBuf.getNumSamples());
                        player.pvOutBuf.clear (0, peekWant);
                        const int peeked = player.vocoder->peekOutput (
                            player.pvOutBuf, 0, peekWant);
                        if (peeked > 0)
                        {
                            const int pvCh = player.pvOutBuf.getNumChannels();
                            for (int i = 0; i < outSamples; ++i)
                            {
                                const double exactFP = player.pvOutFrac
                                                     + (double) i * outRate;
                                const int    ip      = (int) exactFP;
                                const float  frac    = (float)(exactFP - ip);
                                if (ip + 1 >= peeked) break;
                                const int im1 = juce::jmax (0, ip - 1);
                                const int ip2 = juce::jmin (ip + 2, peeked - 1);
                                float fadeG = 1.0f;
                                if (fadeMode != 0)
                                {
                                    const float t = ((float) i + 0.5f)
                                                  / (float) outSamples;
                                    fadeG = (fadeMode > 0) ? t : (1.0f - t);
                                }
                                for (int ch = 0; ch < numOut; ++ch)
                                {
                                    const int   srcCh = ch % pvCh;
                                    const float p0 = player.pvOutBuf.getSample (srcCh, im1);
                                    const float p1 = player.pvOutBuf.getSample (srcCh, ip);
                                    const float p2 = player.pvOutBuf.getSample (srcCh, ip + 1);
                                    const float p3 = player.pvOutBuf.getSample (srcCh, ip2);
                                    const float v  = (p1 + 0.5f * frac * ((p2 - p0)
                                                   + frac * (2.f * p0 - 5.f * p1 + 4.f * p2 - p3
                                                   + frac * (3.f * (p1 - p2) + p3 - p0))))
                                                   * gain * fadeG;
                                    clipScratch.addSample (ch, bufOffset + i, v);
                                }
                            }
                            const int adv = juce::jmin (consume, peeked);
                            player.vocoder->advanceOutput (adv);
                            player.pvOutFrac = needF - (double) adv;
                            if (player.pvOutFrac >= 1.0)
                                player.pvOutFrac -= std::floor (player.pvOutFrac);
                        }
                    }

                    if (fadeMode == -1)
                    {
                        // Exit fade complete: the clip is fully back on the
                        // pristine direct path from the next block.
                        player.alignEngaged    = false;
                        player.alignLastLawEnd = -1.0;
                        player.expectedFilePos =
                            (juce::int64) std::llround (pStart + tc);
                    }
                    else
                        player.alignLastLawEnd = lawEnd + player.alignPosCorr;

                    // QA-Fd: stamp the block's SOURCE positions for the pill
                    // applicator.  The outstanding glide correction (file
                    // frames) folds back through the raw-law slope so the
                    // stamp tracks what is actually audible, not just the
                    // law target (glide-exact to first order).
                    {
                        const double x0 = sourcePosAt ((double) T0);
                        const double x1 = sourcePosAt ((double) (T0 + outSamples));
                        const double rawSlope = juce::jmax (1.0e-9,
                            (rawLawAt ((double) (T0 + outSamples)) - rawLawAt ((double) T0))
                            / (double) juce::jmax (1, outSamples));
                        ownEntry.srcX0   = x0 + corrBefore / rawSlope;
                        ownEntry.srcRate = (x1 - x0) / (double) juce::jmax (1, outSamples);
                        ownEntry.srcSet  = true;
                    }
                    warpHandled = true;
                }
            }
        }
    }

    if (! warpHandled)
    {
    if (usePV)
    {
        // ── Phase vocoder path (BPM stretch + pitch preservation) ────────────
        player.vocoder->setStretchRatio (stretchRatio);

        // QA-Ea Task 0c: mirror of Site A pvRefPos offset (Vox/Inst FilePlay).
        // QA-Ec G1-boundary fix: beat-domain when the timeline is live (posDB
        // already IS the source frame - consumption per beat is mode-blind).
        const int64 pvRefPos  = TempoMap::isActive() && player.originalBPM > 0.f
            ? (int64) std::llround (posDB)
            : (int64) ((double) outPosInClip * readRatio / stretchRatio) + contentStart;
        const int64 pvReadPos = player.expectedFilePos;

        const bool seekNeeded = unmuteResync ||
            (pvReadPos == 0 && pvRefPos > (int64) mSampleRate) ||
            (pvReadPos  > 0
             && std::abs (pvRefPos - pvReadPos) > (int64)(mSampleRate * 2));

        if (seekNeeded)
        {
            player.vocoder->reset();
            // G1 smoke round 6: requestSeek, NOT seek() - see Path A (audio-
            // thread disk IO under the reader lock = blown deadline).
            player.source->requestSeek (pvRefPos);
            player.expectedFilePos = pvRefPos;
            player.pvOutFrac       = 0.0;   // G1 fix: fresh fractional output position
        }

        // Clamp to pvInBuf capacity, same reason and same shape as the Site A
        // clamp in renderAudioClipsForRow and the align-warp clamp above: the
        // demand is a RATIO of the block, so it overruns readRaw's write on the
        // extremes of the matrix.  stretchRatio floors at 1/64 (a clip whose BPM
        // tag survived the UI's jmax(1.f) floor pins it there), which asks for
        // 64x the block -- 65,536 samples at a 1024 buffer against a 34,816-
        // sample allocation.  Truncating degrades to a brief glitch on that
        // degenerate clip instead of corrupting memory.
        const int numFileSamples = (int) juce::jmin (
            (juce::int64) std::ceil ((double) outSamples * readRatio / stretchRatio),
            (juce::int64) player.pvInBuf.getNumSamples());

        player.pvInBuf.clear (0, numFileSamples);
        const bool gotRaw = player.source->readRaw (
            player.pvInBuf, 0, numFileSamples, player.expectedFilePos);

        if (gotRaw)
        {
            player.expectedFilePos += numFileSamples;
            player.vocoder->push (player.pvInBuf, 0, numFileSamples);

            // QA-Ec G1-boundary fix: peek + fractional advance (mirror of
            // Path A - see its comment; the pull() lookahead-discard clicked
            // at every block boundary on stretched clips).
            const double needF   = player.pvOutFrac + (double) outSamples * readRatio;
            const int    consume = (int) needF;
            const int    peekWant = juce::jmin (consume + 2,
                                                player.pvOutBuf.getNumSamples());
            player.pvOutBuf.clear (0, peekWant);
            const int peeked = player.vocoder->peekOutput (player.pvOutBuf, 0, peekWant);

            if (peeked > 0)
            {
                const int pvCh = player.pvOutBuf.getNumChannels();
                for (int i = 0; i < outSamples; ++i)
                {
                    const double exactFP = player.pvOutFrac + (double) i * readRatio;
                    const int    ip      = (int) exactFP;
                    const float  frac    = (float)(exactFP - ip);

                    if (ip + 1 >= peeked) break;

                    // G1 smoke round 5: Catmull-Rom (was linear) - see Path A.
                    const int im1 = juce::jmax (0, ip - 1);
                    const int ip2 = juce::jmin (ip + 2, peeked - 1);

                    for (int ch = 0; ch < numOut; ++ch)
                    {
                        const int   srcCh = ch % pvCh;
                        const float p0    = player.pvOutBuf.getSample (srcCh, im1);
                        const float p1    = player.pvOutBuf.getSample (srcCh, ip);
                        const float p2    = player.pvOutBuf.getSample (srcCh, ip + 1);
                        const float p3    = player.pvOutBuf.getSample (srcCh, ip2);
                        const float v     = (p1 + 0.5f * frac * ((p2 - p0)
                                          + frac * (2.f * p0 - 5.f * p1 + 4.f * p2 - p3
                                          + frac * (3.f * (p1 - p2) + p3 - p0)))) * gain;
                        clipScratch.addSample (ch, bufOffset + i, v);
                        peak = juce::jmax (peak, std::abs (v));
                    }
                }
                const int adv = juce::jmin (consume, peeked);
                player.vocoder->advanceOutput (adv);
                player.pvOutFrac = needF - (double) adv;
                // G1 smoke round 4: drop whole-sample shortfall debt (see
                // Path A) - carrying it compounds into an unbounded demand.
                if (player.pvOutFrac >= 1.0)
                    player.pvOutFrac -= std::floor (player.pvOutFrac);
            }
        }
        // gotRaw false: expectedFilePos NOT advanced - retry next block.
    }
    else
    {
        // ── Direct path: SR-only interpolation (no BPM stretch) ──────────────
        // posDB keeps the fractional position (sub-sample block continuity).
        peak = player.source->readAndMix (
            clipScratch, bufOffset, outSamples, posDB, readRatio, numOut, gain);
        player.expectedFilePos = (int64) std::llround (posDB + (double) outSamples * readRatio);
    }
    }   // !warpHandled (QA-Fa recovery: pristine paths untouched when the warp regime decoded)

    // QA-Fa recovery: audibility tracker for every path -- an engage glide
    // (vs a hard start at the warped law) is only correct when the clip was
    // actually sounding in the immediately-previous block.
    if (isVoxRoute)
        player.alignLastEndTimeline = ctx.projectStart + bufOffset + outSamples;

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

    // ── QA-MultiBlockHazard (Task 2): sum this clip's RAW decode into sumDest ──
    // The engine + insert chain run ONCE per block on the sum in
    // finalizeFilePlayStrip -- not per clip -- so a stateful engine + rack
    // advance once per block instead of once per FilePlay clip.
    {
        const int nc = juce::jmin (sumDest.getNumChannels(), clipScratch.getNumChannels());
        for (int c = 0; c < nc; ++c)
            sumDest.addFrom (c, 0, clipScratch, c, 0, numSamples);
    }
    return true;
}

// finalizeFilePlayStrip: run the Vox/Inst engine + insert chain ONCE on the
// summed FilePlay clips (engineSum, filled by decodeFilePlayClip), then route
// into mtDest.  routeCh = the strip's channel id (all summed clips share it).
// See header for invariants.
void BaySickDAWProcessor::finalizeFilePlayStrip (int                          routeCh,
                                                const AudioClipBlockContext& ctx,
                                                juce::MidiBuffer&            engineMidi,
                                                juce::AudioBuffer<float>*    mtDest,
                                                juce::AudioBuffer<float>&    engineSum)
{
    if (mtDest == nullptr) return;

    const bool isVoxRoute  = routeCh >= MixerChannelIds::kVoxBase
                           && routeCh <  MixerChannelIds::kVoxBase + kMaxVoxPages;
    const bool isInstRoute = routeCh >= MixerChannelIds::kInstBase
                           && routeCh <  MixerChannelIds::kInstBase + kMaxInstPages;
    if (! isVoxRoute && ! isInstRoute) return;

    const int numSamples = ctx.numSamples;

    // pushScToEng: inline the processBlock stack lambda via dynamic_cast to
    // ISidechainEngine + setSidechainBuffers.
    auto pushScToEng = [this] (juce::AudioProcessor* eng, int channelId)
    {
        if (auto* sc = dynamic_cast<ISidechainEngine*> (eng))
        {
            const auto arr = mVibeGraph.getScRecvArray (channelId);
            juce::AudioBuffer<float>* bufs[BaySickGraph::kMaxScRecvSlots];
            for (int s = 0; s < BaySickGraph::kMaxScRecvSlots; ++s)
                bufs[s] = arr[(size_t) s];
            sc->setSidechainBuffers (bufs, BaySickGraph::kMaxScRecvSlots);
        }
    };

    if (isVoxRoute)
    {
        const int vi = routeCh - MixerChannelIds::kVoxBase;
        auto* eng = mVoxEngines[(size_t) vi];
        if (eng == nullptr) return;

        // setForcePitchBypass(true) - realtime pitch was baked into the wet
        // recording at capture time, so don't double-apply on FilePlay.
        // QA-Fa: stamp the block's timeline position alongside it -- the
        // BaySickPitch applicator maps this strip's audio onto composite
        // note regions by absolute position.  QA-Fd: forward the decode
        // layer's source-position stamp (identity when no law engaged) so
        // the applicator resolves pills in the SOURCE domain.
        if (auto* vp = dynamic_cast<BaySickVocalProcessor*> (eng))
        {
            vp->setForcePitchBypass (true);
            vp->setFilePlayTimelineSample (ctx.projectStart);
            const auto& e = mBlockAlignEntries[(size_t) vi];
            vp->setFilePlaySourceStamp (e.srcX0, e.srcRate, e.srcSet);
        }

        pushScToEng (eng, MixerChannelIds::voxInsert (vi));
        eng->processBlock (engineSum, engineMidi);
        mVibeGraph.processInsert (BaySickGraph::InsertKind::Vox, vi,
                                   engineSum, ctx.bpm, ctx.anySolo);

        const int nc = juce::jmin (mtDest->getNumChannels(), engineSum.getNumChannels());
        for (int c = 0; c < nc; ++c)
            mtDest->addFrom (c, 0, engineSum, c, 0, numSamples);
    }
    else   // isInstRoute
    {
        const int ii = routeCh - MixerChannelIds::kInstBase;
        auto* eng = mInstEngines[(size_t) ii];
        if (eng == nullptr) return;

        pushScToEng (eng, MixerChannelIds::instInsert (ii));
        eng->processBlock (engineSum, engineMidi);
        mVibeGraph.processInsert (BaySickGraph::InsertKind::Inst, ii,
                                   engineSum, ctx.bpm, ctx.anySolo);

        const int nc = juce::jmin (mtDest->getNumChannels(), engineSum.getNumChannels());
        for (int c = 0; c < nc; ++c)
            mtDest->addFrom (c, 0, engineSum, c, 0, numSamples);
    }
}

// ── processBlock ──────────────────────────────────────────────────────────────
void BaySickDAWProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midiMessages)
{
    // THREAD SAFETY: the acknowledgement settleAudioThread waits on.  It has to
    // be the FIRST statement -- ahead of the shield's early-out below -- because
    // a shielded block that never acknowledged would leave every settle burning
    // its full timeout instead of returning in a block or two.
    mAudioBlockCounter.fetch_add (1, std::memory_order_release);

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
    // The generation is published into the RetirementQueue via
    // setInUseGeneration (release-store after the load-acquire), which is what
    // closes the GC race: the drainer acquire-loads that published gen, so a
    // retired snapshot whose retiredBeforeGen is still ahead of it stays alive
    // until this audio thread loads a newer snapshot in a future block.
    {
        auto* snap = mActiveAudioClips.load (std::memory_order_acquire);
        // Bootstrap (ctor) guarantees this is non-null on first entry; the
        // mutator only ever publishes non-null pointers.
        mCurrentBlockClipSnapshot = snap;
        mClipRetirement.setInUseGeneration (snap->generation);
    }

    // The roll queue's generation gate advances only when someone acquires a
    // snapshot, and the note scheduler below acquires only while the transport
    // is playing -- so a stopped transport froze the gate and every roll edit's
    // retired table piled up behind it.  The result is deliberately discarded:
    // this call exists for the generation publish alone.  Wait-free and
    // allocation-free (one acquire-load plus one release-store), same cost as
    // the clip publish above.
    if (mPatternManager != nullptr)
        (void) mPatternManager->acquireRollSnapshot();

    // QA-Fa recovery: capture each Vox engine's published align-warp
    // snapshot + chain/pitch gates ONCE per block (see mBlockAlignEntries).
    // Idle cost = kMaxVoxPages casts + atomic loads per BLOCK, not per clip.
    for (int vi = 0; vi < kMaxVoxPages; ++vi)
    {
        auto& e = mBlockAlignEntries[(size_t) vi];
        e = {};
        if (auto* vp = dynamic_cast<BaySickVocalProcessor*> (mVoxEngines[(size_t) vi]))
        {
            const auto* s = vp->loadAlignPlaySnapshot();
            if (s != nullptr && s->isUsable())
            {
                e.snap      = s;
                e.chainOn   = vp->isAlignChainOn();
                e.pitchOn   = vp->isAlignPitchOn();
                e.transpose = vp->alignTransposeSemis();
            }
        }
    }

    // ── Render dispatch ────────────────────────────────────────────────────
    // Batch 9a (2026-05-06): the dispatch site lives AFTER all MIDI scheduling
    // + anySolo computation + routing-graph rebuild.  Reason: BlockContext
    // needs to carry per-engine MIDI buffer pointers, anySolo, the live-input
    // snapshot, and a routing-graph-aware predecessor list.  Building that
    // context cleanly requires the pre-dispatch code below to populate the
    // inputs first.  See the dispatch site after applyChokeGroupDispatch().
    //
    // QA-Ef (2026-05-21): the dispatcher is the single render path; the legacy
    // serial render tail was deleted and the gMultiThreadedEngineEnabled flag
    // now gates worker-park vs full-parallel within the same dispatcher.  The
    // project-load barrier above gates the single path, so the render dispatch
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
        // Never-fires safety net now that prepareToPlay sizes for the real
        // input count.  GROW-ONLY: passing numSamples verbatim would SHRINK
        // the prepared buffer on a driver whose first callback is short, and
        // the next full-size block would allocate again.  Deliberately not a
        // clamp on numInputs -- silently dropping channels would silently kill
        // armed Vox / Inst strips on the dropped ones.
        if (mLiveInputSnapshot.getNumChannels() < numInputs
            || mLiveInputSnapshot.getNumSamples() < numSamples)
            mLiveInputSnapshot.setSize (juce::jmax (numInputs, mLiveInputSnapshot.getNumChannels()),
                                        juce::jmax (numSamples, mLiveInputSnapshot.getNumSamples()),
                                        false, false, true);
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
    auto* ph = getPlayHead();
    if (ph != nullptr)
        if (auto optPos = ph->getPosition())
            pos = *optPos;

    // ── Child engines need the playhead too (2026-07-30) ─────────────────────
    // NOTHING ever called setPlayHead on a child engine -- the only call sites
    // in the tree are on THIS processor -- so every engine's getPlayHead()
    // returned null and Harmless / BaySickSynth / BaySickBass silently fell back
    // to their "no transport" default of 120 BPM.  Net effect: tempo-synced LFO
    // rates and envelope times did not follow project tempo AT ALL.  Set the
    // song to 90 or 140 and those still ran at 120.
    //
    // Propagated on CHANGE rather than every block (a pointer compare, then N
    // pointer stores only when it actually moves).  It has to react to changes,
    // not just run once at startup: the offline render SWAPS this processor's
    // playhead for its own and swaps it back, so a one-shot at prepare would
    // leave every engine pointing at a dead render head afterwards.
    if (ph != mLastEnginePlayHead)
    {
        mLastEnginePlayHead = ph;
        if (mEngineRig != nullptr)
            mEngineRig->forEachEngine ([ph] (juce::AudioProcessor& p) { p.setPlayHead (ph); });
        for (auto& g : mGuitarsEngine) if (g) g->setPlayHead (ph);
        for (auto& b : mBassesEngine)  if (b) b->setPlayHead (ph);
        if (mRustyDrumsEngine) mRustyDrumsEngine->setPlayHead (ph);
    }

    // Publish transport state for editor panels (see DSPBase::sTransportPlaying:
    // panels lock latency-changing controls while the transport runs).
    // NOT during an offline render: the OfflineHead reports playing with its
    // own positions, so an export was publishing LIVE transport signals --
    // panels locked their latency controls, phantom play/wrap edges opened
    // version-capture takes, and the master Integrated window reset mid-song.
    // Same guard as the recorders and the metronome, missed on these.
    if (! isNonRealtime())
    {
        DSPBase::sTransportPlaying.store (pos.getIsPlaying(), std::memory_order_relaxed);

        // QA-RustyMeter Task 3 (2026-05-30): reset the master LUFS Integrated
        // window on transport play-from-top / loop-start.  Edge =
        // stopped->playing, OR a backward ppq jump while playing (loop wrap /
        // relocate-to-start).  Done before the graph runs so this block opens
        // a fresh Integrated window.  Momentary + Short-Term keep tracking
        // continuously (inside LufsMeterDSP).
        const bool   lufsPlaying = pos.getIsPlaying();
        const double lufsPpq     = pos.getPpqPosition().orFallback (0.0);
        const bool   playStarted = lufsPlaying && ! mLufsWasPlaying;
        // A backward ppq jump while playing is a loop wrap OR a relocate; both
        // start a fresh Integrated window, and both end a capture take.
        const bool   loopWrapped = lufsPlaying && ! playStarted
                                   && lufsPpq + 1.0e-6 < mLufsLastPpq;
        if (playStarted || loopWrapped)
            mVibeGraph.resetMasterLufsIntegrated();

        // TS7 §3.2: publish the SAME two edges for version capture rather than
        // running a second detector that could disagree with this one about
        // where a take begins.
        if (playStarted) mPlayStartEdges.fetch_add (1, std::memory_order_relaxed);
        if (loopWrapped) mLoopWrapEdges .fetch_add (1, std::memory_order_relaxed);

        mLufsWasPlaying = lufsPlaying;
        mLufsLastPpq    = lufsPpq;
    }

    // ── Layers piano roll: build MIDI from piano roll + incoming MIDI ─────
    // Members, not stack objects: clear() retains each buffer's allocation, so
    // steady-state MIDI assembly costs zero mallocs on the audio thread.  The
    // clear MUST happen here at the top -- the engine-side consumers do not
    // clear their own feed, so a stale buffer would replay last block's notes.
    auto& allMidi        = mAllMidi;
    auto& layerPageMidi  = mLayerPageMidi;   // per-page MIDI for layer engines
    auto& bassPageMidi   = mBassPageMidi;    // per-page MIDI for bass engines
    auto& drumPageMidi   = mDrumPageMidi;    // D1.2: per-drum-page MIDI (dynamic-drum model)
    auto& clipPageMidi   = mClipPageMidi;    // G-3 (2026-04-28): per-clip-page MIDI (sampler-style triggering)
    auto& voxPageMidi    = mVoxPageMidi;     // G-4 (2026-04-28): per-Vox-page MIDI
    auto& instPageMidi   = mInstPageMidi;    // G-4 (2026-04-28): per-Inst-page MIDI
    auto& pluginPageMidi = mPluginPageMidi;  // QA-ModelShell TS6: per-plugin-tab MIDI
    allMidi.clear();
    for (auto& b : layerPageMidi)  b.clear();
    for (auto& b : bassPageMidi)   b.clear();
    for (auto& b : drumPageMidi)   b.clear();
    for (auto& b : clipPageMidi)   b.clear();
    for (auto& b : voxPageMidi)    b.clear();
    for (auto& b : instPageMidi)   b.clear();
    for (auto& b : pluginPageMidi) b.clear();
    allMidi.addEvents(midiMessages, 0, numSamples, 0);

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
            // QA-ModelShell TS6 (BLU-447) -- MISSED, fixed TS7 2026-07-30.  The
            // SCHEDULER already emitted kPluginsPRTarget note-ons; all three
            // decode chains stopped at Rusty, so every plugin note-off was
            // silently dropped and the note hung until the panic CC.
            else if (off.target >= kPluginsPRTarget && off.target < kPluginsPRTarget + (int) kMaxPluginPages)
                pluginPageMidi[off.target - kPluginsPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
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
        for (auto& b : pluginPageMidi) b.addEvent(juce::MidiMessage::allNotesOff(1), 0);  // TS6
        // QA-L-Fix: drop trigger holds too -- the allNotesOff above already
        // silenced the voices, so leaving one armed would fire a stray
        // note-off into the next block.
        for (auto& h : mCcTriggerHolds)  h = {};
        for (auto& n : mNoteTriggerHeld) n = -1;
        mAnyCcHoldActive = false;
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
                    else if (off.target >= kPluginsPRTarget && off.target < kPluginsPRTarget + (int) kMaxPluginPages)
                        pluginPageMidi[off.target - kPluginsPRTarget].addEvent(juce::MidiMessage::noteOff(1, off.midiNote), 0);
                }
                mPRPendingOffs.clear();
            }
            mPRLastBlockStraddled = false;
        }
        else if (mPatternManager)
        {
            double bpmVal    = pos.getBpm().orFallback(120.0);
            double bs        = juce::jmax(1e-6, bpmVal / (60.0 * mSampleRate));
            double beatStart = pos.getPpqPosition().orFallback(0.0);
            double beatEnd   = beatStart + numSamples * bs;

            // ── QA-Ed: exact integer clock + sample-accurate loop-seam windows ─
            // mSamplePos is the playhead's exact int64 sample count; we share its
            // wrap point (computed from the SAME loopEnd beat + bpm + sr) so the
            // straddle test is integer-exact and can never disagree with the
            // playhead by a block.  A block that crosses the loop boundary builds
            // TWO windows: [beatStart, loopEnd) at sample 0, and the wrapped tail
            // [loopStart, loopStart+overshoot) starting at the exact in-block wrap
            // sample.  A note fires in at most one (disjoint, half-open) window,
            // so there is no double-trigger.  This replaces the float kWrapSlop /
            // jumped / windowStart / mPRLastBeatEnd band-aid entirely.
            const int64_t samplePos     = pos.getTimeInSamples().orFallback ((int64_t) 0);
            const double  loopStartBeat = mLoopStartBeats.load (std::memory_order_relaxed);
            const double  loopEndBeat   = mCachedPatternLoopBeats.load (std::memory_order_relaxed);
            const double  spb           = 60.0 * mSampleRate / juce::jmax (1e-6, bpmVal);
            // QA-TempoMap: with a published timeline, every beat<->sample
            // conversion goes through the map so tempo steps land sample-
            // exactly (incl. INSIDE a block); without one (legacy VST target,
            // host owns tempo) everything falls back to the pre-map linear
            // math below.
            const bool tmActive = TempoMap::isActive();
            if (tmActive)
                beatEnd = TempoMap::beatAtSample (samplePos + numSamples);   // exact across an in-block tempo step
            // Loop sample-bounds: the map is an absolute sample<->beat
            // function, so from-origin lookups are exact at any tempo; the
            // linear fallback measures from the CURRENT (samplePos, beatStart)
            // point, matching advanceBlock's pre-map math.
            const int64_t loopEndSamp   = loopEndBeat > 0.0
                ? (tmActive ? TempoMap::sampleAtBeat (loopEndBeat)
                            : samplePos + (int64_t) std::llround ((loopEndBeat   - beatStart) * spb)) : 0;
            const int64_t loopStartSamp = loopEndBeat > 0.0
                ? (tmActive ? TempoMap::sampleAtBeat (loopStartBeat)
                            : samplePos + (int64_t) std::llround ((loopStartBeat - beatStart) * spb)) : 0;
            const int64_t loopSpanSamp  = loopEndSamp - loopStartSamp;

            struct RollWindow { double winStart; double winEnd; int sampleBase; };
            RollWindow windows[2];
            int nWin    = 0;
            int wrapSmp = numSamples;   // off-pass sentinel = "no wrap in this block"
            // Sub-block-loop guard: a loop shorter than one block (span <
            // numSamples) spans >1 iteration per block, which the 2-window model
            // can't represent -> fall back to one window + the off clamp so
            // nothing hangs (degenerate; documented limitation).
            const bool straddle = loopEndBeat > 0.0 && loopSpanSamp >= (int64_t) numSamples
                               && samplePos < loopEndSamp && samplePos + numSamples > loopEndSamp;
            if (straddle)
            {
                wrapSmp = (int) juce::jlimit<int64_t> (0, numSamples - 1, loopEndSamp - samplePos);
                // QA-TempoMap: the wrapped tail's musical end derives through
                // the map from the post-wrap sample range (the tempo just
                // after loopStart can differ from the one at loopEnd).
                const double wrapEnd = tmActive
                    ? TempoMap::beatAtSample (loopStartSamp + (int64_t) (numSamples - wrapSmp))
                    : loopStartBeat + (beatEnd - loopEndBeat);
                windows[nWin++] = { beatStart,     loopEndBeat, 0 };
                windows[nWin++] = { loopStartBeat, wrapEnd,     wrapSmp };
            }
            else
            {
                windows[nWin++] = { beatStart, beatEnd, 0 };
            }

           #if JUCE_DEBUG
            // [G3 BAR1]: window readout on any block touching beat 0.  POD into
            // the diag ring -- a juce::String or a file write here would be an
            // allocation / blocking IO inside the render callback.
            if (beatStart < 0.05 || (nWin > 1 && windows[1].winStart < 0.05))
                G3PlayheadDiag::pushRT (
                    "[G3 BAR1] windows nWin/w0Start/w0End/w1Start/w1End/wrapSmp", 6,
                    (double) nWin,
                    windows[0].winStart, windows[0].winEnd,
                    nWin > 1 ? windows[1].winStart : 0.0,
                    nWin > 1 ? windows[1].winEnd   : 0.0,
                    (double) wrapSmp);
           #endif

            // QA-TempoMap: beat -> in-block sample offset within a window.
            // Map path: offset = exact sample distance from the window's
            // musical start (window 1's base re-anchors at the wrap sample).
            // Fallback: the pre-map linear division.
            auto beatToSmpInWindow = [&] (double absBeat, const RollWindow& w) -> int
            {
                if (tmActive)
                    return (int) juce::jlimit<int64_t> (0, (int64_t) numSamples - 1,
                        (int64_t) w.sampleBase
                          + (TempoMap::sampleAtBeat (absBeat) - TempoMap::sampleAtBeat (w.winStart)));
                return juce::jlimit (0, numSamples - 1,
                                     w.sampleBase + (int) ((absBeat - w.winStart) / bs));
            };

            // Decode a pending-off's target -> per-engine buffer + emit a noteOff.
            auto emitOff = [&] (const PRPendingOff& off, int smp)
            {
                if (off.target < kMaxLayerPages)
                    layerPageMidi[off.target].addEvent (juce::MidiMessage::noteOff (1, off.midiNote), smp);
                else if (off.target >= kBassPRTarget && off.target < kBassPRTarget + kMaxBassPages)
                    bassPageMidi[off.target - kBassPRTarget].addEvent (juce::MidiMessage::noteOff (1, off.midiNote), smp);
                else if (off.target >= kDrumPRTarget && off.target < kDrumPRTarget + kMaxDrumPages)
                    drumPageMidi[off.target - kDrumPRTarget].addEvent (juce::MidiMessage::noteOff (1, off.midiNote), smp);
                else if (off.target >= kClipPRTarget && off.target < kClipPRTarget + kMaxClipPages)
                    clipPageMidi[off.target - kClipPRTarget].addEvent (juce::MidiMessage::noteOff (1, off.midiNote), smp);
                else if (off.target >= kVoxPRTarget && off.target < kVoxPRTarget + kMaxVoxPages)
                    voxPageMidi[off.target - kVoxPRTarget].addEvent (juce::MidiMessage::noteOff (1, off.midiNote), smp);
                else if (off.target >= kInstPRTarget && off.target < kInstPRTarget + kMaxInstPages)
                    instPageMidi[off.target - kInstPRTarget].addEvent (juce::MidiMessage::noteOff (1, off.midiNote), smp);
                else if (off.target == kRustyPRTarget)
                    mRustyDrumsMidi.addEvent (juce::MidiMessage::noteOff (1, off.midiNote), smp);
                else if (off.target >= kPluginsPRTarget && off.target < kPluginsPRTarget + (int) kMaxPluginPages)
                    pluginPageMidi[off.target - kPluginsPRTarget].addEvent (juce::MidiMessage::noteOff (1, off.midiNote), smp);
            };

            // ── Pending note-off handling (shared by song + pattern) ────────
            // A backward seek (consumed once/block) cuts held notes at sample 0.
            // Past-due offs fire at 0; on a straddle, offs at/after loopEnd
            // (overrunning notes) fire at the exact wrap sample so they are cut
            // precisely at the loop boundary (no leak); in-block offs fire at
            // their sample; the rest are kept for a later block.
            const bool seekFlush = (mSeekDiscontinuity != nullptr
                                    && mSeekDiscontinuity->exchange (false, std::memory_order_acq_rel));
            // QA-Ee Task 1c: one-shot loop-wrap flush.  When the loop wrap landed
            // exactly on a block boundary the prior block was NOT a straddle, so a
            // note-off sitting at loopEnd matched none of the cases below and the
            // note hung until the next loop.  In the first post-wrap block, fire
            // those loop-end offs at sample 0 (mirrors the backward-seek flush).
            const bool loopFlush = (mLoopWrapped != nullptr
                                    && mLoopWrapped->exchange (false, std::memory_order_acq_rel));
            {
                // The loop-wrap flush only covers wraps the straddle path did
                // NOT handle (wrap exactly on a block boundary).  After a
                // straddle block, every pre-wrap off at/past loopEnd already
                // fired at the wrap sample -- an off found here at loopEnd
                // belongs to the seam-restarted full-pattern note, and
                // flushing it would cut that note one block into the new pass.
                const bool loopEndFlush = loopFlush && ! mPRLastBlockStraddled;
                mPRKeepScratch.clear();
                for (auto& off : mPRPendingOffs)
                {
                    if (seekFlush)                              { emitOff (off, 0);       continue; }
                    if (loopEndFlush && off.beatOff >= loopEndBeat) { emitOff (off, 0);   continue; }
                    if (off.beatOff <= beatStart)               { emitOff (off, 0);       continue; }
                    if (straddle && off.beatOff >= loopEndBeat) { emitOff (off, wrapSmp); continue; }
                    if (off.beatOff < beatEnd)
                    {
                        emitOff (off, beatToSmpInWindow (off.beatOff, windows[0]));
                        continue;
                    }
                    mPRKeepScratch.push_back (off);
                }
                mPRPendingOffs.swap (mPRKeepScratch);
            }
            mPRLastBlockStraddled = straddle;

            // ── Note-on scheduler (shared by both modes) ────────────────────
            // absOffset: pattern -> 0 (note.startBeat is loop-local); song ->
            // blkStartBeat (absolute song beat).  contentHi: song viewport end
            // (a note past the clip edge = silence) else +inf.  offHi: clamp the
            // note-off so an overrun is cut at the loop / clip boundary.
            // QA-G Task 5: contentLo masks notes that begin BEFORE the clip
            // window -- a sliced right piece must not re-trigger notes that
            // started left of the cut (tiles can extend before the block).
            auto scheduleRollWindows = [&] (const std::vector<PianoNote>& notes,
                                            juce::MidiBuffer& buf, int target,
                                            double absOffset, double contentHi, double offHi,
                                            double contentLo = -1.0e18,
                                            double swingAmt = 0.0, bool swingTrunc = false)
            {
                // S-1/S-5: slide emits deferred to AFTER the note-on loop so each
                // lands after its source's same-sample note-on regardless of note-
                // vector order (a MidiBuffer keeps equal-timestamp events in insertion
                // order).  Without this a co-starting source scheduled after the slide
                // would out-live an RT/Porta mono-cut, or be missed entirely by an RP
                // takeover bend.  Stack-local: no audio-thread allocation.  64 slide
                // note-ons per roll per block is far beyond any real pattern.
                struct MonoCut  { int note; int smp; };
                struct RampBend { const PianoNote* note; int anchor; int timeMs; int smp; };
                MonoCut  monoCuts [64];
                RampBend rampBends[64];
                int      nMonoCuts  = 0;
                int      nRampBends = 0;
                for (const auto& note : notes)
                {
                    if (note.muted) continue;
                    // SW-2/SW-4 (QA-G3Smoke Swing): scheduling-time transform.
                    // An off-beat 16th (odd index in the PATTERN-LOCAL grid;
                    // floor keeps humanized starts coherent) is pushed by
                    // global x player-mix x half a 16th.  Local, not absolute,
                    // so block placement never changes a pattern's feel.  The
                    // whole note shifts (its off rides rawStart below).
                    double localStart = note.startBeat;
                    bool   swung      = false;
                    if (swingAmt > 0.0
                        && (((int) std::floor (localStart / 0.25)) & 1) != 0)
                    {
                        localStart += swingAmt;
                        swung = true;
                    }
                    // SW-5: a pushed note may newly overlap the NEXT same-pitch
                    // note; the per-player Truncate toggle clips its off to that
                    // note's post-swing start (same-pitch scope only -- chords
                    // stay intact).
                    auto truncSwungOff = [&] (double off) -> double
                    {
                        if (! (swingTrunc && swung)) return off;
                        double nextLocal = 1.0e18;
                        for (const auto& n : notes)
                        {
                            if (&n == &note || n.muted || n.midiNote != note.midiNote) continue;
                            double ns = n.startBeat;
                            if ((((int) std::floor (n.startBeat / 0.25)) & 1) != 0) ns += swingAmt;
                            if (ns > localStart + 1.0e-6 && ns < nextLocal) nextLocal = ns;
                        }
                        return juce::jmin (off, absOffset + nextLocal);
                    };
                    const double rawStart = absOffset + localStart;
                    if (rawStart >= contentHi) continue;   // starts past the window end
                    // B-3: a note straddling the window's LEFT edge (contentLo) is
                    // clamped to the boundary and plays its remaining fragment (FL
                    // mid-note slice, NO copy) instead of being dropped.  Because
                    // contentLo is the block's fixed arrangement start, the clamped
                    // note only lands in a window on the block-crossing block, so it
                    // fires once - no per-block retrigger.  chainDur is computed once
                    // for the straddle test + reused for the note-off below.
                    double absStart = rawStart;
                    double chainDur = -1.0;
                    if (rawStart < contentLo)
                    {
                        chainDur = rampChainDurationBeats (notes, note);
                        if (rawStart + chainDur <= contentLo) continue;   // entirely before
                        absStart = contentLo;
                    }
                    for (int w = 0; w < nWin; ++w)
                    {
                        if (absStart >= windows[w].winStart && absStart < windows[w].winEnd)
                        {
                            const int smp = beatToSmpInWindow (absStart, windows[w]);
                            // Musical beats -> wall-clock ms at this position
                            // (map-exact under a TempoMap; linear otherwise).
                            auto beatsToMs = [&] (double atBeat, double beats) -> double
                            {
                                if (tmActive)
                                    return (double) (TempoMap::sampleAtBeat (atBeat + beats)
                                                     - TempoMap::sampleAtBeat (atBeat))
                                           * 1000.0 / mSampleRate;
                                return beats * spb * 1000.0 / mSampleRate;
                            };

                            if (note.type == NoteType::RampSlide)
                            {
                                // Takeover bend: no noteOn, no own note-off - the
                                // anchor's off was extended to the chain end when the
                                // anchor was scheduled.  Chain broken (nothing
                                // sounding) = silent.  S-1: a co-starting base now
                                // qualifies as the anchor; deferring the emit lets the
                                // takeover grab it at the shared sample.
                                const int anchor = findRampAnchorPitch (notes, note);
                                if (anchor >= 0 && nRampBends < 64)
                                    rampBends[nRampBends++] = { &note, anchor,
                                        (int) std::llround (beatsToMs (absStart, note.durationBeats)),
                                        smp };
                                break;
                            }

                            if (note.type == NoteType::Bend)
                            {
                                // Native pitch-wheel bend (Guitars/Basses): a plain
                                // noteOn + the bend transport; the sfizz processor
                                // ramps the wheel over the note in the chosen shape.
                                // Off scheduled like a normal note (B-3 straddle clamp).
                                const int timeMs = (int) std::llround (
                                    beatsToMs (absStart, note.durationBeats));
                                emitBend (buf, note, timeMs, smp);
                                if (chainDur < 0.0) chainDur = rampChainDurationBeats (notes, note);
                                double boff = truncSwungOff (rawStart + chainDur);
                                if (boff > offHi) boff = offHi;
                                mPRPendingOffs.push_back ({ boff, note.midiNote, target });
                                break;
                            }

                            int glideFrom = -1, glideMs = -1;
                            if (note.type == NoteType::Portamento)
                            {
                                glideFrom = findGlideSourcePitch (notes, note);
                                // S-4: Porta glides over its own per-note length in
                                // beats, ignoring the block/note length.
                                if (glideFrom >= 0)
                                    glideMs = (int) std::llround (
                                        beatsToMs (absStart, note.portaLengthBeats));
                            }
                            else if (note.type == NoteType::RetrigSlide)
                            {
                                glideFrom = findGlideSourcePitch (notes, note);
                                // S-3: RT glides over the slide note's own length.
                                if (glideFrom >= 0)
                                    glideMs = (int) std::llround (
                                        beatsToMs (absStart, note.durationBeats));
                            }
                            // S-5: RT + Porta are single-voice - record a mono-cut of
                            // the glide source at the slide's start.  Skipped when
                            // source == target (a degenerate zero-interval slide) so
                            // the fresh voice is never cut.
                            if (glideFrom >= 0 && glideFrom != note.midiNote
                                && nMonoCuts < 64)
                                monoCuts[nMonoCuts++] = { glideFrom, smp };
                            // #11 (G-4): RT glide note-ons carry pan as a CC89
                            // ramp target (glides over the slide); Porta + plain
                            // notes keep the instant CC10.
                           #if JUCE_DEBUG
                            if (absStart < 0.05)
                                G3PlayheadDiag::pushRT (
                                    "[G3 BAR1] noteOn pitch/absStart/smp/winStart/winEnd", 5,
                                    (double) note.midiNote, absStart, (double) smp,
                                    windows[w].winStart, windows[w].winEnd);
                           #endif
                            emitPianoNoteOn (buf, note, smp, glideFrom, glideMs,
                                             note.type == NoteType::RetrigSlide && glideFrom >= 0);
                            // B-3: the off is the note's ORIGINAL end (rawStart-based),
                            // so a clamped straddling note stops where it really ends,
                            // not full-duration past the boundary.
                            if (chainDur < 0.0) chainDur = rampChainDurationBeats (notes, note);
                            double off = truncSwungOff (rawStart + chainDur);
                            if (off > offHi) off = offHi;
                            mPRPendingOffs.push_back ({ off, note.midiNote, target });
                            break;   // a note fires in at most one window
                        }
                    }
                }
                // S-1/S-5: apply the deferred slide emits last, so each lands after
                // every note-on above.  RP takeover bends first (grab the anchor
                // voice), then RT/Porta mono-cuts (source cut, target voice untouched).
                for (int i = 0; i < nRampBends; ++i)
                    emitRampSlide (buf, *rampBends[i].note, rampBends[i].anchor,
                                   rampBends[i].timeMs, rampBends[i].smp);
                for (int i = 0; i < nMonoCuts; ++i)
                    buf.addEvent (juce::MidiMessage::noteOff (1, monoCuts[i].note),
                                  monoCuts[i].smp);
            };

            // #30b (G-6): ALL roll reads below go through the lock-free
            // snapshot -- one wait-free acquire per block; the message thread
            // republishes on every edit.  The old per-family try-locks
            // (drums/clips/vox/inst) silently DISCARDED every note-on in a
            // contended block, and layers/bass/rusty read live vectors bare
            // (torn reads under concurrent edits).  Both hazards end here.
            const SchedulerRollSnapshot* rollSnap = mPatternManager->acquireRollSnapshot();

            // QA-G3Smoke Swing (SW-4): effective push = global x player mix x
            // half a 16th (0.125 beats).  Cached raw param atomics; one global
            // load per block, one mix load per roll dispatch.  A null mix
            // pointer = full global (clip rolls -- Jeff 2026-07-23).
            const float gSw = mSwingGlobal != nullptr
                ? mSwingGlobal->load (std::memory_order_relaxed) : 0.f;
            auto swMix = [&] (std::atomic<float>* mix) -> double
            {
                if (gSw <= 0.f) return 0.0;
                const float m = mix != nullptr ? mix->load (std::memory_order_relaxed) : 1.f;
                return (double) gSw * (double) m * 0.125;
            };
            auto swTrunc = [] (std::atomic<float>* t) -> bool
            {
                return t != nullptr && t->load (std::memory_order_relaxed) >= 0.5f;
            };

            // ── SONG MODE ─────────────────────────────────────────────────
            if (rollSnap != nullptr && mSongMode.load(std::memory_order_relaxed))
            {

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

                // Schedule notes from every Pattern arrangement block that
                // overlaps this block's window(s).  Each block is a VIEWPORT
                // onto its pattern (Issue 2): notes play once at their absolute
                // song beat, masked by [blkStartBeat, blkEndBeat); note-offs
                // clamp to the clip end (and to loopEnd in a song-loop).  The
                // loop-seam window split handles a song-loop wrap sample-exactly.
                for (int blkIdx = 0; blkIdx < mPatternManager->getNumBlocks(); ++blkIdx)
                {
                    const auto& blk = mPatternManager->getBlock(blkIdx);
                    if (blk.clipType != ClipType::Pattern || blk.muted) continue;
                    if (!mPatternManager->isRowAudible(blk.trackRow)) continue;
                    if (blk.patternIndex < 0 || blk.patternIndex >= (int) rollSnap->patterns.size()) continue;

                    // QA-Ee: play the block's EXACT (sub-bar) span, not the
                    // ceil'd whole-bar count -- a sub-bar-resized pattern block
                    // now plays the length it is drawn (length of block ==
                    // length of playback).
                    double blkStartBeat = effectiveStartBeats (blk);
                    double blkEndBeat   = effectiveStartBeats (blk) + effectiveLengthBeats (blk);
                    // Relevant iff the block overlaps either active window.
                    bool relevant = false;
                    for (int w = 0; w < nWin; ++w)
                        if (blkStartBeat < windows[w].winEnd && blkEndBeat > windows[w].winStart)
                        { relevant = true; break; }
                    if (!relevant) continue;

                    const double offHi = (loopEndBeat > 0.0) ? juce::jmin (blkEndBeat, loopEndBeat) : blkEndBeat;
                    const auto& sPat = *rollSnap->patterns[(size_t) blk.patternIndex];

                    // #24 (QA-G3Smoke): tiling is GONE -- one pass from the
                    // content offset.  The pattern's content length (computed at
                    // snapshot publish; furthest note/step end, bar-ceiled at the
                    // pattern's bpb, min 1 bar) masks the schedule window:
                    // block length past available content is silent, and an
                    // offset past the content is silent.  contentLo still masks
                    // notes starting left of a slice cut (B-3 straddle clamp).
                    const double contentBeats = sPat.contentBeats;
                    const double offsetBeats  = juce::jmax (0.0, ticksToBeats (blk.contentOffsetTicks));
                    const double origin       = blkStartBeat - offsetBeats;
                    const double contentEnd   = juce::jmin (blkEndBeat, origin + contentBeats);
                    if (contentEnd <= blkStartBeat) continue;

                    auto sched = [&] (const std::vector<PianoNote>& notes, juce::MidiBuffer& buf, int target,
                                      double swingAmt = 0.0, bool swingTrunc = false)
                    {
                        scheduleRollWindows (notes, buf, target, origin, contentEnd,
                                             offHi, blkStartBeat, swingAmt, swingTrunc);
                    };

                    for (int pi = 0; pi < kMaxLayerPages; ++pi)
                        sched (sPat.layerNotes[pi], layerPageMidi[pi], pi,
                               swMix (mSwingMixLayer[pi]), swTrunc (mSwingTruncLayer[pi]));
                    for (int bi2 = 0; bi2 < kMaxBassPages; ++bi2)
                        sched (sPat.bassNotes[bi2], bassPageMidi[bi2], kBassPRTarget + bi2,
                               swMix (mSwingMixBass[bi2]), swTrunc (mSwingTruncBass[bi2]));

                    // #30b: per-page ENGINE existence is irrelevant to roll
                    // scheduling -- an absent page's MIDI buffer is stable and
                    // simply never consumed (the layers/bass loops above are the
                    // long-standing precedent).  Family-level activity atomics
                    // remain the fast-path gate; the per-page unique_ptr checks
                    // (which the deleted try-locks existed to make safe) are gone.
                    // D1.2 note: no drum transpose compensation - preset
                    // transpose IS the sound design.
                    if (mAnyDrumPageActive.load(std::memory_order_acquire))
                        for (int di = 0; di < kMaxDrumPages; ++di)
                            sched (sPat.drumNotes[di], drumPageMidi[di], kDrumPRTarget + di,
                                   swMix (mSwingMixDrum[di]), swTrunc (mSwingTruncDrum[di]));
                    // G-3: per-clip-page rolls.  Swing = full global (no per-page
                    // mix param -- Jeff 2026-07-23).
                    if (mAnyClipPageActive.load(std::memory_order_acquire))
                        for (int ci = 0; ci < kMaxClipPages; ++ci)
                            sched (sPat.clipNotes[ci], clipPageMidi[ci], kClipPRTarget + ci,
                                   swMix (nullptr), false);
                    // G-4: per-Vox-page rolls.  Swing-excluded (no vox MIDI --
                    // Jeff 2026-07-23).
                    if (mAnyVoxPageActive.load(std::memory_order_acquire))
                        for (int vi = 0; vi < kMaxVoxPages; ++vi)
                            sched (sPat.voxNotes[vi], voxPageMidi[vi], kVoxPRTarget + vi);
                    if (mAnyInstPageActive.load(std::memory_order_acquire))
                        for (int ii = 0; ii < kMaxInstPages; ++ii)
                        {
                            // K-3 / L-2: only sfizz-source Inst pages (Guitars /
                            // Basses) take MIDI; live-input chains drop it.
                            if (! mGuitarsActive[ii].load(std::memory_order_acquire)
                                && ! mBassesActive[ii].load(std::memory_order_acquire)) continue;
                            sched (sPat.instNotes[ii], instPageMidi[ii], kInstPRTarget + ii,
                                   swMix (mSwingMixInst[ii]), swTrunc (mSwingTruncInst[ii]));
                        }
                    // QA-ModelShell TS6: hosted VST3 instrument tabs.  Gate is
                    // the engine pointer itself -- a plugin tab with no loaded
                    // plugin has nothing to send MIDI to.
                    for (int pi = 0; pi < kMaxPluginPages; ++pi)
                        if (mPluginEngines[(size_t) pi] != nullptr)
                            sched (sPat.pluginNotes[pi], pluginPageMidi[pi], kPluginsPRTarget + pi,
                                   swMix (mSwingMixPlugin[pi]), swTrunc (mSwingTruncPlugin[pi]));
                    // J-7b: BaySickRustyDrums singleton roll.
                    if (mRustyDrumsActive.load(std::memory_order_acquire))
                        sched (sPat.rustyNotes, mRustyDrumsMidi, kRustyPRTarget,
                               swMix (mSwingMixRusty), swTrunc (mSwingTruncRusty));
                }
            }
            else if (rollSnap != nullptr && ! rollSnap->patterns.empty())
            {
                // ── PATTERN MODE ──────────────────────────────────────────
                // The current pattern loops [0, patLen) (or a time-selection
                // [loopStart, loopEnd)).  Notes are loop-local (absOffset = 0);
                // the loop-seam window split fires the wrapped first note at the
                // exact in-block sample, and note-offs clamp to loopEnd so an
                // overrun is cut at the boundary.  No band-aid, no double-fire.
                // #30b: the viewed pattern comes from the snapshot's stamped
                // index (published by setCurrentPattern), not the live object.
                const int curIdx = juce::jlimit (0, (int) rollSnap->patterns.size() - 1,
                                                 rollSnap->currentPatternIndex);
                const auto& pat = *rollSnap->patterns[(size_t) curIdx];
                const double offHi = (loopEndBeat > 0.0) ? loopEndBeat : 1.0e18;
                constexpr double kInf = 1.0e18;   // pattern has no viewport mask

                for (int i = 0; i < kMaxLayerPages; ++i)
                    scheduleRollWindows (pat.layerNotes[i], layerPageMidi[i], i, 0.0, kInf, offHi, -1.0e18,
                                         swMix (mSwingMixLayer[i]), swTrunc (mSwingTruncLayer[i]));
                for (int bi = 0; bi < kMaxBassPages; ++bi)
                    scheduleRollWindows (pat.bassNotes[bi], bassPageMidi[bi], kBassPRTarget + bi, 0.0, kInf, offHi, -1.0e18,
                                         swMix (mSwingMixBass[bi]), swTrunc (mSwingTruncBass[bi]));

                // #30b: try-locks + per-page engine checks deleted (see the
                // song-mode note); family atomics stay as the fast-path gate.
                if (mAnyDrumPageActive.load(std::memory_order_acquire))
                    for (int di = 0; di < kMaxDrumPages; ++di)
                        scheduleRollWindows (pat.drumNotes[di], drumPageMidi[di], kDrumPRTarget + di, 0.0, kInf, offHi, -1.0e18,
                                             swMix (mSwingMixDrum[di]), swTrunc (mSwingTruncDrum[di]));
                // G-3: per-clip-page rolls -- swing = full global (Jeff 2026-07-23).
                if (mAnyClipPageActive.load(std::memory_order_acquire))
                    for (int ci = 0; ci < kMaxClipPages; ++ci)
                        scheduleRollWindows (pat.clipNotes[ci], clipPageMidi[ci], kClipPRTarget + ci, 0.0, kInf, offHi, -1.0e18,
                                             swMix (nullptr), false);
                // G-4: per-Vox-page rolls -- swing-excluded (no vox MIDI).
                if (mAnyVoxPageActive.load(std::memory_order_acquire))
                    for (int vi = 0; vi < kMaxVoxPages; ++vi)
                        scheduleRollWindows (pat.voxNotes[vi], voxPageMidi[vi], kVoxPRTarget + vi, 0.0, kInf, offHi);
                if (mAnyInstPageActive.load(std::memory_order_acquire))
                    for (int ii = 0; ii < kMaxInstPages; ++ii)
                    {
                        // K-3 / L-2: only sfizz-source Inst pages take MIDI.
                        if (! mGuitarsActive[ii].load(std::memory_order_acquire)
                            && ! mBassesActive[ii].load(std::memory_order_acquire)) continue;
                        scheduleRollWindows (pat.instNotes[ii], instPageMidi[ii], kInstPRTarget + ii, 0.0, kInf, offHi, -1.0e18,
                                             swMix (mSwingMixInst[ii]), swTrunc (mSwingTruncInst[ii]));
                    }
                // QA-ModelShell TS6: hosted VST3 instrument tabs.
                for (int pi = 0; pi < kMaxPluginPages; ++pi)
                    if (mPluginEngines[(size_t) pi] != nullptr)
                        scheduleRollWindows (pat.pluginNotes[pi], pluginPageMidi[pi], kPluginsPRTarget + pi,
                                             0.0, kInf, offHi, -1.0e18,
                                             swMix (mSwingMixPlugin[pi]), swTrunc (mSwingTruncPlugin[pi]));
                // J-7b: BaySickRustyDrums singleton roll.
                if (mRustyDrumsActive.load(std::memory_order_acquire))
                    scheduleRollWindows (pat.rustyNotes, mRustyDrumsMidi, kRustyPRTarget, 0.0, kInf, offHi, -1.0e18,
                                         swMix (mSwingMixRusty), swTrunc (mSwingTruncRusty));
            }
        }
    }

    // ── Drum basic-sequence step triggering ───────────────────────────────
    if (!pos.getIsPlaying())
    {
        mLastDrumStep = -1;
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
            // Per-drum-tab notes flow through the D1.2 dispatch above.
        }
    }

    // ── Automation clip playback ──────────────────────────────────────────
    // Smoke round 3 (Jeff): SONG MODE ONLY -- automation clips live on the
    // Builder grid, so their bar-overlap math is only meaningful against the
    // song transport.  Pattern mode's looping beat used to be misread as a
    // grid position, so clips parked in the first bars drove their params
    // during pattern playback.  Gate matches the audio-clip block below.
    if (mSongMode.load (std::memory_order_relaxed) && pos.getIsPlaying() && mPatternManager)
    {
        // QA-UndoCoverage: automation replay is a programmatic writer -- its
        // values must never enter the undo history (write-time marking; the
        // scope is a thread_local bool flip, allocation-free on this thread).
        juce::AudioProcessorValueTreeState::ScopedProgrammaticParamWrites spw;

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

            // QA-Ee: honor the clip's exact (sub-bar) span for the automation
            // window + relPos, matching the drawn length (effective* fall back
            // to the bar fields for bar-aligned clips, so bar-aligned is a no-op).
            double clipStart = effectiveStartBars (blk);
            double clipEnd   = clipStart + effectiveLengthBars (blk);
            if (autoBar < clipStart || autoBar >= clipEnd) continue;

            float relPos = (float)((autoBar - clipStart) / effectiveLengthBars (blk));
            relPos = juce::jlimit(0.f, 1.f, relPos);

            // Shared evaluator (PatternManager.h) -- the offline render replay
            // and the editor both call it, so live and exported automation
            // cannot drift.  It reads the whole lane, so LFO mode and the
            // per-point tension / Spline shapes render here too.
            const float value01 = evalAutomationLaneAt (blk.automationLane, relPos);

            // Apply to APVTS parameter (setValue is audio-thread-safe)
            if (auto* param = apvts.getParameter(blk.automationLane.paramId))
                param->setValue(juce::jlimit(0.f, 1.f, value01));
        }
    }


    // 5F-4a Batch 6: compute anySolo across all inserts once per block
    const bool anySolo = mVibeGraph.isAnyInsertSoloed();

    // 5F-4b B1b: refresh routing graph
    mVibeGraph.rebuildRoutingFromApvts();
    // C.4 Phase 1 (2026-04-30): clear SC receive buffers each block before
    // sources fan their post-everything taps in.
    mVibeGraph.clearScRecvBuffers();

    // Batch 9a (2026-05-06): rebuild render-graph predecessor / child links
    // from the freshly-rebuilt RoutingGraph each block, so the dispatcher sees
    // fresh predecessor / child / mInitialDeps state on the next block after
    // any routing change.
    mRenderDispatcher.rebuildLinks (mVibeGraph.getRoutingGraph());

    // 2026-05-06 (Batch 9c B1): the per-site try-lock pattern this comment
    // used to describe is gone.  Audio thread now reads the AudioClipSnapshot
    // captured at the top of processBlock (mCurrentBlockClipSnapshot) and
    // every iteration site -- the FilePlay pre-scan, applyChokeGroupDispatch,
    // renderAudioClipsForRow under CompositeAudioInsertTask, VoxStripTask,
    // InstStripTask -- reads from that single snapshot.
    // Mutator (rebuildAudioClipPlayers) atomic-exchanges a new snapshot in
    // and retires the old to mClipRetirement; the slow ~AudioClipStreamer
    // destruction runs on the GC drainer thread, never on audio.

    // C.3 (2026-04-30): drain hardware MIDI input collector and route into
    // the engine page-buffer named by the Piano Roll's currently-focused
    // engine (set via setLiveMidiTarget on focus change).  Runs before
    // choke-group dispatch so live note-ons participate in the same choke
    // semantics as piano-roll scheduled notes.
    //
    // Routes to every MIDI-instrument kind, into the SAME page buffer the
    // pattern/song scheduler above feeds (so the target engine already
    // consumes it): Layer / Bass / Drum + the sfizz instruments Guitars /
    // Basses (shared instPageMidi[idx]) / Rusty Drums (singleton
    // mRustyDrumsMidi).  Only true live-input pages (Vox + live-input Inst),
    // Clip, and the DrumKit grid drop -- those take audio, not MIDI notes.
    {
        juce::MidiBuffer liveMidi;
        mLiveMidiCollector.removeNextBlockOfMessages (liveMidi, numSamples);
        if (! liveMidi.isEmpty())
        {
            // Resolve the live-MIDI target engine first -- both the routing
            // dest and the per-engine live-keyboard octave offset key off it.
            const int kind = mLiveMidiTargetKind .load (std::memory_order_relaxed);
            const int idx  = mLiveMidiTargetIndex.load (std::memory_order_relaxed);
            juce::MidiBuffer* dest = nullptr;
            // Encoding matches PianoRollPage::EngineKind ordering:
            // 1 Layer / 2 Bass / 3 Drum / 4 Clip / 7 Guitars / 8 Basses /
            // 9 Rusty Drums / 10 hosted plugin (BLU-447).
            if      (kind == 1 && idx >= 0 && idx < kMaxLayerPages) dest = &layerPageMidi[idx];
            else if (kind == 2 && idx >= 0 && idx < kMaxBassPages)  dest = &bassPageMidi [idx];
            else if (kind == 3 && idx >= 0 && idx < kMaxDrumPages)  dest = &drumPageMidi [idx];
            else if (kind == 4 && idx >= 0 && idx < kMaxClipPages)  dest = &clipPageMidi [idx];
            else if ((kind == 7 || kind == 8) && idx >= 0 && idx < kMaxInstPages) dest = &instPageMidi[idx];
            else if (kind == 9)                                    dest = &mRustyDrumsMidi;
            // QA-ModelShell TS6 (BLU-447): 10 = hosted VST3 instrument tab.
            else if (kind == 10 && idx >= 0 && idx < kMaxPluginPages) dest = &pluginPageMidi[idx];

            // Per-engine live-keyboard octave offset.  The sfizz BaySickBasses
            // kit (kind 8) is sampled an octave below where a 61-key controller
            // sits, so its keyswitches + playable range only fall under the
            // hardware keys after a -12 shift (whole octave -> note class C->C
            // preserved).  Applied to the WHOLE live path below (recorder +
            // monitor + engine) so played/recorded/displayed pitch all agree.
            // Live keyboard ONLY -- roll notes + stored patterns are untouched
            // (the pattern scheduler feeds instPageMidi natively elsewhere).
            const int liveTranspose = (kind == 8) ? -12 : 0;

            // QA-Ea Task 0b (2026-05-18): merge liveMidi into allMidi so the MIDI
            // recorder (allMidi's only consumer) captures the hardware
            // performance -- allMidi is built from host midiMessages otherwise
            // and never saw the keyboard.  Engines are driven by `dest`.  Forks #25.
            for (const auto m : liveMidi)
            {
                auto msg = m.getMessage();
                if (liveTranspose != 0 && msg.isNoteOnOrOff())
                    msg.setNoteNumber (juce::jlimit (0, 127, msg.getNoteNumber() + liveTranspose));

                allMidi.addEvent (msg, m.samplePosition);
                // Live-note monitor (audio -> UI): held hardware notes light the
                // on-screen keyboard.  Uses the post-offset note so the lit key
                // matches the sounding pitch.
                if      (msg.isNoteOn())  updateLiveHeldNote (msg.getNoteNumber(), true);
                else if (msg.isNoteOff()) updateLiveHeldNote (msg.getNoteNumber(), false);
                else if (msg.isAllNotesOff() || msg.isAllSoundOff()) clearLiveHeldNotes();

                // ── TRANSPORT-SYNC FILTER (Jeff, 2026-07-31) ──────────────────
                // A controller that can act as a tempo source streams MIDI clock
                // continuously whenever it is powered on -- 24 ticks per beat,
                // measured at 48/sec on Jeff's rig -- plus active sensing and
                // start/stop/continue.  All of it was being forwarded verbatim to
                // whichever engine is the live target.
                //
                // OUR engines ignore it, which is why this went unnoticed for the
                // whole life of the live path.  A HOSTED PLUGIN does not: any
                // arpeggiator, step sequencer or tempo-synced delay/LFO inside it
                // will lock to the CONTROLLER's tempo instead of the project's,
                // and when the two disagree the plugin drifts against the song
                // with nothing on screen to explain it.  We are the host; our
                // transport is the only tempo authority a hosted plugin should
                // see, and it already gets that through the playhead.
                //
                // Also filtered BEFORE the trigger-learn capture below, so a user
                // binding a drum pad while a controller streams clock cannot
                // accidentally bind to a clock tick.
                //
                // allMidi (the recorder) is deliberately untouched -- it captures
                // the raw hardware performance and is a separate surface.
                if (msg.isMidiClock() || msg.isMidiStart() || msg.isMidiStop()
                    || msg.isMidiContinue() || msg.isSongPositionPointer()
                    || msg.isActiveSense() || msg.isQuarterFrame())
                    continue;

                // QA-L-Fix (2026-07-19): per-drum kit triggers.  Runs HERE, in
                // the live-MIDI loop, rather than in the block-rate learn-queue
                // drain below -- this path preserves m.samplePosition, and drum
                // hits are the most timing-sensitive events in the app.
                //
                // Learn capture first: a captured event is suppressed from both
                // trigger dispatch AND engine routing, so the pad the user just
                // bound doesn't also sound the focused engine on that hit.
                if (mDrumTriggers.tryCaptureLearnRT (msg))
                    continue;

                dispatchDrumTriggers (msg, m.samplePosition, kind, drumPageMidi);

                if (dest != nullptr)
                    dest->addEvent (msg, m.samplePosition);
            }
            // Non-routed targets (DrumKit grid / Vox / live-input Inst / unset)
            // leave dest null -- dropped for ENGINE routing only; allMidi
            // already captured the performance so the recorder still sees it.
            // Clip (4) and hosted plugins (10) ARE routed above.
        }
    }

    // QA-L-Fix: expire CC-trigger holds that never received a CC-0 release.
    // Runs every block regardless of whether live MIDI arrived, so a controller
    // that goes silent mid-hold still releases.
    tickCcTriggerHolds (numSamples, drumPageMidi);

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

    // ── QA-E (2026-05-12): FilePlay pre-scan -- MUST run BEFORE dispatch.
    // Sets mVoxFilePlayActive / mInstFilePlayActive, read by VoxStripTask /
    // InstStripTask to gate their FilePlay branch.  Previously located after
    // the (since-removed) MT early return, so the flags weren't set when the
    // tasks ran -> FilePlay clips never decoded and arrangement playback
    // through Vox/Inst pages was silent.  Originally I-16 G-9 (2026-05-03);
    // moved here QA-E (2026-05-12).
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
            if (p.routeChannel == 0 || p.source == nullptr) continue;
            const int64 cs = clipBeatToSample (p.clipStartBeat, secPerBeatPS, mSampleRate);
            const int64 ce = clipBeatToSample (p.clipEndBeat,   secPerBeatPS, mSampleRate);
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
    // All inputs the dispatcher needs are now in scope: numSamples + pos +
    // anySolo + per-engine MidiBuffers + mLiveInputSnapshot + routing graph
    // (rebuilt above).  Build BlockContext once and hand off to the dispatcher.
    //
    // QA-Ef (2026-05-21): the dispatcher is the single, UNCONDITIONAL render
    // path.  The serial render tail that used to follow (skipped via an early
    // return when this flag was true) was DELETED here -- serial execution for
    // diagnosis is now the worker-park mode inside BaySickThreadPool
    // (gMultiThreadedEngineEnabled == false), not a code branch.  The bare
    // scope block below just bounds the dispatch-body locals.
    {
        BlockContext mtCtx;
        mtCtx.numSamples         = numSamples;
        mtCtx.sampleRate         = getSampleRate();
        mtCtx.bpm                = pos.getBpm().orFallback (120.0);
        mtCtx.anySolo            = anySolo;
        mtCtx.songMode           = mSongMode.load (std::memory_order_relaxed);
        // 2026-05-06 (Batch 9b): cache project pan law for bus tasks.
        mtCtx.panLaw             =
            (apvts.getRawParameterValue("master_pan_law") != nullptr)
                ? (int) apvts.getRawParameterValue("master_pan_law")->load()
                : 0;
        mtCtx.posInfo            = &pos;

        // §6.8: pattern-mode freeze reads a per-pattern file at a LOOP-LOCAL
        // position.  Mirrors the scheduler's own loop-bound math (see the
        // loopStartSamp / loopEndSamp block above) so the frozen audio lines up
        // with the notes that produced it, including under a tempo map.
        if (! mtCtx.songMode && mPatternManager != nullptr)
        {
            const double loopStartBeat = mLoopStartBeats.load (std::memory_order_relaxed);
            const double loopEndBeat   = mCachedPatternLoopBeats.load (std::memory_order_relaxed);

            if (loopEndBeat > loopStartBeat)
            {
                const juce::int64 samplePos = pos.getTimeInSamples().orFallback ((juce::int64) 0);
                const double      beatNow   = pos.getPpqPosition().orFallback (0.0);
                const double      spb       = 60.0 * mSampleRate
                                            / juce::jmax (1e-6, pos.getBpm().orFallback (120.0));

                const bool tm = TempoMap::isActive();
                const juce::int64 startSamp = tm ? TempoMap::sampleAtBeat (loopStartBeat)
                    : samplePos + (juce::int64) std::llround ((loopStartBeat - beatNow) * spb);
                const juce::int64 endSamp   = tm ? TempoMap::sampleAtBeat (loopEndBeat)
                    : samplePos + (juce::int64) std::llround ((loopEndBeat   - beatNow) * spb);
                const juce::int64 span      = endSamp - startSamp;

                if (span > 0)
                {
                    juce::int64 local = (samplePos - startSamp) % span;
                    if (local < 0) local += span;   // pre-roll / negative seek
                    mtCtx.patternIndex        = mPatternManager->getCurrentPatternIndex();
                    mtCtx.patternLocalSamples = local;
                }
            }
        }
        mtCtx.layerPageMidi      = layerPageMidi.data();
        mtCtx.bassPageMidi       = bassPageMidi .data();
        mtCtx.drumPageMidi       = drumPageMidi .data();
        mtCtx.clipPageMidi       = clipPageMidi .data();
        mtCtx.voxPageMidi        = voxPageMidi  .data();
        mtCtx.instPageMidi       = instPageMidi .data();
        mtCtx.pluginPageMidi     = pluginPageMidi.data();
        mtCtx.rustyDrumsMidi     = &mRustyDrumsMidi;
        mtCtx.liveInputSnapshot  = &mLiveInputSnapshot;

        // TS7 (2026-07-31): publish the block's transport for RACK effects.
        // Built from the same `pos` the BlockContext above carries, so a hosted
        // VST3 in a rack slot sees exactly the transport its strip is rendering
        // at.  Set BEFORE dispatchBlock -- the nodes read it during the render,
        // and the pool's release/acquire on task submit is what publishes it to
        // the workers.
        {
            DSPBase::HostTransport tp;
            tp.bpm           = mtCtx.bpm;
            tp.ppqPosition   = pos.getPpqPosition().orFallback (0.0);
            tp.timeInSamples = pos.getTimeInSamples().orFallback ((juce::int64) 0);
            tp.isPlaying     = pos.getIsPlaying();
            if (auto ts = pos.getTimeSignature())
            {
                tp.timeSigNum = ts->numerator;
                tp.timeSigDen = ts->denominator;
            }
            BaySickGraph::setBlockTransport (tp);
        }

        mRenderDispatcher.dispatchBlock (buffer, mtCtx);

        // Feed the master + MIDI recorders and run metronome/count-in.  buffer
        // here is the final master, pre-metronome (the metro is added inside),
        // so the recording stays click-free -- the D-5 ordering (MIDI rec ->
        // master rec -> metro).  Shared helper (QA-Ea Task 0b, Forks #25).
        applyPostMixRecordAndMetro (buffer, allMidi, pos, numSamples);

        // Drain UI meter atomics, otherwise dBFS / VU / per-effect meters all
        // sit at -inf (the audio path's peak writes never reach the UI
        // mirrors).  The MasterTask + per-strip tasks have already populated
        // the node-level atomics by this point; we just promote them.
        drainMeterAtomicsForUI();

        // Drive the DSP-load meter: deadline-proximity wall-clock across
        // dispatchBlock, the same math under MT and ST (the Multi-core toggle
        // cannot change the reading), and purely a UI signal -- nothing sheds
        // load.  NOT DURING AN OFFLINE RENDER: wall-clock against the block's
        // realtime duration is meaningless when the loop deliberately runs
        // faster than realtime, and would peg the meter + overload flash.
        if (! isNonRealtime())
            measureDspLoadAndOverload (t0, numSamples);
        return;
    }

}

// 2026-05-18 (QA-Ea Task 0b): post-mix recorders + metronome/count-in.  Shared
// helper that feeds the master + MIDI recorders and runs the metronome/count-in
// after dispatch.  Originally serial-tail-only, which left a 104-byte empty
// master WAV + no MIDI/metro capture once MT became the render path (Forks
// #25); extracting it fixed that.  D-5 invariant: MIDI rec -> master rec
// (pre-metronome buffer) -> metronome/count-in.  bpm derives from the passed
// playhead position.
void BaySickDAWProcessor::applyPostMixRecordAndMetro (juce::AudioBuffer<float>& buffer,
                                                     const juce::MidiBuffer& allMidi,
                                                     const juce::AudioPlayHead::PositionInfo& pos,
                                                     int numSamples)
{
    // Nothing in here may run under an offline render.  Export, measure and
    // freeze all drive this same processBlock, and auto-freeze can fire during a
    // count-in while the user is recording, so a render's blocks would otherwise
    // be folded into the live take.  The two audio taps below keep their own
    // isNonRealtime() term; this is the funnel that also covers the MIDI
    // recorder's clock and the pre-roll counter, which had none.  The pre-roll
    // gap was the damaging one: the WAV writer IS gated, so the file never grew
    // while the counter did, and commitRecordingResult subtracts the pre-roll
    // from the file length -- an inflated counter drives the take's effective
    // content negative, dropping the whole performance from the arrangement and
    // the Audio Library with no message.
    if (isNonRealtime())
        return;

    const double bpm = pos.getBpm().orFallback (120.0);

    // ── MIDI recording: capture note events sent to the graph this block ─
    if (mMidiRecorder.isRecording())
    {
        // QA-Ee: feed the recorder this block's length, NOT pos.getPpqPosition().
        // The recorder runs its own count-in-inclusive clock; the transport PPQ
        // freezes during the count-in (post-QA-Ed advanceBlock gates on mPlaying,
        // false until the count-in timer fires), which dropped the count-in bar
        // out of recorded note positions (notes a measure early + sheared length).
        double bps = bpm / (60.0 * mSampleRate);
        // QA-TempoMap: while the transport runs, feed the exact per-block
        // AVERAGE beats-per-sample from the map so the recorder's accumulated
        // clock cannot drift across a tempo-boundary block (a persistent
        // whole-take error, unlike one-off placement).  The count-in keeps
        // the linear clock - the transport is frozen then and the map delta
        // would be zero.
        if (pos.getIsPlaying() && TempoMap::isActive())
        {
            const int64_t p0 = pos.getTimeInSamples().orFallback((int64_t) 0);
            bps = juce::jmax(1e-9, (TempoMap::beatAtSample(p0 + numSamples)
                                    - TempoMap::beatAtSample(p0)) / (double) numSamples);
        }
        mMidiRecorder.processBlock(allMidi, numSamples, bps);
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
    // BOTH writers are gated on isNonRealtime() (2026-07-30).  An offline render
    // -- export, measure or freeze -- drives this same processBlock, so without
    // the gate the render's audio was written INTO the user's in-progress
    // recording.  That is data loss, not a glitch: auto-freeze can fire while
    // recording, and the take is silently corrupted with material from a
    // different part of the song.  The metronome block below was already gated
    // for exactly this reason; the recorders were missed.
    if (! isNonRealtime()
        && mMasterTapLive.load (std::memory_order_acquire)
        && mMasterRecorder.isRecording())
        mMasterRecorder.writeBlock (buffer);

    // TS7 §3.5: version capture's audio half, written at the SAME pre-metronome
    // point as the record path so a captured take never carries a click track.
    // A SECOND AudioFileRecorder rather than mMasterRecorder itself: capture and
    // a user recording can be running at once, and sharing one writer would make
    // whichever started second silently steal the file from the first.
    if (! isNonRealtime()
        && mCaptureTapLive.load (std::memory_order_acquire)
        && mCaptureRecorder.isRecording())
        mCaptureRecorder.writeBlock (buffer);

    // ── Metronome click DSP ───────────────────────────────────────────────────
    // Count-in always runs (independent of metro button); transport metro requires enabled.
    // QA-ModelShell TS2: the click is injected post-master-tap on the LIVE
    // graph, so under the offline drive it would print into the export --
    // gated out entirely (locked export-semantics call; matches the
    // record-master convention of keeping the click out of captured audio).
    if (! isNonRealtime())
    {
        const float  metroVol  = mMetro.volume.load(std::memory_order_relaxed);
        const int    sndType   = mMetro.soundType.load(std::memory_order_relaxed);
        const float  twoPi     = juce::MathConstants<float>::twoPi;
        // QA-Fe2 PDC: this buffer's mix content is totalLatencySamples late
        // vs the transport clock (bus + master compensation), so every click
        // defers by the same amount to land ON the music instead of leading
        // it by the full PDC.
        const int    pdcSamples = juce::jmax (0,
            mVibeGraph.totalLatencySamples.load (std::memory_order_relaxed));

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
            mMetro.countInBeatsFired = 0;
            // QA-Fe2 PDC: defer the WHOLE count-in by the current PDC so the
            // interval from its last click into the first transport click
            // stays exactly one beat (the transport clicks defer below).
            mMetro.countInDelaySamp  = pdcSamples;
        }
        mMetro.countInWasActive = ciActive;

        if (ciActive)
        {
            const double bpm = mMetro.countInBpm.load(std::memory_order_relaxed);
            const double bps = juce::jmax(1e-6, bpm / (60.0 * mSampleRate));
            // QA-G Task 6: count-in clicks run in DENOMINATOR units (7/8
            // counts seven 8ths per bar), accent on the bar start.  The
            // signature is captured at record start (song = marker map at
            // the record position; pattern = the pattern's effective TS).
            const int    ciNum   = juce::jmax (1, mMetro.countInNum.load(std::memory_order_relaxed));
            const int    ciDen   = juce::jmax (1, mMetro.countInDen.load(std::memory_order_relaxed));
            const double clickIv = 4.0 / (double) ciDen;   // quarter-beats per click
            for (int s = 0; s < numSamples; ++s)
            {
                if (mMetro.countInDelaySamp > 0)
                {
                    --mMetro.countInDelaySamp;
                }
                else
                {
                    // D-5 semantics preserved: beat 1 fires immediately once
                    // the PDC deferral elapses (was: on the rising edge).
                    if (mMetro.countInBeatsFired == 0)
                    {
                        mMetro.countInBeatsFired = 1;
                        triggerClick(true);
                    }
                    double prevPhase = mMetro.countInPhase;
                    mMetro.countInPhase += bps;
                    if ((long long)(mMetro.countInPhase / clickIv) > (long long)(prevPhase / clickIv))
                    {
                        // Crossing a click boundary; countInBeatsFired tracks
                        // how many clicks have fired so far.
                        ++mMetro.countInBeatsFired;
                        triggerClick((mMetro.countInBeatsFired - 1) % ciNum == 0);
                    }
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
        bool transportMetroRan = false;
        if (!ciActive && mMetro.enabled.load(std::memory_order_relaxed))
        {
            if (auto pi = getPlayHead() ? getPlayHead()->getPosition()
                                        : juce::Optional<juce::AudioPlayHead::PositionInfo>{})
            {
                if (pi->getIsPlaying())
                {
                    transportMetroRan = true;
                    const double bpmV = pi->getBpm().orFallback(120.0);
                    const double bps  = juce::jmax(1e-6, bpmV / (60.0 * mSampleRate));
                    const double bs0  = pi->getPpqPosition().orFallback(0.0);

                    // QA-G Task 6: clicks run in DENOMINATOR units with the
                    // accent on bar starts.  SONG mode reads the marker
                    // timeline (TsMap -- the sole played source, docket #14);
                    // PATTERN mode uses the current pattern's effective TS
                    // (its roll grid; beats are pattern-local so bar starts
                    // sit at multiples of its bar length from 0).  4/4 with
                    // no markers degenerates to the pre-Task-6 behavior.
                    const bool songTs = mSongMode.load(std::memory_order_relaxed)
                                        && TsMap::isActive();
                    int patNum = 4, patDen = 4;
                    if (mPatternManager)
                    {
                        patNum = juce::jmax (1, mPatternManager->currentPattern().tsNum);
                        patDen = juce::jmax (1, mPatternManager->currentPattern().tsDen);
                    }

                    // QA-TempoMap: this metronome is the marker ear-check
                    // instrument, so tick placement must stay sample-exact
                    // through a tempo step - split the block at the boundary
                    // (one per block is the realistic case) into constant-
                    // tempo spans instead of one linear sweep.  QA-G Task 6
                    // additionally splits each tempo span at time-signature
                    // boundaries so the click unit / accent base flip at the
                    // exact marker beat.  Fallback = the pre-map single span.
                    // QA-Fe2 PDC: the grid derives from the DELAYED clock
                    // (transport minus pdcSamples).  TempoMap extrapolates
                    // segment 0 linearly below sample 0, so the sub-zero
                    // region right after a song-top start stays exact; clicks
                    // for negative beats are suppressed in the loop.
                    struct MetroSpan { int s0; int s1; double beat0; double bps;
                                       double tsBase; double clickIv; int num; };
                    MetroSpan spans[4];
                    int nSpans = 0;
                    if (TempoMap::isActive())
                    {
                        const int64_t smp0 = pi->getTimeInSamples().orFallback((int64_t) 0)
                                             - (int64_t) pdcSamples;
                        const int64_t bnd  = TempoMap::nextBoundaryAfter(smp0);
                        const int cut = (bnd > smp0 && bnd < smp0 + numSamples)
                                          ? (int)(bnd - smp0) : numSamples;
                        spans[nSpans++] = { 0, cut, TempoMap::beatAtSample(smp0),
                                            juce::jmax(1e-6, TempoMap::bpmAtSample(smp0) / (60.0 * mSampleRate)),
                                            0.0, 1.0, 4 };
                        if (cut < numSamples)
                            spans[nSpans++] = { cut, numSamples, TempoMap::beatAtSample(bnd),
                                                juce::jmax(1e-6, TempoMap::bpmAtSample(bnd) / (60.0 * mSampleRate)),
                                                0.0, 1.0, 4 };
                    }
                    else
                    {
                        spans[nSpans++] = { 0, numSamples, bs0 - pdcSamples * bps, bps,
                                            0.0, 1.0, 4 };
                    }

                    // Signature assignment + TS-boundary split (song mode).
                    if (songTs)
                    {
                        const int nTempo = nSpans;
                        MetroSpan out[4];
                        int nOut = 0;
                        for (int t = 0; t < nTempo && nOut < 4; ++t)
                        {
                            MetroSpan cur = spans[t];
                            const double spanEndBeat = cur.beat0
                                + (double)(cur.s1 - cur.s0) * cur.bps;
                            const double tsBnd = TsMap::nextBoundaryAfterBeat (cur.beat0);
                            if (tsBnd > cur.beat0 && tsBnd < spanEndBeat && nOut < 3)
                            {
                                const int sCut = juce::jlimit (cur.s0 + 1, cur.s1,
                                    cur.s0 + (int) std::ceil ((tsBnd - cur.beat0)
                                                              / juce::jmax (1e-12, cur.bps)));
                                MetroSpan a = cur;  a.s1 = sCut;
                                MetroSpan b = cur;  b.s0 = sCut;
                                b.beat0 = cur.beat0 + (double)(sCut - cur.s0) * cur.bps;
                                const auto bbA = TsMap::barBeatAt (a.beat0);
                                a.tsBase = bbA.barStartBeat;
                                a.clickIv = 4.0 / (double) juce::jmax (1, bbA.den);
                                a.num = juce::jmax (1, bbA.num);
                                const auto bbB = TsMap::barBeatAt (b.beat0);
                                b.tsBase = bbB.barStartBeat;
                                b.clickIv = 4.0 / (double) juce::jmax (1, bbB.den);
                                b.num = juce::jmax (1, bbB.num);
                                out[nOut++] = a;
                                out[nOut++] = b;
                            }
                            else
                            {
                                const auto bb = TsMap::barBeatAt (cur.beat0);
                                cur.tsBase = bb.barStartBeat;
                                cur.clickIv = 4.0 / (double) juce::jmax (1, bb.den);
                                cur.num = juce::jmax (1, bb.num);
                                out[nOut++] = cur;
                            }
                        }
                        for (int i = 0; i < nOut; ++i) spans[i] = out[i];
                        nSpans = nOut;
                    }
                    else
                    {
                        for (int i = 0; i < nSpans; ++i)
                        {
                            spans[i].tsBase  = 0.0;
                            spans[i].clickIv = 4.0 / (double) patDen;
                            spans[i].num     = patNum;
                        }
                    }

                    // QA-Fe2 PDC: play-start edge seeds the grid one below the
                    // NEXT crossing, so a start exactly ON a beat (pdc 0,
                    // count-in handoff) still clicks at sample 0 while the
                    // deferral can't fire a stale catch-up click mid-beat --
                    // the first click otherwise lands on the next real
                    // crossing.  lastBeatFloor holds the last CLICK-UNIT floor
                    // (quarter-beats in 4/4 -- unchanged there).
                    if (!mMetro.transportWasPlaying)
                        mMetro.lastBeatFloor =
                            std::ceil((spans[0].beat0 - spans[0].tsBase) / spans[0].clickIv) - 1.0;

                    double prevTsBase = spans[0].tsBase;
                    double prevIv     = spans[0].clickIv;
                    for (int sp = 0; sp < nSpans; ++sp)
                    {
                        // Entering a new signature segment: the click-unit
                        // basis is discontinuous -- reseed one below the next
                        // crossing (the boundary itself is a bar start, so
                        // its click fires with the accent).
                        if (sp > 0 && (spans[sp].tsBase != prevTsBase
                                       || spans[sp].clickIv != prevIv))
                            mMetro.lastBeatFloor =
                                std::ceil((spans[sp].beat0 - spans[sp].tsBase) / spans[sp].clickIv) - 1.0;
                        prevTsBase = spans[sp].tsBase;
                        prevIv     = spans[sp].clickIv;

                        for (int s = spans[sp].s0; s < spans[sp].s1; ++s)
                        {
                            double sampleBeat = spans[sp].beat0 + (s - spans[sp].s0) * spans[sp].bps;
                            double clickPos   = (sampleBeat - spans[sp].tsBase) / spans[sp].clickIv;
                            double posFloor   = std::floor(clickPos);
                            if (posFloor < mMetro.lastBeatFloor - 1.0)
                                mMetro.lastBeatFloor = posFloor - 1.0;
                            if (posFloor > mMetro.lastBeatFloor)
                            {
                                mMetro.lastBeatFloor = posFloor;
                                long long ci = (long long)std::round(posFloor);
                                if (sampleBeat >= 0.0)   // sub-zero = the first pdcSamples after a song-top start
                                    triggerClick ((((ci % spans[sp].num) + spans[sp].num) % spans[sp].num) == 0);
                            }
                            float s0 = synthClick();
                            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                                buffer.addSample(ch, s, s0);
                        }
                    }
                }
            }
        }
        mMetro.transportWasPlaying = transportMetroRan;
    }
}

// QA-AudioMeters (2026-05-24): UI-facing per-insert peak drain.  Exchange-resets
// the appropriate m<Kind>InsertPeakDb*L/R mirror to -inf and returns the running
// max-since-last-call.  MixerTrackStrip / Mixer page poll this once per vblank.
// Wait-free; safe from any thread but typically called from the UI thread.
// QA-RustyMeter (2026-05-30): RMS sibling of drainInsertPeakDbStereo for the
// split meter's scrolling top half.  No PluginProcessor mirror -- the RMS is a
// current value read straight off the insert node + exchange-reset there.
std::pair<float, float>
BaySickDAWProcessor::drainInsertRmsDbStereo (BaySickGraph::InsertKind kind, int index) noexcept
{
    return mVibeGraph.drainInsertNodeRms (kind, index);
}

// QA-RustyMeter part 2 (2026-05-30): bus RMS sibling for the split meter.  Thin
// passthrough to mVibeGraph.drainBusRms -- like the insert RMS, the bus RMS is a
// current value read straight off BaySickGraph's per-bus atoms + exchange-reset
// there, so there is no PluginProcessor mirror (unlike the bus peak path).
std::pair<float, float>
BaySickDAWProcessor::drainBusRmsDbStereo (int busChId) noexcept
{
    return mVibeGraph.drainBusRms (busChId);
}

// QA-RustyMeter Task 3 (2026-05-30): master LUFS readout passthrough.  mode
// 0=Momentary / 1=Short-Term / 2=Integrated.  Read once per UI vblank.
float BaySickDAWProcessor::getMasterLufs (int mode) const noexcept
{
    return mVibeGraph.getMasterLufs (mode);
}

// CL-044 (QA-ModelShell TS7): master-out spectrum passthrough for the analyzer
// window.  Both halves are gated inside BaySickGraph, so an inactive tap is one
// relaxed load per block on the audio side and a false return here.
void BaySickDAWProcessor::setMasterSpectrumActive (bool on) noexcept
{
    mVibeGraph.setMasterSpectrumActive (on);
}

bool BaySickDAWProcessor::pollMasterSpectrum (float* dest, int& outCount) noexcept
{
    return mVibeGraph.pollMasterSpectrum (dest, outCount);
}

float BaySickDAWProcessor::getMasterTruePeakDb() const noexcept
{
    return mVibeGraph.getMasterTruePeakDb();
}

void BaySickDAWProcessor::setMasterAnalysisActive (bool on) noexcept
{
    mVibeGraph.setMasterAnalysisActive (on);
}

float BaySickDAWProcessor::getMasterTruePeakMaxDb() const noexcept
{
    return mVibeGraph.getMasterTruePeakMaxDb();
}

void BaySickDAWProcessor::resetMasterTruePeakMax() noexcept
{
    mVibeGraph.resetMasterTruePeakMax();
}

// THREAD SAFETY (both of these, message thread only): AudioFileRecorder::
// stopRecording destroys the ThreadedWriter straight after clearing its own
// unfenced flag, so a block already past the tap's isRecording() test would
// write into freed memory.  Close the gate, settle the in-flight block, THEN
// stop the writer -- the same order the record path uses for mMasterTapLive.
bool BaySickDAWProcessor::startMasterCapture (const juce::File& target)
{
    if (mCaptureRecorder.isRecording())
    {
        mCaptureTapLive.store (false, std::memory_order_release);
        settleAudioThread();
        mCaptureRecorder.stopRecording();
    }
    target.getParentDirectory().createDirectory();
    // Gate raised BEFORE the writer opens so no block at the head of the capture
    // falls between the two stores: the tap still short-circuits on
    // isRecording(), which AudioFileRecorder sets last.
    mCaptureTapLive.store (true, std::memory_order_release);
    // Same latency trim as the record path: the master tap is post-PDC, so the
    // capture would otherwise start totalLatencySamples late against the grid.
    if (! mCaptureRecorder.startRecording (target, mSampleRate, 2,
            juce::jmax (0, mVibeGraph.totalLatencySamples.load (std::memory_order_relaxed))))
    {
        mCaptureTapLive.store (false, std::memory_order_release);
        return false;
    }
    return true;
}

juce::File BaySickDAWProcessor::stopMasterCapture()
{
    if (! mCaptureRecorder.isRecording()) return {};
    mCaptureTapLive.store (false, std::memory_order_release);
    settleAudioThread();
    return mCaptureRecorder.stopRecording();
}

// ── TS7 §6: the freeze driver ────────────────────────────────────────────────
// Only the ENGINE-DRIVEN kinds can freeze: Layers / Bass / Drums / Plugins are
// the ones whose audio comes from an EngineInsertTask this can switch.  Clips
// already ARE audio files, and Vox / Inst are live-input chains where freezing
// the input would freeze nothing the user could unfreeze.
// Returns RenderTask*, not EngineInsertTask* (2026-07-30).  Vox and Inst strips
// are plain RenderTasks, so the old return type could not express them and
// freeze was structurally shut out of both -- see RenderTask::setFrozenSource.
RenderTask* BaySickDAWProcessor::renderTaskForTab (TabKind kind, int pageIndex) noexcept
{
    auto at = [] (auto& arr, int i) -> RenderTask*
    {
        return (i >= 0 && i < (int) arr.size()) ? arr[(size_t) i].get() : nullptr;
    };
    switch (kind)
    {
        case TabKind::Layers:  return at (mLayerRenderTasks,  pageIndex);
        case TabKind::Bass:    return at (mBassRenderTasks,   pageIndex);
        case TabKind::Drums:   return at (mDrumRenderTasks,   pageIndex);
        case TabKind::Plugins: return at (mPluginRenderTasks, pageIndex);
        // Jeff, 2026-07-30: every tab.  These freeze their GRID PLAYBACK -- a
        // recorded take on one of these strips replays through that strip's own
        // chain, which is exactly what a freeze should be standing in for.
        case TabKind::Vox:     return at (mVoxRenderTasks,    pageIndex);
        case TabKind::Inst:    return at (mInstRenderTasks,   pageIndex);
        // Clips freezes too (Jeff, 2026-07-30).  My "it is already a file, so
        // freezing renders a file to copy a file" was wrong: the page's
        // BaySickPlayer engine runs behind that file every block, plus the
        // strip's rack -- which is exactly the CPU freeze exists to reclaim.
        // The composite task owns BOTH the clip decode and the engine trigger,
        // so substituting at it replaces the whole cost.
        case TabKind::Clips:   return at (mAudioRenderTasks,  pageIndex);
    }
    return nullptr;
}

// EXHAUSTIVE on purpose -- no `default`.  The previous version defaulted every
// unhandled kind to InsertKind::Layer, which is not a harmless fallback: it would
// have armed the freeze tap on Layer page N while rendering Inst/Clips/Vox tab N,
// producing a freeze file of the WRONG track with no error anywhere.  A missing
// case is now a compiler warning instead of a silent mis-tap.
static BaySickGraph::InsertKind insertKindForTab (TabKind k) noexcept
{
    switch (k)
    {
        case TabKind::Layers:  return BaySickGraph::InsertKind::Layer;
        case TabKind::Bass:    return BaySickGraph::InsertKind::Bass;
        case TabKind::Drums:   return BaySickGraph::InsertKind::Drum;
        case TabKind::Plugins: return BaySickGraph::InsertKind::Plugin;
        case TabKind::Clips:   return BaySickGraph::InsertKind::Audio;
        case TabKind::Vox:     return BaySickGraph::InsertKind::Vox;
        case TabKind::Inst:    return BaySickGraph::InsertKind::Inst;
        // The kit has THIRTEEN inserts, not one -- this returns the kind, and the
        // Rusty freeze path supplies the strip index per file rather than using
        // the single-index callers below.
        case TabKind::Rusty:   return BaySickGraph::InsertKind::Rusty;
    }
    return BaySickGraph::InsertKind::Layer;   // unreachable; silences C4715
}

// §6.7.  The kind is spelled as a NAME, never the raw TabKind ordinal: the enum
// is append-only today by accident rather than by rule (Plugins was appended for
// exactly this reason), and inserting a value mid-enum would silently re-point
// every freeze file a saved project refers to.  A name cannot drift that way.
juce::File BaySickDAWProcessor::freezeFileFor (TabKind kind, int pageIndex, int patternIndex)
{
    const juce::File dir = getProjectFreezeDir();
    if (dir == juce::File()) return {};

    // freezeFilePrefixFor is the ONE builder of the name-and-index prefix (it
    // carried its own copy of the kind-name map until 2026-07-31, which is the
    // drift its own comment warned about).  For the kit, pageIndex is the
    // STRIP index -- one file per drum, all written by a single freeze action.
    const juce::String prefix = freezeFilePrefixFor (kind, pageIndex);
    if (prefix.isEmpty()) return {};

    // §6.8: the SCOPE is in the name.  A freeze file is either the arrangement
    // (`_song`) or one pattern's own render (`_patN`) -- the two are different
    // audio of different lengths and cannot share a filename.  Without this a
    // pattern render would silently overwrite the song render and vice versa.
    // No migration for the old unsuffixed name: pre-v1, and a stale file is
    // swept as an orphan rather than loaded.
    const juce::String scope = patternIndex < 0
                             ? juce::String ("song")
                             : ("pat" + juce::String (patternIndex));

    return dir.getChildFile (prefix + scope + ".wav");
}

// §6.8: which patterns this tab actually PLAYS IN.  A frozen instrument renders
// the song plus one file per pattern it has content in -- not every pattern, and
// not the other instruments in those patterns, which is what keeps the per-
// pattern cache cheap (a few bars each rather than an arrangement).
std::vector<int> BaySickDAWProcessor::patternsWithContentFor (TabKind kind, int pageIndex) const
{
    std::vector<int> out;
    if (mPatternManager == nullptr) return out;

    const int nPat = mPatternManager->getNumPatterns();

    for (int p = 0; p < nPat; ++p)
    {
        const auto& pat = mPatternManager->getPattern (p);
        const std::vector<PianoNote>* notes = nullptr;

        auto pick = [&] (const auto& arr, int idx) -> const std::vector<PianoNote>*
        {
            return (idx >= 0 && idx < (int) arr.size()) ? &arr[(size_t) idx].notes : nullptr;
        };

        switch (kind)
        {
            case TabKind::Layers:  notes = pick (pat.layerRoll,  pageIndex); break;
            case TabKind::Bass:    notes = pick (pat.bassRoll,   pageIndex); break;
            case TabKind::Drums:   notes = pick (pat.drumRolls,  pageIndex); break;
            case TabKind::Clips:   notes = pick (pat.clipRoll,   pageIndex); break;
            case TabKind::Vox:     notes = pick (pat.voxRoll,    pageIndex); break;
            case TabKind::Inst:    notes = pick (pat.instRoll,   pageIndex); break;
            case TabKind::Plugins: notes = pick (pat.pluginRoll, pageIndex); break;
            // The kit is ONE instrument across 13 strips: its roll is a single
            // shared one, so every strip's per-pattern set is the kit's.
            case TabKind::Rusty:   notes = &pat.baySickRustyDrumsRoll.notes;  break;
        }

        if (notes != nullptr && ! notes->empty())
            out.push_back (p);
    }
    return out;
}

// §6.7's two cleanup rules, both of which were missing entirely.
void BaySickDAWProcessor::deleteFreezeFileFor (TabKind kind, int pageIndex)
{
    // EVERY SCOPE, not just the song file: a per-instrument freeze can leave one
    // file per pattern behind it, and deleting only the song render would strand
    // all of them as orphans the moment the tab goes.
    const juce::File dir = getProjectFreezeDir();
    if (dir == juce::File() || ! dir.isDirectory()) return;

    const juce::String prefix = freezeFilePrefixFor (kind, pageIndex);
    if (prefix.isEmpty()) return;

    juce::Array<juce::File> files;
    dir.findChildFiles (files, juce::File::findFiles, false, prefix + "*.wav");
    for (const auto& f : files) f.deleteFile();
}

// The name up to (and including) the scope separator -- `tab_layers_0_`.  One
// place builds it so the delete sweep, the orphan sweep and the per-scope file
// naming cannot drift apart.
juce::String BaySickDAWProcessor::freezeFilePrefixFor (TabKind kind, int pageIndex) const
{
    const char* n = nullptr;
    switch (kind)
    {
        case TabKind::Layers:  n = "layers";  break;
        case TabKind::Bass:    n = "bass";    break;
        case TabKind::Drums:   n = "drums";   break;
        case TabKind::Clips:   n = "clips";   break;
        case TabKind::Vox:     n = "vox";     break;
        case TabKind::Inst:    n = "inst";    break;
        case TabKind::Plugins: n = "plugins"; break;
        case TabKind::Rusty:   n = "rusty";   break;
    }
    if (n == nullptr) return {};
    return juce::String ("tab_") + n + "_" + juce::String (pageIndex) + "_";
}

// Called after a project's tabs are restored.  Without this the Freeze folder
// only ever grows: a tab deleted in a previous session leaves a song-length WAV
// behind forever, and per-instrument freeze multiplies how many of those there
// can be.
void BaySickDAWProcessor::sweepOrphanFreezeFiles()
{
    const juce::File dir = getProjectFreezeDir();
    if (dir == juce::File() || ! dir.isDirectory()) return;

    juce::Array<juce::File> files;
    dir.findChildFiles (files, juce::File::findFiles, false, "tab_*.wav");

    // Build the live set from the rig rather than parsing names back into kinds:
    // the rig is the authority on which tabs exist, and a name we cannot parse
    // should be LEFT ALONE rather than deleted on a guess.
    // PREFIXES, not whole filenames: a live tab now owns a whole FAMILY of files
    // (`_song` plus one `_patN` per pattern it appears in), and matching exact
    // names would have deleted every per-pattern render as an orphan the first
    // time this ran.  Rusty included -- its 13 strips are indices under the
    // `rusty` name, and omitting it swept the entire frozen kit.
    // The pattern set travels WITH the prefix: ownership is per SCOPE, not per
    // tab.  Deleting a pattern re-indexes the arrangement but leaves that
    // pattern's renders on disk, so a prefix-only match keeps `pat7.wav` forever
    // in a project that now has three patterns -- and Save As / Duplicate copies
    // the whole Freeze folder, so the dead weight compounds per copy.
    struct LiveFreezeScope
    {
        juce::String     prefix;
        std::vector<int> patterns;
    };
    std::vector<LiveFreezeScope> livePrefixes;
    static constexpr TabKind kAll[] = { TabKind::Layers, TabKind::Bass,
                                        TabKind::Drums,  TabKind::Clips,
                                        TabKind::Vox,    TabKind::Inst,
                                        TabKind::Plugins, TabKind::Rusty };
    for (auto k : kAll)
        for (int i = 0; i < 64; ++i)
            if (mEngineRig->findTab (k, i) != nullptr)
            {
                if (k == TabKind::Rusty)
                {
                    // One tab, thirteen strip indices -- and ONE shared roll, so
                    // every strip's pattern set is the kit's (index 0).
                    const auto pats = patternsWithContentFor (k, 0);
                    for (int s = 0; s < MixerChannelIds::kMaxRustyStrips; ++s)
                        livePrefixes.push_back ({ freezeFilePrefixFor (k, s), pats });
                }
                else
                {
                    livePrefixes.push_back ({ freezeFilePrefixFor (k, i),
                                              patternsWithContentFor (k, i) });
                }
            }

    for (const auto& f : files)
    {
        const juce::String name = f.getFileName();
        bool owned = false;

        for (const auto& p : livePrefixes)
        {
            if (! name.startsWith (p.prefix)) continue;

            // A tail we cannot parse is KEPT, the same conservative rule the
            // whole-name match already applies to a name we cannot attribute.
            owned = true;

            const juce::String tail = name.substring (p.prefix.length()).dropLastCharacters (4);
            if (tail == "song") break;

            if (tail.startsWith ("pat"))
            {
                const juce::String digits = tail.substring (3);
                if (digits.isNotEmpty() && digits.containsOnly ("0123456789"))
                {
                    const int patIdx = digits.getIntValue();
                    owned = false;
                    for (int live : p.patterns)
                        if (live == patIdx) { owned = true; break; }
                }
            }
            break;
        }

        if (! owned) f.deleteFile();
    }
}

// ── TS7 §6.9: the kit freezes as ONE unit (Jeff's option (c)) ────────────────
// Thirteen strips, ONE user action, ONE render pass.  Rendering them separately
// would mean thirteen offline renders -- thirteen times the setup cost and
// thirteen times the silence -- for audio that is all produced by the same
// single sfizz instance in the same pass.
//
// Captured at each strip's own handoff point, so every strip keeps its rack, EQ,
// fader, pan, mute/solo and meter LIVE.  Only the drum sounds bake.  Capturing
// at the kit BUS instead would have baked all thirteen strips' mixer settings --
// see the plan's §6.9 entry for why that shape was abandoned.
bool BaySickDAWProcessor::setRustyFrozenPatternSourcesImpl (
    const std::vector<std::unique_ptr<AudioClipStreamer>>* streams, int patternIndex)
{
    for (size_t i = 0; i < mRustyRenderTasks.size(); ++i)
    {
        auto* t = mRustyRenderTasks[i].get();
        if (t == nullptr) continue;

        AudioClipStreamer* s = (streams != nullptr && i < streams->size())
                             ? (*streams)[i].get() : nullptr;
        t->setFrozenPatternSource (s, patternIndex);
    }
    // The producer's pattern-mode skip mirrors what was just published: only
    // when a real stream set is up may it stop synthesizing.
    mRustyFrozenPatternIndex.store (streams != nullptr ? patternIndex : -1,
                                    std::memory_order_release);
    return true;
}

void BaySickDAWProcessor::setRustyFrozenPatternSources (
    const std::vector<std::unique_ptr<AudioClipStreamer>>* streams, int patternIndex)
{
    setRustyFrozenPatternSourcesImpl (streams, patternIndex);
}

bool BaySickDAWProcessor::freezeRustyKit (juce::String& outErr, bool byUser, bool reuseValid,
                                         bool songScopeOnly)
{
    auto* tab = mEngineRig->findTab (TabKind::Rusty, 0);
    if (tab == nullptr)              { outErr = "The drum kit is not available."; return false; }
    if (mRustyDrumsEngine == nullptr){ outErr = "No drum kit is loaded.";         return false; }
    if (! onRenderKitFreezeFiles)    { outErr = "The freeze renderer is not available."; return false; }

    const juce::File dir = getProjectFreezeDir();
    if (dir == juce::File())
    {
        outErr = "Save the project before freezing - the freeze files live beside it.";
        return false;
    }

    const int strips = mRustyDrumsEngine->getStripCount();
    if (strips <= 0) { outErr = "The drum kit has no pieces loaded."; return false; }

    std::vector<juce::File> dests;
    dests.reserve ((size_t) strips);
    for (int i = 0; i < strips; ++i)
        dests.push_back (freezeFileFor (TabKind::Rusty, i));

    // ONE pass, thirteen writers.  Renders with the kit still LIVE -- the capture
    // reads each strip as the engine hands it over, so the engine must be
    // producing while this runs.
    // The kit render reads the strips straight off the engine rather than through
    // the freeze tap, so the ONE task that has to keep running is the producer
    // that calls the engine.  Everything the producer itself depends on comes
    // along through the dispatcher's upstream walk.
    const juce::uint32 songStamp = mEngineRig->freezeContentStamp (TabKind::Rusty, 0, -1);
    const bool songReusable = reuseValid
                           && tab->freezeSpan.stampMatches (songStamp)
                           && std::all_of (dests.begin(), dests.end(),
                                           [] (const juce::File& f) { return f.existsAsFile(); });

    if (! songReusable
        && ! onRenderKitFreezeFiles (dests, mRustyProducerTask.get(), -1, outErr))
        return false;

    // Belt-and-braces mirroring freezeTab's: on a RE-freeze the thirteen strips
    // still point into the streams the clear below destroys.  refreshFreeze
    // already nulls + settles before calling here; this keeps the invariant for
    // every other route.
    if (tab->frozen)
    {
        retractFrozenSources (TabKind::Rusty, 0);
        settleAudioThread();
    }

    tab->freezeStreams.clear();
    tab->freezeStreams.reserve ((size_t) strips);
    for (int i = 0; i < strips; ++i)
    {
        std::unique_ptr<juce::AudioFormatReader> raw (
            mAudioFormatManager.createReaderFor (dests[(size_t) i]));
        if (raw == nullptr)
        {
            // Partial failure is worse than none: some strips frozen and some
            // live would play the kit against itself.  Unwind completely.
            tab->freezeStreams.clear();
            outErr = "Could not open a rendered kit file.";
            return false;
        }
        auto s = std::make_unique<AudioClipStreamer> (std::move (raw), mAudioFileThread);
        s->seek (0);
        tab->freezeStreams.push_back (std::move (s));
    }

    // §6.8: PER-PATTERN RENDERS FOR THE KIT TOO.  Built last, because the kit
    // renders through its own thirteen-writer path rather than freezeTab -- and
    // that is exactly how it came to be the one tab kind of eight left with
    // song-scope freeze only.  Each pattern is one more thirteen-file pass.
    //
    // A per-pattern failure is NOT fatal, same as the single-track path: the song
    // render already succeeded, so the kit stays frozen in song mode and plays
    // live in the pattern that failed.
    std::map<int, std::vector<std::unique_ptr<AudioClipStreamer>>> patStreams;
    std::map<int, juce::uint32>                                    patStamps;

    // Ruling 2-b: an AUTOMATIC kit freeze lands the song scope only; the
    // staggered filler completes pattern coverage one render per quiet tick.
    const std::vector<int> kitPats = songScopeOnly
        ? std::vector<int>{} : patternsWithContentFor (TabKind::Rusty, 0);

    for (int p : kitPats)
    {
        std::vector<juce::File> pdests;
        pdests.reserve ((size_t) strips);
        for (int i = 0; i < strips; ++i)
            pdests.push_back (freezeFileFor (TabKind::Rusty, i, p));

        const juce::uint32 stamp = mEngineRig->freezeContentStamp (TabKind::Rusty, 0, p);
        auto prev = tab->patternStamps.find (p);
        const bool reusable = reuseValid
                           && prev != tab->patternStamps.end()
                           && prev->second != 0 && prev->second == stamp
                           && std::all_of (pdests.begin(), pdests.end(),
                                           [] (const juce::File& f) { return f.existsAsFile(); });

        juce::String perr;
        if (! reusable
            && ! onRenderKitFreezeFiles (pdests, mRustyProducerTask.get(), p, perr))
            continue;

        std::vector<std::unique_ptr<AudioClipStreamer>> v;
        v.reserve ((size_t) strips);
        bool ok = true;

        for (const auto& f : pdests)
        {
            auto raw = SafeAudioReader::guard (
                std::unique_ptr<juce::AudioFormatReader> (mAudioFormatManager.createReaderFor (f)));
            if (raw == nullptr) { ok = false; break; }
            auto s = std::make_unique<AudioClipStreamer> (std::move (raw), mAudioFileThread);
            s->seek (0);
            v.push_back (std::move (s));
        }

        if (ok && (int) v.size() == strips)
        {
            patStreams[p] = std::move (v);
            patStamps[p]  = stamp;
        }
    }

    // Null the readers before the map they point into is replaced -- the same
    // use-after-free the single-track path guards.
    setRustyFrozenPatternSourcesImpl (nullptr, -1);
    tab->freezePatternStreams = std::move (patStreams);
    tab->patternStamps        = std::move (patStamps);
    tab->stalePatterns.clear();

    tab->frozen       = true;
    tab->frozenByUser = byUser;
    tab->userUnfroze  = false;
    tab->freezeStale  = false;
    // The kit is the singleton (Rusty, page 0) -- freezeRustyKit has no kind /
    // pageIndex of its own because there is only ever one.
    tab->freezeSpan   = mEngineRig->songFreezeSpanFor (TabKind::Rusty, 0);

    // Publish to the strips FIRST, then let the producer stop: between those two
    // stores a strip may still read the engine, which is merely redundant.  The
    // reverse order would leave a strip reading an engine that had already
    // stopped producing -- a gap.
    for (int i = 0; i < strips && i < (int) mRustyRenderTasks.size(); ++i)
        if (mRustyRenderTasks[(size_t) i])
            mRustyRenderTasks[(size_t) i]->setFrozenSource (tab->freezeStreams[(size_t) i].get());
    mRustyKitFrozen.store (true, std::memory_order_release);

    // Point the thirteen strips at the pattern currently looping (see the same
    // call in freezeTab for why the editor's change-gated poll is not enough).
    if (mPatternManager != nullptr)
        mEngineRig->republishPatternSources (mPatternManager->getCurrentPatternIndex());

    if (mEngineRig->onFreezeStateChanged) mEngineRig->onFreezeStateChanged (TabKind::Rusty, 0);
    return true;
}

bool BaySickDAWProcessor::freezeTab (TabKind kind, int pageIndex,
                                    juce::String& outErr, bool byUser, bool reuseValid,
                                    bool songScopeOnly)
{
    // The kit is ONE action over THIRTEEN strips (Jeff's option (c)), so it has
    // its own path -- there is no single task or single file for it, which is
    // exactly why renderTaskForTab cannot express it.
    if (kind == TabKind::Rusty)
        return freezeRustyKit (outErr, byUser, reuseValid, songScopeOnly);

    auto* tab  = mEngineRig->findTab (kind, pageIndex);
    auto* task = renderTaskForTab (kind, pageIndex);
    if (tab == nullptr || task == nullptr)
    {
        outErr = "That tab cannot be frozen.";
        return false;
    }
    if (! onRenderFreezeFile)
    {
        outErr = "The freeze renderer is not available.";
        return false;
    }

    const juce::File dir = getProjectFreezeDir();
    if (dir == juce::File())
    {
        outErr = "Save the project before freezing - the freeze file lives beside it.";
        return false;
    }

    // §6.7: ONE file per track, overwritten in place.  The name is derived from
    // the tab's identity rather than its user-facing name, so renaming a tab
    // does not orphan its freeze.
    const juce::File dest = freezeFileFor (kind, pageIndex);

    // §6.8: the render set is the SONG plus one file per pattern this instrument
    // actually plays in.  Not every pattern (a tab silent in a pattern needs no
    // file) and not the other instruments in those patterns -- freeze is
    // per-instrument, so drums frozen and bass live means bass stays live in
    // every pattern.
    // The full render set: song + one file per pattern this instrument plays in.
    // Not every pattern (a tab silent in one needs no file) and not the other
    // instruments in those patterns -- freeze is per-instrument, so drums frozen
    // and bass live means bass stays live in every pattern.
    //
    // `reuseValid` lets each file be SKIPPED when its content stamp still
    // matches.  Project restore passes it: without the stamp, load had to
    // re-render every frozen tab from scratch, and once a tab meant 1+N renders
    // that multiplied load time by the pattern count.
    // Ruling 2-b: automatic freezes are song-scope; the staggered filler owns
    // pattern coverage so no uninvited render runs a whole multi-pattern set.
    const std::vector<int> pats = songScopeOnly
        ? std::vector<int>{} : patternsWithContentFor (kind, pageIndex);
    const int totalRenders = 1 + (int) pats.size();
    int       doneRenders  = 0;

    auto step = [&] (const juce::String& what)
    {
        if (onFreezeStep)
            onFreezeStep (doneRenders, totalRenders, what);
    };

    // REUSE WHEN THE STAMP STILL MATCHES.  The file on disk is only valid if
    // nothing that determines its sound has moved since -- engine state, notes,
    // arrangement, tempo.  That is exactly what the stamp hashes, so this is a
    // safe skip rather than the "assume it still fits" shortcut it replaces.
    // Restore is the caller that needs it: without a stamp it had to re-render
    // every frozen tab on every project load.
    const juce::uint32 songStamp = mEngineRig->freezeContentStamp (kind, pageIndex, -1);
    const bool songReusable = reuseValid
                           && dest.existsAsFile()
                           && tab->freezeSpan.stampMatches (songStamp);

    if (! songReusable)
    {
        step ("Freezing - arrangement");

        // Render with the tab still LIVE: the tap reads the engine's pre-rack
        // output, so it must be producing audio while this runs.
        if (! onRenderFreezeFile (insertKindForTab (kind), pageIndex, task, -1, dest, outErr))
            return false;
    }
    ++doneRenders;

    // Same construction the clip players use: a reader handed to the shared
    // background prefetch thread, then a synchronous seek(0) pre-fill so the tab
    // plays from the first block rather than under-running into the live engine.
    auto openStream = [this] (const juce::File& f) -> std::unique_ptr<AudioClipStreamer>
    {
        auto r = SafeAudioReader::guard (
            std::unique_ptr<juce::AudioFormatReader> (mAudioFormatManager.createReaderFor (f)));
        if (r == nullptr) return nullptr;
        auto s = std::make_unique<AudioClipStreamer> (std::move (r), mAudioFileThread);
        s->seek (0);
        return s;
    };

    auto stream = openStream (dest);
    if (stream == nullptr)
    {
        outErr = "The freeze file was rendered but could not be opened for playback.";
        return false;
    }

    // Per-pattern renders.  A FAILURE HERE IS NOT FATAL: the song render already
    // succeeded, so the tab can freeze in song mode and simply play live in the
    // pattern that failed -- which is freeze's existing fall-back-to-live rule.
    // Dropping the whole freeze because one pattern would not render would be a
    // worse trade than the CPU it saves.
    std::map<int, std::vector<std::unique_ptr<AudioClipStreamer>>> patStreams;

    std::map<int, juce::uint32> patStamps;

    for (int p : pats)
    {
        const juce::File   pf    = freezeFileFor (kind, pageIndex, p);
        const juce::uint32 stamp = mEngineRig->freezeContentStamp (kind, pageIndex, p);

        // Per-pattern reuse is INDEPENDENT: editing pattern 3 leaves patterns
        // 1, 2 and 4 valid, which is what makes per-pattern staleness exact
        // instead of "any edit invalidates everything".
        auto prev = tab->patternStamps.find (p);
        const bool reusable = reuseValid
                           && pf.existsAsFile()
                           && prev != tab->patternStamps.end()
                           && prev->second != 0
                           && prev->second == stamp;

        bool ok = reusable;

        if (! reusable)
        {
            step ("Freezing - pattern " + juce::String (doneRenders) + " of "
                  + juce::String (totalRenders - 1));

            juce::String perr;
            ok = onRenderFreezeFile (insertKindForTab (kind), pageIndex, task, p, pf, perr);
        }

        if (ok)
            if (auto ps = openStream (pf))
            {
                std::vector<std::unique_ptr<AudioClipStreamer>> v;
                v.push_back (std::move (ps));
                patStreams[p] = std::move (v);
                patStamps[p]  = stamp;
            }

        ++doneRenders;
    }

    // Publish LAST: the task only starts reading once the streamer is fully open,
    // and the rig holds the streamer so it outlives any block that could read it.
    // Belt-and-braces for the same hazard refreshFreeze guards above: whatever
    // route got here, the audio thread must not be holding a pointer into the
    // storage we are about to replace -- BOTH pointers, and a block's settle
    // when this tab was already frozen, or the invariant only holds for the
    // callers that happen to clear first today.
    task->setFrozenPatternSource (nullptr, -1);
    task->setFrozenSource (nullptr);
    if (tab->frozen)
        settleAudioThread();

    tab->freezeStreams.clear();
    tab->freezeStreams.push_back (std::move (stream));
    tab->freezePatternStreams = std::move (patStreams);
    tab->patternStamps        = std::move (patStamps);
    tab->stalePatterns.clear();
    tab->frozen       = true;
    tab->frozenByUser = byUser;
    tab->userUnfroze  = false;
    tab->freezeStale  = false;
    tab->freezeSpan   = mEngineRig->songFreezeSpanFor (kind, pageIndex);
    task->setFrozenSource (tab->freezeStreams.front().get());

    // §6.8: point the task at the render for the pattern that is CURRENTLY
    // looping.  The editor's poll only republishes when the current pattern
    // CHANGES, so without this a tab frozen while sitting on pattern 2 would
    // have pattern 2's file on disk and never play it -- the user would have to
    // switch away and back before their own freeze took effect in pattern mode.
    if (mPatternManager != nullptr)
        mEngineRig->republishPatternSources (mPatternManager->getCurrentPatternIndex());

    if (mEngineRig->onFreezeStateChanged) mEngineRig->onFreezeStateChanged (kind, pageIndex);
    return true;
}

void BaySickDAWProcessor::retractFrozenSources (TabKind kind, int pageIndex)
{
    if (kind == TabKind::Rusty)
    {
        for (auto& t : mRustyRenderTasks)
            if (t)
            {
                t->setFrozenSource (nullptr);
                t->setFrozenPatternSource (nullptr, -1);
            }
        // The producer resumes synthesis: a stale kit PLAYS LIVE (§6.6)
        // rather than serving the old render, and the strips' substitution is
        // already off because their pointers are null.  Both skip signals
        // cleared -- song flag AND the published pattern index.
        mRustyKitFrozen.store (false, std::memory_order_release);
        mRustyFrozenPatternIndex.store (-1, std::memory_order_release);
        return;
    }

    if (auto* task = renderTaskForTab (kind, pageIndex))
    {
        task->setFrozenSource (nullptr);
        task->setFrozenPatternSource (nullptr, -1);
    }
}

int BaySickDAWProcessor::freezePatternIndexNow() const noexcept
{
    return mPatternManager != nullptr ? mPatternManager->getCurrentPatternIndex() : -1;
}

void BaySickDAWProcessor::unfreezeTab (TabKind kind, int pageIndex)
{
    auto* tab = mEngineRig->findTab (kind, pageIndex);
    if (tab == nullptr || ! tab->frozen) return;

    // ORDER IS LOAD-BEARING: clear the audio thread's pointer FIRST, then release
    // the streamer.  Releasing first would leave a block in flight reading freed
    // memory.  The engine was never destroyed, so it simply resumes.
    if (kind == TabKind::Rusty)
    {
        retractFrozenSources (TabKind::Rusty, 0);
    }
    else if (auto* task = renderTaskForTab (kind, pageIndex))
    {
        task->setFrozenSource (nullptr);
        // §6.8: the PATTERN pointer too.  Clearing only the song source left an
        // unfrozen tab still playing its per-pattern render in pattern mode --
        // the freeze would look released and be audible anyway.
        task->setFrozenPatternSource (nullptr, -1);
    }

    // One block's settle between the null and the free: the null stops NEW
    // reads, but a block already past its load still holds the old pointer
    // until it finishes (the page-dtor contract's one-block settle).
    settleAudioThread();

    tab->freezeStreams.clear();
    tab->freezePatternStreams.clear();
    tab->patternStamps.clear();
    tab->stalePatterns.clear();
    tab->frozen      = false;
    tab->freezeStale = false;
    // Jeff's ruling: an explicit unfreeze takes this tab out of auto-freeze's
    // reach for the session.  The freeze button is the only route here, so
    // every call IS explicit.
    tab->userUnfroze = true;

    if (mEngineRig->onFreezeStateChanged) mEngineRig->onFreezeStateChanged (kind, pageIndex);
}

// ── Ruling 2-b (2026-07-31): staggered pattern coverage ──────────────────────
// Automatic freezes land song-scope only; these two fill in the per-pattern
// renders ONE at a time from the editor's quiet-tick poll, so an uninvited
// render never stalls the app for a whole multi-pattern set.
bool BaySickDAWProcessor::findPendingPatternFreeze (TabKind& outKind, int& outPage,
                                                   int& outPattern) const
{
    if (mEngineRig == nullptr) return false;

    static constexpr TabKind kAll[] = { TabKind::Layers, TabKind::Bass,
                                        TabKind::Drums,  TabKind::Clips,
                                        TabKind::Vox,    TabKind::Inst,
                                        TabKind::Plugins, TabKind::Rusty };
    for (auto k : kAll)
        for (int i = 0; i < EngineRig::capacityOf (k); ++i)
        {
            const auto* t = mEngineRig->findTab (k, i);
            // Stale tabs are the refresh queue's job -- filling their patterns
            // here would render against content already known to be wrong.
            if (t == nullptr || ! t->frozen || t->freezeStale) continue;

            for (int p : patternsWithContentFor (k, i))
                if (t->freezePatternStreams.count (p) == 0)
                {
                    outKind = k; outPage = i; outPattern = p;
                    return true;
                }
        }
    return false;
}

bool BaySickDAWProcessor::renderPatternFreeze (TabKind kind, int pageIndex,
                                              int patternIndex, juce::String& outErr)
{
    auto* tab = mEngineRig != nullptr ? mEngineRig->findTab (kind, pageIndex) : nullptr;
    if (tab == nullptr || ! tab->frozen || tab->freezeStale) return false;
    if (tab->freezePatternStreams.count (patternIndex) > 0)  return true;

    auto openStream = [this] (const juce::File& f) -> std::unique_ptr<AudioClipStreamer>
    {
        auto raw = SafeAudioReader::guard (
            std::unique_ptr<juce::AudioFormatReader> (mAudioFormatManager.createReaderFor (f)));
        if (raw == nullptr) return nullptr;
        auto s = std::make_unique<AudioClipStreamer> (std::move (raw), mAudioFileThread);
        s->seek (0);
        return s;
    };

    const juce::uint32 stamp = mEngineRig->freezeContentStamp (kind, pageIndex, patternIndex);
    const auto         prev  = tab->patternStamps.find (patternIndex);
    const bool stampMatches  = prev != tab->patternStamps.end()
                            && prev->second != 0 && prev->second == stamp;

    std::vector<std::unique_ptr<AudioClipStreamer>> v;

    if (kind == TabKind::Rusty)
    {
        if (mRustyDrumsEngine == nullptr || ! onRenderKitFreezeFiles) return false;

        const int strips = juce::jmin ((int) mRustyDrumsEngine->getChannels().size(),
                                       (int) MixerChannelIds::kMaxRustyStrips);
        if (strips <= 0) return false;

        std::vector<juce::File> pdests;
        pdests.reserve ((size_t) strips);
        for (int s = 0; s < strips; ++s)
            pdests.push_back (freezeFileFor (TabKind::Rusty, s, patternIndex));

        const bool reusable = stampMatches
                           && std::all_of (pdests.begin(), pdests.end(),
                                           [] (const juce::File& f) { return f.existsAsFile(); });
        if (! reusable
            && ! onRenderKitFreezeFiles (pdests, mRustyProducerTask.get(), patternIndex, outErr))
            return false;

        for (const auto& f : pdests)
        {
            auto s = openStream (f);
            if (s == nullptr) { outErr = "Could not open a rendered kit file."; return false; }
            v.push_back (std::move (s));
        }
    }
    else
    {
        auto* task = renderTaskForTab (kind, pageIndex);
        if (task == nullptr || ! onRenderFreezeFile) return false;

        const juce::File pf = freezeFileFor (kind, pageIndex, patternIndex);
        const bool reusable = stampMatches && pf.existsAsFile();

        if (! reusable
            && ! onRenderFreezeFile (insertKindForTab (kind), pageIndex, task,
                                     patternIndex, pf, outErr))
            return false;

        auto s = openStream (pf);
        if (s == nullptr) { outErr = "Could not open the rendered file."; return false; }
        v.push_back (std::move (s));
    }

    // Map INSERT, never a whole-map assign: entries the audio thread is reading
    // through published pointers must not move.  Publish follows the insert.
    tab->freezePatternStreams[patternIndex] = std::move (v);
    tab->patternStamps[patternIndex]        = stamp;
    tab->stalePatterns.erase (patternIndex);

    if (mPatternManager != nullptr)
        mEngineRig->republishPatternSources (mPatternManager->getCurrentPatternIndex());
    return true;
}

bool BaySickDAWProcessor::refreshFreeze (TabKind kind, int pageIndex, juce::String& outErr,
                                        bool songScopeOnly)
{
    auto* tab = mEngineRig->findTab (kind, pageIndex);
    if (tab == nullptr || ! tab->frozen) return false;

    const bool wasByUser = tab->frozenByUser;

    // Drop to the live engine for the duration: the render needs the engine
    // producing audio for the tap to capture, and playback falling back to live
    // meanwhile is exactly §6.6's rule -- so the re-render is never a silence.
    // USE-AFTER-FREE if the PATTERN pointer is not cleared here (found
    // 2026-07-31).  This path re-renders an ALREADY-FROZEN tab, so
    // freezePatternStreams is populated and the task is pointing into it.
    // freezeTab below move-assigns that map, destroying the streamer the audio
    // thread is still reading.  ORDER IS LOAD-BEARING, exactly as for the song
    // source: null the reader's pointer FIRST, release the storage after.
    if (kind == TabKind::Rusty)
    {
        retractFrozenSources (TabKind::Rusty, 0);
    }
    else if (auto* task = renderTaskForTab (kind, pageIndex))
    {
        task->setFrozenSource (nullptr);
        task->setFrozenPatternSource (nullptr, -1);
    }
    // Same one-block settle as unfreezeTab, same reason: a block that loaded
    // the pointer before the null is still reading the streamer these clears
    // destroy.
    settleAudioThread();
    tab->freezeStreams.clear();
    tab->freezePatternStreams.clear();

    // The kit renders through its own thirteen-strip path; routing it through
    // freezeTab failed the render and silently UNFROZE a stale kit.
    const bool refreshed = (kind == TabKind::Rusty)
        ? freezeRustyKit (outErr, wasByUser, /*reuseValid*/ false, songScopeOnly)
        : freezeTab (kind, pageIndex, outErr, wasByUser, /*reuseValid*/ false, songScopeOnly);
    if (! refreshed)
    {
        // Left unfrozen and playing live rather than pointing at a stale file.
        tab->frozen      = false;
        tab->freezeStale = false;
        if (mEngineRig->onFreezeStateChanged) mEngineRig->onFreezeStateChanged (kind, pageIndex);
        return false;
    }
    return true;
}

std::pair<float, float>
BaySickDAWProcessor::drainInsertPeakDbStereo (BaySickGraph::InsertKind kind, int index) noexcept
{
    constexpr float kNI = -std::numeric_limits<float>::infinity();
    auto drainPair = [] (std::atomic<float>& l, std::atomic<float>& r) noexcept
    {
        return std::pair<float, float> {
            l.exchange (kNI, std::memory_order_relaxed),
            r.exchange (kNI, std::memory_order_relaxed)
        };
    };
    switch (kind)
    {
        case BaySickGraph::InsertKind::Layer:
            if (index >= 0 && index < kMaxLayerPages)
                return drainPair (mLayerInsertPeakDbL[index], mLayerInsertPeakDbR[index]);
            break;
        case BaySickGraph::InsertKind::Bass:
            if (index >= 0 && index < kMaxBassPages)
                return drainPair (mBassInsertPeakDbL[index], mBassInsertPeakDbR[index]);
            break;
        case BaySickGraph::InsertKind::Drum:
            if (index >= 0 && index < kMaxDrumPages)
                return drainPair (mDrumInsertPeakDbL[index], mDrumInsertPeakDbR[index]);
            break;
        case BaySickGraph::InsertKind::Audio:
            if (index >= 0 && index < kMaxAudioRows)
                return drainPair (mAudioRowPeakDbL[index], mAudioRowPeakDbR[index]);
            break;
        case BaySickGraph::InsertKind::Aux:
            if (index >= 0 && index < MixerChannelIds::kMaxAuxStrips)
                return drainPair (mAuxInsertPeakDbL[index], mAuxInsertPeakDbR[index]);
            break;
        case BaySickGraph::InsertKind::Vox:
            if (index >= 0 && index < MixerChannelIds::kMaxVoxStrips)
                return drainPair (mVoxInsertPeakDbL[index], mVoxInsertPeakDbR[index]);
            break;
        case BaySickGraph::InsertKind::Inst:
            if (index >= 0 && index < MixerChannelIds::kMaxInstStrips)
                return drainPair (mInstInsertPeakDbL[index], mInstInsertPeakDbR[index]);
            break;
        case BaySickGraph::InsertKind::Rusty:
            if (index >= 0 && index < MixerChannelIds::kMaxRustyStrips)
                return drainPair (mRustyInsertPeakDbL[index], mRustyInsertPeakDbR[index]);
            break;
        case BaySickGraph::InsertKind::Plugin:
            if (index >= 0 && index < MixerChannelIds::kMaxPluginStrips)
                return drainPair (mPluginInsertPeakDbL[index], mPluginInsertPeakDbR[index]);
            break;
    }
    return { -60.f, -60.f };
}

// QA-AudioMeters (2026-05-24): UI-meter atomic drain.  Single boundary point
// where every UI-visible peak atomic gets updated, called once per processBlock
// after dispatchBlock.  Two parts (post-QA-AudioMeters):
//   1. Bus mirrors -- every bus (Layers / Bass / Drums / Master / FX /
//      AudioClips / Vox / Vox2 / Inst / Inst2 / Inst3 / Rusty) drains via
//      the unified G1 loop: drainAndMerge from BaySickGraph public-member atomics
//      that the corresponding BusNode exchange-stored during the block.
//   2. Insert mirrors -- every InsertKind (Layer / Bass / Drum / Audio / Aux /
//      Vox / Inst / Rusty) drains the same way: drainAndMerge from
//      mVibeGraph.<kind>InsertPeakDb*[index] -> m<Kind>InsertPeakDb*[index].
//      (Audio kind drains into the existing mAudioRowPeakDb* arrays, kept under
//      that name for Builder grid backward compat.)  Audio publishes via
//      InsertNode::process -> publishPeakReading; processInsert exchange-stores
//      InsertNode peakDb/L/R into the BaySickGraph per-kind array; this drain
//      moves it into the PluginProcessor mirror that UI polls.
// Pre-QA-AudioMeters there was also a Group 3 step -- per-insert peakDbSnap
// promotion via promoteAllInsertPeakSnapshots -- that's gone (peakDbSnap layer
// removed entirely).  promoteAllRackSlotSnapshots is still called for the
// effect-rack-slot peak meters in every InsertNode + every BusNode (separate
// surface; effect-panel DBFSMeter + VU input meters).
// All parts happen back-to-back so a UI vblank firing anywhere outside this
// small window catches a coherent snapshot across every meter.
void BaySickDAWProcessor::drainMeterAtomicsForUI()
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
    // Unified G1 bus drain -- every bus drained below
    // follows the same chain post-QA-Eg: BusNode peakDbL/R (audio-thread
    // publishPeakReading) -> BaySickGraph member atomic (processBus exchange-
    // store) -> mirror (this drainAndMerge).
    drainAndMerge (mLayersPeakDbL, mVibeGraph.layersPeakDbL);
    drainAndMerge (mLayersPeakDbR, mVibeGraph.layersPeakDbR);
    drainAndMerge (mBassPeakDbL,   mVibeGraph.bassPeakDbL);
    drainAndMerge (mBassPeakDbR,   mVibeGraph.bassPeakDbR);
    drainAndMerge (mDrumsPeakDbL,  mVibeGraph.drumsPeakDbL);
    drainAndMerge (mDrumsPeakDbR,  mVibeGraph.drumsPeakDbR);
    drainAndMerge (mMasterPeakDbL, mVibeGraph.masterPeakDbL);
    drainAndMerge (mMasterPeakDbR, mVibeGraph.masterPeakDbR);
    drainAndMerge (mFxBusPeakDbL,  mVibeGraph.fxBusPeakDbL);
    drainAndMerge (mFxBusPeakDbR,  mVibeGraph.fxBusPeakDbR);
    drainAndMerge (mAudioClipsBusPeakDbL, mVibeGraph.audioClipsPeakDbL);
    drainAndMerge (mAudioClipsBusPeakDbR, mVibeGraph.audioClipsPeakDbR);
    drainAndMerge (mVoxBusPeakDbL,   mVibeGraph.voxBusPeakDbL);
    drainAndMerge (mVoxBusPeakDbR,   mVibeGraph.voxBusPeakDbR);
    drainAndMerge (mVoxBus2PeakDbL,  mVibeGraph.voxBus2PeakDbL);
    drainAndMerge (mVoxBus2PeakDbR,  mVibeGraph.voxBus2PeakDbR);
    drainAndMerge (mInstBusPeakDbL,  mVibeGraph.instBusPeakDbL);
    drainAndMerge (mInstBusPeakDbR,  mVibeGraph.instBusPeakDbR);
    drainAndMerge (mInstBus2PeakDbL, mVibeGraph.instBus2PeakDbL);
    drainAndMerge (mInstBus2PeakDbR, mVibeGraph.instBus2PeakDbR);
    drainAndMerge (mInstBus3PeakDbL, mVibeGraph.instBus3PeakDbL);
    drainAndMerge (mInstBus3PeakDbR, mVibeGraph.instBus3PeakDbR);
    drainAndMerge (mRustyDrumsBusPeakDbL, mVibeGraph.rustyDrumsBusPeakDbL);
    drainAndMerge (mRustyDrumsBusPeakDbR, mVibeGraph.rustyDrumsBusPeakDbR);
    // TS6 (BLU-447) -- MISSED, fixed TS7 2026-07-31.  Both the graph-side and
    // processor-side atomics existed; nothing connected them, so the Plugins bus
    // strip's dBFS meter sat at its floor while the waveform meter (fed from a
    // different surface) worked -- which is exactly how Jeff spotted it.
    drainAndMerge (mPluginsBusPeakDbL, mVibeGraph.pluginsBusPeakDbL);
    drainAndMerge (mPluginsBusPeakDbR, mVibeGraph.pluginsBusPeakDbR);
    // QA-Layout T10: secondary group buses.
    drainAndMerge (mLayersBus2PeakDbL,  mVibeGraph.layersBus2PeakDbL);
    drainAndMerge (mLayersBus2PeakDbR,  mVibeGraph.layersBus2PeakDbR);
    drainAndMerge (mBassBus2PeakDbL,    mVibeGraph.bassBus2PeakDbL);
    drainAndMerge (mBassBus2PeakDbR,    mVibeGraph.bassBus2PeakDbR);
    drainAndMerge (mClipsBus2PeakDbL,   mVibeGraph.clipsBus2PeakDbL);
    drainAndMerge (mClipsBus2PeakDbR,   mVibeGraph.clipsBus2PeakDbR);
    drainAndMerge (mPluginsBus2PeakDbL, mVibeGraph.pluginsBus2PeakDbL);
    drainAndMerge (mPluginsBus2PeakDbR, mVibeGraph.pluginsBus2PeakDbR);
    // QA-SOUNDNESS: second drum kit's bus.
    drainAndMerge (mDrumsBus2PeakDbL,   mVibeGraph.drumsBus2PeakDbL);
    drainAndMerge (mDrumsBus2PeakDbR,   mVibeGraph.drumsBus2PeakDbR);

    // QA-AudioMeters (2026-05-24): per-kind insert mirror drain.  Same
    // drainAndMerge primitive as the bus loop above; 8 InsertKinds.  Audio
    // kind drains into the pre-existing mAudioRowPeakDb* arrays (kept under
    // that name for Builder grid backward compat per L9).
    for (int i = 0; i < kMaxLayerPages; ++i)
    {
        drainAndMerge (mLayerInsertPeakDbL[i], mVibeGraph.layerInsertPeakDbL[i]);
        drainAndMerge (mLayerInsertPeakDbR[i], mVibeGraph.layerInsertPeakDbR[i]);
    }
    for (int i = 0; i < kMaxBassPages; ++i)
    {
        drainAndMerge (mBassInsertPeakDbL[i], mVibeGraph.bassInsertPeakDbL[i]);
        drainAndMerge (mBassInsertPeakDbR[i], mVibeGraph.bassInsertPeakDbR[i]);
    }
    for (int i = 0; i < kMaxDrumPages; ++i)
    {
        drainAndMerge (mDrumInsertPeakDbL[i], mVibeGraph.drumInsertPeakDbL[i]);
        drainAndMerge (mDrumInsertPeakDbR[i], mVibeGraph.drumInsertPeakDbR[i]);
    }
    for (int i = 0; i < kMaxAudioRows; ++i)
    {
        drainAndMerge (mAudioRowPeakDbL[i], mVibeGraph.audioInsertPeakDbL[i]);
        drainAndMerge (mAudioRowPeakDbR[i], mVibeGraph.audioInsertPeakDbR[i]);
    }
    for (int i = 0; i < MixerChannelIds::kMaxAuxStrips; ++i)
    {
        drainAndMerge (mAuxInsertPeakDbL[i], mVibeGraph.auxInsertPeakDbL[i]);
        drainAndMerge (mAuxInsertPeakDbR[i], mVibeGraph.auxInsertPeakDbR[i]);
    }
    for (int i = 0; i < MixerChannelIds::kMaxVoxStrips; ++i)
    {
        drainAndMerge (mVoxInsertPeakDbL[i], mVibeGraph.voxInsertPeakDbL[i]);
        drainAndMerge (mVoxInsertPeakDbR[i], mVibeGraph.voxInsertPeakDbR[i]);
    }
    for (int i = 0; i < MixerChannelIds::kMaxInstStrips; ++i)
    {
        drainAndMerge (mInstInsertPeakDbL[i], mVibeGraph.instInsertPeakDbL[i]);
        drainAndMerge (mInstInsertPeakDbR[i], mVibeGraph.instInsertPeakDbR[i]);
    }
    for (int i = 0; i < MixerChannelIds::kMaxRustyStrips; ++i)
    {
        drainAndMerge (mRustyInsertPeakDbL[i], mVibeGraph.rustyInsertPeakDbL[i]);
        drainAndMerge (mRustyInsertPeakDbR[i], mVibeGraph.rustyInsertPeakDbR[i]);
    }
    // TS6 (BLU-447) -- MISSED, fixed TS7 2026-07-31.  Ninth InsertKind; the loop
    // above it was the last one TS6 left untouched.
    for (int i = 0; i < MixerChannelIds::kMaxPluginStrips; ++i)
    {
        drainAndMerge (mPluginInsertPeakDbL[i], mVibeGraph.pluginInsertPeakDbL[i]);
        drainAndMerge (mPluginInsertPeakDbR[i], mVibeGraph.pluginInsertPeakDbR[i]);
    }

    // Effect-rack slot atomics on every InsertNode + every BusNode (separate
    // surface; effect-panel DBFSMeter + VU input meters).
    mVibeGraph.promoteAllRackSlotSnapshots();
}

// 2026-05-07 (Batch 10): DSP-load measurement.
// Runs after dispatchBlock, which blocks until every render task completes, so
// (now - t0) is the block's real render wall-clock = how close it came to the
// buffer deadline.  That headroom fraction is what the meter DISPLAYS and what
// the overload / color-tier thresholds are calibrated against
// (Jeff, a1 2026-07-19).  Both MT and ST use the same wall-clock: under MT it
// already spans the parallel render (the audio thread waits for the workers).
// [Superseded QA-N (DIAG-02), which read the pool's summed per-task busy ticks
//  -- total work across cores.  That is NOT deadline-proximity: a healthy N-core
//  render reads >100% and false-tripped the overload/color at normal MT load.
//  The sum-of-cores machinery is now meter-unused -> route to the Phase-6
//  MT-diagnostic compile-gate (marathon 12e).]
void BaySickDAWProcessor::measureDspLoadAndOverload (juce::int64 t0Ticks, int numSamples)
{
    const double ticksPerSec = (double) juce::Time::getHighResolutionTicksPerSecond();
    const double bufDur      = numSamples / juce::jmax (1.0, mSampleRate);
    // A zero-length block carries no measurement, and the smoothing below is
    // keyed on the block duration, so there is nothing to fold in.  Snapping the
    // meter to zero here would be a lie about the render.
    if (bufDur <= 0.0) return;
    // a1: wall-clock render duration across dispatchBlock (both MT + ST) -- the
    // deadline-proximity headroom, not QA-N's sum-of-cores total work.
    const double workSeconds =
        (double) (juce::Time::getHighResolutionTicks() - t0Ticks) / ticksPerSec;
    // Display/threshold ceiling.  The old 10.f (1000%) headroom existed for
    // QA-Md's sum-of-cores readings and is moot under a1 (wall-clock rarely
    // exceeds ~2x the deadline), but the exact V1 cap stays a HOLD-FOR-Phase-6
    // UX call (marathon 12d = 2.0) -- see Main Plan §5 QA-Audit + Future State
    // CL-291.  Left at 10.f (harmless: wall-clock won't reach it) pending that pass.
    const float  rawLoad  = juce::jlimit (0.f, 10.f, (float) (workSeconds / bufDur));

    // Exponential smoothing with a block-INDEPENDENT time constant.  The former
    // fixed 0.85 / 0.15 pair was a per-BLOCK coefficient, so the meter's real
    // response ran from ~1 ms at 192k/32 to ~571 ms at 44.1k/4096 and only hit
    // its documented value at the one point it was tuned on.  kDspLoadTauSeconds
    // reproduces exactly that point: -(512/44100) / ln(0.85) = 71.4 ms.
    static constexpr double kDspLoadTauSeconds = 0.0714;
    if (bufDur != mDspLoadAlphaBufDur)
    {
        mDspLoadAlphaBufDur = bufDur;
        mDspLoadAlpha = (float) (1.0 - std::exp (-bufDur / kDspLoadTauSeconds));
    }

    const float prev     = mAudioDspLoad.load (std::memory_order_relaxed);
    const float smoothed = prev + mDspLoadAlpha * (rawLoad - prev);
    mAudioDspLoad.store (smoothed,         std::memory_order_relaxed);
    mDspOverload95.store (smoothed > 0.95f, std::memory_order_relaxed);
}

// ── Parameter sync helpers ────────────────────────────────────────────────────
// §P4.3 B7 (2026-04-22): legacy updateDrumsEQ + updateLayerPageEQsFromApvts +
// updateBassPageEQsFromApvts deleted along with their DSP instances
// (mDrumsEQDSP / mLayerPageEQs / mBassPageEQs).  All pre-rack EQs now live on
// InsertNode / BusNode preEq members and are sync'd by the unified
// updateAllPreRackEQsFromApvts pass (which iterates every registered mixer
// strip prefix).


// ── Session B: universal EQ update helpers ────────────────────────────────────
namespace
{
    // The ONE canonical enumeration of every mixer-strip prefix that owns an EQ
    // bank.  Its order defines the EQ param-pointer cache's strip-slot index, so
    // the registration-side resolver and both audio-side sweeps stay in lockstep
    // by construction -- there is no second list that could drift out of order.
    struct EqBusEntry
    {
        const char* prefix;
        EQ8MsDSP* (*post) (BaySickGraph&);
        EQ8MsDSP* (*pre)  (BaySickGraph&);
    };

    const EqBusEntry kEqBuses[] = {
        { "mixer_layers",     [](BaySickGraph& g) { return g.getLayersBusEQ();      }, [](BaySickGraph& g) { return g.getLayersBusPreEQ();      } },
        { "mixer_bass",       [](BaySickGraph& g) { return g.getBassBusEQ();        }, [](BaySickGraph& g) { return g.getBassBusPreEQ();        } },
        { "mixer_drums",      [](BaySickGraph& g) { return g.getDrumsBusEQ();       }, [](BaySickGraph& g) { return g.getDrumsBusPreEQ();       } },
        { "mixer_master",     [](BaySickGraph& g) { return g.getMasterEQ();         }, [](BaySickGraph& g) { return g.getMasterPreEQ();         } },
        { "mixer_fx",         [](BaySickGraph& g) { return g.getEffectsBusEQ();     }, [](BaySickGraph& g) { return g.getEffectsBusPreEQ();     } },
        { "mixer_clipsbus",   [](BaySickGraph& g) { return g.getAudioClipsBusEQ();  }, [](BaySickGraph& g) { return g.getAudioClipsBusPreEQ();  } },
        { "mixer_voxbus",     [](BaySickGraph& g) { return g.getVoxBusEQ();         }, [](BaySickGraph& g) { return g.getVoxBusPreEQ();         } },
        { "mixer_instbus",    [](BaySickGraph& g) { return g.getInstBusEQ();        }, [](BaySickGraph& g) { return g.getInstBusPreEQ();        } },
        { "mixer_voxbus2",    [](BaySickGraph& g) { return g.getVoxBus2EQ();        }, [](BaySickGraph& g) { return g.getVoxBus2PreEQ();        } },
        { "mixer_instbus2",   [](BaySickGraph& g) { return g.getInstBus2EQ();       }, [](BaySickGraph& g) { return g.getInstBus2PreEQ();       } },
        { "mixer_instbus3",   [](BaySickGraph& g) { return g.getInstBus3EQ();       }, [](BaySickGraph& g) { return g.getInstBus3PreEQ();       } },
        { "mixer_rustybus",   [](BaySickGraph& g) { return g.getRustyDrumsBusEQ();  }, [](BaySickGraph& g) { return g.getRustyDrumsBusPreEQ();  } },  // J-6
        { "mixer_pluginbus",  [](BaySickGraph& g) { return g.getPluginsBusEQ();     }, [](BaySickGraph& g) { return g.getPluginsBusPreEQ();     } },  // TS6 (missed, fixed TS7)
        { "mixer_layersbus2", [](BaySickGraph& g) { return g.getLayersBus2EQ();     }, [](BaySickGraph& g) { return g.getLayersBus2PreEQ();     } },  // T10
        { "mixer_bassbus2",   [](BaySickGraph& g) { return g.getBassBus2EQ();       }, [](BaySickGraph& g) { return g.getBassBus2PreEQ();       } },
        { "mixer_clipsbus2",  [](BaySickGraph& g) { return g.getClipsBus2EQ();      }, [](BaySickGraph& g) { return g.getClipsBus2PreEQ();      } },
        { "mixer_pluginbus2", [](BaySickGraph& g) { return g.getPluginsBus2EQ();    }, [](BaySickGraph& g) { return g.getPluginsBus2PreEQ();    } },
        { "mixer_drumsbus2",  [](BaySickGraph& g) { return g.getDrumsBus2EQ();      }, [](BaySickGraph& g) { return g.getDrumsBus2PreEQ();      } },  // QA-SOUNDNESS
    };

    struct EqInsertFamily { BaySickGraph::InsertKind kind; const char* prefixBase; int count; };

    constexpr EqInsertFamily kEqInsertFamilies[] = {
        { BaySickGraph::InsertKind::Layer,  "mixer_layer_",  kMaxLayerPages },
        { BaySickGraph::InsertKind::Bass,   "mixer_bass_",   kMaxBassPages  },
        { BaySickGraph::InsertKind::Drum,   "mixer_drum_",   kMaxDrumPages  },
        { BaySickGraph::InsertKind::Audio,  "mixer_audio_",  BaySickDAWProcessor::kMaxAudioRows },
        { BaySickGraph::InsertKind::Aux,    "mixer_aux_",    MixerChannelIds::kMaxAuxStrips    },
        { BaySickGraph::InsertKind::Vox,    "mixer_vox_",    MixerChannelIds::kMaxVoxStrips    },
        { BaySickGraph::InsertKind::Inst,   "mixer_inst_",   MixerChannelIds::kMaxInstStrips   },
        { BaySickGraph::InsertKind::Rusty,  "mixer_rusty_",  MixerChannelIds::kMaxRustyStrips  },  // J-6
        { BaySickGraph::InsertKind::Plugin, "mixer_plugin_", MixerChannelIds::kMaxPluginStrips },  // TS6
    };

    constexpr int eqInsertSlotTotal()
    {
        int n = 0;
        for (const auto& f : kEqInsertFamilies) n += f.count;
        return n;
    }
} // namespace

// Generic EQ syncer.  Reads the full per-band set -- 9 static params plus the
// 8-param Dynamic block when registered -- x 8 bands for both mid + side inner
// EQs and applies via the standard setBand* setters (all internally CPU-guarded
// so no-change calls are free).
//
// AUDIO THREAD.  Every value arrives through the pre-resolved pointer cache
// (see mEqParamCache in the header) -- no APVTS map lookup, no juce::String, no
// allocation on this path.  A band whose Freq pointer is still null has not been
// registered yet (new InsertNode whose ensureMixerStripParams has not run) and
// is skipped, exactly as the old getParameter existence test did.
void BaySickDAWProcessor::updateEQFromCache (EQ8MsDSP* eq, int stripSlot, int bank)
{
    if (eq == nullptr || stripSlot < 0 || stripSlot >= kEqNumStripSlots) return;

    auto syncSide = [this, stripSlot, bank] (EQ8DSP& side, int sideIdx)
    {
        for (int b = 0; b < kEqBands; ++b)
        {
            const auto& slots = mEqParamCache[(size_t) eqCacheIndex (stripSlot, bank, sideIdx, b)];

            // Freq is the band's publication flag: the message thread stores it
            // LAST with release, so a non-null read here makes all sixteen other
            // pointers visible and the relaxed loads below are safe.
            auto* freqP = slots.p[eqSlotFreq].load (std::memory_order_acquire);
            if (freqP == nullptr) continue;

            auto get = [&slots] (int which) -> float
            {
                if (auto* p = slots.p[which].load (std::memory_order_relaxed))
                    return p->load (std::memory_order_relaxed);
                return 0.f;
            };

            auto cur = side.getBand(b);
            float f  = freqP->load (std::memory_order_relaxed);
            if (f  != cur.freq)   side.setBandFreq (b, f);
            float gn = get(eqSlotGain);
            if (gn != cur.gainDb) side.setBandGain (b, gn);
            float q  = get(eqSlotQ);
            if (q  != cur.q)      side.setBandQ    (b, q);
            int   t  = (int) get(eqSlotType);
            if (t  != cur.type)   side.setBandType (b, t);
            int   s  = (int) get(eqSlotSlope);
            if (s  != cur.slope)  side.setBandSlope(b, s);
            side.setBandOn    (b, get(eqSlotOn)   > 0.5f);
            side.setBandMuted (b, get(eqSlotMute) > 0.5f);
            side.setBandSoloed(b, get(eqSlotSolo) > 0.5f);
            int ch = juce::jlimit(0, 4, (int) get(eqSlotChannel));
            if (ch != cur.channel) side.setBandChannel(b, ch);
            // 12j Dynamic EQ params (read only when registered - Dynamic's
            // absence short-circuits all subsequent reads cheaply).
            if (slots.p[eqSlotDynamic].load (std::memory_order_relaxed) != nullptr)
            {
                side.setBandDynamic  (b, get(eqSlotDynamic) > 0.5f);
                float thr = get(eqSlotThreshold); if (thr != cur.threshold) side.setBandThreshold(b, thr);
                float rt  = get(eqSlotRatio);     if (rt  != cur.ratio)     side.setBandRatio    (b, rt);
                float at  = get(eqSlotAttack);    if (at  != cur.attack)    side.setBandAttack   (b, at);
                float re  = get(eqSlotRelease);   if (re  != cur.release)   side.setBandRelease  (b, re);
                float rg  = get(eqSlotRange);     if (rg  != cur.rangeDb)   side.setBandRange    (b, rg);
                side.setBandUpward(b, get(eqSlotUpward) > 0.5f);
                int sc = (int) get(eqSlotScSource);
                if (sc != cur.scSourceId) side.setBandScSource(b, sc);
            }
        }
    };

    syncSide (eq->mid(),  0);
    syncSide (eq->side(), 1);
}

// Iterate every post-rack EQ instance (18 buses + the insert families) and sync
// it from its cached param pointers. Safe to call from processBlock - getters
// return nullptr for indices that have no registered InsertNode, and the inner
// band loop short-circuits on strips whose params are not cached yet.
void BaySickDAWProcessor::updateAllPostRackEQsFromApvts()
{
    static_assert ((int) (sizeof (kEqBuses) / sizeof (kEqBuses[0])) == kEqNumBusSlots,
                   "kEqBuses and kEqNumBusSlots must agree - the EQ cache is indexed by both");
    static_assert (eqInsertSlotTotal() == kEqNumInsertSlots,
                   "kEqInsertFamilies and kEqNumInsertSlots must agree - the EQ cache is indexed by both");

    for (int i = 0; i < kEqNumBusSlots; ++i)
        if (auto* eq = kEqBuses[i].post (mVibeGraph))
            updateEQFromCache (eq, i, kEqBankPost);

    int slot = kEqNumBusSlots;
    for (const auto& fam : kEqInsertFamilies)
    {
        for (int i = 0; i < fam.count; ++i)
            if (auto* eq = mVibeGraph.getInsertEQ (fam.kind, i))
                updateEQFromCache (eq, slot + i, kEqBankPost);
        slot += fam.count;
    }
}

// §P4.3: Pre-rack EQ sync - mirror of updateAllPostRackEQsFromApvts but uses
// the `_preeq_` sub-prefix to read pre-EQ params + the new pre-EQ accessors.
// Bus + insert pre-EQ DSPs were added in B2; their params in B3.  Called once
// per processBlock alongside the post-rack version.
void BaySickDAWProcessor::updateAllPreRackEQsFromApvts()
{
    for (int i = 0; i < kEqNumBusSlots; ++i)
        if (auto* eq = kEqBuses[i].pre (mVibeGraph))
            updateEQFromCache (eq, i, kEqBankPre);

    int slot = kEqNumBusSlots;
    for (const auto& fam : kEqInsertFamilies)
    {
        for (int i = 0; i < fam.count; ++i)
            if (auto* eq = mVibeGraph.getInsertPreEQ (fam.kind, i))
                updateEQFromCache (eq, slot + i, kEqBankPre);
        slot += fam.count;
    }
}

// Band values are APVTS-backed, so File > New's default sweep reaches them
// through the two passes above.  Main level, phase mode, linear precision,
// anti-cramping, proportional Q and the A/B spare have no parameter behind them
// -- they live only in the DSP and in the saved rack blob, which File > Open
// restores via BaySickGraph::applyRackStates and File > New does not write at all.
// Without this, a blank project keeps the previous one's output trim (audible:
// EQ8DSP::isIdentity refuses to short-circuit on a non-zero main level) and its
// linear-phase FFT latency (BaySickGraph sums it into PDC), with the previous
// project's B bank still sitting in the spare.
//
// THREAD SAFETY: setPhaseMode allocates and frees the linear-phase processor and
// the band push writes coefficients processBlock reads, so the caller runs this
// under the project-load shield with a settle already paid.
void BaySickDAWProcessor::resetEqStatesToDefaults()
{
    auto forEachSide = [this] (auto&& fn)
    {
        auto both = [&fn] (EQ8MsDSP* eq)
        {
            if (eq == nullptr) return;
            fn (eq->mid());
            fn (eq->side());
        };

        for (int i = 0; i < kEqNumBusSlots; ++i)
        {
            both (kEqBuses[i].post (mVibeGraph));
            both (kEqBuses[i].pre  (mVibeGraph));
        }
        for (const auto& fam : kEqInsertFamilies)
            for (int i = 0; i < fam.count; ++i)
            {
                both (mVibeGraph.getInsertEQ    (fam.kind, i));
                both (mVibeGraph.getInsertPreEQ (fam.kind, i));
            }
    };

    // Back to the main bank BEFORE the band sync: swapWithSpare exchanges the
    // two banks, so leaving it until afterwards would push the freshly
    // defaulted bands back into the spare and pull the old ones into view.
    forEachSide ([] (EQ8DSP& s) { if (s.isViewingSpare()) s.swapWithSpare(); });

    // Land the defaulted band values now rather than on the next block, so the
    // saveToSpare below seeds the spare from defaults and not from the bank the
    // outgoing project left behind.
    updateAllPostRackEQsFromApvts();
    updateAllPreRackEQsFromApvts();

    forEachSide ([] (EQ8DSP& s)
    {
        s.lockSpare (false);
        s.saveToSpare();
        s.setPhaseMode (EQ8DSP::PhaseMode::Standard);
        s.setLinearPhasePrecision (EQ8DSP::kDefaultLinearPrec);
        s.setAntiCramping (false);
        s.setProportionalQ (true);
        s.setMainLevel (0.0f);
    });
}

// ── CL-281 decode-once cache helpers ─────────────────────────────────────────

juce::String BaySickDAWProcessor::clipAudioCacheKey (const juce::File& resolvedFile)
{
    if (! resolvedFile.existsAsFile())
        return {};

    // Identity is the RESOLVED path, which is what collapses "library:x",
    // "mysamples:x" and the absolute spelling of one file onto one entry.
    // Lower-cased because this is a Windows-only app and its paths are
    // case-insensitive, so two spellings that differ only in case ARE the same
    // file and must not decode twice.
    //
    // Size + modification time are the change stamp: the decoded PCM is a
    // function of the file's bytes and nothing else, so a file edited or
    // replaced under a live project keys somewhere new and decodes again
    // instead of playing stale audio.
    return resolvedFile.getFullPathName().toLowerCase()
         + "|" + juce::String (resolvedFile.getSize())
         + "|" + juce::String (resolvedFile.getLastModificationTime().toMilliseconds());
}

DecodedClipAudioPtr
BaySickDAWProcessor::decodeClipAudioIfCacheable (juce::AudioFormatReader& reader)
{
    // The threshold and the stereo fold are AudioClipStreamer's, read from that
    // class rather than restated, so cached and streamed clips split at exactly
    // one place: a file the streamer would have RAM-loaded is cached instead,
    // and everything bigger keeps streaming.
    const juce::int64 len = reader.lengthInSamples;
    const int numCh = (int) juce::jmin ((juce::int64) 2, (juce::int64) reader.numChannels);
    if (len <= 0 || numCh <= 0)
        return {};

    const juce::int64 totalBytes = (juce::int64) sizeof (float)
                                       * (juce::int64) numCh * len;
    if (totalBytes <= 0 || totalBytes > AudioClipStreamer::kRamThresholdBytes)
        return {};

    auto decoded = std::make_shared<DecodedClipAudio>();
    decoded->fileSampleRate  = (reader.sampleRate > 0.0) ? reader.sampleRate : 44100.0;
    decoded->numChannels     = numCh;
    decoded->lengthInSamples = len;
    decoded->samples.setSize (numCh, (int) len, false, true, false);
    reader.read (&decoded->samples, 0, (int) len, 0, true, true);
    return decoded;
}

// ── Audio clip playback ───────────────────────────────────────────────────────
void BaySickDAWProcessor::rebuildAudioClipPlayers()
{
    if (!mPatternManager) return;

    // 2026-05-06 (Batch 9c B1): build a fresh AudioClipSnapshot + assign it
    // a new monotonic generation.  Atomic-exchanges the publication pointer
    // and retires the OLD snapshot to mClipRetirement so its slow teardown --
    // ~AudioClipStreamer (file close + bg-thread unregister), plus the release
    // of any cached PCM this was the last holder of -- runs on the GC drainer
    // thread instead of here on the message thread.
    // Set when this pass banks an undecodable-clip entry.  A live edit has to
    // drain it: the store is process-wide, so an entry left banked by a
    // drag-drop surfaces later under whatever unrelated gesture happens to
    // drain next, wearing that gesture's source noun.
    bool bankedUnreadable = false;

    // CL-281: every cache key this pass actually used.  Anything left out of it
    // is referenced by no clip in the arrangement being published, which is the
    // eviction rule (see mDecodedClipCache).
    std::set<juce::String> liveCacheKeys;

    auto newSnap = std::make_unique<AudioClipSnapshot>();
    auto& newPlayers = newSnap->players;
    for (int i = 0; i < mPatternManager->getNumBlocks(); ++i)
    {
        auto& blk = mPatternManager->getBlock(i);
        if (blk.clipType != ClipType::Audio || blk.audioFilePath.isEmpty() || blk.muted)
            continue;
        // NOTE: no isRowAudible() gate here - runtime mute/solo is handled in the
        // live render loop so toggling mute does not require a player rebuild.

        // QA-ClipDrop Task 2 (SC-I) load migration: legacy Audio clips saved with
        // routeChannel==0 routed by their grid row.  Stamp them ONCE with their
        // owning Clips-page strip (== audioInsert of the row they were created on)
        // so they route by owner and survive a block move.  Idempotent: once
        // non-zero it never re-derives, so a later move preserves the owner (the
        // post-Task-5 retag and placeAudioLibraryEntry already stamp new clips).
        if (blk.routeChannel == 0)
            blk.routeChannel = MixerChannelIds::audioInsert (blk.trackRow);

        // P4: resolve relative paths like "Samples/kick.wav" against the
        // current project folder.  Absolute paths fall through unchanged
        // (legacy pre-P4 projects stored full paths).
        const auto resolvedFile = resolveProjectFile (blk.audioFilePath);

        // CL-281: the cache is consulted BEFORE the reader is opened, so a hit
        // costs two stat calls instead of a file open plus a full decode.  This
        // is what makes the same file placed four times decode once, and what
        // stops every arrangement edit re-decoding the whole timeline.  A file
        // replaced on disk changes size or modification time, so it keys to a
        // different entry and decodes again.
        const juce::String cacheKey = clipAudioCacheKey (resolvedFile);
        DecodedClipAudioPtr cached;
        if (cacheKey.isNotEmpty())
        {
            const auto it = mDecodedClipCache.find (cacheKey);
            if (it != mDecodedClipCache.end())
                cached = it->second;
        }

        std::unique_ptr<juce::AudioFormatReader> rawReader;
        if (cached == nullptr)
            rawReader = SafeAudioReader::guard (
                std::unique_ptr<juce::AudioFormatReader> (mAudioFormatManager.createReaderFor (resolvedFile)));

        if (cached == nullptr && rawReader == nullptr)
        {
            // Present-but-undecodable (truncated WAV, sync artifact): the block
            // stays visible on the grid and contributes silence, so it must not
            // vanish into the missing-file report's blind spot.  Deduped per
            // path -- this rebuild runs on every arrangement edit.
            if (resolvedFile.existsAsFile()
                && ! mReportedUnreadableClips.contains (blk.audioFilePath))
            {
                mReportedUnreadableClips.add (blk.audioFilePath);
                MissingFileReport::add ("Clip audio (failed to load)", blk.audioFilePath);
                bankedUnreadable = true;
            }
            continue;
        }

        AudioClipPlayer p;
        // QA-Ea Task 0c (2026-05-20 - Option A slip-edit + sub-bar) / 8A:
        // effectiveStartBeats reads the beats-authoritative block start
        // (sub-bar precision, possibly negative after a slip-edit drag-left).
        // The audio loop math at PluginProcessor.cpp
        // :485-785 already handles negative clipStartBeat correctly
        // (outPosInClip = projectStart - clipStart works for clipStart < 0;
        // the (un)played pre-bar portion is naturally skipped by the
        // projectStart >= 0 transport).
        p.clipStartBeat  = effectiveStartBeats (blk);
        // 2026-04-24: prefer block.lengthTicks when set (sub-bar precision
        // from recordings) so playback ends at the real audio end, not the
        // ceil'd bar count.
        p.clipEndBeat    = effectiveStartBeats (blk) + effectiveLengthBeats (blk);
        p.trackRow       = blk.trackRow;
        p.routeChannel   = blk.routeChannel;   // I-16 G-9: Vox/Inst page link
        p.originalBPM    = (blk.originalBPM > 0.f) ? blk.originalBPM : 120.f;
        p.stretchMode    = blk.stretchMode;
        p.fileSampleRate = (cached != nullptr) ? cached->fileSampleRate
                                               : rawReader->sampleRate;
        // QA-Ea Task 0c (FL pre-roll record + non-destructive clip trim):
        // copy the block's file-position offset to the player so the audio
        // thread can read it without a back-pointer into ArrangementBlock.
        // Component 5 (below) consumes this in the file-position computation
        // sites in renderAudioClipsForRow + decodeFilePlayClip / finalizeFilePlayStrip.
        p.contentStartSamples = blk.contentStartSamples;
        // D3: look up the source clip's choke group from the library.
        p.chokeGroup     = 0;
        for (int li = 0; li < mPatternManager->getNumAudioLibrary(); ++li)
            if (mPatternManager->getAudioLibraryPath(li) == blk.audioFilePath)
                { p.chokeGroup = mPatternManager->getAudioLibraryChokeGroup(li); break; }

        // CL-281: a cache MISS on a file at or under the RAM threshold decodes
        // here, once, and the result is banked for every later clip and every
        // later rebuild.  Anything bigger returns null and keeps the streaming
        // path, whose seek(0) synchronously pre-fills kPrefillSeconds so the
        // clip plays immediately.
        if (cached == nullptr)
        {
            cached = decodeClipAudioIfCacheable (*rawReader);
            if (cached != nullptr && cacheKey.isNotEmpty())
                mDecodedClipCache[cacheKey] = cached;
        }

        if (cached != nullptr)
        {
            if (cacheKey.isNotEmpty())
                liveCacheKeys.insert (cacheKey);
            p.source = std::make_unique<ClipSource> (std::move (cached));
        }
        else
        {
            auto stream = std::make_unique<AudioClipStreamer> (std::move (rawReader),
                                                               mAudioFileThread);
            stream->seek (0);
            p.source = std::make_unique<ClipSource> (std::move (stream));
        }

        // QA-ClipPlayback Task 3: always create the phase vocoder so length-preserving
        // pitch (and reverse) work live on any clip, not just BPM-stretched ones.  It
        // is bypassed in the render when the clip is forward + unstretched + unpitched
        // (usePV false), so no CPU cost when idle -- only the buffer memory.
        {
            const int pvCh = p.source->getNumChannels();
            p.vocoder = std::make_unique<PhaseVocoder> (pvCh);
            // Pre-allocate scratch buffers - sized for worst-case block + PV headroom.
            // pvInBuf:  file samples fed into PV each block (up to ~4x block size for large stretch)
            // pvOutBuf: stretched output from PV (one full analysis window of headroom)
            const int maxBlockSamples = 8192;
            const int pvInCap  = maxBlockSamples * 4 + PhaseVocoder::kFFTSize;
            const int pvOutCap = maxBlockSamples * 4 + PhaseVocoder::kFFTSize;
            p.pvInBuf .setSize (pvCh, pvInCap,  false, true, false);
            p.pvOutBuf.setSize (pvCh, pvOutCap, false, true, false);
            // The OLA queue has to admit what this consumer can ask for in one
            // pull, and peekOutput is already clamped to pvOutBuf -- so pvOutCap
            // IS the maximum, not a new constant.  Without this the vocoder keeps
            // its fixed default queue and the output half stays undersized.
            // The FILE rate, not the device rate: the analysis window is a
            // duration in the source material, so a 96 kHz file needs twice the
            // sample count a 44.1 kHz one does to cover the same milliseconds.
            // Set at line ~5547 above, so it is in hand here.
            p.vocoder->prepare (pvOutCap, p.fileSampleRate);
        }

        p.expectedFilePos = 0;
        // QA-ClipPlayback Task 2: prepare the per-clip control-chain filter (message
        // thread -- allocation ok).  SVF lowpass, 2 ch, current sample rate.  maxBlock
        // is nominal (processSample doesn't allocate per-block).  The filter bakes
        // its coefficients against the rate given here and never re-reads it, so
        // prepareToPlay re-prepares every live player -- see the sweep there.
        {
            const juce::dsp::ProcessSpec spec { mSampleRate,
                                                (juce::uint32) juce::jmax (1, mBlockSize),
                                                (juce::uint32) 2 };
            p.clipFilter.prepare (spec);
            p.clipFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        }
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

    // CL-281 eviction.  Runs AFTER the publish so the erase can only ever drop
    // the map's own alias: the snapshot that just went live holds one per clip,
    // and the retired one holds its own until the GC drainer destroys it.  An
    // entry no clip references any more therefore frees here, on the message
    // thread, and one that a still-retired snapshot is reading frees later on
    // the drainer -- neither on the audio thread.
    for (auto it = mDecodedClipCache.begin(); it != mDecodedClipCache.end(); )
    {
        if (liveCacheKeys.find (it->first) != liveCacheKeys.end())
            ++it;
        else
            it = mDecodedClipCache.erase (it);
    }

    // Drained at the producer rather than at each of this function's ~10 call
    // sites, so a caller added later cannot forget it.  Two things say "someone
    // else owns this report", and a bare drain here would steal from both:
    // the project-load shield (raised = a project/template restore is running
    // and the entry belongs in that load's single batched dialog) and an open
    // ScopedGesture (this rebuild is a side effect of a tab duplicate / preset
    // load, not the gesture the user performed).  Neither is expressible as a
    // ScopedGesture around this function -- the restore opens no scope of its
    // own, so one here would become the outermost and drain mid-load.
    if (bankedUnreadable && ! isProjectLoadInProgress() && ! isNonRealtime()
        && ! MissingFileReport::isGestureOpen())
        MissingFileReport::reportIfAny ("arrangement");
}

// ── QA-F Task 1: offline channel-composite renderer ──────────────────────────
// MESSAGE THREAD ONLY.  Shared analysis foundation for BaySickAlign /
// BaySickPitch: decodes every un-muted arrangement audio clip routed to
// `channelId` (INCLUDING Vox/Inst FilePlay takes, which the realtime row
// renderer skips) at its grid position and sums it, mono, at the device
// sample rate.  Opens its own readers from the block paths and never touches
// the audio-thread snapshot / streamers / vocoders, so it is safe to call
// during live playback.  Strip/row mutes and choke state are deliberately
// ignored (block-level mute IS respected): analysis must see the channel's
// content even while the user monitors with the strip muted.
// outStartBeat = the timeline beat of composite sample 0; composite index i
// maps to timeline sample clipBeatToSample(outStartBeat, ...) + i.
juce::AudioBuffer<float> BaySickDAWProcessor::renderChannelComposite (int channelId,
                                                                     double& outStartBeat,
                                                                     juce::int64& outStartSample)
{
    outStartBeat   = 0.0;
    outStartSample = 0;
    juce::AudioBuffer<float> composite;
    if (mPatternManager == nullptr)
        return composite;

    const double globalBpm  = juce::jmax (1.0, mPatternManager->getGlobalTempo());
    const double secPerBeat = 60.0 / globalBpm;

    struct CompClip
    {
        std::unique_ptr<juce::AudioFormatReader> reader;
        double      startBeat    = 0.0;
        double      endBeat      = 0.0;     // already file-EOF-clamped
        double      originalBPM  = 120.0;
        bool        stretchMode  = true;
        juce::int64 contentStart = 0;
    };
    std::vector<CompClip> clips;

    for (int i = 0; i < mPatternManager->getNumBlocks(); ++i)
    {
        auto& blk = mPatternManager->getBlock (i);
        // alignBake exclusion: a placed bake must never feed the composite
        // of the channel it was rendered FROM (see ArrangementBlock::alignBake).
        if (blk.clipType != ClipType::Audio || blk.audioFilePath.isEmpty()
            || blk.muted || blk.alignBake)
            continue;

        // Same owner resolution as the realtime paths: routeChannel when
        // stamped; legacy 0 falls back to the creation row's Clips strip.
        // Read-only here -- rebuildAudioClipPlayers owns the migration stamp.
        const int route = (blk.routeChannel != 0)
            ? blk.routeChannel
            : MixerChannelIds::audioInsert (blk.trackRow);
        if (route != channelId)
            continue;

        auto reader = SafeAudioReader::guard (std::unique_ptr<juce::AudioFormatReader> (
            mAudioFormatManager.createReaderFor (resolveProjectFile (blk.audioFilePath))));
        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
            continue;

        CompClip c;
        c.startBeat    = effectiveStartBeats (blk);
        c.endBeat      = c.startBeat + effectiveLengthBeats (blk);
        c.originalBPM  = (blk.originalBPM > 0.f) ? (double) blk.originalBPM : 120.0;
        c.stretchMode  = blk.stretchMode;
        c.contentStart = juce::jmax ((juce::int64) 0, blk.contentStartSamples);
        // File-EOF clamp in the beat domain (consumption per beat is mode-
        // blind -- see clipFilePosForBeat).
        const double playableBeats =
            (double) (reader->lengthInSamples - c.contentStart)
            / reader->sampleRate * c.originalBPM / 60.0;
        c.endBeat = juce::jmin (c.endBeat, c.startBeat + playableBeats);
        if (c.endBeat <= c.startBeat)
            continue;

        c.reader = std::move (reader);
        clips.push_back (std::move (c));
    }

    if (clips.empty())
        return composite;

    double startBeat = clips.front().startBeat;
    double endBeat   = clips.front().endBeat;
    for (const auto& c : clips)
    {
        startBeat = juce::jmin (startBeat, c.startBeat);
        endBeat   = juce::jmax (endBeat,   c.endBeat);
    }

    const juce::int64 compStart = clipBeatToSample (startBeat, secPerBeat, mSampleRate);
    const juce::int64 compEnd   = clipBeatToSample (endBeat,   secPerBeat, mSampleRate);
    // 30-min cap: corrupt beat data would otherwise demand a multi-GB
    // allocation; align/pitch material is song-length (minutes).  Truncates
    // silently past the cap -- acceptable for an analysis composite.
    const juce::int64 totalLen = juce::jmin (compEnd - compStart,
                                             (juce::int64) (1800.0 * mSampleRate));
    if (totalLen <= 0)
        return composite;

    composite.setSize (1, (int) totalLen);
    composite.clear();
    outStartBeat   = startBeat;
    outStartSample = compStart;

    // Timeline sample -> beat: exact inverse of clipBeatToSample (map when
    // published, linear fallback otherwise).
    auto beatAtTimeline = [secPerBeat, this] (juce::int64 t)
    {
        if (TempoMap::isActive())
            return TempoMap::beatAtSample (t);
        return (double) t / (secPerBeat * mSampleRate);
    };

    for (auto& c : clips)
    {
        const juce::int64 clipS  = clipBeatToSample (c.startBeat, secPerBeat, mSampleRate);
        const juce::int64 clipE  = clipBeatToSample (c.endBeat,   secPerBeat, mSampleRate);
        const juce::int64 dstOff = clipS - compStart;
        const int dstLen = (int) juce::jmin (clipE - clipS,
                                             (juce::int64) composite.getNumSamples() - dstOff);
        if (dstLen <= 0 || dstOff < 0)
            continue;
        const juce::int64 tEnd = clipS + dstLen;

        // Mono-fold the rendered source range into RAM once (chunked read).
        // Sized from the beats actually rendered, not the full clip, so the
        // 30-min composite cap bounds this too; 2^28-sample backstop (~1 GB)
        // skips anything a degenerate SR/length combo could still ask for.
        const double srcPerBeat    = c.reader->sampleRate * 60.0 / c.originalBPM;
        const double renderedBeats = juce::jmin (c.endBeat - c.startBeat,
                                                 beatAtTimeline (tEnd) - c.startBeat);
        const juce::int64 srcLen = juce::jmin (
            c.reader->lengthInSamples - c.contentStart,
            (juce::int64) std::ceil (juce::jmax (0.0, renderedBeats) * srcPerBeat) + 8);
        if (srcLen <= 0 || srcLen > (juce::int64) 1 << 28)
            continue;

        juce::AudioBuffer<float> monoSrc (1, (int) srcLen);
        monoSrc.clear();
        {
            const int nCh = juce::jmax (1, (int) c.reader->numChannels);
            juce::AudioBuffer<float> tmp (nCh, 1 << 16);
            juce::int64 done = 0;
            while (done < srcLen)
            {
                const int n = (int) juce::jmin ((juce::int64) tmp.getNumSamples(),
                                                srcLen - done);
                tmp.clear();
                if (! c.reader->read (&tmp, 0, n, c.contentStart + done, true, true))
                    break;
                for (int ch = 0; ch < nCh; ++ch)
                    monoSrc.addFrom (0, (int) done, tmp, ch, 0, n, 1.0f / (float) nCh);
                done += n;
            }
        }

        // Constant-tempo spans across the clip's rendered range.
        struct Span { juce::int64 t0, t1; double bpm; };
        std::vector<Span> spans;
        if (TempoMap::isActive())
        {
            juce::int64 t = clipS;
            while (t < tEnd)
            {
                const double      bpm = TempoMap::bpmAtSample (t);
                const juce::int64 nb  = TempoMap::nextBoundaryAfter (t);
                const juce::int64 t1  = (nb < 0 || nb > tEnd) ? tEnd : nb;
                spans.push_back ({ t, t1, juce::jmax (1.0, bpm) });
                t = t1;
            }
        }
        else
            spans.push_back ({ clipS, tEnd, globalBpm });

        bool needPV = false;
        if (c.stretchMode)
            for (const auto& s : spans)
                if (std::abs (c.originalBPM / s.bpm - 1.0) > 0.001)
                    { needPV = true; break; }

        // F3-style 5 ms edge declick (matches the realtime clip render; also
        // keeps clip boundaries from reading as spectral-flux onsets).
        const int fadeN = juce::jmax (1, juce::jmin (
            (int) std::round (mSampleRate * 0.005), dstLen / 2));
        auto edgeGain = [fadeN, dstLen] (int p)
        {
            float g = 1.0f;
            if (p < fadeN)           g = (float) (p + 1) / (float) fadeN;
            if (p >= dstLen - fadeN) g = juce::jmin (g, (float) (dstLen - p) / (float) fadeN);
            return g;
        };

        const float* src = monoSrc.getReadPointer (0);
        float*       dst = composite.getWritePointer (0);

        if (! needPV)
        {
            // Direct / resample decode: exact beat-domain anchor at each span
            // start, linear advance inside (constant bpm) -- positions match
            // the realtime paths through every tempo step.
            for (const auto& s : spans)
            {
                double fp = clipFilePosForBeat (beatAtTimeline (s.t0) - c.startBeat,
                                                c.reader->sampleRate, c.originalBPM, 0.0);
                const double fpStep = (s.bpm / c.originalBPM)
                                      * (c.reader->sampleRate / mSampleRate);
                for (juce::int64 t = s.t0; t < s.t1; ++t, fp += fpStep)
                {
                    const int ip = (int) fp;
                    if (ip + 1 >= (int) srcLen) break;
                    const float frac = (float) (fp - (double) ip);
                    const float v    = src[ip] + (src[ip + 1] - src[ip]) * frac;
                    const int   p    = (int) (t - clipS);
                    dst[dstOff + p] += v * edgeGain (p);
                }
            }
        }
        else
        {
            // Stretch decode: offline PhaseVocoder pass per constant-tempo
            // span (pitch preserved, matching the realtime stretch path).
            // The synthesis hop is integer-rounded, so the EFFECTIVE ratio is
            // synthHop/kHop, not the request -- both the pre-roll skip and
            // the exact-length resample below must use it or the span end
            // drifts ~0.1% (tens of ms over a minute of material).  Span
            // boundaries reset PV phase; a seam there coincides with a tempo
            // step (already a musical discontinuity).
            PhaseVocoder pv (1);
            juce::AudioBuffer<float> feed   (1, PhaseVocoder::kHopSize);
            juce::AudioBuffer<float> pvPull (1, 1 << 14);
            std::vector<float> outAccum;

            for (const auto& s : spans)
            {
                const double ratio = juce::jlimit (1.0 / 64.0, 64.0,
                                                   c.originalBPM / s.bpm);
                const double effRatio =
                    (double) juce::jmax (1, juce::roundToInt (
                        (double) PhaseVocoder::kHopSize * ratio))
                    / (double) PhaseVocoder::kHopSize;

                const double srcA = clipFilePosForBeat (
                    beatAtTimeline (s.t0) - c.startBeat,
                    c.reader->sampleRate, c.originalBPM, 0.0);
                const double srcB = clipFilePosForBeat (
                    beatAtTimeline (s.t1) - c.startBeat,
                    c.reader->sampleRate, c.originalBPM, 0.0);

                const juce::int64 srcA64 = juce::jlimit ((juce::int64) 0, srcLen,
                    (juce::int64) std::floor (srcA));
                const juce::int64 srcB64 = juce::jlimit (srcA64, srcLen,
                    (juce::int64) std::ceil (srcB));
                if (srcB64 <= srcA64)
                    continue;

                // Pre-roll primes the OLA ramp-in; tail flushes the settle
                // margin.  Both scale up for compressive ratios (< 1) where
                // one source sample yields < 1 output sample.
                const juce::int64 preN = juce::jmin (srcA64,
                    (juce::int64) std::ceil ((double) PhaseVocoder::kFFTSize
                                             / juce::jmin (1.0, ratio)));
                const juce::int64 tailN = (juce::int64) std::ceil (
                    2.0 * PhaseVocoder::kFFTSize / juce::jmin (1.0, ratio));

                const juce::int64 outProjected = (juce::int64) (
                    (double) (preN + (srcB64 - srcA64) + tailN) * effRatio) + 4096;
                if (outProjected > (juce::int64) 1 << 28)
                    continue;   // hostile ratio x length combo -- skip span

                pv.reset();
                pv.setStretchRatio (ratio);
                outAccum.clear();
                outAccum.reserve ((size_t) outProjected);

                // The immediate pull() drain after every push is what keeps the
                // OLA queue shallow at large ratios -- one frame at ratio 64
                // advances the write head by 32768.  (The input ring is
                // self-limiting; PhaseVocoder::push drains as it writes.)
                juce::int64 fpos = srcA64 - preN;
                const juce::int64 feedEnd = srcB64 + tailN;
                while (fpos < feedEnd)
                {
                    const int n = (int) juce::jmin (
                        (juce::int64) PhaseVocoder::kHopSize, feedEnd - fpos);
                    feed.clear();
                    const juce::int64 availSrc = juce::jmin ((juce::int64) n,
                                                             srcLen - fpos);
                    if (availSrc > 0)
                        feed.copyFrom (0, 0, monoSrc, 0, (int) fpos, (int) availSrc);
                    pv.push (feed, 0, n);
                    for (int got = 0;
                         (got = pv.pull (pvPull, 0, pvPull.getNumSamples())) > 0;)
                        outAccum.insert (outAccum.end(),
                                         pvPull.getReadPointer (0),
                                         pvPull.getReadPointer (0) + got);
                    fpos += n;
                }

                // Map the span onto [outSkip, outSkip + stretchedLen) and
                // resample to the exact device-sample span length (absorbs
                // the hop rounding; SR conversion happens here too).
                const double outSkip      = (double) preN * effRatio;
                const double stretchedLen = juce::jmax (1.0,
                    (double) (srcB64 - srcA64) * effRatio);
                const juce::int64 spanDst = s.t1 - s.t0;

                for (juce::int64 j = 0; j < spanDst; ++j)
                {
                    const double op = outSkip
                        + ((double) j / (double) spanDst) * stretchedLen;
                    const int ip = (int) op;
                    if (ip + 1 >= (int) outAccum.size()) break;
                    const float fr = (float) (op - (double) ip);
                    const float v  = outAccum[(size_t) ip]
                        + (outAccum[(size_t) ip + 1] - outAccum[(size_t) ip]) * fr;
                    const int p = (int) ((s.t0 - clipS) + j);
                    dst[dstOff + p] += v * edgeGain (p);
                }
            }
        }
    }

    return composite;
}

juce::int64 BaySickDAWProcessor::channelClipSignature (int channelId) const
{
    if (mPatternManager == nullptr) return 0;
    juce::int64 sig = 0;
    for (int i = 0; i < mPatternManager->getNumBlocks(); ++i)
    {
        const auto& blk = mPatternManager->getBlock (i);
        // Filter matches renderChannelComposite exactly (the signature hashes
        // what the composite would render -- a bake placement must not read
        // as "the channel changed").
        if (blk.clipType != ClipType::Audio || blk.audioFilePath.isEmpty()
            || blk.muted || blk.alignBake)
            continue;
        const int route = (blk.routeChannel != 0)
            ? blk.routeChannel
            : MixerChannelIds::audioInsert (blk.trackRow);
        if (route != channelId) continue;

        // Order-independent (sum of per-block hashes): a block move changes
        // its own term; reordering the block list does not.
        juce::int64 h = (juce::int64) blk.audioFilePath.hashCode64();

        // 2026-07-30 (Jeff): the file's CONTENT IDENTITY, not just its path.
        //
        // Regenerate De-noise rewrites the cleaned take IN PLACE -- same path,
        // same length, same geometry -- so a path-and-geometry hash saw nothing
        // change.  Neither editor re-analyzed: Align kept its old timing map
        // over freshly cleaned audio, and Pitch kept serving a bake made from
        // the PREVIOUS cleaning of that file, indefinitely, until the user
        // happened to hit Analyze.  Silent wrong-audio, and nothing on screen
        // explained it.
        //
        // Modification time AND size: mtime alone can be equal across a fast
        // rewrite on a coarse filesystem clock, and size alone is unchanged by
        // a same-length re-clean, which is exactly this case.  Together they
        // catch it.  Both are cheap stat reads and this runs on a 4 Hz poll,
        // not per block.
        {
            const juce::File f = resolveProjectFile (blk.audioFilePath);
            if (f.existsAsFile())
            {
                h = h * 31 + f.getLastModificationTime().toMilliseconds();
                h = h * 31 + f.getSize();
            }
        }

        h = h * 31 + (juce::int64) std::llround (effectiveStartBeats (blk) * 1000.0);
        h = h * 31 + (juce::int64) std::llround (effectiveLengthBeats (blk) * 1000.0);
        h = h * 31 + blk.contentStartSamples;
        h = h * 31 + (blk.stretchMode ? 1 : 0);
        h = h * 31 + (juce::int64) std::llround ((double) blk.originalBPM * 100.0);
        sig += h;
    }

    // QA-Fa: fold the tempo timeline in -- a tempo change re-maps every
    // clip's beat->sample position, so Align warp maps and Pitch note
    // regions authored under the old tempo are stale even though no clip
    // moved.  Stepped map when published, global tempo otherwise.
    if (TempoMap::isActive())
    {
        juce::int64 t = 0;
        for (int guard = 0; guard < TempoMap::kMaxSegs; ++guard)
        {
            sig = sig * 31 + (juce::int64) std::llround (TempoMap::bpmAtSample (t) * 100.0);
            sig = sig * 31 + t;
            const juce::int64 nb = TempoMap::nextBoundaryAfter (t);
            if (nb < 0) break;
            t = nb;
        }
    }
    else
        sig = sig * 31 + (juce::int64) std::llround (
            juce::jmax (1.0, mPatternManager->getGlobalTempo()) * 100.0);

    return sig;
}

std::vector<std::pair<int, juce::String>> BaySickDAWProcessor::listAudioClipChannels() const
{
    std::vector<std::pair<int, juce::String>> outList;
    if (mPatternManager == nullptr) return outList;

    std::vector<int> seen;
    for (int i = 0; i < mPatternManager->getNumBlocks(); ++i)
    {
        const auto& blk = mPatternManager->getBlock (i);
        if (blk.clipType != ClipType::Audio || blk.audioFilePath.isEmpty()
            || blk.muted || blk.alignBake)
            continue;
        const int route = (blk.routeChannel != 0)
            ? blk.routeChannel
            : MixerChannelIds::audioInsert (blk.trackRow);
        if (std::find (seen.begin(), seen.end(), route) != seen.end())
            continue;
        seen.push_back (route);

        juce::String label;
        if (route >= MixerChannelIds::kVoxBase
            && route < MixerChannelIds::kVoxBase + MixerChannelIds::kMaxVoxStrips)
            label = "Vox " + juce::String (route - MixerChannelIds::kVoxBase + 1);
        else if (route >= MixerChannelIds::kInstBase
                 && route < MixerChannelIds::kInstBase + MixerChannelIds::kMaxInstStrips)
            label = "Inst " + juce::String (route - MixerChannelIds::kInstBase + 1);
        else if (route >= MixerChannelIds::kAudioBase
                 && route < MixerChannelIds::kAudioBase + kMaxAudioRows)
            label = "Clips " + juce::String (route - MixerChannelIds::kAudioBase + 1);
        else
            label = "Channel " + juce::String (route);
        outList.push_back ({ route, label });
    }
    std::sort (outList.begin(), outList.end());
    return outList;
}

// ─────────────────────────────────────────────────────────────────────────────
// D3: Choke-group dispatch (audio thread, wait-free).
// ─────────────────────────────────────────────────────────────────────────────
// Build the per-buffer noteOn list, then for each entry whose source insert
// has chokeGroup G > 0, scan all OTHER inserts (across all 3 engine types)
// and inject a noteOff (per-channel allNotesOff) into their buffers at the
// same sample position so the engines silence before consuming the buffer.
//
// Cost: O(buffers x notes x inserts).  In practice small - typical block has
// 0-3 noteOns; the sweep covers every Layer / Bass / Drum / Vox / Inst page
// slot.
//
// Audio-clip choke is handled separately at clip-start time (Batch 4).
void BaySickDAWProcessor::applyChokeGroupDispatch(
    std::array<juce::MidiBuffer, kMaxLayerPages>& layerMidi,
    std::array<juce::MidiBuffer, kMaxBassPages>&  bassMidi,
    std::array<juce::MidiBuffer, kMaxDrumPages>&  drumMidi,
    std::array<juce::MidiBuffer, kMaxVoxPages>&   voxMidi,
    std::array<juce::MidiBuffer, kMaxInstPages>&  instMidi,
    juce::int64 projectStartSamp,
    int         numSamples,
    double      secPerBeat)
{
    using Kind = BaySickGraph::InsertKind;

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
            const juce::int64 cs = clipBeatToSample (player.clipStartBeat, secPerBeat, mSampleRate);
            const juce::int64 ce = clipBeatToSample (player.clipEndBeat,   secPerBeat, mSampleRate);
            if (projectEnd <= cs || projectStartSamp >= ce)
                player.mutedByChoke = false;
        }
    }

    // ── 2. Build the fires list (synth note-ons + audio clip starts) ────
    // clear() retains capacity, so the common zero-choke-group case costs no
    // allocator traffic at all (see mChokeFireScratch in the header).
    auto& fires = mChokeFireScratch;
    fires.clear();

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
            const juce::int64 cs = clipBeatToSample (p.clipStartBeat, secPerBeat, mSampleRate);
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

void BaySickDAWProcessor::updateDrumMixLevels()
{
    // No-op now that DrumSynth is gone - per-drum-tab levels flow through
    // their mixer strips' faders, applied inside each drum InsertNode.
}

void BaySickDAWProcessor::syncMixerFromPatternManager()
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
void BaySickDAWProcessor::startRecording (RecordMode mode,
                                          double startBeat,
                                          const juce::String& projectName,
                                          const juce::File& samplesFolder)
{
    mRecordMode.store (mode, std::memory_order_relaxed);
    mRecordStartBeat = startBeat;
    // Close the audio thread out of mStripRecorders before mutating it; it is
    // re-opened after the last push_back below.
    mStripTapsLive.store (false, std::memory_order_release);
    mMasterTapLive.store (false, std::memory_order_release);
    // THREAD SAFETY: the gates stop the NEXT block from entering a tap; this
    // proves the block that already passed one has returned before clear()
    // destroys the recorders it is holding.  stopRecording happens to leave the
    // container empty today, which makes the clear look safe by caller ordering
    // -- this makes it safe by construction instead.
    settleAudioThread();
    mStripRecorders.clear();
    mFailedStripArms.clear();
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
            if (! sr.recorder->startRecording (sr.file, mSampleRate, 1))
            {
                // An armed strip must never just vanish from the take results:
                // the commit dialog names every failed capture.
                mFailedStripArms.emplace_back (sr.channelId, sr.displayName);
                continue;
            }

            // I-16 G-9: Vox-only wet recorder + push pointer to the
            // BaySickVocalProcessor for this page so its processBlock taps
            // post-realtime-pitch audio into the wet file.
            // QA-Fb conditional-WET (G2-condWET + Jeff 2026-07-10): realtime
            // pitch bypassed means WET == DRY -- skip the second writer
            // entirely; the take is DRY-only and commitRecordingResult's
            // existing no-wet fallback places the DRY file on the grid.
            // (QA-Fd 3a/12b: the page-master bsv_bypass condition retired
            // with the param.)
            if (isVox && i < kMaxVoxPages)
            {
                BaySickVocalProcessor* vp = nullptr;
                if (auto* eng = mVoxEngines[i])
                    vp = dynamic_cast<BaySickVocalProcessor*> (eng);

                bool rtPitchActive = false;
                if (vp != nullptr)
                    if (auto* byp = vp->apvts.getRawParameterValue ("bsv_pitch_realtime_bypass"))
                        rtPitchActive = byp->load() < 0.5f;

                if (rtPitchActive)
                {
                    sr.wetFile     = samplesFolder.getChildFile (
                        projectName + " - " + sr.displayName + " - " + ts + " - WET.wav");
                    sr.wetRecorder = std::make_unique<AudioFileRecorder>();
                    if (sr.wetRecorder->startRecording (sr.wetFile, mSampleRate, 1))
                        vp->setWetRecorder (sr.wetRecorder.get());
                    else
                    {
                        sr.wetRecorder.reset();   // failed to open -> drop wet recording
                        mFailedStripArms.emplace_back (sr.channelId,
                                                       sr.displayName + " (wet take)");
                    }
                }
            }

            mStripRecorders.push_back (std::move (sr));
        }
    };
    scan ("mixer_vox_",  MixerChannelIds::kMaxVoxStrips,
          MixerChannelIds::kVoxBase,  "Vox",  /*isVox=*/true);
    scan ("mixer_inst_", MixerChannelIds::kMaxInstStrips,
          MixerChannelIds::kInstBase, "Inst", /*isVox=*/false);

    // Container is final -- publish it to the audio thread.  Release pairs with
    // tapDryRecorder's acquire load.
    mStripTapsLive.store (true, std::memory_order_release);

    // No strips armed -> fall back to master output capture.
    // QA-Fe2 PDC: the master tap is post-compensation, so the capture runs
    // totalLatencySamples late vs the beat grid -- trim that many leading
    // samples so the WAV lands on the grid like the strip recorders do.
    if (mStripRecorders.empty())
    {
        auto file = samplesFolder.getChildFile (
            projectName + " - Master - " + ts + ".wav");
        // Gate raised BEFORE the writer opens so no block at the head of the
        // take falls between the two stores: the tap still short-circuits on
        // isRecording(), which AudioFileRecorder sets last.
        mMasterTapLive.store (true, std::memory_order_release);
        if (! mMasterRecorder.startRecording (file, mSampleRate, 2,
                juce::jmax (0, mVibeGraph.totalLatencySamples.load (std::memory_order_relaxed))))
        {
            mMasterTapLive.store (false, std::memory_order_release);
            // Same rule as the armed strips above: a capture whose writer never
            // opened must reach the commit dialog instead of vanishing.  With no
            // strips armed this IS the take, so an unwritable Samples folder
            // would otherwise lose the whole performance in silence.  Channel 0
            // is the report's unknown-channel value; only the name is shown.
            mFailedStripArms.emplace_back (0, "Master");
        }
    }
}

BaySickDAWProcessor::RecordResult BaySickDAWProcessor::stopRecording()
{
    RecordResult out;
    // Close the audio thread out of EVERY tap first: the two container gates
    // here, and the wet pointer each Vox engine holds (I-16 G-9, 2026-05-03).
    mStripTapsLive.store (false, std::memory_order_release);
    mMasterTapLive.store (false, std::memory_order_release);
    for (auto& sr : mStripRecorders)
    {
        if (! sr.wetRecorder) continue;
        const int voxIdx = sr.channelId - MixerChannelIds::kVoxBase;
        if (voxIdx >= 0 && voxIdx < kMaxVoxPages)
            if (auto* eng = mVoxEngines[voxIdx])
                if (auto* vp = dynamic_cast<BaySickVocalProcessor*> (eng))
                    vp->setWetRecorder (nullptr);
    }

    // THREAD SAFETY: the gates above stop the NEXT block from entering a tap;
    // this proves the block that already passed one has returned.  It has to
    // happen before the first AudioFileRecorder::stopRecording below, because
    // that call destroys the ThreadedWriter an in-flight writeBlock is about to
    // dereference (its own mRecording flag is cleared without any fence).
    settleAudioThread();

    out.startBeat = mRecordStartBeat;
    out.midiNotes = mMidiRecorder.stopRecording();
    // QA-Ea Task 0c (FL pre-roll record): drain the pre-roll counter into
    // the result.  exchange(0) leaves the counter clean for the next
    // session so startRecording's defensive zero is belt+suspenders.
    out.preRollSamples = mPreRollSamples.exchange (0, std::memory_order_relaxed);

    // A take whose writer refused blocks reached disk with a splice in it.  The
    // count survives stopRecording (only the next startRecording zeroes it), so
    // it is read after the writer closes.  Only reported for captures that
    // produced a file -- one that produced none is already in failedStrips.
    auto noteDrops = [&out] (const AudioFileRecorder& rec, int channelId,
                             const juce::String& displayName)
    {
        const int dropped = rec.getDroppedBlockCount();
        if (dropped > 0)
            out.droppedTakes.push_back ({ channelId, displayName, dropped });
    };

    if (mMasterRecorder.isRecording())
    {
        out.masterFile = mMasterRecorder.stopRecording();
        // Channel 0 is the report's unknown-channel value, matching the failed
        // master arm in startRecording; only the name is shown.
        if (out.masterFile.existsAsFile())
            noteDrops (mMasterRecorder, 0, "Master");
    }

    for (auto& sr : mStripRecorders)
    {
        if (sr.recorder && sr.recorder->isRecording())
        {
            auto f = sr.recorder->stopRecording();
            if (f.existsAsFile())
            {
                out.stripFiles.emplace_back (sr.channelId, f);
                noteDrops (*sr.recorder, sr.channelId, sr.displayName);
            }
            else
                out.failedStrips.emplace_back (sr.channelId, sr.displayName);
        }
        if (sr.wetRecorder && sr.wetRecorder->isRecording())
        {
            auto f = sr.wetRecorder->stopRecording();
            if (f.existsAsFile())
            {
                out.stripWetFiles.emplace_back (sr.channelId, f);
                noteDrops (*sr.wetRecorder, sr.channelId,
                           sr.displayName + " (wet take)");
            }
            else
                out.failedStrips.emplace_back (sr.channelId,
                                               sr.displayName + " (wet take)");
        }
    }
    out.failedStrips.insert (out.failedStrips.end(),
                             mFailedStripArms.begin(), mFailedStripArms.end());
    mFailedStripArms.clear();
    // The settle at the top already covers the destructors clear() runs -- the
    // taps have been closed since before the first writer was stopped.
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
void BaySickDAWProcessor::tapDryRecorder (int channelId,
                                          const float* monoSource,
                                          int numSamples)
{
    if (monoSource == nullptr || numSamples <= 0) return;
    // An offline render drives this same path: setFreezePrune keeps the target
    // tab's own strip task in the render's keep-set, so freezing a Vox or Inst
    // tab (or any tab a live strip sidechain-keys) executes that strip task on
    // every offline block.  The strip _arm param is persistent, so an armed take
    // in progress would get the render's blocks spliced into its WAV -- silence
    // or one repeated stale block, ahead of the performer's first note.  Same
    // gate the master and capture taps carry.
    if (isNonRealtime()) return;
    // Entry gate: the message thread lowers this before it mutates or destroys
    // mStripRecorders, so the audio thread never walks the container while it
    // is being rebuilt or torn down.
    if (! mStripTapsLive.load (std::memory_order_acquire)) return;

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
void BaySickDAWProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // QA-Ef close (2026-05-23): mirror serializeProject's manual-node-creation
    // for lazily-registered params so the VST3 host-save path matches the
    // standalone save discipline (auxes / sends added mid-session get
    // saved instead of silently dropped on host save).  Without this, lazy
    // params lack a tree node and copyState() below would skip them.  See
    // serializeProject's header comment for the full rationale + the rebind-
    // reset failure mode that motivated this style of fix.
    {
        juce::SortedSet<juce::String> existingIds;
        for (int c = 0; c < apvts.state.getNumChildren(); ++c)
        {
            auto ch = apvts.state.getChild (c);
            if (ch.hasProperty ("id"))
                existingIds.add (ch["id"].toString());
        }
        for (auto* param : getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
            if (ranged == nullptr) continue;
            if (existingIds.contains (ranged->paramID)) continue;
            juce::ValueTree node ("PARAM");
            node.setProperty ("id", ranged->paramID, nullptr);
            node.setProperty ("value",
                (double) ranged->getNormalisableRange().convertFrom0to1 (ranged->getValue()),
                nullptr);
            apvts.state.appendChild (node, nullptr);
        }
    }

    auto state = apvts.copyState();

    // Always rebuild the rack states child from scratch (avoid stale duplicate).
    state.removeChild(state.getChildWithName("BaySickRackStates"), nullptr);
    juce::ValueTree rackStates("BaySickRackStates");
    mVibeGraph.saveRackStates(rackStates);
    state.addChild(rackStates, -1, nullptr);

    // I-3b (2026-05-02): MIDI Learn registry persistence.  Per-project mapping
    // table sits under <MidiCCMappings> as a child of the saved state.  Load
    // path mirrors -- removeChild before APVTS replaceState, then restore.
    state.removeChild(state.getChildWithName(MidiLearnRegistry::kRootTag), nullptr);
    state.addChild(mMidiLearn.saveToValueTree(), -1, nullptr);

    // QA-L-Fix (D-14): per-drum kit trigger bindings save WITH the project --
    // they're part of the kit setup, like each drum's play note.
    state.removeChild(state.getChildWithName(DrumTriggerMap::kRootTag), nullptr);
    state.addChild(mDrumTriggers.saveToValueTree(), -1, nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void BaySickDAWProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(SafeXml::parseBinaryBlob (data, sizeInBytes));
    if (!xmlState) return;

    auto state = juce::ValueTree::fromXml(*xmlState);
    if (!state.isValid()) return;

    // QA-Ef close (2026-05-23): mirror deserializeProject's project-load shield
    // here so the VST3 host-load path inherits the same correctness floor.  The
    // standalone never calls this entry (it goes through deserializeProject
    // directly); JUCE's host wrapper does on plugin instantiation / project
    // reload.  The legacy juce_add_plugin target still compiles this path even
    // though it's not currently shipped -- leaving it unshielded would silently
    // re-open the rebuild-race + aux-leak bugs on any future re-enable.
    // Nest-aware via setProjectLoadInProgress(true/false); processBlock bails to
    // silence while the shield is up.
    setProjectLoadInProgress (true);
    settleAudioThread();

    // QA-Ef close (2026-05-23): tear down every aux insert from the PRIOR plugin
    // session before the new project's params load and restoreAuxStripsFromState
    // rebuilds.  Mirrors deserializeProject; runs UNDER THE SHIELD so the audio
    // thread is bailing while we mutate the render-task list, and BEFORE
    // replaceState + restoreAuxStripsFromState so we don't wipe the just-restored
    // state.
    clearAllAuxInserts();

    // Extract and apply rack states before passing to APVTS (keeps APVTS tree clean).
    auto rackStates = state.getChildWithName("BaySickRackStates");
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

    // QA-L-Fix (D-14): drum kit trigger bindings.  Unlike the MIDI Learn
    // registry there are no global defaults to fall back on, so a project
    // without the child clears rather than inheriting the last project's kit.
    auto drumTrigs = state.getChildWithName(DrumTriggerMap::kRootTag);
    state.removeChild(drumTrigs, nullptr);
    if (drumTrigs.isValid()) mDrumTriggers.loadFromValueTree(drumTrigs);
    else                     mDrumTriggers.clearAll();

    // QA-Ef #4 (2026-05-22): deep-copy the saved tree BEFORE replaceState, so
    // the aux-restore scan below isn't fooled by stale empty <PARAM> nodes
    // that replaceState's rebind appends for params registered in a prior
    // session but absent from this file (see restoreAuxStripsFromState
    // header comment).
    auto savedFileSnapshot = state.createCopy();

    if (state.hasType(apvts.state.getType()))
        apvts.replaceState(state);

    // 5F-4b B7: re-register any aux strips that were in the saved project.
    // Scans the pre-rebind file snapshot so only auxes ACTUALLY saved in the
    // file get restored.
    restoreAuxStripsFromState (savedFileSnapshot);

    // QA-Ef close (2026-05-23): rebuild complete -- lower the shield so audio
    // resumes (mirror deserializeProject's tail).
    setProjectLoadInProgress (false);
}

// ── Project persistence (P1, 2026-04-23) ────────────────────────────────────
// These methods produce / consume a full project snapshot including everything
// getStateInformation covers (APVTS + rack states) plus the PatternManager tree
// that today has no disk path.  The shape:
//
//   <BaySickDAWProject version="1">
//     <Processor>
//       <APVTSState>...</APVTSState>          ← APVTS + BaySickRackStates child
//     </Processor>
//     <PatternManager version="1">
//       <Patterns>...</Patterns>
//       <Arrangement>...</Arrangement>
//       <AudioLibrary>...</AudioLibrary>
//       <AutomationTemplates>...</AutomationTemplates>
//       (etc.)
//     </PatternManager>
//     <DenoiseProfiles>...</DenoiseProfiles>
//     <MidiCCMappings>...</MidiCCMappings>    -- per-project MIDI Learn overlay
//     <DrumTriggers>...</DrumTriggers>
//   </BaySickDAWProject>
//
// ProjectManager writes this to <projectFolder>/project.xml.  The legacy
// getStateInformation/setStateInformation blob format is kept untouched so any
// prior state files still load (APVTS-only, no pattern content).
void BaySickDAWProcessor::serializeProject (juce::XmlElement& root)
{
    root.setAttribute ("version", 1);

    writeProcessorState (root);

    // PatternManager - patterns, arrangement, piano-roll notes, libraries,
    // row mute/solo, drum-enabled flags, full mixer snapshot.
    if (mPatternManager != nullptr)
    {
        auto pmTree = mPatternManager->toValueTree();
        if (auto pmXml = pmTree.createXml())
            root.addChildElement (pmXml.release());
    }

    // QA-Fe2: De-noise profiles (raw + corrector domain per recording base
    // name) -- standalone-only project data, same placement rule as
    // PatternManager (not in the VST blob).
    if (! mDenoiseProfiles.empty())
    {
        auto* dn = root.createNewChildElement ("DenoiseProfiles");
        for (const auto& [base, pair] : mDenoiseProfiles)
        {
            auto* e = dn->createNewChildElement ("Profile");
            e->setAttribute ("base", base);
            if (pair.first.isValid())  e->setAttribute ("raw", pair.first.toBase64());
            if (pair.second.isValid()) e->setAttribute ("wet", pair.second.toBase64());
        }
    }

    // MIDI Learn CC mappings and per-drum trigger bindings.  Both live here and
    // NOT in writeProcessorState: that block is shared with template save, and a
    // template must not carry one machine's controller bindings.  Same
    // standalone-only placement rule as DenoiseProfiles above.
    if (auto midiXml = mMidiLearn.saveToValueTree().createXml())
        root.addChildElement (midiXml.release());

    if (auto drumXml = mDrumTriggers.saveToValueTree().createXml())
        root.addChildElement (drumXml.release());

    // P1+P2 persistence (2026-04-24): let StandaloneEditor append its tab +
    // engine state under a <UIState> child.  Callback is null in plugin /
    // headless contexts - that's fine; the project just omits UI state.
    if (onSerializeUIState)
        onSerializeUIState (root);
}

// QA-ProjectSave Task 2 (2026-07-26, docket 15=B): the <Processor> child on its
// own, split out of serializeProject so template save emits the identical block.
//
// This is what carries every mixer strip's fader / pan / width / mute / solo /
// polarity / routing + sends, plus each insert's effect rack and post-rack EQ.
// None of it lives in the per-tab engineData under <UIState> -- that captures
// only each engine processor's own state -- so a template without this restores
// tabs and engines onto a defaulted mixer.
void BaySickDAWProcessor::writeProcessorState (juce::XmlElement& root)
{
    // QA-Ef (2026-05-22, refined after the 100->-1 reset was caught on the
    // Save Diag): for every registered APVTS param that lacks a tree node in
    // apvts.state, manually append a <PARAM> child with its CURRENT live value
    // already set.  This materializes nodes for lazy params (aux strips added
    // mid-session, bus sends touched for the first time, etc.) so they get
    // saved -- the original purpose of fix #3 -- WITHOUT the destructive side-
    // effect of apvts.replaceState(apvts.copyState()).
    //
    // That earlier rebind's internal appendChild creates an EMPTY node (id
    // only, no "value" property) for any adapter that lacks a tree, and the
    // resulting valueTreeChildAdded fires setNewState
    // (juce_AudioProcessorValueTreeState.cpp:417-421 + :438-442), which reads
    // the node's missing value as the param's DEFAULT and RESETS the live
    // param.  Observed via Save Diag: mixer_layers_send0_to live=100 before
    // rebind, =-1 after, written=-1 on the FIRST save after drawing the cable.
    // (The SECOND save worked because the first save's rebind had then
    // materialized the node with value=-1; the re-drawn cable wrote 100 into
    // the existing node and the next rebind preserved 100.)
    //
    // Below: pre-setting "value" before appendChild makes setNewState read the
    // CURRENT value -- so setDenormalisedValue is a no-op and the param keeps
    // its current value.  Same listener chain, correct outcome.
    {
        juce::SortedSet<juce::String> existingIds;
        for (int c = 0; c < apvts.state.getNumChildren(); ++c)
        {
            auto ch = apvts.state.getChild (c);
            if (ch.hasProperty ("id"))
                existingIds.add (ch["id"].toString());
        }
        for (auto* param : getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
            if (ranged == nullptr) continue;
            if (existingIds.contains (ranged->paramID)) continue;
            juce::ValueTree node ("PARAM");
            node.setProperty ("id", ranged->paramID, nullptr);
            node.setProperty ("value",
                (double) ranged->getNormalisableRange().convertFrom0to1 (ranged->getValue()),
                nullptr);
            apvts.state.appendChild (node, nullptr);
        }
    }

    // Processor state (APVTS + rack states) - reuse the same ValueTree we
    // produce in getStateInformation, but emit as XML child instead of a
    // MemoryBlock blob.  copyState() flushes any pending live values to
    // existing nodes before snapshotting.
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("BaySickRackStates"), nullptr);
    juce::ValueTree rackStates ("BaySickRackStates");
    mVibeGraph.saveRackStates (rackStates);
    state.addChild (rackStates, -1, nullptr);

    auto* processor = root.createNewChildElement ("Processor");
    if (auto stateXml = state.createXml())
        processor->addChildElement (stateXml.release());
}

void BaySickDAWProcessor::applyPendingRackStates()
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

void BaySickDAWProcessor::resetToBlankState()
{
    // Project boundary: a clip that failed in the OLD project must report
    // again if the next project references it too.
    mReportedUnreadableClips.clear();

    // Same boundary rule: profile keys are prefixed with the project name of the
    // take they were learned from, so a profile carried across File > New or a
    // template apply can never match a take in the new project -- it would just
    // ride along in that project's XML forever.  (The open path clears these in
    // deserializeProject before repopulating; New and template apply do not go
    // through it.)
    mDenoiseProfiles.clear();

    // Same boundary rule again, and the same reason deserializeProject clears
    // this when a project carries no node: bindings are keyed on drum page
    // index, there are no global defaults behind them, and File > New hands the
    // new project's first drum index 0 -- so an inherited binding lands on a
    // drum the user never bound, fires from their pad, and demuxes recorded MIDI
    // into the wrong lane.  serializeProject writes the node unconditionally, so
    // one File > New makes the leak permanent project data.
    mDrumTriggers.clearAll();

    // Reset every registered APVTS param to its default value.  Iterate via
    // getParameters() so lazy-registered engine / mixer-strip / rack params
    // all get swept regardless of when they were added.
    {
        // QA-UndoCoverage Task 6: File > New / Open default sweep is a load
        // boundary -- never history.
        juce::AudioProcessorValueTreeState::ScopedProgrammaticParamWrites spw;
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
    }

    // Clear every rack slot across buses + inserts so File > New / File >
    // Open never bleeds the previous session's effect chains through.
    // loadRackStates only RESTORES from the tree - with no matching entries
    // it wouldn't wipe pre-existing racks, which was the observed bug.
    mVibeGraph.clearAllRackStates();

    // The EQ re-seed and PatternManager back to one empty default pattern.
    //
    // THREAD SAFETY: reset() clears mPatterns AND mArrangement, and the audio
    // thread walks both bare -- the song-mode arrangement scheduler and the
    // automation-clip loop iterate getBlock(), and the step trigger + metronome
    // hold a reference from currentPattern() across their reads.  The EQ sweep
    // reallocates linear-phase processors and writes coefficients the same
    // thread reads.  Shield -> settle -> mutate -> restore is the established
    // teardown idiom here: processBlock bails to silence at its top while the
    // shield is up, and the settle proves the in-flight block returned before
    // anything is freed.  Save/restore keeps an outer load's shield intact and
    // skips a second settle, so File > Open (already shielded) pays nothing; a
    // bare File > New pays two audio blocks of silence, which is the accepted
    // trade.  One region for both so New pays that once.
    {
        const bool shieldWasUp = isProjectLoadInProgress();
        setProjectLoadInProgress (true);
        if (! shieldWasUp) settleAudioThread();

        resetEqStatesToDefaults();
        if (mPatternManager) mPatternManager->reset();

        setProjectLoadInProgress (shieldWasUp);
    }
}

void BaySickDAWProcessor::setCurrentProjectFolder (const juce::File& folder)
{
    {
        juce::ScopedLock sl (mProjectFolderLock);
        mCurrentProjectFolder = folder;
    }

    // Publishing here rather than at construction is what guarantees the
    // resolver is live before the first engine reads a stored reference:
    // ProjectManager calls this ahead of deserializeProject.  Engines and DSP
    // classes reach the resolver through the free function because most are
    // built by the EffectRack factory, which holds no processor reference.
    // Installed OUTSIDE mProjectFolderLock: resolve() calls back into
    // resolveProjectFile, which takes that same lock.
    ProjectFileResolver::install ([this] (const juce::String& stored)
                                  { return resolveProjectFile (stored); });
}

juce::File BaySickDAWProcessor::getCurrentProjectFolder() const
{
    juce::ScopedLock sl (mProjectFolderLock);
    return mCurrentProjectFolder;
}

juce::File BaySickDAWProcessor::resolveProjectFile (const juce::String& storedPath) const
{
    if (storedPath.isEmpty()) return {};
    // QA-ProjectSave Task 4 (2026-07-26): stable-root references resolve FIRST,
    // ahead of the absolute test -- "library:..." / "mysamples:..." are not
    // absolute paths and would otherwise fall through to the project-relative
    // branch and resolve to nonsense inside the project folder.
    if (SampleLibrary::isStableRef (storedPath))
        return SampleLibrary::resolveStableRef (storedPath);
    // Absolute paths pass through (pre-P4 projects stored absolute audio paths).
    if (juce::File::isAbsolutePath (storedPath)) return juce::File (storedPath);
    // Relative paths - resolve against current project folder.
    juce::ScopedLock sl (mProjectFolderLock);
    if (mCurrentProjectFolder == juce::File()) return {};
    return mCurrentProjectFolder.getChildFile (storedPath);
}

void BaySickDAWProcessor::deserializeProject (const juce::XmlElement& root)
{
    // QA-Ef (2026-05-22): raise the project-load shield across the WHOLE load,
    // not just the teardown half.  restoreAuxStripsFromState (below) + the
    // editor's tab/engine rebuild (fired via onDeserializeUIState) both call
    // registerTask, and the audio thread walks the render task list every
    // block; without the shield up across the rebuild a concurrent registerTask
    // races that iteration.  closeAllDynamicTabs (inside the UI rebuild) is
    // nest-aware and leaves the shield raised while we hold it.  The settle
    // drains any in-flight processBlock before we touch the graph, at whatever
    // the device buffer size actually is.  processBlock bails to silence while
    // the shield is up, so audio stays quiet for the brief load instead of
    // rendering a half-built graph.
    setProjectLoadInProgress (true);
    settleAudioThread();

    if (onLoadProgress) onLoadProgress ("Reading project state...");

    // QA-Ef #4 (2026-05-22): tear down every aux insert (engine + render task)
    // from the PRIOR project before the new project's params load and
    // restoreAuxStripsFromState rebuilds.  Without this, prior-project auxes
    // leak across loads ("open 16 auxes, load another project, all 16 still
    // there").  Must run UNDER THE SHIELD (above) so the audio thread is
    // bailing while we mutate the render-task list, and must run BEFORE
    // replaceState + restoreAuxStripsFromState so we don't wipe the just-
    // restored state.
    clearAllAuxInserts();

    applyProcessorState (root);

    if (onLoadProgress) onLoadProgress ("Restoring patterns...");

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

    // QA-Fe2: De-noise profiles.  Cleared unconditionally so a project
    // without the node never inherits the previous project's rooms.
    mDenoiseProfiles.clear();
    if (auto* dn = root.getChildByName ("DenoiseProfiles"))
        for (auto* e : dn->getChildWithTagNameIterator ("Profile"))
        {
            const juce::String base = e->getStringAttribute ("base");
            if (base.isEmpty()) continue;
            mDenoiseProfiles[base] = {
                DenoiseProfile::fromBase64 (e->getStringAttribute ("raw")),
                DenoiseProfile::fromBase64 (e->getStringAttribute ("wet")) };
        }

    // MIDI Learn mappings, mirroring setStateInformation's semantics: a project
    // with no node keeps whatever loadGlobalDefaults seeded at launch, so the
    // per-project table OVERLAYS the globals rather than replacing them.  Older
    // projects have no node and therefore behave exactly as before.
    if (auto* midiXml = root.getChildByName (MidiLearnRegistry::kRootTag))
        mMidiLearn.loadFromValueTree (juce::ValueTree::fromXml (*midiXml));

    // Drum trigger bindings, also mirroring setStateInformation: there are no
    // global defaults to fall back on, so a project without the node CLEARS --
    // otherwise project B inherits project A's kit against project B's tabs.
    if (auto* drumXml = root.getChildByName (DrumTriggerMap::kRootTag))
        mDrumTriggers.loadFromValueTree (juce::ValueTree::fromXml (*drumXml));
    else
        mDrumTriggers.clearAll();

    // P1+P2 persistence: fire after main state is loaded so the editor's
    // engine-processor creation can inherit any APVTS-driven defaults.
    if (onDeserializeUIState)
        onDeserializeUIState (root);

    // QA-Ef (2026-05-22): rebuild complete -- lower the shield so audio resumes.
    setProjectLoadInProgress (false);

    reportMissingFilesIfAny();
}

// QA-ProjectSave Task 3 (2026-07-26, docket 15=B): the <Processor> apply on its
// own, split out of deserializeProject so template load restores the identical
// state through the identical path.  Caller owns the project-load shield and
// must have already run clearAllAuxInserts().
//
// Pairs with writeProcessorState.  Per-insert rack states are STASHED here and
// replayed by applyPendingRackStates once the caller has rebuilt tabs + strips,
// because the InsertNodes they target do not exist yet at this point.
void BaySickDAWProcessor::applyProcessorState (const juce::XmlElement& root)
{
    // Processor state - first child under <Processor>.
    if (auto* processor = root.getChildByName ("Processor"))
    {
        // The saved APVTS tree is the single child of <Processor>.
        for (auto* child : processor->getChildIterator())
        {
            auto state = juce::ValueTree::fromXml (*child);
            if (! state.isValid()) continue;

            auto rackStates = state.getChildWithName ("BaySickRackStates");
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

            // QA-Ef #4 (2026-05-22): deep-copy BEFORE replaceState so the
            // aux-restore scan below isn't fooled by stale empty <PARAM> nodes
            // that the rebind appends for prior-session-registered params
            // missing from this file (e.g. open AT1 with 3 auxes then open
            // AT2 with 1 -- without this, mixer_aux_1/2 phantom nodes would
            // make us re-create the deleted auxes).  See
            // restoreAuxStripsFromState header for the full failure mode.
            auto savedFileSnapshot = state.createCopy();

            if (state.hasType (apvts.state.getType()))
                apvts.replaceState (state);

            // Aux strips follow from the FILE's param presence (not apvts.state,
            // which now has rebind-created stale nodes).  Same path as
            // setStateInformation.
            restoreAuxStripsFromState (savedFileSnapshot);
            break;   // only one APVTS state child expected
        }
    }
}

// QA-Export Task 5: engines that could not find an external file (NAM capture,
// sfizz kit) recorded it rather than skipping in silence.  Reported once, after
// every engine has finished restoring -- a warning per engine would mean a
// dialog stack on a project with several gone missing.
// QA-ProjectSave Task 3 (2026-07-26): shared with template load, whose engines
// carry the same external references.
//
// A forwarder, not a gate: MissingFileReport is a header-only namespace with no
// processor dependency, so any surface can drain.  See the header decl for when
// a bare call is right and when a ScopedGesture is.
void BaySickDAWProcessor::reportMissingFilesIfAny (const juce::String& sourceNoun)
{
    MissingFileReport::reportIfAny (sourceNoun);
}

// 5F-4b B7 / QA-Ef #4 (2026-05-22): scan a saved-file state tree for
// mixer_aux_N params and re-register their InsertNodes + APVTS params so the
// audio path and UI can pick them up.  IMPORTANT: sourceState must be a deep
// copy of the loaded tree taken BEFORE apvts.replaceState -- see the header
// decl comment for the phantom-aux failure mode if we scan apvts.state.
void BaySickDAWProcessor::restoreAuxStripsFromState (const juce::ValueTree& sourceState)
{
    for (int idx = 0; idx < MixerChannelIds::kMaxAuxStrips; ++idx)
    {
        const juce::String testId = "mixer_aux_" + juce::String(idx) + "_level";

        // Check if this param exists in the saved state tree.
        // APVTS stores params as children with property "id".
        bool found = false;
        for (int c = 0; c < sourceState.getNumChildren(); ++c)
        {
            auto child = sourceState.getChild(c);
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
juce::AudioProcessorEditor* BaySickDAWProcessor::createEditor()
{
    return new BaySickDAWPluginEditor(*this);
}
#else
juce::AudioProcessorEditor* BaySickDAWProcessor::createEditor()
{
    return nullptr; // Standalone uses StandaloneEditor, not AudioProcessorEditor
}
#endif

// Required by JUCE VST3 plugin hosting
#ifdef VIBESYNTH_VST
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BaySickDAWProcessor();
}
#endif

// ── Lazy APVTS registration ───────────────────────────────────────────────────
// Mixer-strip params are registered on demand when a strip is first created
// (ensureMixerStripParams).
// JUCE has no remove API -- param objects remain in the APVTS tree forever -- so
// every dyn* helper skips an id APVTS already holds; that check, not the
// mRegisteredTrackParams id accumulator, is what makes re-registration safe.
//
// EQ naming: <mixerPrefix>_{mid|side}_eq{b}{Suffix} -- Suffix is CamelCase with
// no separating underscore (Freq/Gain/Q/Type/On/Slope/Mute/Solo/Channel plus the
// Dynamic block).  The pre-rack bank inserts "preeq_" before {mid|side}.

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

// QA-ApvtsAutomation (2026-07-25): addParamsForHarmless / addParamsForBaySickPlayer
// / addParamsForBaySickSynth / addParamsForBaySickBass / addParamsForEffectRack
// removed with registerParamsForTrack.  (The player one was spelled
// addParamsForVibePlayer when it existed; renamed here to match the tree.)  They registered a "tk_{trackId}_*" mirror
// of the engine param sets that nothing ever read: the ids were mismatched with
// the engine-tagged ids the editors actually stamp ("tk_lay_0_bss_noise" vs this
// set's "tk_lay_0_oscMode"), so the family was automatable-to-nowhere.  Engine
// params now reach automation through the applicator registry instead.
// (2026-04-25: addParamsForBaySickDrums had already gone with the legacy drum
// processor.)

void BaySickDAWProcessor::addParamsForTrackEQ(const juce::String& prefix)
{
    // Post-rack EQ (existing behavior - IDs at prefix + "_mid_eq{b}{Suffix}").
    addParamsForEQBank(prefix, juce::String());
}

// §P4.3: Pre-rack EQ.  IDs at prefix + "_preeq_mid_eq{b}{Suffix}" so the
// post-rack and pre-rack banks coexist on the same strip without collision.
void BaySickDAWProcessor::addParamsForTrackPreEQ(const juce::String& prefix)
{
    addParamsForEQBank(prefix, "preeq_");
}

// Internal helper - registers an 8-band M/S EQ param bank under prefix +
// "_" + subPrefix + "{mid|side}_eq{b}{Suffix}".
//   subPrefix ""        → post-rack ("EQ" labels)
//   subPrefix "preeq_"  → pre-rack  ("Pre EQ" labels - disambiguates automation menus)
void BaySickDAWProcessor::addParamsForEQBank(const juce::String& prefix,
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

// Maps a mixer-strip prefix onto its EQ cache strip slot: buses first in
// kEqBuses order, then the insert families in kEqInsertFamilies order.  Exact
// bus match runs FIRST so "mixer_bass" resolves to the bus rather than falling
// into the "mixer_bass_" insert family.  Returns -1 for any prefix that owns no
// EQ bank the sweep would ever visit.
int BaySickDAWProcessor::eqStripSlotForPrefix (const juce::String& prefix) noexcept
{
    for (int i = 0; i < kEqNumBusSlots; ++i)
        if (prefix == kEqBuses[i].prefix)
            return i;

    int base = kEqNumBusSlots;
    for (const auto& fam : kEqInsertFamilies)
    {
        const juce::String famBase (fam.prefixBase);
        if (prefix.startsWith (famBase))
        {
            const juce::String tail = prefix.substring (famBase.length());
            if (tail.isNotEmpty() && tail.containsOnly ("0123456789"))
            {
                const int idx = tail.getIntValue();
                if (idx >= 0 && idx < fam.count)
                    return base + idx;
            }
            return -1;
        }
        base += fam.count;
    }
    return -1;
}

// The ONE spelling every EQ-band CONSUMER composes from.  addParamsForEQBank is
// the registration source of truth and builds the same strings inline; anything
// that later looks a band's params up must go through here, because an APVTS
// lookup is an exact string compare that returns null on a miss and reports
// nothing -- a divergent copy is a silent no-op, not an error.
namespace EqBandIds
{
    // Index order must match BaySickDAWProcessor::EqBandParamSlot.
    static const char* const kSuffixes[] = {
        "Freq", "Gain", "Q", "Type", "On", "Slope", "Mute", "Solo", "Channel",
        "Dynamic", "Threshold", "Ratio", "Attack", "Release", "Range", "Upward", "ScSource"
    };
    static const char* const kSides[]    = { "mid", "side" };
    static const char* const kBankSubs[] = { "", "preeq_" };

    // e.g. ("mixer_rusty_3", pre, side, 2) -> "mixer_rusty_3_preeq_side_eq2"
    static juce::String bandPrefix (const juce::String& stripPrefix,
                                    int bank, int side, int band)
    {
        return stripPrefix + "_" + kBankSubs[bank] + kSides[side]
                 + "_eq" + juce::String (band);
    }
}

// THREAD SAFETY: the message-thread half of the EQ param-pointer cache (see the
// mEqParamCache declaration for the full contract).  Runs once per strip, right
// after addParamsForEQBank has created the ids, so the pointers it resolves are
// the ones that bank just registered.
void BaySickDAWProcessor::cacheEqParamPointers (const juce::String& prefix)
{
    const int stripSlot = eqStripSlotForPrefix (prefix);
    if (stripSlot < 0) return;

    static_assert ((int) (sizeof (EqBandIds::kSuffixes) / sizeof (EqBandIds::kSuffixes[0]))
                       == (int) eqNumBandParamSlots,
                   "EqBandIds::kSuffixes must carry one entry per EqBandParamSlot");

    for (int bank = 0; bank < kEqBanksPerStrip; ++bank)
        for (int side = 0; side < kEqSidesPerBank; ++side)
            for (int b = 0; b < kEqBands; ++b)
            {
                const juce::String bp = EqBandIds::bandPrefix (prefix, bank, side, b);
                auto& slots = mEqParamCache[(size_t) eqCacheIndex (stripSlot, bank, side, b)];

                // Freq LAST and with release: it is the band's publication flag,
                // so the audio thread's acquire-load of it makes these sixteen
                // relaxed stores visible before it can act on any of them.
                for (int s = eqSlotFreq + 1; s < eqNumBandParamSlots; ++s)
                    slots.p[s].store (apvts.getRawParameterValue (bp + EqBandIds::kSuffixes[s]),
                                      std::memory_order_relaxed);
                slots.p[eqSlotFreq].store (
                    apvts.getRawParameterValue (bp + EqBandIds::kSuffixes[eqSlotFreq]),
                    std::memory_order_release);
            }
}

// ── QA-ModelShell TS2: offline render drive ──────────────────────────────────
bool BaySickDAWProcessor::beginOfflineRender (double renderSampleRate, int renderBlockSize)
{
    if (renderSampleRate <= 0.0 || renderBlockSize <= 0) return false;

    // ONE render at a time, whoever asks: export/measure call this from their
    // background thread while freeze renders call it on the message thread.
    // Two interleaved suspend/restore sequences corrupt both renders AND the
    // restore set, so the loser fails cleanly instead.
    bool expected = false;
    if (! mOfflineRenderActive.compare_exchange_strong (expected, true))
        return false;

    // Device callbacks stop reaching processBlock (the standalone player
    // checks isSuspended and outputs silence); the render loop becomes the
    // only caller.  One settle outlasts any in-flight device block.
    suspendProcessing (true);
    juce::Thread::sleep (30);

    mOfflinePrevSr   = getSampleRate();
    mOfflinePrevBlk  = getBlockSize();
    mOfflinePrevSong = isSongMode();
    mOfflinePrevHead = getPlayHead();

    // A freeze re-render during project load arrives with the load shield
    // raised, and processBlock's shield bail would clear every block of the
    // render (the offline loop calls processBlock directly, so the suspension
    // above does not gate it).  Dropping the shield here is safe because the
    // device is already suspended -- the render loop is the only caller left
    // -- and endOfflineRender restores it before the device resumes.
    mOfflinePrevShield = isProjectLoadInProgress();
    setProjectLoadInProgress (false);

    auto sweepNonRealtime = [this] (bool offline)
    {
        setNonRealtime (offline);
        mEngineRig->forEachEngine ([offline] (juce::AudioProcessor& p)
        {
            p.setNonRealtime (offline);
            // The vocal's embedded NAM/IR is not a rig-owned stage.
            if (auto* v = dynamic_cast<BaySickVocalProcessor*> (&p))
                v->getNamIrProcessor().setNonRealtime (offline);
        });
        for (int i = 0; i < (int) kMaxInstPages; ++i)
        {
            if (auto* g = mGuitarsEngine[(size_t) i].get()) g->setNonRealtime (offline);
            if (auto* b = mBassesEngine[(size_t) i].get())  b->setNonRealtime (offline);
        }
        if (mRustyDrumsEngine) mRustyDrumsEngine->setNonRealtime (offline);
        // Rack slots last: a hosted plugin in a mixer rack is a DSPBase inside
        // an EffectRack, not a rig engine, so nothing above reaches it.
        mVibeGraph.setAllRackSlotsNonRealtime (offline);
    };
    sweepNonRealtime (true);

    // CL-282: clip streamers switch to synchronous blocking reads (a fast
    // render outruns the background prefetch by design); counter reset so
    // endOfflineRender's report proves the export had zero silent gaps.
    AudioClipStreamer::sUnderrunCount.store (0, std::memory_order_relaxed);
    AudioClipStreamer::sPeakPrefillMs.store (0.0f, std::memory_order_relaxed);
    AudioClipStreamer::sOfflineRender.store (true, std::memory_order_release);

    // Wet-tail hygiene: the render must not open on live reverb/delay tails.
    mVibeGraph.reset();

    // Full re-prepare at the render config -- prepareToPlay sweeps every
    // engine + the graph, so the render rate is independent of the device's.
    mOfflineReconfigureThread.store (juce::Thread::getCurrentThreadId(),
                                     std::memory_order_release);
    prepareToPlay (renderSampleRate, renderBlockSize);
    mOfflineReconfigureThread.store (nullptr, std::memory_order_release);
    return true;
}

void BaySickDAWProcessor::endOfflineRender()
{
    // Reverse of begin: device config back, flags off, the render's own
    // tails cleared so they never bleed into live playback, playhead + mode
    // restored, device resumed.

    // A stored config, not an open device: juce::AudioProcessor keeps reporting
    // the last negotiated rate after releaseResources, so this test says only
    // whether there is something to restore.
    if (mOfflinePrevSr > 0.0 && mOfflinePrevBlk > 0)
    {
        mOfflineReconfigureThread.store (juce::Thread::getCurrentThreadId(),
                                         std::memory_order_release);
        prepareToPlay (mOfflinePrevSr, mOfflinePrevBlk);
        mOfflineReconfigureThread.store (nullptr, std::memory_order_release);
    }

    // The render was a consumer and prepareToPlay cleared the idle assertion for
    // it; now that it has finished, the device is the only consumer left, and
    // RetirementQueue's CONSUMER-IDLE CONTRACT wants the assertion back when
    // there is no device -- otherwise nothing retired is ever freed.
    // mAudioDevicePrepared is the truthful test for that (the offline prepares
    // above are gated out of it).
    setRetirementConsumersIdle (! mAudioDevicePrepared.load (std::memory_order_acquire));

    setNonRealtime (false);
    mEngineRig->forEachEngine ([] (juce::AudioProcessor& p)
    {
        p.setNonRealtime (false);
        if (auto* v = dynamic_cast<BaySickVocalProcessor*> (&p))
            v->getNamIrProcessor().setNonRealtime (false);
    });
    for (int i = 0; i < (int) kMaxInstPages; ++i)
    {
        if (auto* g = mGuitarsEngine[(size_t) i].get()) g->setNonRealtime (false);
        if (auto* b = mBassesEngine[(size_t) i].get())  b->setNonRealtime (false);
    }
    if (mRustyDrumsEngine) mRustyDrumsEngine->setNonRealtime (false);
    mVibeGraph.setAllRackSlotsNonRealtime (false);

    // CL-282: back to live streaming; report the render's underrun count
    // (expected 0 -- any other number means a silent gap got printed).
    AudioClipStreamer::sOfflineRender.store (false, std::memory_order_release);
    DBG ("[TS2 EXPORT] clip-stream underruns this render: "
         << AudioClipStreamer::sUnderrunCount.load (std::memory_order_relaxed));

    mVibeGraph.reset();

    setPlayHead (mOfflinePrevHead);
    setSongMode (mOfflinePrevSong);

    // The engine-side pointer must not keep naming the render's DEAD STACK
    // head: enginePlayHead() seeds every engine created between now and the
    // next live block, and the per-block change-gate only reaches engines that
    // already exist.
    mLastEnginePlayHead = mOfflinePrevHead;
    if (mEngineRig != nullptr)
        mEngineRig->forEachEngine ([this] (juce::AudioProcessor& p)
                                   { p.setPlayHead (mOfflinePrevHead); });
    for (auto& g : mGuitarsEngine) if (g) g->setPlayHead (mOfflinePrevHead);
    for (auto& b : mBassesEngine)  if (b) b->setPlayHead (mOfflinePrevHead);
    if (mRustyDrumsEngine) mRustyDrumsEngine->setPlayHead (mOfflinePrevHead);

    mOfflinePrevHead = nullptr;

    // Shield back up (if it was) BEFORE the device resumes, so no live block
    // runs against a project still mid-load.
    setProjectLoadInProgress (mOfflinePrevShield);
    mOfflinePrevShield = false;

    suspendProcessing (false);
    mOfflineRenderActive.store (false, std::memory_order_release);
}

// ── Engine processor registration ────────────────────────────────────────────
// See the declaration for why the binding lives here and is never undone.
void BaySickDAWProcessor::bindSampleLoadShield (juce::AudioProcessor* eng) noexcept
{
    if (auto* player = dynamic_cast<BaySickPlayerProcessor*> (eng))
        player->setHostProcessor (this);
}

void BaySickDAWProcessor::registerLayerEngine(int idx, juce::AudioProcessor* eng)
{
    bindSampleLoadShield (eng);

    {
        juce::SpinLock::ScopedLockType lk(mLayerEngineLock);
        if (idx >= 0 && idx < kMaxLayerPages) mLayerEngines[idx] = eng;
    }
    // 5F-4a: ensure mixer strip params + Layer InsertNode exist for this page.
    // Message thread only - safe to call APVTS and BaySickGraph.
    if (idx >= 0 && idx < kMaxLayerPages && eng != nullptr)
    {
        const juce::String prefix = "mixer_layer_" + juce::String(idx);
        ensureMixerStripParams(prefix, MixerStripKind::Insert, MixerChannelIds::kLayersBus);
        mVibeGraph.ensureInsertNode(BaySickGraph::InsertKind::Layer, idx,
                                     "Layer " + juce::String(idx + 1), prefix);

        // Batch 3 (2026-05-06): create + register the render task for this
        // Layer.  QA-Ef (2026-05-21): the dispatcher is the single render
        // path, so this is the live audio plumbing -- not dead scaffolding.
        auto task = std::make_unique<EngineInsertTask>(
            eng, EngineInsertTask::Kind::Layer, idx,
            MixerChannelIds::layerInsert(idx), mVibeGraph);
        mRenderDispatcher.registerTask(task.get());
        mLayerRenderTasks[(size_t) idx] = std::move(task);
    }
}
// QA-ModelShell TS6 (BLU-447): hosted VST3 instrument tab.  Byte-for-byte the
// Layer shape -- engine pointer under a SpinLock, mixer strip params, an
// InsertNode, and an EngineInsertTask on the dispatcher.  The only differences
// are the parent bus and which per-tab MIDI array the task reads.
void BaySickDAWProcessor::registerPluginEngine(int idx, juce::AudioProcessor* eng)
{
    {
        juce::SpinLock::ScopedLockType lk(mPluginEngineLock);
        if (idx >= 0 && idx < kMaxPluginPages) mPluginEngines[idx] = eng;
    }
    if (idx >= 0 && idx < kMaxPluginPages && eng != nullptr)
    {
        const juce::String prefix = "mixer_plugin_" + juce::String(idx);
        ensureMixerStripParams(prefix, MixerStripKind::Insert, MixerChannelIds::kPluginsBus);
        mVibeGraph.ensureInsertNode(BaySickGraph::InsertKind::Plugin, idx,
                                     "Plugin " + juce::String(idx + 1), prefix);

        auto task = std::make_unique<EngineInsertTask>(
            eng, EngineInsertTask::Kind::Plugin, idx,
            MixerChannelIds::pluginInsert(idx), mVibeGraph);
        mRenderDispatcher.registerTask(task.get());
        mPluginRenderTasks[(size_t) idx] = std::move(task);
    }
}

// THREAD SAFETY: quiescent-state reclamation for the teardown shield -- the
// same idea as RetirementQueue::setInUseGeneration, applied to processBlock as
// a whole.  mAudioBlockCounter is published by the audio thread with release
// semantics at the top of processBlock; this waiter observes it with acquire,
// so once it has advanced twice the block that may have been mid-render when
// the shield went up has provably returned and its writes are visible here.
// A fixed wait cannot make that claim at any single duration: one block is
// 23 ms at 1024 samples / 44.1 kHz and 46 ms at 2048, so under a large ASIO
// buffer it expires while the audio thread is still inside the render.
void BaySickDAWProcessor::settleAudioThread() noexcept
{
    jassert (juce::MessageManager::existsAndIsCurrentThread());

    // Nothing is calling processBlock in these states, so no acknowledgement
    // can ever arrive and waiting would freeze the UI for the whole timeout
    // instead of protecting anything.  mAudioDevicePrepared covers both never
    // started and started-then-stopped; isSuspended covers the offline-render
    // window, where the standalone player skips the device callback entirely
    // (juce::AudioProcessorPlayer checks it) and the render loop that replaces
    // it may be driven by this very thread.
    if (! mAudioDevicePrepared.load (std::memory_order_acquire)) return;
    if (isSuspended()) return;

    const double sr  = mSampleRate;
    const int    blk = mBlockSize;
    if (sr <= 0.0 || blk <= 0) return;

    // Four block periods is generous headroom over the two advances we need;
    // the clamp keeps a tiny buffer from timing out on scheduler jitter and a
    // huge one from stalling the message thread for a visible beat.
    const int timeoutMs = juce::jlimit (10, 250,
                                        (int) (4.0 * (double) blk * 1000.0 / sr) + 5);

    const std::uint64_t start = mAudioBlockCounter.load (std::memory_order_acquire);
    const juce::uint32  t0    = juce::Time::getMillisecondCounter();

    while ((mAudioBlockCounter.load (std::memory_order_acquire) - start) < 2u)
    {
        if ((juce::uint32) (juce::Time::getMillisecondCounter() - t0)
              >= (juce::uint32) timeoutMs)
            break;

        juce::Thread::sleep (1);
    }
}

// Teardown shield for every unregister*Engine below.
//
// The audio thread walks the dispatcher's task vector on EVERY block, and each
// of these functions erases from that vector and then FREES the task object.
// On a user gesture (tab close, engine re-pick) no shield is up -- unlike a
// project load -- so without this the erase races the range-for and the free
// races an in-flight task->run().  Raising mProjectLoadInProgress bails
// processBlock to silence at its top; the settle lets the in-flight block
// finish before anything is freed.  ORDER: raise -> settle -> free -> restore.
//
// The shieldWasUp save/restore keeps an outer load's shield intact (a nested
// teardown must not lower it early) AND skips the settle when the caller
// already paid one, so closing many tabs at once costs a single mute.  Same
// idiom as loadBaySickRustyDrumsKit / destroyBaySickRustyDrums; it is safe
// because the shield is only ever raised from the message thread (see the
// mProjectLoadInProgress declaration).
//
// Cost: two audio blocks of silence on a tab close / engine swap -- about 5 ms
// at 128 samples / 48 kHz (2.7 ms a block, and two is the FLOOR: the shield can
// land mid-block), scaling with the device buffer.  The mute itself was
// accepted (Jeff, 2026-08-06) as the trade against the crash.
void BaySickDAWProcessor::unregisterPluginEngine(int idx)
{
    if (idx < 0 || idx >= kMaxPluginPages) return;
    const bool shieldWasUp = isProjectLoadInProgress();
    setProjectLoadInProgress (true);
    if (! shieldWasUp) settleAudioThread();
    // Task down BEFORE the engine pointer clears, so the dispatcher never sees
    // a task aimed at a dead engine.
    if (mPluginRenderTasks[(size_t) idx])
    {
        mRenderDispatcher.unregisterTask(MixerChannelIds::pluginInsert(idx));
        mPluginRenderTasks[(size_t) idx].reset();
    }
    {
        juce::SpinLock::ScopedLockType lk(mPluginEngineLock);
        mPluginEngines[idx] = nullptr;
    }
    setProjectLoadInProgress (shieldWasUp);
    // InsertNode retained on purpose - preserves mixer state if the tab returns.
}

void BaySickDAWProcessor::unregisterLayerEngine(int idx)
{
    if (idx < 0 || idx >= kMaxLayerPages) return;
    const bool shieldWasUp = isProjectLoadInProgress();   // see the shield note above
    setProjectLoadInProgress (true);
    if (! shieldWasUp) settleAudioThread();
    // Batch 3: tear down the task BEFORE clearing the engine pointer so the
    // dispatcher never sees a task pointing at a dead engine.
    if (mLayerRenderTasks[(size_t) idx])
    {
        mRenderDispatcher.unregisterTask(MixerChannelIds::layerInsert(idx));
        mLayerRenderTasks[(size_t) idx].reset();
    }
    {
        juce::SpinLock::ScopedLockType lk(mLayerEngineLock);
        mLayerEngines[idx] = nullptr;
    }
    setProjectLoadInProgress (shieldWasUp);
    // InsertNode retained on purpose - preserves mixer state if the page is re-opened.
}
void BaySickDAWProcessor::registerBassEngine(int pageIdx, juce::AudioProcessor* eng)
{
    bindSampleLoadShield (eng);
    if (pageIdx < 0 || pageIdx >= kMaxBassPages) return;
    {
        juce::SpinLock::ScopedLockType lk(mBassEngineLock);
        mBassEngines[pageIdx] = eng;
    }
    if (eng != nullptr)
    {
        const juce::String prefix = "mixer_bass_" + juce::String(pageIdx);
        ensureMixerStripParams(prefix, MixerStripKind::Insert, MixerChannelIds::kBassBus);
        mVibeGraph.ensureInsertNode(BaySickGraph::InsertKind::Bass, pageIdx,
                                     "Bass " + juce::String(pageIdx + 1), prefix);

        // Batch 3 (2026-05-06): MT render task wrapper.
        auto task = std::make_unique<EngineInsertTask>(
            eng, EngineInsertTask::Kind::Bass, pageIdx,
            MixerChannelIds::bassInsert(pageIdx), mVibeGraph);
        mRenderDispatcher.registerTask(task.get());
        mBassRenderTasks[(size_t) pageIdx] = std::move(task);
    }
}
void BaySickDAWProcessor::unregisterBassEngine(int pageIdx)
{
    if (pageIdx < 0 || pageIdx >= kMaxBassPages) return;
    const bool shieldWasUp = isProjectLoadInProgress();   // see the shield note above
    setProjectLoadInProgress (true);
    if (! shieldWasUp) settleAudioThread();
    if (mBassRenderTasks[(size_t) pageIdx])
    {
        mRenderDispatcher.unregisterTask(MixerChannelIds::bassInsert(pageIdx));
        mBassRenderTasks[(size_t) pageIdx].reset();
    }
    {
        juce::SpinLock::ScopedLockType lk(mBassEngineLock);
        mBassEngines[pageIdx] = nullptr;
    }
    setProjectLoadInProgress (shieldWasUp);
}
// 2026-04-25: registerDrumsEngine / unregisterDrumsEngine removed - legacy
// 16-slot BaySickDrumsProcessor deleted.  Per-drum-tab registration uses
// registerDrumEngine (singular) below.

// ─────────────────────────────────────────────────────────────────────────────
// QA-L-Fix (2026-07-19): per-drum kit trigger dispatch.  Audio thread only.
// ─────────────────────────────────────────────────────────────────────────────
int BaySickDAWProcessor::drumPlayNoteRT (int drumIdx) const noexcept
{
    if (drumIdx < 0 || drumIdx >= kMaxDrumPages) return 60;
    if (auto* p = mDrumPlayNotePtr[(size_t) drumIdx].load (std::memory_order_acquire))
        return juce::jlimit (0, 127, (int) std::lround (p->load()));
    return 60;
}

void BaySickDAWProcessor::dispatchDrumTriggers (
    const juce::MidiMessage& msg,
    int samplePosition,
    int liveTargetKind,
    std::array<juce::MidiBuffer, kMaxDrumPages>& drumPageMidi) noexcept
{
    // Fast-path bypass: one atomic load skips the 16-slot scan entirely until
    // the user binds a trigger (reference_audio_thread_fast_path_bypass).
    if (! mDrumTriggers.anyBound()) return;

    const bool isNoteOn  = msg.isNoteOn();
    const bool isNoteOff = msg.isNoteOff();
    const bool isCc      = msg.isController();
    if (! isNoteOn && ! isNoteOff && ! isCc) return;

    // D-8: note triggers fire ONLY while the Drum Kit is the focused engine.
    // On any other surface the note plays that engine normally, so a pad note
    // can never double-fire.  Kind 0 == PianoRollPage::EngineKind::DrumKit.
    const bool kitFocused = (liveTargetKind == 0);

    const int  msgChannel = msg.getChannel();
    const int  msgNumber  = isCc ? msg.getControllerNumber() : msg.getNoteNumber();

    // D-11: velocity source is app-wide.  Fixed exists for pads that aren't
    // velocity sensitive -- they'd otherwise send one constant value and every
    // hit would land at that level anyway, just an arbitrary one.
    const bool useFixedVel = DrumTriggerVelocity::gUseFixed.load (std::memory_order_relaxed);

    for (int di = 0; di < kMaxDrumPages; ++di)
    {
        const auto b = mDrumTriggers.getBindingRT (di);
        if (! b.isSet()) continue;
        // Message TYPE must match before the number does: a Note binding on 42
        // and CC 42 both carry "42" but mean entirely different things, and
        // matching on the number alone would cross-fire them.
        const bool typeMatches = (b.kind == DrumTriggerMap::Kind::Cc)
                                     ? isCc
                                     : (isNoteOn || isNoteOff);
        if (! typeMatches) continue;
        if (b.number != msgNumber) continue;
        if (b.channel != 0 && b.channel != msgChannel) continue;

        const int playNote = drumPlayNoteRT (di);

        if (b.kind == DrumTriggerMap::Kind::Cc && isCc)
        {
            auto& hold = mCcTriggerHolds[(size_t) di];
            if (msg.getControllerValue() > 0)
            {
                // Re-trigger while already held: release the old note first so
                // the engine sees a clean on/off pair rather than two stacked
                // note-ons the single hold slot could never release.
                if (hold.active)
                    drumPageMidi[di].addEvent (
                        juce::MidiMessage::noteOff (1, hold.note), samplePosition);

                const float vel = useFixedVel
                                      ? DrumTriggerVelocity::kFixedVelocity
                                      : (float) msg.getControllerValue() / 127.0f;
                drumPageMidi[di].addEvent (
                    juce::MidiMessage::noteOn (1, playNote, vel), samplePosition);
                hold.active      = true;
                hold.note        = playNote;
                hold.samplesLeft = (int64_t) (kCcTriggerMaxHoldSeconds
                                              * juce::jmax (1.0, getSampleRate()));
                mAnyCcHoldActive = true;
            }
            else if (hold.active)
            {
                drumPageMidi[di].addEvent (
                    juce::MidiMessage::noteOff (1, hold.note), samplePosition);
                hold = {};
            }
        }
        else if (b.kind == DrumTriggerMap::Kind::Note && kitFocused)
        {
            auto& held = mNoteTriggerHeld[(size_t) di];
            if (isNoteOn)
            {
                // Already held (pad re-struck without a note-off): release the
                // old voice first so it can't outlive its owner.
                if (held >= 0)
                    drumPageMidi[di].addEvent (
                        juce::MidiMessage::noteOff (1, held), samplePosition);

                const float vel = useFixedVel ? DrumTriggerVelocity::kFixedVelocity
                                              : msg.getFloatVelocity();
                drumPageMidi[di].addEvent (
                    juce::MidiMessage::noteOn (1, playNote, vel), samplePosition);
                held = playNote;
            }
            else if (held >= 0)
            {
                // Release the pitch the hit STARTED at, not the current play
                // note -- re-assigning the drum mid-hold would otherwise leave
                // the original voice sounding forever.
                drumPageMidi[di].addEvent (
                    juce::MidiMessage::noteOff (1, held), samplePosition);
                held = -1;
            }
        }
    }
}

void BaySickDAWProcessor::tickCcTriggerHolds (
    int numSamples,
    std::array<juce::MidiBuffer, kMaxDrumPages>& drumPageMidi) noexcept
{
    // Fast-path bypass.  Deliberately NOT gated on anyBound(): a hold can
    // outlive the binding that started it (user unbinds mid-hold), and skipping
    // the tick then would strand that voice.  The flag is self-correcting --
    // set when a hold is armed, recomputed from the survivors each pass.
    if (! mAnyCcHoldActive) return;

    bool stillActive = false;
    for (int di = 0; di < kMaxDrumPages; ++di)
    {
        auto& hold = mCcTriggerHolds[(size_t) di];
        if (! hold.active) continue;
        hold.samplesLeft -= numSamples;
        if (hold.samplesLeft > 0) { stillActive = true; continue; }

        drumPageMidi[di].addEvent (juce::MidiMessage::noteOff (1, hold.note), 0);
        hold = {};
    }
    mAnyCcHoldActive = stillActive;
}

// D1.2 (2026-04-24): per-drum-page engine registration.  The engine is
// rig-owned (EngineTab in EngineRig); this wires the raw, non-owning pointer
// into the audio graph the same way layers/bass do.  Mixer strip + InsertNode
// reuse the existing Drum kind/range so the mixer UI stays consistent during
// the D1 transition.
void BaySickDAWProcessor::registerDrumEngine(int pageIdx, juce::AudioProcessor* eng)
{
    bindSampleLoadShield (eng);
    if (pageIdx < 0 || pageIdx >= kMaxDrumPages) return;
    {
        juce::SpinLock::ScopedLockType lk(mDrumEngineLock);
        mDrumEngines[pageIdx] = eng;
    }
    if (eng != nullptr)
    {
        const juce::String prefix = "mixer_drum_" + juce::String(pageIdx);
        // QA-SOUNDNESS (2026-08-07): bank 1 (pages 0..15) defaults to the Drums
        // Bus, bank 2 (16..31) to Drums Bus 2.  Only the DEFAULT -- a project
        // saved before this change restores its own _sendTo and is unaffected.
        ensureMixerStripParams(prefix, MixerStripKind::Insert,
                               MixerChannelIds::drumBusForPage(pageIdx));
        // Publish the play-pitch pointer for kit-trigger dispatch (the param
        // exists as of the ensure above; audio thread acquire-loads it).
        mDrumPlayNotePtr[(size_t) pageIdx].store(
            apvts.getRawParameterValue(prefix + "_playNote"),
            std::memory_order_release);
        mVibeGraph.ensureInsertNode(BaySickGraph::InsertKind::Drum, pageIdx,
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
void BaySickDAWProcessor::unregisterDrumEngine(int pageIdx)
{
    if (pageIdx < 0 || pageIdx >= kMaxDrumPages) return;
    const bool shieldWasUp = isProjectLoadInProgress();   // see the shield note above
    setProjectLoadInProgress (true);
    if (! shieldWasUp) settleAudioThread();
    if (mDrumRenderTasks[(size_t) pageIdx])
    {
        mRenderDispatcher.unregisterTask(MixerChannelIds::drumInsert(pageIdx));
        mDrumRenderTasks[(size_t) pageIdx].reset();
    }
    {
        juce::SpinLock::ScopedLockType lk(mDrumEngineLock);
        mDrumEngines[pageIdx] = nullptr;
    }
    // Closed drum stops accepting kit triggers (param persists; re-register
    // re-publishes the pointer).
    mDrumPlayNotePtr[(size_t) pageIdx].store(nullptr, std::memory_order_release);
    bool any = false;
    for (auto* e : mDrumEngines) if (e) { any = true; break; }
    mAnyDrumPageActive.store(any, std::memory_order_release);
    setProjectLoadInProgress (shieldWasUp);
    // InsertNode retained - preserves mixer state if drum is re-added.
}

// G-3 (2026-04-28): per-clip-page engine registration.  pageIdx is the audio-
// row index for the bound clip (1:1 mapping to mixer_audio_<row>).  Unlike
// Layer / Bass / Drum engines which create their own InsertNode + mixer
// strip, Clip engines share the existing Audio InsertNode for that row -
// arrangement-playback audio + piano-roll-triggered audio mix into the same
// strip so the user sees one channel per clip rather than two.
void BaySickDAWProcessor::registerClipEngine(int pageIdx, juce::AudioProcessor* eng)
{
    bindSampleLoadShield (eng);
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

void BaySickDAWProcessor::unregisterClipEngine(int pageIdx)
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
void BaySickDAWProcessor::registerVoxEngine(int pageIdx, juce::AudioProcessor* eng)
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

void BaySickDAWProcessor::unregisterVoxEngine(int pageIdx)
{
    if (pageIdx < 0 || pageIdx >= kMaxVoxPages) return;

    const bool shieldWasUp = isProjectLoadInProgress();   // see the shield note above
    setProjectLoadInProgress (true);
    if (! shieldWasUp) settleAudioThread();

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
    setProjectLoadInProgress (shieldWasUp);
}

void BaySickDAWProcessor::registerInstEngine(int pageIdx, juce::AudioProcessor* eng)
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

void BaySickDAWProcessor::unregisterInstEngine(int pageIdx)
{
    if (pageIdx < 0 || pageIdx >= kMaxInstPages) return;

    const bool shieldWasUp = isProjectLoadInProgress();   // see the shield note above
    setProjectLoadInProgress (true);
    if (! shieldWasUp) settleAudioThread();

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
    setProjectLoadInProgress (shieldWasUp);
}

// §P4.3 B7 (2026-04-22): register/unregister{Layer,Bass,Drums}PageEQ APIs
// deleted.  Per-page EQ DSPs (mLayerPageEQs / mBassPageEQs / mDrumsPageEQ)
// are gone - pages now bind their EQ display to the InsertNode / BusNode
// preEq directly via BaySickGraph::getInsertPreEQ() / getXxxBusPreEQ().

// QA-ApvtsAutomation (2026-07-25): registerParamsForTrack /
// unregisterParamsForTrack / isTrackRegistered removed.  Everything they
// registered was dead -- the engine mirror sets (id-mismatched with the ids the
// engine editors stamp) and the 6-slot "tk_{id}_rack_slot{s}_*" family, which had
// zero readers anywhere in the tree.  Real racks live on InsertNodes under the
// "mixer_*" prefixes with uuid-keyed automation ids; isTrackRegistered had no
// callers.  mRegisteredTrackParams itself STAYS -- ensureMixerStripParams and the
// EQ-bank helpers use it as their id accumulator, keyed by mixer prefix rather
// than by trackId, so the two never shared entries.

// ═══════════════════════════════════════════════════════════════════════════════
//  5F-4a: Mixer-strip lazy APVTS registration
// ═══════════════════════════════════════════════════════════════════════════════

void BaySickDAWProcessor::addParamsForMixerStrip(const juce::String& prefix,
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
    // visual range (kFaderMax in MixerTrackStrip + kFMax in BaySickLAF's
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
        // not bound to APVTS at all → the master node read a null pointer →
        // master mute did nothing).  Solo + polarity are intentionally
        // omitted - master has no peer to solo against and polarity at the
        // master is rarely useful (and would invert ALL output, easy to
        // mistake for "broken").
        addB(prefix + "_mute", prefix + " Mute", false);
    }

    // FX Bypass on ALL strip types (master/bus/insert) - each has its own rack.
    addB(prefix + "_bypass", prefix + " FX Bypass", false);

    // QA-RustyMeter Task 5 (2026-05-30): bus collapse/expand view state.  Buses
    // ONLY (master + inserts have no member group beneath them to collapse).
    // UI-only -- no audio path reads it; registered here so it serializes with
    // the project's APVTS state and the collapsed buses restore on load.
    if (kind == MixerStripKind::Bus)
        addB(prefix + "_collapsed", prefix + " Collapsed", false);

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

    // Main-out line 0: covers every reserved channel id (0..999). Default =
    // natural parent.  Id and range are frozen -- saved projects hold it.
    addI(prefix + "_sendTo", prefix + " Send-To", 0, 999, defaultSendTo);

    // Main-out lines 1..3: a strip may feed up to four destinations, each a
    // full-level copy.  -1 = inactive, which is also how a project saved before
    // these existed resolves (the param is simply absent from its state).
    for (int m = 1; m < MixerChannelIds::kMaxMainOutsPerStrip; ++m)
        addI(MixerChannelIds::mainOutParamId(prefix, m),
             prefix + " Main Out " + juce::String(m) + " To", -1, 999, -1);

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

    // Per-drum play pitch: the MIDI note this drum SOUNDS at.  Kit hits are
    // stamped here and kit triggers fire here; it is never an input filter.
    // Default C5 (60) matches the historical fixed kit note, so every drum
    // starts assigned.  Drums only -- other inserts have no kit concept.
    if (kind == MixerStripKind::Insert && prefix.startsWith("mixer_drum_"))
        addI(prefix + "_playNote", prefix + " Play Note", 0, 127, 60);
}

bool BaySickDAWProcessor::ensureMixerStripParams(const juce::String& prefix,
                                                 MixerStripKind kind,
                                                 int defaultSendTo)
{
    if (mRegisteredMixerStrips.count(prefix) > 0)
        return false;

    addParamsForMixerStrip(prefix, kind, defaultSendTo);
    // Session B: every mixer strip also gets a full EQ band param set under the
    // same prefix (prefix + "_mid_eq{b}<Suffix>" / "_side_eq{b}<Suffix>"), so the
    // post-rack EQ on this strip is automatable.  Idempotent because dynF/dynB/dynI
    // skip any id APVTS already holds -- there is no remove API, so a duplicate
    // registration would be permanent.
    addParamsForTrackEQ(prefix);
    // §P4.3: every strip ALSO gets a pre-rack EQ block under
    // prefix + "_preeq_mid_eq{b}*" / "_preeq_side_eq{b}*".  Same idempotent
    // guard.  Bus/insert preEq DSPs (added in B2) bind to these params via
    // updateAllPreRackEQsFromApvts (B4).
    addParamsForTrackPreEQ(prefix);
    // THREAD SAFETY: resolve this strip's EQ param pointers NOW, on the message
    // thread, so the audio-thread sweep never has to look an id up in APVTS's
    // adapterTable -- which this very function is concurrently emplacing into.
    // Must follow both EQ-bank registrations above: it caches what they created.
    cacheEqParamPointers(prefix);
    mRegisteredMixerStrips.insert(prefix);
    // QA-ModelShell TS3 (2026-07-27): automation registration for these ids is
    // triggered HERE, at param materialization, rather than by whatever view
    // happened to show them.  Strips are created lazily and long after the
    // startup sweep, so registerStaticAutomationHandlers could not have seen
    // them -- which is why the mixer strip and the EQ display each registered
    // their own, view-scoped, and needed a re-registration shim at every
    // project boundary to survive.
    if (onMixerStripParamsCreated) onMixerStripParamsCreated (prefix);
    return true;
}

void BaySickDAWProcessor::ensureMixerBusAndMasterParams()
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
    // QA-ModelShell TS6 (BLU-447) -- MISSED, fixed TS7 2026-07-30.  TS6 added the
    // channel id, the InsertNode, the routing-graph entry and the mixer strip and
    // never registered the bus's PARAMS, so `mixer_pluginbus_sendTo` did not
    // exist: rebuildRoutingFromApvts looked it up, found nothing, and the bus had
    // no edge to Master.  Hosted plugin audio had no path out.  Every other bus
    // on this list is here for exactly that reason.
    ensureMixerStripParams("mixer_pluginbus", MixerStripKind::Bus,   kMaster);
    // QA-Layout T10 (L13): secondary group buses -- always registered for the
    // same reason as every bus above.
    ensureMixerStripParams("mixer_layersbus2",  MixerStripKind::Bus, kMaster);
    ensureMixerStripParams("mixer_bassbus2",    MixerStripKind::Bus, kMaster);
    ensureMixerStripParams("mixer_clipsbus2",   MixerStripKind::Bus, kMaster);
    ensureMixerStripParams("mixer_pluginbus2",  MixerStripKind::Bus, kMaster);
    // QA-SOUNDNESS (2026-08-07): second drum kit's bus -- registered here for
    // the same reason as every bus above (no params, no _sendTo, no edge out).
    ensureMixerStripParams("mixer_drumsbus2",   MixerStripKind::Bus, kMaster);
}

// QA-G3Smoke Swing (SW-6): eager bulk registration at startup (mirrors
// ensureMixerBusAndMasterParams) + raw-atomic pointer caching for the
// scheduler's per-block reads.  Family set = {layer, bass, drum, inst, plugin}
// x page + rusty.  Vox registers nothing (no vox MIDI -- Jeff 2026-07-23); clip
// rolls follow the GLOBAL knob at full mix by design, no per-page params
// (Jeff 2026-07-23).  Params persist with the project via APVTS state.
void BaySickDAWProcessor::ensureSwingParams()
{
    auto addF = [&](const juce::String& id, const juce::String& name,
                    float def) -> std::atomic<float>*
    {
        if (apvts.getParameter(id) == nullptr)
            apvts.createAndAddParameter(std::make_unique<juce::AudioParameterFloat>(
                VID(id), name, juce::NormalisableRange<float>(0.f, 1.f), def));
        return apvts.getRawParameterValue(id);
    };
    auto addB = [&](const juce::String& id, const juce::String& name,
                    bool def) -> std::atomic<float>*
    {
        if (apvts.getParameter(id) == nullptr)
            apvts.createAndAddParameter(std::make_unique<juce::AudioParameterBool>(
                VID(id), name, def));
        return apvts.getRawParameterValue(id);
    };

    mSwingGlobal = addF("globalSwing", "Global Swing", 0.f);
    for (int i = 0; i < kMaxLayerPages; ++i)
    {
        const juce::String p = "swing_layer_" + juce::String(i);
        mSwingMixLayer[i]   = addF(p + "_mix",   p + " Mix", 1.f);
        mSwingTruncLayer[i] = addB(p + "_trunc", p + " Truncate", false);
    }
    for (int i = 0; i < kMaxBassPages; ++i)
    {
        const juce::String p = "swing_bass_" + juce::String(i);
        mSwingMixBass[i]   = addF(p + "_mix",   p + " Mix", 1.f);
        mSwingTruncBass[i] = addB(p + "_trunc", p + " Truncate", false);
    }
    for (int i = 0; i < kMaxDrumPages; ++i)
    {
        const juce::String p = "swing_drum_" + juce::String(i);
        mSwingMixDrum[i]   = addF(p + "_mix",   p + " Mix", 1.f);
        mSwingTruncDrum[i] = addB(p + "_trunc", p + " Truncate", false);
    }
    for (int i = 0; i < kMaxInstPages; ++i)
    {
        const juce::String p = "swing_inst_" + juce::String(i);
        mSwingMixInst[i]   = addF(p + "_mix",   p + " Mix", 1.f);
        mSwingTruncInst[i] = addB(p + "_trunc", p + " Truncate", false);
    }
    for (int i = 0; i < kMaxPluginPages; ++i)
    {
        const juce::String p = "swing_plugin_" + juce::String(i);
        mSwingMixPlugin[i]   = addF(p + "_mix",   p + " Mix", 1.f);
        mSwingTruncPlugin[i] = addB(p + "_trunc", p + " Truncate", false);
    }
    mSwingMixRusty   = addF("swing_rusty_mix",   "swing_rusty Mix", 1.f);
    mSwingTruncRusty = addB("swing_rusty_trunc", "swing_rusty Truncate", false);
}

BaySickDAWProcessor::SwingKnobBinding
BaySickDAWProcessor::makeSwingKnobBinding (const juce::String& mixId,
                                          const juce::String& truncId)
{
    SwingKnobBinding b;
    auto* ap = &apvts;
    b.getMix = [ap, mixId]() -> float
    {
        if (auto* v = ap->getRawParameterValue (mixId)) return v->load();
        return 1.0f;
    };
    b.setMix = [ap, mixId] (float v)
    {
        beginParamUndoGesture (*ap, mixId); // Task 6 (12-iv)
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (ap->getParameter (mixId)))
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (
                juce::jlimit (0.0f, 1.0f, v)));
    };
    b.getTrunc = [ap, truncId]() -> bool
    {
        if (auto* v = ap->getRawParameterValue (truncId)) return v->load() >= 0.5f;
        return false;
    };
    b.setTrunc = [ap, truncId] (bool on)
    {
        beginParamUndoGesture (*ap, truncId); // Task 6 (12-iv)
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (ap->getParameter (truncId)))
            p->setValueNotifyingHost (on ? 1.0f : 0.0f);
    };
    return b;
}

void BaySickDAWProcessor::setSongMode (bool b)
{
    const bool was = mSongMode.exchange (b, std::memory_order_relaxed);
    if (was == b) return;

    // Smoke round 3 (Jeff): automation clips only drive params in song mode
    // (the evaluator is gated above), so the mode boundary is where the
    // pre-automation values are captured + restored.  ENTRY: snapshot the
    // current value of every param any automation clip targets.  EXIT:
    // restore the snapshot so the knobs return to where the user left them.
    // Message thread only (the transport SONG toggle / project-restore path).
    if (b)
    {
        mAutomationBaseline.clear();
        if (mPatternManager != nullptr)
            for (int bi = 0; bi < mPatternManager->getNumBlocks(); ++bi)
            {
                const auto& blk = mPatternManager->getBlock (bi);
                if (blk.clipType != ClipType::Automation)   continue;
                if (blk.automationLane.paramId.isEmpty())   continue;
                bool seen = false;
                for (const auto& p : mAutomationBaseline)
                    if (p.first == blk.automationLane.paramId) { seen = true; break; }
                if (seen) continue;
                if (auto* param = apvts.getParameter (blk.automationLane.paramId))
                    mAutomationBaseline.push_back ({ blk.automationLane.paramId,
                                                     param->getValue() });
            }
    }
    else
    {
        // QA-UndoCoverage: baseline restore is programmatic -- not history.
        juce::AudioProcessorValueTreeState::ScopedProgrammaticParamWrites spw;
        for (const auto& p : mAutomationBaseline)
            if (auto* param = apvts.getParameter (p.first))
                param->setValueNotifyingHost (p.second);
        mAutomationBaseline.clear();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  5F-4a: Audio-row mixer strip registration
// ═══════════════════════════════════════════════════════════════════════════════

void BaySickDAWProcessor::ensureAudioInsert(int row, const juce::String& displayName)
{
    if (row < 0 || row >= kMaxAudioRows) return;

    const juce::String prefix = "mixer_audio_" + juce::String(row);
    ensureMixerStripParams(prefix, MixerStripKind::Insert, MixerChannelIds::kClipsBus);
    mVibeGraph.ensureInsertNode(BaySickGraph::InsertKind::Audio, row,
                                 displayName.isNotEmpty() ? displayName
                                     : ("Audio " + juce::String(row + 1)),
                                 prefix);

    // Batch 5 (2026-05-06): create the per-row CompositeAudioInsertTask if it
    // doesn't exist yet.  No removeAudioInsert hook exists - audio inserts
    // persist for the project lifetime; the unique_ptr cleans up on plugin
    // destroy.
    if (! mAudioRenderTasks[(size_t) row])
    {
        auto task = std::make_unique<CompositeAudioInsertTask>(
            row, MixerChannelIds::audioInsert(row), mVibeGraph, *this);
        mRenderDispatcher.registerTask(task.get());
        mAudioRenderTasks[(size_t) row] = std::move(task);
    }
}

// 5F-4b B2: Aux strip registration (receive-only, default routes to Master).
void BaySickDAWProcessor::ensureAuxInsert(int idx, const juce::String& displayName)
{
    if (idx < 0 || idx >= MixerChannelIds::kMaxAuxStrips) return;   // matches MixerChannelIds aux range

    const juce::String prefix = "mixer_aux_" + juce::String(idx);
    ensureMixerStripParams(prefix, MixerStripKind::Insert, MixerChannelIds::kFxBus);
    mVibeGraph.ensureInsertNode(BaySickGraph::InsertKind::Aux, idx,
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

// QA-Ef #4 (2026-05-22): tear down every aux insert (engine + render task)
// on project load.  Called from the three load-entry points -- deserializeProject
// (project open), setStateInformation (VST3 host load), and the editor's
// doFileNew + loadTemplate paths -- BEFORE restoreAuxStripsFromState rebuilds
// from the loaded project, so aux strips from the prior session don't leak
// across loads.  Audio-thread safety: each caller raises mProjectLoadInProgress
// and waits out the in-flight block first, so processBlock is bailing to
// silence while we mutate the render-task list.
void BaySickDAWProcessor::clearAllAuxInserts()
{
    for (size_t i = 0; i < mAuxRenderTasks.size(); ++i)
    {
        if (mAuxRenderTasks[i] != nullptr)
        {
            mRenderDispatcher.unregisterTask (MixerChannelIds::auxStrip ((int) i));
            mAuxRenderTasks[i].reset();
        }
    }
    mVibeGraph.clearAuxInserts();
}

// R1 (2026-04-23): Vox / Inst strip registration.  Same pattern as Aux but
// each kind has its own bus parent (VoxBus / InstBus) instead of FxBus.
// R2 adds the ASIO input-channel APVTS param at the same registration site.
void BaySickDAWProcessor::ensureVoxInsert(int idx, const juce::String& displayName)
{
    if (idx < 0 || idx >= MixerChannelIds::kMaxVoxStrips) return;
    const juce::String prefix = "mixer_vox_" + juce::String(idx);
    ensureMixerStripParams(prefix, MixerStripKind::Insert, MixerChannelIds::kVoxBus);
    addLiveInputParams (prefix);
    mVibeGraph.ensureInsertNode(BaySickGraph::InsertKind::Vox, idx,
                                 displayName.isNotEmpty() ? displayName
                                     : ("Vox " + juce::String(idx + 1)),
                                 prefix);
}

void BaySickDAWProcessor::ensureInstInsert(int idx, const juce::String& displayName)
{
    if (idx < 0 || idx >= MixerChannelIds::kMaxInstStrips) return;
    const juce::String prefix = "mixer_inst_" + juce::String(idx);
    ensureMixerStripParams(prefix, MixerStripKind::Insert, MixerChannelIds::kInstBus);
    addLiveInputParams (prefix);
    mVibeGraph.ensureInsertNode(BaySickGraph::InsertKind::Inst, idx,
                                 displayName.isNotEmpty() ? displayName
                                     : ("Inst " + juce::String(idx + 1)),
                                 prefix);
}

// J-5 (2026-05-03): BaySickRustyDrums per-strip registration.  Same pattern as
// Vox/Inst but the parent bus is kRustyDrumsBus (the dedicated BaySickRustyDrums
// bus), and there's no live-input param block (these strips receive only from
// the singleton sfizz engine, never from a hardware input).
void BaySickDAWProcessor::ensureRustyInsert(int idx, const juce::String& displayName)
{
    if (idx < 0 || idx >= MixerChannelIds::kMaxRustyStrips) return;
    const juce::String prefix = "mixer_rusty_" + juce::String(idx);
    ensureMixerStripParams(prefix, MixerStripKind::Insert, MixerChannelIds::kRustyDrumsBus);
    mVibeGraph.ensureInsertNode(BaySickGraph::InsertKind::Rusty, idx,
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

void BaySickDAWProcessor::removeRustyInsert(int idx)
{
    if (idx < 0 || idx >= MixerChannelIds::kMaxRustyStrips) return;

    // Batch 6: tear down the per-strip RustyInsertTask before clearing
    // the InsertNode so the dispatcher never holds a stale task pointer.
    if (mRustyRenderTasks[(size_t) idx])
    {
        mRenderDispatcher.unregisterTask(MixerChannelIds::rustyInsert(idx));
        mRustyRenderTasks[(size_t) idx].reset();
    }

    mVibeGraph.removeInsertNode(BaySickGraph::InsertKind::Rusty, idx);
    // APVTS params persist (existing pattern - JUCE doesn't allow unregister).
    // Reset to defaults so a future re-create starts clean.  Common cleanup
    // covers: level (0 dB), pan (centre), mute/solo/polarity/bypass/arm (off).
    const juce::String prefix = "mixer_rusty_" + juce::String(idx);
    // QA-UndoCoverage Task 6: teardown default pushes are programmatic.
    juce::AudioProcessorValueTreeState::ScopedProgrammaticParamWrites spw;
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

bool BaySickDAWProcessor::hasBaySickRustyDrums() const noexcept
{
    return mRustyDrumsActive.load (std::memory_order_acquire);
}

// ─────────────────────────────────────────────────────────────────────────────
// K-2 (2026-05-05): BaySickGuitars per-instance lifecycle.  Up to kMaxInstPages
// instances coexist; each Inst page whose source = BaySickGuitars owns one
// slot.  Mirrors the Rusty pattern but indexed by instIdx instead of singleton.
// ─────────────────────────────────────────────────────────────────────────────

BaySickGuitarsProcessor* BaySickDAWProcessor::getBaySickGuitars (int instIdx) noexcept
{
    if (instIdx < 0 || instIdx >= (int) kMaxInstPages) return nullptr;
    return mGuitarsEngine[(size_t) instIdx].get();
}

bool BaySickDAWProcessor::loadBaySickGuitarsKit (int instIdx, const juce::File& sfzPath)
{
    if (instIdx < 0 || instIdx >= (int) kMaxInstPages) return false;
    if (! sfzPath.existsAsFile()) return false;

    // Both gates stay FALSE for the whole load: mGuitarsActive[] alone is not
    // enough, because the chain processor calls the engine's processBlock
    // directly without consulting it, so the engine's own mProcessingEnabled
    // comes down too (below, once the instance exists).
    mGuitarsActive[(size_t) instIdx].store (false, std::memory_order_release);

    // THREAD SAFETY: those gates are check-then-act and drain nothing.  Once
    // the audio thread is past its mProcessingEnabled load it is committed to
    // renderBlock for the rest of that block, and loadKit's very first act is
    // sfizz's prepareSfzLoad, which drops the regions and voices (and the file
    // pool whenever the path differs) that the in-flight render is reading --
    // so this is a free, not a stale table.  The vendored sfizz has no callback
    // guard of its own: renderBlock and loadSfzFile are unsynchronized against
    // each other.  The shield bails processBlock at its top and the
    // acknowledgement-based settle waits for that block to return, so the sfizz
    // mutation starts with no reader inside the engine.  Same bracket as
    // loadBaySickRustyDrumsKit, shieldWasUp save/restore included so an outer
    // project load keeps its shield.
    const bool shieldWasUp = isProjectLoadInProgress();
    setProjectLoadInProgress (true);
    if (! shieldWasUp) settleAudioThread();

    {
        const juce::SpinLock::ScopedLockType sl (mGuitarsEngineLock[(size_t) instIdx]);
        if (! mGuitarsEngine[(size_t) instIdx])
        {
            mGuitarsEngine[(size_t) instIdx] = std::make_unique<BaySickGuitarsProcessor> (instIdx, mUndoManager);
            // 2026-07-31: see mLastEnginePlayHead -- the per-block propagation is
            // change-gated and the change already happened, so a later-created
            // engine only gets a playhead if its creation path hands it one.
            mGuitarsEngine[(size_t) instIdx]->setPlayHead (enginePlayHead());
            mGuitarsEngine[(size_t) instIdx]->prepareToPlay (
                getSampleRate() > 0.0 ? getSampleRate() : 48000.0,
                getBlockSize()  > 0   ? getBlockSize()  : 512);
        }
    }

    if (auto* eng = mGuitarsEngine[(size_t) instIdx].get())
        eng->setProcessingEnabled (false);

    // The SFZ parse + sample load below is the single longest blocking step in
    // a project load, and it happens once PER sfizz tab -- a 23 s load is
    // mostly this (Jeff, measured 2026-07-28).  Report BEFORE the call: the
    // overlay pumps the peer's pending paint synchronously on every state
    // change, so the label reaches the screen before the freeze rather than
    // after it.
    if (onLoadProgress)
        onLoadProgress ("Loading BaySickGuitars " + juce::String (instIdx + 1)
                        + " - " + sfzPath.getFileNameWithoutExtension() + "...");

    if (! mGuitarsEngine[(size_t) instIdx]->loadKit (sfzPath))
    {
        // Even on failure, re-enable processing so the slot doesn't sit
        // permanently silent (the engine will produce silence from the
        // partially-loaded state until a successful load replaces it).  The
        // shield has to come down on this path too, or the caller is left with
        // audio bailed indefinitely.
        if (auto* eng = mGuitarsEngine[(size_t) instIdx].get())
            eng->setProcessingEnabled (true);
        setProjectLoadInProgress (shieldWasUp);
        return false;
    }

    // Now safe - engine fully loaded.  Audio thread can begin rendering.
    if (auto* eng = mGuitarsEngine[(size_t) instIdx].get())
        eng->setProcessingEnabled (true);
    setProjectLoadInProgress (shieldWasUp);
    mGuitarsActive[(size_t) instIdx].store (true, std::memory_order_release);
    if (onSfizzEngineReady) onSfizzEngineReady (SfizzEngineKind::Guitars, instIdx);
    return true;
}

void BaySickDAWProcessor::destroyBaySickGuitars (int instIdx)
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

BaySickBassesProcessor* BaySickDAWProcessor::getBaySickBasses (int instIdx) noexcept
{
    if (instIdx < 0 || instIdx >= (int) kMaxInstPages) return nullptr;
    return mBassesEngine[(size_t) instIdx].get();
}

bool BaySickDAWProcessor::loadBaySickBassesKit (int instIdx, const juce::File& sfzPath)
{
    if (instIdx < 0 || instIdx >= (int) kMaxInstPages) return false;
    if (! sfzPath.existsAsFile()) return false;

    mBassesActive[(size_t) instIdx].store (false, std::memory_order_release);

    // THREAD SAFETY: see loadBaySickGuitarsKit -- the two gates are
    // check-then-act, so the shield + acknowledgement settle is what actually
    // gets the audio thread out of renderBlock before loadKit frees sfizz's
    // regions and samples underneath it.
    const bool shieldWasUp = isProjectLoadInProgress();
    setProjectLoadInProgress (true);
    if (! shieldWasUp) settleAudioThread();

    {
        const juce::SpinLock::ScopedLockType sl (mBassesEngineLock[(size_t) instIdx]);
        if (! mBassesEngine[(size_t) instIdx])
        {
            mBassesEngine[(size_t) instIdx] = std::make_unique<BaySickBassesProcessor> (instIdx, mUndoManager);
            mBassesEngine[(size_t) instIdx]->setPlayHead (enginePlayHead());   // see above
            mBassesEngine[(size_t) instIdx]->prepareToPlay (
                getSampleRate() > 0.0 ? getSampleRate() : 48000.0,
                getBlockSize()  > 0   ? getBlockSize()  : 512);
        }
    }

    if (auto* eng = mBassesEngine[(size_t) instIdx].get())
        eng->setProcessingEnabled (false);

    if (onLoadProgress)
        onLoadProgress ("Loading BaySickBasses " + juce::String (instIdx + 1)
                        + " - " + sfzPath.getFileNameWithoutExtension() + "...");

    if (! mBassesEngine[(size_t) instIdx]->loadKit (sfzPath))
    {
        if (auto* eng = mBassesEngine[(size_t) instIdx].get())
            eng->setProcessingEnabled (true);
        setProjectLoadInProgress (shieldWasUp);
        return false;
    }

    if (auto* eng = mBassesEngine[(size_t) instIdx].get())
        eng->setProcessingEnabled (true);
    setProjectLoadInProgress (shieldWasUp);
    mBassesActive[(size_t) instIdx].store (true, std::memory_order_release);
    if (onSfizzEngineReady) onSfizzEngineReady (SfizzEngineKind::Basses, instIdx);
    return true;
}

void BaySickDAWProcessor::destroyBaySickBasses (int instIdx)
{
    if (instIdx < 0 || instIdx >= (int) kMaxInstPages) return;
    mBassesActive[(size_t) instIdx].store (false, std::memory_order_release);
    {
        const juce::SpinLock::ScopedLockType sl (mBassesEngineLock[(size_t) instIdx]);
        mBassesEngine[(size_t) instIdx].reset();
    }
}

BaySickRustyDrumsProcessor* BaySickDAWProcessor::getBaySickRustyDrums() noexcept
{
    return mRustyDrumsEngine.get();
}

bool BaySickDAWProcessor::loadBaySickRustyDrumsKit (const juce::File& sfzPath)
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

    // QA-DispatcherAffinity Task 3 (2026-05-29): raise the message-thread
    // shield around the entire engine mutation window -- create +
    // prepareToPlay + producer-task register + loadKit + dispatcher
    // task-list mutations + ensureRustyInsert loop.  Audit during Task 3
    // surfaced a latent race in loadKit() (which mutates sfizz internal
    // state for seconds while the audio thread could still be inside an
    // in-flight processStrips against the same engine) that the pre-Task-3
    // engine SpinLock did NOT actually cover (the lock was only held for
    // the pointer assignment, not for loadKit itself).  The shield closes
    // both that latent race AND the new race window that opens with
    // Sub-A = (i)'s removal of the per-insert try-locks.  The shieldWasUp
    // save/restore matches the existing project-load entry-point pattern.
    const bool shieldWasUp = isProjectLoadInProgress();
    setProjectLoadInProgress (true);
    if (! shieldWasUp) settleAudioThread();

    // Create the singleton on first call.  Shield raised above bails the
    // audio thread at processBlock top, so no concurrent reader exists.
    if (! mRustyDrumsEngine)
    {
        mRustyDrumsEngine = std::make_unique<BaySickRustyDrumsProcessor> (mUndoManager);
        mRustyDrumsEngine->setPlayHead (enginePlayHead());   // see mLastEnginePlayHead
        mRustyDrumsEngine->prepareToPlay (getSampleRate() > 0.0 ? getSampleRate() : 48000.0,
                                          getBlockSize()  > 0   ? getBlockSize()  : 512);
    }

    // Batch 6 (2026-05-06): create the producer task on first kit load.
    // Producer must exist before ensureRustyInsert runs (it adds a synthetic
    // dep from the producer to each insert task).
    if (! mRustyProducerTask)
    {
        mRustyProducerTask = std::make_unique<RustyDrumsProducerTask>(*this);
        mRenderDispatcher.registerTask(mRustyProducerTask.get());
    }

    if (onLoadProgress)
        onLoadProgress ("Loading BaySickRustyDrums - "
                        + sfzPath.getFileNameWithoutExtension() + "...");

    if (! mRustyDrumsEngine->loadKit (sfzPath))
    {
        // QA-DispatcherAffinity Task 3 (2026-05-29): early-return must
        // restore the shield to its prior state so the caller doesn't get
        // stuck with audio bailed indefinitely.
        setProjectLoadInProgress (shieldWasUp);
        return false;
    }

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
        mVibeGraph.removeInsertNode (BaySickGraph::InsertKind::Rusty, i);
    }

    // Spawn one strip per discovered channel, in drummer-conventional order.
    const auto& channels = mRustyDrumsEngine->getChannels();
    for (size_t i = 0; i < channels.size() && i < (size_t) MixerChannelIds::kMaxRustyStrips; ++i)
        ensureRustyInsert ((int) i, channels[i].name);

    // QA-DispatcherAffinity Task 3 (2026-05-29): lower the shield now that
    // the engine + tasks + strips are fully built.  Audio thread can begin
    // rendering on the next block.
    setProjectLoadInProgress (shieldWasUp);

    // J-7b: now safe - the engine is fully loaded, output buses sized,
    // strip InsertNodes registered.  Audio thread can begin rendering.
    mRustyDrumsActive.store (true, std::memory_order_release);
    if (onSfizzEngineReady) onSfizzEngineReady (SfizzEngineKind::RustyDrums, 0);

    // §6.8: the kit's freeze state lives on a rig tab.  Created HERE, at kit
    // creation, not at page-show -- restorePendingFreezes and auto-freeze both
    // need the tab before any page has ever opened.  (The page-show creation
    // stays as an idempotent fallback.)  The engine pointer stays null: the
    // kit engine is processor-owned, and every teardown path early-returns on
    // that.  Player-axis watcher attached here too -- the rig's APVTS watcher
    // never runs for a null-engine tab, so kit-sound changes were invisible
    // to staleness.
    {
        auto* rustyTab = mEngineRig->findTab (TabKind::Rusty, 0);
        if (rustyTab == nullptr)
            rustyTab = mEngineRig->addTab (TabKind::Rusty, 0);
        if (rustyTab != nullptr && mRustyDrumsEngine != nullptr)
        {
            // LIFETIME: a reload against the SAME singleton engine (page-preset
            // apply reaches this without going through destroyBaySickRustyDrums)
            // would otherwise free the old listener while the engine's listener
            // array still holds its address -- the next parameter write walks
            // that array through a dead vtable.  Same detach-before-destroy
            // contract destroyBaySickRustyDrums and EngineRig::teardownEngine
            // honor, and the reason freezeListenedProcs exists.
            if (rustyTab->freezeProcListener != nullptr)
            {
                for (auto* p : rustyTab->freezeListenedProcs)
                    if (p != nullptr) p->removeListener (rustyTab->freezeProcListener.get());
                rustyTab->freezeProcListener.reset();
            }
            rustyTab->freezeListenedProcs.clear();

            rustyTab->freezeProcListener = std::make_unique<EngineTab::FreezeProcListener>();
            mRustyDrumsEngine->addListener (rustyTab->freezeProcListener.get());
            rustyTab->freezeListenedProcs.push_back (mRustyDrumsEngine.get());
        }
    }
    return true;
}

// QA-ModelShell TS3 fix (2026-07-28): the offline lane replay's engine sweep
// runs over EngineRig, which does not own these three -- so without this the
// sfizz CC lanes would apply during playback and be absent from every render.
void BaySickDAWProcessor::forEachSfizzApvts (
    const std::function<void (juce::AudioProcessorValueTreeState&)>& fn)
{
    if (! fn) return;
    for (int i = 0; i < (int) kMaxInstPages; ++i)
    {
        if (auto* g = getBaySickGuitars (i)) fn (g->apvts);
        if (auto* b = getBaySickBasses  (i)) fn (b->apvts);
    }
    if (auto* r = getBaySickRustyDrums()) fn (r->apvts);
}

void BaySickDAWProcessor::destroyBaySickRustyDrums()
{
    // TS7 §6.8: the kit's freeze must not outlive the kit.  mRustyKitFrozen and
    // the rig tab's cached streams survived this function, so after a program
    // change / tab delete / project switch the NEXT kit inherited them: the
    // stale flag skipped its producer in song mode (silent drums), and the
    // republish poll pushed the OLD kit's pattern renders into the new kit's
    // strips.  Retract while the strip tasks are still alive, settle one
    // block, then drop the storage.  The tab entry itself is the CALLER's
    // call (tab delete removes it; a program change keeps it, stale, so the
    // refresh queue re-renders the new sound).
    retractFrozenSources (TabKind::Rusty, 0);
    if (auto* frozenTab = mEngineRig->findTab (TabKind::Rusty, 0))
    {
        // Detach the kit's freeze listener UNCONDITIONALLY (frozen or not)
        // before the engine it listens to is reset below.
        if (frozenTab->freezeProcListener != nullptr)
        {
            for (auto* p : frozenTab->freezeListenedProcs)
                if (p != nullptr) p->removeListener (frozenTab->freezeProcListener.get());
            frozenTab->freezeListenedProcs.clear();
            frozenTab->freezeProcListener.reset();
        }

        if (frozenTab->frozen)
        {
            settleAudioThread();
            frozenTab->freezeStreams.clear();
            frozenTab->freezePatternStreams.clear();
            frozenTab->patternStamps.clear();
            frozenTab->stalePatterns.clear();
            frozenTab->freezeStale = true;
            if (mEngineRig->onFreezeStateChanged)
                mEngineRig->onFreezeStateChanged (TabKind::Rusty, 0);
        }
    }

    // QA-DispatcherAffinity Task 3 (2026-05-29): raise the message-thread
    // shield around the whole teardown.  Audio thread bails at processBlock
    // top while we mutate (see the mProjectLoadInProgress check there) + the
    // settle waits for any in-flight block to drain.  This replaces the
    // safety the per-insert ScopedTryLockType previously provided -- Sub-A =
    // (i) removed those try-locks so 13 RustyInsertTasks can run lock-free
    // under MT execution.  The shieldWasUp save/restore matches the existing
    // pattern at StandaloneEditor's closeAllDynamicTabs + project-load entry
    // points.
    //
    // ORDER IS THE POINT: the shield sits ABOVE the task-list mutation, not
    // below it.  The audio thread walks the dispatcher's task vector every
    // block, and removeRustyInsert unregisters AND frees a RustyInsertTask,
    // so raising the shield after the loop protected only the engine reset and
    // left the task frees racing a live dispatch.  loadBaySickRustyDrumsKit is
    // the correct sibling; this now mirrors it.
    const bool shieldWasUp = isProjectLoadInProgress();
    setProjectLoadInProgress (true);
    if (! shieldWasUp) settleAudioThread();

    // Remove all 13 InsertNodes first (audio thread will see the Rusty chId range
    // [kRustyBase..kRustyBase+kMaxRustyStrips) emptied in mInsertsByChannel
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

    // QA-DispatcherAffinity Task 3 (2026-05-29): clear the Rusty piano roll
    // on every pattern.  Matches the destroy-confirmation dialog's promise
    // ("clear the Rusty piano roll on every pattern" -- the
    // "Delete BaySickRustyDrums?" onDeleteRequested AlertWindow in
    // StandaloneEditor.cpp) which was previously undelivered: only
    // BaySickRustyDrumsPage::tearDownCurrentProgram (program-change path)
    // did the clear; the tab-delete path did not.  Moved into
    // destroyBaySickRustyDrums so both call sites (tab delete +
    // program change) inherit the same behavior automatically -- single
    // source of truth for "destroy Rusty completely".  Safe to do inside
    // the shield window: the audio thread is bailed out at processBlock
    // top, so a concurrent MIDI-schedule read against half-cleared notes
    // can't fire.  Surfaced by Jeff at Task 3 Verify 2 (kit-swap test).
    if (mPatternManager != nullptr)
    {
        for (int i = 0; i < mPatternManager->getNumPatterns(); ++i)
            mPatternManager->getPattern (i).baySickRustyDrumsRoll.notes.clear();
        // #30b: the loop above mutated EVERY pattern outside the notify choke
        // point -- republish so the scheduler snapshot drops the cleared notes.
        mPatternManager->publishAllRollSnapshots();
    }

    // Shield raised above bails the audio thread at processBlock top, so
    // no concurrent reader exists when we reset the engine pointer.
    mRustyDrumsEngine.reset();

    setProjectLoadInProgress (shieldWasUp);

    // Reset bus-level mixer params to defaults so a future re-create starts
    // clean (mixer_rustybus_* params persist as APVTS zombies, harmless).
    // QA-UndoCoverage Task 6: teardown default pushes are programmatic.
    juce::AudioProcessorValueTreeState::ScopedProgrammaticParamWrites spwBus;
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

void BaySickDAWProcessor::resetBaySickRustyDrumsMixerState()
{
    // Walks every `mixer_rusty_*` insert prefix + `mixer_rustybus_*` and
    // resets the standard strip/bus params to the registered defaults.
    // Called when the user switches programs (Full <-> Basic) so the
    // freshly-spawned strips for the new program start clean.
    // QA-UndoCoverage Task 6: program-swap default pushes are programmatic.
    juce::AudioProcessorValueTreeState::ScopedProgrammaticParamWrites spw;

    // Ask each param for its OWN registered default rather than restating one
    // here: a restated list has to be right on two independent axes (the id
    // spelling AND the value) and an APVTS miss is silent, so a drifted entry
    // resets nothing and says nothing.  getDefaultValue() is already 0-1
    // normalized, which is what setValueNotifyingHost wants.
    auto resetParam = [&] (const juce::String& id)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
            p->setValueNotifyingHost (p->getDefaultValue());
    };

    auto resetStripPrefix = [&] (const juce::String& prefix)
    {
        resetParam (prefix + "_level");
        resetParam (prefix + "_pan");
        resetParam (prefix + "_width");
        resetParam (prefix + "_mute");
        resetParam (prefix + "_solo");
        resetParam (prefix + "_polarity");
        resetParam (prefix + "_bypass");
        resetParam (prefix + "_arm");   // vox/inst inserts only; no-op here
        resetParam (prefix + "_chokeGroup");
        for (int s = 0; s < 4; ++s)
        {
            resetParam (prefix + "_send" + juce::String (s) + "_to");
            resetParam (prefix + "_send" + juce::String (s) + "_amount");
            resetParam (prefix + "_send" + juce::String (s) + "_prepost");
        }
        // Both EQ banks (post-rack + pre-rack), both M/S sides, every band and
        // every band param -- composed through EqBandIds so this cannot drift
        // from the spelling addParamsForEQBank registers.
        for (int bank = 0; bank < kEqBanksPerStrip; ++bank)
            for (int side = 0; side < kEqSidesPerBank; ++side)
                for (int b = 0; b < kEqBands; ++b)
                {
                    const juce::String bp = EqBandIds::bandPrefix (prefix, bank, side, b);
                    for (const char* suffix : EqBandIds::kSuffixes)
                        resetParam (bp + suffix);
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
void BaySickDAWProcessor::addLiveInputParams (const juce::String& prefix)
{
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
    // QA-Fe Task 5: live-monitor mode (Vox only -- the realtime pitch corrector
    // is a vocal stage).  Right-click the strip's Listen LED to choose:
    //   0 = True Dry (bare voice, no corrector + no chain)
    //   1 = Bypass Pitch Corrector (chain character, no correction)
    //   2 = With Effect (full processed chain incl. correction) -- DEFAULT
    // The RECORDED take is corrected in every mode (the WET tap is separate).
    // QA-Fe2 docket 2a (Jeff): default flipped 1 -> 2 -- With-Effect
    // monitoring now runs the ~12 ms time-domain monitor shifter instead of
    // the ~48 ms R3 stream, so corrected live monitoring ships on by default.
    if (prefix.startsWith ("mixer_vox_")
        && apvts.getParameter (prefix + "_monitorMode") == nullptr)
        apvts.createAndAddParameter (std::make_unique<juce::AudioParameterInt> (
            VID(prefix + "_monitorMode"),
            prefix + " Monitor Mode", 0, 2, 2));
    // QA-OctavePedal Task 5: Inst live-monitor mode -- TWO values only (no
    // bypass-corrector middle; that's a vocal-only stage):
    //   0 = Dry         (raw strip input -- zero added latency, tight playing)
    //   1 = With Effect (the page's full processed chain) -- DEFAULT (Jeff 2026-07-18)
    // Monitor-only fork: the recorded take (raw DI tap) + playback (DI -> engine)
    // are identical in both modes.
    if (prefix.startsWith ("mixer_inst_")
        && apvts.getParameter (prefix + "_monitorMode") == nullptr)
        apvts.createAndAddParameter (std::make_unique<juce::AudioParameterInt> (
            VID(prefix + "_monitorMode"),
            prefix + " Monitor Mode", 0, 1, 1));
}

void BaySickDAWProcessor::setInputChannelName (const juce::String& stripPrefix,
                                                const juce::String& name)
{
    juce::ScopedLock sl (mInputChannelNamesLock);
    apvts.state.setProperty (juce::Identifier (stripPrefix + "_inputChannelName"),
                              name, nullptr);
}

juce::String BaySickDAWProcessor::getInputChannelName (const juce::String& stripPrefix) const
{
    juce::ScopedLock sl (mInputChannelNamesLock);
    return apvts.state.getProperty (juce::Identifier (stripPrefix + "_inputChannelName"),
                                     juce::String()).toString();
}
