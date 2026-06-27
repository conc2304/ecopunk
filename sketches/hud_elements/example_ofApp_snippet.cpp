#include "hud_elements/HudElements.h"

// Place these in ofApp.h as members:
hud::ScannerWidget scanner;
hud::NodeNetworkWidget network;
hud::FlowFieldWidget flow;
hud::ContourWidget contours;
hud::GaugeWidget gauge;
hud::DataCardWidget card;
hud::HexGridWidget hex;
hud::ReticleWidget reticles;

// Place this in ofApp::setup():
hud::HudTheme theme;
theme.colors.primary = ofColor(124, 232, 230, 220);
theme.colors.secondary = ofColor(103, 255, 142, 200);
theme.colors.accent = ofColor(244, 255, 106, 220);
theme.colors.muted = ofColor(124, 232, 230, 70);
theme.colors.background = ofColor(0, 20, 16, 18);
theme.frame.style = hud::FrameStyle::Corners;
theme.frame.showTicks = true;
theme.frame.showScanLines = false;
theme.additive = true;

scanner.setTheme(theme);
network.setTheme(theme);
flow.setTheme(theme);
contours.setTheme(theme);
gauge.setTheme(theme);
card.setTheme(theme);
hex.setTheme(theme);
reticles.setTheme(theme);

int w = ofGetWidth();
int h = ofGetHeight();
scanner.setBounds(w * 0.04f, h * 0.08f, w * 0.22f, w * 0.22f);
network.setBounds(w * 0.34f, h * 0.08f, w * 0.42f, h * 0.26f);
flow.setBounds(w * 0.08f, h * 0.62f, w * 0.46f, h * 0.22f);
contours.setBounds(w * 0.58f, h * 0.55f, w * 0.34f, h * 0.28f);
gauge.setBounds(w * 0.78f, h * 0.10f, w * 0.16f, w * 0.16f);
card.setBounds(w * 0.04f, h * 0.42f, w * 0.24f, h * 0.15f);
hex.setBounds(0, 0, w, h);
reticles.setBounds(0, 0, w, h);

scanner.setup();
network.setup();
flow.setup();
contours.setup();
gauge.setup();
card.setup();
hex.setup();
reticles.setup();

// Place this in ofApp::update():
float dt = ofGetLastFrameTime();
scanner.update(dt);
network.update(dt);
flow.update(dt);
contours.update(dt);
gauge.update(dt);
card.update(dt);
hex.update(dt);
reticles.update(dt);

// Place this in ofApp::draw(), after drawing video/nature content:
hex.draw();
contours.draw();
flow.draw();
network.draw();
scanner.draw();
gauge.draw();
card.draw();
reticles.draw();
