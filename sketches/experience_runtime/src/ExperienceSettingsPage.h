#pragma once

// ============================================================================
// ExperienceSettingsPage.h — In-App Experience Management Page v0.1: the
// page MODEL (OF-free, header-only, covered by test/experience_settings_page_
// tests.cpp). ExperienceRuntime owns the one instance; the view
// (ExperienceSettingsPageView) only draws what rows() returns.
//
// Navigation (D-pad; the keyboard arrows + Enter are the same inputs):
//   not editing   Up/Down move focus (wraps; HudFocusNavigator is the focus
//                 owner, disabled rows stay focusable so their reason can be
//                 read). OK: toggle (immediately confirmed + persisted),
//                 begin editing a value row, activate Apply/Return.
//   editing       Up/Down move between the row's edit slots (value fields ...
//                 then CANCEL); Left/Right adjust the current slot; OK on a
//                 value slot confirms, OK on CANCEL (or Back/Esc) abandons.
//                 Focus is never trapped and nothing persists while stepping.
//   confirming    after OK on Apply: OK again applies, any other input cancels.
// The page never touches the OS clock or persistence itself: it returns
// Actions that ExperienceRuntime performs through the one installation
// controller and SystemClockService.
// ============================================================================

#include "DpadInput.h"
#include "HudFocusNavigator.h"
#include "InstallationControl.h"
#include "SystemClockService.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace settingspage {

using Dpad = DpadInput;

enum class RowId {
	ReturnTop,
	SceneAuto,
	SceneInterval,
	MediaAuto,
	MediaInterval,
	ClockFormat,
	Timezone,
	TimeSync,
	DateTime,
	ApplyClock,
	ReturnBottom,
};

inline const char* rowKey(RowId r) {
	switch (r) {
		case RowId::ReturnTop: return "settings.return_top";
		case RowId::SceneAuto: return "settings.scene_auto";
		case RowId::SceneInterval: return "settings.scene_interval";
		case RowId::MediaAuto: return "settings.media_auto";
		case RowId::MediaInterval: return "settings.media_interval";
		case RowId::ClockFormat: return "settings.clock_format";
		case RowId::Timezone: return "settings.timezone";
		case RowId::TimeSync: return "settings.time_sync";
		case RowId::DateTime: return "settings.datetime";
		case RowId::ApplyClock: return "settings.apply_clock";
		case RowId::ReturnBottom: return "settings.return_bottom";
	}
	return "";
}
constexpr int kRowCount = 11;

// Bounded selectable intervals (seconds). Every value is inside the
// installation controller's validated range (scene 10..3600, media 5..3600).
inline const std::vector<float>& sceneIntervalSteps() {
	static const std::vector<float> v = {10, 15, 20, 30, 45, 60, 90, 120, 180, 300, 600, 900, 1800, 3600};
	return v;
}
inline const std::vector<float>& mediaIntervalSteps() {
	static const std::vector<float> v = {5, 10, 15, 20, 30, 45, 60, 90, 120, 180, 300, 600, 900, 1800, 3600};
	return v;
}
inline int nearestStep(const std::vector<float>& steps, float seconds) {
	int best = 0;
	for (size_t i = 0; i < steps.size(); ++i) {
		if (std::fabs(steps[i] - seconds) < std::fabs(steps[size_t(best)] - seconds)) best = int(i);
	}
	return best;
}
inline std::string intervalText(float seconds) {
	char buf[48];
	const int s = int(std::lround(seconds));
	if (s >= 60 && s % 60 == 0) std::snprintf(buf, sizeof(buf), "%d min (%d s)", s / 60, s);
	else std::snprintf(buf, sizeof(buf), "%d s", s);
	return buf;
}

// What the runtime hands the page each frame (a snapshot; the page owns no
// settings, clock or persistence state).
struct Context {
	installation::Settings settings;
	bool persistenceEnabled = false; // a settings file path is configured
	std::string persistenceStatus;   // "saved" / "SAVE FAILED: ..." / "not saved (persistence off)" / ""
	bool persistenceFailed = false;
	systemclock::Snapshot clock;     // current OS clock (live)
	bool clockWritable = false;
	std::string clockWriteNote;
	std::string clockResult;         // outcome of the last Apply ("" until one ran)
	bool clockResultFailed = false;
	const std::vector<std::string>* zones = nullptr;
	size_t commonZones = 0;
};

struct Action {
	enum class Kind { Close, SetSceneAuto, SetSceneInterval, SetMediaAuto, SetMediaInterval, SetClock24, ApplyClock };
	Kind kind = Kind::Close;
	bool flag = false;
	float seconds = 0.0f;
	systemclock::ClockChange change;
};

struct RowView {
	RowId id = RowId::ReturnTop;
	std::string label;
	std::string value;  // already formatted; editing shows [field] markers
	std::string note;   // focused-row hint / disabled reason (always text, never colour-only)
	bool focused = false;
	bool editing = false;
	bool disabled = false;
};

class Page {
public:
	Page() : focus_(slotIds()) {}

	bool isOpen() const { return open_; }
	void setContext(const Context& c) { ctx_ = c; if (open_) refreshFocus(); }
	const Context& context() const { return ctx_; }

	void open(const Context& c) {
		ctx_ = c;
		open_ = true;
		editing_ = false;
		confirming_ = false;
		stagedTz_.reset();
		stagedNtp_.reset();
		stagedDt_.reset();
		focus_ = HudFocusNavigator(slotIds());
		refreshFocus();
		focus_.step(+1); // first step engages: first enabled row (Return to Experience)
		refreshFocus();
	}
	void close() { open_ = false; editing_ = false; confirming_ = false; }

	bool editing() const { return editing_; }
	bool confirming() const { return confirming_; }
	std::optional<RowId> focusedRow() const {
		if (!focus_.focused()) return std::nullopt;
		for (int i = 0; i < kRowCount; ++i) if (*focus_.focused() == rowKey(RowId(i))) return RowId(i);
		return std::nullopt;
	}
	bool hasStagedChanges() const { return stagedTz_ || stagedNtp_ || stagedDt_; }
	systemclock::ClockChange stagedChange() const {
		systemclock::ClockChange c;
		c.timezone = stagedTz_;
		c.ntp = stagedNtp_;
		c.dateTime = stagedDt_;
		return c;
	}

	// Why Apply cannot run right now; empty when it can.
	std::string applyBlockedReason() const {
		if (!hasStagedChanges()) return "No clock changes staged.";
		if (!ctx_.clockWritable) return "Requires platform setup: " + ctx_.clockWriteNote;
		if (stagedDt_ && effectiveNtpOn()) return "Manual time needs Time sync = MANUAL (stage that first).";
		return std::string();
	}

	std::vector<Action> handle(Dpad d) {
		std::vector<Action> out;
		if (!open_) return out;
		refreshFocus();
		if (confirming_) {
			confirming_ = false;
			if (d == Dpad::Ok && applyBlockedReason().empty()) {
				Action a;
				a.kind = Action::Kind::ApplyClock;
				a.change = stagedChange();
				stagedTz_.reset();
				stagedNtp_.reset();
				stagedDt_.reset();
				out.push_back(a);
			}
			return out;
		}
		if (editing_) return handleEditing(d);

		switch (d) {
			case Dpad::Up: focus_.step(-1); break;
			case Dpad::Down: focus_.step(+1); break;
			case Dpad::Left:
			case Dpad::Right: break; // adjusting only happens while editing
			case Dpad::Back: out.push_back(closeAction()); break;
			case Dpad::Ok: activate(out); break;
		}
		return out;
	}

	std::vector<RowView> rows() const {
		std::vector<RowView> v;
		const auto focused = focusedRow();
		for (int i = 0; i < kRowCount; ++i) {
			const RowId id = RowId(i);
			RowView r = describe(id);
			r.focused = focused && *focused == id;
			r.editing = r.focused && editing_;
			if (r.editing) r.value = editingValue(id);
			if (r.focused && confirming_ && id == RowId::ApplyClock) {
				r.value = "CONFIRM?";
				r.note = "OK = apply to the OS clock now.  Any other button = cancel.";
			}
			v.push_back(std::move(r));
		}
		return v;
	}

	// Footer hint for the current input mode.
	std::string hint() const {
		if (confirming_) return "OK: confirm apply     any other button: cancel";
		if (editing_) return "Left/Right: change     Up/Down: next field / CANCEL     OK: confirm";
		return "Up/Down: move     OK: select/toggle/edit     Esc: return to experience";
	}
	std::string editSlotName() const { return editing_ ? slotName() : std::string(); }

private:
	Context ctx_;
	bool open_ = false;
	HudFocusNavigator focus_;
	bool editing_ = false;
	bool confirming_ = false;
	int slot_ = 0;
	int pendingIdx_ = 0;          // interval step index
	size_t zoneIdx_ = 0;          // timezone editor position in ctx_.zones
	bool pendingNtp_ = false;
	systemclock::DateTime pendingDt_;
	std::optional<std::string> stagedTz_;
	std::optional<bool> stagedNtp_;
	std::optional<systemclock::DateTime> stagedDt_;

	static std::vector<std::string> slotIds() {
		std::vector<std::string> ids;
		for (int i = 0; i < kRowCount; ++i) ids.push_back(rowKey(RowId(i)));
		return ids;
	}
	static Action closeAction() { Action a; a.kind = Action::Kind::Close; return a; }

	bool currentNtpOn() const { return ctx_.clock.ntp == systemclock::NtpState::On; }
	bool effectiveNtpOn() const { return stagedNtp_ ? *stagedNtp_ : currentNtpOn(); }

	void refreshFocus() {
		focus_.refresh([&](const std::string& id) {
			return id == rowKey(RowId::ApplyClock) && !applyBlockedReason().empty() ? HudControlAvailability::Disabled
			                                                                        : HudControlAvailability::Enabled;
		});
	}

	// -- activation -------------------------------------------------------
	void activate(std::vector<Action>& out) {
		const auto row = focusedRow();
		if (!row) return;
		switch (*row) {
			case RowId::ReturnTop:
			case RowId::ReturnBottom: out.push_back(closeAction()); break;
			case RowId::SceneAuto: {
				Action a; a.kind = Action::Kind::SetSceneAuto; a.flag = !ctx_.settings.sceneAutoCycle.enabled; out.push_back(a); break;
			}
			case RowId::MediaAuto: {
				Action a; a.kind = Action::Kind::SetMediaAuto; a.flag = !ctx_.settings.mediaAutoCycle.enabled; out.push_back(a); break;
			}
			case RowId::ClockFormat: {
				Action a; a.kind = Action::Kind::SetClock24; a.flag = !ctx_.settings.clock24Hour; out.push_back(a); break;
			}
			case RowId::ApplyClock:
				if (applyBlockedReason().empty()) confirming_ = true; // blocked: the row's note already says why
				break;
			default: beginEdit(*row); break;
		}
	}

	void beginEdit(RowId row) {
		editing_ = true;
		slot_ = 0;
		switch (row) {
			case RowId::SceneInterval: pendingIdx_ = nearestStep(sceneIntervalSteps(), ctx_.settings.sceneAutoCycle.durationSeconds); break;
			case RowId::MediaInterval: pendingIdx_ = nearestStep(mediaIntervalSteps(), ctx_.settings.mediaAutoCycle.durationSeconds); break;
			case RowId::Timezone: {
				const std::string cur = stagedTz_ ? *stagedTz_ : ctx_.clock.timezone;
				zoneIdx_ = 0;
				if (ctx_.zones) {
					// Prefer the shortlist entry, then the full list.
					for (size_t i = 0; i < ctx_.zones->size(); ++i) {
						if ((*ctx_.zones)[i] == cur) { zoneIdx_ = i; break; }
					}
				}
				break;
			}
			case RowId::TimeSync: pendingNtp_ = effectiveNtpOn(); break;
			case RowId::DateTime: pendingDt_ = stagedDt_ ? *stagedDt_ : ctx_.clock.local; break;
			default: editing_ = false; break;
		}
	}

	int slotCount(RowId row) const {
		switch (row) {
			case RowId::DateTime: return 6; // year month day hour minute CANCEL
			case RowId::Timezone: return 3; // region zone CANCEL
			default: return 2;              // value CANCEL
		}
	}

	std::string slotName() const {
		const auto row = focusedRow();
		if (!row) return std::string();
		static const char* dt[] = {"YEAR", "MONTH", "DAY", "HOUR", "MINUTE", "CANCEL"};
		static const char* tz[] = {"REGION", "ZONE", "CANCEL"};
		if (*row == RowId::DateTime) return dt[std::clamp(slot_, 0, 5)];
		if (*row == RowId::Timezone) return tz[std::clamp(slot_, 0, 2)];
		return slot_ == 0 ? "VALUE" : "CANCEL";
	}

	bool onCancelSlot(RowId row) const { return slot_ == slotCount(row) - 1; }

	// -- editing ----------------------------------------------------------
	std::vector<Action> handleEditing(Dpad d) {
		std::vector<Action> out;
		const auto row = focusedRow();
		if (!row) { editing_ = false; return out; }
		switch (d) {
			case Dpad::Back: editing_ = false; break;
			case Dpad::Up: slot_ = (slot_ - 1 + slotCount(*row)) % slotCount(*row); break;
			case Dpad::Down: slot_ = (slot_ + 1) % slotCount(*row); break;
			case Dpad::Left: adjust(*row, -1); break;
			case Dpad::Right: adjust(*row, +1); break;
			case Dpad::Ok:
				if (onCancelSlot(*row)) editing_ = false;
				else commit(*row, out);
				break;
		}
		return out;
	}

	void adjust(RowId row, int dir) {
		if (onCancelSlot(row)) return;
		switch (row) {
			case RowId::SceneInterval:
				pendingIdx_ = std::clamp(pendingIdx_ + dir, 0, int(sceneIntervalSteps().size()) - 1); break;
			case RowId::MediaInterval:
				pendingIdx_ = std::clamp(pendingIdx_ + dir, 0, int(mediaIntervalSteps().size()) - 1); break;
			case RowId::TimeSync: pendingNtp_ = !pendingNtp_; break;
			case RowId::Timezone: adjustZone(dir); break;
			case RowId::DateTime: adjustDateTime(dir); break;
			default: break;
		}
	}

	void adjustZone(int dir) {
		if (!ctx_.zones || ctx_.zones->empty()) return;
		const int n = int(ctx_.zones->size());
		if (slot_ == 1) {
			zoneIdx_ = size_t(((int(zoneIdx_) + dir) % n + n) % n);
			return;
		}
		// REGION slot: jump to the first zone of the next/previous region
		// ("Common" shortlist first, then by the prefix before '/').
		auto region = [&](int i) -> std::string {
			if (size_t(i) < ctx_.commonZones) return "Common";
			const std::string& z = (*ctx_.zones)[size_t(i)];
			const size_t slash = z.find('/');
			return slash == std::string::npos ? z : z.substr(0, slash);
		};
		const std::string cur = region(int(zoneIdx_));
		int i = int(zoneIdx_);
		if (dir > 0) {
			while (i < n && region(i) == cur) ++i;
			if (i >= n) i = 0;
		} else {
			while (i >= 0 && region(i) == cur) --i;      // leave the current region
			if (i < 0) i = n - 1;                        // wrap to the last region ...
			const std::string target = region(i);
			while (i > 0 && region(i - 1) == target) --i; // ... then its first zone
		}
		zoneIdx_ = size_t(i);
	}

	void adjustDateTime(int dir) {
		using namespace systemclock;
		DateTime& d = pendingDt_;
		auto wrap = [&](int v, int lo, int hi) { return v < lo ? hi : v > hi ? lo : v; };
		switch (slot_) {
			case 0: d.year = std::clamp(d.year + dir, kMinYear, kMaxYear); break;
			case 1: d.month = wrap(d.month + dir, 1, 12); break;
			case 2: d.day = wrap(d.day + dir, 1, daysInMonth(d.year, d.month)); break;
			case 3: d.hour = wrap(d.hour + dir, 0, 23); break;
			case 4: d.minute = wrap(d.minute + dir, 0, 59); break;
			default: break;
		}
		d.day = std::min(d.day, daysInMonth(d.year, d.month)); // month/year change can shorten the month
	}

	void commit(RowId row, std::vector<Action>& out) {
		switch (row) {
			case RowId::SceneInterval: {
				Action a; a.kind = Action::Kind::SetSceneInterval; a.seconds = sceneIntervalSteps()[size_t(pendingIdx_)]; out.push_back(a); break;
			}
			case RowId::MediaInterval: {
				Action a; a.kind = Action::Kind::SetMediaInterval; a.seconds = mediaIntervalSteps()[size_t(pendingIdx_)]; out.push_back(a); break;
			}
			case RowId::Timezone:
				if (ctx_.zones && zoneIdx_ < ctx_.zones->size()) {
					const std::string& z = (*ctx_.zones)[zoneIdx_];
					if (z == ctx_.clock.timezone) stagedTz_.reset(); else stagedTz_ = z;
				}
				break;
			case RowId::TimeSync:
				if (ctx_.clock.ntp != systemclock::NtpState::Unknown && pendingNtp_ == currentNtpOn()) stagedNtp_.reset();
				else stagedNtp_ = pendingNtp_;
				break;
			case RowId::DateTime:
				if (systemclock::validDateTime(pendingDt_)) stagedDt_ = pendingDt_;
				break;
			default: break;
		}
		editing_ = false;
	}

	// -- presentation -----------------------------------------------------
	static std::string two(int v) { char b[8]; std::snprintf(b, sizeof(b), "%02d", v); return b; }
	static std::string dateText(const systemclock::DateTime& d) {
		char b[32];
		std::snprintf(b, sizeof(b), "%04d-%02d-%02d %02d:%02d", d.year, d.month, d.day, d.hour, d.minute);
		return b;
	}
	std::string ntpText(systemclock::NtpState n) const {
		using systemclock::NtpState;
		if (n == NtpState::Unknown) return "UNKNOWN (not discoverable here)";
		if (n == NtpState::Off) return "MANUAL (network time off)";
		if (ctx_.clock.ntpSynchronizedKnown) return ctx_.clock.ntpSynchronized ? "AUTOMATIC (NTP, synchronized)" : "AUTOMATIC (NTP, not synchronized)";
		return "AUTOMATIC (NTP)";
	}

	RowView describe(RowId id) const {
		RowView r;
		r.id = id;
		const auto& s = ctx_.settings;
		switch (id) {
			case RowId::ReturnTop: r.label = "<< Return to Experience"; break;
			case RowId::ReturnBottom: r.label = "<< Return to Experience"; break;
			case RowId::SceneAuto:
				r.label = "Scene auto-cycle"; r.value = s.sceneAutoCycle.enabled ? "ON" : "OFF";
				r.note = "OK toggles. Saved immediately. Paused while this page is open.";
				break;
			case RowId::SceneInterval:
				r.label = "Scene interval"; r.value = intervalText(s.sceneAutoCycle.durationSeconds);
				r.note = "OK to edit (10 s - 60 min). Saved on confirm.";
				break;
			case RowId::MediaAuto:
				r.label = "Media auto-cycle"; r.value = s.mediaAutoCycle.enabled ? "ON" : "OFF";
				r.note = "OK toggles. Saved immediately. Scenes that cycle media at their own boundary keep doing so.";
				break;
			case RowId::MediaInterval:
				r.label = "Media interval"; r.value = intervalText(s.mediaAutoCycle.durationSeconds);
				r.note = "Minimum time per clip (5 s - 60 min); longer clips play to their end.";
				break;
			case RowId::ClockFormat:
				r.label = "Time display"; r.value = s.clock24Hour ? "24-hour" : "12-hour";
				r.note = "App preference (saved). It does not change the OS clock.";
				break;
			case RowId::Timezone:
				r.label = "Timezone"; r.value = ctx_.clock.timezone.empty() ? std::string("(unknown)") : ctx_.clock.timezone;
				if (stagedTz_) r.value += "  ->  " + *stagedTz_ + " (staged)";
				r.note = "OK to choose. Staged until you Apply clock changes.";
				break;
			case RowId::TimeSync:
				r.label = "Time sync"; r.value = ntpText(ctx_.clock.ntp);
				if (stagedNtp_) r.value += "  ->  " + std::string(*stagedNtp_ ? "AUTOMATIC" : "MANUAL") + " (staged)";
				r.note = "OK to choose automatic (NTP) or manual. Staged until Apply.";
				break;
			case RowId::DateTime:
				r.label = "Set date & time";
				r.value = stagedDt_ ? dateText(*stagedDt_) + " (staged)" : std::string("(not changing)");
				r.note = "OK to edit year/month/day/hour/minute. Needs Time sync = MANUAL. Staged until Apply.";
				break;
			case RowId::ApplyClock: {
				r.label = "Apply clock changes";
				const std::string blocked = applyBlockedReason();
				r.disabled = !blocked.empty();
				r.value = r.disabled ? "(disabled)" : "READY";
				r.note = r.disabled ? blocked : "OK, then OK again to confirm. Changes the OS clock.";
				break;
			}
		}
		return r;
	}

	std::string editingValue(RowId id) const {
		auto cancel = [&]() { return std::string(onCancelSlot(id) ? "  [CANCEL]" : "  CANCEL"); };
		auto bracket = [&](bool on, const std::string& t) { return on ? "[" + t + "]" : t; };
		switch (id) {
			case RowId::SceneInterval: return "< " + bracket(slot_ == 0, intervalText(sceneIntervalSteps()[size_t(pendingIdx_)])) + " >" + cancel();
			case RowId::MediaInterval: return "< " + bracket(slot_ == 0, intervalText(mediaIntervalSteps()[size_t(pendingIdx_)])) + " >" + cancel();
			case RowId::TimeSync: return "< " + bracket(slot_ == 0, pendingNtp_ ? "AUTOMATIC" : "MANUAL") + " >" + cancel();
			case RowId::Timezone: {
				std::string z = ctx_.zones && zoneIdx_ < ctx_.zones->size() ? (*ctx_.zones)[zoneIdx_] : std::string("?");
				const std::string region = zoneIdx_ < ctx_.commonZones ? "Common" : z.substr(0, z.find('/'));
				return bracket(slot_ == 0, "region " + region) + "  " + bracket(slot_ == 1, z) + cancel();
			}
			case RowId::DateTime: {
				const auto& d = pendingDt_;
				char b[16];
				std::snprintf(b, sizeof(b), "%04d", d.year);
				return bracket(slot_ == 0, b) + "-" + bracket(slot_ == 1, two(d.month)) + "-" + bracket(slot_ == 2, two(d.day)) + " "
					+ bracket(slot_ == 3, two(d.hour)) + ":" + bracket(slot_ == 4, two(d.minute)) + cancel();
			}
			default: return std::string();
		}
	}
};

// "16:42:07" or "4:42:07 PM" — the page's own clock line.
inline std::string clockLine(const systemclock::Snapshot& c, bool clock24Hour) {
	char b[64];
	if (clock24Hour) {
		std::snprintf(b, sizeof(b), "%04d-%02d-%02d  %02d:%02d:%02d", c.local.year, c.local.month, c.local.day, c.local.hour, c.local.minute, c.second);
	} else {
		const int h12 = c.local.hour % 12 == 0 ? 12 : c.local.hour % 12;
		std::snprintf(b, sizeof(b), "%04d-%02d-%02d  %d:%02d:%02d %s", c.local.year, c.local.month, c.local.day, h12, c.local.minute, c.second,
			c.local.hour < 12 ? "AM" : "PM");
	}
	return b;
}

} // namespace settingspage
