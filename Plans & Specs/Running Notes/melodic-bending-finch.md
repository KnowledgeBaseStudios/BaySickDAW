# Running Notes — QA-Fa (melodic-bending-finch)

> Append-only running log for QA-Fa. New `## YYYY-MM-DD — <checkpoint>` entry at every checkpoint per `feedback_draft_doc_running_notes_every_checkpoint.md`. Under BULK-RUN mode ([`Batch Plans/swift-stampeding-caribou.md`](../Batch Plans/swift-stampeding-caribou.md)) there are no per-task verify entries; the Work Log close entry is drafted + HELD here under `## Held Work Log entry (apply at section pass)` at code-complete, applied at Master Test Plan §B.7 section pass (R2).
>
> Pair file: [`Plans & Specs/Batch Plans/melodic-bending-finch.md`](../Batch Plans/melodic-bending-finch.md). Conventions: Main Plan §0.

## 2026-07-09 — Group open (G2) — seeded

Plan approved 2026-07-09 (G2 group approval, R5). QA-Fa consumes QA-F's shared foundation (composite renderer + shifters); §14 fully locked. Code starts after QA-F Tasks 1-2 land.

### Locked spec calls
- **§14a-§14g** — full BaySickPitch redesign locked; composite-driven, Slice/Edit modes, per-note vibrato/formant/volume sub-curves, realtime applicator (Mode C), render-to-bake `Pitched/`, ~10-15 `bsp_*` params.
- **Call 4a** — DSP-04 drag-drop import DEAD; composite-auto-resolve only (§14b removed the Load button).
- **Colors** (§14c/§15) — pills Effects-purple fill / Vox-teal waveform / Bass-green pitch curve. 3 knobs → Focus/Mod/Speed.
- **Send Notes to** (§14b) — active Layers/Bass/Drums/Clips; MIDI only (not audio).
- **Brand-safety** — remove Newtone trade-dress (renames + `:8` comment); engine name BaySickPitch stays. QA-F owns the BaySickVocal-wide sweep; QA-LegalReview does the tree-wide pass.

### Surface map (current code, verified 2026-07-09 via Explore agent A)
- **No `BaySickPitchDSP` exists** anywhere. Net-new DSP.
- `BaySickPitchEditor` zero APVTS attachments; CENTER/VARIATION/TRANS inert (`bigKnob` lambda `:127-146`, no onValueChange); `Load` no `onClick` (`:89`); grid handlers early-return `!hasAudio()` (never true); `:8` = "Newtone-clone".
- No drag-drop import path (no `FileDragAndDropTarget`).
- Consumes QA-F: `renderChannelComposite` (Task 1) + `PitchShifters.h` (Task 2) — confirm interfaces final before Task 1 (architectural-alignment risk per §5).

### Fork-out (Rule 3)
- QA-J overlap: SEQUENTIAL same-row clips only; overlap → campaign QA-J-Verify (§C ledger item 2).

## 2026-07-10 — Task 1 code-complete — BaySickPitchDSP (net-new)

- **Shipped (DSP):** `Source/DSP/BaySickPitchDSP.h/.cpp` (+ CMakeLists source entry). `PitchNoteRegion` (detected start/end/midi/f0/vibrato + edits shift/formant/vibDepthMult/vibRateMult/volShape points, XML round-trip). `analyzeComposite`: reuses `BaySickAlignDSP::estimateF0Track` (2048 hop) → voiced grouping w/ 2-frame gap ends + 0.6-semi/2-frame sustained-jump splits + 60 ms minimum; per-note vibrato detect (√2·RMS cents depth, zero-crossing rate, <15 cents = none); edits carry across re-analysis (±50 ms start match). **Realtime applicator (Mode C):** immutable snapshot via atomic swap + retire-ring-of-8 (audio holds the pointer one block only); per-sample monotonic region cursor w/ seek rewind; target semis = pill shift + Focus·(nearest-semi pull) + ADDITIVE vibrato synth (mult 1 = nothing added — flattening real vibrato needs phase tracking we don't do; documented); one-pole glide from Speed; PSOLA period from region f0; formant engine engaged only when any region has formant edits (constant latency per snapshot); volume shape interp. **Fast path:** one atomic load + two float compares when no edits + neutral knobs (lazy-activate lock honored). `renderOffline` = same applicator streamed with PSOLA+formant latency-compensating lead-in.
- **Shipped (processor):** `mPitch` member + prepare; 6 `bsp_*` params (focus 0-100 DEF 0 — analyzed-but-untouched channels play bit-identical; mod 0-100 def 50 → ×1 natural; speed 0-100 def 50 → 300−2.95v ms glide; preset 0-3; preset_dirty; mode Slice/Edit) — the plan's "~10-15" estimate lands at 6 honest params (same pattern as QA-F's 13-vs-20). Push per block via CPU-guarded setters; **applicator wired in processBlock's FilePlay branch** (mutually exclusive with the realtime corrector — the force-bypass else-branch). `finalizeFilePlayStrip` stamps `setFilePlayTimelineSample(ctx.projectStart)` next to the force-bypass set (one site covers ST + MT). `<PitchEdits>` placeholder NOW REAL: PitchState + Pitched/ render history + analyze-time signature (NamIr extract pattern). Actions: `analyzePitch` (own-channel composite auto-resolve), `renderPitchedTake` (bake → `Pitched/{label}_pitch_v{N}.wav`, versioned, history), `isPitchStale`.
- **Improvement (both editors):** `channelClipSignature` now folds the TEMPO TIMELINE (stepped-map walk or global tempo) — a tempo change re-maps beat→sample for every clip, staling Align warp maps + Pitch note regions even with no clip moved; the badge now catches it.
- **OPEN (to ask at close):** does a Pitch bake auto-place like the Align bake (row below + row-mute)? Placement deliberately unwired; the applicator already plays edits live, so the bake is an export artifact until answered.

## 2026-07-10 — Task 2 code-complete — BaySickPitchEditor full redesign (§14b-§14f)

- **Shipped:** BaySickPitchEditor.h/.cpp FULL REWRITE (641-line paint-only shell → live editor; old public API had no external callers — BaySickVocalEditor only constructs it, ctor signature unchanged). §14 coverage: toolbar (title/3-mode preset combo + Save/Load + dirty dot/Slice+Edit radio/Reset/Render/"Send Notes to..."/Undo/Redo/Auto-Scroll A + keybind/stale RE-ANALYZE badge/LENGTH readout `X bars / M:SS.f` + `SEL` variant through the TempoMap/Focus-Mod-Speed knobs APVTS-attached); canvas (keyboard + time ruler + note lanes + Bass-green F0 curve from the analysis track + purple pills w/ teal waveform interiors at the EDITED pitch + green edit dots + per-note VIB/FRM/VOL sub-curve strip under the selected pill + FilePlay playhead w/ auto-scroll); Slice = click-split (halves inherit edits), Edit = vertical pitch drag (0.1 st) + edge trims (neighbor-clamped); InfoBar monospace Pitch/Cents/Length on hover/selection/drag; wheel = V-scroll, Shift = H-scroll, Ctrl = H-zoom.
- **Newtone bloat REMOVED per §14b:** File/TEMPO/SYNC/Loop/Play-Stop/Slaved/Load/Cut-Adv-Vib modes/"→PR" all gone; CENTER/VARIATION/TRANS → Focus/Mod/Speed; `:8` clone comment died with the rewrite. **Tree-wide `Newtone` grep: ZERO hits** at code-complete.
- **"Send Notes to..." (§14b):** `StandaloneEditor::listPitchNoteTargets()` (walks mPages for Layers/Bass/Drum/Clips pages — Clips listed only when a Clips page exists, inherently) + `sendPitchNotesToTab()` (ContourNote seconds → beats at transport tempo, appended into the target page's roll in the CURRENT pattern from beat 0, numBars extended, markDirty). MIDI only — audio never moves. Editor reaches it via `findParentComponentOfClass<StandaloneEditor>` (the established pattern).
- **Presets (§14f):** Loose {0,50,30} / Close {50,50,50} / Tight {100,20,80} drive the 3 knobs (values = my DSP calibration; §14f locks the structure, not the numbers); user presets → `Presets/BaySickPitch/My Presets/` (independent library from Align per §14b); dirty dot = snapshot compare, mirrored into `bsp_preset_dirty`.
- **Design judgments (logged):** local edit-undo stack (regions snapshots, cap 50 — same rationale as the Align editor); scrollbars replaced by wheel/zoom navigation + auto-scroll (§14e "generic mechanisms" reading — no Viewport widgets); analysis auto-runs on sub-tab visibility when unanalyzed or stale (§14b auto-resolve; silent on empty channels — the canvas empty-state carries the message); LENGTH bars resolve through the TempoMap (120 linear fallback).

## 2026-07-10 — Code-complete checkpoint — §B.7 authored; held Work Log entry below

- Both tasks code-complete. §B.7 authored (FA-1..FA-11; FA-10 ear-check marked for the G2 boundary per the owner's 2026-07-10 deferral; FA-6 references the OPEN pitch-bake placement call).
- Rule 4 diagnostic catalog: **no diagnostic instrumentation added in QA-Fa** (catalog empty).
- Awaiting: Jeff's build → fix to clean → the pitch-bake placement answer → commit approval → ONE batch commit.

## 2026-07-10 — DESIGN RECOVERY (Jeff catch) + QA-Fa recovery-round bundle LOCKED — THE SPEC for the next session

- **QA-Fa checkpoint commit `d8cc9494`** (both configs BUILD CLEAN, Jeff-verified; commit approved). The recovery bundle below lands as a SECOND commit ("QA-Fa recovery round" — the `35ac9928` follow-on precedent).
- **The drift (own it):** the G2-warp lock in the QA-F plan ("offline analyzeOffline; render-to-bake; playback loads the baked wav") was authored at group open from the STALE H-6a Option C comment in BaySickAlignDSP.h — it contradicts the LOCKED May design: [`Running Notes/phantom-recording-mongoose.md`](phantom-recording-mongoose.md) **§5 (2026-05-14, "design locked") line ~555: "Playback applicator: when a dub-channel clip plays via FilePlay, look up the dub channel's warp map; for each block render, apply the slice... PhaseVocoder does both time-stretch (from warp ratio) AND pitch shift (from anchor semitones) in one pass"** — i.e. BOTH editors play live with Render as opt-in export. Jeff caught the loss 2026-07-10. Every downstream artifact (the bake-placement spec call, the row-below + row-mute A/B model) was solving problems that only exist in the drifted version. §9 Forks entry documenting the recovery rides the section pass.
- **Engineering correction to my own latency claim:** live align warping needs NO added latency — during FilePlay the audio is ON DISK; the applicator warps the FILE READ POSITION (w(t) source remap at the decode layer), not a post-decode stream. The cost is decode-path integration, not playback latency.

### The locked bundle (Jeff, 2026-07-10 — every item confirmed in chat)

1. **Align realtime warp applicator at the clip-decode layer.** During FilePlay on the follower channel, remap the clip read position through the channel's WarpMap (at output timeline position t, read source at the warp-inverse position); PV stretch ratio compounds warp slope × tempo-stretch; per-anchor pitch semis apply in the same pass (§5's one-pass design). Surface: `decodeFilePlayClip` / the FilePlay position math (PluginProcessor.cpp) + a per-channel map lookup (the vocal engine holds the map; the decode path needs access — hook or atomic snapshot on the processor keyed by channel). CAREFUL: this is the G1-stabilized hot path (beat-domain position law, PV peek/advance, `clipFilePosForBeat`).
2. **ON/OFF "in the chain" switch per editor.** Align: reuse `bsa_align_on` (its §13e master-enable semantic = this). Pitch: NEW `bsp_on` Bool default true. Toggling mid-play slews (rule 5), never hard-switches.
3. **Apply + version history, both editors.** Every applied state = a version entry {snapshot (WarpMap / pitch regions+edits), grid signature at apply, ISO timestamp} in a per-editor dropdown; revert in stages; entries whose signature differs from the current grid show a "(grid changed)" marker (revert allowed → stale badge lights immediately). Versions persist in project XML (<AlignEdits>/<PitchEdits> children). Align's Analyze button becomes Analyze/Apply (computing + committing = one act). Pitch: edits stay INSTANT while dragging (May §4 iterative-UX lock) — versions are restore points (snapshot on analyze + an explicit snapshot action), NOT an edit gate.
4. **Auto-re-analyze, STOP-GATED (Jeff's rule).** Grid/tempo change on the analyzed channel(s) (the existing signature): transport STOPPED → debounced (~1s) auto re-analyze+apply (new version appended; history = the safety net that makes auto safe); transport PLAYING → mark pending + badge, run at transport stop (poll `DSPBase::isTransportPlaying()` — exists since QA-EffectsReview). Applies to BOTH editors (pitch's analyze-on-tab-open stays as well). Playback during the pending window keeps applying the last-applied state (stable-but-stale beats shifting-under-you). Manual Apply mid-play allowed (slews).
5. **No-click rule for mid-play changes:** warp/read-position changes from user actions (ON/OFF toggle, manual Apply) slew over ~50 ms (micro-varispeed glide, no splice). Auto swaps happen only at transport stop = silent by construction. Pitch target changes already glide via the Speed smoothing (no work needed). Accepted artifact (flippable at ear-check): the ONE-TIME engage tick when a channel's first-ever edit wakes the lazy-activated shifter (~13 ms step; formant first-engage ~20 ms) — lazy-activate stays locked.
6. **Render = EXPORT ONLY, both editors.** Writes `<project>/Aligned/{name}_align_v{N}.wav` / `<project>/Pitched/{name}_pitch_v{N}.wav` + history entry — NO browser/library entry, NO grid placement, nothing audible changes. RETIRE the QA-F auto-placement: remove the `onPlaceBakedClip` call from `renderAlignedTake` + the hook install in VoxPage (committed in `35ac9928`; `StandaloneEditor::placeAlignedBake` + the `alignBake` flag STAY — the Add-From-Export flow reuses placement machinery, and the flag still guards composite self-feedback for placed exports). History entries GAIN a `startBeat` field (the render's timeline origin: Align = commonStartBeat, Pitch = composite startBeat) so re-import can land at the original position.
7. **"+ Add New Vox From Export" submenu in the Vox ribbon dropdown** (where Layers/Bass "+ Add New" live; overrides the G-4 no-add-in-Vox-dropdown convention for this one entry — Jeff's design). Submenu scans `<project>/Aligned/*.wav` + `<project>/Pitched/*.wav` grouped by folder; the ENTRY greys when: no exports exist / Vox cap reached (kMaxVoxStrips=6) / project unsaved (no folder). Picking one: spawn a new Vox strip via the Mixer-add path (strip + InsertNode + params + tab) → place the export as a clip on the new channel at its ORIGINAL timeline position (history startBeat lookup across the project's vox processors; fallback beat 0 for orphan files) → **PROMPT 1** "Clone the source tab's vocal chain settings?" (reuse the G-6 exportVoxState/importVoxState duplicate machinery, chain-state portion) → **PROMPT 2** "Mute the original Vox strip?" (strip mute param). ASCII-only strings.
8. **Version history vs global undo:** separate by design (dropdown = coarse restore points; Ctrl+Z = gestures). Fine-grained editor undo stays LOCAL until QA-UndoCoverage (G4) wires the 9b processor-owned authority; at that batch, Apply/revert also register as single global-undo entries. No wiring now — note only.
9. **Ear-checks:** all deferred to the G2 boundary smoke (owner call 2026-07-10, recorded in crooning-warping-lynx notes).
10. **§B.7 test plan needs updating** to the recovered semantics: FA-4 gains ON/OFF + slew checks; FA-6 becomes export-only + the Add-From-Export flow (prompts, grey rules, position); FA-9 gains stop-gating + version-dropdown revert + "(grid changed)" marker; NEW scenarios for Align live playback (map applies during FilePlay with no render) and version revert. §B.6's F-5 placement scenario needs rewriting to the retired-placement reality.

### Carry-Over — 2026-07-10 (session end; recovery round NOT started)

- **Completed:** QA-Fa Tasks 1+2 code-complete, BUILD CLEAN both configs, checkpoint committed `d8cc9494` (tree clean). QA-F closed (`9262c746` + follow-on `35ac9928`). Bundle design fully locked (above) — no open spec calls; the pitch-bake placement question DISSOLVED (export-only model supersedes it).
- **In-flight:** nothing mid-edit. The recovery-round bundle is specced but zero code written.
- **Assumptions changed:** G2-warp lock superseded by the §5 recovery (list above). The QA-F row-below/row-mute placement model is RETIRED as the playback story (machinery stays for Add-From-Export).
- **Resume action:** fresh session: read Main Plan §0 + this file IN FULL (this entry = the spec). Build order: (1) align decode-layer applicator + slew (the big rock — re-read `decodeFilePlayClip`/`clipFilePosForBeat`/the QA-Ec G1-boundary comments FIRST), (2) ON/OFF params + bsp_on, (3) version system + persistence, (4) stop-gated auto-re-analyze, (5) export-only rewiring + history startBeat, (6) Add-From-Export flow + prompts, (7) §B.7/§B.6 scenario updates, (8) editors' version-dropdown + Apply UI. Then Jeff builds → fix clean → surface commit message + FULL status → "QA-Fa recovery round" commit → append a recovery addendum to the held Work Log entry → QA-Fb′ opens.
- **Work-Log note:** the held entry below predates the recovery — the fresh session appends a recovery-round section before the batch's section pass.

## Held Work Log entry (apply at section pass)

> Apply to `Implemented Work Log.md` when §B.7 passes (R2). Stamp `HH:MM PT` at apply time.

```markdown
### 2026-07-10 — QA-Fa — BaySickPitch composite-driven editor + DSP build-out

**Bucket:** Players, Effects
**Plan:** `Batch Plans/melodic-bending-finch.md` · **Running notes:** `Running Notes/melodic-bending-finch.md` · **Commit:** <hash at commit>

#### Done

- **Task 1 — BaySickPitchDSP (net-new).** `Source/DSP/BaySickPitchDSP.h/.cpp` (+ CMake entry): frame-YIN note segmentation over the channel composite (gap + sustained-jump splits, 60 ms floor, per-note vibrato detection), per-note edits (pitch/formant/vibrato/volume-shape) with ±50 ms carry across re-analysis; REALTIME APPLICATOR (Mode C) — atomic snapshot + retire-ring, per-sample region cursor, Focus semitone pull + additive vibrato synth + Speed glide, PSOLA from region F0 + formant engine only when formant edits exist, one-atomic-load fast path for zero-edit channels; `renderOffline` (latency-compensated streaming twin) → `Pitched/{name}_pitch_v{N}.wav` + history. Wired into `BaySickVocalProcessor` (mPitch, 6 `bsp_*` params — plan's "~10-15" estimate → 6 honest: focus DEF 0/mod/speed/preset/dirty/mode; `<PitchEdits>` placeholder now real) and `finalizeFilePlayStrip` (timeline stamp beside the force-bypass set; applicator runs in the FilePlay branch, mutually exclusive with the realtime corrector).
- **Task 2 — BaySickPitchEditor full redesign (§14b-§14f).** 641-line shell replaced: pills/curve/sub-curves canvas, Slice/Edit, toolbar per §14b (LENGTH via TempoMap, presets + dirty dot, Render, "Send Notes to..." MIDI-only via new `StandaloneEditor::listPitchNoteTargets/sendPitchNotesToTab`, Undo/Redo local stack, Auto-Scroll + A key, stale badge), FilePlay playhead, InfoBar population, Focus/Mod/Speed APVTS-attached. Newtone trade-dress fully gone (tree-wide grep zero).
- **Improvement (both editors):** `channelClipSignature` folds the tempo timeline — tempo changes now trip the Align + Pitch stale badges (they re-map every clip's beat→sample position).

#### Found along the way

- The FilePlay force-bypass site in `finalizeFilePlayStrip` doubles as the one-site timeline anchor for both ST and MT paths — no per-task plumbing needed.

#### Spec calls

- Locked pre-batch: §14a-§14g + Call 4a (DSP-04 dead) + colors + Send-Notes MIDI-only. Mid-batch surfaced, OPEN at code-complete: **pitch-bake placement** (mirror the Align row-below + row-mute model, or file+history only) — placement unwired pending the answer; FA-6 verifies against the outcome.

#### Routed (Rule 3)

- QA-J-Verify (§C item 2): overlapping-same-row scenarios. BaySickPitch reference-product comment handoff from QA-F: CLOSED here (rewrite).
```
