#!/bin/sh
# RT-004 Pi validation campaign -- single entrypoint (DIAGNOSTIC ONLY).
#   ./scripts/rt004/run_pi_validation.sh --plan        print scenarios/commands; launches nothing
#   ./scripts/rt004/run_pi_validation.sh --preflight   checks + 21 offline watchdog tests; no runtime launch
#   ./scripts/rt004/run_pi_validation.sh --run         preflight (always) + authorized campaign
#   ./scripts/rt004/run_pi_validation.sh --stop        ask a running campaign to stop safely
# Run from anywhere; operates on sketches/experience_runtime.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
APP_DIR=$(cd "$HERE/../../sketches/experience_runtime" && pwd)
RUN_ROOT=${RT004_RUN_ROOT:-$HOME/rt004fp}
case "${1:-}" in
  --plan)      exec python3 "$HERE/pi_validation.py" plan --app-dir "$APP_DIR" --run-root "$RUN_ROOT" ;;
  --preflight) exec python3 "$HERE/pi_validation.py" preflight --app-dir "$APP_DIR" --run-root "$RUN_ROOT" ;;
  --run)       shift; cd "$APP_DIR"; exec python3 "$HERE/pi_validation.py" run --app-dir "$APP_DIR" --run-root "$RUN_ROOT" "$@" ;;
  --stop)
    latest=$(ls -1d "$RUN_ROOT"/campaign-*/ 2>/dev/null | tail -n 1 || true)
    [ -n "$latest" ] || { echo "no campaign directory under $RUN_ROOT" >&2; exit 1; }
    touch "${latest}STOP"; echo "STOP requested: ${latest}STOP" ;;
  *) echo "usage: $0 --plan | --preflight | --run [--only S1,S2] [--continue-on-functional-failure] | --stop" >&2; exit 64 ;;
esac
