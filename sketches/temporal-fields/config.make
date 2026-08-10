################################################################################
# CONFIGURE PROJECT MAKEFILE (optional)
#   This file is where we make project specific configurations.
################################################################################

################################################################################
# OF ROOT
#   The location of your root openFrameworks installation
#       (default) OF_ROOT = ../../.. 
################################################################################
OF_ROOT = ../../../../..

################################################################################
# PROJECT ROOT
#   The location of the project - a starting place for searching for files
#       (default) PROJECT_ROOT = . (this directory)
#    
################################################################################
# PROJECT_ROOT = .

################################################################################
# PROJECT SPECIFIC CHECKS
#   This is a project defined section to create internal makefile flags to 
#   conditionally enable or disable the addition of various features within 
#   this makefile.  For instance, if you want to make changes based on whether
#   GTK is installed, one might test that here and create a variable to check. 
################################################################################
# None

################################################################################
# PROJECT EXTERNAL SOURCE PATHS
#   These are fully qualified paths that are not within the PROJECT_ROOT folder.
#   Like source folders in the PROJECT_ROOT, these paths are subject to 
#   exlclusion via the PROJECT_EXLCUSIONS list.
#
#     (default) PROJECT_EXTERNAL_SOURCE_PATHS = (blank) 
#
#   Note: Leave a leading space when adding list items with the += operator
################################################################################
PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src

################################################################################
# PROJECT EXCLUSIONS
#   These makefiles assume that all folders in your current project directory 
#   and any listed in the PROJECT_EXTERNAL_SOURCH_PATHS are are valid locations
#   to look for source code. The any folders or files that match any of the 
#   items in the PROJECT_EXCLUSIONS list below will be ignored.
#
#   Each item in the PROJECT_EXCLUSIONS list will be treated as a complete 
#   string unless teh user adds a wildcard (%) operator to match subdirectories.
#   GNU make only allows one wildcard for matching.  The second wildcard (%) is
#   treated literally.
#
#      (default) PROJECT_EXCLUSIONS = (blank)
#
#		Will automatically exclude the following:
#
#			$(PROJECT_ROOT)/bin%
#			$(PROJECT_ROOT)/obj%
#			$(PROJECT_ROOT)/%.xcodeproj
#
#   Note: Leave a leading space when adding list items with the += operator
################################################################################
# Previously excluded shared/src/hud here on a claim that it "double-
# compiles/link-fails for any sketch that pulls in all of shared/src" — that
# didn't hold up: blueprint_emergence's and quadrant-crosshair's obj/ dirs
# already contain successfully-linked hud/*.o files, and a full Debug build
# of this sketch with the exclusion removed links cleanly (all 11 hud
# widgets + TFHudLayer.o + MotionExtraction.o present in the final link
# line, exit 0). No file-level or class-name collision exists between
# shared/src/hud and this sketch's TF* sources either. Leaving unexcluded.
#
# The three exclusions below ARE a real, separate collision, unrelated to
# the hud/ note above: shared/src/video-effects/test/,
# shared/src/hud-compositor-test/, and shared/src/video-playback/test/ each
# contain a standalone *_tests.cpp with its own main() (the dependency-free
# test convention documented in each one's own Makefile.tests), which
# collides with src/*.cpp's real main() at link time ("duplicate symbol
# '_main'") if PROJECT_EXTERNAL_SOURCE_PATHS pulls them in unexcluded — the
# same fix already applied to sketches/blob-region-prototype/config.make
# (Shared Video Playback System increment) and
# sketches/shader-effect-debugger/config.make (Shared Effect Knowledge
# Engineering Session 2); mirrored here rather than reinvented, required by
# this increment's own temporal-fields build/test step. The trailing "%"
# matches each directory's own contents — see fragment-trail/config.make's
# own comment on this Makefile system's exact-string, non-recursive
# exclusion matching.
PROJECT_EXCLUSIONS = ../../shared/src/video-playback/test% ../../shared/src/video-effects/test% ../../shared/src/hud-compositor-test%

################################################################################
# PROJECT LINKER FLAGS
#	These flags will be sent to the linker when compiling the executable.
#
#		(default) PROJECT_LDFLAGS = -Wl,-rpath=./libs
#
#   Note: Leave a leading space when adding list items with the += operator
################################################################################

# Currently, shared libraries that are needed are copied to the 
# $(PROJECT_ROOT)/bin/libs directory.  The following LDFLAGS tell the linker to
# add a runtime path to search for those shared libraries, since they aren't 
# incorporated directly into the final executable application binary.
# TODO: should this be a default setting?
# PROJECT_LDFLAGS=-Wl,-rpath=./libs

################################################################################
# PROJECT DEFINES
#   Create a space-delimited list of DEFINES. The list will be converted into 
#   CFLAGS with the "-D" flag later in the makefile.
#
#		(default) PROJECT_DEFINES = (blank)
#
#   Note: Leave a leading space when adding list items with the += operator
################################################################################
# PROJECT_DEFINES = 

################################################################################
# PROJECT CFLAGS
#   This is a list of fully qualified CFLAGS required when compiling for this 
#   project.  These CFLAGS will be used IN ADDITION TO the PLATFORM_CFLAGS 
#   defined in your platform specific core configuration files. These flags are
#   presented to the compiler BEFORE the PROJECT_OPTIMIZATION_CFLAGS below. 
#
#		(default) PROJECT_CFLAGS = (blank)
#
#   Note: Before adding PROJECT_CFLAGS, note that the PLATFORM_CFLAGS defined in 
#   your platform specific configuration file will be applied by default and 
#   further flags here may not be needed.
#
#   Note: Leave a leading space when adding list items with the += operator
################################################################################
# PROJECT_CFLAGS = 

################################################################################
# PROJECT OPTIMIZATION CFLAGS
#   These are lists of CFLAGS that are target-specific.  While any flags could 
#   be conditionally added, they are usually limited to optimization flags. 
#   These flags are added BEFORE the PROJECT_CFLAGS.
#
#   PROJECT_OPTIMIZATION_CFLAGS_RELEASE flags are only applied to RELEASE targets.
#
#		(default) PROJECT_OPTIMIZATION_CFLAGS_RELEASE = (blank)
#
#   PROJECT_OPTIMIZATION_CFLAGS_DEBUG flags are only applied to DEBUG targets.
#
#		(default) PROJECT_OPTIMIZATION_CFLAGS_DEBUG = (blank)
#
#   Note: Before adding PROJECT_OPTIMIZATION_CFLAGS, please note that the 
#   PLATFORM_OPTIMIZATION_CFLAGS defined in your platform specific configuration 
#   file will be applied by default and further optimization flags here may not 
#   be needed.
#
#   Note: Leave a leading space when adding list items with the += operator
################################################################################
# PROJECT_OPTIMIZATION_CFLAGS_RELEASE = 
# PROJECT_OPTIMIZATION_CFLAGS_DEBUG = 

################################################################################
# PROJECT COMPILERS
#   Custom compilers can be set for CC and CXX
#		(default) PROJECT_CXX = (blank)
#		(default) PROJECT_CC = (blank)
#   Note: Leave a leading space when adding list items with the += operator
################################################################################
# PROJECT_CXX = 
# PROJECT_CC = 

# osx template

# Uncomment/comment below to switch between C++11 and C++17 ( or newer ). On macOS C++17 needs 10.15 or above.
# export MAC_OS_MIN_VERSION = 10.15
# export MAC_OS_CPP_VER = -std=c++17

################################################################################
# TEMPORAL RUNTIME ASSET REPRODUCIBILITY (Temporal Production Scene #2,
# asset-closure session)
#
# This sketch is the canonical owner of the Temporal-only shader pairs
# (fragmentDissolve/particleExistenceFade/textureBlendFade) and the
# backgrounds/ ambient-texture set, authored at
# sketches/temporal-fields/data/{shaders,backgrounds}/ (tracked — not under
# any bin/ tree). Its own bin/data/ copies are just as .gitignore'd as
# experience_runtime's (repo-root .gitignore's `**/bin/data/*`), so this
# sketch needs the same deterministic sync, not just the sketch that
# consumes it downstream. See
# scripts/sync-temporal-runtime-assets.py and experience_runtime/config.make's
# matching hook for the full rationale.
################################################################################
Release Debug: sync-temporal-runtime-assets
.PHONY: sync-temporal-runtime-assets
sync-temporal-runtime-assets:
	@python3 ../../scripts/sync-temporal-runtime-assets.py $(CURDIR)
