#include "TFComposition.h"
#include "ofGraphics.h"
#include "ofLog.h"
#include "ofUtils.h"
#include <cstdlib>

void TFComposition::setup(const Timing& timing_, TFPattern* bspPattern_, TFPattern* blobGridPattern_, int canvasW_, int canvasH_) {
	timing = timing_;
	bspPattern = bspPattern_;
	blobGridPattern = blobGridPattern_;
	canvasW = canvasW_;
	canvasH = canvasH_;
}

void TFComposition::setOnFragmentReassigned(std::function<void(float nx, float ny)> cb) {
	if (bspPattern) {
		bspPattern->setOnFragmentReassigned(cb);
	}
	if (blobGridPattern) {
		blobGridPattern->setOnFragmentReassigned(cb);
	}
}

void TFComposition::setTransitionParams(float duration, float hardCutWeight_, float crossfadeWeight_, float erosionWeight_) {
	transitionDuration = duration;
	hardCutWeight = hardCutWeight_;
	crossfadeWeight = crossfadeWeight_;
	erosionWeight = erosionWeight_;
}

void TFComposition::startCycle() {
	switchToPattern(TFPatternType::BSP);
	phase = CyclePhase::RUNNING;
	phaseElapsed = 0.0f;
}

void TFComposition::update(float dt) {
	phaseElapsed += dt;
	activePattern()->update(dt);
	sceneTransition.update(dt);

	if (phase == CyclePhase::RUNNING && phaseElapsed >= timing.cycleDuration) {
		beginTransitionToNextPattern();
	} else if (phase == CyclePhase::PATTERN_TRANSITION && !sceneTransition.isActive()) {
		phase = CyclePhase::RUNNING;
		phaseElapsed = 0.0f;
	}
}

void TFComposition::draw() {
	if (!sceneTransition.isActive()) {
		activePattern()->draw();
		return;
	}

	// Mid scene-level transition: render the (already-switched-to)
	// incoming pattern fresh into its own FBO every frame — never frozen,
	// same principle as TFFragmentTransition's per-fragment "to" side —
	// and blend it against the frozen outgoing snapshot captured at
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
	activePattern()->draw();
	incomingRenderFbo.end();

	ofRectangle full(0, 0, static_cast<float>(canvasW), static_cast<float>(canvasH));
	sceneTransition.draw(full, incomingRenderFbo.getTexture(), full);
}

void TFComposition::forceNextPattern() {
	beginTransitionToNextPattern();
}

void TFComposition::forcePattern(TFPatternType type) {
	if (type == activeType) {
		return;
	}
	switchToPattern(type);
	phase = CyclePhase::PATTERN_TRANSITION;
	phaseElapsed = 0.0f;
}

void TFComposition::beginTransitionToNextPattern() {
	TFPatternType next = (activeType == TFPatternType::BSP) ? TFPatternType::BLOB_GRID : TFPatternType::BSP;
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
	activePattern()->draw(); // still the OLD pattern — activeType hasn't changed yet
	outgoingSnapshotFbo.end();
}

void TFComposition::switchToPattern(TFPatternType type) {
	captureOutgoingSnapshot();

	activeType = type;
	cycleSeed = static_cast<int>(ofRandom(1, 1000000));
	srand(cycleSeed);

	ofLogNotice("TFComposition") << "pattern -> " << (activeType == TFPatternType::BSP ? "BSP" : "BLOB_GRID")
		<< " seed=" << cycleSeed;

	activePattern()->reset(cycleSeed);

	TFFragmentTransition::Style style = tfPickTransitionStyle(hardCutWeight, crossfadeWeight, erosionWeight);
	ofRectangle full(0, 0, static_cast<float>(canvasW), static_cast<float>(canvasH));
	sceneTransition.begin(style, transitionDuration, full, outgoingSnapshotFbo.getTexture(), full);

	if (onPatternChangedCb) {
		onPatternChangedCb(activeType);
	}
}

TFPattern* TFComposition::activePattern() const {
	return activeType == TFPatternType::BSP ? bspPattern : blobGridPattern;
}
