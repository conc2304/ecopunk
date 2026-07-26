#pragma once

#include <vector>
#include <cmath>
#include "TimeOffsetVideoBuffer.h"
#include "ofMath.h"
#include "ofVec2f.h"

// Assigns each already-computed desired offset to a playhead in
// videoBuffer's pool, reusing a playhead whenever multiple fragments want
// the same quantized offset (the pool-sharing behavior every persistent-
// cell pattern needs, since the pool is a small fixed size while any of
// them can easily produce more fragments than that). Falls back to the
// closest already-assigned playhead if the pool is exhausted this frame.
//
// Factored out from tfAssignPlayheadsByNoise() below so patterns that don't
// want a noise-derived offset (Bands' Strata offset mode assigns a fixed,
// index-proportional offset instead — see TFPatternBands.cpp) can reuse the
// exact same pool-sharing policy without going through ofNoise() at all.
inline void tfAssignPlayheadsByDesiredOffsets(
	TimeOffsetVideoBuffer& videoBuffer,
	const std::vector<float>& desiredOffsets,
	std::vector<int>& outPlayheadIndices) {
	int numPlayheads = videoBuffer.getNumPlayheads();
	std::vector<float> assignedOffsets(numPlayheads, -1.0f);
	outPlayheadIndices.assign(desiredOffsets.size(), -1);

	for (size_t i = 0; i < desiredOffsets.size(); i++) {
		float desiredOffset = desiredOffsets[i];

		int chosen = -1;

		for (int p = 0; p < numPlayheads; p++) {
			if (assignedOffsets[p] >= 0.0f && assignedOffsets[p] == desiredOffset) {
				chosen = p;
				break;
			}
		}

		if (chosen < 0) {
			for (int p = 0; p < numPlayheads; p++) {
				if (assignedOffsets[p] < 0.0f) {
					chosen = p;
					assignedOffsets[p] = desiredOffset;
					videoBuffer.jumpPlayhead(p, desiredOffset);
					break;
				}
			}
		}

		if (chosen < 0) {
			float bestDist = 2.0f;
			for (int p = 0; p < numPlayheads; p++) {
				float d = std::abs(assignedOffsets[p] - desiredOffset);
				if (d < bestDist) {
					bestDist = d;
					chosen = p;
				}
			}
		}

		outPlayheadIndices[i] = chosen;
	}
}

// Assigns each fragment's noise-derived target offset to a playhead in
// videoBuffer's pool. Shared by TFPatternBSP and TFPatternBlobGrid — the
// pool-sharing logic is identical for both, only how each pattern
// enumerates its own fragments differs.
inline void tfAssignPlayheadsByNoise(
	TimeOffsetVideoBuffer& videoBuffer,
	float noiseTime,
	float noiseScale,
	const std::vector<ofVec2f>& normalizedCenters,
	std::vector<int>& outPlayheadIndices) {
	std::vector<float> desiredOffsets(normalizedCenters.size());
	for (size_t i = 0; i < normalizedCenters.size(); i++) {
		float gray = ofNoise(normalizedCenters[i].x * noiseScale, normalizedCenters[i].y * noiseScale, noiseTime);
		desiredOffsets[i] = videoBuffer.quantize(gray);
	}

	tfAssignPlayheadsByDesiredOffsets(videoBuffer, desiredOffsets, outPlayheadIndices);
}
