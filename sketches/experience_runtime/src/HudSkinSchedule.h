#pragma once

// HudSkinSchedule — runtime-owned day/night HUD skin selection.
//
// Configured by assets/hud/skins/skin-schedule.json (a runtime file next to
// the skin packages, NOT part of any skin.json — the HUD-004 manifest schema
// is unchanged). Each period lists skin package IDs (in switch order) and a
// text palette name ("day" = dark ink for light skins, "night" = light text
// for dark skins). Auto mode follows local time; the 'v' key cycles every
// skin manually and 'V' returns to the schedule.
//
// Plain C++17 (no OF), unit-tested in test/hud_skin_schedule_tests.cpp.

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

struct HudSkinSchedule {
	enum class Period { Day, Night };

	struct Group {
		std::vector<std::string> skins;
		std::string palette;
	};

	int dayStartMinute = 7 * 60;    // local minutes since midnight
	int nightStartMinute = 19 * 60;
	Group day;
	Group night;

	bool valid() const { return !day.skins.empty() || !night.skins.empty(); }

	// "HH:MM" -> minutes since midnight; nullopt when malformed.
	static std::optional<int> parseClock(const std::string& hhmm) {
		const size_t colon = hhmm.find(':');
		if (colon == std::string::npos) return std::nullopt;
		try {
			const int h = std::stoi(hhmm.substr(0, colon));
			const int m = std::stoi(hhmm.substr(colon + 1));
			if (h < 0 || h > 23 || m < 0 || m > 59) return std::nullopt;
			return h * 60 + m;
		} catch (...) {
			return std::nullopt;
		}
	}

	// Day runs [dayStart, nightStart), wrapping past midnight if needed.
	Period periodAt(int localMinute) const {
		const int t = ((localMinute % 1440) + 1440) % 1440;
		const bool day = dayStartMinute <= nightStartMinute ? (t >= dayStartMinute && t < nightStartMinute)
		                                                    : (t >= dayStartMinute || t < nightStartMinute);
		return day ? Period::Day : Period::Night;
	}

	const Group& group(Period p) const { return p == Period::Day ? day : night; }

	// The skin auto mode shows for a period: its first listed skin, else the
	// other period's first (so a one-sided schedule still shows a skin).
	std::string autoSkin(Period p) const {
		if (!group(p).skins.empty()) return group(p).skins.front();
		const Group& other = group(p == Period::Day ? Period::Night : Period::Day);
		return other.skins.empty() ? std::string() : other.skins.front();
	}

	// Manual cycle order: day skins then night skins.
	std::vector<std::string> allSkins() const {
		std::vector<std::string> all = day.skins;
		for (const auto& s : night.skins) if (std::find(all.begin(), all.end(), s) == all.end()) all.push_back(s);
		return all;
	}

	std::string nextSkin(const std::string& current) const {
		const auto all = allSkins();
		if (all.empty()) return std::string();
		auto it = std::find(all.begin(), all.end(), current);
		return (it == all.end() || std::next(it) == all.end()) ? all.front() : *std::next(it);
	}

	// Palette for a skin: the period that lists it (day wins if both),
	// else "night" (the HUD's default light text).
	std::string paletteFor(const std::string& skinId) const {
		if (std::find(day.skins.begin(), day.skins.end(), skinId) != day.skins.end()) return day.palette.empty() ? "day" : day.palette;
		if (std::find(night.skins.begin(), night.skins.end(), skinId) != night.skins.end()) return night.palette.empty() ? "night" : night.palette;
		return "night";
	}
};
