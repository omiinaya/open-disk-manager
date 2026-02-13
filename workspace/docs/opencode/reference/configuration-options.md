# OpenCode Configuration Options Reference

Based on https://opencode.ai/config.json.

## Top-level keys

- `tui` — scroll_acceleration, scroll_speed, diff_style
- `server` — port, hostname, mdns, mdnsDomain, cors
- `tools` — enable/disable built-in tools (e.g., `"write": false`)
- `provider` — provider-specific options (timeout, setCacheKey, region, profile, endpoint)
- `model` — default model ID (provider/model format)
- `small_model` — model for lightweight tasks (title generation, etc.)
- `agent` — agent definitions (JSON or markdown files)
- `default_agent` — name of default primary agent
- `share` — "manual" | "auto" | "disabled"
- `command` — custom slash commands (JSON)
- `keybinds` — custom keybindings
- `autoupdate` — boolean or "notify"
- `formatter` — formatter configurations (e.g., prettier)
- `permission` — global permissions (tools allow/ask/deny)
- `compaction` — { auto, prune, reserved }
- `watcher` — file watcher ignore patterns
- `mcp` — MCP server configurations
- `plugin` — npm plugin packages
- `instructions` — array of instruction file paths/globs
- `disabled_providers` — array of provider IDs to ignore
- `enabled_providers` — allowlist of provider IDs
- `experimental` — experimental flags

## Config precedence (later overrides earlier)

1. Remote config (`.well-known/opencode`)
2. Global (`~/.config/opencode/opencode.json`)
3. Custom (`OPENCODE_CONFIG`)
4. Project (`opencode.json` in project root)
5. `.opencode/` directories (agents, commands, plugins)
6. Inline (`OPENCODE_CONFIG_CONTENT`)

## Variables

- `{env:VAR}` — environment variable
- `{file:path}` — file contents (relative to config or absolute)

## Agent options

See `concepts/agents.md` for full list.

Common: `description`, `mode` (`primary`|`subagent`|`all`), `model`, `prompt`, `tools`, `permission`, `temperature`, `steps`, `color`, `top_p`, `hidden`, `task`.

## Permission values

- `"allow"` — run automatically
- `"ask"` — prompt for approval
- `"deny"` — block

Granular: per-tool with patterns, wildcards, external_directory.

## Defaults

If unspecified, most tools default to `allow`. Exceptions:
- `external_directory` → ask
- `doom_loop` → ask
- `read` denies `.env*` by default.

---
Derived from https://opencode.ai/docs/config
