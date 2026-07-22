#pragma once

// Canonical HUD widget library aggregator. #include this single header to
// pull in every shared contract and all eleven widgets. See README.md in
// this directory for integration instructions — do not copy this library
// into a sketch; point your build at this directory instead.

#include "shared/HudTypes.h"
#include "shared/HudUtils.h"
#include "shared/HudWidget.h"
#include "shared/HudFrameRenderer.h"

#include "ScannerWidget/ScannerWidget.h"
#include "NodeNetworkWidget/NodeNetworkWidget.h"
#include "FlowFieldWidget/FlowFieldWidget.h"
#include "ContourWidget/ContourWidget.h"
#include "GaugeWidget/GaugeWidget.h"
#include "DataCardWidget/DataCardWidget.h"
#include "HexGridWidget/HexGridWidget.h"
#include "ReticleWidget/ReticleWidget.h"
#include "GlitchTearWidget/GlitchTearWidget.h"
#include "StatusLightWidget/StatusLightWidget.h"
#include "LogScrollWidget/LogScrollWidget.h"
