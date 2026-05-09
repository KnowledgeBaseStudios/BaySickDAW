---
description: Show the table of agents available in this BaySickDAW session.
---

Display the following table verbatim — no preamble, no editorial commentary.

---

# Agents available in BaySickDAW (14 total)

| Slash command | Agent | Scope | When to use |
|---------------|-------|-------|-------------|
| `/standup` | `standup-summarizer` | Global | Session start / after compaction — Done / In-flight / Next / Blocked summary from git log + plan docs |
| `/draft-commit` | `commit-drafter` | Global | Before commit — drafts message matching project's existing commit style |
| `/diagnose-build` | `build-error-diagnoser` | Global | When a build fails — likely cause + ranked fix candidates |
| `/explain` | `concept-explainer` | Global | Hit an unfamiliar concept — explanation grounded in our codebase |
| `/extract-spec` | `spec-extractor` | Global | After a long planning discussion — convert to structured spec doc |
| `/audit-licenses` | `license-auditor` | Global | Pre-release sweep — vendored libs + addons + asset attribution |
| `/read-doc` | `doc-reader` | BaySickDAW | Pull a section from `Plans & Specs/` without loading the full file |
| `/draft-doc` | `doc-drafter` | BaySickDAW | Compile batch close / Forks / Future State entries — drafter pattern |
| `/review-batch` | `batch-code-reviewer` | BaySickDAW | At batch close, before final commit |
| `/test-signal` | `dsp-test-signal` | BaySickDAW | Generate validation test plan for a DSP module |
| `/preset-gaps` | `preset-coverage-mapper` | BaySickDAW | Pre-QA-Templates batch — preset library gap analysis |
| `/research` | `competitive-research` | BaySickDAW | Pre-milestone — competitive feature sweep for Future State |
| `/architecture` | `daw-architecture-research` | BaySickDAW | Before architecture decision — comparative DAW engineering research |
| `/perf-audit` | `performance-auditor` | BaySickDAW | Every 3 batches OR pre-milestone — codebase perf scan |

## Notes

- **Drafter-only enforcement** — any agent that produces content destined for `Plans & Specs/` returns text in code blocks. Parent session applies via Edit.
- **Orchestration rules** — when to dispatch automatically — live in `Plans & Specs/Main Plan.md` §0 "Agent Orchestration Rules."
- **Cross-project agents** (the 6 Global ones above) live at `~/.claude/agents/` and work in any session.
- **BaySickDAW-specific agents** live at `.claude/agents/`.
