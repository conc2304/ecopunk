#!/bin/bash
S=/private/tmp/claude-501/-Users-joseconchello-openFrameworks/3708143b-e35f-47d2-be0e-a27f9b54b179/scratchpad; P=EXPERIENCE_RUNTIME_SCENE_ACTIVITY_; T=EXPERIENCE_RUNTIME_TEMPORAL_SCENE=1; B=EXPERIENCE_RUNTIME_SCENE_COLOR_STATS=1; G=${P}GPU_TIMER=1; C=EXPERIENCE_RUNTIME_SCENE_COLOR=0
for rep in 1 2; do
$S/run3.sh $S/perf4/r$rep/base 45 $T $C ${P}MODE=off
$S/run3.sh $S/perf4/r$rep/color_sync 45 $T $B ${P}MODE=off
$S/run3.sh $S/perf4/r$rep/color_deferred 45 $T $B EXPERIENCE_RUNTIME_SCENE_COLOR_MODE=deferred ${P}MODE=off
$S/run3.sh $S/perf4/r$rep/cpu_deferred 45 $T $C $G ${P}MODE=cpu
$S/run3.sh $S/perf4/r$rep/cpu_sync 45 $T $C $G ${P}MODE=cpu ${P}READBACK=sync
$S/run3.sh $S/perf4/r$rep/gpu_deferred 45 $T $C $G ${P}MODE=gpu
$S/run3.sh $S/perf4/r$rep/gpu_sync 45 $T $C $G ${P}MODE=gpu ${P}READBACK=sync
$S/run3.sh $S/perf4/r$rep/both_sync_color 45 $T $B $G ${P}MODE=cpu
$S/run3.sh $S/perf4/r$rep/both_deferred 45 $T $B $G EXPERIENCE_RUNTIME_SCENE_COLOR_MODE=deferred ${P}MODE=cpu
done
