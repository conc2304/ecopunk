#include "BlobEffectPolicy.h"

namespace blobeffects {

const std::vector<FragmentTreatment> & curatedFragmentTreatments() {
	static const std::vector<FragmentTreatment> kTreatments = {
		FragmentTreatment::original(),
		FragmentTreatment::shader("recolor"),
	};
	return kTreatments;
}

const std::string & backgroundEffectId() {
	static const std::string kBackground = "desaturate";
	return kBackground;
}

bool MediaEffectPolicy::observeMedia(const std::optional<std::string> & mediaId) {
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
	size_t previous = index_;
	index_ = (index_ + 1) % curatedFragmentTreatments().size();
	if (index_ == previous) {
		return false;
	}
	changeCount_++;
	return true;
}

const FragmentTreatment & MediaEffectPolicy::currentTreatment() const {
	return curatedFragmentTreatments()[index_];
}

} // namespace blobeffects
