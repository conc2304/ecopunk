#pragma once

#include <string>
#include <vector>
#include "ofImage.h"
#include "ofParameter.h"
#include "ofShader.h"

// One ambient background texture (leaf vein tracery, woodgrain, moss,
// contour lines, ...), loaded from a _tint/_mask PNG pair produced by
// prep_textures.sh. Category (display name / underlay-vs-overlay /
// opacity default) comes from textures_manifest.json next to the images,
// not from the filename -- prep_textures.sh's output files are named by
// UUID with no semantic info, so there's nothing in the filename itself to
// key off of.
struct TFBackgroundTexture {
	enum class Layer { UNDERLAY, OVERLAY };

	std::string baseName;
	std::string displayName;
	Layer layer = Layer::UNDERLAY;

	// Reserved for schema parity with sibling sketches that do have a Zone
	// A/B divider (e.g. blueprint_emergence's GridSystem/AnnotationRenderer).
	// Temporal Fields has no such divider, so this is carried through but
	// never enforced here.
	bool spansBothZones = false;

	// Independent of `layer` -- which side of the composition a texture
	// draws on (underlay/overlay) says nothing about how its pixels combine
	// with what's beneath it. Not derived from layer in code even though in
	// practice most overlays end up "screen" and most underlays "multiply".
	ofBlendMode blendMode = OF_BLENDMODE_ALPHA;

	ofImage tint;
	ofImage mask; // loaded per spec; not yet composited -- reserved for a future per-pixel alpha-mask draw pass

	// 0..kMaxOpacity. Opacity lives here, at runtime, never baked into the
	// PNG pixels -- prep_textures.sh intentionally leaves the source at
	// full contrast/opacity for exactly this reason.
	ofParameter<float> opacity;
};

// Loads every matched _tint/_mask pair from a folder and draws them as two
// full-canvas ambient layers -- underlay (beneath fragments) and overlay
// (above fragments and the HUD's measurement-line annotations, i.e. after
// TFHudLayer::drawOverlay()). Deliberately independent of TFHudLayer's
// underlay/overlay widget rotation: that pool exists to keep HUD chrome
// from feeling too busy by showing only 1-2 widgets at a time, but these
// textures need their opacity sliders to be live and effective every
// frame, not gated behind a rotation roll.
class TFAmbientTextureLayer {
	public:
		static constexpr float kMaxOpacity = 0.4f;

		// folderPath: directory containing <uuid>_tint.png / <uuid>_mask.png
		// pairs plus a textures_manifest.json (see bin/data/backgrounds/).
		// Missing/unreadable/unmatched files are logged and skipped, never
		// fatal.
		void setup(int canvasW, int canvasH, const std::string& folderPath);

		// drawLayer() derives every draw rect from canvasW/canvasH fresh
		// each call, so this is the only state a resize needs to update.
		void resizeCanvas(int canvasW_, int canvasH_) {
			canvasW = canvasW_;
			canvasH = canvasH_;
		}

		void drawUnderlay() const;
		void drawOverlay() const;

		// Mounted into TFParameterPanel's ofxGui panel by the caller --
		// this class only builds the group, it doesn't know about ofxPanel.
		ofParameterGroup& getParamGroup() { return group; }

	private:
		void drawLayer(TFBackgroundTexture::Layer layer) const;

		std::vector<TFBackgroundTexture> textures;
		ofParameterGroup group;
		int canvasW = 0;
		int canvasH = 0;

		// SCREEN/MULTIPLY need this to make opacity visible at all (see
		// textureBlendFade.frag) -- ALPHA mode doesn't use it. mutable so it
		// can stay lazily-loaded from inside drawLayer(), which is const
		// (drawUnderlay()/drawOverlay() already were, from the prior phase).
		mutable ofShader blendFadeShader;
		mutable bool blendFadeShaderLoaded = false;
};
