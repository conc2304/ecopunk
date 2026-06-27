#include "DataCardWidget.h"
#include "../shared/HudUtils.h"

namespace hud {

void DataCardWidget::setup() {
    spark.resize(32);
    for (auto& v : spark) v = ofRandom(0.15f, 0.95f);
}

void DataCardWidget::update(float dt) {
    HudWidget::update(dt);
    if (spark.empty()) setup();
    if (std::fmod(time, 0.2f) < dt) {
        spark.erase(spark.begin());
        spark.push_back(ofClamp(spark.back() + ofRandom(-0.18f, 0.18f), 0.1f, 0.95f));
    }
}

void DataCardWidget::draw() {
    ofPushStyle();
    ofEnableAlphaBlending();
    ofFill();
    ofSetColor(0, 0, 0, 128);
    ofDrawRectangle(bounds.x, bounds.y, bounds.width, bounds.height);
    if (theme.additive) ofEnableBlendMode(OF_BLENDMODE_ADD); else ofEnableAlphaBlending();
    frame.draw(bounds, theme.colors, theme.frame, time);

    float p = theme.frame.padding + su(bounds, 8.0f);
    ofSetColor(scaledAlpha(theme.colors.primary, motion.opacity));
    drawTextFallback(options.title, bounds.x + p, bounds.y + p + su(bounds, 8.0f), theme.textScale * su(bounds, 0.72f));
    ofSetColor(scaledAlpha(theme.colors.secondary, motion.opacity * 0.9f));
    drawTextFallback(options.value, bounds.x + p, bounds.y + bounds.height * 0.50f, theme.textScale * su(bounds, 1.0f));
    ofSetColor(scaledAlpha(theme.colors.muted, motion.opacity * 0.9f));
    drawTextFallback(options.subtitle, bounds.x + p, bounds.y + bounds.height * 0.68f, theme.textScale * su(bounds, 0.58f));

    if (options.showMeter) {
        float mx = bounds.x + p;
        float my = bounds.y + bounds.height - p - su(bounds, 12.0f);
        float mw = bounds.width * 0.42f;
        float mh = std::max(3.0f, su(bounds, 5.0f));
        ofNoFill();
        ofSetColor(scaledAlpha(theme.colors.muted, motion.opacity * 0.5f));
        ofDrawRectangle(mx, my, mw, mh);
        ofFill();
        ofSetColor(scaledAlpha(theme.colors.accent, motion.opacity * 0.85f));
        ofDrawRectangle(mx, my, mw * ofClamp(options.meter, 0.0f, 1.0f), mh);
    }

    if (options.showSparkline && spark.size() > 1) {
        ofPolyline line;
        float sx0 = bounds.x + bounds.width * 0.58f;
        float sy0 = bounds.y + bounds.height * 0.36f;
        float sw = bounds.width * 0.30f;
        float sh = bounds.height * 0.36f;
        for (size_t i = 0; i < spark.size(); ++i) {
            float u = i / static_cast<float>(spark.size() - 1);
            line.addVertex(sx0 + u * sw, sy0 + (1.0f - spark[i]) * sh);
        }
        ofNoFill();
        ofSetLineWidth(std::max(1.0f, su(bounds, 1.0f)));
        ofSetColor(scaledAlpha(theme.colors.secondary, motion.opacity * 0.8f));
        line.draw();
    }

    ofDisableBlendMode();
    ofPopStyle();
}

} // namespace hud
