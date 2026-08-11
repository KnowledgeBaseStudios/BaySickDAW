# Running Notes - QA-Manuals (lucid-annotating-lemur)

> Append-only mid-batch log. A new entry goes in at EVERY checkpoint: a commit
> landing, a sub-task verified, a finding captured, a spec call resolved, a
> scope pivot. Per `feedback_draft_doc_running_notes_every_checkpoint.md`,
> capture as it happens rather than reconstructing at the end. At batch close
> `/draft-doc batch-close` reads this file as the primary input for the single
> Implemented Work Log entry.

**Paired plan file:** `Plans & Specs/Batch Plans/lucid-annotating-lemur.md`
**Convention:** Main Plan section 0, "Batch Plans + Running Notes layout
(locked 2026-05-11)".

---

## 2026-08-11 - Batch open - recon before planning

**What ran.** A 13-agent verification sweep over the shipping tree, before any
plan text was written: eight agents each verifying one QA-CLEANUP DELTAS claim
against source, five inventorying the raw material (System Reference set, DSP
modules, architecture subsystems, screenshots, and whether the DELTAS block is
complete).

**Why.** The Main Plan section 5 QA-Manuals entry instructs "Verify each against
the shipping tree before writing." Taking the block at its word would have put
wrong statements into a shipping manual.

### Correction logged against this session's own first pass

The session's opening read treated `MANUAL-1 Screenshot List.md` as a live
capture plan and reported that only 42 of 723 planned screenshots existed,
concluding Manual 1 could not be written. That was wrong.

Jeff killed the 723-shot plan on 2026-08-09 (transcript session
`8ef0905c-2c0d-4df6-afe4-76ed73f6b966`, message 1335). The agreed shape is
roughly 22 densely annotated screens with callout numbers as the footnote
anchors, state variants as insets, Manual 1 at 10-15 pages. Jeff shot that set,
expanded to 42 files where a screen has tabs or sections, and closed the capture
with "I have all of them done now" (message 1513). The 42 images ARE the
complete Manual 1 figure set.

The failure was not checking the transcript before treating a superseded
document as spec. Recorded here because the same document will mislead the next
reader the same way, which is why plan Task 1 retires its `SHOT-###` ids with a
header note.

### Findings that changed the plan

**1. The DELTAS block names the wrong baseline.** It treats
`d2aeb63f -> working tree` as one batch. `git log --oneline d2aeb63f..HEAD`
returns two commits. `485499ae` (2026-08-10 13:36, QA-Soundness follow-up, 34
source files) sits between the System Reference set and QA-Cleanup and is not
mentioned anywhere in the block.

**2. Seven user-visible changes are missing from the block.** Verified in
source: "Lead" voice mode removed from BaySickSynth and BaySickBass (zero
`"Lead"` hits in either directory); BaySickPlayer's LFO relabelled to
`VIBRATO` / `VIB RATE` / `VIB DEPTH` (`BaySickPlayerEditor.cpp:69`, `:177`,
`:178`); engine gain staging (Synth/Bass -12 dB, Harmless -6 dB); audio clips
gained pitch vibrato; Compressor `CS Style` renamed to `Pedal`; Harmless RM VOL
taper changed; the Edit menu's three fixed "New ... Tab" items replaced with the
full ribbon "+" list.

**3. Every named delta is wrong in at least one manual-reaching way.** Full
table in the plan file. The ones most likely to have shipped as errors:
`plugins_scan_crashes.txt` is at `Documents\BaySickDAW\` and is UTF-16LE, not
the exe folder, and the "delete a line to retry" advice is backwards because the
blacklist self-clears; only two of the four refused-file families produce a
reason string at all; there is no Tape "Advanced tab", only an Advanced MODE;
the Drum Kit vertical scrollbar shows at the shipped DEFAULT window size rather
than in a shrunken edge case.

**4. Four images are verified stale**, all because they predate `485499ae`:
`BaySickSynth OSC.png` and `Main frame.png` (both show the removed Lead voice
mode), `BaySickPlayer.png` (shows the old LFO labels), `Drum Kit.png` (empty kit
and no vertical scrollbar). Confirmed by opening each image and grepping source.

**5. Four user-facing surfaces are documented by no System Reference doc:**
the Master Analyzer window, version capture / captured takes, the Core Library
fetcher (`Options > Get Sound Content...`), and the Help menu items.

**6. Four stale source comments found**, logged for Task 1 repair per
`feedback_no_docs_only_commit_fix_wrong_comments`: `WorkspaceWindow.h:97-100`,
`StandaloneApp.cpp:1389`, `BaySickRustyDrumsProcessor.h` (14 vs 13 channels),
and two dead-name references in `PluginProcessor.cpp:7473` and
`Tools/gen_factory_presets.py:278`.

### State

- Plan file written with seven open sub-spec calls (SSC-1 through SSC-7).
- Nothing executed. No source or document edited.
- Awaiting Jeff's answers on the docket before Task 1 begins.

### Diagnostic Instrumentation Catalog

| Site | Tag | Purpose | Disposition |
|---|---|---|---|
| (none yet) | - | - | - |

---

## 2026-08-11 - Docket resolved - SSC-1..SSC-7 answered, one new call opened

All seven open sub-spec calls answered by Jeff and recorded as SC-7 through
SC-13 in the plan file.

| Was | Answer |
|---|---|
| SSC-1 marker placement | Coordinate data in percent, with a drag-to-nudge mode so Jeff repositions after Claude places |
| SSC-2 format | HTML source generating BOTH a PDF and the in-app window |
| SSC-3 in-app window | In scope. "It most certainly builds the fucking window" |
| SSC-4 stale images | Three, not five. Replacements supplied. Drum Kit is NOT stale. Mode lists get no figures |
| SSC-5 Manual 3 depth | (a) all ~40 modules in full, magic-number tables transcribed |
| SSC-6 tree | QA-Cleanup commits in the other session first; manuals describe a committed HEAD |
| SSC-7 undocumented surfaces | Master Analyzer IN (carries captured takes with it). Core Library fetcher OUT - test-install setup, not normal functionality |

**Correction to this session's recon.** The plan's first draft said five images
were stale and that the Drum Kit vertical scrollbar shows at the shipped default
window size. Both wrong. The count was four in the table and five in the prose,
and the fifth was the Compressor mode list, which is not a figure and should not
become one. The scrollbar claim leaned on the Piano Roll window's default and
minimum size constant; in the running app the bar appears only when the window
is not full-size, and the existing full-window shot is the correct figure.

**Figure set is now 44.** Three replaced (`Main frame.png`,
`BaySickSynth OSC.png`, `BaySickPlayer.png`), three added
(`Effects Panel Menu.png`, `Master Analyzer.png`, `Master Analyzer Menu.png`),
`Drum Kit.png` kept. New screen codes `FXM`, `ANLZ`, `ANLZM`.

`Effects Panel Menu.png` is load-bearing beyond its own figure: it is the proof
that Advanced is a MODE (`Show Advanced Controls`) rather than a tab, which the
DELTAS block got wrong and which would otherwise have propagated into the Tape
chapter.

**New call opened - SSC-8, what renders the manual in the in-app window.**
`juce_gui_extra` is already in the build (pulled transitively by
`juce_audio_processors`, confirmed in the generated `JuceHeader.h`), so
`juce::WebBrowserComponent` is available. But `JUCE_USE_WIN_WEBVIEW2` is not
set anywhere in `CMakeLists.txt`, so as it stands the component would fall back
to the legacy IE-era engine. Options a-d recorded in the plan file.

**Blocking action on Jeff:** the seven supplied images exist only as chat
attachments. Claude cannot write image bytes to disk. They must be saved into
`Plans & Specs/System Reference/Pictures/` under the exact filenames before
Task 2 runs.

### State

- Plan file updated: SC-7..SC-13 locked, figure-set table added, task list
  extended to 12 with a new Task 10 for the PDF export and the manuals window.
- Nothing executed. No source edited.
- Blocked on: SSC-8, the QA-Cleanup commit, and the images being saved.

---

## 2026-08-11 - QA-Cleanup committed as `ade5a10b` - plan re-checked against it

Jeff committed QA-Cleanup Tasks 2-17 in the other session (SC-12 satisfied).
Working tree is now clean apart from this batch's own two plan files and the
five picture additions/replacements. The whole recon was re-verified against the
new HEAD.

**Holds unchanged.** `plugins_scan_crashes.txt` still resolves through
`AppPaths::appRoot()` to `Documents\BaySickDAW` (`PluginManager.cpp:30`); the
Tape `Vibe` label is still the only user-facing hit; `Lead` is still absent from
both synth engines; `JUCE_USE_WIN_WEBVIEW2` is still unset in `CMakeLists.txt`
so the SSC-8 analysis stands; and the Main Plan's DELTAS block committed
verbatim, so every correction this plan records still applies.

**The 37-doc System Reference set was NOT touched by the commit**
(`git diff --stat 7f816b2e HEAD -- "Plans & Specs/System Reference/"` is empty),
so Manual 2's raw material is exactly what the inventory measured.

**All three stale source comments survived the commit** and are still wrong at
HEAD: `WorkspaceWindow.h:97-100` still claims a hosted plugin's fixed-size
surface scales via a transform; `StandaloneApp.cpp:1387-1391` still says nothing
anywhere calls `SetDllDirectory`; `BaySickRustyDrumsProcessor.h:25` still says
14 channels. Task 1's repair list is unchanged.

**Three changes to the plan**, all from things the commit carries that the recon
had not seen:

1. **The `Safe*` layer is FIVE files, not four.** `SafeAudioFormats.h` joins the
   four the DELTAS block names. It is the single audio-format registration
   point, replacing `registerBasicFormats()` calls at 19 sites across 11 files.
   Manual 3's Safe* chapter grows by one file.
2. **MP3 decoding moved from JUCE to the vendored LAME tree** via
   `MpglibAudioFormat`, with `JUCE_USE_MP3AUDIOFORMAT=0`. CMakeLists states
   ".mp3 import is UNCHANGED for the user", so this is Manual 3 only, plus
   naming the import set (WAV / AIFF / FLAC / Ogg / MP3) in Manual 2.
3. **Hosted VST3 effects can now take a sidechain**
   (`HostedPluginEffect.h:53-55` overriding `usesSidechain()`). This makes an
   existing System Reference sentence wrong: `Effect Modules.md:41` states "Six
   modules consume a sidechain" and enumerates them. Added to Task 1's
   correction list, and to the Manual 3 hosted-plugin chapter.

Item 3 is the one that matters: it is a net-new user-visible capability that
neither the DELTAS block nor the System Reference set mentions, and it
contradicts a specific sentence in the raw material.

### State

- Plan updated with the three changes. Otherwise unchanged and still awaiting
  approval.
- SC-12 satisfied: HEAD is `ade5a10b` and that is the tree the manuals describe.
- Still open: SSC-8 (manual renderer), and Jeff's approval of the plan.

---

## 2026-08-11 - SSC-8 resolved (WebView2), Send Menu added, coverage pass run

**SSC-8 answered: WebView2** (SC-14). `JUCE_USE_WIN_WEBVIEW2=1`, SDK headers
vendored into `libs/`, PDF as the universal fallback, and QA-Installer inherits
the Evergreen bootstrapper step. The deciding factor was
`Options::withNativeFunction`, which exists only on the WebView2 backend and is
what lets a Manual 1 callout open the actual page it describes.

**`Send Menu.png` added** (46 files). Jeff supplied it after the hosted-plugin
sidechain finding made him notice no figure covered the mixer send menu. Screen
code `SEND`. Worth recording how that happened: a source-verification finding
about sidechains surfaced a FIGURE gap, which is not a connection either the
recon or the plan would have made on its own.

**Coverage pass run.** Walked `Workspace and Windows.md:164-172` (the
authoritative window-kind table) plus all 36 System Reference docs against the
46 images. Confirmed `BaySickVocals.png` IS the Vox page window (Mix knob, A/B
snapshot picker, Realtime Pitch Correction board with the hardcoded
`Detected: -- Hz` readout), not a satellite, so Vox is covered. Effect panel,
effect visual, EQ, both satellite families and the Master Analyzer are covered.

**SSC-9 opened** with eight uncovered surfaces, ranked by how much documentation
would otherwise carry no figure. Top three: the Event Editor (39 control rows,
its own window, zero coverage), the Key Binds window (210 rows, the densest
reference surface in the app), and a Plugins page with a hosted VST3 actually
loaded (the surface whose behaviour changed most at QA-Cleanup, and the current
`Plugin Search.png` is the scan manager, not the tab).

Deliberately NOT asked for: anything from the retired 723-shot list, per-mode
lists, per-dialog variants, or state permutations. The test applied was "own
window, own documentation, no figure anywhere".

### State

- SC-14 locked. Plan otherwise unchanged.
- Open: SSC-9 (which uncovered surfaces get a figure), and Jeff's approval.

---

## 2026-08-11 - SSC-9 answered, figure set closed at 53, two manual-found defects

**Eight figures added, one declined.** Final set is 53. New screen codes: `EVT`
(Event Editor), `KEYS` (Key Binds), `PLUGT` (Hosted Plugin tab), `UNDO`
(History window), `RDMAP` (Rusty Keys), `RCLK` (right-click knob/slider menu),
`BUNDLE` (Export Project Bundle), `SEND` (mixer send menu).

**Clips declined, correctly.** Jeff: a Clips tab IS BaySickPlayer, so
`BaySickPlayer.png` already covers it. `Clips Page.md` documents the tab's
lifecycle, not a separate surface.

**MIDI Learn has no window.** Jeff, 2026-08-11: choosing MIDI Learn opens
nothing; the entry point is the right-click menu on an automatable control.
`MIDI Learn.md` must be written around that menu rather than around a dialog,
and `RightClick Knob or Slider.png` is the figure. This corrects an assumption
in the SSC-9 list, which had asked for "the learn window and its 30-second
binding state" as though a window existed.

### Two defects found by gathering figures

Logged under a new `MF-<n>` scheme. The severity test adopted: write the
sentence the manual would have to contain if we shipped as-is; if that sentence
is unacceptable, it is a fix-now.

**MF-1 - exports silently overwrite. CONFIRMED, fix-now.** Every save-mode file
chooser passes `saveMode | canSelectFiles` and omits
`FileBrowserComponent::warnAboutOverwriting` (128). Vendored JUCE sets
`FOS_OVERWRITEPROMPT` / `OFN_OVERWRITEPROMPT` only when that flag is present
(`juce_FileChooser_windows.cpp:60`, `:199`, `:612`), so the OS never asks. Six
sites: `BuilderPage.cpp:10488`, `:10595`, `StandaloneEditor.cpp:13905`,
`:14212`, `:14213`, `:18365`. The manual sentence would be "exporting to a
filename that already exists replaces it immediately, with no warning and no
undo" - data loss, and it contradicts the app's own convention, since
QA-Soundness built `UserFileSave.h` precisely to prompt Replace / Save a Copy /
Cancel on collision. Folded into Task 10 as a one-flag-per-site fix.

**MF-2 - bundle size confirmation. NOT CONFIRMED.** Jeff reported it missing.
The prompt exists and is native (`StandaloneEditor.cpp:14238-14251`), gated on
`estimateCopyBytes(refs, scope) > 0`; that function counts only refs where
`needsCopying(kind, scope)` is true and the file exists
(`ProjectBundler.cpp:292-299`). A project whose audio is all Core Library
content, or a scope copying nothing, legitimately yields zero and correctly
shows no prompt. Diagnostic requested from Jeff before calling it broken. This
is the `feedback_diagnose_before_fixing` case exactly - the reported symptom has
a correct-behaviour explanation that has to be eliminated first.

**Policy adopted for the rest of the batch.** Every defect found while writing
gets an `MF-<n>` entry carrying site, the sentence the manual would have to
contain, and the verdict. Fix-now items land in Task 10 (already the code task).
Everything else routes per Rule 3 with a §9 Forks entry. Nothing is documented
aspirationally; nothing is silently dropped.

### State

- SSC-9 closed. Figure set final at 53.
- MF-1 folded into Task 10. MF-2 blocked on one diagnostic.
- Still open: Jeff's approval of the plan.

---

## 2026-08-11 - No routing; MF-2 confirmed by Jeff's diagnostic

**Standing correction to how this batch handles defects.** Jeff, 2026-08-11:
V1 code is closed, so "route it per Rule 3 to the batch that owns the surface"
is dead language - there is no later batch. Anything the manuals surface is
fixed in THIS batch, or it ships and the manual documents it truthfully, and for
anything that costs a user their work the second option does not exist. The
plan's "Routing notes" section is replaced by "Defect handling during
execution". Stop offering routing as an option.

**MF-2 CONFIRMED, and it is a bigger defect than the missing prompt.**
Jeff's diagnostic: a project with a Vox recording exported with no size prompt
and reported "Extra files copied: 0", yet the zip contained both Vox WAVs.

Traced end to end:

- A recording lives inside the project folder, so `classify` returns
  `RefKind::ProjectRelative` (`ProjectBundler.cpp:20`) and `needsCopying`
  returns false, commented "inside the folder already" (`:59`).
- `estimateCopyBytes` sums only refs where `needsCopying` is true
  (`:292-299`), so it returns 0 and the `bytes > 0` gate suppresses the prompt.
- Both write branches nonetheless take the WHOLE project folder: folder mode
  `projectFolder.copyDirectoryTo (destination)` (`:432`), zip mode a recursive
  `findChildFiles` adding everything not under `Freeze/` (`:538-547`).

So the estimator answers "bytes from OUTSIDE the project folder" while the
bundle weighs "the whole project folder minus Freeze, PLUS those outside bytes".
For any real project the omitted term dominates, so the warning fails exactly
when it matters - which defeats the purpose written at its own call site,
"show the size BEFORE writing, so a large export is a decision rather than a
surprise" (`StandaloneEditor.cpp:14238`).

**Root cause worth naming:** two functions decide bundle contents
independently. The fix must make them share one rule, not duplicate it, or the
same class of bug returns. `isExcludedFromBundle` is currently a lambda inside
`write()` (`:400-406`) and has to be hoisted to file scope so the estimator can
use it.

**Knock-ons found with it:** the prompt's wording becomes wrong once the figure
includes the project; for zip mode the figure is uncompressed against a level-9
archive so it overstates the finished file; and `Result::filesCopied` counts
externals only, which is what produced the misleading "Extra files copied: 0".

**Correction to my own MF-2 entry from earlier today.** It was logged
NOT CONFIRMED on the reasoning that a Core-Library-only project legitimately
yields zero bytes. That reasoning was sound but incomplete - it explained why
the prompt COULD be silent without explaining why the zip was large anyway. The
diagnostic Jeff ran is the one that separates the two, and it took a live export
to get it.

### State

- MF-1 and MF-2 both confirmed fix-now, both folded into Task 10.
- One spec call open on MF-2's user-facing wording.
- Plan still awaiting approval; Jeff's condition is that these land first.

---

## 2026-08-11 - MF-1 and MF-2 fixed

Jeff's wording call on MF-2: **size and file count, both before and after.** He
caught that my option (b) had the second number as a size, not a count.

### MF-1 - silent overwrite

`juce::FileBrowserComponent::warnAboutOverwriting` added to all five save-mode
FILE choosers: `BuilderPage.cpp` (2 sites), `StandaloneEditor.cpp` export-audio,
bundle-zip, and export-take.

**The folder branch needed a different fix, not the flag.** The OS folder picker
has no overwrite prompt to enable, and `write()` merges into whatever directory
it is handed via `copyDirectoryTo`, replacing same-named files without a word.
Added an explicit guard in `doExportProjectBundle`: if the chosen folder already
has children, a native OK/Cancel names the folder and says same-named files will
be replaced. Placed before the enumerate so the bail is cheap.

### MF-2 - the estimate measured the wrong thing

- `kFreezeDir` and `isExcludedFromBundle` HOISTED from a lambda inside `write()`
  to file scope, so estimator and writer share ONE exclusion rule. This is the
  actual root-cause fix; leaving them separate and duplicating the rule would
  reproduce the bug class.
- `estimateCopyBytes` replaced by `estimateBundle (refs, scope, projectFolder)`
  returning `Estimate { bytes, files }`, summing the same two sets `write()`
  consumes: project-folder files minus exclusions, plus external `toCopy` refs.
  `projectFolder` is now a required argument, which is what makes the omission
  impossible to repeat.
- `Result` gained `totalBytes` / `totalFiles`, measured off the FINISHED
  artefact: a recursive walk for folder mode, and for zip the archive's real
  on-disk size plus a `zipEntriesWritten` tally kept as entries are added.
  `filesCopied` is retained unchanged because the reference-rewrite logic keys
  off it; it is simply no longer the headline number.
- Prompt now: "This bundle will be about X across N files." plus, for zip only,
  "The zip will be smaller once compressed." - the estimate is uncompressed and
  saying so beats reporting a number the user will find wrong afterwards.
- Result now: "X across N files." replacing "Extra files copied: 0".

Gate changed from `bytes > 0` to `est.files > 0`, so an export of a project
folder that exists at all now reports, rather than only when externals are
pulled in.

### State

- Build gate running.
- Awaiting Jeff's runtime verification, then plan approval.

---

## 2026-08-11 - MF-1 regression, MF-3 found, both fixed

### Regression I introduced, and how

Jeff's export test failed: the size prompt appeared, OK did nothing. Cause:
`NativeMessageBox::showOkCancelBox`'s synchronous overload CANNOT work in this
build. `ConcreteScopedMessageBoxImpl::showUnmanaged`
(`juce_ScopedMessageBoxImpl.h:58-69`) compiles its `runSync` branch only under
`JUCE_MODAL_LOOPS_PERMITTED`, which is 0 (`juce_PlatformDefs.h:323/328`), so
with a null callback it always takes `runAsync` and returns 0 - "Cancel" -
before the box is on screen. The caller bailed instantly; the user's click had
nothing listening.

**That line was pre-existing and always broken. Making it reachable was mine.**
The old `bytes > 0` gate was false on real projects, so the block never ran. The
MF-2 estimate fix made it true, and a dead confirmation became an abort. The
lesson worth keeping: correcting a gate that made code unreachable is shipping
that code for the first time, and it needs testing as new code, not as a
one-line condition change.

Swept the whole family before fixing: 16 `showOkCancelBox` / `showYesNoBox`
call sites in `Source/`. Every other one already passes a
`ModalCallbackFunction` and ignores the return. Only the two in
`doExportProjectBundle` used the return with a null callback - the pre-existing
size prompt and the folder guard I had just copied from it. Both converted to
async; `doExportProjectBundle` restructured into a `performWrite` lambda chained
behind `confirmSize` behind the folder guard. Nothing in the chain captures the
editor, so there is no lifetime hazard and no SafePointer is needed.

### MF-3 - every LiveInst tab falsely reported "corrupt data"

Jeff's second report: loading a project with recorded guitar showed
"Missing files -> Engine settings (corrupt data): LiveInst 1 / LiveInst 2",
while the project played fine.

Not corruption. `EngineChainProcessor::getStateInformation` is a no-op by design
(`EngineChainProcessor.h:68`), but `MemoryBlock::toBase64Encoding` writes a
`"<size>."` prefix, so a stateless engine saves the two characters `"0."`, not
an empty string. That passes the `isEmpty()` guard, decodes to zero bytes, and
the old test read zero bytes as corrupt. Verified directly in
`Projects/Untitled Project (132)/project.xml`: `engineData="0."` on both Inst
tabs. This has fired on every LiveInst tab in every project since the check
landed.

Fixed: an empty decoded blob now means "no state to restore" and returns. Only a
base64 failure, or a non-empty blob failing the XML magic check, is corruption.

**Jeff's follow-up question - what if that channel has pedals or NAM/IR set up?
- was the right challenge and the answer is verified, not assumed:**

- `InstPage::exportInstState()` (`InstPage.cpp:1225-1249`) writes BOTH
  `<NamIrState>` and `<PedalsState>` into a DIFFERENT attribute,
  `instChainState`. Jeff's file carries `<NamIrState data="2206.VMj...">`.
- Restore calls `ip->importInstState (chainXml)` at
  `StandaloneEditor.cpp:17730-17734`, BEFORE the live-input/sfizz branch, with
  the comment "applies to live-input AND sfizz Inst tabs. Without this, Pedals +
  NAM/IR settings reset to defaults on every project load."
- `EngineChainProcessor::setStateInformation` is ALSO a no-op (`:69`), so the
  skipped call was inert even when the blob was judged valid.

What would have made the fix wrong: if pedals/NAM state had lived in
`engineData`, the early return would have turned a loud false alarm into silent
data loss. The thing that rules it out is not the no-op - it is that
`importInstState` runs unconditionally on a separate attribute first.

Note for the manual: `ProjectBundler.cpp:94` already names `instChainState` as a
nested-XML attribute, and `:119` already documents the `"<size>."` quirk naming
the Inst chain. The knowledge existed in two places; the restore walker had
neither.

### Build gate

GREEN. Six exit codes 0, four link lines, zero errors.

### State

- MF-1 (overwrite, 5 file choosers + folder guard), MF-2 (estimate + wording),
  MF-3 (false corrupt-data) all fixed and building clean.
- Awaiting Jeff's runtime verification.
- Plan still unapproved.

---

## 2026-08-11 - PLAN APPROVED

Jeff verified all six scenarios pass (export with the size prompt, folder mode,
folder overwrite, file overwrite across all five choosers, MF-3 clean load with
pedals/NAM intact, and corruption still reporting) and approved the plan.

**Fourteen spec calls locked, SC-1 through SC-14. No open sub-spec calls.**

**Pre-batch work rides in the batch-open commit** (Jeff's call): the MF-1 /
MF-2 / MF-3 fixes were done during planning because the manuals could not
honestly document the behaviour they corrected. Build green, runtime-verified,
currently uncommitted. They go in this batch's FIRST commit rather than a
separate pre-batch one, so the batch's history starts with the tree the manuals
actually describe.

Working tree at approval, on HEAD `ade5a10b`:

- `Source/Standalone/BuilderPage.cpp` - MF-1 (2 choosers)
- `Source/Standalone/StandaloneEditor.cpp` - MF-1 (3 choosers + folder guard),
  MF-2 (async chain + wording), MF-3 (empty-blob guard)
- `Source/Standalone/ProjectBundler.cpp` / `.h` - MF-2 (hoisted exclusion rule,
  `estimateBundle`, `Estimate`, post-write totals)
- `Plans & Specs/System Reference/Pictures/` - 3 replaced, 11 new (53 total)
- `Plans & Specs/Batch Plans/lucid-annotating-lemur.md` + this file

### Resume action

Execution moves to a fresh session. First action there: Task 1, correcting the
System Reference set against the verified deltas, starting with the seven
changes from `485499ae` that the Main Plan's DELTAS block never named.

### Implemented-work entry needed at close

QA-Manuals: three cross-referenced manuals (53-figure visual atlas + music
technical reference + design technical document), the callout-id registry, the
WebView2 in-app manuals window with F1 wired, PDF export, and three defects the
writing surfaced (silent overwrite on every save-mode chooser, a bundle size
estimate that omitted the project folder, and a false corrupt-data report on
every LiveInst tab).

---

## 2026-08-11 - Task 10 removed; batch is documents-only

Jeff's call: pull Task 10 (PDF export + the in-app WebView2 manuals window) out
of QA-Manuals so the batch contains no code, and do that work separately ahead
of the manuals. SC-8, SC-9 and SC-14 still describe it and still stand; only its
home changed. Plan renumbered: Task 10 is now the cross-reference integrity
pass, Task 11 the batch close, with a callout recording the removal.

**Blocker found on executing it immediately.** The WebView2 backend needs the
Microsoft.Web.WebView2 SDK at COMPILE time, not just at runtime.
`JUCE_USE_WIN_WEBVIEW2=1` only selects dynamic vs static loader linkage; the
header is required either way. JUCE finds it through
`juce/extras/Build/CMake/FindWebView2.cmake`, which globs
`%USERPROFILE%/AppData/Local/PackageManagement/NuGet/Packages/*Microsoft.Web.WebView2*`
or an explicit `JUCE_WEBVIEW2_PACKAGE_LOCATION`. Verified NOT present on this
machine - `%LOCALAPPDATA%\PackageManagement` does not exist - and nothing
matching WebView2 is vendored under `libs/`.

So the window cannot be built without first obtaining that package. Surfaced to
Jeff as a download decision rather than actioned: a third-party package fetch
plus a new vendored dependency is his call, and installing it only into a user
profile would contradict the portable-build work QA-Soundness did (BUILDING.md,
`do_build.bat`) and reproduce the "built on my laptop and things were missing"
failure.

**The PDF half cannot be done now regardless** - there is no HTML to convert. It
is an authoring output of the manuals, not application code, so it belongs with
whoever produces the HTML.

### State

- QA-Manuals plan is approved, documents-only, Tasks 0-11.
- Manuals window + PDF: pulled out, blocked on the WebView2 package decision.

---

## 2026-08-11 - Manuals window built (pulled out of QA-Manuals)

Jeff overruled the stoppage: WebView2 was already decided, so fetching the SDK
is execution, not a new gate. Recorded because the failure mode is worth not
repeating - turning an approved decision's implementation detail into a fresh
permission request reads as stalling, and it was.

### Vendored

`libs/webview2/`: `WebView2.h` + `WebView2EnvironmentOptions.h` (2.7 MB), the
x64 `WebView2Loader.dll` (160 KB), plus `LICENSE.txt` / `NOTICE.txt`. Package
Microsoft.Web.WebView2 1.0.3485.44 from nuget.org - the version JUCE's own
`FindWebView2.cmake` names. License is BSD 3-clause, redistributable; flagged
for the QA-LegalReview manifest.

Deliberately NOT vendored: `WebView2LoaderStatic.lib` (10.4 MB per arch) and the
arm64 / x86 trees. JUCE resolves the entry point with `LoadLibraryA` +
`GetProcAddress` (`juce_WebBrowserComponent_windows.cpp:642-657`), so nothing
links at build time and dynamic loading needs headers plus the small loader
only.

### CMake

`JUCE_WEB_BROWSER=1` + `JUCE_USE_WIN_WEBVIEW2=1` on the STANDALONE TARGET ONLY.
They are not in `VIBESYNTH_DEFS` because that list also feeds
`BaySickPluginHost`, and the 32-bit helper would then need an x86 WebView2 build
for a browser it never opens. `JUCE_WEB_BROWSER=0` is REMOVED from the copied
list rather than appended over - defining a macro twice is C4005 and last-wins
is not a contract. POST_BUILD `copy_if_different` stages the loader DLL beside
the exe, mirroring the `Resources/` staging. Verified present in BOTH configs.

### The window

`Source/Standalone/ManualsWindow.h/.cpp`. Desktop `DocumentWindow` owned by the
main window via `WindowChrome::ownToMainWindow`, same shape as Key Binds,
re-fronted rather than duplicated. Requests `Backend::webview2` EXPLICITLY -
JUCE still defaults to the IE engine on Windows even with the macro on.

Content contract: `<exe dir>/Manuals/index.html`, resolved through one static
`manualsIndexFile()` so every reader shares it (standing single-source-of-truth
rule for paths). When absent - the state today, and on any clean install before
the manuals ship - the window says so and offers to open the folder rather than
showing a blank frame that reads as broken.

### F1 was never wired

The Help menu printed "(F1)" as a label only; no key handler existed anywhere.
Added `cmdShowManuals = 0x1001b` to the `BSCommands` table with `F1Key`, so it
dispatches like F5-F12 and appears as a rebindable row. Commands auto-enumerate
from that table, so registration, key mapping and the Key Binds window all
follow from the one row.

**Consequence: `Keybinds.png` is now STALE.** The General category gained a
"Help Index" row. It needs a reshoot before Manual 1's figure work - it is one
of the images Jeff captured today.

### Still outstanding for the manuals window

- The PDF export. Cannot be built yet: there is no HTML to convert. It is an
  authoring output of the manuals, not application code.
- `Plans & Specs/Test Plans/v1-master-test-plan.md:2860` still tells the tester
  not to report Help Index as dead UI. That note is wrong the moment this ships.
- `MANUAL-1 Screenshot List.md:156` and the APPENDIX both state Help Index has
  no handler. Both are now false and Task 1 must correct them.

### Build gate

GREEN. Six exit codes 0, four link lines, zero errors, loader DLL staged in
Release and Debug.

---

## 2026-08-11 - Plan reconciled to the shipped window

Jeff caught the prompt telling the next session to "ask Jeff for the window's
state at Task 9". That is bookkeeping the plan should carry, not a question for
him. Reconciled every reference:

- Task 10 removal callout now states the window is BUILT and only the PDF
  remains, rather than describing both as pending.
- New "The manuals window, already built" section records the facts the writing
  depends on: `Help Index` and F1 both live, the
  `<exe dir>/Manuals/index.html` content contract, WebView2 rendering, the
  empty-folder state, and `libs/webview2/` for QA-LegalReview.
- SC-9 marked DONE with the date.
- The undocumented-surfaces entry no longer says Help Index "stops being dead
  in this batch" - it already has.
- Task 9 gained the two real deliverables that were hiding inside old Task 10:
  write the HTML to the contract path, and generate the PDF from the same
  source.
- Verification gained steps 8 and 9: F1 must render THIS batch's output rather
  than the placeholder, and the PDF must match the in-app copy.

Also recorded in the plan, since all three would otherwise ship as lies: two
statements in `MANUAL-1 Screenshot List.md` (line 156 and the APPENDIX) and one
in `v1-master-test-plan.md:2860` all assert Help Index is dead UI.

### State

- QA-Manuals: approved, documents-only, Tasks 0-11, plan reconciled.
- Manuals window: shipped, build green, awaiting content.
- PDF export: a Task 9 deliverable of this batch.

---

## 2026-08-11 - PDF/HTML output relocated out of Task 9

Jeff asked whether the PDF export was actually in the plan. It was - added to
Task 9 in the previous pass - but checking exposed that Task 9 is the wrong
home. Task 9 is **Manual 1's body**; the HTML output and the PDF cover **all
three manuals**. Chronologically it still worked (Task 9 is the last content
task) but the wording scoped a three-manual deliverable to one manual, which is
exactly the kind of thing an executing session follows literally.

Moved to Task 10, renamed "Cross-reference integrity pass, then ship the
output", with the ordering rationale stated: integrity first, because a dead
footnote is invisible until a reader follows it. Task 9 now carries an explicit
pointer saying the output is NOT there and why.

Task 10's output steps, expanded past what Task 9 had held:

- Write all three manuals to `<exe dir>/Manuals/index.html`, and VERIFY by
  pressing F1 - it must render the manuals, not the placeholder.
- Stage into BOTH build configs, the way `Resources/` and the plugin-host
  helpers are staged. A manual next to only one exe is unopenable for half the
  testers.
- Generate the PDF from the same HTML, and check callout markers land
  identically in both - they are positioned as percentages and a print
  stylesheet can reflow them.
- Settle with Jeff where the shipping PDF lives, since QA-Installer bundles the
  manuals and this batch produces them.

Two of those (both-config staging, and marker drift between screen and print)
were not in the plan at all before this pass and would have surfaced as defects
during verification.

### State

- QA-Manuals: approved, documents-only, Tasks 0-11.
- Task 10 now owns the integrity pass AND the output for all three manuals.

---

## 2026-08-11 - Task 1 - source of truth corrected (documents)

**What ran.** A 15-area verification workflow over the shipping tree at
`ade5a10b`, each area returning apply-ready replacement text, then a second
ADVERSARIAL pass per area that re-read the source, re-grepped every `oldText`
against the real file, and hunted for sites the first pass missed. 30 agents,
zero errors. **All 15 areas came back `needs-correction` from the refute pass**,
which is the entire justification for running it - see "What the refute pass
caught" below.

Applied: 112 document edits across 25 files. No agent wrote to
`Plans & Specs/` - agents returned text, this session applied it.

### The seven unlisted `485499ae` changes are in

All verified in source first, then swept tree-wide by string rather than by
editing the doc that came to mind:

- **Lead voice mode removed.** Both engines are now
  `StringArray { "Poly", "Mono", "Legato" }` (`BaySickSynthProcessor.cpp:192-194`,
  `BaySickBassProcessor.cpp:184-186`), editors a 1x3 `BssLedRadio`. Nine doc
  sites fixed across `BaySickSynth.md` / `BaySickBass.md` / MANUAL-1.
- **BaySickPlayer LFO is now pitch vibrato**, box `VIBRATO`, knobs `VIB RATE` /
  `VIB DEPTH`, `kVibratoMaxCents = 50.0` (plus/minus half a semitone). The old
  doc line asserted in BOLD that it was a volume tremolo of about 12 percent -
  exactly backwards. Ids stay `lfo_rate` / `lfoAmt` deliberately.
- **Clip vibrato.** Same law on the timeline path
  (`kClipVibratoMaxCents = 50.0`), user-controllable because a Clips tab hosts
  the full BaySickPlayer editor. Documented as control rows, not a footnote.
- **Engine gain staging.** `kOutputHeadroom` 0.251189 (-12 dB) on Synth/Bass,
  0.501187 (-6 dB) on Harmless, applied AFTER the user's knob. Documented as a
  calibration trim; the "old presets are quieter" migration framing was
  deliberately left out per the no-backward-compat-pre-v1 rule.
- **Compressor `CS Style` to `Pedal`.** Menu literal is `Pedal (Sustain)`, short
  label `Pedal`. Eight sites across three docs.
- **Harmless RM VOL taper.** The 3.5 dB cliff is gone; the row described the old
  gated 1.5x behavior verbatim including the false "0.667 is also unity".
- **Edit menu rebuilt.** It does not COPY the ribbon "+" list, it calls
  `RibbonTabBar::buildAddMenu()` directly, so the two cannot drift - the docs
  now say that rather than describing two lists that happen to agree.

### The six DELTAS areas, corrected rather than transcribed

- **There is no "Advanced tab" - and the System Reference set was already
  right.** `Effect Modules.md:50-53` already described Advanced as a
  *Show Advanced Controls* menu toggle. The DELTAS block is what is wrong. No
  edit needed. Also logged: **two different knobs are labelled `Bias`** -
  Overdrive's (-1..+1, default 0) and Tape's (0..10, default 5).
- **Drum Kit scrollbar.** The real gate is grid HEIGHT alone (below ~302 px);
  widening the window changes nothing. The bare wheel genuinely changes meaning.
  The control lane is the second lever, and its height is user-draggable 16..240,
  not a fixed 240.
- **Mixer hamburger.** Exactly four entries confirmed. Removed label was
  `Run MT Diagnostic (2s capture)`; `MT Diagnostic` was only the prompt title.
  Net-new find: the Pan Law rows all end `at center`, which two docs had dropped
  from Triangular and Square.
- **Plugin window sizing.** The DELTAS sentence is true only for FIXED-SIZE
  plugins. In-process is centred while it fits and anchors top-left only on
  overflow; bridged is centred then INTERSECTED with the frame, so an oversized
  bridged plugin is genuinely clipped. No scrollbars anywhere.
- **Scan blacklist.** Path, UTF-16LE+BOM and self-clearing confirmed. Two recon
  claims softened as over-corrections: "app root" is this codebase's own term for
  `Documents\BaySickDAW`, and hand-deleting a line IS the only fix for one real
  case - a stranded line whose plugin no longer sits under any scan folder is
  never walked past, so it is never cleared.
- **Refused files.** Confirmed: only two of five gates produce a reason at all,
  SFZ reasons never reach a dialog interactively, two reason strings are dead
  code, NAM reasons are discarded on restore. **One DELTAS number is wrong:**
  SFZ `kMaxIncludeFiles` is **4096**, not 256 - raised on 2026-08-11 because the
  shipped Big Rusty Drums kit was being falsely refused at 256.

Plus: the `Effect Modules.md:41` sidechain sentence now names hosted VST3 as a
seventh case, with the caveat that a BRIDGED plugin never reports one because the
discovery pass sits after the bridged early-return.

### `SHOT-###` ids retired (SC-2)

`MANUAL-1 Screenshot List.md` retitled to **Element Inventory** with a status
banner naming the specific failure it exists to prevent: a session on 2026-08-11
read it as a live capture plan and reported the batch blocked. "How the ids work"
and "Sittings" marked retired/historical.

### All three "Help Index is dead UI" assertions corrected

Re-grepped for a fourth; there is none. The two surviving hits sit in
`grand-inverting-mammoth.md` and `keen-combing-heron.md`, closed-batch records
that were true on their date - left alone.

**The test-plan site was at line 2920, not 2860** (SND-44). Rewritten as a
POSITIVE check that also states the "not installed" placeholder is the CORRECT
answer on a dev build, so a tester does not file it as the very defect the note
used to suppress.

Also corrected: the appendix's dead-code bullet claimed
`DrumPage::buildDrumKitTab()` exists but is never called. **The recon was also
wrong here** - it said the function does not exist anywhere. It does:
`BaySickRustyDrumsPage::buildDrumKitTab()` (`:158`) and it IS called (`:76`);
what does not exist is the `DrumPage` one, folded out at QA-Cleanup. The routing
conclusion still holds.

### New deliverable: `System Reference/Verbatim Strings.md`

The Task 1 harvest, in a durable file rather than lost with the session. Nine
sections: the Mixer hamburger, every refusal dialog on the file-load paths, the
effect-panel menu, the sidechain picker, the Mixer "+" menu, the presets menu,
the SFZ/NAM reason strings (with the two dead ones marked), the Help menu and
manuals window, and the Master Analyzer.

It exists because **three Task 1 errors came from quoting a string from memory**:
`Help Index  (F1)` carries TWO spaces before the parenthesis, the Pan Law rows
all end `at center`, and the Compressor mode is `Pedal (Sustain)` in the menu but
`Pedal` on the button. None of those survive being retyped from a screenshot.

### What the refute pass caught (the reason it exists)

Every area needed correction. The ones that would have shipped as damage:

- **A table-row delete that would have broken two markdown tables.** `oldText`
  carried no trailing newline and `newText` was empty, so the applied result is a
  BLANK LINE mid-table - which terminates the table and drops the Legato row out
  of it. Reformulated as a two-line replace.
- **Advice the app cannot follow.** Two engine docs were about to tell the reader
  to "raise the mixer fader rather than pinning OUT VOL to the top" against a
  12 dB trim. The fader tops out at **+5.6 dB** (`PluginProcessor.cpp:8405`). A
  beginner runs out of fader and is still quiet. Cut.
- **A level-parity claim sourced from a code comment**, not from code. Cut, per
  never-trust-a-comment-over-the-code.
- **A bare dB figure with no reference point** on the Harmless CLIP row - the
  1/6 ceiling is measured before the -6 dB engine trim, so a meter shows about
  -21.5 dBFS, not -15.5. Reworded to "about a sixth of full scale".
- **`SHOT-050`'s "The ONLY way" claim**, which the Edit-menu rebuild falsifies -
  missed by the finder, caught by the refuter.
- **Two stale `SHOT-225` / `SHOT-226` headers** the finder refused to touch on
  the false premise that fixing them meant reproducing a non-ASCII em-dash. A
  suffix-only match never touches the dash.
- **A wrong prefs file** in the drafted Analyzer doc (`settings.xml` for
  `ui_prefs.xml`), which would have put the doc set at war with two docs that are
  already correct.
- **A false MF report.** The Edit-menu agent filed "Help Index is documented as
  dead" as a live defect; its refuter proved both sites had already been fixed
  earlier the same session and that the agent had read a mid-write copy.
  Reporting it to Jeff would have been a false alarm.

Two merged table rows and one dangling `INDEX.md` reference slipped through the
appliers and were caught by a structural sweep afterwards - an `insert-after`
whose `newText` lacked a leading newline welds two rows onto one line. The sweep
is now the standard post-apply check.

### Deliberately NOT applied

- **The two MP3 size-ceiling edits** (`Builder Page.md`, `Clips Page.md`). They
  document `MpglibReader`'s 512 MB / ~22-minute refusal ceilings, and those
  ceilings probably never fire - see MF-4. Documenting them would put a false
  statement in a shipping manual.
- **A `Right-Click + Wheel` row** in the Drum Kit gesture table. The gesture is
  real in code but is registered only under `Category::PianoRoll`, so it is
  absent from the app's own Drum Kit catalog. `Keyboard Shortcuts.md` promises it
  mirrors that catalog. Adding the row breaks the promise in the other direction.
  Spec call, not a silent edit.
- **`Master Analyzer.md`.** In scope (SC-13) but it is Manual 2 chapter material,
  not a Task 1 correction. Its full string set is banked in
  `Verbatim Strings.md` section I so nothing is lost. Its INDEX row lands with
  the doc, so the index never points at a file that is not there.

### Manual-found defects, MF-4 onward

All are SOURCE fixes, and this batch is documents-only by Jeff's instruction, so
all are surfaced rather than actioned.

- **MF-4 - the MP3 security change is defeated in the common case. VERIFIED
  DIRECTLY.** `JUCE_USE_WINDOWS_MEDIA_FORMAT` defaults to 1 on Windows and
  CMakeLists never overrides it, so `registerBasicFormats()` registers
  `WindowsMediaAudioFormat` - whose extension list at
  `juce_WindowsMediaAudioFormat.cpp:136` begins `".mp3"` - and it is registered
  BEFORE `MpglibAudioFormat`. `AudioFormatManager::createReaderFor` takes the
  FIRST format whose `canHandleFile` passes. So .mp3 goes to Wmvcore, not to the
  vendored decoder the whole QA-Cleanup change was built to route through, and
  mpglib's size ceilings are bypassed with it. `SafeAudioFormats.h:27-29` says in
  as many words "this is the only one claiming '.mp3' now, so ordering does not
  matter" - that comment is false. Side effect: the app also opens
  .wma / .asf / .wm / .wmv, which no UI filter offers and no doc mentions.
  Found independently by two agents; confirmed in source by this session.
- **MF-5 - the Layers and Bass "Polyphony" tab-menu item does nothing.** Found by
  one agent as a lead, independently confirmed by its refuter.
  `LayersPage.cpp:400/534` and `BassPage.cpp:385/517` query `mProcessor.apvts`
  for `tk_lay_N_bss_voiceMode` / `tk_bas_N_bsb_voiceMode`, but those ids live on
  each ENGINE's own APVTS. `DrumPage.cpp:1141/1314` does it correctly via
  `bss->apvts`. `Engine Tabs (Layers, Bass, Drums).md:93` documents it as working.
- **MF-6 - the wheel is dead over the Drum Kit vertical scrollbar.**
  `setSingleStepSize` is never called, so the bar keeps JUCE's 0.1 default against
  a range measured in PIXELS: about 0.23 px per notch, and `scrollBarMoved`
  truncates to int and writes the integer back, discarding the remainder.
  Unhurried notches move ZERO pixels. Documented honestly for now.
- **MF-7 - a rack-slot plugin window is 4 px short.** `EffectWindows.cpp:148`
  sizes to the plugin's exact w/h while `SlotComponent.cpp:614` lays out at
  `reduced(2)`. A Plugins TAB is exact; only the rack slot is wrong.
- **MF-8 - a rack-slot plugin window is force-sized to 691 x 268** by the first
  poll tick, AFTER the panel is built, and that also sets the constrainer minimum.
  A plugin smaller than 683 x 234 cannot get a fitting window on the first fit.
- **MF-9 - the Vocal Chain compressor offers a mode the chain cannot hold.**
  Pre-existing, not caused by the rename: `bsv_comp_type` spans 0..2, so picking
  `Pedal (Sustain)` mounts the Pedal panel and the engine snaps the DSP back to
  Opto on the next block. The user sees a Pedal face driving an Opto compressor.

### Stale source comments confirmed at HEAD

The four from the plan, all verified wrong, plus seven more the sweep found.
Drafted replacements are ready; none applied, pending Jeff's call on whether a
documents-only batch may touch comments.

- `WorkspaceWindow.h:97-100` - claims a hosted plugin's fixed-size surface DOES
  scale via a transform. There is no `setTransform` or `AffineTransform` anywhere
  on that path; the three remaining references are all comments. The stated
  REASON is also inverted.
- `StandaloneApp.cpp:1389` - "nothing anywhere calls AddDllDirectory or
  SetDllDirectory". `ScopedPluginDllDirectory.h:53` calls `SetDllDirectoryW`.
- `BaySickRustyDrumsProcessor.h:25-26` - "resolves to 14 channels".
  `kMaxRustyStrips` is 13 (`BaySickGraph.h:96`).
- `PluginProcessor.cpp:7473` names `addParamsForVibePlayer`;
  `Tools/gen_factory_presets.py:278` points at `Source/VibePlayer/`.
- NEW: `StandaloneEditor.cpp:15791` - "Hosted plugins keep their plugin-derived
  floors". Both `setResizeFloor` call sites pass `(0, 0)`.
- NEW: `HostedPlugin.cpp:903-908` - the SECURITY block's rationale describes a
  persisting-minimum mechanism that no longer exists, same cause. The 8192 clamp
  itself is still correct and must stay.
- NEW: `DrumKitGrid.h:20` lists "Vertical scrollbar (16 rows is fixed)" under
  "Discarded", and `DrumKitGrid.cpp:3538` says "no V scrollbar - 16 rows are
  fixed" ten lines above the code that creates it.
- NEW: `ManualsWindow.h:21-25` claims a WebView2-runtime-missing fallback that
  the .cpp does not implement; `CMakeLists.txt:782` states the real behavior.
  The two comments contradict each other.
- NEW: six `voiceMode` comments still describe the retired 4-choice list
  (`LayersPage.cpp:399/533`, `BassPage.cpp:384/516`, `DrumPage.cpp:1139/1312`).
- NEW: `BaySickSynthVoice.h:22-24` asserts APVTS persists choice params
  NORMALIZED and that an old Lead project reads back as Mono. Both halves are
  wrong - APVTS stores the denormalised index
  (`juce_AudioProcessorValueTreeState.cpp:485`), so an old Lead project stored
  `2` and now loads `2` = **Legato**, which glides instead of retriggering.
  Nothing crashes; the comment's "nothing needed migrating" conclusion is not
  what the code does.
- NEW: `KeyBindings.cpp:649-658` carries three stale USER-FACING Drum Kit
  strings that now contradict the corrected `Keyboard Shortcuts.md`. This one is
  a shipping-string fix, not a comment.

### State

- Task 1 document work COMPLETE. 25 files changed, 112 edits, plus one new file.
- Build gate not run: no source touched, so there is nothing to compile.
- Blocked on Jeff for: the source-edit docket above, and MF-4's ruling before
  the two MP3 edits can land.

---

## 2026-08-11 - Task 1b - the MF fix pass (Jeff's call: fix it, do not document it)

**Jeff, 2026-08-11:** "I need you to fix all of this stuff that you just said
isn't working before you just start documenting stuff." That overrides the
documents-only constraint for this batch. Every MF item surfaced at Task 1 close
is fixed here rather than carried.

### Owning MF-4 plainly

QA-Cleanup vendored mpglib, set `JUCE_USE_MP3AUDIOFORMAT=0`, and wrote into
`SafeAudioFormats.h` that ours "is the only one claiming '.mp3' now, so ordering
does not matter." **That sentence was false when it was written.**
`registerBasicFormats()` also registers `WindowsMediaAudioFormat`, whose
extension list LEADS with `".mp3"`, and it registers BEFORE ours;
`AudioFormatManager::createReaderFor` returns the first format whose
`canHandleFile` passes. So every .mp3 was decoded by Wmvcore, the vendoring was
a fallback that mostly never ran, and `MpglibReader`'s size ceilings were
bypassed with it. The vendoring was not wasted - it was one line short of doing
what it claimed - but the claim shipped unverified, and it took a manual-writing
read of the code to catch it. That is the failure worth remembering: the change
was never tested against the question "does our decoder actually get the file?"

### Fixes

- **MF-4** - `JUCE_USE_WINDOWS_MEDIA_FORMAT=0` added beside
  `JUCE_USE_MP3AUDIOFORMAT=0` in `CMakeLists.txt`, with both defines documented
  as load-bearing. The false comment in `SafeAudioFormats.h` is replaced with
  one that says ordering DOES matter and why. Dropping Windows Media also drops
  `.wma` / `.asf` / `.wm` / `.wmv`, which no file filter in the app offers and
  no document mentions - they were only ever reachable by accident, and each was
  a second unaudited parser on an untrusted-input path.
- **Right-click + wheel, Jeff's call: it belongs in BOTH.** The gesture was
  already implemented in the drum grid (`DrumKitGrid::mouseWheelMove:2228-2238`,
  identical tool-cycle to the Piano Roll) - only the catalog row was missing, so
  it never appeared in the Key Binds window and the doc could not honestly list
  it. Row added to `Category::DrumKit`, and the matching row added to
  `Keyboard Shortcuts.md`.
- **Three stale USER-FACING Drum Kit strings** in the same catalog block. They
  told the user "the 16 drum rows are fixed in place, so there is no vertical
  scroll" - false since the scrollbar shipped at QA-Cleanup. Rewritten to state
  the two meanings of the bare wheel and the dead-over-the-scrollbar case.
- **MF-5** - `LayersPage` and `BassPage` now resolve the Polyphony toggle
  through the ENGINE's own APVTS (`dynamic_cast` + `getParamPrefix()`), the way
  `DrumPage` always did. Four sites: two label builders, two click handlers.
  Both pages were querying `mProcessor.apvts` for ids that live on the engine,
  so every lookup returned nullptr - the label was permanently "Polyphonic" and
  the click did nothing on Layers and Bass tabs.
- **MF-6** - `mVScroll->setSingleStepSize (rowH)` in `syncScrollState`. The bar's
  range is in PIXELS and JUCE's default step is 0.1, so a notch moved ~0.23 px,
  and the int truncation on the way back discarded the remainder: unhurried
  notches over the bar moved nothing at all. One row per notch now.
- **MF-7** - the rack-slot window sizes to the plugin PLUS the bed
  `SlotComponent::resized()` insets it by. Both sites now read one shared
  `SlotComponent::kPanelOnlyInset`; a literal `reduced (2)` at one end and an
  exact `w, h` at the other is exactly how they drifted 4 px apart.
- **MF-8** - the generic 691x268 floor is no longer pushed for
  `EffectType::VST3Plugin`. It fired on the FIRST poll tick (guard starts at
  0/0), after `buildPanel`, and set a constrainer minimum as well as a size - so
  a plugin editor smaller than that could never get a window that fit it.
  `onNaturalSizeChanged` owns sizing for that type.
- **MF-9** - `bsv_comp_type` widened from 0..2 to 0..3. `CompressorDSP` has
  always implemented `Type::CS`; only the chain's parameter range excluded it,
  so picking the fourth mode mounted its panel and then clamped 3 to 2 and let
  `applyChainParams` push Opto back on the next block. **This is the one
  judgment call in the pass:** the alternative was suppressing the fourth item
  on chain slots. Widening was chosen because the fix should make the control
  the UI already offers do what it says, not remove it - but it does mean the
  Vocal Chain now genuinely has a fourth compressor mode. Flag it if that is not
  wanted. `Vocal Chain.md`'s caveat paragraph updated to match.

### Stale comments fixed (eleven sites)

`WorkspaceWindow.h:97-100` (claimed a transform that no longer exists, and the
stated reason was inverted); `StandaloneApp.cpp:1389` (claimed nothing calls
SetDllDirectory - `ScopedPluginDllDirectory.h:53` does); `BaySickRustyDrumsProcessor.h`
(14 channels vs `kMaxRustyStrips` 13); `PluginProcessor.cpp:7473` +
`Tools/gen_factory_presets.py:278` (dead `Vibe*` names);
`StandaloneEditor.cpp:15791` (plugin-derived floors that no longer exist);
`HostedPlugin.cpp:903-908` (a persisting-minimum mechanism that no longer
exists - the 8192 clamp itself stays, only its rationale was wrong);
`DrumKitGrid.h:20` + `DrumKitGrid.cpp` (two "16 rows are fixed / no V scrollbar"
claims, one of them ten lines above the code that creates the V scrollbar);
`ManualsWindow.h:21-25` (claimed a WebView2-runtime-missing fallback the .cpp
does not implement - JUCE silently substitutes the IE control instead);
`DrumPage.cpp:1139` + `:1312` (the retired 4-choice voiceMode list).

**`BaySickSynthVoice.h:22-24` was the worst of them** and is worth keeping in
mind, because it was confidently wrong in a way that would have justified a bad
decision. It claimed `AudioParameterChoice` persists NORMALIZED and that an old
Lead project therefore reads back as Mono, "so nothing needed migrating."
Verified directly against vendored JUCE: APVTS stores the DENORMALISED value
(`juce_AudioProcessorValueTreeState.cpp:485`), and for a choice parameter that
is the raw index, because its range is `{ 0, choices.size() - 1 }` interval 1. So
an old Lead project wrote `2`, and `2` in the three-entry list is **Legato** -
which glides instead of retriggering, so it does not sound like the Mono the
comment promised. Left unmigrated per the no-backward-compat-pre-v1 rule, but
the comment now records what actually happens.

### Doc edits unblocked by the fixes

The two MP3 size-ceiling paragraphs (`Builder Page.md`, `Clips Page.md`) were
held at Task 1 close because the ceilings did not fire. With MF-4 fixed they do,
so the text is now true and has landed. The Export Audio sample-rate row landed
with them.

### State

- Build gate: **GREEN**. Six exit codes 0, four link lines, zero errors. First
  attempt failed on LNK1104 against a locked Release exe (Jeff had the app
  open); Debug had already compiled and linked clean from the same sources, so
  that was never a code failure. Re-run after unlock passed outright.
- Runtime verification is Jeff's, and MF-4 / MF-5 / MF-6 / MF-9 all need an
  in-app check - see the verify script in chat.
