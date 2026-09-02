# QA-ManualPress (shutter-snapping-shrike)

**Opened:** 2026-08-28. **Status:** DRAFTED, awaiting Jeff's approval.
**What:** the manual redo as ONE batch (Jeff's ruling 6: "all one thing so
we don't lose track") - the bsd_shot headless screenshot harness, the
generated parameter reference, the voice rewrite, the In The Weeds audit,
and self-anchoring callout dots. Sources: the 2026-08-28 KBS manual review
(seven-reader pass over the KBS Plugins manual set, its pipeline, and our
own set) + Jeff's six rulings the same day.

## Rulings (locked 2026-08-28)

| id | Ruling | Notes |
|---|---|---|
| M-1 (1a) | bsd_shot ships as a `--shot` mode INSIDE the app exe - branches in StandaloneApp::initialise before the splash/device code, runs the suite, quits | Zero extra compile cost, do_build untouched, dormant at runtime. 92 of 94 figures automatable; `Main frame.png` + `Hosted Plugin.png` stay hand-captured (native peers / foreign HWND) |
| M-2 (2A) | Menu figures via a headless PopupMenu COMPOSER | Pixel-faithful, fully automatic; needs a small menuForShot hook at ~25 builder sites; validated against hand shots before trusted |
| M-3 (3A) | Generated per-figure control tables from a `--docs` dump of the APVTS registrations + newly authored one-line blurbs per control | The KBS model: the reference is read from the code every run and cannot rot; the blurbs are the writing job |
| M-4 (4A) | Callout dots KEPT and AUTO-ANCHORED: the shot tool emits marker coordinates from live component bounds via componentIDs | Jeff: "we may change to C (drop dots) after depending on how I like it" - the composer/table work is identical either way, so the switch stays cheap |
| M-5 (5C) | In The Weeds: full audit of every code dump - verify every excerpt CURRENT against live source, and extend coverage so EVERY topic carries real code, not just some | Jeff: "most of them look like barely anything at all" |
| M-6 | All of it is ONE batch with one ledger | This file |

## The register (applies to every rewritten chapter and every new blurb)

Calibrated 2026-08-28 to what KBS actually SHIPS (screenshots + short
functional blurbs + control tables with real numbers) - NOT to the hidden
manual_text.py bodies, which are the same slop register Jeff ordered
removed over there and which this plan must not resurrect:

1. Facts only. A control gets: what it does (the mechanism), its range
   with real units, its default, and what changes as you move it. No
   personality anywhere - if a sentence exists to sound good, it goes.
2. Banned outright: taglines, epigram closers, aphorisms, rhetorical
   questions, scene-setting, sensory metaphor as spec ("silky",
   "shimmer"), the hyphen-aside cadence, "the one worth knowing about"
   editorializing.
3. Table blurbs are short and functional, like the shipped KBS tables:
   verb-first, 3-9 words, the audible consequence ("Cuts bleed between
   hits"), never "adjusts the X parameter" and never a flourish.
4. Every number comes from the parameter registrations via the --docs
   dump, never from prose memory.
5. Mechanism over sensation: "the threshold follows the track's level"
   is documentation; "warm" is not. Real terms used once, defined in one
   flat clause.
6. KEEP the two things our set does that the register must not lose:
   every mouse/keyboard gesture documented, and every tradeoff stated
   with its number (latency, pre-ring, ranges of validity).
7. Short declarative sentences. US spelling, ASCII, no em-dashes, casing
   per Jeff (unchanged rules).

## Tasks

### Task 0 - open
- [ ] Commit plan (rides the 2026-08-28 fix commit if approved together).

### Task 1 - the harness core + first figures
- [ ] `--shot <outdir> [figure...]` branch in StandaloneApp::initialise: processor + StandalonePlayHead + prepareToPlay, NO device manager init, no window; snapshot/save plumbing at 2x; font self-check; figure names exactly matching `Manuals/figures/*.png`.
- [ ] Bucket A figures (~18, self-contained): BaySickSynth panels x6, BaySickSolstice, transport bar, ribbon, window chrome, dialogs, pedal views.
- [ ] Build gate. Commit. Running notes.

### Task 2 - state-rich figures
- [ ] Programmatic figure state at model level (rig + APVTS + tool-owned PatternManager; never a personal project): EQ (bands + pink noise through StripEq), analyzer, VU/mixer meter pokes, rack/panel/visual, players with preloads, sfizz kit/SFZ loads with bounded readiness waits, Builder/Piano Roll/Event Editor/Undo History scripted content (~31 figures).
- [ ] The open EQ RE-SHOOT debt (both EQ batches + `EQ Instances.png`) is absorbed here.
- [ ] Gate. Commit. Running notes.

### Task 3 - the menu engine
- [ ] Headless PopupMenu composer (LAF-driven paint into an Image); `menuForShot` hooks at the ~25 builder sites; the 6 master menus via getMenuForIndex; validation against 2-3 hand shots.
- [ ] All ~43 menu figures. Fall back to show-and-scrape for any menu the composer cannot reproduce (documented per figure).
- [ ] Gate. Commit. Running notes.

### Task 4 - the docs dump + control tables
- [ ] `--docs <out.json>` mode: per figure, every control's name, range, units, default, skew, choices from the APVTS registrations + componentID mapping.
- [ ] generate-manual.py renders a "Controls" table per figure at the In Depth level (name / blurb / default / range with real units; enum choices by NAME, never indices - the KBS bug class, avoided).
- [ ] Gate. Commit. Running notes.

### Tasks 5-7 - blurb authoring (Shell / Instrument / Mixing & Effects)
- [ ] One effect-first blurb per control, house lexicon, ranges + defaults verified against the registrations (not prose memory). Blurbs live in the docs pipeline so the tables carry them.
- [ ] Commit per group. Running notes.

### Tasks 8-10 - the In Depth rewrite (same three groups)
- [ ] All 91 chapters flattened to the register above: slop register out wholesale, tables carry the numbers, prose survives only where it states mechanism, gesture, or tradeoff.
- [ ] Commit per group. Running notes.

### Task 11 - In The Weeds audit (M-5)
- [ ] Every code excerpt diffed against live source and refreshed; every topic gains real, current code where it has little or none; formulas kept; stale function dumps replaced or trimmed to the load-bearing excerpt.
- [ ] Gate (docs only unless hooks changed). Commit. Running notes.

### Task 12 - self-anchoring dots (M-4)
- [ ] componentID -> callout map; `--shot` emits marker coordinates from live component bounds for every automated figure; marker-coords.py becomes generated-plus-exceptions (the 2 manual figures keep hand coords).
- [ ] Gate. Commit. Running notes.

### Task 13 - close
- [ ] Full regeneration: shots, docs, tables, manual, three PDFs; a side-by-side diff sheet (old figure vs tool figure) for Jeff's approval BEFORE the new images ship.
- [ ] System Reference: new Manual Pipeline doc; Callout Registry/Screenshot List reconciled to the generated world; /review-batch; Work Log entry; final commit (surface + approval).

## Verification
Jeff: approve the diff sheet (Task 13); read one rewritten chapter per
group against the rulebook; spot-check three control tables against the
running app; confirm the dot anchoring on two re-generated figures.
`Main frame.png` + `Hosted Plugin.png` remain his two hand captures.

## Routing notes (Rule 3)
Findings in scope: fix in-batch. The KBS-side manual-generator bugs are
NOT ours - they ride "Files For Claude/KBS Note - Sat Knob, Resonant Pass
Filters, Manual Generator Bugs.md" to the KBS session. Ruling M-4's
possible later switch to plain shots (option C) would retire Task 12's
output only - the tables and shots survive either way.
