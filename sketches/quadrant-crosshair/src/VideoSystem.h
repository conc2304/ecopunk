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
    void allocateAdjustFbo();
    void processAdjustment();
    void loadFile(int index);
    void buildPlaylist();
};
