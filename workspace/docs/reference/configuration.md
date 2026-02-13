# Configuration Reference

This document is a truncated but comprehensive reference for `~/.openclaw/openclaw.json`.

Full official reference: https://docs.openclaw.ai/gateway/configuration-reference

## Top-level fields

- `agents` — agent definitions and defaults
- `tools` — tool allow/deny, elevated, profiles
- `channels` — per-channel config (whatsapp, telegram, discord, slack, etc.)
- `gateway` — network, auth, control UI, tailscale
- `skills` — skill loading and entries
- `plugins` — plugin allowlist and configs
- `memory` — memory backend and search settings (including qmd)
- `cron` — scheduler enable/disable, store path, max concurrent
- `logging` — level, file, console style, redaction
- `env` — inline env vars and shellEnv
- `ui` — seamColor, assistant identity
- `session` — scopes, reset policies, maintenance
- `messages` — responsePrefix, ackReaction, queue, inbound debounce, TTS
- `heartbeat` — (also under `agents.defaults.heartbeat`) global heartbeat overrides?
- `browser` — browser control settings
- `canvasHost` — canvas UI server
- `discovery` — mDNS and wide-area DNS-SD
- `hooks` — webhook server and mappings
- `talk` — talk mode defaults

## Agents section highlights

```
agents: {
  defaults: {
    workspace: "~/.openclaw/workspace",
    model: { primary: "...", fallbacks: [...] },
    heartbeat: { every: "30m", target: "last" },
    compaction: { mode: "safeguard", memoryFlush: { enabled: true } },
    sandbox: { mode: "off" },
    tools: { profile: "coding" }
  },
  list: [
    {
      id: "main",
      default: true,
      name: "...",
      workspace: "...",
      agentDir: "...",
      identity: { name, theme, emoji, avatar },
      groupChat: { mentionPatterns [...] },
      sandbox: { ... },
      tools: { allow: [...], deny: [...], elevated: { enabled: true } },
      heartbeat: { ... } // per-agent override
    }
  ]
}
```

## Tools groups

- `group:runtime` — exec, process
- `group:fs` — read, write, edit, apply_patch
- `group:sessions` — sessions_list, sessions_history, sessions_send, sessions_spawn, session_status
- `group:memory` — memory_search, memory_get
- `group:web` — web_search, web_fetch
- `group:ui` — browser, canvas
- `group:automation` — cron, gateway
- `group:messaging` — message
- `group:nodes` — nodes
- `group:openclaw` — all built-in tools (excluding provider plugins)

## Memory configuration

- `memory.backend` — `"sqlite"` (default) or `"qmd"`
- `memory.citations` — `"auto"`, `"on"`, `"off"`
- `memory.qmd` — QMD-specific: `update.interval`, `limits.maxResults`, `paths[]`, `scope`, `sessions.enabled`
- `agents.defaults.memorySearch` — provider, local, remote, hybrid, cache, extraPaths

## Channel quick examples

### WhatsApp
```
channels: {
  whatsapp: {
    enabled: true,
    dmPolicy: "pairing", // pairing|allowlist|open|disabled
    allowFrom: ["+15555550123"],
    groups: { "*": { requireMention: true } },
    groupPolicy: "allowlist"
  }
}
```

### Discord
```
channels: {
  discord: {
    enabled: true,
    token: "...",
    dm: { enabled: true, policy: "pairing", allowFrom: ["1234567890"] },
    guilds: {
      "123456789012345678": {
        slug: "my-guild",
        channels: {
          general: { allow: true }
        }
      }
    }
  }
}
```

## Cron configuration

```
{
  cron: {
    enabled: true,
    store: "~/.openclaw/cron/jobs.json",
    maxConcurrentRuns: 1
  }
}
```

## Security-relevant defaults to consider

- `plugins.allow` — explicit allowlist for plugins
- `tools.deny` — deny dangerous tools globally
- `agents.defaults.sandbox.mode` — `"non-main"` or `"all"` for isolation
- `gateway.bind` — `"loopback"` for local-only, `"lan"` exposes to network
- `gateway.auth` — always require token or password
- `channels.<channel>.allowFrom` — strict allowlists

---
Source: https://docs.openclaw.ai/gateway/configuration-reference
