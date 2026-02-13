{
  "id": "archy",
  "name": "Archie",
  "description": "Manages OpenCode server and spawns Smith agents for code analysis and refactoring.",
  "agentDir": "agents/archy/agent",
  "workspace": "/root/.openclaw/agents/archy/workspace",
  "model": "openrouter/stepfun/step-3.5-flash:free",
  "subagents": {
    "allowAgents": ["smith"]
  },
  "tools": {
    "allow": ["exec", "sessions_spawn", "message", "read", "write", "list", "glob"]
  },
  "session": {
    "scope": "global"
  }
}
