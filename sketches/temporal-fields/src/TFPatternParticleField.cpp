#include "TFPatternParticleField.h"
#include "TFRandom.h"
#include "TFTextureCropFill.h"
#include "ofGraphics.h"
#include "ofLog.h"
#include "ofMath.h"
#include "ofShader.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
	constexpr float NOISE_SCALE = 2.2f;
	// ~3x slower for the "airy and calm" pass (was 0.05).
	constexpr float NOISE_TIME_SPEED = 0.0167f;

	// Same per-pixel hashed-noise idea as TFFragmentTransition's dissolve
	// shader (fragmentDissolve.frag), but a single-texture existence fade
	// rather than a two-texture value crossfade: `progress` ramps 0->1 at
	// spawn and 1->0 at death using the SAME hash mask both times, which is
	// what makes it "one shared random blob layout" per the brief's Section
	// 6 — reveal and reverse-reveal share the same per-pixel thresholds.
	ofShader& existenceFadeShader() {
		static ofShader shader;
		static bool loaded = false;
		if (!loaded) {
			loaded = true;
			if (!shader.load("shaders/particleExistenceFade.vert", "shaders/particleExistenceFade.frag")) {
				ofLogError("TFPatternParticleField") << "failed to load particleExistenceFade shader";
			}
		}
		return shader;
	}
}

void TFPatternParticleField::setup(TimeOffsetVideoBuffer* videoBuffer_, int canvasW_, int canvasH_, const Params& params_) {
	videoBuffer = videoBuffer_;
	canvasW = canvasW_;
	canvasH = canvasH_;
	params = params_;
}

void TFPatternParticleField::resizeCanvas(int canvasW_, int canvasH_) {
	canvasW = canvasW_;
	canvasH = canvasH_;
}

void TFPatternParticleField::reset(int seed) {
	std::srand(seed);
	noiseTime = tfRandRangeF(0.0f, 1000.0f);
	spawnTimer = 0.0f;
	// No masking/hatch/wholesale-regenerate concept for this pattern — a
	// "reset" is just clearing every live particle and letting the spawn
	// system repopulate naturally (Section 6).
	particles.clear();
}

void TFPatternParticleField::update(float dt) {
	noiseTime += dt * NOISE_TIME_SPEED;

	int maxCount = std::max(0, params.maxParticleCount);
	float spawnInterval = 1.0f / std::max(0.01f, params.spawnRate);

	spawnTimer += dt;
	while (spawnTimer >= spawnInterval && static_cast<int>(particles.size()) < maxCount) {
		spawnTimer -= spawnInterval;
		spawnParticle();
	}
	if (static_cast<int>(particles.size()) >= maxCount) {
		// At the cap — don't let the timer build up unboundedly while
		// waiting for room, so spawning resumes promptly once a particle dies.
		spawnTimer = std::min(spawnTimer, spawnInterval);
	}

	for (auto& p : particles) {
		p.age += dt;
		p.pos += p.velocity * dt;
	}

	particles.erase(
		std::remove_if(particles.begin(), particles.end(), [](const Particle& p) { return p.age >= p.lifespan; }),
		particles.end());
}

void TFPatternParticleField::spawnParticle() {
	Particle p;

	float shorterEdge = static_cast<float>(std::min(canvasW, canvasH));
	p.size = tfRandRangeF(params.minSize, params.maxSize) * shorterEdge;

	// Margin so particles can spawn partially off-canvas, not just fully inside it.
	p.pos = ofVec2f(
		tfRandRangeF(-p.size * 0.5f, canvasW + p.size * 0.5f),
		tfRandRangeF(-p.size * 0.5f, canvasH + p.size * 0.5f));

	p.lifespan = std::max(0.1f, tfRandRangeF(params.minLife, params.maxLife));
	p.age = 0.0f;

	float baseAngle = 0.0f;
	float spread = TWO_PI;
	if (params.driftDirection == DriftDirection::UPWARD) {
		baseAngle = -HALF_PI;
		spread = PI * 0.5f;
	} else if (params.driftDirection == DriftDirection::DOWNWARD) {
		baseAngle = HALF_PI;
		spread = PI * 0.5f;
	}
	float angle = (params.driftDirection == DriftDirection::OMNIDIRECTIONAL) ? tfRandRangeF(0.0f, TWO_PI)
																			  : baseAngle + tfRandRangeF(-spread * 0.5f, spread * 0.5f);
	float speed = params.driftSpeed * tfRandRangeF(0.5f, 1.5f) * shorterEdge * 0.05f;
	p.velocity = ofVec2f(std::cos(angle), std::sin(angle)) * speed;

	p.style = tfPickTransitionStyle(params.hardCutWeight, params.crossfadeWeight, params.erosionWeight);

	// Picks (never jumps) a playhead — Particle Field shares the pool
	// read-only rather than mutating it, since a fixed-at-spawn offset with
	// no mid-life reassignment doesn't fit the "which fragments currently
	// want which offset" whole-list model tfAssignPlayheadsByNoise() and
	// friends are built around (see the class comment in the header).
	int numPlayheads = videoBuffer->getNumPlayheads();
	if (numPlayheads > 0) {
		float gray = ofNoise(p.pos.x / canvasW * NOISE_SCALE, p.pos.y / canvasH * NOISE_SCALE, noiseTime);
		float desiredOffset = videoBuffer->quantize(gray);

		int chosen = 0;
		float bestDist = std::abs(videoBuffer->getPlayheadOffset(0) - desiredOffset);
		for (int i = 1; i < numPlayheads; i++) {
			float d = std::abs(videoBuffer->getPlayheadOffset(i) - desiredOffset);
			if (d < bestDist) {
				bestDist = d;
				chosen = i;
			}
		}
		p.playheadIndex = chosen;
	}

	if (onFragmentReassignedCb) {
		// A spawn is this pattern's equivalent of the per-fragment
		// reassignment tick every other pattern's ripple/HUD cadence keys
		// off (Section 7) — there's no "reassignment" here, so a spawn
		// event stands in for it.
		onFragmentReassignedCb(p.pos.x / canvasW, p.pos.y / canvasH);
	}

	particles.push_back(p);
}

float TFPatternParticleField::existenceAlpha(const Particle& p) const {
	if (p.style == TFFragmentTransition::Style::HARD_CUT) {
		return 1.0f; // pops fully in/out instantly, no animation
	}

	float duration = std::max(0.01f, params.transitionDuration);
	float alphaIn = ofClamp(p.age / duration, 0.0f, 1.0f);
	float alphaOut = ofClamp((p.lifespan - p.age) / duration, 0.0f, 1.0f);
	return std::min(alphaIn, alphaOut);
}

float TFPatternParticleField::existenceProgress(const Particle& p) const {
	float duration = std::max(0.01f, params.transitionDuration);
	if (p.age < duration) {
		return ofClamp(p.age / duration, 0.0f, 1.0f); // growing in
	}
	float remaining = p.lifespan - p.age;
	if (remaining < duration) {
		return ofClamp(remaining / duration, 0.0f, 1.0f); // shrinking out
	}
	return 1.0f; // fully revealed, steady state
}

void TFPatternParticleField::draw() {
	std::vector<const Particle*> order;
	order.reserve(particles.size());
	for (const auto& p : particles) {
		order.push_back(&p);
	}

	if (params.depthOrder == DepthOrder::NEWEST_ON_TOP) {
		// Oldest (largest age) drawn first, newest (smallest age) drawn
		// last so it composites on top.
		std::sort(order.begin(), order.end(), [](const Particle* a, const Particle* b) { return a->age > b->age; });
	} else {
		// LARGEST_BEHIND: biggest drawn first (behind), smallest drawn last
		// (in front) — a cheap parallax read, not real depth.
		std::sort(order.begin(), order.end(), [](const Particle* a, const Particle* b) { return a->size > b->size; });
	}

	ofEnableAlphaBlending();
	for (const Particle* p : order) {
		drawParticle(*p);
	}
}

void TFPatternParticleField::drawParticle(const Particle& p) const {
	if (p.playheadIndex < 0) {
		return;
	}
	const ofTexture& tex = videoBuffer->getPlayheadTexture(p.playheadIndex);
	if (!tex.isAllocated()) {
		return;
	}

	ofRectangle localRect(-p.size * 0.5f, -p.size * 0.5f, p.size, p.size);
	// "background-size: cover" against the WHOLE shared video texture, not
	// a canvas-proportional crop — particles are free-floating and don't
	// tile to reassemble one intact image the way every other pattern's
	// fragments do.
	ofRectangle srcRect = tfComputeCropFillSrcRect(tex.getWidth(), tex.getHeight(), localRect);

	ofPushMatrix();
	ofTranslate(p.pos.x, p.pos.y);

	if (p.style == TFFragmentTransition::Style::EROSION) {
		ofShader& shader = existenceFadeShader();
		ofSetColor(255);
		shader.begin();
		shader.setUniformTexture("tex", tex, 0);
		shader.setUniform1f("progress", existenceProgress(p));
		tex.drawSubsection(localRect.x, localRect.y, localRect.width, localRect.height,
			srcRect.x, srcRect.y, srcRect.width, srcRect.height);
		shader.end();
	} else {
		float alpha = existenceAlpha(p);
		ofSetColor(255, 255, 255, static_cast<int>(ofClamp(alpha, 0.0f, 1.0f) * 255));
		tex.drawSubsection(localRect.x, localRect.y, localRect.width, localRect.height,
			srcRect.x, srcRect.y, srcRect.width, srcRect.height);
		ofSetColor(255);
	}

	ofPopMatrix();
}

std::vector<ofVec2f> TFPatternParticleField::getActiveFragmentCenters() const {
	std::vector<ofVec2f> centers;
	centers.reserve(particles.size());
	for (const auto& p : particles) {
		if (p.playheadIndex < 0) {
			continue;
		}
		centers.push_back(ofVec2f(p.pos.x / canvasW, p.pos.y / canvasH));
	}
	return centers;
}
