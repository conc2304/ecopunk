#pragma once

#include "EffectEvolutionController.h"
#include "PatternDriftController.h"
#include "VideoEffectService.h"
#include "ofMain.h"
#include "ofxGui.h"
#include <memory>
#include <vector>

// Standalone shader/effect preview + tuning app — the extraction target
// described in docs/implement-shader-effect-debugger-and-service.md Phase 2
// and docs/shared-video-effect-architecture.md §8, built against
// VideoEffectService rather than duplicating quadrant-crosshair/src/DebugMode's
// shader-cycling logic. See that class for the original single-preview +
// live-param-panel pattern this generalizes.
class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;

private:
	// --- effect navigation ---
	void switchToEffect(int index);
	void nextEffect();
	void prevEffect();
	void rebuildEffectGui();
	void pullGuiIntoCurrentParams();
	void pushParamsIntoGui(const videoeffects::VideoEffectParameters & params);
	videoeffects::VideoEffectParameters defaultParamsForCurrent() const;

	// --- video navigation ---
	void scanMediaFiles();
	void loadVideo(int index);
	void nextVideo();
	void prevVideo();

	// --- actions ---
	void randomizeCurrent();
	void resetCurrentToDefaults();
	void saveCurrentToWhitelist();
	void saveCurrentToBlacklist();
	void togglePausePlay();

	// Shared Effect Knowledge, scoped extension: bundles every effect's
	// accumulated whitelist/blacklist entries into one cross-scene-consumable
	// pack file — see shared/src/video-effects/knowledge/EffectKnowledgePack.h.
	void exportKnowledgePack();

	videoeffects::VideoEffectService service;
	std::vector<std::string> effectIds;
	int currentEffectIndex = -1;
	std::unique_ptr<videoeffects::VideoEffectInstance> currentInstance;
	std::unique_ptr<videoeffects::VideoEffectInstance> auxMotionExtraction; // only used when the active effect is motion_composite

	ofVideoPlayer video;
	std::vector<std::string> videoFiles;
	int currentVideoIndex = -1;
	bool videoPaused = false;

	videoeffects::EffectEvolutionController evolution;
	videoeffects::PatternDriftController drift;
	videoeffects::VideoEffectParameters currentRenderParams; // resolved each update(): GUI, or evolution+drift applied on top

	// --- GUI ---
	ofxPanel gui;
	ofParameterGroup globalGroup;
	ofParameter<bool> pEvolutionEnabled { "Scene evolution", false };
	ofParameter<bool> pDriftEnabled { "Pattern drift", false };
	ofParameter<int> pSeed { "Seed (0 = random)", 0, 0, 999999 };

	ofParameterGroup effectParamGroup;
	// Backing storage for the dynamically-rebuilt effect param group — cleared
	// and repopulated on every effect switch (see rebuildEffectGui()). Using
	// separate typed vectors instead of a type-erased container because
	// ofParameter<T> is the type ofxGui widgets bind to directly; kept small
	// (one entry per schema param) so the churn of rebuilding per switch is
	// cheap. Values are pulled into VideoEffectParameters once per frame
	// rather than via addListener(), so there is nothing to unhook when the
	// group is rebuilt — see this file's rebuildEffectGui().
	struct GuiFloatParam {
		std::string id;
		ofParameter<float> param;
	};
	struct GuiIntParam {
		std::string id;
		ofParameter<int> param;
	};
	struct GuiBoolParam {
		std::string id;
		ofParameter<bool> param;
	};
	std::vector<std::shared_ptr<GuiFloatParam>> guiFloatParams;
	std::vector<std::shared_ptr<GuiIntParam>> guiIntParams;
	std::vector<std::shared_ptr<GuiBoolParam>> guiBoolParams;

	float lastToastAt = -1000.0f;
	std::string lastToastMessage;
	void showToast(const std::string & message);
};
