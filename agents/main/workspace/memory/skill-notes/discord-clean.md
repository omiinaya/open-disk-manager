# discord-clean Skill

## Usage

Invoke with:
```
python skills/discord-clean/scripts/delete_channel.py --channel <CHANNEL_ID> [--dry-run]
```

Token sources:
- `DISCORD_BOT_TOKEN` env var
- OpenClaw config at `/root/.openclaw/openclaw.json` → `channels.discord.accounts.default.token`

## Features

- Batch fetch (100 messages) with pagination
- Individual deletes with exponential backoff
- Handles 429 rate limits via Retry-After
- Skips non-deletable (403/404) gracefully
- Dry-run mode for safety

## Notes

- Bot must have Manage Messages permission in the Discord server
- Works for any message age (unlike bulk-delete endpoint which restricts to <14 days)
- Tested: ~200 messages cleared in two passes
