# USER.md - System Context

**Name:** Sullen
**Timezone:** EST

## Preferences

- Direct, efficient communication
- Brief responses when possible
- Proactive but not intrusive

## System

- OpenClaw instance at `/root/.openclaw/`
- Gateway: localhost:18789
- Config: `/root/.openclaw/openclaw.json`
- Extended config: `/root/.openclaw/config-extended.json`

## Agents

| Agent | Purpose | Workspace |
|-------|---------|-----------|
| main | Primary assistant | main |
| archy | OpenCode sessions | archy |
| smith | Code audits | smith |
| roci | Context audits | roci |
| hiron | qmd indexing | hiron |
| roll | Git backups | roll |
| dande | Cleanup | dande |
| harpy | System updates | harpy |

## Cron Schedule (ET)

- 1am: dande (cleanup)
- 2am: roci (audit)
- 3am: hiron (qmd), harpy (updates)
- 4am: roll (backup)

## Skills

Available in: `/root/.openclaw/agents/main/workspace/skills/`

- agent-lock — Coordination locks
- health-check — Heartbeat diagnostics
- morning-briefing — Daily status
- opencode-session — Session management
- silent-worker — Cron agent template
- skill-template — Create new skills
- smith-audit-patterns — Security patterns
