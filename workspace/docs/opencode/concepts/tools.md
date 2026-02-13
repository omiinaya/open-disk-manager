# Tools (OpenCode)

Tools allow the LLM to perform actions in your codebase. OpenCode comes with built-in tools and can be extended with custom tools or MCP servers.

By default, all tools are enabled. Control behavior through `permission` config.

## Configure permissions

Global:
```
{
  "$schema": "https://opencode.ai/config.json",
  "permission": {
    "*": "ask",
    "bash": "allow",
    "edit": "deny"
  }
}
```

Per-agent overrides supported.

## Built-in tools

- `bash` — Execute shell commands in project environment.
- `edit` — Modify existing files using exact string replacements.
- `write` — Create new files or overwrite existing ones.
- `read` — Read file contents (supports line ranges).
- `grep` — Search file contents using regex.
- `glob` — Find files by pattern (e.g., `**/*.js`).
- `list` — List files and directories (accepts glob filters).
- `lsp` (experimental) — LSP operations: goToDefinition, findReferences, hover, documentSymbol, workspaceSymbol, goToImplementation, call hierarchy.
- `patch` — Apply patch files (requires edit permission).
- `skill` — Load a skill (SKILL.md) and return its content.
- `todowrite` / `todoread` — Manage todo lists.
- `webfetch` — Fetch web content.
- `websearch` — Search the web using Exa AI (no API key needed).
- `question` — Ask the user questions during execution (multiple choice or free text).

## Custom tools

Define in config or via MCP servers.

## Internals

Tools like `grep`, `glob`, `list` use ripgrep under the hood and respect `.gitignore`. Use a `.ignore` file to explicitly include ignored paths (e.g., `!node_modules/`).

## Defaults

If unspecified, most permissions default to `"allow"`. Exceptions:
- `doom_loop` (repeated identical tool calls) → `"ask"`
- `external_directory` (paths outside workspace) → `"ask"`
- `.env` files are denied for `read` by default.

---
Source: https://opencode.ai/docs/tools
