# OpenClaw Models Config

## Adding New Model Providers

**Key insight:** `/models` command shows models from `agents.defaults.models` allowlist, NOT from `models.providers`.

Two places need updating:
1. **`models.providers`** - provider config (baseUrl, apiKey, api type, models list)
2. **`agents.defaults.models`** - model allowlist (keys are `provider/model` like `github/openai/gpt-5`)

Example entry in `agents.defaults.models`:
```json
"github/openai/gpt-5": {
  "alias": "GPT-5 (GitHub)"
}
```

## Known Providers
- nvidia (integrate.api.nvidia.com)
- opencode (opencode.ai/zen)
- openrouter (openrouter.ai)
- github (models.inference.ai.azure.com) - uses `api: "github-copilot"`
