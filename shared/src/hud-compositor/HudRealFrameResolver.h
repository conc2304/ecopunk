#pragma once

// ============================================================================
// HudRealFrameResolver.h — the PRODUCTION source resolver, consuming the
// real, approved shared/src/hud-runtime/HudFrameData directly.
//
// Deliberately a SEPARATE class/file from HudSourceResolver (which
// resolves the fake-only FakeHudFrameData), not a second overload on the
// same class — HudFrameData.h transitively includes
// shared/src/scene/SceneContract.h, which includes ofTexture.h. Folding
// this resolver into HudSourceResolver.h would make EVERY consumer of
// that header (including this domain's ~500-check dependency-free test
// suite) require openFrameworks on the include path — exactly the
// regression this domain's whole OF-independent testing strategy
// (HudDataTypes.h's header comment) exists to prevent. Splitting instead
// means the dependency-free suite stays dependency-free, and this file
// gets its own, OF-dependent-but-still-widget-independent test coverage
// (see shared/src/hud-compositor-test/hud_real_frame_tests.cpp).
//
// Reuses HudSourceResolver's static, frame-independent canonical-ID table
// (isKnownCanonicalSource/staticValueTypeOf/allCanonicalSourceIds/
// looksLikeSceneMetricId) rather than duplicating it — both resolvers
// agree on the same ~40 canonical IDs and the same scene-metric ID shape;
// only the frame-dependent resolution logic differs (because the two
// frame types are shaped differently — see FakeHudSemanticTypes.h's
// reconciliation note for exactly how).
//
// This class never polls SceneManager, scenes, RuntimeServices, video, or
// effect services itself — it only ever reads the one HudFrameData value
// passed to resolve(), matching this task's "HudCompositor remains a pure
// consumer" requirement.
// ============================================================================

#include "HudDataTypes.h"
#include "HudSourceResolver.h" // for the shared static canonical-ID table

#include "HudFrameData.h" // real, shared/src/hud-runtime/ — OF-dependent transitively (SceneContract.h -> ofTexture.h)

#include <optional>
#include <string>

namespace hudpresent {

class HudRealFrameResolver {
public:
	// Resolves `sourceId` against the real `frame`. Same nullopt-vs-
	// missing contract as HudSourceResolver::resolve() — see that class's
	// header comment for the exact rule.
	std::optional<HudResolvedValue> resolve(const std::string& sourceId, const HudFrameData& frame) const;

	// Architecture-Closure Session (DEC-015): production default is
	// false — a missing HudFrameData.effects snapshot resolves
	// effects.active as MISSING, not silently backed by
	// SceneHudStatus::activeEffects (see resolveEffects()'s own comment
	// for why). Set true ONLY for an explicitly-labeled compatibility-
	// fixture/demo case (e.g. one deliberate Validation Studio scenario
	// proving the old field still works for as-yet-unmigrated tooling) —
	// never for a real production HudFrameData consumer.
	void setActiveEffectsCompatibilityFallbackEnabled(bool enabled) { compatibilityFallbackEnabled_ = enabled; }
	bool activeEffectsCompatibilityFallbackEnabled() const { return compatibilityFallbackEnabled_; }

private:
	std::optional<HudResolvedValue> resolveCanonical(const std::string& sourceId, const HudFrameData& frame) const;
	std::optional<HudResolvedValue> resolveSceneMetric(const std::string& sourceId, const HudFrameData& frame) const;
	std::optional<HudResolvedValue> resolveEffects(const std::string& sourceId, const HudFrameData& frame) const;

	bool compatibilityFallbackEnabled_ = false;
};

} // namespace hudpresent
