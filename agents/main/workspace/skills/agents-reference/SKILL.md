# Agents Reference

## Agent Table

| Agent | Purpose | Workspace |
|-------|---------|-----------|
| main | Primary assistant | main |
| archy | OpenCode sessions | archy |
| smith | Code audits | smith |
| roci | Context audits | roci |
| hiron | qmd indexing | hiron |
| roll | Git backups | roll |
| dande | Cleanup | dande |
| harpy | System updates | harpy |

## Cron Schedule (ET)

| Time | Agent | Task |
|------|-------|------|
| 1am | dande | Cleanup |
| 2am | roci | Audit |
| 3am | hiron, harpy | QMD, Updates |
| 4am | roll | Backup |
