# Running Notes — QA-ClipDrop (fancy-kindling-dongarra)

> Append-only running log for the QA-ClipDrop batch.  A new dated `## YYYY-MM-DD
> — Task N — <name>` entry is appended at every checkpoint (commit landed /
> sub-task verified / finding captured / spec call resolved / scope pivot) per
> `feedback_draft_doc_running_notes_every_checkpoint.md`.  At batch close,
> `/draft-doc batch-close` consumes this file as the primary input for the single
> Implemented Work Log entry.  Never edit prior entries — new findings get a new
> dated entry.

**Pair file:** `Plans & Specs/Batch Plans/fancy-kindling-dongarra.md` (the plan).
**Conventions:** Main Plan §0 Document Formatting Conventions + the Batch
Plans / Running Notes required-sections rule (locked 2026-05-11; exemplar
`federated-bouncing-cupcake.md`).

---

## 2026-06-02 — Task 0 — Batch open

- QA-ClipDrop opened as a **long-running diagnostic batch** (Jeff's call,
  2026-06-02).  The audio-clip drop bug is **intermittent / session-state-dependent**
  — it cleared on a fresh app restart and would not reproduce on demand, so per
  `feedback_diagnose_before_fixing.md` we **instrument-and-catch** rather than ship
  a blind fix.
- Diagnosis (collaborative, 2026-06-02): drag-from-Explorer symptoms did NOT
  reproduce (3 targeted scenarios + the original pattern-and-clip test rig all
  passed); the reliable repro was the Clips ribbon `+ Add New Clip` picker silently
  no-op'ing (no page / strip / browser entry).  Source trace: `+ Add New Clip`
  (StandaloneEditor.cpp:3704) and drag-drop both converge on
  `ArrangementGrid::importAudioFile` (BuilderPage.cpp:3350); the missing browser
  entry proves the drop bails BEFORE `addAudioToLibrary` (:3415) — at the file-missing
  (:3365) or copy-on-drop-empty (:3402) early-return.  `importSample`
  (ProjectManager.cpp:484) only returns empty (project open) on `copyFileTo` failure
  → the project `Samples/` folder state goes bad mid-session; a restart rebuilds
  `mCurrentFolder`.
- Reframe: very likely ONE intermittent session-state failure (copy-on-drop poisoned
  by mid-session project lifecycle churn during the QA-Ed test-rig setup), not the 6
  separate deterministic symptoms the §5 entry first listed.
- Framing correction (SC-B): the §5 "pre-existing" wording was wrong — corrected this
  task to "regression that recurred / intermittent / session-state-dependent."
- Spec calls locked this open: SC-A slot (after QA-Ed, before QA-Ee) · SC-B framing ·
  SC-C instrument-and-catch · SC-D diag form = AlertWindow popup + append-to-log in
  `Documents/BaySickDAW/`, Debug + Release · SC-E coverage = full drop cascade ·
  SC-F lifecycle = held open until evidence or close-as-not-reproduced by end of QA.
- Deferred sub-spec calls: DS-1 (§5.5 Domain Coverage backfill placement — QA-ClipDrop
  + QA-Ed + QA-TempoMap missing) · DS-2 (probe strip/keep if not reproduced) · DS-3
  (fix scope at evidence-time) · DS-4 (end-of-QA trigger).
- Reuse found for Task 1: `namirLog()` / `pedalsLog()` one-off file-logger convention
  (append to `Documents/BaySickDAW/*.txt`, Debug + Release); `ProjectManager::getSettingsFile()`
  canonical Documents/BaySickDAW resolver.

## Diagnostic Instrumentation Catalog

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| _(populated at Task 1)_ | `[QA-ClipDrop DIAG]` | full-cascade clip-drop trap (popup + log) | Remove at batch close (or Keep per DS-2) |
