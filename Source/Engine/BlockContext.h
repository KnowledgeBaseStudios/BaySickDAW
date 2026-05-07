#pragma once

#include <JuceHeader.h>

// Per-block context passed by reference to every RenderTask in the parallel
// section. Fields are set by PluginProcessor at the top of the dispatch block
// (after MIDI scheduling completes serially) and remain stable for the life
// of the block. Tasks read fields they need; they MUST NOT write back into
// this struct from worker threads.
//
// Phase 2 scaffolding: only the framework fields are defined now. Per-engine
// MIDI buffer pointers + live-input snapshot are added in Batches 3-5 as the
// corresponding strip wrappers ship.
struct BlockContext
{
    int    numSamples = 0;
    double bpm        = 120.0;
    bool   anySolo    = false;
    // 2026-05-06 (Batch 9b): project-level master_pan_law (0=-3dB constant
    // power / 1=linear / 2=-6dB).  Read once by PluginProcessor at the top
    // of the dispatch block; bus tasks pass it to VibeGraph::processBus.
    int    panLaw     = 0;

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
    juce::MidiBuffer* rustyDrumsMidi = nullptr;   // single buffer

    // Live audio input (for armed Vox/Inst strips). Snapshot taken at the top
    // of processBlock before buffer.clear(). Read-only from worker threads.
    const juce::AudioBuffer<float>* liveInputSnapshot = nullptr;
};
