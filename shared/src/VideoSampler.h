#pragma once

#include <deque>
#include <functional>
#include <string>
#include <vector>
#include "ofVideoPlayer.h"

// Wraps the single ofVideoPlayer instance every sketch is constrained to
// (VideoCore IV limit on Pi, kept as a hard rule on Mac too for parity).
// Captures are serialized through a small request queue: seek, wait a
// fixed settle window, grab pixels, hand them back via callback. Never
// call requestCapture()'s result synchronously — the callback fires from
// a later update(), per the doc's documented 1-2 frame capture latency.
class VideoSampler {
	public:
		using CaptureCallback = std::function<void(const ofPixels&)>;

		void setup(const std::string& mediaPath);
		bool hasMedia() const { return !mediaFiles.empty(); }
		void selectVideoForCycle();

		// Returns false (and queues nothing) if there's no media loaded or
		// the in-flight queue is already at its cap — caller should fall
		// back to a placeholder fill in that case.
		bool requestCapture(float normalizedOffset, CaptureCallback callback);

		// Drops any queued/in-flight requests without invoking their
		// callbacks. Must be called before destroying the Fragments those
		// callbacks would touch (e.g. CompositionBase::startCycle() clearing
		// the fragment list) — otherwise a capture that completes after the
		// fragment is gone would call back into a dangling pointer.
		void cancelPending();

		void update();

		// Live-video accessors — valid once selectVideoForCycle() has been called.
		const ofTexture& getTexture() const { return player.getTexture(); }
		int getVideoWidth()  const { return static_cast<int>(player.getWidth()); }
		int getVideoHeight() const { return static_cast<int>(player.getHeight()); }

	private:
		enum class State { IDLE, SEEKING };

		void recordLatency(float ms);
		void logLatencyStats() const;

		struct PendingRequest {
			float offset;
			CaptureCallback callback;
		};

		ofVideoPlayer player;
		std::deque<PendingRequest> queue;
		State state = State::IDLE;
		int settleFramesRemaining = 0;
		float seekRequestTime = 0;

		std::vector<float> seekLatenciesMs;
		bool latencyLogged = false;

		std::vector<std::string> mediaFiles;
		int currentFileIndex = -1;
};
