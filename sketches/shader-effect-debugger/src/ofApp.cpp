#include "ofApp.h"
#include "EffectKnowledgePack.h"
#include "EffectPresetId.h"
#include "KnowledgePackSelfTest.h"
#include "MotionExtractionEffect.h"

using namespace videoeffects;

namespace {
	constexpr float kToastDuration = 2.0f;
}

void ofApp::setup() {
	ofSetWindowTitle("shader-effect-debugger");
	ofBackground(10, 10, 12);
	ofSetVerticalSync(true);
	ofEnableAlphaBlending();

	VideoEffectServiceConfig config;
	service.setup(config);
	ofLogNotice("ofApp") << "registered effects: " << service.registry().allIds().size();

	for (const auto & id : service.registry().allIds()) {
		const VideoEffectDefinition * def = service.getDefinition(id);
		if (def != nullptr && def->debuggerAvailable) {
			effectIds.push_back(id);
		}
	}

	// Engineering Session 2 acceptance criterion: prove the debugger's
	// export path and the shared import path round-trip real knowledge
	// without semantic loss, using the real registered catalog to exercise
	// unknown-effect-id rejection. Runs unconditionally at startup (not
	// gated behind a keypress) so this is provable from a console log
	// without GUI interaction — see KnowledgePackSelfTest.h.
	runKnowledgePackRoundTripSelfTest(service.registry().allIds());

	scanMediaFiles();
	if (!videoFiles.empty()) {
		loadVideo(0);
	} else {
		ofLogWarning("ofApp") << "no media found in bin/data/media/ — drop .mp4 clips there to preview effects against real footage";
	}

	gui.setup("Shader Effect Debugger");
	globalGroup.setName("Global");
	globalGroup.add(pEvolutionEnabled);
	globalGroup.add(pDriftEnabled);
	globalGroup.add(pSeed);
	gui.add(globalGroup);

	if (!effectIds.empty()) {
		switchToEffect(0);
	} else {
		ofLogError("ofApp") << "no effects registered — nothing to preview";
	}
}

// ----------------------------------------------------------------- video ----

void ofApp::scanMediaFiles() {
	videoFiles.clear();
	ofDirectory dir(ofToDataPath("media", true));
	if (!dir.exists()) return;
	dir.allowExt("mp4");
	dir.allowExt("mov");
	dir.listDir();
	for (size_t i = 0; i < dir.size(); ++i) {
		videoFiles.push_back(dir.getPath(i));
	}
}

void ofApp::loadVideo(int index) {
	if (videoFiles.empty()) return;
	index = ((index % static_cast<int>(videoFiles.size())) + static_cast<int>(videoFiles.size())) % static_cast<int>(videoFiles.size());
	currentVideoIndex = index;
	video.load(videoFiles[index]);
	video.play();
	videoPaused = false;
	showToast("Video: " + ofFilePath::getFileName(videoFiles[index]));
}

void ofApp::nextVideo() {
	if (videoFiles.empty()) return;
	loadVideo(currentVideoIndex + 1);
}

void ofApp::prevVideo() {
	if (videoFiles.empty()) return;
	loadVideo(currentVideoIndex - 1);
}

void ofApp::togglePausePlay() {
	videoPaused = !videoPaused;
	video.setPaused(videoPaused);
}

// ---------------------------------------------------------------- effects ----

VideoEffectParameters ofApp::defaultParamsForCurrent() const {
	VideoEffectParameters result;
	if (currentEffectIndex < 0 || currentEffectIndex >= static_cast<int>(effectIds.size())) return result;
	const VideoEffectDefinition * def = service.getDefinition(effectIds[currentEffectIndex]);
	if (def == nullptr) return result;
	for (const auto & p : def->params) {
		result.set(p.id, p.defaultValue);
	}
	return result;
}

void ofApp::switchToEffect(int index) {
	if (effectIds.empty()) return;
	index = ((index % static_cast<int>(effectIds.size())) + static_cast<int>(effectIds.size())) % static_cast<int>(effectIds.size());

	// Skip ids that fail to instantiate (e.g. a shader failed to compile) —
	// try the whole ring once before giving up, so one broken effect doesn't
	// strand the debugger.
	for (int attempts = 0; attempts < static_cast<int>(effectIds.size()); ++attempts) {
		auto instance = service.createInstance(effectIds[index]);
		if (instance) {
			currentEffectIndex = index;
			currentInstance = std::move(instance);
			auxMotionExtraction.reset();
			if (effectIds[index] == "motion_composite") {
				auxMotionExtraction = service.createInstance("motion_extraction");
			}
			rebuildEffectGui();
			resetCurrentToDefaults();
			const VideoEffectDefinition * def = service.getDefinition(effectIds[index]);
			if (def != nullptr) {
				evolution.reset(*def, currentRenderParams);
				PatternDriftConfig driftConfig;
				driftConfig.enabled = pDriftEnabled.get();
				drift.setup(*def, driftConfig);
			}
			showToast("Effect: " + effectIds[index]);
			return;
		}
		ofLogError("ofApp") << "failed to create instance for effect: " << effectIds[index] << " — skipping";
		index = (index + 1) % static_cast<int>(effectIds.size());
	}

	ofLogError("ofApp") << "every registered effect failed to instantiate";
	currentInstance.reset();
}

void ofApp::nextEffect() { switchToEffect(currentEffectIndex + 1); }
void ofApp::prevEffect() { switchToEffect(currentEffectIndex - 1); }

void ofApp::rebuildEffectGui() {
	// Clear + rebuild rather than swap in place — no addListener() is used
	// anywhere in this class (values are pulled each frame in
	// pullGuiIntoCurrentParams() instead), so there is nothing that can dangle
	// when the old ofParameter storage is destroyed here.
	guiFloatParams.clear();
	guiIntParams.clear();
	guiBoolParams.clear();
	effectParamGroup.clear();

	if (currentEffectIndex < 0) return;
	const VideoEffectDefinition * def = service.getDefinition(effectIds[currentEffectIndex]);
	if (def == nullptr) return;

	effectParamGroup.setName(def->displayName.empty() ? def->id : def->displayName);

	for (const auto & schema : def->params) {
		if (!schema.visibleInDebugger) continue;
		switch (schema.type) {
			case VideoEffectParameterType::Float: {
				auto entry = std::make_shared<GuiFloatParam>();
				entry->id = schema.id;
				entry->param.set(schema.label.empty() ? schema.id : schema.label, asFloat(schema.defaultValue), asFloat(schema.hardMin), asFloat(schema.hardMax));
				guiFloatParams.push_back(entry);
				effectParamGroup.add(entry->param);
				break;
			}
			case VideoEffectParameterType::Int: {
				auto entry = std::make_shared<GuiIntParam>();
				entry->id = schema.id;
				entry->param.set(schema.label.empty() ? schema.id : schema.label, asInt(schema.defaultValue), asInt(schema.hardMin), asInt(schema.hardMax));
				guiIntParams.push_back(entry);
				effectParamGroup.add(entry->param);
				break;
			}
			case VideoEffectParameterType::Bool: {
				auto entry = std::make_shared<GuiBoolParam>();
				entry->id = schema.id;
				entry->param.set(schema.label.empty() ? schema.id : schema.label, asBool(schema.defaultValue));
				guiBoolParams.push_back(entry);
				effectParamGroup.add(entry->param);
				break;
			}
			default:
				break; // Vec2/3/4 — not GUI-exposed in v1, see EffectRandomizer.h
		}
	}

	gui.add(effectParamGroup);
}

void ofApp::pullGuiIntoCurrentParams() {
	for (const auto & e : guiFloatParams) currentRenderParams.set(e->id, e->param.get());
	for (const auto & e : guiIntParams) currentRenderParams.set(e->id, e->param.get());
	for (const auto & e : guiBoolParams) currentRenderParams.set(e->id, e->param.get());
}

void ofApp::pushParamsIntoGui(const VideoEffectParameters & params) {
	// Sliders track the animated value while evolution/drift are active —
	// they remain technically editable (no widget-level lock implemented),
	// but a manual edit is overwritten again next frame, same net effect.
	for (auto & e : guiFloatParams) e->param.set(params.getFloat(e->id, e->param.get()));
	for (auto & e : guiIntParams) e->param.set(params.getInt(e->id, e->param.get()));
	for (auto & e : guiBoolParams) e->param.set(params.getBool(e->id, e->param.get()));
}

void ofApp::resetCurrentToDefaults() {
	currentRenderParams = defaultParamsForCurrent();
	pushParamsIntoGui(currentRenderParams);
	showToast("Reset to defaults");
}

void ofApp::randomizeCurrent() {
	if (currentEffectIndex < 0) return;
	const VideoEffectDefinition * def = service.getDefinition(effectIds[currentEffectIndex]);
	if (def == nullptr) return;

	RandomizeRequest req;
	req.effectId = def->id;
	if (pSeed.get() != 0) req.seed = static_cast<uint32_t>(pSeed.get());

	currentRenderParams = service.randomizer().generate(req, *def, &service.knowledgeBase());
	pushParamsIntoGui(currentRenderParams);
	showToast("Randomized");
}

void ofApp::saveCurrentToWhitelist() {
	if (currentEffectIndex < 0) return;
	pullGuiIntoCurrentParams();
	KnowledgeEntry entry;
	entry.effect = effectIds[currentEffectIndex];
	entry.list = "whitelist";
	entry.sourceSketch = "shader-effect-debugger";
	entry.sourceVideo = currentVideoIndex >= 0 ? ofFilePath::getFileName(videoFiles[currentVideoIndex]) : "";
	for (const auto & e : guiFloatParams) entry.snapshot[e->id] = e->param.get();
	for (const auto & e : guiIntParams) entry.snapshot[e->id] = static_cast<float>(e->param.get());
	for (const auto & e : guiBoolParams) entry.snapshot[e->id] = e->param.get() ? 1.0f : 0.0f;

	// Stable canonical preset identity (DEC-016 / Shared Effects
	// Architecture-Closure Session): every entry saved to the whitelist
	// through THIS action is, by definition, a "reusable authored/favored
	// preset" -- exactly the category DEC-016 requires a presetId for
	// before schema freeze. The slug is a timestamp plus a per-run
	// disambiguating counter, deliberately NOT derived from `entry.snapshot`
	// -- DEC-016 explicitly prohibits generating a preset id "only from
	// current floating-point snapshot serialization." A blank/failed
	// synthesis (should not happen for a well-formed effect id) leaves
	// presetId unset rather than saving a malformed one -- the entry still
	// saves as a legacy-anonymous preset in that case, not a hard failure.
	static int presetSaveCounter = 0;
	std::string slug = ofGetTimestampString("%Y%m%d_%H%M%S") + "_" + ofToString(++presetSaveCounter);
	std::string candidateId = videoeffects::synthesizeMigrationPresetId(entry.effect, slug);
	if (!candidateId.empty()) entry.presetId = candidateId;

	bool ok = service.knowledgeBase().appendWhitelist(entry);
	showToast(ok ? "Saved to whitelist" : "Save failed (duplicate or write error — see log)");
}

void ofApp::saveCurrentToBlacklist() {
	if (currentEffectIndex < 0) return;
	pullGuiIntoCurrentParams();
	KnowledgeEntry entry;
	entry.effect = effectIds[currentEffectIndex];
	entry.list = "blacklist";
	entry.sourceSketch = "shader-effect-debugger";
	entry.sourceVideo = currentVideoIndex >= 0 ? ofFilePath::getFileName(videoFiles[currentVideoIndex]) : "";
	for (const auto & e : guiFloatParams) entry.snapshot[e->id] = e->param.get();
	for (const auto & e : guiIntParams) entry.snapshot[e->id] = static_cast<float>(e->param.get());
	for (const auto & e : guiBoolParams) entry.snapshot[e->id] = e->param.get() ? 1.0f : 0.0f;

	bool ok = service.knowledgeBase().appendBlacklist(entry);
	showToast(ok ? "Saved to blacklist" : "Save failed (duplicate or write error — see log)");
}

void ofApp::exportKnowledgePack() {
	// Bundles every known effect id's accumulated whitelist/blacklist —
	// not just the currently-loaded one — so the pack always reflects a
	// full, current snapshot of this debugger's knowledge, per
	// EffectKnowledgePack.h's "full export snapshot, not an incremental
	// log" contract.
	//
	// Written directly to the canonical AUTHORED root (DEC-016 / Shared
	// Effects Architecture-Closure Session), not this sketch's own
	// bin/data/ — per that decision's own wording, "per-sketch copies are
	// derived deployment/build artifacts," and this export action IS the
	// authoring step, not a deployment step. Every consuming sketch
	// (including this debugger's own bin/data/, if it wants a locally
	// runnable copy) receives its copy from
	// scripts/sync-video-effect-assets.py syncing OUT of this same path,
	// never by reading it directly at runtime — see that script's
	// sync_knowledge_pack() and this session's implementation report,
	// "Canonical path/distribution evidence."
	const std::string outputPath = "../../../../assets/shared/video-effects/knowledge/effect-knowledge-pack.json";
	bool ok = videoeffects::exportEffectKnowledgePack(service.knowledgeBase(), effectIds, outputPath, "shader-effect-debugger");
	showToast(ok ? "Exported knowledge pack" : "Export failed — see log");
}

// ------------------------------------------------------------------ loop ----

void ofApp::update() {
	video.update();

	float dt = ofGetLastFrameTime();
	if (dt <= 0.0f || dt > 0.5f) dt = 1.0f / 60.0f;

	pullGuiIntoCurrentParams();

	drift.setConfig([this] {
		PatternDriftConfig c = drift.getConfig();
		c.enabled = pDriftEnabled.get();
		return c;
	}());

	if (pEvolutionEnabled.get()) {
		evolution.update(dt, &service.randomizer(), &service.knowledgeBase());
		currentRenderParams = evolution.getCurrent();
		pushParamsIntoGui(currentRenderParams);
	}

	if (pDriftEnabled.get()) {
		bool transitionActive = pEvolutionEnabled.get() && evolution.getPhase() == EvolutionPhase::Transitioning;
		drift.update(dt, transitionActive);
		currentRenderParams = drift.apply(currentRenderParams);
	}

	if (currentInstance) {
		VideoEffectContext context;
		if (video.isLoaded()) {
			context.sourceTexture = &video.getTexture();
			context.sourcePixels = &video.getPixels();
		}
		context.time = ofGetElapsedTimef();
		context.deltaTime = dt;
		currentInstance->update(context, currentRenderParams);
	}

	if (auxMotionExtraction && video.isLoaded()) {
		VideoEffectContext auxContext;
		auxContext.sourceTexture = &video.getTexture();
		auxContext.time = ofGetElapsedTimef();
		auxContext.deltaTime = dt;
		VideoEffectParameters auxParams; // defaults are fine for preview purposes
		auxMotionExtraction->update(auxContext, auxParams);
	}
}

void ofApp::draw() {
	ofRectangle destRect(0, 0, static_cast<float>(ofGetWidth()), static_cast<float>(ofGetHeight()));

	if (currentInstance && video.isLoaded()) {
		VideoEffectContext context;
		context.sourceTexture = &video.getTexture();
		context.sourcePixels = &video.getPixels();
		context.destinationRect = destRect;
		context.time = ofGetElapsedTimef();
		context.deltaTime = ofGetLastFrameTime();
		context.alpha = 1.0f;

		if (auxMotionExtraction) {
			auto * me = dynamic_cast<MotionExtractionEffect *>(auxMotionExtraction.get());
			if (me != nullptr) {
				context.auxiliaryTextures["motionTex"] = &me->getMotionTexture();
				context.auxiliaryTextures["motionDelayedTex"] = &me->getDelayedMotionTexture();
			}
		}

		currentInstance->render(context, currentRenderParams);
	} else {
		ofSetColor(120);
		ofDrawBitmapString(
			video.isLoaded() ? "No effect selected" : "No video loaded — drop .mp4/.mov clips in bin/data/media/", 20, ofGetHeight() * 0.5f);
	}

	// --- overlay ---
	ofSetColor(255);
	std::string effectName = currentEffectIndex >= 0 ? effectIds[currentEffectIndex] : "(none)";
	std::string videoName = currentVideoIndex >= 0 ? ofFilePath::getFileName(videoFiles[currentVideoIndex]) : "(none)";
	std::string phaseStr = pEvolutionEnabled.get() ? (evolution.getPhase() == EvolutionPhase::Transitioning ? "transitioning" : "holding") : "off";

	std::stringstream ss;
	ss << "EFFECT: " << effectName << "  (" << (currentEffectIndex + 1) << "/" << effectIds.size() << ")  [Left/Right cycle]\n";
	ss << "VIDEO:  " << videoName << "  [/] cycle, [space] pause\n";
	ss << "evolution: " << phaseStr << "   drift: " << (pDriftEnabled.get() ? "on" : "off") << "\n";
	ss << "[r] randomize  [0] reset  [w] whitelist  [b] blacklist  [k] export pack  [e] evolution  [p] drift\n";

	const VideoEffectDefinition * def = currentEffectIndex >= 0 ? service.getDefinition(effectIds[currentEffectIndex]) : nullptr;
	if (def != nullptr) {
		ss << "kind: " << toString(def->kind);
		if (!def->capabilities.safeForAutomaticSelection) ss << "  [NOT VALIDATED for automatic selection]";
		ss << "\n";
	}

	if (ofGetElapsedTimef() - lastToastAt < kToastDuration) {
		ss << ">> " << lastToastMessage << "\n";
	}

	ofDrawBitmapStringHighlight(ss.str(), 20, 20);

	gui.draw();
}

void ofApp::keyPressed(int key) {
	if (key == OF_KEY_LEFT) {
		prevEffect();
	} else if (key == OF_KEY_RIGHT) {
		nextEffect();
	} else if (key == '[') {
		prevVideo();
	} else if (key == ']') {
		nextVideo();
	} else if (key == ' ') {
		togglePausePlay();
	} else if (key == 'r' || key == 'R') {
		randomizeCurrent();
	} else if (key == '0') {
		resetCurrentToDefaults();
	} else if (key == 'w' || key == 'W') {
		saveCurrentToWhitelist();
	} else if (key == 'b' || key == 'B') {
		saveCurrentToBlacklist();
	} else if (key == 'k' || key == 'K') {
		exportKnowledgePack();
	} else if (key == 'e' || key == 'E') {
		pEvolutionEnabled.set(!pEvolutionEnabled.get());
	} else if (key == 'p' || key == 'P') {
		pDriftEnabled.set(!pDriftEnabled.get());
	}
}

void ofApp::showToast(const std::string & message) {
	lastToastMessage = message;
	lastToastAt = ofGetElapsedTimef();
}
