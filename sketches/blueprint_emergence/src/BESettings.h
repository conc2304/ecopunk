#pragma once
#include "../../../shared/src/Settings.h"

// Canvas
constexpr int   CANVAS_W                  = 1280;
constexpr int   CANVAS_H                  = 720;
constexpr int   TARGET_FPS                = 24;

// Grid
constexpr int   GRID_COLS                 = 6;
constexpr int   GRID_ROWS                 = 8;
constexpr int   DIVIDER_COL               = 3;   // orange divider at col 3->4 boundary (0-indexed)

// Cycle timing (seconds)
constexpr float CYCLE_DURATION_MIN        = 90.0f;
constexpr float CYCLE_DURATION_MAX        = 180.0f;
constexpr float PLACEMENT_INTERVAL_MIN    = 4.0f;
constexpr float PLACEMENT_INTERVAL_MAX    = 12.0f;
constexpr float PLACEMENT_INTERVAL_DENSE  = 17.5f; // midpoint of 15-20s density range
constexpr float DISSOLVE_FADE_MIN         = 8.0f;
constexpr float DISSOLVE_FADE_MAX         = 12.0f;
constexpr float DIVIDER_DRAW_DURATION     = 3.0f;
constexpr float GRID_FADEIN_DURATION      = 2.0f;

// Fragments
constexpr int   MAX_FRAGMENTS             = 12;
constexpr int   PLACEMENT_MAX_ATTEMPTS    = 50;  // guard against infinite loop when canvas full
constexpr float DESATURATE_MAX            = 0.40f;

// Drift (Phase 3)
constexpr float DRIFT_AMP_X               = 2.0f;   // pixels
constexpr float DRIFT_AMP_Y               = 1.5f;
constexpr float DRIFT_FREQ_X              = 0.08f;  // radians/second
constexpr float DRIFT_FREQ_Y              = 0.06f;

// Arrival animations
constexpr float SCAN_REVEAL_DURATION      = 0.8f;
constexpr float IRIS_OPEN_DURATION        = 1.0f;
constexpr float SLIDE_IN_DURATION         = 0.6f;

// Measurement lines
constexpr float MLINE_DRAW_SPEED          = 400.0f; // px/second
constexpr float MLINE_FADE_OPACITY        = 0.20f;
constexpr float MLINE_FADE_DELAY          = 4.0f;   // seconds before fade to 20%

// Media path
#ifdef PLATFORM_PI
	constexpr char MEDIA_PATH[] = "/home/pi/blueprint/media/";
#else
	constexpr char MEDIA_PATH[] = "media/";
#endif
