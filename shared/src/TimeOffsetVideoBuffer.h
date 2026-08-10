#pragma once

#include <deque>
#include <vector>
#include <string>
#include "ofVideoPlayer.h"
#include "ofPixels.h"
#include "ofTexture.h"

// Plays a single video continuously (forward, looping) and keeps a rolling
// history of downscaled frames in CPU memory, so multiple "playheads" can
// each read a different point in the recent past with zero additional
// decode/seek calls per frame.
//
// This is a deliberately different shape from VideoSampler: VideoSampler
// exists to mask the cost of occasional discrete seeks (its whole design —
// request queue, settle frames, latency logging — is built around seeking
// being expensive). This class never seeks at all; it just accumulates
// what's already being decoded forward. See
// docs/temporal-fields-implementation-brief.md, Section 1's verification
// notes, for why VideoSampler wasn't extended instead.
//
// History is stored as ofPixels (system RAM), not resident ofTexture (VRAM):
// only the small number of playheads actually in use get uploaded to a
// texture, once per frame, on demand — the full history buffer never lives
// on the GPU. Pi 3B has 1GB of RAM shared between CPU and GPU; a naive
// texture-per-history-frame approach (e.g. 240 frames at 640x360 RGBA)
// would burn ~220MB of that on its own. See Phase 8 of the brief — real
// on-device memory measurement is still outstanding.
class TimeOffsetVideoBuffer {
	public:
		struct Settings {
			int bufferWidth = 640;
			int bufferHeight = 360;
			float maxHistorySeconds = 10.0f;
			int numPlayheads = 6; // 5-8 per the brief
			int numQuantizeBands = 12; // 8-16 per the brief
			float assumedSourceFps = 24.0f; // for sizing the history buffer; see tools/convert_media.sh spec

			// A clip must satisfy BOTH before advanceToNextMedia() will
			// actually swap it out — see isMediaAdvanceEligible().
			float minPlaytimeSeconds = 30.0f;
			int minLoopCount = 4;
		};

		// Applies settings (buffer dimensions, history length, playhead
		// count/quantization, min-playtime/loop gating) and (re)allocates
		// the playhead pool + clears history — everything setup() below
		// does EXCEPT the directory scan/shuffle/load. Extracted so a
		// canonical-service-driven caller (see loadExplicit() below and
		// shared/src/video-playback/adapters/TimeOffsetPlaybackAdapter.h)
		// can configure this buffer's shape without ever going through the
		// legacy scan path — setup() itself now just calls this and then
		// does the scan/shuffle/load on top.
		void configure(const Settings& settings);

		// mediaFolderPath is a FOLDER (scanned for .mp4 files, mirroring
		// VideoSampler's ofDirectory + allowExt("mp4") idiom), not a single
		// file — loads the first file found and keeps the rest of the
		// playlist for advanceToNextMedia().
		//
		// LEGACY PATH — retained for callers that still own their own local
		// media selection (see this header's own top comment history and
		// DEC-014, Shared Video / Temporal Playback-History Boundary). A
		// canonical-service-driven caller should use configure() +
		// loadExplicit() below instead and never call setup()/
		// advanceToNextMedia() at all.
		void setup(const std::string& mediaFolderPath, const Settings& settings);
		void update(float dt);

		// Wraps to the next file in the scanned playlist. Clears the
		// history ring buffer first — buffered frames from the old source
		// are meaningless once the video itself has changed. No-op if only
		// zero or one file were found.
		//
		// LEGACY PATH — see setup()'s comment above. A canonical-service-
		// driven caller must never call this (DEC-014: "must not
		// independently scan, shuffle, or select media").
		void advanceToNextMedia();

		// Loads exactly the file at absolutePath — no directory scan, no
		// shuffle, no playlist, no fallback selection (DEC-014, Shared
		// Effects — Temporal Playback / History Boundary). This is the one
		// sanctioned entry point for a caller (see
		// shared/src/video-playback/adapters/TimeOffsetPlaybackAdapter.h)
		// that receives its media selection from VideoPlaybackService.
		//
		// Reuses setup()/advanceToNextMedia()'s own load sequence (close
		// any current player, clear history, load, loop, play) rather than
		// duplicating it — the only difference is the path comes from the
		// caller instead of an internal scanned/shuffled list, and
		// mediaFiles/currentFileIndex (the legacy playlist state) are left
		// completely untouched, so this path and the legacy scan path never
		// interfere with each other even if a future caller mixed both
		// (not a supported configuration, but not something that should
		// corrupt state either).
		//
		// History is cleared unconditionally, even on failure — a failed
		// load must never leave stale frames from whatever was playing
		// before attributed to the new (failed) identity. Returns the
		// decoder's own load success/failure honestly; on failure,
		// hasMedia() becomes false and the history/playhead APIs behave
		// exactly as they do before any media has ever loaded (see
		// getPlayheadTexture()'s own "idx < 0" handling) — no local
		// fallback selection of any kind is attempted here.
		bool loadExplicit(const std::string& absolutePath);

		// Returns the explicitly-loaded path if loadExplicit() was ever
		// called (even if that load failed — matches legacy
		// setup()/advanceToNextMedia()'s own behavior of recording the
		// attempted file regardless of outcome), otherwise falls back to
		// the legacy scanned-playlist's current file. The two sources are
		// mutually exclusive in any real caller (a caller uses either the
		// legacy scan path OR loadExplicit(), never both), so there is no
		// real ambiguity about which one "wins" here in practice.
		std::string getCurrentMediaFilename() const;

		// True once the current clip has played at least
		// settings.minPlaytimeSeconds AND completed at least
		// settings.minLoopCount full loops — callers (e.g. a
		// pattern-switch callback) should gate advanceToNextMedia() on
		// this so a fast switch cadence cannot cut a clip short.
		bool isMediaAdvanceEligible() const;

		bool hasMedia() const { return mediaLoaded; }
		int getHistoryFrameCount() const { return static_cast<int>(history.size()); }
		int getHistoryCapacityFrames() const;
		int getBufferWidth() const { return settings.bufferWidth; }
		int getBufferHeight() const { return settings.bufferHeight; }

		int getNumPlayheads() const { return static_cast<int>(playheads.size()); }

		// normalizedOffset: 0 = live/most-recent frame (white), 1 = as far
		// back as maxHistorySeconds allows (black) — matches the brief's
		// gray-to-offset mapping directly, so a pattern can feed a sampled
		// luminance value straight in.
		void jumpPlayhead(int playheadIndex, float normalizedOffset);
		void rampPlayheadTo(int playheadIndex, float normalizedOffset, float unitsPerSecond);
		void holdPlayhead(int playheadIndex);

		// Actual (quantized) offset a playhead is currently sampling from.
		float getPlayheadOffset(int playheadIndex) const;

		// Uploads (once per frame, at most) and returns the texture for a
		// playhead's current buffered frame. Safe to call from multiple
		// fragments referencing the same playhead in the same frame.
		const ofTexture& getPlayheadTexture(int playheadIndex);

		// The live ofVideoPlayer's own texture directly — full resolution,
		// un-quantized, un-delayed, bypassing the history ring buffer
		// entirely. Every playhead (even offset 0.0) still goes through
		// that buffer's downscale/re-upload path; this is the one way to
		// get the actual raw frame.
		const ofTexture& getRawVideoTexture() const { return player.getTexture(); }

		// The live ofVideoPlayer's own decoded CPU pixels, same "raw,
		// un-delayed, bypasses the history ring buffer" relationship to
		// getPlayheadTexture() as getRawVideoTexture() above. Added so a
		// CPU-side analysis consumer (e.g. BlobDetector) can reuse the
		// pixels this class already pulls off the decoder every frame
		// (see update()) instead of standing up a second ofVideoPlayer
		// or doing its own GPU texture readback. Valid only when
		// hasMedia() and update() has run at least once.
		const ofPixels& getRawVideoPixels() const { return player.getPixels(); }

		// Snaps a normalized [0,1] offset to this buffer's configured
		// quantize bands — exposed so callers can pre-quantize a value
		// (e.g. for display) without going through a playhead.
		float quantize(float normalizedOffset) const;

		// Live-tunable independently of setup() — quantize() reads this
		// each call, nothing needs reallocating, so a GUI slider can drive
		// it directly.
		void setNumQuantizeBands(int bands) { settings.numQuantizeBands = bands; }
		int getNumQuantizeBands() const { return settings.numQuantizeBands; }

		// Live-tunable independently of setup() — update() recomputes the
		// history deque's frame cap from this every call (see maxFrames in
		// the .cpp), so shrinking trims the buffer over the next few frames
		// and growing lets it accumulate further, with no reallocation or
		// restart needed.
		void setMaxHistorySeconds(float seconds) { settings.maxHistorySeconds = seconds; }
		float getMaxHistorySeconds() const { return settings.maxHistorySeconds; }

	private:
		enum class PlayheadMotion { HOLD, RAMP };

		struct Playhead {
			float currentOffset = 0.0f;
			float targetOffset = 0.0f;
			float rampSpeed = 0.0f;
			PlayheadMotion motion = PlayheadMotion::HOLD;
			ofTexture texture;
			bool uploadedThisFrame = false;
		};

		int offsetToHistoryIndex(float quantizedOffset) const;

		// Fisher-Yates shuffle of mediaFiles via ofRandom (same idiom as
		// TFHudLayer::respawnLayout()'s corner shuffle) — called once in
		// setup() and again every time advanceToNextMedia() wraps back to
		// the front, so playback order is randomized per lap rather than
		// the fixed alphabetical order ofDirectory::sort() produces.
		void shuffleMediaFiles();

		ofVideoPlayer player;
		Settings settings{};
		bool mediaLoaded = false;

		std::vector<std::string> mediaFiles;
		int currentFileIndex = -1;

		// Set only by loadExplicit() — kept entirely separate from
		// mediaFiles/currentFileIndex (the legacy scan/playlist state)
		// above, so the two selection paths can never cross-contaminate
		// each other. See getCurrentMediaFilename()'s header comment.
		std::string explicitMediaPath;

		// Minimum-playtime tracking for isMediaAdvanceEligible() — reset in
		// setup() and advanceToNextMedia(). Loop completion is detected by a
		// position drop (wrap from ~1.0 back down near 0.0), not a fixed
		// frame-count threshold, since clip lengths vary.
		float mediaElapsedTime = 0.0f;
		int mediaLoopCount = 0;
		float lastMediaPosition = 0.0f;

		std::deque<ofPixels> history; // front = most recent frame
		std::vector<Playhead> playheads;
};
