#pragma once

// Fake scenario for `quadrant-crosshair`. §11.5 / §18.5.
#include "FakeHudScenarioBase.h"

namespace hudpresent {

class FakeQuadrantScenario : public FakeHudScenarioBase {
public:
	FakeQuadrantScenario();

protected:
	SceneBaseline buildBaseline(float t, uint64_t seed) const override;
};

} // namespace hudpresent
