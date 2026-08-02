#pragma once

#include "ofFbo.h"
#include "ofPixels.h"
#include "ofRectangle.h"
#include "ofTexture.h"
#include <map>
#include <string>

// Everything a VideoEffectInstance::render() needs for one frame, per
// docs/shared-video-effect-architecture.md §5. Deliberately does not carry
// composition/layout/mask-ownership concerns — those stay sketch-owned
// (architecture doc §13); this struct is just the render inputs/outputs for
// a single effect application.
namespace videoeffects {

	struct VideoEffectContext {
		const ofTexture * sourceTexture = nullptr;
		const ofPixels * sourcePixels = nullptr; // for CpuRenderer effects (e.g. ridgeline)
		const ofTexture * maskTexture = nullptr; // optional

		ofRectangle sourceRect; // in source texture space; zero-size = full texture
		ofRectangle destinationRect; // where to draw

		float time = 0.0f;
		float deltaTime = 0.0f;
		float alpha = 1.0f;

		// Named auxiliary textures an effect may need beyond source/mask (e.g.
		// "motionTex"/"motionDelayedTex" for motion_composite). Non-owning —
		// the caller keeps these alive for the duration of the render() call.
		std::map<std::string, const ofTexture *> auxiliaryTextures;

		ofFbo * destinationFbo = nullptr; // optional; null = draw to whatever's currently bound
	};

} // namespace videoeffects
