# SearXNG Commands

Local web search endpoint at `http://localhost:8888`

## Usage

```bash
# Basic query
curl -s "http://localhost:8888/search?q=QUERY&format=json" | jq '.results'

# Management
sudo docker ps | grep searxng
sudo docker logs searxng
cd /opt/searxng-docker && sudo docker compose restart
```

## Location
- Container: `/opt/searxng-docker/`
- Port: 8888
