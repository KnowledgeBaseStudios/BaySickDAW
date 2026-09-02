# Running Notes — QA-TransportDisplay (punctual-gliding-otter)

> Append-only mid-batch log. New entry at every checkpoint (commit landed / finding captured /
> spec call resolved / scope pivot). Consumed at doc-close: under the BULK RUN this batch's Work
> Log entry is drafted at code-complete, HELD here under `## Held Work Log entry (apply at section
> pass)`, and applied only when its Master Test Plan §B section passes.

Pair file: [`Batch Plans/punctual-gliding-otter.md`](../Batch Plans/punctual-gliding-otter.md).
Conventions: Main Plan §0 (Batch Plans + Running Notes layout, locked 2026-05-11) + the bulk-run
adjustments in [`Batch Plans/swift-stampeding-caribou.md`](../Batch Plans/swift-stampeding-caribou.md).

## 2026-07-08 — G1 group open — plan approved + surface map (source-verified)

Plan approved with the G1 set (single R5 approval). Locked specs: 17a-17e + A1-A5 (see the run
plan's marathon + G1 plan-write answer tables). Condensed surface map from the 2026-07-08 Explore
pass — these refs are VERIFIED and supersede any older doc line refs:

**Bar layout:** `GlobalTransportBar.cpp:822-867` resized(); fixed-pixel left cluster ends x≈476;
`kControlsWidth = 520` (`.h:116`); the D-4 reserve = the 476→520 gap (comments `.cpp:198-200`,
`:854-856`); D-4 button inserts after the metro-arrow block (`.cpp:858-859`). Overlay layout:
`StandaloneEditor.cpp:9020-9055` — pattern button x 528-704 (`:9036-9038`), ribbon `ribX = 712`
(`:9041`), `ribW` proportional with `>60` guard (`:9042-9044`), `kCPUReserve = 120` (`:9034`).
Readout inserts at ribX; ribbon shifts right.

**Clock reads:** `StandalonePlayHead::getCurrentBeat()` (`StandaloneApp.h:23`) encapsulates the
seqlock (`deriveBeat` `.cpp:142-158`); `mSamplePos` (`.h:64`); pattern mode is ALREADY
pattern-relative via the loop wrap in `advanceBlock` (`.cpp:181-208`). Existing 30/timer consumers
to mirror: `PianoRollPage.cpp:41-83`, `BuilderPage.cpp:5719-5745` (bar = beat/4). TS chain for
pattern beats-per-bar: `StandaloneEditor.cpp:774-781` → `GlobalTransportBar.cpp:626-630`. Ticks:
`kTicksPerBeat = 96` (`VibesynthConstants.h:34-39`), converters `PatternManager.h:7-16`.

**settings.xml pattern to clone:** `load/saveMultiCoreRenderingPref` (`StandaloneApp.cpp:317-359`,
decl `.h:148-158`) — re-parse + preserve-siblings on save; root `<BaySickDAWSettings>`.

**Keybind (Ctrl+T):** enum `KeyBindings.h:23-76` — next free ID `0x10071` (comment `:75`);
catalog `buildCatalog` `.cpp:21-184` (Ctrl+M example `:152-155`); dispatch =
`StandaloneEditor.cpp:6924-7050` (`perform` switch), wiring `:1353-1368` (KeyPressMappingSet
listener; NO keyPressed override exists — clean).

**D-4 note path (A4 = collector, chord-safe + records):** hardware precedent
`StandaloneApp.cpp:747-763` → `getLiveMidiCollector().addMessageToQueue` (`:752`); active-tab
target `setLiveMidiTarget` (`PluginProcessor.h:331-335`, atomics `.h:1167-1168`), focus wiring
`StandaloneEditor.cpp:1574`/`:1588`; drain + route `PluginProcessor.cpp:1813-1867` — kinds
1/2/3/4/7/8/9 routed, Vox/live-Inst dropped (hardware parity), kind 8 gets −12, on-screen keyboard
lit `:1856-1858`, `allMidi` feeds the MIDI recorder. Alternative hold-audition atomics
(mono-per-block, NOT chosen): `auditionNoteOn/Off` on Synth/BaySickSolstice/Player/Bass
(off-before-on `BaySickSynthProcessor.cpp:65-72`).

## 2026-07-08 — Tasks 1+2 CODE-COMPLETE (both configs build clean, Jeff)

- **Task 1 — readout.** New `TransportPositionReadout` (GlobalTransportBar.h/.cpp; BPM-field LCD
  palette; own 30 Hz timer, repaint-on-change) instanced by StandaloneEditor as an overlay child
  between the pattern button and the ribbon (`resized()` — ribbon absorbs the 108px, `kPosReadoutW
  = 100`). Click toggles beats<->time; persisted via new `<TransportDisplay showTime>` settings.xml
  child (load/saveTransportDisplayPref, MT-pref clone). New
  `StandalonePlayHead::getCurrentTimeSeconds()` (sample-derived wall time — survives QA-TempoMap).
  Beats math: ticks-first rounding then carry; song = 4 beats/bar, pattern = pattern tsNum
  (downbeat-alignment criterion); 1-based display.
- **Task 2 — D-4 typing keyboard.** New `TypingKeyboardMap.h` (inline atomic + two-row map +
  bypass predicate); bypass early-returns added to the THREE grid key handlers (PianoRollGrid /
  DrumKitGrid / ArrangementGrid — containers just delegate, verified). `KeyboardMidiButton` in the
  reserved bar slot (476->520 gap; layout now 4+32, 20px margin left); `cmdToggleTypingKeyboard =
  0x10071` + Ctrl+T catalog entry + `perform()` case; editor owns state (`toggleTypingKeyboard`),
  overrides `keyPressed` (note-on, auto-repeat guard, PgUp/PgDn octave shift with release-first,
  bare-keys-only) + `keyStateChanged` (release diff vs OS key state, never consumes). Notes inject
  into the live-MIDI collector (self-timestamped — collector asserts on zero stamps) → record
  parity + keyboard lighting free. Releases on mode-off / octave-shift / tab-switch
  (`showPageForTab` head).
- **Accepted limitation (logged, not a bug):** tab-switch note-release has a one-audio-block race —
  the collector routes at DRAIN time, so a noteOff enqueued just before the target flips could
  reach the new engine if the block boundary lands in between (~5 ms window, requires holding a
  typed note through the exact switch block; recovery = retap or mode toggle). Accepted at
  implementation; revisit only if the campaign reproduces it.
- **Diagnostics:** none added (nothing for the Rule 4 catalog).
- **Docs:** `STANDALONE_UI_CHANGES.md` gained the QA-TransportDisplay section; Master Test Plan
  §B.1 authored (13 scenarios TD-1..TD-13; `blocks:` hash backfills at the next test-plan touch).
- **Files:** `GlobalTransportBar.h/.cpp`, `StandaloneEditor.h/.cpp`, `StandaloneApp.h`,
  `KeyBindings.h/.cpp`, `TypingKeyboardMap.h` (new), `PianoRoll.cpp`, `DrumKitGrid.cpp`,
  `BuilderPage.cpp` (bypass lines), `STANDALONE_UI_CHANGES.md`, test plan §B.1.

## 2026-07-08 — DEFECT found by Jeff at app-open + FIXED (z-order)

Jeff launched the app after the QA-TempoMap build and saw EMPTY SPACE where the readout belongs.
Root cause: `addAndMakeVisible (*mPosReadout)` ran in the ctor's transport-WIRING block
(StandaloneEditor.cpp:897), but `addAndMakeVisible(*mTransport)` runs later (:987) — later child =
higher z-order, so the bar's full-width brushed-aluminum paint covered the readout entirely. The
overlay convention (STANDALONE_UI_CHANGES.md: transport added FIRST as background; pattern button
+ ribbon added after) was documented and I missed applying it to the new overlay child. Fix: the
add moved to immediately after the transport's add (comment records the rule at both sites).
Fix commit references QA-TransportDisplay (owning batch) per the bulk-run fix-commit rule.
Lesson logged: any new overlay child of the 40px bar must be added after mTransport.

## Held Work Log entry (apply at section pass)

> Apply verbatim below at §B.1 section pass; fill `<hash>` with the batch's source commit and
> `<section-pass date/outcome>` from the campaign walk. Group review outcome (R3: one /review-batch
> per group) gets its line filled at the G1 boundary.

### <APPLY-DATE> — QA-TransportDisplay — Transport-bar position readout (bars:beats:ticks / M:SS.mmm click-toggle, live in both modes, settings.xml persistence) + D-4 typing-keyboard MIDI (two-row map, PgUp/PgDn octaves, record parity via the live-MIDI collector, bar button + Ctrl+T, grid tool-key bypass)

**Bucket:** UI / L&F / Theming, Cross-cutting Infrastructure

#### Done

- **Task 1 — position readout.** `TransportPositionReadout` (GlobalTransportBar.h/.cpp) — LCD
  (BPM-field palette), own 30 Hz timer repainting only on string change; overlay child of
  StandaloneEditor between pattern button and ribbon (ribbon absorbs the width per the no-expand
  rule; bar stays 40px). Click toggles `bars:beats:ticks` (96 PPQ, 1-based, ticks-first rounding)
  <-> `M:SS.mmm`; mode persists app-wide (`<TransportDisplay>` settings.xml child, MT-pref
  pattern). Song mode counts 4 beats/bar (playback's grid); pattern mode counts the pattern's
  tsNum (metronome-accent alignment) and is naturally pattern-relative (17a — the clock
  loop-wraps). New `StandalonePlayHead::getCurrentTimeSeconds()` (sample-derived; QA-TempoMap-proof).
- **Task 2 — D-4 typing-keyboard MIDI** (the 2026-05-08 triage-gap item, folded at the marathon).
  `TypingKeyboardMap.h` (new): inline atomic mode flag + two-row key map (A1) + bypass predicate;
  the three grid key handlers decline bare mapped keys while the mode is on so they bubble to
  StandaloneEditor's converter (single-letter tool shortcuts + S-cycle suppressed by design while
  typing). `KeyboardMidiButton` fills the bar slot reserved since D-5 polish; `cmdToggleTypingKeyboard`
  (0x10071, Ctrl+T). Editor owns the mode; keyPressed converts (velocity 0.8 = A3; auto-repeat
  guarded; PgUp/PgDn octave shift clamped [-5..+3] with release-first = A2); keyStateChanged diffs
  held notes vs OS key state for releases and never consumes. Notes self-timestamp into the
  live-MIDI collector (A4) — active-tab routing (A5), MIDI-recorder capture, and on-screen keyboard
  lighting all ride the existing dispatch. Held notes release on mode-off / octave-shift /
  tab-switch.

#### Found along the way

- **Tab-switch release race (accepted limitation):** the collector routes at drain time, so a
  noteOff enqueued immediately before the live-MIDI target flips can land on the new engine if the
  block boundary falls in between (~one block, ~5 ms). Accepted; revisit only on campaign repro.
- The D-4 reserved slot + `kControlsWidth` reserve comments were accurate; layout consumed 4+32 of
  the ~44px gap (20px margin remains before kControlsWidth).

#### What was done about each finding

- Race: documented here + in running notes; no code (fix would need per-note engine addressing the
  collector path deliberately avoids). Nothing routed out.

#### Group review (R3 — one /review-batch per checkpoint group)

- G1 group review 2026-07-08: **1 NEEDS-FIX here, FIXED in the G1 review-fix commit.** The plan's
  "release held notes on app focus loss" trigger was dropped unrecorded — Alt+Tab mid-hold sent
  the key-up to the other app and the note droned until refocus. Fix:
  `VibeSynthWindow::activeWindowStatusChanged` flushes via `releaseAllTypingNotes()` (made public
  for the window). 2 NITs also fixed: `semitoneForKey` now matches both letter cases (the
  codebase's `code || code+32` convention; Windows-only uppercase delivery made it non-functional
  today but a portability landmine); Ctrl+T catalog entry lowercased to match file style.

#### Diagnostic Instrumentation Catalog

- None added.

#### Files touched

`Source/Standalone/GlobalTransportBar.h/.cpp`, `Source/Standalone/StandaloneEditor.h/.cpp`,
`Source/Standalone/StandaloneApp.h`, `Source/Standalone/KeyBindings.h/.cpp`,
`Source/Standalone/TypingKeyboardMap.h` (new), `Source/Standalone/PianoRoll.cpp`,
`Source/Standalone/DrumKitGrid.cpp`, `Source/Standalone/BuilderPage.cpp`,
`Source/Standalone/STANDALONE_UI_CHANGES.md`, `Plans & Specs/Test Plans/v1-master-test-plan.md`
(§B.1), paired plan + running notes.

#### Commit(s)

`<hash>` (Tasks 1+2 + §B.1 + held entry + running notes — single batch commit per the bulk-run
model). Verified via Master Test Plan §B.1, <section-pass date/outcome>.

#### Next action

- <filled at apply: next unchecked §B section>.
