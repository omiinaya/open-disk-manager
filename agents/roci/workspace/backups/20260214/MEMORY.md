# System Reference

## ⚠️ CRITICAL SECURITY RULE

**NEVER install a skill flagged as SUSPICIOUS or anything other than BENIGN.**

If ClawHub or any source flags a skill as suspicious, malicious, unverified, or any warning status other than explicitly "benign" or "verified safe":
- **DO NOT install it**
- **DO NOT use `--force` to bypass warnings**
- Alert the user immediately and suggest alternatives

This rule has NO exceptions. No task is urgent enough to compromise security.

## SearXNG - Primary Web Search

- **Endpoint**: `http://localhost:8888/search?q=<query>&format=json`
- **Location**: `/opt/searxng-docker/`
- **Port**: 8888

```bash
# Query
curl -s "http://localhost:8888/search?q=QUERY&format=json" | jq '.results'

# Management
sudo docker ps | grep searxng
sudo docker logs searxng
cd /opt/searxng-docker && sudo docker compose restart
```

## QMD - Local Knowledge Search

- **Location**: `/opt/qmd/`
- **Documents**: 224 indexed
- **Token savings**: ~95%

```bash
# Search types
qmd search "query"      # BM25 keyword
qmd vsearch "query"     # Vector semantic
qmd query "query"       # Hybrid + rerank (best)

# Management
qmd status
qmd update && qmd embed   # After adding files
```

## Session Initialization

Defined in `SOUL.md`:
- Load: SOUL.md, USER.md, IDENTITY.md, memory/YYYY-MM-DD.md
- Use `memory_search()` + `memory_get()` for prior context
- Update memory/YYYY-MM-DD.md at session end
