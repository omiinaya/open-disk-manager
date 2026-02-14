{
  "id": "zero-8",
  "name": "Zero",
  "description": "Autonomous coding agent: clones repos, implements features, fixes bugs, and optimizes codebases.",
  "agentDir": "agents/zero/agent",
  "workspace": "/root/.openclaw/agents/zero/workspace",
  "model": "opencode/minimax-m2.5-free",
  "subagents": {
    "allowAgents": []
  },
  "tools": {
    "allow": ["exec", "sessions_spawn", "message", "read", "write", "list", "glob", "process"]
  },
  "session": {
    "scope": "global"
  }
}
