#pragma once
#include "ofMain.h"
#include <map>
#include <string>

class ShaderLibrary {
public:
    void      setup();
    ofShader& get(const std::string& name);
    bool      has(const std::string& name) const;

private:
    std::map<std::string, ofShader> shaders;
    void load(const std::string& name, const std::string& fragPath);
};
