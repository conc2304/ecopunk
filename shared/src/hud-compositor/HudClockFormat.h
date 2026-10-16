#pragma once

// ============================================================================
// HudClockFormat.h — HUD Clock v0.1 presentation formatting (header-only,
// OF-free, no OS calls). Pure arithmetic on one already-sampled instant
// (unix seconds + its OS-resolved UTC offset + abbreviation), so every
// result is deterministic and testable without the live system clock or a
// TZ setting. Civil-date conversion is Howard Hinnant's days_from_civil
// inverse (proleptic Gregorian), valid for the whole kMin..kMaxUnixSeconds
// range below.
//
// Text is ASCII-only (fixed English tables, never locale strftime) because
// every HUD widget draws with OF's ASCII bitmap font.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>

namespace hudpresent {
namespace hudclock {

constexpr int64_t kSecondsPerDay = 86400;
constexpr int64_t kMinUnixSeconds = -62135596800LL; // 0001-01-01T00:00:00Z
constexpr int64_t kMaxUnixSeconds = 253402300799LL; // 9999-12-31T23:59:59Z
// Anything a day or more away from UTC cannot be a real local offset and
// would break the date arithmetic below.
constexpr int32_t kMaxAbsUtcOffsetSeconds = 86399;
// Longest abbreviation shown verbatim; longer or non-printable ones fall back
// to the numeric offset so the label stays inside its 23-glyph budget.
constexpr size_t kMaxAbbreviationLength = 6;

inline int64_t floorDiv(int64_t a, int64_t b) {
	int64_t q = a / b;
	if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
	return q;
}

inline int64_t floorMod(int64_t a, int64_t b) { return a - floorDiv(a, b) * b; }

inline bool isValidInstant(int64_t unixSeconds, int32_t utcOffsetSeconds) {
	return unixSeconds >= kMinUnixSeconds && unixSeconds <= kMaxUnixSeconds
		&& utcOffsetSeconds >= -kMaxAbsUtcOffsetSeconds && utcOffsetSeconds <= kMaxAbsUtcOffsetSeconds;
}

// Local calendar day index (days since 1970-01-01 local).
inline int64_t localDayNumber(int64_t unixSeconds, int32_t utcOffsetSeconds) {
	return floorDiv(unixSeconds + utcOffsetSeconds, kSecondsPerDay);
}

// Local time of day, 0..86399.
inline int localSecondsOfDay(int64_t unixSeconds, int32_t utcOffsetSeconds) {
	return static_cast<int>(floorMod(unixSeconds + utcOffsetSeconds, kSecondsPerDay));
}

struct CivilDate {
	int64_t year = 1970;
	int month = 1;   // 1..12
	int day = 1;     // 1..31
	int weekday = 4; // 0 = Sunday .. 6 = Saturday
};

inline CivilDate civilFromDays(int64_t days) {
	const int64_t z = days + 719468;
	const int64_t era = floorDiv(z, 146097);
	const int64_t doe = z - era * 146097;                                 // [0, 146096]
	const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399]
	const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);          // [0, 365]
	const int64_t mp = (5 * doy + 2) / 153;                               // [0, 11]
	CivilDate d;
	d.day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
	d.month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
	d.year = yoe + era * 400 + (d.month <= 2 ? 1 : 0);
	d.weekday = static_cast<int>(floorMod(days + 4, 7)); // 1970-01-01 was a Thursday
	return d;
}

// tzdata-style numeric zone: "+05", "+0530", "-0330", "+00".
inline std::string offsetText(int32_t utcOffsetSeconds) {
	const char sign = utcOffsetSeconds < 0 ? '-' : '+';
	const int32_t magnitude = utcOffsetSeconds < 0 ? -utcOffsetSeconds : utcOffsetSeconds;
	const int hours = magnitude / 3600;
	const int minutes = (magnitude % 3600) / 60;
	char buf[16];
	if (minutes == 0) std::snprintf(buf, sizeof(buf), "%c%02d", sign, hours);
	else std::snprintf(buf, sizeof(buf), "%c%02d%02d", sign, hours, minutes);
	return buf;
}

inline bool isDisplayableAbbreviation(const std::string& abbreviation) {
	if (abbreviation.empty() || abbreviation.size() > kMaxAbbreviationLength) return false;
	for (char c : abbreviation) {
		if (c < 0x21 || c > 0x7E) return false;
	}
	return true;
}

inline std::string zoneText(const std::string& abbreviation, int32_t utcOffsetSeconds) {
	return isDisplayableAbbreviation(abbreviation) ? abbreviation : offsetText(utcOffsetSeconds);
}

// "THU 15 OCT 2026 CEST" — at most 22 glyphs (15 + space + 6).
inline std::string labelText(int64_t unixSeconds, int32_t utcOffsetSeconds, const std::string& abbreviation) {
	static const char* const kWeekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
	static const char* const kMonths[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
	const CivilDate d = civilFromDays(localDayNumber(unixSeconds, utcOffsetSeconds));
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%s %02d %s %04lld", kWeekdays[d.weekday], d.day, kMonths[d.month - 1],
		static_cast<long long>(d.year));
	return std::string(buf) + " " + zoneText(abbreviation, utcOffsetSeconds);
}

// "14:32" from local seconds-of-day (any value is wrapped into 0..86399).
inline std::string timeText(int64_t secondsOfDay) {
	const int64_t s = floorMod(secondsOfDay, kSecondsPerDay);
	char buf[8];
	std::snprintf(buf, sizeof(buf), "%02d:%02d", static_cast<int>(s / 3600), static_cast<int>((s % 3600) / 60));
	return buf;
}

// Rebuilds a string only when what it displays can change: the time text on
// a new displayed minute, the label on a new local date, offset, abbreviation
// or DST state. A wall-clock correction or a timezone change therefore
// invalidates exactly when (and only when) it changes the visible text.
class ClockTextCache {
public:
	const std::string& label(int64_t unixSeconds, int32_t utcOffsetSeconds, const std::string& abbreviation,
		std::optional<bool> isDst) {
		const int64_t day = localDayNumber(unixSeconds, utcOffsetSeconds);
		if (!haveLabel_ || day != labelDay_ || utcOffsetSeconds != labelOffset_ || abbreviation != labelAbbreviation_
			|| isDst != labelIsDst_) {
			labelText_ = labelText(unixSeconds, utcOffsetSeconds, abbreviation);
			labelDay_ = day;
			labelOffset_ = utcOffsetSeconds;
			labelAbbreviation_ = abbreviation;
			labelIsDst_ = isDst;
			haveLabel_ = true;
			++labelBuilds_;
		}
		return labelText_;
	}

	const std::string& time(int64_t secondsOfDay) {
		const int64_t minute = floorDiv(floorMod(secondsOfDay, kSecondsPerDay), 60);
		if (!haveTime_ || minute != timeMinute_) {
			timeText_ = timeText(secondsOfDay);
			timeMinute_ = minute;
			haveTime_ = true;
			++timeBuilds_;
		}
		return timeText_;
	}

	uint64_t labelBuilds() const { return labelBuilds_; }
	uint64_t timeBuilds() const { return timeBuilds_; }

private:
	bool haveLabel_ = false;
	int64_t labelDay_ = 0;
	int32_t labelOffset_ = 0;
	std::string labelAbbreviation_;
	std::optional<bool> labelIsDst_;
	std::string labelText_;
	uint64_t labelBuilds_ = 0;

	bool haveTime_ = false;
	int64_t timeMinute_ = 0;
	std::string timeText_;
	uint64_t timeBuilds_ = 0;
};

} // namespace hudclock
} // namespace hudpresent
