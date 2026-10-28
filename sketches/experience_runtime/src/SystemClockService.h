#pragma once

// ============================================================================
// SystemClockService.h — Experience Settings page v0.1: the OS clock seam.
//
// Linux (systemd/Armbian) owns system time, timezone and NTP. This is the
// ONLY place the runtime talks to them, and it is deliberately narrow:
//   * READ: wall-clock fields via localtime_r, plus `timedatectl show`
//     (fixed argv, never a shell string) for timezone / NTP state. On macOS
//     (or without timedatectl) the timezone comes from /etc/localtime and
//     NTP state is reported as unknown.
//   * WRITE: `timedatectl set-timezone|set-ntp|set-time` with a fixed argv
//     (no shell, no sudo, `--no-ask-password` so it can never hang on a
//     polkit prompt). Writes are OFF unless the platform has explicitly
//     authorised them (EXPERIENCE_RUNTIME_CLOCK_WRITE=1, set by the Rock Pi
//     platform after installing a narrowly-scoped polkit rule for the kiosk
//     user) — see writeCapability(). The app never runs as root and never
//     stores a fake wall-clock offset.
//
// OF-free and header-declared so test/system_clock_tests.cpp runs headless;
// process execution is injected (Runner) so tests never execute anything.
// ============================================================================

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace systemclock {

struct DateTime {
	int year = 2026;
	int month = 1;  // 1..12
	int day = 1;    // 1..31
	int hour = 0;   // 0..23
	int minute = 0; // 0..59
};

inline bool isLeapYear(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
inline int daysInMonth(int year, int month) {
	static const int kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	if (month < 1 || month > 12) return 31;
	return month == 2 && isLeapYear(year) ? 29 : kDays[month - 1];
}
constexpr int kMinYear = 2000;
constexpr int kMaxYear = 2099;
inline bool validDateTime(const DateTime& d) {
	return d.year >= kMinYear && d.year <= kMaxYear && d.month >= 1 && d.month <= 12 && d.day >= 1
		&& d.day <= daysInMonth(d.year, d.month) && d.hour >= 0 && d.hour <= 23 && d.minute >= 0 && d.minute <= 59;
}

enum class NtpState { Unknown, On, Off };

struct Snapshot {
	DateTime local;        // OS local date/time (second precision in `second`)
	int second = 0;
	std::string timezone;  // IANA name when discoverable, else empty
	NtpState ntp = NtpState::Unknown;
	bool ntpSynchronized = false;
	bool ntpSynchronizedKnown = false;
	std::string source;    // "timedatectl" | "/etc/localtime" | "none" — where timezone/ntp came from
};

struct ClockChange {
	std::optional<std::string> timezone;
	std::optional<bool> ntp;
	std::optional<DateTime> dateTime;
	bool empty() const { return !timezone && !ntp && !dateTime; }
};

struct ApplyResult {
	bool ok = false;
	std::string message; // human-readable; never claims success on failure
};

struct WriteCapability {
	bool enabled = false;
	std::string note; // why not (shown verbatim as "Requires platform setup: ...")
};

// Runs a fixed argv (no shell). Returns the process exit status (0 = ok) and
// fills `output` with combined stdout/stderr; -1 when it could not be run.
using Runner = std::function<int(const std::vector<std::string>& argv, std::string& output)>;

class SystemClockService {
public:
	SystemClockService();
	explicit SystemClockService(Runner runner, std::string zoneinfoDir = "/usr/share/zoneinfo");

	// Wall clock now + timezone/NTP state (one `timedatectl show` exec on Linux;
	// callers should not poll this every frame).
	Snapshot read() const;

	// Selectable IANA zones: the first commonCount() are a curated shortlist
	// (UTC + common US zones + a few others), then every installed zone,
	// sorted. Enumerated from <zoneinfo>/zone1970.tab; shortlist only if absent.
	const std::vector<std::string>& zones() const { return zones_; }
	size_t commonCount() const { return commonCount_; }
	bool isKnownZone(const std::string& zone) const;

	// Whether apply() may be used, and why not if it cannot.
	WriteCapability writeCapability() const;
	// Tests only: pretend the platform authorised (or refused) writes.
	void setWriteEnabledForTesting(bool enabled) { writeOverride_ = enabled; }
	// Sequence: set-ntp, set-timezone, set-time (so a manual time can follow
	// NTP being switched off in the same change). Stops at the first failure.
	ApplyResult apply(const ClockChange& change) const;

	// Pure helpers (tested): parse `timedatectl show` key=value output.
	static void parseTimedatectlShow(const std::string& text, Snapshot& out);
	static std::string zoneFromLocaltimeLink(const std::string& linkTarget);
	// Real fork/execv runner (no shell, 5 s timeout).
	static int runFixedArgv(const std::vector<std::string>& argv, std::string& output);

private:
	Runner runner_;
	std::string zoneinfoDir_;
	std::vector<std::string> zones_;
	size_t commonCount_ = 0;
	std::optional<bool> writeOverride_;
	void loadZones();
	static bool timedatectlPresent();
};

} // namespace systemclock
