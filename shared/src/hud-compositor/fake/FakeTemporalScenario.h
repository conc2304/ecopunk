#pragma once

// Fake scenario for `temporal-fields`. §11.3 / §18.3.
#include "FakeHudScenarioBase.h"

namespace hudpresent {

class FakeTemporalScenario : public FakeHudScenarioBase {
public:
	FakeTemporalScenario();

protected:
	SceneBaseline buildBaseline(float t, uint64_t seed) const override;
};

} // namespace hudpresent
