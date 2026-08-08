#pragma once

// ============================================================================
// IFakeHudScenario.h — exactly the interface shape this task's §11
// specifies, moved into the hudpresent namespace and typed against
// FakeHudFrameData.
// ============================================================================

#include "../FakeHudSemanticTypes.h"

#include <cstdint>
#include <string>

namespace hudpresent {

class IFakeHudScenario {
public:
	virtual ~IFakeHudScenario() = default;
	virtual std::string scenarioId() const = 0;
	virtual void reset(uint64_t seed) = 0;
	virtual FakeHudFrameData sample(float elapsedSeconds) = 0;
};

} // namespace hudpresent
