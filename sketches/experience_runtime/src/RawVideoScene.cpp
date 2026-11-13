#include "RawVideoScene.h"

#include "ofGraphics.h"
#include "ofLog.h"

#include <algorithm>

namespace {

// Same conventions as shared/src/phosphor-contours: GLSL 1.20 (desktop GL 2.1), the same text under GLES2 via
// the precision header (UNTESTED on Pi), plain GL_TEXTURE_2D with NORMALIZED uv.
const char* kHeader =
	"#version 120\n"
	"#ifdef GL_ES\n"
	"#ifdef GL_FRAGMENT_PRECISION_HIGH\nprecision highp float;\n#else\nprecision mediump float;\n#endif\n"
	"#endif\n";

const char* kVert = R"GLSL(
varying vec2 vUV;
void main() {
	vUV = gl_MultiTexCoord0.xy;
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
)GLSL";

// Same maths as shared/assets/video-effects/utility/video_adjust.glsl (brightness offset, contrast about
// mid-grey, saturation mix against Rec.601 luma).
const char* kFrag = R"GLSL(
uniform sampler2D tex0;
uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;
varying vec2 vUV;
void main() {
	vec4 c = texture2D(tex0, vUV);
	c.rgb += uBrightness;
	c.rgb = (c.rgb - 0.5) * uContrast + 0.5;
	float luma = dot(c.rgb, vec3(0.299, 0.587, 0.114));
	c.rgb = mix(vec3(luma), c.rgb, uSaturation);
	gl_FragColor = vec4(clamp(c.rgb, 0.0, 1.0), 1.0);
}
)GLSL";

SceneMetric identifierMetric(const std::string& id, const std::string& value) {
	SceneMetric m;
	m.metricId = id;
	m.valueType = HudMetricValueType::Identifier;
	m.dataClass = HudDataClass::Literal;
	m.valueId = value;
	return m;
}

} // namespace

RawVideoScene::RawVideoScene(VideoPlaybackService& video)
	: video_(video) {
}

bool RawVideoScene::compileShader() {
	const std::string header = kHeader;
	const bool ok = adjust_.setupShaderFromSource(GL_VERTEX_SHADER, header + kVert)
		&& adjust_.setupShaderFromSource(GL_FRAGMENT_SHADER, header + kFrag)
		&& adjust_.linkProgram();
	ofLogNotice("RawVideo") << "adjust shader: " << (ok ? "OK" : "FAILED");
	return ok;
}

void RawVideoScene::setup(const SceneServices& services) {
	(void)services; // consumes the canonical VideoPlaybackService directly, like the other scenes
	shaderOk_ = compileShader(); // failure is reported via hudStatus(); the scene then shows the untreated video
	srcFbo_.allocate(kNativeWidth, kNativeHeight, GL_RGBA);
	srcFbo_.begin();
	ofClear(0, 0, 0, 255);
	srcFbo_.end();
	didSetup_ = true;
	didShutdown_ = false;
}

void RawVideoScene::activate() {
	if (!didSetup_ || didShutdown_) return;
	active_ = true;
	time_ = 0.f;
}

void RawVideoScene::deactivate() {
	active_ = false;
}

void RawVideoScene::update(float dt) {
	if (!active_) return;
	time_ += dt;
}

void RawVideoScene::drawToCurrentTarget() {
	if (!didSetup_ || didShutdown_ || !srcFbo_.isAllocated()) return;
	const ofTexture* tex = video_.currentTexture();

	// Source into the scene's own target, cover-fit (the same framing every other scene uses).
	srcFbo_.begin();
	ofDisableAlphaBlending();
	ofSetColor(0);
	ofDrawRectangle(0, 0, kNativeWidth, kNativeHeight);
	haveFrame_ = tex != nullptr && tex->isAllocated() && tex->getWidth() > 0 && tex->getHeight() > 0;
	if (haveFrame_) {
		const float vw = tex->getWidth(), vh = tex->getHeight();
		const float s = std::max(float(kNativeWidth) / vw, float(kNativeHeight) / vh);
		const float dw = vw * s, dh = vh * s;
		ofSetColor(255);
		tex->draw((kNativeWidth - dw) * 0.5f, (kNativeHeight - dh) * 0.5f, dw, dh);
	}
	srcFbo_.end();

	// Present into the runtime-owned scene target (already bound + cleared). At the identity detents (or if the
	// shader failed) the source frame is drawn as-is: no pass, no cost, no possible tint.
	ofSetColor(255);
	if (shaderOk_ && !dials_.isIdentity()) {
		adjust_.begin();
		adjust_.setUniformTexture("tex0", srcFbo_.getTexture(), 0);
		adjust_.setUniform1f("uBrightness", dials_.value(rawvideo::kBrightness));
		adjust_.setUniform1f("uContrast", dials_.value(rawvideo::kContrast));
		adjust_.setUniform1f("uSaturation", dials_.value(rawvideo::kSaturation));
		srcFbo_.draw(0, 0, kNativeWidth, kNativeHeight);
		adjust_.end();
	} else {
		srcFbo_.draw(0, 0, kNativeWidth, kNativeHeight);
	}
}

SceneHudStatus RawVideoScene::hudStatus() const {
	SceneHudStatus status;
	status.schemaVersion = 1;
	status.sceneId = kSceneId;
	status.displayName = displayName();
	if (didShutdown_) {
		status.health = SceneHealth::Failed;
		status.message = std::string("shut down");
	} else if (!didSetup_ || !active_) {
		status.health = SceneHealth::Loading;
	} else if (!shaderOk_) {
		status.health = SceneHealth::Degraded;
		status.message = std::string("Adjust shader failed; untreated video");
	} else if (!haveFrame_) {
		status.health = SceneHealth::Degraded;
		status.message = std::string("No video frame");
	} else {
		status.health = SceneHealth::Ready;
	}
	{
		VideoPlaybackStatus videoStatus = video_.status();
		if (videoStatus.fallbackDisplayTitle) status.mediaName = videoStatus.fallbackDisplayTitle;
		else if (videoStatus.titleId) status.mediaName = videoStatus.titleId;
	}
	// No mode model (SceneHudStatus.mode stays nullopt: never a fabricated 1-of-1). Three selector rows
	// (BRIGHTNESS / CONTRAST / SATURATION), published only while running (missing data -> HUD rows hide).
	if (didSetup_ && active_ && !didShutdown_) {
		SceneSemanticData data;
		data.state.primaryStateId = "scene.raw.state.online";
		data.timing.activeSeconds = time_;
		for (int i = 0; i < rawvideo::kDialCount; ++i) {
			const std::string p = "scene.raw.metric.c" + std::to_string(i);
			data.metrics.push_back(identifierMetric(p + "_label", rawvideo::dialLabel(i)));
			data.metrics.push_back(identifierMetric(p + "_value", rawvideo::dialText(i, dials_.index[i])));
			data.metrics.push_back(identifierMetric(p + "_pos", rawvideo::dialPosition(dials_.index[i])));
		}
		status.semantic = data;
	}
	status.paused = false;
	return status;
}

SceneCapabilities RawVideoScene::capabilities() const {
	SceneCapabilities caps;
	SceneCommandDescriptor next;
	next.command = SceneCommand::NextMedia; next.label = "Next"; next.shortLabel = "NEXT";
	caps.commands.push_back(next);
	SceneCommandDescriptor prev;
	prev.command = SceneCommand::PreviousMedia; prev.label = "Previous"; prev.shortLabel = "PREV";
	caps.commands.push_back(prev);
	for (int i = 0; i < rawvideo::kDialCount; ++i) {
		SceneOptionAxis a;
		a.id = rawvideo::dialId(i);
		a.label = rawvideo::dialLabel(i);
		a.count = rawvideo::kSteps;
		caps.optionAxes.push_back(a);
	}
	return caps; // Reset honoured but not advertised (no visible reset control); no modes, no reseed
}

bool RawVideoScene::executeCommand(SceneCommand command) {
	if (!didSetup_ || didShutdown_) return false;
	switch (command) {
		case SceneCommand::NextMedia: return video_.next();
		case SceneCommand::PreviousMedia: return video_.previous();
		case SceneCommand::Reset:
			reset();
			return true;
		default:
			return false;
	}
}

bool RawVideoScene::stepOption(int axisIndex, int delta) {
	if (!didSetup_ || didShutdown_) return false;
	return dials_.step(axisIndex, delta);
}

void RawVideoScene::reset() {
	dials_.reset();
	time_ = 0.f;
}

void RawVideoScene::shutdown() {
	active_ = false;
	didShutdown_ = true;
}
