# QA-InsertMaps — Flatten InsertNode std::map to std::array — Plan (zany-wandering-russell)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/zany-wandering-russell.md`
> Paired running notes: `Plans & Specs/Running Notes/zany-wandering-russell.md`

> **For execution:** use `superpowers:executing-plans` inline. Steps use `- [ ]` checkbox syntax for tracking. Build commands run by Jeff (`do_build.bat`) — never by Claude. Verify in Debug exe FIRST, then Release (per CLAUDE.md Build System standing rule).

## Context

QA-InsertMaps is the second batch in the QA-Eg-close-spawned perf-audit cluster (`... → QA-AudioMeters → QA-InsertMaps → QA-VoicePool → QA-EngineApvts → QA-Ed ...`). It flattens the 8 `std::map<int, std::unique_ptr<InsertNode>>` member tables on `VibeGraph` ([Source/VibeGraph.h:727-733+750](Source/VibeGraph.h:727)) into a single owning array indexed directly by ChannelId, eliminating the `selectInsertMap` switch ([Source/VibeGraph.cpp:2253-2277](Source/VibeGraph.cpp:2253)) + the 4× red-black-tree walks per insert per audio block (1 in `processInsert` via `getInsertNode` :2335 + 3 inside `pushScArrayToStrip` via `getInsertPreEQ` / `getInsertRack` / `getInsertEQ` :2887). Mirrors `RenderGraphDispatcher::mTasksByChannel`'s already-in-tree flat-array-by-ChannelId pattern; ends the architectural inconsistency between the dispatcher side (flat array) and the InsertNode side (8 std::maps).

**Origin:** `/perf-audit` H1 at QA-Eg close 2026-05-24 (also absorbs M3 — UI-side `getInsertPeakDb` per-vblank `std::map::find`). Confirmed via source-trace at plan-open: the §5 entry's "4× map.find per insert per block" claim verified ([VibeGraph.cpp:2335 + :2887](Source/VibeGraph.cpp:2335)); the §5 entry's "touches" list is a routing-level subset of the real surface (pre-flight inventory in Task 1 will map the full surface — initial grep found 12+ iteration sites over the 8 maps + 5 direct `.find()` callers outside `selectInsertMap` + `walkInserts` / `addInsertMap` / `restoreInsert` / `promoteRacksInMap` / G1-drain sweeps).

**Architectural shift:** 8 `std::map<int, std::unique_ptr<InsertNode>>` → 1 `std::array<std::unique_ptr<InsertNode>, kMaxStripChannels>` indexed directly by ChannelId. Hot-path lookups become single-pointer indirection. Existing `(kind, index)` external API kept as thin wrappers (zero call-site churn at the 8+ consumer files); internal lookup substitutes the flat-array index for the selectInsertMap-then-find pair.

**Risk:** **medium** — audio-thread hot-path refactor; many call sites to migrate; behavioral change is mechanical (lookup mechanism only — no audio-path arithmetic change). Worst case: a migration site missed silently still does map lookups (caught by grep post-refactor) OR a null-check site missed (caught by Debug build's `jassert` on first run with a nonexistent insert).

**Effort estimate:** ~5-8 hours per §5 entry. Per-task breakdown: Task 0 docs ~10 min, Task 1 inventory ~30-45 min, Task 2 structural one-shot ~3-4 hr, Task 3 stress-file verify ~30 min (Jeff's normal cycle), Task 4 cleanup + grep sweep ~30 min, Task 5 close sequence ~1 hr.

**Dependencies:** QA-AudioMeters closed (just landed 2026-05-24 at `65f57ad`). Specifically, QA-AudioMeters added 8 per-kind range-for sweeps at [VibeGraph.cpp:2930-2951](Source/VibeGraph.cpp:2930) (drainMeterAtomicsForUI G1 8-per-kind loop) which this batch must convert to the new flat-array iteration shape.

**Estimated CPU win:** ~1-3% on busy sessions per `/perf-audit` H1 (red-black-tree walk replaced with single pointer-load at ~30k+ lookups/sec on ~50 active inserts at ~6 ms block cadence).

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| L1 | **Option 2** — single `std::array<...>` flat by ChannelId, mirror `RenderGraphDispatcher::mTasksByChannel`. (Rejected: leave-as-is + per-kind flat arrays.) | Jeff-locked at QA-Eg close 2026-05-24; §9 thirty-third Forks entry; `/perf-audit` H1 priority. |
| L2 | **kMaxStripChannels = 1000** (covers full 0..999 MixerChannelIds allocation; ~8 KB at 8 B/ptr per array, sparsely populated). | §5 entry: "Keep `kMaxStripChannels` sized to the existing `MixerChannelIds` allocation (0..999) — the array is sparsely populated; `nullptr` slots are the 'no insert at this id' signal that callers already null-check." |
| L3 | **Sequencing**: immediately after QA-AudioMeters, before QA-VoicePool. | §6 arrow; §5 entry; Jeff's confirmed slot per `feedback_slot_placement_is_spec_call.md`. |
| L4 | **Plan-file silly-name** = `zany-wandering-russell` (plan-mode runtime assignment). | Per `feedback_silly_name_is_my_pick.md` — naming is Claude's call, not a spec call; runtime auto-assigned. |
| L5 | **MT verification cadence**: normal Debug-then-Release per-task verify; no separate MT-vs-serial verification pass. | Both code paths (MT default + serial diagnostic) call into the same `processInsert` entry; lookup mechanism is identical across both branches. MT works in Debug AND Release per QA-Md (closed 2026-05-09 — `project_mt_engine_works_in_debug.md`). |
| L6 | **Sub-A — Storage shape** = `std::array<std::unique_ptr<InsertNode>, kMaxStripChannels> mInsertsByChannel` (array IS owning storage; lookup via `[chId].get()`). | Resolved 2026-05-24 (Jeff, post-ExitPlanMode sub-spec dispatch). Single source of truth; pointer-stable (unique_ptr doesn't move); `.get()` is a single load with zero indirection cost beyond the array index itself. |
| L7 | **Sub-B — API shape** = keep `(kind, index)` external API as thin wrappers; chId computed inside via `int computeChannelId(InsertKind, int)` helper. | Resolved 2026-05-24 (Jeff). Zero call-site churn at the 30+ external sites in 6 files; encapsulation preserved; hot-path win is in the flat-array lookup, not the API shape. |
| L8 | **Sub-C — Iteration replacement** = single `std::vector<int> mLiveInsertChannels` companion list (push on `ensureInsertNode`, erase on `removeInsertNode`); kind-aware loops use per-iter `node->kind` dispatch. | Resolved 2026-05-24 (Jeff). Matches §5 entry's "small `mLiveInsertChannels` companion list" wording verbatim; allocations only on add/remove (message thread, never audio-thread); typical live-insert counts ~20-50. |
| L9 | **Sub-D — InsertNode caches chId** = YES. Add `int chId` member to `InsertNode`, set at construction in `ensureInsertNode` via `computeChannelId(kind, index)`; `processInsert`'s 8-way kind→chId switch at [VibeGraph.cpp:2343-2353](Source/VibeGraph.cpp:2343) dies (uses `node->chId` for the `pushScArrayToStrip` call). | Resolved 2026-05-24 (Jeff — "Cache chId on the InsertNode at construction. Kill the switch statement on the audio thread."). Coherence with channel-id-keyed storage; switch death on the audio thread is a clean natural consequence; G1 drain loops use `node->kind` for per-kind dispatch (L8 interaction). |
| L10 | **Sub-E — Verify cadence** = single all-kinds stress-file verify (Jeff's existing big stress-test arrangement exercising all 8 InsertKinds + EffectRack slot meters + buses + mute decay in one pass), mirroring QA-AudioMeters' L6 re-collapse to 6-task structure. | Resolved 2026-05-24 (Jeff). Per `feedback_no_full_release_reverify_at_batch_close.md`; per-kind verify locks into impractical tab-switching (QA-AudioMeters L6 re-collapse rationale); the lookup-mechanism swap is mechanical with no per-kind behavioral difference. |
| L11 | **Sub-F — Task count / structure** = 6-task QA-AudioMeters mirror: Task 0 open / Task 1 inventory / Task 2 structural one-shot / Task 3 stress-file verify / Task 4 cleanup + grep sweep / Task 5 close. | Resolved 2026-05-24 (Jeff — "Use the proven 6-task structure with a single, comprehensive stress-file verification pass."). QA-AudioMeters' final 6-task shape (after L6 re-collapse) landed mechanically clean; structural arc is similar; `feedback_commit_at_checkpoints.md` fits cleanly with this cadence. |

---

## Sub-spec calls surfaced for ExitPlanMode

**Resolved 2026-05-24 (Jeff, post-ExitPlanMode sub-spec dispatch — see L6-L11 above for the locked decisions):** All 6 sub-spec calls picked = **Option (a)**, the Recommended option in every row of the table below.  Decisions also captured in L6-L11 of the "Spec calls already locked" table above for one-stop reference; the table below is preserved as the plan record per the federated-bouncing-cupcake exemplar pattern (question / option set / recommendation / reasoning).

| ID | Question | Recommendation | Reasoning |
|----|----------|----------------|-----------|
| Sub-A | **Storage shape** — (a) `std::array<std::unique_ptr<InsertNode>, kMaxStripChannels> mInsertsByChannel` (array IS owning storage; lookup via `[chId].get()`); (b) separate owning `std::vector<std::unique_ptr<InsertNode>>` + lookup `std::array<InsertNode*, kMaxStripChannels>`; (c) owning array + raw-pointer hot-path array? | **(a)** — array IS owning storage, lookup via `.get()`. | Single source of truth; pointer-stable (unique_ptr doesn't move); `.get()` is a single load with zero indirection cost beyond the array index itself. (b) doubles ensure/remove bookkeeping; (c) is redundant with (a). |
| Sub-B | **API shape** — (a) keep `(kind, index)` external API as thin wrappers (chId computed inside wrapper via `int computeChannelId(kind, index)` helper); (b) migrate every call site to `(channelId)`-only; (c) both — overloads for both forms? | **(a)** — keep `(kind, index)` external API, change internal lookup only. | 30+ external call sites in 6 files (`PluginProcessor.cpp` × 8 ensure + 2 remove + 2 EQ; `EffectsPage.cpp` × 22; `StandaloneEditor.cpp` × 5; `MixerPage.cpp` × 1; `ClipsPage.cpp` × comments only). Zero call-site churn at consumers; the `(kind, index) → chId` switch is `O(1)` and compiler-inlinable; the hot-path win is in the flat-array lookup, not the API shape. (b) inflates batch scope significantly with no perf benefit. |
| Sub-C | **Iteration replacement** — the 12+ existing range-for loops over individual maps (drainMeterAtomicsForUI G1 8-per-kind, walkInserts × 8, addInsertMap × 8 XML save, restoreInsert × 8 XML restore, promoteRacksInMap × 8, etc.) need a replacement. (a) `std::vector<int> mLiveInsertChannels` single companion list (push on `ensureInsertNode`, erase on `removeInsertNode`) — single allocation per add/remove (message thread only, never audio-thread); call sites iterate the list and `mInsertsByChannel[chId].get()`; per-iteration `node->kind` switch routes to per-kind destination where the loop is kind-aware. (b) 8 per-kind sub-lists (`std::array<std::vector<int>, 8>`) — direct per-kind iteration with no per-iteration kind switch. (c) Sparse scan over the full 1000-entry array, skipping nulls — zero companion-list bookkeeping, but 1000 pointer-loads per iteration. | **(a)** single `std::vector<int> mLiveInsertChannels` companion list with per-iteration `node->kind` dispatch in the kind-aware loops. | (a) matches the §5 entry's "small `mLiveInsertChannels` companion list" wording verbatim; one allocation per add/remove (rare, message-thread); typical live-insert counts are ~20-50 so the per-kind dispatch switch happens that many times per iteration (cheap). (b) doubles add/remove bookkeeping and adds 8 separate vectors. (c) wastes 1000 pointer-loads on iteration sites that don't justify it. |
| Sub-D | **InsertNode caches chId?** — (a) YES — add `int chId` member to `InsertNode`, compute at construction in `ensureInsertNode` via the same `computeChannelId(kind, index)` helper; `processInsert`'s 8-way switch at [VibeGraph.cpp:2343-2353](Source/VibeGraph.cpp:2343) dies (uses `node->chId` for the `pushScArrayToStrip` call); the G1 drain loops use `node->kind` for per-kind dispatch (Sub-C interaction). (b) NO — keep `processInsert`'s switch; InsertNode doesn't know its chId. | **(a)** — InsertNode caches chId, processInsert switch dies. | Coherence with the channel-id-keyed storage; InsertNode already knows its `kind` + `index` so chId is trivially derivable at construction (one switch in `ensureInsertNode`); processInsert's switch death is a natural consequence; the `node->chId` cached read is faster than re-computing the switch every audio block per insert. |
| Sub-E | **Verify cadence** — (a) single all-kinds stress-file verify (Jeff's normal big stress-test arrangement exercising all 8 InsertKinds + EffectRack slot meters + buses + mute decay), mirroring QA-AudioMeters' L6 re-collapse to 6-task structure; (b) per-kind verify checkpoint per InsertKind (8 verify Tasks), mirroring QA-AudioMeters' original L7-pivot 10-task structure. | **(a)** single all-kinds stress-file verify per QA-AudioMeters L6 precedent. | `feedback_no_full_release_reverify_at_batch_close.md` — Jeff's per-task verify cycle covers Debug+Release; per-kind verify locks into impractical tab-switching (QA-AudioMeters L6 re-collapse rationale); the lookup-mechanism swap is mechanical with no per-kind behavioral difference. |
| Sub-F | **Task count / structure** — (a) 6-task QA-AudioMeters mirror: Task 0 open / Task 1 inventory / Task 2 structural one-shot / Task 3 stress-file verify / Task 4 cleanup + grep sweep / Task 5 close; (b) different shape (more or fewer tasks, different split)? | **(a)** 6-task QA-AudioMeters mirror. | QA-AudioMeters' final 6-task shape (after L6 re-collapse) landed mechanically clean; the structural arc is similar (architectural shift across many call sites + verify + cleanup); reusing the proven shape minimizes process risk; Jeff's `feedback_commit_at_checkpoints.md` rule fits cleanly with this cadence. |

---

## Files to modify

### Task 1 — Pre-flight inventory (read-only)
- Grep + read every call site to verify the full surface; expand the §5 entry's incomplete touch list into a complete file:line map for Task 2; no source edits.

### Task 2 — Structural one-shot

**[Source/VibeGraph.h](Source/VibeGraph.h)** — 8 std::map declarations → 1 flat array + companion list:
- :466 — `struct InsertNode;` forward decl: add `int chId` member visible to call sites (full def stays in .cpp).
- :472-486 — Keep existing public API `ensureInsertNode / removeInsertNode / getInsertNode / getInsertRack / getInsertEQ / getInsertPreEQ` signatures (zero call-site churn per Sub-B(a)).
- :727-733+750 — Replace 8 `std::map<int, std::unique_ptr<InsertNode>>` member decls with:
  - `static constexpr int kMaxStripChannels = 1000;`
  - `std::array<std::unique_ptr<InsertNode>, kMaxStripChannels> mInsertsByChannel;`
  - `std::vector<int> mLiveInsertChannels;`  // populated by ensure / cleared by remove
- :724-726 — Rewrite the doc comment block ("Created lazily via ensureInsertNode(). Each kind has its own map for O(1) lookup..." → describe flat-array-by-chId with companion live-channels list).

**[Source/VibeGraph.cpp](Source/VibeGraph.cpp)** — replace selectInsertMap + every map call site:
- :2253-2277 — `selectInsertMap` namespace anon helper: **delete entire helper** (replaced by `computeChannelId(kind, index)` static-inline helper).
- :2281-2314 — `ensureInsertNode`: rewrite to use `mInsertsByChannel[chId]` lookup; cache `node->chId = chId` at construction; push `chId` onto `mLiveInsertChannels` if newly created.
- :2316-2320 — `removeInsertNode`: rewrite to `mInsertsByChannel[chId].reset()` + erase chId from `mLiveInsertChannels`.
- :2322-2329 — `getInsertNode`: rewrite to `mInsertsByChannel[computeChannelId(kind, index)].get()`.
- :2331-2424 — `processInsert`: replace 8-way switch at :2343-2353 with `node->chId` (cached); rest of function (storeAxes CAS-max etc.) untouched (QA-AudioMeters territory).
- :2425-2447 — `getInsertRack` / `getInsertEQ` / `getInsertPreEQ` wrappers: unchanged signatures, route through new `getInsertNode`.
- :1364-1365, :1409, :2015-2017, :2567 — for-loops over `{ &mLayerInserts, &mBassInserts, ... }` pointer initializer lists: rewrite as range-for over `mLiveInsertChannels` with per-iteration `mInsertsByChannel[chId].get()` (the function bodies determine whether per-kind dispatch is needed via `node->kind`).
- :1755-1762 — `walkInserts(mXxxInserts)` × 8: replace with single `for (int chId : mLiveInsertChannels) walkInserts(mInsertsByChannel[chId].get())`.
- :1798-1813 — direct `.find()` × 3 (Layer + Bass + Aux specific lookups): rewrite as `mInsertsByChannel[computeChannelId(kind, idx)].get()`.
- :1983-1990 — `addInsertMap(name, mXxxInserts)` × 8 (XML save): rewrite as iteration over `mLiveInsertChannels` with `MixerChannelIds::prefixFromChannelId(chId)` for kind label; OR keep 8 calls and route to a unified internal helper that walks the live list filtered by kind.
- :2126-2133 — `restoreInsert(name, mXxxInserts)` × 8 (XML restore): mirror the save-side approach.
- :2195+:2207 — `mAudioInserts.find(row)` × 2 (Audio-specific lookups in pushAudio-related sites): rewrite as `mInsertsByChannel[MixerChannelIds::audioInsert(row)].get()`.
- :2477-2484 — `promoteRacksInMap(mXxxInserts)` × 8 (QA-AudioMeters rack-promotion sweep): replace with `for (int chId : mLiveInsertChannels) promoteRacksInMap(mInsertsByChannel[chId].get())`.
- :2512-2514 — another selectInsertMap-style switch (need to verify Task 1 inventory what this drives): rewrite per the same pattern.
- :2853-2890 — `pushScArrayToStrip`: the `push3(getInsertPreEQ(kind, idx), getInsertRack(kind, idx), getInsertEQ(kind, idx))` call at :2887 — verify Task 1 inventory whether `kind, idx` arrive from the call-site OR from a chId reverse-lookup. Adjust to use cached node accessor.
- :2930-2951 — drainMeterAtomicsForUI G1 8-per-kind loops (QA-AudioMeters): replace 8 separate range-for blocks with single `for (int chId : mLiveInsertChannels) { auto* node = mInsertsByChannel[chId].get(); if (!node) continue; switch (node->kind) { case Layer: ... case Bass: ... } }` routing to the correct per-kind PluginProcessor mirror.
- :2970 — `mAuxInserts` range-for (Aux-specific iteration): rewrite as filtered range over `mLiveInsertChannels` with `node->kind == Aux` filter.
- Top of file or near InsertNode definition — add `static inline int computeChannelId(InsertKind kind, int index)` helper performing the per-kind base-offset math (mirrors the existing `MixerChannelIds::<kind>Insert(idx)` inline functions; could just delegate to those + switch).
- InsertNode full definition: add `int chId;` field; constructor adds `chId` parameter (set by `ensureInsertNode`).

**[Source/PluginProcessor.cpp](Source/PluginProcessor.cpp)** — call-site verify (no functional change expected per Sub-B(a)):
- :3543, :3581, :3622, :4009, :4033, :4081, :4093, :4108 — 8 `ensureInsertNode` calls (one per InsertKind): API signature unchanged, no edit.
- :4138, :4354 — 2 `removeInsertNode` calls: API signature unchanged, no edit.
- :2390, :2445 — 2 `getInsertEQ` / `getInsertPreEQ` calls: API signature unchanged, no edit.
- :4345-4370 — legacy direct removeInsertNode path comment block: update if it references the old map mechanism.

**[Source/Standalone/EffectsPage.cpp](Source/Standalone/EffectsPage.cpp)** — call-site verify (no functional change):
- :405-483 — 22 `getInsertRack` / `getInsertEQ` calls (per-kind switch dispatching to engine + drum + audio + aux + vox + inst + rusty + bass): API unchanged, no edit.
- :540-547 — hardcoded chId-range-to-(kind, index) switch for preEQ lookup: SIMPLIFY post-batch to a single `mVibeGraph.getInsertPreEQByChannel(chId)` accessor if added (NEW helper to consider — see Sub-B sub-question below). OR leave as-is (still works; just calls existing per-kind getters).

**[Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp)** — call-site verify:
- :3012-3020, :3222 — 5 `getInsertRack` calls: API unchanged, no edit.

**[Source/Standalone/MixerPage.cpp](Source/Standalone/MixerPage.cpp)** — call-site verify:
- :2822 — 1 `removeInsertNode(Aux, idx)` call: API unchanged, no edit.

**[Source/Clips/ClipsPage.cpp](Source/Clips/ClipsPage.cpp)** — comments only: no source edit.

### Task 4 — Cleanup + grep sweep
- Sweep `grep -rn "selectInsertMap"` — should be ZERO matches (helper deleted).
- Sweep `grep -rn "m\(Layer\|Bass\|Drum\|Audio\|Aux\|Vox\|Inst\|Rusty\)Inserts\."` — should be ZERO matches in `.cpp` files (only the new flat array + companion list remain; old member-name references gone).
- Sweep stale doc comments: VibeGraph.h:724-726 + any blueprint-doc reference to "Each kind has its own map for O(1) lookup".
- Sweep stale "8 std::map" / "per-kind map" / "tree walk" references in comments that became stale post-flatten.
- Carry-Forward Reference §1 contradiction note: per-insert std::map architecture replaced with flat-array — captured in close entry (per `feedback_closed_batch_carryforward_via_forks.md` no Carry-Forward edits, contradiction goes in Work Log close).

---

## Tasks

### Task 0 — Open commit

- [ ] Mirror `~/.claude/plans/zany-wandering-russell.md` → `Plans & Specs/Batch Plans/zany-wandering-russell.md` (Write tool); **delete the home-dir copy** (per `feedback_plan_mirror_one_way.md`).
- [ ] Update Main Plan §5 QA-InsertMaps entry header with `**Plan file:** Plans & Specs/Batch Plans/zany-wandering-russell.md` line (replacing the current `**Plan file:** <silly-name>.md (when started)` placeholder).
- [ ] Seed `Plans & Specs/Running Notes/zany-wandering-russell.md` with header + purpose blockquote + pair-file reference + convention reference + initial `## 2026-05-24 — Task 0 — open` entry per Main Plan §0:235-239 required sections.
- [ ] Surface full `git status`. Dispatch `/draft-commit`. Surface drafted message + git status to Jeff for approval. Commit on approval.
- [ ] Mark Task 0 done; update TaskList.

### Task 1 — Pre-flight inventory (read-only)

Validate the §5 entry's "touches" list against the actual surface; map every call site before Task 2's structural one-shot. Output: a complete file:line table appended to running notes for Task 2 reference.

- [ ] Grep + read `selectInsertMap` (anonymous-namespace helper at VibeGraph.cpp:2253) — confirm only the 3 callers (ensureInsertNode :2285, removeInsertNode :2318, getInsertNode :2325).
- [ ] Grep + read direct `m(Layer|Bass|Drum|Audio|Aux|Vox|Inst|Rusty)Inserts` references across all of `Source/` — expand on the initial 50+ hits seen at plan-open; categorize each as `range-for / direct .find() / member access / comment reference`.
- [ ] Grep + read the 2nd selectInsertMap-style switch at VibeGraph.cpp:2512-2514 — identify the surrounding function + what it drives.
- [ ] Grep + read `pushScArrayToStrip` callers across `Source/` (not just `processInsert`) — verify the chId-vs-(kind, index) argument shape at every call site.
- [ ] Grep + read `walkInserts`, `addInsertMap`, `restoreInsert`, `promoteRacksInMap` helper definitions — note the per-kind name-label argument so the rewrite preserves XML-tag compatibility.
- [ ] Inspect the InsertNode full definition in VibeGraph.cpp (forward-declared at :466) — note current constructor signature for adding the `chId` parameter.
- [ ] Confirm CompositeAudioInsertTask / VoxStripTask / InstStripTask / EngineInsertTask do NOT call the VibeGraph insert accessors directly (initial grep showed no matches in `Source/Engine/Tasks/`).
- [ ] Append a `## Task 1 — Pre-Flight Inventory` section to running notes with: complete file:line table of every site that touches the 8 std::maps, the 2 selectInsertMap-style switches, the InsertNode definition file:line, and any surprises that require sub-spec call follow-up.
- [ ] Dispatch `/draft-doc running-notes` and apply.
- [ ] Surface full `git status`. Dispatch `/draft-commit` (docs-only commit per QA-AudioMeters Task 1 precedent at `3c87264`). Surface message + status. Commit on approval.

### Task 2 — Structural one-shot (the big architectural shift)

Single commit landing the flat-array storage + all migration sites in one mechanical sweep. Mirrors QA-AudioMeters' Task 2 shape at `0fd9b91` (6 files, +362/-283).

- [ ] Edit [Source/VibeGraph.h](Source/VibeGraph.h): add `static constexpr int kMaxStripChannels = 1000`; add `int chId` to forward-decl InsertNode visibility (or wait until Task 1 confirms it's safe to expose via the forward decl alone); replace 8 std::map decls with `std::array<std::unique_ptr<InsertNode>, kMaxStripChannels> mInsertsByChannel` + `std::vector<int> mLiveInsertChannels`; rewrite the :724-726 doc comment.
- [ ] Edit [Source/VibeGraph.cpp](Source/VibeGraph.cpp): add `static inline int computeChannelId(InsertKind kind, int index)` helper near top; delete `selectInsertMap` anon helper at :2253-2277; rewrite `ensureInsertNode` (chId cache + companion-list push) + `removeInsertNode` (reset + companion-list erase) + `getInsertNode` (single array lookup); rewrite `processInsert`'s chId switch at :2343-2353 → `node->chId`; rewrite the 5 direct `.find()` sites at :1798/:1805/:1813/:2195/:2207; rewrite the 12+ iteration sites (walkInserts × 8, addInsertMap × 8, restoreInsert × 8, promoteRacksInMap × 8, drainMeterAtomicsForUI G1 sweeps at :2930-2951, :2970 Aux range-for, :1364/:1409/:2015/:2567 pointer-initializer-list loops); add `chId` parameter to InsertNode constructor + member init.
- [ ] Edit [Source/PluginProcessor.cpp](Source/PluginProcessor.cpp) + [EffectsPage.cpp](Source/Standalone/EffectsPage.cpp) + [StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp) + [MixerPage.cpp](Source/Standalone/MixerPage.cpp) ONLY IF Task 1 inventory surfaces any actual call-site impact (per Sub-B(a) keep-external-API recommendation, expected to be comment-only).
- [ ] Tell Jeff: "Run `do_build.bat`. Both Release and Debug must come up clean. No new warnings, no jasserts firing at startup."
- [ ] Wait for Jeff's build report.
- [ ] If build fails: dispatch `/diagnose-build` with the failing block from `build_log.txt`; surface root cause + fix candidates to Jeff; iterate.
- [ ] If build passes: surface full git status. Dispatch `/draft-commit`. Surface message + git status to Jeff for approval. Commit on approval.
- [ ] Dispatch `/draft-doc running-notes` and apply.

### Task 3 — All-kinds stress-file verify

Per QA-AudioMeters L6 precedent: single all-kinds stress-file verify (vs per-kind verify Tasks). Jeff's existing big stress-test arrangement exercises all 8 InsertKinds + EffectRack slot meters + 13 G1 buses in parallel.

- [ ] Tell Jeff: "Load your existing all-kinds stress-test arrangement (the one used at QA-AudioMeters Task 3). Run for ~30 sec. Watchlist:
  - **(1)** All 8 InsertKind per-strip meters (Layer / Bass / Drum / Audio insert / Aux / Vox / Inst / Rusty) read activity matching source + decay smoothly.
  - **(2)** All 13 G1 bus meters read activity correctly (regression check — the flat-array refactor shouldn't touch buses, but cross-surface verification catches accidental breakage).
  - **(3)** Click a strip mute on any kind → strip meter ballistic decay over ~20 ms (no snap-to-floor) matches bus mute behavior.
  - **(4)** MT-on (default) vs MT-off (Mixer hamburger → Multi-Core Rendering toggle): identical metering behavior in both modes.
  - **(5)** Save + reload the project: every insert still routes audio correctly post-load.
  - **(6)** EffectRack slot meter spot-check: click into any insert's effect rack, verify per-slot meters animate correctly during playback.
  - **(7)** Add / remove a strip during playback (create new Layer, delete an Aux): no audio glitch, no missing meter, no orphan strip post-remove.
  - **(8)** Audition gestures (right-click Layer tab → Play; piano roll click): immediate audible playback through the corrected lookup path."
- [ ] Wait for Jeff's verify report.
- [ ] If any scenario fails: surface to Jeff with proposed root-cause hypotheses; iterate on Task 2 fixes; commit fix as separate "Task 3 fix-up" commit (mirroring QA-AudioMeters' Task 5 fix-up pattern at `2cba7b7`).
- [ ] On full PASS: docs-only checkpoint commit if any plan-spec adjustment surfaced; otherwise no Task 3 commit (verify-only). Dispatch `/draft-doc running-notes` and apply.

### Task 4 — Cleanup + grep cleanliness sweep

Comment-only sweep + grep-cleanliness verification. Mirrors QA-AudioMeters Task 4 shape at `11b4fe7` (3 comment-only source edits).

- [ ] Run `grep -rn "selectInsertMap"` across `Source/` — expect ZERO matches.
- [ ] Run `grep -rn "m(Layer|Bass|Drum|Audio|Aux|Vox|Inst|Rusty)Inserts\\." Source/` — expect ZERO matches (no surviving direct map-member access).
- [ ] Run `grep -rn "Each kind has its own map" Source/` — expect ZERO matches (comment rewritten in Task 2).
- [ ] Run `grep -rn "std::map<int, std::unique_ptr<InsertNode>>"` — expect ZERO matches (all 8 decls gone).
- [ ] Audit remaining `mInsertsByChannel` / `mLiveInsertChannels` comments — make sure inline comments + function-header docstrings match post-batch reality.
- [ ] Surface any surprise stale comments to Jeff; deferred-or-fix decision per finding.
- [ ] Surface full git status. Dispatch `/draft-commit` (mirror QA-AudioMeters Task 4 docs-mostly commit message style). Surface message + git status to Jeff for approval. Commit on approval.
- [ ] Dispatch `/draft-doc running-notes` and apply.

### Task 5 — Close sequence

Mirrors QA-AudioMeters Task 5 shape: optional fix-up commit (if `/review-batch` finds BLOCKER / NEEDS-FIX) + close commit. Per `feedback_no_full_release_reverify_at_batch_close.md`: NO separate Release-re-verify gate at close — Jeff's per-task verify cycle covers Debug+Release.

- [ ] Dispatch `/draft-doc batch-close` with a synthesis of running notes.
- [ ] Apply the close-entry draft to `Plans & Specs/Implemented Work Log.md` via Edit (insert at top of Entries section after the QA-AudioMeters entry at line 1017).
- [ ] Dispatch `/review-batch QA-InsertMaps`.
- [ ] Address BLOCKERs + NEEDS-FIX in-batch per `feedback_closed_batch_carryforward_via_forks.md`. Land as separate "Task 5 fix-up" commit (between the cleanup commit and the close commit) so the rollback boundary stays clean.
- [ ] Defer NITs into the close-entry routing table.
- [ ] Update Main Plan §5 QA-InsertMaps STATUS banner with `/review-batch` outcome + close-commit SHA.
- [ ] Apply any Carry-Forward Reference contradiction notes to the close entry (per `feedback_closed_batch_carryforward_via_forks.md` — Carry-Forward is frozen; contradictions go in Work Log).
- [ ] Route side findings per Rule 3:
  - Resolved in-batch → close entry's routing table.
  - Outside-batch → §9 Forks entry + surface placement options to Jeff (no unilateral slot pick per `feedback_slot_placement_is_spec_call.md`).
- [ ] Surface full git status. Dispatch `/draft-commit` for the close commit. Surface message + status. Commit on approval.

---

## Verification (end-to-end smoke)

Post-Task-5 close commit, before declaring batch shipped:

1. **Build clean.** Release + Debug both green at the final close commit. No new warnings.
2. **All-kinds stress-file (Task 3 watchlist re-run).** Jeff's existing big stress-test arrangement; full 8-point watchlist PASS.
3. **`grep` cleanliness.** Zero surviving `selectInsertMap` / `m<Kind>Inserts.` / `std::map<int, std::unique_ptr<InsertNode>>` references in the source tree.
4. **CPU win measurement.** On a busy session (~50 inserts spread across the 8 kinds), CPU-load meter shows ~1-3% drop on the audio thread vs pre-batch baseline (per `/perf-audit` H1 estimate). NOT a gate — even if the win isn't observable Jeff-side, the architectural alignment with `mTasksByChannel` justifies the batch.
5. **No regression on QA-AudioMeters work.** All 8 InsertKind per-strip meters + 13 G1 bus meters + EffectRack slot meters + mute decay behavior + save+reload still pass Jeff's QA-AudioMeters Task 3 watchlist.

---

## Routing notes (Rule 3 application during execution)

- Findings about additional `.find()` / iteration sites surfaced during Task 1 inventory beyond the initial grep → fold into Task 2 scope; note as Task 1 finding in running notes (no §9 entry needed — same-batch surface).
- Findings about CompositeAudioInsertTask / Engine/Tasks/* calling VibeGraph insert accessors (initial grep said no — Task 1 verifies) → fold into Task 2 if found; surface to Jeff if scope inflation is significant.
- Findings about bus-side `std::map` usage (e.g. `mVoxBusNode` / `mInstBusNode` / similar bus-level state on VibeGraph) being a parallel cleanup candidate → route to Future State or §9 Forks per Jeff's call; out of QA-InsertMaps scope (insert-level only).
- Findings about UI-side `getInsertPeakDb` per-vblank `std::map::find` (perf-audit M3) → already absorbed by this batch per §5 ("Also absorbs the related M3 finding"); confirm during Task 4 grep sweep that no UI-side `<kind>Inserts.find(` survives.
- Findings about EffectsPage.cpp:540-547's hardcoded chId-range-to-(kind, index) switch being a simplification target → out-of-scope (it's a different ID-decoding pattern; doesn't touch the std::maps directly); route to Future State if it's worth a follow-up cleanup.
- Findings about diagnostic instrumentation (DBG / Logger / temp jasserts) added during Task 1 or Task 2 debug → log immediately in the running-notes Diagnostic Instrumentation Catalog per §0 Rule 4; strip Remove entries at Task 5 close after Jeff approves the strip list.

---

## Carry-Forward Reference touch points

- Read at Task 1 start: [Carry-Forward Reference.md §1](Plans & Specs/Carry-Forward Reference.md) — Render Engine Primitives (general orientation; the per-insert architecture is the relevant primitive, not docked at a specific §-section).
- Read at Task 2 start: [Carry-Forward Reference.md §3](Plans & Specs/Carry-Forward Reference.md:185) — Mixer / Page Lifecycle File:Line Index — mostly about MIX-* bug routings, but the `removeInsertNode` lifecycle context applies.
- Read at Task 2 start: [Carry-Forward Reference.md §4](Plans & Specs/Carry-Forward Reference.md:315) — Decisions Already Made — verify the "DSP-12 fix shape = Composite RenderTask" decision (already in CompositeAudioInsertTask which calls into processInsert via the task layer); no contradiction expected, just sanity check.
- Carry-Forward contradiction expected post-batch: per-insert std::map architecture (§1 implicit via file:line refs to `mLayerInserts` etc.) replaced with flat-array. Per `feedback_closed_batch_carryforward_via_forks.md` Carry-Forward is FROZEN; contradiction recorded in Work Log close entry instead.
