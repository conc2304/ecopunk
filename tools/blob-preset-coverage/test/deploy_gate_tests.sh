#!/bin/bash
# Deploy-gate behavior tests for scripts/check-blob-detection-presets.sh,
# using throwaway fixtures (never the real assets). Run via `make test`.

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$HERE/../../.." && pwd)"
GATE="$REPO_ROOT/scripts/check-blob-detection-presets.sh"
TOOL="$REPO_ROOT/tools/blob-preset-coverage/blob_preset_coverage"

TMP="$(mktemp -d "${TMPDIR:-/tmp}/blob_gate_tests.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

PASSED=0
FAILED=0

# id_of FILE -> canonical media id, from the tool itself (production code).
id_of() {
    local root="$TMP/idprobe"
    rm -rf "$root"
    mkdir -p "$root"
    : > "$root/$1"
    printf '{"schemaVersion": 1, "profiles": {}}' > "$TMP/idprobe.json"
    "$TOOL" --media-root "$root" --profiles "$TMP/idprobe.json" | grep -m1 -o 'media\.auto\.[0-9a-f]*'
}

make_fixture() { # name, profiles-json, media files...
    local name="$1" json="$2"
    shift 2
    mkdir -p "$TMP/$name/media"
    for f in "$@"; do : > "$TMP/$name/media/$f"; done
    printf '%s' "$json" > "$TMP/$name/profiles.json"
}

A_ID="$(id_of a.mp4)"
B_ID="$(id_of b.mp4)"
make_fixture covered "{\"schemaVersion\": 1, \"profiles\": {\"$A_ID\": {\"threshold\": 30}, \"$B_ID\": {\"threshold\": 40}}}" a.mp4 b.mp4
make_fixture missing "{\"schemaVersion\": 1, \"profiles\": {\"$A_ID\": {\"threshold\": 30}}}" a.mp4 b.mp4
make_fixture malformed '{"schemaVersion": 1, "profiles": { broken' a.mp4

# check NAME EXPECTED_EXIT STDIN FIXTURE [gate args...] ; expectations on output via globals
check() {
    local name="$1" expect="$2" input="$3" fixture="$4"
    shift 4
    local out rc
    out="$(printf '%s' "$input" | "$GATE" --media-root "$TMP/$fixture/media" --profiles "$TMP/$fixture/profiles.json" "$@" 2>&1)"
    rc=$?
    local ok=1
    [[ "$rc" == "$expect" ]] || ok=0
    if [[ -n "${MUST:-}" ]] && ! grep -q -- "$MUST" <<<"$out"; then ok=0; fi
    if [[ -n "${MUST_NOT:-}" ]] && grep -q -- "$MUST_NOT" <<<"$out"; then ok=0; fi
    if [[ "$ok" == "1" ]]; then
        PASSED=$((PASSED + 1))
        echo "PASS  $name (exit $rc)"
    else
        FAILED=$((FAILED + 1))
        echo "FAIL  $name (exit $rc, wanted $expect)"
        echo "$out" | sed 's/^/      | /'
    fi
    MUST=""
    MUST_NOT=""
}

MUST="Blob detection presets: 2/2 configured" MUST_NOT="Continue deployment"
check "PASS -> no prompt, continue" 0 "" covered --force-prompt

MUST="Continuing deployment" ; check "WARN + user yes -> continue" 0 $'y\n' missing --force-prompt
MUST="Continue deployment? \[y/N\]" ; check "WARN + user no -> stop" 1 $'n\n' missing --force-prompt
MUST="Deployment stopped" ; check "WARN + empty response (default No) -> stop" 1 $'\n' missing --force-prompt
MUST="Deployment stopped" ; check "WARN + closed stdin at prompt -> stop" 1 "" missing --force-prompt
MUST="non-interactive" MUST_NOT="Continue deployment?" ; check "WARN non-interactive, no override -> fail" 1 "" missing
MUST="--allow-missing-blob-presets given" MUST_NOT="Continue deployment?" ; check "WARN non-interactive + override -> continue" 0 "" missing --allow-missing-blob-presets
MUST="WARNING: Blob detection presets missing for 1 media item" ; check "WARN lists the missing media" 1 "" missing
MUST="report only" ; check "WARN + --report-only (deploy dry run) -> continue" 0 "" missing --report-only
MUST="not overridable" MUST_NOT="Continue deployment?" ; check "ERROR + override -> cannot override" 2 $'y\n' malformed --allow-missing-blob-presets --force-prompt
MUST="not overridable" ; check "ERROR + report-only -> still stops" 2 "" malformed --report-only

echo "deploy_gate_tests: $PASSED/$((PASSED + FAILED)) checks passed"
[[ "$FAILED" == "0" ]]
