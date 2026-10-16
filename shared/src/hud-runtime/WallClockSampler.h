#pragma once

// ============================================================================
// WallClockSampler.h — ExperienceRuntime's OS wall-clock sampling for HUD
// Clock v0.1. Header-only and OF-free (POSIX localtime_r/tzset).
//
// Cadence: sample() is cheap to call every frame — it only reads
// std::chrono::system_clock. The OS local-time conversion runs once per
// distinct whole second, so it also re-runs immediately when the wall clock
// is corrected (forward or backward), and each conversion calls tzset() so a
// changed TZ or /etc/localtime is honoured within one second. Nothing here
// assumes a fixed UTC offset: every field comes from one localtime_r() of one
// instant, so date, offset, abbreviation and DST state always agree.
// ============================================================================

#include "WallClockStatus.h"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <functional>
#include <limits>
#include <optional>
#include <time.h>

namespace wallclock {

// One coherent OS conversion of one instant. nullopt when the instant does
// not fit the platform time_t (e.g. past 2038 with a 32-bit time_t) or the OS
// conversion fails — never a fabricated value.
inline std::optional<WallClockStatus> convertWithOsTimezone(int64_t unixSeconds) {
	if (unixSeconds < static_cast<int64_t>(std::numeric_limits<std::time_t>::min())
		|| unixSeconds > static_cast<int64_t>(std::numeric_limits<std::time_t>::max())) {
		return std::nullopt;
	}
	const std::time_t t = static_cast<std::time_t>(unixSeconds);
	std::tm local{};
	tzset(); // localtime_r is not required to re-read TZ or /etc/localtime itself
	if (localtime_r(&t, &local) == nullptr) return std::nullopt;

	WallClockStatus status;
	status.unixSeconds = unixSeconds;
	status.utcOffsetSeconds = static_cast<int32_t>(local.tm_gmtoff);
	if (local.tm_zone != nullptr) status.timezoneAbbreviation = local.tm_zone;
	if (local.tm_isdst > 0) status.isDst = true;
	else if (local.tm_isdst == 0) status.isDst = false;
	return status;
}

class WallClockSampler {
public:
	using Converter = std::function<std::optional<WallClockStatus>(int64_t)>;

	explicit WallClockSampler(Converter convert = convertWithOsTimezone) : convert_(std::move(convert)) {}

	// The current snapshot for wall-clock instant `now`.
	const std::optional<WallClockStatus>& sample(std::chrono::system_clock::time_point now) {
		return sampleUnixSeconds(std::chrono::floor<std::chrono::seconds>(now.time_since_epoch()).count());
	}

	const std::optional<WallClockStatus>& sampleUnixSeconds(int64_t unixSeconds) {
		if (!lastSecond_ || *lastSecond_ != unixSeconds) {
			current_ = convert_(unixSeconds);
			lastSecond_ = unixSeconds;
			++conversions_;
		}
		return current_;
	}

	uint64_t conversionCount() const { return conversions_; }

private:
	Converter convert_;
	std::optional<int64_t> lastSecond_;
	std::optional<WallClockStatus> current_;
	uint64_t conversions_ = 0;
};

} // namespace wallclock
