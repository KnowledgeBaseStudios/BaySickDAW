# Running Notes — QA-Layout (roomy-retiling-ocelot)

> Append-only mid-batch log for QA-Layout.  A new entry lands at every checkpoint —
> commit landed / sub-task verified / finding captured / spec call resolved / scope
> pivot — via `/draft-doc running-notes` dispatches, per
> `feedback_draft_doc_running_notes_every_checkpoint.md`.  At batch close,
> `/draft-doc batch-close` reads this file as the primary input when compiling the
> Implemented Work Log entry.  Per L32 the Work Log entry is expected to HOLD to the
> G4 boundary (confirm with Jeff at close).

Pair file: [`Plans & Specs/Batch Plans/roomy-retiling-ocelot.md`](../Batch Plans/roomy-retiling-ocelot.md).
Convention: Main Plan §0 "Batch Plans + Running Notes layout" (locked 2026-05-11).

## 2026-08-03 — Planning session — scope locked, plan approved

- **Batch plan approved by Jeff and landed** (this file's pair).  Planning session ran in-chat:
  Jeff's authored spec `Files For Claude/Final V1 Layout.md` read first, then §0 + Carry-Forward
  §1–§3 + the mammoth held entries, then five parallel read-only source sweeps, then 16 dockets
  + follow-up rulings.  All rulings are in the plan's locked table (L1–L32); genuinely deferred
  calls are D1–D7.
- **Supersessions on record:** locked call 5a REVERSED (full-screen toggle ships; order
  preset | full-screen | close); the held "engine pickers onto title bars" note superseded —
  pickers are DELETED (Layers/Bass combos, Clips decorative combo, Drums "Pick a sound"
  button); Test Plans §B.31.0's drag-and-report floor collection superseded by the diag-driven
  flow (rewritten in place at T6); the held "Live Instrument" rename lands as **BaySickLiveInst**
  (+ menu) / **LiveInst** (tab/strip/titles); + menu keeps "BaySickVocal" singular; VST entry
  reads "VSTPlugin".
- **Structural rulings worth restating:** Window-7 = five sub-page windows (Pitch / Align /
  Vocal Chain / Pedals / NAM-IR), in-page views retired; the pedalboard IS the LiveInst player
  (tab click fronts it; NAM/IR button on its title strip; no Inst page window for live-input
  tabs); ribbon dropdown "Pages:" becomes a per-instance window list (LiveInst rows read
  "Pedals"); three-lifetime persistence in-batch with crash survival = autosave timer flush;
  stretch = native resize + free transform zoom for fixed-size plugins; caps Layers 20 /
  Bass 10 / Drums 32 / Clips 100 / Vox 10 / Inst 30 with the PR-target shift accepted and the
  second drum-kit PR entry workshopped before code (D3); Add menu rework + four new group buses
  (Layers/Bass/Clips/Plugins, one each, kVoxBus2 pattern) with the used-once-then-hide
  lifecycle; SYS coloring per-token; BLU-110 three-zone limiter panel IN (Jeff: "Build it");
  VibePlayer knobs literal ~18px.
- **Sequencing (L7):** T1–T5 before the T6 diag handoff (title-bar work + Window-7 land before
  Jeff sizes anything, so he sizes the decoupled windows); T9/T10/T11 may run while his sizing
  pass is in flight; T7/T8 wait on his numbers; D1/D2 re-docketed with data.
- **Verification shape:** part of G4 — NO batch smoke; per-task build gates only; this batch's
  §B section authored at code-complete (T14) and walked at the G4 boundary.  Bridged-specific
  `1cd1f5d6` items (program-name relay, param-touch relay, 32-bit path) recorded as UNTESTED —
  the smoke must not assume them.
- **Source corrections made during planning, recorded so they are not re-derived:** the
  perf-readout overlap is a 120-vs-160 gutter mismatch, not a text-length problem alone;
  `kMaxAudioRows` caps audio CHANNELS (Clips pages / Audio strips), NOT Builder grid rows — the
  grid has 500 rows and clips route many-to-one via `routeChannel` (the constant's name is a
  pre-QA-E fossil); every channel-id type owns a 100-wide block, so no cap raise needs
  re-basing; raising instance caps is constants + literal sweep, NOT the new-strip-type ~15-site
  job (Inst was bumped 6→10→20 in G-4/G-6 exactly that way); the vocal-chain saturation bug is
  the `sat_type` 0..1 range clamping Tape=2 back to Console every block.
- **CLAUDE.md stale fact noted for T14:** ArrangementGrid constants say kNumRows=32; source says
  500 (`BuilderPage.h:601`).

## Diagnostic Instrumentation Catalog (Rule 4)

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| (T6 diag, when built) | `[QA-Layout DIAG]` | Window-sizing collection: per-resize append of persist-key + title + WxH + effects panel mode to `Documents/BaySickDAW/window-sizing-diag.txt` | Remove at batch close |
