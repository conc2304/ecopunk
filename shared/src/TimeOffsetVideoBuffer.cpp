#include "TimeOffsetVideoBuffer.h"
#include "Rt004StallProbe.h" // RT-004 diagnostic probe (env-gated, no behavior change)
#include "ofFileUtils.h"
#include "ofLog.h"
#include "ofMath.h"
#include "ofUtils.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {
// RT-004: pause before close — see OfVideoDecoder.cpp's pauseThenClose()
// for why (ofGstUtils::close()'s up-to-5 s EOS wait on a playing pipeline).
void pauseThenClose(ofVideoPlayer& player) {
	if (player.isLoaded()) player.setPaused(true);
	player.close();
}
} // namespace

void TimeOffsetVideoBuffer::configure(const Settings& settings_) {
	settings = settings_;
	playheads.assign(std::max(1, settings.numPlayheads), Playhead{});
	resetHistoryAndPlayheadTextures();
}

void TimeOffsetVideoBuffer::resetHistoryAndPlayheadTextures() {
	history.clear();
	// TEMP-004: the GPU-side playhead textures are part of the history's
	// presentable state. Leaving them allocated across a reset let callers
	// (whose contract is "unallocated => no frame yet, skip") draw the last
	// pre-reset frame — previous activation or previous media — for every
	// frame until the new history refilled.
	for (auto& ph : playheads) {
		if (ph.texture.isAllocated()) {
			ph.texture.clear();
		}
		ph.uploadedThisFrame = false;
		ph.uploadedGeneration = 0;
	}
}

void TimeOffsetVideoBuffer::setup(const std::string& mediaFolderPath, const Settings& settings_) {
	configure(settings_);

	ofDirectory dir;
	dir.allowExt("mp4");
	dir.listDir(mediaFolderPath);
	dir.sort();

	mediaFiles.clear();
	for (const auto& file : dir.getFiles()) {
		mediaFiles.push_back(file.getAbsolutePath());
	}
	// dir.sort() above only exists to make the scan itself deterministic
	// (stable across re-scans) — playback order is randomized from here so
	// clips don't always play in the same fixed alphabetical sequence.
	shuffleMediaFiles();

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

void TimeOffsetVideoBuffer::shuffleMediaFiles() {
	for (int i = static_cast<int>(mediaFiles.size()) - 1; i > 0; i--) {
		int j = static_cast<int>(ofRandom(i + 1));
		std::swap(mediaFiles[i], mediaFiles[j]);
	}
}

void TimeOffsetVideoBuffer::advanceToNextMedia() {
	if (mediaFiles.size() < 2) {
		return;
	}

	std::string finishedFile = mediaFiles[currentFileIndex];
	currentFileIndex++;
	if (currentFileIndex >= static_cast<int>(mediaFiles.size())) {
		// Completed a full lap through the shuffled order — reshuffle for
		// the next one so the whole playlist is heard before any repeat,
		// but the order itself keeps changing lap to lap.
		shuffleMediaFiles();
		currentFileIndex = 0;
		// Avoid the same clip playing twice back-to-back across the lap
		// boundary (the just-finished clip landing back at the front).
		if (mediaFiles[0] == finishedFile) {
			std::swap(mediaFiles[0], mediaFiles[1 + static_cast<int>(ofRandom(mediaFiles.size() - 1))]);
		}
	}
	resetHistoryAndPlayheadTextures(); // buffered frames are from the previous source — stale, discard

	pauseThenClose(player);
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

void TimeOffsetVideoBuffer::releaseForInactive() {
	RT004_SCOPE("TimeOffsetVideoBuffer.releaseForInactive");
	rt004::Probe& probe = rt004::Probe::get();
	const size_t historyBytes = getHistoryPixelBytes();
	const int historyFrames = getHistoryFrameCount();
	const int playheadTextures = getAllocatedPlayheadTextureCount();
	const size_t playheadBytes = getPlayheadTextureBytesEstimate(true);
	const bool decoderWasLoaded = player.isLoaded();
	const int64_t t0 = rt004::nowNs();
	{ RT004_SCOPE("TimeOffsetVideoBuffer.releaseForInactive.resetHistory"); resetHistoryAndPlayheadTextures(); }
	const int64_t t1 = rt004::nowNs();
	{ RT004_SCOPE("TimeOffsetVideoBuffer.releaseForInactive.decoderClose"); pauseThenClose(player); }
	const int64_t t2 = rt004::nowNs();
	mediaLoaded = false;
	pendingStartSeconds = 0.0f;
	if (probe.enabled()) {
		probe.note("temporal.releaseForInactive",
			"historyFrames=" + std::to_string(historyFrames) + " historyPixMiB=" + ofToString(historyBytes / (1024.0 * 1024.0), 2)
			+ " playheadTex=" + std::to_string(playheadTextures) + " playheadEstRgbaMiB=" + ofToString(playheadBytes / (1024.0 * 1024.0), 2)
			+ " decoderWasLoaded=" + std::to_string(decoderWasLoaded) + " resetMs=" + ofToString((t1 - t0) / 1e6, 2)
			+ " decoderCloseMs=" + ofToString((t2 - t1) / 1e6, 2) + " | after: historyFrames=" + std::to_string(getHistoryFrameCount())
			+ " playheadTex=" + std::to_string(getAllocatedPlayheadTextureCount()) + " decoderLoaded=" + std::to_string(player.isLoaded()));
	}
}

bool TimeOffsetVideoBuffer::loadExplicit(const std::string& absolutePath, float startSeconds) {
	// No scan, no shuffle, no playlist mutation, no fallback selection —
	// mediaFiles/currentFileIndex are deliberately untouched here (DEC-014).
	RT004_SCOPE("TimeOffsetVideoBuffer.loadExplicit");
	rt004::Probe::get().note("temporal.loadExplicit", ofFilePath::getFileName(absolutePath));
	{ RT004_SCOPE("TimeOffsetVideoBuffer.resetHistory"); resetHistoryAndPlayheadTextures(); }

	{ RT004_SCOPE("TimeOffsetVideoBuffer.player.close"); pauseThenClose(player); }
	{ RT004_SCOPE("TimeOffsetVideoBuffer.player.load"); mediaLoaded = player.load(absolutePath); }
	mediaElapsedTime = 0.0f;
	mediaLoopCount = 0;
	lastMediaPosition = 0.0f;
	// Recorded regardless of success/failure, matching setup()/
	// advanceToNextMedia()'s own precedent of tracking the attempted file
	// (getCurrentMediaFilename() reflects "what was last asked for," not
	// "what is definitely playing" — hasMedia() is the success signal).
	explicitMediaPath = absolutePath;

	if (mediaLoaded) {
		player.setLoopState(OF_LOOP_NORMAL);
		player.play();
		pendingStartSeconds = 0.0f;
		if (startSeconds > 0.0f) {
			if (player.getDuration() > 0.0f) applyStartSeconds(startSeconds);
			else pendingStartSeconds = startSeconds; // decoder not ready yet: update() applies it once
		}
		ofLogNotice("TimeOffsetVideoBuffer") << "loadExplicit: loaded " << absolutePath;
	} else {
		ofLogWarning("TimeOffsetVideoBuffer") << "loadExplicit: failed to load " << absolutePath;
	}
	return mediaLoaded;
}

std::string TimeOffsetVideoBuffer::getCurrentMediaFilename() const {
	if (!explicitMediaPath.empty()) {
		return explicitMediaPath;
	}
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

void TimeOffsetVideoBuffer::applyStartSeconds(float startSeconds) {
	const float duration = player.getDuration();
	if (duration > 0.0f && startSeconds > 0.0f && startSeconds < duration) {
		lastMediaPosition = startSeconds / duration; // a seek is not a loop
		player.setPosition(lastMediaPosition);
	}
}

void TimeOffsetVideoBuffer::update(float dt) {
	if (!mediaLoaded) {
		return;
	}

	diagPushedThisFrame = false;
	{ RT004_SCOPE("TimeOffsetVideoBuffer.player.update"); player.update(); }

	if (pendingStartSeconds > 0.0f && player.getDuration() > 0.0f) {
		applyStartSeconds(pendingStartSeconds);
		pendingStartSeconds = 0.0f;
	}

	rt004::Probe& probe = rt004::Probe::get();
	probe.count("TOVB.update.calls");
	if (player.isFrameNew()) {
		RT004_SCOPE("TimeOffsetVideoBuffer.historyPush(copy+resize)");
		probe.count("TOVB.update.newSourceFrame");
		if (probe.enabled()) {
			const ofPixels& src = player.getPixels();
			std::string fmt = std::to_string(src.getWidth()) + "x" + std::to_string(src.getHeight()) + " ch="
				+ std::to_string(src.getNumChannels()) + " fmt=" + std::to_string(static_cast<int>(src.getPixelFormat()))
				+ " -> " + std::to_string(settings.bufferWidth) + "x" + std::to_string(settings.bufferHeight);
			if (fmt != diagLastSourceFormat) {
				probe.note("temporal.historySourceFormat", fmt);
				probe.count("TOVB.history.sourceFormatChanged");
				diagLastSourceFormat = fmt;
			}
		}
		// Nested inside historyPush: these scopes are its parts, not extra cost.
		ofPixels frame;
		const bool fast = downscaleIntoHistoryFrame(player.getPixels(), frame); // scoped inside as TOVB.history.fastDownscale
		if (fast) {
			probe.count("TOVB.history.fastPath");
			probe.count("TOVB.history.heapAlloc", 1); // destination only
		} else {
			RT004_SCOPE("TOVB.history.legacyDownscale"); // copySource + resize below
			{ RT004_SCOPE("TOVB.history.copySource"); frame = player.getPixels(); }
			{ RT004_SCOPE("TOVB.history.resize"); frame.resize(settings.bufferWidth, settings.bufferHeight); }
			probe.count("TOVB.history.legacyPath");
			probe.count("TOVB.history.heapAlloc", 2); // full-size copy + resized destination
			probe.count("TOVB.history.heapFree", 1);  // full-size copy released by resize()
		}
		static const bool verifyDownscale = std::getenv("EXPERIENCE_RUNTIME_TEMPORAL_DOWNSCALE_VERIFY") != nullptr;
		if (fast && verifyDownscale) {
			// Verification only: rebuild the frame the original way and compare bytes.
			ofPixels reference = player.getPixels();
			reference.resize(settings.bufferWidth, settings.bufferHeight);
			const bool same = reference.getWidth() == frame.getWidth() && reference.getHeight() == frame.getHeight()
				&& reference.getPixelFormat() == frame.getPixelFormat() && reference.size() == frame.size()
				&& std::equal(reference.begin(), reference.end(), frame.begin());
			if (!same) ofLogError("TimeOffsetVideoBuffer") << "downscale verify: fast path differs from ofPixels::resize";
			probe.count(same ? "TOVB.history.verifyMatch" : "TOVB.history.verifyMismatch");
		}

		RT004_SCOPE("TOVB.history.pushPop");
		history.push_front(std::move(frame));
		historyGeneration++;
		diagPushedThisFrame = true;

		size_t maxFrames = static_cast<size_t>(settings.maxHistorySeconds * settings.assumedSourceFps);
		while (history.size() > maxFrames) {
			history.pop_back();
			probe.count("TOVB.history.heapFree");
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
	diagUploadedGenerationsThisFrame.clear();
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

	rt004::Probe& probe = rt004::Probe::get();
	probe.count("TOVB.playhead.getCalls");
	if (!ph.uploadedThisFrame) {
		RT004_SCOPE("TimeOffsetVideoBuffer.playheadUpload");
		int idx;
		{ RT004_SCOPE("TOVB.playhead.select"); idx = offsetToHistoryIndex(ph.currentOffset); } // offset -> history index
		if (idx >= 0) {
			const ofPixels& px = history[idx];
			// History frames are never modified after push, so a generation
			// identifies a frame's exact pixels for as long as it exists.
			const uint64_t gen = historyGeneration - static_cast<uint64_t>(idx);
			if (!ph.texture.isAllocated()
				|| ph.texture.getWidth() != px.getWidth()
				|| ph.texture.getHeight() != px.getHeight()) {
				RT004_SCOPE("TOVB.playhead.texAllocate");
				ph.texture.allocate(px);
				ph.uploadedGeneration = 0; // fresh storage: never treat as holding a frame
				probe.count("TOVB.playhead.texAllocate");
				if (probe.enabled()) {
					probe.note("temporal.playheadTexFormat", std::to_string(px.getWidth()) + "x" + std::to_string(px.getHeight())
						+ " ch=" + std::to_string(px.getNumChannels()) + " glInternalFormat=0x"
						+ ofToHex(ph.texture.getTextureData().glInternalFormat));
				}
			}
			// RT-004 playhead upload: skip only a provably redundant upload —
			// this playhead's texture already holds exactly this history
			// frame. EXPERIENCE_RUNTIME_TEMPORAL_PLAYHEAD_UPLOAD=legacy always
			// uploads (read per call so a driver script can A/B in-process).
			const char* uploadMode = std::getenv("EXPERIENCE_RUNTIME_TEMPORAL_PLAYHEAD_UPLOAD");
			const bool legacyUpload = uploadMode != nullptr && std::string(uploadMode) == "legacy";
			const bool redundant = gen == ph.uploadedGeneration;
			if (probe.enabled() && redundant) {
				// Why the same frame came round again (diagnostic).
				probe.count(diagPushedThisFrame ? "TOVB.playhead.sameFrame.newSourceThisFrame" : "TOVB.playhead.sameFrame.noNewSourceThisFrame");
				probe.count(history.size() >= static_cast<size_t>(getHistoryCapacityFrames()) ? "TOVB.playhead.sameFrame.historyFull" : "TOVB.playhead.sameFrame.historyFilling");
			}
			if (redundant && !legacyUpload) {
				probe.count("TOVB.playhead.uploadSkippedRedundant");
				static const bool verifySkip = std::getenv("EXPERIENCE_RUNTIME_TEMPORAL_PLAYHEAD_UPLOAD_VERIFY") != nullptr;
				if (verifySkip) {
					// Verification only (desktop GL): the texture must already
					// hold exactly the pixels the skipped upload would have sent.
					ofPixels held;
					ph.texture.readToPixels(held);
					const bool same = held.size() == px.size() && std::equal(px.begin(), px.end(), held.begin());
					if (!same) ofLogError("TimeOffsetVideoBuffer") << "playhead skip verify: texture differs from selected history frame";
					probe.count(same ? "TOVB.playhead.skipVerifyMatch" : "TOVB.playhead.skipVerifyMismatch");
				}
			} else {
				{ RT004_SCOPE("TOVB.playhead.loadData"); ph.texture.loadData(px); }
				ph.uploadedGeneration = gen;
				probe.count("TOVB.playhead.upload");
				probe.count("TOVB.playhead.uploadBytes", px.size());
				if (redundant) probe.count("TOVB.playhead.uploadSameFrameAsLast");
				if (probe.enabled()) {
					if (std::find(diagUploadedGenerationsThisFrame.begin(), diagUploadedGenerationsThisFrame.end(), gen)
						!= diagUploadedGenerationsThisFrame.end()) {
						probe.count("TOVB.playhead.uploadDuplicateInFrame");
					} else {
						diagUploadedGenerationsThisFrame.push_back(gen);
					}
				}
			}
		} else {
			probe.count("TOVB.playhead.emptyHistory");
		}
		ph.uploadedThisFrame = true;
	}

	return ph.texture;
}

bool TimeOffsetVideoBuffer::downscaleIntoHistoryFrame(const ofPixels& src, ofPixels& dst) {
	// Read per call (not cached) so the RT-004 driver's temporal-ab script
	// can alternate legacy/fast windows inside one process.
	const bool forceLegacy = std::getenv("EXPERIENCE_RUNTIME_TEMPORAL_LEGACY_DOWNSCALE") != nullptr;
	if (forceLegacy || !src.isAllocated() || settings.bufferWidth <= 0 || settings.bufferHeight <= 0) return false;
	switch (src.getPixelFormat()) {
		case OF_PIXELS_GRAY: case OF_PIXELS_RGB: case OF_PIXELS_BGR: case OF_PIXELS_RGBA: case OF_PIXELS_BGRA: break;
		default: return false; // planar/YUV and other formats stay on ofPixels::resize()
	}
	const size_t srcW = src.getWidth();
	const size_t srcH = src.getHeight();
	const size_t dstW = static_cast<size_t>(settings.bufferWidth);
	const size_t dstH = static_cast<size_t>(settings.bufferHeight);
	const size_t channels = src.getNumChannels();

	DownscaleMap& m = downscaleMap;
	if (m.srcWidth != srcW || m.srcHeight != srcH || m.dstWidth != dstW || m.dstHeight != dstH || m.channels != channels) {
		// Same arithmetic, in the same types and order, as
		// ofPixels_::resizeTo()'s OF_INTERPOLATE_NEAREST_NEIGHBOR branch.
		const float srcxFactor = (float)srcW / dstW;
		const float srcyFactor = (float)srcH / dstH;
		std::vector<size_t> srcIndexPerRow(dstH);
		std::vector<float> srcxPerCol(dstW);
		float srcy = 0.5;
		for (size_t y = 0; y < dstH; y++) {
			srcIndexPerRow[y] = static_cast<size_t>(srcy) * srcW;
			srcy += srcyFactor;
		}
		float srcx = 0.5;
		for (size_t x = 0; x < dstW; x++) {
			srcxPerCol[x] = srcx;
			srcx += srcxFactor;
		}
		// The reference indexes with static_cast<size_t>(srcIndex + srcx),
		// a float sum; the cached form splits it into row + column integer
		// parts. Use the cache only if the two agree for every pixel.
		bool exact = true;
		const size_t srcPixels = srcW * srcH;
		for (size_t y = 0; y < dstH && exact; y++) {
			const size_t srcIndex = srcIndexPerRow[y];
			for (size_t x = 0; x < dstW; x++) {
				const size_t reference = static_cast<size_t>(srcIndex + srcxPerCol[x]);
				const size_t split = srcIndex + static_cast<size_t>(srcxPerCol[x]);
				if (reference != split || split >= srcPixels) { exact = false; break; }
			}
		}
		m.srcWidth = srcW; m.srcHeight = srcH; m.dstWidth = dstW; m.dstHeight = dstH; m.channels = channels;
		m.rowByteOffset.resize(dstH);
		m.colByteOffset.resize(dstW);
		for (size_t y = 0; y < dstH; y++) m.rowByteOffset[y] = srcIndexPerRow[y] * channels;
		for (size_t x = 0; x < dstW; x++) m.colByteOffset[x] = static_cast<size_t>(srcxPerCol[x]) * channels;
		m.exact = exact;
		rt004::Probe::get().note("temporal.downscaleMap", std::to_string(srcW) + "x" + std::to_string(srcH) + " -> "
			+ std::to_string(dstW) + "x" + std::to_string(dstH) + " ch=" + std::to_string(channels) + (exact ? " exact" : " NOT exact (legacy path)"));
		if (!exact) ofLogWarning("TimeOffsetVideoBuffer") << "cached downscale not provably identical for " << srcW << "x" << srcH << "; using ofPixels::resize()";
	}
	if (!m.exact) return false;

	RT004_SCOPE("TOVB.history.fastDownscale"); // allocate + copy; excludes the one-time map build above
	dst.allocate(dstW, dstH, src.getPixelFormat());
	const unsigned char* srcData = src.getData();
	unsigned char* out = dst.getData();
	for (size_t y = 0; y < dstH; y++) {
		const unsigned char* row = srcData + m.rowByteOffset[y];
		if (channels == 3) {
			for (size_t x = 0; x < dstW; x++) {
				const unsigned char* p = row + m.colByteOffset[x];
				out[0] = p[0]; out[1] = p[1]; out[2] = p[2];
				out += 3;
			}
		} else {
			for (size_t x = 0; x < dstW; x++) {
				std::memcpy(out, row + m.colByteOffset[x], channels);
				out += channels;
			}
		}
	}
	return true;
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
