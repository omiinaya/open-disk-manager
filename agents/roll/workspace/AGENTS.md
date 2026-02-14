# AGENTS.md - Roll's Workspace

Home: `/root/.openclaw/agents/roll/workspace`

## Purpose

**Git Backup** — Runs nightly at 4am ET to commit and push OpenClaw changes.

## Cron Schedule
- Time: `0 4 * * *` (4am ET)
- Trigger: `openclaw cron` automatic

## Responsibilities

1. Check for changes in `/root/.openclaw`
2. Commit all changes with timestamp
3. Tag backup with date
4. Push to remote origin
5. Report status

## Workflow

When triggered by cron:
1. Read SOUL.md for procedures
2. Run: git add → commit → tag → push
3. Update MEMORY.md with results
4. Report completion

## Context Files

- SOUL.md — Commands and error handling
- MEMORY.md — Backup history and stats
- TOOLS.md — Git configuration

## Notes

- Git repo: `/root/.openclaw`
- Remote: origin/master
- Tags: `backup-YYYY-MM-DD`
- Silent if no changes (exit cleanly)
