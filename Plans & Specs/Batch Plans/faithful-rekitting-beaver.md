# QA-M — Engine Restoration Lifecycle (Rusty vs kit-load) — Plan (faithful-rekitting-beaver)

> **Canonical path (post-approval):** `Plans & Specs/Batch Plans/faithful-rekitting-beaver.md`.
> Paired running notes: `Running Notes/faithful-rekitting-beaver.md`.
> **Execution mode: bulk run** (R1-R5): §B at code-complete; Work Log HELD; ONE commit;
> spec calls ASKED.

## Context

LIFE-01/LIFE-02, both root-located by the scout: the full-kit loader (`loadKitImpl`) tears
down EVERY Drums-type ribbon tab — and the Rusty tab is Drums-typed — then only re-creates
DrumPage tabs, so Rusty is destroyed and never re-spawned (its "replace?" prompt also only
scans DrumPage engines, so a Rusty-only project loses it silently). Re-adding Rusty builds an
empty page: the reload primitive (`reloadForProjectRestore`) exists but is wired only to
project restore — no session memory of the last kit. Risk: low-medium (teardown paths).
Effort: ~3-5h. Dependencies: none.

## Spec calls already locked

| ID | Decision |
|----|----------|
| §5 | LIFE-01: kit-load must not destroy Rusty; LIFE-02: re-adding Rusty auto-reloads its last kit |

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open.

## Files to modify

- `Source/Standalone/StandaloneEditor.cpp` — `loadKit`/`loadKitImpl` (:6684/:6725; teardown
  loop :6756-6764; prompt scan :6689-6694), `addBaySickRustyDrumsTab` (:7349),
  restore-path precedent (:10837-10862), `onTabClosed` Rusty branch (:4395-4433)
- `Source/Standalone/BaySickRustyDrumsPage.cpp` — `reloadForProjectRestore` (:200-228)
- `Source/PluginProcessor.cpp` — `destroyBaySickRustyDrums` (:5814) interaction only

## Tasks

### Task 1 — LIFE-01: kit-load leaves Rusty alone
- [ ] `loadKitImpl` teardown filters Rusty tabs OUT of the Drums-type close loop (kit loads
      target DrumPage kits; Rusty untouched, keeps playing its kit).
- [ ] Replace-confirmation prompt scan counts Rusty content too (no more silent loss framing;
      prompt text stays honest about what gets replaced — DrumPage tabs only).
- [ ] Build-confirm gate + running-notes checkpoint.

### Task 2 — LIFE-02: re-add auto-reloads the last kit
- [ ] Session-side last-kit memory captured when a Rusty kit loads (and on tab close);
      `addBaySickRustyDrumsTab` calls the existing `reloadForProjectRestore(lastKit)` when a
      remembered kit exists — combo + program state synced (restore-path parity). Fresh
      session with no memory = current empty behavior.
- [ ] Build-confirm gate + checkpoint.

### Task 3 — Close (bulk run)
- [ ] §B section authored; Work Log entry HELD; ONE commit (message + full git status →
      Jeff approves).

## Verification (§B-destined scenarios)

1. Rusty tab with kit loaded + DrumPage tabs present → load a full kit from the kit menu:
   DrumPage tabs replaced per the prompt, Rusty untouched and still playing.
2. Rusty-only project → load a kit: Rusty survives; prompt behavior honest.
3. Delete the Rusty tab → re-add Rusty: last kit auto-loads, program combo correct, plays.
4. Save/close/reopen a Rusty project: restore path unchanged (regression check).

## Routing notes (Rule 3)

Teardown findings beyond these two paths (other engine types with the same class of bug) get
logged + routed at section pass, not silently expanded here.

## Carry-Forward Reference touch points

- Phase J Rusty records (kit load + program change reuse — `loadBaySickRustyDrumsKit` does
  NOT destroy on program change; keep it that way).
