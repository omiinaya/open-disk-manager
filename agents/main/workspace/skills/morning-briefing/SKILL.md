---
name: morning-briefing
description: Generate a morning status report. Runs around 7am to summarize system health, recent activity, and tasks.
---

# Morning Briefing Skill

Use this skill to generate a morning status report for the user. Provides a quick snapshot of system health and recent activity.

## When to Use

- Scheduled cron job at 7am
- User requests "morning briefing" or "status"
- After system restart

## Procedure

1. **Check gateway status**
   ```bash
   openclaw status --format json
   ```

2. **Check cron health**
   ```bash
   openclaw cron runs --last 24h
   ```

3. **Check disk space**
   ```bash
   df -h
   ```

4. **Check recent git activity**
   ```bash
   cd /root/.openclaw && git log --oneline -5
   ```

5. **Check active sessions**
   - Read archy's sessions.json if exists
   
6. **Check recent errors**
   - Check logs for errors in last 24h

## Report Template

```
☀️ Morning Briefing — <date>

## System Status
- Gateway: <online/offline>
- Uptime: <duration>
- Disk: <percent used>

## Cron Jobs (24h)
- Success: <count>
- Failed: <count>
<list any failures>

## Git Activity
<recent commits>

## Active Sessions
<list from archy/sessions.json>

## Action Items
<any tasks needing attention>

## Notes
<anything else>
```

## Example Output

```
☀️ Morning Briefing — 2026-02-14

## System Status
- Gateway: ✅ online
- Uptime: 3 days
- Disk: 45% used

## Cron Jobs (24h)
- Success: 12
- Failed: 1 (harpy update)
  - clawhub sync timeout

## Git Activity
- fe05ca3 Rename skill config-save to save-config
- ec3f183 Remove supermemory, enable memory-core

## Active Sessions
- ses_abc123 (project-x) - idle since 2h ago

## Action Items
- ⚠️ harpy sync failed — check clawhub auth
- ⏳ Roll backup pending (no changes)

## Notes
- Weekend approaching — confirm scheduled jobs
```

## Cron Setup

```bash
# Add to crontab
0 7 * * * openclaw cron trigger --agent main --message "morning-briefing"
```

## Notes

- Keep it concise — 1 minute to read
- Highlight actionable items
- Link to logs for details
- Save to memory/YYYY-MM-DD.md after
