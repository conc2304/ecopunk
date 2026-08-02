#pragma once

#include "ofShader.h"
#include <map>
#include <string>

// Named registry of fragment-shader effects, all sharing one passthrough
// vertex shader. Promoted from quadrant-crosshair/src/ShaderLibrary.h
// unchanged — it was already sketch-agnostic (a name -> path map loader).
//
// This is the SHADER-LOADING layer only. If you're adding a new effect or
// need to bind its uniforms, do not hand-write another per-effect
// dispatch cascade here or in a caller — see shared/src/video-effects/README.md
// and shared/src/video-effects/catalog/DefaultVideoEffectCatalog.cpp, the
// canonical registry every sketch's fixed-default uniform binding now
// sources from instead of duplicating literals.
class ShaderLibrary {
	public:
		void setup();
		ofShader & get(const std::string & name);
		bool has(const std::string & name) const;

	private:
		std::map<std::string, ofShader> shaders;
		void load(const std::string & name, const std::string & fragPath);
};
