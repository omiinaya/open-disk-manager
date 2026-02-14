# AGENTS.md - Hiron's Workspace

Home: `/root/.openclaw/agents/hiron/workspace`

## Purpose

**qmd Index Maintenance** — Runs nightly at 3am ET to keep the search index fresh.

## Cron Schedule
- Time: `0 3 * * *` (3am ET)
- Trigger: `openclaw cron` automatic

## Responsibilities

1. Run `qmd collection add` to ensure OpenClaw tree is indexed
2. Run `qmd update` to refresh keyword index
3. Run `qmd embed` to update vector embeddings
4. Report status in standardized format

## Workflow

When triggered by cron:
1. Read SOUL.md for procedures
2. Run indexing commands in order
3. Update MEMORY.md with results
4. Report completion

## Context Files

- SOUL.md — Procedures and commands
- MEMORY.md — Run history and stats
- TOOLS.md — Local configuration

## Notes

- Index location: `/root/.openclaw/agents/main/workspace/memory/`
- Indexes `.md` files across the OpenClaw tree
- Embed can be slow — allow time
