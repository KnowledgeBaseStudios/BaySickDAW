# QA-Fa — BaySickPitch Composite-Driven Editor + DSP Build-Out — Plan (melodic-bending-finch)

> **Canonical path** (mirrored after group approval):
> `Plans & Specs/Batch Plans/melodic-bending-finch.md`
> Paired running notes: `Plans & Specs/Running Notes/melodic-bending-finch.md`

> **For execution (BULK-RUN mode — see [`Batch Plans/swift-stampeding-caribou.md`](swift-stampeding-caribou.md)):** code ALL tasks, NO per-task verify pause. Jeff runs `do_build.bat` after all tasks; fix to clean. Verify scenarios author into Master Test Plan **§B.7**. **ONE** source commit (Rule 9). Work Log close entry drafted + HELD; applied at §B.7 section pass (R2). The **after-QA-Fa ear-check** (pitch-edit quality) is the one Jeff gate in-batch.

## Context

BaySickPitch moves from a paint-only Newtone-clone shell to a functional composite-driven pitch editor. It is the sibling of BaySickAlign (both channel-level, both composite-driven — §14a) and **consumes QA-F's shared foundation** (channel-composite renderer Task 1 + the extracted pitch-shifters Task 2). Two-stage workflow: BaySickAlign first (coarse timing + channel-level pitch), BaySickPitch second (fine note-level correction).

**Current state (verified 2026-07-09, Explore agent A):**
- **No `BaySickPitchDSP` class exists anywhere** — not in `Source/DSP/`, not in `Source/BaySickVocal/`. Net-new DSP.
- `BaySickPitchEditor` ([Source/BaySickVocal/BaySickPitchEditor.cpp](Source/BaySickVocal/BaySickPitchEditor.cpp)) has **zero** APVTS attachments; CENTER/VARIATION/TRANS knobs are inert (built by a `bigKnob` lambda [:127-146](Source/BaySickVocal/BaySickPitchEditor.cpp:127) with no `onValueChange`); `Load` button has no `onClick` ([:89](Source/BaySickVocal/BaySickPitchEditor.cpp:89)); `Save` disabled; grid mouse handlers early-return on `!hasAudio()` which is never true.
- Source comment [:8](Source/BaySickVocal/BaySickPitchEditor.cpp:8) reads `"Newtone-clone visual + interaction model"` — trade-dress target.
- **DSP-04 (drag-drop import) DEAD** (Call 4a) — no `FileDragAndDropTarget`; the `Load` no-op stub is removed, replaced by composite-auto-resolve (§14b).

**Risk:** medium — editor + DSP build on a paint-only shell; architectural alignment with QA-F's composite/shifter interfaces (they must land first). **Effort:** medium-large (~8-14h). **Dependencies:** QA-F Tasks 1-2 (composite renderer + shifters). **Bucket:** Players, Effects.

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| G2-14 | BaySickPitch per-control redesign per [`Running Notes/phantom-recording-mongoose.md`](../Running Notes/phantom-recording-mongoose.md) §14a-§14g (locked 2026-05-14). Composite-driven, Slice/Edit modes, per-note vibrato/formant/volume sub-curves, ~10-15 `bsp_*` params. | Full redesign locked; breaks Newtone trade-dress. |
| G2-Q4 | **DSP-04 drag-drop import DEAD.** BaySickPitch operates only on the channel composite auto-resolved from grid clips (§14b removed the Load button). | Call 4a (Jeff 2026-07-09). |
| G2-shared | Consume QA-F's `renderChannelComposite` (Task 1) + `PitchShifters.h` (Task 2). Realtime applicator (Mode C) is default; Render/Freeze optional bake to `<project>/Pitched/{name}_pitch_v{N}.wav`. | §4 + §14a/§14g. Low runtime cost via lazy-activate (zero-edit clips cost zero). |
| G2-colors | Pills = Effects purple fill / Vox-teal waveform interior; pitch curve overlay = Bass green. 3 knobs renamed Focus/Mod/Speed. | §14c + §15 palette. |
| G2-send | "Send Notes to..." popup targets active Layers/Bass/Drums/Clips tabs; sends **MIDI only** (quantized contour), not vocal audio. | §14b. |

## Sub-spec calls surfaced for approval
**None open.** §14 locks every per-control decision; DSP-04 resolved (Call 4a). Any mid-execution call stops that piece and surfaces to Jeff.

## Files to modify

### Task 1 — BaySickPitchDSP (net-new)
- New [Source/DSP/BaySickPitchDSP.h](Source/DSP/BaySickPitchDSP.h) / [.cpp](Source/DSP/BaySickPitchDSP.cpp):
  - Consume `renderChannelComposite` (QA-F Task 1) → mono buffer; run YIN ([PitchTrackerYIN](Source/DSP/PitchTrackerYIN.h)) once over it.
  - **Note segmentation** → note regions with ABSOLUTE timeline positions (bar.beat).
  - Per-note edits (pitch shift / formant / vibrato depth-rate / volume) stored in a per-channel ValueTree keyed by absolute timeline position.
  - **Realtime applicator (Mode C):** during FilePlay, map the running clip's playhead to overlapping composite note region(s), apply stored edits per block via `PitchShifters.h` (QA-F Task 2). Lazy-activate (skip clips with zero edits).
  - Vibrato analyze+synthesize; formant shifter per-note (reuse QA-F formant machinery); volume envelope per-note.
  - Render/Freeze → `<project>/Pitched/{name}_pitch_v{N}.wav`; render history (separate list from Align).
- [Source/BaySickVocal/BaySickVocalProcessor.h](Source/BaySickVocal/BaySickVocalProcessor.h) / [.cpp](Source/BaySickVocal/BaySickVocalProcessor.cpp) — add `BaySickPitchDSP mPitch;` member; register ~10-15 `bsp_*` params ([createLayout :25-116](Source/BaySickVocal/BaySickVocalProcessor.cpp:25)) with isIdentity + dirty-flag; persist per-channel edits + render history.

### Task 2 — BaySickPitchEditor full redesign (§14b-§14f)
- [Source/BaySickVocal/BaySickPitchEditor.h](Source/BaySickVocal/BaySickPitchEditor.h) / [.cpp](Source/BaySickVocal/BaySickPitchEditor.cpp) — full rewrite:
  - Toolbar: KEEP title; REMOVE File/TEMPO/SYNC/Loop/Play-Stop/Slaved/Load; RETAIN LENGTH (`X bars / M:SS.f`, `SEL` variant); ADD preset combo + Save/Load + dirty dot + Undo/Redo; rename `→PR`→"Send Notes to..." popup; Save→"Render"; keep Reset/Auto-Scroll(A).
  - Modes collapse to **Slice / Edit**; Vibrato/Formant/Volume become always-visible per-note sub-curves under the selected pill.
  - Canvas: keep MIDI keyboard + ruler + grid; note regions = Pills (4-6px radius, Effects-purple fill, Vox-teal waveform interior); pitch curve = Bass green; ADD playhead during composite playback; per-note sub-curves (draggable depth/rate/shape).
  - InfoBar (bottom): populate monospace Pitch/Cents/Length at hover/selection; drop Loop field.
  - 3 global knobs renamed **Focus / Mod / Speed** (labels visible), APVTS-attached.
  - **Every control APVTS-attached — no paint-only.** Remove the Newtone-clone comment ([:8](Source/BaySickVocal/BaySickPitchEditor.cpp:8)).

## Tasks

### Task 1 — BaySickPitchDSP (net-new)
- [ ] Create `BaySickPitchDSP` consuming QA-F's composite renderer + shifters.
- [ ] YIN over composite + note segmentation (absolute positions).
- [ ] Per-channel edit ValueTree (pitch/formant/vibrato/volume, keyed by timeline position).
- [ ] Realtime applicator (Mode C) mapping clip playhead → composite note regions; lazy-activate.
- [ ] Vibrato analyze/synth; per-note formant + volume; Render/Freeze bake + history.
- [ ] Add `mPitch` member + ~10-15 `bsp_*` params (isIdentity + dirty flag); persist edits + history.

### Task 2 — BaySickPitchEditor full redesign
- [ ] Toolbar rebuild (presets/Save/Load/dirty dot/Undo/Redo/Render/Send Notes to); remove Newtone bloat.
- [ ] Slice/Edit modes; pills (purple/teal/green) + per-note sub-curves (vibrato/formant/volume).
- [ ] Playhead during composite playback; InfoBar population; 3 knobs → Focus/Mod/Speed.
- [ ] "Send Notes to..." popup (active Layers/Bass/Drums/Clips; MIDI only). All controls APVTS-attached; remove clone comment.
- [ ] **Brand-safety:** the redesign renames every Newtone-flavored control (Cut/Adv/Vib → Slice/Edit, CENTER/VARIATION/TRANS → Focus/Mod/Speed, TEMPO/SYNC/Loop/Slaved removed) + removes the `:8` clone comment; confirm no Newtone/brand strings remain in the Pitch editor. **KEEP by design (§12):** engine name BaySickPitch (ours). (QA-F owns the BaySickVocal-wide semantic sweep; QA-LegalReview does the final tree-wide pass.)

### Batch close (one commit)
- [ ] Jeff runs `do_build.bat`; fix Release+Debug to clean.
- [ ] Author Master Test Plan **§B.7** from Verify scenarios below.
- [ ] Draft + HOLD Work Log close entry in running notes; append code-complete entry + Rule 4 rows.
- [ ] Surface message + FULL git status → approval → **one** commit: `QA-Fa Tasks 1-2: BaySickPitch composite-driven DSP + editor redesign (BaySickVocal, DSP, test plan B.7 + running notes)`.

## Verify scenarios (→ Master Test Plan §B.7; `blocks:` QA-Fa commit)
1. **Composite auto-resolve** — put two vocal clips on a Vox channel; open BaySickPitch → note pills auto-appear over both clips at correct bar positions (no manual Load).
2. **Slice/Edit** — Slice splits a note pill at click; Edit drags a pill vertically (pitch) / horizontally (start/end).
3. **Sub-curves** — select a note; vibrato/formant/volume sub-curves show under the pill; drag vibrato depth → playback vibrato changes.
4. **Realtime applicator** — nudge a flat note up a semitone; press global Play → it plays corrected, no bake. A zero-edit clip on the same channel plays unaltered (lazy-activate).
5. **Render/Freeze** — Render writes `Pitched/{name}_pitch_v1.wav`; reload plays the baked file; second render → `_v2` in history.
6. **[EAR-CHECK] Pitch-edit quality** — a corrected note sounds natural (formant-preserved, no chipmunk) using the shared low-latency shifter; a manual pitch draw follows the drawn curve.
7. **Send Notes to** — with an active Layers tab, "Send Notes to → Layer 1" sends the detected contour as MIDI to that tab (audio not sent); Clips appears as a target only when an active Clips page exists.
8. **Persistence** — save project → reopen → all note edits + render history restored; user preset save/reload restores Focus/Mod/Speed + dirty dot clears.
9. **InfoBar** — hover a pill → monospace Pitch/Cents/Length populates; selection toggles LENGTH to `SEL ...`.
10. **Brand-safety** — no Newtone/brand strings remain in any Pitch-editor tooltip, label, menu, or source comment (visual pass); engine name BaySickPitch retained by design.

## Routing notes (Rule 3)
- Findings on QA-Fb′/Fc surfaces → fold + note; §9 routing at §B.7 section pass.
- Newtone brand-safety (comment + control renames) folded into Task 2.
- QA-J overlap fork: SEQUENTIAL same-row clips only; overlap → campaign QA-J-Verify (§C ledger item 2).

## Carry-Forward Reference touch points
- Read §4 (MT render path) before Task 1 (the applicator runs inside VoxStripTask FilePlay decode).
- Confirm QA-F Tasks 1-2 interfaces (`renderChannelComposite`, `PitchShifters.h`) are final before Task 1 (architectural-alignment risk called out in §5 QA-Fa dependency).
