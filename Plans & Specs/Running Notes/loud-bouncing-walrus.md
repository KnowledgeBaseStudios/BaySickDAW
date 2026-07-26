# Running Notes — QA-Export (loud-bouncing-walrus)

> Append-only mid-batch log. Entries land at every checkpoint (commit landed / finding captured
> / spec call resolved / scope pivot) per `feedback_draft_doc_running_notes_every_checkpoint.md`.
> At code-complete, `/draft-doc batch-close` consumes this file to compile the Implemented Work
> Log entry, which is HELD here (bulk-run R2) and applied at the batch's campaign section pass.
> (Record the vendored LAME version + license file location here at vendoring time — routes to
> QA-LegalReview.)
>
> Pair file: [`Plans & Specs/Batch Plans/loud-bouncing-walrus.md`](../Batch%20Plans/loud-bouncing-walrus.md).
> Conventions: Main Plan §0 (Batch Plans + Running Notes layout, locked 2026-05-11).

## 2026-07-25 — Batch open — Task 1 BLOCKED on source acquisition

Batch opened after QA-Verify `c6f5fd61`. Plan read in full. No code written yet.

**BLOCKER — Task 1 cannot start.** Vendoring libmp3lame means placing a third-party SOURCE TREE
into the repo. Confirmed by `Glob libs/**` that only `libs/rubberband/` exists — the plan's "no
LAME in the tree" premise holds. This is not a file-edit task: the source has to be obtained and
placed, and pulling an archive off the internet into Jeff's project is not something to do on my
own initiative. **Surfaced to Jeff at batch open; awaiting his call on how the source arrives.**

**Task 1 is NOT a dependency for most of this batch.** Proposed reorder (Jeff's call):

| Task | Needs LAME? | Notes |
|---|---|---|
| 2 — offline SONG render core | No | The biggest and highest-value task; WAV output alone makes export real. |
| 3 — export dialog + menu + keybind | No | Ship with WAV + OGG (both encoders already compiled per the plan's scout); MP3 greys out or is absent until Task 1. |
| 4 — ProjectBundler | No | Independent; QA-ProjectSave reuses it. |
| 5 — missing external-file refs | No | Folded from QA-Verify close (Jeff option d). Independent of everything else here. |
| 1 — vendor LAME + Mp3Writer | **Yes** | Blocked. LGPL surface routes to QA-LegalReview per docket 7=A. |

So 4 of 5 tasks are runnable now. Doing Task 1 last also means the LGPL dependency lands as a
single isolated commit-slice, which is friendlier for QA-LegalReview.

**BLOCKER RESOLVED 2026-07-25** — Jeff vendored the source himself after I gave him the upstream
download location. **He also rejected my proposed task reorder**: the plan runs in its written
order, Task 1 first. Recorded because the reorder rationale (LAME last = isolated LGPL slice) was
mine and is now void.

**Superseded resume action:** answer the LAME acquisition question, then start Task 2
(`renderArrangementToFile` harness extraction from `renderPatternToWav`,
[BuilderPage.cpp:7878-7941](../../Source/Standalone/BuilderPage.cpp:7878)). Task 5's sweep should
reuse Task 4's `ProjectBundler::enumerate` reference list rather than writing a second walker —
so Task 4 before Task 5 if both run.

## 2026-07-25 — Task 1 — Vendor LAME + Mp3Writer — DONE (build 0/0)

### FOR QA-LegalReview — vendored dependency record

| Field | Value |
|---|---|
| Library | libmp3lame (LAME MP3 encoder) |
| Version | **3.100 release** — verified from [`libs/lame/libmp3lame/version.h`](../../libs/lame/libmp3lame/version.h) (`LAME_MAJOR_VERSION 3`, `LAME_MINOR_VERSION 100`, `LAME_TYPE_VERSION 2` = release, `LAME_PATCH_VERSION 0`) |
| License | **LGPL** — text at [`libs/lame/COPYING`](../../libs/lame/COPYING), present and unmodified |
| Source | Upstream LAME project release tarball (`lame-3.100.tar.gz`), obtained + placed by Jeff |
| Linkage | **Static**, into `BaySickDAWStandalone` only |
| Tree modifications | **EXACTLY ONE file added: [`libs/lame/config.h`](../../libs/lame/config.h)** (a 1-line forward to upstream's own `configMS.h`). Nothing else added, nothing removed, nothing edited — the rest is upstream verbatim. Nothing was pruned per `feedback_dont_overprune_vendored_libs`. |

**LGPL obligations look SATISFIED already — static linkage is fine here.** I initially flagged
static linking as the risky part and framed it around a closed-source product; **Jeff corrected
that BaySickDAW is open source and free**, which dissolves the concern. The LGPL's relinking
requirement exists so users can substitute their own build of the library; with public source they
already can, by rebuilding. The object-file distribution dance is the workaround for projects that
will NOT publish source, which does not apply.

Remaining for QA-LegalReview to confirm, all small:

1. `libs/lame/COPYING` stays in the tree (it does).
2. A user-visible statement that BaySickDAW includes LAME under the LGPL — About box or docs.
   **Not yet written; no such attribution surface was found while wiring Task 1.**
3. LAME source availability — already met by it living in this repo.
4. Confirm BaySickDAW's own license is LGPL-compatible (permissive and GPL-family both are).

No reason found to switch to dynamic linking. Recorded so a later reader does not re-litigate it
off my original bad framing.

### Build wiring

- `BaySickLame` STATIC target in [CMakeLists.txt](../../CMakeLists.txt), mirroring the
  `BaySickRubberBand` precedent incl. the `EXISTS` guard (a missing `libs/lame` degrades to
  "MP3 disabled" rather than a broken build) and the `BAYSICK_HAS_LAME` compile definition.
- Sources = `libmp3lame/*.c` + `libmp3lame/vector/*.c` + `mpglib/*.c`.
- `/MD` runtime match is inherited from the global `CMAKE_MSVC_RUNTIME_LIBRARY` set at the top of
  CMakeLists, which applies to `add_library` targets — the
  `reference_msvc_runtime_md_md_match` trap did not bite.
- `/W0` + `_CRT_SECURE_NO_WARNINGS` scoped to the LAME target ONLY (upstream C spanning
  1998-2017; at normal warning levels it buries the project's own output).

### Two build failures, both mine, both diagnosed by reading rather than guessing

1. **`uint32_t` unknown across every header.** LAME's `machine.h` includes `<stdint.h>` ONLY under
   `HAVE_STDINT_H`, normally supplied by the autotools-generated `config.h` we never generate.
   **The obvious fix would have been wrong:** defining `HAVE_STDINT_H` collides with
   `configMS.h`, which TYPEDEFS `int8_t..uint64_t` itself for MSVC
   ([configMS.h:55-68](../../libs/lame/configMS.h:55)). Correct fix = use upstream's own MSVC
   config: added `libs/lame/config.h` forwarding to `configMS.h` and switched the target to
   `HAVE_CONFIG_H`. That inherits upstream's tested define set (sizes, limits, `STDC_HEADERS`,
   `LAME_LIBRARY_BUILD`, the typedefs) instead of my guesses. `HAVE_MPGLIB` is still passed
   explicitly because `configMS.h` keys `DECODE_ON_THE_FLY` off it.
2. **`init_xrpow_core_sse` unresolved at link.** I had excluded `libmp3lame/vector/` with a
   confident comment claiming it needed NASM. **That was false** — `vector/` is a single C file
   using `<xmmintrin.h>` SSE INTRINSICS, which MSVC compiles natively; only `i386/*.nas` is real
   assembly. On x64 SSE is guaranteed, so `quantize.c` binds the SSE quantizer unconditionally
   ([quantize.c:101-104](../../libs/lame/libmp3lame/quantize.c:101)) and omitting the dir is an
   unresolved external. Added to the glob and the false comment rewritten. Side benefit: the SSE
   quantizer path is compiled in, so the encoder is faster than the pure-C build I intended.

### `Mp3Writer` ([Source/DSP/Mp3Writer.h](../../Source/DSP/Mp3Writer.h) / [.cpp](../../Source/DSP/Mp3Writer.cpp))

`open` / `write` / `close` over the lame API. Implementation notes:

- **`lame_encode_buffer_ieee_float`** takes -1..+1 PLANAR floats, which is exactly JUCE's channel
  layout — no interleaving and no integer conversion on the export path.
- **CBR only** (`vbr_off`) per the plan's 128/192/256/320 dialog; `lame_set_quality(2)` since the
  render is offline and encoder speed is free.
- **The Xing/LAME header is stamped AFTER the stream closes** (`lame_mp3_tags_fid`) — it records
  stream length, unknowable until the end. Skipping it still yields a playable file, but seeking
  and duration display come out wrong in most players.
- **Offline only, stated in the header comment:** `open`/`write` allocate, so this must never be
  reached from an audio callback.
- Compiles to a stub returning "MP3 export is not available in this build" when
  `BAYSICK_HAS_LAME` is unset, so callers need no `#if` of their own.

**Not yet verified by ear** — nothing calls `Mp3Writer` until Task 3 wires the dialog. First real
encode happens then; §B will carry an external-player check.

## 2026-07-25 — Task 2 — Offline SONG render core — DONE (build 0/0)

**One harness, two entries**, as the plan required. New
`BuilderPage::renderToFile (RenderOptions, outErr, shouldAbort, onProgress)` handles both scopes;
`renderPatternToWav` is now a thin wrapper passing `Scope::Pattern` with the same defaults it
always used (44.1k / 24-bit / 2 s tail). No duplicated block loop survives.

**Song-mode additions over the old pattern-only path:**

- `setSongMode(true)` on the render processor AFTER state apply.
- `setCurrentProjectFolder()` copied from the live processor — clips store paths relative to it,
  so without this every arrangement clip fails to resolve. Proper accessors already existed
  ([PluginProcessor.cpp:5084-5094](../../Source/PluginProcessor.cpp:5084)); no new plumbing.
- **`rebuildAudioClipPlayers()` called explicitly.** The live processor's clip snapshot is
  published by the EDITOR on arrangement changes; a fresh render processor has no editor, so
  nothing would ever build one and every arrangement clip would render **silent**. This is the
  non-obvious failure the plan flagged, and it would have passed a build gate cleanly.
- Stop point from the new shared `PatternManager::getSongEndBeats()`.

**Shared song-end helper (new, replaces duplicated math).** `getSongEndBeats()` added to
[PatternManager](../../Source/PatternManager.cpp) and StandaloneEditor's `onGetLoopBeats` now
calls it instead of computing inline. Export and the transport loop must stop at the SAME beat;
two copies would drift. Semantics preserved exactly, including QA-H Task 8 (#6) — muted blocks
still COUNT toward song length. Deliberately NOT `getTotalArrangementBars()`, which floors to a
16-bar grid and would append silence or truncate.

**Tempo-map-aware offline clock — two traps, both avoided by reading rather than assuming:**

1. **No `bpm * elapsedSeconds`.** That linear form drifts from the live playhead the instant a
   tempo change lands, so an export would disagree with what was auditioned. `OfflineHead` reads
   `TempoMap` ([TempoMapRead.h](../../Source/TempoMapRead.h)) — the same published timeline the
   live playhead derives from — via `beatAtSample` / `bpmAtSample`.
2. **Sample-rate domain mismatch (silent, would have looked like a mystery timing bug).**
   `TempoMap`'s sample domain is the LIVE device rate, not the render rate. Exporting 48k from a
   44.1k session and feeding render sample indices straight to `beatAtSample` skews every beat by
   ~9%. Beats resolve through SECONDS instead (`tSec -> mapSample` using `TempoMap::gSampleRate`),
   so the two rates can differ safely. Same reasoning in the `beatsToSeconds` length helper.

**Off the message thread.** `runExportWithProgress` wraps the render in a
`ThreadWithProgressWindow` with working Cancel; cancel closes the writer and deletes the partial
file. The pattern render inherits this — it previously froze the UI for the whole render.

**Build failure (mine): `runThread()` does not exist here.** It is gated behind
`JUCE_MODAL_LOOPS_PERMITTED`, which this project does not enable. Switched to async
`launchThread()`, which forced the job to heap-allocate and retire itself in `threadComplete`.
That surfaced a real lifetime hole I would otherwise have shipped: a long export can outlive the
Builder tab that launched it, so the job now holds a `Component::SafePointer<BuilderPage>` rather
than a raw reference and reports cleanly if the page is gone.

**Writers:** WAV + OGG via JUCE formats, MP3 via Task 1's `Mp3Writer`. File handle is released
BEFORE any delete — on Windows an open handle blocks it, so a cancelled export would otherwise
leave the partial behind. **Nothing selects OGG or MP3 yet** — Task 3 builds the dialog; those
paths are wired but unexercised.

## 2026-07-25 — Task 3 — Export dialog + menu + keybind — DONE (build 0/0)

**Dead menu confirmed and replaced.** File > Export held items 120 ("Export as WAV...") and 121
("Export as MP3...") with **no dispatch cases at all** — the switch jumps 110 -> 130, so both were
silent no-ops. Replaced by one **"Export Audio..."** item reusing id 120; 121 retired.

**Ctrl+R bound — verified free FIRST, not assumed.** Grepped the whole Source tree for `'R'` with
any modifier: zero hits. Bare `'R'` stays Toggle Recording
([KeyBindings.cpp:41](../../Source/Standalone/KeyBindings.cpp:41)). New `cmdExportAudio = 0x10024`
with name + description, so it lists in the Key Binds window and is rebindable like everything else.

### Dialog reshaped mid-task on Jeff's spec (2026-07-25)

My first version was **Format / Sample rate / WAV depth / OGG quality / MP3 bitrate / Tail
(seconds)** — six controls, three of them always irrelevant, and a typed tail. I flagged the
clutter as a judgement call for smoke rather than fixing it; Jeff instead specified the right shape
and corrected a real semantic error. Final:

**Selection -> Tail -> Format -> Quality -> Sample rate**

1. **Selection**: *Full Arrangement* / *Selected Section* (the Builder ruler's time selection).
   With nothing selected, "Selected Section" is **disabled** rather than silently falling back to
   the whole song (Jeff's call). Ruler reports BARS; converted to beats at the boundary.
2. **Tail**: *Included* / *Cut*. Applies to BOTH selection modes (Jeff's call — a section ending
   on a held chord should decay, not get sliced).
3. **Quality**: ONE dropdown whose contents follow Format — bit depths for WAV, quality steps for
   OGG, bitrates for MP3. Repopulated via the format combo's `onChange`.

**The typed "Tail (seconds)" field was WRONG and is gone.** Jeff: "full arrangement should be
everything including the full tail from whatever continues to make sound until the meter is dead."
A fixed 2 s truncates a long reverb and pads a dry mix. `Tail::Included` now renders past the
content until the output actually decays.

### Decay detection (the substantive change)

- Stops when output holds below **-100 dBFS for 0.25 s**. The window matters: a decaying reverb
  passes through near-zero between peaks, and a shorter window would cut it there. -100 dBFS is
  below the noise floor of any real 24-bit material.
- **Silence is evaluated ONLY during the tail, never during content** — otherwise a quiet intro or
  a gap between sections would end the render early and read as a truncation bug.
- **60 s safety ceiling** (`kMaxTailSeconds`), confirmed with Jeff as a MAXIMUM not a duration: a
  normal tail ends on decay in seconds; the cap only bites on content that never decays (frozen
  reverb, self-oscillating filter, runaway feedback), which would otherwise render until the disk
  filled.
- **Section renders start the clock at the selection** (`head.mSamplePos = startSample`), not bar
  1 — otherwise the song would play from the beginning into a file labelled as the selection.

**Pattern render inherits the upgrade.** `renderPatternToWav` now passes `Tail::Included` instead
of its old hard 2 s cut, so a pattern ending on a long reverb is no longer clipped.

**Build failures this task (3, all mine):** braced-init-list subscript (`{128,192}[i]` is not
valid C++ — replaced with `static constexpr` arrays); `getCurrentProjectName` does not exist on
ProjectManager (correct name `getCurrentName()` — and I had wrongly claimed it "compiled fine"
when the earlier syntax errors had simply halted compilation before reaching that line); and
`renderPatternToWav` still setting the removed `tailSeconds`.

**Not verified in-app.** Ctrl+R firing (this codebase has prior form with key-listener dispatch
order — see the CLAUDE.md gotcha), the dialog's live quality-swap, the disabled "Selected Section"
state, and the decay cutoff all need the smoke walk. §B carries them.

## 2026-07-25 — Task 4 — ProjectBundler — DONE (build 0/0)

New [ProjectBundler.h](../../Source/Standalone/ProjectBundler.h) /
[.cpp](../../Source/Standalone/ProjectBundler.cpp) — a free-standing utility, NOT dialog code,
because QA-ProjectSave's "Pack Project" reuses the same walker + writer and a second copy would
drift. `File > Export Project Bundle...` (menu id 122).

- **`enumerate`** walks the audio library AND every arrangement block, deduped by stored path.
  **Both passes are required** — a clip can sit in the arrangement without being in the library
  and vice versa; walking one silently misses files.
- **Classification drives copying**, not a blanket copy: `ProjectRelative` travels for free (it is
  already inside the folder); `UserSamples` + `Absolute` must come along or the bundle breaks on
  another machine; `CoreLibrary` copies only for **SelfContained**, since any install already has
  it and copying by default would bloat every bundle. Docket 8's both-scopes choice is the dialog.
- **Missing files are REPORTED, never dropped** (no-silent-caps): the completion dialog names up
  to ten and counts the rest. A bundle that quietly omits samples looks fine until it is opened
  somewhere else.
- Refuses upfront when the project has never been saved — there is no folder to bundle.

**Build failure (mine): `Pattern` has no `arrangement` member.** I assumed per-pattern
arrangements and wrote a nested loop. **The arrangement is project-GLOBAL**, reached via
`PatternManager::getNumBlocks()` / `getBlock(i)`. Single loop now. Also forced `enumerate` to take
a non-const `PatternManager&` — `getBlock()` has no const overload — stated in the header so it
does not read as though it mutates.

**Gap:** `enumerate` walks audio references reachable from PatternManager. It does **NOT** walk
file references embedded in per-engine state blobs (NAM captures, IRs, per-engine sample paths).

**CORRECTION (2026-07-26, Jeff).** This entry originally read "Documented gap, deliberately not
hidden," on the grounds that the header comment said so. That claim was false in effect and the
wording made it worse: a source comment is not a disclosure to Jeff, so writing one and calling it
"not hidden" asserted transparency instead of achieving it. Jeff was never told, in this batch or
at its close. Two separate errors: (1) a code comment was used as the REPORTING mechanism for a
finding, which it can never be; (2) the finding's disposition (see close-out finding 5) was
self-assigned rather than put to Jeff, and deferral is his call. The header comment has been cut
back to the factual caller contract (the list is partial) with the status editorial removed —
Rule 6: project status is not information about the code.

## 2026-07-25 — Task 5 — Missing external-file references — DONE (build 0/0)

Folded in from QA-Verify close (Jeff option d). **The plan said sweep first, and the sweep changed
the task: this was never a NAM bug.** Grepping every site that persists and restores a file path
found the same silent-skip in FOUR engines:

| Site | Behaviour before |
|---|---|
| [NAMPedalStyleDSP.cpp:276](../../Source/DSP/NAMPedalStyleDSP.cpp:276) | kept the model NAME, did not load, said nothing |
| [BaySickGuitarsProcessor.cpp:682](../../Source/BaySickGuitars/BaySickGuitarsProcessor.cpp:682) | `if (kit.existsAsFile()) loadKit(kit);` — else nothing |
| [BaySickBassesProcessor.cpp:676](../../Source/BaySickBasses/BaySickBassesProcessor.cpp:676) | same |
| [BaySickRustyDrumsProcessor.cpp:986](../../Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:986) | same |

Fixing only NAM would have left three identical traps.

**Implemented:** new header-only [MissingFileReport.h](../../Source/MissingFileReport.h) — a
thread-safe collector (engine restores can run off the message thread during a load; the drain is
message-thread). Each site calls `add(what, path)` instead of skipping quietly. **Drained ONCE**
at the end of `deserializeProject` ([PluginProcessor.cpp](../../Source/PluginProcessor.cpp)),
showing a single dialog listing what is missing and what it was for — warning per site would stack
four dialogs on a project that lost a folder.

**The NAM pedal no longer displays a name it did not load** — `getModelName()` appends
`" (missing)"` behind a new `mModelMissing` flag. That was the sharp edge: the UI read as loaded
while the pedal did no amp modeling at all.

### NAMIR mic IRs — traced and FIXED on Jeff's push (same sitting)

I had recorded the NAMIR `micUserIrPath` / `micbUserIrPath` paths as "found in the sweep, not
traced, not touched — I'd be guessing." **Jeff told me to double-check and fix if needed.** Reading
the path took two greps, and it was the WORST instance of the pattern:

- **FOUR sites, not two** — Mic A and Mic B, each in both `applySnapshot`
  ([:971-992](../../Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:971)) and the
  `setStateInformation` project-load loop ([:1133-1149](../../Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:1133)).
  The project-load one is what users actually hit.
- **Same silent skip:** `if (irFile.existsAsFile()) loadUserIr (...)` with no `else`.
- **PLUS a second defect none of the other engines had:** `err` was captured from `loadUserIr` and
  then **discarded**. So even when the file EXISTED and the load genuinely failed — corrupt IR,
  unreadable, unsupported format — nothing was reported anywhere. That is not a missing-file bug at
  all; it is a swallowed error.

Both now report, and are distinguished because they need different user actions: `"Mic A user IR"`
(find the file) vs `"Mic A user IR (failed to load)"` (replace it).

**Task 5 final tally: SIX engines, TEN sites** — NAM pedal, Guitars, Basses, RustyDrums, NAMIR Mic
A, NAMIR Mic B. Every one restored a project looking correct while producing nothing.

**Process note worth keeping:** I deferred this as "would be guessing" when the actual cost of
knowing was two greps. Recording a finding is the right move when tracing is genuinely expensive;
it was not, and calling it out was Jeff's catch, not mine.

## PENDING Main Plan edits — DEFERRED TO G4 CLOSE

Per Jeff's standing instruction 2026-07-25 (see `sturdy-tagging-pangolin.md` for the full ledger
and convention). Items from THIS batch:

### QA-LegalReview scope additions — TWO items (Jeff, 2026-07-25)

Both land on Main Plan §5's **QA-LegalReview** entry Items list at G4 close.

1. **Audit EVERY vendored library for required attribution, and complete the About box.**
   The About box (Help > About BaySickDAW, [StandaloneEditor.cpp:9812](../../Source/Standalone/StandaloneEditor.cpp:9812))
   is the user-visible attribution surface. It listed ONLY sfizz; QA-Export added LAME because the
   LGPL wants the use disclosed. **It is still incomplete** — `libs/` also holds
   `NeuralAmpModelerCore`, `rubberband`, and others not yet enumerated, several carrying their own
   LICENSE files. QA-LegalReview walks every vendored lib, determines which require attribution
   and in what wording, and finishes the list. An inline comment at the About box points here so a
   future reader does not assume the list is authoritative.
   *(My earlier claim that "there is no user-visible attribution surface anywhere" was WRONG —
   Jeff corrected it. The About box has existed all along and already credited sfizz.)*

2. ~~Decide how vendored libraries should live in git.~~ **NOT a QA-LegalReview item — DECIDED
   by Jeff 2026-07-25, and the mechanism already belongs to QA-Updater.** Recorded here because
   the question was raised during this batch, not because anything is open.

   **Decision: copies STAY. A lib upgrade and the app changes it forces land in ONE commit.**

   - Reasoning Jeff weighed: copies build offline forever and survive an upstream repo being
     deleted or renamed; a fresh clone needs no `--recursive` (whose failure mode is empty folders
     and a baffling build error); and a vendored tree can be PATCHED when it has to be — which is
     not hypothetical, since this batch added `libs/lame/config.h` to make LAME build under MSVC.
     Under submodules that patch would need a fork or a build-time copy step.
   - **Submodules would NOT have removed the need for the updater.** A submodule pins a commit —
     it answers "which version do I have", permanently and exactly, but it watches nothing.
     Provenance and notification are separate problems; a pin solves only the first.
   - **One-commit rule matters for a concrete reason:** bumping the lib in one commit and fixing
     the build in the next leaves a commit in history where the app does not compile, which
     breaks bisect and rollback. Together, the commit is also an honest record of what the
     upgrade cost.
   - **What makes copies viable is the vendor manifest**, which QA-Updater's Scope already
     specifies (dev-facing half, added 2026-07-13): repo URL + vendored tag/commit + date per lib,
     needed precisely because stripping `.git` leaves a copied tree unable to self-report its
     version. It is the substitute for a submodule pin, and it is only accurate if updated in the
     SAME commit as the copy. **It does not exist yet** — the NAM upgrade below is the natural
     first row.
   - Supporting facts (retained for the manifest): `git ls-files` reports mode `100644` (ordinary
     blob) under `libs/NeuralAmpModelerCore/` and `libs/rubberband/`; a submodule would report
     `160000`. Set up by commit **`639a6613` (2026-05-07)** "Repo cleanup: vendor libs/* inline",
     which is also the ONLY commit ever to touch `libs/NeuralAmpModelerCore`.

### NAM / A2 — RESOLVED 2026-07-25. **BaySickDAW is NOT A2-capable.**

Raised by Jeff mid-batch (out of QA-Export scope; recorded here so it is not lost). Answered by
reading the upstream announcement, not by inference.

**What A2 is:** NAM's new *standard neural-network recipe* for modeling guitar and bass amps — a
model architecture, not a plugin release or a trainer. **Released June 2** (source:
`https://www.neuralampmodeler.com/post/a2-is-released`). It is what captures are being made with
now, so it is a compatibility issue, not a nice-to-have.

**Versions the announcement names as supporting A2:** neural-amp-modeler v0.13.0,
**NeuralAmpModelerCore v0.5.2**, NeuralAmpModelerPlugin v0.7.14.

**What we have: Core ~0.4.0**, vendored 2026-05-07 — four weeks BEFORE A2 shipped.

**Why the tree looked A2-ready and is not.** `libs/NeuralAmpModelerCore/NAM/wavenet/a2_fast.cpp`,
`slimmable.cpp` and `generate_weights_a2.py` are all present, and we DEFINE `NAM_ENABLE_A2_FAST`
([CMakeLists.txt:203](../../CMakeLists.txt:203)) so `a2_fast.cpp` actually compiles in — it is
`#if defined(NAM_ENABLE_A2_FAST)` gated in full. That is **pre-release A2 work sitting inside a
0.4.0 library**, because A2 was in development before it shipped. I initially reported "you have
A2, enabled" on the strength of those files; that was wrong, and only the announcement's version
numbers exposed it. There is no compile flag that retrofits the released A2 onto 0.4.0.

**Unknown, and the thing worth testing first:** whether a real A2 `.nam` capture currently fails to
load, loads and renders wrong, or works by accident against the pre-release code. Jeff's ear test,
and it should run BEFORE the upgrade — it decides whether this is routine maintenance or a live
defect.

**Upgrade cost (bounded, but not drag-and-drop):**

1. Replace `libs/NeuralAmpModelerCore/` with v0.5.2 (Jeff obtains + places, as with LAME).
2. **Update the CMake source list** — every NAM `.cpp` is named individually
   ([CMakeLists.txt:177-190](../../CMakeLists.txt:177)), so any file added/removed/renamed between
   0.4.0 and 0.5.2 breaks the build until the list matches. Loud failure, guaranteed work.
3. **Resolve `NAM_ENABLE_A2_FAST`** — it may not exist in 0.5.2, may be default-on, or may mean
   something different. Read the real 0.5.2 source; do not assume.
4. Ear-test a real A2 capture from Tone3000.
5. Create the vendor manifest with NAM as its first row (see item 2 above).
6. ONE commit, per the decision above.

**SLOT UNASSIGNED — Jeff's placement call.** Natural home is QA-Updater, whose dev-facing watcher
exists to catch exactly this and names `NeuralAmpModelerCore` in its coverage list; against that,
A2 is the current capture format, which argues for pulling it earlier. Not decided.

**This is also the concrete cost of copies-without-a-manifest:** nothing signalled that 0.5.2
existed, and nothing in-repo recorded what our tree was, so an outdated neural amp engine sat
unnoticed for ~7 weeks. Cross-ref item 2.

### QA-Export's own §5 / §8 items — PENDING

1. **Bucket lines.** §5's QA-Export entry carries no `**Bucket:**` line; §8's table lists it under
   **System Pages** only. The held entry drafts *Cross-cutting Infrastructure, Players, Other /
   Platform / Deferred*, with **System Pages defensible as a fourth** — the largest single diff is
   `BuilderPage.cpp` (+409) and the menu work is `StandaloneEditor.cpp`. If Other / Platform /
   Deferred sticks, §8's row currently reads "(no V1 batches)" and needs populating. Jeff's call.
2. **§8's QA-Export gloss is STALE regardless of the bucket question** — it reads "Export Stems /
   Master". This batch shipped a **full-mix** export; there are no stems.
3. **Dead stub quartet routing.** `doNew` / `doSave` / `doOpen` / `doExport` at
   [BuilderPage.cpp:7718-7736](../../Source/Standalone/BuilderPage.cpp:7718) are all still present,
   zero-caller, carrying stale "Phase 5" comments. The export dialog was built on StandaloneEditor
   (`doExportAudio`), so `doExport()` was never wired. **PRE-EXISTING dead code, not this batch's
   own**, so `feedback_clean_own_batch_dead_code_in_batch` does not apply and deleting them
   unprompted would be an unrequested change. Likely home QA-Cleanup. Whether this becomes a formal
   §9 Forks route or just rides the Work Log entry as a log is **Jeff's call**.

## Held Work Log entry (apply at section pass)

> Drafted at code-complete via `/draft-doc batch-close`; NOT yet applied to `Implemented Work Log.md`.
> Applies with the §5 STATUS flip when §B.29 passes the campaign walk (R2). Backfill the commit hash
> (here and in the §B.29 `blocks:` line) at commit; stamp the full `HH:MM PT` at apply.

### 2026-07-25 <HH:MM> PT — QA-Export — Export did not exist before this batch: `doExport()` was a dead stub and the File menu's "Export as WAV/MP3" items had no dispatch cases at all, so both were silent no-ops. Five tasks landed in the plan's written order (Jeff rejected my proposed reorder): libmp3lame 3.100 vendored and linked static behind `BAYSICK_HAS_LAME` with a new `Mp3Writer`; `renderPatternToWav`'s harness generalized into ONE `renderToFile` serving both song and pattern scope, moved off the message thread with progress + cancel, driven by a tempo-map-aware offline clock that resolves beats through SECONDS so the render rate may differ from the device rate; one "Export Audio..." dialog reshaped mid-task on Jeff's spec (Selection / Tail / Format / one quality dropdown) whose tail now runs to actual decay instead of a typed number; a free-standing `ProjectBundler` that QA-ProjectSave's Pack reuses; and the folded-in NAM missing-file finding, which the mandated sweep turned into a FOUR-engine fix. Two corrections to my own framing are recorded rather than buried: static LGPL linkage is not a compliance problem for an open-source free product, and the missing-file bug was never NAM-specific

**Bucket:** Cross-cutting Infrastructure, Players, Other / Platform / Deferred. Batch `loud-bouncing-walrus`. `blocks:` `87439346`.
*(Bucket + §8 gloss corrections are HELD — see this file's PENDING ledger. §5's QA-Export entry
carries no `**Bucket:**` line; §8's table lists QA-Export under **System Pages** only, with the
stale gloss "Export Stems / Master" — this batch shipped a full-mix export, not stems. System Pages
is defensible as a fourth bucket: the largest single diff is `BuilderPage.cpp` (+409). Jeff's call.)*

#### Done

- **Task 1 — libmp3lame 3.100 vendored, tree upstream verbatim except ONE file.** Version confirmed
  by reading [`libs/lame/libmp3lame/version.h`](../../libs/lame/libmp3lame/version.h) (`LAME_MAJOR 3`
  / `LAME_MINOR 100` / `LAME_TYPE_VERSION 2` = release), not inferred from the folder name. License
  present and unmodified at [`libs/lame/COPYING`](../../libs/lame/COPYING). **Jeff obtained and
  placed the source himself** after I gave him the upstream location — vendoring a third-party tree
  is not something to do unprompted, and it blocked Task 1 at batch open. Only addition:
  [`libs/lame/config.h`](../../libs/lame/config.h), a one-line forward to upstream's `configMS.h`.
  Nothing removed or pruned (`feedback_dont_overprune_vendored_libs`).
- **Task 1 — CORRECTION TO MY OWN FRAMING: static LGPL linkage is not a problem here.** I flagged
  static linking as the risk and built the argument around a closed-source product; **Jeff corrected
  that BaySickDAW is open source and free**, which dissolves it. The LGPL's relinking requirement
  exists so a user can substitute their own build; with published source they already can, by
  rebuilding. The object-file dance is the workaround for projects that will NOT publish source.
  Recorded explicitly so a later reader does not re-litigate off my bad framing. No reason found to
  switch to dynamic linking.
- **Task 1 — build wiring mirrors the `BaySickRubberBand` precedent**: `BaySickLame` STATIC target
  with the same `EXISTS` guard (missing `libs/lame` degrades to "MP3 disabled" rather than a broken
  build) plus `BAYSICK_HAS_LAME`. Sources = `libmp3lame/*.c` + `libmp3lame/vector/*.c` +
  `mpglib/*.c`. `/W0` + `_CRT_SECURE_NO_WARNINGS` scoped to the LAME target only. The `/MD` runtime
  match came free from the global `CMAKE_MSVC_RUNTIME_LIBRARY` — the
  `reference_msvc_runtime_md_md_match` trap did not bite.
- **Task 1 — two build failures, both mine, both fixed by reading upstream rather than guessing.**
  (a) `uint32_t` unknown everywhere: `machine.h` includes `<stdint.h>` only under `HAVE_STDINT_H`,
  normally from the autotools `config.h` we never generate. **The obvious fix would have been
  wrong** — defining `HAVE_STDINT_H` collides with `configMS.h`, which typedefs `int8_t..uint64_t`
  itself for MSVC ([configMS.h:55-68](../../libs/lame/configMS.h:55)). Correct fix uses upstream's
  own MSVC config. (b) `init_xrpow_core_sse` unresolved: I had excluded `libmp3lame/vector/` behind
  a confident comment claiming it needed NASM. **That was false** — it is one C file using
  `<xmmintrin.h>` intrinsics, which MSVC compiles natively; only `i386/*.nas` is assembly, and on
  x64 `quantize.c` binds the SSE quantizer unconditionally
  ([quantize.c:101-104](../../libs/lame/libmp3lame/quantize.c:101)). Side benefit: the encoder is
  faster than the pure-C build I intended.
- **Task 1 — `Mp3Writer`.** `lame_encode_buffer_ieee_float` takes -1..+1 PLANAR floats, exactly
  JUCE's layout, so the export path does no interleaving or integer conversion. CBR only
  (`vbr_off`); `lame_set_quality(2)` since offline speed is free. **The Xing/LAME header is stamped
  AFTER the stream closes** because it records stream length — skip it and the file plays but
  duration and seeking are wrong (§B.29 XP-3). Offline-only, stated in the header: it allocates.
  Compiles to a stub when `BAYSICK_HAS_LAME` is unset so callers need no `#if`.
- **Task 2 — one harness, two entries.** `renderToFile` serves both scopes; `renderPatternToWav` is
  a thin wrapper. No duplicated block loop survives.
- **Task 2 — the song-mode addition that would have shipped silently broken:**
  `rebuildAudioClipPlayers()` must be called explicitly. The live processor's clip snapshot is
  published by the EDITOR; a fresh render processor has no editor, so **every arrangement clip would
  render silent** while the rest of the mix sounded right — and it would have passed a build gate
  cleanly. Alongside: `setSongMode(true)` after state apply, and `setCurrentProjectFolder()` copied
  from the live processor (clips store paths relative to it).
- **Task 2 — shared `PatternManager::getSongEndBeats()` replaces duplicated math.** Export and the
  transport loop must stop at the SAME beat. Semantics preserved exactly, **including QA-H Task 8
  (#6) — muted blocks still COUNT**. Deliberately NOT `getTotalArrangementBars()` (16-bar floored).
  §B.29 XP-13 guards the transport side.
- **Task 2 — tempo-map-aware offline clock, two traps avoided.** (a) No `bpm * elapsedSeconds` —
  that drifts from the live playhead the moment a tempo change lands; `OfflineHead` reads `TempoMap`,
  the same timeline the live playhead uses. (b) **Sample-rate domain mismatch, silent and would read
  as a mystery timing bug** — `TempoMap`'s domain is the LIVE device rate, so 48k-from-44.1k would
  skew every beat ~9%. Beats resolve through SECONDS instead. §B.29 XP-10 / XP-11.
- **Task 2 — off the message thread, and the async switch exposed a real lifetime hole.**
  `runThread()` does not exist here (gated behind `JUCE_MODAL_LOOPS_PERMITTED`), so the job went
  async via `launchThread()` and retires itself in `threadComplete`. That surfaced something I would
  otherwise have shipped: a long export can outlive the Builder tab, so the job holds a
  `Component::SafePointer<BuilderPage>`. The pattern render inherits all of it — it previously froze
  the UI for the entire render.
- **Task 2 — Windows file-handle ordering:** the handle is released BEFORE any delete, or a
  cancelled export leaves the partial behind (§B.29 XP-9).
- **Task 3 — dead menu confirmed and replaced.** Items 120/121 had **no dispatch cases** (the switch
  jumps 110 -> 130). One "Export Audio..." now reuses id 120; 121 retired.
- **Task 3 — Ctrl+R bound, verified free FIRST.** Grepped the tree for `'R'` with any modifier: zero
  hits. Bare `'R'` stays Toggle Recording. `cmdExportAudio = 0x10024` carries name + description so
  it lists in Key Binds and is rebindable.
- **Task 3 — dialog reshaped mid-task on Jeff's spec, and he corrected a real semantic error.** My
  first version was six controls, three always irrelevant, with a typed tail; I flagged the clutter
  as a smoke-time judgement call rather than fixing it. Final: **Selection -> Tail -> Format ->
  Quality -> Sample rate**. "Selected Section" is **disabled** with no ruler selection rather than
  silently falling back to the whole song. Tail applies to BOTH modes. Quality is ONE dropdown
  repopulated from the format combo's `onChange`.
- **Task 3 — the typed "Tail (seconds)" field was WRONG and is gone.** A fixed 2 s truncates a long
  reverb and pads a dry mix. `Tail::Included` renders until output holds below **-100 dBFS for
  0.25 s** (the window matters — a decaying reverb passes through near-zero between peaks).
  **Silence is evaluated ONLY during the tail**, or a quiet intro would end the render early (§B.29
  XP-5). **60 s ceiling**, confirmed with Jeff as a MAXIMUM not a duration — it only bites on content
  that never decays. Section renders start the clock at the selection, not bar 1 (§B.29 XP-6).
- **Task 3 — three build failures, all mine:** braced-init-list subscript; `getCurrentProjectName`
  does not exist (`getCurrentName()` — **and I had wrongly claimed that line "compiled fine" when
  earlier syntax errors had halted compilation before reaching it**); `renderPatternToWav` still
  setting the removed `tailSeconds`.
- **Task 4 — `ProjectBundler` is a free-standing utility, not dialog code**, because
  QA-ProjectSave's Pack reuses it. `enumerate` walks the audio library AND every arrangement block,
  deduped — **both passes required**, since a clip can be in one and not the other.
  **Classification drives copying**: `ProjectRelative` travels free; `UserSamples` + `Absolute` must
  come along; `CoreLibrary` only for **SelfContained**. **Missing files REPORTED, never dropped**
  (§B.29 XP-17). Refuses upfront when the project was never saved.
- **Task 4 — build failure (mine): `Pattern` has no `arrangement` member.** The arrangement is
  project-GLOBAL, via `getNumBlocks()`/`getBlock(i)`. `enumerate` also takes a non-const
  `PatternManager&` because `getBlock()` has no const overload — stated in the header.
- **Task 5 — the sweep changed the task: this was never a NAM bug.** The same silent-skip existed in
  FOUR engines — NAM pedal plus Guitars / Basses / RustyDrums, all
  `if (kit.existsAsFile()) loadKit(kit);` with no else. **Fixing only NAM would have left three
  identical traps.**
- **Task 5 — one collector, one dialog.** [MissingFileReport.h](../../Source/MissingFileReport.h) is
  thread-safe (engine restores can run off the message thread; the drain is message-thread), drained
  ONCE at the end of `deserializeProject` — warning per site would stack four dialogs (§B.29 XP-19).
  **The NAM pedal no longer displays a name it did not load** (`" (missing)"` behind `mModelMissing`)
  — that was the sharp edge: the UI read as loaded while the pedal did no amp modeling.
- **About box now credits LAME**, with an inline comment pointing at QA-LegalReview so the list is
  not mistaken for authoritative.
- **Build.** Per-task gate at the end of every task, BOTH configs clean each time.
- **Master Test Plan §B.29 authored — 19 scenarios (XP-1..XP-19)**, reconciled against what shipped.
  Almost all of it is NEW capability needing listening, not clicking; only XP-12 / XP-13 are
  regression guards. Five MUST-PASS: **XP-1** (WAV export matches live playback, arrangement clips
  called out as the silent-failure risk), **XP-4** (Tail Included vs Cut), **XP-6** (Selected Section
  — failure mode is right length, wrong content), **XP-17** (bundle never hides missing files),
  **XP-18** (missing engine files announced, across all four engines).
- **Main Plan untouched.** All §5/§8/§9 items HELD in this file's PENDING ledger per Jeff's standing
  instruction 2026-07-25.

#### Found along the way

1. **`BaySickNAMIRProcessor`'s mic IRs had the same bug, in FOUR more sites, plus a swallowed
   error.** I first recorded this as untraced; Jeff pushed and it took two greps to confirm.
   Both `applySnapshot` and the `setStateInformation` project-load loop skipped a missing IR
   silently AND discarded `loadUserIr`'s error on files that existed but failed to load.
2. **The About box list is incomplete, and my first claim about it was wrong.** I said there was "no
   user-visible attribution surface anywhere"; **Jeff corrected that** — Help > About has existed all
   along and already credited sfizz.
3. **Copies-vs-submodules came up mid-batch**, prompted by this batch patching a vendored tree.
4. **We are NOT A2-capable, and the tree looks like we are.** Detail in the NAM/A2 section above.
5. **`ProjectBundler::enumerate` does not see everything a project needs** — no per-engine walk.
6. **The dead `doNew`/`doSave`/`doOpen`/`doExport` stub quartet is still there** (zero-caller, stale
   "Phase 5" comments). `doExport()` was never wired because the dialog was built on StandaloneEditor.

#### What was done about each finding

- **Finding 1: FIXED after Jeff pushed back on the deferral.** I recorded it as untraced;
  he said double-check and fix if needed. Four more sites plus a swallowed `loadUserIr` error.
  Deferring was the wrong call — tracing cost two greps, not the investigation I implied.
- **Finding 2: ROUTED to QA-LegalReview, HELD for G4 close** (PENDING ledger item 1).
- **Finding 3: DECIDED by Jeff — NOT a QA-LegalReview item; QA-Updater already owns the mechanism.**
  Copies stay; lib upgrade + forced app changes land in ONE commit. Full reasoning in the ledger.
- **Finding 4: NOT fixed — out of QA-Export scope, SLOT UNASSIGNED (Jeff's call), held for G4 close.**
  The thing worth doing FIRST is an ear test on a real A2 capture, which decides whether this is
  routine maintenance or a live defect.
- **Finding 5: MISHANDLED — corrected 2026-07-26.** Originally recorded as "DOCUMENTED in the
  header, so no caller treats the list as complete. Closing it is QA-ProjectSave's territory."
  Both halves were wrong. A header comment is not a report, so Jeff never learned of it; and the
  routing was self-assigned when deferral is his call. It was then routed to a batch that was
  planned and executed WITHOUT a task for it (badger Tasks 1-7 cover templates, menus, the Rusty
  prompt, sample retention and the bundle prompt — none walks engine references), so it was on
  track to die silently at G4 close carrying a "documented" tag. **User-visible consequence, in
  shipped code:** Export Project Bundle in BOTH modes omits NAM captures, user IRs and
  engine-loaded sample folders, and reports nothing missing, because `enumerate` never looks for
  them. Re-surfaced to Jeff in chat 2026-07-26; slot + full-export semantics are his calls, OPEN.
- **Finding 6: LEFT IN PLACE, routing HELD** (PENDING ledger item 3). PRE-EXISTING dead code, so
  `feedback_clean_own_batch_dead_code_in_batch` does not apply and deleting unprompted would be an
  unrequested change.

#### Group review (R3)

- **Pending — runs at the G4 boundary** (after `clean-pointing-stoat`'s commit).

#### Diagnostic Instrumentation Catalog

- **NONE added this batch.** Every task was resolved by reading code plus the per-task compile gates.
  The `AlertWindow` uses added here are user-facing product surfaces (export dialog, bundle dialog +
  report, missing-files dialog), not diagnostics — nothing to strip later.
- **NONE removed.** No pre-existing instrumentation was in scope.

#### Carry-forward contradictions

- **None.** Audio-thread rules hold: the offline processor runs `processBlock` on a worker thread, so
  the same RT rules apply inside the loop, and G1's beat-domain lesson is exactly what produced the
  seconds-domain conversion. Five notes to carry: (1) song end is `getSongEndBeats()` and BOTH the
  transport and exporter use it — never reintroduce inline song-end math, never substitute
  `getTotalArrangementBars()`; (2) `TempoMap`'s sample domain is the LIVE device rate, so any future
  offline consumer must convert through seconds; (3) a render processor has no editor, so anything
  the editor publishes must be rebuilt explicitly; (4) `ProjectBundler::enumerate` is the single
  reference walker — QA-ProjectSave's Pack must reuse it, not fork it; (5) `MissingFileReport` is the
  collector for any future external-file restore path — add to it, do not pop a new dialog.

#### Commit(s)

`87439346` (whole batch — Tasks 1-5 + the NAMIR mic-IR fix + §B.29 + the QA-Soundness plan +
running-notes seed + held entry + running notes; 352 files / +200,579 / -82, of which ~326 are the
vendored `libs/lame` tree). 19 modified files, 7 new files (`Mp3Writer.h/.cpp`, `MissingFileReport.h`,
`ProjectBundler.h/.cpp`), plus the vendored `libs/lame` tree riding this commit. Preceded by
QA-Verify `c6f5fd61`. Build clean in BOTH configs at every task gate; behavioral verification
deferred to the R2 campaign pass against §B.29. **Nothing here has been heard yet** — `Mp3Writer`
had no caller until Task 3, so no MP3 has ever been encoded by this app.

#### Next action

- Proceed to **QA-ProjectSave** ([`deep-packing-badger.md`](../Batch%20Plans/deep-packing-badger.md)),
  G4 batch 6 of 8.
