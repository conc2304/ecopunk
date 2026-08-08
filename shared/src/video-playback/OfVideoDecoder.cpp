#include "OfVideoDecoder.h"

bool OfVideoDecoder::load(const std::string& absolutePath) {
	player_.close();
	bool ok = player_.load(absolutePath);
	if (!ok) return false;
	player_.setLoopState(OF_LOOP_NORMAL);
	player_.setVolume(0);
	player_.play();
	return true;
}

void OfVideoDecoder::update(float dt) {
	(void)dt;  // ofVideoPlayer::update() paces itself from the system clock
	if (player_.isLoaded()) player_.update();
}

void OfVideoDecoder::close() {
	player_.close();
}

float OfVideoDecoder::getPosition() const {
	if (!player_.isLoaded()) return -1.0f;
	return player_.getPosition();
}

float OfVideoDecoder::getDuration() const {
	if (!player_.isLoaded()) return 0.0f;
	return player_.getDuration();
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
