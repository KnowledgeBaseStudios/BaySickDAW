# Running Notes - QA-TrueLevel (candid-panning-pangolin)

> Append-only mid-batch log. A new entry goes in at EVERY checkpoint: a commit
> landing, a sub-task verified, a finding captured, a spec call resolved, a
> scope pivot. Per `feedback_draft_doc_running_notes_every_checkpoint.md`,
> capture as it happens rather than reconstructing at the end. At batch close
> `/draft-doc batch-close` reads this file as the primary input for the single
> Implemented Work Log entry.

**Paired plan file:** `Plans & Specs/Batch Plans/candid-panning-pangolin.md`
**Convention:** Main Plan section 0, "Batch Plans + Running Notes layout
(locked 2026-05-11)".

---

## 2026-08-22 - Batch open

The whole investigation that produced this batch (the LUFS comparison, the
hidden Master Gain, the two-system pan law, the dead browser menu, the empty
Reports section, the analyzer spec miss) is logged in the QA-Manuals running
notes under 2026-08-22 and in
`Research Reports/daw-architecture-pan-law-stages-2026-08-22.md`. Not repeated
here. Every ruling is in the plan's spec-call table (SC-1..SC-18).

State at open: the QA-Manuals ruling-B work (offline session on the message
thread, 5 files) is STAGED and uncommitted - verified by Jeff on installer
20260822-1438 (his normalize test ran on it) but never approved as a commit
because the normalize question came in first. Surfaced again at batch open; it
commits as a QA-Manuals commit before Task 1's.

Jeff's FL measurement that pinned the fold coefficient (SC-3): left-only
material panned 100% right reads -3 (so the far side lands in the near channel
at 0.707); both-sides material reads +3 on the near side (0.707 x (L + R) for
matching content); left-only at center reads 0 (center-unity confirmed).

Task 1 started.

## 2026-08-22 - Tasks 1-9 - bulk run (Jeff: "big boy pants on time group batch
## style"), checkpoint commits authorized without approval

Build gate held at every task; no per-task verify pauses. Commits:
be906040 (the QA-Manuals ruling-B set, committed first as its own commit),
cfc9e63c T1, fc4c9c90 T2, dc83f3f5 T3, b8d13ead T4, 53639a8b T5,
74b5a2ff T6+7, d43e5f2a T8. T9 rides in the batch-close commit.

### Task 1 - found while deleting Master Gain
The hidden 0.8 was applied THREE times to a clip, not twice. The three MT
strip tasks (Composite / Vox / Inst) handed the clip decode
`mx.masterLevel x masterGain` as a per-sample gain, and the master chain
applied both again. So the MASTER FADER itself was double-applied to clips
in the production path - at 0 dB invisible, at -6 dB clips dropped 12 while
synths dropped 6. `ctx.masterGain` is deleted; the decode multiplies by
nothing; the master fader lives in the master chain only. Not a finding
Jeff's measurement could have separated from the 0.8 (his test was at 0 dB);
it fell out of the grep. Recorded here so the +1.9 dB expectation (TL-1)
also carries the fader-drop check.

### Task 2 - the law is a global
Agent recommended a processor atomic handed to each engine through
EngineRig. Shipped as `baysick::pan::gLaw` (process-wide atomic, published
by the processor at the top of every block from master_pan_law, relaxed
loads everywhere) because a global also reaches engines built outside the
rig (auditions, pickers) and the app is standalone-only by charter. Same
precedent as MasterOutputRouting and AudioClipStreamer::sOfflineRender.
`GainCache` keeps the per-sample voice loops trig-free for a static note.

### Task 4 - channel-id ceiling
`kMaxStripChannels` is 1000, so Direct strips live at 950..965 (16 max),
not 1000+; the Effects dropdown uses 1100+ for them (dropdown ids are a
separate space). D2M's `_sendTo` is locked to the master, so the Mixer's
own bucket-by-destination layout puts the strip under the master with no
special case, and the Effects dropdown files it under DIRECT ROUTING the
same way.

### Task 5 - SS-1 / SS-2 shipped on the literal reading
One prompt per missing file, in sequence (SS-1 a); the grey "+" lands on
the browser row AND its grid blocks (SS-2 a). Both are one-line flips.
The load-time "Clip audio" entries in the MissingFileReport summary were
retired - the sweep supersedes them; kits / IRs / NAM captures still use
the summary (they have no Locate flow and are out of SC-12's scope).

### Tasks 6+7 - what "export a take" now means
The analyzer's Export Take writes the take's REPORT (a one-take session
file), copying the audio beside it when the take was captured with audio.
The old audio-only export needed a setting that defaults off, so its
button could never light - Jeff's "doesn't do anything". The in-app
take operations are single-take (export / remove the SELECTED take); the
multi-take set export lives in the HTML page (tick takes, Save selected as
report), per G.

### Task 8 - what the blueprint could not give
Per-channel true peak and L/R correlation did not exist at the master tap;
both were added (TruePeakMeter per-channel maxima, a one-pole correlation
in processMasterChain) so the Levels view could be the plugin's.

### Task 9 - manual
MIXMNU + ANLZ pages rewritten, Mixer.md / Builder Page.md / Verbatim
Strings / Screenshot List / Callout Registry updated, manual regenerated
(91 figures, 731 markers), PDFs reprinted. OPEN FOR JEFF: the Analyzer
figure's master (`Analyzer.png`) is the OLD window; the ANLZ dots were
moved to the new layout's approximate positions and the three cluster
crops removed so they auto-derive - the figure needs a re-shoot (Loudness
view as the master; Levels and Spectrum as extra masters if he wants the
clusters pictured).

### Open
- Main Plan section 5 / 6 / 9 registration for QA-TrueLevel (Jeff owns
  the plan docs - draft text is in the plan file's Routing notes).
- The system reference `Freeze and Export.md` `Measure` line still says
  two lines of text; the analyzer now also draws the momentary line.

## 2026-08-22 - Close review (/review-batch) + fixes

Two BLOCKERs, both mine: (1) `clearDirectStrips` had no caller, so File >
New / template apply left the old project's Direct strips playing and saved
them into the new project.xml - now called from `resetToBlankState`, the
same boundary the aux inserts clear at; (2) the strip's "file missing"
overlay was drawn in `paint()`, i.e. UNDER the child controls, and the
fader still took clicks - moved to `paintOverChildren`, and the strip
swallows its children's clicks while missing so the whole strip is the
Locate target. NEEDS-FIX all taken: params + insert node materialize with
the MODEL (a missing-at-load strip keeps its fader/pan/rack and its
controls bind), node removal under the shield (tearDownDirectStrip),
`resolveDirectStripFile` through `resolveProjectFile`, ONE stored-path
writer (`storedProjectPathFor`), analyzer Reset resets integrated,
correlation one-pole from the real rate, D2M browser rows grey + Locate,
undecodable-file message, take-audio copy failure reported, report labels
can't close the data block, US spellings, four doc lines that contradicted
the code. NITs left by design: stacked prompts when Remove empties a page
mid-sweep; A->B->A opens a second session file for A; drawSeries has no
per-pixel decimation (18k points x 3 at 60 Hz is fine today).

Installer `20260822-2218` (52.7 MB) carries the whole batch + the
regenerated manual + reprinted PDFs.

## 2026-08-24 - Jeff's A -> B -> A find: File > Open never ran the clean slate

Jeff loaded Display Project (fine), another project (fine), reloaded the
first: buses played but nothing fed the master; Debug identical; no save had
happened; app restart cleared it. File proven innocent by his own test.

Root cause is a whole CLASS, already half-known in the code: `replaceState`
leaves any registered parameter the incoming tree does not mention at its
CURRENT session value (the load path's own comment describes the stale-node
side of it, and clearAllRackStates exists because racks had the same leak on
File > New). File > New ran a full clean slate (param default sweep +
clearAllRackStates + EQ re-seed); File > Open restored the file OVER the
live session, so anything project B created or touched that project A's
older file predates kept B's value into A's reload. Jeff's ruling: nothing
carries between projects - the file is the only source.

Fix: the slate extracted to `resetSessionStateToDefaults()` (params to
defaults under ScopedProgrammaticParamWrites, clearAllRackStates, EQ
re-seed under the shield) - File > New calls it as before, and
`applyProcessorState` (File > Open + template apply) now calls it BEFORE
restoring. PatternManager stays out of the slate (Open replaces it from the
file; New resets it separately). MIDI hardware bindings are global-by-design
and load from the global defaults file - untouched. TL-21 added as a
MUST-PASS (A->B->A both directions + New-after-load). Which exact parameter
carried in Jeff's case is unproven (no capture); the class fix covers every
instance, and the Save-As-broken-state trap stands if anything like it ever
shows again.

Also from Jeff's first pass, fixed + committed earlier (1d277851): tree
right-click menus opened toward the top-left (TreeView re-bases the item
event; menus now take the desktop's mouse position) and the analyzer's dBFS
bars strobed (it read the mixer's exchange-and-reset peak window; it now has
its own window at the tap and draws through the master strip's own DBFSMeter
component). Installer 20260822-2308 carried those two.

## 2026-08-24 - MIDI Learn goes project-only (Jeff's ruling), viewer window added

Chasing the A -> B -> A class into MIDI Learn forced the admission: I had
built an UNSPECCED global-defaults layer (a Documents-level MidiMappings.xml
loaded at launch, plus a "Save MIDI mappings as global default" knob-menu row
gated behind sHasAnyMidiMappings) that Jeff never asked for and never saw --
the gate condition was never true on his rig, so the menu row never rendered
and no global file ever existed on his disk. Worse, the load paths treated
"project has no MidiCCMappings node" as KEEP the current table: the same
no-node inheritance leak as the A -> B -> A parameter carry. The previous
entry's line "MIDI hardware bindings are global-by-design and load from the
global defaults file - untouched" described that unspecced design as if it
were settled; this entry supersedes it.

Jeff's ruling: MIDI Learn holds to the PROJECT and nothing else, templates
included (a template made from a project with MIDI Learn carries it), plus a
Help > "View Projects MidiMap" window showing everything mapped with enough
info to tell what each row is.

Shipped:
- Global layer deleted outright: globalDefaultsFile / saveAsGlobalDefaults /
  loadGlobalDefaults (registry), the MidiLearnUI wrapper, the launch-time
  load in StandaloneApp, the SharedUI menu row + sOnMidiSaveAsDefault /
  sHasAnyMidiMappings hooks, the editor's save handler. Grep sweep confirms
  zero references remain.
- Every load boundary now REPLACES the table: applyProcessorState (File >
  Open + template apply) loads the file's node or clears when absent;
  setStateInformation gets the same else-clear; resetToBlankState (File >
  New) clears; deserializeProject's old overlay-load block deleted.
- Template save carries the maps: saveTemplateAs adds the registry node to
  the template root (drum triggers deliberately stay out - keyed on drum
  page indices one machine's kit happened to use).
- New Source/MidiLearn/MidiMapView.h: read-only table (Control / Hardware /
  Channel / Device columns, friendly names via VKnobAutomation's resolver,
  "CC N" / Pitch Bend / Aftertouch, Omni + Any-device fallbacks, sorted,
  2 Hz signature-diff refresh, empty-state text). Opened via Help > "View
  Projects MidiMap" (item 605) as aux window "midimap"; registry gained
  getAllMappings() (copy under the lock) as its data source.

Build green (six exit codes 0, four link lines, no error lines). TL-22
added to section B.37 (learn -> New empties -> reload restores -> template
carries -> no global-default menu row). Checkpoint commit under the batch's
standing authorization.
