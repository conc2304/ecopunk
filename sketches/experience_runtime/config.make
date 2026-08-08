################################################################################
# CONFIGURE PROJECT MAKEFILE (optional)
################################################################################
OF_ROOT = ../../../../..

################################################################################
# PROJECT EXTERNAL SOURCE PATHS
#
# CONFIRMED (not just warned, per quadrant-crosshair/config.make's older
# comment) during the Shared Video Playback System increment:
# PROJECT_EXTERNAL_SOURCE_PATHS really does only work correctly with ONE
# value on this OF checkout's Makefile system, the moment that value
# actually needs real .cpp compilation. The evidence: this sketch
# previously listed two scoped paths space-separated
# (../../shared/src/scene ../../shared/src/hud-runtime) and built fine —
# but both of those are header-only (no .cpp), so
# config.project.mk:568's `OBJS_WITHOUT_EXTERNAL = $(subst
# $(strip $(PROJECT_EXTERNAL_SOURCE_PATHS)),,$(OBJS_WITH_PREFIX))` never
# actually had an object file to mis-transform. Adding
# shared/src/video-playback/ (real .cpp files) as a THIRD space-separated
# entry immediately broke the build: `subst`'s first argument is the
# *entire* multi-word PROJECT_EXTERNAL_SOURCE_PATHS string treated as one
# literal search pattern, which never matches any single object's actual
# path, so the embedded "../../" from that path is never stripped and the
# resulting object target (e.g.
# "obj/osx/Release/../../shared/src/video-playback/VideoPlaybackService.o")
# doesn't match compile.project.mk's per-external-path pattern rule ->
# "No rule to make target" at link time. See this increment's
# implementation report, "Newly discovered risks", for the full
# diagnosis — this is a pre-existing Makefile-system limitation, not
# something specific to this subsystem, and will bite the same way for any
# future second external directory that contains real source files.
#
# Fix used here: exactly ONE PROJECT_EXTERNAL_SOURCE_PATHS value
# (shared/src/video-playback/, the one directory that actually needs
# object compilation). shared/src/scene/ and shared/src/hud-runtime/
# remain header-only and are made visible to the compiler via a plain
# PROJECT_CFLAGS -I flag instead (below) — no object-path substitution
# logic involved for pure include-search paths, so the same bug can't
# recur there.
################################################################################
PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src/video-playback

################################################################################
# PROJECT EXCLUSIONS
#
# shared/src/video-playback/test/ contains standalone *_tests.cpp files,
# each defining its own main() for the dependency-free test convention
# (see that directory's Makefile.tests header comment) — excluded here so
# PROJECT_EXTERNAL_SOURCE_PATHS' recursive directory sweep doesn't pull
# them into this sketch's real build and collide with main.cpp's main() at
# link time. The trailing "%" matches this directory's own contents, not
# just an exact "test" path segment — see fragment-trail/config.make's own
# comment on this Makefile system's exclusion-matching behavior (exact
# per-discovered-subdirectory string match, not recursive path matching).
################################################################################
PROJECT_EXCLUSIONS = ../../shared/src/video-playback/test%

################################################################################
# PROJECT CFLAGS
#
# Pure include-search-path additions for the header-only shared
# directories this sketch depends on (SceneContract.h,
# SceneSemanticTypes.h, HudFrameData.h) — see the
# PROJECT_EXTERNAL_SOURCE_PATHS comment above for why these are NOT also
# listed there.
#
# video-effects/{core,evolution,knowledge} added in Engineering Session 2
# for shared/src/video-effects/knowledge/EffectActivityStatus.h's own
# transitive include chain (EffectEvolutionController.h -> EffectKnowledgeBase.h/
# EffectRandomizer.h -> VideoEffectDefinition.h/VideoEffectParameters.h) —
# every header on this chain is documented OF-free (EffectActivityStatus.h's
# own header comment) and EffectActivityStatus.h/.cpp only USE the
# EvolutionPhase enum VALUE from that chain, never call an
# EffectEvolutionController/EffectKnowledgeBase/EffectRandomizer/
# VideoEffectDefinition METHOD — confirmed empirically: this sketch links
# cleanly with none of those classes' own .cpp files compiled in, only
# EffectActivityStatus.cpp itself (see src/EffectActivityStatus.cpp, a
# symlink to the real file — see that file's own note on why a symlink
# rather than a second PROJECT_EXTERNAL_SOURCE_PATHS entry, which the
# comment above already shows breaks this Makefile system).
#
# hud-compositor + hud-compositor/widgets added for HudCompositorBridge's
# real HUD renderer path (HudWireframeRenderer + friends). Same "one real
# PROJECT_EXTERNAL_SOURCE_PATHS entry only" limitation applies here too, so
# the same symlink-individual-.cpp-files-into-src/ pattern already
# established for EffectActivityStatus.cpp above is reused for all of
# shared/src/hud-compositor/{*.cpp,widgets/*.cpp} — see
# src/HudWireframeRenderer.cpp etc., each a symlink to the real file.
# Deliberately excludes shared/src/hud-compositor/fake/ — the six
# deterministic FakeHudScenario implementations are a Validation-Studio-only
# concern (hud_validation_studio/config.make's own
# PROJECT_EXTERNAL_SOURCE_PATHS already owns that tree); ExperienceRuntime
# consumes the real HudFrameData path only (HudWireframeRenderer::update(
# float, const HudFrameData&) — see that method's own comment) and links
# none of the fake/ scenario .cpp files.
################################################################################
PROJECT_CFLAGS = -I../../shared/src/scene -I../../shared/src/hud-runtime \
	-I../../shared/src/video-effects/core \
	-I../../shared/src/video-effects/evolution \
	-I../../shared/src/video-effects/knowledge \
	-I../../shared/src/hud-compositor \
	-I../../shared/src/hud-compositor/widgets
