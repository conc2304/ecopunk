// HUD Clock v0.1 — ExperienceRuntime's OS wall-clock sampling
// (shared/src/hud-runtime/WallClockSampler.h), end to end through the HUD's
// pure formatter (shared/src/hud-compositor/HudClockFormat.h).
//
// Deterministic: every instant is a fixed Unix second and every zone is set
// explicitly through TZ, so no check depends on the live system clock or on
// this machine's configured timezone. Expected values were taken from the
// OS itself (`TZ=<zone> date -r <epoch>`), independently of this code.

#include "HudClockFormat.h"
#include "WallClockSampler.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace {

int g_total = 0;
int g_failures = 0;

void check(bool cond, const std::string& what) {
	g_total++;
	if (!cond) {
		g_failures++;
		std::cerr << "FAIL: " << what << "\n";
	}
}

void setTz(const char* zone) { setenv("TZ", zone, 1); } // convertWithOsTimezone() calls tzset() itself

std::string label(const WallClockStatus& s) {
	return hudpresent::hudclock::labelText(s.unixSeconds, s.utcOffsetSeconds, s.timezoneAbbreviation);
}

std::string time(const WallClockStatus& s) {
	return hudpresent::hudclock::timeText(hudpresent::hudclock::localSecondsOfDay(s.unixSeconds, s.utcOffsetSeconds));
}

struct Expect {
	const char* zone;
	int64_t unixSeconds;
	int32_t offset;
	const char* abbreviation;
	bool isDst;
	const char* time;
	const char* label;
};

void checkConversion(const Expect& e) {
	setTz(e.zone);
	auto s = wallclock::convertWithOsTimezone(e.unixSeconds);
	const std::string tag = std::string(e.zone) + " @" + std::to_string(e.unixSeconds);
	check(s.has_value(), tag + ": OS conversion present");
	if (!s) return;
	check(s->unixSeconds == e.unixSeconds, tag + ": instant preserved");
	check(s->utcOffsetSeconds == e.offset, tag + ": offset " + std::to_string(s->utcOffsetSeconds) + " == " + std::to_string(e.offset));
	check(s->timezoneAbbreviation == e.abbreviation, tag + ": abbreviation '" + s->timezoneAbbreviation + "' == '" + e.abbreviation + "'");
	check(s->isDst.has_value() && *s->isDst == e.isDst, tag + ": isDst");
	check(time(*s) == e.time, tag + ": time '" + time(*s) + "' == '" + e.time + "'");
	check(label(*s) == e.label, tag + ": label '" + label(*s) + "' == '" + e.label + "'");
}

void test_dst_transitions_and_offsets() {
	const Expect cases[] = {
		// New York spring forward: 01:59:59 EST -> 03:00:00 EDT.
		{"America/New_York", 1772953199, -18000, "EST", false, "01:59", "SUN 08 MAR 2026 EST"},
		{"America/New_York", 1772953200, -14400, "EDT", true, "03:00", "SUN 08 MAR 2026 EDT"},
		// New York fall back: 01:59:59 EDT -> 01:00:00 EST.
		{"America/New_York", 1793512799, -14400, "EDT", true, "01:59", "SUN 01 NOV 2026 EDT"},
		{"America/New_York", 1793512800, -18000, "EST", false, "01:00", "SUN 01 NOV 2026 EST"},
		// Berlin spring forward: CET -> CEST.
		{"Europe/Berlin", 1774745999, 3600, "CET", false, "01:59", "SUN 29 MAR 2026 CET"},
		{"Europe/Berlin", 1774746000, 7200, "CEST", true, "03:00", "SUN 29 MAR 2026 CEST"},
		// Sydney (southern hemisphere) spring forward in October: AEST -> AEDT.
		{"Australia/Sydney", 1791043199, 36000, "AEST", false, "01:59", "SUN 04 OCT 2026 AEST"},
		{"Australia/Sydney", 1791043200, 39600, "AEDT", true, "03:00", "SUN 04 OCT 2026 AEDT"},
		// Fractional offsets, positive and negative.
		{"Asia/Kolkata", 1791547200, 19800, "IST", false, "17:30", "FRI 09 OCT 2026 IST"},
		{"Asia/Kathmandu", 1791547200, 20700, "+0545", false, "17:45", "FRI 09 OCT 2026 +0545"},
		{"America/St_Johns", 1768478400, -12600, "NST", false, "08:30", "THU 15 JAN 2026 NST"},
		// UTC itself, across a year boundary.
		{"UTC", 1798761599, 0, "UTC", false, "23:59", "THU 31 DEC 2026 UTC"},
		{"UTC", 1798761600, 0, "UTC", false, "00:00", "FRI 01 JAN 2027 UTC"},
	};
	for (const auto& e : cases) checkConversion(e);
}

void test_invalid_timezone_and_unavailable_instant() {
	// An unknown zone name: both macOS and glibc fall back to UTC — a real,
	// coherent OS answer (offset 0), never a crash or garbage offset.
	setTz("Not/A_Real_Zone");
	auto s = wallclock::convertWithOsTimezone(1792065600);
	check(s.has_value() && s->utcOffsetSeconds == 0, "unknown TZ resolves to the OS's UTC fallback");
	if (s) check(!label(*s).empty() && label(*s).size() <= 23, "unknown TZ still yields a bounded label: '" + label(*s) + "'");

	// An instant the OS cannot convert (year overflows struct tm) -> nullopt.
	setTz("UTC");
	check(!wallclock::convertWithOsTimezone(std::numeric_limits<int64_t>::max()).has_value(),
		"unconvertible instant is unavailable, not fabricated");
}

void test_sampler_cadence_and_corrections() {
	using namespace std::chrono;
	setTz("Europe/Berlin");
	wallclock::WallClockSampler sampler;
	const int64_t t = 1792065600; // 2026-10-15T12:00:00Z
	const system_clock::time_point base{seconds(t)};

	// 60 frames inside one second: exactly one OS conversion.
	for (int i = 0; i < 60; ++i) sampler.sample(base + milliseconds(i * 16));
	check(sampler.conversionCount() == 1, "one conversion per distinct second");
	check(sampler.sample(base)->utcOffsetSeconds == 7200, "Berlin is CEST at t");

	// Next second converts again.
	sampler.sample(base + seconds(1));
	check(sampler.conversionCount() == 2, "new second -> new conversion");

	// Wall clock corrected backward by an hour, then forward by a day.
	auto back = sampler.sample(base - hours(1));
	check(sampler.conversionCount() == 3 && back && back->unixSeconds == t - 3600, "backward correction re-sampled immediately");
	auto fwd = sampler.sample(base + hours(24));
	check(fwd && fwd->unixSeconds == t + 86400 && label(*fwd) == "FRI 16 OCT 2026 CEST", "forward correction re-sampled immediately");

	// Pre-epoch instants floor to the earlier second (not truncate toward 0).
	auto pre = sampler.sample(system_clock::time_point{milliseconds(-1)});
	check(pre && pre->unixSeconds == -1, "pre-epoch sub-second floors to -1");
}

void test_sampler_timezone_change() {
	setTz("Europe/Berlin");
	wallclock::WallClockSampler sampler;
	const int64_t t = 1792065600;
	auto berlin = sampler.sampleUnixSeconds(t);
	check(berlin && berlin->timezoneAbbreviation == "CEST", "starts in CEST");

	// Zone reconfigured: picked up on the next distinct second (tzset per conversion).
	setTz("Asia/Kolkata");
	sampler.sampleUnixSeconds(t); // same second: cached, by design
	auto kolkata = sampler.sampleUnixSeconds(t + 1);
	check(kolkata && kolkata->utcOffsetSeconds == 19800 && kolkata->timezoneAbbreviation == "IST",
		"timezone change honoured within one second");
	check(kolkata && time(*kolkata) == "17:30", "local time follows the new zone");

	setTz("America/New_York");
	auto ny = sampler.sampleUnixSeconds(t + 2);
	check(ny && ny->utcOffsetSeconds == -14400 && ny->isDst == true, "second reconfiguration (DST zone) honoured");
}

void test_sampler_unavailable_then_recovers() {
	bool available = false;
	wallclock::WallClockSampler sampler([&](int64_t unixSeconds) -> std::optional<WallClockStatus> {
		if (!available) return std::nullopt;
		WallClockStatus s;
		s.unixSeconds = unixSeconds;
		s.timezoneAbbreviation = "UTC";
		s.isDst = false;
		return s;
	});
	check(!sampler.sampleUnixSeconds(100).has_value(), "unavailable OS conversion -> nullopt snapshot");
	available = true;
	check(sampler.sampleUnixSeconds(101).has_value(), "recovers on the next second once the OS answers");
}

} // namespace

int main() {
	const char* originalTz = std::getenv("TZ");
	const std::string savedTz = originalTz ? originalTz : "";

	test_dst_transitions_and_offsets();
	test_invalid_timezone_and_unavailable_instant();
	test_sampler_cadence_and_corrections();
	test_sampler_timezone_change();
	test_sampler_unavailable_then_recovers();

	if (originalTz) setenv("TZ", savedTz.c_str(), 1);
	else unsetenv("TZ");

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed.\n";
	return g_failures == 0 ? 0 : 1;
}
