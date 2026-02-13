# Memory

OpenClaw memory is plain Markdown in the agent workspace. The files are the source of truth; the model only “remembers” what gets written to disk.

Memory search tools provided by active memory plugin (default: `memory-core`). Disable with `plugins.slots.memory = "none"`.

## Memory files

- `memory/YYYY-MM-DD.md` — daily log (append-only). Read today + yesterday at session start.
- `MEMORY.md` (optional) — curated long-term memory. Only load in main private session.

## When to write memory

- Decisions, preferences, durable facts → `MEMORY.md`
- Day-to-day notes and running context → `memory/YYYY-MM-DD.md`
- “Remember this” → write it down; do not keep in RAM.

## Automatic memory flush

When a session is close to auto-compaction, OpenClaw triggers a silent agentic turn to store durable memory before compaction. Controlled by `agents.defaults.compaction.memoryFlush`.

## Vector memory search (built-in)

OpenClaw builds a small vector index over `MEMORY.md` and `memory/*.md` for semantic queries.

- Enabled by default.
- Watches files for changes (debounced).
- Configure under `agents.defaults.memorySearch`.
- Uses remote embeddings by default (OpenAI, Gemini, Voyage) or local (`memorySearch.provider = "local"`).
- Optional hybrid search (BM25 + vector).
- Embedding cache in SQLite available.

## QMD backend (experimental)

Set `memory.backend = "qmd"` to use QMD (local-first BM25+vectors+reranking). Requires `qmd` CLI on PATH and Bun + SQLite with extensions.

- `memory.qmd.update.interval` default `5m`.
- `memory.qmd.limits.maxResults` default 6.
- `memory.qmd.citations` can be `auto`/`on`/`off`.
- QMD runs with its own XDG dirs under `~/.openclaw/agents/<agentId>/qmd/`.

## Additional memory paths

Add extra directories to index:
```
agents.defaults.memorySearch.extraPaths = ["../team-docs", "/srv/shared-notes/overview.md"]
```

## Tools

- `memory_search` — returns snippets with file + line ranges.
- `memory_get` — read memory file content by path (workspace-relative).

---
Source: https://docs.openclaw.ai/concepts/memory
