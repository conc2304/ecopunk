#include "TemporalWarmHarness.h"

#include "ofAppRunner.h"
#include "ofFbo.h"
#include "ofGLUtils.h"
#include "ofGraphics.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#ifdef __APPLE__
#include <mach/mach.h>
#endif

namespace {

const std::string kBlobId = BlobProductionScene::kSceneId;
const std::string kTemporalId = TemporalProductionScene::kSceneId;
const std::string kBlueprintId = BlueprintProductionScene::kSceneId;
constexpr const char* kOutDir = "captures/temporal-warm";
constexpr float kSettleSeconds = 6.0f;
constexpr float kSwitchTimeoutSeconds = 30.0f;

double residentMegabytes() {
#ifdef __APPLE__
	mach_task_basic_info info;
	mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
		return static_cast<double>(info.resident_size) / (1024.0 * 1024.0);
	}
#elif defined(__linux__)
	std::ifstream status("/proc/self/status");
	std::string key;
	while (status >> key) {
		if (key == "VmRSS:") { double kb = 0; status >> kb; return kb / 1024.0; }
		status.ignore(4096, '\n');
	}
#endif
	return -1.0;
}

// Linux (Pi) only: contiguous-memory-allocator headroom the VC4 driver
// allocates GPU buffers from. -1 where unavailable.
double cmaFreeMegabytes() {
#if defined(__linux__)
	std::ifstream meminfo("/proc/meminfo");
	std::string key;
	while (meminfo >> key) {
		if (key == "CmaFree:") { double kb = 0; meminfo >> kb; return kb / 1024.0; }
		meminfo.ignore(4096, '\n');
	}
#endif
	return -1.0;
}

double msSince(std::chrono::steady_clock::time_point t0) {
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

} // namespace

bool TemporalWarmHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_TEMPORAL_WARM_HARNESS") != nullptr;
}

TemporalWarmHarness::TemporalWarmHarness(ExperienceRuntime& runtime) : runtime_(runtime) {
	std::filesystem::create_directories(ofToDataPath(kOutDir, true));
	legs_ = {
		{kBlobId, "1_blob", 0},
		{kTemporalId, "2_temporal_first", ']'},
		{kBlobId, "3_blob", '['},
		{kTemporalId, "4_temporal_second", ']'},
		{kBlueprintId, "5_blueprint", ']'},
		{kTemporalId, "6_temporal_third", '['},
		{kBlobId, "7_blob", '['},
		{kTemporalId, "8_temporal_fourth", ']'},
	};
	TemporalProductionScene& temporal = runtime_.temporalSceneForTesting();
	const auto& ambient = temporal.coreForTesting().ambientTextures();
	note("startup: Temporal ambient textures preloaded=" + std::string(ambient.isLoaded() ? "yes" : "no") +
		", count=" + ofToString(ambient.textureCount()) + ", decoded pixel bytes=" +
		ofToString(ambient.pixelBytes() / (1024.0 * 1024.0), 1) + " MB, Temporal scene set up yet=" +
		(temporal.coreForTesting().didSetup() ? "yes" : "no") + ", resident " + ofToString(residentMegabytes(), 0) + " MB");
	ambientHashAtStartup_ = ambientRenderCheck("startup");
}

std::string TemporalWarmHarness::ambientRenderCheck(const std::string& when) {
	TemporalProductionScene& temporal = runtime_.temporalSceneForTesting();
	TFAmbientTextureLayer& ambient = temporal.coreForTesting().ambientTextures();
	const glm::ivec2 size = temporal.nativeRenderSize();
	ofFbo fbo;
	fbo.allocate(size.x, size.y, GL_RGBA);
	const bool fboOk = fbo.checkStatus();
	fbo.begin();
	ofClear(128, 128, 128, 255);
	ambient.drawUnderlay();
	ambient.drawOverlay();
	fbo.end();
	ofPixels px;
	fbo.readToPixels(px);
	uint64_t hash = 1469598103934665603ULL;
	size_t changed = 0;
	const unsigned char* d = px.getData();
	const size_t channels = px.getNumChannels();
	for (size_t i = 0; i < px.size(); i++) {
		hash = (hash ^ d[i]) * 1099511628211ULL;
		if (channels >= 3 && i % channels == 0 && (d[i] != 128 || d[i + 1] != 128 || d[i + 2] != 128)) changed++;
	}
	const size_t pixels = px.getWidth() * px.getHeight();
	const std::string hex = ofToHex(hash);
	note("[ambient " + when + "] residency=" + (ambient.residency() == TFAmbientTextureLayer::Residency::Bounded ? "bounded" : "legacy") +
		" hash=" + hex + " changedPixels=" + ofToString(changed) + "/" + ofToString(pixels) +
		" tintTextures=" + ofToString(ambient.residentTintTextures()) + " maskTextures=" + ofToString(ambient.residentMaskTextures()) +
		" cpuMiB=" + ofToString(ambient.pixelBytes() / (1024.0 * 1024.0), 1) +
		" gpuEstMiB=" + ofToString(ambient.gpuBytesEstimate() / (1024.0 * 1024.0), 1));
	check("[ambient " + when + "] FBO complete", fboOk);
	check("[ambient " + when + "] every ambient pair has a resident tint texture",
		ambient.textureCount() > 0 && ambient.residentTintTextures() == ambient.textureCount(),
		ofToString(ambient.residentTintTextures()) + "/" + ofToString(ambient.textureCount()));
	check("[ambient " + when + "] ambient layers visibly drawn (not black/missing)", changed > pixels / 2,
		ofToString(changed) + "/" + ofToString(pixels) + " pixels changed");
	return hex;
}

void TemporalWarmHarness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	note(std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " — " + detail));
}

void TemporalWarmHarness::note(const std::string& line) {
	ofLogNotice("TemporalWarmHarness") << line;
	lines_.push_back(line);
}

bool TemporalWarmHarness::idleOn(const std::string& sceneId) const {
	const HudFrameData& f = runtime_.currentHudFrameData();
	return f.sceneManager.transitionPhase == SceneTransitionPhase::Idle && f.sceneManager.activeSceneId == sceneId
		&& f.scene.sceneId == sceneId;
}

void TemporalWarmHarness::onLegLive() {
	const Leg& leg = legs_[leg_];
	if (leg.key != 0) {
		const float switchSeconds = clock_ - switchStart_;
		note("[" + leg.label + "] switch request -> first live frame " + ofToString(switchSeconds * 1000.0f, 0) +
			" ms, worst frame " + ofToString(maxFrame_ * 1000.0f, 1) + " ms");
		if (leg.sceneId == kTemporalId) {
			check("[" + leg.label + "] no multi-second freeze entering Temporal (worst frame < 500 ms)", maxFrame_ < 0.5f,
				ofToString(maxFrame_ * 1000.0f, 1) + " ms");
		}
	}
	if (leg.sceneId == kTemporalId) {
		const HudFrameData& f = runtime_.currentHudFrameData();
		auto& temporal = runtime_.temporalSceneForTesting();
		const auto& adapter = temporal.videoAdapterForTesting();
		check("[" + leg.label + "] effects snapshot present (TFEffectPicker producer)", f.effects.has_value());
		check("[" + leg.label + "] Temporal decoder on the canonical media",
			f.video && f.video->mediaId && adapter.loadedMediaId() == *f.video->mediaId);
		check("[" + leg.label + "] history refills from a fresh start on activation",
			temporal.coreForTesting().historyFrameCount() < 30, ofToString(temporal.coreForTesting().historyFrameCount()) + " frames");
	}
}

void TemporalWarmHarness::onLegSettled() {
	const Leg& leg = legs_[leg_];
	const double mb = residentMegabytes();
	residentMb_.push_back(mb);
	TemporalProductionScene& temporal = runtime_.temporalSceneForTesting();
	const TFAmbientTextureLayer& ambient = temporal.coreForTesting().ambientTextures();
	note("[" + leg.label + "] settled: resident " + ofToString(mb, 0) + " MB, CmaFree " + ofToString(cmaFreeMegabytes(), 0) +
		" MB, tintTextures=" + ofToString(ambient.residentTintTextures()) + " maskTextures=" + ofToString(ambient.residentMaskTextures()) +
		" playheadTextures=" + ofToString(temporal.videoAdapterForTesting().buffer().getAllocatedPlayheadTextureCount()) +
		"/" + ofToString(temporal.videoAdapterForTesting().buffer().getNumPlayheads()));
	if (leg.sceneId == kTemporalId) {
		const HudFrameData& f = runtime_.currentHudFrameData();
		check("[" + leg.label + "] activation-relative time (fresh)",
			f.scene.semantic && f.scene.semantic->timing.activeSeconds < kSettleSeconds + 2.0f,
			f.scene.semantic ? ofToString(f.scene.semantic->timing.activeSeconds, 1) + " s" : std::string("no semantic"));
	}
}

void TemporalWarmHarness::step(float dt) {
	if (done_) return;
	runtime_.draw();
	clock_ += dt;
	maxFrame_ = std::max(maxFrame_, dt);

	const Leg& leg = legs_[leg_];
	if (waiting_) {
		if (idleOn(leg.sceneId)) {
			waiting_ = false;
			legSeconds_ = 0.0f;
			onLegLive();
		} else if (clock_ - switchStart_ > kSwitchTimeoutSeconds) {
			check("[" + leg.label + "] switch completed", false, "timed out");
			finish();
		}
		return;
	}

	legSeconds_ += dt;
	if (legSeconds_ >= kSettleSeconds) {
		onLegSettled();
		if (leg_ + 1 >= legs_.size()) {
			finish();
			return;
		}
		leg_++;
		waiting_ = true;
		maxFrame_ = 0.0f;
		switchStart_ = clock_;
		runtime_.keyPressed(legs_[leg_].key); // real InputRouter -> RuntimeCommand::Next/PreviousScene
	}
}

void TemporalWarmHarness::finish() {
	// Legs 2/4/6 are Temporal, 1/3 Blob: memory must not climb across repeats.
	if (residentMb_.size() >= 6) {
		const double growth = residentMb_[5] - residentMb_[3];
		check("no resident-memory growth across repeated Temporal activations (leg 4 -> leg 6 < 64 MB)", growth < 64.0,
			ofToString(growth, 1) + " MB");
	}
	{
		// Same ambient output after repeated Blob/Temporal/Blueprint switching
		// (no stale/evicted/use-after-free texture), then prove a mask that
		// is not eagerly resident can still be loaded on request.
		const std::string endHash = ambientRenderCheck("after switching");
		check("ambient output unchanged across the run", endHash == ambientHashAtStartup_, ambientHashAtStartup_ + " vs " + endHash);
		TFAmbientTextureLayer& ambient = runtime_.temporalSceneForTesting().coreForTesting().ambientTextures();
		// On-demand mask residency, 3 x (load -> use -> release) on the same
		// mask. glFinish() is test-only, so latencies include the GPU work.
		// Errors queued before this block belong to earlier code: drained and
		// reported separately rather than charged to the mask path.
		GLenum stale = glGetError();
		note("[mask cycles] GL error flag on entry (from earlier code): 0x" + ofToHex(static_cast<int>(stale)));
		while (stale != GL_NO_ERROR) stale = glGetError();
		const bool bounded = ambient.residency() == TFAmbientTextureLayer::Residency::Bounded;
		const size_t baseMasks = ambient.residentMaskTextures();
		for (int cycle = 1; cycle <= 3; cycle++) {
			const std::string c = "[mask cycle " + ofToString(cycle) + "] ";
			const double rssBefore = residentMegabytes();
			const size_t before = ambient.residentMaskTextures();
			auto t0 = std::chrono::steady_clock::now();
			const bool loaded = ambient.ensureMaskLoaded(0);
			glFinish();
			const double loadMs = msSince(t0);
			const double rssLoaded = residentMegabytes();
			const GLenum errLoad = glGetError();
			const ofTexture& mask = ambient.textureAt(0).mask.getTexture();
			const bool texAfterLoad = mask.isAllocated();
			const std::string dims = ofToString(ambient.textureAt(0).mask.getWidth()) + "x" + ofToString(ambient.textureAt(0).mask.getHeight());
			const size_t loadedCount = ambient.residentMaskTextures();
			// Usable: draw the mask into a small FBO and read it back.
			ofFbo probeFbo;
			probeFbo.allocate(64, 64, GL_RGBA);
			probeFbo.begin();
			ofClear(0, 0, 0, 255);
			ofSetColor(255);
			if (mask.isAllocated()) mask.draw(0, 0, 64, 64);
			probeFbo.end();
			ofPixels px;
			probeFbo.readToPixels(px);
			size_t lit = 0;
			for (size_t i = 0; i + 2 < px.size(); i += px.getNumChannels()) lit += (px[i] | px[i + 1] | px[i + 2]) ? 1 : 0;
			const GLenum errUse = glGetError();
			t0 = std::chrono::steady_clock::now();
			const bool released = ambient.releaseMask(0);
			glFinish();
			const double releaseMs = msSince(t0);
			const double rssReleased = residentMegabytes();
			const GLenum errRelease = glGetError();
			const size_t after = ambient.residentMaskTextures();
			note(c + "masks " + ofToString(before) + " -> " + ofToString(loadedCount) + " -> " + ofToString(after) +
				", load(decode+upload+finish) " + ofToString(loadMs, 2) + " ms, release " + ofToString(releaseMs, 3) + " ms, tex " + dims +
				", drawn lit pixels " + ofToString(lit) + "/4096, RSS " + ofToString(rssBefore, 1) + " -> " + ofToString(rssLoaded, 1) + " -> " +
				ofToString(rssReleased, 1) + " MB, CmaFree " + ofToString(cmaFreeMegabytes(), 0) + " MB");
			check(c + "mask loads on demand", loaded && texAfterLoad && loadedCount == (bounded ? 1 : baseMasks),
				ofToString(loadedCount) + " resident after load");
			check(c + "loaded mask is drawable (non-black readback)", lit > 0, ofToString(lit) + "/4096");
			check(c + "release returns resident masks to baseline", released && after == (bounded ? 0 : baseMasks - 1),
				ofToString(after) + " resident");
			check(c + "no GL error in load/use/release", errLoad == GL_NO_ERROR && errUse == GL_NO_ERROR && errRelease == GL_NO_ERROR,
				"load 0x" + ofToHex(static_cast<int>(errLoad)) + " use 0x" + ofToHex(static_cast<int>(errUse)) + " release 0x" + ofToHex(static_cast<int>(errRelease)));
			if (!bounded) ambient.ensureMaskLoaded(0); // legacy keeps every mask resident: restore it
		}
		const GLenum err = glGetError();
		check("no GL error at end of run", err == GL_NO_ERROR, "0x" + ofToHex(static_cast<int>(err)));
	}
	if (residentMb_.size() >= 8) {
		// Temporal legs 2/4/6/8: classify the settled-RSS trend.
		const double t2 = residentMb_[1], t4 = residentMb_[3], t6 = residentMb_[5], t8 = residentMb_[7];
		const bool monotonic = t4 > t2 + 1.0 && t6 > t4 + 1.0 && t8 > t6 + 1.0;
		const double spread = std::max({t4, t6, t8}) - std::min({t4, t6, t8});
		note("memory trend over Temporal legs 2/4/6/8: " + ofToString(t2, 0) + " / " + ofToString(t4, 0) + " / " + ofToString(t6, 0) + " / " +
			ofToString(t8, 0) + " MB -> " + (monotonic ? "GROWS MONOTONICALLY" : (spread < 16.0 ? "stable" : "oscillates")) +
			" (legs 4-8 spread " + ofToString(spread, 1) + " MB)");
		check("no resident-memory growth across repeated Temporal activations (leg 4 -> leg 8 < 64 MB)", t8 - t4 < 64.0,
			ofToString(t8 - t4, 1) + " MB");
	}
	note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
	std::ofstream out(ofToDataPath(std::string(kOutDir) + "/results.txt", true));
	for (const auto& l : lines_) out << l << "\n";
	done_ = true;
	ofExit(failures_ == 0 ? 0 : 1);
}
