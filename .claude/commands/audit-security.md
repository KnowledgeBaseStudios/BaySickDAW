---
description: Audit BaySickDAW's handling of untrusted input - project files, media files, hosted plugin binaries, the Core Library fetcher. Read-only. Pre-release or when a new input surface lands.
---

Dispatch the `security-auditor` agent.

The agent audits what happens when input is **hostile or malformed** - not
whether the code does what it means to do. That second question is correctness,
it was swept at QA-Soundness, and it is deliberately out of scope here.

Surfaces, in the order the agent works them: project / preset / template XML,
media loading (WAV / SFZ / IR / NAM), path handling and the `library:` /
`mysamples:` stable-ref resolvers, hosted VST3 binaries across the sandbox
bridge, the Core Library fetcher's network path, and secrets / logging.

**The test every finding must pass:** could this data have come from a file or a
server someone else controls? If the app wrote the value itself, missing
validation is not a finding no matter how unguarded it looks. Most of what looks
alarming in an audio codebase is the app reading its own state back.

**Tiering comes from Future State CL-289 and tiers by RELEASE PHASE, not by who
owns the code.** Tier 1 (V1 pre-release) is four things together: a vendored
CVE scan against NVD / GitHub Advisories, a file-parser audit across WAV / MP3 /
SFZ / project-XML / preset readers (ours AND vendored), DLL search-order safety,
and a save-file format audit for XXE / billion-laughs. Tier 2 is the updater
network audit and only becomes runnable when QA-Updater lands. Tier 3 is post-V1
cloud work.

Vendored code is a separate SECTION of the report, not a separate tier - a
sample pack reaches sfizz's parser, not ours. Default to Tier 1, all four parts,
unless I say otherwise, and if a run covers only part of a tier the report must
say so in Scope.

**No finding without an exploit path.** "Could theoretically overflow" is not a
finding. The agent must name the file, the field, what a crafted value does, and
what I would observe. Expect most real findings to be MEDIUM - a crash on a
malformed project is a bug, not a compromise.

Returns a report saved to
`Plans & Specs/Research Reports/security-audit-<YYYY-MM-DD>.md` (drafter
pattern - the agent returns text, the parent applies via Write).

Show me the report in plain English; I do not read code. Don't apply fixes -
walk me through what each finding means first, then I'll rule on what gets
fixed.

**Frequency:** pre-release, or whenever a new way of reading someone else's file
lands in the app.

**Distinct from:**
- `/perf-audit` - audio-thread allocations, locking, SIMD. Real-time safety is
  explicitly NOT this agent's job.
- `/audit-licenses` - license compatibility of vendored libs and assets.
- `/review-batch` - one batch's diff against the plan and the coding rules.
