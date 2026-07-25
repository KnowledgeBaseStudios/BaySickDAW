# Running Notes — QA-L-Fix (eager-thumping-marmot)

> **Purpose:** append-only running log for the QA-L per-drum-MIDI-kit-trigger fix.
> Append an entry at EVERY checkpoint — a task's build gate cleared, a finding captured,
> a spec call asked/resolved, a scope pivot, a commit landed — not just at the end
> (`feedback_draft_doc_running_notes_every_checkpoint`). At close, `/draft-doc batch-close`
> reads this file as the primary input for the single Implemented Work Log entry, which is
> **drafted and HELD** here until the §B.18 campaign pass (bulk-run R2).
>
> **Pair file:** [`Plans & Specs/Batch Plans/eager-thumping-marmot.md`](../Batch Plans/eager-thumping-marmot.md)
> **Conventions:** Main Plan §0 (three-doc system, Rules 1-9; Batch Plans + Running Notes layout).

## 2026-07-19 — Batch opened (design locked, not yet executed)

Design workshopped and locked with Jeff on 2026-07-19 at the G3 boundary; the full D-1..D-14
decision table lives in the plan file. Root cause captured there too: the shipped QA-L feature
(`2e2df50a`) was note-only and gated on drum-tab focus, so it fires nothing on Jeff's CC pads and
the kit-trigger workflow it existed for was unreachable. Jeff's call: this is a **defect fix, not
new scope**, and it **blocks the G3 boundary commit**.

Master Test Plan already updated ahead of execution: §B.18 L-8 marked SUPERSEDED, new **L-9..L-14**
authored for the redesign (bulk-run R4 — verify scripts live in the test plan, not run mid-batch).

**State at open:** the 12 G3 boundary review-fixes are in the working tree and build clean
(Debug + Release, confirmed by Jeff 2026-07-19) — **do not disturb them**. Three doc stragglers
(§B.21 hash, `locked-doubling-frog` running notes + carry-over) are also dirty and ride the
boundary commit.

**Resume action:** execute Task 1 of the plan file.

## 2026-07-19 — Task 1 — Menu split + "MIDI Note" becomes the play pitch (code complete, build gate pending)

Task 1 is **code complete but NOT yet built** — the build gate is Jeff's (`do_build.bat`). Nothing
committed this batch; HEAD is still `d6abc38b`.

**Param rename + semantics flip (D-4 / D-5).** `mixer_drum_{N}_inputNote` is now
`mixer_drum_{N}_playNote`, range flipped from `-1..127` (default `-1` = unmapped) to `0..127`
(default `60` = C5) at [PluginProcessor.cpp:5692](Source/PluginProcessor.cpp:5692). The meaning
flipped with it: no longer an *input filter*, now **the drum's play pitch**. Member
`mDrumInputNotePtr` → `mDrumPlayNotePtr` ([PluginProcessor.h:1001](Source/PluginProcessor.h:1001)),
updated in `registerDrumEngine` / `unregisterDrumEngine`
([:5342](Source/PluginProcessor.cpp:5342), [:5374](Source/PluginProcessor.cpp:5374)).

**Menu split (D-2).** `DrumPage::showContextMenu` gained a `bool fromKit` parameter
([DrumPage.h:121](Source/Standalone/DrumPage.h:121)). Page callers pass `false`
([DrumPage.cpp:251](Source/Standalone/DrumPage.cpp:251) picker click,
[:1029](Source/Standalone/DrumPage.cpp:1029) right-click listener); both kit callers pass `true`
([StandaloneEditor.cpp:6211](Source/Standalone/StandaloneEditor.cpp:6211),
[:6339](Source/Standalone/StandaloneEditor.cpp:6339)). The whole MIDI block is kit-gated — pages
show neither MIDI item.

**Menu contents (D-3 / D-5).** Parent submenu label stays **"MIDI Note"**. The "Unassigned" item is
gone, replaced by a disabled **"Assigned: \<note\>"** status row at the top of the submenu, then a
separator, then the octave submenus.

**Single mutation path.** New `DrumPage::getPlayNote()` / `setPlayNote()`
([DrumPage.cpp:1032](Source/Standalone/DrumPage.cpp:1032),
[:1040](Source/Standalone/DrumPage.cpp:1040)) so Task 2's D-10 learn-a-note prompt reuses one path
instead of duplicating the write. `setPlayNote` writes the param, then fires the new
`onPlayNoteChanged(pageIdx, oldNote, newNote)` callback
([DrumPage.h:103](Source/Standalone/DrumPage.h:103)).

**Kit grid stamps the assigned note.** `DrumKitGrid` gained `setApvts()`,
`playNoteForPage(pageIdx)` ([DrumKitGrid.cpp:645](Source/Standalone/DrumKitGrid.cpp:645) — falls
back to `kKitMidiNote` = 60 when the strip param is not registered yet), and `repitchDrumHits()`.
All three hit-stamp sites (Paint tool, Paint drag, Draw commit —
[:1465](Source/Standalone/DrumKitGrid.cpp:1465),
[:1627](Source/Standalone/DrumKitGrid.cpp:1627),
[:1861](Source/Standalone/DrumKitGrid.cpp:1861)) now stamp the drum's assigned note instead of a
hardcoded 60; all three are `pi >= 0` guarded. The retune dot in `paint()` keys off the drum's
assigned note rather than fixed C5, with the `playNoteForPage` lookup hoisted per-row
([:2219](Source/Standalone/DrumKitGrid.cpp:2219)) — it is a by-string APVTS lookup and has no
business inside the per-note paint loop.

**Re-pitch on assignment change (D-6).** `repitchDrumHits` moves **only** the hits sitting at the
OLD play note, wrapped in the existing `beginEdit` / `commitEdit` + `DrumKitSnapshot` so one Ctrl+Z
restores. It skips creating an undo entry entirely when no hits sit at the old note. Hits
deliberately placed at other pitches are left alone, per the locked call.

**Wiring.** `DrumPage::onPlayNoteChanged` is wired in `StandaloneEditor::wireDrumPageKitView`
([StandaloneEditor.cpp:6139](Source/Standalone/StandaloneEditor.cpp:6139)) to the PianoRollPage kit
container's `repitchDrumHits` + `refreshAllKitViews()`. Routed to the **PianoRollPage** kit
specifically because that grid owns the `drumRolls` undo stack; both kit views share the same
dataset, so the other view picks the change up through the refresh.
`DrumKitContainer::setApvts` now forwards to the grid as well as the sidebar, and a
`DrumKitContainer::repitchDrumHits` forwarder was added
([:3326](Source/Standalone/DrumKitGrid.cpp:3326)).

### Spec call asked and resolved — kit-row audition pitch (NEW, not in the D-1..D-14 table)

Surfaced during Task 1: the two kit-row press-and-hold audition dispatchers hardcoded note 60 with
the comment "60 = C5 = the kit-grid placement note" — the exact assumption Task 1 just made
per-drum. Left as-is, a drum assigned to D5 would **preview** at C5 but **play** D5 everywhere else.

- Question: should the kit-row audition follow the drum's assigned play note, or stay hardcoded C5?
- Options: (a) audition follows `getPlayNote()`; (b) stays hardcoded C5; (c) something else.
- **Jeff's answer (2026-07-19): (a).**

Implemented in both dispatchers ([StandaloneEditor.cpp:6166](Source/Standalone/StandaloneEditor.cpp:6166),
[:6291](Source/Standalone/StandaloneEditor.cpp:6291)). Added a `std::shared_ptr<int> heldNote` latch
captured by value into the dispatch lambda: the pitch is read at **press** and reused at **release**,
because `auditionNoteOff` targets a specific note — releasing against a freshly-read (possibly
changed) assignment would strand a voice. The `shared_ptr` is what makes the latch shared across the
two handler copies, since the on/off handlers each capture `auditionDispatch` by value.

### Judgment call — Task 2's fan-out deletion pulled forward into Task 1

The plan put the deletion of the `kind == 3` kit fan-out block
(`PluginProcessor.cpp:2749-2766`) in Task 2. **It was deleted in Task 1 instead.** Reason: the moment
`_playNote` defaults to 60 for every drum, that block fires **all 16 drums** on any incoming C5 while
a drum tab is focused. Handing Jeff a knowingly-broken intermediate at the build gate was not worth
the tidier task boundary. Same deletion the plan already called for, one task earlier; Task 2 still
builds the replacement dispatch. Flagged to Jeff in chat at the time — no objection.
Post-change, only the routing-destination line survives at
[PluginProcessor.cpp:2713](Source/PluginProcessor.cpp:2713).

### Verification

Inline self-check only, per the standing no-per-unit-review-agents rule (`/review-batch` runs once at
Task 4 close). Swept for stale `inputNote` references — **zero remain**. Confirmed: every
`showContextMenu` caller updated; all declaration/definition pairs match; `mApvts` landed in the
private section; all three hit-stamp sites `pi >= 0` guarded; the `<memory>`-provided symbols the
`shared_ptr` latch needs are already in use in `StandaloneEditor.cpp`.

**Files touched (Task 1):** [Source/PluginProcessor.h](Source/PluginProcessor.h),
[Source/PluginProcessor.cpp](Source/PluginProcessor.cpp),
[Source/Standalone/DrumPage.h](Source/Standalone/DrumPage.h),
[Source/Standalone/DrumPage.cpp](Source/Standalone/DrumPage.cpp),
[Source/Standalone/DrumKitGrid.h](Source/Standalone/DrumKitGrid.h),
[Source/Standalone/DrumKitGrid.cpp](Source/Standalone/DrumKitGrid.cpp),
[Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp).

**Commits:** none this batch — nothing committed yet, HEAD `d6abc38b`.

**State:** Task 1 code complete, **not yet built**. Build gate is Jeff's.

**Resume action:** after a clean Debug + Release build, execute Task 2 — `DrumTriggerMap` (binding
store + capture + persistence), MIDI Learn note/CC capture, trigger dispatch replacing the deleted
fan-out, and the kit-focus atomic.

> *Correction (2026-07-20, append-only doc so noted here rather than edited above):* the audition
> dispatcher line numbers cited in this entry (`:6166` / `:6291`) drifted when Tasks 2-3 landed;
> current locations are [StandaloneEditor.cpp:6176](Source/Standalone/StandaloneEditor.cpp:6176) and
> [:6301](Source/Standalone/StandaloneEditor.cpp:6301). Also: the "kit-focus atomic" named in the
> resume action above was **never built** — `mLiveMidiTargetKind == 0` was reused instead. See the
> D-16 entry below.

## 2026-07-19 — Premise correction — the batch's stated root cause was wrong

The plan's Context section claimed **"Jeff's drum pads send CC, not notes"** and that the QA-L
feature therefore **"fires nothing on the target hardware."** **That was wrong**, and it was wrong in
a way that reached into the D-table (D-8 split note vs CC on the assumption CC was the primary path).

Hardware-tested on the target controller (Novation FLkey 61) using shipped app behavior, two tests:

- **Test A** — right-click a knob -> MIDI Learn -> hit a pad -> **no capture**. The learn queue admits
  only CC / pitch-bend / channel-pressure (`MidiLearnRegistry::buildMappingFromEvent`,
  [MidiLearnRegistry.cpp:254](Source/MidiLearn/MidiLearnRegistry.cpp:254) — the `MessageType` enum has
  no Note case at all). No capture on a pad hit therefore proves the pads are **not sending CC**.
- **Test B** — select a Layer with an instrument in the Piano Roll -> hit a pad -> **it plays**. Proves
  the pads **are sending notes** and that those notes do reach the app.

Matches Novation's own documentation: FLkey pads are Note-based in every mode. The CC "Type" selector
that made the CC premise plausible exists on the sibling **Launchkey MK4**, not FLkey. A doc-research
sweep predicted this before the hardware test confirmed it — the test was run to settle it, not to
discover it.

**The defect is real regardless.** The shipped fan-out was gated on drum-tab focus (`kind == 3`) AND
skipped the focused drum (`if (di == idx) continue;`), so with the Drum Kit selected — the surface
you actually play a kit from — it did nothing no matter what the pads send. That reasoning is
hardware-independent. **Only the stated cause was wrong, not the conclusion.**

Plan Context corrected in place with a **"Premise correction"** blockquote
([Batch Plans/eager-thumping-marmot.md](../Batch Plans/eager-thumping-marmot.md)).
Consequence re-posed to Jeff before implementation — see **D-16** below.

## 2026-07-19 — Spec calls — D-16 (re-look at D-8) + two dispatch-path decisions

Two spec calls this batch that were **not** in the original 2026-07-19 workshop. D-15 (kit-row
audition pitch) is already recorded in full under the Task 1 entry above — Jeff answered **(a)
audition follows `getPlayNote()`**, implemented in both dispatchers with a `std::shared_ptr<int>
heldNote` latch (pitch read at press, reused at release, because `auditionNoteOff` targets a specific
note and releasing against a changed assignment would strand a voice; the `shared_ptr` is what shares
the latch across the two handler copies).

### D-16 — re-look at D-8 after the premise correction (asked + answered 2026-07-19)

D-8 split note triggers from CC triggers on the assumption CC was the **primary** path and the
focus-gated note path was the fallback. Notes turned out to be the **only** path on this hardware,
which inverts that: D-8's focus gate now governs the **whole feature** rather than a fallback branch.

- Question: with notes as the only trigger path, does the focus gate stand?
- Options: **(a)** notes fire only while the Drum Kit is the focused engine (D-8 exactly as locked);
  **(b)** fire globally, scoped to the pads' MIDI channel; **(c)** fire globally with the note
  consumed so the focused engine never sees it.
- **Jeff's answer (2026-07-19): (a).** D-8 stands unchanged.

Noted for later: option **(b)** only becomes viable if the pads transmit on a **different channel**
than the keys. The new MIDI Learn menu label reads back each binding's channel
(`describeBinding` appends ` omni` / ` chN`, [DrumTriggerMap.cpp:96](Source/MidiLearn/DrumTriggerMap.cpp:96)),
so that is checkable at the G3 boundary smoke without any instrumentation. If the channels do differ,
(b) is a one-condition change to the note branch at
[PluginProcessor.cpp:5445](Source/PluginProcessor.cpp:5445).

### Also settled 2026-07-19 (implementation calls, not spec calls)

- **Dispatch path = the live-MIDI loop**, not the block-rate learn-queue drain. The live loop
  preserves `m.samplePosition`; drum hits are the most timing-sensitive events in the app. Rationale
  recorded at [PluginProcessor.cpp:2755-2758](Source/PluginProcessor.cpp:2755).
- **Kit-focus signal = reuse `mLiveMidiTargetKind == 0`** (`PianoRollPage::EngineKind::DrumKit`)
  rather than adding a parallel atomic and a second publish path from `StandaloneEditor`. The
  processor already resolves that value once per block at
  [PluginProcessor.cpp:2715](Source/PluginProcessor.cpp:2715); it is passed straight into the
  dispatcher. This **supersedes** the plan's Task 2 line "publish a drum-kit-focused flag from
  StandaloneEditor (atomic)" — no new flag was added.

## 2026-07-19 — Research input that drove the dispatch-path decision

A verified multi-agent sweep (29 agents; 24 load-bearing claims adversarially checked, most
corrected) fed the dispatch-path call. What it established:

- Shipping DAWs **preserve intra-block sample offsets for live MIDI**, and treated block-rate
  application as a **defect to fix**, not a shipping tradeoff — 15 of 18 surveyed hosts had done so by
  2012. That settled the live-MIDI loop over the learn-queue drain.

It also **corrected two of my own framings**, both of which had been stated in chat as fact:

1. **`MidiMessageCollector` is NOT sample-exact.** It reconstructs sample position from wall-clock
   deltas, and on WinMM the timestamps are whole milliseconds. The real comparison is **~1 ms vs
   ~10.7 ms** (one 512-frame block at 48 kHz), not "exact vs 10 ms". The live path still wins by an
   order of magnitude, but the reason it wins is not the one I gave.
2. **Per-event device identity is not something major DAWs carry at all.** They resolve device
   identity **once at the port** — FL port numbers, Ableton input streams, Bitwig controller scripts —
   never per event. That correction is what collapsed the decision: if nobody carries per-event device
   identity, the live path's loss of it is not a tradeoff worth designing around.

Consequence recorded in the header of the new module
([DrumTriggerMap.h](Source/MidiLearn/DrumTriggerMap.h)): device scoping is **not carried**;
bindings match on type + number + channel only, and channel scoping covers the collision that
actually occurs (pad note ranges overlapping keyboard ranges).

## 2026-07-20 — Task 2 — DrumTriggerMap + MIDI Learn + trigger dispatch (started 2026-07-19)

**Builds clean, Debug + Release, confirmed by Jeff.** Nothing committed; HEAD is still `d6abc38b`.

**New module.** [Source/MidiLearn/DrumTriggerMap.h](Source/MidiLearn/DrumTriggerMap.h) +
[.cpp](Source/MidiLearn/DrumTriggerMap.cpp), registered at
[CMakeLists.txt:523](CMakeLists.txt:523). Sibling to `MidiLearnRegistry` per D-13, deliberately not
built on it.

**Binding shape — `deviceName` REMOVED from the plan's sketched struct.** The plan specified
`{ Kind, number, channel, String deviceName }`; what shipped is
`{ Kind (None/Note/Cc), number, channel }`. The live-MIDI path carries no device identity, so a
`deviceName` field could only ever be empty — that is the tradeoff the dispatch-path decision
accepted, recorded here explicitly as a **removal** rather than an omission. Bindings match on
type + number + channel; channel 0 = omni.

**Audio-thread contract.** The audio thread reads **one lock-free `std::atomic<uint32_t>` per drum**
(kind in bits 0-1, number+1 in bits 2-9, channel in bits 10-14). No locks, no allocation, no
`juce::String` anywhere on that path. A binding edit is one word, so it can never be seen
half-applied. Message thread mutates under `mLock` and republishes with a release store.

**Learn capture never calls a `std::function` on the audio thread.** The audio thread packs the
captured event into an atomic slot and sets a ready flag (`tryCaptureLearnRT`); DrumPage's
**existing 24 Hz page timer** polls `takeCapturedBinding` and commits on the message thread
(`pollTriggerLearn`). Capture ignores note-off and CC value 0 — both are release events, and binding
on let-go would (worse, for a pad idling at 0) self-bind with no user gesture at all.

**Kit menu (D-3).** "MIDI Learn" sits **directly below** "MIDI Note". The label reads the binding
back — `MIDI Learn: D5 ch10` when set, plain `MIDI Learn` when not — and a
**"MIDI Forget: \<binding\>"** item appears only once a binding exists.

**D-10 prompt on NOTE capture only:** "Also set this drum's play note to \<note\>?" Yes routes through
the Task-1 `setPlayNote` path, so the D-6 re-pitch + undo come along for free. **No prompt on CC** —
a CC carries no pitch, so there is nothing to offer.

**Dispatch** (`dispatchDrumTriggers`, called from the live-MIDI loop). Learn capture runs first and
`continue`s so the pad the user just bound doesn't also sound the focused engine on that same hit. CC
triggers fire **regardless of focus**; note triggers fire **only while the kit is focused** (D-8).
Either kind fires the drum at its **assigned play note** (`drumPlayNoteRT`) — the learned note/CC is
only ever the input, never the pitch (D-9).

**CC release with a safety timeout** (D-11 "safest", Jeff 2026-07-19). CC > 0 fires, CC = 0 releases,
and a **1 s countdown force-releases** so a pad that never sends a release cannot strand a voice
(`kCcTriggerMaxHoldSeconds = 1.0`; countdown in `tickCcTriggerHolds`, ticked every block regardless of
whether live MIDI arrived). **`PRPendingOff` could not be reused** for this: it is keyed on
**absolute BEAT** and therefore does nothing at all while the transport is stopped — which is exactly
when someone jams on pads.

**Persistence (D-14).** Bindings save with the **project** under `<DrumTriggers>`. An **absent** child
calls `clearAll()` rather than leaving the previous state in place — loading a project with no
triggers must not inherit the last project's kit mapping.

**Trigger holds cleared on the Stop-button flush**, alongside the existing piano-roll pending-off
clear, so Stop kills held CC and note triggers the same way it kills everything else.

### Self-check caught three bugs in the new code (before the build gate)

Inline self-check per the standing no-per-unit-review-agents rule. All three were live defects in
code I had just written, found by reading it back rather than by the compiler:

- **`AlertWindow` double-free.** `enterModalState (..., deleteWhenDismissed = true)` fighting a
  `unique_ptr` member — JUCE deletes the window, then the smart pointer deletes it again. Switched to
  `juce::Component::SafePointer<juce::AlertWindow>` so JUCE owns the lifetime and the pointer nulls
  itself.
- **Cross-fire between Note and CC bindings.** A `Note 42` binding matched an incoming `CC 42` because
  the **number was compared before the message type** — both carry "42" and mean entirely different
  things. It emitted a spurious note-off on every matching CC. Type is now checked first.
- **Note release used the CURRENT play pitch, not the pitch the hit started at.** Re-assigning a
  drum's play note mid-hold would send note-off for the new pitch and strand the original voice
  sounding forever. Added a per-drum held-note array (`mNoteTriggerHeld`, initialized to -1). The CC
  branch has the same latch via `hold.note`.

**Files touched (Task 2):** [Source/MidiLearn/DrumTriggerMap.h](Source/MidiLearn/DrumTriggerMap.h)
(new), [Source/MidiLearn/DrumTriggerMap.cpp](Source/MidiLearn/DrumTriggerMap.cpp) (new),
[CMakeLists.txt](CMakeLists.txt), [Source/PluginProcessor.h](Source/PluginProcessor.h),
[Source/PluginProcessor.cpp](Source/PluginProcessor.cpp),
[Source/Standalone/DrumPage.h](Source/Standalone/DrumPage.h),
[Source/Standalone/DrumPage.cpp](Source/Standalone/DrumPage.cpp).

## 2026-07-20 — Real-time fixes in the existing MIDI Learn subsystem (I-3b, pre-existing)

**Not introduced by this batch.** These are real-time violations in the I-3b MIDI Learn subsystem
(shipped 2026-05-02), surfaced while reading that code as the pattern reference for `DrumTriggerMap`.
Originally surfaced as findings and **initially routed to §9 Forks**; **Jeff then directed they be
fixed in-batch**, so **there is no Forks entry — they are fixed in the tree** and belong in the
Implemented Work Log instead.

**1. Heap free on the audio thread.** `MidiLearnEventQueue::Event` held a `juce::String deviceName`,
and the drain's `mDraining.clear()` ran `~juce::String` **inside the render callback**. This genuinely
freed, not theoretically: `MidiInput::getName()` returns **by value**, and the MIDI-thread local died
when its callback returned, leaving the queue the **sole owner** — so `clear()` dropped the last
reference and called `free()`.
**Fixed** by interning device names to integer ids in a fixed, never-resized table (`Event` now holds
`int deviceId`; `internDeviceName` / `deviceNameForId`). The drain hands the consumer a `const&`
**into** that table, so no refcounted type is constructed or destroyed on the audio thread. Consumer
signature unchanged. Also pinned the invariant that makes the rest of it safe: `juce::MidiMessage`
heap-allocates only above 8 bytes, and everything this queue admits (CC / pitch-bend /
channel-pressure, 2-3 bytes) lives inline — **widening the push filter to sysex would reintroduce an
audio-thread free.** Filter before pushing, not after draining.

**2. Per-event allocation in `dispatchEvent`.** It built a
`std::vector<std::pair<juce::String, MessageType>>` per dispatched event — a fresh heap allocation
plus a string copy in the render callback, continuously, for the whole duration of a knob sweep.
**Fixed**: fixed stack array of already-resolved `juce::RangedAudioParameter*`. Resolving **inside**
the lock is what makes releasing it safe — APVTS parameters outlive the app, whereas a pointer into
the map's keys would dangle if the message thread erased that mapping. Capped at 16 hits per event;
one event firing 17+ mappings means the user bound 17 params to a single CC, and the tail is
**dropped rather than allocated for**.

**3. Four violations in `tryCaptureLearn`** — a `std::function` copy, the callback's own
`MessageManager::callAsync`, a `std::map` insert, and an `onChanged()` notify, all on the audio
thread. **Fixed**: the capture callback is **removed entirely**. The audio thread fills a
pre-constructed `Mapping` under the try-lock it already held and sets a flag; `MidiLearnUI`'s timer —
now **20 Hz**, with the 30 s cancel converted from a one-shot into a **deadline compared per tick** —
does the commit. Same handshake shape as `DrumTriggerMap`, arrived at independently and then
deliberately aligned.

**4. A fourth defect, caught inside my own fix during self-check.**
`mPendingCapture.paramId = mLearnTargetParamId` **releases its previous value** — and if the mapping
that paramId belonged to had since been removed via MIDI Forget, that release drops the last reference
and frees **in the render callback**: the exact same defect one level down, reintroduced by the fix
for it. The clear now happens on the **message thread**, in `takeCapturedMapping` and `beginLearn`, so
the audio thread only ever assigns into empty strings.

**Two wrong comments fixed while in those regions** (Rule 6 cleanup clause — these were factually
wrong, not merely non-conforming):
- [StandaloneApp.cpp](Source/Standalone/StandaloneApp.cpp) claimed `addMessageToQueue` is wait-free.
  It takes `MidiMessageCollector::midiCallbackLock` and briefly contends with the audio thread's
  `removeNextBlockOfMessages`.
- [MidiLearnRegistry.h](Source/MidiLearn/MidiLearnRegistry.h) claimed the lock was released before the
  drain loop. It is not — the try-lock is scoped to the whole function.
- A third, in [DrumTriggerMap.h](Source/MidiLearn/DrumTriggerMap.h), said the registry's heap free was
  "routed to Main Plan section 9 Forks rather than copied here." True when written, wrong once Jeff
  directed the in-batch fix. Corrected 2026-07-20.

**Files touched (RT fixes):** [Source/MidiLearn/MidiLearnRegistry.h](Source/MidiLearn/MidiLearnRegistry.h),
[Source/MidiLearn/MidiLearnRegistry.cpp](Source/MidiLearn/MidiLearnRegistry.cpp),
[Source/MidiLearn/MidiLearnUI.h](Source/MidiLearn/MidiLearnUI.h),
[Source/Standalone/StandaloneApp.cpp](Source/Standalone/StandaloneApp.cpp).

## 2026-07-20 — Task 3 — Global velocity toggle (D-11)

**Builds clean, Debug + Release, confirmed by Jeff.**

**"MIDI trigger velocity" submenu** in the **Mixer hamburger**, alongside Pan Law / Master Output /
Multi-core Rendering: **`From controller`** (default) / **`Fixed`**, ticked to reflect current state.
**Hot-swaps** — the next trigger picks up the new mode, no restart.

**Persisted to `settings.xml`** as `<MidiTriggerVelocity fixed="0|1"/>` using the same
sibling-preserving read-modify-write as `MultiCoreRendering`; loaded at startup before the processor
is constructed.

**Applied to BOTH trigger kinds** in the Task-2 dispatch: `From controller` uses note-on velocity for
note triggers or the CC value scaled to 0-1 for CC triggers; `Fixed` uses **0.8** for both —
`PianoNote`'s default velocity, i.e. exactly what a drawn kit hit gets (`kFixedVelocity`).

**The flag is a namespaced global atomic** (`DrumTriggerVelocity::gUseFixed`), matching
`RenderEngine::gMultiThreadedEngineEnabled` / `MeterLatencyComp::gEnabled`. **Not a stylistic choice**
— the preference loads before the processor is constructed, so it cannot live on the processor.

**Files touched (Task 3):** [Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp),
[Source/Standalone/StandaloneApp.h](Source/Standalone/StandaloneApp.h),
[Source/Standalone/StandaloneApp.cpp](Source/Standalone/StandaloneApp.cpp),
[Source/MidiLearn/DrumTriggerMap.h](Source/MidiLearn/DrumTriggerMap.h),
[Source/MidiLearn/DrumTriggerMap.cpp](Source/MidiLearn/DrumTriggerMap.cpp),
[Source/PluginProcessor.cpp](Source/PluginProcessor.cpp).

## 2026-07-20 — Scope note + batch state

**This batch grew past its plan.** As planned it was Tasks 1-3 on drum kit triggers. It now also
carries **three (four) real-time fixes in the I-3b MIDI Learn subsystem** — a genuinely separate
surface from drum kit triggers, touching different files, fixing pre-existing defects from a different
batch. That growth was Jeff's call (fix in-batch rather than route to §9), not scope creep, but it
should be visible at close rather than buried in a single entry.

**A commit split is available and is Jeff's call at the boundary commit:**
- **(A)** Tasks 1-3 — drum kit triggers (`DrumTriggerMap`, `DrumPage`, `DrumKitGrid`,
  `PluginProcessor` dispatch, velocity toggle).
- **(B)** MIDI Learn RT fixes (`MidiLearnRegistry.h/.cpp`, `MidiLearnUI.h`, `StandaloneApp.cpp`).

They are cleanly separable by file except for `PluginProcessor.h/.cpp` and `StandaloneApp.cpp`, which
both sets touch. Splitting those needs index-level staging (`git apply --cached`) so the working tree
always matches the verified state — **never** source edits to shape the hunks. Single combined commit
is equally fine; flagging the option, not recommending one.

**State.** All of Tasks 1-3 plus the RT fixes are **code complete and build clean (Debug + Release,
confirmed by Jeff)**, and **uncommitted by design** — this is G3 boundary work riding Jeff's boundary
commit. HEAD is still `d6abc38b`. **Verification is NOT run mid-batch** (bulk-run R4): the scenarios
live in [`v1-master-test-plan.md`](../Test Plans/v1-master-test-plan.md) §B.18 as
**L-9..L-14** (L-8 SUPERSEDED), and get walked at the **G3 boundary smoke** plus the §B.18 campaign
pass.

**Resume action:** Task 4 close — `/review-batch` over the batch diff, draft + HOLD the Work Log
entry, then the **G3 boundary smoke from the main session**, then the boundary commit (with the
split decision above).

## 2026-07-20 — Task 4 — `/review-batch` round (1 BLOCKER + 5 NEEDS-FIX + 6 NITs, all addressed)

`/review-batch` ran over the full batch diff at close. Outcome: **1 BLOCKER, 5 NEEDS-FIX, 6 NITs.**
**Every one addressed in-batch** — nothing deferred, nothing routed out. Rebuilt clean, Debug +
Release, confirmed by Jeff. Still uncommitted by design; HEAD is still `d6abc38b`.

### BLOCKER — every DrumPage raced to consume the learn capture

`DrumPage::pollTriggerLearn` called `takeCapturedBinding` — which consumes **destructively** via
`mCaptureReady.exchange(false)` — **before** checking `drum != mPageIndex`. Every `DrumPage` runs its
own `startTimerHz(24)` from construction to destruction (not just the visible one), so with N drum
tabs open, whichever page ticked first swallowed the capture and discarded it. MIDI Learn would have
succeeded roughly **1-in-N times** — and N > 1 is the definition of a kit, which is the exact surface
this batch exists to serve. The headline feature would have shipped broken on any real kit. The
existing comment stated the contract the code violated.

**Fixed at the source, not at the call site.** The API is now `takeCapturedBindingFor(drumIdx, out)`,
which checks ownership **inside** the take, so a non-owning page cannot consume at all. A caller-side
`if (drum != mPageIndex) return;` guard would have read as a fix while leaving the identical race —
the wrong page still wins the `exchange`, it just discards the capture politely.

A related hazard the bug had been hiding was closed at the same time: a stale page's 30 s deadline
called `map.cancelLearn()` unconditionally, killing a **different** page's in-flight learn. Both the
timeout path and the modal-cancel path now disarm only if `getLearnTargetDrum() == mPageIndex`.

### NEEDS-FIX resolved (all five)

- **D-6 re-pitch was current-pattern only.** `mixer_drum_{N}_playNote` is **one project-global param**,
  so the re-pitch had to follow it into **every** pattern. **I flagged this to Jeff as a spec call and
  he correctly rejected that framing** — it was incomplete implementation, not a decision for him. D-6
  reads "that drum's existing hits" with no pattern qualifier; all-patterns was always the requirement.
  Fixed. It needed more than a wider loop: the kit's `DrumKitSnapshot` covers only the **current**
  pattern, so a full sweep would not have been undoable. Added a dedicated **`DrumRepitchAction`**
  (in [DrumKitGrid.cpp](Source/Standalone/DrumKitGrid.cpp), beside the existing `DrumKitEditAction`)
  that snapshots one drum's notes across all patterns — ordinary kit edits stay cheap, and the
  re-pitch still restores on one Ctrl+Z. **Trap avoided:** undo could NOT be implemented as replaying
  the move backwards (`newNote -> oldNote`), because hits the user deliberately placed at the new pitch
  **before** the change are indistinguishable from re-pitched ones **after** it, and would be dragged
  along and corrupted. Snapshot-before/after is the only correct shape.
- **"MIDI Forget" menu item was not in the locked D-table.** Surfaced to Jeff; he approved keeping it
  as-is (answer: **(a)**). It is required — `clearBinding` has **no other caller**, so without the item
  a binding could never be removed.
- **MIDI Learn queue held its lock across the whole drain callback loop.** Not an audio-thread
  violation; the cost lands on the MIDI **input** thread, which spin-waits in `push()` for the whole
  duration of parameter notification. Surfaced to Jeff in plain English; he chose to restore the
  release (answer: **(a)**). The lock is now scoped to the swap only — safe because `mDraining` is
  audio-thread-private once swapped. This also makes the long-standing class comment true again; it
  had claimed the release for years while the code did the opposite.
- **Closing a drum tab mid-learn left the map armed forever.** The modal callback's
  `if (! safeThis) return;` ran **before** the disarm, so if the DrumPage died first the map stayed
  armed and the next note-on anywhere was silently swallowed as a capture. The callback now holds the
  trigger map **by reference** (the processor outlives every page), so the disarm runs regardless of
  the page's fate.
- **D-8 focus-gate substitution** (`mLiveMidiTargetKind == 0` reused instead of a new atomic) was
  already asked, answered by Jeff, and recorded in the plan (D-16 + the superseded Task-2 checkbox)
  **before** the review ran. No further action.

### NITs resolved (all six)

- **Fast-path bypass on both new audio-thread loops** via `DrumTriggerMap::anyBound()` — with one
  deliberate exception: the **CC-hold tick is NOT gated on it**, because a hold can outlive the binding
  that started it (unbind mid-hold) and skipping the tick would strand that voice. The tick uses a
  self-correcting `mAnyCcHoldActive` flag recomputed from survivors instead.
- **Dead `DrumTriggerMap::onChanged` removed** — declared, fired from four sites, assigned by nobody.
  Own-batch dead code, cleaned in the batch that created it.
- **`mLock`'s actual purpose documented** — it guards project load off the message thread, **not** the
  audio thread. The comment previously implied the audio thread contended for it.
- **Kit-row audition latch made per-row** instead of one shared slot (the D-15 `heldNote` latch).
- **`MidiLearnUI::cancelLearn` UI teardown no longer gated on `isLearning()`**, so an Escape landing in
  the window between capture and commit cannot strand the dashed overlay on screen.
- **D-10's suppression when the learned note already equals the play note** documented as a deliberate
  divergence (prompting "set the play note to the note it already is?" is noise).

### Reviewer confirmed sound (stated because the batch's central claim needed adversarial checking)

The RT-safety claims hold under review: `tryCaptureLearn` genuinely cannot free on the audio thread;
the interned device-name table's release/acquire pairing is correct and the audio thread's `const&`
read is safe against a concurrent append; there is no lock nesting or inversion anywhere in the two
subsystems; `DrumTriggerMap`'s packed publish/consume and its learn handshake are correct; stuck-note
coverage is complete (no stranding sequence constructible across note triggers, CC holds, transport
stop, tab close, or unbind-mid-hold); ASCII + US spelling clean; persistence round-trips.

### Unowned diff hunk — verified not ours

The reviewer flagged `StandaloneEditor::addDefaultDrumTab` as a diff hunk this batch does not claim.
Verified: it is tagged **NIT-14**, one of the **12 G3 boundary review-fixes** already in the working
tree at batch open. Correctly **not** part of this batch — left undisturbed per the plan. Same for
`UndoActions.h`, which is dirty from those fixes and NOT touched by this batch.

**State:** Tasks 1-3 + the I-3b RT fixes + this review round are all code complete and build clean
(Debug + Release, Jeff). Uncommitted by design — G3 boundary work riding Jeff's boundary commit.

**Resume action:** the Work Log entry is drafted and **HELD** below (applies at the §B.18 campaign
pass per bulk-run R2, together with the §5 STATUS flip). Then: surface the commit message + full
`git status` for approval, the G3 boundary smoke from the main session, then the boundary commit.

---

### HELD Work Log entry (apply verbatim at the §B.18 campaign pass — R2; stamp `HH:MM PT` at apply)

**DO NOT APPLY EARLY.** This is parked here per the plan's Task 4 and bulk-run R2. It lands in
`Implemented Work Log.md` at the §B.18 campaign pass, together with the Main Plan §5 STATUS flip.

### 2026-07-20 — QA-L-Fix — Per-drum MIDI kit triggers rebuilt: `_inputNote` -> `_playNote` play-pitch semantics + kit-only menu split + D-6 re-pitch across ALL patterns via a dedicated undo action; new lock-free `DrumTriggerMap` (note-or-CC MIDI Learn, no `std::function` on the audio thread, sample-position-preserving live-MIDI dispatch, CC-hold safety timeout, project persistence); app-wide MIDI-trigger-velocity toggle; plus 4 folded-in real-time fixes to the pre-existing I-3b MIDI Learn subsystem; the batch's stated "pads send CC" root cause was disproven by hardware test and the defect re-grounded on a hardware-independent reason

**Bucket:** Players, System Pages, Cross-cutting Infrastructure

#### Done

- **Task 1 — menu split + "MIDI Note" becomes the play pitch (D-2..D-6).** `mixer_drum_{N}_inputNote`
  renamed to `mixer_drum_{N}_playNote`, range flipped `-1..127` (default `-1` = unmapped) to `0..127`
  (default `60` = C5), and its **meaning flipped with it**: no longer an input filter, now the drum's
  **play pitch**. `DrumPage::showContextMenu` gained `bool fromKit`; the whole MIDI block is kit-gated
  so drum **pages** show neither MIDI item (D-2). The "Unassigned" item is gone, replaced by a disabled
  **"Assigned: \<note\>"** status row at the top of the submenu (D-5). One mutation path
  (`getPlayNote()` / `setPlayNote()` + `onPlayNoteChanged(pageIdx, oldNote, newNote)`) so Task 2's D-10
  prompt reuses it rather than duplicating the write. `DrumKitGrid` stamps new kit hits at the drum's
  assigned note at all three hit-stamp sites (Paint tool / Paint drag / Draw commit, all `pi >= 0`
  guarded), and the retune dot keys off the assigned note with the by-string APVTS lookup hoisted
  per-row out of the per-note paint loop. The dead `kind == 3` fan-out was **deleted here rather than
  in Task 2** (judgment call, flagged to Jeff at the time): once `_playNote` defaults to 60 for every
  drum, that block would fire **all 16 drums** on any incoming C5 while a drum tab was focused, so
  handing Jeff a knowingly-broken intermediate at the build gate was not worth the tidier task
  boundary.
- **Task 1 — D-6 re-pitch, corrected at review to span ALL patterns.** Changing a drum's assigned note
  re-pitches **only** that drum's hits sitting at the OLD assigned note; hits deliberately placed at
  other pitches stay put. As first shipped it swept the **current pattern only** — wrong, because
  `_playNote` is one project-global param, so the re-pitch has to follow it everywhere. Fixed at close
  via a dedicated **`DrumRepitchAction`** that snapshots one drum's notes across every pattern, so
  ordinary kit edits keep using the cheap `DrumKitSnapshot` while the re-pitch still restores on a
  single Ctrl+Z. **Undo could not be a reverse replay** (`newNote -> oldNote`): hits the user
  deliberately placed at the new pitch *before* the change are indistinguishable from re-pitched ones
  *after* it, and a reverse replay would drag them along and corrupt them.
- **Task 2 — `DrumTriggerMap`, a sibling to `MidiLearnRegistry` (D-7 / D-13 / D-14).** New module
  `Source/MidiLearn/DrumTriggerMap.h/.cpp`. Per-drum binding `{ Kind (None/Note/Cc), number, channel }`,
  channel 0 = omni. **Audio-thread contract:** one lock-free `std::atomic<uint32_t>` per drum (kind in
  bits 0-1, number+1 in bits 2-9, channel in bits 10-14) — no locks, no allocation, no `juce::String`
  on that path, and because a binding edit is a single word it can never be seen half-applied. The
  message thread mutates under `mLock` and republishes with a release store. **Learn capture never
  calls a `std::function` on the audio thread:** the audio thread packs the captured event into an
  atomic slot and sets a ready flag (`tryCaptureLearnRT`), and `DrumPage`'s **existing** 24 Hz page
  timer polls and commits on the message thread. Capture ignores note-off and CC value 0 — both are
  release events, and binding on let-go would (worse, for a pad idling at 0) self-bind with no user
  gesture at all.
- **Task 2 — kit menu + D-10 prompt.** "MIDI Learn" sits **directly below** "MIDI Note" (D-3), reading
  the binding back in its label (`MIDI Learn: D5 ch10` when set, plain `MIDI Learn` when not).
  A **"MIDI Forget: \<binding\>"** item appears only once a binding exists. On a **note** capture the
  D-10 prompt offers "Also set this drum's play note to \<note\>?" and routes Yes through the Task-1
  `setPlayNote` path, so the D-6 re-pitch + undo come along for free. **No prompt on CC** (a CC carries
  no pitch), and none when the learned note already equals the current play note.
- **Task 2 — dispatch in the live-MIDI loop, not the block-rate drain.** `dispatchDrumTriggers` runs
  inside the live-MIDI loop so `m.samplePosition` is preserved — drum hits are the most
  timing-sensitive events in the app. Learn capture runs first and `continue`s, so the pad the user
  just bound does not also sound the focused engine on that same hit. **CC triggers fire regardless of
  focus; note triggers fire only while the kit is the focused surface** (D-8, upheld as D-16). Either
  kind fires the drum at its **assigned play note** — the learned note/CC is only ever the input, never
  the pitch (D-9). **CC release carries a safety timeout:** CC > 0 fires, CC = 0 releases, and a 1 s
  countdown force-releases so a pad that never sends a release cannot strand a voice.
  `PRPendingOff` **could not be reused** for this — it is keyed on absolute BEAT and therefore does
  nothing while the transport is stopped, which is exactly when someone jams on pads. Trigger holds are
  also cleared on the Stop-button flush, alongside the existing piano-roll pending-off clear.
- **Task 2 — kit-focus signal reused, not duplicated.** The plan called for a new "drum kit focused"
  atomic published from `StandaloneEditor`. **No new flag was added** — the state already existed:
  `PianoRollPage::EngineKind::DrumKit` is `0` and `StandaloneEditor` already pushes it into
  `mLiveMidiTargetKind` on every engine selection including the boot-time push, so
  `mLiveMidiTargetKind == 0` **is** "kit focused". One source of truth instead of two publish paths.
- **Task 2 — persistence (D-14).** Bindings save with the **project** under `<DrumTriggers>`. An
  **absent** child calls `clearAll()` rather than leaving previous state in place — loading a project
  with no triggers must not inherit the last project's kit mapping.
- **Task 3 — app-wide "MIDI trigger velocity" toggle (D-11).** `From controller` (default) / `Fixed`,
  in the Mixer hamburger alongside Pan Law / Master Output / Multi-core Rendering, ticked to reflect
  current state and **hot-swapping** (next trigger picks up the new mode, no restart). Persisted to
  `settings.xml` as `<MidiTriggerVelocity fixed="0|1"/>` using the same sibling-preserving
  read-modify-write as `MultiCoreRendering`. Applied to **both** trigger kinds: `From controller` uses
  note-on velocity or the CC value scaled to 0-1; `Fixed` uses **0.8** for both — `PianoNote`'s default
  velocity, i.e. exactly what a drawn kit hit gets. The flag is a namespaced global atomic
  (`DrumTriggerVelocity::gUseFixed`), matching `RenderEngine::gMultiThreadedEngineEnabled` /
  `MeterLatencyComp::gEnabled` — **not a stylistic choice**, the preference loads before the processor
  is constructed, so it cannot live on the processor.
- **Folded in — 4 real-time fixes to the pre-existing I-3b MIDI Learn subsystem (Jeff's direction).**
  (1) **Heap free on the audio thread:** `MidiLearnEventQueue::Event` held a `juce::String deviceName`
  and the drain's `mDraining.clear()` ran `~juce::String` inside the render callback — a genuine free,
  since `MidiInput::getName()` returns **by value** and the MIDI-thread local died when its callback
  returned, leaving the queue the sole owner. Fixed by interning device names to integer ids in a
  fixed, never-resized table; the drain hands the consumer a `const&` **into** that table, consumer
  signature unchanged. Pinned the invariant that keeps it safe: `juce::MidiMessage` heap-allocates only
  above 8 bytes and everything this queue admits (CC / pitch-bend / channel-pressure) lives inline, so
  **widening the push filter to sysex would reintroduce an audio-thread free** — filter before pushing,
  not after draining. (2) **Per-event allocation in `dispatchEvent`:** it built a
  `std::vector<std::pair<juce::String, MessageType>>` per dispatched event — a heap allocation plus a
  string copy in the render callback, continuously, for the whole duration of a knob sweep. Replaced
  with a fixed stack array of already-resolved `juce::RangedAudioParameter*`; resolving **inside** the
  lock is what makes releasing it safe (APVTS parameters outlive the app, whereas a pointer into the
  map's keys would dangle if the message thread erased that mapping). Capped at 16 hits per event, tail
  **dropped rather than allocated for**. (3) **Four violations in `tryCaptureLearn`** — a
  `std::function` copy, the callback's own `MessageManager::callAsync`, a `std::map` insert, and an
  `onChanged()` notify, all on the audio thread. The capture callback is **removed entirely**; the
  audio thread fills a pre-constructed `Mapping` under the try-lock it already held and sets a flag,
  and `MidiLearnUI`'s timer (now 20 Hz, with the 30 s cancel converted from a one-shot into a deadline
  compared per tick) does the commit. Same handshake shape as `DrumTriggerMap`, arrived at
  independently then deliberately aligned. (4) **A fourth defect caught inside my own fix during
  self-check:** `mPendingCapture.paramId = mLearnTargetParamId` **releases its previous value**, and if
  that mapping had since been removed via MIDI Forget the release drops the last reference and frees
  in the render callback — the exact same defect one level down, reintroduced by the fix for it. The
  clear now happens on the **message thread**, so the audio thread only ever assigns into empty
  strings.
- **Three factually-wrong comments fixed while in those regions** (Rule 6 cleanup clause — wrong, not
  merely non-conforming). `StandaloneApp.cpp` claimed `addMessageToQueue` is wait-free; it takes
  `MidiMessageCollector::midiCallbackLock` and briefly contends with the audio thread's
  `removeNextBlockOfMessages`. `MidiLearnRegistry.h` claimed the lock was released before the drain
  loop; it was not (the review round then made the claim true again — see below). `DrumTriggerMap.h`
  said the registry's heap free was "routed to Main Plan section 9 Forks rather than copied here" —
  true when written, wrong once Jeff directed the in-batch fix.

#### Found along the way

- **PREMISE CORRECTION — the batch's stated root cause was wrong.** The plan's Context claimed "Jeff's
  drum pads send **CC**, not notes," and that the QA-L feature therefore "fires nothing on the target
  hardware." **That was wrong**, and wrong in a way that reached into the D-table (D-8 split note vs CC
  on the assumption CC was the primary path). Hardware-tested on the target controller (Novation
  FLkey 61) using shipped app behavior: **(A)** right-click knob -> MIDI Learn -> hit a pad -> **no
  capture**, and the learn queue admits only CC / pitch-bend / channel-pressure (`MessageType` has no
  Note case at all), which proves the pads are **not sending CC**; **(B)** select a Layer with an
  instrument in the Piano Roll -> hit a pad -> **it plays**, which proves the pads **are sending notes**
  and that those notes reach the app. Matches Novation's own documentation — FLkey pads are Note-based
  in every mode; the CC "Type" selector that made the CC premise plausible exists on the sibling
  **Launchkey MK4**, not FLkey. **The defect was real regardless, for a hardware-independent reason:**
  the shipped fan-out was gated on drum-tab focus (`kind == 3`) AND skipped the focused drum
  (`if (di == idx) continue;`), so with the Drum Kit selected — the surface you actually play a kit
  from — it did nothing no matter what the pads send. Only the stated cause was wrong, not the
  conclusion.
- **D-15 and D-16 were spec calls that arose during execution, not in the 2026-07-19 workshop.** D-15
  (kit-row audition pitch): the two press-and-hold audition dispatchers hardcoded note 60 with the
  comment "60 = C5 = the kit-grid placement note" — the exact assumption Task 1 had just made per-drum,
  so a drum assigned to D5 would **preview** at C5 but **play** D5 everywhere else. D-16 (re-look at
  D-8 after the premise correction): D-8's focus gate went from governing a *fallback branch* to
  governing the *whole feature* once notes turned out to be the only trigger path.
- **`deviceName` REMOVED from the plan's sketched binding struct.** The plan specified
  `{ Kind, number, channel, String deviceName }`. The live-MIDI path carries no device identity, so a
  `deviceName` field could only ever be empty. Recorded explicitly as a **removal**, not an omission.
  This came out of a verified multi-agent research sweep that also **corrected two of my own framings**,
  both previously stated in chat as fact: `MidiMessageCollector` is **not** sample-exact (it
  reconstructs sample position from wall-clock deltas, whole milliseconds on WinMM — the real
  comparison is ~1 ms vs ~10.7 ms, not "exact vs 10 ms"), and **per-event device identity is not
  something major DAWs carry at all** (they resolve it once at the port — FL port numbers, Ableton
  input streams, Bitwig controller scripts). That second correction is what collapsed the decision: if
  nobody carries per-event device identity, the live path's loss of it is not a tradeoff worth
  designing around.
- **SCOPE GROWTH — the batch carries a second, separate surface.** As planned it was Tasks 1-3 on drum
  kit triggers. It also carries the four I-3b MIDI Learn real-time fixes: different files, different
  originating batch, pre-existing defects. Surfaced while reading that code as the **pattern reference**
  for `DrumTriggerMap`.
- **Self-check caught three bugs in new code before the build gate** (inline self-check per the
  standing no-per-unit-review-agents rule; all three found by reading the code back, not by the
  compiler). **(a) `AlertWindow` double-free** — `enterModalState(..., deleteWhenDismissed = true)`
  fighting a `unique_ptr` member; JUCE deletes the window, then the smart pointer deletes it again.
  **(b) Note/CC cross-fire on matching numbers** — a `Note 42` binding matched an incoming `CC 42`
  because the **number was compared before the message type**; it emitted a spurious note-off on every
  matching CC. **(c) Note release used the CURRENT play pitch, not the pitch the hit started at** —
  re-assigning a drum's play note mid-hold would send note-off for the new pitch and strand the
  original voice sounding forever.

#### What was done about each finding

- **Premise correction: plan Context corrected in place** with a "Premise correction" blockquote, and
  the consequence **re-posed to Jeff before implementation** rather than coded past. The defect
  justification was re-grounded on the hardware-independent focus/skip reasoning.
- **D-15 asked in chat, answered (a) — audition follows `getPlayNote()`.** Implemented in both
  dispatchers with a `heldNote` latch: the pitch is read at **press** and reused at **release**, because
  `auditionNoteOff` targets a specific note and releasing against a freshly-read (possibly changed)
  assignment would strand a voice. Made **per-row** at review.
- **D-16 asked in chat with three lettered options, answered (a) — D-8 stands unchanged.** Notes fire
  only while the Drum Kit is the focused engine. Option (b) (fire globally, scoped to the pads' MIDI
  channel) stays available if the pads turn out to transmit on a different channel than the keys; the
  MIDI Learn menu label reads each binding's channel back, so it is checkable at the G3 boundary smoke
  with no instrumentation, and (b) would be a one-condition change to the note branch.
- **`deviceName` removal recorded as a removal** in the new module's header, with the reasoning, per
  the loud-paper-trail rule for dropped options.
- **RT fixes: fixed in-batch on Jeff's direction, NOT routed.** They were **initially routed to §9
  Forks**; Jeff then directed they be fixed in-batch. **There is therefore no Forks entry for them** —
  they are in the tree and belong in this Work Log entry instead. The scope growth is called out here
  rather than buried, and a commit split (A: Tasks 1-3 / B: the RT fixes) was surfaced as Jeff's call
  at the boundary commit; the two sets are cleanly separable by file except `PluginProcessor.h/.cpp`
  and `StandaloneApp.cpp`, which would need index-level staging (never source edits to shape hunks).
- **All three self-check bugs fixed before the build gate.** (a) switched to
  `juce::Component::SafePointer<juce::AlertWindow>` so JUCE owns the lifetime and the pointer nulls
  itself; (b) message type is now checked before the number; (c) added a per-drum held-note array
  (`mNoteTriggerHeld`, initialized to -1), with the CC branch carrying the same latch via `hold.note`.
- **A §9 Forks entry back-referencing QA-L is STILL OWED.** The plan's Routing-notes section calls for
  it: the per-drum-MIDI redesign, why the shipped QA-L version was unreachable (fan-out drum-tab-focus-
  gated AND skipping the focused drum, so it did nothing on the kit itself), the locked D-1..D-16
  design, and the note that the original "vs. CC pads" framing was **disproven by hardware test**. It
  is **NOT yet written** and must be authored at the §B.18 campaign pass alongside this entry. It is
  **separate from the RT fixes**, which explicitly get **no** Forks entry because they were **fixed**
  rather than routed.

#### `/review-batch` outcome

- **1 BLOCKER, 5 NEEDS-FIX, 6 NITs — every one addressed in-batch. Nothing deferred, nothing routed
  out.** Full detail in the review-round running-notes entry above; summary here.
- **BLOCKER — every `DrumPage` raced to consume the learn capture.** `pollTriggerLearn` called
  `takeCapturedBinding` (destructive via `mCaptureReady.exchange(false)`) **before** checking
  `drum != mPageIndex`. Every page runs its own `startTimerHz(24)` from construction to destruction, so
  with N drum tabs open whichever page ticked first swallowed the capture and discarded it — MIDI Learn
  would have worked roughly 1-in-N times, and N > 1 is the definition of a kit. **Fixed at the source:**
  the API is now `takeCapturedBindingFor(drumIdx, out)`, checking ownership **inside** the take so a
  non-owning page cannot consume at all — a caller-side guard would have left the identical race, just
  with a politer discard. A hazard the bug had hidden was closed with it: a stale page's 30 s deadline
  called `map.cancelLearn()` and killed a **different** page's in-flight learn; both the timeout and
  the modal-cancel paths now disarm only if `getLearnTargetDrum() == mPageIndex`.
- **NEEDS-FIX (all five).** (1) **D-6 re-pitch was current-pattern only** -> fixed to sweep all
  patterns via the new `DrumRepitchAction`; **I flagged this to Jeff as a spec call and he correctly
  rejected that framing** — incomplete implementation, not his decision; D-6 carries no pattern
  qualifier. (2) **"MIDI Forget" was not in the locked D-table** -> surfaced, Jeff approved keeping it
  (answer **(a)**); it is required, since `clearBinding` has no other caller. (3) **The MIDI Learn
  queue held its lock across the whole drain callback loop** -> not an audio-thread violation, but it
  made the MIDI **input** thread spin-wait in `push()` through parameter notification; surfaced in
  plain English, Jeff chose to restore the release (answer **(a)**); the lock is now scoped to the swap
  only, safe because `mDraining` is audio-thread-private once swapped — which also makes the
  long-standing class comment true again. (4) **Closing a drum tab mid-learn left the map armed
  forever** -> the modal callback now holds the trigger map by reference so the disarm runs even when
  the page dies first. (5) **D-8 focus-gate substitution** was already asked, answered, and recorded
  (D-16 + superseded checkbox) before the review ran — no action.
- **NITs (all six fixed).** Fast-path bypass added to both new audio-thread loops via
  `DrumTriggerMap::anyBound()` — with the deliberate exception that the **CC-hold tick is not gated on
  it**, since a hold can outlive the binding that started it (unbind mid-hold) and skipping the tick
  would strand that voice; it uses a self-correcting `mAnyCcHoldActive` flag recomputed from survivors
  instead. Dead `DrumTriggerMap::onChanged` removed (declared, fired from four sites, assigned by
  nobody — own-batch dead code). `mLock`'s actual purpose documented (project load off-thread, **not**
  the audio thread). Kit-row audition latch made per-row instead of one shared slot.
  `MidiLearnUI::cancelLearn` UI teardown no longer gated on `isLearning()`, so an Escape landing
  between capture and commit cannot strand the dashed overlay. D-10's suppression when the learned note
  already equals the play note documented as a deliberate divergence.
- **Reviewer confirmed the batch's central claims sound** (recorded because an RT-safety claim deserves
  adversarial checking): `tryCaptureLearn` genuinely cannot free on the audio thread; the interned
  device-name table's release/acquire pairing is correct and the audio thread's `const&` read is safe
  against a concurrent append; no lock nesting or inversion; `DrumTriggerMap`'s packed publish/consume
  and learn handshake are correct; stuck-note coverage is complete (no stranding sequence
  constructible); ASCII + US spelling clean; persistence round-trips.
- **One unowned diff hunk verified not ours.** The reviewer flagged
  `StandaloneEditor::addDefaultDrumTab`; it is tagged **NIT-14**, one of the 12 G3 boundary
  review-fixes already in the tree at batch open, correctly **not** part of this batch.

#### Carry-forward contradictions (if any)

- **None against Carry-Forward §1-§3.** Two in-batch corrections to the **plan's own** stated
  architecture, both recorded above rather than silently absorbed: (1) the plan's "there is no drum-kit
  focused state yet — Task 2 adds one" was wrong (`mLiveMidiTargetKind == 0` already carried it, so it
  was reused and the Task-2 checkbox marked superseded); (2) the plan's sketched binding struct carried
  a `deviceName` field that was **removed**. Neither contradicts the frozen carry-forward snapshot.
- **One deliberate semantics change, no migration (pre-v1, per `feedback_no_backward_compat_pre_v1`):**
  `mixer_drum_{N}_inputNote` -> `mixer_drum_{N}_playNote`, default `-1` -> `60`, meaning flipped from
  input filter to play pitch. Existing projects' stored values are reinterpreted; no migration code was
  written and none is planned.
- The `mLiveMidiTargetKind == 0` reuse inherits that flag's existing semantics: it tracks the Piano
  Roll's **selected** engine, so it stays true when the user navigates to Mixer/Effects. Consistent
  with how every other engine kind already behaves for live MIDI; recorded so it is not re-discovered
  as a bug.

#### Diagnostic Instrumentation Catalog

- **NONE added this batch.** No `DBG` / `juce::Logger` / temp `jassert` / debug `AlertWindow` /
  temp-file trace. The premise correction was settled by **hardware tests using shipped app behavior**
  (MIDI Learn on a knob; a Layer in the Piano Roll), not by instrumentation, and every other finding
  was resolved by reading code. Nothing to strip. Verified by grep over the batch diff + the new files.

#### Files touched

- **Task 1 (play-pitch semantics + menu split + kit stamping + re-pitch):**
  `Source/PluginProcessor.h` / `.cpp` (`_playNote` param + `mDrumPlayNotePtr` + `kind == 3` fan-out
  deletion), `Source/Standalone/DrumPage.h` / `.cpp` (`fromKit` gate, "Assigned:" row,
  `getPlayNote` / `setPlayNote` / `onPlayNoteChanged`), `Source/Standalone/DrumKitGrid.h` / `.cpp`
  (`setApvts`, `playNoteForPage`, `repitchDrumHits`, three hit-stamp sites, retune dot, container
  forwarders), `Source/Standalone/StandaloneEditor.cpp` (kit callers pass `true`; `wireDrumPageKitView`;
  both audition dispatchers per D-15).
- **Task 2 (`DrumTriggerMap` + learn + dispatch + persistence):**
  `Source/MidiLearn/DrumTriggerMap.h` (new), `Source/MidiLearn/DrumTriggerMap.cpp` (new),
  `CMakeLists.txt`, `Source/PluginProcessor.h` / `.cpp` (map ownership, `dispatchDrumTriggers`,
  `tickCcTriggerHolds`, `mNoteTriggerHeld`, Stop-flush clear, `<DrumTriggers>` save/load),
  `Source/Standalone/DrumPage.h` / `.cpp` (MIDI Learn / MIDI Forget items, D-10 prompt,
  `pollTriggerLearn`).
- **Task 3 (velocity toggle):** `Source/Standalone/StandaloneEditor.cpp` (Mixer hamburger submenu),
  `Source/Standalone/StandaloneApp.h` / `.cpp` (`settings.xml` read-modify-write + startup load),
  `Source/MidiLearn/DrumTriggerMap.h` / `.cpp` (`DrumTriggerVelocity::gUseFixed`),
  `Source/PluginProcessor.cpp` (applied in dispatch).
- **Folded RT fixes (I-3b MIDI Learn):** `Source/MidiLearn/MidiLearnRegistry.h` / `.cpp` (device-name
  interning, alloc-free `dispatchEvent`, callback-free `tryCaptureLearn`, message-thread `paramId`
  clear, lock scoped to the swap), `Source/MidiLearn/MidiLearnUI.h` (20 Hz timer, per-tick 30 s
  deadline, ungated `cancelLearn` teardown), `Source/Standalone/StandaloneApp.cpp` (wrong wait-free
  comment).
- **Review round:** `Source/MidiLearn/DrumTriggerMap.h` / `.cpp` (`takeCapturedBindingFor`,
  `anyBound()`, dead `onChanged` removed, `mLock` purpose documented),
  `Source/Standalone/DrumPage.cpp` (ownership-checked poll, scoped disarm on both cancel paths,
  by-reference modal callback), `Source/Standalone/DrumKitGrid.cpp` (all-patterns re-pitch +
  `DrumRepitchAction`), `Source/Standalone/StandaloneEditor.cpp` (per-row audition latch),
  `Source/PluginProcessor.h` / `.cpp` (fast-path bypass + `mAnyCcHoldActive`),
  `Source/MidiLearn/MidiLearnUI.h` (ungated teardown).
- **Docs:** paired `Running Notes/eager-thumping-marmot.md` + `Batch Plans/eager-thumping-marmot.md`;
  `Test Plans/v1-master-test-plan.md` §B.18 (L-8 SUPERSEDED, new L-9..L-14, authored ahead of
  execution per bulk-run R4); this Work Log entry; Main Plan §5 (QA-L-Fix STATUS flip) **and a §9 Forks
  entry back-referencing QA-L — STILL OWED, not yet written** (see "What was done about each finding").
  The RT fixes get **no** Forks entry (fixed on Jeff's direction, not routed).
- **NOT this batch, present in the same working tree:** the 12 G3 boundary review-fixes
  (`BaySickSynthVoice.h`, `BroadcastSynthesiser.h`, `OctaveStyleDSP.h`, `AdditiveVoice.h`,
  `BuilderPage.*`, `MixerPage.*`, `MixerTrackStrip.h`, `PianoRoll.cpp`, `RibbonTabBar.*`,
  `LayersPage.cpp`, `BassPage.cpp`, `UndoActions.h`, and the `addDefaultDrumTab` hunk = NIT-14) plus the
  `locked-doubling-frog` doc stragglers. Untouched by this batch.

#### Commit(s)

**None.** Nothing was committed for this batch — HEAD remained `d6abc38b` throughout. All of it is G3
boundary work that rides **Jeff's boundary commit** from the main session, with the A/B commit-split
decision (drum kit triggers vs. MIDI Learn RT fixes) left as his call at that point. Builds clean,
Debug + Release, confirmed by Jeff (Tasks 1-3, the RT fixes, and the post-review rebuild).
**Verification was NOT run mid-batch** (bulk-run R4): scenarios live in
[`v1-master-test-plan.md`](../Test Plans/v1-master-test-plan.md) §B.18 as **L-9..L-14** (L-8
SUPERSEDED) and get walked at the **G3 boundary smoke** plus the §B.18 campaign pass.

#### Next action

- **G3 boundary smoke** from the main session, then the boundary commit (with the split decision).
- At the **§B.18 campaign pass (R2)**: apply this entry, flip Main Plan §5 QA-L-Fix to CLOSED, and
  **author the owed §9 Forks entry back-referencing QA-L**.
