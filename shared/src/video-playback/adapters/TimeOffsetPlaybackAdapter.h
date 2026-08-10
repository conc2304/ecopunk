#pragma once

#include "TimeOffsetVideoBuffer.h"
#include <string>

// ============================================================================
// TimeOffsetPlaybackAdapter.h — the Temporal Fields specialized Shared Video
// adapter seam (DEC-014, Shared Video — Temporal Fields Specialized Adapter
// Seam session).
//
// Couples VideoPlaybackService's canonical media selection to Temporal's
// dedicated TimeOffsetVideoBuffer decoder/history pipeline, WITHOUT this
// class ever becoming a second selection/catalog/history authority itself:
//
//   VideoPlaybackService (owns catalog, selection, session history,
//   Previous/Next, hold timing, VideoPlaybackStatus — DEC-013)
//       |
//       | caller reads status().mediaId + currentAbsolutePath() once per
//       | frame (or whenever polled) and passes both to...
//       v
//   TimeOffsetPlaybackAdapter::synchronizeSelectedMedia(mediaId, path)
//       |
//       | on a CHANGED mediaId only: buffer_.loadExplicit(path)
//       v
//   TimeOffsetVideoBuffer (dedicated decoder, rolling history, six
//   playheads, quantization — unchanged, DEC-014's specialized exception)
//
// This class does NOT own a MediaCatalog, VideoSelectionPolicy, playlist,
// or any history of previously-selected media IDs of its own — it tracks
// exactly one thing (loadedMediaId_, "what is the buffer currently loaded
// with") purely to decide whether a reload is needed, never to make a
// selection decision itself. It never calls
// TimeOffsetVideoBuffer::advanceToNextMedia() or setup() (the legacy
// scan/shuffle/select path) — only loadExplicit().
//
// The caller (a scene's ofApp/setup-update loop) is responsible for owning
// the VideoPlaybackService instance and driving next()/previous() from
// whatever input reaches it — this class only ever reacts to an identity
// it is told about; it never inspects selection origin
// (Startup/Automatic/ManualPrevious/ManualNext/Recovery) to choose a
// different reset strategy (§4.3 of this session's prompt — identical
// reset semantics for every origin).
//
// Public API kept intentionally small — every method here is required by
// at least one acceptance-criteria test in this session's implementation
// report ("Exact public API delta" section). No activate()/deactivate()/
// shutdown() lifecycle hooks: Temporal is not (yet) an IEcopunkScene-hosted
// production scene in this increment (that migration is explicitly out of
// scope — see the prompt's "Explicit non-goals"), and TimeOffsetVideoBuffer
// itself has no teardown method for this class to wrap.
class TimeOffsetPlaybackAdapter {
public:
	// Forwards straight to the owned TimeOffsetVideoBuffer::configure() —
	// the ONE-TIME buffer configuration (dimensions, history length,
	// playhead count, quantize bands), not a media selection. Does not
	// load any media itself (configure() only allocates the playhead pool
	// and clears history — see TimeOffsetVideoBuffer.h); the first real
	// load happens on the first synchronizeSelectedMedia() call once the
	// caller has a real VideoPlaybackService selection to report.
	void setup(const TimeOffsetVideoBuffer::Settings& settings);

	// Called by the owning scene once per frame (or whenever
	// VideoPlaybackService's status is polled) with the currently active
	// media's stable catalog ID and resolved absolute path.
	//
	//   - mediaId unchanged from the last successful synchronization: no-op
	//     (buffer_ is left exactly as it is — same media, same history, no
	//     reload, no clear).
	//   - mediaId changed (including the very first call): exactly one
	//     buffer_.loadExplicit(absolutePath) call, which clears history
	//     unconditionally per TimeOffsetVideoBuffer::loadExplicit()'s own
	//     contract, then returns that call's success/failure honestly.
	//
	// Returns true if the buffer ends this call with the requested media
	// loaded (either because it already was, or because the reload just
	// succeeded); false only when a reload was attempted and the dedicated
	// decoder failed to open it. A false return does NOT roll back
	// loadedMediaId_ — the adapter still considers `mediaId` the "current"
	// shared selection (VideoPlaybackService remains authoritative on it
	// regardless of whether Temporal's OWN decoder could open it), it just
	// has no usable local history/playhead output until a future call
	// reports a different (or the same, retried) mediaId.
	bool synchronizeSelectedMedia(const std::string& mediaId, const std::string& absolutePath);

	// Forwards to the owned TimeOffsetVideoBuffer's own update — playhead
	// stepping, quantization, decode pump, history push/trim. Unchanged
	// behavior vs. calling buffer().update(dt) directly.
	void update(float dt);

	// Temporal Production Scene #2 Migration — the smallest specialized
	// lifecycle mechanism needed for correct scene reactivation (see this
	// session's migration prompt §8.4): a scene stops calling update(dt)
	// on this adapter while deactivated (per IEcopunkScene lifecycle
	// discipline — see TemporalProductionScene::deactivate()), so the
	// dedicated decoder/history sit frozen, not decoding, for however long
	// the scene stays inactive. synchronizeSelectedMedia()'s own
	// unchanged-mediaId no-op (see its comment above) means that if the
	// canonical selection happens to be the SAME media on reactivation as
	// it was at deactivation, the very next synchronizeSelectedMedia()
	// call would silently do nothing — leaving the frozen, stale
	// pre-deactivation history/playhead state in place and indistinguishable
	// from continuously-captured live history.
	//
	// Call this once, before the first post-reactivation
	// synchronizeSelectedMedia() call (see TemporalProductionScene::
	// activate()) — it only forgets this adapter's OWN cached
	// loadedMediaId_, forcing the next synchronizeSelectedMedia() call to
	// reload+clear history unconditionally, even for an unchanged media
	// identity. Does not touch VideoPlaybackService, does not change which
	// media is canonically selected, does not touch VideoPlaybackStatus,
	// and does not alter the two-decoder boundary (DEC-014) — it only
	// resets this adapter's own local "have I already loaded this" memory.
	void invalidateForReactivation() { loadedMediaId_.clear(); }

	// Direct access to the owned buffer for every existing playhead/
	// history/raw-texture accessor (getPlayheadTexture, getPlayheadOffset,
	// getHistoryFrameCount, getRawVideoTexture, ...) — deliberately NOT
	// re-wrapped one-by-one here. This is what lets every existing
	// TFPattern*/TFBackgroundLayer call site keep taking a
	// `TimeOffsetVideoBuffer*` completely unchanged; only the object a
	// scene's ofApp passes that pointer FROM changes (from its own raw
	// member to `&adapter.buffer()`).
	TimeOffsetVideoBuffer& buffer() { return buffer_; }
	const TimeOffsetVideoBuffer& buffer() const { return buffer_; }

	// Diagnostic/test-only counters — NOT a shared contract, NOT consumed
	// by any production HUD/status path (see this session's prompt §7:
	// "the adapter must not publish... as a second shared-video status
	// source"). Exist solely because §11.2/§11.3's required tests
	// ("unchanged mediaId does not reload," "changed mediaId reloads
	// once," "repeated media changes do not leak history state") cannot be
	// proven honestly from decode timing alone with a real ofVideoPlayer —
	// see this session's implementation report, "Exact public API delta,"
	// for the justification of each of these three specifically.
	int reloadCount() const { return reloadCount_; }
	bool lastLoadSucceeded() const { return lastLoadSucceeded_; }
	const std::string& loadedMediaId() const { return loadedMediaId_; }

private:
	TimeOffsetVideoBuffer buffer_;
	std::string loadedMediaId_;
	int reloadCount_ = 0;
	bool lastLoadSucceeded_ = false;
};
