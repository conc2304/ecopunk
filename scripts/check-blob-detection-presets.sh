#!/bin/bash
# Predeploy gate: every production video should have a Blob detection preset.
#
# Usage:
#   scripts/check-blob-detection-presets.sh [--allow-missing-blob-presets] [--report-only]
#                                           [--media-root DIR] [--profiles FILE]
#
# Coverage logic lives in tools/blob-preset-coverage (built on demand; it
# uses the production media-catalog and preset-store code). This script only
# decides what a result means for a deployment:
#
#   PASS     every production video has a preset         -> exit 0, no prompt
#   WARNING  some videos have no preset (runtime falls back to detector
#            defaults for them)
#              interactive terminal -> "Continue deployment? [y/N]" (default No)
#              --allow-missing-blob-presets -> continue without asking
#              non-interactive, no override -> exit 1
#              --report-only (deploy --dry-run) -> report, exit 0
#   ERROR    preset file / media inputs invalid          -> exit 2, never overridable
#
# Exit: 0 continue, 1 stopped (missing presets), 2 invalid inputs.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TOOL_DIR="$REPO_ROOT/tools/blob-preset-coverage"
TOOL="$TOOL_DIR/blob_preset_coverage"

ALLOW_MISSING=0
REPORT_ONLY=0
FORCE_PROMPT=0 # test hook: prompt even when stdin is not a terminal
TOOL_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --allow-missing-blob-presets) ALLOW_MISSING=1 ;;
        --report-only) REPORT_ONLY=1 ;;
        --force-prompt) FORCE_PROMPT=1 ;;
        --media-root|--profiles)
            [[ $# -ge 2 ]] || { echo "missing value for $1" >&2; exit 2; }
            TOOL_ARGS+=("$1" "$2")
            shift
            ;;
        -h|--help)
            sed -n '2,22p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            exit 2
            ;;
    esac
    shift
done

if ! make -s -C "$TOOL_DIR" blob_preset_coverage >/dev/null; then
    echo "ERROR: could not build the Blob preset coverage tool ($TOOL_DIR)." >&2
    exit 2
fi

REPORT="$("$TOOL" ${TOOL_ARGS[@]+"${TOOL_ARGS[@]}"})"
STATUS=$?

case "$STATUS" in
    0)
        # PASS: one line, no prompt (stale-preset notes are informational).
        echo "$REPORT" | grep -E '^Blob detection presets: '
        if echo "$REPORT" | grep -q '^Note: '; then
            echo "$REPORT" | sed -n '/^Note: /,/^$/p'
        fi
        exit 0
        ;;
    1)
        echo "$REPORT"
        echo
        if [[ "$REPORT_ONLY" == "1" ]]; then
            echo "(report only — not blocking)"
            exit 0
        fi
        if [[ "$ALLOW_MISSING" == "1" ]]; then
            echo "Continuing: --allow-missing-blob-presets given."
            exit 0
        fi
        if [[ "$FORCE_PROMPT" == "1" || -t 0 ]]; then
            printf 'Continue deployment? [y/N] ' >&2
            read -r ANSWER || ANSWER=""
            case "$ANSWER" in
                y|Y|yes|YES|Yes)
                    echo "Continuing deployment with missing Blob presets."
                    exit 0
                    ;;
                *)
                    echo "Deployment stopped: Blob detection presets missing."
                    exit 1
                    ;;
            esac
        fi
        echo "Deployment stopped: Blob detection presets missing (non-interactive)." >&2
        echo "Re-run with --allow-missing-blob-presets to deploy anyway." >&2
        exit 1
        ;;
    *)
        echo "$REPORT"
        echo
        echo "Deployment stopped: Blob detection preset inputs are invalid (not overridable)." >&2
        exit 2
        ;;
esac
