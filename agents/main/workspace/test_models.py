#!/usr/bin/env python3
import json
import requests
import sys

config_path = "/root/.openclaw/workspace/agents/ciel/models.json"
with open(config_path) as f:
    config = json.load(f)

providers = config.get("providers", {})
results = {}

for provider_name, provider in providers.items():
    base_url = provider.get("baseUrl", "").rstrip("/")
    api_key = provider.get("apiKey", "")
    auth_type = provider.get("auth", "api-key")
    models = provider.get("models", [])
    results[provider_name] = {}

    for model in models:
        model_id = model["id"]
        model_name = model.get("name", model_id)
        endpoint = f"{base_url}/chat/completions"
        headers = {"Content-Type": "application/json"}
        if auth_type == "api-key":
            headers["Authorization"] = f"Bearer {api_key}"
        else:
            headers["Authorization"] = f"{auth_type} {api_key}"

        payload = {
            "model": model_id,
            "messages": [{"role": "user", "content": "hi"}],
            "max_tokens": 5,
            "stream": False
        }

        try:
            resp = requests.post(endpoint, json=payload, headers=headers, timeout=10)
            if resp.status_code == 200:
                results[provider_name][model_id] = {"status": "ok", "code": resp.status_code}
            else:
                results[provider_name][model_id] = {"status": "error", "code": resp.status_code, "detail": resp.text[:200]}
        except Exception as e:
            results[provider_name][model_id] = {"status": "exception", "error": str(e)[:200]}

# Print summary
print("\n=== Model Connectivity Test ===")
for provider, models in results.items():
    print(f"\nProvider: {provider}")
    for model_id, info in models.items():
        status = info["status"]
        if status == "ok":
            print(f"  ✓ {model_id} — Working (HTTP {info['code']})")
        else:
            print(f"  ✗ {model_id} — {status.upper()}")
            if status == "error":
                print(f"     HTTP {info['code']}, detail: {info.get('detail','')}")
            elif status == "exception":
                print(f"     Error: {info.get('error','')}")
