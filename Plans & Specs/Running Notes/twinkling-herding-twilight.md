# Running Notes — QA-A (twinkling-herding-twilight)

> **Purpose.** Append-only mid-batch log of what was done, what was found, what
> was decided, and what was deferred during QA-A execution.  Compiled from
> `/draft-doc running-notes` dispatches at every significant checkpoint
> (commit landed / sub-task verified / finding captured / decision made /
> scope pivot / spec call resolved).
>
> At batch close, `/draft-doc batch-close` reads this file (plus git log,
> memory entries, the per-batch plan, and conversation context) and produces
> the single Implemented Work Log entry that goes into `Plans & Specs/
> Implemented Work Log.md`.  This file is the source-of-truth intermediate
> artifact during the batch; the close entry is the durable summary.
>
> **Pair file:** `Plans & Specs/Batch Plans/twinkling-herding-twilight.md`
> (the per-batch plan).
>
> **Convention:** see `Plans & Specs/Main Plan.md` §0 (folder-scope rule +
> Agent Orchestration Rules' mid-batch checkpoint trigger).  Established as a
> new subfolder 2026-05-09 mid-QA-A after Jeff flagged the gap that running
> notes had no documented home.

---

## 2026-05-09 — Retrospective backfill (initial entry)

> Compiled at the back-half of QA-A as a catch-up pass — running notes were
> not kept incrementally up to this point.  Rule now memory-locked
> (`feedback_draft_doc_running_notes_every_checkpoint.md`); future
> dispatches at every checkpoint.

### Done so far

#### Task 0 — Open the QA-A batch (commit `314fe37`)
- Mirrored `~/.claude/plans/twinkling-herding-twilight.md` -> `Plans & Specs/Batch Plans/twinkling-herding-twilight.md`.
- Updated `**Plan file:**` pointer on the QA-A entry in Main Plan §5.

#### Phase 1 — BaySickTitleBar component (commits `d9a95be`, `9c915c2`)
- Step 1.1: created `Source/Standalone/BaySickTitleBar.h` + `.cpp` per the API block in the plan.  Standardized 32 px height, 16 pt bold font, 8 px padding, accent color taken at construction (no LAF coupling), optional `bool bloom` flag.
- Step 1.2: added `Source/Standalone/BaySickTitleBar.cpp` to the Standalone source cluster in `CMakeLists.txt`.
- Step 1.3: build clean (`RELEASE_EXIT_CODE=0`, `DEBUG_EXIT_CODE=0`); commit `d9a95be` (skeleton).
- Step 1b (corrective recommit `9c915c2`): bloom math reworked twice — see Found-along-the-way item 1.  Final implementation converts the engine-name string to a `juce::GlyphArrangement` -> `juce::Path`, strokes the path at 2.5 px / 30% accent alpha, then fills the same path crisp on top.  Symmetric halo around every glyph contour, halo + crisp text pixel-aligned by construction.  Bloom default flipped from `false` to `true` so all engines get the halo unless they explicitly opt out.
- Commit `d9a95be` was amended once (previous hash `582ab46`) to drop a stray "VibePlayer" mention from the body — see Found-along-the-way item 9.
- Commit `9c915c2` was amended once (previous hash `c2e1669`) to fix stray backslashes in the message body.

#### Phase 2 — BaySickPlayer proof-of-concept (commit `8fd4584`)
- Tasks 2.1-2.6: `VibePlayerEditor` adopts `BaySickTitleBar`.  `mTitleLbl` member deleted; `mTitleBar { "BaySickPlayer", juce::Colour (0xFFD4A017) }` added inline.  Header paint block deleted from editor (TitleBar owns it).  `kHdrH` shifted 36 -> 32 (4 px more body).  Preset + help button laid out via `mTitleBar.getTrailingArea(110 + 8 + 24)`.
- Build clean; Debug + Release verified by Jeff.

#### Phase 3 — Sweep existing title bars

- **Task 3.1 - HarmlessEditor (commit `9882630`)**: `mTitleBar { "Harmless", juce::Colour (HarmlessLAF::kAccent), /*bloom*/ true }`.  `kHdrH` 36 -> 32.  Bloomed paint block at lines 631-642 of the old `HarmlessEditor.cpp` deleted; the new path-stroke halo replaces it.  Preset button laid out via `getTrailingArea(86)`.
- **Task 3.2 - BaySickSynthEditor (commit `1face93`)**: `mTitleBar { "BaySickSynth", juce::Colour (BaySickSynthLAF::kGreen) }`.  STYLE-06 fix — preset dropdown moved LEFT -> RIGHT via `getTrailingArea(88)`.  Title text now in FL green on the left.
- **Task 3.3 - BaySickBassEditor (commit `4a02615`)**: same shape as Synth.  `mTitleBar { "BaySickBass", juce::Colour (BaySickBassLAF::kGreen) }`.  Preset L -> R; B1 neon green title.
- **Task 3.4 - BaySickNAMIREditor (NOT YET COMMITTED — see In-flight)**: `mTitleBar { "BaySickNAM/IR", juce::Colour (0xFFE0303F) }` (Mesa red).  `kHeaderH` 28 -> 32 (4 px more chrome).  Slot A/B buttons re-anchored via `getTrailingArea(2 * kSlotABtnW)`.  Old title-strip fill + divider lines deleted.

#### Mid-Phase plan-doc edits

- **Commit `d529602`**: scope expansion of Phase 4 — added Task 4.3 (AriaControlPanel `Binding` extension), Task 4.4 (InstPage `kHeaderRowH` removal + BaySickGuitars / BaySickBasses wiring), Task 4.5 (BaySickRustyDrumsPage title bar).  Per-engine accent table grew from 7 entries to 10.  Pre-amble note explains the scope expansion.  See Found-along-the-way item 7.
- **Commit `0c4431d`**: casing correction sweep — engine names in source files + plan doc converted from ALL CAPS ("HARMLESS" / "BAYSICKPLAYER" / etc.) back to brand mixed-case ("Harmless" / "BaySickPlayer" / etc.).  Also corrected Task 4.2's title text from "BaySickGuitars" -> "BaySickPedals" (Pedals and Guitars are distinct engines; original STYLE-04 phrasing conflated them).  See Found-along-the-way item 4.

### In-flight

- **Phase 3.4 NAMIR** — code edits applied, builds, but the commit is pending behind a defensive HarmlessLAF guard (see Found-along-the-way item 5).  Both will land in the next commit pair.
- **Uncommitted source files**:
  - `Source/Harmless/HarmlessLAF.h` defensive early-return at top of `LinearVertical` branch in `drawLinearSlider` when `width <= 0 || height <= 0`.
  - `Source/BaySickNAMIR/BaySickNAMIREditor.h` + `.cpp` — title bar adoption.
- **Uncommitted doc files**: this Running Notes file + a `Main Plan.md` §0 update documenting the new `Running Notes/` subfolder + a memory entry update for the new file location.

### Found along the way

1. **Bloom math iterated three times.** Original Step 1 implementation used a size-delta bloom (17 pt underlay at offset (-1, -1), 16 pt overlay at (0, 0)).  Jeff caught it on Harmless verification — read as a directional shadow, not a halo, because both texts share the left-aligned edge so the larger underlay only extended right + top + bottom, not left.  First fix attempt: removed the (-1, -1) offset, kept size delta — still read as directional shadow for the same reason.  Final fix (commit `9c915c2`): convert engine name to `juce::GlyphArrangement` -> `juce::Path`, stroke at 2.5 px / 30% accent alpha, then fill the same path crisp on top.  Symmetric halo on every glyph contour.  Bloom default flipped to `true` per Jeff's approval.  Future engines opt out via `bloom = false` if their visual identity calls for it.

2. **SettableTooltipClient detour during Phase 3.4 NAMIR.** Original BaySickNAMIR title label had a hover tooltip ("Neural Amp Modeler + IR cabinet").  Tried extending `BaySickTitleBar` to inherit from `juce::SettableTooltipClient` to preserve it.  After rebuild, picking Harmless crashed in `HarmlessLAF::drawLinearSlider`.  Reverted the SettableTooltipClient base; restored the original `setInterceptsMouseClicks(false, true)` line.  Tooltip dropped permanently per Jeff's call (option 1 of 3 surfaced via plain text discussion).  The Harmless crash turned out to be a separate latent bug, not caused by the SettableTooltipClient change — see item 5.

3. **Plan-mirror rule revised mid-batch.** Plan-mode forces the planning file to `~/.claude/plans/<silly-name>.md` for its UI; the project canonical location is `Plans & Specs/Batch Plans/<silly-name>.md`.  Original behavior: mirror to the canonical path on ExitPlanMode, then `cp` back to the home-dir copy after any mid-batch edit "to keep them in sync."  Jeff caught the duplicate.  Revised rule: mirror once on ExitPlanMode AND DELETE the home-dir copy, so only one version exists from then on.  Memory file `feedback_plan_mirror_one_way.md` created and indexed.

4. **Casing correction.** Steps 2 / 3 / 4 / 5 shipped with engine names in ALL CAPS ("HARMLESS" / "BAYSICKPLAYER" / "BAYSICKSYNTH" / "BAYSICKBASS") because the legacy paint code had it that way.  Jeff caught the unilateral up-casing during Phase 3.4.  Locked convention: brand mixed-case ("Harmless" / "BaySickPlayer" / etc.) for every UI surface; never up-case unilaterally even when a legacy source string had it that way.  Commit `0c4431d` swept source files + per-engine accent table in the plan doc + every in-task ctor / verify reference.  Memory file `feedback_match_jeff_text_casing.md` created and indexed.

5. **HarmlessLAF latent bug surfaced by Phase 3.1.**  Picking Harmless crashed in `HarmlessLAF::drawLinearSlider` -> `g.fillRoundedRectangle` -> Direct2D `coordsToRectangle` assert on NaN/negative coords.  Root cause: when a vertical slider has zero-sized bounds (not yet laid out, or computed to 0), `fh = (float)height = 0` makes the `norm` calc divide by zero, NaN propagates through `thumbY` and the cap rect into Direct2D's clip-list assert.  Latent bug; Phase 3.1's `kHdrH 36 -> 32` shift in HarmlessEditor body layout surfaced it on Harmless engine pick.  **Defensive fix queued for next commit**: early-return at top of `LinearVertical` branch when `width <= 0 || height <= 0`.  The upstream "why is the slider 0 px in the first place" question stays open as a Phase 6 follow-up.

6. **BaySickPedals title text correction.** Original plan had Phase 4.2's BaySickPedalsEditor title text as "BaySickGuitars" per STYLE-04's literal phrasing.  Jeff clarified mid-execution that BaySickPedals is a distinct engine from BaySickGuitars (the latter is sfizz-driven and shares AriaControlPanel; the former is the FX rack).  Title text corrected to "BaySickPedals" in the plan doc; original STYLE-04 phrasing documented as historical at Task 4.2 step 4.2.3 and in the per-engine accent table.

7. **Scope expansion 2026-05-09 (commit `d529602`).**  Mid-execution Jeff flagged that BaySickGuitars / BaySickBasses / BaySickRustyDrums were missing from the original plan.  They share `AriaControlPanel` for kit-artwork rendering and have no BaySickDAW-style chrome of their own, so each needs its own title bar.  Decision: extend `AriaControlPanel`'s `Binding` struct with optional `engineName` + `accentColor` fields (Phase 4.3); InstPage configures the binding per source mode (Phase 4.4); BaySickRustyDrumsPage adds its own title bar above the existing menu buttons (Phase 4.5).  Pedals + Guitars + Basses share navy accent (#1C3A8A — Inst tab active color); RustyDrums uses Drums tab red (#CC2222).  Four colors confirmed via AskUserQuestion 2026-05-09.

8. **InstPage parent chrome black bar.**  After the BaySickNAMIR title bar landed in the Phase 3.4 working tree, Jeff noticed an "extra black bar" between the BaySickTitleBar and the page-level sub-tab buttons above.  Diagnosis: it's InstPage's own `kHeaderRowH = 36` chrome strip (paint at `Source/Inst/InstPage.cpp:1192-1199`), originally there to host the pedalboard preset button + engine title text.  Removal queued for Phase 4.4 (depends on Phase 4.2 migrating the preset button into BaySickPedalsEditor first).  Verification step 4.4.8 includes "no leftover dark chrome strip from the old InstPage header" as a pass condition.

9. **VibePlayer -> BaySickPlayer rename side-finding.**  Internal source file/class names still use `VibePlayer*` (per CLAUDE.md "internal source is still `VibePlayer*`; class / file renames deferred").  User-facing brand is `BaySickPlayer`.  Jeff flagged commit-message hygiene mid-batch: future commits use `BaySickPlayer` in body text (file paths in diffs unavoidably show `Source/VibePlayer/...` until the rename lands).  Commit `582ab46` was amended -> `d9a95be` to drop a stray "VibePlayer" reference.  **Side-finding to route at QA-A close per Rule 3**: rename `Source/VibePlayer/*` -> `Source/BaySickPlayer/*` + class renames; queued for Phase 6 QA-Cleanup-1 (or a dedicated batch — parent to decide at close).

10. **Workflow gap — `/read-doc` underuse.**  Used direct `Read` on Plans & Specs docs throughout the session instead of `/read-doc` (the doc-reader agent).  Jeff flagged this; going forward, `/read-doc` for first-time lookups, direct `Read` only for content already in context.

11. **Workflow gap — running-notes not dispatched.**  Eleven QA-A commits with zero `/draft-doc running-notes` dispatches.  Jeff flagged this; rule now memory-locked.  This running-notes pass is the retrospective backfill.  Going forward: every checkpoint dispatches it.

12. **Running-notes home gap — established a new subfolder.**  §0's folder-scope rule explicitly invites new subfolders for new artifact types.  Running notes had no documented home.  Created `Plans & Specs/Running Notes/` 2026-05-09 with `<silly-name>.md` per-batch convention matching `Batch Plans/`.  §0's approved-subfolders list updated in the same commit.

### Decisions made

- **D1**: `BaySickTitleBar` accent is a `juce::Colour` parameter at construction, no LAF coupling inside the component. (AskUserQuestion 2026-05-09.)
- **D2**: Bloom is opt-in per ctor flag.  Default flipped from `false` -> `true` at Step 1b after Jeff approved the path-stroke halo.  Future engines opt out via `bloom = false` if needed.
- **D3**: Bloom implementation = path-stroke halo (`juce::GlyphArrangement` -> `juce::Path`, 2.5 px stroke at 30% accent alpha, then crisp fill on top).  Replaces the original size-delta-with-offset approach which read as a directional shadow.
- **D4**: Casing convention = brand mixed-case throughout.  Never up-case engine names to ALL CAPS.  Locked 2026-05-09; memory file `feedback_match_jeff_text_casing.md`.
- **D5**: Plan file mirrors once to `Plans & Specs/Batch Plans/<silly-name>.md` on ExitPlanMode and the home-dir copy is deleted; never re-mirror after edits.  Locked 2026-05-09; memory file `feedback_plan_mirror_one_way.md`.
- **D6**: Phase 4 scope expanded to include AriaControlPanel-shared engines (BaySickGuitars / BaySickBasses) and BaySickRustyDrums.  Each gets its own title bar.  AriaControlPanel's `Binding` struct gains optional `engineName` + `accentColor` fields.
- **D7**: Pedals + Guitars + Basses share navy accent #1C3A8A (Inst tab active color).  RustyDrums uses Drums tab red #CC2222.  Confirmed via AskUserQuestion 2026-05-09.
- **D8**: BaySickNAMIR title-label tooltip dropped permanently (no SettableTooltipClient base on `BaySickTitleBar`).
- **D9**: HarmlessLAF zero-bounds NaN crash gets a defensive guard now; the upstream "why is the slider 0 px" question gets routed to Phase 6 (QA-Audit / QA-Cleanup) — not chased inside QA-A.
- **D10**: Title text "BaySickPedals" (corrected from "BaySickGuitars" per the original STYLE-04 phrasing).  Pedals and Guitars are distinct engines.
- **D11**: Running notes get a new subfolder home: `Plans & Specs/Running Notes/<silly-name>.md`.  §0 approved-subfolders list updated 2026-05-09.

### Files touched (so far)

**Created:**
- `Source/Standalone/BaySickTitleBar.h`
- `Source/Standalone/BaySickTitleBar.cpp`
- `Plans & Specs/Running Notes/twinkling-herding-twilight.md` (this file)

**Modified (committed):**
- `CMakeLists.txt`
- `Source/VibePlayer/VibePlayerEditor.h` + `.cpp`
- `Source/Harmless/HarmlessEditor.h` + `.cpp`
- `Source/BaySickSynth/BaySickSynthEditor.h` + `.cpp`
- `Source/BaySickBass/BaySickBassEditor.h` + `.cpp`
- `Plans & Specs/Batch Plans/twinkling-herding-twilight.md` (Task 0 mirror; commits `d529602` + `0c4431d` for scope expansion + casing)
- `Plans & Specs/Main Plan.md` (`**Plan file:**` pointer added to QA-A entry)

**Modified (working tree, uncommitted):**
- `Source/BaySickNAMIR/BaySickNAMIREditor.h` + `.cpp` (Phase 3.4 NAMIR refactor)
- `Source/Harmless/HarmlessLAF.h` (defensive zero-bounds guard)
- `Plans & Specs/Main Plan.md` (Running Notes subfolder added to §0 approved list — current edit batch)

**Created during batch (memory):**
- `~/.claude/projects/.../feedback_plan_mirror_one_way.md`
- `~/.claude/projects/.../feedback_match_jeff_text_casing.md`
- `~/.claude/projects/.../feedback_draft_doc_running_notes_every_checkpoint.md`
- All indexed in `~/.claude/projects/.../MEMORY.md`.

### Commits

| Hash | Phase | Description |
|------|-------|-------------|
| `314fe37` | Task 0 | open batch with plan file reference |
| `d9a95be` | Phase 1 | scaffold BaySickTitleBar shared component (skeleton) — amended once from `582ab46` to drop stray "VibePlayer" mention |
| `9c915c2` | Phase 1b | BaySickTitleBar bloom uses path-stroke halo, on by default — amended once from `c2e1669` to fix stray backslashes in body |
| `8fd4584` | Phase 2 | BaySickPlayer editor adopts BaySickTitleBar (PoC) |
| `9882630` | Phase 3.1 | Harmless editor adopts BaySickTitleBar |
| `1face93` | Phase 3.2 | BaySickSynth editor adopts BaySickTitleBar |
| `4a02615` | Phase 3.3 | BaySickBass editor adopts BaySickTitleBar |
| `d529602` | plan-doc | expand plan scope to cover BaySickGuitars / BaySickBasses / BaySickRustyDrums |
| `0c4431d` | plan-doc + source | correct engine title casing to brand mixed-case |
| `c900f55` | meta-doc | establish Running Notes/ subfolder + §0 update + seed twinkling-herding-twilight.md |
| `679af33` | source | guard HarmlessLAF::drawLinearSlider against zero-sized bounds |
| `27a10bd` | Phase 3.4 | BaySickNAM/IR editor adopts BaySickTitleBar |
| `8c5924c` | running-notes | append Phase 3 close checkpoint |
| `1a31aba` | Phase 4.1 | BaySickVocal cluster (Vocals + Align + Pitch via BaySickEngineLabel) |
| `40c8be8` | running-notes | append Phase 4.1 close checkpoint |
| `652998b` | Phase 4.2 | BaySickPedals editor adopts BaySickTitleBar (preset btn migrates into trailing area) |
| `4fbe40d` | running-notes | append Phase 4.2 close checkpoint |
| `21394d5` | Phase 4.3 | AriaControlPanel hosts optional BaySickTitleBar (plumbing-only; no callers wired yet) |
| `662effb` | running-notes | append Phase 4.3 close checkpoint |
| `1d0304d` | Phase 4.4 | InstPage chrome strip removed; per-source title bars wired (Pedals preset hookup + Guitars/Basses navy bars) |

### Phases done / in flight / remaining

- Phase 1 (component scaffold + bloom upgrade) — DONE.
- Phase 2 (BaySickPlayer PoC) — DONE.
- Phase 3 (sweep existing title bars: Harmless / BaySickSynth / BaySickBass / BaySickNAM/IR) — DONE.
- Phase 4 (BaySickVocal STYLE-03 / BaySickPedalsEditor + preset btn migration / AriaControlPanel extension / InstPage cleanup / BaySickRustyDrumsPage) — ALL pending.
- Phase 5 (STYLE-01 ribbon truncation) — pending.
- Phase 6 (cross-engine consistency check) — pending.
- Phase 7 (close sequence: `/draft-doc batch-close` -> `/review-batch` -> apply close -> commit) — pending.

### Next action

Commit the Running Notes infrastructure (this file + §0 update + memory entry update) as the meta-doc commit, then the HarmlessLAF defensive guard, then the Phase 3.4 NAMIR refactor.  After all three commits land, proceed to Phase 4.1 (BaySickVocalEditor STYLE-03).

Going forward: every mid-batch checkpoint dispatches `/draft-doc running-notes` and the parent applies the returned text by appending a new `## YYYY-MM-DD HH:MM PT — <summary>` block to this file.

---

## 2026-05-09 14:12 PT — Phase 3 close + Running Notes infra landed

> Checkpoint after the four commits closing Phase 3 of QA-A.  Phase 3 is now
> fully done across Harmless / BaySickSynth / BaySickBass / BaySickNAM/IR.
> Running Notes home is established and memory-locked.

### Done since last checkpoint

- **`0c4431d` — QA-A: correct engine title casing to brand mixed-case.**  Casing fix swept across the four already-shipped player editors (HARMLESS / BAYSICKPLAYER / BAYSICKSYNTH / BAYSICKBASS) plus the per-batch plan doc.  Convention locked 2026-05-09: brand mixed-case for every UI surface, never up-case unilaterally even when legacy source had it that way.  Memory file `feedback_match_jeff_text_casing.md` indexed.
- **`c900f55` — QA-A: establish Running Notes subfolder + seed twinkling-herding-twilight.**  New `Plans & Specs/Running Notes/` subfolder added to §0's approved list; per-batch running-notes file paired with the batch plan by silly-name.  Initial entry in this file is the retrospective backfill committed in this commit.  Memory file `feedback_draft_doc_running_notes_every_checkpoint.md` updated with the concrete file location.
- **`679af33` — QA-A: guard HarmlessLAF::drawLinearSlider against zero-sized bounds.**  Defensive early-return at top of the `LinearVertical` branch when `width <= 0 || height <= 0`.  Stops the NaN-coord Direct2D `coordsToRectangle` assert that surfaced after Phase 3.1's `kHdrH 36 -> 32` shift in HarmlessEditor body layout.  Latent root cause ("why is the slider 0 px in the first place") stays open as a Phase 6 follow-up per D9.
- **`27a10bd` — QA-A Step 6: BaySickNAM/IR editor adopts BaySickTitleBar.**  Phase 3.4 close.  `mTitleBar { "BaySickNAM/IR", juce::Colour (0xFFE0303F) }` (Mesa red).  `kHeaderH` 28 -> 32 (4 px more chrome).  A/B slot toggles right-anchor via `getTrailingArea(2 * kSlotABtnW)`.  Old title-strip fill + divider lines deleted.  Tooltip dropped permanently per D8.

### Findings / decisions added

None new this checkpoint — all routings already captured in the retrospective backfill above (items 5, 8, 9, 12; decisions D4, D8, D9, D11).  Worth re-flagging for visibility:

- **InstPage parent chrome black bar** (item 8) — Jeff sees an "extra black bar" above the new BaySickNAM/IR title bar.  Confirmed in commit `27a10bd` body: it's InstPage's own `kHeaderRowH = 36` chrome strip (paint at `Source/Inst/InstPage.cpp:1192-1199`).  Removal queued for Phase 4.4 (depends on Phase 4.2 migrating the pedalboard preset button into BaySickPedalsEditor first).

### In-flight

- Working tree clean.  12 QA-A commits ahead of origin.

### Next action

Phase 4.1 — BaySickVocalEditor STYLE-03 swap to BaySickTitleBar.

---

## 2026-05-09 14:55 PT — Phase 4.1 close — BaySickVocal cluster adopts BaySickTitleBar

> Checkpoint after the single commit closing Phase 4.1.  Original scope was
> BaySickVocalsPanel only (STYLE-03 caption swap); grew mid-phase to cover
> BaySickAlign + BaySickPitch when Jeff flagged off-brand title styling on
> the two sibling sub-pages.  All three Vox sub-pages now share the same
> engine-name styling (font + color + bloom).

### Done since last checkpoint

- **`1a31aba` — QA-A Step 7: BaySickVocal cluster adopts BaySickTitleBar styling.**  Phase 4.1 close.  Three sub-pages updated in one commit:
  - **BaySickVocalsPanel** — STYLE-03 swap.  "PAGE CONTROLS" `g.drawText` caption deleted; `BaySickTitleBar` adopted.
  - **BaySickAlignEditor** — `mTitleLbl` (was 14 pt bold `juce::Label` with off-brand colors) swapped to `BaySickEngineLabel`.  Existing toolbar chrome (preset combo, save/reset/file labels) preserved.
  - **BaySickPitchEditor** — same swap as Align; multi-label declaration line split so `mTitleLbl` becomes `BaySickEngineLabel`.
  - **`BaySickTitleBar.h` / `.cpp` extensions** — added `BaySickEngineLabel` (thin `juce::Component` subclass painting just the engine name with optional bloom, no background/divider — drop-in replacement for `juce::Label` where existing chrome stays); added static `paintEngineName(g, name, accent, rect, bloom, fontSize)` helper called by both `BaySickTitleBar::paint` and `BaySickEngineLabel::paint` so bloom math stays in lock-step.  Default bloom = `true` on `BaySickEngineLabel` (matches Step 1b convention per D2).

### Findings / decisions added

- **Finding 13 — Phase 4.1 scope expansion.**  Original Phase 4.1 spec was BaySickVocalsPanel only (STYLE-03 caption swap).  After the initial work landed in the working tree but before committing, Jeff flagged that the two sibling sub-pages within BaySickVocal (BaySickAlign + BaySickPitch) already had `mTitleLbl` showing the engine name in 14 pt bold with off-brand colors.  He wanted them to match BaySickVocals's title styling (16 pt bold, teal #0FAFA5, bloom halo) without disrupting the rest of those toolbars (which have other widgets — preset combo, save/reset/file labels, etc.).  Phase 4.1 grew to cover all three Vox sub-pages; single commit covers them all.  Drove decisions D12 + D13.
- **D12 — `BaySickEngineLabel` component added.**  Drop-in replacement for `juce::Label` that paints engine names in matching `BaySickTitleBar` style (font + color + bloom) without background/divider chrome.  Used by BaySickAlign + BaySickPitch toolbars where existing chrome stays.  Default bloom = `true` (matches D2 / Step 1b convention).  Lives alongside `BaySickTitleBar` in `Source/Standalone/BaySickTitleBar.h` + `.cpp`.
- **D13 — `BaySickTitleBar::paintEngineName` static helper extracted.**  Shared between `BaySickTitleBar::paint` and `BaySickEngineLabel::paint` so bloom path-stroke logic (per D3) stays centralized — single source of truth for the GlyphArrangement -> Path -> stroke + fill pipeline.

### In-flight

- Working tree has only this running-notes append uncommitted (it'll commit immediately).  14 QA-A commits ahead of origin after this commit lands.

### Next action

Phase 4.2 — BaySickPedalsEditor adopts `BaySickTitleBar` (navy accent #1C3A8A per D7) + pedalboard preset button migrates from InstPage chrome into the title bar's trailing area.  Unlocks Phase 4.4 (InstPage `kHeaderRowH` removal) per finding 8.

---

## 2026-05-09 15:38 PT — Phase 4.2 close — BaySickPedals editor adopts BaySickTitleBar

> Checkpoint after the single commit closing Phase 4.2.  No spec pivots, no
> new findings — Tasks 4.2.1-4.2.9 executed straight against the plan.
> Coupled-batch wiring intentionally leaves a no-op preset button + visible
> InstPage chrome strip until Phase 4.4 lands the wiring + chrome removal.

### Done since last checkpoint

- **`652998b` — QA-A Step 8: BaySickPedals editor adopts BaySickTitleBar.**  Phase 4.2 close.  `mTitleBar { "BaySickPedals", juce::Colour (0xFF1C3A8A) }` (navy — Inst-tab active color, per D7).  Hosted at the top of the 4x2 pedal grid; grid origin shifts down by `BaySickTitleBar::kStandardHeight` (32 px).  The pedalboard preset button migrated from InstPage's `kHeaderRowH = 36` chrome strip into the title bar's trailing area via `mTitleBar.getTrailingArea(88)`.  New `std::function<void()> onPedalboardPresetMenu` callback hook exposed on the editor; Phase 4.4 will set it from InstPage's side (`mPedalsEditor->onPedalboardPresetMenu = [this] { showPedalboardPresetMenu(); }`).  Until then the new preset button renders but its click is a no-op AND the legacy InstPage chrome strip with the original preset button still coexists above the editor (intentional visual duplication; both go away in Phase 4.4 per finding 8).  Title text "BaySickPedals" was corrected from STYLE-04's literal "BaySickGuitars" phrasing in plan-doc commit `0c4431d` per finding 6 / D10; this commit is the source-side application.

### Findings / decisions added

None new this checkpoint.  Phase 4.2 was small and aligned with the plan; all routings (callback wiring deferral, InstPage chrome coexistence, navy accent, title-text correction) were already captured in the retrospective backfill (findings 6, 8; decisions D7, D10) and the Phase 4.1 close.

### In-flight

- Working tree clean.  16 QA-A commits ahead of origin.

### Next action

Phase 4.3 — extend `AriaControlPanel`'s `Binding` struct with optional
`engineName` + `accentColor` fields (per D6) so BaySickGuitars +
BaySickBasses + BaySickRustyDrums can render their own title bars through
the shared kit-artwork panel.  Unblocks Phase 4.4 (InstPage `kHeaderRowH`
removal + per-source-mode title-bar wiring + Pedals preset-button hookup).

---

## 2026-05-09 16:24 PT — Phase 4.3 close — AriaControlPanel BaySickTitleBar plumbing

> Checkpoint after the single commit closing Phase 4.3.  Plumbing-only
> landing — `AriaControlPanel` now CAN host a `BaySickTitleBar` but no
> caller exercises it yet, so visual output is unchanged.  Wiring lands
> in Phase 4.4 (InstPage source-mode bindings) + Phase 4.5
> (BaySickRustyDrumsPage).

### Done since last checkpoint

- **`21394d5` — QA-A Step 9: AriaControlPanel hosts optional BaySickTitleBar.**  Phase 4.3 close.  Two optional fields added to `AriaControlPanel::Binding`: `juce::String engineName {}` + `juce::Colour accentColor { juce::Colours::transparentBlack }`.  Private `std::unique_ptr<BaySickTitleBar> mTitleBar` member added; ctor / `setEngine` create / update / drop the bar based on whether `engineName` is non-empty.  `resized()` anchors it at `(0, 0, w, BaySickTitleBar::kStandardHeight)`; `paint()` trims the same `kStandardHeight` off the top before computing the kit-artwork drawing area.  Backward-compatible — every existing caller still passes an empty `engineName`, so `mTitleBar` stays null and the panel renders identically to before.  Files touched: `Source/Standalone/AriaControlPanel.h` + `.cpp`.

### Findings / decisions added

- **Finding 14 — InstPage-vs-VoxPage chrome distinction (refines finding 8).**  The "extra black bar" Jeff sees above BaySickNAM/IR is **only on the Inst page**, not the Vox page.  BaySickNAM/IR is hosted in either parent — InstPage spawns it as the source-mode FX-rack alternative; VoxPage spawns it under the BaySickVocal effect chain.  Per the comment block at `Source/Vox/VoxPage.cpp:572-577` ("page is just a thin host for BaySickVocalEditor; editor paints its own background.  No header bar anymore"), VoxPage is a thin wrapper with no parent chrome — so the BaySickNAM/IR instance there sits flush at the top.  InstPage carries its own `kHeaderRowH = 36` chrome strip with `mPedalsHeaderTitle` + `mPedalsPresetBtn` (per `Source/Inst/InstPage.cpp:1192-1199`), which is the actual source of the bar.  Phase 4.4 deletes that strip across all Inst-page sources (Pedals + Guitars + Basses + NAM/IR); VoxPage requires no work for the bar removal.  Refines finding 8 — the Vox-vs-Inst split wasn't explicit in the retrospective backfill.

### In-flight

- Working tree clean.  18 QA-A commits ahead of origin.

### Next action

Phase 4.4 — InstPage cleanup.  Removes the `kHeaderRowH = 36` parent chrome strip (deletes `mPedalsHeaderTitle` + `mPedalsPresetBtn` + the paint block at `Source/Inst/InstPage.cpp:1192-1199`) AND wires per-source-mode title configuration through the new `AriaControlPanel::Binding` fields for Pedals / Guitars / Basses (navy #1C3A8A per D7) and BaySickNAM/IR (Mesa red #E0303F per Phase 3.4).  Also hooks `mPedalsEditor->onPedalboardPresetMenu` from InstPage's side so the trailing-area button migrated in Phase 4.2 stops being a no-op.  Per finding 14, this is where the "extra black bar" finally goes away for every Inst-page engine.

---

## 2026-05-09 17:18 PT — Phase 4.4 close — InstPage cleanup, extra black bar gone

> Checkpoint after the single source commit closing Phase 4.4.  The `kHeaderRowH = 36`
> parent chrome strip is gone across every Inst-page source mode; the migrated
> Pedals preset button is now functional; BaySickGuitars + BaySickBasses exercise
> Phase 4.3's AriaControlPanel plumbing for the first time.  Closes finding 8 +
> finding 14 (the "extra black bar" item) for InstPage.

### Done since last checkpoint

- **`1d0304d` — QA-A Step 10: InstPage drops chrome strip, wires per-source title bars.**  Phase 4.4 close.  Five interlocked edits in one commit:
  - `kHeaderRowH = 36` constant deleted; dark-fill paint block at `Source/Inst/InstPage.cpp:1192-1199` deleted; the `resized()` reservation that gave it space is gone.  Each Inst-page engine UI now fills the full page area with its own title bar at y=0.
  - `mPedalsHeaderTitle` (`juce::Label`) and `mPedalsPresetBtn` (`std::unique_ptr<juce::TextButton>`) members deleted from `Source/Inst/InstPage.h`.
  - `mPedalsEditor->onPedalboardPresetMenu = [this] { showPedalboardPresetMenu(); }` wired in InstPage's constructor (`dynamic_cast<BaySickPedalsEditor*>` on `mPedalsEditor.get()`); the trailing-area preset button migrated in Phase 4.2 stops being a no-op.
  - `showPedalboardPresetMenu`'s popup target switched from `mPedalsPresetBtn.get()` (deleted) to `mPedalsEditor.get()` (whole editor as anchor).
  - `rebuildPlayerPanel()` now sets `binding.engineName = "BaySickGuitars"` / `"BaySickBasses"` + `binding.accentColor = juce::Colour (0xFF1C3A8A)` (navy per D7) — Phase 4.3's `AriaControlPanel::Binding` plumbing exercised by a real caller for the first time.  Both sfizz engines now render their own `BaySickTitleBar` at the top of the kit-artwork area.
  - `#include "../BaySickPedals/BaySickPedalsEditor.h"` added to `InstPage.cpp` for the dynamic_cast.
  - **Verification (Jeff confirmed Debug + Release):** all five Inst source modes pass — BaySickPedals (no extra bar; preset menu opens), BaySickNAM/IR (no extra bar; engine's own Phase-3.4 title bar flush at top), BaySickGuitars (new navy "BaySickGuitars" title bar over kit artwork), BaySickBasses (same with "BaySickBasses"), LiveInput (no engine UI; unchanged).

### Findings / decisions added

- **Finding 15 — Piano Roll deep-link button crash re-sighted (not a Phase 4.4 regression).**  Mid-Phase-4.4 testing, Jeff hit a crash on the Piano Roll button.  Stack: `StandaloneEditor::showPageForTab` line 4135's `<lambda_14>::operator()(int i)`.  This is the same crash family as findings 13 + 14 from QA-0a's cold-start triage (captured-raw `InstPage*` in lambda gets freed during engine swap or project reload; `juce::Component::SafePointer` or index-lookup fix queued).  Already routed to QA-E per §9 Forks 3rd entry "QA-0 close routings".  The crash path lives in `Source/Standalone/StandaloneEditor.cpp` and is **untouched** by the Phase 4.4 InstPage chrome refactor — captured here purely for traceability so the QA-A close drafter sees that the re-sighting landed during this batch's verification (no QA-A-side action required).

### In-flight

- Working tree clean.  20 QA-A commits ahead of origin.

### Next action

Phase 4.5 — `BaySickRustyDrumsPage` adds its own title bar above the existing menu buttons (Drums-tab red `#CC2222` per D7).  Last sub-phase of Phase 4 before Phase 5 (STYLE-01 ribbon truncation).

---

## 2026-05-09 17:41 PT — Phase 4.5 close — BaySickRustyDrums adopts BaySickTitleBar + tab-strip positioner

> Checkpoint after the single source commit closing Phase 4.5.  Last
> sub-phase of Phase 4.  Mid-phase scope iteration — tab strip layout
> migrated from "reserved band above the kit artwork" to "draggable
> overlay anchored in native artwork coords" after Jeff confirmed
> Option B.  Debug-only positioner ran the bake, captured `{0, -11}`,
> then the positioner code was stripped before commit so the shipping
> binary carries only the literal offset.

### Done since last checkpoint

- **`e86a887` — QA-A Step 11: BaySickRustyDrums adopts BaySickTitleBar + tab strip overlay.**  Phase 4.5 close.  Three interlocked edits in one commit:
  - `BaySickRustyDrumsPage` now passes `binding.engineName = "BaySickRustyDrums"` + `binding.accentColor = juce::Colour (0xFFCC2222)` (Drums-tab red per D7) into `AriaControlPanel`.  Page background `#141618` selected to fuse seamlessly with the title bar's own bg (per D15).  Phase 4.3's `AriaControlPanel::Binding` plumbing now exercised by all three intended callers (Guitars + Basses from Phase 4.4, RustyDrums from this phase).
  - `AriaControlPanel` tab-strip positioning rewired: `mTabStripNativeOffset` (a `juce::Point<int>` stored in NATIVE artwork coords per D17) controls a Y-only overlay placement above the kit artwork.  `computePanelDrawArea` reverted to centre-anchor for the kit (top-anchor breaks breathing room — see finding 16).  Final baked offset = `{0, -11}` per D19; applied as the literal initializer in `AriaControlPanel.h`.
  - All Debug-only positioner scaffolding stripped before commit: `DraggableTabButton` subclass (anonymous namespace) deleted, "Save Pos" trailing-area button member + helper deleted, `debugSaveTabStripPositionToFile()` impl + forward declaration deleted.  Shipping binary carries only the literal `{0, -11}` offset.
  - **Verification (Jeff confirmed Debug + Release):** kit artwork breathes correctly (no flush-against-title-bar pinning); tab strip sits centred horizontally with the chosen Y nudge; title bar renders red over the dark page bg without seams.

### Findings / decisions added

- **Finding 16 — Top-anchor regression caught mid-phase.**  My initial implementation of `computePanelDrawArea` top-anchored the kit artwork to make room for the tab strip.  Combined with `tabBarH = 0` (since the strip moved to overlay rather than reserved-band layout), this pinned the kit flush against the title bar with no breathing room.  Jeff caught it ("you slid the player all the way up to the top beneath the title bar so there is no where to put the buttons").  Reverted to centre-anchor; the tab strip now overlays the artwork at the chosen Y offset rather than displacing it.  Drove D16 (overlay model, not reserved-band) and the final layout that survived bake.
- **Finding 17 — Debug-only Y-axis lock improvement over Jeff's request.**  Jeff asked to centre-bake the strip horizontally because the Debug Shift+drag was choppy and X-positioning was hard.  Instead of just baking horizontally and shipping the X-free drag, I locked the drag itself to Y-axis-only (X force-zeroed each frame) so the strip stays auto-centred while Y remains user-tunable.  Net result: the bake step still produced a single `{x, y}` value but the `x` component is guaranteed zero by the positioner UX, not just by the user happening to pick zero.  Drove D18.
- **D14 — BaySickRustyDrums accent = `#CC2222`.**  Drums-tab red per D7 (the original 2026-05-09 AskUserQuestion locked Pedals/Guitars/Basses to navy + RustyDrums to Drums-tab red).  Applied via `binding.accentColor = juce::Colour (0xFFCC2222)` in `BaySickRustyDrumsPage`.
- **D15 — BaySickRustyDrums page background = `#141618`.**  Selected to fuse with `BaySickTitleBar`'s own bg so the title bar reads as part of the page rather than sitting on a contrasting seam.  Applied via the page's `paint()` fill.
- **D16 — `AriaControlPanel` tab strip overlays kit artwork (Option B), not a reserved band above.**  After two interpretation iterations with Jeff (interpretation A then B), Jeff confirmed Option B = make tabs a draggable box that saves coordinates, like Rusty hitboxes.  Overlay model means the kit artwork stays centre-anchored and the tab buttons render on top at the user-chosen anchor.
- **D17 — `mTabStripNativeOffset` stored in NATIVE artwork coords.**  Survives window resizes (the offset is in artwork-space, not panel-pixel-space, so the strip stays anchored to the same artwork feature regardless of how the panel scales).  Mirrors the existing Rusty hitbox coordinate convention.
- **D18 — Tab strip positioner drag is Y-axis only.**  X locked to centre; X is force-zeroed each frame during Debug Shift+drag.  Refinement over Jeff's "centre horizontally because Debug drag is choppy" request — locking the drag axis is a stronger guarantee than baking a centre value.  Positioner stripped from shipping binary; rule applies to any future re-bake session in Debug.
- **D19 — Final baked value: `mTabStripNativeOffset = {0, -11}`.**  Captured from Jeff's positioning session and frozen as the literal initializer in `AriaControlPanel.h`.  Re-bake (if ever needed) would re-introduce the Debug-only positioner scaffolding, run the Y-only drag, and re-strip before commit.

### In-flight

- Working tree clean.  22 QA-A commits ahead of origin.

### Files touched this phase

- `Source/Standalone/AriaControlPanel.cpp` (+65 / -18 net)
- `Source/Standalone/AriaControlPanel.h` (+12)
- `Source/Standalone/BaySickRustyDrumsPage.cpp` (+13)

### Next action

Phase 5 — STYLE-01 ribbon truncation fix.  Last source-side phase before
Phase 6 (cross-engine consistency check) and Phase 7 (mandatory close
sequence: `/draft-doc batch-close` -> `/review-batch` -> apply close ->
commit).

---

## Bucket assignment (for batch-close drafter at QA-A close)

- **UI / L&F / Theming** (primary): the BaySickTitleBar component + every engine-editor refactor.
- **Players**: every refactored engine editor (Harmless, BaySickPlayer, BaySickSynth, BaySickBass, BaySickNAM/IR, BaySickVocal, BaySickPedals, BaySickGuitars, BaySickBasses, BaySickRustyDrums).
- **System Pages**: InstPage chrome cleanup (Phase 4.4), BaySickRustyDrumsPage layout (Phase 4.5), RibbonTabBar truncation fix (Phase 5).
- **Meta**: plan-doc edits (`d529602`, `0c4431d`), Running Notes subfolder + §0 update, memory entries created during the batch.
