{
  "id": "roci",
  "name": "Roci",
  "agentDir": "agents/roci/agent",
  "workspace": "agents/roci/workspace",
  "model": {
    "primary": "nvidia/stepfun-ai/step-3.5-flash"
  },
  "subagents": {
    "allowAgents": []
  },
  "tools": {
    "allow": ["message"]
  },
  "session": {
    "scope": "global"
  }
}