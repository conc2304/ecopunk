#pragma once

// ============================================================================
// HudWidgetInput.h — the resolved, formatted, policy-applied data a widget
// actually receives. Still OF-independent (see HudDataTypes.h's header
// comment) — only shared/src/hud-compositor/widgets/ (the draw layer)
// depends on openFrameworks.
//
// This is the boundary this task's §7 constraints describe: everything
// upstream of HudWidgetInput (HudSourceResolver, HudMissingDataController,
// HudFormattingService, HudVocabularyResolver, HudHistoryStore) has
// already run by the time a widget sees one of these — a widget:
//   - never searches the semantic snapshot (it only sees roles/values
//     already picked out for it);
//   - never resolves vocabulary (captionText/formattedText below are
//     already-resolved display strings);
//   - never knows scene IDs (no field here carries one — see
//     shared/src/hud-compositor-test/hud_presentation_tests.cpp's
//     test_no_scene_id_leaks_into_compiled_bindings());
//   - never calculates scene semantics or chooses source fallbacks (the
//     orchestrator + HudMissingDataController already did, before
//     constructing this);
//   - never owns an independent data-sampling clock (elapsedSeconds below
//     is handed in by the orchestrator every draw call, not accumulated
//     by the widget itself).
// ============================================================================

#include "HudDataTypes.h"
#include "HudHistoryStore.h"
#include "HudWidgetTypes.h"

#include <string>
#include <vector>

namespace hudpresent {

struct HudWidgetRoleValue {
	std::string role;
	bool shouldRender = false;
	bool dimmed = false;
	bool useAmbientFallback = false;
	HudResolvedValue value;        // meaningful when shouldRender && !useAmbientFallback
	std::string formattedText;     // already run through HudFormattingService (+ vocabulary where relevant)

	// Populated only for IdentifierList-valued roles (effects.active,
	// etc.) — each list item already resolved through vocabulary
	// individually by the orchestrator, so a per-item widget (EffectChips)
	// never has to call the vocabulary resolver itself. `formattedText`
	// above is still populated too (the comma-joined form), for widgets
	// that want a single-string summary instead.
	std::vector<std::string> formattedItems;
};

struct HudWidgetInput {
	std::string bindingId;
	std::string regionId;
	HudWidgetType widgetType = HudWidgetType::Label;

	// Already resolved via HudVocabularyResolver at compile time
	// (HudCompiledBinding::resolvedLabelText) — a static caption, not a
	// live value.
	std::string captionText;

	std::vector<HudWidgetRoleValue> roles; // one per binding.sources[i], in binding order

	// True when NO role in this binding rendered anything this frame AND
	// the region as a whole has no other binding covering it either
	// (HudMissingDataController::regionNeedsAmbientFallback) — distinct
	// from an individual role's own useAmbientFallback, which fires from
	// that role's own HudMissingPolicy::UseAmbientFallback regardless of
	// its neighbors.
	bool regionAmbientFallback = false;

	// Handed in by the orchestrator every draw call — see this struct's
	// header comment on "no independent data-sampling clock".
	float elapsedSeconds = 0.0f;

	// Read-only, pre-fetched from HudHistoryStore for the binding's
	// primary "value" role, when history was requested — only ever
	// populated for widgets whose HudWidgetContract sets
	// supportsHistory=true (Sparkline, in this increment).
	//
	// Engineering Session 2: HudHistoryStore::SampleView, not
	// std::vector<Sample> — a fixed-capacity, stack-copied view (see that
	// type's own comment), closing the one steady-state heap allocation
	// Session 1 flagged as a documented trade-off. Supports the same
	// size()/operator[]/range-for/back() surface a std::vector did, so no
	// widget draw-call logic needed to change, only this field's type.
	HudHistoryStore::SampleView historySamples;
	HudHistoryStore::Stats historyStats;

	// Validation-Studio-only: populated when the orchestrator substitutes
	// a BindingPlaceholder for a binding HudProfileCompiler marked
	// invalid, so the placeholder can show WHY it's a placeholder. Always
	// empty for production/non-placeholder widgets.
	std::vector<std::string> compileIssueSummaries;

	const HudWidgetRoleValue* findRole(const std::string& role) const {
		for (const auto& r : roles) {
			if (r.role == role) return &r;
		}
		return nullptr;
	}
};

} // namespace hudpresent
