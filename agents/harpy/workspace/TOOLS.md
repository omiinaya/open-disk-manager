# TOOLS.md - Harpy's Tools

## Commands

```bash
# Disk space check
df -h

# Linux packages
apt-get update
apt-get upgrade -y

# Check reboot
[ -f /var/run/reboot-required ] && echo "REBOOT_REQUIRED"

# Clean git cache
cd /root/openclaw && git clean -fdX .clawhub agents || true

# OpenClaw update
openclaw update

# Gateway restart
openclaw gateway restart

# Skills sync
clawhub whoami
clawhub sync

# Plugin updates (detect package manager)
pnpm update -r  # if pnpm-workspace.yaml exists
# or: npm update -g / yarn upgrade

# Health check
openclaw status
```

## Paths

- Workspace: `/root/.openclaw/agents/harpy/workspace`
- OpenClaw: `/root/openclaw`
- Gateway: port 18789

## Notes

- Retry failed commands once
- Check disk before updates — abort if <10% free
- Restart gateway after openclaw update
- Use clawhub whoami to verify auth before sync
