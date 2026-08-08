#pragma once

// ============================================================================
// FakeHudScenarioRegistry.h — the six selectable fake scene profiles, in
// one place, for tests and the Validation Studio's scene-switching UI.
// ============================================================================

#include "IFakeHudScenario.h"

#include <memory>
#include <string>
#include <vector>

namespace hudpresent {

class FakeHudScenarioRegistry {
public:
	FakeHudScenarioRegistry();

	const std::vector<std::string>& sceneIds() const { return sceneIds_; }

	// Returns a freshly-constructed scenario for `sceneId` (already
	// reset(0) once), or nullptr for an unrecognized ID. Ownership
	// transfers to the caller — each call yields an independent instance,
	// which matters for tests that want to reset() with a different seed
	// without disturbing another test's scenario.
	std::unique_ptr<IFakeHudScenario> create(const std::string& sceneId) const;

private:
	std::vector<std::string> sceneIds_;
};

} // namespace hudpresent
