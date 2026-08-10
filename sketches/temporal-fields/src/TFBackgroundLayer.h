#pragma once

#include <string>
#include "EffectActivityStatus.h"
#include "ShaderLibrary.h"
#include "TFEffectPicker.h"
#include "TFImageCycler.h"
#include "TimeOffsetVideoBuffer.h"
#include "ofRectangle.h"

// Orchestrates the background layer's three content modes on a weighted,
// timed pick — the same pattern used everywhere else in this sketch
// (transition styles, per-fragment offsets): Full Video (raw or one of
// TFEffectPicker's 16 shader effects), Full Image (TFImageCycler's
// crossfade), or Split (canvas divided by one axis-aligned line, video on
// one side, image on the other, each cropped to fill its own region with
// no scaling — see TFTextureCropFill.h). Drawn in ofApp::draw() before
// composition.draw() — everything else in this sketch (Blob Grid's mask
// gaps, BSP's transparency) shows this layer through, not a flat clear.
class TFBackgroundLayer {
	public:
		enum class Mode { FULL_VIDEO, FULL_IMAGE, SPLIT };

		struct Params {
			float fullVideoWeight = 40.0f;
			float fullImageWeight = 30.0f;
			float splitWeight = 30.0f;
			float modeChangeInterval = 20.0f;

			float splitRatio = 0.5f; // 0..1, position of the split line
			int splitAxisChoice = 0; // 0 = Random, 1 = Force Vertical, 2 = Force Horizontal

			TFEffectPicker::Weights effectWeights;
			TFImageCycler::Params imageParams;
		};

		void setup(TimeOffsetVideoBuffer* videoBuffer, ShaderLibrary* shaderLib,
			const std::string& imagesFolder, int canvasW, int canvasH, const Params& params);
		void setParams(const Params& p);
		// draw() derives every rect from canvasW/canvasH fresh each call, so
		// this is the only state a resize needs to update.
		void resizeCanvas(int canvasW_, int canvasH_) {
			canvasW = canvasW_;
			canvasH = canvasH_;
		}
		void update(float dt);
		void draw();

		Mode getCurrentMode() const { return currentMode; }
		std::string getCurrentEffectName() const { return effectPicker.getCurrentEffectName(); } // "" = raw

		// Temporal Production Scene #2 Migration: narrow one-level
		// forward of TFEffectPicker::activityStatus() (the accepted
		// DEC-015 canonical accessor -- see TFEffectPicker.h) so a
		// caller that only holds a TFBackgroundLayer (TemporalSceneCore)
		// can reach it without reaching into effectPicker's private
		// member directly. Returns the value unchanged -- no field is
		// added, removed, or reconstructed here.
		videoeffects::EffectActivityStatus effectActivityStatus() const { return effectPicker.activityStatus(); }

	private:
		void pickNextMode();
		void drawFullVideo();
		void drawFullImage();
		void drawSplit();

		TimeOffsetVideoBuffer* videoBuffer = nullptr;
		TFEffectPicker effectPicker;
		TFImageCycler imageCycler;
		Params params;
		int canvasW = 0;
		int canvasH = 0;

		Mode currentMode = Mode::FULL_VIDEO;
		bool currentSplitIsVertical = true; // resolved once per mode-pick if axis == Random
		bool videoOnFirstHalf = true; // resolved once per mode-pick, for variety
		float modeTimer = 0.0f;
};
