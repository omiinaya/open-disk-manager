# OpenClaw Documentation Index

Welcome to the locally cached OpenClaw documentation. This is a snapshot of the official docs for offline reference.

## Getting Started

- [Agent Runtime](concepts/agent.md) — how agents work, bootstrap files, skills
- [Agent Workspace](concepts/agent-workspace.md) — workspace layout, file meanings, git backup
- [Memory](concepts/memory.md) — memory files, vector search, QMD backend
- [Multi-Agent Routing](concepts/multi-agent.md) — multiple agents, bindings, sandbox per agent

## Automation

- [Cron Jobs](automation/cron-jobs.md) — scheduler, main vs isolated, delivery
- [Cron vs Heartbeat](automation/cron-vs-heartbeat.md) — when to use which
- [Heartbeat Guide](../workspace/docs/heartbeat-guide.md) *(see also local HEARTBEAT.md in workspace)*

## Configuration

- [Configuration Reference](reference/configuration.md) — comprehensive field reference (top-level, agents, tools, channels, memory, etc.)

## Additional Resources

- Official docs site: https://docs.openclaw.ai
- GitHub: https://github.com/openclaw/openclaw
- Community Discord: https://discord.com/invite/clawd

## Local Notes

Your current setup:
- Main agent workspace: `~/.openclaw/agents/main/workspace`
- Other agents: `~/.openclaw/agents/<id>/workspace`
- Config: `~/.openclaw/openclaw.json`
- Gateway status: `openclaw status`
- Doctor: `openclaw doctor --fix`

Always back up workspace memory files (private git recommended).
