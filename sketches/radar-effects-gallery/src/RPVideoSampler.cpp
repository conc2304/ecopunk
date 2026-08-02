#include "RPVideoSampler.h"
#include <algorithm>
#include <numeric>
#include <random>
#include <chrono>

void RPVideoSampler::setup(const std::string & mediaPath) {
	ofDirectory dir(mediaPath);
	dir.allowExt("mp4");
	dir.listDir();
	for (auto & f : dir.getFiles()) files.push_back(f.getAbsolutePath());

	if (files.empty()) {
		ofLogError("RPVideoSampler") << "No MP4 files found in " << mediaPath;
		return;
	}

	buildPlaylist();
	loadFile(playlist[playlistPos]);
	ready = true;
}

void RPVideoSampler::buildPlaylist() {
	playlist.resize(files.size());
	std::iota(playlist.begin(), playlist.end(), 0);
	auto rng = std::default_random_engine(
		static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::shuffle(playlist.begin(), playlist.end(), rng);
	// Avoid replaying the same file at the seam between shuffles.
	if (files.size() > 1 && playlist[0] == fileIndex) {
		std::swap(playlist[0], playlist[1]);
	}
	playlistPos = 0;
}

void RPVideoSampler::loadFile(int index) {
	player.stop();
	player.close();
	player.load(files[index]);
	player.setLoopState(OF_LOOP_NONE);
	player.setVolume(0);
	player.play();
	fileIndex = index;
	elapsedOnCurrent = 0.0f;

	size_t slash = files[index].rfind('/');
	std::string base = (slash == std::string::npos) ? files[index] : files[index].substr(slash + 1);
	size_t dot = base.rfind('.');
	currentFilename = (dot != std::string::npos) ? base.substr(0, dot) : base;
}

void RPVideoSampler::nextFile() {
	playlistPos++;
	if (playlistPos >= static_cast<int>(playlist.size())) {
		buildPlaylist();
	}
	loadFile(playlist[playlistPos]);
}

void RPVideoSampler::update() {
	if (!ready) return;

	float dt = ofGetLastFrameTime();
	player.update();
	elapsedOnCurrent += dt;

	bool hitLoopEnd = player.getIsMovieDone() || player.getPosition() >= 0.998f;
	if (hitLoopEnd) {
		if (elapsedOnCurrent >= minPlaySeconds) {
			nextFile();
		} else {
			// Not done playing its minimum duration yet — loop the same
			// clip again rather than advancing.
			player.setPosition(0.0f);
			player.play();
		}
	}
}
