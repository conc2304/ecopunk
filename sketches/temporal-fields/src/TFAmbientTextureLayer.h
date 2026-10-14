#pragma once

#include <string>
#include <vector>
#include "ofImage.h"
// oF 0.12.0's ofImage.h only forward-declares ofPixels_; ofImage members
// here need the complete type (0.12.1 includes it from ofImage.h).
#include "ofPixels.h"
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
	// Not yet composited -- reserved for a future per-pixel alpha-mask draw
	// pass. Under Residency::Bounded it is not decoded at setup (nothing
	// samples it); TFAmbientTextureLayer::ensureMaskLoaded() loads it from
	// maskPath on first request. The file always stays in the asset package.
	ofImage mask;
	std::string maskPath;

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
		bool isLoaded() const { return loaded; }

		// RT-004 Priority C. Legacy: every tint AND mask decoded and kept as
		// CPU pixels + GL texture for the process lifetime. Bounded (default):
		// the same 19 tint textures stay GPU-resident (every one is drawn
		// every frame), but their CPU pixels are released once the texture is
		// confirmed allocated (drawing only binds the texture), and masks --
		// never sampled by any draw path -- are not decoded until
		// ensureMaskLoaded() asks for one. Chosen once at setup by
		// EXPERIENCE_RUNTIME_TEMPORAL_AMBIENT_RESIDENCY=legacy|bounded
		// (verification/attribution toggle, not a user setting).
		enum class Residency { Legacy, Bounded };
		Residency residency() const { return residencyMode; }

		// Lazily decodes + uploads texture `index`'s mask (main/GL thread
		// only, like every other ambient GL call). Returns false if the
		// index is out of range or the file cannot be loaded.
		bool ensureMaskLoaded(size_t index);
		// Releases texture `index`'s mask (CPU pixels + GL texture); the next
		// ensureMaskLoaded(index) reloads it from disk. Main/GL thread only.
		bool releaseMask(size_t index);
		const TFBackgroundTexture& textureAt(size_t index) const { return textures[index]; }

		// Resource reporting. CPU = decoded pixel bytes still held; GPU =
		// estimate from texture dimensions x channels (a driver may pad RGB
		// to 4 bytes/pixel -- see gpuBytesEstimate(true)).
		size_t textureCount() const { return textures.size(); }
		size_t pixelBytes() const { return tintPixelBytes() + maskPixelBytes(); }
		size_t tintPixelBytes() const;
		size_t maskPixelBytes() const;
		size_t residentTintTextures() const;
		size_t residentMaskTextures() const;
		size_t gpuBytesEstimate(bool padRgbToRgba = false) const;

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

		// Never copied or reallocated after setup() (reserved up front): an
		// ofImage copy re-creates its texture from its CPU pixels, which
		// Residency::Bounded has released.
		std::vector<TFBackgroundTexture> textures;
		bool loaded = false;
		Residency residencyMode = Residency::Bounded;
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
