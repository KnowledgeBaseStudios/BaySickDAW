# Running Notes — QA-M (faithful-rekitting-beaver)

> Append-only mid-batch log. New entry at every checkpoint (commit landed / sub-task
> verified / finding captured / spec call resolved / scope pivot). Consumed at doc-close
> (section pass under bulk-run R2) as the primary input for the held Work Log entry.

Pair file: `Plans & Specs/Batch Plans/faithful-rekitting-beaver.md`
Convention: Main Plan §0 "Batch Plans + Running Notes layout" (exemplar
`federated-bouncing-cupcake.md`).

## 2026-07-17 — Group open (G3)

Seeded at G3 group approval. Both LIFE items root-located at scout (kit loader tears down
ALL Drums-type tabs incl. Rusty and never re-spawns it; reload primitive exists but is
wired only to project restore — no session last-kit memory). Coding starts after QA-L.

## 2026-07-18 — Tasks 1 + 2 — LIFE-01 kit-load leaves Rusty alone + LIFE-02 re-add auto-reloads

LIFE-01: loadKitImpl teardown (StandaloneEditor.cpp ~:7149) rewritten — the Drums-type
close loop now FILTERS OUT the Rusty tab (dynamic_cast<BaySickRustyDrumsPage*> == nullptr
guard) and closes per-id via mRibbon->closeTab(id) (which fires onTabClosed itself, the
Rusty delete flow's documented behavior — RibbonTabBar.cpp:159 closeTab is callback-free
direct removal but the editor's onTabClosed does the engine teardown) instead of the
old type-wide clearTabsOfType(Drums) that wiped Rusty's ribbon tab too. Kit loads target
DrumPage kits; Rusty singleton keeps playing. Replace prompt (loadKit :7075):
DOCKET pick 2 (Jeff) — scan stays DrumPage-only (no prompt when nothing is replaced;
the plan's "count Rusty too" half was vestigial, written pre-teardown-fix — a kit load
can no longer harm Rusty so there's nothing to protect). Text corrected from "remove all
currently selected drums" (overbroad post-fix) to "replace all of your current drum tabs
with the new kit's drums" + a conditional "(BaySickRustyDrums is not affected by kit
loads.)" line appended only when a Rusty tab exists.

LIFE-02: new StandaloneEditor::mLastRustyKitFile (juce::File, session-only — project
restore carries its own kitPath record so this is NOT persisted). Captured in TWO spots
while the engine is alive: the onKitLoaded lambda (:7761, every kit load — eng->
getCurrentKitPath()) and the onTabClosed Rusty branch (before mPages.remove destroys the
engine). addBaySickRustyDrumsTab (:7838-area): after registerBaySickRustyDrumsPianoRoll,
if mLastRustyKitFile.existsAsFile() && ! isProjectLoadInProgress() -> rawPage->
reloadForProjectRestore(mLastRustyKitFile) (the existing restore primitive: loadKit +
program-combo sync + ARIA panel + onKitLoaded strip spawn + busy sign). Project-load
guard prevents a double-load (restore path drives its own reload right after the spawn,
:11332/:11357). Fresh session w/ no memory = the untouched pick-a-program overlay.

Both tasks one combined build gate (planned). No findings routed out. No diagnostics
added. Carry-Forward touch point honored: loadBaySickRustyDrumsKit / program-change reuse
paths untouched (reloadForProjectRestore is the only entry, matches restore).

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-18.

## 2026-07-18 — QA-M CODE-COMPLETE

Tasks 1-2 shipped, one combined build gate clean. One docket asked + resolved mid-batch
(replace-prompt scan scope — Jeff pick 2). §B.19 authored (4 scenarios; `blocks:` hash
backfills at the next docs commit per precedent). Work Log entry drafted + HELD below.
ONE batch commit surfaced for approval (carries the QA-L doc straggler: the B.18 hash
backfill).

## Held Work Log entry

> HELD per bulk-run R2 — apply to `Implemented Work Log.md` verbatim when §B.19 passes.

### 2026-07-18 23:00 PT — QA-M — Engine restoration lifecycle: kit-load leaves Rusty alone + Rusty re-add auto-reloads last kit

**Bucket:** Players, System Pages

#### Done

- **Task 1 — LIFE-01 (kit-load leaves Rusty alone):** the full-kit loader
  (`loadKitImpl`) tore down EVERY Drums-type ribbon tab — and the Rusty tab is
  Drums-typed — via a type-wide `clearTabsOfType(Drums)`, then re-created only DrumPage
  tabs, so a kit load silently destroyed Rusty and never re-spawned it. The teardown
  loop now filters the Rusty page out (`dynamic_cast<BaySickRustyDrumsPage*>` guard) and
  closes the remaining DrumPage tabs per-id via `mRibbon->closeTab` (which fires
  `onTabClosed` for the full engine teardown); Rusty's ribbon tab + singleton engine
  survive and keep playing. The "Replace Drums?" confirmation keeps its DrumPage-only
  scan (docket pick 2 — no prompt when nothing is replaced, e.g. a Rusty-only project),
  and its text is corrected from the overbroad "remove all currently selected drums" to
  "replace all of your current drum tabs with the new kit's drums," with a
  "(BaySickRustyDrums is not affected by kit loads.)" line shown only when a Rusty tab
  is present.
- **Task 2 — LIFE-02 (re-add auto-reloads the last kit):** new session-scoped
  `mLastRustyKitFile`, captured on every kit load (the `onKitLoaded` lambda) and again
  at Rusty tab close (while the engine is still alive). `addBaySickRustyDrumsTab` now
  calls the existing `reloadForProjectRestore(mLastRustyKitFile)` when a remembered kit
  exists — program combo + ARIA panel + mixer strips all synced through the restore
  primitive, busy overlay for free — so re-adding Rusty after deleting it brings back
  the last kit instead of an empty page. Guarded off during project load (the restore
  path drives its own reload) and on fresh sessions with no memory (the
  pick-a-program overlay is unchanged). Not persisted — project save/restore carries its
  own `kitPath` record.

#### Found along the way

The plan line "prompt scan counts Rusty content too" was vestigial once the teardown
fix landed — pre-fix it was protective (a kit load destroyed Rusty and the
DrumPage-only scan gave no warning); post-fix a kit load can't touch Rusty, so counting
it would only fire an empty prompt. Surfaced as a docket; Jeff picked DrumPage-only scan
+ text clarification (pick 2). Nothing else routed out.

#### Known seams (campaign-visible)

`mLastRustyKitFile` is session memory only — quitting and relaunching forgets the last
kit (by design; project restore is the persistent path). The auto-reload keys on the
last kit's file still existing on disk (`existsAsFile` guard) — a deleted kit file
falls back to the empty pick-a-program page.

**Verification:** bulk-run R2 — campaign section §B.19 (4 scenarios). Build-confirmed
clean (Release+Debug) at the combined task gate, 2026-07-18.
