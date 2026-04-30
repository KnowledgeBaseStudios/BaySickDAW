#pragma once

// ── Core counts ───────────────────────────────────────────────────────────────
// Phase 2+ page limits (each page = one engine instance, not a 4-layer stack)
static constexpr int kMaxLayerPages    = 8;    // max simultaneous Layers pages
static constexpr int kMaxBassPages     = 4;    // max simultaneous Bass pages
static constexpr int kMaxDrumPages     = 16;   // max simultaneous Drums pages (D1: dynamic-drum model)
// Phase G-2/G-3 (2026-04-28): Clips pages — 1:1 with audio inserts (mixer_audio_0..49).
// Each Clips tab is spawned by drag/drop of an audio file onto Builder; pageIndex is
// the audio-row index, so the engine output can mix into the matching audio_<row>
// InsertNode without a separate audio-row lookup table.
static constexpr int kMaxClipPages     = 50;   // matches the audio-insert cap
// Phase G-4 (2026-04-28): Vox + Inst pages — 1:1 with their mixer inserts.
// Spawn trigger is the Mixer page's "Add Vox/Inst Strip" button (NOT drop).
// kMaxVoxStrips / kMaxInstStrips live in VibeGraph.h's MixerChannelIds; mirror
// here for piano-roll dispatch sizing without pulling that header into core.
static constexpr int kMaxVoxPages      = 6;    // matches MixerChannelIds::kMaxVoxStrips
static constexpr int kMaxInstPages     = 20;   // matches MixerChannelIds::kMaxInstStrips (G-4 bumped 6 → 10; G-6 bumped 10 → 20)
static constexpr int kBassPRTarget     = kMaxLayerPages; // PRPendingOff target ID for bass roll
static constexpr int kDrumPRTarget     = kMaxLayerPages + kMaxBassPages; // PRPendingOff target ID base for drum rolls
static constexpr int kClipPRTarget     = kMaxLayerPages + kMaxBassPages + kMaxDrumPages; // PRPendingOff target ID base for clip rolls (G-3)
static constexpr int kVoxPRTarget      = kClipPRTarget + kMaxClipPages;       // G-4
static constexpr int kInstPRTarget     = kVoxPRTarget  + kMaxVoxPages;        // G-4
static constexpr int MAX_DRUM_SOUNDS   = 46;   // total drum sound library size
static constexpr int MAX_DRUM_ROWS     = 16;   // max rows visible in the drums grid
static constexpr int MAX_STEPS_TOTAL   = 64;   // max steps across any sequence
static constexpr int DEFAULT_STEPS     = 16;   // 4 bars x 4 steps/bar
static constexpr int DEFAULT_BARS      = 4;
static constexpr int DEFAULT_SPB       = 4;    // steps per bar

// ── Bass sounds ───────────────────────────────────────────────────────────────
static constexpr int NUM_BASS_SOUNDS   = 15;

enum class BassSoundType : int
{
    Sub808       = 0,   // deep pitch-swept sine
    KickBass909  = 1,   // punchy transient sub
    TrapSub      = 2,   // sliding pitch-drop sub
    JazzWalking  = 3,   // upright bass simulation
    IndustrialGrind = 4,// distorted saw sub
    AnalogSine   = 5,   // clean pure sine
    MoogStyle    = 6,   // warm saw + resonant filter
    AcidBass     = 7,   // 303-style resonant square
    GrowlBass    = 8,   // FM dubstep growl
    RubberBass   = 9,   // short decay pitch-drop bounce
    HipHopThump  = 10,  // mid-heavy punchy sub
    PopSustain   = 11,  // clean sustaining bass
    RnBSmooth    = 12,  // warm sine slow attack
    BoomBap      = 13,  // classic hip-hop low end
    FunkSlap     = 14   // short transient mid punch
};

static const char* kBassSoundNames[NUM_BASS_SOUNDS] = {
    "808 Sub", "909 Kick Bass", "Trap Sub", "Jazz Walking", "Industrial Grind",
    "Analog Sine", "Moog Style", "Acid Bass", "Growl Bass", "Rubber Bass",
    "Hip Hop Thump", "Pop Sustain", "R&B Smooth", "Boom Bap", "Funk Slap"
};

// ── Drum sound names (46 sounds across 9 groups) ─────────────────────────────
// Groups: Kick(5) Snare(6) HiHat(5) Cymbal(5) Tom(4) Perc(6) Ethnic(5) Electronic(6) FX(4)
static const char* kDrumSoundNames[MAX_DRUM_SOUNDS] = {
    // KICK (0-4)
    "Kick Thump", "Kick Snap", "Kick Sub", "808 Kick", "Kick Short",
    // SNARE (5-10)
    "Snare Crack", "Snare Rim", "Snare Brush", "Rimshot", "Cross Stick", "Snare Ghost",
    // HI-HAT (11-15)
    "HH Closed", "HH Open", "HH Pedal", "HH Tight", "HH Loose",
    // CYMBAL (16-20)
    "Ride Bell", "Ride Edge", "Crash", "China", "Splash",
    // TOM (21-24)
    "Tom High", "Tom Mid", "Tom Low", "Floor Tom",
    // PERC (25-30)
    "Clap", "Snap", "Clave", "Cowbell", "Woodblock", "Shaker",
    // ETHNIC (31-35)
    "Tambourine", "Bongo High", "Bongo Low", "Conga", "Djembe",
    // ELECTRONIC (36-41)
    "808 Clap", "808 Tom", "Noise Hit", "Laser", "Glitch", "Vinyl Noise",
    // FX (42-45)
    "Reverse Cymbal", "Pitched Kick", "Sub Boom", "Impact"
};
// Group names and start indices for grouped UI display
static const char* kDrumGroupNames[] = {
    "KICK", "SNARE", "HI-HAT", "CYMBAL", "TOM", "PERC", "ETHNIC", "ELECTRONIC", "FX"
};
static const int kDrumGroupStart[] = { 0, 5, 11, 16, 21, 25, 31, 36, 42 };
static const int kDrumGroupSize[]  = { 5, 6,  5,  5,  4,  6,  5,  6,  4 };
static constexpr int NUM_DRUM_GROUPS = 9;

// ── Sequence routing ──────────────────────────────────────────────────────────
enum class SeqRouting : int
{
    BasicSequence   = 0,
    ComplexSequence = 1
};

// Pages that have sequence routing
enum class SequencedPage : int
{
    Layers = 0,
    Bass   = 1,
    Drums  = 2
};

// ── Step types (Complex Sequence, A1-style) ───────────────────────────────────
enum class StepType : int
{
    NoStep    = 0,   // rest / silence
    ShortStep = 1,   // plays for 1/4 of the step length
    LongStep  = 2,   // plays for the full step length
    StepLink  = 3    // ties into the next step (extends note)
};

// ── FX chain ──────────────────────────────────────────────────────────────────
static constexpr int NUM_LAYER_FX      = 4;   // orderable middle slots per layer
// Full chain order: Compression(fixed) + Character1 + [4 orderable] + Character2 + Spread(fixed)
// "Character" = Distortion / Tube Saturation / Transistor / Soft Clip + Frequency Mode

enum class CharacterType : int
{
    Distortion       = 0,
    TubeSaturation   = 1,
    Transistor       = 2,
    SoftClip         = 3
};

enum class SaturationFreqMode : int
{
    KeepLow    = 0,  // saturate 500Hz+ only
    Normal     = 1,  // full spectrum
    KeepHigh   = 2   // saturate below 6kHz only
};

// ── Standalone pages ──────────────────────────────────────────────────────────
static constexpr int NUM_STANDALONE_PAGES = 4;
static constexpr int PAGE_LAYERS  = 0;
static constexpr int PAGE_BASS    = 1;
static constexpr int PAGE_DRUMS   = 2;
static constexpr int PAGE_BUILDER = 3;

static const char* kPageNames[NUM_STANDALONE_PAGES] = {
    "Layers", "Bass", "Drums", "Builder"
};

// ── VST Magnify Window scale factors (Keyscape/Spectrasonics standard) ────────
static constexpr float kMagnifyScales[] = {
    0.8f, 0.9f, 1.0f, 1.1f, 1.2f, 1.3f, 1.5f, 1.7f, 2.0f
};
static constexpr int kNumMagnifyScales = 9;
static const char* kMagnifyLabels[kNumMagnifyScales] = {
    "Magnify Window 0.8x", "Magnify Window 0.9x", "Magnify Window 1x",
    "Magnify Window 1.1x", "Magnify Window 1.2x", "Magnify Window 1.3x",
    "Magnify Window 1.5x", "Magnify Window 1.7x", "Magnify Window 2x"
};
static constexpr int kDefaultMagnifyIndex = 2;  // 1.0x
