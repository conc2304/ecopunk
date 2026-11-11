#pragma once

// Precomputed clip activity (OC-004 cost-effective path). OF-free.
//
// Instead of sampling the rendered scene every frame, each shared-media clip is measured ONCE, offline
// (tools/clip-activity/generate-clip-activity.py): 64x36 box-mean luma, mean absolute difference between
// successive frames at a fixed step, stored as `raw` values in 0..1 (same quantity as the live probe's raw).
// At runtime the active clip's playhead position is looked up in that curve and fed to the same
// VisualActivityTracker / snapshot the live probe uses, so the HUD contract (scene.visual.activity: state +
// unsmoothed fixed-log level) is unchanged. Cost per frame: one table lookup. No GL, no readback.
//
// What it measures is the CLIP, not the composited scene: effects, overlays and scene-generated content are
// not in it, and a scene that does not show the shared clip still reports the clip's activity while one is
// playing. The live sampler (EXPERIENCE_RUNTIME_SCENE_VISUAL_ACTIVITY=1) measures the scene and takes
// precedence when enabled.

#include "SceneVisualActivityTracker.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace clipactivity {

struct Curve {
	double stepSeconds = 0.1;     // spacing of raw[]; raw[k] is the difference across (k*step, (k+1)*step)
	double durationSeconds = 0.0; // clip length as measured
	std::vector<float> raw;       // mean |delta luma| per step, 0..1
};

class ClipActivityIndex {
public:
	// Rejects a curve with no samples, a non-positive step, or any non-finite / negative value.
	bool add(const std::string& fileName, Curve c) {
		if (fileName.empty() || c.raw.empty() || !(c.stepSeconds > 0.0)) return false;
		for (float v : c.raw) if (!std::isfinite(v) || v < 0.0f) return false;
		curves_[fileName] = std::move(c);
		return true;
	}
	void clear() { curves_.clear(); }
	bool empty() const { return curves_.empty(); }
	size_t size() const { return curves_.size(); }
	bool has(const std::string& fileName) const { return curves_.count(fileName) != 0; }

	// Raw activity at `positionSeconds` into the clip's current loop (linear between step centres; clamped
	// at the ends). nullopt for an unknown clip or a position that is not a finite, non-negative number.
	std::optional<double> rawAt(const std::string& fileName, double positionSeconds) const {
		const auto it = curves_.find(fileName);
		if (it == curves_.end() || !std::isfinite(positionSeconds) || positionSeconds < 0.0) return std::nullopt;
		const Curve& c = it->second;
		const double u = std::clamp(positionSeconds / c.stepSeconds - 0.5, 0.0, static_cast<double>(c.raw.size() - 1));
		const size_t i = static_cast<size_t>(u);
		const size_t j = std::min(i + 1, c.raw.size() - 1);
		const double f = u - static_cast<double>(i);
		return c.raw[i] + (c.raw[j] - c.raw[i]) * f;
	}

private:
	std::map<std::string, Curve> curves_;
};

// Turns "which clip is playing, and where" into the visual-activity snapshot, once per frame.
class ClipActivityFeeder {
public:
	explicit ClipActivityFeeder(const ClipActivityIndex& index) : index_(index) {}

	void beginGeneration(uint64_t generation, const std::string& sceneId) {
		tracker_.beginGeneration(generation, sceneId);
		feedNext_ = true;
	}

	// sceneLive:  the active scene rendered this frame (false = transition static frame).
	// fileName/positionSeconds: the shared video's current clip and loop position; empty/nullopt when no clip
	// is ready (the caller passes health == Ready only).
	void update(double now, bool sceneLive, const std::string& fileName, std::optional<double> positionSeconds) {
		if (!sceneLive) { // the retained frame is not a measurement
			tracker_.onStatic();
			feedNext_ = true;
			return;
		}
		const auto raw = positionSeconds ? index_.rawAt(fileName, *positionSeconds) : std::nullopt;
		if (!raw) { // no clip, or one that was never measured: honestly unavailable, never zero
			tracker_.onUnsupported();
			feedNext_ = true;
			return;
		}
		if (feedNext_ || now - lastFeedAt_ >= tracker_.period()) {
			tracker_.onSample(now, *raw);
			lastFeedAt_ = now;
			feedNext_ = false;
		}
	}

	const std::shared_ptr<const scenevisual::SceneVisualActivitySnapshot>& publish(double now) {
		return publisher_.publish(tracker_, now);
	}
	const sceneactivity::VisualActivityTracker& tracker() const { return tracker_; }

private:
	const ClipActivityIndex& index_;
	sceneactivity::VisualActivityTracker tracker_;
	sceneactivity::VisualActivityPublisher publisher_;
	double lastFeedAt_ = 0.0;
	bool feedNext_ = true;
};

} // namespace clipactivity
