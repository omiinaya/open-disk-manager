# TUI (OpenCode)

Interactive terminal interface for working with the AI coding agent.

## Start

```
opencode
opencode /path/to/project
```

## Features

- **File references**: `@filename` to include file in prompt
- **Bash commands**: start message with `!cmd` to run shell command
- **Slash commands**: `/command` for actions (connect, init, undo, redo, share, etc.)
- **Agent switching**: Tab cycles primary agents (build, plan, custom)
- **Subagent invocation**: `@general` etc.
- **Session navigation**: `+Right` / `+Left` to cycle parent/child sessions

## Built-in slash commands

| Command | Description | Keybind |
|---------|-------------|---------|
| `/connect` | Add provider API key |  |
| `/compact` | Compact session (alias `/summarize`) | ctrl+x c |
| `/details` | Toggle tool execution details | ctrl+x d |
| `/editor` | Open external editor for message | ctrl+x e |
| `/exit` | Exit OpenCode (aliases `/quit`, `/q`) | ctrl+x q |
| `/export` | Export conversation to Markdown | ctrl+x x |
| `/help` | Show help dialog | ctrl+x h |
| `/init` | Create/update AGENTS.md | ctrl+x i |
| `/models` | List available models | ctrl+x m |
| `/new` | Start new session (alias `/clear`) | ctrl+x n |
| `/sessions` | List/switch sessions (aliases `/resume`, `/continue`) | ctrl+x l |
| `/share` | Share current session link | ctrl+x s |
| `/themes` | List themes | ctrl+x t |
| `/thinking` | Toggle reasoning visibility |  |
| `/undo` | Undo last message and file changes | ctrl+x u |
| `/unshare` | Unshare session |  |

## Editor setup

`/editor` and `/export` use `$EDITOR`. For GUI editors include `--wait`:

```
export EDITOR="code --wait"
```

## TUI config

```
{
  "tui": {
    "scroll_acceleration": { "enabled": true },
    "scroll_speed": 3,
    "diff_style": "auto"
  }
}
```

## Customization via command palette

Press ctrl+x h (or `/help`) to open palette. Toggle username display and other UI settings persist across sessions.

---
Source: https://opencode.ai/docs/tui
