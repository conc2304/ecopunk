#include "RegistrationMark.h"
#include "ofMath.h"
#include "ofPath.h"

namespace hud {

void RegistrationMark::draw(const glm::vec2 & c, float r, const RegistrationMarkStyle & s) {
	if (s.alpha <= 0.001f || r <= 0.5f) return;
	const float w = std::max(1.0f, s.lineWidth);

	ofPushStyle();
	ofSetColor(s.color, static_cast<int>(s.color.a * ofClamp(s.alpha, 0.0f, 1.0f)));
	ofSetLineWidth(w);
	ofSetCircleResolution(48);

	auto cross = [&](float arm) {
		ofFill();
		ofDrawRectangle(c.x - arm, c.y - w * 0.5f, arm * 2, w);
		ofDrawRectangle(c.x - w * 0.5f, c.y - arm, w, arm * 2);
	};
	auto ring = [&](float rad) {
		ofNoFill();
		ofDrawCircle(c.x, c.y, rad);
	};

	switch (s.kind) {
	case RegistrationMarkStyle::Kind::Cross:
		cross(r);
		break;
	case RegistrationMarkStyle::Kind::Circle:
		ring(r);
		cross(r * 1.35f);
		break;
	case RegistrationMarkStyle::Kind::Square:
		ofNoFill();
		ofDrawRectangle(c.x - r, c.y - r, r * 2, r * 2);
		cross(r * 1.35f);
		break;
	case RegistrationMarkStyle::Kind::SplitCircle:
		ring(r);
		ofFill();
		{
			// filled upper-left and lower-right quadrants
			ofPath p;
			p.setCircleResolution(48);
			p.setFillColor(ofGetStyle().color);
			p.moveTo(c.x, c.y);
			p.arc(c.x, c.y, r, r, 180, 270);
			p.close();
			p.moveTo(c.x, c.y);
			p.arc(c.x, c.y, r, r, 0, 90);
			p.close();
			p.draw();
		}
		cross(r * 1.35f);
		break;
	case RegistrationMarkStyle::Kind::TargetDot:
		ring(r);
		ofFill();
		ofDrawCircle(c.x, c.y, std::max(1.5f, r * 0.28f));
		break;
	}
	ofPopStyle();
}

}
