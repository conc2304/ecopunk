#pragma once

#include "ofColor.h"

// Color tokens shared by every sketch. Sketch-level *Settings.h files may
// override these for sketch-specific variants.
static const ofColor GROUND_DARK  = ofColor::fromHex(0x0D0D0D); // primary canvas bg
static const ofColor GROUND_LIGHT = ofColor::fromHex(0xE8E6E0); // warm off-white, zone variant
static const ofColor RULE_WHITE   = ofColor(255, 255, 255, 77); // 30% opacity grid lines
static const ofColor RULE_ORANGE  = ofColor::fromHex(0xC87941); // vertical divider accent
static const ofColor TEXT_CODE    = ofColor(255, 255, 255, 153); // 60% — code fragment overlays
static const ofColor TEXT_DIM     = ofColor::fromHex(0x888888); // secondary labels, coords
