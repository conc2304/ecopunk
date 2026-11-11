#pragma once

// CrtRegistries.h - OpenFrameworks-FREE registries and selection model for the
// CRT Phosphor display axes (composition MODE, STRUCTURE, COLOR MAPPING,
// BEHAVIOR). Shared by the standalone sketch, the runtime CrtPhosphorScene and
// the unit tests (which link no GL). Stable string IDs are independent of enum
// order; display order is the registry order below.

#include <array>
#include <string>
#include <vector>

namespace crt {

// ------------------------------------------------------------ mode registry
// A MODE is an image/signal construction algorithm (independent of LOOK =
// Params presets and PALETTE = Palette). IDs are stable strings; order below
// is display order only and nothing persists an index.
enum class Mode { PhosphorPortrait, RasterExcavation, PhosphorMemory, SignalColumns, ContourEmission, SignalCollapse };

struct ModeInfo {
	Mode mode;
	const char* id;
	const char* name;
	bool implemented;
};

inline const std::array<ModeInfo, 6>& modeRegistry() {
	static const std::array<ModeInfo, 6> r = {{
		{ Mode::PhosphorPortrait,  "phosphor_portrait",  "Phosphor Portrait",  true },
		{ Mode::RasterExcavation,  "raster_excavation",  "Raster Excavation",  true },
		{ Mode::PhosphorMemory,    "phosphor_memory",    "Phosphor Memory",    true },
		{ Mode::SignalColumns,     "signal_columns",     "Signal Columns",     true },
		{ Mode::ContourEmission,   "contour_emission",   "Contour Emission (planned)", false },
		{ Mode::SignalCollapse,    "signal_collapse",    "Signal Collapse (planned)",  false },
	}};
	return r;
}
inline const ModeInfo& modeInfo(Mode m) { for (auto& i : modeRegistry()) if (i.mode == m) return i; return modeRegistry()[0]; }
inline const ModeInfo* modeById(const std::string& id) { for (auto& i : modeRegistry()) if (id == i.id) return &i; return nullptr; }
// Next/previous IMPLEMENTED mode (planned ones are never selectable by stepping).
inline Mode stepMode(Mode m, int dir) {
	const auto& r = modeRegistry();
	int idx = 0;
	for (int i = 0; i < (int)r.size(); i++) if (r[i].mode == m) idx = i;
	for (int k = 0; k < (int)r.size(); k++) {
		idx = ((idx + dir) % (int)r.size() + (int)r.size()) % (int)r.size();
		if (r[idx].implemented) return r[idx].mode;
	}
	return m;
}


// ================================================== DISPLAY AXES (orthogonal)
// Composition MODE (above) x STRUCTURE x COLOR MAPPING x BEHAVIOR (x look,
// theme palette, dither, seed). Nothing here multiplies into mode IDs.
enum class Structure { Original, RgbTriad, ApertureGrille, RoundMatrix, AsciiMatrix, VectorBeam };
struct StructureInfo { Structure s; const char* id; const char* name; };
inline const std::array<StructureInfo, 6>& structureRegistry() {
	static const std::array<StructureInfo, 6> r = {{
		{ Structure::Original,       "original_phosphor", "Original Phosphor" },
		{ Structure::RgbTriad,       "rgb_triad",         "RGB Phosphor Triads" },
		{ Structure::ApertureGrille, "aperture_grille",   "Aperture Grille" },
		{ Structure::RoundMatrix,    "round_matrix",      "Round Phosphor Matrix" },
		{ Structure::AsciiMatrix,    "ascii_matrix",      "ASCII Character Matrix" },
		{ Structure::VectorBeam,     "vector_beam",       "Vector Beam Display" },
	}};
	return r;
}
inline const StructureInfo& structureInfo(Structure s) { for (auto& i : structureRegistry()) if (i.s == s) return i; return structureRegistry()[0]; }
inline const StructureInfo* structureById(const std::string& id) { for (auto& i : structureRegistry()) if (id == i.id) return &i; return nullptr; }
inline Structure stepStructure(Structure s, int dir) {
	int n = (int)structureRegistry().size(), idx = (int)s;
	return structureRegistry()[((idx + dir) % n + n) % n].s;
}

enum class ColorMap { ClassicMono, ThemePrimarySecondary, SourceRgb, Ega16, Sunset, Biolum, Thermal, FourTone, DualPhosphor };
struct ColorMapInfo { ColorMap c; const char* id; const char* name; };
inline const std::array<ColorMapInfo, 9>& colorMapRegistry() {
	static const std::array<ColorMapInfo, 9> r = {{
		{ ColorMap::ClassicMono,           "classic_mono",   "Classic Mono (green)" },
		{ ColorMap::ThemePrimarySecondary, "theme_primary_secondary", "Theme Primary/Secondary" },
		{ ColorMap::SourceRgb,             "source_rgb",     "Source RGB" },
		{ ColorMap::Ega16,                 "ega16",          "EGA 16-color (RGB quantised)" },
		{ ColorMap::Sunset,                "sunset",         "Sunset Spectrum" },
		{ ColorMap::Biolum,                "biolum",         "Bioluminescent Spectrum" },
		{ ColorMap::Thermal,               "thermal",        "Thermal / False Color" },
		{ ColorMap::FourTone,              "four_tone",      "Four-tone LCD" },
		{ ColorMap::DualPhosphor,          "dual_phosphor",  "Dual Phosphor" },
	}};
	return r;
}
inline const ColorMapInfo& colorMapInfo(ColorMap c) { for (auto& i : colorMapRegistry()) if (i.c == c) return i; return colorMapRegistry()[0]; }
inline const ColorMapInfo* colorMapById(const std::string& id) { for (auto& i : colorMapRegistry()) if (id == i.id) return &i; return nullptr; }
inline ColorMap stepColorMap(ColorMap c, int dir) {
	int n = (int)colorMapRegistry().size(), idx = (int)c;
	return colorMapRegistry()[((idx + dir) % n + n) % n].c;
}

enum class Behavior { None, AdaptiveMosaic, FloatingPhosphors, InterlacedColorFields };
struct BehaviorInfo { Behavior b; const char* id; const char* name; };
inline const std::array<BehaviorInfo, 4>& behaviorRegistry() {
	static const std::array<BehaviorInfo, 4> r = {{
		{ Behavior::None,                 "none",                    "None" },
		{ Behavior::AdaptiveMosaic,       "adaptive_mosaic",         "Adaptive Pixel Mosaic" },
		{ Behavior::FloatingPhosphors,    "floating_phosphors",      "Floating Phosphors" },
		{ Behavior::InterlacedColorFields,"interlaced_color_fields", "Interlaced Color Fields" },
	}};
	return r;
}
inline const BehaviorInfo& behaviorInfo(Behavior b) { for (auto& i : behaviorRegistry()) if (i.b == b) return i; return behaviorRegistry()[0]; }
inline const BehaviorInfo* behaviorById(const std::string& id) { for (auto& i : behaviorRegistry()) if (id == i.id) return &i; return nullptr; }
inline Behavior stepBehavior(Behavior b, int dir) {
	int n = (int)behaviorRegistry().size(), idx = (int)b;
	return behaviorRegistry()[((idx + dir) % n + n) % n].b;
}

// Explicit compatibility model. nullptr = supported; otherwise the visible reason.
// Unsupported combinations are NEVER rendered as an alternate look: the sketch
// rejects the change (or resets the behavior to None) and shows this text.
inline const char* behaviorBlockReason(Structure s, Behavior b) {
	if (b == Behavior::None) return nullptr;
	switch (s) {
	case Structure::Original:
	case Structure::RgbTriad:
	case Structure::ApertureGrille:
		if (b == Behavior::InterlacedColorFields) return nullptr;
		return b == Behavior::AdaptiveMosaic ? "adaptive_mosaic needs square/round cells (round_matrix only)"
		                                      : "floating_phosphors needs discrete round emitters (round_matrix only)";
	case Structure::RoundMatrix: return nullptr; // all three
	case Structure::AsciiMatrix: return "ASCII cells are glyphs: no behavior adapter (use none)";
	case Structure::VectorBeam:  return "vector_beam is not a cell structure: no behavior adapter (use none)";
	}
	return nullptr;
}


// ---------------------------------------------------------------- HUD labels
// Visitor-facing, ASCII-only, <= 24 glyphs (fits the HUD's 8 px bitmap font in
// a 401 px text-safe row with the caption, position and gaps; see the HUD
// selector variant). IDs above stay the stable identity; these are display text.
inline const char* hudLabel(Mode m) {
	switch (m) {
	case Mode::PhosphorPortrait: return "PHOSPHOR PORTRAIT";
	case Mode::RasterExcavation: return "RASTER EXCAVATION";
	case Mode::PhosphorMemory:   return "PHOSPHOR MEMORY";
	case Mode::SignalColumns:    return "SIGNAL COLUMNS";
	case Mode::ContourEmission:  return "CONTOUR EMISSION";
	case Mode::SignalCollapse:   return "SIGNAL COLLAPSE";
	}
	return "";
}
inline const char* hudLabel(Structure s) {
	switch (s) {
	case Structure::Original:       return "ORIGINAL PHOSPHOR";
	case Structure::RgbTriad:       return "RGB PHOSPHOR TRIADS";
	case Structure::ApertureGrille: return "APERTURE GRILLE";
	case Structure::RoundMatrix:    return "ROUND PHOSPHOR MATRIX";
	case Structure::AsciiMatrix:    return "ASCII CHARACTER MATRIX";
	case Structure::VectorBeam:     return "VECTOR BEAM DISPLAY";
	}
	return "";
}
inline const char* hudLabel(ColorMap c) {
	switch (c) {
	case ColorMap::ClassicMono:           return "CLASSIC MONO";
	case ColorMap::ThemePrimarySecondary: return "THEME PRIMARY/SECONDARY";
	case ColorMap::SourceRgb:             return "SOURCE COLOR";
	case ColorMap::Ega16:                 return "EGA 16-COLOR";
	case ColorMap::Sunset:                return "SUNSET SPECTRUM";
	case ColorMap::Biolum:                return "BIOLUMINESCENT SPECTRUM";
	case ColorMap::Thermal:               return "THERMAL FALSE COLOR";
	case ColorMap::FourTone:              return "FOUR-TONE LCD";
	case ColorMap::DualPhosphor:          return "DUAL PHOSPHOR";
	}
	return "";
}

// Runtime availability of a colour mapping. theme_primary_secondary needs a
// scene-accessible theme palette; no such API exists (HUD theme is private to
// the HUD), so in the RUNTIME scene it would just duplicate classic_mono. It is
// therefore HIDDEN (not counted, not selectable), never faked.
inline bool runtimeAvailable(ColorMap c) { return c != ColorMap::ThemePrimarySecondary; }

// ------------------------------------------------------------- selection model
// Three independent axes, each a wrapping list of currently SELECTABLE entries.
// Counts/positions are accurate to what is actually available: planned modes and
// theme-less palettes are not in the lists. Changing one axis never touches the
// others. Positions are 1-based for display; indices are 0-based into the list.
enum class Axis { Composition, Structure, Palette };

class Selection {
public:
	Selection() {
		for (auto& i : modeRegistry()) if (i.implemented) modes_.push_back(i.mode);
		for (auto& i : structureRegistry()) structures_.push_back(i.s);
		for (auto& i : colorMapRegistry()) if (runtimeAvailable(i.c)) palettes_.push_back(i.c);
	}
	int count(Axis a) const { return a == Axis::Composition ? (int)modes_.size() : a == Axis::Structure ? (int)structures_.size() : (int)palettes_.size(); }
	int index(Axis a) const { return a == Axis::Composition ? mi_ : a == Axis::Structure ? si_ : pi_; }
	int position(Axis a) const { return index(a) + 1; }
	// Wraps in both directions; |delta| may exceed the count.
	void step(Axis a, int delta) {
		int n = count(a);
		int& i = a == Axis::Composition ? mi_ : a == Axis::Structure ? si_ : pi_;
		i = ((i + delta) % n + n) % n;
	}
	Mode mode() const { return modes_[mi_]; }
	Structure structure() const { return structures_[si_]; }
	ColorMap colorMap() const { return palettes_[pi_]; }
	const char* id(Axis a) const {
		return a == Axis::Composition ? modeInfo(mode()).id : a == Axis::Structure ? structureInfo(structure()).id : colorMapInfo(colorMap()).id;
	}
	const char* label(Axis a) const {
		return a == Axis::Composition ? hudLabel(mode()) : a == Axis::Structure ? hudLabel(structure()) : hudLabel(colorMap());
	}
	// Select by stable id; false (state unchanged) if unknown or not selectable.
	bool setById(Axis a, const std::string& id) {
		if (a == Axis::Composition) { for (size_t k = 0; k < modes_.size(); k++) if (id == modeInfo(modes_[k]).id) { mi_ = (int)k; return true; } }
		else if (a == Axis::Structure) { for (size_t k = 0; k < structures_.size(); k++) if (id == structureInfo(structures_[k]).id) { si_ = (int)k; return true; } }
		else { for (size_t k = 0; k < palettes_.size(); k++) if (id == colorMapInfo(palettes_[k]).id) { pi_ = (int)k; return true; } }
		return false;
	}
	void reset() { mi_ = si_ = pi_ = 0; }
	std::vector<std::string> ids(Axis a) const {
		std::vector<std::string> v;
		if (a == Axis::Composition) for (auto m : modes_) v.push_back(modeInfo(m).id);
		else if (a == Axis::Structure) for (auto s : structures_) v.push_back(structureInfo(s).id);
		else for (auto c : palettes_) v.push_back(colorMapInfo(c).id);
		return v;
	}
private:
	std::vector<Mode> modes_;
	std::vector<Structure> structures_;
	std::vector<ColorMap> palettes_;
	int mi_ = 0, si_ = 0, pi_ = 0; // defaults: phosphor_portrait, original_phosphor, classic_mono
};

} // namespace crt
