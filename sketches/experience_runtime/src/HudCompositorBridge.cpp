#include "HudCompositorBridge.h"

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
	if (outcome.activated) {
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
	skinLoader_.shutdown();
}

void HudCompositorBridge::syncShellTexture() {
	const auto& active = skinLoader_.active();
	renderer_.setShellTexture(active ? skinBackend_.texture(active->handle) : nullptr);
}
