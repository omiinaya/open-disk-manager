---
name: nvidia-model-config
description: Configure NVIDIA Integrate API models for OpenClaw with correct context window sizes. Use when adding or updating NVIDIA models (stepfun-ai/step-3.5-flash, moonshotai/kimi-k2.5, z-ai/glm4.7, etc.) to ensure proper contextWindow and maxTokens values. Prevents the 'Model context window too small' errors by using actual model specs instead of inferring from maxTokens.
---

# NVIDIA Model Configuration

Configure NVIDIA Integrate API models correctly for OpenClaw.

## Common Error

The most common mistake is setting `contextWindow` to the same value as `maxTokens`:

```json
// WRONG - causes "context window too small" errors
{
  "contextWindow": 8192,  // This is just the output limit!
  "maxTokens": 8192
}
```

## Correct Configuration Pattern

```json
{
  "id": "stepfun-ai/step-3.5-flash",
  "name": "Step-3.5-Flash",
  "reasoning": false,
  "input": ["text"],
  "cost": { "input": 0, "output": 0, "cacheRead": 0, "cacheWrite": 0 },
  "contextWindow": 256000,  // Actual context window from model specs
  "maxTokens": 8192          // Output token limit (separate!)
}
```

## Key Distinction

- **contextWindow**: Total tokens the model can process (input + output + system prompt + tools)
- **maxTokens**: Maximum tokens the model will generate in a single response

These are completely separate values. Never set them equal.

## NVIDIA Integrate API Models

### Step-3.5-Flash
```json
{
  "id": "stepfun-ai/step-3.5-flash",
  "name": "Step-3.5-Flash",
  "reasoning": false,
  "input": ["text"],
  "cost": { "input": 0, "output": 0, "cacheRead": 0, "cacheWrite": 0 },
  "contextWindow": 256000,
  "maxTokens": 8192
}
```

### Kimi K2.5
```json
{
  "id": "moonshotai/kimi-k2.5",
  "name": "Kimi K2.5",
  "reasoning": true,
  "input": ["text", "image"],
  "cost": { "input": 0, "output": 0, "cacheRead": 0, "cacheWrite": 0 },
  "contextWindow": 262000,
  "maxTokens": 8192
}
```

### GLM-4.7
```json
{
  "id": "z-ai/glm4.7",
  "name": "GLM-4.7",
  "reasoning": false,
  "input": ["text"],
  "cost": { "input": 0, "output": 0, "cacheRead": 0, "cacheWrite": 0 },
  "contextWindow": 203000,
  "maxTokens": 8192
}
```

## Adding a New NVIDIA Model

1. **Find the actual context window**:
   - Check OpenRouter: https://openrouter.ai/models
   - Check NVIDIA catalog: https://integrate.api.nvidia.com/v1/models
   - Check model documentation (NOT just max_tokens in the API response)

2. **Run config.patch** with the new model:

```bash
# Example: Adding a hypothetical new model
openclaw config patch --raw '{
  "models": {
    "providers": {
      "nvidia": {
        "models": [
          ...existing models...,
          {
            "id": "vendor/model-name",
            "name": "Display Name",
            "reasoning": false,
            "input": ["text"],
            "cost": { "input": 0, "output": 0, "cacheRead": 0, "cacheWrite": 0 },
            "contextWindow": 128000,  // <--- THE IMPORTANT PART
            "maxTokens": 8192
          }
        ]
      }
    }
  }
}'
```

3. **Add to agents.defaults.models**:

```json
{
  "agents": {
    "defaults": {
      "models": {
        "nvidia/vendor/model-name": {
          "alias": "Display Name (NVIDIA)"
        }
      }
    }
  }
}
```

## Debugging Context Window Errors

If you see:
```
blocked model (context window too small): nvidia/stepfun-ai/step-3.5-flash ctx=8192 (min=16000)
```

The `ctx=8192` value comes from the model's `contextWindow` config. Fix by updating to the actual context window size (check OpenRouter or NVIDIA docs - it's usually much larger than 8K).

## Useful Resources

- OpenRouter specs: Check https://openrouter.ai/models for accurate context lengths
- OpenClaw requires minimum 16K context for primary models