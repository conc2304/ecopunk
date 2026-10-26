#include "HudCompositorBridge.h"
#include "HudReactiveControlArt.h"

#include "ofLog.h"

void HudCompositorBridge::setup() {
	// The one and only canonical canvas size — see this class's header
	// comment for why no destRect/scale step is needed downstream.
	renderer_.setup(1280.0f, 720.0f);

	// Production behavior, not Validation Studio behavior: an invalid
	// binding is skipped + logged once, never rendered as a visible
	// BindingPlaceholder (HudWireframeRenderer::setStudioMode()'s own
	// doc comment) — studioMode_ defaults to false already, this call is
	// here only to make the choice explicit rather than implicit.
	renderer_.setStudioMode(false);
}

void HudCompositorBridge::update(float dt, const HudFrameData& frame) {
	// Scene epoch, frame freshness, and generation are all derived inside
	// this call from frame.sceneManager.activeSceneId /
	// frame.sceneFrame.frameNumber / frame.scene.semantic->timing.generation
	// respectively — see HudWireframeRenderer::update(float, const
	// HudFrameData&)'s own comment. This bridge adds no logic of its own.
	renderer_.update(dt, frame);
	refreshFocus(); // availability follows this frame's resolved controls
}

void HudCompositorBridge::refreshFocus() {
	focus_.refresh([this](const std::string& regionId) {
		const std::optional<bool> enabled = renderer_.controlEnabled(regionId);
		if (!enabled) return HudControlAvailability::Hidden;
		return *enabled ? HudControlAvailability::Enabled : HudControlAvailability::Disabled;
	});
	renderer_.setFocusedControl(focus_.focused());
}

bool HudCompositorBridge::stepFocus(int delta) {
	const bool moved = focus_.step(delta);
	renderer_.setFocusedControl(focus_.focused());
	return moved;
}

void HudCompositorBridge::draw() const {
	drawCallCount_++;
	if (!visible_) return;
	renderer_.draw();
}

void HudCompositorBridge::configureSkins(const HudAssetPaths& paths,
	const hudskin::SkinManifestValidationPolicy& expectedBlueprint) {
	hudskin::SkinLoaderConfig config;
	config.skinsRoot = paths.skinsRoot;
	config.expectedBlueprint = expectedBlueprint;
	const ofRectangle b = renderer_.pixelBoundsFor(hudpresent::reactive::kSceneNextRegionId);
	const auto sprite = hudpresent::reactive::pressedSpriteRect(b.x, b.y, b.width, b.height);
	config.pressedSpriteWidth = static_cast<uint32_t>(sprite.width);
	config.pressedSpriteHeight = static_cast<uint32_t>(sprite.height);
	// Shared button faces: control region + 4 px bleed per side. Every
	// navigation control is the same square, so one size serves all six.
	const ofRectangle ctl = renderer_.pixelBoundsFor("controls.scene_previous");
	const auto face = hudpresent::reactive::buttonFaceRect(ctl.x, ctl.y, ctl.width, ctl.height);
	config.buttonFaceWidth = static_cast<uint32_t>(face.width);
	config.buttonFaceHeight = static_cast<uint32_t>(face.height);
	skinLoader_.configure(std::move(config));
}

void HudCompositorBridge::setSkinsRootForTesting(const std::string& root) {
	hudskin::SkinLoaderConfig config = skinLoader_.config();
	config.skinsRoot = root;
	skinLoader_.configure(std::move(config));
}

hudskin::SkinLoadOutcome HudCompositorBridge::requestSkin(const std::string& selection) {
	// The renderer keeps drawing the currently active shell (or wireframe)
	// throughout: the request is synchronous and nothing is drawn mid-call,
	// and the loader only releases the previous texture AFTER the candidate
	// is fully staged — syncShellTexture() then repoints the renderer before
	// the next draw.
	hudskin::SkinLoadOutcome outcome = skinLoader_.requestSkin(selection);
	syncShellTexture();

	const std::string active = outcome.activeSkinIdAfter ? *outcome.activeSkinIdAfter : std::string("none (wireframe)");
	for (const auto& d : outcome.optionalDiagnostics) {
		ofLogWarning("HudSkin") << "optional asset '" << d.asset << "' not used (nonfatal) at " << hudskin::stageName(d.stage)
			<< " [" << d.code << "]: " << d.detail;
	}
	if (outcome.activated) {
		ofLogNotice("HudSkin") << "optional controls/button faces: " << outcome.buttonFacesLoaded << "/3 loaded (idle, focused, active)";
		ofLogNotice("HudSkin") << "optional " << hudpresent::reactive::kSceneNextPressedPath << ": "
			<< (outcome.sceneNextPressedLoaded ? "loaded" : outcome.optionalDiagnostics.empty() ? "absent" : "invalid (fallback)");
		ofLogNotice("HudSkin") << "activated '" << selection << "'"
			<< (outcome.releasedPrevious ? " (previous skin released)" : "")
			<< "; peak GPU " << outcome.peakGpuBytesDuringRequest << " B, steady " << skinLoader_.steadyGpuBytes() << " B";
	} else {
		ofLogWarning("HudSkin") << "rejected '" << selection << "' at stage " << hudskin::stageName(outcome.stage)
			<< " [" << outcome.code << "]" << (outcome.asset.empty() ? "" : " asset=" + outcome.asset)
			<< ": " << outcome.detail << "; active: " << active
			<< (outcome.retainedPreviousActive ? " (retained)" : "");
	}
	return outcome;
}

void HudCompositorBridge::shutdownSkins() {
	renderer_.setShellTexture(nullptr); // stop referencing before release
	renderer_.setPressedArtwork(hudpresent::reactive::kSceneNextRegionId, nullptr);
	renderer_.setButtonFaces(nullptr, nullptr, nullptr);
	skinLoader_.shutdown();
}

void HudCompositorBridge::syncShellTexture() {
	const auto& active = skinLoader_.active();
	renderer_.setShellTexture(active ? skinBackend_.texture(active->handle) : nullptr);
	// Reactive Control Artwork v0.1: the optional pressed face follows the
	// active skin exactly — a replacement without it, or a failed load of it,
	// clears the pointer (the previous texture was released with its skin).
	const ofTexture* pressed = (active && active->sceneNextPressed)
		? skinBackend_.texture(active->sceneNextPressed->handle) : nullptr;
	renderer_.setPressedArtwork(hudpresent::reactive::kSceneNextRegionId, pressed);
	auto faceTex = [&](const std::optional<hudskin::OptionalSkinImage>& img) -> const ofTexture* {
		return (active && img) ? skinBackend_.texture(img->handle) : nullptr;
	};
	renderer_.setButtonFaces(active ? faceTex(active->buttonIdle) : nullptr, active ? faceTex(active->buttonFocused) : nullptr,
		active ? faceTex(active->buttonActive) : nullptr);
}
