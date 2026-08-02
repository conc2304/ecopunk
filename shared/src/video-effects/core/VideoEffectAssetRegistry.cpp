#include "VideoEffectAssetRegistry.h"
#include "ofFileUtils.h"
#include "ofUtils.h"
#include <utility>

namespace videoeffects {

	VideoEffectAssetRegistry::VideoEffectAssetRegistry(std::string managedSubdir_)
		: managedSubdir(std::move(managedSubdir_)) {}

	std::string VideoEffectAssetRegistry::resolve(const std::string & canonicalRelativePath) const {
		return managedSubdir + "/" + canonicalRelativePath;
	}

	bool VideoEffectAssetRegistry::exists(const std::string & canonicalRelativePath) const {
		return ofFile(ofToDataPath(resolve(canonicalRelativePath), true)).exists();
	}

} // namespace videoeffects
