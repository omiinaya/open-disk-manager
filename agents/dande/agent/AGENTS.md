{
  "id": "dande",
  "name": "Dande",
  "agentDir": "agents/dande/agent",
  "workspace": "agents/dande/workspace",
  "model": {
    "primary": "nvidia/stepfun-ai/step-3.5-flash"
  },
  "subagents": {
    "allowAgents": []
  },
  "tools": {
    "allow": ["exec"]
  },
  "session": {
    "scope": "global"
  }
}