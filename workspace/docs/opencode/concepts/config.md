# Configuration (OpenCode)

OpenCode uses a JSON/JSONC config file. Schema: https://opencode.ai/config.json

## Locations (precedence order, later overrides earlier)

1. Remote config (from `.well-known/opencode`) — organizational defaults
2. Global config (`~/.config/opencode/opencode.json`) — user preferences
3. Custom config (`OPENCODE_CONFIG` env var) — custom overrides
4. Project config (`opencode.json` in project root) — project-specific
5. `.opencode/` directories — agents, commands, plugins
6. Inline config (`OPENCODE_CONFIG_CONTENT` env var) — runtime overrides

Configs are merged; non-conflicting keys are preserved.

## Format

JSON with optional comments (JSONC). Example:

```
{
  "$schema": "https://opencode.ai/config.json",
  // Theme configuration
  "theme": "opencode",
  "model": "anthropic/claude-sonnet-4-5",
  "autoupdate": true,
}
```

## Key sections

### `tui` — Terminal UI settings

- `scroll_acceleration.enabled` — macOS-style smooth scrolling
- `scroll_speed` — multiplier (default 3)
- `diff_style` — "auto" or "stacked"

### `server` — `opencode serve` settings

- `port`, `hostname`
- `mdns` — service discovery
- `mdnsDomain`
- `cors` — additional origins for browser clients

### `tools` — enable/disable tools globally

```
"tools": { "write": false, "bash": false }
```

### `provider` / `model` / `small_model`

Configure providers and models. Model format: `provider/model-id`. `small_model` for lightweight tasks.

Provider-specific options (e.g., `anthropic.options.timeout`, `amazon-bedrock.options.region`).

### `agent` — define agents

See agents.md.

### `default_agent` — which primary agent to use by default

Must be a primary agent (not subagent).

### `share` — session sharing mode

`"manual"` (default), `"auto"`, `"disabled"`.

### `command` — custom slash commands

Define in JSON or via markdown files in commands directories.

### `keybinds` — custom keybindings

### `autoupdate` — boolean or `"notify"`

### `formatter` — code formatter configuration

### `permission` — global permissions (tools allow/ask/deny)

### `compaction` — context compaction behavior

```
"compaction": { "auto": true, "prune": true, "reserved": 10000 }
```

### `watcher` — file watcher ignore patterns

```
"watcher": { "ignore": ["node_modules/**", "dist/**"] }
```

### `mcp` — MCP server configurations

### `plugin` — npm plugin packages

### `instructions` — array of instruction file paths or glob patterns

### `disabled_providers` / `enabled_providers` — provider allowlist/deny

### `experimental` — experimental flags

## Variables in config

- `{env:VARIABLE}` — substitute environment variable
- `{file:path}` — substitute file contents (relative to config or absolute)

## Schema validation

Your editor can use `https://opencode.ai/config.json` for autocomplete and validation. Set `"$schema"` at the top of your config.

---
Source: https://opencode.ai/docs/config
