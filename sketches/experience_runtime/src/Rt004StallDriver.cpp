#include "Rt004StallDriver.h"

#include "ExperienceRuntime.h"
#include "Rt004StallProbe.h"
#include "ofAppRunner.h"
#include "ofFileUtils.h"
#include "ofGLUtils.h"
#include "ofImage.h"
#include "ofTexture.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>

bool Rt004StallDriver::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_STALL_DRIVER") != nullptr;
}

Rt004StallDriver::Rt004StallDriver(ExperienceRuntime & runtime)
	: runtime_(runtime) {
	const char * path = std::getenv("EXPERIENCE_RUNTIME_STALL_DRIVER_LOG");
	logPath_ = path ? path : "rt004-stall-driver.log";

	// Matrix (§Phase 3): startup in Blob; first Temporal activation; media
	// change while Temporal is active; media change during Blob dwell then
	// Temporal reactivation; reactivation with media not manually changed;
	// repeated so each event type occurs >= 3 times. The canonical
	// service's own 30 s automatic advance also fires and is annotated by
	// the probe ("canonical.load" notes), not suppressed.
	const char * scriptName = std::getenv("EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT");
	if (scriptName != nullptr && std::string(scriptName) == "ambient-split") {
		// Diagnostic replica of TFAmbientTextureLayer::setup()'s per-file
		// work, split into file read+PNG decode (ofLoadImage into ofPixels)
		// and GL texture allocate+upload (ofTexture::loadData). Production
		// code is not called or changed.
		ofDirectory dir;
		dir.allowExt("png");
		dir.listDir("backgrounds");
		double decodeTotal = 0.0, uploadTotal = 0.0, decodeMax = 0.0, uploadMax = 0.0;
		for (size_t i = 0; i < dir.size(); i++) {
			ofPixels px;
			int64_t t0 = rt004::nowNs();
			ofLoadImage(px, dir.getPath(i));
			int64_t t1 = rt004::nowNs();
			ofTexture tex;
			tex.allocate(px);
			tex.loadData(px);
			glFinish(); // diagnostic only: charge the upload to this step
			int64_t t2 = rt004::nowNs();
			double d = (t1 - t0) / 1e6, u = (t2 - t1) / 1e6;
			decodeTotal += d; uploadTotal += u;
			decodeMax = std::max(decodeMax, d); uploadMax = std::max(uploadMax, u);
			std::ostringstream os;
			os << "IMAGE " << dir.getName(i) << " " << px.getWidth() << "x" << px.getHeight() << "x" << px.getNumChannels()
			   << " decodeMs=" << d << " uploadMs=" << u;
			log(os.str());
		}
		std::ostringstream os;
		os << "AMBIENT-SPLIT files=" << dir.size() << " decodeTotalMs=" << decodeTotal << " uploadTotalMs=" << uploadTotal
		   << " decodeMaxMs=" << decodeMax << " uploadMaxMs=" << uploadMax;
		log(os.str());
		script_.push_back({ 1.0f, 0, "exit" });
		return;
	}
	if (scriptName != nullptr && std::string(scriptName) == "first-activation") {
		// One cold first Temporal activation per process, then exit.
		script_.push_back({ 8.0f, ']', "Blob->Temporal (first activation)" });
		script_.push_back({ 16.0f, ']', "Temporal->Blob" });
		script_.push_back({ 22.0f, 0, "exit" });
		log("=== RT-004 stall driver: script first-activation ===");
		return;
	}
	if (scriptName != nullptr && std::string(scriptName) == "temporal-steady") {
		// Priority B: sustained Temporal frame time, kept apart from the
		// first-activation and media-change frames by the probe's "window"
		// context (frame intervals are reported per owner/window). Run with
		// EXPERIENCE_RUNTIME_VIDEO_DWELL set above the run length so the
		// canonical automatic advance cannot land inside the steady window.
		float steadySeconds = 120.0f;
		if (const char * s = std::getenv("EXPERIENCE_RUNTIME_STALL_DRIVER_STEADY_SECONDS")) steadySeconds = std::max(10.0f, static_cast<float>(std::atof(s)));
		script_.push_back({ 5.0f, ']', "Blob->Temporal (first activation)" });
		script_.push_back({ 5.0f, kMarkWindow, "warmup" });
		script_.push_back({ 20.0f, kMarkWindow, "steady" });
		script_.push_back({ 20.0f + steadySeconds, kMarkWindow, "media-change" });
		script_.push_back({ 20.0f + steadySeconds, 'n', "next media while Temporal active" });
		script_.push_back({ 35.0f + steadySeconds, 0, "exit" });
		log("=== RT-004 stall driver: script temporal-steady, steady window " + std::to_string(steadySeconds) + " s ===");
		return;
	}
	if (scriptName != nullptr && std::string(scriptName) == "upload-split") {
		// Measurement only (no production code): cost of one playhead-sized
		// 640x360 upload as RGB (what Temporal uses, GL_RGB8) vs RGBA, to tell
		// whether the driver pays for an RGB->RGBA conversion. glFinish()
		// charges the GPU/driver work to each upload.
		const int w = 640, h = 360, iterations = 300;
		for (int channels : { 3, 4 }) {
			ofPixels px;
			px.allocate(w, h, channels == 3 ? OF_PIXELS_RGB : OF_PIXELS_RGBA);
			for (size_t i = 0; i < px.size(); i++) px[i] = static_cast<unsigned char>((i * 2654435761u) >> 24);
			ofTexture tex;
			tex.allocate(px);
			glFinish();
			std::vector<double> ms;
			for (int i = 0; i < iterations; i++) {
				px[i % px.size()] ^= 0xFF; // different data each time
				int64_t t0 = rt004::nowNs();
				tex.loadData(px);
				glFinish();
				ms.push_back((rt004::nowNs() - t0) / 1e6);
			}
			std::sort(ms.begin(), ms.end());
			double sum = 0.0;
			for (double v : ms) sum += v;
			std::ostringstream os;
			os << "UPLOAD-SPLIT " << w << "x" << h << (channels == 3 ? " RGB" : " RGBA") << " internalFormat=0x" << std::hex
			   << tex.getTextureData().glInternalFormat << std::dec << " bytes=" << px.size() << " n=" << iterations
			   << " meanMs=" << sum / iterations << " p50Ms=" << ms[iterations / 2] << " p95Ms=" << ms[iterations * 95 / 100]
			   << " p99Ms=" << ms[iterations * 99 / 100] << " glError=0x" << std::hex << glGetError();
			log(os.str());
		}
		script_.push_back({ 1.0f, 0, "exit" });
		return;
	}
	if (scriptName != nullptr && std::string(scriptName) == "playhead-ab") {
		// Playhead upload A/B in ONE process (same media, B fast downscale and
		// C bounded residency held at their defaults): alternates legacy
		// (always upload) and skip-redundant windows, interleaved.
		float windowSeconds = 30.0f;
		if (const char * s = std::getenv("EXPERIENCE_RUNTIME_STALL_DRIVER_STEADY_SECONDS")) windowSeconds = std::max(10.0f, static_cast<float>(std::atof(s)));
		int pairs = 4;
		if (const char * s = std::getenv("EXPERIENCE_RUNTIME_STALL_DRIVER_AB_PAIRS")) pairs = std::max(1, std::atoi(s));
		script_.push_back({ 5.0f, ']', "Blob->Temporal (first activation)" });
		script_.push_back({ 5.0f, kMarkWindow, "warmup" });
		float t = 20.0f;
		for (int i = 0; i < pairs; i++) {
			script_.push_back({ t, kSetEnv, "EXPERIENCE_RUNTIME_TEMPORAL_PLAYHEAD_UPLOAD=legacy" });
			script_.push_back({ t, kMarkWindow, "settle" });
			script_.push_back({ t + 2.0f, kMarkWindow, "steady-upload-legacy" });
			t += 2.0f + windowSeconds;
			script_.push_back({ t, kSetEnv, "EXPERIENCE_RUNTIME_TEMPORAL_PLAYHEAD_UPLOAD=" });
			script_.push_back({ t, kMarkWindow, "settle" });
			script_.push_back({ t + 2.0f, kMarkWindow, "steady-upload-skip" });
			t += 2.0f + windowSeconds;
		}
		script_.push_back({ t, kMarkWindow, "media-change" });
		script_.push_back({ t, 'n', "next media while Temporal active" });
		script_.push_back({ t + 15.0f, 0, "exit" });
		log("=== RT-004 stall driver: script playhead-ab, " + std::to_string(pairs) + " pairs x " + std::to_string(windowSeconds) + " s ===");
		return;
	}
	if (scriptName != nullptr && std::string(scriptName) == "temporal-ab") {
		// Priority B A/B in ONE process (same media, decoder state, thermal
		// conditions): alternates the legacy history downscale and the
		// cached one in equal windows, interleaved so drift cancels. Frame
		// intervals are reported per window (steady-legacy / steady-fast).
		float windowSeconds = 30.0f;
		if (const char * s = std::getenv("EXPERIENCE_RUNTIME_STALL_DRIVER_STEADY_SECONDS")) windowSeconds = std::max(10.0f, static_cast<float>(std::atof(s)));
		int pairs = 4;
		if (const char * s = std::getenv("EXPERIENCE_RUNTIME_STALL_DRIVER_AB_PAIRS")) pairs = std::max(1, std::atoi(s));
		script_.push_back({ 5.0f, ']', "Blob->Temporal (first activation)" });
		script_.push_back({ 5.0f, kMarkWindow, "warmup" });
		float t = 20.0f;
		for (int i = 0; i < pairs; i++) {
			// Each mode gets a 2 s settle sub-window after the switch.
			script_.push_back({ t, kSetLegacyDownscale, "1" });
			script_.push_back({ t, kMarkWindow, "settle" });
			script_.push_back({ t + 2.0f, kMarkWindow, "steady-legacy" });
			t += 2.0f + windowSeconds;
			script_.push_back({ t, kSetLegacyDownscale, "0" });
			script_.push_back({ t, kMarkWindow, "settle" });
			script_.push_back({ t + 2.0f, kMarkWindow, "steady-fast" });
			t += 2.0f + windowSeconds;
		}
		script_.push_back({ t, kMarkWindow, "media-change" });
		script_.push_back({ t, 'n', "next media while Temporal active" });
		script_.push_back({ t + 15.0f, 0, "exit" });
		log("=== RT-004 stall driver: script temporal-ab, " + std::to_string(pairs) + " pairs x " + std::to_string(windowSeconds) + " s ===");
		return;
	}
	float t = 15.0f;
	for (int cycle = 0; cycle < 3; cycle++) {
		std::string c = "cycle" + std::to_string(cycle + 1) + ":";
		script_.push_back({ t, ']', c + "Blob->Temporal (activation with media as currently selected)" });
		t += 15.0f;
		script_.push_back({ t, 'n', c + "next media while Temporal active" });
		t += 15.0f;
		script_.push_back({ t, ']', c + "Temporal->Blob" });
		t += 10.0f;
		script_.push_back({ t, 'n', c + "next media during Blob dwell" });
		t += 10.0f;
		script_.push_back({ t, ']', c + "Blob->Temporal (reactivation after Blob-dwell media change)" });
		t += 15.0f;
		script_.push_back({ t, ']', c + "Temporal->Blob" });
		t += 10.0f;
		script_.push_back({ t, ']', c + "Blob->Temporal (reactivation, no manual media change)" });
		t += 15.0f;
		script_.push_back({ t, ']', c + "Temporal->Blob" });
		t += 10.0f;
	}
	script_.push_back({ t + 5.0f, 0, "exit" });

	log("=== RT-004 stall driver: " + std::to_string(script_.size()) + " actions, probe "
		+ (rt004::Probe::get().enabled() ? "ON" : "OFF") + " ===");
}

void Rt004StallDriver::log(const std::string & line) {
	if (FILE * f = std::fopen(logPath_.c_str(), "a")) {
		std::fprintf(f, "%s\n", line.c_str());
		std::fclose(f);
	}
}

void Rt004StallDriver::step(float dt) {
	elapsed_ += dt;

	double frameMs = ofGetLastFrameTime() * 1000.0;
	windowFrames_++;
	windowSumMs_ += frameMs;
	windowMaxMs_ = std::max(windowMaxMs_, frameMs);
	windowElapsed_ += dt;
	if (windowElapsed_ >= 5.0f) {
		std::ostringstream os;
		os << "WINDOW t=" << elapsed_ << "s frames=" << windowFrames_ << " meanFrameMs=" << windowSumMs_ / windowFrames_
		   << " maxFrameMs=" << windowMaxMs_;
		log(os.str());
		windowElapsed_ = 0.0f;
		windowFrames_ = 0;
		windowSumMs_ = 0.0;
		windowMaxMs_ = 0.0;
	}

	while (next_ < script_.size() && elapsed_ >= script_[next_].atSeconds) {
		const Action & a = script_[next_++];
		std::ostringstream os;
		os << "ACTION t=" << elapsed_ << "s probeFrame=" << rt004::Probe::get().frameIndex() << " " << a.label;
		log(os.str());
		rt004::Probe::get().note("driver.action", a.label);
		if (a.key == 0) {
			ofExit();
			return;
		}
		if (a.key == kMarkWindow) {
			rt004::Probe::get().setContext("window", a.label);
			continue;
		}
		if (a.key == kSetEnv) {
			// label "NAME=VALUE" sets, "NAME=" unsets
			const size_t eq = a.label.find('=');
			const std::string name = a.label.substr(0, eq);
			const std::string value = eq == std::string::npos ? "" : a.label.substr(eq + 1);
			if (value.empty()) unsetenv(name.c_str());
			else setenv(name.c_str(), value.c_str(), 1);
			continue;
		}
		if (a.key == kSetLegacyDownscale) {
			if (a.label == "1") setenv("EXPERIENCE_RUNTIME_TEMPORAL_LEGACY_DOWNSCALE", "1", 1);
			else unsetenv("EXPERIENCE_RUNTIME_TEMPORAL_LEGACY_DOWNSCALE");
			continue;
		}
		runtime_.keyPressed(a.key);
	}
}
