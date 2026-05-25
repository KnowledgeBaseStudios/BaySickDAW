# QA-AudioMeters — Per-Row Builder Audio Meters G1 Migration — Plan (mellow-bubbling-pancake)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/mellow-bubbling-pancake.md`
> Paired running notes: `Plans & Specs/Running Notes/mellow-bubbling-pancake.md`

> **For execution:** use `superpowers:executing-plans` inline. Steps use `- [ ]` checkbox syntax for tracking. Build commands run by Jeff (`do_build.bat`) — never by Claude. Verify in Debug exe FIRST, then Release (per CLAUDE.md Build System standing rule).

## Context

QA-AudioMeters is the next Phase 1 batch after QA-Eg per the §6 sequencing arrow (`... → QA-Eg*************** → QA-AudioMeters****************** → QA-InsertMaps********************...`). It applies the same G1 migration pattern QA-Eg landed across the 8 buses to the per-row Builder audio meter surface — the last remaining instance of the centralized `PluginProcessor::*Run` mirror pattern. Same architectural smell QA-Eg eliminated (the G2 mirror is a VST/AU plugin-segregation workaround unnecessary for a standalone that owns the whole graph); same low-medium risk envelope (meter / UI-state only; audio arithmetic unaffected); same migration shape (node-internal `peakDb` atomics + `VibeGraph` public-member arrays + a unified G1 drain loop in `drainMeterAtomicsForUI`).

**Source-confirmed pre-batch state (2026-05-24):**

- **Snapshot mirrors (UI poll target)** — [PluginProcessor.h:620-623](Source/PluginProcessor.h:620): three `std::atomic<float> mAudioRowPeakDb / mAudioRowPeakDbL / mAudioRowPeakDbR [kMaxAudioRows]` arrays. `kMaxAudioRows = 50` matches `MixerState::kMaxAudioRows`.
- **`*Run` mirrors (running-max accumulator)** — [PluginProcessor.h:624-629](Source/PluginProcessor.h:624): three matching `*Run` arrays the audio thread CAS-maxes into during the block. Promoted to snapshot at end-of-block.
- **G3 promotion loop** — [PluginProcessor.cpp:2107-2112](Source/PluginProcessor.cpp:2107): `drainAndMerge (mAudioRowPeakDb*, mAudioRowPeakDb*Run)` per row. Located inside `drainMeterAtomicsForUI` after the G1 bus drain loop. The QA-Eg Task 8 NEEDS-FIX-2 sweep relabeled the surrounding comment as "per-row deferred to a separate batch per S2" — this batch is that batch.
- **Initialisers** — [PluginProcessor.cpp:155-163](Source/PluginProcessor.cpp:155): both snapshot (`-60.0f`) and `*Run` (`-inf`) zero-init in a single per-row loop.
- **Publishing sites (8 total)** — confirmed by grep:
  1. [CompositeAudioInsertTask.cpp:113-115](Source/Engine/Tasks/CompositeAudioInsertTask.cpp:113) — Flow A (clip-engine sampler MIDI trigger): after `processInsert`, drains InsertNode peak via `mGraph->drainInsertPeakDbStereo(InsertKind::Audio, mIndex)` + CAS-maxes (pkL, pkR, max(pkL,pkR)) into the three `*Run` arrays.
  2. [PluginProcessor.cpp:585-587](Source/PluginProcessor.cpp:585) — Flow B (arrangement-clip timeline decode) inside `renderAudioClipsForRow`: same drain + CAS-max pattern.
  3-7. **Force-reset paths** ([PluginProcessor.cpp:415](Source/PluginProcessor.cpp:415) `mutedByChoke` / [:420](Source/PluginProcessor.cpp:420) `rowMuted||builderRowMuted` / [:448](Source/PluginProcessor.cpp:448) `filePos>=fileTotal` — all inside `renderAudioClipsForRow`; [:642](Source/PluginProcessor.cpp:642) + [:647](Source/PluginProcessor.cpp:647) + [:668](Source/PluginProcessor.cpp:668) — same three predicates inside `renderFilePlayPlayer`): store `-60.0f` directly into `mAudioRowPeakDbRun[row]` to force the meter to read silent regardless of upstream peak.
- **InsertNode primitives (already in place)** — audio rows ARE InsertNodes (`InsertKind::Audio`, `mInsertsAudio[row]`). InsertNode has `peakDb / peakDbL / peakDbR` atomics (confirmed via [VibeGraph.cpp:1241](Source/VibeGraph.cpp:1241) — the publish site inside `processInsert` stores `juce::jmax(newL, newR)` into `peakDb`). `mGraph->drainInsertPeakDbStereo(InsertKind::Audio, row)` is the existing exchange-drain accessor both publishing sites already use.
- **VibeGraph public-member atomics (existing G1 pattern)** — [VibeGraph.h:634-672](Source/VibeGraph.h:634): each migrated bus has 3 atomics (`fxBusPeakDb / fxBusPeakDbL / fxBusPeakDbR` etc.). Per-row equivalent needs `std::array<std::atomic<float>, kMaxAudioRows>` × 3 axes added in the same block.

**Risk:** **medium-high** (scope-up from L7-revised below). The original §5-entry's "per-row Builder audio meters only" risk envelope (low-medium, meter/UI-state only) expanded post-Task-1 inventory when the L7 / Option B assumption proved structurally wrong. Jeff locked the pivot to L7-revised Option 2 (restructure InsertNode publish entirely to bus-pattern) — affecting all 8 InsertKinds (~140 slots total) + both consumer surfaces (per-row Builder + per-insert Mixer strip). Structural twists vs original plan: (i) InsertNode::process rewritten to use publishPeakReading (CAS-max + latency-comp ring) — replaces the existing load-decay-max-store at `:1231-1242`; (ii) peakDbSnap snapshot-promotion layer removed entirely from InsertNode struct + promoteAllInsertPeakSnapshots; (iii) 8 sets of `VibeGraph::<kind>InsertPeakDb*` public-member arrays added (parallel to the existing `<bus>PeakDb` pattern); (iv) 8 sets of `PluginProcessor::m<Kind>InsertPeakDb*` snapshot mirrors added per L9 (parallel to existing `m<Bus>PeakDb`); (v) `drainInsertPeakDbStereo` either rewritten to read from new mirror or replaced with a new accessor; (vi) MixerTrackStrip per-strip meter consumer rewired to new accessor; (vii) Builder grid consumer reads `mAudioRowPeakDb` (or renamed `mAudioInsertPeakDb`) which is now one of the 8 mirror arrays; (viii) L8 / Sub-C B2 still applies — 6 force-reset stores deleted; per-row meters now decay over ~20ms (DBFSMeter ballistic) matching all other meters (no kind has per-block force-reset post-batch). Net: every per-strip meter on Mixer page + every per-row meter on Builder grid + every EffectRack slot meter goes through the unified G1 pattern (publishPeakReading → VibeGraph member atomic → PluginProcessor mirror → UI poll), ending the bus-vs-insert architectural inconsistency that QA-Eg's bus migration left exposed.

**Effort estimate:** ~12-18 hours (scope-up from original 3-5 hours). Task 2 structural one-shot ~6-9 hr (InsertNode publish rewrite + new VibeGraph arrays + new PluginProcessor mirrors + drainMeterAtomicsForUI extension + consumer rewiring + force-reset deletions + *Run deletion). Tasks 3-7 per-kind verify ~3-5 hr (Jeff drives each verify; my work is `/draft-commit` + `/draft-doc running-notes` + commit per task). Task 8 cleanup + grep sweep ~1 hr. Task 9 close + `/review-batch` ~2-3 hr (the bigger surface raises the review-batch detail count).

**Dependencies:** QA-Eg closed (`888a01b`). The G1 pattern + publishPeakReading + drainAndMerge primitives all landed in QA-Eg; this batch extends them to the insert surface.

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| L1 | **Migration pattern = G1** (publishing into node-internal atomics + exchange/CAS-max into VibeGraph public-member array + drain via unified G1 loop in `drainMeterAtomicsForUI`) | Per §5 entry: "Apply the same G1 pattern QA-Eg landed across the 8 buses". Architectural smell QA-Eg eliminated is identical on the per-row surface. |
| L2 | **Scope = per-row Builder audio meters only** (the `mAudioRowPeakDb*` + `*Run` surface; `kMaxAudioRows = 50` rows) | Per §5 entry. Other meter surfaces (per-insert / per-strip on tabs) are out of scope. |
| L3 | **Surface inventory: 8 publishing sites + 1 promotion loop + 6 `*Run` arrays + 6 initialiser-store lines + 1 comment block** | Source-confirmed pre-batch. Files to modify section lists each. §5 entry called out 1 site (`CompositeAudioInsertTask.cpp:113-115`); my grep surfaced the other 7 — preserves the per-flow + force-reset semantics in the migration. |
| L4 | **Sequencing = immediately after QA-Eg, before QA-InsertMaps** | Per Main Plan §6 arrow + §9 thirty-first Forks entry. Jeff-confirmed slot at QA-Eg close per `feedback_slot_placement_is_spec_call.md`. |
| L5 | **Silly-name = `mellow-bubbling-pancake`** (plan-mode runtime) | My pick per `feedback_silly_name_is_my_pick.md` (not a spec call). |
| L6 | **Task structure = 10-task** (Task 0 open / Task 1 read-only pre-flight inventory / Task 2 structural one-shot / Task 3 Layer verify / Task 4 Bass verify / Task 5 Drum verify / Task 6 Audio verify [Builder + Mixer strip] / Task 7 Aux+Vox+Inst+Rusty bundle verify / Task 8 cleanup + comment sweep / Task 9 close) — **REVISED 2026-05-24 mid-Task-1** from the original 5-task structure after the L7 pivot expanded scope to all 8 InsertKinds. | Jeff 2026-05-24 mid-Task-1 re-spec (L6-revised). Mirrors QA-Eg's per-bus verify rhythm (Tasks 2-6 in QA-Eg verified each bus separately) — gives clean per-kind rollback boundaries on a structural change that touches ~140 insert slots. Task 2 lands the entire structural one-shot in one commit (so the system is internally consistent post-Task-2); Tasks 3-7 are verify-only checkpoints (Jeff drives each verify; commit-per-task gives bisect boundaries on UX regression per kind). |
| L7 | **Per-insert publish architecture = Option 2 restructure** — REVISED 2026-05-24 mid-Task-1 from the original Option B. InsertNode::process rewritten to use the bus-pattern `publishPeakReading` (CAS-max + latency-comp ring) instead of the current load-decay-max-store at `:1231-1242`. The peakDbSnap / peakDbLSnap / peakDbRSnap snapshot-promotion layer is REMOVED entirely from InsertNode struct. At end of `VibeGraph::processInsert`, exchange-store from InsertNode peakDb/L/R into new `VibeGraph::<kind>InsertPeakDb*[index]` public-member arrays (parallel to existing `<bus>PeakDb` atomics). All 8 InsertKinds (Layer / Bass / Drum / Audio / Aux / Vox / Inst / Rusty) adopt this standard. | Jeff 2026-05-24 mid-Task-1 re-spec (L7-revised). The original Option B assumption — "InsertNode publish at `:1241` needs a CAS-max upgrade then single exchange-store at end of CompositeAudioInsertTask::run" — turned out to NOT fit because the existing publish IS already an accumulator (load-decay-max-store, not simple-store) AND because the per-row Builder consumer drains the peakDbSnap layer (NOT peakDb directly), and peakDbSnap has TWO consumers (per-row Builder + per-insert Mixer strip) — exchange-resetting it from one consumer breaks the other. Option 2 restructures the entire insert publish to mirror the bus pattern exactly, ending the bus-vs-insert architectural inconsistency that QA-Eg's bus migration left exposed. Jeff: "Let's embrace the scope and get the architecture right." |
| L8 | **Force-reset path handling = B2** — DELETE all 6 force-reset stores at PluginProcessor.cpp:415/420/448/642/647/668. Per-row Builder audio meters decay naturally over ~20ms (DBFSMeter ballistic) on mute / choke / file-end — same visible behavior as every bus meter + every other InsertKind. | Jeff 2026-05-24 ExitPlanMode (Sub-C). Aligns per-row audio meter behavior to bus meter behavior + every other insert kind's meter behavior (none of the other 7 InsertKinds have force-reset paths — only the audio-row consumer had them, layered onto the *Run mirror). Eliminates the per-row-specific instant-silent-on-mute branch that was a leftover from the G2 mirror era; under L7-revised the entire force-reset machinery is unnecessary because publishPeakReading + drainAndMerge + DBFSMeter handle the decay end-to-end. |
| L9 | **Snapshot mirror location = PluginProcessor parallel mirrors** — add 8 sets of `std::array<std::atomic<float>, max_for_kind>` × 3 axes on PluginProcessor (mirroring the existing `mLayersPeakDb` etc. bus pattern + the existing `mAudioRowPeakDb*` per-row pattern). Naming: `mLayerInsertPeakDb` / `mBassInsertPeakDb` / `mDrumInsertPeakDb` / `mAudioRowPeakDb` (KEPT — existing name preserved for Builder grid backward compat; semantically equivalent to `mAudioInsertPeakDb`) / `mAuxInsertPeakDb` / `mVoxInsertPeakDb` / `mInstInsertPeakDb` / `mRustyInsertPeakDb`. `drainMeterAtomicsForUI` adds 8 per-kind drain loops drainAndMerge'ing `mVibeGraph.<kind>InsertPeakDb*[index]` → `m<Kind>InsertPeakDb*[index]`. UI consumers (Builder grid, Mixer page per-strip meter) poll PluginProcessor mirrors directly. | Jeff 2026-05-24 mid-Task-1 re-spec (L9-new). Matches the bus pattern exactly (every existing bus mirror lives on PluginProcessor as the UI poll target). VibeGraph-as-UI-poll-target would break this consistency (which would create a new bus-vs-insert inconsistency in the opposite direction). The mAudioRowPeakDb name is preserved because (a) it's the Builder grid's natural label and (b) renaming would force a sweep of every Builder consumer for no architectural gain. |

---

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open at ExitPlanMode. All three (Sub-A / Sub-B / Sub-C) locked pre-exit by Jeff and recorded as L6 / L7 / L8 above.

**Post-ExitPlanMode re-spec (2026-05-24 mid-Task-1):** Task 1's read-only inventory surfaced that L7's Option B structural assumption was wrong (see Task 1 running-notes entry for details). Jeff re-spec'd L6 + L7 + added L9; original locks for L1-L5 + L8 stand. Updated table above is the authoritative locked set; all spec calls remain closed.

---

## Files to modify

### Task 1 — Read-only pre-flight inventory
**No edits.** Pure read pass. Inventory output is captured in running notes + surfaced to Jeff before Task 2.

### Task 2 — Structural one-shot (L7-revised Option 2: full InsertNode publish refactor, all 8 InsertKinds)
*Single commit landing the entire architectural restructure. After Task 2 commit, the system is internally consistent — all 8 InsertKinds use the bus-pattern G1 publish chain; the peakDbSnap layer is gone; the *Run mirror is gone; consumers are wired to the new mirrors. Tasks 3-7 verify per-kind.*

**InsertNode struct rewrite ([Source/VibeGraph.cpp:1037-1244](Source/VibeGraph.cpp:1037)):**
- DELETE: `peakDbSnap`, `peakDbLSnap`, `peakDbRSnap` (3 atomics).
- DELETE: `peakDecayDbPerBlock` field + the `kDecayDbPerSec` constant + the prepare-time computation (the load-decay-max-store machinery is replaced by publishPeakReading + DBFSMeter ballistic).
- KEEP: `peakDb`, `peakDbL`, `peakDbR` (now CAS-max written by publishPeakReading).
- KEEP: `peakRingL`, `peakRingR`, `peakRingIdx` (publishPeakReading consumes these).
- REPLACE the inline publish at `:1231-1242` (load-decay-max-store) with a single `publishPeakReading (buf, peakRingL, peakRingR, peakRingIdx, peakDbL, peakDbR, peakDb);` call — mirrors the bus pattern at `:407 / :569 / :727 / :864 / :1012`.

**VibeGraph member additions ([Source/VibeGraph.h:634-672](Source/VibeGraph.h:634)):**
- Add 8 sets × 3 axes = 24 public-member `std::array<std::atomic<float>, max_for_kind>` arrays after the existing bus block:
  ```cpp
  std::array<std::atomic<float>, kMaxLayerInserts>  layerInsertPeakDb,  layerInsertPeakDbL,  layerInsertPeakDbR;
  std::array<std::atomic<float>, kMaxBassInserts>   bassInsertPeakDb,   bassInsertPeakDbL,   bassInsertPeakDbR;
  std::array<std::atomic<float>, kMaxDrumInserts>   drumInsertPeakDb,   drumInsertPeakDbL,   drumInsertPeakDbR;
  std::array<std::atomic<float>, kMaxAudioInserts>  audioInsertPeakDb,  audioInsertPeakDbL,  audioInsertPeakDbR;
  std::array<std::atomic<float>, kMaxAuxInserts>    auxInsertPeakDb,    auxInsertPeakDbL,    auxInsertPeakDbR;
  std::array<std::atomic<float>, kMaxVoxInserts>    voxInsertPeakDb,    voxInsertPeakDbL,    voxInsertPeakDbR;
  std::array<std::atomic<float>, kMaxInstInserts>   instInsertPeakDb,   instInsertPeakDbL,   instInsertPeakDbR;
  std::array<std::atomic<float>, kMaxRustyInserts>  rustyInsertPeakDb,  rustyInsertPeakDbL,  rustyInsertPeakDbR;
  ```
- Introduce `kMax<Kind>Inserts` constants inside `VibeGraph` (50 for Audio matching `VibeSynthProcessor::kMaxAudioRows`; 16 for Layer/Bass/Drum/Aux matching existing slot limits; 6 for Vox/Inst matching `kMaxVoxPages`/`kMaxInstPages`; ~13 for Rusty matching `kMaxRustySlots`). Add a single `.cpp` static_assert tying VibeGraph::kMaxAudioInserts == VibeSynthProcessor::kMaxAudioRows.

**VibeGraph::processInsert exchange-store ([Source/VibeGraph.cpp:processInsert body](Source/VibeGraph.cpp)):**
- After InsertNode::process completes (the chain returns), add an exchange-store from InsertNode peakDb/L/R into the VibeGraph member array indexed by (kind, index). Pattern parallel to bus exchange-stores at [VibeGraph.cpp:1471 / :1503 / :1521 / ...](Source/VibeGraph.cpp:1471):
  ```cpp
  constexpr float kNI = -std::numeric_limits<float>::infinity();
  auto storePeak = [&] (std::atomic<float>& dst, std::atomic<float>& src) noexcept {
      dst.store(src.exchange(kNI, std::memory_order_relaxed), std::memory_order_relaxed);
  };
  // Select per-kind array and write at [index]:
  switch (kind) {
      case InsertKind::Layer:
          storePeak (layerInsertPeakDb [index], node->peakDb);
          storePeak (layerInsertPeakDbL[index], node->peakDbL);
          storePeak (layerInsertPeakDbR[index], node->peakDbR);
          break;
      // ... 7 more cases
  }
  ```

**peakDbSnap layer removal ([Source/VibeGraph.cpp:2470 promoteAllInsertPeakSnapshots](Source/VibeGraph.cpp:2470)):**
- DELETE the insert-promotion half (the `promoteMap` calls at `:2491-2498`). The peakDbSnap atomics no longer exist.
- KEEP the rack-promotion half (the `promoteRack` / `promoteRacksInMap` calls at `:2513-` end of function). EffectRack slot atomics are a SEPARATE surface (effect-panel meters) and still need this promotion.
- RENAME the function to `promoteAllRackSlotSnapshots()` to reflect its slimmer scope. Update the call site at [PluginProcessor.cpp:2115](Source/PluginProcessor.cpp:2115) accordingly.

**Consumer rewiring — drainInsertPeakDbStereo ([Source/VibeGraph.cpp:2437](Source/VibeGraph.cpp:2437)):**
- Rewrite body to read from the new VibeGraph member arrays (the kind+index-indexed atomics). Since this is the per-block UI drain target (called by Mixer-page per-strip meter), it exchange-reads `<kind>InsertPeakDb*[index]` and returns the values. Wait — actually under the new architecture, the UI consumer reads PluginProcessor mirrors directly per L9, NOT VibeGraph member atomics. The `drainInsertPeakDbStereo` function may be DELETED entirely if its only consumer (MixerTrackStrip) is rewired to read PluginProcessor mirrors. Task 1 surfaced that drainInsertPeakDbStereo is the only audio-side path that was draining peakDbSnap — under L7-revised, this function's role disappears.
- **Plan**: DELETE `drainInsertPeakDbStereo` from VibeGraph.h + .cpp. Replace MixerTrackStrip consumer with a new `VibeSynthProcessor::getInsertPeakDbStereo(InsertKind, int)` accessor that reads from `m<Kind>InsertPeakDb*[index]` mirrors.

**PluginProcessor.h additions ([Source/PluginProcessor.h](Source/PluginProcessor.h)):**
- Add 8 sets × 3 axes = 24 snapshot mirror arrays after the existing per-row mirror block at `:620-629`:
  ```cpp
  std::atomic<float> mLayerInsertPeakDb  [kMaxLayerInserts];
  std::atomic<float> mLayerInsertPeakDbL [kMaxLayerInserts];
  std::atomic<float> mLayerInsertPeakDbR [kMaxLayerInserts];
  // ... 7 more kind blocks
  ```
- KEEP the existing `mAudioRowPeakDb / mAudioRowPeakDbL / mAudioRowPeakDbR [kMaxAudioRows]` arrays as the Audio kind's mirror (preserves Builder grid consumer's naming per L9; semantically equivalent to `mAudioInsertPeakDb`).
- DELETE the `*Run` declarations at `:624-629` (the three `mAudioRowPeakDb*Run [kMaxAudioRows]` arrays + the 5-line "2026-05-02: running-max companion" comment block).
- Add public accessor: `std::pair<float, float> getInsertPeakDbStereo (InsertKind, int) const noexcept;` reading from the appropriate `m<Kind>InsertPeakDb*L/R[index]`. Returns `{-60, -60}` for out-of-range or inactive indices.

**PluginProcessor.cpp updates ([Source/PluginProcessor.cpp](Source/PluginProcessor.cpp)):**
- **`:155-163` initialiser loop**: DELETE the three `*Run` initialiser lines (`:161-163`) + the surrounding "running-max variants" comment lines. Add per-kind initialiser loops for the 8 new mirror sets (init to `-60.0f`).
- **`:415 / :420 / :448` force-reset sites inside `renderAudioClipsForRow`**: per L8 / B2, **DELETE all three force-reset stores**.
- **`:583-587` per-flow drain + CAS-max site inside `renderAudioClipsForRow` (Flow B)**: per L7-revised, **DELETE the entire block** (the `drainInsertPeakDbStereo` call + the three `arCasMax` lines + the `arCasMax` lambda definition).
- **`:642 / :647 / :668` force-reset sites inside `renderFilePlayPlayer`**: per L8 / B2, **DELETE all three force-reset stores**.
- **`:2107-2112` G3 promotion loop inside `drainMeterAtomicsForUI`**: REPLACE with 8 per-kind drain loops draining `mVibeGraph.<kind>InsertPeakDb*[index]` → `m<Kind>InsertPeakDb*[index]`. The Audio kind's loop preserves the `mAudioRowPeakDb*[r]` snapshot mirror target.
- **`:2115` `promoteAllInsertPeakSnapshots` call**: RENAME to `promoteAllRackSlotSnapshots()` (matches the function's slimmed scope).

**CompositeAudioInsertTask.cpp ([Source/Engine/Tasks/CompositeAudioInsertTask.cpp:113](Source/Engine/Tasks/CompositeAudioInsertTask.cpp:113)):**
- **`:100-116` Flow A per-flow drain + CAS-max block**: **DELETE this entire block** (the `drainInsertPeakDbStereo` call + the `casMax` lambda + the three casMax calls into `mProcessor->mAudioRowPeakDb*Run`). The exchange-store at end of `processInsert` (added above in VibeGraph.cpp) handles the publish for both Flow A and Flow B.
- No new end-of-task exchange-store needed — `processInsert` (called by both Flow A at `:94` and Flow B inside `renderAudioClipsForRow`) does the publish.

**MixerTrackStrip / Mixer page consumer rewire:**
- Find every `mProcessor->mVibeGraph.drainInsertPeakDbStereo(...)` call site (or equivalent VibeGraph::drainInsertPeakDbStereo direct call); replace with `mProcessor->getInsertPeakDbStereo(kind, index)`. Likely lives in MixerTrackStrip's meter-feed code path.

### Task 3 — Layer kind end-to-end verify
**No source edits.** Verify Layer InsertKind meters: Mixer page per-strip meter on Layer 1/2 tabs reads activity for Harmless / BaySickSynth / BaySickPlayer engines; decays correctly; MT ON + MT OFF parity. `/draft-commit` per task (Task 3 verify commit) + `/draft-doc running-notes`.

### Task 4 — Bass kind end-to-end verify
**No source edits.** Same pattern as Task 3 for Bass InsertKind (Bass 1/2 tabs).

### Task 5 — Drum kind end-to-end verify
**No source edits.** Same pattern for Drum InsertKind (Drums tabs + per-drum strips).

### Task 6 — Audio kind end-to-end verify (Builder grid + Mixer per-strip)
**No source edits.** The original §5 scope: Builder grid per-row meter + Mixer page per-audio-insert strip meter, both consumers verified. Mute / choke / file-end edge cases (decay ~20ms per L8). MT ON + MT OFF parity.

### Task 7 — Aux + Vox + Inst + Rusty bundle end-to-end verify
**No source edits.** Smaller surfaces bundled: Aux strips, Vox/Inst tabs (live input + prerecorded audio sources per `project_vox_inst_accept_prerecorded_audio.md`), Rusty kit. Mixer per-strip meter for each.

### Task 8 — Cleanup + comment sweep + grep cleanliness
- Grep cleanliness: `grep -rn "mAudioRowPeakDbRun\|mAudioRowPeakDbLRun\|mAudioRowPeakDbRRun\|peakDbSnap\|peakDbLSnap\|peakDbRSnap" Source/` — result must be empty post-Task-2.
- Comment sweep: grep for "Group 2" / "Group 3" / "running-max companion" / "deferred to a separate batch per S2" / "peakDbSnap" / "layer-vs-bus ping-pong" / "promoteAllInsertPeakSnapshots" / `getInsertPeakDbStereoExchange` — update or delete remaining references:
  - The stale inline comment at PluginProcessor.cpp:2067 "Group 1: bus mirrors (Layers/Bass/Drums/Master)" — covers 13 buses post-QA-Eg.
  - The publishPeakReading comment at VibeGraph.cpp:113-115 referencing the stale `getInsertPeakDbStereoExchange` name (current name is `drainInsertPeakDbStereo`, deleted entirely under L7-revised).
  - The `drainMeterAtomicsForUI` function-header comment at PluginProcessor.cpp:2038-2055 — update from "three parts" model to reflect that per-row drain is unified with bus drain.
  - The `mAudioRowPeakDb` declaration comment at PluginProcessor.h:617-619 — update to reflect the publishing path now writes into VibeGraph member arrays via processInsert exchange-store (no more *Run).

### Task 9 — Close sequence
- No source edits. `/draft-doc batch-close` + `/review-batch QA-AudioMeters` + apply + `/draft-commit` + close commit.

---

## Tasks

### Task 0 — Open commit
- [ ] Mirror `~/.claude/plans/mellow-bubbling-pancake.md` → `Plans & Specs/Batch Plans/mellow-bubbling-pancake.md` (Write tool); delete the home-dir copy per `feedback_plan_mirror_one_way.md`.
- [ ] Update Main Plan §5 QA-AudioMeters entry header (`Main Plan.md:1165`) — flip `**Plan file:**` line from `` `<silly-name>.md (when started)` `` to backticked path `` `Plans & Specs/Batch Plans/mellow-bubbling-pancake.md` ``. Match the form of sibling §5 entries (e.g. QA-Eg at `:1145`).
- [ ] Seed `Plans & Specs/Running Notes/mellow-bubbling-pancake.md` with the §0-required header (title / purpose blockquote / pair-file ref / convention ref) + initial Task-0 entry. Match the form of `Plans & Specs/Running Notes/squishy-scribbling-flurry.md`.
- [ ] Surface full git status (every dirty + untracked entry) per `feedback_surface_full_git_status_before_commit.md` + dispatch `/draft-commit`. Surface drafted message + git status to Jeff for approval per `feedback_surface_drafted_commit_message_for_approval.md`. Commit on approval.
- [ ] Mark Task 0 done.

### Task 1 — Read-only pre-flight inventory + Jeff surface
- [ ] Read [VibeGraph.cpp:91 publishPeakReading body](Source/VibeGraph.cpp:91) — confirm its publish semantics. Specifically: does it write peakDb via simple-store or CAS-max? The buses publish once per block (no within-block accumulation) so simple-store is sufficient for them; the per-row path under L7 / Sub-B Option B publishes TWICE per task (Flow A then Flow B), so the InsertNode publish site at `:1241` needs CAS-max semantics to accumulate. Task 1 confirms whether the bus pattern already CAS-maxes (so the upgrade is a no-op for the bus side) or whether we need to upgrade ONLY the InsertNode publish site (in which case the change is localized to `:1241`).
- [ ] Re-read [VibeGraph.cpp InsertNode struct definition](Source/VibeGraph.cpp) — confirm exact `peakDb / peakDbL / peakDbR / peakRingL / peakRingR / peakRingIdx` field set. Confirm there's no `peakDecayDbPerBlock`-equivalent dead state (QA-Eg's S6 surprise — the 5 G1 BusNodes had it). If dead state exists, route to Task 3 cleanup (surface to Jeff at this step's findings).
- [ ] Read [VibeGraph.cpp `processInsert`](Source/VibeGraph.cpp) — confirm exactly when/where InsertNode peakDb gets written during the chain. Locates the publishPeakReading call site for InsertNode + verifies the `:1241` `peakDb.store(juce::jmax(newL, newR), ...)` site is the ONLY write site (no hidden secondary publish).
- [ ] Grep for any remaining consumers of `mAudioRowPeakDb*Run` outside the 8 publishing sites + the G3 promotion loop (e.g., direct readers in MixerPage, EffectsPage, tests, etc.). Expected result: zero — the `*Run` mirrors are write-only from the audio thread + read-only by the promotion loop. If any other consumer surfaces, surface to Jeff.
- [ ] Confirm `kMaxAudioRows == 50` is the only definition: `VibeSynthProcessor::kMaxAudioRows = 50` at [PluginProcessor.h:619](Source/PluginProcessor.h:619) + `MixerState::kMaxAudioRows = 50` (referenced in the comment block). Decide: introduce `VibeGraph::kMaxAudioRows = 50` mirror with `static_assert` in a `.cpp` (avoids circular include) vs include PluginProcessor.h from VibeGraph.h (likely creates a circular include since PluginProcessor.h already includes VibeGraph.h).
- [ ] Check for / decide on a `VibeGraph::getInsertNode(InsertKind, int)` public accessor — Task 2's exchange-store needs the InsertNode reference. If no such accessor exists, plan adding a minimal `InsertNode* getInsertNode(InsertKind, int) noexcept;` in `VibeGraph.h` + body in `.cpp`. Confirm it returns non-null only for active insert slots (audio rows are dynamic — created on first drop; un-dropped rows have no InsertNode).
- [ ] Verify the exchange-store sentinel choice: bus migration uses `kBusNegInf = -std::numeric_limits<float>::infinity()` (see [VibeGraph.cpp:1471 etc.](Source/VibeGraph.cpp:1471)). Confirm the same sentinel is appropriate for per-row drain (it should be — `drainAndMerge` max-promotes which handles `-inf` correctly).
- [ ] Verify `drainAndMerge` semantic: read its body in PluginProcessor.cpp. Likely either (a) `snapshot = max(snapshot, source.exchange(-inf))` or (b) `snapshot = source.exchange(-inf)` (write-through). Task 2's exchange-store at end-of-task assumes pattern (a) — confirm. If pattern (b), update Task 2 to handle accordingly.
- [ ] Surface findings to Jeff as a 1-paragraph plain-English inventory per `feedback_design_approval_in_plain_english.md`: (1) whether the InsertNode `:1241` CAS-max upgrade is mechanically safe (confirms L7 / Sub-B Option B); (2) whether a `getInsertNode(InsertKind, int)` accessor needs adding; (3) whether InsertNode has any `peakDecayDbPerBlock`-equivalent dead state to clean up as Task 3 work; (4) whether the 8-site publishing-surface inventory I drafted matches what Task 1 found; (5) any surprise (e.g., a hidden consumer of `mAudioRowPeakDb*Run` outside the expected set; an unexpected `drainAndMerge` semantic).
- [ ] **If Task 1 surfaces a blocker** that invalidates L7 (e.g., the InsertNode `:1241` CAS-max upgrade has a hidden gotcha that breaks bus metering), surface to Jeff for a re-spec call per `feedback_dont_make_unilateral_spec_calls.md`. Don't silently pivot.
- [ ] No code changes this task. Tell Jeff: "Pre-flight inventory complete; surfacing findings now; ready for Task 2 (per-row migration bundle) on your green-light."
- [ ] Wait for Jeff's confirm.
- [ ] Dispatch `/draft-doc running-notes` → apply to running-notes file (Task 1 entry: pre-flight findings + Jeff's green-light + any L7-blocker re-spec).

### Task 2 — Structural one-shot (L7-revised Option 2: full InsertNode publish refactor, all 8 InsertKinds)
*Single commit landing the entire architectural restructure. After Task 2 commit, the system is internally consistent — all 8 InsertKinds use the bus-pattern G1 publish chain; peakDbSnap layer gone; *Run mirror gone; consumers wired to new mirrors. Tasks 3-7 verify per-kind.*

- [ ] **InsertNode struct rewrite ([VibeGraph.cpp:1049-1110](Source/VibeGraph.cpp:1049))** — DELETE `peakDbSnap / peakDbLSnap / peakDbRSnap` fields + `peakDecayDbPerBlock` + `kDecayDbPerSec` constant + the prepare-time decay computation at `:1107-1109`. KEEP `peakDb / peakDbL / peakDbR / peakRingL / peakRingR / peakRingIdx`.
- [ ] **InsertNode::process publish rewrite ([VibeGraph.cpp:1231-1242](Source/VibeGraph.cpp:1231))** — REPLACE the entire load-decay-max-store block with a single call:
  ```cpp
  publishPeakReading (buf, peakRingL, peakRingR, peakRingIdx, peakDbL, peakDbR, peakDb);
  ```
  Mirrors the bus pattern at `:407 / :569 / :727 / :864 / :1012`.
- [ ] **VibeGraph member additions ([VibeGraph.h:634-672](Source/VibeGraph.h:634))** — add 8 sets × 3 axes (`<kind>InsertPeakDb / L / R`) parallel to existing bus block:
  ```cpp
  static constexpr int kMaxLayerInserts = 16;
  static constexpr int kMaxBassInserts  = 16;
  static constexpr int kMaxDrumInserts  = 16;
  static constexpr int kMaxAudioInserts = 50;
  static constexpr int kMaxAuxInserts   = 16;
  static constexpr int kMaxVoxInserts   = 6;   // matches kMaxVoxPages
  static constexpr int kMaxInstInserts  = 6;   // matches kMaxInstPages
  static constexpr int kMaxRustyInserts = 13;  // matches kMaxRustySlots

  std::array<std::atomic<float>, kMaxLayerInserts>  layerInsertPeakDb,  layerInsertPeakDbL,  layerInsertPeakDbR;
  std::array<std::atomic<float>, kMaxBassInserts>   bassInsertPeakDb,   bassInsertPeakDbL,   bassInsertPeakDbR;
  std::array<std::atomic<float>, kMaxDrumInserts>   drumInsertPeakDb,   drumInsertPeakDbL,   drumInsertPeakDbR;
  std::array<std::atomic<float>, kMaxAudioInserts>  audioInsertPeakDb,  audioInsertPeakDbL,  audioInsertPeakDbR;
  std::array<std::atomic<float>, kMaxAuxInserts>    auxInsertPeakDb,    auxInsertPeakDbL,    auxInsertPeakDbR;
  std::array<std::atomic<float>, kMaxVoxInserts>    voxInsertPeakDb,    voxInsertPeakDbL,    voxInsertPeakDbR;
  std::array<std::atomic<float>, kMaxInstInserts>   instInsertPeakDb,   instInsertPeakDbL,   instInsertPeakDbR;
  std::array<std::atomic<float>, kMaxRustyInserts>  rustyInsertPeakDb,  rustyInsertPeakDbL,  rustyInsertPeakDbR;
  ```
  Add `static_assert(VibeGraph::kMaxAudioInserts == VibeSynthProcessor::kMaxAudioRows)` in a `.cpp` that includes both headers.
- [ ] **VibeGraph::prepare()** init: per-element `-60.0f` store loops for all 24 new arrays.
- [ ] **VibeGraph::processInsert exchange-store** — after `InsertNode::process` completes (chain returns), add an exchange-store from InsertNode peakDb/L/R into `<kind>InsertPeakDb*[index]`. Switch on `kind` to select the right array. Pattern parallel to bus exchange-stores at [VibeGraph.cpp:1471 etc.](Source/VibeGraph.cpp:1471):
  ```cpp
  constexpr float kNI = -std::numeric_limits<float>::infinity();
  auto storeAxes = [&] (std::atomic<float>& dM, std::atomic<float>& dL, std::atomic<float>& dR) noexcept {
      dM.store(node->peakDb .exchange(kNI, std::memory_order_relaxed), std::memory_order_relaxed);
      dL.store(node->peakDbL.exchange(kNI, std::memory_order_relaxed), std::memory_order_relaxed);
      dR.store(node->peakDbR.exchange(kNI, std::memory_order_relaxed), std::memory_order_relaxed);
  };
  switch (kind) {
      case InsertKind::Layer: storeAxes(layerInsertPeakDb [index], layerInsertPeakDbL [index], layerInsertPeakDbR [index]); break;
      case InsertKind::Bass:  storeAxes(bassInsertPeakDb  [index], bassInsertPeakDbL  [index], bassInsertPeakDbR  [index]); break;
      // ... 6 more cases
  }
  ```
- [ ] **peakDbSnap layer removal ([VibeGraph.cpp:2470 promoteAllInsertPeakSnapshots](Source/VibeGraph.cpp:2470))** — DELETE the `promoteMap` insert-promotion calls at `:2491-2498`. KEEP the rack-promotion half (`promoteRack` / `promoteRacksInMap` at `:2513-` through end of function). RENAME the function to `promoteAllRackSlotSnapshots()` (matches its slimmer scope). Update the call site at [PluginProcessor.cpp:2115](Source/PluginProcessor.cpp:2115).
- [ ] **VibeGraph::drainInsertPeakDbStereo** ([VibeGraph.cpp:2437](Source/VibeGraph.cpp:2437)) — DELETE the function entirely (along with its declaration in `VibeGraph.h`). Confirmed safe by Task 1 inventory: under L7-revised, the only audio-thread consumer (Flow A/B in CompositeAudioInsertTask + renderAudioClipsForRow) is deleted; the UI consumer (Mixer per-strip meter) rewires to a new PluginProcessor accessor (below).
- [ ] **VibeGraph::getInsertPeakDbStereo** ([VibeGraph.cpp:2411](Source/VibeGraph.cpp:2411)) — non-exchange variant — likely also unused post-refactor. Grep for callers; delete if dead.
- [ ] **PluginProcessor.h additions** — add 8 sets × 3 axes = 24 snapshot mirror arrays parallel to existing `mAudioRowPeakDb*` at `:619-623`:
  ```cpp
  std::atomic<float> mLayerInsertPeakDb  [VibeGraph::kMaxLayerInserts], mLayerInsertPeakDbL [VibeGraph::kMaxLayerInserts], mLayerInsertPeakDbR [VibeGraph::kMaxLayerInserts];
  // ... 7 more kinds (Bass / Drum / Audio uses existing mAudioRowPeakDb / Aux / Vox / Inst / Rusty)
  ```
  KEEP the existing `mAudioRowPeakDb / mAudioRowPeakDbL / mAudioRowPeakDbR [kMaxAudioRows]` arrays as the Audio kind's mirror (preserves Builder grid consumer's naming per L9).
  Add public accessor declaration: `std::pair<float, float> getInsertPeakDbStereo (VibeGraph::InsertKind, int) const noexcept;`.
- [ ] **PluginProcessor.h DELETE** — remove the `*Run` declarations at `:624-629` (the three `mAudioRowPeakDb*Run [kMaxAudioRows]` arrays + the 5-line "2026-05-02: running-max companion" comment block).
- [ ] **PluginProcessor.cpp accessor body** — implement `VibeSynthProcessor::getInsertPeakDbStereo(VibeGraph::InsertKind kind, int index)`: switch on kind, bounds-check index, return `{m<Kind>InsertPeakDbL[index].load(), m<Kind>InsertPeakDbR[index].load()}`. Fallback `{-60.f, -60.f}` for out-of-range.
- [ ] **PluginProcessor.cpp `:155-163` initialiser loop** — DELETE the three `*Run` initialiser lines + surrounding comment. ADD per-kind initialiser loops for the 8 new mirror sets (init to `-60.0f`).
- [ ] **PluginProcessor.cpp `:415 / :420 / :448` force-reset sites** (inside `renderAudioClipsForRow`): per L8 / B2, **DELETE the three force-reset stores entirely**. Surrounding `if`/`continue` logic stays intact.
- [ ] **PluginProcessor.cpp `:583-587` per-flow drain + CAS-max block** (inside `renderAudioClipsForRow`, Flow B): per L7-revised, **DELETE the entire block** — the `drainInsertPeakDbStereo` call at `:583-584`, the three `arCasMax` calls at `:585-587`, AND the `arCasMax` lambda definition. The exchange-store in `processInsert` (added above) handles the publish.
- [ ] **PluginProcessor.cpp `:642 / :647 / :668` force-reset sites** (inside `renderFilePlayPlayer`): per L8 / B2, **DELETE the three force-reset stores entirely**.
- [ ] **PluginProcessor.cpp `:2107-2112` G3 promotion loop** (inside `drainMeterAtomicsForUI`): REPLACE with 8 per-kind drain loops:
  ```cpp
  for (int r = 0; r < VibeGraph::kMaxLayerInserts; ++r) {
      drainAndMerge (mLayerInsertPeakDb [r], mVibeGraph.layerInsertPeakDb [r]);
      drainAndMerge (mLayerInsertPeakDbL[r], mVibeGraph.layerInsertPeakDbL[r]);
      drainAndMerge (mLayerInsertPeakDbR[r], mVibeGraph.layerInsertPeakDbR[r]);
  }
  // ... 6 more kind loops, with the Audio loop draining into mAudioRowPeakDb* (kept name)
  for (int r = 0; r < VibeGraph::kMaxAudioInserts; ++r) {
      drainAndMerge (mAudioRowPeakDb [r], mVibeGraph.audioInsertPeakDb [r]);
      drainAndMerge (mAudioRowPeakDbL[r], mVibeGraph.audioInsertPeakDbL[r]);
      drainAndMerge (mAudioRowPeakDbR[r], mVibeGraph.audioInsertPeakDbR[r]);
  }
  ```
  Update the surrounding comment block (was QA-Eg Task 8 NEEDS-FIX-2 rewrite) to reflect the now-unified G1 drain covering 13 buses + 8 insert kinds.
- [ ] **PluginProcessor.cpp `:2115` `promoteAllInsertPeakSnapshots` call** — RENAME to `promoteAllRackSlotSnapshots()` (matches the function's slimmed scope).
- [ ] **CompositeAudioInsertTask.cpp `:100-116` Flow A per-flow drain + CAS-max block** — per L7-revised, **DELETE the entire block**. No new end-of-task exchange-store needed (the per-call exchange-store in `processInsert` handles it).
- [ ] **MixerTrackStrip consumer rewire** — find every call site that reads insert peaks (likely calls something like `mProcessor->mVibeGraph.drainInsertPeakDbStereo(...)` or the equivalent direct accessor). Replace with `mProcessor->getInsertPeakDbStereo(kind, index)`. Grep for call sites; update each. Possible sites: MixerTrackStrip.cpp / MixerPage.cpp / EffectsPage.cpp.
- [ ] Tell Jeff: "Run `do_build.bat`. Build-only verify this task — Task 2 is the structural one-shot. If the build is clean, proceed to Task 3 (Layer kind verify). If the build fails: `/diagnose-build` + fix in-batch."
- [ ] Wait for Jeff's build result.
- [ ] On clean build: dispatch `/draft-commit`, surface drafted message + full git status, commit on approval.
- [ ] Dispatch `/draft-doc running-notes` → apply.

### Task 3 — Layer kind end-to-end verify
- [ ] Tell Jeff: "Run `do_build.bat` if not already built; then in Debug:
  - **(1)** New project (default Layer 1 tab present). Add Harmless / BaySickSynth / BaySickPlayer engine on Layer 1. Audition or trigger sound (piano roll click). Verify Layer 1 per-strip meter on Mixer page reads activity.
  - **(2)** Add Layer 2 tab; assign different engine; play. Verify both Layer 1 and Layer 2 strips meter independently.
  - **(3)** Stop. Verify both Layer meters decay to silent over ~20ms.
  - **(4)** Multi-core OFF; repeat (1)+(2). Verify Layer meters still read in serial-diagnostic mode.
  - **(5)** Multi-core ON. Save project. Reload. Play. Verify Layer meters still read post-reload."
- [ ] Wait for Jeff's verify result.
- [ ] On pass: `/draft-commit` + surface + commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 4 — Bass kind end-to-end verify
- [ ] Tell Jeff: "Same shape as Task 3 verify but on Bass tabs (Bass 1 / Bass 2 with BaySickBass / Harmless / BaySickPlayer engines). Mixer page per-strip meter, Multi-core ON + OFF, save+reload."
- [ ] Wait for verify; `/draft-commit` on pass; `/draft-doc running-notes` → apply.

### Task 5 — Drum kind end-to-end verify
- [ ] Tell Jeff: "Same shape on Drums tabs (per-drum strips on Mixer page). Verify each drum strip meter reads when its drum hits. Multi-core ON + OFF, save+reload."
- [ ] Wait for verify; `/draft-commit` on pass; `/draft-doc running-notes` → apply.

### Task 6 — Audio kind end-to-end verify (Builder grid + Mixer per-strip)
*The original §5 scope. Both consumer surfaces verified together since they share the Audio kind's mirror.*

- [ ] Tell Jeff: "Run `do_build.bat` if not already built; then in Debug:
  - **(1)** Drop a WAV onto Builder grid row 0. Drop a second WAV onto row 1. Play. Verify Builder-grid per-row meter on both rows reads activity AND Mixer-page Audio insert per-strip meter for those rows reads activity (cross-check both consumers show the same level on the same row).
  - **(2)** Stop. Verify both consumers decay to silent over ~20ms (DBFSMeter ballistic — matches bus and other-kind meters).
  - **(3)** Mute row 0 via the row mute toggle while playing. Verify both consumers (Builder-grid + Mixer-strip) decay to silent over ~20ms. Row 1 still active.
  - **(4)** Trigger a choke-group case (or set up overlapping clips that choke). Verify the choked row decays cleanly on both consumers.
  - **(5)** Audio clip past file end: a clip whose playhead runs past its file length. Verify both consumers decay cleanly when the clip exhausts.
  - **(6)** Multi-core OFF; repeat (1)+(3). Verify both consumers still read correctly in serial-diagnostic mode.
  - **(7)** Multi-core ON. Save project. Reload. Play. Verify post-reload."
- [ ] Wait for Jeff's verify result.
- [ ] On pass: `/draft-commit` + surface + commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 7 — Aux + Vox + Inst + Rusty bundle end-to-end verify
- [ ] Tell Jeff: "Bundle verify on the smaller insert surfaces:
  - **(1) Aux**: Add an Aux strip on Mixer page; route a Layer's sends to it; trigger Layer sound. Verify Aux strip meter reads activity.
  - **(2) Vox** (live + prerecorded per `project_vox_inst_accept_prerecorded_audio.md`): Add Vox tab; arm + speak (live input); verify Vox bus strip meter. Then point Vox at a prerecorded vocal clip; play; verify meter again.
  - **(3) Inst**: Same dual scenario — live input + prerecorded source per the same project memory.
  - **(4) Rusty**: Add a BaySickRustyDrums tab; trigger a drum hit (audition the kit graphic). Verify each Rusty insert strip meter (the kit's 13 drums show as individual strips on Mixer page) reads when its drum hits.
  - **(5)** Multi-core OFF; spot-check each of the 4 kinds quickly.
  - **(6)** Multi-core ON. Project save+reload."
- [ ] Wait for verify result.
- [ ] On pass: `/draft-commit` + surface + commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 8 — Cleanup + comment sweep + grep cleanliness
- [ ] Grep cleanliness: `grep -rn "mAudioRowPeakDbRun\|mAudioRowPeakDbLRun\|mAudioRowPeakDbRRun\|peakDbSnap\|peakDbLSnap\|peakDbRSnap\|drainInsertPeakDbStereo\|promoteAllInsertPeakSnapshots\|getInsertPeakDbStereoExchange" Source/` — result must be empty (or limited to expected post-rename hits).
- [ ] Comment sweep: grep for "Group 2" / "Group 3" / "running-max companion" / "deferred to a separate batch per S2" / "peakDbSnap" / "layer-vs-bus ping-pong" — update or delete:
  - The stale inline comment at PluginProcessor.cpp:2067 "Group 1: bus mirrors (Layers/Bass/Drums/Master)" — covers 13 buses post-QA-Eg + 8 insert kinds post-this-batch.
  - The publishPeakReading comment at VibeGraph.cpp:112-115 referencing stale `getInsertPeakDbStereoExchange` name + the obsolete "buses vs inserts have different drain paths" model.
  - The `drainMeterAtomicsForUI` function-header comment at PluginProcessor.cpp:2038-2055 — update from "three parts" model to reflect that bus + insert + rack-slot all use unified G1 chain.
  - The `mAudioRowPeakDb` declaration comment at PluginProcessor.h:617-619 — update to reflect publishing now flows through VibeGraph member arrays (no *Run intermediate).
- [ ] Tell Jeff: "Run `do_build.bat`. Then in Debug + Release:
  - **(1)** Full end-to-end stress: project with audio on every kind (Layers + Bass + Drums + Audio rows + Aux + Vox + Inst + Rusty + every bus). Multi-core ON. Verify every meter on every surface (Builder grid + Mixer page per-strip + per-bus) reads correctly with no glitch / drop / lag.
  - **(2)** Multi-core OFF. Same arrangement; confirm serial-diagnostic mode reads correctly.
  - **(3)** Project save+reload + post-reload spot-check.
  - **(4)** Bus meter regression: every bus meter (13 G1 buses landed by QA-Eg) still reads identically to pre-batch behavior — the InsertNode publishPeakReading rewrite is the cross-surface touch point worth eyeballing here too."
- [ ] Wait for verify result.
- [ ] On pass: `/draft-commit` + surface + commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 9 — Close sequence
- [ ] Dispatch `/draft-doc batch-close` with a synthesis of the running-notes file.
- [ ] Apply the close entry to `Plans & Specs/Implemented Work Log.md` via Edit per `feedback_targeted_edits_not_wholesale_rewrite.md`.
- [ ] Dispatch `/review-batch QA-AudioMeters`.
- [ ] Address BLOCKERs / NEEDS-FIX in-batch per `feedback_closed_batch_carryforward_via_forks.md`. Defer NITs into close-entry routing table.
- [ ] Route side findings per Rule 3:
  - Resolved in-batch → close entry's routing table.
  - Outside-batch → §9 Forks entry in Main Plan + corresponding §5 / §6 edits. Slot SURFACED to Jeff per `feedback_slot_placement_is_spec_call.md`; don't pick.
- [ ] Surface full git status (every dirty + untracked entry) per `feedback_surface_full_git_status_before_commit.md`.
- [ ] Dispatch `/draft-commit` for close commit. Surface message + status. Commit on approval.

---

## Verification (end-to-end smoke)

After Task 8 commit lands (cleanup + comment sweep + grep cleanliness) and before the close commit (Task 9):

1. **Build clean.** `do_build.bat` Release + Debug both green; no new warnings.
2. **Per-kind meter sanity (MT ON)** — verified per Task 3-7 already; final smoke is a single arrangement with audio on every kind (Layer / Bass / Drum / Audio rows / Aux / Vox / Inst / Rusty + every bus) and every per-strip + per-row meter on Mixer page + Builder grid reads correctly.
3. **Per-kind meter sanity (MT OFF / serial-diagnostic)** — same arrangement, Multi-core OFF; every meter still reads correctly.
4. **20ms-decay coverage** (L8 / B2 alignment to bus behavior — applies to all 8 kinds, not just Audio):
   - **Muted by choke / row mute / file-end**: Audio row meters decay over ~20ms (DBFSMeter ballistic).
   - **Mute toggle on any per-strip meter** (any kind): decays identically to bus mute behavior.
   - **Subjective comparison**: hit mute on a bus strip vs hit mute on a Layer/Bass/Drum/Audio/etc strip — decay rate visually identical.
5. **Project save/reload** — save the stress arrangement; close project; reopen; every meter reads correctly post-reload.
6. **Grep cleanliness** — `grep -rn "mAudioRowPeakDbRun\|peakDbSnap\|drainInsertPeakDbStereo\|promoteAllInsertPeakSnapshots\|getInsertPeakDbStereoExchange" Source/` shows zero hits (or limited to renamed `promoteAllRackSlotSnapshots` only).
7. **Bus regression check** — every bus meter (the 13 G1 buses from QA-Eg) reads identically to pre-batch behavior. The InsertNode publishPeakReading rewrite is the cross-surface change to eyeball.
8. **EffectRack slot meter regression check** — open an effect panel on any kind's rack (Layer / Bass / Drum / etc.); verify the slot meter still works post-`promoteAllRackSlotSnapshots` rename. The rack-promotion half stayed; this is a sanity check.

---

## MT-awareness static-analysis

The batch is meter / UI-state only. No audio-thread arithmetic changes. Under L7-revised Option 2, the migration adopts the exact same threading shape as the QA-Eg bus migration, applied uniformly across all 8 InsertKinds:

1. **Publish (per insert per `processInsert` call)** — `publishPeakReading` writes to `peakDb*` (running-max atomics, CAS-max + latency-comp ring). Runs on whichever thread executes the insert's PassiveStripTask (worker under MT, audio under 1-worker serial). Identical to bus publish today.
2. **Exchange-store (end of `processInsert`)** — moves InsertNode peakDb*/L/R into VibeGraph's `<kind>InsertPeakDb*[index]` member atomic. Single exchange per axis. Identical primitive to bus exchange-stores at [VibeGraph.cpp:1471 etc.](Source/VibeGraph.cpp:1471).
3. **Drain (end-of-block, audio thread)** — `drainMeterAtomicsForUI`'s 8 new per-kind loops drainAndMerge VibeGraph member atomic → PluginProcessor mirror. Identical to bus drain loops at `:2068-2103`.
4. **UI poll** — `getInsertPeakDbStereo(kind, index)` accessor on PluginProcessor reads from `m<Kind>InsertPeakDb*L/R[index]`. Plain atomic load; no exchange (UI doesn't reset).

Same primitives (`std::atomic<float>` + `exchange` + `compare_exchange_weak`), same memory ordering (`memory_order_relaxed`), same threading shape. `processInsert` runs on worker threads under MT and on the audio thread under 1-worker serial-diagnostic — every existing bus migration already proved this dual-mode pattern. No new races introduced.

**The peakDbSnap layer removal** is safe because: (a) the snapshot-promotion at end-of-block was the OLD mechanism for atomicity across consumer reads; under the new pattern, the audio-thread drain happens once per block in `drainMeterAtomicsForUI` (same single boundary point as buses use), and UI consumers poll the resulting mirror at their own cadence — no mid-block partial-write race because there's no consumer reading mid-block atomics; (b) the bus pattern has been MT-validated against the same property for the 13 buses already on G1 — no per-bus "ping-pong" reported post-QA-Eg.

**The promoteAllInsertPeakSnapshots → promoteAllRackSlotSnapshots split** is safe because: (a) the insert-promotion half is removed entirely (peakDbSnap is gone), so the function no longer touches insert atomics; (b) the rack-promotion half (EffectRack slot atomics for effect-panel meters) is preserved as-is — same call site at the end of `drainMeterAtomicsForUI`, same surface, just a slimmer function body and a clearer name.

---

## Routing notes (Rule 3 application during execution)

- Findings about meter behavior on **per-insert surfaces** (the per-strip Mixer-page meters that are now restructured by this batch) → in-batch per `feedback_qa_batches_fix_bugs_dont_defer.md`. Tasks 3-7's per-kind verify is where regressions surface.
- Findings about **EffectRack slot meters** (the surviving half of `promoteAllRackSlotSnapshots`) → in-batch if regression is caused by the rename / split; outside-batch if it's an unrelated finding (effect-panel UX is separate scope).
- Findings about **publishPeakReading semantics** that don't match the L7-revised assumption (e.g., a hidden side-effect when the call frequency increases — `processInsert` is called more often than the bus equivalents) → in-batch per `feedback_qa_batches_fix_bugs_dont_defer.md`; surface to Jeff as a structural re-spec if needed.
- Findings about **MT-vs-serial parity** (Multi-core OFF doesn't produce the same meter behavior as Multi-core ON) → in-batch; same diagnostic instinct as QA-Eg.
- Findings about **dead state** on InsertNode after the L7-revised cleanup (any field that's left orphaned after `peakDecayDbPerBlock` + `peakDbSnap*` are deleted, e.g., the `kDecayDbPerSec` constant in `InsertNode::prepare`) → fix in Task 8 cleanup; surface for inclusion in the close routing.
- Findings about **hot-path performance** (the structural change adds an exchange-store per `processInsert` call — ~140 inserts × ~86 blocks/sec = ~12k exchange-stores/sec on the audio thread) → route to `/perf-audit` for follow-up; minor regression is acceptable (relaxed-order exchange is sub-ns; the bus migration showed no measurable delta).
- Findings about **other QA-Eg-adjacent architectural smells** (any other `*Run` or `*Snap` intermediate mirror anywhere else not previously surfaced) → log + surface to Jeff; route per Rule 3 at close.
- Findings about **Builder grid consumer** (the `mAudioRowPeakDb*` name preservation per L9) — if the Builder grid's reader code path needs to change for a non-naming reason, fix in-batch; if it's just naming, leave the existing name and document in close.

---

## Carry-Forward Reference touch points

- **§1 (MT Render Path Primitives)** — read at Task 1 start; confirms the file:line index for the per-row publishing surface (note: §1 was frozen 2026-05-07; the per-row mirror was added 2026-05-02 so it predates the freeze; the field:line citations should still hold but verify).
- **§2 (Mixer / Routing architecture)** — read at Task 1 start; confirms the `kMaxAudioRows = 50` row model + `MixerChannelIds 400..449` audio-row ID block + `InsertKind::Audio` mapping.
- **§4 (Lock-free primitives)** — read at Task 1 start; confirms `publishPeakReading` + ring-buffer + relaxed-store pattern is the locked architectural primitive — and that the migration adopts the same primitive as the buses already do.

---

## Carry-Over

(populated mid-batch on session pause per §0 Rule 2; empty at batch open)
