#include "HudOverlayDialPanel.h"

namespace hudoverlay {

void HudOverlayDialPanel::setup(const HudOverlayDialState& initial) {
	intensityParam.set("Intensity", initial.intensity, 0.0f, 1.0f);
	disciplineParam.set("Discipline", initial.discipline, 0.0f, 1.0f);
	eventCouplingParam.set("Event Coupling", initial.eventCoupling, 0.0f, 1.0f);
	int initialPalette = initial.palette == Palette::Orange ? 1 : (initial.palette == Palette::Lime ? 2 : 0);
	paletteParam.set("Palette (0 Mono / 1 Orange / 2 Lime)", initialPalette, 0, 2);

	rootGroup.setName("HUD Glitch Overlay");
	rootGroup.add(intensityParam);
	rootGroup.add(disciplineParam);
	rootGroup.add(eventCouplingParam);
	rootGroup.add(paletteParam);

	panel.setup(rootGroup);
}

void HudOverlayDialPanel::update(HudOverlayDialState& outDials) {
	outDials.intensity     = intensityParam;
	outDials.discipline    = disciplineParam;
	outDials.eventCoupling = eventCouplingParam;
	switch (paletteParam.get()) {
		case 1: outDials.palette = Palette::Orange; break;
		case 2: outDials.palette = Palette::Lime; break;
		default: outDials.palette = Palette::Mono; break;
	}
}

void HudOverlayDialPanel::draw() {
	if (visible) panel.draw();
}

} // namespace hudoverlay
