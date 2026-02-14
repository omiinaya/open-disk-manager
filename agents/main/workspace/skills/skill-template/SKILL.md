---
name: skill-template
description: Generate a new skill from extracted content. Use when Rocí audits files and finds reusable content to extract.
---

# Skill Template

Use this skill when you need to create a new skill from content extracted during an audit.

## When to Use

- Rocí finds large tables, procedures, or static references in context files
- Content can be reused across agents
- You want to modularize the context

## Procedure

1. **Identify the content** to extract:
   - Large tables or reference data
   - Repeated procedures across files
   - Static configuration that doesn't change

2. **Determine skill name**:
   - Use kebab-case: `my-skill-name`
   - Match the function: e.g., `config-merge`, `git-backup`

3. **Create directory**:
   ```
   ~/.openclaw/agents/main/workspace/skills/<skill-name>/
   ```

4. **Write SKILL.md** with:
   ```markdown
   ---
   name: <skill-name>
   description: <what it does>
   ---

   # <Skill Name>

   <Detailed description>

   ## When to Use
   - <scenario 1>
   - <scenario 2>

   ## Procedure
   1. <step 1>
   2. <step 2>
   ...

   ## Examples
   ```
   <example usage>
   ```

   ## Notes
   - <any caveats>
   ```

5. **Add run.sh if executable logic needed**:
   ```bash
   #!/bin/bash
   # Skill: <skill-name>
   # Purpose: <what it does>

   # Your logic here
   ```

6. **Make executable**: `chmod +x run.sh`

7. **Replace source content** with reference:
   ```
   See skill: <skill-name>
   ```

8. **Update MEMORY.md** with the new skill

## Example

Creating `config-merge` skill from repeated merge procedures:
```
skills/config-merge/
├── SKILL.md    # Merge two JSON configs
└── run.sh      # jq-based merge logic
```

## Notes

- Keep skills focused: one skill, one purpose
- Always include `When to Use` section
- Use clear, actionable procedure steps
