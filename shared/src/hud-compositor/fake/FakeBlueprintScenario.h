#pragma once

// Fake scenario for `blueprint_emergence`. §11.6 / §18.6.
#include "FakeHudScenarioBase.h"

namespace hudpresent {

class FakeBlueprintScenario : public FakeHudScenarioBase {
public:
	FakeBlueprintScenario();

protected:
	SceneBaseline buildBaseline(float t, uint64_t seed) const override;
};

} // namespace hudpresent
