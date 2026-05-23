# Running Notes — QA-Eg (squishy-scribbling-flurry)

> Append-only mid-batch log. A new `## YYYY-MM-DD — Task N — <name>` entry is
> appended at every checkpoint (commit landed / sub-task verified / finding
> captured / spec call resolved / scope pivot) per
> `feedback_draft_doc_running_notes_every_checkpoint.md`. At batch close,
> `/draft-doc batch-close` consumes this file as the primary input for the
> single Implemented Work Log entry. Never edited retroactively.

**Pair:** `Plans & Specs/Batch Plans/squishy-scribbling-flurry.md` (the plan).
**Conventions:** Main Plan §0 — Document Formatting Conventions + running-notes
required sections (locked 2026-05-11) + Rule 4 (Diagnostic Instrumentation Catalog).

---

## 2026-05-23 — Task 0 — Batch open (docs)

- **Scope.** QA-Eg unifies bus-meter draining onto the G1 pattern (each bus node
  owns its peak; UI polls nodes directly via `drainMeterAtomicsForUI` — the FL
  Studio mixer model). Migrates the 8 G2-mirror buses (AudioClips / FX / Vox /
  Inst / Rusty + Vox2 / Inst2 / Inst3) off the `PluginProcessor::*Run`
  intermediate mirrors onto node-owned atomics + VibeGraph public-member
  atomics + a unified `drainAndMerge` loop. Supersedes QA-Ef's interim FX-bus
  Group-2-style fix.
- **Pre-batch.** `/standup` (QA-Ef closed `ad956bf` + paperwork `fcc2297`; 3
  unpushed commits ahead of `origin/main`; tree clean of source; only untracked
  `Templates/My Templates/` from QA-Ef close NIT-v). Full direct self-read of
  Main Plan §0 (lines 1-600 covering Rules 1-4 + Document Formatting Conventions
  incl. the locked Batch Plans + Running Notes required-sections rule + the
  federated-bouncing-cupcake exemplar + canonical buckets + Agent Orchestration
  Rules). `/read-doc`-style targeted reads: §5 QA-Eg entry (`Main Plan.md:1141-
  1159`), §6 sequencing arrow + footnotes (`:1686-1785`), §9 twenty-eighth
  Forks (QA-Eg routing, `:3933-3974`) + twenty-ninth (QA-NativeDialogs) +
  thirtieth (QA-ProjectSave). CLAUDE.md status claims spot-checked: QA-Ef
  shown closed in §5 STATUS banner; commit ladder matches Work Log.
- **Pre-plan source reads** (per `feedback_check_code_before_calling_it_expected.md`):
  - G1 BusNode struct definitions ([Source/VibeGraph.cpp:223 / :426 / :593 /
    :757 / :906](Source/VibeGraph.cpp) — each carries internal `peakDb / peakDbL
    / peakDbR` atomics).
  - G1 exchange-store pattern ([VibeGraph.cpp:1486-1488 master + :1545-1547
    layers + :1554-1556 bass + :1563-1565 drums](Source/VibeGraph.cpp:1486)).
  - G2 publishing site — CAS-max into `mBusPeakRefs` in `processBus` generic-bus
    section ([VibeGraph.cpp:1684-1705](Source/VibeGraph.cpp:1684)).
  - G2 pointer registration ([PluginProcessor.cpp:288-315](Source/PluginProcessor.cpp:288)).
  - `drainMeterAtomicsForUI` full body ([PluginProcessor.cpp:2085-2161](Source/PluginProcessor.cpp:2085)
    — G1 loop at `:2096-2108`, G2 promotion at `:2112-2151`, QA-Ef interim
    FX-bus block at `:2115-2133`).
  - InstrChannelNode definition ([VibeGraph.cpp:1276-1332](Source/VibeGraph.cpp:1276) —
    confirmed has NO peak atomics; just name/preEq/rack/eq/pPolarity/pWidth).
  - Per-row Builder audio meters ([PluginProcessor.h:645-654 + :620-622 +
    CompositeAudioInsertTask.cpp:113-115](Source/PluginProcessor.h:645) —
    confirmed same dual-mirror architecture as G2 buses; routed to S2 deferral).
- **Spec calls resolved with Jeff (2026-05-23):**
  - **S1 = all 8 G2 buses** (Clips / FX / Vox / Inst / Rusty + Vox2 / Inst2 /
    Inst3). Leaving secondaries on `*Run` re-creates exactly the split this
    batch removes.
  - **S2 = per-row Builder audio meters DEFERRED** to a new dedicated batch.
    Same architectural smell confirmed by source-read, but folding adds ~3-5
    hours + `kMaxAudioRows` verify scenarios + touches CompositeAudioInsertTask
    (the DSP-12 surface). Routed at QA-Eg close via §9 Forks + new §5 batch
    per Rule 3; slot surfaced to Jeff at close-time.
  - **S3 = per-bus tasks** (FX → Clips → Vox+Vox2 → Inst+Inst2+Inst3 → Rusty →
    cleanup). Each task individually verifiable by ear; clean rollback
    boundaries; matches `feedback_commit_at_checkpoints.md`. FX first because
    EffectsBusNode already G1-shaped (smallest scope to validate the pattern).
  - **S4 = publishPeakReading() with the per-block latency-comp ring buffer**
    (not the simpler CAS-max). Whole point of standardizing is uniformity
    across all 11 buses; simpler approach would re-introduce a subtle ballistics
    split.
  - **S5 = silly-name `squishy-scribbling-flurry`** (my pick per
    `feedback_silly_name_is_my_pick.md`; runtime-assigned by plan-mode).
- **Plan structure.** 9 tasks total: Task 0 open commit (this entry) + Task 1
  read-only pre-flight inventory + Tasks 2-6 per-bus migrations + Task 7
  infrastructure cleanup + comment sweep + final stress + Task 8 close
  sequence. Plan file written to `~/.claude/plans/squishy-scribbling-flurry.md`,
  approved via ExitPlanMode, mirrored to `Plans & Specs/Batch Plans/squishy-
  scribbling-flurry.md`, home-dir copy deleted per `feedback_plan_mirror_one_
  way.md`.
- **Files changed in Task 0 (docs-only, no source touched):**
  - `Plans & Specs/Batch Plans/squishy-scribbling-flurry.md` (new — the plan).
  - `Plans & Specs/Running Notes/squishy-scribbling-flurry.md` (new — this file).
  - `Plans & Specs/Main Plan.md` (1 line edit — §5 QA-Eg `**Plan file:**`
    pointer flipped from `` `<silly-name>.md (when started)` `` to the
    backticked-path form, matching sibling §5 entries).
- **Risk this commit:** none — docs-only. Source work begins at Task 1
  (read-only inventory) then Task 2 (FX bus migration, first source touch).

## 2026-05-23 — Task 0 — Open commit landed (`6594b3a`)

- **Commit:** `6594b3a` (top of `main`, 4 ahead of `origin/main`). Docs-only —
  3 files, 374 insertions / 1 deletion.
- **Files in commit:** `Plans & Specs/Batch Plans/squishy-scribbling-flurry.md`
  (new — the plan), `Plans & Specs/Running Notes/squishy-scribbling-flurry.md`
  (new — this file), `Plans & Specs/Main Plan.md` (1 line — §5 QA-Eg
  `**Plan file:**` pointer flip).
- **Pre-commit surface:** drafted commit message + full `git status` surfaced
  to Jeff per `feedback_surface_drafted_commit_message_for_approval.md`;
  disposition table covered all 4 entries (3 staged + 1 untracked-not-staged);
  Jeff approved before `git commit` ran.
- **Working tree post-commit:** clean of source changes. Only untracked is
  `Templates/My Templates/` (Jeff's local verify-test asset directory,
  intentionally NOT staged per
  `feedback_surface_full_git_status_before_commit.md` — carry-forward from
  QA-Ef close NIT-v, unchanged this batch).
- **Build state:** N/A — docs-only commit, no source touched, no build run.
- **Next action.** Task 1 — read-only pre-flight inventory. Re-verify the 8 G2
  buses against current post-QA-Ef source (re-measure the `*Run` mirror list +
  `processBus` CAS-max call sites + `drainAndMerge` promotion path +
  `publishPeakReading` ring-buffer signature L/B/D/Master share). Surface
  inventory to Jeff in plain English before any source touch in Task 2.

## 2026-05-23 — Task 1 — Pre-flight inventory (read-only) + S6 surfaced

- **Method.** Read-only Task 1 per the plan — direct reads of all 5 G1 BusNode
  struct definitions + the universal publish helper + every consumer site of
  the registration / mirror / drain infrastructure against current post-QA-Ef
  source (no source edits, no commit). Line numbers below are current and
  confirm the plan's deletion + addition map.

#### Plan assumptions validated

- **Uniform G1 peak field set.** All 5 G1 BusNodes carry an IDENTICAL field
  list: `peakDb / peakDbL / peakDbR` atomics + `peakRingL / peakRingR` arrays +
  `peakRingIdx int`. LayersBusNode ([VibeGraph.cpp:217](Source/VibeGraph.cpp:217)),
  BassBusNode ([:420](Source/VibeGraph.cpp:420)), DrumsBusNode
  ([:587](Source/VibeGraph.cpp:587)), MasterBusNode ([:752](Source/VibeGraph.cpp:752)),
  EffectsBusNode ([:901-1042](Source/VibeGraph.cpp:901)). Task 3 InstrChannelNode
  extension copies this exact pattern — no invention.
- **publishPeakReading is the universal publish helper.** Defined
  [VibeGraph.cpp:91](Source/VibeGraph.cpp:91); 5 G1 call-sites at `:413, :580,
  :743, :885, :1038` — each at the END of its respective BusNode processBlock
  with the canonical 7-arg signature `(buf, peakRingL, peakRingR, peakRingIdx,
  peakDbL, peakDbR, peakDb)`. Tasks 3-6 add the same call to processBus's
  generic-bus section pointing at the new InstrChannelNode fields.
- **FX exchange-store insertion point confirmed.** Right after
  `processEffectsBus(buf, bpm, anyBus, panLaw)` returns in processBus's `kFxBus`
  dispatch ([:1531](Source/VibeGraph.cpp:1531)), BEFORE the `return;`. Parallel
  to L/B/D pattern at `:1545-1547 / :1554-1556 / :1563-1565`.
- **Cleanup infrastructure is fully self-contained.** `registerBusPeakAtomics`
  has 7 callers (all [PluginProcessor.cpp:288-315](Source/PluginProcessor.cpp:288));
  `mBusPeakRefs` used ONLY by `registerBusPeakAtomics` + the CAS-max site in
  `processBus` ([:1687](Source/VibeGraph.cpp:1687)); `drainEffectsBusPeakDbStereo`
  has exactly 1 caller ([PluginProcessor.cpp:2120](Source/PluginProcessor.cpp:2120)
  — the QA-Ef interim block Task 2 deletes). NO consumers outside PluginProcessor
  + VibeGraph. Task 7's wholesale delete is safe.
- **No surprises.** The deletion + addition map in the plan matches reality.

#### Side finding — S6 (dead field across all 5 G1 BusNodes)

- **`peakDecayDbPerBlock` is DEAD STATE on every G1 BusNode.** Initialised +
  recalculated in `prepare()` (Layers `:233 / :293`, Bass `:435 / :488`, Drums
  `:602 / :653`, Master `:766 / :816`, EffectsBus `:916 / :954`) but NEVER read
  on any BusNode. InsertNode has its own copy at `:1095 / :1134 / :1261-1262`
  that IS used — separate field. Dead carry-over from pre-2026-05-02
  meter-ballistics model that the lock-free `publishPeakReading` rewrite
  obsoleted.

#### S6 resolution (Jeff, 2026-05-23)

- **Skip the dead field on the new InstrChannelNode** (Task 3 plan note updated).
- **Also delete the dead field + its prepare-time recalc lines from the 5
  existing G1 BusNodes as part of Task 7 cleanup** (Task 7 plan step inserted).
- **Net post-batch:** all 11 buses uniform with no dead carry-over.

#### Plan file edits (3 targeted Edits per `feedback_targeted_edits_not_wholesale_rewrite.md`)

1. New row **S6** added to "Spec calls already locked" table — records
   dead-field decision + reasoning.
2. Task 3 InstrChannelNode-extension code block updated — comment explicitly
   notes the field set EXCLUDES `peakDecayDbPerBlock` per S6.
3. New step inserted at the TOP of Task 7's checklist — "S6 dead-field cleanup"
   with file:line references for all 10 deletion points (5 nodes x 2 sites
   each: declaration + prepare-time recalc).

- **Source / commit / build.** None this task — Task 1 is read-only by design.
  No diagnostic instrumentation added.
- **Next action.** Surface to Jeff for green-light on Task 2 — FX bus migration
  (first source touch): add `fxBusPeakDb*` VibeGraph members, add the 3-line
  exchange-store in processBus's `kFxBus` dispatch (post-`processEffectsBus`,
  pre-`return`), add 3 `drainAndMerge` lines to the G1 loop in
  `drainMeterAtomicsForUI`, delete the QA-Ef interim FX-bus drain block at
  `PluginProcessor.cpp:2115-2133`, delete `mFxBusPeakDb*Run` declarations +
  initialisers.

## Diagnostic Instrumentation Catalog

(per Main Plan §0 Rule 4 — append a row WITH the diagnostic add, walk + strip
at task/batch close. Format: Site / Tag / Purpose / Disposition.)

No diagnostics added yet (Task 0 = docs-only, no source touched).

**Pre-existing Keep entries** (retro-added per Rule 4 "pre-existing diagnostics
get retro-added with Keep when first surfaced"):

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| `PluginProcessor.cpp:1857-1885` (DSP-load meter, MT-Md hamburger readout) | n/a | "Run MT Diagnostic" + DSP-load smoothing/cap | Keep (V1 release fixture; surfaces under the Mixer hamburger). |
| `RenderEngine::gMultiThreadedEngineEnabled` toggle | n/a | Multi-core Rendering ON/OFF runtime gate | Keep (Production toggle, persisted to settings.xml; ON = parallel render, OFF = serial-diagnostic mode with workers parked). |
