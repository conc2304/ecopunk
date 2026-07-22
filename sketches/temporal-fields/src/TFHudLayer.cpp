#include "TFHudLayer.h"
#include "TFSettings.h"
#include "Settings.h"
#include <algorithm>
#include <cmath>

namespace {
	// Motion energy above this reads as an eventful spike worth flagging on
	// the status light, rather than just "footage is playing normally."
	constexpr float kMotionAlertThreshold = 0.55f;
	constexpr float kLogPushInterval = 2.2f; // seconds between ambient log lines
}

void TFHudLayer::setup(int canvasW_, int canvasH_) {
	canvasW = canvasW_;
	canvasH = canvasH_;

	// Same house palette used by quadrant-crosshair's HudManager and
	// blueprint_emergence's BEComposition, so this sketch's chrome reads as
	// the same family rather than a fourth, unrelated look.
	theme.colors.primary    = ofColor(124, 232, 230, 220);
	theme.colors.secondary  = ofColor(103, 255, 142, 200);
	theme.colors.accent     = ofColor(244, 255, 106, 220);
	theme.colors.muted      = ofColor(124, 232, 230, 70);
	theme.colors.background = ofColor(0, 20, 16, 40);
	theme.frame.style        = hud::FrameStyle::Corners;
	theme.frame.showTicks    = true;
	theme.additive           = true;

	scanTheme = theme;
	scanTheme.frame.showScanLines = true;

	float W = static_cast<float>(canvasW);
	float H = static_cast<float>(canvasH);

	moire.setup(canvasW, canvasH);

	// Underlay — full-canvas ambient field, faded low so it reads as
	// something glimpsed through the gaps rather than competing with footage.
	hud::ContourOptions cOpts;
	cOpts.contourCount = 7;
	cOpts.samples      = 70;
	cOpts.noiseScale   = 1.3f;
	contours.setBounds(0, 0, W, H);
	contours.setTheme(theme);
	contours.setOptions(cOpts);
	contours.setup();

	hud::HexGridOptions hOpts;
	hOpts.cellSize    = 30.0f;
	hOpts.activation  = 0.10f;
	hOpts.filledCells = false;
	hexGrid.setBounds(0, 0, W, H);
	hexGrid.setTheme(theme);
	hexGrid.setOptions(hOpts);
	hexGrid.setup();

	// PCB-trace/tech-mesh look — edgeStyle left at its Straight default, so
	// this is genuinely the widget "as-is" (Priority 2 of the second
	// investigation pass).
	hud::NodeNetworkOptions nOpts;
	nOpts.nodeCount          = 22;
	nOpts.connectionDistance = 0.16f;
	nOpts.wrap               = true;
	nOpts.showPackets        = true;
	network.setBounds(0, 0, W, H);
	network.setTheme(theme);
	network.setOptions(nOpts);
	network.setup();

	// Root/mycelium look — a second, sparser instance in Organic edge mode
	// (Priority 3), layered under the tech-mesh one above. Fewer nodes and a
	// dimmer theme than `network` so the two don't read as visual clutter
	// stacked on the same full canvas.
	hud::NodeNetworkOptions noOpts;
	noOpts.nodeCount          = 14;
	noOpts.connectionDistance = 0.20f;
	noOpts.wrap               = true;
	noOpts.showPackets        = true;
	noOpts.edgeStyle          = hud::NodeNetworkEdgeStyle::Organic;
	noOpts.organicBulge       = 0.4f;
	networkOrganic.setBounds(0, 0, W, H);
	hud::HudTheme organicTheme = theme;
	organicTheme.colors.muted.a     = static_cast<unsigned char>(organicTheme.colors.muted.a * 0.6f);
	organicTheme.colors.secondary.a = static_cast<unsigned char>(organicTheme.colors.secondary.a * 0.6f);
	networkOrganic.setTheme(organicTheme);
	networkOrganic.setOptions(noOpts);
	networkOrganic.setup();

	// Drifting dust motes — sparsified, curves off, slowed via motion.speed.
	// Known limitation (see prior investigation pass): particles still
	// travel along a fixed, pre-baked 1D sine curve per line, not free 2D
	// drift, so this reads as "sparse pulsing dots wandering along invisible
	// paths" rather than true scattered motes. Shipping it as configured
	// here rather than rewriting the widget's particle system — flag back
	// if seeing it running reads as insufficient.
	hud::FlowFieldOptions ffOpts;
	ffOpts.lineCount        = 4;
	ffOpts.particlesPerLine = 3;
	ffOpts.showCurves       = false;
	ffOpts.density          = 1.0f;
	motes.setBounds(0, 0, W, H);
	motes.setTheme(theme);
	motes.setOptions(ffOpts);
	motes.setup();
	motes.setMotion(hud::MotionSettings{ 0.35f, 1.0f, 1.0f, 1.0f }); // slow drift

	// Overlay — ambient chrome
	hud::ScannerOptions sOpts;
	sOpts.rings         = 3;
	sOpts.ticks         = 40;
	sOpts.showSweep     = true;
	sOpts.showCrosshair = false;
	sOpts.showPulses    = true;
	float scanR = 110.0f;
	scanner.setBounds(W - scanR * 2.0f - 30.0f, 30.0f, scanR * 2.0f, scanR * 2.0f);
	scanner.setTheme(theme);
	scanner.setOptions(sOpts);
	scanner.setup();

	hud::ReticleOptions rOpts;
	rOpts.targetCount   = 4;
	rOpts.showLabels    = true;
	rOpts.randomTargets = true;
	rOpts.preset        = hud::ReticlePreset::Tracking;
	reticles.setBounds(0, 0, W, H);
	// Registration marks (Priority 4) live on this full-canvas widget's
	// frame so they land at the actual canvas corners.
	hud::HudTheme regTheme = theme;
	regTheme.frame.style = hud::FrameStyle::Registration;
	reticles.setTheme(regTheme);
	reticles.setOptions(rOpts);
	reticles.setup();

	// Overlay — real-signal readouts
	hud::DataCardOptions dcOpts;
	dcOpts.title         = "MOTION";
	dcOpts.subtitle      = "NO SIGNAL";
	dcOpts.value         = "optical_flow: --";
	dcOpts.meter         = 0.0f;
	dcOpts.showMeter     = true;
	dcOpts.showSparkline = true;
	motionCard.setBounds(24.0f, H - 150.0f, 220.0f, 120.0f);
	motionCard.setTheme(scanTheme); // this is the one place FrameOptions::showScanLines is on
	motionCard.setOptions(dcOpts);
	motionCard.setup();

	// Specimen ID ticker — increments once per pattern switch (see
	// onPatternSwitch()), not per fragment reassignment or a fixed timer.
	// Deliberately untouched by the Event Layer phase: per-fragment fires
	// many times a second (fragments reassign continuously via ofNoise, not
	// discrete spawn events), and a fixed timer has no compositional
	// meaning; a pattern switch is the closest thing to "a new specimen"
	// this sketch actually has. tickStampCounter (below) is the Event
	// Layer's own, separate, per-fragment-cadence counter.
	hud::DataCardOptions specOpts;
	specOpts.title         = "SPECIMEN";
	specOpts.subtitle      = "BSP";
	specOpts.value         = "#000";
	specOpts.showMeter     = false;
	specOpts.showSparkline = false;
	specimenCard.setBounds(24.0f, 30.0f, 200.0f, 110.0f);
	specimenCard.setTheme(theme);
	specimenCard.setOptions(specOpts);
	specimenCard.setup();

	// Charge-level bar — real signal: TimeOffsetVideoBuffer's history ring
	// buffer fill fraction, 0 at startup climbing to 1 once at capacity.
	hud::GaugeOptions bufOpts;
	bufOpts.style     = hud::GaugeStyle::Segmented;
	bufOpts.value     = 0.0f;
	bufOpts.segments  = 20;
	bufOpts.label     = "BUFFER";
	bufOpts.units     = "%";
	bufOpts.showValue = true;
	bufferGauge.setBounds(24.0f, 150.0f, 130.0f, 130.0f);
	bufferGauge.setTheme(theme);
	bufferGauge.setOptions(bufOpts);

	hud::StatusLightOptions slOpts;
	slOpts.label = "MEDIA";
	slOpts.state = hud::StatusState::Idle;
	mediaStatus.setBounds(24.0f, H - 178.0f, 140.0f, 24.0f);
	mediaStatus.setTheme(theme);
	mediaStatus.setOptions(slOpts);
	mediaStatus.setup();

	hud::LogScrollOptions lOpts;
	lOpts.maxLines    = 30;
	lOpts.scrollSpeed = 9.0f;
	lOpts.showFrame   = true;
	log.setBounds(W - 260.0f, H - 190.0f, 236.0f, 160.0f);
	log.setTheme(theme);
	log.setOptions(lOpts);
	log.setup();

	glitch.setBounds(0, 0, W, H);
	glitch.setTheme(theme);
	glitch.setup();
}

void TFHudLayer::update(float dt, float motionEnergy01, bool hasMedia, const std::string& currentMediaFilename,
	float historyBufferFill01) {
	motionEnergy01 = ofClamp(motionEnergy01, 0.0f, 1.0f);

	if (hasMedia && currentMediaFilename != lastMediaFilename) {
		lastMediaFilename = currentMediaFilename;
		log.pushLine("> loaded " + ofFilePath::getFileName(currentMediaFilename));
		glitch.trigger();
	}

	hud::DataCardOptions dcOpts;
	dcOpts.title         = "MOTION";
	dcOpts.subtitle      = hasMedia ? "LIVE FEED" : "NO SIGNAL";
	dcOpts.value         = "optical_flow: " + ofToString(motionEnergy01, 2);
	dcOpts.meter         = motionEnergy01;
	dcOpts.showMeter     = true;
	dcOpts.showSparkline = true;
	motionCard.setOptions(dcOpts);

	bufferGauge.setValue(ofClamp(historyBufferFill01, 0.0f, 1.0f));

	hud::StatusState state = hud::StatusState::Idle;
	if (hasMedia) state = (motionEnergy01 > kMotionAlertThreshold) ? hud::StatusState::Alert : hud::StatusState::Active;
	mediaStatus.setState(state);

	logPushTimer += dt;
	if (hasMedia && logPushTimer >= kLogPushInterval) {
		logPushTimer = 0.0f;
		log.pushLine("optical_flow " + ofToString(motionEnergy01, 2));
	}

	moire.update(dt);
	contours.update(dt);
	hexGrid.update(dt);
	network.update(dt);
	networkOrganic.update(dt);
	motes.update(dt);
	scanner.update(dt);
	reticles.update(dt);
	motionCard.update(dt);
	specimenCard.update(dt);
	bufferGauge.update(dt);
	mediaStatus.update(dt);
	log.update(dt);
	glitch.update(dt);

	for (auto& t : connectionThreads) t.age += dt;
	connectionThreads.erase(
		std::remove_if(connectionThreads.begin(), connectionThreads.end(),
			[](const ConnectionThread& t) { return t.age >= CONNECTION_THREAD_LIFETIME; }),
		connectionThreads.end());

	for (auto& s : tickStamps) s.age += dt;
	tickStamps.erase(
		std::remove_if(tickStamps.begin(), tickStamps.end(),
			[](const TickStamp& s) { return s.age >= TICK_STAMP_LIFETIME; }),
		tickStamps.end());
}

void TFHudLayer::drawUnderlay() {
	moire.draw();
	contours.draw();
	hexGrid.draw();
	network.draw();
	networkOrganic.draw();
	motes.draw();
}

void TFHudLayer::drawOverlay() {
	scanner.draw();
	reticles.draw();
	motionCard.draw();
	specimenCard.draw();
	bufferGauge.draw();
	mediaStatus.draw();
	log.draw();
	glitch.draw();

	ofPushStyle();
	ofEnableBlendMode(OF_BLENDMODE_ADD);
	drawConnectionThreads();
	drawTickStamps();
	ofDisableBlendMode();
	ofPopStyle();
}

void TFHudLayer::onPatternSwitch(const std::string& patternName) {
	glitch.trigger();

	// Pattern-switch stays available as the alternate/fallback cadence
	// (Section 5) — gated by cadenceOnPatternSwitch, default true.
	if (cadenceOnPatternSwitch) {
		hexGrid.pulseAt(0.5f, 0.5f);
	}

	// Specimen ticker cadence deliberately untouched by the Event Layer
	// phase — see the comment on specimenCard in setup().
	specimenCount++;
	std::string idStr = ofToString(specimenCount);
	while (idStr.size() < 3) idStr = "0" + idStr;

	hud::DataCardOptions specOpts;
	specOpts.title         = "SPECIMEN";
	specOpts.subtitle      = patternName;
	specOpts.value         = "#" + idStr;
	specOpts.showMeter     = false;
	specOpts.showSparkline = false;
	specimenCard.setOptions(specOpts);

	log.pushLine("> specimen #" + idStr + " (" + patternName + ")");
}

void TFHudLayer::onFragmentReassigned(float nx, float ny, const std::vector<ofVec2f>& activeFragmentCentersNorm) {
	if (!cadenceOnFragmentReassign) return;

	ofVec2f originPx(nx * canvasW, ny * canvasH);

	// Same ripple HexGridWidget already had (Priority 1 of the prior
	// phase), now also available on the finer-grained cadence — real per-
	// fragment plumbing didn't exist when that was first wired in, so it
	// only fired from onPatternSwitch(); this is that gap closing.
	hexGrid.pulseAt(nx, ny);

	spawnConnectionThreads(originPx, activeFragmentCentersNorm);

	tickStampCounter++;
	TickStamp stamp;
	stamp.pos = originPx;
	stamp.age = 0.0f;
	stamp.id  = tickStampCounter;
	tickStamps.push_back(stamp);
}

void TFHudLayer::spawnConnectionThreads(const ofVec2f& originPx, const std::vector<ofVec2f>& activeFragmentCentersNorm) {
	struct Candidate {
		ofVec2f px;
		float distSq;
	};
	std::vector<Candidate> candidates;
	candidates.reserve(activeFragmentCentersNorm.size());

	for (const auto& c : activeFragmentCentersNorm) {
		ofVec2f px(c.x * canvasW, c.y * canvasH);
		float dx = px.x - originPx.x;
		float dy = px.y - originPx.y;
		float distSq = dx * dx + dy * dy;
		if (distSq < 4.0f) continue; // this is the origin fragment itself (< 2px away)
		candidates.push_back({ px, distSq });
	}

	std::sort(candidates.begin(), candidates.end(),
		[](const Candidate& a, const Candidate& b) { return a.distSq < b.distSq; });

	int n = std::min(static_cast<int>(candidates.size()), 3);
	for (int i = 0; i < n; i++) {
		ConnectionThread t;
		t.origin = originPx;
		t.target = candidates[i].px;
		t.age    = 0.0f;
		connectionThreads.push_back(t);
	}
}

void TFHudLayer::drawConnectionThreads() const {
	for (const auto& t : connectionThreads) {
		float u = ofClamp(1.0f - t.age / CONNECTION_THREAD_LIFETIME, 0.0f, 1.0f);
		if (u <= 0.0f) continue;

		ofVec2f dir = t.target - t.origin;
		float len = dir.length();
		if (len < 0.001f) continue;
		dir /= len;

		ofSetColor(hud::scaledAlpha(theme.colors.secondary, u * 0.85f));
		ofSetLineWidth(1.0f);

		// Dashed segments — same stepped-line technique
		// AnnotationRenderer::drawMeasurementLines() uses for its
		// measurement lines, reimplemented here rather than reused: that
		// class's methods take Fragment*/GridSystem* (blueprint_emergence's
		// own types), which don't exist in Temporal Fields.
		for (float d = 0.0f; d < len; d += 8.0f) {
			float segEnd = std::min(d + 4.0f, len);
			ofVec2f a = t.origin + dir * d;
			ofVec2f b = t.origin + dir * segEnd;
			ofDrawLine(a.x, a.y, b.x, b.y);
		}
	}
}

void TFHudLayer::drawTickStamps() const {
	constexpr float crossLen = 10.0f;
	constexpr float tickLen  = 6.0f;
	constexpr float boxHalf  = 16.0f;

	for (const auto& s : tickStamps) {
		float u = ofClamp(1.0f - s.age / TICK_STAMP_LIFETIME, 0.0f, 1.0f);
		if (u <= 0.0f) continue;

		ofSetColor(hud::scaledAlpha(theme.colors.accent, u));
		ofSetLineWidth(1.0f);

		ofDrawLine(s.pos.x - crossLen, s.pos.y, s.pos.x + crossLen, s.pos.y);
		ofDrawLine(s.pos.x, s.pos.y - crossLen, s.pos.x, s.pos.y + crossLen);

		auto corner = [&](float sx, float sy) {
			ofDrawLine(s.pos.x + sx * boxHalf, s.pos.y + sy * boxHalf,
				s.pos.x + sx * (boxHalf - tickLen), s.pos.y + sy * boxHalf);
			ofDrawLine(s.pos.x + sx * boxHalf, s.pos.y + sy * boxHalf,
				s.pos.x + sx * boxHalf, s.pos.y + sy * (boxHalf - tickLen));
		};
		corner(1.0f, 1.0f);
		corner(-1.0f, 1.0f);
		corner(1.0f, -1.0f);
		corner(-1.0f, -1.0f);

		// TEXT_CODE-equivalent opacity (matches this codebase's existing
		// code-text/annotation aesthetic), scaled by this stamp's own fade.
		std::string idStr = ofToString(s.id);
		while (idStr.size() < 4) idStr = "0" + idStr;
		ofColor labelColor = TEXT_CODE;
		labelColor.a = static_cast<unsigned char>(labelColor.a * u);
		ofSetColor(labelColor);
		ofDrawBitmapString("[LOGGED " + idStr + "]", s.pos.x + boxHalf + 6.0f, s.pos.y - boxHalf);
	}
}
