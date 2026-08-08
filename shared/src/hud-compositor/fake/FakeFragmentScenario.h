#pragma once

// Fake scenario for `fragment-trail`. §11.4 / §18.4.
#include "FakeHudScenarioBase.h"

namespace hudpresent {

class FakeFragmentScenario : public FakeHudScenarioBase {
public:
	FakeFragmentScenario();

protected:
	SceneBaseline buildBaseline(float t, uint64_t seed) const override;
};

} // namespace hudpresent
