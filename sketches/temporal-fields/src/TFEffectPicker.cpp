#include "TFEffectPicker.h"
#include "DefaultVideoEffectCatalog.h"
#include "EffectKnowledgePack.h"
#include "EffectKnowledgePrecedence.h"
#include "EffectLevelKnowledge.h"
#include "EffectPresetId.h"
#include "TFRandom.h"
#include "TFTextureCropFill.h"
#include "VideoEffectRegistry.h"
#include "VideoEffectTypes.h"
#include "ofColor.h"
#include "ofGraphics.h"
#include "ofLog.h"
#include "ofMath.h"
#include "ofUtils.h"
#include <cmath>
#include <utility>
#include <vector>

namespace {
	// Lazily-built, process-lifetime registry of the canonical Contract-A
	// single-pass catalog — see applyEffectUniforms()'s fallback branch
	// below. Mirrors shared/src/VideoRegionEffectRenderer.cpp's identical helper.
	const videoeffects::VideoEffectRegistry & tfCatalogRegistry() {
		static videoeffects::VideoEffectRegistry registry = [] {
			videoeffects::VideoEffectRegistry r;
			videoeffects::registerSinglePassEffects(r);
			return r;
		}();
		return registry;
	}
}

namespace {
	// Bounded retry, mirroring EffectRandomizer::generate()'s own
	// reject/retry cap (docs/shader-effect-system-probe.md §9) rather than
	// inventing a new policy -- never blocks forever, always terminates by
	// accepting whatever the last attempt landed on.
	constexpr int kBlacklistAvoidanceMaxAttempts = 3;
	constexpr float kBlacklistMatchEpsilon = 1e-4f;

	// Same content-equality rule as EffectKnowledgeBase::isDuplicate (exact
	// key set + all values within epsilon) -- deliberately not exposed by
	// EffectKnowledgeBase itself (it's private), so this is a narrow,
	// independent re-implementation scoped to this one call site rather
	// than a shared-header change.
	bool snapshotMatchesEntry(const std::map<std::string, float>& snapshot, const videoeffects::KnowledgeEntry& entry) {
		if (snapshot.empty() || entry.snapshot.size() != snapshot.size()) return false;
		for (const auto& kv : snapshot) {
			auto it = entry.snapshot.find(kv.first);
			if (it == entry.snapshot.end() || std::fabs(it->second - kv.second) > kBlacklistMatchEpsilon) return false;
		}
		return true;
	}

	// Matches EffectSceneCompatibility.cpp's own scene-id string exactly
	// (shared/src/video-effects/knowledge/EffectSceneCompatibility.cpp) --
	// this is a DIFFERENT, narrower mechanism (that file tracks whether a
	// scene consumes the shared service at all; this constant is the scene
	// id resolveCompatibility() checks a specific preset/effect-default
	// against), but reuses the identical string so the two never drift
	// apart into two different spellings of "temporal-fields."
	const std::string kSceneId = "temporal-fields";
}

void TFEffectPicker::setup(ShaderLibrary* shaderLib_) {
	shaderLib = shaderLib_;

	// Shared Effect Knowledge integration -- Engineering Session 2's first
	// real proof target, see docs/temporal-fields-knowledge-pack-integration.md.
	// This bin/data-relative path is now a real, populated file (Shared
	// Effects Architecture-Closure Session, DEC-016): the canonical
	// AUTHORED copy lives at assets/shared/video-effects/knowledge/
	// effect-knowledge-pack.json (written by shader-effect-debugger's [k]
	// export action), and scripts/sync-video-effect-assets.py copies it
	// into this exact path on every sync run -- see that script's
	// sync_knowledge_pack()/discover_knowledge_only_sketches(), and
	// docs/shared-effect-knowledge-schema-v1.md's "Canonical path/
	// distribution evidence" section for the real end-to-end proof
	// (debugger export -> sync -> this import, all real files, no mocks).
	// An absent pack (sync never run, or nothing exported yet) still
	// degrades safely to an empty knowledgeBase -- every check below that
	// consults it is a no-op in that case, so pickNext()'s behavior is
	// unaffected either way.
	videoeffects::EffectKnowledgePackImportReport report = videoeffects::importEffectKnowledgePack(
		"shared-video-effects/knowledge/effect-knowledge-pack.json", knowledgeBase, tfCatalogRegistry().allIds());
	if (!report.ok) {
		ofLogWarning("TFEffectPicker") << "shared effect knowledge pack present but rejected (schemaVersion "
										<< report.schemaVersion << ", versionSupported=" << report.versionSupported
										<< ") -- proceeding exactly as if no pack existed";
	} else if (report.importedWhitelist > 0 || report.importedBlacklist > 0) {
		ofLogNotice("TFEffectPicker") << "shared effect knowledge: imported " << report.importedWhitelist << " whitelist + "
									   << report.importedBlacklist << " blacklist entries";
	}

	pickNext(); // don't sit empty until the first cycleInterval elapses
}

void TFEffectPicker::update(float dt) {
	timer += dt;
	if (timer >= weights.cycleInterval) {
		timer = 0.0f;
		pickNext();
	}
}

void TFEffectPicker::pickNext() {
	std::vector<std::pair<std::string, float>> options;
	options.push_back({ "", weights.rawWeight });
	for (auto& kv : weights.effectWeights) {
		options.push_back({ kv.first, kv.second });
	}

	// Preserves 100% of the existing selection/randomization behavior when
	// knowledgeBase is empty (the common case today, see setup()'s
	// comment): the loop always runs at least once, and an empty blacklist
	// for the picked effect means `blocked` never becomes true, so the
	// very first attempt is accepted exactly as pickNext() always did
	// before this change.
	for (int attempt = 0; attempt < kBlacklistAvoidanceMaxAttempts; ++attempt) {
		currentEffect = tfWeightedPick(options);

		if (currentEffect.empty()) {
			randomizeEffectParams(currentEffect); // zeroes paramX..W for "Raw" -- unchanged
			lastPickUsedCanonicalPreset_ = false;
			lastAppliedPresetId_.reset();
			break; // "Raw" has no params to check against blacklist entries, and no preset to apply
		}

		// Shared Effect Knowledge v1 Freeze Policy (DEC-016) production-
		// selector adoption: prefer a real, canonically-eligible authored
		// preset for the effect weights.effectWeights already chose above.
		// See applyEligibleCanonicalPreset()'s own header comment for exactly
		// which rules gate this and why WHICH EFFECT is chosen is unaffected.
		lastPickUsedCanonicalPreset_ = applyEligibleCanonicalPreset(currentEffect);
		if (!lastPickUsedCanonicalPreset_) {
			lastAppliedPresetId_.reset();
			randomizeEffectParams(currentEffect); // pre-this-increment fallback, unchanged
		}

		std::map<std::string, float> snapshot = currentParamSnapshot(currentEffect);
		if (snapshot.empty()) break; // nothing this class snapshots for this effect -- nothing to compare

		bool blocked = false;
		for (const auto& entry : knowledgeBase.loadBlacklist(currentEffect)) {
			if (snapshotMatchesEntry(snapshot, entry)) {
				blocked = true;
				break;
			}
		}
		if (!blocked) break;

		ofLogNotice("TFEffectPicker") << "re-picking: '" << currentEffect
									   << "' landed on a known-blacklisted parameter combination (attempt "
									   << (attempt + 1) << "/" << kBlacklistAvoidanceMaxAttempts << ")";
	}
}

bool TFEffectPicker::applyEligibleCanonicalPreset(const std::string& effectName) {
	// WHICH EFFECT to render remains weights.effectWeights' own local
	// policy (unaffected by this method) -- gating effect-SELECTION itself
	// on eligibility was deliberately rejected: no EffectLevelKnowledge
	// record exists yet for any of this class's 7 randomized effects in
	// the real canonical pack, so requiring "Allowed" at the effect-
	// selection layer would make every one of them Unclassified-and-
	// therefore-ineligible today, collapsing effectWeights' whole candidate
	// pool to "Raw" -- a large, sudden, real behavior change this
	// increment's own migration-risk guidance says to avoid ("do not
	// silently broaden eligibility... document any visible selection-rate
	// change" implies the inverse too: don't silently CRUSH it either, for
	// a scene that never asked to be filtered). This method instead governs
	// only which PARAMETER VALUES an already-chosen effect uses.
	std::vector<videoeffects::KnowledgeEntry> candidates = knowledgeBase.loadWhitelist(effectName);
	if (candidates.empty()) return false;

	std::optional<videoeffects::EffectLevelKnowledge> effectDefault = knowledgeBase.loadEffectLevelKnowledge(effectName);
	const videoeffects::EffectLevelKnowledge* effectDefaultPtr = effectDefault.has_value() ? &(*effectDefault) : nullptr;

	// Production Selector / Eligibility Increment 2: loaded once, up
	// front, so blocked-ness is a HARD CANDIDATE-FILTERING criterion --
	// checked before a blocked entry can ever reach `eligible`, not left
	// to the pre-existing post-hoc bounded retry loop in pickNext() (that
	// loop still exists for the fallback-randomized path and as defense
	// in depth here, but a canonical authored preset that is also
	// blacklisted must never be a candidate at all, per this increment's
	// own "close the strongest safety gap" mandate -- with only one
	// eligible-and-blocked candidate, the OLD design could exhaust the
	// retry budget and apply it anyway; this filter makes that
	// structurally impossible instead of merely unlikely).
	std::vector<videoeffects::KnowledgeEntry> blockedEntries = knowledgeBase.loadBlacklist(effectName);

	std::vector<const videoeffects::KnowledgeEntry*> eligible;
	for (const auto& entry : candidates) {
		// Legacy anonymous presets (no stable presetId) remain usable for
		// this class's existing content-match blacklist-avoidance path, but
		// are never automatic-production-selection-eligible per DEC-016 --
		// see EffectPresetId.h's isReusableAuthoredPreset().
		if (!videoeffects::isReusableAuthoredPreset(entry)) continue;

		videoeffects::KnowledgeClassification classification =
			videoeffects::resolveCompatibility(entry, effectDefaultPtr, kSceneId);
		if (!videoeffects::isEligibleForAutomaticProductionSelection(classification)) continue;

		// Hard exclusion: hierarchy is reusable AND compatible AND NOT
		// blocked -- favored/weight preference (the uniform pick below)
		// only ever runs over what survives this filter, so blocked can
		// never win via favored status or (once weights exist) higher
		// weight, per this increment's own required precedence.
		bool blocked = false;
		for (const auto& blockedEntry : blockedEntries) {
			if (snapshotMatchesEntry(entry.snapshot, blockedEntry)) {
				blocked = true;
				break;
			}
		}
		if (blocked) continue;

		eligible.push_back(&entry);
	}
	if (eligible.empty()) return false;

	// "If only whitelist/favored membership exists, use the existing
	// behavior" -- no numeric per-preset weight field exists in this
	// schema (Shared Effects Production Selector task, Step 3), so a
	// uniform pick among the eligible (i.e. favored-by-whitelist-
	// membership) set is the existing behavior this adopts, not a new
	// policy invented here.
	std::size_t index = static_cast<std::size_t>(ofRandom(0.0f, static_cast<float>(eligible.size())));
	if (index >= eligible.size()) index = eligible.size() - 1; // ofRandom's upper bound is exclusive in practice, but clamp defensively
	const videoeffects::KnowledgeEntry* chosen = eligible[index];

	applyParamsFromSnapshot(effectName, chosen->snapshot);
	lastAppliedPresetId_ = chosen->presetId;
	ofLogNotice("TFEffectPicker") << "applying canonical eligible preset '"
								   << (chosen->presetId.has_value() ? *chosen->presetId : std::string("(no presetId?)"))
								   << "' for effect '" << effectName << "'";
	return true;
}

void TFEffectPicker::applyParamsFromSnapshot(const std::string& name, const std::map<std::string, float>& snapshot) {
	// Inverse of currentParamSnapshot()'s key mapping -- see that method's
	// own comment for why these exact keys (canonical
	// VideoEffectDefinition parameter ids) and not paramX..W names.
	paramX = paramY = paramZ = paramW = 0.0f;
	auto get = [&](const std::string& key, float fallback) {
		auto it = snapshot.find(key);
		return it != snapshot.end() ? it->second : fallback;
	};
	if (name == "dither") {
		paramX = get("alpha", paramX);
		paramY = get("maxPixelation", paramY);
	} else if (name == "threshold") {
		paramX = get("threshold", paramX);
	} else if (name == "channelshift") {
		paramX = get("shift", paramX);
	} else if (name == "hue_rotate") {
		paramX = get("hueOffset", paramX);
		paramY = get("hueSpeed", paramY);
		paramZ = get("saturationMult", paramZ);
		paramW = get("valueMult", paramW);
	} else if (name == "pixel_sorting") {
		paramX = get("threshold", paramX);
		paramY = get("direction", paramY);
	} else if (name == "heatmap_recolor") {
		paramX = get("gamma", paramX);
		paramY = get("minLuminance", paramY);
		paramZ = get("maxLuminance", paramZ);
		// palette/reverse (paramW's packed encoding) intentionally not
		// sourced from a canonical preset -- same limitation as
		// currentParamSnapshot()'s own comment on this effect.
	}
	// "recolor": tint is vector-typed, never representable in a scalar
	// snapshot (see currentParamSnapshot()'s own comment) -- never applied
	// from a canonical preset; applyEligibleCanonicalPreset() may still
	// return true for a "recolor" entry with an empty/irrelevant snapshot,
	// which would leave paramX..W at 0 -- Session evidence (see this
	// increment's report) confirms no "recolor" whitelist entry with a
	// meaningful scalar snapshot exists in the real canonical pack today,
	// so this is a documented latent gap, not silently masked.
}

std::map<std::string, float> TFEffectPicker::currentParamSnapshot(const std::string& name) const {
	// Keys here MUST match the canonical VideoEffectDefinition's real
	// parameter ids (shared/src/video-effects/catalog/DefaultVideoEffectCatalog.cpp)
	// -- a shader-effect-debugger-authored blacklist entry is keyed by
	// those ids, not by this class's internal paramX..W naming. Verified
	// against that file directly, not assumed.
	std::map<std::string, float> snapshot;
	if (name == "dither") {
		snapshot["alpha"] = paramX; // "Arc position", per dither.glsl's own comment -- not opacity
		snapshot["maxPixelation"] = paramY;
	} else if (name == "threshold") {
		snapshot["threshold"] = paramX;
	} else if (name == "recolor") {
		// recolor's only randomized parameter is "tint", a Vec3 --
		// KnowledgeEntry::snapshot is scalar-only (map<string,float>) and
		// deliberately does not cover vector-typed parameters in v1 (see
		// EffectRandomizer.h's own comment on this same limitation). There
		// is currently no way to represent a blacklisted tint at all, so
		// intentionally nothing is returned here rather than fabricating a
		// key ("tint.r" or similar) that doesn't exist in the real schema
		// and could never legitimately match an authored entry.
	} else if (name == "channelshift") {
		snapshot["shift"] = paramX;
	} else if (name == "hue_rotate") {
		snapshot["hueOffset"] = paramX;
		snapshot["hueSpeed"] = paramY;
		snapshot["saturationMult"] = paramZ;
		snapshot["valueMult"] = paramW;
	} else if (name == "pixel_sorting") {
		snapshot["threshold"] = paramX;
		snapshot["direction"] = paramY;
	} else if (name == "heatmap_recolor") {
		snapshot["gamma"] = paramX;
		snapshot["minLuminance"] = paramY;
		snapshot["maxLuminance"] = paramZ;
		// paramW packs paletteIndex + reverse-flag into one float (see
		// randomizeEffectParams()) -- not decomposed into "palette"/
		// "reverse" here, since a debugger-authored entry would use those
		// two separate real ids, not this class's packed encoding. Left
		// out rather than compared against the wrong keys.
	}
	return snapshot;
}

videoeffects::EffectActivityStatus TFEffectPicker::activityStatus() const {
	videoeffects::EffectActivityStatus status;

	// Health, independent of which effect (if any) is selected below: this
	// class's only real "something isn't working as expected" condition is
	// a picked effect whose shader failed to load -- the exact same check
	// drawCurrent() already makes for its own raw-draw fallback (see that
	// method). Mirrors deriveEffectHealth()'s "runs, but not with what was
	// expected" Degraded case (shared/src/video-effects/knowledge/
	// EffectActivityStatus.cpp) without needing a VideoEffectLoadReport --
	// this class never owned one (it uses the bare VideoEffectRegistry for
	// parameter defaults and its own ShaderLibrary, never VideoEffectService
	// -- see docs/shared-effect-knowledge-engineering-session-2-verification.md's
	// Blob/Temporal ownership findings for why no VideoEffectLoadReport is
	// available here to begin with). Raw/No-Effect (currentEffect.empty())
	// short-circuits this to Ready -- an empty selection is never a failure.
	if (!currentEffect.empty() && (shaderLib == nullptr || !shaderLib->has(currentEffect))) {
		status.health = videoeffects::EffectHealth::Degraded;
		status.messageId = "effect.shader_unavailable"; // stable id, never raw text -- same discipline as SceneHudStatus::message
	}

	if (currentEffect.empty()) {
		return status; // "Raw / No Effect" -- zero slots, not a slot with an empty id
	}

	videoeffects::EffectActivitySlot slot;
	slot.slotId = "temporal_fields.background";
	slot.effectId = currentEffect;
	const videoeffects::VideoEffectDefinition* def = tfCatalogRegistry().getDefinition(currentEffect);
	slot.displayName = (def != nullptr && !def->displayName.empty()) ? def->displayName : currentEffect;
	slot.phase = videoeffects::EvolutionPhase::Holding; // hard-cut picker, see this method's header comment
	slot.transitionProgress01 = 1.0f;
	slot.prominence = 1.0f; // sole content layer in FULL_VIDEO mode -- no competing slot to rank against
	status.slots.push_back(std::move(slot));
	return status;
}

void TFEffectPicker::randomizeEffectParams(const std::string& name) {
	paramX = paramY = paramZ = paramW = 0.0f;

	if (name == "dither") {
		paramX = ofRandom(0.15f, 0.85f);
		paramY = ofRandom(2.0f, 10.0f);
	} else if (name == "threshold") {
		paramX = ofRandom(0.35f, 0.65f);
	} else if (name == "recolor") {
		ofColor c = ofColor::fromHsb(static_cast<int>(ofRandom(0, 255)), 200, 255);
		paramX = c.r / 255.0f;
		paramY = c.g / 255.0f;
		paramZ = c.b / 255.0f;
	} else if (name == "channelshift") {
		paramX = ofRandom(0.002f, 0.01f);
	} else if (name == "hue_rotate") {
		paramX = ofRandom(0.0f, 360.0f);
		paramY = ofRandom(15.0f, 60.0f) * (ofRandom(1.0f) < 0.5f ? -1.0f : 1.0f);
		paramZ = ofRandom(0.8f, 1.3f);
		paramW = ofRandom(0.9f, 1.1f);
	} else if (name == "pixel_sorting") {
		paramX = ofRandom(0.3f, 0.8f);
		paramY = ofRandom(0.0f, 1.0f) < 0.5f ? 0.0f : 1.0f;
	} else if (name == "heatmap_recolor") {
		// paramW packs palette index + reverse flag (paletteIndex + 0.5 if
		// reversed) — mirrors blueprint_emergence's BEFragment.cpp encoding,
		// since only 4 float slots exist here too.
		paramX = ofRandom(0.7f, 1.3f);           // gamma
		paramY = ofRandom(0.0f, 0.12f);          // minLuminance
		paramZ = ofRandom(0.88f, 1.0f);          // maxLuminance
		int paletteIdx = static_cast<int>(ofRandom(4.0f));
		bool reversePalette = ofRandom(1.0f) < 0.2f;
		paramW = paletteIdx + (reversePalette ? 0.5f : 0.0f);
	}
}

void TFEffectPicker::drawCurrent(const ofTexture& sourceTex, const ofRectangle& destRect, float alpha) {
	if (!sourceTex.isAllocated() || destRect.width <= 0 || destRect.height <= 0) {
		return;
	}

	if (currentEffect.empty() || shaderLib == nullptr || !shaderLib->has(currentEffect)) {
		tfDrawTextureCroppedToFill(sourceTex, destRect, alpha);
		return;
	}

	int w = static_cast<int>(destRect.width);
	int h = static_cast<int>(destRect.height);

	if (!sourceFbo.isAllocated() || static_cast<int>(sourceFbo.getWidth()) != w
		|| static_cast<int>(sourceFbo.getHeight()) != h) {
		ofFbo::Settings s;
		s.width = w;
		s.height = h;
		s.internalformat = GL_RGBA;
		s.useDepth = false;
		sourceFbo.allocate(s);
		resultFbo.allocate(s);
	}

	// Stage 1: re-render the crop into a clean FBO with texcoords spanning
	// [0,1] over its own w x h — several effects (dither's Bayer grid,
	// ascii's cell grid, pixel snapping) assume that, which a direct
	// drawSubsection() of an arbitrary crop sub-rect doesn't give them. See
	// this class's header comment / BEFragment.cpp:330-347.
	ofRectangle srcCrop = tfComputeCropFillSrcRect(sourceTex.getWidth(), sourceTex.getHeight(), destRect);
	sourceFbo.begin();
	ofClear(0, 0, 0, 0);
	ofSetColor(255);
	sourceTex.drawSubsection(0, 0, static_cast<float>(w), static_cast<float>(h),
		srcCrop.x, srcCrop.y, srcCrop.width, srcCrop.height);
	sourceFbo.end();

	// Stage 2: run the selected shader against that clean crop.
	ofShader& sh = shaderLib->get(currentEffect);
	resultFbo.begin();
	ofClear(0, 0, 0, 0);
	sh.begin();
	sh.setUniformTexture("tex", sourceFbo.getTexture(), 0);
	sh.setUniformTexture("tex0", sourceFbo.getTexture(), 0);
	sh.setUniform2f("resolution", static_cast<float>(w), static_cast<float>(h));
	applyEffectUniforms(sh, currentEffect, static_cast<float>(w), static_cast<float>(h));
	ofSetColor(255);
	sourceFbo.getTexture().draw(0, 0, static_cast<float>(w), static_cast<float>(h));
	sh.end();
	resultFbo.end();

	// Stage 3: composite at the destination with a plain textured draw so
	// `alpha` blends correctly even for shaders with no alpha uniform of
	// their own (same reasoning as BEFragment.cpp:363-366).
	ofSetColor(255, 255, 255, static_cast<int>(ofClamp(alpha, 0.0f, 1.0f) * 255));
	resultFbo.getTexture().draw(destRect.x, destRect.y, destRect.width, destRect.height);
	ofSetColor(255);
}

void TFEffectPicker::applyEffectUniforms(ofShader& sh, const std::string& name, float w, float h) const {
	if (name == "dither") {
		sh.setUniform1f("alpha", paramX);
		sh.setUniform1f("opacity", 1.0f);
		sh.setUniform1f("maxPixelation", paramY);
	} else if (name == "threshold") {
		sh.setUniform1f("threshold", paramX);
		sh.setUniform1f("alpha", 1.0f);
	} else if (name == "recolor") {
		sh.setUniform3f("tint", paramX, paramY, paramZ);
		sh.setUniform1f("alpha", 1.0f);
	} else if (name == "channelshift") {
		sh.setUniform1f("shift", paramX);
		sh.setUniform1f("alpha", 1.0f);
	} else if (name == "hue_rotate") {
		sh.setUniform1f("hueOffset", paramX);
		sh.setUniform1f("hueSpeed", paramY);
		sh.setUniform1f("time", ofGetElapsedTimef());
		sh.setUniform1f("saturationMult", paramZ);
		sh.setUniform1f("valueMult", paramW);
		sh.setUniform1f("alpha", 1.0f);
	} else if (name == "pixel_sorting") {
		sh.setUniform1f("threshold", paramX);
		sh.setUniform1f("rangePx", 12.0f);
		sh.setUniform1f("direction", paramY);
		sh.setUniform1f("intensity", 1.0f);
	} else if (name == "heatmap_recolor") {
		sh.setUniform1f("alpha", 1.0f);
		sh.setUniform1f("intensity", 1.0f);
		sh.setUniform1f("gamma", paramX);
		sh.setUniform1f("minLuminance", paramY);
		sh.setUniform1f("maxLuminance", paramZ);
		int paletteIdx = static_cast<int>(paramW);
		bool reversePalette = (paramW - paletteIdx) > 0.25f;
		sh.setUniform1i("palette", paletteIdx);
		sh.setUniform1i("reverse", reversePalette ? 1 : 0);
	} else {
		// Every remaining effect (invert/solarize/scanlines, ascii_solarpunk,
		// bioluminescence, chromatic_aberration, edge_glow, ink_outlines,
		// pixel_drift, water_refraction) has no per-instance randomized
		// state — same fixed literals every time — so it binds from the
		// canonical catalog (shared/src/video-effects/catalog/DefaultVideoEffectCatalog.h)
		// instead of duplicating those literals a third time (see
		// docs/shader-effect-system-probe.md §10). Effects with real
		// per-instance randomization stay above as explicit branches reading
		// paramX..W.
		const videoeffects::VideoEffectDefinition * def = tfCatalogRegistry().getDefinition(name);
		if (def != nullptr) {
			for (const auto & param : def->params) {
				if (param.id == "alpha") {
					sh.setUniform1f("alpha", 1.0f);
					continue;
				}
				switch (param.type) {
					case videoeffects::VideoEffectParameterType::Float:
						sh.setUniform1f(param.id, videoeffects::asFloat(param.defaultValue));
						break;
					case videoeffects::VideoEffectParameterType::Int:
						sh.setUniform1i(param.id, videoeffects::asInt(param.defaultValue));
						break;
					case videoeffects::VideoEffectParameterType::Bool:
						sh.setUniform1i(param.id, videoeffects::asBool(param.defaultValue) ? 1 : 0);
						break;
					case videoeffects::VideoEffectParameterType::Vec3: {
						glm::vec3 v = videoeffects::asVec3(param.defaultValue);
						sh.setUniform3f(param.id, v.x, v.y, v.z);
						break;
					}
					default:
						break;
				}
			}
			sh.setUniform1f("time", ofGetElapsedTimef());
		}
	}
	(void)w;
	(void)h;
}
