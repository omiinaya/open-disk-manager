---
name: silent-worker
description: Base template for cron-triggered agents (hiron, roll, dande, harpy). Standardized execution, reporting, and error handling.
---

# Silent Worker Skill

Use this skill as a template for agents that run on cron schedule. Provides consistent structure for hiron, roll, dande, harpy.

## Template Structure

Every silent worker should follow this pattern:

```bash
#!/bin/bash
# Agent: <AGENT_NAME>
# Purpose: <what it does>

set -e

# Configuration
AGENT_NAME="<agent-name>"
LOG_FILE="/tmp/${AGENT_NAME}.log"
LOCKFILE="/tmp/.openclaw-${AGENT_NAME}"
TIMEOUT=300  # 5 minutes max

# Initialize
echo "[$(date -u +%Y-%m-%dT%H:%M:%SZ)] Starting $AGENT_NAME" >> "$LOG_FILE"

# Acquire lock (prevent conflicts)
acquire_lock() {
    for i in {1..30}; do
        if mkdir "$LOCKFILE" 2>/dev/null; then
            echo "$$" > "$LOCKFILE/pid"
            echo "[$(date)] Lock acquired" >> "$LOG_FILE"
            return 0
        fi
        sleep 1
    done
    echo "[$(date)] Could not acquire lock" >> "$LOG_FILE"
    return 1
}

# Release lock
release_lock() {
    if [ -d "$LOCKFILE" ]; then
        rm -rf "$LOCKFILE"
        echo "[$(date)] Lock released" >> "$LOG_FILE"
    fi
}

# Error handler
error_exit() {
    echo "[$(date)] ERROR: $1" >> "$LOG_FILE"
    release_lock
    # Optionally notify main agent
    exit 1
}

# Success handler  
success_exit() {
    echo "[$(date)] Complete: $1" >> "$LOG_FILE"
    release_lock
    exit 0
}

# Trap for cleanup
trap 'release_lock' EXIT

# Main execution
main() {
    acquire_lock || { echo "Could not acquire lock, exiting"; exit 0; }
    
    # === AGENT-SPECIFIC COMMANDS HERE ===
    # Replace with actual task
    
    # Report in standardized format
    echo "[Agent: ${AGENT_NAME^}] complete - <summary>"
    
    success_exit "<stats>"
}

main "$@"
```

## Standardized Output Format

### Start
```
[Agent: <Name>] starting - <brief description>
```

### Complete
```
[Agent: <Name>] complete - <metric1>, <metric2>
```

### Error
```
[Agent: <Name>] error - <error message>
```

## Agent-Specific Implementations

### hiron (qmd index)
```bash
qmd collection add /root/.openclaw/ --name openclaw --mask "**/*.md" || error_exit "collection add failed"
qmd update || error_exit "update failed"
qmd embed || error_exit "embed failed"
success_exit "Collection: ok, Update: ok, Embed: ok"
```

### roll (backup)
```bash
cd /root/.openclaw
git add -A || error_exit "git add failed"
git commit -m "Nightly backup $(date -u +%Y-%m-%d)" || { echo "No changes"; success_exit "No changes"; }
git tag "backup-$(date -u +%Y-%m-%d)"
git push --follow-tags || error_exit "git push failed"
success_exit "$(git rev-list --count HEAD) files committed"
```

### dande (cleanup)
```bash
SESSION_LOGS=$(find /root/.openclaw/agents/*/sessions/ -type f -mtime +30 -delete 2>/dev/null | wc -l)
CRON_LOGS=$(find /root/.openclaw/cron/runs/ -type f -mtime +30 -delete 2>/dev/null | wc -l)
APP_LOGS=$(find /root/.openclaw/logs/ -type f -mtime +7 -delete 2>/dev/null | wc -l)
TEMP=$(find /tmp/ -type f -mtime +1 2>/dev/null | wc -l)
[ "$TEMP" -gt 10000 ] && error_exit "Temp file count too high: $TEMP"
find /tmp/ -type f -mtime +1 -delete 2>/dev/null || true
success_exit "Deleted $SESSION_LOGS session logs, $CRON_LOGS cron logs, $APP_LOGS app logs, $TEMP temp files"
```

## Notes

- Always use lock file to prevent conflicts
- Set timeout to prevent runaway processes
- Log all actions with timestamps
- Use trap for cleanup on errors
- Notify main agent only on critical errors
