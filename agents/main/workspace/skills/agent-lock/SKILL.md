---
name: agent-lock
description: File-based locking for coordinated operations between agents. Prevents roll/dande/harpy from conflicting.
---

# Agent Lock Skill

Use this skill when you need to coordinate between agents that might run simultaneously (e.g., roll backup and dande cleanup).

## When to Use

- Roll is about to run backup
- Dande is about to run cleanup  
- Harpy is about to run updates
- Any two agents might conflict

## Procedure

### Acquire Lock

```bash
LOCKFILE="/tmp/.openclaw-agent-lock"
LOCKNAME="$1"  # e.g., "roll", "dande", "harpy"

# Wait for lock (max 60s)
for i in {1..60}; do
    if mkdir "$LOCKFILE" 2>/dev/null; then
        echo "$$" > "$LOCKFILE/pid"
        echo "$LOCKNAME" > "$LOCKFILE/holder"
        date -u +"%Y-%m-%dT%H:%MZ" > "$LOCKFILE/acquired"
        break
    fi
    sleep 1
done

# Check if we got it
if [ "$(cat "$LOCKFILE/holder" 2>/dev/null)" != "$LOCKNAME" ]; then
    echo "LOCKED by $(cat "$LOCKFILE/holder") since $(cat "$LOCKFILE/acquired")"
    exit 1
fi
```

### Release Lock

```bash
rm -rf "$LOCKFILE"
```

### Check Lock Status

```bash
if [ -d "$LOCKFILE" ]; then
    echo "Locked by $(cat $LOCKFILE/holder) since $(cat $LOCKFILE/acquired)"
    exit 1
else
    echo "Available"
fi
```

## Usage in Agents

### In Roll (backup)
```bash
# Before backup
skill agent-lock acquire roll || { echo "Waiting for other agent..."; exit 0; }

# Do backup work...

# After backup (always release)
skill agent-lock release roll
```

### In Dande (cleanup)
```bash
# Before cleanup, check if lock held by someone else
skill agent-lock check
if [ $? -eq 0 ]; then
    skill agent-lock acquire dande
    # Do cleanup...
    skill agent-lock release dande
fi
```

## Notes

- Lock expires after 5 minutes automatically (fail-safe)
- Always release lock even on errors
- Use `trap` for cleanup:
  ```bash
  trap 'skill agent-lock release roll' EXIT
  ```
