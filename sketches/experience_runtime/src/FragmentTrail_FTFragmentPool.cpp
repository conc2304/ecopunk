// RT-006: compiles sketches/fragment-trail/src/FTFragmentPool.cpp into ExperienceRuntime.
// A wrapper translation unit, not a src/ symlink: the included file's own
// quoted includes then resolve from fragment-trail/src first, so its local
// LFOBank.h/TriggerBus.h (namespace fragmenttrail) are found instead of the
// same-named shared/src classes earlier on this sketch's include path. See
// config.make (FT_PRODUCTION_RUNTIME).
#include "../../fragment-trail/src/FTFragmentPool.cpp"
