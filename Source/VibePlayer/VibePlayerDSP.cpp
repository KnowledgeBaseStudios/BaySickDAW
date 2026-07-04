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
    resetKeyswitchState();
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
// SFZ parser - 4-level cascading inheritance per SFZ v1 spec.
// Format: opcodes as key=value pairs after a <global>/<master>/<group>/<region>
// header.  Opcodes inside a parent scope accumulate into that scope's default
// state and are inherited by every <region> that follows until the next
// same-or-higher scope header.  <control> is orthogonal (default_path only).
// Comments: // to end of line.
void VibeSampleManager::parseSFZ (const juce::File& sfzFile)
{
    const juce::File sfzDir = sfzFile.getParentDirectory();
    juce::StringArray lines;
    lines.addLines (sfzFile.loadFileAsString());

    enum class Scope { None, Global, Master, Group, Region };

    struct SfzParseState
    {
        Scope scope { Scope::None };
        bool  inControl { false };
        VibeRegion globalDefaults;
        VibeRegion masterDefaults;
        VibeRegion groupDefaults;
        VibeRegion current;

        // Sub-O scope-priority sw_default init: first-wins per scope.
        // captureExplicitSwDefault writes here when sw_default appears in the
        // opcode stream; getInitialSwLast resolves the priority chain at
        // end-of-parseSFZ to seed VibeSampleManager::mActiveSwLast.
        int explicitGlobalSwDefault { -1 };
        int explicitMasterSwDefault { -1 };
        int explicitGroupSwDefault  { -1 };
        int firstRegionSwDefault    { -1 };

        void enterGlobal()  { globalDefaults = VibeRegion{};
                              masterDefaults = globalDefaults;
                              groupDefaults  = masterDefaults;
                              scope = Scope::Global;  inControl = false; }
        void enterMaster()  { masterDefaults = globalDefaults;
                              groupDefaults  = masterDefaults;
                              scope = Scope::Master;  inControl = false; }
        void enterGroup()   { groupDefaults = masterDefaults;
                              scope = Scope::Group;   inControl = false; }
        void enterRegion()  { current = groupDefaults;
                              scope = Scope::Region;  inControl = false; }
        void enterControl() { scope = Scope::None;    inControl = true;  }
        void exitOther()    { scope = Scope::None;    inControl = false; }

        // Push the current region into the output vector if it has audio data,
        // then reset current.  Safe to call on any header transition.
        void tryFlushRegion (std::vector<VibeRegion>& regions)
        {
            if (scope == Scope::Region && current.audioData)
                regions.push_back (current);
            current = VibeRegion{};
        }

        // Return pointer to the VibeRegion that subsequent opcodes should write
        // to, based on current scope.  Returns nullptr outside any scope (e.g.
        // bare opcodes before the first header, or while inControl).
        VibeRegion* currentTarget()
        {
            switch (scope)
            {
                case Scope::Region: return &current;
                case Scope::Group:  return &groupDefaults;
                case Scope::Master: return &masterDefaults;
                case Scope::Global: return &globalDefaults;
                default:            return nullptr;
            }
        }

        // Sub-O: record sw_default at the current scope.  First-wins per scope:
        // subsequent same-scope sw_default opcodes don't override (so multiple
        // <group> blocks with their own sw_default leave the first-set value
        // intact for the post-parse priority lookup).
        void captureExplicitSwDefault (int n)
        {
            switch (scope)
            {
                case Scope::Global: if (explicitGlobalSwDefault == -1) explicitGlobalSwDefault = n; break;
                case Scope::Master: if (explicitMasterSwDefault == -1) explicitMasterSwDefault = n; break;
                case Scope::Group:  if (explicitGroupSwDefault  == -1) explicitGroupSwDefault  = n; break;
                case Scope::Region: if (firstRegionSwDefault    == -1) firstRegionSwDefault    = n; break;
                default: break;
            }
        }

        // Sub-O: post-parse priority resolution.  Global > Master > Group >
        // first Region.  -1 if no sw_default set anywhere.
        int getInitialSwLast() const
        {
            if (explicitGlobalSwDefault != -1) return explicitGlobalSwDefault;
            if (explicitMasterSwDefault != -1) return explicitMasterSwDefault;
            if (explicitGroupSwDefault  != -1) return explicitGroupSwDefault;
            if (firstRegionSwDefault    != -1) return firstRegionSwDefault;
            return -1;
        }
    };

    SfzParseState state;
    juce::String defaultPath;   // from <control> default_path= - prepended to all sample= values

    for (const auto& rawLine : lines)
    {
        // Strip comment
        juce::String line = rawLine.upToFirstOccurrenceOf ("//", false, false).trim();
        if (line.isEmpty()) continue;

        // Header dispatch.  Order matters: check <region> first because the
        // string "<region>" trivially contains '<'.  Same for the others.
        if (line.containsIgnoreCase ("<region>"))
        {
            state.tryFlushRegion (mRegions);
            state.enterRegion();
            // Opcodes can be on same line as <region>
            line = line.fromFirstOccurrenceOf ("<region>", false, true).trim();
        }
        else if (line.containsIgnoreCase ("<group>"))
        {
            state.tryFlushRegion (mRegions);
            state.enterGroup();
            continue;
        }
        else if (line.containsIgnoreCase ("<master>"))
        {
            state.tryFlushRegion (mRegions);
            state.enterMaster();
            continue;
        }
        else if (line.containsIgnoreCase ("<global>"))
        {
            state.tryFlushRegion (mRegions);
            state.enterGlobal();
            continue;
        }
        else if (line.containsIgnoreCase ("<control>"))
        {
            state.tryFlushRegion (mRegions);
            state.enterControl();
            continue;
        }
        else if (line.startsWithChar ('<'))
        {
            // Unknown header - flush + clear scope so subsequent opcodes are ignored
            state.tryFlushRegion (mRegions);
            state.exitOther();
            continue;
        }

        // Inside <control>: only care about default_path (may contain spaces)
        if (state.inControl)
        {
            auto v = sfzOpcode (line, "default_path", true);
            if (v.isNotEmpty())
                defaultPath = v.replace ("\\", "/");
            continue;
        }

        // Opcode dispatch - single target-pointer per line via helper.
        // Writes go to current region OR the active scope's defaults
        // accumulator; on the next enterRegion() the accumulator is copied
        // into current as the new region's baseline (the inheritance step).
        auto* t = state.currentTarget();
        if (! t) continue;

        // sample= (read to EOL - path may contain spaces)
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
                    t->audioData      = loadFile (f, mFormatManager, sr);
                    t->sampleFile     = f;
                    t->fileSampleRate = sr;
                }
            }
        }

        // key= (shorthand: sets lokey, hikey, pitch_keycenter)
        {
            auto v = sfzOpcode (line, "key");
            if (v.isNotEmpty())
            {
                int n = sfzNote (v);
                t->loNote   = n;
                t->hiNote   = n;
                t->rootNote = n;
            }
        }

        // lokey / hikey
        {
            auto v = sfzOpcode (line, "lokey");
            if (v.isNotEmpty()) t->loNote = sfzNote (v);
        }
        {
            auto v = sfzOpcode (line, "hikey");
            if (v.isNotEmpty()) t->hiNote = sfzNote (v);
        }

        // pitch_keycenter
        {
            auto v = sfzOpcode (line, "pitch_keycenter");
            if (v.isNotEmpty()) t->rootNote = sfzNote (v);
        }

        // lovel / hivel
        {
            auto v = sfzOpcode (line, "lovel");
            if (v.isNotEmpty()) t->loVel = v.getIntValue();
        }
        {
            auto v = sfzOpcode (line, "hivel");
            if (v.isNotEmpty()) t->hiVel = v.getIntValue();
        }

        // tune= (cents -> semitones)
        {
            auto v = sfzOpcode (line, "tune");
            if (v.isNotEmpty()) t->tuneOffset = v.getFloatValue() / 100.f;
        }

        // volume= (dB)
        {
            auto v = sfzOpcode (line, "volume");
            if (v.isNotEmpty()) t->volumeOffset = v.getFloatValue();
        }

        // group= (maps to articulationGroup 0-3)
        {
            auto v = sfzOpcode (line, "group");
            if (v.isNotEmpty())
                t->articulationGroup = juce::jlimit (0, 3, v.getIntValue() - 1);
        }

        // seq_position / seq_length (round-robin)
        {
            auto v = sfzOpcode (line, "seq_position");
            if (v.isNotEmpty()) t->roundRobinIndex = v.getIntValue();
        }
        {
            auto v = sfzOpcode (line, "seq_length");
            if (v.isNotEmpty()) t->roundRobinTotal = v.getIntValue();
        }

        // Sub-L: SFZ keyswitching opcodes.  sw_lokey/sw_hikey define the
        // explicit keyswitch range (used to populate mIsKeyswitch at end of
        // parseSFZ).  sw_last/sw_down/sw_up are the per-region active-state
        // filters (read by findRegion).  sw_default is the initial active
        // keyswitch (resolved via Sub-O scope-priority post-parse).
        {
            auto v = sfzOpcode (line, "sw_lokey");
            if (v.isNotEmpty()) t->swLokey = sfzNote (v);
        }
        {
            auto v = sfzOpcode (line, "sw_hikey");
            if (v.isNotEmpty()) t->swHikey = sfzNote (v);
        }
        {
            auto v = sfzOpcode (line, "sw_last");
            if (v.isNotEmpty()) t->swLast = sfzNote (v);
        }
        {
            auto v = sfzOpcode (line, "sw_down");
            if (v.isNotEmpty()) t->swDown = sfzNote (v);
        }
        {
            auto v = sfzOpcode (line, "sw_up");
            if (v.isNotEmpty()) t->swUp = sfzNote (v);
        }
        {
            auto v = sfzOpcode (line, "sw_default");
            if (v.isNotEmpty())
            {
                const int n = sfzNote (v);
                t->swDefault = n;
                state.captureExplicitSwDefault (n);
            }
        }
        // sw_label: human-readable name for the keyswitch (read to EOL since
        // the value can contain spaces, e.g. sw_label=C6 Sustain).
        {
            auto v = sfzOpcode (line, "sw_label", true);
            if (v.isNotEmpty()) t->swLabel = v;
        }
    }

    state.tryFlushRegion (mRegions); // final region

    // Sub-L/N post-parse: populate mIsKeyswitch from the union of
    // sw_lokey..sw_hikey ranges across all loaded regions.  Engine queries
    // isKeyswitchNote(midiNote) on every incoming MIDI note in
    // VibePlayerProcessor::processBlock to decide whether to dispatch to the
    // synth (playable note) or update keyswitch state (keyswitch note).
    for (const auto& r : mRegions)
    {
        if (r.swLokey >= 0 && r.swHikey >= 0)
        {
            const int lo = juce::jlimit (0, 127, r.swLokey);
            const int hi = juce::jlimit (0, 127, r.swHikey);
            for (int n = lo; n <= hi; ++n)
                mIsKeyswitch[(size_t) n] = true;
        }
    }

    // Sub-O scope-priority sw_default init.  Global > Master > Group > first
    // Region.  -1 if no sw_default set anywhere (keyswitching effectively
    // inert until the user presses a keyswitch note explicitly).
    mActiveSwLast = state.getInitialSwLast();

    // Sub-Q: populate mKeyswitchLabels from each region's (swLast/swDown/swUp,
    // swLabel) pair.  First-wins per keyswitch note - multiple regions can
    // share the same keyswitch (e.g. 4 staccato regions all with sw_last=c#6
    // + sw_label="C#6 Staccato"); the label they share is recorded once.
    for (const auto& r : mRegions)
    {
        if (r.swLabel.isEmpty()) continue;
        auto recordIfEmpty = [&] (int note)
        {
            if (note >= 0 && note < 128 && mKeyswitchLabels[(size_t) note].isEmpty())
                mKeyswitchLabels[(size_t) note] = r.swLabel;
        };
        recordIfEmpty (r.swLast);
        recordIfEmpty (r.swDown);
        recordIfEmpty (r.swUp);
    }
}

//==============================================================================
// Extract the value for a given opcode key from a line of SFZ text.
// Returns empty string if not found.
// readToEOL=true is used for path-type opcodes (sample=, default_path=) whose
// values may contain spaces - those read to end of line rather than stopping at
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
    // Gather all candidates matching note + velocity + artic.  Use indices to
    // avoid iterator invalidation concerns.
    //
    // QA-VoicePool Task 4 (L5=(a)): std::array<int, kMaxCandidates> stack-alloc
    // replaces the pre-batch std::vector<int> + reserve(8), eliminating the
    // per-note-on heap allocation on the audio thread.  Cap of 32 covers every
    // realistic sample mapping (typical SFZ packs ship 2-8 RR variations per
    // (note, velocity, articGroup) tuple; heavily-layered packs rarely exceed
    // 16).  Overflow silently drops the 33rd+ candidate per Jeff's L5=(a) lock
    // at Task 0 ExitPlanMode ("UX-acceptable degradation for an edge case that
    // should never fire on Jeff's libraries").
    constexpr int kMaxCandidates = 32;
    std::array<int, kMaxCandidates> candidates {};
    int numCandidates = 0;

    for (int i = 0; i < (int) mRegions.size(); ++i)
    {
        const auto& r = mRegions[i];

        // Sub-N: keyswitch filtering BEFORE note/velocity/articulationGroup.
        // Isolates sustain vs staccato candidate pools so the RR cycling logic
        // below sees a single-articulation pool only (natural divide-by-zero
        // fix for the mixed-pool crash that surfaced 2026-05-27 before
        // keyswitching landed).
        if (r.swLast != -1 && mActiveSwLast        != r.swLast) continue;
        if (r.swDown != -1 && ! mSwDownHeld[(size_t) r.swDown]) continue;
        if (r.swUp   != -1 &&   mSwDownHeld[(size_t) r.swUp])   continue;

        if (midiNote  < r.loNote || midiNote  > r.hiNote)  continue;
        if (velocity  < r.loVel  || velocity  > r.hiVel)   continue;
        if (r.articulationGroup != articulationGroup)        continue;
        if (numCandidates < kMaxCandidates)
            candidates[numCandidates++] = i;
    }

    if (numCandidates == 0) return nullptr;

    // Check if regions have round-robin markers
    bool hasRR = false;
    for (int j = 0; j < numCandidates; ++j)
        if (mRegions[candidates[j]].roundRobinTotal > 0) { hasRR = true; break; }

    if (!hasRR)
        return &mRegions[candidates[0]]; // single region or no RR

    // Round-robin: pick the region whose roundRobinIndex matches the current counter
    const int noteKey    = juce::jlimit (0, 127, midiNote);
    const int articKey   = juce::jlimit (0, 3,   articulationGroup);
    int& counter         = mRRCounters[noteKey][articKey];

    // Find the total from the first RR region
    int rrTotal = mRegions[candidates[0]].roundRobinTotal;
    counter = (counter % rrTotal) + 1;  // SFZ seq_position is 1-based

    for (int j = 0; j < numCandidates; ++j)
        if (mRegions[candidates[j]].roundRobinIndex == counter)
            return &mRegions[candidates[j]];

    // Fallback: first candidate
    return &mRegions[candidates[0]];
}

//==============================================================================
// SFZ keyswitching API (Sub-N: state lives on VibeSampleManager).
// Single-threaded - all calls come from the audio thread (processBlock or the
// findRegion-adjacent code paths).

bool VibeSampleManager::isKeyswitchNote (int midiNote) const noexcept
{
    if (midiNote < 0 || midiNote > 127) return false;
    return mIsKeyswitch[(size_t) midiNote];
}

void VibeSampleManager::handleKeyswitchNoteOn (int midiNote) noexcept
{
    if (midiNote < 0 || midiNote > 127) return;
    mActiveSwLast = midiNote;          // sw_last: last keyswitch pressed wins
    mSwDownHeld[(size_t) midiNote] = true;
}

void VibeSampleManager::handleKeyswitchNoteOff (int midiNote) noexcept
{
    if (midiNote < 0 || midiNote > 127) return;
    mSwDownHeld[(size_t) midiNote] = false;
    // sw_last is NOT cleared on release (sticky per SFZ v1 semantics - the
    // last-pressed keyswitch remains active until a new one is pressed).
}

void VibeSampleManager::resetKeyswitchState() noexcept
{
    mActiveSwLast    = -1;
    mSwDownHeld      = {};
    mIsKeyswitch     = {};   // parseSFZ rebuilds this on every load
    for (auto& s : mKeyswitchLabels) s.clear();
}

juce::String VibeSampleManager::getKeyswitchLabel (int midiNote) const noexcept
{
    if (midiNote < 0 || midiNote > 127) return {};
    return mKeyswitchLabels[(size_t) midiNote];
}


// QA-VoicePool Task 2 (2026-05-25): ReversedMemoryAudioSource + new sibling
// VibeForwardMemoryAudioSource moved to VibePlayerDSP.h so VibeVoice can own
// them as direct members for the fat-voice refactor.  No anon-namespace class
// here; both sources defined alongside VibeSynthSound in the header.

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

    // QA-VoicePool Task 2: hoist resampler prepareToPlay off the per-note path.
    // Both directions prepared on the message thread; startNote calls
    // flushBuffers() + setResamplingRatio() on the active one.
    mForwardResamp.prepareToPlay (blockSize, mSampleRate);
    mReverseResamp.prepareToPlay (blockSize, mSampleRate);
}

VibeVoice::~VibeVoice() { releaseResources(); }

//==============================================================================
void VibeVoice::releaseResources()
{
    // QA-VoicePool Task 2: fat sources stay allocated.  Per-note state is the
    // active-pointer pair + the shared_ptr to the region's audio buffer (so
    // VibeSampleManager can free its regions on reload once all voices drop
    // their references).
    mActiveSrc    = nullptr;
    mActiveResamp = nullptr;
    mSampleBuffer.reset();
    mIsPlaying    = false;

    // QA-VoicePool Task 3: per-note steal-state flags also clear here.  mAdsr
    // override state (mAdsrOverridden + mPreStealAdsrParams) is preserved across
    // releaseResources - startNote restores it on the next note allocation.
    mIsActive.store (false, std::memory_order_release);
    mInRelease = false;
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

    // QA-VoicePool Task 3: restore user's ADSR params if the previous note was
    // stolen with a quick-release override.  mPreStealAdsrParams holds either the
    // params at steal time OR any user-driven setAdsr updates that arrived during
    // the override (see VibeVoice::setAdsr).
    if (mAdsrOverridden)
    {
        mAdsr.setParameters (mPreStealAdsrParams);
        mAdsrOverridden = false;
    }

    const int velInt = juce::roundToInt (velocity * 127.f);
    const VibeRegion* region = mManager.findRegion (midiNote, velInt, mArticGroup);
    if (!region || !region->audioData) { clearCurrentNote(); return; }

    mSampleBuffer = region->audioData;

    // QA-VoicePool Task 2: re-point the chosen fat source at the new region's
    // buffer + select the matching permanent resampler.  No heap allocation on
    // the audio thread.  Reverse playback uses ReversedMemoryAudioSource which
    // reads the same buffer backward; forward uses VibeForwardMemoryAudioSource.
    if (mReverse)
    {
        mReverseSrc.setBuffer (*mSampleBuffer);
        mActiveSrc    = &mReverseSrc;
        mActiveResamp = &mReverseResamp;
    }
    else
    {
        mForwardSrc.setBuffer (*mSampleBuffer);
        mActiveSrc    = &mForwardSrc;
        mActiveResamp = &mForwardResamp;
    }

    // Sample-start skip-into (S1 2026-04-21) applied to the source before resampling.
    // Positions are expressed in forward-time: the reverse source maps them internally.
    if (mSampleStart > 0.0f && mSampleBuffer->getNumSamples() > 0)
    {
        const juce::int64 startPos = (juce::int64)
            (juce::jlimit (0.f, 0.999f, mSampleStart) * (float) mSampleBuffer->getNumSamples());
        mActiveSrc->setNextReadPosition (startPos);
    }

    // ResamplingAudioSource handles pitch shifting; per-note ratio drives transposition.
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

    // QA-VoicePool Task 2: flushBuffers() clears the resampler's per-channel
    // filter histories + readahead buffer; setResamplingRatio() updates the
    // pitch ratio.  prepareToPlay was hoisted to VibeVoice::prepareForPlayback.
    mActiveResamp->flushBuffers ();
    mActiveResamp->setResamplingRatio (resampRatio);

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

    // QA-VoicePool Task 3: flip supplemental active flag (Sub-A=(a)) for
    // findStealCandidate's atomic scan; clear release predicate (fresh note).
    mIsActive.store (true, std::memory_order_release);
    mInRelease = false;

    // S1 Incr3: monotonic age stamp for voiceCap-based oldest-first stealing.
    static juce::uint32 sGlobalNoteCounter = 0;
    mNoteStartCounter = ++sGlobalNoteCounter;
}

//==============================================================================
void VibeVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        // QA-VoicePool Task 3: flip release predicate so findStealCandidate's
        // L7(b) hybrid can prefer this voice (release-phase oldest).  Cleared
        // at ADSR-end via releaseResources in renderNextBlock's idle path.
        mInRelease = true;
        mAdsr.noteOff();
    }
    else
    {
        clearCurrentNote();
        releaseResources();  // also clears mIsActive + mInRelease (Task 3)
    }
}

//==============================================================================
void VibeVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                  int startSample, int numSamples)
{
    if (!mIsPlaying || mActiveResamp == nullptr) return;

    // Ensure temp buffer is big enough (2ch, numSamples)
    mTmpBuffer.setSize (2, numSamples, false, false, true);
    mTmpBuffer.clear();

    // Read from resampled source (QA-VoicePool Task 2: mActiveResamp points
    // to either mForwardResamp or mReverseResamp; set by startNote).
    juce::AudioSourceChannelInfo info (&mTmpBuffer, 0, numSamples);
    mActiveResamp->getNextAudioBlock (info);

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
// Parameter setters - called from VibeSynth::forEachVoice.
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
    // QA-VoicePool Task 3: if a steal-quick-release override is active, don't
    // disturb mAdsr's in-flight 1.5 ms release - save the new user setting so
    // startNote can restore it on the next note allocation instead.
    if (mAdsrOverridden)
        mPreStealAdsrParams = p;
    else
        mAdsr.setParameters (p);
}

// QA-VoicePool Task 3: initiateSteal saves the user's ADSR params and overrides
// mAdsr's release to ~1.5 ms (~64 samples @ 44.1 kHz / ~72 @ 48 kHz - "instantly
// fade it out" per Jeff's verbatim blueprint).  Caller (VibeSynth voiceCap branch)
// follows with stopNote(0.f, true) so the voice enters its quick-release naturally
// inside renderNextBlock - no synchronous render past the audio block boundary.
// Idempotent: re-stealing an already-overridden voice is a no-op (the in-flight
// quick-release continues without disturbance).
void VibeVoice::initiateSteal() noexcept
{
    if (mAdsrOverridden)
        return;

    mPreStealAdsrParams = mAdsr.getParameters();
    mAdsrOverridden     = true;

    juce::ADSR::Parameters quick = mPreStealAdsrParams;
    quick.release = 0.0015f;  // 1.5 ms - matches L6 64-sample target at 44.1 kHz
    mAdsr.setParameters (quick);
}


// ─────────────────────────────────────────────────────────────────────────────
//  VibeSynth
// ─────────────────────────────────────────────────────────────────────────────

VibeSynth::VibeSynth()
{
    // QA-0a (2026-05-07): set a placeholder sample rate before adding voices.
    // JUCE's Synthesiser::addVoice propagates the synth's current sample rate
    // to each new voice via setCurrentPlaybackSampleRate.  Without a non-zero
    // value here, every voice's ADSR + StateVariableTPTFilter trip Debug
    // asserts during construction (Release silently accepts 0).  prepare()
    // overwrites with the real rate before any audio processing.
    mSynth.setCurrentPlaybackSampleRate (44100.0);
    mSynth.addSound (new VibeSynthSound());
    // QA-VoicePool Task 3: physical pool over-provisioned to kMaxVoices (24) so
    // the steal quick-release fade-out lands on reserve slots; cache the
    // VibeVoice* into mVoices alongside mSynth.addVoice so the audio-thread
    // voiceCap scan can iterate without dynamic_cast per voice (Sub-A=(a)).
    for (int i = 0; i < kMaxVoices; ++i)
    {
        auto* v = new VibeVoice (mManager);
        mSynth.addVoice (v);
        mVoices[i] = v;
    }
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

// QA-VoicePool Task 3: L7(b) 3-tier hybrid steal selection (refined 2026-05-25
// after Jeff caught the 2-tier original's "held lead note stolen by looping
// chord" pathology - the overall-oldest fallback eventually picked the
// sustained lead because it was the oldest active voice, even though it was
// still being physically held).  Scans cached mVoices[] (no dynamic_cast per
// voice - Sub-A=(a) rationale) with a 3-tier priority hierarchy.  Within each
// tier, pick the oldest-by-mNoteStartCounter.  Return from the lowest-priority
// tier that has a candidate (Tier 0 first, falling back to Tier 2 only if
// neither Tier 0 nor Tier 1 yielded a victim):
//
//   Tier 0 (steal first): mInRelease == true  - voice is in ADSR release phase,
//                         fading out anyway; cheapest steal.
//   Tier 1 (medium):      ! isKeyDown()       - key has been physically released
//                         but voice is still active (sustain pedal holding the
//                         note in sustain phase, or rare attack/decay-with-key-up
//                         cases).  juce::SynthesiserVoice::isKeyDown() is managed
//                         by JUCE's Synthesiser::startVoice (sets true) +
//                         noteOff/sustain-pedal handling (sets false).
//   Tier 2 (protect):     isKeyDown() == true - key physically held; pro-DAW
//                         convention is to almost never steal these.  Only
//                         stolen if no Tier 0 or Tier 1 candidate exists (the
//                         pathological all-keys-held scenario).
//
// newPitch is reserved for future "don't steal a voice playing the same pitch"
// logic; unused for now.  Returns nullptr only if zero active voices exist
// (shouldn't happen at steal-time since activeCount >= cap is the precondition).
VibeVoice* VibeSynth::findStealCandidate (int /*newPitch*/) const noexcept
{
    VibeVoice*   victims[3] = { nullptr, nullptr, nullptr };
    juce::uint32 ages   [3] = {
        std::numeric_limits<juce::uint32>::max(),
        std::numeric_limits<juce::uint32>::max(),
        std::numeric_limits<juce::uint32>::max()
    };

    for (int vi = 0; vi < kMaxVoices; ++vi)
    {
        auto* vv = mVoices[vi];
        if (vv == nullptr || ! vv->isActive())
            continue;

        const auto age = vv->getNoteStartCounter();
        int tier;
        if (vv->isInRelease())
            tier = 0;   // Tier 0: release phase
        else if (vv->isNoteOffQueued() || ! vv->isKeyDown())
            tier = 1;   // Tier 1: about-to-release this block (look-ahead) OR key
                        // released but voice still sustaining (sustain pedal held)
        else
            tier = 2;   // Tier 2: key physically held + no queued noteOff (protect)

        if (age < ages[tier])
        {
            victims[tier] = vv;
            ages[tier]    = age;
        }
    }

    if (victims[0] != nullptr) return victims[0];
    if (victims[1] != nullptr) return victims[1];
    return victims[2];
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

    // QA-VoicePool Task 3 look-ahead fix (2026-05-25): pre-scan MIDI for
    // noteOffs that WILL be delivered this block (not stripped by a same-pitch
    // later noteOn) and flip mNoteOffQueued on the voices playing those pitches.
    // Reason: noteOns are dispatched synchronously via mSynth.noteOn in the main
    // loop below (which fires voiceCap stealing), but noteOffs are deferred to
    // filteredMidi and only delivered via mSynth.renderNextBlock at the bottom
    // of this function.  Without this look-ahead, the voiceCap-stealing
    // findStealCandidate sees about-to-release voices as still-key-down (their
    // noteOff hasn't fired yet), classifies them as Tier 2 alongside a held lead,
    // and picks the lead as the oldest Tier 2 victim.  With the look-ahead flag,
    // those voices demote to Tier 1 and the lead stays protected.
    //
    // Clear all flags first (transient per-block), then mark voices about to
    // receive a non-stripped noteOff.
    for (int vi = 0; vi < kMaxVoices; ++vi)
        if (mVoices[vi] != nullptr)
            mVoices[vi]->setNoteOffQueued (false);

    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (! msg.isNoteOff())
            continue;
        const int p = msg.getNoteNumber();
        if (p < 0 || p >= 128)
            continue;
        // Mirror the same-pitch note-off strip logic below: a noteOff at
        // sample <= lastNoteOnPerPitch[p] gets stripped (dropped from
        // filteredMidi) and never fires - skip marking voices in that case.
        if (lastNoteOnPerPitch[p] >= 0 && m.samplePosition <= lastNoteOnPerPitch[p])
            continue;
        // noteOff will be delivered this block.  Mark any voice playing this
        // pitch so findStealCandidate sees it as Tier 1 (about-to-release).
        for (int vi = 0; vi < kMaxVoices; ++vi)
        {
            auto* vv = mVoices[vi];
            if (vv != nullptr && vv->isVoiceActive() && vv->getCurrentlyPlayingNote() == p)
                vv->setNoteOffQueued (true);
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
            // ADSR release - avoids clicks on tonal sources like violin).
            // Combined with the per-pitch note-off strip below, this prevents
            // juce::Synthesiser's note-off matching from cascading onto a
            // newly allocated voice for the same pitch. Standard sampler
            // behaviour (Kontakt / HALion / FL Sampler all do this).
            //
            // QA-VoicePool Task 3: iterate cached mVoices[] (no dynamic_cast
            // per voice per Sub-A=(a) rationale - this loop fires on every
            // note-on so the dynamic_cast cost was on the hot path).
            for (int vi = 0; vi < kMaxVoices; ++vi)
            {
                auto* vv = mVoices[vi];
                if (vv != nullptr && vv->isVoiceActive()
                    && vv->getCurrentlyPlayingNote() == note)
                    vv->stopNote (0.f, true);   // soft stop (ADSR release)
            }

            // ── Cut-self: hard-stop ALL voices (any pitch) before the new note ──
            if (mCutSelf)
                mSynth.allNotesOff (0, false);

            // ── Unison fan-out: N voices per note-on with per-voice cents ───
            const int   N           = juce::jlimit (1, 8, mUnisonVoices);
            const float detCents    = mLastDetune  == 9999.f ? 0.f : mLastDetune;
            const int   detMode     = juce::jlimit (0, 2, mLastDetuneMode < 0 ? 0 : mLastDetuneMode);
            const float spreadCents = mUnisonSpread;
            // QA-VoicePool Task 3: cap is the LOGICAL polyphony limit (user-facing,
            // default kLogicalCap=16).  Stealing fires when active >= cap, NOT when
            // active >= kMaxVoices (=24).  The (kMaxVoices - kLogicalCap) = 8 reserve
            // voices give the steal-quick-release path a safe landing zone.
            const int   cap         = mLastVoiceCap > 0 ? mLastVoiceCap : kLogicalCap;

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

                // QA-VoicePool Task 3: voice-cap enforcement via mIsActive atomic
                // scan + findStealCandidate's L7(b) hybrid (prefer release-phase
                // oldest; fall back to overall-oldest).  Victim enters a ~1.5 ms
                // quick-release via initiateSteal + stopNote(0.f, true); it
                // continues fading inside JUCE's renderNextBlock while the new
                // note's mSynth.noteOn allocates to one of the reserve voices
                // (kMaxVoices=24 - kLogicalCap=16 = 8 reserves), so the fade-out
                // and the new note never collide on the same slot in the same block.
                int activeCount = 0;
                for (int vi = 0; vi < kMaxVoices; ++vi)
                    if (mVoices[vi] != nullptr && mVoices[vi]->isActive())
                        ++activeCount;

                if (activeCount >= cap)
                {
                    if (auto* victim = findStealCandidate (note))
                    {
                        victim->initiateSteal();
                        victim->stopNote (0.f, true);  // enter quick-release naturally
                    }
                }

                // Pre-tag all voices with cents_i - juce::Synthesiser::noteOn will
                // allocate exactly one idle voice and call startNote synchronously,
                // which consumes mNextUnisonCents and resets it to 0.
                forEachVoice ([cents_i] (VibeVoice& v) { v.setNextUnisonCents (cents_i); });

                mSynth.noteOn (ch, note, vel);
            }
        }
        else
        {
            // Per-pitch note-off strip (always on - paired with per-pitch preempt).
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
// CPU-guarded parameter setters - only push to voices when value actually changed.

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
    // QA-ClipPlayback: bipolar stereo param (-1..1) -> M/S width (0 mono, 1 full, 2 wide).
    mStereoWidth  = 1.f + width;
}

void VibeSynth::setTreble (float treble) noexcept
{
    if (treble == mLastTreble) return;
    mLastTreble  = treble;
    // Bug fix 2026-04-21: APVTS range is -12..+12 (centered 0 = flat).
    // Map linearly to gain multiplier for high component:
    //   treble = 0    → mTrebleGain = 0   (flat - no boost/cut)
    //   treble = +12  → mTrebleGain = +1  (~+6 dB shelf at 8 kHz)
    //   treble = -12  → mTrebleGain = -1  (full high cut - cancels highs)
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
    // QA-VoicePool Task 7 NIT 1 fix-up: clamp upper bound is kLogicalCap (16),
    // NOT kMaxVoices (24).  The user-facing polyphony range is 1..16; the 8
    // reserve voices (kMaxVoices - kLogicalCap) are RESERVED for stolen-voice
    // ADSR-quick-release fade-out overflow per Task 3 Correction 1 physical
    // over-provisioning.  Clamping at kMaxVoices here would let a future APVTS
    // range bump (or a saved-state voiceCap=20 from a future version) silently
    // consume the reserve voices and break the fade-out mechanic.
    if (cap == mLastVoiceCap) return;
    mLastVoiceCap = juce::jlimit (1, kLogicalCap, cap);
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
