#include "HudArtReferenceExporter.h"
#include <algorithm>
#include <set>
#include <map>
#include "HudReactiveControlArt.h"

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
// the one production allowlist, split by provenance
// (HudWireframeRenderer::productionProvenance()): the historical 13-region
// MinimalSlice, then Living HUD v0.1, HUD Clock v0.1 and Mode Navigation v0.1.
const char* const kScopeMesh = "production (mesh, always drawn; historical MinimalSlice-13)";
const char* const kScopeOriginal = "production (historical MinimalSlice-13)";
const char* const kScopeLivingHud = "production (Living HUD v0.1)";
const char* const kScopeClock = "production (HUD Clock v0.1)";
const char* const kScopeNavigation = "production (Mode Navigation v0.1)";
const char* const kScopeVitals = "production (System Telemetry v1)";
const char* const kScopeFullOnly = "FullProfile only (not drawn in production)";

std::string scopeOf(const std::string& id) {
	if (id == "media_viewport") return kScopeMesh;
	const char* part = HudWireframeRenderer::productionProvenance(id);
	if (part == nullptr) return kScopeFullOnly;
	const std::string p(part);
	return p == "original" ? kScopeOriginal : p == "living_hud" ? kScopeLivingHud : p == "clock" ? kScopeClock : p == "system_telemetry" ? kScopeVitals : kScopeNavigation;
}

// Proposed display names (docs/hud-art/blueprint-v3-reference/RENAME_PROPOSAL.md).
// Presentation only: region IDs are frozen and unchanged until the rename proposal is accepted.
std::string displayNameOf(const std::string& id) {
	static const std::map<std::string, std::string> names = {
		{"universal.scene_title", "scene_title"}, {"universal.media_title", "scene_mode"},
		{"universal.primary_state", "media_state"}, {"universal.activity", "scene_primary_state"},
		{"universal.effect_summary", "scene_secondary_state"}, {"universal.timing", "activity_meter"},
		{"universal.activity_trace", "frame_metrics_a"}, {"universal.secondary_trace", "frame_metrics_b"},
		{"flexible.primary", "runtime_metrics"}, {"flexible.secondary_a", "active_time"}, {"flexible.secondary_b", "date_time"},
		{"overlay.health", "transition_state"}, {"overlay.transition", "runtime_log"},
	};
	const auto it = names.find(id);
	return it == names.end() ? id : it->second;
}

// Reading order of the shipped HUD (operator-specified 2026-11-10): right column top-down,
// then the navigation row left-to-right, then the two overlays, then the media viewport.
int readingOrderOf(const std::string& id) {
	static const std::vector<std::string> order = {
		"universal.scene_title", "universal.media_title", "universal.primary_state", "universal.activity",
		"universal.effect_summary", "universal.timing", "universal.activity_trace", "universal.secondary_trace",
		"flexible.primary", "flexible.secondary_a", "flexible.secondary_b",
		"controls.scene_previous", "controls.scene_next", "controls.mode", "controls.media_previous",
		"controls.media_next", "controls.fullscreen", "controls.menu",
		"overlay.health", "overlay.transition", "media_viewport",
	};
	const auto it = std::find(order.begin(), order.end(), id);
	return it == order.end() ? int(order.size()) : int(it - order.begin());
}

ofColor scopeColor(const std::string& scope, HudRegionRole role, int alpha) {
	if (scope == kScopeFullOnly) return ofColor(230, 150, 40, alpha);
	if (scope == kScopeLivingHud) return ofColor(150, 200, 60, alpha);
	if (scope == kScopeClock) return ofColor(80, 150, 240, alpha);
	if (scope == kScopeNavigation) return ofColor(235, 215, 70, alpha);
	if (scope == kScopeVitals) return ofColor(240, 120, 120, alpha);
	if (role == HudRegionRole::Overlay) return ofColor(200, 90, 200, alpha);
	return ofColor(40, 170, 160, alpha);
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
	outDir_ = (dir && dir[0]) ? std::string(dir) : ofToDataPath("../../../../docs/hud-art/blueprint-v3-reference", true);
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
	// Export order = HUD reading order (see readingOrderOf), not catalog order.
	auto regions = r.regionCatalog().all();
	std::stable_sort(regions.begin(), regions.end(), [&](const auto& a, const auto& b) {
		return readingOrderOf(a.regionId) < readingOrderOf(b.regionId);
	});

	check("no skin active (comparison frame is the plain wireframe)", !bridge.skinLoader().active().has_value());
	check("catalog has 21 regions (HUD-006 v2 + MENU)", regions.size() == 21, ofToString(regions.size()));
	int originalCount = 0, productionCount = 0, livingCount = 0, clockCount = 0, navCount = 0, vitalsCount = 0;
	for (const auto& reg : regions) {
		const std::string scope = scopeOf(reg.regionId);
		originalCount += scope == kScopeMesh || scope == kScopeOriginal;
		productionCount += scope != kScopeFullOnly;
		livingCount += scope == kScopeLivingHud;
		clockCount += scope == kScopeClock;
		navCount += scope == kScopeNavigation;
		vitalsCount += scope == kScopeVitals;
	}
	const int fullOnlyCount = int(regions.size()) - productionCount;
	check("historical MinimalSlice part has 12 regions (v1 13 minus retired controls.reseed)", originalCount == 12, ofToString(originalCount));
	check("production presentation has 21 regions (12 + 2 Living HUD + clock + mode panel/MODE/FULLSCREEN/MENU + 2 System Telemetry v1 traces)",
		productionCount == 21 && livingCount == 2 && clockCount == 1 && navCount == 4 && vitalsCount == 2, ofToString(productionCount));
	check("controls.reseed retired; controls.mode and controls.fullscreen present",
		r.regionCatalog().find("controls.reseed") == nullptr && r.regionCatalog().find("controls.mode") != nullptr
			&& r.regionCatalog().find("controls.fullscreen") != nullptr);
	check("flexible.secondary_a = navigation-scope region (active time), flexible.secondary_b = Clock",
		scopeOf("flexible.secondary_a") == kScopeNavigation && scopeOf("flexible.secondary_b") == kScopeClock);

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
		ofColor fill = scopeColor(scope, reg.role, 110);
		beginOverBlend(); // ofDrawBitmapString() (previous label) changes GL blend state
		ofFill();
		if (reg.regionId == "media_viewport") { ofSetColor(70, 80, 95, 255); mesh.draw(); }
		else { ofSetColor(fill); ofDrawRectangle(b); }
		ofNoFill();
		ofSetColor(235, 240, 235, 255);
		if (reg.regionId == "media_viewport") drawOutline(outline); else ofDrawRectangle(b);
		const std::string tag = "#" + ofToString(index);
		const std::string named = tag + " " + displayNameOf(reg.regionId);
		const std::string label = (b.width >= 8.0f * named.size() + 6.0f) ? named : tag;
		if (reg.regionId != "media_viewport") ofDrawBitmapString(label, b.x + 3, b.y + 13);
		legend.push_back(ofToString(index) + (index < 10 ? "  " : " ") + reg.regionId +
			std::string(std::max<size_t>(1, 26 - reg.regionId.size()), ' ') + rectText(b) + "  " + displayNameOf(reg.regionId));
		index++;
	}
	ofSetColor(235, 240, 235, 255);
	float ly = vp.y + 22;
	ofDrawBitmapString("ecopunk-hud-blueprint / 2 — " + ofToString(regions.size()) + " canonical regions (px, 1280x720; from HudRegionCatalog)", vp.x + 10, ly);
	ly += 18;
	for (const auto& l : legend) { ofDrawBitmapString(l, vp.x + 10, ly); ly += 15; }
	ly += 6;
	ofSetColor(40, 170, 160, 255);
	ofDrawBitmapString("teal = historical MinimalSlice (" + ofToString(originalCount) + ")", vp.x + 10, ly);
	ofSetColor(200, 90, 200, 255); ofDrawBitmapString("magenta = Overlay (in historical slice)", vp.x + 340, ly);
	ly += 15;
	ofSetColor(150, 200, 60, 255); ofDrawBitmapString("green = Living HUD v0.1 (+" + ofToString(livingCount) + ")", vp.x + 10, ly);
	ofSetColor(80, 150, 240, 255); ofDrawBitmapString("blue = HUD Clock v0.1 (+" + ofToString(clockCount) + ")", vp.x + 340, ly);
	ly += 15;
	ofSetColor(235, 215, 70, 255); ofDrawBitmapString("yellow = Mode Navigation v0.1 (+" + ofToString(navCount) + ")", vp.x + 10, ly);
	ly += 16;
	ofSetColor(240, 120, 120, 255); ofDrawBitmapString("red = System Telemetry v1 (+" + ofToString(vitalsCount) + ")", vp.x + 10, ly);
	ofSetColor(230, 150, 40, 255);
	ofDrawBitmapString("amber = FullProfile only (" + ofToString(fullOnlyCount) + ")", vp.x + 340, ly);
	ly += 15;
	ofSetColor(235, 240, 235, 255);
	ofDrawBitmapString("current production presentation = " + ofToString(productionCount) + " regions", vp.x + 10, ly);
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

	// -- 4. Current production presentation ------------------------------------------------
	// Only what production draws: the 17 production regions filled by
	// provenance; the FullProfile-only regions outlined grey and marked as
	// not drawn. Distinguishes current production from the historical 13.
	fbo.begin();
	ofClear(16, 18, 20, 255);
	ofPushStyle();
	int pIndex = 0;
	for (const auto& reg : regions) {
		pIndex++;
		const ofRectangle b = r.pixelBoundsFor(reg.regionId);
		const std::string scope = scopeOf(reg.regionId);
		const std::string tag = "#" + ofToString(pIndex);
		beginOverBlend();
		if (reg.regionId == "media_viewport") {
			ofFill(); ofSetColor(70, 80, 95, 255); mesh.draw();
			ofNoFill(); ofSetColor(235, 240, 235, 255); drawOutline(outline);
			ofDrawBitmapString(tag + " " + displayNameOf(reg.regionId), vp.x + 10, vp.getBottom() - 12);
			continue;
		}
		const bool production = scope != kScopeFullOnly;
		ofFill();
		ofSetColor(production ? scopeColor(scope, reg.role, 120) : ofColor(60, 60, 60, 90));
		ofDrawRectangle(b);
		ofNoFill();
		ofSetColor(production ? ofColor(235, 240, 235, 255) : ofColor(130, 130, 130, 255));
		ofDrawRectangle(b);
		std::string label = tag + " " + displayNameOf(reg.regionId);
		if (!production) label += " (not drawn)";
		if (b.width < 8.0f * label.size() + 6.0f) label = tag; // narrow squares: number only
		ofDrawBitmapString(label, b.x + 3, b.y + 13);
	}
	ofSetColor(235, 240, 235, 255);
	float py = vp.y + 22;
	ofDrawBitmapString("CURRENT PRODUCTION PRESENTATION - " + ofToString(productionCount) + " of " + ofToString(regions.size())
		+ " regions (from the live allowlist)", vp.x + 10, py); py += 18;
	ofDrawBitmapString("numbered in HUD reading order: right column top-down (1-11), nav row (12-18),", vp.x + 10, py); py += 15;
	ofDrawBitmapString("overlays (19-20), media viewport (21). Names are PROPOSED; region IDs are unchanged.", vp.x + 10, py); py += 18;
	ofDrawBitmapString("grey = FullProfile only, NOT drawn in production (" + ofToString(fullOnlyCount) + ")", vp.x + 10, py); py += 24;
	ofDrawBitmapString("All text is live runtime output: never bake it into skin art.", vp.x + 10, py);
	ofPopStyle();
	fbo.end();
	ofPixels production = readFbo(fbo); fix(production);

	// -- Write + verify --------------------------------------------------------------------------
	// Overlay references (wireframe, constraints) must keep alpha so they can be
	// layered over artwork. The schematic is opaque by design; FreeImage writes
	// a fully opaque image as an RGB PNG, which is expected for that one.
	struct Out { std::string name; ofPixels* px; size_t channels; };
	const Out outs[] = {
		{"hud_reference_wireframe_1280x720.png", &wire, 4},
		{"hud_reference_schematic_1280x720.png", &schematic, 3},
		{"hud_reference_constraints_1280x720.png", &constraints, 4},
		{"hud_reference_production_1280x720.png", &production, 3},
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
	j["source"] = "derived from live HudWireframeRenderer/HudRegionCatalog at export time; HUD-006 ecopunk-hud-blueprint / 2";
	j["blueprintId"] = "ecopunk-hud-blueprint";
	j["blueprintVersion"] = 2;
	j["canvas"] = {{"width", kW}, {"height", kH}};
	j["presentation"] = {{"canonicalRegions", regions.size()}, {"historicalMinimalSlice", originalCount},
		{"production", productionCount},
		{"productionParts", {{"historicalMinimalSlice", originalCount}, {"livingHud", livingCount}, {"clock", clockCount},
			{"modeNavigation", navCount}, {"systemTelemetry", vitalsCount}}},
		{"fullProfileOnly", fullOnlyCount}};
	j["mediaViewportMesh"] = {{"cornerRadius", 18}, {"bevel", 42}, {"roundedCorners", {"top-left", "top-right", "bottom-left"}},
		{"bevelCorner", "bottom-right"}, {"outlineVertexCount", outline.size()}};
	int n = 1;
	for (const auto& reg : regions) {
		const ofRectangle b = r.pixelBoundsFor(reg.regionId);
		j["regions"].push_back({{"index", n++}, {"id", reg.regionId}, {"displayName", displayNameOf(reg.regionId)}, {"role", roleName(reg.role)},
			{"normalized", {reg.bounds.x, reg.bounds.y, reg.bounds.width, reg.bounds.height}},
			{"px", {b.x, b.y, b.width, b.height}}, {"scope", scopeOf(reg.regionId)},
			{"production", scopeOf(reg.regionId) != kScopeFullOnly}});
	}
	// Derived sprite rectangle for the optional NEXT SCENE pressed artwork
	// (HUD-006 v2 D3): region + 4 px bleed, snapped outward.
	{
		const ofRectangle b = r.pixelBoundsFor("controls.scene_next");
		const auto sp = hudpresent::reactive::pressedSpriteRect(b.x, b.y, b.width, b.height);
		j["reactiveArtwork"] = {{"controls.scene_next", {{"file", "controls/scene_next/pressed.png"},
			{"spriteRectPx", {sp.x, sp.y, sp.width, sp.height}}, {"rule", "region expanded 4 px per side, snapped outward"}}}};
	}
	// Shared navigation-button faces: control region + 4 px bleed per side,
	// centred and rounded to whole pixels; one size, six rectangles.
	{
		ofJson rects = ofJson::object();
		int fw = 0, fh = 0;
		for (const auto& reg : regions) {
			if (reg.role != hudpresent::HudRegionRole::Controls) continue;
			const ofRectangle b = r.pixelBoundsFor(reg.regionId);
			const auto f = hudpresent::reactive::buttonFaceRect(b.x, b.y, b.width, b.height);
			fw = f.width; fh = f.height;
			rects[reg.regionId] = {f.x, f.y, f.width, f.height};
		}
		j["buttonFace"] = {{"rule", "control region + 4 px bleed per side, centred, rounded to whole canvas pixels, drawn unscaled"},
			{"size", {fw, fh}},
			{"files", {"controls/button/idle.png", "controls/button/focused.png", "controls/button/active.png"}},
			{"rectsPx", rects}};
	}
	ofSavePrettyJson(fs::path(outDir_) / "regions.json", j);
	note("wrote regions.json (" + ofToString(regions.size()) + " regions)");

	// Production membership table.
	{
		std::ofstream md(fs::path(outDir_) / "production_membership.md");
		md << "# HUD-006 v2 production membership (generated from HudRegionCatalog; numbered in HUD reading order)\n\n";
		md << "| # | Region | Proposed name | Role | px (x, y, w, h) | Scope |\n|---|---|---|---|---|---|\n";
		int k = 1;
		for (const auto& reg : regions) {
			const ofRectangle b = r.pixelBoundsFor(reg.regionId);
			md << "| " << k++ << " | `" << reg.regionId << "` | `" << displayNameOf(reg.regionId) << "` | " << roleName(reg.role) << " | " << rectText(b) << " | " << scopeOf(reg.regionId) << " |\n";
		}
		md << "\nCanonical " << regions.size() << ", production " << productionCount << ", FullProfile only " << fullOnlyCount << ".\n";
		note("wrote production_membership.md");
	}

	// v1 -> v2 crosswalk, computed against the committed v1 export (no hand-written geometry).
	{
		ofJson v1;
		try { v1 = ofLoadJson(fs::path(outDir_).parent_path() / "blueprint-v1-reference" / "regions.json"); } catch (...) {}
		std::map<std::string, ofJson> v1px;
		if (v1.is_object() && v1.contains("regions")) for (const auto& e : v1["regions"]) v1px[e.value("id", std::string())] = e["px"];
		ofJson cw;
		cw["from"] = "ecopunk-hud-blueprint / 1 (docs/hud-art/blueprint-v1-reference/regions.json)";
		cw["to"] = "ecopunk-hud-blueprint / 2 (this folder's regions.json)";
		std::set<std::string> v2ids;
		for (const auto& reg : regions) {
			v2ids.insert(reg.regionId);
			const ofRectangle b = r.pixelBoundsFor(reg.regionId);
			const ofJson now = {b.x, b.y, b.width, b.height};
			auto it = v1px.find(reg.regionId);
			std::string status = it == v1px.end() ? "added" : "unchanged";
			if (it != v1px.end()) {
				for (int i = 0; i < 4; ++i) if (std::abs(it->second[i].get<float>() - now[i].get<float>()) > 0.01f) status = "moved/resized";
			}
			ofJson e = {{"id", reg.regionId}, {"status", status}, {"v2px", now}};
			if (it != v1px.end()) e["v1px"] = it->second;
			cw["regions"].push_back(e);
		}
		for (const auto& [id, px] : v1px) {
			if (!v2ids.count(id)) cw["regions"].push_back({{"id", id}, {"status", "retired"}, {"v1px", px},
				{"note", id == "controls.reseed" ? "IMMERSIVE moved to keyboard 'i'; FULLSCREEN is controls.fullscreen" : ""}});
		}
		cw["behaviour"] = {{"flexible.secondary_a", "active time (was the informational mode panel; the one-line mode text now lives in universal.media_title; NEXT MODE -> controls.mode)"},
			{"focusRing", {"controls.scene_previous", "controls.scene_next", "controls.mode", "controls.media_previous",
				"controls.media_next", "controls.fullscreen", "controls.menu"}}};
		check("crosswalk computed against the v1 export", !v1px.empty(), ofToString(v1px.size()) + " v1 regions");
		ofSavePrettyJson(fs::path(outDir_) / "crosswalk-v1-v2.json", cw);
		note("wrote crosswalk-v1-v2.json");
	}
	note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
}
