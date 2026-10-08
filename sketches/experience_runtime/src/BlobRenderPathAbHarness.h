#pragma once

// Opt-in (EXPERIENCE_RUNTIME_BLOB_RENDER_AB_HARNESS) pixel-equivalence check
// for VideoRegionEffectRenderer's direct-output path vs its 3-stage
// (scratchResultFbo + composite) path. Renders the same synthetic fragment
// sets — single, multiple, overlapping, large, off-canvas, fractional
// bounds, fading (alpha < 1), reduced effectAmount — through both paths for
// every Blob effect choice, over both an opaque video background
// (production background mode 1) and a transparent one, and diffs the
// RGBA readbacks. Source is a fixed still (a controlled Blob scene-only
// capture), so the comparison has no video-timing nondeterminism.
//
// Runs synchronously from ofApp::setup() (GL context already live), logs a
// RESULT line, writes captures/blob_render_ab/*.png, then ofExit()s.
class BlobRenderPathAbHarness {
public:
	static bool isRequested();
	// Returns true if every case met its equivalence bound.
	static bool run();
};
