#!/usr/bin/env python3
"""
Bulk delete messages in a Discord channel with proper rate limiting.

Usage:
  python delete_channel.py --channel CHANNEL_ID [--guild GUILD_ID] [--dry-run]

Requires a Discord bot token with MANAGE_MESSAGES permission.
Token is read from OPENCLAW_CONFIG (or can be passed via --token).
"""

import os
import sys
import json
import time
import argparse
import requests
from typing import List, Optional

API_BASE = "https://discord.com/api/v10"


def get_token() -> str:
    """Extract Discord bot token from OpenClaw config."""
    # Try environment variable first
    token = os.environ.get("DISCORD_BOT_TOKEN")
    if token:
        return token

    # Fallback: read from config file (common location)
    config_paths = [
        "/root/.openclaw/openclaw.json",
        os.path.expanduser("~/.openclaw/openclaw.json"),
    ]
    for path in config_paths:
        if os.path.exists(path):
            with open(path) as f:
                cfg = json.load(f)
            # Extract token from channels.discord.accounts.default.token
            try:
                token = cfg["channels"]["discord"]["accounts"]["default"]["token"]
                if token and token != "__OPENCLAW_REDACTED__":
                    return token
            except KeyError:
                pass

    raise RuntimeError(
        "Discord bot token not found. Set DISCORD_BOT_TOKEN env var or ensure token is in OpenClaw config."
    )


def fetch_messages(channel_id: str, token: str, before: Optional[str] = None) -> List[str]:
    """Fetch up to 100 messages; return list of message IDs (oldest-first order)."""
    headers = {"Authorization": f"Bot {token}", "Content-Type": "application/json"}
    params = {"limit": 100}
    if before:
        params["before"] = before

    resp = requests.get(f"{API_BASE}/channels/{channel_id}/messages", headers=headers, params=params)
    resp.raise_for_status()
    msgs = resp.json()
    if not isinstance(msgs, list):
        return []
    return [m["id"] for m in msgs if "id" in m]


def delete_message(channel_id: str, message_id: str, token: str) -> bool:
    """Delete a single message. Returns True if successful (204), False otherwise."""
    headers = {"Authorization": f"Bot {token}"}
    url = f"{API_BASE}/channels/{channel_id}/messages/{message_id}"
    resp = requests.delete(url, headers=headers)
    if resp.status_code == 204:
        return True
    elif resp.status_code == 429:
        # Rate limited; parse retry-after
        retry = resp.headers.get("Retry-After")
        if retry:
            time.sleep(float(retry))
            return delete_message(channel_id, message_id, token)  # retry once
        else:
            time.sleep(1.0)
            return delete_message(channel_id, message_id, token)
    elif resp.status_code in (403, 404):
        # Forbidden (no perms) or Not Found (already deleted/unknown)
        return False
    else:
        # Other errors: treat as failure but continue
        return False


def main():
    parser = argparse.ArgumentParser(description="Bulk delete Discord channel messages")
    parser.add_argument("--channel", required=True, help="Channel ID")
    parser.add_argument("--guild", help="Guild ID (optional, for future use)")
    parser.add_argument("--dry-run", action="store_true", help="Only count, do not delete")
    parser.add_argument("--token", help="Discord bot token (overrides config lookup)")
    args = parser.parse_args()

    token = args.token or get_token()
    channel_id = args.channel

    print(f"Starting bulk delete for channel {channel_id}")
    total_deleted = 0
    before = None
    backoff = 0.5

    while True:
        ids = fetch_messages(channel_id, token, before)
        if not ids:
            break

        print(f"Fetched {len(ids)} messages to process.")

        for msg_id in ids:
            if args.dry_run:
                print(f"[DRY RUN] Would delete {msg_id}")
                total_deleted += 1
            else:
                # Delete with simple backoff on non-rate-limit failures too
                while True:
                    try:
                        success = delete_message(channel_id, msg_id, token)
                        if success:
                            total_deleted += 1
                            break
                        else:
                            # Non-retryable failure; skip this message
                            break
                    except requests.RequestException as e:
                        print(f"Error deleting {msg_id}: {e}")
                        time.sleep(backoff)
                        backoff = min(backoff * 1.5, 10.0)
                        continue

                # brief pause between deletes to stay under rate limits
                time.sleep(backoff)

        # Paginate: use the last (oldest) ID as the 'before' cursor for next batch
        before = ids[-1]
        print(f"Total deleted so far: {total_deleted}")

    print(f"Finished. Total messages deleted: {total_deleted}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
