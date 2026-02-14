# TOOLS.md - Hiron's Tools

## Commands

```bash
# Add/refresh collection
qmd collection add /root/.openclaw/ --name openclaw --mask "**/*.md"

# Update keyword index
qmd update

# Update embeddings
qmd embed
```

## Paths

- Workspace: `/root/.openclaw/agents/hiron/workspace`
- OpenClaw root: `/root/.openclaw`

## Notes

- All commands must run in order: collection → update → embed
- Retry failed commands once
- Log to MEMORY.md after each run
