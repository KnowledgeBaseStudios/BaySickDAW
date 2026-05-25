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

**Risk:** **low-medium** — meter / UI-state only; audio path arithmetic unaffected. Same migration pattern as QA-Eg (well-established by 8 buses migrated one at a time across Tasks 2-6). Three structural twists vs the bus migration: (i) per-row indexing means VibeGraph members are arrays not scalars; (ii) under L7 / Sub-B Option B, the InsertNode `:1241` publish site needs a one-line CAS-max upgrade — this site is shared across ALL `InsertKind`s (Layer / Bass / Drum / Audio / Aux / Vox / Inst / Rusty), so Task 1 must confirm the upgrade is mechanically safe for every kind, not just `Audio`; (iii) under L8 / Sub-C B2, deleting the 6 force-reset stores changes the per-row meter's mute decay from 1-block-snap to 20ms-ballistic — visible behavioral change that Jeff approved as desirable (aligns to bus behavior).

**Effort estimate:** ~3-5 hours (§5 entry's estimate; matches my source-read).

**Dependencies:** QA-Eg closed (`888a01b`). The migration pattern + cleanup of the central mirror infrastructure landed in QA-Eg; this batch consumes the same pattern.

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| L1 | **Migration pattern = G1** (publishing into node-internal atomics + exchange/CAS-max into VibeGraph public-member array + drain via unified G1 loop in `drainMeterAtomicsForUI`) | Per §5 entry: "Apply the same G1 pattern QA-Eg landed across the 8 buses". Architectural smell QA-Eg eliminated is identical on the per-row surface. |
| L2 | **Scope = per-row Builder audio meters only** (the `mAudioRowPeakDb*` + `*Run` surface; `kMaxAudioRows = 50` rows) | Per §5 entry. Other meter surfaces (per-insert / per-strip on tabs) are out of scope. |
| L3 | **Surface inventory: 8 publishing sites + 1 promotion loop + 6 `*Run` arrays + 6 initialiser-store lines + 1 comment block** | Source-confirmed pre-batch. Files to modify section lists each. §5 entry called out 1 site (`CompositeAudioInsertTask.cpp:113-115`); my grep surfaced the other 7 — preserves the per-flow + force-reset semantics in the migration. |
| L4 | **Sequencing = immediately after QA-Eg, before QA-InsertMaps** | Per Main Plan §6 arrow + §9 thirty-first Forks entry. Jeff-confirmed slot at QA-Eg close per `feedback_slot_placement_is_spec_call.md`. |
| L5 | **Silly-name = `mellow-bubbling-pancake`** (plan-mode runtime) | My pick per `feedback_silly_name_is_my_pick.md` (not a spec call). |
| L6 | **Task structure = 5-task** (Task 0 open / Task 1 read-only pre-flight inventory / Task 2 per-row migration bundle / Task 3 cleanup + comment sweep + grep cleanliness / Task 4 close) | Jeff 2026-05-24 ExitPlanMode (Sub-A). QA-Eg's Task 1 caught the S6 dead-field surprise before code landed; this batch has equivalent inventory questions (InsertNode publish-site CAS-max upgrade feasibility under L7, `drainAndMerge` semantic, `peakDecayDbPerBlock`-equivalent dead state on InsertNode). 30 min of read-time vs the cost of mid-batch surprise rework. |
| L7 | **Per-row accumulator semantics = Option B** — single exchange-store at end of `CompositeAudioInsertTask::run`; both flows publish into InsertNode peakDb via `processInsert`; per-flow drains at CompositeAudioInsertTask:113-115 + PluginProcessor:585-587 are REMOVED. Requires upgrading the InsertNode publish site at [VibeGraph.cpp:1241](Source/VibeGraph.cpp:1241) from `peakDb.store(juce::jmax(newL, newR), ...)` to a CAS-max so consecutive publishes within one task accumulate correctly. | Jeff 2026-05-24 ExitPlanMode (Sub-B). Cleaner architectural match to the bus migration; eliminates the per-flow drain pattern entirely; ends QA-Eg's bus-vs-row architectural inconsistency. Task 1 confirms the InsertNode publish-site CAS-max upgrade is mechanically safe before code lands. |
| L8 | **Force-reset path handling = B2** — DELETE all 6 force-reset stores at PluginProcessor.cpp:415/420/448/642/647/668. Per-row meters decay naturally over ~20ms (DBFSMeter ballistic) on mute / choke / file-end — same visible behavior as every bus meter. | Jeff 2026-05-24 ExitPlanMode (Sub-C). Aligns per-row meter behavior to bus meter behavior (which DBFSMeter ballistic decay already covers). Eliminates the per-row-specific instant-silent-on-mute branch that was a leftover from the G2 mirror era; the 20ms decay is what every bus meter already does and what the user is already familiar with. |

---

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open at ExitPlanMode. All three (Sub-A / Sub-B / Sub-C) locked pre-exit by Jeff and recorded as L6 / L7 / L8 above.

---

## Files to modify

### Task 1 — Read-only pre-flight inventory
**No edits.** Pure read pass. Inventory output is captured in running notes + surfaced to Jeff before Task 2.

### Task 2 — Per-row migration (single source-touching bundle, Sub-B Option B + Sub-C B2)
- [Source/VibeGraph.h](Source/VibeGraph.h) — add three `std::array<std::atomic<float>, kMaxAudioRows>` public-member arrays parallel to the existing per-bus atomics block at `:634-672`. Naming: `audioRowPeakDb / audioRowPeakDbL / audioRowPeakDbR`. Sizing: introduce a `static constexpr int VibeGraph::kMaxAudioRows = 50;` (Task 1 confirms whether to mirror the value or include `VibeSynthProcessor::kMaxAudioRows` — circular-include risk is real); add a single `static_assert(VibeGraph::kMaxAudioRows == VibeSynthProcessor::kMaxAudioRows)` in a `.cpp` that includes both headers.
- [Source/VibeGraph.cpp](Source/VibeGraph.cpp):
  - **`:1241` InsertNode peakDb publish site** (per L7 / Sub-B Option B): upgrade from `peakDb.store(juce::jmax(newL, newR), std::memory_order_relaxed)` to a CAS-max so consecutive `processInsert` calls within one `CompositeAudioInsertTask::run` accumulate correctly. Pattern:
    ```cpp
    const float newPeak = juce::jmax(newL, newR);
    float cur = peakDb.load(std::memory_order_relaxed);
    while (cur < newPeak
           && ! peakDb.compare_exchange_weak(cur, newPeak, std::memory_order_relaxed)) {}
    ```
    Task 1 confirms this is the only InsertNode peakDb write site + verifies the bus nodes' equivalent publishes already CAS-max (or whether the buses get away with simple-store because they only publish once per block).
  - **VibeGraph::prepare()** init loop: add per-element `-60.0f` init for the three new `audioRowPeakDb*` arrays. Pattern parallel to how bus public-member atomics get their init (Task 1 confirms whether the bus atomics rely on `{ -60.f }` aggregate-init or a `prepare`-time loop).
- [Source/PluginProcessor.h](Source/PluginProcessor.h) — DELETE the `*Run` declarations at `:624-629` (the three `mAudioRowPeakDb*Run [kMaxAudioRows]` arrays + the 5-line "2026-05-02: running-max companion" comment block). KEEP the snapshot mirrors at `:619-623` (UI poll target). `kMaxAudioRows` constant at `:619` stays.
- [Source/PluginProcessor.cpp](Source/PluginProcessor.cpp):
  - **`:155-163` initialiser loop**: DELETE the three `*Run` initialiser-store lines (`:161-163`) + the surrounding "running-max variants" comment lines; KEEP the snapshot mirror initialisers (`:155-157`).
  - **`:415 / :420 / :448` force-reset sites inside `renderAudioClipsForRow`**: per L8 / Sub-C B2, **DELETE these three force-reset stores entirely** (each is `mAudioRowPeakDbRun[row].store (-60.0f, ...)` on a single line; surrounding `if`/`continue` logic stays). 3 lines deleted.
  - **`:585-587` per-flow drain + CAS-max site inside `renderAudioClipsForRow` (Flow B)**: per L7 / Sub-B Option B, **DELETE the three `arCasMax` lines + the `drainInsertPeakDbStereo` call at `:583-584` that feeds them**. The `pkL` / `pkR` locals + the `arCasMax` lambda definition above this block also become dead — delete the lambda definition too. ~7 lines deleted.
  - **`:642 / :647 / :668` force-reset sites inside `renderFilePlayPlayer`**: per L8 / Sub-C B2, **DELETE these three force-reset stores entirely** (same pattern as the renderAudioClipsForRow force-resets — single-line stores; surrounding logic stays). 3 lines deleted.
  - **`:2107-2112` G3 promotion loop inside `drainMeterAtomicsForUI`**: change the per-row drain to source from the VibeGraph arrays:
    ```cpp
    // Before:
    drainAndMerge (mAudioRowPeakDb [r], mAudioRowPeakDbRun [r]);
    // After:
    drainAndMerge (mAudioRowPeakDb [r], mVibeGraph.audioRowPeakDb [r]);
    ```
    The loop body becomes a structural G1 drain (matches the bus G1 loop at `:2096-2108` post-QA-Eg). Update the surrounding comment block to drop the "per-row deferred to a separate batch per S2" note and reflect the unified G1 drain that now covers per-row alongside the 13 buses.
- [Source/Engine/Tasks/CompositeAudioInsertTask.cpp](Source/Engine/Tasks/CompositeAudioInsertTask.cpp:113):
  - **`:100-116` Flow A per-flow drain + CAS-max block**: per L7 / Sub-B Option B, **DELETE this entire block** (the `if (mIndex >= 0 && mIndex < VibeSynthProcessor::kMaxAudioRows)` guard + the `drainInsertPeakDbStereo` call + the `casMax` lambda definition + the three `casMax` calls into `mProcessor->mAudioRowPeakDb*Run`). ~17 lines deleted.
  - **End-of-`run` body** (after Flow B's `renderAudioClipsForRow` call at `:154`): add a single exchange-store from InsertNode peakDb into the VibeGraph public-member array. Pattern (parallel to the bus exchange-stores at [VibeGraph.cpp:1471 / :1503 / :1521 / ...](Source/VibeGraph.cpp:1471)):
    ```cpp
    if (mIndex >= 0 && mIndex < VibeGraph::kMaxAudioRows)
    {
        // Drain InsertNode peakDb into VibeGraph public-member array for the UI drain loop.
        if (auto* node = mGraph->getInsertNode(VibeGraph::InsertKind::Audio, mIndex))
        {
            constexpr float kNI = -std::numeric_limits<float>::infinity();
            mGraph->audioRowPeakDb [mIndex].store(node->peakDb .exchange(kNI, std::memory_order_relaxed), std::memory_order_relaxed);
            mGraph->audioRowPeakDbL[mIndex].store(node->peakDbL.exchange(kNI, std::memory_order_relaxed), std::memory_order_relaxed);
            mGraph->audioRowPeakDbR[mIndex].store(node->peakDbR.exchange(kNI, std::memory_order_relaxed), std::memory_order_relaxed);
        }
    }
    ```
    Task 1 confirms whether VibeGraph exposes a `getInsertNode(InsertKind, index)` accessor returning the InsertNode reference (or whether the existing `drainInsertPeakDbStereo` is the only accessor and we need a new public getter). If accessor doesn't exist, add a minimal `InsertNode* getInsertNode(InsertKind, int)` public method in `VibeGraph.h`.

### Task 3 — Cleanup + comment sweep + grep cleanliness
- Comment sweep: grep for `*Run` / "running-max" / "2026-05-02" / "Group 3" referencing the deleted per-row mirror surface. Update or delete:
  - The "Group 3" comment block above `drainMeterAtomicsForUI`'s former G3 loop (at the line range just above `:2107` post-Task-2; was the QA-Eg Task 8 NEEDS-FIX-2 rewrite). Update to reflect that the unified G1 drain now covers per-row alongside the 13 buses.
  - The `Source/PluginProcessor.h:617-619` comment block "Per-row peak dB for audio strip meters..." stays correct in spirit but should reflect that the publishing path now writes into VibeGraph atomics (the snapshot mirror is the UI poll target only).
- Grep cleanliness check: `grep -rn "mAudioRowPeakDbRun\|mAudioRowPeakDbLRun\|mAudioRowPeakDbRRun" Source/` — result must be empty post-Task-2.
- Cross-check that no other reader of `mAudioRowPeakDb*Run` exists outside the migrated sites (Task 1 inventory output confirms this).

### Task 4 — Close sequence
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

### Task 2 — Per-row migration (single source-touching bundle, L7 Option B + L8 B2)
*Single bundle: all 50 rows behave identically — no per-row variance to split into multiple tasks (unlike QA-Eg's per-bus per-task structure).*

- [ ] **VibeGraph.h additions**: add `static constexpr int kMaxAudioRows = 50;` inside the `VibeGraph` class. Add the three public-member arrays parallel to the existing bus block at `:634-672`:
  ```cpp
  std::array<std::atomic<float>, kMaxAudioRows> audioRowPeakDb  {};
  std::array<std::atomic<float>, kMaxAudioRows> audioRowPeakDbL {};
  std::array<std::atomic<float>, kMaxAudioRows> audioRowPeakDbR {};
  ```
- [ ] **VibeGraph.cpp `:1241` InsertNode peakDb publish site** (L7 / Option B's structural change — confirms Task 1 finding): upgrade simple-store to CAS-max so consecutive `processInsert` calls within one task accumulate:
  ```cpp
  // Before (line 1241):
  peakDb.store(juce::jmax(newL, newR), std::memory_order_relaxed);
  // After:
  const float newPeak = juce::jmax(newL, newR);
  float cur = peakDb.load(std::memory_order_relaxed);
  while (cur < newPeak
         && ! peakDb.compare_exchange_weak(cur, newPeak, std::memory_order_relaxed)) {}
  ```
- [ ] **VibeGraph: getInsertNode accessor** (if Task 1 confirmed it doesn't exist): add `InsertNode* getInsertNode(InsertKind, int) noexcept;` to `VibeGraph.h` public API + minimal body in `.cpp` that returns the pointer from the appropriate map (or nullptr if no such insert).
- [ ] **VibeGraph: `static_assert` tying `VibeGraph::kMaxAudioRows == VibeSynthProcessor::kMaxAudioRows`** — place in a `.cpp` that includes both headers (e.g., `VibeGraph.cpp` if it already includes `PluginProcessor.h` for processor access; otherwise add the include).
- [ ] **VibeGraph::prepare()** init: add a per-element `-60.0f` store loop for the three new arrays (Task 1 confirms whether bus atomics rely on aggregate-init or a prepare-time loop; match that pattern).
- [ ] **PluginProcessor.h**: DELETE the `*Run` declarations at `:624-629` (the three `mAudioRowPeakDb*Run [kMaxAudioRows]` arrays + the 5-line "2026-05-02: running-max companion" comment block). KEEP the snapshot mirrors at `:619-623`.
- [ ] **PluginProcessor.cpp `:155-163` initialiser loop**: DELETE the three `*Run` initialiser-store lines at `:161-163` + the surrounding "running-max variants" comment lines. KEEP the snapshot mirror initialisers at `:155-157`.
- [ ] **PluginProcessor.cpp `:415 / :420 / :448` force-reset sites** (inside `renderAudioClipsForRow`): per L8 / B2, **DELETE these three force-reset stores entirely**. Each is a single `mAudioRowPeakDbRun[row].store(-60.0f, ...)` line; the surrounding `if`/`continue` early-return logic stays intact. Per-row meter decays naturally over ~20ms via DBFSMeter ballistic (matching bus behavior).
- [ ] **PluginProcessor.cpp `:583-587` per-flow drain + CAS-max block** (inside `renderAudioClipsForRow`, Flow B): per L7 / Option B, **DELETE this entire block**. Specifically: delete the `const auto [pkL, pkR] = mVibeGraph.drainInsertPeakDbStereo(...)` call at `:583-584`; delete the three `arCasMax (mAudioRowPeakDb*Run[row], ...)` calls at `:585-587`; delete the `arCasMax` lambda definition that feeds them (if it lives nearby — Task 1 confirms its scope). The InsertNode peakDb now stays populated until end of `CompositeAudioInsertTask::run`'s single exchange-store.
- [ ] **PluginProcessor.cpp `:642 / :647 / :668` force-reset sites** (inside `renderFilePlayPlayer`): per L8 / B2, **DELETE these three force-reset stores entirely**. Same pattern as `:415 / :420 / :448` — single-line stores; surrounding logic stays.
- [ ] **PluginProcessor.cpp `:2107-2112` G3 promotion loop** (inside `drainMeterAtomicsForUI`): rewrite to drain from VibeGraph public-member arrays:
  ```cpp
  // After:
  for (int r = 0; r < kMaxAudioRows; ++r)
  {
      drainAndMerge (mAudioRowPeakDb [r], mVibeGraph.audioRowPeakDb [r]);
      drainAndMerge (mAudioRowPeakDbL[r], mVibeGraph.audioRowPeakDbL[r]);
      drainAndMerge (mAudioRowPeakDbR[r], mVibeGraph.audioRowPeakDbR[r]);
  }
  ```
  Update the surrounding comment block to drop the "per-row deferred to a separate batch per S2" note + reflect the unified G1 drain that now covers per-row alongside the 13 buses.
- [ ] **CompositeAudioInsertTask.cpp `:100-116` Flow A per-flow drain + CAS-max block**: per L7 / Option B, **DELETE the entire block** (the `if (mIndex >= 0 && mIndex < VibeSynthProcessor::kMaxAudioRows)` guard, the `drainInsertPeakDbStereo` call, the `casMax` lambda definition, and the three `casMax` calls into `mProcessor->mAudioRowPeakDb*Run`). ~17 lines deleted.
- [ ] **CompositeAudioInsertTask.cpp `run()` end-of-body** (after Flow B's `renderAudioClipsForRow` call at `:154`): add a single exchange-store from InsertNode peakDb into the VibeGraph public-member array. Pattern parallel to bus exchange-stores at [VibeGraph.cpp:1471 / :1503 / :1521 / ...](Source/VibeGraph.cpp:1471):
  ```cpp
  if (mIndex >= 0 && mIndex < VibeGraph::kMaxAudioRows)
  {
      if (auto* node = mGraph->getInsertNode(VibeGraph::InsertKind::Audio, mIndex))
      {
          constexpr float kNI = -std::numeric_limits<float>::infinity();
          mGraph->audioRowPeakDb [mIndex].store(node->peakDb .exchange(kNI, std::memory_order_relaxed), std::memory_order_relaxed);
          mGraph->audioRowPeakDbL[mIndex].store(node->peakDbL.exchange(kNI, std::memory_order_relaxed), std::memory_order_relaxed);
          mGraph->audioRowPeakDbR[mIndex].store(node->peakDbR.exchange(kNI, std::memory_order_relaxed), std::memory_order_relaxed);
      }
  }
  ```
  (Task 1's `getInsertNode` decision determines whether this is `mGraph->getInsertNode(...)` or a direct lookup via existing API.)
- [ ] Tell Jeff: "Run `do_build.bat`. Then in Debug:
  - **(1)** New project. Drop a WAV onto the Builder grid (creates an audio clip row 0). Drop a second WAV onto a different row (row 1). Play the arrangement. Verify the **per-row meters on the Builder grid** read activity on both rows (compare to the Clips Bus meter on Mixer page for sanity).
  - **(2)** Stop. Verify both per-row meters decay to silent over ~20ms (DBFSMeter ballistic — same as bus meter decay).
  - **(3)** Mute row 0 via the row mute toggle while playing. Verify row 0's meter decays over ~20ms (no longer instant-silent under L8 / B2; matches bus behavior). Row 1's meter still reads activity.
  - **(4)** Set up a choke-group case (two rows with overlapping triggers; one chokes the other). Trigger the choke. Verify the choked row's meter decays over ~20ms (no longer instant-silent).
  - **(5)** Trigger a file-position-out-of-range case (an audio clip whose playhead runs past its file length). Verify the row meter decays over ~20ms when the clip ends.
  - **(6)** Switch Multi-core Rendering OFF (Mixer hamburger → 'Multi-core Rendering' toggle). Repeat (1) + (3). Verify per-row meters still read correctly + decay correctly in serial-diagnostic mode.
  - **(7)** Switch Multi-core Rendering ON. Save the project. Reload. Play. Verify per-row meters still read correctly post-reload.
  - **(8)** Regression check: confirm every bus meter (Layers / Bass / Drums / Master / FX / AudioClips / Vox / Inst / Rusty / Vox2 / Inst2 / Inst3) still reads correctly post-migration. The InsertNode `:1241` CAS-max upgrade touches the per-insert publish surface — bus paths don't use it directly, but the sanity-eyeball matters."
- [ ] Wait for Jeff's verify result.
- [ ] If verify passes: dispatch `/draft-commit`, surface drafted message + full git status, commit on approval.
- [ ] Dispatch `/draft-doc running-notes` → apply to running-notes file.

### Task 3 — Cleanup + comment sweep + grep cleanliness
- [ ] Grep cleanliness check: `grep -rn "mAudioRowPeakDbRun\|mAudioRowPeakDbLRun\|mAudioRowPeakDbRRun" Source/` — result must be empty post-Task-2. If any site remains, fix in this task before the close.
- [ ] Comment sweep: grep for `Group 3` / `running-max companion` / `2026-05-02` (the per-row `*Run`-introduction date) / "deferred to a separate batch per S2" — update or delete remaining references:
  - The "Group 3" header comment above `drainMeterAtomicsForUI`'s former G3 loop (location TBD post-Task-2 — likely just above the rewritten per-row loop). Update to reflect that per-row drain is now part of the unified G1 path (post-QA-Eg + QA-AudioMeters, all 13 buses + the per-row surface drain through a single G1-style loop).
  - The QA-Eg Task 8 NEEDS-FIX-2 sweep rewrote the `drainMeterAtomicsForUI` function-header comment to describe "the unified G1 drain that all 13 buses now share" — verify this comment is updated to include the per-row surface too post-Task-2.
  - The `PluginProcessor.h:617-623` comment block "Per-row peak dB for audio strip meters (audio thread writes, UI timer reads)" should stay accurate — verify nothing in the snapshot mirror's role changed (it's still the UI poll target).
- [ ] InsertNode `peakDecayDbPerBlock`-equivalent dead-state check (if Task 1 inventory surfaced one): delete the field + its prepare-time recalc lines. **Surface to Jeff at Task 1 finding-time, not in this task** (S6-style spec call).
- [ ] Tell Jeff: "Run `do_build.bat`. Then in Debug + Release:
  - **(1)** Full end-to-end stress: project with audio playing on every Builder row that has audio (≥4 rows ideally — kicks / hats / bass-line / pad / vocal sample). Multi-core ON. Verify every per-row meter reads correctly with no glitch / drop / lag vs the pre-batch MT baseline. Cross-verify the Clips Bus meter (the parent bus) shows the summed activity matching what the rows show.
  - **(2)** Toggle Multi-core OFF (serial-diagnostic mode). Verify the same stress arrangement: every per-row meter still reads correctly under serial execution.
  - **(3)** Toggle Multi-core back ON. Save the project. Close. Reopen. Confirm every per-row meter still reads correctly post-reload.
  - **(4)** 20ms-decay subjective comparison (L8 / B2 verify): from any state with active per-row meters, trigger each early-return condition (mute / choke / file-out-of-range) and verify the affected row's meter **decays over ~20ms** matching the bus meter ballistic — not the pre-batch instant-silent-in-1-block behavior. Compare side-by-side: hit mute on the row strip vs hit mute on the Clips Bus strip — the decay rate should look identical."
- [ ] Wait for Jeff's verify result.
- [ ] On pass: `/draft-commit`, surface, commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 4 — Close sequence
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

After Task 3 commit lands (cleanup + comment sweep + grep cleanliness) and before the close commit (Task 4):

1. **Build clean.** `do_build.bat` Release + Debug both green.
2. **Per-row meter sanity (MT ON).** Project with audio on every Builder row that has audio (≥4 rows). Every row's per-row meter reads activity matching its source signal. No stuck peaks; clean ~20ms ballistic decay to silence on stop (matches bus meter decay).
3. **Per-row meter sanity (MT OFF / serial-diagnostic).** Same arrangement, Multi-core OFF. Every row meter still reads correctly under serial execution.
4. **20ms-decay coverage** (L8 / B2 alignment to bus behavior — replaces the pre-batch instant-silent-on-mute behavior). All three early-return predicates exercised:
   - **Muted by choke**: choke group hits → muted row's meter decays over ~20ms (DBFSMeter ballistic).
   - **Row muted / Builder-row muted**: mute toggle → row meter decays over ~20ms.
   - **File-position out of range**: clip past file end → row meter decays over ~20ms.
   - **Subjective comparison**: hit mute on a bus strip vs hit mute on a Builder-row strip — the decay rate should look identical post-batch (same DBFSMeter ballistic on both).
5. **Project save/reload.** Save the stress arrangement; close project; reopen; per-row meters still read correctly post-reload.
6. **Grep cleanliness.** `grep -rn "mAudioRowPeakDbRun\|mAudioRowPeakDbLRun\|mAudioRowPeakDbRRun" Source/` shows zero hits.
7. **Bus regression check.** Every bus meter (the 13 G1 buses landed by QA-Eg) still reads correctly — no spillover from the per-row migration into the bus surface (the InsertNode `:1241` CAS-max upgrade is the cross-surface touch point worth eyeballing).
8. **Build-log clean.** No new warnings introduced by the VibeGraph array additions, the InsertNode publish-site upgrade, or the per-site migration.

---

## MT-awareness static-analysis

The batch is meter / UI-state only. No audio-thread arithmetic changes. Under L7 / Option B, the migration adopts the exact same threading shape as the QA-Eg bus migration: the InsertNode publish site at [VibeGraph.cpp:1241](Source/VibeGraph.cpp:1241) gets a one-line CAS-max upgrade (so consecutive `processInsert` calls within one `CompositeAudioInsertTask::run` accumulate); end-of-task exchange-store moves InsertNode peakDb into the `VibeGraph::audioRowPeakDb*` public-member arrays; `drainMeterAtomicsForUI` drains them at end-of-block. Same primitives (`std::atomic<float>` + `exchange` + `compare_exchange_weak`), same memory ordering (`memory_order_relaxed`), same threading shape (audio thread or worker thread writes during the block; audio thread drains end-of-block). `CompositeAudioInsertTask::run` is already MT-validated (it runs on worker threads under MT and on the audio thread under 1-worker serial-diagnostic — the same dual mode every existing bus migration already proved). No new races introduced.

The InsertNode `:1241` CAS-max upgrade is the cross-surface touch point — every InsertKind (Layer / Bass / Drum / Audio / Aux / Vox / Inst / Rusty) goes through `processInsert` and hits that site. Task 1 confirms the change is mechanically safe for ALL kinds, not just `InsertKind::Audio`.

---

## Routing notes (Rule 3 application during execution)

- Findings about meter behavior on **other surfaces** (per-insert meters on tabs, per-strip meters that aren't bus or row, master-output bus meter) → log in running notes; route at close per Rule 3 (most likely outside-batch since this is per-row scope).
- Findings about **publishPeakReading or InsertNode `:1241` publish-site CAS-max semantics** that don't match the L7 assumption — surface to Jeff as an L7 re-spec call per `feedback_dont_make_unilateral_spec_calls.md`; do NOT silently pivot. If the InsertNode CAS-max upgrade has a hidden side effect on the bus path, that's a real blocker that needs the Sub-B Option A fallback re-considered.
- Findings about **MT-vs-serial parity** (anything where Multi-core OFF doesn't produce the same meter behavior as Multi-core ON) → investigate in-batch per `feedback_qa_batches_fix_bugs_dont_defer.md`; same diagnostic instinct as QA-Eg.
- Findings about **`peakDecayDbPerBlock`-equivalent dead-state** on InsertNode → surface at Task 1 inventory; fix in Task 3 cleanup if applicable (mirrors QA-Eg's S6).
- Findings about **CompositeAudioInsertTask hot-path performance** (the migration touches a hot per-block site; if the end-of-task exchange-store + the InsertNode `:1241` CAS-max upgrade are observably slower than the pre-batch CAS-max-into-`*Run`) → route to `/perf-audit` for follow-up; minor regression is acceptable (the bus migration showed no measurable delta).
- Findings about **other QA-Eg-adjacent architectural smells** (any `*Run`-style mirror anywhere else not previously surfaced) → log + surface to Jeff; route per Rule 3 at close.

---

## Carry-Forward Reference touch points

- **§1 (MT Render Path Primitives)** — read at Task 1 start; confirms the file:line index for the per-row publishing surface (note: §1 was frozen 2026-05-07; the per-row mirror was added 2026-05-02 so it predates the freeze; the field:line citations should still hold but verify).
- **§2 (Mixer / Routing architecture)** — read at Task 1 start; confirms the `kMaxAudioRows = 50` row model + `MixerChannelIds 400..449` audio-row ID block + `InsertKind::Audio` mapping.
- **§4 (Lock-free primitives)** — read at Task 1 start; confirms `publishPeakReading` + ring-buffer + relaxed-store pattern is the locked architectural primitive — and that the migration adopts the same primitive as the buses already do.

---

## Carry-Over

(populated mid-batch on session pause per §0 Rule 2; empty at batch open)
