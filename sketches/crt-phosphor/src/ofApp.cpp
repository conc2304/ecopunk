#include "ofApp.h"
#include "../../../shared/src/video-playback/SharedMediaRoot.h"

#include <algorithm>
#include <cstdlib>

void ofApp::setup() {
	ofSetFrameRate(60);
	ofSetVerticalSync(true);
	ofDisableArbTex(); // the shared pipeline is GL_TEXTURE_2D regardless; keeps the video texture 2D too
	ofBackground(0);

	presets = crt::makePresets();
	if (const char* d = std::getenv("CRT_SHADER_DIR")) shaderDir = d;
	if (const char* d = std::getenv("CRT_AUTOSHOT_DIR")) autoDir = d;

	ofLogNotice("crt") << "GL " << glGetString(GL_VERSION) << " / GLSL " << glGetString(GL_SHADING_LANGUAGE_VERSION)
					   << " / " << glGetString(GL_RENDERER);

	// Media: sketch-local bin/data/media wins, else canonical shared library.
	ofDirectory local(ofToDataPath("media", true));
	std::string dir = (local.exists() && local.listDir() > 0) ? ofToDataPath("media", true) : ecopunk::sharedMediaRoot();
	ofDirectory d(dir);
	d.allowExt("mp4"); d.allowExt("mov"); d.allowExt("m4v");
	if (d.exists()) { d.listDir(); d.sort(); for (auto& f : d.getFiles()) clips.push_back(f.getAbsolutePath()); }
	ofLogNotice("crt") << clips.size() << " clips in " << dir;

	int start = 0;
	if (const char* n = std::getenv("CRT_VIDEO")) {
		for (size_t i = 0; i < clips.size(); i++) if (clips[i].find(n) != std::string::npos) { start = (int)i; break; }
	}
	const char* srcEnv = std::getenv("CRT_SOURCE");
	if (!clips.empty() && !(srcEnv && std::string(srcEnv) == "pattern")) openVideo(start);
	else ofLogWarning("crt") << "no video in use -> procedural test pattern (fallback)";

	applyPreset(0);
	manualPal = crt::manualPalettes();
	if (const char* e = std::getenv("CRT_FIXED_DT")) fixedDt = (float)std::atof(e);
	if (const char* e = std::getenv("CRT_FREEZE")) freezeAt = (float)std::atof(e);
	if (const char* e = std::getenv("CRT_PRESET")) applyPreset(std::atoi(e) % 3);
	if (const char* e = std::getenv("CRT_PALETTE")) setPalette(std::atoi(e) % 3);
	if (const char* e = std::getenv("CRT_STRUCTURE")) { if (auto* i = crt::structureById(e)) setStructure(i->s); }
	if (const char* e = std::getenv("CRT_CMAP")) { if (auto* i = crt::colorMapById(e)) setCmap(i->c); }
	if (const char* e = std::getenv("CRT_BEHAVIOR")) { if (auto* i = crt::behaviorById(e)) setBehavior(i->b); }
	if (const char* e = std::getenv("CRT_MODE")) { if (auto* mi = crt::modeById(e)) setMode(mi->mode); }
	// Dev/automation start-state overrides (also handy for isolating a pass).
	if (std::getenv("CRT_BYPASS")) bypass = true;
	if (std::getenv("CRT_NO_PERSIST")) persistOn = false;
	if (std::getenv("CRT_NO_BLOOM")) bloomOn = false;
	pipe.loadShaders(shaderDir);
	allocate();
	if (std::getenv("CRT_MATRIX")) buildMatrixScript();
	if (std::getenv("CRT_GALLERY")) buildGalleryScript();
}

void ofApp::allocate() {
	float scale = quality == 0 ? 1.f : 0.5f;
	pipe.allocate(int(ofGetWidth() * scale), int(ofGetHeight() * scale), quality);
	ofLogNotice("crt") << "FBOs: internal " << pipe.width() << "x" << pipe.height() << " bloom " << pipe.bloomWidth() << "x" << pipe.bloomHeight()
					   << " RGBA8, ~" << ofToString(pipe.gpuBytes() / 1048576.0, 2) << " MiB GPU (6 targets)";
}

void ofApp::openVideo(int index) {
	if (clips.empty()) return;
	clipIndex = (index % (int)clips.size() + (int)clips.size()) % (int)clips.size();
	video.close();
	if (video.load(clips[clipIndex])) {
		video.setLoopState(OF_LOOP_NORMAL);
		video.setVolume(0);
		video.play();
		useVideo = true;
		awaitingFrame = true; // never draw the previous clip's texture
		lastPos = 0;
		ofLogNotice("crt") << "video: " << clips[clipIndex];
	} else {
		useVideo = false;
		ofLogError("crt") << "video load failed, using test pattern: " << clips[clipIndex];
	}
	if (pipe.allocated()) pipe.clearHistory(); // never carry the previous source's imagery
}

void ofApp::applyPreset(int i) {
	presetIdx = i;
	P = presets[i];
	ofLogNotice("crt") << "preset " << P.name;
}

void ofApp::drawPattern(float w, float h) {
	float t = simTime;
	ofSetColor(0); ofDrawRectangle(0, 0, w, h);
	for (int i = 0; i < 64; i++) { ofSetColor(i * 4); ofDrawRectangle(w * 0.05f + i * w * 0.9f / 64.f, h * 0.9f, w * 0.9f / 64.f + 1, h * 0.06f); }
	ofPushMatrix();
	ofTranslate(w * 0.5f + sin(t * 0.5f) * w * 0.04f, h * 0.5f);
	for (int i = 20; i > 0; i--) { ofSetColor(255 * (1.f - i / 22.f) + 20); ofDrawEllipse(0, h * 0.62f, w * 0.55f * i / 20.f, h * 0.55f * i / 20.f); }
	for (int i = 20; i > 0; i--) { ofSetColor(255 * (1.f - i / 24.f) + 30); ofDrawEllipse(0, -h * 0.05f, w * 0.2f * i / 20.f, h * 0.42f * i / 20.f); }
	ofSetColor(0); ofDrawEllipse(-w * 0.04f, -h * 0.1f, w * 0.025f, h * 0.03f); ofDrawEllipse(w * 0.04f, -h * 0.1f, w * 0.025f, h * 0.03f);
	ofPopMatrix();
	ofSetColor(255); ofDrawRectangle(fmod(t * 160.f, w + 80) - 80, h * 0.08f, 80, 10);
	for (int i = 0; i < 5; i++) { ofSetColor(80 + i * 40); ofDrawCircle(w * 0.15f + cos(t + i) * w * 0.08f, h * 0.5f + sin(t * 1.3f + i) * h * 0.25f, 14); }
	ofSetColor(255); // orientation marker: asymmetric glyph, top-left
	ofPushMatrix(); ofTranslate(20, 20); ofScale(5, 5); ofDrawBitmapString("R", 0, 10); ofPopMatrix();
}

void ofApp::renderSource() {
	const int W = pipe.width(), H = pipe.height();
	pipe.sourceFbo().begin();
	ofDisableAlphaBlending();
	ofSetColor(255);
	if (useVideo && awaitingFrame) {
		ofSetColor(0); ofDrawRectangle(0, 0, W, H); // black until the new clip's first frame
	} else if (useVideo && video.isLoaded() && video.getTexture().isAllocated()) {
		ofSetColor(0); ofDrawRectangle(0, 0, W, H); ofSetColor(255);
		float vw = video.getWidth(), vh = video.getHeight();
		float s = coverFit ? std::max(W / vw, H / vh) : std::min(W / vw, H / vh);
		float dw = vw * s, dh = vh * s;
		video.draw((W - dw) * 0.5f, (H - dh) * 0.5f, dw, dh);
	} else {
		drawPattern(W, H);
	}
	pipe.sourceFbo().end();
}

void ofApp::update() {
	simTime += fixedDt > 0.f ? fixedDt : ofGetLastFrameTime();
	if (useVideo && freezeAt >= 0.f && !frozen && ofGetFrameNum() > 10) {
		video.setPosition(freezeAt); video.setPaused(true); frozen = true; // same source frame across modes
	}
	if (useVideo) {
		video.update();
		if (awaitingFrame && video.isFrameNew()) { awaitingFrame = false; pipe.clearHistory(); }
		float pos = video.getPosition();
		if (pos + 0.25f < lastPos) pipe.clearHistory(); // loop discontinuity
		lastPos = pos;
	}
}

void ofApp::draw() {
	auto t0 = std::chrono::steady_clock::now();
	pipe.setPassTiming(timing);
	renderSource();

	if (!bypass) {
		crt::Pipeline::Options o;
		o.bloom = bloomOn; o.persistence = persistOn;
		o.mode = mode; o.palette = resolvePalette(); o.modes = modeSettings; o.display = disp;
		pipe.process(P, simTime, fixedDt > 0.f ? fixedDt : ofGetLastFrameTime(), o);
	} else {
		pipe.clearHistory();
	}
	const bool treat = !bypass && pipe.usable();

	ofSetColor(255);
	ofBackground(0);
	(treat ? pipe.output() : pipe.sourceFbo()).draw(0, 0, ofGetWidth(), ofGetHeight());

	double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
	frameMsAvg = frameMsAvg * 0.95 + ms * 0.05;
	if (ofGetFrameNum() > 60) { frameMsSum += ofGetLastFrameTime() * 1000.0; frameMsN++; }

	if (debug) {
		const crt::Palette pal = resolvePalette();
		std::string s = std::string("MODE ") + crt::modeInfo(mode).name + " (" + crt::modeInfo(mode).id + ")   LOOK " + P.name
			+ "   PALETTE " + pal.source + "  sec " + ofToString(pal.secondaryInfluence, 2) + "\n"
			+ "CRT PHOSPHOR  " + ofToString(ofGetFrameRate(), 1) + " fps"
			+ "  cpu-submit " + ofToString(frameMsAvg, 2) + " ms\n"
			+ (treat ? "treatment ON" : (bypass ? "BYPASS" : "TREATMENT UNAVAILABLE (shader failure)"))
			+ "  bloom:" + (bloomOn ? "on" : "off") + "  persist:" + (persistOn ? "on" : "off")
			+ "  quality:" + (quality ? "low" : "full") + "  src:" + (useVideo ? "video" : "pattern") + (coverFit ? " cover" : " fit") + "\n"
			+ "shaders " + pipe.status() + "\n"
			+ "internal " + ofToString(pipe.width()) + "x" + ofToString(pipe.height()) + "  bloom " + ofToString(pipe.bloomWidth()) + "x" + ofToString(pipe.bloomHeight()) + "\n";
		if (timing) {
			s += "per-pass wall ms (glFinish, DESKTOP diagnostic only): mode " + ofToString(pipe.passMs(crt::Pipeline::P_MODE), 2) + " base " + ofToString(pipe.passMs(crt::Pipeline::P_BASE), 2)
				+ " bloom " + ofToString(pipe.passMs(crt::Pipeline::P_BLOOM), 2) + " comp " + ofToString(pipe.passMs(crt::Pipeline::P_COMP), 2) + "\n";
		}
		if (useVideo && clipIndex >= 0) s += ofFilePath::getFileName(clips[clipIndex]) + "\n";
		if (!lastShot.empty()) s += "shot: " + lastShot + "\n";
		s += modeParamText() + "\n";
		s += axesText();
		s += "B bypass L bloom P persist 1/2/3 look D dbg R reload S shot N/M video V src F fit Q qual T timing [ ] mode | x/z/a struct/cmap/behav g dither -/= pitch 0 reset w panel o/i recipe";
		ofSetColor(0, 0, 0, 170); ofDrawRectangle(8, 8, 900, showPanel ? 206 : 146);
		ofSetColor(235); ofDrawBitmapString(s, 14, 22);
		// resolved palette swatches (primary dominant, secondary accent)
		ofSetColor(pal.primary.x * 255, pal.primary.y * 255, pal.primary.z * 255); ofDrawRectangle(780, 14, 56, 18);
		ofSetColor(pal.secondary.x * 255, pal.secondary.y * 255, pal.secondary.z * 255); ofDrawRectangle(840, 14, 56, 18);
	}

	autoshotStep();
}

void ofApp::savePng(const std::string& tag) {
	// User-triggered (or autoshot) readback of the final composite, never in the live path.
	ofFbo& f = (bypass || !pipe.usable()) ? pipe.sourceFbo() : pipe.output();
	ofPixels px;
	f.readToPixels(px);
	std::string dir = autoDir.empty() ? ofToDataPath("screenshots", true) : autoDir;
	ofDirectory::createDirectory(dir, true, true);
	std::string path = dir + "/crt_" + tag + "_" + ofGetTimestampString("%H%M%S") + ".png";
	ofSaveImage(px, path);
	lastShot = path;
	ofLogNotice("crt") << "screenshot " << path << " (" << px.getWidth() << "x" << px.getHeight() << ")";
}

// Deterministic acceptance run: shot default, clean, damaged, then print stats and exit.
void ofApp::autoshotStep() {
	if (autoDir.empty()) return;
	autoFrame++;
	if (!script.empty()) {
		while (scriptPos < script.size() && script[scriptPos].first <= autoFrame) runAction(script[scriptPos++].second);
		return;
	}
	if (autoFrame == 150) savePng("reference");
	if (autoFrame == 151) applyPreset(1);
	if (autoFrame == 210) savePng("clean");
	if (autoFrame == 211) applyPreset(2);
	if (autoFrame == 270) savePng("damaged");
	if (autoFrame == 271) { timing = true; applyPreset(0); }
	if (autoFrame == 400) {
		ofLogNotice("crt") << "STATS avg frame " << ofToString(frameMsN ? frameMsSum / frameMsN : 0, 2) << " ms over " << frameMsN
						   << " frames; per-pass ms: base " << pipe.passMs(crt::Pipeline::P_BASE) << " bloom " << pipe.passMs(crt::Pipeline::P_BLOOM)
						   << " comp " << pipe.passMs(crt::Pipeline::P_COMP);
		ofExit(0);
	}
}


std::string ofApp::axesText() const {
	const crt::DisplaySettings& st = disp.settings;
	std::string t = std::string("[ ] MODE ") + crt::modeInfo(mode).id + "  |  STRUCTURE " + crt::structureInfo(disp.structure).id
		+ "  |  COLOR " + crt::colorMapInfo(disp.cmap).id + "  |  BEHAVIOR " + crt::behaviorInfo(disp.behavior).id
		+ "  |  dither " + (disp.dither == 0 ? "off" : disp.dither == 1 ? "low(2x2)" : "high(4x4)") + "  seed " + ofToString(disp.seed) + "\\n";
	if (showPanel) {
		t += std::string("pitch x") + ofToString(st.cellScale, 2) + (st.dense ? " dense" : " normal")
			+ "  float " + ofToString(st.floatEnergy, 2) + (st.floatEnergy > 0.7f ? " energized" : " calm")
			+ "  ramp '" + st.glyphRamp + "'  beam " + ofToString(st.beamW, 1) + "px  levels " + ofToString(st.levels, 0) + "\\n";
		t += std::string("effective: structure ") + crt::structureInfo(pipe.lastStructure()).id + ", behavior " + crt::behaviorInfo(pipe.lastBehavior()).id + "\\n";
		t += warn.empty() ? std::string("compat: ok\\n") : std::string("WARNING: ") + warn + "\\n";
	}
	return t;
}

void ofApp::setStructure(crt::Structure s) {
	disp.structure = s;
	if (const char* why = crt::behaviorBlockReason(s, disp.behavior)) {
		warn = std::string("behavior ") + crt::behaviorInfo(disp.behavior).id + " reset to none: " + why;
		disp.behavior = crt::Behavior::None; // deterministic + non-destructive
	} else warn.clear();
	ofLogNotice("crt") << "structure " << crt::structureInfo(s).id << (warn.empty() ? "" : "  (" + warn + ")");
}

void ofApp::setBehavior(crt::Behavior b) {
	if (const char* why = crt::behaviorBlockReason(disp.structure, b)) {
		warn = std::string("rejected ") + crt::behaviorInfo(b).id + " on " + crt::structureInfo(disp.structure).id + ": " + why + " (kept " + crt::behaviorInfo(disp.behavior).id + ")";
		ofLogNotice("crt") << warn;
		return;
	}
	warn.clear();
	disp.behavior = b;
	ofLogNotice("crt") << "behavior " << crt::behaviorInfo(b).id;
}

void ofApp::setCmap(crt::ColorMap c) {
	disp.cmap = c;
	ofLogNotice("crt") << "color mapping " << crt::colorMapInfo(c).id;
}

void ofApp::resetToReference() {
	mode = crt::Mode::PhosphorPortrait;
	disp = crt::Display();
	applyPreset(0);
	setPalette(0);
	secInf = 0.30f;
	warn.clear();
	pipe.clearHistory();
	ofLogNotice("crt") << "reset to reference: phosphor_portrait + original_phosphor + classic_mono + none + GREEN_PHOSPHOR_REFERENCE";
}

// ---- gallery: tiles are captured from the final composite (user/script-triggered readback only)
void ofApp::captureTile(const std::string& label) {
	Tile t; t.label = label;
	ofFbo& f = (bypass || !pipe.usable()) ? pipe.sourceFbo() : pipe.output();
	f.readToPixels(t.px);
	tiles.push_back(std::move(t));
}

void ofApp::composeSheet(const std::string& name, int cols) {
	if (tiles.empty()) return;
	const int tw = 480, th = 270, lab = 20, pad = 6;
	int rows = ((int)tiles.size() + cols - 1) / cols;
	ofFbo sheet;
	ofFboSettings fs; fs.width = cols * (tw + pad) + pad; fs.height = rows * (th + lab + pad) + pad;
	fs.internalformat = GL_RGBA; fs.useDepth = false; fs.numSamples = 0;
	sheet.allocate(fs);
	sheet.begin();
	ofEnableAlphaBlending();
	ofClear(24, 24, 28, 255);
	for (size_t i = 0; i < tiles.size(); i++) {
		int cx = (int)i % cols, cy = (int)i / cols;
		float x = pad + cx * (tw + pad), y = pad + cy * (th + lab + pad);
		ofImage img; img.setFromPixels(tiles[i].px);
		img.getTexture().setTextureMinMagFilter(GL_LINEAR, GL_LINEAR);
		ofSetColor(255); img.draw(x, y, tw, th);
		ofSetColor(235, 235, 200); ofDrawBitmapString(tiles[i].label, x + 2, y + th + 14); // label OUTSIDE the image
	}
	sheet.end();
	ofPixels px; sheet.readToPixels(px);
	std::string dir = autoDir.empty() ? ofToDataPath("screenshots", true) : autoDir;
	ofDirectory::createDirectory(dir, true, true);
	std::string path = dir + "/sheet_" + name + ".png";
	ofSaveImage(px, path);
	ofLogNotice("crt") << "sheet " << path << " (" << px.getWidth() << "x" << px.getHeight() << ", " << tiles.size() << " tiles)";
	tiles.clear();
}

// ---- recipes: LOCAL sketch JSON only (stable string IDs); NOT EffectPresetIds, not shared knowledge
void ofApp::saveRecipe() {
	ofJson j;
	j["mode"] = crt::modeInfo(mode).id; j["structure"] = crt::structureInfo(disp.structure).id;
	j["color_mapping"] = crt::colorMapInfo(disp.cmap).id; j["behavior"] = crt::behaviorInfo(disp.behavior).id;
	j["look"] = presetIdx; j["theme_palette"] = paletteIdx; j["dither"] = disp.dither; j["seed"] = disp.seed;
	j["dense"] = disp.settings.dense; j["pitch_scale"] = disp.settings.cellScale; j["float_energy"] = disp.settings.floatEnergy;
	std::string dir = ofToDataPath("recipes", true);
	ofDirectory::createDirectory(dir, true, true);
	std::string path = dir + "/recipe_" + ofGetTimestampString("%Y%m%d_%H%M%S") + ".json";
	ofSavePrettyJson(path, j);
	ofLogNotice("crt") << "recipe saved " << path;
}

void ofApp::loadRecipe() {
	ofDirectory d(ofToDataPath("recipes", true));
	d.allowExt("json");
	if (!d.exists() || d.listDir() == 0) { warn = "no recipes saved yet"; return; }
	d.sort();
	auto f = d.getFile(recipeCursor++ % d.size());
	ofJson j = ofLoadJson(f.getAbsolutePath());
	std::string unknown;
	auto pickMode = j.value("mode", std::string()); if (auto* i = crt::modeById(pickMode)) { if (i->implemented) mode = i->mode; } else unknown += " mode:" + pickMode;
	if (auto* i = crt::structureById(j.value("structure", std::string()))) disp.structure = i->s; else unknown += " structure";
	if (auto* i = crt::colorMapById(j.value("color_mapping", std::string()))) disp.cmap = i->c; else unknown += " color_mapping";
	disp.behavior = crt::Behavior::None;
	if (auto* i = crt::behaviorById(j.value("behavior", std::string()))) { if (!crt::behaviorBlockReason(disp.structure, i->b)) disp.behavior = i->b; else unknown += " behavior(incompatible)"; } else unknown += " behavior";
	applyPreset(std::clamp(j.value("look", 0), 0, 2)); setPalette(std::clamp(j.value("theme_palette", 0), 0, 2));
	disp.dither = std::clamp(j.value("dither", 0), 0, 2); disp.seed = j.value("seed", 1u);
	disp.settings.dense = j.value("dense", true); disp.settings.cellScale = j.value("pitch_scale", 0.5f); disp.settings.floatEnergy = j.value("float_energy", 0.55f);
	warn = unknown.empty() ? std::string() : "recipe had unknown/unsupported IDs, defaults used:" + unknown;
	pipe.clearHistory();
	ofLogNotice("crt") << "recipe loaded " << f.getFileName() << (unknown.empty() ? "" : " [" + unknown + "]");
}

void ofApp::setMode(crt::Mode m) {
	if (!crt::modeInfo(m).implemented) { ofLogNotice("crt") << crt::modeInfo(m).id << ": planned, not implemented"; return; }
	if (m == mode) return;
	mode = m;                 // pipeline clears mode-local history on change/re-entry
	ofLogNotice("crt") << "mode " << crt::modeInfo(m).id;
}

void ofApp::setPalette(int i) {
	paletteIdx = i;
	ofLogNotice("crt") << "palette " << manualPal[i].source;
}

// Precedence: manual developer override -> theme palette (NOT reachable: no
// scene-accessible theme API exists in this repo) -> local green fallback.
crt::Palette ofApp::resolvePalette() const {
	crt::Palette p = manualPal[paletteIdx]; // classic_mono = paletteIdx 0 (green); theme = manual override
	p.secondaryInfluence = secInf;
	return p;
}

std::string ofApp::modeParamText() const {
	switch (mode) {
	case crt::Mode::RasterExcavation:
		return "excavation: scan " + ofToString(modeSettings.excavation.scanSeconds, 1) + "s  band " + ofToString(modeSettings.excavation.bandWidth, 3)
			+ "  tau " + ofToString(modeSettings.excavation.tau, 1) + "s  " + (modeSettings.excavation.vertical ? "vertical" : "horizontal") + "  (K/J speed, H dir)";
	case crt::Mode::PhosphorMemory:
		return "memory: tau " + ofToString(modeSettings.memory.tau, 2) + "s  GPU luma-diff motion weight (frame-difference proxy, not optical flow)  (K/J)";
	case crt::Mode::SignalColumns:
		return "columns: group " + ofToString(modeSettings.columns.groupPx, 0) + "px  amp " + ofToString(modeSettings.columns.amp, 3) + "  spatial/time displacement only, no delayed sampling  (K/J)";
	default: return "portrait: no mode-local parameters";
	}
}

void ofApp::runAction(const std::string& a) {
	auto eq = a.find('=');
	std::string k = a.substr(0, eq), v = eq == std::string::npos ? "" : a.substr(eq + 1);
	if (k == "mode") { if (auto* mi = crt::modeById(v)) setMode(mi->mode); }
	else if (k == "palette") setPalette(std::atoi(v.c_str()));
	else if (k == "preset") applyPreset(std::atoi(v.c_str()));
	else if (k == "shot") savePng(v);
	else if (k == "next_video") openVideo(clipIndex + 1);
	else if (k == "timing") timing = (v == "1");
	else if (k == "perf") {
		perfLog += "PERF " + v + ": mode " + ofToString(pipe.passMs(crt::Pipeline::P_MODE), 3) + " ms, base " + ofToString(pipe.passMs(crt::Pipeline::P_BASE), 3)
			+ " ms, bloom " + ofToString(pipe.passMs(crt::Pipeline::P_BLOOM), 3) + " ms, comp " + ofToString(pipe.passMs(crt::Pipeline::P_COMP), 3)
			+ " ms | GPU ~" + ofToString(pipe.gpuBytes() / 1048576.0, 2) + " MiB (lazy mode FBOs included)\n";
	}
	else if (k == "structure") { if (auto* i = crt::structureById(v)) setStructure(i->s); }
	else if (k == "cmap") { if (auto* i = crt::colorMapById(v)) setCmap(i->c); }
	else if (k == "behavior") { if (auto* i = crt::behaviorById(v)) setBehavior(i->b); }
	else if (k == "dither") disp.dither = std::atoi(v.c_str());
	else if (k == "seed") disp.seed = (uint32_t)std::atoi(v.c_str());
	else if (k == "dense") disp.settings.dense = (v == "1");
	else if (k == "energy") disp.settings.floatEnergy = (float)std::atof(v.c_str());
	else if (k == "pitch") disp.settings.cellScale = (float)std::atof(v.c_str());
	else if (k == "ramp") disp.settings.glyphRamp = v;
	else if (k == "pause") { if (useVideo) video.setPaused(v == "1"); }
	else if (k == "reset") resetToReference();
	else if (k == "tile") captureTile(v);
	else if (k == "sheet") { auto cpos = v.find(','); composeSheet(v.substr(0, cpos), std::atoi(v.substr(cpos + 1).c_str())); }
	else if (k == "mem") perfLog += "MEM " + v + ": GPU ~" + ofToString(pipe.gpuBytes() / 1048576.0, 2) + " MiB\n";
	else if (k == "perfs") {
		perfLog += "PERFS " + v + ": mode " + ofToString(pipe.passMs(crt::Pipeline::P_MODE), 3) + ", display " + ofToString(pipe.passMs(crt::Pipeline::P_BASE), 3)
			+ ", bloom " + ofToString(pipe.passMs(crt::Pipeline::P_BLOOM), 3) + ", comp " + ofToString(pipe.passMs(crt::Pipeline::P_COMP), 3) + " ms\n";
	}
	else if (k == "exit") { ofLogNotice("crt") << "\n" << perfLog; ofExit(0); }
}


// CRT_MATRIX=1 (+ CRT_AUTOSHOT_DIR): deterministic capture matrix. With CRT_SOURCE=pattern
// and CRT_FIXED_DT it is reproducible; with video it additionally proves history reset on
// source change. Times are in frames.
void ofApp::buildMatrixScript() {
	int f = 5;
	auto add = [&](int at, const std::string& a) { script.push_back({at, a}); };
	modeSettings.excavation.scanSeconds = 3.f;
	const char* ids[4] = { "phosphor_portrait", "raster_excavation", "phosphor_memory", "signal_columns" };
	const char* tag[4] = { "m1_portrait", "m2_excavation", "m3_memory", "m4_columns" };
	const char* pal[3] = { "green", "amber", "violet" };
	for (int m = 0; m < 4; m++) {
		add(f, std::string("mode=") + ids[m]); add(f, "palette=0"); add(f, "preset=0");
		f += (m == 1 ? 100 : 120);
		add(f, std::string("shot=") + tag[m] + "_green");
		for (int p = 1; p < 3; p++) { add(f + 1, "palette=" + ofToString(p)); f += 14; add(f, std::string("shot=") + tag[m] + "_" + pal[p]); }
		add(f + 1, "palette=0"); f += 20;
	}
	if (useVideo) {
		for (int m = 1; m < 3; m++) { // excavation + memory: source-change reset proof
			add(f, std::string("mode=") + ids[m]); f += 150;
			add(f, std::string("shot=") + tag[m] + "_before_source_change");
			add(f + 1, "next_video"); f += 4;
			add(f, std::string("shot=") + tag[m] + "_after_source_change"); f += 30;
		}
	}
	for (int m = 0; m < 4; m++) { // per-mode GPU pass timing (glFinish, desktop only)
		add(f, std::string("mode=") + ids[m]); add(f + 40, "timing=1"); add(f + 160, std::string("perf=") + ids[m]); add(f + 161, "timing=0"); f += 170;
	}
	add(f + 2, "exit");
}

// CRT_GALLERY=1 (+ CRT_AUTOSHOT_DIR, CRT_FIXED_DT, CRT_VIDEO, optional CRT_FREEZE): contact sheets for all display axes.
void ofApp::buildGalleryScript() {
	int f = 12;
	auto add = [&](const std::string& a) { script.push_back({f, a}); };
	auto settle = [&](int n = 10) { f += n; };
	auto cfg = [&](const std::string& s, const std::string& cm, const std::string& bh = "none", const std::string& mo = "phosphor_portrait") {
		add("mode=" + mo); add("structure=" + s); add("cmap=" + cm); add("behavior=" + bh);
		add(cm == "theme_primary_secondary" ? "palette=1" : "palette=0"); // theme = manual amber/teal override
		settle();
	};
	const char* structs[6] = { "original_phosphor", "rgb_triad", "aperture_grille", "round_matrix", "ascii_matrix", "vector_beam" };
	const char* cmaps[9] = { "classic_mono", "theme_primary_secondary", "source_rgb", "ega16", "sunset", "biolum", "thermal", "four_tone", "dual_phosphor" };
	add("reset"); settle(30);
	add("tile=BASELINE phosphor_portrait+original_phosphor+classic_mono+none"); settle(1);
	add("sheet=01_baseline,1");
	// 2. structure contact sheets (same frame, same palette, same scaling)
	for (const char* cm : { "source_rgb", "classic_mono" }) {
		for (auto s : structs) { cfg(s, cm); add(std::string("tile=") + s + " / " + cm); settle(1); }
		add(std::string("sheet=02_structures_") + cm + ",3"); settle(1);
	}
	// dense variants
	add("dense=0"); add("pitch=1");
	for (auto s : { "rgb_triad", "aperture_grille", "round_matrix", "ascii_matrix" }) { cfg(s, "source_rgb"); add(std::string("tile=NORMAL (coarse) ") + s); settle(1); }
	add("sheet=02b_structures_coarse,2"); add("dense=1"); add("pitch=0.5"); settle(1);
	// 3. palette grids on two structures
	for (auto s : { "round_matrix", "aperture_grille" }) {
		for (auto cm : cmaps) { cfg(s, cm); add(std::string("tile=") + s + " / " + cm); settle(1); }
		add(std::string("sheet=03_palettes_") + s + ",3"); settle(1);
	}
	for (int d = 0; d < 3; d++) { cfg("round_matrix", "four_tone"); add("dither=" + ofToString(d)); settle(); add("tile=four_tone dither " + std::string(d == 0 ? "off" : d == 1 ? "low" : "high")); settle(1); }
	for (int d = 0; d < 3; d++) { cfg("round_matrix", "ega16"); add("dither=" + ofToString(d)); settle(); add("tile=ega16 dither " + std::string(d == 0 ? "off" : d == 1 ? "low" : "high")); settle(1); }
	add("sheet=03b_dither,3"); add("dither=0"); settle(1);
	// 4. behaviors: 3 frames each, 20 frames apart
	struct B { const char* s; const char* id; const char* cm; };
	const B bs[] = { { "round_matrix", "none", "sunset" }, { "round_matrix", "adaptive_mosaic", "sunset" }, { "round_matrix", "floating_phosphors", "sunset" },
	                 { "aperture_grille", "interlaced_color_fields", "source_rgb" }, { "round_matrix", "interlaced_color_fields", "source_rgb" } };
	for (auto& b : bs) {
		cfg(b.s, b.cm, b.id); add("pause=0");
		for (int k = 0; k < 3; k++) { add(std::string("tile=") + b.s + " + " + b.id + " t" + ofToString(k)); settle(20); }
	}
	add("sheet=04_behaviors,3");
	// 5. composition cross-check
	for (auto mo : { "raster_excavation", "phosphor_memory" })
		for (auto s : { "round_matrix", "rgb_triad" })
			for (auto cm : { "sunset", "ega16" }) { cfg(s, cm, "none", mo); settle(70); add(std::string("tile=") + mo + " x " + s + " x " + cm); settle(1); }
	add("sheet=05_composition_cross,4");
	// 6. switch/reset smoke test + allocation growth
	add("mem=before_smoke");
	for (auto mo : { "signal_columns", "phosphor_memory", "raster_excavation", "phosphor_portrait" })
		for (auto s : structs) { cfg(s, cmaps[(int)(f / 7) % 9], "none", mo); settle(3); }
	for (int k = 0; k < 3; k++) add("next_video"), settle(8), add("structure=round_matrix"), settle(2);
	add("reset"); settle(40); add("tile=AFTER RESET (compare with baseline)"); settle(1);
	add("mem=after_smoke"); add("sheet=06_after_reset,1");
	// 7. per-structure GPU pass timing (glFinish, desktop only)
	for (auto s : structs) { cfg(s, "source_rgb"); add("timing=1"); settle(60); add(std::string("perfs=") + s); add("timing=0"); settle(1); }
	for (auto b : { "adaptive_mosaic", "floating_phosphors" }) { cfg("round_matrix", "source_rgb", b); add("timing=1"); settle(60); add(std::string("perfs=round_matrix+") + b); add("timing=0"); settle(1); }
	add("reset"); add("exit");
}

void ofApp::keyPressed(int key) {
	switch (key) {
	case 'b': case 'B': bypass = !bypass; break;
	case 'l': case 'L': bloomOn = !bloomOn; break;
	case 'p': case 'P': persistOn = !persistOn; pipe.clearHistory(); break;
	case '1': applyPreset(0); break;
	case '2': applyPreset(1); break;
	case '3': applyPreset(2); break;
	case 'd': case 'D': debug = !debug; break;
	case 'r': case 'R': pipe.loadShaders(shaderDir); break;
	case 's': case 'S': savePng(P.name); break;
	case 'n': case 'N': openVideo(clipIndex + 1); break;
	case 'm': case 'M': openVideo(clipIndex - 1); break;
	case 'v': case 'V':
		if (!clips.empty()) { useVideo = !useVideo; if (useVideo && !video.isLoaded()) openVideo(0); pipe.clearHistory(); }
		break;
	case 'f': case 'F': coverFit = !coverFit; pipe.clearHistory(); break;
	case 'q': case 'Q': quality = 1 - quality; allocate(); break;
	case 't': case 'T': timing = !timing; break;
	case ']': setMode(crt::stepMode(mode, +1)); break;
	case '[': setMode(crt::stepMode(mode, -1)); break;
	case OF_KEY_F1: setMode(crt::Mode::PhosphorPortrait); break;
	case OF_KEY_F2: setMode(crt::Mode::RasterExcavation); break;
	case OF_KEY_F3: setMode(crt::Mode::PhosphorMemory); break;
	case OF_KEY_F4: setMode(crt::Mode::SignalColumns); break;
	case OF_KEY_F5: ofLogNotice("crt") << "contour_emission: planned, not implemented"; break;
	case OF_KEY_F6: ofLogNotice("crt") << "signal_collapse: planned, not implemented"; break;
	case 'c': case 'C': setPalette((paletteIdx + 1) % 3); if (paletteIdx != 0 && disp.cmap == crt::ColorMap::ClassicMono) setCmap(crt::ColorMap::ThemePrimarySecondary); break;
	case 'x': setStructure(crt::stepStructure(disp.structure, +1)); break;
	case 'X': setStructure(crt::stepStructure(disp.structure, -1)); break;
	case 'z': setCmap(crt::stepColorMap(disp.cmap, +1)); break;
	case 'Z': setCmap(crt::stepColorMap(disp.cmap, -1)); break;
	case 'a': setBehavior(crt::stepBehavior(disp.behavior, +1)); break;
	case 'A': setBehavior(crt::stepBehavior(disp.behavior, -1)); break;
	case 'g': case 'G': disp.dither = (disp.dither + 1) % 3; break;
	case 'y': case 'Y': disp.seed++; break;
	case '-': disp.settings.cellScale = std::max(0.5f, disp.settings.cellScale / 1.18f); break;
	case '=': disp.settings.cellScale = std::min(3.f, disp.settings.cellScale * 1.18f); break;
	case '\\': disp.settings.dense = !disp.settings.dense; break;
	case 'e': case 'E': disp.settings.floatEnergy = disp.settings.floatEnergy > 0.7f ? 0.35f : 0.95f; break;
	case '0': resetToReference(); break;
	case 'w': case 'W': showPanel = !showPanel; break;
	case 'o': case 'O': saveRecipe(); break;
	case 'i': case 'I': loadRecipe(); break;
	case ',': secInf = std::max(0.f, secInf - 0.05f); break;
	case '.': secInf = std::min(1.f, secInf + 0.05f); break;
	case 'h': case 'H': modeSettings.excavation.vertical = !modeSettings.excavation.vertical; break;
	case 'k': case 'K': case 'j': case 'J': {
		float f = (key == 'k' || key == 'K') ? 1.25f : 0.8f;
		if (mode == crt::Mode::RasterExcavation) modeSettings.excavation.scanSeconds *= f;
		else if (mode == crt::Mode::PhosphorMemory) modeSettings.memory.tau *= f;
		else if (mode == crt::Mode::SignalColumns) modeSettings.columns.groupPx = std::max(2.f, modeSettings.columns.groupPx * f);
		break; }
	}
}

void ofApp::windowResized(int, int) { allocate(); }

void ofApp::exit() { video.close(); }
