# HEARTBEAT.md - Periodic Checks

Run these checks every heartbeat (every 4h):

## Quick Check

- Check server status (is OpenCode running?)
- Review `sessions.json` for stale sessions
- Update `lastActivity` in `repos.json` for active repos

## Health Check

If issues found, report to user.
