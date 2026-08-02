#include "RPRevealMask.h"
#include <algorithm>
#include <string>

#ifdef TARGET_OPENGLES
#ifndef GL_MAX_EXT
#define GL_MAX_EXT 0x8008
#endif
#ifndef GL_FUNC_ADD_EXT
#define GL_FUNC_ADD_EXT 0x8006
#endif
#endif

void RPRevealMask::setup(int width_, int height_, float trailPersistence_) {
	width = width_;
	height = height_;
	trailPersistence = trailPersistence_;

	ofFbo::Settings s;
	s.width = width;
	s.height = height;
	s.internalformat = GL_RGBA;
	s.useDepth = false;

	captureFbo.allocate(s);
	fboA.allocate(s);
	fboB.allocate(s);

	captureFbo.begin(); ofClear(0, 0, 0, 0); captureFbo.end();
	fboA.begin(); ofClear(0, 0, 0, 0); fboA.end();
	fboB.begin(); ofClear(0, 0, 0, 0); fboB.end();

	bool loaded = maskShader.load("shaders/reveal_mask.vert", "shaders/reveal_mask.frag");
	if (!loaded) {
		ofLogError("RPRevealMask") << "failed to load reveal_mask shader";
	}

	detectMaxBlendSupport();
	ready = true;
}

void RPRevealMask::detectMaxBlendSupport() {
#ifdef TARGET_OPENGLES
	const char * ext = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
	supportsMaxBlend = (ext != nullptr) && (std::string(ext).find("GL_EXT_blend_minmax") != std::string::npos);
#else
	// Desktop GL: glBlendEquation(GL_MAX) is core since OpenGL 1.4, no
	// extension check needed.
	supportsMaxBlend = true;
#endif
	ofLogNotice("RPRevealMask") << "pulse-stamp compositing: "
		<< (supportsMaxBlend ? "GL_MAX blend equation" : "sorted alpha-over fallback (GL_EXT_blend_minmax unavailable)");
}

void RPRevealMask::beginStamp() {
	captureFbo.begin();
	ofClear(0, 0, 0, 0);
}

void RPRevealMask::drawStampMesh(const RPPulseStamp & stamp) const {
	const int segments = 48;
	float inner = std::max(0.0f, stamp.radius - stamp.bandWidth);

	ofMesh mesh;
	mesh.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);
	for (int i = 0; i <= segments; ++i) {
		float a = TWO_PI * i / static_cast<float>(segments);
		float ca = std::cos(a);
		float sa = std::sin(a);

		mesh.addVertex({ stamp.x + ca * inner, stamp.y + sa * inner, 0.0f });
		mesh.addColor(ofFloatColor(stamp.tint.r, stamp.tint.g, stamp.tint.b, 0.0f));

		mesh.addVertex({ stamp.x + ca * stamp.radius, stamp.y + sa * stamp.radius, 0.0f });
		mesh.addColor(ofFloatColor(stamp.tint.r, stamp.tint.g, stamp.tint.b, 1.0f));
	}
	mesh.draw();
}

void RPRevealMask::stampPulses(const std::vector<RPPulseStamp> & stamps) {
	if (!ready || stamps.empty()) return;

	ofPushStyle();
	ofSetColor(255);
	glEnable(GL_BLEND);

	if (supportsMaxBlend) {
		// Draw order doesn't matter — overlapping stamps combine via true
		// component-wise max, so the brightest wins outright regardless of
		// which pulse happens to be drawn last.
		glBlendFunc(GL_ONE, GL_ONE);
#ifdef TARGET_OPENGLES
		glBlendEquationEXT(GL_MAX_EXT);
#else
		glBlendEquation(GL_MAX);
#endif
		for (const auto & s : stamps) drawStampMesh(s);
#ifdef TARGET_OPENGLES
		glBlendEquationEXT(GL_FUNC_ADD_EXT);
#else
		glBlendEquation(GL_FUNC_ADD);
#endif
	} else {
		// Sorted-draw fallback: dimmest/oldest first, so the freshest stamp
		// lands on top via ordinary alpha-over — an approximation, not a
		// true max, of the intended blend (see RPRevealMask.h).
		std::vector<RPPulseStamp> sorted = stamps;
		std::sort(sorted.begin(), sorted.end(), [](const RPPulseStamp & a, const RPPulseStamp & b) {
			return a.freshness < b.freshness;
		});
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		for (const auto & s : sorted) drawStampMesh(s);
	}

	glDisable(GL_BLEND);
	ofPopStyle();
}

void RPRevealMask::endStamp() {
	captureFbo.end();
}

void RPRevealMask::update() {
	if (!ready) return;

	writeFbo().begin();
	maskShader.begin();
	maskShader.setUniformTexture("history", readFbo().getTexture(), 0);
	maskShader.setUniformTexture("current", captureFbo.getTexture(), 1);
	maskShader.setUniform1f("trailPersistence", trailPersistence);
	maskShader.setUniform1f("maxOpacity", maxOpacity);
	ofSetColor(255);
	captureFbo.getTexture().draw(0, 0, static_cast<float>(width), static_cast<float>(height));
	maskShader.end();
	writeFbo().end();

	pingPong = !pingPong;
}

ofTexture & RPRevealMask::getMaskTexture() {
	return readFbo().getTexture();
}

void RPRevealMask::clear() {
	fboA.begin(); ofClear(0, 0, 0, 0); fboA.end();
	fboB.begin(); ofClear(0, 0, 0, 0); fboB.end();
}
