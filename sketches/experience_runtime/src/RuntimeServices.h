#pragma once

#include "RuntimeTelemetryCollector.h"
#include "SceneContract.h"
#include "VideoPlaybackService.h"

// RuntimeServices — minimal shell, per Development Stream 1's explicit
// requirement for one. Owns RuntimeTelemetryCollector and, as of the
// Shared Video Playback System increment, one VideoPlaybackService — the
// approved target ownership per Implement-Shared-Video-Playback-System-
// Agent-Prompt.md §2:
//
//   ExperienceRuntime
//   └── RuntimeServices
//       └── VideoPlaybackService
//
// Scene semantic data never lives here — see SceneContract.h's header
// comment on why RuntimeTelemetry and scene semantic data are kept
// structurally separate; VideoPlaybackStatus follows the same rule (it is
// runtime/service-owned, never scene-owned — see
// shared/src/video-playback/README.md and HUD-Semantic-Slot-Model-v1.md
// §6.3).
class RuntimeServices {
public:
	// No-op (does not call VideoPlaybackService::setup()) if
	// videoConfig.mediaRoot is empty — a caller that has no media root to
	// offer yet (e.g. this harness's own FakeScene-only setup, which has
	// no real scene consuming video) simply leaves the video service
	// unconfigured; videoPlaybackStatus() then returns nullopt rather than
	// a populated-but-Unavailable status, so a consumer can distinguish
	// "not set up" from "set up, no media found" (see HudFrameData.h).
	void setup(const VideoPlaybackService::Config& videoConfig) {
		if (videoConfig.mediaRoot.empty()) return;
		videoService_.setup(videoConfig);
		videoConfigured_ = true;
	}

	void update(float dt) {
		telemetryCollector_.update(dt);
		if (videoConfigured_) videoService_.update(dt);
	}

	void shutdown() {
		if (videoConfigured_) videoService_.shutdown();
		videoConfigured_ = false;
	}

	const RuntimeTelemetry& telemetry() const { return telemetryCollector_.snapshot(); }

	VideoPlaybackService& video() { return videoService_; }
	const VideoPlaybackService& video() const { return videoService_; }

	std::optional<VideoPlaybackStatus> videoPlaybackStatus() const {
		if (!videoConfigured_) return std::nullopt;
		return videoService_.status();
	}

private:
	RuntimeTelemetryCollector telemetryCollector_;
	VideoPlaybackService videoService_;
	bool videoConfigured_ = false;
};
