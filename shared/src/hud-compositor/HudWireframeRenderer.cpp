#include "HudWireframeRenderer.h"

#include "HudFormattingService.h"

#include "ofUtils.h"

#include <algorithm>
#include <unordered_set>

namespace hudpresent {

namespace {

// 1:1 conversion from the real ::SceneHealth (SceneContract.h) to this
// domain's FakeSceneHealth mirror — see FakeHudSemanticTypes.h's header
// comment for why that mirror still exists (OF-independence for the
// dependency-free test/fake-scenario layer) and why HudMissingDataController
// deliberately keeps a single FakeSceneHealth-typed parameter rather than
// two overloads: it never needed to know it was talking to real data in
// the first place, so converting once at this one call site was simpler
// than templating a controller class whose logic doesn't otherwise care.
FakeSceneHealth toFakeSceneHealth(SceneHealth h) {
	switch (h) {
		case SceneHealth::Ready: return FakeSceneHealth::Ready;
		case SceneHealth::Loading: return FakeSceneHealth::Loading;
		case SceneHealth::Degraded: return FakeSceneHealth::Degraded;
		case SceneHealth::Failed: return FakeSceneHealth::Failed;
	}
	return FakeSceneHealth::Ready;
}

// This task's §13 first-vertical-slice region set.
const std::unordered_set<std::string>& minimalSliceRegions() {
	static const std::unordered_set<std::string> regions = {
		"universal.scene_title", "universal.media_title", "universal.primary_state", "universal.activity",
		"controls.scene_previous", "controls.scene_next", "controls.media_previous", "controls.media_next", "controls.reseed",
		"flexible.primary",
		"overlay.health", "overlay.transition",
	};
	return regions;
}

} // namespace

HudWireframeRenderer::HudWireframeRenderer() : compiler_(regions_, vocabulary_) {}

void HudWireframeRenderer::setup(float canvasWidth, float canvasHeight) {
	canvasWidth_ = canvasWidth;
	canvasHeight_ = canvasHeight;
}

void HudWireframeRenderer::setCanvasSize(float canvasWidth, float canvasHeight) {
	canvasWidth_ = canvasWidth;
	canvasHeight_ = canvasHeight;
}

void HudWireframeRenderer::setScene(const std::string& sceneId) {
	if (sceneId == currentSceneId_ && !compiled_.bindings.empty()) return; // already compiled for this scene
	currentSceneId_ = sceneId;
	compiled_ = compiler_.compile(sceneId, profiles_);
	lastValid_.clear();
	widgetInputs_.clear();
	haveLastGeneration_ = false;

	// New scene epoch: every tracked history series clears. Per-binding
	// history tracking is (re)established lazily the next time
	// resolveBinding() sees a Sparkline-supporting binding with
	// binding.history set, via HudHistoryStore::track()'s own
	// already-tracked guard.
	static uint64_t epochCounter = 0;
	history_.resetForSceneEpoch(++epochCounter);
}

bool HudWireframeRenderer::regionInMinimalSlice(const std::string& regionId) {
	return minimalSliceRegions().count(regionId) != 0;
}

ofRectangle HudWireframeRenderer::pixelBoundsFor(const std::string& regionId) const {
	const auto* region = regions_.find(regionId);
	if (!region) return {};
	return ofRectangle(
		region->bounds.x * canvasWidth_,
		region->bounds.y * canvasHeight_,
		region->bounds.width * canvasWidth_,
		region->bounds.height * canvasHeight_);
}

std::string HudWireframeRenderer::primaryRoleFor(HudWidgetType type) const {
	switch (type) {
		case HudWidgetType::Label: return "text";
		case HudWidgetType::MetadataCard: return "title";
		case HudWidgetType::Timeline: return "phase";
		case HudWidgetType::ChannelStrip: return "channel0";
		case HudWidgetType::AmbientField: return "influence";
		default: return "value";
	}
}

void HudWireframeRenderer::update(float dt, const FakeHudFrameData& frame) {
	elapsedSeconds_ += dt;
	history_.update(dt);

	// Generation-change detection drives HudHistoryStore's event-pulse
	// marker (a same-scene curated reseed, distinct from a full scene
	// switch, which setScene() handles via resetForSceneEpoch instead).
	// generation lives inside the optional semantic payload now (real
	// ::SceneTimingStatus, Engineering Session 2 reconciliation) — a
	// semantic-absent frame (Loading) simply can't report a generation
	// change this frame, which is the correct, conservative behavior.
	uint64_t currentGeneration = frame.scene.semantic ? frame.scene.semantic->timing.generation : lastGeneration_;
	bool generationChanged = haveLastGeneration_ && frame.scene.semantic && currentGeneration != lastGeneration_;
	lastGeneration_ = currentGeneration;
	haveLastGeneration_ = haveLastGeneration_ || frame.scene.semantic.has_value();

	for (const auto& compiled : compiled_.bindings) {
		if (!compiled.valid) continue; // invalid bindings never resolve data — see draw()'s BindingPlaceholder substitution
		if (generationChanged && compiled.binding.history.has_value()) {
			if (const auto* valueSrc = compiled.binding.findSource("value")) {
				history_.markGenerationChange(valueSrc->sourceId);
			}
		}
		resolveBinding(compiled, frame);
	}
}

void HudWireframeRenderer::resolveBinding(const HudCompiledBinding& compiled, const FakeHudFrameData& frame) {
	const auto& binding = compiled.binding;

	HudWidgetInput input;
	input.bindingId = binding.bindingId;
	input.regionId = binding.regionId;
	input.widgetType = binding.widgetType;
	input.captionText = compiled.resolvedLabelText;
	input.elapsedSeconds = elapsedSeconds_;

	std::vector<HudMissingDataController::Outcome> outcomes;
	outcomes.reserve(binding.sourceCount);

	for (size_t i = 0; i < binding.sourceCount; ++i) {
		const auto& src = binding.sources[i];
		if (src.sourceId.empty()) continue;

		auto resolvedNow = resolver_.resolve(src.sourceId, frame);
		std::optional<HudResolvedValue> fallbackValue;
		if (binding.fallbackSourceId) {
			fallbackValue = resolver_.resolve(*binding.fallbackSourceId, frame);
		}

		const std::string lastValidKey = binding.bindingId + "\x1f" + src.role;
		std::optional<HudResolvedValue> lastValid;
		auto lastValidIt = lastValid_.find(lastValidKey);
		if (lastValidIt != lastValid_.end()) lastValid = lastValidIt->second;

		auto outcome = missingData_.decide(compiled, resolvedNow, lastValid, frame.scene.health, fallbackValue);
		if (outcome.shouldRender && !outcome.useAmbientFallback) {
			lastValid_[lastValidKey] = outcome.valueToShow;
		}
		outcomes.push_back(outcome);

		HudWidgetRoleValue roleValue;
		roleValue.role = src.role;
		roleValue.shouldRender = outcome.shouldRender;
		roleValue.dimmed = outcome.dimmed;
		roleValue.useAmbientFallback = outcome.useAmbientFallback;
		roleValue.value = outcome.valueToShow;

		if (outcome.shouldRender && !outcome.useAmbientFallback) {
			roleValue.formattedText = HudFormattingService::format(outcome.valueToShow, binding.format, currentSceneId_, vocabulary_);
			if (outcome.valueToShow.valueType == HudSourceValueType::IdentifierList) {
				// Engineering Session 2 reconciliation: IdentifierList's one
				// production source (effects.active, resolved from the
				// real SceneHudStatus::activeEffects) is ALREADY curated
				// display text per shared/src/video-effects/knowledge/
				// EffectActivityStatus.h's own comment ("only curated
				// display labels ever cross that boundary") — not a list
				// of vocabulary-resolvable IDs. Session 1's "effect.<id>.
				// label" lookup convention assumed a future ID-list field
				// that turned out not to be how the real contract works;
				// removed here in favor of a direct passthrough, and the
				// fake scenarios were updated (see fake/Fake*Scenario.cpp)
				// to emit the same kind of already-curated text so both
				// paths agree.
				for (size_t item = 0; item < outcome.valueToShow.listValue.count; ++item) {
					roleValue.formattedItems.push_back(outcome.valueToShow.listValue.items[item]);
				}
			}
		}

		input.roles.push_back(roleValue);
	}

	// Per-binding ambient-fallback trigger (see HudWireframeRenderer.h's
	// header comment on why this is computed per-binding rather than
	// literally substituting a different widget class per-region — every
	// region in HudRegionCatalog has maxBindings==1, so the two are
	// equivalent in this increment's concrete profiles).
	input.regionAmbientFallback = HudMissingDataController::regionNeedsAmbientFallback(outcomes);

	// History: only for bindings that requested it, using the RAW
	// resolver result for the binding's primary role (never the
	// display-policy outcome, which may repeat a retained/dimmed value
	// that isn't really "this frame's data") — and paused entirely while
	// the scene is Loading, per this task's §9 "loading pauses histories".
	if (binding.history.has_value()) {
		std::string primaryRole = primaryRoleFor(binding.widgetType);
		if (const auto* src = binding.findSource(primaryRole)) {
			history_.track(src->sourceId, *binding.history); // no-op if already tracked
			history_.pause(src->sourceId, frame.scene.health == FakeSceneHealth::Loading);
			auto resolvedNow = resolver_.resolve(src->sourceId, frame);
			std::optional<float> sampleValue;
			if (resolvedNow && resolvedNow->present) sampleValue = resolvedNow->numberValue;
			history_.pushSample(src->sourceId, sampleValue); // std::nullopt writes a gap marker, never a synthetic 0
			input.historySamples = history_.samplesOldestToNewest(src->sourceId);
			input.historyStats = history_.stats(src->sourceId);
		}
	}

	if (!compiled.valid) {
		for (const auto& issue : compiled_.issues) {
			if (issue.bindingId == binding.bindingId) input.compileIssueSummaries.push_back(issue.detail);
		}
	}

	widgetInputs_[binding.bindingId] = std::move(input);
}

void HudWireframeRenderer::update(float dt, const HudFrameData& frame) {
	// Scene epoch: a change in the ONE approved, already-existing signal
	// for "which scene is active" (SceneManagerStatus::activeSceneId) —
	// no new field invented, per this task's §3. setScene() itself is a
	// no-op if the ID hasn't actually changed (its own early-out), so
	// calling it unconditionally every frame here is cheap and correct.
	if (frame.sceneManager.activeSceneId != currentSceneId_ || compiled_.bindings.empty()) {
		setScene(frame.sceneManager.activeSceneId);
	}

	// Frame freshness: frame.sceneFrame.frameNumber is ExperienceRuntime's
	// own existing, already-approved monotonic counter (Scene-HUD-Contract-v1.md
	// §4 step 12) — reused here as-is, not a new field. See this method's
	// declaration comment for exactly what isFrameFresh() promises.
	uint64_t frameNumber = frame.sceneFrame.frameNumber;
	lastFrameWasFresh_ = !haveObservedFrameNumber_ || frameNumber != lastObservedFrameNumber_;
	lastObservedFrameNumber_ = frameNumber;
	haveObservedFrameNumber_ = true;

	// Captured for draw()'s MediaViewportMesh call — a non-owning pointer
	// into ExperienceRuntime's own SceneFbo texture, valid only for this
	// frame (matches SceneFrame's own documented lifetime rule; this
	// class never allocates or owns a texture itself, matching "no
	// production textures in this task").
	currentSceneTexture_ = frame.sceneFrame.texture;
	currentSceneNativeSize_ = frame.sceneFrame.nativeSize;

	elapsedSeconds_ += dt;
	history_.update(dt);

	// Generation change: same-scene reseed/reconfiguration signal, kept
	// structurally distinct from the epoch change above — see this
	// method's declaration comment.
	const auto* semantic = frame.scene.semantic ? &(*frame.scene.semantic) : nullptr;
	uint64_t currentGeneration = semantic ? semantic->timing.generation : lastGeneration_;
	bool generationChanged = haveLastGeneration_ && semantic && currentGeneration != lastGeneration_;
	lastGeneration_ = currentGeneration;
	haveLastGeneration_ = haveLastGeneration_ || (semantic != nullptr);

	for (const auto& compiled : compiled_.bindings) {
		if (!compiled.valid) continue;
		if (generationChanged && compiled.binding.history.has_value()) {
			if (const auto* valueSrc = compiled.binding.findSource("value")) {
				history_.markGenerationChange(valueSrc->sourceId);
			}
		}
		resolveBindingReal(compiled, frame);
	}
}

void HudWireframeRenderer::resolveBindingReal(const HudCompiledBinding& compiled, const HudFrameData& frame) {
	const auto& binding = compiled.binding;

	HudWidgetInput input;
	input.bindingId = binding.bindingId;
	input.regionId = binding.regionId;
	input.widgetType = binding.widgetType;
	input.captionText = compiled.resolvedLabelText;
	input.elapsedSeconds = elapsedSeconds_;

	std::vector<HudMissingDataController::Outcome> outcomes;
	outcomes.reserve(binding.sourceCount);

	for (size_t i = 0; i < binding.sourceCount; ++i) {
		const auto& src = binding.sources[i];
		if (src.sourceId.empty()) continue;

		auto resolvedNow = realResolver_.resolve(src.sourceId, frame);
		std::optional<HudResolvedValue> fallbackValue;
		if (binding.fallbackSourceId) {
			fallbackValue = realResolver_.resolve(*binding.fallbackSourceId, frame);
		}

		const std::string lastValidKey = binding.bindingId + "\x1f" + src.role;
		std::optional<HudResolvedValue> lastValid;
		auto lastValidIt = lastValid_.find(lastValidKey);
		if (lastValidIt != lastValid_.end()) lastValid = lastValidIt->second;

		auto outcome = missingData_.decide(compiled, resolvedNow, lastValid, toFakeSceneHealth(frame.scene.health), fallbackValue);
		if (outcome.shouldRender && !outcome.useAmbientFallback) {
			lastValid_[lastValidKey] = outcome.valueToShow;
		}
		outcomes.push_back(outcome);

		HudWidgetRoleValue roleValue;
		roleValue.role = src.role;
		roleValue.shouldRender = outcome.shouldRender;
		roleValue.dimmed = outcome.dimmed;
		roleValue.useAmbientFallback = outcome.useAmbientFallback;
		roleValue.value = outcome.valueToShow;

		if (outcome.shouldRender && !outcome.useAmbientFallback) {
			roleValue.formattedText = HudFormattingService::format(outcome.valueToShow, binding.format, currentSceneId_, vocabulary_);
			if (outcome.valueToShow.valueType == HudSourceValueType::IdentifierList) {
				for (size_t item = 0; item < outcome.valueToShow.listValue.count; ++item) {
					roleValue.formattedItems.push_back(outcome.valueToShow.listValue.items[item]);
				}
			}
		}

		input.roles.push_back(roleValue);
	}

	input.regionAmbientFallback = HudMissingDataController::regionNeedsAmbientFallback(outcomes);

	if (binding.history.has_value()) {
		std::string primaryRole = primaryRoleFor(binding.widgetType);
		if (const auto* src = binding.findSource(primaryRole)) {
			history_.track(src->sourceId, *binding.history);
			history_.pause(src->sourceId, frame.scene.health == SceneHealth::Loading);
			auto resolvedNow = realResolver_.resolve(src->sourceId, frame);
			std::optional<float> sampleValue;
			if (resolvedNow && resolvedNow->present) sampleValue = resolvedNow->numberValue;
			history_.pushSample(src->sourceId, sampleValue);
			input.historySamples = history_.samplesOldestToNewest(src->sourceId);
			input.historyStats = history_.stats(src->sourceId);
		}
	}

	if (!compiled.valid) {
		for (const auto& issue : compiled_.issues) {
			if (issue.bindingId == binding.bindingId) input.compileIssueSummaries.push_back(issue.detail);
		}
	}

	widgetInputs_[binding.bindingId] = std::move(input);
}

void HudWireframeRenderer::draw() const {
	// MediaViewportMesh — drawn first (chrome/widgets layer on top),
	// always, regardless of RenderScope (it's not a binding, so
	// MinimalSlice/FullProfile filtering doesn't apply to it). Textured
	// when the most recent update(HudFrameData) supplied an allocated
	// scene texture; otherwise the placeholder-fill wireframe path — see
	// MediaViewportMesh::draw()'s own comment. currentSceneTexture_ stays
	// nullptr for the whole Validation Studio / fake-scenario path, so
	// this always draws in placeholder mode there, which is exactly
	// "wireframe mode" for that host.
	{
		MediaViewportGeometryParams params;
		params.bounds = pixelBoundsFor("media_viewport");
		mediaViewportMesh_.updateGeometry(params, currentSceneNativeSize_);
		mediaViewportMesh_.draw(currentSceneTexture_, ofColor(20, 26, 24, 255));
	}

	stats_ = FrameStats{};
	stats_.historySignalCount = static_cast<int>(history_.trackedSignalCount());
	stats_.historyBytes = HudHistoryStore::estimatedBytes();

	uint64_t drawStart = ofGetElapsedTimeMicros();

	// Pre-pass: most regions host exactly one binding, but
	// overlay.transition now hosts up to three (progress bar + phase
	// label + message label — this task's §4). Count how many bindings
	// will actually attempt to draw THIS frame per region (respecting the
	// same scope/studio filtering the main loop below applies), so a
	// multi-binding region can be sliced into vertically-stacked slots
	// instead of every binding drawing on top of the others at identical
	// bounds.
	std::unordered_map<std::string, int> regionSlotCounts;
	for (const auto& compiled : compiled_.bindings) {
		if (renderScope_ == RenderScope::MinimalSlice && !regionInMinimalSlice(compiled.binding.regionId)) continue;
		if (!compiled.valid && !studioMode_) continue;
		regionSlotCounts[compiled.binding.regionId]++;
	}
	std::unordered_map<std::string, int> regionSlotCursor;

	for (const auto& compiled : compiled_.bindings) {
		if (renderScope_ == RenderScope::MinimalSlice && !regionInMinimalSlice(compiled.binding.regionId)) continue;

		HudWidgetType effectiveType = compiled.binding.widgetType;
		if (!compiled.valid) {
			if (!studioMode_) continue; // production: skip an invalid optional binding entirely
			effectiveType = HudWidgetType::BindingPlaceholder; // studio: visible placeholder
		}

		auto it = widgetInputs_.find(compiled.binding.bindingId);
		if (it == widgetInputs_.end()) continue; // update() never ran for this binding yet

		ofRectangle bounds = pixelBoundsFor(compiled.binding.regionId);
		int slotCount = regionSlotCounts[compiled.binding.regionId];
		if (slotCount > 1) {
			int slotIndex = regionSlotCursor[compiled.binding.regionId]++;
			float slotHeight = bounds.height / static_cast<float>(slotCount);
			bounds = ofRectangle(bounds.x, bounds.y + slotHeight * static_cast<float>(slotIndex), bounds.width, slotHeight);
		}
		HudWidgetColors colors; // no skin system in this increment — see this task's "No skin-specific C++ logic" constraint

		uint64_t widgetStart = ofGetElapsedTimeMicros();
		widgets_.widgetFor(effectiveType).draw(bounds, colors, it->second);
		uint64_t widgetEnd = ofGetElapsedTimeMicros();

		stats_.activeWidgetCount++;
		stats_.perWidgetMicros.emplace_back(compiled.binding.bindingId, static_cast<double>(widgetEnd - widgetStart));
	}

	uint64_t drawEnd = ofGetElapsedTimeMicros();
	stats_.totalDrawMicros = static_cast<double>(drawEnd - drawStart);
}

} // namespace hudpresent
