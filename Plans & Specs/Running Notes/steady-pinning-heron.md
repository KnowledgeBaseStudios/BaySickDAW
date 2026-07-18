# Running Notes — QA-G (steady-pinning-heron)

> Append-only mid-batch log. New entry at every checkpoint (commit landed / sub-task
> verified / finding captured / spec call resolved / scope pivot). Consumed at doc-close
> (section pass under bulk-run R2) as the primary input for the held Work Log entry.

Pair file: `Plans & Specs/Batch Plans/steady-pinning-heron.md`
Convention: Main Plan §0 "Batch Plans + Running Notes layout" (exemplar
`federated-bouncing-cupcake.md`).

## 2026-07-17 — Group open (G3)

Seeded at G3 group approval. All spec calls pre-locked in the plan file's tables (G3 docket
rounds, 2026-07-17). Scope at open: ruler pin, zoom alignment, track groups/colors (saved),
note-preview true positions, pattern-block slice w/ content-offset model, full
time-signature system (markers sole driver; pattern-TS -> linked auto-markers B1a/B2a).
The TS option-semantics change carries the loud paper-trail line in the plan file; the
running-notes line lands when the code does. Coding starts Task 1.

## 2026-07-17 — Task 1 — Ruler pin (BUILD-02)

Shipped via the preserve-mappings path the plan sanctioned: the ruler stays part of the
scrolling grid surface but counter-translates by the parent Viewport's vertical scroll
(`ArrangementGrid::rulerPinY()`, BuilderPage.h/.cpp). All ruler drawing (band bg, ticks,
bar labels, perf pulse, time-sel band, marker pennants, TS pills, tempo pills, playhead
arrow) offsets by the pin; all six ruler hit zones follow (blockAtPos guard, getTooltip,
itemDragMove/itemDropped ghost rejection, mouseDown ruler branch, mouseDoubleClick).
Time-selection body wash hoisted from drawRuler into paint() so the pin offset never
displaces a content-space fill. drawBlocks cull bound updated to the pinned band's bottom.
Row/block geometry (rowToY/yToRow/drawGrid/resized/Alt-zoom anchor) untouched; horizontal
behavior untouched. No viewport/component restructure — JUCE repaints the moved content
fully on scroll, so the pin recomputes per paint with zero sync plumbing (the
TrackHeaderPanel timer sync stays as-is). Build-confirm: Jeff "Clean" (Release+Debug),
2026-07-17.

## 2026-07-17 — Task 2 — Zoom alignment (BUILD-03)

Block right edges now derive from the same barToX mapping that draws the grid lines:
`width = barToX(start+len) - barToX(start)` (1px gutter preserved) replacing the
independently-truncated `(int)(len * mPPBar)` at 12 sites — drawBlocks, blockAtPos,
nearRightEdge, hitTestAutomPoint, drawPreviewBlock, drawGhostClip,
drawPerformanceOverlays, finaliseMarquee, the Draw-tool automation rect, both automation
drag rects (point + curve-handle), and the double-click curve-handle rect. Three of those
(finaliseMarquee + the two automation drag rects) were siblings the group-open scout list
missed — caught by a fresh `* mPPBar` sweep; each keeps its own existing start-base
(startBar vs effectiveStartBars, raw vs effective length), width-derivation only, so no
behavior change beyond alignment. Toolbar +/- zoom centre anchor now all-float end to end:
new `ArrangementGrid::xToBarF(float)` used by both the toolbar onZoom lambda and
BuilderPage::doZoom (the `(int)centreGridX` truncation creep is gone). Remaining
`* mPPBar` sites verified intentional (barToX itself + resized() component width).
Build-confirm: Jeff "clean" (Release+Debug), 2026-07-17.

## 2026-07-17 — Task 3 — Track right-click additions (IN PROGRESS; findings + one spec call)

**FINDING (real bug, fixed in-batch): Builder track renames never persisted.** Full-tree
grep confirmed zero serialization sites for the grid's `mRowNames` — rename a track, save,
reload, name gone. Fix: row names moved into PatternManager (single source of truth next
to row mute/solo), serialized as sparse `<Row i name group groupColor>` children of the
existing RowState node (child elements, not CSV, so names may contain any character);
ArrangementGrid's array deleted, grid reads/writes through PM (`getRowNames` pass-through,
`setRowName` delegate, undo snapshot paths via PM, static_assert pins kNumRows ==
kMaxArrangementRows). Defaults re-fill on reset() + fromValueTree so stale names/groups
can never leak across project loads.

**FINDING (real bug, fixed in-batch): Move Up/Down left row state behind.** The handler
swapped names + block rows only — mute/solo stayed at the old positions. Now carries
mute/solo/group/color with the track. Also: Move Up/Down + Delete Track Clips had no undo
registration and no arrangement-changed/dirty notification (audio-clip players kept a
stale trackRow; project close lost the edit silently) — both now wrapped in
beginEdit/commitEdit + a new TrackHeaderPanel::notifyArrangementChanged() mirroring
BuilderPage's notify (player rebuild + markDirty). Rename dirties via PM::setRowName.

**Shipped so far:** group state (id 0 = ungrouped, ids 1+) + per-row color in
PatternManager w/ project serialization (#8a); menu items Group with Above / Remove from
Group / Color Group... (enable-gated; Color Group reuses PatternColorPicker::showAsync
with live preview across all rows of the group); header signifier = 3px color band at
x 0..2 (LEDs start at x=4) + 10%-alpha row tint; 8-color default palette cycled by group
id at creation.

**Spec call RESOLVED (Jeff, 2026-07-17): 500 tracks, FL parity — dissolves the overflow
question.** Jeff: the 50-row cap was my arbitrary early constant treated as gospel, never
an intended spec; FL allows 500, so allow 500. SUPERSEDES marathon answer 5's "keep 50
stock rows" (Jeff's own lock, overridden by Jeff in chat). kNumRows 50 -> 500
(BuilderPage.h, with owner-call comment) + kMaxArrangementRows 50 -> 500 (PatternManager.h;
static_assert keeps them locked). New kMaxRowsInView = 50: the Alt-zoom-out floor stays
"at most 50 rows in view" so the zoom range feels identical (500 in view would be ~1.5px
rows); the 500 are reached by scrolling, like FL. Audit swept every row-count consumer:
all Builder/PM uses are clamps/loops that scale; no audio-thread per-block row iteration
(per-player O(1) isRowAudible); DrumKitGrid's kNumRows=16 is unrelated; mixer's 50-audio-
strip cap (kMaxAudioRows / mixer_audio_{0..49}) is a separate surface, untouched. Swept-up
fossil: StandaloneEditor.cpp:2748 local kMaxRows=32 (pre-50-era!) capping the free-row
scan for Clips-page spawn -> now ArrangementGrid::kNumRows. Old projects load fine (50-char
mute/solo strings -> rows 50+ default).

**Task 3 + row-alignment/scrollbar/500-row build-confirm: Jeff "clean" (Release+Debug),
2026-07-17.**

**Insert Track Above wired** (menu id 8, after Move Down): shifts the clicked row +
everything below down one (names / mute / solo / group / color / blocks), new row gets
defaults, undoable (blocks+names via beginEdit snapshot). At the literal row-500 edge
(last row holds anything): silent no-op per the Move Up/Down boundary precedent —
practically unreachable, recorded here as the shipped edge behavior. Name semantics on
insert AND move: positional defaults ("Track N") stay positional; only CUSTOM names travel
with the track (prevents shifted default numbers reading off-by-one below an insert).

## 2026-07-17 — Task 4 — Pattern-block note preview at true musical positions

drawMidiShading rewritten to the owner's G1 spec: note x/w now derive from bar-normalized
positions through the same barToX mapping as the grid lines (noteBar = startBeat /
patternBeatsPerBar; one pattern bar == one grid bar at ANY block length — the old code
stretched fraction-of-pattern over block width). Longer/looped blocks TILE the pattern per
cycle (cycle length = pattern bar count; notes crossing the cycle boundary clamp at it);
short blocks clip at the block edge; per-cycle left-cull via mBarOff keeps huge tiled
blocks cheap. Non-4/4 patterns stop skewing (intrinsic TS drives beats-per-bar; NOTE:
full display-vs-audio agreement for non-4/4 lands with Task 6 when grid markers become
the sole played source — tasks land in one commit so no shipped mismatch).

**FINDING (real bug, fixed in same rework): drum notes missing from previews since
Phase D.** The old shading painted only the LEGACY `pat.drumRoll` (pre-D1 field, migrated
away on load) and never the live per-drum `pat.drumRolls[16]` — pattern blocks showed no
drum content. Preview now aggregates layer + bass + all 16 per-drum rolls + the legacy
field (usually empty).

**Task 4 build-confirm: Jeff "clean" (Release+Debug), 2026-07-17.**

## 2026-07-17 — Mid-batch owner docket (TS follower model + 4 additions) — ALL LOCKED

Chat rounds with Jeff (2026-07-17), all confirmed. Supplements the plan's locked table:

1. **Piano-roll grid <- pattern TS confirmed live** (pages push tsNum/tsDen at load +
   pattern switch; C.5b bar lines). **FINDING: autoDerivePatternTimeSig() is DEAD CODE**
   (zero callers — orphaned in the C.5b revert); marker->pattern derive never fires today.
2. **Follower lifecycle (Task 6 scope):** pattern TS = one of three states. User-set
   (popup; only these spawn linked markers — confirmed) / placed follower (live-follows
   the marker in effect at its EARLIEST block's start bar; re-evaluates on marker edits +
   block moves/deletes) / unplaced follower (see 4). **Reset to Default** button joins the
   pattern TS popup: clears the lock, pattern re-enters the lifecycle as never-set;
   still-linked auto-markers it spawned are REMOVED (manual + unlinked stay).
3. **Move-across-TS prompt:** follower pattern + has notes + move actually changes its
   governing signature -> prompt Proceed (follow new region) / Lock Previous TS (becomes
   user-set at old signature; the move NEVER plants a marker; future fresh placements of
   the now-user-set pattern spawn per B).
4. **Current-TS selection (unplaced patterns):** 0 markers = 4/4; 1 marker = all unplaced
   never-set patterns follow it. At 2+ markers a project-level "current" selection exists:
   add-marker prompt asks which is current; transport pattern-dropdown gains an entry
   (grayed <2 markers) listing markers (bar + sig, tick on current) for manual re-pick.
   Current stamps NEW patterns at creation ONLY — never re-maps existing patterns; each
   never-set pattern keeps its born binding (live to that marker's value edits).
   Current-marker deleted: 2+ remain -> immediate re-prompt; one remains -> silent
   fallback. Non-current marker dies -> its bound patterns re-bind to current.
   **TS suffix**: pattern dropdown (button + list entries) shows "Name N/D" whenever a
   pattern's EFFECTIVE signature != 4/4 (followers included).
5. **NEW TASK — Split by Player Engine** (browser pattern right-click; slots after Task 6):
   one new pattern per TAB ENTRY with MIDI data in the pattern, across Layers/Bass/Drums/
   Clips/Inst; names = "OriginalName - <tab name>"; original destroyed; ONE undo step
   (incl. block replacements + row-group changes). Placed blocks: replaced in place, split
   siblings stacked DIRECTLY below the original row (contiguous); in-the-way rows (whole-
   row non-blank test) shift down insert-style consuming blank rows from the bottom. If
   any original block's row is grouped: ONE prompt per split op — Yes adds each stack's
   new rows to that stack's own local group / No changes no grouping. Capacity: if the
   shift would push content past row 500 -> "maximum tracks reached" prompt + abort
   untouched. **SUPERSEDE (loud): Insert Track Above's silent no-op at the full-grid edge
   becomes the same max-reached prompt** (retrofit lands with the split task).

## 2026-07-17 — Task 5 — Pattern-block slice rework (content-offset model)

`ArrangementBlock::contentOffsetTicks` added (int64 grid ticks, default 0, serialized
skip-when-0 alongside the QA-Ee tick fields; full-snapshot undo + wholesale
clipboard/duplicate copies carry it automatically). Slice tool rewritten: cuts at SNAP
resolution via the existing snapBar path (Alt bypass honored; 1-tick minimum piece guard;
kills the int-bar truncation — 1-bar blocks are now sliceable), pieces get exact tick
start/length via setStartBeats/setLengthBeats (legacy bar fields kept as coarse
fallbacks). Content continuation per clip type: Pattern -> right.offset = (left.offset +
cut distance) mod pattern cycle; Audio -> right.contentStartSamples advances by
cutBeats * fileRate * 60 / originalBPM (the renderer's beat-domain mapping, identical in
Stretch/Resample; reader opened one-shot via mAFM; unresolvable file = offset unadjusted,
degraded-not-broken); Automation -> lane physically split at the cut (new static
evalAutomationLaneAt mirrors the processor's linear/Stepped apply — tension is not
applied at runtime there either — plus splitAutomationLane renormalizing each side with
interpolated boundary points). commitEdit + PM mutators already fire arrangement-changed
+ dirty, so players rebuild without extra plumbing.

**Playback model change (plan-locked, loud): song-mode pattern blocks now TILE.**
scheduleRollWindows gained a contentLo mask (defaulted, pattern-mode callers untouched);
the song-mode scheduler replaced the single per-block viewport call with per-tile calls:
cycle = pattern bar count on the uniform grid, origin = blockStart - offset + k*cycle,
per-tile contentHi = min(block end, tile end), contentLo = block start (notes starting
left of the cut never re-trigger; notes past the pattern's own cycle never sound — both
exactly mirror the Task 4 preview's clamps). Previously a block played its pattern ONCE
as a masked viewport (QA-Ee Issue-2 semantics) — a 4-bar block of a 1-bar pattern played
1 bar + 3 bars silence; it now loops 4x as drawn. Display + audio share the same
offset/tiling math end to end.

Build 1 FAILED (my error): C2181 at PatternManager.cpp:1654 — the contentOffsetTicks
load was inserted between the startTicks if-branch and its else (legacy startBeats
migration), orphaning the else. Moved after the full if/else. Build 2: Jeff "Clean"
(Release+Debug), 2026-07-17.

## 2026-07-17 — Task 6 — TS system: RECON RESULTS + DESIGN (per plan's bounded-recon step)

**Recon (current state):**
- TS markers: full infra in PatternManager (mTimeSigChanges, add/remove/find,
  getEffectiveTimeSigAtBar, barStartBeat (ZERO external callers — orphaned C.5b),
  serialization, ruler type-in prompts BuilderPage :3528/:3549). Consumed nowhere played.
- The played-TS driver: StandaloneEditor.cpp:785 onGetTimeSig -> currentPattern tsNum/tsDen
  pushed into the playhead every transport tick ("markers are decorative-only" comment).
- Metronome (PluginProcessor :2845-2973): transport accent = beat % currentPattern.tsNum
  (the reported regression's root); count-in same source; BOTH fire on INTEGER quarter-beat
  crossings only — /8 meters (7/8 bar = 3.5 quarter-beats) can't click their true downbeats.
- Pattern TS UI: 8-preset submenu in the transport pattern menu (StandaloneEditor :1025-1048,
  handler :1113) -> setPatternTimeSig. Docket B locks "same type-in popup" -> presets replaced
  by the marker-style type-in dialog (OPTION CHANGE, logged loud here).
- TempoMap (TempoMapRead.h): namespace-global seqlock stepped map — the publish/read pattern
  the TS map mirrors. autoDerivePatternTimeSig = dead (replaced + deleted by the new lifecycle).
- MidiRecorder: zero TS usage (recording's TS surface = count-in only). TimeMarkers +
  TempoChanges are UNIFORM-bar-authored time events — they keep uniform positions; their
  flags may sit between map bar lines after a TS change (known seam, campaign-visible).

**Design (positions stay beat-uniform; bars become a view/metronome/readout concept):**
- Blocks/notes/ticks/zoom/audio math UNCHANGED (96 ticks/quarter). No position migration.
- NEW Source/TsMapRead.h: namespace TsMap, TempoMap-style seqlock; segments {startBeat,
  beatsPerBar(quarter), num, den, barIndexAtStart}; barBeatAt(beat) O(log n). Published by
  PatternManager (message thread) on marker mutations + load + reset.
- PM: TimeSigChange += uid + linkedPattern(-1=manual); Pattern += tsBoundMarkerUid;
  project-level current-TS uid. Effective-TS lifecycle (user-set / placed follower via
  marker at earliest block / unplaced follower via binding->current->4/4) cached into
  tsNum/tsDen by refreshPatternTimeSigs() so every existing consumer (rolls, tiling,
  pattern-mode metronome, suffix) reads the right value untouched. New-pattern creation
  stamps binding = current; dead-binding re-binds to current. onTimeSigStateChanged
  callback -> StandaloneEditor refreshes rolls/dropdown/builder.
- Linked auto-markers: spawn at PLACEMENT EVENTS only (drop/draw/paint/paste/duplicate of
  a USER-SET pattern; same-bar manual wins; dedupe per bar); block move relocates its
  marker; PM cleanup pass (commitEdit/applySnapshot) removes linked markers whose
  (pattern, bar) lost its block (covers delete + undo); marker EDIT unlinks to manual
  (B2a); marker DELETE just deletes (event-only adds -> no respawn). Linked visual tell =
  outline pill vs filled. Undo/redo does NOT restore markers (markers were never in the
  undo domain — pre-existing class; logged seam).
- Metronome: click unit = 4/den quarter-beats, accent every num clicks from the bar start;
  song mode reads TsMap per span (block split at tempo AND TS boundaries), pattern mode
  uses the pattern's effective TS; count-in gains countInNum/Den atomics set at record
  start. 4/4 degenerates to today's behavior exactly.
- Ruler/grid: bar lines + labels from the map (barStartBeat/4 -> barToX); sub-bar rungs +
  ruler subdivision ticks stay beat-uniform; snap divisions >= 384 ticks snap to map bar
  starts, finer snaps unchanged; TS pill x from map; ruler TS hit-tests convert clicked
  uniform-bar -> map bar. Pattern block placement sets musical tick length
  (bars x patternBPB beats) so "1 pattern bar = 1 grid bar" holds under its auto-marker.
- Task 4/5 tiling goes BEAT-TRUE both sides (display noteBeat/4 + cycle = bars x patBPB/4;
  processor cycleBeats = bars x patBPB) — display==audio for every signature (closes the
  flagged interim mismatch).
- onGetTimeSig provider: song mode -> map at playhead beat; pattern mode -> pattern
  effective. Transport readout: song mode bars:beats:ticks from the map (beat number in
  DENOMINATOR units, e.g. 7/8 counts 1..7); pattern mode unchanged pattern-relative.
- Move-across-TS prompt: capture follower's effective TS at drag start; at move commit if
  changed AND pattern has notes -> Proceed / Lock Previous TS (Lock = setPatternTimeSig
  old values; plants NO marker).

## 2026-07-17 — Task 6 — TS system CODE-COMPLETE

**!! OPTION-SEMANTICS CHANGE (the loud paper-trail line, landing WITH the code per the
plan): the per-pattern time-signature setting STOPS driving playback/metronome/recording
directly — grid TS markers are now the SOLE played source (docket #14).** The pattern
popup is NOT removed: it is re-purposed as the auto-marker link source (docket B) and the
user-set/follower lifecycle entry point. Additionally (docket B lock "same type-in
popup"): **the old 8-preset TS submenu on the pattern dropdown is REPLACED by the
marker-style free type-in dialog** (Set / Reset to Default / Cancel).

Shipped, by file:
- NEW `Source/TsMapRead.h`: beat-indexed stepped TS timeline (TempoMap-style seqlock);
  segments {startBeat, bpb, num, den, barIndexAtStart}; barBeatAt O(log n);
  nextBoundaryAfterBeat for metronome span-splitting. Published by PatternManager on
  every marker mutation + load + reset + construction (4/4 map always live).
- PatternManager: TimeSigChange += uid + linkedPattern; Pattern += tsBoundMarkerUid;
  project current-TS uid (serialized in the TimeSigChanges node; legacy markers get
  fresh uids on load). addTimeSigChange(+linkedPattern) with manual-wins same-bar rule
  (manual write over a bar UNLINKS the holder = B2a via the edit prompt's same-bar
  replace); remove maintains current per 4B (sole survivor auto-current; else unset).
  refreshPatternTimeSigs() = the follower lifecycle (placed -> marker at earliest block
  beat; unplaced -> binding -> current -> 4/4; dead bindings re-bind to current);
  effective values CACHED in tsNum/tsDen so every consumer (rolls, tiling, suffix,
  pattern-mode metronome, loop lengths) reads correctly unchanged. resetPatternTimeSig
  (clears lock + removes still-linked spawned markers). cleanupLinkedMarkers (removal
  half; spawns are placement-event-driven). patternHasNotes. setPatternTimeSig updates
  linked markers + never spawns. addPattern stamps binding = current.
  autoDerivePatternTimeSig DELETED (orphaned C.5b, zero callers).
  **FINDING fixed in passing: reset() (File > New) never cleared ruler markers — time
  markers, TS changes, AND tempo changes all leaked into fresh projects.** Now cleared.
- Metronome (PluginProcessor): clicks run in DENOMINATOR units (7/8 = seven 8th clicks)
  with accents on map bar starts; song mode reads TsMap with the block split at tempo
  AND TS boundaries (spans carry tsBase/clickIv/num; reseed at segment entry so the
  boundary click fires accented); pattern mode uses the pattern's effective TS with
  base 0. 4/4-no-markers degenerates to the exact pre-Task-6 stream. Count-in gains
  countInNum/Den atomics; the record path captures the position's signature (song = map
  at playhead, pattern = effective) and the count-in DURATION is now one bar OF THAT
  SIGNATURE (was hardcoded 4 beats — a 3/4 count-in is 3 beats now).
- Builder grid: ruler bar lines + labels map-driven (bars literally resize at markers;
  negative pre-roll territory stays uniform; 8192-bar paint guard); drawGrid bar rung
  replaced by map bar lines (declutter parity); subdivision ticks stay beat-uniform;
  TS pills at map positions, LINKED pills draw as OUTLINE (the visual tell) vs solid
  manual; ruler TS hit-tests/tooltip/context-menu convert clicked position -> map bar;
  bar-level snap (>= 384 ticks) targets map bar starts, finer snaps beat-uniform.
  Auto-marker spawn hooks at ALL placement events (drop, ghost place, Draw, Paint x2,
  Paste, Duplicate x2); commitEdit + applySnapshot run cleanup (move/delete/undo/redo
  marker follow + follower re-derive); move commit re-spawns at new positions and fires
  the Proceed/Lock-Previous-TS prompt for followers-with-notes whose governing signature
  changed (captured at drag start on both move paths). Pattern placements + browser
  ghosts get MUSICAL tick lengths when non-4/4 (one pattern bar == one map bar under its
  auto-marker). Slice's offset wrap fixed to the musical cycle. Orphan cleanup:
  ArrangementGrid::setTimeSignature + mTimeSig removed (its last read left with the old
  uniform isMajor; setter had zero callers).
- Task 4/5 tiling now BEAT-TRUE both sides (preview x = quarter-beats; processor cycle =
  bars x effective bpb) — display == audio for every signature; the flagged interim
  mismatch is closed.
- StandaloneEditor: onGetTimeSig provider = map-at-playhead in song mode / pattern
  effective in pattern mode (the "markers are decorative" comment era ends); pattern
  dropdown gains the TS suffix on names + button ("Synths 7/8", effective, followers
  included), the type-in dialog w/ Reset, and the "Current Time Signature (new
  patterns)" submenu (grayed <2 markers, tick on current, manual re-pick per docket #4);
  onTimeSigStateChanged wired for instant refresh (rolls + pattern label also self-heal
  on their existing per-tick TS pushes — verified LayersPage/BassPage/DrumPage timer
  re-push + PianoRollPage per-tick provider, all change-guarded).
- Transport readout: song mode = map bars:beats:ticks (beats in DENOMINATOR units,
  ticks 96-PPQ within the beat unit); pattern mode = pattern effective (num AND den —
  onGetPatternTsNum callback replaced by onGetPatternTs(num&, den&)).
- Current-TS picker (grid): fires after every 2nd+ marker add and on current-marker
  delete with 2+ left; transport submenu allows manual re-pick any time.
- Known seams (logged, campaign-visible): tempo flags + time markers stay uniform-bar
  time events (their pennants can sit between map bar lines after a TS change);
  undo/redo does not restore auto-markers (markers were never in the undo domain);
  Paint-tool stamps stay 1 uniform bar (its literal contract) vs drop/ghost musical
  lengths; slice pieces do not spawn markers (a cut is not a placement).

**Task 6 build-confirm: Jeff "clean" (Release+Debug), 2026-07-17.**

## 2026-07-17 — NEW TASK — Split by Player Engine CODE-COMPLETE (owner docket 5/5a/5b + revs)

Browser pattern right-click gains "Split by Player Engine..." (id 7, next to Render to
WAV). One new pattern per TAB ENTRY with MIDI data across Layers/Bass/Drums/Clips/Inst
(rolls kept in their ORIGINAL slots so the same tab/engine keeps playing them); names =
"Original - <tab name>" via a new grid callback resolved through StandaloneEditor's page
registry + a new RibbonTabBar::getTabName(tabId) accessor (generic "Layer N" fallback);
new patterns inherit bars/TS-state/color. Placed blocks replaced in place: first split
pattern on the original row, siblings stacked DIRECTLY below (per-distinct-row, bottom-up
so insertions never shift unprocessed rows); the N-1 rows below are used when whole-row
blank, else fresh rows are INSERTED there (in-the-way rows shift down consuming blank
rows from the bottom — the new shared insertBlankRowsAt/canInsertRows/rowIsBlank grid
primitives). ONE group prompt per split op when any replaced block's row is grouped
(Yes = each stack's new rows adopt that stack's own group+color / No = untouched).
Capacity: prompt "maximum tracks reached" + FULL restore-abort (from the undo snapshot)
when blanks run out. ONE undo step (SplitPatternUndoAction: full patterns + blocks +
row names/groups/colors/mute/solo snapshot; linked markers stay outside the undo domain
per the established seam). Empty pattern -> info dialog, no-op. Original pattern deleted
last; current-pattern selection follows the first split child.

**FINDING (real pre-existing bug, fixed in-batch): PatternManager::removePattern never
re-indexed the project.** Deleting ANY pattern (the dropdown Delete flow, since forever)
left every block referencing a higher-indexed pattern pointing one pattern off (silently
playing the wrong pattern), left blocks of the deleted pattern dangling past the end
(rendered via the getPattern clamp), and — post-Task-6 — left linked TS markers with
stale pattern indices. removePattern now erases the dead pattern's blocks + linked
markers, decrements higher references (blocks, markers, current-pattern index), and
refreshes/republishes the TS state.

**Insert Track Above retrofit (owner rev): the full-grid silent no-op is now the
"Maximum Tracks Reached" prompt**, and the handler rides the shared insertBlankRowsAt
primitive (same positional-name normalization + state carry as before).

**Split + retrofit build-confirm: Jeff "Clean" (Release+Debug), 2026-07-17. QA-G is
code-complete (Tasks 1-6 + owner additions: row alignment + scrollbar rework + 500
tracks + Split by Player Engine).**

## Held Work Log entry

> HELD per bulk-run R2 — apply to `Implemented Work Log.md` verbatim when §B.13 passes.

### 2026-07-17 19:20 PT — QA-G — Timeline geometry + full time-signature system + 500 tracks + Split by Player Engine

**Bucket:** System Pages, Cross-cutting Infrastructure, UI / L&F / Theming

#### Done

- **Task 1 — ruler pin (BUILD-02):** ruler counter-translates by the viewport's vertical
  scroll (`ArrangementGrid::rulerPinY`), pinned like the header corner; all six ruler hit
  zones follow; time-selection body wash hoisted to content space; row/block mappings +
  horizontal behavior untouched.
- **Task 2 — zoom alignment (BUILD-03):** block right edges derive from barToX at 12
  sites (draw, hit-test, resize zone, marquee, automation rects, ghosts, perf overlays;
  1px gutter kept); toolbar centre-zoom anchor all-float via `xToBarF` (creep killed).
- **Task 3 — track right-click (marathon 5, #8a):** Insert Track Above, Group with
  Above / Remove from Group / Color Group... (PatternColorPicker live preview), header
  band + tint signifier, groups/colors serialized in RowState. Fixed in-batch: **Builder
  track renames never persisted** (names moved into PatternManager, sparse `<Row>`
  serialization; grid is a pass-through view); **Move Up/Down left mute/solo behind**
  (now carries mute/solo/group/color) and Move + Delete Track Clips gained undo
  registration + arrangement-changed/dirty notification.
- **Owner addition — row alignment + horizontal scrollbar:** header rows delegate ALL
  geometry to the grid (rowToY/yToRow/rowHeightPx — int row-height drift + the black
  strip below the last track killed; grid's own yToRow was also drifting hit-tests);
  Alt-zoom clamp accounts for the ruler band; horizontal scrolling unified onto ONE
  mechanism (grid = viewport-width, vertical-only viewport; a dedicated external
  scrollbar drives/tracks mBarOff with a DYNAMIC range that re-tightens when the view
  returns from empty territory).
- **Owner call — 500 tracks (FL parity; supersedes marathon-5 "keep 50"):** kNumRows +
  kMaxArrangementRows 50 -> 500; kMaxRowsInView=50 keeps the zoom-out feel; swept every
  row-count consumer (incl. a fossil 32-row cap in the Clips-page free-row scan); the
  Insert-Above overflow question dissolved.
- **Task 4 — note preview at true positions (Jeff's G1 spec):** beat-true preview
  (x = quarter-beats), musical tiling, clip at bounds. Fixed in-batch: **drum notes
  missing from every pattern preview since Phase D** (legacy `drumRoll` painted instead
  of `drumRolls[16]`).
- **Task 5 — slice content-offset model:** `ArrangementBlock::contentOffsetTicks`
  (serialized; clipboard/undo carry it); snap-resolution cuts (1-bar blocks sliceable);
  right pieces CONTINUE their content (pattern offset wrap in the musical cycle; audio
  contentStartSamples advance via the renderer's beat-domain mapping; automation lanes
  physically split with boundary points). **Song-mode pattern blocks now TILE their
  pattern** (plan-locked; was play-once viewport + silence) — scheduler gained a
  contentLo mask + per-tile windows mirroring the preview exactly.
- **Task 6 — time-signature system (#14, A, B, B1a, B2a + the 2026-07-17 owner docket):**
  **grid TS markers are the SOLE played source — the per-pattern TS setting stopped
  driving playback/metronome/recording (the loud option-semantics change), and the old
  8-preset pattern TS submenu is replaced by the marker-style type-in dialog.** New
  `Source/TsMapRead.h` seqlock timeline (PatternManager publishes). Metronome clicks in
  denominator units w/ map bar-start accents (odd + /8 meters correct, sample-tight
  through mid-song switches; spans split at tempo AND TS boundaries); count-in duration/
  clicks/accent follow the record position's signature (was hardcoded 4 beats). Ruler
  bars resize at markers (map bar lines/labels/snap; sub-bar grid stays beat-uniform).
  Pattern lifecycle: user-set (spawns linked OUTLINE-pill markers at every placement
  type, relocating on move, dying with their blocks, unlinking on edit; manual wins
  same-bar) / placed follower (earliest block governs, live) / unplaced follower
  (creation-time binding -> current-TS selection -> 4/4; re-binds when its marker dies);
  Reset to Default; current-TS add-prompt + delete re-prompt + transport-dropdown
  submenu; TS suffix on pattern names ("Synths 7/8", effective); move-across-TS
  Proceed / Lock Previous TS prompt; transport readout map-aware in denominator units.
  Fixed in-batch: **File > New leaked all three ruler marker lists into fresh projects.**
- **Owner addition — Split by Player Engine (docket 5/5a/5b):** browser pattern
  right-click; one pattern per tab entry with MIDI data (Layers/Bass/Drums/Clips/Inst,
  rolls kept in-slot), named "Original - <tab name>" (ribbon names via new
  `RibbonTabBar::getTabName`); blocks replaced in place, siblings stacked directly
  below w/ insert-style row shifting (shared insertBlankRowsAt primitive); ONE group
  prompt; capacity prompt + full restore-abort; ONE undo step (full project-slice
  snapshot). **Insert Track Above's full-grid edge now prompts "Maximum Tracks Reached"
  (supersedes the same-day silent no-op).** Fixed in-batch: **removePattern never
  re-indexed the project** — deleting any pattern shifted every higher block onto the
  wrong pattern and left dead blocks dangling; now re-indexes blocks, linked markers,
  and the current-pattern selection.

#### Found along the way (all fixed in-batch; none deferred)

Rename persistence; Move Up/Down state carry + missing undo/notify on header ops;
grid yToRow fractional-zoom hit drift; Phase-D drum-preview blanking; File > New marker
leak; removePattern re-index; the stale 32-row Clips-scan cap; C2181 build break (own
error, contentOffsetTicks load orphaned a legacy else — fixed same day).

#### Known seams (campaign-visible, logged in running notes)

Tempo/time markers stay uniform-bar time events (pennants can sit between map bar
lines); auto-markers live outside the undo domain; Paint stamps stay 1 uniform bar;
slice pieces don't spawn markers; Source-Picker ghost path doesn't set musical lengths.

**Verification:** bulk-run R2 — campaign section §B.13 (20 scenarios). Build-confirmed
clean (Release+Debug) at every task gate, 2026-07-17.

## 2026-07-17 — Owner-directed scope addition — Builder row alignment + horizontal scrollbar

Jeff (mid-Task-3): header/grid rows drift apart at Alt-scroll zoom-out + black space below
track 50 + the bottom scrollbar doesn't track shift-scroll panning and can't scroll back.
Verified NOT in any G3/G4 batch scope -> lands in QA-G per Jeff's directive ("if this
isn't in one of the batches in g3 or 4 this needs to be done here").

**Row alignment (header vs grid):** root cause = TrackHeaderPanel computed row y as
`kRulerH + r * (int)rowH` (per-row int truncation accumulates) while the grid's rowToY
truncates the float product once. Fix: rowToY/yToRow/new rowHeightPx(row) made public on
ArrangementGrid; the header now delegates ALL row geometry to them (paint loop, yToRow,
hitTestLed). Grid's own yToRow ALSO divided by int row height (found while fixing —
clicks at deep rows hit the wrong row at fractional zoom): now float division. Every
`(int)mEffectiveRowH` height consumer (drawRowBgs / drawBlocks / preview / ghost / perf
overlays / marquee / all 4 automation rects / dbl-click handle rect) now uses
rowHeightPx(row) so row bounds tile exactly. Orphaned getEffectiveRowH() accessor removed
(zero callers). Alt-zoom clamp now subtracts the ruler band: full zoom-out = ruler + all
50 rows fill the viewport exactly (kills the residual scroll + the black strip below
track 50; header height now matches the viewport since the scrollbar band is carved
before the header split).

**Horizontal scrollbar:** root cause = two competing mechanisms (viewport h-scrollbar
scrolled a totalBars-wide canvas via viewPositionX; shift-scroll panned virtual mBarOff
the scrollbar never saw). Fix: ONE mechanism — grid component width = viewport width
(resized() no longer grows with content), viewport is vertical-only
(setScrollBarsShown(true,false)), and a new external juce::ScrollBar under the grid
drives/tracks mBarOff. Range = [min(0, -maxRevealableNegativeBars, barOff),
max(totalVisibleBars, view end)] recomputed on the 30 Hz UI timer -> dynamic: panning
right extends the runway (+8 bars past view, matching totalVisibleBars), dragging back
left re-tightens the range to content ("goes back to how it would be normally"), thumb
always reflects the true view position, drag-back works. scrollBarMoved writes mBarOff +
repaints; sync pushes use dontSendNotification (no feedback loop). Both toolbar-zoom
centre anchors simplified (viewport X is always 0 now). Grid class-header Pan comment
updated.
