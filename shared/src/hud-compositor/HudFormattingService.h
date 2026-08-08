#pragma once

// ============================================================================
// HudFormattingService.h — turns a resolved value + HudFormatKind into
// display text.
//
// Pure functions, no state, no caching needed here (unlike vocabulary
// resolution, formatting a live numeric value is cheap and must reflect
// the current frame's value, so there is nothing to usefully cache) — see
// this task's §8 "Cache resolved strings" instruction, which applies to
// HudVocabularyResolver, not this service. VocabularyValue-kind formatting
// is the one exception: it defers to a HudVocabularyResolver, which does
// its own caching.
// ============================================================================

#include "HudDataTypes.h"
#include "HudPresentationTypes.h"
#include "HudVocabularyResolver.h"

#include <string>

namespace hudpresent {

class HudFormattingService {
public:
	// `sceneId` is forwarded only to the vocabulary resolver, for
	// VocabularyValue-kind formatting's scene-override tier — this
	// function itself does not branch on it.
	static std::string format(const HudResolvedValue& value, HudFormatKind kind,
		const std::string& sceneId, const HudVocabularyResolver& vocabulary);

private:
	static std::string formatInteger(float v);
	static std::string formatDecimal(float v);
	static std::string formatPercentage(float ratio01);
	static std::string formatDuration(float seconds);
	static std::string formatBytes(float bytes);
	static std::string formatTemperature(float celsius);
	static std::string formatAutomatic(const HudResolvedValue& value, const std::string& sceneId, const HudVocabularyResolver& vocabulary);
};

} // namespace hudpresent
