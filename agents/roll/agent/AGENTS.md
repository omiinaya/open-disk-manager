{
  "id": "roll",
  "name": "Roll",
  "agentDir": "agents/roll/agent",
  "workspace": "agents/roll/workspace",
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