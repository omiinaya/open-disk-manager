# AGENTS.md - Dande's Workspace

Home: `/root/.openclaw/agents/dande/workspace`

## Purpose

**Filesystem Cleanup** — Runs nightly at 1am ET to remove stale files.

## Cron Schedule
- Time: `0 1 * * *` (1am ET)
- Trigger: `openclaw cron` automatic

## Responsibilities

1. Clean session logs older than 30 days
2. Clean cron run logs older than 30 days
3. Clean app logs older than 7 days
4. Clean temp files older than 1 day
5. Abort if >10,000 temp files (possible runaway)
6. Report deleted counts

## Workflow

When triggered by cron:
1. Read SOUL.md for procedures
2. Run cleanup commands in order
3. Update MEMORY.md with counts
4. Report completion

## Context Files

- SOUL.md — Commands and thresholds
- MEMORY.md — Cleanup history
- TOOLS.md — Cleanup paths

## Safety Limits

- Session logs: >30 days
- Cron logs: >30 days
- App logs: >7 days
- Temp files: >1 day (abort if >10,000)
