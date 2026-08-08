// Standalone, dependency-free tests for the HUD presentation-model
// foundation (shared/src/hud-compositor/). Deliberately does NOT link any
// part of openFrameworks — every header this file includes is itself
// OF-independent (see HudDataTypes.h's header comment). Follows the exact
// convention already established by
// sketches/temporal-fields/test/tf_timeline_tests.cpp: a bare `c++
// -std=c++17` compile, hand-rolled assertions, one plain main(), build via
// Makefile.tests in this directory.
//
// Lives in shared/src/hud-compositor-test/ — a SIBLING of
// shared/src/hud-compositor/, not a subdirectory of it. openFrameworks'
// Makefile system recursively sweeps every subdirectory under a
// PROJECT_EXTERNAL_SOURCE_PATHS entry into an app's build (see
// shared/src/hud/README.md); a test/ subdirectory nested inside
// hud-compositor/ would get swept into any sketch that points at
// hud-compositor/ (e.g. the hud_validation_studio app) and collide with
// its own main() at link time — the exact failure tf_timeline_tests.cpp's
// own header comment already documents happening once before in this
// repo. Kept as a sibling instead, matching that precedent.
//
// All time advancement is via explicit sample(t)/update(dt) calls with
// fixed values — never wall-clock sleeps — so results are fully
// deterministic.

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../hud-compositor/FakeHudSemanticTypes.h"
#include "../hud-compositor/HudDataTypes.h"
#include "../hud-compositor/HudFormattingService.h"
#include "../hud-compositor/HudHistoryStore.h"
#include "../hud-compositor/HudMissingDataController.h"
#include "../hud-compositor/HudPresentationProfile.h"
#include "../hud-compositor/HudPresentationTypes.h"
#include "../hud-compositor/HudProfileCompiler.h"
#include "../hud-compositor/HudRegionCatalog.h"
#include "../hud-compositor/HudSourceResolver.h"
#include "../hud-compositor/HudVocabularyResolver.h"
#include "../hud-compositor/HudWidgetContract.h"
#include "../hud-compositor/HudWidgetTypes.h"
#include "../hud-compositor/widgets/HudTextMetricsCache.h"
#include "../hud-compositor/fake/FakeHudScenarioBase.h"
#include "../hud-compositor/fake/FakeHudScenarioRegistry.h"

using namespace hudpresent;

namespace {

int g_total = 0;
int g_failures = 0;

void reportFailure(const char* file, int line, const std::string& expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}

} // namespace

#define HUD_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

#define HUD_CHECK_NEAR(a, b, eps) \
	do { \
		g_total++; \
		double _a = static_cast<double>(a); \
		double _b = static_cast<double>(b); \
		if (std::abs(_a - _b) > (eps)) { \
			std::ostringstream _oss; \
			_oss << #a << " (" << _a << ") != " << #b << " (" << _b << ") within " << (eps); \
			reportFailure(__FILE__, __LINE__, _oss.str()); \
		} \
	} while (0)

#define HUD_CHECK_EQ_STR(a, b) \
	do { \
		g_total++; \
		std::string _a = (a); \
		std::string _b = (b); \
		if (_a != _b) { \
			std::ostringstream _oss; \
			_oss << #a << " (\"" << _a << "\") != " << #b << " (\"" << _b << "\")"; \
			reportFailure(__FILE__, __LINE__, _oss.str()); \
		} \
	} while (0)

namespace {

// ---------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------

FakeHudFrameData makeMinimalFrame() {
	FakeHudFrameData f;
	f.scene.sceneId = "blob-region-prototype";
	f.scene.displayTitleId = "scene.blob-region-prototype.title";
	SceneSemanticData semantic;
	semantic.state.primaryStateId = "scene.blob.state.analyzing";
	semantic.timing.activeSeconds = 12.5f;
	semantic.timing.generation = 3;
	f.scene.semantic = semantic;
	return f;
}

HudSlotBinding makeSimpleBinding(std::string id, std::string region, HudWidgetType type) {
	HudSlotBinding b;
	b.bindingId = std::move(id);
	b.regionId = std::move(region);
	b.widgetType = type;
	return b;
}

// ---------------------------------------------------------------------
// "missing versus zero"
// ---------------------------------------------------------------------
void test_missing_vs_zero() {
	HudSourceResolver resolver;

	FakeHudFrameData zeroFrame = makeMinimalFrame();
	zeroFrame.scene.semantic->activity.overall = 0.0f; // present, exactly zero

	FakeHudFrameData missingFrame = makeMinimalFrame();
	// activity.overall left as std::nullopt (default)

	auto zeroResult = resolver.resolve("scene.activity.overall", zeroFrame);
	auto missingResult = resolver.resolve("scene.activity.overall", missingFrame);

	HUD_CHECK(zeroResult.has_value());
	HUD_CHECK(zeroResult->present == true);
	HUD_CHECK_NEAR(zeroResult->numberValue, 0.0f, 1e-6);

	HUD_CHECK(missingResult.has_value()); // known source, just unpopulated
	HUD_CHECK(missingResult->present == false);
}

// ---------------------------------------------------------------------
// "missing versus empty list"
//
// Engineering Session 2 reconciliation finding: the real contract has no
// separate, optional "effects snapshot" a scene can omit entirely —
// SceneHudStatus::activeEffects is a plain, always-present
// std::vector<string> (confirmed: shared/src/scene/SceneContract.h; no
// HudFrameData.effects field exists at all — see
// HudSourceResolver.cpp's canonicalTable() comment). So "effects.active"
// itself can never be "missing" the way, say, "scene.state.primary" can
// (whose whole containing `semantic` payload IS optional) — it is always
// present, and its only two states are "present, empty" and "present,
// populated." This test now verifies exactly that: emptiness is
// preserved as a real, distinct, non-missing state, and — the part that
// actually exercises the "missing" side of the distinction — that
// activeEffectIds stays present-and-resolvable even when the surrounding
// `scene.semantic` payload is entirely absent (Loading-shaped frame),
// since activeEffects structurally lives OUTSIDE `semantic`.
// ---------------------------------------------------------------------
void test_missing_vs_empty_list() {
	HudSourceResolver resolver;

	FakeHudFrameData emptyListFrame = makeMinimalFrame();
	// activeEffectIds left as its default-constructed empty vector.

	auto emptyResult = resolver.resolve("effects.active", emptyListFrame);
	HUD_CHECK(emptyResult.has_value());
	HUD_CHECK(emptyResult->present == true);
	HUD_CHECK(emptyResult->listValue.empty());
	HUD_CHECK(emptyResult->isEmptyList());

	FakeHudFrameData noSemanticFrame = makeMinimalFrame();
	noSemanticFrame.scene.semantic = std::nullopt; // Loading-shaped: no semantic payload at all
	auto stillResolvable = resolver.resolve("effects.active", noSemanticFrame);
	HUD_CHECK(stillResolvable.has_value());
	HUD_CHECK(stillResolvable->present == true); // activeEffects is NOT inside `semantic` — unaffected by its absence
	HUD_CHECK(stillResolvable->isEmptyList());

	FakeHudFrameData populatedFrame = makeMinimalFrame();
	populatedFrame.scene.activeEffectIds = {"Edge Glow"};
	auto populatedResult = resolver.resolve("effects.active", populatedFrame);
	HUD_CHECK(populatedResult.has_value());
	HUD_CHECK(populatedResult->present == true);
	HUD_CHECK(!populatedResult->isEmptyList());

	// A genuinely "missing" (not just empty) value still exists elsewhere
	// in this same frame — scene.state.primary, whose containing
	// `semantic` payload IS optional — preserving the missing-vs-empty
	// distinction as a real, tested behavior in this domain, just not via
	// effects.active specifically anymore.
	auto missingState = resolver.resolve("scene.state.primary", noSemanticFrame);
	HUD_CHECK(missingState.has_value());
	HUD_CHECK(missingState->present == false);
}

// ---------------------------------------------------------------------
// "source lookup"
// ---------------------------------------------------------------------
void test_source_lookup() {
	HudSourceResolver resolver;
	FakeHudFrameData frame = makeMinimalFrame();
	frame.runtime.fps = 29.5f;
	frame.controls.reseed = true;
	frame.controls.sceneNext = false;

	auto scene = resolver.resolve("scene.id", frame);
	HUD_CHECK(scene && scene->present && scene->textValue == "blob-region-prototype");

	auto fps = resolver.resolve("runtime.fps", frame);
	HUD_CHECK(fps && fps->present);
	HUD_CHECK_NEAR(fps->numberValue, 29.5f, 1e-6);

	auto reseed = resolver.resolve("control.scene.reseed.available", frame);
	HUD_CHECK(reseed && reseed->present && reseed->boolValue == true);

	auto sceneNext = resolver.resolve("control.scene.next.available", frame);
	HUD_CHECK(sceneNext && sceneNext->present && sceneNext->boolValue == false);

	// Every documented canonical ID must at least resolve to *something*
	// (present or missing, never nullopt) against a default-constructed
	// frame — a cheap exhaustiveness check against the task's own list.
	FakeHudFrameData blank;
	for (const auto& id : HudSourceResolver::allCanonicalSourceIds()) {
		auto v = resolver.resolve(id, blank);
		HUD_CHECK(v.has_value());
	}
}

// ---------------------------------------------------------------------
// "metric lookup"
// ---------------------------------------------------------------------
void test_metric_lookup() {
	HudSourceResolver resolver;
	FakeHudFrameData frame = makeMinimalFrame();

	SceneMetric m;
	m.metricId = "scene.blob.metric.region_count";
	m.valueType = HudMetricValueType::Count;
	m.dataClass = HudDataClass::Literal;
	m.value = 7.0f;
	frame.scene.semantic->metrics.push_back(m);

	auto found = resolver.resolve("scene.blob.metric.region_count", frame);
	HUD_CHECK(found && found->present);
	HUD_CHECK_NEAR(found->numberValue, 7.0f, 1e-6);

	// Metric-shaped but not present in this snapshot: "missing", not "unknown".
	auto notPresentButShaped = resolver.resolve("scene.blob.metric.fragment_count", frame);
	HUD_CHECK(notPresentButShaped.has_value());
	HUD_CHECK(notPresentButShaped->present == false);

	HUD_CHECK(HudSourceResolver::looksLikeSceneMetricId("scene.blob.metric.region_count"));
	HUD_CHECK(!HudSourceResolver::looksLikeSceneMetricId("scene.activity.overall"));
}

// ---------------------------------------------------------------------
// "unknown source"
// ---------------------------------------------------------------------
void test_unknown_source() {
	HudSourceResolver resolver;
	FakeHudFrameData frame = makeMinimalFrame();

	auto result = resolver.resolve("scene.activity.bogus_signal", frame); // not canonical, not metric-shaped
	HUD_CHECK(!result.has_value());

	HUD_CHECK(!HudSourceResolver::isKnownCanonicalSource("scene.activity.bogus_signal"));
	HUD_CHECK(!HudSourceResolver::staticValueTypeOf("scene.activity.bogus_signal").has_value());
}

// ---------------------------------------------------------------------
// "widget/source type compatibility"
// ---------------------------------------------------------------------
void test_widget_source_type_compatibility() {
	HudRegionCatalog regions;
	HudVocabularyResolver vocab;
	HudProfileCompiler compiler(regions, vocab);

	// ProgressBar's "value" role requires Ratio; scene.state.primary is
	// Identifier — a deliberate mismatch.
	auto b = makeSimpleBinding("test.mismatch", "universal.activity", HudWidgetType::ProgressBar);
	b.addSource({"value", "scene.state.primary", true});

	auto compiled = compiler.compileBindings("blob-region-prototype", {b});
	HUD_CHECK(compiled.hasErrors());
	HUD_CHECK(!compiled.bindings.empty() && compiled.bindings.front().valid == false);

	bool foundMismatch = false;
	for (const auto& issue : compiled.issues) {
		if (issue.kind == HudProfileIssueKind::SourceValueTypeMismatch) foundMismatch = true;
	}
	HUD_CHECK(foundMismatch);
}

// ---------------------------------------------------------------------
// "unknown region"
// ---------------------------------------------------------------------
void test_unknown_region() {
	HudRegionCatalog regions;
	HudVocabularyResolver vocab;
	HudProfileCompiler compiler(regions, vocab);

	auto b = makeSimpleBinding("test.unknown_region", "no.such.region", HudWidgetType::Label);
	b.labelVocabularyId = "control.scene.reseed.label"; // give Label a valid caption so this is the ONLY error

	auto compiled = compiler.compileBindings("blob-region-prototype", {b});
	HUD_CHECK(compiled.hasErrors());
	HUD_CHECK(compiled.bindings.front().valid == false);

	bool foundUnknownRegion = false;
	for (const auto& issue : compiled.issues) {
		if (issue.kind == HudProfileIssueKind::UnknownRegion) foundUnknownRegion = true;
	}
	HUD_CHECK(foundUnknownRegion);
}

// ---------------------------------------------------------------------
// "invalid widget for region"
// ---------------------------------------------------------------------
void test_invalid_widget_for_region() {
	HudRegionCatalog regions;
	HudVocabularyResolver vocab;
	HudProfileCompiler compiler(regions, vocab);

	// controls.scene_previous only accepts Label/BindingPlaceholder.
	auto b = makeSimpleBinding("test.bad_widget", "controls.scene_previous", HudWidgetType::Sparkline);
	b.addSource({"value", "scene.activity.overall", true});

	auto compiled = compiler.compileBindings("blob-region-prototype", {b});
	HUD_CHECK(compiled.hasErrors());
	bool foundNotAllowed = false;
	for (const auto& issue : compiled.issues) {
		if (issue.kind == HudProfileIssueKind::WidgetNotAllowedInRegion) foundNotAllowed = true;
	}
	HUD_CHECK(foundNotAllowed);
}

// ---------------------------------------------------------------------
// "required source role missing"
// ---------------------------------------------------------------------
void test_required_source_role_missing() {
	HudRegionCatalog regions;
	HudVocabularyResolver vocab;
	HudProfileCompiler compiler(regions, vocab);

	// NumericValue requires role "value" — deliberately not bound.
	auto b = makeSimpleBinding("test.missing_role", "universal.timing", HudWidgetType::NumericValue);

	auto compiled = compiler.compileBindings("blob-region-prototype", {b});
	HUD_CHECK(compiled.hasErrors());
	bool foundMissingRole = false;
	for (const auto& issue : compiled.issues) {
		if (issue.kind == HudProfileIssueKind::RequiredSourceRoleMissing) foundMissingRole = true;
	}
	HUD_CHECK(foundMissingRole);
}

// ---------------------------------------------------------------------
// "flexible-region occupancy conflict"
// ---------------------------------------------------------------------
void test_flexible_region_occupancy_conflict() {
	HudRegionCatalog regions;
	HudVocabularyResolver vocab;
	HudProfileCompiler compiler(regions, vocab);

	auto first = makeSimpleBinding("test.occ_first", "flexible.primary", HudWidgetType::AmbientField);
	auto second = makeSimpleBinding("test.occ_second", "flexible.primary", HudWidgetType::AmbientField);

	auto compiled = compiler.compileBindings("blob-region-prototype", {first, second});
	HUD_CHECK(compiled.hasErrors());
	HUD_CHECK(compiled.bindings.size() == 2);
	HUD_CHECK(compiled.bindings[0].valid == true);  // first claimant keeps the region
	HUD_CHECK(compiled.bindings[1].valid == false); // second is the conflict

	bool foundConflict = false;
	for (const auto& issue : compiled.issues) {
		if (issue.kind == HudProfileIssueKind::FlexibleRegionOccupancyConflict) foundConflict = true;
	}
	HUD_CHECK(foundConflict);
}

// ---------------------------------------------------------------------
// "invalid flexible bindings must not remove universal HUD content"
// (bonus, directly tied to the occupancy/unknown-scene tests above)
// ---------------------------------------------------------------------
void test_invalid_flexible_does_not_remove_universal() {
	HudRegionCatalog regions;
	HudVocabularyResolver vocab;
	HudPresentationProfileRegistry registry;
	HudProfileCompiler compiler(regions, vocab);

	auto validCompiled = compiler.compile("blob-region-prototype", registry);
	auto unknownSceneCompiled = compiler.compile("totally-unrecognized-scene-id", registry);

	size_t universalPlusOverlayCount =
		registry.universalProfile().bindings.size() +
		registry.overlayHealthProfile().bindings.size() +
		registry.overlayTransitionProfile().bindings.size();

	HUD_CHECK(!unknownSceneCompiled.hasErrors());
	HUD_CHECK(unknownSceneCompiled.bindings.size() == universalPlusOverlayCount);
	HUD_CHECK(validCompiled.bindings.size() > universalPlusOverlayCount); // has flexible content too
}

// ---------------------------------------------------------------------
// "vocabulary fallback"
// ---------------------------------------------------------------------
void test_vocabulary_fallback() {
	HudVocabularyResolver vocab;

	// Deliberately unregistered key.
	HUD_CHECK_EQ_STR(vocab.resolve("", "totally.unregistered.key"), "totally.unregistered.key");
	HUD_CHECK(!vocab.resolvedThroughVocabulary("", "totally.unregistered.key"));

	// A real, in-use key deliberately left out of the canonical table
	// (see HudVocabularyResolver.cpp's comment) — the fake Contour
	// scenario's "reforming" state.
	HUD_CHECK_EQ_STR(vocab.resolve("contour-portrait", "scene.contour.state.reforming"), "scene.contour.state.reforming");
	HUD_CHECK(!vocab.resolvedThroughVocabulary("contour-portrait", "scene.contour.state.reforming"));

	// A registered key resolves through vocabulary, not to itself.
	HUD_CHECK_EQ_STR(vocab.resolve("", "ready"), "READY");
	HUD_CHECK(vocab.resolvedThroughVocabulary("", "ready"));
}

// ---------------------------------------------------------------------
// "scene override"
// ---------------------------------------------------------------------
void test_scene_override() {
	HudVocabularyResolver vocab;

	// Canonical default, no override.
	HUD_CHECK_EQ_STR(vocab.resolve("temporal-fields", "control.scene.reseed.label"), "RESEED");

	// Blob has a scene override registered in HudVocabularyResolver's
	// constructor — must win over canonical.
	HUD_CHECK_EQ_STR(vocab.resolve("blob-region-prototype", "control.scene.reseed.label"), "GENERATE VARIANT");

	// Scene override must also win over a selected pack that ALSO
	// defines this key (resolution order: scene override highest).
	vocab.selectPack("alt-pack");
	HUD_CHECK_EQ_STR(vocab.resolve("blob-region-prototype", "control.scene.reseed.label"), "GENERATE VARIANT");
	// A scene with no override falls through to the pack instead.
	HUD_CHECK_EQ_STR(vocab.resolve("temporal-fields", "control.scene.reseed.label"), "RECONFIGURE");
	vocab.selectPack("");
}

// ---------------------------------------------------------------------
// "missing-data policy"
// ---------------------------------------------------------------------
void test_missing_data_policy() {
	HudMissingDataController controller;

	HudCompiledBinding hideBinding;
	hideBinding.binding.widgetType = HudWidgetType::EffectChips;
	hideBinding.binding.missingPolicy = HudMissingPolicy::Hide;

	auto missing = HudResolvedValue::missing(HudSourceValueType::IdentifierList, HudDataClass::Literal);

	// Hide: missing -> shouldRender=false.
	{
		auto outcome = controller.decide(hideBinding, missing, std::nullopt, FakeSceneHealth::Ready, std::nullopt);
		HUD_CHECK(outcome.shouldRender == false);
	}

	// Placeholder: missing -> shouldRender=true, an Ambient-classed stand-in.
	{
		HudCompiledBinding b = hideBinding;
		b.binding.missingPolicy = HudMissingPolicy::Placeholder;
		auto outcome = controller.decide(b, missing, std::nullopt, FakeSceneHealth::Ready, std::nullopt);
		HUD_CHECK(outcome.shouldRender == true);
		HUD_CHECK(outcome.valueToShow.dataClass == HudDataClass::Ambient);
	}

	// RetainLastValid: missing + no lastValid -> hidden; missing + lastValid -> shown.
	{
		HudCompiledBinding b = hideBinding;
		b.binding.missingPolicy = HudMissingPolicy::RetainLastValid;
		auto outcomeNoLast = controller.decide(b, missing, std::nullopt, FakeSceneHealth::Ready, std::nullopt);
		HUD_CHECK(outcomeNoLast.shouldRender == false);

		auto lastValid = HudResolvedValue::list(HudIdentifierList{}, HudDataClass::Literal);
		auto outcomeWithLast = controller.decide(b, missing, lastValid, FakeSceneHealth::Ready, std::nullopt);
		HUD_CHECK(outcomeWithLast.shouldRender == true);
		HUD_CHECK(outcomeWithLast.dimmed == false);
	}

	// DimLastValid: same as above but always dimmed.
	{
		HudCompiledBinding b = hideBinding;
		b.binding.missingPolicy = HudMissingPolicy::DimLastValid;
		auto lastValid = HudResolvedValue::list(HudIdentifierList{}, HudDataClass::Literal);
		auto outcome = controller.decide(b, missing, lastValid, FakeSceneHealth::Ready, std::nullopt);
		HUD_CHECK(outcome.shouldRender == true);
		HUD_CHECK(outcome.dimmed == true);
	}

	// UseFallbackSource: missing + no fallback -> hidden; missing + fallback -> shown.
	{
		HudCompiledBinding b = hideBinding;
		b.binding.missingPolicy = HudMissingPolicy::UseFallbackSource;
		auto outcomeNoFallback = controller.decide(b, missing, std::nullopt, FakeSceneHealth::Ready, std::nullopt);
		HUD_CHECK(outcomeNoFallback.shouldRender == false);

		auto fallback = HudResolvedValue::identifier("fallback-value");
		auto outcomeWithFallback = controller.decide(b, missing, std::nullopt, FakeSceneHealth::Ready, fallback);
		HUD_CHECK(outcomeWithFallback.shouldRender == true);
		HUD_CHECK_EQ_STR(outcomeWithFallback.valueToShow.textValue, "fallback-value");
	}

	// UseAmbientFallback: always renders via the ambient path.
	{
		HudCompiledBinding b = hideBinding;
		b.binding.missingPolicy = HudMissingPolicy::UseAmbientFallback;
		auto outcome = controller.decide(b, missing, std::nullopt, FakeSceneHealth::Ready, std::nullopt);
		HUD_CHECK(outcome.shouldRender == true);
		HUD_CHECK(outcome.useAmbientFallback == true);
	}

	// Unknown source (std::nullopt from the resolver) is always hidden,
	// regardless of policy.
	{
		HudCompiledBinding b = hideBinding;
		b.binding.missingPolicy = HudMissingPolicy::Placeholder;
		auto outcome = controller.decide(b, std::nullopt, std::nullopt, FakeSceneHealth::Ready, std::nullopt);
		HUD_CHECK(outcome.shouldRender == false);
	}

	// Failure suppresses live Sparkline traces regardless of policy.
	{
		HudCompiledBinding b;
		b.binding.widgetType = HudWidgetType::Sparkline;
		b.binding.missingPolicy = HudMissingPolicy::RetainLastValid;
		auto present = HudResolvedValue::number(HudSourceValueType::Ratio, 0.5f, HudDataClass::Normalized);
		auto outcome = controller.decide(b, present, std::nullopt, FakeSceneHealth::Failed, std::nullopt);
		HUD_CHECK(outcome.shouldRender == false);
	}

	// Loading dims an otherwise-present value.
	{
		HudCompiledBinding b;
		b.binding.widgetType = HudWidgetType::NumericValue;
		b.binding.missingPolicy = HudMissingPolicy::Hide;
		auto present = HudResolvedValue::number(HudSourceValueType::Scalar, 3.0f, HudDataClass::Literal);
		auto outcome = controller.decide(b, present, std::nullopt, FakeSceneHealth::Loading, std::nullopt);
		HUD_CHECK(outcome.shouldRender == true);
		HUD_CHECK(outcome.dimmed == true);
	}

	// Region-level ambient fallback: only when EVERY binding in the region failed.
	{
		HudMissingDataController::Outcome allHidden;
		allHidden.shouldRender = false;
		HudMissingDataController::Outcome oneShown;
		oneShown.shouldRender = true;

		HUD_CHECK(HudMissingDataController::regionNeedsAmbientFallback({allHidden, allHidden}) == true);
		HUD_CHECK(HudMissingDataController::regionNeedsAmbientFallback({allHidden, oneShown}) == false);
		HUD_CHECK(HudMissingDataController::regionNeedsAmbientFallback({}) == false); // unbound region, not this domain's concern
	}
}

// ---------------------------------------------------------------------
// "history sample cadence"
// ---------------------------------------------------------------------
void test_history_sample_cadence() {
	HudHistoryStore store;
	HUD_CHECK(store.track("scene.activity.overall", HudHistoryRequest{16, 2.0f})); // 2 Hz -> 0.5s interval

	// 1.0 simulated second at dt=0.1 -> exactly 2 samples should land
	// (at accumSeconds crossing 0.5 and 1.0).
	for (int i = 0; i < 10; ++i) {
		store.update(0.1f);
		store.pushSample("scene.activity.overall", 0.5f);
	}

	auto samples = store.samplesOldestToNewest("scene.activity.overall");
	HUD_CHECK(samples.size() == 2);

	// A second track() call for an already-tracked source is a no-op
	// (returns false), and capacity is respected.
	HUD_CHECK(store.track("scene.activity.overall", HudHistoryRequest{16, 2.0f}) == false);
	HUD_CHECK(store.trackedSignalCount() == 1);

	// Fill kMaxTrackedSignals to confirm capacity is actually enforced
	// (fixed-size, no growth).
	HudHistoryStore capStore;
	for (size_t i = 0; i < HudHistoryStore::kMaxTrackedSignals; ++i) {
		HUD_CHECK(capStore.track("signal_" + std::to_string(i), HudHistoryRequest{8, 4.0f}));
	}
	HUD_CHECK(capStore.track("one_too_many", HudHistoryRequest{8, 4.0f}) == false);
	HUD_CHECK(capStore.trackedSignalCount() == HudHistoryStore::kMaxTrackedSignals);
}

// ---------------------------------------------------------------------
// "history reset on scene epoch"
// ---------------------------------------------------------------------
void test_history_reset_on_scene_epoch() {
	HudHistoryStore store;
	store.track("scene.activity.motion", HudHistoryRequest{8, 10.0f}); // 0.1s interval
	for (int i = 0; i < 5; ++i) {
		store.update(0.1f);
		store.pushSample("scene.activity.motion", 0.25f * static_cast<float>(i));
	}
	HUD_CHECK(store.stats("scene.activity.motion").hasData == true);

	store.resetForSceneEpoch(42);
	HUD_CHECK(store.currentEpoch() == 42);
	HUD_CHECK(store.isTracked("scene.activity.motion") == true); // still tracked, just cleared
	HUD_CHECK(store.stats("scene.activity.motion").hasData == false);
	HUD_CHECK(store.samplesOldestToNewest("scene.activity.motion").empty());

	// Gap markers: pushing std::nullopt never inserts a fabricated zero.
	HudHistoryStore gapStore;
	gapStore.track("x", HudHistoryRequest{4, 10.0f});
	gapStore.update(1.0f);
	gapStore.pushSample("x", std::nullopt);
	auto gapSamples = gapStore.samplesOldestToNewest("x");
	HUD_CHECK(gapSamples.size() == 1);
	HUD_CHECK(gapSamples[0].gap == true);

	// Paused sampling freezes the trace entirely (no gap-fill either).
	HudHistoryStore pausedStore;
	pausedStore.track("y", HudHistoryRequest{4, 10.0f});
	pausedStore.pause("y", true);
	for (int i = 0; i < 5; ++i) {
		pausedStore.update(1.0f);
		pausedStore.pushSample("y", 1.0f);
	}
	HUD_CHECK(pausedStore.samplesOldestToNewest("y").empty());

	// Generation-change marker.
	HudHistoryStore eventStore;
	eventStore.track("z", HudHistoryRequest{4, 10.0f});
	eventStore.markGenerationChange("z");
	eventStore.update(1.0f);
	eventStore.pushSample("z", 5.0f);
	HUD_CHECK(eventStore.stats("z").eventPulse == true);
	eventStore.update(1.0f);
	eventStore.pushSample("z", 6.0f);
	HUD_CHECK(eventStore.stats("z").eventPulse == false); // only the marked write is flagged
}

// ---------------------------------------------------------------------
// "deterministic fake output"
// ---------------------------------------------------------------------
void test_deterministic_fake_output() {
	FakeHudScenarioRegistry registry;

	for (const auto& sceneId : registry.sceneIds()) {
		auto a = registry.create(sceneId);
		auto b = registry.create(sceneId);
		HUD_CHECK(a != nullptr);
		HUD_CHECK(b != nullptr);
		if (!a || !b) continue;

		a->reset(1234);
		b->reset(1234);

		for (float t : {0.0f, 3.7f, 12.25f, 40.1f, 55.999f}) {
			auto frameA = a->sample(t);
			auto frameB = b->sample(t);

			HUD_CHECK_EQ_STR(frameA.scene.sceneId, frameB.scene.sceneId);
			HUD_CHECK(frameA.scene.semantic.has_value() == frameB.scene.semantic.has_value());
			if (frameA.scene.semantic && frameB.scene.semantic) {
				HUD_CHECK_EQ_STR(frameA.scene.semantic->state.primaryStateId, frameB.scene.semantic->state.primaryStateId);
				HUD_CHECK(frameA.scene.semantic->metrics.size() == frameB.scene.semantic->metrics.size());
				HUD_CHECK_NEAR(frameA.scene.semantic->timing.activeSeconds, frameB.scene.semantic->timing.activeSeconds, 1e-6);
				if (frameA.scene.semantic->activity.overall && frameB.scene.semantic->activity.overall) {
					HUD_CHECK_NEAR(*frameA.scene.semantic->activity.overall, *frameB.scene.semantic->activity.overall, 1e-6);
				} else {
					HUD_CHECK(frameA.scene.semantic->activity.overall.has_value() == frameB.scene.semantic->activity.overall.has_value());
				}
			}
			if (frameA.runtime.fps && frameB.runtime.fps) {
				HUD_CHECK_NEAR(*frameA.runtime.fps, *frameB.runtime.fps, 1e-6);
			}
		}

		// A different seed must be able to produce a different result
		// somewhere in the Nominal window (sanity check that this isn't
		// trivially constant regardless of seed).
		auto c = registry.create(sceneId);
		c->reset(9999);
		auto frameNominalA = a->sample(1.0f);
		auto frameNominalC = c->sample(1.0f);
		bool anyDifference =
			(frameNominalA.scene.semantic && frameNominalC.scene.semantic &&
				frameNominalA.scene.semantic->activity.overall != frameNominalC.scene.semantic->activity.overall) ||
			(frameNominalA.scene.semantic && frameNominalC.scene.semantic &&
				frameNominalA.scene.semantic->state.primaryStateId != frameNominalC.scene.semantic->state.primaryStateId);
		HUD_CHECK(anyDifference);
	}

	// Every required case phase must be reachable and itself deterministic.
	auto blob = registry.create("blob-region-prototype");
	blob->reset(7);
	for (int i = 0; i < static_cast<int>(FakeCasePhase::kCount); ++i) {
		auto phase = static_cast<FakeCasePhase>(i);
		float t = FakeHudScenarioBase::elapsedSecondsForPhase(phase, 1.0f);
		HUD_CHECK(FakeHudScenarioBase::phaseForElapsed(t) == phase);

		auto frame1 = blob->sample(t);
		auto frame2 = blob->sample(t);
		HUD_CHECK(frame1.scene.semantic.has_value() == frame2.scene.semantic.has_value());
		if (frame1.scene.semantic && frame2.scene.semantic) {
			HUD_CHECK_EQ_STR(frame1.scene.semantic->state.primaryStateId, frame2.scene.semantic->state.primaryStateId);
		}
		HUD_CHECK(frame1.scene.health == frame2.scene.health);
	}

	// Spot-check a few of the required cases actually produce the
	// documented shape, not just "runs without crashing".
	auto quadrant = registry.create("quadrant-crosshair");
	quadrant->reset(3);

	auto loadingFrame = quadrant->sample(FakeHudScenarioBase::elapsedSecondsForPhase(FakeCasePhase::Loading, 1.0f));
	HUD_CHECK(loadingFrame.scene.health == FakeSceneHealth::Loading);
	HUD_CHECK(!loadingFrame.media.has_value());
	HUD_CHECK(!loadingFrame.scene.semantic.has_value()); // Loading = no semantic payload at all

	auto failedFrame = quadrant->sample(FakeHudScenarioBase::elapsedSecondsForPhase(FakeCasePhase::Failed, 1.0f));
	HUD_CHECK(failedFrame.scene.health == FakeSceneHealth::Failed);
	HUD_CHECK(failedFrame.scene.messageId.has_value());

	auto maxMetricFrame = quadrant->sample(FakeHudScenarioBase::elapsedSecondsForPhase(FakeCasePhase::MaxMetricCount, 1.0f));
	HUD_CHECK(maxMetricFrame.scene.semantic.has_value());
	HUD_CHECK(maxMetricFrame.scene.semantic->metrics.size() == kMaxSceneMetrics);

	auto maxEffectFrame = quadrant->sample(FakeHudScenarioBase::elapsedSecondsForPhase(FakeCasePhase::MaxEffectList, 1.0f));
	HUD_CHECK(maxEffectFrame.scene.activeEffectIds.size() == kMaxActiveEffectIds);

	auto emptyFlexFrame = quadrant->sample(FakeHudScenarioBase::elapsedSecondsForPhase(FakeCasePhase::EmptyFlexible, 1.0f));
	HUD_CHECK(emptyFlexFrame.scene.semantic.has_value());
	HUD_CHECK(emptyFlexFrame.scene.semantic->metrics.empty());

	auto longStringsFrame = quadrant->sample(FakeHudScenarioBase::elapsedSecondsForPhase(FakeCasePhase::LongestStrings, 1.0f));
	HUD_CHECK(longStringsFrame.scene.displayTitleId.size() > 40);

	// registry.create() rejects an unrecognized scene ID cleanly.
	HUD_CHECK(registry.create("not-a-real-scene") == nullptr);
}

// ---------------------------------------------------------------------
// "no scene ID passed to widget implementations"
//
// The widget layer itself (shared/src/hud-compositor/widgets/) is
// openFrameworks-dependent and therefore not part of this dependency-free
// binary — this is enforced structurally there: IHudWidget's draw()/
// update() methods (HudWidgetBase.h) take no scene-identifying parameter
// at all, only a resolved HudBoundValue/HudResolvedValue. What IS
// verifiable here, in the dependency-free logic layer, is the other half
// of the guarantee: that scene ID is consumed at exactly one boundary
// (HudProfileCompiler::compile()'s sceneId parameter, which selects a
// profile and threads scene context through to vocabulary resolution)
// and never leaks into the compiled, scene-agnostic HudSlotBinding/
// HudCompiledBinding values a widget would actually receive.
// ---------------------------------------------------------------------
void test_no_scene_id_leaks_into_compiled_bindings() {
	HudRegionCatalog regions;
	HudVocabularyResolver vocab;
	HudPresentationProfileRegistry registry;
	HudProfileCompiler compiler(regions, vocab);

	auto blobCompiled = compiler.compile("blob-region-prototype", registry);
	auto temporalCompiled = compiler.compile("temporal-fields", registry);

	// The universal bindings' bindingId/regionId/sourceId strings must be
	// byte-identical across scenes — nothing scene-specific leaked into
	// them during compilation (only resolvedLabelText, a vocabulary
	// OUTPUT, is allowed to differ per scene, and only via the compiler's
	// single sceneId parameter, never inside the binding shape itself).
	for (const auto& universalBinding : registry.universalProfile().bindings) {
		auto findIn = [&](const HudCompiledProfile& profile) -> const HudCompiledBinding* {
			for (const auto& cb : profile.bindings) {
				if (cb.binding.bindingId == universalBinding.bindingId) return &cb;
			}
			return nullptr;
		};
		const auto* inBlob = findIn(blobCompiled);
		const auto* inTemporal = findIn(temporalCompiled);
		HUD_CHECK(inBlob != nullptr);
		HUD_CHECK(inTemporal != nullptr);
		if (inBlob && inTemporal) {
			HUD_CHECK_EQ_STR(inBlob->binding.regionId, inTemporal->binding.regionId);
			HUD_CHECK(inBlob->binding.sourceCount == inTemporal->binding.sourceCount);
			for (size_t i = 0; i < inBlob->binding.sourceCount; ++i) {
				HUD_CHECK_EQ_STR(inBlob->binding.sources[i].sourceId, inTemporal->binding.sources[i].sourceId);
			}
		}
	}
}

// ---------------------------------------------------------------------
// Formatting spot-checks (supports several of the above; not one of the
// task's explicitly-named list items on its own, kept small).
// ---------------------------------------------------------------------
// Architecture-Closure Session — programmatic bounds/safe-margin
// validation (closure plan §7.2), computed at the canonical 1280x720
// canvas. Two checks: (1) every region stays fully within the canvas
// itself (no region defined partly or fully off-canvas), and (2) no two
// regions of the SAME role overlap each other — Universal-vs-Universal,
// Controls-vs-Controls, Flexible-vs-Flexible. Overlay-role regions are
// deliberately EXEMPT from the cross-region check: they are designed to
// sit ON TOP of media_viewport (a Universal region) by definition — an
// "overlay" that never overlapped anything would be a contradiction, not
// a safety property — see docs/reviews/hud-wireframe-bounds-v1.md for the
// full per-region table and this exemption's own note.
void test_region_bounds_within_canvas_and_no_same_role_overlap() {
	constexpr float kCanvasW = 1280.0f;
	constexpr float kCanvasH = 720.0f;

	struct PixelRect {
		float x, y, w, h;
		bool overlaps(const PixelRect& o) const {
			return x < o.x + o.w && o.x < x + w && y < o.y + o.h && o.y < y + h;
		}
	};

	HudRegionCatalog catalog;
	for (const auto& region : catalog.all()) {
		PixelRect r{region.bounds.x * kCanvasW, region.bounds.y * kCanvasH,
			region.bounds.width * kCanvasW, region.bounds.height * kCanvasH};
		bool withinCanvas = r.x >= 0.0f && r.y >= 0.0f
			&& (r.x + r.w) <= kCanvasW && (r.y + r.h) <= kCanvasH;
		HUD_CHECK(withinCanvas);
	}

	const auto& regions = catalog.all();
	for (size_t i = 0; i < regions.size(); ++i) {
		for (size_t j = i + 1; j < regions.size(); ++j) {
			if (regions[i].role != regions[j].role) continue;
			if (regions[i].role == HudRegionRole::Overlay) continue; // see this test's own header comment
			PixelRect a{regions[i].bounds.x * kCanvasW, regions[i].bounds.y * kCanvasH,
				regions[i].bounds.width * kCanvasW, regions[i].bounds.height * kCanvasH};
			PixelRect b{regions[j].bounds.x * kCanvasW, regions[j].bounds.y * kCanvasH,
				regions[j].bounds.width * kCanvasW, regions[j].bounds.height * kCanvasH};
			bool overlaps = a.overlaps(b);
			if (overlaps) {
				std::cerr << "  (same-role overlap: " << regions[i].regionId << " / " << regions[j].regionId << ")\n";
			}
			HUD_CHECK(!overlaps);
		}
	}
}

// HudTextMetricsCache::truncateToWidth()'s own core invariant, fuzzed
// across a battery of representative (text, budget) pairs — the
// structural proof behind "text measured bounds ⊆ text live bounds"
// (closure plan §7.2). This is dependency-free (no OF headers needed —
// HudTextMetricsCache lives in widgets/ but has no OF dependency, see
// that file's own header comment) even though it's declared alongside
// region-catalog checks here rather than in the OF-touching test tier.
void test_truncation_never_exceeds_budget() {
	HudTextMetricsCache cache;
	const std::vector<std::string> strings = {
		"", "A", "READY", "An Extremely Long Scene Display Name That Cannot Possibly Fit Its Region",
		"A Deliberately Overlong Media Title Used Only To Exercise Deterministic Ellipsis Truncation",
		"heatmap_recolor", "scene.blob.state.analyzing_region_boundaries_carefully",
	};
	const std::vector<float> budgets = {0.0f, 1.0f, 8.0f, 23.0f, 24.0f, 25.0f, 50.0f, 99.52f, 253.12f, 406.72f, 1000.0f};

	for (const auto& text : strings) {
		for (float budget : budgets) {
			const auto& result = cache.truncateToWidth(text, budget);
			HUD_CHECK(result.widthLocal <= budget + 1e-4f);
		}
	}
}

void test_formatting_spot_checks() {
	HudVocabularyResolver vocab;

	auto ratio = HudResolvedValue::number(HudSourceValueType::Ratio, 0.5f, HudDataClass::Normalized);
	HUD_CHECK_EQ_STR(HudFormattingService::format(ratio, HudFormatKind::Percentage, "", vocab), "50%");

	auto duration = HudResolvedValue::number(HudSourceValueType::DurationSeconds, 75.0f, HudDataClass::Literal);
	HUD_CHECK_EQ_STR(HudFormattingService::format(duration, HudFormatKind::Duration, "", vocab), "1:15");

	auto missingValue = HudResolvedValue::missing(HudSourceValueType::Scalar, HudDataClass::Literal);
	HUD_CHECK_EQ_STR(HudFormattingService::format(missingValue, HudFormatKind::Automatic, "", vocab), "");

	auto id = HudResolvedValue::identifier("ready");
	HUD_CHECK_EQ_STR(HudFormattingService::format(id, HudFormatKind::VocabularyValue, "", vocab), "READY");
}

} // namespace

int main() {
	test_missing_vs_zero();
	test_missing_vs_empty_list();
	test_source_lookup();
	test_metric_lookup();
	test_unknown_source();
	test_widget_source_type_compatibility();
	test_unknown_region();
	test_invalid_widget_for_region();
	test_required_source_role_missing();
	test_flexible_region_occupancy_conflict();
	test_invalid_flexible_does_not_remove_universal();
	test_vocabulary_fallback();
	test_scene_override();
	test_missing_data_policy();
	test_history_sample_cadence();
	test_history_reset_on_scene_epoch();
	test_deterministic_fake_output();
	test_no_scene_id_leaks_into_compiled_bindings();
	test_region_bounds_within_canvas_and_no_same_role_overlap();
	test_truncation_never_exceeds_budget();
	test_formatting_spot_checks();

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed.\n";
	if (g_failures > 0) {
		std::cerr << g_failures << " FAILURE(S)\n";
		return 1;
	}
	return 0;
}
