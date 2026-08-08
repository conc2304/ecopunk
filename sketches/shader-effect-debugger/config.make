################################################################################
# CONFIGURE PROJECT MAKEFILE (optional)
################################################################################
OF_ROOT = ../../../../..
PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src

# PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src pulls in every
# subdirectory recursively, including several that contain standalone
# *_tests.cpp files with their own main() (the dependency-free test
# convention documented in shared/src/video-effects/test/Makefile.tests,
# shared/src/hud-compositor-test/, and shared/src/video-playback/test/).
# Left unexcluded, any of these collide with src/main.cpp's real main() at
# link time ("duplicate symbol '_main'") -- this is the exact pre-existing
# link failure identified in the Shared Effect Knowledge scoped-extension
# session's implementation report ("Newly discovered risks") and
# independently re-confirmed and fixed the same way for
# sketches/blob-region-prototype/config.make during the Shared Video
# Playback System increment; mirrored here rather than reinvented. The
# trailing "%" matches each directory's own contents -- see
# fragment-trail/config.make's own comment on this Makefile system's
# exact-string, non-recursive exclusion matching (a bare directory
# exclusion does not cover its own subdirectories without the wildcard).
PROJECT_EXCLUSIONS = ../../shared/src/video-playback/test% ../../shared/src/video-effects/test% ../../shared/src/hud-compositor-test%
