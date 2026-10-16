#pragma once

#include <cstdint>

// TEMP-008B: which ambient texture a group (underlay / overlay) shows for one
// composition cycle. Pure and oF-free so test/tf_ambient_selection_tests.cpp
// can exercise it headless.
//
// The pick is derived from the cycle's own seed (TFComposition::cycleSeed,
// already drawn once per cycle via ofRandom) — this consumes NO random
// numbers itself, so every existing ofRandom/srand sequence in Temporal is
// unchanged. `salt` keeps the underlay and overlay picks independent.
// `previous` (-1 = none) avoids repeating the last cycle's pick when the
// group has more than one entry. Returns -1 when the group is empty.
namespace tfambient {

inline uint64_t mix64(uint64_t z) {
	// splitmix64 finalizer
	z += 0x9E3779B97F4A7C15ULL;
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
	return z ^ (z >> 31);
}

inline int pickForCycle(int count, int cycleSeed, uint64_t salt, int previous) {
	if (count <= 0) return -1;
	const uint64_t h = mix64(static_cast<uint64_t>(static_cast<uint32_t>(cycleSeed)) ^ mix64(salt));
	int index = static_cast<int>(h % static_cast<uint64_t>(count));
	if (count > 1 && index == previous) {
		const int step = 1 + static_cast<int>((h >> 32) % static_cast<uint64_t>(count - 1));
		index = (index + step) % count;
	}
	return index;
}

constexpr uint64_t kUnderlaySalt = 0x554E4445524C4159ULL; // "UNDERLAY"
constexpr uint64_t kOverlaySalt = 0x4F5645524C415953ULL;  // "OVERLAYS"

} // namespace tfambient
