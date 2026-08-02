#include "EffectRandomizer.h"
#include "ofUtils.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>

namespace videoeffects {

	namespace {
		std::mt19937 makeRng(const std::optional<uint32_t> & seed) {
			if (seed.has_value()) {
				return std::mt19937(*seed);
			}
			// Non-deterministic default, entirely local to this call — never
			// touches ofRandom()'s global stream, so every existing sketch's
			// current unseeded visual behavior is unaffected by this class
			// existing.
			std::random_device rd;
			auto timeComponent = static_cast<uint32_t>(
				std::chrono::steady_clock::now().time_since_epoch().count());
			return std::mt19937(rd() ^ timeComponent);
		}

		bool valueForbidden(float value, const std::string & paramId, const std::vector<KnowledgeEntry> & blacklist) {
			for (const auto & entry : blacklist) {
				auto it = entry.forbiddenRanges.find(paramId);
				if (it == entry.forbiddenRanges.end()) continue;
				if (value >= it->second.first && value <= it->second.second) return true;
			}
			return false;
		}

		// Distance outside the *nearest* forbidden range's edge (0 = not
		// forbidden). Used only for the final safe-fallback clamp after
		// exhausting retries.
		float pushOutsideForbidden(float value, const std::string & paramId, const std::vector<KnowledgeEntry> & blacklist, float hardMin, float hardMax) {
			for (const auto & entry : blacklist) {
				auto it = entry.forbiddenRanges.find(paramId);
				if (it == entry.forbiddenRanges.end()) continue;
				float lo = it->second.first;
				float hi = it->second.second;
				if (value >= lo && value <= hi) {
					// Push to whichever side stays inside hard bounds; prefer
					// below the range, fall back to above it.
					float below = lo - 1e-3f;
					float above = hi + 1e-3f;
					if (below >= hardMin) return below;
					if (above <= hardMax) return above;
					return value; // forbidden range spans the entire hard range — nothing safe to fall back to
				}
			}
			return value;
		}
	} // namespace

	VideoEffectParameters EffectRandomizer::generate(
		const RandomizeRequest & request, const VideoEffectDefinition & definition, const EffectKnowledgeBase * knowledgeBase) const {
		VideoEffectParameters result;
		std::mt19937 rng = makeRng(request.seed);
		std::uniform_real_distribution<float> unit(0.0f, 1.0f);

		std::vector<KnowledgeEntry> whitelist;
		std::vector<KnowledgeEntry> blacklist;
		if (knowledgeBase != nullptr) {
			whitelist = knowledgeBase->loadWhitelist(request.effectId);
			blacklist = knowledgeBase->loadBlacklist(request.effectId);
		}

		for (const auto & param : definition.params) {
			if (!param.randomizable) {
				result.set(param.id, param.defaultValue);
				continue;
			}

			if (param.type == VideoEffectParameterType::Bool) {
				bool value = unit(rng) < 0.5f;
				result.set(param.id, value);
				continue;
			}

			if (param.type != VideoEffectParameterType::Float && param.type != VideoEffectParameterType::Int) {
				// Vec2/3/4 — intentionally unimplemented, see header comment.
				result.set(param.id, param.defaultValue);
				continue;
			}

			float hardMin = asFloat(param.hardMin, 0.0f);
			float hardMax = asFloat(param.hardMax, 1.0f);
			float artisticMin = std::clamp(asFloat(param.artisticMin, hardMin), hardMin, hardMax);
			float artisticMax = std::clamp(asFloat(param.artisticMax, hardMax), hardMin, hardMax);

			auto overrideIt = request.overrideRanges.find(param.id);
			float rangeMin = artisticMin;
			float rangeMax = artisticMax;
			if (overrideIt != request.overrideRanges.end()) {
				rangeMin = std::clamp(overrideIt->second.first, hardMin, hardMax);
				rangeMax = std::clamp(overrideIt->second.second, hardMin, hardMax);
				if (rangeMin > rangeMax) std::swap(rangeMin, rangeMax);
			}
			if (rangeMin > rangeMax) std::swap(rangeMin, rangeMax);

			float candidate = 0.0f;
			int attempt = 0;
			bool accepted = false;

			while (attempt <= request.maxRetries && !accepted) {
				bool useWhitelistBias = !whitelist.empty() && unit(rng) < request.whitelistBiasProbability;
				if (useWhitelistBias) {
					const KnowledgeEntry & pick = whitelist[static_cast<size_t>(unit(rng) * whitelist.size()) % whitelist.size()];
					auto snapIt = pick.snapshot.find(param.id);
					if (snapIt != pick.snapshot.end()) {
						float center = snapIt->second;
						auto tolIt = pick.tolerance.find(param.id);
						float jitter = tolIt != pick.tolerance.end() ? tolIt->second : (rangeMax - rangeMin) * 0.05f;
						std::uniform_real_distribution<float> jitterDist(center - jitter, center + jitter);
						candidate = std::clamp(jitterDist(rng), hardMin, hardMax);
					} else {
						useWhitelistBias = false;
					}
				}
				if (!useWhitelistBias) {
					std::uniform_real_distribution<float> dist(rangeMin, rangeMax);
					candidate = dist(rng);
				}

				accepted = !valueForbidden(candidate, param.id, blacklist);
				++attempt;
			}

			if (!accepted) {
				// Bounded retry exhausted — fall back safely rather than
				// looping forever, per the task's explicit requirement.
				candidate = pushOutsideForbidden(candidate, param.id, blacklist, hardMin, hardMax);
			}

			if (param.type == VideoEffectParameterType::Int) {
				result.set(param.id, static_cast<int>(std::round(candidate)));
			} else {
				result.set(param.id, candidate);
			}
		}

		return result;
	}

} // namespace videoeffects
