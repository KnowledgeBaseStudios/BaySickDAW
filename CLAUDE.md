# BaySickDAW — Claude Working Guide

## Plans & Specs Location (READ FIRST)

All planning documents live in `Plans & Specs/` at the repo root:
- `Plans & Specs/Main Plan.md` — master sequencing of work, every batch and phase.
- `Plans & Specs/Carry-Forward Reference.md` — frozen architectural snapshot from 2026-05-07.
- `Plans & Specs/Implemented Work Log.md` — running ledger of QA-era execution (2026-05-07 onward).
- `Plans & Specs/Previously Implemented.md` — historical record of pre-QA build work.
- `Plans & Specs/Future State.md` — post-V1 roadmap.
- `Plans & Specs/Batch Plans/<silly-name>.md` — per-batch implementation plans.

Per the three-doc system in Main Plan §0, every per-batch session starts by
reading Main Plan + Carry-Forward + Implemented Work Log.  See Main Plan §0
Rule 3 for the convention on how findings discovered during execution get
routed at batch close.

---

## Working Rules (standing — Main Plan §0 Rules 6-9 is authoritative)

Four standing rules govern how I work here (full text + the existing
Rules 1-5 live in Main Plan §0):

- **Rule 6 — Comment Policy.** Comments only for the six keeper
  categories: (1) architectural why, (2) real-time audio-thread danger
  zones, (3) DSP/domain references, (4) framework quirks/workarounds,
  (5) magic-number calibrations, (6) thread-safety/ownership.  Never
  narrate WHAT the code does — the code is the single source of truth.
  When editing code, clean non-conforming comments in the regions you
  touch (edited regions only, never a whole-file audit).  No retroactive
  mass strip.
- **Rule 7 — Communication: direct, no cheerleading.** No "that's
  absolutely right" / "great question."  Say when an idea is flawed,
  incomplete, or half-baked.  Casual tone, occasional profanity when it
  fits.  Practical over positive.
- **Rule 8 — Technical approach: challenge assumptions.** Surface the
  hard questions (implementation, scalability, real-world viability); if
  something won't work, say so and explain why.  Challenging is not
  deciding — spec calls still go to Jeff (Rule 5).
- **Rule 9 — Commit messages stay brief.** Files/areas touched +
  base-level what-was-done; no narrative (that lives in the in-repo
  docs).  Skip `/draft-commit`; write the one-liner directly.  See
  `## Git Commit Mechanics` below.

---

## Subagents (added 2026-05-08)

BaySickDAW-specific subagents live at `.claude/agents/`, with slash
commands at `.claude/commands/`. Use these instead of doing the work
inline — they save context budget and produce more consistent output.

| Slash command | Agent | When to use |
|---------------|-------|-------------|
| `/read-doc <query>` | `doc-reader` | Pull a section from a Plans & Specs doc without loading the full file (e.g., `Previously Implemented.md` is 262 KB — grep through the agent instead). |
| `/draft-doc <mode> <context>` | `doc-drafter` | Compile batch close entries / §9 Forks entries / Future State additions. Returns proposed text in a code block; never autonomously edits Plans & Specs/. Modes: `running-notes` / `batch-close` / `forks-entry` / `future-state`. |
| `/review-batch <batch-id>` | `batch-code-reviewer` | At batch close, before final commit. Checks diff against plan + CLAUDE.md rules + canonical conventions + memory-tracked gotchas. Outputs BLOCKER / NEEDS-FIX / NIT findings. |
| `/test-signal <module>` | `dsp-test-signal` | Generate validation test plan for a DSP module. Useful during 5F-9 DSP Quality Pass batches and any new DSP work. |
| `/preset-gaps` | `preset-coverage-mapper` | Audit factory + user preset library for genre / instrument-family gaps. Useful before QA-Templates batch. |
| `/research [focus area]` | `competitive-research` | One-shot competitive sweep before milestones. Outputs draft Future State entries with verified-source confidence ratings. Run sparingly. |
| `/architecture <topic>` | `daw-architecture-research` | Research how other DAWs solve a specific audio-engine architecture problem (threading, FFT planning, voice mgmt, lock-free patterns, etc.). Returns comparative analysis with implementation recommendation. On-demand only — pre-architecture-decision or pre-milestone. |
| `/audit-security` | `security-auditor` | Audit handling of UNTRUSTED input (project/preset XML, sample/SFZ/IR/NAM files, hosted plugin binaries, the Core Library fetcher). Read-only; correctness is not its job. Pre-release, or when a new input surface lands. |
| `/perf-audit` | `performance-auditor` | Recurring scan of BaySickDAW codebase for performance opportunities (audio-thread allocations, SIMD candidates, lock contention, FFT plan reuse, APVTS dirty-flag compliance, etc.). Context-aware — won't flag prepare-time allocations as audio-thread issues. Run every 3 batches OR pre-milestone. |

**Cross-project** agents also available (live at `~/.claude/agents/`):

| Slash command | Agent | When to use |
|---------------|-------|-------------|
| `/diagnose-build [log]` | `build-error-diagnoser` | When `do_build.bat` fails. Identifies likely cause + ranked fix candidates. |
| `/explain <concept>` | `concept-explainer` | Hit an unfamiliar concept. Returns explanation calibrated to the "I debug if walked through" learning style, with codebase examples where they exist. |
| `/standup` | `standup-summarizer` | Session start / end of day. Done / Next / Blocked summary from git log + plan docs. |
| `/extract-spec` | `spec-extractor` | After a long planning discussion — convert it to a structured spec doc. |
| `/audit-licenses` | `license-auditor` | Pre-release sweep. Vendored libs + addons + asset attribution check. |

**Key rule:** the doc-drafter operates in **drafter-only mode** — it
returns proposed text, the main session reviews and applies via Edit.
Same goes for any agent that touches Plans & Specs/. Past blast-radius
incidents (the 6-of-10 buckets mistake on 2026-05-08) confirm this is
the right boundary.

**Orchestration rules** — when to dispatch which agent — live in
`Plans & Specs/Main Plan.md` §0 "Agent Orchestration Rules" (locked
2026-05-08).  Every BaySickDAW session reads those rules at start.
Highlights:

- **Session open** → `/standup`
- **Mid-batch checkpoint** → `/draft-doc running-notes`
- **Build failure** → `/diagnose-build`
- **Every 3 batches OR pre-milestone** → `/perf-audit`
- **Before architecture decision** (multiple plausible approaches) → `/architecture <topic>`
- **Concept blocker** → `/explain <concept>`
- **Pre-commit (Rule 9)** → write the brief one-liner directly, skip `/draft-commit`
- **Batch close (mandatory)** → `/draft-doc batch-close` → `/review-batch` → apply draft → commit
- **Pre-release** → `/audit-licenses` + `/audit-security` (Tier 1)
- **New way of reading someone else's file lands** → `/audit-security`
- **Pre-milestone** → `/research [focus]` (one-shot per focus)

Anti-rules + the full table live in Main Plan §0.  When in doubt, ask
the user before dispatching.

---

## Project Overview
JUCE 8 C++ music production app (vendored JUCE 8.0.x; formerly Vibesynth, then VibeDAW). **Standalone Windows app only** — no VST/plugin version planned; a legacy `juce_add_plugin` target still exists in CMake but is not shipped. Future platform plan: tablet "DJ Party" variant, still not a VST.
**App name:** BaySickDAW by KnowledgeBase Studios
**Target audience:** people who have never made music before.
**User-facing engine names:** BaySickSolstice, BaySickPlayer (sample player — internal source is still `VibePlayer*`; class / file renames deferred), BaySickSynth, BaySickBass. Drums are now per-tab engine instances (Phase D dynamic-drum architecture, 2026-04-25) — each Drums tab owns one BaySickPlayer or BaySickSynth instance; the legacy monolithic `BaySickDrums` engine was deleted. Since QA-ModelShell (2026-07-31) the app also HOSTS third-party VST3 plugins — effects in rack slots (`EffectType::VST3Plugin`) and instruments as Plugins tabs — still standalone-only as a product.

**Owner:** Jeff — professional FL Studio user. Technically capable. **Claude runs the builds** (changed 2026-07-25 — see `## Build System`). Jeff still owns every spec call, per-batch commit approval, and all in-app / ear verification.

---

## Build System

- **Build command:** Run `do_build.bat` from `C:\Users\jeffm\Documents\BaySickDAW\`. Builds BOTH Release and Debug per QA-0a (2026-05-07).
- **Who runs it: Claude (changed 2026-07-25).** Supersedes the old "Jeff runs builds himself — never try to run do_build.bat in bash (MSVC env not available)" rule, whose premise was stale: `do_build.bat` is self-contained — it resets `PATH` to a bare minimum and calls `vcvars64.bat` itself (lines 3-4), so it does NOT need the caller to have an MSVC environment. Verified working 2026-07-25 (`MSBuild version 18.7.8` in the log, both exit codes 0).
  - **Invocation (pin this string verbatim — it is the allowlisted one; any variation re-prompts):**
    `cmd.exe /c "C:\Users\jeffm\Documents\BaySickDAW\do_build.bat"`
    via the PowerShell tool with `run_in_background: true` (a full rebuild exceeds the 10-min synchronous tool cap).
    **Keep it a SINGLE statement** — no `;`, no `&&`, no `$VAR`. A compound command cannot be reduced to a reusable allow rule, so "always allow" silently fails to match the next run and Jeff gets prompted every single build. (Learned the hard way 2026-07-25: the original string appended `; Write-Output "WRAPPER_EXIT=$LASTEXITCODE"`, which was useless anyway — see the next bullet.)
  - **Read the result from `build_log.txt`**, not the wrapper exit code (it reported 0 twice while the log said failure): SIX exit codes must ALL be 0 — `RELEASE_EXIT_CODE`, `DEBUG_EXIT_CODE`, `HELPER64_EXIT_CODE`, `HELPER32_CONFIG_EXIT_CODE`, `HELPER32_EXIT_CODE`, `ARTEFACTS_EXIT_CODE` — plus FOUR `vcxproj -> ....exe` link lines (two `BaySickDAW.exe` + `BaySickPluginHost64.exe` + `BaySickPluginHost32.exe`). Grep for `error C` / `error LNK` / `error MSB`; do not dump the whole log. (The plugin-sandbox helpers build x64 into `build\` and x86 into `build32\` — QA-ModelShell TS6.)
  - `ARTEFACTS_EXIT_CODE` (added 2026-08-10 with the portable-build rewrite) is NOT a compiler result: it checks that the six exe files the app needs at runtime are actually ON DISK. That is the case the five compiler codes cannot see — a locked-exe link failure leaves stale objects and still reports success. Note the four link lines and the six files are different sets: the helpers link into `BaySickPluginHost_artefacts`, and a `CMakeLists.txt` POST_BUILD step stages each into BOTH configs of `BaySickDAWStandalone_artefacts`, which is where the app loads the bridge from. Helper exes carrying an older timestamp than the main exes is CORRECT when the helper sources did not change — `copy_if_different` is doing its job, not skipping work.
  - **Cadence:** one build gate at the end of EVERY task (the G3 exemplar `Plans & Specs/Batch Plans/silky-gliding-lynx.md` is canonical). Bulk-run's "no per-task verify pauses" retires the per-task FUNCTIONAL test + running-notes checkpoint ONLY — it never retired the compile gate.
  - **Exe-lock convention:** do not build while Jeff has `BaySickDAW.exe` open (Debug or Release) — the link step fails on the locked file. He says when he is in the app.
  - **Hard stop:** if the build fails for any reason that is NOT the code under edit (env, tooling, locked exe), stop and hand it back to Jeff. Do not debug the harness — that failure mode is what created the original rule.
  - **Unchanged:** Jeff still runs the smoke (Debug exe FIRST, screenshot any jassert, then Release), all ear checks, and approves every commit.
- **Release exe:** `build\BaySickDAWStandalone_artefacts\Release\BaySickDAW.exe` - the shipping binary, used for music production.
- **Debug exe:** `build\BaySickDAWStandalone_artefacts\Debug\BaySickDAW.exe` - the diagnostic binary, used for verifying fixes. Window title shows `[DEBUG]` suffix. Same embedded icon (JUCE+VS multi-config gotcha; differentiation via window title only).
- **Build dir:** `C:\Users\jeffm\Documents\BaySickDAW\build\`
- **Build log:** `build_log.txt` at repo root. Five exit codes (see above). Any non-zero = that piece failed. An exe-locked link failure can leave STALE OBJECTS — judge the NEXT build by the error grep AND the link-line count, not exit codes alone.
- **Header dependencies:** handled automatically by MSBuild. No manual `.obj` deletion after header edits - just re-run `do_build.bat`.
- **Standing rule (verifying Claude fixes):** run the Debug exe FIRST. Any `jassert` that fires shows a Windows dialog with file path + line + condition - screenshot to share. Then re-run in Release as the actual user test. Debug runs slower; audio that glitches in Debug under heavy load may be fine in Release. Always confirm in Release before declaring a real performance regression.
- **Don't run both simultaneously.** ASIO opens audio devices exclusively (second instance gets no audio). Both exes share `Documents\BaySickDAW\settings.xml` + `audio_settings.xml` - changes in one are seen by the other on next start.

---

## Git Commit Mechanics

- **Commit messages stay brief (Main Plan §0 Rule 9, adopted 2026-06-24 at QA-Rules).** Only the files/areas touched + base-level what-was-done. NO multi-paragraph narrative — the full narrative lives in the Implemented Work Log + running notes, so duplicating it in the commit body just doubles the work + tokens. Format: `<Batch> Task N: <one-line what> (<scope>)` + a `Co-Authored-By` trailer.
- **Skip `/draft-commit`.** Write the one-liner directly, surface the message + full `git status`, commit only after explicit approval. (Reverses the former every-commit-via-drafter rule — with no long narrative left, there's nothing to keep uniform.)
- **Mechanic: `git commit -m`.** Use `git commit -F .git/COMMIT_EDITMSG_<batch>-<task>.txt` (then `rm <that file>`) ONLY when the brief one-liner carries a quoting/encoding hazard — a `§` glyph, apostrophes, backticks, or `$`/`&`/`<`/`>`. The Bash tool harness's quoting layer mangles those; `-F` reads the file verbatim, bypassing all shell parsing of the message body.
- **Supersedes** the prior multi-paragraph-technical-narrative convention (adopted 2026-05-25, retired 2026-06-24 at QA-Rules).

---

## Next Steps

**Do NOT record the current position here.** A hardcoded "current batch" in this
file goes stale within days and then actively misdirects — this section sat on
"next batch: QA-Md" for nearly three months after QA-Md closed. The live position
lives in the plan docs, which are updated as work happens:

- **Where we are + what's next:** `Plans & Specs/Main Plan.md` §5 (per-batch entries)
  and §6 (sequencing arrow).
- **What's already shipped:** `Plans & Specs/Previously Implemented.md` (pre-QA era)
  and `Plans & Specs/Implemented Work Log.md` (QA era onward).
- **What's deferred / dropped / post-v1.0:** `Plans & Specs/Future State.md`.
- **The active batch's own plan + running notes:** `Plans & Specs/Batch Plans/<silly-name>.md`
  and `Plans & Specs/Running Notes/<silly-name>.md`.

Read those at session open (Main Plan §0 Rule 1) rather than trusting any summary here.

---

## Key Technical Notes

### JUCE Gotchas
- `TabbedComponent` has NO `addChangeListener` (it is not a ChangeBroadcaster) -- override the virtual `currentTabChanged` or poll the index yourself
- `juce::Path::addArc()` starts a NEW subpath — breaks filled paths. Use `lineTo` loop with cos/sin.
- `juce::LookAndFeel_V4::drawLabel()` override required to force text color when VKnob calls `label.setColour()`
- `juce::dsp::IIR::Filter<float>::coefficients` starts NULL — pointer copy only (`mFilter.coefficients = c`), NEVER deref
- `juce::dsp::FFT` not default-constructible — must be in constructor init list: `: mFFT(kFFTOrder)`
- Notch filter: `Coeffs::makeNotch(sr, freq, q)` — NOT `makeNotchFilter`
- `juce::Font::Font(size, style)` generates C4996 warnings — harmless, ignore
- `CharPointer_UTF8` + `String` concatenation: wrap in `juce::String(juce::CharPointer_UTF8("..."))` before `+`
- `juce::dsp::StateVariableTPTFilter<float>`: `resonance` parameter = Q factor directly. Q=0.001 with fc=20kHz gives -34dB at 1kHz — effectively silent. Butterworth Q=0.7071 is the correct "transparent" default.
- `juce::dsp::StateVariableTPTFilter` highpass at 20kHz BLOCKS all audible audio. If using filter type as "bypass", initialize as **lowpass** at 20kHz instead (transparent when fully open).
- `setSize()` triggers `resized()` BEFORE unique_ptr members are constructed when called in an editor constructor. Always null-guard: `if (mFoo) mFoo->setBounds(...)`.
- `constexpr const char*[]` out-of-class definition causes C++17 MSVC compile error. Define inline in the header only; do NOT repeat the definition in the .cpp.
- `addChildComponent()` adds a child WITHOUT making it visible (preserves setVisible(false)). `addAndMakeVisible()` unconditionally sets visible=true.
- `juce::PopupMenu` has no `isEmpty()` method — use `getNumItems() > 0`.
- LCG noise generator `x * 1103515245.0 + 12345.0` overflows double in ~35 samples. Wrap with `std::fmod(..., 4294967296.0)` every iteration.
- `Component::addKeyListener` dispatch is REVERSE registration order (`ComponentPeer::handleKeyPress` iterates `for (i = size(); --i >= 0;)`), and listeners run BEFORE the component's own `keyPressed`. Highest priority = registered LAST. Also asymmetric: `handleKeyUpOrDown` runs the VIRTUAL first, then listeners (still reversed).

### BaySickSolstice-specific
- Internal `BaySickSolsticeCurvePoint { float time; float value; int curveType; }` is defined inside `BaySickSolsticeModEditor` — deliberately NOT shared with `PatternManager::ControlPoint`. BaySickSolstice modulation is per-note 0-1 phase; PatternManager automation is tick-based song position. Different domains.
- `BaySickSolsticeLAF::kBipolar` property on a Slider makes the ring-glow knob render from 12 o'clock outward (left or right). Set via `slider.getProperties().set(BaySickSolsticeLAF::kBipolar, "true")`.
- `BaySickSolsticeEditor::rebindToPart()` swaps the whole `mDualSliders` / `mDualButtons` set (8 sliders + 1 button) between their Part A and Part B params at runtime by destroying and recreating the attachment, and (QA-ApvtsAutomation 2026-07-25) re-points each control's componentID at the visible part. Their slider range is only valid once an attachment exists. **Part A and Part B are simultaneous layers, not alternate modes** — both always render, `timbre_blend` crossfades them, and `part_sel` is editor view state that no DSP reads. So each part's param owns an independent automation lane: `StandaloneEditor::registerModelEngineAutomation` walks the engine's whole parameter list off engine creation, so BOTH part ids get applicators that write the PARAMETER, not the shared control — a control-driven applicator would write whichever part is bound and collapse two lanes onto one target. `part_sel` is deliberately unstamped (no Automate menu).

### Drum sample root note
- `VibeSampleManager::detectRootNote()` parses filename numbers as MIDI notes. `Kick_01.wav` → rootNote=1 → 30× pitch. Drum pages must pass `normalizeRoot = 60` to `loadSampleFolder()` / `loadSampleSFZ()` (VibePlayerProcessor forwards it to `normalizeRootNotes`): voice pitch is `midiNote - rootNote`, so root 60 makes a drum play at its recorded pitch on the default trigger note, with the per-drum `mixer_drum_{N}_playNote` (default 60) transposing from there.
- `loadSingleFile()` already initializes rootNote=60.

### Model-owned engines (EngineRig) — QA-ModelShell 2026-07
- `Source/EngineRig.h/.cpp` owns every dynamic-tab engine, keyed `(TabKind, pageIndex)` over Layers/Bass/Drums/Clips/Vox/Inst/Plugins. Pages are NON-OWNING VIEWS — a page dtor touches only its editor; engines keep running when windows close. The sfizz trio (Guitars/Basses/RustyDrums) stays PROCESSOR-owned on its own race-safe load paths.
- Teardown order is load-bearing: `rig.removeTab` runs BEFORE `destroyBaySickGuitars/Basses` (the rig-owned Inst chain holds the spliced sfizz stage pointer; one audio block in the wrong order = use-after-free).
- ALL automation registration is model-side (engine creation events, `onMixerStripParamsCreated`, `onSfizzEngineReady`, rack/pedal model events). Applicators re-resolve their target THROUGH the model at apply time. NEVER register automation against a widget — destroy-on-close windows make every widget-scoped registration a guaranteed dead lane. Any NEW lane class needs its live registration AND an `applyOfflineLaneValue` branch in the same pass, or it plays live and vanishes from exports.

### Contained-window shell (WorkspaceWindow) — QA-ModelShell 2026-07
- Pages live in `WorkspaceWindow`s — REAL native child windows (WS_CHILD) inside the fixed fullscreen main frame. A child peer positions AND reads back in the PARENT'S CLIENT space (contract documented in `WorkspaceWindow.h`); persistence stores workspace-local bounds.
- **Z-ORDER TRAP (silent):** a native child peer always renders above anything drawn into its parent — `setAlwaysOnTop` does NOT cross the drawn/native boundary. Any drawn overlay meant to cover the workspace must be promoted to its own desktop window (the app's ONE `VibeTooltip` is parentless for exactly this reason -- a parented `TooltipWindow` draws inside the frame's client area and so lands under every child peer; per-window tooltips would double up, because a parentless tip's peer gate always passes). A progress surface that paints while the message thread is blocked must ALSO select the Software Renderer on its own peer — JUCE 8's Direct2D `performAnyPendingRepaintsNow()` is an EMPTY override.
- Peer-keyed suspend convention: repeating UI work (vblank drains, page poll timers) starts/stops in `parentHierarchyChanged()` keyed on `getPeer() != nullptr` — never in constructors (pages can exist unframed). Page destruction on window close is OFF (Jeff's measured ruling; the editor's cached raw page pointers, `mMixerPage` etc., are unguarded).

### True offline export / render path — QA-ModelShell 2026-07
- The LIVE processor renders itself: `beginOfflineRender(sr, blk)` / `endOfflineRender()` (suspend + `setNonRealtime` sweep over every engine + graph reset + full re-prepare + restore set; one render at a time by compare-exchange). `BuilderPage::runOfflineLoop` is the ONE render core — export, `measureRender`, and freeze renders all consume it; never copy the loop.
- `isNonRealtime()` gates: metronome, recorders, DSP load meter, `markEngineContentChanged` / `markAllFreezesStale` (an automation REPLAY is not a user edit — without these guards frozen tabs re-render in a cascade), transport-edge publishes. `AudioClipStreamer::sOfflineRender` switches clip reads to blocking (offline outruns prefetch by design).
- Freeze: files in `<project>\Freeze\`, per-tab song + per-pattern renders, FNV-1a content stamp for restore reuse, substitution via `FrozenSourceRead.h`; frozen tabs substitute SILENCE while the transport is stopped; staleness RETRACTS published pointers.

### Engine audition pattern
- All 7 engine processors (BaySickSynth, BaySickBass, BaySickSolstice, VibePlayer, BaySickGuitars, BaySickBasses, BaySickRustyDrums) have `auditionNote(int midiNote)` + `std::atomic<int> mAuditionNote { -1 }`.
- `processBlock` opens with `int n = mAuditionNote.exchange(-1); if (n >= 0) { ...noteOff-any, noteOn n... }`.
- Page audition callbacks resolve engines THROUGH the rig (pages are views; do not cache engine pointers): Layers / Bass / Drums cascade the legacy engine-type casts (all silent on cast failure). Inst tabs (BaySickGuitars / BaySickBasses) wire piano-roll keyboard clicks to `eng->auditionNote(n)` via `EngineConnection::auditionMomentary`; BaySickRustyDrums wires kit-graphic hitbox clicks the same way (BaySickRustyDrumsKitGraphic.cpp). Hosted VST3 instrument tabs have NO auditionNote — the roll keyboard reaches them through the live-MIDI route (`EngineKind::Plugin` = target kind 10); do not synthesize a second MIDI path for them.
- QA-C (2026-05-10) added a public `bool isAuditionPending() const noexcept` peek on the 3 sfizz engines (Guitars / Basses / RustyDrums) so the idle-suspend dispatcher predicates can wake the chain when an audition arrives.  Reads `mAuditionNote.load(std::memory_order_acquire) != -1`.
- **Validated correct** by `Plans & Specs/Research Reports/daw-architecture-research-2026-05-08.md` §3 (Lock-free MIDI dispatch). Vital + Surge XT delegate UI→audio MIDI to host's `MidiBuffer` in `processBlock` and put their queue investment on parameter automation, NOT note dispatch — same baseline as us. **DO NOT** rip this out for `juce::MidiMessageCollector` — its source has a `CriticalSection midiCallbackLock` mutex (the very thing this pattern avoids). Multi-event ring upgrade lives in Future State CL-272..CL-274 and is deferred until chord-strum / paste-to-roll / arpeggio-from-UI-clock use cases land.

### Constructor Order / resized() Safety
- Never call `resized()` during construction before all members it touches are initialized
- Guard with null checks: `if (mFoo) mFoo->setBounds(...)`

### APVTS Binding Pattern
```cpp
// Push UI → APVTS:
auto* p = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id));
if (p) p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(value));
// Read from APVTS:
if (auto* ap = apvts.getRawParameterValue(id)) return ap->load();
```

### CPU Safeguarding (standing rule)
Every DSP update function MUST guard numeric setters with value-change comparisons.
Only call the setter if the new value differs from the current DSP state.
processBlock fires hundreds of times/sec — unconditional recalculation wastes CPU.

### SpectrumFeed (seqlock audio→UI)
- Audio thread: `feed.push(src, n)` — increments seq to odd, copies, increments to even
- UI timer: `feed.poll(buf, count)` — reads seq before/after copy; returns false if write was in-progress (frame dropped, fine for visualizer)
- Seqlock is wait-free on audio thread, correct under C++ memory model

### ArrangementGrid Layout Constants
- kRowH=40, kRulerH=18, kLabelW=120, kNumRows=500, kResizeZone=8
- mPPBar (public): pixels per bar (zoom state), default 80
- Undo: one app-wide `juce::UndoManager` on StandaloneEditor -- the grid holds only an `UndoContext` and performs typed actions from `UndoActions.h` (an arrangement edit is a full before/after snapshot of blocks + row names + per-row group/color/mute/solo). Depth is a user setting (100/250/500/1000); `maxUnits = 1` is deliberate so `minimumTransactionsToKeep` IS the retained transaction count.

### UI Changes Reference
All deliberate standalone UI changes are documented in:
**`Source/Standalone/STANDALONE_UI_CHANGES.md`**
Read this file before modifying any of the listed components.

### DSP Quality Pass Reference (Phase 5F-9)
All approved DSP upgrades for the 12 effect modules are spec'd in:
**`Files For Claude/DSP Review/_APPROVED_CHANGES.md`**
Covers 12 modules (Chorus, Compressor, Delay, Flanger, Limiter (net-new), Overdrive, Phaser, Reverb, Saturation, Tape, Transient Shaper, EQ8). Includes per-module change list with implementation notes, CPU budget, ordering, and deferred UI tasks. The source prompts Jeff provided are at `Files For Claude/DSP Review/*.txt` (one per module). Read `_APPROVED_CHANGES.md` before touching any DSP file listed there.

Notes:
- **Limiter UI** — `Limiter.txt`'s 3-zone layout spec is SUPERSEDED for the panel (Jeff, 2026-08-06, QA-Layout T13/T18): the panel keeps its standard knob layout; Zone A (scrolling waveform, #00FFF2 cyan GR / #FF9100 orange ceiling) lives in the effect's VISUAL window (`EffectVisualWindow`), which every effect has. Do NOT build the skeuomorphic panel rewrite from that doc.
- **EQ band count** — stays at 8 (`kNumBands = 8`). Review suggested 7; we keep our existing 8.
- **Dynamic EQ** is full feature (DSP + UI), not DSP-only.
