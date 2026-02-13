---
name: workspace-save
description: Save workspace configuration by committing and pushing changes to the git remote.
---

# Workspace Save Skill

Use this skill when the user says "save your configuration" or "save workspace" or any variant that means: commit all changes in `/root/.openclaw` and push to the remote.

## Procedure

1. Navigate to `/root/.openclaw`
2. Check `git status` to see what will be committed
3. Stage all changes: `git add -A`
4. Commit with a clear message like "Save configuration" or include the user's specific request
5. Push to origin: `git push origin master` (or current branch)
6. Report success or any errors

## Notes

- The openclaw directory is a git repo with submodules and sensitive files ignored via .gitignore.
- If there's nothing to commit, report that the workspace is already up to date.
- If push fails due to network/auth, report the error and ask for credentials.
- Use the user-provided context for the commit message if given; otherwise default to "Update workspace configuration".