#pragma once

#include <vector>
#include <cmath>
#include "TimeOffsetVideoBuffer.h"
#include "ofMath.h"
#include "ofVec2f.h"

// Assigns each fragment's noise-derived target offset to a playhead in
// videoBuffer's pool, reusing a playhead whenever multiple fragments want
// the same quantized offset (the pool-sharing behavior both patterns need,
// since the pool is a small fixed size while either pattern can easily
// produce more fragments than that). Falls back to the closest
// already-assigned playhead if the pool is exhausted this frame. Shared by
// TFPatternBSP and TFPatternBlobGrid — the pool-sharing logic is identical
// for both, only how each pattern enumerates its own fragments differs.
inline void tfAssignPlayheadsByNoise(
	TimeOffsetVideoBuffer& videoBuffer,
	float noiseTime,
	float noiseScale,
	const std::vector<ofVec2f>& normalizedCenters,
	std::vector<int>& outPlayheadIndices) {
	int numPlayheads = videoBuffer.getNumPlayheads();
	std::vector<float> assignedOffsets(numPlayheads, -1.0f);
	outPlayheadIndices.assign(normalizedCenters.size(), -1);

	for (size_t i = 0; i < normalizedCenters.size(); i++) {
		float gray = ofNoise(normalizedCenters[i].x * noiseScale, normalizedCenters[i].y * noiseScale, noiseTime);
		float desiredOffset = videoBuffer.quantize(gray);

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
