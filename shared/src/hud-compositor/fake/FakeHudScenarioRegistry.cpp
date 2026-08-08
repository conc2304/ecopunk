#include "FakeHudScenarioRegistry.h"

#include "FakeBlobScenario.h"
#include "FakeBlueprintScenario.h"
#include "FakeContourScenario.h"
#include "FakeFragmentScenario.h"
#include "FakeQuadrantScenario.h"
#include "FakeTemporalScenario.h"

namespace hudpresent {

FakeHudScenarioRegistry::FakeHudScenarioRegistry() {
	// Order matches this task's own §11 listing (Blob, Contour, Temporal,
	// Fragment, Quadrant, Blueprint).
	sceneIds_ = {
		"blob-region-prototype",
		"contour-portrait",
		"temporal-fields",
		"fragment-trail",
		"quadrant-crosshair",
		"blueprint_emergence",
	};
}

std::unique_ptr<IFakeHudScenario> FakeHudScenarioRegistry::create(const std::string& sceneId) const {
	std::unique_ptr<IFakeHudScenario> scenario;
	if (sceneId == "blob-region-prototype") scenario = std::make_unique<FakeBlobScenario>();
	else if (sceneId == "contour-portrait") scenario = std::make_unique<FakeContourScenario>();
	else if (sceneId == "temporal-fields") scenario = std::make_unique<FakeTemporalScenario>();
	else if (sceneId == "fragment-trail") scenario = std::make_unique<FakeFragmentScenario>();
	else if (sceneId == "quadrant-crosshair") scenario = std::make_unique<FakeQuadrantScenario>();
	else if (sceneId == "blueprint_emergence") scenario = std::make_unique<FakeBlueprintScenario>();
	else return nullptr;

	scenario->reset(0);
	return scenario;
}

} // namespace hudpresent
