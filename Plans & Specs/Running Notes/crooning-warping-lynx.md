# Running Notes — QA-F (crooning-warping-lynx)

> Append-only running log for QA-F. New `## YYYY-MM-DD — <checkpoint>` entry at every checkpoint (commit landed / finding captured / spec call resolved / scope pivot) per `feedback_draft_doc_running_notes_every_checkpoint.md`. Under BULK-RUN mode ([`Batch Plans/swift-stampeding-caribou.md`](../Batch Plans/swift-stampeding-caribou.md)) there are no per-task verify entries; the Work Log close entry is drafted + HELD here under `## Held Work Log entry (apply at section pass)` at code-complete, applied at Master Test Plan §B.6 section pass (R2).
>
> Pair file: [`Plans & Specs/Batch Plans/crooning-warping-lynx.md`](../Batch Plans/crooning-warping-lynx.md). Conventions: Main Plan §0 (Batch Plans + Running Notes layout).

## 2026-07-09 — Group open (G2) — seeded

Plan approved 2026-07-09 (G2 group approval, R5). QA-F is the keystone of G2 (builds the shared foundation consumed by QA-Fa + QA-Fb). Code starts at Task 1.

### Locked spec calls (2026-07-08 marathon + 2026-07-09 group-open)
- **Call 1a** — composite renderer + shifters built HERE (Tasks 1-2) as first consumer; order F→Fa→Fb′→Fc; **ONE commit** (G1 model, TempoMap precedent).
- **Call 2a + latency refinement** — full realtime-pitch quality pass: build formant-preserve cepstral DSP + wire Throat Shift + swap live shifter to **LOW-LATENCY** (PSOLA/granular, NOT phase vocoder — PV adds ~40ms, confuses learners monitoring live) + tune retune-speed/strength. "Nothing happening" = on-key (expected; verify-not-fix). **No new params** — all pitch params exist.
- **Call 3a** — build all three shifters (extract+generalize PSOLA + Granular for arbitrary ratio; PhaseVocoder reused). Back the Algos dropdown (offline) + the live low-latency path.
- **§13a-§13g** — full BaySickAlign redesign locked; lanes Leader=Bass-green / Follower=Vox-teal / Output=Drums-red.
- **Brand-safety** — remove VocAlign trade-dress (renames + `:8` clone comment) + semantic sweep of BaySickVocal UI strings (incl. the 2 `Auto-Tune` tooltips `BaySickVocalEditor.cpp:189/:252`). KEEP engine name BaySickAlign + universal keybinds (§12). Engine names stay (Jeff 2026-07-09).

### Surface map (current code, verified 2026-07-09 via Explore agent A)
- BaySickAlign/BaySickPitch are sub-tab editors in `Source/BaySickVocal/` (built `BaySickVocalEditor.cpp:498-499`); DSP + params on the single `BaySickVocalProcessor` (40 params, zero Align/Pitch). `BaySickAlignDSP` in `Source/DSP/`.
- `BaySickAlignDSP` never instantiated; `analyzeOffline` real but never called; `applyWarp` memcpy passthrough (`BaySickAlignDSP.cpp:257-266`). `BaySickAlignEditor` zero APVTS attachments; `:8` = "VocAlign-clone".
- Realtime Vox pipeline LIVE + correct: pitch correction defaults OFF (`bsv_pitch_realtime_bypass`=true), bypassed during FilePlay. Formant/Throat no-ops (`PitchCorrectorDSP.cpp:327`, `juce::ignoreUnused`). Params: `bsv_pitch_formantPreserve/_throatShift/_retuneSpeed/_strength/_key/_scale/_humanize`.
- No analysis composite renderer (reference walk `renderAudioClipsForRow` PluginProcessor.cpp:521 — realtime, skips FilePlay). Only `PhaseVocoder` reusable (`PhaseVocoder.h:27`); PSOLA (`OctaveStyleDSP::PeriodDoubler`) + Granular (`OctaveStyleDSP::GranularShifter`, `PitchCorrectorDSP::Shifter`) nested/private.

### Fork-out (Rule 3)
- QA-J overlap: verify scenarios SEQUENTIAL same-row clips only; overlap → campaign QA-J-Verify (§C ledger item 2).
