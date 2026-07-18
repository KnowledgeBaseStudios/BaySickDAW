# Running Notes — QA-H (ghostly-riffing-moth)

> Append-only mid-batch log. New entry at every checkpoint (commit landed / sub-task
> verified / finding captured / spec call resolved / scope pivot). Consumed at doc-close
> (section pass under bulk-run R2) as the primary input for the held Work Log entry.

Pair file: `Plans & Specs/Batch Plans/ghostly-riffing-moth.md`
Convention: Main Plan §0 "Batch Plans + Running Notes layout" (exemplar
`federated-bouncing-cupcake.md`).

## 2026-07-17 — Group open (G3)

Seeded at G3 group approval. All spec calls pre-locked (G3 docket rounds, 2026-07-17):
note-type button + S, Note Properties system (7 fields incl. new Release + Resonance,
slide/porta made audible per engine), Humanize + Randomize + Riff Machine FL replicas
(references captured in-chat: Jeff's Humanize screenshot + both manual pulls), Ctrl+drag
selected-only lane scrub, ghost producer (all tabs, tinted), pitch-row select, lane guides
+ header-label bug, folded Builder fixes (#6 mute/length from Jeff's live repro, #17
teardown, #19 re-drop, #20 active drop type). BUILD-06 verified moot at scout (campaign
§C retest stands). Coding starts after QA-G.

## 2026-07-17 — Task 1 — Note-type button + S key (#4i/#4ii)

Armed-note-type TextButton added between Select and Zoom on the piano-roll toolbar
("Standard"/"Slide"/"Porta"; grey at Standard, toggle-lit on Slide/Porta — same lit
rendering as the active tool). Click cycles. S cycles the SAME state (grid owns it:
`cycleNewNoteType()` + `onNoteTypeArmChanged` mirror callback -> container
`refreshNoteTypeButton()`); with a selection S ALSO converts the selected notes to the
newly armed type as ONE undoable "Change Type" edit (any-change guard so an
already-matching selection registers no junk undo entry). Supersedes the old either/or
semantics (per-note cycle with selection / silent arm without) — the silent-arm
invisibility is dead; every press visibly updates the button. "Sel" tool label ->
"Select". Gesture-map S entry (KeyBindings.cpp:367-369) rewritten to the new semantics.
In-region string fix: Zoom tool tooltip claimed "(Shift+Z)" but the binding is bare Z
(B-4 rebind) — corrected.

Observation (untouched; plan scope = PianoRoll): DrumKitGrid's kit toolbar still shows
"Sel" (DrumKitGrid.cpp:3136). Drums carry no note types so no armed button belongs
there; whether that label also becomes "Select" is Jeff's consistency call at routing.

Build-confirm: Jeff "Clean" (Release+Debug), 2026-07-17.

## 2026-07-17 — Task 2 — Note Properties system (#3, D=B) + slide/porta made real

**Spec call RESOLVED mid-task (Jeff, 2026-07-17): BOTH slide semantics ship as separate
note types.** Posed ramp-vs-retrigger for the slide's attack behavior; Jeff: ship both,
named Ramp Slide + Retrigger Slide, toolbar/selector labels "RP Slide" / "RT Slide".
Follow-up rename (same sitting): Standard displays as "Flat" on the armed button + the
properties popup (internal enum name unchanged).

**Data:** PianoNote += `releaseAmt` + `resonance` (0.5 neutral; sparse-serialized "r"/"q").
NoteType = { Standard=0, RampSlide=1, Portamento=2, RetrigSlide=3 } — RetrigSlide APPENDS
so prior saved Slide notes (1) load as RampSlide; no migration needed.

**Popup:** double-left-click a note (Draw/Select tools) -> Note Properties CallOutBox
(NotePropsPanel, PianoRoll.cpp): Flat/RP Slide/RT Slide/Porta selector + Velocity, Release,
Fine Pitch, Panning, Filter Cutoff, Resonance sliders. Clicked note in selection -> edits
apply to the whole group-expanded selection; live-apply; lazy beginEdit on first change +
commitEdit at dismissal = ONE undo step, untouched popup = no undo entry. Double-click that
CREATED a note is suppressed (bool guard reset on single-click mouseDown — index-free, so
sortNotes reordering can't break it). S-cycle + armed button now run Flat -> RP Slide ->
RT Slide -> Porta; RT notes draw the slide triangle as OUTLINE (RP filled); gesture map
(KeyBindings.cpp) updated incl. the new double-click row.

**Emit path (PluginProcessor):** all five per-note expression values emit UNCONDITIONALLY
(CC10 pan / PitchWheel / CC74 cutoff / CC71 resonance / CC72 release).
**FINDING (real pre-existing bug, fixed): skip-at-neutral emits let a neutral note
silently INHERIT the previous note's pan/bend/cutoff channel state.** Flip side is the
already-documented channel-wide "last note wins": a held note's channel values now reset
when a neutral note starts (logged at the emit site). Glide transport: CC84 source note +
CC5/CC37 14-bit glide-time ms; CC85 = ramp-bend target.

**Scheduler:** RT Slide = noteOn + CC84(nearest preceding note) + time(=note length,
TempoMap-exact ms). Porta = CC84 only (engine glide time). RP Slide = NO noteOn: anchor
walk-back (first non-ramp note through connected predecessors; broken chain = silent) ->
emitRampSlide CCs; the anchor's noteOff is extended at ITS schedule time through the
connected ramp chain (rampChainDurationBeats; clamped by offHi so sliced/tiled windows
stay bounded). Chained ramps work (the juce voice keeps the anchor note number, which is
also what the extended off matches).

**Engines (BaySickSynth+Bass shared voice / Harmless AdditiveVoice / VibePlayer VibeVoice):**
CC71 res offset + CC72 release scale as pending->active-at-startNote stashes (a sounding
voice keeps its own values when the next note's CCs land); release scale applied over
stored base ADSR params (setters are change-guarded, so voices re-apply per note);
per-note glide: ratio glide (synth), tau=t/3 coefficient glide w/ in-loop land-check
(Harmless), 64-sample-chunk resample-ratio ramp (player). Porta fallback 60 ms where the
engine glide param is 0/absent (player always) — calibration under the plan's per-engine
latitude, campaign-tunable. CC85 ramp handler bends the voice whose juce playing note ==
CC84 anchor; EVERY voice clears the glide stash at CC85 (no leak into later noteOns).

**FINDING (real pre-existing bug, fixed): juce::Synthesiser::handleController is
channel-gated — idle/cleared voices (channel 0) never receive CCs, so the CC-before-noteOn
stash missed whenever the next noteOn allocated a cold voice.** Batch E's CC74 cutoff
shipped with this hole (worked only when allocation reused a warm voice). Fix: new
`Source/BroadcastSynthesiser.h` (broadcasts controllerMoved to every voice; pitch wheel
keeps stock startVoice replay) — HarmlessSynth.mSynth + BaySickSynthDSP.mSynth swapped
(the :427 const_cast retyped to match).

**FINDING (real pre-existing bug, fixed): VibeSynth dispatches noteOns SYNCHRONOUSLY in
its event loop but deferred CCs to filteredMidi — delivered AFTER every noteOn in the
block, i.e. per-note CCs arrived one note late on the player** (same Batch E family).
Fix: expression CCs (5/37/71/72/74/84/85) broadcast inline in buffer order and consumed.

**Known seams (logged for campaign):** sfizz-family rolls (Inst/Rusty/Vox) ignore the
glide CCs + per-note consume (plan scopes the 4 engine families); BaySickSynth Legato mode
dispatches noteOns in preprocess so a fresh legato voice's CCs lag one block; Harmless
strum staggers chord noteOns after the chord's CCs (mixed per-chord-note expression under
strum = last-emitted wins); poly chords under one ramp slide: every overlapping note's off
extends, only the anchor-matched voice bends (mono-lead is the use case); ramp slide whose
anchor is masked out by a slice/tile window = silent (graceful).

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-17.

## 2026-07-17 — Task 3 — Humanize (FL replica, #1)

Tools menu gains "Humanize..." (id 67) -> HumanizePanel CallOutBox (PianoRoll.cpp, before
toolQuantize). Controls per the captured screenshot spec: Start Time / Duration / Velocity
Range+Offset knob pairs (rotary + value box), Distribution combo (Quasi-Normal), Start
Time Max Interval combo (unified snap divisions 2..10, default Step), Seed (IncDec
arrows), Preview toggle (default ON, live-applies), Reset / Regenerate / Accept.
Selection-or-all (group-expanded); seeded + reproducible (one rng, sorted-target draw
order); quasi-normal = mean of three uniforms. Calibrations (logged, campaign-checked
against FL side-by-side): start-time random is LATE-biased 0..range over the interval
(offset knob bipolar re-centers), duration + velocity bipolar around original; defaults
Start 20% / Dur 0% / Vel 10%; mutates startBeat/durationBeats only (tick fields derive at
save per QA-Ee, same as every existing tool). Accept = restore-original -> beginEdit ->
apply -> commitEdit (ONE undo step, true before/after pair); dismiss/Esc = restore, no
undo entry. Preview mutates live without onNotesChanged until Accept (popup precedent).

Build-confirm: Jeff "Clean" (Release+Debug), 2026-07-17.

## 2026-07-17 — Task 4 — Randomize rebuilt (FL replica, #1=B)

**!! Tool-replacement line (loud, per the option-removal rule): the old instant
Randomize (selection-only velocity +-0.2 / start +-half-snap jitter, no dialog) is
REMOVED, replaced by the FL-replica dialog** — Jeff's own docket call (#1=B). Alt+R +
Tools>Randomize now open RandomizePanel (CallOutBox). Pattern section (enable default ON
per FL; Octave 1-7 / Range 1-4 oct / Key / Scale (roll's kScaleDefs) / Length in fixed
1/16 steps + Variation % / Population % at the current snap grid over the roll span /
Stack % extra chord notes (max +2, dup-guarded) / Random Portamento % -> NoteType::
Portamento / Merge Same Notes / own Seed w/ arrows). Levels section (six bipolar
-100..+100% wheels velocity/pan/fine-pitch/release/cutoff(MODX)/resonance(MODY) per
D=B; Reset Before Processing; Bipolar toggle default ON (off = one-directional); own
Seed). Pattern ON = generate replaces the roll, Levels applies to generated; OFF =
Levels on selection-or-all (group-expanded). Live preview always (FL model — no preview
toggle); full-roll snapshot restore on cancel; Accept = ONE "Randomize" undo step +
clearSelection. Calibrations logged: wheel delta = rnd * wheel% * field-range (clamped);
6 draws per note in fixed order (stable per-wheel streams); stack limit 2; pitch pool =
inKey degrees offset from key over octave..octave+range (base C4=60 convention).
Note-vector replacement during live playback = same message-thread-mutation model every
existing add/delete path uses (no new seam class).

Build-confirm: Jeff "Clean" (Release+Debug), 2026-07-17.

## 2026-07-17 — Task 5 — Riff Machine (FL replica, #2=A) + D-6 Alt+E

Tools menu "Riff Machine..." + Alt+E (D-6) -> RiffMachinePanel (CallOutBox): 8 step-tab
pages, per-step enable + Reset + Random. (1) Progression: 8 authored starter presets
(degree sequences incl. seeded Random walk) + chord rate Bar/Half/Beat; (2) Chords:
8 degree-stack presets (100+k sentinel = +octave degrees, scale-size-agnostic);
(3) Arp: 8 patterns, Mode Normal/Flip/Alternate, Sync Time(=snap)/Block(=beat)/
Chord(=segment/m), Gate % (notes at current snap length per plan via Sync=Time);
(4) Mirror: chance % reflect around mean pitch; (5) Levels: PAN/VEL/REL/MODX/MODY/PITCH
wheels + Bipolar + the ONE master Seed (all stages derive per-stage streams: seed*8+k —
fully reproducible); (6) Articulation: 6 presets; (7) Groove: 6 presets (snap-grid
odd-step swing shifts + push/laid-back); (8) Fit: Key/Scale + min/max range octave-fold
(auto-widened to >= 1 octave) + snap-to-scale (nearest in-key, down-preferred).
Globals: Preview-to-step (stages past it skipped), Work on existing score (stages 4-8
transform the roll's notes; 1-3 skipped), Length bars (default = roll bars), Start over,
Dice (randomize all steps + new seed; step 8's Random deliberately no-ops — key/scale/
range is the user's musical frame), Accept. Degree resolution always uses Fit's
key/scale (degrees need a basis; Fit-enable gates only fold/snap). Live preview +
full-roll snapshot restore on cancel; Accept = ONE "Riff Machine" undo step +
clearSelection. Preset content authored in-code (plan's "or in-file" option). Step
enables default 1/2/3/8 ON, 4/5/6/7 OFF (logged calibration). Gesture map: Alt+E row
added; Alt+R row REWRITTEN (it still described Task 4's removed instant-jitter).
Caught in-edit: the in-class static constexpr preset arrays initially had out-of-class
definitions — the documented MSVC C++17 gotcha — removed before the build.

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-17.

## 2026-07-17 — Task 7 — Ghost producer (4b) + pitch-row select (MIDI-01)

**Ghosts live (setGhostData's first producer):** PianoRollPage::timerCallback iterates
mConns, skips mActive, collects {dataAccessor(), noteColor} for non-empty rolls, sorts by
pointer (unordered_map order jitter) and pushes to the active container behind a
change-guard (mLastGhosts) since the grid repaints on every push. Runs before the
playhead early-return (works with no playhead). Live closures track pattern switches;
the render reads note vectors at paint time so source-roll edits appear on the next
repaint. View > Ghost Notes toggle (id 56) now gates real content via the container's
existing store/re-forward. Note: drum-roll ghosts draw via the active roll's
displayMidiForNote (melodic rolls show them at their raw pitches, i.e. C5 pile for
slot-tagged hits — display-convention consistent, logged).

**MIDI-01:** PianoKeyboard Ctrl+click -> onCtrlClickPitch -> grid selectAllAtPitch
(additive, deduped; no audition fires on the Ctrl+click; plain click/preview unchanged).
Gesture map row added.

Build-confirm: Jeff "Clean" (Release+Debug), 2026-07-17.

## 2026-07-18 — Task 8 — Folded Builder fixes (#6 + #17 + #19 + #20)

**#6 (real bug, Jeff's repro):** song-end derivation (StandaloneEditor getLoopLengthBeats
song branch) skipped muted blocks — a muted block SHORTENED the song. Skip removed:
mute silences (scheduler still skips muted for audio) but always counts toward
song/loop length.
**#17 (shutdown UAF):** BrowserPanel had no dtor; mAudioTree declared BEFORE mAudioRoot,
so reverse-order destruction killed the root while the TreeView still pointed at it.
New inline dtor: setRootItem(nullptr) first.
**#19:** placeAudioLibraryEntry's missing-file silent return -> async "Audio File Not
Found" AlertWindow with the path + restore/remove guidance. FINDING (absorption): the
"deletion rebuilds the tree" half was ALREADY SHIPPED — the audio Remove path calls
rebuildAudioRows() right after removeAudioFromLibraryAt (QA-Fe2 browser-groups era);
verified, no change needed.
**#20 (active drop type):** ArrangementGrid gains BrowserDropKind {Pattern/Audio/
Automation} + per-kind ref (Pattern rides mBrowserSelection). Armed by: pattern-row
click (selectPattern), audio-leaf click (new AudioBrowserItem::onSelected — single
click previously did nothing), automation-row click (new selectAutomationItem —
automation rows were DRAG-ONLY, now click-select + highlight), and tab switches
(re-arm with that tab's remembered pick, -1 = none). Empty-grid Draw places the armed
kind: pattern (unchanged path incl. QA-G auto-marker hook), audio via
placeAudioLibraryEntry (file-length clip; also gets the #19 dialog), automation from
the clicked template at the DRAWN length. Builder Draw-tool gesture-map row updated.

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-18.

## 2026-07-18 — QA-H CODE-COMPLETE

Tasks 1-8 shipped, every gate build-confirmed clean. §B.14 authored (14 scenarios;
`blocks:` hash backfills at the next docs commit per the B.12/B.13 precedent). Work Log
entry drafted + HELD below. ONE batch commit surfaced for approval (carries the two
QA-G doc stragglers per plan).

## Held Work Log entry

> HELD per bulk-run R2 — apply to `Implemented Work Log.md` verbatim when §B.14 passes.

### 2026-07-18 01:30 PT — QA-H — Piano-roll note types + properties + FL tool replicas + Builder fixes

**Bucket:** Players, System Pages, UI / L&F / Theming

#### Done

- **Task 1 — armed note-type button + S (#4i/#4ii):** toolbar button between Select and
  Zoom mirrors the grid's armed type; click + S cycle it; S with a selection also
  converts (one undo, any-change guarded); "Sel" -> "Select"; silent-arm invisibility
  dead.
- **Task 2 — Note Properties system (#3, D=B) + slide/porta made real:** PianoNote +=
  releaseAmt + resonance (serialized "r"/"q"); double-click popup (type + 6 properties,
  multi-select applies to all, live edits, one undo per session); **owner call
  mid-task: BOTH slide semantics ship** — Ramp Slide (FL takeover: no re-attack, the
  anchor bends over the slide's length, noteOff extended through connected chains,
  broken chain = silent) and Retrigger Slide (own attack, glides in from the previous
  pitch over the note) — labels "RP Slide"/"RT Slide", Standard displays as **"Flat"**;
  Porta = retrigger + engine-glide (60 ms fallback). Per-engine implementations on
  BaySickSynth+Bass / Harmless / BaySickPlayer via a CC transport (CC84 source, CC5/37
  time, CC85 ramp target, CC71/72 resonance/release as pending->active-at-startNote
  stashes). **Three real transport bugs fixed in-batch:** skip-at-neutral emits let
  neutral notes inherit the previous note's pan/bend/cutoff (all five expression values
  now emit unconditionally); juce's channel-gated handleController never reached cold
  voices (new `Source/BroadcastSynthesiser.h` broadcasts — Harmless + Synth/Bass);
  VibeSynth delivered CCs one note late (deferred while noteOns dispatched
  synchronously — expression CCs now broadcast inline in order).
- **Task 3 — Humanize (FL replica):** Start/Duration/Velocity Range+Offset pairs,
  Quasi-Normal distribution, Start Time Max Interval (snap list), seed + regenerate,
  live Preview, Reset/Accept; selection-or-all; late-biased start randomness; one undo.
- **Task 4 — Randomize (FL replica; REPLACES the old instant jitter — loud):** Pattern
  generator (octave/range/key/scale/length/variation/population/stack/random-porta/
  merge/seed) + Levels wheels (vel/pan/fine/release/cutoff/resonance, reset-before,
  bipolar, seed); live preview; Alt+R.
- **Task 5 — Riff Machine (FL replica, D-6 Alt+E):** 8 step pages (Progression/Chords/
  Arp/Mirror/Levels/Articulation/Groove/Fit) w/ per-step enable+Reset+Random, authored
  starter preset sets, preview-to-step, Dice, Work-on-existing, Length, one master seed
  (per-stage derived streams); arps land at snap length.
- **Task 6 — lane scrub (#5/MIDI-02):** Ctrl+drag sweeps SELECTED dots (Y-path
  interpolated); 25/50/75% guides + labels in unipolar modes; **fixed: Filter Cutoff
  lane header read "Pitch Bend"** (3-entry kModeNames fall-through).
- **Task 7 — ghost producer (4b) + pitch-row select (MIDI-01):** PianoRollPage feeds
  the active roll all other rolls tinted per source tab (change-guarded timer push;
  View toggle now real); Ctrl+click a piano key selects that pitch row (additive).
- **Task 8 — Builder folds:** **#6** muted blocks count toward song length (mute != 
  shorter song; playback still silent); **#17** BrowserPanel dtor drops the tree root
  before member teardown (shutdown UAF); **#19** missing-file re-drop shows "Audio File
  Not Found" + path (the delete-rebuilds-tree half verified ALREADY SHIPPED by QA-Fe2);
  **#20** browser clicks arm the grid's drop type (pattern/audio/automation — automation
  rows gained click-select; tab switches re-arm) and empty-grid Draw places the armed
  type (automation at drawn length).

#### Found along the way (all fixed in-batch; none deferred)

Expression-CC stale-bleed; cold-voice CC delivery hole (Batch E's CC74 shipped with
it); VibeSynth CC ordering inversion (player CCs one note late); Filter Cutoff lane
header; #6 muted-length; #17 shutdown UAF; #19 silent return; stale Zoom tooltip
"(Shift+Z)"; stale Alt+R gesture text; #19's rebuild half found already shipped
(absorption).

#### Known seams (campaign-visible, logged in running notes)

sfizz-family rolls (Inst/Rusty/Vox) ignore glide + per-note consume CCs; BaySickSynth
Legato-mode first-note CC lag; Harmless strum staggers chord noteOns after the chord's
CCs; poly chords under one ramp slide extend every overlapping off but bend only the
anchor voice; ramp slides whose anchor is masked by a slice/tile window are silent;
porta 60 ms fallback + Humanize/Randomize/Riff parity calibrations tuned at campaign
A/B vs FL.

**Verification:** bulk-run R2 — campaign section §B.14 (14 scenarios). Build-confirmed
clean (Release+Debug) at every task gate, 2026-07-17/18.

## 2026-07-17 — Task 6 — Control lane scrub (#5/MIDI-02) + guides + header bug

**Ctrl+drag scrub** (docket 5i=a/5ii=a): sweep sets each SELECTED note's dot from the
cursor's Y path as it passes the dot's X (per-segment linear interpolation between drag
points so fast sweeps land correct values); selected-only per the lock — no selection =
no-op AND no edit opened (guard before beginEdit, so no junk undo entries); ONE
"Scrub Lane Values" undo per sweep (beginEdit at gesture start / commit at mouseUp);
plain single-dot drag + Alt+Wheel unchanged. **Guides:** unipolar modes (Velocity /
Filter Cutoff) draw 25/50/75% horizontal reference lines + right-edge labels (50%
brighter); bipolar centre line untouched. **FIXED (real bug): Filter Cutoff lane header
read "Control > Pitch Bend"** — kModeNames had 3 entries and the mode index fell
through; now 4 entries + explicit index. Gesture map: new Ctrl+Drag scrub row; Alt+Wheel
lane row's mode list gained filter-cutoff (stale since Batch E exposed the mode).

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-17.
