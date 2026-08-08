#include "EffectActivityStatus.h"
#include <algorithm>
#include <map>

namespace videoeffects {

	namespace {

		struct EffectGroup {
			std::string effectId;
			std::string displayName;
			float prominence = 0.0f;
			bool anyTransitioning = false;
			float transitioningProgress01 = 1.0f; // first-found transitioning member's progress
			std::string minSlotId; // smallest slotId seen, for deterministic tie-break
		};

		// Shared by resolveDominantEffectIds() and resolveDominantEffectLabels()
		// so the two can never silently rank things differently — see
		// EffectActivityStatus.h's dominance-resolution-rules comment.
		std::vector<EffectGroup> resolveDominantGroups(const EffectActivityStatus & status, const DominanceConfig & config) {
			std::map<std::string, EffectGroup> groups; // keyed by effectId

			for (const auto & slot : status.slots) {
				if (slot.prominence < config.minProminenceToShow) continue;

				EffectGroup & group = groups[slot.effectId];
				bool isNewGroup = group.effectId.empty();
				if (isNewGroup) {
					group.effectId = slot.effectId;
					group.displayName = slot.displayName.empty() ? slot.effectId : slot.displayName;
					group.minSlotId = slot.slotId;
				}

				group.prominence = std::max(group.prominence, slot.prominence);
				if (slot.phase == EvolutionPhase::Transitioning && !group.anyTransitioning) {
					group.transitioningProgress01 = slot.transitionProgress01;
				}
				group.anyTransitioning = group.anyTransitioning || (slot.phase == EvolutionPhase::Transitioning);
				if (slot.slotId < group.minSlotId) group.minSlotId = slot.slotId;
			}

			std::vector<EffectGroup> ordered;
			ordered.reserve(groups.size());
			for (auto & kv : groups) ordered.push_back(kv.second);

			std::sort(ordered.begin(), ordered.end(), [](const EffectGroup & a, const EffectGroup & b) {
				if (a.prominence != b.prominence) return a.prominence > b.prominence;
				return a.minSlotId < b.minSlotId;
			});

			std::size_t take = std::min(config.maxLabels, ordered.size());
			ordered.resize(take);
			return ordered;
		}

	} // namespace

	std::vector<DominantEffectResult> resolveDominantEffectIds(const EffectActivityStatus & status, const DominanceConfig & config) {
		std::vector<DominantEffectResult> results;
		for (const auto & group : resolveDominantGroups(status, config)) {
			DominantEffectResult r;
			r.effectId = group.effectId;
			r.transitioning = group.anyTransitioning;
			r.transitionProgress01 = group.anyTransitioning ? group.transitioningProgress01 : 1.0f;
			r.prominence = group.prominence;
			results.push_back(std::move(r));
		}
		return results;
	}

	std::vector<std::string> resolveDominantEffectLabels(const EffectActivityStatus & status, const DominanceConfig & config) {
		std::vector<std::string> labels;
		for (const auto & group : resolveDominantGroups(status, config)) {
			std::string label = group.displayName;
			if (config.annotateTransitioning && group.anyTransitioning) {
				label += config.transitioningSuffix;
			}
			labels.push_back(std::move(label));
		}
		return labels;
	}

	EffectHealth deriveEffectHealth(const VideoEffectLoadReport & loadReport, bool knowledgePackRejected) {
		if (!loadReport.missing.empty() || !loadReport.failed.empty()) {
			return EffectHealth::Failed;
		}
		if (!loadReport.fallback.empty() || knowledgePackRejected) {
			return EffectHealth::Degraded;
		}
		return EffectHealth::Ready;
	}

} // namespace videoeffects
