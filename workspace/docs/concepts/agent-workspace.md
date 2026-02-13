# Agent Workspace

The workspace is the agent’s home. It is the only working directory used for file tools and for workspace context. Keep it private and treat it as memory. This is separate from `~/.openclaw/`, which stores config, credentials, and sessions.

Important: the workspace is the default cwd, not a hard sandbox. Tools resolve relative paths against the workspace, but absolute paths can still reach elsewhere on the host unless sandboxing is enabled.

## Default location

- Default: `~/.openclaw/workspace`
- If `OPENCLAW_PROFILE` is set and not `default`, the default becomes `~/.openclaw/workspace-<profile>`
- Override in `~/.openclaw/openclaw.json`: `{ agent: { workspace: "~/.openclaw/workspace" } }`

## Workspace file map

- `AGENTS.md` — operating instructions, loaded every session
- `SOUL.md` — persona, tone, boundaries
- `USER.md` — who the user is and how to address them
- `IDENTITY.md` — agent name/vibe/emoji (bootstrap)
- `TOOLS.md` — notes about local tools and conventions
- `HEARTBEAT.md` — optional tiny checklist for heartbeat runs
- `BOOT.md` — optional startup checklist (when hooks enabled)
- `BOOTSTRAP.md` — one-time first-run ritual (delete after)
- `memory/YYYY-MM-DD.md` — daily log, append-only
- `MEMORY.md` — curated long-term memory (load only in main private session)
- `skills/` — workspace-specific skills (overrides)
- `canvas/` — canvas UI files for node displays

## What is NOT in the workspace

These live under `~/.openclaw/` and should NOT be committed to the workspace repo:

- `~/.openclaw/openclaw.json` (config)
- `~/.openclaw/credentials/` (OAuth tokens, API keys)
- `~/.openclaw/agents/<agentId>/sessions/` (session transcripts)
- `~/.openclaw/skills/` (managed skills)

## Git backup (recommended, private)

Treat the workspace as private memory. Put it in a private git repo.

Initialize:
```
cd ~/.openclaw/workspace
git init
git add AGENTS.md SOUL.md TOOLS.md IDENTITY.md USER.md HEARTBEAT.md memory/
git commit -m "Add agent workspace"
```

Add remote and push.

## Moving workspace

Clone to desired path, set `agents.defaults.workspace` in config, run `openclaw setup --workspace` to seed missing files. Copy sessions separately from `~/.openclaw/agents/<agentId>/sessions/`.

---
Source: https://docs.openclaw.ai/concepts/agent-workspace
