#pragma once

// ============================================================================
// HudPresentationTypes.h — renderer-private slot-binding types.
//
// Shapes below follow this task's own "Suggested conceptual shape"
// closely, with one documented deviation: HudSlotBinding::sources uses a
// fixed-capacity array instead of std::vector, per this task's own
// instruction ("Use fixed-size containers instead of vectors where the
// existing C++ conventions and Pi constraints make that preferable.
// Document the decision.") — see kMaxSources's comment below for why.
// ============================================================================

#include "HudWidgetTypes.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>

namespace hudpresent {

enum class HudMissingPolicy : uint8_t {
	Hide,
	Placeholder,
	RetainLastValid,
	DimLastValid,
	UseFallbackSource,
	UseAmbientFallback
};

enum class HudFormatKind : uint8_t {
	Automatic,
	Integer,
	Decimal,
	Percentage,
	Duration,
	Bytes,
	Temperature,
	VocabularyValue
};

// A named role within one binding ("primary", "label", "secondary", …) —
// the widget implementation looks sources up by role, never by position,
// so binding authors are free to omit optional roles or reorder them.
struct HudSourceRef {
	std::string role;
	std::string sourceId;
	bool required = true;
};

struct HudHistoryRequest {
	size_t sampleCount = 32;
	float sampleRateHz = 8.0f;
};

struct HudSlotBinding {
	// A wireframe-era binding needs at most: one primary value, one label,
	// one secondary/unit value, one fallback-adjacent extra role. None of
	// this task's 11 widget types bind more than 4 source roles (the
	// richest, MetadataCard, binds title+value+meta+optional-secondary —
	// 4 roles). Fixing the capacity at 4 keeps HudSlotBinding a
	// copy-cheap, allocation-free POD-shaped value (it is copied around
	// during profile compilation and by the Validation Studio's inspection
	// panels), matching the Pi-conscious "cap it, don't grow it"
	// convention this codebase already uses for SceneMetric-shaped lists
	// (see FakeHudSemanticTypes.h's kMaxSceneMetrics/kMaxActiveEffectIds).
	// addSource() reports capacity overflow explicitly rather than
	// silently dropping a source, so a binding author's mistake is
	// visible as a compile-time profile issue (see HudProfileCompiler),
	// not a silently-missing widget input.
	static constexpr size_t kMaxSources = 4;

	std::string bindingId;
	std::string regionId;
	HudWidgetType widgetType = HudWidgetType::Label;

	std::array<HudSourceRef, kMaxSources> sources{};
	size_t sourceCount = 0;

	std::optional<std::string> labelVocabularyId;
	HudFormatKind format = HudFormatKind::Automatic;
	HudMissingPolicy missingPolicy = HudMissingPolicy::Hide;
	std::optional<std::string> fallbackSourceId;
	std::optional<HudHistoryRequest> history;
	int priority = 0;
	std::string variantId;

	// Returns false without modifying the binding if kMaxSources is
	// already reached — callers (profile-authoring code, tests) can
	// treat that as a hard authoring error.
	bool addSource(HudSourceRef ref) {
		if (sourceCount >= kMaxSources) return false;
		sources[sourceCount++] = std::move(ref);
		return true;
	}

	const HudSourceRef* findSource(const std::string& role) const {
		for (size_t i = 0; i < sourceCount; ++i) {
			if (sources[i].role == role) return &sources[i];
		}
		return nullptr;
	}
};

} // namespace hudpresent
