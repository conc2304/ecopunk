#include "OfVideoDecoder.h"
#include "Rt004StallProbe.h" // RT-004 diagnostic probe (env-gated, no behavior change)

namespace {
// RT-004: pause before close. On the GStreamer backend (Pi),
// ofGstUtils::close() sends EOS to a playing, unpaused pipeline and blocks
// up to 5 s waiting for it ("didn't received EOS in 5s"); a paused pipeline
// skips that wait and goes straight to READY/NULL teardown. Draining to EOS
// serves no purpose for a decode-only pipeline. Same thread, same player,
// no GL work; harmless on AVFoundation.
void pauseThenClose(ofVideoPlayer& player) {
	if (player.isLoaded()) player.setPaused(true);
	player.close();
}
} // namespace

bool OfVideoDecoder::load(const std::string& absolutePath) {
	{ RT004_SCOPE("OfVideoDecoder.close"); pauseThenClose(player_); }
	bool ok;
	{ RT004_SCOPE("OfVideoDecoder.ofVideoPlayer.load"); ok = player_.load(absolutePath); }
	if (!ok) return false;
	player_.setLoopState(OF_LOOP_NORMAL);
	player_.setVolume(0);
	{ RT004_SCOPE("OfVideoDecoder.play"); player_.play(); }
	return true;
}

void OfVideoDecoder::update(float dt) {
	(void)dt;  // ofVideoPlayer::update() paces itself from the system clock
	if (player_.isLoaded()) { RT004_SCOPE("OfVideoDecoder.ofVideoPlayer.update"); player_.update(); }
}

void OfVideoDecoder::close() {
	pauseThenClose(player_);
}

float OfVideoDecoder::getPosition() const {
	if (!player_.isLoaded()) return -1.0f;
	return player_.getPosition();
}

float OfVideoDecoder::getDuration() const {
	if (!player_.isLoaded()) return 0.0f;
	return player_.getDuration();
}

bool OfVideoDecoder::seekToSeconds(float seconds) {
	if (!player_.isLoaded()) return false;
	const float duration = player_.getDuration();
	if (duration <= 0.0f || seconds < 0.0f || seconds >= duration) return false;
	player_.setPosition(seconds / duration);
	return true;
}

glm::ivec2 OfVideoDecoder::getSize() const {
	if (!player_.isLoaded()) return glm::ivec2(0, 0);
	return glm::ivec2(static_cast<int>(player_.getWidth()), static_cast<int>(player_.getHeight()));
}

const ofTexture* OfVideoDecoder::getTexture() const {
	if (!player_.isLoaded()) return nullptr;
	const ofTexture& tex = player_.getTexture();
	return tex.isAllocated() ? &tex : nullptr;
}

const ofPixels* OfVideoDecoder::getPixels() const {
	if (!player_.isLoaded()) return nullptr;
	const ofPixels& px = player_.getPixels();
	return px.isAllocated() ? &px : nullptr;
}
