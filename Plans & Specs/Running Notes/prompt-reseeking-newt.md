# Running Notes — QA-J' (prompt-reseeking-newt)

> Append-only mid-batch log. New entry at every checkpoint (commit landed / sub-task
> verified / finding captured / spec call resolved / scope pivot). Consumed at doc-close
> (section pass under bulk-run R2) as the primary input for the held Work Log entry.

Pair file: `Plans & Specs/Batch Plans/prompt-reseeking-newt.md`
Convention: Main Plan §0 "Batch Plans + Running Notes layout" (exemplar
`federated-bouncing-cupcake.md`).

## 2026-07-17 — Group open (G3)

Seeded at G3 group approval. Two residuals only (QA-J collapsed to QA-J-Verify at the
marathon; campaign owns the re-verify ledger). Sites re-located at scout: seekNeeded/PV
logic now in PluginProcessor.cpp (:734/:1440/:1590), the growing maps at
StandaloneEditor.h:709/:713. Coding starts after QA-I.

## 2026-07-18 — Task 1 — Unmute re-sync (stretch-path staleness)

New `unmuteResync` bool on AudioClipPlayer (PluginProcessor.h, next to pvOutFrac). Mute
gates skip the whole per-clip render body, freezing expectedFilePos while the transport
advances; a sub-2-second gap slips under the PV seek tolerance and playback resumes
offset, permanently. Both gate sites set the flag (choke gate + row/builder-mute gate at
each; Path A renderAudioClipsForRow :727, Path B decodeFilePlayClip :1180); consumed on
the first audible block AFTER all pre-render early-outs (out-of-range / outSamples<=0 /
EOF continues can't eat it), then OR'd into the seekNeeded expressions so the existing
re-sync body (vocoder reset + requestSeek(pvRefPos) + expectedFilePos = pvRefPos +
pvOutFrac 0) fires. THREE seekNeeded sites covered, not the plan's two: pristine PV Path
A (:848), pristine PV Path B (:1718), and the QA-Fa align-warp PV path (:1564) — same
2-second tolerance, same staleness (its own comment admits the divergence "sat below the
2 s seek net"); scout refs predated QA-G/H/I line shifts and missed it. Warp site guards
`fadeMode != +1`: a fresh engage prefill just positioned the feed at pStart, and a forced
reset would empty the OLA it built. Direct/reverse paths self-heal (position computed
fresh from transport) — flag consumption alone. Loop-wrap/seek tolerances + beat-domain
posD math untouched (Carry-Forward touch point honored). No allocation, no locks — plain
bool in the same per-player thread domain as expectedFilePos. No spec calls, no
diagnostics added.

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-18.

## 2026-07-18 — Task 2 — Applicator-map hygiene (+ docket-1 rack-pid stamping fix)

Neither automation map (mAutomationApplicators / mAutomationValueReaders) had an erase
path — std::map operator[] registration only ever inserted, so tab churn grew them
unbounded and closed tabs kept dead paramIds listed in the Event Editor param browser.
New StandaloneEditor::eraseAutomationEntriesWithPrefix (generic over both maps); every
dynamic-tab close in onTabClosed drops its channel's families: Layers/Bass/Drums/Clips
erase the mixer-strip family ("mixer_layer_{N}_" — strip knobs + the ~288 per-channel EQ
band entries registered when the Effects page visits the channel) AND the rack-slot
family ("layer_{N+1}_" — 1-based for layers/basses per EffectsPage::getChannelPrefix's
id-199 mapping, 0-based for drum/audio); Vox/Inst erase theirs; a Rusty close erases all
kMaxRustyStrips strip families. Full clear in resetProjectState (runs inside every
closeAllDynamicTabs = project load / New Project / editor teardown) + STATIC RE-SEED:
the ctor's static registrations (every APVTS param + "global_tempo") moved verbatim into
new registerStaticAutomationHandlers(), called from the ctor and again after the clear —
a bare clear would have killed tempo automation for the rest of the session
("global_tempo" is the non-APVTS lane that actually flows through the map; APVTS-backed
lanes short-circuit via apvts.getParameter before consulting it, so the map's live
consumers are non-APVTS applicators, reader seeds, and the Event Editor param list).
Registration paths untouched per the plan lock.

FINDING -> DOCKET -> FIXED IN-BATCH (Jeff pick: option 1): EffectsPage::getChannelPrefix
(the table stamping rack-slot automation pids that lanes persist) was never updated for
the J-6 dropdown renumbering (2026-05-03) — Aux strips stamped "vox_N_", Vox strips
"inst_N_", Inst + Rusty strips fell to bare "fx"; Rusty Bus (dropdown id 12, also added
at J-6) had no switch case (bare "fx" too). Fixed: 600=aux_N / 700=vox_N / 800=inst_N /
900=rusty_N + case 12 "rusty_bus" (the fifth limb of the same defect, flagged to Jeff on
top of the docket text with a standing veto). Accepted break per docket-1: lanes saved
on aux/vox/inst/rusty(+Rusty Bus) RACK knobs before this batch stop matching (stale in
the browser; re-automate recreates); no migration shim per the pre-v1 rule; mixer-strip
knob lanes unaffected. The now-safe rack-family erases were then added to the
Vox/Inst/Rusty close paths (the first Task-2 pass had skipped them because the
wrong-kind aliasing made a prefix erase unsafe against live strips).

FINDINGS ROUTED (Rule 3 at section pass; both verified in source; display-only,
pre-existing, outside the locked scope):
1. resolveAutomationDisplayName::findRackForBase (StandaloneEditor.cpp:3230) has no
   vox_/inst_/aux_/rusty_/rusty_bus/vox_bus*/inst_bus* cases -> rack lanes on those
   strips display as raw paramIds in the Automate menu / Event Editor titles (already
   true pre-fix; the re-key does not regress it).
2. Same resolver's layer_/bass_ cases feed the stamped 1-BASED number straight into the
   0-based getLayerPageRack/getBassPageRack (VibeGraph.h:367) -> UUID lookup misses or
   resolves the wrong page's rack for layer/bass rack-lane display names.

No spec calls beyond the docket above; no diagnostics added.

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-18 — one gate covering the hygiene
pass + the docket-1 fix together.

## 2026-07-18 — QA-J' CODE-COMPLETE

Tasks 1-2 shipped + the docket-1 rack-pid stamping fix; both gates build-confirmed
clean. §B.16 authored (5 scenarios; `blocks:` hash backfills at the next docs commit per
the B.12-B.15 precedent). Work Log entry drafted + HELD below. ONE batch commit surfaced
for approval (carries the two QA-I doc stragglers per plan). Two display-only findings
routed at section pass (see the Task 2 entry).

## Held Work Log entry

> HELD per bulk-run R2 — apply to `Implemented Work Log.md` verbatim when §B.16 passes.

### 2026-07-18 19:00 PT — QA-J' — Stretch-path unmute re-sync + automation applicator-map hygiene + rack-pid stamping fix

**Bucket:** Cross-cutting Infrastructure, Effects, System Pages

#### Done

- **Task 1 — unmute re-sync (stretch-path staleness):** new `unmuteResync` bool on
  `AudioClipPlayer` (PluginProcessor.h). Mute/choke gates skip a clip's whole render
  body, freezing `expectedFilePos` while the transport advances; a sub-2-second gap
  slipped under the PV seek tolerance and playback resumed offset — and STAYED offset
  (both positions then advance in lockstep, so the drift never crossed the threshold).
  Both gate sites set the flag (renderAudioClipsForRow + decodeFilePlayClip; choke gate
  and row/builder-mute gate at each); it is consumed on the first audible block after
  every pre-render early-out (out-of-range / zero-sample / EOF skips can't eat it) and
  OR'd into the seekNeeded expressions, so the existing re-sync body (vocoder reset +
  `requestSeek(pvRefPos)` + `expectedFilePos = pvRefPos` + pvOutFrac reset) fires.
  THREE seekNeeded sites covered where the plan listed two — the QA-Fa align-warp PV
  path carries the same 2 s tolerance and the same staleness (scout refs predated the
  QA-G/H/I line shifts); its site guards `fadeMode != +1` since a fresh engage prefill
  has just positioned the feed and a forced reset would empty the OLA it built.
  Direct/reverse paths self-heal (position computed fresh from the transport) — flag
  consumption only. Loop-wrap/seek tolerances and the beat-domain posD math untouched
  (Carry-Forward touch point honored). Plain bool — no allocation, no locks, same
  per-player thread domain as expectedFilePos.
- **Task 2 — applicator-map hygiene:** the automation applicator + value-reader maps
  had NO erase path (registration only ever inserted), so tab churn grew them unbounded
  and closed tabs kept dead paramIds in the Event Editor param browser. New
  `eraseAutomationEntriesWithPrefix()` — every dynamic-tab close in onTabClosed drops
  its channel's paramId families (mixer-strip family for all kinds; rack-slot family
  where the stamping is unambiguous — 1-based layer/bass quirk matched, Rusty erases
  all strip families). Full clear in `resetProjectState` (inside every
  closeAllDynamicTabs: project load / New Project / editor teardown) plus static
  re-seed via new `registerStaticAutomationHandlers()` (the ctor's APVTS-param +
  "global_tempo" registrations, moved verbatim; called from ctor and post-clear) — a
  bare clear would have killed tempo automation for the session, since "global_tempo"
  is the non-APVTS lane that actually flows through the map while APVTS lanes
  short-circuit ahead of it. Registration paths untouched per the plan lock.
- **Docket-1 fix (asked mid-batch; Jeff pick: option 1):**
  `EffectsPage::getChannelPrefix` — the table stamping rack-slot automation paramIds —
  never learned the J-6 dropdown renumbering: Aux strips stamped `vox_N_`, Vox strips
  `inst_N_`, Inst/Rusty strips bare `fx`, and Rusty Bus (id 12) had no case. Corrected
  to `aux_N` / `vox_N` / `inst_N` / `rusty_N` + `rusty_bus` (the bus case flagged
  separately with a standing veto — same table, same defect). Accepted break: lanes
  saved on those RACK knobs before this batch show stale (re-automate recreates); no
  migration per the pre-v1 rule. The now-safe rack-family erases added to the
  Vox/Inst/Rusty close paths.

#### Found along the way

- The align-warp third seekNeeded site (fixed in Task 1, above).
- getChannelPrefix's stale J-6 ranges + missing Rusty Bus case (docketed; fixed
  in-batch per Jeff's pick, above).
- ROUTED at section pass (display-only, pre-existing, verified in source):
  resolveAutomationDisplayName::findRackForBase lacks vox/inst/aux/rusty(+bus) cases
  (those rack lanes display as raw paramIds), and its layer_/bass_ cases feed the
  1-based stamped number into the 0-based page-rack getters (wrong/no rack resolved
  for display names).

#### Known seams (campaign-visible)

Old saves with lanes on aux/vox/inst/rusty(+Rusty Bus) rack knobs show stale lanes
(accepted at docket-1). Reader seeds for a param registered only by a closed tab fall
back to 0.5 until its UI re-registers (bounded by the project-boundary re-seed). Rack
lanes on vox/inst/aux/rusty strips still display raw paramIds (routed finding above).

**Verification:** bulk-run R2 — campaign section §B.16 (5 scenarios). Build-confirmed
clean (Release+Debug) at both gates, 2026-07-18.
