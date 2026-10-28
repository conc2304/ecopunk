// Experience Settings page v0.1 — page model + SystemClockService (OF-free).

#include "ExperienceSettingsPage.h"
#include "SystemClockService.h"

#include <iostream>
#include <string>
#include <vector>

using namespace settingspage;
using systemclock::NtpState;

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

const std::vector<std::string> kZones = {"UTC", "America/Denver", "America/New_York", "Europe/Madrid", "America/Argentina/Buenos_Aires", "Asia/Tokyo"};

Context makeContext(bool writable, NtpState ntp = NtpState::On) {
	Context c;
	c.settings.sceneAutoCycle = {false, 120.0f};
	c.settings.mediaAutoCycle = {true, 30.0f};
	c.persistenceEnabled = true;
	c.clock.local = {2026, 10, 27, 16, 42};
	c.clock.second = 7;
	c.clock.timezone = "America/Denver";
	c.clock.ntp = ntp;
	c.clockWritable = writable;
	c.clockWriteNote = "Platform must authorise clock writes.";
	c.zones = &kZones;
	c.commonZones = 4;
	return c;
}

std::vector<Action> press(Page& p, Dpad d, int times = 1) {
	std::vector<Action> all;
	for (int i = 0; i < times; ++i) {
		auto a = p.handle(d);
		all.insert(all.end(), a.begin(), a.end());
	}
	return all;
}

void gotoRow(Page& p, RowId row) {
	for (int i = 0; i < kRowCount && p.focusedRow() != row; ++i) p.handle(Dpad::Down);
}

void test_focus_traversal() {
	Page p;
	p.open(makeContext(false));
	check(p.isOpen() && p.focusedRow() == RowId::ReturnTop, "opens with focus on Return to Experience (first)");
	p.handle(Dpad::Down);
	check(p.focusedRow() == RowId::SceneAuto, "Down moves to the next row");
	p.handle(Dpad::Up);
	p.handle(Dpad::Up);
	check(p.focusedRow() == RowId::ReturnBottom, "Up from the first row wraps to Return (last)");
	auto a = press(p, Dpad::Ok);
	check(a.size() == 1 && a[0].kind == Action::Kind::Close, "OK on Return closes");
	Page q;
	q.open(makeContext(false));
	auto b = press(q, Dpad::Back);
	check(b.size() == 1 && b[0].kind == Action::Kind::Close, "Back/Esc closes from the list");
	check(press(q, Dpad::Left).empty() && press(q, Dpad::Right).empty(), "Left/Right do nothing outside editing");
}

void test_toggles_and_intervals() {
	Page p;
	p.open(makeContext(false));
	gotoRow(p, RowId::SceneAuto);
	auto a = press(p, Dpad::Ok);
	check(a.size() == 1 && a[0].kind == Action::Kind::SetSceneAuto && a[0].flag, "OK toggles scene auto-cycle (off -> on) immediately");
	gotoRow(p, RowId::MediaAuto);
	a = press(p, Dpad::Ok);
	check(a.size() == 1 && a[0].kind == Action::Kind::SetMediaAuto && !a[0].flag, "OK toggles media auto-cycle (on -> off)");

	gotoRow(p, RowId::SceneInterval);
	check(press(p, Dpad::Ok).empty() && p.editing(), "OK on an interval enters editing without changing anything");
	check(press(p, Dpad::Right, 2).empty(), "stepping emits no action (nothing persists per D-pad step)");
	a = press(p, Dpad::Ok);
	check(a.size() == 1 && a[0].kind == Action::Kind::SetSceneInterval && a[0].seconds == 300.0f && !p.editing(), "confirm commits the stepped value (120 s + 2 list steps = 300 s)");

	gotoRow(p, RowId::MediaInterval);
	press(p, Dpad::Ok);
	press(p, Dpad::Left, 99);
	a = press(p, Dpad::Ok);
	check(a.size() == 1 && a[0].seconds == 5.0f, "media interval clamps at the 5 s minimum (no wrap)");
	press(p, Dpad::Ok);
	press(p, Dpad::Right, 99);
	a = press(p, Dpad::Ok);
	check(a.size() == 1 && a[0].seconds == 3600.0f, "media interval clamps at the 3600 s maximum");

	// Cancel paths
	gotoRow(p, RowId::SceneInterval);
	press(p, Dpad::Ok);
	press(p, Dpad::Right);
	press(p, Dpad::Down); // CANCEL slot
	check(p.editing() && press(p, Dpad::Ok).empty() && !p.editing(), "OK on CANCEL abandons the edit with no action");
	press(p, Dpad::Ok);
	press(p, Dpad::Right);
	check(press(p, Dpad::Back).empty() && !p.editing(), "Back/Esc abandons the edit with no action");
	check(p.focusedRow() == RowId::SceneInterval, "focus stays on the row after cancelling (not trapped)");

	for (float s : sceneIntervalSteps()) check(installation::validSceneDuration(s), "scene step valid: " + std::to_string(s));
	for (float s : mediaIntervalSteps()) check(installation::validMediaDuration(s), "media step valid: " + std::to_string(s));
}

void test_clock_staging_and_apply() {
	Page p;
	p.open(makeContext(false));
	gotoRow(p, RowId::Timezone);
	press(p, Dpad::Ok);
	press(p, Dpad::Down); // ZONE slot
	press(p, Dpad::Right);
	press(p, Dpad::Ok);
	check(p.hasStagedChanges() && p.stagedChange().timezone && *p.stagedChange().timezone == "America/New_York", "timezone staged, not applied");
	gotoRow(p, RowId::ApplyClock);
	check(p.applyBlockedReason().find("Requires platform setup") == 0, "Apply is disabled with 'Requires platform setup' when writes are not authorised");
	check(press(p, Dpad::Ok).empty() && !p.confirming(), "OK on a disabled Apply never starts a confirmation");
	bool disabledShown = false;
	for (const auto& r : p.rows()) if (r.id == RowId::ApplyClock) disabledShown = r.disabled && r.value == "(disabled)" && r.note.find("platform") != std::string::npos;
	check(disabledShown, "disabled state is stated in text with the reason");

	Page w;
	w.open(makeContext(true));
	gotoRow(w, RowId::Timezone);
	press(w, Dpad::Ok); press(w, Dpad::Down); press(w, Dpad::Right); press(w, Dpad::Ok);
	gotoRow(w, RowId::ApplyClock);
	check(w.applyBlockedReason().empty(), "Apply enabled when authorised and something is staged");
	press(w, Dpad::Ok);
	check(w.confirming(), "OK on Apply asks for confirmation");
	check(press(w, Dpad::Left).empty() && !w.confirming() && w.hasStagedChanges(), "any other input cancels the confirmation, keeps staging");
	press(w, Dpad::Ok);
	auto a = press(w, Dpad::Ok);
	check(a.size() == 1 && a[0].kind == Action::Kind::ApplyClock && a[0].change.timezone && *a[0].change.timezone == "America/New_York", "second OK emits ApplyClock with the staged change");
	check(!w.hasStagedChanges(), "staging cleared after apply");

	// Nothing staged -> blocked with a reason
	Page n;
	n.open(makeContext(true));
	check(n.applyBlockedReason() == "No clock changes staged.", "no staged changes -> Apply blocked");
}

void test_datetime_editor_and_ntp_conflict() {
	Page p;
	Context c = makeContext(true, NtpState::On);
	c.clock.local = {2026, 1, 31, 23, 59};
	p.open(c);
	gotoRow(p, RowId::DateTime);
	press(p, Dpad::Ok);
	press(p, Dpad::Down);        // MONTH
	press(p, Dpad::Right);       // Feb: day must clamp 31 -> 28
	press(p, Dpad::Down);        // DAY
	bool clamped = false;
	for (const auto& r : p.rows()) if (r.id == RowId::DateTime) clamped = r.value.find("2026-[02]") == std::string::npos && r.value.find("-[28]") != std::string::npos;
	check(clamped, "changing the month clamps the day into the new month (31 -> 28)");
	press(p, Dpad::Down); press(p, Dpad::Right);       // HOUR 23 -> 00 (wraps)
	press(p, Dpad::Down); press(p, Dpad::Right);       // MINUTE 59 -> 00 (wraps)
	press(p, Dpad::Ok);
	check(p.hasStagedChanges() && p.stagedChange().dateTime && p.stagedChange().dateTime->month == 2 && p.stagedChange().dateTime->day == 28
		&& p.stagedChange().dateTime->hour == 0 && p.stagedChange().dateTime->minute == 0, "date/time staged with wrapped hour/minute");
	check(p.applyBlockedReason().find("Time sync = MANUAL") != std::string::npos, "manual time + NTP on -> explicit conflict, Apply blocked");

	gotoRow(p, RowId::TimeSync);
	press(p, Dpad::Ok);
	press(p, Dpad::Right); // AUTOMATIC -> MANUAL
	press(p, Dpad::Ok);
	check(p.stagedChange().ntp && !*p.stagedChange().ntp, "NTP staged OFF");
	check(p.applyBlockedReason().empty(), "staging NTP off clears the manual-time conflict");

	check(systemclock::validDateTime({2024, 2, 29, 10, 0}) && !systemclock::validDateTime({2026, 2, 29, 10, 0}) && !systemclock::validDateTime({2026, 13, 1, 0, 0}),
		"date validation (leap years, month range)");
}

void test_clock_format_and_labels() {
	Context c = makeContext(false);
	check(clockLine(c.clock, true) == "2026-10-27  16:42:07", "24-hour clock line");
	check(clockLine(c.clock, false) == "2026-10-27  4:42:07 PM", "12-hour clock line");
	c.clock.local.hour = 0;
	check(clockLine(c.clock, false) == "2026-10-27  12:42:07 AM", "12-hour midnight");
	Page p;
	p.open(makeContext(false));
	gotoRow(p, RowId::ClockFormat);
	auto a = press(p, Dpad::Ok);
	check(a.size() == 1 && a[0].kind == Action::Kind::SetClock24 && !a[0].flag, "OK toggles the 24-hour preference");
}

void test_clock_service() {
	systemclock::Snapshot s;
	systemclock::SystemClockService::parseTimedatectlShow("Timezone=Europe/Madrid\nNTP=yes\nNTPSynchronized=no\nCanNTP=yes\n", s);
	check(s.timezone == "Europe/Madrid" && s.ntp == NtpState::On && s.ntpSynchronizedKnown && !s.ntpSynchronized, "parse timedatectl show");
	systemclock::SystemClockService::parseTimedatectlShow("Timezone=../../etc/passwd\nNTP=no\n", s);
	check(s.timezone == "Europe/Madrid" && s.ntp == NtpState::Off, "unsafe timezone string rejected, NTP=no parsed");
	check(systemclock::SystemClockService::zoneFromLocaltimeLink("/var/db/timezone/zoneinfo/America/Denver") == "America/Denver", "zone from /etc/localtime link");
	check(systemclock::SystemClockService::zoneFromLocaltimeLink("/etc/localtime-weird").empty(), "no zone from a non-zoneinfo link");

	std::vector<std::vector<std::string>> calls;
	int failOn = -1;
	systemclock::SystemClockService svc([&](const std::vector<std::string>& argv, std::string& out) {
		calls.push_back(argv);
		out = "Failed to set time: Automatic time synchronization is enabled";
		return int(calls.size()) - 1 == failOn ? 1 : 0;
	});
	svc.setWriteEnabledForTesting(false);
	auto r = svc.apply([] { systemclock::ClockChange c; c.timezone = "UTC"; return c; }());
	check(!r.ok && r.message.find("Requires platform setup") == 0 && calls.empty(), "writes refused without authorisation; nothing executed");

	svc.setWriteEnabledForTesting(true);
	systemclock::ClockChange c;
	c.ntp = false;
	c.timezone = "UTC";
	c.dateTime = systemclock::DateTime{2026, 10, 27, 9, 5};
	r = svc.apply(c);
	check(r.ok && calls.size() == 3, "three timedatectl calls");
	check(calls.size() == 3 && calls[0][1] == "--no-ask-password" && calls[0][2] == "set-ntp" && calls[0][3] == "false", "set-ntp first, never prompts");
	check(calls.size() == 3 && calls[1][2] == "set-timezone" && calls[1][3] == "UTC", "then set-timezone");
	check(calls.size() == 3 && calls[2][2] == "set-time" && calls[2][3] == "2026-10-27 09:05:00", "then set-time (after NTP off)");
	check(calls.size() == 3 && calls[0][0] == "/usr/bin/timedatectl", "absolute fixed argv, no shell");

	calls.clear();
	failOn = 0;
	r = svc.apply(c);
	check(!r.ok && r.message.find("FAILED") == 0 && calls.size() == 1, "first failure stops the sequence and is reported as FAILED");
	systemclock::ClockChange bad;
	bad.timezone = "Not/AZone";
	check(!svc.apply(bad).ok, "unknown timezone refused");
	systemclock::ClockChange badTime;
	badTime.dateTime = systemclock::DateTime{2026, 2, 30, 0, 0};
	check(!svc.apply(badTime).ok, "invalid date refused");
}

} // namespace

int main() {
	test_focus_traversal();
	test_toggles_and_intervals();
	test_clock_staging_and_apply();
	test_datetime_editor_and_ntp_conflict();
	test_clock_format_and_labels();
	test_clock_service();
	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
