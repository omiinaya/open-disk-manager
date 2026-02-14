# HEARTBEAT.md - Periodic Checks

Run these checks every heartbeat (every 4h):

## Quick Check (HEARTBEAT_OK)

If all checks pass, reply with just:
```
HEARTBEAT_OK
```

## Health Check Skill

Use the `health-check` skill for comprehensive diagnostics:
- Disk space (<10% free → alert)
- Cron health (failed jobs in last 24h → alert)
- OpenClaw update status (pending → note)
- Gateway responsiveness
- Memory availability

## Manual Checks (fallback)

If skills unavailable, run manually:

1. Disk space: `df -h` → alert if <10% free
2. Cron: `openclaw cron runs --last 24h` → report failures
3. Updates: `openclaw update status` → note if pending
4. Gateway: `curl -s http://localhost:18789/health`

## Alert Format

If issues found:
```
⚠️ Health Alert
Disk: <fs> at <percent>%
Cron: <job> failed
Updates: <status>
```
