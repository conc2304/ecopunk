#include "VideoPlaybackService.h"

#include "OfVideoDecoder.h"

#include "ofFileUtils.h"
#include "ofLog.h"

#include <algorithm>

namespace {
constexpr const char* kLogName = "VideoPlaybackService";
}

VideoPlaybackService::VideoPlaybackService(std::unique_ptr<IVideoDecoder> decoder)
	: decoder_(decoder ? std::move(decoder) : std::make_unique<OfVideoDecoder>()) {
}

bool VideoPlaybackService::setup(const Config& config) {
	setupComplete_ = false;
	health_ = VideoPlaybackHealth::Unavailable;
	messageId_.reset();
	errorCode_.reset();

	if (config.mediaRoot.empty()) {
		messageId_ = "media.error.no_media_root_configured";
		ofLogError(kLogName) << "setup() called with an empty mediaRoot";
		return false;
	}
	mediaRoot_ = config.mediaRoot;

	// -- Discover playable files beneath the canonical root -------------
	// .mp4 only, matching every existing wrapper's own scan convention
	// (see docs/video-playback-ownership-probe-report.md §B — VideoSampler,
	// TimeOffsetVideoBuffer, and VideoSystem all filter to .mp4 only).
	std::vector<std::string> discoveredRelativePaths;
	ofDirectory dir;
	dir.allowExt("mp4");
	if (dir.doesDirectoryExist(mediaRoot_)) {
		dir.listDir(mediaRoot_);
		for (const auto& file : dir.getFiles()) {
			// relativePath is stored relative to mediaRoot_ so a later
			// activateCandidate() can rejoin it against whatever root this
			// service was configured with, and so mediaId synthesis stays
			// stable regardless of mediaRoot_'s absolute location on disk
			// (MediaMetadata.h's "stable, independent of absolute paths").
			discoveredRelativePaths.push_back(file.getFileName());
		}
	} else {
		ofLogWarning(kLogName) << "media root does not exist: " << mediaRoot_;
	}

	// -- Optional catalog-file overrides ---------------------------------
	std::vector<std::string> catalogErrors;
	std::vector<MediaCatalogEntryOverride> overrides =
		loadCatalogOverridesFromJsonFile(config.catalogPath, catalogErrors);
	for (const auto& err : catalogErrors) ofLogWarning(kLogName) << err;

	MediaCatalogBuildResult built = MediaCatalog::build(discoveredRelativePaths, overrides);
	for (const auto& err : built.errors) ofLogWarning(kLogName) << err;
	catalog_ = std::move(built.entries);

	ofLogNotice(kLogName) << "catalog built: " << catalog_.size() << " playable-candidate entr"
		<< (catalog_.size() == 1 ? "y" : "ies") << " from " << mediaRoot_;

	VideoSelectionPolicy::Config policyConfig;
	policyConfig.automaticAdvanceEnabled = config.automaticAdvance;
	policyConfig.holdDurationSeconds = config.holdDurationSeconds;
	policy_.reset(catalog_, policyConfig);

	setupComplete_ = true;

	if (catalog_.empty()) {
		health_ = VideoPlaybackHealth::Unavailable;
		messageId_ = "media.error.empty_catalog";
		return true;  // structurally successful setup — see header comment
	}

	attemptSelection([this] { return policy_.requestInitialCandidate(); }, MediaSelectionOrigin::Startup);
	return true;
}

bool VideoPlaybackService::activateCandidate(int catalogIndex) {
	if (catalogIndex < 0 || catalogIndex >= static_cast<int>(catalog_.size())) return false;
	const MediaMetadata& item = catalog_[static_cast<size_t>(catalogIndex)];
	std::string absolutePath = ofFilePath::join(mediaRoot_, item.relativePath);
	bool ok = decoder_->load(absolutePath);
	if (!ok) {
		ofLogWarning(kLogName) << "failed to load: " << absolutePath;
	}
	return ok;
}

bool VideoPlaybackService::attemptSelection(const std::function<std::optional<int>()>& firstCandidateFn, MediaSelectionOrigin baseOrigin) {
	std::optional<int> candidate = firstCandidateFn();
	std::vector<int> tried;
	bool anyFailure = false;

	while (candidate) {
		tried.push_back(*candidate);
		if (activateCandidate(*candidate)) {
			MediaSelectionOrigin finalOrigin = anyFailure ? MediaSelectionOrigin::Recovery : baseOrigin;
			policy_.confirmActivationSucceeded(*candidate, finalOrigin);
			health_ = VideoPlaybackHealth::Ready;
			messageId_.reset();
			errorCode_.reset();
			return true;
		}
		anyFailure = true;
		errorCode_ = "media.error.load_failed";
		candidate = policy_.requestRecoveryCandidate(tried);
	}

	// Every playable candidate (bounded by playableCount(), §12) has now
	// failed to load this pass.
	health_ = VideoPlaybackHealth::Failed;
	messageId_ = "media.error.all_candidates_failed";
	return false;
}

void VideoPlaybackService::update(float dt) {
	if (!setupComplete_) return;
	if (decoder_->isLoaded()) decoder_->update(dt);

	if (health_ == VideoPlaybackHealth::Ready) {
		policy_.updateHold(dt);
		if (policy_.isHoldComplete()) {
			attemptSelection([this] { return policy_.requestAutomaticAdvanceCandidate(); }, MediaSelectionOrigin::Automatic);
		}
	}
}

void VideoPlaybackService::shutdown() {
	decoder_->close();
	setupComplete_ = false;
	health_ = VideoPlaybackHealth::Unavailable;
}

bool VideoPlaybackService::next() {
	if (!setupComplete_ || catalog_.empty()) return false;
	if (!policy_.canSelectNext()) return false;
	return attemptSelection([this] { return policy_.requestManualNextCandidate(); }, MediaSelectionOrigin::ManualNext);
}

bool VideoPlaybackService::previous() {
	if (!setupComplete_ || catalog_.empty()) return false;
	if (!policy_.canSelectPrevious()) return false;
	return attemptSelection([this] { return policy_.requestManualPreviousCandidate(); }, MediaSelectionOrigin::ManualPrevious);
}

const ofTexture* VideoPlaybackService::currentTexture() const {
	if (health_ != VideoPlaybackHealth::Ready && health_ != VideoPlaybackHealth::Degraded) return nullptr;
	return decoder_->getTexture();
}

const ofPixels* VideoPlaybackService::currentPixels() const {
	if (health_ != VideoPlaybackHealth::Ready && health_ != VideoPlaybackHealth::Degraded) return nullptr;
	return decoder_->getPixels();
}

bool VideoPlaybackService::isFrameNew() const {
	if (health_ != VideoPlaybackHealth::Ready && health_ != VideoPlaybackHealth::Degraded) return false;
	return decoder_->isFrameNew();
}

glm::ivec2 VideoPlaybackService::sourceSize() const {
	if (health_ != VideoPlaybackHealth::Ready && health_ != VideoPlaybackHealth::Degraded) return glm::ivec2(0, 0);
	return decoder_->getSize();
}

VideoPlaybackStatus VideoPlaybackService::status() const {
	VideoPlaybackStatus s;
	s.schemaVersion = 1;
	s.health = health_;
	s.messageId = messageId_;
	s.errorCode = errorCode_;
	s.selectionOrigin = policy_.activeSelectionOrigin();
	s.canSelectPrevious = policy_.canSelectPrevious();
	s.canSelectNext = policy_.canSelectNext();

	if (policy_.hasActiveItem()) {
		const MediaMetadata& item = catalog_[static_cast<size_t>(policy_.activeCatalogIndex())];
		s.mediaId = item.mediaId;
		s.titleId = item.titleId;
		s.fallbackDisplayTitle = item.fallbackDisplayTitle;
	}

	if (health_ == VideoPlaybackHealth::Ready || health_ == VideoPlaybackHealth::Degraded) {
		float duration = decoder_->getDuration();
		float position = decoder_->getPosition();  // normalized [0,1], -1 if unavailable
		if (duration > 0.0f && position >= 0.0f) {
			s.durationSeconds = duration;
			s.positionSeconds = position * duration;
			s.playbackProgress = std::clamp(position, 0.0f, 1.0f);
		}
		// else: leave all three nullopt — "never use zero to represent
		// missing information" (§4.5).
	}

	s.holdProgress = policy_.holdProgress();
	s.holdElapsedSeconds = policy_.holdElapsedSeconds();
	s.holdDurationSeconds = policy_.holdDurationSeconds();
	s.holdRemainingSeconds = policy_.holdRemainingSeconds();

	return s;
}
