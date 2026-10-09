#include "HudArtReferenceExporter.h"

#include "ofAppRunner.h"
#include "ofFbo.h"
#include "ofGraphics.h"
#include "ofImage.h"
#include "ofJson.h"
#include "ofLog.h"
#include "ofPolyline.h"
#include "ofUtils.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using hudpresent::HudRegionRole;
using hudpresent::HudWireframeRenderer;

namespace {

constexpr int kW = 1280, kH = 720;
const ofColor kRuntimeBackground(12, 12, 12); // ExperienceRuntime::draw(): ofBackground(12,12,12)

const char* roleName(HudRegionRole r) {
	switch (r) {
		case HudRegionRole::Universal: return "Universal";
		case HudRegionRole::Controls: return "Controls";
		case HudRegionRole::Flexible: return "Flexible";
		case HudRegionRole::Overlay: return "Overlay";
	}
	return "?";
}

// Production presentation membership, from the renderer's own rules:
// media_viewport is drawn unconditionally (HudWireframeRenderer::draw(), mesh
// block runs before and independent of RenderScope); binding regions follow
// regionInMinimalSlice().
std::string scopeOf(const std::string& id) {
	if (id == "media_viewport") return "both (mesh, always drawn)";
	return HudWireframeRenderer::regionInMinimalSlice(id) ? "MinimalSlice+FullProfile" : "FullProfile only";
}

std::string rectText(const ofRectangle& r) {
	char buf[96];
	std::snprintf(buf, sizeof(buf), "%.1f,%.1f %.1fx%.1f", r.x, r.y, r.width, r.height);
	return buf;
}

// Straight-alpha "over" for BOTH colour and alpha, so translucent fills drawn
// into a transparent FBO produce correct PNG alpha (plain OF_BLENDMODE_ALPHA
// would square the destination alpha).
void beginOverBlend() {
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

std::vector<glm::vec2> meshOutline(const ofMesh& mesh) {
	// Triangle fan: vertex 0 = centre, last vertex repeats vertex 1 (closing).
	std::vector<glm::vec2> pts;
	for (size_t i = 1; i + 1 < mesh.getNumVertices(); ++i) pts.emplace_back(mesh.getVertex(i).x, mesh.getVertex(i).y);
	return pts;
}

void drawOutline(const std::vector<glm::vec2>& pts) {
	ofPolyline line;
	for (const auto& p : pts) line.addVertex(p.x, p.y);
	line.close();
	line.draw();
}

ofPixels readFbo(ofFbo& fbo) {
	ofPixels px;
	fbo.readToPixels(px);
	return px;
}

} // namespace

bool HudArtReferenceExporter::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_HUD_ART_EXPORT") != nullptr;
}

HudArtReferenceExporter::HudArtReferenceExporter(ExperienceRuntime& runtime) : runtime_(runtime) {
	const char* dir = std::getenv("EXPERIENCE_RUNTIME_HUD_ART_EXPORT_DIR");
	outDir_ = (dir && dir[0]) ? std::string(dir) : ofToDataPath("../../../../docs/hud-art/blueprint-v1-reference", true);
	std::error_code ec;
	fs::create_directories(outDir_, ec);
	note("HudArtReferenceExporter: output " + outDir_);
}

void HudArtReferenceExporter::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	note(std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " — " + detail));
}

void HudArtReferenceExporter::note(const std::string& line) {
	ofLogNotice("HudArtReferenceExporter") << line;
	lines_.push_back(line);
}

void HudArtReferenceExporter::step(float /*dt*/) {
	if (frame_ > 90) return; // export done; ofExit() pending
	runtime_.draw();
	if (++frame_ == 90) { // let the real scene and HUD settle
		exportAll();
		std::ofstream out(fs::path(outDir_) / "verification.txt");
		for (const auto& l : lines_) out << l << "\n";
		out.close();
		ofExit(failures_ == 0 ? 0 : 1);
	}
}

void HudArtReferenceExporter::exportAll() {
	const auto& bridge = runtime_.hudCompositorBridgeForTesting();
	const HudWireframeRenderer& r = bridge.renderer();
	const auto& regions = r.regionCatalog().all();

	check("no skin active (comparison frame is the plain wireframe)", !bridge.skinLoader().active().has_value());
	check("catalog has 19 regions", regions.size() == 19, ofToString(regions.size()));

	// -- Real runtime frame (the frame just drawn by runtime_.draw()) ----------
	ofImage runtimeFrame;
	runtimeFrame.grabScreen(0, 0, kW, kH);
	runtimeFrame.save(fs::path(outDir_) / "runtime_wireframe_reference_1280x720.png");
	const ofPixels& rt = runtimeFrame.getPixels();
	check("runtime capture is 1280x720", rt.getWidth() == kW && rt.getHeight() == kH,
		ofToString(rt.getWidth()) + "x" + ofToString(rt.getHeight()));

	// -- Geometry from the live renderer ----------------------------------------------
	const ofRectangle vp = r.pixelBoundsFor("media_viewport");
	const ofMesh& mesh = r.mediaViewportMesh().mesh();
	const auto outline = meshOutline(mesh);
	check("viewport mesh present (drawn by the runtime this run)", mesh.getNumVertices() >= 3,
		ofToString(mesh.getNumVertices()) + " vertices");
	auto hasVertex = [&](float x, float y) {
		for (const auto& p : outline) if (std::abs(p.x - x) < 0.01f && std::abs(p.y - y) < 0.01f) return true;
		return false;
	};
	const float bevel = 42.0f; // MediaViewportGeometryParams default, unclamped at 768x504 (clamp is 40% of 504)
	check("mesh outline has lower-right bevel endpoint (right edge, y = bottom - 42)",
		hasVertex(vp.getRight(), vp.getBottom() - bevel), rectText(vp));
	check("mesh outline has lower-right bevel endpoint (bottom edge, x = right - 42)",
		hasVertex(vp.getRight() - bevel, vp.getBottom()));
	check("no mesh vertex at the square lower-right corner (bevel, not rounded/square)",
		!hasVertex(vp.getRight(), vp.getBottom()));

	ofFbo fbo;
	ofFboSettings s;
	s.width = kW; s.height = kH; s.internalformat = GL_RGBA; s.useDepth = false; s.numSamples = 0;
	fbo.allocate(s);

	// Silhouette mask = the real mesh, filled (same triangle fan the renderer draws).
	fbo.begin();
	ofClear(0, 0, 0, 0);
	ofPushStyle(); ofDisableBlendMode(); ofSetColor(255); mesh.draw(); ofPopStyle();
	fbo.end();
	ofPixels mask = readFbo(fbo);
	// Orientation: FBO readback vs screen top-left origin. A pixel at the
	// viewport's top edge region must be inside the silhouette; its vertical
	// mirror (near the bottom margin, below the viewport) must not be.
	const bool flipped = mask.getColor(400, 130).a == 0 && mask.getColor(400, kH - 1 - 130).a != 0;
	auto fix = [&](ofPixels& p) { if (flipped) p.mirror(true, false); };
	fix(mask);
	check("silhouette orientation resolved", mask.getColor(400, 130).a > 0 && mask.getColor(5, 5).a == 0,
		flipped ? "FBO readback was flipped; corrected" : "not flipped");

	auto inMask = [&](int x, int y) { return mask.getColor(x, y).a > 0; };

	// -- Viewport alignment against the real runtime frame ------------------------------
	// Pixels inside the viewport BOUNDS but outside the mesh silhouette (the
	// bevel wedge and rounded-corner wedges) must show plain runtime background
	// in the real frame; the top-left corner is excluded because the runtime
	// overlay.transition widget draws there by design.
	// The runtime window is multisampled (GLFW default), so pixels within 1 px
	// of the silhouette edge are legitimately antialiased blends; they are
	// counted separately, not treated as misalignment.
	auto nearEdge = [&](int x, int y) {
		for (int dy = -1; dy <= 1; ++dy)
			for (int dx = -1; dx <= 1; ++dx)
				if (inMask(x + dx, y + dy) != inMask(x, y)) return true;
		return false;
	};
	int wedgeTotal = 0, wedgeBg = 0, bevelTotal = 0, bevelBg = 0, aaEdge = 0;
	for (int y = int(vp.y); y < int(vp.getBottom()); ++y) {
		for (int x = int(vp.x); x < int(vp.getRight()); ++x) {
			if (x + 0.5f < vp.x || y + 0.5f < vp.y || x + 0.5f > vp.getRight() || y + 0.5f > vp.getBottom()) continue;
			if (inMask(x, y)) continue;
			if (x < vp.x + 30 && y < vp.y + 30) continue; // top-left: overlay region by design
			if (nearEdge(x, y)) { aaEdge++; continue; }
			const bool bg = rt.getColor(x, y) == kRuntimeBackground;
			wedgeTotal++; wedgeBg += bg;
			if (x > vp.getRight() - 50 && y > vp.getBottom() - 50) { bevelTotal++; bevelBg += bg; }
		}
	}
	check("runtime frame: bevel wedge (outside mesh, inside bounds) is plain background",
		bevelTotal > 0 && bevelBg == bevelTotal, ofToString(bevelBg) + "/" + ofToString(bevelTotal) + " px");
	check("runtime frame: all non-overlay corner wedges are plain background",
		wedgeTotal > 0 && wedgeBg == wedgeTotal, ofToString(wedgeBg) + "/" + ofToString(wedgeTotal) + " px");
	note("antialiased silhouette-edge pixels excluded from the wedge checks (<=1 px from edge): " + ofToString(aaEdge));
	// Inner edge: 2 px inside the silhouette along the bevel, the real frame
	// shows scene content (not background).
	int edgeTotal = 0, edgeScene = 0;
	for (float t = 0.05f; t <= 0.95f; t += 0.01f) {
		const float ex = vp.getRight() - bevel * t, ey = vp.getBottom() - bevel * (1.0f - t);
		const int x = int(ex - 2.5f), y = int(ey - 2.5f);
		if (!inMask(x, y)) continue;
		edgeTotal++; edgeScene += rt.getColor(x, y) != kRuntimeBackground;
	}
	check("runtime frame: just inside the bevel line shows scene content", edgeTotal > 0 && edgeScene * 10 >= edgeTotal * 9,
		ofToString(edgeScene) + "/" + ofToString(edgeTotal) + " samples");

	// -- 1. Clean wireframe ---------------------------------------------------------------
	fbo.begin();
	ofClear(0, 0, 0, 0);
	ofPushStyle();
	beginOverBlend();
	ofNoFill();
	ofSetLineWidth(1);
	ofSetColor(200, 225, 210, 255);
	for (const auto& reg : regions) {
		if (reg.regionId == "media_viewport") continue;
		ofDrawRectangle(r.pixelBoundsFor(reg.regionId));
	}
	ofSetColor(120, 230, 160, 255);
	drawOutline(outline);
	ofPopStyle();
	fbo.end();
	ofPixels wire = readFbo(fbo); fix(wire);

	// -- 2. Labeled schematic ---------------------------------------------------------------
	fbo.begin();
	ofClear(16, 18, 20, 255);
	ofPushStyle();
	beginOverBlend();
	int index = 1;
	std::vector<std::string> legend;
	for (const auto& reg : regions) {
		const ofRectangle b = r.pixelBoundsFor(reg.regionId);
		const std::string scope = scopeOf(reg.regionId);
		ofColor fill = scope == "FullProfile only" ? ofColor(230, 150, 40, 110)
			: reg.role == HudRegionRole::Overlay ? ofColor(200, 90, 200, 120)
			: ofColor(40, 170, 160, 100);
		beginOverBlend(); // ofDrawBitmapString() (previous label) changes GL blend state
		ofFill();
		if (reg.regionId == "media_viewport") { ofSetColor(70, 80, 95, 255); mesh.draw(); }
		else { ofSetColor(fill); ofDrawRectangle(b); }
		ofNoFill();
		ofSetColor(235, 240, 235, 255);
		if (reg.regionId == "media_viewport") drawOutline(outline); else ofDrawRectangle(b);
		const std::string tag = "#" + ofToString(index);
		const std::string label = (b.width >= 8 * (reg.regionId.size() + tag.size() + 2)) ? tag + " " + reg.regionId : tag;
		if (reg.regionId != "media_viewport") ofDrawBitmapString(label, b.x + 3, b.y + 13);
		legend.push_back(ofToString(index) + (index < 10 ? "  " : " ") + reg.regionId +
			std::string(std::max<size_t>(1, 26 - reg.regionId.size()), ' ') + rectText(b) + "  " + roleName(reg.role) + ", " + scope);
		index++;
	}
	ofSetColor(235, 240, 235, 255);
	float ly = vp.y + 22;
	ofDrawBitmapString("ecopunk-hud-blueprint / 1 — 19 canonical regions (px, 1280x720; from HudRegionCatalog)", vp.x + 10, ly);
	ly += 18;
	for (const auto& l : legend) { ofDrawBitmapString(l, vp.x + 10, ly); ly += 15; }
	ly += 6;
	ofSetColor(40, 170, 160, 255); ofDrawBitmapString("teal = MinimalSlice+FullProfile", vp.x + 10, ly);
	ofSetColor(230, 150, 40, 255); ofDrawBitmapString("amber = FullProfile only", vp.x + 290, ly);
	ofSetColor(200, 90, 200, 255); ofDrawBitmapString("magenta = Overlay (both scopes)", vp.x + 490, ly);
	ofPopStyle();
	fbo.end();
	ofPixels schematic = readFbo(fbo); fix(schematic);

	// -- 3. Artwork constraint map ----------------------------------------------------------------
	fbo.begin();
	ofClear(0, 0, 0, 0);
	ofPushStyle();
	beginOverBlend();
	ofFill();
	// Inside viewport bounds, outside the mesh (bevel + corner wedges): no explicit rule.
	ofSetColor(240, 220, 40, 150);
	ofDrawRectangle(vp);
	ofSetColor(220, 40, 40, 255);
	ofPushStyle(); ofDisableBlendMode(); mesh.draw(); ofPopStyle(); // overwrite: media silhouette opaque in map key colour
	beginOverBlend();
	for (const auto& reg : regions) {
		if (reg.regionId == "media_viewport") continue;
		const ofRectangle b = r.pixelBoundsFor(reg.regionId);
		beginOverBlend();
		ofSetColor(reg.role == HudRegionRole::Overlay ? ofColor(200, 90, 200, 150) : ofColor(240, 140, 30, 130));
		ofDrawRectangle(b);
	}
	ofNoFill();
	ofSetColor(255, 255, 255, 255);
	for (const auto& reg : regions) if (reg.regionId != "media_viewport") ofDrawRectangle(r.pixelBoundsFor(reg.regionId));
	drawOutline(outline);
	float ky = vp.y + 160;
	auto key = [&](const ofColor& c, const std::string& text) {
		beginOverBlend();
		ofFill(); ofSetColor(c); ofDrawRectangle(vp.x + 40, ky - 10, 12, 12);
		ofNoFill(); ofSetColor(255); ofDrawRectangle(vp.x + 40, ky - 10, 12, 12); // visible even on the red silhouette
		ofDrawBitmapString(text, vp.x + 60, ky); ky += 18;
	};
	key(ofColor(220, 40, 40), "MEDIA SILHOUETTE: keep fully transparent (MediaViewportMesh clipping)");
	key(ofColor(240, 140, 30), "LIVE CONTENT REGION: runtime widgets draw here; no ornament/emission");
	key(ofColor(200, 90, 200), "OVERLAY REGION (over media): keep transparent; protect live text");
	key(ofColor(240, 220, 40), "NO EXPLICIT RULE: inside viewport bounds, outside mesh (bevel/corners)");
	ofSetColor(255);
	ofDrawBitmapString("UNMARKED: frame/ornament area - only rule: preserve region rects + gaps", vp.x + 60, ky);
	ky += 18;
	ofDrawBitmapString("(DEC-019 v1 safe-area = region rects + separation gaps; no other zones defined)", vp.x + 60, ky);
	ofPopStyle();
	fbo.end();
	ofPixels constraints = readFbo(fbo); fix(constraints);

	// -- Write + verify --------------------------------------------------------------------------
	// Overlay references (wireframe, constraints) must keep alpha so they can be
	// layered over artwork. The schematic is opaque by design; FreeImage writes
	// a fully opaque image as an RGB PNG, which is expected for that one.
	struct Out { std::string name; ofPixels* px; size_t channels; };
	const Out outs[] = {
		{"hud_reference_wireframe_1280x720.png", &wire, 4},
		{"hud_reference_schematic_1280x720.png", &schematic, 3},
		{"hud_reference_constraints_1280x720.png", &constraints, 4},
	};
	for (const auto& o : outs) {
		const fs::path p = fs::path(outDir_) / o.name;
		ofSaveImage(*o.px, p);
		ofPixels back;
		const bool ok = ofLoadImage(back, p);
		check(o.name + " written as 1280x720 " + (o.channels == 4 ? "RGBA (overlay)" : "RGB (opaque sheet)"),
			ok && back.getWidth() == kW && back.getHeight() == kH && back.getNumChannels() == o.channels,
			ofToString(back.getWidth()) + "x" + ofToString(back.getHeight()) + "x" + ofToString(back.getNumChannels()));
	}
	// Wireframe outline pixels coincide with the silhouette boundary.
	int onEdge = 0, edgeChecked = 0;
	for (int y = int(vp.y) - 1; y <= int(vp.getBottom()) + 1; ++y) {
		for (int x = int(vp.x) - 1; x <= int(vp.getRight()) + 1; ++x) {
			const ofColor c = wire.getColor(x, y);
			if (c.a == 0 || c.r >= 160 || c.g < 200) continue; // mesh outline colour (120,230,160) only, not region outlines (200,225,210)
			edgeChecked++;
			bool nearBoundary = false;
			for (int dy = -1; dy <= 1 && !nearBoundary; ++dy)
				for (int dx = -1; dx <= 1 && !nearBoundary; ++dx)
					if (x + dx >= 0 && y + dy >= 0 && x + dx < kW && y + dy < kH && inMask(x, y) != inMask(x + dx, y + dy)) nearBoundary = true;
			onEdge += nearBoundary;
		}
	}
	check("wireframe viewport outline lies on the real mesh boundary (±1 px)", edgeChecked > 0 && onEdge * 100 >= edgeChecked * 98,
		ofToString(onEdge) + "/" + ofToString(edgeChecked) + " outline px");
	// Region outline pixels sit on renderer bounds: top edge of universal.scene_title, and not at its vertical mirror.
	const ofRectangle st = r.pixelBoundsFor("universal.scene_title");
	check("wireframe orientation: scene_title top edge present at top, absent at mirror",
		wire.getColor(int(st.x) + 40, int(std::floor(st.y))).a > 0 && wire.getColor(int(st.x) + 40, kH - 1 - int(std::floor(st.y))).a == 0,
		"line rasterized on row " + ofToString(int(std::floor(st.y))) + " (y=" + ofToString(st.y, 1) + ")");

	// regions.json — derived export of the live catalog (not an independent definition).
	ofJson j;
	j["source"] = "derived from live HudWireframeRenderer/HudRegionCatalog at export time; HUD-006 ecopunk-hud-blueprint / 1";
	j["canvas"] = {{"width", kW}, {"height", kH}};
	j["mediaViewportMesh"] = {{"cornerRadius", 18}, {"bevel", 42}, {"roundedCorners", {"top-left", "top-right", "bottom-left"}},
		{"bevelCorner", "bottom-right"}, {"outlineVertexCount", outline.size()}};
	int n = 1;
	for (const auto& reg : regions) {
		const ofRectangle b = r.pixelBoundsFor(reg.regionId);
		j["regions"].push_back({{"index", n++}, {"id", reg.regionId}, {"role", roleName(reg.role)},
			{"normalized", {reg.bounds.x, reg.bounds.y, reg.bounds.width, reg.bounds.height}},
			{"px", {b.x, b.y, b.width, b.height}}, {"scope", scopeOf(reg.regionId)}});
	}
	ofSavePrettyJson(fs::path(outDir_) / "regions.json", j);
	note("wrote regions.json (" + ofToString(regions.size()) + " regions)");
	note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
}
