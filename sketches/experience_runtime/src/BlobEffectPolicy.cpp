#include "BlobEffectPolicy.h"

namespace blobeffects {

const std::string & backgroundEffectId() {
	static const std::string kBackground = "desaturate";
	return kBackground;
}

bool MediaGeneration::observeMedia(const std::optional<std::string> & mediaId) {
	if (!mediaId.has_value()) {
		return false;
	}
	if (!lastMediaId_.has_value()) {
		lastMediaId_ = mediaId;
		return false;
	}
	if (*lastMediaId_ == *mediaId) {
		return false;
	}
	lastMediaId_ = mediaId;
	generation_++;
	return true;
}

} // namespace blobeffects
