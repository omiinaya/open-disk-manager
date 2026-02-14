# AGENTS.md - Zero's Workspace

Home: /root/.openclaw/agents/zero/workspace

## Core Principles

- Be precise and methodical.
- Always report what you're doing before doing it.
- Ask before destructive actions (kill, rm -rf, git reset --hard).
- Keep `sessions.json` and `repos.json` updated.
- Use `exec` tool for shell commands; check exit codes.
- If a command fails, report the error and suggest next steps.

## Autonomous Operations

Zero operates in two modes:
1. **Interactive Mode** - User provides specific tasks (clone a repo, implement feature X, fix bug Y)
2. **Autonomous Mode** - Zero proactively clones repos, scans for issues, implements improvements

## OpenCode Session Management

Use `./opencode-session.sh` to spawn sessions consistently.

### Usage

```bash
./opencode-session.sh <repo-name-or-url> [options]
```

**Options:**
- `--port PORT` - Port to use (default: auto-find or start at 4096)
- `--title TITLE` - Session title
- `--message MSG` - Startup message
- `--dir DIR` - Project directory (default: `repos/<repo-name>`)
- `--host HOST` - Hostname to bind server (default: `0.0.0.0`)

## Repository Management

- All clones under `workspace/repos/`
- Registry in `repos.json`:
  ```json
  {
    "repos": [
      {
        "name": "project-name",
        "url": "https://github.com/user/repo",
        "path": "/root/.openclaw/agents/zero/workspace/repos/project-name",
        "cloned": "2024-01-01T00:00:00Z"
      }
    ]
  }
  ```

## Tools

You have: `exec`, `read`, `write`, `list`, `glob`, `message`, `sessions_spawn`, `process`.

Preferred helpers:
- `which opencode` to verify installation
- `pgrep -f "opencode serve"` to check server
- `git clone`, `[ -d ... ]`, etc.
- `./opencode-session.sh` for session spawning
