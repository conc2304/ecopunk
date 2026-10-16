#pragma once

// ============================================================================
// WallClockStatus.h — HUD Clock v0.1 runtime clock snapshot (approved,
// docs/reviews/hud-clock-v0.1-contract-change-proposal.md).
//
// Raw facts about ONE sampled wall-clock instant, all taken from the same
// OS local-time conversion so they are mutually coherent. No formatted text
// lives here — formatting is HUD presentation (hud-compositor/HudClockFormat.h).
//
// Produced only by ExperienceRuntime (via WallClockSampler.h) and carried in
// HudFrameData::wallClock; never produced by a scene. OF-free on purpose so
// dependency-free tests can include it.
// ============================================================================

#include <cstdint>
#include <optional>
#include <string>

struct WallClockStatus {
	// UTC instant, whole Unix epoch seconds.
	int64_t unixSeconds = 0;

	// OS-resolved UTC offset for THIS instant (DST included), seconds east
	// of UTC — e.g. +7200 for CEST, -12600 for NST.
	int32_t utcOffsetSeconds = 0;

	// OS-supplied local zone abbreviation for this instant ("CEST", "IST",
	// "+0545"). Empty when the OS supplied none.
	std::string timezoneAbbreviation;

	// tm_isdst: true/false when the OS knows, nullopt when it reports unknown.
	std::optional<bool> isDst;
};
