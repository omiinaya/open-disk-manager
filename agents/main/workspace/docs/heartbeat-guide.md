# Heartbeat Guide

When you receive a heartbeat poll, use it productively. This guide covers when and how to use heartbeats vs cron jobs, what to check, and best practices.

## Heartbeat vs Cron: When to Use Each

**Use heartbeat when:**
- Multiple checks can batch together (inbox + calendar + notifications in one turn)
- You need conversational context from recent messages
- Timing can drift slightly (every ~30 min is fine, not exact)
- You want to reduce API calls by combining periodic checks

**Use cron when:**
- Exact timing matters ("9:00 AM sharp every Monday")
- Task needs isolation from main session history
- You want a different model or thinking level for the task
- One-shot reminders ("remind me in 20 minutes")
- Output should deliver directly to a channel without main session involvement

**Tip:** Batch similar periodic checks into `HEARTBEAT.md` instead of creating multiple cron jobs. Use cron for precise schedules and standalone tasks.

## Suggested Checks (rotate 2-4 times per day)

- **Emails** - Any urgent unread messages?
- **Calendar** - Upcoming events in next 24-48h?
- **Mentions** - Twitter/social notifications?
- **Weather** - Relevant if your human might go out?

## Track Your Checks

Store timestamps in `memory/heartbeat-state.json` to avoid repeating work:

```json
{
  "lastChecks": {
    "email": 1703275200,
    "calendar": 1703260800,
    "weather": null
  }
}
```

## When to Reach Out

- Important email arrived
- Calendar event coming up in <2 hours
- Something interesting you discovered
- It's been >8 hours since you last messaged

## When to Stay Quiet (HEARTBEAT_OK)

- Late night (23:00-08:00) unless urgent
- Human is clearly busy
- Nothing new since last check
- You just checked <30 minutes ago

## Proactive Work (No Need to Ask)

- Read and organize memory files
- Check on projects (git status, etc.)
- Update documentation
- Commit and push your own changes
- Review and update `MEMORY.md` (see below)

## Memory Maintenance During Heartbeats

Every few days:

1. Read recent `memory/YYYY-MM-DD.md` files
2. Identify significant events, lessons, or insights worth keeping long-term
3. Update `MEMORY.md` with distilled learnings
4. Remove outdated info from `MEMORY.md` that's no longer relevant

Think of it like a human reviewing their journal and updating their mental model. Daily files are raw notes; `MEMORY.md` is curated wisdom.

**Goal:** Be helpful without being annoying. Check in a few times a day, do useful background work, but respect quiet time.
