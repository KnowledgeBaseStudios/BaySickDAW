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
| G3 start | 4a exact S-key/indicator interaction; 4c FL Humanize reference capture (Jeff screenshot); D-6 Riff Machine spec; D-8 Note Properties spec |
| G6 start (QA-Templates open) | 13a specific genre picks; 13b gap-fill decisions (with the /preset-gaps report) |
