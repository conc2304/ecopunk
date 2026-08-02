################################################################################
# CONFIGURE PROJECT MAKEFILE (optional)
#   This file is where we make project specific configurations.
################################################################################

OF_ROOT = ../../../../..

################################################################################
# PROJECT EXTERNAL SOURCE PATHS
#   shared/src is included (recursively contributing its hud/ and ridgeline/
#   subdirectories, for WindowChrome + the canonical hud:: widget library),
#   but its own top level is excluded wholesale below — see PROJECT_EXCLUSIONS.
#
#   CrosshairSystem, TriggerBus, ShaderLibrary, and LFOBank are NOT reused
#   from shared/src here — those class names collide with unrelated
#   blueprint_emergence-owned classes of the same name that live at
#   shared/src's top level (see quadrant-crosshair/config.make, which hit the
#   same collision first). This sketch copies its own independent versions
#   of those four classes into src/, same as quadrant-crosshair does.
#
#   TimeOffsetVideoBuffer (needed here, unlike quadrant-crosshair) is ALSO
#   a local copy in src/, not read from shared/src, even though it has no
#   naming collision — a scoped PROJECT_EXCLUSIONS listing only the four
#   colliding FILES (leaving TimeOffsetVideoBuffer unexcluded) was tried
#   first and failed: confirmed via a real Debug build that excluding
#   individual files within an already-included directory does not
#   reliably work in this Makefile system (shared/src/LFOBank.cpp and
#   ShaderLibrary.cpp were compiled and linked in despite being listed,
#   producing duplicate-symbol link errors against the local copies of the
#   same name).
#
#   PROJECT_EXCLUSIONS below now has TWO entries — this does work, once you
#   understand the actual mechanism (see config.project.mk's
#   `OF_PROJECT_SOURCE_PATHS = $(filter-out $(OF_PROJECT_EXCLUSIONS), ...)`):
#   each entry is matched against the full list of individually-discovered
#   subdirectories under PROJECT_EXTERNAL_SOURCE_PATHS, not matched
#   recursively against a directory tree. A bare `../../shared/src` entry
#   therefore excludes ONLY files sitting directly in that top-level
#   directory (ShaderLibrary.cpp, LFOBank.cpp, MotionExtraction.cpp, etc.) —
#   subdirectories like hud/, ridgeline/, and (newly) video-effects/ are
#   each their own separate entries in that discovered list and need their
#   own exclusion pattern to be dropped. `../../shared/src/video-effects%`
#   (with Make's `%` wildcard) matches every nested video-effects/*
#   subdirectory as a suffix pattern, which is what actually excludes the
#   whole subtree — a bare second entry without the wildcard does NOT work
#   (confirmed: it left shared/src/video-effects/effects/ still compiled).
################################################################################
PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src

# shared/src/video-effects/ (added for the video-effect-library
# consolidation, see docs/shared-video-effect-architecture.md) pulls in
# shared/src/MotionExtraction.h at the top level of shared/src, which is
# unreachable here since the exclusion above drops shared/src's own
# top-level include path — same root cause as the ShaderLibrary/LFOBank
# collision this file already documents. FT was deliberately deferred from
# adopting the shared video-effect service (see that doc's Phase 8), so
# exclude the whole subtree rather than leave it half-compiled.
PROJECT_EXCLUSIONS = ../../shared/src ../../shared/src/video-effects%

# osx template
# export MAC_OS_MIN_VERSION = 10.15
# export MAC_OS_CPP_VER = -std=c++17
