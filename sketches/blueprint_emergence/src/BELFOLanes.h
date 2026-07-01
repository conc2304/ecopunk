#pragma once

// Blueprint Emergence's lane vocabulary for the shared LFOBank (see
// shared/src/LFOBank.h). Cast to int when calling LFOBank methods.
enum class BELFOLane {
	GRID_OPACITY = 0,
	DIVIDER_BRIGHTNESS,
	PLACEMENT_BIAS,
	FRAG_DESAT_OFFSET_A, // per-fragment desat nudge, bank A
	FRAG_DESAT_OFFSET_B, // per-fragment desat nudge, bank B
	CODE_TEXT_WEIGHT,
	COUNT
};
