#pragma once

// HudSkinPng — openFrameworks-independent structural check of a skin PNG's
// raw bytes, run BEFORE any decoder sees them (HUD-004 Skin Package
// Specification v1 §4: "Reject invalid signature, corruption/truncation,
// wrong decoded format/channels, or dimensions; manifest metadata alone is
// insufficient proof").
//
// Why a structural check exists at all, instead of trusting the decoder:
// openFrameworks' FreeImage path silently CONVERTS palette (indexed) and
// <8-bit images to 32-bit RGBA during decode (libs/openFrameworks/graphics/
// ofImage.cpp putBmpIntoPixels()), so a decoded-pixel check alone cannot
// tell a real RGBA8 PNG from an indexed one with transparency. The spec's
// "non-indexed RGBA8" requirement is therefore enforced here, on the IHDR
// header itself. Walking every chunk with its CRC also makes corruption and
// truncation fail deterministically, independent of how leniently a given
// decoder build handles a damaged stream.
//
// Plain C++17, no OF/GL — covered by test/hud_skin_loader_tests.cpp.

#include <cstddef>
#include <cstdint>
#include <string>

namespace hudskin {

namespace pngcodes {
	inline constexpr const char* SignatureInvalid = "SKIN_PNG_SIGNATURE_INVALID";
	inline constexpr const char* Truncated = "SKIN_PNG_TRUNCATED";
	inline constexpr const char* CrcMismatch = "SKIN_PNG_CRC_MISMATCH";
	inline constexpr const char* IhdrInvalid = "SKIN_PNG_IHDR_INVALID";
	inline constexpr const char* NotRgba8 = "SKIN_PNG_NOT_RGBA8";
	inline constexpr const char* NoImageData = "SKIN_PNG_NO_IDAT";
	inline constexpr const char* TrailingData = "SKIN_PNG_TRAILING_DATA";
} // namespace pngcodes

struct PngStructureResult {
	bool ok = false;
	std::string code;   // one of pngcodes::*, empty when ok
	std::string detail;
	uint32_t width = 0;  // from IHDR, valid when IHDR parsed
	uint32_t height = 0;
	uint8_t bitDepth = 0;
	uint8_t colorType = 0;
};

// Accepts exactly: PNG signature; IHDR first (length 13) with bit depth 8,
// color type 6 (RGBA — non-indexed truecolor with alpha), compression 0,
// filter 0, interlace 0 or 1; every chunk's CRC correct; at least one IDAT;
// IEND last with no bytes after it. Dimensions are reported, not judged —
// the caller compares them to the required 1280x720.
PngStructureResult checkPngStructure(const uint8_t* data, size_t size);

} // namespace hudskin
