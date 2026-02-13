#!/usr/bin/env python3
import json
import requests
import sys

config_path = "/root/.openclaw/workspace/agents/ciel/models.json"
with open(config_path) as f:
    config = json.load(f)

provider = config["providers"]["nvidia"]
base_url = provider["baseUrl"].rstrip("/")
api_key = provider["apiKey"]
auth_type = provider.get("auth", "api-key")
models = provider["models"]

# Find the specific model
model_id = "stepfun-ai/step-3.5-flash"
model_info = None
for m in models:
    if m["id"] == model_id:
        model_info = m
        break

if not model_info:
    print(f"Model {model_id} not found in NVIDIA provider config")
    sys.exit(1)

endpoint = f"{base_url}/chat/completions"
headers = {"Content-Type": "application/json"}
if auth_type == "api-key":
    headers["Authorization"] = f"Bearer {api_key}"
else:
    headers["Authorization"] = f"{auth_type} {api_key}"

payload = {
    "model": model_id,
    "messages": [{"role": "user", "content": "Hello, are you working?"}],
    "max_tokens": 256,
    "stream": False
}

try:
    resp = requests.post(endpoint, json=payload, headers=headers, timeout=30)
    if resp.status_code == 200:
        data = resp.json()
        # Extract the assistant message content
        if "choices" in data and len(data["choices"]) > 0:
            message = data["choices"][0].get("message", {})
            content = message.get("content", "")
            print(f"Model responded successfully:\n{content}")
        else:
            print("No choices in response:", data)
    else:
        print(f"Error: HTTP {resp.status_code}")
        print(resp.text)
except Exception as e:
    print(f"Exception: {e}")
