#include "HudRegionCatalog.h"

#include <algorithm>

namespace hudpresent {

namespace {

HudRegionDescriptor makeRegion(
	std::string id,
	HudRegionRole role,
	HudNormalizedBounds bounds,
	std::vector<HudWidgetType> accepted,
	int maxBindings,
	bool historyAllowed) {
	HudRegionDescriptor r;
	r.regionId = std::move(id);
	r.role = role;
	r.bounds = bounds;
	r.acceptedWidgetTypes = std::move(accepted);
	r.maxBindings = maxBindings;
	r.historyAllowed = historyAllowed;
	return r;
}

} // namespace

HudRegionCatalog::HudRegionCatalog() {
	using W = HudWidgetType;

	// -- Universal regions (task §3 list, in the same order) -----------
	regions_.push_back(makeRegion("universal.scene_title", HudRegionRole::Universal,
		{0.66f, 0.03f, 0.32f, 0.06f}, {W::Label}, 1, false));

	regions_.push_back(makeRegion("universal.media_title", HudRegionRole::Universal,
		{0.66f, 0.10f, 0.32f, 0.05f}, {W::Label}, 1, false));

	regions_.push_back(makeRegion("universal.primary_state", HudRegionRole::Universal,
		{0.66f, 0.16f, 0.32f, 0.06f}, {W::StatusBadge}, 1, false));

	regions_.push_back(makeRegion("universal.activity", HudRegionRole::Universal,
		{0.66f, 0.23f, 0.32f, 0.06f}, {W::NumericValue, W::ProgressBar}, 1, false));

	// maxBindings=2 (Architecture-Closure Session, DEC-015): the chips
	// binding (effects.active, dominance-derived) plus a StatusBadge
	// binding (effects.health, direct from the frozen EffectHealth enum)
	// stacked in the same region — same multi-binding-per-region slicing
	// HudWireframeRenderer::draw() already applies to overlay.transition,
	// reused here rather than inventing new geometry, per the closure
	// plan's "preserve canonical wireframe geometry" instruction.
	regions_.push_back(makeRegion("universal.effect_summary", HudRegionRole::Universal,
		{0.66f, 0.30f, 0.32f, 0.06f}, {W::EffectChips, W::StatusBadge}, 2, false));

	regions_.push_back(makeRegion("universal.timing", HudRegionRole::Universal,
		{0.66f, 0.37f, 0.32f, 0.05f}, {W::NumericValue, W::Label}, 1, false));

	regions_.push_back(makeRegion("universal.activity_trace", HudRegionRole::Universal,
		{0.66f, 0.43f, 0.32f, 0.10f}, {W::Sparkline}, 1, true));

	regions_.push_back(makeRegion("universal.secondary_trace", HudRegionRole::Universal,
		{0.66f, 0.54f, 0.32f, 0.10f}, {W::Sparkline}, 1, true));

	// -- Controls regions ------------------------------------------------
	regions_.push_back(makeRegion("controls.scene_previous", HudRegionRole::Controls,
		{0.02f, 0.90f, 0.08f, 0.06f}, {W::Label}, 1, false));
	regions_.push_back(makeRegion("controls.scene_next", HudRegionRole::Controls,
		{0.12f, 0.90f, 0.08f, 0.06f}, {W::Label}, 1, false));
	regions_.push_back(makeRegion("controls.media_previous", HudRegionRole::Controls,
		{0.24f, 0.90f, 0.08f, 0.06f}, {W::Label}, 1, false));
	regions_.push_back(makeRegion("controls.media_next", HudRegionRole::Controls,
		{0.34f, 0.90f, 0.08f, 0.06f}, {W::Label}, 1, false));
	regions_.push_back(makeRegion("controls.reseed", HudRegionRole::Controls,
		{0.46f, 0.90f, 0.10f, 0.06f}, {W::Label}, 1, false));

	// -- Flexible scene-specific regions ---------------------------------
	// flexible.primary accepts the broadest set — it is where each scene's
	// single richest scene-specific widget lands (per this task's §13
	// "one flexible primary widget"). maxBindings=1 is the load-bearing
	// rule HudProfileCompiler's "mutually exclusive flexible-region
	// occupancy" check enforces.
	regions_.push_back(makeRegion("flexible.primary", HudRegionRole::Flexible,
		{0.66f, 0.64f, 0.32f, 0.16f},
		{W::MetadataCard, W::ChannelStrip, W::EffectChips, W::ProgressRing, W::Sparkline, W::Timeline, W::AmbientField},
		1, true));

	regions_.push_back(makeRegion("flexible.secondary_a", HudRegionRole::Flexible,
		{0.66f, 0.81f, 0.15f, 0.06f},
		{W::NumericValue, W::ProgressBar, W::ProgressRing, W::StatusBadge, W::Sparkline, W::AmbientField},
		1, true));

	regions_.push_back(makeRegion("flexible.secondary_b", HudRegionRole::Flexible,
		{0.83f, 0.81f, 0.15f, 0.06f},
		{W::NumericValue, W::ProgressBar, W::ProgressRing, W::StatusBadge, W::Sparkline, W::AmbientField},
		1, true));

	// -- Media viewport (this task's §13 "canonical wireframe candidate")
	// The upper-left shaped viewport MediaViewportMesh clips into — see
	// MediaViewportMesh.h. Occupies the space the universal/controls/
	// flexible/overlay regions above and below don't: those already form
	// an L-shape (right-hand column + bottom row + top-left overlay
	// corner), leaving this rectangle as the natural "upper-left" slot.
	// Deliberately NOT a HudSlotBinding target — no widget type accepts
	// it (empty acceptedWidgetTypes list before the BindingPlaceholder
	// append below) — HudWireframeRenderer draws MediaViewportMesh into
	// this region directly, bypassing the binding/widget pipeline
	// entirely, since it isn't a semantic-slot value at all.
	regions_.push_back(makeRegion("media_viewport", HudRegionRole::Universal,
		{0.02f, 0.16f, 0.60f, 0.70f}, {}, 0, false));

	// -- Overlay regions ---------------------------------------------------
	regions_.push_back(makeRegion("overlay.health", HudRegionRole::Overlay,
		{0.02f, 0.03f, 0.20f, 0.06f}, {W::StatusBadge, W::Label}, 1, false));

	// maxBindings=3: ProgressBar (progress) + 2 Labels (phase, message) —
	// see HudPresentationProfile.cpp's "overlay.transition.phase_label"/
	// "overlay.transition.message_label" bindings, added this session to
	// satisfy this task's "manager transition… phase/progress/message"
	// requirement (progress alone was Session 1's whole overlay).
	regions_.push_back(makeRegion("overlay.transition", HudRegionRole::Overlay,
		{0.02f, 0.10f, 0.20f, 0.08f}, {W::ProgressBar, W::Label}, 3, false));

	// Every region in this catalog is also a legal home for the
	// tooling-only BindingPlaceholder widget (Validation Studio's
	// "show visible binding-error placeholder" requirement, §6 of this
	// task) — appended here rather than repeated in every makeRegion()
	// call above.
	for (auto& region : regions_) {
		region.acceptedWidgetTypes.push_back(W::BindingPlaceholder);
	}
}

const HudRegionDescriptor* HudRegionCatalog::find(const std::string& regionId) const {
	auto it = std::find_if(regions_.begin(), regions_.end(),
		[&](const HudRegionDescriptor& r) { return r.regionId == regionId; });
	return it == regions_.end() ? nullptr : &(*it);
}

bool HudRegionCatalog::acceptsWidget(const std::string& regionId, HudWidgetType type) const {
	const auto* region = find(regionId);
	if (!region) return false;
	return std::find(region->acceptedWidgetTypes.begin(), region->acceptedWidgetTypes.end(), type)
		!= region->acceptedWidgetTypes.end();
}

} // namespace hudpresent
