#!/bin/bash
# Deploy EcopunkVideoCollage source to Raspberry Pi and build ExperienceRuntime.
# Configuration: scripts/deploy.local.env
#
# Usage:
#   scripts/deploy.sh --dry-run
#   scripts/deploy.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

DRY_RUN=0
case "${1:-}" in
    --dry-run) DRY_RUN=1 ;;
    "") ;;
    *) echo "Usage: $0 [--dry-run]" >&2; exit 1 ;;
esac

ENV_FILE="$SCRIPT_DIR/deploy.local.env"
if [[ ! -f "$ENV_FILE" ]]; then
    echo "Missing $ENV_FILE" >&2
    exit 1
fi

# shellcheck disable=SC1090
source "$ENV_FILE"

: "${REMOTE_USER:?REMOTE_USER not set}"
: "${REMOTE_HOST:?REMOTE_HOST not set}"
: "${REMOTE_PATH:?REMOTE_PATH not set}"
: "${MAKE_JOBS:=2}"

REMOTE="$REMOTE_USER@$REMOTE_HOST"
BUILD_DIR="$REMOTE_PATH/sketches/experience_runtime"

RSYNC_CMD=(
    rsync -avz --no-times
    --exclude 'bin/'
    --exclude 'obj/'
    --exclude '.git/'
    --exclude '.claude/'
    --exclude 'deploy.local.env'
    "$REPO_ROOT/"
    "$REMOTE:$REMOTE_PATH/"
)

SSH_BUILD_CMD=(
    ssh "$REMOTE"
    "cd '$BUILD_DIR' && make Release -j$MAKE_JOBS"
)

if [[ "$DRY_RUN" == "1" ]]; then
    echo "Source: $REPO_ROOT"
    echo "Target: $REMOTE:$REMOTE_PATH"
    echo "Build directory: $BUILD_DIR"
    echo
    echo "Would run:"
    printf ' %q' "${RSYNC_CMD[@]}"
    echo
    printf ' %q' "${SSH_BUILD_CMD[@]}"
    echo
    exit 0
fi

echo "Syncing EcopunkVideoCollage..."
"${RSYNC_CMD[@]}"

echo "Building ExperienceRuntime on Raspberry Pi..."
"${SSH_BUILD_CMD[@]}"

echo "Deployment complete."
