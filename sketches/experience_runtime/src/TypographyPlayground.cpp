#include "TypographyPlayground.h"

#include "ofFileUtils.h"
#include "ofGraphics.h"
#include "ofJson.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <filesystem>

namespace {

constexpr int kMinSize = 8;
constexpr int kMaxSize = 32;
constexpr float kSpacingStep = 0.05f;
constexpr float kMinSpacing = 0.70f;
constexpr float kMaxSpacing = 1.80f;
const float kGlowSteps[] = {0.0f, 0.10f, 0.20f, 0.35f};

ofColor parseHex(const std::string& hex, const ofColor& fallback) {
	std::string h = hex;
	if (!h.empty() && h[0] == '#') h = h.substr(1);
	if (h.size() != 6) return fallback;
	try {
		const unsigned long v = std::stoul(h, nullptr, 16);
		return ofColor((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
	} catch (...) {
		return fallback;
	}
}

std::string toHex(const ofColor& c) {
	char buf[8];
	std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", c.r, c.g, c.b);
	return buf;
}

} // namespace

bool TypographyPlayground::isRequested() {
	const char* v = std::getenv("EXPERIENCE_RUNTIME_TYPOGRAPHY_PLAYGROUND");
	return v != nullptr && std::string(v) == "1";
}

void TypographyPlayground::load(const std::string& directory) {
	namespace fs = std::filesystem;
	directory_ = directory;
	presetPath_ = (fs::path(directory) / "preset.json").string();
	cache_.clear(); // 'r' re-reads fonts from disk (e.g. a replaced file)
	fonts_.clear();
	colors_.clear();
	targets_.clear();
	missing_.clear();

	// Built-in baseline: the HUD's current native bitmap font (declines -> unchanged path).
	FontEntry bitmap;
	bitmap.name = "HUD bitmap 8x13 (current)";
	bitmap.builtinBitmap = true;
	bitmap.present = true;
	fonts_.push_back(bitmap);
	colors_.push_back({"HUD teal (current)", ofColor(124, 232, 230), true});

	const fs::path registry = fs::path(directory) / "fonts.json";
	ofJson j;
	try {
		if (fs::exists(registry)) j = ofLoadJson(registry.string());
	} catch (...) {
		j = ofJson();
	}
	if (j.is_null() || !j.is_object()) {
		ofLogWarning("Typography") << "fonts.json missing or invalid at " << registry.string() << " — bitmap baseline only";
	} else {
		if (j.contains("fonts") && j["fonts"].is_array()) {
			for (const auto& f : j["fonts"]) {
				if (!f.is_object() || !f.contains("file") || !f["file"].is_string()) continue;
				FontEntry e;
				e.file = f["file"].get<std::string>();
				e.name = f.value("name", e.file);
				e.path = fs::weakly_canonical(fs::path(directory) / e.file).string();
				std::error_code ec;
				e.present = fs::is_regular_file(e.path, ec);
				if (!e.present) missing_.push_back(e.name + " (" + e.file + ")");
				fonts_.push_back(e);
			}
		}
		if (j.contains("colors") && j["colors"].is_array()) {
			for (const auto& c : j["colors"]) {
				if (!c.is_object()) continue;
				colors_.push_back({c.value("name", std::string("color")), parseHex(c.value("hex", std::string("#FFFFFF")), ofColor(255)), false});
			}
		}
		if (j.contains("targets") && j["targets"].is_array()) {
			for (const auto& t : j["targets"]) if (t.is_string()) targets_.insert(t.get<std::string>());
		}
	}
	if (targets_.empty()) {
		targets_ = {"controls.scene_previous", "controls.scene_next", "controls.media_previous", "controls.media_next", "controls.reseed"};
	}

	// Preset (optional): font by registry name (or file), size, spacing, colour, glow.
	fontIndex_ = 0;
	ofJson p;
	try {
		if (fs::exists(presetPath_)) p = ofLoadJson(presetPath_);
	} catch (...) {
		p = ofJson();
	}
	if (p.is_object()) {
		const std::string font = p.value("font", std::string());
		for (size_t i = 0; i < fonts_.size(); ++i) {
			if (!font.empty() && (fonts_[i].name == font || fonts_[i].file == font)) fontIndex_ = int(i);
		}
		sizePx_ = std::clamp(p.value("size", sizePx_), kMinSize, kMaxSize);
		letterSpacing_ = std::clamp(p.value("letterSpacing", letterSpacing_), kMinSpacing, kMaxSpacing);
		std::string hex = p.value("color", std::string());
		std::transform(hex.begin(), hex.end(), hex.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
		colorIndex_ = 0;
		for (size_t i = 0; i < colors_.size(); ++i) {
			if (!colors_[i].hudDefault && toHex(colors_[i].color) == hex) colorIndex_ = int(i);
		}
		if (p.contains("glow") && p["glow"].is_object()) {
			const bool on = p["glow"].value("enabled", false);
			glow_ = on ? std::clamp(p["glow"].value("intensity", 0.15f), 0.0f, 0.5f) : 0.0f;
		}
	}

	int available = 0;
	for (const auto& f : fonts_) available += (f.present && !f.builtinBitmap) ? 1 : 0;
	ofLogNotice("Typography") << "registry " << registry.string() << ": " << available << " font file(s) available, "
		<< missing_.size() << " missing";
	for (const auto& m : missing_) ofLogNotice("Typography") << "  missing candidate: " << m;
	if (!ensureFontLoaded(fontIndex_)) fontIndex_ = 0;
	for (int g = 0; g < 3; ++g) styles_[g] = {g == 0 ? fontIndex_ : 0, sizePx_, letterSpacing_, colorIndex_, glow_};
	overrides_.clear();
	panel_.clear();
	// Full preset (all groups + per-panel overrides). Legacy single-style
	// presets (above) still load, applied to the buttons group.
	if (p.is_object() && p.contains("groups") && p["groups"].is_object()) {
		auto parseStyle = [&](const ofJson& o, Style& s) {
			if (!o.is_object()) return;
			const std::string font = o.value("font", std::string());
			s.font = 0;
			for (size_t i = 0; i < fonts_.size(); ++i) {
				if (!font.empty() && (fonts_[i].name == font || fonts_[i].file == font)) s.font = int(i);
			}
			s.size = std::clamp(o.value("size", 15), kMinSize, kMaxSize);
			s.spacing = std::clamp(o.value("letterSpacing", 1.0f), kMinSpacing, kMaxSpacing);
			std::string hex = o.value("color", std::string());
			std::transform(hex.begin(), hex.end(), hex.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
			s.color = 0;
			for (size_t i = 0; i < colors_.size(); ++i) {
				if (!colors_[i].hudDefault && toHex(colors_[i].color) == hex) s.color = int(i);
			}
			s.glow = 0.0f;
			if (o.contains("glow") && o["glow"].is_object() && o["glow"].value("enabled", false)) {
				s.glow = std::clamp(o["glow"].value("intensity", 0.15f), 0.0f, 0.5f);
			}
		};
		const char* names[3] = {"buttons", "title", "info"};
		for (int g = 0; g < 3; ++g) {
			if (p["groups"].contains(names[g])) parseStyle(p["groups"][names[g]], styles_[g]);
		}
		if (p.contains("panels") && p["panels"].is_object()) {
			for (auto it = p["panels"].begin(); it != p["panels"].end(); ++it) {
				if (!it.value().is_object()) continue;
				PanelOverride o;
				if (it.value().contains("size")) o.size = std::clamp(it.value().value("size", 15), kMinSize, kMaxSize);
				if (it.value().contains("letterSpacing")) o.spacing = std::clamp(it.value().value("letterSpacing", 1.0f), kMinSpacing, kMaxSpacing);
				overrides_[it.key()] = o;
			}
		}
	}
	group_ = 0;
	loadGroup();
	preloadAll();
	logState("loaded");
}

ofTrueTypeFont* TypographyPlayground::cachedFont(int index) const {
	if (index < 0 || index >= int(fonts_.size()) || fonts_[index].builtinBitmap) return nullptr;
	auto it = cache_.find({fonts_[index].path, sizePx_});
	return it == cache_.end() ? nullptr : it->second.get();
}

bool TypographyPlayground::ensureFontLoaded(int index) { return ensureLoaded(index, sizePx_); }

bool TypographyPlayground::ensureLoaded(int index, int size) {
	if (index < 0 || index >= int(fonts_.size())) return false;
	FontEntry& e = fonts_[index];
	if (e.builtinBitmap) return true;
	if (!e.present || e.invalid) return false;
	const auto key = std::make_pair(e.path, size);
	if (auto it = cache_.find(key); it != cache_.end()) return it->second != nullptr;

	auto font = std::make_unique<ofTrueTypeFont>();
	ofTrueTypeFontSettings settings(e.path, size);
	settings.antialiased = true;
	settings.dpi = 72; // size == pixel em size on the 1280x720 canvas
	settings.addRanges(ofAlphabet::Latin);
	const bool ok = font->load(settings);
	if (!ok || !font->isLoaded()) {
		e.invalid = true;
		fontLoadFailures_++;
		cache_[key] = nullptr;
		ofLogWarning("Typography") << "font failed to load (skipped): " << e.name << " (" << e.file << ")";
		return false;
	}
	fontLoads_++;
	cache_[key] = std::move(font);
	return true;
}

void TypographyPlayground::selectFont(int direction) {
	const int n = int(fonts_.size());
	for (int step = 1; step <= n; ++step) {
		const int i = ((fontIndex_ + direction * step) % n + n) % n;
		if (ensureFontLoaded(i)) {
			fontIndex_ = i;
			return;
		}
	}
}

bool TypographyPlayground::handleKey(int key) {
	std::string why;
	if (key == OF_KEY_TAB) {
		if (ofGetKeyPressed(OF_KEY_SHIFT) && group_ == 2) {
			// Shift+Tab: step through the info/meta panels ("" = the whole group).
			std::vector<std::string> list = {""};
			list.insert(list.end(), infoPanels_.begin(), infoPanels_.end());
			auto it = std::find(list.begin(), list.end(), panel_);
			panel_ = (it == list.end() || std::next(it) == list.end()) ? list.front() : *std::next(it);
			ofLogNotice("Typography") << "panel: " << (panel_.empty() ? std::string("(whole info/meta group)") : panel_);
			return true;
		}
		storeGroup();
		group_ = (group_ + 1) % 3;
		panel_.clear();
		loadGroup();
		if (!ensureFontLoaded(fontIndex_)) fontIndex_ = 0;
		logState("group");
		return true;
	}
	// A selected info/meta panel: size, spacing and reset apply to that panel only.
	if (group_ == 2 && !panel_.empty()) {
		PanelOverride& o = overrides_[panel_];
		const int curSize = o.size > 0 ? o.size : styles_[2].size;
		const float curSpacing = o.spacing > 0.0f ? o.spacing : styles_[2].spacing;
		bool handled = true;
		switch (key) {
			case '=': case '+': o.size = std::min(kMaxSize, curSize + 1); ensureLoaded(styles_[2].font, o.size); break;
			case '-': case '_': o.size = std::max(kMinSize, curSize - 1); ensureLoaded(styles_[2].font, o.size); break;
			case '0': o.spacing = std::min(kMaxSpacing, curSpacing + kSpacingStep); break;
			case '9': o.spacing = std::max(kMinSpacing, curSpacing - kSpacingStep); break;
			case 'x': case 'X': overrides_.erase(panel_); ofLogNotice("Typography") << "panel " << panel_ << ": reset to group"; return true;
			default: handled = false; break;
		}
		if (handled) {
			ofLogNotice("Typography") << "panel " << panel_ << ": " << (o.size > 0 ? o.size : styles_[2].size) << " px, spacing "
				<< ofToString(o.spacing > 0.0f ? o.spacing : styles_[2].spacing, 2);
			return true;
		}
	}
	switch (key) {
		case '.': case '>': selectFont(+1); why = "font"; break;
		case ',': case '<': selectFont(-1); why = "font"; break;
		case '=': case '+':
			sizePx_ = std::min(kMaxSize, sizePx_ + 1);
			if (!ensureFontLoaded(fontIndex_)) fontIndex_ = 0;
			why = "size";
			break;
		case '-': case '_':
			sizePx_ = std::max(kMinSize, sizePx_ - 1);
			if (!ensureFontLoaded(fontIndex_)) fontIndex_ = 0;
			why = "size";
			break;
		case '0': letterSpacing_ = std::min(kMaxSpacing, letterSpacing_ + kSpacingStep); why = "spacing"; break;
		case '9': letterSpacing_ = std::max(kMinSpacing, letterSpacing_ - kSpacingStep); why = "spacing"; break;
		case 'k': case 'K': colorIndex_ = (colorIndex_ + 1) % int(colors_.size()); why = "color"; break;
		case 'g': case 'G': {
			const int n = int(sizeof(kGlowSteps) / sizeof(kGlowSteps[0]));
			int i = 0;
			while (i < n && kGlowSteps[i] < glow_ - 1e-4f) ++i;
			glow_ = kGlowSteps[(i + 1) % n];
			why = "glow";
			break;
		}
		case 'r': case 'R': reload(); status_ = "reloaded " + presetPath_; return true;
		case 'w': case 'W': status_ = save() ? "saved " + presetPath_ : "SAVE FAILED " + presetPath_; ofLogNotice("Typography") << status_; return true;
		case 'u': case 'U': overlayVisible_ = !overlayVisible_; return true;
		case 'l': case 'L': fitLayout_ = !fitLayout_; ofLogNotice("Typography") << "fit to layout: " << (fitLayout_ ? "on" : "off"); return true;
		default: return false;
	}
	storeGroup();
	if (group_ == 2 && why == "font") preloadAll();
	logState(why);
	return true;
}

bool TypographyPlayground::save() {
	storeGroup();
	auto styleJson = [&](const Style& s) {
		const FontEntry& f = fonts_[std::clamp(s.font, 0, int(fonts_.size()) - 1)];
		ofJson o;
		o["font"] = f.builtinBitmap ? std::string("") : f.name;
		o["fontFile"] = f.file; // informational; relative to this folder
		o["size"] = s.size;
		o["letterSpacing"] = std::round(double(s.spacing) * 100.0) / 100.0;
		const ColorPreset& c = colors_[std::clamp(s.color, 0, int(colors_.size()) - 1)];
		o["color"] = c.hudDefault ? std::string("") : toHex(c.color);
		o["glow"] = {{"enabled", s.glow > 0.0f}, {"intensity", std::round(double(s.glow > 0.0f ? s.glow : 0.15f) * 100.0) / 100.0}};
		return o;
	};
	ofJson j;
	j["version"] = 2;
	j["groups"] = {{"buttons", styleJson(styles_[0])}, {"title", styleJson(styles_[1])}, {"info", styleJson(styles_[2])}};
	ofJson panels = ofJson::object();
	for (const auto& [region, o] : overrides_) {
		ofJson e = ofJson::object();
		if (o.size > 0) e["size"] = o.size;
		if (o.spacing > 0.0f) e["letterSpacing"] = std::round(double(o.spacing) * 100.0) / 100.0;
		if (!e.empty()) panels[region] = e;
	}
	j["panels"] = panels;
	return ofSavePrettyJson(presetPath_, j);
}

void TypographyPlayground::logState(const std::string& why) const {
	const FontEntry& f = fonts_[fontIndex_];
	ofLogNotice("Typography") << why << ": font '" << f.name << "' " << sizePx_ << " px, spacing " << ofToString(letterSpacing_, 2)
		<< ", color " << colors_[colorIndex_].name << ", glow " << ofToString(glow_, 2) << " (loads " << fontLoads_ << ")";
}

TypographyPlayground::Style TypographyPlayground::effective(const std::string& regionId) const {
	const int g = groupOf(regionId);
	Style s = styles_[g];
	if (g == 2) {
		auto it = overrides_.find(regionId);
		if (it != overrides_.end()) {
			if (it->second.size > 0) s.size = it->second.size;
			if (it->second.spacing > 0.0f) s.spacing = it->second.spacing;
		}
	}
	return s;
}

void TypographyPlayground::preloadAll() {
	for (int g = 0; g < 3; ++g) {
		if (!ensureLoaded(styles_[g].font, styles_[g].size)) styles_[g].font = 0;
	}
	for (const auto& [region, o] : overrides_) ensureLoaded(styles_[2].font, o.size > 0 ? o.size : styles_[2].size);
}

int TypographyPlayground::groupOf(const std::string& regionId) {
	if (regionId.rfind("controls.", 0) == 0) return 0;
	if (regionId == "universal.scene_title") return 1;
	return 2;
}

bool TypographyPlayground::drawLabel(const std::string& regionId, const ofRectangle& bounds, const std::string& text, float opacity) const {
	const int g = groupOf(regionId);
	const Style s = effective(regionId);
	if (g == 2) infoPanels_.insert(regionId);
	ofTrueTypeFont* font = nullptr;
	if (s.font > 0 && s.font < int(fonts_.size()) && !fonts_[s.font].builtinBitmap) {
		auto it = cache_.find({fonts_[s.font].path, s.size});
		if (it != cache_.end()) font = it->second.get();
	}
	const bool active = overlayVisible_ && g == group_ && (panel_.empty() || panel_ == regionId);
	if (font == nullptr) {
		if (active) { // minimal marker: which group Tab is editing
			ofPushStyle(); ofFill(); ofSetColor(244, 255, 106, 200);
			ofDrawRectangle(bounds.x, bounds.getBottom() - 2.0f, 10.0f, 2.0f); ofPopStyle();
		}
		return false; // bitmap baseline for this group
	}
	font->setLetterSpacing(s.spacing);
	// Buttons: one word per line, the block centred on the control by real
	// glyph boxes. Everything else: a single line, left-aligned.
	std::vector<std::string> lines;
	if (g == 0) {
		std::string word;
		for (char ch : text + " ") {
			if (ch == ' ') { if (!word.empty()) lines.push_back(word); word.clear(); }
			else word += ch;
		}
	}
	if (lines.empty()) lines.push_back(text);
	const float lineGap = std::max(2.0f, float(s.size) * 0.25f);
	std::vector<ofRectangle> boxes;
	float blockH = 0.0f;
	for (const auto& line : lines) {
		boxes.push_back(font->getStringBoundingBox(line, 0.0f, 0.0f));
		blockH += boxes.back().height;
	}
	blockH += lineGap * float(lines.size() - 1);
	labelDraws_++;
	ofColor c = colors_[std::clamp(s.color, 0, int(colors_.size()) - 1)].color;
	ofPushStyle();
	ofEnableAlphaBlending();
	float top = std::round(bounds.getCenter().y - blockH * 0.5f);
	ofRectangle used;
	for (size_t i = 0; i < lines.size(); ++i) {
		const ofRectangle& box = boxes[i];
		const std::string& line = lines[i];
		const float x = std::round(g == 0 ? bounds.getCenter().x - (box.x + box.width * 0.5f) : bounds.x + 10.0f - box.x);
		const float y = std::round(top - box.y); // box.y is negative: top of the glyphs above the baseline
		const ofRectangle drawn(x + box.x, y + box.y, box.width, box.height);
		used = i == 0 ? drawn : used.getUnion(drawn);
		ofSetColor(0, 0, 0, static_cast<unsigned char>(140.0f * opacity)); // mild drop shadow
		font->drawString(line, x + 1.0f, y + 1.0f);
		if (s.glow > 0.0f) {
			for (int ring = 1; ring <= 2; ++ring) {
				ofColor gc = c;
				gc.a = static_cast<unsigned char>(std::clamp(255.0f * s.glow * opacity / float(ring * 2), 0.0f, 255.0f));
				ofSetColor(gc);
				for (int dy = -ring; dy <= ring; dy += ring)
					for (int dx = -ring; dx <= ring; dx += ring)
						if (dx || dy) font->drawString(line, x + dx, y + dy);
			}
		}
		c.a = static_cast<unsigned char>(255.0f * opacity);
		ofSetColor(c);
		font->drawString(line, x, y);
		top += box.height + lineGap;
	}
	lastBoxes_[regionId] = used;
	if (active) { ofFill(); ofSetColor(244, 255, 106, 200); ofDrawRectangle(bounds.x, bounds.getBottom() - 2.0f, 10.0f, 2.0f); }
	ofPopStyle();
	return true;
}

// All non-label HUD text: group style; bitmap groups scale by size/15 (15 = native).
bool TypographyPlayground::drawText(const std::string& regionId, const std::string& text, float x, float y, float nativeScale, const ofColor& color) const {
	if (text.empty()) return false;
	const int g = groupOf(regionId);
	const Style s = effective(regionId);
	if (g == 2) {
		infoPanels_.insert(regionId);
		if (overlayVisible_ && group_ == 2 && (panel_.empty() || panel_ == regionId)) { // which panel(s) are being edited
			ofPushStyle(); ofFill(); ofSetColor(244, 255, 106, panel_.empty() ? 120 : 230);
			ofDrawRectangle(x - 5.0f, y - 8.0f, 2.0f, 8.0f); ofPopStyle();
		}
	}
	ofTrueTypeFont* font = nullptr;
	if (s.font > 0 && s.font < int(fonts_.size()) && !fonts_[s.font].builtinBitmap) {
		auto it = cache_.find({fonts_[s.font].path, s.size});
		if (it != cache_.end()) font = it->second.get();
	}
	ofColor c = colors_[std::clamp(s.color, 0, int(colors_.size()) - 1)].hudDefault ? color : colors_[s.color].color;
	c.a = color.a;
	ofPushStyle();
	if (font == nullptr) {
		if (s.size == 15) { ofPopStyle(); return false; } // native bitmap, unchanged
		float k = nativeScale * float(s.size) / 15.0f;
		if (fitLayout_) k = std::min(k, nativeScale); // layout reserved native width; only shrink
		ofSetDrawBitmapMode(OF_BITMAPMODE_MODEL);
		for (int pass = 0; pass < 2; ++pass) {
			ofSetColor(pass == 0 ? ofColor(0, 0, 0, static_cast<unsigned char>(c.a * 0.55f)) : c);
			ofPushMatrix(); ofTranslate(x + (pass == 0 ? 1.0f : 0.0f), y + (pass == 0 ? 1.0f : 0.0f)); ofScale(k, k);
			ofDrawBitmapString(text, 0, 0); ofPopMatrix();
		}
		ofPopStyle();
		return true;
	}
	font->setLetterSpacing(s.spacing);
	float k = nativeScale;
	if (fitLayout_) {
		// The widget laid this string out for the bitmap font (8 px per
		// character x its scale). A wider TTF string would run past the panel
		// or into the next item, so shrink it uniformly to that width.
		const float reserved = 8.0f * float(text.size()) * nativeScale;
		const float ttfWidth = font->getStringBoundingBox(text, 0.0f, 0.0f).width * nativeScale;
		if (ttfWidth > reserved && ttfWidth > 0.0f) k = nativeScale * reserved / ttfWidth;
	}
	ofPushMatrix();
	ofTranslate(x, y);
	ofScale(k, k);
	ofSetColor(0, 0, 0, static_cast<unsigned char>(c.a * 0.55f)); // mild drop shadow
	font->drawString(text, 1.0f / k, 1.0f / k);
	ofSetColor(c);
	font->drawString(text, 0, 0);
	ofPopMatrix();
	ofPopStyle();
	return true;
}

// Mirrors drawText()'s sizing so widgets can centre / right-align correctly.
float TypographyPlayground::measureText(const std::string& regionId, const std::string& text, float nativeScale) const {
	if (text.empty()) return -1.0f;
	const Style s = effective(regionId);
	const float reserved = 8.0f * float(text.size()) * nativeScale;
	ofTrueTypeFont* font = nullptr;
	if (s.font > 0 && s.font < int(fonts_.size()) && !fonts_[s.font].builtinBitmap) {
		auto it = cache_.find({fonts_[s.font].path, s.size});
		if (it != cache_.end()) font = it->second.get();
	}
	if (font == nullptr) {
		if (s.size == 15) return -1.0f;
		const float w = reserved * float(s.size) / 15.0f;
		return fitLayout_ ? std::min(w, reserved) : w;
	}
	font->setLetterSpacing(s.spacing);
	const float w = font->getStringBoundingBox(text, 0.0f, 0.0f).width * nativeScale;
	return fitLayout_ ? std::min(w, reserved) : w;
}

// Multi-line text (activity log): scale the native 13 px line with the font size.
float TypographyPlayground::lineHeight(const std::string& regionId, float nativeLine) const {
	const Style s = effective(regionId);
	const bool ttf = s.font > 0 && s.font < int(fonts_.size()) && !fonts_[s.font].builtinBitmap;
	if (ttf) return float(s.size) * 1.2f;
	return nativeLine * float(s.size) / 15.0f;
}

void TypographyPlayground::drawOverlay() const {
	if (!overlayVisible_) return;
	const FontEntry& f = fonts_[fontIndex_];
	int available = 0;
	for (const auto& e : fonts_) available += (e.present && !e.invalid) ? 1 : 0;
	std::vector<std::string> lines = {
		std::string("TYPOGRAPHY PLAYGROUND (dev only)   editing: ") + (group_ == 0 ? "BUTTONS" : group_ == 1 ? "SCENE TITLE" : "INFO / META") + (group_ == 2 && !panel_.empty() ? " > " + panel_ : std::string()) + "  [tab: next group]",
		"font " + ofToString(fontIndex_ + 1) + "/" + ofToString(fonts_.size()) + ": " + f.name,
		"size " + ofToString(sizePx_) + " px   spacing " + ofToString(letterSpacing_, 2) + "   color " + colors_[colorIndex_].name
			+ "   glow " + (glow_ > 0.0f ? ofToString(glow_, 2) : std::string("off")),
		ofToString(available) + " usable, " + ofToString(missing_.size()) + " missing candidate(s) (see log)",
		", .  font    - =  size    9 0  spacing    k color   g glow",
		"r reload   w save preset   u hide/show this panel",
		"info/meta: shift+tab next panel (size/spacing per panel)   x reset panel to group",
		std::string("l fit text to layout width: ") + (fitLayout_ ? "ON (size = maximum)" : "OFF (may overflow)"),
	};
	if (!status_.empty()) lines.push_back(status_.size() > 70 ? "..." + status_.substr(status_.size() - 67) : status_);
	const float x = 36.0f, y = 128.0f, lineH = 14.0f;
	float w = 0.0f;
	for (const auto& l : lines) w = std::max(w, 8.0f * float(l.size()));
	ofPushStyle();
	ofEnableAlphaBlending();
	ofFill();
	ofSetColor(0, 0, 0, 190);
	ofDrawRectangle(x - 6.0f, y - 12.0f, w + 12.0f, lineH * float(lines.size()) + 8.0f);
	for (size_t i = 0; i < lines.size(); ++i) {
		ofSetColor(i == 0 ? ofColor(244, 255, 106) : ofColor(220, 230, 210));
		ofDrawBitmapString(lines[i], x, y + lineH * float(i));
	}
	ofPopStyle();
}
