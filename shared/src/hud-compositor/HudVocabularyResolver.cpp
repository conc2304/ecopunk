#include "HudVocabularyResolver.h"

namespace hudpresent {

namespace {

std::unordered_map<std::string, std::string> buildCanonicalDefaults() {
	return {
		// -- Universal slot captions ------------------------------------
		{"slot.scene.activity.label", "ACTIVITY"},
		{"slot.scene.timing.label", "ACTIVE TIME"},
		{"overlay.transition.label", "TRANSITION"},

		// -- Health (shared across scene.health/media.health/
		// effects.health/runtime.health — all four resolve the same
		// four-value vocabulary, since SceneHealth's meaning is
		// identical wherever it's reused) --------------------------
		{"ready", "READY"},
		{"loading", "LOADING"},
		{"degraded", "DEGRADED"},
		{"failed", "FAILED"},

		// -- SceneManagerStatus transition phases ------------------------
		{"idle", "IDLE"},
		{"fading_out", "TRANSITIONING OUT"},
		{"fading_in", "TRANSITIONING IN"},

		// -- Controls -----------------------------------------------------
		{"control.scene.previous.label", "PREV SCENE"},
		{"control.scene.next.label", "NEXT SCENE"},
		{"control.media.previous.label", "PREV MEDIA"},
		{"control.media.next.label", "NEXT MEDIA"},
		{"control.scene.reseed.label", "RESEED"},

		// -- Scene titles (displayTitleId values) ------------------------
		{"scene.blob-region-prototype.title", "REGION FIELD STUDY"},
		{"scene.contour-portrait.title", "CONTOUR PORTRAIT"},
		{"scene.temporal-fields.title", "TEMPORAL FIELDS"},
		{"scene.fragment-trail.title", "FRAGMENT TRAIL"},
		{"scene.quadrant-crosshair.title", "QUADRANT ARRAY"},
		{"scene.blueprint_emergence.title", "BLUEPRINT EMERGENCE"},

		// -- Scene-specific flexible-card/ring/channel-strip captions ----
		{"scene.blob.card.label", "REGION FIELD"},
		{"scene.fragment.card.label", "TRAIL STATUS"},
		{"scene.quadrant.channels.label", "QUADRANTS"},
		{"scene.contour.metric.breakup_progress.label", "BREAKUP"},
		{"scene.blueprint.metric.phase_progress.label", "CYCLE PROGRESS"},

		// -- Blob state IDs (probe §11.1 / Semantic Slot Model §18.1) ---
		{"scene.blob.state.analyzing", "ANALYZING"},
		{"scene.blob.state.fragmenting", "FRAGMENTING"},
		{"scene.blob.state.stable", "STABLE"},
		{"scene.blob.state.degraded", "SIGNAL DEGRADED"},

		// -- Contour state IDs (§18.2) — "reforming" deliberately left
		// undefined here so tests/studio can exercise the stable-ID
		// vocabulary fallback tier against a real, in-use key. -----------
		{"scene.contour.state.tracing", "TRACING"},
		{"scene.contour.state.displacing", "DISPLACING"},
		{"scene.contour.state.breaking", "BREAKING UP"},

		// -- Temporal state IDs (§18.3) -----------------------------------
		{"scene.temporal.state.running", "RUNNING"},
		{"scene.temporal.state.transitioning", "TRANSITIONING"},
		{"scene.temporal.state.evolving", "EVOLVING"},
		{"scene.temporal.state.regenerating", "REGENERATING"},

		// -- Temporal pattern IDs (scene.temporal.metric.pattern's
		// Identifier valueId) — bare keys, not prefixed with the metricId,
		// matching how TemporalProductionScene.cpp's patternSemanticId()
		// and shared/src/hud-compositor/fake/FakeTemporalScenario.cpp both
		// already emit this value (e.g. "particle_field", not
		// "scene.temporal.metric.pattern.particle_field") — same bare-key
		// convention already used above for health/transition-phase values
		// ("ready"/"idle"/etc.), not the metricId-prefixed convention
		// scene.fragment.metric.movement_preset.* uses. Covers the complete
		// production TFPatternType inventory (TFPatternType.h) — every
		// value TemporalSceneCore.cpp registers into TFComposition, so
		// every pattern Temporal can actually reach in production. Casing/
		// word-break style matches the effect-label convention above
		// (underscore -> space, uppercase); BSP is spelled out to "BSP
		// FIELD" since the bare 3-letter acronym alone reads as noise at
		// cinematic HUD scale, unlike the other multi-word ids.
		{"bsp", "BSP FIELD"},
		{"blob_grid", "BLOB GRID"},
		{"bands", "BANDS"},
		{"column_grid", "COLUMN GRID"},
		{"telescoping_frames", "TELESCOPING FRAMES"},
		{"particle_field", "PARTICLE FIELD"},
		{"ecological_succession", "ECOLOGICAL SUCCESSION"},
		{"network_growth", "NETWORK GROWTH"},
		{"temporal_tides", "TEMPORAL TIDES"},

		// -- Fragment state IDs (§18.4) -----------------------------------
		{"scene.fragment.state.drifting", "DRIFTING"},
		{"scene.fragment.state.scanning", "SCANNING"},
		{"scene.fragment.state.hunting", "HUNTING"},
		{"scene.fragment.state.nervous", "NERVOUS"},
		{"scene.fragment.state.orbiting", "ORBITING"},
		{"scene.fragment.state.decaying", "DECAYING"},
		{"scene.fragment.metric.movement_preset.drift", "DRIFT"},
		{"scene.fragment.metric.movement_preset.scan", "SCAN"},
		{"scene.fragment.metric.movement_preset.hunt", "HUNT"},

		// -- Quadrant state IDs (§18.5) — already visitor-shaped per the
		// probe's own finding (Observability Profile Part 5) ------------
		{"scene.quadrant.state.online", "ONLINE"},
		{"scene.quadrant.state.quiet", "QUIET PHASE"},
		{"scene.quadrant.state.standby", "STANDBY"},
		{"scene.quadrant.state.expanding", "EXPANDING"},
		{"scene.quadrant.state.reconfiguring", "RECONFIGURING"},

		// -- Blueprint state IDs (§18.6) -----------------------------------
		{"scene.blueprint.state.blank", "BLANK"},
		{"scene.blueprint.state.placing", "PLACING"},
		{"scene.blueprint.state.building", "BUILDING"},
		{"scene.blueprint.state.settling", "SETTLING"},
		{"scene.blueprint.state.dissolving", "DISSOLVING"},
		{"scene.blueprint.state.hold", "HOLDING"},

		// -- Effects (mirrors the shared video-effects catalog's
		// displayName strings, per docs/probes/…#9's precedent) ---------
		{"effect.chromatic_aberration.label", "CHROMATIC ABERRATION"},
		{"effect.bioluminescence.label", "BIOLUMINESCENCE"},
		{"effect.edge_glow.label", "EDGE GLOW"},
		{"effect.heatmap_recolor.label", "HEATMAP RECOLOR"},
		{"effect.hue_rotate.label", "HUE ROTATE"},
		{"effect.water_refraction.label", "WATER REFRACTION"},
		{"effect.ink_outlines.label", "INK OUTLINES"},
		{"effect.pixel_drift.label", "PIXEL DRIFT"},

		// -- Media titles (curated, not raw filenames) --------------------
		{"media.sample_01", "FUNGAL BLOOM STUDY"},
		{"media.sample_02", "CANOPY DRIFT"},
		{"media.sample_03", "RIVERBED SURVEY"},
	};
}

} // namespace

HudVocabularyResolver::HudVocabularyResolver() {
	setCanonicalDefaults(buildCanonicalDefaults());

	// A small alternate test pack — deliberately overrides only a couple
	// of keys, so pack-switching tests can assert both "overridden key
	// changes" and "un-overridden key still falls through to canonical".
	setPack("alt-pack", {
		{"ready", "NOMINAL"},
		{"control.scene.reseed.label", "RECONFIGURE"},
		{"scene.blob.card.label", "FIELD SCAN"},
	});

	// One demonstration scene override, exercising the highest-priority
	// resolution tier — Blob's reseed control reads "GENERATE VARIANT"
	// regardless of pack selection, per the resolution order's own
	// definition (scene override always wins).
	setSceneOverrides("blob-region-prototype", {
		{"control.scene.reseed.label", "GENERATE VARIANT"},
	});
}

void HudVocabularyResolver::setCanonicalDefaults(std::unordered_map<std::string, std::string> table) {
	canonical_ = std::move(table);
	clearCache();
}

void HudVocabularyResolver::setPack(const std::string& packId, std::unordered_map<std::string, std::string> table) {
	packs_[packId] = std::move(table);
	clearCache();
}

void HudVocabularyResolver::selectPack(const std::string& packId) {
	if (selectedPackId_ == packId) return;
	selectedPackId_ = packId;
	clearCache();
}

void HudVocabularyResolver::setSceneOverrides(const std::string& sceneId, std::unordered_map<std::string, std::string> table) {
	sceneOverrides_[sceneId] = std::move(table);
	clearCache();
}

void HudVocabularyResolver::clearSceneOverrides(const std::string& sceneId) {
	sceneOverrides_.erase(sceneId);
	clearCache();
}

void HudVocabularyResolver::clearCache() {
	cache_.clear();
}

HudVocabularyResolver::Lookup HudVocabularyResolver::lookupUncached(const std::string& sceneId, const std::string& key) const {
	if (!sceneId.empty()) {
		auto sceneIt = sceneOverrides_.find(sceneId);
		if (sceneIt != sceneOverrides_.end()) {
			auto keyIt = sceneIt->second.find(key);
			if (keyIt != sceneIt->second.end()) return {true, keyIt->second};
		}
	}
	if (!selectedPackId_.empty()) {
		auto packIt = packs_.find(selectedPackId_);
		if (packIt != packs_.end()) {
			auto keyIt = packIt->second.find(key);
			if (keyIt != packIt->second.end()) return {true, keyIt->second};
		}
	}
	auto canonicalIt = canonical_.find(key);
	if (canonicalIt != canonical_.end()) return {true, canonicalIt->second};

	return {false, key}; // stable-ID fallback
}

const std::string& HudVocabularyResolver::resolve(const std::string& sceneId, const std::string& key) const {
	const std::string cacheKey = sceneId + "\x1f" + selectedPackId_ + "\x1f" + key;
	auto it = cache_.find(cacheKey);
	if (it != cache_.end()) return it->second.value;

	Lookup result = lookupUncached(sceneId, key);
	auto inserted = cache_.emplace(cacheKey, std::move(result));
	return inserted.first->second.value;
}

bool HudVocabularyResolver::resolvedThroughVocabulary(const std::string& sceneId, const std::string& key) const {
	const std::string cacheKey = sceneId + "\x1f" + selectedPackId_ + "\x1f" + key;
	auto it = cache_.find(cacheKey);
	if (it != cache_.end()) return it->second.found;

	Lookup result = lookupUncached(sceneId, key);
	bool found = result.found;
	cache_.emplace(cacheKey, std::move(result));
	return found;
}

} // namespace hudpresent
