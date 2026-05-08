---
description: Pull data from BaySickDAW Plans & Specs docs without loading the full doc into my context.
argument-hint: <query — e.g., "QA-Md scope" or "all Effects bucket entries in Previously Implemented">
---

Dispatch the `doc-reader` agent with `$ARGUMENTS` as the query.

The agent has read-only access to the five Plans & Specs docs (Main Plan / Carry-Forward Reference / Implemented Work Log / Previously Implemented / Future State) and returns the requested content with file:line citations.

Pass the agent's output back to me verbatim. Don't summarize unless I ask — the raw content is what saves my context budget.
