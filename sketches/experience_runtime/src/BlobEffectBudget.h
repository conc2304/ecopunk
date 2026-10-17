#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// BlobEffectBudget.h — Blob production effect budget (design v0,
// docs/reviews/blob-production-effect-budget-report.md).
//
// Separates artistic selection from hardware budget enforcement:
//   eligible palette -> deterministic weighted candidate per fragment id
//   -> budget check -> cheaper eligible fallback -> Original fallback.
// A fragment's treatment is chosen once, when it is created, and its cost
// is held until the fragment is gone (VideoRegionController's
// effectAssigner/effectReleaser). Nothing is re-chosen per frame.
//
// Costs are integer centi-units (100 = 1.0) so totals are exact.
// Measured on Rock Pi 4B (Experiment 2); PROVISIONAL until the Pi 3B+
// spot-check (runbook) confirms or adjusts individual costs.
//
// Pure and OF-free (test/blob_effect_budget_tests.cpp). Blob-local: not a
// Shared Effects authority, no HUD schema.
namespace blobbudget {

constexpr int kMaxEffectCostCenti = 1000; // MAX_EFFECT_COST = 10

struct Treatment {
	std::string effectId; // "" = Original (unshaded)
	int costCenti = -1; // < 0 = no cost metadata -> never assigned
	int weight = 0; // art-direction preference, not a hardware number
	bool eligible = false; // visual / BLOB-002 eligibility, independent of cost
	std::string note;

	bool isOriginal() const { return effectId.empty(); }
	bool assignable() const { return eligible && weight > 0 && costCenti >= 0; }
};

// Design v0 table: Original + Recolor, Edge Glow, Water Refraction,
// Heatmap Recolor, Pixel Sorting, with the design's default weights.
const std::vector<Treatment> & defaultTable();

// "original:5,recolor:3,pixel_sorting:1" -> overrides weights of entries
// already in `table` (unknown ids are ignored and reported in `ignored`).
std::vector<Treatment> withWeights(std::vector<Treatment> table, const std::string & spec, std::vector<std::string> * ignored = nullptr);

std::string label(const std::string & effectId); // "original" for ""

class EffectBudget {
public:
	explicit EffectBudget(std::vector<Treatment> table = defaultTable(), int maxCostCenti = kMaxEffectCostCenti);

	// Palette generation (bumped per media change): candidates are a pure
	// function of (fragment id, generation), so the same id draws
	// differently on different media but never changes while alive.
	void setGeneration(uint64_t generation) { generation_ = generation; }
	// Replaces the palette (eligibility/weights/costs) for future
	// assignments. Live fragments keep the cost they were charged.
	void setTable(std::vector<Treatment> table) { table_ = std::move(table); }
	uint64_t generation() const { return generation_; }

	// Deterministic weighted candidate, before any budget check.
	std::string candidateFor(int fragmentId) const;

	// Chooses and records the treatment for a new fragment. Never exceeds
	// the budget; falls back to a cheaper eligible shader, then Original.
	std::string assign(int fragmentId);
	// Returns the fragment's cost to the budget. Unknown ids are ignored.
	void release(int fragmentId);
	void releaseAll();

	int maxCostCenti() const { return maxCostCenti_; }
	int usedCostCenti() const { return usedCenti_; }
	int activeOriginalCount() const;
	int activeShaderCount() const;
	std::map<std::string, int> activeCostByEffect() const; // centi-units, shader effects only
	int fallbackCount() const { return fallbackCount_; } // cumulative: assigned != candidate
	int activeCount() const { return static_cast<int>(active_.size()); }
	// Treatment charged for a live fragment; false if the id is not active.
	bool activeEffectFor(int fragmentId, std::string & effectId) const;
	const std::vector<Treatment> & table() const { return table_; }
	int costOf(const std::string & effectId) const; // centi; 0 for Original, -1 if unknown

private:
	const Treatment * pick(const std::vector<const Treatment *> & options, int fragmentId, uint64_t salt) const;

	std::vector<Treatment> table_;
	int maxCostCenti_;
	uint64_t generation_ = 0;
	struct Charge {
		std::string effectId;
		int costCenti = 0;
	};
	std::unordered_map<int, Charge> active_;
	int usedCenti_ = 0;
	int fallbackCount_ = 0;
};

} // namespace blobbudget
