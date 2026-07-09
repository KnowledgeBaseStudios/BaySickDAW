# Running Notes — QA-TransportDisplay (punctual-gliding-otter)

> Append-only mid-batch log. New entry at every checkpoint (commit landed / finding captured /
> spec call resolved / scope pivot). Consumed at doc-close: under the BULK RUN this batch's Work
> Log entry is drafted at code-complete, HELD here under `## Held Work Log entry (apply at section
> pass)`, and applied only when its Master Test Plan §B section passes.

Pair file: [`Batch Plans/punctual-gliding-otter.md`](../Batch Plans/punctual-gliding-otter.md).
Conventions: Main Plan §0 (Batch Plans + Running Notes layout, locked 2026-05-11) + the bulk-run
adjustments in [`Batch Plans/swift-stampeding-caribou.md`](../Batch Plans/swift-stampeding-caribou.md).

## 2026-07-08 — G1 group open — plan approved + surface map (source-verified)

Plan approved with the G1 set (single R5 approval). Locked specs: 17a-17e + A1-A5 (see the run
plan's marathon + G1 plan-write answer tables). Condensed surface map from the 2026-07-08 Explore
pass — these refs are VERIFIED and supersede any older doc line refs:

**Bar layout:** `GlobalTransportBar.cpp:822-867` resized(); fixed-pixel left cluster ends x≈476;
`kControlsWidth = 520` (`.h:116`); the D-4 reserve = the 476→520 gap (comments `.cpp:198-200`,
`:854-856`); D-4 button inserts after the metro-arrow block (`.cpp:858-859`). Overlay layout:
`StandaloneEditor.cpp:9020-9055` — pattern button x 528-704 (`:9036-9038`), ribbon `ribX = 712`
(`:9041`), `ribW` proportional with `>60` guard (`:9042-9044`), `kCPUReserve = 120` (`:9034`).
Readout inserts at ribX; ribbon shifts right.

**Clock reads:** `StandalonePlayHead::getCurrentBeat()` (`StandaloneApp.h:23`) encapsulates the
seqlock (`deriveBeat` `.cpp:142-158`); `mSamplePos` (`.h:64`); pattern mode is ALREADY
pattern-relative via the loop wrap in `advanceBlock` (`.cpp:181-208`). Existing 30/timer consumers
to mirror: `PianoRollPage.cpp:41-83`, `BuilderPage.cpp:5719-5745` (bar = beat/4). TS chain for
pattern beats-per-bar: `StandaloneEditor.cpp:774-781` → `GlobalTransportBar.cpp:626-630`. Ticks:
`kTicksPerBeat = 96` (`VibesynthConstants.h:34-39`), converters `PatternManager.h:7-16`.

**settings.xml pattern to clone:** `load/saveMultiCoreRenderingPref` (`StandaloneApp.cpp:317-359`,
decl `.h:148-158`) — re-parse + preserve-siblings on save; root `<BaySickDAWSettings>`.

**Keybind (Ctrl+T):** enum `KeyBindings.h:23-76` — next free ID `0x10071` (comment `:75`);
catalog `buildCatalog` `.cpp:21-184` (Ctrl+M example `:152-155`); dispatch =
`StandaloneEditor.cpp:6924-7050` (`perform` switch), wiring `:1353-1368` (KeyPressMappingSet
listener; NO keyPressed override exists — clean).

**D-4 note path (A4 = collector, chord-safe + records):** hardware precedent
`StandaloneApp.cpp:747-763` → `getLiveMidiCollector().addMessageToQueue` (`:752`); active-tab
target `setLiveMidiTarget` (`PluginProcessor.h:331-335`, atomics `.h:1167-1168`), focus wiring
`StandaloneEditor.cpp:1574`/`:1588`; drain + route `PluginProcessor.cpp:1813-1867` — kinds
1/2/3/4/7/8/9 routed, Vox/live-Inst dropped (hardware parity), kind 8 gets −12, on-screen keyboard
lit `:1856-1858`, `allMidi` feeds the MIDI recorder. Alternative hold-audition atomics
(mono-per-block, NOT chosen): `auditionNoteOn/Off` on Synth/Harmless/Player/Bass
(off-before-on `BaySickSynthProcessor.cpp:65-72`).
