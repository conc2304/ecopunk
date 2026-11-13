#include "ofApp.h"
#include "../../../shared/src/video-playback/SharedMediaRoot.h"

#include <algorithm>
#include <cstdlib>
#include <sys/resource.h>
#include <mach/mach.h>

void ofApp::setup() {
	ofSetFrameRate(60);
	ofSetVerticalSync(true);
	ofDisableArbTex(); // the shared pipeline is GL_TEXTURE_2D regardless; keeps the video texture 2D too
	ofBackground(0);
	presets = phc::makePresets();
	if (const char* d = std::getenv("PHC_AUTOSHOT_DIR")) autoDir = d;

	ofLogNotice("phc") << "GL " << glGetString(GL_VERSION) << " / GLSL " << glGetString(GL_SHADING_LANGUAGE_VERSION)
					   << " / " << glGetString(GL_RENDERER);

	ofDirectory local(ofToDataPath("media", true));
	std::string dir = (local.exists() && local.listDir() > 0) ? ofToDataPath("media", true) : ecopunk::sharedMediaRoot();
	ofDirectory d(dir);
	d.allowExt("mp4"); d.allowExt("mov"); d.allowExt("m4v");
	if (d.exists()) { d.listDir(); d.sort(); for (auto& f : d.getFiles()) clips.push_back(f.getAbsolutePath()); }
	ofLogNotice("phc") << clips.size() << " clips in " << dir;

	int start = 0;
	if (const char* n = std::getenv("PHC_VIDEO")) {
		for (size_t i = 0; i < clips.size(); i++) if (clips[i].find(n) != std::string::npos) { start = (int)i; break; }
	}
	if (!clips.empty()) openVideo(start);
	else ofLogWarning("phc") << "no video available -> developer test pattern (standalone fallback only)";

	if (const char* e = std::getenv("PHC_QUALITY")) quality = std::string(e) == "pisafe" ? phc::Quality::PiSafe : phc::Quality::Desktop;
	applyPreset(std::getenv("PHC_PRESET") ? std::atoi(std::getenv("PHC_PRESET")) : 0);
	if (const char* e = std::getenv("PHC_SEED")) opt.seed = (uint32_t)std::atoi(e);
	if (const char* e = std::getenv("PHC_FIXED_DT")) fixedDt = (float)std::atof(e);
	if (const char* e = std::getenv("PHC_FREEZE")) freezeAt = (float)std::atof(e);
	if (!autoDir.empty() && fixedDt <= 0.f) fixedDt = 1.f / 60.f;
	if (const char* e = std::getenv("PHC_MATRIX")) matrixTag = e;
	bench = std::getenv("PHC_BENCH") != nullptr;
	if (const char* e = std::getenv("PHC_SEQ")) seqN = std::max(2, std::atoi(e));
	pipe.loadShaders();
	allocate();
}

void ofApp::allocate() {
	pipe.allocate(ofGetWidth(), ofGetHeight(), quality);
	ofLogNotice("phc") << "FBOs: " << pipe.width() << "x" << pipe.height() << " quality "
					   << (quality == phc::Quality::Desktop ? "Desktop" : "PiSafe") << ", ~"
					   << ofToString(pipe.gpuBytes() / 1048576.0, 2) << " MiB GPU, " << pipe.status();
}

void ofApp::buildKnobs() {
	knobs = {
		// source structure
		{ "structBlur", &P.structBlur, 1.f, 0.f, 48.f }, { "tonalLo", &P.tonalLo, 0.02f, 0.f, 0.9f }, { "tonalHi", &P.tonalHi, 0.02f, 0.1f, 1.f },
		{ "contrast", &P.contrast, 0.05f, 0.25f, 4.f }, { "gamma", &P.gamma, 0.05f, 0.3f, 3.f }, { "detail", &P.detail, 0.02f, 0.f, 1.f },
		// isophotes
		{ "levels", &P.levels, 1.f, 2.f, 80.f }, { "lineWidth", &P.lineWidth, 0.1f, 0.5f, 12.f }, { "softness", &P.softness, 0.05f, 0.25f, 3.f },
		{ "gradEmph", &P.gradientEmphasis, 0.05f, 0.f, 1.f }, { "lumaFloor", &P.lumaFloor, 0.02f, 0.f, 1.f },
		// relief
		{ "reliefLines", &P.reliefLines, 2.f, 4.f, 200.f }, { "reliefAmp", &P.reliefAmp, 2.f, -200.f, 200.f }, { "reliefPivot", &P.reliefPivot, 0.02f, 0.f, 1.f },
		{ "reliefWidth", &P.reliefWidth, 0.1f, 0.5f, 12.f }, { "reliefDrift", &P.reliefDrift, 1.f, -60.f, 60.f },
		// hybrid
		{ "hybridIso", &P.hybridIso, 0.05f, 0.f, 1.f },
		// phosphor
		{ "cellW", &P.cellW, 0.5f, 2.f, 16.f }, { "cellH", &P.cellH, 0.5f, 2.f, 12.f }, { "cellSnap", &P.cellSnap, 0.05f, 0.f, 1.f },
		{ "dotRadius", &P.dotRadius, 0.02f, 0.2f, 0.7f }, { "dropout", &P.dropout, 0.02f, 0.f, 0.8f },
		{ "warpX", &P.warpX, 1.f, 0.f, 60.f }, { "warpY", &P.warpY, 0.5f, 0.f, 20.f },
		{ "rowAmp", &P.rowAmp, 1.f, 0.f, 60.f }, { "slip", &P.slip, 2.f, 0.f, 100.f },
		{ "persistTau", &P.persistenceTau, 0.02f, 0.f, 0.8f },
		{ "bloom", &P.bloomAmount, 0.05f, 0.f, 3.f }, { "glowRadius", &P.bloomRadius, 0.1f, 0.5f, 6.f },
		{ "grain", &P.grain, 0.01f, 0.f, 0.3f }, { "gain", &P.gain, 0.1f, 0.2f, 4.f },
		{ "tintR", &P.tint.x, 0.02f, 0.f, 1.f }, { "tintG", &P.tint.y, 0.02f, 0.f, 1.f }, { "tintB", &P.tint.z, 0.02f, 0.f, 1.f },
	};
	knob = std::min(knob, (int)knobs.size() - 1);
}

void ofApp::applyPreset(int i) {
	presetIdx = ((i % phc::kPresetCount) + phc::kPresetCount) % phc::kPresetCount;
	P = presets[presetIdx];
	buildKnobs();
	if (!std::getenv("PHC_SET") && !std::getenv("PHC_ALGO")) { /* preset values as authored */ }
	if (const char* e = std::getenv("PHC_SET")) { // "levels=20,blur=6": developer/QA overrides applied after every preset load
		for (const std::string& kv : ofSplitString(e, ",", true, true)) {
			const auto eq = ofSplitString(kv, "=");
			if (eq.size() != 2) continue;
			for (auto& k : knobs) if (eq[0] == k.name) *k.v = (float)std::atof(eq[1].c_str());
			if (eq[0] == "toneFill") P.toneFill = (float)std::atof(eq[1].c_str());
		}
	}
	Pbase = P;
	if (pipe.allocated()) pipe.clearHistory();
	ofLogNotice("phc") << "preset " << P.name << " seed " << opt.seed;
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
		lastPos = 0; frozen = false;
		ofLogNotice("phc") << "video: " << clips[clipIndex];
	} else {
		useVideo = false;
		ofLogError("phc") << "video load failed, using test pattern: " << clips[clipIndex];
	}
	if (pipe.allocated()) pipe.clearHistory(); // no stale previous-video residue
}

void ofApp::drawPattern(float w, float h) {
	// Developer-only diagnostic: a recognisable synthetic face-ish portrait. Never used when video works.
	ofSetColor(20); ofDrawRectangle(0, 0, w, h);
	for (int i = 14; i > 0; i--) { ofSetColor(255 * (1.f - i / 16.f) + 20); ofDrawEllipse(w * 0.5f, h * 0.5f, w * 0.34f * i / 14.f, h * 0.8f * i / 14.f); }
	ofSetColor(15); ofDrawEllipse(w * 0.44f, h * 0.42f, 40, 20); ofDrawEllipse(w * 0.56f, h * 0.42f, 40, 20);
	ofSetColor(255); ofDrawBitmapString("NO VIDEO - DEV PATTERN", 20, 20);
}

void ofApp::renderSource() {
	const int W = pipe.width(), H = pipe.height();
	pipe.sourceFbo().begin();
	ofDisableAlphaBlending();
	ofSetColor(0); ofDrawRectangle(0, 0, W, H);
	if (useVideo && !awaitingFrame && video.isLoaded() && video.getTexture().isAllocated()) {
		ofSetColor(255);
		const float vw = video.getWidth(), vh = video.getHeight();
		const float s = std::max(W / vw, H / vh); // cover-fit
		video.draw((W - vw * s) * 0.5f, (H - vh * s) * 0.5f, vw * s, vh * s);
	} else if (!useVideo) {
		drawPattern(W, H);
	}
	pipe.sourceFbo().end();
}

void ofApp::update() {
	simTime += fixedDt > 0.f ? fixedDt : ofGetLastFrameTime();
	if (useVideo) {
		if (seqN > 0 && autoFrame >= 60 && video.getDuration() > 0.f) { // frame-accurate stepping, independent of wall clock
			if (!video.isPaused()) video.setPaused(true);
			const float start = freezeAt >= 0.f ? freezeAt : 0.f;
			video.setPosition(std::min(0.995f, start + (float)seqIdx / 30.f / video.getDuration()));
		} else if (freezeAt >= 0.f && !frozen && ofGetFrameNum() > 10 && !awaitingFrame) {
			video.setPosition(freezeAt); video.setPaused(true); frozen = true;
		}
		video.update();
		if (awaitingFrame && video.isFrameNew()) { awaitingFrame = false; pipe.clearHistory(); }
		const float pos = video.getPosition();
		if (pos + 0.25f < lastPos) pipe.clearHistory(); // loop discontinuity
		lastPos = pos;
	}
}

void ofApp::shot(const std::string& name, ofFbo& fbo) {
	ofPixels px;
	fbo.readToPixels(px);
	ofDirectory::createDirectory(autoDir.empty() ? ofToDataPath("captures", true) : autoDir, true, true);
	const std::string path = (autoDir.empty() ? ofToDataPath("captures", true) : autoDir) + "/" + name + ".png";
	ofSaveImage(px, path);
	ofLogNotice("phc") << "saved " << path;
}

void ofApp::draw() {
	pipe.setPassTiming(timing || !autoDir.empty());
	renderSource();
	const float dt = fixedDt > 0.f ? fixedDt : ofGetLastFrameTime();
	pipe.process(P, simTime, dt, opt);
	ofSetColor(255);
	ofFbo& f = pipe.view(viewMode);
	(pipe.usable() ? f : pipe.sourceFbo()).draw(0, 0, ofGetWidth(), ofGetHeight()); // Source view == sourceFbo
	if (showPanel && autoDir.empty()) {
		ofSetColor(0, 160); ofDrawRectangle(8, 8, 520, 14 * (float)(knobs.size() + 6) + 8);
		ofSetColor(230, 255, 120);
		float y = 22;
		auto line = [&](const std::string& s) { ofDrawBitmapString(s, 14, y); y += 14; };
		line(std::string(P.name) + " / " + phc::algorithmName(P.algorithm) + " (A)  seed " + ofToString(opt.seed) + "  " + (quality == phc::Quality::Desktop ? "Desktop" : "PiSafe"));
		line("view " + std::to_string((int)viewMode) + " (V)  fps " + ofToString(ofGetFrameRate(), 1) + (timing ? "  gpu " + ofToString(pipe.totalMs(), 2) + "ms" : ""));
		line(std::string("persist(P) ") + (opt.persistence ? "on" : "off") + " glow(G) " + (opt.glow ? "on" : "off") + " dropout(D) " + (opt.dropout ? "on" : "off")
		 + " slip(L) " + (opt.slip ? "on" : "off") + " grain(K) " + (opt.grain ? "on" : "off") + "  " + (useVideo ? "video" : "pattern"));
		line(pipe.usable() ? "shaders ok  range(X) " + std::string(P.autoRange ? "auto " : "manual ") + ofToString(pipe.rangeLo(), 2) + ".." + ofToString(pipe.rangeHi(), 2) : "SHADERS FAILED: " + pipe.status());
		for (size_t i = 0; i < knobs.size(); i++) line(std::string(i == (size_t)knob ? "> " : "  ") + knobs[i].name + " " + ofToString(*knobs[i].v, 3));
	}
	if (!autoDir.empty()) autoshotStep();
}

void ofApp::grab(const std::string& label, ofFbo& fbo) {
	ofPixels px;
	fbo.readToPixels(px);
	ofImage img;
	img.setFromPixels(px);
	mTiles.push_back(img);
	mLabels.push_back(label);
	shot(matrixTag + "_" + label, fbo);
}

void ofApp::matrixStep() {
	autoFrame++;
	auto bare = [&](int a) {
		P = Pbase; P.algorithm = (phc::Algorithm)a;
		opt.persistence = opt.glow = opt.dropout = opt.slip = opt.grain = false;
		pipe.clearHistory();
		autoFrame = 0;
	};
	auto finished = [&](int a) {
		P = Pbase; P.algorithm = (phc::Algorithm)a;
		opt.persistence = opt.glow = opt.dropout = opt.slip = opt.grain = true;
		pipe.clearHistory();
		autoFrame = 0;
	};
	if (mStage == 0 && autoFrame >= 90) { // clip frozen and settled
		mStage = 1; mAlgo = 0; bare(0);
	} else if (mStage == 1 && autoFrame >= 3) {
		if (mAlgo == 0) { grab("source", pipe.sourceFbo()); grab("structural_luma", pipe.monoView(1)); }
		grab(std::string("bare_") + phc::algorithmName((phc::Algorithm)mAlgo), pipe.monoView(0));
		if (++mAlgo < phc::kAlgorithmCount) bare(mAlgo); else { mStage = 2; mAlgo = 0; finished(0); }
	} else if (mStage == 2 && autoFrame >= 60) {
		grab(std::string("finished_") + phc::algorithmName((phc::Algorithm)mAlgo), pipe.output());
		if (++mAlgo < phc::kAlgorithmCount) finished(mAlgo); else { mStage = 3; finished((int)Pbase.algorithm); }
	} else if (mStage == 3 && autoFrame >= 60) {
		grab(std::string("preferred_") + Pbase.name, pipe.output());
		// contact sheet: row0 source | structural luma | preferred ; row1 bare x3 ; row2 finished x3
		const int tw = 640, th = 360;
		ofFbo sheet;
		sheet.allocate(tw * 3, th * 3, GL_RGBA);
		sheet.begin();
		ofClear(0, 0, 0, 255);
		auto put = [&](int idx, int cx, int cy) {
			ofSetColor(255);
			mTiles[idx].draw(cx * tw, cy * th, tw, th);
			ofSetColor(0, 200); ofDrawRectangle(cx * tw, cy * th, 8.f * (float)mLabels[idx].size() + 12.f, 16);
			ofSetColor(230, 255, 120); ofDrawBitmapString(mLabels[idx], cx * tw + 6, cy * th + 12);
		};
		// tile indices: 0 source, 1 luma, 2..4 bare, 5..7 finished, 8 preferred
		put(0, 0, 0); put(1, 1, 0); put(8, 2, 0);
		for (int i = 0; i < 3; i++) { put(2 + i, i, 1); put(5 + i, i, 2); }
		sheet.end();
		ofPixels px; sheet.readToPixels(px);
		ofSaveImage(px, autoDir + "/" + matrixTag + "_contact_sheet.png");
		ofLogNotice("phc") << "matrix done: " << matrixTag << "  GPU ms " << pipe.totalMs() << " (" << phc::algorithmName(Pbase.algorithm) << " finished)";
		std::exit(0);
	}
}

// ms/frame (glFinish-bracketed pass cost incl. the occasional range readback) for every algorithm x quality x
// auto-range setting, plus GPU target bytes and process memory. Desktop GPU numbers only.
void ofApp::benchStep() {
	static const phc::Quality qs[2] = { phc::Quality::Desktop, phc::Quality::PiSafe };
	const int combos = 2 * phc::kAlgorithmCount * 2; // quality x algorithm x autoRange
	autoFrame++;
	auto cfg = [&](int i) {
		const int q = i / (phc::kAlgorithmCount * 2), a = (i / 2) % phc::kAlgorithmCount, ar = i % 2;
		if (qs[q] != quality) { quality = qs[q]; allocate(); }
		P = Pbase; P.algorithm = (phc::Algorithm)a; P.autoRange = ar == 0;
		opt.persistence = opt.glow = opt.dropout = opt.slip = opt.grain = true;
		pipe.clearHistory();
	};
	if (bStage == 0 && autoFrame >= 60) { cfg(0); autoFrame = 0; bStage = 1; return; }
	if (bStage >= 1 && autoFrame >= 240) {
		const int i = bStage - 1;
		mach_task_basic_info info; mach_msg_type_number_t cnt = MACH_TASK_BASIC_INFO_COUNT;
		task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &cnt);
		ofLogNotice("phc-bench") << (quality == phc::Quality::Desktop ? "Desktop" : "PiSafe") << " " << phc::algorithmName(P.algorithm)
			<< " autoRange=" << (P.autoRange ? "on " : "off") << " " << pipe.width() << "x" << pipe.height()
			<< "  pass-ms " << ofToString(pipe.totalMs(), 2) << "  gpu-targets " << ofToString(pipe.gpuBytes() / 1048576.0, 2)
			<< " MiB  rss " << ofToString(info.resident_size / 1048576.0, 1) << " MiB";
		if (i + 1 >= combos) { ofLogNotice("phc-bench") << "done"; std::exit(0); }
		cfg(i + 1); autoFrame = 0; bStage++;
	}
}

void ofApp::seqStep() {
	autoFrame++;
	if (autoFrame < 60) return; // settle: shaders warm, range measured
	ofPixels o, s;
	pipe.output().readToPixels(o);
	pipe.sourceFbo().readToPixels(s);
	const size_t n = o.size();
	std::vector<unsigned char> curO(o.getData(), o.getData() + n), curS(s.getData(), s.getData() + s.size());
	if (!seqPrevOut.empty()) {
		double dO = 0, dS = 0; size_t big = 0;
		for (size_t i = 0; i < n; i++) { const int d = std::abs((int)curO[i] - (int)seqPrevOut[i]); dO += d; big += d > 64; }
		for (size_t i = 0; i < curS.size(); i++) dS += std::abs((int)curS[i] - (int)seqPrevSrc[i]);
		dO /= double(n); dS /= double(curS.size());
		seqSumOut += dO; seqSumSrc += dS; seqMaxOut = std::max(seqMaxOut, dO); seqFracBig += double(big) / double(n);
	}
	seqPrevOut.swap(curO); seqPrevSrc.swap(curS);
	ofSaveImage(o, autoDir + "/seq_" + ofToString(seqIdx, 3, '0') + ".png");
	if (++seqIdx >= seqN) {
		const double k = double(seqN - 1);
		ofLogNotice("phc-seq") << seqN << " frames @ 1/30 s: mean|d| effect " << ofToString(seqSumOut / k, 2) << "/255 (max " << ofToString(seqMaxOut, 2)
			<< ")  source " << ofToString(seqSumSrc / k, 2) << "/255  ratio " << ofToString((seqSumOut / k) / std::max(1e-6, seqSumSrc / k), 2)
			<< "  px with |d|>64: " << ofToString(100.0 * seqFracBig / k, 2) << "%";
		std::exit(0);
	}
}

void ofApp::autoshotStep() {
	if (seqN > 0) { seqStep(); return; }
	if (bench) { benchStep(); return; }
	if (!matrixTag.empty()) { matrixStep(); return; }
	autoFrame++;
	if (autoStage == 0 && autoFrame >= 90) {
		autoMs.push_back(pipe.totalMs());
		shot("1_source", pipe.sourceFbo());
		shot("2_luma", pipe.view(phc::View::Luma));
		shot("3_contour", pipe.view(phc::View::Contour));
		shot("4_raster", pipe.view(phc::View::Raster));
		shot(std::string("5_final_") + P.name, pipe.output());
		ofLogNotice("phc") << "GPU ms/frame (glFinish, " << pipe.width() << "x" << pipe.height() << "): " << pipe.totalMs();
		autoStage = 1; autoFrame = 0;
		applyPreset(1);
	} else if (autoStage == 1 && autoFrame >= 60) {
		shot("5_final_SparseSignal", pipe.output());
		applyPreset(2); autoStage = 2; autoFrame = 0;
	} else if (autoStage == 2 && autoFrame >= 60) {
		shot("5_final_MeltedRaster", pipe.output());
		applyPreset(0);
		if (std::getenv("PHC_STABILITY")) { autoStage = 4; autoFrame = 0; return; } // measure on the same frozen clip
		if (clips.size() > 1) { freezeAt = -1.f; openVideo(clipIndex + std::max(1, (int)clips.size() / 3)); }
		autoStage = 3; autoFrame = 0;
	} else if (autoStage == 3 && autoFrame >= 120) {
		shot("6_other_source", pipe.sourceFbo());
		shot("6_other_final", pipe.output());
		if (std::getenv("PHC_STABILITY")) { autoStage = 4; autoFrame = 0; return; }
		ofLogNotice("phc") << "autoshot done";
		std::exit(0);
	} else if (autoStage == 4) { // consecutive frames (fixed dt) for temporal-stability measurement
		shot("stab_" + ofToString(autoFrame, 2, '0') + "_final", pipe.output());
		shot("stab_" + ofToString(autoFrame, 2, '0') + "_source", pipe.sourceFbo());
		if (autoFrame >= 8) { ofLogNotice("phc") << "autoshot done"; std::exit(0); }
	}
}

void ofApp::keyPressed(int key) {
	switch (key) {
	case '1': case '2': case '3': case '4': applyPreset(key - '1'); break;
	case '0': opt.seed = 1; applyPreset(0); break;
	case 'v': case 'V': viewMode = (phc::View)(((int)viewMode + 1) % 5); break;
	case 'r': case 'R': opt.seed++; pipe.clearHistory(); ofLogNotice("phc") << "seed " << opt.seed; break;
	case 'q': case 'Q': quality = quality == phc::Quality::Desktop ? phc::Quality::PiSafe : phc::Quality::Desktop; allocate(); break;
	case 'p': case 'P': opt.persistence = !opt.persistence; pipe.clearHistory(); break;
	case 'g': case 'G': opt.glow = !opt.glow; break;
	case 'a': case 'A': P.algorithm = (phc::Algorithm)(((int)P.algorithm + 1) % phc::kAlgorithmCount); pipe.clearHistory(); break;
	case 'd': case 'D': opt.dropout = !opt.dropout; break;
	case 'l': case 'L': opt.slip = !opt.slip; break;
	case 'k': case 'K': opt.grain = !opt.grain; break;
	case 'x': case 'X': P.autoRange = !P.autoRange; pipe.clearHistory(); break;
	case 'b': case 'B': // bare structure: every finishing stage off (toggle back with the individual keys)
		opt.persistence = opt.glow = opt.dropout = opt.slip = opt.grain = false; pipe.clearHistory(); break;
	case 't': case 'T': timing = !timing; break;
	case 'h': case 'H': showPanel = !showPanel; break;
	case 'n': case 'N': openVideo(clipIndex + 1); break;
	case 'm': case 'M': openVideo(clipIndex - 1); break;
	case 's': case 'S': shot("manual_" + ofGetTimestampString(), pipe.view(viewMode)); break;
	case OF_KEY_UP: knob = (knob + (int)knobs.size() - 1) % (int)knobs.size(); break;
	case OF_KEY_DOWN: knob = (knob + 1) % (int)knobs.size(); break;
	case OF_KEY_LEFT: case OF_KEY_RIGHT: {
		Knob& k = knobs[knob];
		*k.v = std::min(k.hi, std::max(k.lo, *k.v + (key == OF_KEY_RIGHT ? k.step : -k.step)));
		phc::clampParams(P);
		break;
	}
	default: break;
	}
}

void ofApp::windowResized(int w, int h) {
	if (w < 8 || h < 8) return;
	allocate(); // reallocates and clears history; never stale at the old size
}

void ofApp::exit() {
	video.close();
	pipe.release();
}
