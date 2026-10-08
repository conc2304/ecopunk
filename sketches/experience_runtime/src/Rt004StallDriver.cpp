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
		runtime_.keyPressed(a.key);
	}
}
