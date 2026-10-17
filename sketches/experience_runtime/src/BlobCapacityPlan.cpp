#include "BlobCapacityPlan.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace blobcapacity {

const char * toString(AssignmentMode mode) {
	switch (mode) {
		case AssignmentMode::Single: return "single";
		case AssignmentMode::RoundRobin: return "round_robin";
		case AssignmentMode::Weighted: return "weighted";
	}
	return "unknown";
}

const std::vector<EffectInfo> & effectTable() {
	// Order matches BlobSceneCore::effectChoices() (checked at bench setup).
	static const std::vector<EffectInfo> kTable = {
		{ "desaturate", "compatible", "valid" },
		{ "invert", "outside-tolerance", "valid" },
		{ "recolor", "compatible", "valid" },
		{ "threshold", "outside-tolerance", "valid" },
		{ "dither", "compatible", "no-op-at-defaults" },
		{ "solarize", "outside-tolerance", "valid" },
		{ "scanlines", "outside-tolerance", "uv-artifacts" },
		{ "channelshift", "compatible", "uv-artifacts" },
		{ "hue_rotate", "compatible", "no-op-at-defaults" },
		{ "ascii_solarpunk", "outside-tolerance", "alpha-artifacts" },
		{ "bioluminescence", "outside-tolerance", "uv-artifacts" },
		{ "chromatic_aberration", "outside-tolerance", "uv-artifacts" },
		{ "edge_glow", "outside-tolerance", "uv-artifacts" },
		{ "ink_outlines", "outside-tolerance", "uv-artifacts" },
		{ "pixel_drift", "outside-tolerance", "uv-artifacts" },
		{ "pixel_sorting", "outside-tolerance", "uv-artifacts" },
		{ "water_refraction", "outside-tolerance", "uv-artifacts" },
		{ "heatmap_recolor", "outside-tolerance", "valid" },
	};
	return kTable;
}

const EffectInfo * findEffect(const std::string & id) {
	for (const EffectInfo & e : effectTable()) {
		if (e.id == id) {
			return &e;
		}
	}
	return nullptr;
}

const std::vector<EffectSet> & effectSets() {
	static const std::vector<EffectSet> kSets = [] {
		std::vector<EffectSet> sets = {
			// Production-compatible under BLOB-002 and visually valid.
			{ 'q', "curated", { "recolor", "desaturate" } },
			// Single-tap, pointwise color maps.
			{ 'w', "pointwise", { "recolor", "heatmap_recolor", "solarize", "threshold", "invert", "desaturate" } },
			// Neighbor-sampling (3-9 taps).
			{ 'e', "multitap", { "channelshift", "chromatic_aberration", "bioluminescence", "ink_outlines", "edge_glow" } },
			// Time-animated / displacement.
			{ 'r', "animated", { "water_refraction", "pixel_drift", "scanlines", "hue_rotate" } },
			// Loops / large shaders.
			{ 't', "heavy", { "pixel_sorting", "dither", "ascii_solarpunk" } },
		};
		EffectSet all { 'y', "all18", {} };
		for (const EffectInfo & e : effectTable()) {
			all.effects.push_back(e.id);
		}
		sets.push_back(all);
		return sets;
	}();
	return kSets;
}

std::vector<std::string> Plan::activeEffects() const {
	int n = std::max(0, std::min(distinct, static_cast<int>(effectSet.size())));
	return std::vector<std::string>(effectSet.begin(), effectSet.begin() + n);
}

std::vector<std::string> Plan::pattern() const {
	std::vector<std::string> slots(kSlots);
	if (mode == AssignmentMode::Weighted) {
		// Smooth weighted round robin: deterministic, interleaved.
		std::vector<int> current(weights.size(), 0);
		int total = 0;
		for (const auto & w : weights) {
			total += std::max(0, w.second);
		}
		if (total <= 0) {
			return slots; // all Original
		}
		for (int s = 0; s < kSlots; s++) {
			size_t best = 0;
			for (size_t i = 0; i < weights.size(); i++) {
				current[i] += std::max(0, weights[i].second);
				if (current[i] > current[best]) {
					best = i;
				}
			}
			current[best] -= total;
			slots[s] = weights[best].first == "original" ? std::string() : weights[best].first;
		}
		return slots;
	}

	std::vector<std::string> active = activeEffects();
	int k = std::max(0, std::min(shaderSlots, kSlots));
	int rank = 0;
	for (int s = 0; s < kSlots; s++) {
		// Spread k shaded slots evenly over kSlots.
		bool shaded = ((s + 1) * k) / kSlots > (s * k) / kSlots;
		if (!shaded || active.empty()) {
			continue;
		}
		slots[s] = mode == AssignmentMode::Single ? active.front() : active[rank % active.size()];
		rank++;
	}
	return slots;
}

std::string Plan::effectForId(int id) const {
	int slot = ((id % kSlots) + kSlots) % kSlots;
	return pattern()[slot];
}

int Plan::plannedShaderSlots() const {
	int n = 0;
	for (const std::string & s : pattern()) {
		n += s.empty() ? 0 : 1;
	}
	return n;
}

namespace {
	std::string tag(const std::string & id) {
		const EffectInfo * info = findEffect(id);
		if (info == nullptr) {
			return id + "[unknown]";
		}
		if (info->productionCandidate()) {
			return id + "[candidate]";
		}
		return id + "[diagnostic:" + (info->blob002 != "compatible" ? info->blob002 : info->visual) + "]";
	}

	std::string join(const std::vector<std::string> & v, const char * sep) {
		std::string out;
		for (size_t i = 0; i < v.size(); i++) {
			out += (i ? sep : "") + v[i];
		}
		return out;
	}
}

std::string Plan::banner() const {
	std::vector<std::string> tagged;
	std::vector<std::string> slots = pattern();
	if (mode == AssignmentMode::Weighted) {
		for (const auto & w : weights) {
			tagged.push_back((w.first == "original" ? std::string("original") : tag(w.first)) + "x" + std::to_string(w.second));
		}
	} else {
		for (const std::string & e : activeEffects()) {
			tagged.push_back(tag(e));
		}
	}
	std::vector<std::string> shown;
	for (const std::string & s : slots) {
		shown.push_back(s.empty() ? std::string("O") : s);
	}
	int shaded = plannedShaderSlots();
	std::ostringstream ss;
	ss << "[BLOB EFFECT CAPACITY]\n"
	   << "case=" << caseId << "\n"
	   << "shaderTarget=" << shaded << "/" << kSlots << " slots\n"
	   << "originalTarget=" << (kSlots - shaded) << "/" << kSlots << " slots\n"
	   << "distinctShaders=" << (mode == AssignmentMode::Weighted ? -1 : static_cast<int>(activeEffects().size())) << "\n"
	   << "effectSet=" << setName << "\n"
	   << "effects=" << join(tagged, ",") << "\n"
	   << "assignment=" << toString(mode) << "\n"
	   << "pattern(id%10)=" << join(shown, ",");
	return ss.str();
}

std::string Plan::fields() const {
	std::vector<std::string> effects;
	if (mode == AssignmentMode::Weighted) {
		for (const auto & w : weights) {
			effects.push_back(w.first + ":" + std::to_string(w.second));
		}
	} else if (plannedShaderSlots() > 0) {
		effects = activeEffects();
	}
	std::ostringstream ss;
	ss << "case=" << caseId << " mode=" << toString(mode) << " set=" << setName
	   << " shaderSlots=" << plannedShaderSlots() << " distinctTarget=" << (mode == AssignmentMode::Weighted ? -1 : static_cast<int>(activeEffects().size()))
	   << " effects=" << (effects.empty() ? std::string("-") : join(effects, "+"));
	return ss.str();
}

namespace {
	const EffectSet * setByKey(char key) {
		for (const EffectSet & s : effectSets()) {
			if (s.key == key) {
				return &s;
			}
		}
		return nullptr;
	}

	Plan fromSet(const std::string & caseId, char setKey, int distinct, int slots, AssignmentMode mode) {
		Plan p;
		const EffectSet * set = setByKey(setKey);
		p.caseId = caseId;
		p.setName = set->name;
		p.effectSet = set->effects;
		p.distinct = distinct;
		p.shaderSlots = slots;
		p.mode = mode;
		return p;
	}
}

bool preset(char letter, Plan & out) {
	switch (letter) {
		case 'A': out = fromSet("A", 'q', 1, 0, AssignmentMode::Single); return true; // 10 Original
		case 'B': out = fromSet("B", 'q', 1, 10, AssignmentMode::Single); return true; // 10 Recolor
		case 'C': out = fromSet("C", 'q', 1, 5, AssignmentMode::Single); return true; // 5 Original + 5 Recolor
		case 'D': out = fromSet("D", 'w', 2, 5, AssignmentMode::RoundRobin); return true; // 5 Original + 5 shaded, 2 shaders
		case 'E': out = fromSet("E", 'w', 4, 8, AssignmentMode::RoundRobin); return true; // 2 Original + 8 shaded, 4 shaders
		case 'F': out = fromSet("F", 'q', 2, 10, AssignmentMode::RoundRobin); return true; // all shaded, max compatible set
		case 'G': out = fromSet("G", 'y', 18, 10, AssignmentMode::RoundRobin); return true; // diagnostic stress, all 18
		default: return false;
	}
}

std::vector<std::string> splitList(const std::string & s, char sep) {
	std::vector<std::string> out;
	std::string cur;
	std::istringstream in(s);
	while (std::getline(in, cur, sep)) {
		cur.erase(0, cur.find_first_not_of(" \t"));
		cur.erase(cur.find_last_not_of(" \t") + 1);
		if (!cur.empty()) {
			out.push_back(cur);
		}
	}
	return out;
}

std::vector<std::pair<std::string, int>> parseWeights(const std::string & s) {
	std::vector<std::pair<std::string, int>> out;
	for (const std::string & entry : splitList(s, ',')) {
		size_t colon = entry.find(':');
		if (colon == std::string::npos) {
			continue;
		}
		std::string id = entry.substr(0, colon);
		int w = std::atoi(entry.substr(colon + 1).c_str());
		if (w <= 0 || (id != "original" && findEffect(id) == nullptr)) {
			continue;
		}
		out.emplace_back(id, w);
	}
	return out;
}

namespace {
	bool parseStep(const std::string & token, ScriptStep & step, std::string & error) {
		size_t colon = token.rfind(':');
		if (colon == std::string::npos) {
			error = "missing ':<seconds>' in '" + token + "'";
			return false;
		}
		std::string spec = token.substr(0, colon);
		float seconds = static_cast<float>(std::atof(token.substr(colon + 1).c_str()));
		if (seconds <= 0.0f) {
			error = "bad duration in '" + token + "'";
			return false;
		}
		step.spec = spec;
		step.seconds = seconds;

		if (spec.size() == 1 && preset(spec[0], step.plan)) {
			return true;
		}
		size_t at = spec.find('@');
		if (at == std::string::npos) {
			error = "unknown step '" + spec + "'";
			return false;
		}
		int slots = std::atoi(spec.substr(at + 1).c_str());
		if (slots < 0 || slots > kSlots) {
			error = "slots out of range in '" + spec + "'";
			return false;
		}
		std::string head = spec.substr(0, at);
		size_t hash = head.find('#');
		if (hash != std::string::npos) {
			const EffectSet * set = head.size() > 0 ? setByKey(head[0]) : nullptr;
			int distinct = std::atoi(head.substr(hash + 1).c_str());
			if (set == nullptr || hash != 1 || distinct < 1) {
				error = "bad set spec '" + spec + "'";
				return false;
			}
			step.plan = fromSet(spec, head[0], distinct, slots, AssignmentMode::RoundRobin);
			return true;
		}
		if (findEffect(head) == nullptr) {
			error = "unknown effect '" + head + "'";
			return false;
		}
		Plan p;
		p.caseId = spec;
		p.setName = "single:" + head;
		p.effectSet = { head };
		p.distinct = 1;
		p.shaderSlots = slots;
		p.mode = AssignmentMode::Single;
		step.plan = p;
		return true;
	}
}

bool parseScript(const std::string & script, std::vector<ScriptStep> & out, std::string & error) {
	out.clear();
	std::vector<std::string> tokens;
	for (const std::string & t : splitList(script, ',')) {
		if (t == "matrix") {
			for (const char * c : { "A:40", "B:40", "C:40", "D:40", "E:40", "F:40", "G:40" }) {
				tokens.push_back(c);
			}
		} else if (t == "scaling") {
			for (const char * fx : { "recolor", "heatmap_recolor", "edge_glow", "water_refraction", "pixel_sorting" }) {
				for (int n : { 2, 4, 6, 8, 10 }) {
					tokens.push_back(std::string(fx) + "@" + std::to_string(n) + ":30");
				}
			}
		} else if (t == "distinct") {
			// Similar-cost pointwise shaders isolate switching from per-shader
			// cost; the all-18 step is the diagnostic worst case.
			for (const char * d : { "w#1@10:30", "w#2@10:30", "w#4@10:30", "w#6@10:30", "y#18@10:30" }) {
				tokens.push_back(d);
			}
		} else {
			tokens.push_back(t);
		}
	}
	for (const std::string & t : tokens) {
		ScriptStep step;
		if (!parseStep(t, step, error)) {
			return false;
		}
		out.push_back(step);
	}
	if (out.empty()) {
		error = "empty script";
		return false;
	}
	return true;
}

} // namespace blobcapacity
