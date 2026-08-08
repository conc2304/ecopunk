# shared/build/test-main-exclusions.mk
#
# ONE canonical, repository-level list of shared/src subdirectories that
# contain standalone *_tests.cpp files, each defining its own main() —
# per the dependency-free test convention documented in
# shared/src/video-playback/test/Makefile.tests,
# shared/src/video-effects/test/Makefile.tests, and
# sketches/experience_runtime/test/Makefile.tests.
#
# Why this file exists (Engineering Session 2, Task B): any sketch whose
# config.make points PROJECT_EXTERNAL_SOURCE_PATHS at the whole of
# ../../shared/src (not a scoped subdirectory) gets every one of these
# directories' .cpp files swept into its OWN production build too, via
# that Makefile system's recursive `find $(PROJECT_EXTERNAL_SOURCE_PATHS)
# -mindepth 1 -type d` directory discovery
# (libs/openFrameworksCompiled/project/makefileCommon/config.project.mk).
# Since each of those .cpp files defines its own main(), the production
# sketch's real main.cpp collides with it at link time
# ("duplicate symbol '_main'"). Confirmed directly during the Shared Video
# Playback System increment (Session 1) for
# shared/src/video-effects/test/ and shared/src/hud-compositor-test/,
# and would recur for shared/src/video-playback/test/ without exclusion.
#
# Rather than every sketch's config.make inventing its own copy of this
# list (which drifts the moment a new shared/src test directory is
# added — exactly what happened between Session 1 and Session 2), sketches
# that need the exclusion list now do:
#
#   include ../../shared/build/test-main-exclusions.mk
#   PROJECT_EXCLUSIONS = $(SHARED_TEST_MAIN_EXCLUSIONS)
#
# (or, if a sketch already has its own additional exclusions:
#   PROJECT_EXCLUSIONS = $(SHARED_TEST_MAIN_EXCLUSIONS) ../../shared/src/some-other-thing%
# )
#
# CONVENTION for anyone adding a new standalone-test directory under
# shared/src/: prefer nesting it as a `test/` subdirectory of the
# subsystem it tests (matches video-playback/test/, video-effects/test/)
# so the "%test%"-style wildcard entries below continue to cover it
# without a new line here. shared/src/hud-compositor-test/ is a
# pre-existing exception (a top-level directory, not nested) predating
# this convention — listed explicitly below since the wildcard pattern
# can't match it structurally; do not add another one shaped like it.
#
# When you add a new *_tests.cpp with its own main() anywhere under
# shared/src/ that does NOT fit the "test/" nesting convention, add its
# directory to this list explicitly, in this file only — not by copying
# an exclusion line into individual sketches' config.make files.
SHARED_TEST_MAIN_EXCLUSIONS = \
	../../shared/src/video-playback/test% \
	../../shared/src/video-effects/test% \
	../../shared/src/hud-compositor-test%
