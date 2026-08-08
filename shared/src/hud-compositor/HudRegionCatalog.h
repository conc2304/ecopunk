#pragma once

// ============================================================================
// HudRegionCatalog.h — the provisional HUD-owned region catalog.
//
// Bounds below are placeholders only — normalized [0,1] fractions of a
// notional 1280x720 canvas, chosen to be visually non-overlapping enough
// for the Validation Studio's wireframe mode to be legible, NOT a claim
// about the frozen HUD Blueprint (Roadmap Phase 10, not started). Every
// region's bounds may move freely once real layout work begins; nothing
// in HudProfileCompiler or the widget layer depends on these specific
// numbers, only on region *identity* and *role*.
//
// media_viewport is deliberately NOT a region here — this task's own
// region-ID list (given verbatim in the prompt) does not include one, and
// per docs/probes/hud-runtime-validation-studio-probe.md §18's
// recommended next task, MediaViewportMesh is separate, unstarted work.
// This increment's wireframe covers only the L-shaped chrome/telemetry
// side of the canonical layout, not the scene viewport itself.
// ============================================================================

#include "HudWidgetTypes.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace hudpresent {

enum class HudRegionRole : uint8_t {
	Universal,
	Controls,
	Flexible,
	Overlay
};

struct HudNormalizedBounds {
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
};

struct HudRegionDescriptor {
	std::string regionId;
	HudRegionRole role = HudRegionRole::Universal;
	HudNormalizedBounds bounds; // PLACEHOLDER — see file header
	std::vector<HudWidgetType> acceptedWidgetTypes;
	int maxBindings = 1;
	bool historyAllowed = false;
};

class HudRegionCatalog {
public:
	HudRegionCatalog();

	const HudRegionDescriptor* find(const std::string& regionId) const;
	const std::vector<HudRegionDescriptor>& all() const { return regions_; }

	bool acceptsWidget(const std::string& regionId, HudWidgetType type) const;

private:
	std::vector<HudRegionDescriptor> regions_;
};

} // namespace hudpresent
