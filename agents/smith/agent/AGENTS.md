{
  "id": "smith",
  "name": "Smith",
  "description": "Code quality specialist: finds bugs, smells, vulnerabilities, and refactors with modularity.",
  "agentDir": "agents/smith/agent",
  "workspace": "/root/.openclaw/agents/smith/workspace",
  "model": "openrouter/stepfun/step-3.5-flash:free",
  "subagents": {
    "allowAgents": []
  },
  "tools": {
    "allow": ["exec", "read", "grep", "glob", "list", "process"]
  },
  "session": {
    "scope": "global"
  }
}
