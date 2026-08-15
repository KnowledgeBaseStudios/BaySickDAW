# QA-Manuals - Three cross-referenced manuals (Visual Atlas + Music Technical Reference + Design Technical Document) - Plan (lucid-annotating-lemur)

**Canonical path:** `Plans & Specs/Batch Plans/lucid-annotating-lemur.md`
**Paired running notes:** `Plans & Specs/Running Notes/lucid-annotating-lemur.md`

**For execution.** This is the punch-list, not a discussion document. Every
`- [ ]` is an action. Every "Tell Jeff:" is a hard stop where execution waits.
Read Main Plan sections 0 + 5 (the `QA-Manuals` entry incl. its QA-CLEANUP
DELTAS block) + Carry-Forward + the Implemented Work Log before starting,
per Rule 1.

---

## Context

**What this batch is.** The first batch of Phase 7 and the first non-coding
batch of the project. It produces three manuals that form one cross-referenced
system:

| Manual | What it is | Source material |
|---|---|---|
| **Manual 1 - Visual Atlas** | Every screen pictured, every visible element numbered and named, so a reader can point at anything on screen and identify it. Its callouts footnote into Manual 2. | The 56 screenshots at `Plans & Specs/System Reference/Pictures/` + the element enumeration in `MANUAL-1 Screenshot List.md` |
| **Manual 2 - Music Technical Reference** | Every knob, button, slider and action, and what it does. Organized by page and engine. Its footnotes lead into Manual 3. | The 37-doc System Reference set (10,876 lines, ~1,900 control-table rows) |
| **Manual 3 - Design Technical Document** | The implementation: DSP math, signal flow, architecture. | READ FROM THE CODE. See "The DSP Review trap" below. |

**Manual 1's shape is settled and is NOT the 723-shot plan.** Jeff killed that
on 2026-08-09 ("you think the first section of the manual is going to have 700
images which would mean it's going to be atleast 300 pages"). The agreed shape
is a set of roughly 22 densely annotated screens, with callout NUMBERS as the
footnote anchors rather than shot ids, and state variants carried as insets and
captions rather than as separate images. Jeff then shot that set, expanding it
to 42 files where a screen has tabs or sections (BaySickSynth's six panel tabs,
Rusty's seven section tabs, Pedals' two, EQ's two, Effects' three), and closed
the capture with "I have all of them done now."  **That 42 is history, not the
live count** - the set went 42 -> 45 -> 53 -> 55 -> **56** as figures were
replaced and gaps filled.  Every count in this document is 56; the number on
disk is the number that governs.

**`MANUAL-1 Screenshot List.md` is therefore NOT a live capture plan.** Its
`SHOT-###` ids are retired. What it still is, and what it was correctly called
when it was built, is an EXHAUSTIVE ELEMENT INVENTORY: every visible thing in
the app, read out of the code by a seven-agent sweep. That is precisely the
raw material for callout lists, and this batch consumes it as such. Its
APPENDIX ("Things that are NOT shots, but will bite during capture") is the
single most useful page in it for Manual 1's prose, because it enumerates the
invisible hit targets, the live-looking controls that intentionally do nothing,
and the one-per-app elements.

**The DSP Review trap.** Manual 3's formulas come from reading the DSP code.
The specs in `Files For Claude/DSP Review/` predate the implementation and
describe an app that does not exist (Jeff, 2026-08-08: "those dsp algos are
from when we first started and have no actual code or understanding of how they
work so there is no way those docs are useful"). Do not open them. The code
carries the real material: `SaturationDSP.cpp:902-906` spells out
`alpha_base = 0.2 + 0.8 * tapeSpeed`, `EffectEditorPanels.cpp:3251` documents
`k = 0.3 * Vibe in the sigmoid`, and `HostedPlugin.cpp:880-900` carries a full
architectural rationale block.

**The Tape "Vibe" trap.** The Tape effect's `Vibe` knob is the one deliberate
survivor of the app-wide `Vibe*` to `BaySick*` rename (SC-8 scope c). It stays
`Vibe` in all three manuals, alongside `Hyst` and `Bias`. Verified: exactly ONE
user-facing "Vibe" string literal exists in the whole Source tree
(`EffectEditorPanels.cpp:3251`), and exactly TWO "Vibe" hits exist across the
39 files in `System Reference/`, both the Tape knob. A manual that "helpfully"
renames that knob is wrong.

**Risk.** Low on the code side (this batch writes documents). The real risk is
writing confidently wrong statements into a shipping manual, which is why
Task 1 exists.

**Effort.** Large, multi-session (~30-50 hours per the Main Plan entry).

**Dependencies.** Main Plan says QA-RC; QA-RC dissolved into the test campaign
on 2026-08-10, so the live dependency is QA-Cleanup, which is code-complete.

---

## What the recon found (2026-08-11, before planning)

A 13-agent verification sweep checked every QA-CLEANUP DELTAS claim against the
shipping tree and inventoried the raw material. Findings that change the plan:

### The DELTAS block names the wrong baseline and is materially incomplete

The block says the captures are "one batch stale" and names six changes.
`git log --oneline d2aeb63f..HEAD` returns **two** commits, not one:

| Commit | Date | Carries |
|---|---|---|
| `d2aeb63f` | 2026-08-10 09:42 | QA-Soundness. **The System Reference set was written here.** |
| `485499ae` | 2026-08-10 13:36 | QA-Soundness follow-up. 34 source files. **Not mentioned in the DELTAS block at all.** |
| `7f816b2e` | 2026-08-10 | QA-Cleanup Task 1 (the rename). |
| working tree | uncommitted | QA-Cleanup Tasks 2+. 106 Source files, 4 untracked `Safe*.h`. |

Everything in `485499ae` and later postdates BOTH the System Reference set AND
the photos (shot 2026-08-08 17:56 through 2026-08-09 06:59). Changes the DELTAS
block never mentions, verified in source:

- [ ] **"Lead" voice mode removed** from BaySickSynth and BaySickBass. Four
  voice-mode buttons became three. VERIFIED: `grep '"Lead"'` over
  `Source/BaySickSynth/` and `Source/BaySickBass/` returns zero hits.
- [ ] **BaySickPlayer's LFO became pitch vibrato and was relabelled.**
  `BaySickPlayerEditor.cpp:69` box title is now `VIBRATO`; `:177` / `:178` are
  `VIB RATE` / `VIB DEPTH`. `BaySickPlayer.md:185` currently asserts the
  opposite behaviour in bold.
- [ ] **Engine gain staging**: BaySickSynth and BaySickBass down 12 dB,
  Harmless down 6 dB. Not visible, but audible, and the single change most
  likely to be reported as a regression by a reader.
- [ ] **Audio clips gained the same pitch vibrato** (clip LFO now modulates
  read position). `Clips Page.md` / `Automation.md` untouched.
- [ ] **Compressor "CS Style" mode is now "Pedal."** Hits `Effect Modules.md`
  (3 sites), `Pedalboard.md`, `Vocal Chain.md` (4 sites).
- [ ] **Harmless RM VOL taper changed** (3.5 dB cliff removed at the top).
- [ ] **Edit menu rebuilt**: "New Layers Tab / New Bass Tab / New Drums Tab"
  replaced with the full ribbon "+" list (Jeff, 2026-08-09 message 1513).

### Every named delta is wrong in at least one way that would reach a manual

| Delta | Verdict | The correction that matters |
|---|---|---|
| Naming current | CONFIRMED | But there is **no "Advanced tab"**. Advanced is a MODE of the single Tape panel toggled by the effect window menu item `Show Advanced Controls` (`EffectWindows.cpp:237`); Vibe / Hyst / Bias are simply hidden when basic (`EffectEditorPanels.cpp:3384-3387`). Also **two different knobs are labelled "Bias"** (`:2660` at -1..+1 default 0, and `:3253` at 0..10 default 5). |
| Drum Kit scrollbar | PARTLY WRONG | The bar appears when the window is **not full-size** (Jeff, 2026-08-11, from the running app). The name sidebar scrolls in lockstep; the ruler band and Lock button stay pinned. The bare mouse wheel **changes meaning** (rows while the bar is live, horizontal at full height), which is the part a manual must state. `DrumPage::buildDrumKitTab` **does not exist** anywhere in the tree, so the screenshot list's appendix claim about it is false. |
| MT Diagnostic removed | CONFIRMED | The removed label was **`Run MT Diagnostic (2s capture)`**, not "MT Diagnostic" (that was the prompt title). `MIDI trigger velocity` also left this menu earlier (QA-Layout T10). Current menu is exactly four entries: `Pan Law` submenu, `Master Output` submenu, `Latency-compensate meters`, `Multi-core Rendering`. |
| Plugin window sizing | PARTLY WRONG | "The plugin's own magnify is the only size control" is true only for **fixed-size** plugins. A **resizable** plugin resizes by dragging the host window (`HostedPlugin.cpp:913-923`). The window **clamps to the workspace** (`WorkspaceWindow.cpp:335-344`). When the window is bigger than the plugin the surround paints flat `0xff1c1c1e`. Bridged plugins centre; in-process anchor top-left. No scrollbars exist for the cropped region. |
| Scan crash blacklist | PARTLY WRONG | The file is at **`Documents\BaySickDAW\plugins_scan_crashes.txt`**, not the exe folder (`AppPaths.h:10-14`). It is **UTF-16LE with a BOM**. **"Delete a line to retry" is wrong** - the blacklist self-clears on the next completed scan, and a plugin that crashes the HELPER is never written to it in the first place. There is **no in-app clear control**. |
| Refused files | PARTLY WRONG | Only **two** of the four families produce a reason. `SafeXml` and `SafeAudioReader` are boolean gates with no reason string at all. SFZ reasons **never reach a dialog** on the interactive path (the user sees a bare "Could not load program"). Two reason strings are **dead code**. NAM reasons are **discarded on project restore**. |

### The images

THREE images were verified stale by opening them and checking source, all
because they predate `485499ae`. Jeff supplied replacements on 2026-08-11.

| File | What it showed | Why stale |
|---|---|---|
| `BaySickSynth OSC.png` | VOICE MODE `Poly / Mono / Lead / Legato` | Lead removed |
| `Main frame.png` | BaySickBass OSC with the same four buttons | Lead removed |
| `BaySickPlayer.png` | box `LFO`, knobs `LFO RATE` / `LFO` | now `VIBRATO` / `VIB RATE` / `VIB DEPTH` |

**`Drum Kit.png` is NOT stale** (SC-10). The recon leaned on the Piano Roll
window's default/minimum constant to claim the vertical scrollbar is the normal
state; in the running app it is not. The bar appears only when the window is
not full-size, and the existing full-window shot is the correct figure. The
scrollbar is documented in Manual 2 prose, not given a figure.

**The Compressor `CS Style` to `Pedal` rename gets no figure.** Mode lists are
text, and giving every list its own shot is the 700-image trap the atlas exists
to avoid. It is a Manual 2 text correction (8 sites across `Effect Modules.md`,
`Pedalboard.md`, `Vocal Chain.md`) and nothing more.

Cosmetic, not stale: `Vocai Chain.png` is a typo (the window title inside the
image reads `Vox 1 - Vocal Chain`); `BaySickRustyDrums Hi-Hat.png` vs the app's
tab label `Hi-hat`; `Noises and Clicks.png` vs the app's `Noises`.

### Undocumented surfaces the System Reference set misses entirely

All 37 docs conform structurally (37/37 carry the full six-section shape). Real
coverage holes found:

- **The Master Analyzer window.** IN SCOPE (SC-13). Registered as a window kind
  in three docs; its contents documented in none. Jeff supplied both figures.
  The window carries: a source picker, `Export Take...`, six readouts (MOM /
  SHORT / INT / LRA / PEAK / TRUE PK), and a target line drawn at the chosen
  loudness. Its Menu carries `View` (Loudness / Spectrum), `Source`
  (Live, plus captured takes), `Target` (Streaming -14 LUFS, Streaming -16
  LUFS, EBU R128 -23 LUFS, ATSC A/85 -24 LKFS, `Custom...`) and
  `Reset history`.
- **Version capture / captured takes.** IN SCOPE, and it turns out to live
  inside the Analyzer's `Source` menu (the `No captured takes yet` row), so it
  is documented as part of the Analyzer rather than as its own system.
- **The Core Library fetcher.** OUT OF SCOPE (SC-13). `Options > Get Sound
  Content...` is a test-install mechanism, not normal application
  functionality. Do not document it and do not "correct" the two docs that say
  nothing in the app downloads the library.
- **Help menu items.** `Help Index (F1)` and `About BaySickDAW v1.0` appear in
  no document. `Help Index` is now LIVE - it opens the manuals window, built
  2026-08-11 ahead of this batch (SC-9).

### Re-check against the QA-Cleanup commit (2026-08-11, `ade5a10b`)

QA-Cleanup landed as `ade5a10b` (Tasks 2-17). The whole recon above was
re-verified against it. Everything holds: `plugins_scan_crashes.txt` still
resolves through `AppPaths::appRoot()` to `Documents\BaySickDAW`; the Tape
`Vibe` label is still the single user-facing hit; `Lead` is still absent from
both synth engines; the Main Plan's DELTAS block committed verbatim with no
change to the text this plan corrects; and **the 37-doc System Reference set was
not touched by the commit**, so Manual 2's raw material is exactly what the
inventory measured.

Three things the commit carries that the recon had not seen:

- **The `Safe*` layer is FIVE files, not four.** `SafeAudioFormats.h` joins
  `SafeXml.h` / `SafeAudioReader.h` / `SafeNamModel.h` / `SafeSfzKit.h`. It is
  the single registration point for audio input formats, replacing
  `registerBasicFormats()` calls at 19 sites across 11 files. Manual 3's Safe*
  chapter covers five, and the DELTAS block's list of four is incomplete.
- **MP3 decoding is now ours, not JUCE's.** `MpglibAudioFormat` wraps the
  vendored LAME tree's mpglib and `CMakeLists.txt` sets
  `JUCE_USE_MP3AUDIOFORMAT=0`, retiring JUCE's hand-ported decoder. CMakeLists
  states in as many words that ".mp3 import is UNCHANGED for the user", so
  this is a **Manual 3 item only** and needs no Manual 2 text beyond naming the
  supported import formats (WAV, AIFF, FLAC, Ogg from JUCE; MP3 from mpglib).
- **Hosted VST3 effects can now take a sidechain.** `HostedPluginEffect.h:53-55`
  overrides `usesSidechain()` to return true when the hosted plugin declares a
  sidechain input (`HostedPlugin.h:69`). This makes an existing System Reference
  sentence WRONG: `Effect Modules.md:41` says "Six modules consume a sidechain
  (`usesSidechain()` returns true)" and names them. The count is now those six
  built-ins PLUS any hosted VST3 that declares one. Added to Task 1.

### Manual-found defects, and the policy for them

Writing a manual is a UI audit. It finds a class of defect that code reading
structurally cannot: things that are VISUALLY or BEHAVIOURALLY wrong while being
functionally fine, so no test fails. Two surfaced during the gap-fill round.

**The rule that makes this tractable: the manual documents what SHIPS.** There
are only two honest end states for any defect found here - fix it and document
the fix, or ship it and document the truth. There is no third state where the
manual describes what we meant. So the severity test is a single question:

> Write the sentence the manual would have to contain if we shipped as-is.
> If that sentence is unacceptable, the defect is a fix-now.

**MF-1 - Exports silently overwrite an existing file. CONFIRMED.**
Every save-mode file chooser in the app passes
`FileBrowserComponent::saveMode | canSelectFiles` and **omits the
`warnAboutOverwriting` flag** (value 128). JUCE sets `FOS_OVERWRITEPROMPT` /
`OFN_OVERWRITEPROMPT` only when that flag is present
(`juce_FileChooser_windows.cpp:60`, `:199`, `:612`), so the OS never asks.
Six sites: [`BuilderPage.cpp:10488`](Source/Standalone/BuilderPage.cpp:10488),
[`:10595`](Source/Standalone/BuilderPage.cpp:10595),
[`StandaloneEditor.cpp:13905`](Source/Standalone/StandaloneEditor.cpp:13905),
[`:14212`](Source/Standalone/StandaloneEditor.cpp:14212),
[`:14213`](Source/Standalone/StandaloneEditor.cpp:14213),
[`:18365`](Source/Standalone/StandaloneEditor.cpp:18365).
The sentence: *"Exporting to a filename that already exists replaces it
immediately, with no warning and no undo."* That is data loss, and the app
already has the opposite convention everywhere else - QA-Soundness built
`UserFileSave.h` specifically to prompt Replace / Save a Copy / Cancel on
collision. **Fix-now.**

**MF-2 - The bundle size estimate measures the wrong thing. CONFIRMED.**
Jeff's diagnostic (2026-08-11): a project with a Vox recording exported with no
size prompt and reported "Extra files copied: 0", yet the zip contained both Vox
WAVs. Traced:

- A recording lives INSIDE the project folder, so `classify` returns
  `RefKind::ProjectRelative`
  ([`ProjectBundler.cpp:20`](Source/Standalone/ProjectBundler.cpp:20)) and
  `needsCopying` returns false for it, commented "inside the folder already"
  ([`:59`](Source/Standalone/ProjectBundler.cpp:59)).
- `estimateCopyBytes` sums ONLY refs where `needsCopying` is true
  ([`:292-299`](Source/Standalone/ProjectBundler.cpp:292)), so it returns 0.
- But **both write branches take the entire project folder**: folder mode calls
  `projectFolder.copyDirectoryTo (destination)`
  ([`:432`](Source/Standalone/ProjectBundler.cpp:432)); zip mode walks
  `projectFolder.findChildFiles (..., recursive)` and adds every file that is not
  under `Freeze/` ([`:538-547`](Source/Standalone/ProjectBundler.cpp:538)).

So the estimate answers "how many bytes come from OUTSIDE the project folder"
while the bundle's actual weight is "the whole project folder minus `Freeze/`,
PLUS those outside bytes". For any real project the omitted term is the dominant
one, which means **the warning fails exactly when it matters**. That defeats the
feature's own stated purpose, written at its call site: "show the size BEFORE
writing, so a large export is a decision rather than a surprise"
([`StandaloneEditor.cpp:14238`](Source/Standalone/StandaloneEditor.cpp:14238)).

The fix is to make the estimate consume the same two sets the writer does:
project-folder bytes excluding `Freeze/`, plus the external `toCopy` bytes. That
requires hoisting the `isExcludedFromBundle` lambda out of `write()`
([`:400-406`](Source/Standalone/ProjectBundler.cpp:400)) to file scope so the
estimator and the writer share one exclusion rule. Two functions deciding bundle
contents independently is the root cause here, and duplicating the rule would
reproduce it.

Knock-on: the prompt's wording "This bundle will copy X of audio alongside the
project" becomes wrong once X includes the project, and for zip mode the figure
is uncompressed bytes against a level-9 archive, so it overstates the final
file. Both need rewording. `Result::filesCopied` also counts externals only,
which is what produced the misleading "Extra files copied: 0".

**Policy for the rest of the batch.** Every defect found while writing gets an
`MF-<n>` entry in the running notes carrying: the site, the sentence the manual
would have to contain, and the verdict. **All of them are fixed in this batch.**
V1 code is closed, so there is no later batch to route to; a defect the manuals
surface is either fixed here or shipped and documented truthfully, and for
anything that loses a user's data the second option does not exist. Fix work
lands in Task 10, which is already the batch's code task.

### The manuals window, already built (2026-08-11, ahead of this batch)

Pulled out of this batch so QA-Manuals stays documents-only, then built
immediately. It is in the working tree, builds green, and the manuals this batch
writes are what it will display. Facts the writing depends on:

- **`Help > Help Index` and the F1 key both work.** `case 601` opens
  `ManualsWindow`; `cmdShowManuals` (`0x1001b`) is a real rebindable command
  bound to `F1Key`. F1 had never been wired to anything before - the menu only
  printed the label.
- **Content contract: `<exe dir>/Manuals/index.html`.** Resolved through
  `ManualsWindow::manualsIndexFile()`. **This batch's HTML output must land
  there**, or the window has nothing to show.
- **It renders with WebView2**, so the in-app copy is the same HTML as the PDF.
- **When the folder is empty the window says so** and offers to open it. That is
  today's state and the state of any install before the manuals ship.
- **`libs/webview2/`** now holds the vendored SDK (headers + x64 loader DLL,
  BSD 3-clause). Add it to the QA-LegalReview manifest.

**Three documents now assert the opposite and Task 1 must fix all three:**

| Where | Says | Truth |
|---|---|---|
| `MANUAL-1 Screenshot List.md:156` | "Help Index (F1) has no handler by design" | it opens the manuals window |
| same file, APPENDIX "Live-looking things that intentionally do nothing" | lists Help Index | it is live |
| `Plans & Specs/Test Plans/v1-master-test-plan.md:2860` | tells the tester not to report Help Index as dead UI | it is not dead |

**`Keybinds.png` is STALE** - the General category gained a "Help Index" row
when the command was added. Reshoot before Task 9.

**Still outstanding: the PDF export.** Nothing to convert until the HTML exists.
It is a Task 9 deliverable of this batch, not app code.

### Stale comments in source, found while verifying

Per `feedback_no_docs_only_commit_fix_wrong_comments`, wrong comments get fixed:

- `WorkspaceWindow.h:97-100` still says a hosted plugin's fixed-size surface
  DOES scale via a transform. It does not; that path was deleted.
- `StandaloneApp.cpp:1389` says "nothing anywhere calls AddDllDirectory or
  SetDllDirectory". `ScopedPluginDllDirectory.h:53` calls `SetDllDirectoryW`.
- `BaySickRustyDrumsProcessor.h` says discovery "resolves to 14 channels".
  It resolves to 13 (`kMaxRustyStrips`).
- `PluginProcessor.cpp:7473` names `addParamsForVibePlayer`, a function that no
  longer exists. `Tools/gen_factory_presets.py:278` points at
  `Source/VibePlayer/VibePlayerProcessor.cpp`, now `Source/BaySickPlayer/`.

---

## The cross-reference system

Three id namespaces. Section numbers are NOT ids. Callout numbering is
contiguous per screen - a removal renumbers the screen and every
cross-reference mechanically in one pass, coordinates carried through the map.
(The never-renumber wording that stood here until 2026-08-13 was a constraint
Claude invented during planning, not something Jeff asked for; corrected when
it left figures with dots starting at 2.) `IMP-<n>` ids do not renumber.

### Namespace 1 - callout ids (`<SCREEN>-<n>`), owned by Manual 1

Each of the 56 images gets a short SCREEN CODE - one code per FILE, so the code count is the file count. Each numbered callout drawn on
that image is `<SCREEN>-<n>`, e.g. `MIX-14`. This is the scheme Jeff described
as the spine and the one already proposed and accepted on 2026-08-09.

| Group | Codes |
|---|---|
| Shell | `FRAME`, `TRAN`, `RIB`, `RIBM`, `CHR` |
| System pages | `BLD`, `MIX`, `FXI`, `PR`, `DKIT` |
| BaySickSynth | `SYN-OSC`, `SYN-OENV`, `SYN-FLT`, `SYN-FENV`, `SYN-LFO`, `SYN-MOD` |
| Other engines | `HARM`, `PLY`, `GTR`, `BAS` |
| Rusty | `RD-MAIN`, `RD-KICK`, `RD-SNARE`, `RD-TOM`, `RD-HAT`, `RD-CYM`, `RD-NOISE` |
| Pedals | `PED`, `PEDL` |
| Vocal | `VOX`, `VCH`, `PITCH`, `ALIGN`, `NAMIR` |
| Effects and EQ | `FX`, `FXV`, `EQ`, `EQB` |
| Dialogs | `EXP`, `AUD`, `FILE`, `PLUG` |

Two rules keep ids unique without a 2,000-row registry:

- A control that repeats WITHIN one image gets ONE callout, labelled as the
  pattern (`MIX-7 Channel fader - every strip has one`).
- A control that repeats ACROSS images is owned by the image where it is
  taught; other images cross-reference it (`see MIX-7`) rather than minting a
  second id.

**Actual total: 593 callouts across 56 images** (Task 2, 2026-08-12).  The
estimates this document carried before then - 1,100-1,500 against 42 images,
then 1,450-1,950 against 55 - were both made BEFORE the two collapse rules were
applied to real screens, and both were roughly double the truth: a mixer strip is
22 callouts in total rather than 22 per strip times the strip count, and every
window title strip resolves to `see CHR` instead of minting seven fresh ids per
screen.

### Namespace 2 - Manual 2 sections

Manual 2 mirrors the seven groups already used by `System Reference/INDEX.md`,
so no reorganization is needed and cross-doc lookups stay one-shot. Each
control entry is headed by its callout id, carries `Fig.` back-references to
every image that pictures it, and carries a forward footnote to Manual 3 when
there is implementation depth.

### Namespace 3 - `IMP-<n>`, owned by Manual 3

One id per implementation topic (a DSP algorithm or an architecture subsystem).
Roughly 65 expected: ~40 DSP modules plus ~24 architecture subsystems. Each
`IMP-<n>` section carries back-references to the callout ids it explains.

### The registry

One generated join table at
`Plans & Specs/System Reference/Callout Registry.md`, columns:
`callout id | screen | on-screen label | Manual 2 section | IMP id | System Reference source`.

This is the batch's spine and it is why Task 2 precedes all three manual
bodies: once every id is allocated, the three manuals can be written in any
order without renumbering.

---

## Spec calls already locked

| ID | Decision | Reasoning |
|---|---|---|
| SC-1 | Manual 1 is a set of ~56 densely annotated screens, NOT 723 shots | Jeff 2026-08-09. The 723-shot list is retired as a capture plan. |
| SC-2 | `MANUAL-1 Screenshot List.md` is kept as the ELEMENT INVENTORY, and its `SHOT-###` ids are retired | It is the code-read enumeration of every visible element; that is the callout raw material. Retiring the ids avoids a dead second anchor namespace competing with the callout ids. |
| SC-3 | Callout ids are `<SCREEN>-<n>`, assigned once, append-only, retire-never-reuse | Jeff 2026-08-09, the `MIX-14` scheme. |
| SC-4 | Manual 3's formulas come from reading the code; `Files For Claude/DSP Review/` is not opened | Jeff 2026-08-08. Those specs predate the implementation. |
| SC-5 | The Tape `Vibe` / `Hyst` / `Bias` knobs keep their names in all three manuals | SC-8 scope c of QA-Cleanup. The one deliberate survivor of the rename. |
| SC-6 | Manual 2 mirrors the `System Reference/INDEX.md` grouping | The source material is already organized that way; reorganizing buys nothing and breaks the one-shot lookup. |
| SC-7 | Callout markers are placed by COORDINATE DATA (percent of image width/height) held in a per-figure block, not baked into pixels | Jeff 2026-08-11, resolving SSC-1: he must be able to move a marker after Claude places it. Percentages survive any image re-scale, and a marker is repositioned by editing one number. The authoring page ships a drag-to-nudge mode that writes updated coordinates back for pasting. |
| SC-8 | Authoring format is HTML; deliverables are a PDF (opens anywhere) AND an in-app manuals window bound to F1 | Jeff 2026-08-11, resolving SSC-2: "I need a pdf or something like that so a user can open it anywhere, but I also want it to be loadable in the application from the f1 help button". One HTML source generates both, so the two can never drift. |
| SC-9 | The in-app manuals window is built. **DONE 2026-08-11, ahead of this batch** | Jeff 2026-08-11, resolving SSC-3: "It most certainly builds the fucking window". Then pulled OUT of QA-Manuals so this batch holds no code, and built immediately. `HOLD-FOR-MANUALS-WINDOW` at `StandaloneEditor.cpp` is retired; `Help Index` and F1 both open `ManualsWindow`. |
| SC-10 | THREE images were stale and Jeff supplied replacements; `Drum Kit.png` is NOT stale; mode lists do not get their own figures | Jeff 2026-08-11, resolving SSC-4. The Drum Kit vertical scrollbar is the not-full-window state, and the existing full-window shot is the correct figure. Per-mode list shots are exactly the 700-image trap the atlas exists to avoid. |
| SC-11 | Manual 3 covers all ~40 DSP modules in full, with the magic-number tables transcribed | Jeff 2026-08-11, resolving SSC-5 (pick a). |
| SC-12 | QA-Cleanup's working tree is committed in a separate session before this batch executes | Jeff 2026-08-11, resolving SSC-6: "I am closing that out now and yes I will have that commited in the other session before we move forward with this plan". The manuals therefore describe a committed HEAD. |
| SC-13 | The Core Library fetcher is OUT of scope. The Master Analyzer is IN | Jeff 2026-08-11, resolving SSC-7: the fetcher "is a test install setup and has nothing to do with the normal functionality of the application". Documenting the Analyzer also covers version capture / captured takes, which live in its Source menu. |
| SC-14 | The in-app manuals window uses **WebView2** (`JUCE_USE_WIN_WEBVIEW2=1`), with the PDF as the universal fallback | Jeff 2026-08-11, resolving SSC-8. Accessibility is already covered by the PDF, so the in-app window is free to be the best renderer rather than the lowest common denominator. `Options::withNativeFunction` (WebView2 backend only) is what lets a Manual 1 callout open the actual page it describes. Costs: WebView2 SDK headers vendored into `libs/`, an `/audit-licenses` pass on them, and a bootstrapper step inherited by QA-Installer. |

---

## Figure set (56 images, all on disk)

Verified present in `Plans & Specs/System Reference/Pictures/` on 2026-08-11.
The 42 originals with three replaced in place, plus three new. Filenames below
are the REAL disk names, checked rather than assumed.

| File | Status |
|---|---|
| `Main frame.png` | REPLACED - VOICE MODE now reads `Poly / Mono / Legato` |
| `BaySickSynth OSC.png` | REPLACED - same three-button voice mode |
| `BaySickPlayer.png` | REPLACED - box is `VIBRATO`, knobs `VIB RATE` / `VIB DEP...` |
| `Effects Panel Menu.png` | NEW - `Show Advanced Controls` / `Mode: Modern...` / `SC: Off...` / `Presets...` / `Visual`. The figure that proves Advanced is a MODE, not a tab. |
| `Analyzer.png` | NEW - the window: source picker, `Export Take...`, six readouts (MOM / SHORT / INT / LRA / PEAK / TRUE PK), target line at -14.0 LUFS |
| `Analyzer Menu.png` | NEW - `View` (Loudness / Spectrum), `Source` (Live / `No captured takes yet`), `Target` (four presets + `Custom...`), `Reset history` |
| `Send Menu.png` | NEW - the mixer send menu. Added 2026-08-11 after the hosted-plugin sidechain finding surfaced that no figure covered sends. |
| `Drum Kit.png` | KEPT - full-window shot, correct as-is |

Screen codes for the four new figures: `FXM`, `ANLZ`, `ANLZM`, `SEND`.

### Gap-fill round (2026-08-11, SSC-9 answered)

Jeff supplied eight more figures after the coverage pass, taking it to 53.
Two more landed on 2026-08-12 with the surfaces they picture: `VU Meter.png`
(the master VU window) and `Effects Rack Menu.png` (the rack Menu, which gained
a `VU Meter` entry next to VU Calibration).  Both still need screen codes -
Task 2 assigns them.  `Effects Rack.png` followed on 2026-08-12 and took
`FXI`, the code the plan had already reserved for the rack page.  **Count on
disk: 56.**

| File | Screen code | Covers |
|---|---|---|
| `Event Editor.png` | `EVT` | the automation-clip editor (F12) |
| `Keybinds.png` | `KEYS` | the Key Binds window |
| `Hosted Plugin.png` | `PLUGT` | a Plugins tab with a hosted VST3 loaded |
| `UndoHistory.png` | `UNDO` | the History window |
| `Rusty Keys.png` | `RDMAP` | Help > Rusty Drums Map |
| `RightClick Knob or Slider.png` | `RCLK` | the right-click menu on any automatable control. **MIDI Learn has no window of its own** (Jeff, 2026-08-11) - this menu is the entire MIDI-Learn entry point, and `MIDI Learn.md` must be written around it rather than around a dialog. |
| `Export Project Bundle.png` | `BUNDLE` | the bundle dialog as it currently behaves |
| `Send Menu.png` | `SEND` | the mixer send menu |

**Clips gets no figure** (Jeff, 2026-08-11): a Clips tab IS BaySickPlayer, so
`BaySickPlayer.png` already covers it. `Clips Page.md` documents the tab's
lifecycle, not a separate surface.

### Coverage pass against the window-kind list

`Workspace and Windows.md:164-172` is the authoritative list of window kinds.
Checked figure-by-figure. Page windows Builder / Mixer / Effects / Piano Roll /
Layers / Bass / Drums / Vox / Inst are covered; `BaySickVocals.png` is confirmed
to BE the Vox page window (Mix knob, A/B snapshot picker, the Realtime Pitch
Correction board), not a satellite. Effect panel, effect visual, EQ, the Vox
satellites (Chain / Pitch / Align / NAM-IR), the Inst satellites (Pedals /
NAM-IR) and the Master Analyzer are all covered.

**Uncovered surfaces are listed in the open-questions section below.** Jeff
rules on which get a figure.

Two filename notes carried from the recon:

- `Vocai Chain.png` was `Vocal Chain` misspelled. **RESOLVED 2026-08-12** - Jeff
  renamed it to `Vocal Chain.png`; nothing linked to the old path.
- `BaySickRustyDrums Hi-Hat.png` / `Noises and Clicks.png` were recorded as
  "differing from the app's tab labels `Hi-hat` / `Noises`". **That had it
  backwards** (Jeff, 2026-08-12): those filenames are the names the ARIA PANEL
  ITSELF prints - the Noises page's own heading reads `NOISES AND CLICKS` - and
  our ribbon tab button is the abbreviation, not the authority.  Nothing to fix.
  The manual can use either, but must not treat the tab label as the panel's
  real name.

---

## Sub-spec calls surfaced for ExitPlanMode

**SSC-1 through SSC-8 are RESOLVED** and recorded as SC-7 through SC-14 above.
One call is open:

**SSC-9 - Which uncovered surfaces get a figure?**

Derived by walking `Workspace and Windows.md`'s window-kind table and the 36
System Reference docs against the 46 images. These are surfaces with their own
window and their own documentation that no current figure shows. This is NOT a
request to reopen the retired shot list; it is the short list of genuine holes.

Ranked by how much documentation would otherwise have no picture:

| Surface | Doc weight | Why it looks like a real hole |
|---|---|---|
| **Event Editor** (F12) | 39 control rows, 237 lines | Its own window with its own canvas, curve types, LFO mode and MIDI CC import. Nothing in the set shows it. |
| **Key Binds window** | 210 rows, 459 lines | The densest reference surface in the app, six tabs. Manual 2's largest single chapter has no figure. |
| **Plugins page, a hosted VST3 loaded** | 16 rows | `Plugin Search.png` is the scan manager, not the tab. This is also the surface whose behaviour changed most at QA-Cleanup (window wraps the plugin, gray surround, clipping at right and bottom), so the manual describes something with no picture. |
| **Clips page** | 24 rows | Its own tab type with its own strip, rack and roll. No coverage. |
| **Undo History window** | 24 rows | Its own window and the whole Ctrl+Z story. |
| **Rusty Drums Map window** | - | `BaySickRustyDrums.md` calls it "the authoritative list" for whatever kit is installed, so the manual has to point the reader at a screen it never shows. |
| **MIDI Learn** | 21 rows | The learn window and the 30-second binding state. |
| **Export Project Bundle** | 9 rows | Small, but its size-confirmation is one of only TWO native Windows dialogs in the whole app, and the atlas teaches that native-vs-styled distinction. |

**Superseded sub-spec calls (kept for the record) follow.**

**SSC-8 (RESOLVED - WebView2, SC-14) - What renders the manual inside the in-app window?**

Facts checked in the tree, not assumed:

- Vendored JUCE is **8.0.12**. `juce_gui_extra` is already in the build, pulled
  transitively by `juce_audio_processors` (confirmed in the generated
  `JuceHeader.h` for all three artefact targets).
- `JUCE_USE_WIN_WEBVIEW2` defaults to **0** (`juce_gui_extra.h:118`), so today
  `WebBrowserComponent` falls back to the IE engine
  (`juce_WebBrowserComponent.h:74`).
- JUCE 8 exposes `Options::withNativeFunction` and `withEventListener`
  (`juce_WebBrowserComponent.h:320`), i.e. two-way JavaScript to C++ binding,
  but ONLY on the WebView2 backend.

**Recommendation: (a), WebView2, with the PDF as the universal fallback.**

The reasoning, since the question was which is most functional AND most
accessible:

- **Accessibility is already solved by the PDF.** SC-8 ships one regardless. Any
  user on any machine can read the manual with no runtime at all. That frees
  the in-app window to be the BEST renderer rather than the
  lowest-common-denominator one.
- **Runtime coverage is high and the gap is closeable.** WebView2 Runtime is
  preinstalled on Windows 11 and has shipped to Windows 10 through Windows
  Update and through Microsoft 365 / Edge for years. Where it is missing,
  Microsoft's Evergreen Bootstrapper is a ~2 MB redistributable an NSIS
  installer runs silently, which is exactly the mechanism QA-Installer is
  already building. Belt and braces: if the runtime is absent at runtime, the
  window opens the PDF in the system viewer instead of showing an error.
- **Functionality is not close.** WebView2 gives real text search, selectable
  and copyable text, working cross-manual hyperlinks, zoom, and correct modern
  CSS, all for free. Option (b) means reimplementing text layout, reflow,
  search, hyperlinks and image scaling in JUCE to end up somewhere worse, and
  duplicating work the HTML already does. Option (c) is an IE-era engine that
  cannot be trusted with modern CSS and is a dead end on current Windows.
  Option (d) loses search, selection and links outright, and blurs on zoom.
- **The one thing only (a) can do.** `withNativeFunction` lets the manual call
  back into the app. A callout in Manual 1 can open the actual page or window it
  describes. For a manual embedded in the application it documents, that is the
  feature worth having, and it is unavailable on every other option.

**Cost, stated honestly.** Vendoring the WebView2 SDK headers is a new
dependency in `libs/`, it needs a `/audit-licenses` pass (it is BSD-style
permissive, but that gets verified not assumed), and QA-Installer inherits a
bootstrapper step. That is the whole bill.

**This is Jeff's call to approve or overrule.** Nothing is built until he does.

---

## Superseded sub-spec calls (kept for the record)

**These are open and block execution. Surfaced in chat 2026-08-11; awaiting
Jeff's answers before any task body is treated as final.**

**SSC-1 - How do the callout numbers physically get onto the 42 images?**
This decides whether Manual 1 is writable by Claude at all. Claude cannot run
an image editor.

- (a) Manual 1 is HTML: each figure is the PNG with absolutely positioned
  numbered markers over it in CSS. Claude writes and can render/verify it.
  Ships as HTML plus a print stylesheet.
- (b) Claude emits an SVG overlay per image with the markers positioned, and
  the manual composites PNG + SVG. Same authoring model, more portable.
- (c) Claude emits a coordinates table per image and Jeff draws the numbers.
- (d) No drawn numbers: each figure gets a caption table keyed by on-screen
  label plus a position description ("top-left of the WAVEFORM box").

**SSC-2 - What format and where do the three manuals live?**

- (a) Markdown under a new `Manuals/` folder at repo root.
- (b) HTML under `Manuals/` (pairs with SSC-1 a).
- (c) Markdown under `Plans & Specs/Manuals/`.
- (d) Something else.

Note: `Plans & Specs/` section 0 scopes that folder to durable PROJECT
artifacts. A shipped user manual is a PRODUCT artifact that QA-Installer
bundles, which argues for repo root, but the call is Jeff's.

**SSC-3 - Does this batch build the in-app manuals window?**
The Main Plan section 5 entry says "Risk: low (no code changes)". The section 6
arrow says QA-Manuals is "the beginner manual + in-app help screens".
`StandaloneEditor.cpp:11766` carries `HOLD-FOR-MANUALS-WINDOW` on
`Help Index (F1)`, held for "the G5 manuals window", and Phase 7 IS now G5.

- (a) Documents only this batch; the window becomes its own later batch.
- (b) Documents plus the window in this batch.
- (c) Documents plus a minimal `Help Index` that opens the manuals in the
  system browser or PDF viewer.

**SSC-4 - Which stale images get reshot, and when?**
Four are verified stale (Synth OSC, Main frame, BaySickPlayer, Drum Kit) plus
whichever shows the Compressor mode list.

- (a) Jeff reshoots those five before Manual 1 body text is written.
- (b) Manual 1 is written now against the current images, with the five marked
  and reshot at the end.
- (c) Write Manual 1 from the corrected TEXT and treat the images as
  replaceable, reshooting at batch close.

**SSC-5 - Manual 3 depth for the ~30 pedal and amp "Style" DSPs.**
There are ~40 DSP modules. The 12 core effects carry real formulas. The ~30
`*StyleDSP` pedal and amp models are each modest but numerous, and the recon
flagged ~8 magic-number tables (mic archetypes, voicings, sync divisions) that
need transcribing out of the .cpp files.

- (a) Full treatment for all ~40, magic-number tables transcribed.
- (b) Full treatment for the 12 core effects and the engines; the ~30 Style
  DSPs get a one-paragraph-each summary table with no coefficient tables.
- (c) Full for the 12, summary for the 30, and the coefficient tables land in
  an appendix.

**SSC-6 - Does QA-Cleanup's working tree get committed before this batch
starts?** 106 modified Source files, 4 untracked `Safe*.h`, 1,809 `libs/eigen`
deletions are uncommitted. Five of the six named deltas exist only there.

- (a) Commit QA-Cleanup first; the manuals then describe HEAD.
- (b) Leave it; the manuals describe the working tree.

**SSC-7 - Are the four undocumented surfaces in scope?** Master Analyzer
window, version capture / captured takes, the Core Library fetcher
(`Options > Get Sound Content...`), and the Help menu items are covered by no
System Reference doc. Manual 2 needs them, which means writing them from code.

- (a) In scope; write them from code as part of Manual 2.
- (b) In scope, and back-fill the System Reference set first so the source of
  truth stays complete.
- (c) Out of scope; note them as gaps.

---

## Tasks

### Task 0 - Batch open

**PLAN APPROVED by Jeff 2026-08-11**, after MF-1 / MF-2 / MF-3 were fixed and
he verified all six scenarios pass. No open sub-spec calls.

**Pre-batch work already in the working tree, to be included in the batch-open
commit (Jeff's call 2026-08-11):** the MF-1 / MF-2 / MF-3 fixes below were done
during planning because the manuals could not honestly document the behaviour
they corrected. They are built green and runtime-verified but UNCOMMITTED. They
land in this batch's first commit, not a separate one.

| File | Why |
|---|---|
| `Source/Standalone/BuilderPage.cpp` | MF-1: `warnAboutOverwriting` on 2 save-mode choosers |
| `Source/Standalone/StandaloneEditor.cpp` | MF-1: 3 more choosers + the folder-overwrite guard; MF-2: async prompt chain + wording; MF-3: empty-blob guard |
| `Source/Standalone/ProjectBundler.cpp` | MF-2: hoisted exclusion rule, `estimateBundle`, post-write totals |
| `Source/Standalone/ProjectBundler.h` | MF-2: `Estimate` struct, `Result::totalBytes` / `totalFiles` |
| `Plans & Specs/System Reference/Pictures/*` | 3 replaced + 11 new figures |
| `Plans & Specs/Batch Plans/` + `Running Notes/lucid-annotating-lemur.md` | this plan and its notes |

- [x] Create this plan file and the paired running notes.
- [x] SSC-1 through SSC-7 resolved 2026-08-11, recorded as SC-7..SC-13.
- [x] Figure set verified on disk: 45 files, three replaced in place and three
      added, real filenames checked against the folder.
- [ ] **Jeff approves this plan.** Nothing below starts until he does.
- [ ] Resolve SSC-8 (manual renderer) as part of that approval.
- [ ] WAIT for QA-Cleanup's commit to land in the other session (SC-12), then
      record its hash in the running notes as the tree the manuals describe.

### Task 1 - Correct the source of truth

The System Reference set is Manual 2's raw material and it is two commits plus
one working tree stale. Fix it BEFORE writing from it, so every downstream
sentence inherits a correct source.

- [ ] Apply the seven unlisted changes from `485499ae` (Lead removal, gain
      staging, BaySickPlayer vibrato relabel, clip vibrato, Compressor
      `CS Style` to `Pedal`, Harmless RM VOL taper, Edit menu rebuild) to every
      affected doc. Grep each old string tree-wide rather than editing the
      doc you first think of; the Compressor rename alone hits 8 sites across
      three docs.
- [ ] Apply the six DELTAS-block areas WITH the corrections in the recon table
      above, not as the block words them.
- [ ] Fix `Effect Modules.md:41`. "Six modules consume a sidechain" is now
      wrong: hosted VST3 effects that declare a sidechain input do too
      (`HostedPluginEffect.h:53-55`). `Effect Racks.md` and `Plugins Page.md`
      need the same addition wherever they describe the sidechain pick.
- [ ] Add MP3 to the documented import formats where the set is listed, and
      note the full set is WAV / AIFF / FLAC / Ogg / MP3.
- [ ] Harvest and transcribe the exact user-facing strings the manuals must
      quote verbatim: the `SafeSfzKit.h` / `SafeNamModel.h` reason strings, the
      carrier sentences they are embedded in, the four refusal dialog titles
      and bodies, and the Mixer hamburger's four current items.
- [ ] Fix the four stale source comments listed in the recon section. These are
      wrong comments, not style, so Rule 6's edited-regions scoping does not
      apply.
- [ ] Retire the `SHOT-###` ids in `MANUAL-1 Screenshot List.md` with a header
      note recording that the file is now the element inventory (SC-2), so the
      next reader does not repeat the mistake this session made.
- [ ] Build gate.
- [ ] Tell Jeff: nothing to ear-test; this task is documents plus four comment
      fixes. Confirm the build is green and the diff is documents-only apart
      from those comments.
- [ ] `/draft-doc running-notes` and apply.

### Task 2 - Allocate every id and build the registry

- [ ] Assign the 56 screen codes per the table above.
- [ ] Walk each image, enumerate its visible elements against the element
      inventory, and assign callout ids. Apply the within-image and
      across-image collapse rules.
- [ ] Allocate `IMP-<n>` ids across the ~40 DSP modules and ~24 architecture
      subsystems (depth per SSC-5).
- [ ] Emit `Plans & Specs/System Reference/Callout Registry.md`.
- [ ] Tell Jeff: review the screen codes and spot-check three images' callout
      lists for anything missed or mislabelled.
- [ ] `/draft-doc running-notes` and apply.

### Tasks 3-6 - Manual 2, in four chunks following the INDEX groups

- [ ] Task 3: the shell and system pages (Workspace, Transport, Keyboard
      Shortcuts, Projects and Saving, Undo History, MIDI Learn, Builder,
      Patterns and Arrangement, Piano Roll, Event Editor, Automation).
- [ ] Task 4: the instruments (Harmless, BaySickSynth, BaySickBass,
      BaySickPlayer, BaySickGuitars, BaySickBasses, BaySickRustyDrums, plus the
      Engine Tabs / Clips / Inst / Plugins / Vox tab-family docs).
- [ ] Task 5: mixing, effects and tone (Mixer, Effect Racks, Effect Modules,
      EQ, Pedalboard, NAM Amp and Cab).
- [ ] Task 6: the vocal chain plus content, files and output (Vocal Chain,
      Pitch Editor, Align Editor, Sample Library, Presets, Templates, Project
      Bundles, Freeze and Export), plus the SSC-7 surfaces if in scope.

Each of Tasks 3-6:
- [ ] Every control entry headed by its callout id, with `Fig.` back-refs and
      `IMP-` forward footnotes.
- [ ] Tell Jeff: read one chapter cold and confirm it teaches the surface.
- [ ] `/draft-doc running-notes` and apply.

### Task 7 - Manual 3, architecture and signal flow

- [ ] The ~24 subsystems. The recon flagged eight that need real investigation
      rather than header-reading: the per-strip processing ORDER inside
      `BaySickGraph.cpp` (3,277 lines), the per-block sequence in
      `PluginProcessor::processBlock`, the `project.xml` shape (only ~40%
      knowable from `serializeProject`), automation lane coverage incl. which
      lanes have `applyOfflineLaneValue` branches, freeze staleness and the
      FNV-1a content stamp, the plugin bridge's failure and recovery paths, the
      idle-suspend predicates, and `settleAudioThread`'s acknowledgement
      protocol.
- [ ] Correct naming: the class is `BaySickGraph`; only the PluginProcessor
      member is still `mVibeGraph`. Do not write "VibeGraph" as a subsystem
      name.
- [ ] The undo manager lives on the PROCESSOR (`PluginProcessor.h:277`), not
      where CLAUDE.md's ArrangementGrid note implies.
- [ ] The plugin bridge chapter must state that audio and MIDI do NOT ride the
      protocol: `Process` (4) and `ProcessReply` (103) are reserved, and
      per-block data crosses via a named file mapping plus two named events.
- [ ] The `Safe*` chapter covers FIVE files, not the four the DELTAS block
      names. `SafeAudioFormats.h` is the single audio-format registration point
      that replaced `registerBasicFormats()` at 19 sites across 11 files.
- [ ] The audio-decode chapter states that MP3 decoding comes from the vendored
      LAME tree via `MpglibAudioFormat`, with JUCE's own decoder switched off by
      `JUCE_USE_MP3AUDIOFORMAT=0`, and why (an unaudited hand-ported decoder on
      an untrusted-input path).
- [ ] The hosted-plugin chapter covers the net-new sidechain path
      (`HostedPlugin.h:69` `hasSidechainInput`, `setSidechainSource` borrowing
      BaySickGraph's per-strip `scRecv` buffer for exactly one call).
- [ ] Tell Jeff: confirm the architecture chapters match how you understand the
      app works.
- [ ] `/draft-doc running-notes` and apply.

### Task 8 - Manual 3, the DSP

- [ ] Per SSC-5 depth. The user-facing name authority is
      `SlotComponent::effectTypeName` (`SlotComponent.cpp:1246-1302`); the
      browsable list is `showEffectPicker` (`:715-793`) plus
      `BaySickPedalsEditor.cpp:407-494`. The recon flagged that these two
      disagree, so reconcile before naming anything.
- [ ] Transcribe the flagged magic-number tables if SSC-5 lands on (a) or (c).
- [ ] Two modules need their .cpp read before they can be described honestly:
      `SynthStyleDSP` (its header claims monophonic; verify) and
      `OctaveStyleDSP` polyphonic mode.
- [ ] `/draft-doc running-notes` and apply.

### Task 9 - Manual 1 body

- [ ] Figure pages: image, callout markers per SSC-1, caption table mapping
      callout id to on-screen label to Manual 2 section.
- [ ] State variants as insets and captions, not as new figures.
- [ ] The orienting prose: the contained-window model, the fact that closing a
      window does not stop the engine, and the APPENDIX material (invisible hit
      targets, live-looking controls that do nothing, the one-per-app
      elements).
- [ ] Marker coordinates are percentages of image width/height in a per-figure
      data block (SC-7), never baked into pixels.
- [ ] Ship the drag-to-nudge authoring mode on the HTML page so Jeff can move
      any marker and copy the corrected coordinates back.
- [ ] `Help Index (F1)` OPENS THE MANUALS. Document it as live. Do not repeat
      the retired "has no handler by design" line from the element inventory.
- [ ] `/draft-doc running-notes` and apply.

(The HTML output and the PDF are NOT here. They cover all three manuals, so
they live in Task 10 below, after every manual exists.)

> **Task 10 was REMOVED from this batch on 2026-08-11 (Jeff's call)** so that
> QA-Manuals contains no code. **The window half is BUILT AND SHIPPED** ahead of
> this batch, in the same working tree (see "The manuals window, already built"
> above). The PDF half is not built and cannot be until the HTML exists; it is
> an authoring output of this batch, not application code.

### Task 10 - Cross-reference integrity pass, then ship the output

The integrity check runs first: shipping a broken cross-reference is worse than
shipping late, because a dead footnote is invisible until a reader follows it.

**Output covers ALL THREE manuals**, which is why it sits here rather than
inside any one manual's task.

- [ ] Every callout id referenced in Manual 2 resolves to a real marker in
      Manual 1.
- [ ] Every `IMP-` footnote in Manual 2 resolves to a real Manual 3 section.
- [ ] Every Manual 3 back-reference resolves to a real callout id.
- [ ] No id appears twice; no id is skipped without a retirement note.
- [ ] Mechanical check, scripted, not eyeballed.

**Then ship it. Both outputs come from the ONE HTML source (SC-8), so they
cannot drift:**

- [ ] **Write all three manuals to `<exe dir>/Manuals/`, entry point
      `index.html`** - the exact path `ManualsWindow::manualsIndexFile()`
      resolves. The window is already built and is waiting on this file. Verify
      by pressing F1 in the app: it must render the manuals, not the
      "not installed" placeholder.
- [ ] Stage into BOTH build configs (Release and Debug), the way
      `CMakeLists.txt` stages `Resources/` and the plugin-host helpers. A
      manual that only exists next to one exe is a manual half the testers
      cannot open.
- [ ] **Generate the PDF from that same HTML.** This is the half of the old
      Task 10 that could not be built before the content existed. Confirm the
      callout markers land in the same place in both, since they are positioned
      as percentages and a print stylesheet can reflow them.
- [ ] Decide with Jeff where the PDF lives for shipping - QA-Installer bundles
      the manuals, and this is the batch that produces them.
- [ ] `/draft-doc running-notes` and apply.

### Task 11 - Batch close

- [ ] `/draft-doc batch-close`.
- [ ] `/review-batch QA-Manuals`.
- [ ] Apply the draft via Edit in the parent session.
- [ ] Commit the close separately from the content commits.

---

## Verification (end-to-end)

1. Open Manual 1 at any figure, pick a control, follow its callout id into
   Manual 2, and confirm the entry describes the thing pointed at.
2. From that Manual 2 entry, follow the `IMP-` footnote into Manual 3 and
   confirm it explains the same control.
3. Pick five controls Jeff knows well across different pages and confirm range,
   default and behaviour match the app.
4. Confirm the Tape panel section names the knobs `Vibe`, `Hyst`, `Bias` and
   describes Advanced as a MODE toggled from the effect window menu, not a tab.
5. Confirm no manual anywhere says "VibeDAW", "Vibesynth", or renames the Tape
   knob.
6. Confirm the refused-file section quotes the real strings and does not
   promise a reason where the code shows none.
7. Confirm `plugins_scan_crashes.txt` is documented at
   `Documents\BaySickDAW\`, and that the retry advice matches the self-clearing
   behaviour rather than telling the user to hand-edit.
8. Press **F1 in the app**. The manuals window opens and renders the HTML this
   batch produced, not the "not installed" placeholder. That is the end-to-end
   proof the output landed at `<exe dir>/Manuals/index.html`.
9. Open the generated **PDF** outside the app and confirm it matches the in-app
   copy, including callout marker positions.

---

## Defect handling during execution

**There is no routing. V1 code is closed** (Jeff, 2026-08-11), so a defect the
manuals surface has no later batch to go to. It is fixed in this batch or it
ships and the manual documents it truthfully, and for anything that costs a user
their work the second option is not available.

- Every defect gets an `MF-<n>` entry: the site, the sentence the manual would
  have to contain if we shipped as-is, and the verdict. That sentence is the
  severity test.
- Fixes land in Task 10 with their own build gate and their own verify script.
- Already open from the recon, to be triaged the same way when their chapters
  are written: the rack-slot plugin window's ~4 px crop
  (`EffectWindows.cpp:148` passes exact w/h while `SlotComponent.cpp:614` lays
  out at `reduced(2)`), and the drum-kit vertical scrollbar's unset
  `setSingleStepSize` (wheel over the bar moves ~1 px per notch instead of a
  row).
- Wrong COMMENTS get fixed in place per
  `feedback_no_docs_only_commit_fix_wrong_comments`.
- A gap that is a genuinely MISSING FEATURE rather than a defect still goes to
  Future State, since that is a v2 scope decision and not a v1 correctness one.
  Jeff makes that call, not the plan.

---

## Carry-Forward Reference touch points

- Section 6 (Patterns To Reuse) at Task 7 start, for the architecture chapters.
- Section 8 (Anti-Patterns) throughout, especially "Read code before calling
  something expected" and "Don't speculate about FL Studio behavior".
- Note that the Carry-Forward is FROZEN as of 2026-05-07 and the System
  Reference set plus the code win wherever they disagree. Every System
  Reference doc already carries a "Differs from Carry-Forward" section
  recording exactly where.
