#include "BlobEffectBudget.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace blobbudget {

const std::vector<Treatment> & defaultTable() {
	// Costs: Experiment 2 Rock Pi 4B matrix (10 viable -> 1.0; ~8 -> 1.25;
	// ~4 -> 2.5). Weights: design v0 art-direction defaults.
	// Eligibility is visual, independent of cost: the three neighbor-
	// sampling effects are blocked because their pixel offsets assume the
	// crop spans the whole scratch FBO (it is a grow-only high-water mark),
	// so on real tracked fragments they sample cleared texels — glow rims
	// on the right/bottom edges (edge_glow), black fragments
	// (water_refraction), same mechanism (pixel_sorting). Captures:
	// docs/reviews/blob-effect-budget-captures/. Re-enable after a renderer
	// sampling fix; costs and weights are kept for that.
	static const std::vector<Treatment> kTable = {
		{ "", 0, 5, true, "shader bypass" },
		{ "recolor", 100, 3, true, "10/10 viable" },
		{ "edge_glow", 100, 2, false, "10/10 viable; blocked: right/bottom glow rims (scratch-FBO sampling)" },
		{ "water_refraction", 100, 2, false, "10/10 viable; blocked: black fragments (scratch-FBO sampling)" },
		{ "heatmap_recolor", 125, 1, true, "~8 practical; pointwise, slight color change at fade end" },
		{ "pixel_sorting", 250, 1, false, "~4 practical; blocked: same scratch-FBO sampling as above" },
	};
	return kTable;
}

std::string label(const std::string & effectId) {
	return effectId.empty() ? std::string("original") : effectId;
}

std::vector<Treatment> withWeights(std::vector<Treatment> table, const std::string & spec, std::vector<std::string> * ignored) {
	std::istringstream in(spec);
	std::string entry;
	while (std::getline(in, entry, ',')) {
		size_t colon = entry.find(':');
		std::string id = entry.substr(0, colon);
		if (id == "original") {
			id.clear();
		}
		bool found = false;
		if (colon != std::string::npos) {
			for (Treatment & t : table) {
				if (t.effectId == id) {
					t.weight = std::max(0, std::atoi(entry.substr(colon + 1).c_str()));
					found = true;
				}
			}
		}
		if (!found && ignored != nullptr && !entry.empty()) {
			ignored->push_back(entry);
		}
	}
	return table;
}

namespace {
	uint64_t mix(uint64_t x) {
		// splitmix64 finalizer: stable across platforms and runs.
		x += 0x9e3779b97f4a7c15ULL;
		x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
		x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
		return x ^ (x >> 31);
	}
}

EffectBudget::EffectBudget(std::vector<Treatment> table, int maxCostCenti)
	: table_(std::move(table))
	, maxCostCenti_(maxCostCenti) {
}

const Treatment * EffectBudget::pick(const std::vector<const Treatment *> & options, int fragmentId, uint64_t salt) const {
	long long total = 0;
	for (const Treatment * t : options) {
		total += t->weight;
	}
	if (total <= 0) {
		return nullptr;
	}
	uint64_t h = mix(static_cast<uint64_t>(static_cast<uint32_t>(fragmentId)) ^ (generation_ << 32) ^ salt);
	long long r = static_cast<long long>(h % static_cast<uint64_t>(total));
	for (const Treatment * t : options) {
		if (r < t->weight) {
			return t;
		}
		r -= t->weight;
	}
	return options.back();
}

std::string EffectBudget::candidateFor(int fragmentId) const {
	std::vector<const Treatment *> palette;
	for (const Treatment & t : table_) {
		if (t.assignable()) {
			palette.push_back(&t);
		}
	}
	const Treatment * c = pick(palette, fragmentId, 0);
	return c != nullptr ? c->effectId : std::string();
}

std::string EffectBudget::assign(int fragmentId) {
	release(fragmentId); // defensive: an id is only ever charged once
	std::string candidate = candidateFor(fragmentId);
	std::string chosen;
	int candidateCost = costOf(candidate);
	if (candidateCost >= 0 && usedCenti_ + candidateCost <= maxCostCenti_) {
		chosen = candidate;
	} else if (candidateCost > 0) {
		// Cheaper eligible shader that still fits, same deterministic draw.
		std::vector<const Treatment *> cheaper;
		for (const Treatment & t : table_) {
			if (t.assignable() && !t.isOriginal() && t.costCenti < candidateCost && usedCenti_ + t.costCenti <= maxCostCenti_) {
				cheaper.push_back(&t);
			}
		}
		const Treatment * c = pick(cheaper, fragmentId, 0x5bd1e995ULL);
		chosen = c != nullptr ? c->effectId : std::string();
	}
	if (chosen != candidate) {
		fallbackCount_++;
	}
	int cost = std::max(0, costOf(chosen));
	usedCenti_ += cost;
	active_[fragmentId] = { chosen, cost };
	return chosen;
}

void EffectBudget::release(int fragmentId) {
	auto it = active_.find(fragmentId);
	if (it == active_.end()) {
		return;
	}
	usedCenti_ -= it->second.costCenti;
	active_.erase(it);
}

void EffectBudget::releaseAll() {
	active_.clear();
	usedCenti_ = 0;
}

int EffectBudget::costOf(const std::string & effectId) const {
	if (effectId.empty()) {
		return 0;
	}
	for (const Treatment & t : table_) {
		if (t.effectId == effectId) {
			return t.costCenti;
		}
	}
	return -1;
}

int EffectBudget::activeOriginalCount() const {
	int n = 0;
	for (const auto & kv : active_) {
		n += kv.second.effectId.empty() ? 1 : 0;
	}
	return n;
}

int EffectBudget::activeShaderCount() const {
	return static_cast<int>(active_.size()) - activeOriginalCount();
}

std::map<std::string, int> EffectBudget::activeCostByEffect() const {
	std::map<std::string, int> out;
	for (const auto & kv : active_) {
		if (!kv.second.effectId.empty()) {
			out[kv.second.effectId] += kv.second.costCenti;
		}
	}
	return out;
}

bool EffectBudget::activeEffectFor(int fragmentId, std::string & effectId) const {
	auto it = active_.find(fragmentId);
	if (it == active_.end()) {
		return false;
	}
	effectId = it->second.effectId;
	return true;
}

} // namespace blobbudget
