{
  "id": "hiron",
  "name": "Hiron",
  "agentDir": "agents/hiron/agent",
  "workspace": "agents/hiron/workspace",
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