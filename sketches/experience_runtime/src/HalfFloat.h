// OC-003 experimental RGBA16F readback: EXACT IEEE-754 binary16 -> binary32 widening. OpenFrameworks-free.
//
// Every binary16 value (normal, subnormal, +-0, +-Inf, NaN) is exactly representable in binary32, so widening
// loses nothing: a float buffer produced by widening a half buffer is bit-identical to the float buffer a GL
// driver produces when it reads the same RGBA16F texels with GL_FLOAT (NaN payloads aside). Integer ops only,
// so the result does not depend on the FPU mode or on the host having a native half->float instruction.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace halffloat {

inline float toFloat(uint16_t h) {
	const uint32_t sign = static_cast<uint32_t>(h & 0x8000u) << 16;
	const uint32_t exp = (h >> 10) & 0x1Fu;
	uint32_t man = h & 0x3FFu;
	uint32_t bits;
	if (exp == 0) {
		if (man == 0) {
			bits = sign; // +-0
		} else { // subnormal: value = man * 2^-24; normalise so bit 10 is set
			int e = -1;
			do { man <<= 1; ++e; } while ((man & 0x400u) == 0);
			bits = sign | (static_cast<uint32_t>(112 - e) << 23) | ((man & 0x3FFu) << 13);
		}
	} else if (exp == 31) {
		bits = sign | 0x7F800000u | (man << 13); // Inf (man == 0) or NaN (payload kept, still NaN)
	} else {
		bits = sign | ((exp + 112u) << 23) | (man << 13);
	}
	float f;
	std::memcpy(&f, &bits, sizeof f);
	return f;
}

inline void widen(const uint16_t* src, float* dst, std::size_t count) {
	for (std::size_t i = 0; i < count; ++i) dst[i] = toFloat(src[i]);
}

} // namespace halffloat
