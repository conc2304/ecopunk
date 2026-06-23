#include "AnnotationRenderer.h"
#include "Settings.h"
#include "ofGraphics.h"
#include "ofMath.h"

namespace {
	constexpr float BITMAP_CHAR_WIDTH = 8; // ofDrawBitmapString's built-in font is a fixed 8px-wide glyph
}

void AnnotationRenderer::setup(const GridSystem* grid_, int dividerCol_, int canvasW_, int canvasH_){
	grid = grid_;
	dividerCol = dividerCol_;
	canvasW = canvasW_;
	canvasH = canvasH_;
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
