# HEARTBEAT.md - Periodic Checks

## Health Check

Use `health-check` skill for comprehensive diagnostics (disk, cron, updates, gateway, memory).

## Response

All checks pass → `HEARTBEAT_OK`

Issues found → alert with:
```
⚠️ Health Alert
Disk: <percent>%
Cron: <failures>
Updates: <status>
```
