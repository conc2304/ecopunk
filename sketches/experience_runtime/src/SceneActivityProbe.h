// OC-004 -- Scene Visual Activity Measurement Probe: GL half. DIAGNOSTIC, opt-in, ExperienceRuntime-owned.
//
// Inert unless EXPERIENCE_RUNTIME_SCENE_ACTIVITY_PROBE=<output dir> (CSV diagnostics) and/or
// EXPERIENCE_RUNTIME_SCENE_VISUAL_ACTIVITY=1 (OC-004 publication to HudFrameData::sceneVisualActivity, no CSV
// unless the dir is also set) is set. Both unset = production default: no GL objects, no sampling, no publication. Reads the SceneFBO after the
// scene pass and before HUD composition, reduces it on the GPU to a 64x36 luminance grid (own small
// RGBA32F targets, nothing shared with SceneColorSampler), and differences successive grids either on
// the CPU or on the GPU. It never touches SceneColorSampler, the published scene-colour snapshot, HudFrameData
// or scene.activity.overall.
//
//   EXPERIENCE_RUNTIME_SCENE_ACTIVITY_MODE=cpu|gpu|verify|off   (default cpu)
//       cpu    read the 64x36 grid back, difference on the CPU
//       gpu    difference + reduce to 1x1 on the GPU, read back one texel
//       verify both from the same pair; logs |gpu-cpu|
//       off    no sampling; only frames.csv (frame-time baseline)
//   EXPERIENCE_RUNTIME_SCENE_ACTIVITY_READBACK=sync|deferred    (default deferred: read 2 frames later)
//   EXPERIENCE_RUNTIME_SCENE_ACTIVITY_HZ=10
//   EXPERIENCE_RUNTIME_SCENE_ACTIVITY_FRAME_SECONDS=<s>          save a SceneFBO BMP this often (0 = never)
//   EXPERIENCE_RUNTIME_SCENE_ACTIVITY_SEGMENTS=12
//   EXPERIENCE_RUNTIME_SCENE_ACTIVITY_QUANT8=1                  quantise the grid to 8 bits (emulates a Pi RGBA8 path)
//   EXPERIENCE_RUNTIME_SCENE_ACTIVITY_FIXED_REF / _GAMMA / _FLOOR  normaliser parameters
//
// Outputs <dir>/activity.csv (one row per sample event) and <dir>/frames.csv (one row per presented frame).
#pragma once

#include "SceneActivityCore.h"
#include "SceneVisualActivityTracker.h"

#include "ofTexture.h"

#include <memory>
#include <string>

namespace sceneactivity {

class SceneActivityProbe {
public:
	SceneActivityProbe();
	~SceneActivityProbe();
	SceneActivityProbe(const SceneActivityProbe&) = delete;
	SceneActivityProbe& operator=(const SceneActivityProbe&) = delete;

	static bool requested(); // env var set (read once)
	// OC-004 production publication: true only when EXPERIENCE_RUNTIME_SCENE_VISUAL_ACTIVITY=1 (default OFF).
	bool publishing() const;
	const VisualActivityTracker& tracker() const;

	void beginGeneration(uint64_t generation, const std::string& sceneId, double now);
	void onLiveFrame(const ofTexture& sceneTexture, double now);
	void onStaticFrame(double now);
	// Once per presented frame, after the scene pass: completes deferred readbacks, stale rows, frames.csv.
	void poll(double now);

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace sceneactivity
