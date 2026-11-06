#!/bin/bash
# usage: run3.sh <outdir> <seconds> [ENV=VAL ...]  -- isolated binary, uncapped, logs load at start
out=$1; secs=$2; shift 2; mkdir -p "$out"
uptime > "$out/load.txt"; ps -A -o %cpu,comm -r | sed -n 2,5p >> "$out/load.txt"
cd /private/tmp/claude-501/-Users-joseconchello-openFrameworks/3708143b-e35f-47d2-be0e-a27f9b54b179/scratchpad/app3/experience_runtime.app/Contents/MacOS
env EXPERIENCE_RUNTIME_SCENE_ACTIVITY_PROBE="$out" EXPERIENCE_RUNTIME_FRAME_PHASE_PROBE=1 EXPERIENCE_RUNTIME_FRAME_PHASE_LOG="$out/phase.log" EXPERIENCE_RUNTIME_FRAME_LIMITER=off EXPERIENCE_RUNTIME_VSYNC=off "$@" ./experience_runtime > "$out/app.log" 2>&1 &
pid=$!; sleep "$secs"; kill $pid 2>/dev/null; sleep 1; kill -9 $pid 2>/dev/null; wait $pid 2>/dev/null
