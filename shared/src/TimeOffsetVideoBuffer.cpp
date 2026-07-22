#include "TimeOffsetVideoBuffer.h"
#include "ofFileUtils.h"
#include "ofLog.h"
#include "ofMath.h"
#include <algorithm>
#include <cmath>

void TimeOffsetVideoBuffer::setup(const std::string& mediaFolderPath, const Settings& settings_) {
	settings = settings_;
	playheads.assign(std::max(1, settings.numPlayheads), Playhead{});
	history.clear();

	ofDirectory dir;
	dir.allowExt("mp4");
	dir.listDir(mediaFolderPath);
	dir.sort();

	mediaFiles.clear();
	for (const auto& file : dir.getFiles()) {
		mediaFiles.push_back(file.getAbsolutePath());
	}

	currentFileIndex = -1;
	mediaLoaded = false;

	if (mediaFiles.empty()) {
		ofLogWarning("TimeOffsetVideoBuffer") << "no .mp4 files found in " << mediaFolderPath;
		return;
	}

	currentFileIndex = 0;
	mediaLoaded = player.load(mediaFiles[currentFileIndex]);
	mediaElapsedTime = 0.0f;
	mediaLoopCount = 0;
	lastMediaPosition = 0.0f;
	if (mediaLoaded) {
		player.setLoopState(OF_LOOP_NORMAL);
		player.play();
		ofLogNotice("TimeOffsetVideoBuffer") << "found " << mediaFiles.size() << " file(s) in " << mediaFolderPath
			<< " — loaded " << mediaFiles[currentFileIndex]
			<< " — buffering " << settings.maxHistorySeconds << "s at "
			<< settings.bufferWidth << "x" << settings.bufferHeight;
	} else {
		ofLogWarning("TimeOffsetVideoBuffer") << "failed to load " << mediaFiles[currentFileIndex];
	}
}

void TimeOffsetVideoBuffer::advanceToNextMedia() {
	if (mediaFiles.size() < 2) {
		return;
	}

	currentFileIndex = (currentFileIndex + 1) % static_cast<int>(mediaFiles.size());
	history.clear(); // buffered frames are from the previous source — stale, discard

	player.close();
	mediaLoaded = player.load(mediaFiles[currentFileIndex]);
	mediaElapsedTime = 0.0f;
	mediaLoopCount = 0;
	lastMediaPosition = 0.0f;
	if (mediaLoaded) {
		player.setLoopState(OF_LOOP_NORMAL);
		player.play();
		ofLogNotice("TimeOffsetVideoBuffer") << "advanced to " << mediaFiles[currentFileIndex];
	} else {
		ofLogWarning("TimeOffsetVideoBuffer") << "failed to load " << mediaFiles[currentFileIndex];
	}
}

std::string TimeOffsetVideoBuffer::getCurrentMediaFilename() const {
	if (currentFileIndex < 0 || currentFileIndex >= static_cast<int>(mediaFiles.size())) {
		return "";
	}
	return mediaFiles[currentFileIndex];
}

bool TimeOffsetVideoBuffer::isMediaAdvanceEligible() const {
	if (!mediaLoaded) {
		return true; // nothing playing — don't block a caller waiting on this
	}
	return mediaElapsedTime >= settings.minPlaytimeSeconds && mediaLoopCount >= settings.minLoopCount;
}

void TimeOffsetVideoBuffer::update(float dt) {
	if (!mediaLoaded) {
		return;
	}

	player.update();

	if (player.isFrameNew()) {
		ofPixels frame = player.getPixels();
		frame.resize(settings.bufferWidth, settings.bufferHeight);
		history.push_front(std::move(frame));

		size_t maxFrames = static_cast<size_t>(settings.maxHistorySeconds * settings.assumedSourceFps);
		while (history.size() > maxFrames) {
			history.pop_back();
		}

		mediaElapsedTime += dt;
		float position = player.getPosition();
		if (position < lastMediaPosition - 0.5f) {
			// Position dropped by more than half a cycle in one frame —
			// only happens on a loop wrap (forward playback otherwise only
			// ever inches position up), so this is safe against variable
			// per-frame position deltas.
			mediaLoopCount++;
		}
		lastMediaPosition = position;
	}

	for (auto& ph : playheads) {
		if (ph.motion == PlayheadMotion::RAMP) {
			float step = ph.rampSpeed * dt;
			if (ph.currentOffset < ph.targetOffset) {
				ph.currentOffset = std::min(ph.targetOffset, ph.currentOffset + step);
			} else {
				ph.currentOffset = std::max(ph.targetOffset, ph.currentOffset - step);
			}
			if (std::abs(ph.currentOffset - ph.targetOffset) < 1e-4f) {
				ph.motion = PlayheadMotion::HOLD;
			}
		}
		ph.currentOffset = quantize(ph.currentOffset);
		ph.uploadedThisFrame = false;
	}
}

void TimeOffsetVideoBuffer::jumpPlayhead(int playheadIndex, float normalizedOffset) {
	Playhead& ph = playheads[playheadIndex];
	ph.targetOffset = ofClamp(normalizedOffset, 0.0f, 1.0f);
	ph.currentOffset = quantize(ph.targetOffset);
	ph.motion = PlayheadMotion::HOLD;
}

void TimeOffsetVideoBuffer::rampPlayheadTo(int playheadIndex, float normalizedOffset, float unitsPerSecond) {
	Playhead& ph = playheads[playheadIndex];
	ph.targetOffset = ofClamp(normalizedOffset, 0.0f, 1.0f);
	ph.rampSpeed = unitsPerSecond;
	ph.motion = PlayheadMotion::RAMP;
}

void TimeOffsetVideoBuffer::holdPlayhead(int playheadIndex) {
	playheads[playheadIndex].motion = PlayheadMotion::HOLD;
}

float TimeOffsetVideoBuffer::getPlayheadOffset(int playheadIndex) const {
	return playheads[playheadIndex].currentOffset;
}

const ofTexture& TimeOffsetVideoBuffer::getPlayheadTexture(int playheadIndex) {
	Playhead& ph = playheads[playheadIndex];

	if (!ph.uploadedThisFrame) {
		int idx = offsetToHistoryIndex(ph.currentOffset);
		if (idx >= 0) {
			const ofPixels& px = history[idx];
			if (!ph.texture.isAllocated()
				|| ph.texture.getWidth() != px.getWidth()
				|| ph.texture.getHeight() != px.getHeight()) {
				ph.texture.allocate(px);
			}
			ph.texture.loadData(px);
		}
		ph.uploadedThisFrame = true;
	}

	return ph.texture;
}

float TimeOffsetVideoBuffer::quantize(float normalizedOffset) const {
	int bands = std::max(2, settings.numQuantizeBands);
	float clamped = ofClamp(normalizedOffset, 0.0f, 1.0f);
	return std::round(clamped * (bands - 1)) / static_cast<float>(bands - 1);
}

int TimeOffsetVideoBuffer::getHistoryCapacityFrames() const {
	return static_cast<int>(settings.maxHistorySeconds * settings.assumedSourceFps);
}

int TimeOffsetVideoBuffer::offsetToHistoryIndex(float quantizedOffset) const {
	if (history.empty()) {
		return -1;
	}
	size_t maxIndex = history.size() - 1;
	size_t idx = static_cast<size_t>(std::lround(quantizedOffset * static_cast<float>(maxIndex)));
	return static_cast<int>(idx);
}
