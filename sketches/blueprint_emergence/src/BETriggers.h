#pragma once

// Blueprint Emergence's trigger vocabulary for the shared TriggerBus
// mechanics (see shared/src/TriggerBus.h). Index order doesn't matter as
// long as it matches the order setup()/setCooldown()/addListener() calls
// use in BEComposition.
enum class BETrigger {
	ZONE_IMBALANCE, // Zone A and Zone B fragment counts differ by 2+
	DENSITY_HIGH, // 7+ fragments placed
	DENSITY_CRITICAL, // 10+ fragments placed (approaching MAX_FRAGMENTS)
	LONG_SILENCE, // 15+ seconds since last fragment placement
	PHASE_TRANSITION, // any phase change (blank->placement, placement->density, etc.)
	MEASUREMENT_HUB, // a single fragment has 3+ measurement lines connecting to it
	CIRCLE_PLACED, // the circle fragment has just been placed (fires once per cycle)
	CYCLE_START, // new cycle has begun
	COUNT
};
