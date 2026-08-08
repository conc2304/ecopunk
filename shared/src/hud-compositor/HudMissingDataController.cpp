#include "HudMissingDataController.h"

namespace hudpresent {

namespace {

HudResolvedValue placeholderFor(HudSourceValueType type) {
	HudResolvedValue v;
	v.valueType = type;
	v.dataClass = HudDataClass::Ambient; // a placeholder is explicitly not a real measurement
	v.present = true;
	v.numberValue = 0.0f;
	v.boolValue = false;
	// Plain ASCII hyphen, not a UTF-8 em dash (Engineering Session 2 fix —
	// caught visually in a real Validation Studio screenshot: OF's fixed
	// bitmap font, per widgets/HudWidgetDrawUtils.h, is freeglut's classic
	// ASCII-only 8x13 font and has no glyph for U+2014, so the em dash
	// silently rendered as nothing). Resolves through vocabulary to
	// itself if unregistered (stable-ID fallback), same as before.
	v.textValue = "-";
	return v;
}

} // namespace

HudMissingDataController::Outcome HudMissingDataController::decide(
	const HudCompiledBinding& compiled,
	const std::optional<HudResolvedValue>& resolvedNow,
	const std::optional<HudResolvedValue>& lastValid,
	FakeSceneHealth sceneHealth,
	const std::optional<HudResolvedValue>& fallbackValue) const {
	Outcome out;

	// Unknown source: always hidden, no policy applies — there was never
	// a valid value to retain, placeholder-for, or fall back from.
	if (!resolvedNow) {
		out.shouldRender = false;
		return out;
	}

	// Failure suppresses live traces (Sparkline specifically — see header
	// comment for why this is scoped to that widget type).
	if (sceneHealth == FakeSceneHealth::Failed && compiled.binding.widgetType == HudWidgetType::Sparkline) {
		out.shouldRender = false;
		return out;
	}

	const bool loading = (sceneHealth == FakeSceneHealth::Loading);

	if (resolvedNow->present) {
		out.shouldRender = true;
		out.valueToShow = *resolvedNow;
		out.dimmed = loading; // "loading… may dim last-valid values" — broadened to all values while loading, see header comment
		return out;
	}

	// From here on, resolvedNow->present == false: apply missingPolicy.
	switch (compiled.binding.missingPolicy) {
		case HudMissingPolicy::Hide:
			out.shouldRender = false;
			return out;

		case HudMissingPolicy::Placeholder:
			out.shouldRender = true;
			out.valueToShow = placeholderFor(resolvedNow->valueType);
			out.dimmed = loading;
			return out;

		case HudMissingPolicy::RetainLastValid:
			if (lastValid && lastValid->present) {
				out.shouldRender = true;
				out.valueToShow = *lastValid;
				out.dimmed = loading;
				return out;
			}
			out.shouldRender = false;
			return out;

		case HudMissingPolicy::DimLastValid:
			if (lastValid && lastValid->present) {
				out.shouldRender = true;
				out.valueToShow = *lastValid;
				out.dimmed = true; // always dimmed, regardless of loading state
				return out;
			}
			out.shouldRender = false;
			return out;

		case HudMissingPolicy::UseFallbackSource:
			if (fallbackValue && fallbackValue->present) {
				out.shouldRender = true;
				out.valueToShow = *fallbackValue;
				out.dimmed = loading;
				return out;
			}
			out.shouldRender = false;
			return out;

		case HudMissingPolicy::UseAmbientFallback:
			out.shouldRender = true;
			out.useAmbientFallback = true;
			out.dimmed = false; // ambient content is not "dimmed", it's a different rendering mode entirely
			return out;
	}

	out.shouldRender = false;
	return out;
}

bool HudMissingDataController::regionNeedsAmbientFallback(const std::vector<Outcome>& outcomesInRegion) {
	if (outcomesInRegion.empty()) return false; // an unbound region is not "empty content", it's simply not this domain's concern
	for (const auto& outcome : outcomesInRegion) {
		if (outcome.shouldRender) return false; // at least one binding rendered something (real or its own ambient fallback)
	}
	return true;
}

} // namespace hudpresent
