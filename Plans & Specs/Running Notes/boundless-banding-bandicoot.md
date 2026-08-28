# Running Notes — QA-EqFlagship (boundless-banding-bandicoot)

> Append-only mid-batch artifact. A new `## YYYY-MM-DD — Task N — <name>`
> entry lands at every checkpoint (commit landed / sub-task verified /
> finding captured / spec call resolved / scope pivot). At batch close,
> `/draft-doc batch-close` reads this file as the primary input when
> compiling the single Implemented Work Log entry.

Pair file: `Plans & Specs/Batch Plans/boundless-banding-bandicoot.md`.
Convention: Main Plan §0, Batch Plans + Running Notes layout (locked
2026-05-11).

## 2026-08-27 — Task 0 — Batch open

Plan cut from the workshopped full-market competitive review
(`Research Reports/eq-flagship-competitive-review-2026-08-27.md` + the
feature matrix `eq-feature-matrix-2026-08-27.html`). All 26 workshop
rulings locked as W-1..W-26 in the plan's spec table; Jeff approved the
plan 2026-08-27. Handoff ruling recorded: the KBS handoff is written at
batch CLOSE against finished work. The QA-EqPro follow-up work this batch
builds on committed as `7654dba8` immediately before open.

## 2026-08-27 - Task 1 - 96 bands + paged chip row

kMaxBands 24 -> 96 through ONE new free constant (kbs::kEqMaxBands) so the
home-frequency law can read it; every consumer (StripEq, kEqBands,
kEqCacheSize, graph/rail/match loops) already chained off constants - zero
hardcoded 24s found in the sweep. Home-frequency law redistributed: bands
9..96 share the same 56 Hz-13.8 kHz log span the 24-band law covered,
parametric on the pool size so the KBS handoff inherits it. Chip row is now
paged: 24 chips per page, up/down arrows at the row's left (grey at the
ends, hidden entirely while only page 1 exists), a page appears only when
the previous fills or already holds an on-band, "+" fills the visible page
first and the row follows the band it makes, selection made on the graph
snaps the row to that band's page (only on selection CHANGE, so manual
browsing is never fought). Pages numbered, never lettered - A/B stays the
setup pill.

Two findings: (1) the harness quoting layer ate the backslash in a printf
passed through a bash heredoc - repaired by direct edit (known hazard,
recorded); (2) FIRST REAL 96-BAND COST: BaySickEqTests crashed at startup
with a stack overflow - main() holds ~22 engine locals across sibling
scopes and a 96-band ParametricEq is ~100KB, so MSVC's cumulative frame
reservation blew the default 1MB stack at the prologue probe. The PRODUCTS
heap-allocate every engine (graph nodes / plugin processor), so this is
test-harness-only; fixed with /STACK:8MB on the test target, reasoning in
CMakeLists. New EqTests section 14 (5 checks): pool size, home-frequency
monotonicity + audible-span bounds, band 96 boosts like any band, 95 idle
bands contribute exactly nothing. Suite green; build gate six zeros + four
link lines.
