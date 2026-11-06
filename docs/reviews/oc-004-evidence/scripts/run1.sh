#!/bin/bash
# usage: run1.sh <outdir> <seconds> [ENV=VAL ...]   (scene env passed through)
out=$1; secs=$2; shift 2
mkdir -p "$out"
cd /Users/joseconchello/openFrameworks/apps/myApps/EcopunkVideoCollage/sketches/experience_runtime/bin/experience_runtime.app/Contents/MacOS
env EXPERIENCE_RUNTIME_SCENE_ACTIVITY_PROBE="$out" "$@" ./experience_runtime > "$out/app.log" 2>&1 &
pid=$!
sleep "$secs"; kill $pid 2>/dev/null; sleep 1; kill -9 $pid 2>/dev/null; wait $pid 2>/dev/null
