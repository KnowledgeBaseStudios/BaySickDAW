# QA-RustyMeter — BaySickRustyDrums per-layer-volume CC vs per-strip dBFS meter — Plan (sorted-whistling-shannon)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/sorted-whistling-shannon.md`
> Paired running notes: `Plans & Specs/Running Notes/sorted-whistling-shannon.md`

> **For execution:** use `superpowers:executing-plans` inline. Steps use `- [ ]` checkbox syntax. Builds run by Jeff (`do_build.bat`) — never by Claude. Verify in the Debug exe FIRST, then Release (CLAUDE.md Build System standing rule). **Investigation-first batch** (Jeff-locked S1): Task 1 diagnoses + PAUSES for Jeff's root-cause review + fix-shape pick; Task 2 implements the chosen fix; Task 3 closes. The **fix shape is a genuinely deferred spec call** (Sub-A) — it is NOT pre-picked anywhere in this plan.

## Context

QA-RustyMeter fixes a pre-existing, BaySickRustyDrums-specific bug surfaced by Jeff at QA-DispatcherAffinity Task 3 Verify 2 (the kit-swap stability test) and routed forward as its own batch (§9 forty-second Forks entry; slotted after QA-DispatcherAffinity, before QA-EngineApvts).

**The bug:** the AriaControlPanel per-layer-volume CC sliders inside the BaySickRustyDrums kit player (KICK section Kick/OH/Punch, SNARE section Btm/Top/OH/Snap/Punch/Epic, and the equivalent level sliders on every other channel) audibly change the rendered output, but the per-strip dBFS meter on the Mixer page does NOT move. Confirmed pre-existing (present under Sub-K-on, before any QA-DispatcherAffinity Task 3 change). Confirmed BaySickRustyDrums-specific: BaySickGuitars + BaySickBasses volume knobs DO move their per-strip meters.

**Why BaySickRustyDrums is the odd one out:** it is the only engine that loads via `buildOutputRoutedSfzWrapper` (a synthesized wrapper SFZ that injects `output=N` per `<master>`/`<group>` line so each kit piece routes to its own sfizz output bus → its own per-strip InsertNode). BaySickGuitars + BaySickBasses use plain `loadSfzFile` (single stereo out, no multi-out wrapper) and don't exhibit the bug.

**Code-grounded findings from pre-batch mapping (read, not assumed):**
- Per-layer sliders write APVTS `brd_cc<N>` params → `parameterChanged` → `mSfizz->cc(0, cc, v)` ([BaySickRustyDrumsProcessor.cpp:53](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:53)). sfizz applies the CC internally.
- There is exactly ONE sfizz render — `mSfizz->renderBlock(mMultiOutPtrs.data(), …, stripCount)` into `mMultiOutScratch` ([:273](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:273)). There is NO separate stereo-mix render. One global `outVol` is then applied to all channels ([:277](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:277)).
- The per-strip meter AND the audible path both read the same `getStripBuffer` view into `mMultiOutScratch` ([:281-295](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:281)) via [RustyInsertTask.cpp:68](Source/Engine/Tasks/RustyInsertTask.cpp:68) → `VibeGraph::processInsert` → `InsertNode::processBlock` → `publishPeakReading`.
- **Tension that defines the investigation:** because audible + metered share one buffer, the §9 "final stereo mix-down also gets CC scaling but the per-strip path bypasses it" hypothesis is suspect — there is no second mix to bypass. The likelier mechanism is in `buildOutputRoutedSfzWrapper` ([:656-776](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:656)): it injects `output=N` ONLY into `<master>`/`<group>` tags, via a sticky `currentPieceOutput` tracker (top-level control blocks before any piece context get `currentPieceOutput = -1` → NO output= → route to output 0), and never annotates `<global>`/`<control>`/`<region>`/`<effect>` blocks ([:770-772](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:770)). If the kit defines a per-layer-volume CC as a level control at a block level the wrapper mis-routes (sticky) or never routes (output 0), the CC scaling can land on a different sfizz output than the metered strip. The actual Big Rusty Drums kit SFZ is in-repo (`Files For Claude/karoryfer.big-rusty-drums-1.100/Programs/`), so this is confirmable statically.

**Risk:** medium (sfizz parser + wrapper-synthesis territory; investigation depth uncertain until the static SFZ read lands). **Effort:** ~4-8h (investigation dominates; a wrapper-synthesis patch is small + bounded, a sfizz-internal patch is larger). **Dependencies:** QA-DispatcherAffinity closed (`5e830e2`). **Bucket:** Players + Mixer / Routing.

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| S1 | Task structure = 3 tasks (Investigate → Fix → Close). Task 1 diagnoses + PAUSES for Jeff's root-cause review + fix-shape pick; Task 2 implements; Task 3 closes. | Jeff 2026-05-29 (AskUserQuestion). Investigation-first batch — fix shape unknown until the trace lands; the pause gives a clean root-cause review + a separate commit boundary for diagnosis vs fix. |
| S2 | Investigation methodology = static-first (read the in-repo kit SFZ + reproduce the wrapper `output=N` synthesis); escalate to a runtime per-strip peak-trace ONLY if the static read is inconclusive; surface the confirmed root cause + fix-shape options to Jeff before any fix code is written. | Locked in the S1 question framing Jeff accepted. Cheapest decisive diagnostic first; aligns with `feedback_diagnose_before_fixing.md` (diagnose with Jeff's A/B before shipping a fix). |
| S3 | Silly-name = `sorted-whistling-shannon`. | Assigned by the plan-mode runtime (per the `federated-bouncing-cupcake` S8 precedent: "assigned by plan-mode runtime"); adopted for the canonical mirror + running-notes filename so one consistent name is used. |
| S4 | Verify ladder = §5's locked 3 scenarios: (1) per-layer-volume slider audibly changes output AND the per-strip dBFS meter tracks it in real-time; (2) no regression on BaySickGuitars + BaySickBasses volume-knob-to-meter; (3) no regression on the Stage D Sub-K-disabled MT test (6-cymbal bit-crusher stays absent). Debug first, then Release. | §5 QA-RustyMeter entry + §9 forty-second Forks entry. |

---

## Sub-spec calls surfaced for ExitPlanMode (genuinely deferred — resolved mid-batch, NOT pre-picked)

| ID | Question | Resolves when | Option space (no recommendation — Jeff picks) |
|----|----------|---------------|-----------------------------------------------|
| Sub-A | **Fix shape.** | Task 1 close, after the root cause is confirmed (static read, or Jeff's A/B if a runtime trace was needed). | (1) Wrapper-synthesis-level patch — fix `buildOutputRoutedSfzWrapper` so the block(s) carrying each per-layer-volume CC receive the correct `output=N` (e.g. annotate the un-/mis-routed control block, or fix the sticky tracker). (2) sfizz-internal patch — change CC interpretation / output-routing order inside vendored sfizz. (3) Alternative SFZ wrapper construction — restructure how the wrapper assigns outputs. (4) A shape the investigation surfaces. Per §5 + §9. |
| Sub-B | **Runtime-trace style** — only arises IF the static SFZ read is inconclusive. | Task 1, if/when the runtime phase is reached. | (a) Diagnostic `juce::AlertWindow` showing per-strip `mMultiOutScratch` peaks (Jeff-doesn't-code → a readable on-screen surface; constraint noted, not a pick). (b) `juce::Logger` line to `build_log.txt`. (c) Temp-file dump to `Documents/BaySickDAW/` (existing folder convention). Jeff picks if/when reached. |

> Per Main Plan §0 Rule 5: both rows above are genuinely deferred decisions that depend on a later finding — not picks staged as truth. Nothing in the Task 2 body assumes a particular Sub-A outcome.

---

## Files to modify

### Task 0 — Open
- `Plans & Specs/Main Plan.md` — §5 QA-RustyMeter entry: replace the `**Plan file:** \`<silly-name>.md (when started)\`` placeholder with `**Plan file:** \`Plans & Specs/Batch Plans/sorted-whistling-shannon.md\``.
- `Plans & Specs/Batch Plans/sorted-whistling-shannon.md` — mirror of this plan (Write).
- `Plans & Specs/Running Notes/sorted-whistling-shannon.md` — seed (Write).
- Delete `~/.claude/plans/sorted-whistling-shannon.md` after mirroring.

### Task 1 — Investigate (READ-ONLY; source-modifying ONLY if the runtime trace phase is reached)
- READ (no edit): the kit SFZ `Files For Claude/karoryfer.big-rusty-drums-1.100/Programs/01-full.sfz` + its `#include` chain (`mappings/*_map.sfz`, per-piece files, `keymap/`, `default/`, the `sn_stir_mute_groups.sfz` / `t18_stir_mute_groups.sfz` CC-crossfade blocks); the AriaControlPanel per-layer-volume slider→CC binding (locate: `Source/Standalone/AriaControlPanel.cpp` parse path + `ccLabel`/`ccParamId`, and the kit's control-GUI definition); `buildOutputRoutedSfzWrapper` ([BaySickRustyDrumsProcessor.cpp:656-776](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:656)); the CC dispatch ([:41-75](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:41)); the render + meter path ([processStrips :196-279](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:196), [getStripBuffer :281-295](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:281), [RustyInsertTask.cpp:18-88](Source/Engine/Tasks/RustyInsertTask.cpp:18), `InsertNode::processBlock` + `publishPeakReading` in `Source/VibeGraph.cpp`).
- CONDITIONAL edit (only if static read inconclusive): temp diagnostic trace at the candidate site — per-strip peak of `mMultiOutScratch` before + after `outVol` at [BaySickRustyDrumsProcessor.cpp ~276-278](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:276). Style = Sub-B. Cataloged per Rule 4; stripped at close.

### Task 2 — Fix (candidate surfaces — PINNED once Sub-A resolves at Task 1 close)
- If Sub-A = (1) or (3): `Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp` — `buildOutputRoutedSfzWrapper` ([:656-776](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:656)) and/or `loadKit` ([:498-535](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:498)).
- If Sub-A = (2): `libs/sfizz/src/...` (VENDORED — caution: vendored-lib edit; re-check the /MD runtime-library match per memory `reference_msvc_runtime_md_md_match.md`; don't over-prune per `feedback_dont_overprune_vendored_libs.md`; flag rebuild-cost + future-merge implications to Jeff before landing).
- Exact files/lines recorded in the running notes when Sub-A resolves.

---

## Tasks

### Task 0 — Open commit
- [ ] Mirror `~/.claude/plans/sorted-whistling-shannon.md` → `Plans & Specs/Batch Plans/sorted-whistling-shannon.md` (Write); delete the home-dir copy (one-copy hygiene per `feedback_plan_mirror_one_way.md`).
- [ ] Update Main Plan §5 QA-RustyMeter entry's `**Plan file:**` pointer (targeted Edit, not a rewrite — `feedback_targeted_edits_not_wholesale_rewrite.md`).
- [ ] Seed `Plans & Specs/Running Notes/sorted-whistling-shannon.md` (title / purpose blockquote / pair ref / convention ref / `## 2026-05-29 — Task 0 — open` entry) per §0 running-notes required sections.
- [ ] Surface FULL git status — including the 3 pre-existing CRLF-residue files (`BaySick{Basses,Guitars,RustyDrums}Processor.h`, zero content diff per `git diff --ignore-all-space`). Propose disposition: leave as-is (harmless, same as the prior 6 QA-DispatcherAffinity + QA-Sfizz commits) OR `git checkout --` them — Jeff's call.
- [ ] `/draft-commit` → surface drafted message + status to Jeff → commit on approval. Stage ONLY the plan docs (Main Plan.md + the two new Batch Plans / Running Notes files); never `git add -A`.
- [ ] Mark Task 0 done.

### Task 1 — Investigate (diagnose root cause; PAUSE for the fix-shape pick)

**Static phase (no build):**
- [ ] Read Carry-Forward §1 (RustyDrumsProducerTask + RustyInsertTask 1-to-13 fan-out) for dispatcher context. Note the meter publish path + the wrapper synthesis are NOT in Carry-Forward (frozen 2026-05-07, pre-Phase-J).
- [ ] Enumerate every per-layer-volume slider → CC number (KICK Kick/OH/Punch; SNARE Btm/Top/OH/Snap/Punch/Epic; every other section's level sliders) from the AriaControlPanel control-GUI definition.
- [ ] In the kit SFZ + its `#include` chain, grep each per-layer-volume CC number → identify the level opcode (`amplitude_oncc<N>` / `gain_oncc<N>` / `volume_oncc<N>`) AND the block level (`<global>` / `<control>` / `<master>` / `<group>` / `<region>`) it sits in.
- [ ] Reproduce the wrapper transform on the program SFZ; for each per-layer-volume CC's carrying block, classify the `output=N` outcome:
  - (a) un-annotated → routes to sfizz output 0 / strip 0 only;
  - (b) sticky-mis-annotated → inherits the last-seen piece's output, not its own;
  - (c) correctly annotated → CC reaches the metered strip; root cause is downstream (peak/meter path or a routing-expectation mismatch).
- [ ] Form the root-cause hypothesis; decide if it's confirmable from the static read alone.

**Runtime phase (ONLY if the static read is inconclusive):**
- [ ] Resolve Sub-B with Jeff. Add the temp per-strip-peak diagnostic at [BaySickRustyDrumsProcessor.cpp ~276-278](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:276). Add the Rule 4 `## Diagnostic Instrumentation Catalog` row in the SAME running-notes edit pass (Site / Tag / Purpose / Disposition = Remove at batch close).
- [ ] Tell Jeff: "Run `do_build.bat`. In Debug: load Big Rusty Drums (Full kit). Play a kick+snare pattern. Open the diagnostic. Move the KICK 'Kick' per-layer-volume slider up; report which per-strip peak changes (kick strip / a different strip / none). Repeat for SNARE 'Top'."
- [ ] Interpret the A/B → confirm root cause.

**Pause for the fix-shape pick (Sub-A):**
- [ ] Surface to Jeff IN PLAIN ENGLISH (`feedback_design_approval_in_plain_english.md`): the confirmed root cause + the candidate fix shapes (Sub-A option space) with the user-visible behavior trade-off of each. Jeff picks → resolves Sub-A.
- [ ] `/draft-doc running-notes` → apply (finding + A/B outcome + Sub-A resolution + any Rule 4 catalog rows).
- [ ] Task 1 commit — CONDITIONAL: if a runtime trace was added, it stays in (cataloged → stripped at close) or is reverted now; if pure static investigation (no source change), there is NO Task 1 source commit (the running-notes checkpoint is the artifact). Any commit routes via `/draft-commit` + surface + approve.

### Task 2 — Fix (shape per Sub-A; implement + verify)
- [ ] Pin the concrete fix surface from Sub-A (files/lines) into the running notes.
- [ ] Implement the fix. (If it somehow introduces a new APVTS-synced DSP path — unlikely for a routing/synthesis fix — apply `isIdentity()` + dirty-flag per memory; most likely N/A.)
- [ ] If the fix touches vendored sfizz: re-check the /MD runtime-library match + don't over-prune (memories above); flag blast radius to Jeff.
- [ ] Tell Jeff (verify ladder, S4): "Run `do_build.bat`. In Debug:
  - **(1)** Load Big Rusty Drums. Play a kick+snare pattern. Turn a per-layer-volume slider up/down (KICK Kick/OH/Punch; SNARE Btm/Top/OH/Snap/Punch/Epic). Verify: audible level change AND the per-strip dBFS meter on the Mixer page moves in real-time in the same direction.
  - **(2)** Open a BaySickGuitars tab + a BaySickBasses tab. Turn their volume knobs. Verify their per-strip meters still move (no regression).
  - **(3)** 6-cymbal MT-on test (Stage D Sub-K-disabled): play sustained cymbals/hi-hats under MT. Verify the bit-crusher is still ABSENT.
  - Repeat (1)-(3) in Release."
- [ ] Wait for Jeff's verify result (Debug then Release).
- [ ] On pass: `/draft-commit` → surface message + FULL git status → commit on approval. Commit body uses `BaySickRustyDrums` (engine path strings in the diff are unavoidable).
- [ ] `/draft-doc running-notes` → apply.

### Task 3 — Close sequence
- [ ] `/draft-doc batch-close` (synthesize from the running notes).
- [ ] Apply the close entry to `Plans & Specs/Implemented Work Log.md` via Edit (`**Bucket:** Players, Mixer / Routing`).
- [ ] `/review-batch QA-RustyMeter` — audit diff vs plan + CLAUDE.md rules + memory gotchas. Address BLOCKER / NEEDS-FIX in-batch; defer NITs into the close entry.
- [ ] Strip any remaining diagnostic instrumentation (Rule 4 catalog Remove rows) — surface the strip list to Jeff for approval FIRST.
- [ ] Route side findings (Rule 3): in-scope → close-entry routing table; out-of-scope → §9 Forks entry + §5/§6/Future State edits (surface slot options to Jeff; don't pick).
- [ ] Surface FULL git status. `/draft-commit` for the close commit → surface + approve → commit (separate from the Task 2 source commit — clean rollback boundary).

---

## Verification (end-to-end smoke)

After Task 2 lands:
1. **Build clean** — `do_build.bat` Release + Debug both green.
2. **Core fix** — per-layer-volume slider → audible change AND the per-strip dBFS meter tracks it in real-time, same direction.
3. **No sfizz-sibling regression** — BaySickGuitars + BaySickBasses volume-knob-to-meter still works.
4. **No dispatcher regression** — 6-cymbal MT-on test: bit-crusher absent (QA-DispatcherAffinity cure holds).
5. **Kit lifecycle** — load Full, load Basic, program-change while playing: no crash, meters behave.

---

## Routing notes (Rule 3 application during execution)

- If the investigation surfaces the SAME wrapper bug affecting OTHER CCs (per-note pan CC10, filter CC74, etc.) → fold if it's the same root cause + same fix; else route to §9 + a follow-up batch (surface slot options to Jeff, don't pick).
- If the fix needs a vendored-sfizz change with broad blast radius → surface to Jeff before landing (rebuild cost, future-merge, licensing).
- If a BaySickGuitars/BaySickBasses meter issue appears during regression → it contradicts the "unaffected" premise; diagnose, then route (likely fold if same root cause).
- Diagnostic instrumentation (Rule 4): every trace gets a running-notes catalog row in the SAME edit pass; strip all Remove rows at close (surface the list to Jeff first); Keep/Remove borderline calls are Jeff's.

---

## Carry-Forward Reference touch points

- **Task 1 start:** Carry-Forward §1 (Render Engine Primitives — RustyDrumsProducerTask + RustyInsertTask 1-to-13 fan-out) for dispatcher context only. The meter publish path (`publishPeakReading → drainMeterAtomicsForUI`) + `buildOutputRoutedSfzWrapper` `output=N` synthesis are NOT documented in Carry-Forward (frozen 2026-05-07, pre-Phase-J; confirmed by the QA-DispatcherAffinity + QA-Sfizz carry-forward-contradiction notes). QA-RustyMeter surfaces them as new Implemented Work Log entries at close.
- No other §-sections apply (the wrapper synthesis + per-strip metering are post-freeze surfaces).
