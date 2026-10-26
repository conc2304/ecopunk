// Day/night HUD skin schedule (../src/HudSkinSchedule.h). Pure; no OF.
#include "HudSkinSchedule.h"

#include <iostream>
#include <string>

namespace {
int g_total = 0, g_failures = 0;
void check(bool ok, const char* what, int line) {
	++g_total;
	if (!ok) { ++g_failures; std::cout << "FAIL line " << line << ": " << what << "\n"; }
}
#define CHECK(e) check((e), #e, __LINE__)
} // namespace

int main() {
	using P = HudSkinSchedule::Period;
	HudSkinSchedule s;
	s.day = {{"plywood-marble"}, "day"};
	s.night = {{"dark-moss-experimental", "abyssal-bioluminescence"}, "night"};
	CHECK(s.valid());
	// 07:00..19:00 day; boundaries.
	CHECK(s.periodAt(7 * 60) == P::Day && s.periodAt(18 * 60 + 59) == P::Day);
	CHECK(s.periodAt(19 * 60) == P::Night && s.periodAt(6 * 60 + 59) == P::Night && s.periodAt(0) == P::Night);
	CHECK(s.autoSkin(P::Day) == "plywood-marble" && s.autoSkin(P::Night) == "dark-moss-experimental");
	// Manual cycle: day skins then night skins, wrapping; unknown -> first.
	CHECK(s.nextSkin("plywood-marble") == "dark-moss-experimental");
	CHECK(s.nextSkin("dark-moss-experimental") == "abyssal-bioluminescence");
	CHECK(s.nextSkin("abyssal-bioluminescence") == "plywood-marble");
	CHECK(s.nextSkin("") == "plywood-marble");
	CHECK(s.paletteFor("plywood-marble") == "day" && s.paletteFor("abyssal-bioluminescence") == "night" && s.paletteFor("x") == "night");
	// Day window wrapping past midnight (e.g. 20:00 -> 06:00 "day").
	HudSkinSchedule w = s;
	w.dayStartMinute = 20 * 60;
	w.nightStartMinute = 6 * 60;
	CHECK(w.periodAt(23 * 60) == P::Day && w.periodAt(2 * 60) == P::Day && w.periodAt(12 * 60) == P::Night);
	// One-sided schedule still shows a skin in the empty period.
	HudSkinSchedule o;
	o.night = {{"abyssal-bioluminescence"}, "night"};
	CHECK(o.autoSkin(P::Day) == "abyssal-bioluminescence");
	CHECK(HudSkinSchedule::parseClock("07:30") == 450 && !HudSkinSchedule::parseClock("25:00") && !HudSkinSchedule::parseClock("x"));
	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
