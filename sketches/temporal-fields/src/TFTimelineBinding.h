#pragma once

#include <functional>

// Typed adapter between TFPresetTimeline's engine (which knows nothing about
// ofParameter/openFrameworks) and whatever actually owns a parameter's live
// value -- in production, a real ofParameter<float/int/bool> in
// TFParameterPanel (see buildTimelineBindings() in TFParameterPanel.cpp);
// in tests, a plain in-memory double. Every value crossing this boundary is
// a double regardless of the underlying type -- Kind is what tells the
// engine which interpolation policy applies to it:
//
//   Float / Int    -> continuous lerp (Int rounds at write time)
//   Bool / EnumInt -> switch at 50% eased transition progress
//   Unsupported    -> left untouched by the timeline entirely (no
//                     non-numeric/string ofParameter exists anywhere in this
//                     panel's animatable groups today, so this is a reserved
//                     extension point, not a live code path)
struct TFTimelineBinding {
	enum class Kind { Float, Int, Bool, EnumInt, Unsupported };

	Kind kind = Kind::Unsupported;
	std::function<double()> get;
	std::function<void(double)> set;

	bool hasRange = false;
	double minValue = 0.0;
	double maxValue = 0.0;
};
