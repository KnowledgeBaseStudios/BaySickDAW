# Running Notes — QA-Ea (polished-snuggling-token)

> **Purpose.** Append-only mid-batch log of what was done, what was found, what
> was decided, and what was deferred during QA-Ea execution.  Compiled from
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
> **Pair file:** `Plans & Specs/Batch Plans/polished-snuggling-token.md`
> (the per-batch plan).
>
> **Convention:** see `Plans & Specs/Main Plan.md` §0 (folder-scope rule +
> Agent Orchestration Rules' mid-batch checkpoint trigger + plan/running-notes
> required sections, locked 2026-05-11).  Running-notes subfolder convention
> established 2026-05-09 mid-QA-A.

---

## 2026-05-17 — Task 0 — open

**Batch.** QA-Ea — Bus Solo + Layers/Bass/Drums Output-Path Unification (DSP-09).
Highest-risk Phase-3 batch: hot-path audio-engine refactor. Owner-locked order
**Part B first, then Part A**. Next batch after QA-E (closed 2026-05-17,
`f903eaa`); §6 arrow `QA-E → QA-Ea → QA-Eb → QA-F`.

**Spec calls resolved this session (pre-plan).**
- SC1: Part B (output-path unification) before Part A (single solo gate).
- SC2: all 11 buses uniformly soloable (layers/bass/drums/fx/clipsbus/voxbus/
  instbus/voxbus2/instbus2/instbus3/rustybus) — soloing any silences all
  others (subject to B1). Intended changes: RustyDrumsBus gains a solo gate;
  triad joins the unified set; ClipsBus moves from bespoke 6-bus to uniform 11.
- SC3: add an explicit serial↔MT bit-parity verify gate (existing Mixer
  hamburger "Multi-core Rendering" toggle + Render-to-WAV, metronome OFF),
  alongside the no-solo bit-compare + 5 DSP-09 scenarios.
- Sub-calls A/B1/C/D + GUARDRAIL: §9 nineteenth Forks (Jeff-locked, not
  re-litigated).

**Findings / corrections during open.**
- CLAUDE.md "MT engine in Debug = no-op" bullet was STALE (QA-Md closed
  2026-05-09; the "no-op" was a DSP-meter-cap-saturation misdiagnosis; MT
  works in Debug). Removed at Jeff's instruction (CLAUDE.md:95). Memory
  `project_mt_engine_works_in_debug.md` saved. SC3's framing corrected
  accordingly (serial↔MT parity runs in the normal Debug+Release cycle, not
  Release-only). CLAUDE.md:351 stale "Next batch: QA-Md" flagged to Jeff (not
  edited — plan-state, awaiting his call on a broader Next-Steps pass).
- Initial plan draft did not follow the Main Plan §0 batch-plan required-
  sections rule (wrote prose summary, then inferred structure from the last
  batch). Corrected: read §0:218-241 (locked 2026-05-11, exemplar
  federated-bouncing-cupcake.md/QA-D); plan rewritten to conform. Memory
  `feedback_batch_plan_structure_follows_s0_rule.md` saved.

**OQ-1 resolved (code read).** `processBus` returns early for the triad at
VibeGraph.cpp:1642/1651/1660; the shared formula at :1775 is receive-group-
only; triad solo lives in `BusNode::processChainOnly` :355-364. Task 4 = two
concrete edit sites, no branching.

**Task 0 actions.**
- Plan mirrored to `Plans & Specs/Batch Plans/polished-snuggling-token.md`;
  home-dir copy deleted (one-copy hygiene, `feedback_plan_mirror_one_way.md`).
- Main Plan §5 QA-Ea entry: `**Plan file:**` pointer added (backticked-path
  form, matching existing §5 entries' convention).
- This running-notes file seeded.
- Golden-capture instructions issued to Jeff (serial + MT WAV of a
  deterministic test pattern, metronome OFF). **Deferred to the Task-1
  checkpoint** — the Task-0 open commit is docs/scaffolding only and is not
  blocked on a manual build+render; golden hashes + baseline delta get
  recorded in running notes before Task-1 verify. (Deliberate deviation from
  the plan's Task-0 checklist order; reason: don't gate a scaffolding commit
  on Jeff's build cycle.)
- **CLAUDE.md disposition (Jeff's call): fold the stale MT-bullet removal
  INTO the Task-0 open commit** (not a separate commit). One open commit:
  plan mirror + §5 pointer + running-notes seed + CLAUDE.md:95 removal.
- `pre-QA-Ea` git tag recommended at the Task-0 commit (rollback boundary).

**Status:** Task 0 open in progress; golden capture deferred to Task-1
checkpoint; Task-0 open commit pending `/draft-commit` + Jeff approval. No
source changed.
