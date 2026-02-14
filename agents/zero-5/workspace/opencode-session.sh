#!/bin/bash
#
# opencode-session.sh - Spawn OpenCode sessions on LAN-accessible servers
#
# Usage: opencode-session.sh <repo-name-or-url> [options]
#
# Options:
#   --port PORT        Port to use (default: find free port starting at 4096)
#   --title TITLE      Session title (default: repo name)
#   --message MSG      Session message (default: "Session for <repo>")
#   --dir DIR          Project directory (default: repos/<repo-name>)
#   --host HOST        Hostname to bind to (default: 0.0.0.0 for LAN)
#
# Examples:
#   opencode-session.sh zero-re
#   opencode-session.sh https://github.com/user/repo --port 8080 --title "My Project"
#

set -euo pipefail

# Configuration
REPOS_DIR="${HOME}/.openclaw/agents/archy/workspace/repos"
DEFAULT_PORT=4096
DEFAULT_HOST="0.0.0.0"
SERVER_CHECK_TIMEOUT=5
SESSION_TIMEOUT=45

# Parse arguments
REPO=""
PORT=""
TITLE=""
MESSAGE=""
PROJECT_DIR=""
HOST="${DEFAULT_HOST}"

while [[ $# -gt 0 ]]; do
    case $1 in
        --port)
            PORT="$2"
            shift 2
            ;;
        --title)
            TITLE="$2"
            shift 2
            ;;
        --message)
            MESSAGE="$2"
            shift 2
            ;;
        --dir)
            PROJECT_DIR="$2"
            shift 2
            ;;
        --host)
            HOST="$2"
            shift 2
            ;;
        -*)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
        *)
            REPO="$1"
            shift
            ;;
    esac
done

if [[ -z "${REPO}" ]]; then
    echo "Usage: $0 <repo-name-or-url> [options]" >&2
    exit 1
fi

# Determine repo name from URL or directory name
if [[ "${REPO}" == http* ]]; then
    REPO_NAME=$(basename "${REPO}" .git)
else
    REPO_NAME="${REPO}"
fi

# Set default values
PROJECT_DIR="${PROJECT_DIR:-${REPOS_DIR}/${REPO_NAME}}"
TITLE="${TITLE:-${REPO_NAME}}"
MESSAGE="${MESSAGE:-Session for ${REPO_NAME}}"

echo "==> OpenCode Session Manager"
echo "    Repository: ${REPO}"
echo "    Project dir: ${PROJECT_DIR}"
echo "    Title: ${TITLE}"
echo "    Host: ${HOST}"

# Step 1: Ensure repository exists
if [[ ! -d "${PROJECT_DIR}" ]]; then
    echo "==> Cloning repository..."
    if [[ "${REPO}" == http* ]]; then
        git clone "${REPO}" "${PROJECT_DIR}"
    else
        # Try to find in repos directory
        if [[ -d "${REPOS_DIR}/${REPO_NAME}" ]]; then
            PROJECT_DIR="${REPOS_DIR}/${REPO_NAME}"
        else
            echo "Error: Repository '${REPO_NAME}' not found in ${REPOS_DIR}" >&2
            echo "       Provide a full URL or ensure the repo exists." >&2
            exit 1
        fi
    fi
fi

# Step 2: Find or start OpenCode server
check_server() {
    local port="$1"
    if ss -tuln 2>/dev/null | grep -qE "LISTEN.*:${port}\b"; then
        if curl -s "http://localhost:${port}/health" 2>/dev/null | grep -q "OpenCode"; then
            return 0  # server exists and is healthy
        fi
    fi
    return 1  # no server on this port
}

start_server() {
    local port="$1"
    local host="$2"
    local cwd="$3"

    echo "==> Starting OpenCode server on ${host}:${port}..."
    cd "${cwd}"
    nohup opencode serve --port "${port}" --hostname "${host}" > /tmp/opencode-${port}.log 2>&1 &
    local pid=$!
    echo "${pid}"

    # Wait for server to be ready
    local attempts=30
    while ((attempts > 0)); do
        if curl -s "http://localhost:${port}/health" 2>/dev/null | grep -q "OpenCode"; then
            echo "==> Server started (PID ${pid})"
            return 0
        fi
        sleep 1
        ((attempts--))
    done

    echo "Error: Server failed to start within timeout" >&2
    cat "/tmp/opencode-${port}.log" >&2
    return 1
}

# Use configured port (default 4096, or user-specified)
PORT="${PORT:-${DEFAULT_PORT}}"

# Check if server exists on that port, otherwise start it
echo "==> Checking for existing OpenCode server on port ${PORT}..."
if check_server "${PORT}"; then
    echo "    Found server on port ${PORT}"
else
    echo "    No server found on port ${PORT}, starting new server..."
    start_server "${PORT}" "${HOST}" "${PROJECT_DIR}"
fi

# Step 3: Create the session
echo "==> Creating session..."
# Use 'script' to allocate a pseudo-tty for opencode, which expects interactive terminal
SESSION_JSON=$(cd "${PROJECT_DIR}" && timeout "${SESSION_TIMEOUT}" script -q -c "opencode run \
    --attach \"http://localhost:${PORT}\" \
    --title \"${TITLE}\" \
    --format json \
    \"${MESSAGE}\"" /dev/null 2>&1 || true)

# Debug: log raw output (to stderr)
echo "DEBUG: Raw output length: ${#SESSION_JSON}" >&2
if [[ ${#SESSION_JSON} -lt 100 ]]; then
    echo "DEBUG: Raw output: ${SESSION_JSON}" >&2
else
    echo "DEBUG: First 200 chars: ${SESSION_JSON:0:200}" >&2
fi

# Extract session ID (first occurrence only)
SESSION_ID=$(echo "${SESSION_JSON}" | grep -o '"sessionID":"[^"]*"' | head -1 | cut -d'"' -f4 || true)

if [[ -z "${SESSION_ID}" ]]; then
    echo "Error: Failed to create session" >&2
    echo "Output: ${SESSION_JSON}" >&2
    exit 1
fi

echo "==> Session created successfully!"
echo "    Session ID: ${SESSION_ID}"
echo "    Server: http://${HOST}:${PORT}"
echo "    Project: ${PROJECT_DIR}"

# Update sessions.json
SESSIONS_JSON="${REPOS_DIR}/../sessions.json"
if [[ -f "${SESSIONS_JSON}" ]]; then
    # Read existing sessions
    if command -v jq &>/dev/null; then
        # Use jq to add/replace session
        jq --arg sid "${SESSION_ID}" --arg proj "${REPO_NAME}" --arg title "${TITLE}" --arg port "${PORT}" \
           --arg created "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
           'map(select(.sessionId != $sid)) + [{"sessionId":$sid,"project":$proj,"title":$title,"created":$created,"port":$port|tonumber,"status":"active"}]' \
           "${SESSIONS_JSON}" > "${SESSIONS_JSON}.tmp" && mv "${SESSIONS_JSON}.tmp" "${SESSIONS_JSON}"
    else
        # Fallback: simple append (may duplicate)
        echo "{\"sessionId\":\"${SESSION_ID}\",\"project\":\"${REPO_NAME}\",\"title\":\"${TITLE}\",\"created\":\"$(date -u +%Y-%m-%dT%H:%M:%SZ)\",\"port\":${PORT},\"status\":\"active\"}" >> "${SESSIONS_JSON}"
    fi
fi

# Return just the session ID for scripting
echo "${SESSION_ID}"
