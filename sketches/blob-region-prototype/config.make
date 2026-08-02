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
# PROJECT EXTERNAL SOURCE PATHS
#   These are fully qualified paths that are not within the PROJECT_ROOT folder.
#   Like source folders in the PROJECT_ROOT, these paths are subject to
#   exlclusion via the PROJECT_EXLCUSIONS list.
#
#     (default) PROJECT_EXTERNAL_SOURCE_PATHS = (blank)
#
#   Note: Leave a leading space when adding list items with the += operator
################################################################################
# This is the blob-driven video-region prototype (see docs/blob-region-*.md).
# It reuses the canonical shared/src systems (Fragment, ShaderLibrary,
# TimeOffsetVideoBuffer, the new VideoRegion* / BlobDetector / BlobTracker
# classes) rather than re-implementing them sketch-locally, so shared/src
# must be on the build's source path — same convention as blueprint_emergence
# and temporal-fields.
PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src

################################################################################
# PROJECT EXCLUSIONS
################################################################################
PROJECT_EXCLUSIONS =

################################################################################
# PROJECT COMPILERS
################################################################################
# PROJECT_CXX =
# PROJECT_CC =

# osx template

# Uncomment/comment below to switch between C++11 and C++17 ( or newer ). On macOS C++17 needs 10.15 or above.
# export MAC_OS_MIN_VERSION = 10.15
# export MAC_OS_CPP_VER = -std=c++17
