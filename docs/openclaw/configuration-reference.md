# Configuration Reference

Complete field-by-field reference for `~/.openclaw/openclaw.json`

Config format is **JSON5** (comments + trailing commas allowed). All fields are optional — OpenClaw uses safe defaults when omitted.

***

## Channels

Each channel starts automatically when its config section exists (unless `enabled: false`).

### DM and group access

All channels support DM policies and group policies:

| DM policy           | Behavior                                                        |
| ------------------- | --------------------------------------------------------------- |
| `pairing` (default) | Unknown senders get a one-time pairing code; owner must approve |
| `allowlist`         | Only senders in `allowFrom` (or paired allow store)             |
| `open`              | Allow all inbound DMs (requires `allowFrom: ["*"]`)             |
| `disabled`          | Ignore all inbound DMs                                          |

| Group policy          | Behavior                                               |
| --------------------- | ------------------------------------------------------ |
| `allowlist` (default) | Only groups matching the configured allowlist          |
| `open`                | Bypass group allowlists (mention-gating still applies) |
| `disabled`            | Block all group/room messages                          |

<Note>
  `channels.defaults.groupPolicy` sets the default when a provider's `groupPolicy` is unset.
  Pairing codes expire after 1 hour. Pending DM pairing requests are capped at **3 per channel**.
  Slack/Discord have a special fallback: if their provider section is missing entirely, runtime group policy can resolve to `open` (with a startup warning).
</Note>

... [full content preserved from source]