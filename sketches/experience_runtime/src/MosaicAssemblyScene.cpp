#include "MosaicAssemblyScene.h"

#include "ofMain.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

using mosaic::Bg;
using mosaic::CType;
using mosaic::Content;
using mosaic::ModeId;
using mosaic::Tile;

namespace {
constexpr int kDetents = 11;
int detentOf(float n) { return std::clamp(int(std::lround(n * float(kDetents - 1))), 0, kDetents - 1); }
const char* env(const char* k) { return std::getenv(k); }

SceneMetric identifierMetric(const std::string& id, const std::string& value) {
	SceneMetric m; m.metricId = id; m.valueType = HudMetricValueType::Identifier; m.dataClass = HudDataClass::Literal; m.valueId = value; return m;
}
SceneMetric scalarMetric(const std::string& id, float v) {
	SceneMetric m; m.metricId = id; m.valueType = HudMetricValueType::Scalar; m.dataClass = HudDataClass::Literal; m.value = v; m.normalizedValue = std::clamp(v, 0.f, 1.f); return m;
}
std::string dialText(const mosaic::ModeSpec& sp, int d, float phys) {
	char b[48]; const char* u = sp.dial[d].unit;
	if (phys >= 10.f) std::snprintf(b, sizeof(b), "%.0f %s", phys, u); else if (phys == std::floor(phys)) std::snprintf(b, sizeof(b), "%.0f %s", phys, u); else std::snprintf(b, sizeof(b), "%.2f %s", phys, u);
	return b;
}
const char* bgName(Bg b) { return b == Bg::Video ? "VIDEO" : b == Bg::Desat ? "DESAT" : b == Bg::Black ? "BLACK" : "SOLID"; }
} // namespace

MosaicAssemblyScene::MosaicAssemblyScene(VideoPlaybackService& video) : video_(video) {}

void MosaicAssemblyScene::setup(const SceneServices& services) {
	(void)services;
	const char* q = env("EXPERIENCE_RUNTIME_MOSAIC_QUALITY");
	pi_ = q && std::strcmp(q, "pi") == 0;
	rw_ = pi_ ? kNativeWidth / 2 : kNativeWidth; rh_ = pi_ ? kNativeHeight / 2 : kNativeHeight;
	if (!effects_.setup()) ofLogError("MosaicAssembly") << "VideoEffectService setup failed";
	desat_ = effects_.createInstance("desaturate"); // canonical default (desaturated) look; no other effect is used
	desatOk_ = desat_ && desat_->isReady();
	for (ofFbo* f : {&srcFbo_, &desatFbo_}) { f->allocate(rw_, rh_, GL_RGBA); f->begin(); ofClear(0, 0, 0, 255); f->end(); }
	colorFbo_.allocate(mosaic::kCols, mosaic::kRows, GL_RGBA); colorFbo_.begin(); ofClear(0, 0, 0, 255); colorFbo_.end();
	uint64_t seed = 20261114;
	if (const char* s = env("EXPERIENCE_RUNTIME_MOSAIC_SEED")) seed = std::strtoull(s, nullptr, 10);
	engine_.configure(kNativeWidth, kNativeHeight, pi_, seed);
	if (const char* a = env("EXPERIENCE_RUNTIME_MOSAIC_AUTOCYCLE")) engine_.setAutoCycle(std::strcmp(a, "0") != 0);
	if (const char* m = env("EXPERIENCE_RUNTIME_MOSAIC_MODE")) {
		for (int i = 0; i < mosaic::kModeCount; ++i) if (std::strcmp(mosaic::spec(ModeId(i)).key, m) == 0) { engine_.setMode(ModeId(i)); if (!env("EXPERIENCE_RUNTIME_MOSAIC_AUTOCYCLE")) engine_.setAutoCycle(false); }
	}
	didSetup_ = true; didShutdown_ = false;
}

void MosaicAssemblyScene::activate() {
	if (!didSetup_ || didShutdown_) return;
	active_ = true; time_ = 0; ++generation_;
	engine_.hardReset(engine_.mode()); // fresh deterministic layout on every (re)activation; dials persist
	lastMediaId_.clear(); colorClock_ = 99.f;
}
void MosaicAssemblyScene::deactivate() { active_ = false; }

void MosaicAssemblyScene::update(float dt) {
	if (!active_) return;
	dt_ = dt; time_ += dt;
	const VideoPlaybackStatus st = video_.status();
	const std::string mid = st.mediaId ? *st.mediaId : std::string();
	if (mid != lastMediaId_) { if (!lastMediaId_.empty()) engine_.onMediaChanged(); lastMediaId_ = mid; colorClock_ = 99.f; }
	engine_.update(dt);
}

void MosaicAssemblyScene::renderSource(const ofTexture* tex) {
	srcFbo_.begin();
	ofDisableAlphaBlending(); ofClear(0, 0, 0, 255);
	haveFrame_ = tex && tex->isAllocated() && tex->getWidth() > 0 && tex->getHeight() > 0;
	if (haveFrame_) {
		const float s = std::max(float(rw_) / tex->getWidth(), float(rh_) / tex->getHeight()), dw = tex->getWidth() * s, dh = tex->getHeight() * s;
		ofSetColor(255); tex->draw((rw_ - dw) * 0.5f, (rh_ - dh) * 0.5f, dw, dh); // cover-fit, any aspect (portrait included)
	}
	srcFbo_.end();
}

// One tiny GPU downscale (16x9 = one texel per grid cell) read back at 4 Hz (2 Hz on Pi): ~600 bytes, no full-frame CPU read.
void MosaicAssemblyScene::sampleColors() {
	colorClock_ += dt_;
	if (colorClock_ < (pi_ ? 0.5f : 0.25f) || !haveFrame_) return;
	colorClock_ = 0;
	colorFbo_.begin(); ofDisableAlphaBlending(); ofSetColor(255); srcFbo_.getTexture().draw(0, 0, mosaic::kCols, mosaic::kRows); colorFbo_.end();
	ofPixels px; colorFbo_.readToPixels(px);
	if (px.getWidth() != mosaic::kCols || px.getHeight() != mosaic::kRows) return;
	std::vector<float> rgb(size_t(mosaic::kCells) * 3);
	for (int i = 0; i < mosaic::kCells; ++i) for (int k = 0; k < 3; ++k) rgb[size_t(i) * 3 + size_t(k)] = float(px.getData()[size_t(i) * px.getNumChannels() + size_t(k)]) / 255.f;
	engine_.setColorGrid(rgb);
}

void MosaicAssemblyScene::drawBackground(Bg bg, float alpha) {
	if (alpha <= 0.002f) return;
	const int a = int(255.f * std::clamp(alpha, 0.f, 1.f));
	ofEnableAlphaBlending();
	if (bg == Bg::Video || bg == Bg::Desat) {
		ofSetColor(255, 255, 255, a);
		(bg == Bg::Video ? srcFbo_ : desatFbo_).getTexture().draw(0, 0, kNativeWidth, kNativeHeight);
	} else if (bg == Bg::Solid) {
		float r, g, b; engine_.solidColor(r, g, b); ofSetColor(int(r * 255), int(g * 255), int(b * 255), a); ofDrawRectangle(0, 0, kNativeWidth, kNativeHeight);
	} // Black: the opaque black clear already drawn
}

namespace {
// Tessellates one tile layer: antialiased silhouette = opaque inner fan + 1.4 px rim ramping to alpha 0 at the outline.
// The SAME polygon clips video and solid colour. Points never leave the tile rect (maskOutline clamps).
struct Tess {
	std::vector<mosaic::Pt> outer, inner;
	void build(const Tile& t) {
		mosaic::maskOutline(t.rx, t.ry, t.rw, t.rh, t.drawMask, outer, 6);
		const float cx = t.rx + t.rw * 0.5f, cy = t.ry + t.rh * 0.5f, half = std::max(1.f, std::min(t.rw, t.rh) * 0.5f), k = std::max(0.f, 1.f - 1.4f / half);
		inner.resize(outer.size());
		for (size_t i = 0; i < outer.size(); ++i) inner[i] = {cx + (outer[i].x - cx) * k, cy + (outer[i].y - cy) * k};
	}
};
} // namespace

void MosaicAssemblyScene::drawTiles() {
	const float cellPx = engine_.cellPixels();
	const ofTexture& tileTex = engine_.tileDesat() ? desatFbo_.getTexture() : srcFbo_.getTexture();
	Tess tess;
	auto layer = [&](const Tile& t, const Content& c, float alpha) {
		if (c.type == CType::Vacant || alpha <= 0.003f || t.rw < 2.f) return;
		tess.build(t);
		const size_t n = tess.outer.size(); if (n < 3) return;
		const bool video = c.type == CType::Video; ofMesh& mesh = video ? texMesh_ : solidMesh_;
		float cr = 1, cg = 1, cb = 1; if (!video) { const auto rgb = engine_.tileRgb(c); cr = rgb[0]; cg = rgb[1]; cb = rgb[2]; }
		float u0 = 0, u1 = 1, v0 = 0, v1 = 1;
		if (video) {
			const float su = std::min(1.f, (t.span * cellPx) / float(kNativeWidth) / std::max(0.05f, c.zoom)), sv = std::min(1.f, (t.span * cellPx) / float(kNativeHeight) / std::max(0.05f, c.zoom));
			float a0 = c.u - su * 0.5f, a1 = c.u + su * 0.5f, b0 = c.v - sv * 0.5f, b1 = c.v + sv * 0.5f;
			if (a0 < 0) { a1 -= a0; a0 = 0; } if (a1 > 1) { a0 -= a1 - 1; a1 = 1; } if (b0 < 0) { b1 -= b0; b0 = 0; } if (b1 > 1) { b0 -= b1 - 1; b1 = 1; }
			u0 = c.fx ? a1 : a0; u1 = c.fx ? a0 : a1; v0 = c.fy ? b1 : b0; v1 = c.fy ? b0 : b1; // mirror X = reverse U, mirror Y = reverse V
		}
		auto vert = [&](const mosaic::Pt& p, float a) {
			mesh.addVertex({p.x, p.y, 0});
			mesh.addColor(ofFloatColor(cr, cg, cb, a));
			if (video) { const float fu = std::clamp((p.x - t.rx) / t.rw, 0.f, 1.f), fv = std::clamp((p.y - t.ry) / t.rh, 0.f, 1.f); mesh.addTexCoord(tileTex.getCoordFromPercent(u0 + (u1 - u0) * fu, v0 + (v1 - v0) * fv)); }
		};
		const float cx = t.rx + t.rw * 0.5f, cy = t.ry + t.rh * 0.5f;
		for (size_t i = 0; i < n; ++i) {
			const size_t j = (i + 1) % n;
			vert({cx, cy}, alpha); vert(tess.inner[i], alpha); vert(tess.inner[j], alpha);                       // opaque core
			vert(tess.inner[i], alpha); vert(tess.outer[i], 0.f); vert(tess.outer[j], 0.f);                      // antialiased rim
			vert(tess.inner[i], alpha); vert(tess.outer[j], 0.f); vert(tess.inner[j], alpha);
		}
	};
	auto flush = [&]() {
		if (solidMesh_.getNumVertices()) { solidMesh_.draw(); solidMesh_.clear(); }
		if (texMesh_.getNumVertices()) { tileTex.bind(); texMesh_.draw(); tileTex.unbind(); texMesh_.clear(); }
	};
	solidMesh_.setMode(OF_PRIMITIVE_TRIANGLES); texMesh_.setMode(OF_PRIMITIVE_TRIANGLES);
	ofEnableAlphaBlending(); ofSetColor(255);
	// pass 1: ghosts of the outgoing layout (fading); pass 2: settled tiles (layer A then crossfade layer B); pass 3: tiles in flight on top
	for (const Tile& t : engine_.ghostTiles()) layer(t, t.a, t.alpha);
	flush();
	for (const Tile& t : engine_.allTiles()) if (t.live && !t.pending && !t.moving) layer(t, t.a, t.alpha);
	flush();
	for (const Tile& t : engine_.allTiles()) if (t.live && !t.pending && !t.moving && t.hasB) layer(t, t.b, t.alpha * t.mix);
	flush();
	for (const Tile& t : engine_.allTiles()) if (t.live && (t.pending || t.moving)) { layer(t, t.a, t.alpha); if (t.hasB) layer(t, t.b, t.alpha * t.mix); }
	flush();
	ofDisableAlphaBlending(); ofSetColor(255);
}

void MosaicAssemblyScene::drawToCurrentTarget() {
	if (!didSetup_ || didShutdown_ || !srcFbo_.isAllocated()) return;
	renderSource(video_.currentTexture());
	if (desatOk_) {
		desatFbo_.begin(); ofDisableAlphaBlending(); ofClear(0, 0, 0, 255);
		videoeffects::VideoEffectContext ctx; ctx.sourceTexture = &srcFbo_.getTexture(); ctx.destinationRect = ofRectangle(0, 0, rw_, rh_); ctx.time = time_; ctx.deltaTime = dt_; ctx.alpha = 1.f;
		videoeffects::VideoEffectParameters params; params.set("alpha", 1.0f);
		desat_->update(ctx, params); desat_->render(ctx, params); desatFbo_.end();
	} else { desatFbo_.begin(); ofSetColor(255); srcFbo_.draw(0, 0, rw_, rh_); desatFbo_.end(); }
	sampleColors();
	ofDisableAlphaBlending(); ofSetColor(0); ofDrawRectangle(0, 0, kNativeWidth, kNativeHeight); // a true opaque clear: vacancy never shows stale pixels
	if (engine_.backgroundFade() < 1.f) { drawBackground(engine_.previousBackground(), 1.f); drawBackground(engine_.background(), engine_.backgroundFade()); }
	else drawBackground(engine_.background(), 1.f);
	drawTiles();
	ofSetColor(255);
}

SceneHudStatus MosaicAssemblyScene::hudStatus() const {
	SceneHudStatus status; status.schemaVersion = 1; status.sceneId = kSceneId; status.displayName = displayName();
	if (didShutdown_) { status.health = SceneHealth::Failed; status.message = std::string("shut down"); }
	else if (!didSetup_ || !active_) status.health = SceneHealth::Loading;
	else if (!haveFrame_) { status.health = SceneHealth::Degraded; status.message = std::string("No video frame"); }
	else if (!desatOk_) { status.health = SceneHealth::Degraded; status.message = std::string("Desaturate unavailable; untreated video"); }
	else status.health = SceneHealth::Ready;
	{ VideoPlaybackStatus vs = video_.status(); if (vs.fallbackDisplayTitle) status.mediaName = vs.fallbackDisplayTitle; else if (vs.titleId) status.mediaName = vs.titleId; }
	if (didSetup_ && !didShutdown_) {
		const auto& sp = mosaic::spec(engine_.mode()); SceneModeStatus mode; mode.modeId = sp.key; mode.displayName = sp.title; mode.index = int(engine_.mode()); mode.count = mosaic::kModeCount;
		status.mode = mode; status.modeName = mode.displayName;
	}
	if (didSetup_ && active_ && !didShutdown_) {
		const auto& t = engine_.telemetry(); const ModeId mm = engine_.mode(); const auto& sp = mosaic::spec(mm);
		SceneSemanticData data; data.state.primaryStateId = "scene.mosaic.state.online"; data.timing.activeSeconds = time_;
		for (int d = 0; d < 2; ++d) {
			const std::string k = "scene.mosaic.metric.c" + std::to_string(d) + "_";
			data.metrics.push_back(identifierMetric(k + "label", sp.dial[d].label));
			data.metrics.push_back(identifierMetric(k + "value", dialText(sp, d, engine_.dialPhys(mm, d))));
			data.metrics.push_back(identifierMetric(k + "pos", std::to_string(detentOf(engine_.dial(mm, d)) + 1) + "/" + std::to_string(kDetents)));
		}
		data.metrics.push_back(identifierMetric("scene.mosaic.metric.mode", sp.key));
		data.metrics.push_back(identifierMetric("scene.mosaic.metric.phase", t.phase));
		data.metrics.push_back(identifierMetric("scene.mosaic.metric.background", bgName(t.bg)));
		data.metrics.push_back(scalarMetric("scene.mosaic.metric.occupancy", t.occupancy));
		data.metrics.push_back(scalarMetric("scene.mosaic.metric.vacancy", t.vacancy));
		data.metrics.push_back(scalarMetric("scene.mosaic.metric.movers", float(t.movers)));
		status.activeItemCount = t.tiles;
		status.semantic = data;
	}
	status.paused = false;
	return status;
}

SceneCapabilities MosaicAssemblyScene::capabilities() const {
	SceneCapabilities caps;
	auto cmd = [&caps](SceneCommand c, const char* label, const char* shortLabel) { SceneCommandDescriptor d; d.command = c; d.label = label; d.shortLabel = shortLabel; caps.commands.push_back(d); };
	cmd(SceneCommand::NextMedia, "Next", "NEXT"); cmd(SceneCommand::PreviousMedia, "Previous", "PREV");
	cmd(SceneCommand::Regenerate, "Reseed", "RESEED"); cmd(SceneCommand::NextMode, "Next mode", "NEXT MODE");
	for (int d = 0; d < 2; ++d) { SceneOptionAxis a; a.id = "dial" + std::to_string(d); a.label = mosaic::spec(engine_.mode()).dial[d].label; a.count = kDetents; caps.optionAxes.push_back(a); }
	return caps; // exactly the active mode's two dials; Reset honoured, not advertised
}

bool MosaicAssemblyScene::executeCommand(SceneCommand command) {
	if (!didSetup_ || didShutdown_) return false;
	switch (command) {
		case SceneCommand::NextMedia: return video_.next();
		case SceneCommand::PreviousMedia: return video_.previous();
		case SceneCommand::Regenerate: ++generation_; engine_.reseed(engine_.currentSeed() * 6364136223846793005ULL + 1442695040888963407ULL + generation_); return true;
		case SceneCommand::NextMode: engine_.setMode(ModeId((int(engine_.mode()) + 1) % mosaic::kModeCount)); return true;
		case SceneCommand::Reset: reset(); return true;
		default: return false;
	}
}

bool MosaicAssemblyScene::stepOption(int axisIndex, int delta) {
	if (!didSetup_ || didShutdown_ || axisIndex < 0 || axisIndex > 1) return false;
	const ModeId m = engine_.mode();
	const int i = std::clamp(detentOf(engine_.dial(m, axisIndex)) + (delta > 0 ? 1 : -1), 0, kDetents - 1);
	engine_.setDial(m, axisIndex, float(i) / float(kDetents - 1));
	return true;
}

void MosaicAssemblyScene::reset() { engine_.resetDials(); engine_.hardReset(engine_.mode()); time_ = 0; colorClock_ = 99.f; }

void MosaicAssemblyScene::shutdown() { active_ = false; didShutdown_ = true; desat_.reset(); }
