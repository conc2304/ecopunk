#include "BlobRenderPathAbHarness.h"

#include "BlobSceneCore.h"
#include "ShaderLibrary.h"
#include "VideoRegionEffectRenderer.h"
#include "ofMain.h"

#include <cstdlib>
#include <vector>

namespace {

	constexpr int kW = 1280;
	constexpr int kH = 720;
	constexpr const char * kLog = "BlobRenderPathAbHarness";
	constexpr const char * kProductionEffect = "desaturate";
	// A controlled real Blob scene-only frame, relative to bin/data.
	constexpr const char * kSourceImage = "../../../../docs/reviews/blob-parity-captures-controlled/runtime_scene_only.png";

	struct Frag {
		ofRectangle dest;
		float alpha = 1.0f;
		float amount = 1.0f;
	};

	struct Case {
		std::string name;
		std::vector<Frag> frags;
		// Inputs that keep the 3-stage path on both sides — must match exactly.
		bool expectSamePath = false;
		bool fractionalBounds = false;
	};

	struct DiffStats {
		int maxRgb = 0;
		int maxA = 0;
		double meanRgb = 0.0;
		double pctRgbOver8 = 0.0;
		double pctAOver8 = 0.0;
		int maxAlphaLostByDirect = 0; // max(legacyA - directA)
	};

	std::vector<Case> buildCases() {
		auto f = [](float x, float y, float w, float h, float alpha = 1.0f, float amount = 1.0f) {
			return Frag { ofRectangle(x, y, w, h), alpha, amount };
		};
		std::vector<Case> cases;
		cases.push_back({ "single", { f(100, 100, 200, 150) } });
		cases.push_back({ "multiple", { f(60, 80, 180, 120), f(400, 300, 220, 160), f(800, 100, 160, 200), f(1000, 450, 200, 180) } });
		cases.push_back({ "overlap", { f(300, 200, 300, 220), f(420, 280, 300, 220), f(360, 340, 260, 200) } });
		cases.push_back({ "large", { f(40, 30, 1200, 660) } });
		cases.push_back({ "edge", { f(-60, -40, 220, 180), f(1180, 620, 200, 160) } });
		cases.push_back({ "fractional", { f(123.4f, 77.6f, 201.7f, 149.3f), f(510.25f, 333.75f, 180.5f, 120.5f) }, false, true });

		Case ten { "ten_fragments", {}, false, true };
		for (int i = 0; i < 10; i++) {
			float x = 40.0f + (i % 5) * 235.3f;
			float y = 60.0f + (i / 5) * 290.7f + (i % 2) * 37.1f;
			ten.frags.push_back(f(x, y, 260.0f + i * 7.3f, 210.0f + i * 5.9f));
		}
		cases.push_back(ten);

		cases.push_back({ "fading_overlap", { f(300, 200, 300, 220, 0.5f), f(420, 280, 300, 220, 0.25f), f(360, 340, 260, 200, 0.8f) }, true });
		cases.push_back({ "reduced_amount", { f(100, 100, 200, 150, 1.0f, 0.5f) }, true });
		return cases;
	}

	bool glStateIsBaseline(const ofFbo & target, std::string & why) {
		GLint program = 0, fbo = 0, src = 0, dst = 0, vp[4] = { 0, 0, 0, 0 };
		glGetIntegerv(GL_CURRENT_PROGRAM, &program);
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
		glGetIntegerv(GL_BLEND_SRC_RGB, &src);
		glGetIntegerv(GL_BLEND_DST_RGB, &dst);
		glGetIntegerv(GL_VIEWPORT, vp);
		if (program != 0) why += " program=" + ofToString(program);
		if (fbo != static_cast<GLint>(target.getId())) why += " fbo=" + ofToString(fbo);
		if (!glIsEnabled(GL_BLEND) || src != GL_SRC_ALPHA || dst != GL_ONE_MINUS_SRC_ALPHA) why += " blend";
		if (vp[0] != 0 || vp[1] != 0 || vp[2] != kW || vp[3] != kH) why += " viewport";
		return why.empty();
	}

	bool renderCase(ofFbo & fbo, VideoRegionEffectRenderer & renderer, ShaderLibrary & lib, const ofTexture & src,
		const Case & c, const std::string & effect, bool opaqueBackground, std::string & glIssue) {
		fbo.begin();
		ofClear(0, 0, 0, 0);
		ofEnableBlendMode(OF_BLENDMODE_ALPHA);
		if (opaqueBackground) {
			ofSetColor(255);
			src.draw(0, 0, kW, kH);
		}
		for (const Frag & fr : c.frags) {
			VideoRegionEffectRenderer::RenderRequest req;
			req.sourceTexture = &src;
			req.sourceCropPixels = fr.dest; // source-aligned overlay; source is scene-sized
			req.destinationBounds = fr.dest;
			req.alpha = fr.alpha;
			req.effectName = effect;
			req.effectAmount = fr.amount;
			renderer.render(req, lib);
		}
		bool ok = glStateIsBaseline(fbo, glIssue);
		fbo.end();
		return ok;
	}

	DiffStats diff(const ofPixels & a, const ofPixels & b, ofPixels * amplified) {
		DiffStats s;
		size_t n = a.getWidth() * a.getHeight();
		size_t overRgb = 0, overA = 0;
		double sum = 0.0;
		const unsigned char * pa = a.getData();
		const unsigned char * pb = b.getData();
		for (size_t i = 0; i < n; i++) {
			int dr = std::abs(pa[i * 4 + 0] - pb[i * 4 + 0]);
			int dg = std::abs(pa[i * 4 + 1] - pb[i * 4 + 1]);
			int db = std::abs(pa[i * 4 + 2] - pb[i * 4 + 2]);
			int da = std::abs(pa[i * 4 + 3] - pb[i * 4 + 3]);
			int d = std::max(dr, std::max(dg, db));
			s.maxRgb = std::max(s.maxRgb, d);
			s.maxA = std::max(s.maxA, da);
			s.maxAlphaLostByDirect = std::max(s.maxAlphaLostByDirect, pb[i * 4 + 3] - pa[i * 4 + 3]);
			sum += (dr + dg + db) / 3.0;
			if (d > 8) overRgb++;
			if (da > 8) overA++;
			if (amplified != nullptr) {
				unsigned char v = static_cast<unsigned char>(std::min(255, std::max(d, da) * 8));
				amplified->getData()[i * 3 + 0] = v;
				amplified->getData()[i * 3 + 1] = v;
				amplified->getData()[i * 3 + 2] = v;
			}
		}
		s.meanRgb = sum / n;
		s.pctRgbOver8 = 100.0 * overRgb / n;
		s.pctAOver8 = 100.0 * overA / n;
		return s;
	}

}

bool BlobRenderPathAbHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_BLOB_RENDER_AB_HARNESS") != nullptr;
}

bool BlobRenderPathAbHarness::run() {
	ofImage source;
	if (!source.load(kSourceImage)) {
		ofLogError(kLog) << "could not load source image " << kSourceImage;
		ofLogNotice(kLog) << "RESULT: FAIL";
		return false;
	}

	// Freeze ofGetElapsedTimef() (fixed-rate mode only advances per frame,
	// and this whole run is inside one setup() call) so time-animated
	// effects get the same "time" uniform on both sides.
	ofSetTimeModeFixedRate();

	ShaderLibrary lib;
	lib.setup();

	VideoRegionEffectRenderer direct;
	VideoRegionEffectRenderer legacy;
	legacy.setDirectOutputEnabled(false);

	ofFbo fboDirect, fboLegacy;
	fboDirect.allocate(kW, kH, GL_RGBA);
	fboLegacy.allocate(kW, kH, GL_RGBA);

	ofFilePath::createEnclosingDirectory("captures/blob_render_ab/x.png");

	int checks = 0;
	int failures = 0;
	auto check = [&](bool ok, const std::string & what) {
		checks++;
		if (!ok) {
			failures++;
			ofLogError(kLog) << "FAIL: " << what;
		}
	};

	std::vector<Case> cases = buildCases();
	for (const std::string & effect : BlobSceneCore::effectChoices()) {
		check(lib.has(effect), effect + " loaded");
		for (const Case & c : cases) {
			for (bool opaque : { true, false }) {
				std::string tag = effect + "/" + c.name + (opaque ? "/opaque_bg" : "/transparent_bg");

				direct.resetStageTimings();
				legacy.resetStageTimings();
				std::string glDirect, glLegacy;
				check(renderCase(fboDirect, direct, lib, source.getTexture(), c, effect, opaque, glDirect), tag + " GL baseline after direct:" + glDirect);
				check(renderCase(fboLegacy, legacy, lib, source.getTexture(), c, effect, opaque, glLegacy), tag + " GL baseline after legacy:" + glLegacy);

				int n = static_cast<int>(c.frags.size());
				const auto & sd = direct.getStageTimings();
				const auto & sl = legacy.getStageTimings();
				check(sl.legacyDraws == n && sl.directDraws == 0, tag + " legacy renderer took 3-stage path only");
				if (c.expectSamePath) {
					check(sd.legacyDraws == n && sd.directDraws == 0, tag + " fade/reduced-amount falls back to 3-stage path");
				} else {
					check(sd.directDraws == n && sd.legacyDraws == 0, tag + " direct renderer took direct path");
				}

				ofPixels pa, pb;
				fboDirect.readToPixels(pa);
				fboLegacy.readToPixels(pb);
				bool saveImages = (effect == kProductionEffect && opaque) || (effect == "ascii_solarpunk" && opaque && c.name == "ten_fragments");
				ofPixels amplified;
				if (saveImages) {
					amplified.allocate(kW, kH, OF_PIXELS_RGB);
				}
				DiffStats s = diff(pa, pb, saveImages ? &amplified : nullptr);

				ofLogNotice(kLog) << "DIFF " << tag << " maxRgb=" << s.maxRgb << " maxA=" << s.maxA
								  << " meanRgb=" << s.meanRgb << " pctRgb>8=" << s.pctRgbOver8 << " pctA>8=" << s.pctAOver8
								  << " alphaLostByDirect=" << s.maxAlphaLostByDirect;

				if (c.expectSamePath) {
					check(s.maxRgb == 0 && s.maxA == 0, tag + " identical (both 3-stage)");
				} else if (effect == kProductionEffect) {
					// Bounds fixed before the first run: integer-aligned
					// fragments map 1:1 in both paths; fractional ones only
					// differ by the old path's extra bilinear resample.
					// First run disproved "maxA <= 2" for fractional bounds:
					// the 3-stage composite samples one texel past the
					// shader's w x h output on the bottom/right edge, leaving
					// a 1px translucent seam (scene alpha ~214-232) that the
					// direct path doesn't have. So: RGB stays within
					// resample tolerance, and the direct path is never less
					// opaque than the 3-stage one.
					if (c.fractionalBounds) {
						check(s.meanRgb <= 1.0 && s.pctRgbOver8 <= 0.1, tag + " production effect RGB within resample tolerance");
						check(s.maxAlphaLostByDirect <= 2, tag + " direct path never less opaque than 3-stage");
					} else {
						check(s.maxRgb <= 2 && s.maxA <= 2, tag + " production effect near-identical");
					}
				}

				if (saveImages) {
					std::string base = "captures/blob_render_ab/" + effect + "_" + c.name;
					ofSaveImage(pa, base + "_direct.png");
					ofSaveImage(pb, base + "_legacy.png");
					ofSaveImage(amplified, base + "_diff_x8.png");
				}
			}
		}
	}

	ofLogNotice(kLog) << "RESULT: " << (checks - failures) << "/" << checks << " checks passed — "
					  << (failures == 0 ? "PASS" : "FAIL");
	return failures == 0;
}
