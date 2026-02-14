# SearXNG Integration

## Primary Web Search Method

**SearXNG is configured as the primary web search method.**

- **Endpoint**: `http://localhost:8888/search?q=<query>&format=json`
- **Location**: `/opt/searxng-docker/`
- **Port**: 8888

### Usage

Query SearXNG via curl:
```bash
curl -s "http://localhost:8888/search?q=URL_ENCODED_QUERY&format=json" | jq '.results'
```

### Fallback

If SearXNG is unavailable, fall back to the `web_search` tool (Brave Search API).

### Management Commands

```bash
# Check status
sudo docker ps | grep searxng

# View logs
sudo docker logs searxng

# Restart
cd /opt/searxng-docker && sudo docker compose restart

# Stop
cd /opt/searxng-docker && sudo docker compose down

# Start
cd /opt/searxng-docker && sudo docker compose up -d
```

### Configuration

- Settings: `/opt/searxng-docker/searxng/settings.yml`
- JSON API enabled
- Limiter disabled (private instance)
- Multiple search engines: Google, Bing, DuckDuckGo, Brave, Startpage, Wikipedia, etc.

## Installed: 2026-02-14

---

# QMD - Local Markdown Knowledge Base Search

**QMD is configured for searching the local markdown knowledge base.**

- **Location**: `/opt/qmd/`
- **Index**: `~/.cache/qmd/index.sqlite`
- **Documents**: 224 markdown files indexed
- **Vectors**: 263 chunks embedded

### What It Does

QMD provides **95% token savings** by indexing markdown files locally and only sending relevant chunks to the LLM, instead of entire documents.

### Search Methods

| Command | Description | Use Case |
|---------|-------------|----------|
| `qmd search "query"` | BM25 keyword search | Fast, exact matches |
| `qmd vsearch "query"` | Vector semantic search | Meaning-based search |
| `qmd query "query"` | Hybrid + reranking | Best quality results |

### Collections

| Collection | Files | Context |
|------------|-------|---------|
| `workspace` | 19 | Workspace files and project code |
| `docs` | 19 | Documentation files |
| `memory-dir` | 14 | Memory and knowledge base |
| `openclaw` | 171 | OpenClaw agent system |
| `memory-root` | 1 | Root MEMORY.md |

### GGUF Models (auto-downloaded)

| Model | Purpose | Size |
|-------|---------|------|
| `embeddinggemma-300M-Q8_0` | Vector embeddings | 314MB |
| `qmd-query-expansion-1.7B-q4_k_m` | Query expansion | 1.2GB |

### Usage Examples

```bash
# Keyword search
qmd search "docker" -n 5

# Semantic search
qmd vsearch "how to deploy applications" -n 5

# Hybrid search (best quality)
qmd query "authentication flow" -n 5

# Get full document
qmd get "path/to/file.md" --full

# JSON output for agents
qmd query "API" --json -n 10

# Search within collection
qmd search "config" -c openclaw
```

### Management Commands

```bash
# Status
qmd status

# Update index
qmd update

# Regenerate embeddings
qmd embed

# Add new collection
qmd collection add ~/path/to/markdown --name mydocs
```

### Integration with Memory

When searching for prior work, decisions, or knowledge:
1. Use `qmd query "search terms"` for best results
2. Use `qmd get "filepath"` to retrieve full documents
3. This avoids feeding entire docs to LLM, saving ~95% tokens

## Installed: 2026-02-14
