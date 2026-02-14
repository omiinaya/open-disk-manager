# Long-term Memory

## API Keys & Tokens

**NEVER** put secrets directly in `openclaw.json`. Use this pattern:

1. Add key to `~/.openclaw/.env`:
   ```
   NEW_API_KEY=sk-xxx
   ```

2. Reference in `openclaw.json` using `${VAR}` syntax:
   ```json
   {
     "models": {
       "providers": {
         "myprovider": {
           "apiKey": "${NEW_API_KEY}"
         }
       }
     }
   }
   ```

3. The `.env` file is gitignored — safe to add new env vars anytime

4. Restart gateway after changes: `openclaw gateway restart`

Current keys in `.env`: NVIDIA_API_KEY, OPENCODE_API_KEY, OPENROUTER_API_KEY, DISCORD_TOKEN, GATEWAY_TOKEN

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
