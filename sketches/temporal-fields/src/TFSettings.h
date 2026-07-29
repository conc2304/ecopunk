#pragma once

constexpr int TARGET_FPS = 24;

// Slowed for a calmer, airier feel (was 25) — pattern switches roughly
// every minute and a half instead of every 25s.
constexpr float CYCLE_DURATION = 90.0f;

// --- Pattern A: Amorphic BSP ---
constexpr float BSP_IRREGULARITY = 0.55f;
constexpr float BSP_CELL_DENSITY = 0.06f;
// Slowed further still (was 3.0, then 12.0, then 30.0) — fragments now
// live much longer before repartitioning.
constexpr float BSP_GEOMETRY_RESHUFFLE_RATE = 75.0f;
constexpr int BSP_REGIONS_TOUCHED_PER_TICK = 1;
constexpr float BSP_TRANSPARENCY_AMOUNT = 0.15f; // nonzero default — ships alongside the background layer it's meant to reveal

// --- Pattern B: Blob Grid ---
// Fewer, larger cells feel less crowded (was 18).
constexpr int BLOBGRID_GRID_RESOLUTION = 12;
constexpr int BLOBGRID_BLOB_CENTERS = 3;
// Slowed drift further (was 1.0, then 0.4) — fragment motion overall is
// now ~3x slower than the prior pass.
constexpr float BLOBGRID_DRIFT_SPEED = 0.133f;
constexpr float BLOBGRID_BLOB_RADIUS = 0.28f;
constexpr float BLOBGRID_EDGE_SOFTNESS = 0.30f;
constexpr float BLOBGRID_SIZE_VARIATION = 0.50f;
// Slowed further still (was 3.0, then 12.0, then 30.0), matches BSP's
// reshuffle-rate slowdown.
constexpr float BLOBGRID_FRAGMENT_REFRESH_RATE = 75.0f;

// Fragment masking & background compositing (Section 6a)
constexpr bool BLOBGRID_MASK_TO_BLOB = true;
constexpr bool BLOBGRID_TRANSPARENT_BACKGROUND = false;

// --- Shared Pattern Regen Rate ---
// Used by the three wholesale-reshuffling patterns (Bands, Column Grid,
// Telescoping Frames) — separate from BSP's Geometry
// Reshuffle Rate and Blob Grid's Fragment Refresh Rate, which stay
// pattern-specific. Particle Field doesn't use this at all (see its own
// constants below). Matches BSP/Blob Grid's slowed cadence.
constexpr float PATTERN_REGEN_RATE = 75.0f;

// --- Pattern C: Bands ---
constexpr int BANDS_BAND_COUNT = 10;
constexpr float BANDS_WIDTH_VARIATION = 0.3f;
constexpr float BANDS_DIAGONAL_ANGLE_DEG = 30.0f;

// --- Pattern D: Column Grid ---
constexpr int COLUMNGRID_COLUMN_COUNT = 6;
constexpr int COLUMNGRID_ROWS_PER_COLUMN = 6;
constexpr float COLUMNGRID_ROW_HEIGHT_VARIATION = 0.35f;
constexpr bool COLUMNGRID_BRICK_OFFSET = true;

// --- Pattern F: Telescoping Frames ---
constexpr int TELESCOPINGFRAMES_RING_COUNT = 6;
constexpr float TELESCOPINGFRAMES_THICKNESS_VARIATION = 0.35f;

// --- Pattern G: Particle Field ---
// Browser-prototype starting points — not yet validated against real
// footage or Pi hardware. Max Particle Count is cheap even at the
// prototype's ~200 ceiling: particles reference the same small (6-slot)
// shared playhead-texture pool via many draw calls, not one texture each
// (see Section 0 of the brief).
// Spawn rate cut and drift slowed ~3x for the "airy and calm" pass — see
// TFSettings.h's other _DRIFT_SPEED/_NOISE_TIME_SPEED reductions.
constexpr float PARTICLEFIELD_SPAWN_RATE = 1.0f;
constexpr int PARTICLEFIELD_MAX_PARTICLE_COUNT = 60;
constexpr float PARTICLEFIELD_MIN_SIZE = 0.05f;
constexpr float PARTICLEFIELD_MAX_SIZE = 0.18f;
constexpr float PARTICLEFIELD_MIN_LIFE = 3.0f;
constexpr float PARTICLEFIELD_MAX_LIFE = 8.0f;
constexpr float PARTICLEFIELD_DRIFT_SPEED = 0.33f;

// --- Transition system (per-fragment content changes) ---
// Slower, gentler transitions (was 0.8, then 1.6) — still read as abrupt.
constexpr float FRAGMENT_TRANSITION_DURATION = 3.0f;
// Shifted weight away from Hard Cut (jarring) toward Crossfade (calm) —
// was an even 33/34/33 split.
constexpr float TRANSITION_HARD_CUT_WEIGHT = 15.0f;
constexpr float TRANSITION_CROSSFADE_WEIGHT = 60.0f;
constexpr float TRANSITION_EROSION_WEIGHT = 25.0f;

// --- Shared time-offset mechanic (Phase 2) ---
// Fewer bands (was 12) — coarser, less visually busy offset banding.
constexpr int TIME_OFFSET_QUANTIZE_BANDS = 8;
// How far back (seconds) the most-delayed slice reaches — matches
// TimeOffsetVideoBuffer::Settings::maxHistorySeconds's own default. Larger
// = more pronounced differences between slices, smaller = more subtle.
// Kept modest since the history ring buffer lives in CPU RAM (see
// TimeOffsetVideoBuffer.h's Pi 3B memory note).
constexpr float TIME_OFFSET_MAX_HISTORY_SECONDS = 3.0f;

// --- Background layer: mode selection ---
constexpr float BACKGROUND_FULL_VIDEO_WEIGHT = 40.0f;
constexpr float BACKGROUND_FULL_IMAGE_WEIGHT = 30.0f;
constexpr float BACKGROUND_SPLIT_WEIGHT = 30.0f;
// Slowed (was 20) — fewer mode changes per minute.
constexpr float BACKGROUND_MODE_CHANGE_INTERVAL = 90.0f;
constexpr float BACKGROUND_SPLIT_RATIO = 0.5f; // 0.33 is an example alt value, not the default

// --- Background layer: effect cycling ---
// Slowed (was 8) — effects linger much longer between changes.
constexpr float BACKGROUND_EFFECT_CYCLE_INTERVAL = 45.0f;
// Raised so raw/unmodified video dominates the background layer (was 20,
// vs. 5 per named effect) — "airy and calm" favors plain footage over
// shader noise.
constexpr float BACKGROUND_EFFECT_RAW_WEIGHT = 60.0f;
constexpr float BACKGROUND_EFFECT_DEFAULT_WEIGHT = 5.0f; // starting point, one per named effect

// --- Background layer: image cycling ---
// Slowed (was 10/2) — images hold longer and fade more gently.
constexpr float BACKGROUND_IMAGE_CYCLE_INTERVAL = 40.0f;
constexpr float BACKGROUND_IMAGE_FADE_DURATION = 4.0f;

// --- Parameter evolution (Phase 7) ---
// None of these three have a brief-specified default (the brief names the
// mechanism, not exact numbers) — reasonable starting points, not tuned.
constexpr bool EVOLUTION_ENABLED = true;
// Scenes (a waypoint's parameter drift) hold for a random duration in this
// range before the next one is picked — user-specified 30-60s band.
constexpr float WAYPOINT_CHANGE_INTERVAL_MIN = 30.0f;
constexpr float WAYPOINT_CHANGE_INTERVAL_MAX = 60.0f;
// Gentler pull (was 0.15) — dials drift toward a waypoint slowly instead
// of snapping toward it.
constexpr float WAYPOINT_PULL_STRENGTH = 0.05f; // per-second exponential pull toward the waypoint's values
// Wobble amplitude is an internal constant, not a panel dial — the brief
// says every dial should "drift slightly", not that the amount itself
// should be user-tunable. See TFParameterPanel.cpp.

// --- Media playback minimum (TimeOffsetVideoBuffer) ---
// A clip must play at least this long AND complete at least this many
// loops before it's eligible to be swapped out at a pattern switch —
// whichever condition is satisfied later. Prevents fast pattern-switch
// cadences from cutting videos short.
constexpr float MEDIA_MIN_PLAYTIME_SECONDS = 30.0f;
constexpr int MEDIA_MIN_LOOP_COUNT = 4;

// --- Event layer (per-fragment reassignment effects) ---
// Both default true: per-fragment reassignment is the primary cadence (per
// the browser-prototype pick), pattern-switch stays available as an
// alternate/fallback rather than being removed. Runtime-tunable via the
// panel — see TFParameterPanel's "HUD Events" group.
constexpr bool HUD_CADENCE_ON_FRAGMENT_REASSIGN = true;
constexpr bool HUD_CADENCE_ON_PATTERN_SWITCH = true;

// Connection-thread / tick-stamp fade durations, matched to the browser
// prototype's timing — not yet validated against real footage/hardware.
constexpr float CONNECTION_THREAD_LIFETIME = 0.75f; // 0.7-0.8s
constexpr float TICK_STAMP_LIFETIME = 1.05f; // ~1.0-1.1s

// --- Quarantine hatch ---
// BSP: hold duration before a leaf selected for re-partitioning actually
// splits, ~500-550ms per the browser prototype.
constexpr float BSP_QUARANTINE_HOLD_DURATION = 0.52f;
// Blob Grid: cells at or below this coverage (already computed for
// masking) show the hatch, scaled by how close to full fade-out they are.
constexpr float BLOBGRID_HATCH_COVERAGE_THRESHOLD = 0.25f;

// --- Pattern H: Ecological Succession ---
// Browser-prototype starting points — not yet validated against real
// footage or Pi hardware, same caveat as Particle Field's constants above.
constexpr int SUCCESSION_GRID_RESOLUTION = 12;
constexpr float SUCCESSION_MATURITY_TIME = 45.0f; // seconds to reach full climax
constexpr float SUCCESSION_YOUNG_TURNOVER_INTERVAL = 2.0f; // reassignment interval at maturity 0
constexpr float SUCCESSION_CLIMAX_TURNOVER_INTERVAL = 20.0f; // reassignment interval at maturity 1
constexpr float SUCCESSION_YOUNG_HARD_CUT_WEIGHT = 70.0f; // hard-cut weight at maturity 0, lerped to 0 at maturity 1
constexpr float SUCCESSION_CLIMAX_EROSION_WEIGHT = 70.0f; // erosion weight at maturity 1, lerped from 0 at maturity 0
constexpr float SUCCESSION_CROSSFADE_WEIGHT = 30.0f; // held constant across maturity
constexpr float SUCCESSION_TRANSITION_DURATION = 1.2f;
// Patches below this maturity are freshly disturbed / not yet "sprouted" —
// skip the draw call entirely (Section 0's Pi requirement) rather than
// drawing at alpha 0.
constexpr float SUCCESSION_SPROUT_THRESHOLD = 0.08f;
constexpr float SUCCESSION_MIN_RENDER_SCALE = 0.25f; // inset size just past the sprout threshold
constexpr float SUCCESSION_DISTURBANCE_INTERVAL = 12.0f; // seconds between disturbance events
constexpr float SUCCESSION_DISTURBANCE_RADIUS = 0.18f; // normalized, fraction of the shorter canvas edge
// Reuses the Quarantine Hatch visual (TFQuarantineHatch.h) for the reset
// flash rather than a second overlapping visual language — see the brief.
constexpr float SUCCESSION_DISTURBANCE_FLASH_DURATION = 0.6f;

// --- Pattern I: Network Growth ---
constexpr int NETWORKGROWTH_SEED_NODE_COUNT = 2; // 1-3
constexpr float NETWORKGROWTH_GROWTH_INTERVAL = 2.0f; // seconds between branch attempts
constexpr int NETWORKGROWTH_MAX_NODE_COUNT = 60;
constexpr float NETWORKGROWTH_BRANCH_DISTANCE = 0.08f; // normalized, fraction of the shorter canvas edge
constexpr float NETWORKGROWTH_BRANCH_DISTANCE_JITTER = 0.35f; // +/- fraction of branchDistance
constexpr float NETWORKGROWTH_BRANCH_ANGLE_JITTER_DEG = 35.0f;
constexpr float NETWORKGROWTH_NODE_MIN_SIZE = 0.04f; // normalized, fraction of the shorter canvas edge
constexpr float NETWORKGROWTH_NODE_MAX_SIZE = 0.09f;
constexpr float NETWORKGROWTH_NODE_REASSIGN_INTERVAL = 6.0f; // each node's own offset-reassignment clock
constexpr float NETWORKGROWTH_RECTANGULAR_CHANCE = 20.0f; // 0-100%
constexpr float NETWORKGROWTH_MIN_ASPECT_RATIO = 1.2f; // long:short ratio, rectangular nodes only
constexpr float NETWORKGROWTH_MAX_ASPECT_RATIO = 2.5f;
constexpr float NETWORKGROWTH_CROP_CHANCE = 50.0f; // 0-100%, square nodes only
constexpr bool NETWORKGROWTH_NODE_LIFESPAN_ENABLED = true;
constexpr float NETWORKGROWTH_MAX_NODE_AGE = 30.0f; // seconds; only used when lifespan is enabled
constexpr float NETWORKGROWTH_EDGE_THICKNESS = 1.5f;
constexpr float NETWORKGROWTH_TRANSITION_DURATION = 1.0f;
constexpr float NETWORKGROWTH_HARD_CUT_WEIGHT = 33.0f;
constexpr float NETWORKGROWTH_CROSSFADE_WEIGHT = 34.0f;
constexpr float NETWORKGROWTH_EROSION_WEIGHT = 33.0f;

// --- Pattern J: Temporal Tides ---
// No transition system at all (Section 0 open-question resolution: hard
// cutoff, confirmed) — offset and presence are both a direct per-frame
// function of position and time. See TFPatternTemporalTides.cpp.
constexpr int TIDES_GRID_RESOLUTION = 16;
constexpr float TIDES_TIDE_SPEED = 0.3f;
constexpr float TIDES_WAVE_LENGTH = 1.0f;
constexpr float TIDES_AMPLITUDE = 1.0f;
constexpr int TIDES_WAVE_DIRECTION = 0; // 0 Horizontal / 1 Vertical / 2 Diagonal
// Cells whose wave value falls below this are exposed/dry ground — skip the
// draw call entirely, the same underlying scalar field gating both offset
// and presence (the brief's "shoreline" requirement).
constexpr float TIDES_EXPOSED_THRESHOLD = 0.35f;

// --- Moire drift underlay ---
// Two independently-rotating fine line grids. Values matched to the
// browser prototype — not yet validated against real footage/hardware.
constexpr float MOIRE_LINE_SPACING = 14.0f; // px, within the 12-16px range
constexpr float MOIRE_OPACITY = 0.065f; // within ~5-8%
constexpr float MOIRE_ANGULAR_SPEED_A = 1.4f; // deg/sec
constexpr float MOIRE_ANGULAR_SPEED_B = -2.1f; // deg/sec — different sign and magnitude so the two drift relative to each other
