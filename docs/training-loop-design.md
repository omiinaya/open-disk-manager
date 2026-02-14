# Training Loop Design for OpenClaw Agents

## 1. Goal
Enable any OpenClaw agent to self-reflect after completing a user task, propose targeted SOUL improvements, and receive user approval before updating its behavior document. The loop is supervised to prevent drift and maintain alignment.

## 2. Activation Toggle (Implementation Note)

**Do not add custom fields to `openclaw.json`** — it has a strict schema and will fail validation.

Instead, use `~/.openclaw/config-extended.json` (tracked in git, no secrets):

```json
{
  "agents": {
    "<agentId>": {
      "trainingMode": true
    }
  }
}
```

Agents must read this file at the start of each turn and check `agents[agentId].trainingMode`. If present and true, enter training mode for that turn.

See `MEMORY.md` for the full rationale on `config-extended.json`.

## 3. Reflection Protocol (After Task Completion)
If `trainingMode` is true, the agent must generate a reflection and send it to the user.

### 3.1 Metrics
- `success`: true/false – did the agent fulfill the user request?
- `durationSeconds`: wall‑clock time from task start to final output.
- `guidelineCompliance`: 0.0–1.0 – fraction of SOUL rules followed (checklist).
- `alignmentScore`: weighted sum (success: 0.4, time: 0.3, compliance: 0.3).

### 3.2 Reflection Output
The agent sends a concise text summary followed by a yes/no question:

```
[Training Reflection]
Agent: <agentId>
Task: <short user request>
Metrics:
  success: true
  durationSeconds: 42
  guidelineCompliance: 0.92
Alignment score: 0.88 (weighted: success 0.4, time 0.3, compliance 0.3)
Lessons:
  - <bullet 1>
  - <bullet 2>
Suggested SOUL changes:
  - Section: "<section name>"
    Original: "<exact snippet>"
    Proposed: "<new snippet>"
    Reason: "<why>"
Approve SOUL update? (yes/no)
```

### 3.3 Proposed SOUL Changes
- Identify exact section of `SOUL.md`.
- Provide original and proposed snippets.
- Changes must be incremental; preserve everything else.

## 4. Approval & Application
User replies:
- `yes` → agent backs up `SOUL.md` to `SOUL.md.bak-<timestamp>`, applies changes, optionally commits, and informs user.
- `no` → agent logs rejection and does nothing.

Agent must wait for reply (timeout ≈60 s). If no reply, no update.

## 5. Logging
All reflections saved to:
```
docs/competencies/<agentId>/<YYYY-MM-DD>HHMM-<summary>.json
```
JSON fields:
- `timestamp`
- `agentId`
- `task`
- `metrics` (success, durationSeconds, guidelineCompliance)
- `alignmentScore`
- `previousMedianAlignment` (if available)
- `lessons`
- `suggestedSoulChanges`
- `approval`: "approved"/"rejected"/"timeout"
- `timestampApplied` (if approved)
- `agentVersion`: SOUL file hash or mtime (optional)

## 6. Directory Structure
```
~/.openclaw/agents/<agent>/workspace/
├── SOUL.md
├── SOUL.md.bak-*
└── docs/
    └── competencies/
        └── <agent>/
            ├── 2026-02-13T18-30-00-reflection.json
            └── updates.log
```

## 7. Implementation Steps (Per Agent)
1. Add `trainingMode` to agent config.
2. Add a “Reflection” section to the agent’s SOUL describing the process.
3. Implement reflection generation at the end of the reasoning loop.
4. Compute metrics, compare with historical median (last N=5), propose changes only if alignment improved.
5. Send reflection to user and await approval.
6. On approval, apply changes; on rejection, log and stop.

## 8. Safeguards
- No autonomous updates – require explicit user “yes”.
- Always backup before write.
- Do not modify the “identity” paragraph (“You are …”).
- Rate limiting: one reflection per task.

## 9. Open Questions
- Should we use a dedicated training channel to keep reflections separate?
- Normalization of time metric across tasks?
- Should the agent also propose changes to `AGENTS.md` (tool allowances)?

---
```

Saved: `~/.openclaw/docs/training-loop-design.md`.