################################################################################
# CONFIGURE PROJECT MAKEFILE (optional)
################################################################################
OF_ROOT = ../../../../..

################################################################################
# PROJECT EXTERNAL SOURCE PATHS
#
# hud_validation_studio needs shared/src/hud-compositor/ (the whole tree:
# the presentation-model core, fake/ scenarios, widgets/) — deliberately
# NOT shared/src/hud-compositor-test (a sibling directory, not a
# subdirectory, precisely so this recursive sweep never reaches it; see
# shared/src/hud-compositor-test/hud_presentation_tests.cpp's header
# comment for the collision this avoids) and NOT shared/src/hud/ (the
# scene-local widget library this app deliberately does not depend on —
# see shared/src/hud-compositor/widgets/HudWidgetBase.h's header comment).
################################################################################
PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src/hud-compositor

################################################################################
# PROJECT CFLAGS
#
# Engineering Session 2's production path (HudRealFrameResolver.h,
# MediaViewportMesh.h via HudWireframeRenderer.h) needs
# shared/src/scene/SceneContract.h + SceneSemanticTypes.h,
# shared/src/hud-runtime/HudFrameData.h, and
# shared/src/video-playback/VideoPlaybackStatus.h on the include path.
# Added as plain -I flags (header-only usage — nothing under
# video-playback/ beyond VideoPlaybackStatus.h's own struct is used, so
# none of that directory's real .cpp files need PROJECT_EXTERNAL_SOURCE_PATHS'
# object-compilation sweep), matching the exact same safe pattern
# sketches/experience_runtime/config.make already established for this
# repo's "PROJECT_EXTERNAL_SOURCE_PATHS breaks with more than one
# real-source-containing value" Makefile limitation (see that file's own
# comment) — this sketch's own external-source slot stays reserved for
# shared/src/hud-compositor/ alone.
#
# Architecture-Closure Session: shared/src/hud-runtime/HudFrameData.h now
# ALSO includes shared/src/video-effects/knowledge/EffectActivityStatus.h
# (DEC-015's additive `effects` field), whose own transitive chain needs
# the same three flat per-subdirectory include paths
# sketches/experience_runtime/config.make already added for identical
# reasons. HudRealFrameResolver.cpp now also CALLS
# videoeffects::resolveDominantEffectIds() (not just references the
# header's types), so EffectActivityStatus.cpp itself must be compiled in
# too — symlinked directly into src/ (src/EffectActivityStatus.cpp) rather
# than added as a second PROJECT_EXTERNAL_SOURCE_PATHS entry, for the same
# "only one real-source-containing external path works on this Makefile
# system" reason already documented above and in
# sketches/experience_runtime/config.make.
################################################################################
PROJECT_CFLAGS = -I../../shared/src/scene -I../../shared/src/hud-runtime -I../../shared/src/video-playback \
	-I../../shared/src/video-effects/core \
	-I../../shared/src/video-effects/evolution \
	-I../../shared/src/video-effects/knowledge
