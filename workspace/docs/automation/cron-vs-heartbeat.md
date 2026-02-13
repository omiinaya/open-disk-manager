# Cron vs Heartbeat

Both let you run tasks on a schedule. Use the right tool for the job.

## Quick Decision

Use **Heartbeat** when:
- Can be batched with other checks (inbox, calendar, notifications)
- Context-aware decisions needed
- Conversational continuity helpful
- Low-overhead monitoring (one turn replaces many polls)

Use **Cron** (isolated) when:
- Exact timing required (“9:00 AM sharp”)
- Standalone tasks, no context needed
- Different model/thinking needed
- One-shot reminders (“in 20 minutes”)
- Noisy/frequent tasks that would clutter main history

## Comparison

| Feature          | Heartbeat           | Cron (main)         | Cron (isolated)       |
|------------------|--------------------|---------------------|-----------------------|
| Session          | Main               | Main (via event)    | Fresh `cron:<jobId>`  |
| Context          | Full               | Full                | None                  |
| Timing           | Approximate (~N min)| Immediate on wake  | Exact                 |
| Model override   | No                 | No (uses main)      | Yes                   |
| Output           | Delivered if not HEARTBEAT_OK | Heartbeat prompt | Announce summary (default) |
| History          | Shared             | Shared              | Isolated              |

## Configuring Heartbeat

```
agents.defaults.heartbeat: {
  every: "30m",
  target: "last",
  activeHours: { start: "08:00", end: "22:00" }
}
```

Add tasks in `HEARTBEAT.md`. Keep it small to minimize tokens.

## Combining both

Typical efficient setup:

- Heartbeat (every 30 min): checks inbox, calendar, notifications in one turn.
- Cron (isolated):
  - Daily morning briefing at exactly 7 AM
  - Weekly deep analysis with Opus
  - One-shot reminders

---
Source: https://docs.openclaw.ai/automation/cron-vs-heartbeat
