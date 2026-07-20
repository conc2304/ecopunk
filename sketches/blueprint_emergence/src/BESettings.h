#pragma once
#include "../../../shared/src/Settings.h"

// -----------------------------------------------------------------------------
// BESettings preset toggle
// -----------------------------------------------------------------------------
//
// Existing code should continue to use the same setting names, for example:
//
//   CYCLE_DURATION_MIN
//   PLACEMENT_INTERVAL_MIN
//   DENSITY_DURATION
//   SCAN_REVEAL_DURATION
//
// Those names now map to the currently selected preset.
//
// To switch presets, change only this line:
//
//   constexpr BEPreset BE_SETTINGS_PRESET = BEPreset::Default;
//
// or:
//
//   constexpr BEPreset BE_SETTINGS_PRESET = BEPreset::SlowCinematic;
//
// -----------------------------------------------------------------------------

enum class BEPreset {
	Default,
	SlowCinematic
};

// Active preset toggle.
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
	float PLACEMENT_SCORE_W_CENTER;   // off-center bias: lower = more center-friendly
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
	float CIRCLE_REGION_CENTER_X_FRAC;
	float CIRCLE_REGION_CENTER_Y_FRAC;
	float CIRCLE_REGION_SIZE_FRAC;
	float CIRCLE_TRIGGER_MIN_FRACTION;
	float CIRCLE_TRIGGER_MAX_FRACTION;
	int CIRCLE_PLACEMENT_MAX_ATTEMPTS;
	float IRIS_OPEN_LEAD_FRACTION;
	float GHOST_RING_SCALE;
};

constexpr BEPresetValues BE_DEFAULT_PRESET = {
	// Cycle timing
	90.0f, // CYCLE_DURATION_MIN
	180.0f, // CYCLE_DURATION_MAX
	1.0f, // PLACEMENT_INTERVAL_MIN
	4.0f, // PLACEMENT_INTERVAL_MAX
	6.0f, // PLACEMENT_INTERVAL_DENSE
	8.0f, // DISSOLVE_FADE_MIN
	12.0f, // DISSOLVE_FADE_MAX
	3.0f, // DIVIDER_DRAW_DURATION
	2.0f, // GRID_FADEIN_DURATION

	// Composition cycle stage durations
	8.0f, // BLANK_DURATION
	30.0f, // DENSITY_DURATION
	30.0f, // DISSOLVE_DURATION
	2.0f, // RESET_HOLD_DURATION

	// Fragments
	18, // MAX_FRAGMENTS
	50, // PLACEMENT_MAX_ATTEMPTS
	0.40f, // DESATURATE_MAX
	30.0f, // DESATURATE_RAMP_DURATION

	// Placement scoring weights
	0.3f, // PLACEMENT_SCORE_W_CENTER
	1.0f, // PLACEMENT_SCORE_W_PROXIMITY
	0.5f, // PLACEMENT_SCORE_W_ZONE
	0.2f, // PLACEMENT_SCORE_JITTER

	// Drift
	6.0f, // DRIFT_AMP_X
	4.0f, // DRIFT_AMP_Y
	0.25f, // DRIFT_FREQ_X
	0.18f, // DRIFT_FREQ_Y

	// Arrival animations
	0.8f, // SCAN_REVEAL_DURATION
	0.3f, // BORDER_DRAW_DURATION
	1.0f, // IRIS_OPEN_DURATION
	0.6f, // SLIDE_IN_DURATION
	2.0f, // AFTERIMAGE_FADE_DURATION

	// Measurement lines
	400.0f, // MLINE_DRAW_SPEED
	0.20f, // MLINE_FADE_OPACITY
	4.0f, // MLINE_FADE_DELAY

	// Typography
	14, // SIZE_LABEL
	16, // SIZE_CODE
	9, // SIZE_ANNOTATION

	// Code text overlay
	3.0f, // CODE_TEXT_INTERVAL_MIN
	8.0f, // CODE_TEXT_INTERVAL_MAX
	0.50f, // CODE_TEXT_OPACITY_MIN
	0.70f, // CODE_TEXT_OPACITY_MAX

	// Circle fragment
	3.5f, // CIRCLE_DIAMETER_MIN_CELLS
	4.5f, // CIRCLE_DIAMETER_MAX_CELLS
	0.25f, // CIRCLE_REGION_CENTER_X_FRAC (320/1280)
	0.50f, // CIRCLE_REGION_CENTER_Y_FRAC (360/720)
	0.156f, // CIRCLE_REGION_SIZE_FRAC (200/1280)
	0.50f, // CIRCLE_TRIGGER_MIN_FRACTION
	0.65f, // CIRCLE_TRIGGER_MAX_FRACTION
	20, // CIRCLE_PLACEMENT_MAX_ATTEMPTS
	0.10f, // IRIS_OPEN_LEAD_FRACTION
	1.15f // GHOST_RING_SCALE
};

constexpr BEPresetValues BE_SLOW_CINEMATIC_PRESET = {
	// Cycle timing
	180.0f, // CYCLE_DURATION_MIN
	300.0f, // CYCLE_DURATION_MAX
	4.0f, // PLACEMENT_INTERVAL_MIN
	10.0f, // PLACEMENT_INTERVAL_MAX
	10.0f, // PLACEMENT_INTERVAL_DENSE
	14.0f, // DISSOLVE_FADE_MIN
	22.0f, // DISSOLVE_FADE_MAX
	6.0f, // DIVIDER_DRAW_DURATION
	4.0f, // GRID_FADEIN_DURATION

	// Composition cycle stage durations
	8.0f, // BLANK_DURATION
	60.0f, // DENSITY_DURATION
	45.0f, // DISSOLVE_DURATION
	3.0f, // RESET_HOLD_DURATION

	// Fragments
	18, // MAX_FRAGMENTS
	50, // PLACEMENT_MAX_ATTEMPTS
	0.40f, // DESATURATE_MAX
	60.0f, // DESATURATE_RAMP_DURATION

	// Placement scoring weights
	0.3f, // PLACEMENT_SCORE_W_CENTER
	1.0f, // PLACEMENT_SCORE_W_PROXIMITY
	0.5f, // PLACEMENT_SCORE_W_ZONE
	0.2f, // PLACEMENT_SCORE_JITTER

	// Drift
	4.0f, // DRIFT_AMP_X
	3.0f, // DRIFT_AMP_Y
	0.10f, // DRIFT_FREQ_X
	0.08f, // DRIFT_FREQ_Y

	// Arrival animations
	2.0f, // SCAN_REVEAL_DURATION
	0.8f, // BORDER_DRAW_DURATION
	2.5f, // IRIS_OPEN_DURATION
	1.6f, // SLIDE_IN_DURATION
	4.0f, // AFTERIMAGE_FADE_DURATION

	// Measurement lines
	180.0f, // MLINE_DRAW_SPEED
	0.20f, // MLINE_FADE_OPACITY
	8.0f, // MLINE_FADE_DELAY

	// Typography
	14, // SIZE_LABEL
	16, // SIZE_CODE
	9, // SIZE_ANNOTATION

	// Code text overlay
	8.0f, // CODE_TEXT_INTERVAL_MIN
	16.0f, // CODE_TEXT_INTERVAL_MAX
	0.50f, // CODE_TEXT_OPACITY_MIN
	0.70f, // CODE_TEXT_OPACITY_MAX

	// Circle fragment
	3.5f, // CIRCLE_DIAMETER_MIN_CELLS
	4.5f, // CIRCLE_DIAMETER_MAX_CELLS
	0.25f, // CIRCLE_REGION_CENTER_X_FRAC (320/1280)
	0.50f, // CIRCLE_REGION_CENTER_Y_FRAC (360/720)
	0.156f, // CIRCLE_REGION_SIZE_FRAC (200/1280)
	0.50f, // CIRCLE_TRIGGER_MIN_FRACTION
	0.65f, // CIRCLE_TRIGGER_MAX_FRACTION
	20, // CIRCLE_PLACEMENT_MAX_ATTEMPTS
	0.10f, // IRIS_OPEN_LEAD_FRACTION
	1.15f // GHOST_RING_SCALE
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
// GridSystem itself no longer uses a fixed column/row grid (see the "Grid
// System Handoff" doc — it now generates dynamic, content-derived lines).
// GRID_COLS/GRID_ROWS survive here only as GridState's (the vitality-system
// activity tracker) fixed sampling resolution — an unrelated, coarser grid.
constexpr int GRID_COLS = 6;
constexpr int GRID_ROWS = 8;
constexpr float DIVIDER_X_FRACTION = 0.40f; // orange divider position, fraction of canvas width

// Cycle timing (seconds)
constexpr float CYCLE_DURATION_MIN = BE_VALUES.CYCLE_DURATION_MIN;
constexpr float CYCLE_DURATION_MAX = BE_VALUES.CYCLE_DURATION_MAX;
// PLACEMENT_INTERVAL_* drove CompositionBase's single global placement
// timer. BEComposition now overrides usesAutomaticPlacementTimer() to false
// and drives 4 independently-paced slots itself (see SLOT_* below), so
// these are no longer consumed — kept only because the BEPresetValues
// struct still carries them.
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
// Quadrant-style slot model (see plan: "lets-update-the-blueprint-emergence"):
// up to 4 simultaneous fragments, each its own independently-paced slot,
// rather than the old organic growth up to BE_VALUES.MAX_FRAGMENTS (18).
constexpr int MAX_FRAGMENTS = 4;
constexpr int PLACEMENT_MAX_ATTEMPTS = BE_VALUES.PLACEMENT_MAX_ATTEMPTS; // guard against infinite loop when canvas full
constexpr float DESATURATE_MAX = BE_VALUES.DESATURATE_MAX;
constexpr float DESATURATE_RAMP_DURATION = BE_VALUES.DESATURATE_RAMP_DURATION; // seconds of DRIFTING to reach DESATURATE_MAX

// Slot lifecycle (quadrant-style): each of the up to MAX_FRAGMENTS slots
// independently cycles READY -> arrive -> HOLD -> dissolve -> SILENCE -> READY.
constexpr float SLOT_HOLD_MIN = 22.0f; // seconds a fragment is guaranteed to stay visible before it's eligible to dissolve
constexpr float SLOT_HOLD_MAX = 45.0f;
constexpr float SLOT_SILENCE_MIN = 10.0f; // seconds a slot waits, empty, before respawning
constexpr float SLOT_SILENCE_MAX = 22.0f;
constexpr float SLOT_DISSOLVE_FADE_MIN = 3.0f; // per-slot turnover fade, shorter than the cycle-ending DISSOLVE_FADE_*
constexpr float SLOT_DISSOLVE_FADE_MAX = 7.0f;
// Per-slot stagger applied once at cycle start so slots don't all attempt
// their first spawn simultaneously when PLACEMENT phase opens.
constexpr float SLOT_INITIAL_STAGGER_MIN = 10.0f;
constexpr float SLOT_INITIAL_STAGGER_MAX = 20.0f;

// HUD widget — occasionally fills unoccupied grid space instead of a video
// fragment. Independent of the 4 video slots; reserves its own occupancy
// rect via GridSystem so fragment placement doesn't overlap it.
constexpr float HUD_WIDGET_PROBABILITY = 0.30f;
constexpr float HUD_HOLD_MIN = 12.0f;
constexpr float HUD_HOLD_MAX = 24.0f;
// Speed multiplier applied to all autonomous (non-data-driven) HUD widgets —
// ContourWidget, HexGridWidget, FlowFieldWidget, NodeNetworkWidget, ReticleWidget.
// 1.0 = default animation rate; 0.333 = one-third speed (3× slower).
constexpr float HUD_VISUAL_WIDGET_SPEED = 1.0f / 3.0f;

// Chance a freshly-spawned video-fragment slot shows MotionExtraction's
// accumulated motion-trail texture instead of live video.
constexpr float MOTION_CONTENT_PROBABILITY = 0.20f;

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
constexpr float CIRCLE_REGION_CENTER_X_FRAC = BE_VALUES.CIRCLE_REGION_CENTER_X_FRAC; // fraction of canvas width, left-of-center of zone A
constexpr float CIRCLE_REGION_CENTER_Y_FRAC = BE_VALUES.CIRCLE_REGION_CENTER_Y_FRAC; // fraction of canvas height, vertical midpoint
constexpr float CIRCLE_REGION_SIZE_FRAC = BE_VALUES.CIRCLE_REGION_SIZE_FRAC; // fraction of canvas width, side of the randomisation square
// CIRCLE_TRIGGER_MIN/MAX_FRACTION drove the old "Nth organic placement"
// trigger, which doesn't map onto independently-respawning slots. The hero
// circle is now a per-spawn-attempt probability roll instead (still capped
// at most once per cycle) — see CIRCLE_SPAWN_PROBABILITY below.
constexpr float CIRCLE_TRIGGER_MIN_FRACTION = BE_VALUES.CIRCLE_TRIGGER_MIN_FRACTION;
constexpr float CIRCLE_TRIGGER_MAX_FRACTION = BE_VALUES.CIRCLE_TRIGGER_MAX_FRACTION;
constexpr float CIRCLE_SPAWN_PROBABILITY = 0.15f; // chance any given slot respawn becomes the hero circle instead
constexpr int CIRCLE_PLACEMENT_MAX_ATTEMPTS = BE_VALUES.CIRCLE_PLACEMENT_MAX_ATTEMPTS;
constexpr float IRIS_OPEN_LEAD_FRACTION = BE_VALUES.IRIS_OPEN_LEAD_FRACTION; // how far ahead the outline leads the fill
constexpr float GHOST_RING_SCALE = BE_VALUES.GHOST_RING_SCALE; // ghost ring radius as a multiple of targetRadius

// Media path
#ifdef PLATFORM_PI
constexpr char MEDIA_PATH[] = "/home/pi/blueprint/media/";
#else
constexpr char MEDIA_PATH[] = "media/";
#endif

// -----------------------------------------------------------------------------
// Vitality systems — LFOBank / TriggerBus / GridState / ErosionFBO
// Ported from sketches/quadrant-crosshair. See docs handoff for the design
// rationale; these constants are the BE-specific tuning for that design.
// -----------------------------------------------------------------------------

// ── LFOBank ──────────────────────────────────────────────────────────────────
constexpr float LFO_GRID_OPACITY_HZ = 0.008f;
constexpr float LFO_DIVIDER_BRIGHTNESS_HZ = 0.003f;
constexpr float LFO_PLACEMENT_BIAS_HZ = 0.012f;
constexpr float LFO_FRAG_DESAT_A_HZ = 0.019f;
constexpr float LFO_FRAG_DESAT_B_HZ = 0.011f;
constexpr float LFO_CODE_TEXT_WEIGHT_HZ = 0.007f;
constexpr float LFO_GRID_OPACITY_MIN = 0.20f;
constexpr float LFO_GRID_OPACITY_MAX = 0.45f;
constexpr float LFO_DIVIDER_MIN = 0.70f;
constexpr float LFO_DIVIDER_MAX = 1.00f;
constexpr float LFO_PLACEMENT_BIAS_RANGE = 3.0f; // +/- seconds
constexpr float LFO_DESAT_NUDGE_MAX = 0.12f;
constexpr float LFO_CODE_TEXT_MIN_CD = 8.0f; // seconds
constexpr float LFO_CODE_TEXT_MAX_CD = 25.0f;

// ── TriggerBus ───────────────────────────────────────────────────────────────
constexpr float TRIGGER_ZONE_IMBALANCE_CD = 20.0f;
constexpr int TRIGGER_ZONE_IMBALANCE_DIFF = 2;
constexpr float TRIGGER_DENSITY_HIGH_CD = 30.0f;
constexpr int TRIGGER_DENSITY_HIGH_COUNT = 3; // retuned for the 4-slot model (was 7, tuned for up to 18 organic fragments)
constexpr float TRIGGER_DENSITY_CRIT_CD = 60.0f;
constexpr int TRIGGER_DENSITY_CRIT_COUNT = 4; // all slots full (was 10)
constexpr float TRIGGER_LONG_SILENCE_CD = 20.0f;
constexpr float TRIGGER_LONG_SILENCE_SECS = 15.0f;
constexpr float TRIGGER_PHASE_TRANSITION_CD = 5.0f;
constexpr int TRIGGER_HUB_MIN_CONNECTIONS = 3;
constexpr float TRIGGER_MEASUREMENT_HUB_CD = 45.0f;
constexpr float TRIGGER_DENSITY_HIGH_INTERVAL_SCALE = 0.80f;
constexpr float TRIGGER_DENSITY_HIGH_DESAT_MAX = 0.55f;
constexpr float TRIGGER_DENSITY_CRIT_GRID_DIM = 0.10f;
constexpr float TRIGGER_ZONE_IMBALANCE_SCORE_MULT = 3.0f;

// ── GridState ────────────────────────────────────────────────────────────────
constexpr float GRIDSTATE_DECAY_RATE = 0.995f;
constexpr float GRIDSTATE_PLACEMENT_PENALTY = 0.30f; // score reduction weight
constexpr float GRIDSTATE_WARM_AMOUNT = 0.18f; // ground tint strength

// ── MotionExtraction overlay ──────────────────────────────────────────────────
// Ported from quadrant-crosshair, which drove these from crosshair velocity/
// stillness. blueprint_emergence has no crosshair, so intensity is driven by
// GridState's average activity (placement cadence) and the composition
// phase instead — quiet during BLANK/RESET_HOLD, ramping through PLACEMENT/
// DENSITY, fading through DISSOLVE. Mode is kept fixed (LUMA_GLOW +
// REFERENCE_ACCUMULATION) — no crosshair-style mode switching to replicate.
constexpr float MOTION_DECAY_MIN = 0.90f; // less activity -> faster-fading ghost
constexpr float MOTION_DECAY_MAX = 0.985f; // more activity -> longer-lingering ghost
constexpr float MOTION_SENSITIVITY_MIN = 1.5f;
constexpr float MOTION_SENSITIVITY_MAX = 5.0f;
constexpr float MOTION_OPACITY_MIN = 0.05f;
constexpr float MOTION_OPACITY_MAX = 0.35f;
constexpr float MOTION_OPACITY_SMOOTH_SECONDS = 2.0f; // time constant for easing toward the target opacity

// ── ErosionFBO ───────────────────────────────────────────────────────────────
// decayRate is the fraction of the prior frame retained each frame in a
// normalized blend (mix(current, history, decayRate)) — at 24fps, 0.92 means
// roughly an 0.5-1s fading trail.
constexpr float EROSION_DECAY_RATE = 0.92f;

// ── Fragment shader effects ──────────────────────────────────────────────────
// Each stable/drifting fragment autonomously cycles through the shader pool
// (mirrors quadrant-crosshair's per-quadrant ShaderSlot cycle), independent
// of the always-on desaturate ramp.
constexpr float FRAG_EFFECT_SILENCE_MIN = 6.0f;
constexpr float FRAG_EFFECT_SILENCE_MAX = 16.0f;
constexpr float FRAG_EFFECT_FADE_MIN = 1.5f;
constexpr float FRAG_EFFECT_FADE_MAX = 3.0f;
constexpr float FRAG_EFFECT_DWELL_MIN = 8.0f;
constexpr float FRAG_EFFECT_DWELL_MAX = 18.0f;

// Quadrant-style scale drift — a slow noise-driven "breathing" around each
// fragment's own center, independent per slot. Kept subtle relative to
// quadrant-crosshair's wider ranges since fragments are precisely
// snap-placed against grid lines; too much drift would visibly clash with
// neighboring lines.
constexpr float FRAG_SCALE_MIN_LO = 0.90f;
constexpr float FRAG_SCALE_MIN_HI = 0.97f;
constexpr float FRAG_SCALE_MAX_LO = 1.03f;
constexpr float FRAG_SCALE_MAX_HI = 1.12f;
constexpr float FRAG_SCALE_NOISE_SPEED = 0.03f; // matches Quadrant::update()'s t*0.03f

// ── Continuous Cycle Modes ────────────────────────────────────────────────────
// Valid divider X range: columns 2→4 (2/6 to 4/6 of canvas width).
// MIN_DIVIDER_X / MAX_DIVIDER_X are computed at runtime inside BEComposition
// using canvasW so they adapt to any window size.
constexpr float DIVIDER_JUMP_ANIM_DURATION = 2.0f;   // seconds, GHOST_LAYERS mode only

// ── GHOST_LAYERS mode ─────────────────────────────────────────────────────────
constexpr float GHOST_OPACITY_FLOOR        = 0.07f;   // fragments dissolve to this, not 0
constexpr float GHOST_DECAY_BASE           = 0.00015f; // per second at zero GridState activity
constexpr float GHOST_DECAY_ACTIVITY_MULT  = 0.0012f;  // additional per second per activity unit

// ── PERPETUAL mode ────────────────────────────────────────────────────────────
constexpr float PERPETUAL_DISSOLVE_DURATION  = 4.0f;   // rolling-replacement dissolve (seconds)
constexpr int   PERPETUAL_SEED_INTERVAL      = 8;      // placements between seed refreshes
constexpr float DIVIDER_DRIFT_SPEED          = 0.35f;  // pixels/second (~10 min per column)
constexpr float PERPETUAL_MODE_DURATION_MIN  = 240.0f; // 4 minutes minimum
constexpr float PERPETUAL_MODE_DURATION_MAX  = 480.0f; // 8 minutes maximum

// ── Divider axis-flip animation ───────────────────────────────────────────────
constexpr float DIVIDER_AXIS_FLIP_CHANCE     = 0.20f;  // 20% of relocations trigger a rotation
constexpr float DIVIDER_ROTATION_DURATION    = 2.5f;   // seconds for full 90° arc
constexpr float MIN_TRAVEL_DEGREES          = 15.0f;  // degrees swept before pivot B candidates qualify
constexpr float INTERSECTION_SNAP_RADIUS    = 6.0f;   // px — distance to line for intersection to count
constexpr float MIN_PIVOT_DISTANCE          = 30.0f;  // px — pivot B must be at least this far from A
