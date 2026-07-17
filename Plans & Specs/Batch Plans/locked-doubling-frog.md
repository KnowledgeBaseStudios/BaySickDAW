# QA-OctavePedal — Octave Engine Fix + Pedal UI + Low-Latency Inst Monitoring — Plan (locked-doubling-frog)

> **Canonical path (post-approval):** `Plans & Specs/Batch Plans/locked-doubling-frog.md`.
> Paired running notes: `Running Notes/locked-doubling-frog.md`.
> **Execution mode: bulk run** (R1-R5): §B at code-complete; Work Log HELD; ONE commit;
> spec calls ASKED.

## Context

Inserted 2026-07-13 (§9 fifty-eighth). Scout diagnosis of the "broken bell": the shipped
period-doubler is period-length OLA, NOT pitch-synchronous — grain boundaries free-run with no
zero-crossing marks (the June research's central prescription), so replay seams land mid-cycle
at the fundamental rate = metallic ring. Compounding: mid-confidence SUMS two desynced
octave-down engines (comb), fixed 64-sample seam exceeds high-note periods (beating), integer
period quantization (slow beat), lag-guard hard re-anchor on glides. Voicing (duck + LP)
matches research and stays. Pedal-mode editor overlap: OctaveStylePanel has no Pedal layout
branch — the full-width rack layout collapses in the ~200 px tile (working reference:
FurmanEQStylePanel's Pedal branch). Monitoring: Inst strips have NO monitor-mode
infrastructure (vocal split never generalized) and the pedal board reports ZERO latency to
PDC. Docket + Jeff's folded Rule-3 item close all of it. Risk: medium-high (audio-thread
pitch DSP + architecture port). Effort: ~14-20h. Dependencies: QA-Fe2 (PDC hooks +
MonitorPitchShifter precedent).

## Spec calls already locked

| ID | Decision |
|----|----------|
| #12=a | REAL polyphonic tracking wired (PolyPitchTracker per the June research; "Polyphonic" mode stops being mono-driven) |
| #13=a (modified) | Inst strips get a TWO-mode monitor on the listen LED right-click: **Dry** (raw interface input) and **With Effect** (everything the page's setup runs); no bypass-corrector middle mode |
| Jeff folded item (2026-07-17) | BaySickPedals PDC latency reporting, PULL model per Jeff's note: board-level message-thread getter sums non-bypassed slot latencies (`getSlotEffect` resolution, `bsp_slot{N}_bypass` honored, board-level bypass honored, mSlotsLock); EngineChainProcessor special-cases the Pedals stage; 5 Hz solve picks up swaps/bypass free; computation message-thread only; back-ref §9 fifty-ninth |
| Scout companion | The octave pedal itself reports its internal delay (doubler ~1 period + seam / granular half-grain worst-case stable figure) via DSPBase::getLatencySamples so the board sum carries it |
| §9 fifty-eighth | Pedal-mode layout rebuilt using the working rack layout as reference; June research is the engine-fix reference |

**Flagged interpretation (veto at approval):** With Effect is the DEFAULT monitor mode for
Inst strips — matches the batch goal (player hears the processed result) and the vocal-side
default lock from QA-Fe2. Say the word if Dry should be default instead.

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open beyond the flagged default above.

## Files to modify

- `Source/DSP/OctaveStyleDSP.h/.cpp` — PeriodDoubler (.h:126-155, .cpp:111-193), process()
  confidence handoff (.cpp:332-386), Schmitt mark reuse (Vintage divider .cpp:387-445),
  latency report
- `Source/DSP/PolyPitchTracker.*` — wire into Polyphonic mode per research :151-153/:178-183
- `Source/BaySickPedals/BaySickPedalsProcessor.h/.cpp` — `getChainLatencySamples()` pull
  getter (slots 0..7, bypass-aware, locked)
- `Source/Standalone/EngineChainProcessor.h` — Pedals special-case in the chain sum
  (:35-44 comment updated)
- `Source/Standalone/EffectEditorPanels.cpp` — `OctaveStylePanel::resized()` Pedal branch
  (:4536-4547; FurmanEQ reference :5549-5576; dbfsOut column dropped in pedal mode)
- `Source/Engine/Tasks/InstStripTask.cpp` — monitor fork (listen gate :272-273, engine render
  :261); `Source/PluginProcessor.cpp` — `mixer_inst_{n}_monitorMode` 2-value param (vox
  precedent :5998-6002); `Source/Standalone/MixerTrackStrip.cpp` — Inst listen-LED
  right-click menu (vox menu precedent)

## Tasks

### Task 1 — Pitch-synchronous octave-down (the bell fix)
- [ ] Grain boundaries anchored to Schmitt zero-crossing marks validated by YIN (research
      :144-150); share the Vintage-mode crossing detector as the mark source.
- [ ] Period-proportional seam crossfade (fixed 64 dies; cap at a period fraction).
- [ ] Fractional-period replay (sub-sample read; kills the round(P) beat).
- [ ] Clean engine handoff: no dual-engine summing band — switch with short fade;
      unvoiced/low-conf fallback per research (granular or mute).
- [ ] Lag-guard re-anchor smoothed (no hard readPos snap on glides).
- [ ] Offline A/B render old-vs-new on a DI riff (dev-side; Jeff's ear at the campaign).
- [ ] Build-confirm gate + running-notes checkpoint.

### Task 2 — Real polyphonic tracking (#12)
- [ ] PolyPitchTracker feeds the Polyphonic mode (range hints per research :178-183);
      chord input stops collapsing to mono tracking; mono material behavior preserved.
- [ ] Build-confirm gate + checkpoint.

### Task 3 — Pedal-mode layout rework
- [ ] `PanelMode::Pedal` branch for OctaveStylePanel: compact grid in the tile per the
      FurmanEQ reference; dBFS column dropped in pedal mode; all 5 knobs + mode selector
      visible, zero overlap; rack layout untouched.
- [ ] Build-confirm gate + checkpoint.

### Task 4 — PDC latency (Jeff's folded item + companion)
- [ ] OctaveStyleDSP reports its worst-case stable internal delay via getLatencySamples.
- [ ] BaySickPedalsProcessor::getChainLatencySamples(): sum non-bypassed slot latencies
      (getSlotEffect pending-aware; `bsp_slot{N}_bypass` + any board bypass honored;
      mSlotsLock; message-thread only per the updateBusLatencies contract).
- [ ] EngineChainProcessor special-cases the Pedals stage into the chain sum; 5 Hz solve
      carries it — no new trigger plumbing.
- [ ] Verify per Jeff's spec: drive pedals move the LAT readout (~1 ms class full board);
      bypass mid-play re-aligns within ~200 ms (one soft click = documented PDC cost);
      time-based pedals do NOT move it.
- [ ] Build-confirm gate + checkpoint.

### Task 5 — Low-latency Inst monitoring (#13)
- [ ] `mixer_inst_{n}_monitorMode` (Dry / With Effect; default With Effect per the flagged
      line); Inst listen-LED right-click menu (vox precedent, two entries).
- [ ] Monitor fork in InstStripTask: Dry = raw interface input to the monitor; With Effect =
      the page's full setup, with the octave stage's monitor tap running the low-latency
      time-domain path (fixed-ratio PeriodDoubler class — NOT the continuous-ratio
      MonitorPitchShifter, wrong tool for clean octaves) while the record/mix path keeps the
      full engine; edge fades on mode flips (Fe2 fade precedent).
- [ ] Recordings + mix unchanged by monitor mode (monitor-only fork).
- [ ] Build-confirm gate + checkpoint.

### Task 6 — Close (bulk run)
- [ ] §B section authored (incl. Jeff's folded-item verify lines); Work Log entry HELD;
      ONE commit (message + full git status → Jeff approves).

## Verification (§B-destined scenarios)

1. Octave -1 on a held DI note: no metallic ring/comb, no beat against dry (the bell is dead).
2. Legato glide under octave -1: no snap; high-register notes: no amplitude beating.
3. Chord into Polyphonic mode: tracks as chords (audibly distinct from the mono build).
4. Pedal-mode tile: all controls visible, no overlap; rack view unchanged.
5. LAT readout: rises with drive pedals loaded, falls on bypass (re-align ≤ ~200 ms, one soft
   click allowed), indifferent to delay/chorus pedals; octave pedal active on an Inst strip →
   bounce lands on the grid vs a no-pedal bounce.
6. Inst listen LED right-click: Dry vs With Effect audibly distinct; With Effect while the
   octave pedal is active tracks at the low-latency class (no ~50 ms slapback feel);
   recording quality identical in both modes.

## Routing notes (Rule 3)

Engine-quality tuning beyond "research-conformant + scenario-clean" → Forks entry at section
pass. SynthStyleDSP shares tracker code — findings there get logged + routed, not fixed here.

## Carry-Forward Reference touch points

- June research doc §Recommendation (:136-195) — the engine-fix reference.
- QA-Fe2 records: PDC solve + monitor-split precedent (gentle-scrubbing-otter scope addition;
  prancy-crunching-bear 2026-07-16/17 entries).
