# Running Notes — QA-Verify code half (sturdy-tagging-pangolin)

> Append-only mid-batch log. Entries land at every checkpoint (commit landed / finding captured
> / spec call resolved / scope pivot) per `feedback_draft_doc_running_notes_every_checkpoint.md`.
> At code-complete, `/draft-doc batch-close` consumes this file to compile the Implemented Work
> Log entry, which is HELD here (bulk-run R2) and applied at the batch's campaign section pass.
> (The 10-engine / 4-family preset walk is campaign §E, not this batch.)
>
> Pair file: [`Plans & Specs/Batch Plans/sturdy-tagging-pangolin.md`](../Batch%20Plans/sturdy-tagging-pangolin.md).
> Conventions: Main Plan §0 (Batch Plans + Running Notes layout, locked 2026-05-11).

## 2026-07-25 — Tasks 1-3 — code-complete (build 0/0 at every gate)

### Task 1 — Root/APVTS tag disambiguation

The collision was real and worse than "two things share a name": `kStateRootTag` was
`"BaySickPedalsState"` AND the APVTS is constructed with that exact type string
([BaySickPedalsProcessor.cpp](../../Source/BaySickPedals/BaySickPedalsProcessor.cpp) ctor
initialiser), so any tag lookup starting ABOVE the pedals root could land on the wrapper.

- New saves write `kStateRootTag = "BaySickPedalsRoot"`; `kStateRootTagLegacy` retains
  `"BaySickPedalsState"`. `restoreFullState` accepts either with one `||`, and
  `loadPedalboardPreset` falls back to the legacy payload tag. Load-tolerance, not a migration
  system — `feedback_no_backward_compat_pre_v1` holds.
- **APVTS child lookup now keys off `apvts.state.getType()`** rather than a hard-coded string.
  Stronger than the plan's "match the child that has PARAM children": it cannot drift if the
  APVTS type is ever renamed, and it is exact rather than heuristic.
- **Plan self-contradiction (mine to resolve):** the Context section specifies outer tag
  `BaySickPedalboardRoot`, Task 1's checkbox specifies `BaySickPedalsRoot`. Took the checkbox —
  it is the executable line. No user-visible difference; flagged to Jeff in chat.
- **Plan bullet 3 was a false alarm — InstPage needs NO change.** The plan asked to confirm
  whether the page-preset config keys on the outer tag.
  [InstPage.cpp:405](../../Source/Inst/InstPage.cpp:405) sets
  `slot.engineRootTag = "BaySickPedalsState"`, but that field is read ONLY inside the sfizz
  kit-path branch at [PagePresetIO.cpp:119-127](../../Source/Standalone/PagePresetIO.cpp:119),
  which is gated on `slot.kitLoadCallback` — pedals never sets one. Pedals state flows through
  `setStateInformation` -> `restoreFullState`, which now accepts both tags. Editing it would
  have touched the sfizz path for no benefit. **Left alone deliberately.** The value is now
  arguably misleading (it names the APVTS type, not the blob root); noted, not changed, since
  the region is out of scope and the field is inert for this engine.

### Task 2 — FX-rack enum ordinals pinned

All 13 values in the [EffectRack.h](../../Source/EffectRack.h) `EffectType` head block made
explicit (`None = 0` .. `DeEsser = 12`), matching the previously-implicit ordinals exactly —
zero behavior change. Rule-6 category-6 comment added recording WHY: both FX-rack slots and
pedal slots persist `type` as the raw int, so an insertion or re-order silently repoints every
saved slot at a different effect. The pedal-native block (`= 100`+) was already explicit; this
closes the gap. Covered by §B.28 QV-7 / QV-8.

### Task 3 — `pedalsLog` stripped

Function + all 8 call sites removed (capture x2, restore x4, plus the two decode traces). Grep
confirms zero remaining references. Two locals that existed only to feed the log (`sp`,
`dataBytes` in `captureFullState`) are retained under `juce::ignoreUnused` — `sp` still selects
which DSP pointer to serialize, so it is load-bearing; only its log use went.

The on-disk `Documents/BaySickDAW/pedals_state_log.txt` is deliberately NOT touched — Jeff's to
keep or delete. §B.28 QV-9 verifies it stops growing.

## Diagnostic Instrumentation Catalog

| Site | Tag | Purpose | Disposition |
|---|---|---|---|
| `BaySickPedalsProcessor.cpp` save/load path (8 sites) | (untagged, plain lines) | Bug-B round-trip forensics, added 2026-05-05 | **REMOVED this batch** (Jeff 2026-07-25, docket 6 log=A). Always-on Release disk I/O on the save/load path; the evidence had already served its purpose (1 failing restore vs 1,150+ clean cycles). |

- **NONE added this batch.** Nothing new to strip.

### Found along the way

1. **Routing-note watch items, unchanged and NOT in scope** (per the plan's Rule 3 note, logged
   for close-time routing; campaign §E exercises both surfaces): (a) NAM pedal state restores by
   ABSOLUTE path, so a moved/missing `.nam` file is a silent no-load; (b) per-DSP field
   completeness of the EQ / Tuner pedal serializers is unverified by desk-read.
2. **`kStateVersion` is written but never read.** `captureFullState` sets a `version` property
   and no reader anywhere checks it. Harmless, and arguably the right shape for a future format
   change — noted rather than removed, since deleting it would remove the only hook a real
   migration would use.
3. **Two stale comments referenced the deleted `pedalsLog`.** The grep the running notes promised
   found zero LIVE references but two comment mentions naming it as a logger-convention exemplar,
   in [ClipDropDiag.h:13](../../Source/ClipDropDiag.h:13) and
   [G3PlayheadDiag.h:13](../../Source/G3PlayheadDiag.h:13). **Fixed** — a comment pointing at a
   function that no longer exists is wrong, and wrong comments get fixed wherever they live
   (`feedback_no_docs_only_commit_fix_wrong_comments`); Rule 6's edited-regions scoping governs
   STYLE audits, not factual errors. `namirLog` still exists, so each now names only that.
   Caught because the drafter flagged that it could not verify the running notes' grep claim and
   I re-ran it rather than let the assertion stand.

## PENDING Main Plan edits — DEFERRED TO G4 CLOSE (Jeff, 2026-07-25)

> **Standing instruction, applies to every remaining G4 batch:** do NOT stop mid-run to edit
> `Main Plan.md`. Accumulate every needed edit here with enough detail to apply it cold, and land
> them all in one pass at the G4 boundary (after `clean-pointing-stoat`'s commit, alongside R3
> review + the carry-over block). Each remaining batch appends its own pending items to its own
> running-notes file under this same heading; the G4-close pass walks all of them.
>
> This is separate from the R2 held-entry mechanism — R2 defers the §5 STATUS flip + Work Log
> apply to each batch's CAMPAIGN section pass. The items below are plan-accuracy edits that do
> not wait on the campaign.

### From QA-ApvtsAutomation (`wired-lassoing-crane`, `bbb4e639`) — ALREADY APPLIED

- §5 + §6 bucket lines corrected in place to "Players, Effects, Cross-cutting Infrastructure"
  and the BLU-492 PRESET-BREAK marked void. **Done in the crane commit — nothing pending.**

### From QA-Verify (`sturdy-tagging-pangolin`, this batch) — PENDING

1. **Rule 3 fold — QA-Export gains the NAM finding.** §5's QA-Export entry Items list needs a
   line: *"+ missing external-file references (NAM pedal absolute `.nam` path, plus any IR /
   sample / rack-NAM sites the sweep finds) must fail VISIBLY — folded 2026-07-25 from QA-Verify
   close, Jeff pick option d."* The walrus plan file already carries the full task (Task 5); this
   is the §5 mirror only.
2. **§9 Forks entry** recording that fold: origin = QA-Verify close 2026-07-25; finding =
   NAM pedal keeps the model NAME on a missing file while not loading it, so the UI presents as
   loaded ([NAMPedalStyleDSP.cpp:247](../../Source/DSP/NAMPedalStyleDSP.cpp:247), :276-281);
   routing = Jeff picked QA-Export (option d) over QA-ProjectSave / a new §5 row / deferral, with
   my flag on record that QA-Export has no thematic relation; scope caveat = only the NAM-pedal
   path was verified, so the task sweeps first.
3. **QA-Verify bucket — Jeff's call, still unanswered.** §5's QA-Verify entry carries NO
   `**Bucket:**` line at all (unlike its neighbours), and §8's bucket table lists QA-Verify under
   **Players** + **Workflow Polish**. Workflow Polish belongs to the CAMPAIGN half (§E), not this
   code half — nothing shipped here is workflow polish. Held entry drafts **Players, Effects**.
   Needs: add a §5 Bucket line and/or correct the §8 row. **Do not apply without his answer.**

### From later G4 batches — append below as they close

- *(QA-Export / QA-ProjectSave / QA-UndoCoverage / QA-DirtyFlag: add pending items here.)*

## Held Work Log entry (apply at section pass)

> Drafted at code-complete via `/draft-doc batch-close`; NOT yet applied to `Implemented Work Log.md`.
> Applies with the §5 STATUS flip when §B.28 passes the campaign walk (R2). Backfill the commit hash
> (here and in the §B.28 `blocks:` line) at commit; stamp the full `HH:MM PT` at apply.

### 2026-07-25 <HH:MM> PT — QA-Verify (code half) — The founding premise was already void before Task 1 opened: the pedalboard-restore regression died with the 2026-05-05 base64 fix, and the always-on round-trip log showed ONE failing restore in its entire history against 1,150+ clean cycles. What shipped instead is state hygiene (docket 6=B + log A) — the BaySickPedals outer-wrapper / APVTS tag collision broken with a V2 root tag plus one-line legacy load tolerance and a type-derived child lookup; all 13 FX-rack `EffectType` head-block ordinals pinned explicit against silent slot repointing; the always-on Release save/load file logger stripped. Zero behavior change by design, so all nine §B.28 scenarios are regression guards. One plan self-contradiction resolved and one plan bullet turned out to be a false alarm

**Bucket:** Players, Effects. Batch `sturdy-tagging-pangolin`. `blocks:` `<hash>`.
*(Main Plan §5's QA-Verify entry carries no `**Bucket:**` line at all; §8's bucket table lists
QA-Verify under **Players** ("per-engine smoke") and **Workflow Polish** ("E-bucket walk"). Workflow
Polish belongs to the CAMPAIGN half (§E), not this code half — nothing here is workflow polish. The
shipped diff is `BaySickPedalsProcessor` (Players) plus the shared `EffectType` enum in
`Source/EffectRack.h`, which is the FX rack's own type registry (Effects); QA-NativeDialogs took
Effects for a smaller pedals touch the same week. Adding a §5 Bucket line and/or correcting the §8
row is Jeff's call, per the QA-NativeDialogs precedent.)*

#### Done

- **Premise — the batch's founding bug was already dead.** The QA-Inventory "pedalboard preset doesn't restore" regression was fixed 2026-05-05 by the base64 decode fix (`MemoryBlock::fromBase64Encoding` replacing raw `convertFromBase64`, which silently returned 0 bytes on the `<byteCount>.<base64>` prefix). Verified against the always-on round-trip log before any code was touched: exactly ONE failing restore event in the log's entire 1.3 MB history (six `decoded 0 bytes` lines sharing a single timestamp = one pre-fix restore with six populated slots), against 1,150+ clean cycles since. The round-trip walk (capture -> APVTS copy + per-slot base64 blobs -> restore) drops no persistent field, and the page-preset and project paths share that core. **Nothing in this batch fixes a live bug; every task closes a latent hazard.** Runtime confirmation of the preset system stays campaign §E, where it was locked 2026-07-08.
- **Task 1 — the collision was worse than "two things share a name".** `kStateRootTag` was `"BaySickPedalsState"` AND the APVTS is constructed with that exact type string (`BaySickPedalsProcessor.cpp` ctor initialiser), so any tag lookup starting ABOVE the pedals root could land on the wrapper instead of the parameter child. Restore worked only because `getChildWithName` happened to hit the APVTS child first — a coincidence of tree order, not a guarantee.
- **Task 1 — V2 root tag with one-line legacy load tolerance.** New saves write `kStateRootTag = "BaySickPedalsRoot"`; `kStateRootTagLegacy` retains `"BaySickPedalsState"`. `restoreFullState` accepts either with one `||`, and `loadPedalboardPreset` falls back to the legacy payload tag inside the `Pedalboard` preset envelope. This is load acceptance, not a migration system — `feedback_no_backward_compat_pre_v1` holds, no on-disk file is rewritten or converted, and Jeff's existing "Jeff 1.xml" plus every saved project keeps loading. Covered by §B.28 QV-1 and QV-3, both MUST-PASS.
- **Task 1 — the APVTS child lookup keys off `apvts.state.getType()`, stronger than the plan asked for.** The plan's checkbox offered "select the child that has PARAM children / matches `apvts.state.getType()`"; the type-derived form is exact rather than heuristic and cannot drift if the APVTS type is ever renamed. On legacy files, where the outer wrapper shares that same string, `getChildWithName` searches children only, so the correct child is still resolved. This is the part that actually removes the hazard — the tag rename alone would only have made the tree readable, not the lookup safe.
- **Task 1 — plan self-contradiction resolved (mine to resolve, no user-visible difference).** The plan's Context section specified outer tag `BaySickPedalboardRoot`; Task 1's checkbox specified `BaySickPedalsRoot`. Took the checkbox — it is the executable line. Flagged to Jeff in chat when it was hit rather than after.
- **Task 1 — the plan's third bullet was a FALSE ALARM; `InstPage` needed no change.** The plan asked whether the page-preset config keys on the outer tag. `InstPage.cpp:405` does set `slot.engineRootTag = "BaySickPedalsState"`, but that field is read ONLY inside the sfizz kit-path branch at `PagePresetIO.cpp:119-127`, which is gated on `slot.kitLoadCallback` — pedals never sets one. Pedals state flows through `setStateInformation` -> `restoreFullState`, which now accepts both tags. Editing it would have touched the sfizz path for zero benefit. **Left alone deliberately.**
- **Task 2 — all 13 `EffectType` head-block values pinned explicit** (`None = 0` .. `DeEsser = 12`) in `Source/EffectRack.h`, matching the previously-implicit ordinals exactly: zero behavior change, by construction. A Rule-6 category-6 comment records WHY — both FX-rack slots and pedal slots persist `type` as the raw int, so an insertion or re-order silently repoints every saved slot at a different effect (a Compressor comes back as a Reverb). The pedal-native block (`= 100`+) was already explicit; this closes the gap so the whole enum is now append-only-with-an-explicit-value. Covered by §B.28 QV-7 / QV-8.
- **Task 3 — `pedalsLog` stripped: the function plus all 8 call sites.** Two in `captureFullState`, four in `restoreFullState` (including the reject path and the per-slot trace), plus the two base64 decode traces. Grep confirms zero remaining live references. Two locals that survived only to feed the log (`sp`, `dataBytes` in `captureFullState`) are retained under `juce::ignoreUnused` — `sp` still selects which DSP pointer to serialize, so it is load-bearing; only its logging use went.
- **Task 3 — the on-disk log file is deliberately NOT touched.** `Documents/BaySickDAW/pedals_state_log.txt` stays for Jeff to keep or delete; §B.28 QV-9 verifies only that it stops growing.
- **Build.** Per-task gate at the end of every task, BOTH configs clean each time (`RELEASE_EXIT_CODE=0` / `DEBUG_EXIT_CODE=0`).
- **Master Test Plan §B.28 authored — 9 scenarios (QV-1..QV-9)**, reconciled against what shipped rather than transcribed from the plan's 6-item verify ladder: three were ADDED (QV-6 bypass-state round-trip, because bypass lives in the pedals parameter store and this batch changed how that child is located on load; QV-7 / QV-8 as the enum-pinning regression guards on FX rack and pedal slots respectively). Two MUST-PASS, both compatibility: **QV-1** (pre-batch pedalboard preset still loads) and **QV-3** (pre-batch PROJECT with pedals restores). The section carries a scope note recording the void premise, so the campaign walker knows every scenario is a guard and not a new capability, and directs the walk at real pre-batch files rather than freshly-made ones.

#### Found along the way

1. **Routing-note watch items, unchanged and NOT in scope** (carried from the scout via the plan's Rule 3 note): (a) NAM pedal state restores by **absolute path**, so a moved or missing `.nam` file is a silent no-load; (b) per-DSP field completeness of the EQ and Tuner pedal serializers is unverified by desk-read.
2. **`kStateVersion` is written but never read.** `captureFullState` sets a `version` property and `savePedalboardPreset` sets a `version` attribute; no reader anywhere in the tree checks either. Harmless, and arguably the right shape to leave in place for a future format change.
3. **`InstPage`'s `slot.engineRootTag = "BaySickPedalsState"` is now arguably misleading** — post-Task-1 it names the APVTS type rather than the blob root. It is inert for this engine (see the Task 1 false-alarm bullet), so it reads as a stale label rather than a bug.
4. **Two stale comments referenced the deleted `pedalsLog`** (`ClipDropDiag.h:13`, `G3PlayheadDiag.h:13`), naming it as a logger-convention exemplar.

#### What was done about each finding

- **Finding 1: ROUTED at close (Jeff, 2026-07-25) — the two items split.**
  - **(a) NAM pedal absolute-path restore — gets a real fix on its own merits; does NOT ride §E.**
    Desk-verified before routing rather than relayed from the scout: the pedal stores the absolute
    `.nam` path ([NAMPedalStyleDSP.cpp:247](../../Source/DSP/NAMPedalStyleDSP.cpp:247)), and on a
    missing file it KEEPS the remembered name for the editor label, does not load the model, and
    emits nothing ([:276-281](../../Source/DSP/NAMPedalStyleDSP.cpp:276)). The failure is
    actively misleading rather than merely silent — the UI shows a model name it did not load, so
    the user has no signal that amp modeling is absent. Move the captures folder, rename a file,
    or open the project on another machine and it presents as loaded. **The app already has the
    correct pattern for this exact case:** the sfizz kit path in
    [PagePresetIO.cpp:128-131](../../Source/Standalone/PagePresetIO.cpp:128) pops a warning
    dialog when its file is missing. NAM simply does not use it.
    **SLOT: QA-Export (`loud-bouncing-walrus`), G4 batch 5 — Jeff's pick (option d), 2026-07-25.**
    I flagged when posing the docket that QA-Export has no thematic relation to external-file
    restore and that folding it there widens the batch; Jeff picked it regardless, so it lands
    there in full. Rule 3 fold: the walrus plan file gains a task; Main Plan §5's QA-Export Items
    + a §9 Forks entry record the addition.
    **Scope caveat carried into walrus:** only the NAM PEDAL path was desk-verified. IR files and
    sample references may share the identical weakness — unchecked. The walrus task therefore
    opens with a sweep of the other external-file reference sites before fixing, so the batch
    either fixes the pattern or records precisely which sites were out of scope.
  - **(b) EQ / Tuner serializer field completeness — rides campaign §E as-is.** No new batch, no
    §9 entry. Stated precisely so the campaign walker is not hunting a phantom: there is **no
    evidence any of these DSPs drops a field**. The item exists because completeness was never
    verified, not because a round-trip failed. §E's per-family preset walk is exactly the test
    that would surface it.
- **Finding 2: noted, no code.** Removing it would delete the only hook a real format migration would use, and leaving it costs one property per save.
- **Finding 3: left alone deliberately.** The region is out of this batch's scope and the field is inert for pedals, so Rule 6 scoping applies (edited regions only, never a whole-file audit). Changing the literal would have touched the sfizz page-preset path for no functional gain.
- **Finding 4: FIXED.** A comment pointing at a function that no longer exists is a factual error, and those get fixed wherever they live — Rule 6's edited-regions scoping governs STYLE audits, not wrong statements. Each now names only `namirLog`, which still exists.

#### Group review (R3)

- **Pending — runs at the G4 boundary** (after `clean-pointing-stoat`'s commit) over the group's combined diff.

#### Diagnostic Instrumentation Catalog

| Site | Tag | Purpose | Disposition |
|---|---|---|---|
| `BaySickPedalsProcessor.cpp` save/load path (8 sites) | (untagged, plain lines) | Bug-B round-trip forensics, added 2026-05-05 | **REMOVED this batch** (Jeff 2026-07-25, docket 6 log=A). Always-on Release disk I/O on the save/load path; the evidence had already served its purpose (1 failing restore vs 1,150+ clean cycles). The on-disk `pedals_state_log.txt` is left for Jeff to delete. |

- **NONE added this batch.** No `DBG` / `juce::Logger` / temp `jassert` / debug `AlertWindow` / temp-file trace added — every task was resolved by reading code plus the per-task compile gates. Nothing new to strip.

#### Carry-forward contradictions

- None. The plan's Carry-Forward touch points were "none" and that held: message-thread state IO only, no DSP, no audio-thread surface, no routing. Two notes to carry: (1) BaySickPedals state now writes outer tag `BaySickPedalsRoot` with `BaySickPedalsState` accepted on load, and the APVTS child is located by `apvts.state.getType()` rather than a hard-coded string; (2) `EffectType` ordinals are now pinned explicitly and are persisted as raw ints by BOTH the FX rack and pedal slots — append only, never reorder or insert.

#### Commit(s)

`<hash>` (whole batch — Tasks 1-3 + §B.28 + the QA-ApvtsAutomation hash backfill + held entry + running notes; single batch commit per the bulk-run model). Preceded by QA-ApvtsAutomation `bbb4e639`. Build clean in BOTH configs at every task gate; behavioral verification deferred to the R2 campaign pass against §B.28.

#### Next action

- Proceed to **QA-Export** ([`loud-bouncing-walrus.md`](../Batch%20Plans/loud-bouncing-walrus.md)), G4 batch 5 of 8.
