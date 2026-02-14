---
name: opencode-session
description: Spawn and manage OpenCode sessions on the LAN server. Extracts the opencode-session.sh logic into a reusable skill.
---

# OpenCode Session Skill

Use this skill to manage OpenCode server sessions: check status, start server, clone repos, create sessions.

## When to Use

- User asks to create a new OpenCode session
- Need to check if OpenCode server is running
- Need to clone a repo before creating a session

## Prerequisites

- OpenCode installed: `which opencode`
- Port 4096 available
- Network accessible (0.0.0.0 for LAN)

## Procedures

### 1. Check Server Status

```bash
check_server() {
    # Check pid file
    if [ -f "server.pid" ] && kill -0 $(cat server.pid) 2>/dev/null; then
        echo "running"
        return 0
    fi
    
    # Check port
    if ss -tln | grep -q ':4096 '; then
        local pid=$(ss -tlnp | awk '/:4096 /{gsub(/[^0-9]/,"",$6); print $6}')
        if [ -n "$pid" ] && ps -p "$pid" -o comm= | grep -q opencode; then
            echo "running"
            return 0
        fi
    fi
    
    echo "stopped"
    return 1
}
```

### 2. Start Server

```bash
start_server() {
    mkdir -p logs
    
    # Check if already running
    if [ "$(check_server)" = "running" ]; then
        echo "Server already running"
        return 0
    fi
    
    nohup opencode serve --host 0.0.0.0 --port 4096 > logs/serve.log 2>&1 &
    local pid=$!
    echo $pid > server.pid
    sleep 2
    
    # Verify
    if kill -0 $pid 2>/dev/null && curl -s http://localhost:4096 > /dev/null; then
        echo "Server started (PID: $pid) on port 4096"
        return 0
    else
        echo "Server failed to start"
        return 1
    fi
}
```

### 3. Clone Repository

```bash
clone_repo() {
    local url=$1
    local name=$(basename "$url" .git)
    local target="repos/$name"
    
    if [ -d "$target" ]; then
        if [ -d "$target/.git" ]; then
            echo "Repo already exists: $target"
            return 0
        else
            echo "Path exists but not git repo: $target"
            return 1
        fi
    fi
    
    git clone "$url" "$target"
    echo "Cloned $url → $target"
}
```

### 4. Create Session

```bash
create_session() {
    local project_path=$1
    local project_name=$(basename "$project_path")
    local port=${2:-4096}
    
    # Ensure server running
    [ "$(check_server)" = "stopped" ] && start_server
    
    # Create session
    cd "$project_path"
    timeout 30s bash -c "
        opencode run \
            --attach http://localhost:$port \
            --title '$project_name' \
            --message 'Session for $project_name' \
            --command 'pwd' \
            --format json \
            --print-logs
    " 2>&1 | tee /tmp/session_output.log
    
    # Extract session ID
    local session_id=$(grep -oP 'session id=\K[a-zA-Z0-9_]+' /tmp/session_output.log | head -1)
    
    if [ -n "$session_id" ]; then
        # Update sessions.json
        local now=$(date -u +%Y-%m-%dT%H:%M:%SZ)
        local entry="{\"sessionId\":\"$session_id\",\"projectPath\":\"$project_path\",\"projectName\":\"$project_name\",\"port\":$port,\"created\":\"$now\",\"status\":\"active\"}"
        
        if [ -f sessions.json ]; then
            # Append to existing
            local tmp=$(mktemp)
            jq ". += [$entry]" sessions.json > "$tmp" && mv "$tmp" sessions.json
        else
            echo "[$entry]" > sessions.json
        fi
        
        echo "Created session $session_id for $project_name (port $port)"
    else
        echo "Failed to create session"
        return 1
    fi
}
```

## Quick Reference

| Action | Command |
|--------|---------|
| Check server | `check_server` |
| Start server | `start_server` |
| Clone repo | `clone_repo <url>` |
| Create session | `create_session <path> [port]` |
| List sessions | `jq '.[] | "\(.sessionId) \(.projectName)"' sessions.json` |

## Notes

- Server always on port 4096
- sessions.json must be updated after each session
- Use `opencode run --attach` to connect to existing server
- Session ID appears in logs as `session id=<id>`
