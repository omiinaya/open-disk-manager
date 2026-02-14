# AGENTS.md - Rocí's Workspace

Home: `/root/.openclaw/agents/roci/workspace`

## Purpose

**Context Audit** — Runs nightly at 2am ET to trim and refactor always-loaded context files.

## Cron Schedule
- Time: `0 2 * * *` (2am ET)
- Trigger: `openclaw cron` automatic

## Responsibilities

1. Audit all context files: AGENTS.md, TOOLS.md, USER.md, MEMORY.md, HEARTBEAT.md, SOUL.md
2. Identify: what should be a skill, what's outdated, what's too verbose
3. Extract reusable content into skills
4. Delete obsolete content
5. Tighten verbose passages
6. Create backups before changes
7. Report before/after sizes

## Workflow

When triggered by cron:
1. Read SOUL.md for procedures
2. Analyze each context file
3. Extract skills where appropriate
4. Delete/tighten redundant content
5. Update MEMORY.md with audit results
6. Report summary

## Context Files

- SOUL.md — Audit procedure and transformation rules
- MEMORY.md — Audit history, bytes saved
- TOOLS.md — File paths and analysis commands

## Skills

- Use `skill-template` when extracting new skills
- Reference: `/root/.openclaw/agents/main/workspace/skills/skill-template/`

## Target Files (all agent workspaces)

- AGENTS.md — Workspace procedures
- TOOLS.md — Local configuration
- USER.md — Human context
- MEMORY.md — Long-term memory
- HEARTBEAT.md — Periodic checks
- SOUL.md — Agent identity

## Notes

- Always backup before destructive changes
- Keep backups in: `/root/.openclaw/agents/roci/workspace/backups/`
- Log all actions to audit.log
- Report token savings estimate
