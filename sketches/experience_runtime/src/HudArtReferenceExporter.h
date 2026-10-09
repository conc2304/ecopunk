#pragma once

#include "ExperienceRuntime.h"

#include <string>
#include <vector>

// HudArtReferenceExporter — HUD Art reference-image export (tooling only).
//
// Opt-in: EXPERIENCE_RUNTIME_HUD_ART_EXPORT=1 (output directory overridable
// with EXPERIENCE_RUNTIME_HUD_ART_EXPORT_DIR; default
// <repo>/docs/hud-art/blueprint-v1-reference/). Must run with no skin
// selected (EXPERIENCE_RUNTIME_HUD_SKIN unset) so the comparison frame is the
// plain wireframe.
//
// Defines NO layout of its own. Every rectangle, ID, role and scope comes from
// the LIVE production renderer inside this ExperienceRuntime:
//   HudWireframeRenderer::regionCatalog()        — the 19 HUD-006 regions
//   HudWireframeRenderer::pixelBoundsFor()       — the renderer's own px bounds
//   HudWireframeRenderer::regionInMinimalSlice() — production scope rule
//   HudWireframeRenderer::mediaViewportMesh()    — the triangulated mesh the
//                                                  renderer actually drew
// After rendering real frames it captures one, writes three 1280x720 RGBA
// PNGs plus regions.json and verification.txt, verifies dimensions,
// orientation and the viewport silhouette (incl. the 42 px lower-right
// bevel) against the captured runtime frame, then exits.
class HudArtReferenceExporter {
public:
	static bool isRequested();

	explicit HudArtReferenceExporter(ExperienceRuntime& runtime);

	// Call once per frame from ofApp::update(), AFTER runtime.update(dt).
	void step(float dt);

private:
	void exportAll();
	void check(const std::string& name, bool passed, const std::string& detail = {});
	void note(const std::string& line);

	ExperienceRuntime& runtime_;
	std::string outDir_;
	int frame_ = 0;
	int passes_ = 0;
	int failures_ = 0;
	std::vector<std::string> lines_;
};
