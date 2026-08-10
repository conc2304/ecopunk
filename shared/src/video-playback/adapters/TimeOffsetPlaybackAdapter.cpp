#include "TimeOffsetPlaybackAdapter.h"

void TimeOffsetPlaybackAdapter::setup(const TimeOffsetVideoBuffer::Settings& settings) {
	buffer_.configure(settings);
}

bool TimeOffsetPlaybackAdapter::synchronizeSelectedMedia(const std::string& mediaId, const std::string& absolutePath) {
	if (!loadedMediaId_.empty() && mediaId == loadedMediaId_) {
		// Same media already loaded (or already the last-attempted one) —
		// no-op, per this class's own "unchanged mediaId does not reload"
		// contract. Neither buffer_ nor the counters below are touched.
		return lastLoadSucceeded_;
	}

	bool ok = buffer_.loadExplicit(absolutePath);
	loadedMediaId_ = mediaId;
	lastLoadSucceeded_ = ok;
	reloadCount_++;
	return ok;
}

void TimeOffsetPlaybackAdapter::update(float dt) {
	buffer_.update(dt);
}
