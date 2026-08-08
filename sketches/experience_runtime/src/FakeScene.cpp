#include "FakeScene.h"

#include "ofGraphics.h"
#include "ofLog.h"
#include "ofPixels.h"

#include <cmath>

namespace {

constexpr glm::ivec2 kNativeRenderSize{320, 180};

// GLSL 120 (no #version-core-profile requirement) — this window's GL
// context rejected both #version 150 and #version 330 as "not supported"
// at build time (logged), meaning it's a legacy/compatibility-profile
// context here, not the 3.2+ core profile OF's programmable renderer
// usually assumes. 120 with attribute/varying/gl_FragColor is the
// broadest-compatibility fallback.
const std::string kMarkerVertexShader = R"(
#version 120

uniform mat4 modelViewProjectionMatrix;
attribute vec4 position;

void main() {
    gl_Position = modelViewProjectionMatrix * position;
}
)";

const std::string kMarkerFragmentShader = R"(
#version 120

void main() {
    gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0);
}
)";

ofColor healthColor(SceneHealth health) {
	switch (health) {
		case SceneHealth::Ready:    return ofColor(60, 200, 140);
		case SceneHealth::Loading:  return ofColor(120, 120, 120);
		case SceneHealth::Degraded: return ofColor(220, 160, 40);
		case SceneHealth::Failed:   return ofColor(210, 60, 60);
	}
	return ofColor(255, 0, 255);
}

} // namespace

void FakeScene::setup(const SceneServices& services) {
	if (!lifecycle_.onSetup()) {
		ofLogError("FakeScene") << "setup() called after shutdown() — ignored";
		return;
	}
	services_ = services;

	ofPixels px;
	px.allocate(4, 4, OF_PIXELS_RGBA);
	px.setColor(ofColor(255, 255, 255, 255));
	markerTexture_.allocate(px); // allocates and uploads in one call

	markerShaderLoaded_ = markerShader_.setupShaderFromSource(GL_VERTEX_SHADER, kMarkerVertexShader)
		&& markerShader_.setupShaderFromSource(GL_FRAGMENT_SHADER, kMarkerFragmentShader)
		&& markerShader_.bindDefaults()
		&& markerShader_.linkProgram();
	if (!markerShaderLoaded_) {
		// Best-effort: shader-binding contamination is skipped gracefully
		// if this GL context rejects the inline GLSL source. Every other
		// contamination category is independent of this one.
		ofLogWarning("FakeScene") << "marker shader failed to load; shader-binding "
			"contamination will be skipped for this run";
	}

	nestedContaminationFbo_.allocate(8, 8, GL_RGBA);

	testHealth_ = SceneHealth::Ready;
}

void FakeScene::activate() {
	if (!lifecycle_.onActivate()) {
		ofLogError("FakeScene") << "activate() called after shutdown()";
	}
}

void FakeScene::deactivate() {
	if (!lifecycle_.onDeactivate()) {
		ofLogError("FakeScene") << "deactivate() called after shutdown()";
	}
	// Deliberately does NOT touch markerTexture_/markerShader_ — per
	// contract §6, deactivate() must not imply resource destruction and
	// the scene must remain valid for a later activate().
}

void FakeScene::update(float dt) {
	if (!lifecycle_.onUpdate()) {
		return;
	}
	if (lifecycle_.active) {
		activeSeconds_ += dt;
	}
}

void FakeScene::drawToCurrentTarget() {
	if (!lifecycle_.onDraw()) {
		return;
	}

	ofPushStyle();
	ofSetColor(healthColor(testHealth_));
	// Deterministic, resetEpoch-driven position: proves a command/reset
	// visibly changed scene output rather than merely returning true.
	float markerX = 10.0f + static_cast<float>((lifecycle_.resetEpoch * 17) % (kNativeRenderSize.x - 40));
	ofDrawRectangle(markerX, 10.0f, 30.0f, 30.0f);
	ofSetColor(255);
	ofDrawBitmapString(sceneId(), 10, kNativeRenderSize.y - 10);
	ofPopStyle();

	if (contaminateOnDraw_) {
		applyGLContamination();
	}
}

void FakeScene::applyGLContamination() const {
	// Deliberately unbalanced/left-enabled GL state, on purpose, for
	// SceneRenderGuard's restoration to be proven against. Every category
	// listed in Scene-HUD-Contract-v1.md §5's baseline is represented.
	glEnable(GL_SCISSOR_TEST);
	glScissor(0, 0, 5, 5);

	glEnable(GL_STENCIL_TEST);

	ofEnableBlendMode(OF_BLENDMODE_ADD); // non-standard, left enabled

	markerTexture_.bind(); // left bound, no unbind()

	if (markerShaderLoaded_) {
		markerShader_.begin(); // left bound, no end()
	}

	ofPushMatrix();
	ofTranslate(9999.0f, 9999.0f); // unbalanced: no matching ofPopMatrix()

	ofSetColor(255, 0, 255);
	ofSetLineWidth(7.0f); // left changed, no ofPushStyle()/ofPopStyle() around it

	// Framebuffer binding contamination: rebind the RAW GL framebuffer to
	// a different, throwaway FBO's id — exactly the "expected framebuffer
	// rebound" category in Scene-HUD-Contract-v1.md §5's baseline.
	//
	// Deliberately uses a raw glBindFramebuffer() call here, NOT
	// nestedContaminationFbo_.begin() — begin() also pushes onto OF's own
	// internal, process-global ofMatrixStack (view/projection/viewport/
	// orientation), and since this contamination is intentionally never
	// end()'d, that push is never popped either. That corrupts OF's
	// shared matrix-stack bookkeeping for the rest of the process, not
	// just this one guarded draw — a real, separately-documented finding
	// (see the implementation report's GL-restoration section: raw
	// framebuffer-binding contamination is fully recoverable by
	// SceneRenderGuard; OF-level unmatched ofFbo::begin() nesting is a
	// distinct hazard SceneRenderGuard cannot generically repair, because
	// it would require it to know how many extra levels to pop from a
	// stack it doesn't own).
	glBindFramebuffer(GL_FRAMEBUFFER, nestedContaminationFbo_.getId());

	glViewport(0, 0, 3, 3); // deliberately wrong viewport
}

glm::ivec2 FakeScene::nativeRenderSize() const {
	return kNativeRenderSize;
}

std::string FakeScene::sceneId() const {
	return kSceneId;
}

std::string FakeScene::displayName() const {
	return kDisplayName;
}

SceneSemanticData FakeScene::buildSemanticData() const {
	SceneSemanticData data;
	data.schemaVersion = 1;

	// Deterministic, resetEpoch-driven state-ID variation — covers
	// "changing primary and secondary state IDs" without any randomness.
	data.state.primaryStateId = (lifecycle_.resetEpoch % 2 == 0)
		? "scene.fake.state.alpha"
		: "scene.fake.state.beta";
	if (lifecycle_.resetEpoch % 3 == 0) {
		data.state.secondaryStateId = "scene.fake.state.secondary";
	}

	data.timing.activeSeconds = activeSeconds_;
	data.timing.stateElapsedSeconds = activeSeconds_;
	data.timing.stateProgress = std::fmod(activeSeconds_, 1.0f); // the ONLY progress value
	data.timing.generation = static_cast<uint64_t>(lifecycle_.resetEpoch);

	switch (semanticVariant_) {
		case SemanticVariant::Absent:
			// Never reached here — hudStatus() short-circuits before
			// calling this for Absent. Kept for switch completeness.
			break;

		case SemanticVariant::Complete: {
			data.activity.overall = 0.5f;
			data.activity.motion = 0.25f;
			data.activity.density = 0.75f;
			data.activity.variation = 0.1f;
			data.activity.transition = 0.0f; // present-and-zero, valid alongside populated siblings

			data.metrics.reserve(5);
			SceneMetric scalar;
			scalar.metricId = "scene.fake.metric.scalar";
			scalar.valueType = HudMetricValueType::Scalar;
			scalar.dataClass = HudDataClass::Literal;
			scalar.value = 42.0f;
			data.metrics.push_back(scalar);

			SceneMetric count;
			count.metricId = "scene.fake.metric.count";
			count.valueType = HudMetricValueType::Count;
			count.dataClass = HudDataClass::Literal;
			count.value = static_cast<float>(lifecycle_.resetEpoch);
			data.metrics.push_back(count);

			SceneMetric ratio;
			ratio.metricId = "scene.fake.metric.ratio";
			ratio.valueType = HudMetricValueType::Ratio;
			ratio.dataClass = HudDataClass::Normalized;
			ratio.normalizedValue = 0.33f;
			data.metrics.push_back(ratio);

			SceneMetric duration;
			duration.metricId = "scene.fake.metric.duration";
			duration.valueType = HudMetricValueType::DurationSeconds;
			duration.dataClass = HudDataClass::Derived;
			duration.value = activeSeconds_;
			duration.unitId = "unit.seconds";
			data.metrics.push_back(duration);

			SceneMetric identifier;
			identifier.metricId = "scene.fake.metric.identifier";
			identifier.valueType = HudMetricValueType::Identifier;
			identifier.dataClass = HudDataClass::Ambient;
			identifier.valueId = data.state.primaryStateId;
			data.metrics.push_back(identifier);
			break;
		}

		case SemanticVariant::MissingSignals:
			// Only overall populated — the rest stay std::nullopt
			// (missing), never defaulted to 0.
			data.activity.overall = 0.6f;
			break;

		case SemanticVariant::ZeroSignals:
			// All five populated AND exactly zero — "present but
			// inactive," per the contract's own rule, distinct from
			// MissingSignals above.
			data.activity.overall = 0.0f;
			data.activity.motion = 0.0f;
			data.activity.density = 0.0f;
			data.activity.variation = 0.0f;
			data.activity.transition = 0.0f;
			break;

		case SemanticVariant::ZeroMetrics:
			data.activity.overall = 0.4f;
			// metrics stays default-constructed (empty) — a valid state.
			break;

		case SemanticVariant::ManyMetrics:
			data.activity.overall = 0.8f;
			data.metrics.reserve(kMaxMetrics);
			for (size_t i = 0; i < kMaxMetrics; ++i) {
				SceneMetric m;
				m.metricId = "scene.fake.metric." + std::to_string(i);
				m.valueType = HudMetricValueType::Scalar;
				m.dataClass = HudDataClass::Literal;
				m.value = static_cast<float>(i);
				data.metrics.push_back(m);
			}
			break;
	}

	return data;
}

namespace {

// Two deterministic slots (resetEpoch-driven, no randomness) — shared by
// buildActiveEffectsLabels() (compatibility labels) and
// currentEffectActivityStatus() (canonical HudFrameData.effects source).
// Each caller independently decides what to DO with these slots; neither
// derives its output from the other's output — see both call sites' own
// comments.
std::vector<videoeffects::EffectActivitySlot> buildDemoSlots(int resetEpoch) {
	std::vector<videoeffects::EffectActivitySlot> slots;

	videoeffects::EffectActivitySlot primary;
	primary.slotId = "primary";
	primary.effectId = "heatmap_recolor";
	primary.displayName = "Heatmap Recolor";
	bool transitioning = (resetEpoch % 2) != 0;
	primary.phase = transitioning ? videoeffects::EvolutionPhase::Transitioning
		: videoeffects::EvolutionPhase::Holding;
	primary.transitionProgress01 = transitioning ? 0.5f : 1.0f;
	primary.prominence = 0.8f;
	slots.push_back(primary);

	videoeffects::EffectActivitySlot secondary;
	secondary.slotId = "secondary";
	secondary.effectId = "channelshift";
	secondary.displayName = "Channel Shift";
	secondary.phase = videoeffects::EvolutionPhase::Holding;
	secondary.transitionProgress01 = 1.0f;
	secondary.prominence = 0.3f;
	slots.push_back(secondary);

	return slots;
}

} // namespace

std::vector<std::string> FakeScene::buildActiveEffectsLabels() const {
	if (effectActivityTestState_ != EffectActivityTestState::PresentActive) {
		// A real, valid state of the underlying type too — zero slots,
		// not a placeholder.
		return {};
	}

	// Fed through the REAL videoeffects::resolveDominantEffectLabels() —
	// proves the shared header links and its dominance-resolution logic
	// actually runs against this scene's data, exactly as a real
	// multi-slot scene (e.g. quadrant-crosshair) would use it.
	videoeffects::EffectActivityStatus effectStatus;
	effectStatus.slots = buildDemoSlots(lifecycle_.resetEpoch);
	return videoeffects::resolveDominantEffectLabels(effectStatus);
}

std::optional<videoeffects::EffectActivityStatus> FakeScene::currentEffectActivityStatus() const {
	lifecycle_.onEffectActivityPoll();

	if (effectActivityTestState_ == EffectActivityTestState::NoSnapshot) {
		// No authoritative Shared Effects snapshot available — distinct
		// from PresentEmpty below, per this increment's frozen semantics.
		return std::nullopt;
	}

	videoeffects::EffectActivityStatus status;

	// Health derived via the REAL deriveEffectHealth(), against a real
	// (honestly empty) VideoEffectLoadReport — this scene requests zero
	// shader effects, so allOk() is genuinely true and Ready is the
	// correct, non-synthesized answer, not a faked default. A full
	// videoeffects::VideoEffectService was deliberately NOT instantiated
	// here — see this increment's report ("authoritative Shared Effects
	// owner/source") for why that would have been out of this closure
	// session's scope (real shader/manifest loading this harness has none
	// of, matching the non-goal "production effect-selector completion").
	videoeffects::VideoEffectLoadReport emptyLoadReport;
	status.health = videoeffects::deriveEffectHealth(emptyLoadReport);
	status.messageId = std::nullopt; // always nullopt when health == Ready, per this field's own contract

	if (effectActivityTestState_ == EffectActivityTestState::PresentActive) {
		status.slots = buildDemoSlots(lifecycle_.resetEpoch);
	}
	// else PresentEmpty: status.slots stays default-constructed (empty) —
	// "snapshot exists, zero active effects," never inferred as failure.

	return status;
}

SceneHudStatus FakeScene::hudStatus() const {
	lifecycle_.onHudStatusPoll();

	SceneHudStatus status;
	status.schemaVersion = 1;
	status.sceneId = sceneId();
	status.displayName = displayName();
	status.health = testHealth_;
	if (testHealth_ == SceneHealth::Degraded) {
		status.message = "fake scene: degraded (test state)";
	} else if (testHealth_ == SceneHealth::Failed) {
		status.message = "fake scene: failed (test state)";
	}
	status.modeName = "fake-mode";
	status.activeItemCount = lifecycle_.resetEpoch;
	status.paused = !lifecycle_.active;
	status.activeEffects = buildActiveEffectsLabels();

	if (semanticVariant_ != SemanticVariant::Absent) {
		status.semantic = buildSemanticData();
	}
	// else: status.semantic stays std::nullopt — the approved "semantic
	// data is optional" behavior, exercised explicitly by SemanticVariant::Absent.

	return status;
}

SceneCapabilities FakeScene::capabilities() const {
	lifecycle_.onCapabilitiesPoll();

	SceneCapabilities caps;
	SceneCommandDescriptor resetCmd;
	resetCmd.command = SceneCommand::Reset;
	resetCmd.label = "Reset Fake Scene";
	resetCmd.shortLabel = "Reset";
	resetCmd.prominent = true;
	caps.commands.push_back(resetCmd);
	return caps;
}

bool FakeScene::executeCommand(SceneCommand command) {
	bool isSupported = (command == SceneCommand::Reset);
	return lifecycle_.onExecuteCommand(isSupported, isSupported);
}

void FakeScene::reset() {
	if (!lifecycle_.onDirectReset()) {
		return;
	}
	activeSeconds_ = 0.0f;
}

void FakeScene::shutdown() {
	if (!lifecycle_.onShutdown()) {
		return;
	}
	markerTexture_.clear();
	markerShader_.unload();
	markerShaderLoaded_ = false;
}
