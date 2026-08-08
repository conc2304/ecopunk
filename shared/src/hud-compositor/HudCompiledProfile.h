#pragma once

// ============================================================================
// HudCompiledProfile.h — HudProfileCompiler's output type.
// ============================================================================

#include "HudPresentationTypes.h"

#include <string>
#include <vector>

namespace hudpresent {

enum class HudProfileIssueSeverity : uint8_t {
	Warning,
	Error
};

enum class HudProfileIssueKind : uint8_t {
	UnknownRegion,
	WidgetNotAllowedInRegion,
	UnknownStaticSource,
	SourceValueTypeMismatch,
	RequiredSourceRoleMissing,
	FlexibleRegionOccupancyConflict,
	HistoryNotAllowedForRegion,
	HistoryNotSupportedByWidget,
	VocabularyKeyUnresolved,
	LabelMissingTextSource // Label-specific: neither a "text" role source nor labelVocabularyId is set
};

struct HudProfileIssue {
	HudProfileIssueSeverity severity = HudProfileIssueSeverity::Warning;
	HudProfileIssueKind kind = HudProfileIssueKind::UnknownRegion;
	std::string bindingId;
	std::string detail;
};

// One binding as it survived compilation. `valid == false` means at least
// one Error-severity issue was raised against it — per this task's §6:
// production must skip an invalid *optional* flexible binding (log once,
// continue universal HUD), while the Validation Studio instead renders a
// visible BindingPlaceholder for it and records a test failure. Both
// behaviors read `valid`/`issues`; neither is decided by the compiler
// itself, which stays policy-agnostic.
struct HudCompiledBinding {
	HudSlotBinding binding;
	bool valid = true;
	// Vocabulary resolution happens once, at compile time (per this
	// task's §8 "Cache resolved strings. Do not parse vocabulary data
	// during draw.") — the resolved caption text for
	// binding.labelVocabularyId, if any, cached here so the draw path
	// never touches HudVocabularyResolver again this profile's lifetime.
	std::string resolvedLabelText;
};

struct HudCompiledProfile {
	std::string sceneId;
	std::vector<HudCompiledBinding> bindings; // universal + selected flexible + both overlays, priority order
	std::vector<HudProfileIssue> issues;

	bool hasErrors() const {
		for (const auto& issue : issues) {
			if (issue.severity == HudProfileIssueSeverity::Error) return true;
		}
		return false;
	}
};

} // namespace hudpresent
