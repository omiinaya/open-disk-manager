# Agent Runtime

OpenClaw runs a single embedded agent runtime derived from pi-mono.

## Workspace (required)

OpenClaw uses a single agent workspace directory (agents.defaults.workspace) as the agent's only working directory (cwd) for tools and context.

Full workspace layout: see [Agent Workspace](/concepts/agent-workspace)

If agents.defaults.sandbox is enabled, non-main sessions can override with per-session workspaces.

## Bootstrap files (injected)

Inside agents.defaults.workspace, OpenClaw expects these user-editable files:

- `AGENTS.md` — operating instructions + memory
- `SOUL.md` — persona, boundaries, tone
- `TOOLS.md` — user-maintained tool notes
- `BOOTSTRAP.md` — one-time first-run ritual (delete after completion)
- `IDENTITY.md` — agent name/vibe/emoji
- `USER.md` — user profile + preferred address
- `HEARTBEAT.md` — optional checklist for heartbeat runs
- `BOOT.md` — optional startup checklist (when hooks enabled)

These are injected at session start. Missing files get a placeholder marker.

## Built-in tools

Core tools (read/exec/edit/write and related system tools) are always available, subject to tool policy. `apply_patch` is optional and gated by `tools.exec.applyPatch`.

## Skills

OpenClaw loads skills from:

- Bundled (shipped with install)
- Managed/local: `~/.openclaw/skills`
- Workspace: `<workspace>/skills`

Workspace skills override managed/bundled on name conflict.

## Sessions

Session transcripts stored as JSONL at:

- `~/.openclaw/agents/<agentId>/sessions/<sessionId>.jsonl`

Legacy Pi/Tau session folders are not read.

## Model references

Model refs parsed by splitting on first `/`. Use `provider/model` (e.g., `anthropic/claude-opus-4-6`). If provider omitted, OpenClaw assumes anthropic (deprecated).

## Minimal config

At minimum, set:

- `agents.defaults.workspace`
- `channels.whatsapp.allowFrom` (strongly recommended)

---
Source: https://docs.openclaw.ai/concepts/agent
