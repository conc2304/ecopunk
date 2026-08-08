#include "HudProfileCompiler.h"

#include "HudSourceResolver.h"
#include "HudWidgetContract.h"

#include <algorithm>
#include <unordered_map>

namespace hudpresent {

HudProfileCompiler::HudProfileCompiler(const HudRegionCatalog& regions, HudVocabularyResolver& vocabulary)
	: regions_(regions), vocabulary_(vocabulary) {}

namespace {

void addIssue(HudCompiledProfile& out, HudProfileIssueSeverity sev, HudProfileIssueKind kind,
	const std::string& bindingId, std::string detail) {
	out.issues.push_back({sev, kind, bindingId, std::move(detail)});
}

} // namespace

void HudProfileCompiler::validateAndAppend(const std::string& sceneId, const HudSlotBinding& binding, HudCompiledProfile& out) const {
	HudCompiledBinding compiled;
	compiled.binding = binding;
	compiled.valid = true;

	// -- 1. Region existence ---------------------------------------------
	const HudRegionDescriptor* region = regions_.find(binding.regionId);
	if (!region) {
		addIssue(out, HudProfileIssueSeverity::Error, HudProfileIssueKind::UnknownRegion,
			binding.bindingId, "region '" + binding.regionId + "' does not exist in HudRegionCatalog");
		compiled.valid = false;
		out.bindings.push_back(compiled);
		return; // nothing further can be validated meaningfully without a region
	}

	// -- 2. Widget allowance for region -----------------------------------
	if (!regions_.acceptsWidget(binding.regionId, binding.widgetType)) {
		addIssue(out, HudProfileIssueSeverity::Error, HudProfileIssueKind::WidgetNotAllowedInRegion,
			binding.bindingId, "region '" + binding.regionId + "' does not accept this widget type");
		compiled.valid = false;
	}

	// -- 3/4. Source existence + value-type compatibility (statically
	// known sources only — scene-metric-shaped IDs are dynamic) ---------
	for (size_t i = 0; i < binding.sourceCount; ++i) {
		const auto& src = binding.sources[i];
		if (src.sourceId.empty()) continue;
		if (HudSourceResolver::looksLikeSceneMetricId(src.sourceId)) continue; // dynamic, not statically validated

		auto staticType = HudSourceResolver::staticValueTypeOf(src.sourceId);
		if (!staticType) {
			addIssue(out, HudProfileIssueSeverity::Error, HudProfileIssueKind::UnknownStaticSource,
				binding.bindingId, "source '" + src.sourceId + "' (role '" + src.role + "') is not a known canonical source");
			compiled.valid = false;
			continue;
		}

		// Compatibility is checked against the widget's declared
		// acceptedTypes for this role, if the role is one the widget
		// contract knows about; an unrecognized role name is caught
		// separately by the required-role pass below (it will simply
		// never be matched), so it's not re-flagged here.
		const auto& contract = HudWidgetContracts::forType(binding.widgetType);
		auto roleIt = std::find_if(contract.roles.begin(), contract.roles.end(),
			[&](const HudWidgetRoleSpec& r) { return r.role == src.role; });
		if (roleIt != contract.roles.end() && !roleIt->acceptedTypes.empty()) {
			bool ok = std::find(roleIt->acceptedTypes.begin(), roleIt->acceptedTypes.end(), *staticType) != roleIt->acceptedTypes.end();
			if (!ok) {
				addIssue(out, HudProfileIssueSeverity::Error, HudProfileIssueKind::SourceValueTypeMismatch,
					binding.bindingId, "source '" + src.sourceId + "' role '" + src.role + "' has an incompatible value type for " + " this widget");
				compiled.valid = false;
			}
		}
	}

	// -- 5. Required source roles ------------------------------------------
	{
		const auto& contract = HudWidgetContracts::forType(binding.widgetType);
		for (const auto& roleSpec : contract.roles) {
			if (!roleSpec.required) continue;
			const HudSourceRef* found = binding.findSource(roleSpec.role);
			if (!found || found->sourceId.empty()) {
				addIssue(out, HudProfileIssueSeverity::Error, HudProfileIssueKind::RequiredSourceRoleMissing,
					binding.bindingId, "required role '" + roleSpec.role + "' has no bound source");
				compiled.valid = false;
			}
		}
	}

	// -- Label-specific rule: needs a "text" role source OR a
	// labelVocabularyId (or both) — see HudWidgetContract.h's comment on
	// why "text" is not marked required=true in the generic table. -----
	if (binding.widgetType == HudWidgetType::Label) {
		const HudSourceRef* text = binding.findSource("text");
		bool hasTextSource = text && !text->sourceId.empty();
		if (!hasTextSource && !binding.labelVocabularyId) {
			addIssue(out, HudProfileIssueSeverity::Error, HudProfileIssueKind::LabelMissingTextSource,
				binding.bindingId, "Label binding needs a 'text' role source or labelVocabularyId");
			compiled.valid = false;
		}
	}

	// -- 7. History eligibility --------------------------------------------
	if (binding.history.has_value()) {
		const auto& contract = HudWidgetContracts::forType(binding.widgetType);
		if (!region->historyAllowed) {
			addIssue(out, HudProfileIssueSeverity::Error, HudProfileIssueKind::HistoryNotAllowedForRegion,
				binding.bindingId, "region '" + binding.regionId + "' does not allow history");
			compiled.valid = false;
		}
		if (!contract.supportsHistory) {
			addIssue(out, HudProfileIssueSeverity::Error, HudProfileIssueKind::HistoryNotSupportedByWidget,
				binding.bindingId, "this widget type does not support history");
			compiled.valid = false;
		}
	}

	// -- 8. Vocabulary resolution (cached at compile time) -----------------
	if (binding.labelVocabularyId) {
		compiled.resolvedLabelText = vocabulary_.resolve(sceneId, *binding.labelVocabularyId);
		if (!vocabulary_.resolvedThroughVocabulary(sceneId, *binding.labelVocabularyId)) {
			addIssue(out, HudProfileIssueSeverity::Warning, HudProfileIssueKind::VocabularyKeyUnresolved,
				binding.bindingId, "vocabulary key '" + *binding.labelVocabularyId + "' fell through to stable-ID fallback");
			// Warning only — a degraded-but-functional outcome, not invalid.
		}
	}

	out.bindings.push_back(compiled);
}

HudCompiledProfile HudProfileCompiler::compile(const std::string& sceneId, const HudPresentationProfileRegistry& registry) const {
	std::vector<HudSlotBinding> bindings;
	for (const auto& b : registry.universalProfile().bindings) bindings.push_back(b);
	for (const auto& b : registry.overlayHealthProfile().bindings) bindings.push_back(b);
	for (const auto& b : registry.overlayTransitionProfile().bindings) bindings.push_back(b);

	if (const auto* flexible = registry.flexibleProfileForScene(sceneId)) {
		for (const auto& b : flexible->bindings) bindings.push_back(b);
	}
	// A completely unknown sceneId is not itself an error at this layer —
	// it simply means no flexible bindings are appended, so the compiled
	// profile still contains every universal/overlay binding (this is
	// exactly this task's §9 rule "invalid flexible bindings must not
	// remove universal HUD content", generalized to "no flexible content
	// at all" as the degenerate case). Callers that need to know whether
	// `sceneId` was recognized should check
	// registry.flexibleProfileForScene(sceneId) themselves.

	return compileBindings(sceneId, bindings);
}

HudCompiledProfile HudProfileCompiler::compileBindings(const std::string& sceneId, const std::vector<HudSlotBinding>& bindings) const {
	HudCompiledProfile out;
	out.sceneId = sceneId;

	for (const auto& b : bindings) validateAndAppend(sceneId, b, out);
	checkFlexibleOccupancy(out);
	return out;
}

void HudProfileCompiler::checkFlexibleOccupancy(HudCompiledProfile& out) const {
	// -- 6. Mutually exclusive flexible-region occupancy --------------------
	// A single pass over every appended binding (valid or not so far),
	// grouped by regionId, in insertion order. The first
	// region->maxBindings bindings assigned to a region keep their
	// existing validity; every binding beyond that count is marked
	// invalid with a FlexibleRegionOccupancyConflict Error, regardless of
	// whether it was otherwise valid — occupancy is a structural property
	// of the whole compiled profile, not of any one binding in isolation.
	std::unordered_map<std::string, int> occupancy;
	for (auto& compiled : out.bindings) {
		const HudRegionDescriptor* region = regions_.find(compiled.binding.regionId);
		if (!region) continue; // already flagged as UnknownRegion above
		int& count = occupancy[compiled.binding.regionId];
		count++;
		if (count > region->maxBindings) {
			addIssue(out, HudProfileIssueSeverity::Error, HudProfileIssueKind::FlexibleRegionOccupancyConflict,
				compiled.binding.bindingId,
				"region '" + compiled.binding.regionId + "' already has " + std::to_string(region->maxBindings) + " binding(s) assigned");
			compiled.valid = false;
		}
	}
}

} // namespace hudpresent
