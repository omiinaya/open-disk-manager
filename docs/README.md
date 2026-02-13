# OpenClaw Documentation Archive

This directory contains archived OpenClaw documentation for offline reference.

## Structure

- `openclaw/` — Main documentation mirror from https://docs.openclaw.ai
  - [INDEX.md](openclaw/INDEX.md) — Catalog of all stored documentation
  - Getting started, configuration reference, concepts, security, etc.
- `training-loop-design.md` — Self-reflection learning system design doc
- `group-chat-guide.md` — Group chat interaction guidelines
- `heartbeat-guide.md` — Heartbeat check procedures

## Sources

Official docs: https://docs.openclaw.ai
Full index: https://docs.openclaw.ai/llms.txt

## Purpose

- Offline access to OpenClaw documentation
- Historical reference for configuration and architecture decisions
- Training material for agents working with OpenClaw

Last updated: 2026-02-13

## Adding New Docs

To update the archive:

1. Fetch the desired page from https://docs.openclaw.ai
2. Save it to `docs/openclaw/` preserving the path structure
3. Update the `INDEX.md` if adding new categories
4. Commit changes with `openclaw save config`

See the skill: `skills/config-save` for the save procedure.