#!/bin/bash
# Deploy EcopunkVideoCollage source to Raspberry Pi and build ExperienceRuntime.
# Configuration: scripts/deploy.local.env
#
# Usage:
#   scripts/deploy.sh
#   scripts/deploy.sh --dry-run
#   scripts/deploy.sh --clean
#   scripts/deploy.sh --clean --dry-run
#   scripts/deploy.sh --allow-missing-blob-presets
#
# Options:
#   --clean    Remove stale remote files that no longer exist locally.
#   --dry-run  Preview rsync changes without modifying the Pi or building.
#   --allow-missing-blob-presets
#              Deploy even if some videos have no Blob detection preset
#              (they use detector defaults); skips the confirmation prompt.
#
# Notes:
#   - Normal deployment does not delete remote files.
#   - Cleanup preserves excluded bin/, obj/, and other excluded paths.
#   - Cleanup only affects files under REMOTE_PATH.
#   - Remote build runs only after a successful real sync.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

DRY_RUN=0
CLEAN=0
ALLOW_MISSING_BLOB_PRESETS=0

# ------------------------------------------------------------
# Arguments
# ------------------------------------------------------------

for arg in "$@"; do
    case "$arg" in
        --dry-run)
            DRY_RUN=1
            ;;
        --clean)
            CLEAN=1
            ;;
        --allow-missing-blob-presets)
            ALLOW_MISSING_BLOB_PRESETS=1
            ;;
        -h|--help)
            echo "Usage: $0 [--dry-run] [--clean] [--allow-missing-blob-presets]"
            echo
            echo "  --dry-run  Preview file transfers and deletions."
            echo "  --clean    Remove stale remote files."
            echo "  --allow-missing-blob-presets"
            echo "             Deploy even if some videos lack a Blob detection preset."
            exit 0
            ;;
        *)
            echo "Unknown argument: $arg" >&2
            echo "Usage: $0 [--dry-run] [--clean] [--allow-missing-blob-presets]" >&2
            exit 1
            ;;
    esac
done

# ------------------------------------------------------------
# Configuration
# ------------------------------------------------------------

ENV_FILE="$SCRIPT_DIR/deploy.local.env"

if [[ ! -f "$ENV_FILE" ]]; then
    echo "Missing configuration: $ENV_FILE" >&2
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

# ------------------------------------------------------------
# Safety checks
# ------------------------------------------------------------

if [[ "$REMOTE_PATH" != /* || "$REMOTE_PATH" == "/" ]]; then
    echo "ERROR: REMOTE_PATH must be an absolute path other than /." >&2
    exit 1
fi

# Remove trailing slash for consistent path construction.
REMOTE_PATH="${REMOTE_PATH%/}"
BUILD_DIR="$REMOTE_PATH/sketches/experience_runtime"

# ------------------------------------------------------------
# Predeploy: Blob detection preset coverage
# ------------------------------------------------------------
# Missing presets warn and need explicit confirmation (or the override
# flag); invalid preset data always stops. A dry run reports only.

PRESET_CHECK_ARGS=()
if [[ "$DRY_RUN" == "1" ]]; then
    PRESET_CHECK_ARGS+=(--report-only)
fi
if [[ "$ALLOW_MISSING_BLOB_PRESETS" == "1" ]]; then
    PRESET_CHECK_ARGS+=(--allow-missing-blob-presets)
fi
if ! "$SCRIPT_DIR/check-blob-detection-presets.sh" ${PRESET_CHECK_ARGS[@]+"${PRESET_CHECK_ARGS[@]}"}; then
    echo "Deployment aborted before any file transfer." >&2
    exit 1
fi
echo

# ------------------------------------------------------------
# Rsync
# ------------------------------------------------------------

RSYNC_CMD=(
    rsync
    -avz
    --no-times
    --itemize-changes

    # Preserve remote build outputs and local-only configuration.
    --exclude 'bin/'
    --exclude 'obj/'
    --exclude 'docs/'
    --exclude '.git/'
    --exclude '.claude/'
    --exclude 'deploy.local.env'
)

# Delete stale remote source files only when requested.
#
# By default, rsync protects excluded files/directories from deletion.
# Do not add --delete-excluded unless intentionally removing them.
if [[ "$CLEAN" == "1" ]]; then
    RSYNC_CMD+=(--delete)
fi

# Use rsync's actual dry-run mode to preview changes,
# including files that would be deleted.
if [[ "$DRY_RUN" == "1" ]]; then
    RSYNC_CMD+=(--dry-run)
fi

RSYNC_CMD+=(
    "$REPO_ROOT/"
    "$REMOTE:$REMOTE_PATH/"
)

# ------------------------------------------------------------
# Remote build
# ------------------------------------------------------------

SSH_BUILD_CMD=(
    ssh "$REMOTE"
    "cd '$BUILD_DIR' && make Release -j$MAKE_JOBS"
)

# ------------------------------------------------------------
# Deployment summary
# ------------------------------------------------------------

echo "============================================"
echo " EcopunkVideoCollage Raspberry Pi Deployment"
echo "============================================"
echo
echo "Source:      $REPO_ROOT"
echo "Target:      $REMOTE:$REMOTE_PATH"
echo "Build:       $BUILD_DIR"
echo "Make jobs:   $MAKE_JOBS"
echo "Cleanup:     $([[ "$CLEAN" == "1" ]] && echo enabled || echo disabled)"
echo "Dry run:     $([[ "$DRY_RUN" == "1" ]] && echo enabled || echo disabled)"
echo

# ------------------------------------------------------------
# Synchronize
# ------------------------------------------------------------

if [[ "$DRY_RUN" == "1" ]]; then
    echo "Previewing remote changes..."
elif [[ "$CLEAN" == "1" ]]; then
    echo "Synchronizing and cleaning stale remote source files..."
else
    echo "Synchronizing source files..."
fi

"${RSYNC_CMD[@]}"

# ------------------------------------------------------------
# Dry-run exit
# ------------------------------------------------------------

if [[ "$DRY_RUN" == "1" ]]; then
    echo
    echo "============================================"
    echo " Dry run complete"
    echo "============================================"
    echo
    echo "No files were transferred or deleted."
    echo "No remote build was executed."
    echo
    echo "To apply these changes, run:"
    if [[ "$CLEAN" == "1" ]]; then
        echo "  scripts/deploy.sh --clean"
    else
        echo "  scripts/deploy.sh"
    fi
    exit 0
fi

# ------------------------------------------------------------
# Build ExperienceRuntime
# ------------------------------------------------------------

echo
echo "Building ExperienceRuntime on Raspberry Pi..."

"${SSH_BUILD_CMD[@]}"

# ------------------------------------------------------------
# Complete
# ------------------------------------------------------------

echo
echo "============================================"
echo " Deployment complete"
echo "============================================"
