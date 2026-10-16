// Quadrant Crosshair production scene: compiles
// sketches/quadrant-crosshair/src/QuadrantManager.cpp into ExperienceRuntime. A wrapper
// translation unit, not a src/ symlink: the included file's own quoted
// includes then resolve from quadrant-crosshair/src first, so its local
// LFOBank.h/TriggerBus.h/ShaderLibrary.h/MotionExtraction.h/GridState.h
// (namespace quadrantcrosshair) are found instead of the same-named shared/src
// classes earlier on this sketch's include path. See config.make
// (QC_PRODUCTION_RUNTIME).
#include "../../quadrant-crosshair/src/QuadrantManager.cpp"
