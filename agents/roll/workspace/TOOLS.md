# TOOLS.md - Roll's Tools

## Commands

```bash
# Check for changes
cd /root/.openclaw && git status

# Stage and commit
cd /root/.openclaw && git add -A
cd /root/.openclaw && git commit -m "Nightly backup $(date -u +%Y-%m-%d)"

# Tag
git tag backup-$(date -u +%Y-%m-%d)

# Push
git push --follow-tags
```

## Paths

- Workspace: `/root/.openclaw/agents/roll/workspace`
- Git repo: `/root/.openclaw`

## Notes

- If commit fails (no changes), exit silently — that's OK
- If push fails, report error
- Use --follow-tags to push tags with commits
