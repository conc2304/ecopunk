#include "HudFormattingService.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace hudpresent {

namespace {

std::string sprintfString(const char* fmt, double v) {
	char buf[64];
	std::snprintf(buf, sizeof(buf), fmt, v);
	return std::string(buf);
}

} // namespace

std::string HudFormattingService::formatInteger(float v) {
	return sprintfString("%.0f", static_cast<double>(std::lround(v)));
}

std::string HudFormattingService::formatDecimal(float v) {
	return sprintfString("%.1f", static_cast<double>(v));
}

std::string HudFormattingService::formatPercentage(float ratio01) {
	float clamped = std::clamp(ratio01, 0.0f, 1.0f);
	return sprintfString("%.0f%%", static_cast<double>(clamped * 100.0f));
}

std::string HudFormattingService::formatDuration(float seconds) {
	if (seconds < 0.0f) seconds = 0.0f;
	int totalSeconds = static_cast<int>(seconds + 0.5f);
	if (totalSeconds < 60) {
		return std::to_string(totalSeconds) + "s";
	}
	int minutes = totalSeconds / 60;
	int secs = totalSeconds % 60;
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%d:%02d", minutes, secs);
	return std::string(buf);
}

std::string HudFormattingService::formatBytes(float bytes) {
	static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
	double value = static_cast<double>(bytes);
	int unitIndex = 0;
	while (value >= 1024.0 && unitIndex < 4) {
		value /= 1024.0;
		unitIndex++;
	}
	char buf[32];
	if (unitIndex == 0) {
		std::snprintf(buf, sizeof(buf), "%.0f%s", value, units[unitIndex]);
	} else {
		std::snprintf(buf, sizeof(buf), "%.1f%s", value, units[unitIndex]);
	}
	return std::string(buf);
}

std::string HudFormattingService::formatTemperature(float celsius) {
	return sprintfString("%.0f", static_cast<double>(celsius)) + "C";
}

std::string HudFormattingService::formatAutomatic(const HudResolvedValue& value, const std::string& sceneId, const HudVocabularyResolver& vocabulary) {
	switch (value.valueType) {
		case HudSourceValueType::Scalar:
			return formatDecimal(value.numberValue);
		case HudSourceValueType::Count:
			return formatInteger(value.numberValue);
		case HudSourceValueType::Ratio:
			return formatPercentage(value.numberValue);
		case HudSourceValueType::DurationSeconds:
			return formatDuration(value.numberValue);
		case HudSourceValueType::Boolean:
			return value.boolValue ? "TRUE" : "FALSE";
		case HudSourceValueType::Identifier:
			return vocabulary.resolve(sceneId, value.textValue);
		case HudSourceValueType::Text:
			return value.textValue;
		case HudSourceValueType::IdentifierList: {
			std::string out;
			for (size_t i = 0; i < value.listValue.count; ++i) {
				if (i > 0) out += ", ";
				out += vocabulary.resolve(sceneId, value.listValue.items[i]);
			}
			return out;
		}
	}
	return "";
}

std::string HudFormattingService::format(const HudResolvedValue& value, HudFormatKind kind,
	const std::string& sceneId, const HudVocabularyResolver& vocabulary) {
	if (!value.present) return "";

	switch (kind) {
		case HudFormatKind::Automatic:
			return formatAutomatic(value, sceneId, vocabulary);
		case HudFormatKind::Integer:
			return formatInteger(value.numberValue);
		case HudFormatKind::Decimal:
			return formatDecimal(value.numberValue);
		case HudFormatKind::Percentage:
			return formatPercentage(value.numberValue);
		case HudFormatKind::Duration:
			return formatDuration(value.numberValue);
		case HudFormatKind::Bytes:
			return formatBytes(value.numberValue);
		case HudFormatKind::Temperature:
			return formatTemperature(value.numberValue);
		case HudFormatKind::VocabularyValue:
			// Engineering Session 2 fix: VocabularyValue must only resolve
			// Identifier-typed values through the vocabulary resolver.
			// Text-typed values mean "already final text" by this domain's
			// own type-system convention (HudSourceValueType's own doc
			// comment) — the real SceneHudStatus::displayName/
			// VideoPlaybackStatus::fallbackDisplayTitle/mediaId are exactly
			// this case (see HudSourceResolver.cpp's resolveMediaTitle()).
			// Running literal display text through vocabulary.resolve()
			// would previously "work" only by accident, via the
			// resolution chain's own stable-ID fallback tier returning the
			// input unchanged — semantically wrong (a wasted, misleading
			// lookup attempt), fixed here rather than left relying on that
			// coincidence.
			if (value.valueType == HudSourceValueType::Text) return value.textValue;
			return vocabulary.resolve(sceneId, value.textValue);
	}
	return "";
}

} // namespace hudpresent
