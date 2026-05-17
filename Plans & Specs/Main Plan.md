# BaySickDAW Post-Batch-10 QA Triage & Batching Plan

## Context

Batch 10 of the multi-threaded render engine project shipped on 2026-05-06.
The MT path is now production: runtime atomic toggle (`gMultiThreadedEngineEnabled`),
hamburger-menu UI, settings.xml persistence, default ON.

The active backlog at close-out was 64 entries (59 active + 4 optional + 1 never)
in `Files For Claude/BaySickDAW Master QA & Issue Backlog (UNIFIED 2026-05-07).txt`.
This plan converts that backlog into a sequence of small, granular batches.

**Diagnostic discovery during this triage** (2026-05-07): WAV/MP3 drops on the
Builder grid produce silent playback under MT. Toggling MT off restores
correct playback. This is **DSP-12 in action** — `ClipPageTask` (auto-spawned
by `spawnClipsTabIfMissing`) and `AudioInsertTask` both register at the same
channel id under MT, the dispatcher's most-recent-registration-wins rule
overwrites AudioInsertTask, and the Clips-tab page has no MIDI notes
triggering its sampler so the row outputs silence. Validation suite missed
it because Vox/Inst recording tests use different render-task types
(`VoxStripTask`/`InstStripTask`) with no channel-id conflict.

**This bug is now the first batch** — it blocks core arrangement
functionality (drop a WAV, hear it play) and gates every downstream audio-path
batch's verification baseline.

**Plan goal:** convert the backlog into a sequence of small batches that
preserve the MT-path discipline, each with its own commit/rollback boundary,
and avoid one-fix-breaks-another rework by coordinating items that touch
the same code surface. **This is a triage plan, not implementation plans** —
each batch gets its own `<silly-name>.md` plan file when it starts.

**Sequencing decision (user-confirmed):** Option A — confidence-first.
**DSP-12 fix shape (user-confirmed):** Composite RenderTask.
**FILE-02 reassignment timing (user-confirmed):** Immediate (next audio block).

---

## 0. Three-Doc System + Carry-Over Discipline (READ FIRST)

This plan operates with **three companion documents** that together preserve
context across the 15-batch execution. Per-batch plan authors read all
three at start; per-batch execution updates them in different ways. The
three-doc structure protects against losing context across sessions —
the #1 risk when a 15-batch plan stretches over weeks rather than days.

| Doc | Filename | Purpose | Update cadence |
|-----|----------|---------|---------------|
| **Plan** | `Main Plan.md` (this file) | Master sequencing + per-batch scope + dependencies. The "what to do, in what order". | **Hybrid: inline back-refs + master fork log.** When work scope changes, edit the affected section(s) inline with a one-line back-ref to the master fork log (e.g. "QA-0a fork — see §9"), AND append a full narrative entry to §9 Forks. Inline annotations preserve the linear-read mode (sections describe their actual content); the master log preserves the chronological "what's changed since plan write?" overview. Original content is never deleted — inline back-refs add a line, master log appends entries. Convention adopted 2026-05-07 (see §9 first entry). |
| **Carry-Forward Reference** | `Carry-Forward Reference.md` | Architectural primitives + file:line index + decisions already made + per-item status + patterns to reuse. The "what was true on 2026-05-07 when triage closed". | **Frozen after creation.** This is the reference point captured during the 2026-05-07 triage session — never edited. New findings during execution go into the implemented-work doc instead. If a carry-forward entry turns out to be wrong, the implemented-work doc records "carry-forward §X said Y, verified to be Z" — the carry-forward stays as the historical snapshot. |
| **Implemented Work & Findings** | `Implemented Work Log.md` | Running log of executed batches: what was done, what was found along the way, what was done about each finding. The "what's happened since plan write". | **Append-only.** New entry per batch (or per significant stopping point within a batch). Never edit prior entries — surprise findings later get their own new entry. Also captures contradictions of the carry-forward as new entries. |

The carry-forward and implemented-work files are created when this plan
exits plan mode. The carry-forward starts populated with everything
verified during this triage session and is then sealed. The
implemented-work log starts empty.

Three standing rules apply to every per-batch plan derived from this plan.

**Rule 1 — Every per-batch plan starts by reading all three companion docs.**
Plan: read your batch's section (scope, dependencies, risk). Carry-forward:
read §1-3 minimum (architectural primitives, files, decisions); skim the
rest. Implemented-work: read every entry since your batch's predecessors
were planned, so you know what changed beneath your feet.

**Rule 2 — Every stopping point produces a carry-over block.** "Stopping
point" = any time the work pauses long enough that context will be lost
(end of session, end of day, end of batch, blocked-on-decision pause).
Before pausing, write a 5-10 line carry-over block at the bottom of the
active per-batch plan file under a clear `## Carry-Over` heading covering:

- **Completed**: which steps in the per-batch plan finished + verified.
- **In-flight**: which step is mid-execution + what state the code is in
  (uncommitted edits? failing build? partial test?).
- **Assumptions changed**: anything learned during the batch that
  contradicts the plan, the carry-forward file, CLAUDE.md, or the
  implemented-work log so far. Carry-forward contradictions go into the
  implemented-work doc as new entries — the carry-forward itself is not
  edited.
- **Resume action**: the literal first thing the next session should do
  (e.g., "re-run `do_build.bat`", "read carry-forward §3 then re-read
  CompositeAudioInsertTask::run()", "ask Jeff about edge case X").
- **Implemented-work entry needed**: a one-line summary of what to log
  in the implemented-work doc when the batch closes. Includes any
  carry-forward contradictions surfaced during the session.

Non-negotiable for any batch that takes more than one session. Skipping
it = guaranteed re-discovery cost when the next session opens.

**Rule 3 — Findings discovered during batch execution get routed at
batch close (no spaghetti, no Phase-6 punt).** Adopted 2026-05-07
during QA-0 execution. At every batch close, review the implemented-
work entry's "Found along the way" items + any open prior findings.
For each:

- **Touches a not-yet-started batch's surface in §5** → fold into that
  batch's scope (expand the Items + Scope of the existing §5 entry).
  §9 Forks entry records the addition.
- **Touches a completed batch's surface** → annotate the completed
  batch's §5 entry with a one-line pointer (e.g., `*Post-close
  findings: see §9 [date]*`). All details (the finding, the fix's
  plan-file path, eventual commit hashes) live in the §9 Forks entry.
  **No new §5 batch row.**  Commits stay sequential as flag points;
  the plan reflects conceptual ownership.
- **No surface match** (genuinely new work area) → new dedicated §5
  batch row, slotted into the appropriate phase. §9 Forks entry
  chronicles the addition.
- **Phase 6 stays reserved** for dead/dormant code cleanup. Functional
  bugs never get punted there.

Net: §5 stays the planned-sequence map; §9 Forks is the canonical
"what changed since plan write" log; the implemented-work doc is the
chronological execution ledger. Reviewing §9 gives you every fork +
addition + post-close finding in one place.

**Rule 4 — Diagnostic instrumentation tracked in a per-batch
catalog.** Adopted 2026-05-12 during QA-E Task 3 MT-FilePlay
diagnosis after ~15 `[QA-E DIAG]` sites had been shipped across
`PluginProcessor.cpp` + `StandaloneEditor.cpp` with no running
record of what got added, where, or whether it was meant to stay.

Every diagnostic addition (`DBG` / `juce::Logger::writeToLog` /
temp `jassert` added for diagnosis / debug `juce::AlertWindow`
popups / temp file logging / `std::cout`-style traces) gets logged
in the batch's running notes file (`Plans & Specs/Running Notes/
<silly-name>.md`) under a `## Diagnostic Instrumentation Catalog`
section.  Catalog row format: markdown table with four columns --
`Site` (file:line or symbolic tag if line is unstable), `Tag` (the
literal log-string prefix used, e.g. `[QA-E DIAG]` or `[NAMIR]`),
`Purpose` (one sentence), `Disposition` (`Remove at <task> close` /
`Remove at batch close` / `Keep`).

- **Append the catalog row WITH the code change**, not after the
  fact.  The catalog row gets written in the same edit pass as the
  diagnostic add.
- **At task / batch close**, walk the catalog and strip every
  `Remove` entry from the source.  Leave `Keep` entries alone.
  Surface the strip list to the user for approval BEFORE running
  the strip pass.
- **Pre-existing diagnostics** (e.g., NAMIR / Pedals / Audio Setup
  state logs / MT QA-Md hamburger menu item) get retro-added with
  `Keep` when first surfaced during a batch so the catalog stays
  complete across sessions.
- **`Keep` vs `Remove` borderline calls** surface to the user.
  Don't decide alone (cross-refs Rule 3 routing-spec-call discipline
  and the "spec calls are the user's" convention).

Why: diagnostic instrumentation accumulates silently.  Added during
a bug hunt, then forgotten when the hunt closes.  Without a
catalog, future sessions can't tell which sites have long-term
value vs. one-shot trace noise -- either useful logging gets
stripped accidentally or dead diagnostics bloat the runtime log
forever.

### Document Formatting Conventions (canonical for all `Plans & Specs/` docs)

All five `Plans & Specs/` documents follow a shared formatting doctrine
so that grep patterns, cross-references, and timestamp parsing stay
uniform. Each doc has a brief local "Header conventions" section that
points back here for the cross-doc rules and documents its own doc-
specific layout. **When adding new content, match these conventions
before writing.**

**Heading hierarchy (universal):**

- `#` — document title (one per doc).
- `##` — top-level section.
- `###` — sub-section / batch / phase / dated entry header.
- `####` — item or sub-section within a sub-section.
- `#####` — bullet group within an item (sparingly; prefer bold labels).

**Timestamps:**

- ISO date: `YYYY-MM-DD` (e.g., `2026-05-08`).
- Time: 24-hour clock with `PT` (Pacific Time) suffix — covers both
  PDT and PST without DST-overlap ambiguity. Example: `10:42 PT`.
- Combined date+time header: `YYYY-MM-DD HH:MM PT — <Batch ID> — <Summary>`.

**Cross-references:**

- Section reference: `§N` (e.g., `§5`, `§9 fifth entry`).
- File path link: `[Source/path/file.cpp](Source/path/file.cpp)` markdown
  link, optionally with `:line` suffix (e.g.,
  `[PluginProcessor.cpp:1737](Source/PluginProcessor.cpp:1737)`). Within
  `Plans & Specs/` docs, the link target is repo-relative.
- Sister doc reference: `[Main Plan.md](Main Plan.md)` (no path prefix
  since they're co-located).
- Commit hash: backticked 7-char short hash, e.g. `` `c05ce61` ``.

**Item ID prefixes (used across all docs for the same logical work):**

| Prefix | Source | Doc home |
|--------|--------|----------|
| `BLU-*` | `Files For Claude/vibedaw_blueprint.md` | `Previously Implemented.md` (Done) / `Future State.md` (Tier 3) |
| `FSW-*` | `Files For Claude/Final Stretch Work.txt` | same as BLU |
| `LDT-*` | `C:/Users/jeffm/.claude/plans/lucky-discovering-tiger.md` | same as BLU |
| `CL-*` | Claude fire-hose proposals (QA-Inventory Phase 5) | `Future State.md` Section 2 |
| `QA-*` | Active V1 batches | `Main Plan.md` §5 |

**Per-doc layout patterns:**

| Doc | Top-level section style | Entry style |
|-----|-------------------------|-------------|
| `Main Plan.md` | `## N. <Title>` numbered phases (§0-§9) | `### QA-<Letter> — <Title>` batch headers inside §5; `### YYYY-MM-DD — <Title>` entries inside §9 Forks |
| `Carry-Forward Reference.md` | `## §N. <Title>` numbered (§1-§9); FROZEN | `### <Sub-topic>` clusters; `- <bullet>` items |
| `Implemented Work Log.md` | `## <Section>` (Header conventions, How to read, Entries) | `### YYYY-MM-DD HH:MM PT — <Batch ID> — <Summary>` per batch close; `#### <Sub-section>` (Done, Found along the way, etc.) |
| `Previously Implemented.md` | `## <Section>` (Header conventions, Sources surveyed, How to read, Entries) | `### Phase X - <Name>` grouping; `#### **<ID>: <Title>**` entries with uniform 5-line shape (Sources / Implemented / Source / Verified) |
| `Future State.md` | `# Section N` major bands; `## <Cluster>` within | `- **[<ID> / <TAG>]** <Title> — <description>.` one-liner per item |

**Batch Plans + Running Notes layout (locked 2026-05-11):**

Per-batch files (`Plans & Specs/Batch Plans/<silly-name>.md` + paired
`Plans & Specs/Running Notes/<silly-name>.md`) follow a uniform structure
so plan execution is mechanical + verifiable.  Reference exemplar:
[`Plans & Specs/Batch Plans/federated-bouncing-cupcake.md`](Batch Plans/federated-bouncing-cupcake.md)
(QA-D).  Structure every new batch plan + running notes pair to match.

**Plan file (`Batch Plans/<silly-name>.md`) required sections:**
- Title line: `# QA-<Letter> — <Scope summary> — Plan (<silly-name>)`
- Canonical-path callout + "For execution" note at top
- **Context** — why this batch, risk, effort estimate, dependencies
- **Spec calls already locked** — table with ID / Decision / Reasoning columns
- **Sub-spec calls surfaced for ExitPlanMode** (or "No sub-spec calls open" if all resolved)
- **Files to modify** — per-task list with file:line references for every surface touched
- **Tasks** — numbered `### Task N — <Name>` sections, each containing:
  - `- [ ]` checkbox steps for every executable action
  - Embedded code blocks (```` ```cpp ```` / ```` ```markdown ````) showing non-trivial fix patterns, before/after pairs where useful
  - Explicit "Tell Jeff: ..." verify scripts with numbered test scenarios `(1)`, `(2)`, `(3)` ...
  - `/draft-commit` → surface drafted message + full git status → Jeff approves → commit step (per `feedback_surface_drafted_commit_message_for_approval.md` + `feedback_surface_full_git_status_before_commit.md`)
  - `/draft-doc running-notes` dispatch + apply step (per `feedback_draft_doc_running_notes_every_checkpoint.md`)
- **Verification (end-to-end smoke)** — final integration test scenarios after all source tasks land
- **Routing notes (Rule 3 application during execution)** — guidance for findings that surface mid-batch
- **Carry-Forward Reference touch points** — which §-sections to read at which task start

**Running notes file (`Running Notes/<silly-name>.md`) required sections:**
- Title line: `# Running Notes — QA-<Letter> (<silly-name>)`
- Purpose blockquote explaining append-only nature + checkpoint trigger + close-time consumption
- Pair file reference + convention reference
- `## YYYY-MM-DD — Task N — <name>` entries appended at every checkpoint (commit landed / sub-task verified / finding captured / spec call resolved / scope pivot) per `feedback_draft_doc_running_notes_every_checkpoint.md`

**Why the rigid structure:** plan files are read mid-execution by tired humans + agents.  Checkboxes turn the plan into a literal punch-list.  Code blocks pre-resolve "how does this change look in source" questions before commit.  Numbered verify scripts let Jeff drive the test cycle without rebuilding context each time.  Embedded "Tell Jeff:" markers make it unambiguous when execution pauses for the build cycle.  Pre-QA-D plan files don't fully match this shape; the rule applies going forward.

**Folder-level scope (added 2026-05-08):**

`Plans & Specs/` is the **umbrella for ALL durable project artifacts**,
not just the literal "plans and specs" the folder name implies.  The
name is preserved (renaming would break every existing link) but the
scope is broader than the name suggests.  Anything that is:

- a planning artifact (Main Plan, batch plans, Carry-Forward),
- a record of what shipped (Previously Implemented, Implemented Work Log),
- a roadmap or wishlist (Future State),
- a research artifact (competitive sweeps, technical investigations),
- a decision log or post-mortem,
- a recurring report (smoke tests, audits),

…lives under `Plans & Specs/` in an appropriate subfolder.  Subfolders
are organized by artifact TYPE, not by topic / domain (the canonical
domain buckets are inside the docs, not the folder structure).

Current and approved subfolders:

- `Plans & Specs/Batch Plans/` — per-batch implementation plan files (`<silly-name>.md`).
- `Plans & Specs/Research Reports/` — output from competitive sweeps,
  technical investigations, market scans (e.g., the dated competitive-
  research aggregates that `competitive-research` agent produces).
- `Plans & Specs/Running Notes/` — per-batch running-notes log
  (`<silly-name>.md` matching the paired `Batch Plans/<silly-name>.md`).
  Append-only mid-batch artifact populated by `/draft-doc running-notes`
  dispatches at every checkpoint (commit landed / sub-task verified /
  finding captured / decision made / scope pivot / spec call resolved).
  At batch close, `/draft-doc batch-close` reads this file as the
  primary input when compiling the single Implemented Work Log entry.
  Established 2026-05-09 mid-QA-A after the running-notes home gap was
  flagged.
- (more subfolders added here as the need arises — decision logs,
  post-mortems, audit reports, etc.; document each new subfolder here
  when added so the structure stays discoverable).

The five canonical top-level docs (`Main Plan.md`, `Carry-Forward
Reference.md`, `Implemented Work Log.md`, `Previously Implemented.md`,
`Future State.md`) stay at the root of `Plans & Specs/` regardless of
how many subfolders accumulate.

**Grep patterns (uniform across docs):**

- `^### ` finds all sub-section / batch / phase / dated headers.
- `^#### ` finds all items / nested sub-sections.
- `^## ` finds all top-level sections.
- `^# ` finds the document title (or major section bands in
  `Future State.md`).

**Append-only / edit policy:**

- `Carry-Forward Reference.md`: **frozen.** Never edit. Contradictions
  are recorded as new entries in `Implemented Work Log.md`.
- `Implemented Work Log.md`: **append-only.** Never edit prior entries.
  New findings later get their own new entry.
- `Previously Implemented.md`: **append-only.** Manual edits to existing
  entries are intentionally rare; updates record changes over time in
  the QA-era log instead of rewriting history.
- `Main Plan.md`: **edit per Rule 3** (inline back-refs + master fork
  log). Original §1-§8 content preserved; §9 Forks is the canonical
  chronological log of changes since plan write.
- `Future State.md`: **append-only**, organised by domain bucket
  (per the canonical bucket list below). New items land under their
  natural domain alongside source-doc + fire-hose + walked entries
  from the same domain. Section 2 ("Considered & Dropped") stays
  separate as its own top-level section so we don't re-litigate, and
  retains its drop-reason groupings.

**Canonical domain buckets (cross-doc rule, locked 2026-05-08):**

All `Plans & Specs/` docs (Main Plan §5.5 / `Implemented Work Log.md`
/ `Previously Implemented.md` / `Future State.md`) use the same
top-level domain vocabulary so cross-doc lookups stay one-shot. To
answer "what's our story for X?" you grep one bucket name across all
four docs and find what's done, what's in flight, what's next, and
what's deferred — all in one consistent place.

| # | Bucket | Covers |
|---|--------|--------|
| 1 | **Effects** | DSP modules (Chorus / Compressor / Delay / Flanger / Limiter / Overdrive / Phaser / Reverb / Saturation / Tape / Transient Shaper / EQ8) + FX rack mechanics + future / restoration effects + dynamic EQ + VST3 effect hosting. |
| 2 | **Players** | Every sound-producing engine: Harmless, BaySickPlayer, BaySick family (Synth + Bass), dynamic-drum work (Phase D), BaySickVocal (Phase H), BaySickPedals (Phase I), BaySickRustyDrums (Phase J), BaySickGuitars (Phase K), BaySickBasses (Phase L), BaySickNAM/IR + future engines (Wavetable / FM / Analog / Modal / Strings / Vocoder / etc.). |
| 3 | **Mixer / Routing** | Mixer page, mixer strips, routing graph (`VibeGraph`/`RoutingGraph`), sends + aux strips, cable overlay, mastering chain, metering. |
| 4 | **System Pages** | Builder, Effects Page, Audio Settings, Project Persistence (XML / restore walker / migration), Keyboard input + ApplicationCommandManager, Mouse-modifier docs. |
| 5 | **UI / L&F / Theming** | VibeLAF, palette, themes, layouts, pattern colours, ribbon visuals, knob styling, look-and-feel cross-cuts. |
| 6 | **Cross-cutting Infrastructure** | Engine (MT render path), `RenderGraphDispatcher`, BlockContext, audio device init, MIDI input, recording lifecycle, `Project Persistence` sub-cluster, `Audio-Device Infrastructure` sub-cluster, performance / efficiency optimization. |
| 7 | **User Tools / Learning** | AI helpers (Composer / Mixer / Master), tutorials, smart melody / chord / drum / bass generators, hover-to-hear, scale picker, beat detection, sound-design guides. |
| 8 | **Workflow Polish** | Multi-window, project snapshots / version control, cloud sync, sharing, comments, macros, performance pad, drum-pad mode, track grouping, section markers, action recorder. |
| 9 | **Other / Platform / Deferred** | VST/AU plugin hosting (instrument + effect), surround / Atmos, tablet DJ app, multi-touch, hardware MIDI output, MIDI export. |
| 10 | **Meta** | Sessions / Decisions / Standing Parallel Work / Open Issues — non-domain bookkeeping that doesn't belong to any product surface. |

**Bucket usage rule (per doc):**

- **Main Plan**: §5 batch sequence stays in chronological / dependency
  order. New §5.5 Domain Coverage section maps each bucket → which
  QA-* batches fall in it (per-batch entries each get a `**Bucket:**`
  line in their §5 header).
- **Implemented Work Log**: every batch close entry has a `**Bucket:**`
  line under the date header so grepping by bucket finds all completed
  work in that domain.
- **Previously Implemented**: top-level `## ` sections ARE the bucket
  names (one per bucket). Existing phase-named clusters (Phase D /
  Phase H / Phase 5F / etc.) are preserved as `### ` sub-clusters
  inside their natural bucket.
- **Future State**: top-level `## ` sections inside Section 1 ARE the
  bucket names. Source-doc + fire-hose + walked items from the same
  domain live together under sub-clusters (`### §1 ChorusDSP`, `### §P1
  Harmless`, `### Fire-Hose net-new`, etc.). Section 2 (Considered &
  Dropped) stays separate from this domain layout — drops stay grouped
  by drop-reason, not by domain.

### Agent Orchestration Rules (canonical for all sessions, locked 2026-05-08)

The project has a tiered subagent system at `~/.claude/agents/` (cross-
project), `<repo>/.claude/agents/` (project-specific) plus matching
slash commands.  Subagents are **manually dispatched** — there is no
event system that auto-fires them.  These rules tell future sessions
WHEN to dispatch which agent so the value gets captured without
relying on the user to remember every slash command at every moment.

**Cross-doc rule:** every BaySickDAW session reads these orchestration
rules at start (alongside §0's Document Formatting Conventions and
the canonical bucket list).  Future sessions in the CotBB game repo
mirror the equivalents from that project's CLAUDE.md.

#### Slash command + agent reference table

| Scope | Slash command | Agent name |
|-------|---------------|------------|
| Global | `/draft-commit` | `commit-drafter` |
| Global | `/diagnose-build` | `build-error-diagnoser` |
| Global | `/explain` | `concept-explainer` |
| Global | `/standup` | `standup-summarizer` |
| Global | `/extract-spec` | `spec-extractor` |
| Global | `/audit-licenses` | `license-auditor` |
| BaySickDAW | `/read-doc` | `doc-reader` |
| BaySickDAW | `/draft-doc` | `doc-drafter` |
| BaySickDAW | `/review-batch` | `batch-code-reviewer` |
| BaySickDAW | `/test-signal` | `dsp-test-signal` |
| BaySickDAW | `/preset-gaps` | `preset-coverage-mapper` |
| BaySickDAW | `/research` | `competitive-research` |
| BaySickDAW | `/architecture` | `daw-architecture-research` |
| BaySickDAW | `/perf-audit` | `performance-auditor` |
| CotBB | `/write-scene` | `scene-script-writer` |
| CotBB | `/build-dialog` | `dialog-tree-builder` |
| CotBB | `/level-checklist` | `level-design-checklist` |
| CotBB | `/validate-naming` | `asset-naming-validator` |
| CotBB | `/architecture` | `godot-architecture-research` |
| CotBB | `/perf-audit` | `performance-auditor` (Godot-specific, distinct from BaySickDAW's same-named agent — slash command resolves project-scoped) |

#### Session lifecycle

- **Session open** → `/standup`.  Brings the session up to speed on git
  state + recent batch closes + uncommitted work + plan position
  before any other work begins.
- **Resume after compaction or context break** → `/standup` again.
  Treat it as a context refresh, not just a daily ritual.

#### Batch lifecycle

- **Batch start** — read the batch's plan file, then dispatch
  `/standup` with focus on the batch ID so any in-flight context from
  prior sessions is surfaced.
- **Mid-batch checkpoint** (after each significant chunk: a major file
  change committed, a sub-task verified, a finding captured) →
  `/draft-doc running-notes`.  Mode: append.  This is the
  context-preservation mechanism — capture before context fills up,
  not after.
- **Build failure** (any `do_build.bat` non-zero exit) →
  `/diagnose-build` with the failing block from `build_log.txt`.
  Diagnose before retrying.
- **Concept blocker** (you hit an unfamiliar idea) → `/explain
  <concept>`.  Don't bluff or web-search inline; route to the agent.
- **Pre-commit** → `/draft-commit`.  Show the user the proposed
  message; commit only after explicit approval.
- **Batch close (mandatory sequence):**
  1. `/draft-doc batch-close` — compile the Implemented Work Log
     entry from running notes.
  2. `/review-batch <id>` — diff vs plan + rules + memory-tracked
     gotchas.  Address BLOCKERS before proceeding.
  3. Apply the doc draft via Edit (parent session, not the agent).
  4. Commit the close (separate commit from the batch's source
     commits — clean rollback boundary).

#### Domain-specific triggers

- **New DSP module added or upgraded** → `/test-signal <module>` once
  wiring is complete.  Run before declaring the module shippable.
- **New mixer-strip type** → cross-check against memory
  `reference_mixer_strip_pattern_audit.md` BEFORE the implementation
  diff lands; the audit checklist catches the ~15 sites a new strip
  type touches.
- **Every 3 batches OR pre-milestone** → `/perf-audit`.  Recurring
  scan of the codebase for performance opportunities; context-aware
  (audio thread vs setup time) so prepare-time allocations don't get
  flagged as audio-thread issues.  Findings either fold into the next
  planned batch or get a dedicated `QA-Perf-Sweep-<N>` batch.
- **Before architecture decision** (where multiple plausible approaches
  exist) → `/architecture <topic>`.  Researches how other DAWs solve
  the topic with a recommendation.  On-demand only; do NOT run
  weekly.  Output saved to `Plans & Specs/Research Reports/
  daw-architecture-<topic>-<date>.md` via drafter pattern.
- **Pre-release sweep** (before tagging V1) → `/audit-licenses`.
  Vendored libs, plugin licenses, asset attribution, EULA scope.
- **Pre-QA-Templates batch** → `/preset-gaps`.  Gap analysis informs
  what factory presets the QA-Templates batch should add.
- **Pre-milestone (V1, V2)** → `/research [focus area]` one-shot per
  focus area.  Don't run weekly — public info skews to marketing puff
  and the value comes from milestone-spaced sweeps, not continuous
  scanning.
- **Long planning / triage discussion just wrapped** → `/extract-spec`
  to capture decisions before they get lost across compactions.

#### Anti-rules (when NOT to dispatch)

- **Trivial 1-line edits, 2-file commits** — agent overhead exceeds
  value.  Just do the work inline.
- **Read-only lookups you already know** — don't dispatch `/read-doc`
  for a section you've just read in the same session.
- **Speculative "let me just check" runs** — no.  Only run on a real
  trigger from the rules above.
- **Building drafts NOT requested by the user** — `/draft-doc`
  produces text the parent session reviews and applies.  Drafting
  ahead of need wastes tokens.

#### Drafter-only enforcement

`/draft-doc`, `/research`, and any agent that produces content destined
for `Plans & Specs/` operates in **drafter-only mode**: returns text
in code blocks; the parent session reviews and applies via Edit.  No
agent autonomously writes to `Plans & Specs/` files.  This boundary
exists because of the 2026-05-08 6-of-10 buckets blast-radius incident
(see `feedback_canonical_structure_no_eliding.md` in user memory).

#### When in doubt

If a situation isn't covered by a rule above, ask the user before
dispatching an agent.  Pattern: "I think `/foo` would help here
because <reason>; want me to dispatch?"  Never auto-dispatch on a
gray-area trigger.

**Initial carry-over for this plan (2026-05-07):**

- **Completed**: Triage + verification + plan write + carry-forward
  reference write + implemented-work doc creation (empty).
- **In-flight**: Plan approved by user; entering execution phase.
- **Assumptions changed**: WAV/MP3 drop on Builder is silent under MT
  (was working pre-Batch-10). DSP-12 fix is now top-priority blocker.
- **Resume action**: Start QA-0 by reading the carry-forward file's
  §1-3 + the actual `AudioInsertTask::run()` and `ClipPageTask::run()`
  bodies. Then write the QA-0 per-batch implementation plan.
- **Implemented-work entry needed**: Initial entry "2026-05-07 Triage —
  plan + carry-forward + implemented-work docs created. No code changes."

---

## 1. Architectural Orientation (informs sequencing)

All items below confirmed against current HEAD by reading code. Citations
are `file:line` so you can grep back.

### MT render path (production, default ON)

- **Flag**: `RenderEngine::gMultiThreadedEngineEnabled` is `inline std::atomic<bool>`
  at [Source/Engine/RenderEngineFlags.h:44](Source/Engine/RenderEngineFlags.h:44).
  Read with acquire at [PluginProcessor.cpp:1831](Source/PluginProcessor.cpp:1831);
  toggled with release from Mixer hamburger at [StandaloneEditor.cpp:4450-4497](Source/Standalone/StandaloneEditor.cpp:4450).
  Persistence: `VibesynthStandaloneApp::saveMultiCoreRenderingPref()`
  ([StandaloneApp.cpp:240-264](Source/Standalone/StandaloneApp.cpp:240)).
- **Dispatcher**: `RenderGraphDispatcher::rebuildLinks()` runs every block
  ([PluginProcessor.cpp:1737](Source/PluginProcessor.cpp:1737)). `dispatchBlock(const BlockContext&)`
  is the parallel pump under flag=true.
- **BlockContext** ([Source/Engine/BlockContext.h](Source/Engine/BlockContext.h)) carries
  `numSamples`, `bpm`, `posInfo`, `anySolo`, `busAnySolo`, `panLaw`, 7 per-engine
  MIDI buffers, `liveInputSnapshot`.
- **Tasks** in `Source/Engine/Tasks/`:
  - `EngineInsertTask` (Layer/Bass/Drum)
  - `PassiveStripTask` (Aux + Bus)
  - `MasterTask` (sink)
  - `VoxStripTask` / `InstStripTask` (live input + source-mode aware)
  - `ClipPageTask` / `AudioInsertTask` — **conflict at `audioInsert(N)`**
    (DSP-12 root). Comment at [PluginProcessor.cpp:4240-4245](Source/PluginProcessor.cpp:4240)
    acknowledges composite case unresolved.

### Lock-free + lifecycle primitives (Batch 9c shipped — reuse, do not reinvent)

- **AudioClipSnapshot RCU** ([PluginProcessor.h:512-516](Source/PluginProcessor.h:512))
  + atomic publish via `mActiveAudioClips.exchange(...)`. Audio thread
  load-acquires once at top of processBlock, uses same pointer for FilePlay
  scan, Pass 2, applyChokeGroupDispatch, AudioInsertTask, VoxStripTask,
  InstStripTask.
- **RetirementQueue<T>** ([Source/Engine/RetirementQueue.h](Source/Engine/RetirementQueue.h))
  generic generation-stamped queue + dedicated drainer thread. Currently
  used for `RetirementQueue<AudioClipSnapshot>` only.
- **closeAllDynamicTabs** ([StandaloneEditor.cpp:8672-8719](Source/Standalone/StandaloneEditor.cpp:8672)):
  sets `mProjectLoadInProgress(true)` → 30 ms sleep → close every dynamic
  tab → clear ribbon → reset barrier. **First step of `~StandaloneEditor`**
  ([:1304-1326](Source/Standalone/StandaloneEditor.cpp:1304)). Called before
  project-open at [:7670, :8076](Source/Standalone/StandaloneEditor.cpp:7670).
  **Reuse this — do not invent parallel teardowns.**
- **mProjectLoadInProgress barrier** ([PluginProcessor.h:885-889](Source/PluginProcessor.h:885)):
  audio thread acquire-loads, clears buffer to silence if true.
- **mShuttingDown gate** in `BaySickVocalProcessor` only
  ([BaySickVocalProcessor.h:170](Source/BaySickVocal/BaySickVocalProcessor.h:170)).
- **drainMeterAtomicsForUI** ([PluginProcessor.cpp:2880](Source/PluginProcessor.cpp:2880))
  on audio thread, called from BOTH branches.
- **measureDspLoadAndOverload** ([PluginProcessor.cpp:2951-2956](Source/PluginProcessor.cpp:2951))
  wall-clock measurement, called from BOTH branches. Audio-thread-only
  measurement under MT — full sum-of-cores reading is DIAG-02 work item.
- **pullSidechainPredecessorsToGraph** ([Source/Engine/SidechainPullHelper.h:42-63](Source/Engine/SidechainPullHelper.h:42)).

### Mixer/page lifecycle (key file:line index)

- **Spawn cascades** ([MixerPage.cpp](Source/Standalone/MixerPage.cpp)):
  `addVoxChannelAtIndex` (:1677), `addInstChannelAtIndex` (:1999),
  `removeVoxChannel` (:2331), `removeInstChannel` (:2323). Maps:
  `mVoxStrips` (:311), `mInstStrips` (:316).
- **onTabClosed Vox vs Inst** ([StandaloneEditor.cpp:3525-3535 vs :3631-3632](Source/Standalone/StandaloneEditor.cpp:3525)):
  Inst calls `removeVoxChannel`-equivalent; Vox does NOT. **MIX-01 confirmed open.**
- **Project XML restore walker** ([:6571 Vox, :6623 Inst](Source/Standalone/StandaloneEditor.cpp:6571)):
  spawn calls present, but tab-reload-destroys-strip still happens — bug is
  downstream of spawn (post-spawn teardown or guard fail in spawn helpers).
  **MIX-02/04/06 confirmed still open by user.**
- **Recording finalize** ([StandaloneEditor.cpp:9443-9520](Source/Standalone/StandaloneEditor.cpp:9443)):
  `addAudioToLibrary` at :9455 (master) / :9503 (Vox dry); `dropWavAsClip`
  at :9478, :9506, :9512, :9517. **REC-01 confirmed still open by user**
  (calls present but library doesn't show the entries).
- **MIX-03 nature**: NOT an auto-spawn-at-recording bug. Recording correctly
  attaches to Vox strip; on save→reload the Vox strip disappears (MIX-02
  cause) and the orphan recording on the Builder grid becomes a clips strip
  on next load. **MIX-03 = symptom of MIX-02; fixes together.**
- **Effects-page dropdown** ([EffectsPage.cpp:28](Source/Standalone/EffectsPage.cpp:28)):
  `onInstrChannelListChanged` callback wired but doesn't fire on tab-close
  cascade in practice. **MIX-07 confirmed still open by user.**
- **WAV-clip stretch** ([BuilderPage.cpp:3505-3509](Source/Standalone/BuilderPage.cpp:3505)):
  block resize doesn't call `rebuildAudioClipPlayers()`. **BUILD-06 confirmed.**
- **Idle-suspend gate** ([InstStripTask.cpp:115-119](Source/Engine/Tasks/InstStripTask.cpp:115)):
  missing `auditionPending = eng->mAuditionNote.load() != -1` predicate.
  **DSP-10 confirmed.**
- **Bus solo** ([VibeGraph.cpp:358 Layers](Source/VibeGraph.cpp:358)):
  current per-group anySolo formula doesn't match observed behavior (Drums
  plays when Layers solos despite formula including `drumSolo`). User-specified
  target behavior: solo a bus → that bus + everything routed into it plays;
  every OTHER bus silenced at master mix. **DSP-09 confirmed open with
  specified behavior.**
- **Right-click → Automate** ([SharedUI.cpp:1751-1794 VKnob::mouseDown](Source/Standalone/SharedUI.cpp:1751)):
  outer right-click correctly gated. The bug is JUCE's default `PopupMenu`
  accepting any mouse button as item-activation — needs wrapper. **UI-01
  confirmed open.**
- **Automation lane UUID resolver** ([StandaloneEditor.cpp:2538-2544](Source/Standalone/StandaloneEditor.cpp:2538)):
  reachable from `sOnAutomate` at :2416-2420. **UI-02 confirmed still open
  by user.** Diagnose with UI-01.
- **Dead Properties duplicate** ([BuilderPage.cpp:2561](Source/Standalone/BuilderPage.cpp:2561)):
  `m.addItem(7, "Properties...");` added unconditionally for all block types,
  no `case 7` in switch. **Dead. Delete this line entirely.**

---

## 2. Verify-Before-Touching List

Most items the user has confirmed still open during this triage round.
Only two remain in pure verification mode:

| Item | Action |
|------|--------|
| **DSP-07** (single observed silent-first-drop, didn't repro) | Watch-item only. No code action. If it surfaces again, suspect one-block routing-graph rebuild latency. |
| **DSP-12 verification matrix** (after QA-0 fix) | Test all 4 cases: {WAV, MP3} × {drop on Builder, drop on Clips tab}. User confirmed MP3 hits the same auto-spawn cascade as WAV. |

All other items previously listed for verification are confirmed still open
and folded into their respective batches below.

---

## 3. Items to Park / Defer / Fold

Math first, since the v1 number was wrong:

- Total entries in unified backlog: **64**
- Active queue (the 59 work items, ignoring optional + never):       **59**
- Long-horizon optionals (OPT-01..04 + softened NEVER-01):           **5** (kept on the long-horizon shelf, see below)

Of the 59 active items:

- **Folded** (resolves with another item, no separate work):
  - **MIX-03** → folds into MIX-02 fix (symptom of Vox-strip-disappears-on-reload).
  - **STATE-03** → folds into APP-03 (modal load progress dialog cures the symptom).

- **Parked** (no batch unless conditions change):
  - **DIAG-01** — synthetic test for `rebuildLinks`. Internal `jassert` provides enough coverage. Park unless a real test-infra batch surfaces.
  - **APP-01** — shutdown wait climbing. Test-scenario inflation, not a real bug. Park unless shutdown timing logs show >10 sec on a clean session.
  - **DSP-07** — single observed silent-first-drop, didn't repro. Watch-item only.

- **Long-horizon optional** (deferred but not killed — revisit if circumstances change):
  - **OPT-01** — per-stage parallelism inside a strip. Unlikely needed at 17-task DAG; revisit if profiling shows we want finer grain.
  - **OPT-02** — worker thread priority elevation. Add cautiously after MT runtime data shows measured benefit.
  - **OPT-03** — TSAN integration in CI.
  - **OPT-04** — replace serial path entirely. Estimate 6+ months post-Batch 10.
  - **NEVER-01** — per-band EQ parallelism. Current measurement says overhead > benefit at 8 bands. **Not actively planned; not blocked from future reconsideration if EQ topology changes (e.g. more bands, dynamic EQ adds heavier per-band cost).**

That gives **active queue = 59 − 2 folded − 3 parked = 54 items addressed across 15 batches**, plus the 5 long-horizon items kept on the shelf. **DIAG-02 stays in the active queue per user — full sum-of-cores DSP meter is required (lands as QA-N).**

---

## 4. Items Needing Design Call BEFORE Implementation

Resolved during triage:
- **DSP-09 Bus Solo** — user specified target behavior (solo a bus → that
  bus + incoming strips plays; other buses silenced at master).
- **DSP-12 fix shape** — user confirmed Composite RenderTask.
- **FILE-02 routing dropdown** — user confirmed Vox + Inst + Clips options.
- **FILE-02 reassignment timing** — user confirmed Immediate.
- **NAV-04 Piano Roll buttons** — deep-link buttons keyed to active piano-roll
  dropdown selection (visually match standard nav buttons).
- **NAV-03 FX Rack button** — routing per page type:
  - Layers/Bass/Vox/Inst pages → that player's per-strip FX rack
  - Individual drum tab → that drum's individual FX rack
  - Drum Kit (sequencer) page → that kit's drum bus rack
  - Rusty's main page → Rusty's drum bus rack
- **NAV-05 Builder hamburger** — remove. Reclaim vertical space.
- **MIX-03** — symptom of MIX-02; not a separate design call.

**No outstanding design calls remain.** Plan can execute end-to-end without
further blocking input from user.

---

## 5. Proposed Batching Structure

15 batches grouped into 5 phases. Phase 1's three batches can run in
parallel (different code surfaces); within each later phase, batches are
sequential per Option A.

**Convention (2026-05-07):** when a per-batch plan file is created
(start of any batch), its absolute path is recorded as a bold
**`Plan file:`** line directly below the batch's `####` header. This
makes the file lookup one scan away when reviewing §5 — no grep
needed to find what you should pull up to review the work.

### Phase 1 — Critical regression fix + fast wins

#### **QA-0a: Debug Build Workflow Setup**  (forked in 2026-05-07 — see §9)
**Plan file:** `Batch Plans\i-want-you-to-adaptive-dongarra.md`
- Items: workflow infrastructure (no items from the unified backlog).
- Scope: modify `do_build.bat` to build BOTH Release and Debug
  configs; gate the embedded exe icon for Release-only so Debug
  exe shows the generic Windows .exe icon (taskbar pins
  differentiate); append " [DEBUG]" to the window title in Debug
  builds; cold-start triage of existing `jassert` calls; document
  the new workflow in CLAUDE.md.
- Risk: low. Build infrastructure, no audio code.
- Dependencies: none. Runs first in Phase 1.
- Effort: small-medium (~2 hours). Triage is the variable.
- Why before QA-0: QA-0 ships a `jassertfalse` tripwire on the
  dispatcher's most-recent-wins fallback. Without QA-0a's Debug
  build, that tripwire is compiled out of Release and never fires
  in user workflow. QA-0a makes the tripwire actually useful.

#### **QA-0: MT Composite RenderTask (DSP-12 restore)**  ⚠️ TOP PRIORITY
**Plan file:** `Batch Plans\composite-merging-rivers-twilight.md`
- Items: DSP-12.
- Scope: build a `CompositeAudioInsertTask` that owns BOTH render flows
  (`AudioInsertTask` arrangement-timeline path + `ClipPageTask` sampler-MIDI
  path) and sums them internally before insert DSP. Replaces the
  most-recent-registration-wins behavior at `audioInsert(N)` channel ids.
  - Decision contract: when a Clips-tab page IS auto-spawned for a row,
    use Composite (both flows contribute). When only one is present
    (no Clips-tab page, or no Builder-grid clip), the active flow runs
    standalone within the composite.
  - Critical files: new `Source/Engine/Tasks/CompositeAudioInsertTask.h/.cpp`,
    edits to `RenderGraphDispatcher::registerTask` ([dispatcher .cpp](Source/Engine/RenderGraphDispatcher.cpp)),
    edits to spawn cascade in `StandaloneEditor::createBuilderPage`'s
    `onAudioClipAdded` ([:1914-1949](Source/Standalone/StandaloneEditor.cpp:1914))
    + `spawnClipsTabIfMissing`.
  - Reuse pattern: existing `AudioInsertTask` and `ClipPageTask` `run()`
    bodies become helpers invoked by the composite.
- Risk: medium-high. MT-only audio path. Must preserve serial vs MT parity
  (serial path already sums them — composite restores MT to match).
- Dependencies: none. First batch.
- Effort: small-medium (~3-5 hours). Composite shape is well-understood.
- Verification: full DSP-12 verification matrix below.

#### **QA-Inventory: Comprehensive Source-Doc Triage** (added 2026-05-07 via Rule 3 — see §9)
**Plan file:** N/A — executed inline from chat breakdown (per user direction; no separate per-batch plan file in `Batch Plans/`).
- Items: review + categorize every distinct entry across the three pre-QA source documents and route each per its bucket's rules.
- Source docs:
  - `Files For Claude/Final Stretch Work.txt` (~886 lines)
  - `Files For Claude/vibedaw_blueprint.md` (~3333 lines)
  - `C:/Users/jeffm/.claude/plans/lucky-discovering-tiger.md` (~3629 lines — original location preserved as historical record per user direction; Plans & Specs/ is forward-only).
- Buckets:
  - **A** — claimed not-done, still needed → walk one-by-one with user; route per Rule 3 (fold into existing §5 batch OR new dedicated §5 batch).
  - **B** — claimed not-done, drop candidate → confirm with user; one-line entry in `Future State.md` "Considered & Dropped".
  - **C** — claimed done + source-verified → entry in `Previously Implemented.md`.
  - **D** — Claude future-state additions across Audio Quality / Performance / User Tools / Workflow Polish → entries in `Future State.md`. User direction: every plausible idea, not bounded.
  - **E** — claimed done but unverified → walked with user alongside A; per-item Decision dropdown: Update (= reroute to A as work-to-do) / Archive (= reroute to C as verified) / Delete (= folds into Phase 6 cleanup carry-list).
- Workflow: parse → initial bucket → create Google Sheet via Drive connector with all rows pre-filled + frozen-row legend of valid Decision values → walk A/B/E with user (checkpoint pings; Claude downloads CSV to read decisions back) → source-verify C in parallel → fire-hose D → apply decisions to Main Plan §5/§9 + `Previously Implemented.md` + `Future State.md` → close with §9 Forks routing entry.
- Source verification depth: headline-verify every claim (file/class/function exists) + sample 20% of named sub-items per claim + escalate to full verification on any miss + skip anything already verified in Carry-Forward Reference.
- Risk: zero — read-only triage + doc updates; no source code changes.
- Dependencies: QA-0 closed (✓ 2026-05-07).
- Effort: large (~10–15 hours, multi-session expected).
- Why this slot: pre-QA source docs intermix shipped work, claimed-but-unverified work, abandoned work, and future-state ideas without disambiguation. Without the triage, every downstream batch's plan author re-parses the same ambiguity. Done as a dedicated batch (vs scattered across each downstream batch start) because the parse work is identical across docs and most efficient as a single sweep. Also populates the new `Plans & Specs/` doc skeletons (`Previously Implemented.md`, `Future State.md`) created in commit `c05ce61`.

#### **QA-Md: MT Engine Debug-Build Investigation** (added 2026-05-07 via Rule 3 — see §9; promoted from Phase 5 to Phase 1 same day)
**Plan file:** `Plans & Specs/Batch Plans/glittery-tinkering-salamander.md`

> **Outcome (2026-05-09 QA-Md close):** original "MT no-op under Debug" premise was a *misdiagnosis*.  MT engine works correctly in Debug at full efficiency (workers do 91.8% of tasks; ~48% wall-clock reduction with MT-on vs MT-off matching Release's ~40%).  The symptom that motivated the batch (DSP meter reads identical with MT on vs MT off in Debug) was DSP meter cap saturation -- both Debug-MT-on (450%) and Debug-MT-off (870%) sit above the original 200% cap and read as "200%".  No fix shipped to the MT engine itself; Phase 3 of the per-batch plan was skipped.  Diagnostic value lives in the permanent counters + Mixer hamburger menu item.  See §9 Forks 8th entry for full diagnosis chronology + the 2026-05-09 Implemented Work Log entry for execution detail.  The §5 scope description below is preserved as the historical framing the batch started from -- read it as "what we set out to investigate," not "what's still true."
- Items: QA-0a/QA-0 finding #9 — MT engine is a no-op under Debug
  builds.  DSP meter reads identical with MT on vs MT off; settings.xml
  persistence works; toggle UI works; but the dispatcher isn't actually
  distributing work to threads.  Workers aren't picking up tasks OR the
  MT branch is degrading to single-thread silently.  Release MT works
  fine (verified during Batch 10 ship + QA-0 12-case matrix).
- Scope: diagnostic investigation — determine the Debug-only failure
  point.  Likely candidates:
  - `VibeThreadPool` worker creation skips under Debug.
  - `dispatchBlock`'s `runUntilOrTimeout` returns immediately under
    Debug timing (workers can't keep up).
  - A compile-time gate elsewhere short-circuits the MT branch.
- Why this slot: QA-0 stood up the diagnostic Debug build; the whole
  point of that infrastructure is MT-aware diagnosis going forward.
  Every downstream batch that touches audio (most of them) needs Debug
  to actually engage MT to surface MT-specific bugs precisely.
  Without QA-Md, downstream batches fly blind on MT in Debug.
- Risk: low-medium. Diagnostic-first; fix scope depends on what's
  found.  No audio path changes likely.
- Dependencies: QA-0 (Composite must be in place so dispatcher work
  has tasks to distribute).
- Effort: small-medium (~2-4 hours diagnostic + ~1-2 hours fix
  depending on cause).

#### **QA-A: STYLE Cluster — Unified TitleBar Component** (parallel with QA-0)
**Plan file:** `Plans & Specs/Batch Plans/twinkling-herding-twilight.md`
- Items: STYLE-01 (BaySickPlayer ribbon tab text clips), STYLE-02 (logo size/font
  standardize), STYLE-03 (Vox "Page Controls" → "BaySickVocals"),
  STYLE-04 (Inst "BaySickGuitars" between player + sub-tabs),
  STYLE-05 (extra black bar on BaySickNAM/IR), STYLE-06 (Synth/Bass preset
  dropdown to right + green title logo).
- Scope: build `Source/Standalone/BaySickTitleBar.h/.cpp` with standardized
  height/font/flex layout + per-engine color slot. Refactor each player page.
- Risk: very low. UI-only.
- Dependencies: none.
- Effort: medium (~4-6 hours).

#### **QA-B: Verification Sweep** (deferred to after QA-E — see §9 tenth Forks entry)
- Items: DSP-07 + DSP-12 verification matrix.
- Scope: NO code changes. Diagnostic session.
- Risk: zero.
- Dependencies: QA-0 must land before DSP-12 matrix can be exercised. **As of 2026-05-10**: QA-E must also land first so mute-isolation testing of the DSP-12 simultaneous case is available (findings #16a / #16b / #21 — pattern row mute no-op, right-click block mute no-op, track row mute permanent — all routed to QA-E).
- Effort: small (~1-2 hours).
- **Sequencing note (2026-05-10):** Deferred from Phase 1 (after QA-A) to immediately after QA-E. Without QA-E's mute-dispatch fixes, the DSP-12 simultaneous case (Builder + piano roll both placed) can only be verified via "two distinct audio contents + meter inspection" rather than the canonical mute-A → only-B → unmute → both-sum → mute-B → only-A isolation check. Deferring buys methodologically-sound verification on a known-good substrate. DSP-07 (parked watch-item) defers with the rest of QA-B per user spec call — gives a longer observation window for any post-QA-E reproduction. See §9 tenth Forks entry.
- **Test premise correction + sequencing decision (2026-05-11):** the original DSP-12 simultaneous-case verification ("Builder + piano roll both placed, both play summed") tested that both play simultaneously, but did NOT test that both play through the SAME chain.  Under the current implementation the two paths flow through different inserts (piano-roll-triggered Clips → Clips InsertNode; grid-placed Clips audio → row audio insert).  Intended design is unified routing — one clip, one chain regardless of trigger source.  Routing unification fix is folded into QA-J (see §9 thirteenth Forks entry, amended 2026-05-11).  **Sequencing decision (resolved 2026-05-11 — Option A per user spec call):** QA-B slides entirely to after QA-J close (vs. splitting into single-flow cells after QA-E + simultaneous case after QA-J).  The corrected premise ("both play simultaneously THROUGH THE SAME Clips engine + InsertNode chain") requires unified-routing source which lands in QA-J.  Sequencing-arrow + footnote updated (§6 arrow + footnote *******).  See §9 fourteenth Forks entry.

#### **QA-C: Tiny One-Liners**
**Plan file:** `Plans & Specs/Batch Plans/cozy-mend-ferret.md`
- Items: DSP-10 (idle-suspend audition wake — predicate fix at
  [InstStripTask.cpp:115-119](Source/Engine/Tasks/InstStripTask.cpp:115) +
  Rusty equivalent), MIX-01 (Vox-tab `onTabClosed` missing `removeVoxChannel`
  call — mirror Inst branch at :3631).
- Scope: two tiny single-file patches.
- Risk: very low.
- Dependencies: independent.
- Effort: tiny (~1 hour total).

### Phase 2 — Project state hardening

#### **QA-D: Project State Reset**
**Plan file:** `Plans & Specs/Batch Plans/federated-bouncing-cupcake.md`
- Items: STATE-01 (dirty flag triggers on load), STATE-02 (Guitar/Bass
  counters don't reset on new project), STATE-04 (load while playing
  doesn't stop playback first).
  - **Folded in 2026-05-07 (QA-0a/QA-0 finding #8 via Rule 3)**:
    MenuBarModel listener-dangle during `closeAllDynamicTabs` cascade
    -- a shared MenuBarModel can be destroyed before all
    MenuBarComponents that reference it, fires `removeListener`
    assertion (suppressed in vendored JUCE, real fix queued here).
    Touches `PianoRollPage::unregisterEngine` + the closeAllDynamicTabs
    teardown ordering.  See §9 Forks first entry + 2026-05-07 implemented
    log.
- Scope: `StandalonePlayHead::stop()` at top of project-open path
  (STATE-04). APVTS listener-silent gate around the load window
  (STATE-01). `resetProjectState()` helper invoked from `closeAllDynamicTabs`
  (STATE-02).  **Plus (folded):** ensure MenuBarModel outlives any
  MenuBarComponent that references it during the closeAllDynamicTabs
  cascade -- likely move ownership of the shared MenuBarModel to a
  longer-lived parent OR clear each component's model before the
  model itself is destroyed.
- Risk: medium. Project-load critical path.
- Dependencies: none.
- Effort: medium (~4-6 hours; folded MenuBarModel item adds 1-2 hours).
- MT-awareness: verify `mProjectLoadInProgress` barrier still engages
  with the new playhead-stop step.

### Phase 3 — Vox/Inst lifecycle + DSP cluster (the big consolidated batch)

#### **QA-E: Vox/Inst Lifecycle + Recording + FILE-02**

*(DSP-09 struck 2026-05-15 — moved to new batch QA-Ea; see §9 nineteenth Forks entry + the DSP-09 line below.)*
**Plan file:** `Plans & Specs/Batch Plans/phantom-recording-mongoose.md`
- Items consolidated per user direction (Q6 = Option A bundling):
  - **MIX-02 / MIX-04 / MIX-06** — Vox/Inst tab reload destroys mixer
    strip + phantom strips on reload (MIX-03 falls out as side effect).
  - **REC-01** — Vox/Inst recording library hand-off broken.
  - **FILE-01** — Vox wet + Vox dry + Inst dry recordings should appear
    in the Audio browser panel (Vox / Inst categories) so the user can
    drag any of them onto the Builder grid.  Currently none appear in
    the browser because page-bound paths aren't `audioLibrary`-
    registered (Vox wet, Inst dry) and Vox dry isn't page-bound at all.
    Fix: extend `VoxPage` with a `dryClipPath` slot + setter; recording
    finalize binds dry to the Vox page in addition to wet;
    `addAudioToLibrary` called for every recorded file at finalize;
    browser walk emits one `CategorizedAudioEntry` per non-empty
    page-bound path (Vox = wet + dry, Inst = dry).  Wet/dry tag flows
    naturally via the filename suffix ("- DRY.wav" / "- WET.wav" baked
    at record time, [Source/PluginProcessor.cpp:3490](Source/PluginProcessor.cpp:3490)
    / :3501); existing `renameAudioAt` + `ensureUniqueBrowserName`
    titling system handles user-renames + uniqueness checks unchanged.
    Drag-from-browser behavior: dry and wet are first-class audio
    clips, both droppable on the grid, both routable to a Vox/Inst
    page; multiple clips routed to the same page all pass through that
    page's signal chain.  Reworded 2026-05-11 (original "Vox wet
    delete should land in browser bin" framing was a wording drift
    from the underlying Master QA Backlog entry "does not land in
    browser panel"; no "browser bin" / `RetirementQueue` UI is being
    introduced).
  - **DSP-09** — **MOVED to QA-Ea (2026-05-15; see §9 nineteenth Forks
    entry + running-notes §34-§40).** Bus solo (solo a bus → that bus +
    incoming strips play; other buses silenced at master mix) plus the
    Layers/Bass/Drums bus-output-path unification are now QA-Ea's scope.
    The Task 6 pre-task spec-call surfaced a Phase-1-vs-5F-4b legacy-split
    topology (Layers/Bass/Drums use a bespoke dedicated-buffer master sum;
    every other bus uses the generic routeInsertOutput→kMaster path) that
    makes the correct fix a hot-path audio-engine refactor warranting its
    own plan + its own /review-batch.  QA-E no longer touches bus-solo.
  - **FILE-02** — Multi-recording on a single player page via
    Properties-popup routing dropdown (Vox / Inst / Clips, defaulting to
    creating page; immediate rebuild on change).
  - **Dead Properties cleanup** — delete [BuilderPage.cpp:2561](Source/Standalone/BuilderPage.cpp:2561).
  - **Folded in 2026-05-07 (QA-0a finding #13 + QA-0 finding #14 via Rule 3); audit-expanded 2026-05-11 (Sub-Phase A -- see §9 twelfth Forks entry)**:
    use-after-free crash family in `StandaloneEditor::showPageForTab`
    lambdas.  Originally captured 2 sites (#13 InstPage at line 4135,
    #14 ClipsPage Piano Roll button at line 4048).  Audit at QA-E open
    2026-05-11 (triggered by user repro of the DrumPage "Drum Kit"
    sub-tab crash, same family) expanded to **all 7 page-type branches**
    in `showPageForTab`: LayersPage 4080, BassPage 4114, ClipsPage 4156,
    VoxPage 4197, InstPage 4239, DrumPage 4300, BaySickRustyDrumsPage
    4334.  Each branch's `mPageMenuBar->setTabSlots(...)` callback
    captures the page's raw pointer into the onClick lambda; page
    destruction between dispatch and fire (engine swap, project reload,
    tab delete + re-add) leaves the lambda holding a dangling pointer
    -- next click crashes.  Secondary finding: 5 of 7 branches have an
    "inner SafePointer" pattern (ClipsPage 4144, LayersPage 4070,
    BassPage 4104, VoxPage 4185, InstPage 4219, DrumPage 4287) that's
    ALSO wrong -- the SafePointer is constructed inside the lambda body
    from a raw `xp` that may already be dangling at fire time.  Fix
    (locked 2026-05-11 as **C-i**, matches existing convention in the
    same file): lift `juce::Component::SafePointer<XxxPage> safe (xp);`
    to outer scope right after `dynamic_cast`; replace every
    `[this, xp, ...]` capture with `[this, safe, ...]`; inside lambdas
    use `if (auto* p = safe.getComponent()) { ... }`.  Mechanical,
    branch-symmetric.  DrumPage repro user-confirmed 2026-05-11;
    LayersPage / BassPage / VoxPage / BaySickRustyDrumsPage vulnerable
    per audit but untested by Jeff yet.  Confirmed #40 (QA-A close
    re-sighting) + #55 (QA-D close re-sighting) are the same family;
    no new captures needed.
  - **Folded in 2026-05-07 (QA-0 findings #16a + #16b + #21 via Rule 3)**:
    pattern row-level mute + per-pattern-block right-click "mute" + audio
    track row mute (with audio clip) all have asymmetric / non-functional
    behavior.  Pattern dispatch ignores both row and block mute states;
    audio row mute is sticky / no way to unmute.  Same surface family as
    DSP-09 bus solo (mute/solo dispatch).  Touch:
    `PatternManager::isRowAudible`, MIDI dispatch loop in
    `processBlock`, audio-row mute UI binding.
  - **Folded in 2026-05-08 (QA-Inventory close via Rule 3)** — REC-01 scope expansion:
    - **BLU-470** "Audio recording findings" — document master mix + per-track arm + debug pops; verify recording lifecycle works end-to-end.
    - **Vox recording not playing on Builder after recording** (QA-Inventory walk runtime test) — Vox strip records audio successfully, file lands in library, but builder grid playback shows the clip silent. Likely related to FilePlay routing (`mForcePitchBypass=true` set on FilePlay paths but never cleared after stop) OR the auto-spawned ClipsBus path stealing the audio (DSP-12 family).
    - **Inst recording not playing on Builder after recording** (same issue, also tested) — same surface as Vox; covers the parallel Inst path through `BaySickGuitars` / `BaySickBasses` / `BaySickPedals` chain.
    - **Pedalboard presets don't work** (QA-Inventory walk runtime test) — preset save/load for `BaySickPedalsProcessor` either round-trips wrong slot configuration or doesn't restore parameters. Same surface family as REC-01 (engine-level state restoration). NOTE: this also expands QA-Verify scope to verify ALL preset paths across all engines (Harmless, BaySickPlayer, BaySickSynth, BaySickBass, BaySickPedals, BaySickVocal, BaySickGuitars, BaySickBasses, BaySickRustyDrums, BaySickNAMIR).  **Re-routed 2026-05-11 to R3B-i:** all preset-related work (including the BaySickPedals fix) moves to QA-Verify; QA-E does not touch preset code.  REC-01's Vox+Inst-not-playing sub-items are R-2-a subsumed by FILE-01 (browser visibility fix resolves the playback path); BLU-470 sub-item is R-1-c (documentation + verify + fix anything verify surfaces) executed inside QA-E REC-01 surface as it touches the same recording lifecycle code.
  - **Folded in 2026-05-11 (QA-D close NIT carry-forward via §9 eleventh Forks entry)** -- **Sub-Phase Z: QA-D NIT corrections**.  Three QA-D NITs returned by `/review-batch` at close on 2026-05-10 that were bulk-deferred without Jeff's per-finding call (violated `feedback_qa_batches_fix_bugs_dont_defer.md`).  Three are scope-completion gaps in QA-D's own commits and fold here as a final sub-phase sequenced after Dead Properties cleanup and before the close sequence:
    - **NIT-1** `BaySickRustyDrumsPage` missing from `onTabRenamed` page-type dispatch ([Source/Standalone/StandaloneEditor.cpp:1263-1315](Source/Standalone/StandaloneEditor.cpp)); QA-D Task 2.8 dispatch landed for 6 page types but omitted this 7th.  Add the dispatch branch (1-line mechanical addition mirroring the other 6).
    - **NIT-2** `restoreAudioStripsFromArrangement` `clearDirty()` assumes load-path-only callers; inline comment is documentation not a guard.  Add a `loadContext` bool parameter OR `jassert` that all callers are load paths (defensive guard against the trap if a non-load caller is ever added).
    - **NIT-3** Legacy `"Drums"` / `"Layers"` / `"Bass"` tab names from pre-QA-D saved projects don't bump the STATE-02 monotonic counters; loading an old project leaves counter at 0 and next user-added tab collides.  Extend `advanceCountersFromRestoredTabs` parser to handle no-number-tail legacy names (bump counter to at-least-1 on each legacy match).
    - NIT-4 (per-page `LayersPage::setTabName` dead-`mPianoRoll` writeback) is dead-code shape and routes to **QA-Cleanup-1** (Phase 6 source cleanup), NOT QA-E.
  - **Folded in 2026-05-12 (QA-E Task 3 verify finding via Rule 3; see §9 sixteenth Forks entry)** -- **Task 3 audio-routing root cause: MT pre-scan gap + Clips-strip restore guard.**  MIX-02/04/06 root cause is NOT page-lifecycle (F-A K-6 Vox mirror) NOR routeChannel persistence (R-1) alone -- it's the FilePlay pre-scan being orphaned by the MT branch insertion at [Source/PluginProcessor.cpp:1874](Source/PluginProcessor.cpp:1874).  Fix lifts the pre-scan BEFORE the MT branch.  Companion fix: Clips-strip route-guard at [Source/Standalone/StandaloneEditor.cpp:9811](Source/Standalone/StandaloneEditor.cpp:9811) prevents phantom Audio strips for Vox/Inst-routed blocks on reload.  All four fixes (F-A + R-1 + Fix 1 + Fix 2) verified working 2026-05-12.
  - **Folded in 2026-05-12 (QA-E Task 3 mid-verify finding via Rule 3; see §9 sixteenth Forks entry)** -- **Task 9: Dirty-flag investigation (record-finalize side effect)**.  Post-record + save still shows dirty on reopen.  Did NOT happen until the WAV files were on the Builder grid (i.e., post-`commitRecordingResult`'s `markDirty()` call at [Source/Standalone/StandaloneEditor.cpp:9979](Source/Standalone/StandaloneEditor.cpp:9979)).  Pattern resembles a QA-D STATE-01 regression -- something re-flips dirty after save.  Scope: investigate root cause + fix + verify across all three record modes (Vox-only, Inst-only, Vox+Inst combined).  Inserted as Task 9 in the batch plan, between Task 8 Sub-Phase Z and the close sequence (which renumbers to Task 10).
  - **Folded in 2026-05-12 (QA-E Task 3 user feature request; see §9 sixteenth Forks entry)** -- **Task 7 sub-bullet: "Add a new Page" options in Routing dropdown.**  FILE-02's Routing dropdown (Task 7 step 2) extends to include "Add a new Clip Page", "Add a new Vox Page", "Add a new Inst Page" entries so the user can route a clip to a newly-created page without first navigating to the ribbon to add a tab.
  - **Folded in 2026-05-12 (QA-E Task 4 plan-review architectural finding via Rule 3; see §9 seventeenth Forks entry)** -- **Task 4 scope expanded to library-driven page-owner model.**  Original plan's `mClipPath` + new `mDryClipPath` shape was single-take-per-page; user surfaced that multi-file-to-one-page was always the intent.  Expanded scope: add `pageOwnerChannelId` field to `AudioLibraryEntry` ([Source/PatternManager.h:528](Source/PatternManager.h:528)); library becomes single source of truth for "files routed to this page"; browser walk groups library entries by ownerChannelId.  Vox/Inst `mClipPath` deleted (engine-irrelevant).  Clips `mClipPath` retained transitionally for sample-player preload (deletable post-QA-J).  Same model unblocks Task 7's multi-route Properties dropdown intent + future multi-take recording.
  - **Closed at QA-E close 2026-05-17 (Task 1 / M1 disposition via §9 twenty-first Forks entry)** -- mute findings #16a / #16b / #21 verified **no-longer-reproducible** (8/8 Debug+Release PASS; `git log -L` provenance: dispatch gates pre-date the QA-0 captures by months; verify-only commit `57f8edd`, no source change).
  - **Routed at QA-E close 2026-05-17 (Task 7 §60 finding via §9 twenty-second Forks entry)** -- dead flat-list `BrowserItem::Kind::Audio` choke / rename / switch paths in `BrowserPanel::showItemContextMenu` (found during Task 7 Choke-Group verify; pure dead code post-FILE-01) routed to **QA-Cleanup-1** with the source-verified `renameAudioAt` shared-use pre-delete guard.
- Scope: SINGLE coordinated batch because all touch the MixerPage spawn
  cascade + project XML restoration walker + StripRecorder finalize +
  bus DSP path. Splitting causes merge churn. Walk the full Vox/Inst
  lifecycle (create → save → close → reopen → load → record → reassign
  recording target → reload → delete) and fix every break in one pass.
  - Critical files: `MixerPage.cpp` (spawn cascade), `StandaloneEditor.cpp`
    (XML restore walker, `onTabClosed`, recording finalize, automation
    UUID resolver context), `VibeGraph.cpp` (bus solo logic + processBus),
    `BuilderPage.cpp` (Properties popup edit + dead line delete),
    `PluginProcessor.cpp` (rebuildRoutingFromApvts on FILE-02 reassignment).
- Risk: highest of any batch. Multi-file, multi-callback, audio + project
  serialization + bus DSP. Most likely to break unrelated paths.
- Dependencies: QA-0 (FILE-02's "fix Clips setup so they work in both
  places" depends on Composite RenderTask), QA-D (clean project load
  baseline).
- Effort: large (~12-16 hours, possibly multiple sessions; bumped from
  ~10-14 hours per QA-E open scope expansion 2026-05-11 -- Sub-Phase A
  crash family expanded from 2 to 7 sites adds ~1.5 hr, Sub-Phase Z
  QA-D NIT corrections adds ~1-2 hr, BaySickPedals + all-engines preset
  audit removed via R3B-i routing to QA-Verify).
- Trade-off: COULD split into QA-E1 (Vox/Inst lifecycle MIX-02..06 + REC-01),
  QA-E2 (DSP-09 bus solo standalone), QA-E3 (FILE-02 multi-record routing).
  User confirmed bundled (Q6 Option A). Re-evaluate at start if scope feels
  off; the split is mechanical.  **2026-05-15: the QA-E2 (DSP-09) split was
  actualized as the standalone batch QA-Ea below — DSP-09 is no longer in
  QA-E.**

#### **QA-Ea: Bus Solo + Layers/Bass/Drums Output-Path Unification (DSP-09)** *(NEW — inserted 2026-05-15)*

**Plan file:** `Plans & Specs/Batch Plans/polished-snuggling-token.md`
- Items: **DSP-09** (bus solo) + **Layers/Bass/Drums bus-output-path
  unification** (the architecturally-correct fix that removes the
  bus-solo bug class, not just the instance).  Punted out of QA-E Task 6
  at the Task 6 pre-task spec-call (see §9 nineteenth Forks entry +
  running-notes §34-§40 for the full diagnosis).
- Scope:
  - **Bus-solo fix.** Replace the three scattered/inconsistent solo
    formulas (BaySickGraph `BusNode::processBlock` triad at
    `VibeGraph.cpp:355-363`/:522/:682; `processBus` `useGroupSolo` +
    ClipsBus 6-bus + Rusty-standalone variants at :1698-1702/:1727-1728;
    receive-group `busAnySolo` at `PluginProcessor.cpp:2572-2577` which
    excludes layers/bass/drums) with ONE shared `VibeGraph::anyBusSoloed()`
    helper reading **bus `_solo` params only** + a single formula
    `silenced = thisMuted || (anyBusSoloed && !thisBusSoloed)`.  Locked
    sub-calls: **A** mute-wins; **B1** direct-to-Master bypass routes are
    NOT silenced by bus solo (gating `masterExtra` wholesale would zero
    the soloed bus's own output — it routes through that same accumulator);
    **C** multi-bus solo is additive; **D** per-strip `_solo` is already
    global via `isAnyInsertSoloed()` and is NOT touched.
  - **CRITICAL GUARDRAIL** (`/review-batch` must verify): the new helper
    must NEVER read `isAnyInsertSoloed()` (strip-level) — prior serial
    bug muted whole buses when one strip soloed; warned at
    `VibeGraph.cpp:1876-1885`.
  - **Output-path unification (Option 2).** Route Layers/Bass/Drums bus
    outputs through `routeInsertOutput`→kMaster like every other bus;
    delete the bespoke `layersBuf`/`bassBuf`/`drumsBuf` master sum
    (`VibeGraph.cpp:1547-1570`).  This collapses the Phase-1-vs-5F-4b
    legacy split so there is ONE output path + ONE gate site.
- Own plan file (`Plans & Specs/Batch Plans/<silly-name>.md` when
  started) + its OWN `/review-batch` pass specifically targeting the
  hot-path safety surface.
- Risk: **high** — hot-path audio-engine refactor.  Touches the
  master-sum, the MT `MasterTask` structure, and the `BusNode` buffer
  model.  Mandatory `/review-batch` before close per Jeff's safety call.
- Dependencies: QA-E (same code area + same test material; clean
  recording/strip surface from QA-E's MIX-02..06 + FILE-01/02 work).
- Sequencing: **adjacent to QA-E** (Jeff's confirmed slot — runs right
  after QA-E so it's tested with the same setup in a slightly different
  order).  See §6 arrow.
- Effort: medium-large (~8-14 hours; bus-solo helper ~3-4 hr, L/B/D
  output-path unification + MT MasterTask rework ~4-7 hr, verify ~2-3 hr).
- Verify (own plan file will detail): the 5 DSP-09 scenarios from the
  old QA-E Task 6 list (solo Layers; unsolo; multi-bus solo; solo+mute
  interaction = mute wins; persistence across save/load) + a regression
  pass that the L/B/D output-path rewrite didn't change non-solo mix
  output (bit-compare a no-solo render before/after).

#### **QA-Eb: Standalone App-Window Resizability** *(NEW — inserted 2026-05-17)*
- Items: standalone app-window user-resizability (new feature; Jeff
  request 2026-05-17 during the QA-E Task 7 verify session).  **NOT a
  carve-out of QA-L's `NAV-01`** — this is a fresh independent request;
  `NAV-01` is a separate concern and QA-L is untouched (see §9 twentieth
  Forks entry + running-notes §53).
- Scope:
  - **Resizable window + maximize.** Make the standalone
    `DocumentWindow` user-resizable with a working maximize button.
  - **Min-size clamp.** A minimum-size constraint so the window cannot
    shrink below the usable design size (content stays legible /
    operable; no sub-design squash).
  - **Outer `juce::Viewport` + scrollbars.** When the window is smaller
    than the content's design size, wrap the main content in a
    `juce::Viewport` that shows scrollbars so all controls remain
    reachable at sub-design window sizes.
  - **Self-scrolling pages opt out.** Pages that already own their own
    scrollbars (Piano Roll, Builder grid) must NOT be double-wrapped and
    must NOT fight the outer Viewport — special-case / opt those pages
    out so there is exactly one scroll authority per surface.
  - **OUT of scope (explicitly, post-V1):** any per-page proportional /
    FlexBox / Grid relayout.  QA-Eb is window-chrome + outer-Viewport
    only; it does not re-flow page internals.  A real responsive
    per-page relayout is a separate post-V1 concern and is NOT folded
    here.
- Rationale: doing this adjacent to QA-E "vastly speeds up testing for
  [Jeff] not having to go back and forth over and over again" — a
  resizable window removes window-juggling overhead from the remaining
  QA-E-adjacent verify passes.  QA-Eb and QA-E do NOT group by code
  area; the adjacency is justified purely by testing efficiency, the
  same justification basis as the QA-Ea adjacency precedent.
- Dependencies: none functionally (UI-only window-chrome change).
  Sequenced adjacent to QA-E for testing-efficiency, not a code
  dependency.
- Sequencing: **immediately after QA-Ea, before QA-F** (`QA-E → QA-Ea
  → QA-Eb → QA-F`).  Jeff's confirmed slot per
  `feedback_slot_placement_is_spec_call.md`, mirroring the QA-Ea
  adjacency precedent.  See §6 arrow.
- Risk: **low-medium** — UI-only window-chrome + outer-Viewport change.
  No audio-thread / DSP / routing surface touched.  Main risk surface is
  the self-scrolling-page opt-out (avoiding double-wrap / scroll-fight on
  Piano Roll + Builder grid) and the min-size clamp value choice.
- Effort: small-medium (~3-5 hours).
- Verify (own plan file will detail): window drags to resize smoothly;
  maximize works and restores; window will not shrink below the min-size
  clamp; at sub-design window sizes the outer Viewport shows scrollbars
  and every control is reachable; Piano Roll page scrolls via its own
  scrollbars with no outer-Viewport double-scroll / fight; Builder grid
  same; no per-page relayout regression (page internals unchanged — only
  the outer window + Viewport behavior is new).

#### **QA-F: BaySickAlign Build-Out + Vox DSP Disconnect (Cluster 1)**
- Items: DSP-02 (Vox FX bypassed), DSP-03 (Vox pitch correction does
  nothing), DSP-05 (BaySickAlign full build-out per redesign spec).
- Scope: audit `BaySickVocalProcessor::processBlock` for FX-array pipeline
  wiring + full BaySickAlign engine build-out (currently a paint-only
  shell — `BaySickAlignDSP` exists but is never instantiated; editor
  has no APVTS attachments and no DSP wiring; `applyWarp` is a
  passthrough memcpy).  DSP-02/03 likely co-occur.  DSP-05 expands
  from "verification pass on warp markers" to full editor + DSP build
  per the locked redesign at running notes
  `Plans & Specs/Running Notes/phantom-recording-mongoose.md` §13a-§13g.
  - **Folded in 2026-05-08 (QA-Inventory close via Rule 3)** — DSP-03 sub-scope expansion:
    - **Formant Preserve + Throat Shift no-op stubs** in `PitchCorrectorDSP` (`Source/DSP/PitchCorrectorDSP.cpp:326-327`): code comment says *"Formant Preserve / Throat Shift toggles are stored but DSP is no-op for H-5 -- a follow-up batch will add cepstral envelope swap"*. The UI exposes the knobs with descriptive tooltips ("Keeps the vocal character intact while correction shifts pitch... pitch-shift artifacts (chipmunk-up, demon-down)") but the DSP literally `juce::ignoreUnused (mFormantPreserve, mThroatSemis);`. UI-promised, DSP-not-delivered. Wire actual cepstral-envelope swap. Affects both realtime path (BaySickVocals tab) and offline path (BaySickPitch).
    - **BaySickVocal H-1..H-6 cluster review** (BLU-445 / BLU-608 / BLU-609 / BLU-610 / BLU-611 / BLU-612) — QA-Inventory walk reclassified all six from "claimed Done" to "Review" because the realtime pitch correction was confirmed broken at runtime (YIN tracker not detecting pitch despite live audio reaching the engine; granular shifter idles at ratio=1.0 producing "faint vibration" artifact only). Whole subsystem (skeleton + comp ext + de-esser + YIN + pitch correction + editor) needs end-to-end re-verification as part of QA-F's `BaySickVocalProcessor::processBlock` audit.
  - **Folded in 2026-05-14 (mid-QA-E BaySickAlign + BaySickPitch redesign detour close — see §9 eighteenth Forks entry)** — full BaySickAlign visual + DSP build-out.  Visual identity rebuild to escape VocAlign trade-dress (the editor source comment at `BaySickAlignEditor.cpp:8` literally reads `"VocAlign-clone visual + interaction model"`): 3-lane Leader / Follower / Output layout with Bass-green / Vox-teal / Drums-red lane colors; single always-visible right panel with Align + Pitch boxes; 6-preset combo (Loose-Align / Loose-Align+Pitch / Close-Align / Close-Align+Pitch / Tight-Align / Tight-Align+Pitch) + Save / Load Preset + preset-dirty green-dot; Mode dropdown (Loose / Close / Tight, renamed from VocAlign's ALIGNMENT RULE) drives Fine Tune knob base + Pitch box Range knob center+range.  DSP additions: `BaySickAlignDSP` instance on processor (currently never instantiated anywhere); channel-composite renderer (shared with QA-Fa + QA-Fb); `applyWarp` PhaseVocoder integration (currently passthrough memcpy); YIN pitch detection; PSOLA / Granular / Phase Vocoder pitch shifters; sync points data model; protected areas data model; render-to-bake (`Aligned/{name}_align_v{N}.wav`); render history persistence; ~20 APVTS params.  See running notes §13a-§13g for the full per-control spec.
- **Fork-out (QA-J re-verify required):** verify scenarios in this batch use sequential audio clips on the same Vox row only.  Overlapping-same-row scenarios hit the multi-clip stacking bug (DSP-06) and are deferred to QA-J re-verify.  See §9 eighteenth Forks entry.
- Risk: medium-high. Audio-thread DSP + full editor rebuild on what was a paint-only shell. MT-orthogonal at the inside-engine
  level (VoxStripTask calls engine.processBlock; the FX-array runs there). Visual redesign is mechanical but extensive; DSP build-out is the bulk of the work.
- Dependencies: QA-E (shares VoxInsertNode surface + clean recording surface).
- Effort: large (~14-22 hours; folded DSP-03 items add 2-4 hours; full BaySickAlign redesign + DSP build-out adds 8-12 hours).

#### **QA-Fa: BaySickPitch Build-Out + Audio Import**
- Items: DSP-04 (BaySickPitch audio import) + full BaySickPitch build-out per locked redesign.
- Scope: BaySickPitch moves from paint-only shell (no DSP class exists
  at all — there is no `BaySickPitchDSP` in `Source/DSP/`; editor has
  no APVTS attachments; CENTER / VARIATION / TRANS knobs do literally
  nothing) to functional editor + DSP.  Composite-mode setup (confirmed
  mid-detour): same channel-composite renderer as QA-F produces a mono
  buffer; YIN runs once over the composite; note segmenter produces
  note regions with absolute timeline positions; realtime applicator
  (Mode C) applies edits live during playback; render-to-bake writes
  `<project>/Pitched/{name}_pitch_v{N}.wav`.  Full per-control redesign
  at running notes
  `Plans & Specs/Running Notes/phantom-recording-mongoose.md` §14a-§14g.
  - Visual identity rebuild to escape Newtone trade-dress (the editor source comment at `BaySickPitchEditor.cpp:8` literally reads `"Newtone-clone visual + interaction model"`): LENGTH info bar shows `X bars / M:SS.f` (+ `SEL` variant when selection active); 2-mode Slice / Edit setup (Vibrato + Formant + Volume become per-note sub-curves under selected note rather than separate mode toggles); Pills note region in Effects-purple fill with Vox-teal waveform interior; pitch curve overlay in Bass-green (replaces Newtone's orange `#e89c5a`); 3 global knobs renamed CENTER → **Focus** / VARIATION → **Mod** / TRANS → **Speed** (labels visible); Send Notes to popup target list (active Layers / Bass / Drums / Clips tabs; only MIDI sent, not vocal audio); 3 mode presets (Loose / Close / Tight) drive the three global knobs + Save / Load Preset + preset-dirty dot; all Newtone-specific toolbar bloat removed (Load button, Slaved Playback Mode, Loop button, internal Play / Stop transport).
  - DSP additions: YIN pitch tracker; note segmentation; PSOLA / Granular / Phase Vocoder pitch shifters (shared with QA-F); vibrato analyze + synthesize; formant shifter per-note; volume envelope per-note; per-channel pitch-edit ValueTree storage; render-to-bake pipeline shared with QA-F; realtime applicator (Mode C); render history (per-channel list, separate from QA-F's); ~10-15 APVTS params.  DSP-04 (drag-and-drop file listener) lives within this redesigned editor.
- **Fork-out (QA-J re-verify required):** same as QA-F — verify scenarios use sequential clips on the same Vox row only.  Overlapping-same-row scenarios deferred to QA-J re-verify.  See §9 eighteenth Forks entry.
- Risk: medium. Editor + DSP build on a paint-only shell.  Composite-mode sharing with QA-F means architectural alignment risk if either batch diverges mid-implementation.
- Dependencies: QA-F (composite renderer + YIN + pitch shifter shared infrastructure must land first; BaySickPitch consumes those).
- Effort: medium-large (~8-14 hours).

#### **QA-Fb: Recording Lifecycle + Channel-Composite Renderer** *(NEW — inserted 2026-05-14)*
- Items: dual-buffer recording architecture; conditional WET tap; multi-take capture-bleed fix; audio-clip-resize-doesn't-stretch fix; dirty-flag investigation (page-creation + record-finalize triggers); channel-composite renderer (shared dependency with QA-F + QA-Fa, lives here as the foundational layer).
- Scope: post-QA-F + QA-Fa architecture lift — the recording flow underneath the BaySickAlign + BaySickPitch DSP work.  Per running notes
  `Plans & Specs/Running Notes/phantom-recording-mongoose.md` §2 + §3 + §7 + §8 + §9.
  - **Dual-buffer recording** lets the recorder capture the live input pre-realtime-pitch AND the FilePlay playback simultaneously through separate taps.  Fixes multi-take capture-bleed: take 2 currently captures take 1's playback through the live input loop because the single-buffer recording flow can't distinguish "what's coming in fresh" from "what's playing back through the chain."
  - **Conditional WET tap** skips the second buffer when realtime pitch is bypassed (no Vox pitch correction active → no WET to capture; saves the buffer-copy cost on every block).
  - **Multi-take capture-bleed fix** lands as the recording-finalize-side complement to the dual-buffer architecture (lifecycle path: arm → record → finalize → reload).
  - **Audio-clip-resize-doesn't-stretch fix** addresses block-state-not-propagating-to-backend-playback (same family of bug as the recording-lifecycle: visual feedback present in the UI, backend audio doesn't follow the visual state).
  - **Channel-composite renderer** is the shared dependency for both QA-F (BaySickAlign needs Leader + Follower composites) and QA-Fa (BaySickPitch needs channel composite to detect notes across all clips on the channel).  Lives in this batch as the foundational layer that both upstream batches consume.
  - **Dirty-flag investigation** covers BOTH triggers (page-creation AND record-finalize) per the Task 9 fold-out from QA-E.
- **Fork-out (QA-J re-verify required):** verify scenarios use sequential clips on the same Vox row only.  Overlapping-same-row scenarios deferred to QA-J re-verify.  See §9 eighteenth Forks entry.
- Risk: medium-high.  Recording-flow restructure + new shared composite renderer.  Affects every Vox / Inst recording path.  Test surfaces span recording-finalize, project save / reload, multi-take capture, and clip-resize.
- Dependencies: QA-F + QA-Fa (composite renderer is consumed by both; QA-Fb is the cleanest home since QA-F's DSP-build-out + QA-Fa's editor-rebuild are independent of the recording-lifecycle work and the composite renderer needs both QA-F and QA-Fa to be functional consumers before its integration is verifiable).
- Effort: large (~10-16 hours).

#### **QA-Fc: BaySickNAMIR Dual-Mic Stack** *(NEW — inserted 2026-05-14)*
- Items: dual mic-sim/placement path on BaySickNAMIR engine (new feature; lives on the audited-clean BaySickNAMIR foundation — see §9 eighteenth Forks entry's audit-outcome bullet).
- Scope: simulate two microphones on the same source rather than the current single-mic chain.  Real-recording workflow — most pro recordings use 2+ mics on guitar cabs, vocals, drums, etc.; the two mics summed produce more energy and dimension than either alone.  Existing single-mic chain becomes Mic A; new parallel Mic B path mirrors Mic A's controls.  **Output is sum (Mic A + Mic B), not blend / crossfade.**  Per running notes
  `Plans & Specs/Running Notes/phantom-recording-mongoose.md` §23 — parallel-paths-not-blend architecture is the central spec call (post-cab buffer copied to scratch; Mic A processes in-place on the main buffer; Mic B processes scratch through its own Mic Sim + Mic Placement chain; Mic B output sums back into main).  `nam_micb_active` toggle (default false) bypasses the entire Mic B path when off — byte-identical to today's single-mic chain.  Editor layout splits the existing Mic Sim + Mic Placement rows into Mic A | Mic B columns side-by-side; the existing single-mic UI squishes to half-width, second mic mirrors it on the right half.  Picks up automatically on both Vox + Inst pages (BaySickNAMIR is a shared engine module hosted as a sub-tab on each — `Source/BaySickNAMIR/`, audit confirmed clean wiring 2026-05-14).
- APVTS additions: 8 new params with `_b_` infix (mirrors existing `nam_micsim_*` + `nam_placement_*`): `nam_micb_active`, `nam_micsim_b_mode`, `nam_micsim_b_model`, `nam_micsim_b_mix`, `nam_placement_b_distance_cm`, `nam_placement_b_angle_deg`, `nam_placement_b_polar`, `nam_placement_b_mix`.
- DSP additions: 2 new `MicSimDSP` + `MicPlacementDSP` instances on the processor (Mic B path); 2 new scratch buffers (`mPreMicScratch` saves post-cab buffer state for Mic B input; `mMicBScratch` is Mic B's processing buffer that sums into main).  9 new fields in `SlotSnapshot` for per-A/B-slot Mic B state preservation.  State serialization extended for both the new SlotSnapshot fields and the new APVTS params.
- Risk: medium.  New APVTS params + new DSP path on a heavily-used engine.  Worst case: Mic B bypass logic broken — either silent Mic B (zero impact, just an unused feature) or Mic B always active (changes Mic A's behavior — would be caught immediately by ear).  Verify scenarios listed in running notes §23.
- Dependencies: clean BaySickNAMIR foundation — audit confirmed 2026-05-14 (running notes §20: all 18 existing APVTS params declared / read / wired; no follow-on wiring fixes needed, conditional QA-Fd batch dropped).  Sequencing-independent of QA-F + QA-Fa + QA-Fb (orthogonal surfaces) but slotted after QA-Fb so all engine-side dust has settled before adding a new DSP path.
- Effort: medium-large (~7-12 hours; ~2-4 hours processor changes, ~3-5 hours editor layout, ~1 hour snapshot expansion, ~1-2 hours verify).

### Phase 4 — Builder + UX work

#### **QA-G: Timeline Geometry**
- Items: BUILD-01 (100 tracks), BUILD-02 (ruler freeze), BUILD-03 (zoom
  alignment).
- Scope: refactor BuilderPage Viewport. Extract ruler from vertical
  Viewport, float-precision zoom math, bump array limit + recompute
  scrollable height.
- Risk: low-medium. UI-only.
- Dependencies: none.
- Effort: medium (~4-6 hours).

#### **QA-H: Builder Polish + Piano Roll Features**
- Items: BUILD-04 (ghost notes static), BUILD-05 ('s' keybind dead),
  BUILD-06 (WAV-clip stretch missing rebuild trigger), NAV-05 (REMOVE
  Builder hamburger), MIDI-01 (Ctrl+click row select), MIDI-02
  (control-lane dots follow slider), MIDI-03 (control-lane reference
  grid lines), MIDI-04 (Humanize tool).
  - **Folded in 2026-05-07 (QA-0 finding #15 via Rule 3)**: dropping a
    WAV/MP3 on Builder grid auto-navigates to the player page; user
    expects to stay on Builder.  Touches `onAudioClipAdded` cascade in
    `StandaloneEditor.cpp` -- remove the post-spawn tab-select.
  - **Folded in 2026-05-07 (QA-0 finding #17 via Rule 3)**: app-shutdown
    crash in `BuilderPage::~BuilderPage` -> `TreeView::~TreeView` ->
    `TreeViewItem::setOwnerView` walks dangling subItem pointer.
    Pre-existing destructor-ordering bug in BuilderPage's tree teardown
    (`BuilderPage.cpp:4418`).  Fix: ensure TreeView outlives its
    TreeViewItems OR restructure ownership so JUCE manages item lifetime.
  - **Folded in 2026-05-07 (QA-0 finding #18 via Rule 3)**: muting a
    block resets its loop count to 1 (block stays visible but silent +
    loop state lost).  Block-mute logic must preserve loop count.
  - **Folded in 2026-05-07 (QA-0 finding #19 via Rule 3)**: can't drag
    audio clips back onto Builder grid after deletion -- browser ->
    Builder drop only works for first-time imports.  Drop handler in
    `BuilderPage::ArrangementGrid::filesDropped` needs to accept
    library-resolved paths.
  - **Folded in 2026-05-07 (QA-0 finding #20 via Rule 3)**: UX gap --
    clicking a pattern / audio clip / automation in browser should make
    it the "active drop type" for clicks on empty Builder space
    (mimicking piano-roll last-block-type behavior).
- Scope: coherent UX pass on Builder + piano roll.  Plus folded Builder
  block state, drop handler, shutdown teardown, and UX click-to-place.
- Risk: low-medium. UI-only mostly. BUILD-06 calls
  `rebuildAudioClipPlayers()` (message thread, MT-aware).  Folded #17
  is a real teardown bug -- moderate risk if ownership restructure
  ripples.
- Dependencies: QA-G (timeline geometry foundation).
- Effort: medium-large (~8-12 hours; folded items add 2-4 hours).

#### **QA-I: Heavy Operation Progress Overlay**
- Items: NAV-02 (engine swap loading sign), APP-02 (shutdown overlay
  replacing black-screen), APP-03 (project load modal dialog), STATE-03
  (folded into APP-03 — symptomatic, fixed by UX), STATE-04 (already
  fixed in QA-D, gets visual feedback layer here).
- Scope: build reusable `Source/Standalone/HeavyOperationOverlay.h/.cpp`
  (modal/non-modal, step labels, progress bar, busy cursor). Wire each
  long-running operation to dispatch step updates.
- Risk: medium. Touches UI lifecycle around shutdown + load. APP-02
  window-management pattern needs careful Windows DWM testing.
- Dependencies: QA-D (STATE-04 playhead-stop) for the load-progress
  path to deliver perf benefit.
- Effort: medium-large (~6-10 hours).

### Phase 5 — Audio engine cleanup + UI polish

#### **QA-J: Multi-Clip Stacking Fix (DSP-06)**
- Scope: restructure per-row audio-clip rendering so the rack/EQ runs
  ONCE per row per block on the SUM of the row's clips, not N times per
  block.
  - **Folded in 2026-05-07 (QA-0 finding #16c via Rule 3)**: when an
    audio row is muted, the audio clip's streamer pauses at its current
    `expectedFilePos`; on unmute the streamer resumes from that frozen
    position rather than syncing to current project transport, causing
    visible desync.  Same surface (`renderAudioClipsForRow`, audio-clip
    streamer position management).  Fix likely advances streamer's
    expectedFilePos even when mute-gated, OR seeks on unmute to current
    project transport.
  - **Folded in 2026-05-08 (QA-Inventory close via Rule 3)** — BLU-501 "Prune stale applicators on swap" (memory cleanliness in audio-thread automation applicator map; memory leak on engine swap). Same surface family as the audio-thread renderAudioClipsForRow restructure.
  - **Folded in 2026-05-11 (QA-E open-time finding via Rule 3; see §9 thirteenth Forks entry — amended same day to include Clips routing unification + DSP-12 test premise correction)** — FilePlay multi-clip restructure + Clips routing unification: extend the once-per-sum restructure to the per-page engine + insert chain path across all three engine families (Vox + Inst + Clips).  Two clips routed to the same Vox/Inst/Clips page mix into one input buffer per page, then run the engine + insert chain once per page, rather than processing each clip sequentially through the shared engine state (which leaves compressor envelope / reverb tail / LFO phase from clip A bleeding into clip B's processing pass).  Touches Pass 1 loop at [Source/PluginProcessor.cpp:2415-2433](Source/PluginProcessor.cpp:2415), `renderFilePlayPlayer` at [Source/PluginProcessor.cpp:867-901](Source/PluginProcessor.cpp:867), and MT-path equivalents in [Source/Engine/Tasks/VoxStripTask.cpp](Source/Engine/Tasks/VoxStripTask.cpp) + [Source/Engine/Tasks/InstStripTask.cpp](Source/Engine/Tasks/InstStripTask.cpp).  **Clips routing unification:** Pass 1 loop's `isVox || isInst` filter expands to include Clips channels; grid-placed audio clips that reference Clips-page-loaded files default their `routeChannel` to that Clips page's channel ID (currently defaults to 0 / row audio insert).  Net result: a Clips file plays through the same Clips engine + InsertNode chain regardless of trigger source (piano roll OR Builder grid).  **Test premise correction:** DSP-12 simultaneous-case test premise updated from "both play simultaneously" to "both play simultaneously through the same chain"; QA-B's deferred re-verification runs under the corrected premise post-QA-J close.
  - **Folded in 2026-05-14 (BaySickAlign + BaySickPitch redesign detour close — see §9 eighteenth Forks entry)** — re-verify scenarios deferred from QA-F + QA-Fa + QA-Fb.  Per the Option 1 spec call (running notes §16), those three batches' verify cycles use sequential clips on the same Vox row ONLY; overlapping-same-row scenarios (multiple takes recorded on top of each other on the same row, harmony stacks, etc.) hit the multi-clip stacking bug being fixed here and were deferred forward.  At QA-J close, re-verify the overlapping-same-row cases for: (1) BaySickAlign Leader / Follower composite rendering with stacked clips on the Vox row; (2) BaySickPitch composite-mode pitch detection across stacked clips on the Vox row; (3) recording-finalize behavior when a new take is recorded onto a row that already has overlapping clips; (4) audio-clip-resize-doesn't-stretch fix's interaction with the per-row sum (resize one clip in a stacked-clip layout, verify the sum re-renders correctly).
- Risk: high. Architectural restructure, audio thread, MT-aware.
- Dependencies: QA-0 (composite task pattern established) + QA-E
  (audio clip surface stability).
- Effort: large (~13-18 hours; folded streamer-sync + applicator cleanup adds ~2 hours; folded FilePlay restructure adds ~3-4 hours; folded Clips routing unification adds ~1-2 hours; folded QA-F/Fa/Fb overlapping-row re-verify adds ~1-2 hours).

#### **QA-K: Audio Engine Polish**
- Items: APP-04 (SetPriorityClass + MMCSS), APP-05 (Open ASIO Control
  Panel button), DSP-08 (Tascam Model 24 outputs 21/22 stereo bug),
  DSP-11 (live ASIO buffer-size change), DSP-01 (Harmless lazersaw silent
  + headless preset audit test).
- Scope: small audio-system polish items. APP-04 ~5 lines in
  `VibesynthStandaloneApp::initialise`. APP-05 single button via
  `juce::AudioIODevice::showControlPanel`. DSP-08 needs hardware in
  hand. DSP-11 may end up "out of scope, document workaround".
- Risk: low-medium each.
- Dependencies: independent.
- Effort: medium (~4-6 hours total).

#### **QA-L: UI Polish**
- Items: UI-01 (right-click on PopupMenu activates item — JUCE wrapper),
  UI-02 (auto-lane "(deleted slot)" UUID — diagnose with UI-01), MIX-05
  (mixer strip overlap after delete — missing `resized()`/`repaint()`
  trigger), MIX-07 (Effects-page dropdown stale entries — verify why
  the wired callback doesn't fire on tab-close), NAV-01 (Builder grid
  doesn't line up with the tracks when the grid is resized; meaning
  clarified by Jeff 2026-05-17 — prior "window resize layout — strict
  FlexBox/Grid + min size" wording was vague; see §9 twentieth Forks
  entry.  NOT the QA-Eb app-window-resizability batch — separate item),
  NAV-03 (FX Rack button on
  player pages), NAV-04 (Piano Roll deep-link buttons), FILE-03
  (browser delete removes all duplicate-named instances — auto-numbering
  on duplicate drop).
  - **Folded in 2026-05-08 (QA-Inventory close via Rule 3)** — UI-Polish scope expansion:
    - **BLU-378** "Right-click Automate menu gap" (componentID on sliders) — UI-01 sub-item: sliders need stable `componentID` so the Automate menu can identify the target. PRESET-SAFE.
    - **BLU-379** "A9 slider-sync verify" — verification that all editor sliders use `SliderAttachment` correctly so APVTS round-trips and sync paths are consistent.
    - **LDT-394** "General UI Touch-ups (5F-8)" — Piano roll mouse accuracy + final spacing/alignment pass.
    - **BLU-492** "Combo-box automation infrastructure" — make combo-box selections become APVTS params so they're automatable. PRESET-BREAK at preset-format level (combo selections currently aren't in preset state).
    - **LDT-026** "D1.5 Per-drum MIDI input note + UI" — populate the MIDI Map placeholder in the per-drum context menu so pad-controllers can map to specific drums. Per-drum `mInputNote` field + UI.
    - **FSW-123** "Picker-disable-during-playback for Clips" — UX polish: disable engine/sound pickers while transport is playing on Clips tabs to prevent mid-playback engine swaps.
- Scope: collection of UI polish. Group so same surface touched once.
- Risk: low-medium each.
- Dependencies: independent.
- Effort: medium-large (~6-10 hours).
- Trade-off: COULD split into QA-L1 (UI-01/02 PopupMenu wrapper +
  automation UUID), QA-L2 (everything else). User stated preference for
  granular commits — recommend split if scope feels off at start.

#### **QA-M: Engine Restoration Lifecycle**
- Items: LIFE-01 (DrumKit kit-load destroys Rusty), LIFE-02 (re-add Rusty
  doesn't auto-reload kit).
- Scope: dedicated debug session in DrumPage / DrumKitGrid kit-load path
  + auto-reload kit on Rusty re-add.
- Risk: medium.
- Dependencies: independent.
- Effort: medium (~4-6 hours).

#### **QA-Drum-Polish: Per-drum MIDI Note Map (D1.5)** (added 2026-05-08 via Rule 3 — see §9)
**Plan file:** TBD.
- Items: LDT-026 (D1.5 Per-drum MIDI Note Map for pad-controller mapping).
- Scope: implement per-drum `mInputNote` field that pad-controllers can map to. Populates the MIDI Map placeholder in the per-drum context menu (Phase D D1.4-fix(c) shipped placeholder; D1.5 wires it).
- Risk: low. Per-drum APVTS param + MIDI dispatch routing.
- Dependencies: QA-M (drum lifecycle stable).
- Effort: small-medium (~2-4 hours).
- Why this slot: drum-related work cluster.

#### **QA-N: DSP Meter Sum-of-Cores (DIAG-02)**
- Items: DIAG-02.
- Scope: refine the DSP meter under MT to sum audio-thread + per-worker
  times (or use wall-clock dispatch-entry-to-mAllDone). Target: "% of
  one core" reading that tracks total render work.
- Risk: low. Read-only measurement; no audio path changes.
- Dependencies: independent.
- Effort: small-medium (~3-5 hours).

#### **QA-VibeSlider: App-wide juce::Slider → VibeSlider refactor** (added 2026-05-08 via Rule 3 — see §9)
**Plan file:** TBD (silly-name file when batch starts).
- Items: BLU-493 (App-wide refactor; PRESET-SAFE; ~150-300 sites).
- Scope: replace every plain `juce::Slider` instance across the app with `VibeSlider` (defined in `Source/Standalone/SharedUI.h:956`), which swallows right-click events. Without this, right-clicking a `LinearVertical` slider with snap-to-mouse enabled snaps the value to the click Y — UX bug whenever the user is trying to right-click to reach the Automate menu. Currently only EQ widget + DynamicParamsPopout + MixerTrackStrip pan/width/fader use VibeSlider; everything else (Harmless / BaySickSynth / BaySickBass / VibePlayer / Pedals / NAMIR / Vocal editors + all effect panels) still uses raw `juce::Slider`.
- Risk: low. Per-class subclass swap; `VibeSlider` inherits all `juce::Slider` API. Build verifies + per-page interactive sanity.
- Dependencies: independent (could run alongside any other batch).
- Effort: medium (~5-8 hours). 150-300 mechanical sites.
- Why this slot: blocks the right-click Automate workflow being usable across the app; runs late in Phase 5 because nothing depends on it but it's needed before QA-RC's UX checklist verification.

#### **QA-Verify: Phase 5A/5B/5C systems verification** (added 2026-05-08 via Rule 3 — see §9)
**Plan file:** TBD.
- Items: LDT-169 (5A Project Serialization), LDT-170 (5B Template System), LDT-171 (5C Per-Engine Preset System).
- Scope: end-to-end verification that project save/load + templates + presets work correctly across every engine (Harmless, BaySickPlayer, BaySickSynth, BaySickBass, BaySickPedals, BaySickVocal, BaySickGuitars, BaySickBasses, BaySickRustyDrums, BaySickNAMIR). Includes the **pedalboard preset bug** confirmed in QA-Inventory walk runtime testing — `BaySickPedalsProcessor` preset save/load doesn't restore correctly. Per-engine: load every factory preset, verify all params restore + audio plays as expected; save user preset, reload, verify identical state; test save/load round-trip across project save/load.
- Risk: medium. Touches every engine's preset state path; pedalboard preset bug is concrete known regression.
- Dependencies: all preceding QA batches (must verify against final-state engines).
- Effort: medium-large (~6-10 hours; one engine at a time).
- Why this slot: late Phase 5 because final-state engines must be present. Feeds into QA-RC test plan.

#### **QA-Export: Audio Export rebuild + Project Bundle** (added 2026-05-08 via Rule 3 — see §9)
**Plan file:** TBD.
- Items: FSW-065 (D-9 Export Audio rebuild — Ctrl+R, format/bitdepth/SR/tail; render path itself broken), LDT-172 (5D Audio Export — WAV/MP3/OGG codecs), BLU-529 (Project Bundle & Export — copy samples into target folder/.zip).
- Scope: rebuild the song-mode audio export pipeline. Currently the only render-to-WAV path is per-pattern right-click in BrowserPanel (`BuilderPage.h/.cpp`); no song-mode export, no MP3/OGG, no format/bitdepth/SR/tail options. **Plus** Project Bundle & Export: zip-all-samples-and-project-into-shareable-archive. User confirmed in QA-Inventory walk that zip bundle is REQUIRED (not deferred per original 5D-BUNDLE plan).
- Risk: medium. New audio-export code path. Bundle path involves filesystem operations on user samples.
- Dependencies: QA-Verify (need confirmed-working preset/state restore so exported project restores intact on the receiving end).
- Effort: large (~8-12 hours; export pipeline + bundle pipeline + format codecs + UI).
- Why this slot: late Phase 5 because depends on stable preset/state from QA-Verify.

#### **QA-RC: Pre-Release Test Plan + RC Build** (added 2026-05-08 via Rule 3 — see §9)
**Plan file:** TBD.
- Items: LDT-414 (Q&A clean build + 2nd clean build + testing plan + test to failure) — original Phase 5G work that was never executed; expanded with QA-Inventory walk findings (LDT-096 menu audit, LDT-097 keybinds audit, LDT-296 Global Tooltip System review, FSW-303 global FX bypass verify).
- Scope:
  - **2nd clean build**: delete entire `build/` directory, fresh Release+Debug rebuild from scratch, audit ALL compiler warnings.
  - **Page-by-page test plan**: documented checklist covering every page (Builder, Mixer, Effects, Layers, Bass, Drums, Clips, Vox, Inst, Rusty, NAMIR) and every workflow (audio I/O, MIDI I/O, transport, effects, mixer routing, save/load, recording, automation, undo/redo). Execute the plan, log findings.
  - **Test to failure**: long sessions, large projects, edge cases (100 tracks, 50 audio clips, 10 plugins routed sidechains, hours of continuous playback, sample-rate switches mid-session, ASIO buffer-size changes, project reload while recording).
  - **Menu audit** (LDT-096): walk every menu (File menu / page menus / right-click menus / global menus) for completeness + correctness + keyboard shortcuts.
  - **Keybind audit** (LDT-097): verify every keybind in the catalog actually fires + does what it says + doesn't conflict.
  - **Tooltip review** (LDT-296): every UI control has tooltip, tooltip text accurate, tooltip explains action not implementation.
  - **Global FX bypass verify** (FSW-303): the master strip's global FX bypass actually bypasses every bus's rack — verify per-bus with audible test.
- Risk: zero (read-only verification). Findings spawn fixes via Rule 3 to other batches OR new follow-ups.
- Dependencies: QA-Audit + QA-Cleanup-1..4 should land first (don't waste time testing code that's about to get cleaned up). QA-Verify + QA-Export.
- Effort: large (~10-15 hours possibly multiple sessions). Bounded by app surface, not complexity.
- Why this slot: AFTER all bug-fix + cleanup work lands. The whole point is verifying the cleaned-up build.

### Phase 6 — Pre-Release Cleanup Audit (its own phase, AFTER all 15 bug batches)

The goal: ship a release that doesn't carry dead/bloated code, and where
"people who are most interested in seeing how the back end works" can read
the source without confusion from dormant or unhooked code paths. Phase 6
runs only after Phases 1-5 land — cleaning up code that hasn't been touched
yet would be wasted work.

Every component in the build gets classified into:
- **Dead** — not referenced, not hooked up, not invoked at runtime.
  Candidate for deletion.
- **Dormant** — present but inactive. Two sub-categories:
  - *Holding for future state* — keep with documented reason
    (inline `// HOLD-FOR-<reason>` comment + one-line entry in the
    implemented-work doc). Kept intentionally. The inline comment is
    the source of truth; the implemented-work entry is the searchable
    index of what's dormant and why.
  - *No reason found* — promote to Dead.
- **Active** — in use by the build today. No-action.

#### **QA-Audit: Codebase classification sweep (read-only)**
- Scope: sweep entire build (`Source/`, `libs/`, `Assets/`, `Kits/`,
  `Presets/`, `Templates/`, plus dev-repo scaffolding) and produce a
  classification manifest. NO code changes; this is the input doc that
  drives QA-Cleanup-1..4.
- Method:
  - File-level: cross-reference against `CMakeLists.txt` + `#include` chains.
  - Function/class/macro-level: grep for symbol references.
  - Asset-level: cross-reference against installer config + runtime load paths.
  - Vendored-lib-level: cross-reference against actually-linked targets
    (memory says over-pruning sfizz broke configure on 2026-05-03 — be careful).
- Output: manifest appended to the implemented-work doc, structured per
  top-level directory.
- **Pre-release decisions to revisit** (added 2026-05-08 mid-QA-Md — see
  §9): a small docket of "should we / shouldn't we" decisions surfaced
  during prior batch execution that aren't covered by the dead/dormant/
  active classification. Each gets a single decision call and a routing
  note for the cleanup batches. Initial entries:
  - **AlertWindow API migration** — currently using older
    `showMessageBoxAsync` / `showOkCancelBox` convenience wrappers
    (~25 sites). Newer `showAsync(MessageBoxOptions...)` builder API
    is JUCE's currently-recommended pattern. Decision: migrate-as-sweep
    in QA-Cleanup-1 / stay-until-deprecated / skip. See Future State
    `CL-288`.
  - **`/audit-security` agent creation** — pre-release / pre-public-repo
    + pre-network-feature security sweep agent. Tier-1 scope (vendored
    CVE scan + file-parser audit + DLL safety + save-file XXE) is
    relevant for V1; Tier-2 (network code, appcast verify) becomes
    relevant once QA-Updater lands. Decision: build-the-agent-now (so
    QA-RC can run a security sweep before V1) vs build-when-network-
    features-land (post-V1). See Future State `CL-289`.
  - **Crash-report + symbol-server pipeline** — `.pdb` generation +
    archival, in-app crash reporter (WER vs third-party), symbol
    server, symbolication tooling. Pairs naturally with QA-Updater
    scope — both are post-release support infrastructure. Decision
    calls: third-party SDK vs OS-native WER; symbol-server hosting
    model; consent flow (prompt-per-crash vs EULA-blanket-consent).
    See Future State `CL-290`.
  - **DSP meter cap V1 release value** — currently 10.0 (1000%) post-
    QA-Md (was 2.0 = 200% pre-batch; raised after diagnostic capture
    proved Debug overload was masked by the 200% cap). V1 release
    value is a UX call: 2.0 (original; FL Studio convention; loses
    diagnosis), 5.0 (middle ground; shows real overload to 500%),
    or 10.0 (max diagnostic; may surprise novice users with "870%"
    readings). Decide ideally with a few weeks of Release session-
    load data. See Future State `CL-291`.
  - **MT diagnostic compile-flag gate** — wrap MT diagnostic counters
    + Mixer hamburger "Run MT Diagnostic" menu item behind
    `#if BAYSICKDAW_MT_DIAGNOSTIC` so Release shipping builds don't
    expose a developer-facing menu item to end users. Active during
    development; compiled out of V1 Release. Touches CMakeLists.txt,
    `RenderEngineFlags.h`, `VibeThreadPool.cpp`,
    `RenderGraphDispatcher.cpp`, `StandaloneEditor.cpp`. Decision in
    Phase 6 QA-Audit (also: should the menu item move to a "Help ->
    Developer" submenu when present?). See Future State `CL-292`.
  - **HarmlessLAF zero-px slider root cause** (added 2026-05-10 mid-QA-A
    — see ninth Forks entry) — QA-A Phase 3.1's `kHdrH 36 -> 32` body-
    layout shift surfaced a latent NaN-coord crash in the
    `LinearVertical` branch of `HarmlessLAF::drawLinearSlider`: when a
    vertical slider has zero-sized bounds, `fh = (float)height = 0`
    makes `norm` divide by zero and NaN propagates into Direct2D's
    `coordsToRectangle` clip-list assert. Symptom-side defensive
    early-return shipped in QA-A commit `679af33`; upstream "why is
    the slider 0 px in the first place?" question deferred to QA-Audit.
    Investigate Harmless layout pass to find which slider is being
    sized to zero (timer-based rebuild? null-guarded `setBounds`? early
    `resized()` before children laid out?). See Future State `CL-293`.
- Risk: zero (read-only).
- Dependencies: all 15 prior batches landed.
- Effort: large (~10-15 hours, possibly multiple sessions). Bounded by
  codebase size, not complexity.

#### **QA-Cleanup-1: Source code cleanup**
- Items: execute the source-code section of the QA-Audit manifest.
  - **Folded in 2026-05-11 (QA-D close NIT-4 carry-forward via §9 eleventh Forks entry)**: per-page `LayersPage::setTabName` writeback to dead `mPianoRoll` state ([Source/Standalone/LayersPage.cpp:321-325](Source/Standalone/LayersPage.cpp) + parallels in `BassPage` / `DrumPage`).  QA-D STATE-02 added the writeback path that lands at a now-dead piano-roll state member (`mPianoRoll` is allocated but not user-visible post-D-5; unified `PianoRollPage` is what the user sees).  Two fix shapes: (i) minimal symptom-fix — delete the `setTabName` writeback lines in each per-page; (ii) full per-page `mPianoRoll` drop — delete the dead member entirely + walk every reference.  Routes here because dead-code shape, not functional bug.
  - **Folded in 2026-05-17 (QA-E Task 7 close-routing via §9 twenty-second Forks entry)**: dead flat-list `BrowserItem::Kind::Audio` choke-group / rename / switch cases in `BrowserPanel::showItemContextMenu` ([Source/Standalone/BuilderPage.cpp](Source/Standalone/BuilderPage.cpp), dead `Audio` case ~line 912).  Post-FILE-01 (QA-E Task 4 library-driven model) no `Audio`-kind `BrowserItem` is ever constructed, so these are unreachable duplicates of the live audio-choke path in `showAudioTreeContextMenu` (choke value lives on `AudioLibraryEntry`; live tree path unaffected).  Pure dead code, no behavior change to remove.  **Source-verified pre-delete guard:** `renameAudioAt` (`BuilderPage.cpp:1117`) is SHARED — called from the live tree path (`BuilderPage.cpp:411`) AND the dead flat-list case — so retain `renameAudioAt`; delete ONLY the dead `BrowserItem::Kind::Audio` flat-list call site.  Routes here because dead-code shape, not functional bug.
- Scope: delete Dead source files; add `// HOLD-FOR-<reason>` comments
  for Dormant + one-line implemented-work entries for each; clean up
  stale comments referencing deleted code.
- Risk: medium — wrong deletion breaks build.
- Mitigation: build after every delete; full verification ladder
  (Section 7's per-batch list) after each meaningful chunk.
- Dependencies: QA-Audit.
- Effort: medium-large (~6-10 hours; folded NIT-4 adds ~15-90 min depending on fix shape chosen; folded §60 dead flat-list audio paths add ~15-30 min — mechanical deletion of the dead `BrowserItem::Kind::Audio` case, `renameAudioAt` retained per the source-verified shared-use guard).

#### **QA-PlayerRename: VibePlayer/* → BaySickPlayer/* internal rename** (forked in 2026-05-10 — see §9)
- Items: QA-A finding #39 (close-time routing).
- Scope: rename the `Source/VibePlayer/` directory to `Source/BaySickPlayer/`; rename `VibePlayerProcessor` / `VibePlayerEditor` / `VibePlayerDSP` / `VibePlayerLAF` and friends to their `BaySickPlayer*` counterparts; sweep every `#include`, every `dynamic_cast<VibePlayer...>`, every comment / doc reference; rename `vp_*` APVTS prefix where used; update CMakeLists target names. User-facing brand ("BaySickPlayer") is already locked since QA-A; this batch closes the source-side / class-side gap.
- Risk: low (mechanical rename across files). One careful pass; risk is missing a stray include / cast in an unrelated file.
- Dependencies: QA-Cleanup-1 (deletes Dead source files first; no point renaming files that are about to be deleted).
- Effort: medium (~2-3 hours, dominated by grep + careful sweep + project-load round-trip verification).

#### **QA-Cleanup-2: Vendored libraries cleanup**
- Items: execute the `libs/` section of the QA-Audit manifest.
- Scope: prune unused vendored libs; for each kept lib, prune unused
  subdirs after grepping for unconditional `configure_file()` references.
- Risk: medium — over-prune breaks configure (sfizz precedent).
- Mitigation: `do_build.bat` configure + build after each lib pruned.
- Dependencies: QA-Audit.
- Effort: medium (~4-6 hours).

#### **QA-Cleanup-3: Assets + presets cleanup**
- Items: execute the assets section of the QA-Audit manifest.
- Scope: prune unreferenced assets, kits, presets, templates; verify
  installer config still produces a functional shipping bundle; document
  any Dormant kept for factory-content reasons.
- Risk: low-medium. Mostly drops bloat from installer.
- Dependencies: QA-Audit.
- Effort: medium (~4-6 hours).

#### **QA-Cleanup-4: Dev-repo scaffolding cleanup (non-shipping)**
- Scope: review and triage non-shipping dev artifacts:
  - `Files For Claude/` legacy docs — what's still relevant vs stale.
  - `build_*.txt`, `*_log.txt`, `null` and similar build-byproduct files
    that escaped `.gitignore`.
  - Old experimental `Tools/` scripts and `*.bat` variants.
  - **The three companion plan docs (plan + carry-forward +
    implemented-work) — KEEP per user direction; they are the historical
    record.**
- Decide per item: keep, `.gitignore`, or delete.
- Risk: low.
- Dependencies: independent of QA-Audit (could run alongside if desired).
- Effort: small-medium (~2-4 hours).

### Phase 7 — Documentation, Templates, Installer (added 2026-05-08 via QA-Inventory close — see §9)

These four batches were planned in the original `lucky-discovering-tiger` Phase 6 (Documentation, Templates, Presets & Installer) but had no representation in the post-Batch-10 Main Plan. QA-Inventory close adds them as a dedicated phase. Runs AFTER Phase 6 cleanup so docs/installer reflect the final cleaned codebase.

#### **QA-Manuals: 3 manuals (Quick Start + Music Tech + Design Tech)** (added 2026-05-08 via Rule 3 — see §9)
**Plan file:** TBD.
- Items: LDT-176 + LDT-415 (Manual 1 Quick Start, 10-15 pages, annotated screenshots), LDT-416 (Manual 2 Music Technical Reference, 40-50 pages, every knob/button/slider organized by page/engine), LDT-417 (Manual 3 Design Technical Document, formulas + signal flow + architecture diagrams).
- Scope: write all three manuals. Image workflow per LDT-415 spec: Claude writes manual + detailed image descriptions, Jeff hands to Copilot for image generation, images returned for compilation. Update VibeDAW references to BaySickDAW.
- Risk: low (no code changes).
- Dependencies: QA-RC (need final stable feature set).
- Effort: large (~30-50 hours, multi-session).
- Why this slot: Phase 7 documentation runs after all features stable.

#### **QA-Templates: Factory templates + AI-Assisted Skill** (added 2026-05-08 via Rule 3 — see §9)
**Plan file:** TBD.
- Items: LDT-177 + LDT-418 (in-app factory presets per engine + factory drum kits + 5 genre-specific starter templates: hip-hop / pop / electronic / lofi / orchestral) + LDT-419 (AI-Assisted Template & Preset Generation Claude Skill — separate skill document).
- Scope: build out factory presets (quantities TBD after engines + tested) for every engine; ship genre-specific templates; create the Claude skill document for AI-assisted preset/template generation per the workflow spec in `lucky-discovering-tiger.md:3487-3502`.
- Risk: low.
- Dependencies: QA-Verify (need preset system verified working) + QA-RC.
- Effort: medium-large (~10-20 hours).
- Why this slot: factory presets + templates ship with the installer.

#### **QA-Installer: NSIS Installer + TTF embed** (added 2026-05-08 via Rule 3 — see §9)
**Plan file:** TBD.
- Items: LDT-178 + LDT-420 (NSIS Installer with sample-package downloads — `vibedaw_installer.nsi`, 11 sample packs from GitHub) + **LDT-173** (5E Font & Asset Bundling — embed TTF in `BaySickDAWAssets` BinaryData so VibeLAF font choices render correctly on clean Windows installs without relying on system fonts).
- Scope: build the installer; bundle TTFs alongside existing PNG/SVG assets; configure sample-pack download UI; update VibeDAW references to BaySickDAW; verify clean-machine install produces functional shipping bundle.
- Risk: medium. Installer build = first time touching NSIS for this project; bundling decisions affect download size + first-run experience.
- Dependencies: QA-Manuals + QA-Templates (installer ships them).
- Effort: medium-large (~10-15 hours).
- Why this slot: installer last; everything ships through it.

#### **QA-Updater: WinSparkle auto-update integration** (added 2026-05-08 via user spec call — see §9 sixth Forks entry)
**Plan file:** TBD.
- Items: User-requested 2026-05-08 — auto-updater integration (no source-doc precedent in `lucky-discovering-tiger.md` / `Final Stretch Work.txt` / `vibedaw_blueprint.md`).
- Scope: vendor WinSparkle (BSD-licensed Windows updater C++ library); link into app launch path; configure GitHub Releases as the appcast source (BaySickDAW repo's Releases tab hosts signed `BaySickDAW-Setup-X.Y.Z.exe` artifacts + an `appcast.xml` manifest); wire once-per-launch background check (with "Remind me later" / "Skip this version" buttons in the prompt) + manual `Help → Check for Updates` menu item; "Auto-check for updates" toggle lives in a **General Settings dialog** (sub-spec at execution time: extend an existing settings dialog or create a new one — current Audio Settings dialog is audio-device-only); silent-skip on no internet (no error UI when offline); update-available prompt offers in-app "Update Now" → WinSparkle downloads the new installer → app exits → NSIS handles the upgrade → app relaunches; **signature-verify** the downloaded installer before running (defends against MITM tampering); rebuild installer to bundle WinSparkle DLL alongside the app binary; stable channel only — beta channel deferred to Future State `CL-287`.
- Risk: medium-high. First WinSparkle integration; cryptographic signing chain depends on QA-Framework's signed-binary path (signing key needs to exist for both the binary and the appcast manifest); testing requires a staged GitHub Release dry-run before V1.0 cuts. WinSparkle has good documentation but the full GitHub-Releases-as-appcast pattern requires a small custom helper to translate Releases JSON → appcast XML (or Sparkle-compatible RSS).
- Dependencies: QA-Installer (NSIS skeleton + sample-pack download infra), QA-Framework signing setup (signature verify needs the signing certificate + key established).
- Effort: medium-large (~10-15 hours).
- Why this slot: layers WinSparkle onto QA-Installer's NSIS skeleton; rebuilds the installer to include the updater. Sequencing means V1.0.0 ships with auto-update from day one (post-QA-Framework cut).

#### **QA-Framework: Framework Document** (added 2026-05-08 via Rule 3 — see §9)
**Plan file:** TBD.
- Items: LDT-179 + LDT-421 (Framework Document — architecture patterns reusable blueprint for future projects).
- Scope: distill BaySickDAW's architectural patterns (APVTS lazy registration, RetirementQueue<T>, closeAllDynamicTabs barrier, MT render-task DAG, source-mux engine wrappers, etc.) into a reusable framework document. Update VibeDAW references to BaySickDAW.
- Risk: zero (documentation only).
- Dependencies: independent (could parallel QA-Manuals).
- Effort: medium (~6-10 hours).
- Why this slot: meta-deliverable; ships as a separate document alongside the manuals.

---

## 5.5 Domain Coverage (batch → bucket map)

§5 batches are listed in execution order (Phase 1 → Phase 7) so the
sequencing arrow + dependencies stay readable. This section flips the
view: each canonical bucket from §0 lists which QA-* batches touch it.
Use this for "what's our V1 coverage in domain X?" lookups; pair with
`Previously Implemented.md` (same buckets, what's already shipped) +
`Future State.md` (same buckets, post-V1 candidates) for the full
cross-doc picture.

| Bucket | V1 batches that touch it (§5) |
|--------|-------------------------------|
| **Effects** | QA-F (DSP-03 PitchCorrector / Formant-Preserve no-op + BaySickVocal H-1..H-6), QA-Fa, QA-Audit (effects audit), QA-Cleanup-1 (dead DSP code) |
| **Players** | QA-D (project-load teardown for engine instances), QA-E (Vox/Inst recording lifecycle, REC-01), QA-F (BaySickVocal DSP), QA-M, QA-Drum-Polish (Phase D dynamic drum UX), QA-Verify (per-engine smoke), QA-Audit (engine surface) |
| **Mixer / Routing** | QA-D (mute/solo dispatch), QA-E (recording-finalize → strip lifecycle), QA-Audit (routing graph), QA-Cleanup-3 (orphan strip cleanup) |
| **System Pages** | QA-G (Builder UX cluster), QA-H (Builder UX cluster — drop / mute / loop / drag), QA-I, QA-J (per-row audio-clip), QA-K, QA-L (Clips / Browser / picker-disable), QA-Export (Export Stems / Master), QA-Manuals, QA-Templates |
| **UI / L&F / Theming** | QA-A (STYLE / fonts), QA-B, QA-C, QA-VibeSlider (right-click-swallow refactor), QA-Audit (UI surface), QA-Manuals (in-app help screens) |
| **Cross-cutting Infrastructure** | QA-0 (DSP-12 Composite RenderTask), QA-0a (Debug build workflow), QA-Md (MT engine Debug-build investigation), QA-Audit (manifest), QA-Cleanup-1..4 (dead-code cleanup phase), QA-RC (release-candidate sweep), QA-Installer (TTF embed + EULA + signing), QA-Updater (WinSparkle auto-update + GitHub Releases appcast + signature verify), QA-Framework (icons / version stamping / registry) |
| **User Tools / Learning** | QA-Manuals (beginner manual + in-app help), QA-Templates (factory project templates) |
| **Workflow Polish** | QA-Verify (E-bucket walk), QA-RC (final QoL sweep) |
| **Other / Platform / Deferred** | (no V1 batches — Section 9 of Future State holds these post-V1 candidates) |
| **Meta** | QA-Inventory (triage + bucket categorisation across the three pre-QA source docs) |

A given batch can touch multiple buckets; the table lists every bucket
the batch's scope touches, not just the primary one. When a batch
closes, the `**Bucket:**` line in its `Implemented Work Log.md` entry
records the same set so cross-doc grep stays consistent.

---

## 6. Sequencing — Option A confirmed

**Bug-fix phases (1-5):**
```
QA-0a* → QA-0 → QA-Inventory*** → QA-Md** → QA-A → QA-C → QA-D → QA-E → QA-Ea********* → QA-Eb********** → QA-F
   → QA-Fa → QA-Fb******** → QA-Fc******** → QA-G → QA-H → QA-I → QA-J → QA-B******* → QA-K → QA-L
   → QA-M → QA-Drum-Polish**** → QA-N → QA-VibeSlider**** → QA-Verify**** → QA-Export****
```

\* QA-0a inserted 2026-05-07 ahead of QA-0 — Debug build workflow
setup so QA-0's dispatcher tripwire is useful in the user's
shipping-binary workflow. See §9 first entry.

\*\* QA-Md inserted 2026-05-07 immediately after QA-0 (originally
queued in Phase 5; promoted to Phase 1) — MT engine is a no-op in
Debug per finding #9; downstream batches need working MT under Debug
for diagnostic purposes. See §9.

\*\*\* QA-Inventory inserted 2026-05-07 between QA-0 and QA-Md —
comprehensive triage + bucket categorization across the three pre-QA
source docs (`Final Stretch Work.txt`, `vibedaw_blueprint.md`,
`.claude/plans/lucky-discovering-tiger.md`). Read-only; no source
code changes. Populates the new `Plans & Specs/` doc skeletons
(`Previously Implemented.md`, `Future State.md`) and routes
still-needed work per Rule 3. See §9.

\*\*\*\* Inserted 2026-05-08 at QA-Inventory close. **QA-Drum-Polish**
(after QA-M) — beginner UX polish on Phase D dynamic-drum architecture
(LDT-298 sound-pack ribbon, LDT-299 audition button, etc.). **QA-VibeSlider**
(after QA-N) — refactor every right-click-swallowing slider in the codebase
to the existing `VibeSlider` subclass (SharedUI.h:956); ~493 sliders flagged.
**QA-Verify** (after QA-VibeSlider) — quick verification batch that walks
every "Done-claimed-but-unverified" / E-bucket item flagged during the
QA-Inventory triage; targeted Release smoke pass per item. **QA-Export**
(after QA-Verify) — wire the Export Stems / Export Master flows that the
ribbon/menu placeholders point at. See §9 QA-Inventory close entry.

\*\*\*\*\* QA-Updater inserted 2026-05-08 via user spec call (sixth
Forks entry). Phase 7, between QA-Installer and QA-Framework. WinSparkle
auto-update integration with GitHub Releases as the appcast source;
once-per-launch + manual `Help → Check for Updates`; signature-verify on
download; "Auto-check for updates" toggle in General Settings; stable
channel only (beta channel deferred to Future State `CL-287`). See §9
sixth Forks entry.

QA-0 and QA-A could run in **parallel** (different code surfaces, no
audio-path overlap between QA-A UI work and QA-0 dispatcher fix).
QA-B was originally in this parallel group but deferred 2026-05-10
to after QA-E, then deferred again 2026-05-11 to after QA-J — see
footnote *******. Everything Phase 2 onward is sequential per
Option A.

**Pre-release cleanup phase (6) — runs ONLY after all of QA-0..N + the
2026-05-08 QA-Inventory close additions have landed and verified:**
```
QA-Audit  →  QA-Cleanup-1  →  QA-PlayerRename******  →  QA-Cleanup-2  →  QA-Cleanup-3  →  QA-Cleanup-4  →  QA-RC****
```

QA-Audit is the keystone — it produces the manifest that drives 1..3.
QA-Cleanup-4 (dev-repo scaffolding) is independent and could ride
alongside QA-Audit if the user prefers; default sequencing keeps it last.
**QA-RC** (release-candidate verification) was added 2026-05-08 at
QA-Inventory close as the gate before Phase 7 — a full project lifecycle
sweep across the cleaned-up build to confirm nothing regressed during
the cleanup phase.

\*\*\*\*\*\* **QA-PlayerRename** inserted 2026-05-10 at QA-A close
(ninth Forks entry). Phase 6, after QA-Cleanup-1. Internal-source
rename of `Source/VibePlayer/` directory + `VibePlayer*` classes /
files to their `BaySickPlayer*` counterparts so the source side
matches the already-locked user-facing brand name. Mechanical sweep;
no behavioural changes. Sequenced after QA-Cleanup-1 so the
rename only touches files that survived the Dead-source-file
deletion pass. See §9 ninth Forks entry.

\*\*\*\*\*\*\* QA-B has been deferred twice:

**First deferral — 2026-05-10:** from after QA-A to after QA-E.
The DSP-12 verification matrix's simultaneous case (Builder + piano
roll both placed, both play summed) is the architectural heart of
QA-0's Composite RenderTask fix. Clean verification of that case
requires mute-isolation testing — but findings #16a (pattern row
mute no effect), #16b (right-click block mute no effect), #21 (track
row mute permanent) are all routed to QA-E. Without those fixes,
mute-isolation isn't available and the simultaneous-case verification
can only lean on "two distinct audio contents + meter inspection,"
which is materially weaker. Deferring QA-B until after QA-E gives a
clean methodologically-sound verification on a known-good substrate.
See §9 tenth Forks entry.

**Second deferral — 2026-05-11:** from after QA-E to after QA-J.
The Clips routing-unification fix (folded into QA-J per §9 thirteenth
Forks entry, amended same day) changes the architectural premise of
the DSP-12 simultaneous-case test from "both play simultaneously" to
"both play simultaneously THROUGH THE SAME chain."  The corrected
premise can only be exercised against unified-routing source, which
lands in QA-J.  Per user spec call 2026-05-11 (Option A): slide QA-B
entirely to after QA-J close (vs. splitting into single-flow cells
after QA-E + simultaneous case after QA-J).  See §9 fourteenth
Forks entry.

\*\*\*\*\*\*\*\* **QA-Fb + QA-Fc** inserted 2026-05-14 at the
BaySickAlign + BaySickPitch redesign detour close (mid-QA-E).  Phase
3, slotted after QA-Fa and before QA-G.  **QA-Fb** — Recording
Lifecycle + Channel-Composite Renderer (dual-buffer recording, conditional
WET tap, multi-take capture-bleed fix, audio-clip-resize-doesn't-stretch
fix, dirty-flag investigation, channel-composite renderer shared with
QA-F/QA-Fa).  **QA-Fc** — BaySickNAMIR Dual-Mic Stack (two parallel
mic-sim paths summed rather than blended; new feature; sits on the
audited-clean BaySickNAMIR foundation).  See §9 eighteenth Forks entry
for the full four-decision package + the queued QA-Fd-dropped audit
outcome.

\*\*\*\*\*\*\*\*\* **QA-Ea** inserted 2026-05-15 at the QA-E Task 6
(DSP-09) pre-task spec-call.  Slotted **immediately after QA-E, before
QA-F** (Jeff's confirmed slot — same code area + same test material as
QA-E, run in a slightly different order).  Scope: DSP-09 bus-solo fix +
Layers/Bass/Drums bus-output-path unification (the Phase-1-vs-5F-4b
legacy-split removal — the architecturally-correct fix that eliminates
the bus-solo bug class).  DSP-09 was struck out of QA-E entirely; QA-E
Task 6 is vacated and QA-E resumes at Task 7 (FILE-02).  QA-Ea gets its
own plan file + its own mandatory /review-batch for the hot-path safety
surface (master-sum, MT MasterTask, BusNode buffer model).  See §9
nineteenth Forks entry + running-notes §34-§40 for the full diagnosis.

\*\*\*\*\*\*\*\*\*\* **QA-Eb** inserted 2026-05-17 during the QA-E
Task 7 (FILE-02) verify session.  Slotted **immediately after QA-Ea,
before QA-F** (Jeff's confirmed slot — adjacent to QA-E purely for
testing efficiency: a resizable window removes window-juggling from the
remaining QA-E-adjacent verify passes; QA-Eb and QA-E do NOT group by
code area).  Scope: standalone app-window user-resizability — resizable
window + maximize + min-size clamp + outer `juce::Viewport` scrollbars
at sub-design size, with Piano Roll / Builder-grid self-scrolling pages
opted out of double-wrap.  Per-page proportional / FlexBox / Grid
relayout is explicitly OUT of scope (post-V1).  **NOT a carve-out of
QA-L's `NAV-01`** — fresh independent request; `NAV-01` / QA-L
untouched.  See §9 twentieth Forks entry + running-notes §53.

**Phase 7 — Documentation, Templates, Installer (runs ONLY after QA-RC):**
```
QA-Manuals****  →  QA-Templates****  →  QA-Installer****  →  QA-Updater*****  →  QA-Framework****
```

Four added 2026-05-08 at QA-Inventory close + one (QA-Updater) added
2026-05-08 via user spec call. **QA-Manuals** — the beginner manual +
in-app help screens (LDT-218, LDT-219, etc.). **QA-Templates** — factory
project templates / starter packs (LDT-220, LDT-221). **QA-Installer**
— Windows installer build with embedded TTF fonts (LDT-173) and licence /
EULA flow. **QA-Updater** — WinSparkle auto-update integration with
GitHub Releases as the appcast source, once-per-launch + manual check,
signature-verify on download, "Auto-check for updates" toggle in General
Settings, stable channel only (see Future State `CL-287` for the beta
channel deferral). **QA-Framework** — final installable framework
checks (icons, version stamping, registry keys, signed binary path). See
§9 QA-Inventory close entry + sixth Forks entry (QA-Updater) for the full
per-batch source-trace.

---

## 7. Verification Approach

**Per-batch verification (every batch must pass before commit):**

> **Note (2026-05-07):** post QA-0a, every "build" step in this list
> produces both Release and Debug exes. The standing rule is to
> verify in the Debug exe FIRST (any `jassert` fires as a precise
> dialog you can screenshot), then re-run the same checks in Release
> as the actual user-facing test. See §9 first entry + CLAUDE.md
> Build System.

1. `do_build.bat` clean.
2. App launches, audio plays at default settings.
3. Open existing big project (5 Guitars + 5 Bass + 1 Rusty) — no crash,
   audio plays.
4. Save → close → reopen → load — round-trip clean.
5. **MT toggle round-trip**: hamburger → toggle MT off → audio plays
   serial-path identically → toggle on → audio plays MT-path identically.
   Settings.xml persistence verified across restart.
6. Item-specific repro from the unified backlog.
7. Regression sweep on neighboring items in the same cluster.

**QA-0-specific verification matrix (DSP-12 after composite lands):**

| Test | Expected (MT on) | Expected (MT off) |
|------|------------------|-------------------|
| WAV drop on Builder grid → Builder grid playback | Plays | Plays |
| WAV drop on Builder grid → piano roll playback (auto-spawned Clips page) | Plays | Plays |
| MP3 drop on Builder grid → Builder grid playback | Plays | Plays |
| MP3 drop on Builder grid → piano roll playback | Plays | Plays |
| WAV drop on Clips tab → Builder grid playback (when block placed) | Plays | Plays |
| MP3 drop on Clips tab → piano roll playback | Plays | Plays |
| Both placed simultaneously (Builder + piano roll) | Both play, summed | Both play, summed |

**Cross-batch verification (every 2-3 batches):**

1. `git log --oneline` since last verification — readable + each commit
   compiles + passes (1)-(5).
2. Multi-take recording session (8-bar loop, 5 instruments, 2 vocal
   takes, 1 audio clip drop, 1 MP3 drop) end-to-end. Confirms project
   XML restoration walker + recording finalize + clip browser + mixer
   strip lifecycle all healthy.
3. DSP meter under MT reads sensible values across batch sizes
   (32, 64, 128, 256, 512). After QA-N lands, also verify the
   sum-of-cores reading.

---

## 8. Critical Files (touched by ≥1 proposed batch)

Pre-flight a `git status` on each before starting any batch — uncommitted
work here is a red flag for cross-batch contamination.

- [Source/PluginProcessor.cpp](Source/PluginProcessor.cpp) — every audio-thread batch.
- [Source/PluginProcessor.h](Source/PluginProcessor.h) — APVTS, snapshot, barrier.
- [Source/Engine/RenderGraphDispatcher.cpp/.h](Source/Engine/RenderGraphDispatcher.cpp) — task
  registration, link rebuild. **QA-0 epicentre.**
- New: [Source/Engine/Tasks/CompositeAudioInsertTask.h/.cpp](Source/Engine/Tasks/CompositeAudioInsertTask.h) — created in QA-0.
- [Source/Engine/Tasks/AudioInsertTask.cpp](Source/Engine/Tasks/AudioInsertTask.cpp) +
  [ClipPageTask.cpp](Source/Engine/Tasks/ClipPageTask.cpp) — body becomes
  helpers invoked by composite in QA-0.
- [Source/Engine/Tasks/InstStripTask.cpp](Source/Engine/Tasks/InstStripTask.cpp) — idle-suspend
  gate. QA-C.
- [Source/Engine/Tasks/RustyDrumsProducerTask.cpp](Source/Engine/Tasks/RustyDrumsProducerTask.cpp) — idle-suspend gate parallel. QA-C.
- [Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp) — onTabClosed,
  closeAllDynamicTabs, project XML restore walker, recording finalize,
  hamburger menu, automation lane resolver. QA-A (potentially), QA-C, QA-D,
  QA-E, QA-I, QA-L.
- [Source/Standalone/MixerPage.cpp](Source/Standalone/MixerPage.cpp) — Vox/Inst spawn
  cascade. QA-C, QA-E, QA-L.
- [Source/Standalone/BuilderPage.cpp](Source/Standalone/BuilderPage.cpp) — timeline
  geometry, block resize, dead Properties line, FILE-02 Properties popup
  edit. QA-E, QA-G, QA-H.
- [Source/Standalone/PianoRoll.cpp](Source/Standalone/PianoRoll.cpp) — MIDI features. QA-H.
- [Source/Standalone/EffectsPage.cpp](Source/Standalone/EffectsPage.cpp) — channel dropdown
  refresh path. QA-L.
- [Source/Standalone/SharedUI.cpp](Source/Standalone/SharedUI.cpp) — VKnob right-click,
  PopupMenu wrapper. QA-L.
- [Source/VibeGraph.cpp](Source/VibeGraph.cpp) — bus DSP, solo logic. QA-D
  (potentially), QA-E (DSP-09), QA-J.
- [Source/BaySickVocal/BaySickVocalProcessor.cpp](Source/BaySickVocal/BaySickVocalProcessor.cpp) — Vox FX pipeline. QA-F.
- [Source/Standalone/StandaloneApp.cpp](Source/Standalone/StandaloneApp.cpp) — settings.xml,
  initialise, MT preference. QA-K (APP-04).

---

## End of plan

Plan executes end-to-end without further blocking input. Each batch
gets its own per-batch implementation plan when started, in
`C:\Users\jeffm\Documents\BaySickDAW\Plans & Specs\Batch Plans\<silly-name>.md`. Each per-batch plan
follows the three-doc system + carry-over discipline established in
Section 0.

**Three companion docs (created on plan-mode exit):**
- [Carry-Forward Reference.md](Carry-Forward Reference.md) — architectural reference + decisions + per-item status
- [Implemented Work Log.md](Implemented Work Log.md) — running log of executed work + findings

**Cleanup at project end:** when all 20 batches have landed (15 bug-fix
in Phases 1-5 + 5 cleanup in Phase 6) and the plan is closed out, the
three companion docs become the historical record of this work. Keep
them in place; they're the source of truth for "why was X done this
way?" questions in future sessions. (Phase 6 explicitly excludes the
companion docs from its cleanup-targets list — they're scaffolding
that's earned its keep.)

First batch: **QA-0a — Debug Build Workflow Setup** (forked in 2026-05-07; see §9). Then QA-0.

---

## 9. Forks

Chronological log of scope/sequencing changes since plan write
(2026-05-07). Entries are append-only. Each entry pairs with inline
back-references in the affected sections (per §0 Rule's hybrid
convention).

### 2026-05-07 — QA-0a inserted before QA-0 (Debug build workflow setup)

**Trigger:** during the QA-0 plan-mode session, the user decided the
dispatcher's most-recent-wins fallback should be tightened to
`jassertfalse`. Investigation surfaced that `jassertfalse` is
compiled out of Release builds and `do_build.bat` only builds
Release, so the tripwire would never fire in user workflow.

**Deeper finding:** the user is a solo developer with no coding
background. Today's diagnostic loop is bottlenecked by describing
behavior in plain English (no precise error messages from Release).
A Debug build alongside Release short-circuits that loop — when a
`jassert` fires, Windows pops up a precise file:line dialog the user
can screenshot. Round-trips collapse from many to one.

**Decision:** insert QA-0a (Debug build workflow setup) BEFORE QA-0.
QA-0a's dispatcher tripwire (in QA-0) becomes genuinely useful from
the moment QA-0 lands.

**QA-0a scope:** modify `do_build.bat` to build both Release and
Debug; gate the embedded exe icon for Release-only (Debug exe shows
generic Windows .exe icon, so taskbar pins differentiate); append
" [DEBUG]" to the window title in Debug builds; cold-start triage of
existing `jassert` calls that fire on a clean default session;
document the new workflow in CLAUDE.md.

**Inline back-refs:**
- §0 Rule updated to document the hybrid annotation convention.
- §5 Phase 1 has new QA-0a entry above QA-0.
- §6 Sequencing arrow updated.
- §7 Verification gained a Debug-first note.

**Plan files:**
- QA-0a: `Batch Plans\i-want-you-to-adaptive-dongarra.md`
- QA-0: `Batch Plans\composite-merging-rivers-twilight.md`

**Verification:** QA-0a closes when the dual-config build produces
both exes, taskbar differentiation works, cold-start triage is done,
and CLAUDE.md reflects the new workflow.

### 2026-05-07 — Rule 3 added to §0 (findings-during-execution routing convention)

**Trigger:** during QA-0 Task 5 execution, multiple real-bug findings
had accumulated from QA-0a's cold-start triage that weren't in §5's
sequence (#8 MenuBarModel listener-dangle, #9 MT no-op in Debug, #13
InstPage* use-after-free in tab-click lambda). User raised the
question: how do we route these without fragmenting work or polluting
Phase 6?

**Decision:** add Rule 3 to §0 codifying the routing convention.
Findings touching a not-yet-started batch's surface fold into that
batch; findings touching a completed batch get a §5-entry annotation
+ §9 Forks entry but no new §5 row (commits stay sequential, plan
reflects conceptual ownership); findings with no surface match get
a new dedicated §5 batch row. Phase 6 reserved for dead-code cleanup
only.

**Inline back-refs:** §0 "Standing rules" header updated from "Two"
to "Three"; new Rule 3 paragraph added after Rule 2's non-negotiable
note.

**Application timeline:** convention applies retroactively to QA-0a's
findings at QA-0 close — finding #8 folds into QA-D, finding #13
folds into QA-E, finding #9 becomes a new dedicated §5 batch.

### 2026-05-07 — QA-0 close routings (Rule 3 first application)

QA-0 closed 2026-05-07 with DSP-12 verified in Release under both
MT-on and MT-off (Composite RenderTask sums clip-engine and
arrangement-clip flows correctly).  Final commits: `611db82` /
`f72cd09` / `df6f0a3` / `0ef0c95` / `4200479`.

Per Rule 3, findings accumulated during QA-0a + QA-0 execution were
routed at close.  Summary of routings:

**Folded into not-yet-started batches:**
- **QA-D** ← finding #8 (MenuBarModel listener-dangle during
  closeAllDynamicTabs cascade).  Same surface as QA-D's project-load
  teardown work.
- **QA-E** ← finding #13 (use-after-free in showPageForTab tab-click
  lambda, `0xDDDDDDDDDDDDDDDD` confirmed); finding #14 (same family,
  Clips player-page Piano Roll button); findings #16a + #16b
  (pattern row mute / right-click pattern block mute have no effect);
  finding #21 (track row mute permanently mutes audio clip).  All
  touch the same audio-row + tab-callback + mute-dispatch surfaces
  QA-E already targets.
- **QA-H** ← finding #15 (Builder drop auto-navigates to player page);
  finding #17 (BuilderPage TreeView destructor dangling subItem on
  app shutdown); finding #18 (muting a block resets loop count);
  finding #19 (can't drag audio clips back to Builder after deletion);
  finding #20 (UX: clicking a browser item should set "active drop
  type" for empty Builder clicks).  Builder UX + state cluster.
- **QA-J** ← finding #16c (audio row mute desync — streamer pauses
  at expectedFilePos; on unmute resumes from frozen position rather
  than syncing to current project transport).  Same surface as
  QA-J's per-row audio-clip rendering work.

**Promoted from Phase 5 deferred to Phase 1 immediate:**
- **QA-Md** (MT Engine Debug-Build Investigation, finding #9) —
  originally queued as deferred Phase 5 batch; promoted to Phase 1
  immediately after QA-0 because every downstream batch that touches
  audio code needs Debug to actually engage MT for diagnostic
  precision.  QA-0a built the diagnostic Debug build infrastructure;
  QA-Md restores the MT path under Debug so that infrastructure
  delivers value going forward.  New sequence:
  `QA-0a → QA-0 → QA-Md → QA-A → QA-B → ...`.

**Suppressed in vendored JUCE (not real bugs by design):** findings
#1-7, #10-12 from the QA-0a cold-start triage were suppressed via
inline `// jassert(...)` patches in JUCE source + the
`JUCE_DISABLE_CAUTIOUS_PARAMETER_ID_CHECKING` define + the em-dash
ASCII sweep + the voice-ctor sample-rate-at-source fix.  All shipped
in QA-0a commits (`b34c54d` / `a472a44` / `bd67fdf`).  Not routed
to future batches — already addressed.

**Inline back-refs:** §5 entries for QA-D, QA-E, QA-H, QA-J each
got "Folded in 2026-05-07" sub-bullets per Rule 3.  §5 has new QA-Md
entry between QA-0 and QA-A.  §6 sequencing arrow updated with
QA-Md slot + footnote.

**Plan files affected:**
- QA-0 plan: `composite-merging-rivers-twilight.md` (closed).
- QA-Md plan: TBD silly-name file when batch starts (next).
- §9 of this main plan: this entry (third Forks entry).

### 2026-05-07 — QA-Inventory inserted between QA-0 and QA-Md (comprehensive source-doc triage)

**Trigger:** post-QA-0 close, the three pre-QA source docs
(`Files For Claude/Final Stretch Work.txt`,
`Files For Claude/vibedaw_blueprint.md`,
`C:/Users/jeffm/.claude/plans/lucky-discovering-tiger.md`) intermix
shipped work, claimed-but-unverified work, abandoned work, and
future-state ideas without disambiguation. Without a dedicated
triage pass, every downstream batch's plan author re-parses the
same ambiguity. Additionally, the new `Plans & Specs/` doc
skeletons (`Previously Implemented.md`, `Future State.md`) created
in commit `c05ce61` need to be populated with verified content.

**Decision:** insert QA-Inventory between QA-0 (closed) and QA-Md
(next). Comprehensive review + bucket categorization across the
three docs:

- **A** — claimed not-done, still needed → walk one-by-one with user;
  route per Rule 3 (fold into existing §5 batch OR new dedicated §5 batch).
- **B** — claimed not-done, drop candidate → confirm with user;
  one-line entry in `Future State.md` "Considered & Dropped".
- **C** — claimed done + source-verified → entry in
  `Previously Implemented.md`.
- **D** — Claude future-state additions across Audio Quality,
  Performance, User Tools, Workflow Polish → entries in
  `Future State.md`. User direction: every plausible idea, not bounded.
- **E** — claimed done but unverified → walked with user alongside A;
  per-item Decision dropdown: Update (reroute to A) / Archive (reroute
  to C) / Delete (folds into Phase 6 cleanup carry-list).

**Source verification depth (locked):** headline-verify every claim
(file/class/function exists) + sample 20% of named sub-items per
claim + escalate to full verification on any miss + skip anything
already verified in the 2026-05-07 Carry-Forward Reference snapshot.

**Walkthrough medium:** Google Sheet created via the user's Drive
connector with all items pre-filled (`ID | Bucket | Source Doc |
Section/Line | Title | Original Status Claim | Brief Context |
Decision | Your Notes`); valid Decision values listed in a frozen
first-row legend; user fills Decision column at own pace; Claude
downloads sheet as CSV at checkpoints to apply decisions to plan
docs.

**Plan file:** N/A — executed inline from chat breakdown (per user
direction; no separate per-batch plan file in `Batch Plans/`).

**Inline back-refs:**
- §5 has new QA-Inventory entry between QA-0 and QA-Md.
- §6 sequencing arrow updated; new `***` footnote added.
- §9 Forks: this entry (fourth).

**Verification:** QA-Inventory closes when (a) every distinct item
across all three source docs has a recorded bucket + decision, (b)
`Previously Implemented.md` is populated for all bucket-C items, (c)
`Future State.md` is populated for B "Considered & Dropped" + all
D entries, (d) Main Plan §5 reflects all bucket-A foldings (existing
batch scope expansions) + new batches if any, (e) §9 Forks has a
"QA-Inventory close routings" entry chronicling all foldings/
additions/deletions-to-Phase-6, (f) `Implemented Work Log.md` has
the QA-Inventory closing entry.

### 2026-05-08 — QA-Inventory close routings (1429 items triaged across 3 source docs)

QA-Inventory closed 2026-05-08 after a multi-day triage pass.  Final
counts: 1429 items extracted from the three source docs; 1089 routed
to `Previously Implemented.md` after exact-title + cross-doc dedupe
(31 rows merged across 29 clusters; report at
`C:\Users\jeffm\.claude\plans\qa-inventory-dedupe-report.md`); 17
walked-to-Drop entries + 6 walked-to-D-bucket entries written to
`Future State.md`; 26 walked-to-Update entries (E→A reroute) folded
into existing §5 batch scopes per Rule 3; 9 brand-new §5/§6/§7
batches added for items with no existing surface match.

**Cluster decisions (per dedupe report):**
- Cluster 1 (BLU/FSW G-1.x parity) — keep all (FSW = primary; BLU
  rows merged into single FSW entry).
- Cluster 2 (BLU/FSW G-2 / G-3 / G-5 parity) — keep all (same
  treatment as Cluster 1).
- Cluster 3 (BLU/LDT D1.x dynamic-drum parity) — confirmed merge
  (LDT = primary; BLU rows referenced LDT).
- Cluster 4 (BLU/FSW F-1 per-pattern colour) — confirmed merge.
- Cluster 5 (BLU/LDT/FSW EQ8 §12 cluster) — confirmed merge; 5d
  (treble range-mapping bug) folded into §12 EQ8 entry rather than
  living as separate row.
- Cluster 6 (LDT L&F sprint cross-refs L1/L8/L9/L11) — keep all
  (kept the duplicate row count rather than collapsing because L&F
  sub-items shipped at different commits).
- Cluster 7 (Project zip-bundle / lifecycle) — confirmed merge.
- Cluster 8 (Mixer page review backlog) — confirmed merge.
- Cluster 9 (Audition vs playback level mismatch) — confirmed merge.
- Cluster 10 (Delete prompt UX) — keep both rows pending full
  delete-prompt review (deferred sweep).
- Cluster 11 (Browser panel collapsible) — confirmed merge.

**Folded into existing not-yet-started §5 batches (Rule 3):**
- **QA-A** ← STYLE-02 / LDT-167 (font + size sweep findings).
- **QA-E** ← REC-01 cluster: BLU-470 (vox + inst recordings not
  playing on Builder), pedalboard-preset round-trip (recording
  finalize touches the same surface).
- **QA-F** ← DSP-03 cluster: realtime pitch correction broken at
  runtime (BaySickVocal `mPitchCorrector.process()` is in the chain
  but YIN tracker never detects); Formant-Preserve / Throat-Sim
  no-op confirmed (PitchCorrectorDSP.cpp:326 `juce::ignoreUnused`).
  + BaySickVocal H-1..H-6 sub-items.
- **QA-J** ← BLU-501 (per-row audio-clip rendering polish).
- **QA-L** ← Clips/Browser cluster: BLU-378/379 (browser panel
  state), LDT-394 (clip context-menu), BLU-492 (clips ribbon
  badge), LDT-026 (clip rename), FSW-123 (picker-disable when no
  clip selected).

**New §5/§6/§7 batches added (no existing surface match):**

| Batch | Position | Source-trace |
|-------|----------|---------------|
| **QA-Drum-Polish** | after QA-M | LDT-298 (sound-pack ribbon), LDT-299 (audition button), LDT-300 (per-drum locked-state polish), LDT-301..305 (Phase D polish backlog). |
| **QA-VibeSlider** | after QA-N | ~493 sliders flagged across the codebase that swallow right-click; refactor each call site to `VibeSlider` (SharedUI.h:956). User-approved as own batch given scope. |
| **QA-Verify** | after QA-VibeSlider | walks every E-bucket "Done-claimed-but-unverified" item flagged during inventory; Release smoke pass per item; reroutes any miss to a fresh §5 follow-up batch. |
| **QA-Export** | after QA-Verify | wires the Export Stems / Export Master flows that the existing ribbon/menu placeholders point at (no audio path written yet). |
| **QA-RC** | after QA-Cleanup-4 | release-candidate sweep across the cleaned-up build before Phase 7 documentation/installer work begins. |
| **QA-Manuals** | Phase 7 | beginner manual + in-app help screens (LDT-218, LDT-219, etc.). |
| **QA-Templates** | Phase 7 | factory project templates / starter packs (LDT-220, LDT-221). |
| **QA-Installer** | Phase 7 | Windows installer with embedded TTF fonts (LDT-173) + EULA + signed binary path. |
| **QA-Updater** | Phase 7 (after QA-Installer) | WinSparkle auto-update + GitHub Releases appcast + once-per-launch + manual `Help → Check for Updates` + signature-verify + Auto-check toggle in General Settings + stable channel only (beta channel = Future State `CL-287`). User-requested 2026-05-08; see sixth Forks entry. |
| **QA-Framework** | Phase 7 | final framework checks (icons, version stamping, registry keys). |

**Walked-to-Drop (B bucket → `Future State.md` Considered & Dropped):**
17 entries written to Section 3 of `Future State.md`. Highlights:
12× Harmless UI polish items (cosmetic-only, decided against pre-v1.0);
BLU-423 (legacy DrumsPage refactor — superseded by Phase D dynamic-drum);
3× ambiguous spec items deferred from Phase 5F-5 / 5F-6; FSW-244 (Show
Input Diagnostics dialog — verified at MixerPage.cpp:1886-1976 as not
built, agreed to drop until proven needed); BLU-605 (voxRoll/instRoll
infrastructure — kept; reclassified as Drop because it's NEEDED for
Inst BaySickGuitars/Basses + reserved for future SFZ vocal, not work).

**Walked-to-D-bucket (B bucket → `Future State.md` Section 4):**
6 entries: BLU-088, BLU-146, BLU-147, BLU-407, FSW-121, FSW-330. Each
is a plausible post-v1.0 idea the user wants kept on the radar but
not in scope for the QA cycle.

**Phase 6.1 dedupe stats (subagent ab7220a69335dc191):**
- Input rows: 1120 (Done-claimed-and-verified across 3 docs).
- Output rows: 1089.
- Saved (rows removed): 31.
- Clusters with 2+ source rows: 29.
- Singletons: 1060.
- Output: `C:\Users\jeffm\.claude\plans\qa-inventory-deduped-final.tsv`.

**Side findings surfaced during walkthrough (routed at close, not
held over):**
- BaySickVocal realtime pitch correction broken at runtime — DSP path
  exists, YIN doesn't fire (process is wired in chain but reports
  "Detected --"). Routed to QA-F DSP-03 (existing surface) rather
  than CL-024 fresh entry (CL-024 was for T-Pain hard-tune, which is
  achievable via existing realtime params once DSP-03 lands).
- Pedalboard preset round-trip broken — verified user side; routed
  to QA-E REC-01 (recording lifecycle owns preset XML).
- LDT-173 TTF embed (font in installer) is distinct from STYLE-02
  font choices — kept as scope inside QA-Installer; not collapsed
  into QA-A.

**Inline back-refs:**
- §5 entries gained "QA-Inventory fold-in 2026-05-08" sub-bullets
  for QA-A / QA-E / QA-F / QA-J / QA-L.
- §5 has new entries for QA-Drum-Polish, QA-VibeSlider, QA-Verify,
  QA-Export, QA-RC, QA-Manuals, QA-Templates, QA-Installer,
  QA-Framework.
- §6 sequencing arrow rewritten with all 9 new batches; new `****`
  footnote covers the close additions.
- New Phase 7 section added in §6 between QA-RC and the §7 header.
- §9 Forks: this entry (fifth — QA-Inventory close).

**Plan files affected:**
- `Plans & Specs/Previously Implemented.md` — populated with 1089
  deduped entries (subagent output).
- `Plans & Specs/Future State.md` — Section 3 populated (17 Drops);
  Section 4 populated (6 D-bucket items).
- `Plans & Specs/Implemented Work Log.md` — header convention
  section added; existing entries bumped from `##` to `###` with
  PT timestamps; QA-Inventory close entry appended (Phase 7 of
  this batch).
- `Plans & Specs/Main Plan.md` — this entry + scope expansions
  noted above.
- `CLAUDE.md` — post-close cleanup pass scheduled (stale OPEN BUG
  drum-woofy entry, etc.).

**Verification:** every (a)-(f) condition from the QA-Inventory
insertion entry above met. Closure commit follows after Phase 6.5
(per-chunk commits) + Phase 7 (Implemented Work Log entry).

### 2026-05-08 — QA-Updater added to Phase 7 (auto-update infrastructure)

**Trigger:** user spec call. After the v2 competitive sweep + DAW
architecture research landed (commits `200d5cf` / `f9f7b89`), user
requested an auto-update facility be added to the installer scope —
"if the user is connected to the internet do a check for update files,
prompt the user with an available update, preferably with a way to
update from the prompt instead of a lazy prompt telling them to go
somewhere."

**Spec calls confirmed by user 2026-05-08 (in order):**

1. **Hosting:** GitHub Releases (BaySickDAW repo's Releases tab; user
   already plans to take repo public once design docs land).
2. **Updater library:** WinSparkle (BSD-licensed, well-maintained,
   integrates with NSIS + GitHub-Releases-as-appcast pattern).
3. **Check timing:** combo — once-per-launch background check (with
   "Remind me later" / "Skip this version" buttons in the prompt) plus
   manual `Help → Check for Updates` menu item.
4. **Install flow:** download new installer + app exits + NSIS handles
   the upgrade + app relaunches (in-app prompt, not "go to website").
5. **No-internet behavior:** silent skip (no error UI when offline).
6. **Preference:** "Auto-check for updates" toggle in **General
   Settings** (sub-spec at execution: extend an existing settings
   dialog or create a new one — current Audio Settings dialog is
   audio-device-only).
7. **Channels:** stable only for V1 (would need installer infra
   complete before all V1 features are finished to populate beta
   cycle); beta channel deferred to Future State `CL-287`.
8. **Signature verification:** YES — verify signature on downloaded
   installer before running (defends against MITM tampering).
9. **Scope placement:** new sibling batch QA-Updater in Phase 7
   (after QA-Installer), NOT folded into QA-Installer or deferred to
   Future State.

**Decision:** insert QA-Updater between QA-Installer and QA-Framework
in Phase 7. QA-Installer continues to own NSIS skeleton + sample-pack
download infra + TTF embed + EULA flow. QA-Updater layers WinSparkle
on top: vendors the library, integrates check + prompt UI, adds the
General Settings toggle, configures GitHub Releases as the appcast
source, signature-verifies downloads, and rebuilds the installer to
bundle the WinSparkle DLL. QA-Framework still closes Phase 7 with
final installable framework checks (icons / version stamping /
registry / signed binary path); the signing key established in
QA-Framework is what QA-Updater's signature-verify chain depends on
(coordinate signing setup across both batches).

**QA-Updater scope summary (full text in §5 Phase 7 entry):**
WinSparkle vendor + link; GitHub Releases as appcast (custom helper
for Releases JSON → appcast XML translation); once-per-launch +
manual menu check; "Auto-check for updates" toggle in General
Settings; silent-skip on no internet; download-and-run-installer
flow; signature-verify before running downloaded installer; rebuild
installer to bundle WinSparkle DLL; stable channel only.

**Inline back-refs:**
- §5 Phase 7 has new QA-Updater entry between QA-Installer and
  QA-Framework.
- §5.5 Cross-cutting Infrastructure row updated to include QA-Updater.
- §6 Phase 7 sequencing arrow updated with QA-Updater slot + new
  `*****` footnote covering the user-spec-call trigger.
- §6 Phase 7 paragraph updated from "All four added 2026-05-08..."
  to "Four added 2026-05-08 at QA-Inventory close + one (QA-Updater)
  added 2026-05-08 via user spec call".
- §9 batch summary table gained QA-Updater row.
- §9 Forks: this entry (sixth).

**Plan files affected:**
- `Plans & Specs/Main Plan.md` — this entry + §5 / §5.5 / §6 / §9
  edits noted above.
- `Plans & Specs/Future State.md` — `CL-287 / WP` Beta channel
  opt-in entry added under Cross-cutting Infrastructure (new sub-cluster
  "Auto-update infrastructure"); pairs with QA-Updater stable channel.

**Verification:** QA-Updater closes when (a) WinSparkle vendored +
linked + builds clean Release + Debug, (b) GitHub Releases dry-run
serves a test appcast XML the app actually consumes, (c) update prompt
fires on launch when remote version > local, (d) "Auto-check for
updates" toggle persists across launches, (e) signature verify rejects
a tampered test installer with a clear user-facing error, (f) silent
skip works when offline (no toast / dialog).

### 2026-05-08 — QA-Audit scope expanded with pre-release decisions docket (mid-QA-Md side findings)

> **Note:** The 2026-05-09 QA-Md close (eighth Forks entry below) added
> two further items to this same docket (`CL-291` DSP meter cap V1
> release value + `CL-292` MT diagnostic compile-flag gate) bringing
> the total to five.  See that entry for context.

**Trigger:** during QA-Md Task 4 execution (adding the Run MT
Diagnostic menu item to the Mixer hamburger), a side conversation
surfaced three pre-release decisions worth capturing rather than
deciding inline:

1. **JUCE AlertWindow API migration.** Task 4's plan-as-written used
   `showAsync(MessageBoxOptions...)` (the newer builder-pattern API)
   for the result popup. Pre-edit codebase audit found ~25 existing
   call sites on the older convenience wrappers (`showMessageBoxAsync`
   / `showOkCancelBox`) and zero on the newer builder. To preserve
   pattern consistency, Task 4 was rewritten against the older
   convention. The newer API has marginal advantages (forward-compat
   with future JUCE-only features, alignment with current JUCE
   tutorials) but is not technical debt in the strict sense -- the
   wrappers compile to the same generated code. Decision deferred
   to a single sweep batch, never one call at a time.

2. **`/audit-security` agent creation.** User raised whether to
   create a new agent for known-vulnerability scanning + file-parser
   hardening + DLL safety review + auto-updater chain audit, mirroring
   the `/audit-licenses` cadence. Tier-1 scope (vendored CVE +
   file-parser + DLL + save-file XXE) is V1-relevant; Tier-2 (network
   code + appcast verify + signature-verify chain) becomes relevant
   when QA-Updater lands. Open question: build-now (so QA-RC includes
   the security sweep) vs build-when-network-features-land.

3. **Crash-report + symbol-server pipeline.** While explaining what
   ships on a user's computer, user observed that `.pdb` files (which
   map crash addresses back to source lines) are part of post-release
   support infrastructure that needs scoping. Pairs naturally with
   QA-Updater work since both deal with post-release flows. Four
   moving parts: `.pdb` generation + archival; in-app crash reporter
   (WER vs Sentry/Bugsnag/Crashpad); symbol-server hosting; sym-
   bolication tooling. Decision calls: third-party SDK vs OS-native
   WER; symbol-server hosting model; consent flow (prompt-per-crash
   vs EULA-blanket-consent).

**Decision:** capture all three as Future State entries (`CL-288`
AlertWindow API migration, `CL-289` audit-security agent, `CL-290`
crash-report + symbol-server pipeline) AND fold all three into
QA-Audit's scope as a "Pre-release decisions to revisit" docket.
QA-Audit is read-only and produces a manifest -- the decisions docket
is a natural extension. Actual execution (if any decision lands as
"do it") routes into QA-Cleanup-1, QA-Updater, or a new dedicated
batch as appropriate. No new §5 batch row added; no §6 sequencing
change.

**Rationale for routing in QA-Audit (not new batches):** all three
items are read-and-decide work that naturally couples with QA-Audit's
manifest production. Creating separate batches would fragment Phase 6
without adding clarity. Future similar side-findings can fold into
the same docket via Rule 3.

**Inline back-refs:**
- §5 QA-Audit entry has new "Pre-release decisions to revisit" sub-section listing all three items.
- `Future State.md` Cross-cutting Infrastructure has three new sub-clusters: "API consistency / migrations" (CL-288), "Security / Hardening" (CL-289), "Crash reporting / symbol-server infrastructure" (CL-290).
- §9 Forks: this entry (seventh).

**Plan files affected:**
- `Plans & Specs/Main Plan.md` — this entry + §5 QA-Audit scope expansion.
- `Plans & Specs/Future State.md` — CL-288 + CL-289 + CL-290 added.

**Verification:** decisions docket entries get processed during
QA-Audit; outcomes route into QA-Cleanup-1 (if migrate), QA-Updater
(if crash-reporter lands as scope), or stay as Future State items
(if defer). Closure of this fork happens when all three items have a
recorded decision in the QA-Audit close entry of
`Implemented Work Log.md`.

### 2026-05-09 — QA-Md outcome: original "MT no-op in Debug" premise was wrong; MT works in Debug

**Trigger:** QA-Md Phase 2 diagnosis (Tasks 5-7) executed against the
Phase 1 instrumentation (Steps 1-4 commits `d9ed843` / `7c4ba0b` /
`6709fdb` / `830f103`).  Counter capture showed Debug+MT-on workers
doing 91.8% of tasks, watchdog never firing, dispatcher fully engaged
-- essentially identical to Release.  But the user's prior observation
("DSP meter reads identical with MT toggle on vs off in Debug") still
held against the in-app meter readings.

**Diagnosis path:** initial reading was that this is the unanticipated
Branch D from the per-batch plan -- "MT works but meter shows no
difference".  Three plausible explanations: (a) Debug synchronization
overhead exactly cancels parallelism gain, (b) DSP meter is broken in
Debug, (c) meter resolution too coarse.  User then surfaced the
critical pattern by capturing all four quadrants:

| Build   | MT on | MT off |
|---------|-------|--------|
| Release | 33%   | 55%    |
| Debug   | 200%  | 200%   | (capped)

The 200%/200% in Debug looked like equivalence but was actually meter
saturation -- both modes were above the 200% display cap.  Quick
diagnostic edit raised the cap from 2.f to 10.f (display side already
supports up to 999% via `GlobalTransportBar`'s `juce::jlimit(0,
999, ...)`) and re-captured:

| Build   | MT on | MT off | MT improvement |
|---------|-------|--------|----------------|
| Release | 33%   | 55%    | -40%           |
| Debug   | 450%  | 870%   | -48%           |

**MT works in Debug at full efficiency.**  Reduction ratio is
essentially identical to Release; if anything Debug benefits SLIGHTLY
MORE from parallelism (heavier per-task Debug work means more
parallelism opportunity).  The original QA-0a finding #9 was a misread
of the meter, not a bug in the MT engine.

**Decision:** keep the meter cap raise (now 10.f) as the active-
development value -- supports diagnostic visibility for downstream
MT-touching batches; release value (2.f / 5.f / 10.f) deferred to
Phase 6 QA-Audit decisions docket as `CL-291`.  Also fold "MT
diagnostic compile-flag gate" (`CL-292`) into the same docket so
Release ships without exposing the Mixer hamburger "Run MT
Diagnostic" item to end users.  Five total items now live in the
docket.

**No fix shipped:** Phase 3 of the QA-Md plan (Branch A / B / C / D
fix) is not needed.  There was nothing to fix in the MT engine
itself.  Diagnostic value of QA-Md instead lives in the permanent
instrumentation (counters + menu item) for future MT-related batches.

**Carry-Forward contradictions:** carry-forward §1 says "MT engine is
production, default ON".  Per QA-0a's earlier entry, this was true
for Release but flagged as no-op under Debug.  After QA-Md: the
carry-forward statement is now true for both Release AND Debug -- the
Debug "no-op" was a meter-display artifact, not an engine issue.

**Inline back-refs:**
- §5 QA-Audit "Pre-release decisions to revisit" sub-section gains two
  new items (`CL-291` DSP meter cap V1, `CL-292` MT diagnostic compile-
  flag gate); count goes from 3 -> 5.
- §5 QA-Md entry needs no annotation -- batch-close entry in Implemented
  Work Log.md will chronicle the outcome.
- `Future State.md` Cross-cutting Infrastructure gains two new sub-
  clusters: "DSP meter UX" (`CL-291`) and "MT diagnostic compile-flag
  gate" (`CL-292`).
- §9 Forks: this entry (eighth).
- Above seventh entry (2026-05-08 docket creation) gains a "Note"
  pointer to this entry so future readers see the count evolution.

**Plan files affected:**
- `Source/PluginProcessor.cpp` — meter cap permanent edit (2.f -> 10.f) with HOLD-FOR-Phase-6-review comment.
- `Plans & Specs/Main Plan.md` — this entry + §5 QA-Audit additions + seventh-entry pointer.
- `Plans & Specs/Future State.md` — CL-291 + CL-292 added.
- `Plans & Specs/Implemented Work Log.md` — QA-Md batch-close entry (next, Task 10).

**Verification:** QA-Md closes when (a) meter cap permanent edit lands
with comment that flags Phase 6 review, (b) plan-doc references for
CL-291 + CL-292 are in place, (c) Implemented Work Log batch-close
entry chronicles the diagnostic findings + outcome, (d) `/review-batch`
flags no BLOCKERS, (e) QA-0a finding #9 marked resolved-as-misdiagnosed
with pointer to this entry.

### 2026-05-10 — QA-A close routings (Rule 3 application)

**Trigger:** QA-A (STYLE Cluster — Unified BaySickTitleBar Component)
batch-close.  The visual-sweep + cross-engine-consistency work produced
21 findings (#22-#42 in the QA-A close entry), most resolved inside the
batch.  Three routings push work outside QA-A's scope and need plan-doc
capture; three meta conventions / memory rules locked during the batch
need surfacing here so future-batch authors see them in the canonical
plan rather than only in the per-batch close entry.

**Routings (Rule 3 — findings-during-execution that escape this batch):**

- **#25 HarmlessLAF zero-px slider — defensive guard shipped, root
  cause deferred.**  Phase 3.1's `kHdrH 36 -> 32` body-layout shift
  surfaced a latent NaN-coord crash in the `LinearVertical` branch of
  `HarmlessLAF::drawLinearSlider`: when a vertical slider has
  zero-sized bounds (not yet laid out / computed to 0), `fh = (float)
  height = 0` makes the `norm` calc divide by zero, NaN propagates
  through `thumbY` + the cap rect, and Direct2D's `coordsToRectangle`
  asserts.  Symptom-side fix shipped this batch (commit `679af33` —
  early-return when `width <= 0 || height <= 0`).  Upstream "why is
  the slider 0 px in the first place" deferred to **Phase 6 / QA-Audit
  decisions docket** as `CL-293`.  Pairs with QA-Audit's existing
  pre-release decisions cluster (CL-288 .. CL-292).

- **#39 VibePlayer/* -> BaySickPlayer/* internal source rename —
  routed to a new dedicated batch in Phase 6.**  User-facing brand
  ("BaySickPlayer") is locked since QA-A; commit hygiene during this
  batch already used "BaySickPlayer" in body text.  But internal
  source files / class names still use `VibePlayer*` per CLAUDE.md's
  longstanding "rename deferred" footnote.  At QA-A close, parent
  elected a **dedicated batch (QA-PlayerRename) within Phase 6 —
  cleanup phase — slotted after QA-Cleanup-1**.  Rationale: this is
  cleanup work, belongs with the other cleanup batches; sequencing
  after QA-Cleanup-1 means the rename only touches files that
  survived the Dead-source deletion pass (no point renaming files
  that were about to be deleted).  See §5 Phase 6 QA-PlayerRename
  entry; §6 Phase-6 sequencing arrow updated.

- **#40 Piano Roll deep-link button crash — already routed at QA-0
  close; re-sighted only.**  Stack: `StandaloneEditor::showPageForTab`
  line 4135 `<lambda_14>::operator()(int i)`.  Same crash family as
  findings #13 + #14 (captured-raw `InstPage*` in lambda freed during
  engine swap or project reload).  Routing into QA-E was decided in
  the third Forks entry (2026-05-07 QA-0 close routings).  Phase 4.4's
  InstPage chrome refactor is **untouched** by the crash path; no new
  routing required.  Captured here for traceability so future readers
  scanning the QA-A close don't re-route what's already routed.

**Conventions / discipline locked during the batch (process-shaping):**

- **`Plans & Specs/Running Notes/<silly-name>.md` subfolder
  established** (commit `c900f55`).  Per-batch running notes paired
  with `Batch Plans/<silly-name>.md` -- captures findings / decisions
  / sub-task verifications across the lifetime of a batch so the
  batch-close drafter has a single source to compile from.  §0
  approved-subfolders list updated in the same commit.  Affects
  every future batch; not a one-off.

- **Memory rule
  `feedback_draft_doc_running_notes_every_checkpoint.md` locked
  mid-batch.**  `/draft-doc running-notes` fires at every checkpoint
  (post-commit, post-sub-task, post-finding, post-spec-call,
  post-scope-pivot), not "at the end".  Output goes to
  `Plans & Specs/Running Notes/<silly-name>.md`.  Caught back-half
  of QA-A after 11 commits with zero running-notes dispatches; the
  retrospective backfill compiled the Phase 1-3 history; Phases 3-6
  dispatched at every close checkpoint.

- **Memory rule `feedback_plan_mirror_one_way.md` locked.**  Plan-mode
  forces the planning file to `~/.claude/plans/<silly-name>.md` for
  its UI; canonical project location is
  `Plans & Specs/Batch Plans/<silly-name>.md`.  After ExitPlanMode
  mirror, the home-dir copy is **deleted** so only one source of
  truth exists; do not back-copy after canonical edits.

- **Memory rule `feedback_match_jeff_text_casing.md` locked** (commit
  `0c4431d`).  Engine names in user-facing UI strings render in brand
  mixed-case ("Harmless", "BaySickPlayer", "BaySickSynth", "BaySickBass",
  "BaySickNAM/IR", "BaySickVocals", "BaySickPedals", "BaySickGuitars",
  "BaySickBasses", "BaySickRustyDrums") — never up-case unilaterally,
  even when legacy paint code did so.  This memory rule is what
  drives QA-PlayerRename's source-side scope: brand-mixed-case is now
  the canonical convention and the `VibePlayer*` internal names
  diverge from it.

**Mid-batch plan edits:**

- `d529602` — expanded QA-A Phase 4 scope to cover BaySickGuitars /
  BaySickBasses / BaySickRustyDrums (the three sfizz-driven kit
  engines that share `AriaControlPanel`).  Original plan only listed
  Pedals.
- `0c4431d` — engine title casing sweep (memory + plan + source
  files).

**Decision:** keep the QA-A close entry's Finding #39 row pointing at
`QA-PlayerRename` (now a real §5 row) rather than the original "QA-
Cleanup-1 or dedicated batch — parent decides at close" placeholder.
QA-Audit's pre-release decisions docket gains `CL-293` (HarmlessLAF
zero-px root cause).

**Carry-forward contradictions:** none.  QA-A's scope was UI / theming
+ existing-component refactor; no architectural primitives in
Carry-Forward §1-§3 changed shape.  The new `BaySickTitleBar` family
is additive.  Memory rules locked during the batch are workflow
discipline, not architectural facts (Carry-Forward §1-§3 unaffected).

**Inline back-refs:**
- §5 Phase 6 gains a new `QA-PlayerRename` entry after QA-Cleanup-1;
  §6 Phase-6 sequencing arrow updated with new `******` footnote.
- §5 QA-Audit "Pre-release decisions to revisit" sub-section gains
  `CL-293` (HarmlessLAF zero-px root cause).  Count goes from 5 -> 6.
- §0 approved-subfolders list already updated by `c900f55`; no edit
  needed here.
- `Future State.md` Cross-cutting Infrastructure: optional `CL-293`
  cluster pointer (parent decides at apply time -- Phase 6 / QA-Audit
  routing is sufficient on its own).
- §9 Forks: this entry (ninth).

**Plan files affected:**
- `Plans & Specs/Main Plan.md` — this entry + §5 Phase 6 QA-
  PlayerRename row (slotted after QA-Cleanup-1) + §5 QA-Audit
  `CL-293` addition + §6 Phase-6 sequencing arrow + new `******`
  footnote.
- `Plans & Specs/Implemented Work Log.md` — QA-A close entry's
  Finding #39 row updated to point at QA-PlayerRename (was "parent
  decides at close" placeholder).
- `Plans & Specs/Future State.md` — `CL-293` (optional; parent
  decides at apply time).

**Verification:** QA-A's close commit lands with (a) `Implemented
Work Log.md` close entry's Finding #39 row pointing at
QA-PlayerRename, (b) this §9 Forks ninth entry, (c) §5 QA-PlayerRename
row + §5 QA-Audit `CL-293` addition, (d) §6 sequencing arrow + new
`******` footnote, (e) `/review-batch` already returned READY-TO-
COMMIT (run prior to this entry's draft).

### 2026-05-10 — QA-B deferred from Phase 1 to after QA-E (mute-isolation dependency)

**Trigger:** QA-B kickoff session (2026-05-10). Pre-batch read of Plans & Specs surfaced the DSP-12 verification matrix's simultaneous case ("Builder + piano roll both placed, both play summed") as the architectural heart of QA-0's Composite RenderTask fix. User flagged that clean verification of the simultaneous case requires mute-isolation testing, and that pattern-block mute / track-row mute don't work yet (QA-0 close findings #16a, #16b, #21 — all routed to QA-E).

**Diagnosis:** QA-B's job is independent confirmation of DSP-12 + a watch-check on DSP-07. The DSP-12 matrix has 7 cells; 6 are single-flow cases (WAV/MP3 × Builder/Clips × MT-on/off) that don't depend on mute behavior. The 7th cell (simultaneous case) is the one cell that exercises the Composite RenderTask's summation logic — which is the entire reason the fix exists. Without mute, that cell can only be verified via "two distinct audio contents + meter inspection," materially weaker than the canonical mute-A → mute-B → both check.

**Options surfaced:**
- 6a. Run QA-B now with the workaround verification.
- 6b. Run 6 single-flow cells now, defer simultaneous-case to a follow-up after QA-E.
- 6c. Defer the entirety of QA-B (DSP-07 + all 7 DSP-12 cells) to after QA-E.

**Decision:** 6c. Locks the verification batch onto a known-good substrate where the canonical isolation technique is available. Avoids the "run-once-now-then-rerun-after-QA-E" recursion that 6a / 6b would force.

**Sub-spec calls resolved at 2026-05-10:**
- **(A) Placement:** immediately after QA-E (new arrow segment `... → QA-D → QA-E → QA-B → QA-F → ...`). Earliest moment clean verification is possible; further delay only adds drift.
- **(B) DSP-07 split-or-defer:** defer with the rest of QA-B. User wants extra observation window to attempt repro between now and QA-E landing; if it doesn't surface in that window, QA-B will close DSP-07 as parked-no-resurfacing. Post-launch user reports remain the long-term tripwire.

**Time-decay risk assessment:** Low. None of the intermediate batches (QA-C one-liners — DSP-10 + MIX-01; QA-D Project State Reset — STATE-01/02/04; QA-E audio-row + tab-callback + mute-dispatch — findings #13/14/16a/16b/21) touch the audio-insert composite-task surface directly. Waiting doesn't expose the DSP-12 surface to silent regressions in the path of intermediate work.

**Carry-forward contradictions:** None. Carry-Forward §1 indexes the file:line primitives; QA-B was always characterized as zero-risk diagnostic. Re-sequencing doesn't change the architectural facts.

**Inline back-refs:**
- §5 QA-B entry: header flagged with deferral pointer; Dependencies updated to reference QA-E mute fixes; new "Sequencing note (2026-05-10)" subsection.
- §6 sequencing arrow: QA-B moved from after QA-A to after QA-E; existing parallel-callout (line ~1300) updated to drop QA-B from the parallel group; new footnote `*******` chronicling the deferral.
- §9 Forks: this entry (tenth).

**Plan files affected:**
- `Plans & Specs/Main Plan.md` — this entry + §5 QA-B sequencing note + §6 arrow / footnote edits.
- No source code changes.
- No new running-notes or batch-plan files (QA-B kickoff aborted; will resume after QA-E lands).

**Verification:** This fork closes when QA-B actually runs (post-QA-E) and produces a normal Implemented Work Log close entry. Until then, the deferral itself is the active state.

### 2026-05-11 — QA-D close NIT bulk-defer corrected mid-QA-E (Rule 3 routing + new process gate)

**Trigger:** QA-E open session (2026-05-11).  Pre-batch read of the
QA-D close entry surfaced that the 4 NITs returned by `/review-batch`
on 2026-05-10 had been bulk-deferred into the close entry's
"Deferred NITs" section without each finding being individually
surfaced to Jeff for fix-vs-defer call.  That bulk-defer violates
`feedback_qa_batches_fix_bugs_dont_defer.md` (locked mid-QA-D
2026-05-10 — "real bugs surfaced mid-QA-batch get fixed in-batch by
default; deferral requires explicit justification + Jeff's call").
The QA-D close commit `d6c84c6` landed before Jeff had the chance to
make the call per-finding.

**Diagnosis:** of the 4 NITs, three are scope-completion gaps inside
QA-D's own commits and one is dead-code shape that belongs with the
Phase 6 cleanup pass:

- **NIT-1 — `BaySickRustyDrumsPage` missing from `onTabRenamed`
  dispatch** (`Source/Standalone/StandaloneEditor.cpp:1263-1315`).
  QA-D Task 2.8's explicit success criterion was "ribbon-rename
  propagates to piano-roll label."  The dispatch landed for 6 page
  types but omitted `BaySickRustyDrumsPage` -- functional regression
  on the kit-engine page, in scope of the task that shipped.

- **NIT-2 — `restoreAudioStripsFromArrangement` `clearDirty()`
  assumes load-path-only callers.**  QA-D Task 3 commit `6288e85`
  introduced the `clearDirty()` call; the inline comment ("called
  only on project load") is documentation, not a guard.  If a future
  caller invokes the restore path outside load (rebuild / repair /
  partial-merge), the dirty marker silently clears under user edits.

- **NIT-3 — Legacy `"Drums"` / `"Layers"` / `"Bass"` tab names from
  pre-QA-D saved projects don't bump the new monotonic counters.**
  QA-D Task 2 commit `a8796c9` introduced the STATE-02 monotonic
  counters but left the pre-QA-D-saved-project migration path
  unaddressed; loading an older project leaves the next-suffix
  counter at 0 and the next user-added tab collides.

- **NIT-4 — Per-page `LayersPage::setTabName` writeback to dead
  `mPianoRoll` state.**  QA-D Task 2.6 / 2.7 / 2.8 added the
  writeback path that lands at a now-dead piano-roll state member.
  Dead-code shape, not a functional bug.

**Decision (R2 path — fix in current open batch + §9 Forks entry
that back-refs the prior batch):**

- **NIT-1 / NIT-2 / NIT-3** fold into QA-E as a new final sub-phase
  ("Sub-Phase Z — QA-D NIT corrections"), sequenced after the Dead
  Properties cleanup work and before the close sequence.  Functional
  fixes, mechanical to apply, sit naturally with QA-E's existing
  scope (tab-callback wiring + project-state correctness).
- **NIT-4** routes to QA-Cleanup-1 (Phase 6 source-code cleanup) per
  Jeff's call.  The per-page `mPianoRoll` dead-writeback is
  dead-code cleanup, not a functional fix, and belongs with the
  other dead-code-deletion work in QA-Cleanup-1.
- QA-D's commits stay intact; **no rewrite**.  The QA-D close
  entry's "Deferred NITs" section gets a small amendment note
  pointing to this Forks entry so future readers see the correction
  trail.

**Process discipline locked mid-this-conversation
(workflow-shaping):**

- **New memory rule
  `feedback_closed_batch_carryforward_via_forks.md`.**  Findings
  discovered AFTER a previous batch's close (re-sighting of deferred
  items, audits surfacing gaps, `/review-batch` outcomes at close
  that weren't individually surfaced) get fixed in the current open
  batch + recorded via a §9 Forks entry that back-refs the original
  batch.  Never reopen the prior batch's commits.  Never leave
  deferred without Jeff's call.
- **Update to `feedback_qa_batches_fix_bugs_dont_defer.md`.**
  Cross-ref pointer to the new memory rule so the existing
  in-batch-fix-by-default rule explicitly covers both mid-batch AND
  post-close cases.
- **Going-forward gate:** every future `/review-batch` outcome
  surfaces each finding individually with options before the close
  entry is drafted.  No bulk "all deferred (pre-existing /
  harmless)" framing.  Severity (BLOCKER / NEEDS-FIX / NIT) does
  not collapse the surface-individually requirement.

**Carry-forward contradictions:** none.  NITs are minor by
definition; routing them through the canonical Forks mechanism
doesn't change architectural facts in Carry-Forward §1-§3.  QA-D's
shipped fixes (STATE-01/02/04 + MenuBarModel listener-dangle) remain
correct as committed.

**Inline back-refs:**
- §5 QA-E entry: new "Sub-Phase Z — QA-D NIT corrections"
  sub-section under the folded findings list; bullets NIT-1 / NIT-2
  / NIT-3 with brief fix shape + back-ref to this Forks entry.
- §5 QA-Cleanup-1 entry: scope list gains NIT-4 (per-page
  `mPianoRoll` dead-writeback removal).
- `Implemented Work Log.md` QA-D close entry: amendment note in the
  "Deferred NITs" section reading "Premature bulk-defer; routing
  corrected via §9 Forks entry 2026-05-11."
- §9 Forks: this entry (eleventh).

**Plan files affected:**
- `Plans & Specs/Main Plan.md` — this entry + §5 QA-E
  "Sub-Phase Z" sub-section + §5 QA-Cleanup-1 scope addition.
- `Plans & Specs/Implemented Work Log.md` — QA-D close entry
  amendment note.
- `Plans & Specs/Batch Plans/phantom-recording-mongoose.md` (QA-E
  plan file) — Sub-Phase Z entry routing to this Forks entry.
- `C:\Users\jeffm\.claude\projects\C--Users-jeffm-Documents-BaySickDAW\memory\feedback_closed_batch_carryforward_via_forks.md`
  (new memory file).
- `C:\Users\jeffm\.claude\projects\C--Users-jeffm-Documents-BaySickDAW\memory\feedback_qa_batches_fix_bugs_dont_defer.md`
  (cross-ref pointer added).

**Verification:** fork closes when (a) §5 QA-E entry updated with
Sub-Phase Z, (b) §5 QA-Cleanup-1 entry updated with NIT-4, (c)
`Implemented Work Log.md` QA-D close amendment note added, (d) both
memory rule files saved, (e) QA-E plan file
(`phantom-recording-mongoose.md`) drafted with Sub-Phase Z routing
correctly to this Forks entry, (f) Sub-Phase Z's three fixes
verified by Jeff in Debug + Release at QA-E close.

### 2026-05-11 — QA-E Sub-Phase A scope expanded: showPageForTab lambda crashes affect all 7 page-type branches, not 2

**Trigger:** QA-E open (2026-05-11).  User repro'd one of the
use-after-free crashes originally captured at QA-0 close (findings #13 +
#14) by clicking the "Drum Kit" sub-tab button on a DrumPage's menu bar.
The original captures had identified only TWO crash sites in
`StandaloneEditor::showPageForTab` -- #13 InstPage at line 4135 + #14
ClipsPage Piano Roll button at line 4048 -- on the implicit assumption
that those were the surfaces actually exercised in normal use.  User
asked the obvious follow-up: the deep-link sub-tab buttons are the same
button class doing the same thing across page types, so why would only
two of seven be vulnerable.

**Diagnosis:** Source audit of `StandaloneEditor::showPageForTab`
(`Source/Standalone/StandaloneEditor.cpp`) confirmed all SEVEN
page-type branches contain the unsafe raw-page-pointer capture
pattern.  Each branch's `mPageMenuBar->setTabSlots(...)` callback
captures the page's raw pointer (`lp` / `bp` / `cp` / `vp` / `ip` /
`dp` / `rp`) into the onClick lambda; sub-tab buttons (Player /
Piano Roll / Drum Kit / FX Rack / etc.) dispatch through those
lambdas.  When the page is destroyed between dispatch and fire
(engine swap, project reload, tab delete + re-add), the lambda
fires with a dangling pointer.

Vulnerable branches (all in `Source/Standalone/StandaloneEditor.cpp`):

| Page type             | setTabSlots callback line | Status                            |
|-----------------------|---------------------------|-----------------------------------|
| LayersPage            | 4080                      | suspected (untested)              |
| BassPage              | 4114                      | suspected (untested)              |
| ClipsPage             | 4156                      | original #14 finding              |
| VoxPage               | 4197                      | not user-tested                   |
| InstPage              | 4239                      | original #13 finding              |
| DrumPage              | 4300                      | user repro confirmed 2026-05-11   |
| BaySickRustyDrumsPage | 4334                      | not user-tested                   |

Secondary finding: the existing "inner SafePointer" pattern in 5 of 7
branches (ClipsPage 4144 / LayersPage 4070 / BassPage 4104 / VoxPage
4185 / InstPage 4219 / DrumPage 4287) is itself wrong.  The
`juce::Component::SafePointer<X> safe (xp)` line lives INSIDE the
lambda body and is constructed from a raw `xp` that may already be
dangling at fire time.  Constructing a SafePointer from a freed raw
pointer is undefined behavior -- the surrounding code "looks like"
it's using SafePointer correctly, but the protective primitive is
created too late to do its job.  Correct pattern: lift the
SafePointer construction to the OUTER scope of each branch (right
after the `dynamic_cast`) and capture the SafePointer (not the raw
pointer) into every lambda below.

**Options surfaced:**
- **C-i (SafePointer-at-outer-scope).**  Lift
  `juce::Component::SafePointer<XxxPage> safe (xp);` to the outer
  scope of each branch; replace every `[this, xp, ...]` capture
  with `[this, safe, ...]`; inside lambdas use
  `if (auto* p = safe.getComponent()) { ... }`.  Mechanical,
  branch-symmetric, matches the convention used elsewhere in the
  same file, silent no-op when the page is gone.
- **C-ii (index-lookup at fire-time).**  Capture only the tab index
  + page kind; re-resolve the live page from the tab manager when
  the lambda fires.  Strictly safer (cannot dangle by construction)
  but surprising UX after engine swap (sub-tab click could land on
  a different engine instance than the one the user clicked from)
  and adds a new lookup primitive that doesn't exist elsewhere.

**Decision (Rule 3 + closed-batch carry-forward via Forks):**
Expand QA-E Sub-Phase A scope from "InstPage + ClipsPage lambdas
only" (findings #13 / #14 / #40 / #55) to **all seven page-type
branches in `showPageForTab` plus their `setTabSlots` callbacks**.
Capture pattern: **C-i (SafePointer-at-outer-scope)** -- matches the
existing convention in the same file, most JUCE-idiomatic, safest
fallback (silent no-op instead of crash), no new primitive
introduced.  Index-lookup (C-ii) rejected for the post-swap UX
surprise.

Effort estimate for Sub-Phase A bumps from ~30 min (two branches)
to ~1.5-2 hr (seven branches + the inner-SafePointer-too-late fix
in 5 of those + verify), all confined to a single function.

**Carry-forward contradictions:** None.  Carry-Forward §1-§3 doesn't
describe this specific pattern; the fix is mechanical lambda-capture
hygiene, not an architectural primitive change.  No new vocabulary
or new file:line primitives need adding to Carry-Forward.

**Inline back-refs:**
- §5 QA-E entry: Sub-Phase A scope description rewritten -- "originally
  captured 2 sites (#13 InstPage at line 4135, #14 ClipsPage at line
  4048); audit at QA-E open 2026-05-11 expanded to all 7 page-type
  branches (LayersPage 4080, BassPage 4114, ClipsPage 4156, VoxPage
  4197, InstPage 4239, DrumPage 4300, BaySickRustyDrumsPage 4334) +
  their inner-SafePointer-too-late fixes (ClipsPage 4144, LayersPage
  4070, BassPage 4104, VoxPage 4185, InstPage 4219, DrumPage 4287)".
  Effort estimate bumped from ~30 min to ~1.5-2 hr.  Capture pattern
  locked as SafePointer-at-outer-scope.
- §9 Forks: this entry (twelfth).

**Plan files affected:**
- `Plans & Specs/Main Plan.md` -- this entry + §5 QA-E entry update
  (Sub-Phase A scope + effort estimate + capture pattern).
- No source code changes in this routing entry; the SafePointer
  rewrite lands as part of QA-E Sub-Phase A execution.
- No new running-notes or batch-plan files (QA-E plan + running-notes
  files already exist; the scope expansion gets recorded there in
  the normal QA-E flow).

**Verification:** This fork closes when (a) §5 QA-E entry reflects
the expanded scope + the C-i capture pattern call, (b) QA-E Sub-Phase
A lands the SafePointer-at-outer-scope pattern across all 7 branches
+ removes the inner-SafePointer-too-late lines from the 5 affected
branches, (c) user verifies in Debug + Release that the DrumPage
"Drum Kit" sub-tab crash repro no longer fires + spot-checks the
other 6 page types' sub-tab buttons show no regression.

### 2026-05-11 — QA-J scope expanded with FilePlay multi-clip restructure (QA-E open-time finding)

**Trigger:** at QA-E open (2026-05-11), the user was confirming
FILE-01's intent — both Vox wet + dry rendered clips visible in the
browser, both droppable on the Builder grid, both routable through the
same Vox page.  Source-side verification of the multi-clip-to-one-page
case at [Source/PluginProcessor.cpp:2415-2433](Source/PluginProcessor.cpp:2415)
(the FilePlay Pass 1 loop) showed that the architecture does support
that routing — but in a way that processes each clip SEQUENTIALLY
through the same engine + insert chain within a single audio block,
rather than mixing the inputs once and running the chain once on the
sum.  User flagged that this was never spec'd in the Batch 9b Item 9
FilePlay routing work and is unacceptable for v1.

**Diagnosis:** the FilePlay Pass 1 loop iterates
`mCurrentBlockClipSnapshot->players` and calls `renderFilePlayPlayer`
once per FilePlay-routed clip.  Each call (Vox branch at
[Source/PluginProcessor.cpp:867-901](Source/PluginProcessor.cpp:867))
clears the engine scratch buffer, copies one clip's audio in, calls
`eng->processBlock(...)`, then routes via `mVibeGraph.processInsert` +
`routeInsertOutput`.  Engine state + insert chain state persist across
calls.  Two clips routed to the same Vox page = engine processBlock +
insert chain runs TWICE on the same engine instance, sequentially,
with state carry-over between calls.

User-facing intent (both clips through the chain → summed at the
parent bus) is delivered.  Second-order behavioral correctness is
wrong: compressor envelope follower from clip A leaks into clip B's
processing pass; reverb tail builds in arrival order; LFO phase
advances per call.  Stateful DSP nodes in the insert chain are not
designed to be re-entered mid-block on overlapping inputs.

Industry-standard pattern: traditional audio tracks (FL Studio,
Ableton, Logic, Pro Tools) sum the contents of an audio track into one
buffer first, then run the insert chain ONCE on the sum.  Per-clip
processing belongs to ARA-style offline editors (Melodyne, Newtone),
not the live render path.

The same architectural shape — "rack/EQ runs once per clip instead of
once on sum" — already has a routed batch in §5: QA-J
(DSP-06 Multi-Clip Stacking).  QA-J's existing scope is to fix this
pattern for NON-FilePlay clips (`renderAudioClipsForRow`, per-row
rack/EQ).  The FilePlay path has the identical pattern on different
code (per-page Vox/Inst engine + insert chain).

Note: an agent dispatch during the diagnosis speculated about FL
Studio's internal ordering and was overruled by the user (FL DOES
support audio-clip playback through Vox/Inst-style setups, and FL
doesn't process per-clip in that flow).  Per
`feedback_dont_speculate_about_fl_studio.md`, agent dispatches that
speculate about FL Studio should be caught before passing through.

**Decision (OPT-A — fold into QA-J):** extend QA-J's scope to cover
the per-clip-through-engine path across all three engine families
that drive audio clips through their chains (Vox + Inst + Clips):

- **Non-FilePlay (existing):** `renderAudioClipsForRow` per-row
  pre-pass that mixes all row clips into one buffer before running
  the rack/EQ once per row.
- **FilePlay extended (new):** per-page pre-pass that mixes all
  clips routed to the same Vox / Inst / Clips page into one buffer
  before running the engine + insert chain once per page.  Touches
  the Pass 1 loop at
  [Source/PluginProcessor.cpp:2415-2433](Source/PluginProcessor.cpp:2415),
  `renderFilePlayPlayer` at
  [Source/PluginProcessor.cpp:867-901](Source/PluginProcessor.cpp:867),
  and the MT-path equivalents in
  [Source/Engine/Tasks/VoxStripTask.cpp](Source/Engine/Tasks/VoxStripTask.cpp)
  and [Source/Engine/Tasks/InstStripTask.cpp](Source/Engine/Tasks/InstStripTask.cpp).
- **Clips routing unification (new — amended 2026-05-11 same day as initial entry):**
  Pass 1 loop's `isVox || isInst` filter expands to include Clips
  channels.  Audio clips placed on the Builder grid that reference
  clips loaded in a Clips page default their `routeChannel` to that
  Clips page's channel ID (currently defaults to 0 / row audio
  insert).  Net result: regardless of whether a Clips clip is
  triggered via piano roll OR placed on the Builder grid, audio
  flows through the same Clips engine + InsertNode chain.  This was
  the intended design (user-stated 2026-05-11 — "regardless of where
  you add it is available both in the piano roll and on the builder
  page all playing through one place"); the current split routing
  (piano-roll → Clips InsertNode; grid-placed → row audio insert) was
  a Batch 9b Item 9 oversight that surfaced during QA-E open under
  FILE-02 routing scoping.

**Test premise correction (amended 2026-05-11):** the DSP-12
verification matrix's simultaneous case ("Builder + piano roll both
placed, both play summed") was originally scoped to verify both
play simultaneously.  It did NOT verify both play through the
SAME chain.  Under the actual current implementation (split
routing), the test passed against a premise that doesn't match the
intended design.  Once QA-J lands the Clips routing unification,
QA-B's deferred DSP-12 simultaneous-case test re-verifies under the
corrected premise: "both play simultaneously THROUGH THE SAME Clips
engine + InsertNode chain, with the chain running once per block on
the summed input."  QA-B sequencing decision (Option A — slide
entirely to after QA-J close) resolved 2026-05-11 same-day; see §9
fourteenth Forks entry + §5 QA-B test-premise addendum.

QA-J's effort estimate goes from ~8-12 hours to ~12-16 hours (bumped
2026-05-11 initial entry) and bumps further to ~13-18 hours with
the Clips routing unification work folded in (adds ~1-2 hours for
Pass 1 filter extension + default-routeChannel logic).  Risk
stays "high — architectural restructure, audio thread, MT-aware"
(same risk tier).  Dependencies unchanged: QA-0 (composite task
pattern established) + QA-E (audio clip surface stability) per
existing declarations.

**Carry-forward contradictions:** None.  Carry-Forward Reference's MT
primitive section (§1) describes the dispatcher + task subclasses
architecturally; the per-clip-vs-per-page-summed distinction is a
within-task design choice, not an architectural primitive change.

**Inline back-refs:**
- §5 QA-J entry: new sub-bullet under scope — "Folded in 2026-05-11
  (QA-E open-time finding via Rule 3)" — extending the once-per-sum
  restructure to the FilePlay path (per-page engine + insert chain).
  Two clips routed to the same Vox/Inst page should mix into one
  input buffer per page, then run the engine + insert chain once per
  page, rather than processing each clip sequentially through the
  shared engine state.  Effort estimate range updated to "~12-16
  hours; folded FilePlay restructure adds ~3-4 hours".
- §9 Forks: this entry (thirteenth).

**Plan files affected:**
- `Plans & Specs/Main Plan.md` — this entry + §5 QA-J scope sub-bullet
  + effort estimate bump.
- No source code changes from this Forks entry itself; the fix lands
  in QA-J (post-QA-E per existing dependency).

**Verification:** fork closes when (a) §5 QA-J entry gains the folded
sub-bullet + updated effort estimate, (b) QA-J actually runs (post-
QA-E per existing dependency) and produces an Implemented Work Log
close entry that covers BOTH the non-FilePlay and FilePlay
restructures.

### 2026-05-11 — QA-B second deferral resolved (after-QA-E → after-QA-J, Option A)

**Trigger:** the §9 thirteenth Forks entry (amended same day) surfaced
an open sequencing call for QA-B in light of the DSP-12 test premise
correction.  The amended entry's "Test premise correction" section
noted that the simultaneous-case test ("Builder + piano roll both
placed, both play summed") couldn't be properly verified until QA-J
landed the Clips routing unification, and surfaced three options for
QA-B's sequencing without unilaterally picking one.

**Diagnosis:** the architecturally-correct DSP-12 simultaneous-case
test requires "both play simultaneously THROUGH THE SAME Clips engine
+ InsertNode chain."  Source for that unified routing lands in QA-J
(§9 thirteenth Forks entry, amended same day).  Running QA-B against
split-routing source produces a passing test against the wrong premise
(the original 2026-05-10 first-deferral pattern that buys
methodologically-sound verification doesn't help if the verification
itself targets the wrong premise).  Three sequencing options were
surfaced.

**Options surfaced:**
- **(a)** Slide QA-B entirely to after QA-J close.
- **(b)** Run other DSP-12 cells (single-flow cases) after QA-E close
  as originally planned; hold the simultaneous-case sub-test for after
  QA-J close.  Two-pass execution.
- **(c)** Other.

**Decision (user spec call 2026-05-11):** Option (a).  Slide QA-B
entirely to after QA-J close.  Avoids two-pass execution overhead;
keeps QA-B as a single atomic verification batch on a known-good
substrate (unified-routing source).  Aligns with the original
zero-risk diagnostic framing of QA-B — better to run once against
the corrected premise than fragment the batch.

**Time-decay risk assessment:** Low.  None of the intermediate batches
between QA-E and QA-J (QA-F / QA-Fa / QA-G / QA-H / QA-I) touch the
audio-insert / FilePlay surface directly.  Waiting until after QA-J
doesn't expose the DSP-12 architectural surface to silent regressions
during intermediate work.

**Carry-forward contradictions:** None.  Carry-Forward Reference's MT
primitive section (§1) describes the dispatcher + task subclasses
architecturally; QA-B's slot in the verification sequence is a
sequencing decision, not an architectural change.

**Inline back-refs:**
- §6 sequencing arrow: QA-B moved from after-QA-E to after-QA-J
  (between QA-J and QA-K).  Footnote `*******` updated to chronicle
  both deferrals (2026-05-10 from after-QA-A to after-QA-E; 2026-05-11
  from after-QA-E to after-QA-J).  Parallel-group note updated.
- §5 QA-B entry: "Test premise correction + sequencing decision
  (2026-05-11)" line now explicitly records Option A as resolved.
  Dependencies clause already references QA-E mute fixes; updated to
  also reference QA-J routing-unification fix.
- §9 thirteenth Forks entry: "Test premise correction" section
  back-refs this entry for the sequencing-decision close.
- §9 Forks: this entry (fourteenth).

**Plan files affected:**
- `Plans & Specs/Main Plan.md` — this entry + §6 arrow / footnote /
  parallel-group note + §5 QA-B test-premise-line update + §9
  thirteenth back-ref.
- No source code changes.
- No new running-notes or batch-plan files (QA-B kickoff aborted; will
  resume after QA-J lands).

**Verification:** fork closes when QA-B actually runs (post-QA-J) and
produces a normal Implemented Work Log close entry validating the
corrected DSP-12 simultaneous-case test premise on unified-routing
source.  Until then, the deferral itself is the active state.

### 2026-05-11 — Clips strip restore audit: no K-6 parallel fix needed (QA-E Task 3 spec call)

**Trigger:** at QA-E Task 3 diagnosis (2026-05-11), the root cause of
MIX-02 (Vox/Inst tab reload destroys mixer strip) surfaced as an
asymmetric K-6 fix -- the Vox restore path is missing the parallel
safety-net `addVoxChannelAtIndex(pageIndex)` call that the Inst restore
path got on 2026-05-05.  Once the Vox-side fix shape was clear, the
obvious follow-up audit-question got raised: does the Clips restore
path have the same parallel hole, and if so should QA-E Task 3 fold a
matching fix in (F-B in the Task 3 spec-call list)?  User approved
F-B as "folded in and added to forks" -- implying both the
investigation should happen and a Forks entry should land documenting
the result.

**Diagnosis:** Clips mixer strips are architecturally different from
Vox / Inst strips, so the K-6 parallel does not cleanly transfer:

- **Vox / Inst strips are PAGE-KEYED.**  Page index N maps directly
  to mixer strip prefix `mixer_vox_N` / `mixer_inst_N`.
  `restoreStripNames("VoxNames" / "InstNames")` and the K-6 safety-net
  `addVoxChannelAtIndex(pageIndex)` / `addInstChannelAtIndex(pageIndex)`
  both feed into the page-keyed `mAudioStrips`.  Invariant: page
  exists -> strip exists, independent of arrangement state.

- **Clips strips are ARRANGEMENT-BLOCK-KEYED.**  Audio row R (where a
  clip is placed on the Builder grid) maps to mixer strip prefix
  `mixer_audio_R`.  Strips are created by
  `restoreAudioStripsFromArrangement`
  ([Source/Standalone/StandaloneEditor.cpp:9796-9811](Source/Standalone/StandaloneEditor.cpp:9796))
  walking `mPM->getBlock(...)` for `ClipType::Audio` blocks.
  ClipsPage exists at pageIndex P; audio row R might or might not
  have a clip block.  Invariant: block exists -> strip exists; page
  existence is independent.

The natural case (ClipsPage created WITH a grid block, saved, reloaded)
is correctly handled by `restoreAudioStripsFromArrangement`.  The edge
case (orphan ClipsPage at pageIndex P with no block at row P) would
result in a page without a strip -- but this is rare and recoverable
(the user can drop a clip back on the grid, which fires
`onAudioClipAdded` and creates the strip via the existing add path).

**Options surfaced:**
- **F-B-a (mechanical mirror).**  Apply a K-6-style safety-net call
  in the Clips restore branch (`mMixerPage->addAudioChannel(pageIndex,
  tabName)` after `spawnClipsTabIfMissing`).  Risk: silent
  naming-source-of-truth conflict between the tab name (from XML
  "name" attribute) and the block's `displayAlias` (from
  PatternManager state).  The common case works fine without this;
  the safety-net only helps the orphan-page case but costs naming
  regressions for users who renamed strips but not tabs in old
  projects.
- **F-B-b (skip the source fix, document the investigation).**
  Document the audit result + routing decision in §9 Forks.  If the
  orphan-page case surfaces in user testing as a real bug, route to
  a follow-up batch (QA-J already has Clips routing unification
  scope per §9 thirteenth Forks entry -- natural home for any
  Clips-strip-lifecycle fix).
- **F-B-c (defer the F-B decision entirely).**  Capture as a §5
  sub-item to investigate during Task 3 verify.

**Decision (user spec call 2026-05-11):** **F-B-b.**  Skip the source
fix at QA-E Task 3.  The naming-conflict risk in F-B-a is not
justified by the narrow orphan-page case.  If a real
Clips-strip-lifecycle bug surfaces in user testing, the right home
for the fix is QA-J's Clips routing unification scope (per §9
thirteenth Forks entry, amended same day) -- that batch already
touches the Clips channel routing + page <-> insert wiring surface
and is the natural place for any follow-on safety-net work.

**Carry-forward contradictions:** None.  Carry-Forward Reference
doesn't describe Clips strip lifecycle as page-keyed; the
arrangement-block-keyed model is the canonical understanding.  This
audit reinforces that distinction -- Vox / Inst are page-keyed,
Clips are arrangement-block-keyed, and they have different correct
restore patterns by design.

**Inline back-refs:**
- §5 QA-E entry: no change.  Clips strip restore is NOT in QA-E's
  scoped list; Task 3 focuses on Vox MIX-02 / MIX-04 / MIX-06 only.
  The Clips parallel was an audit-question, not a scoped item, and
  the audit result is "no fix needed at this batch level."
- §9 thirteenth Forks entry: the future home for any
  Clips-strip-lifecycle fixes is QA-J (the Clips routing
  unification batch).  Cross-ref helps future readers find the
  natural follow-up batch if the orphan-page case becomes a real
  bug.
- §9 Forks: this entry (fifteenth).

**Plan files affected:**
- `Plans & Specs/Main Plan.md` -- this entry only.
- No source code changes.
- No new running-notes or batch-plan files (QA-E Task 3 running
  notes already capture the audit + decision separately as part of
  the normal Task 3 flow).

**Verification:** this fork closes when one of two things happens --
(a) QA-E ships without any Clips-strip-related user-reported bug
surfacing in subsequent testing (default: fork stays open as a
"watch item" routed to QA-J's natural follow-up scope), OR (b) a
real Clips orphan-page bug surfaces and gets routed to QA-J or a
dedicated batch, at which point this Forks entry references the new
batch and the F-B-b routing decision is superseded.

### 2026-05-12 — QA-E Task 3 root-cause: MT pre-scan gap + Clips-strip restore guard + Task 9 added

**Trigger:** at QA-E Task 3 verify (2026-05-12), the audio-routing
fix that landed in working tree (F-A K-6 Vox mirror + R-1
routeChannel persistence + 15th Forks F-B-b Clips audit no-fix
decision) was tested.  Pre-save fresh-record + Play through Vox/Inst
was silent.  Post-save + reload + Play was also silent (regression
vs. pre-R-1, which played audio through phantom Clips strips).
Diagnostic instrumentation added across `Source/PluginProcessor.cpp`
+ `Source/Standalone/StandaloneEditor.cpp` (~15 `[QA-E DIAG]` sites)
traced the failure.

**Diagnosis chain:**

1. **Recording side is correct.**  `commitRecordingResult` creates
   blocks with correct routes (Vox channelIds 600..605, Inst
   700..719).  `rebuildAudioClipPlayers` adds them to the audio
   snapshot with `streamer=ok`.
2. **`processBlock` is running normally.**  ENTRY + PAST-BARRIER
   heartbeats fire every ~3s with `barrier=0`.
3. **Pass 1 / Pass 2 / `renderFilePlayPlayer` are NEVER REACHED in
   MT mode.**  `processBlock` takes the MT branch at
   [Source/PluginProcessor.cpp:1874](Source/PluginProcessor.cpp:1874)
   and returns at line 1934 — skipping the serial Pass 1 path AND
   the pre-scan that populates `mVoxFilePlayActive` /
   `mInstFilePlayActive`.
4. **MT-side FilePlay gating fails.**
   [Source/Engine/Tasks/VoxStripTask.cpp:47](Source/Engine/Tasks/VoxStripTask.cpp:47)
   + the InstStripTask equivalent gate their FilePlay branch on
   `mProcessor->mVoxFilePlayActive[mIndex]`.  With the pre-scan
   never running, those flags stay false → tasks fall through to
   the live-input branch → recorded clip audio never decodes.
5. **A/B confirmation.**  User toggled
   `RenderEngine::gMultiThreadedEngineEnabled` off via the Mixer
   hamburger menu (Multi-core Rendering).  Audio immediately played
   through Vox + 2 Inst strips correctly.  Re-enabled MT → silence
   returned.  Confirmed MT path is the failure mode; serial path
   was correct all along.

**Root cause:** the FilePlay pre-scan was located AFTER the MT
branch's early-return.  Serial path saw the flag set correctly; MT
path never ran the pre-scan.  Architectural gap introduced by
Batch 9a (2026-05-06) when the MT branch was inserted at line 1874
— the pre-scan at line 2143 wasn't lifted with it.

**Separate Clips-strip restore bug surfaced during the diagnosis:**
the project-load block-loop at
[Source/Standalone/StandaloneEditor.cpp:9808-9824](Source/Standalone/StandaloneEditor.cpp:9808)
unconditionally calls `addAudioRowChannel + ensureAudioInsert +
addAudioChannel` for every `ClipType::Audio` block, with NO
`routeChannel` check.  Vox/Inst-routed blocks (`routeChannel != 0`)
trigger phantom Audio strips on reload.  DISTINCT from the F-B-b
audit closed in §9 fifteenth Forks entry — that audit was about
whether a K-6-style parallel safety-net was needed for Clips; this
bug is the deserialize-side block-loop's missing route-guard.  Same
class of fix but a different code path.

**Fixes applied (this entry):**

- **Fix 1 — Pre-scan move:** pre-scan block lifted from
  [Source/PluginProcessor.cpp:2143](Source/PluginProcessor.cpp:2143)
  (after MT early-return) to before the MT branch at line 1860.
  Both serial and MT paths now see correctly-populated
  `mVoxFilePlayActive` / `mInstFilePlayActive`.  Mechanical move,
  no logic change.
- **Fix 2 — Clips-strip route-guard:** `if (b.routeChannel != 0)
  continue;` added at
  [Source/Standalone/StandaloneEditor.cpp:9811](Source/Standalone/StandaloneEditor.cpp:9811)
  in `deserializeUIState` block-loop.  Vox/Inst-routed blocks no
  longer spawn phantom Audio strips on reload.  Mirrors existing
  `if (routeChannel == 0)` guard inside `dropWavAsClip` at line 9912.
- **Fix 3 — F-A (Vox K-6 mirror) retained:** still needed.  Ensures
  Vox mixer strip exists for un-renamed Vox tabs on reload.
- **Fix 4 — R-1 (routeChannel persistence) retained:** still
  needed.  Without it, reloaded blocks default to routeChannel=0 —
  the new pre-scan would NOT flag the right pages AND the new
  Clips-strip guard would NOT skip the block.  Both new fixes
  depend on R-1 producing correct routeChannel values on reload.

**Verified working 2026-05-12:** with all four fixes applied + MT
ON, project reload + Play routes audio through Vox + 2 Inst strips
correctly; no phantom Clips strips on the mixer; no audio bleed
through Audio rows.

**Task 9 added — Dirty-flag investigation (record-finalize side effect):**

User observation during diagnostic: post-record, the project shows
dirty state.  Save clears it.  Reopen sometimes still shows dirty.
Did NOT happen until the WAV files were on the Builder grid (i.e.,
post-`commitRecordingResult`'s `markDirty()` call at
[Source/Standalone/StandaloneEditor.cpp:9979](Source/Standalone/StandaloneEditor.cpp:9979)).
Pattern resembles a QA-D STATE-01 regression — `markDirty` fires
correctly post-record (state did change), but a follow-up cascade
re-flips dirty after save.  Folded as a new **Task 9 — Dirty-flag
investigation** in the QA-E batch plan (inserted between Task 8
Sub-Phase Z and the close sequence, which renumbers to Task 10).
Per `feedback_qa_batches_fix_bugs_dont_defer.md`, real bugs surfaced
mid-QA-batch get fixed in-batch.  Scope: investigate root cause +
fix + verify across all three record modes (Vox-only, Inst-only,
combined Vox + Inst).

**Task 7 fold-in — "Add a new Page" options in Routing dropdown:**

User feature request surfaced during this diagnostic: the FILE-02
Routing dropdown (Task 7 step 2) should include entries for "Add a
new Clip Page", "Add a new Vox Page", "Add a new Inst Page" so the
user can route a clip to a NEWLY-created page without first
navigating to the ribbon to add a tab.  Folded as a sub-bullet under
Task 7 step 2.

**Rule 4 locked alongside this finding:**

Main Plan §0 Rule 4 (2026-05-12) — every diagnostic addition gets
logged in the per-batch Diagnostic Instrumentation Catalog in the
running notes file, with disposition (`Remove at task/batch close`
/ `Keep`).  Established after I'd shipped ~15 `[QA-E DIAG]` sites
with no running record; catalog filled retroactively in the QA-E
running notes.

**Inline back-refs:**
- §5 QA-E entry: scope expanded -- Task 9 (Dirty-flag investigation)
  added + Task 7 routing-dropdown fold-in.  Audio-routing fix family
  (Fixes 1-4) lands in Task 3.
- §9 fifteenth Forks entry: F-B-b "no K-6 parallel fix needed for
  Clips" stays valid as a K-6-shape decision, but does NOT cover the
  deserialize-side block-loop guard fix that landed here -- different
  bug shape, different code path.  Cross-ref this entry for the
  actual Clips-strip restore fix.
- §9 eighth Forks entry (QA-Md outcome): original "MT no-op under
  Debug" premise was diagnosed wrong at QA-Md close.  This 16th
  Forks entry adds a SECOND MT-related finding -- the FilePlay
  pre-scan was orphaned by the MT branch insertion.  Together,
  these suggest QA-Audit should sweep `processBlock` for other
  state-setup code that needs to live BEFORE the MT/serial branch
  fork.
- §9 Forks: this entry (sixteenth).

**Plan files affected:**
- `Plans & Specs/Main Plan.md` -- this entry + §5 QA-E scope update.
- `Plans & Specs/Batch Plans/phantom-recording-mongoose.md` -- Task 7
  step 2 routing-dropdown sub-bullet, insert Task 9 (Dirty-flag
  investigation), renumber close to Task 10, update Files-to-modify
  summary.
- Source files: `Source/PluginProcessor.cpp` (pre-scan moved) +
  `Source/Standalone/StandaloneEditor.cpp` (Clips-strip guard).
- `Plans & Specs/Running Notes/phantom-recording-mongoose.md` --
  Task 3 verify pass entry + new "Diagnostic Instrumentation
  Catalog" section.

**Verification:** Fixes 1-4 verified working 2026-05-12 (MT-on +
reload + play = audio through Vox + 2 Inst strips correctly; no
phantom Clips strips).  Task 9 (Dirty-flag investigation)
verification pending its own execution.  Task 7 fold-in
verification deferred to Task 7 execution.

### 2026-05-12 — QA-E Task 4 scope expansion: library-driven page-owner model (multi-file per page)

**Trigger:** at QA-E Task 4 plan review (2026-05-12), user surfaced a
bug masked in the original plan: the planned `mClipPath` + new
`mDryClipPath` shape on `VoxPage` / `InstPage` is a SINGLE pair of
paths per page.  Recording two takes onto the same Vox page would
overwrite the first take's bound paths -- only the most recent take
would appear in the browser, even though all takes still exist in
the audio library + on the Builder grid.  User stated intent: "at
every point I asked you to set it up so multiple files could be
recorded to one player page and all played through the same page."
Same gap blocks Task 7's "route multiple files to one source"
because the single-path-per-page shape destroys any prior assignment
on every re-route.

**Diagnosis:** the page itself is the wrong place to hold the
file-association.  The `mClipPath` / `mDryClipPath` model is N=1.
Multi-file requires N: per-page.  Two storage options:

- **Option A (per-page list).**  `VoxPage` / `InstPage` /
  `ClipsPage` each hold a `std::vector<juce::String>` (or pair list
  for Vox wet+dry).  Browser walks the page's vector.  Requires
  duplicating list mechanics across three classes + serialization +
  drag handler reroute on three surfaces.
- **Option B (library-driven).**  `AudioLibraryEntry` (in
  [Source/PatternManager.h:528](Source/PatternManager.h:528)) gains a new
  field `int pageOwnerChannelId { 0 };`.  Default 0 = generic /
  unowned (existing behavior preserved).  Recording / drag-to-page
  / Properties dialog route assignment all tag the library entry's
  ownerChannelId.  Browser walk iterates `mAudioLibrary` once,
  groups by ownerChannelId.  Single source of truth in
  PatternManager.

**Decision (user spec call 2026-05-12):** Option B (library-driven).
User additionally specified:
- **All three page types** (Vox / Inst / Clips) get the symmetric
  library treatment.
- **`VoxPage::mClipPath` + `InstPage::mClipPath`** are deleted
  entirely (engine doesn't read them; label-only display
  pre-deletion).  Drag-onto-page handlers reroute to tag the new
  library `pageOwnerChannelId` field.
- **`ClipsPage::mClipPath` is retained transitionally** because the
  Clips sample-player engine reads it via
  [Source/Clips/ClipsPage.cpp:510-511](Source/Clips/ClipsPage.cpp:510)
  for piano-roll-triggered playback.  Until QA-J's Clips routing
  unification refactors the Clips engine away from preload-on-load
  to streaming-from-FilePlay, the field stays as "currently loaded
  sample for the engine."  Library still tracks ALL N files routed
  to that Clips page; `mClipPath` is the one currently preloaded.
  Post-QA-J the field becomes deletable.
- **No migration heuristic for pre-fix saved projects.**  User
  stated they'll make new projects (test/recording session
  artifacts from this batch will be discarded).  Default 0 on
  deserialize means legacy library entries land in the generic
  Audio category, not Vox / Inst / Clips.  Acceptable.

**Scope this expands QA-E Task 4 to:**
- `Source/PatternManager.h`: add `pageOwnerChannelId` field to
  `AudioLibraryEntry`; extend `addAudioToLibrary` signature with
  optional `int pageOwnerChannelId = 0`; add
  `getAudioLibraryPageOwner(int idx)` + `setAudioLibraryPageOwner(int
  idx, int channelId)` accessors.
- `Source/PatternManager.cpp`: serialize / deserialize the new field
  (default 0 on read for back-compat); implement the new
  accessors.
- `Source/Vox/VoxPage.h` + `.cpp`: delete `mClipPath` +
  `getClipFilePath` + `setClipFilePath` + `mClipFileLabel` (the
  label was the only consumer of the path).
- `Source/Inst/InstPage.h` + `.cpp`: same deletion.
- `Source/Clips/ClipsPage.{h,cpp}`: `mClipPath` retained; drag
  handler at line 382 ALSO tags the dropped file's library entry
  with `kAudioBase + pageIdx` (the existing pageIdx == audioRow
  mapping).  `setClipFilePath` retained for engine preload.
- `Source/Standalone/StandaloneEditor.cpp`:
  - `commitRecordingResult`: Vox WET + DRY library entries get
    `ownerChannelId = voxInsert(voxIdx)`; Inst DRY library entry
    gets `ownerChannelId = instInsert(instIdx)`.
  - Browser walk (~line 2150-2310): rewrite Vox / Inst / Clips
    branches to iterate `mAudioLibrary` filtered by ownerChannelId
    range.  Audio "generic" branch shows ownerChannelId == 0
    entries.
  - Drop-onto-page spawn cascade at line 7099 (Clips drop): tag the
    library entry with `kAudioBase + audioRow` ownerChannelId.

**Inline back-refs:**
- §5 QA-E entry: scope expanded -- Task 4 grows from narrow
  "browser visibility" fix to library-driven multi-file model that
  also enables Task 7's multi-route intent + future multi-take
  recording.
- §9 thirteenth Forks entry (QA-J Clips routing unification + DSP-12
  test premise correction): QA-J's scope absorbs the post-Task-4
  ClipsPage engine refactor (sample-player → streaming-from-FilePlay).
  Once that lands, `ClipsPage::mClipPath` becomes deletable.
- §9 sixteenth Forks entry (Task 3 audio-routing fix family): the
  `routeChannel` field added by R-1 persistence is the OTHER half of
  the library-driven model -- block-level routing + library-level
  ownership are paired bindings.  Task 4 (this entry) completes the
  ownership side; routing was already done in Task 3.
- §9 Forks: this entry (seventeenth).

**Plan files affected:**
- `Plans & Specs/Main Plan.md` -- this entry + §5 QA-E scope update.
- `Plans & Specs/Batch Plans/phantom-recording-mongoose.md` -- Task 4
  section rewritten to library-driven model; Files-to-modify
  summary updated.
- `Plans & Specs/Running Notes/phantom-recording-mongoose.md` -- new
  "Task 4 scope expansion" entry.
- Source files (in the subsequent commit): `Source/PatternManager.h`,
  `Source/PatternManager.cpp`, `Source/Vox/VoxPage.h`,
  `Source/Vox/VoxPage.cpp`, `Source/Inst/InstPage.h`,
  `Source/Inst/InstPage.cpp`, `Source/Clips/ClipsPage.cpp`,
  `Source/Standalone/StandaloneEditor.cpp`.

**Verification:** the implementation commit's verify steps (in the
rewritten plan-file Task 4 section) cover: (1) single-take Vox
record produces 2 browser entries; (2) single-take Inst record
produces 1 entry; (3) multi-take Vox record produces ALL takes'
entries (this is the regression-prevention check); (4) Properties
dialog route reassignment moves a library entry between Vox / Inst
/ Clips categories; (5) drag-onto-Clips-tab tags a library entry
correctly + Clips engine preloads it; (6) save + reload preserves
all entries' ownerChannelId.

### 2026-05-14 — BaySickAlign + BaySickPitch redesign scope; QA-Fb + QA-Fc batches added; QA-J overlap-interaction fork

**Trigger:** mid-QA-E (after Task 4 source commit `1d928fc`).  QA-E Task 4 verify surfaced a multi-take recording capture-bleed bug + opened a longer design discussion about why the BaySickAlign and BaySickPitch editors looked functional but produced no audio.  Source inspection during the detour confirmed both editors are **paint-only shells** at the source level: `BaySickAlignDSP` exists but is never instantiated anywhere; `applyWarp` is a passthrough `memcpy`; `BaySickPitchDSP` does not exist as a class at all; both editors hold a `BaySickVocalProcessor&` reference but never read from it; all UI controls are pure local component state with no APVTS attachments or DSP wiring.  Source comments inside the editors literally read `"VocAlign-clone visual + interaction model"` and `"Newtone-clone visual + interaction model"` — visual cloning was done before the underlying engines were built.

**Decision package — four batch decisions, all locked 2026-05-14:**

1. **QA-F scope expansion — BaySickAlign full visual + DSP build-out folded in.**  Prior QA-F was "Vox DSP Disconnect (Cluster 1, regression fixes only)" — focused on DSP-02 / DSP-03 / DSP-05 + the QA-Inventory-folded DSP-03 sub-scope.  Expanded to include the full BaySickAlign engine build-out per the locked redesign at running notes `Plans & Specs/Running Notes/phantom-recording-mongoose.md` §13a-§13g.  Visual identity rebuild to escape VocAlign trade-dress (3-lane Leader / Follower / Output layout, Bass-green / Vox-teal / Drums-red lane colors, single always-visible right panel with Align + Pitch boxes, 6-preset combo + Save / Load + preset-dirty dot, Mode dropdown driving Fine Tune + Range knobs, Algos dropdown PSOLA / Granular / Phase Vocoder).  DSP build-out: instantiate `BaySickAlignDSP` on processor, channel-composite renderer, real `applyWarp` PhaseVocoder integration, YIN pitch detection, three pitch shifters, sync points data model, protected areas data model, render-to-bake, render history, ~20 APVTS params.  Trade-dress framing: the concern is the cumulative bundle of identical layouts + identical control names + identical engine-name shapes — NOT literal trademark on terms or universal DAW idioms; the redesign breaks the visual + control-naming legs hard while keeping engine names + universal-DAW idioms.

2. **QA-Fa scope expansion — BaySickPitch full visual + DSP build-out folded in.**  Prior QA-Fa was "BaySickPitch Audio Import (additive feature, split from QA-F)" — DSP-04 only (drag-and-drop file listener).  Expanded to include the full BaySickPitch engine build-out per the locked redesign at running notes §14a-§14g.  Composite-mode setup (corrected mid-detour — Pitch CAN use the same channel-composite as Align; sibling architecture, not unrelated tools).  Visual identity rebuild to escape Newtone trade-dress (LENGTH info bar `X bars / M:SS.f`, 2-mode Slice / Edit with Vibrato + Formant + Volume as per-note sub-curves, Pills note region in Effects-purple with Vox-teal waveform interior + Bass-green pitch curve, CENTER / VARIATION / TRANS renamed Focus / Mod / Speed, Send Notes to popup, 3 mode presets driving global knobs).  DSP build-out: YIN, note segmentation, three pitch shifters (shared with QA-F), vibrato + formant + volume per-note, per-channel ValueTree storage, render-to-bake, realtime applicator (Mode C), render history, ~10-15 APVTS params.

3. **QA-Fb inserted — Recording Lifecycle + Channel-Composite Renderer.**  New dedicated batch slotted between QA-Fa and QA-G.  Scope: dual-buffer recording (live + FilePlay simultaneously, separate taps), conditional WET tap (skip when realtime pitch bypassed), multi-take capture-bleed fix (the trigger that opened the detour), audio-clip-resize-doesn't-stretch fix (block-state-not-propagating family), dirty-flag investigation (both page-creation + record-finalize triggers per Task 9 fold-out from QA-E), and the channel-composite renderer (shared dependency for QA-F + QA-Fa, lives here as the foundational layer).  Per running notes §17.  Slot pick was the owner's per `feedback_slot_placement_is_spec_call.md` — Fb between QA-Fa (DSP foundations) and QA-G (Builder UX assumes recordings land cleanly).

4. **QA-Fc inserted — BaySickNAMIR Dual-Mic Stack.**  New dedicated batch slotted between QA-Fb and QA-G.  Scope: two parallel mic-sim paths (sum, not blend) on the BaySickNAMIR engine — real-recording workflow simulating two mics on the same source.  Per running notes §23.  Parallel-paths-not-blend architecture is the central spec call (post-cab buffer copied to scratch; Mic A processes in place; Mic B processes scratch through its own Mic Sim + Mic Placement chain; Mic B output sums back into main).  `nam_micb_active` toggle bypasses entire Mic B path when off (byte-identical to today's single-mic chain).  8 new APVTS params with `_b_` infix.  9 new SlotSnapshot fields.  Editor splits Mic Sim + Mic Placement rows into Mic A | Mic B columns side-by-side.  Picks up on both Vox + Inst pages automatically (BaySickNAMIR is shared engine hosted as a sub-tab on each).

**Audit outcome — QA-Fd queued + dropped.**  Mid-detour the owner asked for a sanity-check pass on BaySickNAMIR and BaySickPedals to confirm every UI element is wired to a real APVTS param + actually does something.  Conditional QA-Fd batch was queued ("BaySickNAMIR + BaySickPedals wiring fixes — if either audit surfaces gaps").  Both audits came back **clean** (running notes §20 + §21): BaySickNAMIR has all 18 APVTS params declared / read in processBlock / wired (12 via JUCE attachment helpers, 6 manually-synced for custom selector widgets); BaySickPedals has the correct minimal 8-param surface (per-slot bypass bools only; per-pedal params owned by each `DSPBase` instance, mirroring the `EffectRack` pattern); zero dead UI elements in either engine.  **QA-Fd was dropped** — no follow-on wiring batch needed.  QA-Fc proceeds on the audited-clean foundation.

**QA-J overlap-interaction fork — Option 1 picked.**  The new QA-F / QA-Fa / QA-Fb test scenarios involve overlapping audio clips on the same Vox row (multi-take recording is the trigger workflow for the whole detour).  Those scenarios hit the multi-clip stacking bug (DSP-06) being fixed in QA-J — currently the rack + EQ chain runs once per clip on overlapping clips, with comp envelope / reverb tail / LFO phase bleeding from clip A into clip B's processing pass.  Three options surfaced (running notes §16): Option 1 = design QA-F / QA-Fa / QA-Fb tests with sequential clips on the same row only, defer overlapping-same-row scenarios to QA-J re-verify; Option 2 = block QA-F / QA-Fa / QA-Fb on QA-J landing first; Option 3 = land knowing the overlap bug exists, re-verify post-QA-J.  **Owner picked Option 1** — verbatim: "i'll line clips back to back to test things".  Fork-note structure agreed: QA-F + QA-Fa + QA-Fb each get inline "QA-J re-verify required" notes (applied above this entry); QA-J gets a fork-in note for the re-verify scenarios (applied to QA-J entry); this Forks entry documents the fork itself.

**APVTS attachment terminology clarification (mid-audit).**  Owner asked about whether the BaySickNAMIR Slot A / B buttons were automatable since I'd categorized them under "Non-APVTS UI elements."  That categorization was misleading.  APVTS-backed parameters are automatable regardless of whether the UI uses a standard JUCE attachment helper (`SliderAttachment` / `ButtonAttachment` / `ComboBoxAttachment`).  Manually-synced controls (chicken heads, radio-style TextButton pairs driving Choice params) call `setValueNotifyingHost` — same semantics, fully automatable.  The accurate distinction is "control uses a standard helper" vs. "control manually syncs to APVTS" vs. "action UI (truly non-parameter — file pickers, drag-drop)."  All 18 BaySickNAMIR params (including `ab_slot`) are real APVTS params and fully automatable.

**Color palette correction (mid-detour).**  Owner clarified mid-conversation that Layers active = orange (NOT green) and Bass active = green.  This corrected an earlier Align Leader-lane assignment in the conversation from "Layers active green" to the actual color (Bass active green).  Applied to the consolidated QA-F entry above + QA-Fa pitch curve overlay color.

**Plan files affected:**

- §5 QA-F entry — scope-expanded to include full BaySickAlign build-out + 2026-05-14 fold bullet + QA-J fork-out note + Risk bumped medium → medium-high + Effort bumped to ~14-22 hours.
- §5 QA-Fa entry — scope-expanded to include full BaySickPitch build-out + QA-J fork-out note + Risk + Effort bumped.
- §5 QA-Fb entry — INSERTED (new batch).
- §5 QA-Fc entry — INSERTED (new batch).
- §5 QA-J entry — fork-in note added for QA-F / QA-Fa / QA-Fb overlapping-same-row re-verify scenarios + Effort delta noted.
- §6 sequencing arrow — `→ QA-Fb → QA-Fc` inserted between `QA-Fa` and `QA-G`; new footnote `********` added for the two inserted batches.
- §9 this entry.

**Verification:** at QA-F / QA-Fa / QA-Fb close, the batch-close drafter confirms the no-stubs gate held (every UI line item ships functional code + UI together — no more paint-only shells), and the QA-J fork-out note flowed through to QA-J's re-verify list correctly.  At QA-Fc close, the batch-close drafter confirms the `nam_micb_active=false` regression check (byte-identical to today's single-mic chain) + the correlated-sum amplification check (Mic A + Mic B identical settings produces 2× Mic A amplitude).  At QA-J close, the four re-verify scenarios listed in QA-J's folded bullet are exercised against the unified stacking-fix surface.

### 2026-05-15 — QA-E Task 6 (DSP-09) pre-task spec-call → DSP-09 punted to new batch QA-Ea

**Trigger:** QA-E Task 6 = "DSP-09 (Bus solo)".  The mandatory pre-task spec-call (read Carry-Forward §3 "Bus solo" + §4 Decisions Already Made, read `VibeGraph.cpp`/`PluginProcessor.cpp` bus-solo dispatch, surface still-open sub-calls) surfaced a topology finding that materially changed the implementation approach.  Full diagnosis is in running-notes `Plans & Specs/Running Notes/phantom-recording-mongoose.md` §34-§40.  No source changed in Task 6 — pure spec / diagnosis work; QA-E Task 5 close commit `6b044aa` stands.

**Diagnosis:** the reported "Drums plays when Layers solos" bug is structural — bus-solo logic is scattered across THREE inconsistent sites: (1) `BusNode::processBlock` triad (`VibeGraph.cpp:355-363`/:522/:682) — correctly zeroes the Drums BusNode but isn't the leak; (2) `processBus` `useGroupSolo` + ClipsBus 6-bus + Rusty-standalone variants (`VibeGraph.cpp:1698-1702`/:1727-1728); (3) receive-group `busAnySolo` (`PluginProcessor.cpp:2572-2577`) which deliberately EXCLUDES layers/bass/drums bus solo.  Plus the master-sum stage (`VibeGraph.cpp:1547-1570`) sums `layersBuf+bassBuf+drumsBuf+audioClipsPreRendered+masterExtra` with zero solo gating; the `kMaster` accumulator is fully ungated.  Drum audio fanning into the ungated `masterExtra` or the excluded Site-3 formula bypasses the gate.

**Topology finding (the key discovery):** bus *inputs* are uniform (every bus reads a per-channel accumulator fed by `routeInsertOutput`) but bus *outputs* are NOT — Layers/Bass/Drums bus outputs are hardcoded into dedicated `layersBuf`/`bassBuf`/`drumsBuf` and explicitly summed (`VibeGraph.cpp:1551-1553`), while Vox/Inst/Vox2/Inst2/Inst3/FX/Rusty bus outputs go through the generic `routeInsertOutput`→kMaster path (`PluginProcessor.cpp:2593`).  This is a Phase-1 (original AudioProcessorGraph shim) vs 5F-4b (April 2026 unified routing-accumulator model) **legacy split** — the expansion never retrofitted the original three.  This asymmetry IS the structural root of the scattered solo logic; a clean single-gate fix is only possible after the output paths are unified.

**Sub-calls locked (Jeff's decisions):** **A** solo+mute same bus → mute wins.  **B1** direct-to-Master bypass routes are NOT silenced by bus solo (Jeff caught that gating `masterExtra` wholesale would zero the soloed bus's own output — it routes through that same accumulator; B2 "gate at routeInsertOutput source" rejected as re-spreading solo logic into the router).  **C** multi-bus solo is additive.  **D** (corrected from an earlier wrong "within their group" claim) per-strip `_solo` is ALREADY global via `isAnyInsertSoloed()` scanning all 8 insert maps — NOT touched by DSP-09; **critical guardrail:** the bus-solo gate must NEVER read `isAnyInsertSoloed()` (prior serial bug muted whole buses; warned at `VibeGraph.cpp:1876-1885`) — read bus `_solo` params only.  **E** persistence automatic via APVTS (factual).

**Options analyzed:** Option 1 = shared `anyBusSoloed()` helper at both gate sites (fixes the user-visible bug, keeps the two output paths; medium risk).  Option 2 = Option 1 PLUS unify the Layers/Bass/Drums output path through `routeInsertOutput`→kMaster + delete the bespoke master sum (removes the bug CLASS; high risk — touches master-sum, MT MasterTask, BusNode buffer model).

**Decision (Jeff):** Option 2 is what we actually want, but it gets its OWN batch **QA-Ea** with its own plan + its own mandatory `/review-batch` for the hot-path safety concerns.  BOTH the DSP-09 bus-solo fix AND the Option-2 Layers/Bass/Drums output-path unification are QA-Ea's scope.  DSP-09 is struck from QA-E entirely; QA-E Task 6 is vacated and QA-E resumes at Task 7 (FILE-02).  Rationale: same code area + same test material as QA-E, run in a slightly different order — QA-Ea slotted **immediately after QA-E, before QA-F** (Jeff's confirmed slot per `feedback_slot_placement_is_spec_call.md`).  This is a scope-reduction + new-batch creation on the OPEN QA-E batch (NOT a closed-batch carry-forward), documented via this §9 entry per §0 Rule 3 — no prior commit reopened.

**Plan files affected:**
- §5 QA-E entry — title struck to remove DSP-09; DSP-09 line item annotated "MOVED to QA-Ea"; trade-off note updated (QA-E2 split actualized as QA-Ea).
- §5 QA-Ea entry — INSERTED (new batch, before QA-F).
- §6 sequencing arrow — `→ QA-Ea*********` inserted between `QA-E` and `QA-F`; new footnote `*********` added.
- §9 this entry (nineteenth Forks entry).
- `Plans & Specs/Batch Plans/phantom-recording-mongoose.md` — Task 6 section annotated (DSP-09 moved to QA-Ea; Task 7 FILE-02 is next executable).
- `Plans & Specs/Running Notes/phantom-recording-mongoose.md` — §34-§40 capture (the full diagnosis + decision).

**Verification:** QA-Ea will run the 5 DSP-09 scenarios from the old QA-E Task 6 list (solo Layers; unsolo; multi-bus solo additive; solo+mute = mute wins; persistence across save/load) + a regression bit-compare that the L/B/D output-path rewrite didn't change non-solo mix output, and its mandatory `/review-batch` must verify the new helper never reads `isAnyInsertSoloed()`.

### 2026-05-17 — QA-Eb inserted: standalone app-window resizability (new independent batch, NOT a NAV-01 carve-out)

**Trigger:** QA-E Task 7 (FILE-02) verify session (Task 7 PASSED Debug + Release; see running-notes §50-§52).  During verify Jeff requested a new feature: make the standalone app window user-resizable.  Jeff's stated rationale (verbatim sense): doing it adjacent to QA-E "vastly speeds up testing for me not having to go back and forth over and over again" — a resizable window removes the window-juggling overhead from the remaining QA-E-adjacent verify passes.  No source changed by this routing decision — QA-E Task 7 close work stands; this is a pure new-batch creation + sequencing entry.

**Decision (Jeff, 2026-05-17):** standalone app-window resizability becomes its **OWN new independent batch QA-Eb**.  This is a **fresh feature request from Jeff**, NOT a carve-out of any existing backlog item.  In particular it is **NOT** the QA-L `NAV-01` item.  Jeff clarified (2026-05-17, backlog authority) that he never previously requested app-window resizability and that `NAV-01` is a SEPARATE Builder-grid bug: the Builder grid doesn't line up with the tracks when the grid is resized.  QA-Eb does **NOT** carve out, rescope, or take over `NAV-01` — `NAV-01` stays in QA-L, owned by QA-L.  Per Jeff's instruction the QA-L §5 `NAV-01` line was corrected the same day to that accurate description (the prior "window resize layout — strict FlexBox/Grid + min size" wording was vague/misleading) — a wording-accuracy fix only, NOT a scope or ownership change.  QA-Eb is slotted **immediately after QA-Ea and before QA-F** (`... QA-E → QA-Ea → QA-Eb → QA-F ...`), mirroring the QA-Ea adjacency precedent.  QA-Eb and QA-E do not group by code area; the slot is justified purely by testing efficiency for Jeff's remaining QA-E-adjacent verify work.  Slot / placement is **Jeff's confirmed call** per `feedback_slot_placement_is_spec_call.md` (sequencing position is a spec call, not a unilateral pick).

**§53 framing reconciliation:** an earlier in-conversation framing had described QA-Eb as living in the "NAV-01 area" / as a NAV-01 carve-out.  That was an unsubstantiated assumption, **corrected on 2026-05-17 at Jeff's instruction** — QA-Eb is a fresh independent request.  The committed running-notes §53 reflects the corrected framing ("independent of the QA-L NAV-01 item; NAV-01 is a separate concern; QA-L is untouched"); this §9 entry is the canonical record reconciling that and supersedes any earlier "NAV-01 carve-out" reading.

**Scope (Jeff-locked 2026-05-17, running-notes §53):** ONLY:
- Resizable standalone window + working maximize.
- A min-size clamp so the window cannot shrink below the usable design size.
- A `juce::Viewport` with scrollbars when the window is smaller than the content's design size.
- Pages that already own scrollbars (Piano Roll, Builder grid) must NOT be double-wrapped and must NOT fight the outer Viewport (opt out / special-case those).
- **Explicitly OUT of scope (post-V1):** any per-page proportional / FlexBox / Grid relayout.  QA-Eb is window-chrome + outer-Viewport only — it does not re-flow page internals.

**Sequencing:** Phase 3, `QA-E → QA-Ea → QA-Eb → QA-F`.  Inserted between QA-Ea and QA-F per the QA-Ea adjacency precedent (Jeff's confirmed slot).

**Plan files affected:**
- §5 QA-Eb entry — INSERTED (new independent batch, after QA-Ea, before QA-F).
- §6 sequencing arrow — `→ QA-Eb**********` inserted between `QA-Ea*********` and `QA-F`; new footnote added.
- §9 this entry (twentieth Forks entry).
- `Plans & Specs/Running Notes/phantom-recording-mongoose.md` — §53 (the committed decision capture; framing corrected to "independent of NAV-01").
- **NOT touched:** §5 QA-L entry / `NAV-01` (separate concern, untouched per Jeff).

**Verification:** n/a — this is a routing / sequencing entry, no source change.  QA-Eb's own §5 entry carries its verify list (resize behavior, maximize, min-size clamp, outer-Viewport scrollbars at sub-design-size, no double-wrap fight on Piano Roll / Builder grid).

### 2026-05-17 — QA-E Task 1 (M1): mute findings #16a / #16b / #21 closed no-longer-reproducible (provenance trace, no source change)

**Trigger:** QA-E Task 1 (M1) = "Mute Findings Verify-and-Close" for the three QA-0-captured mute findings #16a (pattern-row mute), #16b (audio-row mute), and #21 (right-click block-level mute behavior).  Scope was verify-and-close, not implement.

**Diagnosis:** source review at QA-E open found every mute-dispatch gate ALREADY present and correct, no QA-E source change required: pattern-dispatch gate `Source/PluginProcessor.cpp:1194-1195`; audio-render gate `Source/PluginProcessor.cpp:493-501`; block-level gate `Source/Standalone/StandaloneEditor.cpp:2406`; LED handler `Source/Standalone/BuilderPage.cpp:4092`.  `git log -L` traced all four gates to commit `cc011e0` (MT-engine batch series) plus the initial commit `d595ee3` — both PRE-DATE QA-0's #16a / #16b / #21 capture by months.  Jeff verified all four mute scenarios in BOTH Debug and Release: 8/8 PASS (running-notes Task 1).  Verify-only commit `57f8edd` carries no source change.

**Options considered:** (a) treat as still-open and write a fix anyway; (b) close as no-longer-reproducible with a provenance trace.

**Decision (Jeff):** findings #16a / #16b / #21 are **no longer reproducible** in current source.  Either the original QA-0 captures were inaccurate, or an unrelated commit since the QA-0 snapshot incidentally fixed the behavior; root cause of the captures is unidentified but moot given the clean 8/8 verify plus the pre-dating provenance.  No source commit; closed via this Forks entry at QA-E close per §0 Rule 3.  Verify-only commit `57f8edd` stands.

**Carry-forward contradictions:** the Carry-Forward Reference / QA-0 backlog listed #16a / #16b / #21 as open mute bugs.  This entry records they are NOT reproducible in current source.  Per the §0 three-doc discipline the Carry-Forward snapshot stays frozen as the 2026-05-07 record; this Forks entry IS the canonical contradiction record.  No Carry-Forward §1-§3 architectural facts change.

**Inline back-refs:**
- §5 QA-E entry — Task 1 (M1) annotated: mute findings #16a / #16b / #21 verified no-longer-reproducible, closed via §9 twenty-first Forks entry.
- `Plans & Specs/Implemented Work Log.md` QA-E close entry — Task 1 under Done (8/8 mute PASS, no source change); disposition cross-refs this entry.

**Plan files affected:**
- `Plans & Specs/Main Plan.md` — this entry + §5 QA-E Task 1 annotation.
- `Plans & Specs/Running Notes/phantom-recording-mongoose.md` — Task 1 section (8/8 verify + provenance).
- **NOT touched:** `Plans & Specs/Carry-Forward Reference.md` (frozen — contradiction recorded here).

**Verification:** Jeff verified all four mute scenarios (pattern-row, audio-row, right-click block, combined) in BOTH Debug and Release at QA-E Task 1 — 8/8 PASS (running-notes Task 1).  No regression check needed: no source changed (verify-only commit `57f8edd`).

### 2026-05-17 — QA-E §60: dead flat-list BrowserItem::Kind::Audio choke/rename/switch paths routed to QA-Cleanup-1

**Trigger:** QA-E Task 7, during Choke-Group verification (running-notes §45).  While confirming audio-clip Choke Group behavior, the dead flat-list `BrowserItem::Kind::Audio` browser paths in `BrowserPanel::showItemContextMenu` ([Source/Standalone/BuilderPage.cpp](Source/Standalone/BuilderPage.cpp), ~line 928; the dead `Audio` case ~line 912) were found unreachable.

**Diagnosis:** post-FILE-01 (QA-E Task 4 library-driven browser model) no `Audio`-kind `BrowserItem` is ever constructed anywhere.  The orphaned Choke Group submenu plus the related switch / rename cases in `BrowserPanel::showItemContextMenu` are unreachable duplicates of the LIVE audio choke path in `BrowserPanel::showAudioTreeContextMenu` ([BuilderPage.cpp](Source/Standalone/BuilderPage.cpp):438).  Audio-clip choke grouping is NOT lost: the folder / tree menu is the live path and the choke value lives on the `AudioLibraryEntry`.  Pure dead code; deleting it has no behavior change.

**Options considered:** (a) delete in-batch in QA-E (functional-bug-found → fix-in-batch default); (b) route to the established Phase-6 dead-code cleanup batch QA-Cleanup-1, since this is pure dead code (no functional bug — the live path works) and §0 Rule 3 reserves Phase 6 for dead/dormant code cleanup.

**Decision (Jeff — S4 spec call locked 2026-05-11):** route the §60 dead-code item to **QA-Cleanup-1**.  Dead code, not a functional bug — the live `showAudioTreeContextMenu` path is correct and unaffected — so it falls under §0 Rule 3's Phase-6 carve-out, alongside the QA-D NIT-4 dead-writeback already folded there (§9 eleventh Forks entry).  Formal routing deferred to QA-E close (Task 10) per running-notes §45 / §49 / §54.

**Cleanup-scope caveat (source-verified at QA-E close — carry into QA-Cleanup-1 execution):** `renameAudioAt` ([BuilderPage.cpp](Source/Standalone/BuilderPage.cpp):1117) IS shared — it is called from the LIVE tree path ([BuilderPage.cpp](Source/Standalone/BuilderPage.cpp):411) as well as the dead flat-list `BrowserItem::Kind::Audio` case (~`BuilderPage.cpp`:912).  QA-Cleanup-1 MUST retain `renameAudioAt` and delete ONLY the dead flat-list call site (the `BrowserItem::Kind::Audio` case in `showItemContextMenu`) — not the shared `renameAudioAt` itself.

**Carry-forward contradictions:** none.  Post-FILE-01 dead code created by QA-E Task 4's own library-driven rewrite; does not contradict any Carry-Forward §1-§3 architectural fact.  Live audio-choke behavior unchanged.

**Inline back-refs:**
- §5 QA-E entry — Task 7 annotated: dead flat-list `BrowserItem::Kind::Audio` choke/rename/switch paths found during Choke-Group verify, routed to QA-Cleanup-1 via §9 twenty-second Forks entry.
- §5 QA-Cleanup-1 entry — `- Items:` gains a folded sub-bullet for the §60 dead paths incl. the verified `renameAudioAt` shared-use note, cross-referencing this entry.
- `Plans & Specs/Implemented Work Log.md` QA-E close entry — "Found along the way" #60 records finding + routing.

**Plan files affected:**
- `Plans & Specs/Main Plan.md` — this entry + §5 QA-E Task 7 annotation + §5 QA-Cleanup-1 scope addition.
- `Plans & Specs/Running Notes/phantom-recording-mongoose.md` — §45 / §49 / §54 / §60.
- `Plans & Specs/Implemented Work Log.md` — QA-E close "Found along the way" #60.

**Verification:** n/a — routing decision, no QA-E source change.  QA-Cleanup-1's own verify ladder covers the eventual deletion; the source-verified `renameAudioAt` shared-use note is the explicit pre-delete guard.
