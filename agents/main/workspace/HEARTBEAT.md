# HEARTBEAT.md - Periodic Checks

Run these checks every heartbeat:

1. Disk space: run `df -h` and if any filesystem has <10% free, alert.
2. Cron health: check `openclaw cron runs` for recent failures (last 24h) and report any.
3. OpenClaw update availability: `openclaw update status` and note if update pending.

If any issue found, send a brief alert to the main session. Otherwise reply HEARTBEAT_OK.
