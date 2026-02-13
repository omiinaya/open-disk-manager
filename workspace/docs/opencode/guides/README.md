# OpenCode Getting Started

OpenCode is an open source AI coding agent. Available as terminal UI, desktop app, or IDE extension.

## Prerequisites

- Modern terminal emulator (WezTerm, Alacritty, Ghostty, Kitty)
- API keys for LLM providers you want to use

## Installation

```bash
# YOLO
curl -fsSL https://opencode.ai/install | bash

# Package managers
npm i -g opencode-ai@latest   # or bun/pnpm/yarn
scoop install opencode        # Windows
choco install opencode        # Windows
brew install anomalyco/tap/opencode  # macOS/Linux (recommended)
paru -S opencode-bin          # Arch Linux
mise use -g opencode          # Any OS
```

Desktop app also available from releases page.

### Installation directory priority

1. `$OPENCODE_INSTALL_DIR`
2. `$XDG_BIN_DIR`
3. `$HOME/bin` (if exists or can be created)
4. `$HOME/.opencode/bin` (default)

## Configure

Use `/connect` in TUI or set provider API keys directly in config.

Recommended: OpenCode Zen (curated models) via https://opencode.ai/auth.

## Initialize

In a project:

```
cd /path/to/project
opencode
/init
```

This creates `AGENTS.md` to help OpenCode understand the project.

## Usage

Ask questions, add features, make changes, undo/redo, share sessions.

### Agent modes

- **Build** — full-access development (default)
- **Plan** — read-only, asks permissions (ideal for analysis)
- Use Tab to switch.

### Commands

Built-in slash commands: `/connect`, `/init`, `/undo`, `/redo`, `/share`, `/help`, etc.

Custom commands can be defined in config or `.opencode/commands/`.

## Customize

- Themes: `/theme` or `theme:` config
- Keybinds: `keybinds` config
- Formatters: `formatter` config
- Agents: create custom agents via `agent:` config or markdown files

---
Source: https://opencode.ai/docs (and README)
