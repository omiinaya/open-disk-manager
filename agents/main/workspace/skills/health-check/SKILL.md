---
name: health-check
description: Periodic health check for heartbeat. Verifies disk space, cron health, and OpenClaw updates.
---

# Health Check Skill

Use this skill for periodic heartbeat checks. Verifies system health and alerts on issues.

## When to Use

- Heartbeat poll (every 4h)
- User requests "health check" or "diagnostic"
- After system changes

## Procedure

### 1. Disk Space Check

```bash
df -h
# Alert if any filesystem <10% free
```

**Alert threshold:** <10% free

### 2. Cron Health Check

```bash
openclaw cron runs --last 24h
# Check for failures
```

**Alert if:** Any failed cron jobs

### 3. OpenClaw Update Status

```bash
openclaw update status
# Check if update available
```

**Alert if:** Update pending (user may want to approve)

### 4. Gateway Health

```bash
curl -s http://localhost:18789/health
# Or check process
pgrep -f openclaw-gateway
```

**Alert if:** Gateway not responding

### 5. Memory Check

```bash
free -h
# Check available memory
```

**Alert if:** <500MB available

### 6. Agent Status

```bash
openclaw agent list
# Check all agents
```

## Output Format

### Healthy
```
HEARTBEAT_OK
```
(only this, nothing else)

### Issues Found
```
⚠️ Health Alert

Disk: <filesystem> at <percent>% (threshold: 10%)
Cron: <job> failed — <reason>
Gateway: <status>
Memory: <percent>% available
Updates: <available|up to date>
```

## Integration with HEARTBEAT.md

Read HEARTBEAT.md in workspace for any custom checks:

```markdown
# HEARTBEAT.md - Periodic Checks

Run these checks every heartbeat:

1. Disk space: run `df -h` and if any filesystem has <10% free, alert.
2. Cron health: check `openclaw cron runs` for recent failures (last 24h) and report any.
3. OpenClaw update availability: `openclaw update status` and note if update pending.

If any issue found, send a brief alert to the main session. Otherwise reply HEARTBEAT_OK.
```

## Cron Job Example

```json
{
  "name": "heartbeat-health",
  "schedule": { "kind": "every", "everyMs": 14400000 },
  "payload": {
    "kind": "agentTurn",
    "message": "Run health-check skill"
  },
  "sessionTarget": "main"
}
```

## Notes

- Return `HEARTBEAT_OK` if all checks pass
- Be brief — heartbeat should be fast
- Link to logs for debugging
- Use message tool to alert main session on issues
