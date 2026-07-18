# Running Notes — QA-K (brisk-prioritizing-wren)

> Append-only mid-batch log. New entry at every checkpoint (commit landed / sub-task
> verified / finding captured / spec call resolved / scope pivot). Consumed at doc-close
> (section pass under bulk-run R2) as the primary input for the held Work Log entry.

Pair file: `Plans & Specs/Batch Plans/brisk-prioritizing-wren.md`
Convention: Main Plan §0 "Batch Plans + Running Notes layout" (exemplar
`federated-bouncing-cupcake.md`).

## 2026-07-17 — Group open (G3)

Seeded at G3 group approval.

## 2026-07-18 — Task 1 — APP-04 process priority + MMCSS

`SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS)` at the top of
`VibesynthStandaloneApp::initialise` (StandaloneApp.cpp) — ABOVE_NORMAL deliberately, not
HIGH/REALTIME (aggressive classes starve input/compositing + can priority-invert against
drivers). Each render worker registers MMCSS "Pro Audio" at `workerLoop` entry via
`AvSetMmThreadCharacteristicsW` and reverts via `AvRevertMmThreadCharacteristics` after
the shutdown loop exits (VibeThreadPool.cpp); null handle = worker runs unboosted (no
failure path). RetirementQueue drainer + writer threads untouched per plan. Includes per
the codebase idiom (`#if JUCE_WINDOWS` + explicit `<windows.h>`, GlobalTransportBar.cpp
precedent) + `<avrt.h>` + `#pragma comment(lib, "avrt.lib")` (no CMake change). No
findings, no spec calls, no diagnostics added.

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-18.

## 2026-07-18 — Task 3 — DSP-11 live buffer-size change (feasibility-gated 10b)

applySettings gained a live path ahead of the pending-file flow: when selected type +
device + sample rate all match the CURRENT open device (fresh getAudioDeviceSetup, not
the ctor snapshot) and only the buffer size differs (and a device is actually open), new
applyBufferSizeLive(newBuf) runs instead — fresh setup copy with only bufferSize
changed -> setAudioDeviceSetup(setup, treatAsChosenDevice=true) (startup
live-reconfigure precedent, StandaloneApp.cpp:740; the current setup already carries the
ASIO input-name + channel-mask fixups so buffer-only inherits them). QUIESCE SHIELD: the
dialog's stored mCallback (finally earning its keep — held since the pre-pending-flow
era, previously unused) is removeAudioCallback'd before the call (BLOCKS until any
in-flight callback returns; none of our code runs during the close/reopen window — the
documented WASAPI-exclusive crash class is stream teardown racing the device's render
thread) and re-added after. Failure: async alert with the driver's error, previous
settings stay, dialog stays open. Success: dialog closes, NO restart prompt; shutdown's
saveAudioSettings persists the accepted size (no pending file); the manager change
broadcast refreshes output-latency + meter comp via the existing StandaloneApp listener.
Type/device/rate changes: pending + restart flow byte-for-byte untouched. Feasibility
verdict (10b) = Jeff's Debug-first §B scenario on real devices; infeasible -> revert this
hunk, fallback documented. No spec calls, no diagnostics added.

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-18.

## 2026-07-18 — Task 4 — DSP-01 Lasersaw fix + factory-preset audit (#9) + docketed fixes

Lasersaw.xml amp_s 0.0 -> 0.85 (the plan's pre-authorized data fix). AUDIT: 857 preset
XMLs + 36 code-defined EffectPresetIO entries swept by four parallel read-only agents
split by preset FORMAT (Harmless / BSS+BSB+synth-format drums / BaySickPlayer+player
formats / Effects+pages), each reading the owning engine's createLayout + voice source
first, scripting the bulk sweep, hand-verifying every flag with file:line citations;
parent spot-verified the two systemic premises (EffectPresetIO.cpp:728 skip-if-exists;
:611 setReleaseMs(5.0f)). Deliverable: `Plans & Specs/Research Reports/
factory-preset-audit-2026-07.md`. Agent premise corrections captured: Harmless
pluck_decay is a static spectral tilt, NOT a temporal decay (can't rescue zero-sustain);
BaySickNAMIR folder is NOT empty (2 library files; my ls counted *.xml only); all 172
BaySickDrums presets are synth-format (no player-format subset existed).

Headline findings: 3 provably-dead presets via the HP@20kHz brick-wall gotcha (Tropical
Pluck / 606 Closed Hat / Sleigh Bells) + 2 Shimmer patches center-silenced the same way;
7 transpose values at +/-36 vs the +/-24 range; 4 byte-identical dup drum presets (the
2026-04-25 category move copied without deleting); frozen-seed systemic
(seedFactoryPresets skip-if-exists + repo-is-Documents = factory effect XMLs frozen at
the 2026-05-02 snapshot; Vocal Tame missing its VocalBooth algorithm key); Limiter
"Brick Wall" table value below the 10 ms floor; Rusty player-preset save capped at
CC<128 vs 512 registered; assorted LOW items. Player family pristine (all 217 sample
refs library-relative, case-exact resolve, 0 range violations).

DOCKETS (asked in chat, Jeff's picks): #1=a (fix HIGH+MED data flags in-batch),
#2=c (fix all code findings incl. structural), #3=c (delete factory orphans + user
files), plus Jeff's adds: fix LOW items Bell Lead + 808 Claves. Sub-docket
(frozen-seed mechanism, flagged inside #2c): Jeff clarified the real blast radius is
his two machines only (repo public but zero external installs, no installer) — pick
= versioned seeding (option 1).

FIXES SHIPPED: 22 XML data fixes (script-applied, 22/22 verified: 3 tonal HP@20k ->
LP@20k open; 2 noise hats HP cutoff 20000 -> 8000 sibling range; 7 transpose -> +/-24;
8 env zeros -> 0.001; Bell Lead amp_s -> 0.85; 808 Claves decay 0.015 -> 0.04).
DELETIONS: Tuned Percussion dups x4 + BaySick Kit 1.bsd + My Bass Preset.xml (git rm,
tracked) + Guitars Page folder + 2 pre-J-11 Rusty Player presets (plain rm, untracked
user files — explicit 3c authorization). CODE: Rusty save loop 128 ->
BaySickRustyDrumsProcessor::kCcCount (extended CCs round-trip); EffectPresetIO Brick
Wall 5 -> 10 ms; 70s Plate setAlgorithm(0=Plate — the topology its name promises) +
Cathedral setAlgorithm(1=Hall) pins; SaturationDSP.cpp:786 wrong comment corrected
(construction default is IR OFF); gen_factory_presets.py stale-category cleanup pass
(move-without-delete class killed at the root); VERSIONED SEEDING in seedFactoryPresets
(kFactorySeedVersion=2 + factory_seed_version.txt stamp at the Effects presets root;
stale/missing stamp -> factory files rewrite from current code, then stamp updates; My
Presets never touched; first run of the new build performs the one-time heal — the
rewritten factory XMLs + stamp will show dirty/untracked after, expected). Stale
DeEsser-trio / Vocal Tame disk files heal via that same re-seed (no hand edits needed).

Nothing left routed out of QA-K: all seven audit code-candidates resolved in-batch; the
report remains the campaign's §E ear-list input. No diagnostics added.

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-18.

## 2026-07-18 — QA-K CODE-COMPLETE

Tasks 1-4 shipped (4 build gates, all clean; Task 4's dockets resolved in-sitting).
§B.17 authored (6 scenarios; `blocks:` hash backfills at the next docs commit per
precedent). Work Log entry drafted + HELD below. ONE batch commit surfaced for approval
(carries the QA-J' doc straggler: the B.16 hash backfill).

## Held Work Log entry

> HELD per bulk-run R2 — apply to `Implemented Work Log.md` verbatim when §B.17 passes.

### 2026-07-18 20:15 PT — QA-K — Process priority/MMCSS + ASIO control panel + live buffer-size path + Lasersaw/preset-data fixes + factory-preset audit + versioned effect-preset seeding

**Bucket:** Cross-cutting Infrastructure, Effects, Players, System Pages

#### Done

- **Task 1 — APP-04 priority (process + workers):**
  `SetPriorityClass(ABOVE_NORMAL_PRIORITY_CLASS)` at initialise top (deliberately not
  HIGH/REALTIME — those starve input/compositing and can priority-invert drivers);
  MMCSS "Pro Audio" registration on every render worker for the thread's lifetime
  (`AvSetMmThreadCharacteristicsW` at workerLoop entry, revert after the shutdown loop;
  null handle = unboosted, no failure path). RetirementQueue drainer + writer threads
  untouched. Includes per the JUCE_WINDOWS + explicit windows.h idiom; avrt via pragma
  lib.
- **Task 2 — APP-05 ASIO Control Panel button:** footer button in AudioSettingsDialog,
  enabled only when the LIVE device hasControlPanel() (ASIO-only); handler mirrors the
  vendored-JUCE selector flow (verified in source, not memory): modal shield ->
  showControlPanel() -> on reported change closeAudioDevice() + restartLastAudioDevice()
  + re-front. Safe despite the dialog's no-live-touch design (ASIO gate = the WASAPI
  crash class can't reach it).
- **Task 3 — DSP-11 live buffer-size change (10b feasibility-gated):** Apply detects
  buffer-size-ONLY changes (type+device+rate match the live device) and reconfigures in
  place — fresh setup copy, setAudioDeviceSetup(treatAsChosenDevice) per the startup
  precedent; the dialog's stored mCallback is removed (blocks until in-flight callback
  returns — the quiesce shield) and re-added around the call. Failure: driver error
  alerted, dialog stays. Success: dialog closes, no restart prompt, shutdown persists
  the size, latency comp refreshes via the existing listener. Type/device/rate changes
  keep the pending+restart flow untouched. Feasibility verdict = Jeff's Debug-first
  §B.17 run; infeasible -> revert the hunk.
- **Task 4 — DSP-01 + audit (#9) + docketed fixes:** Lasersaw amp_s 0 -> 0.85. Full
  857-preset + code-table audit (4 format-split agents, premises source-verified,
  parent spot-checks) -> `Research Reports/factory-preset-audit-2026-07.md`. Docket
  picks (1a/2c/3c + two LOW adds + versioned-seeding sub-pick) all executed in-batch:
  22 verified XML data fixes (HP@20k kills, transpose clamps, env-zero tidies, Bell
  Lead, 808 Claves), 9 orphan/dup deletions (incl. user files per explicit 3c), Rusty
  512-CC save fix, Limiter/Reverb-pin/comment table fixes, generator stale-category
  cleanup, and versioned factory seeding (kFactorySeedVersion=2 + stamp file; one-time
  heal rewrites the frozen 2026-05-02 factory effect XMLs on first launch — Vocal
  Tame's VocalBooth algorithm finally reaches disk; My Presets untouched).

#### Found along the way

Agent premise corrections (pluck_decay = spectral tilt not decay; NAMIR folder not
empty; all drum presets synth-format). Frozen-seed systemic + Rusty CC cap + Limiter
floor violation + unpinned reverb algorithms + generator move-without-delete — all
surfaced by the audit and ALL resolved in-batch per docket 2c (nothing deferred).
Jeff's scope clarification: zero external installs exist (repo public, no installer),
so the stale-seed blast radius was his two machines; versioned seeding fixes both his
machines (one-time heal) and the post-installer future.

#### Known seams (campaign-visible)

First launch of this build rewrites factory effect XMLs + drops the stamp file (dirty
git status after — expected one-time heal, rides a later commit). DSP-08 (Tascam
outputs 21/22) remains the campaign hardware test per marathon 10a. The report's LOW
items not chosen for fixing (Lasersaw Stab, Filtered Saw Bass, Boom-Bap 808,
Harpsichord Stab, cutSelfMode generator omission) stay report-documented for the §E
ear pass.

**Verification:** bulk-run R2 — campaign section §B.17 (6 scenarios). Build-confirmed
clean (Release+Debug) at all four task gates, 2026-07-18. — APP-05 ASIO Control Panel button

New "Open ASIO Control Panel" TextButton in AudioSettingsDialog's footer (bottom-left,
176 px; Apply/Close untouched at bottom-right). Enabled only when the LIVE device
reports hasControlPanel() (ASIO only on Windows) — computed at ctor from
mMgr.getCurrentAudioDevice(), deliberately NOT the combo selection (the dialog never
touches the live device until Apply, so the live device can't change under an open
dialog). Handler mirrors juce::AudioDeviceSelectorComponent::showDeviceUIPanel —
VERIFIED in vendored JUCE 7.0.12 source (juce_AudioDeviceSelectorComponent.cpp:472-502),
not from memory: opaque modal shield component on the desktop for the panel's duration,
then on showControlPanel() == true (settings changed) closeAudioDevice() +
restartLastAudioDevice() + re-front the top-level window. Restart is safe despite the
dialog's no-live-touch design: the hasControlPanel gate = ASIO only, so the documented
WASAPI-exclusive hot-swap crash class can't reach the path. Existing device-change hook
(StandaloneApp mDeviceManager listener) refreshes output-latency + meter comp for free.
No findings, no spec calls, no diagnostics added.

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-18. Locked: DSP-08 = campaign hardware test; DSP-11 feasibility-
gated (10b); docket #9 re-scoped DSP-01 to a data-read audit of ALL factory presets ->
flagged-candidates report (`Research Reports/factory-preset-audit-2026-07.md`), no in-app
tool; Lasersaw root = amp sustain 0.0 in the preset XML. Coding starts after QA-J'.
