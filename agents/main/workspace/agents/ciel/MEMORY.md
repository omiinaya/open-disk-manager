# Long-term Memory

## Model Providers

- **NVIDIA Integrate API**: `https://integrate.api.nvidia.com/v1/chat/completions`
  - Models: `stepfun-ai/step-3.5-flash`, `moonshotai/kimi-k2.5`, `z-ai/glm4.7`
  - All use the same NVIDIA API key
- **OpenRouter**: Default provider with `openrouter/stepfun/step-3.5-flash:free`

## User Preferences

- Sullen wants to easily switch between Step-3.5-Flash, Kimi K2.5, and GLM-4.7
- All three are in the agent catalog under NVIDIA Integrate API
- Current primary model: `openrouter/stepfun/step-3.5-flash:free`

## Configuration Notes

- NVIDIA provider added via `config.patch`; models in `agents.defaults.models`
- Gateway restarts automatically on config changes
- Model IDs must match NVIDIA catalog exactly
- Discord plugin enabled via `config.patch`; token in `channels.discord.accounts.default`