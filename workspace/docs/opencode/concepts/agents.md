# Agents (OpenCode)

Agents are specialized AI assistants that can be configured for specific tasks and workflows. You can switch between agents during a session or invoke them with `@` mention.

## Types

- **Primary agents**: Main assistants you interact with directly. Cycle with Tab key. Handle main conversation. Tool access configured via permissions.
- **Subagents**: Specialized assistants that primary agents can invoke. Manually invoked via `@mention`.

## Built-in Agents

- **build** (primary) — Default, full-access agent for development work. All tools enabled.
- **plan** (primary) — Restricted agent for planning and analysis. Edits and bash require approval by default. Denies file edits and bash by default.
- **general** (subagent) — General-purpose for complex searches and multi-step tasks. Full tool access (except todo). Invoke with `@general`.
- **explore** (subagent) — Fast, read-only agent for exploring codebases. Cannot modify files. Use for quick file pattern searches, code keyword searches, answering codebase questions.

System agents (hidden): compaction, title, summary.

## Configuration

Configure agents in JSON config or via markdown files.

### JSON example

```
{
  "$schema": "https://opencode.ai/config.json",
  "agent": {
    "build": {
      "mode": "primary",
      "model": "anthropic/claude-sonnet-4-20250514",
      "prompt": "{file:./prompts/build.txt}",
      "tools": { "write": true, "edit": true, "bash": true }
    },
    "plan": {
      "mode": "primary",
      "model": "anthropic/claude-haiku-4-20250514",
      "tools": { "write": false, "edit": false, "bash": false }
    },
    "code-reviewer": {
      "description": "Reviews code for best practices and potential issues",
      "mode": "subagent",
      "model": "anthropic/claude-sonnet-4-20250514",
      "prompt": "You are a code reviewer. Focus on security, performance, and maintainability.",
      "tools": { "write": false, "edit": false }
    }
  }
}
```

### Markdown agents

Place markdown files in `~/.config/opencode/agents/` (global) or `.opencode/agents/` (project). Filename becomes agent name.

```
---
description: Reviews code for quality and best practices
mode: subagent
tools:
  write: false
  edit: false
---
Only analyze code and suggest changes.
```

## Options

- `description` — brief description of agent’s purpose (required).
- `temperature` — 0.0–1.0; default 0 for most, 0.55 for Qwen.
- `steps` — max agentic iterations before forced response.
- `disable` — set true to disable the agent.
- `prompt` — custom system prompt file (relative to config dir).
- `model` — override model for this agent (provider/model format).
- `tools` — enable/disable specific tools (true/false). Wildcards supported.
- `permission` — granular allow/ask/deny rules for tools.
- `mode` — primary, subagent, or all (default all).
- `hidden` — hide subagent from @ autocomplete (internal only).
- `task` — which subagents can be invoked (glob patterns).
- `color` — UI color (hex or theme color name).
- `top_p` — alternative diversity control (0.0–1.0).
- Any additional keys are passed to the provider as model options.

## Use cases

- **build** — full development work
- **plan** — analysis and planning without changes
- **review** — code review (read-only)
- **debug** — investigation with bash and read
- **docs** — documentation writing (file ops, no system commands)

---
Source: https://opencode.ai/docs/agents
