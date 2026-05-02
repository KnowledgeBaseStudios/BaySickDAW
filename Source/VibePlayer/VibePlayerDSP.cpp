#include "VibePlayerDSP.h"
#include <cmath>
#include <cctype>
#include <algorithm>
#include <limits>
#include <string>    // std::stoi, std::string

// ─────────────────────────────────────────────────────────────────────────────
//  VibeSampleManager
// ─────────────────────────────────────────────────────────────────────────────

VibeSampleManager::VibeSampleManager()
{
    mFormatManager.registerBasicFormats(); // WAV, AIFF, OGG
    std::memset (mRRCounters, 0, sizeof (mRRCounters));
}

void VibeSampleManager::clear()
{
    mRegions.clear();
    mLoadedFolder = juce::File{};
    std::memset (mRRCounters, 0, sizeof (mRRCounters));
}

//==============================================================================
void VibeSampleManager::loadFolder (const juce::File& folder)
{
    clear();
    mLoadedFolder = folder;

    // Collect all audio files in folder (non-recursive for now)
    static const char* exts[] = { ".wav", ".aiff", ".aif", ".flac", nullptr };
    juce::Array<juce::File> files;
    for (int i = 0; exts[i]; ++i)
        files.addArray (folder.findChildFiles (juce::File::findFiles, false,
                                               juce::String ("*") + exts[i]));
    files.sort();

    for (const auto& f : files)
    {
        double sr = 44100.0;
        auto buf = loadFile (f, mFormatManager, sr);
        if (!buf) continue;

        VibeRegion r;
        r.sampleFile     = f;
        r.audioData      = buf;
        r.fileSampleRate = sr;
        r.rootNote       = detectRootNote (f.getFileNameWithoutExtension());
        r.loNote         = 0;
        r.hiNote         = 127;
        r.loVel          = 0;
        r.hiVel          = 127;
        mRegions.push_back (std::move (r));
    }
}

//==============================================================================
void VibeSampleManager::loadSingleFile (const juce::File& file)
{
    clear();
    mLoadedFolder = file.getParentDirectory();
    double sr = 44100.0;
    auto buf = loadFile (file, mFormatManager, sr);
    if (!buf) return;
    VibeRegion r;
    r.sampleFile     = file;
    r.audioData      = buf;
    r.fileSampleRate = sr;
    r.rootNote       = 60;
    r.loNote         = 0;
    r.hiNote         = 127;
    r.loVel          = 0;
    r.hiVel          = 127;
    mRegions.push_back (std::move (r));
}

//==============================================================================
void VibeSampleManager::loadSFZ (const juce::File& sfzFile)
{
    clear();
    mLoadedFolder = sfzFile.getParentDirectory();
    parseSFZ (sfzFile);
}

//==============================================================================
// SFZ parser — handles the most common opcodes.
// Format: opcodes as key=value pairs after a <region> header.
// Comments: // to end of line.
void VibeSampleManager::parseSFZ (const juce::File& sfzFile)
{
    const juce::File sfzDir = sfzFile.getParentDirectory();
    juce::StringArray lines;
    lines.addLines (sfzFile.loadFileAsString());

    VibeRegion current;
    bool inRegion  = false;
    bool inControl = false;
    juce::String defaultPath;   // from <control> default_path= — prepended to all sample= values

    auto flushRegion = [&]()
    {
        if (inRegion && current.audioData)
            mRegions.push_back (current);
        current = VibeRegion{};
        inRegion = false;
    };

    for (const auto& rawLine : lines)
    {
        // Strip comment
        juce::String line = rawLine.upToFirstOccurrenceOf ("//", false, false).trim();
        if (line.isEmpty()) continue;

        // Check for section headers
        if (line.containsIgnoreCase ("<region>"))
        {
            flushRegion();
            inControl = false;
            inRegion  = true;
            // Opcodes can be on same line as <region>
            line = line.fromFirstOccurrenceOf ("<region>", false, true).trim();
        }
        else if (line.containsIgnoreCase ("<control>"))
        {
            // Read default_path from this section
            flushRegion();
            inControl = true;
            continue;
        }
        else if (line.startsWithChar ('<'))
        {
            // <group>, <global>, etc. — flush current region, stop accumulating
            flushRegion();
            inControl = false;
            continue;
        }

        // Inside <control>: only care about default_path (may contain spaces)
        if (inControl)
        {
            auto v = sfzOpcode (line, "default_path", true);
            if (v.isNotEmpty())
                defaultPath = v.replace ("\\", "/");
            continue;
        }

        if (!inRegion) continue;

        // ── sample= (read to EOL — path may contain spaces) ──────────────────
        {
            auto v = sfzOpcode (line, "sample", true);
            if (v.isNotEmpty())
            {
                // Prepend default_path (if any) then resolve relative to SFZ directory
                juce::String rel = defaultPath + v.replace ("\\", "/");
                juce::File f = sfzDir.getChildFile (rel);
                if (f.existsAsFile())
                {
                    double sr = 44100.0;
                    current.audioData      = loadFile (f, mFormatManager, sr);
                    current.sampleFile     = f;
                    current.fileSampleRate = sr;
                }
            }
        }

        // ── key= (shorthand: sets lokey, hikey, pitch_keycenter) ─────────────
        {
            auto v = sfzOpcode (line, "key");
            if (v.isNotEmpty())
            {
                int n = sfzNote (v);
                current.loNote   = n;
                current.hiNote   = n;
                current.rootNote = n;
            }
        }

        // ── lokey / hikey ─────────────────────────────────────────────────────
        {
            auto v = sfzOpcode (line, "lokey");
            if (v.isNotEmpty()) current.loNote = sfzNote (v);
        }
        {
            auto v = sfzOpcode (line, "hikey");
            if (v.isNotEmpty()) current.hiNote = sfzNote (v);
        }

        // ── pitch_keycenter ───────────────────────────────────────────────────
        {
            auto v = sfzOpcode (line, "pitch_keycenter");
            if (v.isNotEmpty()) current.rootNote = sfzNote (v);
        }

        // ── lovel / hivel ─────────────────────────────────────────────────────
        {
            auto v = sfzOpcode (line, "lovel");
            if (v.isNotEmpty()) current.loVel = v.getIntValue();
        }
        {
            auto v = sfzOpcode (line, "hivel");
            if (v.isNotEmpty()) current.hiVel = v.getIntValue();
        }

        // ── tune= (cents) ─────────────────────────────────────────────────────
        {
            auto v = sfzOpcode (line, "tune");
            if (v.isNotEmpty()) current.tuneOffset = v.getFloatValue() / 100.f; // cents→semitones
        }

        // ── volume= (dB) ──────────────────────────────────────────────────────
        {
            auto v = sfzOpcode (line, "volume");
            if (v.isNotEmpty()) current.volumeOffset = v.getFloatValue();
        }

        // ── group= (maps to articulationGroup 0-3) ────────────────────────────
        {
            auto v = sfzOpcode (line, "group");
            if (v.isNotEmpty())
                current.articulationGroup = juce::jlimit (0, 3, v.getIntValue() - 1);
        }

        // ── seq_position / seq_length (round-robin) ───────────────────────────
        {
            auto v = sfzOpcode (line, "seq_position");
            if (v.isNotEmpty()) current.roundRobinIndex = v.getIntValue();
        }
        {
            auto v = sfzOpcode (line, "seq_length");
            if (v.isNotEmpty()) current.roundRobinTotal = v.getIntValue();
        }
    }

    flushRegion(); // final region
}

//==============================================================================
// Extract the value for a given opcode key from a line of SFZ text.
// Returns empty string if not found.
// readToEOL=true is used for path-type opcodes (sample=, default_path=) whose
// values may contain spaces — those read to end of line rather than stopping at
// the first whitespace.
juce::String VibeSampleManager::sfzOpcode (const juce::String& line, const char* key,
                                             bool readToEOL)
{
    const juce::String needle = juce::String (key) + "=";
    const int pos = line.indexOfIgnoreCase (needle);
    if (pos < 0) return {};

    // Word-boundary check: reject if the match is preceded by an alphanumeric/underscore
    // (e.g. "key=" must not match inside "lokey=" or "hikey=")
    if (pos > 0 && (juce::CharacterFunctions::isLetterOrDigit (line[pos - 1])
                    || line[pos - 1] == '_'))
        return {};

    juce::String rest = line.substring (pos + needle.length()).trimStart();

    if (readToEOL)
        return rest.trim();

    // Value ends at the next whitespace or end of string
    int end = 0;
    while (end < rest.length() && rest[end] != ' ' && rest[end] != '\t') ++end;
    return rest.substring (0, end).trim();
}

//==============================================================================
int VibeSampleManager::sfzNote (const juce::String& val)
{
    if (val.isEmpty()) return 60;

    // If it starts with a letter, treat as note name
    if (std::isalpha ((unsigned char) val[0]))
        return noteNameToMidi (val);

    return val.getIntValue();
}

//==============================================================================
// Convert SFZ-style note names to MIDI numbers.
// SFZ standard: C4 = 60.
int VibeSampleManager::noteNameToMidi (const juce::String& name)
{
    if (name.isEmpty()) return 60;

    const juce::String up = name.toUpperCase();
    int semitone = -1;
    switch (up[0])
    {
        case 'C': semitone = 0;  break;
        case 'D': semitone = 2;  break;
        case 'E': semitone = 4;  break;
        case 'F': semitone = 5;  break;
        case 'G': semitone = 7;  break;
        case 'A': semitone = 9;  break;
        case 'B': semitone = 11; break;
        default:  return name.getIntValue();
    }

    int pos = 1;
    if (pos < up.length() && up[pos] == '#') { semitone += 1; ++pos; }
    if (pos < up.length() && up[pos] == 'B') { semitone -= 1; ++pos; }

    // Octave: may be negative (e.g. "C-1")
    const juce::String octStr = up.substring (pos);
    const int octave = octStr.isEmpty() ? 4 : octStr.getIntValue();

    return 12 * (octave + 1) + semitone;
}

//==============================================================================
// Detect root note from filename heuristic.
// Scans for note name patterns like "A3", "C#4", "Bb2", or bare MIDI numbers.
int VibeSampleManager::detectRootNote (const juce::String& filename)
{
    // Work in plain ASCII std::string for safe character operations
    const std::string f = filename.toUpperCase().toStdString();
    const int len = (int) f.size();

    // Scan for note-name pattern: [A-G][#B]?[0-9]
    for (int i = 0; i < len - 1; ++i)
    {
        const char c = f[(size_t) i];
        if (c < 'A' || c > 'G') continue;

        // Not preceded by a letter (part of a word)
        if (i > 0 && std::isalpha ((unsigned char) f[(size_t)(i - 1)])) continue;

        int  pos          = i + 1;
        bool hasAccidental = false;

        if (pos < len && (f[(size_t) pos] == '#' || f[(size_t) pos] == 'B'))
        {
            // 'B' as flat: only treat as flat if followed by a digit
            if (f[(size_t) pos] == 'B')
            {
                if (pos + 1 >= len || !std::isdigit ((unsigned char) f[(size_t)(pos + 1)]))
                    continue; // it's the note letter B, not a flat
            }
            hasAccidental = true;
            ++pos;
        }

        if (pos >= len) continue;
        if (!std::isdigit ((unsigned char) f[(size_t) pos])) continue;

        // Support negative octave: look for '-' just before the note letter
        int octEnd = pos;
        // Grab the note name + digit
        juce::String noteName;
        if (i > 0 && f[(size_t)(i - 1)] == '-')
            noteName = juce::String (f.c_str() + i - 1, (size_t)(octEnd - i + 2));
        else
            noteName = juce::String (f.c_str() + i, (size_t)(octEnd - i + 1));

        const int midi = noteNameToMidi (noteName.trimCharactersAtStart (" "));
        if (midi >= 0 && midi <= 127) return midi;
    }

    // Fallback: look for a standalone 1-3 digit number in the filename
    for (int i = 0; i < len; ++i)
    {
        if (!std::isdigit ((unsigned char) f[(size_t) i])) continue;
        int j = i + 1;
        while (j < len && std::isdigit ((unsigned char) f[(size_t) j])) ++j;
        const int num = std::stoi (f.substr ((size_t) i, (size_t)(j - i)));
        if (num >= 0 && num <= 127)
        {
            const bool okL = (i == 0 || !std::isalnum ((unsigned char) f[(size_t)(i - 1)]));
            const bool okR = (j >= len || !std::isalnum ((unsigned char) f[(size_t) j]));
            if (okL && okR) return num;
        }
        i = j - 1;
    }

    return 60; // default C4
}

//==============================================================================
std::shared_ptr<juce::AudioBuffer<float>>
VibeSampleManager::loadFile (const juce::File& f,
                              juce::AudioFormatManager& fmt,
                              double& sampleRateOut)
{
    std::unique_ptr<juce::AudioFormatReader> reader (fmt.createReaderFor (f));
    if (!reader) return nullptr;

    sampleRateOut = reader->sampleRate;
    const int numCh  = juce::jmin (2, (int) reader->numChannels);
    const auto maxSmp = (juce::int64) (reader->sampleRate * 60.0);  // cap at 60 seconds
    const int numSmp = (int) juce::jmin (reader->lengthInSamples, maxSmp);

    auto buf = std::make_shared<juce::AudioBuffer<float>> (numCh, numSmp);
    reader->read (buf.get(), 0, numSmp, 0, true, numCh > 1);

    // If mono source, duplicate channel 0 to channel 1 for stereo playback
    if (numCh == 1)
    {
        buf = std::make_shared<juce::AudioBuffer<float>> (2, numSmp);
        reader->read (buf.get(), 0, numSmp, 0, true, false);
        buf->copyFrom (1, 0, *buf, 0, 0, numSmp);
    }

    return buf;
}

//==============================================================================
const VibeRegion* VibeSampleManager::findRegion (int midiNote, int velocity,
                                                   int articulationGroup)
{
    // Gather all candidates matching note + velocity + artic
    // Use indices to avoid iterator invalidation concerns
    std::vector<int> candidates;
    candidates.reserve (8);

    for (int i = 0; i < (int) mRegions.size(); ++i)
    {
        const auto& r = mRegions[i];
        if (midiNote  < r.loNote || midiNote  > r.hiNote)  continue;
        if (velocity  < r.loVel  || velocity  > r.hiVel)   continue;
        if (r.articulationGroup != articulationGroup)        continue;
        candidates.push_back (i);
    }

    if (candidates.empty()) return nullptr;

    // Check if regions have round-robin markers
    bool hasRR = false;
    for (int idx : candidates)
        if (mRegions[idx].roundRobinTotal > 0) { hasRR = true; break; }

    if (!hasRR)
        return &mRegions[candidates[0]]; // single region or no RR

    // Round-robin: pick the region whose roundRobinIndex matches the current counter
    const int noteKey    = juce::jlimit (0, 127, midiNote);
    const int articKey   = juce::jlimit (0, 3,   articulationGroup);
    int& counter         = mRRCounters[noteKey][articKey];

    // Find the total from the first RR region
    int rrTotal = mRegions[candidates[0]].roundRobinTotal;
    counter = (counter % rrTotal) + 1;  // SFZ seq_position is 1-based

    for (int idx : candidates)
        if (mRegions[idx].roundRobinIndex == counter)
            return &mRegions[idx];

    // Fallback: first candidate
    return &mRegions[candidates[0]];
}


// ─────────────────────────────────────────────────────────────────────────────
//  ReversedMemoryAudioSource (S1 Incr3 2026-04-21)
//   Reads a shared AudioBuffer in reverse. Position semantics match the forward
//   juce::MemoryAudioSource (0 = start of playback = last sample of buffer),
//   so sample-start seek works without special-casing the caller.
// ─────────────────────────────────────────────────────────────────────────────
namespace
{
class ReversedMemoryAudioSource : public juce::PositionableAudioSource
{
public:
    explicit ReversedMemoryAudioSource (const juce::AudioBuffer<float>& buf) noexcept
        : mBuf (&buf), mPosFwd (0) {}

    void prepareToPlay (int, double) override {}
    void releaseResources ()          override {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        const int total   = mBuf->getNumSamples();
        const int srcChs  = mBuf->getNumChannels();
        const int dstChs  = info.buffer->getNumChannels();
        // Bug fix 2026-04-21: wrap src channel via modulo so mono buffers
        //   feed both L and R of a stereo destination (matches juce::MemoryAudioSource
        //   behaviour). Without this, mono samples played in reverse only fed L.
        if (srcChs <= 0) return;

        for (int i = 0; i < info.numSamples; ++i)
        {
            const juce::int64 fwdPos  = mPosFwd + i;
            const juce::int64 readIdx = (juce::int64) total - 1 - fwdPos;
            if (readIdx < 0 || fwdPos >= (juce::int64) total)
            {
                for (int c = 0; c < dstChs; ++c)
                    info.buffer->setSample (c, info.startSample + i, 0.f);
            }
            else
            {
                for (int c = 0; c < dstChs; ++c)
                {
                    const int srcChan = c % srcChs;   // wrap mono -> L+R
                    info.buffer->setSample (c, info.startSample + i,
                                            mBuf->getSample (srcChan, (int) readIdx));
                }
            }
        }
        mPosFwd = juce::jmin ((juce::int64) total, mPosFwd + info.numSamples);
    }

    juce::int64 getNextReadPosition () const override                 { return mPosFwd; }
    void        setNextReadPosition (juce::int64 p)      override     { mPosFwd = juce::jlimit ((juce::int64) 0, (juce::int64) mBuf->getNumSamples(), p); }
    juce::int64 getTotalLength      () const override                 { return mBuf->getNumSamples(); }
    bool        isLooping           () const override                 { return false; }

private:
    const juce::AudioBuffer<float>* mBuf;
    juce::int64 mPosFwd;   // forward-time position (0 = first sample emitted)
};
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  VibeVoice
// ─────────────────────────────────────────────────────────────────────────────

VibeVoice::VibeVoice (VibeSampleManager& manager)
    : mManager (manager)
{
    mFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
}

void VibeVoice::prepareForPlayback (int blockSize)
{
    mBlockSize = blockSize;
    mTmpBuffer.setSize (2, blockSize, false, true, false);
}

VibeVoice::~VibeVoice() { releaseResources(); }

//==============================================================================
void VibeVoice::releaseResources()
{
    mResampSrc.reset();
    mMemSrc.reset();
    mSampleBuffer.reset();
    mIsPlaying = false;
}

//==============================================================================
void VibeVoice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
    mSampleRate = newRate;
    mAdsr.setSampleRate (newRate);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = newRate;
    spec.maximumBlockSize = (juce::uint32) mBlockSize;
    spec.numChannels      = 2;
    mFilter.prepare (spec);
    mFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    mFilter.setCutoffFrequency (20000.f);
    mFilter.setResonance (0.5f);
}

//==============================================================================
bool VibeVoice::canPlaySound (juce::SynthesiserSound* s)
{
    return dynamic_cast<VibeSynthSound*> (s) != nullptr;
}

//==============================================================================
void VibeVoice::startNote (int midiNote, float velocity,
                            juce::SynthesiserSound*,
                            int /*pitchWheelPos*/)
{
    releaseResources();

    const int velInt = juce::roundToInt (velocity * 127.f);
    const VibeRegion* region = mManager.findRegion (midiNote, velInt, mArticGroup);
    if (!region || !region->audioData) { clearCurrentNote(); return; }

    mSampleBuffer = region->audioData;

    // S1 Incr3 2026-04-21: reverse playback uses a file-local ReversedMemoryAudioSource
    // which reads the same shared buffer backward. Forward uses juce::MemoryAudioSource.
    if (mReverse)
        mMemSrc = std::make_unique<ReversedMemoryAudioSource> (*mSampleBuffer);
    else
        mMemSrc = std::make_unique<juce::MemoryAudioSource> (*mSampleBuffer, false, false);

    // Sample-start skip-into (S1 2026-04-21) applied to the source before resampling.
    // Positions are expressed in forward-time: the reverse source maps them internally.
    if (mSampleStart > 0.0f && mSampleBuffer->getNumSamples() > 0)
    {
        const juce::int64 startPos = (juce::int64)
            (juce::jlimit (0.f, 0.999f, mSampleStart) * (float) mSampleBuffer->getNumSamples());
        mMemSrc->setNextReadPosition (startPos);
    }

    // ResamplingAudioSource handles pitch shifting
    // ratio = (fileSampleRate / playbackSampleRate) * pitchRatio
    // S1 2026-04-21: global tune (semitones) + per-voice unison cents (set by VibeSynth fan-out).
    //  mNextUnisonCents already encodes (mDetune via mode) + (unisonSpread) per voice; consumed on startNote.
    const double pitchSemitones = (double) (midiNote - region->rootNote)
                                + (double) region->tuneOffset
                                + (double) mTune
                                + (double) mNextUnisonCents / 100.0;
    mNextUnisonCents = 0.0f;  // consume the tag
    const double pitchRatio = std::pow (2.0, pitchSemitones / 12.0);
    const double resampRatio = (region->fileSampleRate / mSampleRate) * pitchRatio
                               * (double) juce::jlimit (0.5f, 2.0f, mStretch);

    mResampSrc = std::make_unique<juce::ResamplingAudioSource> (mMemSrc.get(), false, 2);
    mResampSrc->setResamplingRatio (resampRatio);
    mResampSrc->prepareToPlay (mBlockSize, mSampleRate);

    // Sensitivity: reshape velocity curve (0→flat, 1→very responsive)
    const float sensExp = 2.0f - mSensitivity * 1.8f; // 0→2.0, 0.5→1.1, 1→0.2
    const float adjustedVelocity = std::pow (juce::jlimit (0.001f, 1.f, velocity), sensExp);

    // Per-note velocity + region volume offset (kept separate from mVolume).
    // S1 2026-04-21: VEL->VOLUME amount crossfades between flat (1.0) and full velocity scaling.
    //   mVelToVolume = 1 => adjustedVelocity (v1 behaviour)
    //   mVelToVolume = 0 => 1.0 (velocity has no effect on volume)
    const float volVelMix = (1.0f - mVelToVolume) + mVelToVolume * adjustedVelocity;
    mVelocityScale = juce::Decibels::decibelsToGain (region->volumeOffset) * volVelMix;

    // Muffle: reduce filter cutoff. MUFFLE is a static control; VEL→MUFFLE routes
    // velocity so that louder hits bypass some of the muffle (brighter on loud hits).
    {
        const float velFactor = 1.0f - mVelToMuffle * adjustedVelocity;
        mNoteCutoffBias = mMuffle * velFactor * (mBaseCutoff - 200.f);
    }
    // Hardness: HARDNESS is a base resonance control (always active). VEL→HARDNESS
    // scales it with velocity so that louder hits are harder-sounding.
    {
        const float velScale = 1.0f - mVelToHardness * (1.0f - adjustedVelocity);
        mNoteResBias = mHardness * velScale;
    }

    mFilter.reset();
    mAdsr.noteOn();
    mLfoPhase  = 0.0;
    mReductHold = 0;
    mReductStep = 0;
    mIsPlaying  = true;

    // S1 Incr3: monotonic age stamp for voiceCap-based oldest-first stealing.
    static juce::uint32 sGlobalNoteCounter = 0;
    mNoteStartCounter = ++sGlobalNoteCounter;
}

//==============================================================================
void VibeVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        mAdsr.noteOff();
    }
    else
    {
        clearCurrentNote();
        releaseResources();
    }
}

//==============================================================================
void VibeVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                  int startSample, int numSamples)
{
    if (!mIsPlaying || !mResampSrc) return;

    // Ensure temp buffer is big enough (2ch, numSamples)
    mTmpBuffer.setSize (2, numSamples, false, false, true);
    mTmpBuffer.clear();

    // Read from resampled source
    juce::AudioSourceChannelInfo info (&mTmpBuffer, 0, numSamples);
    mResampSrc->getNextAudioBlock (info);

    // ── ADSR ─────────────────────────────────────────────────────────────────
    mAdsr.applyEnvelopeToBuffer (mTmpBuffer, 0, numSamples);

    if (!mAdsr.isActive())
    {
        clearCurrentNote();
        releaseResources();
        return;
    }

    // ── Apply per-note filter bias (muffle + hardness + Batch E #2 CC74) ────
    if (mNoteCutoffBias > 0.001f || mNoteResBias > 0.001f
        || mPerNoteCutoffOctaves != 0.0f)
    {
        float effCutoff = juce::jlimit (20.f, 20000.f, mBaseCutoff - mNoteCutoffBias);
        if (mPerNoteCutoffOctaves != 0.0f)
            effCutoff = juce::jlimit (20.f, 20000.f,
                                       effCutoff * std::pow (2.0f, mPerNoteCutoffOctaves));
        const float effRes    = juce::jlimit (0.5f, 10.0f, mBaseRes + mNoteResBias * 9.5f);
        mFilter.setCutoffFrequency (effCutoff);
        mFilter.setResonance       (effRes);
    }

    // ── Vibrato/shimmer LFO ───────────────────────────────────────────────────
    if (mLfoAmt > 0.001f)
    {
        const double phaseInc = (juce::MathConstants<double>::twoPi * mLfoRate) / mSampleRate;
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = mTmpBuffer.getWritePointer (ch);
            double phase = mLfoPhase;
            for (int i = 0; i < numSamples; ++i)
            {
                const float mod = 1.f + mLfoAmt * 0.12f * (float) std::sin (phase);
                d[i] *= mod;
                phase += phaseInc;
            }
        }
        mLfoPhase = std::fmod (mLfoPhase + phaseInc * numSamples,
                               juce::MathConstants<double>::twoPi);
    }

    // ── Drive (tanh waveshaper) ───────────────────────────────────────────────
    if (mDrive > 1.001f)
    {
        const float tanhDrive = std::tanh (mDrive);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = mTmpBuffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                d[i] = std::tanh (d[i] * mDrive) / tanhDrive;
        }
    }

    // ── Sample-rate reduction (REDUCT) ────────────────────────────────────────
    if (mReduct > 0.01f)
    {
        const int holdN = 1 + juce::roundToInt (mReduct * 15.f); // 1–16 samples hold
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = mTmpBuffer.getWritePointer (ch);
            float held = d[0];
            int   step = mReductStep;
            for (int i = 0; i < numSamples; ++i)
            {
                if (step == 0) held = d[i];
                d[i] = held;
                step = (step + 1) % holdN;
            }
            if (ch == 0) mReductStep = step;
        }
    }

    // ── SVF filter ────────────────────────────────────────────────────────────
    {
        juce::dsp::AudioBlock<float>              block (mTmpBuffer);
        juce::dsp::ProcessContextReplacing<float> ctx   (block);
        mFilter.process (ctx);
    }

    // ── Mix into output (volume + velocity scale + pan) ──────────────────────
    const float gain   = mVolume * mVelocityScale;
    const int   outChs = outputBuffer.getNumChannels();
    if (outChs >= 2)
    {
        outputBuffer.addFrom (0, startSample, mTmpBuffer, 0, 0, numSamples, gain * mPanL);
        outputBuffer.addFrom (1, startSample, mTmpBuffer, 1, 0, numSamples, gain * mPanR);
    }
    else
    {
        outputBuffer.addFrom (0, startSample, mTmpBuffer, 0, 0, numSamples, gain);
    }
}

//==============================================================================
// Parameter setters — called from VibeSynth::forEachVoice.
// CPU-safe: voices cache their own values; VibeSynth guards before broadcast.
void VibeVoice::setFilterParams (float cutoffHz, float q) noexcept
{
    mBaseCutoff = juce::jlimit (20.f, 20000.f, cutoffHz);
    mBaseRes    = juce::jlimit (0.5f, 10.0f,  q);
    // Apply base values immediately (per-note bias is layered on top in renderNextBlock)
    mFilter.setCutoffFrequency (mBaseCutoff);
    mFilter.setResonance       (mBaseRes);
}

void VibeVoice::setDrive         (float drive)    noexcept { mDrive        = juce::jmax (1.f, drive); }
void VibeVoice::setReduct        (float reduct)   noexcept { mReduct       = juce::jlimit (0.f, 1.f, reduct); }
void VibeVoice::setVolume        (float vol)      noexcept { mVolume       = juce::jmax (0.f, vol); }
void VibeVoice::setLfoAmt        (float amt)      noexcept { mLfoAmt       = juce::jlimit (0.f, 1.f, amt); }
void VibeVoice::setStretch       (float stretch)  noexcept { mStretch      = juce::jlimit (0.5f, 2.f, stretch); }
void VibeVoice::setMuffle        (float muffle)   noexcept { mMuffle       = juce::jlimit (0.f, 1.f, muffle); }
void VibeVoice::setVelToMuffle   (float amt)      noexcept { mVelToMuffle  = juce::jlimit (0.f, 1.f, amt); }
void VibeVoice::setHardness      (float hardness) noexcept { mHardness     = juce::jlimit (0.f, 1.f, hardness); }
void VibeVoice::setVelToHardness (float amt)      noexcept { mVelToHardness= juce::jlimit (0.f, 1.f, amt); }
void VibeVoice::setSensitivity   (float sens)     noexcept { mSensitivity  = juce::jlimit (0.f, 1.f, sens); }

// S1 2026-04-21 setters
void VibeVoice::setTune          (float semitones) noexcept { mTune         = juce::jlimit (-48.f, 48.f, semitones); }
void VibeVoice::setVelToVolume   (float amt)       noexcept { mVelToVolume  = juce::jlimit (0.f, 1.f, amt); }
void VibeVoice::setSampleStart   (float norm)      noexcept { mSampleStart  = juce::jlimit (0.f, 1.f, norm); }
void VibeVoice::setLfoRate       (float hz)        noexcept { mLfoRate      = juce::jlimit (0.1f, 20.f, hz); }

void VibeVoice::setPan (float pan) noexcept
{
    // pan: -1 = hard left, 0 = centre, +1 = hard right
    const float p   = juce::jlimit (-1.f, 1.f, pan);
    const float ang = (p + 1.f) * juce::MathConstants<float>::halfPi * 0.5f;
    mPanL = std::cos (ang);
    mPanR = std::sin (ang);
}

void VibeVoice::setAdsr (float a, float d, float s, float r) noexcept
{
    juce::ADSR::Parameters p;
    p.attack  = juce::jmax (0.001f, a);
    p.decay   = juce::jmax (0.001f, d);
    p.sustain = juce::jlimit (0.f, 1.f, s);
    p.release = juce::jmax (0.001f, r);
    mAdsr.setParameters (p);
}


// ─────────────────────────────────────────────────────────────────────────────
//  VibeSynth
// ─────────────────────────────────────────────────────────────────────────────

VibeSynth::VibeSynth()
{
    mSynth.addSound (new VibeSynthSound());
    for (int i = 0; i < kMaxVoices; ++i)
        mSynth.addVoice (new VibeVoice (mManager));
}

//==============================================================================
void VibeSynth::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate;
    mSynth.setCurrentPlaybackSampleRate (sampleRate);

    // Prepare each voice's temp buffer via public method
    forEachVoice ([maxBlockSize] (VibeVoice& v)
    {
        v.prepareForPlayback (maxBlockSize);
    });
}

//==============================================================================
void VibeSynth::renderNextBlock (juce::AudioBuffer<float>& buffer,
                                  juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // S1 Incr3 2026-04-21: intercept note-ons for cut-self + unison fan-out + voiceCap.
    //   Note-offs, CCs and pitch-bend pass through unchanged.
    //   Timing is block-granular (accepted jitter of up to one block length for triggered voices).
    //
    // Bug fix 2026-04-21 (revised): per-pitch preempt + pre-noteon note-off strip
    //   are now always-on (not gated on cut-self). When we manually allocate a
    //   voice via mSynth.noteOn for a note-on at sample position T, any note-off
    //   for the SAME pitch at position <=T in the same block would fire against
    //   the fresh voice and kill it immediately. Strip those. Note-offs at
    //   positions >T belong to the freshly-allocated note and pass through.
    //
    //   Pre-scan: record the LATEST note-on sample position per MIDI pitch.
    int lastNoteOnPerPitch[128];
    std::fill_n (lastNoteOnPerPitch, 128, -1);
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (msg.isNoteOn())
        {
            const int p = msg.getNoteNumber();
            if (p >= 0 && p < 128 && m.samplePosition > lastNoteOnPerPitch[p])
                lastNoteOnPerPitch[p] = m.samplePosition;
        }
    }

    juce::MidiBuffer filteredMidi;

    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();

        if (msg.isNoteOn())
        {
            const int   note = msg.getNoteNumber();
            const float vel  = msg.getFloatVelocity();
            const int   ch   = msg.getChannel();

            // ── Same-pitch preemption (bug fix 2026-04-21, revised) ─────────
            // Soft-stop any voice already playing this pitch (tail-off via
            // ADSR release — avoids clicks on tonal sources like violin).
            // Combined with the per-pitch note-off strip below, this prevents
            // juce::Synthesiser's note-off matching from cascading onto a
            // newly allocated voice for the same pitch. Standard sampler
            // behaviour (Kontakt / HALion / FL Sampler all do this).
            for (int vi = 0; vi < mSynth.getNumVoices(); ++vi)
            {
                if (auto* vv = dynamic_cast<VibeVoice*> (mSynth.getVoice (vi)))
                {
                    if (vv->isVoiceActive() && vv->getCurrentlyPlayingNote() == note)
                        vv->stopNote (0.f, true);   // soft stop (ADSR release)
                }
            }

            // ── Cut-self: hard-stop ALL voices (any pitch) before the new note ──
            if (mCutSelf)
                mSynth.allNotesOff (0, false);

            // ── Unison fan-out: N voices per note-on with per-voice cents ───
            const int   N           = juce::jlimit (1, 8, mUnisonVoices);
            const float detCents    = mLastDetune  == 9999.f ? 0.f : mLastDetune;
            const int   detMode     = juce::jlimit (0, 2, mLastDetuneMode < 0 ? 0 : mLastDetuneMode);
            const float spreadCents = mUnisonSpread;
            const int   cap         = mLastVoiceCap > 0 ? mLastVoiceCap : kMaxVoices;

            for (int i = 0; i < N; ++i)
            {
                // Per-voice detune contribution based on detuneMode
                float detPart = 0.f;
                switch (detMode)
                {
                    case 0: // simple: all voices get the same global offset
                        detPart = detCents;
                        break;
                    case 1: // random: each voice gets a random value in [-|det|, +|det|]
                        detPart = (mRng.nextFloat() * 2.f - 1.f) * std::abs (detCents);
                        break;
                    case 2: // pair: symmetric split across voices
                        detPart = (N <= 1) ? 0.f
                                            : (((float) i / (float) (N - 1)) * 2.f - 1.f) * detCents;
                        break;
                }

                // Unison-spread contribution: symmetric across the voice stack
                const float spreadPart = (N <= 1) ? 0.f
                                                   : (((float) i / (float) (N - 1)) * 2.f - 1.f) * spreadCents;

                const float cents_i = detPart + spreadPart;

                // Voice-cap enforcement: steal oldest active voice if at capacity
                int           activeCount = 0;
                VibeVoice*    oldest      = nullptr;
                juce::uint32  oldestAge   = std::numeric_limits<juce::uint32>::max();
                for (int vi = 0; vi < mSynth.getNumVoices(); ++vi)
                {
                    if (auto* vv = dynamic_cast<VibeVoice*> (mSynth.getVoice (vi)))
                    {
                        if (vv->isVoiceActive())
                        {
                            ++activeCount;
                            const auto age = vv->getNoteStartCounter();
                            if (age < oldestAge) { oldestAge = age; oldest = vv; }
                        }
                    }
                }
                if (activeCount >= cap && oldest)
                    oldest->stopNote (0.f, false);

                // Pre-tag all voices with cents_i — juce::Synthesiser::noteOn will
                // allocate exactly one idle voice and call startNote synchronously,
                // which consumes mNextUnisonCents and resets it to 0.
                forEachVoice ([cents_i] (VibeVoice& v) { v.setNextUnisonCents (cents_i); });

                mSynth.noteOn (ch, note, vel);
            }
        }
        else
        {
            // Per-pitch note-off strip (always on — paired with per-pitch preempt).
            // Drop any note-off whose pitch has a later-or-equal note-on in the
            // same block: that stale note-off would kill the fresh voice.
            if (msg.isNoteOff())
            {
                const int p = msg.getNoteNumber();
                if (p >= 0 && p < 128
                    && lastNoteOnPerPitch[p] >= 0
                    && metadata.samplePosition <= lastNoteOnPerPitch[p])
                {
                    continue;
                }
            }
            // Pass through note-offs (post-last-noteon for their pitch), CCs, pitch-bend etc.
            filteredMidi.addEvent (msg, metadata.samplePosition);
        }
    }

    mSynth.renderNextBlock (buffer, filteredMidi, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    const bool isStereo  = buffer.getNumChannels() >= 2;

    // ── Treble shelf (one-pole high-shelf approximation) ─────────────────────
    if (std::abs (mTrebleGain) > 0.001f && isStereo)
    {
        // alpha for one-pole LP at ~8kHz
        const float omega = 2.0f * juce::MathConstants<float>::pi * 8000.f
                            / (float) mSampleRate;
        const float alpha = omega / (1.0f + omega);   // one-pole coefficient

        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            float lp = mTrebleLp[ch];
            for (int i = 0; i < numSamples; ++i)
            {
                lp    += alpha * (d[i] - lp);    // one-pole LP
                d[i]  += mTrebleGain * (d[i] - lp);  // add / cut high component
            }
            mTrebleLp[ch] = lp;
        }
    }

    // ── Stereo M/S width ──────────────────────────────────────────────────────
    if (isStereo && mStereoWidth != 1.0f)
    {
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getWritePointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float mid  = (L[i] + R[i]) * 0.5f;
            const float side = (L[i] - R[i]) * 0.5f * mStereoWidth;
            L[i] = mid + side;
            R[i] = mid - side;
        }
    }
}

//==============================================================================
void VibeSynth::allNotesOff()
{
    for (int ch = 1; ch <= 16; ++ch)
        mSynth.allNotesOff (ch, true);
}

//==============================================================================
// CPU-guarded parameter setters — only push to voices when value actually changed.

void VibeSynth::setFilterParams (float cutoffHz, float resonance) noexcept
{
    if (cutoffHz == mLastCutoff && resonance == mLastRes) return;
    mLastCutoff = cutoffHz;
    mLastRes    = resonance;
    const float q = 0.5f + resonance * 9.5f; // map 0-1 → Q 0.5-10
    forEachVoice ([cutoffHz, q] (VibeVoice& v) { v.setFilterParams (cutoffHz, q); });
}

void VibeSynth::setDrive (float drive) noexcept
{
    if (drive == mLastDrive) return;
    mLastDrive = drive;
    // Map 0-1 → drive factor 1-12 (1 = unity, 12 = heavy saturation)
    const float d = 1.f + drive * 11.f;
    forEachVoice ([d] (VibeVoice& v) { v.setDrive (d); });
}

void VibeSynth::setReduct (float reduct) noexcept
{
    if (reduct == mLastReduct) return;
    mLastReduct = reduct;
    forEachVoice ([reduct] (VibeVoice& v) { v.setReduct (reduct); });
}

void VibeSynth::setVolume (float vol) noexcept
{
    if (vol == mLastVol) return;
    mLastVol = vol;
    forEachVoice ([vol] (VibeVoice& v) { v.setVolume (vol); });
}

void VibeSynth::setPan (float pan) noexcept
{
    if (pan == mLastPan) return;
    mLastPan = pan;
    forEachVoice ([pan] (VibeVoice& v) { v.setPan (pan); });
}

void VibeSynth::setAdsr (float a, float d, float s, float r) noexcept
{
    if (a == mLastAmpA && d == mLastAmpD && s == mLastAmpS && r == mLastAmpR) return;
    mLastAmpA = a; mLastAmpD = d; mLastAmpS = s; mLastAmpR = r;
    forEachVoice ([a, d, s, r] (VibeVoice& v) { v.setAdsr (a, d, s, r); });
}

void VibeSynth::setLfoAmt (float amt) noexcept
{
    if (amt == mLastLfoAmt) return;
    mLastLfoAmt = amt;
    forEachVoice ([amt] (VibeVoice& v) { v.setLfoAmt (amt); });
}

void VibeSynth::setStereo (float width) noexcept
{
    if (width == mLastStereo) return;
    mLastStereo   = width;
    mStereoWidth  = width;
}

void VibeSynth::setTreble (float treble) noexcept
{
    if (treble == mLastTreble) return;
    mLastTreble  = treble;
    // Bug fix 2026-04-21: APVTS range is -12..+12 (centered 0 = flat).
    // Map linearly to gain multiplier for high component:
    //   treble = 0    → mTrebleGain = 0   (flat — no boost/cut)
    //   treble = +12  → mTrebleGain = +1  (~+6 dB shelf at 8 kHz)
    //   treble = -12  → mTrebleGain = -1  (full high cut — cancels highs)
    mTrebleGain  = juce::jlimit (-1.f, 1.f, treble / 12.f);
}

void VibeSynth::setStretch (float stretch) noexcept
{
    if (stretch == mLastStretch) return;
    mLastStretch = stretch;
    forEachVoice ([stretch] (VibeVoice& v) { v.setStretch (stretch); });
}

void VibeSynth::setMuffle (float muffle) noexcept
{
    if (muffle == mLastMuffle) return;
    mLastMuffle = muffle;
    forEachVoice ([muffle] (VibeVoice& v) { v.setMuffle (muffle); });
}

void VibeSynth::setVelToMuffle (float amt) noexcept
{
    if (amt == mLastVelToMuffle) return;
    mLastVelToMuffle = amt;
    forEachVoice ([amt] (VibeVoice& v) { v.setVelToMuffle (amt); });
}

void VibeSynth::setHardness (float hardness) noexcept
{
    if (hardness == mLastHardness) return;
    mLastHardness = hardness;
    forEachVoice ([hardness] (VibeVoice& v) { v.setHardness (hardness); });
}

void VibeSynth::setVelToHardness (float amt) noexcept
{
    if (amt == mLastVelToHard) return;
    mLastVelToHard = amt;
    forEachVoice ([amt] (VibeVoice& v) { v.setVelToHardness (amt); });
}

void VibeSynth::setSensitivity (float sens) noexcept
{
    if (sens == mLastSensitivity) return;
    mLastSensitivity = sens;
    forEachVoice ([sens] (VibeVoice& v) { v.setSensitivity (sens); });
}

void VibeSynth::setArticulationGroup (int group) noexcept
{
    if (group == mLastArtic) return;
    mLastArtic = group;
    forEachVoice ([group] (VibeVoice& v) { v.setArticulationGroup (group); });
}

// S1 2026-04-21 CPU-guarded forwarders
void VibeSynth::setTune (float semitones) noexcept
{
    if (semitones == mLastTune) return;
    mLastTune = semitones;
    forEachVoice ([semitones] (VibeVoice& v) { v.setTune (semitones); });
}

// S1 Incr3 2026-04-21: detune + detuneMode are authoritative on VibeSynth only
// (used to compute per-voice mNextUnisonCents during fan-out). No voice-level state.
void VibeSynth::setDetune (float cents) noexcept
{
    if (cents == mLastDetune) return;
    mLastDetune = cents;
}

void VibeSynth::setDetuneMode (int mode) noexcept
{
    if (mode == mLastDetuneMode) return;
    mLastDetuneMode = mode;
}

void VibeSynth::setVelToVolume (float amt) noexcept
{
    if (amt == mLastVelToVolume) return;
    mLastVelToVolume = amt;
    forEachVoice ([amt] (VibeVoice& v) { v.setVelToVolume (amt); });
}

void VibeSynth::setSampleStart (float norm) noexcept
{
    if (norm == mLastSampleStart) return;
    mLastSampleStart = norm;
    forEachVoice ([norm] (VibeVoice& v) { v.setSampleStart (norm); });
}

void VibeSynth::setLfoRate (float hz) noexcept
{
    if (hz == mLastLfoRate) return;
    mLastLfoRate = hz;
    forEachVoice ([hz] (VibeVoice& v) { v.setLfoRate (hz); });
}

void VibeSynth::setVoiceCap (int cap) noexcept
{
    // Enforcement happens in renderNextBlock manual dispatch (oldest-first steal).
    if (cap == mLastVoiceCap) return;
    mLastVoiceCap = juce::jlimit (1, kMaxVoices, cap);
}

void VibeSynth::setCutSelf (bool on) noexcept
{
    mCutSelf = on;
}

void VibeSynth::setReverse (bool rev) noexcept
{
    if (rev == mLastReverse) return;
    mLastReverse = rev;
    mReverse     = rev;
    forEachVoice ([rev] (VibeVoice& v) { v.setReverse (rev); });
}

void VibeSynth::setUnisonVoices (int n) noexcept
{
    const int clamped = juce::jlimit (1, 8, n);
    if (clamped == mLastUnisonVoices) return;
    mLastUnisonVoices = clamped;
    mUnisonVoices     = clamped;
}

void VibeSynth::setUnisonSpread (float cents) noexcept
{
    const float clamped = juce::jlimit (0.f, 200.f, cents);
    if (clamped == mLastUnisonSpread) return;
    mLastUnisonSpread = clamped;
    mUnisonSpread     = clamped;
}
