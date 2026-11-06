#include "HudWireframeRenderer.h"
#include "HudReactiveControlArt.h"
#include "widgets/HudWidgetDrawUtils.h" // hudTextShadowColor()

#include "HudFormattingService.h"

#include "ofGraphics.h"
#include "ofImage.h"
#include "ofPolyline.h"
#include "ofUtils.h"

#include <algorithm>
#include <cmath>
#include <array>
#include <cstdlib>
#include <string>
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

// Chip text for one effects.active item: the curated displayName carried by
// the same EffectActivityStatus slot (VideoEffectDefinition::displayName),
// or the effect ID itself when no slot supplies one.
const std::string& effectDisplayLabel(const HudFrameData& frame, const std::string& effectId) {
	if (frame.effects) {
		for (const auto& slot : frame.effects->slots) {
			if (slot.effectId == effectId && !slot.displayName.empty()) return slot.displayName;
		}
	}
	return effectId;
}

// The production presentation allowlist, kept as its two provenance parts so
// tooling (HudArtReferenceExporter) can tell them apart without a second list.
// All parts together remain a permitted HUD-006 subset — no geometry or
// Blueprint identity change.
//
// Part 1: this task's §13 first vertical slice (13 regions with media_viewport).
const std::unordered_set<std::string>& originalSliceRegions() {
	static const std::unordered_set<std::string> regions = {
		"universal.scene_title", "universal.media_title", "universal.primary_state", "universal.activity",
		"controls.scene_previous", "controls.scene_next", "controls.media_previous", "controls.media_next",
		// controls.reseed retired in HUD-006 v2 (this historical part is now 12).
		"flexible.primary",
		"overlay.health", "overlay.transition",
	};
	return regions;
}

// Part 2: Living HUD v0.1 activations (15 regions in total) —
// universal.effect_summary (real HudFrameData.effects; Hide when no
// authoritative snapshot exists, "NONE ACTIVE" when present-empty) and
// universal.timing (real semantic timing.activeSeconds).
const std::unordered_set<std::string>& livingHudRegions() {
	static const std::unordered_set<std::string> regions = {
		"universal.effect_summary", "universal.timing",
	};
	return regions;
}

// Part 3: HUD Clock v0.1 (16 regions in total) — flexible.secondary_b
// (runtime-owned HudFrameData::wallClock; Hide when no valid snapshot).
const std::unordered_set<std::string>& clockRegions() {
	static const std::unordered_set<std::string> regions = {
		"flexible.secondary_b",
	};
	return regions;
}

// Part 4: Scene & Pattern Navigation (17 regions in total) —
// flexible.secondary_a (active scene's mode + the NEXT MODE focus target;
// Hide when the scene has no mode model).
// HUD-006 v2 adds the MODE and FULLSCREEN controls here (18 regions in total);
// flexible.secondary_a stays as the informational mode panel (no focus).
const std::unordered_set<std::string>& navigationRegions() {
	static const std::unordered_set<std::string> regions = {
		"flexible.secondary_a", "controls.mode", "controls.fullscreen", "controls.menu",
	};
	return regions;
}

// Part 5: System Telemetry v1 — the two trace regions host the five system
// vitals sparklines (3 + 2) fed by HudFrameData::systemVitals. Enabling
// existing regions is not a Blueprint version change (Architecture-Freeze-
// HUD-006-Canonical-Blueprint-Identity-v1); bounds and IDs are untouched.
const std::unordered_set<std::string>& systemTelemetryRegions() {
	static const std::unordered_set<std::string> regions = {
		"universal.activity_trace", "universal.secondary_trace",
	};
	return regions;
}

// Regions whose bindings reserve a slot only while they actually render (Option B,
// Orbit Console v0.1 correction): universal.effect_summary hosts the effect chips OR the
// scene status, which never coexist, so a Hide-d binding must not leave an empty slice.
// A Placeholder / DimLastValid / dimmed-while-Loading binding still renders, so it still
// occupies. Deliberately opt-in: Sparkline regions (vitals, Orbit) draw their own NO DATA
// state and keep reserving their slots.
const std::unordered_set<std::string>& visibilityAwareRegions() {
	static const std::unordered_set<std::string> regions = {"universal.effect_summary"};
	return regions;
}

} // namespace

bool HudWireframeRenderer::bindingOccupiesSlot(const HudCompiledBinding& compiled) const {
	if (visibilityAwareRegions().count(compiled.binding.regionId) == 0) return true;
	if (!compiled.valid) return true; // studio placeholder for an invalid binding
	const auto it = widgetInputs_.find(compiled.binding.bindingId);
	if (it == widgetInputs_.end()) return true; // not resolved yet: reserve
	for (const auto& role : it->second.roles) {
		if (role.shouldRender) return true;
	}
	return false;
}

int HudWireframeRenderer::regionSlotCount(const std::string& regionId) const {
	const auto counts = computeRegionSlotCounts();
	const auto it = counts.find(regionId);
	return it == counts.end() ? 0 : it->second;
}

HudWireframeRenderer::HudWireframeRenderer() : compiler_(regions_, vocabulary_) {
	realResolver_.setVocabulary(&vocabulary_);
}

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
	return originalSliceRegions().count(regionId) != 0 || livingHudRegions().count(regionId) != 0
		|| clockRegions().count(regionId) != 0 || navigationRegions().count(regionId) != 0
		|| systemTelemetryRegions().count(regionId) != 0;
}

bool HudWireframeRenderer::regionInOriginalMinimalSlice(const std::string& regionId) {
	return originalSliceRegions().count(regionId) != 0;
}

const char* HudWireframeRenderer::productionProvenance(const std::string& regionId) {
	if (originalSliceRegions().count(regionId) != 0) return "original";
	if (livingHudRegions().count(regionId) != 0) return "living_hud";
	if (clockRegions().count(regionId) != 0) return "clock";
	if (navigationRegions().count(regionId) != 0) return "navigation";
	if (systemTelemetryRegions().count(regionId) != 0) return "system_telemetry";
	return nullptr;
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
	if (activationPending_) { // see triggerActivationFeedback(): starts on the frame that shows it
		activationStart_ = elapsedSeconds_;
		activationPending_ = false;
	}
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
	input.variantId = binding.variantId;
	input.labelTypography = labelTypography_; // dev-only playground; null in production
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
	slotCounts_ = computeRegionSlotCounts(); // sizes the system-vitals traces (update path, never draw)
	if (activationPending_) { // see triggerActivationFeedback(): starts on the frame that shows it
		activationStart_ = elapsedSeconds_;
		activationPending_ = false;
	}
	history_.update(dt);

	// Generation change: same-scene reseed/reconfiguration signal, kept
	// structurally distinct from the epoch change above — see this
	// method's declaration comment.
	const auto* semantic = frame.scene.semantic ? &(*frame.scene.semantic) : nullptr;
	uint64_t currentGeneration = semantic ? semantic->timing.generation : lastGeneration_;
	bool generationChanged = haveLastGeneration_ && semantic && currentGeneration != lastGeneration_;
	lastGeneration_ = currentGeneration;
	haveLastGeneration_ = haveLastGeneration_ || (semantic != nullptr);

	frameLoading_ = frame.scene.health == SceneHealth::Loading;
	updateOrbit(frame);
	visualActivity_.update(frame.sceneVisualActivity.get(), dt); // OC-004: resets on invalid/stale/transition/generation change
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
	input.variantId = binding.variantId;
	input.labelTypography = labelTypography_; // dev-only playground; null in production
	input.captionText = compiled.resolvedLabelText;
	input.elapsedSeconds = elapsedSeconds_;

	std::vector<HudMissingDataController::Outcome> outcomes;
	outcomes.reserve(binding.sourceCount);

	for (size_t i = 0; i < binding.sourceCount; ++i) {
		const auto& src = binding.sources[i];
		if (src.sourceId.empty()) continue;

		auto resolvedNow = realResolver_.resolve(src.sourceId, frame);
		// OC-004: the resolver yields the unsmoothed level only while Valid; the HUD-owned ballistics value
		// replaces it (present exactly when the resolved value is present).
		if (resolvedNow && resolvedNow->present && src.sourceId == "scene.visual.activity" && visualActivity_.has()) {
			resolvedNow->numberValue = visualActivity_.value();
		}
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
				// effects.active (the one production IdentifierList source)
				// stays a list of effect IDs semantically; the chip text is
				// the same snapshot slot's curated displayName, falling back
				// to the ID when absent.
				for (size_t item = 0; item < outcome.valueToShow.listValue.count; ++item) {
					roleValue.formattedItems.push_back(effectDisplayLabel(frame, outcome.valueToShow.listValue.items[item]));
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

	if (binding.widgetType == HudWidgetType::Sparkline && binding.variantId == "vitals") {
		resolveVitalsTrace(compiled, frame, input);
	}

	if (binding.widgetType == HudWidgetType::Sparkline && binding.variantId.compare(0, 6, "orbit.") == 0) {
		input.orbit = &orbitDraw_;
	}

	widgetInputs_[binding.bindingId] = std::move(input);
}

std::unordered_map<std::string, int> HudWireframeRenderer::computeRegionSlotCounts() const {
	std::unordered_map<std::string, int> counts;
	for (const auto& compiled : compiled_.bindings) {
		if (renderScope_ == RenderScope::MinimalSlice && !regionInMinimalSlice(compiled.binding.regionId)) continue;
		if (!compiled.valid && !studioMode_) continue;
		if (!bindingOccupiesSlot(compiled)) continue;
		counts[compiled.binding.regionId]++;
	}
	return counts;
}

void HudWireframeRenderer::resolveVitalsTrace(const HudCompiledBinding& compiled, const HudFrameData& frame, HudWidgetInput& input) {
	const auto& binding = compiled.binding;
	const auto* valueSrc = binding.findSource("value");
	const VitalsMetricSpec* spec = valueSrc ? vitalsSpecForSource(valueSrc->sourceId) : nullptr;
	if (!spec) return; // not a system-vitals binding: leave input.vitals null

	// Slot size = the region's pixel size, divided by this region's slot count
	// exactly as draw() slices it.
	const ofRectangle region = pixelBoundsFor(binding.regionId);
	const auto countIt = slotCounts_.find(binding.regionId);
	const int slots = (countIt == slotCounts_.end() || countIt->second < 1) ? 1 : countIt->second;
	HudVitalsDrawData& d = vitalsDraw_[binding.bindingId];
	d.metric = spec->metric;
	// System Performance dials: the slots sit side by side (columns), not stacked.
	d.slotWidth = region.width / static_cast<float>(slots);
	d.slotHeight = region.height;
	d.plotX = kVitalsPad;
	d.plotY = 3.0f;
	d.plotW = std::max(0.0f, d.slotWidth - 2.0f * kVitalsPad);
	d.plotH = std::max(0.0f, d.slotHeight - 2.0f * d.plotY);

	VitalsSeriesView view; // default = Unavailable, empty
	if (const auto* snap = frame.systemVitals.get()) {
		switch (spec->metric) {
			case sysvitals::Metric::Fps: view = vitalsViewOf(snap->fps); break;
			case sysvitals::Metric::FrameTimeMs: view = vitalsViewOf(snap->frameTimeMax); break;
			case sysvitals::Metric::CpuPercent: view = vitalsViewOf(snap->cpuBusy); break;
			case sysvitals::Metric::GpuTempC: view = vitalsViewOf(snap->gpuTemp); break;
			case sysvitals::Metric::RssMiB: view = vitalsViewOf(snap->processRss); break;
			default: break;
		}
	}
	// A null snapshot (default view) has revision 0, which would alias a real
	// first revision: key the empty case off the trace's own state instead.
	if (d.trace.update(*spec, view, d.plotW, d.plotH) || d.linesBuiltAtRebuild != d.trace.rebuildCount()) {
		d.lines.clear();
		d.dots.clear();
		for (size_t i = 0; i < d.trace.segmentCount(); ++i) {
			const auto seg = d.trace.segment(i);
			if (seg.count == 1) {
				const auto& v = d.trace.vertex(seg.first);
				d.dots.push_back(v);
				continue;
			}
			ofPolyline line;
			line.getVertices().reserve(seg.count);
			for (uint16_t k = 0; k < seg.count; ++k) {
				const auto& v = d.trace.vertex(seg.first + k);
				line.addVertex(v.x, v.y);
			}
			d.lines.push_back(std::move(line));
		}
		d.linesBuiltAtRebuild = d.trace.rebuildCount();
		d.polylineRebuilds++;
	}
	input.vitals = &d;
}


// Orbit Console v0.1: builds the shared orbit geometry once per update. The
// series comes from the ratified scene-colour snapshot once HudFrameData
// carries it; until then (and whenever it is absent) the view is Unavailable,
// never zeros.
void HudWireframeRenderer::updateOrbit(const HudFrameData& frame) {
	OrbitSeriesView view; // default = Unavailable, empty
	static const char* const fx = std::getenv("EXPERIENCE_RUNTIME_ORBIT_FIXTURE"); // verification fixture (SYNTHETIC), read once
	if (orbitOverride_) view = *orbitOverride_;
	else if (fx) { // verification only: SYNTHETIC, not a measurement
		static std::array<OrbitSample, kOrbitMaxSamples> fixture;
		const std::string mode = fx;
		const double now = elapsedSeconds_ + 70.0; // virtual start 70 s in: a full 60 s window from frame one
		const double lag = mode == "stale" ? 6.0 : 0.0; // stale: newest sample is 6 s old
		uint32_t n = 0;
		for (int i = 0; i < int(kOrbitMaxSamples); ++i) {
			const double t = std::floor((now - lag) / 0.5) * 0.5 - (kOrbitMaxSamples - 1 - i) * 0.5;
			if (t < 0) continue;
			OrbitSample& smp = fixture[n++];
			const float a = float(t * 0.35), sat = 0.55f + 0.35f * std::sin(float(t) * 0.2f);
			smp.t = t;
			smp.r = std::clamp(0.5f + sat * 0.5f * std::cos(a), 0.0f, 1.0f);
			smp.g = std::clamp(0.5f + sat * 0.5f * std::cos(a - 2.094f), 0.0f, 1.0f);
			smp.b = std::clamp(0.5f + sat * 0.5f * std::cos(a + 2.094f), 0.0f, 1.0f);
			smp.luma = 0.2126f * smp.r + 0.7152f * smp.g + 0.0722f * smp.b;
			smp.valid = !(mode == "gap" && (i % 23) > 17); // gap: periodic invalid runs
		}
		view.state = (n == 0) ? OrbitState::Unavailable : (mode == "stale" ? OrbitState::Stale : OrbitState::Valid);
		view.generation = 1;
		view.revision = uint64_t(std::floor((now - lag) / 0.5)) + 1;
		view.samples = fixture.data();
		view.count = n;
	}
	else if (const auto* snap = frame.sceneColor.get()) { // the real, runtime-owned measurement
		if (snap->revision != orbitBufRevision_) {
			const uint32_t n = std::min<uint32_t>(snap->count, static_cast<uint32_t>(kOrbitMaxSamples));
			for (uint32_t i = 0; i < n; ++i) {
				const scenecolor::Sample& in = snap->history[i];
				OrbitSample& out = orbitBuf_[i];
				out.t = in.t;
				out.valid = in.valid;
				out.r = in.valid ? srgbEncode(in.r) : 0.0f; // swatch/ribbon/orbit/bars/chart are display-referred
				out.g = in.valid ? srgbEncode(in.g) : 0.0f;
				out.b = in.valid ? srgbEncode(in.b) : 0.0f;
				out.luma = in.valid ? in.luma : 0.0f;      // relative luminance stays linear
			}
			orbitBufCount_ = n;
			orbitBufRevision_ = snap->revision;
		}
		view.state = snap->state == scenecolor::State::Valid ? OrbitState::Valid
			: snap->state == scenecolor::State::Stale ? OrbitState::Stale : OrbitState::Unavailable;
		view.generation = snap->generation;
		view.revision = snap->revision;
		view.samples = orbitBuf_.data();
		view.count = orbitBufCount_;
	}
	// RAW VIDEO series (decoded video colour before scene effects): the runtime-owned
	// HudFrameData::sceneColorRaw (OC-003B, opt-in EXPERIENCE_RUNTIME_VIDEO_COLOR=1). Null = Unavailable
	// (NO RAW VIDEO). The SYNTHETIC verification fixture (EXPERIENCE_RUNTIME_ORBIT_RAW_FIXTURE=valid|gap|
	// stale|black) takes precedence only when that env var is set. Never derived from the SceneFBO series.
	OrbitSeriesView rawView;
	const OrbitSeriesView* rawPtr = nullptr;
	static const char* const rawFx = std::getenv("EXPERIENCE_RUNTIME_ORBIT_RAW_FIXTURE");
	if (orbitRawOverride_) { // verification only (OC-005 harness): deterministic RAW series
		rawView = *orbitRawOverride_;
		rawPtr = &rawView;
	}
	else if (rawFx && view.samples && view.count > 0) {
		static std::array<OrbitSample, kOrbitMaxSamples> rawFixture;
		const std::string mode = rawFx;
		const double lag = mode == "stale" ? 6.0 : 0.0;
		uint32_t rn = 0;
		for (uint32_t i = 0; i < view.count; ++i) {
			const OrbitSample& ps = view.samples[i];
			if (ps.t > view.samples[view.count - 1].t - lag) continue;
			OrbitSample& o = rawFixture[rn++];
			const float a = float(ps.t * 0.12);
			o.t = ps.t;
			o.valid = !(mode == "gap" && (int(ps.t * 2.0) % 31) > 22);
			o.r = mode == "black" ? 0.0f : 0.5f + 0.45f * std::sin(a);
			o.g = mode == "black" ? 0.0f : 0.5f + 0.45f * std::sin(a + 1.0f);
			o.b = mode == "black" ? 0.0f : 0.5f + 0.45f * std::sin(a + 4.0f);
			o.luma = 0.2126f * o.r + 0.7152f * o.g + 0.0722f * o.b;
		}
		rawView.state = rn == 0 ? OrbitState::Unavailable : (mode == "stale" ? OrbitState::Stale : OrbitState::Valid);
		rawView.generation = view.generation;
		rawView.revision = view.revision;
		rawView.samples = rawFixture.data();
		rawView.count = rn;
		rawPtr = &rawView;
	}
	else if (const auto* rsnap = frame.sceneColorRaw.get()) { // the real RAW measurement, independent of FX
		if (rsnap->revision != orbitRawBufRevision_) {
			const uint32_t n = std::min<uint32_t>(rsnap->count, static_cast<uint32_t>(kOrbitMaxSamples));
			for (uint32_t i = 0; i < n; ++i) {
				const scenecolor::Sample& in = rsnap->history[i];
				OrbitSample& out = orbitRawBuf_[i];
				out.t = in.t;
				out.valid = in.valid;
				out.r = in.valid ? srgbEncode(in.r) : 0.0f;
				out.g = in.valid ? srgbEncode(in.g) : 0.0f;
				out.b = in.valid ? srgbEncode(in.b) : 0.0f;
				out.luma = in.valid ? in.luma : 0.0f;
			}
			orbitRawBufCount_ = n;
			orbitRawBufRevision_ = rsnap->revision;
		}
		rawView.state = rsnap->state == scenecolor::State::Valid ? OrbitState::Valid
			: rsnap->state == scenecolor::State::Stale ? OrbitState::Stale : OrbitState::Unavailable;
		rawView.generation = rsnap->generation;
		rawView.revision = rsnap->revision;
		rawView.samples = orbitRawBuf_.data();
		rawView.count = orbitRawBufCount_;
		rawPtr = &rawView;
	}
	const ofRectangle upper = pixelBoundsFor("universal.activity_trace");
	const ofRectangle lower = pixelBoundsFor("universal.secondary_trace");
	OrbitLayout layout;
	// Verification-only override of the in-code tuning (kOrbitHistoryTuning); read once, off by default.
	static const float segW = [] { const char* v = std::getenv("EXPERIENCE_RUNTIME_ORBIT_SEG_WIDTH_PX"); return v ? float(std::atof(v)) : kOrbitHistoryTuning.segmentWidthPx; }();
	static const float segGap = [] { const char* v = std::getenv("EXPERIENCE_RUNTIME_ORBIT_SEG_GAP_PX"); return v ? float(std::atof(v)) : kOrbitHistoryTuning.segmentGapPx; }();
	layout.segmentWidthPx = segW > 0.0f ? segW : kOrbitHistoryTuning.segmentWidthPx;
	layout.segmentGapPx = segGap >= 0.0f ? segGap : kOrbitHistoryTuning.segmentGapPx;
	layout.w = upper.width;
	layout.upperH = upper.height;
	layout.lowerH = lower.height;
	OrbitDrawData& d = orbitDraw_;
	d.upperW = upper.width;
	d.upperH = upper.height;
	d.lowerH = lower.height;
	const bool rebuilt = d.model.update(view, layout, rawPtr);
	if (!rebuilt && d.linesBuiltAtRebuild == d.model.rebuildCount()) return;
	const OrbitConsoleModel& m = d.model;
	auto buildLines = [](const OrbitTrace& t, std::vector<ofPolyline>& lines, std::vector<OrbitPt>& dots) {
		lines.clear();
		dots.clear();
		for (size_t i = 0; i < t.segN; ++i) {
			const auto seg = t.seg[i];
			if (seg.count == 1) { dots.push_back(t.v[seg.first]); continue; }
			ofPolyline line;
			line.getVertices().reserve(seg.count);
			for (uint16_t k = 0; k < seg.count; ++k) line.addVertex(t.v[seg.first + k].x, t.v[seg.first + k].y);
			lines.push_back(std::move(line));
		}
	};
	// One quad per painted bin: width = segmentWidthPx, the gap to the next bin is the pitch minus width.
	auto quad = [](ofMesh& mesh, const HistoryRect& r, const HistoryFill& f) {
		const unsigned base = static_cast<unsigned>(mesh.getNumVertices());
		const float x0 = r.x, x1 = r.x + r.w, y0 = r.y, y1 = r.y + r.h;
		const ofFloatColor col(f.r, f.g, f.b, 1.0f);
		mesh.addVertex({x0, y0, 0}); mesh.addVertex({x1, y0, 0}); mesh.addVertex({x1, y1, 0}); mesh.addVertex({x0, y1, 0});
		for (int i = 0; i < 4; ++i) mesh.addColor(col);
		mesh.addIndex(base); mesh.addIndex(base + 1); mesh.addIndex(base + 2);
		mesh.addIndex(base); mesh.addIndex(base + 2); mesh.addIndex(base + 3);
	};
	for (int si = 0; si < 2; ++si) {
		const OrbitSourceModel& sm = m.source(OrbitSourceId(si));
		OrbitSourceDraw& sd = d.src[si];
		for (int c = 0; c < 3; ++c) buildLines(sm.rgb[c], sd.rgbLines[c], sd.rgbDots[c]);
		buildLines(sm.luma, sd.lumaLines, sd.lumaDots);
		sd.ribbonMesh.clear();
		sd.ribbonMesh.setMode(OF_PRIMITIVE_TRIANGLES);
		for (size_t i = 0; i < sm.history.count(); ++i) {
			const HistorySegment& seg = sm.history.seg(i);
			const HistoryFill cf = sm.ribbon.fill(seg);
			if (cf.paint) quad(sd.ribbonMesh, sm.ribbon.cell(seg), cf);
		}
	}
	d.linesBuiltAtRebuild = m.rebuildCount();
	d.polylineRebuilds++;
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
		mediaViewportMesh_.draw(currentSceneTexture_, viewportBacking_);
		if (viewportBorderOn_) {
			// 1 px outline along the viewport shape (mesh vertex 0 is the fan centre).
			const auto& v = mediaViewportMesh_.mesh().getVertices();
			if (v.size() > 3) {
				ofPolyline line;
				for (size_t i = 1; i < v.size(); ++i) line.addVertex(v[i].x, v[i].y);
				line.close();
				ofPushStyle();
				ofEnableAlphaBlending();
				ofNoFill();
				ofSetLineWidth(1.0f);
				ofSetColor(viewportBorder_);
				line.draw();
				ofPopStyle();
			}
		}
	}

	// HUD-004 shell layer — after the mesh-clipped scene, before every
	// widget/overlay binding below (see setShellTexture()). Straight-alpha
	// source-over: OF_BLENDMODE_ALPHA = GL_FUNC_ADD with GL_SRC_ALPHA /
	// GL_ONE_MINUS_SRC_ALPHA in both OF renderers. Untinted (white) so the
	// sampled texel colour and alpha pass through unchanged. Full canvas,
	// canonical 1280x720 coordinates — the same space every region uses.
	if (shellTexture_ != nullptr && shellTexture_->isAllocated()) {
		ofPushStyle();
		ofEnableBlendMode(OF_BLENDMODE_ALPHA);
		ofSetColor(255, 255, 255, 255);
		shellTexture_->draw(0.0f, 0.0f, canvasWidth_, canvasHeight_);
		ofPopStyle();
	}

	// Reactive Control Artwork v0.1 — the optional pressed sprite, above the
	// shell and below every widget/label/focus bracket, only during the
	// existing accepted-activation feedback interval (same fade). Straight-
	// alpha source-over like the shell; drawn UNSCALED (1:1 canvas pixels) at
	// the rectangle derived from the region (+4 px bleed, snapped outward) —
	// see HudReactiveControlArt.h.
	// Shared button faces (idle / focused, with the active face fading over
	// the focused control) — drawn first so the legacy NEXT SCENE sprite,
	// when a skin supplies only that one, still works unchanged.
	if (hasButtonFaces()) {
		ofPushStyle();
		ofEnableBlendMode(OF_BLENDMODE_ALPHA);
		for (const auto& reg : regions_.all()) {
			if (reg.role != HudRegionRole::Controls) continue;
			if (!controlEnabled(reg.regionId).has_value()) continue; // hidden/unsupported controls keep the baked housing
			const ofRectangle b = pixelBoundsFor(reg.regionId);
			const auto face = reactive::buttonFaceRect(b.x, b.y, b.width, b.height);
			const bool focusedHere = focusedControl_ && *focusedControl_ == reg.regionId;
			const ofTexture* base = (focusedHere && buttonFocusedTexture_) ? buttonFocusedTexture_ : buttonIdleTexture_;
			if (base) {
				ofSetColor(255, 255, 255, 255);
				base->draw(float(face.x), float(face.y), float(face.width), float(face.height));
			}
			const float active = buttonActiveOpacity(reg.regionId);
			if (active > 0.0f) {
				ofSetColor(255, 255, 255, static_cast<int>(255.0f * active + 0.5f));
				buttonActiveTexture_->draw(float(face.x), float(face.y), float(face.width), float(face.height));
			}
		}
		ofPopStyle();
	}

	const float pressedOpacity = buttonActiveTexture_ ? 0.0f : pressedArtworkOpacity(); // the shared active face supersedes it
	if (pressedOpacity > 0.0f) {
		ofPushStyle();
		ofEnableBlendMode(OF_BLENDMODE_ALPHA);
		ofSetColor(255, 255, 255, static_cast<int>(255.0f * pressedOpacity + 0.5f));
		const ofRectangle b = pixelBoundsFor(pressedRegionId_);
		const auto sprite = reactive::pressedSpriteRect(b.x, b.y, b.width, b.height);
		pressedTexture_->draw(float(sprite.x), float(sprite.y), float(sprite.width), float(sprite.height));
		ofPopStyle();
	}

	// Nav-button labels sit on the skin's own faces when it supplies them
	// (light marble in the day skin, dark glass at night): palette primary
	// with the palette shadow, not the light-on-black baked-face tokens.
	HudWidgetColors widgetColors = colors_;
	if (buttonIdleTexture_ || buttonFocusedTexture_) {
		widgetColors.buttonText = widgetColors.primary;
		widgetColors.buttonShadow = widgetColors.shadow;
	}
	devTextHook() = labelTypography_; // dev typography playground (nullptr in production)
	hudTextShadowColor() = colors_.shadow;
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
	const std::unordered_map<std::string, int> regionSlotCounts = computeRegionSlotCounts();
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
		if (!bindingOccupiesSlot(compiled)) continue; // hidden: reserves no slice and draws nothing

		ofRectangle bounds = pixelBoundsFor(compiled.binding.regionId);
		const auto slotIt = regionSlotCounts.find(compiled.binding.regionId);
		const int slotCount = slotIt == regionSlotCounts.end() ? 1 : slotIt->second;
		if (slotCount > 1) {
			int slotIndex = regionSlotCursor[compiled.binding.regionId]++;
			if (it->second.vitals) { // System Performance dials: columns
				const float slotWidth = bounds.width / static_cast<float>(slotCount);
				bounds = ofRectangle(bounds.x + slotWidth * static_cast<float>(slotIndex), bounds.y, slotWidth, bounds.height);
			} else {
				float slotHeight = bounds.height / static_cast<float>(slotCount);
				bounds = ofRectangle(bounds.x, bounds.y + slotHeight * static_cast<float>(slotIndex), bounds.width, slotHeight);
			}
		}
		const std::string& regionId = compiled.binding.regionId;
		const bool focusedHere = focusedControl_ && *focusedControl_ == regionId;
		widgetColors.iconOpacity = 0.8f;
		if (focusedHere) {
			widgetColors.iconOpacity = 0.9f;
			if (controlEnabled(regionId).has_value()
				&& reactive::pressedArtworkOpacity(true, true, true, activationAccepted_, activationStart_, elapsedSeconds_, kActivationFlashSeconds) > 0.0f)
				widgetColors.iconOpacity = 1.0f;
		}
		const HudWidgetColors& colors = widgetColors; // day/night palette from the skin schedule

		uint64_t widgetStart = ofGetElapsedTimeMicros();
		if (labelTypography_) devTextRegion() = compiled.binding.regionId;
		widgets_.widgetFor(effectiveType).draw(bounds, colors, it->second);
		uint64_t widgetEnd = ofGetElapsedTimeMicros();

		stats_.activeWidgetCount++;
		stats_.perWidgetMicros.emplace_back(compiled.binding.bindingId, static_cast<double>(widgetEnd - widgetStart));
	}

	// Playable HUD Menu v0.1 — focus indicator + activation flash, drawn last
	// so it sits above the control's own label. Only for a control that is
	// actually visible this frame (hidden controls cannot hold focus).
	if (focusedControl_ && controlEnabled(*focusedControl_).has_value()) {
		const ofRectangle b = pixelBoundsFor(*focusedControl_);
		const HudWidgetColors& colors = colors_;
		ofPushStyle();
		ofEnableAlphaBlending();
		const float age = elapsedSeconds_ - activationStart_;
		// While the optional pressed artwork shows, IT is the accepted
		// feedback for that control — no accent fill over it. Refused
		// feedback, and every control without artwork, are unchanged.
		const bool pressedArtworkShowing = pressedOpacity > 0.0f || buttonActiveOpacity(*focusedControl_) > 0.0f;
		if (!pressedArtworkShowing && activationStart_ >= 0.0f && age >= 0.0f && age < kActivationFlashSeconds) {
			const float fade = 1.0f - age / kActivationFlashSeconds;
			ofColor c = activationAccepted_ ? colors.accent : colors.warning;
			c.a = static_cast<unsigned char>(45.0f * fade); // subtle (was 110)
			ofFill();
			ofSetColor(c);
			ofDrawRectangle(b);
		}
		// Reactive Control Artwork v0.1 — subtle focused-edge illumination for
		// a control whose skin provides reactive artwork: three inset hairlines
		// fading inward, inside the region's own bounds (labels are centred, so
		// the edges never cross text), slowly breathing; dimmer when the
		// control is disabled. Complements the brackets below, never replaces
		// them. No allocation: plain line primitives.
		if (pressedTexture_ != nullptr && !buttonFocusedTexture_ && *focusedControl_ == pressedRegionId_) {
			const bool enabled = controlEnabled(*focusedControl_).value_or(false);
			const float breath = 0.80f + 0.20f * std::sin(elapsedSeconds_ * 2.0f * 3.14159265f * 0.4f);
			const float strength = breath * (enabled ? 1.0f : 0.4f);
			ofNoFill();
			ofSetLineWidth(1.0f);
			const float alphas[] = {90.0f, 50.0f, 22.0f};
			for (int i = 0; i < 3; ++i) {
				const float inset = 1.0f + 1.5f * static_cast<float>(i);
				ofColor c = colors.accent;
				c.a = static_cast<unsigned char>(alphas[i] * strength);
				ofSetColor(c);
				ofDrawRectangle(b.x + inset, b.y + inset, b.width - 2.0f * inset, b.height - 2.0f * inset);
			}
		}
		// Corner brackets 3 px outside the region (inside the existing gaps),
		// so the label itself is never overdrawn.
		const ofRectangle o(b.x - 3.0f, b.y - 3.0f, b.width + 6.0f, b.height + 6.0f);
		const float arm = std::min(12.0f, o.height * 0.4f);
		ofSetColor(colors.accent);
		ofSetLineWidth(2.0f);
		auto corner = [&](float x, float y, float dx, float dy) {
			ofDrawLine(x, y, x + dx * arm, y);
			ofDrawLine(x, y, x, y + dy * arm);
		};
		corner(o.x, o.y, 1, 1);
		corner(o.getRight(), o.y, -1, 1);
		corner(o.x, o.getBottom(), 1, -1);
		corner(o.getRight(), o.getBottom(), -1, -1);
		ofPopStyle();
	}

	// Diagnostic (EXPERIENCE_RUNTIME_HUD_LOADING_CAPTURE_DIR=<dir>, off by default): save the complete HUD
	// window for the first few REAL frames whose scene health is Loading. Never changes timing or state.
	if (const char* dir = std::getenv("EXPERIENCE_RUNTIME_HUD_LOADING_CAPTURE_DIR")) {
		if (frameLoading_ && loadingCaptures_ < 6) {
			ofImage img;
			img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
			const std::string path = std::string(dir) + "/loading_" + currentSceneId_ + "_" + std::to_string(loadingCaptures_) + ".png";
			img.save(path);
			ofLogNotice("HudLoading") << "captured real Loading frame " << path;
			++loadingCaptures_;
		}
	}

	devTextHook() = nullptr;
	devTextRegion().clear();
	uint64_t drawEnd = ofGetElapsedTimeMicros();
	stats_.totalDrawMicros = static_cast<double>(drawEnd - drawStart);
	// Opt-in desktop/Pi measurement (EXPERIENCE_RUNTIME_HUD_DRAW_STATS=1): mean HUD draw cost and the
	// two orbit widgets share (mean/p50/p95/p99/max), logged every 600 frames. Off by default.
	static const bool statsOn = std::getenv("EXPERIENCE_RUNTIME_HUD_DRAW_STATS") != nullptr;
	if (statsOn) {
		static std::vector<double> totals, orbits;
		double orbitUs = 0;
		for (const auto& [id, us] : stats_.perWidgetMicros) if (id.compare(0, 15, "universal.orbit") == 0) orbitUs += us;
		totals.push_back(stats_.totalDrawMicros); orbits.push_back(orbitUs);
		if (totals.size() == 600) {
			auto summarize = [](std::vector<double> v, double out[5]) {
				std::sort(v.begin(), v.end());
				double sum = 0; for (double x : v) sum += x;
				out[0] = sum / v.size(); out[1] = v[v.size() / 2]; out[2] = v[static_cast<size_t>(v.size() * 0.95)];
				out[3] = v[static_cast<size_t>(v.size() * 0.99)]; out[4] = v.back();
			};
			double t[5], o[5];
			summarize(totals, t); summarize(orbits, o);
			ofLogNotice("HudDrawStats") << "frames=600 meanHudDrawUs=" << t[0] << " p50=" << t[1] << " p95=" << t[2] << " p99=" << t[3] << " max=" << t[4]
				<< " meanOrbitDrawUs=" << o[0] << " orbitP50=" << o[1] << " orbitP95=" << o[2] << " orbitP99=" << o[3] << " orbitMax=" << o[4]
				<< " orbitPolylineRebuilds=" << orbitDraw_.polylineRebuilds;
			totals.clear(); orbits.clear();
		}
	}
}

float HudWireframeRenderer::buttonActiveOpacity(const std::string& regionId) const {
	if (buttonActiveTexture_ == nullptr || !buttonActiveTexture_->isAllocated()) return 0.0f;
	const bool focusedIsRegion = focusedControl_ && *focusedControl_ == regionId;
	return reactive::pressedArtworkOpacity(true, focusedIsRegion, focusedIsRegion && controlEnabled(regionId).has_value(),
		activationAccepted_, activationStart_, elapsedSeconds_, kActivationFlashSeconds);
}

float HudWireframeRenderer::pressedArtworkOpacity() const {
	if (pressedTexture_ == nullptr || !pressedTexture_->isAllocated()) return 0.0f;
	const bool focusedIsRegion = focusedControl_ && *focusedControl_ == pressedRegionId_;
	const bool visible = focusedIsRegion && controlEnabled(pressedRegionId_).has_value();
	return reactive::pressedArtworkOpacity(true, focusedIsRegion, visible, activationAccepted_,
		activationStart_, elapsedSeconds_, kActivationFlashSeconds);
}

std::optional<bool> HudWireframeRenderer::controlEnabled(const std::string& regionId) const {
	for (const auto& [bindingId, input] : widgetInputs_) {
		if (input.regionId != regionId) continue;
		if (renderScope_ == RenderScope::MinimalSlice && !regionInMinimalSlice(regionId)) return std::nullopt;
		const auto* enabled = input.findRole("enabled");
		if (enabled == nullptr || !enabled->shouldRender) return std::nullopt; // not a control, or unsupported (hidden)
		return enabled->value.boolValue;
	}
	return std::nullopt;
}

} // namespace hudpresent
