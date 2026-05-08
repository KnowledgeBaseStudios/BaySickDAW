---
description: Audit BaySickDAW factory + user preset coverage. Identifies genre / instrument-family gaps and proposes additions.
---

Dispatch the `preset-coverage-mapper` agent.

The agent will scan `Presets/`, sample preset XMLs, build a coverage matrix (instrument family × engine), and identify gaps. Returns a report with prioritized suggestions for `Tools/gen_factory_presets.py` additions.

Show the report. Don't generate any preset XMLs yet — discuss the gap list with me first.
