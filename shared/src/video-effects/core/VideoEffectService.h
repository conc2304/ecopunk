#pragma once

#include "../knowledge/EffectKnowledgeBase.h"
#include "../knowledge/EffectRandomizer.h"
#include "VideoEffectAssetRegistry.h"
#include "VideoEffectInstance.h"
#include "VideoEffectLoadReport.h"
#include "VideoEffectRegistry.h"
#include <memory>
#include <string>

// Sketch-facing entry point per docs/shared-video-effect-architecture.md §5.
// Owned as a plain member by each consuming sketch's ofApp (or an
// equivalent top-level owner) — matching how ShaderLibrary/MotionExtraction
// are owned today, per §13's ownership boundary: this is explicitly NOT a
// singleton. Each sketch gets the full canonical registry
// (registerDefaultVideoEffects, DefaultVideoEffectCatalog.h) and applies its
// own effect-manifest.json on top for asset-sync/load-report purposes.
namespace videoeffects {

	struct VideoEffectServiceConfig {
		std::string manifestPath = "effect-manifest.json"; // resolved via ofToDataPath
		std::string managedAssetSubdir = "shared-video-effects";
		std::string knowledgeDataDir = "knowledge";
	};

	struct VideoEffectInstanceConfig {
		// Reserved for future per-instance construction options. Intentionally
		// empty today — avoid speculative fields with no current caller.
	};

	class VideoEffectService {
	public:
		// Populates the canonical registry (registerDefaultVideoEffects) and
		// validates this sketch's effect-manifest.json against it, building
		// getLoadReport(). Returns false only if the registry itself failed to
		// populate (e.g. a duplicate-id bug in the catalog) — a missing or
		// unreadable manifest is not fatal, it just yields an empty load
		// report (equivalent to "no effects declared yet").
		bool setup(const VideoEffectServiceConfig & config = {});

		bool hasEffect(const std::string & effectId) const;
		const VideoEffectDefinition * getDefinition(const std::string & effectId) const;

		// Creates a fresh instance and calls its setup(). Returns nullptr (and
		// logs) if the id is unknown or the instance's own setup() fails
		// (e.g. a shader failed to compile) — callers must null-check, same
		// discipline ShaderLibrary::has()/get() already requires today.
		std::unique_ptr<VideoEffectInstance> createInstance(const std::string & effectId, const VideoEffectInstanceConfig & instanceConfig = {});

		const VideoEffectLoadReport & getLoadReport() const { return loadReport; }
		const VideoEffectRegistry & registry() const { return effectRegistry; }
		const VideoEffectAssetRegistry & assets() const { return assetRegistry; }

		EffectKnowledgeBase & knowledgeBase() { return knowledge; }
		EffectRandomizer & randomizer() { return effectRandomizer; }

	private:
		VideoEffectRegistry effectRegistry;
		VideoEffectAssetRegistry assetRegistry;
		VideoEffectLoadReport loadReport;
		EffectKnowledgeBase knowledge { "knowledge" };
		EffectRandomizer effectRandomizer;
		bool setupComplete = false;
	};

} // namespace videoeffects
