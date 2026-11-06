#!/bin/bash
# Workstream A: controlled arms, Temporal, 30 fps limiter + vsync (Pi-like cadence), 2 Hz FX sampling off (probe measured alone), 10 Hz probe.
S=/private/tmp/claude-501/-Users-joseconchello-openFrameworks/3708143b-e35f-47d2-be0e-a27f9b54b179/scratchpad
P=EXPERIENCE_RUNTIME_SCENE_ACTIVITY_; T=EXPERIENCE_RUNTIME_TEMPORAL_SCENE=1; C=EXPERIENCE_RUNTIME_SCENE_COLOR=0
cp /Users/joseconchello/openFrameworks/apps/myApps/EcopunkVideoCollage/sketches/experience_runtime/bin/experience_runtime.app/Contents/MacOS/experience_runtime $S/app6/experience_runtime.app/Contents/MacOS/experience_runtime
for rep in 1 2 3; do
$S/run6.sh $S/wsa/r$rep/base 40 $T $C ${P}MODE=off
$S/run6.sh $S/wsa/r$rep/cpu_frames 40 $T $C ${P}MODE=cpu ${P}DEFER=frames
$S/run6.sh $S/wsa/r$rep/cpu_fence 40 $T $C ${P}MODE=cpu ${P}DEFER=fence
$S/run6.sh $S/wsa/r$rep/gpu_frames 40 $T $C ${P}MODE=gpu ${P}DEFER=frames
$S/run6.sh $S/wsa/r$rep/gpu_fence 40 $T $C ${P}MODE=gpu ${P}DEFER=fence
$S/run6.sh $S/wsa/r$rep/verify_frames 40 $T $C ${P}MODE=verify ${P}DEFER=frames
$S/run6.sh $S/wsa/r$rep/cpu_sync 40 $T $C ${P}MODE=cpu ${P}READBACK=sync
done
touch $S/wsa/DONE
