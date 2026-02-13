# Cron Jobs (Gateway Scheduler)

Cron is the Gateway’s built-in scheduler. It persists jobs, wakes the agent at the right time, and can optionally deliver output back to a chat.

## TL;DR

- Cron runs inside the Gateway (not inside the model).
- Jobs persist under `~/.openclaw/cron/jobs.json`.
- Two execution styles:
  - Main session: enqueue a system event, run on next heartbeat (`session: "main"`, `payload.kind: "systemEvent"`).
  - Isolated: run a dedicated agent turn (`session: "isolated"`, `payload.kind: "agentTurn"`).
- Wakeups: `wakeMode: "now"` (immediate) or `"next-heartbeat"`.

## Quick start

One-shot reminder:
```
openclaw cron add \
  --name "Reminder" \
  --at "2026-02-01T16:00:00Z" \
  --session main \
  --system-event "Reminder: check the cron docs draft" \
  --wake now \
  --delete-after-run
```

Recurring isolated job with delivery:
```
openclaw cron add \
  --name "Morning brief" \
  --cron "0 7 * * *" \
  --tz "America/Los_Angeles" \
  --session isolated \
  --message "Summarize overnight updates." \
  --announce \
  --channel slack \
  --to "channel:C1234567890"
```

## Concepts

- **Job**: schedule + payload + optional delivery + optional agent binding.
- **Schedules**:
  - `at`: one-shot ISO timestamp
  - `every`: fixed interval (milliseconds via `everyMs`)
  - `cron`: 5-field cron expression with optional IANA timezone
- **Main vs Isolated**:
  - Main: systemEvent, runs on next heartbeat; good for context-aware tasks.
  - Isolated: fresh agent turn; can override model/thinking; defaults to announce summary.
- **Delivery** (isolated only):
  - `mode`: `announce` (deliver summary) or `none` (internal only).
  - `channel` and `to` target the output.
- **Model override**: isolated jobs can set `model` and `thinking`.

## Storage

- Job store: `~/.openclaw/cron/jobs.json`
- Run history: `~/.openclaw/cron/runs/*.jsonl` (auto-pruned)

## Config

```
{
  cron: {
    enabled: true,
    store: "~/.openclaw/cron/jobs.json",
    maxConcurrentRuns: 1
  }
}
```

## Troubleshooting

- “Nothing runs”: check `cron.enabled`, gateway running continuously, timezone.
- Recurring job delays after failures: exponential backoff (30s, 1m, 5m, 15m, 60m). Resets after success.
- Telegram wrong topic: use `-100…:topic:123` format.

---
Source: https://docs.openclaw.ai/automation/cron-jobs
