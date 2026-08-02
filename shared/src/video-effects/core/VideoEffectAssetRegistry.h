#pragma once

#include <string>

// Resolves a VideoEffectDefinition's canonical-relative asset paths
// (VideoEffectDefinition::assetPaths) into paths ofShader::load() can use,
// rooted at the CURRENT sketch's own bin/data/<managedSubdir>/ — populated
// ahead of time by scripts/sync-video-effect-assets.py. This class never
// reads shared/assets/video-effects/ directly and never escapes the current
// sketch's own data directory, per docs/shared-video-effect-architecture.md §7
// ("No shared effect definition should hardcode a sketch name or escape the
// current sketch's bin/data").
namespace videoeffects {

	class VideoEffectAssetRegistry {
	public:
		explicit VideoEffectAssetRegistry(std::string managedSubdir = "shared-video-effects");

		// e.g. resolve("single-pass/heatmap_recolor.glsl") -> "shared-video-effects/single-pass/heatmap_recolor.glsl"
		std::string resolve(const std::string & canonicalRelativePath) const;

		// True if the resolved path actually exists under this sketch's bin/data.
		bool exists(const std::string & canonicalRelativePath) const;

	private:
		std::string managedSubdir;
	};

} // namespace videoeffects
