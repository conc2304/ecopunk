#pragma once

// Fake scenario for `blob-region-prototype`. Candidate signals per
// docs/probes/hud-runtime-validation-studio-probe.md §11.1 and
// HUD-Semantic-Slot-Model-v1.md §18.1 — provisional IDs, not frozen slot
// IDs (per this task's own instruction).
#include "FakeHudScenarioBase.h"

namespace hudpresent {

class FakeBlobScenario : public FakeHudScenarioBase {
public:
	FakeBlobScenario();

protected:
	SceneBaseline buildBaseline(float t, uint64_t seed) const override;
};

} // namespace hudpresent
