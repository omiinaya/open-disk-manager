{
  "id": "harpy",
  "name": "Harpy",
  "agentDir": "agents/harpy/agent",
  "workspace": "agents/harpy/workspace",
  "model": {
    "primary": "nvidia/stepfun-ai/step-3.5-flash"
  },
  "subagents": {
    "allowAgents": []
  },
  "tools": {
    "allow": ["exec", "gateway", "cron", "message"]
  },
  "session": {
    "scope": "global"
  }
}
