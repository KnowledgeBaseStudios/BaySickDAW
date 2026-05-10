# Running Notes — QA-D (federated-bouncing-cupcake)

> **Purpose.** Append-only mid-batch log of what was done, what was found, what
> was decided, and what was deferred during QA-D execution.  Compiled from
> `/draft-doc running-notes` dispatches at every significant checkpoint
> (commit landed / sub-task verified / finding captured / decision made /
> scope pivot / spec call resolved).
>
> At batch close, `/draft-doc batch-close` reads this file (plus git log,
> memory entries, the per-batch plan, and conversation context) and produces
> the single Implemented Work Log entry that goes into `Plans & Specs/
> Implemented Work Log.md`.  This file is the source-of-truth intermediate
> artifact during the batch; the close entry is the durable summary.
>
> **Pair file:** `Plans & Specs/Batch Plans/federated-bouncing-cupcake.md`
> (the per-batch plan).
>
> **Convention:** see `Plans & Specs/Main Plan.md` §0 (folder-scope rule +
> Agent Orchestration Rules' mid-batch checkpoint trigger).  Running-notes
> subfolder convention established 2026-05-09 mid-QA-A.

---

## 2026-05-10 — Task 0 (open) — pre-batch setup

### Done

- Pre-batch reads complete: `/standup`, Main Plan §5 QA-D entry + §6
  sequencing + §0 Agent Orchestration Rules + recent §9 Forks entries
  (7-10), Carry-Forward Reference §2/§4/§5/§6/§8 (project-load lifecycle
  primitives), recent Implemented Work Log entries (top 5 close entries
  via persisted reader).
- Plan-mode draft authored at `~/.claude/plans/federated-bouncing-cupcake.md`,
  approved by Jeff at ExitPlanMode.
- Plan mirrored to canonical `Plans & Specs/Batch Plans/federated-bouncing-cupcake.md`.
- Home-dir copy at `~/.claude/plans/federated-bouncing-cupcake.md` deleted
  (one-way mirror per `feedback_plan_mirror_one_way.md`).
- Main Plan §5 QA-D entry now points at the canonical plan file.
- This running-notes file seeded.

### Spec calls — locked at pre-batch

S1, S2, S3, S4, S5, S6, S7, S8 all locked per the plan file's "Spec calls
already locked" table.  S6 (one-commit-per-item) and S8 (silly-name) were
the only ones Jeff picked outright; the rest he asked for industry-
standard recommendations and approved them.

### Sub-spec calls — locked at ExitPlanMode

- **Sub-A (singular vs plural):** singular form (`Layer N` / `Bass N` /
  `Drum N`).  Matches existing internal `mTabName` style in
  LayersPage.cpp:113 / BassPage.cpp:104 / DrumPage.cpp:275 + Vox/Inst
  singular convention.
- **Sub-B (0-based vs 1-based):** 1-based (`Layer 1`, not `Layer 0`).
  Matches existing Vox/Inst/Clips behavior; user-friendly DAW UX.
- **Sub-C (counter behavior on saved-project load):** restart from 1 on
  closeAllDynamicTabs, then advance to `max(restored-name-numbers) + 1`
  after deserializeUIState.

### Findings (mid-planning)

- **STATE-02 reframe (per Jeff at spec-call surface):** original §5
  scope ("Guitar/Bass counters don't reset on new project") was the tip
  of a broader bug.  Actual user-visible symptom is that *every*
  dynamic-tab type's name-numbering reuses the freed-slot index when a
  tab is deleted and a new one is added (e.g. delete "Layer 2", add
  new → name becomes "Layer 2 again" instead of "Layer 4").  Layers/
  Bass/Drums tabs additionally don't carry a number suffix in the
  ribbon at all, while Vox/Inst/Clips do — unified inconsistency.
  Plan re-scoped: unified monotonic-counter scheme across all 8
  tab-naming sites (Layers / Bass / Drums / Vox / Inst-LiveInput /
  Inst-BaySickGuitars / Inst-BaySickBasses / Clips), all counters
  reset on closeAllDynamicTabs, all advanced past max-restored after
  deserializeUIState.
- **MenuBarModel ownership (Explore agent finding):** the bug was
  suppressed in vendored JUCE.  Current declaration order in
  PianoRoll.h:645-646 / BuilderPage.h:750-751 / DrumKitGrid.h:494-495
  has component declared FIRST, model declared SECOND, which means
  model is destroyed FIRST (reverse declaration order) — backwards.
  Fix shape: swap declaration order (model FIRST → destroyed LAST) +
  defensive `mMenuBar->setModel(nullptr); mMenuBar.reset()` at top of
  each container's destructor.
- **Closed-form check (Carry-Forward §2):** `ProjectManager::openProject`
  body at ProjectManager.cpp:280-300 already has `mIgnoreDirty` for the
  main APVTS but per-engine `ApvtsDirtyTracker` instances aren't
  gated by it — that's the STATE-01 gap.  Fix: wrap the dirty-hook
  callback in StandaloneEditor with an `isLoadingProject()` check
  (12 wrap sites per pre-batch grep).

### Routing notes (Rule 3, opened)

- (none yet — file held open for findings during execution)
