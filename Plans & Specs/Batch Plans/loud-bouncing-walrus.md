# QA-Export — Song-mode audio export (WAV/OGG/MP3) + project bundle — Plan (loud-bouncing-walrus)

> **Canonical path:** `Plans & Specs/Batch Plans/loud-bouncing-walrus.md` (mirrored at G4 group
> approval; home-dir copy deleted). **For execution:** bulk-run G4 batch 5 of 8. §B authored at
> code-complete; one source commit.

## Context

Scout state (2026-07-25, all refs current): `doExport()` is a dead stub with zero callers
([BuilderPage.cpp:7727-7730](Source/Standalone/BuilderPage.cpp:7727)); the File menu's
"Export as WAV/MP3" items 120/121 exist but have NO dispatch cases — silent no-ops today.
The only working render is per-pattern (`renderPatternToWav`, :7878-7941): fresh
`VibeSynthProcessor` + `getStateInformation` clone + inline OfflineHead + block loop — solid
harness, but pattern-mode-only, synchronous on the message thread (UI freezes), and hardcoded
(44.1k / 512 / 2ch / 24-bit / 2.0 s tail / 4/4 single-tempo `bars*4` length math).

What song mode adds that the offline harness must cover: the arrangement pattern sequence,
song-only automation (G3 semantics), arrangement audio clips (snapshot rebuild needed on the
fresh processor — no editor callbacks offline), tempo-map/TS-aware length. Live-input strips
are inherently silent offline (no device) — correct for a mixdown. Buses/sends/master run
unconditionally in the graph, so they're covered once sources render.

Encoders: WAV + **OGG + FLAC encoders are already compiled** (JUCE defaults on); **no LAME
exists in the tree** and JUCE's LAME wrapper is a CLI shell-out. Jeff locked 7=A: vendor
libmp3lame source and link it. `ZipFile::Builder` is available (with a progress pointer); no
bundle code exists yet. Song end: mirror the exact `songEnd` beats computation
(`onGetLoopBeats`, [StandaloneEditor.cpp:989-1011](Source/Standalone/StandaloneEditor.cpp:989)),
NOT the 16-bar-floored `getTotalArrangementBars()`.

- **Risk:** medium — new offline path for song content, new vendored dependency, filesystem
  walker over user samples.
- **Effort:** ~10-14 h (LAME vendoring + song offline + dialog + bundle).
- **Dependencies:** QA-ProjectSave's "Pack Project" (next batch) REUSES this batch's bundle
  walker — build it as a shared utility, not inline UI code.

## Spec calls already locked

| ID | Decision | Reasoning |
|----|----------|-----------|
| 7a + docket 7=A | WAV + OGG + MP3; MP3 via vendored libmp3lame source, linked | LGPL surface routes to QA-LegalReview |
| 7b | Minimal options: format / bit depth / SR / tail / quality (lossy) | One dialog |
| 7c | Bundle offered as BOTH single .zip AND plain folder | Locked 2026-07-08 |
| Docket 8 | Bundle scope is the USER'S choice per pack: references-only vs fully-self-contained (Core Library samples copied + refs rewritten) | Jeff 2026-07-25 |
| — | One "Export Audio…" menu item replaces dead items 120/121; exact-beats stop point; async render with progress + cancel | Baked-pending-veto 2026-07-25, no veto |

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open. Implementation calls stated for R5: render runs on a
`ThreadWithProgressWindow`-style background thread (cancel deletes the partial file); SR
choices 44.1/48 kHz; WAV bit depths 16/24/32-float; OGG quality 0-10 map; MP3 CBR 128/192/256/320
via lame presets; Ctrl+R binds `cmdExportAudio` only if the chord is free in the keymap at
execution (FSW-065 asked for it; bare R = record-toggle stays).

## Files to modify

- Task 1: `libs/lame/` (new vendored source), `CMakeLists.txt` (static lib target + link;
  `/MD` runtime match per `reference_msvc_runtime_md_md_match`), new
  `Source/DSP/Mp3Writer.h/.cpp` (thin lame-API float-buffer encoder)
- Task 2: `Source/Standalone/BuilderPage.cpp/.h` (offline harness generalization),
  `Source/PluginProcessor.h/.cpp` (clip-snapshot rebuild entry + song-end setter for offline),
  `Source/Standalone/StandaloneEditor.cpp` (songEnd computation reuse/exposure)
- Task 3: `Source/Standalone/StandaloneEditor.cpp` (File menu + dispatch + export dialog),
  `Source/Standalone/KeyBindings.h/.cpp` (cmdExportAudio if chord free)
- Task 4: new `Source/Standalone/ProjectBundler.h/.cpp` (shared walker + zip/folder writer),
  `Source/Standalone/StandaloneEditor.cpp` ("Export Project Bundle…" menu + dialog)

## Tasks

### Task 1 — Vendor LAME + Mp3Writer

- [ ] Vendor libmp3lame source under `libs/lame/` (encoder core only; keep LICENSE + version
  marker for QA-LegalReview; do NOT over-prune per `feedback_dont_overprune_vendored_libs` —
  grep its CMake for unconditional refs first).
- [ ] CMake static-lib target, linked to both app configs; runtime-library setting matched to
  the project (the /MD-vs-/MT vendored-lib gotcha).
- [ ] `Mp3Writer`: init lame (SR/channels/bitrate), feed float blocks, flush + free; unit-shaped
  smoke = encode 2 s of sine offline at startup NOT included (no test scaffolding — verify via
  §B listening).
- [ ] Build gate.

### Task 2 — Offline SONG render core

- [ ] Extract `renderPatternToWav`'s harness into `renderArrangementToFile(opts)`: fresh
  processor + state clone (unchanged pattern), then the song-mode additions:
  - [ ] enter song mode on the render processor (`setSongMode(true)`) AFTER state apply;
  - [ ] rebuild the audio-clip snapshot directly (locate the publisher the UI's
    `onArrangementChanged` path calls — the fresh processor has no editor; call it explicitly;
    verify decoded clips resolve via `resolveProjectFile` against the CURRENT project folder —
    set it on the render processor from the live one);
  - [ ] set the render stop point from the exact songEnd-beats computation (refactor the
    `onGetLoopBeats` song-end block into a shared helper both callers use — no duplicated math);
  - [ ] OfflineHead follows the tempo map: advance in samples, derive beat via the same
    map/clock the live playhead uses (no `bpm*elapsed` linear math — G1's beat-domain lesson);
    publish bpm-at-position each block;
  - [ ] honor SR/blockSize/bit-depth/tail from opts; total samples = map-derived seconds
    (songEnd) * SR + tail.
- [ ] Keep `renderPatternToWav` working by re-expressing it over the shared harness
  (pattern mode, current-pattern length) — one harness, two entries.
- [ ] Move the render loop off the message thread: progress window with cancel; cancel closes
  the writer and deletes the partial file.
- [ ] Build gate.

### Task 3 — Export dialog + menu + keybind

- [ ] Replace File items 120/121 with one **"Export Audio…"** (reuse id 120; delete 121):
  dialog = Format (WAV/OGG/MP3), Bit depth (WAV only), Quality (OGG/MP3 only), Sample rate
  (44.1/48), Tail seconds (default 2.0), then native save chooser (default `userMusicDirectory`,
  extension follows format).
- [ ] Add `cmdExportAudio`; bind Ctrl+R if free in the default map + user keymap-safe (the
  KeyBinds window lists it either way).
- [ ] Wire `doExport()` stub to the same dialog (or delete the stub trio if `doNew/doSave/doOpen`
  siblings are equally dead — grep callers; dead = remove per clean-own-batch rule... they are
  PRE-EXISTING dead code, so log + route instead of deleting unprompted).
- [ ] Build gate.

### Task 4 — ProjectBundler (shared with QA-ProjectSave's Pack)

- [ ] `ProjectBundler::enumerate(project)`: walk PatternManager audio library + every
  `ArrangementBlock.audioFilePath` + Clips (`My Samples` bare-name refs) + engine
  `library:`/absolute refs inside engineData (per-engine walk) -> reference list tagged
  {projectRelative, mySamples, coreLibrary, absolute, missing}.
- [ ] `ProjectBundler::write(list, mode, scope, progress)`: mode = zip (`ZipFile::Builder` with
  progress) | folder (`copyDirectoryTo` pattern); scope = **references** (project Samples/ +
  absolute + My-Samples files copied; `library:` refs left as references) | **self-contained**
  (additionally copies Core Library files + rewrites refs into the bundle's Samples/). Missing
  files listed in a completion report dialog — never silently dropped (no-silent-caps rule).
- [ ] "Export Project Bundle…" File item: dialog = destination, zip-vs-folder, scope choice
  (docket 8: both offered per pack).
- [ ] Build gate.

## Batch close (bulk-run per-batch loop — one commit per batch)

- [ ] Tell Jeff to run `do_build.bat`; fix until BOTH configs build clean.
- [ ] Author this batch's Master Test Plan §B section from the Verification list below
  (`blocks:` = this batch's commit, hash backfilled at commit).
- [ ] `/draft-doc batch-close` -> held Work Log entry in running notes (R2); no §5 touch.
- [ ] Running-notes code-complete entry (+ Rule 4 rows if any diagnostics shipped).
- [ ] ONE batch commit (Rule 9): `QA-Export: <one-line what> (<scope>)` + trailer; message +
  FULL git status; commit on Jeff's approval. (New `libs/lame` tree rides this commit —
  call it out in the status walk.)

## Verification (authors into Master Test Plan §B)

1. Export a real multi-pattern song to WAV 24/44.1: file length = song end + tail; plays
   correctly in an external player; content matches live playback by ear (patterns, drums,
   clips, sends, master chain).
2. Same song to OGG (mid quality) and MP3 (256): both play externally; no truncation.
3. Song with a tempo marker mid-way: exported audio hits the tempo change at the right
   wall-clock position (readout-verified against the transport display).
4. Song-only automation lane audibly present in the export; pattern-mode state untouched after
   export (mode baselines respected on the RENDER processor only — live app never flips).
5. Arrangement audio clip (stretched + unstretched) present and clean in the export.
6. Cancel mid-render: no partial file left; app responsive throughout (progress window live).
7. Per-pattern right-click render still works (regression over the shared harness).
8. Bundle as FOLDER (references scope): copy to another location, rename the original project
   folder away, open the bundled copy — everything loads (Core Library refs resolve via the
   installed library).
9. Bundle as ZIP (self-contained scope): unzip elsewhere, open — loads with NO dependence on
   My Samples/original folder; spot-check a rewritten engine sample plays.
10. Missing-file case: delete one referenced sample, bundle — completion report names it.
11. Ctrl+R (if bound) opens the export dialog; the KeyBinds window lists Export Audio.

## Routing notes (Rule 3)

Encoder/licensing artifacts route to QA-LegalReview (G6) — record the LAME version + license
file location in running notes at vendoring time. The dead `doNew/doSave/doOpen` stub trio
(pre-existing) logs for close-time routing (likely QA-Cleanup), not deleted here.

## Carry-Forward Reference touch points

§1 render/graph primitives + §2 audio-thread rules before Task 2 (the offline processor runs
processBlock on a worker thread — same RT rules apply inside the loop). The G1 beat-domain
carry-over (swift-stampeding-caribou smoke findings #4/#10) is required reading for the
OfflineHead tempo-map work.
