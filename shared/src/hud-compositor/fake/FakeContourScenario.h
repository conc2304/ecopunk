#pragma once

// Fake scenario for `contour-portrait`. §11.2 / §18.2.
#include "FakeHudScenarioBase.h"

namespace hudpresent {

class FakeContourScenario : public FakeHudScenarioBase {
public:
	FakeContourScenario();

protected:
	SceneBaseline buildBaseline(float t, uint64_t seed) const override;
};

} // namespace hudpresent
