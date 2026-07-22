#pragma once

constexpr int TARGET_FPS = 24;

// Slowed for a calmer, airier feel (was 25) — pattern switches roughly
// every minute and a half instead of every 25s.
constexpr float CYCLE_DURATION = 90.0f;

// --- Pattern A: Amorphic BSP ---
constexpr float BSP_IRREGULARITY = 0.55f;
constexpr float BSP_CELL_DENSITY = 0.06f;
// Slowed (was 3.0) — fewer geometry reshuffles per minute reads as less
// "busy"/twitchy.
constexpr float BSP_GEOMETRY_RESHUFFLE_RATE = 12.0f;
constexpr int BSP_REGIONS_TOUCHED_PER_TICK = 1;
constexpr float BSP_TRANSPARENCY_AMOUNT = 0.15f; // nonzero default — ships alongside the background layer it's meant to reveal

// --- Pattern B: Blob Grid ---
// Fewer, larger cells feel less crowded (was 18).
constexpr int BLOBGRID_GRID_RESOLUTION = 12;
constexpr int BLOBGRID_BLOB_CENTERS = 3;
// Slowed drift (was 1.0) for a calmer motion feel.
constexpr float BLOBGRID_DRIFT_SPEED = 0.4f;
constexpr float BLOBGRID_BLOB_RADIUS = 0.28f;
constexpr float BLOBGRID_EDGE_SOFTNESS = 0.30f;
constexpr float BLOBGRID_SIZE_VARIATION = 0.50f;
// Slowed (was 3.0), matches BSP's reshuffle-rate slowdown.
constexpr float BLOBGRID_FRAGMENT_REFRESH_RATE = 12.0f;

// Fragment masking & background compositing (Section 6a)
constexpr bool BLOBGRID_MASK_TO_BLOB = true;
constexpr bool BLOBGRID_TRANSPARENT_BACKGROUND = false;

// --- Transition system (per-fragment content changes) ---
// Slower, gentler transitions (was 0.8).
constexpr float FRAGMENT_TRANSITION_DURATION = 1.6f;
// Shifted weight away from Hard Cut (jarring) toward Crossfade (calm) —
// was an even 33/34/33 split.
constexpr float TRANSITION_HARD_CUT_WEIGHT = 15.0f;
constexpr float TRANSITION_CROSSFADE_WEIGHT = 60.0f;
constexpr float TRANSITION_EROSION_WEIGHT = 25.0f;

// --- Shared time-offset mechanic (Phase 2) ---
// Fewer bands (was 12) — coarser, less visually busy offset banding.
constexpr int TIME_OFFSET_QUANTIZE_BANDS = 8;

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
// Slowed (was 30) — waypoints (and any preset/HUD cadence keyed off this)
// change far less often.
constexpr float WAYPOINT_CHANGE_INTERVAL = 120.0f; // seconds between picking a new target preset
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

// --- Moire drift underlay ---
// Two independently-rotating fine line grids. Values matched to the
// browser prototype — not yet validated against real footage/hardware.
constexpr float MOIRE_LINE_SPACING = 14.0f; // px, within the 12-16px range
constexpr float MOIRE_OPACITY = 0.065f; // within ~5-8%
constexpr float MOIRE_ANGULAR_SPEED_A = 1.4f; // deg/sec
constexpr float MOIRE_ANGULAR_SPEED_B = -2.1f; // deg/sec — different sign and magnitude so the two drift relative to each other
