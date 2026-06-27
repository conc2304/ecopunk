#pragma once
#include "ofMain.h"
#include <vector>
#include <string>

class VideoSystem {
public:
    void setup(const std::string& mediaPath);
    void update();
    ofTexture&  getTexture();
    glm::vec2   getVideoSize() const;
    void nextFile();

    const ofPixels& getPixels()    const { return player.getPixels(); }
    bool            isFrameNew()   const { return player.isFrameNew(); }
    bool            fileChanged()        { bool v = _fileChanged; _fileChanged = false; return v; }

    // ── In-code dials ──────────────────────────────────────────────────────
    float adjSaturation =  1.15f;   // +15% saturation
    float adjContrast   =  1.10f;   // +10% contrast
    float adjBrightness = -0.10f;   // -10% brightness

    int   loopMin  = 3;     // min play-throughs before advancing to next file
    int   loopMax  = 5;     // max play-throughs
    bool  pingPong = false; // palindrome playback (note: H.264 reverse costs more CPU)

private:
    ofVideoPlayer            player;
    std::vector<std::string> files;
    int                      fileIndex    = 0;
    std::vector<int>         playlist;
    int                      playlistPos  = 0;
    bool                     transitioning  = false;
    bool                     _fileChanged   = false;
    ofFbo                    fboAdjusted;
    ofShader                 adjustShader;
    bool                     shaderReady    = false;
    int                      loopCount      = 0;
    int                      targetLoops    = 3;
    bool                     playingForward = true;
    void allocateAdjustFbo();
    void processAdjustment();
    void loadFile(int index);
    void buildPlaylist();
    void pickTargetLoops();
};
