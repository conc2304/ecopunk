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

    const ofPixels& getPixels()  const { return player.getPixels(); }
    bool            isFrameNew() const { return player.isFrameNew(); }

private:
    ofVideoPlayer            player;
    std::vector<std::string> files;
    int                      fileIndex    = 0;
    std::vector<int>         playlist;
    int                      playlistPos  = 0;
    bool                     transitioning = false;
    void loadFile(int index);
    void buildPlaylist();
};
