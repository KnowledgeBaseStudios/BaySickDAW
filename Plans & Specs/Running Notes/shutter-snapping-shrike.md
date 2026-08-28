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

## 2026-08-28 - Task 2 - state-rich figures

32 more figures; the suite now runs all 49 in ~25 seconds, 0 failed.
Fifteen shoot functions covering the whole Bucket A recon: effects
cluster (empty rack first, then DeEsser + Reverb loaded, Basic off, and
the panel + visual windows composed into one host at the app's stacking),
mixer (bass channel added then mVibeGraph.rebuildRoutingFromApvts - the
cables only exist after a routing rebuild; strip crop adds Vox + Inst
channels for the live-input strip types), VU meter, Builder (pattern
manager content BEFORE the page constructor: named rows, placed blocks,
marker/timesig/tempo changes, a NoopAction so Undo enables, playhead
parked mid-ruler), Piano Roll (notes straight into bassRoll[0] + a
hand-built PianoRollConnection), Event Editor (an Automation block with a
mixer_master_fader lane + the browser pane's resolve hook), Undo History,
BaySickPlayer, the vox family (five windows off one editor; channel-id +
composite-render hooks BEFORE hosting), the rusty family (Core Library
full kit + the seven section tabs via the panel hook), Drum Kit grid,
Guitars/Basses (kits loaded BEFORE InstPage so setSource sees them), the
EQ cluster (real pre-EQ StripEq on master, six bands incl. a dynamic
bell, 48 noise blocks interleaved with graph polls so the analyzer +
GR are live; channels 1/2 get post bands for the instance browser),
analyzer, plugin search, audio settings.

Hooks: AriaControlPanel::selectTabForShot (its tab buttons ride
callAsync - dead headless), BaySickRustyDrumsPage::getAriaPanelForShot,
UndoHistoryWindow grew a showOnConstruct ctor flag, EffectEqWindow
graphForShot/railForShot, EqInstanceBrowser::pollForShot (rows only build
in its visibility-gated timer), shots::makeAudioSettingsComponent over
the file-scope dialog class. Compile lessons: BaySickRustyDrumsPage.h
only forward-declares AriaControlPanel; BaySickVocalEditor must come from
createEditor() - its panel types are complete only in its own TU, so a
by-value editor cannot destruct anywhere else.

Timer findings: sfizz kit loads are SYNCHRONOUS headless (no waits); the
vox family needs the batch's ONE timer pump (sleep 550ms then
callPendingTimersSynchronously - pitch's analyze callAfterDelay, vocals'
10Hz readout, align's preset mirror); the EQ rail syncs everything in its
own timer, so headless it kept construction-state visibility - FREQ/Q
blank and SLOPE/PHASE rows painting on a bell band. railForShot +
pollNow x2 fixed it: live note caption (W-5), filled value boxes,
conditional rows matching the live app.

Fix pass: the tab-slot pill's dropdown glyph mojibake'd - the literal
UTF-8 arrow got double-encoded somewhere in the patch pipeline; the
"\xe2\x96\xbe" escape spelling (Task 1's transport button already used
it) is immune. Both pill sites re-spelled.

Verified sample: 16 of the 32 eyeballed against masters. Mixer is
pixel-parity including the cable highlights; BaySickPitch matches its
master INCLUDING the ANALYSIS FAILED badge + raw-float readouts (the
master shows the same); Rusty Main, Vocal Chain (all six stage skins),
Event Editor, Effects Panel with Visual all land. Diff-sheet notes:
Builder is now state-rich where the old master was an empty grid (named
tracks + placed blocks per the plan's brief); EQ Instances.png is NEW
(QA-EqFlagship browser, no prior master); Audio & Midi Settings
enumerates the live machine's devices, same as any hand shot. Gate six
zeros + four links x3 to green (two compile fixes, then the fix pass +
glyph respins).

## 2026-08-28 - Task 3 - the menu engine

39 menu figures; the suite now runs all 88, 0 failed. Recon first (five
agents): the manual references 40 menu figures and four on-disk menu
PNGs are orphans (BaySickSynth/BaySickPlayer/BaySickBass/Hosted Plugin
Menu - all superseded; the live pair is the shared
BaySickSynth-BaySickPlayer-Harmless image + BaySickBass Menu Updated).
Premise corrections that shaped the build: there is no Recording
menubar menu (the six are File/Edit/Patterns/View/Options/Help;
Recording Menu is the transport record chevron, Ribbon + Menu is the
add-tab popup); showing a real menu headless is impossible by
construction (JUCE_MODAL_LOOPS_PERMITTED=0 removes show(), MenuWindow
is module-private, showMenuAsync needs the desktop) - so show-and-scrape
was never on the table and the composer is the only path.

THE COMPOSER (MenuCanvas in ShotHarness.cpp): reproduces MenuWindow's
single-column layout and paints through the SAME LookAndFeel virtuals
the real window calls. Metrics copied from juce_PopupMenu.cpp: measured
text carries the "   " + shortcut suffix, item height clamps 1..600,
the trailing separator drops, the border (2) pads only vertically,
section headers size through the header virtual, custom components
(TooltipMenuItem, HeaderSubMenuItem) mount as real children and paint
themselves. BaySickLAF overrides nothing menu-side (stock V4 recolored
Panel/Text/Highlight), so the default LAF is the right brush for every
app menu.

THE CAPTURE HOOK (NEW ShotMenuHook.h): one arm-and-return global -
build+show sites call shots::maybeCapture(m) one line before their
showMenuAsync and hand the BUILT menu over headless. 13 sites hooked
(six page-action menus, EQ band, send, effect picker, pedals swap,
slot-window title, record chevron, view-menu lambda). Public triggers
where the entry was private: PageMenuBar::triggerMenuForShot /
triggerExtraHeadingForShot, MixerPage::showSendMenuForShot,
GlobalTransportBar::showRecordMenuForShot,
BaySickPedalsEditor::showSwapMenuForShot. Build-only splits where
cleaner: GlobalAutoRightClick::buildControlMenu (static, shared with
mouseDown), MixerPage::buildAddMenu (hoisted from the editor lambda),
shots::buildMixerTitleMenu + shots::buildAnalyzerMenu (hoisted from
editor-resident lambdas into StandaloneEditor.cpp free functions via
ShotFactories.h; the editor's builders now call them then show - one
source, no drift). Already build-only, zero seams needed: BuilderPage's
three menus, EffectsPage::buildTitleMenu, RibbonTabBar::buildAddMenu,
PianoRollMenuBar/DrumKitMenuBar (public model classes the harness
instantiates directly), StandaloneEditor::getMenuForIndex.

THE EDITOR GROUP: the six menubar figures need getMenuForIndex, a
non-static member whose state (recents, template walks, the ribbon's
New Tab submenu, pattern count) is the editor's own - so the LAST
figure group constructs a full StandaloneEditor headless (ctor makes no
peer; its deferred callAsyncs are SafePointer-guarded; it re-points the
processor's PatternManager and sets process statics, hence last-by-
contract, documented in the registry). RightClick Knob or Slider rides
the same group - the label resolver is an editor-installed static.
BaySickRustyDrums Menu is the ONE replica (its builder lives inline in
an editor lambda whose items need editor actions); replicated
row-for-row with a freeze-dressed bar, drift risk documented.

Metronome Menu is not a menu - MetroPanel in a PARENTED juce::CallOutBox
(parent non-null skips addToDesktop entirely, real frame + up arrow).

Verified: size sweep across all 39 vs masters lands within 1-2px
essentially everywhere - File Menu is pixel-identical, Effects Panel
List (custom section headers + submenu arrows) matches at 1px, the
shared engine menu, Rusty replica, Mixer/Send/Add, Analyzer, Plugins
(matches its master exactly with no hosted plugin), Recording, Ribbon +,
Builder x3, Roll x5, Kit x3, Pedals x3 all parity. Deliberate
new-truth deviations for the diff sheet: Help Menu gains the View
Projects MidiMap row the stale master lacks; Analyzer Menu gains the
Levels view row; EQ Band Menu is the QA-EqFlagship rebuild (old master
shows a menu that no longer exists; shot on the dynamic bell - Slope
appears only on pass-type bands, callout text reconciles at Tasks
8-10); RightClick matches content but the old master was captured at
100% desktop scale (ours is 125% like every other master); Metronome
keeps the real CallOutBox border the hand shot cropped.

Fix pass: DrumPage null-guards its anchor (dummy anchor passed), and
the BaySickDrum Menu master documents the KIT PAD right-click shape
(fromKit=true, per-drum MIDI Note/MIDI Learn rows) - not the window
Menu dropdown; switched, parity 290x330 vs 289x330. Gate six zeros +
four links x4 (seams interim, figures, anchor fix, fromKit fix).
