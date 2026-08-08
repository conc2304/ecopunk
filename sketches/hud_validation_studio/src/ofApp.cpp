#include "ofApp.h"

#include "AllocationCounter.h"

#include <cstdlib>

using namespace hudpresent;

namespace {

const char* widgetTypeName(HudWidgetType t) {
	switch (t) {
		case HudWidgetType::Label: return "Label";
		case HudWidgetType::StatusBadge: return "StatusBadge";
		case HudWidgetType::NumericValue: return "NumericValue";
		case HudWidgetType::ProgressBar: return "ProgressBar";
		case HudWidgetType::ProgressRing: return "ProgressRing";
		case HudWidgetType::Sparkline: return "Sparkline";
		case HudWidgetType::EffectChips: return "EffectChips";
		case HudWidgetType::MetadataCard: return "MetadataCard";
		case HudWidgetType::ChannelStrip: return "ChannelStrip";
		case HudWidgetType::AmbientField: return "AmbientField";
		case HudWidgetType::Timeline: return "Timeline";
		case HudWidgetType::BindingPlaceholder: return "BindingPlaceholder";
	}
	return "?";
}

const char* missingPolicyName(HudMissingPolicy p) {
	switch (p) {
		case HudMissingPolicy::Hide: return "Hide";
		case HudMissingPolicy::Placeholder: return "Placeholder";
		case HudMissingPolicy::RetainLastValid: return "RetainLastValid";
		case HudMissingPolicy::DimLastValid: return "DimLastValid";
		case HudMissingPolicy::UseFallbackSource: return "UseFallbackSource";
		case HudMissingPolicy::UseAmbientFallback: return "UseAmbientFallback";
	}
	return "?";
}

} // namespace

void ofApp::setup() {
	ofSetFrameRate(60);
	ofSetVerticalSync(true);
	ofSetLogLevel(OF_LOG_NOTICE);
	ofBackground(8, 10, 9);

	renderer_.setup(1280.0f, 720.0f);
	renderer_.setStudioMode(debugMode_);

	switchScene(scenarioRegistry_.sceneIds().front());

	buildSyntheticMediaTexture();
	realFrameCases_ = buildRealFrameCases();

	baselineCases_ = {
		{"blob-region-prototype", FakeCasePhase::Nominal, 1.0f, "blob-region-prototype_Nominal_t1.00"},
		{"contour-portrait", FakeCasePhase::Nominal, 1.0f, "contour-portrait_Nominal_t1.00"},
		{"temporal-fields", FakeCasePhase::Nominal, 1.0f, "temporal-fields_Nominal_t1.00"},
		{"fragment-trail", FakeCasePhase::Nominal, 1.0f, "fragment-trail_Nominal_t1.00"},
		{"quadrant-crosshair", FakeCasePhase::Nominal, 1.0f, "quadrant-crosshair_Nominal_t1.00_MultiChannel"},
		{"blueprint_emergence", FakeCasePhase::Nominal, 1.0f, "blueprint_emergence_Nominal_t1.00"},
		{"blob-region-prototype", FakeCasePhase::MissingOptional, 1.0f, "blob-region-prototype_MissingOptional_t1.00"},
		{"blob-region-prototype", FakeCasePhase::Loading, 1.0f, "blob-region-prototype_Loading_t1.00"},
		{"blob-region-prototype", FakeCasePhase::Failed, 1.0f, "blob-region-prototype_Failed_t1.00"},
		{"quadrant-crosshair", FakeCasePhase::LongestStrings, 1.0f, "quadrant-crosshair_LongestStrings_t1.00"},
		// Architecture-Closure Session additions — canonical screenshot
		// matrix §6.2's remaining "health/manager states" and "geometry"
		// gaps.
		{"blob-region-prototype", FakeCasePhase::Degraded, 1.0f, "blob-region-prototype_Degraded_t1.00"},
		{"quadrant-crosshair", FakeCasePhase::MaxEffectList, 1.0f, "quadrant-crosshair_MaxEffectList_t1.00"},
		{"quadrant-crosshair", FakeCasePhase::MaxMetricCount, 1.0f, "quadrant-crosshair_MaxMetricCount_t1.00"},
	};

	if (std::getenv("HUD_STUDIO_AUTOCAPTURE") != nullptr) {
		autoCaptureThenExit_ = true;
		runBaselineCapture();
	}
}

void ofApp::switchScene(const std::string& sceneId) {
	scenario_ = scenarioRegistry_.create(sceneId);
	if (!scenario_) return;
	currentSceneId_ = sceneId;
	scenario_->reset(0);
	renderer_.setScene(sceneId);
}

void ofApp::jumpToPhase(FakeCasePhase phase) {
	elapsedSeconds_ = FakeHudScenarioBase::elapsedSecondsForPhase(phase, 1.0f);
}

// Engineering Session 2 — a small, fully procedural (no disk asset, so no
// dependency on this repo's media catalog and no non-determinism from a
// missing/changed file) checkerboard pattern, deliberately a different
// aspect ratio (640x360, 16:9) than the media_viewport region's own bounds
// (0.60 x 0.70 of a 1280x720 canvas, i.e. NOT 16:9) — this exercises
// MediaViewportMesh's cover-fit UV cropping visibly (alternating light/
// dark squares make the crop boundary and any UV-mapping error obvious at
// a glance, unlike a flat-color fill). Colors and grid are fixed literals,
// not randomized — same screenshot every run.
void ofApp::buildSyntheticMediaTexture() {
	const int w = 640;
	const int h = 360;
	const int cell = 40;
	ofPixels pixels;
	pixels.allocate(w, h, OF_PIXELS_RGB);
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			bool light = ((x / cell) + (y / cell)) % 2 == 0;
			ofColor c = light ? ofColor(90, 200, 170) : ofColor(20, 40, 38);
			pixels.setColor(x, y, c);
		}
	}
	syntheticMediaTexture_.allocate(pixels);
	syntheticMediaTexture_.loadData(pixels);
}

namespace {

// Shared literal defaults so every RealFrameCase below only has to state
// what's actually distinctive about it — reduces this function to a
// readable list of deltas instead of ~15 fully-repeated struct literals.
HudFrameData baseRealFrame(const std::string& sceneId) {
	HudFrameData frame;
	frame.schemaVersion = 1;
	frame.scene.schemaVersion = 1;
	frame.scene.sceneId = sceneId;
	frame.scene.displayName = "Real-Frame Studio Case";
	frame.scene.health = SceneHealth::Ready;
	frame.sceneManager.activeSceneId = sceneId;
	frame.sceneManager.transitionPhase = SceneTransitionPhase::Idle;
	frame.sceneManager.transitionProgress = 0.0f;
	frame.runtime.fps = 60.0f;
	frame.runtime.frameTimeMs = 16.7f;
	frame.sceneFrame.frameNumber = 1;
	return frame;
}

} // namespace

std::vector<ofApp::RealFrameCase> ofApp::buildRealFrameCases() const {
	std::vector<RealFrameCase> cases;

	// -- Manager transition overlay (SceneManager-owned phase/progress/
	//    message, bound via overlay.transition — see HudRegionCatalog.cpp/
	//    HudPresentationProfile.cpp's "phase_label"/"message_label"
	//    bindings added this session). Deliberately an sceneId
	//    ("real-frame-demo") this session's HudPresentationProfile
	//    registry does not recognize — same "unknown sceneId degrades to
	//    universal+overlay bindings only" behavior already proven for
	//    ExperienceRuntime's own FakeScene (sceneId "fake-scene"); see
	//    HudProfileCompiler::compile()'s own comment. -------------------
	{
		HudFrameData f = baseRealFrame("real-frame-demo");
		f.sceneManager.transitionPhase = SceneTransitionPhase::FadingOut;
		f.sceneManager.transitionProgress = 0.35f;
		f.sceneManager.message = "Fading to next scene";
		cases.push_back({"real_manager_transition_fading_out", f});
	}
	{
		HudFrameData f = baseRealFrame("real-frame-demo");
		f.sceneManager.transitionPhase = SceneTransitionPhase::Loading;
		f.sceneManager.transitionProgress = 0.72f;
		f.sceneManager.message = "Preparing next scene";
		cases.push_back({"real_manager_transition_loading", f});
	}
	{
		// Architecture-Closure Session addition — screenshot matrix §6.2's
		// "Manager FadingIn" case.
		HudFrameData f = baseRealFrame("real-frame-demo");
		f.sceneManager.transitionPhase = SceneTransitionPhase::FadingIn;
		f.sceneManager.transitionProgress = 0.88f;
		f.sceneManager.message = "Arriving at next scene";
		cases.push_back({"real_manager_transition_fading_in", f});
	}
	{
		// Architecture-Closure Session addition — screenshot matrix §6.2's
		// "Manager Failed/message" case.
		HudFrameData f = baseRealFrame("real-frame-demo");
		f.sceneManager.transitionPhase = SceneTransitionPhase::Failed;
		f.sceneManager.transitionProgress = 0.0f;
		f.sceneManager.message = "Scene transition failed to complete";
		cases.push_back({"real_manager_transition_failed", f});
	}

	// -- Effects: the 10 deterministic cases the Architecture-Closure
	//    Session's closure plan §11/§13 requires, now against the REAL,
	//    frozen HudFrameData.effects sibling snapshot (DEC-015/DEC-016) —
	//    see HudRealFrameResolver.cpp's resolveEffects() for exactly what
	//    each of these proves. ------------------------------------------
	using videoeffects::EffectActivitySlot;
	using videoeffects::EffectActivityStatus;
	using videoeffects::EffectHealth;
	using videoeffects::EvolutionPhase;
	{
		// effects.absent — no authoritative snapshot this frame.
		HudFrameData f = baseRealFrame("real-frame-demo");
		f.effects = std::nullopt;
		cases.push_back({"effects_absent", f});
	}
	{
		// effects.empty.ready — snapshot exists, zero active effects,
		// health Ready. A fully valid, common state, not a failure.
		HudFrameData f = baseRealFrame("real-frame-demo");
		EffectActivityStatus status;
		status.health = EffectHealth::Ready;
		f.effects = status;
		cases.push_back({"effects_empty_ready", f});
	}
	{
		// effects.one.ready — a single active slot.
		HudFrameData f = baseRealFrame("real-frame-demo");
		EffectActivityStatus status;
		status.health = EffectHealth::Ready;
		EffectActivitySlot slot;
		slot.slotId = "primary";
		slot.effectId = "heatmap_recolor";
		slot.displayName = "Heatmap Recolor";
		slot.phase = EvolutionPhase::Holding;
		slot.prominence = 1.0f;
		status.slots = {slot};
		f.effects = status;
		cases.push_back({"effects_one_ready", f});
	}
	{
		// effects.multi.ready — several active slots.
		HudFrameData f = baseRealFrame("real-frame-demo");
		EffectActivityStatus status;
		status.health = EffectHealth::Ready;
		EffectActivitySlot a;
		a.slotId = "quadrant_0";
		a.effectId = "heatmap_recolor";
		a.prominence = 0.8f;
		EffectActivitySlot b;
		b.slotId = "quadrant_1";
		b.effectId = "channel_shift";
		b.prominence = 0.6f;
		status.slots = {a, b};
		f.effects = status;
		cases.push_back({"effects_multi_ready", f});
	}
	{
		// effects.multi.dominance-not-first — the higher-prominence slot
		// is inserted SECOND, proving dominance orders by prominence, not
		// vector/insertion order (HudRealFrameResolver's own resolver
		// unit test — test_real_effects_dominance_not_first_in_vector —
		// proves this at the resolver level; this case proves it's also
		// visually true end-to-end through the real widget chain).
		HudFrameData f = baseRealFrame("real-frame-demo");
		EffectActivityStatus status;
		status.health = EffectHealth::Ready;
		EffectActivitySlot low;
		low.slotId = "quadrant_0";
		low.effectId = "desaturate";
		low.prominence = 0.2f;
		EffectActivitySlot high;
		high.slotId = "quadrant_1";
		high.effectId = "bioluminescence";
		high.prominence = 0.9f;
		status.slots = {low, high}; // low-prominence FIRST in the vector
		f.effects = status;
		cases.push_back({"effects_multi_dominance_not_first", f});
	}
	{
		// effects.transition — the dominant slot is actively transitioning
		// at a bounded mid-progress value.
		HudFrameData f = baseRealFrame("real-frame-demo");
		EffectActivityStatus status;
		status.health = EffectHealth::Ready;
		EffectActivitySlot slot;
		slot.slotId = "primary";
		slot.effectId = "contour_glow";
		slot.phase = EvolutionPhase::Transitioning;
		slot.transitionProgress01 = 0.42f;
		slot.prominence = 1.0f;
		status.slots = {slot};
		f.effects = status;
		cases.push_back({"effects_transition", f});
	}
	{
		// effects.degraded.empty — Degraded health with zero active
		// slots; the two fields are independent (empty != failure, and
		// Degraded != inferred from slot count either way).
		HudFrameData f = baseRealFrame("real-frame-demo");
		EffectActivityStatus status;
		status.health = EffectHealth::Degraded;
		status.messageId = "effects.message.degraded_fallback";
		f.effects = status;
		cases.push_back({"effects_degraded_empty", f});
	}
	{
		// effects.degraded.active — Degraded health WITH active slots
		// (still presented, per DEC-016: degraded is not "hide effects").
		HudFrameData f = baseRealFrame("real-frame-demo");
		EffectActivityStatus status;
		status.health = EffectHealth::Degraded;
		status.messageId = "effects.message.degraded_fallback";
		EffectActivitySlot slot;
		slot.slotId = "primary";
		slot.effectId = "channel_shift";
		slot.prominence = 1.0f;
		status.slots = {slot};
		f.effects = status;
		cases.push_back({"effects_degraded_active", f});
	}
	{
		// effects.failed — no usable active media/effect right now.
		HudFrameData f = baseRealFrame("real-frame-demo");
		EffectActivityStatus status;
		status.health = EffectHealth::Failed;
		status.messageId = "effects.message.load_failed";
		f.effects = status;
		cases.push_back({"effects_failed", f});
	}
	{
		// effects.no-intensity — DEC-016: prominence must NEVER populate
		// effects.intensity, proven here with a maximal-prominence slot
		// (see HudRealFrameResolver.cpp's resolveEffects() and
		// test_real_effects_intensity_always_absent()). No widget in this
		// profile currently binds effects.intensity to anything visible —
		// this case exists for the screenshot-matrix record, not because
		// the render differs from effects_one_ready.
		HudFrameData f = baseRealFrame("real-frame-demo");
		EffectActivityStatus status;
		status.health = EffectHealth::Ready;
		EffectActivitySlot slot;
		slot.slotId = "primary";
		slot.effectId = "bioluminescence";
		slot.prominence = 1.0f; // maximal — must not leak into effects.intensity
		status.slots = {slot};
		f.effects = status;
		cases.push_back({"effects_no_intensity", f});
	}
	{
		// Compatibility-fallback demo — explicitly opted in, for tooling/
		// fixture demonstration only (DEC-015 §10.5). Frame has NO
		// authoritative snapshot but a populated SceneHudStatus::activeEffects;
		// this is the ONLY case in this app that enables the fallback flag.
		HudFrameData f = baseRealFrame("real-frame-demo");
		f.effects = std::nullopt;
		f.scene.activeEffects = {"Heatmap Recolor (compatibility)", "Channel Shift (compatibility)"};
		cases.push_back({"effects_compatibility_fallback_demo", f, /*compatibilityFallback=*/true});
	}

	// -- Media: HudFrameData.video absent entirely (RuntimeServices'
	//    VideoPlaybackService never configured) vs. present with
	//    VideoPlaybackHealth::Loading vs. present+Ready across all three
	//    title-fallback tiers (titleId -> fallbackDisplayTitle -> mediaId)
	//    this session's HudRealFrameResolver::resolveMediaTitle()
	//    implements. --------------------------------------------------
	{
		HudFrameData f = baseRealFrame("real-frame-demo");
		f.video = std::nullopt;
		cases.push_back({"real_media_unavailable", f});
	}
	{
		HudFrameData f = baseRealFrame("real-frame-demo");
		VideoPlaybackStatus v;
		v.health = VideoPlaybackHealth::Loading;
		f.video = v;
		cases.push_back({"real_media_loading", f});
	}
	{
		HudFrameData f = baseRealFrame("real-frame-demo");
		VideoPlaybackStatus v;
		v.health = VideoPlaybackHealth::Ready;
		v.mediaId = "demo-clip-01";
		v.titleId = "media.title.coastal_drift"; // resolves through vocabulary
		v.fallbackDisplayTitle = "Coastal Drift Study"; // present, but titleId wins
		v.playbackProgress = 0.42f;
		v.holdProgress = 0.10f;
		v.canSelectPrevious = true;
		v.canSelectNext = true;
		f.video = v;
		cases.push_back({"real_media_ready_titleid_tier", f});
	}
	{
		HudFrameData f = baseRealFrame("real-frame-demo");
		VideoPlaybackStatus v;
		v.health = VideoPlaybackHealth::Ready;
		v.mediaId = "clip-042";
		v.titleId = std::nullopt; // falls through to fallbackDisplayTitle
		v.fallbackDisplayTitle = "Coastal Drift Study (unregistered vocab id)";
		v.playbackProgress = 0.18f;
		v.canSelectPrevious = true;
		v.canSelectNext = false; // demonstrates disabled, not just enabled/unsupported
		f.video = v;
		cases.push_back({"real_media_ready_fallback_title_tier", f});
	}
	{
		HudFrameData f = baseRealFrame("real-frame-demo");
		VideoPlaybackStatus v;
		v.health = VideoPlaybackHealth::Ready;
		v.mediaId = "raw-mediaid-9f2e71";
		v.titleId = std::nullopt;
		v.fallbackDisplayTitle = std::nullopt; // bottom tier: raw mediaId shown
		v.playbackProgress = 0.05f;
		f.video = v;
		cases.push_back({"real_media_ready_mediaid_tier", f});
	}

	// -- Typography overflow: a deliberately too-long fallback title,
	//    forcing HudTextMetricsCache::truncateToWidth() to actually
	//    truncate+ellipsize through the REAL resolver/binding path (unit
	//    tests already cover the cache in isolation — this proves the
	//    same behavior end-to-end from a real HudFrameData). -----------
	{
		HudFrameData f = baseRealFrame("real-frame-demo");
		f.scene.displayName = "An Extremely Long Scene Display Name That Cannot Possibly Fit Its Region";
		VideoPlaybackStatus v;
		v.health = VideoPlaybackHealth::Ready;
		v.mediaId = "clip-title-overflow";
		v.fallbackDisplayTitle = "A Deliberately Overlong Media Title Used Only To Exercise Deterministic Ellipsis Truncation";
		f.video = v;
		cases.push_back({"real_typography_overflow", f});
	}

	// -- MediaViewportMesh, textured: a real (non-null, allocated)
	//    ofTexture — the mostly-transparent FBO ExperienceRuntime's own
	//    FakeScene produces makes a poor visual proof (see this session's
	//    review, "MediaViewportMesh" section); this checkerboard fully
	//    covers its own source bounds, so the cover-fit crop and the
	//    rounded-corner+bevel silhouette are both clearly visible here. --
	{
		HudFrameData f = baseRealFrame("real-frame-demo");
		f.sceneFrame.texture = &syntheticMediaTexture_;
		f.sceneFrame.nativeSize = glm::ivec2(640, 360);
		cases.push_back({"real_media_viewport_textured", f});
	}

	return cases;
}

void ofApp::update() {
	if (capturing_ || !scenario_) return;
	if (!paused_) elapsedSeconds_ += kFixedDt;
	auto frame = scenario_->sample(elapsedSeconds_);
	renderer_.update(kFixedDt, frame);
}

std::string ofApp::screenshotName() const {
	char buf[256];
	std::snprintf(buf, sizeof(buf), "%s_%s_t%.2f", currentSceneId_.c_str(),
		fakeCasePhaseName(FakeHudScenarioBase::phaseForElapsed(elapsedSeconds_)), elapsedSeconds_);
	return buf;
}

void ofApp::captureOne(const std::string& suffix) {
	ofSaveScreen("captures/" + suffix + ".png");
	ofLogNotice("hud_validation_studio") << "captured " << suffix;
}

void ofApp::runBaselineCapture() {
	capturing_ = true;
	baselineIndex_ = 0;
	realFrameIndex_ = 0;
	boundsOverlayCaptured_ = false;
}

// Architecture-Closure Session (closure plan §8.2) — see this method's
// own header-file comment. Runs Blob->Temporal->Quadrant->Blueprint (the
// plan's own named sequence) for 5 full cycles, measuring per-cycle
// allocation totals via each switchScene() call (which internally calls
// HudWireframeRenderer::setScene() -> HudProfileCompiler::compile() —
// the one documented, deliberate exception to "never allocate in the
// steady-state path", explicitly gated to setup()/scene-profile-change
// only). Acceptance per the plan is boundedness/determinism, not zero —
// this logs the raw numbers for the closure report to interpret, it does
// not itself pass/fail.
void ofApp::runProfileSwitchAllocationMeasurement() {
	const std::vector<std::string> sequence = {
		"blob-region-prototype", "temporal-fields", "quadrant-crosshair", "blueprint_emergence"};
	constexpr int kCycles = 5;

	alloccounter::reset();
	alloccounter::setEnabled(true);

	for (int cycle = 0; cycle < kCycles; ++cycle) {
		long long cycleNewBefore = alloccounter::newCount();
		for (const auto& sceneId : sequence) {
			switchScene(sceneId);
		}
		long long cycleNewAfter = alloccounter::newCount();
		ofLogNotice("hud_validation_studio") << "PROFILE-SWITCH cycle " << (cycle + 1) << "/" << kCycles
			<< ": " << (cycleNewAfter - cycleNewBefore) << " operator-new calls across "
			<< sequence.size() << " switches ("
			<< sequence[0] << "->" << sequence[1] << "->" << sequence[2] << "->" << sequence[3] << ")";
	}

	alloccounter::setEnabled(false);
	long long totalNew = alloccounter::newCount();
	long long totalDelete = alloccounter::deleteCount();
	ofLogNotice("hud_validation_studio") << "PROFILE-SWITCH measurement complete: " << totalNew
		<< " total operator-new / " << totalDelete << " total operator-delete across " << kCycles
		<< " cycles (" << (kCycles * static_cast<int>(sequence.size())) << " total switches). "
		<< "See the five per-cycle lines above for whether per-cycle cost is stable (bounded, no "
		<< "unbounded growth) or drifting across repeated cycles — the closure report interprets this.";
}

void ofApp::draw() {
	ofBackground(8, 10, 9);

	if (capturing_) {
		if (baselineIndex_ < baselineCases_.size()) {
			const auto& c = baselineCases_[baselineIndex_];
			switchScene(c.sceneId);
			elapsedSeconds_ = FakeHudScenarioBase::elapsedSecondsForPhase(c.phase, c.offsetWithinPhase);
			auto frame = scenario_->sample(elapsedSeconds_);
			renderer_.update(kFixedDt, frame);
			renderer_.draw();
			ofSaveScreen("captures/" + c.name + ".png");
			ofLogNotice("hud_validation_studio") << "captured " << (baselineIndex_ + 1) << "/" << baselineCases_.size() << ": " << c.name;
			baselineIndex_++;

			if (baselineIndex_ == baselineCases_.size()) {
				// One extra, deterministically-named capture for the
				// binding-error placeholder case (this task's §14 requires
				// it as a baseline; it is not scene/phase-driven, since it
				// demonstrates a compiler-rejected binding, not a fake-data
				// state — see drawDebugBindingErrorDemo()).
				renderer_.draw();
				drawDebugBindingErrorDemo();
				ofSaveScreen("captures/binding-error-placeholder_demo.png");
				ofLogNotice("hud_validation_studio") << "captured binding-error-placeholder_demo";
			}
			return;
		}

		if (realFrameIndex_ < realFrameCases_.size()) {
			// Engineering Session 2: real-HudFrameData baseline batch —
			// same renderer_, same draw(), fed via
			// HudWireframeRenderer::update(float, const HudFrameData&)
			// instead of the fake-scenario path above. See
			// buildRealFrameCases()'s own comments for what each case
			// proves.
			//
			// Architecture-Closure Session: switched to FullProfile scope
			// for this whole batch (once, on the first real-frame case) —
			// universal.effect_summary (effects.active/effects.health) is
			// deferred out of MinimalSlice (Session 1's "first vertical
			// slice" scope, which every fake-scenario baseline above
			// still correctly uses) — the required effects_* screenshot
			// matrix cases need it visible, and there's no reason to
			// re-hide it for the media/typography/manager-transition real-
			// frame cases either.
			if (realFrameIndex_ == 0) {
				renderer_.setRenderScope(HudWireframeRenderer::RenderScope::FullProfile);
			}
			const auto& c = realFrameCases_[realFrameIndex_];
			renderer_.setActiveEffectsCompatibilityFallbackEnabled(c.compatibilityFallback);
			renderer_.update(kFixedDt, c.frame);
			renderer_.draw();
			ofSaveScreen("captures/" + c.name + ".png");
			ofLogNotice("hud_validation_studio") << "captured " << (realFrameIndex_ + 1) << "/" << realFrameCases_.size()
				<< " (real-frame): " << c.name;
			realFrameIndex_++;
			return;
		}

		if (!boundsOverlayCaptured_) {
			// Architecture-Closure Session addition — screenshot matrix
			// §6.2's "bounds/safe-margin debug overlay" case: the last
			// real-frame case's resolved state (still loaded in renderer_
			// from the loop above), redrawn once with region outlines on.
			boundsOverlayCaptured_ = true;
			renderer_.draw();
			drawRegionOutlines();
			ofSaveScreen("captures/bounds_safe_margin_overlay.png");
			ofLogNotice("hud_validation_studio") << "captured bounds_safe_margin_overlay";
			return;
		}

		capturing_ = false;
		ofLogNotice("hud_validation_studio") << "Baseline capture complete: " << baselineCases_.size()
			<< " fake-scenario screenshots + 1 binding-error demo + " << realFrameCases_.size() << " real-HudFrameData screenshots.";
		if (autoCaptureThenExit_) {
			runProfileSwitchAllocationMeasurement();
			ofExit();
		}
		return;
	}

	renderer_.draw();
	if (showRegionOutlines_) drawRegionOutlines();
	if (debugMode_) drawDebugBindingErrorDemo();
	if (showInspector_) drawInspectorPanel();
}

void ofApp::drawRegionOutlines() const {
	ofPushStyle();
	ofNoFill();
	ofSetColor(255, 200, 80, 180);
	for (const auto& region : renderer_.regionCatalog().all()) {
		ofRectangle bounds = renderer_.pixelBoundsFor(region.regionId);
		ofDrawRectangle(bounds);
		ofSetColor(255, 200, 80, 220);
		ofDrawBitmapString(region.regionId, bounds.x + 2, bounds.y + 10);
		ofSetColor(255, 200, 80, 180);
	}
	ofPopStyle();
}

void ofApp::drawDebugBindingErrorDemo() const {
	hudpresent::HudWidgetInput demo;
	demo.bindingId = "debug.demo_binding_error";
	demo.compileIssueSummaries = {
		"demo: source value type mismatch",
		"(studio-only — production would skip this binding)",
	};
	hudpresent::HudWidgetColors colors;
	ofRectangle bounds(20.0f, 620.0f, 260.0f, 80.0f);
	debugPlaceholderWidget_.draw(bounds, colors, demo);
}

void ofApp::drawInspectorPanel() const {
	ofPushStyle();
	ofEnableAlphaBlending();

	float panelX = 940.0f;
	float panelY = 4.0f;
	float panelW = 336.0f;

	ofSetColor(0, 0, 0, 170);
	ofDrawRectangle(panelX, panelY, panelW, 716.0f);

	ofSetColor(230, 240, 235, 255);
	float y = panelY + 14.0f;
	auto line = [&](const std::string& text) {
		ofDrawBitmapString(text, panelX + 6.0f, y);
		y += 12.0f;
	};

	FakeCasePhase phase = FakeHudScenarioBase::phaseForElapsed(elapsedSeconds_);
	line("scene: " + currentSceneId_);
	line(std::string("phase: ") + fakeCasePhaseName(phase) + (paused_ ? "  [PAUSED]" : ""));
	line("t=" + ofToString(elapsedSeconds_, 2) + "s  pack=" + (renderer_.selectedVocabularyPack().empty() ? "(canonical)" : renderer_.selectedVocabularyPack()));
	line(std::string("scope: ") + (renderer_.renderScope() == HudWireframeRenderer::RenderScope::MinimalSlice ? "MinimalSlice" : "FullProfile")
		+ "  studio=" + (renderer_.studioMode() ? "on" : "off"));

	const auto& stats = renderer_.lastFrameStats();
	line("widgets drawn: " + ofToString(stats.activeWidgetCount) + "  totalDraw=" + ofToString(stats.totalDrawMicros, 0) + "us");
	line("history signals: " + ofToString(stats.historySignalCount) + "  ~" + ofToString(stats.historyBytes / 1024) + "KB");

	const auto& compiled = renderer_.compiledProfile();
	line("--- compiled profile (" + ofToString(compiled.bindings.size()) + " bindings, "
		+ ofToString(compiled.issues.size()) + " issues) ---");

	for (const auto& cb : compiled.bindings) {
		std::string sources;
		for (size_t i = 0; i < cb.binding.sourceCount; ++i) {
			if (i > 0) sources += ",";
			sources += cb.binding.sources[i].role + "=" + cb.binding.sources[i].sourceId;
		}
		std::string status = cb.valid ? "ok" : "INVALID";
		line((cb.valid ? "  " : "  ! ") + cb.binding.bindingId + " [" + widgetTypeName(cb.binding.widgetType) + "/" + status + "]");
		if (!sources.empty()) line("      " + sources);
		line("      policy=" + std::string(missingPolicyName(cb.binding.missingPolicy)));
	}

	if (!compiled.issues.empty()) {
		line("--- issues ---");
		for (const auto& issue : compiled.issues) {
			line("  " + issue.bindingId + ": " + issue.detail);
		}
	}

	ofPopStyle();
}

void ofApp::keyPressed(int key) {
	const auto& ids = scenarioRegistry_.sceneIds();
	if (key >= '1' && key <= '6') {
		size_t idx = static_cast<size_t>(key - '1');
		if (idx < ids.size()) switchScene(ids[idx]);
		return;
	}

	switch (key) {
		case 'p': paused_ = !paused_; break;
		case OF_KEY_LEFT: elapsedSeconds_ = std::max(0.0f, elapsedSeconds_ - kFixedDt); break;
		case OF_KEY_RIGHT: elapsedSeconds_ += kFixedDt; break;
		case '[': {
			int idx = static_cast<int>(FakeHudScenarioBase::phaseForElapsed(elapsedSeconds_));
			idx = (idx - 1 + static_cast<int>(FakeCasePhase::kCount)) % static_cast<int>(FakeCasePhase::kCount);
			jumpToPhase(static_cast<FakeCasePhase>(idx));
			break;
		}
		case ']': {
			int idx = static_cast<int>(FakeHudScenarioBase::phaseForElapsed(elapsedSeconds_));
			idx = (idx + 1) % static_cast<int>(FakeCasePhase::kCount);
			jumpToPhase(static_cast<FakeCasePhase>(idx));
			break;
		}
		case 'v':
			renderer_.selectVocabularyPack(renderer_.selectedVocabularyPack().empty() ? "alt-pack" : "");
			break;
		case 'w': showRegionOutlines_ = !showRegionOutlines_; break;
		case 'f':
			renderer_.setRenderScope(renderer_.renderScope() == HudWireframeRenderer::RenderScope::MinimalSlice
				? HudWireframeRenderer::RenderScope::FullProfile
				: HudWireframeRenderer::RenderScope::MinimalSlice);
			break;
		case 'd':
			debugMode_ = !debugMode_;
			renderer_.setStudioMode(debugMode_);
			break;
		case 'i': showInspector_ = !showInspector_; break;
		case 's': captureOne(screenshotName()); break;
		case 'c': runBaselineCapture(); break;
		default: break;
	}
}
