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

void VideoSystem::pickTargetLoops() {
    targetLoops = loopMin + (int)ofRandom((float)(loopMax - loopMin + 1));
}

void VideoSystem::loadFile(int index) {
    player.stop();
    player.close();
    player.load(files[index]);
    player.setLoopState(OF_LOOP_NONE);
    player.setVolume(0);
    player.play();
    fileIndex      = index;
    transitioning  = false;
    _fileChanged   = true;
    loopCount      = 0;
    playingForward = true;
    pickTargetLoops();

    if (!shaderReady) {
        adjustShader.load("shaders/vert.glsl", "shaders/video_adjust.glsl");
        shaderReady = true;
    }
    allocateAdjustFbo();
}

void VideoSystem::allocateAdjustFbo() {
    int w = (int)player.getWidth();
    int h = (int)player.getHeight();
    if (w <= 0 || h <= 0) return;
    if (fboAdjusted.isAllocated() &&
        (int)fboAdjusted.getWidth() == w &&
        (int)fboAdjusted.getHeight() == h) return;

    ofFbo::Settings s;
    s.width          = w;
    s.height         = h;
    s.internalformat = GL_RGB;
    s.useDepth       = false;
    fboAdjusted.allocate(s);
    fboAdjusted.begin(); ofClear(0, 0, 0, 255); fboAdjusted.end();
}

void VideoSystem::processAdjustment() {
    if (!shaderReady || !fboAdjusted.isAllocated()) return;
    fboAdjusted.begin();
    adjustShader.begin();
    adjustShader.setUniformTexture("tex",        player.getTexture(), 0);
    adjustShader.setUniform1f("saturation",      adjSaturation);
    adjustShader.setUniform1f("contrast",        adjContrast);
    adjustShader.setUniform1f("brightness",      adjBrightness);
    ofSetColor(255);
    player.getTexture().draw(0, 0, fboAdjusted.getWidth(), fboAdjusted.getHeight());
    adjustShader.end();
    fboAdjusted.end();
}

void VideoSystem::update() {
    player.update();
    if (player.isFrameNew()) {
        allocateAdjustFbo();
        processAdjustment();
    }
    if (transitioning) return;

    float pos = player.getPosition();
    bool hitEnd   = player.getIsMovieDone() || pos >= 0.998f;
    bool hitStart = !playingForward && pos <= 0.002f;

    if (pingPong) {
        if (playingForward && hitEnd) {
            // reverse
            player.setSpeed(-1.0f);
            playingForward = false;
        } else if (hitStart) {
            loopCount++;
            if (loopCount >= targetLoops) {
                loopCount = 0;
                playingForward = true;
                nextFile();
            } else {
                player.setSpeed(1.0f);
                playingForward = true;
            }
        }
    } else {
        if (hitEnd) {
            loopCount++;
            if (loopCount >= targetLoops) {
                loopCount = 0;
                nextFile();
            } else {
                player.setPosition(0.0f);
                player.play();
            }
        }
    }
}

ofTexture& VideoSystem::getTexture() {
    if (fboAdjusted.isAllocated()) return fboAdjusted.getTexture();
    return player.getTexture();
}
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
