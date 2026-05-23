# QA-Eg — Bus-Meter Draining Unification (G1 standardization) — Plan (squishy-scribbling-flurry)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/squishy-scribbling-flurry.md`
> Paired running notes: `Plans & Specs/Running Notes/squishy-scribbling-flurry.md`

> **For execution:** use `superpowers:executing-plans` inline. Steps use `- [ ]` checkbox syntax for tracking. Build commands run by Jeff (`do_build.bat`) — never by Claude. Verify in Debug exe FIRST, then Release (per CLAUDE.md Build System standing rule).

## Context

QA-Eg is the next Phase 1 batch after QA-Ef per the §6 sequencing arrow (`... → QA-Ef************* → QA-Eg*************** → QA-Ed → ...`). It unifies bus-meter draining onto the G1 pattern — each bus node owns its peak; the UI polls nodes directly via `drainMeterAtomicsForUI` (the FL Studio mixer model) — eliminating the G2 intermediate `PluginProcessor::*Run` mirrors that were a VST/AU plugin-segregation workaround unnecessary for a standalone.

**Source-confirmed pre-batch state (2026-05-23):**

- **G1 buses today** (Layers / Bass / Drums / Master): each bus node has internal `peakDb / peakDbL / peakDbR` atomics ([Source/VibeGraph.cpp:223, :426, :593, :757](Source/VibeGraph.cpp), within the bus-node struct definitions). The BusNode's process call publishes into them via `publishPeakReading` (e.g. [LayersBusNode at :1031-1040](Source/VibeGraph.cpp:1031)). At end of `processBus`'s L/B/D dispatch ([VibeGraph.cpp:1545-1547 / :1554-1556 / :1563-1565](Source/VibeGraph.cpp:1545)), `exchange()`-stores promote node-internal peaks into VibeGraph public-member atomics (`layersPeakDb` etc.). `drainMeterAtomicsForUI`'s G1 loop ([PluginProcessor.cpp:2097-2108](Source/PluginProcessor.cpp:2097)) `drainAndMerge`'s those VibeGraph members into the processor mirrors (`mLayersPeakDb` etc.) the UI polls via `MixerPage::drainStereoBus`.
- **G2 buses today** (AudioClips / Vox / Inst / Vox2 / Inst2 / Inst3 / Rusty): backed by `InstrChannelNode` which has **no peak atomics at all** ([VibeGraph.cpp:1276-1332](Source/VibeGraph.cpp:1276) — fields are just `name / preEq / rack / eq / pPolarity / pWidth`). Current publishing path: `VibeGraph::processBus`'s generic-bus section measures `buf.getMagnitude()` and CAS-maxes into a per-bus `mBusPeakRefs[busChId]` entry ([VibeGraph.cpp:1684-1705](Source/VibeGraph.cpp:1684)), where `mBusPeakRefs` holds raw pointers to `PluginProcessor::m<Bus>PeakDb*Run` atomics registered via `VibeGraph::registerBusPeakAtomics` ([PluginProcessor.cpp:288-315](Source/PluginProcessor.cpp:288)). `drainMeterAtomicsForUI`'s G2 promotion loop ([PluginProcessor.cpp:2112-2151](Source/PluginProcessor.cpp:2112)) promotes `*Run → mFxBusPeakDb` etc. (the snapshot mirrors UI polls).
- **FX bus is a hybrid**: `EffectsBusNode` already has internal peak atomics ([VibeGraph.cpp:906, :911-912](Source/VibeGraph.cpp:906)) — structurally G1-shaped — but isn't in the G1 drain loop. The QA-Ef interim fix bridges via `drainEffectsBusPeakDbStereo()` (reads + exchange-resets node atomics) plus a CAS-max indirection into the G2 `mFxBusPeakDb*Run` mirrors ([PluginProcessor.cpp:2115-2133](Source/PluginProcessor.cpp:2115)). Transitional G2-style piece per the §9 twenty-eighth Forks entry.

**Risk:** **low-medium** — meter / UI-state only; audio path arithmetic unaffected. Touches `VibeGraph.cpp` + `VibeGraph.h` (new node-owned atomics + new public-member atomics + processBus per-bus exchange-stores + InstrChannelNode peakDb publishing), `PluginProcessor.h` (mirror field deletions), `PluginProcessor.cpp` (registerBusPeakAtomics-call deletions + `drainMeterAtomicsForUI` rewrite + QA-Ef interim drain removal).

**Effort estimate:** ~5-8 hours (slightly above the §9 ~4-7 hour estimate because the per-bus task structure introduces 5 source-task commits + InstrChannelNode peakDb plumbing on Task 3, which is the structural change the §9 entry undercounted).

**Dependencies:** QA-Ef closed (`ad956bf` + paperwork `fcc2297`). The interim FX-bus meter fix shipped under QA-Ef as a Group-2-style piece; this batch supersedes it. No hard dependency on QA-Ed / QA-Ee / QA-Eb / QA-Ec.

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| S1 | **Bus scope = all 8 G2 buses** (AudioClips / FX / Vox / Inst / Rusty + Vox2 / Inst2 / Inst3) | Jeff 2026-05-23: leaving the secondary buses on the `*Run` mirror re-creates exactly the architectural inconsistency this batch exists to remove. §5 entry's "Clips / Vox / Inst / Rusty + FX" wording was a simplification; the secondaries got the same G2 treatment when they were added (J-7 / G-6 eras) and need the same migration. |
| S2 | **Per-row Builder audio meters DEFERRED** to a new dedicated batch | Jeff 2026-05-23: same architectural smell confirmed by source-read (`mAudioRowPeakDb[]` + `*Run` dual-mirror at PluginProcessor.h:645-654; two publishing sites at PluginProcessor.cpp:620-622 (the shared `renderAudioClipsForRow` helper) and CompositeAudioInsertTask.cpp:113-115 (MT task)) — but folding adds ~3-5 hours + kMaxAudioRows verify scenarios + touches the DSP-12 surface. Routed at QA-Eg close via §9 Forks + new §5 batch per Rule 3 (slot surfaced to Jeff at close-time, not pre-empted here). |
| S3 | **Task structure = per-bus tasks** (FX → Clips → Vox+Vox2 → Inst+Inst2+Inst3 → Rusty → comment sweep) | Jeff 2026-05-23: each task individually verifiable by ear (one bus meter at a time); clean rollback boundaries; matches `feedback_commit_at_checkpoints.md`. FX first because EffectsBusNode already G1-shaped — smallest verification scope to validate the migration pattern before scaling. |
| S4 | **Publishing primitive = match L/B/D/Master exactly** — `publishPeakReading()` with the per-block latency-comp ring buffer (`peakRingL/R/Idx`) | Jeff 2026-05-23: the whole point of standardizing is uniformity across all 11 buses. The simpler raw CAS-max approach (G2's current pattern) would re-introduce a subtle split in per-block meter ballistics. ~5-10 extra lines per bus migration is acceptable for the uniformity gain. |
| S5 | **Silly-name = `squishy-scribbling-flurry`** (runtime-assigned by plan-mode) | My pick per `feedback_silly_name_is_my_pick.md` (not a spec call). |
| S6 | **`peakDecayDbPerBlock` dead field — skip on new, clean up the 5 existing** | Jeff 2026-05-23 mid-Task-1: Task 1 inventory surfaced that `peakDecayDbPerBlock` is initialised + recalculated in `prepare()` on every G1 BusNode (Layers / Bass / Drums / Master / FX) but NEVER read — dead state left behind from the pre-2026-05-02 meter-ballistics model that the lock-free publish rewrite obsoleted. Decision: do NOT add the field to `InstrChannelNode` in Task 3 (no point copying dead state); also DELETE the dead field + its prepare-time recalc lines from the 5 existing G1 BusNodes in Task 7 cleanup. Net post-batch: all 11 buses carry a uniform LIVE field set with no dead carry-over. |

---

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open at plan-mode exit. All four (S1-S4) locked pre-plan-mode by Jeff. **S6 added mid-Task-1 (2026-05-23)** when the inventory surfaced the dead `peakDecayDbPerBlock` field on the 5 G1 BusNodes; decision locked same day.

---

## Files to modify

### Header / declaration files

- [Source/VibeGraph.h](Source/VibeGraph.h) — add public-member atomics `audioClipsPeakDb / audioClipsPeakDbL / audioClipsPeakDbR / fxBusPeakDb / fxBusPeakDbL / fxBusPeakDbR / voxBusPeakDb / voxBusPeakDbL / voxBusPeakDbR / instBusPeakDb / ...` parallel to existing `layersPeakDb` etc. at `:648-651`. Add `peakDb / peakDbL / peakDbR / peakRingL / peakRingR / peakRingIdx` to `struct InstrChannelNode` forward decl at `:681` (definition lives in `.cpp`).
- [Source/PluginProcessor.h](Source/PluginProcessor.h) — DELETE the `*Run` mirror declarations for all 8 buses at `:446-468` (AudioClipsBus / VoxBus / InstBus / FxBus / VoxBus2 / InstBus2 / InstBus3 / RustyDrumsBus, each is 3 atoms). KEEP the snapshot mirrors at `:406-417 + secondary equivalents` (these are what the UI polls via MixerPage). DELETE the comment block at `:413-414` referencing "EffectsBusNode internal atomics so MixerPage can read alongside ..." (replaced by the unified pattern).

### Implementation files

- [Source/VibeGraph.cpp](Source/VibeGraph.cpp) — extend `InstrChannelNode` struct definition at `:1276-1332` with peakDb/L/R atomics + peakRingL/R/Idx + (probably) a small publish helper or call `publishPeakReading` from caller. Add per-bus exchange-store blocks in `processBus` generic-bus section after the existing CAS-max site at `:1684-1705` (replace the entire CAS-max block with publishPeakReading + exchange-store to new VibeGraph member atomics). Add exchange-store after `processEffectsBus` returns from the dispatch at `:1531` (or inside `processEffectsBus` itself if it's the cleaner placement — TBD in Task 2 read-only sub-step).
- [Source/PluginProcessor.cpp](Source/PluginProcessor.cpp) — DELETE the 7 `registerBusPeakAtomics` calls at `:288-315` (becomes obsolete once nothing reads `mBusPeakRefs`). Rewrite `drainMeterAtomicsForUI` at `:2085-2161`:
  - G1 loop expands to include FX + AudioClips + Vox + Inst + Vox2 + Inst2 + Inst3 + Rusty alongside L/B/D/Master.
  - Delete the QA-Ef interim FX-bus drain block at `:2115-2133` (the `casMaxRun` + `drainAndMerge (mFxBusPeakDb*, mFxBusPeakDb*Run)` lines).
  - Delete the G2 promotion loop at `:2112-2151` for the buses (KEEP per-row `mAudioRowPeakDb*` promotion at `:2152-2157` — per S2 deferred). DELETE the reset-to-`-60.0f` initialisers at `:155-163` for the bus `*Run` atomics (KEEP the snapshot mirror initialisers at `:155-157` — they're the surviving UI-poll target).
  - Possibly DELETE `VibeGraph::registerBusPeakAtomics` + `mBusPeakRefs` infrastructure entirely if no other caller remains (Task 7 cleanup pass).
- [Source/VibeGraph.h](Source/VibeGraph.h) — if `registerBusPeakAtomics` becomes unused, remove the declaration + the `mBusPeakRefs` field + the `BusPeakRefs` struct.
- [Source/VibeGraph.cpp](Source/VibeGraph.cpp) — if removing `mBusPeakRefs`, also remove `drainEffectsBusPeakDbStereo` (the QA-Ef interim accessor) at `:1784-1789` and the non-exchange `drainEffectsBusPeakDbStereo` at `:1775-1777` if it had no other consumer. Confirm via grep before deletion (Task 2 sub-step).

---

## Tasks

### Task 0 — Open commit
- [ ] Mirror `~/.claude/plans/squishy-scribbling-flurry.md` → `Plans & Specs/Batch Plans/squishy-scribbling-flurry.md` (Write tool); delete the home-dir copy per `feedback_plan_mirror_one_way.md`.
- [ ] Update Main Plan §5 QA-Eg entry header (`Main Plan.md:1143`) — flip `**Plan file:**` line from `` `<silly-name>.md (when started)` `` to backticked path `` `Plans & Specs/Batch Plans/squishy-scribbling-flurry.md` ``. Match the form of sibling §5 entries (e.g. QA-Ef at `:1127`).
- [ ] Seed `Plans & Specs/Running Notes/squishy-scribbling-flurry.md` with the §0-required header (title / purpose blockquote / pair-file ref / convention ref) + initial Task-0 entry. Match the form of `Plans & Specs/Running Notes/synchronous-dreaming-hummingbird.md`.
- [ ] Surface full git status (every dirty + untracked entry) + dispatch `/draft-commit`. Surface drafted message + git status to Jeff for approval per `feedback_surface_drafted_commit_message_for_approval.md`. Commit on approval.
- [ ] Mark Task 0 done.

### Task 1 — Read-only pre-flight inventory + Jeff surface
- [ ] Re-read `LayersBusNode` / `BassBusNode` / `DrumsBusNode` / `MasterBusNode` / `EffectsBusNode` struct definitions in [VibeGraph.cpp](Source/VibeGraph.cpp) — confirm exact field set (peakDb/L/R + peakRingL/R/Idx?) + confirm the `publishPeakReading` call site within each BusNode's process body. This is the template Task 2-6 will follow when extending InstrChannelNode + adding per-bus exchange-stores.
- [ ] Re-read `processEffectsBus` to find the natural exchange-store insertion point for FX (after the existing peak-publish, before the function returns).
- [ ] Grep for any remaining consumers of `VibeGraph::registerBusPeakAtomics`, `mBusPeakRefs`, `drainEffectsBusPeakDbStereo` outside the migration scope (e.g., in tasks, test harnesses, MixerPage). If consumers exist outside PluginProcessor + VibeGraph, surface to Jeff before proceeding.
- [ ] Confirm by direct read that the 7 G2-bus `*Run` atomics in PluginProcessor.h (`:446-468`) have **no other consumers** beyond `processBus`'s registered-refs CAS-max + `drainMeterAtomicsForUI`'s promotion loop. Grep keys: `mFxBusPeakDbRun`, `mAudioClipsBusPeakDbRun`, `mVoxBusPeakDbRun`, etc. — should ONLY show up in those two surfaces.
- [ ] Surface findings to Jeff as a 1-paragraph plain-English inventory (what InstrChannelNode needs added; whether FX bus migration is "trivial wire-up" or has hidden coupling; whether registerBusPeakAtomics infrastructure can be deleted entirely vs. just left dormant). This is the per-`feedback_design_approval_in_plain_english.md` gate — Jeff confirms the inventory matches what he understood before we touch source.
- [ ] No code changes this task. Tell Jeff: "Pre-flight inventory complete; surfacing findings now; ready for Task 2 (FX bus migration) on your green-light."
- [ ] Wait for Jeff's confirm.
- [ ] Dispatch `/draft-doc running-notes` → apply to running-notes file (Task 1 entry: pre-flight findings + Jeff's green-light).

### Task 2 — FX bus migration (smallest scope — EffectsBusNode already G1-shaped)
- [ ] In [VibeGraph.h:648-651](Source/VibeGraph.h:648) add public-member atomics:
  ```cpp
  std::atomic<float> fxBusPeakDb  { -60.f };
  std::atomic<float> fxBusPeakDbL { -60.f };
  std::atomic<float> fxBusPeakDbR { -60.f };
  ```
  (parallel to `layersPeakDb` etc.)
- [ ] In [VibeGraph.cpp](Source/VibeGraph.cpp) at the natural site (after `processEffectsBus` runs its existing peak publish into `mEffectsBusNode->peakDb*`), add exchange-stores parallel to L/B/D pattern at `:1545-1547`:
  ```cpp
  fxBusPeakDb .store(mEffectsBusNode->peakDb .exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
  fxBusPeakDbL.store(mEffectsBusNode->peakDbL.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
  fxBusPeakDbR.store(mEffectsBusNode->peakDbR.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
  ```
  Exact placement depends on Task 1 finding; if `processEffectsBus` is called from `processBus`'s `kFxBus` dispatch at `:1531`, the exchange-store goes right after that dispatch call (inside `processBus`'s body, mirroring how L/B/D do it).
- [ ] In [PluginProcessor.cpp](Source/PluginProcessor.cpp), in `drainMeterAtomicsForUI` G1 loop (around `:2106-2108`), add FX drain lines parallel to Master:
  ```cpp
  drainAndMerge (mFxBusPeakDb,  mVibeGraph.fxBusPeakDb);
  drainAndMerge (mFxBusPeakDbL, mVibeGraph.fxBusPeakDbL);
  drainAndMerge (mFxBusPeakDbR, mVibeGraph.fxBusPeakDbR);
  ```
- [ ] DELETE the QA-Ef interim FX-bus drain block at [PluginProcessor.cpp:2115-2133](Source/PluginProcessor.cpp:2115) (the comment block + `drainEffectsBusPeakDbStereo` accessor call + the `casMaxRun` lambda + the 3 `casMaxRun` calls + the 3 `drainAndMerge (mFxBusPeakDb*, mFxBusPeakDb*Run)` lines).
- [ ] DELETE `mFxBusPeakDb / mFxBusPeakDbLRun / mFxBusPeakDbRRun / mFxBusPeakDbRun` declarations at [PluginProcessor.h:454-456](Source/PluginProcessor.h:454). **KEEP** the snapshot mirrors `mFxBusPeakDb / mFxBusPeakDbL / mFxBusPeakDbR` at `:415-417` — UI still polls those.
- [ ] DELETE the `*Run` initialisers for FX in [PluginProcessor.cpp:155-163 + the FxBus equivalents](Source/PluginProcessor.cpp:155).
- [ ] Tell Jeff: "Run `do_build.bat`. Then in Debug:
  - **(1)** New project. Open Effects page; drop any DSP module (e.g., Compressor) onto the FX Bus rack. Open Mixer page; route a Layer strip to the FX Bus via the per-strip cable. Drop a Layers tab → Harmless or BaySickSynth. Trigger sound (audition the keyboard, or play a pattern). Verify the FX Bus mixer strip's meter reads activity on the LED bar (matches the meter behavior of Layers/Bass/Drums/Master).
  - **(2)** Stop sound. Verify the FX Bus meter decays to silence (no stuck peak).
  - **(3)** Switch Multi-core Rendering OFF (Mixer hamburger → 'Multi-core Rendering' toggle). Repeat (1). Verify FX Bus meter still reads correctly in serial-diagnostic mode.
  - **(4)** Switch Multi-core Rendering ON. Save the project, reload. Trigger sound. Verify FX Bus meter reads correctly post-reload."
- [ ] Wait for Jeff's verify result.
- [ ] If verify passes: dispatch `/draft-commit`, surface drafted message + full git status, commit on approval.
- [ ] Dispatch `/draft-doc running-notes` → apply to running-notes file.

### Task 3 — AudioClips bus migration (first InstrChannelNode-backed; adds peakDb to InstrChannelNode itself)
- [ ] **InstrChannelNode peakDb plumbing** (one-time structural change; the remaining 6 buses inherit it):
  - In [VibeGraph.cpp:1276-1332](Source/VibeGraph.cpp:1276), extend `struct VibeGraph::InstrChannelNode` with the LIVE-only field set the L/B/D/Master BusNodes carry (confirmed in Task 1; explicitly EXCLUDES `peakDecayDbPerBlock` per S6 — that field is dead carry-over on the existing 5 G1 nodes and gets stripped in Task 7):
    ```cpp
    std::atomic<float> peakDb  { -60.f };
    std::atomic<float> peakDbL { -60.f };
    std::atomic<float> peakDbR { -60.f };
    std::array<float, MeterLatencyComp::kRingSize> peakRingL {}, peakRingR {};
    int peakRingIdx { 0 };
    ```
  - The `prepare` method can stay as-is; the ring buffer's zero-init at struct construction is sufficient.
- [ ] **AudioClipsBus per-bus migration** (the per-bus pattern that Tasks 4-6 will mirror):
  - In [VibeGraph.h:648-651](Source/VibeGraph.h:648) add public-member atomics `audioClipsPeakDb / audioClipsPeakDbL / audioClipsPeakDbR`.
  - In [VibeGraph.cpp:1684-1705](Source/VibeGraph.cpp:1684), in the existing `processBus` generic-bus section's peak-publish block, REPLACE the CAS-max into `mBusPeakRefs` with a `publishPeakReading` call writing into `mAudioClipsBusNode->peakDb*` (using the new InstrChannelNode ring fields). Pattern (parallel to [LayersBusNode at :1031-1040](Source/VibeGraph.cpp:1031)):
    ```cpp
    // Resolve the active node from busChId (helper in Task 1 read-only inventory).
    if (auto* node = nodeForBus(busChId))   // returns InstrChannelNode*
    {
        publishPeakReading (buf,
                            node->peakRingL, node->peakRingR, node->peakRingIdx,
                            node->peakDbL, node->peakDbR, node->peakDb);
    }
    ```
    The existing `mBusPeakRefs[busChId]` CAS-max block at `:1693-1705` is deleted; the publish call is its replacement.
  - For Task 3's AudioClipsBus-only scope: ALSO add an exchange-store right after `processBus` finishes the generic-bus section (or at the end of `processBus` itself, conditional on busChId), parallel to L/B/D pattern at `:1545-1547`:
    ```cpp
    if (busChId == kClipsBus && mAudioClipsBusNode)
    {
        audioClipsPeakDb .store(mAudioClipsBusNode->peakDb .exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        audioClipsPeakDbL.store(mAudioClipsBusNode->peakDbL.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
        audioClipsPeakDbR.store(mAudioClipsBusNode->peakDbR.exchange(kBusNegInf, std::memory_order_relaxed), std::memory_order_relaxed);
    }
    ```
    (Tasks 4-6 will extend this with `else if` branches for their respective buses.)
- [ ] In [PluginProcessor.cpp](Source/PluginProcessor.cpp), in `drainMeterAtomicsForUI` G1 loop, add AudioClips drain lines:
  ```cpp
  drainAndMerge (mAudioClipsBusPeakDb,  mVibeGraph.audioClipsPeakDb);
  drainAndMerge (mAudioClipsBusPeakDbL, mVibeGraph.audioClipsPeakDbL);
  drainAndMerge (mAudioClipsBusPeakDbR, mVibeGraph.audioClipsPeakDbR);
  ```
- [ ] DELETE the AudioClips G2 promotion line at [PluginProcessor.cpp:2112-2114](Source/PluginProcessor.cpp:2112).
- [ ] DELETE `mAudioClipsBusPeakDbRun / mAudioClipsBusPeakDbLRun / mAudioClipsBusPeakDbRRun` at [PluginProcessor.h:446-448](Source/PluginProcessor.h:446).
- [ ] DELETE the `registerBusPeakAtomics` call for `kClipsBus` at [PluginProcessor.cpp:288-291](Source/PluginProcessor.cpp:288).
- [ ] DELETE the `*Run` reset-to-`-60` initialisers for AudioClips.
- [ ] Tell Jeff: "Run `do_build.bat`. Then in Debug:
  - **(1)** New project. Drop a WAV onto the Builder grid (creates an audio clip row + spawns the Clips Bus strip on Mixer page). Open Mixer page. Play. Verify the Clips Bus mixer strip's meter reads activity matching the audio clip's loudness (compare to Master meter for sanity).
  - **(2)** Stop playback. Verify Clips Bus meter decays.
  - **(3)** Switch Multi-core Rendering OFF. Repeat (1). Verify Clips Bus meter still works in serial-diagnostic mode.
  - **(4)** FX Bus regression check: confirm FX Bus meter from Task 2 still reads correctly (no regression from the InstrChannelNode structural change)."
- [ ] Wait for Jeff's verify result.
- [ ] On pass: `/draft-commit`, surface, commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 4 — Vox + Vox2 buses migration
- [ ] In [VibeGraph.h:648-651](Source/VibeGraph.h:648) add public-member atomics `voxBusPeakDb / voxBusPeakDbL / voxBusPeakDbR` and `voxBus2PeakDb / voxBus2PeakDbL / voxBus2PeakDbR`.
- [ ] In [VibeGraph.cpp](Source/VibeGraph.cpp) `processBus` exchange-store block (the conditional `if/else if` chain from Task 3), add Vox + Vox2 branches reading `mVoxBusNode` / `mVoxBus2Node`.
- [ ] In `drainMeterAtomicsForUI` G1 loop add Vox + Vox2 drain lines.
- [ ] DELETE Vox + Vox2 `*Run` declarations + initialisers + G2 promotion lines + `registerBusPeakAtomics` calls.
- [ ] Tell Jeff: "Run `do_build.bat`. Then in Debug:
  - **(1)** New project. Add a Vox tab (Mixer page → Add Mixer Strip → Vox). The primary Vox Bus strip should be present already. Arm the strip; speak / play into the mic input; verify the Vox Bus strip meter reads activity.
  - **(2)** Add a second Vox tab (the secondary Vox Bus 2 strip should auto-appear). Route the second Vox tab to Vox Bus 2 via its per-strip cable. Speak; verify both Vox Bus AND Vox Bus 2 meters read independently.
  - **(3)** Stop input. Verify both meters decay.
  - **(4)** Multi-core OFF; repeat (1)+(2); confirm meters still read in serial-diagnostic mode.
  - **(5)** Regression check: confirm FX Bus + Clips Bus meters from Tasks 2-3 still read correctly."
- [ ] Wait for Jeff's verify result.
- [ ] On pass: `/draft-commit`, surface, commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 5 — Inst + Inst2 + Inst3 buses migration
- [ ] Same shape as Task 4 but for the three Inst buses (kInstBus / kInstBus2 / kInstBus3).
- [ ] Tell Jeff: "Run `do_build.bat`. Then in Debug:
  - **(1)** New project. Add Inst tabs (Mixer page → Add Mixer Strip → Inst type). Verify the primary Inst Bus strip meter reads activity when sound is triggered.
  - **(2)** Add a second + third Inst tab; route them to Inst Bus 2 + Inst Bus 3. Verify all three Inst Bus meters read independently.
  - **(3)** Multi-core OFF; repeat (1)+(2).
  - **(4)** Regression check: FX + Clips + Vox + Vox2 meters still read correctly."
- [ ] Wait for verify, commit on approval, running-notes update.

### Task 6 — Rusty bus migration
- [ ] Same shape as Tasks 4-5 but for `kRustyDrumsBus`.
- [ ] Tell Jeff: "Run `do_build.bat`. Then in Debug:
  - **(1)** New project. Add a BaySickRustyDrums Inst tab. The RustyDrums Bus strip should auto-spawn on Mixer page. Trigger a drum hit (audition the kit graphic or play a pattern). Verify RustyDrums Bus meter reads activity.
  - **(2)** Stop. Verify decay.
  - **(3)** Multi-core OFF; repeat (1).
  - **(4)** Regression check: all 7 previously-migrated bus meters (Layers/Bass/Drums/Master G1 originals + FX + Clips + Vox + Vox2 + Inst + Inst2 + Inst3) still read correctly. **End-to-end stress arrangement**: project with audio on every bus, Multi-core ON. Confirm every bus meter on the Mixer page reads correctly with no glitch/drop/lag vs the pre-batch MT baseline."
- [ ] Wait for verify, commit on approval, running-notes update.

### Task 7 — Infrastructure cleanup + comment sweep + final smoke
- [ ] **S6 dead-field cleanup**: delete `peakDecayDbPerBlock` field + its prepare-time recalc lines from the 5 existing G1 BusNodes (LayersBusNode `:233 + :293`, BassBusNode `:435 + :488`, DrumsBusNode `:602 + :653`, MasterBusNode `:766 + :816`, EffectsBusNode `:916 + :954`). Confirmed dead via Task 1 grep (only InsertNode `:1095 + :1134 + :1261-1262` consumes its OWN copy of the field; the 5 BusNode copies have no readers). Also delete the `kDecayDbPerSec` constant if it's no longer referenced after the 5 BusNode deletions.
- [ ] If no remaining caller of `VibeGraph::registerBusPeakAtomics` / `mBusPeakRefs` / the `BusPeakRefs` struct (confirmed via grep), DELETE all three from `VibeGraph.h/.cpp`. Otherwise leave dormant + add a TODO comment.
- [ ] If no remaining caller of `drainEffectsBusPeakDbStereo()` (the QA-Ef interim accessor at [VibeGraph.cpp:1784-1789](Source/VibeGraph.cpp:1784)) and its non-exchange sibling at `:1775-1777`, DELETE both. Otherwise leave with a "QA-Eg leftover — TODO grep" comment.
- [ ] Comment sweep: grep for "G2", "Group 2", "running-max mirror", "*Run" comments referencing the deleted G2 architecture. Update or delete. Particular hot spots:
  - The comment block above `drainMeterAtomicsForUI` (PluginProcessor.cpp `:2080-2084`) should now describe a unified-G1 drain (not 3-group).
  - The QA-Ef interim FX-bus comment at PluginProcessor.cpp `:2115-2118` is removed with the code; verify no orphan reference.
  - The "5F-4a Batch 6" comments in InstrChannelNode that referenced the audio-clips-bus-only polarity/width pattern stay (unrelated to peak-mirror architecture).
- [ ] Grep cleanliness check: `grep -rn "PeakDbRun\|PeakDbLRun\|PeakDbRRun" Source/` for any G2-mirror reference; result should be empty (or limited to per-row mirrors per S2 deferral).
- [ ] Tell Jeff: "Run `do_build.bat`. Then in Debug + Release:
  - **(1)** Full end-to-end stress: project with audio playing on every bus (L/B/D/Master G1 originals + AudioClips + FX + Vox + Inst + Vox2 + Inst2 + Inst3 + Rusty). Multi-core ON. Verify every mixer-strip bus meter reads correctly with no glitch / drop / lag vs the pre-batch MT baseline you remember.
  - **(2)** Toggle Multi-core OFF (serial-diagnostic mode). Verify the same stress arrangement: every bus meter still reads correctly under serial execution.
  - **(3)** Toggle Multi-core back ON. Save the project. Close. Reopen. Confirm every bus meter still reads correctly post-reload.
  - **(4)** Release re-verify of (1) — same arrangement, Release exe, MT on. Subjective comparison vs pre-batch MT meter behavior (you've been comparing per-task in Debug already; this is the final 'release-binary sanity' pass)."
- [ ] Wait for verify result.
- [ ] On pass: `/draft-commit`, surface, commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 8 — Close sequence
- [ ] Dispatch `/draft-doc batch-close` with a synthesis of the running-notes file.
- [ ] Apply the close entry to `Plans & Specs/Implemented Work Log.md` via Edit.
- [ ] Dispatch `/review-batch QA-Eg`.
- [ ] Address BLOCKERs / NEEDS-FIX in-batch. Defer NITs into close-entry routing table per `feedback_closed_batch_carryforward_via_forks.md`.
- [ ] Route side findings per Rule 3:
  - Resolved in-batch → close entry's routing table.
  - Outside-batch → §9 Forks entry in Main Plan + corresponding §5 / §6 edits. Slot SURFACED to Jeff per `feedback_slot_placement_is_spec_call.md`; don't pick.
  - **Pre-locked at batch open per S2**: per-row Builder audio meters DEFERRED to a new dedicated batch — formalize at close as a §9 Forks entry + new §5 batch row + §6 slot. Surface slot options to Jeff at close-time.
- [ ] Surface full git status (every dirty + untracked entry).
- [ ] Dispatch `/draft-commit` for close commit. Surface message + status. Commit on approval.

---

## Verification (end-to-end smoke)

After Task 7 commit lands (the comment sweep + grep cleanliness commit) and before the close commit (Task 8):

1. **Build clean.** `do_build.bat` Release + Debug both green.
2. **Per-bus meter sanity (MT ON).** Project with audio on every bus (L/B/D/Master + AudioClips + FX + Vox + Vox2 + Inst + Inst2 + Inst3 + Rusty). Every mixer-strip bus meter reads activity matching its source signal. No stuck peaks; clean decay to silence on stop.
3. **Per-bus meter sanity (MT OFF / serial-diagnostic).** Same arrangement, Multi-core OFF. Every bus meter still reads correctly under serial execution.
4. **FX-bus meter regression** (the QA-Ef interim case the batch is meant to supersede): FX-bus meter reads correctly in both MT modes; matches the QA-Ef interim baseline behavior; no regression from removing the `*Run` indirection.
5. **Project save/reload.** Save the stress arrangement; close project; reopen; meters still read correctly post-reload (the project-load shield from QA-Ef stays satisfied; no new races introduced).
6. **Grep cleanliness.** `grep -rn "PeakDbRun\|PeakDbLRun\|PeakDbRRun" Source/` shows no bus-mirror references (per-row `mAudioRowPeakDb*Run` per S2 deferral may remain — confirm only that set survives).
7. **Build-log clean.** No new warnings introduced by InstrChannelNode struct extension or the per-bus migration.

---

## MT-awareness static-analysis

The batch is meter / UI-state only. No audio-thread arithmetic changes. The G1 publishing path (publishPeakReading + exchange-store) is already MT-validated by L/B/D/Master (which use it from PassiveStripTask under MT and from the inline path under serial-diagnostic mode). The exchange-store at end of `processBus` runs on whichever thread executes the bus's PassiveStripTask (worker thread under MT, audio thread under serial-diagnostic) — same as L/B/D today. `drainMeterAtomicsForUI` is called from the audio thread end-of-block (PluginProcessor.cpp:1869), unchanged. The migrated buses adopt the exact same threading shape as L/B/D/Master. No new races introduced.

---

## Routing notes (Rule 3 application during execution)

- Findings about meter behavior on other surfaces (per-insert meters, per-strip meters on tabs, per-row meters per S2 deferral) → log as running-notes entry; per-row routed at close per S2; others surface for routing call at close.
- Findings about the bus-node DSP itself (M/S width / polarity / EQ behavior surfacing while testing) → log as running-notes entry; route at close per Rule 3 (likely outside-batch since this is meter-only scope).
- Findings about MT-vs-serial parity (anything where Multi-core OFF doesn't produce the same meter behavior as Multi-core ON) → may indicate an unmigrated G2 surface or a publishing-site bug; investigate in-batch per `feedback_qa_batches_fix_bugs_dont_defer.md`.
- Findings about per-bus DSP-load impact (the migration adds ring-buffer publishing to 7 InstrChannelNode-backed buses) → expected to be sub-meter-noise but worth eyeballing the DSP-load meter during Task 7's full-stress arrangement. If a real perf delta surfaces, route to `/perf-audit` for follow-up.

---

## Carry-Forward Reference touch points

- **§2 (mixer / routing architecture)** — read at Task 1 start; confirms the 11-bus / channel-ID model the meter migration sits on top of.
- **§4 (lock-free primitives)** — read at Task 1 start; confirms the `publishPeakReading` + ring-buffer + relaxed-store pattern is the locked architectural primitive being adopted across all 11 buses.

---

## Carry-Over

(populated mid-batch on session pause per §0 Rule 2; empty at batch open)
