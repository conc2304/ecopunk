#pragma once
#include "../../../shared/src/Settings.h"

// -----------------------------------------------------------------------------
// BE Settings Presets
// -----------------------------------------------------------------------------
//
// To switch presets in code, change BE_SETTINGS_PRESET below.
//
// All public setting variable names are preserved. Existing code can keep using:
//   CYCLE_DURATION_MIN
//   PLACEMENT_INTERVAL_MIN
//   DENSITY_DURATION
//   SCAN_REVEAL_DURATION
//   etc.
//
// Those names now map to the values of the selected preset.
//
// Example:
//   constexpr BEPreset BE_SETTINGS_PRESET = BEPreset::Default;
//   constexpr BEPreset BE_SETTINGS_PRESET = BEPreset::SlowCinematic;
//
// -----------------------------------------------------------------------------

enum class BEPreset {
    Default,
    SlowCinematic
};

// Toggle this value to switch the whole composition timing preset.
constexpr BEPreset BE_SETTINGS_PRESET = BEPreset::SlowCinematic;

struct BEPresetValues {
    // Cycle timing
    float CYCLE_DURATION_MIN;
    float CYCLE_DURATION_MAX;
    float PLACEMENT_INTERVAL_MIN;
    float PLACEMENT_INTERVAL_MAX;
    float PLACEMENT_INTERVAL_DENSE;
    float DISSOLVE_FADE_MIN;
    float DISSOLVE_FADE_MAX;
    float DIVIDER_DRAW_DURATION;
    float GRID_FADEIN_DURATION;

    // Composition cycle stage durations
    float BLANK_DURATION;
    float DENSITY_DURATION;
    float DISSOLVE_DURATION;
    float RESET_HOLD_DURATION;

    // Fragments
    int MAX_FRAGMENTS;
    int PLACEMENT_MAX_ATTEMPTS;
    float DESATURATE_MAX;
    float DESATURATE_RAMP_DURATION;

    // Placement scoring weights
    float PLACEMENT_SCORE_W_CENTER;
    float PLACEMENT_SCORE_W_PROXIMITY;
    float PLACEMENT_SCORE_W_ZONE;
    float PLACEMENT_SCORE_JITTER;

    // Drift
    float DRIFT_AMP_X;
    float DRIFT_AMP_Y;
    float DRIFT_FREQ_X;
    float DRIFT_FREQ_Y;

    // Arrival animations
    float SCAN_REVEAL_DURATION;
    float BORDER_DRAW_DURATION;
    float IRIS_OPEN_DURATION;
    float SLIDE_IN_DURATION;
    float AFTERIMAGE_FADE_DURATION;

    // Measurement lines
    float MLINE_DRAW_SPEED;
    float MLINE_FADE_OPACITY;
    float MLINE_FADE_DELAY;

    // Typography
    int SIZE_LABEL;
    int SIZE_CODE;
    int SIZE_ANNOTATION;

    // Code text overlay
    float CODE_TEXT_INTERVAL_MIN;
    float CODE_TEXT_INTERVAL_MAX;
    float CODE_TEXT_OPACITY_MIN;
    float CODE_TEXT_OPACITY_MAX;

    // Circle fragment
    float CIRCLE_DIAMETER_MIN_CELLS;
    float CIRCLE_DIAMETER_MAX_CELLS;
    float CIRCLE_REGION_CENTER_X;
    float CIRCLE_REGION_CENTER_Y;
    float CIRCLE_REGION_SIZE;
    float CIRCLE_TRIGGER_MIN_FRACTION;
    float CIRCLE_TRIGGER_MAX_FRACTION;
    int CIRCLE_PLACEMENT_MAX_ATTEMPTS;
    float IRIS_OPEN_LEAD_FRACTION;
    float GHOST_RING_SCALE;
};

constexpr BEPresetValues BE_DEFAULT_PRESET = {
    // Cycle timing
    90.0f,   // CYCLE_DURATION_MIN
    180.0f,  // CYCLE_DURATION_MAX
    1.0f,    // PLACEMENT_INTERVAL_MIN
    4.0f,    // PLACEMENT_INTERVAL_MAX
    6.0f,    // PLACEMENT_INTERVAL_DENSE
    8.0f,    // DISSOLVE_FADE_MIN
    12.0f,   // DISSOLVE_FADE_MAX
    3.0f,    // DIVIDER_DRAW_DURATION
    2.0f,    // GRID_FADEIN_DURATION

    // Composition cycle stage durations
    8.0f,    // BLANK_DURATION
    30.0f,   // DENSITY_DURATION
    30.0f,   // DISSOLVE_DURATION
    2.0f,    // RESET_HOLD_DURATION

    // Fragments
    18,      // MAX_FRAGMENTS
    50,      // PLACEMENT_MAX_ATTEMPTS
    0.40f,   // DESATURATE_MAX
    30.0f,   // DESATURATE_RAMP_DURATION

    // Placement scoring weights
    1.0f,    // PLACEMENT_SCORE_W_CENTER
    1.0f,    // PLACEMENT_SCORE_W_PROXIMITY
    0.5f,    // PLACEMENT_SCORE_W_ZONE
    0.2f,    // PLACEMENT_SCORE_JITTER

    // Drift
    6.0f,    // DRIFT_AMP_X
    4.0f,    // DRIFT_AMP_Y
    0.25f,   // DRIFT_FREQ_X
    0.18f,   // DRIFT_FREQ_Y

    // Arrival animations
    0.8f,    // SCAN_REVEAL_DURATION
    0.3f,    // BORDER_DRAW_DURATION
    1.0f,    // IRIS_OPEN_DURATION
    0.6f,    // SLIDE_IN_DURATION
    2.0f,    // AFTERIMAGE_FADE_DURATION

    // Measurement lines
    400.0f,  // MLINE_DRAW_SPEED
    0.20f,   // MLINE_FADE_OPACITY
    4.0f,    // MLINE_FADE_DELAY

    // Typography
    14,      // SIZE_LABEL
    16,      // SIZE_CODE
    9,       // SIZE_ANNOTATION

    // Code text overlay
    3.0f,    // CODE_TEXT_INTERVAL_MIN
    8.0f,    // CODE_TEXT_INTERVAL_MAX
    0.50f,   // CODE_TEXT_OPACITY_MIN
    0.70f,   // CODE_TEXT_OPACITY_MAX

    // Circle fragment
    3.5f,    // CIRCLE_DIAMETER_MIN_CELLS
    4.5f,    // CIRCLE_DIAMETER_MAX_CELLS
    320.0f,  // CIRCLE_REGION_CENTER_X
    360.0f,  // CIRCLE_REGION_CENTER_Y
    200.0f,  // CIRCLE_REGION_SIZE
    0.50f,   // CIRCLE_TRIGGER_MIN_FRACTION
    0.65f,   // CIRCLE_TRIGGER_MAX_FRACTION
    20,      // CIRCLE_PLACEMENT_MAX_ATTEMPTS
    0.10f,   // IRIS_OPEN_LEAD_FRACTION
    1.15f    // GHOST_RING_SCALE
};

constexpr BEPresetValues BE_SLOW_CINEMATIC_PRESET = {
    // Cycle timing
    180.0f,  // CYCLE_DURATION_MIN
    300.0f,  // CYCLE_DURATION_MAX
    4.0f,    // PLACEMENT_INTERVAL_MIN
    10.0f,   // PLACEMENT_INTERVAL_MAX
    10.0f,   // PLACEMENT_INTERVAL_DENSE
    14.0f,   // DISSOLVE_FADE_MIN
    22.0f,   // DISSOLVE_FADE_MAX
    6.0f,    // DIVIDER_DRAW_DURATION
    4.0f,    // GRID_FADEIN_DURATION

    // Composition cycle stage durations
    8.0f,    // BLANK_DURATION
    60.0f,   // DENSITY_DURATION
    45.0f,   // DISSOLVE_DURATION
    3.0f,    // RESET_HOLD_DURATION

    // Fragments
    18,      // MAX_FRAGMENTS
    50,      // PLACEMENT_MAX_ATTEMPTS
    0.40f,   // DESATURATE_MAX
    60.0f,   // DESATURATE_RAMP_DURATION

    // Placement scoring weights
    1.0f,    // PLACEMENT_SCORE_W_CENTER
    1.0f,    // PLACEMENT_SCORE_W_PROXIMITY
    0.5f,    // PLACEMENT_SCORE_W_ZONE
    0.2f,    // PLACEMENT_SCORE_JITTER

    // Drift
    4.0f,    // DRIFT_AMP_X
    3.0f,    // DRIFT_AMP_Y
    0.10f,   // DRIFT_FREQ_X
    0.08f,   // DRIFT_FREQ_Y

    // Arrival animations
    2.0f,    // SCAN_REVEAL_DURATION
    0.8f,    // BORDER_DRAW_DURATION
    2.5f,    // IRIS_OPEN_DURATION
    1.6f,    // SLIDE_IN_DURATION
    4.0f,    // AFTERIMAGE_FADE_DURATION

    // Measurement lines
    180.0f,  // MLINE_DRAW_SPEED
    0.20f,   // MLINE_FADE_OPACITY
    8.0f,    // MLINE_FADE_DELAY

    // Typography
    14,      // SIZE_LABEL
    16,      // SIZE_CODE
    9,       // SIZE_ANNOTATION

    // Code text overlay
    8.0f,    // CODE_TEXT_INTERVAL_MIN
    16.0f,   // CODE_TEXT_INTERVAL_MAX
    0.50f,   // CODE_TEXT_OPACITY_MIN
    0.70f,   // CODE_TEXT_OPACITY_MAX

    // Circle fragment
    3.5f,    // CIRCLE_DIAMETER_MIN_CELLS
    4.5f,    // CIRCLE_DIAMETER_MAX_CELLS
    320.0f,  // CIRCLE_REGION_CENTER_X
    360.0f,  // CIRCLE_REGION_CENTER_Y
    200.0f,  // CIRCLE_REGION_SIZE
    0.50f,   // CIRCLE_TRIGGER_MIN_FRACTION
    0.65f,   // CIRCLE_TRIGGER_MAX_FRACTION
    20,      // CIRCLE_PLACEMENT_MAX_ATTEMPTS
    0.10f,   // IRIS_OPEN_LEAD_FRACTION
    1.15f    // GHOST_RING_SCALE
};

constexpr BEPresetValues getBEPresetValues(BEPreset preset) {
    return preset == BEPreset::SlowCinematic
        ? BE_SLOW_CINEMATIC_PRESET
        : BE_DEFAULT_PRESET;
}

constexpr BEPresetValues BE_VALUES = getBEPresetValues(BE_SETTINGS_PRESET);

// Canvas
constexpr int CANVAS_W = 1280;
constexpr int CANVAS_H = 720;
constexpr int TARGET_FPS = 24;

// Grid
constexpr int GRID_COLS = 6;
constexpr int GRID_ROWS = 8;
constexpr int DIVIDER_COL = 3; // orange divider at col 3->4 boundary (0-indexed)

// Cycle timing (seconds)
constexpr float CYCLE_DURATION_MIN = BE_VALUES.CYCLE_DURATION_MIN;
constexpr float CYCLE_DURATION_MAX = BE_VALUES.CYCLE_DURATION_MAX;
constexpr float PLACEMENT_INTERVAL_MIN = BE_VALUES.PLACEMENT_INTERVAL_MIN;
constexpr float PLACEMENT_INTERVAL_MAX = BE_VALUES.PLACEMENT_INTERVAL_MAX;
constexpr float PLACEMENT_INTERVAL_DENSE = BE_VALUES.PLACEMENT_INTERVAL_DENSE;
constexpr float DISSOLVE_FADE_MIN = BE_VALUES.DISSOLVE_FADE_MIN;
constexpr float DISSOLVE_FADE_MAX = BE_VALUES.DISSOLVE_FADE_MAX;
constexpr float DIVIDER_DRAW_DURATION = BE_VALUES.DIVIDER_DRAW_DURATION;
constexpr float GRID_FADEIN_DURATION = BE_VALUES.GRID_FADEIN_DURATION;

// Composition cycle stage durations (seconds). Total cycle length is
// randomized (CYCLE_DURATION_MIN..MAX); PLACEMENT absorbs the remainder
// after BLANK/DENSITY/DISSOLVE are subtracted.
constexpr float BLANK_DURATION = BE_VALUES.BLANK_DURATION;
constexpr float DENSITY_DURATION = BE_VALUES.DENSITY_DURATION;
constexpr float DISSOLVE_DURATION = BE_VALUES.DISSOLVE_DURATION;
constexpr float RESET_HOLD_DURATION = BE_VALUES.RESET_HOLD_DURATION; // black hold between cycles

// Fragments
constexpr int MAX_FRAGMENTS = BE_VALUES.MAX_FRAGMENTS;
constexpr int PLACEMENT_MAX_ATTEMPTS = BE_VALUES.PLACEMENT_MAX_ATTEMPTS; // guard against infinite loop when canvas full
constexpr float DESATURATE_MAX = BE_VALUES.DESATURATE_MAX;
constexpr float DESATURATE_RAMP_DURATION = BE_VALUES.DESATURATE_RAMP_DURATION; // seconds of DRIFTING to reach DESATURATE_MAX

// Placement scoring weights (BEComposition::attemptPlacement) — tunable,
// the doc names the four scoring factors but not their relative weights.
constexpr float PLACEMENT_SCORE_W_CENTER = BE_VALUES.PLACEMENT_SCORE_W_CENTER; // off-center distance, higher = more off-center
constexpr float PLACEMENT_SCORE_W_PROXIMITY = BE_VALUES.PLACEMENT_SCORE_W_PROXIMITY; // "near but not touching" nearest-fragment distance
constexpr float PLACEMENT_SCORE_W_ZONE = BE_VALUES.PLACEMENT_SCORE_W_ZONE; // bonus for placing in the underrepresented zone
constexpr float PLACEMENT_SCORE_JITTER = BE_VALUES.PLACEMENT_SCORE_JITTER; // +/- random noise to avoid rigid patterns

// Drift — active during the DENSITY composition-cycle stage
constexpr float DRIFT_AMP_X = BE_VALUES.DRIFT_AMP_X; // pixels
constexpr float DRIFT_AMP_Y = BE_VALUES.DRIFT_AMP_Y;
constexpr float DRIFT_FREQ_X = BE_VALUES.DRIFT_FREQ_X; // radians/second
constexpr float DRIFT_FREQ_Y = BE_VALUES.DRIFT_FREQ_Y; // radians/second

// Arrival animations
constexpr float SCAN_REVEAL_DURATION = BE_VALUES.SCAN_REVEAL_DURATION;
constexpr float BORDER_DRAW_DURATION = BE_VALUES.BORDER_DRAW_DURATION; // clockwise perimeter stroke after RECT/SQUARE reveal
constexpr float IRIS_OPEN_DURATION = BE_VALUES.IRIS_OPEN_DURATION;
constexpr float SLIDE_IN_DURATION = BE_VALUES.SLIDE_IN_DURATION;
constexpr float AFTERIMAGE_FADE_DURATION = BE_VALUES.AFTERIMAGE_FADE_DURATION; // SLIVER's afterimage trail at its start position

// Measurement lines
constexpr float MLINE_DRAW_SPEED = BE_VALUES.MLINE_DRAW_SPEED; // px/second
constexpr float MLINE_FADE_OPACITY = BE_VALUES.MLINE_FADE_OPACITY;
constexpr float MLINE_FADE_DELAY = BE_VALUES.MLINE_FADE_DELAY; // seconds before fade to MLINE_FADE_OPACITY

// Typography (§02). VeraMono.ttf isn't in this repo; using LiberationMono
// (open-license, already bundled elsewhere in this OF install) instead.
// constexpr char  CODE_FONT_PATH[]          = "fonts/LiberationMono-Regular.ttf";
constexpr char CODE_FONT_PATH[] = "fonts/ArgakaFashionRegular.ttf";
constexpr int SIZE_LABEL = BE_VALUES.SIZE_LABEL;
constexpr int SIZE_CODE = BE_VALUES.SIZE_CODE;
constexpr int SIZE_ANNOTATION = BE_VALUES.SIZE_ANNOTATION;

// Code text overlay — spawns on its own timer, independent of placement
// events ("appear autonomously between placements").
constexpr float CODE_TEXT_INTERVAL_MIN = BE_VALUES.CODE_TEXT_INTERVAL_MIN;
constexpr float CODE_TEXT_INTERVAL_MAX = BE_VALUES.CODE_TEXT_INTERVAL_MAX;
constexpr float CODE_TEXT_OPACITY_MIN = BE_VALUES.CODE_TEXT_OPACITY_MIN;
constexpr float CODE_TEXT_OPACITY_MAX = BE_VALUES.CODE_TEXT_OPACITY_MAX;

// Circle fragment (Phase 4 — §06)
constexpr float CIRCLE_DIAMETER_MIN_CELLS = BE_VALUES.CIRCLE_DIAMETER_MIN_CELLS;
constexpr float CIRCLE_DIAMETER_MAX_CELLS = BE_VALUES.CIRCLE_DIAMETER_MAX_CELLS;
constexpr float CIRCLE_REGION_CENTER_X = BE_VALUES.CIRCLE_REGION_CENTER_X; // px, left-of-center of zone A
constexpr float CIRCLE_REGION_CENTER_Y = BE_VALUES.CIRCLE_REGION_CENTER_Y; // px, vertical midpoint
constexpr float CIRCLE_REGION_SIZE = BE_VALUES.CIRCLE_REGION_SIZE; // px, side of the randomisation square
constexpr float CIRCLE_TRIGGER_MIN_FRACTION = BE_VALUES.CIRCLE_TRIGGER_MIN_FRACTION; // fire at >=50% of MAX_FRAGMENTS placed
constexpr float CIRCLE_TRIGGER_MAX_FRACTION = BE_VALUES.CIRCLE_TRIGGER_MAX_FRACTION;
constexpr int CIRCLE_PLACEMENT_MAX_ATTEMPTS = BE_VALUES.CIRCLE_PLACEMENT_MAX_ATTEMPTS;
constexpr float IRIS_OPEN_LEAD_FRACTION = BE_VALUES.IRIS_OPEN_LEAD_FRACTION; // how far ahead the outline leads the fill
constexpr float GHOST_RING_SCALE = BE_VALUES.GHOST_RING_SCALE; // ghost ring radius as a multiple of targetRadius

// Media path
#ifdef PLATFORM_PI
constexpr char MEDIA_PATH[] = "/home/pi/blueprint/media/";
#else
constexpr char MEDIA_PATH[] = "media/";
#endif
