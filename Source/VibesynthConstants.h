#pragma once

// ── Core counts ───────────────────────────────────────────────────────────────
// Phase 2+ page limits (each page = one engine instance, not a 4-layer stack)
// QA-Layout T11 (L18): instance caps raised to their shipping values --
// Layers 20 / Bass 10 / Drums 32 / Clips 100 / Vox 10 / Inst 30.  Each cap
// has a kMax*Strips mirror in VibeGraph.h's MixerChannelIds (kept in
// lockstep BY HAND); range checks read the constants, never a bare literal.
// NOTE the PR-target bases below are DERIVED by summing these caps, so this
// bump shifts every downstream target: pre-existing projects' piano-roll
// routing is invalidated once (accepted, Jeff -- QA-Layout L18).
static constexpr int kMaxLayerPages    = 20;   // max simultaneous Layers pages
static constexpr int kMaxBassPages     = 10;   // max simultaneous Bass pages
static constexpr int kMaxDrumPages     = 32;   // max simultaneous Drums pages (D1: dynamic-drum model)
// Phase G-2/G-3 (2026-04-28): Clips pages - 1:1 with audio inserts (mixer_audio_0..99).
// Each Clips tab is spawned by drag/drop of an audio file onto Builder; pageIndex is
// the audio-row index, so the engine output can mix into the matching audio_<row>
// InsertNode without a separate audio-row lookup table.
static constexpr int kMaxClipPages     = 100;  // matches the audio-insert cap
// Phase G-4 (2026-04-28): Vox + Inst pages - 1:1 with their mixer inserts.
static constexpr int kMaxVoxPages      = 10;   // matches MixerChannelIds::kMaxVoxStrips
static constexpr int kMaxInstPages     = 30;   // matches MixerChannelIds::kMaxInstStrips
// QA-ModelShell TS6 (BLU-447, 2026-07-29): hosted VST3 instrument tabs.
static constexpr int kMaxPluginPages   = 20;   // matches MixerChannelIds::kMaxPluginStrips
static constexpr int kBassPRTarget     = kMaxLayerPages; // PRPendingOff target ID for bass roll
static constexpr int kDrumPRTarget     = kMaxLayerPages + kMaxBassPages; // PRPendingOff target ID base for drum rolls
static constexpr int kClipPRTarget     = kMaxLayerPages + kMaxBassPages + kMaxDrumPages; // PRPendingOff target ID base for clip rolls (G-3)
static constexpr int kVoxPRTarget      = kClipPRTarget + kMaxClipPages;       // G-4
static constexpr int kInstPRTarget     = kVoxPRTarget  + kMaxVoxPages;        // G-4
// J-7b (2026-05-03): BaySickRustyDrums singleton - single PRPendingOff target
// (no array dimension since the engine is a 1-instance lock).
static constexpr int kRustyPRTarget    = kInstPRTarget + kMaxInstPages;
// QA-ModelShell TS6: APPENDED after Rusty deliberately.  These target IDs are
// DERIVED by summing the caps in order, so inserting anywhere earlier -- or
// changing any cap -- shifts every downstream target and invalidates the PR
// targets in saved projects.  Rusty is a 1-instance singleton, so appending
// past it moves nothing.
static constexpr int kPluginsPRTarget  = kRustyPRTarget + 1;
static constexpr int MAX_DRUM_SOUNDS   = 46;   // total drum sound library size
static constexpr int MAX_DRUM_ROWS     = 16;   // max rows visible in the drums grid
static constexpr int MAX_STEPS_TOTAL   = 64;   // max steps across any sequence
static constexpr int DEFAULT_STEPS     = 16;   // 4 bars x 4 steps/bar
static constexpr int DEFAULT_BARS      = 4;
static constexpr int DEFAULT_SPB       = 4;    // steps per bar

// QA-Ee (96 PPQ): authoritative musical-domain resolution.  One beat (one
// quarter-note) = 96 ticks.  Every straight AND triplet division lands on an
// integer tick count (Bar=384, Beat=96, 1/8=48, 1/8-triplet=32, 1/16=24,
// 1/32=12, 1/32-triplet=8, 1/64=6, 1/64-triplet=4).  Rides ABOVE QA-Ed's
// int64-sample transport.  beats<->ticks converters live in PatternManager.h.
static constexpr int kTicksPerBeat = 96;

// QA-Ee Stage 2 (Builder snap: 11-label scheme + dynamic "Line" grid).  Shared
// by Builder / PianoRoll / Record-Quantize so the APVTS Int index 0..10 maps
// identically everywhere.  (Label array static-in-header per the CLAUDE.md C++17
// const-char*[] gotcha -- never define it out-of-line in a .cpp.)
static const char* kUnifiedSnapLabels[11] = {
    "Off", "Line", "Bar", "Beat", "1/2 Beat", "1/3 Beat",
    "Step", "1/2 Step", "1/3 Step", "1/4 Step", "1/6 Step"
};
static constexpr int kNumUnifiedSnapDivs = 11;

// Fixed tick grid for the FIXED divisions (param idx 2..10).  idx 0 (Off) +
// idx 1 (Line) are not fixed values -> return 0 (the caller handles them).
inline int snapDivToTicks (int idx) noexcept
{
    switch (idx)
    {
        case 2:  return kTicksPerBeat * 4;   // Bar      = 384
        case 3:  return kTicksPerBeat;       // Beat     = 96
        case 4:  return kTicksPerBeat / 2;   // 1/2 Beat = 48
        case 5:  return kTicksPerBeat / 3;   // 1/3 Beat = 32  (eighth triplet)
        case 6:  return kTicksPerBeat / 4;   // Step     = 24  (1/16)
        case 7:  return kTicksPerBeat / 8;   // 1/2 Step = 12  (1/32)
        case 8:  return kTicksPerBeat / 12;  // 1/3 Step = 8   (1/32 triplet)
        case 9:  return kTicksPerBeat / 16;  // 1/4 Step = 6   (1/64)
        case 10: return kTicksPerBeat / 24;  // 1/6 Step = 4   (1/64 triplet)
        default: return 0;                   // 0 Off / 1 Line: not a fixed grid
    }
}

// Dynamic "Line" snap ladder (straight-time, coarse->fine, in ticks): Bar, Beat,
// 1/8, 1/16, 1/32, 1/64.  A rung is "live" when its on-screen spacing
// (g/384 * pixelsPerBar) >= the caller's min-line-px threshold.  Within each
// editor, Line snap AND the dynamic grid use the SAME threshold so they lock to
// the exact same set of visible lines: Builder uses kMinLinePx (its FL 16-cell
// cap); the Piano Roll + Drum Kit use kMinGridLinePx so their grid + Line snap
// both reach down to 1/64.
static constexpr int kDynamicSnapLadder[6] = { 384, 96, 48, 24, 12, 6 };
static constexpr int kMinLinePx     = 12;  // Builder grid + Line-snap threshold (on-screen px)
static constexpr int kMinGridLinePx = 5;   // Piano Roll + Drum Kit grid + Line-snap threshold (px) -- finer, down to 1/64

// Finest live ladder rung (ticks) at the given pixels-per-bar zoom.  Floor = Bar
// (384) so a zoomed-way-out Line snap still locks to bars.
inline int dynamicSnapTicks (double pixelsPerBar, int minLinePx = kMinLinePx) noexcept
{
    int finest = kDynamicSnapLadder[0];   // Bar floor
    for (int g : kDynamicSnapLadder)
        if ((double) g / 384.0 * pixelsPerBar >= (double) minLinePx)
            finest = g;
    return finest;
}

// ── Triplet grid ladder ───────────────────────────────────────────────────────
// Parallel to kDynamicSnapLadder but subdividing the beat in THIRDS (ticks,
// coarse->fine): Bar, Beat, 1/3 Beat (1/8 triplet = 32t), 1/16 triplet (16t),
// 1/3 Step (1/32 triplet = 8t), 1/6 Step (1/64 triplet = 4t).  This gives even
// per-beat doubling (3 -> 6 -> 12 -> 24 lines/beat) so triplets keep subdividing
// like triplets as you zoom; without the 16t rung the grid would jump 3 -> 12 and
// read as a 4-way split.  The 16t rung is grid-only (no snap target -- the snap set
// is 1/8T / 1/32T / 1/64T), but every actual snap target still lands on a drawn line.
// A grid renderer uses this ladder when the active snap is a triplet division;
// otherwise it uses the straight kDynamicSnapLadder.  Together these two are the
// single source of truth for grid lines across Builder / Piano Roll / Drum Kit.
static constexpr int kTripletGridLadder[6] = { 384, 96, 32, 16, 8, 4 };

// True when a unified snap division (0..10) is a triplet: 1/3 Beat (5),
// 1/3 Step (8), 1/6 Step (10).
inline bool isTripletSnapDiv (int div) noexcept
{
    return div == 5 || div == 8 || div == 10;
}

// The grid ladder (straight or triplet, ticks, coarse->fine) for the active snap.
// The snap TYPE (straight vs triplet) picks the ladder; the snap DIVISION does NOT
// cap depth -- each renderer draws every rung that clears its pixel threshold, so
// zooming in always reveals down to 1/64 (straight) or 1/6 Step (triplet) no matter
// how coarse the snap is.  countOut receives the rung count.
inline const int* gridLadderForSnap (int div, int& countOut) noexcept
{
    if (isTripletSnapDiv (div)) { countOut = 6; return kTripletGridLadder; }
    countOut = 6; return kDynamicSnapLadder;
}

// QA-Ee Stage 2 (content-bound dynamic zoom).  The zoom-OUT minimum (px/bar or
// px/beat) is computed per-window as vpW / maxThings, where the span is
// max(emptyBaseline, furthestContent + pad).  The empty baseline is
// monitor-dependent (vpW / kDefault*EmptyPx) so an empty project restricts
// zoom-out to a sensible workspace -- not hundreds of empty bars -- and expands
// incrementally as clips / markers / notes are added (FL playlist behavior).
static constexpr float kDefaultPlaylistEmptyPx  = 24.f;   // Builder: px/bar at empty zoom-out baseline (~vpW/24 bars on screen)
static constexpr float kDefaultPianoRollEmptyPx = 160.f;  // Piano Roll: tighter px/bar baseline for micro work (~vpW/160 bars)
static constexpr float kBuilderZoomPadBars      = 8.f;    // Builder: bars of headroom past the furthest content edge
static constexpr float kPianoRollZoomPadBars    = 1.f;    // Piano Roll: bars of headroom past the furthest note edge
// Max zoom-IN: the musical span (in beats) that fills the viewport at deepest
// zoom.  0.5 beat across -> tick-level editing (~16 px/tick on an 800px view).
// Shared by both windows so micro-editing depth is consistent.
static constexpr float kMaxZoomInBeatsAcross    = 0.5f;

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
