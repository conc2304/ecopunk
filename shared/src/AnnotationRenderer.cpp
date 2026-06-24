#include "AnnotationRenderer.h"
#include "Settings.h"
#include "ofGraphics.h"
#include "ofMath.h"
#include "ofLog.h"
#include "ofUtils.h"
#include "glm/glm.hpp"
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

void AnnotationRenderer::setup(const GridSystem* grid_, int dividerCol_, int canvasW_, int canvasH_){
	grid = grid_;
	dividerCol = dividerCol_;
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
		line.p1 = p1;
		line.p2 = p2;
		line.totalDist = dist;
		line.age = 0;
		line.label = ofToString(static_cast<int>(dist)) + "px";
		measurementLines.push_back(line);
	}

	lastPlacedFragment = newFragment;
}

void AnnotationRenderer::update(float dt){
	for(auto& line : measurementLines){
		line.age += dt;
	}

	codeTextTimer -= dt;
	if(codeTextTimer <= 0.0f){
		trySpawnCodeText();
		codeTextTimer = randRangeF(codeTextIntervalMin, codeTextIntervalMax);
	}
}

void AnnotationRenderer::reset(){
	measurementLines.clear();
	codeTexts.clear();
	usedTextCells.clear();
	lastPlacedFragment = nullptr;
	codeTextTimer = randRangeF(codeTextIntervalMin, codeTextIntervalMax);
}

bool AnnotationRenderer::findFreeCellNear(int anchorCol, int anchorRow, int& outCol, int& outRow) const{
	bool anchorInZoneA = anchorCol < dividerCol;

	for(int attempt = 0; attempt < 20; attempt++){
		int col = anchorCol + randRangeI(-2, 2);
		int row = anchorRow + randRangeI(-2, 2);

		if(col < 0 || row < 0 || col >= grid->getCols() || row >= grid->getRows()) continue;
		if((col < dividerCol) != anchorInZoneA) continue; // never cross the divider
		if(grid->isOccupied(col, row)) continue;

		// Reject cells that already carry code text
		bool taken = false;
		for(const auto& [tc, tr] : usedTextCells){
			if(tc == col && tr == row){ taken = true; break; }
		}
		if(taken) continue;

		outCol = col;
		outRow = row;
		return true;
	}
	return false;
}

void AnnotationRenderer::trySpawnCodeText(){
	if(codeFragments.empty() || lastPlacedFragment == nullptr || !codeFont.isLoaded()){
		return;
	}

	const ofRectangle& anchorBounds = lastPlacedFragment->getBounds();
	int anchorCol = static_cast<int>(anchorBounds.x) / grid->getCellWidth();
	int anchorRow = static_cast<int>(anchorBounds.y) / grid->getCellHeight();

	int col, row;
	if(!findFreeCellNear(anchorCol, anchorRow, col, row)){
		return;
	}

	ofRectangle cell = grid->cellRect(col, row);

	CodeTextEntry entry;
	entry.pos = glm::vec2(cell.x + 4, cell.y + cell.height * 0.5f);
	entry.text = codeFragments[randRangeI(0, static_cast<int>(codeFragments.size()) - 1)];
	entry.opacity = randRangeF(codeTextOpacityMin, codeTextOpacityMax);

	// Truncate to fit within the cell (4px left margin already applied, keep 4px right margin)
	float maxW = cell.width - 8.0f;
	while(!entry.text.empty() && codeFont.stringWidth(entry.text) > maxW){
		entry.text.pop_back();
	}
	if(entry.text.empty()) return;

	usedTextCells.push_back({col, row});
	codeTexts.push_back(entry);
}

void AnnotationRenderer::drawGrid(float alpha) const{
	ofColor c = RULE_WHITE;
	c.a *= alpha;
	ofSetColor(c);
	ofSetLineWidth(1);

	for(int col = 0; col <= grid->getCols(); col++){
		float x = col * grid->getCellWidth();
		ofDrawLine(x, 0, x, canvasH);
	}
	for(int row = 0; row <= grid->getRows(); row++){
		float y = row * grid->getCellHeight();
		ofDrawLine(0, y, canvasW, y);
	}
}

void AnnotationRenderer::drawDivider(float progress) const{
	float x = dividerCol * grid->getCellWidth();
	float yEnd = canvasH * ofClamp(progress, 0.0f, 1.0f);

	ofSetColor(RULE_ORANGE);
	ofSetLineWidth(2);
	ofDrawLine(x, 0, x, yEnd);
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
