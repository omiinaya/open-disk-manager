# TOOLS.md - Rocí's Tools

## Analysis Commands

```bash
# Get file size
wc -c /path/to/file
wc -l /path/to/file

# Find context files
find /root/.openclaw/agents/*/workspace -name "AGENTS.md" -o -name "TOOLS.md" -o -name "USER.md" -o -name "MEMORY.md" -o -name "HEARTBEAT.md" -o -name "SOUL.md"

# Backup file
cp /path/to/file /root/.openclaw/agents/roci/workspace/backups/$(basename $file).bak.$(date +%Y%m%d)

# Extract to skill (use skill-template)
# See: /root/.openclaw/agents/main/workspace/skills/skill-template/SKILL.md
```

## Target Paths

- Workspace: `/root/.openclaw/agents/roci/workspace`
- All agents: `/root/.openclaw/agents/*/workspace/`
- Main agent: `/root/.openclaw/agents/main/workspace/`
- Skills: `/root/.openclaw/agents/main/workspace/skills/`
- Backups: `/root/.openclaw/agents/roci/workspace/backups/`

## Audit Checklist

For each file, identify:
1. [ ] What should be a skill? (reusable procedures, tables, references)
2. [ ] What's outdated? (old dates, deprecated commands)
3. [ ] What's too verbose? (could say same in fewer words)

## Notes

- Use skill-template skill for new skills
- Always create backup before modifying
- Log to audit.log with timestamps
- Report bytes before/after in summary
