#include "VideoRegionController.h"
#include <algorithm>

using VideoRegionMath::Rect;

namespace {

	Rect padRect(const Rect & r, float paddingFraction) {
		float padX = r.width * paddingFraction;
		float padY = r.height * paddingFraction;
		return Rect { r.x - padX, r.y - padY, r.width + 2.0f * padX, r.height + 2.0f * padY };
	}

}

void VideoRegionController::setup(ShaderLibrary * shaderLibIn) {
	shaderLib = shaderLibIn;
}

std::string VideoRegionController::assignEffectForId(int /*id*/) {
	// V1: every region uses the single configured effect. The per-id memo
	// still matters — see ManagedRegion::effectName's capture-at-creation
	// comment below — this is the seam a later per-blob effect-variety
	// feature would extend (e.g. hashing id into a small effect pool)
	// without touching VideoRegionController's lifecycle logic at all.
	return params.effectName;
}

void VideoRegionController::update(
	const std::vector<VideoRegion> & trackedRegions,
	const ofTexture & sourceTexture,
	int sourceWidth,
	int sourceHeight,
	const ofRectangle & displayDestRect,
	float dt) {
	if (sourceWidth <= 0 || sourceHeight <= 0) {
		return;
	}

	Rect destRect = VideoRegionMath::toRect(displayDestRect);
	Rect cropFillSourceRect = VideoRegionMath::computeCropFillSourceRect(
		static_cast<float>(sourceWidth), static_cast<float>(sourceHeight), destRect);

	// Index this frame's tracked regions by id for O(1) lookups below.
	std::unordered_map<int, const VideoRegion *> regionById;
	regionById.reserve(trackedRegions.size());
	for (const VideoRegion & r : trackedRegions) {
		regionById[r.id] = &r;
	}

	// Mark which currently-managed fragments are still tracked this frame.
	for (auto & kv : managed) {
		kv.second.presentThisUpdate = regionById.count(kv.first) > 0;
	}

	// Update or begin dissolving every currently-managed fragment.
	for (auto & kv : managed) {
		ManagedRegion & m = kv.second;
		if (m.presentThisUpdate) {
			const VideoRegion & region = *regionById[kv.first];
			Rect padded = padRect(VideoRegionMath::toRect(region.smoothedNormalizedBounds), params.cropPadding);
			Rect screenRect = VideoRegionMath::mapNormalizedSourceRectToScreen(
				padded, static_cast<float>(sourceWidth), static_cast<float>(sourceHeight), cropFillSourceRect, destRect);

			if (params.drawMode == 1) {
				float cx = screenRect.x + screenRect.width * 0.5f;
				float cy = screenRect.y + screenRect.height * 0.5f;
				screenRect.width *= params.fragmentScale;
				screenRect.height *= params.fragmentScale;
				screenRect.x = cx - screenRect.width * 0.5f;
				screenRect.y = cy - screenRect.height * 0.5f;
			} else if (params.drawMode == 2) {
				// Minimal displaced-fragment demonstration: offset by 60%
				// of the fragment's own width. Not tuned/final — V1 only
				// requires mode 0 to be correct (see task spec Part 2.10).
				screenRect.x += screenRect.width * 0.6f;
			}

			m.fragment->setBounds(VideoRegionMath::toOf(screenRect));
			m.fragment->setNormalizedSourceBounds(VideoRegionMath::toOf(padded), sourceWidth, sourceHeight);
			m.fragment->setId(kv.first);
		} else if (m.fragment->getState() != Fragment::State::DISSOLVING
			&& m.fragment->getState() != Fragment::State::DEAD) {
			// BlobTracker already applied its own missing-detection grace
			// period before dropping this id — by the time it disappears
			// from trackedRegions here, the region is genuinely gone. This
			// is a short cosmetic fade-out only, not a second grace period.
			m.fragment->startDissolve(params.fragmentFadeSeconds);
		}
		m.fragment->update(dt);
	}

	// Drop fragments that finished dissolving.
	for (auto it = managed.begin(); it != managed.end();) {
		if (it->second.fragment->isDead()) {
			it = managed.erase(it);
		} else {
			++it;
		}
	}

	// Room for new regions: cap counts every still-alive managed fragment
	// (present or still fading out), not just present ones, so the visible
	// fragment count never exceeds maxActiveFragments even mid-fade-out.
	int occupied = static_cast<int>(managed.size());
	int freeSlots = std::max(0, params.maxActiveFragments - occupied);

	if (freeSlots > 0) {
		std::vector<const VideoRegion *> candidates;
		for (const VideoRegion & r : trackedRegions) {
			if (managed.count(r.id) == 0) {
				candidates.push_back(&r);
			}
		}
		// Prioritize larger/more visually significant blobs when there are
		// more new candidates than free slots.
		std::sort(candidates.begin(), candidates.end(), [](const VideoRegion * a, const VideoRegion * b) {
			return a->smoothedNormalizedBounds.width * a->smoothedNormalizedBounds.height
				> b->smoothedNormalizedBounds.width * b->smoothedNormalizedBounds.height;
		});

		int toCreate = std::min(freeSlots, static_cast<int>(candidates.size()));
		for (int i = 0; i < toCreate; i++) {
			const VideoRegion & region = *candidates[i];

			Rect padded = padRect(VideoRegionMath::toRect(region.smoothedNormalizedBounds), params.cropPadding);
			Rect screenRect = VideoRegionMath::mapNormalizedSourceRectToScreen(
				padded, static_cast<float>(sourceWidth), static_cast<float>(sourceHeight), cropFillSourceRect, destRect);

			Fragment::Params p;
			p.bounds = VideoRegionMath::toOf(screenRect);
			p.placeholderColor = ofColor(60, 60, 60);
			p.phaseOffset = 0.0f;
			p.arrivalDuration = std::max(0.01f, params.fragmentFadeSeconds);
			p.driftAmp = glm::vec2(0.0f, 0.0f); // region-driven fragments track the blob, not ambient drift
			p.driftFreq = glm::vec2(0.0f, 0.0f);
			p.desaturateRampDuration = 1.0f;
			p.desaturateMax = 0.0f; // the built-in Fragment shader's own desaturate stays off; ShaderLibrary effect is separate
			p.circularMask = false;
			p.maskRadius = 0.0f;

			ManagedRegion m;
			m.fragment = std::make_unique<Fragment>();
			m.fragment->setup(p);
			m.fragment->setVideoSource(&sourceTexture, ofRectangle());
			m.fragment->setNormalizedSourceBounds(VideoRegionMath::toOf(padded), sourceWidth, sourceHeight);
			m.fragment->setId(region.id);
			m.effectName = assignEffectForId(region.id);
			m.presentThisUpdate = true;

			managed.emplace(region.id, std::move(m));
		}
	}
}

void VideoRegionController::draw() {
	if (!params.fragmentsEnabled || shaderLib == nullptr) {
		return;
	}

	for (auto & kv : managed) {
		Fragment & f = *kv.second.fragment;
		if (f.getState() == Fragment::State::DEAD) {
			continue;
		}

		VideoRegionEffectRenderer::RenderRequest req;
		req.sourceTexture = f.getVideoTexture();
		req.sourceCropPixels = f.getVideoCrop();
		req.destinationBounds = f.getBounds();
		req.alpha = f.getOpacity();
		req.effectName = kv.second.effectName;
		req.effectAmount = params.effectAmount;

		effectRenderer.render(req, *shaderLib);
	}
}

void VideoRegionController::reset() {
	managed.clear();
}

int VideoRegionController::getActiveFragmentCount() const {
	int count = 0;
	for (const auto & kv : managed) {
		if (kv.second.fragment->getState() != Fragment::State::DEAD) {
			count++;
		}
	}
	return count;
}
