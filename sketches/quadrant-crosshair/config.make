# A single external source path is required here: this OF version's
# Makefile system uses PROJECT_EXTERNAL_SOURCE_PATHS in a plain $(subst)
# (and in a pattern-rule prerequisite) that only works correctly with one
# value, not a space-separated list. Point at the shared/src/ parent (which
# recursively contributes its hud/ and ridgeline/ subdirectories as source
# paths) and exclude the parent's own top-level sources, which are
# blueprint_emergence-specific classes (GridState, LFOBank, MotionExtraction,
# ShaderLibrary, TriggerBus, ...) that collide by name with quadrant-crosshair's
# own independent implementations of the same classes in src/.
PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src
PROJECT_EXCLUSIONS = ../../shared/src
