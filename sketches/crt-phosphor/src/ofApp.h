#pragma once

#include "ofMain.h"
#include "CrtPhosphorPipeline.h"
#include <array>

// crt-phosphor: standalone experimental sketch around the shared, header-only
// crt::Pipeline (shared/src/crt-phosphor/). The same pipeline backs the
// experimental CrtPhosphorScene in experience_runtime (opt-in only).
//
// Keys: B bypass  L bloom  P persistence  1/2/3 preset  D debug  R reload shaders
//       S screenshot  N/M next/prev video  V video<->pattern  F cover<->fit
//       Q quality (full/low)  T per-pass GPU timing (glFinish; diagnostic only)
//   MODE   ([ / ] step implemented modes, F1..F4 direct): image/signal construction
//   LOOK   (1/2/3): shared phosphor styling (Params)      -- independent of mode
//   PALETTE(C cycles manual override: off(green fallback) / amber-teal / violet-cyan)
//   K/J tune the active mode's main parameter, H toggles excavation direction,
//   , / . secondary influence.  Theme palette: no scene-accessible API exists,
//   so precedence is manual override -> (theme: unreachable) -> green fallback.
//   STRUCTURE x/X next/prev   COLOR MAPPING z/Z next/prev   BEHAVIOR a/A next/prev
//   g dither off/low/high   y seed   -/= pitch   \\ dense   e calm/energized floating
//   0 reset to reference   w hide/show dev panel   o save recipe   i load next recipe

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;
	void windowResized(int w, int h) override;
	void exit() override;

private:
	void allocate();
	void openVideo(int index);
	void applyPreset(int i);
	void renderSource();
	void drawPattern(float w, float h);
	void savePng(const std::string& tag);
	void autoshotStep();
	void runAction(const std::string& a);
	void buildMatrixScript();
	std::string modeParamText() const;
	void setMode(crt::Mode m);
	void setPalette(int i);
	crt::Palette resolvePalette() const;
	// display axes (independent of composition mode)
	void setStructure(crt::Structure s);
	void setBehavior(crt::Behavior b);
	void setCmap(crt::ColorMap c);
	void resetToReference();
	void captureTile(const std::string& label);
	void composeSheet(const std::string& name, int cols);
	void saveRecipe();
	void loadRecipe();
	void buildGalleryScript();
	std::string axesText() const;

	crt::Pipeline pipe;
	std::string shaderDir; // optional CRT_SHADER_DIR override for live iteration

	ofVideoPlayer video;
	std::vector<std::string> clips;
	int clipIndex = -1;
	bool useVideo = false, coverFit = true;
	float lastPos = 0.f;

	std::array<crt::Params, 3> presets;
	int presetIdx = 0;
	crt::Params P;

	crt::Mode mode = crt::Mode::PhosphorPortrait;      // MODE
	std::array<crt::Palette, 3> manualPal;
	int paletteIdx = 0;                                 // PALETTE: 0 = no override (fallback)
	crt::ModeSettings modeSettings;
	crt::Display disp;                                  // STRUCTURE / COLOR MAPPING / BEHAVIOR / dither / seed
	std::string warn;                                   // visible incompatibility / fallback report
	bool showPanel = true;
	struct Tile { std::string label; ofPixels px; };
	std::vector<Tile> tiles;
	int recipeCursor = 0;
	float secInf = 0.30f;
	float simTime = 0.f;                                // fixed-dt sim clock (CRT_FIXED_DT) or wall clock
	float fixedDt = 0.f;
	float freezeAt = -1.f; bool frozen = false;
	std::vector<std::pair<int, std::string>> script;    // CRT_MATRIX_DIR scripted run
	size_t scriptPos = 0;
	std::string perfLog;
	bool awaitingFrame = false; // true from openVideo() until the NEW clip delivers its first frame
	bool bypass = false, bloomOn = true, persistOn = true, debug = true, timing = false;
	int quality = 0;
	std::string lastShot;
	double frameMsAvg = 0;
	double frameMsSum = 0;
	int frameMsN = 0;

	// auto-capture (CRT_AUTOSHOT_DIR) for reproducible acceptance screenshots
	std::string autoDir;
	int autoFrame = 0;
};
