#include "ExperienceSettingsPageView.h"

#include "ofGraphics.h"

namespace {

constexpr float kScale = 1.6f;          // bitmap glyph 8 px -> ~13 px: legible at 1280x720
constexpr float kValueX = 372.0f;
constexpr float kRowH = 33.0f;
constexpr size_t kValueChars = 66;      // (1240 - kValueX) / (8 * kScale)

void text(const std::string& s, float x, float y, float scale = kScale) {
	ofPushMatrix();
	ofTranslate(x, y);
	ofScale(scale, scale);
	ofDrawBitmapString(s, 0, 8);
	ofPopMatrix();
}

std::string fit(const std::string& s, size_t maxChars) {
	if (s.size() <= maxChars) return s;
	return s.substr(0, maxChars - 2) + "..";
}

} // namespace

void drawExperienceSettingsPage(const settingspage::Page& page) {
	using settingspage::RowId;
	const auto& ctx = page.context();
	ofPushStyle();
	ofEnableAlphaBlending();
	ofFill();
	ofSetColor(10, 12, 10, 246);
	ofDrawRectangle(0, 0, 1280, 720);
	ofNoFill();
	ofSetLineWidth(2.0f);
	ofSetColor(225, 218, 190);
	ofDrawRectangle(16, 16, 1248, 688);

	ofSetColor(235, 230, 205);
	text("EXPERIENCE SETTINGS", 40, 30, 2.0f);
	text("Local time  " + settingspage::clockLine(ctx.clock, ctx.settings.clock24Hour) + "   "
		+ (ctx.clock.timezone.empty() ? std::string("(timezone unknown)") : ctx.clock.timezone), 40, 62);
	text("The experience keeps running behind this page; automatic scene/media cycling is PAUSED until you return.", 40, 82, 1.2f);

	float y = 108.0f;
	const auto rows = page.rows();
	const settingspage::RowView* focusedRow = nullptr;
	for (const auto& r : rows) {
		if (r.id == RowId::SceneAuto) { ofSetColor(235, 230, 205); text("--- EXPERIENCE AUTOMATION", 40, y, 1.3f); y += 26.0f; }
		if (r.id == RowId::ClockFormat) { y += 4.0f; ofSetColor(235, 230, 205); text("--- DATE & TIME  (source: " + (ctx.clock.source.empty() ? std::string("none") : ctx.clock.source) + ")", 40, y, 1.3f); y += 26.0f; }
		if (r.id == RowId::ReturnBottom) y += 4.0f;

		const float h = kRowH - 6.0f;
		if (r.focused) {
			ofFill();
			ofSetColor(r.editing ? 70 : 38, r.editing ? 78 : 56, r.editing ? 40 : 38, 255);
			ofDrawRectangle(36, y - 3, 1208, h);
			ofNoFill();
			ofSetLineWidth(r.editing ? 4.0f : 2.0f);
			ofSetColor(255, 245, 200);
			ofDrawRectangle(36, y - 3, 1208, h);
			if (r.editing) { ofSetLineWidth(1.0f); ofDrawRectangle(40, y + 1, 1200, h - 8); }
			focusedRow = &r;
		}
		ofSetColor(r.disabled ? 150 : 240, r.disabled ? 148 : 235, r.disabled ? 130 : 210);
		const char* marker = r.editing ? "*" : r.focused ? ">" : " ";
		text(std::string(marker) + " " + r.label + (r.disabled ? "" : ""), 44, y + 2);
		if (!r.value.empty()) text(fit(r.value, kValueChars), kValueX, y + 2);
		y += kRowH;
	}

	// Notes / status footer
	ofSetColor(225, 218, 190);
	if (focusedRow && !focusedRow->note.empty()) text(fit(focusedRow->note, 112), 40, 572, 1.3f);

	std::string persist;
	if (!ctx.persistenceEnabled) persist = "NOT SAVED: persistence is off for this run (no settings file)";
	else if (!ctx.persistenceStatus.empty()) persist = (ctx.persistenceFailed ? "!! " : "") + ctx.persistenceStatus;
	if (!persist.empty()) { ofSetColor(ctx.persistenceFailed ? 255 : 225, ctx.persistenceFailed ? 170 : 218, ctx.persistenceFailed ? 140 : 190); text("Settings file: " + fit(persist, 90), 40, 596, 1.3f); }
	if (!ctx.clockResult.empty()) { ofSetColor(ctx.clockResultFailed ? 255 : 225, ctx.clockResultFailed ? 170 : 218, ctx.clockResultFailed ? 140 : 190); text("Clock: " + fit((ctx.clockResultFailed ? "!! " : "") + ctx.clockResult, 100), 40, 620, 1.3f); }
	if (!ctx.clockWritable) { ofSetColor(190, 184, 160); text("Clock changes disabled: " + fit(ctx.clockWriteNote, 90), 40, 644, 1.2f); }

	ofSetColor(235, 230, 205);
	text(page.hint(), 40, 674, 1.4f);
	ofPopStyle();
}
