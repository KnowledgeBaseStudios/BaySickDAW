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
| `<TBD-M>` | meta-doc | establish Running Notes/ subfolder + §0 update + memory entry update |
| `<TBD-B>` | source | guard HarmlessLAF::drawLinearSlider against zero-sized bounds |
| `<TBD-C>` | Phase 3.4 | BaySickNAMIR editor adopts BaySickTitleBar |

### Phases done / in flight / remaining

- Phase 1 (component scaffold + bloom upgrade) — DONE.
- Phase 2 (BaySickPlayer PoC) — DONE.
- Phase 3 (sweep existing title bars: Harmless / BaySickSynth / BaySickBass / BaySickNAM/IR) — DONE except 3.4 NAMIR commit pending.
- Phase 4 (BaySickVocal STYLE-03 / BaySickPedalsEditor + preset btn migration / AriaControlPanel extension / InstPage cleanup / BaySickRustyDrumsPage) — ALL pending.
- Phase 5 (STYLE-01 ribbon truncation) — pending.
- Phase 6 (cross-engine consistency check) — pending.
- Phase 7 (close sequence: `/draft-doc batch-close` -> `/review-batch` -> apply close -> commit) — pending.

### Next action

Commit the Running Notes infrastructure (this file + §0 update + memory entry update) as the meta-doc commit, then the HarmlessLAF defensive guard, then the Phase 3.4 NAMIR refactor.  After all three commits land, proceed to Phase 4.1 (BaySickVocalEditor STYLE-03).

Going forward: every mid-batch checkpoint dispatches `/draft-doc running-notes` and the parent applies the returned text by appending a new `## YYYY-MM-DD HH:MM PT — <summary>` block to this file.

---

## Bucket assignment (for batch-close drafter at QA-A close)

- **UI / L&F / Theming** (primary): the BaySickTitleBar component + every engine-editor refactor.
- **Players**: every refactored engine editor (Harmless, BaySickPlayer, BaySickSynth, BaySickBass, BaySickNAM/IR, BaySickVocal, BaySickPedals, BaySickGuitars, BaySickBasses, BaySickRustyDrums).
- **System Pages**: InstPage chrome cleanup (Phase 4.4), BaySickRustyDrumsPage layout (Phase 4.5), RibbonTabBar truncation fix (Phase 5).
- **Meta**: plan-doc edits (`d529602`, `0c4431d`), Running Notes subfolder + §0 update, memory entries created during the batch.
