---
name: agent-dashboard
description: Generate a visual dashboard of all agents, their skills, active sessions, cron jobs, and heartbeat activities.
version: 0.1.0
---

# Agent Dashboard

This skill provides a comprehensive overview of your OpenClaw agent ecosystem.

## Usage

Invoke by telling the assistant: **"show agent dashboard"** or **"agent dashboard"**

The assistant will generate a markdown report with:
- Agent hierarchy (who can delegate to whom)
- Skill ownership per agent
- Active sessions and current tasks
- Cron jobs (global and per-agent if applicable)
- Heartbeat configuration per agent

## Output

A structured markdown document with sections:
1. **Agent Overview** — list of all agents, default, models, capabilities
2. **Delegation Hierarchy** — who can spawn whom
3. **Skill Ownership** — which agent owns which skills
4. **Active Sessions** — currently running sessions and their agents
5. **Cron Jobs** — scheduled tasks with targets
6. **Heartbeat Config** — per-agent heartbeat settings

The dashboard helps you understand the current state and spot configuration issues.