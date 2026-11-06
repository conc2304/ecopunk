// ShellTheme -- seed -> token derivation and contrast fitting (OF-free).
#include "../hud-compositor/ShellTheme.h"

#include <iostream>
#include <string>

namespace {
int g_total = 0, g_failures = 0;
void check(bool c, const std::string& what) {
	g_total++;
	if (!c) { g_failures++; std::cerr << "FAIL: " << what << "\n"; }
}
} // namespace

int main() {
	using namespace shelltheme;

	// Contrast maths.
	check(std::fabs(contrastRatio({0, 0, 0}, {255, 255, 255}) - 21.0f) < 0.01f, "black/white = 21:1");
	check(std::fabs(contrastRatio({120, 120, 120}, {120, 120, 120}) - 1.0f) < 1e-4f, "same colour = 1:1");
	check(parseHex("#79C356") && parseHex("79c356")->g == 0xC3 && !parseHex("#12345") && !parseHex("#GG0000"), "hex parsing");

	// fitContrast leaves a passing colour alone and repairs a failing one.
	const Rgb dark{32, 30, 25};
	check(fitContrast({255, 255, 255, 255}, dark, Mode::Dark, 4.5f).r == 255, "passing colour untouched");
	const Rgba weak = fitContrast({60, 60, 60, 255}, dark, Mode::Dark, 4.5f);
	check(contrastOn(weak, dark) >= 4.5f && weak.r > 60, "dark mode lightens a weak colour");
	const Rgb cream{243, 228, 201};
	const Rgba pale = fitContrast({230, 220, 190, 255}, cream, Mode::Light, 4.5f);
	check(contrastOn(pale, cream) >= 4.5f && pale.r < 230, "light mode darkens a weak colour");

	// The shipped shell palettes (keep in sync with assets/hud/themes/shell-palettes.json).
	struct Case { const char* name; Mode mode; const char* primary; const char* secondary; const char* surface; };
	const Case cases[] = {
		{"dark-moss-experimental", Mode::Dark, "#79C356", "#D2B66A", "#201E19"},
		{"abyssal-bioluminescence", Mode::Dark, "#4FD1C5", "#8FB4FF", "#181C20"},
		{"plywood-marble", Mode::Light, "#7A4B12", "#3F6B3A", "#F3E4C9"},
	};
	for (const Case& c : cases) {
		Seeds s;
		s.mode = c.mode;
		s.primary = *parseHex(c.primary);
		s.secondary = parseHex(c.secondary);
		s.surface = *parseHex(c.surface);
		const Tokens t = derive(s);
		const std::string n = c.name;
		for (const auto& [label, tok] : {std::pair<const char*, Rgba>{"primary", t.primary}, {"secondary", t.secondary}, {"accent", t.accent}, {"caption", t.caption}, {"warning", t.warning}})
			check(contrastOn(tok, s.surface) >= kTextContrast - 0.05f, n + ": " + label + " >= 4.5:1");
		for (const auto& [label, tok] : {std::pair<const char*, Rgba>{"muted", t.muted}, {"dataA", t.dataA}, {"dataB", t.dataB}})
			check(contrastOn(tok, s.surface) >= kGraphicContrast - 0.05f, n + ": " + label + " >= 3:1");
		check(t.muted.a < t.primary.a, n + ": muted stays more translucent than primary");
		check(t.border.a == 255 && t.border.r == t.accent.r, n + ": border follows accent");
	}

	// Distinct skins get distinct palettes.
	Seeds a, b;
	a.primary = *parseHex("#79C356"); b.primary = *parseHex("#4FD1C5");
	check(derive(a).primary.g != derive(b).primary.g || derive(a).primary.r != derive(b).primary.r, "different seeds -> different primary");

	// Overrides are exact and unfitted; other tokens still fit.
	Seeds o;
	o.surface = *parseHex("#201E19");
	o.overrides["warning"] = Rgb{40, 40, 40}; // deliberately unreadable
	const Tokens ot = derive(o);
	check(ot.warning.r == 40 && ot.warning.g == 40, "override applied exactly");
	check(contrastOn(ot.primary, o.surface) >= kTextContrast - 0.05f, "other tokens still fitted");

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
