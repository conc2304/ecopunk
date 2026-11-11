// Playable HUD Menu v0.1 — OF-free tests for ../src/HudFocusNavigator.

#include "../src/HudFocusNavigator.h"
#include "../src/HudRegionInteraction.h"
#include "../src/SceneFxModel.h"
#include "../src/RawRevealController.h"
#include "../../../shared/src/hud-compositor/HudFocusEmphasis.h"

#include <cmath>

#include <iostream>
#include <map>
#include <string>

namespace {
int g_total = 0;
int g_failures = 0;
void reportFailure(const char* file, int line, const std::string& expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}
} // namespace

#define HUD_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

namespace {

using A = HudControlAvailability;
const std::vector<std::string> kSlots = {"controls.scene_previous", "controls.scene_next", "controls.media_previous",
	"controls.media_next", "controls.reseed"};

struct World {
	std::map<std::string, A> state;
	HudFocusNavigator::AvailabilityFn fn() {
		return [this](const std::string& id) { auto it = state.find(id); return it == state.end() ? A::Hidden : it->second; };
	}
};

// Production today (Blob/Temporal at cold start): only media controls
// visible; PREV disabled at the start of media history.
World coldStart() {
	World w;
	w.state["controls.media_previous"] = A::Disabled;
	w.state["controls.media_next"] = A::Enabled;
	return w;
}

void test_no_focus_before_first_input() {
	World w = coldStart();
	HudFocusNavigator nav(kSlots);
	for (int i = 0; i < 5; ++i) nav.refresh(w.fn());
	HUD_CHECK(!nav.engaged() && !nav.focused().has_value() && !nav.activationTarget().has_value());
}

void test_first_step_engages_on_first_enabled() {
	World w = coldStart();
	HudFocusNavigator nav(kSlots);
	nav.refresh(w.fn());
	HUD_CHECK(nav.step(+1));
	HUD_CHECK(nav.engaged() && nav.focused() == std::string("controls.media_next")); // skips disabled PREV for the initial focus
	HUD_CHECK(nav.activationTarget() == std::string("controls.media_next"));
}

void test_first_step_falls_back_to_first_visible_when_none_enabled() {
	World w;
	w.state["controls.media_previous"] = A::Disabled;
	w.state["controls.media_next"] = A::Disabled;
	HudFocusNavigator nav(kSlots);
	nav.refresh(w.fn());
	HUD_CHECK(nav.step(-1) && nav.focused() == std::string("controls.media_previous"));
	HUD_CHECK(!nav.activationTarget().has_value());
}

void test_traversal_skips_hidden_and_wraps() {
	World w = coldStart();
	HudFocusNavigator nav(kSlots);
	nav.refresh(w.fn());
	nav.step(+1); // engage: media_next
	nav.step(+1); // wrap -> media_previous (scene_* and reseed hidden)
	HUD_CHECK(nav.focused() == std::string("controls.media_previous"));
	nav.step(+1);
	HUD_CHECK(nav.focused() == std::string("controls.media_next"));
	nav.step(-1);
	HUD_CHECK(nav.focused() == std::string("controls.media_previous"));
	nav.step(-1); // wrap backwards
	HUD_CHECK(nav.focused() == std::string("controls.media_next"));
	nav.step(+3); // encoder: three detents = +1 mod 2
	HUD_CHECK(nav.focused() == std::string("controls.media_previous"));
	nav.step(-4); // four detents back = no net change
	HUD_CHECK(nav.focused() == std::string("controls.media_previous"));
}

void test_full_set_order_when_all_visible() {
	World w;
	for (const auto& s : kSlots) w.state[s] = A::Enabled;
	HudFocusNavigator nav(kSlots);
	nav.refresh(w.fn());
	nav.step(+1);
	std::vector<std::string> seen;
	for (int i = 0; i < 5; ++i) { seen.push_back(*nav.focused()); nav.step(+1); }
	HUD_CHECK(seen == kSlots);
	HUD_CHECK(nav.focused() == kSlots.front()); // wrapped
}

void test_disabled_is_focusable_but_not_activatable() {
	World w = coldStart();
	HudFocusNavigator nav(kSlots);
	nav.refresh(w.fn());
	nav.step(+1);
	nav.step(-1); // -> media_previous (disabled)
	HUD_CHECK(nav.focused() == std::string("controls.media_previous"));
	HUD_CHECK(nav.focusedAvailability() == A::Disabled);
	HUD_CHECK(!nav.activationTarget().has_value());
	// Becomes enabled (e.g. media history grew): same focus, now activatable.
	w.state["controls.media_previous"] = A::Enabled;
	nav.refresh(w.fn());
	HUD_CHECK(nav.focused() == std::string("controls.media_previous"));
	HUD_CHECK(nav.activationTarget() == std::string("controls.media_previous"));
}

void test_focus_stable_across_refreshes() {
	World w = coldStart();
	HudFocusNavigator nav(kSlots);
	nav.refresh(w.fn());
	nav.step(+1);
	for (int i = 0; i < 1000; ++i) nav.refresh(w.fn());
	HUD_CHECK(nav.focused() == std::string("controls.media_next"));
	// A different control's availability changing does not move focus.
	w.state["controls.media_previous"] = A::Enabled;
	nav.refresh(w.fn());
	HUD_CHECK(nav.focused() == std::string("controls.media_next"));
}

void test_hidden_focus_moves_then_clears() {
	World w = coldStart();
	HudFocusNavigator nav(kSlots);
	nav.refresh(w.fn());
	nav.step(+1); // media_next
	w.state.erase("controls.media_next"); // hidden
	nav.refresh(w.fn());
	HUD_CHECK(nav.focused() == std::string("controls.media_previous")); // no later visible -> previous
	w.state.clear();
	nav.refresh(w.fn());
	HUD_CHECK(!nav.focused().has_value() && !nav.activationTarget().has_value());
	HUD_CHECK(!nav.step(+1)); // nothing visible
	// Controls return: the intended control (media_next, the user's last
	// explicit choice) is restored without any input; a step then moves on.
	w = coldStart();
	nav.refresh(w.fn());
	HUD_CHECK(nav.focused() == std::string("controls.media_next") && !nav.restorationPending());
	HUD_CHECK(nav.step(+1) && nav.focused() == std::string("controls.media_previous"));
}

void test_hidden_focus_prefers_next_visible() {
	World w;
	for (const auto& s : kSlots) w.state[s] = A::Enabled;
	HudFocusNavigator nav(kSlots);
	nav.refresh(w.fn());
	nav.step(+1); nav.step(+1); nav.step(+1); // scene_previous -> scene_next -> media_previous
	HUD_CHECK(nav.focused() == std::string("controls.media_previous"));
	w.state.erase("controls.media_previous");
	nav.refresh(w.fn());
	HUD_CHECK(nav.focused() == std::string("controls.media_next"));
}

// -- Focus continuity (intended target vs automatic fallback) ---------------
const std::vector<std::string> kRing = {"controls.scene_previous", "controls.scene_next", "controls.media_previous",
	"controls.media_next", "controls.reseed", "flexible.secondary_a"};
const std::string kPanel = "flexible.secondary_a";

World liveScene() {
	World w = coldStart();
	w.state[kPanel] = A::Enabled; // mode panel with NEXT MODE available
	return w;
}

HudFocusNavigator focusedOnPanel(World& w) {
	HudFocusNavigator nav(kRing);
	nav.refresh(w.fn());
	nav.step(+1); // engages media_next
	nav.step(+1); // -> mode panel
	return nav;
}

void test_restores_intended_focus_after_loading() {
	World w = liveScene();
	HudFocusNavigator nav = focusedOnPanel(w);
	HUD_CHECK(nav.focused() == kPanel && nav.intended() == kPanel);
	w.state.erase(kPanel); // scene Loading: panel hidden
	nav.refresh(w.fn());
	HUD_CHECK(nav.focused() == std::string("controls.media_next")); // automatic fallback (previous visible)
	HUD_CHECK(nav.intended() == kPanel && nav.restorationPending());
	nav.refresh(w.fn()); // still loading: fallback is stable
	HUD_CHECK(nav.focused() == std::string("controls.media_next"));
	w.state[kPanel] = A::Enabled; // incoming scene publishes its mode
	nav.refresh(w.fn());
	HUD_CHECK(nav.focused() == kPanel && !nav.restorationPending());
	HUD_CHECK(nav.activationTarget() == kPanel);
}

void test_restores_to_disabled_but_focusable_control() {
	World w = liveScene();
	HudFocusNavigator nav = focusedOnPanel(w);
	w.state.erase(kPanel);
	nav.refresh(w.fn());
	w.state[kPanel] = A::Disabled; // e.g. Blueprint: modes shown, NEXT MODE not advertised
	nav.refresh(w.fn());
	HUD_CHECK(nav.focused() == kPanel && nav.focusedAvailability() == A::Disabled);
	HUD_CHECK(!nav.activationTarget().has_value()); // still refuses activation
}

void test_explicit_navigation_cancels_restoration() {
	World w = liveScene();
	HudFocusNavigator nav = focusedOnPanel(w);
	w.state.erase(kPanel);
	nav.refresh(w.fn()); // fallback media_next
	nav.step(-1);        // user deliberately moves to media_previous
	HUD_CHECK(nav.focused() == std::string("controls.media_previous") && !nav.restorationPending());
	w.state[kPanel] = A::Enabled;
	nav.refresh(w.fn());
	HUD_CHECK(nav.focused() == std::string("controls.media_previous")); // no focus stealing
}

void test_activation_of_fallback_cancels_restoration() {
	World w = liveScene();
	HudFocusNavigator nav = focusedOnPanel(w);
	w.state.erase(kPanel);
	nav.refresh(w.fn());
	nav.confirmFocus(); // user pressed Enter on the fallback (media_next)
	HUD_CHECK(!nav.restorationPending());
	w.state[kPanel] = A::Enabled;
	nav.refresh(w.fn());
	HUD_CHECK(nav.focused() == std::string("controls.media_next"));
}

void test_absent_target_keeps_predictable_fallback() {
	World w = liveScene();
	HudFocusNavigator nav = focusedOnPanel(w);
	w.state.erase(kPanel); // a scene with no mode model: the panel never returns
	for (int i = 0; i < 5; ++i) nav.refresh(w.fn());
	HUD_CHECK(nav.focused() == std::string("controls.media_next") && nav.restorationPending());
	HUD_CHECK(nav.step(+1) && nav.focused() == std::string("controls.media_previous")); // traversal unaffected
	HUD_CHECK(!nav.restorationPending());
}

void test_whole_ring_hidden_then_restored() {
	World w = liveScene();
	HudFocusNavigator nav = focusedOnPanel(w);
	w.state.clear(); // every control hidden
	nav.refresh(w.fn());
	HUD_CHECK(!nav.focused().has_value() && nav.intended() == kPanel);
	w = liveScene();
	nav.refresh(w.fn());
	HUD_CHECK(nav.focused() == kPanel);
}

} // namespace

// ---- Generic interactive region: state machine + focus-stop integration ----

void test_region_interaction_state_machine() {
	HudRegionInteraction r(2);
	HUD_CHECK(!r.active() && r.row() == 0);
	HUD_CHECK(!r.selectRow(+1) && r.row() == 0);          // not active: row keys do nothing
	HUD_CHECK(r.enter() && r.active() && r.row() == 0);   // defaults to PIXELS
	HUD_CHECK(!r.selectRow(-1) && r.row() == 0);          // Up on the first row: clamp
	HUD_CHECK(r.selectRow(+1) && r.row() == 1);           // Down -> PALETTE
	HUD_CHECK(!r.selectRow(+1) && r.row() == 1);          // Down on the last row: clamp
	HUD_CHECK(r.exit() && !r.active() && r.row() == 1);   // exit keeps the row
	HUD_CHECK(r.enter() && r.row() == 1);                 // re-entry resumes the prior row
	r.forceExit();
	HUD_CHECK(!r.active() && r.row() == 0);               // scene change / unavailable: no stale state
}

void test_region_interaction_encoder_never_traps() {
	HudRegionInteraction r(2);
	r.enter();
	HUD_CHECK(!r.advanceRowOrExit() && r.active() && r.row() == 1); // click: PIXELS -> PALETTE
	HUD_CHECK(r.advanceRowOrExit() && !r.active());                 // click: leaves
	HUD_CHECK(!r.advanceRowOrExit());                               // idle click does nothing here
}

void test_interactive_focus_stop_is_single_and_hidden_when_unavailable() {
	using A = HudControlAvailability;
	const std::string kStop = "interactive.region";
	HudFocusNavigator nav({"controls.menu", kStop, "orbit.a"});
	bool available = true;
	auto avail = [&](const std::string& id) { return id == kStop ? (available ? A::Enabled : A::Hidden) : A::Enabled; };
	nav.refresh(avail);
	nav.step(+1); nav.step(+1); // engage, then next
	HUD_CHECK(nav.focused() == kStop && nav.activationTarget() == kStop); // exactly one stop for the whole region
	nav.step(+1);
	HUD_CHECK(nav.focused() == std::string("orbit.a"));                  // rows are NOT separate outer stops
	nav.step(-1);
	HUD_CHECK(nav.focused() == kStop);
	available = false;                                                    // option port gone / scene changed
	nav.refresh(avail);
	HUD_CHECK(nav.focused() != kStop);                                    // never a stale focus on an unavailable region
	nav.step(+1); nav.step(+1); nav.step(+1);
	HUD_CHECK(nav.focused() != kStop);                                    // absent from traversal
}

// ---- Shared Effects Gallery: descriptor / controller ----

scenefx::ModeDescriptor fxMode(const std::string& id, std::vector<scenefx::ControlDescriptor> controls) {
	scenefx::ModeDescriptor m; m.effectId = id; m.displayName = id; m.controls = std::move(controls); return m;
}
std::vector<scenefx::ModeDescriptor> sampleRoster() {
	using namespace scenefx;
	return {
		fxMode("zero", {}),
		fxMode("one", {numeric("a", "AMOUNT", "alpha", 0.0, 1.0, 0.1, 0.0)}),
		fxMode("two", {numeric("px", "CELL", "cellSize", 6.0, 24.0, 2.0, 6.0, true, "PX"),
		               choice("col", "COLOR", "asciiColorMode", {Choice{"SAMPLED", {0, 0, 0}}, Choice{"B&W", {1, 0, 0}}, Choice{"MONO", {2, 0, 0}}}, 0)}),
	};
}

void test_fx_rejects_more_than_two_controls() {
	using namespace scenefx;
	auto roster = sampleRoster();
	roster[2].controls.push_back(numeric("c", "X", "p", 0, 1, 0.1, 0));
	HUD_CHECK(!validate(roster).empty());
	ModeController bad(roster);
	HUD_CHECK(!bad.ok() && bad.modeCount() == 0);
	ModeController good(sampleRoster());
	HUD_CHECK(good.ok() && good.modeCount() == 3);
}

void test_fx_numeric_and_enum_steps() {
	using namespace scenefx;
	ModeController c(sampleRoster());
	c.stepMode(+1); // "one": AMOUNT 0..1 step 0.1, default 0
	HUD_CHECK(c.controlCount() == 1 && c.formattedValue(0) == "0.0");
	HUD_CHECK(!c.stepControl(0, -1) && c.formattedValue(0) == "0.0");       // clamps at min
	for (int i = 0; i < 3; ++i) c.stepControl(0, +1);
	HUD_CHECK(std::fabs(c.rawValue(0) - 0.3) < 1e-9 && c.formattedValue(0) == "0.3"); // no accumulated float error
	c.stepControl(0, -1);
	HUD_CHECK(c.formattedValue(0) == "0.2");                                  // decrement
	for (int i = 0; i < 20; ++i) c.stepControl(0, +1);
	HUD_CHECK(c.formattedValue(0) == "1.0" && !c.stepControl(0, +1));         // clamps at max
	c.stepMode(+1); // "two"
	HUD_CHECK(c.controlCount() == 2);
	HUD_CHECK(c.formattedValue(0) == "6 PX");                                  // integral control stays integral
	c.stepControl(0, +1);
	HUD_CHECK(c.formattedValue(0) == "8 PX" && c.rawValue(0) == 8.0);
	HUD_CHECK(c.formattedValue(1) == "SAMPLED" && c.positionText(1) == "1/3"); // human-readable, never an integer
	c.stepControl(1, +1);
	HUD_CHECK(c.formattedValue(1) == "B&W" && c.paramValue(1)[0] == 1.0f);
	c.stepControl(1, +1); c.stepControl(1, +1);
	HUD_CHECK(c.formattedValue(1) == "SAMPLED");                               // enum wraps forward
	c.stepControl(1, -1);
	HUD_CHECK(c.formattedValue(1) == "MONO");                                  // enum wraps backward
	HUD_CHECK(!c.stepControl(2, +1) && !c.stepControl(-1, +1) && !c.stepControl(0, 0)); // invalid axis/step refused
}

void test_fx_enum_clamp_policy() {
	using namespace scenefx;
	auto m = fxMode("e", {choice("c", "C", "p", {Choice{"A", {0, 0, 0}}, Choice{"B", {1, 0, 0}}})});
	m.controls[0].wrap = false;
	ModeController c({m});
	HUD_CHECK(!c.stepControl(0, -1) && c.formattedValue(0) == "A");
	c.stepControl(0, +1);
	HUD_CHECK(!c.stepControl(0, +1) && c.formattedValue(0) == "B");           // clamped, no wrap
}

void test_fx_mode_switching() {
	using namespace scenefx;
	ModeController c(sampleRoster());
	HUD_CHECK(c.mode().effectId == "zero" && c.controlCount() == 0);
	c.stepMode(+1); HUD_CHECK(c.mode().effectId == "one" && c.controlCount() == 1);
	c.stepControl(0, +1); c.stepControl(0, +1); // AMOUNT 0.2
	c.stepMode(+1); HUD_CHECK(c.mode().effectId == "two" && c.controlCount() == 2);
	c.stepMode(+1); HUD_CHECK(c.mode().effectId == "zero" && c.modeIndex() == 0); // wraps last -> first
	c.stepMode(-1); HUD_CHECK(c.mode().effectId == "two");
	c.stepMode(+1); c.stepMode(+1);
	HUD_CHECK(c.mode().effectId == "one" && c.formattedValue(0) == "0.2");     // values persist per mode
	c.resetAll();
	HUD_CHECK(c.modeIndex() == 0 && c.mode().effectId == "zero");
	c.stepMode(+1);
	HUD_CHECK(c.formattedValue(0) == "0.0");                                   // reset restores defaults
}

// ---- Parameter navigation (same state machine for CRT and the gallery) ----

void checkNavigation(int rows) {
	HudRegionInteraction r(rows);
	HUD_CHECK(r.enter() && r.active() && r.row() == 0);
	for (int i = 0; i < rows - 1; ++i) { HUD_CHECK(!r.moveDownOrExit() && r.active() && r.row() == i + 1); } // Down walks the rows
	HUD_CHECK(r.row() == rows - 1);
	for (int i = rows - 1; i > 0; --i) { HUD_CHECK(r.selectRow(-1) && r.row() == i - 1); }                    // Up walks back
	HUD_CHECK(!r.selectRow(-1) && r.row() == 0 && r.active());                                                // Up on the first row stays
	for (int i = 0; i < rows - 1; ++i) r.moveDownOrExit();
	HUD_CHECK(r.moveDownOrExit() && !r.active());                                                             // Down on the LAST row exits
}

void test_navigation_two_rows_gallery_and_crt() { checkNavigation(2); }
void test_navigation_one_row_gallery_and_crt() { checkNavigation(1); }

void test_navigation_zero_rows_never_traps() {
	HudRegionInteraction r(0);
	HUD_CHECK(!r.enter() && !r.active());                    // nothing to edit -> cannot be entered
	HudRegionInteraction g(2);
	g.enter(); g.moveDownOrExit();                           // on row 1 of 2
	g.configure(1);                                          // mode change: next effect has ONE control
	HUD_CHECK(g.active() && g.row() == 0);                   // stale row index repaired, never kept against a one-item list
	g.configure(0);                                          // next effect has NO controls
	HUD_CHECK(!g.active() && g.row() == 0 && !g.enter());    // exits, no trap
}

// ---- HUD focus presentation ----

void test_focus_emphasis() {
	using namespace hudpresent;
	HUD_CHECK(navEmphasis(true) == 0.5f && navEmphasis(false) == 1.0f);       // nested focus dims nav to 50 %, restore -> 100 %
	HUD_CHECK(scaleAlpha(220, navEmphasis(true)) == 110);
	HUD_CHECK(scaleAlpha(40, navEmphasis(true)) == 20 && scaleAlpha(40, 0.5f) < scaleAlpha(220, 0.5f)); // multiplies: an already-dim (disabled) control stays dimmer
	HUD_CHECK(scaleAlpha(255, navEmphasis(false)) == 255);
	float lo = 9.f, hi = -9.f;
	for (int i = 0; i < 2000; ++i) { const float p = focusPulse(i * 0.01f); lo = std::min(lo, p); hi = std::max(hi, p); }
	HUD_CHECK(lo >= 1.0f - kFocusPulseDepth - 1e-4f && hi <= 1.0f + 1e-4f && hi - lo > 0.25f); // subtle, bounded, actually pulses
	HUD_CHECK(std::fabs(focusPulse(1.234f) - focusPulse(1.234f)) < 1e-9f);                      // pure function of time (frame-rate independent)
	const float period = 1.0f / kFocusPulseHz;
	HUD_CHECK(std::fabs(focusPulse(0.37f) - focusPulse(0.37f + period)) < 1e-3f);
}

// ---- Raw reveal controller (scene-local, effect-independent) ----

void test_reveal_hidden_dwell_is_mask_free() {
	using namespace scenefx;
	RawRevealController r;
	HUD_CHECK(r.phase() == RevealPhase::HiddenDwell && r.width() == 0.f && !r.visible()); // starts hidden: no flash at activation
	bool leaked = false;
	while (r.phase() == RevealPhase::HiddenDwell) {
		if (r.width() != 0.f || !revealSpanPx(r.left(), r.right(), 1280).empty()) leaked = true;
		r.update(1.f / 60.f);
	}
	HUD_CHECK(!leaked);                                      // exactly zero width and an EMPTY pixel span for the whole dwell
	RawRevealController z;
	HUD_CHECK(revealSpanPx(z.left(), z.right(), 1280).empty() && revealSpanPx(0.4f, 0.4f, 1280).empty());
	HUD_CHECK(revealSpanPx(0.5f, 0.5f + 0.0003f, 1280).empty()); // sub-pixel width draws nothing (no one-pixel raw line)
}

void test_reveal_phases_repeat_and_geometry_bounds() {
	using namespace scenefx;
	RawRevealController r;
	std::vector<RevealPhase> seq;
	float maxW = 0.f, dwellMin = 1e9f, curDwell = 0.f;
	bool boundsOk = true, ordered = true;
	RevealPhase prev = r.phase();
	seq.push_back(prev);
	for (int i = 0; i < 60 * 400 && r.cycleIndex() < 4; ++i) {
		r.update(1.f / 60.f);
		const float w = r.width(), l = r.left(), rt = r.right();
		if (l < -1e-6f || rt > 1.f + 1e-6f || rt < l - 1e-6f || w > 0.40f + 1e-5f) boundsOk = false; // whole width inside the scene
		const RevealSpan sp = revealSpanPx(l, rt, 1280);
		if (!sp.empty() && (sp.x0 < 0 || sp.x1 > 1280)) boundsOk = false;
		maxW = std::max(maxW, w);
		if (r.phase() != prev) {
			if (prev == RevealPhase::HiddenDwell) { dwellMin = std::min(dwellMin, curDwell); }
			curDwell = 0.f;
			prev = r.phase(); seq.push_back(prev);
		}
		if (r.phase() == RevealPhase::HiddenDwell) curDwell += 1.f / 60.f;
	}
	HUD_CHECK(boundsOk);
	HUD_CHECK(maxW >= 0.20f - 1e-4f && maxW <= 0.40f + 1e-5f);                  // width envelope 20..40 %
	HUD_CHECK(r.cycleIndex() >= 4);                                               // repeats
	for (size_t i = 1; i < seq.size(); ++i) {                                    // Hidden -> Grow -> Drift -> Retract -> Hidden ...
		const int a = int(seq[i - 1]), b = int(seq[i]);
		if (b != (a + 1) % 4) ordered = false;
	}
	HUD_CHECK(ordered && seq.size() >= 13);
}

void test_reveal_deterministic_and_time_step_independent() {
	using namespace scenefx;
	RawRevealController a(7u), b(7u), c(8u);
	for (int i = 0; i < 4000; ++i) { a.update(1.f / 60.f); b.update(1.f / 60.f); c.update(1.f / 60.f); }
	HUD_CHECK(a.phase() == b.phase() && a.width() == b.width() && a.left() == b.left());  // same seed + same steps => identical
	bool differ = false;
	RawRevealController d(7u), e(8u);
	for (int i = 0; i < 6000 && !differ; ++i) { d.update(1.f / 60.f); e.update(1.f / 60.f); differ = d.left() != e.left() || d.width() != e.width() || d.phase() != e.phase(); }
	HUD_CHECK(differ);                                                                   // different seed => different sequence
	// Time-step independence: identical total time, 0.0625 s steps vs 0.03125 s steps (exact binary fractions).
	for (double total : {30.0, 47.0, 64.0}) {
		RawRevealController x, y;
		for (double t = 0; t < total - 1e-9; t += 0.0625) x.update(0.0625f);
		for (double t = 0; t < total - 1e-9; t += 0.03125) y.update(0.03125f);
		HUD_CHECK(x.phase() == y.phase() && std::fabs(x.width() - y.width()) < 1e-4f && std::fabs(x.left() - y.left()) < 1e-4f);
	}
	RawRevealController s;
	s.update(30.f);                                                                      // pathological dt spike
	HUD_CHECK(s.phase() == RevealPhase::HiddenDwell && s.cycleIndex() == 0);              // clamped: no jump between states
}

void test_reveal_reset_returns_to_hidden() {
	using namespace scenefx;
	RawRevealController r;
	for (int i = 0; i < 60 * 14 && r.phase() != RevealPhase::Drift; ++i) r.update(1.f / 20.f);
	HUD_CHECK(r.visible());
	r.reset();                                                                            // effect-mode change / activation
	HUD_CHECK(r.phase() == RevealPhase::HiddenDwell && r.width() == 0.f && revealSpanPx(r.left(), r.right(), 1280).empty());
}

int main() {
	test_no_focus_before_first_input();
	test_first_step_engages_on_first_enabled();
	test_first_step_falls_back_to_first_visible_when_none_enabled();
	test_traversal_skips_hidden_and_wraps();
	test_full_set_order_when_all_visible();
	test_disabled_is_focusable_but_not_activatable();
	test_focus_stable_across_refreshes();
	test_hidden_focus_moves_then_clears();
	test_hidden_focus_prefers_next_visible();
	test_restores_intended_focus_after_loading();
	test_restores_to_disabled_but_focusable_control();
	test_explicit_navigation_cancels_restoration();
	test_activation_of_fallback_cancels_restoration();
	test_absent_target_keeps_predictable_fallback();
	test_whole_ring_hidden_then_restored();
	test_region_interaction_state_machine();
	test_region_interaction_encoder_never_traps();
	test_interactive_focus_stop_is_single_and_hidden_when_unavailable();
	test_fx_rejects_more_than_two_controls();
	test_fx_numeric_and_enum_steps();
	test_fx_enum_clamp_policy();
	test_fx_mode_switching();
	test_navigation_two_rows_gallery_and_crt();
	test_navigation_one_row_gallery_and_crt();
	test_navigation_zero_rows_never_traps();
	test_focus_emphasis();
	test_reveal_hidden_dwell_is_mask_free();
	test_reveal_phases_repeat_and_geometry_bounds();
	test_reveal_deterministic_and_time_step_independent();
	test_reveal_reset_returns_to_hidden();

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed.\n";
	if (g_failures > 0) {
		std::cerr << g_failures << " FAILURE(S)\n";
		return 1;
	}
	return 0;
}
