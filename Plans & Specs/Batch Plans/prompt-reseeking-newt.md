# QA-J' — Stacking-Batch Residuals (unmute re-sync + applicator-map hygiene) — Plan (prompt-reseeking-newt)

> **Canonical path (post-approval):** `Plans & Specs/Batch Plans/prompt-reseeking-newt.md`.
> Paired running notes: `Running Notes/prompt-reseeking-newt.md`.
> **Execution mode: bulk run** (R1-R5): §B at code-complete; Work Log HELD; ONE commit;
> spec calls ASKED.

## Context

QA-J collapsed at the marathon (docket 1): the headline restructure shipped via prior batches;
the campaign owns QA-J-Verify (ear-verify DSP-06 + the §C ledger). The TWO code residuals land
here. Scout re-located both (old refs stale): the seekNeeded/PV logic lives in
PluginProcessor.cpp now (:734 / :1440 / :1590, all 2-second tolerance) and the growing UI map
is `mAutomationApplicators`/`mAutomationValueReaders` (StandaloneEditor.h:709/:713, no erase
anywhere). Risk: low-medium (audio-thread adjacent, small diffs). Effort: ~2-4h.
Dependencies: none.

## Spec calls already locked

| ID | Decision |
|----|----------|
| Marathon 1 | QA-J → QA-J-Verify (campaign); the two residuals are G3 code items |

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open.

## Files to modify

- `Source/PluginProcessor.cpp` — mute gates (:623-626, :1061-1068) skip the whole per-clip
  body incl. `expectedFilePos` advance; seekNeeded sites (:734-737, :1440-1443, :1590-1593)
- `Source/Standalone/StandaloneEditor.cpp/.h` — applicator maps (:709/:713),
  `closeAllDynamicTabs` (:10199), `resetProjectState` (:10259), tab-close/unregister paths

## Tasks

### Task 1 — Unmute re-sync (stretch-path staleness)
- [ ] Per-player muted-edge flag: on mute→unmute transition force the re-sync (vocoder reset +
      `requestSeek(pvRefPos)` + `expectedFilePos = pvRefPos`) on the first audible block —
      the sub-2-second stale offset dies; both mute-gate sites covered; loop-wrap/seek
      behavior unchanged.
- [ ] Build-confirm gate + running-notes checkpoint.

### Task 2 — Applicator-map hygiene
- [ ] Erase a track's paramId entries on its tab close/unregister; full clear of both maps in
      `resetProjectState` (project close/load) so loads never inherit stale closures.
      Registration paths untouched.
- [ ] Build-confirm gate + checkpoint.

### Task 3 — Close (bulk run)
- [ ] §B section authored; Work Log entry HELD; ONE commit (message + full git status →
      Jeff approves).

## Verification (§B-destined scenarios)

1. Stretched clip playing → mute its row ~1 bar → unmute: playback is in the RIGHT place
   immediately (no smeared/stale second), song + pattern modes.
2. Mute >2 s then unmute: still correct (old threshold path unharmed).
3. Add/delete engine tabs repeatedly + load a different project: automation still applies to
   live params; no stale-project automation fires (map cleared).

## Routing notes (Rule 3)

Anything bigger than these two surfaces → running notes + section-pass routing (the campaign
owns the QA-J re-verify ledger).

## Carry-Forward Reference touch points

- QA-Ec beat-domain position records (G1 carry-over) — Task 1 must preserve the beat-domain
  posD math; the fix is an edge re-sync, not new position math.
