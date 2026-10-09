#include "HudSkinPng.h"

#include <array>
#include <cstring>

namespace hudskin {

namespace {

// Standard CRC-32 (ISO 3309 / PNG spec Annex D), table-driven.
const std::array<uint32_t, 256>& crcTable() {
	static const std::array<uint32_t, 256> table = [] {
		std::array<uint32_t, 256> t{};
		for (uint32_t n = 0; n < 256; ++n) {
			uint32_t c = n;
			for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
			t[n] = c;
		}
		return t;
	}();
	return table;
}

uint32_t crc32(const uint8_t* data, size_t len) {
	uint32_t c = 0xFFFFFFFFu;
	const auto& t = crcTable();
	for (size_t i = 0; i < len; ++i) c = t[(c ^ data[i]) & 0xFF] ^ (c >> 8);
	return c ^ 0xFFFFFFFFu;
}

uint32_t readBE32(const uint8_t* p) {
	return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

PngStructureResult fail(PngStructureResult r, const char* code, std::string detail) {
	r.ok = false;
	r.code = code;
	r.detail = std::move(detail);
	return r;
}

} // namespace

PngStructureResult checkPngStructure(const uint8_t* data, size_t size) {
	static const uint8_t kSignature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
	PngStructureResult r;

	if (size < 8 || std::memcmp(data, kSignature, 8) != 0) {
		return fail(r, pngcodes::SignatureInvalid, "missing or wrong 8-byte PNG signature");
	}

	size_t pos = 8;
	bool first = true;
	bool sawIdat = false;
	while (true) {
		if (size - pos < 12) { // length + type + crc, with empty data
			return fail(r, pngcodes::Truncated, "file ends before IEND (incomplete chunk header at byte " + std::to_string(pos) + ")");
		}
		const uint32_t length = readBE32(data + pos);
		const uint8_t* type = data + pos + 4;
		if (length > 0x7FFFFFFFu || size - pos - 12 < length) {
			return fail(r, pngcodes::Truncated, "chunk at byte " + std::to_string(pos) + " extends past end of file");
		}
		const uint8_t* body = type + 4;
		const uint32_t storedCrc = readBE32(body + length);
		if (crc32(type, 4 + size_t(length)) != storedCrc) {
			return fail(r, pngcodes::CrcMismatch, "CRC mismatch in chunk '" + std::string(reinterpret_cast<const char*>(type), 4) + "'");
		}
		const std::string typeName(reinterpret_cast<const char*>(type), 4);

		if (first) {
			if (typeName != "IHDR" || length != 13) {
				return fail(r, pngcodes::IhdrInvalid, "first chunk must be a 13-byte IHDR");
			}
			r.width = readBE32(body);
			r.height = readBE32(body + 4);
			r.bitDepth = body[8];
			r.colorType = body[9];
			const uint8_t compression = body[10], filter = body[11], interlace = body[12];
			if (r.width == 0 || r.height == 0 || compression != 0 || filter != 0 || interlace > 1) {
				return fail(r, pngcodes::IhdrInvalid, "invalid IHDR fields");
			}
			if (r.bitDepth != 8 || r.colorType != 6) {
				return fail(r, pngcodes::NotRgba8,
					"PNG must be non-indexed 8-bit RGBA (color type 6, bit depth 8); got color type " +
					std::to_string(r.colorType) + ", bit depth " + std::to_string(r.bitDepth));
			}
			first = false;
		} else if (typeName == "IHDR") {
			return fail(r, pngcodes::IhdrInvalid, "duplicate IHDR");
		}

		if (typeName == "IDAT") sawIdat = true;
		pos += 12 + size_t(length);

		if (typeName == "IEND") {
			if (!sawIdat) return fail(r, pngcodes::NoImageData, "no IDAT chunk before IEND");
			if (pos != size) {
				return fail(r, pngcodes::TrailingData, std::to_string(size - pos) + " byte(s) after IEND");
			}
			r.ok = true;
			return r;
		}
	}
}

} // namespace hudskin
