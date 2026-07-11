# Bulk-Run Expedite Plan — everything past QA-UICleanup — Plan (swift-stampeding-caribou)

> **Canonical path:** `Plans & Specs/Batch Plans/swift-stampeding-caribou.md` (mirrored from plan
> mode 2026-07-08; home-dir copy deleted per convention). **For execution:** this is the RUN plan
> governing the bulk-run mode for every batch after QA-UICleanup; per-batch §0 plan files still
> get written per checkpoint group (R4/R5). Approved by Jeff 2026-07-08. Gate: QA-UICleanup close.

## Context

Jeff's ask (2026-07-08): the per-batch QA process is too slow — 27 batches closed 2026-05-07 →
2026-07-06 (~2.2 days/batch); 36 batches remain after QA-UICleanup (in flight in a separate
session). At the current cadence that's ~11 more weeks (~250-415 old-model hours). Jeff has a
full folder backup + origin push and explicitly authorized a bulk run with Fable 5: code the
remaining batches without per-task verify cycles, then verify via one large sectioned test
document; batch docs close only when their test section passes.

The slowness is not the code — it's the critical path: every batch serializes through per-task
Jeff-verify round-trips (build → manual test → feedback, often multi-day), plus per-batch
ceremony (plan-mode cycle, running-notes appends per checkpoint, /review-batch, close-entry
compile + apply + close commit). The bulk run moves verify + close ceremony OFF the critical
path; code production runs continuously.

**Gate: the bulk run starts only after QA-UICleanup closes** (its session owns piano-roll menu
surfaces that QA-Chords/QA-H also touch).

## Structural decisions — LOCKED (Jeff, 2026-07-08)

| ID | Call | Pick |
|----|------|------|
| R1 | Run structure | **Groups + ear-checks** — §6 order in checkpoint groups; 15-30 min smoke at each boundary; listening checks inside the two audio-critical stretches (G1 transport/stretch, G2 vocal DSP) |
| R2 | Doc-close model | **Close per section pass** — held Work Log entry + STATUS:CLOSED + close commit applied when a batch's test section passes |
| R3 | /review-batch cadence | **Per checkpoint group** — one review over the group's combined diff |
| R4 | Plan files | **Full §0 files** per batch — verify scripts author into the Master Test Plan instead of pausing execution |
| R5 | Plan approval | **Per group, just-in-time** — group's 4-6 plan files written at group start, approved together |

## Absorption findings (source-verified 2026-07-08, 3 Explore agents)

Recent closes silently absorbed chunks of the remaining §5 scopes. Verified state:

**Collapsed / heavily shrunk:**
- **QA-J → "QA-J-Verify" (~13-18h → ~1-3h code + test section).** DSP-06's headline restructure ALREADY SHIPPED via QA-MultiBlockHazard: CompositeAudioInsertTask.cpp:46-151 sums Flow A + Flow B then runs ONE processInsert (:148-150). Folded item (c) routing unification ABSORBED (route-by-owner c616f0d; BuilderPage.cpp:3304 etc.; idempotent legacy migration PluginProcessor.cpp:2654-2655). Item (a) streamer desync architecturally moot (stateless streamer, transport-derived position; residual = stretch-path mute <2 s staleness, self-corrects via seekNeeded :643-646). Item (b) applicator pruning: no audio-thread map exists (direct APVTS-by-id, self-pruning :1763-1764); only a benign UI-thread map grows (StandaloneEditor.h:599). Remaining: ear-verify DSP-06 + the parked re-verify ledger (below) + optional residuals. **Unblocks QA-B into the campaign.**
- **QA-Ec (~5-8h → ~3-6h).** Stretch-follows-tempo WORKS (vocoder always allocated PluginProcessor.cpp:2702-2708; engages on stretchMode && originalBPM != bpm; default true). STILL OPEN: Resample-follow-tempo (option maps to stretchMode=false → native speed), Rubber Band offline stub (moved to BuilderPage.cpp:4451-4454), hardcoded-120 import default (no content-length/BPM detection), fit-to-grid absent, outSamples<=0 guard on the unstretched path.
- **QA-Fb (~10-16h → ~6-10h).** Dual dry/wet tap ALREADY EXISTS (shipped 2026-05-03: StripRecorder dual recorders PluginProcessor.h:726-758; DRY tap PluginProcessor.cpp:3054 from Vox/InstStripTask live-input branch; WET tap BaySickVocalProcessor.cpp:324-340). Multi-take bleed ROOT-LOCATED: WET tap stays attached while VoxStripTask's FilePlay branch (VoxStripTask.cpp:46-118) plays prior takes through the engine. Remaining: WET-bleed gate, conditional-WET check, **channel-composite renderer** (the real bulk — foundation for F/Fa), clip-resize propagation, dirty-flag investigation.
- **QA-NativeDialogs (~4-7h → ~2-3h).** All 17 juce::FileChooser sites are ALREADY OS-native (launchAsync default). Sole conversion target: ProjectBrowserWindow (ProjectBrowserWindow.h:16, custom Open Project browser). Remaining bulk = default-folder routing audit across the 17 sites.
- **QA-G (~4-6h → ~2-4h).** kNumRows already 50 (BuilderPage.h:423, plan assumed 32→100); zoom already float (mPPBar float :555). Remaining: bump to target count + scroll height, confirm/fix ruler pin (BUILD-02), zoom alignment fix (BUILD-03).
- **QA-H (~8-12h → ~6-9h).** NAV-05 Builder hamburger ALREADY REMOVED (BuilderMenuBar replaced it, BuilderPage.h:1011); folded #15 WAV-drop auto-navigate ALREADY REMOVED (StandaloneEditor.cpp:2046-2104 has no selectTab). Ghost notes = dormant, feed unwired (setGhostData has zero callers — needs a producer). 's' keybind exists but cycles note type (PianoRoll.cpp:1027) — BUILD-05's "dead" claim needs re-spec. Humanize (MIDI-04) doesn't exist — real build.
- **QA-VibeSlider (~5-8h → ~3-5h).** Real count: ~115-120 declaration lines / ~190-200 widgets, concentrated in 5 instrument editors (Harmless 54, Synth 16, Bass 16, VibePlayer 15, Vocal 9). The "~493" figure was total slider-family controls incl. already-themed VKnobs — not swap targets. VibeSlider is a drop-in juce::Slider subclass → mechanical.

**Confirmed full-scope (no absorption):**
- **QA-TempoMap — FULLY OPEN.** (One agent called it done; REJECTED — it verified QA-Ed's shipped single anchor, which is TempoMap's precondition, not its scope.) Replace single re-basing seqlock anchor (StandaloneApp.h:82-85, publishAnchor/deriveBeat StandaloneApp.cpp:145-171) with a sample-indexed multi-change tempo map. Writers: setBPM :228-234, StandaloneEditor.cpp:608/:758/:10741, global_tempo applicator :604.
- **QA-F BaySickAlign** — confirmed paint-only shell (BaySickAlignDSP never instantiated; editor zero attachments; applyWarp = memcpy BaySickAlignDSP.cpp:257-268). **Shape change on DSP-02/03:** the Vox FX pipeline + YIN pitch path are WIRED AND LIVE (BaySickVocalProcessor.cpp:268-359) — the batch is a debug-why-wired-path-fails job (QA-Inventory runtime finding: YIN not detecting) plus the Align build. Brand pass = tiny (2 "Auto-Tune" tooltips, BaySickVocalEditor.cpp:189/:252).
- **QA-Fa BaySickPitch** — confirmed paint-only shell (no DSP class; knobs cosmetic, BaySickPitchEditor.cpp:147-151).
- **QA-Fc NAMIR** — confirmed single-mic chain (BaySickNAMIRProcessor.h:258-259; 18 params; SlotSnapshot = snapshot-swap not a second path). Dual-mic build stands.
- **QA-K** — both items fully open (no MMCSS/SetPriorityClass anywhere incl. VibeThreadPool; no showControlPanel — custom AudioSettingsDialog).
- **QA-I** — no HeavyOperationOverlay exists; load path has audio shield + 30 ms sleep only, zero UI.
- **QA-Export** — doExport() is a literal stub (BuilderPage.cpp:5795-5798); only per-pattern render-to-WAV exists (:5946); NO encoders for MP3/OGG (JUCE_USE_MP3AUDIOFORMAT is decode-only; no vorbis/lame linked) — encoder choice is a spec call.
- **QA-ProjectSave** — WORSE than planned, evidence sharpened: saveTemplateAs writes inline <Drum>/<Layer>/<Bass> children (StandaloneEditor.cpp:6314-6356) but loadTemplate reads ONLY Kit path + presetPath attributes (:6229/:6248) → user templates don't round-trip at all; loadTemplate is destructive (closeAllDynamicTabs :6222); doFileNewFromTemplate + showTemplateMenu both still live.
- **QA-DirtyFlag** — 477 setProperty(...,nullptr) sites / 50 files, BUT the audit filter sharpens: ~300 are DSP preset getState() serialization on detached trees (CORRECTLY nullptr — not user edits); real targets = live-state mutations (PatternManager 105 + UI writes). Main APVTS has nullptr UndoManager (PluginProcessor.cpp:159); central UndoManager lives in StandaloneEditor (.h:300), NOT PatternManager.
- **QA-Chords** — confirmed real build: stamps are N independent notes at fixed snap duration (PianoRoll.cpp:388-399), resize path is single-note only (mResizeNoteIdx :1621-1623) — no chord-as-unit resize; scale state per-roll (PianoRoll.h:350-352, container :689-692).

**Deferred re-verify ledger** (parked items that MUST land in the Master Test Plan under QA-J-Verify):
1. QA-AudioMeters BLOCKER (storeAxes CAS-max) — needs overlapping same-row clips.
2. QA-F/Fa/Fb overlapping-same-row scenarios (§9 eighteenth Forks).
3. DSP-12 matrix under corrected premise ("both flows through the SAME chain") = QA-B content.
4. BUILD-06 resize-rebuild half (PhaseVocoder silence fixed 2026-07-02; resize retest pending).

## NEW batch — QA-TransportDisplay (Jeff, 2026-07-08 at plan review)

Playback-position display on the transport bar (`Source/Standalone/GlobalTransportBar.h/.cpp`):
shows current position as **time** or **bars:beats**, user-swappable (click-to-toggle), live during
playback in BOTH song and pattern mode.
- Reads the QA-Ed int64-sample clock via the existing playhead API (`getPosition`/`deriveBeat`) on
  the UI timer — same seqlock-safe reads the rest of the UI uses; survives QA-TempoMap's anchor→map
  rework unchanged since it consumes the derived position, not the anchor internals. Beats format
  rides the 96 PPQ tick base (QA-Ee).
- Constraint: the combined 40px transport bar must NOT grow — readout compacts into existing width
  (standing no-expand rule).
- Risk: low (UI-only, read-only clock consumer). Effort: ~2-4h.
- **Slot (delegated to me by Jeff): FIRST in G1**, before QA-Chords/QA-TempoMap — the readout is a
  verification aid for the transport work and the whole campaign, so it pays for itself earliest there.
- **Bucket:** UI / L&F / Theming, Cross-cutting Infrastructure.
- Main Plan §5 docket entry + §6 arrow insert: drafted + applied at run pre-flight (bundled with the
  bulk-run §9 Forks entry; also avoids editing plan docs while the QA-UICleanup session owns them).
- Sub-spec calls → marathon item 17: pattern-mode semantics (pattern-relative vs absolute position),
  bars:beats vs bars:beats:ticks format, time precision, display-mode persistence (settings.xml vs
  per-project), and exact placement on the bar (visual pick is Jeff's).

## Three more NEW batches (Jeff, 2026-07-08 at plan review)

Own Main Plan §5 entries + §6 arrow inserts for all three, drafted when Jeff asks post-approval.

**QA-ApvtsAutomation — full APVTS + automation coverage review.** Audit EVERY user-changeable
control across every editor/panel (10 engines + effect panels + mixer + pages): is it APVTS-bound,
does it have proper param setup, is it automatable (right-click → Automate works, stable
componentID, attachment present). Fix every gap found. Natural superset of three items currently
folded into QA-L — BLU-378 (componentID on sliders), BLU-379 (SliderAttachment sync verify),
BLU-492 (combo-box → APVTS params, **PRESET-BREAK**) — proposal: migrate those three out of QA-L
into this batch (marathon confirm). PRESET-BREAK means this MUST land before the campaign's
per-engine preset walk and before QA-Templates factory presets.
- **Proposed slot: G4**, after QA-NativeDialogs′ — after the feature builds (G2 adds ~30-45 vocal
  params) so the audit covers the final param surface. Risk: medium (combo-param infra + preset
  format). Effort: ~6-10h. **Bucket:** Cross-cutting Infrastructure, UI / L&F / Theming.

**QA-UndoCoverage — app-wide undo-history coverage review.** Everything the user can change must
register in the central UndoManager (StandaloneEditor.h:300). Audit every mutable surface —
pattern/note edits, arrangement, mixer, engine params, effect params, page state, renames — and
wire the gaps (ParameterAttachments or explicit transactions). This is the "Strict UndoManager
Plumbing" half of QA-DirtyFlag's §5 spec, promoted to its own batch per Jeff; **QA-DirtyFlag
re-scopes to the transaction-pointer system on top of it** (its correctness depends on this
coverage being complete — clean layering). Sizing intel from today: 477 setProperty(...,nullptr)
sites of which ~300 are detached-tree preset serialization (correctly nullptr, out of scope);
real targets = PatternManager (105) + live UI writes; main APVTS currently has a nullptr
UndoManager (PluginProcessor.cpp:159).
- **Proposed slot: G4**, immediately before QA-DirtyFlag (last two of the group; both sit after
  QA-ProjectSave so the audit covers its new state sites, per DirtyFlag's existing dependency
  logic). Risk: medium-high (codebase-wide). Effort: ~6-10h (with DirtyFlag's remainder ~6-10h).
  **Bucket:** Cross-cutting Infrastructure, System Pages, UI / L&F / Theming.

**QA-LegalReview — full legal review at the end.** Nothing shipped in code, comments, UI strings,
assets, presets, templates, manuals, or the installer fails legal standards. Scope: `/audit-licenses`
sweep (vendored libs, fonts incl. the TTF embed, sample packs, IR/asset attribution, EULA text);
tree-wide brand/trademark sweep of USER-FACING strings + asset/preset/template names + manual text
(semantic agent sweep, not keyword grep — the UREI lesson); comment check verifies factual
modeled-product references stay within nominative fair use per the standing no-brand-names rule
(comments MAY factually reference modeled gear; the review confirms compliance, it does not
mass-scrub). Fix everything found.
- **Proposed slot: G6**, after QA-Manuals + QA-Templates and **before QA-Installer** — so the
  installer only ever bundles cleared content; quick re-check of any late additions at ship.
  Risk: low (review + targeted fixes). Effort: ~4-8h with agents. **Bucket:** Other/Platform,
  Meta, UI / L&F / Theming.

## Execution model

### Pre-flight (1-2 sessions, after QA-UICleanup closes)
1. Confirm QA-UICleanup closed + clean tree; check QA-Rules stale `STATUS: OPEN` (Main Plan:1176) — close or confirm why open.
2. `git tag pre-bulk-run` + confirm origin synced (backup already done per Jeff).
3. **Spec-call marathon** — walk the docket (below) in one sitting; locked answers appended to this plan. Rule 5 satisfied up front instead of 24 separate plan-mode pauses.
4. Create `Plans & Specs/Test Plans/` + `v1-master-test-plan.md` skeleton; add §0 folder-registry line (surface drafts for approval; apply via Edit).
   *DONE 2026-07-08 (Jeff's post-approval ask):* Main Plan §5 dockets + §6 arrow/footnotes (markers 37-40) for the four added batches + the §9 **fifty-fifth Forks entry** (which also records the bulk-run mode adoption — no separate mode entry needed at run open). QA-L intentionally unchanged pending docket-18 confirm.
   *Session prompt:* `Files For Claude/bulkrun_group_session_boilerplate.md` is the paste-in prompt for every run session (GROUP: value swapped per session).
5. ~~Mirror this plan to `Plans & Specs/Batch Plans/` under a proper silly-name; delete home-dir copy~~ — DONE at ExitPlanMode 2026-07-08 (this file; home copy deleted).

### Per-group loop (R1/R3/R4/R5)
1. **Group start:** write the group's full §0 plan files (4-6), informed by current code state; surface together; Jeff approves once.
2. **Per batch:** code all tasks → Jeff runs `do_build.bat` (Release+Debug) → I fix until clean → surface commit message + full git status → Jeff approves → commit (one commit per batch, Rule 9 format). Author the batch's Master Test Plan section from the plan file's verify scripts (scenarios derived from actual page/component code, physically executable). Draft + hold the Work Log close entry. Append running-notes entry (code-complete + findings; no per-task verify entries exist to log).
3. **Group boundary:** `/review-batch` over the group's combined diff (findings fixed before proceeding, per the fix-or-reframe canon); Jeff runs the 15-30 min smoke — Debug exe first (jassert dialogs), then Release: launch, audio plays, big project loads, save/close/reopen round-trip (§7 items 1-5). G1/G2 add the ear-checks below.
4. **Mid-run spec calls (Jeff's lock, 2026-07-08): EVERY spec call discovered during execution gets ASKED — never self-decided.** When one surfaces, I stop that piece, pose the options in chat, and while waiting I may only continue work that does NOT depend on the answer (never code past a call on a guess). Accepted cost: a little time per call; payoff: far less fix-it-in-testing churn. This is Rule 5 + `feedback_dont_make_unilateral_spec_calls` applied to execution, unchanged by bulk mode. Non-spec findings (plain bugs, dead code, etc.) log in running notes and route per Rule 3 at that batch's doc-close (section pass), as usual — and real bugs found mid-batch still get fixed in-batch per the standing rule.
5. **QA-ClipDrop trap fires** → capture evidence + log; fix at next boundary (immediately if it blocks the run).

### Checkpoint groups (§6 order preserved; boundaries adjustable at marathon)
- **G1 — Transport + timeline foundations** (ear-check group): **QA-TransportDisplay**, QA-Chords, QA-TempoMap, QA-Eb, QA-Ec′. Ear-check: tempo-change sample accuracy by ear (position readout assists), stretch/resample behavior, chord stamping. **G1 completion = go/no-go checkpoint for the whole model.**
- **G2 — Vocal/creative builds** (ear-check group): QA-F, QA-Fa, QA-Fb′, QA-Fc. Ear-checks mid-group: after QA-F (align/warp quality) and after QA-Fa (pitch-edit quality) — pitch/align DSP cannot be judged by smoke tests, and Fa builds on F's shifters.
- **G3 — Builder/UX/engine polish:** QA-G′, QA-H′, QA-I, QA-J′(residuals only), QA-K (code items; DSP-08 hardware test → campaign), QA-L, QA-M, QA-Drum-Polish, QA-N. Nine batches, three shrunk small.
- **G4 — Mechanical sweeps + data layer:** QA-VibeSlider, QA-NativeDialogs′, **QA-ApvtsAutomation**, QA-Verify (only the BaySickPedals preset FIX is code; the 10-engine walk → campaign), QA-Export, QA-ProjectSave, **QA-UndoCoverage**, QA-DirtyFlag.
- **CAMPAIGN — Master Test Plan execution** (absorbs QA-B + QA-Verify walk + QA-RC's page-by-page plan): Jeff walks sections in commit order; failures → I fix (fix commits reference the batch) → re-run scenario; **section pass → apply held Work Log entry + Main Plan STATUS:CLOSED + close commit for that batch (R2)**.
- **G5 — Phase 6:** QA-Audit (docket decisions pre-resolved at marathon) → QA-Cleanup-1 → QA-PlayerRename → QA-Cleanup-2 → QA-Cleanup-3 → QA-Cleanup-4 — build-after-every-delete discipline unchanged. Then **QA-RC-lite**: 2nd clean build (delete build/, full rebuild, warning audit) + test-to-failure soak + a regression spot-pass (full page-by-page already ran in the campaign).
- **G6 — Phase 7:** QA-Manuals, QA-Templates, **QA-LegalReview**, QA-Installer, QA-Updater, QA-Framework. Deferred call D-1: whether Manuals/Framework drafting overlaps the campaign (they're doc work; I can draft while Jeff tests).

### Master Test Plan (`Plans & Specs/Test Plans/v1-master-test-plan.md`)
Sections, in this shape:
- **§A Global smoke ladder** — §7 items 1-5 (also reused at every group boundary).
- **§B Per-batch sections (commit order)** — numbered scenarios from each batch's plan-file verify scripts: steps, expected result, Debug-first/Release-confirm, checkbox, PASS/FAIL + notes, and a `blocks:` field naming the batch commit for bisect on failure.
- **§C Deferred re-verify ledger** — the 4 parked items, inside QA-J-Verify's section.
- **§D Cross-cutting regression** — §7 cross-batch multi-take session; MT stress arrangement; DSP-meter sanity across buffer sizes.
- **§E Preset + patch-save walk** (QA-Verify content, expanded per Jeff 2026-07-08) — four families, all round-trip-verified (save → reload → identical state + audio), not just menu-clicked:
  1. **Engine presets:** 10 engines × every factory preset loads with all params restored + audio as expected; user preset save/reload identical; presets survive project save/load. Runs AFTER both PRESET-BREAKs (QA-ClipPlayback bipolar-stereo, QA-ApvtsAutomation BLU-492).
  2. **Effect-rack presets:** per-effect preset menus + `EffectPresetIO` factory presets (e.g. "Slapback") — load applies correct state; user effect-preset save/reload round-trips.
  3. **Engine "Save Current Patch As..." flows:** LayersPage (LayersPage.cpp:562), BassPage (:537), DrumPage (:1086 + synth submenu :577 + per-drum context menu), ClipsPage (:141) — saved patch reloads identical via the picker.
  4. **Page-level "Save Page Preset As..." / Load flows (the hamburger-menu page-save path):** all 7 page types — LayersPage (:1059), BassPage (:970), DrumPage (:1506), ClipsPage (:397), VoxPage (:115/:307), InstPage (:259/:629), Rusty kit menu (StandaloneEditor.cpp:5095) — save → delete page → reload from preset → full page state restored; PLUS the "Save Page Preset & Delete" 3-button prompt path on page delete, and BaySickPedals "Save as Default" (BaySickPedalsEditor.cpp:560). (Line refs = 2026-07-08 snapshot; re-resolve at test-plan authoring.)
- **§F RC-grade audits** — menu walk, keybind audit, tooltip review, global FX bypass. Split: I pre-audit by code-read and produce candidate-discrepancy lists; Jeff spot-verifies the flagged items instead of walking everything cold.
- **§G Test-to-failure** — long-soak scenarios (100 tracks, 50 clips, hours-long playback, SR/buffer switches) — scheduled as background soaks during G5.

### Doc-discipline adjustments (one §9 Forks entry at run open documents all of this)
- Work Log entries: drafted at code-complete, **held**, applied at section pass. Append-only preserved — entries just land later.
- Main Plan §5 STATUS lines: flip at section pass. §9 gets the one bulk-run entry + normal close-routing entries as sections pass.
- Running notes: per batch, paired with plan files (R4), appended at code-complete/findings/spec-resolutions — not per-verify.
- Rules 3 (routing), 4 (diagnostics catalog), 5 (spec calls to Jeff), 6-9: unchanged. The marathon front-loads the KNOWN calls; anything discovered mid-run is asked in the moment per the protocol above — no self-picks under bulk mode, period.
- Commit approval + full-git-status surfacing per commit: unchanged (piggybacks on the per-batch build interaction — zero extra round-trips).

### Spec-call marathon docket (walked at pre-flight; answers locked into this plan)
1. QA-J collapse to QA-J-Verify + residual list — confirm.
2. QA-Ec′ re-scope + Resample-follow-tempo model (varispeed-to-tempo) + fit-to-grid shape + import content-length default.
3. QA-Fb′ re-scope (dual-tap done; WET-bleed gate shape) — confirm.
4. QA-H: BUILD-05 re-spec ('s' now cycles note type — what did "dead" mean?); ghost-notes producer (which slots feed ghosts?); Humanize spec (amount/targets); control-lane dots-follow-slider spec.
5. QA-G: BUILD-01 target track count (current 50; plan said 100).
6. QA-NativeDialogs: ProjectBrowserWindow convert-or-keep; default-folder policy (fixed-per-context vs last-used).
7. QA-Export: encoder set (WAV-only / +OGG via JUCE / +MP3 via vendored LAME) + option surface (bit depth/SR/tail) + bundle zip shape.
8. QA-ProjectSave: sample-retention model confirm (source-aware hybrid + Pack lean from QA-Ef close) + existing-projects migration approach.
9. QA-DirtyFlag: confirm audit filter (live-state mutations only; detached-tree serialization stays nullptr) + which UndoManager becomes global authority.
10. QA-K: DSP-08 Tascam hardware availability window; DSP-11 in-or-out.
11. QA-TempoMap: map data model (tempo-change list UI entry point — tempo automation lane vs markers).
12. QA-Audit pre-release decisions docket (AlertWindow migration / security agent / crash pipeline / DSP-meter cap / MT-diag compile flag / HarmlessLAF zero-px root cause).
13. QA-Templates: factory preset quantities + genre template list confirm.
14. D-1: Phase 7 doc-drafting overlap with the campaign.
15. Optional scope-cut lever (Jeff's call, no lean): net-new features that could defer post-V1 for more speed — QA-Chords, QA-Fc, QA-Eb, QA-Drum-Polish, QA-Ec′ Resample-follow, Manuals 2-3 post-release. Or keep all.
16. Group boundaries confirm (G1-G4 composition above).
17. QA-TransportDisplay sub-spec calls: pattern-mode position semantics (pattern-relative vs absolute); bars:beats vs bars:beats:ticks; time format precision; display-mode persistence (settings.xml vs per-project); placement on the 40px bar (no-expand rule — visual pick is Jeff's).
18. QA-ApvtsAutomation: confirm BLU-378/379/492 migrate out of QA-L into it; accept BLU-492's PRESET-BREAK (combo selections enter preset format) before factory presets are authored.
19. QA-UndoCoverage ↔ QA-DirtyFlag boundary confirm (coverage sweep vs transaction-pointer system) + which UndoManager is the single global authority (main APVTS currently gets nullptr).
20. QA-LegalReview: confirm comment standard = nominative fair use per the existing no-brand-names rule (review verifies, doesn't mass-scrub) + sweep depth for asset/preset/manual content.

## Risks (stated plainly)
1. **Stacked-defect cost:** a bug found at campaign time may sit under later batches. Mitigation: §6 order keeps foundations early, per-batch commits give bisect points, group smokes + ear-checks bound blast radius. Debugging at campaign time will still cost more than per-task verify would have — that's the accepted trade for speed.
2. **Blind DSP building:** QA-F/Fa quality (align/warp/pitch artifacts) can't be smoke-tested; expect tuning cycles at ear-checks and campaign. G2's mid-group ear-checks exist for exactly this.
3. **Spec drift:** history says batches grow mid-execution (QA-VoicePool 3 corrections, QA-SfzGroup 2 expansions). The marathon front-loads known calls; discoveries are asked in the moment (Jeff's ask-always lock) — this trades a little run speed for far fewer wrong-guess fixes at campaign time. Some genuine rework is still inevitable where a finding invalidates earlier code.
4. **Campaign is Jeff-hours-bound:** est. 15-25 focused hours of testing. Sectioning + the §F pre-audit split keep it tractable.
5. **Docs lag reality** during the run (held entries). Bounded by per-section closes; the running notes + this plan file remain the live record. Carry-over blocks at every session stop (§0 Rule 2) stay mandatory — the run spans many sessions.
6. **Concurrent session:** do not start until QA-UICleanup closes.

## Timeline estimate (honest)
- Pre-flight 1-2 sessions → G1 ~2-3 → G2 ~3-4 → G3 ~3-4 → G4 ~4-5 sessions (grew: +QA-ApvtsAutomation, +QA-UndoCoverage) (code phase ≈ 2.5-3.5 weeks at normal availability; Jeff's per-batch involvement = one build + one commit approval, plus ask-in-the-moment spec calls).
- Campaign: 1-2 weeks Jeff-paced, me fixing in parallel.
- Phase 6: ~1 week. Phase 7: ~1-2 weeks (Manuals dominate; D-1 overlap can hide some of it).
- **Total ≈ 5-7 weeks vs ~11-week baseline** — the win is removing ~24 ceremony cycles and 100+ per-task verify round-trips from the critical path, plus the ~25-35h of already-absorbed scope found today. Not magic, and campaign-time debugging eats some of it back if foundations wobble — hence the G1 go/no-go.

## Files
- This plan (mirrored to `Plans & Specs/Batch Plans/` at pre-flight; home copy deleted).
- New: `Plans & Specs/Test Plans/v1-master-test-plan.md` (+ §0 registry line).
- Main Plan: one §9 Forks entry (bulk-run mode) at run open; §5 docket entries + §6 inserts for the 4 added batches (on Jeff's post-approval ask); §5 STATUS flips + close routings at section passes.
- Per group: 4-6 full §0 plan files + paired running notes in the standard locations.
- Everything else: per-batch source files as scoped in §5 / re-scoped above.

## Verification
1. **Pre-flight verifiable:** marathon table locked in this plan; test-plan skeleton + §0 line + §9 entry applied; tag pushed.
2. **G1 = go/no-go:** 5 batches coded/built/committed ("4" corrected 2026-07-08 — the line predated QA-TransportDisplay's insertion), group review clean, smoke + ear-check pass. If G1's smoke fails badly or the cadence doesn't feel right, stop and reassess — the pre-bulk-run tag + backup make abort cheap.
3. **Run success =** every §B section passed + closed, §C ledger cleared, §D-§G passed, Phase 6 clean 2nd build, QA-RC-lite green — at which point the Main Plan shows all V1 batches CLOSED and Phase 7 ships the release artifacts.

## Marathon answers — LOCKED 2026-07-08 (pre-flight, first bulk-run session)

Walked in full with Jeff in one sitting (chat, numbered options, no recommendations) plus three
follow-up rounds. These are the working picks, NOT immutable law: any answer whose premise later
gets invalidated comes back to Jeff as a fresh call (the ask-always lock). Sub-details explicitly
deferred are in the "Deferred to group start" ledger below. §5 scope updates implied by these
answers land per each batch's normal close routing (Rule 3 / R2), not eagerly.

| # | Call | Locked answer |
|---|------|---------------|
| 1 | QA-J collapse | Collapse to **QA-J-Verify** (campaign section: ear-verify DSP-06 + the §C ledger). The two residuals (stretch-path unmute staleness; UI-thread map growth) get fixed as small code items in **G3**. |
| 2a | QA-Ec' remainder | Confirmed: Resample-follow, RB-stub replacement, import default, fit-to-grid, outSamples guard. PLUS block-RESIZE re-fit is core scope — fitRatio derives from block length, so resizing a block re-fits playback (this is where "stretch the block and it changes what plays" lives; BUILD-06 (QA-H) + QA-Fb' clip-resize propagation are the downstream fixes, expected to shrink to verification once Ec' lands). |
| 2b | Resample/Stretch model | DUAL-TRIGGER on both tempo change AND block resize. Resample = varispeed (rate + pitch shift together). Stretch = time-stretch, pitch locked. |
| 2c | Import placement | *(CORRECTED by G1 answer G, 2026-07-08 — the "nearest whole bar" wording was Claude's option text, rejected by Jeff.)* Automatic at import: the clip lands at its TRUE wall-clock length converted at the current project tempo — exact, never rounded — and plays 1:1 at that position. (Deliberate pairing with the QA-TransportDisplay readout: where it lands is actually how long it is.) |
| 2d | Import BPM default | *(CORRECTED by G1 answer G, same sitting.)* `originalBPM` = the project tempo at import (so the clip is unstretched at import; ratio = 1); block length = file duration in beats at that tempo. Replaces hardcoded 120. No bar rounding. "Fit-to-grid" as a separate feature is DEAD — what remains is tempo-follow (2b) + Shift+drag re-fit (F below). |
| 3a | QA-Fb' remainder | Confirmed: WET-bleed gate, conditional-WET, channel-composite renderer, clip-resize propagation, dirty-flag investigation. |
| 3b | Bleed gate behavior | New take = live input ONLY; prior-take playback never bleeds in. No bounce toggle. |
| 4a | BUILD-05 / S key | S already works (cycles Standard/Slide/Portamento; converts selections; silently arms the new-note type with no selection — that invisibility was the "dead" report). Spec: visible active-note-type indicator + S converts selections; exact interaction (no-selection behavior, indicator click) pinned at G3/QA-H plan. D-8 fold (below) covers Note Properties + verifying slide/porta DSP audibly works. |
| 4b | Ghost notes | Feed from ALL instrument tabs project-wide, each ghost tinted with its source tab's piano-roll color shade. |
| 4c | Humanize | Replicate FL Studio's Humanize dialog — same adjustable ranges + same presets. Reference capture (Jeff's FL screenshot) at G3 start per the exact-reference-replica rule. |
| 4d | MIDI-02 | Modifier + mouse-drag across selected notes scrubs the active control-lane value up/down (FL-style gesture). Detail at G3 plan. |
| 5 | QA-G tracks | Keep 50 stock rows. NEW scope: track-row right-click menu — Insert track above / Group with above (visual signifier) / Remove from group / Color group. Groups are VISUAL-ONLY for V1 (no linked behavior). |
| 6 | QA-NativeDialogs' scope | Audit runs from the user's seat: EVERY file/project/template/preset pick surface converts to native, whatever widget currently draws it (the "17 sites already native" finding covered only FileChooser-based surfaces). 6a: Open Project browser -> native. 6b: fixed per-context default folders. New-from-Template: stays QA-ProjectSave's locked submenu design (no file dialog there). |
| 7a | Export formats | WAV + OGG + MP3 (vendor LAME; the new license surface routes to QA-LegalReview). |
| 7b | Export options | Minimal + quality setting for lossy: format / bit depth / SR / tail / quality. |
| 7c | Bundle shape | Both offered: single .zip AND plain-folder export. |
| 8a | Sample retention | Source-aware hybrid (library = reference, volatile = copy) + explicit "Pack project" action — for ALL project saves; templates inherit. |
| 8b | Migration | Existing per-project copies left as-is; the hybrid applies going forward. |
| 9a | Dirty audit filter | Live-state mutations only (~PatternManager 105 + live UI writes); detached-tree preset serialization stays nullptr. |
| 9b | Undo authority | Processor-owned UndoManager, handed to the APVTS at construction; StandaloneEditor's manager retires in favor of it; param changes become undoable + count toward the dirty pointer. (Also answers docket 19's authority half.) |
| 10a | DSP-08 hardware | Tascam Model 24 available during the campaign — hardware test lands there. |
| 10b | DSP-11 | IN, feasibility-gated: build live ASIO buffer-size change + diagnose the prior crash as part of it; if the driver layer provably can't do it safely, surface evidence and fall back to the documented workaround (Jeff decides). |
| 11 | QA-TempoMap model | Ruler flags/markers = STEPPED tempo-change points — the sample-indexed map stays a stepped list. RAMPS come via tempo AUTOMATION (the existing global_tempo path), not the map. Marker <-> automation precedence pinned at G1 plan write. |
| 12a | AlertWindow migration | Migrate-as-sweep in QA-Cleanup-1. |
| 12b | /audit-security agent | Build it pre-RC-lite (Tier-1 sweep runs before V1); Tier-2 once QA-Updater's network code exists. |
| 12c | Crash reporting | V1 = OS-native WER + per-release .pdb archival; no third-party SDK / symbol server. |
| 12d | DSP meter cap | 2.0 (200%) for V1 Release (applied in Phase 6). |
| 12e | MT diagnostic | Compile-flag gate (#if BAYSICKDAW_MT_DIAGNOSTIC) — out of V1 Release builds. |
| 12f | HarmlessLAF zero-px | Investigate root cause at QA-Audit as planned. |
| 13a | Genre templates | Based on the on-disk preset style groups (verified: no Jazz/Rock group exists on disk); specific genre picks at QA-Templates open with /preset-gaps in hand. |
| 13b | Factory presets | Existing presets' correctness = campaign §E (locked). QA-Templates authoring is gap-driven: /preset-gaps reports, Jeff decides what gets authored at batch open. |
| 14 | D-1 + test cadence | D-1 = YES: draft Manuals/Framework during the campaign (all code done by then; campaign fixes fold into drafts). Group boundaries stay smoke + ear-check only (model as approved — full section walks happen once, in the campaign). |
| 15 | Scope cuts | NONE — keep all. All 3 manuals ship at launch ("Manuals 2-3 post-release" struck — it was a run-plan lever candidate, never plan-of-record). |
| 16 | Group composition | G1-G4 confirmed as laid out. |
| 17a | Pattern-mode position | Pattern-relative (resets each loop pass). |
| 17b | Beats format | bars:beats:ticks (96 PPQ). |
| 17c | Time format | M:SS.mmm. |
| 17d | Persistence | settings.xml (app-wide). |
| 17e | Placement | Between the pattern dropdown and the ribbon; layout must respect the reserved ~40px keyboard-MIDI slot next to the metronome (D-4 fold below). |
| 18 | QA-ApvtsAutomation | Confirmed: BLU-378/379/492 migrate OUT of QA-L into it; BLU-492 PRESET-BREAK accepted now (before factory presets are authored / the preset walk runs). |
| 19 | UndoCoverage/DirtyFlag | Boundary confirmed (coverage = plumbing audit + wiring; DirtyFlag = transaction pointer on top). Authority = 9b. |
| 20a | Comment standard | Nominative fair use verified, not mass-scrubbed: anything failing the legal standard gets scrubbed; compliant factual references stay. |
| 20b | Sweep depth | Full semantic sweep over ALL user-facing content (UI strings + every asset/preset/template/kit name + complete manual text + installer/EULA). |

### Marathon-discovered findings + scope changes (locked same sitting)

- **Triage gap (2026-05-08 FSW triage):** Final Stretch Work Phase-D items **D-4** (Typing keyboard -> MIDI, Ctrl+T; only the reserved ~40px transport-bar slot next to the metronome ever shipped — GlobalTransportBar.cpp:200/:856), **D-6** (Riff Machine, Alt+E), and **D-8** (Note Properties dialog + slide/porta DSP fix) were never routed to Main Plan / Future State (name-grep verified across all plan docs; every Phase-D sibling is shipped or routed). Routing locked by Jeff: **D-4 -> QA-TransportDisplay (G1)** — that batch roughly doubles (~2-4h -> ~5-8h); **D-6 -> QA-H (G3)**; **D-8 -> QA-H (G3)**.
- **Scope growth locked:** QA-G (+ the item-5 track right-click set), QA-Ec' (+ dual-trigger resize re-fit), QA-H (+ D-6 + D-8 + the 4a-4d specs), QA-TransportDisplay (+ D-4). QA-NativeDialogs' audit basis corrected (user-seat enumeration of ALL custom pick surfaces, not FileChooser-site count).
- **QA-TempoMap** keeps the existing global_tempo automation path as the RAMP mechanism; the new map is stepped markers only.

### Deferred to group start (the ask-in-the-moment ledger)

| When | What |
|------|------|
| G1 plan write | D-4 sub-specs (key layout / octave range / velocity behavior); tempo marker <-> tempo-automation precedence — **ANSWERED, see next table** |

### G1 plan-write answers — LOCKED 2026-07-08 (same sitting, second round)

| # | Call | Locked answer |
|---|------|---------------|
| A1 | D-4 key layout | Two-row DAW convention: Z-row = lower octave whites (+ S D G H J blacks), Q-row = upper octave (+ number-row blacks). |
| A2 | D-4 octave shift | Yes — PgUp/PgDn, +/-1 octave, clamped (Jeff delegated PgUp/PgDn-vs-arrows; arrows collide with roll navigation, so PgUp/PgDn). |
| A3 | D-4 velocity | Fixed 0.8 (matches placed-note default). |
| A4 | D-4 record vs audition | Full hardware-MIDI-keyboard parity — typed notes record. Mechanism: inject noteOn/noteOff into the live-MIDI collector (chord-safe; recorder + on-screen keyboard + active-tab dispatch for free). |
| A5 | D-4 targeting | Active tab's engine (existing live-MIDI target follows focus). Confirmed. |
| B | Marker <-> automation precedence | Last-writer-wins: markers set tempo at their bars; automation writes override while playing; markers re-assert at their boundaries. |
| C | QA-Eb shape | Min-size clamp; NO outer Viewport (premise correction: no fixed design size exists; layout already proportional; a wrap would double-wrap the four self-scrolling surfaces). Floor derived by Claude, tuned by Jeff at the G1 smoke. Batch shrinks ~3-5h -> ~2-3h. |
| D | Chord-as-unit resize mechanism | Selection-based multi-note resize: grabbing an edge when multiple notes are selected resizes ALL selected (the stamp already leaves the chord selected). Groups (Shift+G) stay what they are — manual, for permanent move-together. NO auto-grouping of stamps. |
| E | BPM field once markers exist | Field EDITS the base tempo (bar 1 until the first marker); field DISPLAYS live effective tempo during playback as markers/automation change it. |
| F | Which drag re-fits | Plain right-edge drag = trim/extend (unchanged). Shift+drag = re-fit (varispeed in Resample / pitch-locked stretch in Stretch, per 2b). |
| G | Import behavior | See corrected 2c/2d rows above — true-length placement at project tempo, `originalBPM` = import-time project tempo, plays 1:1, no rounding, no import-time stretch. |
| G3 start | 4a exact S-key/indicator interaction; 4c FL Humanize reference capture (Jeff screenshot); D-6 Riff Machine spec; D-8 Note Properties spec; **Builder pattern-block NOTE-PREVIEW spec (Jeff, G1 smoke):** the mini notes drawn ON arrangement pattern blocks must sit at their true musical positions within the block's viewport - 1 bar of notes fills exactly 1 bar of block regardless of the block's length or stretch (1-bar notes on a 2-bar block occupy only the first half) |
| G6 start (QA-Templates open) | 13a specific genre picks; 13b gap-fill decisions (with the /preset-gaps report) |

## Carry-Over — 2026-07-08 (G1 boundary, first bulk-run session)

- **Completed:** Pre-flight in full (QA-UICleanup close `f2001c9`; `pre-bulk-run` tag pushed on it;
  marathon + G1 plan-write answers locked above; test-plan skeleton + §0 registry; QA-Rules flip).
  G1 group opened (5 plan files + seeded notes, `0e5fed5`). **All five G1 batches code-complete,
  built clean per batch, committed:** QA-TransportDisplay+D-4 `d6d46cf` / QA-Chords `805ca03` /
  QA-TempoMap (+readout z-order fix) `753dddc` / QA-Eb `44d5c01` / QA-Ec `67bd4f6e`. Master Test
  Plan §B.1-B.5 authored (13/11/14/6/11 scenarios); held Work Log entries in all five running
  notes. G1 group review ran (R3): 1 BLOCKER (Play/resume rewrote the base tempo via the
  now-effective field value) + 2 NEEDS-FIX (setLiveTempo segment growth; typing-note drone on app
  deactivation) + 2 NITs — ALL FIXED; 1 NIT logged-deferred (stopped-rebuild override asymmetry,
  steady-marching-ibex notes). Review outcomes recorded in each batch's held entry.
- **In-flight:** the G1 review fixes + TM-13/TM-14 scenarios + review-outcome note fills are
  UNCOMMITTED in the tree (Jeff's call: one boundary commit after the smoke, folding any smoke
  findings). Both configs build clean on this tree.
- **Smoke finding #1 (PRE-EXISTING, fixed at boundary):** switching between two ASIO devices never
  took - the settings dialog preserves the old input-device name (no input picker, by design
  2026-04-30), ASIO cannot open a mismatched in/out pair, and initialise fell back to the old
  device; the empty-input-only net (StandaloneApp.cpp ~:662) never covered the switch case.
  Evidence: Jeff's Apply->Restart loop stuck on ASIO4ALL vs a "Model Mixer ASIO" pending +
  fallback-shaped audio_setup_log.  Fix: force input=output for ASIO in the settings XML BEFORE
  initialise (StandaloneApp.cpp, pending-promote block).  Rule 3 routing at section pass: QA-K
  owns the audio-system polish surface (DSP-08 is Tascam-adjacent).  **Smoke finding #2** (MIDI
  keyboard "not detected") under diagnosis - dialog enumeration returned OS-level empty while
  saved MIDIINPUT entries exist; environmental vs code TBD on Jeff's FLkey-presence check.
  **Finding #1 THEORY CORRECTED** after Jeff's "switched many times before" pushback + vendored
  JUCE read: ASIO createDevice uses the OUTPUT name (mismatch does NOT fall back) - the in=out fix
  stays as hygiene (JUCE's own suppressed jassert documents the pattern) but the real failure is
  upstream: createDevice returns null when the requested driver is MISSING FROM THE ASIO SCAN,
  and the manager silently falls back - i.e. the Tascam driver may not have been enumerable at
  that launch (rhymes with the FLkey being un-enumerable in the same session; two USB devices).
  **Smoke findings RESOLVED:** #2 was environmental (Jeff's USB switcher box wedged BOTH the
  Tascam ASIO driver and the FLkey out of enumeration; his diagnosis) - the new startup
  diagnostics stay (Keep).  #1's in=out fix stays as hygiene.
  **Smoke finding #3 (MISSED LOCKED SCOPE from QA-UICleanup, fixed at boundary):** Jeff's
  original instruction for that batch included restyling the BUILDER GRID's own snap control
  (param Unified_BuilderSnapDiv, independent of the rolls) to the magnet-button pattern - it
  never made the docket/plan (capture miss at the docket write, verified by grep of the plan
  file; my miss).  Fixed: "Snap:" label + ComboBox replaced by a magnet-style Snap button, FIRST
  on the Builder toolbar ahead of Draw (Jeff's placement pick), click = 11-value menu with live
  tick, highlight = snap active; load-restore re-syncs the highlight
  (BuilderPage.h/.cpp; new toolbar onGetSnapDiv callback).  §9 back-ref to QA-UICleanup rides
  the section-pass routing.
  **Smoke finding #4 (REAL BUG, fixed): clip crackle + position error across tempo flags.** Jeff's
  full-song clip crackled from the marker onward in BOTH modes.  Two stacked causes, both fixed:
  (a) `AudioClipStreamer::readAndMix` took an INTEGER file position while the follow/varispeed
  rates are fractional -> the fractional position truncated at every block boundary = sub-sample
  click ~86x/s (his 44.1k file at ratio 1.0 was integer-clean pre-marker, which is why it started
  "instantly" at the flag).  Signature -> double; fraction survives; fast path gated on integral
  start; lookahead +2 -> +3.  (b) STRUCTURAL: all clip file positions were linear elapsed*rate -
  wrong the moment tempo changes mid-clip (position jump at the marker; the PV seek-net absorbed
  <2s of it silently).  Fixed with the BEAT-DOMAIN mapping: file consumption per musical beat is
  tempo-independent in both modes (stretch pins it; resample's follow term cancels:
  (bpm/orig)(SR*60/bpm)(fileSR/SR) = fileSR*60/orig) -> posD = contentBase + beatsIntoClip *
  fileSR*60*vsKnob/originalBPM, exact through any number of steps, fractional end to end.  Both
  render paths (A + B), PV refs + direct reads + expectedFilePos.  Reverse keeps linear (RAM-only
  corner; noted).  Any residual stretch-mode grit after this = PhaseVocoder quality on full-mix
  material (bounded; ear-check at retest).  Routing: QA-Ec surface (its own §B section).
  **Smoke finding #5 (locked-scope gap in my F implementation, fixed):** the toolbar Slip/Stretch
  EDIT MODE never triggered the re-fit - I gated stretching on Shift only, but Stretch mode's
  stated purpose was always "drag = time-stretch (QA-Ec ships it)".  Fixed: mStretching = Audio &&
  (Shift || EditMode::Stretch).  Jeff's F pick refines to: Slip mode = plain drag trims, Shift
  re-fits; Stretch mode = plain drag re-fits.
  **Smoke finding #6 (Jeff request, added to QA-Eb):** window size/maximized state now persists in
  global settings.xml (`<WindowState>`; maximized close -> reopens maximized, sized close ->
  reopens at those bounds; first launch unchanged).
  **Smoke finding #7 (REAL BUG, fixed; Jeff's retest - crackle persisted in Stretch mode):** the
  #4 fixes covered the DIRECT read path only; the PHASE-VOCODER output pull had its own version of
  the same defect class, latent since the vocoder landed: `pull()` consumes-and-clears everything
  it returns, so the +2 interpolation LOOKAHEAD was discarded every block = a 2-sample skip per
  buffer (~86 clicks/s) on every stretched clip, plus the interp fraction restarted at zero per
  block.  Fixed with a peek/advance split on PhaseVocoder (`peekOutput` = copy without consuming;
  `advanceOutput` = consume exactly the fractional true advance + do the OLA slot-clearing) + a
  per-player `pvOutFrac` carrying the phase across blocks (reset with vocoder->reset on seeks);
  both render paths.  ALSO fixed in the same pass: the streamer's ring read-head advanced past
  the interp lookahead every block, marking still-needed samples overwritable (background-thread
  race on streaming files) - now advances by floor(true consumption) only.  **Open semantics
  question from the same retest (slip-revert):** stretch-mode drag bakes the re-fit into the
  clip's tempo identity; a later Slip-mode resize back trims WITHOUT undoing the stretch (Slip
  never touches speed by design/F) - Jeff expected revert-by-resize to restore normal speed;
  options posed, his pick pending.
  **Smoke follow-on #8 (Jeff picks, all built): stretch visibility + tempo detection.**  (1)
  Right-click "Reset Stretch" on audio clips (restores the library natural identity, keeps
  position/length, undoable; enabled only when re-fit; follow-flag re-derived like the Properties
  dialog); the stretch-drag now also sets isOverride so the follow dot stays truthful.  (2)
  Stretch badge on audio blocks: "xN.NN" (factor vs natural, 0.01 precision, hidden at x1.00,
  top-right stacking under the pitch label).  (3) Teaching-app cut (Jeff): the PER-CLIP Properties
  BPM box is now a READ-ONLY detected-tempo display ("Detected tempo: ~139.8 BPM" /
  "(not detectable)", path-cached); the BROWSER entry keeps the single editable correction field.
  (4) NEW DSP `Source/DSP/BpmDetect.h` (header-only, message-thread): onset-energy-flux +
  comb-scored autocorrelation (octave guard) + parabolic sub-BPM refinement, 60 s bounded chunked
  analysis, confidence = comb peak > 2x field mean (CALIBRATION STARTING POINT - tune at
  campaign).  (5) Import (Jeff A/A): a CONFIDENT estimate becomes the clip's natural identity
  (grid-locks a 140 loop dropped at 90 immediately); unconfident keeps the G fallback
  (target-bar tempo, plays 1:1).  Beat detection was a Future State wishlist item - route the
  cross-ref at section pass.
  **Smoke round 3 findings (all REAL BUGS, all fixed at boundary):** (1) undo/redo never rebuilt
  the audio-clip players - `applySnapshot` (the undo/redo restore path) lacked the
  `onArrangementChanged` fire that `commitEdit` has, so a stretch undo reverted the block
  visually but kept playing stretched.  PRE-EXISTING for every audio-clip undo, exposed by the
  stretch feature.  Fixed in applySnapshot (double-fires on fresh commits; idempotent).  (2)
  stretch badge invisible at top-right despite provably-true data (the Reset menu's enable check
  uses the same lookup and worked) - top row is owned by the full-width filename label; moved to
  a bottom-right dark pill, 9pt bold.  (3) Reset Stretch restored speed only; Jeff's spec: reset
  = ORIGINAL DROP FORM (natural speed + full natural length + slip offset cleared, position
  kept) - now re-derives length from the file duration at natural tempo.  (4) BASE tempo edit
  mid-play went through truncate-and-append, leaving the old tempo as history segments = a
  phantom "recorded" tempo change on replay.  Base edits are RULE changes: new
  `rebuildTimeline(-, rebaseWhilePlaying)` mode does a pure origin rebuild mid-play + beat-stable
  sample relocation + mSeekDiscontinuity raise (setBPM only; automation/markers keep
  truncate-append).  Value-change guard added to setBPM.  (5) 1-2 s of static after a mid-play
  tempo edit - expected to be the players chasing the map mismatch block-by-block; the (4)
  rebase + discontinuity flag makes it one clean resync; RETEST, if static persists it gets its
  own investigation.  Test plan: EC-14 revised, TM-15 added.
  **Smoke round 4 (one root cause, three symptoms; REAL BUG from round 2's own fix + a latent
  overrun):** Jeff reported (a) crackle at NORMAL length on a streamed MP3 song, (b) a big
  streamed WAV song LOCKING UP the app (position counter crawling ~1% realtime), (c) small
  RAM-loaded WAV clean.  Root cause: the round-2 `pvOutFrac` carry treated a vocoder output
  SHORTFALL (peeked < consume - happens exactly on streaming-file refill hiccups; RAM never
  misses) as repayable whole-sample DEBT - it compounds per block into an unbounded demand.
  Second defect underneath: `PhaseVocoder::peekOutput` copied into the caller's scratch without
  clamping to its capacity, so once the demand exceeded pvOutBuf the copy wrote past the heap
  allocation every block = memory corruption = the lockup.  Fixes: (1) shortfall debt DROPPED
  after advance, only sub-sample phase carries (both PV sites); (2) request clamped to
  pvOutBuf capacity at both sites; (3) peekOutput clamps to destination room (API-level - a
  runaway request degrades to a short read, never corruption).  ALSO surfaced from the same
  report: detection-stamped identities mean FULL SONGS grid-lock too (his 2:00 MP3 spans
  2:10.378 of timeline = detected ~130.4 in a 120 project; 7 s WAV spans 9.332 s = exactly 4/3;
  2:25 WAV spans 1:30.9 = 0.627, a likely half-tempo octave error) - lengths were the A/A
  design (musical length), spec call posed to Jeff.  **RESOLVED (Jeff, same round): detection is
  DISPLAY-ONLY, no import stamping at all - EVERY clip regardless of length lands at its actual
  wall-clock time at the drop-time tempo and plays 1:1 (portability: tracks recorded in one
  project must not re-stretch when dropped into another).  Supersedes the A/A pick; import
  detection call removed; the Properties detected-tempo readout + cache stay.**  MP3
  length-estimate accuracy noted as a watch item (moot for behavior now; display-only).
  **Smoke round 5 (universal small fizz; diagnosed via Jeff-run discriminator battery, then
  fixed):** small content-correlated crackle on EVERY clip (RAM + streamed, all formats), same
  sample rate, in-app only.  Battery results: synth-only playback CLEAN (rules out xruns /
  master path / driver), buffer-size raise only marginal (and Jeff runs 128 for live tracking -
  FL is clean there), character = random gritty fizz that scales with program density (the
  linear-interpolation error fingerprint), and the round-4 wet/dry vox asymmetry retired as
  evidence (that dry file predates the dry recorder - May-era wet-only rig).  ROOT CAUSE: the
  QA-Ec beat-domain positions are fractional even for unstretched clips, so playback lost the
  bit-exact fast path and EVERYTHING ran through 2-point linear interpolation (error tracks
  signal slope = program-dependent fizz).  FIXES: (1) readAndMix snaps to the nearest frame when
  readRatio == 1.0 exactly - constant +-0.5-frame timing shift, advance stays 1/sample so blocks
  stay continuous, exact-copy fast path restored (1:1 playback bit-exact again); (2) the
  fractional-rate path upgraded linear -> 4-point Catmull-Rom (readAndMix + BOTH PV output
  interpolators) - window lower bound extended 1 frame + ring read-head retention margin -2 for
  the kernel's behind-read.  Verify at 128-buffer live-rig settings (Jeff's baseline).
  **Smoke round 6 (crackle persists post-round-5; Jeff supplied an AI RT-violations explainer to
  review):** scorecard vs evidence - the "global buffer underrun" framing stays REFUTED
  (synth-only playback clean = the callback meets its deadline when clips aren't decoding), and
  the clip path holds the RT rules (pre-allocated buffers, atomics/lock-free ring, no logging) -
  EXCEPT one genuine hit: both PV render branches called `AudioClipStreamer::seek()` from the
  AUDIO THREAD = synchronous disk prefill under the reader lock (sin #3 + priority inversion),
  firing at clip starts / loop wraps / transport+tempo jumps on STREAMED files.  Fixed: new
  lock-free `requestSeek()` (flag-only; the bg thread does the locked prefill; readRaw silent
  until ready); `seek()` re-documented message-thread-only.  Caveat that keeps this from being
  Jeff's fizz: kRamThresholdBytes = 100 MB (~9.5 min stereo) - his songs RAM-load and never hit
  the disk path, so this fix owns the LONG-file / streamed class (and the round-4 lockup family),
  not the ubiquitous fizz.  LEADING remaining hypothesis: his project's clips carry
  detection-stamped BPMs from the pre-ruling build -> EVERY clip secretly runs the phase vocoder
  at ~1.09x even at normal length -> PV transient smear on dense material = content-correlated
  grit.  Decisive test queued: FRESH drop into a blank project post-build (fresh 1:1 = direct
  bit-exact path).  If a fresh drop still fizzes -> next round adds a diagnostic render capture
  (bounce the clip path's exact output to WAV, bit-compare offline vs source).
  **Smoke round 7 (fresh drop STILL fizzes -> stop theorizing, instrument):** clip-decode
  diagnostic tap added (Rule 4 tooling; disposition decide-at-close).  Armed by flag-file
  existence (`Documents/BaySickDAW/enable_clip_tap.txt`) at Play; the audio thread writes the
  FIRST Builder audio clip's decoded output per block (post-decode, PRE-control-chain/declick/
  inserts) to `clip_decode_tap.wav` via the queue-backed AudioFileRecorder (wet-recorder RT
  pattern); Stop finalizes + AlertWindow with the path.  Bisection logic: tap WAV fizzes in an
  external player -> artifact is in the decode data (streamer/interp); tap clean but live
  playback fizzes -> downstream (control chain / inserts / master / driver) and the tap moves
  down the chain next.  Jeff's round-6 answers logged: detected-BPM display reads ~half actual
  (octave error, display-only, campaign calibration); fresh-drop MP3 block length may still
  over-report (VBR length-estimate watch item - WAV-vs-MP3 fresh-drop length comparison
  requested).
  **Smoke round 8 (tap analyzed offline - the fizz is REAL, RARE, and BLOCK-LOCKED):** wrote
  pure-Python analyzers (scratchpad) against clip_decode_tap.wav (27.7 s stereo float).  Strict
  local-contrast detector: 12-15 events/channel (~0.4-0.5/s), period-128 concentration = 1.000
  (= Jeff's ASIO buffer; 12/12 within +-1 of a boundary ~ 1e-20 chance) - every fizz event is a
  ONE-SAMPLE FORWARD SKIP exactly at an audio-block boundary (waveform dumps confirm: boundary
  step ~ 2x local slope then smooth continuation; gain-step hypothesis quantitatively excluded).
  Control test (same metric at mid-block) killed a faster-drift reading (boundary excess ~0).
  Desk math says the beat-domain posD chain CANNOT skip at that rate (rational cancellation ->
  frac ~ 0, llround stable) -> data vs math conflict -> instrument the disputed variable:
  round-8 adds a POSITION-RESIDUAL TRACE alongside the tap (clip_decode_trace.wav, 1 float per
  block = posD advance minus expected; first sample = (fileRate-1)x1e6 rate fingerprint).
  Discriminates: residual +-1 spikes = engine position math skips; flat zeros + audio skips =
  copy layer (readAndMix/ring) or the tap itself.  Also: automation applicator confirmed
  guarded (no periodic mid-play rebuilds exist to blame).
  **Smoke round 9 (trace round 1 analyzed - the position itself wobbles):** trace showed
  fileRate EXACTLY 1.0 (snap engaged) yet posD dips on ISOLATED blocks in paired -r/+r
  residuals: r grows PERFECTLY LINEARLY with elapsed blocks (r = k x 0.0045248, i.e. 1 sample
  per 28800 elapsed samples = 3.47e-5 relative) until CAPPING at exactly +-1.0 sample at block
  ~225, then persists as exact +-1.0 pairs on an irregular ~26-88 Hz message-thread-jitter
  cadence.  Interpretation: TWO position formulas whose slopes differ by 3.47e-5, one used on
  rare blocks - but code reading exhausted the candidates (map reader seqlock verified sound;
  no mid-play publisher exists - transport timer's setLoopBeats/setTimeSignature are plain
  atomic stores; automation applicator inert with no lanes).  Round 9: trace upgraded to 5
  channels (posD residual / bufOffset / outSamples / playhead step error / (fileRate-1)x1e6
  rate wobble) so the guilty INGREDIENT names itself: playhead-clock skip vs sub-span geometry
  vs per-block rate wobble (varispeed knob float round-trip would break the snap on exactly
  the dipped blocks) vs map anomaly.  One more capture cycle, then the fix.
  **Smoke round 10 - THE FIZZ ROOT CAUSE (trace round 2, FOUND + FIXED):** the 5-channel trace
  first exposed a self-own: AudioFileRecorder::writeBlock applies a 5 ms fade-in (221 samples)
  to EVERYTHING - at 1 trace-sample/block that scaled the first 221 blocks of round-1 trace
  data by a 0->1 ramp, manufacturing the whole "grows linearly then caps at 1.0" mystery (the
  growth WAS the fade; the cap was it ending; 225 blocks ~ 221).  Un-scrambled truth: residual
  dips are CONSTANT +-1.0 and ch3 (playhead step error) = +-1 with MATCHING SIGNS -> the
  transport clock ITSELF wobbles one sample block-to-block.  ROOT CAUSE (grep-confirmed x3):
  CompositeAudioInsertTask / InstStripTask / VoxStripTask all derived projectStart via the
  BEAT ROUND-TRIP - integer mSamplePos -> getPpqPosition() double -> `(int64)(beat x secPerBeat
  x sr)` TRUNCATING cast - which lands +-1 sample on FP rounding luck, so every clip rendered a
  stuttering clock (one-sample seams; audible where they hit steep waveform moments; the
  irregular 26-88 Hz "UI-jitter" cadence = FP rounding pattern, no timer involved).  Explains
  why pre-QA-Ec playback was clean: the old accumulator-based position IGNORED the transport
  per block, masking the wobble; the stateless beat-domain math faithfully reproduced it.  FIX:
  all three tasks now use posInfo->getTimeInSamples() (the exact integer clock the playhead
  already publishes) with the beat math as VST-host fallback.  Recorder fade-in flagged as a
  Rule-4 tooling caveat (any future 1-sample-per-block diagnostic must skip or account for it).
  **Smoke item-12 FAIL + fix (typing keyboard vs command layer):** Jeff: some letters still fire
  their commands in typing mode; R and F both arm record; H starts play ("never has before").
  Diagnosis: the D-4 bypass covered the GRID tool keys but not the COMMAND layer - the
  KeyPressMappingSet (a key listener on StandaloneEditor) still dispatched letter bindings, and
  the note-vs-command priority hung on unguaranteed virtual-vs-listener dispatch order.  Real
  default collisions: R = cmdToggleRecord, bare S = cmdToggleSlipStretchMode.  H->play / F->record
  exist in NO current default -> stale overlay from the persisted keymap.xml (defaults are
  applied then user file overlaid; old-generation bindings survive).  FIX: StandaloneEditor now
  also inherits juce::KeyListener (overloads forward to the existing handlers) and registers
  ITSELF as a key listener BEFORE the mapping set - listener registration order = dispatch
  order, so note keys are consumed ahead of command dispatch whenever the mode is on; mode off =
  instant false, commands untouched.  Design stated to Jeff: mode ON trades ALL bare letters for
  notes (R/S included); Ctrl+T restores.  keymap.xml stale-binding cleanup = his Help > Key
  Binds reset (or delete the file).  **Round 2 (Jeff: "still happening"):** the first fix
  registered the gate BEFORE the set on the assumption listener dispatch = registration order -
  WRONG: ComponentPeer::handleKeyPress iterates key listeners in REVERSE registration order
  (`for (i = size(); --i >= 0;)`, verified in vendored juce_ComponentPeer.cpp:206) and runs them
  BEFORE the component's own keyPressed.  So the set (registered later) had ALWAYS outranked
  both the gate and the D-4 virtual handler - explains the original bug AND the failed first
  fix.  Corrected: gate registers AFTER the set (last = first).  Gotcha added to CLAUDE.md.
  **Item-12 round 3 (the F/H ghost binds - SOLVED by reading keymap.xml directly):** F->record
  and H->play were NOT code bindings, JUCE defaults, or my typing-keyboard work - Jeff's
  keymap.xml carried FIVE stray user-overlay mappings (2+H->cmdPlayPause, F+G->cmdToggleRecord,
  D->cmdStopAndDisarm), silently re-applied every launch.  TWO KeyBindsWindow bugs made them
  possible + invisible: (1) the Set capture flow called addKeyPress WITHOUT clearing the
  command's existing keys -> every rebind attempt ACCUMULATED a binding (all 3 apply sites);
  (2) the row displayed only keys.getFirst() -> extras live but invisible (also why Jeff's
  Key Binds check showed nothing).  FIXES: clearAllKeyPresses(cmd) before addKeyPress at all
  3 apply sites (Set = replace); row now joins ALL bindings ", "-separated; Jeff's keymap.xml
  removed (backup .bak) -> pure catalog defaults.  Note-key strays (2/D/G/H) had already been
  silenced by the typing-mode gate - F survived only because it is not a note key.
  **Post-boundary fix (maximize-restore, round 2 - the round-1 "fix" was a no-op):** Jeff proved
  via before/after screenshots (title-bar glyphs + an ~8 px content shift) that the window IS
  truly maximized at close and the RELAUNCH demotes it.  Root cause: the WindowState restore ran
  setFullScreen(true) BEFORE setResizeLimits; setResizeLimits calls setBoundsConstrained, and
  the default constrainer's keep-on-screen clamp nudges a maximized window (legitimately a few
  px above the screen edge) down to y=+7 - a real SetWindowPos, which Windows answers by
  demoting MAXIMIZED -> NORMAL placement at the same size.  Every relaunch opened
  windowed-almost-full; the next close then honestly saved maximized=0 (the round-1 peer-OR
  save fix read the same GetWindowPlacement both ways = dead on arrival; ResizableWindow::
  isFullScreen always defers to the peer for on-desktop windows).  The first-launch path always
  ordered limits-then-maximize, which is why QA-Eb testing never saw it.  FIX: restore block
  moved AFTER setResizeLimits (StandaloneApp initialise); saved y=7 + the screenshot shift were
  the pinning evidence.  Uncommitted - rides with the next commit (Jeff to retest first). (1) StandaloneApp.cpp initialise ~post-restart block - "Device-type scan
  lists" dump (every type's device names) - answers "was the requested ASIO driver enumerable";
  (2) StandaloneApp.cpp C.3 MIDI block - "MIDI at startup" append (available devices, saved-id
  match, enableAll, per-device enabled state) - answers "what did MIDI enumeration return". No
  tag prefix (plain-text log sections named as quoted).
  (3) **Clip-decode tap** (rounds 7-9) - PluginProcessor start/stopClipDecodeTap + the Path A
  render-site write + StandaloneEditor Play/Pause/Stop arm/finalize wiring, flag-file armed.
  Wrote `clip_decode_tap.wav` (first audio clip's decoded output, pre-control-chain) - answered
  "is an audio artifact in the decode data or downstream".  **Disposition: REMOVED at boundary
  close (Jeff) - stripped from source before the boundary commit; rebuild cost if ever needed
  again ~1 hour, and this catalog entry is the spec.**
  (4) **Position-forensics trace** (rounds 8-9) - same switch, wrote `clip_decode_trace.wav`
  (5 ch x 1 float/block: posD residual / bufOffset / outSamples / playhead step error / rate
  wobble) - answered "which position ingredient moved" (named the transport clock).
  **Disposition: REMOVED with (3), same commit.**
  (5) **Tooling caveat, permanent:** AudioFileRecorder::writeBlock applies a 5 ms fade-in
  (mFadeSamples) to every capture - any future 1-sample-per-block diagnostic through it must
  skip/deconvolve the first ~221 writes or the ramp manufactures phantom growth (cost one
  full analysis round in the fizz hunt).
- **Assumptions changed:** none beyond what the running notes + corrected marathon rows already
  record (bar-rounding import correction G; QA-Eb Viewport premise; BUILD-06 staleness;
  shared_ptr->seqlock publish; markers song-domain gate).
- **Resume action:** collect Jeff's G1 smoke + ear-check results -> fix/fold findings -> one
  boundary commit (message + full status -> approval) -> give the honest model assessment ->
  **Jeff's GO/NO-GO on the bulk-run model.**  **RESOLVED 2026-07-09: GO** — G1 boundary closed at
  commit 4920efa0; model continues through G6 at the ORIGINAL cadence (my proposed
  render-path-batch ear-checks REJECTED by Jeff — no added checks; only the two already-planned
  G2 mid-group ear-checks stand).  G2 starts when Jeff opens it.  On GO: G2 group start (plan
  files for QA-F / QA-Fa /
  QA-Fb' / QA-Fc; no marathon leftovers block G2 — item 3 locked; remember the two MID-group
  ear-checks after QA-F and QA-Fa).

## Carry-Over — 2026-07-10 (G2 boundary session)

- **Completed:** session-open per protocol (standup + Main Plan §0 + boundary steps + ferret
  carry-over, all read in full). Test plan §B.9 `blocks:` backfilled `a36ed3cc` (the standing
  first-touch convention; uncommitted, rides the boundary commit). R3 group review over the
  COMBINED G2 diff `e5c62218..71bd93bc` (10 commits, 44 files) ran CLEAN: **0 BLOCKER /
  0 NEEDS-FIX / 4 NITs**, every finding premise-verified against source before surfacing:
  (1) en-GB "colouration" x2 — the ONLY en-GB user-facing strings in the whole G2 diff
  (grep-swept; `BaySickNAMIREditor.cpp:82/:257`); (2) raw-`this` `callAsync` in the NAMIR
  editor param listeners — 4 new QA-Fc sites (:846/:855/:871/:886) extending the 2 pre-existing
  H-6d/QA-A sites (:829/:839); destructor removes the listeners but an already-posted lambda
  survives destruction; the same diff's Align/Pitch editors use `SafePointer` throughout;
  (3) pitch applicator lacks the align side's device-SR-change rederive
  (`BaySickPitchDSP.cpp:269` divides analysis-rate `snap.startSample` by the current device
  rate; align re-derives from the beat on rate mismatch at `PluginProcessor.cpp:1167-1171`;
  `Snapshot::sampleRate` carried but unread on this path; rare trigger, self-heals on any
  re-analysis); (4) stop-gated auto re-analyze = synchronous message-thread analysis fired by
  VoxPage's 4 Hz timer (locked bundle item 4, NOT a violation — flagged as a
  deliberate-judgment watch item inside the ear-check walkthrough: long channel = multi-second
  UI hitch when the auto fires). The review's five special-attention areas (seam-fix case walk
  incl. wrap-on-boundary + stale-wrap consumption; Fb' record-path order corrector→WET
  tap→merge→rack; Fc byte-identical-off + rising-edge-on-`micBRun` reset; PitchShifters
  RT-safety + ring-wrap bounds; cross-batch mux/exclusion sites) all HELD.
- **Docket answers (Jeff, 2026-07-10):** 1=A, 2=A, 3=A — all three FIXED in-tree same session
  (coloration x2 `BaySickNAMIREditor.cpp:82/:257`; SafePointer on all 6 posted lambdas in
  `BaySickNAMIREditor::parameterChanged` incl. the 2 pre-existing H-6d/QA-A sites; pitch
  applicator origin converted through the ANALYSIS rate — seconds-domain `snapStartSec` in
  `BaySickPitchDSP::applyBlockCommon`, one site covers live + monitor streams; offline path
  already composite-relative, untouched). Build pending — rides the align-fix build cycle.
  **Docket 4 CORRECTED:** my "accepted design" framing was WRONG — the tick was flagged FOR
  Jeff's judgment at QA-F code-complete, never signed off by him; presenting it as
  pre-accepted was my error (the self-authored-notes-as-owner-signoff trap). Jeff's answer:
  NOT accepted, needs a real fix, follow-up LATER (slot TBD when we return to it; not
  boundary work).
- **Smoke finding #1 (REAL BUG, Part 3 item 3 — smoke halted there; diagnosis code-traced,
  fix held on a spec call):** mid-play Align ON/OFF toggle makes the follower voice STOP
  (silence/smear) instead of the promised FA-12 glide, then playback reads persistently in
  the WRONG PLACE until a block nudge (rebuild) heals it; Jeff also heard pre-clip count-in
  content (take 2 slip-recorded a count-in before the clip start). TWO stacked defects in
  the QA-Fa recovery glide (`PluginProcessor.cpp:1197-1250`), both mine: **(A)** the ~50 ms
  glide constant drains `glideK*posCorr` per block — for any alignment offset > ~50 ms
  (0.05*SR*readRatio; real align offsets are 100s of ms) the intended per-block law motion
  goes NEGATIVE, the degenerate clamp pins consumption at `outSamples*RR/64` (2 samples at
  Jeff's 128 buffer) → rrEq pegs at 64x → the PV starves/smears ("voice stops") for the
  drain duration (~0.8 s per 200 ms of offset; the ON direction symmetrically fast-forwards
  at unbounded rate). The glide was validated on sub-50 ms law changes only — mis-scoped
  constant. **(B)** during clamped blocks the INTENDED bookkeeping (`alignLastLawEnd =
  lawEnd + posCorr`, :1460) and the ACTUAL feed (`expectedFilePos += clamped tc`) diverge;
  the only reconciler is the 2-SECOND seek net (:1348-1351), so sub-2-s error persists for
  as long as the chain stays engaged → "reading in the wrong place"; repeated toggles
  compound it; divergent reads land outside the [raw..warp] envelope incl. BEFORE the
  clip's content start = the audible count-in (Jeff's slip observation = the fingerprint,
  not the cause). Numbers reproduce all three symptoms at his 128/44.1k rig. Heal-by-nudge
  = player rebuild resets the per-clip glide state (+ stale → auto re-analyze). **Defect (C),
  found during the fix implementation and DOMINANT:** the law-change detector compared
  `lawStart` (law-only) against `alignLastLawEnd` (law + outstanding correction), so it
  misfired on EVERY mid-glide block and the `+=` re-added the full outstanding correction
  each time — compounding ~1.9x/block at the 128 buffer for ANY law change > 4 samples;
  the 2 s seek net kept resetting the FEED but never the correction, so it re-broke every
  block (the true persistence mechanism; A and B are real but secondary). **Design (Jeff,
  approved 2026-07-10 after a VocAlign reference pull — workflow + Max Difference 0-200 ms /
  Max Shift 10-150 ms / 2.0x-0.5x warp restriction, synchroarts.com manuals):** the ON/OFF
  toggle KEEPS a glide; analyze + revert become STOP-GATED (greyed + stop-first tooltip)
  in BOTH editors — "maps only change while stopped", matching the already-stop-gated auto
  re-analyze. **Fix SHIPPED (built pending):** (i) detector strips the correction before
  comparing law ends; rebase is ASSIGN not += (kills C); (ii) drain capped so the audible
  bend never exceeds 2:1 either direction — the reference aligner's own published warp
  restriction; glide time ~= offset traveled (~0.2-0.4 s for a 200 ms-late take); taper
  = the existing ~50 ms exponential inside the cap (kills A); (iii) exact-bookkeeping
  invariant `posCorr = (pStart + tc) − lawEnd` after the degenerate guard (kills B; the
  guard self-accounts); (iv) stop-gates: Align editor Analyze/Apply + Versions, Pitch
  editor Versions + analyze-on-open-stale defers to the poller at stop; guards in
  runAnalyze / runAnalyzeIfNeeded / both async menu-result handlers (menu race);
  `setPlaybackGate` on both toolbar structs driven from the existing editor timers
  (PluginProcessor.cpp glide block; BaySickAlignEditor.cpp; BaySickPitchEditor.cpp).
  gotRaw-miss + peeked-short paths examined and LEFT as-is (pre-existing pristine-path
  conventions, rare streaming corners, self-heal via the seek net). Test plan amended:
  F-6 stop-gate wording, FA-9 analyze-on-open deferral, FA-12 glide recalibration +
  no-drift-after-toggle-spam expectation (failures indict the boundary commit), FA-13
  stopped-revert flow.
- **Assumptions changed:** the QA-Fa recovery's "no-click machinery" bundle-5 claim
  ("law changes glide over ~50 ms — never a splice") is FALSE as built for real-size
  alignment offsets; held finch/ferret Work Log entries stay as-written (append-only —
  the fix gets its own boundary entry at section pass).
- **Part 6 verdict + realtime-board lock (Jeff, 2026-07-10):** the first-engage tick clicks
  on EVERY leg of the Part 6 listen — "annoying and unusable" — BUT the controls are
  set-before-take by nature; Jeff's design: lock the WHOLE realtime board during live
  recording (same pattern as the align/pitch stop-gates) so the artifact can't reach a
  take. SHIPPED same session: `VibeSynthProcessor::isStripRecording(channelId)`
  (message-thread accessor over mStripRecorders) → new `onIsStripRecording` hook on
  BaySickVocalProcessor (wired per-strip in VoxPage::setProcessor via voxInsert(pageIndex))
  → the vocal editor board's 10 Hz timer greys/disables the full realtime section
  (toggle, Key/Scale, Retune/Strength/Humanize/Throat, Formant Preserve; alpha 0.4;
  lock tooltip on the toggle) while THIS strip captures — count-in included,
  armed+listen-off included, monitoring-only stays editable (the setup flow). §B.8 FB-11
  authored (indicts the boundary commit). RESOLVED (Jeff, same session): (a) docket 4
  stays OPEN — the lock does NOT close it; the engage tick itself must be fixed so
  tuning-while-monitoring is clean (fix needs its own design round: latency trade-off
  options — always-resident engines vs latency-matched bypass vs edge crossfade; SLOT
  pending Jeff's pick, posed alongside the build: pull into this boundary vs slot at G3
  open); (b) SAME TREATMENT for the page-wide chain Bypass + A/B slot — SHIPPED same
  session (added to the gate set + lock tooltips; Mix stays live, smooth param; FB-11
  amended). NOTE for QA-ApvtsAutomation (G4): ab_slot's tooltip advertises timeline
  automation; once the confirmed application gap is fixed there, an automated ab_slot
  swap could fire mid-capture and bypass this UI lock — that batch should decide whether
  automation writes to gated params get suppressed/deferred while the strip records
  (Rule 3 routing at section pass).
- **Smoke finding #2 (QA-F ear-check FAIL at retest Part 3 item 5, FIXED same session):**
  Jeff's mode listen: "loose sounds like choppy slop, close chops every once in a while,
  tight relatively good" — mode REACH works, warp QUALITY fails, severity scaling with
  correction size. TWO build defects (code-traced): **(a)** `pairOnsets` was greedy
  nearest-within-tolerance with NO order constraint — a later dub onset could grab an
  EARLIER guide onset (crossing); the publisher's monotone clamp then flattened each
  crossing into plateau+cliff segments = alternating extreme slopes (Loose's 150 ms window
  crosses most; Tight's 50 ms barely — matches the verdict exactly); **(b)** the map is
  piecewise-LINEAR (WarpMap docs + `lookupAtGuideSec`) so playback rate is a stair-step —
  constant per segment, instant jump at EVERY anchor (onset cadence), bounded only by the
  absurd 1/64..64 clamp (reference restricts to 0.5..2.0). FIXES (live path): (1) monotone
  pairing — paired guide onsets must strictly ascend (kills crossings at the source);
  (2) segment slope bound 0.5..2.0 at map build (reference's own warp restriction) —
  out-of-bound AUTO anchors dropped iteratively; sync points + endpoints exempt (user
  intent; lookup clamp bounds a hard-vs-hard breach); (3) monotone-cubic map lookup —
  `AlignPlaySnapshot::tangent` (Fritsch-Butland harmonic-mean tangents, computed at
  publish after a near-equal-guide dedup) + cubic Hermite position AND slope in
  `lookupAtGuideSec` (C1: the decode rate glides through anchors; linear fallback kept
  for tangent-less snapshots). Mode windows (150/100/50 ms ±50 fine) NOT touched — sane
  vs the reference's own Max Shift 10-150 ms. **PARITY FLAG:** the offline Render
  (`applyWarp`, segment-per-anchor PV) still steps at anchors — upgrade follows in-batch
  once the live sound passes Jeff's ear (its calibration informs the offline shape);
  F-3 amended with the fix note + re-Analyze requirement + the parity caveat.
  (BaySickAlignDSP.h/.cpp, BaySickVocalProcessor.cpp publisher.)
- **Smoke finding #3 (retest round 2, 2026-07-10/11):** clean-source takes CLEARED both
  residuals (Loose "off" + pitch clicks = raspy-source material, not engine — chop fix
  verdict STANDS, no chop in any mode). NEW: Tight Analyze fails ("Not enough matching
  transients") after previously succeeding repeatedly on the SAME setup; Loose + Close
  still pass; only align settings were touched between runs; the lanes visually line up
  (confusing — a failed analyze silently KEEPS the previous map applied, so the Output
  lane still shows the old success). Verified honest: fineTune bipolar -50..+50 def 0,
  mode combo order matches bases (150/100/50 ms), presets seat fine=0 — no settings-
  mapping defect. Determinism reasoning: same inputs cannot succeed-then-fail, so the
  suspect is the CHANGED CODE — the smoke-#2 fix's greedy-first monotone matcher has a
  cascade failure (one early grab — e.g. a breath onset taking a real word's partner —
  blocks all later pairs from reaching back; crossing-pairs previously masked it), which
  can starve Tight's +/-50 ms window below the 2-pair minimum on material where pre-fix
  Tight passed (his earlier successes likely pre-fix build; attribution question posed).
  FIXES (same session): (1) `pairOnsets` upgraded to OPTIMAL monotone matching — small DP,
  maximize pair count, tiebreak min total distance, monotone/no-crossings structural;
  greedy fallback retained above a ~4M-cell DP cap (pathological composite lengths);
  (2) approved error-message fix shipped WITH diagnostics: `AlignAnalyzeDiag` out-struct
  from analyzeOffline (guide/dub onset counts, pairs, window) → analyzeAlign's failure
  dialog now distinguishes too-few-word-starts from no-pairs-within-window, reports the
  counts + window ms, and states the previous alignment stays applied (the Jeff-confusion
  fix). F-3 note amended (Tight must succeed wherever pre-chop-fix Tight did). Rule 4:
  the counts ride the user-facing dialog (permanent product improvement, not temp
  instrumentation — catalog stays empty).
- **Align semantics rework — LOCKED (Jeff, 2026-07-11, dockets 9-13):** root insight (Jeff):
  our Mode/Fine Tune GATES which words may match then snaps 100%; the reference's timing
  knob (Max Difference, manual-verified: "the size of the 'window' of allowable variation
  in timing... fully-locked at the Tight end") controls RESIDUAL tightness of the result,
  with matching internal. Root cause of the Fine-Tune footgun (finding #3's ACTUAL cause —
  fine at knob-bottom = effective ~5 ms window = no pairs; the display shows base+fine so
  the screenshot's "0 ms" was the bottomed knob, which I misread as raw-centered; the
  greedy-cascade theory was a wrong detour, DP matcher retained as strictly-better anyway).
  PICKS: **9a** adopt reference semantics — matching window goes INTERNAL (wide, my
  calibration ~400 ms), Mode/Fine Tune becomes residual tightness; **10a** residual CAP
  model (within the window = untouched natural timing; beyond = pulled to the window edge);
  starting calibration Tight=0 / Close~50 / Loose~100 ms, fine +/-50 clamped >= 0 — tuned at
  the re-listen. Docket 8 (footgun floor) SUPERSEDED — 0 now means fully-locked, valid.
  **12a** pitch side gets the full reference set with OUR names (no reference naming
  conventions): "Pitch Variation" = allowable tuning-variation cap (their Max Difference
  (Pitch) role; within = untouched, beyond = pulled to cap edge), "Pitch Blend" = percent
  toward leader contour 0-100 free travel (their Pitch Target role; our existing knob's
  semantics, renamed, mode-clamp travel windows DROPPED — modes preset values instead),
  "Pitch Types" = per-side detection-band pickers (their Pitch Ranges role); **13a** bands:
  Normal / High Vocal / Low Vocal / High Instrument / Low Instrument (Hz ranges = my
  calibration); pitch percent/cap moves from baked-at-analysis to applied-at-publish (live
  knobs, no re-analysis). F-3/F-4 + tooltips rewrite with the rework. Alignment-section gap
  survey (manual-verified) POSED as dockets 14-18: Flexibility rule choice / Maximum Shift
  guard / High-Res 384k render / SmartAlign toggle / vibrato mode — awaiting per-line
  in-batch vs Future State vs skip.
- **Gap dockets 14-18 ANSWERED (Jeff, 2026-07-11):** **14a** — Flexibility rule exposed as a
  user choice, but with JEFF'S simplified rungs: **Low / Normal / High / Max All** (maps to
  stretch-bend ratio bounds, calibration mine ~1.5:1 / 2:1 / 4:1 / unbounded-both; the
  reference's separate Max-Compression/Max-Expansion one-direction rungs collapse into Max
  All; the "+ Pitch" legato variant implicitly dropped — Future State if ever wanted).
  **15a** — Maximum-Shift-style per-word movement cap knob, in. **16a** — High-Res
  (384 kHz-class) oversampled processing folds into the RENDER-parity follow-up (Jeff's
  clarifying question answered: per-effect internal oversampling around one nonlinear stage
  (e.g. NAM/IR's Oversampling selector) is a bounded 2-4x cost on a small block of math and
  lives happily on live paths; the reference's High-Res runs the ENTIRE edit engine at
  384 kHz — ~8.7x data rate through the heaviest DSP — which even the reference treats as a
  processing-time tradeoff in a render-based product; ours lands on Render, same place they
  pay it). **17b** — SmartAlign-style disable toggle → Future State (we have no meaningful
  dumb-mode fallback; a toggle would deliberately re-enable the inferior greedy walk).
  **18b** — vibrato-focused tuning/details mode → Future State.
- **EVERYTHING TABLED (Jeff directive, 2026-07-11):** the entire vocal-editor rework — the
  locked 9a/10a timing-residual model, 12a/13a Pitch Variation + Pitch Blend + Pitch Types,
  14a Flexibility rungs, 15a max-shift cap, 16a render high-res — is TABLED, not started.
  Trigger: walkthrough Part 4 (BaySickPitch) surfaced EIGHT problems; Jeff's call: figure
  out ALL of them + run a FULL Newtone (FL Studio) reference review + a missing-features
  survey, THEN implement everything as ONE body of work. Boundary walkthrough state:
  Part 3 (align) closed good on the current tree's fixes (chop gone all modes, glide/
  toggle clean, fine-tune mystery resolved as knob semantics — now superseded by the 9a
  rework anyway); Part 4 halted on the problems below; Part 5 + FB-11 pending.
- **BaySickPitch problem list (Jeff, Part 4, 2026-07-11 — verbatim-faithful; diagnosis
  belongs to the review phase, NOT yet attempted):**
  1. Pills don't match what Align shows for the SAME channel — a number of pills that
     should be there are missing.
  2. Moving pills up/down does nothing (no effect); pills cannot be moved left-right AT
     ALL — "defeats the purpose and is not how Newtone works which is the setup it's
     supposed to be replicating" (horizontal note movement = reference-parity gap).
  3. Focus/Mod/Speed knobs do nothing; the tightness dropdown does nothing (and reads as
     conceptually out of place in an editor — likely the Loose/Close/Tight PRESET combo,
     naming collision with Align's modes); Slice mode does not slice pills. (Items 2+3
     cluster — smells like the realtime applicator not engaging at all on his rig, but
     that is a lead for the review, not a finding.)
  4. Pills show "some sort of line like a waveform" — unclear if it is one (FA-1 spec says
     teal waveform inside pills; visibility/rendering in question).
  5. Alt+scroll does not zoom vertically — can't inspect the pill contents.
  6. Piano roll scrolls past C0 (range clamp missing).
  7. Playhead follows the main playhead but on STOP it stays where it stopped — does not
     reset with the main transport.
  8. With the view zoomed + scrolled away from the playhead: the moment the playhead goes
     off-screen the view RESETS to the beginning as though never scrolled (auto-scroll
     logic misbehaving with the A toggle default-on).
- **Next phase (the review, before ANY implementation):** (1) full Newtone reference
  review — FL Studio manual pull + Jeff's own FL walkthrough (he is the canonical FL
  source per standing memory; exact-reference-replica rule: fetch + diff the real
  reference FIRST); (2) missing-features survey (what Newtone has that we could use);
  (3) diagnose the 8 problems (split: plain bugs vs parity gaps); (4) consolidated spec +
  remaining dockets to Jeff; (5) implement the WHOLE body (tabled rework + parity + fixes)
  in one pass; (6) full re-listen + walkthrough completion (Parts 4-5, FB-11).
- **Boundary-fix commit LANDED:** `9b024062` (14 files, +732/-79 — all Part-3-verified
  fixes + doc records; Jeff confirmed the last build included the DP matcher + counts
  dialog, so tree == verified build; interim-commit question resolved: commit-then-review,
  Jeff 2026-07-11).
- **Review phase inputs gathered (2026-07-11):** (1) NEWTONE REFERENCE INVENTORY compiled
  from the official Image-Line manual (agent transcript holds the full quoted inventory) —
  headline facts: time model = ELASTIC WARP w/ Ctrl+drag detach ("if possible"), not free
  move; per-note Advanced Edit = 9 handles (Pitch/Position/Formant + Pitch variation +
  pitch & volume ramp-in/out pairs; green=in red=out, volume band top / variation band
  bottom — Jeff supplied the manual's annotated figure); global knobs = Center/Variation/
  Transition (map ~1:1 onto our Focus/Mod/Speed; our Loose/Close/Tight preset combo has
  NO reference counterpart); nav = Alt+wheel vertical zoom, Ctrl+wheel horizontal zoom,
  Shift+wheel h-scroll, middle-drag pan, Ctrl+right-click zoom-to-selection; A =
  auto-scroll toggle; scrub-audition = drag note middle while stopped; Cut mode (C) click
  = slice, Alt+click x2 = join; snap-to-scale; batch re-pitch via ruler click; vibrato
  edit mode (freq line + bi-directional start/end intensity); exports (drag-to-playlist,
  region-marker WAV, MIDI); playhead/view-on-stop behavior NOT documented (Jeff to check
  hands-on in FL). (2) JEFF'S WORKFLOW INPUT: moves notes up/down/left/right + stretch/
  squeeze; multi-point volume like ours; OUR volume line lacking a revert-to-default is
  "pretty rough" — logged as a first-class UX defect; asked for FL-materials gleaning for
  friendliness/intuitiveness → follow-up research pass dispatched (FL-wide control-reset
  conventions + Newtone gesture/reset details). (3) CODE MAP of the 8 symptoms (agent
  transcript holds file:line detail) — triage: #6 BUG (setTopNote clamps top min 24 only,
  no bottom clamp → lanes below C0 render); #8 BUG (auto-scroll recenters on every tick
  the playhead is off-screen while the stamp advances → fights manual scroll; early
  playback recenters to ~0 = "resets to beginning"; A defaults ON); #7 DESIGN GAP (editor
  playhead = FilePlay stamp which freezes when finalize stops being called; editor only
  hides it after 300 ms; main transport explicitly seeks to 0/selection on stop — no
  follow); #1 SEGMENTATION AGGRESSION (60 ms min-note discard + 2-frame gap end + voicing
  gate + 0.6-semi split; unvoiced/short/split-residual material silently dropped) +
  expected inter-clip silence gaps; #2 SPLIT — horizontal move UNIMPLEMENTED (drag kinds =
  pitch/left-trim/right-trim only) = parity gap, while vertical-drag INAUDIBILITY is an
  OPEN MYSTERY (chain verified wired end-to-end: drag writes shiftSemis -> publishEdits ->
  snapshot -> FilePlay applicator, and FilePlay WAS active in his session since the
  playhead stamp advanced — verification pass owns this; candidates: symptom-1 pill/audio
  time mismatch, stamp domain mismatch, my SR-origin fix's tSec mapping); #3 same gate as
  #2 for knobs (wired: apvts -> pushApvtsToDsp -> atomics -> applyEditsToBuffer; Focus
  no-op until >0.001, Mod until |mod-1|>=0.01) + preset combo just writes the 3 knob
  params + Slice needs pill-band hit + 30 ms-interior click (wired but strict) — plus the
  combo itself is a no-reference-counterpart docket; #5 parity gap (wheel handler has
  Ctrl/Shift/else branches only — NO Alt branch, laneH is a fixed constant, no vertical
  zoom exists); #4 BY DESIGN (pill interior = actual composite audio waveform (teal),
  green F0 curve behind pills — visibility hampered by #5's missing zoom).
- **UX-conventions follow-up gathered (2026-07-11, official FL manual quotes in agent
  transcript):** FL-wide control conventions — RESET TRIAD = Alt+Left-click / middle-click
  / right-click -> "reset" menu item (double-click is NOT an FL convention anywhere, and
  in Newtone double-click = enter Advanced Edit, right-click-on-note = snap-to-semitone —
  both taken); fine-adjust = Ctrl+drag (or both buttons); controls DETENT at default +
  useful values, Shift bypasses detents; type-in value via right-click menu or
  click-hold-and-type; hint bar shows hover hints + values on change. Newtone specifics:
  the 9-handle figure's only prose = "click on the controls as indicated and drag in the
  direction of the arrows" (handles draw their drag-axis arrows; no cursor/hit-zone/color
  prose); NO note/canvas context menu exists in Newtone; NO reset/default/restore wording
  anywhere on its page. Current FL SPLIT time-warping out of Newtone into the separate
  NewTime plugin (older Newtone had a Warp Mode — secondary-source only); NewTime's
  warp-marker gestures (Shift+click or double-click add/remove, drag on the
  double-arrow cursor, Ctrl+drag multi-select + Del) = family-convention data for our
  move/stretch handle design. SPEC IMPLICATIONS captured: our curve/handle reset follows
  the FL triad (Alt+click primary); detents-at-default for our knobs; hint-bar-style
  value feedback candidate. GROUND-TRUTH QUESTIONS posed to Jeff: (a) during the failed
  pill drags, did pills visibly move + green dot + InfoBar update with only SOUND
  unchanged, or refuse to move at all (splits #2a between hit-test failure — 10 px lanes,
  no vertical zoom — and audio-chain failure); (b) the Newtone playhead/view-on-stop
  hands-on check (manual silent).
- **#2a/#3a inaudibility — ground truth + hand trace (2026-07-11):** Jeff confirms pills
  MOVE visually + InfoBar tracks the drag — edit lands in the data model; audio chain
  drops it. Hand-verified at desk (direct source reads, not agent paraphrase): stamp
  domain == analysis-origin domain (both beat->timeline-sample via clipBeatToSample /
  TempoMap; stamp set fresh same-block immediately before eng->processBlock,
  PluginProcessor.cpp:1682-1689); processFilePlay gates pass with his edit state
  (snap non-null, bsp_on default true, anyEdits true); applyEditsToBuffer loop sound
  (region cursor, per-region PSOLA period from r.f0Hz, shiftSemis+focus target,
  smoothing, per-sample shifter). PRIME SUSPECT FOUND: the MASTER-BYPASS early return
  (BaySickVocalProcessor.cpp:457-474, `bsv_bypass`) passes audio raw + merges takes raw
  + returns BEFORE the applicator — reproduces the full symptom set (takes audible,
  playhead moves, pills move, zero audibility from any pitch control). Zero-build check
  posed to Jeff: confirm the BaySickVocals sub-tab Bypass button OFF -> retest a pill
  drag. If audible -> closed + NEW SPEC DOCKET (edits-under-bypass behavior: keep edits
  audible under chain bypass vs bypass-with-indication — Jeff's pick at spec time). If
  still silent with bypass OFF -> instrumented diagnostic build (designed: applicator
  gate counters -> atomics -> editor InfoBar readout, flag-file armed per the Rule 4
  clip-tap convention; disposition Remove at close). ALSO CORRECTED this session: the
  Alt+click reset proposal is DEAD — I relayed an agent's unverified FL-manual pull as
  fact and pre-picked a spec call (verify-premise + spec-calls-are-Jeff's violations,
  owner called it out); the curve/handle reset gesture is an OPEN SPEC LINE for Jeff
  with NO recommendation. #7 spec settled by owner's original words: editor playhead
  follows the main transport INCLUDING its stop-reset behavior (Newtone comparison
  dropped — it is standalone-transport by design, not applicable).
- **#2a/#3a: bypass RULED OUT (Jeff: never touched in any test) → instrumented diagnostic
  SHIPPED (2026-07-11, build pending).** Diagnostic Instrumentation Catalog (Rule 4):
  | Site | Tag | Purpose | Disposition |
  | `BaySickPitchDSP.h` Diag struct + ApplicatorState diag fields; `BaySickPitchDSP.cpp`
  processFilePlay gate counters + applyEditsToBuffer inRegion/lastTSec capture;
  `BaySickPitchEditor.h/.cpp` flag-file-armed InfoBar readout | `[PITCH DIAG]` | name
  which applicator gate eats the signal (blocks/snapNull/bailOff/bailNeutral/applied/
  inRegion/regionCount/lastTSec/maxSemis), read off the pitch editor InfoBar while
  `Documents/BaySickDAW/enable_pitch_diag.txt` exists | Remove at close |
  Found while instrumenting: the pitch editor UI timer runs at 400 ms (2.5 Hz) —
  playhead/auto-scroll updates are inherently chunky at that rate; spec note for the
  rework (#7/#8-adjacent). Newtone have/don't-have matrix delivered to Jeff (16-line
  don't-have pick list + have-with-state table; content mirrors the two research reports
  + the 8-symptom code map).
- **Newtone parity picks LOCKED (Jeff, 2026-07-11): items 1-14 IN, 15-16 OUT** (in:
  horizontal move + stretch/squeeze elastic warp w/ detach; vertical zoom + pan +
  zoom-to-selection; scrub-audition; snap-to-scale; multi-select + select ops +
  copy/paste; batch re-pitch via key click; merge/join; right-click snap-to-semitone;
  per-note pitch + volume ramp handles; per-note pitch-variation handle; 9-handle
  Advanced Edit surface; vibrato-edit mode; in-block pitch display; fine-pitch modifier.
  out: export routes; tempo detect/sync tools).
- **NEW UX finding (Jeff, 2026-07-11): pitch tab feels dependent on Align to populate**
  ("forces me to go to align to analyze just to get the pitch screen to load").
  Code truth: the analyses are INDEPENDENT (each runs its own analysis over the shared
  channel-composite renderer — sharing the renderer is correct); the felt coupling is a
  UX failure with a likely ME-MADE component: the boundary stop-gate defers
  analyze-on-open during playback with NO visible state (empty canvas, no badge on
  first-open), and the pitch analyze path is silent-on-failure by design — so pills
  appear only after a stop + the 4 Hz poller fires, which correlates with his align
  round-trips. SPEC DIRECTION (posed): FIRST analysis of a never-analyzed channel runs
  immediately even mid-play (provably inaudible — no edits exist, the fresh snapshot is
  a no-op; the maps-change-while-stopped rule targets RE-analysis swapping an applied
  law), re-analysis stays stop-gated; plus an explicit visible analyzing/deferred state
  instead of silence. Also the felt dependency dies with it.
- **BaySickPitch problem #9 (Jeff, 2026-07-11, during diag setup):** the InfoBar's
  "[edited]" tag does NOT clear after undoing a pill back to default — updateInfoBarFor
  only fires on hover/selection events, so the label goes stale after Ctrl+Z / drag-back
  (and if drag-back leaves a quantized 0.1-st residue, hasEdits stays true legitimately —
  disambiguate at fix time). Joins the rework's bug list. DIAG first run: no DIAG line —
  suspected flag-file name trap (hidden-extensions .txt.txt) or stale build; flag check
  made tolerant (accepts enable_pitch_diag / .txt / .txt.txt), needs one rebuild.
- **DIAG round 1 (Jeff screenshots, 2026-07-11): upstream FULLY EXONERATED.** During play
  at the edited note: `blk:3693 null:0 off:0 neut:0 app:3693 inReg:128 regs:14 tSec:5.77
  maxSemi:23.82` — every block reaches + applies, zero bails, full block in-region, tSec
  matches the playhead, and the smoothed shift peaked at ~24 st (his drag). The failure
  is BELOW the shift computation. Desk-checks of the remaining links came up clean:
  PsolaShifter algorithm verified sound twice (grid-anchored analysis + re-spaced
  synthesis; identity at ratio 1; correct re-spacing at ratio 4), shifters ARE prepared
  (BaySickPitchDSP::prepare :43-50), engineSum IS the decoded takes (decodeFilePlayClip
  sums into it, finalizeFilePlayStrip comment + :1625-1633) so the silent-buffer theory
  died. DIAG ROUND 2 SHIPPED (build pending): signal-level fields at the applicator —
  `in:` (peak input since arm), `out:` (peak output written), `chg:` (samples where
  wet != dry last block). Decision table: in~0 -> buffer silent after all (routing);
  chg 0 with in > 0 -> shifter returns dry at runtime (period/state pathology);
  chg > 0 + still audibly dry -> the applicator's writes are discarded downstream
  (output overwrite / double-path routing hunt in finalize/mtDest). Flag-file check also
  made extension-trap tolerant (.txt/.txt.txt/bare).
- **#2a/#3a ROOT CAUSE FOUND + FIXED (2026-07-11, build pending): PsolaShifter was a
  delay, not a shifter — since birth (QA-F `9262c746`).** Evidence chain: diag round 2
  `in:0.256 out:0.256 chg:128` (real audio, every sample changed) + Jeff's VOL-drop test
  (note goes silent -> the applicator's output IS the audible path) -> the shift itself
  neutralized inside the shifter. Mechanism: `processSample`'s epoch anchor did
  round-to-k*P-grid THEN min() against the written-window bound (writeAbs - P - 2);
  the scheduler pins mNextSynthAbs at the write head, so the off-grid clamp won the
  min() on virtually every epoch -> analysis step collapsed to the synthesis step
  (centers advanced at pOut, not the grid) -> grain re-spacing (the ENTIRE pitch
  mechanism) nullified -> mathematically clean ~2P DELAY at any ratio: every sample
  differs (chg:128), level preserved, volume path intact, pitch untouched. FIX
  (PitchShifters.h): anchor = floor((writeAbs - P - 2)/P) * P — snap DOWN onto the grid
  from the safe bound; case-walked ratio 1 (identity via consecutive cycles), 4 (4x
  cycle repeat at P/4), 0.5 (alternate cycles); latency worst case ~3P (was ~2P doc'd).
  **BLAST RADIUS — the same PsolaShifter drives:** (1) the pitch-editor applicator
  (Jeff's broken test), (2) the QA-F REALTIME CORRECTOR — Retune/Strength/hard-tune have
  NEVER audibly shifted in any build; every prior "pass" heard a delayed copy (the
  engage ticks were delay-jump clicks — consistent), (3) the offline renders' PSOLA
  algo (pitch render + align +Pitch pass), (4) the monitor-stream applicator. All heal
  with this one fix. GranularShifter + PvShifter are separate implementations (render
  Algo alternatives) — quick audit queued for the rework pass. Retest: pill drag
  audible; realtime hard-tune snap check (first-ever real listen); diag line optional.
  **OWNER-VERIFIED 2026-07-11 ("It works") — pitch edits audible on the fix build.**
  Review phase COMPLETE: all 9 problems triaged/root-caused (2 fixed in-tree: the
  shifter + the stop-gate tolerances; the rest are spec'd rework items), reference
  review + parity picks + tabled rework all locked in this block. NEXT DELIVERABLE:
  the consolidated vocal-editor rework spec (the [PITCH DIAG] instrumentation stays
  in-tree through the rework, stripped at its close per the catalog).
- **Resume action (SUPERSEDED by the 2026-07-11 tabling — current version):** the tree
  holds ALL boundary fixes (glide + stop-gates + record-lock incl. Bypass/A/B + dockets
  1-3 + DP matcher + counts dialog + §B.9 backfill + test-plan amendments + this block),
  built by Jeff and verified through Part 3. NEXT: the review phase — Newtone reference
  review (manual pull + Jeff's FL walkthrough) + missing-features survey + diagnose the 8
  BaySickPitch problems → consolidated spec + dockets → implement the tabled rework
  (9a/10a/12a/13a/14a/15a/16a) + parity + fixes as ONE body → full re-listen + Parts 4-5 +
  FB-11 → boundary commit(s) (message + FULL git status → approval) → G3 group-open per
  this plan's G3 section (plan files + G3-start ledger locks: 4a S-key, 4c FL Humanize
  capture, D-6 Riff Machine, D-8 Note Properties, Builder note-preview). An interim
  commit of the already-verified boundary fixes BEFORE the rework starts is Jeff's call
  to make at next session open (clean bisect point vs one combined commit).
- **Work-Log entries needed:** boundary gets its own entry at section pass covering the R3
  review (0/0/4 verified), docket fixes, the align-glide finding + fix, and the docket-4
  correction; NIT-4 (sync auto re-analyze UI freeze) still pending Jeff's deliberate
  judgment at the resumed ear-check.
