#include "TemporalWarmHarness.h"

#include "ofAppRunner.h"
#include "ofFbo.h"
#include "ofGLUtils.h"
#include "ofGraphics.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <algorithm>
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
#endif
	return -1.0;
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
	note("[" + leg.label + "] settled: resident " + ofToString(mb, 0) + " MB");
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
		const size_t masksBefore = ambient.residentMaskTextures();
		const bool maskOk = ambient.ensureMaskLoaded(0);
		check("mask reachable on demand (ensureMaskLoaded)", maskOk && ambient.residentMaskTextures() >= std::max<size_t>(masksBefore, 1),
			"resident masks " + ofToString(masksBefore) + " -> " + ofToString(ambient.residentMaskTextures()));
		const GLenum err = glGetError();
		check("no GL error at end of run", err == GL_NO_ERROR, "0x" + ofToHex(static_cast<int>(err)));
	}
	note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
	std::ofstream out(ofToDataPath(std::string(kOutDir) + "/results.txt", true));
	for (const auto& l : lines_) out << l << "\n";
	done_ = true;
	ofExit(failures_ == 0 ? 0 : 1);
}
