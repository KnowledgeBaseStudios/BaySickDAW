---
description: One-shot competitive research sweep — what features do other DAWs / instruments / plugins ship that we don't? Outputs draft Future State entries.
argument-hint: [optional: focus area like "Players" or "Effects / saturation"]
---

Dispatch the `competitive-research` agent.

Pass `$ARGUMENTS` as the focus area. If empty, the agent does a broad sweep — but a focused sweep is much higher quality, so encourage me to provide a focus.

The agent reads `Plans & Specs/Previously Implemented.md` + `Future State.md` first to avoid proposing duplicates, then WebSearches + WebFetches public competitor info. Returns proposed Future State entries in canonical format with confidence ratings (HIGH / MEDIUM / LOW) and source URLs.

Show the proposals. I'll review and we'll decide which to add.

Note: this agent should run sparingly — public info skews to marketing puff, so it's mostly useful at milestone transitions (V1 release, V2 planning). Don't run it weekly.
