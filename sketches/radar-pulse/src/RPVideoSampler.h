#pragma once
#include "ofMain.h"
#include <string>
#include <vector>

// System F — Live Video Sampler. Continuous/live playback (drives the video
// forward every frame and exposes the current texture), distinct from
// shared/src/VideoSampler.h's seek-and-freeze single-still-capture pattern
// used elsewhere in this repo.
//
// Playlist/rotation strategy is modeled on
// sketches/quadrant-crosshair/src/VideoSystem.cpp: load every .mp4 in a
// shared media folder, shuffle a playlist (avoiding an immediate repeat at
// the reshuffle seam), and advance through it — same shared clip collection
// as the other sketches (this sketch's bin/data/media is a symlink to
// quadrant-crosshair's, not a third physical copy of ~426MB of video).
//
// The one deliberate difference from VideoSystem: that class advances after
// a random loopMin..loopMax *playthrough count*; this one advances after a
// minimum *wall-clock duration* (minPlaySeconds) has elapsed on the current
// clip, checked at the next natural loop boundary — so a clip always plays
// at least minPlaySeconds regardless of its own length, then moves to a
// random next clip rather than looping again.
class RPVideoSampler {
public:
	void setup(const std::string & mediaPath);
	void update();

	ofTexture & getTexture() { return player.getTexture(); }
	bool isReady() const { return ready; }
	bool isFrameNew() const { return player.isFrameNew(); }
	glm::vec2 getVideoSize() const { return { static_cast<float>(player.getWidth()), static_cast<float>(player.getHeight()) }; }
	const std::string & getCurrentFilename() const { return currentFilename; }

	float minPlaySeconds = 30.0f;

private:
	ofVideoPlayer player;
	std::vector<std::string> files;
	std::vector<int> playlist;
	int playlistPos = 0;
	int fileIndex = -1;
	std::string currentFilename;
	float elapsedOnCurrent = 0.0f;
	bool ready = false;

	void buildPlaylist();
	void loadFile(int index);
	void nextFile();
};
