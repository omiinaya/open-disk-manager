---
name: discord-clean
description: Bulk delete messages in a Discord channel with rate limit handling and verification. Use when you need to clear a channel's message history programmatically and safely. Supports fetching in batches, respects Discord rate limits, and retries on 429 responses. Requires the bot to have Manage Messages permission.
---

# Discord Channel Cleaner

## Quick Start

```bash
# Actual deletion (Node.js script, no dependencies):
node scripts/delete_channel_v2.js --channel 1471005448948813874
```

The script automatically discovers the Discord bot token from:
- `DISCORD_BOT_TOKEN` environment variable, OR
- OpenClaw config at `/root/.openclaw/openclaw.json` under `channels.discord.accounts.default.token`

## How It Works

1. Fetches messages in batches of 100 (oldest-first) using Discord API pagination
2. Deletes each message individually with small delays
3. Handles `429 Too Many Requests` by reading `Retry-After` header and waiting
4. Continues until no messages remain
5. Reports total deleted count

## Notes

- Only messages the bot can access will be deleted (respects channel permissions and message age)
- Bulk delete endpoint cannot be used because it requires messages <14 days old and batch size >2; this script deletes one-by-one to handle any age
- The script is deterministic and logs progress to stdout
- Non-deletable messages (pinned, system, or from users the bot can't manage) will be skipped with a `403` or `404`
- Uses native `fetch` (Node 18+), no external dependencies

## Integration with OpenClaw

This skill is intended to be invoked via the `exec` tool. Example agent call:

```yaml
action: exec
command: ["node", "skills/discord-clean/scripts/delete_channel_v2.js", "--channel", "1471005448948813874"]
workdir: /root/.openclaw/workspace/agents/ciel
```

Ensure `tools.exec` is allowed and the bot has `manage_messages` permission in the target Discord server.
