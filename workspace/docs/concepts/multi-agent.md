# Multi-Agent Routing

Goal: multiple isolated agents (separate workspace + agentDir + sessions) plus multiple channel accounts in one running Gateway. Inbound is routed to an agent via bindings.

## What is one agent?

An agent is a fully scoped brain with:

- Workspace (files, AGENTS.md/SOUL.md/USER.md, persona)
- State directory (`agentDir`) for auth profiles, model registry, per-agent config
- Session store under `~/.openclaw/agents/<agentId>/sessions`

Auth profiles are per-agent. Never reuse `agentDir` across agents.

## Paths (quick map)

- Config: `~/.openclaw/openclaw.json`
- State dir: `~/.openclaw`
- Workspace: `~/.openclaw/workspace` (or `workspace-<profile>`)
- Agent dir: `~/.openclaw/agents/<agentId>/agent`
- Sessions: `~/.openclaw/agents/<agentId>/sessions`

## Single-agent mode (default)

- agentId defaults to `main`
- Workspace defaults to `~/.openclaw/workspace`
- State defaults to `~/.openclaw/agents/main/agent`

## Adding an agent

Use `openclaw agents add <id>` wizard, then add bindings.

## Examples

Two WhatsApp numbers → two agents:
```
agents.list: [
  { id: "home", default: true, workspace: "~/.openclaw/workspace-home", agentDir: "~/.openclaw/agents/home/agent" },
  { id: "work", workspace: "~/.openclaw/workspace-work", agentDir: "~/.openclaw/agents/work/agent" }
]
bindings: [
  { agentId: "home", match: { channel: "whatsapp", accountId: "personal" } },
  { agentId: "work", match: { channel: "whatsapp", accountId: "biz" } }
]
```

## Routing rules (most-specific wins)

1. peer match (exact DM/group/channel id)
2. guildId (Discord) / teamId (Slack)
3. accountId match
4. channel-level match (`accountId: "*"`)
5. fallback default agent

## Per-agent sandbox and tools

Each agent can have its own sandbox and tool restrictions (since v2026.1.6). Example:
```
{
  id: "family",
  workspace: "~/.openclaw/workspace-family",
  sandbox: { mode: "all", scope: "agent" },
  tools: { allow: ["read"], deny: ["exec", "write"] }
}
```

---
Source: https://docs.openclaw.ai/concepts/multi-agent
