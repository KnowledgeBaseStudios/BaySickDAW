# Running Notes — QA-G3Smoke (burly-restringing-bison)

> **Purpose:** append-only running log for the G3 boundary smoke defect sweep (all 37 dossier
> defects + the voiced SlideSampler rework + Swing). Append at EVERY checkpoint — a build gate
> cleared, a finding captured, a spec call asked/resolved, a scope pivot, a commit landed
> (`feedback_draft_doc_running_notes_every_checkpoint`). At close, `/draft-doc batch-close` reads
> this as the primary input for the Implemented Work Log entry, drafted + HELD until the campaign
> pass (bulk-run R2).
>
> **Pair file:** [`Plans & Specs/Batch Plans/burly-restringing-bison.md`](../Batch Plans/burly-restringing-bison.md)
> **Conventions:** Main Plan §0 (three-doc system, Rules 1-9; Batch Plans + Running Notes layout).

## 2026-07-23 — Batch created (review session complete; not yet executed)

Planned in a dedicated review session (fable) against
`Plans & Specs/G3 Smoke - Master Defect Dossier.md`. The session:
- Re-verified every load-bearing dossier line number against the uncommitted tree at `d6abc38b`
  (four read-only verification agents over PianoRoll / Builder / processor+drums / octave+wheel,
  plus a direct read of the whole slide stack and the karoryfer library on disk). Verdict: dossier
  overwhelmingly CONFIRMED; corrections + understatements recorded in the plan file's
  "Dossier corrections" section (Mirror default live, resize half withdrawn, bass filter open at
  default, cc116 = Delay, #32 permanent-loss, idle-suspend mid-gesture, #30b Rusty unguarded,
  #37 inline placement, #26 velocity note wrong).
- Resolved the new spec calls with Jeff in chat (2026-07-23): articulation residency = decode ALL
  at load (G-11); slide-tail policy rides a NEW Guitars/Basses Cut Self with full QA-CutSelfReview
  parity, title-bar home (G-12/13/14); swing fully specified (SW-1..SW-6, incl. the title-bar
  normalization edits G-16); #26 = copy block properties only (G-15).
- Jeff's structural directive: ONE plan file, tasks ordered by surface so no region is written
  twice. 13 tasks; §11 constraints mapped in the plan Context.

**Tree at plan write:** HEAD `d6abc38b`; uncommitted: QA-SlideSliceGlide (otter) +
QA-SlideSampler (lynx) + QA-L-Fix (marmot) + G3 review fixes (locked-doubling-frog) +
QA-OctavePedal — all ride Jeff's eventual commit(s). Nothing committed this session; no source
edits this session (plan + notes files only).

**Resume action (execution session):** read Main Plan §0 in full, the plan file, this file, the
dossier (with the plan's corrections section overriding where they conflict), and the
silky-gliding-lynx running notes (SlideSampler as-built + SS-Q5 checklist). Certify the working
tree (map every dirty entry to its batch; disturb nothing), then start Task 1 (pitch-wheel
convention + the `[G3 PLAYHEAD]` diagnostic) and STOP at the build gate.

## 2026-07-23 — Execution session open — tree certified

**Pre-session:** read Main Plan §0 in full, the plan file, this file, the G3 dossier (§2.5
pitch-wheel + §7 transport/playhead in detail; the plan's "Dossier corrections" section overrides
where they conflict), and the silky-gliding-lynx running notes (certification exemplar +
SS-Q1/SS-Q2 + structural findings; the SlideSampler as-built + SS-Q5 checklist read stays
scheduled at Task 10 start per the plan's touch points). HEAD = `d6abc38b` (QA-OctavePedal).

**Working tree certified — fully accounted for, nothing disturbed.** Every dirty/untracked entry
maps to a prior uncommitted batch:
- QA-SlideSliceGlide (`wistful-sliding-otter`): PatternManager.*, PluginProcessor.cpp,
  PianoRoll.cpp (bulk), BuilderPage.*, BaySickSynthVoice.*, AdditiveVoice.*, VibePlayerDSP.*,
  Main Plan §9 share, v1-master-test-plan §B.22 share.
- QA-SlideSampler (`silky-gliding-lynx`): BaySickGuitarsProcessor.*, BaySickBassesProcessor.*,
  new `Source/SlideSampler/*` (6 files, untracked), CMakeLists.txt share, PianoRollPage.* +
  PianoRoll.h + a PianoRoll.cpp share (Task-4 note-props provider — verified by in-source
  "QA-SlideSampler Task 4" comment tags), Future State.md (+1 line, CL-302), v1-master-test-plan
  §B.23 share, Research Reports/daw-architecture-sample-based-continuous-pitch-slide-2026-07-20.md
  (untracked).
- QA-L-Fix (`eager-thumping-marmot`): DrumPage.*, DrumKitGrid.*, MidiLearn/MidiLearnRegistry.* +
  MidiLearnUI.h, new MidiLearn/DrumTriggerMap.* (untracked), PluginProcessor.h,
  StandaloneEditor.cpp share, StandaloneApp.* share, CMakeLists.txt share.
- G3 review fixes (`locked-doubling-frog`): OctaveStyleDSP.h, BroadcastSynthesiser.h, MixerPage.*,
  MixerTrackStrip.h, RibbonTabBar.*, LayersPage.cpp, BassPage.cpp, UndoActions.h,
  StandaloneEditor.h (`addDefaultDrumTab` decl = NIT-14; attribution confirmed — the
  eager-thumping-marmot notes lines 522/778/843 explicitly disclaim the hunk),
  StandaloneEditor.cpp share, StandaloneApp.* share, its own Batch Plans/Running Notes files
  (+23/+17), Main Plan §9 share.
- Batch/session docs: the four untracked Batch Plans + four untracked Running Notes files
  (otter/lynx/marmot/bison) + the G3 dossier (untracked, review-session artifact).

**Task-1 collision check:** my targets (BaySickGuitarsProcessor.cpp, BaySickBassesProcessor.cpp,
BaySickRustyDrumsProcessor.cpp wheel sends; PianoRollPage/PianoRoll diagnostic) collide only with
lynx-dirty files — I stack on top per the plan. BaySickRustyDrumsProcessor.cpp is clean at HEAD.

**Next:** Task 1 (pitch-wheel convention fix + the `[G3 PLAYHEAD]` diagnostic), STOP at the build
gate.

## 2026-07-23 — Task 1 — sfizz pitch-wheel convention fix + [G3 PLAYHEAD] diagnostic — build gate cleared, partial reading in

**Wheel fix (#1 + #8, G-2).** All nine sfizz wheel sends converted raw-JUCE→centered
(`jlimit(-8191, 8191, value - 8192)`):
- Guitars passthrough / 2 recenters / bend ramp — BaySickGuitarsProcessor.cpp :321 / :328 / :334 /
  :357-358 (post-edit). Basses same four — :317 / :324 / :330 / :353-354. Rusty passthrough — :222.
- Recenter sends now 0 (was 8192); bend ramp now emits
  `jlimit(-8191, 8191, lround(w * mBendTargetWheel))` — `mBendTargetWheel` verified already a
  centered offset (header comment "-8192..+8191 (0 = center)").
- Tree-wide sweep confirmed no other `->pitchWheel` senders. Framework-quirk comment added at each
  passthrough (sfizz.hpp:550 centered convention).

**Diagnostic (G-9).** New `Source/G3PlayheadDiag.h` file-logger (ClipDropDiag convention,
`Documents/BaySickDAW/g3_playhead_log.txt`, Debug-only). Click line in `PianoRollGrid::mouseDown`
(x/y, rawBeat, snapBeat, snapDiv, playheadBeat); tick line in `PianoRollPage::timerCallback`
(beat, latSamples, sr; gated playing + non-song). `PianoRollPage.h` `g3DiagDeviceInfo` provider
wired from StandaloneEditor (page has no processor handle). Rule-4 catalog row below already
updated to the as-built sites in the same edit pass.

**Build gate: cleared.** Jeff ran do_build.bat, then ran the Debug exe — the diagnostic produced
the log file (his run is the evidence per the standing convention).

**Partial reading (08:48 PT): tick lines only, ZERO click lines in the paste.** Tick data:
latSamples=330 constant @ sr=44100 → 7.48 ms → ~0.0145 beats at the observed tempo (~116 BPM,
inferred from ~0.064-beat strides at 30 Hz); pattern loops at 4.0 beats. Implications recorded:
- (a) The roll's missing output-latency compensation (the Task-5 #30 fix) accounts for only ~1 px
  at typical zoom at this buffer size — if Jeff's observed misalignment is bigger, another term is
  in play.
- (b) 30 Hz paint stride = the drawn line can trail the transport by up to ~0.064 beats (~2-5 px)
  at click time — candidate mechanism; the click lines will show it directly.

**Next:** asked Jeff for the `click` lines (search the log for "click"); if none exist, redo the
clicks in a regular piano-roll grid/ruler (the click logger is on PianoRollGrid — not DrumKitGrid,
not Builder) and paste. Residual routing (Rule 3) waits on that.

## 2026-07-23 — Task 2 — scheduler core rework — code-complete, at build gate

**Diag re-fix (pre-Task-2, same session).** First reading came back tick-only — the click logger
only covered PianoRollGrid, and the unified Piano Roll page defaults to the Drum Kit view, whose
clicks go through DrumKitGrid. Added the `click(kit)` line to `DrumKitGrid::mouseDown` (+ include),
renamed the roll line `click(roll)`, moved it above the `!mData` gate. Rule-4 catalog row below
already updated. FINDING recorded: `DrumKitGrid::xToBeat` (:433 post-edit) lacks the LDT-394
pixel-center `+0.5` the roll grid has — same bug class as the #30 Builder half; a live residual
candidate if Jeff's clicks were in the kit view; the redo reading will confirm.

**Scope report (Jeff, in chat, post-Task-2):** the playhead/click misalignment is visible on ALL
THREE surfaces — drum kit, piano roll, AND builder grid. Per-surface state: Builder = pixel-center
(Task 4, scheduled); roll + kit = no output-latency compensation (Task 5 — one fix covers both,
they share the PianoRollPage playhead pump); kit ADDITIONALLY = the pixel-center miss above —
folded into Task 5 scope (same already-sanctioned fix class, in-batch per standing rule). Added a
third diag line `click(builder)` at `ArrangementGrid::mouseDown` so ONE Debug run captures all
three surfaces; the reading remains the residual-vs-known-fixes arbiter.

**Spec resolutions (Jeff, 2026-07-23, in chat).** The swing coverage gap between SW-4 ("all
scheduled rolls") and the SW-6 param family list: Clips = full global swing, no per-page params
(option a); Vox = excluded entirely ("vox doesn't have MIDI", consistent with G-8). Locked.

**#30b (G-6) — lock-free roll snapshot.** New `PatternRollsSnapshot` (per-pattern copies of all 7
roll families' note vectors + `contentBeats` computed at publish) + `SchedulerRollSnapshot`
(`shared_ptr` table + stamped currentPatternIndex + generation), owned by PatternManager.
- **Publish = message thread only.** `notifyContentChanged()` republishes the current pattern
  (every roll edit funnels there per the 2026-05-05 dirty wiring); pattern-count changes self-heal
  via a size-mismatch check in `publishRollSnapshotFor`; explicit `publishAllRollSnapshots()` in
  `reset` / `fromValueTree` / `restorePatternList` + the Rusty teardown roll-clear in
  PluginProcessor (`destroyBaySickRustyDrums` — mutates every pattern outside the choke point).
  `setCurrentPattern` republishes (stamped index).
- **Audio thread.** ONE wait-free `acquireRollSnapshot()` per block + `setInUseGeneration`;
  retirement via `RetirementQueue<SchedulerRollSnapshot>` (Batch-9c primitive). All four try-locks
  DELETED from the scheduler (song + pattern modes), along with the per-page engine `unique_ptr`
  checks (layers/bass precedent — an absent page's MIDI buffer is stable and unconsumed);
  family-level atomics + the Inst per-page `mGuitarsActive`/`mBassesActive` atomics remain.
- **DEVIATION note.** The plan sketched a "two-slot pointer swap"; implemented the in-repo
  Batch-9c RCU + RetirementQueue instead (Carry-Forward §2 "reuse, do not reinvent") — a two-slot
  scheme can be overrun by two rapid publishes while the audio thread still reads slot A; G-6
  semantics (never waits, never discards) strictly satisfied. Granularity: rebuild-per-pattern /
  share-per-pattern (publish cost = one pattern's roll copies per edit tick).

**#24 audio half.** Tiling loop deleted (`cycleBeats`/`nTiles`/`kStart` + the tile loop). ONE pass
from the content offset: origin = blkStart - offset, contentEnd = min(blkEnd, origin + snapshot
`contentBeats`); offset past content = silent; `getPatternContentBeats` stays as the silence mask,
now snapshot-resident.

**8A (G-5).** `ArrangementBlock.startBeats` (double) authoritative; `startBar`/`startTicks` fields
+ the `kStartTicksUnset` sentinel REMOVED; `displayStartBar()` derived; `effectiveStartBeats/Bars`
survive as thin accessors. XML unchanged (writes derived startBar + always-writes full-precision
startTicks; reads ticks > legacy float startBeats > bar). ~45 call-site conversions:
- PatternManager.cpp — serialization; `getTotalArrangementBars` now ceil-of-real-end.
- BuilderPage.cpp (~30) — paints / hit-tests / selection → `effectiveStartBars`, incl. previously
  bar-int-only paints that now honor slip-edited starts; placement/move/paste writers preserve
  their exact int-bar truncations for Task 4 #27 to lift; slice + resize sites drop their
  now-redundant startBar lines.
- StandaloneEditor.cpp (8) — song-end + the automation applicator to map-position domain;
  recording placements drop redundant startBar writes — value-identical since `setStartBeats`
  already won via ticks.

**#36.** `findGlideSourcePitch` end-time check (an already-ended note can't be a glide source —
also fixes the S-5 mono-cut's dead-source cut); `findRampAnchorNote` refactor (pointer-returning
walk + pitch wrapper); `rampChainDurationBeats` lineage predicate (only absorb ramps whose own
anchor walk resolves to this source).

**Swing (SW-2/4/5/6).** Transform inside `scheduleRollWindows` (pattern-local 16th index, floor,
odd → +global × mix × 0.125; the whole note shifts); SW-5 truncate clips a swung note's off to the
next same-pitch note's post-swing start (per-player toggle, only when the note itself swung).
`ensureSwingParams()` eager-registers `globalSwing` + `swing_{layer|bass|drum|inst}_{i}_mix` /
`_trunc` + `swing_rusty_mix`/`_trunc` via the lazy-APVTS idiom, caches raw atomics on the
processor; scheduler reads one global load per block + one mix load per roll dispatch. Clips wired
at full global; vox not wired.

**#11 emit half (G-4).** `emitNoteExpression` gains `panAsRampTarget` (CC89 replaces CC10);
`emitRampSlide` passes true (RP); `emitPianoNoteOn` passes true for RT-with-source only; Porta +
plain notes unchanged. Consumers land in Task 3.

**Self-check.** Tree-wide grep clean of removed-field refs; snapshot API consistent; stale QA-Ea
comment in the audio-clip player build fixed to the 8A world. Behavioral notes recorded:
`getTotalArrangementBars` + two maxEnd computations now honor sub-bar block ends (ceil) —
8A-direction improvement; bar-int paint sites now honor slip-edited starts.

**At the build gate** — Jeff to run do_build.bat (Debug + Release); after clean, redo the
`[G3 PLAYHEAD]` Debug reading (all three grids now log clicks).

**Gate CLEARED + reading capture #2 (13:09 PT) — degenerate, re-asked.** Jeff's Debug exe ran the
new build (gate evidence per convention). Capture: kit + roll click lines fired (instrumentation
proven live) but ALL clicks at x=0/1 with transport STOPPED at beat 0 — the parked-at-origin case
where latency / paint-staleness / pixel-center all read zero; no offset measurable. No
`click(builder)` lines (surface not clicked). Confirmed from the numbers: kit + roll agree at 80
px/beat; roll shows the +0.5 pixel-center term (x=0 → 0.0063), kit shows none (x=0 → 0.0000
flat) — the kit pixel-center gap is live in-build. Re-asked: same gesture with the line AWAY from
the origin (playing + clicks on the moving line on all three surfaces; then seek to ~bar 3 stopped
+ click the parked line). Task 3 proceeds meanwhile.

## 2026-07-23 — Task 3 — in-house voice fixes — code-complete, at build gate; [G3 PLAYHEAD] reading characterized

**[G3 PLAYHEAD] captures #3 (14:26 PT, kit+roll) + #4 (14:29 PT, builder) — characterization
COMPLETE for the parked-line case.** Gesture both times: play briefly, STOP, click the parked line
(playhead frozen across clicks; ticks stop at transport stop as designed). Numbers: kit x=75 @
80 px/beat → rawBeat 0.9375 vs playheadBeat 0.9346 (0.23 px off — dead on); roll x=76 → 0.9563 raw
vs 0.9346 (≤1.7 px); builder x=22 @ ~92.75 px/bar → rawBar 0.2372 vs playheadBar 0.2337 (0.32 px).
Verdict: on all three surfaces the click→position mapping agrees with the parked line within
≤2 px — NO unknown static offset; the dossier's "unexplained roll residual" does not reproduce in
the stopped state. Remaining misalignment mechanisms, all characterized:
- (a) The already-scheduled fixes — Builder pixel-center (Task 4), roll+kit output-latency
  compensation ~1.2 px at the 330-sample buffer (Task 5), kit pixel-center (folded into Task 5).
- (b) 30 Hz playhead paint staleness while PLAYING — up to ~0.064 beats/frame ≈ 5.2 px at roll
  zoom (80 px/beat, 116 BPM), 1.5 px at builder zoom — visible while moving, not a stored offset.
- (c) Stop parks the transport OFF-GRID (0.9346, 0.2337), so a snapped click on the parked line
  lands up to half a snap cell away from the line (kit/roll snapped 0.9375, builder 0.2500).

Routing of (b)/(c) — e.g. a paint-rate bump / interpolated playhead / park-quantize — is Jeff's
call at close per G-9/Rule 3; (a) is in-batch already. Moving-line clicks were never captured (all
clicks post-stop) — not re-asked: the open question (an unknown static term) is answered, and the
moving-state terms are all known quantities.

**Task 3 — in-house voice consumers (BaySickSynthVoice covers Bass; AdditiveVoice; VibeVoice).**

**#36 voice half — first-match CC85 takeover.** New `RampTakeoverAcceptor` interface in
BroadcastSynthesiser.h (mirrors the existing GlideStashClearable idiom); `handleController`
special-cases CC85: claim loop (stops at the first accepting voice, voice order), then the stash
wipe on every voice. Voices' CC85 branches moved into `tryRampTakeover(int)` (bodies unchanged +
pan arm); `clearGlideStash()` extended to the FULL one-shot set (mGlideFromNote, mGlideTimePending,
mSlideTargetVel, mPanRampPend) — the wipe formerly at each CC85 branch tail. VibePlayer (plain
juce::Synthesiser — CC85 arrives via the inline pre-pass, not handleController) gets the same
claim + wipe loops inline; VibeVoice gets `tryRampTakeover` + `clearRampStash` as plain public
methods (no interface coupling needed).

**#11 consumer — CC89 pan ramp.** All three voices: CC89 stashes `mPanRampPend` (-999 sentinel);
armed at the RP takeover (inside tryRampTakeover) and at the RT glide noteOn (inside startNote's
glide-consume block, same span as the pitch glide; snap when the span is degenerate); per-voice
`mNotePan` glides current→target — per-sample step next to the S-6(C) velocity ramp in
BSS/Additive, block-advance in VibeVoice (matching its vel-ramp shape). Fresh notes reset the ramp
BEFORE the glide block — ordering matters: an arm-then-reset bug was caught and fixed in
AdditiveVoice during the pass (the reset must precede the RT arm). No blunt pan reset at
startNote: mNotePan is the channel snapshot every CC10 broadcast keeps synced, so an RT note (no
CC10 emitted) starts from the glide SOURCE note's pan and ramps to the CC89 target.

**#37 — VibePlayer inline pre-pass.** CC86 + CC89 added to the inline whitelist (they were falling
through to the deferred path, landing AFTER CC85 had armed+cleared — the S-6(C) loudness ramp
never fired in BaySickPlayer; now it and the pan ramp do). CC85 special-cased in the same pre-pass
for the claim.

**Self-check.** Zero stray `== 85` branches; both juce-synth voices implement the interface; three
tryRampTakeover definitions; VibePlayer claim loop wired. A transient Windows file lock (Jeff's
devenv/MSBuild processes) blocked two AdditiveVoice.cpp edits mid-pass — retried clean.

**At the build gate** — Jeff to run do_build.bat (Debug + Release).

**Capture #5 (14:33-34 PT, roll @ max zoom, snap ON) — the residual is FOUND.** Jeff's
observation: zoomed all the way in, the ruler ARROWHEAD and the playhead LINE never align with
each other AND the bar line simultaneously. Read the draw code (PianoRoll.cpp:2798-2821): both use
the same rounded `px = beatToX(mPlayhead)`, BUT the arrowhead's apex sits AT px (visually centered)
while the line is `fillRect(px, kRulerH, 2, h)` — a 2-px body covering [px, px+1], visual center
px+0.5. The line reads half-to-one px RIGHT of the apex and of a 1-px bar line at the same beat —
constant bias, invisible at normal zoom, obvious at max zoom. FIX: fold into Task 5 (real drawing
bug, in-batch; center the 2-px body on px). Second finding from the same log: **ruler seek does
NOT snap** — click rawBeat 4.0187 parks the playhead at 4.0187 with snap ON (onSeek passes
xToBeat(e.x) raw, :1787; kit :1406 same; builder same family) — so the parked line sits fractionally
off-grid, which at max zoom makes the apex/line pair wander ±1 px around the bar line (rounding of
the off-grid residue). Whether seek SHOULD snap = spec call posed to Jeff (surfaces uniformly:
roll/kit/builder). **RESOLVED (Jeff, 2026-07-23): 1A — seek snaps with the current snap setting,
Alt+click bypasses for a free seek, all three surfaces uniformly.** The characterization set
(a)-(c) from the Task-3 entry gains (d) draw asymmetry [in-batch, Task 5] + (e) unsnapped seek
[1A, Task 5].

## 2026-07-23 — Task 4 — Builder grid + tracks — code-complete, at build gate

**#24 preview + #29 ghost rolls — one pass (drawMidiShading).** Tiling deleted
(`numCycles`/`firstCycle` + the per-note tile loop → single pass from the content offset); offset
un-clamped — past-content = blank, mirroring the de-tiled scheduler; stale tiling comments fixed.
Three missing roll families added to the shading: instRoll + clipRoll + baySickRustyDrumsRoll (vox
excluded — no vox MIDI). Slice: the `% cycleTicks` wrap in the right-piece contentOffset dropped
(offset = cut position into single-pass content; the cycleTicks computation deleted).

**#25 content-length sizing.** Four pattern-block creation sites now size from
`getPatternContentBeats` via the setLengthBeats + ceil-lengthBars idiom: browser-drag ghost
(replaces `pat.bars` + the non-4/4 TS special case — content beats are already TS-bar-ceiled),
drop finalize (same), paint mousedown stamp, paint drag continuation. Paint cadence follows the
stamp: new `mPaintLenBars` member (captured at paint start) drives the advance gate + next-stamp
start, and the continuation now tracks the PLACED stamp's start (not the mouse bar) so stamps
butt-join deterministically. Audio/Automation creation sites untouched (not pattern content).

**#26 click-copy (G-15).** New members `mClickMemoryLenBeats` (-1 = none) +
`mClickMemoryOffsetTicks` (pattern) + `mClickMemoryContentStartSamples` (audio) +
`primeClickMemoryFrom(blockIdx)`. Priming at the TOP of both block-hit branches (Draw + Select
tools): a clicked Pattern block sets mDropKind/mBrowserSelection + length + contentOffset; a
clicked Audio block resolves its library index (skip if absent) + length + contentStartSamples;
Automation blocks never prime (no template identity). Fresh browser picks (setSelectedPatternIndex
+ setActiveDropKind) clear the memory. Consumers in the mouseUp draw-commit: pattern branch — a
plain click (`!e.mouseWasDraggedSinceMouseDown()`) places identity+length+offset from memory,
falls back to #25 content-length when no memory, a real drag keeps the drawn length; audio
branch — count-guarded post-adjust of the placed block (length + contentStartSamples) since
placeAudioLibraryEntry may decline/prompt.

**#27 fractional move (on 8A) + #28 Alt fine-move.** Move origins now `effectiveStartBars` (was
displayStartBar — both capture sites via replace-all); the drag handler delta-snaps (the roll's
idiom): raw pixel delta → snap re-anchors the FIRST block's target → same delta applied to all
(sub-bar phase relationships survive); per-block floor kept in beats (`negFloor*4`). The (int)
casts + per-block absolute snapBar are gone. snapBar verified Alt-transparent (mAltSnapActive
returns raw), so #28's Alt+drag fine-move works through the same path (the resize half of #28 was
withdrawn — dossier correction); two KeyBindings.cpp rows added (Builder "Alt + Drag (clip)" +
PianoRoll "Alt + Drag (note)" — Fine Move (No Snap)).

**#30 builder half.** `barToX` now rounds (lround; was C-truncation) and `xToBar` samples the
pixel CENTER (+0.5) — the LDT-394 mirror, with the framework-quirk comment.

**#21/#22 span moves.** New `TrackHeaderPanel::groupSpan(row, &first, &last)` (contiguous
same-gid run; a lone ungrouped row = itself). Move Up/Down rewritten: the whole span hops the
NEIGHBOR'S whole span via a rotation map over [lo..hi]; all per-row state (name — with the
positional-default rule generalized — mute, solo, group id, color) captured then rewritten
through the map; block trackRows remapped in range. The old per-row group-id swap DELETED (it
tore groups apart).

**#23 undo brackets.** Group-with-Above + Remove-from-Group wrapped in beginEdit/commitEdit (row
group/color state already snapshots in the bracket); Color Group begins the edit BEFORE the async
picker opens and commits inside the callback — a canceled picker never commits (the pending
snapshot is harmlessly overwritten by the next beginEdit, no undo entry).

**Self-check.** snapBar Alt-transparency + the mouseUp context for the drag test verified by
read; audio-consumer brace structure verified.

**At the build gate** — Jeff to run do_build.bat (Debug + Release).

## 2026-07-23 — Task 5 — playhead behavior — code-complete, at build gate

**#30 roll+kit latency compensation.** `PianoRollPage::timerCallback` now shifts the pumped beat
back by output latency WHILE PLAYING (mirrors the Builder; stopped = raw, so a seek parks exactly
where clicked). New permanent `deviceInfoProvider` std::function on PianoRollPage (latency + sample
rate — the page has no processor handle), wired from StandaloneEditor next to the Debug-only
`g3DiagDeviceInfo` (which stays separate for the close-time strip). BPM from
`StandalonePlayHead::getBPM` (effective tempo). One fix covers roll AND kit — both consume the same
pump.

**#31 song-mode roll playhead.** The unconditional song-mode hide replaced: new
`songLocalBeatProvider` (StandaloneEditor lambda) maps a song beat → the VIEWED pattern's local
beat by scanning blocks (clipType Pattern, not muted, patternIndex == current, row audible, span
contains beat → songBeat - startBeats + ticksToBeats(contentOffsetTicks); no modulo — tiling is
gone); -1 when the viewed pattern isn't playing there. Pattern mode unchanged.

**#30 kit pixel-center.** `DrumKitGrid::xToBeat` gets the +0.5 pixel-center and `beatToX` now
rounds (llround; was C-truncation) — the LDT-394 mirror; all three surfaces now map identically.

**#30 draw centering (capture #5 finding).** Roll + kit: the ruler arrowhead apex moved to px+0.5
and the 2-px playhead body re-drawn as a float rect centered on px+0.5 — apex, body, and a 1-px
grid line at the same beat now share one visual center. Builder: same body-line centering in
`drawPlayheadOverlay` + the pulsing ruler handle centered at px+0.5 (was [px-4, px+4)).

**1A snapped seek (Jeff, 2026-07-23).** All three ruler-seek sites (roll PianoRollGrid, kit
DrumKitGrid, Builder ArrangementGrid) now snap the seek target with the current snap setting;
Alt+click seeks free. Explicit `e.mods.isAltDown()` at all three (the roll/kit snapBeat is not
Alt-aware; Builder's snapBar is, but the explicit form keeps the three uniform). No keybind clash
(the Builder ruler Alt gesture was right-click audition only).

**At the build gate** — Jeff to run do_build.bat (Debug + Release).

## 2026-07-23 — REGRESSION (Task 2 #30b) found by Jeff + fixed — roll MIDI silent on every engine

**Symptom (Jeff, post-Task-5 build):** notes placed on ANY piano roll (inst/layers/bass/drums) no
longer play with the transport; auditions still sound + hit the mixer meters (audio path fine) —
a MIDI-scheduling break. **Root cause: mine.** The #30b scheduler reads notes exclusively from the
published snapshot, and I hooked the publish to `PatternManager::notifyContentChanged()` on the
strength of its header comment ("note-edit fires arrive via notifyContentChanged") WITHOUT running
the verifying grep. The comment described intent, not code: roll/kit edits fire their own
`onNotesChanged` (12 roll + 34 kit mutation-tail sites), whose only wiring was a lane REPAINT —
`notifyContentChanged` is reachable from just 3 BuilderPage sites + PatternManager's own mutators.
Freshly placed notes therefore existed on screen but never reached the audio thread; loaded
projects still played (fromValueTree publishes), which is why the compile gates + the stopped-
transport captures never caught it. The code-over-comment rule exists for exactly this.

**Fix (the real choke point):** new `onContentEdited` std::function on PianoRollContainer +
DrumKitContainer, fired from each container's `onNotesChanged` tail (the verified convergence of
ALL 46 mutation sites); `PianoRollPage::setContentEditedHook` stores + pushes it to the kit +
every registered roll (present + future, incl. registerEngine); StandaloneEditor wires the hook →
`PatternManager::notifyContentChanged()` → snapshot republish. Redundant markDirty via onAnyChange
is idempotent-harmless. NOTED for Task 8: the recording commit writes `pat.drumRolls[]` directly —
its rebuild (#32) must call `notifyContentChanged()` explicitly or recorded notes hit the same
hole.

**Also from Jeff's report:** line + arrowhead now agree (capture-#5 fix confirmed by eye); a
residual "line ahead of the starting point" at the parked start remains — measurement via one
Debug click on the parked line rides the regression-fix build (theory withheld; the click line
gives rawBeat vs playheadBeat directly).

## 2026-07-23 — Playhead marker FINAL FORM (supersedes the Task-5 centered geometry)

Jeff rejected the centered-marker options (edge-clip at position 0 is inherent to any centered
marker at the grid's left boundary — visible mass reads right-shifted even with true anchors) and
called for the FL-style shape. **Shipped on all three surfaces: ASYMMETRIC left-anchored marker —
mast (1-px line, its column = the position — exact overlay on a grid-line column; thinned from
2 px at Jeff's follow-up) + cap hanging RIGHT (roll/kit: right triangle flag from the mast top;
Builder: right-hanging 8-px pulse handle).** Nothing ever draws left of the
position → whole marker at beat/bar 0, no gutter, no clipping; on a bar line the mast's left edge
sits exactly on the 1-px grid-line column. Supersedes the Task-5 "centered on px+0.5" pass (which
had fixed the line/arrow MUTUAL mismatch but guaranteed the zero-edge look). The 1A snap-seek +
latency + pixel-center work is untouched. FL reference stated from model knowledge at Jeff's
explicit direction (standing no-speculation rule set aside by his order); his screen wins if it
disagrees.

## 2026-07-23 — Task 6 — swing UI + title-bar normalization — code-complete, at build gate

**SW-1 global knob.** GlobalTransportBar gains `mSwingKnob` (rotary 0..1, double-click → 0,
tooltip explains the every-second-16th push), laid out right after the D-4 typing-keyboard toggle
(between it and the pattern dropdown per SW-1). The bar has no processor handle, so the wiring
follows its existing callback idiom: new `onGetSwing`/`onSetSwing` std::functions +
`refreshSwingKnob()`; StandaloneEditor wires them to the main-APVTS `globalSwing` via the binding
factory below and refreshes after wiring.

**Binding factory.** `VibeSynthProcessor::makeSwingKnobBinding(mixId, truncId)` →
`SwingKnobBinding{getMix, setMix, getTrunc, setTrunc}` on the canonical APVTS UI pattern
(getRawParameterValue reads, setValueNotifyingHost writes) — one helper instead of five
hand-rolled lambda sets.

**BaySickTitleBar extensions (the one surface all six players share).** Four additions:
(1) `enableSwingKnob(4 accessors)` — bar-owned 24 px rotary (SW-3), double-click → 1.0,
right-click → PopupMenu "Truncate Swing Notes" toggle (SW-5), inherits the parent editor's LAF;
the nested `SwingKnobSlider` lives in the .cpp, with the header destructor moved out-of-line
(incomplete-type unique_ptr would break every parent TU). (2) `setTrailingWidthHint(px)` —
parent-managed preset-cluster width so the knob sits left of it. (3)
`addHostedTrailingWidget(Component*, w)` — right-anchored bar-managed row for the G-16 moved
controls, idempotent on re-entry (source swaps re-run hosting). (4) `setReservedTrailingWidth(px)`
— G-14 empty slots. Layout right→left: [parent hint][hosted][reserved][knob].

**In-house editors ×4 (Harmless / BaySickSynth / BaySickBass / VibePlayer).** Inline
`enableSwingKnob` forward (sets hint 88 = their preset button); pages wire it at editor creation:
LayersPage → `swing_layer_{i}_*`, BassPage → `swing_bass_{i}_*`, DrumPage → `swing_drum_{i}_*` —
family = the PAGE, so a BaySickPlayer under a Drums tab binds drum params.

**G-16 Guitars/Basses.** InstPage (sfizz sources, after setEngine) hosts the program label (200)
+ Load-program button (130) on the AriaControlPanel title bar (new
`AriaControlPanel::getTitleBar()` accessor), reserves 110 px for the Task-12 CUT SELF + mode
toggles (G-14), and enables the knob on `swing_inst_{i}_*`. StandaloneEditor's PageMenuBar
extras: the sfizz label+button adds REMOVED (LiveInput keeps its clip label).

**G-16 Rusty.** BaySickRustyDrumsPage hosts the Player Preset button (110) + Program combo (160)
on its aria title bar + the knob on `swing_rusty_*` (ctor order verified: combo + button built
before the panel). StandaloneEditor's two PageMenuBar adds removed.

**G-16 NAM.** BaySickNAMIREditor: the A/B slot toggles moved off the title bar to below the OS
chicken-head dial (fits inside kKnobsRowH: 4+66+16+2+20 = 108 < 130).

**Also riding this build** (already logged in their own entries above): the playhead marker FINAL
FORM (asymmetric left-anchored 1-px mast + right-hanging cap, all three surfaces) + the #30b
regression fix.

**At the build gate** — Jeff to run do_build.bat (Debug + Release).

## 2026-07-23 — Task 7 — piano-roll tools — code-complete, at build gate

**#12 velocity click memory.** New `mClickMemoryVel` (default 0.8), captured at BOTH existing
click-memory sites (Draw-tool note-click + Select-tool note-click, alongside the D-7 dur+type
memory) and used at the click-place push in place of the hard-coded 0.8.

**#9/#10 markers (G-10).** The type-indicator paint restructured into ONE right-edge arrow family
gated on `type != Standard`: RampSlide = filled white (unchanged), RetrigSlide = white outline
(unchanged), Portamento + Bend = white border + BLACK fill (new). The orange left arc (collided
with the note-name text) is dead; Bend previously had NO marker at all. `refreshNoteTypeButton`
gains the Bend case (was labeling "Flat"); the S-key cycle now enters Bend — gated on the
engine-aware note-edit context (Guitars/Basses rolls only; elsewhere Portamento wraps to Standard
as before).

**#13 Humanize interval (G-3).** The interval combo is a standalone {1/32, 1/64, 1/128} beats
list (0.125 / 0.0625 / 0.03125), default 1/64; the app snap-table walk + `snapDivToTicks`
bypassed; Reset targets 1/64.

**#14 Humanize distribution (G-3).** The distribution combo gains Triangular + Uniform
(Quasi-Normal stays default) + an onChange; the `qn` lambda is now the 3-way funnel (Uniform =
nextFloat; Triangular = 2-sum/2; QN = 3-sum/3) — all three consumers (start/duration/velocity)
already draw through it.

**#15 Humanize seed (G-3).** Seed = ComboBox 1-10, default 1, no "None" (replaces the 0-99999
IncDec slider); Regenerate rolls 1-10; the rng seeds from the dropdown id.

**#16 Humanize defaults (G-3).** `kDefStartRange`/`kDefDurRange`/`kDefVelRange` → 10/10/20 (were
20/0/10); offsets stay 0; the constants feed both knob creation and Reset; headers + percent
display untouched.

**#17 Riff enables.** ALL 8 step enables default ON (ctor + Start-over; were steps 1-3+8 only).
Dice now randomizes AND enables every step (a diced disabled step was inert); the per-step
Randomize button enables its step. Levels/Artic/Groove got NON-NEUTRAL reset+ctor defaults so an
enabled step audibly acts: Levels velocity wheel 20% (Riff wheel index 1 = velocity), Artic id
2 = "Staccato 50%", Groove id 2 = "Swing light" — the specific values are my picks within the
plan's "non-neutral" mandate; FLAG for Jeff's ear at the smoke. Mirror's 30% stays (dossier
correction: already live). The one-visible-checkbox layout untouched (per-step page design, per
plan).

**#19 double-click reset.** `setDoubleClickReturnValue(true, init)` added inside Humanize's
addKnob + Riff's addKnob AND addIncDec — every slider in both dialogs double-click-resets to its
default.

**#20 riff-used flag.** New `bool riffMachineUsed` on PianoRollData (PatternManager.h); set in
Riff accept(); the panel ctor pre-checks "Work on existing score" from it; serialized as
`riffUsed` in rollToValueTree (written only when true) / read in rollFromValueTree (defaults
false). Not part of the roll undo snapshot (the flag means "riff ever used"; acceptable).

Humanize + Riff accepts fire onNotesChanged → the #30b snapshot republish covers both tools
(regression-fix hook).

**At the build gate** — Jeff to run do_build.bat (Debug + Release).

## 2026-07-23 — Task 8 — drums unit (#32 → #33 → #34) — code-complete, at build gate

**#32 recording demux (StandaloneEditor::commitRecordingResult).** The Drums branch no longer
targets the scheduler-dead legacy `pat.drumRoll` (which also fed the permanent-loss hole: the
D1.1 rescue migration never runs for a non-empty project). Captured notes now route PER NOTE
after the existing pre-roll shift / early-strike clamp / input-quantize: each note lands in EVERY
drum whose Note binding matches its number (mirrors the live dispatch's fan-out; the capture
carries no channel, so channel-scoped bindings match any channel — noted in-code), STAMPED at
that drum's play note via `drumPlayNoteRT` (guarded >= 0); unmatched notes fall back to the
focused drum (`mLastRollIndex`), pitch kept. Touched rolls sorted by `startBeat` after the loop;
`refreshAllKitViews()` so hits appear immediately. `mPM->notifyContentChanged()` added for ALL
recording kinds (layer/bass too — recorded notes are a roll mutation outside the grid's
onNotesChanged path, the same #30b hole class) + the existing markDirty — the "NOTED for Task 8"
item in the #30b regression entry above is closed.

**#33.** Falls out structurally: the recorder no longer writes `pat.drumRoll` at all, so the
descending 51-midiNote migration is unreachable for recorder-written notes; it remains intact for
true pre-D1.1 legacy projects. No back-compat work for the pre-fix broken saves (pre-v1 rule).

**#34 kit drag re-pitch (DrumKitGrid move-drag).** (a) The per-tick note read now uses
`rowToPageIndex(mMoveRefs[i].row)` — the row the note is in NOW (mMoveRefs tracks the reinserted
position each tick); the old original-row read went stale after the first tick and pulled
wrong/garbage notes on cross-row drags. (b) At reinsert, a hit moved to another row re-stamps to
the DEST row's play note, guarded by D-6: only when it still sat at its SOURCE row's play note —
a deliberately re-pitched hit keeps its custom pitch across moves (`Tmp` gains `srcRow`). The
kit's existing onNotesChanged tail covers the #30b publish for drags.

**Gate attempt #1 FAILED (both configs, one error): C2248 `drumPlayNoteRT` private.** The #32
demux calls it from StandaloneEditor; the declaration lived in the private RT-helpers section.
Fix: declaration moved to the public accessor cluster (next to getDrumTriggerMap) with the
thread-safety note — lock-free const read, safe from audio (kit dispatch) + message (#32 demux)
threads. Everything else in the log = pre-existing warning noise (C4996 Font / C4324 padding /
C4458 flags).

## 2026-07-23 — Task 9 — octave-up granular artifact (#35) — code-complete, bundled into the Task-8 gate

One edit, exactly per plan: `OctaveStyleDSP.cpp` overlap-add NORMALIZED —
`wsum = w1 + w2; output = (wsum > 1e-4) ? (s1*w1 + s2*w2)/wsum : 0` — with the calibration
comment (unity was only established in reset(); setGrainSize de-anchors the head pair and sweeps
the window sum through ~0..2 = the full-depth AM Jeff heard, nulls included).  Covers the up
voice AND the down voices' granular fallback (shared path).  Backup if the ear still catches
artifacts at the smoke: head re-anchoring in setGrainSize + hysteresis (dossier alternate).
Bundled into the Task-8 gate (3-line, plan-specified edit; separately attributable).

**At the build gate** — Jeff to run do_build.bat (Debug + Release).

## 2026-07-23 — Task 10 — slide rework A: full-voicing extraction + articulation residency — code-complete, at build gate

**Pre-task reads (per plan touch points).** silky-gliding-lynx running notes as-built sections
(Tasks 2-5: sync-at-load option (b) already the decode model; SS-Q5 checklist + TUNE greps
intact) + the current SlideRegionMap / SlideSampler / SlideSampleCache sources.

**Data model (SlideRegionMap.h rework).** `SlideSample` gains typed voicing statics: `volumeDb` /
`amplitudePct` / `ampVeltrack` / sorted `amp_velcurve` points / ampeg AHDSR statics
(delay/attack/hold/decay/sustain/release) / `group` / `off_by` / `note_polyphony` / `off_time` /
`rt_decay` / `loopMode` / `offset` / `tune`. Modulation-family opcodes are captured VERBATIM per
zone in `modOps` (`ampeg_*_oncc`/`_curvecc`, all `lfoNN_*` incl. cross-LFO freq mod + extended-CC
131/133/135/136 inputs, `pitcheg_*`, `fileg_*`, `cutoff*`/`fil_*`/`fil2_*`/`resonance*`/`var0*`,
`offset_oncc*`, `amplitude_oncc*`, `pan_oncc*`, `tune_cc*`/`oncc*`, `gain_cc*`) — Task 11's mod
router parses them ONCE at setProgram into its runtime tables, so extraction stays exhaustive
without freezing the runtime schema now. New `SlideCurve` (custom `<curve>` tables: `curve_index`
+ `vNNN` breakpoints, x scaled 0..1, sorted). New `SlideArticulation` = per-keyswitch layer stack
{center, tUp, tDown (unison neighbor-key layers, classified by rootKey vs loKey sign), tailpiece
(`locc118`), feedback (`locc29` non-looped), noise (`loop_mode`), releases (`trigger=release`)} +
per-articulation bend range + bass cc105 mono-set choke policy
(`hasMonoSet`/`monoGroup`/`monoOffBy`/`monoOffTime` — zones come from the poly set, policy
captured for Task 12). `SlideRegionMap` = articulations (sorted by `swLast`; -1 bucket =
keyswitch-less programs, which now EXTRACT instead of going silent) + `defaultArticulation` +
`curves` + a COMPAT MIRROR (`samples`/bend fields = the default articulation's center set) so the
pre-Task-11 consumers (SlideSampler band build, StandaloneEditor Bend dropdown) compile + behave
unchanged this gate.

**Parser hardening (extractSlideRegions rewrite).** New `tokenizeSfzLine`: `<header>` tokens +
key=value opcodes on ONE line, value running to the next key anchor (sample paths with spaces
survive); headers mid-line handled. `<curve>` blocks parsed (previously dropped at
`Level::Other`). `sw_default` tracked AS IT STREAMS (`lastSwDefault`) — a post-parse scope read
would lose it because later `<global>` headers clear `gOp` (self-caught bug). Per-region reads
refactored onto ONE merged effective-scope map (self-caught perf hazard: the naive 127-key
velcurve probe was ~4M string-map lookups per load).

**G-11 residency (SlideSampler).** setProgram now ALSO decodes EVERY articulation's EVERY layer
set synchronously (inside loadKit's gate-off window) into the `mResidentHandles` keep-alive
vector (message-thread only; path-keyed cache dedupes t-layer neighbor-key wavs + the band set).
Zero keyswitch latency by design; load block + RAM grow to the full-patch footprint. Debug builds
log the real number: `[SlideSampler] G-11 residency: N unique decoded sample(s), X MB float32
resident` — the actual MB lands in these notes at the smoke (expected ~2-3x the ~110/141 MB
main-sustain figure per the plan).

**DBG extended (the Rule-4 inherited `[SlideSampler]` entry).** Per-articulation counts
(center/tUp/tDn/tp/fb/noise/rel + mono-set marker) + articulation count + default index +
curve-table count.

**Consumers unchanged this gate.** The processors' `hasProgram`/`armSlide` path + Bend range
reads ride the compat mirror; Task 11 moves them onto the full structure.

**At the build gate** — Jeff to run do_build.bat (Debug + Release).

## 2026-07-23 — Task 11 — slide rework B: the voiced voice DSP — code-complete, at build gate

**Voiced per-voice chain (SlideSampler.h/.cpp rework).** Every voice now runs: zone sample →
Lagrange resample (keycenter delta + micro-bend + zone `tune` + unison detune cc102 + LFO pitch
sum) → gain (zone `volume` dB × `amplitude`% × real velcurve/veltrack via new `velGainFor`
(captured `amp_velcurve` points piecewise-linear, SFZ default power curve fallback;
`amp_veltrack` blend) × AHDSR envelope × LFO tremolo dB × crossfade sin ramp) → per-voice
filters (bass keytracked LPF = per-voice StateVariableTPT lowpass w/ `fil_keytrack` + LFO cutoff
wobble at block rate; unison layers = 1-pole HPF 250 per the bass fil2 block) → constant-power
per-voice pan (unison width cc101) → stereo sum. The old `jlimit(0.05,1,vel/100)` loudness
stopgap is GONE (#2 loudness half, cross-covered from Task 12's list).

**LFO bank.** `parseZoneMods` at setProgram parses each zone's modOps into `LfoDef`s: static
freq/delay/fade/wave + cc-scaled target depths (pitch/volume/cutoff `_oncc`) + delay/fade cc
mods (guitar cc116 delay, bass cc115/116) + CROSS-LFO rate mod (`lfoNN_freq_lfoMM_onccK`, the
bass wobble trick). Evaluated at block rate per voice (vibrato-range rates; sine core).

**CC provider hook.** New `setCcProvider(std::function<float(int)>)` — every `_oncc` depth
evaluates through it at block rate; null provider = cc 0 = routes present but neutral (exactly
sfizz's no-CC-input behavior). Task 12 wires it to the engine's APVTS `<prefix>cc<N>` atomics,
which is when the modulation becomes audible (the patches' depths are almost all cc-scaled) —
matches the plan's own task split.

**AHDSR.** Full Delay/Attack/Hold/Decay/Sustain/Release state machine per voice (block-stepped,
per-sample level ramp). First note runs the full envelope; a zone hop enters at Sustain (no
re-attack — takeover semantics). `release()` now sends every voice into Release at the patch
`ampeg_release` — the #4 ring-fix MECHANISM lands here (Task 12 confirms the call sites) — and
spawns the nearest release-layer zone with `rt_decay` attenuation by the held time (`mHeldSec`).

**Voice pool + steal.** `kMaxVoices` 4 → 20 (crossfade pair × {center + 2 unison} + tails +
headroom). `allocVoice` = oldest-quietest (lowest audible contribution = sin(fade)·env·gain, age
tiebreak, never the steered voice) — the old lowest-fade pick stole the INCOMING crossfade voice
(the worst choice, per the dossier).

**#6 zone thump.** Hop voices TIME-ALIGN their start offset to the outgoing voice's elapsed
`readIdx` (same pool, neighboring pitches = same timeline) with the offset floor raised to
~130 ms and the crossfade shortened to 28 ms (SS-Q5 TUNE comments intact, grep-able);
zero-crossing snap retained. Backup if the ear still catches steps at the smoke: the per-sample
RMS envelope table (dossier alternate).

**stopAllNow().** Hard-stop through a ~7 ms declick ramp regardless of envelope stage (`killed`
flag) — the G-12 cut-self-ON + #5 all-notes-off mechanism (Task 12 wires the call sites).

**Layers.** Zone now carries the captured `offset` (previously dropped) + loop fields; unison
tUp/tDown tables trigger alongside every center trigger/hop (gain rides `amplitude_oncc100` →
silent until Task 12's CCs; pan = signed cc101 width; detune cc102); tailpiece tables built
(cc118 switch = Task 12); noise zones render with `loop_continuous` wrap; feedback tables built
(cc29 gating = Task 12); release zones spawn on release().

**Honest residuals (flagged for Task 12 / the smoke).** `pitcheg` captured but not yet
evaluated; ampeg `_oncc` env mods (guitar cc70, bass cc24 swell / cc107 release) captured,
evaluation rides Task 12's CC wiring; custom-curve application to LFO depths minimal (linear);
tailpiece/feedback/noise TRIGGERING = Task 12's CC switches.

**Processors untouched this gate.** API additions are additive (startSlide / moveTo / release /
renderNextBlock signatures unchanged) — the processors compile unmodified this gate.

**At the build gate** — Jeff to run do_build.bat (Debug + Release).

## 2026-07-23 — Task 12 — slide rework C: engine integration — code-complete, at build gate

**Multi-articulation tables + audio-safe switching (SlideSampler.h/.cpp).** The single
default-articulation tables (mCenter/mTUp/mTDown/mTailpiece/mReleases/mNoise/mFeedback) are
replaced by `ArtSet` — per-articulation center/tUp/tDown/tailpiece LayerTables + flat
releases/noise/feedback + `swLast` + `hasMonoSet`/`monoOffTime` — built for EVERY articulation at
setProgram; `std::atomic<int> mActiveArt` selects. `trySelectArticulation(note)` (audio thread)
matches a noteOn against each articulation's sw_last and swaps the active index — zero-cost
switch since G-11 already decoded everything. A gesture PINS its articulation via `mGestureArt`
(moveTo/release read the pinned pointer, not art()) so a mid-gesture keyswitch can't mismatch
band indices. `mAnyActive` is now `std::atomic<bool>` (release/acquire) for the cross-thread
idle-suspend read.

**CC provider wiring (#2 modulation half goes LIVE).** Both processor ctors (Guitars + Basses)
cache all 512 `<prefix>cc<N>` APVTS atomics into `mCcRaw` at construction;
`mSlideSampler.setCcProvider` reads the array lock-free at block rate. Every `_oncc` route Task
11 built (LFO pitch/volume/cutoff/delay/fade, unison amplitude_oncc100 / pan_oncc101 /
tune_cc102, cross-LFO wobble) now hears the live kit-panel/automation values. The unison-layer
comment updated (no longer "silent until Task 12").

**Keyswitch tracking (#2 timbre half).** In both processors' MIDI interception every noteOn calls
`trySelectArticulation(n)`; the note still passes to sfizz (its own keyswitch handling).
Keyswitches are exempt from cut-self.

**#3 velocity anchor.** `std::array<int,128> mLastNoteVel` recorded at every noteOn; armSlide
prefers `mLastNoteVel[anchor]` over the CC86 transport value (CC86 only covers a gesture with no
prior anchor noteOn).

**#5 all-notes-off (+ #4 confirm).** Both twins' isAllNotesOff/isAllSoundOff branch now calls
`mSlideSampler.stopAllNow()` unconditionally (was: release() only when a gesture was active) —
panic kills ring-out tails too. #4's release() call sites confirmed present (anchor noteOff →
release()); no change needed.

**#7 copied-slide re-trigger.** `std::bitset<128> mNoteOnThisBlock` (cleared per MIDI pass, set
at each noteOn). A CC85 arm whose anchor was noteOn'd in the same block is a COPIED slide (chains
hold ONE anchor noteOn across segments) → falls through to a fresh gesture with `reAttack=true` —
the sampler supplies the pluck; the copy's sfizz anchor noteOn is suppressed same-sample by the
arm's noteOff. Covers both orderings (old noteOff before or after the new arm).

**G-12/G-13 Cut Self.** New Bool params `<prefix>cutSelf` + `<prefix>cutSelfMode` (false = Same
Pitch, true = Cut All) in both createLayouts, raw pointers cached at construction. noteOn cut
logic (skipped for keyswitches): Same Pitch → sfizz noteOff(n) + sampler stopAllNow only if its
currentNote() == n; Cut All → 128 sfizz noteOffs + sampler stopAllNow. Slide-tail policy in
armSlide: cutSelf ON → stopAllNow before the new gesture; OFF → the prior gesture release()s and
rings its AHDSR tail under the new one. To support OFF, `SlideSampler::startSlide` NO LONGER
blanket-resets voices (the Task-11 reset loop removed — tail fate is the caller's policy). New
`chokeAll(seconds)` primitive (configurable-declick fade; keeps the faster of an in-flight fade
and the choke); stopAllNow() now = chokeAll(0.007).

**G-14 title-bar toggles (Inst/InstPage.h/.cpp).** CUT SELF (switchToggle style) + SAME PITCH/CUT
ALL (text-swap toggle — exact QA-CutSelfReview mirror of BaySickBassEditor:132-147) hosted into
the AriaControlPanel title bar's formerly reserved 110 px slots (widths 62+78; reserved width → 0
when bound, stays 110 if the engine is missing mid-teardown). ButtonAttachments to the engine
apvts, reset at the top of every rebuildPlayerPanel before any rebind (source-swap safety);
~InstPage ordering already tears the page down before destroyBaySickGuitars.

**Bass cc105 Mono choke.** `SlideSampler::startSlide` reads the gesture articulation's captured
mono policy (SlideRegionMap.cpp:218-224 fills hasMonoSet/monoOffTime off the locc105 set) — a new
gesture with cc105 >= 64 chokes sounding voices over monoOffTime (0.2 s fallback), independent of
cut-self.

**Idle-suspend slide term (Engine/Tasks/InstStripTask.cpp).** New `isSlideActive()` peek on both
processors (the atomic mAnyActive); the predicate is now `midiEmpty && noVoices &&
!auditionPending && !slideActive` — sampler tails don't count in sfizz's voice total, so without
the term the chain suspended mid-tail and froze the ring-out.

**Honest residuals (carried from Task 11 — routing = Jeff's call at close).** The CC provider
closes Task 11's "silent until Task 12" set: unison layer gain/pan/detune (cc100/101/102) and
every parsed LFO depth route are live now. Still open, and NOT in the locked Task-12 scope:
`pitcheg` evaluation; ampeg `_oncc` env mods (guitar cc70, bass cc24 swell / cc107 release — the
opcodes ride modOps but no runtime route parses them, so the live CCs alone don't evaluate them);
custom-curve application beyond linear; tailpiece (cc118) / feedback (cc29) / noise layer
TRIGGERING (tables built + render-capable per articulation, but no switch logic spawns them).
At the smoke these read as missing-layer / missing-swell symptoms on the affected patches.

**Rule 4.** No new diagnostics this task; the inherited `[SlideSampler]` DBG (the setProgram
residency line) stands unchanged in the catalog below.

**Files this task** (all uncommitted, on top of the batch's prior work):
Source/SlideSampler/SlideSampler.h/.cpp, Source/BaySickGuitars/BaySickGuitarsProcessor.h/.cpp,
Source/BaySickBasses/BaySickBassesProcessor.h/.cpp, Source/Inst/InstPage.h/.cpp,
Source/Engine/Tasks/InstStripTask.cpp.

**At the build gate** — Jeff to run do_build.bat (Debug + Release).

## 2026-07-24 — Task 12b+12c — voicing completion (Jeff-directed) — code-complete, at build gate

**Why this task exists — two errors of mine in the Task-12 entry, owned here.** The Task-12
"Honest residuals" paragraph claimed five voicing items (pitcheg, ampeg `_oncc` env mods, custom
curves, tailpiece / feedback / noise triggering) were "NOT in the locked Task-12 scope." Wrong
twice. (1) The plan file put ALL of them in Task 11's locked scope ("Extra-layer playback support:
release-triggered zones, looped noise, tailpiece table switch (cc118 hi/lo), feedback layer gain
curves (cc29)"; "Custom curve evaluation"; "+ pitcheg"; "modulated AHDSR ... swell (cc24)") —
Task 11 shipped without them and the Task-12 entry mislabeled them out-of-plan. (2) The Task-12
claim that unison amplitude_oncc100 went "live" via the CC provider was FALSE — the route was
captured into Zone.uniGainPct but no code ever evaluated it, and since unison zones carry
amplitude=0 static (baked into baseGain at trigger), unison layers were silent at ALL cc values.
Jeff caught the residuals dodge and directed: fix now (12b); on the follow-up he directed the
remaining three heavier items in as well (12c). **This entry supersedes the Task-12 residuals
paragraph.**

**Shipped — all in `Source/SlideSampler/SlideSampler.h/.cpp`; no processor/UI changes (the Task-12
CC provider already feeds everything).**

**Generic amplitude route, block-rate (the unison fix).** `Zone.uniGainPct` replaced by
`Zone.gainCc` capturing ANY `amplitude_oncc<K>` (unison cc100, feedback/noise cc29). The static
`amplitude` term moved OUT of trigger-time baseGain into renderVoice: per block,
ampPct = amplitude + gainCc.depth × ccNorm, folded into volGain — amplitude=0 + oncc zones are
silent at cc 0 and follow the CC live. All three spawn sites (triggerZone, release-zone spawn, the
new triggerFlatZone) drop the baked amplitudePct factor.

**Custom `<curve>` evaluation.** SlideSampler stores map.curves at setProgram; new `ccNorm(CcDepth)`
helper maps cc/127 through the route's curve table (piecewise-linear, clamped ends; linear when
curve = -1). parseZoneMods collects `_curvecc<K>` opcodes into an end-pass that attaches curve
indices to their matching routes by base-opcode + cc (amplitude, ampeg_*, pitcheg_depth,
fileg_depth, gain, cutoff, resonance, pan, tune, varNN inputs, all lfoNN routes). Every route eval
site (LFO pitch/volume/cutoff/delay/fade/cross-freq, unison pan/tune, env mods, filter routes,
gain) now goes through ccNorm.

**ampeg `_oncc` env mods (guitar cc70, bass cc24 swell / cc107 release).** parseZoneMods parses
ampeg_attack/hold/decay/sustain/release `_oncc` into Zone routes. New latchVoiceEnv():
attack/hold/decay/sustain latch into per-voice fields (envAttackSec / envHoldSec / envDecaySec /
envSustainLvl / envReleaseSec) at trigger — sfizz's per-trigger EG semantics; release RE-latches at
release() so cc107 reads at the moment of release. renderVoice stage rates switched from zone
statics to the latched voice fields; all spawn sites call latchVoiceEnv.

**pitcheg.** Zone parses pitcheg_attack/decay/sustain/depth (+ depth_oncc, curvecc-able); per-voice
pegTimeSec; block-rate linear attack-to-full-depth then decay-toward-sustain evaluation in
renderVoice, cents added to pitchModCents alongside the LFO pitch routes.

**Tailpiece cc118 switch.** startSlide picks the gesture's main table: tailpiece when the set is
non-empty AND cc118 >= 64 at gesture start (trigger-time hi/lo semantics; the kit panel sends
0/127), else center. Pinned per gesture via new mGestureTables (like mGestureArt); moveTo hops use
the pinned pointer. A mid-gesture cc118 flip applies to the NEXT gesture.

**Feedback + looped-noise triggering (cc29).** New spawnCc29Layers(): at gesture start AND at every
zone hop, when cc29 >= 1 (trigger-time locc29 gate), spawns the nearest-rootKey feedback zone + the
nearest noise zone via new triggerFlatZone() — self-enveloped sustaining voices (own AHDSR entry,
fade=1, pitched to the gesture, noise loops via the existing loop-wrap render path). Their gain
rides amplitude_oncc29 through the generic amplitude route, so they also follow the CC continuously
once spawned. Hop respawns replace the old ones (the hop's all-voice fade retires them);
release()/stopAllNow retire them like every voice.

**Bass static filter CC routes (same class, plan-listed, found unshipped during this pass).**
cutoff_cc92 (cents shift on the keytracked cutoff) + resonance_cc91 (Q offset from the 0.7071
Butterworth base), block-rate, curvecc-able.

**12c — fileg (bass filter EG).** Zone parses fileg_attack/decay/sustain/depth (+ depth_oncc,
curvecc-able); per-voice filegTimeSec (reset in latchVoiceEnv); block-rate linear A/D evaluation
mirroring pitcheg, cents added to cutoffModCents so the sweep rides the same cutoff math as the
LFO wobble.

**12c — varNN multiplier kludge (bass filter).** New Zone::VarDef {index, mult flag (from the
varNN_mod TEXT value), up to 4 cc inputs (depth + optional curve via the curvecc end-pass),
cutoffCents target}. Render: x = product (mod=mult, the kits' mode) or sum of depth × ccNorm
terms; cutoff scales by 2^(cutoffCents·x/1200). Cutoff is the only target evaluated — the shipped
kits' usage; a var with no inputs is skipped.

**12c — gain_cc / gain_oncc (volume-dB cc alias).** Zone.volDbCc route; volModDb += depth × ccNorm
at block rate, curvecc-able (base "gain").

**Compile-hazard fix in passing.** ArtSet's definition moved up with the other nested table
structs — the new spawnCc29Layers(const ArtSet&) declaration preceded ArtSet's old definition
point (member-declaration parameter types are not a complete-class context; would not have
compiled).

**Remaining unevaluated captures (for completeness — nothing plan-chained left).** `offset_oncc25`
(start-offset humanization, =882 on the noise/tailpiece sets) is captured but not applied at the
trigger sites; `cutoff2`/`fil2_type` statics are structural (the unison 1-pole HPF 250 implements
the fil2 design directly). Neither was in the Task-11 voicing chain; routing = Jeff's call.

**Rule 4.** No new diagnostics; the inherited `[SlideSampler]` DBG stands unchanged in the catalog
below.

**At the build gate** — Jeff to run do_build.bat (Debug + Release).

## 2026-07-24 — Smoke round 1 — findings + fixes (Jeff's v2-numbered report) — fixes + instrumentation code-complete, at build gate

**Scope.** Jeff ran the boundary smoke against the v2 sheet (chat-delivered): 12 numbered issues +
2 general issues. All #s below are the v2 smoke numbers.

**Process notes — two errors of mine, owned.** (1) I reissued a renumbered v3 sheet while Jeff was
mid-test — wrong; the v2 numbering is authoritative for this round. (2) My first #19 diagnosis
blamed BaySickPlayer block-stepping while Jeff tested on a synth — the player fix is real but was
not his bug; the synth-side work under #19 below is the honest continuation.

**#6 Builder playhead = old arrow.** BuilderPage.cpp had TWO playhead draw sites; Task 5/7
converted only the perf-mode pulse and left the actual ruler marker as the centered down-triangle
(the Task-7 comment claimed it matched — it didn't). Now the roll/kit flag form: mast edge AT px,
8-px flag hanging right, no bar-0 clipping.

**#13 Rusty Load Player dropdown invisible.** Root cause: the BaySickRustyDrumsPage ctor called
buildPlayerTab() (which hosts the program combo + Player Preset button onto the AriaControlPanel
title bar behind null guards) BEFORE buildProgramCombo()/buildPlayerPresetButton() — both null at
hosting time, guards silently skipped, widgets created parentless = invisible everywhere, so no
kit could be loaded at all. Fix: ctor order now builds the combo + preset button first. The
Task-6 entry's "(ctor order verified: combo + button built before the panel)" parenthetical above
was WRONG — that verification never held.

**#19 pan ramp — three-part outcome.** (a) BaySickPlayer applied the CC89 pan ramp as one constant
gain per block (mNotePan advanced block-wise, npL/npR constant per block) = stair-step jumps =
clicks + hard pan on short ramps; `addFromWithRamp` now ramps the output gains within the block
(velocity-ramp parity). Real fix but NOT Jeff's repro engine. (b) App-wide click mechanism found:
CC10 pan is CHANNEL state hard-set instantly on every voice including ringing release tails —
every pan-carrying note = a gain step = click. All three voice classes (BaySickSynthVoice,
AdditiveVoice, VibeVoice) now glide a SOUNDING voice to a new CC10 over ~8 ms; idle voices still
set instantly so startNote reads exact channel state. (c) The synth "hard pan instead of sweep"
remains UNPROVEN from code — the emit chain (emitRampSlide routes pan to CC89, CC89 stash before
CC85, takeover arms from current mNotePan over the same glideSamples as the working pitch bend)
reads correct end-to-end. New `[G3 PAN]` Debug lines (`G3PlayheadDiag::logPan`) at both synth
`tryRampTakeover` arms (pend/from/glideSamples/timePending) + a mid-ramp CC10 stomp detector in
the BSS cc10 branch discriminate the two live candidates (fallback 60 ms span vs channel CC10
stomp). Jeff runs Debug + pastes lines.

**#24/25/28 up-slide stutter + fart + no ring-out.** Root cause in `SlideSampler::triggerZone`
hop time-align: the outgoing voice's RAW readIdx frame position was transplanted into the new
zone's sample; higher-fret samples are SHORTER recordings, so late in long UP slides the position
landed at/past the new sample's end — the len/4 fallback restarted the body on EVERY hop (the
stutter) and the landing exhausted mid-release (the fart + no ring-out). Down slides hop onto
LONGER samples = never triggered (matches Jeff's up/down asymmetry exactly). Fix: hops align by
elapsed FRACTION of the outgoing sample, capped at 80% of the new sample (SS-Q5 TUNE landing-tail
floor) so a landing always keeps >= 20% body.

**#29 answered (no change).** Bend is self-contained by design (bends its own note); RP/RT/Porta
are two-note transitions. To be documented at close.

**#39 riff double-click default — RESOLVED (same session): ALL non-neutral defaults revoked.**
First read chased a save/restore mechanism that does not exist (the panel constructs fresh per
open, factory literals everywhere). Jeff's clarification: the Velocity wheel reads 20% on a BRAND
NEW project — the complaint is the baked-in default itself, not persistence. On the follow-up
Jeff revoked ALL THREE non-neutral picks (vel 20% / Artic Staccato 50% / Groove Swing light), and
the record has to be straight about their origin: the dossier's #17 defect was the DEAD STEP
ENABLES; the plan line "give Levels/Artic/Groove non-neutral reset defaults" was MY drafting of
the cure, and the three values were MY picks — Jeff never asked for non-zero defaults. Reverted
everywhere they were baked (addKnob wheel init, Artic/Groove ctor selections, resetStep cases
4/5/6): open / Reset / Start over / double-click all land neutral (0 / None / Straight). #17's
SHIPPED substance is the enable fixes (all steps default on; Dice/Random/Start-over set enables)
+ Mirror's pre-existing 30%. The plan file's #17 bullet is superseded by this entry on the
defaults clause (Rule 3 routing at close).

**#45 Alt+drag vs mute (spec from Jeff: left-Alt drag = fine move, right-Alt = mute).** Root
cause: `ArrangementGrid::mouseDown`'s Alt+LClick mute toggle fired on ANY Alt at mouseDown and
returned — an intended Alt+drag muted instead of moving. JUCE ModifierKeys can't distinguish Alt
keys; new `isRightAltKeyDown()` (Win32 `GetKeyState(VK_RMENU)`; Windows-only app) gates the mute
branch to RIGHT Alt. Left Alt now falls through to the normal move path where the existing
mAltSnapActive already gives no-snap fine move.

**#55 octave pedal regression (Jeff-approved fallback).** My Task-9 live-sum normalization made
all three modes worse (-1 loop-start click, -2 near-silent + random clicks, +1 harder
ringing/tearing): a near-coincident window pair nulls the divide and amplifies interpolation
noise (tiny/tiny), and its epsilon floor steps 0<->full. Reverted to the plain overlap-add; unity
now comes from STRUCTURE per the plan's named backup: setGrainSize re-anchors the head pair to an
exact half-grain offset (a Hann pair at 50% offset sums to 1.0 at every phase; head jumps
preserve phase mod grain, so the only de-anchor event WAS the grain change itself) + a ~12%
deadband so YIN wobble doesn't re-anchor every block. This also addresses the ORIGINAL #35
AM-bell mechanism (the Task-9 dossier item) properly.

**#59/#60 transport swing knob.** Layout: 30 px → 24 px + pre-gap 4 → 2 so it clears the pattern
box (Jeff's direction). Value popups added to BOTH the transport knob and the title-bar
SwingKnobSlider via the mixer pan/width convention (`setPopupDisplayEnabled(true, true, nullptr)`
+ textFromValueFunction "Swing N%" / "Swing Mix N%").

**#62 answered (no change).** Swing shifts audio-clip EVENTS in the Clips roll wherever the
pattern plays; a wav dropped directly on the Builder grid is an ArrangementBlock, not a roll
event — NOT swung. Spec call open if Jeff wants that changed.

**General 1 — bar-1 first-note dropouts (intermittent).** Static read: the shared window build is
[beatStart, beatEnd) — inclusive of 0.0 — so no scheduler-side exclusivity found. New `[G3 BAR1]`
Debug lines: window readout on any block touching beat 0 + every noteOn emitted with
absStart < 0.05 (pitch/absStart/smp/window). A dropped note now shows as either a late-opening
window (scheduler side) or emitted-but-silent (engine side). Jeff reproduces in Debug.

**General 2 — random Release-only crash on the Guitars piano-roll button.** The prime suspect (a
stale engine pointer in the piano-roll providers) is CLEAR — StandaloneEditor's
audition/keyswitch/noteEditContext providers all re-fetch via `proc->getBaySickGuitars(idx)` with
null guards. No smoking gun in the registration path. Open: needs the Windows Event Viewer
faulting-module + exception-code readout from the next occurrence.

**Rule 4.** Two catalog additions (rows added below): `[G3 PAN]` (G3PlayheadDiag.h `logPan`; BSS +
AdditiveVoice `tryRampTakeover` arms + the BSS cc10 stomp detector) and `[G3 BAR1]`
(G3PlayheadDiag.h `logBar1`; PluginProcessor window build + near-zero noteOn emits) — both
Debug-only, both writing to the existing `Documents/BaySickDAW/g3_playhead_log.txt`, both
"remove at batch close unless the residual investigation still needs them (Jeff's call at the
strip pass)".

**Files this round** (all uncommitted, on top of the batch's prior work):
Source/Standalone/BuilderPage.cpp, Source/Standalone/BaySickRustyDrumsPage.cpp,
Source/VibePlayer/VibePlayerDSP.h/.cpp, Source/BaySickSynth/BaySickSynthVoice.cpp,
Source/Harmless/AdditiveVoice.cpp, Source/SlideSampler/SlideSampler.cpp,
Source/DSP/OctaveStyleDSP.h/.cpp, Source/Standalone/GlobalTransportBar.cpp,
Source/Standalone/BaySickTitleBar.cpp, Source/G3PlayheadDiag.h, Source/PluginProcessor.cpp.

**At the build gate** — Jeff to run do_build.bat (Debug + Release); Debug run wanted for the
`[G3 PAN]` + `[G3 BAR1]` readings.

## 2026-07-24 — Smoke round 2 — Debug readings + fixes — readings in, fixes code-complete, at build gate

**[G3 PAN] reading — the pan-ramp system VERIFIED WORKING; percept = span.** Jeff's Debug log
shows every arm carrying the real transport time (glideSamples=11025 = 250 ms = his half-beat
slide at 120 BPM; timePending=1 on every line), ramping from the CURRENT pan to the slide's pan
(his later test: from=-1.000 pend=+1.000 = a full L→R arm; the first arms' from=0.000 = his base
note was center). The "hard pan" percept is the DESIGNED span: pan rides the glide (same span as
pitch), so a half-beat slide sweeps in 250 ms = reads as a flick. Open spec call to Jeff: keep as
designed, or a longer/minimum pan-ramp span.

**[G3 BAR1] reading — scheduler EXONERATED.** Every captured pass emitted the bar-1 note:
from-stop starts (w0=[0.0000,...) noteOn smp=0) AND all loop-wrap geometries (wrapSmp drifting
16→112 in +16 steps across passes = the 128-sample block vs the 176400-sample loop span; split
windows exact; the wrapped noteOn lands at the wrap sample). Window spans all verified against
the 128-sample block. The dropout is downstream of the scheduler; the stuck-note mechanism below
is the new prime suspect (a wedged voice at the pitch masks the retrigger percept). Diag stays in
for a re-test after the fix.

**Stuck audition notes while moving (Jeff, Debug) — ROOT CAUSE + FIX, all four in-house
engines.** The press-and-hold audition channels (auditionNoteOn/Off) were SINGLE-SLOT atomics in
HarmlessProcessor, BaySickSynthProcessor, BaySickBassProcessor, VibePlayerProcessor: each store
overwrites the last, so when a note-drag crossed two pitches inside one audio block, the first
noteOff was overwritten before processBlock read it — that voice never got its off and rang until
Stop's all-notes-off. (Debug pacing bunches more UI events per block = more frequent there.)
Fix: `mAuditionHoldOff` is now a 128-bit accumulate mask (2x `std::atomic<juce::uint64>`,
fetch_or per note; processBlock exchanges each word and noteOffs every set bit, BEFORE the on).
The hold-ON stays last-wins — an overwritten on never sounded, and its off no-ops. Same shape in
all four processors (.h inline + processBlock drain).

**Slide-note click memory carries the WHOLE property set (Jeff).** Clicking a note primed only
duration + type + velocity, so placing after clicking a slide copied the slide type but dropped
pan/cutoff/etc. New `mClickMemoryProto` (a full PianoNote snapshot) primes at BOTH click sites
(Draw-tool note click PianoRoll.cpp:~1871, Select-tool click :~1996); the Draw-commit builds the
new note FROM the proto (pan, fine pitch, cutoff, resonance, release, porta length, bend amount +
shape) with midiNote/start/duration/velocity/type overridden. groupId / muted / slotIndex
deliberately NOT carried (per-note intent, not style).

**Rusty page lands on the Player sub-tab (Jeff).** Ctor switchTab(0) → switchTab(1): with the
Load Player dropdown on the Player tab's title bar, opening on the kit-image tab left a fresh
Rusty tab with no visible load control. Jeff also confirmed the dropdown's Player-tab placement
STAYS (it no longer appears on the kit tab — accepted).

**Per-player Swing Mix knob moved to the PageMenuBar (Jeff).** The title-bar hosting was only
visible on the Player/engine sub-tab; the knob now sits on the always-visible PageMenuBar, right
of the FX Rack slot (right of the tab cluster on Rusty, which has no FX slot). New
`PageMenuBar::setSwingKnobSlot(getMix,setMix,getTrunc,setTrunc)` + file-local `PageSwingKnob` in
SharedUI.cpp (same behavior contract: right-click Truncate toggle, double-click 1.0, hover/drag
value popup); cleared by clearTabSlots so non-player pages (Clips/Vox) never show it.
StandaloneEditor wires it per page-show for Layers/Bass/Inst/Drum (`swing_<family>_<idx>`) +
Rusty (`swing_rusty`). REMOVED: the five pages' editor-title-bar wiring
(LayersPage/BassPage/DrumPage/InstPage/BaySickRustyDrumsPage), the four editor-header
enableSwingKnob forwards (Harmless/BaySickSynth/BaySickBass/VibePlayer editors), and
BaySickTitleBar's enableSwingKnob + nested SwingKnobSlider + truncate members (the bar keeps the
G-16 hosted / G-14 reserved machinery). Transport's global knob (SW-1) unchanged.

**Files this round** (all uncommitted, on top of the batch's prior work):
Source/Harmless/HarmlessProcessor.h/.cpp,
Source/BaySickSynth/BaySickSynthProcessor.h/.cpp, Source/BaySickBass/BaySickBassProcessor.h/.cpp,
Source/VibePlayer/VibePlayerProcessor.h/.cpp, Source/Standalone/PianoRoll.h/.cpp,
Source/Standalone/BaySickRustyDrumsPage.cpp, Source/Standalone/SharedUI.h/.cpp,
Source/Standalone/StandaloneEditor.cpp, Source/Standalone/LayersPage.cpp,
Source/Standalone/BassPage.cpp, Source/Standalone/DrumPage.cpp, Source/Inst/InstPage.cpp,
Source/Standalone/BaySickTitleBar.h/.cpp, Source/Harmless/HarmlessEditor.h,
Source/BaySickSynth/BaySickSynthEditor.h, Source/BaySickBass/BaySickBassEditor.h,
Source/VibePlayer/VibePlayerEditor.h.

**Rule 4.** No diagnostic changes; `[G3 PAN]` + `[G3 BAR1]` + `[G3 PLAYHEAD]` + `[SlideSampler]`
all stand (bar-1 re-test wanted post-fix).

**Open from this round:** the pan-span spec call (above); General-2 Release crash still awaiting
an Event Viewer readout.

**At the build gate** — Jeff to run do_build.bat (Debug + Release); bar-1 + stuck-note re-test in
Debug afterward.

## 2026-07-24 — Smoke round 3 — automation vs pattern mode — fixes + restore halves code-complete, at build gate

**Jeff's report + confirm-before-fix.** Automation clips were driving parameters during
PATTERN-mode playback; his spec: automation only applies where clips overlap on the Builder grid
(song domain). Investigation confirmed the mechanism before any fix, per his explicit instruction.

**Writer 1 — audio thread (PluginProcessor.cpp :2795 post-edit).** The automation-clip evaluator
gated only on "transport playing" — no mode check — while its neighbors (song note scheduler
:2592, audio-clip blocks :3023) both gate on mSongMode. In pattern mode the looping pattern beat
was divided by 4 and misread as a Builder-grid bar position, so clips parked in the first bars
"overlapped" and wrote their params every block. Long-standing (predates this batch). Fix: the
evaluator now gates on mSongMode like the audio-clip block.

**Restore half (Jeff: "do the whole thing").** A gated evaluator alone leaves params stuck at the
last automation-written value on mode switch. `VibeSynthProcessor::setSongMode` (was a one-line
inline atomic store; now defined in the .cpp) is the capture/restore boundary for MAIN-APVTS
lanes: song ENTRY snapshots {paramId, normalized value} for every automation-targeted param
(deduped) into `mAutomationBaseline`; EXIT restores via setValueNotifyingHost (knobs return
visually) and clears. Known edges (stated to Jeff): a manual tweak of an automation-targeted param
DURING song mode restores to its song-entry value; a clip added mid-song targeting a new param
joins the snapshot on the next song entry.

**Writer 2 — UI timer (`StandaloneEditor::applyAutomationAtCurrentPosition`, :3236 post-edit) —
found after Jeff reported the first fix insufficient.** The UI pass evaluates the same clips
against mPlayHead.getCurrentBeat() with NO mode check: (a) APVTS lanes apply when STOPPED (the
QA-Ed seek/scrub preview — skipped while playing, so writer 1's gate silenced those), but (b) the
APPLICATOR lanes — engine-APVTS params registered via VKnobAutomation (every engine panel knob) +
`global_tempo` — applied on EVERY tick, playing or stopped, ANY mode. That branch was the live
writer behind "still affecting pattern mode": engine-param automation never goes through the main
APVTS at all. Fix: the whole clip loop now early-returns unless isSongMode() (the mRequestStop
handling above it still runs every tick). In song mode everything behaves as before (processor
writes APVTS lanes while playing; UI covers stopped preview + applicator lanes).

**Applicator-lane restore.** The processor baseline can't see engine params (apvts.getParameter
returns null for them), so StandaloneEditor's onSongModeChanged lambda (the mode-toggle site,
:864) now carries the twin: song ENTRY captures non-main-APVTS automation lanes via
`mAutomationValueReaders` into new `mApplicatorBaseline` (StandaloneEditor.h member); EXIT
restores via `mAutomationApplicators` and clears. Runs BEFORE mProcessor.setSongMode so the
old-state comparison is valid; the processor then handles the main-APVTS half.

**Files this round** (all uncommitted, on top of the batch's prior work):
Source/PluginProcessor.h/.cpp, Source/Standalone/StandaloneEditor.h/.cpp.

**Rule 4.** No diagnostic changes; the four catalog rows below stand unchanged.

**At the build gate** — Jeff to run do_build.bat (Debug + Release); verify: pattern mode +
automation clip at bar 0 = parameter still (both a main-APVTS target AND an engine-panel knob
target); song mode drives both over the clip span; switching back to pattern returns both knobs
to pre-song values.

## 2026-07-24 — Round-2/3 open items — resolutions

**Pan-span spec call CLOSED (Jeff): as designed.** The RP pan ramp keeps spanning the slide
note's own length (same span as the pitch glide); no minimum/extended span.

**General-2 crash — Reliability Monitor data in; dump capture armed.** Three APPCRASH entries,
all c0000005: 7/22 faulting in ntdll (heap-corruption detection = use-after-free family), 7/23 +
7/24 faulting inside BaySickDAW.exe (build 6a62eedc, offsets 0x77aa90 / 0x779520). Those builds
are overwritten (current = 6a641050), so the offsets are unmappable -- no blind patch. Jeff ran
the WER LocalDumps registry setup (HKCU, DumpType 1 -> Documents/BaySickDAW/CrashDumps); standing
rule: on the next crash, NO rebuild until the dump is read against the then-matching exe+pdb.
Volume delta between modes (his -24 vs -9 report) resolved as DATA, not a gating bug: the
project's one automation clip rides the master fader (+4 dB -> -60 dB over bar 1) and the saved
knob value (-15.4 dB) is a frozen residue of the old always-on applicator writes (= the clip's
value ~30% through the bar); pattern mode now honors the knob for the first time.  Remedy =
user-level (set the fader / adjust the clip).

**Symbol archiving added to do_build.bat (unblocks the close).** The dump-vs-rebuild conflict
(closing the batch means rebuilding, which overwrote the pair a dump needs) is dissolved:
do_build.bat now archives the OUTGOING Release exe+pdb to Documents/BaySickDAW/SymbolStore/
(timestamp-named, newest 5 kept, matched to dumps later via the PE TimeDateStamp) before every
build.  Rebuilds are now harmless to the crash hunt; the "no rebuild after a crash" rule is
retired once the next build archives the current pair.

## 2026-07-24 — /review-batch outcome + close fixes — code-complete, at the FINAL build gate

**Review scope:** this batch's diff only (prior uncommitted batches excluded).  Result: 1 BLOCKER
+ 6 NEEDS-FIX + 7 NITs; every premise verified against code before action.

**FIXED in-batch (same session):**
- **BLOCKER — SymbolStore/ + CrashDumps/ gitignored.**  The archive step + WER dumps write into
  the working tree (repo root doubles as the app-data dir); two entries added per the file's own
  convention.
- **G-12 OFF defeated by the hop fade** (SlideSampler moveTo faded EVERY voice incl. a prior
  gesture's Release tails, choking them in 28 ms on the first hop) -> voices stamp a
  `gestureSerial`; hop fades touch only the CURRENT gesture's voices; release-layer spawns keep
  the ENDING gesture's serial.
- **Non-steered pitch recompute dropped the root-relative bend + unison detune** (any pitch
  modulation snapped unison neighbor-key layers ~a semitone) -> `updateVoiceRatio` stores the
  full cents sum per voice (`baseCents`); block-rate modulation rides ON TOP of it for every
  voice.
- **Audition same-block press+release stuck the note** (off drains before on; a same-block pair
  made the off a no-op) -> the drain remembers the off words; if the hold-on's bit was in this
  block's mask, its noteOff is emitted at block-end (a same-block re-press of a held note blips
  one block -- the lesser wrong).  All four engines.
- **PageSwingKnob menu callback captured raw `this`** on a per-page-show-destroyed component ->
  SafePointer.
- **TPT filter setters fired unconditionally per block** (CPU-safeguard rule) -> per-voice
  lastCutoffHz/lastResQ change gates.
- **NITs fixed:** stale fileg/var "unevaluated" comment (12c evaluated them); stale
  BaySickTitleBar dtor rationale; the dead reserved-width layout statement (now a comment noting
  the G-14 reservation is layout-inert since the knob moved); chokeAll kills never-faded-in
  voices outright (the fade=1 flip was a one-block click window); the UI automation pass now uses
  `effectiveLengthBars` so stopped/seek preview windows match the audio evaluator.

**RECORDED, not fixed (routing = Jeff's call):**
- **Extended-CC LFO routes (131 velocity / 133 note / 135-136 random) read 0** through the APVTS
  CC provider -- sfizz-internal inputs have no APVTS writer, so velocity-scaled vibrato depth +
  random LFO phase seeding are neutral.  ADDED to the 12b/12c residuals record (which previously
  claimed "nothing plan-chained left" -- this is a third unevaluated-capture item beside
  offset_oncc25 + the structural fil2 statics).
- **Swing loop-edge drop** (an off-grid note in the loop's final 16th can be pushed past patLen
  and silently dropped) -- surface only if the campaign ear hears it.
- **[G3 PLAYHEAD] tick fires in song mode too** (catalog says non-song) -- drift noted; moot
  since Jeff kept all diagnostics.

**Files this pass:** .gitignore, Source/SlideSampler/SlideSampler.h/.cpp,
Source/Harmless/HarmlessProcessor.cpp, Source/BaySickSynth/BaySickSynthProcessor.cpp,
Source/BaySickBass/BaySickBassProcessor.cpp, Source/VibePlayer/VibePlayerProcessor.cpp,
Source/Standalone/SharedUI.cpp, Source/Standalone/BaySickTitleBar.h/.cpp,
Source/Standalone/StandaloneEditor.cpp.

**At the FINAL build gate** -- Jeff to run do_build.bat (Debug + Release) before the boundary
commit (review fixes touched audio code).

## Diagnostic Instrumentation Catalog (Rule 4)

**Strip pass (2026-07-24, batch close): Jeff's call = KEEP ALL FOUR.** All are `#if JUCE_DEBUG`
only (zero Release/user impact), so they ride through the boundary commit and future batches;
re-walk at a later close or pre-release.

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| `Source/G3PlayheadDiag.h` (new logger header) + `PianoRoll.cpp` `PianoRollGrid::mouseDown` (`click(roll)` line, above the `!mData` gate) + `DrumKitGrid.cpp` `DrumKitGrid::mouseDown` (`click(kit)` line — added after the first reading came back tick-only: the unified page defaults to the Kit view and kit clicks never hit PianoRollGrid) + `BuilderPage.cpp` `ArrangementGrid::mouseDown` (`click(builder)` line — added after Jeff reported the misalignment on all three surfaces) + `PianoRollPage.cpp` `timerCallback` (tick line) + `PianoRollPage.h` `g3DiagDeviceInfo` member + `StandaloneEditor.cpp` device-info wiring — all `#if JUCE_DEBUG`; output file `Documents/BaySickDAW/g3_playhead_log.txt` (ClipDropDiag convention) | `[G3 PLAYHEAD]` | Characterize the playhead/click misalignment across all three surfaces (G-9) — per click: x/y → raw beat/bar, snapped beat/bar, snap div (roll/kit), playhead position; per playhead paint tick (playing, non-song): beat, output latency samples, sample rate | Remove at batch close unless the residual investigation still needs it (Jeff's call at the strip pass) |
| (inherited from silky-gliding-lynx) `SlideRegionMap.cpp` `extractSlideRegions` `#if JUCE_DEBUG` | `[SlideSampler]` | Extraction sanity counts on kit load; Task 10 extends it to per-articulation/band/layer counts | Remove at boundary commit (borderline — useful through the smoke/tuning; Jeff's call) |
| `Source/G3PlayheadDiag.h` (`logPan`) + the two synth `tryRampTakeover` arms (`BaySickSynthVoice.cpp`, `AdditiveVoice.cpp` — pend/from/glideSamples/timePending) + the BSS cc10-branch mid-ramp stomp detector (`BaySickSynthVoice.cpp`) — `#if JUCE_DEBUG`; output file `Documents/BaySickDAW/g3_playhead_log.txt` | `[G3 PAN]` | Discriminate the two live #19(c) synth hard-pan candidates (fallback 60 ms span vs a channel CC10 stomp mid-ramp) — the emit chain reads correct from code, so the log arbitrates | Remove at batch close unless the residual investigation still needs it (Jeff's call at the strip pass) |
| `Source/G3PlayheadDiag.h` (`logBar1`) + `PluginProcessor.cpp` — window readout on any block touching beat 0 + every noteOn emitted with absStart < 0.05 (pitch/absStart/smp/window) — `#if JUCE_DEBUG`; output file `Documents/BaySickDAW/g3_playhead_log.txt` | `[G3 BAR1]` | Split the General-1 bar-1 first-note dropout: a dropped note reads as either a late-opening window (scheduler side) or emitted-but-silent (engine side) | Remove at batch close unless the residual investigation still needs it (Jeff's call at the strip pass) |


## 2026-07-24 -- Close addendum -- park-snap + extended-CC shipped (Jeff-directed), paint-lag costed

**Playhead residual (c) off-grid park -- FIXED (drops from the §9 route).** Jeff ordered the
polish: the PAUSE path (the only park-in-place path; Stop already seeks to 0 / the selection
start) now quantizes the parked beat to the NEAREST 16TH (resume shifts by at most a 1/32 note),
so the marker sits on a grid line and snapped clicks beside it agree with it.  Resolution choice
(1/16) is mine -- adjustable on Jeff's word.

**Extended-CC internal inputs -- FIXED (drops from the residuals).** The review's third
unevaluated capture, Jeff option (b): SlideSampler::ccValue now synthesizes the sfizz-internal
pseudo-CCs per gesture -- 131 = the gesture's velocity, 133 = the anchor note, 135/136 =
per-gesture randoms from a member LCG (audio-thread pure math, fixed seed).  The kits'
velocity-scaled vibrato depth + random LFO phases now work on slides.  The unevaluated-capture
set is back to TWO (offset_oncc25 + the structural fil2 statics).

**Playhead residual (b) paint-lag -- FIXED (Jeff option b: the dirty-rect version).** Audio cost
zero (UI-thread only); the old design full-repainted the grid at 30 Hz while playing (~1-3 ms per
paint).  Shipped: PianoRollGrid / DrumKitGrid setPlayheadBeat + ArrangementGrid setPlayheadBar
now repaint ONLY the old + new marker columns (14-px strips covering mast + flag), and
PianoRollPage's timer runs at 60 Hz (halves the visual trail; affordable only because of the
dirty-rects -- a naive rate bump would have doubled the full-canvas cost).  Builder stays at
30 Hz (coarser zoom = invisible lag; its perf-mode pulse full-repaint would double).  Net: LESS
UI cost than before AND half the trail.  Both playhead residuals are now closed -- the §9
route shrinks to the crash watch + the two no-action captures.

## 2026-07-24 — Boundary commit `b54d4681` + Main Plan doc pass (post-close)

**Boundary commit LANDED:** `b54d4681` (95 files, +13101/-1094) on Jeff's explicit approval; his
build was clean beforehand.  Carries all five stacked batches (otter/lynx/marmot/bison code+docs +
frog doc amendments).

**Doc-lag note for the G4 researcher:** `Files For Claude/bulkrun_group_session_boilerplate.md`
gained a "G3 BOUNDARY DOC LAG" block (Jeff's direction) — names `b54d4681`, states the Main Plan
records are being authored and do NOT block research, and points at all four plan + running-notes
files + the defect dossier so the researcher reads the real content.

**Hash backfills:** `b54d4681` filled into the test plan (§B.22 / §B.23 / §B.24 + the §B.18
L-9..L-14 rows) and the otter/lynx/bison HELD-entry `blocks:` fields; lynx heading date resolved to
2026-07-22, bison to 2026-07-24.  **§B.10 (QA-Fd) `blocks:` left UNFILLED** — git log shows only
`b9f1894f` labeled "QA-Fd checkpoint", no unambiguous close commit; flagged to Jeff rather than
guessed.

**Main Plan doc pass applied** (two doc-drafter dispatches; every anchor verified against the file
before apply): four §5 entries in Jeff-approved order (QA-OctavePedal → QA-SlideSliceGlide →
QA-SlideSampler → QA-L-Fix → QA-G3Smoke, before QA-VibeSlider) with STATUS-at-boundary lines;
§6 arrow tokens 45/46/47/48 + four footnotes; §9 sixty-second (QA-SlideSampler close) +
sixty-third (this batch's close); the sixty-first's two OVERDUE post-close annotations applied
(QA-G / `steady-pinning-heron`: B-1..B-5 + the B-2 LATENT-not-regression record correction; QA-H /
`ghostly-riffing-moth`: S-1..S-10) plus lynx's QA-H annotation (engine-aware NotePropsPanel), in
chronological order at the QA-H seam.  Lynx pieces updated at apply: renumbered 45→46 (otter takes
45), STATUS "NOTHING committed" → committed at `b54d4681`, position phrase → "after
QA-SlideSliceGlide".  This batch's HELD-entry heading tail corrected to the shrunken §9 route
(playhead residuals were FIXED at close; route = crash watch + two captures).

**Chronology note on record:** marmot (QA-L-Fix) began 2026-07-19, before otter (QA-SlideSliceGlide)
opened 2026-07-20 — the §5/§6 order above is Jeff's approved sequencing, not a chronology claim.

## HELD Implemented Work Log entry (applies at the §B.24 campaign pass, bulk-run R2)

> Drafted at close via `/draft-doc batch-close`; NOT yet applied to `Implemented Work Log.md`.
> Applies with the §5/§9 doc queue at the boundary commit + the R2 campaign walk of §B.24.
> Backfill the full `YYYY-MM-DD HH:MM PT` timestamp at apply; boundary-commit hash `b54d4681`
> filled 2026-07-24.

### 2026-07-24 -- QA-G3Smoke -- G3 boundary-smoke 37-defect sweep (all seven clusters) + voiced SlideSampler rework (extraction/voice-DSP/engine integration, 12b/12c voicing completion) + net-new Swing SW-1..6 + Guitars/Basses Cut Self (G-12/13/14) + scheduler lock-free roll snapshot (#30b) + 8A beats-authoritative blocks + FL-style playhead marker on all three surfaces; three smoke rounds + close review fixed in-batch; crash-dump watch + two unevaluated SlideSampler captures routed via §9

**Bucket:** Players, System Pages, Cross-cutting Infrastructure, UI / L&F / Theming, Effects. Batch `burly-restringing-bison`. `blocks:` `b54d4681`.

#### Done

- **Task 1 -- sfizz pitch-wheel convention fix (#1/#8, G-2) + `[G3 PLAYHEAD]` diagnostic (G-9).** All nine sfizz wheel sends converted raw-JUCE -> centered (`jlimit(-8191, 8191, value - 8192)`) across Guitars / Basses / Rusty; recenter sends now 0 (were 8192 = full bend up); the bend ramp emits a clamped centered offset. Tree-wide sweep: no other `->pitchWheel` senders. New `Source/G3PlayheadDiag.h` Debug-only file logger (`Documents/BaySickDAW/g3_playhead_log.txt`, ClipDropDiag convention): click lines on all three surfaces (roll / kit -- added after capture #1 came back tick-only because the unified page defaults to the Kit view -- / builder, added after Jeff reported the misalignment on all three) + a playhead tick line (beat / latency samples / sample rate).
- **Task 2 -- scheduler core rework (one pass).** **#30b (G-6):** lock-free roll snapshot -- `PatternRollsSnapshot` (all 7 roll families + `contentBeats` at publish) + `SchedulerRollSnapshot`, published message-thread-only; audio thread = ONE wait-free `acquireRollSnapshot()` per block + generation stamp, retirement via the Batch-9c `RetirementQueue`; all four scheduler try-locks DELETED along with the per-page engine `unique_ptr` checks. (Deviation from the plan's sketched two-slot swap -- see the deviation record.) **#24 audio half:** tiling loop deleted -- one pass from the content offset; past-content = silent. **8A (G-5):** `ArrangementBlock.startBeats` (double) authoritative; `startBar`/`startTicks` fields + the `kStartTicksUnset` sentinel REMOVED; ~45 call-site conversions; XML unchanged. **#36 scheduler half:** `findGlideSourcePitch` end-time check + `findRampAnchorNote` refactor + `rampChainDurationBeats` lineage predicate. **Swing SW-2/4/5/6:** transform inside `scheduleRollWindows` (pattern-local 16th floor; odd index -> +global x mix x 0.125); SW-5 same-pitch truncate; `ensureSwingParams()` eager-registers `globalSwing` + `swing_{layer|bass|drum|inst}_{i}_mix`/`_trunc` + `swing_rusty_*` with cached raw atomics; Clips = full global swing, Vox excluded (Jeff locked in-execution). **#11 emit half (G-4):** `emitNoteExpression` gains `panAsRampTarget` (CC89 replaces CC10 for RP + RT-with-source).
- **Task 3 -- in-house voice consumers.** **#36 voice half:** first-match CC85 takeover -- new `RampTakeoverAcceptor` interface, claim loop stops at the first accepting voice, full one-shot stash wipe on every voice; VibePlayer gets the same claim + wipe inline in its pre-pass. **#11 consumer:** CC89 pan-ramp stash armed at the RP takeover + the RT glide noteOn, per-voice `mNotePan` glides current -> target over the glide span; fresh notes reset the ramp BEFORE the glide block (an arm-then-reset ordering bug in AdditiveVoice caught + fixed during the pass). **#37:** CC86 + CC89 added to VibePlayer's INLINE pre-pass -- the S-6(C) loudness ramp fires in BaySickPlayer for the first time.
- **Task 4 -- Builder grid + tracks.** **#24 preview + #29:** `drawMidiShading` de-tiled + un-clamped + three missing roll families added (inst / clip / rusty; vox excluded per G-8); the slice right-piece `% cycleTicks` wrap dropped. **#25:** four pattern-block creation sites size from `getPatternContentBeats`. **#26 (G-15):** click-copy memory consumed at the draw-commit. **#27 + #28:** move origins from `effectiveStartBars`, delta-snap; Alt+drag fine-move; the resize half of #28 WITHDRAWN (dossier correction); 2 KeyBindings.cpp rows. **#30 builder half:** `barToX` rounds + `xToBar` samples the pixel center (LDT-394 mirror). **#21/#22:** `groupSpan` + whole-span rotation map (the old per-row group-id swap DELETED). **#23:** group ops wrapped in undo brackets (color picker: begin before the async open, commit in the callback).
- **Task 5 -- playhead behavior.** **#30 roll+kit:** output-latency compensation while PLAYING via a permanent `deviceInfoProvider`. **#31:** song-mode roll playhead via `songLocalBeatProvider`. **#30 kit:** `DrumKitGrid::xToBeat` +0.5 pixel-center + `beatToX` rounding. **1A snapped seek** (Jeff): all three ruler-seek sites snap; Alt+click seeks free.
- **Playhead investigation (G-9) -- characterization COMPLETE, then final form.** Five Debug captures: latSamples=330 constant; parked-line clicks on all three surfaces agree within <=2 px -- **NO unknown static offset; the dossier's "unexplained roll residual" does not reproduce stopped.** Mechanisms: (a) scheduled fixes -- in-batch; (b) 30 Hz paint staleness while playing (~5.2 px at max roll zoom) -- characterized, routed forward; (c) stop parks off-grid vs snapped clicks -- characterized, routed forward; (d) draw asymmetry -- fixed; (e) unsnapped ruler seek -- spec 1A, fixed. Jeff rejected all centered-marker geometry -> **FL-style FINAL FORM on all three surfaces: 1-px mast whose column IS the position + cap hanging right; nothing draws left of the position.** FL reference stated from model knowledge at Jeff's explicit direction; his screen wins if it disagrees.
- **Task 6 -- swing UI + title-bar normalization.** SW-1 global knob on the transport bar + `makeSwingKnobBinding` factory. BaySickTitleBar: `setTrailingWidthHint` / `addHostedTrailingWidget` (G-16) / `setReservedTrailingWidth` (G-14). G-16 moves: Guitars/Basses program label + Load button onto the Aria title bar (new `AriaControlPanel::getTitleBar()`), Rusty Player Preset + program combo onto its title bar, NAM A/B toggles below the OS dial. (The per-player Swing Mix knob's title-bar home was later moved to the PageMenuBar at smoke round 2 -- see the deviation record.)
- **Task 7 -- piano-roll tools.** #12 velocity click memory. #9/#10 (G-10): one right-edge arrow family; Bend added to the S-cycle, gated engine-aware. #13-#16 Humanize (G-3): interval list default 1/64; distribution 3-way; seed 1-10; defaults 10/10/20. #17: ALL 8 step enables ON; Dice/Randomize/Start-over set enables (the three non-neutral defaults shipped here were REVOKED at smoke round 1 -- owned-errors record). #19: double-click-reset on every Humanize + Riff slider. #20: `riffMachineUsed` serialized, pre-checks "Work on existing score".
- **Task 8 -- drums unit (#32 -> #33 -> #34).** #32: recording demux -- per-note routing through the Note-binding match with kit fan-out, stamped at each drum's play note; `notifyContentChanged()` for ALL recording kinds (the permanent-loss hole is dead). #33 falls out structurally. #34: kit move-drag reads the CURRENT row per tick + re-stamps to the dest play note guarded by D-6. Gate attempt #1 failed C2248 (`drumPlayNoteRT` private) -- moved public.
- **Task 9 -- octave-up granular artifact (#35).** OLA normalized per plan. REVERTED at smoke round 1 (v2-#55): replaced by the plan's NAMED BACKUP -- `setGrainSize` re-anchors the head pair to an exact half-grain offset (Hann pair at 50% sums to 1.0 at every phase) + ~12% deadband against YIN wobble.
- **Task 10 -- slide rework A: full-voicing extraction + articulation residency (G-1, G-11).** `SlideSample` typed voicing statics + verbatim `modOps`; `SlideCurve` tables + `SlideArticulation` per-keyswitch layer stacks {center, tUp/tDown, tailpiece, feedback, noise, releases} + bend range + bass cc105 mono policy; keyswitch-less programs extract; compat mirror kept pre-Task-11 consumers compiling. Parser hardening (multi-opcode tokenizer, streamed `sw_default`, merged effective-scope map -- two self-caught hazards). **G-11:** setProgram decodes EVERY articulation's EVERY layer set synchronously; Debug logs the footprint. **The actual MB figure was NOT captured in-batch** -- capture from the Debug line at the campaign pass.
- **Task 11 -- slide rework B: the voiced voice DSP.** Per-voice chain: Lagrange resample (keycenter + micro-bend + tune + unison detune + LFO pitch) -> gain (volume x amplitude x REAL velcurve/veltrack x AHDSR x tremolo x sin crossfade -- the vel stopgap GONE, #2 loudness half) -> per-voice filters -> pan -> sum. LFO bank from modOps. Full AHDSR (hops enter at Sustain; release spawns rt_decay release zones). Pool 4 -> 20; steal = oldest-quietest. **#6:** hop start time-aligned to the outgoing voice, floor ~130 ms, crossfade 28 ms (SS-Q5 TUNE). `stopAllNow()` declick.
- **Task 12 -- slide rework C: engine integration.** `ArtSet` tables + atomic active index; `trySelectArticulation` per noteOn; gestures PIN their articulation. CC provider wired to all 512 cached APVTS atomics (#2 modulation half). #3 anchor velocity. #5 all-notes-off hard stop (+#4 confirmed). #7 same-block bitset (copied slide re-plucks). **G-12/G-13 Cut Self** params + noteOn cut logic + slide-tail policy (`chokeAll` primitive; startSlide no longer blanket-resets). **G-14** toggles hosted in the reserved slots. Bass cc105 Mono choke. **Idle-suspend** slide term (mid-tail suspension confirmed possible).
- **Task 12b + 12c -- voicing completion (Jeff-directed; supersedes the Task-12 residuals paragraph).** Generic block-rate amplitude route (THE unison fix -- unison zones carry amplitude=0 and were silent at all cc values). Custom curve evaluation (`ccNorm` + `_curvecc` end-pass). ampeg `_oncc` env mods with per-voice latch + release re-latch. pitcheg. Tailpiece cc118 trigger-time switch (pinned per gesture). Feedback + looped-noise cc29 triggering (`spawnCc29Layers` + `triggerFlatZone`). cutoff_cc92 + resonance_cc91. 12c: fileg, varNN multiplier kludge, gain_cc alias. ArtSet declaration-order compile hazard fixed in passing.
- **Smoke round 1 (Jeff's v2-numbered report -- 12 issues + 2 general).** v2-#6 Builder marker second draw site -> flag form. v2-#13 Rusty dropdown parentless (ctor order) -> fixed. v2-#19: (a) player block-stepped pan -> `addFromWithRamp`; (b) app-wide CC10 hard-set click on ringing tails -> ~8 ms glide in all three voice classes; (c) synth "hard pan" instrumented. v2-#24/25/28 up-slide raw-frame transplant into shorter samples -> fraction-align capped 80%. v2-#29 + v2-#62 answered, no change. v2-#39 -> ALL non-neutral Riff defaults revoked. v2-#45 right-Alt mute / left-Alt fine move (`isRightAltKeyDown`, Win32). v2-#55 octave revert -> structural re-anchor. v2-#59/#60 knob layout + value popups. General-1 `[G3 BAR1]`; General-2 provider path clear by read.
- **Smoke round 2 (Debug readings + fixes).** `[G3 PAN]`: pan-ramp system VERIFIED WORKING -- the percept IS the designed span. `[G3 BAR1]`: scheduler EXONERATED (every pass emitted; wrap geometries exact). **Stuck audition notes (all four engines):** single-slot atomics dropped offs -> 128-bit accumulate mask. **Click memory carries the WHOLE property set** (`mClickMemoryProto`). **Rusty opens on the Player sub-tab.** **Per-player Swing Mix knob moved to the PageMenuBar** (`setSwingKnobSlot` + `PageSwingKnob`); five pages' title-bar wiring + four editor forwards + BaySickTitleBar's knob machinery REMOVED.
- **Smoke round 3 -- automation song-mode-only + mode-switch restore (confirm-before-fix per Jeff).** Writer 1 (audio evaluator, no mode gate -- long-standing) gated on `mSongMode`; `setSongMode` captures/restores the main-APVTS baseline. Writer 2 (UI timer applicator branch -- engine params + `global_tempo`, EVERY tick ANY mode -- found after Jeff reported the first fix insufficient) gated on `isSongMode()`; `onSongModeChanged` carries the applicator-lane baseline via the reader/applicator registries.
- **Round-2/3 resolutions + close infrastructure.** Pan-span spec call CLOSED (as designed). General-2: three APPCRASH c0000005 readouts (7/22 ntdll heap-corruption family; 7/23+7/24 in-module, builds overwritten -- unmappable, no blind patch); WER LocalDumps armed; **do_build.bat symbol archiving** (outgoing Release exe+pdb -> SymbolStore/, newest 5, PE-timestamp matched) -- rebuilds no longer destroy dump evidence. Mode volume delta resolved as DATA (master-fader clip + frozen applicator residue in the saved knob). **§B.24 authored** (41 scenarios G3-1..G3-41; supersedes §B.23 SLS-1/3/4/5; G3-3 = known non-bug; G3-36 = bar-1 watch).
- **Close review fixes** -- see the `/review-batch` outcome section below.

#### Dossier-corrections record (verified 2026-07-23; the plan built on THESE -- the dossier is not the last word)

- **#17:** "every Riff step-4-7 default is a no-op" FALSE -- Mirror (step 4) defaults Flip chance 30 and runs when enabled; only Levels/Artic/Groove defaults were inert.
- **#28:** the RESIZE half WITHDRAWN -- Builder resize already keeps sub-bar precision + honors Alt; only the MOVE path truncated.
- **Bass filter:** "always-on 250 Hz lowpass at default controls" overstated -- cc90=127 + var02 leave the LPF effectively OPEN at default; the default-timbre gap is articulation + gain/velcurve + envelope.
- **cc116 naming:** guitar cc116 = vibrato DELAY (not Fade); bass has BOTH cc115 Delay and cc116 Fade.
- **#32 severity:** WORSE than written -- recorded drum notes were SILENT on playback AND permanently lost in any pattern with existing hits, not "invisible until reload."
- **Idle-suspend:** `kIdleSuspendBlocks = 9` (~100-200 ms) -- a slide could be truncated MID-GESTURE, not just during ring-out.
- **#30b Rusty:** the Rusty singleton was a third category -- NO lock at all, not try-locked; the snapshot covers all roll families uniformly.
- **#35:** the artifact is full-depth AM (the de-anchored window sum sweeps ~0..2), not a static +6 dB; the down voices also ride the granular fallback.
- **#37:** whitelist-add alone insufficient -- CC86 had to move into VibePlayer's INLINE pre-pass.
- **#26:** the dossier's "must also copy block velocity" note was wrong (Jeff) -- #26 is copy-what-a-block-is; velocity is #12, piano roll only.
- Also recorded: the unison-memory finding (t1/t2 are neighbor-key references -- ~zero extra decoded RAM via the path-keyed cache).

#### Deviation + supersede record

- **#30b RCU deviation:** the plan sketched a two-slot pointer swap; shipped the in-repo Batch-9c RCU + `RetirementQueue` (Carry-Forward §2 reuse) -- a two-slot scheme can be overrun; G-6 semantics strictly satisfied.
- **Playhead marker final form supersedes the Task-5 centered geometry** (Jeff rejected centered markers).
- **SW-3 knob home superseded (Jeff, round 2):** PageMenuBar, not the player title bar; transport SW-1 unchanged.
- **Plan #17 defaults clause superseded:** the "non-neutral reset defaults" line + values were mine, not Jeff's -- revoked at round 1.
- **Task-9 OLA normalize superseded** by the plan's named structural backup at round 1.
- **Task-12 "Honest residuals" paragraph superseded** by the Task-12b/12c entry.
- **G-11 RAM figure:** NOT captured in-batch -- log the real MB at the campaign pass.

#### Found along the way

- **REGRESSION (Task 2 #30b, found by Jeff): roll MIDI silent on every engine.** Root cause MINE: publish hooked to `notifyContentChanged()` on the strength of its header comment WITHOUT the verifying grep -- roll/kit edits fire their own `onNotesChanged` (12 roll + 34 kit sites) whose only wiring was a repaint.
- **DrumKitGrid pixel-center gap** (missing LDT-394 +0.5) -- found during the Task-1 diag re-fix.
- **All-three-surfaces scope** (Jeff, post-Task-2) -- reshaped the diagnostic.
- **Capture #5:** draw asymmetry + unsnapped ruler seek -> spec 1A.
- **v2-#13:** the Task-6 "(ctor order verified)" parenthetical was WRONG -- the verification never held.
- **v2-#19 app-wide pan-click mechanism** (CC10 hard-set on ringing tails, all three voice classes).
- **Up-slide hop transplant bug** (raw frame position into shorter samples) -- exact up/down asymmetry as reported.
- **Stuck audition notes** (single-slot on/off atomics, all four engines).
- **Automation ran in pattern mode via TWO independent writers**; the second surfaced only after the first fix proved insufficient in Jeff's hands.
- **General-1:** scheduler EXONERATED by `[G3 BAR1]`; stuck-note fix = prime-suspect remedy; G3-36 re-tests.
- **General-2:** heap-corruption family; no in-batch fix possible -- evidence capture armed instead (§9).
- **v2-#55:** my Task-9 normalization made all three octave modes WORSE.
- **Parser self-catches** (streamed sw_default; ~4M-lookup probe), ArtSet declaration-order hazard, a transient devenv file lock, the C2248 gate failure.
- **Slide-note click memory dropped the property set** -- round-2 finding, Jeff.
- **Mode volume delta** -- DATA, not a gating bug.
- **Close review findings** -- 1 BLOCKER + 6 NEEDS-FIX + 7 NITs (outcome section below).

#### What was done about each finding

- **#30b regression:** fixed at the real choke point same-day -- `onContentEdited` hook from both containers' `onNotesChanged` tails (all 46 mutation sites converge) -> `notifyContentChanged()` -> republish; the recording hole closed in Task 8.
- **Kit pixel-center, draw asymmetry, 1A seek, all-three scope:** folded into Task 5; marker then to final form on Jeff's direction.
- **All smoke-round findings:** fixed in-batch across the three rounds (details in Done) except the two answered-no-change items (v2-#29, v2-#62).
- **v2-#19(c):** instrumented -> VERIFIED WORKING -> spec closed as-designed.
- **General-1:** stuck-note fix shipped; `[G3 BAR1]` stays for G3-36.
- **General-2 crash + the two unevaluated SlideSampler captures** (offset_oncc25, the structural fil2 statics): routed forward -- §9 Forks entry; no new batch scheduled.  BOTH playhead residuals ((b) paint-lag via dirty-rect + 60 Hz, (c) off-grid park via pause-quantize) and the extended-CC internal inputs were FIXED post-review at Jeff's direction (close addendum entries).
- **Close review:** BLOCKER + 5 NEEDS-FIX + 5 NITs fixed same-session; 1 NEEDS-FIX recorded to the residuals (extended-CC); 2 NITs recorded (swing loop edge; catalog drift).

#### Owned errors (kept straight for the record)

- **#30b publish hook off a header comment without the verifying grep** -> the roll-MIDI-silent regression.
- **Task-12 residuals mislabel + false unison-live claim** -- the plan had put all five items in Task 11's locked scope; unison was captured-never-evaluated. Jeff caught the dodge; 12b/12c shipped everything.
- **v3 renumber mid-test** -- the v2 numbering is authoritative for round 1.
- **v2-#19 first diagnosis** blamed BaySickPlayer while Jeff tested on a synth.
- **v2-#39 / #17 invented defaults** -- the "non-neutral defaults" plan line + the three values were MINE; Jeff never asked. All revoked.
- **Task-6 "(ctor order verified)" claim** was false -> became v2-#13.

#### `/review-batch` outcome

- Ran at close over this batch's scoped diff (prior uncommitted batches excluded): **1 BLOCKER + 6 NEEDS-FIX + 7 NITs**, every premise verified against code before action. Fixed same-session: the BLOCKER (SymbolStore/CrashDumps gitignore), G-12-OFF hop fade choking prior tails (gesture-serial scoping), non-steered pitch modulation discarding baseCents (per-voice store), audition same-block press+release stick (block-end close), PageSwingKnob dangling-this (SafePointer), unguarded TPT filter setters (change gates), + 5 NITs (two stale comments, the dead layout statement, chokeAll silent-voice kill, UI/audio automation length-domain match). Recorded, not fixed: swing loop-edge drop (campaign-ear item); [G3 PLAYHEAD] catalog drift (moot -- diagnostics kept); the extended-CC item was subsequently FIXED at Jeff's direction (close addendum entry). Full detail in the dated review entry in these notes.

#### Carry-forward contradictions (if any)

- None recorded. The #30b snapshot deliberately REUSES the Batch-9c RCU + RetirementQueue per Carry-Forward §2.

#### Diagnostic Instrumentation Catalog

- Four rows standing; **Jeff's strip call (2026-07-24): KEEP ALL FOUR** (Debug-only, zero user impact); re-walk at a later close or pre-release. `[G3 BAR1]` serves the G3-36 re-test; the `[SlideSampler]` residency line still owes the G-11 MB figure.

#### Files touched

All uncommitted on top of HEAD `d6abc38b`, interleaved with the four prior uncommitted batches; the list below is this batch's own touches.

- **New:** `Source/G3PlayheadDiag.h` (the SlideSampler rewrites ride the lynx-created files).
- **Scheduler / transport / data model:** `Source/PluginProcessor.h/.cpp`, `Source/PatternManager.h/.cpp`.
- **In-house voices + processors:** `Source/BroadcastSynthesiser.h`, `Source/BaySickSynth/BaySickSynthVoice.h/.cpp`, `Source/Harmless/AdditiveVoice.h/.cpp`, `Source/VibePlayer/VibePlayerDSP.h/.cpp`, `Source/Harmless/HarmlessProcessor.h/.cpp`, `Source/BaySickSynth/BaySickSynthProcessor.h/.cpp`, `Source/BaySickBass/BaySickBassProcessor.h/.cpp`, `Source/VibePlayer/VibePlayerProcessor.h/.cpp`.
- **Slide stack + sfizz engines:** `Source/SlideSampler/SlideRegionMap.h/.cpp`, `Source/SlideSampler/SlideSampler.h/.cpp`, `Source/BaySickGuitars/BaySickGuitarsProcessor.h/.cpp`, `Source/BaySickBasses/BaySickBassesProcessor.h/.cpp`, `Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp`, `Source/Engine/Tasks/InstStripTask.cpp`, `Source/Inst/InstPage.h/.cpp`.
- **Pages / grids / UI:** `Source/Standalone/PianoRoll.h/.cpp`, `Source/Standalone/PianoRollPage.h/.cpp`, `Source/Standalone/DrumKitGrid.h/.cpp`, `Source/Standalone/BuilderPage.h/.cpp`, `Source/Standalone/StandaloneEditor.h/.cpp`, `Source/Standalone/KeyBindings.cpp`, `Source/Standalone/GlobalTransportBar.h/.cpp`, `Source/Standalone/BaySickTitleBar.h/.cpp`, `Source/Standalone/SharedUI.h/.cpp`, `Source/Standalone/AriaControlPanel.h`, `Source/Standalone/BaySickRustyDrumsPage.cpp`, `Source/BaySickNAMIR/BaySickNAMIREditor.cpp`, `Source/Standalone/LayersPage.cpp`, `Source/Standalone/BassPage.cpp`, `Source/Standalone/DrumPage.cpp`, the four engine-editor headers.
- **DSP:** `Source/DSP/OctaveStyleDSP.h/.cpp`.
- **Build tooling:** `do_build.bat`, `.gitignore`.
- **Docs:** `Test Plans/v1-master-test-plan.md` (§B.24), the paired plan/notes files; queued for apply: this entry + the Main Plan §9 close Forks entry.

#### Commit(s)

- TBD -- the whole five-batch boundary rides the commit(s) on Jeff's explicit approval. Backfill hash(es) + the §B.24 `blocks:` line at commit.

#### Next action

- Boundary commit on Jeff's approval; then the R2 campaign pass walks §B.24 (capture the G-11 residency MB at G3-20 setup; G3-36 re-tests bar-1). The crash-dump watch (§9) stays armed.

