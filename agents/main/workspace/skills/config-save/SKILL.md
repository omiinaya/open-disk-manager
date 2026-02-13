---
name: config-save
description: Save configuration by committing and pushing changes in ~/.openclaw to the git remote.
---

# Config Save Skill

Use this skill when the user says "save config" or "save configuration" or any variant that means: commit all changes in `/root/.openclaw` and push to the remote.

## Procedure

1. Navigate to `/root/.openclaw`
2. Check `git status` to see what will be committed
3. **Generate and stage a sanitized config backup:**
   - Run `skills/config-save/guardian` (a Python script)
   - This creates `docs/openclaw/sanitized-config.json` with all secrets redacted
   - The file is automatically staged for commit
4. Stage all other changes: `git add -A`
5. Commit with a clear message like "Save configuration" or include the user's specific request
6. Push to origin: `git push origin master` (or current branch)
7. Report success or any errors

## Guardian Script

Location: `skills/config-save/guardian`

The guardian automates safe config backup:
- Reads `~/.openclaw/openclaw.json`
- Redacts all fields with sensitive keys (token, password, secret, apiKey, auth, etc.)
- Writes `~/.openclaw/docs/openclaw/sanitized-config.json`
- Stages the sanitized file for commit
- Adds `_meta` field with timestamp and purpose

Usage: `guardian [--dry-run]` (dry-run shows actions without writing)

**Important:** The real `openclaw.json` is gitignored and never committed. The sanitized copy is safe to push and serves as a structural backup/template.

## Notes

- The openclaw directory is a git repo with submodules and sensitive files ignored via .gitignore.
- If there's nothing to commit, report that the workspace is already up to date.
- If push fails due to network/auth, report the error and ask for credentials.
- Use the user-provided context for the commit message if given; otherwise default to "Update workspace configuration".
- The sanitized config backup is created every time you run "save config" – it's your versioned template.