# Commands (OpenCode)

Custom commands let you define reusable prompts. Execute with `/command-name`.

## Create command files

Place markdown files in `.opencode/commands/` (project) or `~/.config/opencode/commands/` (global).

Example: `.opencode/commands/test.md`

```
---
description: Run tests with coverage
agent: build
model: anthropic/claude-3-5-sonnet-20241022
---

Run the full test suite with coverage report and show any failures.
Focus on the failing tests and suggest fixes.
```

The filename becomes the command name: `/test`.

## JSON config alternative

```
{
  "command": {
    "test": {
      "template": "Run full test suite with coverage...",
      "description": "Run tests with coverage",
      "agent": "build",
      "model": "anthropic/claude-3-5-sonnet-20241022"
    }
  }
}
```

## Prompt features

- `$ARGUMENTS` — all arguments; `$1`, `$2`, … for positional
- `!command` — inject shell command output
- `@filename` — include file content

Examples:

```
---
description: Create component
---
Create a new React component named $ARGUMENTS with TypeScript.
```

```
---
description: Analyze coverage
---
Current test results:
!`npm test`
Suggest improvements.
```

```
---
description: Review component
---
Review @src/components/Button.tsx for performance issues.
```

## Options

- `template` (required) — prompt text
- `description` — shown in UI
- `agent` — which agent executes (default: current)
- `model` — override model
- `subtask` — force subagent invocation

Built-in commands: `/init`, `/undo`, `/redo`, `/share`, `/help`, `/compact`, `/new`, `/sessions`, `/models`, `/themes`, `/connect`, `/exit`, `/editor`, `/export`, `/details`, `/thinking`, `/redo`, `/unshare`.

---
Source: https://opencode.ai/docs/commands
