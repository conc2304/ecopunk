#pragma once
#include "../../../shared/src/Settings.h"

// Canvas
constexpr int CANVAS_W = 1280;
constexpr int CANVAS_H = 720;
constexpr int TARGET_FPS = 24;

// Grid
constexpr int GRID_COLS = 6;
constexpr int GRID_ROWS = 8;
constexpr int DIVIDER_COL = 3; // orange divider at col 3->4 boundary (0-indexed)

// Cycle timing (seconds)
constexpr float CYCLE_DURATION_MIN = 90.0f;
constexpr float CYCLE_DURATION_MAX = 180.0f;
constexpr float PLACEMENT_INTERVAL_MIN = 1.0f;
constexpr float PLACEMENT_INTERVAL_MAX = 4.0f;
constexpr float PLACEMENT_INTERVAL_DENSE = 6.0f;
constexpr float DISSOLVE_FADE_MIN = 8.0f;
constexpr float DISSOLVE_FADE_MAX = 12.0f;
constexpr float DIVIDER_DRAW_DURATION = 3.0f;
constexpr float GRID_FADEIN_DURATION = 2.0f;

// Composition cycle stage durations (seconds). Total cycle length is
// randomized (CYCLE_DURATION_MIN..MAX); PLACEMENT absorbs the remainder
// after BLANK/DENSITY/DISSOLVE are subtracted.
constexpr float BLANK_DURATION = 8.0f;
constexpr float DENSITY_DURATION = 30.0f;
constexpr float DISSOLVE_DURATION = 30.0f;
constexpr float RESET_HOLD_DURATION = 2.0f; // black hold between cycles

// Fragments
constexpr int MAX_FRAGMENTS = 18;
constexpr int PLACEMENT_MAX_ATTEMPTS = 50; // guard against infinite loop when canvas full
constexpr float DESATURATE_MAX = 0.40f;
constexpr float DESATURATE_RAMP_DURATION = 30.0f; // seconds of DRIFTING to reach DESATURATE_MAX; defaulted to DENSITY_DURATION

// Placement scoring weights (BEComposition::attemptPlacement) — tunable,
// the doc names the four scoring factors but not their relative weights.
constexpr float PLACEMENT_SCORE_W_CENTER = 1.0f; // off-center distance, higher = more off-center
constexpr float PLACEMENT_SCORE_W_PROXIMITY = 1.0f; // "near but not touching" nearest-fragment distance
constexpr float PLACEMENT_SCORE_W_ZONE = 0.5f; // bonus for placing in the underrepresented zone
constexpr float PLACEMENT_SCORE_JITTER = 0.2f; // +/- random noise to avoid rigid patterns

// Drift — active during the DENSITY composition-cycle stage
constexpr float DRIFT_AMP_X = 6.0f; // pixels
constexpr float DRIFT_AMP_Y = 4.0f;
constexpr float DRIFT_FREQ_X = 0.25f; // radians/second (~25s period)
constexpr float DRIFT_FREQ_Y = 0.18f; // radians/second (~35s period)

// Arrival animations
constexpr float SCAN_REVEAL_DURATION = 0.8f;
constexpr float BORDER_DRAW_DURATION = 0.3f; // clockwise perimeter stroke after RECT/SQUARE reveal
constexpr float IRIS_OPEN_DURATION = 1.0f;
constexpr float SLIDE_IN_DURATION = 0.6f;
constexpr float AFTERIMAGE_FADE_DURATION = 2.0f; // SLIVER's afterimage trail at its start position

// Measurement lines
constexpr float MLINE_DRAW_SPEED = 400.0f; // px/second
constexpr float MLINE_FADE_OPACITY = 0.20f;
constexpr float MLINE_FADE_DELAY = 4.0f; // seconds before fade to 20%

// Typography (§02). VeraMono.ttf isn't in this repo; using LiberationMono
// (open-license, already bundled elsewhere in this OF install) instead.
// constexpr char  CODE_FONT_PATH[]          = "fonts/LiberationMono-Regular.ttf";
constexpr char CODE_FONT_PATH[] = "fonts/ArgakaFashionRegular.ttf";
constexpr int SIZE_LABEL = 14;
constexpr int SIZE_CODE = 16;
constexpr int SIZE_ANNOTATION = 9;

// Code text overlay — spawns on its own timer, independent of placement
// events ("appear autonomously between placements").
constexpr float CODE_TEXT_INTERVAL_MIN = 3.0f;
constexpr float CODE_TEXT_INTERVAL_MAX = 8.0f;
constexpr float CODE_TEXT_OPACITY_MIN = 0.50f;
constexpr float CODE_TEXT_OPACITY_MAX = 0.70f;

// Circle fragment (Phase 4 — §06)
constexpr float CIRCLE_DIAMETER_MIN_CELLS = 3.5f;
constexpr float CIRCLE_DIAMETER_MAX_CELLS = 4.5f;
constexpr float CIRCLE_REGION_CENTER_X = 320.0f; // px, left-of-center of zone A
constexpr float CIRCLE_REGION_CENTER_Y = 360.0f; // px, vertical midpoint
constexpr float CIRCLE_REGION_SIZE = 200.0f; // px, side of the randomisation square
constexpr float CIRCLE_TRIGGER_MIN_FRACTION = 0.50f; // fire at >=50% of MAX_FRAGMENTS placed
constexpr float CIRCLE_TRIGGER_MAX_FRACTION = 0.65f;
constexpr int CIRCLE_PLACEMENT_MAX_ATTEMPTS = 20;
constexpr float IRIS_OPEN_LEAD_FRACTION = 0.10f; // how far ahead the outline leads the fill
constexpr float GHOST_RING_SCALE = 1.15f; // ghost ring radius as a multiple of targetRadius

// Media path
#ifdef PLATFORM_PI
constexpr char MEDIA_PATH[] = "/home/pi/blueprint/media/";
#else
constexpr char MEDIA_PATH[] = "media/";
#endif
