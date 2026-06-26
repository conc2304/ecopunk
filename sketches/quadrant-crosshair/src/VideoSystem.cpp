#include "VideoSystem.h"
#include <algorithm>
#include <numeric>
#include <random>

void VideoSystem::setup(const std::string& mediaPath) {
    ofDirectory dir(mediaPath);
    dir.allowExt("mp4");
    dir.listDir();
    for (auto& f : dir.getFiles())
        files.push_back(f.getAbsolutePath());
    if (files.empty()) {
        ofLogError("VideoSystem") << "No MP4 files found in " << mediaPath;
        return;
    }
    buildPlaylist();
    loadFile(playlist[playlistPos]);
}

void VideoSystem::buildPlaylist() {
    playlist.resize(files.size());
    std::iota(playlist.begin(), playlist.end(), 0);
    auto rng = std::default_random_engine(
        (unsigned)std::chrono::steady_clock::now().time_since_epoch().count()
    );
    std::shuffle(playlist.begin(), playlist.end(), rng);
    // avoid replaying the same file at the seam between cycles
    if (files.size() > 1 && playlist[0] == fileIndex)
        std::swap(playlist[0], playlist[1]);
    playlistPos = 0;
}

void VideoSystem::loadFile(int index) {
    player.stop();
    player.close();
    player.load(files[index]);
    player.setLoopState(OF_LOOP_NONE);
    player.setVolume(0);
    player.play();
    fileIndex     = index;
    transitioning = false;
}

void VideoSystem::update() {
    player.update();
    if (!transitioning && player.getIsMovieDone())
        nextFile();
}

ofTexture& VideoSystem::getTexture() { return player.getTexture(); }
glm::vec2  VideoSystem::getVideoSize() const {
    return { (float)player.getWidth(), (float)player.getHeight() };
}

void VideoSystem::nextFile() {
    if (files.empty()) return;
    transitioning = true;
    playlistPos++;
    if (playlistPos >= (int)files.size())
        buildPlaylist();
    loadFile(playlist[playlistPos]);
}
