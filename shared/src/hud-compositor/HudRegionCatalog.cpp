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
	// Content map (HUD content relocation, 2026-10-23; geometry unchanged):
	//   scene_title    scene title + [Scene x/y]        media_title   "Mode: <name>   x/y"
	//   primary_state  media title + progress           activity      primary-state badge
	//   effect_summary activity bar                     timing        active effects (+health)
	//   flexible.secondary_a  active time               overlay.health      transition (+scene health)
	//   overlay.transition    recent activity log
	// The accepted widget sets below were widened to allow exactly that.
	regions_.push_back(makeRegion("universal.scene_title", HudRegionRole::Universal,
		{0.66f, 0.03f, 0.32f, 0.06f}, {W::Label}, 1, false));

	regions_.push_back(makeRegion("universal.media_title", HudRegionRole::Universal,
		{0.66f, 0.10f, 0.32f, 0.05f}, {W::Label}, 1, false));

	regions_.push_back(makeRegion("universal.primary_state", HudRegionRole::Universal,
		{0.66f, 0.16f, 0.32f, 0.06f}, {W::Label, W::StatusBadge}, 1, false));

	regions_.push_back(makeRegion("universal.activity", HudRegionRole::Universal,
		{0.66f, 0.23f, 0.32f, 0.06f}, {W::NumericValue, W::ProgressBar, W::StatusBadge}, 1, false));

	// maxBindings=2 (Architecture-Closure Session, DEC-015): the chips
	// binding (effects.active, dominance-derived) plus a StatusBadge
	// binding (effects.health, direct from the frozen EffectHealth enum)
	// stacked in the same region — same multi-binding-per-region slicing
	// HudWireframeRenderer::draw() already applies to overlay.transition,
	// reused here rather than inventing new geometry, per the closure
	// plan's "preserve canonical wireframe geometry" instruction.
	regions_.push_back(makeRegion("universal.effect_summary", HudRegionRole::Universal,
		{0.66f, 0.30f, 0.32f, 0.06f}, {W::EffectChips, W::StatusBadge, W::ProgressBar, W::NumericValue}, 2, false));

	regions_.push_back(makeRegion("universal.timing", HudRegionRole::Universal,
		{0.66f, 0.37f, 0.32f, 0.05f}, {W::NumericValue, W::Label, W::EffectChips}, 1, false));

	// System Telemetry v1: the two trace regions host the five system-vitals
	// sparklines as stacked slots (draw-time slicing, like effect_summary):
	// activity_trace 3 x 24 px (FPS, FRAME MAX, CPU), secondary_trace
	// 2 x 36 px (GPU TEMP, RSS). maxBindings 1 -> 3 / 2 is the only catalog
	// change; IDs, numbering and bounds are untouched.
	regions_.push_back(makeRegion("universal.activity_trace", HudRegionRole::Universal,
		{0.66f, 0.43f, 0.32f, 0.10f}, {W::Sparkline}, 3, true));

	regions_.push_back(makeRegion("universal.secondary_trace", HudRegionRole::Universal,
		{0.66f, 0.54f, 0.32f, 0.10f}, {W::Sparkline}, 2, true));

	// -- Controls regions (HUD-006 v2: six 72x72 squares) -------------------
	// Row y = 633.6..705.6 (0.88 + 0.10); gaps 12.8 px in a group, 32 px
	// between groups: [PREV SCENE][NEXT SCENE][MODE] [PREV MEDIA][NEXT MEDIA]
	// [FULLSCREEN]. controls.reseed (v1) is retired. Normalized against the
	// 1280x720 canvas: x / 1280, y and h / 720, w = 72 / 1280 = 0.05625.
	regions_.push_back(makeRegion("controls.scene_previous", HudRegionRole::Controls,
		{0.02f, 0.88f, 0.05625f, 0.10f}, {W::Label}, 1, false));
	regions_.push_back(makeRegion("controls.scene_next", HudRegionRole::Controls,
		{0.08625f, 0.88f, 0.05625f, 0.10f}, {W::Label}, 1, false));
	regions_.push_back(makeRegion("controls.mode", HudRegionRole::Controls,
		{0.1525f, 0.88f, 0.05625f, 0.10f}, {W::Label}, 1, false));
	regions_.push_back(makeRegion("controls.media_previous", HudRegionRole::Controls,
		{0.23375f, 0.88f, 0.05625f, 0.10f}, {W::Label}, 1, false));
	regions_.push_back(makeRegion("controls.media_next", HudRegionRole::Controls,
		{0.30f, 0.88f, 0.05625f, 0.10f}, {W::Label}, 1, false));
	regions_.push_back(makeRegion("controls.fullscreen", HudRegionRole::Controls,
		{0.38125f, 0.88f, 0.05625f, 0.10f}, {W::Label}, 1, false));

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
		{0.02f, 0.03f, 0.20f, 0.06f}, {W::StatusBadge, W::Label, W::ProgressBar}, 2, false));

	// maxBindings=3: ProgressBar (progress) + 2 Labels (phase, message) —
	// see HudPresentationProfile.cpp's "overlay.transition.phase_label"/
	// "overlay.transition.message_label" bindings, added this session to
	// satisfy this task's "manager transition… phase/progress/message"
	// requirement (progress alone was Session 1's whole overlay).
	regions_.push_back(makeRegion("overlay.transition", HudRegionRole::Overlay,
		{0.02f, 0.10f, 0.20f, 0.08f}, {W::ProgressBar, W::Label}, 3, false));

	// -- Seventh navigation control (operator-directed narrow exception to the
	// HUD-006 v2 six-button row): MENU opens the Experience Settings page.
	// APPENDED (existing regions keep their numbering); same 72x72 square and
	// row y as the six. Placement derived from canonical coordinates: the
	// bottom navigation panel's inner right edge is x = 723 px in every shipped
	// skin shell (last panel pixel 722), and the existing NEXT MEDIA -> FULLSCREEN
	// edge gap is 456 -> 488 = 32 px, so MENU.right = 723 - 32 = 691 and
	// MENU.x = 691 - 72 = 619 px (619 / 1280 = 0.48359375). It clears FULLSCREEN
	// (right edge 560) by 59 px.
	regions_.push_back(makeRegion("controls.menu", HudRegionRole::Controls,
		{0.48359375f, 0.88f, 0.05625f, 0.10f}, {W::Label}, 1, false));

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
