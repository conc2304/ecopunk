#include "SkinLoaderHarness.h"

#include "HudSkinPng.h"

#include "ofAppRunner.h"
#include "ofFbo.h"
#include "ofGraphics.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#ifdef __APPLE__
#include <mach/mach.h>
#endif

namespace fs = std::filesystem;
namespace loadcodes = hudskin::loadcodes;
namespace codes = hudskin::codes;
namespace pngcodes = hudskin::pngcodes;
using hudskin::SkinLoadStage;

namespace {

// Fixture constants — must match test/skin_fixtures/make_skin_fixtures.py.
constexpr int kRampX0 = 300, kRampX1 = 1000, kRampY = 700;
constexpr int kBackground = 12; // ExperienceRuntime::draw(): ofBackground(12, 12, 12)

size_t residentBytes() {
#ifdef __APPLE__
	mach_task_basic_info info{};
	mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
		return size_t(info.resident_size);
	}
#endif
	return 0;
}

std::string mb(size_t bytes) {
	return ofToString(double(bytes) / (1024.0 * 1024.0), 2) + " MiB";
}

// Straight-alpha source-over of colour c with alpha a (0..255) over backdrop b.
double sourceOver(int c, int a, int b) {
	const double af = a / 255.0;
	return c * af + b * (1.0 - af);
}

int rampAlpha(int x) {
	return int(std::lround(255.0 * (x - kRampX0) / double(kRampX1 - kRampX0 - 1)));
}

bool regionPixelsEqual(const ofPixels& a, const ofPixels& b, int x0, int y0, int x1, int y1) {
	for (int y = y0; y < y1; ++y) {
		for (int x = x0; x < x1; ++x) {
			for (size_t c = 0; c < 3; ++c) {
				if (a.getColor(x, y)[c] != b.getColor(x, y)[c]) return false;
			}
		}
	}
	return true;
}

} // namespace

bool SkinLoaderHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_SKIN_HARNESS") != nullptr;
}

SkinLoaderHarness::SkinLoaderHarness(ExperienceRuntime& runtime) : runtime_(runtime) {
	canonicalRoot_ = runtime_.hudCompositorBridgeForTesting().skinLoader().config().skinsRoot.string();
	scratchRoot_ = ofToDataPath("skin-harness/skins", true);

	std::error_code ec;
	fs::remove_all(ofToDataPath("skin-harness", true), ec);
	fs::create_directories(ofToDataPath("captures/skin-harness", true));
	fs::create_directories(fs::path(scratchRoot_).parent_path(), ec); // fs::copy creates only the leaf
	fs::copy(ofToDataPath("../../test/skin_fixtures/skins", true), scratchRoot_, fs::copy_options::recursive, ec);
	check("setup: fixture packages copied to scratch root", !ec, ec ? ec.message() : std::string{});

	// Runtime-only symlink-escape case: manifest valid, shell.png is a symlink
	// to a file OUTSIDE the package.
	const fs::path esc = fs::path(scratchRoot_) / "symlink-escape";
	fs::create_directories(esc, ec);
	fs::create_directories(fs::path(ofToDataPath("skin-harness/outside", true)), ec);
	fs::copy_file(fs::path(scratchRoot_) / "fixture-shell-proof" / "shell.png",
		ofToDataPath("skin-harness/outside/shell.png", true), fs::copy_options::overwrite_existing, ec);
	{
		std::ifstream src(fs::path(scratchRoot_) / "fixture-shell-proof" / "skin.json");
		std::string text((std::istreambuf_iterator<char>(src)), std::istreambuf_iterator<char>());
		const std::string from = "\"fixture-shell-proof\"";
		const size_t at = text.find(from);
		if (at != std::string::npos) text.replace(at, from.size(), "\"symlink-escape\"");
		std::ofstream(esc / "skin.json") << text;
		check("setup: symlink-escape manifest written", at != std::string::npos);
	}
	fs::create_symlink(ofToDataPath("skin-harness/outside/shell.png", true), esc / "shell.png", ec);
	check("setup: symlink-escape shell.png is a symlink to outside the package",
		!ec && fs::is_symlink(fs::symlink_status(esc / "shell.png")), ec ? ec.message() : std::string{});

	note("SkinLoaderHarness: canonical root " + canonicalRoot_);
	note("SkinLoaderHarness: scratch root   " + scratchRoot_);
	note("RSS at start: " + mb(residentBytes()));
}

void SkinLoaderHarness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	note(std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " — " + detail));
}

void SkinLoaderHarness::note(const std::string& line) {
	ofLogNotice("SkinLoaderHarness") << line;
	lines_.push_back(line);
}

void SkinLoaderHarness::renderFrame() {
	runtime_.draw();
}

ofPixels SkinLoaderHarness::capture(const std::string& name) {
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save("captures/skin-harness/" + name + ".png");
	note("capture: bin/data/captures/skin-harness/" + name + ".png (" + ofToString(img.getWidth()) + "x" + ofToString(img.getHeight()) + ")");
	return img.getPixels();
}

void SkinLoaderHarness::step(float /*dt*/) {
	renderFrame();
	auto& bridge = runtime_.hudCompositorBridgeMutableForTesting();

	if (frame_ == 60) {
		baseline_ = capture("skin_00_wireframe_baseline");
		check("baseline: no active skin at cold start (no selection)", !bridge.skinLoader().active().has_value());
		check("baseline: ramp area shows plain background (no shell)",
			std::abs(int(baseline_.getColor(kRampX0 + 600, kRampY).r) - kBackground) <= 1,
			"r=" + ofToString(int(baseline_.getColor(kRampX0 + 600, kRampY).r)));
		check("capture is canonical 1280x720", baseline_.getWidth() == 1280 && baseline_.getHeight() == 720,
			ofToString(baseline_.getWidth()) + "x" + ofToString(baseline_.getHeight()));
		note("RSS baseline (wireframe): " + mb(residentBytes()));

		runColdRejections();

		// Valid cold load from the CANONICAL root — the production configuration.
		bridge.setSkinsRootForTesting(canonicalRoot_);
		auto o = bridge.requestSkin("fixture-shell-proof");
		check("cold valid shell-only package activates from canonical root", o.activated && o.code == loadcodes::Activated,
			hudskin::stageName(o.stage) + std::string(" ") + o.code);
		const auto& active = bridge.skinLoader().active();
		textureIdA_ = active ? bridge.skinBackend().glTextureId(active->handle) : 0;
		check("one live skin texture after activation", bridge.skinBackend().liveTextureCount() == 1);
		check("GL texture A exists", textureIdA_ != 0 && glIsTexture(textureIdA_) == GL_TRUE, "id=" + ofToString(textureIdA_));
		note("loader steady GPU after A: " + mb(bridge.skinLoader().steadyGpuBytes()) +
			"; request peak GPU " + mb(o.peakGpuBytesDuringRequest) + "; request peak CPU " + mb(o.cpuPeakBytes));
	}
	if (frame_ == 120) {
		ofPixels shellFrame = capture("skin_01_shell_active");
		runBlendChecks(shellFrame);
		runBackdropBlendCheck();
		note("RSS steady (shell A active): " + mb(residentBytes()));
		runFailedReplacements();
	}
	if (frame_ == 150) {
		ofPixels px = capture("skin_02_after_failed_replacements");
		const ofColor tl = px.getColor(5, 5);
		check("after failed replacements: shell A still drawn (TL corner magenta)",
			std::abs(tl.r - int(std::lround(sourceOver(255, 230, kBackground)))) <= 3 && tl.g <= 4,
			"rgb=" + ofToString(int(tl.r)) + "," + ofToString(int(tl.g)) + "," + ofToString(int(tl.b)));
		runSuccessfulReplacement();
	}
	if (frame_ == 180) {
		ofPixels px = capture("skin_03_replaced_b");
		const ofColor tl = px.getColor(5, 5);
		const int expect = int(std::lround(sourceOver(255, 230, kBackground)));
		check("after replacement: shell B drawn (TL corner white)",
			std::abs(tl.r - expect) <= 3 && std::abs(tl.g - expect) <= 3 && std::abs(tl.b - expect) <= 3,
			"rgb=" + ofToString(int(tl.r)) + "," + ofToString(int(tl.g)) + "," + ofToString(int(tl.b)));
		note("RSS steady (shell B active): " + mb(residentBytes()));
		runShutdown();
	}
	if (frame_ == 200) {
		ofPixels px = capture("skin_04_after_shutdown_wireframe");
		check("after shutdown: ramp area back to plain background",
			std::abs(int(px.getColor(kRampX0 + 600, kRampY).r) - kBackground) <= 1);
		check("after shutdown: controls row identical to wireframe baseline",
			regionPixelsEqual(px, baseline_, 26, 649, 716, 691));
		note("RSS after shutdown: " + mb(residentBytes()));
		finish();
	}
	frame_++;
}

void SkinLoaderHarness::runColdRejections() {
	auto& bridge = runtime_.hudCompositorBridgeMutableForTesting();
	bridge.setSkinsRootForTesting(scratchRoot_);

	struct Case { std::string selection; SkinLoadStage stage; std::string code; };
	const std::vector<Case> cases = {
		{"", SkinLoadStage::Selection, loadcodes::NoSelection},
		{"does-not-exist", SkinLoadStage::PackageRoot, loadcodes::PackageNotFound},
		{"wrong-blueprint", SkinLoadStage::Manifest, codes::BlueprintIdMismatch},
		{"wrong-blueprint-version", SkinLoadStage::Manifest, codes::BlueprintVersionMismatch},
		{"path-escape", SkinLoadStage::Manifest, codes::PathParentTraversal},
		{"symlink-escape", SkinLoadStage::Manifest, codes::PathSymlink},
		{"missing-shell", SkinLoadStage::Manifest, codes::PathNotFound},
		{"shell-undeclared", SkinLoadStage::Manifest, codes::AssetRequiredMissing},
		{"id-mismatch", SkinLoadStage::Manifest, loadcodes::SkinIdMismatch},
		{"decoration-declared", SkinLoadStage::Capability, loadcodes::UnsupportedClass},
		{"emissive-declared", SkinLoadStage::Capability, loadcodes::UnsupportedClass},
		{"not-png", SkinLoadStage::ImageStructure, pngcodes::SignatureInvalid},
		{"corrupt-png", SkinLoadStage::ImageStructure, pngcodes::CrcMismatch},
		{"truncated-png", SkinLoadStage::ImageStructure, pngcodes::Truncated},
		{"rgb-png", SkinLoadStage::ImageStructure, pngcodes::NotRgba8},
		{"gray-alpha-png", SkinLoadStage::ImageStructure, pngcodes::NotRgba8},
		{"indexed-png", SkinLoadStage::ImageStructure, pngcodes::NotRgba8},
		{"sixteen-bit-png", SkinLoadStage::ImageStructure, pngcodes::NotRgba8},
		{"wrong-size", SkinLoadStage::ImageStructure, loadcodes::ImageWrongSize},
		// Structurally valid PNG whose zlib stream is damaged: reaches the REAL
		// FreeImage decoder, which must fail it.
		{"corrupt-stream", SkinLoadStage::ImageDecode, loadcodes::DecodeFailed},
	};
	for (const auto& c : cases) {
		auto o = bridge.requestSkin(c.selection);
		const bool ok = !o.activated && o.stage == c.stage && o.code == c.code && !o.activeSkinIdAfter;
		check("cold reject '" + c.selection + "' → " + c.code, ok,
			std::string("stage=") + hudskin::stageName(o.stage) + " code=" + o.code + (o.asset.empty() ? "" : " asset=" + o.asset));
	}

	// Real GPU-upload failure path (deterministic injection after a real decode).
	bridge.skinBackendForTesting().failNextUploadForTesting();
	auto up = bridge.requestSkin("fixture-shell-proof");
	check("cold reject upload failure (injected) → wireframe", !up.activated && up.stage == SkinLoadStage::GpuUpload &&
		up.code == loadcodes::UploadFailed && !up.activeSkinIdAfter, up.detail);

	check("cold rejections: still no active skin (wireframe)", !bridge.skinLoader().active().has_value());
	check("cold rejections: zero live skin textures (candidates released)", bridge.skinBackend().liveTextureCount() == 0);
	check("cold rejections: renderer has no shell texture", bridge.renderer().shellTexture() == nullptr);
}

void SkinLoaderHarness::runBlendChecks(const ofPixels& px) {
	// Live framebuffer, production path: white ramp with straight alpha over
	// the runtime background. Correct straight-alpha source-over gives
	// c·a + b·(1−a); a double-premultiplied path would give c·a² + b·(1−a).
	int worst = 0;
	int worstPremul = 0;
	for (int x = kRampX0 + 10; x < kRampX1 - 10; x += 35) {
		const int a = rampAlpha(x);
		const int got = px.getColor(x, kRampY).r;
		worst = std::max(worst, std::abs(got - int(std::lround(sourceOver(255, a, kBackground)))));
		const double premul = 255.0 * (a / 255.0) * (a / 255.0) + kBackground * (1.0 - a / 255.0);
		worstPremul = std::max(worstPremul, std::abs(got - int(std::lround(premul))));
	}
	check("live blend: ramp matches straight-alpha source-over within ±2", worst <= 2, "max error " + ofToString(worst));
	check("live blend: ramp does NOT match double-premultiplied result", worstPremul > 10, "max deviation " + ofToString(worstPremul));

	const ofColor tl = px.getColor(5, 5);
	check("live blend: TL corner = magenta(255,0,200)@230 over bg",
		std::abs(tl.r - int(std::lround(sourceOver(255, 230, kBackground)))) <= 2 &&
		std::abs(tl.g - int(std::lround(sourceOver(0, 230, kBackground)))) <= 2 &&
		std::abs(tl.b - int(std::lround(sourceOver(200, 230, kBackground)))) <= 2,
		"rgb=" + ofToString(int(tl.r)) + "," + ofToString(int(tl.g)) + "," + ofToString(int(tl.b)));
	const ofColor bl = px.getColor(5, 715);
	check("orientation: BL corner = cyan(0,200,255) (capture not flipped)", bl.b > 200 && bl.r < 20,
		"rgb=" + ofToString(int(bl.r)) + "," + ofToString(int(bl.g)) + "," + ofToString(int(bl.b)));

	// Shell must leave protected live content untouched: the static controls
	// row is pixel-identical to the wireframe baseline.
	check("protected regions: controls row identical to wireframe baseline", regionPixelsEqual(px, baseline_, 26, 649, 716, 691));
	// The hatch stripe sits in the viewport/right-column gutter, outside every region.
	const ofColor hatch = px.getColor(810, 298);
	const ofColor hatchBase = baseline_.getColor(810, 298);
	check("shell visible in gutter (hatch differs from baseline)", hatch != hatchBase,
		"shell rgb=" + ofToString(int(hatch.r)) + "," + ofToString(int(hatch.g)) + "," + ofToString(int(hatch.b)));
}

void SkinLoaderHarness::runBackdropBlendCheck() {
	// Auxiliary: the active shell texture drawn with the renderer's exact
	// state (OF_BLENDMODE_ALPHA, untinted) over opaque black and opaque white
	// backdrops — the spec §4 transparent-edge QA fixture against both.
	auto& bridge = runtime_.hudCompositorBridgeMutableForTesting();
	const ofTexture* tex = bridge.renderer().shellTexture();
	if (tex == nullptr) { check("backdrop blend: shell texture available", false); return; }

	ofFbo fbo;
	fbo.allocate(1280, 720, GL_RGBA);
	for (int backdrop : {0, 255}) {
		fbo.begin();
		ofClear(backdrop, backdrop, backdrop, 255);
		ofPushStyle();
		ofEnableBlendMode(OF_BLENDMODE_ALPHA);
		ofSetColor(255, 255, 255, 255);
		tex->draw(0, 0, 1280, 720);
		ofPopStyle();
		fbo.end();
		ofPixels px;
		fbo.readToPixels(px);
		int worst = 0;
		for (int x = kRampX0 + 10; x < kRampX1 - 10; x += 35) {
			const int a = rampAlpha(x);
			// FBO readback origin can differ from the screen grab; sample the
			// ramp row in both orientations and take the one that matches.
			const int rowA = kRampY, rowB = 719 - kRampY;
			const int expect = int(std::lround(sourceOver(255, a, backdrop)));
			const int e1 = std::abs(px.getColor(x, rowA).r - expect);
			const int e2 = std::abs(px.getColor(x, rowB).r - expect);
			worst = std::max(worst, std::min(e1, e2));
		}
		check("backdrop blend over " + std::string(backdrop == 0 ? "black" : "white") + ": source-over within ±2",
			worst <= 2, "max error " + ofToString(worst));
	}
}

void SkinLoaderHarness::runFailedReplacements() {
	auto& bridge = runtime_.hudCompositorBridgeMutableForTesting();
	const uint64_t handleBefore = bridge.skinLoader().active()->handle;
	bridge.setSkinsRootForTesting(scratchRoot_);

	for (const char* bad : {"corrupt-png", "corrupt-stream", "wrong-blueprint", "emissive-declared", "symlink-escape"}) {
		auto o = bridge.requestSkin(bad);
		check(std::string("failed replacement '") + bad + "' retains active skin A",
			!o.activated && o.retainedPreviousActive && o.activeSkinIdAfter == std::string("fixture-shell-proof"),
			std::string("stage=") + hudskin::stageName(o.stage) + " code=" + o.code);
	}
	bridge.skinBackendForTesting().failNextUploadForTesting();
	auto up = bridge.requestSkin("fixture-shell-proof-b");
	check("failed replacement (injected upload failure) retains active skin A",
		!up.activated && up.stage == SkinLoadStage::GpuUpload && up.retainedPreviousActive, up.detail);

	check("failed replacements: same active handle", bridge.skinLoader().active()->handle == handleBefore);
	check("failed replacements: exactly one live texture (candidates released)", bridge.skinBackend().liveTextureCount() == 1);
	check("failed replacements: GL texture A still alive", glIsTexture(textureIdA_) == GL_TRUE);
	check("failed replacements: renderer still points at A",
		bridge.renderer().shellTexture() == bridge.skinBackend().texture(handleBefore));
	note("RSS after failed replacements: " + mb(residentBytes()));
}

void SkinLoaderHarness::runSuccessfulReplacement() {
	auto& bridge = runtime_.hudCompositorBridgeMutableForTesting();
	auto o = bridge.requestSkin("fixture-shell-proof-b");
	check("successful replacement activates B and releases A", o.activated && o.releasedPrevious &&
		o.activeSkinIdAfter == std::string("fixture-shell-proof-b"));
	textureIdB_ = bridge.skinBackend().glTextureId(bridge.skinLoader().active()->handle);
	check("replacement: old GL texture A deleted", glIsTexture(textureIdA_) == GL_FALSE, "id=" + ofToString(textureIdA_));
	check("replacement: GL texture B exists", textureIdB_ != 0 && glIsTexture(textureIdB_) == GL_TRUE);
	check("replacement: exactly one live texture at steady state", bridge.skinBackend().liveTextureCount() == 1);
	check("replacement: transient peak = A + B (2 x 3.52 MiB)", o.peakGpuBytesDuringRequest == size_t(2) * 1280 * 720 * 4,
		mb(o.peakGpuBytesDuringRequest));
	note("RSS after successful replacement: " + mb(residentBytes()));
}

void SkinLoaderHarness::runShutdown() {
	auto& bridge = runtime_.hudCompositorBridgeMutableForTesting();
	bridge.shutdownSkins();
	check("shutdown: no active skin", !bridge.skinLoader().active().has_value());
	check("shutdown: zero live skin textures", bridge.skinBackend().liveTextureCount() == 0);
	check("shutdown: GL texture B deleted", glIsTexture(textureIdB_) == GL_FALSE);
	check("shutdown: renderer has no shell texture", bridge.renderer().shellTexture() == nullptr);
	note("loader lifetime peak GPU " + mb(bridge.skinLoader().lifetimePeakGpuBytes()) +
		", lifetime peak CPU staging " + mb(bridge.skinLoader().lifetimePeakCpuBytes()));
}

void SkinLoaderHarness::finish() {
	note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
	std::ofstream out(ofToDataPath("captures/skin-harness/results.txt", true));
	for (const auto& l : lines_) out << l << "\n";
	out.close();
	ofExit(failures_ == 0 ? 0 : 1);
}
