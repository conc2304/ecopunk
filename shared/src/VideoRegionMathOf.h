#pragma once

#include "VideoRegionMath.h"
#include "ofRectangle.h"

// ofRectangle <-> VideoRegionMath::Rect conversion, split out from
// VideoRegionMath.h itself so that header can stay includable by a bare
// standalone test binary with zero openFrameworks dependency (see
// VideoRegionMath.h's header comment and test/videoregion_math_tests.cpp).
// Include this header (not VideoRegionMath.h directly) from any production
// .cpp that already depends on ofRectangle — Fragment.cpp, BlobTracker.*,
// VideoRegionController.*.
namespace VideoRegionMath {

	inline Rect toRect(const ofRectangle & r) { return Rect { r.x, r.y, r.width, r.height }; }
	inline ofRectangle toOf(const Rect & r) { return ofRectangle(r.x, r.y, r.width, r.height); }

}
