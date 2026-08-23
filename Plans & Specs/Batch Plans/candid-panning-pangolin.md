# QA-TrueLevel - Honest levels, one pan law, Direct to Master, the analyzer and reports done right - Plan (candid-panning-pangolin)

**Canonical path:** `Plans & Specs/Batch Plans/candid-panning-pangolin.md`
**Paired running notes:** `Plans & Specs/Running Notes/candid-panning-pangolin.md`
**Research input:** `Plans & Specs/Research Reports/daw-architecture-pan-law-stages-2026-08-22.md`

**For execution.** This is the punch-list, not a discussion document. Every
`- [ ]` is an action. Every "Tell Jeff:" is a hard stop where execution waits.
Read Main Plan sections 0 + 5 + Carry-Forward + the Implemented Work Log before
starting, per Rule 1. The Main Plan section 5 entry for this batch is NOT yet
written (Jeff owns the plan docs) - see Routing notes at the bottom.

---

## Context

**Why this batch.** On 2026-08-22, while verifying the QA-Manuals export fix,
Jeff ran a loudness comparison that pulled a thread through the whole level
path. Normalize was accurate to 0.05 dB, yet a re-imported export metered ~7 dB
under its file loudness. Tracing it exposed:

1. A hidden parameter ("masterGain", 0..1, default 0.8, no UI anywhere) taking
   -1.9 dB off everything the app has ever played or exported.
2. A pan system that is two systems: mixer strips/buses/master run a project
   Pan Law that is SKIPPED at dead center (so the selected law never runs on an
   untouched knob and jumps 3 or 6 dB on the first tick), while every engine
   pan (BaySickPlayer, Harmless, timeline clips, our SFZ loader) hardcodes
   cos/sin and sits at -3 dB at center, ignoring the setting. Two of the three
   menu laws are wrong against FL ("Triangular" is a -6 dB crossfade no DAW
   has; "Square" is FL's Triangular) and the third is skipped.
3. The browser's Exports/Reports right-click menu was built dead (rows never
   wired to it) and the Reports section can never populate under default
   settings (retention defaults to session-only; nothing else writes reports).
4. The analyzer was not built to the SPAN + Youlean spec Jeff gave; the KBS
   Meter Suite panel he had built separately IS that spec and becomes the
   blueprint for both the in-app analyzer and the HTML report.
5. Jeff's new feature spec: Direct to Master strips, a browser-wide
   missing-file flow, per-session interactive loudness reports.

**Effort.** Nine source tasks, one build gate each. Large. The order below is
foundation-first: level truth (Tasks 1-2) lands before anything that meters or
plays through it.

**Risk.** Task 1 changes every project's absolute level (+1.9 dB from the
Master Gain removal; panned strips get louder under Ramped). Task 2 lifts
Harmless / BaySickPlayer / clips by +3 dB at default pan. Both are Jeff's
rulings (SC-1, SC-4). Pre-v1: no load shims, no migrations.

**Dependencies.** None external. The QA-Manuals ruling-B work (offline session
on the message thread) sits staged and uncommitted at batch open; it commits
on Jeff's approval as its own QA-Manuals commit before Task 1's commit.

---

## Spec calls already locked (all Jeff, 2026-08-22)

| ID | Decision | Reasoning |
|---|---|---|
| SC-1 | **Delete the hidden `masterGain` parameter outright** (not set to 1.0). The master chain multiplies by the fader alone. Old projects carrying a stored value ignore it. | "This is something you did that I never once asked for... That needs to be removed." Every fader now tells the truth. |
| SC-2 | **Two pan laws, both center-unity, continuous through center, no center skip.** `Ramped` (FL Circular: cos/sin scaled so center = 1.0 and the near side rises to +3 dB at 100%). `Flat` (FL Triangular: near side holds 1.0, far side tapers linearly to 0). The third menu entry is deleted. Labels are `Ramped` / `Flat`; the dropdown rows carry hover tooltips. | FL manual lists exactly two laws; FL measured center-unity / +3 dB at the extreme; every DAW researched converges on center-unity. Jeff's own FL measurement confirmed center reads 0. |
| SC-3 | **FL fold at all three mixer stages** (strip, bus, master): center is identity; as you pan, the far side crossfades INTO the near side; at 100% both sides land in the near channel at 0.707 each. | Jeff measured it in FL: left-only material panned 100% right reads -3; both-sides material "doubles" (+3 dB) on the near side. Pins the coefficient at 0.707. |
| SC-4 | **Every pan knob follows the one project law.** Engine pans (BaySickPlayer engine pan, Harmless master pan, the Clips-page pan that drives timeline clip decode) use the law: mono placement form for synthesized / mono-sample voices, balance form for stereo material. The resulting **+3 dB lift** of those engines at default pan is **accepted** (2 = a). | FL's own SDK has the host compute every native instrument's per-voice L/R gains. The lift also closes the hidden 3 dB gap where BaySickSynth and hosted plugins were already hotter. |
| SC-5 | **Note pan combines with engine pan as one POSITION; the law is evaluated once per voice** (4 = a). Never two laws multiplied. | FL hands its host one Pan value per voice. Multiplying laws double-attenuates or overshoots. |
| SC-6 | **Harmless unison spread stays engine-internal** (3 = a). **Strip stereo width knobs and BaySickPlayer "stereo" are mid/side width controls, not pans - untouched.** | No synth routes spread through a host law; width controls place nothing. |
| SC-7 | **SlideSampler's SFZ `pan` opcode, vendored sfizz, hosted VST3: unchanged.** | Instrument-file content and third-party internals own their pan everywhere; no plugin standard can carry a pan law. Their law-following pan is the strip they sit on. |
| SC-8 | Player Volume default stays 0.8 (D = a). | - |
| SC-9 | **Export row menu targets:** every existing Clip/Vox/Inst page, then "a new Clip Page / Vox Page / Inst Page", then **"Direct to Master"** appended (1 = b). No Move/Copy question. | The shipped (dead) menu already had the first two groups; Jeff keeps them. |
| SC-10 | **Direct to Master (D2M) strip:** a real mixer strip routed straight to the master bus input, grouped under the master exactly like a Layers strip moved under master; grey; no player page; **transport-locked** - plays the file from bar 1 whenever the transport plays (2 = a). The file is referenced by project-relative path; NO library / sample entry is created; the Exports row stays listed. New **"Direct to Master"** group in the browser's Audio tree lists D2M strips. | "It's a strip that plays into the master so of course it plays via the main transport." |
| SC-11 | **"Reveal in Explorer" -> "Show in Explorer"**, on EVERY browser entry that has a file behind it. | Jeff. |
| SC-12 | **Missing file on load, browser-wide** (every audio entry: clips, vox, inst takes, D2M, exports): prompt with **Locate...** (native chooser opened at the last-known folder; picking repaths), **Proceed missing** (greyed with a "+" overlay; clicking reopens that chooser at the last-known location; picking reinstates), **Remove** (deletes the path/entry; then, ONLY if the player page is otherwise empty, offers the standard page-delete trio **Save page preset & delete / Delete / Cancel**; if the page hosts anything else, removal takes just that file and its path, page intact, no delete-everything prompt). No silent failures anywhere. | Jeff, confirmed verbatim 2026-08-22. |
| SC-13 | **"Keep captured takes" defaults to "In the project Reports folder"** (5 = B). | Under session-only nothing ever writes a report, so Reports could never populate. |
| SC-14 | **Session report model:** ONE report file per session (app open to close) per project (F = b), takes as sections, retention Full (no time window). **Momentary is captured alongside short-term** so selection math is exact per BS.1770. App side (analyzer take list): export selected take(s) to their own report (+ audio when captured), remove selected (rewrites the session file). HTML side: select takes and save them as a new self-contained report; interactive zoom / pan / hover readouts / drag-select a region for exact integrated (gated, from the 10 Hz momentary series), LRA (from short-term), max M / S. | Jeff: "Lets do your idea but... just be full"; export/prune split "is fine". |
| SC-15 | **Analyzer "Export Take..." button** becomes "Export Take(s)...": enabled whenever a take is selected; exports the take's report (+ audio when captured). | Today it only enables for audio-captured takes, and audio capture defaults off, so it was permanently grey. Derived from SC-14's app-side export. |
| SC-16 | **Analyzer + report visuals rebuilt to the KBS Meter Suite blueprint** (`C:\Users\jeffm\KnowledgeBase Studios\Plugin Production\KBS Plugins\Source\UI\Meters.h`, `Core\Loudness.h`, `Products\MeterSuite.h`): Levels / Loudness / Spectrum views, verdict colors against target, target-centered M/S bars, per-channel true peak with ceiling markers, correlation, peak-hold spectrum with per-column averaging and low-end interpolation, 60 Hz body-only repaint. **Plus improvements** (Jeff: "if you can find improvements again then do so"): whole-take zoomable history, LRA band shading, PLR/PSR readouts, SPAN-style slope tilt + 1/3-octave mode, two-take overlay A/B, over-ceiling count, verdict graded against the export dialog's chosen spec. | Jeff: the analyzer was never built to the SPAN + Youlean spec; the plugin panel is. |
| SC-17 | **All one batch.** | Jeff. |
| SC-18 | Batch ID `QA-TrueLevel`, silly name `candid-panning-pangolin`. | Naming is mine. |

**Tooltip text (SC-2), pending Jeff's edit - shipped as written unless he changes it:**
- Ramped: `Constant-power panning. A sound keeps its level at center and rises by up to 3 dB as you pan it toward one side, so it feels equally loud anywhere in the stereo field.`
- Flat: `No level compensation. The side you pan toward holds its level while the other side fades out, so a sound is loudest at center and about 3 dB quieter at the sides.`

---

## Sub-spec calls surfaced for ExitPlanMode

Two small presentation calls under SC-12, asked at Task 5 start (Jeff said
"both yours" meaning his; neither blocks Tasks 1-4):

- **SS-1** Several files missing on one load: (a) one prompt per file in
  sequence; (b) one list dialog with per-row choices.
- **SS-2** For a library file with no strip of its own (a clip / take), the
  "Proceed missing" grey-plus presentation lands on (a) its browser row and its
  grid blocks; (b) browser row only.

Everything else is locked above.

---

## Files to modify

**Task 1 - Master Gain + pan law foundation**
- `Source/PluginProcessor.cpp:484` - `addF("masterGain", ...)` DELETE; `:490-495` `master_pan_law` declaration (range 0..1, labels); `:3769-3773` per-block string lookups DELETE.
- `Source/BaySickGraph.cpp:259-300` `applyPanLaw` / `applyStereoPan` DELETE -> `pan::` helper; `:579` insert, `:806` bus, `:846` master call sites; `:738` `pMasterGain` bind + `:839-843` gain math (fader only); comments `:615`, `:819`, `:1086` mentioning masterGain.
- `Source/BaySickGraph.h:450-455` `processBus` dead `panLaw` arg; `Source/Engine/BlockContext.h:20-23` `panLaw` field; `Source/Engine/Tasks/PassiveStripTask.cpp:83`.
- `Source/DSP/PanLaw.h` NEW.
- `Source/Standalone/StandaloneEditor.cpp:7665-7683` menu (two rows, custom tooltip rows).
- `Source/EngineRig.cpp` - hand every engine the law source at creation (consumed in Task 2).

**Task 2 - engine pan sites + note pan**
- `Source/BaySickPlayer/BaySickPlayerDSP.cpp:1212-1219` `setPan`; `:1162-1174` note-pan multiply -> combined position.
- `Source/BaySickPlayer/BaySickPlayerProcessor.cpp:184` pan push change-gate (+ law in the key).
- `Source/Harmless/AdditiveVoice.cpp:786-792` `setPan`; `:726-728` note pan. `:1110-1116` unison LEFT ALONE.
- `Source/Harmless/HarmlessProcessor.cpp:637` change-gate.
- `Source/BaySickSynth/BaySickSynthVoice.cpp:912-914` note pan (no engine pan: note position alone through the law).
- `Source/PluginProcessor.cpp:1010-1013` clip decode pan (stereo file -> balance form; mono -> placement); `:1776-1781` apply.
- `Source/SlideSampler/SlideSampler.cpp:1032` LEFT ALONE. `libs/sfizz` LEFT ALONE.

**Task 3 - browser menu**
- `Source/Standalone/BuilderPage.cpp:741-794` `rebuildRenderRows` fill (wire `onContextMenu` / `onSelected`); `:1034-1075` `showAudioTreeContextMenu` render branch; `:1093` `kIdReveal` label; `:806-860` `beginAddRenderToProject` targets (+ Direct to Master); `:385-417` categories (+ "Direct to Master" group); `Source/Standalone/BuilderPage.h:96-135` `AudioBrowserItem`.
- Every other "Reveal in Explorer" string: grep at task start.

**Task 4 - D2M strip** (surfaces to open at task start; line refs filled in then)
- `Source/Standalone/MixerPage.cpp` strip kinds / moved-under-master grouping / colors; `Source/Standalone/MixerTrackStrip.cpp`.
- `Source/BaySickGraph.cpp/.h` - a strip node fed by a file player, routed to master input.
- `Source/PluginProcessor.cpp` - transport-locked file playback (reuse `AudioClipStreamer`); offline render path (export must include D2M strips).
- `Source/ProjectManager.*` - persistence (path, strip state); `Source/EngineRig.*` if the strip needs a rig key.
- `Source/Standalone/BuilderPage.cpp` browser group + removal.

**Task 5 - missing-file flow**
- `Source/ProjectManager.*` load path (audio library + D2M path resolution); `Source/Standalone/StandaloneEditor.cpp` load-time prompt; `Source/Standalone/BuilderPage.cpp` browser row states; grid block states; `MixerPage.cpp` D2M grey + "+" overlay; page-delete trio reuse (grep the existing "Save page preset" prompt).

**Task 6 - capture defaults + analyzer export button**
- `Source/Standalone/StandaloneEditor.cpp:2031-2032`, `:19936`, `:20035` (`fsCaptureRetain` default 1 -> 2).
- `Source/Standalone/MasterAnalyzerWindow.cpp:61-68`, `:136-146` export button; `Source/Standalone/StandaloneEditor.cpp:16943`, `:18800` `exportCapturedTake`.

**Task 7 - session report model + interactive HTML**
- `Source/Standalone/VersionCapture.h/.cpp` (`kHistoryHz=10` `:161`; `Version` struct + momentary curve; `onPersistTake` `:242` -> session-file append; remove/export API).
- `Source/Standalone/LoudnessReportWriter.cpp/.h` (multi-take session document; embedded data incl. momentary; self-contained JS; `readEmbedded` `:358` multi-take).
- `Source/Standalone/StandaloneEditor.cpp:2066-2110` writer hook; analyzer Source list export/remove.
- `Source/PluginProcessor.h:359-374` reports folder resolver (unchanged unless naming needs it).

**Task 8 - analyzer + report visuals**
- `Source/Standalone/MasterAnalyzerWindow.cpp` (494 lines: `paint` `:275`, `paintReadouts` `:293`, `paintLoudness` `:351`, `paintSpectrum` `:427`) + `.h`.
- `Source/Standalone/LoudnessReportWriter.cpp` visual layer.
- Blueprint (read-only): `KBS Plugins\Source\UI\Meters.h`, `Core\Loudness.h`, `Products\MeterSuite.h`.

**Task 9 - docs, manuals, test plan, installer**
- `Plans & Specs/System Reference/Mixer.md:244`, `:318`; `Verbatim Strings.md:10`, `:28`; `MANUAL-1 Screenshot List.md:1214`; `Callout Registry.md:2058` (pan law: three entries -> two, new names).
- `Plans & Specs/Test Plans/v1-master-test-plan.md` (new section rows).
- `Manuals/` regen per `Plans & Specs/Manuals Rebuild Checklist.md`; `make_installer.bat`.

---

## Tasks

### Task 1 - Master Gain out, one pan law in (mixer stages + menu)

- [ ] Delete `addF("masterGain", ...)` (`PluginProcessor.cpp:484`) and every read: `pMasterGain` bind (`BaySickGraph.cpp:738`), the multiply (`:839-843` -> `g = muted ? 0 : fader`), the three comments. Grep `masterGain` tree-wide afterwards: zero hits in `Source/`.
- [ ] Create `Source/DSP/PanLaw.h` (header-only, `namespace baysick::pan`):

```cpp
enum class Law : int { Ramped = 0, Flat = 1 };

struct MonoGains   { float l, r; };                 // placement of a mono signal
struct StereoMatrix { float ll, rr, lr, rl; };     // lr = L into R, rl = R into L

// Center-unity by construction: pan 0 -> {1, 1}.
//   Ramped: l = sqrt2 * cos(t*pi/2), r = sqrt2 * sin(t*pi/2), t = (pan+1)/2
//           -> 100% right = {0, 1.414} = +3 dB on the near side (FL Circular as measured)
//   Flat:   l = pan <= 0 ? 1 : 1 - pan,  r = pan >= 0 ? 1 : 1 + pan
MonoGains monoGains (float pan, Law) noexcept;

// Balance: each side scaled by its mono gain, no crossfeed (engine pans on stereo material).
StereoMatrix balance (float pan, Law) noexcept;

// FL fold (mixer stages): far side crossfades INTO the near side.  At 100% both
// land in the near channel at 0.707 (Jeff's FL measurement 2026-08-22: left-only
// material panned hard right reads -3; both-sides material reads +3).
//   pan p >= 0:  ll = gL(p),  rr = a(p),  lr = b(p),  rl = 0
//   with a + b = gR(p), a(0) = 1, b(0) = 0, a(1) = b(1) = gR(1)/2,
//   b(p) = gR(p) * p / 2   (smooth; hits the measured ends)   -- mirrored for p < 0
StereoMatrix fold (float pan, Law) noexcept;

// Ramped across the block like the fader; identity short-circuit when both are {1,1,0,0}.
void apply (juce::AudioBuffer<float>&, const StereoMatrix& from, const StereoMatrix& to) noexcept;
```

- [ ] `BaySickGraph.cpp`: delete `applyPanLaw` / `applyStereoPan`; insert / bus / master call `pan::apply` with `fold`, each node keeping a `StereoMatrix mLastPan` for the ramp. Center skip becomes the identity short-circuit inside `apply` (a pure optimization now - the laws ARE unity at center).
- [ ] Law distribution: `std::atomic<int> mPanLaw` on the processor, written by an APVTS listener on `master_pan_law`; graph nodes read it; delete the per-block string lookups (`PluginProcessor.cpp:3769-3773`), `BlockContext::panLaw`, and the dead `processBus` arg. `EngineRig` hands each engine `const std::atomic<int>*` at creation (Task 2 consumes it).
- [ ] `master_pan_law` range 0..1 (Ramped = 0 default, Flat = 1). Stored 2 from old projects clamps to 1 - pre-v1, no shim.
- [ ] Menu (`StandaloneEditor.cpp:7665-7683`): two rows "Ramped" / "Flat" as `PopupMenu::CustomComponent` rows carrying `setTooltip` (plain items cannot tooltip; the app's single parentless `VibeTooltip` finds components inside menu windows). Text per SC-2.
- [ ] Comment policy sweep in every edited region (Rule 6).
- [ ] Build gate: `do_build.bat` (background), six exit codes + four links + error grep.
- [ ] **Tell Jeff:** build green. Verify (Debug first, then Release): (1) open project 132, play: the master LUFS box reads ~1.9 dB higher than before (the Master Gain is gone) and nothing else changed. (2) Mixer menu > Pan Law shows two entries, Ramped / Flat, hover shows the tooltip. (3) A mono sample on a Layers strip, Ramped: sweep the strip pan slowly L -> R watching the master meter - no step at center, level rises toward the side, about +3 dB at 100%. Same sweep under Flat: level stays on the near side, the other side fades, no jump anywhere. (4) A STEREO file as a clip, strip pan 100% right, Ramped: the left side's content is audible in the right channel (the fold), the left channel is silent. (5) Export the project: the file now matches the live meter without the old 1.9 dB offset.
- [ ] Commit (Rule 9): one-liner, surface message + full git status, wait for approval.
- [ ] `/draft-doc running-notes` -> apply.

### Task 2 - every engine pan follows the law; note pan as one position

- [ ] Engines receive the law pointer from EngineRig; each folds the law value into its existing pan change-gate (`HarmlessProcessor.cpp:637`, `BaySickPlayerProcessor.cpp:184`) so gains recompute only when pan OR law changed (CPU Safeguarding).
- [ ] BaySickPlayer voice: `setPan` -> `pan::monoGains (enginePan + notePan clamped, law)`; delete the `npLOf` / `npROf` multiply (`BaySickPlayerDSP.cpp:1162-1174`). Note-pan ramps (CC89 glide) keep ramping the POSITION.
- [ ] Harmless: `AdditiveVoice::setPan` same shape; delete the `npL/npR` multiply at `:726-728`. Unison `:1110-1116` untouched.
- [ ] BaySickSynth: note position alone through `monoGains` (`BaySickSynthVoice.cpp:912-914`).
- [ ] Clip decode (`PluginProcessor.cpp:1010-1013`): stereo file -> `pan::balance`; mono file -> `monoGains`; applied at `:1776-1781` unchanged in shape.
- [ ] Confirm by grep that no cos/sin pan remains outside unison, SlideSampler and libs.
- [ ] Build gate.
- [ ] **Tell Jeff:** (1) Harmless, BaySickPlayer and a clip row at default pan all read +3 dB louder than before; BaySickSynth and a hosted plugin tab do not move; the engine families now sit level with each other. (2) Harmless pan knob sweep under Ramped then Flat behaves exactly like the strip pan did in Task 1. (3) Piano roll: a note with note-pan hard left on a Layers tab whose engine pan is hard right plays CENTERED (positions add), not silent. (4) Harmless unison spread sounds unchanged.
- [ ] Commit -> surface -> wait. Running notes.

### Task 3 - browser right-click lives; Show in Explorer everywhere; Direct to Master in the menu

- [ ] `rebuildRenderRows` fill: wire `leaf->onContextMenu` (-> `showAudioTreeContextMenu`) and `onSelected` exactly as the library-leaf factory at `:578-586` does. The dead menu becomes reachable: Exports rows get "Add to Project..." / "Show in Explorer"; Reports rows get "Open in Analyzer" / "Show in Explorer".
- [ ] Rename every "Reveal in Explorer" to "Show in Explorer" (grep `Reveal in Explorer` tree-wide incl. System Reference + Verbatim Strings); ensure every file-bearing browser entry offers it (library leaves, auto/manual group children, exports, reports, D2M rows).
- [ ] `beginAddRenderToProject` targets: existing pages, the three "new page" entries, then "Direct to Master" (SC-9). The D2M choice calls Task 4's creation API (stub that lands in Task 4 - Task 3 ships the menu item calling a function that Task 4 fills in; until then it creates nothing and says so in a message box).
- [ ] Add the "Direct to Master" category to the Audio tree (grey accent), populated from the D2M strip list (empty until Task 4).
- [ ] Build gate.
- [ ] **Tell Jeff:** (1) right-click an export: the menu appears; "Add to Project..." lists your pages, the three new-page options, and Direct to Master at the bottom; "Show in Explorer" opens the folder. (2) Right-click a clip, a take, a group child: "Show in Explorer" present everywhere a file exists. (3) Drag an export onto the grid still works.
- [ ] Commit -> surface -> wait. Running notes.

### Task 4 - Direct to Master strip

- [ ] Open the surfaces (MixerPage strip kinds + moved-under-master grouping + colors; BaySickGraph strip node creation; transport-locked file playback via `AudioClipStreamer`; ProjectManager persistence; EngineRig key if needed; offline render inclusion). Record file:line in the running notes at task start.
- [ ] Strip: new kind `DirectToMaster`; grey identity color; fader / pan (Task 1 law) / mute / solo / rack like any strip; sits grouped under the master; no page, no "open page" affordance.
- [ ] Playback: file streamed from bar 1 locked to the transport (play / stop / seek / loop follow the transport exactly as a clip does); feeds the master bus INPUT (through master rack + fader, per E). Included in exports and freeze renders (same offline loop; no special path).
- [ ] Persistence: strip + project-relative path in project.xml; restore creates the strip; NO audio library entry.
- [ ] Browser: the "Direct to Master" group lists each D2M strip (name = file name); right-click: Rename / Remove / Show in Explorer. Remove deletes the strip; the Exports row was never hidden (the claimed-file stand-down only applies to library imports).
- [ ] Build gate.
- [ ] **Tell Jeff:** (1) Export row > Add to Project > Direct to Master: a grey strip appears under the master; the browser shows it under Direct to Master; the Exports row is still there. (2) Play from bar 1: the export plays in sync with the song; stop/seek/loop follow. (3) Strip fader/mute/solo work; solo it to A/B against the mix. (4) With Master Gain gone and the fold at center = identity, a -14 normalized export on a D2M strip at unity reads -14 integrated on the analyzer. (5) Save, reopen: strip restored. (6) Export the project: the D2M strip is in the file.
- [ ] Commit -> surface -> wait. Running notes.

### Task 5 - missing-file flow, browser-wide

- [ ] **Tell Jeff (task start): SS-1 and SS-2.**
- [ ] Load path: every audio-library entry, take, and D2M path is resolved; each missing file raises the three-option prompt (SC-12). Locate = native chooser at the last-known folder; picking repaths (library entry / D2M strip) in place.
- [ ] Proceed missing: D2M strip greys with a "+" overlay; clicking opens the same chooser and reinstates. Library entries: per SS-2.
- [ ] Remove: delete the path/entry; if the owning page is otherwise empty -> the existing page-delete trio (reuse the shipped "Save page preset & delete / Delete / Cancel" prompt); else remove only that file and its path.
- [ ] No silent failure remains: grep the load path for swallowed missing-file cases.
- [ ] Build gate.
- [ ] **Tell Jeff:** (1) rename an export on disk, open its project: prompt appears; Locate it -> strip plays. (2) Same with Proceed: grey strip with "+"; click, pick, plays. (3) Same with Remove on a D2M strip: gone. (4) Move a clip's wav away, open: prompt; Remove on a clip that shares its page with another clip: only that clip goes, no page prompt. (5) Remove on the only clip of a page: the Save preset / Delete / Cancel trio appears.
- [ ] Commit -> surface -> wait. Running notes.

### Task 6 - capture defaults + the analyzer export button

- [ ] `fsCaptureRetain` default 1 -> 2 at all three sites (`StandaloneEditor.cpp:2031-2032`, `:19936`, `:20035`).
- [ ] Analyzer button -> "Export Take(s)...", enabled whenever a take is selected; exports the take's report (Task 7's writer; until Task 7 lands it writes the single-take report the existing writer produces) plus the audio file when one was captured.
- [ ] Build gate.
- [ ] **Tell Jeff:** (1) fresh settings: File Settings shows "In the project Reports folder" selected. (2) Play a pass, stop: a report appears under Reports in the browser; right-click > Open in Analyzer works. (3) Select a take in the analyzer: Export is lit; it writes the report.
- [ ] Commit -> surface -> wait. Running notes.

### Task 7 - session report: one file, takes as sections, momentary stored, interactive

- [ ] VersionCapture: capture momentary at 10 Hz alongside short-term (`Version::momentaryCurve`).
- [ ] Session file: `<project>\Reports\<project> - <session start stamp>.html`, created on the first retained take, each further take APPENDED as a section (rewrite-in-place of a self-contained document). `readEmbedded` parses multi-take documents; the analyzer's Source list shows every take in the file.
- [ ] Analyzer take list: "Export selected take(s)..." (new report file with just those takes + audio files when captured) and "Remove selected take(s)" (rewrites the session file).
- [ ] HTML: self-contained vanilla JS, no external assets. Per take: zoom / pan on the curves, hover readout, drag-select a region -> exact gated integrated from the momentary series (400 ms blocks at 100 ms hops = the 10 Hz series; -70 LUFS absolute + -10 LU relative gates), LRA from the short-term distribution (10th/95th percentiles), max M / S, duration. Take checkboxes + "Save selected as report" (browser download of a new self-contained file).
- [ ] Build gate.
- [ ] **Tell Jeff:** (1) play three passes: ONE file under Reports, three takes inside; open in the analyzer: Source lists three. (2) Open the HTML in a browser: drag a region on take 2: integrated for that region matches the app's number for the same span within 0.1 LU. (3) Tick two takes > Save selected: a new file with just those two. (4) Remove take 1 in the analyzer: the file now holds two.
- [ ] Commit -> surface -> wait. Running notes.

### Task 8 - analyzer + report visuals to the KBS blueprint, with improvements

- [ ] Port the KBS `MeterPanel` design into `MasterAnalyzerView`: three views (Levels / Loudness / Spectrum), verdict colors, target-centered M/S bars, per-channel true peak with ceiling markers and MAX TP cells, correlation, peak-hold spectrum with per-column averaging + low-end interpolation, 60 Hz body-only repaint. Read `Meters.h` + `Loudness.h` + `MeterSuite.h` first; port, do not reinvent (Basic = exact replica; our additions are the Advanced layer below).
- [ ] Improvements (SC-16): whole-take zoomable history (the app owns the full take), LRA band shading on the history, PLR / PSR readouts, spectrum slope tilt (SPAN-style, 0 / 3 / 4.5 dB/oct) + 1/3-octave mode, two-take overlay A/B from the Source list, over-ceiling count, verdict graded against the export dialog's chosen LoudnessSpec.
- [ ] Report HTML visual layer restyled to the same design language (dark panel, same verdict colors, same bar shapes).
- [ ] Build gate.
- [ ] **Tell Jeff:** side by side with the KBS Meter Suite plugin: (1) Levels view matches; (2) Loudness view matches, then zoom the whole take; (3) Spectrum view matches, toggle tilt and 1/3-oct; (4) overlay two takes; (5) open the HTML: same look.
- [ ] Commit -> surface -> wait. Running notes.

### Task 9 - docs, test plan, manuals, installer, close

- [ ] System Reference: pan law text (two laws, new names, tooltips) at the four sites listed under Files; Master Gain removal has no doc (it was never documented - grep to be sure); Show in Explorer rename; D2M strip + browser group; missing-file flow; session report; analyzer views.
- [ ] `v1-master-test-plan.md`: a `QA-TrueLevel` section with one row per Tell-Jeff scenario above (pan laws, fold, engine lift, D2M, missing file, reports, analyzer).
- [ ] Manuals regenerated per the Rebuild Checklist (new screens: pan law menu, D2M strip, analyzer views, report); PDFs reprinted.
- [ ] `make_installer.bat`.
- [ ] `/draft-doc batch-close` -> `/review-batch QA-TrueLevel` -> apply -> commit -> surface -> wait.

---

## Verification (end-to-end smoke)

1. Project 132 at defaults: master reads +1.9 dB over the last QA-Manuals build; Harmless / clips / BaySickPlayer tabs +3 dB on top; BaySickSynth + hosted plugin tabs unchanged relative to the master.
2. Pan law menu: Ramped / Flat with tooltips; strip, bus, master, Harmless knob and BaySickPlayer knob all sweep the same way; no step at center anywhere; a stereo clip at 100% folds.
3. Export `Clean.wav` -> Add to Project > Direct to Master -> play from bar 1 -> analyzer integrated = the file's true loudness (-19.2 + the 1.9 the old build stole = what the live mix now reads).
4. Rename that export on disk, reopen: Locate / Proceed / Remove all behave per SC-12.
5. Three playback passes -> one session report, three takes; interactive selection math matches the app; export two, remove one.
6. Analyzer matches the KBS plugin panel view for view.
7. Installed copy (Release) shows all of the above; Debug run first with zero jasserts.

---

## Routing notes (Rule 3 application during execution)

- **Main Plan registration is pending Jeff.** This batch has no section 5 entry, no section 6 slot, no section 9 Forks entry yet. Draft text (for `/draft-doc forks-entry` at close or earlier if Jeff asks): "2026-08-22 - QA-TrueLevel inserted after QA-Manuals (Jeff's LUFS comparison exposed the hidden Master Gain, the two-system pan law, the dead browser menu, the un-populatable Reports section and the off-spec analyzer; Jeff ruled all one batch)."
- Findings inside the level path (any other hidden gain, any other pan) -> fix in-batch under Task 1/2 (QA batches fix bugs found).
- Findings outside (unrelated UI, DSP) -> running notes + Rule 3 routing at close.
- The QA-Manuals running notes already carry the full 2026-08-22 investigation; this batch's notes start from the plan, not a re-telling.

---

## Carry-Forward Reference touch points

- Task 1-2: Carry-Forward "Mixer/page lifecycle" + "Lock-free + lifecycle primitives" (law distribution must be an atomic read, never a per-block APVTS lookup; engines are separate processors - EngineRig is the only model-side hand-off point).
- Task 4: "Model-owned engines (EngineRig)" + "Contained-window shell" (a D2M strip has no page, so nothing page-scoped may be registered against it; automation lanes for its strip params register model-side like every strip).
- Task 5: ProjectManager load order (the sfizz teardown-order trap does not apply, but the load sweep that hides retired buses does - the D2M strip must be swept the same way).
- Task 7-8: "True offline export / render path" (the analyzer tap and capture are gated on `!isNonRealtime()`; session capture must stay silent during exports and freezes).
