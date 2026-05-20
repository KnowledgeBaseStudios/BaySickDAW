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

### 2026-05-17 — Task 0 — open commit landed (`7cdc59c`)

Follow-on to the `## 2026-05-17 — Task 0 — open` block above. The Task-0 open
commit is on `main` and the working tree is clean. No source changed in this
batch yet.

**Commit landed.** `7cdc59c` — "QA-Ea Task 0 (batch open): plan + running-notes
scaffold + Main Plan pointer + stale CLAUDE.md MT-Debug bullet removal." 4 files,
+424/-1. Docs/scaffolding only — zero source edits.

- NEW `Plans & Specs/Batch Plans/polished-snuggling-token.md` — per-batch plan,
  conformant to Main Plan §0:218-241 batch-plan required-sections rule.
- NEW `Plans & Specs/Running Notes/polished-snuggling-token.md` — this file
  (seeded with the Task-0 open block).
- `Plans & Specs/Main Plan.md` +2 — §5 QA-Ea `**Plan file:**` pointer
  (backticked-path form, matching existing §5 entries).
- `CLAUDE.md` -1 — stale "MT engine in Debug = no-op" bullet (CLAUDE.md:95)
  removed. Folded INTO this open commit per Jeff's explicit disposition call
  (not a separate commit), as recorded in the Task-0 open block.

**Rollback boundary.** Git tag `pre-QA-Ea` created at `7cdc59c` — the Part-B
rollback boundary. Confirmed `7cdc59c` is the correct pre-Part-B baseline: it
contains no QA-Ea source changes, so a golden capture taken against current
`main` now is a true pre-Part-B reference.

**Process compliance this checkpoint.**
- Every-commit-via-`/draft-commit` honored (`feedback_every_commit_via_draft_commit.md`).
- Full git status surfaced + explicit Jeff approval obtained before `git commit`
  (`feedback_surface_full_git_status_before_commit.md` +
  `feedback_surface_drafted_commit_message_for_approval.md`).
- CLAUDE.md fold-in disposition was a Jeff spec call, not a unilateral pick
  (`feedback_dont_make_unilateral_spec_calls.md`).
- Memory saved earlier this session: `feedback_batch_plan_structure_follows_s0_rule.md`,
  `feedback_read_governing_docs_yourself.md`, `project_mt_engine_works_in_debug.md`.
- `Files For Claude/batch_session_boilerplate.md` updated (§0-full-self-read +
  /read-doc-bulk-only + CLAUDE.md-cross-check + plan-check-branch-removed).
  Gitignored — no commit; noted here for the close-entry trail.

**Golden capture — NOW DUE (was deferred from the Task-0 checklist).** The
Task-0 open block deferred golden capture so the scaffolding commit wasn't gated
on Jeff's build cycle. That deferral is now resolved: `7cdc59c` is the correct
pre-Part-B baseline (no QA-Ea source yet), so the capture must happen against
current `main` BEFORE any Task-1 source edit.

Jeff to:
1. Build current `main` (`do_build.bat`).
2. Render the deterministic QA-Ea test pattern twice — metronome OFF, no
   LCG-noise synths in the project:
   - Serial: Mixer hamburger "Multi-core Rendering" OFF → `golden_serial_preB.wav`.
   - MT: "Multi-core Rendering" ON → `golden_mt_preB.wav`.
3. Send SHA256 of both WAVs.

Parent then records both hashes here + the serial-vs-MT baseline delta (expected:
bit-identical, or the documented pre-existing delta if any) before Task 1 code
work begins. Hashes pending — `<TBD — fill in after Jeff sends>`.

**Status:** Task-0 open commit landed (`7cdc59c`); `pre-QA-Ea` tag set; working
tree clean. **Resume action: golden capture (serial + MT) against `7cdc59c`
`main` is the explicit blocker before any Task-1 source edit.** Record hashes +
baseline delta in this file, then Task 1 proceeds.

### 2026-05-17 — Task 0 — verification methodology pivot (Jeff-directed)

Follow-on to the `### 2026-05-17 — Task 0 — open commit landed (`7cdc59c`)`
sub-block above. **Supersedes the golden-WAV / bit-compare verification
approach recorded there.** No source code changed in this batch yet; this
checkpoint is a verification-methodology change only. Task 0 still open.

**Why the pivot.** The plan's original verification design (fabricated
`golden_serial_preB.wav` / `golden_mt_preB.wav`, `fc /b` binary diff,
`certutil`/SHA256 hashing, an explicit serial↔MT bit-parity gate — see the
prior sub-block's "Golden capture" steps + SC3) was scrapped at Jeff's
direction. Two faults:
1. **Ungrounded.** Invented filenames + CLI hashing is not how Jeff
   verifies — he doesn't code; he verifies by ear / in-app.
2. **False premise.** Multithreaded render reorders float summation, so
   serial vs MT is *never* bit-identical even with zero behavior change.
   A "serial↔MT bit-parity gate" therefore cannot exist — the SC3 gate as
   written was unsatisfiable. The prior sub-block's "expected: bit-identical"
   baseline-delta note is withdrawn for the same reason.

**Replacement: in-app null test.** Grounded entirely in features verified
in code this session.
- **Source anchor.** A fixed pre-recorded song dropped on the Builder grid
  — bit-exact every play (no RNG / voice / MIDI variance).
- **Capture.** Record with nothing armed → captures the master bus to a
  WAV (`masterFile`; "only when no strips armed" —
  [PluginProcessor.h:670](Source/PluginProcessor.h:670) /
  [PluginProcessor.cpp:3555](Source/PluginProcessor.cpp:3555); auto-drops
  the result on the grid).
- **Compare.** Capture master "before" (pre-Part-B, tagged `pre-QA-Ea` /
  `7cdc59c`) vs "after"; put both clips on the grid, flip the existing
  per-strip polarity ("Reverse") toggle on one, play together. Dead
  silence = Part B changed nothing; audible residual = real regression.
  Benign sub-audible block-size hiss is acceptable; loud/obvious residual
  blocks the gate.
- **Serial vs MT** = a by-ear toggle of the Mixer "Multi-core Rendering"
  hamburger item (must sound right both ways), **never** a bit-compare.
- **Part A** = by ear: the 5 DSP-09 scenarios + GUARDRAIL + B1 +
  Rusty-new-gate.

The "before" WAV artifact itself is now the null reference. **No hashes
are recorded or pending** — the prior sub-block's `<TBD — fill in after
Jeff sends>` hash placeholders are obsolete and not carried forward.

**Test-project spec finalized.** An 8-bar loop containing:
- **Layers + Bass + Drums engine parts** (sampled / phase-reset patches;
  **NO noise-oscillator patches** per the CLAUDE.md LCG-overflow gotcha).
  Required because the routing dropdown is **Vox/Inst/Clips-only**
  (verified [BuilderPage.cpp:2892-2932](Source/Standalone/BuilderPage.cpp:2892)),
  so a dropped song cannot be routed through the L/B/D buses — real engine
  content is the only way to exercise the exact bus→master paths Part B
  rewrites.
- **Vox + Inst + Rusty parts** — needed only for Part A solo listening,
  **not** for the Part B null (Part B does not touch those buses).
- The dropped song anchor.
- Metronome OFF.

**Scope clarification captured this session.** Part B changes ONLY the
Layers/Bass/Drums bus→master path + the serial Clips-routing gap (Q2).
Vox/Inst/FX/Rusty buses already use the unified
`routeInsertOutput`→`kMaster` path and are NOT modified by Part B. This is
why the Part B null only needs L/B/D content.

**Plan-file correction applied (targeted Edits, not a rewrite).** An
earlier wholesale `Write` of the plan file was rejected by Jeff; the
correction was redone surgically via targeted Edits to
`Plans & Specs/Batch Plans/polished-snuggling-token.md` — SC3 row, Task-0
baseline-capture step, Tasks 1/2/3/4/5/6 "Tell Jeff" scripts, Part B GATE,
Verification section. 79 lines changed (37+/42−) against ~345 untouched.
The SC3 row keeps a provenance note recording why the bit-compare ceremony
was dropped, so it is not reintroduced. **Plan correction is uncommitted**
— it will be surfaced in the next commit's git status per the
surface-full-status discipline.

**Process compliance this checkpoint.**
- SC3 method change was Jeff-directed, not a unilateral spec change
  (`feedback_dont_make_unilateral_spec_calls.md`).
- Plan corrected via targeted Edits after a wholesale `Write` was rejected;
  new memory `feedback_targeted_edits_not_wholesale_rewrite.md` saved
  (+ index).
- Memory saved earlier this session:
  `feedback_read_governing_docs_yourself.md`,
  `feedback_batch_plan_structure_follows_s0_rule.md`,
  `project_mt_engine_works_in_debug.md`.

**Open consistency item (surfaced to Jeff, not yet decided).** Main Plan §5
QA-Ea verify line + §9 nineteenth Forks entry both say "bit-compare a
no-solo render before/after". The in-app polarity-null test satisfies that
*intent* (before/after sample-domain equivalence) but the literal
"bit-compare" wording is now method-inaccurate. Pending Jeff: treat as
in-batch execution detail (no Main Plan edit; batch plan + these notes are
the record) vs a §5 wording tweak + §9 Rule-3 back-ref.

**Status:** Task 0 still open; verification method now grounded
(in-app null test, no hashes); no source changed; plan correction
uncommitted. **Resume action: Jeff builds the 8-bar test project +
captures the Part-B "before" master WAV (record-nothing-armed) on
`pre-QA-Ea` / `7cdc59c`. That capture is the hard blocker before any
Task-1 source edit** (replaces — and remains as strict as — the prior
sub-block's golden-capture blocker).

### 2026-05-17 — Task 0 — QA-Ec routed out (Main Plan updated)

Follow-on to the `### 2026-05-17 — Task 0 — verification methodology pivot
(Jeff-directed)` sub-block above. **No QA-Ea source code changed this
checkpoint** — this is a Rule-3 finding-routing event plus the Main Plan
edits that applied it. Task 0 still open.

**What surfaced.** While building the QA-Ea deterministic test rig, three
bugs were found. The first ("Issue 1") is the audio-clip Resample / Stretch
"follow tempo / fit to grid" path being a non-functional shell:
- Abandoned Rubber Band stub at
  [BuilderPage.cpp:4107-4111](Source/Standalone/BuilderPage.cpp:4107).
- Resample-follow has **no code path** at all.
- Stretch engages only on a condition that is never true.
- Hardcoded-120 import default + the
  [PluginProcessor.cpp:533](Source/PluginProcessor.cpp:533)
  `outSamples <= 0` guard → clip goes silent on a project-BPM change.

**Verified in code, not assumed.** `PhaseVocoder` + clip persistence
already exist in the codebase. Issue 1 is therefore a **wiring gap, not a
missing engine** — the time-stretch DSP and the save/load state are present;
nothing connects them through the follow-tempo / fit-to-grid UI path. This
verification is why the routing is "build-out", not "new engine".

**Rule-3 routing decision (Jeff's call).** Per §0 Rule 3 ("no surface match
→ new dedicated §5 batch row + §9 Forks entry"), Issue 1 becomes its own
**new independent batch QA-Ec**. Jeff-confirmed slot = Option 1:
`QA-E → QA-Ea → QA-Eb → QA-Ec → QA-F`.
- QA-Ec is sequenced **before QA-F** so clip stretch/resample is real for
  QA-F vocals + along-the-way testing.
- QA-Ea is **not hard-blocked** by QA-Ec: the null-test anchor wants zero
  time-stretch anyway, so QA-Ea proceeds now with a no-stretch
  deterministic anchor.
- This is **not** a carve-out and does **not** touch QA-Ea / QA-Eb scope.
- The song-mode pattern-scheduler bugs ("Issues 2 & 3") are explicitly
  **NOT** folded into QA-Ec — separate fix (see Resume action below).

**Main Plan edits applied this checkpoint** (four targeted Edits; anchors
verified by direct read first; **not** a wholesale rewrite, per
`feedback_targeted_edits_not_wholesale_rewrite.md`):
- §9 twenty-third Forks entry appended —
  `### 2026-05-17 — QA-Ec inserted: audio-clip Resample/Stretch
  follow-tempo/fit-to-grid build-out (new independent batch)`.
- §5 QA-Ec batch entry inserted between QA-Eb and QA-F.
- §6 arrow updated to `... → QA-Eb********** → QA-Ec*********** → QA-F`.
- §6 QA-Ec footnote (11 asterisks) inserted after the QA-Eb footnote.

**Two Jeff decisions baked into the applied text** (drafter flagged both;
parent surfaced both; Jeff confirmed):
- **Bucket = "System Pages, Cross-cutting Infrastructure"** (both, per the
  canonical-no-eliding rule `feedback_canonical_structure_no_eliding.md`):
  QA-Ec touches `BuilderPage` (System Pages) **and** the
  `PluginProcessor` hot-path render (Cross-cutting Infrastructure).
- **§9 "Options considered" corrected** to record the REAL decision —
  Option 1 slot (`E→Ea→Eb→Ec→F`) vs Option 2 (`E→Ec→Ea→Eb→F`) vs other —
  replacing the drafter's invented fold-into-Eb / fold-into-F /
  Phase-6-punt alternatives, which were not the alternatives Jeff actually
  weighed.

**Process compliance this checkpoint.**
- Drafter-only honored: doc-drafter proposed; parent surfaced both flagged
  decisions to Jeff; Jeff confirmed bucket + chose "apply now"; parent
  verified anchors then applied via targeted Edits.
- Slot/sequencing was Jeff's call, not a unilateral pick
  (`feedback_slot_placement_is_spec_call.md`).
- Memory saved earlier this session:
  `feedback_targeted_edits_not_wholesale_rewrite.md`,
  `feedback_read_governing_docs_yourself.md`,
  `feedback_batch_plan_structure_follows_s0_rule.md`,
  `project_mt_engine_works_in_debug.md`.

**Uncommitted state (for the next commit's surface-status step).** Dirty /
uncommitted: `Plans & Specs/Main Plan.md` (the four QA-Ec Edits), the QA-Ea
plan file `Plans & Specs/Batch Plans/polished-snuggling-token.md` (earlier
verification-methodology correction, still uncommitted from the prior
sub-block), and this running-notes file. **No commit performed this
checkpoint** — all three to be surfaced in the next commit's full git
status per `feedback_surface_full_git_status_before_commit.md`.

**Status:** Task 0 still open; QA-Ec routed out + Main Plan updated
(§5/§6/§9, four targeted Edits applied); no QA-Ea source changed; Main
Plan + QA-Ea plan file + this file uncommitted. **Resume action:**
(1) Issues 2 & 3 (song-mode pattern-scheduler viewport + intermittent
first-note-drop, same scheduler code region; Jeff authorized combining,
"after the Main Plan slot") — surface the exact before/after diff for
Jeff's approval, then implement. (2) Then resume QA-Ea Task 0: build the
no-stretch deterministic test rig + capture the Part-B "before" master WAV
(record-nothing-armed) on `pre-QA-Ea` / `7cdc59c`. QA-Ea Task 0 remains
open; no source changed.

### 2026-05-18 — Task 0 — Strip-restore guard + Issue 2 verified-fixed (pending commit-surface)

Follow-on to the `### 2026-05-17 — Task 0 — QA-Ec routed out (Main Plan
updated)` sub-block above. **The prior block's Resume-action item (1)
("Issues 2 & 3 combined, fix after the Main Plan slot") was superseded by a
Jeff spec call:** Issue 3 (intermittent first-note-drop / transport timing)
was **decoupled into its own scoped batch QA-Ed** (integer-sample transport
rework) and is NOT fixed here. Only **Issue 2** (pattern viewport / no
re-loop) plus the **QA-E strip-restore regression guard** were implemented +
verified this checkpoint. This is QA-E-region carry-forward (strip-restore
guard + Issue 2), not QA-Ea Part B; QA-Ea Part-B scope still untouched.
Task 0 still open.

**What was fixed.**
- **QA-E strip-restore regression guard.** Range-aware vox/inst guard at
  [StandaloneEditor.cpp:10314](Source/Standalone/StandaloneEditor.cpp:10314).
  Owner-confirmed build + verified.
- **Issue 2 (pattern viewport / no re-loop).** Song-mode pattern-scheduler
  viewport + re-loop fix in `PluginProcessor.cpp` `scheduleRoll`.
  Owner-confirmed build + verified.

Both build + verified by owner (Debug + Release per owner's standing
per-task cycle — no separate close re-verify gate,
`feedback_no_full_release_reverify_at_batch_close.md`).

**Process compliance this checkpoint.**
- Diff was surfaced for owner approval before implement per the prior
  sub-block's Resume action; owner authorized Issue 2 + the strip-restore
  guard, and made the explicit spec call to decouple Issue 3 → QA-Ed.
- QA-batch-fixes-don't-defer honored for Issue 2 (fixed in the open batch).
  Issue 3's deferral is **not a punt** — it is a Jeff spec call with
  explicit justification (own scoped transport-rework batch QA-Ed), the
  sanctioned form of deferral per
  `feedback_qa_batches_fix_bugs_dont_defer.md`.
- These are QA-E-region carry-forward fixes landing in the open QA-Ea
  batch; the §9 Forks back-ref disposition for the closed-batch
  carry-forward is an open consistency item carried to the next checkpoint
  (`feedback_closed_batch_carryforward_via_forks.md`).

**Uncommitted state (for the next commit's surface-status step).** Dirty /
uncommitted now includes the two source fixes above
(`Source/Standalone/StandaloneEditor.cpp`, `Source/PluginProcessor.cpp`)
**plus** the still-uncommitted docs from prior sub-blocks: `Plans & Specs/
Main Plan.md` (QA-Ec Edits), `Plans & Specs/Batch Plans/
polished-snuggling-token.md` (verification-methodology correction), this
running-notes file, and the untracked `qae_t9_dirty_trace.txt`. **No commit
performed this checkpoint** — all to be surfaced in the next commit's full
git status per `feedback_surface_full_git_status_before_commit.md`; commit
message via `/draft-commit` + owner approval before `git commit`.

**Status:** strip-restore guard + Issue 2 verified-fixed; Issue 3 decoupled
to QA-Ed (Jeff spec call). Pending commit-surface for owner approval. QA-Ea
Part-B scope still untouched. **Resume action:** surface the drafted commit
message + full pre-commit git status for owner approval; then proceed to the
MT-record-master diagnosis (next sub-block).

### 2026-05-18 — Task 0 — MT master-recorder bug diagnosed + serial-tail divergence audit

Follow-on to the `### 2026-05-18 — Task 0 — Strip-restore guard + Issue 2
verified-fixed (pending commit-surface)` sub-block above. **No source code
changed this checkpoint** — this is a diagnosis + exhaustive
ST/MT-divergence audit plus the scope/sequencing spec calls owner made off
the findings. Task 0 still open. This thread directly unblocks QA-Ea Part-B
verification (the Part-B "before" master capture must work in MT).

#### Diagnosis thread (the audit + the 3-bug finding are one investigation)

**MT-record-master bug — root cause confirmed by code read, not assumed.**
Owner hit a 104-byte header-only WAV when recording the master in song mode
with nothing armed — **MT only**; serial (ST) records correctly. Verified
crux: `mMasterRecorder.writeBlock(buffer)` lives ONLY in the serial tail at
[PluginProcessor.cpp:2709-2710](Source/PluginProcessor.cpp:2709), which is
**after** the MT branch early-return at
[PluginProcessor.cpp:1932](Source/PluginProcessor.cpp:1932). MT's
`RenderGraphDispatcher::dispatchBlock` copies the arena `kMaster` slot into
the host buffer
([RenderGraphDispatcher.cpp:307-316](Source/Engine/RenderGraphDispatcher.cpp:307))
but never feeds the recorder.

Explicitly ruled out (verified in code, not speculation —
`feedback_diagnose_before_fixing.md`):
- **NOT a race.** Pure buffer-ownership gap.
- **NOT transport-shutdown-before-flush.** The stop gate
  (`mRequestStop` / songEnd,
  [PluginProcessor.cpp:1177-1182](Source/PluginProcessor.cpp:1177)) is in
  common code **before** the ST/MT split, and `AudioFileRecorder` is
  queue-backed on its own thread.

The feed was added to the serial tail and never mirrored into the MT branch
after `dispatchBlock`.

**Exhaustive serial-tail audit — full read of
[PluginProcessor.cpp:1933-2896](Source/PluginProcessor.cpp:1933), no
excerpting.** Complete closed ST/MT-divergence inventory:

- **Confirmed serial-only (real bugs, clustered 2697-2857):**
  - Master recorder — `mMasterRecorder.writeBlock`
    [PluginProcessor.cpp:2709-2710](Source/PluginProcessor.cpp:2709).
  - MIDI recorder — `mMidiRecorder.processBlock`
    [PluginProcessor.cpp:2697-2701](Source/PluginProcessor.cpp:2697).
  - Metronome + record count-in DSP —
    [PluginProcessor.cpp:2712-2857](Source/PluginProcessor.cpp:2712).
- **Verified mirrored or inert (NOT divergences):**
  - All engine render loops — mirrored via EngineInsertTask /
    VoxStripTask / InstStripTask / RustyDrumsProducerTask + RustyInsertTask /
    CompositeAudioInsertTask.
  - Audio-clip playback — shared `renderFilePlayPlayer` /
    `renderAudioClipsForRow` (called from both paths).
  - Per-strip armed recording — `tapDryRecorder` (the one recorder feed
    that WAS mirrored; called from Vox/InstStripTask).
  - All bus pipelines (Clips / Vox / Inst / Vox2 / Inst2 / Inst3 / FX /
    RustyDrums) — `PassiveStripTask` Kind::Bus →
    `processBus` / `processEffectsBus`.
  - Aux — `PassiveStripTask` Kind::Aux.
  - Master mixdown — `MasterTask` + arena.
  - `measureDspLoadAndOverload` + `drainMeterAtomicsForUI` — explicitly
    called in the MT branch
    ([PluginProcessor.cpp:1931](Source/PluginProcessor.cpp:1931) /
    [PluginProcessor.cpp:1921](Source/PluginProcessor.cpp:1921)).
  - `midiMessages.clear()` at
    [PluginProcessor.cpp:2884](Source/PluginProcessor.cpp:2884) — not
    mirrored but **inert in standalone**: the host supplies a fresh MIDI
    buffer per callback, and `allMidi` is a fresh copy at
    [PluginProcessor.cpp:1038](Source/PluginProcessor.cpp:1038) before the
    split.
- **Vestigial-in-both (not a divergence).** The NaN/Inf guards at
  [PluginProcessor.cpp:1977-1989](Source/PluginProcessor.cpp:1977) /
  [PluginProcessor.cpp:2015-2024](Source/PluginProcessor.cpp:2015) operate
  on `mLayerEngineSum` / `mBassEngineBuf`, which the code's own comment at
  [PluginProcessor.cpp:2682-2684](Source/PluginProcessor.cpp:2682) says are
  no longer consumed — dead in serial too, so not an ST/MT divergence.
  **Separate open question (logged, not in QA-Ea scope):** whether any
  master-output NaN guard exists at all — a potential WASAPI-silence hazard
  if a NaN reaches the master bus unguarded.

**Empirical confirmation status of the 3 serial-only bugs:**
- **Master recorder** — confirmed by owner earlier (the 104-byte WAV).
- **Metronome / count-in** — confirmed by owner by ear, 2026-05-18.
- **MIDI recorder** — accepted as fact (owner has no MIDI keyboard on hand)
  + code-confirmed by its serial-tail position (same post-split region as
  the other two, identical mirror gap).

#### Strategy / scope / sequencing thread (owner spec calls)

**Spec call — scope (owner): the 3-bug shared-helper fix folds INTO QA-Ea.**
It directly blocks QA-Ea Part-B verification — the master recorder must work
in MT to capture the Part-B "before" master (the in-app null reference
established in the verification-methodology pivot sub-block). The QA-Ea knot
is resolved: fix the master recorder in-batch, then Part-B verifies **in
MT**, not ST (revises the originally-planned ST verification path).
- **Fix shape (owner-confirmed direction).** Extract the post-mix tail
  (MIDI recorder + master recorder + metronome / count-in) into ONE shared
  helper called from BOTH the serial tail AND the MT branch after
  `dispatchBlock`. This is the **5th instance of the proven extract
  pattern** (prior 4: `tapDryRecorder`, `drainMeterAtomicsForUI`,
  `measureDspLoadAndOverload`, `renderFilePlayPlayer` /
  `renderAudioClipsForRow`). A shared helper makes this class of bug
  structurally un-divergeable.
- All 3 bugs fixed together via the one extraction (single un-mirrorable
  call site), per `feedback_qa_batches_fix_bugs_dont_defer.md`.

**Spec call — sequencing (owner): ST-path deletion = new batch QA-Ef.**
Gated on "MT proven on all 3" (master recorder + MIDI recorder +
metronome/count-in working in MT). New batch order:
**QA-E → QA-Ea → QA-Ed → QA-Eb → QA-Ec → QA-Ef → QA-F**
— QA-Ed moves up to immediately after QA-Ea; QA-Ef inserted before QA-F.
Strategic rationale (owner):
- Dual hand-maintained ST/MT parity **is the bug class** — proven leaked 3x
  (master recorder, MIDI recorder, metronome/count-in).
- Owner is sole coder + session-fog / context risk amplifies hand-mirror
  drift.
- Shared-helper extraction kills the class at the source.
- Physical ST deletion is the end state but a **deliberate gated batch**
  (hot-path rip-out, ~960-line serial tail), NOT rushed mid-QA.
- ST's only enduring value (serial-execution bisect for parallelism bugs)
  is preserved post-deletion by a **1-worker MT pool mode**, not a
  duplicate code path.
- Slot/sequencing was owner's call, not a unilateral pick
  (`feedback_slot_placement_is_spec_call.md`); the QA-Ec carve-out is
  unaffected (it stays before QA-F).

**Standing rule established (owner-confirmed direction).** Any new
audio-path code from now on is written as a **single shared helper called
from both the serial tail and the MT branch — never hand-mirrored**. This
is the durable root-cause fix for the session-fog dual-path divergence
class (cross-refs `feedback_own_the_codebase_no_git_alibi.md` —
divergence is owned + designed out, not attributed to sessions).

**Process compliance this checkpoint.**
- Diagnose-before-fixing honored — root cause confirmed by full code read
  (serial tail + dispatcher) before any fix shape was proposed; all "not a
  race / not a shutdown" alternatives ruled out in code
  (`feedback_diagnose_before_fixing.md`).
- Read code before calling anything expected — the full
  [PluginProcessor.cpp:1933-2896](Source/PluginProcessor.cpp:1933) read
  backs every "mirrored / inert / vestigial" classification
  (`feedback_check_code_before_calling_it_expected.md`).
- QA-batch-fixes-don't-defer — the 3-bug fix folds INTO QA-Ea, not punted
  (`feedback_qa_batches_fix_bugs_dont_defer.md`).
- Scope + slot/sequencing were owner spec calls, surfaced and decided by
  owner, not unilateral (`feedback_dont_make_unilateral_spec_calls.md`,
  `feedback_slot_placement_is_spec_call.md`).

**Uncommitted state (for the next commit's surface-status step).**
Unchanged from the prior sub-block — no source changed this checkpoint.
Still dirty / uncommitted: the two Issue-2 + strip-restore source fixes
(`Source/Standalone/StandaloneEditor.cpp`, `Source/PluginProcessor.cpp`),
`Plans & Specs/Main Plan.md` (QA-Ec Edits), `Plans & Specs/Batch Plans/
polished-snuggling-token.md`, this running-notes file, and untracked
`qae_t9_dirty_trace.txt`. **No commit performed this checkpoint.**

**Plan / Main Plan edits NOT YET applied (carry-forward for parent).** The
new sequencing (QA-Ef batch + QA-Ed move-up: `QA-E → QA-Ea → QA-Ed →
QA-Eb → QA-Ec → QA-Ef → QA-F`) requires Main Plan §5 (QA-Ef new entry +
QA-Ed re-slot), §6 (arrow + footnotes), §9 (Rule-3 Forks entry for the
MT-divergence finding + QA-Ef insertion, back-ref QA-Ea), and a QA-Ea
plan-file scope note (3-bug shared-helper fix folded in, Part-B verifies
in MT). These are **not applied yet** — they are the explicit next
documentation action and go through targeted Edits + owner-confirmed
buckets, per `feedback_targeted_edits_not_wholesale_rewrite.md` /
`feedback_dont_make_unilateral_spec_calls.md`.

**Status:** Task 0 still open. MT master-recorder bug root-caused;
exhaustive serial-tail divergence audit complete (3 confirmed serial-only
bugs, everything else mirrored/inert/vestigial); scope (3-bug shared-helper
fix folds into QA-Ea, Part-B verifies in MT) and sequencing (ST deletion =
new QA-Ef, gated on "MT proven on all 3"; order `QA-E → QA-Ea → QA-Ed →
QA-Eb → QA-Ec → QA-Ef → QA-F`) decided by owner. No source changed this
checkpoint. **Resume action:** (1) surface the drafted commit message +
full pre-commit git status (Issue 2 + strip-restore source + all carried
docs) for owner approval, then commit. (2) Apply the Main Plan §5/§6/§9 +
QA-Ea plan-file edits for QA-Ef / QA-Ed re-slot / MT-divergence Forks entry
(targeted Edits, owner-confirmed buckets). (3) Then implement the
post-mix-tail shared-helper extraction (MIDI recorder + master recorder +
metronome/count-in, 5th extract-pattern instance) inside QA-Ea. (4) Then
resume the QA-Ea Part-B "before" master capture — now in MT, once the
recorder works there.

### 2026-05-18 — Task 0 — QA-E carry-forward + MT-divergence reorg committed (`f59cd22` + `8af4205`)

Follow-on to the `### 2026-05-18 — Task 0 — MT master-recorder bug diagnosed +
serial-tail divergence audit` sub-block above. **The prior block's Resume
actions (1) and (2) are now complete:** both pending commits landed and the
full Main Plan / QA-Ea plan-file reorg was applied. This is a tight
completion stamp — the MT-divergence diagnosis, serial-tail audit, and
scope/sequencing spec calls are fully captured above and are not re-narrated.
No QA-Ea Part-B source touched. Task 0 still open.

**Commit #1 landed — `f59cd22`.** QA-E-region carry-forward fixes + the
running-notes update. 3 files, +527/-29.
- QA-E strip-restore regression guard — range-aware vox/inst guard at
  [StandaloneEditor.cpp:10314](Source/Standalone/StandaloneEditor.cpp:10314).
- Issue 2 — pattern-clip viewport scheduler fix in
  `PluginProcessor.cpp` `scheduleRoll` (no re-loop / viewport).
- Both source fixes owner-verified Debug + Release (standing per-task
  cycle, no separate close re-verify gate,
  `feedback_no_full_release_reverify_at_batch_close.md`).

**Commit #2 landed — `8af4205`.** Doc-only reorg, zero source. 2 files,
+261/-49. Applied via targeted Edits with anchors verified by direct read
first (`feedback_targeted_edits_not_wholesale_rewrite.md`).
- `Plans & Specs/Main Plan.md`:
  - §9 twenty-fourth Forks — QA-E carry-forward record (back-ref QA-E;
    source = `f59cd22`).
  - §9 twenty-fifth Forks — MT serial-tail divergence: 3-bug fold into
    QA-Ea + QA-Ef created + QA-Ed created + reorder (back-ref QA-Ea).
  - §9 nineteenth-entry SUPERSEDED back-pointer added.
  - §5 — new QA-Ed + QA-Ef batch entries; **Bucket: Cross-cutting
    Infrastructure** for both (owner-confirmed call).
  - §5 QA-Ea Verify line corrected: bit-compare → in-app null test in MT;
    pre-Part-B 3-bug prerequisite note added.
  - §5 QA-Eb / QA-Ec sequencing strings updated to the new order.
  - §6 arrow rewritten `QA-E → QA-Ea → QA-Ed → QA-Eb → QA-Ec →
    QA-Ef → QA-F`; two new footnotes (QA-Ed 12-asterisk, QA-Ef
    13-asterisk).
- `Plans & Specs/Batch Plans/polished-snuggling-token.md` (QA-Ea plan):
  - Context: pre-Part-B-prerequisite paragraph added.
  - New locked spec-call row SC4.
  - New Task 0b in both `## Files to modify` and `## Tasks` — the 3-bug
    post-mix-tail shared-helper extraction.
  - Task 0 baseline-capture re-pointed to Task 0b / MT.
  - Verification #1 'before'-capture provenance corrected.
- **§5 batch-header structure verified intact post-edit** — each QA-E*
  header appears exactly once, in the correct order
  (`feedback_canonical_structure_no_eliding.md` discipline applied to the
  post-edit verify).

**Process compliance this checkpoint.**
- Both commits routed through `/draft-commit` and surfaced (drafted message
  + full pre-commit git status) for owner approval before `git commit`
  (`feedback_every_commit_via_draft_commit.md` +
  `feedback_surface_drafted_commit_message_for_approval.md` +
  `feedback_surface_full_git_status_before_commit.md`).
- **Correction (owner-caught, both commits).** Self-authored "tightened"
  alternatives were offered alongside the drafter output on BOTH commits.
  Corrected: the drafter output IS the commit message verbatim — the
  drafter exists for uniformity / git-log style match; review of drafter
  output is factual / scope-error only, never restyle. New memory
  `feedback_drafter_output_verbatim_no_restyle.md` saved + MEMORY.md index
  updated.
- §5 QA-Ed / QA-Ef bucket assignment was an owner spec call, not unilateral
  (`feedback_dont_make_unilateral_spec_calls.md`).
- All Main Plan / plan-file changes via targeted Edits, not a wholesale
  rewrite (`feedback_targeted_edits_not_wholesale_rewrite.md`).

**Uncommitted state.** Clean working tree — both commits landed; nothing
dirty. The prior sub-blocks' carried docs (Main Plan QA-Ec Edits, QA-Ea
plan-file verification-methodology correction, this running-notes file) are
now committed across `f59cd22` / `8af4205`; the prior `qae_t9_dirty_trace.txt`
note is no longer carried as a pending item.

**Status:** Task 0 still open. QA-E-region carry-forward fixes committed
(`f59cd22`); MT-divergence reorg fully applied + committed (`8af4205`,
Main Plan §5/§6/§9 + QA-Ea plan-file); §5 batch-header structure verified
intact; working tree clean; no QA-Ea Part-B source touched. **Resume
action:** implement **Task 0b** — the 3-bug post-mix-tail shared-helper
extraction in `PluginProcessor.cpp` / `.h` (extract MIDI recorder + master
recorder + metronome / count-in into ONE helper called from BOTH the serial
tail and the MT branch after `dispatchBlock`; preserve the D-5
pre-metronome recorder ordering — 5th instance of the proven extract
pattern). Surface the exact before/after to owner for approval before
editing the hot path; then owner builds + verifies all 3 in MT and ST;
then the Part-B 'before' capture in MT; then Part B Task 1.

### 2026-05-19 — Task 0b — CLOSED (3-bug post-mix-tail shared-helper extraction committed `f28319e`)

Follow-on to the `### 2026-05-18 — Task 0 — QA-E carry-forward + MT-divergence
reorg committed (`f59cd22` + `8af4205`)` sub-block above. **That block's
Resume action (implement Task 0b — the 3-bug post-mix-tail shared-helper
extraction) is now complete.** Tight CLOSE stamp — the MT-divergence
diagnosis, serial-tail audit, and scope/sequencing spec calls are fully
captured in the prior sub-blocks and are NOT re-narrated; factual delta
only. No QA-Ea Part-B source touched. QA-Ea Task 0 still open.

**Commit landed — `f28319e`.** Task 0b: the 3-bug post-mix-tail shared-helper
extraction. 6 files, +239/-55. Three fixes, all owner build + verified PASS
in BOTH MT and ST (master record + play-through song-end auto-stop,
metronome, count-in, hardware-MIDI record; ST showed no regression).

- **Fix A — post-mix tail extracted into one shared helper.** Master
  recorder + MIDI recorder + metronome / count-in lifted into
  `VibeSynthProcessor::applyPostMixRecordAndMetro`
  (`Source/PluginProcessor.cpp` / `Source/PluginProcessor.h`), called from
  BOTH the serial tail AND the MT branch after `dispatchBlock`. Closes the
  3 serial-only divergences (§9 twenty-fifth Forks / #25): MT no longer
  yields a 104-byte empty master WAV, and MT now captures MIDI + runs
  metronome / count-in. 5th instance of the proven extract pattern (prior 4:
  `tapDryRecorder`, `drainMeterAtomicsForUI`, `measureDspLoadAndOverload`,
  `renderFilePlayPlayer` / `renderAudioClipsForRow`).
- **Fix B — song-end auto-stop now finalizes the recording.** `onStop`
  teardown + halt extracted into shared
  `StandaloneEditor::stopTransportAndFinalizeRecording`, called from `onStop`
  AND the song-end `mRequestStop` auto-stop path. Owner-hit bug: play-through
  to song-end stopped playback but the recorder kept writing silence until a
  manual Stop — the auto-stop path was `stopPlayback`-only. Folded into
  Task 0b per owner (it blocked Task 0b verification).
- **Fix C — hardware-keyboard MIDI now reaches the MIDI recorder.** At the
  `mLiveMidiCollector` drain
  ([PluginProcessor.cpp:~1781](Source/PluginProcessor.cpp:1781)), `liveMidi`
  is now merged into `allMidi` so the MIDI recorder sees the hardware
  keyboard. Discovered during owner's keyboard test: the recorder reads
  `allMidi` (host-only, [PluginProcessor.cpp:1038](Source/PluginProcessor.cpp:1038));
  the hardware keyboard flows via `mLiveMidiCollector` and was never in
  `allMidi`, so hardware-MIDI recording NEVER worked in EITHER ST or MT —
  pre-existing, build-independent, NOT the MT-divergence and NOT a Task 0b
  regression. Double-trigger-safe: `allMidi`'s only real consumer is the
  recorder (`VibeGraph::processBlock` `ignoreUnused(midi)` at
  [VibeGraph.cpp:1545](Source/VibeGraph.cpp:1545); the MT dispatcher never
  receives `allMidi`); engines run off the per-page buffers. Folded into
  Task 0b per owner.

**Side findings logged in the QA-Ea plan-file Rule-3 Routing docket** (all
"owner picks slot at Task 7 close, not picked here" — surfaced for slot
decision, not unilaterally placed):
- **FND-1** — master recordings appear on the Builder grid but NOT in the
  Audio Clips browser; a phantom Clips strip exists (no Clips page); owner
  wants a "Master Recordings" section + open-in-default-player.
- **FND-2** — the 16-pad drum kit does not respond to MIDI-keyboard pads
  (live keyboard works for Harmless / Bass piano rolls).
- **FND-3** — velocity: the core MIDI↔internal formula already matches FL.
  Capture [MidiRecorder.cpp:25](Source/MidiRecorder.cpp:25) `getFloatVelocity`
  = MIDI/127; playback `emitPianoNoteOn`
  [PluginProcessor.cpp:25](Source/PluginProcessor.cpp:25) = `(int)(vel*127)`;
  piano-roll `ControlLane::getVal`
  [PianoRoll.cpp:2170](Source/Standalone/PianoRoll.cpp:2170) returns raw 0-1
  unscaled. So "hardest hit ~= halfway on the grid" is the KEYBOARD's
  velocity curve, not a code bug. Minor FL-parity deviations only:
  `(int)` truncation vs `round()`; default `0.8`
  [PatternManager.h:51](Source/PatternManager.h:51) and inconsistent `0.75`
  ([PianoRoll.cpp:3791](Source/Standalone/PianoRoll.cpp:3791) /
  [PianoRoll.cpp:3810](Source/Standalone/PianoRoll.cpp:3810)) vs FL `0.7874`.
  SEPARATE un-traced open item = the synth velocity→amplitude "too loud"
  curve in `BaySickSynthVoice` / `AdditiveVoice` / `VibePlayerDSP`.

**Process compliance this checkpoint.**
- Velocity-findings scope + the Task-0b commit-proceed were owner spec
  calls, not unilateral picks
  (`feedback_dont_make_unilateral_spec_calls.md`,
  `feedback_slot_placement_is_spec_call.md`).
- Commit routed through `/draft-commit`; drafted message surfaced verbatim +
  full pre-commit git status surfaced for owner approval before `git commit`
  (`feedback_every_commit_via_draft_commit.md` +
  `feedback_surface_drafted_commit_message_for_approval.md` +
  `feedback_surface_full_git_status_before_commit.md`).
- Drafter-output-verbatim discipline held — no self-authored restyle of the
  drafter output (`feedback_drafter_output_verbatim_no_restyle.md`,
  established earlier this session).

**Uncommitted state.** Clean working tree post-`f28319e`. This running-notes
file becomes dirty when this stamp is applied; it rides with the next commit
per the plan's post-commit running-notes cadence.

**Status:** **Task 0b CLOSED** — committed `f28319e`; owner-verified Fix
A / B / C in BOTH MT and ST (ST no regression). QA-Ea Task 0 still open; no
Part-B source touched. **Resume action:** (1) owner builds the deterministic
QA-Ea test rig (8-bar L/B/D engine parts + Vox / Inst / Rusty + dropped
song, metronome OFF, saved); (2) capture the Part-B 'before' master
reference in MT (record-nothing-armed — now works post-Task-0b); (3) QA-Ea
Part B Task 1 (route triad + Clips via `routeInsertOutput`; neutralize the
bespoke sum).

### 2026-05-19 — Task 0 — Post-Task-0b doc reconciliation + Task 0c (FL pre-roll record + non-destructive clip trim) added

Follow-on to the `### 2026-05-19 — Task 0b — CLOSED (3-bug post-mix-tail shared-helper extraction committed `f28319e`)` sub-block above. Tight ADDENDUM stamp documenting post-`f28319e` plan-file reconciliation + a new owner-found bug routed as **Task 0c**. The Task 0b A/B/C fixes and the FND-1/2/3 docket are already captured above and are NOT re-narrated; factual delta only. No QA-Ea Part-B source touched. QA-Ea Task 0 still open.

**Plan-file reconciliation — Vox/Inst/Rusty NOT required for Part B.** Owner caught a contradiction 2026-05-19 between my Task 0b commit-message handoff (which said to build "Vox+Inst+Rusty parts" up front) and what I'd told him verbally earlier (those parts aren't needed for Part B). Verified in code: Part B specifically unifies the Layers/Bass/Drums + Clips routing; Vox/Inst/Rusty already route through `routeInsertOutput` (the exact pattern Part B makes L/B/D match), so they are NOT exercised by Part B at all. Plan file [Batch Plans/polished-snuggling-token.md](Batch Plans/polished-snuggling-token.md) Task-0 "Tell Jeff" build script (line ~85) and Verification intro (line ~333) updated to make the rig **L/B/D + dropped song + metronome OFF only**; Vox/Inst/Rusty NOT required up front (optional, owner's call later if Part A SC2 bus-solo by-ear checks call for them). Reconciles the doc to the agreed scope.

**Task 0c added — FL-pre-roll record + non-destructive clip trim.** Owner-found 2026-05-19 mid Task-0b post-verify (with the MIDI keyboard, while verifying hardware-MIDI record works post-Fix-C): with the 1-bar count-in enabled before Record, the recorded master WAV contains a head bar of pre-roll (silent — metronome click is added post-`writeBlock` per the D-5 invariant) AND the resulting Audio clip is misplaced by one bar on the Builder grid. **Pre-existing** — the recorder feed has always run on `mMasterRecorder.isRecording()` alone with no `countInActive` gate; visible only post-Task-0b in MT because MT recording was the 104-byte empty bug before.

**Wrong-direction fix proposed and REJECTED by owner 2026-05-19 (important record).** I initially recommended a `!mMetro.countInActive` whole-block gate that would skip the recorder's `writeBlock` during count-in. Owner correctly identified this as a regression — cutting up to ~5 ms off the start of a recording slices drum transients and would generate "punchless / cut-off" bug reports. FL Studio does NOT gate; it writes the full pre-roll to the file and trims the visual clip non-destructively in the timeline. Owned per `feedback_diagnose_before_fixing.md` (owner is authoritative on FL behavior per `feedback_dont_speculate_about_fl_studio.md`).

**Spec (FL Studio pre-roll model — owner-locked 2026-05-19).** Recorder writes EVERY block from Record-pressed → Stop-pressed (no count-in gate); capture the **transport-start sample offset** at the moment `mMetro.countInActive` flips false; plumb through `VibeSynthProcessor::RecordResult`; `StandaloneEditor::commitRecordingResult` uses the offset to place the visual Audio clip on the Builder grid so its content-start = transport-start sample (NOT file sample 0); add a content-offset / source-start-sample field on `ArrangementBlock`; the audio-clip playback path honors it; MIDI notes captured during count-in placed relative to the song downbeat (verify existing `beatStart` math handles this); slip-edit UI affordance so owner can drag clip's left edge backwards into the pre-roll to recover early transients / early MIDI hits. Multi-file feature, NOT a 1-line fix.

**Task 0c sequencing (owner-locked 2026-05-19).** **After Task 0b commits (DONE, `f28319e`), before Task 1 Part B starts.** Per `feedback_slot_placement_is_spec_call.md`. Task 0c plan-file entries added in `## Files to modify` + `## Tasks` per `feedback_targeted_edits_not_wholesale_rewrite.md`. Detailed design + before/after surface required before any source edit, per Task 0b discipline.

**Process compliance this checkpoint.**
- Owner caught me re-introducing a contradiction (Vox/Inst/Rusty parts in the commit-message handoff after I'd verbally agreed they're not needed). Owned per `feedback_own_the_codebase_no_git_alibi.md` — corrected the plan file (Task-0 + Verification) instead of arguing.
- Owner caught me recommending a wrong-direction fix (count-in whole-block gate would slice drum transients). Owned per `feedback_diagnose_before_fixing.md` — pivoted to FL's pre-roll model (owner-authoritative on FL behavior per `feedback_dont_speculate_about_fl_studio.md`).
- Wrong inference about an attached WAV "being pre-Task-0b" based on filename timestamp — owned and corrected (both attached WAVs were post-Task-0b real captures; date-of-filename inference wasn't a reliable signal).
- Task 0c scope (own follow-up vs fold vs FND-4) + the doc-commit proceed-now were owner spec calls, not unilateral picks (`feedback_dont_make_unilateral_spec_calls.md`).

**Uncommitted state.** Plan file [Batch Plans/polished-snuggling-token.md](Batch Plans/polished-snuggling-token.md) dirty post-`f28319e` (Vox/Inst/Rusty correction + Task 0c Files-to-modify + Task 0c Tasks entries). This running-notes file becomes dirty when this addendum is applied. Both ride with the upcoming small doc-only follow-up commit. No source change.

**Status:** **Task 0b CLOSED** (`f28319e`); plan-file reconciled post-commit; **Task 0c scheduled** (slot owner-locked, after Task 0b commits, before Part B Task 1). QA-Ea Task 0 still open. **Resume action:** (1) surface the small doc-only follow-up commit (plan-file Vox/Inst/Rusty correction + Task 0c stubs + this addendum) via `/draft-commit` for owner approval; (2) then owner builds the deterministic QA-Ea test rig (8-bar **L/B/D engine parts only** + dropped song, metronome OFF; Vox/Inst/Rusty NOT required — only added later if owner opts in for Part A SC2 by-ear checks); (3) Task 0c — design + surface FL-pre-roll plumbing diff for owner approval before any source edit; implement on approval + owner verify + own commit; (4) Part-B 'before' master capture in MT (record-nothing-armed, now works post-Task-0b); (5) Part B Task 1.

### 2026-05-19 — Task 0 — Task 0c sub-spec design lock-in (post-`6084937`)

Follow-on to the `### 2026-05-19 — Task 0 — Post-Task-0b doc reconciliation + Task 0c (FL pre-roll record + non-destructive clip trim) added` sub-block above. Tight design-lock-in stamp closing the two remaining open Task 0c sub-specs and recording the pre-implementation code investigations. The FL pre-roll model, the Noodling-discard / Early-Strike-clamp MIDI rules, the in-scope strip-recorder coverage, and the slip-in-point UI semantics are already captured above and are NOT re-narrated; factual delta only. No QA-Ea Part-B source touched. QA-Ea Task 0 still open.

**Final Task 0c sub-spec calls locked.** Two remaining sub-specs answered:

- **MIDI input-quantize (owner-locked 2026-05-19, BUILT in Task 0c, NOT deferred).** Code investigation confirmed no global / Transport-level record-quantize setting exists today in the codebase — `Harmless/HarmlessModEditor` has its own SNAP toggle (mod-editor only), and `BuilderPage::mSnapMode` is visual-grid snap for clip placement, not MIDI record-time. Owner directed: build a new global APVTS param `record_quantize_div` (Int 0..6 = Off / 1/4 / 1/8 / 1/16 / 1/32 / 1/64), exposed as a NEW "Global Record-Quantize ▸" submenu added to the existing Record-button dropdown in `GlobalTransportBar` (the dropdown currently toggling ASIO / MIDI mode — owner specifically asked for the param to live there as a third option, not a new Transport widget). When non-Off, the MIDI commit path snaps clamped startBeats to the grid divisor AFTER the Early-Strike clamp-to-0.
- **Slip-edit trigger keybind (owner-locked 2026-05-19).** Owner pointed me to the keybinds plan; found `Ctrl+Alt+Home — toggle "resize from left edge" mode` at [Files For Claude/Keybinds & Feature Plan.txt:96](Files For Claude/Keybinds & Feature Plan.txt:96) in the D-7 (Smaller Piano Roll features) bundle. Domain mismatch surfaced (D-7 is piano-roll note left-edge resize; Task 0c is ArrangementGrid Audio-clip slip-edit). Owner chose **generalize the keybind to both surfaces**: Task 0c wires `Ctrl+Alt+Home` globally in `ApplicationCommandManager` + the ArrangementGrid surface (`mResizeFromLeftEdge` flag + `nearLeftEdge` + `mSlipEditing` + mouseDown / Drag / Up branches + cursor hint at the left edge + undo via `beginEdit("Slip")` / `commitEdit()`). D-7 later extends to the PianoRoll surface using the same keybind. Slip-in-point semantics owner-confirmed: right-end + `block.startBar` fixed; `contentStartSamples` + `lengthBeats` move (drag-left = reveal pre-roll + grow visible; drag-right = trim leading + shrink visible).

**Audio-clip render-loop investigation done.** Read [PluginProcessor.cpp:485-570](Source/PluginProcessor.cpp:485) (`renderAudioClipsForRow`, serial Pass 2) and [PluginProcessor.cpp:695-785](Source/PluginProcessor.cpp:695) (`renderFilePlayPlayer`, serial Pass 1 + MT task). Three categories of file-position computation per site need `+ player.contentStartSamples` injection:
- (a) direct-read `filePos` at [PluginProcessor.cpp:511](Source/PluginProcessor.cpp:511) / [PluginProcessor.cpp:722](Source/PluginProcessor.cpp:722).
- (b) phase-vocoder reference `pvRefPos` at [PluginProcessor.cpp:549-550](Source/PluginProcessor.cpp:549) / [PluginProcessor.cpp:762](Source/PluginProcessor.cpp:762).
- (c) file-EOF guard `fileEOFOutput` at [PluginProcessor.cpp:526-527](Source/PluginProcessor.cpp:526) / [PluginProcessor.cpp:737-738](Source/PluginProcessor.cpp:737) (subtract `contentStartSamples` from `fileTotalSamples` because the playable range starts at the offset).

Total 6 line edits across the 2 sites. `AudioClipStreamer` itself unchanged — already supports arbitrary `seek(int64 filePos)`. `AudioClipPlayer` gains a `juce::int64 contentStartSamples { 0 }` field populated from `block.contentStartSamples` at `rebuildAudioClipPlayers` ([PluginProcessor.cpp:~3286](Source/PluginProcessor.cpp:3286)).

**ArrangementGrid edge-drag investigation done.** Read [BuilderPage.cpp:1358-1364](Source/Standalone/BuilderPage.cpp:1358) (`nearRightEdge`), [BuilderPage.cpp:3875-3911](Source/Standalone/BuilderPage.cpp:3875) (resize mouseDown entry), [BuilderPage.cpp:4080-4104](Source/Standalone/BuilderPage.cpp:4080) (resize mouseDrag), and [BuilderPage.cpp:4238](Source/Standalone/BuilderPage.cpp:4238) (resize mouseUp commit). Right-edge resize exists; NO `nearLeftEdge`. Slip-edit slots in as a parallel `mSlipEditing` state with its own snapshot (`mSlipOrigContentStart` / `mSlipOrigLengthBeats` / `mSlipOrigStartBar`) and mouseDown / Drag / Up branches gated on `mResizeFromLeftEdge` (the global mode flag toggled by Ctrl+Alt+Home).

**Task 0c final scope (10 components).** Plan-file Task 0c entry updated with all locked sub-spec answers ([Batch Plans/polished-snuggling-token.md](Batch Plans/polished-snuggling-token.md), MIDI placement bullet + Slip-edit UI bullet). Components:
1. `mPreRollSamples` atomic + accum + reset + populate.
2. `RecordResult.preRollSamples`.
3. `ArrangementBlock.contentStartSamples` + XML save / load.
4. `AudioClipPlayer.contentStartSamples` plumbed at `rebuildAudioClipPlayers`.
5. Hot-path file-position injection (3 cats x 2 sites = 6 line edits).
6. `commitRecordingResult` master + strip blocks + Stop-during-count-in drop.
7. MIDI Noodling-discard + Early-Strike-clamp + input-quantize snap.
8. **NEW `record_quantize_div` APVTS param + `GlobalTransportBar` Record-button-dropdown "Global Record-Quantize ▸" submenu.**
9. **NEW Ctrl+Alt+Home keybind in `ApplicationCommandManager` toggling resize-from-left-edge mode** (Task 0c: ArrangementGrid; D-7: PianoRoll later).
10. `ArrangementGrid` `nearLeftEdge` + `mSlipEditing` + mouseDown / Drag / Up + cursor hint + undo.

Hot-path before / after for component (5) will be surfaced at commit-surface time per Task 0b discipline.

**Process compliance this checkpoint.**
- All Task 0c spec calls (MIDI placement rule, strip-recorder scope, slip-edit-in-scope, input-quantize-built, slip-edit-trigger-keybind) were owner spec calls, not unilateral picks (`feedback_dont_make_unilateral_spec_calls.md` + `feedback_slot_placement_is_spec_call.md`).
- Plan-file changes via targeted Edits, not wholesale rewrite (`feedback_targeted_edits_not_wholesale_rewrite.md`).
- Pre-implementation code investigations completed for: audio-clip render-loop hot-path ([PluginProcessor.cpp:485-785](Source/PluginProcessor.cpp:485)), input-quantize source-of-truth (none found in codebase), ArrangementGrid edge-drag ([BuilderPage.cpp:1358-4238](Source/Standalone/BuilderPage.cpp:1358)), Keybinds plan ([Files For Claude/Keybinds & Feature Plan.txt:96](Files For Claude/Keybinds & Feature Plan.txt:96)). Findings + design surfaced to owner before any source edit per Task 0b discipline (`feedback_diagnose_before_fixing.md` + `feedback_check_code_before_calling_it_expected.md`).

**Uncommitted state.** Plan file [Batch Plans/polished-snuggling-token.md](Batch Plans/polished-snuggling-token.md) dirty (the locked sub-spec answers Edits, +7/-2 vs `6084937`). This running-notes file becomes dirty when this addendum is applied. Both ride with the small Task 0c design-lock-in doc commit. No source change.

**Status:** **Task 0c design FULLY LOCKED** in plan file (sub-specs: FL pre-roll model + Noodling / Early-Strike / input-quantize MIDI rules + master + strip recorders in scope + slip-in-point UI semantics + Ctrl+Alt+Home keybind generalized + `record_quantize_div` APVTS + Record-button-dropdown submenu). Pre-implementation investigations complete. QA-Ea Task 0 still open. **Resume action:** (1) surface the small doc-only Task 0c design-lock-in commit (plan file + this addendum) via `/draft-commit` for owner approval; commit on approval. (2) Begin Task 0c implementation. Per Task 0b discipline, surface the hot-path file-position injection before / after (component 5) before applying the source edit; other components (1-4, 6-10) implement + surface at commit-surface time. Components touch: `Source/PluginProcessor.cpp` / `.h`, `Source/PatternManager.h` / `.cpp`, `Source/Standalone/StandaloneEditor.cpp`, `Source/Standalone/BuilderPage.cpp`, `Source/Standalone/GlobalTransportBar.cpp` / `.h` (the dropdown extension), possibly more for the keybind in `ApplicationCommandManager`.
