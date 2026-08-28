# QA-ManualPress (shutter-snapping-shrike) - running notes

Batch open 2026-08-28. Plan committed in cf323053 (rode the EQ-fix
commit); Jeff's execution ruling: "Approved, checkpoint commits like last
time, only stop for real spec calls." This stub rides the Task 1 commit.

## 2026-08-28 - Task 1 - the harness core + first figures

NEW Source/Standalone/ShotHarness.h/.cpp: the --shot mode. Branches at the
very top of initialise (before the trace logger, which would truncate the
ASIO diagnostic, and before splash/device/window); multi-instance guard
adjusted so a --shot command line is not swallowed by a running app; exits
via setApplicationReturnValue + quit with shutdown() surviving null-guarded.
Boot: heap processor + playhead + flag hookups, tool-owned PatternManager,
setPlayConfigDetails + prepareToPlay (builds buses/master/params headless -
proven by the offline render path), then releaseResources IMMEDIATELY: it
only clears the prepared flag, and with no audio thread to acknowledge,
settleAudioThread would otherwise timeout-spin on every engine create.
BaySickLAF installed by hand (no editor constructed); fontaudio glyph-width
self-check (an embedded font can link and still draw nothing); output to
Manuals/shots-staging (gitignored) - shipped figures are only replaced at
the Task 13 diff sheet.

DELIBERATE DEVIATION recorded: per-figure scale defaults 1.25 (masters
were captured on a 125% desktop; the generator renders crops at NATIVE
master pixels, so parity keeps page layout stable), not the plan's "2x";
--scale exists for when the generator learns scaled rendering.

17 figures shipping from the first run, dimensions landing on the masters
(Harmless 1306x566 EXACT, Bass crop 651x441 EXACT, Rusty Keys 704x1029
EXACT): the six BaySickSynth tabs + BaySickBass (rig-created engines,
WorkspaceWindow chrome dressed by hand - center title, preset button at
88, dummy swing binding so the knob renders; a public selectTabForShot
hook on BaySickSynthEditor, inherited by Bass), Harmless, Transport Bar
(bar + the two editor-owned satellites composed at the editor's own
coordinates, position readout flipped to time mode), Ribbon Tab Bar
(ribbon + perf readout with deterministic values; the "+" slot verified
painted by pixel check), Keybinds (command catalog registered editor-free
- picks up the Help Index row the old master is flagged STALE for
missing), File Settings (struct hoisted out of showFileSettingsDialog so
shots::makeFileSettingsComponent can host the SAME component), Export
Audio (factory over the real ExportAudioDialog + a live BuilderPage; one
Master stem entry so the toggle matches the app), Export Project Bundle
(the app's inline AlertWindow replicated verbatim - literals kept matched
to source), BaySickPedals Standard + Compact (self-contained pedals
processor, High-Gain loaded, compact slot driven sync through the picker),
Rusty Keys (bare DocumentWindow + chrome, table parsing the installed
Core Library - full kit map rendered).

Findings: "Window Chrome.png" is an ORPHAN (CHR retired 2026-08-13,
nothing references it) - dropped from the figure set, not re-shot. The
shipped "Export Project Bundle" master shows the Contents combo on its
SECOND entry; the app's fresh dialog (and ours) shows the first - the old
master was captured after a click; ours documents the true fresh state
(diff-sheet note). Export Audio's "Check against" reads Jeff's live
ui_prefs.xml (machine state, same as any hand shot). Gate six zeros +
four links twice (initial + fix pass for ribbon width and the stem entry).
