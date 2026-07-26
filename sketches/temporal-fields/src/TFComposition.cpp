#include "TFComposition.h"
#include "ofGraphics.h"
#include "ofLog.h"
#include "ofUtils.h"
#include <cstdlib>

void TFComposition::setup(const Timing& timing_, std::vector<std::pair<TFPatternType, TFPattern*>> patterns_, int canvasW_, int canvasH_) {
	timing = timing_;
	patterns = std::move(patterns_);
	canvasW = canvasW_;
	canvasH = canvasH_;
	if (!patterns.empty()) {
		activeType = patterns.front().first;
	}
}

void TFComposition::resizeCanvas(int canvasW_, int canvasH_) {
	canvasW = canvasW_;
	canvasH = canvasH_;
	for (auto& entry : patterns) {
		entry.second->resizeCanvas(canvasW, canvasH);
	}
	if (TFPattern* p = activePattern()) {
		p->reset(cycleSeed);
	}
}

void TFComposition::setOnFragmentReassigned(std::function<void(float nx, float ny)> cb) {
	for (auto& entry : patterns) {
		entry.second->setOnFragmentReassigned(cb);
	}
}

void TFComposition::setTransitionParams(float duration, float hardCutWeight_, float crossfadeWeight_, float erosionWeight_) {
	transitionDuration = duration;
	hardCutWeight = hardCutWeight_;
	crossfadeWeight = crossfadeWeight_;
	erosionWeight = erosionWeight_;
}

void TFComposition::startCycle() {
	if (patterns.empty()) {
		return;
	}
	switchToPattern(patterns.front().first);
	phase = CyclePhase::RUNNING;
	phaseElapsed = 0.0f;
}

void TFComposition::update(float dt) {
	phaseElapsed += dt;
	if (TFPattern* p = activePattern()) {
		p->update(dt);
	}
	sceneTransition.update(dt);

	if (phase == CyclePhase::RUNNING && !autoCycleSuspended && phaseElapsed >= timing.cycleDuration) {
		beginTransitionToNextPattern();
	} else if (phase == CyclePhase::PATTERN_TRANSITION && !sceneTransition.isActive()) {
		phase = CyclePhase::RUNNING;
		phaseElapsed = 0.0f;
	}
}

void TFComposition::draw() {
	TFPattern* p = activePattern();
	if (!p) {
		return;
	}

	if (!sceneTransition.isActive()) {
		p->draw();
		return;
	}

	// Mid scene-level transition: render the (already-switched-to) incoming
	// pattern fresh into its own FBO every frame — never frozen, same
	// principle as TFFragmentTransition's per-fragment "to" side — and
	// blend it against the frozen outgoing snapshot captured at
	// switchToPattern() time.
	if (!incomingRenderFbo.isAllocated() || static_cast<int>(incomingRenderFbo.getWidth()) != canvasW
		|| static_cast<int>(incomingRenderFbo.getHeight()) != canvasH) {
		ofFbo::Settings s;
		s.width = canvasW;
		s.height = canvasH;
		s.internalformat = GL_RGBA;
		s.useDepth = false;
		incomingRenderFbo.allocate(s);
	}

	incomingRenderFbo.begin();
	ofClear(0, 0, 0, 0);
	p->draw();
	incomingRenderFbo.end();

	ofRectangle full(0, 0, static_cast<float>(canvasW), static_cast<float>(canvasH));
	sceneTransition.draw(full, incomingRenderFbo.getTexture(), full);
}

void TFComposition::forceNextPattern() {
	beginTransitionToNextPattern();
}

void TFComposition::forcePattern(TFPatternType type) {
	if (type == activeType || findPattern(type) == nullptr) {
		return;
	}
	switchToPattern(type);
	phase = CyclePhase::PATTERN_TRANSITION;
	phaseElapsed = 0.0f;
}

void TFComposition::beginTransitionToNextPattern() {
	if (patterns.size() < 2) {
		return;
	}

	size_t currentIdx = 0;
	for (size_t i = 0; i < patterns.size(); i++) {
		if (patterns[i].first == activeType) {
			currentIdx = i;
			break;
		}
	}
	TFPatternType next = patterns[(currentIdx + 1) % patterns.size()].first;

	switchToPattern(next);
	phase = CyclePhase::PATTERN_TRANSITION;
	phaseElapsed = 0.0f;
}

void TFComposition::captureOutgoingSnapshot() {
	if (!outgoingSnapshotFbo.isAllocated() || static_cast<int>(outgoingSnapshotFbo.getWidth()) != canvasW
		|| static_cast<int>(outgoingSnapshotFbo.getHeight()) != canvasH) {
		ofFbo::Settings s;
		s.width = canvasW;
		s.height = canvasH;
		s.internalformat = GL_RGBA;
		s.useDepth = false;
		outgoingSnapshotFbo.allocate(s);
	}

	outgoingSnapshotFbo.begin();
	ofClear(0, 0, 0, 0); // alpha-capable — a background layer drawn beneath composition.draw() must survive scene transitions too
	if (TFPattern* p = activePattern()) {
		p->draw(); // still the OLD pattern — activeType hasn't changed yet
	}
	outgoingSnapshotFbo.end();
}

void TFComposition::switchToPattern(TFPatternType type) {
	captureOutgoingSnapshot();

	activeType = type;
	cycleSeed = static_cast<int>(ofRandom(1, 1000000));
	srand(cycleSeed);

	ofLogNotice("TFComposition") << "pattern -> " << tfPatternTypeName(activeType) << " seed=" << cycleSeed;

	if (TFPattern* p = activePattern()) {
		p->reset(cycleSeed);
	}

	TFFragmentTransition::Style style = tfPickTransitionStyle(hardCutWeight, crossfadeWeight, erosionWeight);
	ofRectangle full(0, 0, static_cast<float>(canvasW), static_cast<float>(canvasH));
	sceneTransition.begin(style, transitionDuration, full, outgoingSnapshotFbo.getTexture(), full);

	if (onPatternChangedCb) {
		onPatternChangedCb(activeType);
	}
}

TFPattern* TFComposition::activePattern() const {
	return findPattern(activeType);
}

TFPattern* TFComposition::findPattern(TFPatternType type) const {
	for (const auto& entry : patterns) {
		if (entry.first == type) {
			return entry.second;
		}
	}
	return nullptr;
}
