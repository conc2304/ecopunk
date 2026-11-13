#pragma once

#include "ofMain.h"
#include "PhosphorContoursPipeline.h"
#include <array>
#include <string>
#include <vector>

// phosphor-contours: standalone developer sketch around the shared, header-only phc::Pipeline
// (shared/src/phosphor-contours/). The same pipeline backs the opt-in PhosphorContoursScene in
// experience_runtime. No shader logic lives here.
//
// Media: sketch-local bin/data/media/ if it has clips, else the canonical shared library
// (assets/shared/media via SharedMediaRoot.h). No camera, no local playlist policy (N/M just step the
// sorted file list for developer convenience).
//
// Keys:  1/2/3 preset  A algorithm  B bare (all finishing off)  D dropout  L slip  K grain  V view (final/source/luma/contour/raster)  R reseed (seed+1)  0 reset preset+seed
//        Up/Down select parameter  Left/Right adjust  Q quality (desktop/pisafe)  P persistence  G glow
//        N/M next/prev video  T GPU timing  H hide panel  S screenshot (bin/data/captures/)
// Env:   PHC_VIDEO=<substr> PHC_PRESET=0..2 PHC_SEED=n PHC_FIXED_DT=sec PHC_FREEZE=0..1 PHC_QUALITY=pisafe
//        PHC_SET="levels=20,blur=6,..." (knob names as in the panel, plus toneFill)
//        PHC_ALGO=0..2 (Isophote/RasterRelief/Hybrid)  PHC_MATRIX=<tag> (with PHC_AUTOSHOT_DIR: frozen-frame A/B contact sheet)
//        PHC_BENCH=1 (with PHC_AUTOSHOT_DIR: ms/frame + memory for every algorithm x quality, then exits)
//        PHC_SEQ=<N> (with PHC_AUTOSHOT_DIR + PHC_FREEZE=<start 0..1>: N frame-accurate motion frames seq_NNN.png + temporal stability stats)
//        PHC_AUTOSHOT_DIR=<dir> (captures source/luma/contour/raster/final + a second clip, prints timing, exits)

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;
	void windowResized(int w, int h) override;
	void exit() override;

private:
	struct Knob { const char* name; float* v; float step; float lo, hi; };

	void openVideo(int index);
	void applyPreset(int i);
	void allocate();
	void renderSource();
	void drawPattern(float w, float h);
	void buildKnobs();
	void shot(const std::string& name, ofFbo& fbo);
	void autoshotStep();
	void matrixStep();
	void benchStep();
	void seqStep();
	// PHC_SEQ=<N>: deterministic motion series. Video is stepped frame-accurately (1/30 s per captured frame, no
	// real-time dependence), effect time advances 1/30 s per frame, every frame is read back and saved, and
	// frame-to-frame change of the effect vs the source is measured.
	int seqN = 0, seqIdx = 0;
	std::vector<unsigned char> seqPrevOut, seqPrevSrc;
	double seqSumOut = 0, seqSumSrc = 0, seqMaxOut = 0, seqFracBig = 0;
	bool bench = false;
	int bStage = 0;

	phc::Pipeline pipe;
	std::array<phc::Params, phc::kPresetCount> presets;
	phc::Params P, Pbase; // Pbase = preset + env overrides (matrix mode restores from it)
	phc::Pipeline::Options opt;
	std::vector<Knob> knobs;
	int knob = 0, presetIdx = 0;
	phc::View viewMode = phc::View::Final;
	phc::Quality quality = phc::Quality::Desktop;

	ofVideoPlayer video;
	std::vector<std::string> clips;
	int clipIndex = 0;
	bool useVideo = false, awaitingFrame = false, showPanel = true, timing = false, frozen = false;
	float lastPos = 0.f, simTime = 0.f, fixedDt = 0.f, freezeAt = -1.f;

	std::string autoDir;
	int autoFrame = 0, autoStage = 0;
	std::vector<double> autoMs;
	// PHC_MATRIX=<tag>: same frozen frame -> source, structural luma, bare Isophote/RasterRelief/Hybrid, finished x3
	// + preferred, saved individually and as one labelled contact sheet.
	std::string matrixTag;
	int mStage = 0, mAlgo = 0;
	std::vector<ofImage> mTiles;
	std::vector<std::string> mLabels;
	void grab(const std::string& label, ofFbo& fbo);
};
