#pragma once

// SceneFxModel — OF-free, data-driven mode/control model for the Shared Effects Gallery scene.
//
//   Mode    = ONE canonical shared effect (SceneFxModeDescriptor::effectId; identity stays canonical).
//   Control = a curated presentation mapping onto ONE canonical effect parameter (paramId).
//             0..2 per mode (kMaxControls). Numeric (bounded step) or Enum (ordered, human labels).
//
// This is a PRESENTATION/CONTROL mapping, not a second effect catalog: it never copies shader
// source or canonical parameter definitions, only curated labels/bounds layered on top of a
// canonical paramId (the scene validates every paramId/bound against the canonical definition).
// Adding an effect = one SceneFxModeDescriptor row; no HUD, input or SceneManager branch.
//
// ModeController owns: the active mode (wraps), and each mode's current control values (kept per
// mode, so returning to an effect restores what the user set). It does not own focus: parameter
// navigation is the shared HudRegionInteraction state machine (rows = controlCount()).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace scenefx {

constexpr int kMaxControls = 2;

enum class ControlKind { Numeric, Enum };

struct Choice {
	std::string label;               // human-readable, never an internal enum integer
	std::array<float, 3> value{};    // applied to the canonical parameter (x for float/int, xyz for vec3)
};

struct ControlDescriptor {
	std::string id;                  // stable control id (unique within a mode)
	std::string label;               // short HUD caption, <= 8 glyphs
	std::string paramId;             // canonical effect parameter this binds to
	ControlKind kind = ControlKind::Numeric;

	// Numeric
	double min = 0.0, max = 1.0, step = 0.1, defaultValue = 0.0;
	bool integral = false;           // integer-backed: stays integral
	int decimals = 1;                // display precision (0 for integral)
	const char* unit = "";           // optional display suffix, e.g. "PX"

	// Enum
	std::vector<Choice> choices;
	int defaultChoice = 0;
	bool wrap = true;                // matches the CRT convention (wrap)
};

struct ModeDescriptor {
	std::string effectId;            // canonical Shared Effects id
	std::string displayName;         // HUD label
	std::vector<ControlDescriptor> controls; // invariant: size() <= kMaxControls
};

inline ControlDescriptor numeric(std::string id, std::string label, std::string paramId, double min, double max, double step, double def, bool integral = false, const char* unit = "") {
	ControlDescriptor c;
	c.id = std::move(id); c.label = std::move(label); c.paramId = std::move(paramId);
	c.kind = ControlKind::Numeric; c.min = min; c.max = max; c.step = step; c.defaultValue = def;
	c.integral = integral; c.decimals = 0;
	if (!integral) { // fewest decimals that represent the step exactly (0.1 -> 1, 0.25 -> 2, 0.002 -> 3)
		for (int d = 0; d <= 4; ++d) { const double k = step * std::pow(10.0, d); if (std::fabs(k - std::round(k)) < 1e-6) { c.decimals = d; break; } c.decimals = d; }
	}
	c.unit = unit;
	return c;
}
inline ControlDescriptor choice(std::string id, std::string label, std::string paramId, std::vector<Choice> choices, int def = 0) {
	ControlDescriptor c;
	c.id = std::move(id); c.label = std::move(label); c.paramId = std::move(paramId);
	c.kind = ControlKind::Enum; c.choices = std::move(choices); c.defaultChoice = def;
	return c;
}

// Returns an empty string when the roster is valid, otherwise the first problem.
inline std::string validate(const std::vector<ModeDescriptor>& modes) {
	if (modes.empty()) return "no modes";
	for (const auto& m : modes) {
		if (m.effectId.empty() || m.displayName.empty()) return "mode without effectId/displayName";
		if (static_cast<int>(m.controls.size()) > kMaxControls) return m.effectId + ": more than " + std::to_string(kMaxControls) + " controls";
		for (const auto& c : m.controls) {
			if (c.id.empty() || c.paramId.empty() || c.label.empty()) return m.effectId + ": control without id/label/paramId";
			if (c.kind == ControlKind::Numeric && !(c.max > c.min && c.step > 0.0)) return m.effectId + "/" + c.id + ": bad numeric bounds";
			if (c.kind == ControlKind::Enum && (c.choices.size() < 2 || c.defaultChoice < 0 || c.defaultChoice >= static_cast<int>(c.choices.size()))) return m.effectId + "/" + c.id + ": bad choices";
		}
	}
	return {};
}

class ModeController {
public:
	// An invalid roster is REJECTED (no modes): callers must check ok().
	explicit ModeController(std::vector<ModeDescriptor> modes) {
		error_ = validate(modes);
		if (!error_.empty()) return;
		modes_ = std::move(modes);
		resetAll();
	}

	bool ok() const { return error_.empty(); }
	const std::string& error() const { return error_; }

	int modeCount() const { return static_cast<int>(modes_.size()); }
	int modeIndex() const { return mode_; }
	const ModeDescriptor& mode() const { return modes_[static_cast<size_t>(mode_)]; }
	const ModeDescriptor& modeAt(int i) const { return modes_[static_cast<size_t>(i)]; }

	// Next effect, wrapping last -> first. Returns the new index.
	int stepMode(int delta) {
		if (modes_.empty()) return 0;
		const int n = modeCount();
		mode_ = ((mode_ + delta) % n + n) % n;
		return mode_;
	}
	bool setModeByEffectId(const std::string& id) {
		for (int i = 0; i < modeCount(); ++i) if (modes_[static_cast<size_t>(i)].effectId == id) { mode_ = i; return true; }
		return false;
	}

	int controlCount() const { return modes_.empty() ? 0 : static_cast<int>(mode().controls.size()); }
	const ControlDescriptor& control(int i) const { return mode().controls[static_cast<size_t>(i)]; }

	// Left/Right on control `i`: dir = -1 / +1. Numeric clamps; Enum wraps (or clamps if !wrap).
	// Returns true if the value changed.
	bool stepControl(int i, int dir) {
		if (i < 0 || i >= controlCount() || (dir != 1 && dir != -1)) return false;
		const ControlDescriptor& c = control(i);
		double& v = value(mode_, i);
		const double before = v;
		if (c.kind == ControlKind::Numeric) {
			double next = v + dir * c.step;
			next = std::clamp(next, c.min, c.max);
			if (c.integral) next = std::round(next);
			// snap to the step grid anchored at min so repeated steps never accumulate float error
			else next = c.min + std::round((next - c.min) / c.step) * c.step;
			v = std::clamp(next, c.min, c.max);
		} else {
			const int n = static_cast<int>(c.choices.size());
			int idx = static_cast<int>(v) + dir;
			idx = c.wrap ? ((idx % n) + n) % n : std::clamp(idx, 0, n - 1);
			v = idx;
		}
		return v != before;
	}

	double rawValue(int i) const { return values_[static_cast<size_t>(mode_)][static_cast<size_t>(i)]; }
	int choiceIndex(int i) const { return static_cast<int>(rawValue(i)); }

	// Value to apply to the canonical parameter (x for float/int, xyz for vec3).
	std::array<float, 3> paramValue(int i) const {
		const ControlDescriptor& c = control(i);
		if (c.kind == ControlKind::Enum) return c.choices[static_cast<size_t>(choiceIndex(i))].value;
		return {static_cast<float>(rawValue(i)), 0.f, 0.f};
	}

	// Human-readable value for the HUD ("12 PX", "SAMPLED") — never an internal enum integer.
	std::string formattedValue(int i) const {
		const ControlDescriptor& c = control(i);
		if (c.kind == ControlKind::Enum) return c.choices[static_cast<size_t>(choiceIndex(i))].label;
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%.*f", c.decimals, rawValue(i));
		return std::string(buf) + (c.unit && c.unit[0] ? std::string(" ") + c.unit : std::string());
	}
	// Enum position text ("2/4"); empty for numeric controls.
	std::string positionText(int i) const {
		const ControlDescriptor& c = control(i);
		if (c.kind != ControlKind::Enum) return {};
		return std::to_string(choiceIndex(i) + 1) + "/" + std::to_string(c.choices.size());
	}

	// Restores every mode's controls to their defaults and returns to the first mode.
	void resetAll() {
		mode_ = 0;
		values_.assign(modes_.size(), {0.0, 0.0});
		for (size_t m = 0; m < modes_.size(); ++m) {
			for (size_t i = 0; i < modes_[m].controls.size(); ++i) {
				const auto& c = modes_[m].controls[i];
				values_[m][i] = c.kind == ControlKind::Enum ? double(c.defaultChoice) : std::clamp(c.defaultValue, c.min, c.max);
			}
		}
	}

private:
	double& value(int m, int i) { return values_[static_cast<size_t>(m)][static_cast<size_t>(i)]; }

	std::vector<ModeDescriptor> modes_;
	std::vector<std::array<double, kMaxControls>> values_;
	int mode_ = 0;
	std::string error_;
};

} // namespace scenefx
