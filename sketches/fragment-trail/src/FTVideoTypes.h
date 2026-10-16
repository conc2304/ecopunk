#pragma once

// Which TimeOffsetVideoBuffer and ShaderLibrary Fragment Trail's classes bind
// to. Both are global classes used only by reference here.
//
// Standalone sketch: the local forks in this directory (the buffer scans and
// plays its own media folder; the shader library loads bin/data/shaders/).
//
// ExperienceRuntime (FT_PRODUCTION_RUNTIME): the shared canonical
// shared/src/TimeOffsetVideoBuffer, driven by TimeOffsetPlaybackAdapter from
// VideoPlaybackService's selection (DEC-013/DEC-014: no local scan or
// selection), and the shared ShaderLibrary, which loads byte-identical copies
// of the nine effects Fragment uses from the canonical shaders/effects/ set.
// The forks' headers are never seen there: both would redefine a global
// class the runtime already links.
#if defined(FT_PRODUCTION_RUNTIME)
#include "../../../shared/src/TimeOffsetVideoBuffer.h"
#include "../../../shared/src/ShaderLibrary.h"
#else
#include "TimeOffsetVideoBuffer.h"
#include "ShaderLibrary.h"
#endif
