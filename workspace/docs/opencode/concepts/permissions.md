# Permissions (OpenCode)

Permissions decide whether an action runs automatically, prompts for approval, or is blocked.

Values: `"allow"`, `"ask"`, `"deny"`.

## Configuration

Global config with per-agent overrides.

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

You can also set all at once: `"permission": "allow"`.

## Granular rules (object syntax)

For most permissions, you can define rules based on input patterns.

```
{
  "permission": {
    "bash": {
      "*": "ask",
      "git *": "allow",
      "npm *": "allow",
      "rm *": "deny"
    },
    "edit": {
      "*": "deny",
      "src/components/**/*.tsx": "allow"
    }
  }
}
```

Rule evaluation: last matching rule wins. Put catch-all `*` first, specific rules after.

### Wildcards

- `*` matches zero or more characters
- `?` matches exactly one character

### Home directory expansion

Use `~` or `$HOME` at start of pattern.

### External directories

Access outside working directory requires explicit `external_directory` permission:

```
{
  "permission": {
    "external_directory": {
      "~/projects/personal/**": "allow"
    },
    "edit": {
      "~/projects/personal/**": "deny"
    }
  }
}
```

## Available permissions

- `read` — reading a file (matches file path)
- `edit` — all file modifications (edit, write, patch, multiedit)
- `glob` — file globbing (matches glob pattern)
- `grep` — content search (matches regex)
- `list` — listing files in a directory (matches directory path)
- `bash` — running shell commands (matches parsed command like `git status --porcelain`)
- `task` — launching subagents (matches subagent type)
- `skill` — loading a skill (matches skill name)
- `lsp` — running LSP queries (non-granular currently)
- `todoread`, `todowrite` — todo list
- `webfetch` — fetching a URL (matches URL)
- `websearch`, `codesearch` — web/code search (matches query)
- `external_directory` — triggered when a tool touches paths outside project
- `doom_loop` — triggered when same tool call repeats 3 times with identical input

## What “ask” does

Prompts with three outcomes:
- `once` — approve just this request
- `always` — approve future matching requests (session-scoped)
- `reject` — deny

## Agent-specific permissions

Agent permissions are merged with global config; agent rules take precedence.

```
{
  "permission": { "bash": { "*": "ask", "git *": "allow" } },
  "agent": {
    "build": {
      "permission": { "bash": { "git push *": "deny" } }
    }
  }
}
```

Markdown agent example:

```
---
description: Code review without edits
mode: subagent
permission:
  edit: deny
  bash:
    "*": ask
    "git diff": allow
    "git log*": allow
    "grep *": allow
  webfetch: deny
---
Only analyze code and suggest changes.
```

---
Source: https://opencode.ai/docs/permissions
