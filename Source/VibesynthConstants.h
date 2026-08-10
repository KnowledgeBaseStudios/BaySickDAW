#pragma once

// ── Core counts ───────────────────────────────────────────────────────────────
// Phase 2+ page limits (each page = one engine instance, not a 4-layer stack)
// QA-Layout T11 (L18): instance caps raised to their shipping values --
// Layers 20 / Bass 10 / Drums 32 / Clips 100 / Vox 10 / Inst 30.  These are the
// ONE statement of each cap: VibeGraph.h's MixerChannelIds::kMax*Strips are
// DERIVED from them, so a page cap cannot outrun its strip cap.  Range checks
// read the constants, never a bare literal.
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
static constexpr int MAX_DRUM_ROWS     = 16;   // max rows visible in the drums grid
static constexpr int MAX_STEPS_TOTAL   = 64;   // max steps across any sequence
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
