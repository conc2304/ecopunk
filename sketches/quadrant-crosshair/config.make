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
# The ExperienceRuntime subsystems added to shared/src after this sketch's
# exclusion list was written (hud-compositor and its -test directory of
# standalone *_tests.cpp main()s, hud-runtime, scene, video-playback) are not
# used here; the recursive sweep otherwise compiles them without their
# include paths (e.g. video-playback's Rt004StallProbe.h, which lives in the
# excluded top level) and fails the build. `%` covers nested subdirectories;
# hud-compositor% also matches hud-compositor-test. video-effects/test (its
# own *_tests.cpp main()s) likewise. video-effects/ (the shared
# catalog Quadrant::drawWithEffect() sources its defaults from), hud/,
# hud_overlay/ and ridgeline/ stay included.
PROJECT_EXCLUSIONS = ../../shared/src \
	../../shared/src/hud-compositor% ../../shared/src/hud-runtime% \
	../../shared/src/scene% ../../shared/src/video-playback% \
	../../shared/src/video-effects/test%
