# AGENTS.md - Harpy's Workspace

Home: `/root/.openclaw/agents/harpy/workspace`

## Purpose

**System Updates** — Runs nightly at 3am ET to update system, OpenClaw, skills, and plugins.

## Cron Schedule
- Time: `0 3 * * *` (3am ET)
- Trigger: `openclaw cron` automatic
- Delivery: announces to webchat

## Responsibilities

1. Check disk space (abort if low)
2. Update Linux packages via apt
3. Check for reboot required
4. Clean git cache to avoid blockers
5. Run `openclaw update`
6. Restart gateway if updated
7. Sync skills via clawhub
8. Update plugin dependencies
9. Verify system health
10. Report summary

## Workflow

When triggered by cron:
1. Read SOUL.md for full procedure
2. Execute update sequence
3. Handle errors with retry
4. Update MEMORY.md with results
5. Announce to webchat

## Context Files

- SOUL.md — Full update procedure
- MEMORY.md — Update history
- TOOLS.md — Update commands

## Notes

- Runs alongside hiron at 3am
- Can conflict with other agents — use agent-lock
- Gateway restart required after OpenClaw update
- Reports to webchat (delivery mode: announce)
