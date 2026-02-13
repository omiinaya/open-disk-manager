---
name: defrag
description: Audit and automatically optimize context files (workspace + all agents). Moves domain-specific skills to matching agents, trims verbosity, and applies improvements without confirmation.
---

# Context Defragmentation

This skill analyzes and automatically optimizes the assistant's context. It scans workspace and all agent directories, then applies improvements including:

- **Skill placement**: moves domain-specific skills (e.g., `myagent`) from `skills/` to `agents/<agent>/skills/`
- **File condensation**: reduces verbosity in AGENTS.md, MEMORY.md, etc. (best-effort)
- **Cleanup**: removes outdated patterns, wraps long lines, and condenses imperatives

## Usage

Invoke by telling the assistant: **"defrag yourself"** or **"run defrag"**

The assistant will:
1. Scan workspace files and all agent directories
2. Generate an audit report with current vs projected sizes
3. **Automatically apply** all safe recommendations, including:
   - Moving skills to their matching agent directories
   - Condensing overly verbose sections
   - Removing obvious outdated patterns
4. Output a summary of changes made and token savings

## Automatic Application

The skill runs in **auto-apply mode** by default. It will:
- Move any skill whose name matches an existing agent ID to `agents/<agent>/skills/`
- Condense AGENTS.md, MEMORY.md, TOOLS.md according to heuristics
- Remove word "old" and other generic placeholders
- Wrap lines >120 chars where safe

No confirmation prompts. Review the report afterward to ensure changes are correct.

## Integration

This skill runs in the main agent session and uses the `read`, `write`, `exec` tools to access and modify files. It requires write permissions on the workspace.
