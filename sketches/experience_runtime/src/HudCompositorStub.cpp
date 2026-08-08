#include "HudCompositorStub.h"

#include "ofGraphics.h"

namespace {

const char* healthName(SceneHealth health) {
	switch (health) {
		case SceneHealth::Ready:    return "Ready";
		case SceneHealth::Loading:  return "Loading";
		case SceneHealth::Degraded: return "Degraded";
		case SceneHealth::Failed:   return "Failed";
	}
	return "?";
}

} // namespace

void HudCompositorStub::draw(const HudFrameData& frame, const ofRectangle& destRect) const {
	if (!visible_) {
		return;
	}

	if (frame.sceneFrame.texture != nullptr && frame.sceneFrame.texture->isAllocated()) {
		frame.sceneFrame.texture->draw(destRect.x, destRect.y, destRect.width, destRect.height);
	}

	// Minimal debug text proving the assembled HudFrameData's fields
	// actually arrived here — not a real HUD readout.
	ofPushStyle();
	ofSetColor(255);
	std::string line1 = "scene=" + frame.scene.sceneId
		+ " health=" + healthName(frame.scene.health)
		+ " semantic=" + (frame.scene.semantic.has_value() ? "present" : "absent");
	std::string line2 = "manager.activeSceneId=" + frame.sceneManager.activeSceneId
		+ " capabilities=" + std::to_string(frame.capabilities.commands.size());
	std::string line3 = "frameNumber=" + std::to_string(frame.sceneFrame.frameNumber)
		+ " fps=" + std::to_string(static_cast<int>(frame.runtime.fps));
	ofDrawBitmapStringHighlight(line1, destRect.x, destRect.y + destRect.height + 16);
	ofDrawBitmapStringHighlight(line2, destRect.x, destRect.y + destRect.height + 34);
	ofDrawBitmapStringHighlight(line3, destRect.x, destRect.y + destRect.height + 52);
	ofPopStyle();
}
