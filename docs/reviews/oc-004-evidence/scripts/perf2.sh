#!/bin/bash
S=/private/tmp/claude-501/-Users-joseconchello-openFrameworks/3708143b-e35f-47d2-be0e-a27f9b54b179/scratchpad
P=EXPERIENCE_RUNTIME_SCENE_ACTIVITY_
T=EXPERIENCE_RUNTIME_TEMPORAL_SCENE=1
B=EXPERIENCE_RUNTIME_SCENE_COLOR_STATS=1
$S/run2.sh $S/perf2/t_base 60 $T EXPERIENCE_RUNTIME_SCENE_COLOR=0 ${P}MODE=off
$S/run2.sh $S/perf2/t_color_sync 60 $T $B ${P}MODE=off
$S/run2.sh $S/perf2/t_color_deferred 60 $T $B EXPERIENCE_RUNTIME_SCENE_COLOR_MODE=deferred ${P}MODE=off
$S/run2.sh $S/perf2/t_cpu_deferred 60 $T EXPERIENCE_RUNTIME_SCENE_COLOR=0 ${P}MODE=cpu
$S/run2.sh $S/perf2/t_cpu_sync 60 $T EXPERIENCE_RUNTIME_SCENE_COLOR=0 ${P}MODE=cpu ${P}READBACK=sync
$S/run2.sh $S/perf2/t_gpu_deferred 60 $T EXPERIENCE_RUNTIME_SCENE_COLOR=0 ${P}MODE=gpu
$S/run2.sh $S/perf2/t_gpu_sync 60 $T EXPERIENCE_RUNTIME_SCENE_COLOR=0 ${P}MODE=gpu ${P}READBACK=sync
$S/run2.sh $S/perf2/t_both 60 $T $B ${P}MODE=cpu
$S/run2.sh $S/perf2/b_base 60 EXPERIENCE_RUNTIME_SCENE_COLOR=0 ${P}MODE=off
$S/run2.sh $S/perf2/b_cpu_deferred 60 EXPERIENCE_RUNTIME_SCENE_COLOR=0 ${P}MODE=cpu
$S/run2.sh $S/q8/blueprint_q8 40 EXPERIENCE_RUNTIME_BLUEPRINT_SCENE=1 EXPERIENCE_RUNTIME_SCENE_COLOR=0 ${P}MODE=gpu ${P}QUANT8=1
$S/run2.sh $S/q8/blueprint_f32 40 EXPERIENCE_RUNTIME_BLUEPRINT_SCENE=1 EXPERIENCE_RUNTIME_SCENE_COLOR=0 ${P}MODE=gpu
