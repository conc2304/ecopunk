#include "FragmentSceneCore.h"

#include "FTCanvas.h"

namespace fragmenttrail {

namespace {
// TriggerBus's HIGH_THRESH/LOW_THRESH are private (this class was copied
// verbatim from quadrant-crosshair per the implementation plan's reuse
// decision — not modified to expose them). Duplicated here rather than
// changing that class's encapsulation; keep in sync with TriggerBus.h if
// those values ever change.
constexpr float kSpeedHighThresh = 4.0f;
constexpr float kSpeedLowThresh = 0.8f;

// Preset 2 ("HUNT") — highest freqA/amplitude combination of the 5
// presets, i.e. the fastest sweep across the canvas. Default preset 0
// ("DRIFT") was too slow to cross the 70px spawn-distance threshold at
// a noticeable rate (user feedback: no fragments visibly appearing).
constexpr int kStartupCrosshairPreset = 2;
} // namespace

hudoverlay::HudOverlayDialState FragmentSceneCore::initialOverlayDials() {
	hudoverlay::HudOverlayDialState dials;
	dials.intensity = 0.18f;     // low -- ~one organism-tier event per 10-20s against a ~3.2s fragment lifespan
	dials.discipline = 0.85f;    // high (see FTOverlayDirector: not load-bearing here yet, kept high for if it ever is)
	dials.eventCoupling = 0.15f; // low -- reinforces the one-organism-at-a-time restraint
	return dials;
}

void FragmentSceneCore::setup(TimeOffsetVideoBuffer* video, ShaderLibrary* shaders, const Config& config) {
	video_ = video;
	shaders_ = shaders;
	config_ = config;
	applyCanvasSize();

	params_.setup();
	dials_ = initialOverlayDials();

	// Full-canvas backdrop beneath the fragment pool — dimmed raw video split
	// across two sections (one alternates with a background image, when the
	// backgrounds folder has any — see FTImageCycler::hasImages()), plus a
	// fixed ambient hex-grid HUD widget. Same "underlay" role as
	// temporal-fields' TFBackgroundLayer/TFHudLayer::drawUnderlay().
	backgroundLayer_.setup(video_, config_.backgroundsFolder, config_.canvasW, config_.canvasH,
		config_.bgVertShaderPath, config_.bgShaderPath);

	resetComposition();
	didSetup_ = true;
}

void FragmentSceneCore::resetComposition() {
	crosshair_ = CrosshairSystem{};
	crosshair_.setup();
	crosshair_.setPreset(kStartupCrosshairPreset);
	crosshair2_ = CrosshairSystem{};
	crosshair2_.setup();
	crosshair2_.setPreset((kStartupCrosshairPreset + 2) % 5); // a different pattern, so the trails don't mirror
	triggerBus_ = TriggerBus{};
	triggerBus_.setup();
	lfoBank_ = LFOBank{};
	lfoBank_.setup();
	modeController_ = FTModeController{};
	modeController_.setup();

	triggerBus_.addListener([this](const TriggerEvent& e) {
		modeController_.onTriggerEvent(e);
	});

	pool_ = FTFragmentPool{};
	pool_.setup(params_.getMaxFragments(), params_.getSpawnMinDistance(), params_.getSpawnMinInterval(),
		params_.getIdlePulseEnabled(), params_.getIdlePulseInterval(),
		params_.getSustainSeconds(), params_.getDecaySeconds(), params_.getMinSize(), params_.getMaxSize(),
		config_.numPlayheads);

	hudOverlay_.setup(static_cast<float>(config_.canvasW), static_cast<float>(config_.canvasH));
	hudOverlay_.setSchedulerEnabled(false); // driven by real fragment events (FTOverlayDirector), not ambient random timing
	hudOverlay_.setFaultCascadeHalftoneEnabled(false); // tear+flash only for the eviction gesture, per the design brief
	hudOverlay_.setSafeArea(config_.safeArea);
	hudOverlay_.setRadarSweepMatchesScanner(true);
	hudOverlay_.setRadarContacts({}); // range rings on; contacts filled from the live pool each update
	overlayDirector_ = FTOverlayDirector{};
	overlayDirector_.setup(&pool_, &hudOverlay_);

	lastPreset_ = crosshair_.getCurrentPreset();
	presetElapsed_ = 0.0f;
}

void FragmentSceneCore::applyCanvasSize() {
	if (config_.trackWindow) setCanvasSize(0.0f, 0.0f); // 0 = follow the live window
	else setCanvasSize(static_cast<float>(config_.canvasW), static_cast<float>(config_.canvasH));
}

void FragmentSceneCore::activate() {
	if (!didSetup_) return;
	applyCanvasSize();
	resetComposition();
	active_ = true;
}

float FragmentSceneCore::crosshairEnergy01() const {
	const float speed = crosshair_.getState().speed;
	return ofClamp(ofMap(speed, kSpeedLowThresh, kSpeedHighThresh, 0.f, 1.f, true), 0.f, 1.f);
}

void FragmentSceneCore::update(float dt) {
	if (!active_ || dt <= 0.f) return;

	lfoBank_.update(dt);
	crosshair_.applyLiveSettings(params_.getCrosshairSpeed(), params_.getCrosshairSmoothness(),
		params_.getCrosshairSpeedVariance(), params_.getCrosshairVarianceRate(),
		params_.getCrosshairPattern(), params_.getCrosshairAutoCycle(),
		params_.getCrosshairAutoCycleInterval());
	crosshair_.update(dt, lfoBank_);
	crosshair2_.applyLiveSettings(params_.getCrosshairSpeed(), params_.getCrosshairSmoothness(),
		params_.getCrosshairSpeedVariance(), params_.getCrosshairVarianceRate(),
		(params_.getCrosshairPattern() + 2) % 5, params_.getCrosshairAutoCycle(),
		params_.getCrosshairAutoCycleInterval());
	crosshair2_.update(dt, lfoBank_);
	triggerBus_.update(crosshair_.getState(), dt);
	backgroundLayer_.update(dt);

	if (crosshair_.getCurrentPreset() != lastPreset_) {
		lastPreset_ = crosshair_.getCurrentPreset();
		presetElapsed_ = 0.0f;
	} else {
		presetElapsed_ += dt;
	}

	modeController_.setDwellDuration(params_.getModeDwellDuration());
	modeController_.update(dt);

	pool_.applyLiveSettings(params_.getMaxFragments(), params_.getSustainSeconds(), params_.getDecaySeconds(),
		params_.getMinSize(), params_.getMaxSize(),
		params_.getIdlePulseEnabled(), params_.getIdlePulseInterval());

	// speedResponse (Section 3): normalize crosshair speed into a 0-1
	// "energy" value via TriggerBus's own high/low thresholds, then scale
	// spawn cadence — faster movement lowers the effective distance/
	// interval thresholds (denser trail while "searching"), slower raises
	// them (sparser, more deliberate emergence).
	float responseAmount = params_.getSpeedResponse();
	float energyScale = ofClamp(1.0f - crosshairEnergy01() * responseAmount, 0.2f, 1.0f);
	float effectiveMinDistance = params_.getSpawnMinDistance() * energyScale;
	float effectiveMinInterval = params_.getSpawnMinInterval() * energyScale;

	pool_.update(dt, crosshair_.getState(), crosshair2_.getState(), *video_, modeController_.getCurrentMode(),
		modeController_.getModeCEffectName(), modeController_.trailBLook(), effectiveMinDistance, effectiveMinInterval);
	// ^ spawn/evict callbacks fire synchronously above, into overlayDirector's gates.

	overlayDirector_.update(dt, dials_);
	updateRadarContacts();
	hudOverlay_.update(dt, dials_); // last, so anything triggered above gets its first tick correctly
}

// Radar = the hunter's view: every live fragment as a contact, placed by
// its real offset from the crosshair (angle + distance), brightness = its
// live opacity; the nearest is the primary (bracketed) contact; heading =
// the crosshair's real velocity. No fragments -> no blips (ambient sweep).
void FragmentSceneCore::updateRadarContacts() {
	const CrosshairState cs = crosshair_.getState();
	const glm::vec2 hunter(cs.cx, cs.cy);
	const float maxRange = std::max(1.0f, std::min(canvasWidth(), canvasHeight()) * 0.6f);
	std::vector<hud::RadarContact> contacts;
	int nearest = -1;
	float nearestDist = 0.0f;
	for (const FTFragment& f : pool_.getFragments()) {
		const float alpha = f.getAlpha();
		if (alpha <= 0.02f) continue;
		const glm::vec2 d = f.getPosition() - hunter;
		const float dist = glm::length(d);
		hud::RadarContact c;
		c.angleDeg = ofRadToDeg(std::atan2(d.y, d.x));
		if (c.angleDeg < 0.0f) c.angleDeg += 360.0f;
		c.range01 = ofClamp(dist / maxRange, 0.0f, 1.0f);
		c.strength = alpha;
		if (nearest < 0 || dist < nearestDist) { nearest = static_cast<int>(contacts.size()); nearestDist = dist; }
		contacts.push_back(c);
	}
	if (nearest >= 0) contacts[static_cast<size_t>(nearest)].primary = true;
	hudOverlay_.setRadarContacts(std::move(contacts));

	const float speed = std::sqrt(cs.vx * cs.vx + cs.vy * cs.vy);
	float heading = ofRadToDeg(std::atan2(cs.vy, cs.vx));
	if (heading < 0.0f) heading += 360.0f;
	hudOverlay_.setRadarHeading(heading, speed > 0.05f ? ofLerp(0.35f, 1.0f, crosshairEnergy01()) : 0.0f);
}

void FragmentSceneCore::draw() {
	ofPushStyle();
	// Scoped to the bound target (the standalone window, or the runtime's
	// scene FBO) — ofClear does not change OF's global background state.
	ofClear(8, 10, 9, 255);
	if (modeController_.getCurrentMode() == FTContentMode::BACKDROP_EFFECT) {
		backgroundLayer_.setBackdropEffect(shaders_, &modeController_.backdropLook());
	} else {
		backgroundLayer_.setBackdropEffect(nullptr, nullptr);
	}
	backgroundLayer_.draw();
	hudOverlay_.drawAmbient(); // BELOW: scanline, radar station, breathing ticks, telemetry -- "the room"
	pool_.draw(*video_, *shaders_);
	hudOverlay_.drawOrganisms(); // BETWEEN/ABOVE: tick-burst accents + the 4 organisms, anchored to live fragment bounds
	crosshair_.draw();
	crosshair2_.draw();
	ofPopStyle();
}

} // namespace fragmenttrail
