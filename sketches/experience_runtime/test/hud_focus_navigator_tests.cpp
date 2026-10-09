// Playable HUD Menu v0.1 — OF-free tests for ../src/HudFocusNavigator.

#include "../src/HudFocusNavigator.h"

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
	// Controls return: first step re-engages.
	w = coldStart();
	nav.refresh(w.fn());
	HUD_CHECK(nav.step(+1) && nav.focused() == std::string("controls.media_next"));
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

} // namespace

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

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed.\n";
	if (g_failures > 0) {
		std::cerr << g_failures << " FAILURE(S)\n";
		return 1;
	}
	return 0;
}
