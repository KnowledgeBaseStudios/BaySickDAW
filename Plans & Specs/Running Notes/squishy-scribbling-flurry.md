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
