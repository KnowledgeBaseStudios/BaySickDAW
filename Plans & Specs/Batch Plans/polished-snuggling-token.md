# QA-Ea — Bus Solo + Layers/Bass/Drums Output-Path Unification (DSP-09) — Plan (polished-snuggling-token)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/polished-snuggling-token.md`
> Paired running notes: `Plans & Specs/Running Notes/polished-snuggling-token.md`

> **For execution:** steps use `- [ ]` checkbox syntax for tracking. Build commands run by Jeff (`do_build.bat`) — never by Claude. Verify in Debug exe FIRST, then Release (per CLAUDE.md Build System standing rule). Structure conforms to Main Plan §0 "Plan file required sections" (locked 2026-05-11); exemplar `federated-bouncing-cupcake.md` (QA-D).

## Context

QA-Ea is the highest-risk batch of Phase 3: a hot-path audio-engine refactor. The serial render path treats Layers/Bass/Drums (the "triad") + Clips specially via a bespoke `layersBuf+bassBuf+drumsBuf+audioClips+kMaster-accum` sum inside `VibeGraph::processBlock` (VibeGraph.cpp:1547-1570). Every other bus, and the entire MT path (`MasterTask` pull-model, MasterTask.cpp:16-71), uses the uniform `processBus`→`routeInsertOutput`→kMaster-accumulator model. This Phase-1-vs-5F-4b legacy split is the structural root of (a) the DSP-09 bus-solo bug ("Drums plays when Layers solos") and (b) five different scattered bus-solo formulas.

QA-Ea conforms the serial path to the already-correct MT model (**Part B**), then collapses all bus-solo logic into ONE shared `VibeGraph::anyBusSoloed()` helper + one formula (**Part A**). Owner-locked order: **Part B first, then Part A** (the §9 nineteenth Forks diagnosis: "a clean single-gate fix is only possible after the output paths are unified").

**Pre-Part-B prerequisite (folded in 2026-05-18 — §9 twenty-fifth Forks entry).** Building the Part-B test rig surfaced an MT serial-tail divergence: the master recorder, MIDI recorder, and metronome/count-in are fed only in the serial tail (after the MT branch early-return at `PluginProcessor.cpp:1932`) and were never mirrored into MT — so a record-nothing-armed master capture in MT produces a 104-byte empty WAV (the Part-B 'before' reference is impossible in MT until fixed). Fix folds in as a new pre-Part-B source task (**Task 0b**): extract the post-mix tail (MIDI recorder + master recorder + metronome/count-in) into ONE shared helper called from BOTH the serial tail and the MT branch after `dispatchBlock` — the 5th instance of the proven extract pattern (`tapDryRecorder`, `drainMeterAtomicsForUI`, `measureDspLoadAndOverload`, `renderFilePlayPlayer`/`renderAudioClipsForRow`). The Part-B 'before' master is then captured **in MT, after Task 0b** (the extraction is behavior-preserving for the master output, so post-0b is still a valid pre-Part-B reference). ST deletion is its own gated batch QA-Ef; Issue 3 is its own batch QA-Ed; both out of QA-Ea scope (see §9 twenty-fifth Forks entry).

- **Risk:** HIGH — touches master-sum, the MT `MasterTask` structure, the `BusNode` buffer model. Mandatory own `/review-batch` before close (Jeff's safety call).
- **Effort:** medium-large (~8-14 h: Part B ~4-7 h, Part A bus-solo helper ~3-4 h, verify ~2-3 h).
- **Dependencies:** QA-E (same code area + test material; clean recording/strip surface). Landed (closed 2026-05-17, `f903eaa`).
- **Source root:** §9 nineteenth Forks entry (full diagnosis) + Carry-Forward §3 "Bus solo" (lines 292-305) + §4 DSP-09 target (line 321).

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| SC1 | **Part B before Part A** (unify output path → one gate site, then add the single gate) | §9 19th Forks: clean single-gate fix only possible after outputs unified; high-risk surgery lands first, no throwaway 3-site solo code. Jeff-locked this session. |
| SC2 | **All 11 buses uniformly soloable** (layers, bass, drums, fx, clipsbus, voxbus, instbus, voxbus2, instbus2, instbus3, rustybus) — soloing any one silences all others (subject to B1) | Matches the locked DSP-09 target (Carry-Forward:321) "every other bus silenced at master mix". Intended behavior changes: RustyDrumsBus gains a solo gate (was ungated); triad joins the unified set; ClipsBus moves from bespoke 6-bus group to uniform 11. Jeff-locked this session. |
| SC3 | **Part B verified by an in-app null test; serial↔MT is a by-ear toggle check, NOT a bit-compare; Part A by-ear via the 5 DSP-09 scenarios.** Null test = a fixed pre-recorded song (bit-exact source anchor) + record-with-nothing-armed master capture (before vs after Part B) + per-strip polarity ("Reverse") null → dead silence = no regression. | **Methodology corrected 2026-05-17 (Jeff-directed).** Original draft used a fabricated `golden_*_preB.wav` / `fc /b` / serial↔MT-bit-parity ceremony — ungrounded (invented filenames, CLI hashing, not how Jeff verifies) and a false premise (MT reorders float summation → never bit-identical to serial even with zero behavior change). SC3's intent (don't let Part B silently regress the no-solo mix; keep serial AND MT correct) is preserved; only the method changed. Grounded in features verified in code: `masterFile` capture (PluginProcessor.h:670 / .cpp:3555), per-strip polarity (5F-4a), routing dropdown is Vox/Inst/Clips-only (BuilderPage.cpp:2898) so the song anchor must be paired with real Layers/Bass/Drums engine parts. |
| A | Solo+mute same bus → **mute wins** | §9 19th Forks (Jeff). `muted ||` short-circuits the formula. |
| B1 | Signals routed DIRECTLY to kMaster (the `masterExtra` accumulator) are **NOT** wholesale solo-gated | §9 19th Forks (Jeff). Gating the kMaster accumulator would zero the soloed bus's own output (it routes through the same accumulator). Preserved by construction: no `processBus` call gates kMaster. |
| C | Multi-bus solo is **additive** | §9 19th Forks (Jeff). Per-bus formula `(absolo && !thisSolo)` → each soloed bus passes; OR-reduced helper makes it additive. |
| D | Per-strip `_solo` (`isAnyInsertSoloed`) is already global and is **NOT touched** | §9 19th Forks (Jeff). Strip solo is a separate axis (`PluginProcessor.cpp:1725`). |
| GUARD | `anyBusSoloed()` reads bus `_solo` params ONLY; **MUST NEVER** call `isAnyInsertSoloed()` (strip-level) | §9 19th Forks CRITICAL GUARDRAIL. Prior serial bug muted whole buses on strip solo (documented PluginProcessor.cpp:1883-1893). `/review-batch` must verify. |
| OQ-1 | **RESOLVED this session by code read.** `processBus` returns early for the triad at VibeGraph.cpp:1642/1651/1660 (`mXNode->processChainOnly` + peak drain + `return;`); the shared formula at :1775 is receive-group-only; triad solo lives in `BusNode::processChainOnly` at :355-364. Task 4 therefore has TWO concrete edit sites (triad processChainOnly + the :1775 formula), no branching. | Read VibeGraph.cpp:325-369 + :1617-1784 this session. |
| OQ-3 | **RESOLVED.** The 5 DSP-09 scenarios = Main Plan §5 canonical: (1) solo Layers; (2) unsolo; (3) multi-bus additive; (4) solo+mute=mute wins; (5) persistence across save/load. | Main Plan §5 QA-Ea entry verify list. |
| SC4 | **MT serial-tail 3-bug fix folds in as pre-Part-B Task 0b; Part-B 'before' captured in MT after it.** Master recorder + MIDI recorder + metronome/count-in extracted into ONE shared helper called from both the serial tail and the MT branch. ST-path deletion split OUT to gated batch QA-Ef; Issue 3 split OUT to batch QA-Ed; standing rule: new audio-path code = single shared helper both paths, never hand-mirrored. | §9 twenty-fifth Forks entry. Master recorder is dead in MT (104-byte WAV) → Part-B 'before' capture impossible in MT until fixed; the fix is the QA-Ea-verification unblocker. Jeff-locked 2026-05-18. Sequencing of QA-Ed/QA-Ef + this fold = Jeff's confirmed call per `feedback_slot_placement_is_spec_call.md`. |

## Sub-spec calls surfaced for ExitPlanMode

**No sub-spec calls open.** The 3 session spec calls (SC1/SC2/SC3) were answered before plan finalization; sub-calls A/B1/C/D + GUARD are §9-locked; OQ-1 + OQ-3 resolved above. **OQ-2** (does `EffectsBusNode`/`processEffectsBus` have access to call/receive `anyBusSoloed()`) is a mechanical implementation detail resolvable by reading `processEffectsBus` at Task 4 start — NOT a spec call. If anything genuinely ambiguous surfaces mid-execution, I'll surface options + wait (Rule 3).

## Files to modify (per task)

### Task 0 — Open (no source)
- `~/.claude/plans/polished-snuggling-token.md` → mirror to `Plans & Specs/Batch Plans/polished-snuggling-token.md`; delete home copy.
- `Plans & Specs/Main Plan.md` — §5 QA-Ea entry: add `**Plan file:**` pointer line.
- `Plans & Specs/Running Notes/polished-snuggling-token.md` — seed (new file).

### Task 0b — Pre-Part-B: MT serial-tail divergence shared-helper fix (3 bugs)
- [Source/PluginProcessor.h](Source/PluginProcessor.h) — declare the post-mix-tail helper near the `drainMeterAtomicsForUI` / `measureDspLoadAndOverload` decls (same extracted-helper cluster, ~:706-727).
- [Source/PluginProcessor.cpp](Source/PluginProcessor.cpp) — define the helper (MIDI recorder `:2697-2701` + master recorder `:2709-2710` + metronome/count-in `:2712-2857`); replace that serial-tail span with one helper call; add the same call in the MT branch after `dispatchBlock` (~:1914) and before `drainMeterAtomicsForUI` (~:1921). Preserve the D-5 invariant (recorder writes the pre-metronome master — in MT the post-`dispatchBlock` buffer is already pre-metronome).

### Task 1 — Part B: route triad+Clips via routeInsertOutput; neutralize bespoke sum
- [Source/PluginProcessor.cpp](Source/PluginProcessor.cpp) — after `processBus(kClipsBus,...)` (~:2517) add `routeInsertOutput(kClipsBus,...)`; add triad generic loop before the `mVibeGraph.processBlock` call (~:2690).
- [Source/VibeGraph.cpp:1551-1561](Source/VibeGraph.cpp:1551) — comment out the triad + `audioClipsPreRendered` `addFrom`s (master-sum reads kMaster accumulator only).

### Task 2 — Part B: delete bespoke buffers + dead PreRendered plumbing
- [Source/VibeGraph.cpp:1506-1570](Source/VibeGraph.cpp:1506) — replace fill/processBus/sum block with the kMaster-accumulator-only final form.
- [Source/VibeGraph.h:749-752](Source/VibeGraph.h:749) — delete `mLayersBuf/mBassBuf/mDrumsBuf`; retain one `mMasterSumBuf`. `processBlock` signature: drop `layers/bass/drums/audioClipsPreRendered` params.
- [Source/VibeGraph.cpp:1451-1454](Source/VibeGraph.cpp:1451) + [:1385-1390](Source/VibeGraph.cpp:1385) + [:1497-1502](Source/VibeGraph.cpp:1497) — delete allocs/resizes for the 3 deleted buffers; keep `mMasterSumBuf` lifecycle.
- [Source/PluginProcessor.cpp:2686-2695](Source/PluginProcessor.cpp:2686) + [:2520](Source/PluginProcessor.cpp:2520) + [:2372](Source/PluginProcessor.cpp:2372) — delete `layersIn/bassIn/drumsIn/audioClipsBusForGraph`; update the `processBlock` call.

### Task 3 — Part A: add `anyBusSoloed()` helper + cached ptrs
- [Source/VibeGraph.h](Source/VibeGraph.h) — declare `bool anyBusSoloed() const noexcept;` near `isAnyInsertSoloed()` decl; add `std::atomic<float>* mBusSoloPtr[11] {};`.
- [Source/VibeGraph.cpp](Source/VibeGraph.cpp) — define `anyBusSoloed()` near :2688; bind `mBusSoloPtr` in the bus-APVTS rebind site (~:2670-2686) using the 11-prefix list (PluginProcessor.cpp:4568-4584).

### Task 4 — Part A: switch all buses to the single gate
- [Source/VibeGraph.cpp:355-364](Source/VibeGraph.cpp:355) (Layers `processChainOnly`), [:522-529](Source/VibeGraph.cpp:522) (Bass), [:682-689](Source/VibeGraph.cpp:682) (Drums) — replace triad-only `anySolo` with threaded `absolo`.
- [Source/VibeGraph.cpp:1617-1784](Source/VibeGraph.cpp:1617) (`processBus`) — compute `absolo = anyBusSoloed()` once; thread into the triad `processChainOnly` calls (new param); replace the :1775 formula + delete `inGroupSolo`/`useGroupSolo` machinery incl. ClipsBus 6-bus block (:1688-1703) + Rusty `useGroupSolo=false` (:1728).
- [Source/VibeGraph.cpp:946-984](Source/VibeGraph.cpp:946) (`EffectsBusNode`/`processEffectsBus`) — FX uses the single formula (OQ-2: confirm access pattern when implementing).

### Task 5 — Part A: delete dead `busAnySolo` plumbing
- [Source/PluginProcessor.cpp:2572-2577](Source/PluginProcessor.cpp:2572) + [:2616-2621](Source/PluginProcessor.cpp:2616) (serial) + [:1883-1893](Source/PluginProcessor.cpp:1883) + [:1899](Source/PluginProcessor.cpp:1899) (MT) — delete; remove `BlockContext::busAnySolo` if no other consumer (grep first).
- [Source/Engine/Tasks/PassiveStripTask.*](Source/Engine/Tasks/PassiveStripTask.cpp) — drop `ctx.busAnySolo` plumbing into `processBus`.

### Task 6 — Part A cleanup (only if strictly unused)
- [Source/VibeGraph.h](Source/VibeGraph.h) / [Source/VibeGraph.cpp](Source/VibeGraph.cpp) — drop vestigial `anySolo` param from `processBus`/BusNode signatures + dead `pSiblingBass`/`pSiblingDrum` cached ptrs, ONLY if grep confirms zero use; else skip + note retention.

## Tasks

### Task 0 — Open batch + baseline capture (no source change)
- [ ] Mirror `~/.claude/plans/polished-snuggling-token.md` → `Plans & Specs/Batch Plans/polished-snuggling-token.md` (Write); delete the home-dir copy (per `feedback_plan_mirror_one_way.md`).
- [ ] Add `**Plan file:** [Batch Plans/polished-snuggling-token.md](Batch Plans/polished-snuggling-token.md)` to the §5 QA-Ea header in Main Plan.
- [ ] Seed `Plans & Specs/Running Notes/polished-snuggling-token.md` (title + purpose blockquote + pair ref + initial "Task 0: open" entry; per §0 running-notes required sections).
- [ ] **Tell Jeff:** "Build the deterministic 8-bar test project: Layers+Bass+Drums engine parts (sampled / phase-reset patches, **NO noise-oscillator patches** per the CLAUDE.md LCG gotcha) + Vox+Inst+Rusty parts (for Part A solo listening) + a dropped **pre-recorded song** (the bit-exact source anchor; also covers the Clips/audio path). **Metronome OFF.** Keep/save this project — it is the QA-Ea test rig for every later task." (The Part-B 'before' master capture is **NOT** taken here — it moves to **Task 0b**, captured in MT *after* the 3-bug shared-helper fix, because the master recorder is dead in MT until then. §9 twenty-fifth Forks.)
- [ ] Confirm the deterministic test project exists + is saved. The 'before' reference capture + its pre-Part-B confirmation are now Task 0b steps (post-0b, in MT, before any Task-1 source edit).
- [ ] Recommend a `pre-QA-Ea` git tag at this commit (rollback boundary).
- [ ] Surface full git status. Dispatch `/draft-commit`. Surface drafted message + git status to Jeff for approval. Commit on approval.
- [ ] Dispatch `/draft-doc running-notes` post-commit and apply.

### Task 0b — Pre-Part-B: MT serial-tail divergence shared-helper fix (3 bugs)

Prerequisite for the Part-B 'before' capture (master recorder is dead in MT — §9 twenty-fifth Forks). Source change; lands before Task 1. The extraction is behavior-preserving for the master output itself (it only ALSO feeds the recorder + runs metro in MT), so the post-0b master is a valid pre-Part-B null reference.

- [ ] Read the serial-tail recorder/MIDI/metronome span (`PluginProcessor.cpp:2697-2857`) + the MT branch (`:1872-1933`) + the extracted-helper decl cluster (`PluginProcessor.h:~706-727`) before editing.
- [ ] Extract MIDI recorder (`:2697-2701`) + master recorder (`:2709-2710`) + metronome/count-in DSP (`:2712-2857`) into ONE shared helper (e.g. `applyPostMixRecordAndMetro(buffer, …)`), declared in `PluginProcessor.h` beside `drainMeterAtomicsForUI`/`measureDspLoadAndOverload`.
- [ ] Replace the serial-tail span with a single call to the helper (same position — preserves the D-5 pre-metronome recorder ordering).
- [ ] Add the same helper call in the MT branch after `dispatchBlock` (~:1914), before `drainMeterAtomicsForUI` (~:1921). The post-`dispatchBlock` buffer is the final master, pre-metronome — correct + click-free for the recorder.
- [ ] **Tell Jeff:** "Build (Debug + Release). In **MT** (default): (1) song mode, nothing armed, Record → master WAV is non-empty + correct audio (not 104 bytes); (2) arm a strip / record with MIDI input if available → MIDI recorder captures notes; (3) metronome ON during playback → audible click; (4) count-in before record → audible 1-2-3-4. Then toggle Multi-core Rendering OFF (**ST**) and re-check all 4 still work (no regression). Tell me any that fail."
- [ ] On Jeff's verify PASS: surface full git status. Dispatch `/draft-commit` (Task 0b own commit). Surface message + status to Jeff for approval. Commit on approval.
- [ ] **Then the Part-B 'before' capture (now in MT):** Tell Jeff — "On the post-0b build, in **MT**, play the deterministic test project with **nothing armed** + Record → that master capture is the **Part-B 'before' reference**. Rename/keep that WAV (e.g. `qaEa_before`)." Confirm it exists, is non-empty, and is post-0b / pre-Task-1 (before any Part-B source edit). Record in running notes (filename + that it is the post-0b / pre-Part-B MT reference). No hashes — the WAV itself is the polarity-null reference.
- [ ] Dispatch `/draft-doc running-notes` post-commit and apply.

### Task 1 — Part B: wire triad + Clips through routeInsertOutput; neutralize bespoke sum

One logical change (new path + old path are mutually exclusive — splitting creates an audible double-gain intermediate state, violating "owner verifies a working state per task").

**Add Clips route (PluginProcessor.cpp, immediately after the existing `processBus(kClipsBus,...)` ~:2517):**

```cpp
// QA-Ea Part B (Q2): serial currently reaches master for Clips ONLY via the
// bespoke audioClipsPreRendered sum.  Route kClipsBus → kMaster like every
// other bus (the kClipsBus→kMaster edge already exists; MT/MasterTask uses it).
routeInsertOutput (MixerChannelIds::kClipsBus, clipsBus, numSamples);
```

**Add triad generic loop (PluginProcessor.cpp, immediately before the `mVibeGraph.processBlock(...)` call ~:2690 — mirrors the bus loop at :2588-2593):**

```cpp
// QA-Ea Part B: route Layers/Bass/Drums through the generic path so the
// serial path matches the MT MasterTask model.  Inserts already fan into
// these accumulators (PluginProcessor.cpp:1972/2011/2047).
for (int busChId : { MixerChannelIds::kLayersBus,
                     MixerChannelIds::kBassBus,
                     MixerChannelIds::kDrumsBus })
{
    auto* accum = mVibeGraph.getChannelAccumulator (busChId);
    if (accum == nullptr || accum->getNumChannels() < 2) continue;
    mVibeGraph.processBus (busChId, *accum, bpmForInserts,
                           /*anySolo unused post-Part-A*/ false, panLaw);
    routeInsertOutput (busChId, *accum, numSamples);
}
```

**Neutralize the bespoke sum (VibeGraph.cpp:1551-1561) so it reads ONLY the kMaster accumulator (prevents double-count):**

```cpp
// BEFORE (VibeGraph.cpp:1551-1561):
sumBuf.addFrom(c, 0, layersBuf, c, 0, numSamples);
sumBuf.addFrom(c, 0, bassBuf,   c, 0, numSamples);
sumBuf.addFrom(c, 0, drumsBuf,  c, 0, numSamples);
// ... + audioClipsPreRendered addFrom ...

// AFTER — triad + clips now arrive via the kMaster accumulator (masterExtra
// block at :1566-1570 unchanged).  QA-Ea Part B: these addFroms removed;
// Task 2 deletes the buffers + this whole block entirely.
// (lines commented/removed; masterExtra kMaster-accum read remains)
```

- [ ] Add the Clips `routeInsertOutput` call.
- [ ] Add the triad generic loop.
- [ ] Comment out the triad + `audioClipsPreRendered` `addFrom`s in the bespoke sum.
- [ ] **Tell Jeff:** "Run `do_build.bat`. Verify in Debug, then Release:
  - **(1) Part-B null test.** Open the test project. Record-nothing-armed → master 'after-T1' capture. Put 'after-T1' + the Task-0 'before' capture on two grid rows, flip per-strip polarity ('Reverse') on one, play together. **Dead silence = pass.** Loud/audible residual = a Part B regression (most likely the Q2 Clips route, or a double/drop on L/B/D) — tell me what you hear.
  - **(2) Mix sanity (ear).** Play normally, no solo: full mix present — Layers/Bass/Drums + the song all audible, nothing obviously dropped or doubled/louder.
  - **(3) Serial vs MT.** Toggle Mixer hamburger 'Multi-core Rendering' OFF then ON — sounds right both ways."
- [ ] Wait for Jeff's Debug+Release verify result.
- [ ] On pass: surface full git status; `/draft-commit`; surface message + status; commit on approval.
- [ ] Dispatch `/draft-doc running-notes` and apply (record the null-test outcome + any residual notes).

### Task 2 — Part B: delete bespoke buffers + dead PreRendered plumbing

**`VibeGraph::processBlock` final form (replace VibeGraph.cpp:1521-1570):**

```cpp
// QA-Ea Part B: master input = the kMaster accumulator ONLY (now fed by
// routeInsertOutput for EVERY bus incl. L/B/D + Clips + direct-to-master
// sends).  Mirrors the MT MasterTask pull-model (MasterTask.cpp:43-65).
juce::AudioBuffer<float> sumBuf (mMasterSumBuf.getArrayOfWritePointers(),
                                 numCh, numSamples);
sumBuf.clear();
if (auto* master = getChannelAccumulator (MixerChannelIds::kMaster))
    for (int c = 0; c < numCh; ++c)
        sumBuf.addFrom (c, 0, *master, c, 0, numSamples);
processMasterBus (sumBuf, bpm);   // :1575 onward unchanged
```

- [ ] Apply the final form; delete `fillFromPreRendered` lambda + the 3 triad `processBus` calls + the whole bespoke sum block.
- [ ] VibeGraph.h: drop `layers/bass/drums/audioClipsPreRendered` params from `processBlock`; delete `mLayersBuf/mBassBuf/mDrumsBuf`; keep one `mMasterSumBuf`.
- [ ] Delete allocs/resizes for the 3 deleted buffers (VibeGraph.cpp:1451-1454, :1385-1390, :1497-1502); keep `mMasterSumBuf`.
- [ ] PluginProcessor.cpp: delete `layersIn/bassIn/drumsIn/audioClipsBusForGraph` (:2686-2688, :2520, :2372); update the `processBlock` call to the new signature.
- [ ] Grep `mLayersBuf|mBassBuf|mDrumsBuf|audioClipsPreRendered|layersPreRendered|bassPreRendered|drumsPreRendered` across `Source/` → must return zero.
- [ ] **Tell Jeff:** "Run `do_build.bat`. Verify in Debug, then Release:
  - **(1) Compile clean.** Both Debug + Release green in `build_log.txt`.
  - **(2) Part-B null test.** Record-nothing-armed → master 'after-T2'. Null vs the Task-0 'before' (polarity-flip one, play together) → **dead silence = pass.** Residual = a regression in the buffer-deletion/signature change (isolated from Task 1's routing, which already nulled clean).
  - **(3) Mix sanity (ear) + serial/MT toggle** both sound right (as Task 1)."
- [ ] Wait for Jeff's Debug+Release verify result.
- [ ] On pass: surface full git status; `/draft-commit`; surface; commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Part B GATE (mandatory — do NOT start Task 3 until green)

- [ ] Task 1 + Task 2 null tests vs the Task-0 'before' capture = dead silence (or only benign sub-audible block-size hiss — Jeff's judgment; a loud/obvious residual blocks the gate).
- [ ] No-solo mix audibly unchanged; "Multi-core Rendering" ON and OFF both correct.
- [ ] Owner sign-off recorded in running notes.

### Task 3 — Part A: add `anyBusSoloed()` helper + cached ptrs (no behavior change)

**Helper (VibeGraph.cpp, near :2688 by `isAnyInsertSoloed`):**

```cpp
bool VibeGraph::anyBusSoloed() const noexcept
{
    // GUARDRAIL (QA-Ea / §9 19th Forks): reads BUS _solo params ONLY.
    // MUST NEVER call isAnyInsertSoloed() (strip-level) — the prior serial
    // bug muted whole buses when one strip soloed (warned at
    // PluginProcessor.cpp:1883-1893).  Sub-call D: strip solo is a separate
    // axis and is untouched.
    for (auto* p : mBusSoloPtr)        // 11 cached atomic<float>* (bus _solo)
        if (p != nullptr && p->load (std::memory_order_relaxed) > 0.5f)
            return true;
    return false;
}
```

**Bind in the bus-APVTS rebind site (~VibeGraph.cpp:2670-2686), 11 prefixes:**

```cpp
// QA-Ea Part A: cache bus _solo atomics (CPU-safeguarding standing rule —
// avoid 11 string-keyed getRawParameterValue lookups per bus per block).
static constexpr const char* kBusSoloPrefixes[11] = {
    "mixer_layers", "mixer_bass", "mixer_drums", "mixer_fx", "mixer_clipsbus",
    "mixer_voxbus", "mixer_instbus", "mixer_voxbus2", "mixer_instbus2",
    "mixer_instbus3", "mixer_rustybus" };
for (int i = 0; i < 11; ++i)
    mBusSoloPtr[i] = mApvts ? mApvts->getRawParameterValue (
        juce::String (kBusSoloPrefixes[i]) + "_solo") : nullptr;
```

- [ ] Declare `anyBusSoloed()` + `mBusSoloPtr[11]` in VibeGraph.h.
- [ ] Define the helper + add binding to the rebind site. Confirm the rebind runs on every APVTS rebind (sample-rate change, state load) so ptrs never stale-null at audio time — inspect all callers.
- [ ] Helper is bound but **not yet called** (one-task "no dead wiring" exception — immediately consumed by Task 4; noted, not a violation pattern).
- [ ] **Tell Jeff:** "Run `do_build.bat`. Verify in Debug, then Release:
  - **(1) Compile clean.**
  - **(2) Ear sanity:** play the test project (no solo) — sounds identical (helper is bound but unused → zero behavior change). Optionally re-null vs the Task-0 'before' → still silent."
- [ ] Wait for Jeff's Debug+Release verify result.
- [ ] On pass: surface git status; `/draft-commit`; surface; commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 4 — Part A: switch all buses to the single gate

**Triad `processChainOnly` (Layers VibeGraph.cpp:355-364; Bass :522-529; Drums :682-689 mirror):**

```cpp
// BEFORE (Layers — :355-364):
const bool  thisSolo  = loadParam(pSolo,        0.f) > 0.5f;
const bool  bassSolo  = loadParam(pSiblingBass, 0.f) > 0.5f;
const bool  drumSolo  = loadParam(pSiblingDrum, 0.f) > 0.5f;
const bool  anySolo   = thisSolo || bassSolo || drumSolo;     // triad-only (3 buses)
const bool  thisMuted = loadParam(pMute, 0.f) > 0.5f;
const float fadDb     = loadParam(pLevel, 0.f);
const float fadLin    = juce::Decibels::decibelsToGain(fadDb, -60.f);
const float g = (thisMuted || (anySolo && ! thisSolo)) ? 0.f : fadLin;

// AFTER — `absolo` threaded in from processBus (new processChainOnly param):
const bool  thisSolo  = loadParam(pSolo, 0.f) > 0.5f;
const bool  thisMuted = loadParam(pMute, 0.f) > 0.5f;
const float fadDb     = loadParam(pLevel, 0.f);
const float fadLin    = juce::Decibels::decibelsToGain(fadDb, -60.f);
// QA-Ea: unified 11-bus solo gate.  Sub-call A (mute wins): `||` short-circuits.
const float g = (thisMuted || (absolo && ! thisSolo)) ? 0.f : fadLin;
```
(`pSiblingBass`/`pSiblingDrum` become dead → removed in Task 6.)

**Receive-group shared formula (VibeGraph.cpp:1775) + compute `absolo` once at processBus top:**

```cpp
// AT processBus TOP (after the kMaster/kFxBus early-delegates ~:1626):
const bool absolo = anyBusSoloed();      // single source of truth, all 11 buses

// REPLACE the per-bus solo machinery: delete inGroupSolo/useGroupSolo locals
// (:1677-1678), the ClipsBus 6-bus re-derivation block (:1688-1703), and the
// Rusty useGroupSolo=false (:1728).  Formula at :1775:

// BEFORE:
const bool  silenced = muted || (inGroupSolo && useGroupSolo && ! soloed);
// AFTER:
const bool  silenced = muted || (absolo && ! soloed);   // QA-Ea single gate
```
Thread `absolo` into the triad `processChainOnly(buf, bpm, absolo)` calls at :1638/:1647/:1656.

**FX (`EffectsBusNode`/`processEffectsBus`, VibeGraph.cpp:946-984):** replace passed-in `busAnySolo` with `anyBusSoloed()`-derived `absolo`; `silenced = muted || (absolo && !soloed)`. (OQ-2: confirm `processEffectsBus` computes/threads it — mechanical.)

- [ ] Edit the 3 triad `processChainOnly` formulas + add the `absolo` param.
- [ ] Compute `absolo` once in `processBus`; delete `inGroupSolo`/`useGroupSolo` + ClipsBus 6-bus block + Rusty override; apply the single :1775 formula; thread `absolo` into triad calls.
- [ ] Update `EffectsBusNode` FX gate to the single formula.
- [ ] **Tell Jeff:** "Run `do_build.bat`. Verify in Debug, then Release — all by EAR with the test project:
  - **(1) No-solo unchanged.** Play with no solo: mix sounds the same as before Part A. (Optionally re-null vs the Task-0 'before' → still silent; Part A must not change the no-solo path.)
  - **(2) DSP-09 #1 — solo Layers.** Solo the Layers BUS → ONLY Layers audible; Bass/Drums/Vox/Inst/Rusty/song all silent.
  - **(3) DSP-09 #2 — unsolo** → full mix returns.
  - **(4) DSP-09 #3 — multi-bus additive.** Solo Layers AND VoxBus → BOTH audible, everything else silent.
  - **(5) DSP-09 #4 — solo+mute = mute wins.** Solo Drums, then also mute Drums → silence.
  - **(6) DSP-09 #5 — persistence.** Solo Layers, Save, close, reopen → Layers still soloed.
  - **(7) RustyDrumsBus NEW gate.** Solo Layers → Rusty now SILENT (was ungated pre-QA-Ea — intended SC2 change). Solo Rusty alone → only Rusty audible.
  - **(8) GUARDRAIL.** Solo a single STRIP (one Layer insert, NOT the bus) → the Layers bus is NOT wholesale silenced (strip-solo is the separate Sub-call D path, untouched).
  - **(9) B1.** Route one insert's main-out directly to Master (Properties → Routes to → Master/Clips as available), solo a DIFFERENT bus → the direct-to-master signal still plays; the soloed bus plays; non-soloed buses silent.
  - **(10) Serial vs MT.** Repeat (2),(4),(5),(7) with 'Multi-core Rendering' OFF then ON — same audible result both ways.
  - Note: Rusty/ClipsBus solo-grouping changes are INTENDED (SC2)."
- [ ] Wait for Jeff's Debug+Release verify result.
- [ ] On pass: surface git status; `/draft-commit`; surface; commit on approval.
- [ ] `/draft-doc running-notes` → apply (all scenario results + intended-change note).

### Task 5 — Part A: delete dead `busAnySolo` plumbing

- [ ] Grep all `busAnySolo` / `.busAnySolo` consumers (incl. "Run MT Diagnostic"); confirm only the solo path reads them.
- [ ] Delete serial `busAnySolo` (PluginProcessor.cpp:2572-2577 + FX recompute :2616-2621) and MT `busAnySolo` (:1883-1893 + `mtCtx.busAnySolo` :1899); remove `BlockContext::busAnySolo` if no other consumer.
- [ ] Remove `ctx.busAnySolo` plumbing in `PassiveStripTask`; update `processBus`/`processEffectsBus` call sites consistently.
- [ ] **Tell Jeff:** "Run `do_build.bat`. Test in Debug, then Release:
  - **(1) Compile clean.** Grep `busAnySolo` → none in `Source/`.
  - **(2) Re-run DSP-09 #1 / #3 / #4 / #7 by ear** — identical audible result to Task 4 (pure dead-code removal, zero behavior delta).
  - **(3) No-solo mix unchanged** (ear)."
- [ ] Wait for Jeff's Debug+Release verify result.
- [ ] On pass: surface git status; `/draft-commit`; surface; commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 6 — Part A cleanup (only if strictly unused)

- [ ] Grep-confirm the `anySolo` param of `processBus`/BusNode + `pSiblingBass`/`pSiblingDrum` are zero-use. If yes: remove. If any use remains: SKIP this task + note retention in running notes (do not force).
- [ ] **Tell Jeff (only if edits made):** "Run `do_build.bat`. Debug then Release: compile clean; no-solo mix unchanged (ear); spot-check DSP-09 #1 by ear."
- [ ] Wait for verify; on pass: surface git status; `/draft-commit`; surface; commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 7 — Close sequence (separate from source commits — clean rollback boundary)
- [ ] Dispatch `/draft-doc batch-close` (synthesis of the running-notes file).
- [ ] Apply the close entry to `Plans & Specs/Implemented Work Log.md` via Edit.
- [ ] Dispatch `/review-batch QA-Ea`. **The review MUST explicitly verify:** (a) `anyBusSoloed()` reads bus `_solo` only, NEVER `isAnyInsertSoloed()`; (b) Sub-call D strip-solo path untouched; (c) no bespoke L/B/D/clips sum remains in `VibeGraph::processBlock`; (d) serial flow matches the generic bus loop shape; (e) MT functionally unchanged except the intended uniform-11 solo behavior.
- [ ] **Surface each `/review-batch` finding individually** (per `feedback_closed_batch_carryforward_via_forks.md`) — no bulk "all deferred". BLOCKER/NEEDS-FIX fixed in-batch; NITs each get options (fold / route-to-X / defer), Jeff picks.
- [ ] Route side findings per Rule 3 — surface placement options to Jeff, never pick the slot.
- [ ] Surface full git status. Dispatch `/draft-commit` for the close commit. Surface message + status. Commit on approval.

## Verification (end-to-end smoke after all source tasks land)

Build clean (Debug + Release). One test project (8-bar loop: L/B/D engine parts + Vox/Inst/Rusty parts + dropped song, metronome OFF), all checks by ear / in-app:

1. **Part B no-regression (null test).** Record-nothing-armed master capture **in MT** after all source tasks; null vs the **Task-0b 'before' capture** (recorded in MT after the 3-bug shared-helper fix, before any Part-B source edit; polarity-flip one, play together) → dead silence (benign sub-audible hiss at worst). Part B is behavior-preserving for the no-solo mix. (Both captures in MT — serial↔MT cannot be bit-identical anyway; the null test is by-ear, per SC3 + §9 twenty-fifth Forks.)
2. **DSP-09 1-5** (Main Plan §5 canonical, by ear): solo Layers → only Layers; unsolo → full mix; multi-bus additive; solo+mute = mute wins; persistence across save/reload.
3. **SC2 intended changes (by ear):** RustyDrumsBus now solo-gated; ClipsBus in the uniform 11-bus group; triad cross-interacts with receive-group buses.
4. **GUARDRAIL (by ear):** single-strip solo does NOT wholesale-silence its bus.
5. **B1 (by ear):** direct-to-Master routes still play while a different bus is soloed.
6. **Serial vs MT (by ear):** "Multi-core Rendering" ON and OFF both produce the correct result for the no-solo mix and the solo scenarios — toggle check, not a compare.
7. **No regressions** in normal playback (no solo/mute): full mix audible, nothing dropped/doubled/level-shifted vs the 'before' capture.

## Routing notes (Rule 3 application during execution)

- Findings discovered mid-batch that are real bugs in QA-Ea's scope → fix in-batch (per `feedback_qa_batches_fix_bugs_dont_defer.md`); record in the close entry's routing table.
- Findings outside QA-Ea's scope → §9 Forks entry in Main Plan + corresponding §5/§6/Future State edits. **Surface placement/slot options to Jeff; never pick the slot** (per `feedback_slot_placement_is_spec_call.md` + `feedback_dont_make_unilateral_spec_calls.md`).
- Re-sighting of a deferred item or a `/review-batch` NIT not individually surfaced → fix in this open batch + §9 Forks back-ref (per `feedback_closed_batch_carryforward_via_forks.md`); never reopen QA-E's closed commits.
- Spec calls (multiple options: scope/slot/name/default/file location) → numbered list to Jeff; don't pick.

### Logged side findings (route at Task 7 close — Jeff picks the slot, NOT picked here)

- **FND-1 — Master recordings: not in Audio Clips browser; phantom Clips strip; "Master Recordings" section + open-in-default-player request.** Reported by Jeff 2026-05-18 during Task 0b verification; explicitly **NOT to be fixed now** — note for end-of-batch routing.
  - **Observed:** a master recording (record-nothing-armed → master-fallback WAV) appears on the **Builder grid**, but does **NOT** appear in the **Audio Clips** browser/section. It **does** create a Clips **mixer strip**; it does **not** create a Clips **page/tab** (the latter being correct/desired).
  - **Jeff's proposed direction (not yet a locked spec):** master recordings get their own **"Master Recordings" section**, plus a **right-click → open in the Windows default media player** action.
  - **Open questions for the follow-up (do NOT resolve now):** (a) is the phantom Clips strip with no page a bug to fix or acceptable; (b) where does a "Master Recordings" section live (browser section vs an Audio-Clips subsection); (c) scope of "open in default player" (master recordings only, or all recorded clips); (d) does the Builder-grid placement stay, change, or coexist.
  - **Routing:** outside QA-Ea's bus-solo scope → handled at the Task 7 close per Rule 3 (§9 Forks entry + §5/§6/Future State as Jeff directs). **Slot/scope/where-it-goes is Jeff's call, surfaced at close — explicitly not picked here** (`feedback_slot_placement_is_spec_call.md` + `feedback_dont_make_unilateral_spec_calls.md`).

- **FND-2 — 16-pad drum kit does not respond to MIDI-keyboard drum pads.** Reported by Jeff 2026-05-18 during MIDI-keyboard testing; **explicitly deferred by Jeff to end-of-batch routing** (not now).
  - **Observed:** with a hardware MIDI keyboard connected (detected by audio settings), live keyboard input plays notes correctly on a Harmless page's piano roll and a Bass page's piano roll (page-focus driven). The **16 drum pads** on the keyboard, tried against a drum kit (Jeff's words: "old drum bus path and not Rusty"), do **not** trigger the drum engine. NOTE for the follow-up: per CLAUDE.md the legacy monolithic BaySickDrums engine was deleted (Phase D, 2026-04-25); "old drum bus path" likely means a DrumPage instance using a drum-kit sound, not the deleted engine — the follow-up confirms the exact path.
  - **Routing:** outside QA-Ea's bus-solo scope → Task 7 close per Rule 3, **Jeff picks the slot, not picked here** (`feedback_slot_placement_is_spec_call.md` + `feedback_dont_make_unilateral_spec_calls.md`).

- **FND-3 — Velocity: core MIDI↔internal formula already matches FL; minor parity deviations; separate synth velocity→amplitude "too loud" curve un-traced.** Raised by Jeff 2026-05-18 after the MIDI-keyboard recording test (hardest hit shows "a little past halfway" on the grid; "synths always seemed too loud at high velocity"). **Confirmed in code, NOT fixed now** — Jeff: log + route at close.
  - **Confirmed faithful (no formula bug):** capture `Source/MidiRecorder.cpp:25` = `msg.getFloatVelocity()` = MIDI/127 (= FL's `V_FL = MIDI/127`); playback `emitPianoNoteOn` `Source/PluginProcessor.cpp:25` = `(int)(note.velocity * 127.f)` (= FL's `MIDI = V_FL×127`); piano-roll velocity lane `ControlLane::getVal` `Source/Standalone/PianoRoll.cpp:2170` returns raw `n.velocity` (0-1), drawn unscaled. The chain is mathematically faithful → "hardest hit ≈ just past halfway on the grid" is the **MIDI keyboard's own velocity curve** (transmitting ~MIDI 70-76 at hardest, not 127), NOT a code bug.
  - **Deviations from EXACT FL parity (fix candidates):** (1) playback uses `(int)` truncation, not FL's `round()` — should be `juce::roundToInt` (truncates down up to 1 step; e.g. 0.7874 → 99 not 100). (2) Default drawn-note velocity is `0.8` (`Source/PatternManager.h:51`), inconsistently `0.75` (`Source/Standalone/PianoRoll.cpp:3791/3810`) and `0.8` (`Source/Standalone/DrumKitGrid.cpp:1414/1576/1804`) — FL default is `0.7874` (100/127); neither FL-matched nor internally consistent.
  - **OPEN — separate concern, NOT yet traced:** "synths too loud at high velocity" is the velocity→**amplitude** curve inside the synth voices (`BaySickSynthVoice` `mCurrentVelocity`, `AdditiveVoice` `mNoteVelocity` w/ `mVelLink`, `VibePlayerDSP`) applied in the render loop — a different axis from the MIDI velocity formula; needs its own investigation when this is picked up.
  - **Routing:** outside QA-Ea's bus-solo scope; NOT blocking Task 0b. Task 7 close per Rule 3 — **Jeff picks the slot, not picked here** (`feedback_slot_placement_is_spec_call.md` + `feedback_dont_make_unilateral_spec_calls.md`).

## Carry-Forward Reference touch points (read at task start)

- **Before Task 1 (Part B):** Carry-Forward §1 MasterTask description (lines 84-85: "terminal sink. Pulls 11 buses + direct-to-master sends → processMasterBus") + the **"No dead wiring" rule** (line 404) — the serial change must keep serial+MT in lockstep.
- **Before Task 3/4 (Part A):** Carry-Forward §3 "Bus solo (VibeGraph.cpp)" cluster (lines 292-305 — enumerates the scattered formulas + the diagnose-first directive) + §4 DSP-09 target behavior (line 321: "solo a bus → that bus + everything routed into it plays; every other bus silenced; NOT FL-style global"). These are FROZEN reference; the diagnosis here supersedes via §9 19th Forks (do not edit Carry-Forward).
- **Throughout:** §8 "Diagnose before fixing" (lines 456-458) + APVTS isIdentity/dirty + fast-path-bypass-via-atomic guardrails (405-406) — relevant if any DSP touch surfaces.
