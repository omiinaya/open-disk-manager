# Long-term Memory

## Model Preferences

Easily switch between:
- nvidia/stepfun-ai/step-3.5-flash
- nvidia/moonshotai/kimi-k2.5
- nvidia/z-ai/glm4.7

For full configuration, use the `config` skill.

## Extended Configuration

When needing custom agent properties that aren't in the official `openclaw.json` schema:
- Use `~/.openclaw/config-extended.json`
- Structure: top-level keys, typically `{ "agents": { "<agentId>": { ... } } }`
- This file is tracked in git (no secrets) and read by agents via the `read` tool
- Agents check this file at runtime to enable experimental features (e.g., `trainingMode`)
- Keep it small and focused; merge into main config if it becomes stable

Current use:
- `agents.archy.trainingMode = true` — enables self-reflection learning loop for Archy

Never add unknown properties to `openclaw.json`; they will break validation.

## Project Knowledge

### Active Projects
- (Add your active projects here)

### Key Contacts
- (Add important contacts, APIs, services)

### Preferences
- (Add user preferences discovered over time)

## Session Stats

| Date | Sessions | Notes |
|------|----------|-------|
| | | |

## Last Updated
Last updated: 2026-02-14
