#pragma once

#include "ofUtils.h"

#include <string>

// SharedMediaRoot — the one place that spells out where the canonical media
// library (assets/shared/media/, DEC-011) lives relative to a sketch.
//
// Every sketch builds to sketches/<name>/bin/, so its data root is
// sketches/<name>/bin/data/ and the repo root is four levels up. That holds on
// macOS (ofToDataPath resolves bin/data/ from inside the .app bundle), on
// Linux, and on the Pi: scripts/deploy.sh rsyncs the repo with the same
// layout and builds in place, so no platform-specific path is needed.
//
// Header-only and openFrameworks-only on purpose: sketches that exclude
// shared/src/video-playback% from their build (quadrant-crosshair,
// fragment-trail) or don't pull in shared/src at all (contour-portrait) can
// still include it by relative path. Call after ofApp setup has begun —
// ofToDataPath needs the data root openFrameworks sets at startup.
namespace ecopunk {

inline std::string sharedMediaRoot() {
	return ofToDataPath("../../../../assets/shared/media", true);
}

} // namespace ecopunk
