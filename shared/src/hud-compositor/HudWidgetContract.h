#pragma once

// ============================================================================
// HudWidgetContract.h — the single source of truth for "what source roles
// does each HudWidgetType need, and what value types can fill them".
//
// Shared by HudProfileCompiler (validates bindings against it at compile
// time — "validate required source roles", "validate source-value-type
// compatibility") and by the widget layer (each widget looks its own
// sources up by the same role names, so drift between "what the compiler
// validated" and "what the widget actually reads" is structurally
// impossible — both sides read this one table).
//
// Deliberately OF-independent (see HudDataTypes.h's header comment for why
// that matters).
// ============================================================================

#include "HudDataTypes.h"
#include "HudWidgetTypes.h"

#include <string>
#include <vector>

namespace hudpresent {

struct HudWidgetRoleSpec {
	std::string role;
	bool required = true;
	// Empty = accepts any HudSourceValueType for this role (used by
	// MetadataCard's "value" role, which is intentionally polymorphic).
	std::vector<HudSourceValueType> acceptedTypes;
};

struct HudWidgetContract {
	HudWidgetType widgetType;
	std::vector<HudWidgetRoleSpec> roles;
	bool supportsHistory = false;
};

class HudWidgetContracts {
public:
	static const HudWidgetContract& forType(HudWidgetType type);
};

} // namespace hudpresent
