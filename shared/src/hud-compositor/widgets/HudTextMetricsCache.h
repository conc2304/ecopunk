#pragma once

// ============================================================================
// HudTextMetricsCache.h — cached text measurement + deterministic
// truncation for this task's §11 "enough text infrastructure for
// wireframe acceptance."
//
// Deliberately does NOT introduce real TTF font loading — the canonical
// shared/src/hud/ widget library (per docs/probes/hud-runtime-validation-
// studio-probe.md §9) never did either, and this increment's widget layer
// already committed to the same ofDrawBitmapString fallback path (see
// HudWidgetDrawUtils.h's header comment) for parity with that precedent.
// "Do not freeze final skin typography" (this task's own non-goal) — this
// is a wireframe-acceptance-grade measurement/truncation layer over OF's
// existing fixed bitmap font, not a font system.
//
// Measurement approach, and why it changed mid-session: the first version
// of this file called ofBitmapFont::getBoundingBox() (a real, if
// internal-facing, OF API — ofBitmapStringGetBoundingBox(), a free
// function, does NOT exist in this OF version, confirmed by direct header
// search) for real measurement. That crashed (SIGSEGV) in this domain's
// headless, no-window/no-GL-context standalone test binary — ofBitmapFont
// touches texture/pixel initialization that isn't safe without a live GL
// context. Rather than require a window just to measure text (which would
// also make this cache unusable from update()-time layout decisions, not
// just draw()-time), this measures against the font's own KNOWN, fixed
// metrics instead: direct inspection of
// libs/openFrameworks/graphics/ofBitmapFont.cpp confirms OF's bitmap font
// is freeglut's classic 8x13 fixed-width font — every glyph is exactly 8
// local pixels wide (the literal first byte of every `bmpChar_8x13_*`
// glyph array in that file is `8`), monospace, no extra inter-character
// spacing. `kGlyphWidthLocal` below is that verified, not guessed,
// constant — this measurement is exact for this font, not an
// approximation, and (a genuine bonus) needs no GL context at all, so it
// works identically whether called from update() or draw(), and from a
// real windowed app or this domain's headless test suite.
// ============================================================================

#include <cstddef>
#include <string>
#include <unordered_map>

namespace hudpresent {

class HudTextMetricsCache {
public:
	// The freeglut/OF fixed bitmap font's exact per-glyph local-pixel
	// advance width — see this file's header comment for how this was
	// verified (not assumed) against libs/openFrameworks/graphics/
	// ofBitmapFont.cpp's own glyph data.
	static constexpr float kGlyphWidthLocal = 8.0f;

	// Full-string width, in the bitmap font's own local (unscaled) pixel
	// units. Cached by `text` alone (trivial to compute — kGlyphWidthLocal
	// * text.size() — but still cached so every call site can treat this
	// uniformly with truncateToWidth() below, and so a future real-font
	// swap only needs to change this one function's body).
	float widthOf(const std::string& text) const;

	struct TruncateResult {
		std::string text;    // original text, or an ellipsis-truncated prefix
		bool truncated = false;
		float widthLocal = 0.0f; // local-unit width of `text` as returned
	};

	// Deterministically truncates `text` (appending "...") so its LOCAL
	// width fits within `maxLocalWidth` — never truncates mid-render, no
	// per-frame re-measurement once cached. A `maxLocalWidth` too small
	// even for "..." alone returns an empty string (never negative-width
	// or partial-glyph output). Cached by (text, maxLocalWidth rounded to
	// the nearest local pixel) — this task's own "cache invalidation only
	// when text/font/region constraints change" requirement: a region
	// resize changes maxLocalWidth and correctly invalidates just that
	// key, not the whole cache.
	const TruncateResult& truncateToWidth(const std::string& text, float maxLocalWidth) const;

	// Clears both caches — call when text/font/region constraints change
	// in a way this class can't key on itself (e.g. a hypothetical future
	// real-font swap). Not required for a canvas-size or per-binding
	// change, which the (text, maxLocalWidth) key already accounts for.
	void clear();

	size_t widthCacheSize() const { return widthCache_.size(); }
	size_t truncateCacheSize() const { return truncateCache_.size(); }

private:
	mutable std::unordered_map<std::string, float> widthCache_;

	struct TruncateKey {
		std::string text;
		int maxLocalWidthRounded;
		bool operator==(const TruncateKey& other) const {
			return text == other.text && maxLocalWidthRounded == other.maxLocalWidthRounded;
		}
	};
	struct TruncateKeyHash {
		size_t operator()(const TruncateKey& k) const {
			return std::hash<std::string>()(k.text) ^ (std::hash<int>()(k.maxLocalWidthRounded) << 1);
		}
	};
	mutable std::unordered_map<TruncateKey, TruncateResult, TruncateKeyHash> truncateCache_;
};

} // namespace hudpresent
