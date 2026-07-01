#pragma once

#include "ofShader.h"
#include <map>
#include <string>

// Named registry of fragment-shader effects, all sharing one passthrough
// vertex shader. Promoted from quadrant-crosshair/src/ShaderLibrary.h
// unchanged — it was already sketch-agnostic (a name -> path map loader).
class ShaderLibrary {
	public:
		void setup();
		ofShader & get(const std::string & name);
		bool has(const std::string & name) const;

	private:
		std::map<std::string, ofShader> shaders;
		void load(const std::string & name, const std::string & fragPath);
};
