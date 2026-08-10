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
#
# Final Shared Effects Source-of-Truth Seam Proof session adds:
#   -I../../shared/src                      -- ShaderLibrary.h (TFEffectPicker.h's include)
#   -I../../shared/src/video-effects/catalog -- DefaultVideoEffectCatalog.h
#   -I../temporal-fields/src                 -- TFEffectPicker.h itself, plus its
#                                               header-only TFRandom.h/TFTextureCropFill.h
#                                               (no .cpp exists for either -- confirmed
#                                               by directory listing before adding this)
# This is a READ-ONLY dependency on temporal-fields' own source (a production scene
# tree) -- nothing under sketches/temporal-fields/ is modified, and none of its own
# build/behavior changes.
#
# Blob First Complete Production Migration adds:
#   -I../blob-region-prototype/src -- BlobSceneCore.h, the reusable scene-local
#                                      core BlobProductionScene.cpp constructs.
#                                      Same READ-ONLY cross-sketch-source pattern
#                                      as the temporal-fields entry immediately
#                                      above -- nothing under
#                                      sketches/blob-region-prototype/ is
#                                      modified by this sketch's own build.
# BlobDetector.h/BlobTracker.h/VideoRegionController.h/VideoRegionEffectRenderer.h/
# VideoRegionMathOf.h are all top-level shared/src headers, already covered by the
# existing "-I../../shared/src" entry above -- no new -I needed for those. Their
# .cpp files are symlinked into src/ (same established pattern as ShaderLibrary.cpp/
# DefaultVideoEffectCatalog.cpp/etc. above), not added as a second
# PROJECT_EXTERNAL_SOURCE_PATHS entry, for the exact Makefile-system reason
# documented at the top of this file.
#
# LINK-TIME NOTE (revised after the first build attempt): TFEffectPicker.cpp is
# compiled and linked as ONE translation unit, unchanged, per this session's own
# "don't modify production code" constraint. Its non-activityStatus()/update()/
# setup() methods (drawCurrent(), applyEffectUniforms(), pickNext()'s blacklist
# check) reference ShaderLibrary::get()/has(), videoeffects::asFloat/asVec3, and
# EffectKnowledgeBase::load{Blacklist,Whitelist,EffectLevelKnowledge}() --  even
# though this proof's harness never exercises those call paths (shaderLib stays
# nullptr and short-circuits every shaderLib-> call before it happens), the
# *linker* still requires every symbol TFEffectPicker.o references to resolve,
# regardless of runtime reachability -- there is no partial-link-by-called-method
# option available here. So ShaderLibrary.cpp, VideoEffectTypes.cpp (asFloat/
# asVec3), EffectKnowledgeBase.cpp, SinglePassShaderEffect.cpp (its vtable), and
# their own two leaf dependencies (EffectKnowledgeSerialization.cpp,
# VideoEffectAssetRegistry.cpp) are ALSO symlinked into src/ now -- see those six
# files (real files, same established symlink pattern). This is a LINK-TIME
# necessity, not a design choice: this harness still never CONSTRUCTS a
# ShaderLibrary instance nor calls one of its methods at runtime (shaderLib is
# nullptr on every path exercised here) -- "the real ShaderLibrary machinery is
# never instantiated" remains true as a RUNTIME claim; it is simply now present,
# unexercised, in the compiled binary, same as any other statically-linked-but-
# dead code path. See this session's implementation report for the full account.
################################################################################
PROJECT_CFLAGS = -I../../shared/src/scene -I../../shared/src/hud-runtime \
	-I../../shared/src/video-effects/core \
	-I../../shared/src/video-effects/evolution \
	-I../../shared/src/video-effects/knowledge \
	-I../../shared/src/video-effects/catalog \
	-I../../shared/src/video-effects/effects \
	-I../../shared/src \
	-I../temporal-fields/src \
	-I../../shared/src/hud-compositor \
	-I../../shared/src/hud-compositor/widgets \
	-I../blob-region-prototype/src \
	-ffunction-sections -fdata-sections

################################################################################
# PROJECT LDFLAGS
#
# -Wl,-dead_strip (ld64's per-symbol dead-code stripping, paired with
# -ffunction-sections/-fdata-sections above so each function/global gets its
# own section to strip independently): needed ONLY because of a link-time
# consequence discovered while wiring the Final Shared Effects Source-of-
# Truth Seam Proof session's TFEffectPicker.cpp symlink (see that session's
# report). Summary: DefaultVideoEffectCatalog.cpp's registerSinglePassEffects()
# is REAL, needed production code (TFEffectPicker::activityStatus() calls
# tfCatalogRegistry(), whose lazy static calls it) -- but it shares one
# translation unit with registerDefaultVideoEffects(), which ALSO calls
# registerMotionExtraction/registerMotionComposite/registerErosionEffects/
# registerRidgeline/registerTemporalTrails/registerReactionDiffusion. None of
# those six are ever called at runtime by anything this sketch links (nothing
# here calls registerDefaultVideoEffects, only registerSinglePassEffects) --
# but without dead-stripping, the linker still demands every symbol referenced
# anywhere in DefaultVideoEffectCatalog.o resolve, which would otherwise force
# symlinking all six of those (heavier, multi-pass) effect implementation
# files in just to satisfy the linker for code this seam proof never
# exercises -- exactly the "pull in the whole shared effects system" scope
# growth this session's own "narrow seam proof, not a Temporal migration"
# instruction rules out. -dead_strip lets the linker discard the
# never-referenced registerDefaultVideoEffects() (and its own six callees'
# unresolved symbols) as unreachable BEFORE symbol resolution fails on them --
# confirmed empirically: build only fails to resolve symbols for code paths
# actually reachable from this sketch's real call graph after enabling this.
################################################################################
PROJECT_LDFLAGS = -Wl,-dead_strip

################################################################################
# TEMPORAL RUNTIME ASSET REPRODUCIBILITY (Temporal Production Scene #2,
# asset-closure session)
#
# bin/data/ is entirely .gitignore'd (repo-root .gitignore's `**/bin/data/*`
# rule) — nothing under it survives a fresh checkout on its own. This
# sketch's Temporal shaders (fragmentDissolve/particleExistenceFade/
# textureBlendFade) and backgrounds/ set were, before this session, supplied
# by hand directly into bin/data/ during the Temporal migration (plain-file
# shader copies + a symlink for backgrounds — see
# docs/temporal-production-scene-2-migration-report.md, "Newly discovered
# risks"), which is real but not reproducible.
#
# Fix: these two asset groups now have a canonical tracked source at
# sketches/temporal-fields/data/{shaders,backgrounds}/ (NOT under any bin/
# tree, so it isn't .gitignore'd), synced into bin/data/ deterministically by
# scripts/sync-temporal-runtime-assets.py. Hooked into Release/Debug here so
# `make Release`/`make Debug` alone reproduces the required runtime asset
# state — no manual copy/symlink step required. Prerequisite-only rule (no
# recipe on Release/Debug themselves) so it merges with, rather than
# replaces, compile.project.mk's own Release/Debug recipes.
################################################################################
Release Debug: sync-temporal-runtime-assets
.PHONY: sync-temporal-runtime-assets
sync-temporal-runtime-assets:
	@python3 ../../scripts/sync-temporal-runtime-assets.py $(CURDIR)
