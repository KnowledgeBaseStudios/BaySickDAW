# QA-Verify (code half) — BaySickPedals state hygiene: tag collision, enum pinning, log strip — Plan (sturdy-tagging-pangolin)

> **Canonical path:** `Plans & Specs/Batch Plans/sturdy-tagging-pangolin.md` (mirrored at G4
> group approval; home-dir copy deleted). **For execution:** bulk-run G4 batch 4 of 8. The
> 10-engine preset walk is campaign §E — NOT this batch. §B authored at code-complete; one
> source commit.

## Context

Premise update (scout + desk verification 2026-07-25): the QA-Inventory "pedalboard preset
doesn't restore" regression is **already fixed** — the 2026-05-05 base64 fix
([BaySickPedalsProcessor.cpp:425-436](Source/BaySickPedals/BaySickPedalsProcessor.cpp:425):
`MemoryBlock::fromBase64Encoding` replacing raw `convertFromBase64`, which silently returned 0
bytes on the `<byteCount>.<base64>` prefix). The always-on round-trip log
(`Documents/BaySickDAW/pedals_state_log.txt`, 1.3 MB) shows exactly ONE failing restore event in
its entire history (six `decoded 0 bytes` lines sharing one timestamp = one pre-fix restore with
six populated slots — verified) and 1,150+ clean cycles since. The round-trip walk (capture ->
APVTS copy + per-slot base64 blobs -> restore) drops no persistent field; page-preset and
project paths share the same core. Jeff's campaign §E walk remains the runtime confirmation.

What Jeff locked instead (docket 6 = B + log A): ship the hygiene hardening now, remove the log.

- **Risk:** low. No DSP, no audio thread; state-shape edits with compat consequences handled
  below.
- **Effort:** ~1-2 h (collapsed from the §5 "medium").
- **Dependencies:** none.

## Spec calls already locked

| ID | Decision | Reasoning |
|----|----------|-----------|
| Docket 6 = B | Fix the root/APVTS tag collision + pin the implicit FX-rack enum ordinals | Both are latent data-integrity hazards for saved pedal state |
| Docket 6 log = A | `pedalsLog` removed now | Always-on Release disk I/O on the save/load path; evidence already served its purpose |
| §E (marathon) | The 10-engine / 4-family preset walk runs in the CAMPAIGN, not here | Locked 2026-07-08 |

**Implementation call, stated for R5 (tag fix without breaking existing data):** the outer
wrapper and the APVTS child are both named `BaySickPedalsState`
([BaySickPedalsProcessor.cpp:16 vs :334](Source/BaySickPedals/BaySickPedalsProcessor.cpp:16)) —
restore currently works only because `getChildWithName` happens to hit the APVTS child first.
Renaming the outer tag outright would reject the pedals state embedded in EVERY existing
project and pedalboard preset (restore validates the root tag). So: new saves write outer tag
`BaySickPedalboardRoot`; the loader accepts old-or-new outer tag with one `||` — a one-line
tolerance, not a migration system. Jeff's existing "Jeff 1.xml" and all saved projects keep
loading; new writes are unambiguous.

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open.

## Files to modify

- Task 1: `Source/BaySickPedals/BaySickPedalsProcessor.cpp` (:15-16 tags, :326-367 capture,
  :369-386 restore validation, :498-558 preset envelope)
- Task 2: `Source/EffectRack.h` (:17-28 implicit enum block)
- Task 3: `Source/BaySickPedals/BaySickPedalsProcessor.cpp` (pedalsLog :19-30 + every call site
  in capture/restore/preset paths)

## Tasks

### Task 1 — Root/APVTS tag disambiguation

- [ ] Add `kStateRootTagV2 = "BaySickPedalsRoot"`; `captureFullState` writes it as the outer
  tag; `restoreFullState` + `loadPedalboardPreset` accept `V2 || legacy "BaySickPedalsState"`.
- [ ] APVTS-child lookup becomes explicit: select the child that has PARAM children / matches
  `apvts.state.getType()`, not first-name-match — the collision's actual hazard.
- [ ] Grep all readers of the pedals state tags (page preset via `engineRootTag`
  "BaySickPedalsState" in [InstPage.cpp:388-400](Source/Inst/InstPage.cpp:388) — confirm that
  config keys on the tag the BLOB parse expects and update if it inspects the outer tag).

### Task 2 — Pin the FX-rack enum ordinals

- [ ] `EffectRack.h:17-28`: assign explicit `= N` values matching CURRENT ordinals exactly
  (zero behavior change) + a Rule-6 category-6-style comment: saved pedal-slot `type` ints
  depend on these — never reorder, append only. (The pedal-native block :34-62 is already
  explicit; this closes the gap.)

### Task 3 — Strip `pedalsLog`

- [ ] Remove the function + all call sites (capture, restore, preset save/load). Grep zero refs.
- [ ] Rule 4 retro-catalog row in running notes: | BaySickPedalsProcessor.cpp save/load path |
  (untagged, plain lines) | Bug-B round-trip forensics, 2026-05-05 | **Removed this batch
  (Jeff 2026-07-25, docket 6 log=A)** |. The on-disk `pedals_state_log.txt` stays for Jeff to
  delete or keep — not touched by the batch.

## Batch close (bulk-run per-batch loop — one commit per batch)

- [ ] Tell Jeff to run `do_build.bat`; fix until BOTH configs build clean.
- [ ] Author this batch's Master Test Plan §B section from the Verification list below
  (`blocks:` = this batch's commit, hash backfilled at commit).
- [ ] `/draft-doc batch-close` -> append under `## Held Work Log entry (apply at section pass)`
  in the running notes. Do NOT touch the Implemented Work Log or the §5 STATUS line now (R2).
- [ ] Append the running-notes code-complete entry (incl. the Rule 4 removal row).
- [ ] ONE batch commit (Rule 9): `QA-Verify: <one-line what> (<scope>)` + `Co-Authored-By`
  trailer; surface message + FULL git status; commit only on Jeff's approval.

## Verification (authors into Master Test Plan §B)

1. Load the existing "Jeff 1" pedalboard preset (pre-batch file, legacy outer tag): all 8 slots
   restore with types + knob values intact.
2. Save a new pedalboard preset -> close app -> reopen -> load it: identical board (new outer
   tag path).
3. Open a pre-batch PROJECT containing pedals state: pedals restore intact (legacy-tag
   tolerance through the project path).
4. Save project with pedals -> reopen: intact (new-tag project path).
5. Page preset with a pedals Inst tab: save -> delete tab -> load page preset: board restored.
6. After a session of pedal edits + preset saves/loads: `pedals_state_log.txt` has not grown
   (log strip confirmed).

## Routing notes (Rule 3)

The scout's remaining watch items — NAM pedal restores by absolute path (missing file = silent
no-load) and per-DSP field-completeness of EQ/Tuner pedal serializers — are NOT in-batch scope;
they log in running notes for close-time routing (campaign §E will exercise both surfaces).

## Carry-Forward Reference touch points

None — message-thread state IO only. The `no-backward-compat pre-v1` standing rule is honored:
the one-line tag tolerance is load-acceptance, not a migration system (stated above for the R5
read; veto-able).
