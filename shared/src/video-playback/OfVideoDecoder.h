#pragma once

#include "IVideoDecoder.h"

#include "ofVideoPlayer.h"

// Concrete IVideoDecoder backed by exactly one ofVideoPlayer — the same
// "one ofVideoPlayer, hard rule, kept for parity between Mac and Pi"
// constraint every other wrapper in this repo already follows (see
// shared/src/VideoSampler.h's header comment). OF_LOOP_NORMAL + muted
// volume, matching VideoSampler.cpp/TimeOffsetVideoBuffer.cpp/
// VideoSystem.cpp's own setup convention.
class OfVideoDecoder : public IVideoDecoder {
public:
	bool load(const std::string& absolutePath) override;
	void update(float dt) override;
	void close() override;

	bool isLoaded() const override { return player_.isLoaded(); }
	bool isFrameNew() const override { return player_.isFrameNew(); }

	float getPosition() const override;
	float getDuration() const override;

	glm::ivec2 getSize() const override;
	const ofTexture* getTexture() const override;
	const ofPixels* getPixels() const override;

private:
	ofVideoPlayer player_;
};
