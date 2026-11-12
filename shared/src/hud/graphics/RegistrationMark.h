#pragma once

#include "ofGraphics.h"
#include "glm/vec2.hpp"

namespace hud {

struct RegistrationMarkStyle {
	enum class Kind { Cross,
		Circle,
		Square,
		SplitCircle,
		TargetDot };
	Kind kind = Kind::Cross;
	ofColor color = ofColor(0);
	float alpha = 1.0f;
	float lineWidth = 1.5f;
};

// Print / alignment target centered on `center` with half-extent `radius`.
class RegistrationMark {
public:
	static void draw(const glm::vec2 & center, float radius, const RegistrationMarkStyle & style);
};

}
