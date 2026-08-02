#include "VideoEffectService.h"
#include "../catalog/DefaultVideoEffectCatalog.h"
#include "ofFileUtils.h"
#include "ofJson.h"
#include "ofLog.h"
#include "ofUtils.h"

namespace videoeffects {

	bool VideoEffectService::setup(const VideoEffectServiceConfig & config) {
		assetRegistry = VideoEffectAssetRegistry(config.managedAssetSubdir);
		knowledge = EffectKnowledgeBase(config.knowledgeDataDir);
		loadReport = VideoEffectLoadReport();

		registerDefaultVideoEffects(effectRegistry);

		std::string resolvedManifestPath = ofToDataPath(config.manifestPath, true);
		ofFile manifestFile(resolvedManifestPath);
		if (!manifestFile.exists()) {
			ofLogNotice("VideoEffectService") << "no " << config.manifestPath << " found — load report will be empty";
			setupComplete = true;
			return true;
		}

		ofJson manifest;
		try {
			manifest = ofLoadJson(resolvedManifestPath);
		} catch (const std::exception & ex) {
			ofLogError("VideoEffectService") << "failed to parse " << config.manifestPath << ": " << ex.what();
			setupComplete = true;
			return true;
		}

		if (!manifest.contains("effects") || !manifest.at("effects").is_object()) {
			ofLogWarning("VideoEffectService") << config.manifestPath << " has no \"effects\" object — treating as empty";
			setupComplete = true;
			return true;
		}

		for (auto it = manifest.at("effects").begin(); it != manifest.at("effects").end(); ++it) {
			const std::string & effectId = it.key();
			bool enabled = it.value().value("enabled", false);

			loadReport.requested.push_back(effectId);

			if (!enabled) {
				loadReport.skipped.push_back(effectId);
				continue;
			}

			const VideoEffectDefinition * definition = effectRegistry.getDefinition(effectId);
			if (definition == nullptr) {
				ofLogError("VideoEffectService") << "manifest requests unknown effect id: " << effectId;
				loadReport.missing.push_back(effectId);
				continue;
			}

			bool assetsOk = true;
			for (const auto & assetPath : definition->assetPaths) {
				if (!assetRegistry.exists(assetPath)) {
					ofLogError("VideoEffectService")
						<< "manifest enables \"" << effectId << "\" but its asset is missing: " << assetPath
						<< " — run scripts/sync-video-effect-assets.py for this sketch";
					assetsOk = false;
				}
			}

			if (!assetsOk) {
				loadReport.failed.push_back(effectId);
				continue;
			}

			if (!definition->capabilities.safeForAutomaticSelection) {
				// Registered and loadable, but flagged as not yet validated for
				// unattended use (e.g. reaction_diffusion, caustics — see
				// docs/video-effect-promotion-inventory.md). Still usable when
				// explicitly requested via createInstance(); just noted here so
				// a sketch's startup log makes that status visible.
				loadReport.unsupported.push_back(effectId);
			}

			loadReport.registered.push_back(effectId);
		}

		ofLogNotice("VideoEffectService") << "load report: " << loadReport.summary();
		setupComplete = true;
		return true;
	}

	bool VideoEffectService::hasEffect(const std::string & effectId) const {
		return effectRegistry.has(effectId);
	}

	const VideoEffectDefinition * VideoEffectService::getDefinition(const std::string & effectId) const {
		return effectRegistry.getDefinition(effectId);
	}

	std::unique_ptr<VideoEffectInstance> VideoEffectService::createInstance(
		const std::string & effectId, const VideoEffectInstanceConfig & instanceConfig) {
		(void)instanceConfig;

		auto instance = effectRegistry.create(effectId);
		if (!instance) {
			return nullptr;
		}
		if (!instance->setup()) {
			ofLogError("VideoEffectService") << "instance setup() failed for effect: " << effectId;
			return nullptr;
		}
		return instance;
	}

} // namespace videoeffects
