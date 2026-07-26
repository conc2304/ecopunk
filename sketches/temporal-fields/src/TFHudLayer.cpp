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

	// Minimum spacing between accepted connection-thread spawns. Without
	// this, a mass fragment reassignment (every fragment exiting at once on
	// a scene/pattern change) fires one spawn per fragment within the same
	// frame or two, flooding the canvas — this collapses a burst down to a
	// single visual pulse. Set well above CONNECTION_THREAD_LIFETIME
	// (0.75s) so consecutive spawns never overlap — at most one spawn's
	// worth of lines (≤3) is ever on screen at once, regardless of how
	// fast the underlying pattern is actually reassigning fragments.
	constexpr float kConnectionThreadCooldown = 2.0f;

	// How often to print the connection-thread churn summary — see
	// connectionThreadDebugLogTimer.
	constexpr float kConnectionThreadDebugLogInterval = 2.0f;
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

	// Underlay — full-canvas ambient field, faded low so it reads as
	// something glimpsed through the gaps rather than competing with footage.
	hud::ContourOptions cOpts;
	cOpts.contourCount = 7;
	cOpts.samples      = 70;
	cOpts.noiseScale   = 1.3f;
	contours.setTheme(theme);
	contours.setOptions(cOpts);
	contours.setup();

	hud::HexGridOptions hOpts;
	hOpts.cellSize    = 30.0f;
	hOpts.activation  = 0.10f;
	hOpts.filledCells = false;
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
	scanner.setTheme(theme);
	scanner.setOptions(sOpts);
	scanner.setup();

	reticleOpts_.targetCount   = 4;
	reticleOpts_.showLabels    = true;
	// false: Locate waypoints come from setLocateTargets() (live fragment
	// centers, fed every update()) rather than uniformly random points.
	reticleOpts_.randomTargets = false;
	reticleOpts_.preset        = hud::ReticlePreset::Tracking;
	reticleOpts_.behavior      = hud::ReticleBehavior::Locate;
	// labelOverride starts empty (falls back to the widget's built-in
	// nature-word table) until the first live fragment centers arrive and
	// reticleLabelRefreshTimer fires in update() — see there for why real
	// per-target coordinates replace that table instead of removing labels
	// outright.
	// Registration marks (Priority 4) live on this full-canvas widget's
	// frame so they land at the actual canvas corners.
	hud::HudTheme regTheme = theme;
	regTheme.frame.style = hud::FrameStyle::Registration;
	reticles.setTheme(regTheme);
	reticles.setOptions(reticleOpts_);
	reticles.setup();
	reticles.setMotion(hud::MotionSettings{ 0.5f, 1.0f, 1.0f, 1.0f }); // slower travel between waypoints

	// Overlay — real-signal readouts
	hud::DataCardOptions dcOpts;
	dcOpts.title         = "MOTION";
	dcOpts.subtitle      = "NO SIGNAL";
	dcOpts.value         = "optical_flow: --";
	dcOpts.meter         = 0.0f;
	dcOpts.showMeter     = true;
	dcOpts.showSparkline = true;
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
	specimenCard.setTheme(theme);
	specimenCard.setOptions(specOpts);
	specimenCard.setup();

	// Charge-level bar — real signal: average playhead time-offset (0 = all
	// live, 1 = as far back as maxHistorySeconds allows). Continuously
	// fluctuates as fragments reassign, unlike the history-buffer-fill
	// metric this replaced (see the update() doc comment).
	hud::GaugeOptions bufOpts;
	bufOpts.style     = hud::GaugeStyle::Segmented;
	bufOpts.value     = 0.0f;
	bufOpts.segments  = 20;
	bufOpts.label     = "DEPTH";
	bufOpts.units     = "%";
	bufOpts.showValue = true;
	bufferGauge.setTheme(theme);
	bufferGauge.setOptions(bufOpts);

	hud::StatusLightOptions slOpts;
	slOpts.label = "MEDIA";
	slOpts.state = hud::StatusState::Idle;
	mediaStatus.setTheme(theme);
	mediaStatus.setOptions(slOpts);
	mediaStatus.setup();

	hud::LogScrollOptions lOpts;
	lOpts.maxLines    = 30;
	lOpts.scrollSpeed = 9.0f;
	lOpts.showFrame   = true;
	log.setTheme(theme);
	log.setOptions(lOpts);
	log.setup();

	glitch.setTheme(theme);
	glitch.setup();

	resize(canvasW_, canvasH_);

	pickNewOverlayRotation();
	pickNewUnderlayRotation();

	// HUD Visibility phase — three subgroups (Underlay pool / Overlay pool /
	// Always-on effects) mirroring the two rotation enums plus the
	// outside-both-enums effects, each with a master toggle ANDed against
	// its own per-widget toggles in isUnderlayWidgetActive()/
	// isOverlayWidgetActive()/drawOverlay(). Order here matches each
	// widget's declared order above (and its rotation-pool enum order) so
	// the on-screen toggle order matches the code's mental model of the
	// pool.
	underlayGroupEnabled.set("Underlay enabled", true);
	underlayEnabled[UW_Moire].set("Moire", true);
	underlayEnabled[UW_Contours].set("Contours", true);
	underlayEnabled[UW_HexGrid].set("Hex Grid", true);
	underlayEnabled[UW_Network].set("Network (Tech)", true);
	underlayEnabled[UW_NetworkOrganic].set("Network (Organic)", true);
	underlayEnabled[UW_Motes].set("Motes", true);
	underlayVisibilityGroup.setName("Underlay");
	underlayVisibilityGroup.add(underlayGroupEnabled);
	for (auto& p : underlayEnabled) underlayVisibilityGroup.add(p);

	overlayGroupEnabled.set("Overlay enabled", true);
	overlayEnabled[OW_Scanner].set("Scanner", true);
	overlayEnabled[OW_Reticles].set("Reticles", true);
	overlayEnabled[OW_MotionCard].set("Motion Card", true);
	overlayEnabled[OW_SpecimenCard].set("Specimen Card", true);
	overlayEnabled[OW_BufferGauge].set("Buffer Gauge", true);
	overlayEnabled[OW_MediaStatus].set("Media Status", true);
	overlayEnabled[OW_Log].set("Log", true);
	overlayVisibilityGroup.setName("Overlay");
	overlayVisibilityGroup.add(overlayGroupEnabled);
	for (auto& p : overlayEnabled) overlayVisibilityGroup.add(p);

	alwaysOnGroupEnabled.set("Always-on enabled", true);
	glitchEnabled.set("Glitch Tears", true);
	connectionThreadsEnabled.set("Connection Threads", true);
	tickStampsEnabled.set("Tick Stamps", true);
	alwaysOnVisibilityGroup.setName("Always-on");
	alwaysOnVisibilityGroup.add(alwaysOnGroupEnabled);
	alwaysOnVisibilityGroup.add(glitchEnabled);
	alwaysOnVisibilityGroup.add(connectionThreadsEnabled);
	alwaysOnVisibilityGroup.add(tickStampsEnabled);

	visibilityGroup.setName("HUD Visibility");
	visibilityGroup.add(underlayVisibilityGroup);
	visibilityGroup.add(overlayVisibilityGroup);
	visibilityGroup.add(alwaysOnVisibilityGroup);
}

void TFHudLayer::resize(int canvasW_, int canvasH_) {
	canvasW = canvasW_;
	canvasH = canvasH_;

	float W = static_cast<float>(canvasW);
	float H = static_cast<float>(canvasH);

	moire.resizeCanvas(canvasW, canvasH);

	contours.setBounds(0, 0, W, H);
	hexGrid.setBounds(0, 0, W, H);
	network.setBounds(0, 0, W, H);
	networkOrganic.setBounds(0, 0, W, H);
	motes.setBounds(0, 0, W, H);
	reticles.setBounds(0, 0, W, H);
	glitch.setBounds(0, 0, W, H);

	// scanner/motionCard/specimenCard/bufferGauge/mediaStatus/log all get
	// their bounds from here — a resize is as good a moment as any to
	// reroll them fresh against the new canvas size.
	respawnLayout();
}

void TFHudLayer::layoutStack(int corner, const std::vector<ofVec2f>& sizes, std::vector<ofVec2f>& outPositions) const {
	constexpr float margin = 24.0f;
	constexpr float gap = 10.0f;
	float W = static_cast<float>(canvasW);
	float H = static_cast<float>(canvasH);
	bool left = (corner == 0 || corner == 2);
	bool top  = (corner == 0 || corner == 1);

	float x = left ? margin : W - margin;
	float y = top ? margin : H - margin;

	outPositions.clear();
	outPositions.reserve(sizes.size());
	for (const auto& sz : sizes) {
		float px = left ? x : x - sz.x;
		float py = top ? y : y - sz.y;
		outPositions.push_back({ px, py });
		if (top) y += sz.y + gap; else y -= sz.y + gap;
	}
}

void TFHudLayer::respawnLayout() {
	// Fisher-Yates shuffle of the 4 canvas corners across the 4 widget
	// clusters below — guarantees no two clusters land on the same corner.
	int corners[4] = { 0, 1, 2, 3 };
	for (int i = 3; i > 0; i--) {
		int j = static_cast<int>(ofRandom(i + 1));
		std::swap(corners[i], corners[j]);
	}
	int cSpecimen = corners[0];
	int cMotion   = corners[1];
	int cLog      = corners[2];
	int cScanner  = corners[3];

	// Specimen cluster: specimenCard (nearest the corner) + bufferGauge.
	// Both widgets scale responsively via su()-relative geometry/typography
	// (shared/src/hud/README.md's own stated design goal), so resizing them
	// within a reasonable range on every respawn is safe.
	{
		ofVec2f specSz(ofRandom(170.0f, 230.0f), ofRandom(90.0f, 130.0f));
		float bufSide = ofRandom(100.0f, 150.0f);
		ofVec2f bufSz(bufSide, bufSide);
		std::vector<ofVec2f> pos;
		layoutStack(cSpecimen, { specSz, bufSz }, pos);
		specimenCard.setBounds(pos[0].x, pos[0].y, specSz.x, specSz.y);
		bufferGauge.setBounds(pos[1].x, pos[1].y, bufSz.x, bufSz.y);
	}

	// Motion cluster: motionCard (nearest the corner) + mediaStatus.
	// mediaStatus's height stays fixed at 24 — it's a single fixed-size dot
	// + one line of text, not content that reads better bigger or smaller.
	{
		ofVec2f motionSz(ofRandom(190.0f, 260.0f), ofRandom(100.0f, 140.0f));
		ofVec2f statusSz(ofRandom(130.0f, 180.0f), 24.0f);
		std::vector<ofVec2f> pos;
		layoutStack(cMotion, { motionSz, statusSz }, pos);
		motionCard.setBounds(pos[0].x, pos[0].y, motionSz.x, motionSz.y);
		mediaStatus.setBounds(pos[1].x, pos[1].y, statusSz.x, statusSz.y);
	}

	// Log — standalone.
	{
		ofVec2f logSz(ofRandom(200.0f, 280.0f), ofRandom(130.0f, 190.0f));
		std::vector<ofVec2f> pos;
		layoutStack(cLog, { logSz }, pos);
		log.setBounds(pos[0].x, pos[0].y, logSz.x, logSz.y);
	}

	// Scanner — standalone, kept square.
	{
		float scanSide = ofRandom(160.0f, 260.0f);
		std::vector<ofVec2f> pos;
		layoutStack(cScanner, { ofVec2f(scanSide, scanSide) }, pos);
		scanner.setBounds(pos[0].x, pos[0].y, scanSide, scanSide);
	}
}

void TFHudLayer::update(float dt, float motionEnergy01, bool hasMedia, const std::string& currentMediaFilename,
	float avgPlayheadDepth01, const std::vector<ofVec2f>& activeFragmentCentersNorm, float patternDrift01) {
	motionEnergy01 = ofClamp(motionEnergy01, 0.0f, 1.0f);
	patternDrift01 = ofClamp(patternDrift01, 0.0f, 1.0f);
	avgPlayheadDepth01 = ofClamp(avgPlayheadDepth01, 0.0f, 1.0f);

	reticles.setLocateTargets(activeFragmentCentersNorm);

	// Real per-target reticle labels — coordinates sampled from the same
	// live fragment centers driving Locate's waypoints, replacing the
	// widget's built-in decorative nature-word table. Throttled (not every
	// frame): changing labelOverride triggers a full ReticleWidget rebuild,
	// which would otherwise restart every target's spawn/tracking animation
	// on every single frame.
	reticleLabelRefreshTimer += dt;
	if (reticleLabelRefreshTimer >= 8.0f && !activeFragmentCentersNorm.empty()) {
		reticleLabelRefreshTimer = 0.0f;
		std::vector<std::string> labels;
		int n = std::min(static_cast<int>(activeFragmentCentersNorm.size()), 6);
		for (int i = 0; i < n; i++) {
			const ofVec2f& c = activeFragmentCentersNorm[i];
			labels.push_back(ofToString(c.x, 2) + " " + ofToString(c.y, 2));
		}
		reticleOpts_.labelOverride = labels;
		reticles.setOptions(reticleOpts_);
	}

	// Contour underlay's noise scale tracks the active pattern's own live
	// irregularity/variation dial (see the update() doc comment) instead of
	// a fixed constant — same rebuild-free setOptions() as hexGrid below.
	hud::ContourOptions cOpts;
	cOpts.contourCount = 7;
	cOpts.samples      = 70;
	cOpts.noiseScale   = ofLerp(0.6f, 2.2f, patternDrift01);
	contours.setOptions(cOpts);

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

	bufferGauge.setValue(avgPlayheadDepth01);

	hud::StatusState state = hud::StatusState::Idle;
	if (hasMedia) state = (motionEnergy01 > kMotionAlertThreshold) ? hud::StatusState::Alert : hud::StatusState::Active;
	mediaStatus.setState(state);

	// Ambient hex noise floor breathes with the same optical-flow signal
	// driving the MOTION card, on top of its existing pattern-switch/
	// fragment-reassign pulses. setOptions() is a plain struct assign here
	// (no rebuild), so it's safe to call every frame.
	hud::HexGridOptions hOpts;
	hOpts.cellSize    = 30.0f;
	hOpts.activation  = ofLerp(0.06f, 0.35f, motionEnergy01);
	hOpts.filledCells = false;
	hexGrid.setOptions(hOpts);

	logPushTimer += dt;
	if (hasMedia && logPushTimer >= kLogPushInterval) {
		logPushTimer = 0.0f;
		log.pushLine("optical_flow " + ofToString(motionEnergy01, 2));
		log.pushLine("depth " + ofToString(avgPlayheadDepth01 * 100.0f, 0) + "%");
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

	overlayRotationTimer += dt;
	if (overlayRotationTimer >= overlayRotationInterval) {
		overlayRotationTimer = 0.0f;
		pickNewOverlayRotation();
	}

	underlayRotationTimer += dt;
	if (underlayRotationTimer >= underlayRotationInterval) {
		underlayRotationTimer = 0.0f;
		pickNewUnderlayRotation();
	}

	connectionThreadCooldownTimer = std::max(0.0f, connectionThreadCooldownTimer - dt);

	connectionThreadDebugLogTimer += dt;
	if (connectionThreadDebugLogTimer >= kConnectionThreadDebugLogInterval) {
		ofLogNotice("TFHudLayer") << "connection threads: " << reassignEventsSinceLog << " reassign events/"
			<< kConnectionThreadDebugLogInterval << "s (" << acceptedSpawnsSinceLog << " spawned, "
			<< rejectedSpawnsSinceLog << " throttled), " << connectionThreads.size() << " alive now";
		if (reassignEventsSinceLog > 0) {
			log.pushLine("threads " + ofToString(acceptedSpawnsSinceLog) + " spawned/"
				+ ofToString(rejectedSpawnsSinceLog) + " throttled");
		}
		connectionThreadDebugLogTimer = 0.0f;
		reassignEventsSinceLog = 0;
		acceptedSpawnsSinceLog = 0;
		rejectedSpawnsSinceLog = 0;
	}
}

void TFHudLayer::pickNewOverlayRotation() {
	overlayRotationInterval = ofRandom(20.0f, 35.0f);

	for (int slot = 0; slot < kMaxActiveOverlayWidgets; slot++) {
		activeOverlayWidgets[slot] = -1; // -1 = no widget in this slot
	}

	// 15% chance of a fully quiet overlay pass — the "no widgets" ask.
	if (ofRandom(1.0f) < 0.15f) {
		return;
	}

	// Sample without replacement from the small fixed pool — fine to do
	// naively since OW_Count is tiny (7).
	int pool[OW_Count];
	for (int i = 0; i < OW_Count; i++) pool[i] = i;
	int poolSize = OW_Count;

	for (int slot = 0; slot < kMaxActiveOverlayWidgets && poolSize > 0; slot++) {
		int pick = static_cast<int>(ofRandom(poolSize));
		activeOverlayWidgets[slot] = pool[pick];
		pool[pick] = pool[--poolSize];
	}

	// Fresh position (and size) every time the active set reshuffles, so a
	// widget re-entering rotation actually lands somewhere new rather than
	// snapping back to wherever it happened to sit last time it was visible.
	respawnLayout();
}

bool TFHudLayer::isOverlayWidgetActive(int id) const {
	if (!overlayGroupEnabled.get() || !overlayEnabled[id].get()) return false;
	for (int i = 0; i < kMaxActiveOverlayWidgets; i++) {
		if (activeOverlayWidgets[i] == id) return true;
	}
	return false;
}

bool TFHudLayer::isUnderlayWidgetActive(int id) const {
	if (!underlayGroupEnabled.get() || !underlayEnabled[id].get()) return false;
	return id == activeUnderlayWidget;
}

void TFHudLayer::pickNewUnderlayRotation() {
	underlayRotationInterval = ofRandom(20.0f, 35.0f);

	// 20% chance of no underlay this pass.
	if (ofRandom(1.0f) < 0.20f) {
		activeUnderlayWidget = -1;
		return;
	}

	activeUnderlayWidget = static_cast<int>(ofRandom(UW_Count));
}

void TFHudLayer::drawUnderlay() {
	if (isUnderlayWidgetActive(UW_Moire)) moire.draw();
	if (isUnderlayWidgetActive(UW_Contours)) contours.draw();
	if (isUnderlayWidgetActive(UW_HexGrid)) hexGrid.draw();
	if (isUnderlayWidgetActive(UW_Network)) network.draw();
	if (isUnderlayWidgetActive(UW_NetworkOrganic)) networkOrganic.draw();
	if (isUnderlayWidgetActive(UW_Motes)) motes.draw();
}

void TFHudLayer::drawOverlay() {
	if (isOverlayWidgetActive(OW_Scanner)) scanner.draw();
	if (isOverlayWidgetActive(OW_Reticles)) reticles.draw();
	if (isOverlayWidgetActive(OW_MotionCard)) motionCard.draw();
	if (isOverlayWidgetActive(OW_SpecimenCard)) specimenCard.draw();
	if (isOverlayWidgetActive(OW_BufferGauge)) bufferGauge.draw();
	if (isOverlayWidgetActive(OW_MediaStatus)) mediaStatus.draw();
	if (isOverlayWidgetActive(OW_Log)) log.draw();
	if (alwaysOnGroupEnabled.get() && glitchEnabled.get()) glitch.draw();

	ofPushStyle();
	ofEnableBlendMode(OF_BLENDMODE_ADD);
	if (alwaysOnGroupEnabled.get() && connectionThreadsEnabled.get()) drawConnectionThreads();
	if (alwaysOnGroupEnabled.get() && tickStampsEnabled.get()) drawTickStamps();
	ofDisableBlendMode();
	ofPopStyle();
}

void TFHudLayer::onPatternSwitch(const std::string& patternName, int cycleSeed) {
	glitch.trigger();

	// Pattern-switch stays available as the alternate/fallback cadence
	// (Section 5) — gated by cadenceOnPatternSwitch, default true.
	if (cadenceOnPatternSwitch) {
		hexGrid.pulseAt(0.5f, 0.5f);
	}

	// Specimen ticker cadence deliberately untouched by the Event Layer
	// phase — see the comment on specimenCard in setup(). ID is the real
	// cycle seed that generated this specimen's geometry, not an arbitrary
	// incrementing tally.
	std::string idStr = ofToString(cycleSeed);
	while (idStr.size() < 6) idStr = "0" + idStr;

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

	reassignEventsSinceLog++;
	if (connectionThreadCooldownTimer <= 0.0f) {
		spawnConnectionThreads(originPx, activeFragmentCentersNorm);
		connectionThreadCooldownTimer = kConnectionThreadCooldown;
		acceptedSpawnsSinceLog++;
	} else {
		rejectedSpawnsSinceLog++;
	}

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
