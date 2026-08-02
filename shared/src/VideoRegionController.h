#pragma once

#include "Fragment.h"
#include "ShaderLibrary.h"
#include "VideoRegion.h"
#include "VideoRegionEffectRenderer.h"
#include "VideoRegionMathOf.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Owns the active-region-to-fragment lifecycle: given each frame's tracked
// VideoRegions (from BlobTracker, or any future region source — a motion
// mask, a manual region, a HUD-selected region), it creates/updates/retires
// one Fragment per region, enforces the active-fragment cap, assigns a
// stable effect per persistent region id, and draws through
// VideoRegionEffectRenderer.
//
// Ownership boundary (see docs/blob-region-architecture.md): this class
// does NOT decode video, does NOT own a second ofVideoPlayer, does NOT run
// OpenCV, does NOT own a second ShaderLibrary, does NOT own temporal frame
// history, and is not a general scene manager — it only manages
// region-backed fragments and hands them to VideoRegionEffectRenderer.
class VideoRegionController {
public:
	struct Params {
		bool fragmentsEnabled = true;
		std::string effectName = "desaturate";
		float effectAmount = 1.0f;
		float fragmentScale = 1.0f; // drawMode 1
		float cropPadding = 0.0f; // normalized fraction; expands the source crop on each side
		int maxActiveFragments = 10;
		int drawMode = 0; // 0 = source-aligned overlay, 1 = scaled around center, 2 = offset/displaced
		float fragmentFadeSeconds = 0.25f;
	};

	// shaderLib: the canonical shared/src ShaderLibrary instance — not
	// owned, must outlive this controller.
	void setup(ShaderLibrary * shaderLibIn);

	// trackedRegions: this frame's BlobTracker::getActiveRegions() (or any
	// other region source producing the same type).
	// sourceTexture/sourceWidth/sourceHeight: the single shared video
	// texture and its native decode resolution.
	// displayDestRect: the screen rect the background video is drawn into
	// — used to keep region fragments aligned with the cover-fit background
	// (see VideoRegionMath::mapNormalizedSourceRectToScreen).
	void update(
		const std::vector<VideoRegion> & trackedRegions,
		const ofTexture & sourceTexture,
		int sourceWidth,
		int sourceHeight,
		const ofRectangle & displayDestRect,
		float dt);

	void draw();

	int getActiveFragmentCount() const;
	int getManagedRegionCount() const { return static_cast<int>(managed.size()); }
	int getScratchFboWidth() const { return effectRenderer.getScratchWidth(); }
	int getScratchFboHeight() const { return effectRenderer.getScratchHeight(); }

	Params params;

private:
	struct ManagedRegion {
		std::unique_ptr<Fragment> fragment;
		std::string effectName; // assigned once at creation, stable for the id's lifetime
		bool presentThisUpdate = false;
	};

	ShaderLibrary * shaderLib = nullptr;
	VideoRegionEffectRenderer effectRenderer;
	std::unordered_map<int, ManagedRegion> managed;

	std::string assignEffectForId(int id);
};
