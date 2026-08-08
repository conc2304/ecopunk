#include "HudTextMetricsCache.h"

namespace hudpresent {

float HudTextMetricsCache::widthOf(const std::string& text) const {
	auto it = widthCache_.find(text);
	if (it != widthCache_.end()) return it->second;
	float width = kGlyphWidthLocal * static_cast<float>(text.size());
	widthCache_.emplace(text, width);
	return width;
}

const HudTextMetricsCache::TruncateResult& HudTextMetricsCache::truncateToWidth(const std::string& text, float maxLocalWidth) const {
	TruncateKey key{text, static_cast<int>(maxLocalWidth + 0.5f)};
	auto it = truncateCache_.find(key);
	if (it != truncateCache_.end()) return it->second;

	TruncateResult result;
	float fullWidth = widthOf(text);
	if (fullWidth <= maxLocalWidth || text.empty()) {
		result.text = text;
		result.truncated = false;
		result.widthLocal = fullWidth;
	} else {
		static const std::string kEllipsis = "...";
		float ellipsisWidth = widthOf(kEllipsis);
		if (ellipsisWidth > maxLocalWidth) {
			// Not even "..." fits — deterministic empty output, never a
			// partial/garbled glyph.
			result.text = "";
			result.truncated = true;
			result.widthLocal = 0.0f;
		} else {
			// Deterministic, linear shrink: drop one character at a time
			// from the end until "<prefix>..." fits. Bounded by text
			// length (never loops more than text.size() times) — cheap
			// for the short label-shaped strings this domain's widgets
			// actually draw (state names, titles, metric labels), and
			// the RESULT is what's cached, so this loop runs once per
			// distinct (text, maxLocalWidth) pair, never per frame.
			std::string prefix = text;
			while (!prefix.empty()) {
				prefix.pop_back();
				float candidateWidth = widthOf(prefix) + ellipsisWidth;
				if (candidateWidth <= maxLocalWidth) {
					result.text = prefix + kEllipsis;
					result.truncated = true;
					result.widthLocal = candidateWidth;
					break;
				}
			}
			if (prefix.empty()) {
				result.text = kEllipsis;
				result.truncated = true;
				result.widthLocal = ellipsisWidth;
			}
		}
	}

	auto inserted = truncateCache_.emplace(key, std::move(result));
	return inserted.first->second;
}

void HudTextMetricsCache::clear() {
	widthCache_.clear();
	truncateCache_.clear();
}

} // namespace hudpresent
