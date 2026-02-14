#!/bin/bash
# Workspace Save Skill (patched)
# Commits and pushes all changes in /root/.openclaw

set -e

WORKSPACE="/root/.openclaw"
cd "$WORKSPACE"

echo "Checking git status..."
git status --short

# Check if there are any changes to commit
if git diff-index --quiet HEAD --; then
    echo "No changes to commit. Workspace is up to date."
    exit 0
fi

echo "Staging all changes..."
git add -A

# Use provided commit message or default
COMMIT_MSG="${1:-Save configuration}"
echo "Committing with message: $COMMIT_MSG"
git commit -m "$COMMIT_MSG"

echo "Pushing to origin..."
git push origin "$(git rev-parse --abbrev-ref HEAD)"

echo "✓ Workspace saved and pushed successfully."