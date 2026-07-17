# QA-L — UI Polish + Navigation + Per-Drum MIDI Notes — Plan (tidy-unsticking-magpie)

> **Canonical path (post-approval):** `Plans & Specs/Batch Plans/tidy-unsticking-magpie.md`.
> Paired running notes: `Running Notes/tidy-unsticking-magpie.md`.
> **Execution mode: bulk run** (R1-R5): §B at code-complete; Work Log HELD; ONE commit;
> spec calls ASKED.

## Context

The grouped-polish batch. Migrations already locked: BLU-378/379/492 OUT (→ QA-ApvtsAutomation,
marathon 18). Docket rounds: #11=B folds per-drum MIDI notes IN (QA-Drum-Polish deleted as a
batch — §5/§9 routing rides the group-open docs commit); #17=c drops Clips picker gating;
#18 + C lock the two navigation builds. Scout premise corrections: MIX-05's real cause is
Layer/Bass/Drum strips never being removed on page close (orphans → overlap), not a missing
resized(); MIX-07 is the same asymmetry (remove helpers skip the refresh callback the
bus-delete path fires); UI-01 is vendored-JUCE any-button-release triggering; NAV-01 is the
header/grid sync living only on a timer tick. Risk: medium (many small surfaces). Effort:
~12-18h. Dependencies: none.

## Spec calls already locked

| ID | Decision |
|----|----------|
| #10=a/a | Per-drum MIDI trigger note, kit fan-out: anywhere in the Drums context every drum with an assigned note listens simultaneously (pad controller plays the kit); unassigned drums keep normal focused-tab behavior; DEFAULT = nothing mapped; assignment via the drum's right-click menu |
| #11=B | Folded into QA-L; QA-Drum-Polish dropped from the group |
| #17=c | Clips picker gating dropped as moot |
| #18 | FX Rack button at the RIGHT END of every page-tab button row (Clips/Vox/Inst/Layers/Bass/Drums; BOTH Inst variants — live + Guitars/Basses player), jumping to that strip's rack on the Effects page |
| C (revised) | Piano-roll menu bar gains TWO buttons right of the roll-selector dropdown: "Player Page" → the selected roll's player page; "FX Rack" → the selected roll's FX rack |
| §5 | FILE-03: duplicate-name delete collapse fixed + auto-numbering on duplicate drop |

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open.

## Files to modify

- Vendored JUCE `juce_PopupMenu.cpp` (:1492-1512 any-button trigger) OR app-side guard —
  UI-01 (one central fix; framework-workaround comment per Rule 6)
- `Source/Standalone/StandaloneEditor.cpp` — `resolveAutomationDisplayName` (:3123-3221),
  tab-close handler (:4340-4470: add Layer/Bass/Drum strip removal + dropdown refresh),
  FX-jump handoff precedent (:3609-3614, `mLastFXChannel`)
- `Source/Standalone/EventEditor.cpp` — raw-paramId fallback (:1116-1122)
- `Source/Standalone/MixerPage.cpp` — new removeLayer/Bass/DrumChannel (clearDynamicStrips
  :2527 + removeClipChannel :2746 precedents), refresh-callback parity with deleteSecondaryBus
  (:2917)
- `Source/Standalone/EffectsPage.cpp` — `rebuildChannelDropdown` (:192) trigger wiring
- `Source/Standalone/BuilderPage.cpp` — NAV-01 header sync (`setViewportYOffset` :5738, the
  timer-only sync :6410; wire event-driven sync on scroll/resize/zoom), FILE-03
  (row→libIdx first-match :1360-1370 + `(path,owner)` cascade :893-897; auto-number hook
  :4222-4239)
- `Source/Standalone/PianoRoll.cpp` — LDT-394 accuracy (beatToX truncation :500, yToNote int
  division :502-506, xToBeat pixel-centering :499); menu-bar Player Page / FX Rack buttons
- Page classes (LayersPage/BassPage/DrumPage/ClipsPage/VoxPage/InstPage both variants) —
  #18 FX Rack button on each page-tab button row
- `Source/Standalone/DrumPage.cpp` — right-click menu gains MIDI note assignment (net-new;
  the "existing placeholder" claim was wrong); `Source/PluginProcessor.cpp` — live-MIDI drum
  branch (:2505, copy loop :2524-2540) kit fan-out; param `mixer_drum_{N}_inputNote`
  (chokeGroup precedent)

## Tasks

### Task 1 — UI-01 right-click menu activation + UI-02 lane names
- [ ] Kill any-button-release item triggering (central fix at the vendored PopupMenu layer,
      left-button-only trigger; Rule 6 framework-workaround comment) — right-click no longer
      activates menu items anywhere.
- [ ] UI-02: "(deleted slot)" resolution — deleted-slot lanes resolve to a stable readable
      name (last-known effect name persisted with the lane) and EventEditor's raw-UUID
      fallback formats readable; stale-row flag stays.
- [ ] Build-confirm gate + running-notes checkpoint.

### Task 2 — Mixer strip lifecycle (MIX-05 real cause) + Effects dropdown (MIX-07)
- [ ] Individual removeLayer/Bass/DrumChannel helpers; tab-close removes the page's strip
      (order-vector hygiene per the removeClipChannel precedent) — orphan overlap dies.
- [ ] All remove* helpers fire the refresh callback (deleteSecondaryBus parity) →
      Effects-page dropdown rebuilds on every close/delete path.
- [ ] Build-confirm gate + checkpoint.

### Task 3 — NAV-01 grid/header alignment
- [ ] Header `mYOffset` sync becomes event-driven: viewport scroll/resize/zoom all push the
      offset immediately (timer stays as backstop) — no more stale-until-tick desync on
      resize.
- [ ] Build-confirm gate + checkpoint.

### Task 4 — Navigation builds (#18 + C)
- [ ] FX Rack button at the right end of every page-tab button row (all six page types, both
      Inst variants): sets the strip handoff + jumps to Effects with that channel selected.
- [ ] Piano-roll menu bar: "Player Page" + "FX Rack" buttons immediately right of the
      roll-selector dropdown, acting on the roll's current engine.
- [ ] Build-confirm gate + checkpoint.

### Task 5 — FILE-03 duplicate-name handling
- [ ] Delete uses the exact library index everywhere (kill the first-match path collapse);
      duplicate drop auto-numbers the new entry (existing "New Page" duplicate-file precedent
      extended) so same-name entries stay distinct.
- [ ] Build-confirm gate + checkpoint.

### Task 6 — LDT-394 piano-roll mouse accuracy
- [ ] Fix the found offenders: beatToX round-not-truncate, yToNote floor-division correctness
      near boundaries, xToBeat pixel-center; note-edge tolerance sanity re-check after.
- [ ] Build-confirm gate + checkpoint.

### Task 7 — Per-drum MIDI trigger notes (#10)
- [ ] `mixer_drum_{N}_inputNote` param (default unmapped) + "MIDI Note" assignment in the
      drum's right-click menu (note picker + Unassign).
- [ ] Kit fan-out in the live-MIDI drum branch: while MIDI focus is on any drum tab, incoming
      notes route to every drum whose assigned note matches (multiple drums = simultaneous);
      unassigned drums keep the normal focused-tab pass-through; assigned drums also still
      play normally when focused.
- [ ] Build-confirm gate + checkpoint.

### Task 8 — Close (bulk run)
- [ ] §B section authored; Work Log entry HELD; ONE commit (message + full git status →
      Jeff approves). §5/§9 QA-Drum-Polish fold-routing rides the group-open docs commit
      (recorded there, not here).

## Verification (§B-destined scenarios)

1. Right-click over any open menu — nothing activates; left-click still does.
2. Delete an effect that has an automation lane — lane shows a readable name, no UUID soup.
3. Close a Layers/Bass/Drum page — its mixer strip disappears, neighbors re-flow, Effects
   dropdown no longer lists it; re-add — one strip, no overlap.
4. Resize the window with the Builder scrolled — track header rows stay glued to grid rows.
5. Every page type: FX Rack button at the row end jumps to that strip's rack; piano roll:
   Player Page / FX Rack buttons jump per the selected roll.
6. Two same-named clips in the browser — delete one, the other survives; re-drop a same-named
   file — auto-numbered entry.
7. Piano roll: clicks land on the intended beat/row at deep zoom (edge cases at row/bar
   boundaries).
8. Pads: assign notes to 3 drums, focus any drum tab — pads play kick/snare/hat
   simultaneously; unassigned drum still plays when focused; fresh project = nothing mapped.

## Routing notes (Rule 3)

Vendored-JUCE edit is a framework workaround — comment per Rule 6, note in running notes for
the license/vendor ledger. Surfaces here are wide; anything spec-shaped mid-task gets ASKED.

## Carry-Forward Reference touch points

- Task 2: `reference_mixer_strip_pattern_audit` memory checklist BEFORE the strip-lifecycle
  diff lands (~15-site audit).
- Task 7: live-MIDI dispatch records (audition pattern section; kDrumPRTarget constants).
