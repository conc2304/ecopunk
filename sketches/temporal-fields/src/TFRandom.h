#pragma once

#include <algorithm>
#include <cstdlib>
#include <utility>
#include <vector>

// std::rand()-based (not ofRandom()) so pattern geometry is reproducible
// from TFComposition's srand(cycleSeed) call — ofRandom() runs its own
// independent generator that srand() doesn't affect. Shared by both
// pattern implementations (TFPatternBSP, TFPatternBlobGrid).
inline float tfRandRangeF(float lo, float hi) {
	return lo + static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * (hi - lo);
}

inline int tfRandRangeI(int lo, int hiInclusive) {
	return lo + std::rand() % (hiInclusive - lo + 1);
}

// Weighted-random pick from a named/weighted list — generalizes
// tfPickTransitionStyle's 3-fixed-option pattern (TFFragmentTransition.cpp)
// to any list, e.g. the 17-way effect picker and the 3-way background mode
// picker.
template <typename T>
inline T tfWeightedPick(const std::vector<std::pair<T, float>>& options) {
	float total = 0.0f;
	for (auto& o : options) {
		total += std::max(0.0f, o.second);
	}
	if (total <= 0.0001f) {
		return options.front().first;
	}
	float r = tfRandRangeF(0.0f, total);
	for (auto& o : options) {
		if (r < o.second) {
			return o.first;
		}
		r -= o.second;
	}
	return options.back().first;
}
