#include "AnnotationRenderer.h"
#include "Settings.h"
#include "ofGraphics.h"
#include "ofMath.h"
#include "ofLog.h"
#include "ofUtils.h"
#include "glm/glm.hpp"
#include <algorithm>
#include <cstdlib>

namespace {
	constexpr float BITMAP_CHAR_WIDTH = 8; // ofDrawBitmapString's built-in font is a fixed 8px-wide glyph

	float randRangeF(float lo, float hi){
		return lo + (hi - lo) * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
	}

	int randRangeI(int lo, int hiInclusive){
		return lo + rand() % (hiInclusive - lo + 1);
	}

	glm::vec2 closestPointOnRect(const ofRectangle& r, glm::vec2 p){
		return glm::vec2(
			ofClamp(p.x, r.x, r.x + r.width),
			ofClamp(p.y, r.y, r.y + r.height)
		);
	}
}

void AnnotationRenderer::setup(const GridSystem* grid_, int canvasW_, int canvasH_){
	grid = grid_;
	canvasW = canvasW_;
	canvasH = canvasH_;
}

void AnnotationRenderer::loadCodeFont(const std::string& path, int size){
	bool ok = codeFont.load(ofToDataPath(path), size);
	if(!ok || !codeFont.isLoaded()){
		ofLogError("AnnotationRenderer") << "failed to load code font at " << path;
	}
}

void AnnotationRenderer::setMeasurementLineTiming(float drawSpeed, float fadeDelay, float fadeOpacity){
	mlineDrawSpeed = drawSpeed;
	mlineFadeDelay = fadeDelay;
	mlineFadeOpacity = fadeOpacity;
}

void AnnotationRenderer::setCodeTextTiming(float intervalMin, float intervalMax, float opacityMin, float opacityMax){
	codeTextIntervalMin = intervalMin;
	codeTextIntervalMax = intervalMax;
	codeTextOpacityMin = opacityMin;
	codeTextOpacityMax = opacityMax;
	codeTextTimer = randRangeF(codeTextIntervalMin, codeTextIntervalMax);
}

void AnnotationRenderer::setCodeFragments(std::vector<std::string> fragments){
	codeFragments = std::move(fragments);
}

void AnnotationRenderer::onFragmentPlaced(Fragment* newFragment, Fragment* nearest){
	if(nearest != nullptr){
		glm::vec2 p1 = closestPointOnRect(newFragment->getBounds(), glm::vec2(nearest->getBounds().getCenter()));
		glm::vec2 p2 = closestPointOnRect(nearest->getBounds(), glm::vec2(newFragment->getBounds().getCenter()));
		float dist = glm::distance(p1, p2);

		MeasurementLine line;
		line.a = newFragment;
		line.b = nearest;
		line.p1 = p1;
		line.p2 = p2;
		line.totalDist = dist;
		line.age = 0;
		line.label = ofToString(static_cast<int>(dist)) + "px";
		measurementLines.push_back(line);

		connectionCounts[newFragment]++;
		connectionCounts[nearest]++;
		recomputeHub();
	}

	lastPlacedFragment = newFragment;
}

void AnnotationRenderer::onFragmentRemoved(Fragment* frag){
	if(frag == nullptr){
		return;
	}

	// Erase first, then recompute: recomputeHub() only ever sets hubFragment
	// from a key still present in connectionCounts, so if frag was the hub
	// this naturally replaces it (with the next-best fragment, or nullptr).
	connectionCounts.erase(frag);
	recomputeHub();

	if(lastPlacedFragment == frag){
		lastPlacedFragment = nullptr;
	}
}

void AnnotationRenderer::recomputeHub(){
	Fragment* best = nullptr;
	int bestCount = 0;
	for(const auto& [frag, count] : connectionCounts){
		if(count > bestCount){
			bestCount = count;
			best = frag;
		}
	}
	if(best != hubFragment){
		hubFragment = best;
		hubHighlightT = 0.0f;
	}
	maxConnectionCount = bestCount;
}

void AnnotationRenderer::pulseMeasurementLines(float peakOpacity, float decaySeconds){
	mlinePulseOpacity = peakOpacity;
	mlinePulseDecay = std::max(decaySeconds, 0.01f);
}

void AnnotationRenderer::update(float dt){
	for(auto& line : measurementLines){
		line.age += dt;
	}

	if(mlinePulseOpacity > 0.0f){
		mlinePulseOpacity -= dt / mlinePulseDecay;
		mlinePulseOpacity = std::max(mlinePulseOpacity, 0.0f);
	}

	if(hubFragment != nullptr && hubHighlightT < 1.0f){
		hubHighlightT = ofClamp(hubHighlightT + dt / 1.0f, 0.0f, 1.0f);
	}

	codeTextTimer -= dt;
	if(codeTextTimer <= 0.0f){
		trySpawnCodeText();
		codeTextTimer = hasCodeTextLfoWeight
			? ofMap(codeTextLfoWeight, 0.0f, 1.0f, codeTextIntervalMin, codeTextIntervalMax)
			: randRangeF(codeTextIntervalMin, codeTextIntervalMax);
	}
}

void AnnotationRenderer::clearCodeTextInRect(const ofRectangle& rect) {
	// codeTexts and usedTextRects are always pushed together in trySpawnCodeText(),
	// so they stay index-aligned. Drop pairs whose slot intersects the given rect.
	std::vector<CodeTextEntry> keptTexts;
	std::vector<ofRectangle>   keptRects;
	for (size_t i = 0; i < codeTexts.size(); ++i) {
		const ofRectangle& slot = (i < usedTextRects.size())
			? usedTextRects[i]
			: ofRectangle(codeTexts[i].pos.x - 4, codeTexts[i].pos.y - 12, 90.0f, 16.0f);
		if (slot.getIntersection(rect).getArea() <= 0) {
			keptTexts.push_back(codeTexts[i]);
			keptRects.push_back(slot);
		}
	}
	codeTexts     = std::move(keptTexts);
	usedTextRects = std::move(keptRects);
}

void AnnotationRenderer::reset(){
	measurementLines.clear();
	codeTexts.clear();
	usedTextRects.clear();
	lastPlacedFragment = nullptr;
	connectionCounts.clear();
	hubFragment = nullptr;
	maxConnectionCount = 0;
	hubHighlightT = 0.0f;
	mlinePulseOpacity = 0.0f;
	codeTextTimer = randRangeF(codeTextIntervalMin, codeTextIntervalMax);
}

bool AnnotationRenderer::findFreeTextSlot(glm::vec2 anchorPos, ofRectangle& outRect) const{
	constexpr float slotW = 90.0f;
	constexpr float slotH = 16.0f;
	float dividerX = grid->getDividerX();
	bool anchorInZoneA = anchorPos.x < dividerX;

	for(int attempt = 0; attempt < 20; attempt++){
		float x = anchorPos.x + randRangeF(-2.0f, 2.0f) * 110.0f;
		float y = anchorPos.y + randRangeF(-2.0f, 2.0f) * 90.0f;
		ofRectangle candidate(x, y, slotW, slotH);

		if(candidate.x < 0 || candidate.y < 0
			|| candidate.x + slotW > canvasW || candidate.y + slotH > canvasH) continue;
		if((candidate.x < dividerX) != anchorInZoneA) continue; // never cross the divider
		if(!grid->isRectFree(candidate, 0.0f)) continue; // overlaps a placed fragment

		bool taken = false;
		for(const auto& used : usedTextRects){
			if(used.getIntersection(candidate).width > 0 && used.getIntersection(candidate).height > 0){
				taken = true;
				break;
			}
		}
		if(taken) continue;

		outRect = candidate;
		return true;
	}
	return false;
}

void AnnotationRenderer::trySpawnCodeText(){
	if(codeFragments.empty() || lastPlacedFragment == nullptr || !codeFont.isLoaded()){
		return;
	}

	glm::vec2 anchorPos = glm::vec2(lastPlacedFragment->getBounds().getCenter());

	ofRectangle slot;
	if(!findFreeTextSlot(anchorPos, slot)){
		return;
	}

	CodeTextEntry entry;
	entry.pos = glm::vec2(slot.x + 4, slot.y + slot.height * 0.5f);
	entry.text = codeFragments[randRangeI(0, static_cast<int>(codeFragments.size()) - 1)];
	entry.opacity = randRangeF(codeTextOpacityMin, codeTextOpacityMax);

	// Truncate to fit within the slot (4px left margin already applied, keep 4px right margin)
	float maxW = slot.width - 8.0f;
	while(!entry.text.empty() && codeFont.stringWidth(entry.text) > maxW){
		entry.text.pop_back();
	}
	if(entry.text.empty()) return;

	usedTextRects.push_back(slot);
	codeTexts.push_back(entry);
}

void AnnotationRenderer::drawGrid(float alpha) const{
	ofSetLineWidth(1);

	auto drawLines = [&](const std::vector<GridLine>& lines, bool vertical){
		for(const auto& line : lines){
			if(line.opacity <= 0.0f) continue;

			ofColor c = RULE_WHITE;
			c.a = static_cast<int>(c.a * line.opacity * alpha / 0.30f); // RULE_WHITE already carries ~30% baseline
			ofSetColor(c);

			if(vertical){
				float y1 = line.extentStart * canvasH;
				float y2 = line.extentEnd * canvasH;
				ofDrawLine(line.position, y1, line.position, y2);
			} else {
				float x1 = line.extentStart * canvasW;
				float x2 = line.extentEnd * canvasW;
				ofDrawLine(x1, line.position, x2, line.position);
			}
		}
	};

	drawLines(grid->getVLines(), true);
	drawLines(grid->getHLines(), false);
}

void AnnotationRenderer::drawDivider(glm::vec2 p1, glm::vec2 p2, float brightness) const{
	ofSetColor(RULE_ORANGE * ofClamp(brightness * grid->getDividerOpacity(), 0.0f, 1.0f));
	ofSetLineWidth(4);
	ofDrawLine(p1.x, p1.y, p2.x, p2.y);
	ofSetLineWidth(1);
}

void AnnotationRenderer::drawCornerLabel(const std::string& label) const{
	float textWidth = label.size() * BITMAP_CHAR_WIDTH;
	float margin = 12;
	float x = canvasW - textWidth - margin;
	float y = canvasH - margin;

	ofSetColor(TEXT_DIM);
	ofDrawBitmapString(label, x, y);
}

void AnnotationRenderer::drawMeasurementLines() const{
	for(const auto& line : measurementLines){
		float growthProgress = (line.totalDist > 0)
			? ofClamp(line.age * mlineDrawSpeed / line.totalDist, 0.0f, 1.0f)
			: 1.0f;
		float grownLen = line.totalDist * growthProgress;

		glm::vec2 dir = line.p2 - line.p1;
		float len = glm::length(dir);
		if(len > 0){
			dir /= len;
		}

		float opacityFactor = (line.age < mlineFadeDelay) ? 1.0f : mlineFadeOpacity;
		opacityFactor = std::max(opacityFactor, mlinePulseOpacity);
		ofColor c = RULE_WHITE;
		c.a *= opacityFactor;
		ofSetColor(c);
		ofSetLineWidth(1);

		for(float d = 0; d < grownLen; d += 8.0f){
			float segEnd = std::min(d + 4.0f, grownLen);
			glm::vec2 a = line.p1 + dir * d;
			glm::vec2 b = line.p1 + dir * segEnd;
			ofDrawLine(a.x, a.y, b.x, b.y);
		}

		if(growthProgress >= 0.5f){
			glm::vec2 mid = (line.p1 + line.p2) * 0.5f;
			ofSetColor(TEXT_DIM, 255 * opacityFactor);
			ofDrawBitmapString(line.label, mid.x, mid.y);
		}
	}
}

void AnnotationRenderer::drawCodeText() const{
	if(!codeFont.isLoaded()){
		return;
	}
	for(const auto& entry : codeTexts){
		ofColor c = TEXT_CODE;
		c.a = 255 * entry.opacity;
		ofSetColor(c);
		codeFont.drawString(entry.text, entry.pos.x, entry.pos.y);
	}
}

void AnnotationRenderer::drawHubHighlight() const{
	if(hubFragment == nullptr || maxConnectionCount < 3 || hubHighlightT <= 0.0f){
		return;
	}

	const ofRectangle& b = hubFragment->getBounds();
	ofColor c = RULE_WHITE;
	c.a = ofLerp(255 * 0.25f, 255 * 0.60f, hubHighlightT);
	ofSetColor(c);
	ofNoFill();
	ofSetLineWidth(2);
	ofDrawRectangle(b);

	// Ghost border, slightly larger than the fragment, at low opacity.
	float expand = 0.04f;
	ofRectangle ghost(
		b.x - b.width * expand * 0.5f,
		b.y - b.height * expand * 0.5f,
		b.width * (1.0f + expand),
		b.height * (1.0f + expand));
	ofSetColor(255, 255, 255, static_cast<int>(255 * 0.15f * hubHighlightT));
	ofDrawRectangle(ghost);
	ofFill();
}
