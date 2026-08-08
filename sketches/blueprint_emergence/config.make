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
# Engineering Session 2, Task B: PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src
# above pulls in every subdirectory recursively, including several that
# contain standalone *_tests.cpp files with their own main() (the
# dependency-free test convention documented in
# shared/src/video-playback/test/Makefile.tests). Left unexcluded, these
# collide with src/main.cpp's real main() at link time ("duplicate symbol
# '_main'") — confirmed directly for this exact sketch during this session
# (this was a pre-existing, latent build break, not introduced by this
# session — see the implementation report's "Newly discovered risks").
# Fixed via the one shared, repository-level exclusion list — see
# shared/build/test-main-exclusions.mk.
#
# Also excludes shared/src/hud-compositor% (the HUD Runtime domain's own,
# separately in-progress semantic-slot-binding/widget subsystem):
# confirmed by direct build attempt during this session that it currently
# fails to compile independent of anything in this session's own changes
# (a FakeSceneHealth/SceneHealth type mismatch in
# shared/src/hud-compositor/HudWireframeRenderer.cpp — see this session's
# implementation report, "Newly discovered risks"), and confirmed by
# `grep` that this sketch's own sources never include anything under
# hud-compositor/ — it is swept in only as an unwanted side effect of the
# unscoped PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src above, not
# because this sketch needs it. Excluding it here is scoping this
# sketch's OWN build to what it actually uses, not a fix to the HUD
# Runtime domain's bug (which remains unfixed and out of this session's
# scope).
include ../../shared/build/test-main-exclusions.mk
PROJECT_EXCLUSIONS = $(SHARED_TEST_MAIN_EXCLUSIONS) ../../shared/src/hud-compositor%

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
