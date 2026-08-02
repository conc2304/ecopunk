#include "GalleryCompositor.h"

const std::array<ofColor, 6> GalleryCompositor::tintPalette = {
	ofColor(255, 120, 120), // red
	ofColor(120, 200, 255), // blue
	ofColor(140, 255, 140), // green
	ofColor(255, 220, 120), // amber
	ofColor(220, 140, 255), // violet
	ofColor(120, 255, 230), // teal
};

void GalleryCompositor::setup(int canvasW_, int canvasH_) {
	canvasW = canvasW_;
	canvasH = canvasH_;

	modeNames = {
		"PASSTHROUGH (BASELINE)",
		"DUOTONE DECAY",
		"PER-EMITTER HUE TINT",
		"POSTERIZE",
		"SCANLINE OVERLAY",
		"PIXELATION-AS-AGING",
		"LEADING-EDGE CHROMATIC SPLIT",
		"TELEMETRY GHOSTING",
		"RADIAL LENS DISTORTION",
		"BOOT-UP STATIC BURST",
		"HEATMAP RECOLOR",
	};

	const std::array<std::string, NUM_MODES> fragFiles = {
		"gallery_mode0_passthrough.frag",
		"gallery_mode1_duotone.frag",
		"gallery_mode2_emitterhue.frag",
		"gallery_mode3_posterize.frag",
		"gallery_mode4_scanlines.frag",
		"gallery_mode5_pixelate.frag",
		"gallery_mode6_chromasplit.frag",
		"gallery_mode7_telemetry.frag",
		"gallery_mode8_lensdistort.frag",
		"gallery_mode9_bootstatic.frag",
		"gallery_mode10_heatmaprecolor.frag",
	};

	for (int i = 0; i < NUM_MODES; ++i) {
		bool loaded = shaders[i].load("shaders/gallery_passthrough.vert", "shaders/" + fragFiles[i]);
		if (!loaded) {
			ofLogError("GalleryCompositor") << "failed to load " << fragFiles[i];
		}
	}

	codeFont.load("fonts/LiberationMono-Regular.ttf", 16);

	ofBuffer buffer = ofBufferFromFile("codefragments.txt");
	for (auto & line : buffer.getLines()) {
		if (!line.empty()) codeFragments.push_back(line);
	}
}

ofColor GalleryCompositor::getEmitterTint(int emitterId) {
	auto it = emitterTints.find(emitterId);
	if (it != emitterTints.end()) return it->second;
	ofColor tint = tintPalette[emitterId % tintPalette.size()];
	emitterTints[emitterId] = tint;
	return tint;
}

void GalleryCompositor::update(float dt, const std::vector<GalleryPulseInfo> & pulses) {
	// Mode 7 ghost-text lifecycle runs regardless of the active mode, so
	// switching into Mode 7 doesn't start from an empty screen every time.
	mode7SpawnTimer -= dt;
	if (mode7SpawnTimer <= 0.0f && !pulses.empty() && !codeFragments.empty()) {
		const auto & p = pulses[static_cast<size_t>(ofRandom(pulses.size()))];
		GhostText gt;
		gt.pos = { p.x, p.y };
		gt.text = codeFragments[static_cast<size_t>(ofRandom(codeFragments.size()))];
		gt.opacity = 1.0f;
		ghostTexts.push_back(gt);
		mode7SpawnTimer = ofRandom(1.5f, 3.0f);
	}

	std::vector<GhostText> alive;
	alive.reserve(ghostTexts.size());
	for (auto & gt : ghostTexts) {
		gt.opacity -= dt * 0.35f;
		if (gt.opacity > 0.0f) alive.push_back(gt);
	}
	ghostTexts = std::move(alive);
}

void GalleryCompositor::applyModeUniforms(ofShader & shader, const std::vector<GalleryPulseInfo> & pulses) {
	ofVec2f resolution(static_cast<float>(canvasW), static_cast<float>(canvasH));

	switch (modeIndex) {
		case 5: // pixelate
			shader.setUniform2f("resolution", resolution.x, resolution.y);
			break;
		case 6: // chromatic split
			shader.setUniform2f("resolution", resolution.x, resolution.y);
			shader.setUniform1f("splitAmount", 0.01f);
			break;
		case 8: // lens distortion
			shader.setUniform2f("resolution", resolution.x, resolution.y);
			shader.setUniform1f("distortAmount", 0.04f);
			break;
		case 9: { // boot-up static burst
			std::vector<ofVec2f> centers;
			std::vector<float> radii;
			std::vector<float> arrivingT;
			for (const auto & p : pulses) {
				if (static_cast<int>(centers.size()) >= maxArriving) break;
				if (p.ageMs >= arrivalWindowMs) continue;
				centers.push_back(ofVec2f(p.x / resolution.x, p.y / resolution.y));
				radii.push_back(std::max(0.02f, p.radius / resolution.x));
				arrivingT.push_back(ofClamp(p.ageMs / arrivalWindowMs, 0.0f, 1.0f));
			}
			shader.setUniform1i("arrivingCount", static_cast<int>(centers.size()));
			for (size_t i = 0; i < centers.size(); ++i) {
				std::string idx = ofToString(i);
				shader.setUniform2f("arrivingCenters[" + idx + "]", centers[i].x, centers[i].y);
				shader.setUniform1f("arrivingRadii[" + idx + "]", radii[i]);
				shader.setUniform1f("arrivingT[" + idx + "]", arrivingT[i]);
			}
			break;
		}
		case 10: // heatmap recolor
			shader.setUniform1f("alpha", 1.0f);
			shader.setUniform1f("intensity", 1.0f);
			shader.setUniform1f("gamma", 0.9f);
			shader.setUniform1f("minLuminance", 0.05f);
			shader.setUniform1f("maxLuminance", 0.95f);
			shader.setUniform1i("palette", 2); // Solarpunk Botanical — matches this sketch's radar-garden theming
			shader.setUniform1i("reverse", 0);
			break;
		default:
			break;
	}
}

void GalleryCompositor::draw(ofTexture & videoTex, ofTexture & maskTex, const std::vector<GalleryPulseInfo> & pulses) {
	ofShader & shader = shaders[modeIndex];

	ofSetColor(255);
	shader.begin();
	shader.setUniformTexture("videoTex", videoTex, 0);
	shader.setUniformTexture("maskTex", maskTex, 1);
	applyModeUniforms(shader, pulses);
	videoTex.draw(0, 0, static_cast<float>(canvasW), static_cast<float>(canvasH));
	shader.end();

	if (modeIndex == 7) drawMode7Text();
}

void GalleryCompositor::drawMode7Text() {
	ofPushStyle();
	ofEnableAlphaBlending();
	for (const auto & gt : ghostTexts) {
		ofSetColor(140, 255, 200, static_cast<int>(gt.opacity * 220.0f));
		codeFont.drawString(gt.text, gt.pos.x, gt.pos.y);
	}
	ofDisableAlphaBlending();
	ofPopStyle();
}
