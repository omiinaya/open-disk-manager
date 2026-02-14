# TOOLS.md - Local Notes

## OpenCode Session Script

```bash
# Location: /root/.openclaw/agents/zero/workspace/opencode-session.sh
# Usage: ./opencode-session.sh <repo> [--port N] [--title "X"]
```

## Port Configuration

- Default OpenCode server port: **4096**
- Server host: `0.0.0.0` (LAN accessible)

## Discord Channel

- Channel: agent-ciel (Discord)
- Use `message action=send channel=discord target=agent-ciel`

## Key Commands

- Check server: `pgrep -f "opencode serve"`
- Check port: `ss -tln | grep 4096`
- List sessions: `cat sessions.json`
- List repos: `cat repos.json`

## Git Workflow

1. Check for testing branch: `git branch -a | grep -i test`
2. Checkout testing: `git checkout testing`
3. Or create from main: `git checkout -b testing`
4. Commit: `git commit -m "Zero: <description>"`
5. Push: `git push origin testing`
