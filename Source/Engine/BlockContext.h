#pragma once

#include <JuceHeader.h>

// Per-block context passed by reference to every RenderTask in the parallel
// section. Fields are set by PluginProcessor at the top of the dispatch block
// (after MIDI scheduling completes serially) and remain stable for the life
// of the block. Tasks read fields they need; they MUST NOT write back into
// this struct from worker threads.
struct BlockContext
{
    int    numSamples = 0;
    double bpm        = 120.0;
    // Session rate for THIS block.  Freeze substitution needs it to notice a
    // file rendered at a different rate (an export at a non-device rate, or a
    // device-rate change after freezing) and read it through the streamer's
    // ratio interpolator instead of raw-frame positions.
    double sampleRate = 0.0;
    bool   anySolo    = false;
    // 2026-05-06 (Batch 9b): project-level master_pan_law (0=-3dB constant
    // power / 1=linear / 2=-6dB).  Read once by PluginProcessor at the top
    // of the dispatch block; bus tasks pass it to VibeGraph::processBus.
    int    panLaw     = 0;

    // TS7 §6.9: SONG MODE for this block.  Freeze substitution is gated on it in
    // every strip task -- a freeze file is the SONG arrangement, and in pattern
    // mode the playhead wraps inside the pattern loop, so reading the file at
    // that position would play the song's opening bars instead of the pattern.
    // Carried here rather than each task reaching for the processor: it is
    // per-block state, which is exactly what this struct is for.
    bool   songMode   = false;

    // ── §6.8 pattern-mode freeze ─────────────────────────────────────────────
    // A freeze file rendered at SONG scope covers the arrangement from bar 1.
    // In pattern mode the transport wraps inside the pattern loop, so reading
    // that file at the absolute playhead played the song's opening bars on loop
    // instead of the pattern -- which is why substitution was gated to song mode
    // and pattern mode got no freeze at all.  A per-pattern render needs to be
    // read at a PATTERN-LOCAL position, which nothing in this context carried.
    //
    // patternIndex is -1 in song mode.  patternLocalSamples is this block's
    // offset from the loop start, already wrapped.
    int         patternIndex        = -1;
    juce::int64 patternLocalSamples = 0;

    // Playhead snapshot. Pointer rather than value so default-constructed
    // BlockContext is cheap; PluginProcessor sets to a stack-local PositionInfo
    // that lives for the duration of dispatchBlock.
    const juce::AudioPlayHead::PositionInfo* posInfo = nullptr;

    // ── Per-engine MIDI buffer arrays (Batches 3-5) ──────────────────────────
    // Each is a pointer to the FIRST element of an array of MidiBuffer owned
    // by PluginProcessor. The task derives its slot from its channelId.
    // Left as nullptr placeholders in Batch 2; populated in Batches 3-5.
    juce::MidiBuffer* layerPageMidi  = nullptr;
    juce::MidiBuffer* bassPageMidi   = nullptr;
    juce::MidiBuffer* drumPageMidi   = nullptr;
    juce::MidiBuffer* clipPageMidi   = nullptr;
    juce::MidiBuffer* voxPageMidi    = nullptr;
    juce::MidiBuffer* instPageMidi   = nullptr;
    juce::MidiBuffer* pluginPageMidi = nullptr;   // QA-ModelShell TS6 (BLU-447)
    juce::MidiBuffer* rustyDrumsMidi = nullptr;   // single buffer

    // Live audio input (for armed Vox/Inst strips). Snapshot taken at the top
    // of processBlock before buffer.clear(). Read-only from worker threads.
    const juce::AudioBuffer<float>* liveInputSnapshot = nullptr;
};
