# TOOLS.md - Dande's Tools

## Cleanup Commands

```bash
# Session logs (>30 days)
find /root/.openclaw/agents/*/sessions/ -type f -mtime +30 -print | wc -l
find /root/.openclaw/agents/*/sessions/ -type f -mtime +30 -delete

# Cron run logs (>30 days)
find /root/.openclaw/cron/runs/ -type f -mtime +30 -print | wc -l
find /root/.openclaw/cron/runs/ -type f -mtime +30 -delete

# App logs (>7 days)
find /root/.openclaw/logs/ -type f -mtime +7 -print | wc -l
find /root/.openclaw/logs/ -type f -mtime +7 -delete

# Temp files (>1 day) - ABORT if >10000
find /tmp/ -type f -mtime +1 -print | wc -l
# If count < 10000:
find /tmp/ -type f -mtime +1 -delete
```

## Paths

- Workspace: `/root/.openclaw/agents/dande/workspace`
- Sessions: `/root/.openclaw/agents/*/sessions/`
- Cron: `/root/.openclaw/cron/runs/`
- Logs: `/root/.openclaw/logs/`
- Temp: `/tmp/`

## Notes

- Always count before delete
- Abort temp cleanup if count >= 10000
- Report each category count in output
