# QA-Ea — Bus Solo + Layers/Bass/Drums Output-Path Unification (DSP-09) — Plan (polished-snuggling-token)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/polished-snuggling-token.md`
> Paired running notes: `Plans & Specs/Running Notes/polished-snuggling-token.md`

> **For execution:** steps use `- [ ]` checkbox syntax for tracking. Build commands run by Jeff (`do_build.bat`) — never by Claude. Verify in Debug exe FIRST, then Release (per CLAUDE.md Build System standing rule). Structure conforms to Main Plan §0 "Plan file required sections" (locked 2026-05-11); exemplar `federated-bouncing-cupcake.md` (QA-D).

## Context

QA-Ea is the highest-risk batch of Phase 3: a hot-path audio-engine refactor. The serial render path treats Layers/Bass/Drums (the "triad") + Clips specially via a bespoke `layersBuf+bassBuf+drumsBuf+audioClips+kMaster-accum` sum inside `VibeGraph::processBlock` (VibeGraph.cpp:1547-1570). Every other bus, and the entire MT path (`MasterTask` pull-model, MasterTask.cpp:16-71), uses the uniform `processBus`→`routeInsertOutput`→kMaster-accumulator model. This Phase-1-vs-5F-4b legacy split is the structural root of (a) the DSP-09 bus-solo bug ("Drums plays when Layers solos") and (b) five different scattered bus-solo formulas.

QA-Ea conforms the serial path to the already-correct MT model (**Part B**), then collapses all bus-solo logic into ONE shared `VibeGraph::anyBusSoloed()` helper + one formula (**Part A**). Owner-locked order: **Part B first, then Part A** (the §9 nineteenth Forks diagnosis: "a clean single-gate fix is only possible after the output paths are unified").

- **Risk:** HIGH — touches master-sum, the MT `MasterTask` structure, the `BusNode` buffer model. Mandatory own `/review-batch` before close (Jeff's safety call).
- **Effort:** medium-large (~8-14 h: Part B ~4-7 h, Part A bus-solo helper ~3-4 h, verify ~2-3 h).
- **Dependencies:** QA-E (same code area + test material; clean recording/strip surface). Landed (closed 2026-05-17, `f903eaa`).
- **Source root:** §9 nineteenth Forks entry (full diagnosis) + Carry-Forward §3 "Bus solo" (lines 292-305) + §4 DSP-09 target (line 321).

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| SC1 | **Part B before Part A** (unify output path → one gate site, then add the single gate) | §9 19th Forks: clean single-gate fix only possible after outputs unified; high-risk surgery lands first, no throwaway 3-site solo code. Jeff-locked this session. |
| SC2 | **All 11 buses uniformly soloable** (layers, bass, drums, fx, clipsbus, voxbus, instbus, voxbus2, instbus2, instbus3, rustybus) — soloing any one silences all others (subject to B1) | Matches the locked DSP-09 target (Carry-Forward:321) "every other bus silenced at master mix". Intended behavior changes: RustyDrumsBus gains a solo gate (was ungated); triad joins the unified set; ClipsBus moves from bespoke 6-bus group to uniform 11. Jeff-locked this session. |
| SC3 | **Add an explicit serial↔MT bit-parity verify gate** (existing Mixer-hamburger "Multi-core Rendering" toggle + Render-to-WAV, metronome OFF), alongside the no-solo bit-compare + 5 DSP-09 scenarios | §5 flags the MT MasterTask surface; Carry-Forward "No dead wiring" mandates serial+MT both exercised; §5 verify list omitted an explicit parity check. Jeff-locked this session. (MT runs in Debug too — QA-Md closed 2026-05-09; the old "MT Debug no-op" was a meter-cap misdiagnosis.) |
| A | Solo+mute same bus → **mute wins** | §9 19th Forks (Jeff). `muted ||` short-circuits the formula. |
| B1 | Signals routed DIRECTLY to kMaster (the `masterExtra` accumulator) are **NOT** wholesale solo-gated | §9 19th Forks (Jeff). Gating the kMaster accumulator would zero the soloed bus's own output (it routes through the same accumulator). Preserved by construction: no `processBus` call gates kMaster. |
| C | Multi-bus solo is **additive** | §9 19th Forks (Jeff). Per-bus formula `(absolo && !thisSolo)` → each soloed bus passes; OR-reduced helper makes it additive. |
| D | Per-strip `_solo` (`isAnyInsertSoloed`) is already global and is **NOT touched** | §9 19th Forks (Jeff). Strip solo is a separate axis (`PluginProcessor.cpp:1725`). |
| GUARD | `anyBusSoloed()` reads bus `_solo` params ONLY; **MUST NEVER** call `isAnyInsertSoloed()` (strip-level) | §9 19th Forks CRITICAL GUARDRAIL. Prior serial bug muted whole buses on strip solo (documented PluginProcessor.cpp:1883-1893). `/review-batch` must verify. |
| OQ-1 | **RESOLVED this session by code read.** `processBus` returns early for the triad at VibeGraph.cpp:1642/1651/1660 (`mXNode->processChainOnly` + peak drain + `return;`); the shared formula at :1775 is receive-group-only; triad solo lives in `BusNode::processChainOnly` at :355-364. Task 4 therefore has TWO concrete edit sites (triad processChainOnly + the :1775 formula), no branching. | Read VibeGraph.cpp:325-369 + :1617-1784 this session. |
| OQ-3 | **RESOLVED.** The 5 DSP-09 scenarios = Main Plan §5 canonical: (1) solo Layers; (2) unsolo; (3) multi-bus additive; (4) solo+mute=mute wins; (5) persistence across save/load. | Main Plan §5 QA-Ea entry verify list. |

## Sub-spec calls surfaced for ExitPlanMode

**No sub-spec calls open.** The 3 session spec calls (SC1/SC2/SC3) were answered before plan finalization; sub-calls A/B1/C/D + GUARD are §9-locked; OQ-1 + OQ-3 resolved above. **OQ-2** (does `EffectsBusNode`/`processEffectsBus` have access to call/receive `anyBusSoloed()`) is a mechanical implementation detail resolvable by reading `processEffectsBus` at Task 4 start — NOT a spec call. If anything genuinely ambiguous surfaces mid-execution, I'll surface options + wait (Rule 3).

## Files to modify (per task)

### Task 0 — Open (no source)
- `~/.claude/plans/polished-snuggling-token.md` → mirror to `Plans & Specs/Batch Plans/polished-snuggling-token.md`; delete home copy.
- `Plans & Specs/Main Plan.md` — §5 QA-Ea entry: add `**Plan file:**` pointer line.
- `Plans & Specs/Running Notes/polished-snuggling-token.md` — seed (new file).

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
- [ ] **Tell Jeff:** "Run `do_build.bat` on current `main` (Debug). Pick a deterministic test pattern for the golden capture: **metronome OFF**, no noise-oscillator synths (CLAUDE.md LCG gotcha). Then:
  - **(1)** Mixer hamburger → uncheck **Multi-core Rendering** (serial). Render the pattern to WAV → save as `golden_serial_preB.wav`.
  - **(2)** Mixer hamburger → check **Multi-core Rendering** (MT). Render the same pattern → `golden_mt_preB.wav`.
  - Send both files (or their hashes via `certutil -hashfile <f> SHA256`)."
- [ ] Record both hashes + the serial-vs-MT baseline delta in running notes (serial and MT need not be byte-identical historically; the invariant is "Part B does not regress that delta").
- [ ] Recommend a `pre-QA-Ea` git tag at this commit (rollback boundary).
- [ ] Surface full git status. Dispatch `/draft-commit`. Surface drafted message + git status to Jeff for approval. Commit on approval.
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
- [ ] **Tell Jeff:** "Run `do_build.bat`. Test in Debug, then Release:
  - **(1) Serial no-solo bit-compare.** Hamburger → Multi-core Rendering OFF. Render the Task-0 test pattern → `t1_serial.wav`. Compare to `golden_serial_preB.wav` (`fc /b golden_serial_preB.wav t1_serial.wav` — must report 'no differences'). Bit-identical = behavior preserved.
  - **(2) MT untouched.** Hamburger → Multi-core Rendering ON. Render → `t1_mt.wav`. `fc /b golden_mt_preB.wav t1_mt.wav` → no differences.
  - **(3) Serial↔MT parity.** Note whether `t1_serial.wav` vs `t1_mt.wav` differ by the SAME delta as the Task-0 baseline (`golden_serial_preB` vs `golden_mt_preB`). Part B must not regress parity.
  - **(4) Audible clips check.** Play the pattern in serial mode — audio clips still audible (Q2 sharp edge: if the kClipsBus→kMaster route is missing at runtime, clips go silent — (1) catches it since golden has clips)."
- [ ] Wait for Jeff's Debug+Release verify result.
- [ ] On pass: surface full git status; `/draft-commit`; surface message + status; commit on approval.
- [ ] Dispatch `/draft-doc running-notes` and apply (record the 4 WAV hashes + parity delta).

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
- [ ] **Tell Jeff:** "Run `do_build.bat`. Test in Debug, then Release:
  - **(1) Compile clean.** Both Debug + Release green in `build_log.txt`.
  - **(2) Serial no-solo bit-compare.** Serial render → `fc /b golden_serial_preB.wav t2_serial.wav` → no differences.
  - **(3) MT untouched.** MT render → `fc /b golden_mt_preB.wav t2_mt.wav` → no differences.
  - **(4) Serial↔MT parity delta == Task-0 baseline.**"
- [ ] Wait for Jeff's Debug+Release verify result.
- [ ] On pass: surface full git status; `/draft-commit`; surface; commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Part B GATE (mandatory — do NOT start Task 3 until green)

- [ ] Task 1 + Task 2 serial renders bit-identical to `golden_serial_preB.wav`.
- [ ] Serial↔MT parity delta unchanged from Task-0 baseline (SC3 explicit parity gate executed + recorded).
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
- [ ] **Tell Jeff:** "Run `do_build.bat`. Test in Debug, then Release:
  - **(1) Compile clean.**
  - **(2) Serial + MT no-solo bit-compare** vs `golden_*_preB.wav` → no differences (helper unused = zero behavior change)."
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
- [ ] **Tell Jeff:** "Run `do_build.bat`. Test in Debug, then Release — full matrix:
  - **(1) No-solo regression.** Serial + MT render (no solo) → `fc /b` vs `golden_*_preB.wav` → no differences.
  - **(2) DSP-09 #1 — solo Layers.** Project with Layers + Bass + Drums + a Vox/Inst bus all audible. Solo the Layers BUS. Expect: ONLY Layers audible; Bass/Drums/Vox/Inst silent.
  - **(3) DSP-09 #2 — unsolo.** Un-solo Layers. Expect: full mix returns.
  - **(4) DSP-09 #3 — multi-bus additive.** Solo Layers AND VoxBus. Expect: BOTH Layers + Vox audible; others silent.
  - **(5) DSP-09 #4 — solo+mute = mute wins.** Solo Drums, then also mute Drums. Expect: silence (mute wins over its own solo).
  - **(6) DSP-09 #5 — persistence.** Solo Layers, Save project, close, reopen. Expect: Layers still soloed; mix state restored.
  - **(7) RustyDrumsBus (NEW gate).** Add a BaySickRustyDrums tab (audible). Solo Layers. Expect: Rusty now SILENT (was ungated pre-QA-Ea — intended change). Solo Rusty alone → only Rusty audible.
  - **(8) GUARDRAIL test.** Solo a single STRIP (one Layer insert, not the bus). Expect: the Layers bus is NOT wholesale silenced — only strip-level solo applies (Sub-call D path untouched).
  - **(9) B1 test.** Route one insert's main-out directly to Master (Properties → Routing → Master), solo a DIFFERENT bus. Expect: the direct-to-master signal still plays; the soloed bus plays; non-soloed buses silent.
  - **(10) Serial↔MT parity.** Repeat (2),(4),(5),(7) in serial vs MT (hamburger toggle) — results identical within Task-0 baseline delta.
  - Note: RustyDrumsBus/ClipsBus solo-grouping changes are INTENDED (SC2)."
- [ ] Wait for Jeff's Debug+Release verify result.
- [ ] On pass: surface git status; `/draft-commit`; surface; commit on approval.
- [ ] `/draft-doc running-notes` → apply (all scenario results + intended-change note).

### Task 5 — Part A: delete dead `busAnySolo` plumbing

- [ ] Grep all `busAnySolo` / `.busAnySolo` consumers (incl. "Run MT Diagnostic"); confirm only the solo path reads them.
- [ ] Delete serial `busAnySolo` (PluginProcessor.cpp:2572-2577 + FX recompute :2616-2621) and MT `busAnySolo` (:1883-1893 + `mtCtx.busAnySolo` :1899); remove `BlockContext::busAnySolo` if no other consumer.
- [ ] Remove `ctx.busAnySolo` plumbing in `PassiveStripTask`; update `processBus`/`processEffectsBus` call sites consistently.
- [ ] **Tell Jeff:** "Run `do_build.bat`. Test in Debug, then Release:
  - **(1) Compile clean.** Grep `busAnySolo` → none in `Source/`.
  - **(2) Re-run DSP-09 #1/#3/#4/#7 + serial↔MT parity** — byte-identical to Task 4 results (pure dead-code removal, zero behavior delta).
  - **(3) No-solo bit-compare** vs `golden_*_preB.wav` → no differences."
- [ ] Wait for Jeff's Debug+Release verify result.
- [ ] On pass: surface git status; `/draft-commit`; surface; commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 6 — Part A cleanup (only if strictly unused)

- [ ] Grep-confirm the `anySolo` param of `processBus`/BusNode + `pSiblingBass`/`pSiblingDrum` are zero-use. If yes: remove. If any use remains: SKIP this task + note retention in running notes (do not force).
- [ ] **Tell Jeff (only if edits made):** "Run `do_build.bat`. Debug then Release: compile clean; no-solo bit-compare vs golden (no differences); spot-check DSP-09 #1 serial↔MT."
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

Build clean (Debug + Release). Using the Task-0 test pattern + hamburger Multi-core toggle, metronome OFF:

1. **No-solo bit-identity.** Serial render == `golden_serial_preB.wav`; MT render == `golden_mt_preB.wav` (Part B is behavior-preserving for the no-solo mix).
2. **Serial↔MT parity** unchanged from Task-0 baseline delta across the DSP-09 scenarios.
3. **DSP-09 1-5** (Main Plan §5 canonical): solo Layers → only Layers; unsolo → full mix; multi-bus additive; solo+mute = mute wins; persistence across save/load.
4. **SC2 intended changes:** RustyDrumsBus now solo-gated; ClipsBus in the uniform 11-bus group; triad cross-interacts with receive-group buses.
5. **GUARDRAIL:** single-strip solo does NOT wholesale-silence its bus.
6. **B1:** direct-to-Master routes still play while a different bus is soloed.
7. **No regressions** in normal playback (no solo/mute engaged): full mix audible, levels unchanged vs golden.

## Routing notes (Rule 3 application during execution)

- Findings discovered mid-batch that are real bugs in QA-Ea's scope → fix in-batch (per `feedback_qa_batches_fix_bugs_dont_defer.md`); record in the close entry's routing table.
- Findings outside QA-Ea's scope → §9 Forks entry in Main Plan + corresponding §5/§6/Future State edits. **Surface placement/slot options to Jeff; never pick the slot** (per `feedback_slot_placement_is_spec_call.md` + `feedback_dont_make_unilateral_spec_calls.md`).
- Re-sighting of a deferred item or a `/review-batch` NIT not individually surfaced → fix in this open batch + §9 Forks back-ref (per `feedback_closed_batch_carryforward_via_forks.md`); never reopen QA-E's closed commits.
- Spec calls (multiple options: scope/slot/name/default/file location) → numbered list to Jeff; don't pick.

## Carry-Forward Reference touch points (read at task start)

- **Before Task 1 (Part B):** Carry-Forward §1 MasterTask description (lines 84-85: "terminal sink. Pulls 11 buses + direct-to-master sends → processMasterBus") + the **"No dead wiring" rule** (line 404) — the serial change must keep serial+MT in lockstep.
- **Before Task 3/4 (Part A):** Carry-Forward §3 "Bus solo (VibeGraph.cpp)" cluster (lines 292-305 — enumerates the scattered formulas + the diagnose-first directive) + §4 DSP-09 target behavior (line 321: "solo a bus → that bus + everything routed into it plays; every other bus silenced; NOT FL-style global"). These are FROZEN reference; the diagnosis here supersedes via §9 19th Forks (do not edit Carry-Forward).
- **Throughout:** §8 "Diagnose before fixing" (lines 456-458) + APVTS isIdentity/dirty + fast-path-bypass-via-atomic guardrails (405-406) — relevant if any DSP touch surfaces.
