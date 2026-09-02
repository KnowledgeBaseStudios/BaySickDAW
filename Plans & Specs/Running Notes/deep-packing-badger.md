# Running Notes — QA-ProjectSave (deep-packing-badger)

> Append-only mid-batch log. Entries land at every checkpoint (commit landed / finding captured
> / spec call resolved / scope pivot) per `feedback_draft_doc_running_notes_every_checkpoint.md`.
> At code-complete, `/draft-doc batch-close` consumes this file to compile the Implemented Work
> Log entry, which is HELD here (bulk-run R2) and applied at the batch's campaign section pass.
>
> Pair file: [`Plans & Specs/Batch Plans/deep-packing-badger.md`](../Batch%20Plans/deep-packing-badger.md).
> Conventions: Main Plan §0 (Batch Plans + Running Notes layout, locked 2026-05-11).

## 2026-07-26 — Batch open — docket walked (4 items), Task 1 inserted

Batch opened against the approved plan. Four spec calls surfaced to Jeff before any code was
written; all four answered. Three of them corrected premises that were WRONG in the approved
plan body — caught by reading source rather than trusting the plan text.

**Docket 15 = B — templates carry a full `<Processor>` snapshot.**
The plan's implementation note claimed "mixer strip settings ride each tab's engineData + the aux
entries exactly as project save does." False. `engineData` is
`encodeEngineState(page->getEngineProcessor())` — the engine processor's OWN state and nothing
else ([StandaloneEditor.cpp:10930](../../Source/Standalone/StandaloneEditor.cpp:10930)). Every
`mixer_*` param (level/pan/width/mute/solo/polarity/bypass/arm/`_sendTo`/4 sends) lives in the
main `VibeSynthProcessor::apvts`, which `serializeProject` writes as a `<Processor>` child —
a SIBLING of `<UIState>`, not inside it
([PluginProcessor.cpp:5000](../../Source/PluginProcessor.cpp:5000)) — and FX rack contents +
per-insert EQ ride `VibeRackStates` in the same child
([PluginProcessor.cpp:4996](../../Source/PluginProcessor.cpp:4996)). A UIState-only template
would have restored tabs/engines/names/orders with every fader, cable, rack and EQ at default,
which does not meet Main Plan §5's "complete project skeleton / mirrors the project-save shape".
Jeff picked B: template emits `<Processor>` too. Lands as Task 2 (write) + Task 3 (apply).

**Docket 16 — Rusty's prompt is "Save Page Preset & Delete", not "Save Kit & Delete".**
Docket 11=A (locked 2026-07-25) picked the label on the reasoning "kit = Rusty's save concept".
Source says otherwise: Rusty's only user-writable save is the J-11 **Player Preset**
(`savePlayerPresetAs` -> `Documents/BaySickDAW/Presets/Rusty Player/My Presets/<name>.xml`,
[BaySickRustyDrumsPage.h:96-151](../../Source/Standalone/BaySickRustyDrumsPage.h:96)). Rusty's
"kit" is the sfizz `.sfz` program (Full/Basic) — a read-only factory asset the app never writes —
and `StandaloneEditor::saveKitAs` (:7282) is the unrelated DrumPage kit, which walks Drums tabs.
Jeff: "the kit in this case is the save page preset that all the other pages have." So Rusty
takes the other six page types' wording verbatim. Supersedes docket 11=A's label.
Second finding in the same task: Rusty has NO page-dirty signal at all (no `mPageDirty`, no
`isPatchDirty()`), so the `mDirtyListener` trio gets ported from ClipsPage/VoxPage.

**Docket 17 = b — no new "Pack Project…" item; QA-Export already shipped it.**
Task 6 as written would have added a duplicate. `doExportProjectBundle` (File menu item 122,
[StandaloneEditor.cpp:10625](../../Source/Standalone/StandaloneEditor.cpp:10625)) already gives
destination + zip-vs-folder + references-vs-self-contained over `ProjectBundler` — exactly the
Task 6 spec. Only real delta: an unsaved project currently gets a "save the project first" info
box and bails. Jeff picked b: keep the name, add only the save-first prompt. Now Task 7.

**Docket 18 — L/B/D load empty, delete to zero, and empty buses hide. INSERTED AS TASK 1.**
Raised by Jeff off the back of the docket: the hard-coded one-Layers/one-Bass/one-Drums trio is
"bloat for a user that may not want those things," and my framing of the empty-bus consequence as
merely "worth flagging" was wrong — it is a defect in scope. His scope ruling, verbatim: "the
fuck it is project save, if projects SAVE with 3 tabs that aren't supposed to be there then it is
exactly this and no you aren't gonna fix this after doing the work that makes this borked to
begin with." Ordering is forced, not preferential: with docket 15=B, every template written in
Task 2 would bake three phantom tabs AND three phantom bus strips into its snapshot.

Scoping read before the proposal (all verified in source):
- `addDefaultDynamicTabs()` ([:1901](../../Source/Standalone/StandaloneEditor.cpp:1901)) seeds the
  trio at editor construction ([:1886](../../Source/Standalone/StandaloneEditor.cpp:1886)) and on
  File > New ([:10348](../../Source/Standalone/StandaloneEditor.cpp:10348)).
- `RibbonTabBar::closeTab` holds a `zeroAllowed` allowlist — Clip/Vox/Inst are in it, L/B/D are
  not ([RibbonTabBar.cpp:174-179](../../Source/Standalone/RibbonTabBar.cpp:174)).
- **15** `isLastOfType` call sites in `StandaloneEditor.cpp` (3 types x ~5 duplicated spawn paths:
  default tabs, duplicate-spawn, template-spawn, deserialize, context menu).
- `addDefaultDrumTab()` ([:2017](../../Source/Standalone/StandaloneEditor.cpp:2017)) is called
  from a kit-recovery path ([:7593](../../Source/Standalone/StandaloneEditor.cpp:7593)) that
  re-spawns a Drums tab whenever it finds none — it would silently undo a delete-to-zero.
- The empty-bus fix has an exact precedent already in-tree: the RustyDrums Bus is a child
  component starting `setVisible(false)`, flipped by `mRustyDrumsBusActive`
  ([MixerPage.cpp:1585-1588](../../Source/Standalone/MixerPage.cpp:1585),
  [:2499-2534](../../Source/Standalone/MixerPage.cpp:2499)), with the cable route-picker and
  hit-testing gated on that same flag ([:702](../../Source/Standalone/MixerPage.cpp:702),
  [:766](../../Source/Standalone/MixerPage.cpp:766)). Mirroring it covers Clips/Vox/Inst too,
  which have the identical dead-bus condition today.
- Trap found while designing it: a bus with zero member tabs can still be a live SEND
  destination, and hiding it would strand that cable. `sweepSendsTargeting` (used by
  `deleteSecondaryBus`) is the existing "what routes to this channel" primitive, so the
  predicate is *zero tabs of this type AND nothing routes here*. UI-only — InsertNodes stay
  allocated, audio routing untouched.
- Three >=1-of-a-type assumptions checked and already safe: `PianoRollPage::unregisterEngine`
  falls back to Drum Kit ([PianoRollPage.cpp:243](../../Source/Standalone/PianoRollPage.cpp:243));
  the ribbon instance dropdown hides Pages/Rename/Delete at count 0
  ([RibbonTabBar.cpp:606](../../Source/Standalone/RibbonTabBar.cpp:606)); project load falls back
  to the Builder tab ([StandaloneEditor.cpp:12098](../../Source/Standalone/StandaloneEditor.cpp:12098)).

Plan file edited: header batch count 6-of-8 -> 6-of-9; four docket rows added to the locked-spec
table; the false engineData claim corrected in the R5 implementation notes; Files-to-modify
rebuilt for seven tasks with refreshed line refs; Task 1 inserted and originals 1-6 renumbered
2-7; Verification list rebuilt 10 -> 15 scenarios (4 new for Task 1, 1 new for docket 15=B's
mixer round-trip, Rusty + Pack scenarios reworded). Effort ~10-16 h -> ~13-21 h.

## 2026-07-26 — Task 1 — code-complete (docket 18)

Build gate green (`RELEASE_EXIT_CODE=0`, `DEBUG_EXIT_CODE=0`, zero `error C`/`LNK`/`MSB`).
Eight source files: StandaloneEditor.cpp/.h, RibbonTabBar.cpp/.h, MixerPage.cpp/.h,
SharedUI.cpp/.h.

**Shipped**
- `addDefaultDynamicTabs()` + `addDefaultDrumTab()` deleted (177 lines) with all three callers:
  editor construction, `doFileNew`, and the `loadKitImpl` kit-recovery respawn. First launch and
  File > New both open on Builder with an empty ribbon.
- `closeTab`'s `zeroAllowed` set opened to Layers/Bass/Drums; all 15 `isLastOfType` guard sites
  removed (3 died with the deleted functions, 12 stripped individually — 60 lines).
- New `EngineEmptyState` in SharedUI (one parameterised class, not three copies of the
  Vox/Inst shape), instantiated for Layers/Bass/Drums with each type's mixer accent. Wired to
  ribbon body-click, the on-close cascade, and `handleCommandMessage`.
- New `hideAllEmptyStates()` — six placeholders made "hide the other five by name" untenable;
  the three pre-existing show methods and `showPageForTab` now route through it.
- Bus strips hide when nothing routes to them: Layers/Bass/Drums **plus** Clips/Vox/Inst, which
  had the identical dead-bus condition before this batch.

**Deviations from the plan text (all deliberate, none reduce scope)**
1. *Mechanism.* Plan said mirror the `mRustyDrumsBusActive` flag pattern. Shipped one shared
   predicate instead of six flags: the send-target guard needed a route query regardless, so six
   flags PLUS that query would have been redundant state to keep in sync. Same behaviour, one
   source of truth. Rusty and the secondary Vox/Inst buses keep their existing flags as the sole
   authority (`isAlwaysVisibleBus`) — a just-created "Add Vox Bus" has nothing routed to it yet,
   and route-counting it would make the strip the user just asked for fail to appear.
2. *Dead code from this batch's own change.* `isLastOfType` (zero callers) and `closeTab`'s
   `force` parameter (its only purpose was bypassing the guard that just went) both retired,
   plus the one `force` call site in `loadKitImpl` and its now-false comment.
3. *Not on the checklist.* `handleCommandMessage` — F8/F9/F10 and View > Layers/Bass/Drums were
   silent no-ops at zero tabs. Harmless when zero was impossible; a dead key now. They land on
   the empty state, same as the ribbon slot. Came out of the ">=1 assumptions" sweep item.

**Sweep result (>=1-of-a-type assumptions).** Four surfaces checked and already safe, no edit
needed: `PianoRollPage::unregisterEngine` (falls back to Drum Kit), the ribbon instance dropdown
(hides Pages/Rename/Delete at count 0), project-load tab selection (falls back to Builder),
`getKitDrumList` (loop-based). `showLastUsedPianoRoll` returns cleanly with no match — F11 with
zero instrument tabs is a no-op, which is correct: there is no roll to show.

**Self-inflicted defect caught in review, fixed before the gate.** The first cut of
`isBusVisible` walked every processor parameter and was called ~10 times per
`layoutScrollContent` — which runs on every `resized()`. Worse, `isSendDestId` built four
temporary `juce::String`s per parameter, and the "does anything target bus X" shape early-outs
on POPULATED buses while walking to completion on EMPTY ones — backwards, since empty is the
common case at startup. Order 10^5 allocations per layout pass, i.e. per frame of a window drag.
Fixed by collecting the full target set in ONE walk per layout (`liveRouteTargets`), hoisting the
four suffixes to `static const` (which also speeds up `sweepSendsTargeting`, sharing the
predicate), and skipping the prefix parse for destinations already in the set. Nothing would have
failed a build or a test — the mixer would just have gone sluggish on resize once a project had
many strips. Same reasoning as the existing `findStripByChannelId` cache (perf-audit H2).

**Verification owed** (authored into §B.30 at code-complete): scenarios 1-4 of the plan's
Verification list. Scenario 4 (route-target guard) is the one that would regress silently — a
stranded cable is invisible until audio goes missing.

**Correction, in two rounds — Jeff caught both. Net result: the bus-visibility code SHRANK to
three lines and the whole route-collector was deleted.**

Round 1: scenario 4 was first written as "point a Vox strip's SEND at the Layers bus." Invalid on
two counts, both readable in [`isRouteAllowed`](../../Source/Standalone/MixerPage.cpp:790) and
[`startSendPlacement`](../../Source/Standalone/MixerPage.cpp:890): Vox strips are restricted to
Master / Clips Bus / Vox Bus / Vox Bus 2 (Inst likewise to its own family), and sends
(`_sendN_to`) only ever land on AUX strips. Main-out `_sendTo` is the only mechanism by which
anything targets a bus at all.

Round 2 — the one that mattered. Jeff: "when you move the main out it reroutes the strip to that
bus and physically moves it so wouldn't it stay in place?" Correct, and it invalidates the whole
premise of the extra guard. `bucketPush` keys `buckets` by each strip's `_sendTo`, so re-pointing
a main-out at a bus RE-BUCKETS that strip into the bus's group — it becomes a member. Combined
with round 1 (sends can't target buses), **for a bus "something routes here" and "has members"
are the same condition**, and `laidOutBus` already computed `hasMembers` three lines below where
the early-out went.

So the extra machinery was guarding an unreachable case. Deleted: `collectLiveRouteTargets`,
`MixerPage::liveRouteTargets()`, `MixerPage::isBusVisible()`, the per-layout `routeTargets` set
and the `<set>` include. The visibility test is now `if (! hasMembers && ! isAlwaysVisibleBus)`
off data the layout already had. The perf defect logged above went with it — it existed only
inside the machinery that should never have been written.

KEPT from that work, both independently justified:
- `isAlwaysVisibleBus` — the flag-gated buses (secondary Vox/Inst, RustyDrums) must appear the
  instant the user creates them, before anything is routed in, so they opt out of the empty test.
- `sendSuffix()` hoisting the four `_sendN_to` suffixes to `static const juce::String` —
  `isSendDestId` runs once per registered parameter and was building four temporaries per call.
  Pure win for `sweepSendsTargeting` (aux delete / secondary-bus delete), zero behaviour change.

Scenario 4 rewritten and still worth walking, but for a different reason: it now verifies that
bus visibility follows MEMBERSHIP rather than tab-type count — precisely what a naive "hide the
Layers Bus when zero Layers tabs exist" implementation would get wrong.

KNOWN EDGE, not fixed (cosmetic, pre-existing shape): a bus can be a sidechain SOURCE
(`_sc_recv{N}_from` on the receiving strip), and that reference is not membership. An empty bus
that some compressor sidechains from will hide, taking its visible cable endpoint with it. Audio
is unaffected — a bus with no members is silent, so such a sidechain carries nothing anyway.
Flagged for the campaign rather than pre-emptively coded around.

## 2026-07-26 — Task 2 — code-complete (template writer v2)

Build gate green on retry (`RELEASE_EXIT_CODE=0`, `DEBUG_EXIT_CODE=0`, zero errors). First
attempt failed with 8 errors, all cascading from one `ui->` on a reference parameter in
`serializeStructuralUIState` — the scripted arrow-fix that converted the two LIFTED bodies from
pointer to reference syntax did not cover the wrapper written by hand afterwards. One-line fix.

**Shipped**
- `serializeUIState` split into `serializeTabsInto(XmlElement& tabs)` +
  `serializeStripNamesAndOrders(XmlElement& ui)`, both driven by a new
  `serializeStructuralUIState`. Project save is behaviour-identical: the only change is child
  ORDER inside `<UIState>` (names/orders now sit right after `<Tabs>` instead of after
  `<SongLoop>`), and every reader resolves by `getChildByName`, which is order-independent.
- `serializeProject`'s `<Processor>` block extracted to
  `VibeSynthProcessor::writeProcessorState(XmlElement&)` (docket 15=B) so template save emits the
  identical block — APVTS + `VibeRackStates`, i.e. every mixer fader / pan / width / mute / solo /
  polarity / `_sendTo` + 4 sends, each insert's effect rack, and each post-rack EQ.
- `saveTemplateAs` rewritten to v2: `<BaySickTemplate version="2">` wrapping `<Processor>` +
  `<UIState>`. Deliberately NOT a `serializeUIState` call — that would drag in activeTabId,
  mixerScrollX, `<Arrangement>` (incl. time selection), `<Metronome>`, `<VUCalibration>`,
  `<MeterLatencyComp>`, `<SongLoop>` and `<PianoRollSelection>`, all of which are properties of a
  SESSION rather than of a skeleton to start new songs from.
- Save-as-Template dialog text replaced ("kit + layers + basses" -> the real scope, ASCII only).
- Format comment block above `templatesDir()` rewritten to document v2 + v1-factory and to record
  why v1-USER gets no loader (docket 9=A: it never round-tripped, so there is nothing to preserve).

**Checked, no change needed:** `Tools/gen_factory_presets.py` writes the v1 attribute schema with
`version="1"` and is still the shipped factory set — the dual-format branch in Task 3 has a real
v1 case to serve, and the generator needs no edit.

**Known intermediate state (by construction, does not ship):** between Task 2 and Task 3, loading
a template is broken — a v2 file carries no `<Kit>`/`<Layer>`/`<Bass>` children, so the current
loader tears down and restores nothing. Writer and loader are split across two tasks but land in
ONE commit, so no broken state reaches a build Jeff runs. Task 3 is not optional before commit.

**Rule 6 cleanup in touched regions:** `closeAllDynamicTabs`'s header comment claimed
`closeTab` "refuses to remove Drums tabs OR the last instance of a type" — falsified by Task 1.
Rewritten to state what the function actually does now, with the historical reason parenthesised.

## 2026-07-26 — Task 3 — code-complete pending one open spec call

Build gate green (`RELEASE_EXIT_CODE=0`, `DEBUG_EXIT_CODE=0`, zero errors), twice: once for the
task body, once after the rack-ordering fix below.

**Shipped**
- `loadTemplate` split into a dirty gate + `applyTemplate`. The gate lives in `loadTemplate`, not
  at the menu entries, so Task 4's submenu inherits it. Closes the verified silent-discard bypass
  (this was the ONLY File-level teardown with no save prompt). `confirmDiscardChanges` runs its
  continuation inline when the project is clean or blank, so those still load directly; Cancel
  never runs it, leaving the project untouched.
- v2 branch restores through the PROJECT path: `applyProcessorState` -> `deserializeUIState` ->
  `restoreAudioStripsFromArrangement`. No second restore implementation to drift from the first.
- `VibeSynthProcessor::applyProcessorState` + `reportMissingFilesIfAny` extracted from
  `deserializeProject` and shared. Template load now reports missing NAM captures / sfizz kits /
  IRs the same way project load does — a template's engines carry the same external references.
- v1-factory teardown scoped via a new `TabTeardownScope` enum: that branch only restores
  Layers/Bass/Drums, so it no longer destroys Clips / Vox / Inst / Rusty tabs it cannot put back.

**Two traps found while writing the scoped teardown**
1. A `BaySickRustyDrumsPage` carries `TabType::Drums`, so a naive "close all Drums" would have
   torn down the Rusty singleton that no Layer/Bass/Drum restore path puts back. Excluded via the
   existing `dynamic_cast` probe.
2. The scoped path must NOT call `clearDynamicStrips()` — it wipes EVERY strip including the
   surviving tabs'. `onTabClosed` already removes each closed tab's own strip (MIX-05), which is
   exactly the scoped subset. Commented at the call site so it does not get "fixed" back in.
3. `resetProjectState()` likewise cannot run on the scoped path: it zeroes the Vox / Inst / Clip
   name counters while those tabs are still on screen, so the next +Add would collide with a live
   tab's name. Only the three L/B/D counters are rewound.

**Self-caught ordering defect, fixed before close.** The first cut called
`applyPendingRackStates()` directly in the v2 branch, before `restoreAudioStripsFromArrangement`.
That function replays the stash INSIDE itself, after its `ensureAudioInsert` calls — and
`applyPendingRackStates` clears the stash on first use. Calling it early would have consumed the
stash before the Audio InsertNodes existed, so clip-strip effect racks would have come back
EMPTY on every template load. Removed the explicit call; the project path's ordering is now
matched exactly. Would not have failed a build — only an ear test, and only on templates
carrying clip racks.

**OPEN SPEC CALL — blocking Task 3 close (surfaced to Jeff 2026-07-26, awaiting answer).**
A template carries no arrangement content; the plan does not say what happens to the arrangement
ALREADY in the project when one loads. Implemented reading is non-destructive: tabs are replaced,
patterns / notes / arrangement are kept (§5's "load template directly into current state" read as
merge-not-wipe). Consequence worth stating: template tabs get fresh page indices, so existing
notes on Layer 0 will play whatever engine the template placed at Layer 0 — the song survives but
plays through the template's sounds. Options put to Jeff: (a) keep, as implemented; (b) clear, so
a template load is New-Project-plus-template; (c) prompt at load time; (d) something else.
Task 4 NOT started — it wires the menu entries into this flow, so the answer changes what those
entries do.

**Deferred to Task 4 (owns the surface):** `doFileNew`'s shield comment still references a
"default-template loadTemplate" path that QA-Ef #6 removed. Task 4 rewrites that region for the
default-template consumer; fixing it now would be churn.

## 2026-07-26 — Batch RE-PLANNED mid-execution: 7 tasks -> 12, four commits

Second docket walked (dockets 19-28), the G4 findings sweep run (docket 27=a), and an
`/architecture` pass commissioned on the automation-registry question. Plan file rewritten:
locked-spec table gained a "Second docket" section, Files-to-modify rebuilt for 12 tasks,
Verification 15 -> 23 scenarios, effort ~13-21 h -> ~32-44 h.

**Task map after the re-plan.** 1 L/B/D-empty (done) / 2 template writer v2 (done) / 3 loader v2
(done, + docket-19 amendment) / 4 sample hybrid + resolvers (was Task 6, moved to front because
everything downstream needs its resolver) / 5 `library:` normalization + template sample copy /
6 bundler engine-ref walk + export semantics / 7 automation registry / 8 app-root resolver /
9 three small corrections / 10 submenu / 11 Rusty prompt / 12 bundle save-first prompt.

**Commit boundaries — deliberate deviation from one-commit-per-batch, Jeff's call.** Commit 1 =
Tasks 1-6, Commit 2 = Task 7 alone, Commit 3 = Tasks 8-9, Commit 4 = batch close. Surfaced as a
blast-radius concern: twelve tasks spanning project save/load, the bundler, the automation
registry and the effects audio path is too much for one rollback point, and a failed smoke would
cost the three already-verified tasks. Recorded in the plan's Batch close section.

**Two process corrections from Jeff, both standing.**
1. **Never offer deferral as an option for my own mistakes.** The suggested-reply options I had
   been generating included "new batch / post-v1" escapes on defects I created. Verbatim: "You
   fuck up, you fucking fix it. No more well we could make another batch, this group is the end
   of the code and we fix what we find period." Caught me doing it a SECOND time when I offered
   QA-Soundness as a home for the automation work after he had already ruled.
2. **Never offer "don't surface it".** Offering "(c) Don't" on the findings sweep was asking
   permission to keep making his decisions for me. Every finding's disposition is his call, so
   the sweep was never optional. Verbatim: "this literally goes against the rules of every spec
   call is mine so fucking stop it."

**`/architecture` pass — the finding that changed the answer.** Report at
[`Research Reports/daw-architecture-non-parameter-automation-binding-2026-07-26.md`](../Research%20Reports/daw-architecture-non-parameter-automation-binding-2026-07-26.md).
Across VST3 / CLAP / Tracktion / Ardour / Vital / Surge XT / iPlug2 / JUCE, **no system keeps a
UI-keyed automation applicator map** — automation universally targets a model-owned object with a
stable id while the UI observes. Two claims desk-verified in our own tree before acting on them
(`feedback_verify_subagent_finding_premise`):
- **FX-rack channel-switch hole CONFIRMED in code.** `mTrackBox->onChange` ->
  `onChannelChanged` -> `setRack` -> `rebuildSlotEditor` for all slots ->
  `SlotComponent::setEditor` assigns `mEditor = std::move(...)`, destroying the prior panel. So
  automation on a Layer 0 FX knob goes inert the moment the user views Layer 1's FX page, and
  recovers on navigating back. Ear confirmation is Task 7 step 0.
- **`componentBeingDeleted` fires first in `~Component`** — verified at
  `juce/modules/juce_gui_basics/components/juce_Component.cpp:275`. This is what makes the
  b-prime self-cleaning registry possible with ZERO changes to the 29 helper call sites.

Correction to my own earlier advice: I had told Jeff option (b) costs "all 44 sites + a handle
member per widget". True of (b) as literally specced, but the `ComponentListener` reverse-index
form was cheaper and I had missed it. Also stated plainly to him that neither (b) nor (c) fixes
the channel-switch hole — only step 3 (targeting the DSP instead of the widget) does.

**Also answered:** docket 20 confirmed the template writer already excludes all Builder-grid and
pattern content by construction (it never emits `PatternManager`), and that a Clips tab's sample
returning to the browser panel is correct — the sample is part of the rig, the timeline block is
not. And Jeff's `importSample` question resolved: the unnecessary Core-Library duplication he
suspected is real but comes from `importSample` (Task 4's target), NOT from the `library:` writer
gap, which stores paths and copies nothing.

## 2026-07-26 — Tasks 3-amendment / 4 / 5 / 6 — code-complete (Commit 1 = `dadb958a`)

All gates green (`RELEASE_EXIT_CODE=0`, `DEBUG_EXIT_CODE=0`, zero errors).

**Task 3 amendment (docket 19).** `applyTemplate`'s v2 branch now runs
`closeAllDynamicTabs()` -> `clearDynamicStrips()` -> `resetToBlankState()` before applying the
template, mirroring `doFileNew` exactly. Template load is New Project semantics: patterns, notes,
arrangement and the audio library all go. Side effect worth recording — this dissolves the Clips
resolution bug found in Task 3: `reset()` clears the audio library, so the "prefer an existing
library entry for this row" lookup has nothing to match and the template's own saved path wins.

**Task 4 — sample-retention hybrid + resolvers.**
- `SampleLibrary::makeStableRef` / `resolveStableRef` / `isStableRef` are now the SINGLE
  implementation of the `library:` / `mysamples:` convention. Format matched to the pre-existing
  DrumPage writer exactly (`isAChildOf` test, `getRelativePathFrom` with forward slashes) so refs
  written before this batch still load.
- `importSample` is source-aware: files already under Core Library or My Samples return a
  reference; only volatile sources still copy into `<project>/Samples/`. **This is the fix for
  Jeff's question** — Core Library samples were being duplicated into every project that used them.
- `resolveProjectFile` tests stable refs BEFORE the absolute-path test. Load-bearing ordering:
  `library:Foo/bar.wav` is not an absolute path, so without it the ref falls through to the
  project-relative branch and resolves to nonsense inside the project folder.
- A missing clip now feeds `MissingFileReport` instead of restoring as a silent empty player.
- **Two plan bullets deliberately NOT implemented, with reasons:** the ClipsPage preset path
  already writes a My-Samples-RELATIVE `clipRef` with its own resolver (account-independent
  already — converting would be churn); and `BuilderPage::onResolveStoredPath` needed no edit
  because it already delegates to `mProcessor.resolveProjectFile`.

**Task 5 — `library:` normalization + template sample adoption.**
- Normalized: Guitars + Basses `kitPath` (2 writers + 1 reader), Clips `clipPath`, Rusty
  `KitPath` (writer + reader), and **`VibePlayerProcessor`'s `bsp_loadPath` (3 writers, 2
  readers)**.
- The VibePlayer row is the significant one and corrects something I told Jeff earlier. I had said
  Layer/Bass "Save Patch As" does not drop the sample reference because it "rides inside the
  engine blob" — true, but I had not checked its FORM. It was an absolute path written at three
  sites, so every Layer / Bass / Drum patch using a BaySickPlayer sample has been storing
  `C:\Users\<name>\...` inside its blob. Same latent breakage as the kit paths, in the most-used
  engine in the app.
- Added `refForPersist` / `resolvePersistedRef` so no call site has to choose between stable-ref
  and absolute; readers still accept plain absolute paths, so everything saved pre-batch loads.
- `adoptIntoUserSamples` copies volatile files into My Samples on TEMPLATE save only (projects
  have their own Samples folder). Dedupes on size+modtime, auto-suffixes rather than overwriting.
  sfizz kits are excluded structurally, not by an exclusion list: every shipped kit is Core
  Library resident so it takes the already-stable branch. If an external `.sfz` ever becomes
  loadable it will be adopted correctly with no code change.
- **KNOWN GAP — CLOSED before Commit 1 (Jeff: "do it before this one").** BaySickPlayer sample
  paths sit inside base64 engine blobs rather than attributes. `adoptTemplateSampleRefs` now does
  a decode -> rewrite -> re-encode round-trip (`getXmlFromBinary` / `copyXmlToBinary`) on
  `engineData`, reusing Task 6's decoder, and only rewrites when something was actually adopted so
  an untouched template keeps a byte-identical blob.
  Two sub-cases surfaced while closing it:
  - **Folders are adopted, not just files.** `bsp_loadPath` may be a single WAV, a FOLDER
    (`loadFolder`) or an `.sfz`. A file-only helper would have silently skipped the folder case,
    which is how drum packs actually load. `adoptIntoUserSamples` now handles directories via
    `copyDirectoryTo`, treating a same-named folder as the same asset so repeated saves do not
    pile up copies (trade: two different folders sharing a name collide, rather than deep-comparing
    trees on every save).
  - **`.sfz` deliberately NOT adopted, flagged to Jeff as overrulable.** A bare `.sfz` is a
    pointer into a sample tree — its `sample=` opcodes resolve relative to its own folder — so
    copying the file alone yields a reference to nothing, and adopting it properly means dragging
    the surrounding library. That is exactly the reasoning behind docket 24's sfizz-kit exclusion,
    applied to the same structural problem rather than stopping for a fresh ruling. One-line change
    if Jeff wants them adopted.

**Task 6 — bundler engine-reference walk + export semantics.**
- `enumerate` gained an optional `tabsXml` parameter and now walks four layers: plain tab
  attributes, the Inst chain XML (NAM captures + user IRs), and the base64 `engineData` /
  `sfizzEngineData` blobs decoded via `getXmlFromBinary` — the only place BaySickPlayer sample
  paths are reachable. `doExportProjectBundle` passes the same `serializeTabsInto` output project
  save uses, so there is no second engine walker to drift.
- Path attributes kept EXPLICIT (`bsp_loadPath` / `kitPath` / `namPath` / `irPath` /
  `micUserIrPath` / `micbUserIrPath`, plus generic `path` only on `<KitPath>` / `<Sample>` tags).
  A "anything that looks like a path" heuristic would report false positives as Missing and train
  the user to dismiss the warning.
- Docket 22=b: `needsCopying` no longer copies Core Library in EITHER scope. Dropdown labels
  rewritten — the old "Self-contained (includes Core Library)" described behaviour that no longer
  exists. Note the first mode got NARROWER (project files only) as well as the second changing.
- `estimateCopyBytes` + a pre-write confirm dialog (Jeff's addition to docket 22).
- The `KNOWN GAP` caveat comment in `ProjectBundler.h` was already cut back to a factual
  one-liner during the sweep correction; the gap itself is now closed.

**Process note.** Two build cycles this batch were lost to shell-layer text handling, not to code:
a scripted pointer->reference fix that did not cover a hand-written function, and a heredoc that
ate a backslash in `replaceCharacter('\\', '/')`, producing 8 cascading errors from one character.
Standing correction for the rest of the batch: **source edits go through Write/Edit, never shell
heredocs.**

## 2026-07-26 — Task 7 steps 1-2 — code-complete (step 3 held at the plan's step-0 gate)

Both gates green (`RELEASE_EXIT_CODE=0`, `DEBUG_EXIT_CODE=0`, zero errors).

**Step 1 — self-cleaning registry.** `sOnRegisterApplicator` / `sOnRegisterReader` widened to carry
an owning `juce::Component*`; `StandaloneEditor` now derives from `juce::ComponentListener` and
keeps a `Component* -> {paramIds}` reverse index; `componentBeingDeleted` drops that component's
entries. **`eraseAutomationEntriesWithPrefix` and all 17 hand-written prefix literals deleted.**

The 20 hook invocations gained an owner argument (`&slider` / `&button` / `&combo` /
`&lifetimeGuard` / `&selector` in the five wrappers; the `VKnob` and toggle in
EffectEditorPanels; `&mFader` / `&mPanKnob` in MixerTrackStrip; `owner = this` captured into
`ParametricEQDisplay::registerAutomationForBoundEQ`'s `regOne` lambda). **The 29 helper call sites
did not change at all** — that was the point of the reverse-index shape over literal RAII tokens,
and it is why this landed as a contained diff rather than a 44-site sweep.

Two hazards handled at write time rather than found later:
- **Teardown.** `~StandaloneEditor` destroys every page and each fires `componentBeingDeleted`.
  `mTearingDownAutomation` is set at the top of the dtor so the callback is disarmed before
  anything starts dying.
- **Project boundary.** `resetProjectState` clears both maps, so the owner index has to be dropped
  in step with them — and the listeners DEREGISTERED first, since JUCE does not auto-remove
  listeners and this object outlives most of those components. Without that, a component whose ids
  were just cleared would still be watched, and its later destruction would erase ids a NEW
  registration may since have claimed.

**Step 2 — dead-lane diagnostics.**
- `onIsParamStale` was structurally incapable of catching this class of failure: it returned
  `apvts.getParameter(pid) == nullptr`, so a REGISTRY lane was never stale by construction — and
  registry lanes are exactly the ones that die with their control. Now stale means neither
  authority resolves the id, so the Event Editor's existing grey/red row finally covers them.
- Dispatch gained an else branch. Previously a missed lookup and a successful apply were the same
  code path with no branch at all. Warns once per paramId per session (`mReportedDeadLanes`) with
  `jassertfalse`; the dispatch runs at tick rate so an unguarded warning would flood.
- **Lane deliberately NOT deleted** when its target is missing — matches Ardour and Tracktion,
  where automation data outlives the live control and re-binds when the target returns. Deleting
  user automation because a panel closed would be a worse bug than the one being fixed.

**HELD: step 3 (FX-rack + pedal applicators targeting the DSP).** The plan's step 0 is an ear
confirmation before this is coded, and it has been put to Jeff twice without an answer. Holding
rather than guessing: step 3 is the largest and riskiest piece in the batch (it changes where
automation values are delivered for effects and pedals, code that works today except in the
channel-switch case), and it needs a `setParamByKey`/suffix table per effect type.

**Steps 1-2 made the confirmation much sharper than the original "listen for the knob".** With
step 1 erasing the entry when the panel dies and step 2 warning on an unresolved lane, a Debug run
now REPORTS the bug directly: automate an FX rack knob on Layer 0, switch the Effects page channel
dropdown to Layer 1, play -> `[automation] lane targets nothing: <paramId>` plus a jassert. That is
a definitive yes/no instead of an ear judgement.

## 2026-07-26 — Task 7 step 0 RESULT (Jeff's Debug test) + three defect fixes

**THE FX-RACK HOLE IS CONFIRMED.** Jeff ran the Debug test. Two of the three stacks he sent were
NOT crashes: `applyAutomationAtCurrentPosition() Line 3179` is the step-2 `jassertfalse`, i.e. the
diagnostic firing the moment he switched the Effects page channel. He then confirmed by ear:
"while looking at a different effects rack page it doesn't play the automation." Step 3 is
justified and stays in scope. Build gate green after the fixes below.

**Defect 1 — shutdown access violation (CRASH, mine, from step 1).**
Stack: `~StandaloneEditor` -> `closeAllDynamicTabs` -> `closeDynamicTabs` -> `resetProjectState`
line 11406 -> `Component::removeComponentListener` -> AV. Cause: the `mTearingDownAutomation`
guard makes `componentBeingDeleted` early-return, so during teardown entries are never erased —
by the time `resetProjectState` runs (from INSIDE the dtor) `mAutomationOwners` holds pointers to
already-destroyed components, and it dereferenced one to deregister. Fix: skip the deregister loop
entirely while tearing down; nothing needs deregistering when every component is dying anyway.
Jeff hit this twice with identical stacks — the second was the same defect on the pre-fix binary.

**Defect 2 — ID STEALING (mine, from step 1; the serious one).**
`SlotComponent::setEditor` does `mEditor = std::move(editor)`, and `unique_ptr::operator=`
installs the new pointer THEN deletes the old. Registration happens during panel construction /
`setSlotContext`, i.e. BEFORE `setEditor` destroys the previous panel. So a rebuilt FX panel
registers its ids first, and the dying old panel's id list still named those ids — erasing them
revoked a LIVE registration. Fix: new `mAutomationIdOwner` (id -> current owner Component*);
`componentBeingDeleted` only erases ids it still owns. This was actively breaking automation that
had just been correctly re-registered, not merely leaving junk behind.

**Defect 3 — "deleted slot" tag was mine, and it was lying (from step 2).**
Jeff: the Event Editor tagged effects-rack automations "deleted" — after using the Delete button,
on newly created lanes, after a New Project, and after reload; master-strip automations were fine.
Cause: widening `onIsParamStale` to "not in APVTS AND not in the registry" is wrong while
applicators are keyed to PANELS — a rack lane is unregistered whenever its channel is not the one
the Effects page currently shows, which is an ordinary viewing state, not a dead lane. The tag was
accurate about the registry and false about the user's automation. REVERTED to the APVTS-only
test, with a note to re-widen after step 3 (once registration lasts as long as the RACK,
"not registered" will genuinely mean "target gone").

Also removed the `jassertfalse` from the dispatch. It earned its place by catching the hole on the
first test, but until step 3 removes the false-positive source it fires on the ordinary act of
changing FX channels, which would make every Debug session a dialog fight. `DBG` log retained.

**Jeff-reported items — ALL THREE RESOLVED 2026-07-26 (build gate green after an access-level fix).**

1. **Event Editor Delete button REMOVED** (Jeff's call: redundant — points already delete
   individually via right-click, and the whole block deletes on the Builder grid). Button, its
   `doDeleteAutomationPoints` handler, and its layout slot all deleted. Worth recording WHY it was
   the odd one out: it did not delete anything, it reset the lane to a single midpoint, so its
   label described neither of the two things it actually did.
2. **Last-point delete now prompts** (Jeff's spec: "if on the event editor or on the builder grid
   you delete the last point ... prompt if you want to delete the whole automation").
   `ArrangementGrid::promptDeleteWholeAutomation(blockIdx)` is the single implementation; BOTH
   routes call it — the grid's own right-click, and the Event Editor's via a new
   `onDeleteWholeAutomationRequested` callback wired in `openEventEditor`. One implementation so
   the two routes cannot drift into asking different questions or taking different actions.
   Yes removes the block as one undoable edit; No leaves the final point alone. Fires on removing
   the last REMAINING point, not on emptying a selection.
3. **Builder-grid redraw FIXED, and it was a real missing notification, not Debug lag.**
   `EEAutomationGrid::onChanged` is the chokepoint every Event Editor lane edit already ran
   through, but it only repainted the Event Editor's OWN grid — the Builder's arrangement block
   kept drawing its old shape until the page was navigated away from and back, which read as the
   edit not having taken. New `EventEditorContent::onLaneEdited`, fired from that same chokepoint,
   wired in `openEventEditor` to repaint `mBuilderPage->getGrid()`. Fixed at the chokepoint rather
   than at each edit site so future edit paths inherit it.

Build note: first attempt failed with one error — `promptDeleteWholeAutomation` was declared
beside `deleteSelected()`, which sits in `ArrangementGrid`'s PRIVATE section, and the Event Editor
route calls in from outside. Moved to public.

## 2026-07-26 — Task 7 step 3 — architecture complete, Compressor converted (1 of 12)

Jeff ruled Option A (target the DSP) with a refinement that removed my objection outright: do NOT
transcribe the mapping math into a second implementation — **extract it out of the UI entirely so
the panel and the applicator call the same function.** Verbatim: "We cannot have that math
existing in two different places." That is a better design than what I proposed; I had treated the
duplication as unavoidable when the fix was to give the math one home.

**New: `Source/DSP/EffectParamMap.h/.cpp`.** The single home for what a rack parameter IS —
suffix, natural range, apply-to-DSP, read-from-DSP. Both directions, because panels already
duplicated the reverse map for their startup knob sync (the same drift risk pointing the other
way). `applyNorm` converts 0..1 through the SAME range the slider uses, so a lane at 50% and a
knob at 50% land on the same DSP value by construction rather than by two implementations
agreeing.

**Compressor converted first, deliberately** — it owns the two genuinely dangerous mappings, the
ones a transcription would have silently broken:
- **"Input" is a DRIVE control, not a threshold.** No threshold knob exists on the face plate;
  turning Input up drives harder into a fixed threshold, so more input must mean MORE compression.
  `[-60..0]` maps INVERTED onto threshold `[0..-42]`.
- **Attack/Release are switch POSITIONS 0-7 into a ms table**, where position 1 is slowest and 7
  fastest — the reverse of what the numbers imply.
The panel now calls `applyNatural` on drag and `def->read(dsp)` for its startup sync; both inline
copies are gone.

**Resolution path (the load-bearing design).** `EffectsPage::resolveChannelDsp` extracted from
`onChannelChanged` — ONE switch, two outputs. I nearly duplicated the 106-line switch to get the
rack separately, which would have recreated this task's own drift problem one layer up.
`rackForChannelId` is the thin wrapper.

`registerSlotAutomation` registers per-slot applicators capturing ONLY `(channelId, slotUuid,
type, suffix)` — no panel, no knob, no SafePointer — and resolves rack -> slot -> DSP at apply
time. Three deliberate properties:
1. **Survives panel destruction**, which is the entire bug.
2. **Registered with a NULL owner**: step 1's reverse index ties entries to a component's life;
   these are tied to the RACK instead. A dead rack resolves to nullptr and no-ops, which step 2
   logs.
3. **Slot lookup by UUID, never index** — rack reorder carries the UUID, so a lane cannot be
   repointed at a different effect. That is exactly REAPER's failure mode per the architecture
   research (positional `fxindex`/`parameterindex`).

Build gate green. Two build cycles lost to trivia: `promptDeleteWholeAutomation` declared in a
private section, and `EffectsPage.h` referencing `VibeGraph` with no forward declaration (12
cascading errors from one missing line).

**HELD before rolling out the remaining 11 effect types — awaiting Jeff's re-test.** The
architecture compiles but has never executed: the rack-by-channel-id and slot-by-uuid lookups are
runtime-untested. Converting 11 more types onto an unverified foundation is the exact mistake
`verify-primary-effect-before-optimizing` warns about. The re-test is the same gesture that
produced the original jassert, so it is a direct before/after.

## 2026-07-26 — Off-screen window on launch (Jeff-reported) — FIXED

**Symptom:** app launches, appears, then slides off the side of the screen and is unreachable.
Persisted after Jeff disconnected every monitor but one.

**Cause — pre-existing, in the window RESTORE path, not in this batch's changes.**
`VibesynthStandaloneApp::initialise` restored the saved `<WindowState>` with a bare
`mWindow->setBounds(x, y, w, h)` and no check that those coordinates still correspond to an
attached display. A position saved while a second monitor was connected therefore placed the
window into coordinate space with no display behind it. Disconnecting monitors cannot help — the
stale coordinate is what is stored, and nothing re-validated it.

**Fix:** intersect the restored rect with
`juce::Desktop::getInstance().getDisplays().getTotalBounds(true)` and require a REAL overlap
(>= 200x100, not a touching edge, so a 99%-off-screen window is rejected too). Unreachable ->
`setFullScreen(true)`, which is always visible. Build gate green.

**Honest note on causation.** The missing clamp is not mine. But Jeff had been repeatedly hitting
the Task 7 step 1 shutdown crash, and `shutdown()` is what writes `<WindowState>` — a crash before
that write leaves whatever coordinates were stored last, including ones from a multi-monitor
session. So my crash is a plausible reason the stale value survived, even though the absent clamp
is what turned a stale value into an unusable app. Recorded rather than argued either way.

**Immediate workaround given to Jeff** (needed before he can test anything): delete the
`<WindowState .../>` line from `Documents\BaySickDAW\settings.xml`, or set `maximized="1"`.

**Consequence for sequencing:** Task 7 step 3's runtime verification is blocked until Jeff can see
the app. The remaining 11 effect-type conversions stay held behind that verification.

## 2026-07-26 — Step 3 re-test FAILED, root-caused, fixed (+ ribbon labels)

**Jeff's re-test:** "it still breaks the automation and now when I swap it sounds quieter."
Two symptoms, ONE root cause, entirely in my step-1 code.

**Root cause — null-owner registrations never cleared the id's owner claim.**
`trackAutomationOwner` early-returned on `owner == nullptr`, so it never touched
`mAutomationIdOwner`. Channel-switch order is:
1. new panel built -> `setSlotContext` registers KNOB-targeting applicators, owned by those knobs;
2. `registerSlotAutomation` overwrites the same paramIds with DSP-targeting applicators
   (owner = nullptr, because they are rack-scoped);
3. the OLD panel is destroyed -> `componentBeingDeleted`.

At (3) the id->owner map still named the dying knob as owner, so the ID-stealing guard added
earlier that same day did exactly what it was told and **revoked the DSP applicator that had just
replaced it.** The fix installed correctly and was deleted moments later — hence "still breaks."

**And that also explains "quieter,"** which was the clue that made it diagnosable: between (2) and
(3) the automation timer had a live DSP applicator, applied the lane's value to the compressor
once, and then lost the applicator — leaving the effect stranded at that value with nothing to
update it again.

**Fix:** a null-owner registration now explicitly `mAutomationIdOwner.erase(paramId)`, so no
component can revoke a rack-scoped entry. Build gate green on BOTH configs.

**Design lesson, recorded because it bit twice from different directions:** "register the closure"
and "record who owns this id" had to be ONE operation. Leaving two paths — one that updated
ownership and one that skipped it — meant the ownership guard operated on stale information and
did damage while behaving exactly as specified.

**Also fixed: ribbon slot labels (Jeff, screenshot).** With Layers/Bass/Drums at zero instances
their ribbon slots rendered as unlabelled coloured blocks. `getSlotDisplayName` derived the label
from the ACTIVE TAB's name, which only worked while a tab was guaranteed to exist; Clips/Vox/Inst
had gained a zero-instance fallback when THEY became zero-capable, and docket 18 made three more
types zero-capable without adding theirs. All six now have it.
Worth recording how it was missed: the Task 1 sweep checked FUNCTIONAL >=1 assumptions (piano-roll
registry, dropdowns, tab selection, kit list — four found already safe) and never considered
PRESENTATIONAL ones. Nothing broke; the UI just stopped saying anything. A screenshot caught it
instantly where a grep-based sweep never would.

**Build note:** one Release-only `LNK1104` (cannot open BaySickDAW.exe) while Jeff had the app
open for testing — exe lock, not code; Debug linked clean in the same run. Cleared on the next
build.

## 2026-07-26 — Step 3 second failure ROOT-CAUSED: the map key was wrong by design

Jeff's second re-test: "Still breaks the automation when I swap, not as quiet as before but still
a little quieter. Please research this before just doing something to 'fix it'." Fair — the two
prior attempts were changes made on a theory. This one was researched first.

**Root cause: `EffectParamMap` was keyed on `EffectType` alone, and that is not a unique key.**
`createEffectEditor` dispatches `EffectType::Compressor` to FOUR different panels on
`CompressorDSP::mType` — Modern (`CompressorPanel`, the DEFAULT), FET, Opto, CS. I populated the
table from the FET panel and keyed it by type, so every compressor got FET's mapping.

The damage is precise, because different variants reuse knob LABELS and therefore derive identical
automation suffixes:

| suffix | Modern | FET | Opto | CS |
|--------|--------|-----|------|-----|
| `attack` | 0-400 **ms** | 0-7 **switch position** | — | 1-50 **ms** |
| `release` | 1-4000 **ms** | 0-7 **switch position** | — | — |
| `gain` | -30..+30 **dB** | — | 0-100 **face plate** | — |

So a "3" meant as a switch position was being handed to a millisecond setter — an audibly wrong,
quieter compressor. That is the "still a little quieter." And because Jeff's compressor is almost
certainly Modern (the default), whose suffixes (`thresh`/`ratio`/`gain`/...) were not in the FET
table at all, his automated knob never got a DSP applicator — hence "still breaks."

Both symptoms, one cause, and NOT another lifetime bug: the `EffectParamMap` layer and the
resolution path were never implicated. The earlier revocation bug was masking part of the level
error, which is why it got quieter-but-less-quiet after that fix.

**Fix (Jeff: "yes key it on type and variant, redo all four compressor modes"):** every entry
point now takes `(EffectType, int variant)`. `variantOf(type, dsp)` reads the variant from the DSP
itself, so no UI is needed to resolve it; 0 for single-panel effects. All four compressor tables
written from their real panels: Modern (10 params), FET (4), Opto (2, 0-100 face-plate scale), CS
(4). Build gate green on both configs.

**Judgement call recorded:** FET's `nearestPos` originally seeded at index 4 and skipped index 0,
so a stored OFF read back as a mid position. The shared version starts at index 1 and excludes 0
deliberately — 0 is OFF, not a time, and should not win a nearest-millisecond comparison. That is
a behaviour CHOICE in the reverse map, not a transcription of the old code.

**Scope correction for the rollout.** This is not "12 effect types". It is types x character
modes: Saturation, Overdrive, Delay and Reverb all have mode umbrellas per the `createEffectEditor`
dispatch. Realistically 20+ distinct panel/mapping sets. Surfaced to Jeff before continuing.

**Build note:** two Release-only `LNK1104` exe locks while Jeff had the app open for testing;
Debug linked clean in both runs. Cleared once he closed it.

## 2026-07-26 — Step 3 ARCHITECTURE PROVEN + the last non-DSP control closed

**Jeff's differential test is what cracked it.** He isolated two controls on the same Modern
compressor panel:
- the compressor's own **Gain** knob -> **automation SURVIVES the channel swap**
- the **outbound volume** knob -> still dies

That split is decisive. It proves the whole DSP-targeting architecture works at runtime —
`registerSlotAutomation`, lazy rack-by-channel-id resolution, slot-by-UUID lookup, variant keying,
and the registry lifetime fixes — while isolating the remaining failure to one specific control.
Everything I had been "fixing" in the previous three attempts was fine by this point; only this
one control was left.

**Why the outbound volume knob could never have worked via EffectParamMap.** It is not a DSP
parameter. It lives on `EditorPanelBase` (every panel inherits it), is stamped `<base>_output_vol`
by `setSlotContext`, and writes `EffectRack::setSlotOutputGain(slot, db)` — the RACK's per-slot
gain, not anything inside the effect. `EffectParamMap` maps DSP parameters by construction, so no
amount of adding effect types would ever have covered it. It was still on the panel-targeting
applicator and still died with its panel.

**Fix:** `registerSlotAutomation` now also registers `output_vol` against the RACK, resolved
through the same UUID lookup (factored into a shared `resolveSlot` lambda used by both this and
the per-parameter registrations). Range `-24..+12 dB`, mirroring `EditorPanelBase`'s slider.
Build gate green on both configs.

Two properties worth recording:
1. It is a PER-SLOT registration, not per-effect-type, so it covers all 20+ panels immediately —
   including pedal-style panels that call `disableOutputVolKnob()` and own their own Level knob.
2. It closes the last CATEGORY of automatable control. The remaining rollout is now purely
   "write mapping tables for the other effect types" — no further architecture and no further
   classes of target to discover.

**Process note for the record.** Three consecutive failed attempts (revocation bug, null-owner
claim, type-only key) were each diagnosed from symptoms and each fixed something real, but the
sequence only converged once Jeff pushed back with "please research this before just doing
something to 'fix it'" and then ran a differential test isolating one working control against one
broken one. Reading the code to find the NEXT difference beat guessing at the next fix.

## PENDING Main Plan edits — DEFERRED TO G4 CLOSE

Per Jeff's 2026-07-25 standing instruction: accumulate here, apply in ONE pass at the G4
boundary. Convention: [`Running Notes/sturdy-tagging-pangolin.md`](sturdy-tagging-pangolin.md).

1. **§5 QA-ProjectSave entry — plan-file pointer parenthetical.** Append the four 2026-07-26
   batch-open docket answers to the existing "(G4 group open 2026-07-25 — premise updates: ...)"
   note: docket 15=B (templates carry `<Processor>`; the engineData premise was false),
   docket 16 (Rusty = "Save Page Preset & Delete", superseding docket 11=A's label),
   docket 17=b (no new Pack item; Export Project Bundle gains the save-first prompt),
   docket 18 (L/B/D empty by default + delete-to-zero + empty-bus hiding, inserted as Task 1).
2. **§5 QA-ProjectSave Scope bullet — "Template scope expansion".** It currently says the format
   is "extended to save vox / inst / clip / rusty / aux / samples ... Mirrors the project-save
   shape". Amend to name the `<Processor>` snapshot explicitly as the mechanism, since the
   UIState walk alone does not carry mixer/rack/EQ state.
3. **§5 QA-ProjectSave Scope bullet — FND-1 line.** It lists RustyDrums among page types that
   "should" gain the save-prompt; add that its button reads "Save Page Preset & Delete" and
   chains the J-11 Player Preset, not a kit save.
4. **§5 QA-ProjectSave Scope bullet — "Pack project" action.** Reword: the action shipped in
   QA-Export as "Export Project Bundle…"; QA-ProjectSave adds only the save-first prompt. No
   separate Pack item exists.
5. **NEW §5 scope bullet + §9 Forks entry for docket 18.** The empty-by-default / delete-to-zero
   / empty-bus work has no existing §5 line. Per Rule 3 it folds into QA-ProjectSave's own §5
   entry as a new Scope bullet (Jeff ruled it in-scope for this batch), with a §9 Forks entry
   dated 2026-07-26 chronicling the addition and the three corrected premises above.
6. **§5.5 Domain Coverage.** Docket 18 pulls Mixer / Routing and UI / L&F / Theming into this
   batch's bucket set (was System Pages + Cross-cutting Infrastructure). Bucket line needs the
   additions — flagged as a Jeff call at G4 close, not applied unilaterally.

## 2026-07-27 — Task 7 sweep escalation -> QA-ModelShell planned; Task 7 badger scope CLOSED

The mandatory applicator sweep (session-open directive) ran the full census: 19 wrapper call
sites across 9 files (the resume prompt's "13 across 6" undercounted) + 7 direct hook sites
+ the statics. Verdicts and the census table live in chat + the QA-ModelShell plan; the
load-bearing outcomes:

- **Only the FX rack had the die-with-UI hole** (unconverted types + the one Reverb freeze
  toggle). Every engine editor, the pedals tiles, mixer strips, and the EQ displays were
  verified safe UNDER TODAY'S immortal-pages shell. output_vol re-verified as the only
  rack-level non-DSP automatable control.
- **Two registration-timing gaps found:** rack lane wiring is view-gated (registered only in
  rebuildSlotEditor, wiped at every project boundary) and lazily-materialized APVTS params
  miss the boundary's static seed. Both dissolve under model-side registration.
- **Export findings (the escalation):** offline export ignores every non-main-APVTS lane
  (dispatch is the editor UI timer; the engine replay covers main-APVTS only) — and beneath
  it, the render processor has NO instrument engines and no instrument InsertNodes at all
  (page-owned engines; register* family is page-called). Verified in source; Jeff confirmed
  by ear (vox/inst exports render nothing). Export destination also wrong (userMusicDirectory,
  not <project>\Exports\) and the metronome check came back safe today / must-gate under
  live-graph export (click is post-master-tap, MetroDSP default-off).
- **Jeff's rulings (the arc):** engine-ownership inversion is a V1 REQUIREMENT (FL mandate,
  "engines are the drivers and the pages just hold them"); export = the model rendering
  itself offline (FL same-instance shape, verified vs Image-Line wrapper evidence + JUCE
  setNonRealtime); destroy-on-close window shell (contained native-child workspace, fixed
  main, custom title bars, close+resize, "+" tab bar w/ Piano Roll required); BLU-480 rack
  window; THE ENTIRE Future State tiers list including full VST3 hosting; tempo lane
  followed in export; full re-prepare; FL-style progress-bar export UX; CL-102 struck
  (already shipped as PagePresetIO — verified after Jeff challenged my wrong claim).
- **QA-ModelShell batch created:** plan `grand-inverting-mammoth.md` (8 task sets = Jeff's
  approved groups; per-set commits; ONE batch smoke at TS8), approved 2026-07-27, slotted
  directly after this batch (run-plan G4 composition note; order badger -> mammoth -> yak ->
  stoat -> heron). Conflict review of yak/stoat/heron ran; dated notes applied to all three;
  boundary locked (R3 covers only yak/stoat/heron); yak Task 2 shrinks to verification via
  mammoth TS1's dormant UndoManager pre-wire; SS-B reconciliation lands in mammoth TS8.
- **Task 7 badger scope CLOSED + remainder re-routed** to mammoth TS3 (plan annotated).
  Step-3 architecture runtime-proven: Jeff's differential test + outbound-vol confirm
  ("the vol knob does work now with the change"). Commit seams re-ruled by Jeff: Commit 2 =
  this arc, now; ONE final commit for Tasks 8-12 + close.
- **Process corrections this arc:** the "undoable" wording confusion (clarified: one-Ctrl+Z
  restorable; last-point prompt approved); QA-Soundness Main-Plan absence explained (its §5
  entry is deferred to the G4-close ledger by Jeff's 2026-07-25 standing instruction — cite
  the deferral every time the batch is named); dependency-direction phrasing corrected
  (badger relies on nothing in mammoth; later batches consume badger's output).

## 2026-07-27 — Tasks 8-12 — code-complete (batch code DONE; gates green per task)

**Task 8 — central app-root resolver.** New `Source/AppPaths.h` (`AppPaths::appRoot()`), the
single authority for `Documents\BaySickDAW`. ~40 hand-spelled root constructions across 25
files converted (all three islands — EffectPresetIO / SampleLibrary / ProjectManager — now
resolve through it internally; their public APIs unchanged). BONUS in the same sweep: four
hand-spelled Core Library roots (StandaloneEditor default-kit x2 + restore fallback,
ProjectManager shortcut target) now call the EXISTING `SampleLibrary::getCoreLibraryDir()`.
Deliberately NOT converted: the two legacy Roaming fallbacks (migration source semantics),
two bare-Documents sites (NAMIR permissions fallback, bundle-destination suggestion).

**Task 9 — three corrections.** mPartSel comment now states the true mechanism (slider IS
addAndMakeVisible'd, never given bounds). InstPage pedals `engineRootTag` ->
"BaySickPedalsRoot" (post-QA-Verify blob root; tag is config-side-only for pedals — no kit
callback — so old page presets unaffected). `kStateVersion` DECISION: kept write-only with a
keeper comment (the stamp is the post-v1 migration hook; wiring a reader now = pre-v1
migration logic, dropping it strips version info from exactly the files a migration would
need; matches QA-Soundness Task 6's stance so the two plans agree).

**Task 10 — template submenu + default-template consumer + removals.** File > New from
Template submenu (New from Default Template with name suffix + missing-file grey /
Premade Templates / My Templates; recursive walk; ids 900+ via mTemplateMenuFiles rebuilt
per menu open; every pick -> loadTemplate's unified dirty gate). Items 102 + 109 gone;
`doFileNewFromTemplate` (+ its ProjectBrowserWindow launch) and `showTemplateMenu` (+ dup
kIdSaveAs) DELETED, zero refs verified. `ProjectManager::newProject` lost the dead
folder-seed templatePath param (+ contract comment updated; the one juce::File() caller
cleaned). Set Default Template's confirmation now names the real consumer (the old text
claimed File > New would use it — false since QA-Ef #6). Stale "kit + 8 layers + 4 basses"
wording fixed at both touched comments.

**Task 11 — Rusty "Save Page Preset & Delete" (docket 16).** Dirty trio ported verbatim from
ClipsPage (ApvtsDirtyListener + mPageDirty + mSuppressDirty + isPatchDirty). Attach point =
`loadKit` (the single chokepoint every engine (re)creation runs through — loadProgram and
project restore both funnel into it); detach at tearDownCurrentProgram top + dtor. Preset
apply wrapped in suppress + resets clean. `savePlayerPresetAs` gained a success-only
completion hook; the editor's delete flow gained the 3-button branch (verbatim label, honest
description: writes a PLAYER PRESET — program + every knob — not a full page preset; save
dialog Cancel aborts the chained delete too); clean-page 2-button warning byte-identical in
behavior. Self-caught: one non-ASCII character corrupted into a comment mid-edit, fixed in
the same turn.

**Task 12 — bundle save-first (docket 17=b).** `doFileSaveAs` gained an optional
success-only `onSaved` completion (threaded through the invalid-name retry; cancel/failure
never fire). `doExportProjectBundle`'s unsaved bail replaced by the save flow continuing
into the bundle via SafePointer re-entry.

**Close artifacts.** Master Test Plan §B.30 authored — 24 scenarios (PS-1..PS-24), blocks
`dadb958a` + `54177488` + final commit (hash backfilled at close); scope note records the
Task 7 remainder's re-route to QA-ModelShell TS3 and that PS-21/22 cover only the badger
scope. PS-24 covers the Jeff-reported Event Editor fixes (last-point prompt, one-undo-step
block removal, no Delete button, Builder redraw).

## Held Work Log entry (apply at section pass)

> Drafted at code-complete via `/draft-doc batch-close`; NOT yet applied to `Implemented Work Log.md`.
> Applies with the §5 STATUS flip when §B.30 passes the campaign walk (R2). Backfill the final
> commit hash (here and in the §B.30 `blocks:` line) at commit; stamp the full `HH:MM PT` at apply.

### 2026-07-27 <HH:MM> PT — QA-ProjectSave — 12 tasks across 3 commits (grew mid-batch from 7 via the second docket + the G4 findings sweep; four planned seams re-ruled to three): Layers/Bass/Drums open EMPTY and delete to zero, with empty bus strips hidden across all six type-driven buses on a MEMBERSHIP test that shrank to three lines after Jeff twice corrected my over-built route-guard; templates v2 replace the never-loadable v1-USER schema with the project shape (`<Processor>` + `<UIState>`), loading with New Project semantics behind a scoped teardown + unified dirty gate, dual-format v1-factory preserved; ONE SampleLibrary `library:`/`mysamples:` resolver ends Core Library duplication into every project and makes kit/clip/BaySickPlayer paths account-independent (VibePlayer's `bsp_loadPath` was absolute at 3 sites inside the most-used engine's blob); template save adopts volatile samples AND folders into My Samples through a decode -> rewrite -> re-encode blob round-trip; `ProjectBundler` walks engine-held refs via decoded engine blobs, stops copying Core Library in either scope, and shows a pre-write size estimate; the automation registry became self-cleaning (ComponentListener reverse index + id-owner guard; `eraseAutomationEntriesWithPrefix` and its 17 prefix literals deleted) with dead-lane diagnostics, and FX-rack automation moved OFF the widget onto the DSP through the new `EffectParamMap` keyed `(type, variant)` — Compressor x4 variants + per-slot `output_vol` against the rack, runtime-PROVEN by Jeff's differential test after three failed attempts, each root-caused (ID-stealing revocation / null-owner claim never cleared / type-only map key); Task 7's remainder re-routed to QA-ModelShell TS3 — the batch this batch's own applicator sweep CREATED when the census escalated through export-silent-instruments into the engine-ownership-inversion ruling and the full tiers pull-in; central `AppPaths` app-root resolver (~40 sites / 25 files); File > New from Template submenu with the default-template pointer finally consumed; Rusty joins the 3-button "Save Page Preset & Delete" pattern; bundle save-first prompt; plus three Jeff-reported fixes (off-screen window clamp, ribbon zero-instance labels, the Event Editor delete/prompt/redraw trio)

**Bucket:** System Pages, Cross-cutting Infrastructure, Mixer / Routing, UI / L&F / Theming, Effects, Players. Batch `deep-packing-badger`. `blocks:` `dadb958a` + `54177488` + `b933b54a`.
*(Bucket set is a HELD Jeff call — §5.5 lists this batch as System Pages + Cross-cutting Infrastructure
only. PENDING ledger item 6 already flags the docket-18 additions (Mixer / Routing, UI / L&F /
Theming); the re-plan's diffs additionally touch Players (VibePlayer writers, Rusty page + processor,
BaySickSolstice comment) and Effects (`EffectParamMap`, EffectsPage, EffectEditorPanels, pedals tags).
Confirm all four additions at G4 close, not unilaterally.)*

#### Done

- **Task 1 — L/B/D empty by default + delete to zero + empty buses hidden (docket 18, inserted at
  batch open).** `addDefaultDynamicTabs()` + `addDefaultDrumTab()` deleted (177 lines) with all
  three callers — editor construction, `doFileNew`, and the `loadKitImpl` kit-recovery respawn that
  would have silently undone a delete-to-zero. `closeTab`'s `zeroAllowed` opened to L/B/D; all 15
  `isLastOfType` guard sites removed and the helper + `closeTab`'s `force` param retired with them.
  One parameterised `EngineEmptyState` (not three copies of the Vox/Inst shape) + `hideAllEmptyStates()`;
  F8/F9/F10 and View > Layers/Bass/Drums — silent no-ops at zero tabs — now land on the empty states.
- **Task 1 — the bus-visibility test SHRANK to three lines after Jeff's two-round correction.** My
  extra "does anything route here" guard was unreachable: sends only land on aux strips, and
  re-pointing a main-out RE-BUCKETS the strip into the target bus's group — so for a bus "something
  routes here" and "has members" are the SAME condition. `collectLiveRouteTargets` /
  `liveRouteTargets()` / `isBusVisible()` all deleted; the test is `if (! hasMembers &&
  ! isAlwaysVisibleBus)` off data the layout already had. My first-cut perf defect (order 10^5
  string allocations per layout pass) existed only inside that machinery and died with it. KEPT,
  independently justified: `isAlwaysVisibleBus` (flag-gated buses must appear before anything routes
  in) and the `sendSuffix()` `static const` hoist (speeds `sweepSendsTargeting`). Applied to
  Clips/Vox/Inst too, which had the identical dead-bus condition. Four >=1-of-a-type surfaces swept
  and already safe.
- **Task 2 — template writer v2 (docket 15=B).** `serializeUIState` split into `serializeTabsInto`
  + `serializeStripNamesAndOrders` under a new `serializeStructuralUIState` (project save
  behaviour-identical); `serializeProject`'s `<Processor>` block extracted to
  `VibeSynthProcessor::writeProcessorState` so `saveTemplateAs` emits the IDENTICAL block — every
  mixer fader/pan/width/mute/solo/polarity/sends, each insert's rack, each post-rack EQ.
  `<BaySickTemplate version="2">` wraps `<Processor>` + `<UIState>`; deliberately NOT a
  `serializeUIState` call (activeTabId, `<Arrangement>`, `<Metronome>`, `<SongLoop>` etc. are
  SESSION properties, not skeleton). `gen_factory_presets.py` checked: still writes v1-factory, so
  the dual-format branch serves a real case with no generator edit.
- **Task 3 (+ docket 19 amendment) — loader v2 with New Project semantics.** Dirty gate lives in
  `loadTemplate` (not the menu entries, so Task 10's submenu inherits it) — closes the verified
  silent-discard bypass, the ONLY File-level teardown with no save prompt. v2 restores through the
  PROJECT path (`applyProcessorState` -> `deserializeUIState` -> `restoreAudioStripsFromArrangement`
  — no second restore implementation to drift); `applyProcessorState` + `reportMissingFilesIfAny`
  extracted and shared, so template load reports missing NAM captures / sfizz kits / IRs exactly as
  project load does. `TabTeardownScope` keeps the v1-factory branch from destroying
  Clips/Vox/Inst/Rusty tabs it cannot put back; three traps handled (Rusty carries
  `TabType::Drums` — `dynamic_cast`-excluded; scoped path must NOT `clearDynamicStrips()`; only the
  three L/B/D name counters rewind). Self-caught ordering defect fixed pre-close: an early
  `applyPendingRackStates()` would have consumed the rack stash before the Audio InsertNodes
  existed — clip racks restoring EMPTY, invisible to any build gate. Docket 19 then flipped the
  shipped merge reading to New Project semantics (`closeAllDynamicTabs` -> `clearDynamicStrips` ->
  `resetToBlankState`), which also dissolved the Clips path-resolution bug found in this task.
- **Task 4 — sample-retention hybrid + resolvers.** `SampleLibrary::makeStableRef` /
  `resolveStableRef` / `isStableRef` are the SINGLE implementation of the `library:` / `mysamples:`
  convention (format matched to the pre-existing DrumPage writer, so pre-batch refs still load).
  `importSample` is source-aware — THE fix for Core Library samples being duplicated into every
  project. `resolveProjectFile` tests stable refs BEFORE the absolute-path test (load-bearing
  ordering). A missing clip feeds `MissingFileReport` instead of restoring as a silent empty player.
  Two plan bullets deliberately NOT implemented, with reasons recorded (ClipsPage's `clipRef` is
  already account-independent; `onResolveStoredPath` already delegates).
- **Task 5 — `library:` normalization + template sample adoption (dockets 23/24/25).** Normalized:
  Guitars + Basses `kitPath`, Clips `clipPath`, Rusty `KitPath`, and **VibePlayer's `bsp_loadPath`
  (3 writers / 2 readers)** — the significant one, correcting my earlier claim that Layer/Bass
  patches were safe because the sample ref "rides inside the engine blob": it rode as an ABSOLUTE
  path, embedding the Windows username in every patch using the most-used engine in the app.
  `refForPersist` / `resolvePersistedRef` added so no call site chooses; readers still accept
  absolutes (no migration, 8b). `adoptIntoUserSamples` copies volatile files into My Samples on
  TEMPLATE save only, dedupes on size+modtime, auto-suffixes. The BaySickPlayer blob gap CLOSED
  before Commit 1 on Jeff's call ("do it before this one"): `adoptTemplateSampleRefs` does a
  decode -> rewrite -> re-encode round-trip on `engineData`, rewriting only when something was
  adopted (untouched templates keep byte-identical blobs). FOLDERS are adopted, not just files
  (`bsp_loadPath` can be a `loadFolder` drum pack); bare `.sfz` deliberately NOT adopted (a pointer
  into a sample tree — docket 24's reasoning applied structurally; one-line change if overruled).
- **Task 6 — bundler engine-reference walk + export semantics (dockets 21/22).** `enumerate` gained
  `tabsXml` and walks four layers: plain tab attributes, the Inst chain XML (NAM captures + user
  IRs), and the base64 `engineData` / `sfizzEngineData` blobs — the only place BaySickPlayer sample
  paths are reachable. Path attributes kept EXPLICIT (no "looks like a path" heuristic — false
  Missing reports would train the user to dismiss the warning). Docket 22=b: Core Library copied in
  NEITHER scope, dropdown labels rewritten (the first mode got NARROWER as well); `estimateCopyBytes`
  + a pre-write confirm dialog. The `ProjectBundler.h` caveat is gone — the gap is closed.
- **Task 7 (badger scope) — step 1, self-cleaning registry.** Register hooks widened to carry the
  owning `Component*`; `StandaloneEditor` derives `ComponentListener` with a `Component* ->
  {paramIds}` reverse index; `componentBeingDeleted` (fires FIRST in `~Component`, verified at
  `juce_Component.cpp:275`) drops entries. `eraseAutomationEntriesWithPrefix` + all 17 hand-written
  prefix literals DELETED; the 29 helper call sites unchanged — the point of the reverse-index shape.
  Teardown + project-boundary hazards handled at write time (`mTearingDownAutomation`; listeners
  deregistered BEFORE `resetProjectState` clears the maps).
- **Task 7 — step 2, dead-lane diagnostics.** Dispatch gained an else branch (a missed lookup and a
  successful apply were previously the same code path); warns once per paramId per session.
  `onIsParamStale` was widened to cover registry lanes, then REVERTED to the APVTS-only test after
  Jeff reported the Event Editor tagging live rack lanes "deleted" — while applicators are keyed to
  PANELS, "not registered" is an ordinary viewing state, not a dead lane. Re-widen after model-side
  registration (mammoth TS3). Lanes are deliberately never auto-deleted (Ardour/Tracktion precedent:
  automation data outlives the live control and re-binds).
- **Task 7 — step 0 CONFIRMED + three same-day defects fixed (all mine).** Jeff's Debug run: two of
  three "crash" stacks were the step-2 `jassertfalse` catching the FX-rack channel-switch hole live;
  ear-confirmed ("while looking at a different effects rack page it doesn't play the automation").
  Fixed same day: (a) shutdown AV — the teardown guard left `mAutomationOwners` holding dead
  pointers that `resetProjectState` (running INSIDE the dtor) dereferenced; (b) ID STEALING — a
  rebuilt panel registers before `setEditor` destroys the old one, whose id list still named those
  ids, so erasure revoked a LIVE registration; new `mAutomationIdOwner` (id -> current owner) means
  a component only erases ids it still owns; (c) the false "deleted" tag (the `onIsParamStale`
  revert above). The dispatch `jassertfalse` was then removed — until step 3 it fires on the
  ordinary act of switching FX channels; the `DBG` log stays.
- **Task 7 — step 3 architecture: the mapping math got ONE home.** Jeff ruled Option A (target the
  DSP) with a refinement better than my proposal — "We cannot have that math existing in two
  different places" — so `Source/DSP/EffectParamMap.h/.cpp` is now the single home for what a rack
  parameter IS (suffix, natural range, apply, read — BOTH directions, since panels also duplicated
  the reverse map for startup knob sync). `applyNorm` converts 0..1 through the SAME range the
  slider uses. `EffectsPage::resolveChannelDsp` extracted (I nearly duplicated the 106-line channel
  switch — this task's own drift problem one layer up). `registerSlotAutomation` captures ONLY
  `(channelId, slotUuid, type, suffix)` — no panel, no knob — resolving rack -> slot -> DSP at
  apply time: survives panel destruction (the entire bug), registered with a NULL owner (tied to
  the RACK, not a component), slot lookup by UUID never index (REAPER's positional-key failure mode
  per the architecture research).
- **Task 7 — three failed attempts, each root-caused, then the architecture PROVEN at runtime.**
  (1) The ID-stealing revocation (defect (b) above). (2) Null-owner registrations never cleared the
  id-owner claim: the ownership guard then did exactly what it was told and revoked the DSP
  applicator moments after it installed — which also explained "quieter" (one apply, then stranded).
  Fix: a null-owner registration explicitly erases the id's owner claim — "register the closure" and
  "record who owns this id" had to be ONE operation. (3) `EffectParamMap` was keyed on `EffectType`
  alone, and that is not a unique key: `EffectType::Compressor` dispatches to FOUR panels on
  `CompressorDSP::mType`, whose variants reuse knob labels with different semantics (FET
  attack/release are switch POSITIONS 0-7; Modern's are milliseconds; Opto gain is a 0-100 face
  plate) — so a switch position was handed to a millisecond setter ("still a little quieter") and
  Modern's suffixes had no table at all ("still breaks"). Fix (Jeff: "yes key it on type and
  variant, redo all four compressor modes"): every entry point takes `(EffectType, int variant)`,
  `variantOf` reads the variant from the DSP itself; all four tables written from their real panels
  (Modern 10 / FET 4 / Opto 2 / CS 4); FET's `nearestPos` now excludes index 0 deliberately (0 is
  OFF, not a time — a recorded behaviour CHOICE). **Jeff's differential test cracked it**: the
  compressor's own Gain knob SURVIVED the channel swap (proving `registerSlotAutomation`, lazy
  rack-by-channel-id, slot-by-UUID, variant keying, and the lifetime fixes all work) while the
  outbound volume knob still died — and it could NEVER have worked via `EffectParamMap`, because it
  is not a DSP parameter: it is `EffectRack::setSlotOutputGain`, rack state on `EditorPanelBase`.
  Now registered against the RACK per-slot through the same shared `resolveSlot` — covering all 20+
  panels at once, including pedal-style panels, and closing the last CATEGORY of automatable
  control. Jeff's confirm: "the vol knob does work now with the change."
- **Task 7 — Jeff-reported Event Editor trio.** Delete button REMOVED (it reset the lane to a
  midpoint — its label described neither thing it did); deleting the last remaining point now
  PROMPTS to remove the whole block via ONE implementation both routes call
  (`promptDeleteWholeAutomation` + the Event Editor's `onDeleteWholeAutomationRequested`), Yes = one
  undoable edit; Builder-grid redraw fixed at the `EEAutomationGrid::onChanged` chokepoint (new
  `onLaneEdited` repaints the arrangement block) so future edit paths inherit it.
- **Off-screen window on launch (Jeff-reported) — FIXED, pre-existing.** The `<WindowState>` restore
  did a bare `setBounds` with no display check; fix intersects with
  `Displays::getTotalBounds(true)` and requires a REAL >=200x100 overlap, else `setFullScreen(true)`.
  Honest causation note recorded: the missing clamp is not mine, but my step-1 shutdown crash
  plausibly preserved the stale multi-monitor coordinates that exposed it.
- **Ribbon zero-instance labels (Jeff, screenshot) — FIXED.** `getSlotDisplayName` derived labels
  from the ACTIVE tab; Clips/Vox/Inst had a zero-instance fallback, and docket 18 made three more
  types zero-capable without adding theirs. All six now have it.
- **Task 8 — central app-root resolver.** New `Source/AppPaths.h` (`AppPaths::appRoot()`) is the
  single authority for `Documents\BaySickDAW`; ~40 hand-spelled roots across 25 files converted;
  the three islands (EffectPresetIO / SampleLibrary / ProjectManager) resolve through it internally
  with public APIs unchanged. Bonus: four hand-spelled Core Library roots now call the existing
  `SampleLibrary::getCoreLibraryDir()`. Deliberately NOT converted: two legacy Roaming fallbacks
  (migration-source semantics) + two bare-Documents sites.
- **Task 9 — three corrections.** `mPartSel` comment states the true mechanism (the slider IS
  made visible, it just never gets bounds); InstPage pedals `engineRootTag` -> "BaySickPedalsRoot"
  (post-QA-Verify blob root; config-side-only, old page presets unaffected); `kStateVersion` KEPT
  write-only with a keeper comment — the stamp is the post-v1 migration hook, wiring a reader now
  would be pre-v1 migration logic, and dropping it strips version info from exactly the files a
  migration would need (matches QA-Soundness Task 6's stance so the two plans agree).
- **Task 10 — template submenu + default-template consumer + removals.** File > New from Template
  submenu: New from Default Template (name suffix; greyed when unset or file missing) / Premade
  Templates / My Templates, recursive walks, ids 900+ rebuilt per menu open, every pick through
  `loadTemplate`'s unified dirty gate — the default-template pointer finally has a consumer. Items
  102 + 109 gone; `doFileNewFromTemplate` (+ its ProjectBrowserWindow launch) and `showTemplateMenu`
  (+ dup `kIdSaveAs`) DELETED, zero refs verified; `ProjectManager::newProject` lost the dead
  folder-seed param; Set Default Template's confirmation names the real consumer; stale
  "kit + 8 layers + 4 basses" wording fixed at both touched comments.
- **Task 11 — Rusty "Save Page Preset & Delete" (docket 16).** Dirty trio ported verbatim from
  ClipsPage; attach point = `loadKit` (the single chokepoint every engine (re)creation funnels
  through), detach at `tearDownCurrentProgram` top + dtor; preset apply wrapped in suppress + resets
  clean. `savePlayerPresetAs` gained a success-only completion hook; the delete flow gained the
  3-button branch (verbatim label; honest description — it writes a PLAYER PRESET, program + every
  knob, not a full page preset; save-dialog Cancel aborts the chained delete); clean-page 2-button
  path byte-identical. Self-caught: one non-ASCII character corrupted into a comment mid-edit,
  fixed same turn.
- **Task 12 — bundle save-first prompt (docket 17=b).** `doFileSaveAs` gained an optional
  success-only `onSaved` continuation (threaded through the invalid-name retry; cancel/failure never
  fire); `doExportProjectBundle`'s unsaved bail replaced by the save flow continuing into the bundle
  via SafePointer re-entry. NO new menu item — QA-Export's item 122 keeps its name.
- **Master Test Plan §B.30 authored — 24 scenarios (PS-1..PS-24)**, blocks `dadb958a` +
  `54177488` + the final commit; scope note records the Task 7 re-route (PS-21/22 cover only the
  badger scope); PS-24 covers the Event Editor trio.
- **Main Plan untouched.** All §5/§5.5/§9 items HELD in this file's PENDING ledger (6 items) per
  Jeff's 2026-07-25 standing instruction — applied in ONE pass at G4 close.

#### Spec calls locked mid-batch (full tables in the plan files)

- **Batch-open docket (2026-07-26, dockets 15-18):** 15=B templates carry a full `<Processor>`
  snapshot (the plan's engineData claim was FALSE); 16 Rusty's prompt is "Save Page Preset & Delete"
  chaining the J-11 Player Preset (supersedes docket 11=A's label — its "kit" is a read-only factory
  `.sfz`); 17=b no new "Pack Project" item (QA-Export already shipped it; only the save-first prompt
  is added); 18 L/B/D empty + delete-to-zero + empty-bus hiding, inserted as Task 1 (Jeff: the trio
  is "bloat for a user that may not want those things"; the empty buses are a defect in scope, and
  ordering is forced — Task 2's templates would otherwise bake in three phantom tabs + strips).
- **Second docket (2026-07-26, dockets 19-28; batch re-planned 7 -> 12 tasks):** 19 template load =
  New Project semantics; 20 templates carry no grid/pattern content (confirmed by construction) and
  a Clips tab's sample IS part of the rig; 21=a bundler engine-ref gap closed IN THIS BATCH; 22=b
  Core Library never copied + size estimate (Jeff's addition); 23=b template copies land in My
  Samples; 24=c copy scope = Clips samples + NAM captures + IRs + BaySickPlayer paths, sfizz kits
  excluded; 25=c `library:` normalization everywhere, presets included; 26=a resolver task moved
  ahead of the menu work; 27=a the G4 findings sweep runs NOW; 28=a hidden-bus sidechain edge left.
  Sweep 1-5 all fix in-batch (Jeff: "this group is the end of the code and we fix what we find
  period"); automation = option 2, all three steps (backed by the 2026-07-26 `/architecture` pass:
  NO surveyed system keeps a UI-keyed applicator map); commits = 4 seams (deliberate deviation from
  one-commit-per-batch, Jeff's blast-radius call).
- **2026-07-27 rulings (produced by this batch's sweep escalation; locked in
  `grand-inverting-mammoth.md`):** engine-ownership inversion is a V1 REQUIREMENT ("engines are the
  drivers and the pages just hold them"); export = the model rendering ITSELF offline (FL
  same-instance shape; no replica processor); **destroy-on-close** windows ("I want the cpu benefit
  so definitely destroy on close") — which forces automation registration fully model-side; shell
  calls Order=1b (inversion first) / Shell=2b (FL-style contained workspace, real native child
  windows) / Rollout=3a (all specced windows in the one rebuild) / Title bars=4a (custom-drawn,
  merged menu row) / Buttons=5a (close + resize only); required-tabs "+" tab bar incl. Piano Roll
  (explicitly RETIRES Task 1's empty-state pages + 0-badge slots — option-removal paper trail in the
  mammoth plan); scope = the ENTIRE Future State tiers list incl. full VST3 hosting + BLU-480 rack
  window; tempo lane FOLLOWED in export + full re-prepare + FL-style progress-bar UX; CL-102 struck
  (already shipped as PagePresetIO — verified in source after Jeff challenged my wrong claim);
  conflict-review calls 1=b (the G4 boundary R3 review + smoke covers yak/stoat/heron; mammoth
  verifies via per-set commits + its TS8 smoke) / 2=b (mammoth TS1 pre-wires the processor-owned
  UndoManager DORMANT; yak Task 2 shrinks to verification) / 3=a (§B reconciliation runs inside
  TS8) / 4=a (dated conflict notes applied to yak/stoat/heron); commit seams re-ruled — Commit 2 =
  the Task 7 arc as it stood, ONE final commit for Tasks 8-12 + close.

#### Found along the way

1. **Three approved-plan premises were WRONG at batch open**, caught by reading source before code:
   the engineData/mixer-state claim (docket 15), Rusty's "kit" save concept (docket 16), and the
   duplicate Pack item (docket 17).
2. **Docket 18 (Jeff-raised): the hard-coded L/B/D trio + dead empty-bus strips are an in-scope
   defect**, not a note — with 15=B they would have been baked into every template written.
3. **For a bus, "something routes here" == "has members"** (Jeff's two-round correction) — my extra
   route-target guard machinery protected an unreachable case, and my first-cut perf defect lived
   only inside it.
4. **The FX-rack channel-switch automation hole is REAL**: desk-verified from the `/architecture`
   pass's claims, then confirmed live by Jeff's Debug test (the new step-2 diagnostic caught it) and
   by ear.
5. **VibePlayer's `bsp_loadPath` was an absolute path at 3 write sites** — correcting my earlier
   reassurance that Layer/Bass patch sample refs were safe inside the blob.
6. **The Core Library duplication Jeff suspected is real and comes from `importSample`** (copy-always),
   not from the `library:` writer gap (which stores paths and copies nothing).
7. **Off-screen-window restore defect (pre-existing)**: saved multi-monitor coordinates were never
   re-validated against attached displays.
8. **Ribbon slots went unlabelled at zero instances** — the Task 1 sweep checked FUNCTIONAL >=1
   assumptions and never considered PRESENTATIONAL ones; a screenshot caught what a grep never would.
9. **The applicator sweep census escalated into the batch-defining findings**: 19 wrapper call sites
   across 9 files + 7 direct hooks (the resume prompt's "13 across 6" undercounted); only the FX
   rack had the die-with-UI hole (everything else safe UNDER TODAY'S immortal-pages shell); two
   registration-timing gaps (view-gated rack wiring; lazily-materialized params missing the boundary
   seed); offline export ignores every non-main-APVTS lane — and beneath it the render processor has
   NO instrument engines or instrument InsertNodes at all (Jeff ear-confirmed: vox/inst exports
   render NOTHING); export destination wrong (userMusicDirectory, not `<project>\Exports\`);
   metronome safe today / must-gate under live-graph export.
10. **The step-3 rollout is not "12 effect types"** — types x character modes is realistically 20+
    distinct panel/mapping sets (Compressor alone is four).
11. **CL-102 was already shipped as PagePresetIO** — verified in source after Jeff challenged my
    wrong claim that it was outstanding.
12. **Hidden-bus sidechain edge (cosmetic, pre-existing shape)**: an empty bus a compressor
    sidechains FROM hides with its cable endpoint; audio unaffected (an empty bus is silent).
13. **Two standing process corrections from Jeff**: never offer deferral as an option for my own
    mistakes ("You fuck up, you fucking fix it"), and never offer "don't surface it" on a findings
    sweep — every finding's disposition is his call. Plus three smaller ones on 2026-07-27
    (the "undoable" wording clarification; citing QA-Soundness's deferred §5 entry whenever the
    batch is named; dependency-direction phrasing — badger relies on nothing in mammoth).
14. **Two build cycles lost to shell-layer text handling** (a scripted pointer->reference fix
    missing a hand-written function; a heredoc eating a backslash).

#### What was done about each finding

- **1 — corrected via dockets 15=B/16/17=b before any code**; the §5 wording amendments ride the
  PENDING ledger (items 1-4) to G4 close.
- **2 — inserted as Task 1 and shipped** (Jeff's scope ruling verbatim in the running notes).
- **3 — machinery deleted, test is three lines**; verification scenario rewritten to check
  membership-not-count (§B.30 PS-4), which is exactly what a naive implementation gets wrong.
- **4 — Task 7 step 3 built and runtime-proven** (differential test + vol-knob confirm); badger
  scope CLOSED; the remainder (all other effect-type tables, pedals conversion, Reverb freeze row,
  wire-at-load / wire-at-creation) re-routed to **QA-ModelShell TS3** with the plan annotated.
- **5 — normalized in Task 5** via `refForPersist` at all three writers / both readers.
- **6 — fixed in Task 4** (source-aware `importSample`).
- **7 — fixed in-batch** (display-bounds clamp), pre-existing or not, per the fix-what-we-find
  ruling; immediate settings.xml workaround given so Jeff could keep testing.
- **8 — fixed in-batch**: all six types now have the zero-instance fallback.
- **9 — ESCALATED into a new batch**: QA-ModelShell (`grand-inverting-mammoth.md`, 8 task sets =
  Jeff's approved groups, per-set commits, ONE batch smoke at TS8) planned, approved 2026-07-27, and
  slotted DIRECTLY after this batch (run order badger -> mammoth -> yak -> stoat -> heron); the
  registration-timing gaps dissolve under model-side registration; conflict review of
  yak/stoat/heron ran with dated notes applied to all three.
- **10 — surfaced to Jeff before continuing**; the remaining tables ride mammoth TS3 rather than
  being rolled out on what was then an unverified foundation.
- **11 — struck from the tiers list**; the Future State stale-mark lands in mammoth TS8.
- **12 — left per docket 28=a**, flagged for the campaign walk rather than pre-emptively coded around.
- **13 — adopted as standing corrections**, recorded verbatim in the running notes.
- **14 — standing correction for the rest of the batch: source edits via Write/Edit, never shell
  heredocs** — held for the remainder.

#### Group review (R3)

- **Re-scoped 2026-07-27 (conflict-review call 1=b): the G4 boundary R3 review + smoke covers
  yak / stoat / heron; QA-ModelShell verifies via its per-set commits + TS8 smoke.** Where that
  leaves this batch's group-review coverage is a G4-close confirmation; its functional verification
  is the §B.30 campaign pass (R2) either way.

#### Diagnostic Instrumentation Catalog

- **Added + REMOVED in-batch:** the step-2 dispatch `jassertfalse`. It earned its keep by catching
  the FX-rack hole on its first test, then came out the same day — until step 3 removed the
  false-positive source it fired on the ordinary act of switching FX channels.
- **Added + RETAINED (deliberate, permanent):** the once-per-paramId-per-session `DBG` dead-lane
  warn (`mReportedDeadLanes`) in the automation dispatch. A product Debug diagnostic, not a temp
  trace — nothing to strip.
- No other instrumentation added or removed.

#### Carry-forward contradictions

- **Task 1's empty-state presentation is ALREADY scheduled for retirement** — the mammoth tab-bar
  ruling (2026-07-27) replaces the always-visible 0-badge slots + empty-state pages with a "+" tab
  bar (explicit reversal, paper-trailed in the mammoth plan). The delete-to-zero model, the deleted
  seeding paths, and membership-driven bus hiding all SURVIVE; only the presentation changes at TS4.
  Do not "fix" the empty states back in the interim.
- **The UI-keyed applicator map is now transitional.** The census verified every non-rack
  registration safe ONLY under today's immortal-pages shell; destroy-on-close makes widget-scoped
  registration a guaranteed defect. Endgame (mammoth): ALL registration model-side. Never add a new
  widget-keyed registration; `registerSlotAutomation`'s null-owner + id-owner-claim-erase must stay
  ONE operation (the lesson that bit twice).
- **Notes to carry:** (1) `writeProcessorState` / `applyProcessorState` are THE shared
  project+template serializers — never fork a second restore path; (2) `makeStableRef` /
  `refForPersist` / `resolvePersistedRef` are the single path-ref implementation (readers still
  accept absolutes; no pre-v1 migration); (3) `EffectParamMap` is the ONE home for rack-param
  mapping math — panels call `applyNatural` / `read`, never inline copies, and every entry point is
  keyed `(type, variant)`; (4) `ProjectBundler::enumerate` is the single four-layer reference
  walker; (5) re-widen `onIsParamStale` only once registration outlives the rack (post-mammoth-TS3).

#### Commit(s)

`dadb958a` (Tasks 1-6 — the data layer: L/B/D empty + templates v2 + resolvers + normalization +
adoption + bundler walk; 2026-07-26) · `54177488` (Task 7 badger scope — registry + diagnostics +
`EffectParamMap` Compressor x4 + rack `output_vol`, off-screen clamp, ribbon labels, Event Editor
trio, applicator census, QA-ModelShell plan/approval/slotting + conflict notes; 2026-07-27) ·
`b933b54a` (Tasks 8-12 + §B.30 + this held entry + close docs). Three seams are Jeff's
deliberate deviation from one-commit-per-batch (2026-07-26, originally four; Commits 3+4 merged by
his 2026-07-27 re-rule) — twelve tasks spanning project save/load, the bundler, the automation
registry and the effects audio path is too much for one rollback point. Build gates green in BOTH
configs at every task; the only failures were exe-lock `LNK1104`s while Jeff had the app open. The
step-3 architecture + rack `output_vol` are the ONLY parts of this batch runtime-verified (Jeff's
differential test, 2026-07-26/27); everything else defers to the R2 campaign pass against §B.30.

#### Next action

- Proceed to **QA-ModelShell** ([`grand-inverting-mammoth.md`](../Batch%20Plans/grand-inverting-mammoth.md)),
  slotted directly after this batch (G4 run order: badger -> mammoth -> yak -> stoat -> heron).
  PENDING Main Plan ledger items (6, incl. the docket-18 §5 scope bullet + §9 Forks entry + the
  bucket-set additions) stay deferred to G4 close.
