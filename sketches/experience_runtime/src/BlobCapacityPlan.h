#pragma once

#include <string>
#include <utility>
#include <vector>

// BlobCapacityPlan.h — Blob Experiment 2 (multi-effect capacity), the pure
// configuration/assignment half. OF-free so it is unit-testable with a bare
// compiler (test/blob_capacity_plan_tests.cpp); the runtime half
// (keys, measurement windows, logs) is BlobEffectCapacityBench.
//
// EXPERIMENT ONLY — not a production effect policy. Nothing here is reached
// unless EXPERIENCE_RUNTIME_BLOB_EFFECT_CAPACITY is set.
//
// Assignment is deterministic and per track id: slot = id mod kSlots picks
// one entry of a kSlots-long pattern, once, when the fragment is created
// (VideoRegionController::effectAssigner). A fragment therefore keeps its
// treatment for its whole lifetime, and consecutive track ids walk the
// pattern so the live mix approaches the pattern's proportions.
namespace blobcapacity {

constexpr int kSlots = 10;

// Budget = the production effect budget (BlobEffectBudget.h) assigns,
// under the bench's controlled load; the pattern is not used.
enum class AssignmentMode { Single, RoundRobin, Weighted, Budget };
const char * toString(AssignmentMode mode);

// Known status of each Blob-selectable effect, from the BLOB-002 A/B harness
// and the 2026-10-15/16 visual reviews. Benchmark data never changes these.
struct EffectInfo {
	std::string id;
	std::string blob002; // "compatible" | "outside-tolerance"
	std::string visual; // "valid" | "no-op-at-defaults" | "uv-artifacts" | "alpha-artifacts"
	bool productionCandidate() const { return blob002 == "compatible" && visual == "valid"; }
};
const std::vector<EffectInfo> & effectTable(); // same 18 ids/order as BlobSceneCore::effectChoices()
const EffectInfo * findEffect(const std::string & id);

// Named effect sets, selected by key; distinct count N uses the first N.
struct EffectSet {
	char key;
	std::string name;
	std::vector<std::string> effects;
};
const std::vector<EffectSet> & effectSets(); // q w e r t y (u = custom, from env)

struct Plan {
	std::string caseId = "A";
	int shaderSlots = 0; // of kSlots; the rest are Original
	std::string setName = "curated";
	std::vector<std::string> effectSet = { "recolor", "desaturate" };
	int distinct = 1;
	AssignmentMode mode = AssignmentMode::Single;
	// Weighted mode only: (treatment, weight); "original" means unshaded.
	std::vector<std::pair<std::string, int>> weights;

	// The first `distinct` effects of the set (clamped).
	std::vector<std::string> activeEffects() const;
	// The kSlots-long treatment pattern ("" = Original).
	std::vector<std::string> pattern() const;
	// Treatment for a track id ("" = Original). Pure function of (plan, id).
	std::string effectForId(int id) const;
	// Planned share of shaded slots (Weighted: from the pattern).
	int plannedShaderSlots() const;

	std::string banner() const; // multi-line operator banner
	std::string fields() const; // one-line key=value
};

// Matrix presets A..G (see docs/reviews/blob-effect-capacity-pi-runbook.md).
bool preset(char letter, Plan & out);

std::vector<std::string> splitList(const std::string & s, char sep);
// "original:6,recolor:2,scanlines:2" -> pairs; invalid entries skipped.
std::vector<std::pair<std::string, int>> parseWeights(const std::string & s);

// Script step: "<preset>:<seconds>", "budget:<seconds>" (production budget),
// "<effect>@<slots>:<seconds>" (single
// effect), or "<setKey>#<distinct>@<slots>:<seconds>" (round robin over a set).
struct ScriptStep {
	Plan plan;
	float seconds = 0.0f;
	std::string spec;
};
// Parses a comma-separated script; "matrix" and "scaling" expand to the
// built-in sequences. Returns false (with error) on any invalid step.
bool parseScript(const std::string & script, std::vector<ScriptStep> & out, std::string & error);

} // namespace blobcapacity
