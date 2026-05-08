# Previously Implemented — Pre-QA Build History

> **Append-only.** This document is the historical record of work completed
> BEFORE the post-Batch-10 QA plan started (2026-05-07).  Each entry was
> verified against the actual build at the time of recording — function /
> file / behavior confirmed present in source.
>
> **Distinct from `Implemented Work Log.md`**, which tracks QA-era work only
> (everything from 2026-05-07 onward).  Splitting keeps the QA log focused
> on the current execution sequence; this doc preserves the broader build
> history without bloating either file.

**How entries are organized:** by build phase / batch / feature cluster.
Each entry has a verified status — what was implemented, where it lives in
source, and what end-user behavior it produces.

---

## How to read this doc

- **Verified entries** are confirmed present in the BaySickDAW source as of
  the date listed.  If you're checking whether something is implemented,
  this doc + actual code is the authoritative answer.
- **References to source**: each entry includes one or more `Source/`
  paths so future-you can grep for evidence.  When code moves or rewrites
  happen, the entry stays as the historical "what was true at the time."
- **Contradictions over time**: if a previously-implemented item later gets
  removed / refactored / superseded, the QA-era `Implemented Work Log.md`
  records the change.  This doc is NOT updated retroactively — it's the
  historical snapshot.

---

## Sources surveyed for this doc

Three pre-QA source documents were reviewed end-to-end and cross-referenced
against the build:

1. `Files For Claude/Final Stretch Work.txt`
2. `Files For Claude/vibedaw_blueprint.md`
3. `Plans & Specs/Batch Plans/lucky-discovering-tiger.md` (pre-QA-era plan
   — pending decision on whether to relocate here from `.claude/plans/`).

Items marked DONE in those files that verified against the build land in
this doc.  Items that didn't verify go into the active follow-up list at
QA close-out (alongside still-needed-not-yet-done items).

---

## Entries

(Populated during the post-QA-0 source-verification pass.  Format below.)

```
### YYYY-MM-DD-ish — <Phase / Batch / Feature> — <one-line summary>

**Implemented:** <what shipped>

**Source:** `Source/<path>:<line-or-class>` (one or more references).

**End-user behavior:** <what the user sees / hears>.

**Verified against build:** <date> — <method: grep / file existence / function presence>.

**Sourced from:** <Final Stretch Work.txt | vibedaw_blueprint.md | lucky-discovering-tiger.md> — <which section / item>.
```

---

(Entries appended below this line as the source-verification pass populates them.)
